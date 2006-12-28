/* This file is part of gpu940.
 *
 * Copyright (C) 2006 Cedric Cellier.
 *
 * Gpu940 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Gpu940 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gpu940; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <stdlib.h>
#include <assert.h>
#include "gli.h"
#include "fixmath.h"

/*
 * Data Definitions
 */

static struct gli_texture_unit texture_units[GLI_MAX_TEXTURE_UNITS];
static struct gli_texture_object default_texture;
static GLuint active_texture_unit;	// idx in textures
#define NB_MAX_TEXOBJ 128
static struct gli_texture_object *binds[NB_MAX_TEXOBJ];
struct pixel_reader {
	void (*read_func)(struct pixel_reader *);
	union {
		struct {
			uint8_t b, g, r, a;	// little endian only
		} c;
		uint32_t u32;
#		define READER_KEY_COLOR ((255U<<24)|(KEY_RED<<16)|(KEY_GREEN<<8)|(KEY_BLUE))	/* FIXME: little endian only */
#		define ALMOST_READER_KEY_COLOR ((255U<<24)|(KEY_RED<<16)|(KEY_GREEN<<8)|(KEY_BLUE-1))
	} color;
	uint8_t const *pixels;
};

/*
 * Private Functions
 */

static int texture_unit_ctor(struct gli_texture_unit *tu)
{
	tu->s = tu->t = tu->r = 0;
	tu->q = 0x10000;
	tu->bound = 0;
	tu->env_mode = GL_MODULATE;
	tu->env_color[0] = tu->env_color[1] = tu->env_color[2] = tu->env_color[3] = 0;
//	tu->enabled = GL_FALSE;
	return gli_matrix_stack_ctor(&tu->ms, GLI_MAX_TEXTURE_STACK_DEPTH);
}

static void texture_unit_dtor(struct gli_texture_unit *tu)
{
	return gli_matrix_stack_dtor(&tu->ms);
}

static int texture_object_ctor(struct gli_texture_object *to)
{
	to->min_filter = GL_NEAREST_MIPMAP_LINEAR;
	to->max_filter = GL_LINEAR;
	to->wrap_s = to->wrap_t = GL_REPEAT;
	to->has_data = false;
	to->was_bound = false;
	to->need_key = false;
	to->have_mean_alpha = false;
	return 0;
}

static void free_to_datas(struct gli_texture_object *to)
{
	if (! to->has_data) return;
	if (to->is_resident) {
		gpuFree(to->img_res);	// no need to keep it any longer
	} else {
		free(to->img_nores);
	}
	to->has_data = false;
	to->need_key = false;
	to->have_mean_alpha = false;
}

static void texture_object_dtor(struct gli_texture_object *to)
{
	if (! to->has_data) return;
	free_to_datas(to);
}

// we keep it in binds[] if it's there
static void texture_object_del(struct gli_texture_object *to)
{
	texture_object_dtor(to);
	free(to);
}

static struct gli_texture_object *texture_object_new(void)
{
	struct gli_texture_object *to = malloc(sizeof(*to));
	if (! to) {
		gli_set_error(GL_OUT_OF_MEMORY);
		return NULL;
	}
	if (0 != texture_object_ctor(to)) {
		free(to);
		return NULL;
	}
	return to;
}

static void read_alpha(struct pixel_reader *reader)
{
	reader->color.c.a = *reader->pixels++;
	reader->color.c.r = reader->color.c.g = reader->color.c.b = 0;
}

static void read_rgb(struct pixel_reader *reader)
{
	reader->color.c.r = *reader->pixels++;
	reader->color.c.g = *reader->pixels++;
	reader->color.c.b = *reader->pixels++;
	reader->color.c.a = 255;
}

static void read_rgba(struct pixel_reader *reader)
{
	reader->color.c.r = *reader->pixels++;
	reader->color.c.g = *reader->pixels++;
	reader->color.c.b = *reader->pixels++;
	reader->color.c.a = *reader->pixels++;
}

static void read_lum(struct pixel_reader *reader)
{
	reader->color.c.r = reader->color.c.g = reader->color.c.b = *reader->pixels++;
	reader->color.c.a = 255;
}

static void read_luma(struct pixel_reader *reader)
{
	reader->color.c.r = reader->color.c.g = reader->color.c.b = *reader->pixels++;
	reader->color.c.a = *reader->pixels++;
}

