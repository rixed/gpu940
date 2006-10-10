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

static GLfixed color[4];
static GLfixed normal[3];
static struct gli_light lights[GLI_MAX_LIGHTS];
static struct gli_material material;
static GLfixed ambient[4];
static enum gli_ShadeModel shade_model;
static enum gli_FrontFace front_face;

/*
 * Private Functions
 */

static void light_ctor(struct gli_light *light)
{
	memset(light, 0, sizeof(*light));
	light->spot_cutoff = 180<<16;
	light->constant_attenuation = 1<<16;
	light->ambient[3] = 1<<16;
	light->position[2] = 1<<16;
	light->spot_direction[2] = -1<<16;
	light->enabled = GL_FALSE;
}

/*
 * Public Functions
 */

int gli_color_begin(void)
{
	color[0] = color[1] = color[2] = color[3] = 0x10000;
	normal[0] = normal[1] = 0;
	normal[2] = 0x10000;
	for (unsigned i=0; i<sizeof_array(lights); i++) {
		light_ctor(lights+i);
	}
	lights[0].diffuse[0] = lights[0].diffuse[1] = lights[0].diffuse[2] = lights[0].diffuse[3] = 1<<16;
	lights[0].specular[0] = lights[0].specular[1] = lights[0].specular[2] = lights[0].specular[3] = 1<<16;
	memset(&material, 0, sizeof(material));
	material.ambient[0] = material.ambient[1] = material.ambient[2] = 13107;
	material.ambient[3] = 1<<16;
	material.diffuse[0] = material.diffuse[1] = material.diffuse[2] = 52429;
	material.diffuse[3] = 1<<16;
	material.specular[3] = 1<<16;
	material.emission[3] = 1<<16;
	ambient[0] = ambient[1] = ambient[2] = 13107;
	ambient[3] = 1<<16;
	shade_model = GL_SMOOTH;
	front_face = GL_CCW;
	return 0;
}

extern inline void gli_color_end(void);

void gli_light_enable(GLenum light)
{
	lights[light-GL_LIGHT0].enabled = GL_TRUE;
}

void gli_light_disable(GLenum light)
{
	lights[light-GL_LIGHT0].enabled = GL_FALSE;
}

void glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
	color[0] = red;
	color[1] = green;
	color[2] = blue;
	color[3] = alpha;
}

void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz)
{
	normal[0] = nx;
	normal[1] = ny;
	normal[2] = nz;
}

void glLightx(GLenum light, GLenum pname, GLfixed param)
{
	unsigned l = light - GL_LIGHT0;
	if (l > sizeof_array(lights)) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if ( param < 0 ) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	switch ((enum gli_ColorParam)pname) {
		case GL_SPOT_EXPONENT:
			if (param > (128<<16)) return gli_set_error(GL_INVALID_VALUE);
			lights[l].spot_exponent = param;
			return;
		case GL_SPOT_CUTOFF:
			if (param > (90<<16) && param != (180<<16)) return gli_set_error(GL_INVALID_VALUE);
			lights[l].spot_cutoff = param;
			return;
		case GL_CONSTANT_ATTENUATION:
			lights[l].constant_attenuation = param;
			return;
		case GL_LINEAR_ATTENUATION:
			lights[l].linear_attenuation = param;
			return;
		case GL_QUADRATIC_ATTENUATION:
			lights[l].quadratic_attenuation = param;
			return;
		default:
			return gli_set_error(GL_INVALID_ENUM);
	}
}

void glLightxv(GLenum light, GLenum pname, GLfixed const *params)
{
	if (pname < GL_AMBIENT) {
		return glLightx(light, pname, *params);
	}
	unsigned l = light - GL_LIGHT0;
	if (l > sizeof_array(lights)) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	GLfixed *dest;
	unsigned nb_p = 4;
	switch ((enum gli_ColorParam)pname) {
		case GL_AMBIENT:
			dest = lights[l].ambient;
			break;
		case GL_DIFFUSE:
			dest = lights[l].diffuse;
			break;
		case GL_SPECULAR:
			dest = lights[l].specular;
			break;
		case GL_POSITION:
			dest = lights[l].position;
			break;
		case GL_SPOT_DIRECTION:
			nb_p = 3;
			dest = lights[l].spot_direction;
			break;
		default:
			return gli_set_error(GL_INVALID_ENUM);
	}
	memcpy(dest, params, nb_p*sizeof(*params));
}

void glMaterialx(GLenum face, GLenum pname, GLfixed param)
{
	if (face != GL_FRONT_AND_BACK || pname != GL_SHININESS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	material.shininess = param;
}

void glMaterialxv(GLenum face, GLenum pname, GLfixed const *params)
{
	if (face != GL_FRONT_AND_BACK) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	switch ((enum gli_ColorParam)pname) {
		case GL_AMBIENT:
			memcpy(material.ambient, params, sizeof(material.ambient));
			return;
		case GL_DIFFUSE:
			memcpy(material.diffuse, params, sizeof(material.diffuse));
			return;
		case GL_SPECULAR:
			memcpy(material.specular, params, sizeof(material.specular));
			return;
		case GL_EMISSION:
			memcpy(material.emission, params, sizeof(material.emission));
			return;
		case GL_SHININESS:
			material.shininess = *params;
			return;
		case GL_AMBIENT_AND_DIFFUSE:
			memcpy(material.ambient, params, sizeof(material.ambient));
			memcpy(material.diffuse, params, sizeof(material.diffuse));
			return;
		default:
			return gli_set_error(GL_INVALID_ENUM);
	}
}

void glLightModelx(GLenum pname, GLfixed param)
{
	switch ((enum gli_LightModParam)pname) {
		case GL_LIGHT_MODEL_TWO_SIDE:	// TODO: seams useless
			(void)param;
			return;
		default:
			return gli_set_error(GL_INVALID_ENUM);
	}
}

void glLightModelxv(GLenum pname, GLfixed const *params)
{
	switch ((enum gli_LightModParam)pname) {
		case GL_LIGHT_MODEL_TWO_SIDE:	// TODO: seams useless
			return;
		case GL_LIGHT_MODEL_AMBIENT:
			memcpy(ambient, params, sizeof(ambient));
			return;
		default:
			return gli_set_error(GL_INVALID_ENUM);
	}
}

void glShadeModel(GLenum mode)
{
	if (mode != GL_FLAT && mode != GL_SMOOTH) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	shade_model = mode;
}

void glFrontFace(GLenum mode)
{
	if (mode != GL_CW && mode != GL_CCW) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	front_face = mode;
}
