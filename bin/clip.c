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
#include <limits.h>
#include "gpu940i.h"

/*
 * Data Definitions
 */

#define CACHE_SIZE 8
static struct {
	int32_t x,y;
	int32_t clipped;
} c2d_cache[CACHE_SIZE];
static unsigned cache_end = 0;
static int cache_depth = 0;
static unsigned cache_hit = 0;
static unsigned cache_miss = 0;

/*
 * Private Functions
 */

static bool have_user_clipPlanes(void)
{
	return ctx.view.nb_clipPlanes > 5;
}

// prev and next must have h <0 and >0 (or vice versa)
static unsigned new_vec(unsigned prev, unsigned next, unsigned p)
{
	static gpuCmdVector cmdVectors[MAX_FACET_SIZE+2*GPU_NB_CLIPPLANES];	// used as a place to store cmds that are not in cmdbuf
	int32_t ha = Fix_abs(ctx.points.vectors[prev].h);
	int32_t hb = Fix_abs(ctx.points.vectors[next].h);
	int32_t ratio = 1<<16, hahb = ha+hb;
	if (hahb) ratio = ((int64_t)ha<<16)/(ha+hb);
	assert(ctx.points.nb_vectors < sizeof_array(ctx.points.vectors));
	ctx.points.vectors[ctx.points.nb_vectors].cmd = cmdVectors + ctx.points.nb_vectors;
	for (unsigned u=ctx.poly.nb_params+3; u--; ) {
		ctx.points.vectors[ctx.points.nb_vectors].cmd->u.all_params[u] =
			ctx.points.vectors[prev].cmd->u.all_params[u] +
			(((int64_t)ratio*(ctx.points.vectors[next].cmd->u.all_params[u]-ctx.points.vectors[prev].cmd->u.all_params[u]))>>16);
	}
	ctx.points.vectors[ctx.points.nb_vectors].prev = prev;
	ctx.points.vectors[ctx.points.nb_vectors].next = next;
	ctx.points.vectors[prev].next = ctx.points.vectors[next].prev = ctx.points.nb_vectors;
	ctx.points.vectors[ctx.points.nb_vectors].clipFlag = (ctx.points.vectors[prev].clipFlag&ctx.points.vectors[next].clipFlag) | (1<<p);
	ctx.points.vectors[ctx.points.nb_vectors].proj = 0;
	return ctx.points.nb_vectors++;
}

static void compute_h(unsigned v, gpuPlane const *const plane)
{
	ctx.points.vectors[v].h = 0;
	for (unsigned c=3; c--; ) {
		if (0 == plane->normal[c]) continue;
		ctx.points.vectors[v].h += Fix_mul(plane->normal[c], ctx.points.vectors[v].cmd->u.geom.c3d[c] - plane->origin[c]);
	}
	if (0 == ctx.points.vectors[v].h) ctx.points.vectors[v].h = 1;
}

static int clip_facet_by_plane(unsigned p)
{
	gpuPlane const *const plane = ctx.view.clipPlanes+p;
	// for each vector, compute H
	unsigned last_in = ~0U;
	unsigned new_v[2], nb_new_v = 0;
	unsigned v = ctx.points.first_vector;
	do {
		compute_h(v, plane);
		v = ctx.points.vectors[v].next;
	} while (v != ctx.points.first_vector);
	do {
		unsigned next = ctx.points.vectors[v].next;
		if (! Fix_same_sign(ctx.points.vectors[v].h, ctx.points.vectors[next].h)) {
			new_v[nb_new_v++] = new_vec(v, next, p);
		}
		if (ctx.points.vectors[v].h > 0) {
			last_in = v;
		}
		v = next;
	} while (nb_new_v < 2 && v != ctx.points.first_vector);
	if (0 == nb_new_v) {
		return ~0U != last_in;
	}
	assert(~0U != last_in && nb_new_v == 2);
	if (ctx.points.vectors[ctx.points.first_vector].h > 0) {
		ctx.points.vectors[new_v[0]].next = new_v[1];
		ctx.points.vectors[new_v[1]].prev = new_v[0];
	} else {
		ctx.points.vectors[new_v[0]].prev = new_v[1];
		ctx.points.vectors[new_v[1]].next = new_v[0];
		ctx.points.first_vector = last_in;
	}
	return 1;
}

static int clip_point_by_plane(unsigned p)
{
	gpuPlane const *const plane = ctx.view.clipPlanes+p;
	compute_h(0, plane);
	return ctx.points.vectors[0].h >= 0;
}