static void read_565(struct pixel_reader *reader)
{
	uint16_t v = *reader->pixels++;
	reader->color.c.r = (v>>11);
	reader->color.c.g = (v>>5) & 0x3f;
	reader->color.c.b = v & 0x1f;
	reader->color.c.a = 255;
}

static void read_4444(struct pixel_reader *reader)
{
	uint16_t v = *reader->pixels++;
	reader->color.c.r = (v>>12);
	reader->color.c.g = (v>>8) & 0xf;
	reader->color.c.b = (v>>4) & 0xf;
	reader->color.c.a = v & 0xf;
}

static void read_5551(struct pixel_reader *reader)
{
	uint16_t v = *reader->pixels++;
	reader->color.c.r = (v>>11);
	reader->color.c.g = (v>>6) & 0x1f;
	reader->color.c.b = (v>>1) & 0x1f;
	reader->color.c.a = v & 0x1;
}

static void pixel_reader_ctor(struct pixel_reader *reader, GLenum format, GLenum type, void const *pixels)
{
	reader->pixels = pixels;
	switch (type) {
		case GL_UNSIGNED_BYTE:
			switch (format) {
				case GL_ALPHA:
					reader->read_func = read_alpha;
					break;
				case GL_RGB:
					reader->read_func = read_rgb;
					break;
				case GL_RGBA:
					reader->read_func = read_rgba;
					break;
				case GL_LUMINANCE:
					reader->read_func = read_lum;
					break;
				case GL_LUMINANCE_ALPHA:
					reader->read_func = read_luma;
					break;
				default:
					assert(0);
			}
			break;
		case GL_UNSIGNED_SHORT_5_6_5:
			reader->read_func = read_565;
			break;
		case GL_UNSIGNED_SHORT_4_4_4_4:
			reader->read_func = read_4444;
			break;
		case GL_UNSIGNED_SHORT_5_5_5_1:
			reader->read_func = read_5551;
			break;
		default:
			assert(0);
	}
}

static void pixel_reader_dtor(struct pixel_reader *reader)
{
	(void)reader;
}

static void pixel_read_next(struct pixel_reader *reader)
{
	reader->read_func(reader);
}

void glTexSubImage2D_nocheck(struct gli_texture_object *to, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid const *pixels)
{
	assert(to->has_data);
	uint32_t *dest;
	if (to->is_resident) {	// destination is GPU memory, so we must write gpuColors
		dest = gli_get_texture_address(to->img_res);
	} else {	// destination is malloced rgb array
		dest = to->img_nores;
	}
	struct pixel_reader reader;
	pixel_reader_ctor(&reader, format, type, pixels);
	uint32_t sum_alpha = 0;	// 32 bits are enought for a 1024x1024 fully opaque texture.
	unsigned nb_alpha = 0;
	for (
		unsigned y = yoffset << to->mipmaps[level].width_log;
		height > 0;
		height--, y += 1 << to->mipmaps[level].width_log
	) {
		for (int x=xoffset; x < xoffset+width; x++) {
			pixel_read_next(&reader);
			if (reader.color.c.a < 5) {	// bellow 5, we consider this is not blending but keying
				to->need_key = true;
				dest[y+x] = to->is_resident ? gpuColor(KEY_RED, KEY_GREEN, KEY_BLUE) : READER_KEY_COLOR;
			} else {
				sum_alpha += reader.color.c.a;
				nb_alpha ++;
				if (reader.color.u32 == READER_KEY_COLOR) {	// don't allow the use of our key color
					reader.color.u32 = ALMOST_READER_KEY_COLOR;
				}
				dest[y+x] = to->is_resident ? gpuColor(reader.color.c.r, reader.color.c.g, reader.color.c.b) : reader.color.u32;
			}
		}
	}
	to->mean_alpha = sum_alpha / nb_alpha;
	if (to->mean_alpha < 250) to->have_mean_alpha = true;
	pixel_reader_dtor(&reader);
}

/*
 * Public Functions
 */

int gli_texture_begin(void)
{
	texture_object_ctor(&default_texture);
	for (unsigned i=0; i<sizeof_array(texture_units); i++) {
		if (0 != texture_unit_ctor(texture_units+i)) return -1;
	}
	active_texture_unit = 0;
	binds[0] = &default_texture;
	for (unsigned b=1; b<sizeof_array(binds); b++) binds[b] = NULL;
	return 0;
}

