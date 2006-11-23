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
#include <stdlib.h>
#include <assert.h>

/*
 * Data Definitions
 */

static enum gli_DrawMode mode;
static gpuCmdFacet cmdFacet = {
	.opcode = gpuFACET,
	.use_key = 0,
	.blend_coef = 0,
	.perspective = 0,
};
static gpuCmdPoint cmdPoint = {
	.opcode = gpuPOINT,
};
static gpuCmdVector cmdVec[4];
static unsigned vec_idx;
static unsigned count;	// count vertexes between begin and end

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

static void reset_facet(void)
{
	cmdFacet.size = mode >= GL_TRIANGLE_STRIP && mode <= GL_TRIANGLES ? 3:4;
}

static int32_t get_luminosity(unsigned vidx)
{
	// We can use a luminosity alone instead of full color component set, if :
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
	GLfixed normal[3];
	gli_multmatrixU(GL_MODELVIEW, normal, gli_current_normal);
//	GLfixed const *scm = gli_material_specular();
	for (unsigned l=0; l<GLI_MAX_LIGHTS; l++) {	// TODO: use a current_min_light, current_max_light
		if (! gli_light_enabled(l)) continue;
		GLfixed this_lum = 0;
		GLfixed vl_dir[3], vl_dist;
		gli_light_dir(l, cmdVec[vidx].u.geom.c3d, vl_dir, &vl_dist);
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
static void add_vertex_color(GLfixed cpri[4], unsigned vidx)
{
	GLfixed const *acm = gli_material_ambient();
	GLfixed const *dcm = gli_material_diffuse();
	GLfixed normal[3];
	gli_multmatrixU(GL_MODELVIEW, normal, gli_current_normal);
//	GLfixed const *scm = gli_material_specular();
	for (unsigned l=0; l<GLI_MAX_LIGHTS; l++) {	// TODO: use a current_min_light, current_max_light
		if (! gli_light_enabled(l)) continue;
		GLfixed vl_dir[3], vl_dist;
		gli_light_dir(l, cmdVec[vidx].u.geom.c3d, vl_dir, &vl_dist);
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

// Returns the color of the current vertex
static GLfixed const *get_color(unsigned vidx)
{
	if (! gli_enabled(GL_LIGHTING)) {
		return gli_current_color;
	}
	static GLfixed cpri[4];
	get_material_color(cpri);
	add_vertex_color(cpri, vidx);
	return cpri;
}

static int32_t get_vertex_depth(unsigned vidx)
{
	return -cmdVec[vidx].u.geom.c3d[2];	// we don't use depth range here, so for us depth = -z
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

static void point_complete(void)
{
	cmdPoint.color = color_GL2gpu(gli_current_color);
	gpuErr const err = gpuWritev(iov_point, 2, true);
	assert(gpuOK == err);
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

static void facet_complete(unsigned size, bool facet_is_inverted, unsigned colorer_idx)
{
	// FIXME: most of these setting shoulb be done in cmd_prepare and not for each facet
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
		if (gli_enabled(GL_LIGHTING)) {
			cmdFacet.use_intens = 1;
			if (! gli_smooth()) {	// intens was not set
				int32_t lum = get_luminosity(colorer_idx);
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
	} else if (gli_smooth && gli_enabled(GL_LIGHTING)) {
		if (gli_simple_lighting()) {	// or if we use texture...
			GLfixed c[4];
			get_material_color(c);
			cmdFacet.color = color_GL2gpu(c);
			cmdFacet.rendering_type = rendering_flat;
			cmdFacet.use_intens = 1;
		} else {
			cmdFacet.rendering_type = rendering_smooth;
		}
	} else {
		GLfixed const *c = get_color(colorer_idx);
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

/*
 * Public Functions
 */

extern inline int gli_cmd_begin(void);
extern inline void gli_cmd_end(void);

void gli_cmd_prepare(enum gli_DrawMode mode_)
{
	mode = mode_;
	vec_idx = 0;
	count = 0;
	reset_facet();
}

void gli_cmd_vertex(int32_t const x, int32_t const y, int32_t const z, int32_t const w)
{
	GLfixed const v[4] = { x, y, z, w };	// please compiler, don't copy stacked arguments !
	gli_multmatrix(GL_MODELVIEW, cmdVec[vec_idx].u.geom.c3d, v);
	cmdVec[vec_idx].same_as = 0;	// by default (TODO: change it later)
	unsigned z_param = 0;
	if (gli_texturing()) {
		// first, set U and V
		cmdVec[vec_idx].u.text_params.u = gli_current_texcoord[0];
		cmdVec[vec_idx].u.text_params.v = gli_current_texcoord[1];
		z_param = 2;
		// then, if some lighting is on, add intens.
		if (gli_enabled(GL_LIGHTING)) {
			if (gli_smooth()) {
				cmdVec[vec_idx].u.text_params.i_zb = get_luminosity(vec_idx);
			}
			// else intens will be set later
			// gpu940 can not uniformly lighten a texture (no flat intens), so
			// if !gli_smooth use the same i for all vertexes. But we don't know
			// which intens to use for now, so it must be added later in cmd_complete.
			z_param ++;
		}
	} else if (gli_smooth() && gli_enabled(GL_LIGHTING)) {
		if (gli_simple_lighting()) {
			cmdVec[vec_idx].u.flat_params.i_zb = get_luminosity(vec_idx);
			z_param ++;
		} else {
			GLfixed const *c = get_color(vec_idx);
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
		cmdVec[vec_idx].u.geom.param[z_param] = get_vertex_depth(vec_idx);
	}
	count ++;
	switch (mode) {
		case GL_POINTS:
			point_complete();
			vec_idx = 0;
			break;
		case GL_LINE_STRIP:
		case GL_LINE_LOOP:
		case GL_LINES:
			assert(0);	// TODO
		case GL_TRIANGLE_STRIP:
			if (count < 3) {
				vec_idx ++;
			} else if (count == 3) {
				facet_complete(3, false, vec_idx);
				vec_idx = 0;
			} else {	// each new vertex gives a new triangle
				facet_complete(3, ! (count & 1), vec_idx);
				vec_idx = vec_idx+1;
				if (vec_idx >= 3) vec_idx = 0;
			}
			break;
		case GL_TRIANGLE_FAN:
			if (count < 3) {
				vec_idx ++;
			} else if (count == 3) {
				facet_complete(3, false, vec_idx);
				vec_idx = 1;
			} else {	// each new vertex gives a new triangle
				facet_complete(3, ! (count & 1), vec_idx);
				vec_idx = vec_idx+1;
				if (vec_idx >= 3) vec_idx = 1;
			}
			break;
		case GL_TRIANGLES:
			if (vec_idx == 2) {
				facet_complete(3, false, 0);
				vec_idx = 0;
			} else {
				vec_idx ++;
			}
			break;
		case GL_QUAD_STRIP:
			if (count == 1) {
				vec_idx = 1;
			} else if (count == 2) {
				vec_idx = 3;
			} else if (count == 3) {
				vec_idx = 2;
			} else if (count == 4) {
				facet_complete(4, false, vec_idx);
				vec_idx = 0;
			} else {
				if (! (count & 1)) {
					facet_complete(4, ! (count & 2), vec_idx);
					vec_idx = (vec_idx + 2) & 3;
				} else {
					if (vec_idx == 0) vec_idx = 1;
					else vec_idx = 2;
				}
			}
			break;
		case GL_QUADS:
			if (vec_idx == 3) {
				facet_complete(4, false, 0);
				vec_idx = 0;
			} else {
				vec_idx ++;
			}
			break;
	}
}

