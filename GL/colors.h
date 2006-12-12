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
#ifndef GL_COLORS_H_061010
#define GL_COLORS_H_061010

struct gli_light {
	GLboolean enabled;
	GLfixed spot_exponent, spot_cutoff, constant_attenuation, linear_attenuation, quadratic_attenuation;
	GLfixed ambient[4], diffuse[4], specular[4], position[4], spot_direction[4];
	bool is_simple;
};

struct gli_material {
	GLfixed shininess;
	GLfixed ambient[4], diffuse[4], specular[4], emission[4];
	bool no_emissive;
	bool is_simple;
};

extern GLfixed gli_current_color[4];
extern GLfixed gli_current_normal[3];
extern GLfixed gli_current_texcoord[4];
extern enum gli_ShadeModel gli_shade_model;

int gli_colors_begin(void);
static inline void gli_colors_end(void) { }
void gli_light_enable(unsigned l);
void gli_light_disable(unsigned l);
bool gli_light_enabled(unsigned l);
void gli_light_dir(unsigned l, GLfixed const v[4], GLfixed dir[4], GLfixed *dist);
GLfixed gli_light_attenuation(unsigned l, GLfixed dist);
GLfixed gli_light_spot(unsigned l);
GLfixed const *gli_light_ambient(unsigned l);
GLfixed const *gli_light_diffuse(unsigned l);
GLfixed const *gli_material_emissive(void);
GLfixed const *gli_material_ambient(void);
GLfixed const *gli_material_diffuse(void);
GLfixed const *gli_scene_ambient(void);
bool gli_smooth(void);
bool gli_front_faces_are_cw(void);
bool gli_simple_lighting(void);

#endif