static void next_cache(void)
{
	if (++ cache_end >= CACHE_SIZE) cache_end = 0;
	if (cache_depth < CACHE_SIZE-1) cache_depth++;
}

static void proj_given(unsigned v)
{
	int32_t const x = ctx.points.vectors[v].cmd->u.geom.c3d[0];
	int32_t const y = ctx.points.vectors[v].cmd->u.geom.c3d[1];
	int32_t const z = ctx.points.vectors[v].cmd->u.geom.c3d[2];
	ctx.points.vectors[v].clipped = 1;
	if (z < ctx.view.clipPlanes[0].origin[2]) {
		int32_t const dproj = ctx.view.dproj;
		int32_t inv_z = Fix_inv(-z);
		ctx.points.vectors[v].c2d[0] = Fix_mul(x<<dproj, inv_z) + (ctx.view.winWidth<<15);
		if ((uint32_t)ctx.points.vectors[v].c2d[0] < (uint32_t)ctx.view.winWidth<<16) {
			ctx.points.vectors[v].c2d[1] = Fix_mul(y<<dproj, inv_z) + (ctx.view.winHeight<<15);
			if ((uint32_t)ctx.points.vectors[v].c2d[1] < (uint32_t)ctx.view.winHeight<<16) {
				ctx.points.vectors[v].clipped = 0;
				ctx.points.vectors[v].proj = 1;
			}
		}
	}
}

static void proj_cached(unsigned v)
{
	assert(v < ctx.poly.cmd->size);
	int const same_as = ctx.points.vectors[v].cmd->same_as - v;
	if (same_as > 0 && same_as <= cache_depth) {
		cache_hit ++;
		int cache_idx = cache_end - same_as;
		if (cache_idx < 0) cache_idx += CACHE_SIZE;
		ctx.points.vectors[v].c2d[0] = c2d_cache[cache_idx].x;
		ctx.points.vectors[v].c2d[1] = c2d_cache[cache_idx].y;
		ctx.points.vectors[v].clipped = c2d_cache[cache_idx].clipped;
		ctx.points.vectors[v].proj = 1;
	} else {
		cache_miss ++;
		ctx.points.vectors[v].proj = 0;
	}
}

static void proj_new_vec(unsigned v)
{
	int32_t const x = ctx.points.vectors[v].cmd->u.geom.c3d[0];
	int32_t const y = ctx.points.vectors[v].cmd->u.geom.c3d[1];
	int32_t const z = ctx.points.vectors[v].cmd->u.geom.c3d[2];
	int32_t const dproj = ctx.view.dproj;
	int32_t c2d;
	int32_t inv_z = Fix_inv(-z);
	switch (ctx.points.vectors[v].clipFlag & 0xa) {
		case 0x2:	// right
			c2d = ctx.view.clipMax[0]<<16;
			break;
		case 0x8:	// left
			c2d = ctx.view.clipMin[0]<<16;
			break;
		default:
			c2d = Fix_mul(x<<dproj, inv_z);
			break;
	}
	ctx.points.vectors[v].c2d[0] = c2d + (ctx.view.winWidth<<15);
	switch (ctx.points.vectors[v].clipFlag & 0x14) {
		case 0x4:	// up
			c2d = ctx.view.clipMax[1]<<16;
			break;
		case 0x10:	// bottom
			c2d = ctx.view.clipMin[1]<<16;
			break;
		default:
			c2d = Fix_mul(y<<dproj, inv_z);
			break;
	}
	ctx.points.vectors[v].c2d[1] = c2d + (ctx.view.winHeight<<15);
}

/*
 * Public Functions
 */

// When we arrive here, only cmdVectors and cmdFacets are set.
// We must first init other members of facet and vectors, then clip, updating facet size as we add/remove vectors.
// return true if something is left to be displayed.
int clip_poly(void)
{
	int disp = 0;
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_CLIP, "clip & proj");
	// init facet
	unsigned new_size = ctx.poly.cmd->size;
	ctx.poly.nb_params = 0;
	switch (ctx.poly.cmd->rendering_type) {
		case rendering_flat:
			break;
		case rendering_text:
			ctx.poly.nb_params = 2;
			break;
		case rendering_smooth:
			ctx.poly.nb_params = 3;
			break;
		default:
			set_error_flag(gpuEPARAM);
			goto ret;
	}
#	ifdef GP2X
	int i_param = -1;
