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

static struct array colors, normals, vertexes, texcoords[GLI_MAX_TEXTURE_UNITS];
static unsigned gli_active_texture;

/*
 * Private Functions
 */

static size_t sizeof_type(enum gli_Types type)
{
	switch (type) {
		case GL_UNSIGNED_BYTE:
			return sizeof(GLubyte);
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
			colors.enabled = bit;
			return;
		case GL_NORMAL_ARRAY:
			normals.enabled = bit;
			return;
		case GL_VERTEX_ARRAY:
			vertexes.enabled = bit;
			return;
		case GL_TEXTURE_COORD_ARRAY:
			texcoords[gli_active_texture].enabled = bit;
			return;
	}
	gli_set_error(GL_INVALID_ENUM);
}

static void array_get(struct array const *arr, GLint idx, int32_t c[4])
{
	void const *v_ = (char *)arr->ptr + idx*arr->raw_size;
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
	memset(&colors, 0, sizeof(colors));
	memset(&normals, 0, sizeof(normals));
	memset(&vertexes, 0, sizeof(vertexes));
	memset(texcoords, 0, sizeof(texcoords));
	colors.size = normals.size = vertexes.size = 4;
	for (unsigned t=0; t<sizeof_array(texcoords); t++) {
		texcoords[t].size = 4;
	}
	gli_active_texture = GL_TEXTURE0;
	return 0;
}

extern  inline void gli_arrays_end(void);

void gli_vertex_get(GLint idx, int32_t c[4])
{
	return array_get(&vertexes, idx, c);
}

void gli_normal_get(GLint idx, int32_t c[4])
{
	if (normals.enabled) {
		array_get(&normals, idx, c);
	} else {
		GLfixed const *n = gli_current_normal();
		for (unsigned i=0; i<3; i++) {
			c[i] = n[i];
		}
		c[3] = 1<<16;
	}
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_UNSIGNED_BYTE };
	set_array(&colors, size, type, stride, pointer, 4, 4, allowed_types, sizeof_array(allowed_types));
}

void glNormalPointer(GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_BYTE, GL_SHORT };
	set_array(&normals, 3, type, stride, pointer, 3, 3, allowed_types, sizeof_array(allowed_types));
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_BYTE, GL_SHORT };
	set_array(&vertexes, size, type, stride, pointer, 2, 4, allowed_types, sizeof_array(allowed_types));
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_BYTE, GL_SHORT };
	set_array(texcoords+gli_active_texture, size, type, stride, pointer, 2, 4, allowed_types, sizeof_array(allowed_types));
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
	}
	if (mode >= GL_TRIANGLE_STRIP) {
		gli_facet_array(mode, first, count);
	} else {	// lines and points
		// TODO
	}
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, GLvoid const *indices)
{
	// TODO
	(void)mode;
	(void)count;
	(void)type;
	(void)indices;
}