void gli_texture_end(void)
{
	for (unsigned i=0; i<sizeof_array(texture_units); i++) {
		texture_unit_dtor(texture_units+i);
	}
	texture_object_dtor(&default_texture);
	for (unsigned b=1; b<sizeof_array(binds); b++) {
		if (binds[b]) {
			texture_object_del(binds[b]);
			binds[b] = NULL;
		}
	}
}

struct gli_texture_unit *gli_get_texture_unit(void)
{
	return texture_units + active_texture_unit;
}

struct gli_texture_object *gli_get_texture_object(void)
{
	return binds[gli_get_texture_unit()->bound];
}

void glTexParameterx(GLenum target, GLenum pname, GLfixed param)
{
	if (target != GL_TEXTURE_2D || /*pname < GL_TEXTURE_MIN_FILTER ||*/ pname > GL_TEXTURE_WRAP_T) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	switch ((enum gli_TexParam)pname) {
		case GL_TEXTURE_MIN_FILTER:
			if (param < GL_NEAREST || param > GL_LINEAR_MIPMAP_LINEAR) {
				return gli_set_error(GL_INVALID_ENUM);
			}
			gli_get_texture_object()->min_filter = param;
			return;
		case GL_TEXTURE_MAG_FILTER:
			if (param != GL_NEAREST && param != GL_LINEAR) {
				return gli_set_error(GL_INVALID_ENUM);
			}
			gli_get_texture_object()->max_filter = param;
			return;
		case GL_TEXTURE_WRAP_S:
			if (param < GL_CLAMP || param > GL_REPEAT) {
				return gli_set_error(GL_INVALID_ENUM);
			}
			gli_get_texture_object()->wrap_s = param;
			return;
		case GL_TEXTURE_WRAP_T:
			if (param < GL_CLAMP || param > GL_REPEAT) {
				return gli_set_error(GL_INVALID_ENUM);
			}
			gli_get_texture_object()->wrap_t = param;
			return;
		default:
			return gli_set_error(GL_INVALID_ENUM);
	}
}

void glTexEnvx(GLenum target, GLenum pname, GLfixed param)
{
	if (target != GL_TEXTURE_ENV || pname != GL_TEXTURE_ENV_MODE) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if ((param < GL_MODULATE || param > GL_REPLACE) && param != GL_BLEND) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	gli_get_texture_unit()->env_mode = param;
}

void glTexEnvxv(GLenum target, GLenum pname, GLfixed const *params)
{
	if (target != GL_TEXTURE_ENV) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (pname == GL_TEXTURE_ENV_MODE) {
		return glTexEnvx(target, pname, *params);
	}
	if (pname != GL_TEXTURE_ENV_COLOR) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	struct gli_texture_unit *const tu = gli_get_texture_unit();
	for (unsigned i=0; i<sizeof_array(tu->env_color); i++) {
		tu->env_color[i] = params[i];
		CLAMP(tu->env_color[i], 0, 0x10000);
	}
}

