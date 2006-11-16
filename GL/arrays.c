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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * Data Definitions
 */

struct array gli_color_array, gli_normal_array, gli_vertex_array, gli_texcoord_array[GLI_MAX_TEXTURE_UNITS];
static unsigned gli_active_texture;
static void const *gli_indices;
static GLenum gli_indices_type;

/*
 * Private Functions
 */

static size_t sizeof_type(enum gli_Types type)
{
	switch (type) {
		case GL_UNSIGNED_BYTE:
			return sizeof(GLubyte);
		case GL_UNSIGNED_SHORT:
			return sizeof(GLushort);
		case GL_UNSIGNED_INT:
			return sizeof(GLuint);
		case GL_FIXED:
			return sizeof(GLfixed);
		case GL_BYTE:
			return sizeof(GLbyte);
		case GL_SHORT:
			return sizeof(GLshort);
		case GL_FLOAT:
		default:
			assert(0);
			return 0;
	}
}

static void set_array(struct array *arr, GLint size, GLenum type, GLsizei stride, GLvoid const *pointer, GLint min_size, GLint max_size, enum gli_Types const *allowed_types, unsigned nb_allowed_types)
{
	if (size < min_size || size > max_size || stride < 0) {
		gli_set_error(GL_INVALID_VALUE);
		arr->ptr = NULL;
		return;
	}
	unsigned t;
	for (t=0; t < nb_allowed_types; t++) {
		if (type == allowed_types[t]) break;
	}
	if (t == nb_allowed_types) {
		gli_set_error(GL_INVALID_ENUM);
		arr->ptr = NULL;
		return;
	}
	arr->type = type;
	arr->stride = stride;
	arr->ptr = pointer;
	arr->size = size;
	arr->raw_size = size*sizeof_type(type)+stride;
}

static void set_enable(GLenum array, int bit)
{
	switch (array) {
		case GL_COLOR_ARRAY:
			gli_color_array.enabled = bit;
			return;
		case GL_NORMAL_ARRAY:
			gli_normal_array.enabled = bit;
			return;
		case GL_VERTEX_ARRAY:
			gli_vertex_array.enabled = bit;
			return;
		case GL_TEXTURE_COORD_ARRAY:
			gli_texcoord_array[gli_active_texture].enabled = bit;
			return;
	}
	gli_set_error(GL_INVALID_ENUM);
}

static unsigned read_index(GLint idx)
{
	assert(gli_indices);
	switch (gli_indices_type) {
		case GL_UNSIGNED_BYTE:
			{
				GLubyte const *indices = gli_indices;
				return indices[idx];
			}
		case GL_UNSIGNED_SHORT:
			{
				GLushort const *indices = gli_indices;
				return indices[idx];
			}
		case GL_UNSIGNED_INT:
			{
				GLuint const *indices = gli_indices;
				return indices[idx];
			}
		default:
			assert(0);
	}
	return 0;
}

static void array_get(struct array const *arr, GLint idx, int32_t *c)
{
	void const *v_;
	if (gli_indices) {
		idx = read_index(idx);
	}
	v_ = (char *)arr->ptr + idx*arr->raw_size;
	switch (arr->type) {
		case GL_BYTE:
			{
				GLbyte const *v = v_;
				c[0] = v[0]<<16;
				c[1] = v[1]<<16;
				c[2] = arr->size > 2 ? v[2]<<16 : 0;
				c[3] = arr->size > 3 ? v[3]<<16 : 1<<16;
			}
			break;
		case GL_SHORT:
			{
				GLshort const *v = v_;
				c[0] = v[0]<<16;
				c[1] = v[1]<<16;
				c[2] = arr->size > 2 ? v[2]<<16 : 0;
				c[3] = arr->size > 3 ? v[3]<<16 : 1<<16;
			}
			break;
		case GL_FIXED:
			{
				GLfixed const *v = v_;
				c[0] = v[0];
				c[1] = v[1];
				c[2] = arr->size > 2 ? v[2] : 0;
				c[3] = arr->size > 3 ? v[3] : 1<<16;
			}
			break;
		default:
			assert(0);
	}
}

/*
 * Public Functions
 */

int gli_arrays_begin(void)
{
	memset(&gli_color_array, 0, sizeof(gli_color_array));
	memset(&gli_normal_array, 0, sizeof(gli_normal_array));
	memset(&gli_vertex_array, 0, sizeof(gli_vertex_array));
	memset(gli_texcoord_array, 0, sizeof(gli_texcoord_array));
	gli_color_array.size = gli_normal_array.size = gli_vertex_array.size = 4;
	for (unsigned t=0; t<sizeof_array(gli_texcoord_array); t++) {
		gli_texcoord_array[t].size = 4;
	}
	gli_active_texture = GL_TEXTURE0;
	return 0;
}

extern  inline void gli_arrays_end(void);

void gli_vertex_get(GLint idx, int32_t c[4])
{
	return array_get(&gli_vertex_array, idx, c);
}

void gli_normal_get(GLint idx, int32_t c[4])
{
	if (gli_normal_array.enabled) {
		array_get(&gli_normal_array, idx, c);
	} else {
		GLfixed const *n = gli_current_normal();
		for (unsigned i=0; i<3; i++) {
			c[i] = n[i];
		}
		c[3] = 1<<16;
	}
}

void gli_texcoord_get(GLint idx, int32_t uv[2])
{
	array_get(gli_texcoord_array+gli_active_texture, idx, uv);
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_UNSIGNED_BYTE };
	set_array(&gli_color_array, size, type, stride, pointer, 4, 4, allowed_types, sizeof_array(allowed_types));
}

void glNormalPointer(GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_BYTE, GL_SHORT };
	set_array(&gli_normal_array, 3, type, stride, pointer, 3, 3, allowed_types, sizeof_array(allowed_types));
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_BYTE, GL_SHORT };
	set_array(&gli_vertex_array, size, type, stride, pointer, 2, 4, allowed_types, sizeof_array(allowed_types));
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_BYTE, GL_SHORT };
	set_array(gli_texcoord_array+gli_active_texture, size, type, stride, pointer, 2, 4, allowed_types, sizeof_array(allowed_types));
}

void glClientActiveTexture(GLenum texture)
{
	if (texture >= GLI_MAX_TEXTURE_UNITS) {
		gli_set_error(GL_INVALID_ENUM);
		return;
	}
	gli_active_texture = texture;
}

void glEnableClientState(GLenum array)
{
	set_enable(array, 1);
}

void glDisableClientState(GLenum array)
{
	set_enable(array, 0);
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	if (/* mode < GL_POINTS ||*/ mode > GL_QUADS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (count < 0) {
		return gli_set_error(GL_INVALID_VALUE);
	} else if (count == 0) {
		return;
	}
	if (mode >= GL_TRIANGLE_STRIP) {
		gli_facet_array(mode, first, count);
	} else if (mode == GL_POINTS) {
		gli_points_array(first, count);
	} else {	// lines
		// TODO
	}
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, GLvoid const *indices)
{
	if (/* mode < GL_POINTS ||*/ mode > GL_QUADS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (count < 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	if (type != GL_UNSIGNED_INT && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_BYTE) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	gli_indices = indices;
	gli_indices_type = type;
	if (mode >= GL_TRIANGLE_STRIP) {
		gli_facet_array(mode, 0, count);
	} else {	// lines and points
		// TODO
	}
	gli_indices = NULL;
}
