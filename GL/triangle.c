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
#include <assert.h>

/*
 * Data Definitions
 */

static gpuCmdFacet cmdFacet;
static gpuCmdVector cmdVec[3];
static struct iovec const iov_triangle[] = {
	{ .iov_base = &cmdFacet, .iov_len = sizeof(cmdFacet) },
	{ .iov_base = cmdVec+0, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+1, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+2, .iov_len = sizeof(*cmdVec) },
};

/*
 * Private Functions
 */

static void prepare_vertex(GLfixed dest[4], GLint arr_idx)
{
	int32_t c[4];
	gli_vertex_get(arr_idx, c);
	gli_multmatrix(GL_MODELVIEW, dest, c);
}

static GLfixed const *get_vertex_color(unsigned vec_idx)
{
	int32_t v[4];
	gli_vertex_get(vec_idx, v);	// FIXME: This is the second time we get this vertex coordinates
	if (! gli_enabled(GL_LIGHTING)) {
		return gli_current_color();
	}
	static GLfixed cpri[4];
	GLfixed const *ecm = gli_material_emissive();
	GLfixed const *acm = gli_material_ambient();
	GLfixed const *acs = gli_scene_ambient();
	GLfixed const *dcm = gli_material_diffuse();
	cpri[3] = dcm[3];
	for (unsigned i=0; i<3; i++) {
		cpri[i] = ecm[i] + Fix_mul(acm[i], acs[i]);
	}
	GLfixed normal[4];
	gli_normal_get(vec_idx, normal);
//	GLfixed const *scm = gli_material_specular();
	for (unsigned l=0; l<GLI_MAX_LIGHTS; l++) {	// Use a current_min_light, current_max_light
		if (! gli_light_enabled(l)) continue;
		GLfixed vl_dir[3], vl_dist;
		gli_light_dir(l, v, vl_dir, &vl_dist);
		GLfixed att = gli_light_attenuation(l, vl_dist);
		GLfixed spot = gli_light_spot(l);
		GLfixed att_spot = Fix_mul(att, spot);
		if (att_spot < 16) continue;	// skip lights if negligible or null contribution
		GLfixed const *acli = gli_light_ambient(l);
		GLfixed const *dcli = gli_light_diffuse(l);
		for (unsigned i=0; i<3; i++) {
			GLfixed sum = Fix_mul(acm[i], acli[i]);
			cpri[i] += Fix_mul(att_spot, sum);
		}
		GLfixed n_vl_dir = Fix_scalar(normal, vl_dir);
		if (n_vl_dir > 0) {
			GLfixed att_spot_n_vl_dir = Fix_mul(att_spot, n_vl_dir);
			for (unsigned i=0; i<3; i++) {
				GLfixed sum = Fix_mul(dcm[i], dcli[i]);
				cpri[i] += Fix_mul(att_spot_n_vl_dir, sum);
			}
		}
		// TODO
		//GLfixed scli = gli_light_specular(l);
	}
	return &cpri[0];
}

static uint32_t color_GL2gpu(GLfixed const *c)
{
	unsigned r = c[0]>>8;
	unsigned g = c[1]>>8;
	unsigned b = c[2]>>8;
	CLAMP(r, 0, 0xFF);
	CLAMP(g, 0, 0xFF);
	CLAMP(b, 0, 0xFF);	
	return gpuColor(r, g, b);
}

static void send_triangle(GLint ci, bool facet_is_inverted)
{
	// Set cull_mode
	cmdFacet.cull_mode = 0;
#	define VIEWPORT_IS_INVERTED true
	bool front_is_cw = gli_front_faces_are_cw() ^ facet_is_inverted ^ VIEWPORT_IS_INVERTED;
	if (! gli_must_render_face(GL_FRONT)) cmdFacet.cull_mode |= 1<<(!front_is_cw);
	if (! gli_must_render_face(GL_BACK)) cmdFacet.cull_mode |= 2>>(!front_is_cw);
	if (cmdFacet.cull_mode == 3) return;
	// Set facet rendering type and color if needed
	if (gli_smooth()) {
		cmdFacet.rendering_type = rendering_cs;
	} else {
		GLfixed const *c = get_vertex_color(ci);
		cmdFacet.color = color_GL2gpu(c);
		cmdFacet.rendering_type = rendering_c;
	}
	// Send to GPU
	gpuErr err = gpuWritev(iov_triangle, sizeof_array(iov_triangle), true);
	assert(gpuOK == err);
}

