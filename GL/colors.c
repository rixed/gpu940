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
#include <assert.h>

/*
 * Data Definitions
 */

GLfixed gli_current_color[4];
GLfixed gli_current_normal[3];
static struct gli_light lights[GLI_MAX_LIGHTS];
static struct gli_material material;
static GLfixed ambient[4];
static bool scene_is_simple;	// if scene ambient is _ambient*(1,1,1)
enum gli_ShadeModel gli_shade_model;
static enum gli_FrontFace front_face;
static unsigned nb_simple_lights, nb_enabled_lights;

/*
 * Private Functions
 */

static bool is_simple(GLfixed comp[4])
{
	// TODO: be more tolerant
	return comp[0] == comp[1] && comp[1] == comp[2];	// we don't care about alpha
}

static bool are_near(GLfixed v1[4], GLfixed v2[4])
{
	for (unsigned i=0; i<3; i++) {	// we don't care about alpha
		if (Fix_abs(v1[i]-v2[i]) > 1<<6) return false;
	}
	return true;
}

static void light_update_simple(struct gli_light *light)
{
	bool was_simple = light->is_simple;
	light->is_simple = is_simple(light->diffuse) && is_simple(light->ambient);
	if (light->enabled == GL_TRUE) {
		if (was_simple && !light->is_simple) nb_simple_lights --;
		else if (!was_simple && light->is_simple) nb_simple_lights ++;
	}
}

static void material_update_simple(struct gli_material *mat)
{
	mat->is_simple = are_near(mat->diffuse, mat->ambient) && mat->no_emissive;
}

static void light_ctor(struct gli_light *light)
{
	memset(light, 0, sizeof(*light));
	light->spot_cutoff = 180<<16;
	light->constant_attenuation = 1<<16;
	light->ambient[3] = 1<<16;
	light->position[2] = 1<<16;
	light->spot_direction[2] = -1<<16;
	light->enabled = GL_FALSE;
	light->is_simple = false;
	light_update_simple(light);
}

/*
 * Public Functions
 */

int gli_colors_begin(void)
{
	gli_current_color[0] = gli_current_color[1] = gli_current_color[2] = gli_current_color[3] = 0x10000;
	gli_current_normal[0] = gli_current_normal[1] = 0;
	gli_current_normal[2] = 0x10000;
	nb_enabled_lights = 0;
	nb_simple_lights = 0;
	for (unsigned i=0; i<sizeof_array(lights); i++) {
		light_ctor(lights+i);
	}
	lights[0].diffuse[0] = lights[0].diffuse[1] = lights[0].diffuse[2] = lights[0].diffuse[3] = 1<<16;
	lights[0].specular[0] = lights[0].specular[1] = lights[0].specular[2] = lights[0].specular[3] = 1<<16;
	light_update_simple(lights+0);
	memset(&material, 0, sizeof(material));
	material.ambient[0] = material.ambient[1] = material.ambient[2] = 13107;
	material.ambient[3] = 1<<16;
	material.diffuse[0] = material.diffuse[1] = material.diffuse[2] = 52429;
	material.diffuse[3] = 1<<16;
	material.specular[3] = 1<<16;
	material.emission[3] = 1<<16;
	material.no_emissive = true;
	material_update_simple(&material);
	ambient[0] = ambient[1] = ambient[2] = 13107;
	ambient[3] = 1<<16;
	scene_is_simple = true;
	gli_shade_model = GL_SMOOTH;
	front_face = GL_CCW;
	return 0;
}

extern inline void gli_colors_end(void);

void gli_light_enable(unsigned l)
{
	if (GL_TRUE == lights[l].enabled) return;
	nb_enabled_lights ++;
	lights[l].enabled = GL_TRUE;
	if (lights[l].is_simple) nb_simple_lights ++;
}

void gli_light_disable(unsigned l)
{
	if (GL_FALSE == lights[l].enabled) return;
	nb_enabled_lights --;
	lights[l].enabled = GL_FALSE;
	if (lights[l].is_simple) nb_simple_lights --;
}

bool gli_light_enabled(unsigned l)
{
	return lights[l].enabled;
}

void gli_light_dir(unsigned l, GLfixed const v[4], GLfixed dir[3], GLfixed *dist)
{
	assert(gli_light_enabled(l));
	*dist = 0;
	if (v[3] == 0 && lights[l].position[3] != 0) {
		for (unsigned i=0; i<3; i++) {
			dir[i] = -v[i];
			*dist += Fix_mul(dir[i], dir[i]);
		}
	} else if (lights[l].position[3] == 0 && v[3] != 0) {
		for (unsigned i=0; i<3; i++) {
			dir[i] = lights[l].position[i];
			*dist += Fix_mul(dir[i], dir[i]);
		}
	} else {
		for (unsigned i=0; i<3; i++) {
			dir[i] = lights[l].position[i] - v[i];
			*dist += Fix_mul(dir[i], dir[i]);
		}
	}
	*dist = Fix_sqrt(*dist);
	if (*dist == 0) return;
	GLfixed inv_dist = Fix_inv(*dist);
	for (unsigned i=0; i<3; i++) {
		dir[i] = Fix_mul(dir[i], inv_dist);
	}
}

