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
#include <string.h>

/*
 * Data Definitions
 */

static struct array colors, normals, vertexes, texcoords[GLI_MAX_TEXTURE_UNITS];
static unsigned gli_active_texture;

/*
 * Private Functions
 */

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
	if (arr->enabled) {
		arr->type = type;
		arr->stride = stride;
		arr->ptr = pointer;
		arr->size = size;
	}
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

void glColorPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_UNSIGNED_BYTE };
	set_array(&colors, size, type, stride, pointer, 4, 4, allowed_types, sizeof_array(allowed_types));
}

void glNormalPointer(GLenum type, GLsizei stride, GLvoid const *pointer)
{
	static enum gli_Types const allowed_types[] = { GL_FIXED, GL_BYTE, GL_SHORT };
	set_array(&normals, 4, type, stride, pointer, 4, 4, allowed_types, sizeof_array(allowed_types));
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
	// TODO
	// for triangles, call gli_triangle with 3 vertexes, normals, colors and texcoords
	// (NULL if not set), of GLfixed type and size=4.
	// The same for lines (2 vertexes etc) and points (1 vertex etc).
	// gli_triangle/point/line performs rotation, culling, fog and lightning.
	(void)mode;
	(void)first;
	(void)count;
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, GLvoid const *indices)
{
	// TODO
	(void)mode;
	(void)count;
	(void)type;
	(void)indices;
}