static void write_vertex(GLfixed v[4], unsigned vec_idx)
{
	cmdVec[vec_idx].geom.c3d[0] = v[0];
	cmdVec[vec_idx].geom.c3d[1] = v[1];
	cmdVec[vec_idx].geom.c3d[2] = v[2];
	if (gli_smooth()) {
		GLfixed const *c = get_vertex_color(vec_idx);
		cmdVec[vec_idx].c_params.r = Fix_gpuColor1(c[0], c[1], c[2]);
		cmdVec[vec_idx].c_params.g = Fix_gpuColor2(c[0], c[1], c[2]);
		cmdVec[vec_idx].c_params.b = Fix_gpuColor3(c[0], c[1], c[2]);
		CLAMP(cmdVec[vec_idx].c_params.r, 0, 0xFFFF);
		CLAMP(cmdVec[vec_idx].c_params.g, 0, 0xFFFF);
		CLAMP(cmdVec[vec_idx].c_params.b, 0, 0xFFFF);
	}
}

/*
 * Public Functions
 */

int gli_triangle_begin(void)
{
	cmdFacet.opcode = gpuFACET;
	cmdFacet.perspective = 0;
	cmdFacet.size = 3;
	return 0;
}

extern inline void gli_triangle_end(void);

void gli_triangle_array(enum gli_DrawMode mode, GLint first, unsigned count)
{
	if (count < 3) return;
	GLint const last = first + count;
	bool facet_is_inverted = true;
	GLfixed v[4];
	do {
		prepare_vertex(v, first++);
		write_vertex(v, 0);
		prepare_vertex(v, first++);
		write_vertex(v, 1);
		prepare_vertex(v, first++);
		write_vertex(v, 2);
		send_triangle(first - (mode == GL_TRIANGLES ? 3:1), facet_is_inverted);
	} while (mode == GL_TRIANGLES && first < last);
	unsigned repl_idx = 0;
	if (mode == GL_TRIANGLE_FAN) repl_idx = 1;
	while (first < last) {
		facet_is_inverted = !facet_is_inverted;
		prepare_vertex(v, first++);
		write_vertex(v, repl_idx);
		send_triangle(first-1, facet_is_inverted);
		if (++ repl_idx > 2) repl_idx = mode == GL_TRIANGLE_FAN ? 1:0;
	}
}

void gli_clear(GLclampx colors[4])
{
	// Won't work with user clip planes
	// Better implement a true fill function
	static gpuCmdVector vec_bg[] = {
		{ .geom = { .c3d = { -10<<16, -10<<16, -257 } }, },
		{ .geom = { .c3d = {  10<<16, -10<<16, -257 } }, },
		{ .geom = { .c3d = {  10<<16,  10<<16, -257 } }, },
		{ .geom = { .c3d = { -10<<16,  10<<16, -257 } }, },
	};
	static gpuCmdFacet facet_bg = {
		.opcode = gpuFACET,
		.size = sizeof_array(vec_bg),
		.color = 0,
		.rendering_type = rendering_c,
		.perspective = 0,
	};
	static struct iovec cmdvec[1+4] = {
		{ .iov_base = &facet_bg, .iov_len = sizeof(facet_bg) },	// first facet to clear the background
		{ .iov_base = vec_bg+0, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+1, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+2, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+3, .iov_len = sizeof(*vec_bg) }
	};
	facet_bg.color = gpuColor((colors[0]>>8)&0xFF, (colors[1]>>8)&0xFF, (colors[2]>>8)&0xFF);
	gpuErr err = gpuWritev(cmdvec, sizeof_array(cmdvec), true);
	assert(gpuOK == err);
}
