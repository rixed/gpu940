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

/*
 * Data Definitions
 */

static GLenum error;

/*
 * Private Functions
 */

/*
 * Public Functions
 */

int gli_state_begin(void)
{
	error = GL_NO_ERROR;
	return 0;
}

extern inline void gli_state_end(void);

void gli_set_error(GLenum err)
{
	if (error == GL_NO_ERROR)
		error = err;
}

GLenum glGetError(void)
{
	GLenum ret = error;
	error = GL_NO_ERROR;
	return ret;
}

GLubyte const *glGetString(GLenum name)
{
	switch (name) {
		case GL_VENDOR:
			return "Free Software Community (see http://gna.org/projects/gpu940/)";
		case GL_RENDERER:
			return "GPU940 for "
#			ifdef GP2X
				"GP2X"
#			else
				"x86"
#			endif
				;
		case GL_VERSION:
			return TOSTR(VERSION);
		case GL_EXTENSIONS:
			return "";
	}
	gli_set_error(GL_INVALID_ENUM);
	return NULL;
}

void glGetIntegerv(GLenum pname, GLint *params)
{
	switch (pname) {
		case GL_ALIASED_POINT_SIZE_RANGE:
		case GL_ALIASED_LINE_WIDTH_RANGE:
		case GL_SMOOTH_LINE_WIDTH_RANGE:
		case GL_SMOOTH_POINT_SIZE_RANGE:
			params[0] = params[1] = 0x10000;
			return;
		case GL_ALPHA_BITS:
		case GL_DEPTH_BITS:
		case GL_STENCIL_BITS:
			params[0] = 0;
			return;
		case GL_BLUE_BITS:
		case GL_GREEN_BITS:
		case GL_RED_BITS:
			params[0] = 8;
			return;
		case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
			params[0] = 0;	// TODO: must be at least 10
			return;
		case GL_COMPRESSED_TEXTURE_FORMATS:
			return;	// See above
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES:
			params[0] = GL_RGB;
			return;
		case GL_IMPLEMENTATION_COLOR_READ_TYPE_OES:
			params[0] = GL_UNSIGNED_BYTE;
			return;
		case GL_MAX_ELEMENTS_INDICES:
		case GL_MAX_ELEMENTS_VERTICES:
			params[0] = 2*(sizeof(shared->cmds)/sizeof(gpuCmdVector))/3;
			return;
		case GL_MAX_LIGHTS:
			params[0] = GLI_MAX_LIGHTS;	// at least 8
			return;
		case GL_MAX_MODELVIEW_STACK_DEPTH:
			params[0] = GLI_MAX_MODELVIEW_STACK_DEPTH;	// at least 16
			return;
		case GL_MAX_PROJECTION_STACK_DEPTH:
			params[0] = GLI_MAX_PROJECTION_STACK_DEPTH; // at least 2
			return;
		case GL_MAX_TEXTURE_STACK_DEPTH:
			params[0] = GLI_MAX_TEXTURE_STACK_DEPTH;	// at least 2
			return;
		case GL_MAX_TEXTURE_SIZE:
			params[0] = 1<<10;
			return;
		case GL_MAX_TEXTURE_UNITS:
			params[0] = GLI_MAX_TEXTURE_UNITS;	// We don't really care
			return;
		case GL_MAX_VIEWPORT_DIMS:
			params[0] = SCREEN_WIDTH;
			params[1] = SCREEN_HEIGHT;
			return;
		case GL_SUBPIXEL_BITS:
			params[0] = 4;	// at least 4, not really significant nor usefull
			return;
	}
	gli_set_error(GL_INVALID_ENUM);
}