#	endif
	if (ctx.poly.cmd->use_intens) {
#		ifdef GP2X
		i_param = ctx.poly.nb_params;
#		endif
		ctx.poly.nb_params++;
	}
	if (ctx.rendering.z_mode != gpu_z_off) ctx.poly.nb_params++;
	// init vectors
	unsigned v;
	unsigned clipped = 0;
	ctx.points.first_vector = 0;
	ctx.points.nb_vectors = ctx.poly.cmd->size;
	for (v=0; v<ctx.points.nb_vectors; v++) {
		ctx.points.vectors[v].next = v+1;
		ctx.points.vectors[v].prev = v-1;
		ctx.points.vectors[v].clipFlag = 0;
		proj_cached(v);
		if (! have_user_clipPlanes()) {	// no user clip planes
			if (! ctx.points.vectors[v].proj) proj_given(v);
			clipped |= ctx.points.vectors[v].clipped;
		}
#		ifdef GP2X
		if (i_param != -1) {	// use this scan to premult i param for YUV illumination
			ctx.points.vectors[v].cmd->u.geom.param[i_param] *= 55;
		}
#		endif
	}
	ctx.points.vectors[0].prev = v-1;
	ctx.points.vectors[v-1].next = 0;
	if (!have_user_clipPlanes() && !clipped) {
		disp = 1;
		goto ret;
	}
	for (v=0; v<ctx.view.nb_clipPlanes; v++) {
		if (! clip_facet_by_plane(v)) goto ret;
	}
	// compute new size and project new vertexes
	new_size = 0;
	v = ctx.points.first_vector;
	do {
		new_size ++;
		if (! ctx.points.vectors[v].proj) {
			proj_new_vec(v);
		}
		v = ctx.points.vectors[v].next;
	} while (v != ctx.points.first_vector);
	disp = 1;
ret:
	for (v=0; v<ctx.poly.cmd->size; v++) {
		// store it in cache
		c2d_cache[cache_end].x = ctx.points.vectors[v].c2d[0];
		c2d_cache[cache_end].y = ctx.points.vectors[v].c2d[1];
		c2d_cache[cache_end].clipped = ctx.points.vectors[v].clipped;
		next_cache();
	}
	ctx.poly.cmd->size = new_size;
	perftime_enter(previous_target, NULL);
	return disp;
}

int clip_point(void)
{
	int disp = 0;
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_CLIP, "clip & proj");
	// init vectors
	ctx.points.vectors[0].clipFlag = 0;
	if (! have_user_clipPlanes()) {	// no user clip planes
		proj_given(0);
		disp = !ctx.points.vectors[0].clipped;
		goto ret;
	}
	for (unsigned p=5; p<ctx.view.nb_clipPlanes; p++) {
		if (! clip_point_by_plane(p)) {
			disp = 0;
			goto ret;
		}
	}
	disp = 1;
ret:
	perftime_enter(previous_target, NULL);
	return disp;
}

// returns true if something is left to draw
int cull_poly(void)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_CULL, "culling");
	int ret = 1;
	if (ctx.poly.cmd->cull_mode == 3) {
		ret = 0;
		goto cull_end;
	}
	if (ctx.poly.cmd->cull_mode == 0) {
		goto cull_end;
	}
	unsigned v = ctx.points.first_vector;
	unsigned nb_v = 0;
	int32_t a = 0;
	do {
		unsigned v_next = nb_v < 2 ? ctx.points.vectors[v].next : ctx.points.first_vector;
		a += Fix_mul(ctx.points.vectors[v].c2d[0]>>1, ctx.points.vectors[v_next].c2d[1]>>1);	// FIXME: 2 may not be enought for greater screen size
		a -= Fix_mul(ctx.points.vectors[v_next].c2d[0]>>1, ctx.points.vectors[v].c2d[1]>>1);
		v = v_next;
		nb_v ++;
	} while (nb_v < 3);	
	if (a == 0) {
		goto cull_end;
	}
	ret = (a > 0 && ctx.poly.cmd->cull_mode == 2) || (a < 0 && ctx.poly.cmd->cull_mode == 1);
cull_end:
	perftime_enter(previous_target, NULL);
	return ret;
}

unsigned proj_cache_ratio(void)
{
	if (cache_hit > (UINT_MAX>>10) || cache_miss > (UINT_MAX>>10)) {
		cache_hit >>= 1;
		cache_miss >>= 1;
	}
	unsigned tot = cache_hit+cache_miss;
	if (!tot) return 0;
	return (100*cache_hit)/tot;
}

void proj_cache_reset(void)
{
	cache_end = 0;
	cache_depth = 0;
	cache_hit = 0;
	cache_miss = 0;
}
