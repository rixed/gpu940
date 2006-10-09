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
#ifndef GL_H_061009
#define GL_H_061009

#include <inttypes.h>

// GL types
typedef enum { GL_FALSE, GL_TRUE } glBool;
typedef unsigned GLenum;
typedef uint8_t GLubyte;
typedef int GLint;

// Replaces GLX, EGL, ...
glBool glOpen(void);
void glClose(void);

// Errors
enum glErrors { GL_NO_ERROR, GL_INVALID_ENUM, GL_INVALID_VALUE, GL_INVALID_OPERATION, GL_STACK_OVERFLOW, GL_STACK_UNDERFLOW, GL_OUT_OF_MEMORY };

// State Queries
enum gli_StringName { GL_VENDOR, GL_RENDERER, GL_VERSION, GL_EXTENSIONS };
GLubyte const *glGetString(GLenum name);
GLenum glGetError(void);
enum gli_ParamName {
	GL_ALIASED_POINT_SIZE_RANGE, GL_ALIASED_LINE_WIDTH_RANGE, GL_ALPHA_BITS, GL_BLUE_BITS, GL_COMPRESSED_TEXTURE_FORMATS,
	GL_DEPTH_BITS, GL_GREEN_BITS, GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES, GL_IMPLEMENTATION_COLOR_READ_TYPE_OES,
	GL_MAX_ELEMENTS_INDICES, GL_MAX_ELEMENTS_VERTICES, GL_MAX_LIGHTS, GL_MAX_MODELVIEW_STACK_DEPTH, GL_MAX_PROJECTION_STACK_DEPTH,
	GL_MAX_TEXTURE_SIZE, GL_MAX_TEXTURE_STACK_DEPTH, GL_MAX_TEXTURE_UNITS, GL_MAX_VIEWPORT_DIMS, GL_NUM_COMPRESSED_TEXTURE_FORMATS,
	GL_RED_BITS, GL_SMOOTH_LINE_WIDTH_RANGE, GL_SMOOTH_POINT_SIZE_RANGE, GL_STENCIL_BITS, GL_SUBPIXEL_BITS,
};
void glGetIntegerv(GLenum pname, GLint *params);

#endif
