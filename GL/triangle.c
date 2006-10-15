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
static bool facet_is_direct;

/*
 * Private Functions
 */

static void prepare_vertex(GLfixed dest[4], GLint arr_idx)
{
	int32_t c[4];
	gli_vertex_get(arr_idx, c);
	gli_multmatrix(GL_MODELVIEW, dest, c);
}

static void send_triangle()
{
	gpuErr err = gpuWritev(iov_triangle, sizeof_array(iov_triangle), true);
	assert(gpuOK == err);
}

static GLfixed const *get_vertex_color(GLfixed const v[4])
{
	if (! gli_enabled(GL_LIGHTING)) {
		return gli_current_color();
	}
	static GLfixed cpri[4];
	GLfixed const *ecm = gli_material_emissive();
	GLfixed const *acm = gli_material_ambient();
	GLfixed const *acs = gli_scene_ambient();
	cpri[3] = 1<<16;	// TODO
	for (unsigned i=0; i<3; i++) {
		cpri[i] = ecm[i] + Fix_mul(acm[i], acs[i]);
	}
//	GLfixed const *v = gli_vertex();
//	GLfixed const *normal = gli_normal();
//	GLfixed const *dcm = gli_material_diffuse();
//	GLfixed const *scm = gli_material_specular();
	for (unsigned l=0; l<GLI_MAX_LIGHTS; l++) {	// Use a current_min_light, current_max_light
		if (! gli_light_enabled(l)) continue;
		GLfixed vl_dir[3], vl_dist;
		gli_light_dir(l, v, vl_dir, &vl_dist);
		GLfixed att = gli_light_attenuation(l, vl_dist);
		GLfixed spot = gli_light_spot(l);
		GLfixed att_spot = Fix_mul(att, spot);
		if ( att_spot < 16 ) continue;	// skip lights if negligible or null contribution
		GLfixed const *acli = gli_light_ambient(l);
		for (unsigned i=0; i<3; i++) {
			GLfixed sum = Fix_mul(acm[i], acli[i]);
			// TODO
			//GLfixed scli = gli_light_specular(l);
			//GLfixed const *dcli = gli_light_diffuse(l);
			cpri[i] += Fix_mul(att_spot, sum);
		}
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

static void write_vertex(GLfixed v[4], unsigned vec_idx)
{
	cmdVec[vec_idx].geom.c3d[0] = v[0];
	cmdVec[vec_idx].geom.c3d[1] = v[1];
	cmdVec[vec_idx].geom.c3d[2] = v[2];
	if (gli_smooth()) {
		GLfixed const *c = get_vertex_color(v);
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
	facet_is_direct = true;
	GLfixed v[4];
	do {
		prepare_vertex(v, first++);
		write_vertex(v, 0);
		prepare_vertex(v, first++);
		write_vertex(v, 1);
		prepare_vertex(v, first++);
		write_vertex(v, 2);
		send_triangle();
	} while (mode == GL_TRIANGLES && first < last);
	unsigned repl_idx = 0;
	if (mode == GL_TRIANGLE_FAN) repl_idx = 1;
	while (first < last) {
		facet_is_direct = !facet_is_direct;
		prepare_vertex(v, first++);
		write_vertex(v, repl_idx);
		send_triangle();
		if (++ repl_idx > 2) repl_idx = mode == GL_TRIANGLE_FAN ? 1:0;
	}
	if (gli_smooth()) {
		cmdFacet.rendering_type = rendering_cs;
	} else {
		GLfixed const *c = gli_current_color();	// FIXME: uses the color of the first vertex
		cmdFacet.color = color_GL2gpu(c);
		cmdFacet.rendering_type = rendering_c;
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
