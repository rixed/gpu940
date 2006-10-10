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
#include "gli.h"
#include "fixmath.h"

/*
 * Data Definitions
 */

static enum gli_TexFilter min_filter, max_filter;
static enum gli_TexWrap wrap_s, wrap_t;
static struct gli_texture_unit texture_units[GLI_MAX_TEXTURE_UNITS];
static GLuint active_texture_unit;	// idx in textures
#define DEFAULT_BINDING 0	// FIXME : l'addresse d'une texture par defaut

/*
 * Private Functions
 */

static void texture_ctor(struct gli_texture *tex)
{
	tex->s = tex->t = tex->r = 0;
	tex->q = 0x10000;
	tex->bound = DEFAULT_BINDING;
}

/*
 * Public Functions
 */

int gli_texture_begin(void)
{
	min_filter = GL_NEAREST_MIPMAP_LINEAR;
	max_filter = GL_LINEAR;
	wrap_s = wrap_t = GL_REPEAT;
	env_mode = GL_MODULATE;
	env_color[0] = env_color[1] = env_color[2] = env_color[3] = 0;
	for (unsigned i=0; i<sizeof_array(textures); i++) {
		texture_ctor(textures+i);
	}
	bound_texture = 0;
	active_texture_unit = 0;
}

static inline void gli_texture_end(void) {}

void glTexParameterx(GLenum target, GLenum pname, GLfixed param)
{
	if (target != GL_TEXTURE_2D || pname < GL_TEXTURE_MIN_FILTER || pname > GL_TEXTURE_WRAP_T) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	switch ((enum gli_TexParam)pname) {
		case GL_TEXTURE_MIN_FILTER:
			if (param < GL_NEAREST || param > GL_LINEAR_MIPMAP_LINEAR) {
				return gli_set_error(GL_INVALID_ENUM);
			}
			min_filter = param;
			return;
		case GL_TEXTURE_MAG_FILTER:
			if (param != GL_NEAREST && param != GL_LINEAR) {
				return gli_set_error(GL_INVALID_ENUM);
			}
			max_filter = param;
			return;
		case GL_TEXTURE_WRAP_S:
			if (param < GL_CLAMP || param > GL_REPEAT) {
				return gli_set_error(GL_INVALID_ENUM);
			}
			wrap_s = param;
			return;
		case GL_TEXTURE_WRAP_T:
			if (param < GL_CLAMP || param > GL_REPEAT) {
				return gli_set_error(GL_INVALID_ENUM);
			}
			wrap_t = param;
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
	if (param < GL_MODULATE || param > GL_REPLACE) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	texture_unit[active_texture_unit].env_mode = param;
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
	for (unsigned i=0; i<sizeof_array(env_color); i++) {
		texture_unit[active_texture_unit].env_color[i] = CLAMP(params[i], 0, 0x10000);
	}
}

void glMultiTexCoord4x(GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
	if (target < GL_TEXTURE0 || target > GL_TEXTURE0+GLI_MAX_TEXTURE_UNITS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	struct gli_texture_unit const *const unit = &texture_units[target-GL_TEXTURE0];
	unit->s = s;
	unit->t = t;
	unit->r = r;
	unit->q = q;
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLvoid const *pixels)
{
	if (
		target != GL_TEXTURE_2D ||
		format < GL_ALPHA || format > GL_LUMINANCE_ALPHA ||
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
		format != internalformat ||
		(type == GL_UNSIGNED_SHORT_5_6_5 && format != GL_RGB) ||
		((type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1) && format != GL_RGBA)
	) {
		return gli_set_error(GL_INVALID_OPERATION);
	}
	// TODO
	// First allocate memory, then, if pixels != NULL, fill it (using glTexSubImage2D)
}

void glBindTexture(GLenum target, GLuint texture)
{
	if (target != GL_TEXTURE_2D) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (texture == 0) {
		texture_unit[active_texture_unit].bound = DEFAULT_BINDING;
	} else {
		texture_unit[active_texture_unit].bound = texture;
	}
}

void glDeleteTextures(GLsizei n, GLuint const *textures)
{
	if (n < 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	for ( ; n--; ) {
		if (0 == textures[n]) continue;
		for (unsigned i=0; i<sizeof_array(texture_units); i++) {
			if (texture_unit[i].bound == textures[n]) {
				texture_unit[i].bound = DEFAULT_BINDING;
			}
		}
		// TODO
		// Free memory
	}
}

void glActiveTexture(GLenum texture)
{
	if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+GLI_MAX_TEXTURE_UNITS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	active_texture_unit = texture-GL_TEXTURE0;
}