void glMultiTexCoord4x(GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
	if (/*target < GL_TEXTURE0 ||*/ target > GL_TEXTURE0+GLI_MAX_TEXTURE_UNITS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	struct gli_texture_unit *const tu = &texture_units[target-GL_TEXTURE0];
	tu->s = s;
	tu->t = t;
	tu->r = r;
	tu->q = q;
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLvoid const *pixels)
{
	(void)internalformat;	// We ignore internalformat alltogether
	if (
		target != GL_TEXTURE_2D ||
		/*format < GL_ALPHA ||*/ format > GL_LUMINANCE_ALPHA ||
		/*internalformat < GL_ALPHA || internalformat > GL_LUMINANCE_ALPHA ||*/
		(type != GL_UNSIGNED_BYTE && (type < GL_UNSIGNED_SHORT_5_6_5 || type > GL_UNSIGNED_SHORT_5_5_5_1))
	) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (
		border != 0 ||
		level < 0 || level > 0 /*TODO: mipmapping*/ ||
		width < 0 || height < 0 || width > GLI_MAX_TEXTURE_SIZE || height > GLI_MAX_TEXTURE_SIZE ||
		!is_power_of_2(width) || !is_power_of_2(height)
	) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	if (
		/*format != (unsigned)internalformat ||*/
		(type == GL_UNSIGNED_SHORT_5_6_5 && format != GL_RGB) ||
		((type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1) && format != GL_RGBA)
	) {
		return gli_set_error(GL_INVALID_OPERATION);
	}
	// if we already had data for this texture, free it
	struct gli_texture_object *to = gli_get_texture_object();
	free_to_datas(to);	// will free non resident as well as resident datas, and set has_data to false
	// allocate memory for non-resident (malloc)
	to->img_nores = malloc(width*height*sizeof(*to->img_nores));
	if (! to->img_nores) return gli_set_error(GL_OUT_OF_MEMORY);
	to->has_data = true;
	to->is_resident = false;
	to->mipmaps[level].height_log = Fix_log2(height);
	to->mipmaps[level].width_log = Fix_log2(width);
	if (pixels) {
		glTexSubImage2D_nocheck(to, level, 0, 0, width, height, format, type, pixels);
	}
	// Notice that the texture will no be made resident untill we actually use it for drawing.
	// It will then remain resident until deleted or no more GPU memory.
	// Unloading unused textures would be a great feature to add later.
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
	if (
		target != GL_TEXTURE_2D ||
		/*format < GL_ALPHA ||*/ format > GL_LUMINANCE_ALPHA ||
		(type != GL_UNSIGNED_BYTE && (type < GL_UNSIGNED_SHORT_5_6_5 || type > GL_UNSIGNED_SHORT_5_5_5_1))
	) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	struct gli_texture_object *to = gli_get_texture_object();
	if (
		level < 0 || level > 0 /*TODO: mipmapping*/ ||
		xoffset < 0 || xoffset+width > (1<<to->mipmaps[level].width_log) ||
		yoffset < 0 || yoffset+height > (1<<to->mipmaps[level].height_log) ||
		width < 0 || height < 0
	) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	if (
		! to->has_data ||
		(type == GL_UNSIGNED_SHORT_5_6_5 && format != GL_RGB) ||
		((type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1) && format != GL_RGBA)
	) {
		return gli_set_error(GL_INVALID_OPERATION);
	}
	glTexSubImage2D_nocheck(to, level, xoffset, yoffset, width, height, format, type, pixels);
}

void glBindTexture(GLenum target, GLuint texture)
{
	if (target != GL_TEXTURE_2D) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (texture >= sizeof_array(binds)) {	// FIXME : there should be a mapping between any uint>0 and a texture object
		return gli_set_error(GL_OUT_OF_MEMORY);
	}
	// when texture = 0, we just bind to it.
	if (!binds[texture]) {
		binds[texture] = texture_object_new();
	}
	gli_get_texture_unit()->bound = texture;
	binds[texture]->was_bound = true;
}

void glDeleteTextures(GLsizei n, GLuint const *textures)
{
	if (n < 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	for ( ; n--; ) {
		if (0 == textures[n]) continue;	// silently ignore request to delete default texture
		if (textures[n] >= sizeof_array(binds) || !binds[textures[n]]) continue;
		texture_object_del(binds[textures[n]]);
		binds[textures[n]] = NULL;
		for (unsigned i=0; i<sizeof_array(texture_units); i++) {
			if (texture_units[i].bound == textures[n]) {
				texture_units[i].bound = 0;
			}
		}
	}
}

void glActiveTexture(GLenum texture)
{
	if (/*texture < GL_TEXTURE0 ||*/ texture > GL_TEXTURE0+GLI_MAX_TEXTURE_UNITS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	active_texture_unit = texture-GL_TEXTURE0;
}

void glGenTextures(GLsizei n, GLuint *textures)
{
	if (n<0) return gli_set_error(GL_INVALID_VALUE);
	for (unsigned b=1; b<sizeof_array(binds) && n > 0; b++) {
		if (binds[b]) continue;
		binds[b] = texture_object_new();	// so that we won't allocate this binds[b] another time
		textures[--n] = b;
	}
	if (n>0) return gli_set_error(GL_OUT_OF_MEMORY);
}

GLboolean glIsTexture(GLuint texture)
{
	if (texture < sizeof_array(binds) && binds[texture] && binds[texture]->was_bound) {
		return GL_TRUE;
	}
	return GL_FALSE;
}

// Tells is texturing is active for this array
bool gli_texturing(void)
{
	return gli_enabled(GL_TEXTURE_2D);// && gli_texcoord_array[active_texture_unit].enabled; if array is unset, use default current texcoords
}
