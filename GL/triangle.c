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
static gpuCmdVector cmdVec[4];
static struct iovec const iov_triangle[] = {
	{ .iov_base = &cmdFacet, .iov_len = sizeof(cmdFacet) },
	{ .iov_base = cmdVec+0, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+1, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+2, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+3, .iov_len = sizeof(*cmdVec) },
};

/*
 * Private Functions
 */

static void rotate_vertex(GLfixed dest[4], GLint arr_idx)
{
	int32_t c[4];
	gli_vertex_get(arr_idx, c);
	gli_multmatrix(GL_MODELVIEW, dest, c);
}

static GLfixed const *get_vertex_color(int32_t v[4], unsigned vec_idx)
{
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
	GLfixed normal_tmp[4], normal[3];
	gli_normal_get(vec_idx, normal_tmp);
	gli_multmatrixU(GL_MODELVIEW, normal, normal_tmp);
//	GLfixed const *scm = gli_material_specular();
	for (unsigned l=0; l<GLI_MAX_LIGHTS; l++) {	// TODO: use a current_min_light, current_max_light
		if (! gli_light_enabled(l)) continue;
		GLfixed vl_dir[3], vl_dist;
		gli_light_dir(l, v, vl_dir, &vl_dist);
		GLfixed const att = gli_light_attenuation(l, vl_dist);
		GLfixed const spot = gli_light_spot(l);
		GLfixed const att_spot = Fix_mul(att, spot);
		if (att_spot < 16) continue;	// skip light if negligible or null contribution
		GLfixed const *acli = gli_light_ambient(l);
		GLfixed const *dcli = gli_light_diffuse(l);
		for (unsigned i=0; i<3; i++) {
			GLfixed sum = Fix_mul(acm[i], acli[i]);
			cpri[i] += Fix_mul(att_spot, sum);
		}
		GLfixed n_vl_dir = Fix_scalar(normal, vl_dir);
		assert(n_vl_dir < 70000);
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

static int32_t get_vertex_depth(int32_t v[4])
{
	return -v[2];	// we don't use depth range here, so for us depth = -z
}

static bool depth_test(void)
{
	return gli_with_depth_buffer && gli_enabled(GL_DEPTH_TEST);
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

// Send a ZMode command if needed
static void set_depth_mode(void)
{
	static gpuCmdZMode cmd = {
		.opcode = gpuZMODE,
		.mode = gpu_z_off,
	};
	gpuZMode needed = gpu_z_off;
	if (depth_test()) {
		switch (gli_depth_func) {
			case GL_NEVER:
			case GL_ALWAYS:
				needed = gpu_z_off;
				break;
			case GL_LESS:
				needed = gpu_z_lt;
				break;
			case GL_EQUAL:
				needed = gpu_z_eq;
				break;
			case GL_LEQUAL:
				needed = gpu_z_lte;
				break;
			case GL_GREATER:
				needed = gpu_z_gt;
				break;
			case GL_NOTEQUAL:
				needed = gpu_z_ne;
				break;
			case GL_GEQUAL:
				needed = gpu_z_gte;
				break;
		}
	}
	if (needed != cmd.mode) {
		cmd.mode = needed;
		gpuErr const err = gpuWrite(&cmd, sizeof(cmd), true);
		assert(gpuOK == err);
	}
}

static void send_facet(
	int32_t vi[4], // vertex that can give its color to facet
	GLint ci,	// idx of this vertex
	bool facet_is_inverted,
	unsigned size)
{
	// Set cull_mode
	cmdFacet.cull_mode = 0;
#	define VIEWPORT_IS_INVERTED false
	bool front_is_cw = gli_front_faces_are_cw() ^ facet_is_inverted ^ VIEWPORT_IS_INVERTED;
	if (! gli_must_render_face(GL_FRONT)) cmdFacet.cull_mode |= 1<<(!front_is_cw);
	if (! gli_must_render_face(GL_BACK)) cmdFacet.cull_mode |= 2>>(!front_is_cw);
	if (cmdFacet.cull_mode == 3) return;
	cmdFacet.size = size;
	// Set facet rendering type and color if needed
	if (gli_smooth()) {
		cmdFacet.rendering_type = rendering_smooth;
	} else {
		GLfixed const *c = get_vertex_color(vi, ci);
		cmdFacet.color = color_GL2gpu(c);
		cmdFacet.rendering_type = rendering_flat;
	}
	// Optionnaly ask for z-buffer
	set_depth_mode();
	cmdFacet.write_out = gli_color_mask_all && (!depth_test() || gli_depth_func != GL_NEVER);
	cmdFacet.write_z = depth_test() && gli_depth_mask;
	// Send to GPU
	gpuErr const err = gpuWritev(iov_triangle, 1+size, true);
	assert(gpuOK == err);
}

static void write_vertex(GLfixed v[4], unsigned vec_idx, GLint arr_idx)
{
	cmdVec[vec_idx].same_as = 0;	// by default
	cmdVec[vec_idx].u.geom.c3d[0] = v[0];
	cmdVec[vec_idx].u.geom.c3d[1] = v[1];
	cmdVec[vec_idx].u.geom.c3d[2] = v[2];
	unsigned z_param = 0;	// we don't use i or now
	if (gli_smooth()) {
		GLfixed const *c = get_vertex_color(v, arr_idx);
		cmdVec[vec_idx].u.smooth_params.r = Fix_gpuColor1(c[0], c[1], c[2]);
		cmdVec[vec_idx].u.smooth_params.g = Fix_gpuColor2(c[0], c[1], c[2]);
		cmdVec[vec_idx].u.smooth_params.b = Fix_gpuColor3(c[0], c[1], c[2]);
		CLAMP(cmdVec[vec_idx].u.smooth_params.r, 0, 0xFFFF);
		CLAMP(cmdVec[vec_idx].u.smooth_params.g, 0, 0xFFFF);
		CLAMP(cmdVec[vec_idx].u.smooth_params.b, 0, 0xFFFF);
		z_param += 3;
	}
	// Add zb parameter if needed
	if (depth_test()) {
		cmdVec[vec_idx].u.geom.param[z_param] = get_vertex_depth(v);
	}
}

static void gli_triangles_array(enum gli_DrawMode mode, GLint first, unsigned count)
{
	if (count < 3) return;
	GLint const last = first + count;
	bool facet_is_inverted = false;
	GLfixed v[4], first_v[4];
	do {
		rotate_vertex(first_v, first);
		write_vertex(first_v, 0, first++);
		rotate_vertex(v, first);
		write_vertex(v, 1, first++);
		rotate_vertex(v, first);
		write_vertex(v, 2, first++);
		send_facet(mode == GL_TRIANGLES ? first_v:v, first - (mode == GL_TRIANGLES ? 3:1), facet_is_inverted, 3);
	} while (mode == GL_TRIANGLES && first < last);
	unsigned repl_idx = 0;
	if (mode == GL_TRIANGLE_FAN) repl_idx = 1;
	while (first < last) {
		facet_is_inverted = !facet_is_inverted;
		for (unsigned i=0; i<3; i++) {
			if (i == repl_idx) {
				rotate_vertex(v, first);
				write_vertex(v, i, first++);
			} else {
				cmdVec[i].same_as = 3;
			}
		}
		send_facet(v, first-1, facet_is_inverted, 3);
		if (++ repl_idx > 2) repl_idx = mode == GL_TRIANGLE_FAN ? 1:0;
	}
}

static void gli_quads_array(enum gli_DrawMode mode, GLint first, unsigned count)
{
	if (count < 4) return;
	GLint const last = first + count;
	bool facet_is_inverted = false;
	GLfixed v[4], first_v[4];
	do {
		rotate_vertex(first_v, first);
		write_vertex(first_v, 0, first++);
		rotate_vertex(v, first);
		write_vertex(v, 1, first++);
		rotate_vertex(v, first);
		write_vertex(v, mode == GL_QUADS ? 2:3, first++);	// quad_strips vertexes are 'crossed'
		rotate_vertex(v, first);
		write_vertex(v, mode == GL_QUADS ? 3:2, first++);
		send_facet(mode == GL_QUADS ? first_v:v, first - (mode == GL_QUADS ? 4:1), facet_is_inverted, 4);
	} while (mode == GL_QUADS && first < last);
	unsigned repl_idx = 0;
	while (first < last) {
		facet_is_inverted = !facet_is_inverted;
		rotate_vertex(v, first);
		write_vertex(v, repl_idx == 0 ? 0:3, first++);
		rotate_vertex(v, first);
		write_vertex(v, repl_idx == 0 ? 1:2, first++);
		unsigned const norep = repl_idx == 0 ? 2:0;
		cmdVec[norep].same_as = 4;
		cmdVec[norep+1].same_as = 4;
		send_facet(v, first-1, facet_is_inverted, 4);
		repl_idx = (repl_idx + 2) & 3;
	}
}

/*
 * Public Functions
 */

int gli_triangle_begin(void)
{
	cmdFacet.opcode = gpuFACET;
	cmdFacet.use_key = 0;
	cmdFacet.use_intens = 0;
	cmdFacet.blend_coef = 0;
	cmdFacet.perspective = 0;
	return 0;
}

extern inline void gli_triangle_end(void);

void gli_facet_array(enum gli_DrawMode mode, GLint first, unsigned count)
{
	if (mode >= GL_TRIANGLE_STRIP && mode <= GL_TRIANGLES) {
		gli_triangles_array(mode, first, count);
	} else {
		assert(mode == GL_QUAD_STRIP || mode == GL_QUADS);
		gli_quads_array(mode, first, count);
	}
}

void gli_clear(gpuBufferType type, GLclampx *val)
{
	static gpuCmdRect rect = {
		.opcode = gpuRECT,
		.type = 0,
		.pos = { 0, 0 },
		.width = SCREEN_WIDTH,
		.height = SCREEN_HEIGHT,
		.relative_to_window = 1,
		.value = 0,
	};
	rect.type = type;
	if (type == gpuZBuffer) {
		if (! gli_with_depth_buffer) return;
		rect.value = *val;
	} else {	// color
		rect.value = gpuColor((val[0]>>8)&0xFF, (val[1]>>8)&0xFF, (val[2]>>8)&0xFF);
	}
	gpuErr const err = gpuWrite(&rect, sizeof(rect), true);
	assert(gpuOK == err);
}

uint32_t *gli_get_texture_address(struct gpuBuf *const buf)
{
	return &shared->buffers[gpuBuf_get_loc(buf)->address];
}

