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
	tu->enabled = GL_FALSE;
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
	return 0;
}

static void texture_object_dtor(struct gli_texture_object *to)
{
	if (!to->has_data) return;
	// TODO
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
	if (
		target != GL_TEXTURE_2D ||
		/*format < GL_ALPHA ||*/ format > GL_LUMINANCE_ALPHA ||
		internalformat < GL_ALPHA || internalformat > GL_LUMINANCE_ALPHA ||
		(type != GL_UNSIGNED_BYTE && (type < GL_UNSIGNED_SHORT_5_6_5 || type > GL_UNSIGNED_SHORT_5_5_5_1))
	) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (
		border != 0 ||
		level < 0 ||
		width < 0 || height < 0 || width > GLI_MAX_TEXTURE_SIZE || height > GLI_MAX_TEXTURE_SIZE ||
		!is_power_of_2(width) || !is_power_of_2(height)
	) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	if (
		format != (unsigned)internalformat ||
		(type == GL_UNSIGNED_SHORT_5_6_5 && format != GL_RGB) ||
		((type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1) && format != GL_RGBA)
	) {
		return gli_set_error(GL_INVALID_OPERATION);
	}
	// TODO
	(void)pixels;
	// First allocate memory, then, if pixels != NULL, fill it (using glTexSubImage2D)
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
		if (!binds[textures[n]]) continue;
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
