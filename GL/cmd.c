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
static gpuCmdMode cmdMode = {
	.opcode = gpuMODE,
	.mode = {
		.named = {
			.perspective = 0,
			.use_txt_blend = 0,
		},
	},
};
static gpuCmdFacet cmdFacet = {
	.opcode = gpuFACET,
};
static gpuCmdPoint cmdPoint = {
	.opcode = gpuPOINT,
};
static gpuCmdLine cmdLine = {
	.opcode = gpuLINE,
};
static gpuCmdVector cmdVec[8];	// Note: if you change its size, also change iov_poly
static unsigned vec_idx;	// used to address cmdVec. Not necessarily equal to count
static unsigned count;	// count vertexes between begin and end
static unsigned prim;	// count primitives between begin and end
static bool (*is_colorer_func)(void);	// tells wether te count vertex is the colorer of the prim primitive (flatshading)
static GLfixed colorer_alpha;

static struct iovec const iov_poly[] = {
	{ .iov_base = &cmdFacet, .iov_len = sizeof(cmdFacet) },
	{ .iov_base = cmdVec+0, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+1, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+2, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+3, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+4, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+5, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+6, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+7, .iov_len = sizeof(*cmdVec) },
};
static struct iovec const iov_point[] = {
	{ .iov_base = &cmdPoint, .iov_len = sizeof(cmdPoint) },
	{ .iov_base = &cmdVec, .iov_len = sizeof(cmdVec[0]) },
};
static struct iovec const iov_line[] = {
	{ .iov_base = &cmdLine, .iov_len = sizeof(cmdLine) },
	{ .iov_base = cmdVec+0, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+1, .iov_len = sizeof(*cmdVec) },
};

