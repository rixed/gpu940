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
static GLuint next_togen;	// used by glGenTextures

/*
 * Private Functions
 */

static int texture_unit_ctor(struct gli_texture_unit *unit)
{
	unit->s = unit->t = unit->r = 0;
	unit->q = 0x10000;
	unit->bound = 0;
	unit->env_mode = GL_MODULATE;
	unit->env_color[0] = unit->env_color[1] = unit->env_color[2] = unit->env_color[3] = 0;
	unit->enabled = GL_FALSE;
	return gli_matrix_stack_ctor(&unit->ms, GLI_MAX_TEXTURE_STACK_DEPTH);
}

static void texture_unit_dtor(struct gli_texture_unit *unit)
{
	return gli_matrix_stack_dtor(&unit->ms);
}

/*
 * Public Functions
 */

int gli_texture_begin(void)
{
	min_filter = GL_NEAREST_MIPMAP_LINEAR;
	max_filter = GL_LINEAR;
	wrap_s = wrap_t = GL_REPEAT;
	for (unsigned i=0; i<sizeof_array(texture_units); i++) {
		if (0 != texture_unit_ctor(texture_units+i)) return -1;
	}
	active_texture_unit = 0;
	next_togen = 0;
	return 0;
}

void gli_texture_end(void)
{
	for (unsigned i=0; i<sizeof_array(texture_units); i++) {
		texture_unit_dtor(texture_units+i);
	}
}

struct gli_texture_unit *gli_get_texture_unit(void)
{
	return texture_units+active_texture_unit;
}

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
	struct gli_texture_unit *const unit = gli_get_texture_unit();
	for (unsigned i=0; i<sizeof_array(unit->env_color); i++) {
		unit->env_color[i] = params[i];
		CLAMP(unit->env_color[i], 0, 0x10000);
	}
}

void glMultiTexCoord4x(GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
	if (target < GL_TEXTURE0 || target > GL_TEXTURE0+GLI_MAX_TEXTURE_UNITS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	struct gli_texture_unit *const unit = &texture_units[target-GL_TEXTURE0];
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
	if (texture == 0) {
		gli_get_texture_unit()->bound = 0;
	} else {
		gli_get_texture_unit()->bound = texture;
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
			if (texture_units[i].bound == textures[n]) {
				texture_units[i].bound = 0;
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

void glGenTextures(GLsizei n, GLuint *textures)
{
	if (n<0) return gli_set_error(GL_INVALID_VALUE);
	for (int i=0; i<n; i++) {
		textures[i] = next_togen++;
	}
}
