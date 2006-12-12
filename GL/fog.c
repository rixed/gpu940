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
#include <stdbool.h>

/*
 * Data Definitions
 */

enum gli_FogMode gli_fog_mode;
GLfixed gli_fog_density, gli_fog_start_param, gli_fog_end_param;
GLfixed gli_fog_color[4];

/*
 * Private Functions
 */

static bool check_mode(enum gli_FogMode mode)
{
	if (mode != GL_EXP && mode != GL_EXP2 && mode != GL_LINEAR) {
		gli_set_error(GL_INVALID_ENUM);
		return false;
	}
	return true;
}

static bool check_pname(enum gli_FogPname pname)
{
	if (/*pname < GL_FOG_MODE ||*/ pname > GL_FOG_COLOR) {
		gli_set_error(GL_INVALID_ENUM);
		return false;
	}
	return true;
}

static GLfixed i2x(GLint i)
{
	return i<<16;
}

static GLfixed color_i2x(GLint i)
{
	return i>>15;
}

/*
 * Public Functions
 */

int gli_fog_begin(void)
{
	gli_fog_mode = GL_EXP;
	gli_fog_density = 1<<16;
	gli_fog_start_param = 0;
	gli_fog_end_param = 1<<16;
	for (unsigned c=4; c--; ) {
		gli_fog_color[c] = 0;
	}
	return 0;
}

extern inline void gli_fog_end(void);

void glFogx(GLenum pname, GLfixed param)
{
	if (! check_pname(pname)) return;
	switch ((enum gli_FogPname)pname) {
		case GL_FOG_MODE:
			if (! check_mode(param)) {
				return;
			}
			gli_fog_mode = param;
			break;
		case GL_FOG_DENSITY:
			if (param < 0) {
				return gli_set_error(GL_INVALID_VALUE);
			}
			gli_fog_density = param;
			break;
		case GL_FOG_START:
			gli_fog_start_param = param;
			break;
		case GL_FOG_END:
			gli_fog_end_param = param;
			break;
		default:
			gli_set_error(GL_INVALID_OPERATION);
	}
}

void glFogxv(GLenum pname, GLfixed const *params)
{
	if (! check_pname(pname)) return;
	switch ((enum gli_FogPname)pname) {
		case GL_FOG_COLOR:
			for (unsigned c=4; c--; ) {
				gli_fog_color[c] = params[c];
			}
			break;
		default:
			gli_set_error(GL_INVALID_OPERATION);
	}
}

void glFogi(GLenum pname, GLint param)
{
	if (! check_pname(pname)) return;
	switch ((enum gli_FogPname)pname) {
		case GL_FOG_MODE:
			return glFogx(pname, param);
		case GL_FOG_DENSITY:
		case GL_FOG_START:
		case GL_FOG_END:
			return glFogx(pname, i2x(param));
		default:
			gli_set_error(GL_INVALID_OPERATION);
	}
}

void glFogiv(GLenum pname, GLint const *params)
{
	if (! check_pname(pname)) return;
	switch ((enum gli_FogPname)pname) {
		case GL_FOG_COLOR:
			for (unsigned c=4; c--; ) {
				gli_fog_color[c] = color_i2x(params[c]);
			}
			break;
		default:
			gli_set_error(GL_INVALID_OPERATION);
	}
}