/*
 * Private Functions
 */

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
static void add_vertex_color(GLfixed cpri[4], GLfixed v_eye[4])
{
	GLfixed const *acm = gli_material_ambient();
	GLfixed const *dcm = gli_material_diffuse();
	GLfixed normal[3];
	gli_multmatrixU(GL_MODELVIEW, normal, gli_current_normal);
//	GLfixed const *scm = gli_material_specular();
	for (unsigned l=0; l<GLI_MAX_LIGHTS; l++) {	// TODO: use a current_min_light, current_max_light
		if (! gli_light_enabled(l)) continue;
		GLfixed vl_dir[3], vl_dist;
		gli_light_dir(l, v_eye, vl_dir, &vl_dist);
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
static GLfixed const *get_color(GLfixed v_eye[4])
{
	if (! gli_enabled(GL_LIGHTING)) {
		return gli_current_color;
	}
	static GLfixed cpri[4];
	get_material_color(cpri);
	add_vertex_color(cpri, v_eye);
	return cpri;
}

static bool depth_test(void)
{
	return gli_with_depth_buffer && gli_enabled(GL_DEPTH_TEST);
}
static bool z_param_needed(void)
{
	return depth_test() && (gli_depth_mask || (gli_depth_func != GL_ALWAYS && gli_depth_func != GL_NEVER));
}

static uint32_t color_GL2gpu(int32_t const *c)
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
	assert(gpuOK == err); (void)err;
}

static void set_current_texture(struct gli_mipmap_data *mm)
{
	gpuErr err;
	// if its not resident, load it.
	if (! mm->is_resident) {
		mm->img_res = gpuAlloc(mm->width_log, 1U<<mm->height_log, false);
		if (! mm->img_res) return gli_set_error(GL_OUT_OF_MEMORY);
		err = gpuLoadImg(gpuBuf_get_loc(mm->img_res), mm->img_nores);
		assert(gpuOK == err); (void)err;
		free(mm->img_nores);
		mm->img_nores = NULL;
		mm->is_resident = true;
	}
	// then, compare with actual tex buffer. if not the same, send SetBuf cmd.
	static uint32_t last_address = 0, last_width;
	struct buffer_loc const *loc = gpuBuf_get_loc(mm->img_res);
	if (loc->address != last_address || loc->width_log != last_width) {
		err = gpuSetBuf(gpuTxtBuffer, mm->img_res, true);
		assert(gpuOK == err); (void)err;
		last_address = loc->address;
		last_width = loc->width_log;
	}
}

static gpuZMode get_depth_mode(void)
{
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
	return needed;
}

static bool require_mipmap(enum gli_TexFilter filter)
{
	return
		filter == GL_NEAREST_MIPMAP_NEAREST ||
		filter == GL_NEAREST_MIPMAP_LINEAR ||
		filter == GL_LINEAR_MIPMAP_NEAREST ||
		filter == GL_LINEAR_MIPMAP_LINEAR;
}

static void set_mode_and_color(uint32_t *color, unsigned nb_vec, unsigned colorer)
{
	cmdMode.mode.named.use_intens = 0;
	cmdMode.mode.named.use_key = 0;
	GLfixed alpha = colorer_alpha;
	// Set facet rendering type and color if needed
	if (gli_texturing()) {
		struct gli_texture_object *to = gli_get_texture_object();
		unsigned level = 0;
		// FIXME: this is not satisfactory
		if (nb_vec > 1 && require_mipmap(to->min_filter) && cmdVec[0].u.geom.c3d[2] != 0) {
			unsigned nb_texels = (cmdVec[1].u.text.u - cmdVec[0].u.text.u) << to->mipmaps[level].width_log;	// TODO: check its width for u
			unsigned nb_pixels = Fix_div(cmdVec[1].u.geom.c3d[0] - cmdVec[0].u.geom.c3d[0], cmdVec[0].u.geom.c3d[2]);
			level = 3;
//			while (nb_texels > nb_pixels && level < sizeof_array(to->mipmaps)-1 && to->mipmaps[level+1].has_data) {
//				level ++;
//				nb_texels >>= 1;
//			}
		}
		set_current_texture(to->mipmaps + level);
		cmdMode.mode.named.rendering_type = rendering_text;
		if (to->mipmaps[level].need_key) {
			cmdMode.mode.named.use_key = 1;
			*color = gpuColorAlpha(KEY_RED, KEY_GREEN, KEY_BLUE, KEY_ALPHA);
		}
		if (to->mipmaps[level].have_mean_alpha) {
			alpha = Fix_mul(alpha, to->mipmaps[level].mean_alpha);
		}
#		define ALMOST_0 (0x2000<<7)
		if (! gli_smooth()) {	// copy colorer intens
			int32_t const colorer_i = cmdVec[colorer].u.text.i;
			if (Fix_abs(colorer_i) > ALMOST_0) {
				cmdMode.mode.named.use_intens = 1;
				for (unsigned v=nb_vec; v--; ) {
					cmdVec[v].u.text.i = colorer_i;
				}
			}
		} else {
			for (unsigned v=nb_vec; v--; ) {
				if (Fix_abs(cmdVec[v].u.text.i) > ALMOST_0) {
					cmdMode.mode.named.use_intens = 1;
					break;
				}
			}
		}
	} else if (gli_smooth()) {
		cmdMode.mode.named.rendering_type = rendering_smooth;
	} else {	// flat
		*color = gpuColor_comp2uint(&cmdVec[colorer].u.smooth.r);
		cmdMode.mode.named.rendering_type = rendering_flat;
	}
	// Use blending ?
	unsigned blend_coef = 0;
	if (gli_enabled(GL_BLEND)) {
		blend_coef = 4 - ((alpha+0x2000)>>14);
	}
	// z-buffer and masks
	cmdMode.mode.named.z_mode = get_depth_mode();
	cmdMode.mode.named.write_out = gli_color_mask_all && (!depth_test() || gli_depth_func != GL_NEVER) && blend_coef != 4;
	cmdMode.mode.named.write_z = depth_test() && gli_depth_mask;
	assert((cmdMode.mode.named.z_mode != gpu_z_off || cmdMode.mode.named.write_z) == z_param_needed());
	if (!cmdMode.mode.named.write_out && !cmdMode.mode.named.write_z) {
		return;
	}
	cmdMode.mode.named.blend_coef = blend_coef & 3;
	// Send to GPU
	static uint32_t prev_rendering_flags = ~0;
	gpuErr err;
	if (cmdMode.mode.flags != prev_rendering_flags) {
		err = gpuWrite(&cmdMode, sizeof(cmdMode), true);
		assert(gpuOK == err); (void)err;
		prev_rendering_flags = cmdMode.mode.flags;
	}
}

static void line_complete(unsigned colorer)
{
	prim ++;
	set_mode_and_color(&cmdLine.color, 2, colorer);
	gpuErr err = gpuWritev(iov_line, 3, true);
	assert(gpuOK == err); (void)err;
}

static void facet_complete(unsigned size, bool facet_is_inverted, unsigned colorer)
{
	// Set cull_mode
	prim ++;
	cmdFacet.cull_mode = 0;
#	define VIEWPORT_IS_INVERTED true
	bool front_is_cw = gli_front_faces_are_cw() ^ facet_is_inverted ^ VIEWPORT_IS_INVERTED;
	if (! gli_must_render_face(GL_FRONT)) cmdFacet.cull_mode |= 1<<(!front_is_cw);
	if (! gli_must_render_face(GL_BACK)) cmdFacet.cull_mode |= 2>>(!front_is_cw);
	if (cmdFacet.cull_mode == 3) return;
	cmdFacet.size = size;
	set_mode_and_color(&cmdFacet.color, size, colorer);
	gpuErr err = gpuWritev(iov_poly, 1+size, true);
	assert(gpuOK == err); (void)err;
}

static bool unused(void)
{
	return false;
}

static bool is_colorer_line_strip(void)
{
	return (count & 1) == 0;
}

static bool is_colorer_polygon(void)
{
	return count == 0;
}

static bool is_colorer_triangle_strip_or_fan(void)
{
	return count == prim + 2;
}

static bool is_colorer_triangles(void)
{
	return count == prim+prim+prim + 2;
}

static bool is_colorer_quad_strip(void)
{
	return count == prim+prim + 3;
}

static bool is_colorer_quads(void)
{
	return count == (prim<<2) + 3;
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
	prim = 0;
	typedef bool colorer_func(void);
	static colorer_func *colorer_funcs[] = {
		unused, is_colorer_line_strip, unused, unused,
		is_colorer_triangle_strip_or_fan, is_colorer_triangle_strip_or_fan, is_colorer_triangles,
		is_colorer_quad_strip, is_colorer_quads,
		is_colorer_polygon
	};
	is_colorer_func = colorer_funcs[mode_];
}

void gli_cmd_submit(void)
{
	if (mode != GL_POLYGON) return;
	if (vec_idx > 2) facet_complete(vec_idx, false, 0);
}

void gli_cmd_vertex(int32_t const *v)
{
	// TODO : precalc P*M
	GLfixed eye_coords[4], clip_coords[4];
	gli_multmatrix(GL_MODELVIEW, eye_coords, v);
	gli_multmatrix(GL_PROJECTION, clip_coords, eye_coords);
	cmdVec[vec_idx].u.geom.c3d[0] = ((int64_t)clip_coords[0] * gli_viewport_width/2) >> 8;	// FIXME: set dproj = 1
	cmdVec[vec_idx].u.geom.c3d[1] = -((int64_t)clip_coords[1] * gli_viewport_height/2) >> 8;	// gpu940 uses Y toward bottom
	cmdVec[vec_idx].u.geom.param[0] = clip_coords[3];	// we use W as Z for gpu940 (which then must use Z toward depths)
	cmdVec[vec_idx].same_as = 0;
	// Now compute vertex colors
	bool is_colorer = is_colorer_func();
	GLfixed const *c = NULL;
	if (gli_smooth() || is_colorer) {
		c = get_color(eye_coords);
		GLfixed r = c[0];
		CLAMP(r, 0, 0xFFFF);
		GLfixed g = c[1];
		CLAMP(g, 0, 0xFFFF);
		GLfixed b = c[2];
		CLAMP(b, 0, 0xFFFF);
		cmdVec[vec_idx].u.smooth.r = Fix_gpuColor1(r, g, b);
		cmdVec[vec_idx].u.smooth.g = Fix_gpuColor2(r, g, b);
		cmdVec[vec_idx].u.smooth.b = Fix_gpuColor3(r, g, b);
		if (is_colorer) {
			colorer_alpha = c[3];	// save it for later
			CLAMP(colorer_alpha, 0, 0xFFFF);
		}
	} // else will be set later
	if (gli_texturing()) {
		// We cannot use both colors and texture. Convert colors to a luminosity.
		if (c) {
			int32_t text_i = ((( c[0] + c[1] + c[1] + c[2]) >> 2) - 0xFFFF) << 7;
			cmdVec[vec_idx].u.text.i = text_i;
		} // else will be set later
		// Set U and V (GPU will scale to actual texture size)
		cmdVec[vec_idx].u.text.u = gli_texture_unit.texcoord[0];
		cmdVec[vec_idx].u.text.v = gli_texture_unit.texcoord[1];
	}
	count ++;
	// FIXME: call an advance_facet() func pointer set in prepare facet to avoid this switch ?
	switch (mode) {
		case GL_POINTS:
			point_complete();
			vec_idx = 0;
			break;
		case GL_LINE_LOOP:	// We should keep the first vec, and use it when glEnd() is called. But Im tired.
		case GL_LINE_STRIP:
			if (count > 2) {
				cmdVec[!vec_idx].same_as = 1;
			}
			if (count > 1) {
				line_complete(0);
			}
			vec_idx = !vec_idx;
			break;
		case GL_LINES:
			vec_idx = !vec_idx;
			if (vec_idx == 0) {
				line_complete(0);
			}
			break;
		case GL_POLYGON:
			vec_idx++;
			if (vec_idx >= sizeof_array(cmdVec)) {	// split the polygon into several polygons
				facet_complete(vec_idx, false, 0);
				cmdVec[1] = cmdVec[sizeof_array(cmdVec)-1];
				vec_idx = 2;
				count = 2;
			}
			break;
		case GL_TRIANGLE_STRIP:
			if (count < 3) {
				vec_idx ++;
			} else {
				if (count > 3) {
					cmdVec[(vec_idx+1)%3].same_as = 3;
					cmdVec[(vec_idx+2)%3].same_as = 3;
				}
				facet_complete(3, ! (count & 1), vec_idx);
				vec_idx ++;
				if (vec_idx >= 3) vec_idx = 0;
			}
			break;
		case GL_TRIANGLE_FAN:
			if (count < 3) {
				vec_idx ++;
			} else {
				if (count > 3) {
					cmdVec[(vec_idx+1)%3].same_as = 3;
					cmdVec[(vec_idx+2)%3].same_as = 3;
				}
				facet_complete(3, ! (count & 1), vec_idx);
				vec_idx = vec_idx+1;
				if (vec_idx >= 3) vec_idx = 1;
			}
			break;
		case GL_TRIANGLES:
			if (vec_idx == 2) {
				facet_complete(3, false, vec_idx);
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
				cmdVec[(vec_idx+2)%4].same_as = 4;
				if (! (count & 1)) {
					facet_complete(4, (count & 2), vec_idx);
					vec_idx = (vec_idx + 2) & 3;
				} else {
					if (vec_idx == 0) vec_idx = 1;
					else vec_idx = 2;
				}
			}
			break;
		case GL_QUADS:
			if (vec_idx == 3) {
				facet_complete(4, false, vec_idx);
				vec_idx = 0;
			} else {
				vec_idx ++;
			}
			break;
	}
}

void gli_points_array(GLint first, unsigned count)
{
	GLint const last = first+count;
	glBegin(GL_POINTS);
	while (first < last) {
		gli_color_set(first);
		gli_vertex_set(first);
		first ++;
	}
	glEnd();
}

void gli_facet_array(enum gli_DrawMode mode, GLint first, unsigned count)
{
	GLint const last = first+count;
	glBegin(mode);
	while (first < last) {
		gli_color_set(first);
		gli_normal_set(first);
		gli_texcoord_set(first);
		gli_vertex_set(first);
		first ++;
	}
	glEnd();
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
	assert(gpuOK == err); (void)err;
}

extern inline uint32_t *gli_get_texture_address(struct gpuBuf *const buf);