GLfixed gli_light_attenuation(unsigned l, GLfixed dist)
{
	assert(gli_light_enabled(l));
	if (lights[l].position[3] == 0) {
		return 1<<16;
	}
	GLfixed att = lights[l].constant_attenuation;
	if (lights[l].linear_attenuation != 0) {
		att += Fix_mul(dist, lights[l].linear_attenuation);
	}
	if (lights[l].quadratic_attenuation != 0) {
		att += Fix_mul( Fix_mul(dist, dist), lights[l].quadratic_attenuation);
	}
	return Fix_inv(att);
}

GLfixed gli_light_spot(unsigned l)
{
	assert(gli_light_enabled(l)); (void)l;
	return 1<<16;	// TODO
}

extern inline void glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
extern inline void glColor3x(GLfixed red, GLfixed green, GLfixed blue);
extern inline void glColor3xv(GLfixed const *v);
extern inline void glColor4xv(GLfixed const *v);

extern inline void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz);
extern inline void glNormal3xv(GLfixed const *v);

extern inline void glTexCoord4x(GLfixed s, GLfixed t, GLfixed r, GLfixed q);
extern inline void glTexCoord1x(GLfixed s);
extern inline void glTexCoord2x(GLfixed s, GLfixed t);
extern inline void glTexCoord3x(GLfixed s, GLfixed t, GLfixed r);
extern inline void glTexCoord1xv(GLfixed const *v);
extern inline void glTexCoord2xv(GLfixed const *v);
extern inline void glTexCoord3xv(GLfixed const *v);
extern inline void glTexCoord4xv(GLfixed const *v);

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
	switch ((enum gli_ColorParam)pname) {
		case GL_AMBIENT:
			dest = lights[l].ambient;
			light_update_simple(lights+l);
			break;
		case GL_DIFFUSE:
			dest = lights[l].diffuse;
			light_update_simple(lights+l);
			break;
		case GL_SPECULAR:
			dest = lights[l].specular;
			break;
		case GL_POSITION:
			return gli_multmatrix(GL_MODELVIEW, lights[l].position, params);
		case GL_SPOT_DIRECTION:
			return gli_multmatrixU(GL_MODELVIEW, lights[l].spot_direction, params);
		default:
			return gli_set_error(GL_INVALID_ENUM);
	}
	memcpy(dest, params, 4*sizeof(*params));
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
			material_update_simple(&material);
			return;
		case GL_DIFFUSE:
			memcpy(material.diffuse, params, sizeof(material.diffuse));
			material_update_simple(&material);
			return;
		case GL_SPECULAR:
			memcpy(material.specular, params, sizeof(material.specular));
			return;
		case GL_EMISSION:
			memcpy(material.emission, params, sizeof(material.emission));
			material.no_emissive = material.emission[0] < 16 && material.emission[1] < 16 && material.emission[2] < 16;	// don't care about alpha
			return;
		case GL_SHININESS:
			material.shininess = *params;
			return;
		case GL_AMBIENT_AND_DIFFUSE:
			memcpy(material.ambient, params, sizeof(material.ambient));
			memcpy(material.diffuse, params, sizeof(material.diffuse));
			material_update_simple(&material);
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
			scene_is_simple = is_simple(ambient);
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
	gli_shade_model = mode;
}

void glFrontFace(GLenum mode)
{
	if (mode != GL_CW && mode != GL_CCW) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	front_face = mode;
}

bool gli_front_faces_are_cw(void)
{
	return front_face == GL_CW;
}

GLfixed const *gli_light_ambient(unsigned l)
{
	assert(gli_light_enabled(l));
	return lights[l].ambient;
}

GLfixed const *gli_light_diffuse(unsigned l)
{
	assert(gli_light_enabled(l));
	return lights[l].diffuse;
}

GLfixed const *gli_material_emissive(void)
{
	return material.emission;
}

GLfixed const *gli_material_ambient(void)
{
	return material.ambient;
}

GLfixed const *gli_material_diffuse(void)
{
	return material.diffuse;
}

GLfixed const *gli_scene_ambient(void)
{
	return ambient;
}

bool gli_smooth(void)
{
	return gli_shade_model == GL_SMOOTH;
}

bool gli_simple_lighting(void)
{
	return nb_simple_lights == nb_enabled_lights && material.is_simple && scene_is_simple;
}
