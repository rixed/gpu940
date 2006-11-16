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
static gpuCmdPoint cmdPoint;
static gpuCmdVector cmdVec[4];
static struct iovec const iov_poly[] = {
	{ .iov_base = &cmdFacet, .iov_len = sizeof(cmdFacet) },
	{ .iov_base = cmdVec+0, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+1, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+2, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+3, .iov_len = sizeof(*cmdVec) },
};
static struct iovec const iov_point[] = {
	{ .iov_base = &cmdPoint, .iov_len = sizeof(cmdPoint) },
	{ .iov_base = &cmdVec, .iov_len = sizeof(cmdVec[0]) },
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

// Returns the color contribution of the material, which is constant for all
// vertexes of an array
static void get_material_color(GLfixed cpri[4])
{
	GLfixed const *ecm = gli_material_emissive();
	GLfixed const *acm = gli_material_ambient();
	GLfixed const *acs = gli_scene_ambient();
	GLfixed const *dcm = gli_material_diffuse();
	cpri[3] = dcm[3];
	for (unsigned i=0; i<3; i++) {
		cpri[i] = ecm[i] + Fix_mul(acm[i], acs[i]);
	}
}

// Returns the color contribution of lights to this particular vertex
static void add_vertex_color(GLfixed cpri[4], int32_t v[4], unsigned vec_idx)
{
	GLfixed const *acm = gli_material_ambient();
	GLfixed const *dcm = gli_material_diffuse();
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
}

// Returns the color of the given vertex
static GLfixed const *get_color(int32_t v[4], unsigned vec_idx)
{
	if (! gli_enabled(GL_LIGHTING)) {
		return gli_current_color();
	}
	static GLfixed cpri[4];
	get_material_color(cpri);
	add_vertex_color(cpri, v, vec_idx);
	return cpri;
}

static int32_t get_luminosity(int32_t v[4], unsigned vec_idx)
{
	// We can only use a luminosity instead of full color component set, if :
	// - acm = dcm (AMBIANT_AND_DIFFUSE like setting)
	// - ecm = 0 (usual case)
	// - acs = _acs*(1,1,1)
	// - dcli = _dcli*(1,1,1)
	// - acli = _acli*(1,1,1)
	// Then, the lighting expression simplify to :
	// color = acm * { acs + Sum(i=0,N-1)[(atti)*(spoti)*(_acli+(scalar(n, VP)*_dcli))] }
	// That is :
	// color = acm * 'luminosity'
	// GPU don't know how to scale, but he can add an intensity to color.
	// This is not the same result, but it's the same idea :->
	GLfixed const _acs = gli_scene_ambient()[0];
	GLfixed lum = _acs;
	GLfixed normal_tmp[4], normal[3];
	gli_normal_get(vec_idx, normal_tmp);
	gli_multmatrixU(GL_MODELVIEW, normal, normal_tmp);
//	GLfixed const *scm = gli_material_specular();
	for (unsigned l=0; l<GLI_MAX_LIGHTS; l++) {	// TODO: use a current_min_light, current_max_light
		if (! gli_light_enabled(l)) continue;
		GLfixed this_lum = 0;
		GLfixed vl_dir[3], vl_dist;
		gli_light_dir(l, v, vl_dir, &vl_dist);
		GLfixed const att = gli_light_attenuation(l, vl_dist);
		GLfixed const spot = gli_light_spot(l);
		GLfixed const att_spot = Fix_mul(att, spot);
		if (att_spot < 16) continue;	// skip light if negligible or null contribution
		GLfixed const _acli = gli_light_ambient(l)[0];
		this_lum = _acli;
		GLfixed n_vl_dir = Fix_scalar(normal, vl_dir);
		if (n_vl_dir > 0) {
			GLfixed const _dcli = gli_light_diffuse(l)[0];
			this_lum += Fix_mul(n_vl_dir, _dcli);
		}
		lum += Fix_mul(this_lum, att_spot);
		// TODO
		//GLfixed scli = gli_light_specular(l);
	}
	// lum is a scale factor to be applied to acm. We change this to a signed value to be added.
	return (lum - (1<<16))<<6;	// this gives good result
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

static void set_current_texture(void)
{
	gpuErr err;
	struct gli_texture_object *to = gli_get_texture_object();
	assert(to->has_data && to->was_bound);
	// if its not resident, load it.
	if (! to->is_resident) {
		to->img_res = gpuAlloc(to->mipmaps[0].width_log, 1U<<to->mipmaps[0].height_log, false);
		if (! to->img_res) return gli_set_error(GL_OUT_OF_MEMORY);
		err = gpuLoadImg(gpuBuf_get_loc(to->img_res), to->img_nores, 0);
		assert(gpuOK == err);
		free(to->img_nores);
		to->img_nores = NULL;
		to->is_resident = true;
	}
	// then, compare with actual tex buffer. if not the same, send SetBuf cmd.
	// FIXME
	static uint32_t last_address = 0, last_width;
	struct buffer_loc const *loc = gpuBuf_get_loc(to->img_res);
	if (loc->address != last_address || loc->width_log != last_width) {
		err = gpuSetBuf(gpuTxtBuffer, to->img_res, true);
		assert(gpuOK == err);
		last_address = loc->address;
		last_width = loc->width_log;
	}
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
	cmdFacet.use_intens = 0;
	// Set facet rendering type and color if needed
	if (gli_texturing()) {
		set_current_texture();
		cmdFacet.rendering_type = rendering_text;
		if (1 /*gli_lighting*/) {
			cmdFacet.use_intens = 1;
			if (! gli_smooth()) {	// intens was not set
				int32_t lum = get_luminosity(vi, ci);
				for (unsigned v=0; v<size; v++) {
					cmdVec[v].u.text_params.i_zb = lum;
				}
			}
		} else {
			cmdFacet.use_intens = 0;
		}
		// Scale UV coord to actual texture sizes
		struct gli_texture_object *to = gli_get_texture_object();
		for (unsigned v=0; v<size; v++) {
			cmdVec[v].u.text_params.u <<= to->mipmaps[0].width_log;
			cmdVec[v].u.text_params.v <<= to->mipmaps[0].height_log;
		}
	} else if (gli_smooth() /*FIXME and gli_lighting*/) {
		if (gli_simple_lighting()) {	// or if we use texture...
			GLfixed c[4];
			get_material_color(c);
			cmdFacet.color = color_GL2gpu(c);
			cmdFacet.rendering_type = rendering_flat;
			cmdFacet.use_intens = 1;
		} else {
			cmdFacet.rendering_type = rendering_smooth;
		}
	} else { /*FIXME and gli_lighting*/
		GLfixed const *c = get_color(vi, ci);
		cmdFacet.color = color_GL2gpu(c);
		cmdFacet.rendering_type = rendering_flat;
	}
	// Optionnaly ask for z-buffer
	set_depth_mode();
	cmdFacet.write_out = gli_color_mask_all && (!depth_test() || gli_depth_func != GL_NEVER);
	cmdFacet.write_z = depth_test() && gli_depth_mask;
	// Send to GPU
	gpuErr const err = gpuWritev(iov_poly, 1+size, true);
	assert(gpuOK == err);
}

static void write_vertex(GLfixed v[4], unsigned vec_idx, GLint arr_idx)
{
	cmdVec[vec_idx].same_as = 0;	// by default
	cmdVec[vec_idx].u.geom.c3d[0] = v[0];
	cmdVec[vec_idx].u.geom.c3d[1] = v[1];
	cmdVec[vec_idx].u.geom.c3d[2] = v[2];
	unsigned z_param = 0;	// we don't use i for now
	if (gli_texturing()) {
		// first, get U and V
		int32_t uv[2];
		gli_texcoord_get(arr_idx, uv);
		cmdVec[vec_idx].u.text_params.u = uv[0];
		cmdVec[vec_idx].u.text_params.v = uv[1];
		z_param = 2;
		// then, if some lighting is on, add intens.
		if (1 /*FIXME gli_lighting*/) {
			if (gli_smooth()) {
				cmdVec[vec_idx].u.text_params.i_zb = get_luminosity(v, arr_idx);
			}
			// else intens will be set later
			// gpu940 can not uniformly lighten a texture (no flat intens), so
			// if !gli_smooth use the same i for all vertexes. But we don't know
			// which intens to use for now, so it must be added later in send_facet.
			z_param ++;
		}
	} else if (gli_smooth() /*FIXME and gli_lighting*/) {
		if (gli_simple_lighting()) {
			cmdVec[vec_idx].u.flat_params.i_zb = get_luminosity(v, arr_idx);
			z_param ++;
		} else {
			GLfixed const *c = get_color(v, arr_idx);
			cmdVec[vec_idx].u.smooth_params.r = Fix_gpuColor1(c[0], c[1], c[2]);
			cmdVec[vec_idx].u.smooth_params.g = Fix_gpuColor2(c[0], c[1], c[2]);
			cmdVec[vec_idx].u.smooth_params.b = Fix_gpuColor3(c[0], c[1], c[2]);
			CLAMP(cmdVec[vec_idx].u.smooth_params.r, 0, 0xFFFF);
			CLAMP(cmdVec[vec_idx].u.smooth_params.g, 0, 0xFFFF);
			CLAMP(cmdVec[vec_idx].u.smooth_params.b, 0, 0xFFFF);
			z_param += 3;
		}
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
	cmdFacet.blend_coef = 0;
	cmdFacet.perspective = 0;
	cmdPoint.opcode = gpuPOINT;
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

void gli_points_array(GLint first, unsigned count)
{
	GLfixed v[4];
	GLint const last = first+count;
	while (first < last) {
		rotate_vertex(v, first);
		cmdVec[0].u.geom.c3d[0] = v[0];
		cmdVec[0].u.geom.c3d[1] = v[1];
		cmdVec[0].u.geom.c3d[2] = v[2];
		GLfixed const *c = gli_current_color();
		cmdPoint.color = color_GL2gpu(c);
		gpuErr const err = gpuWritev(iov_point, 2, true);
		assert(gpuOK == err);
		first ++;
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

