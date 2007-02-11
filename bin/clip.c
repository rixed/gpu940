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
static gpuVector *new_vec(gpuVector *prev, gpuVector *next, unsigned p)
{
	static gpuCmdVector cmdVectors[MAX_FACET_SIZE+2*GPU_NB_CLIPPLANES];	// used as a place to store cmds that are not in cmdbuf
	int32_t ha = Fix_abs(prev->h);
	int32_t hb = Fix_abs(next->h);
	int32_t ratio = 1<<16, hahb = ha+hb;
	if (hahb) ratio = ((int64_t)ha<<16)/(ha+hb);
	assert(ctx.points.nb_vectors < sizeof_array(ctx.points.vectors));
	gpuVector *new = ctx.points.vectors + ctx.points.nb_vectors;
	new->cmd = cmdVectors + ctx.points.nb_vectors;
	for (unsigned u=sizeof_array(new->cmd->u.all_params); u--; ) {
		new->cmd->u.all_params[u] =
			prev->cmd->u.all_params[u] +
			Fix_mul(ratio, (next->cmd->u.all_params[u] - prev->cmd->u.all_params[u]));
	}
	new->prev = prev;
	new->next = next;
	prev->next = next->prev = new;
	new->clipFlag = (prev->clipFlag & next->clipFlag) | (1<<p);
	new->proj = 0;
	ctx.points.nb_vectors++;
	return new;
}

static void compute_h(gpuVector *v, gpuPlane const *const plane)
{
	v->h = 0;
	for (unsigned c=3; c--; ) {
		if (0 == plane->normal[c]) continue;
		v->h += Fix_mul(plane->normal[c], v->cmd->u.geom.c3d[c] - plane->origin[c]);
	}
	if (0 == v->h) v->h = 1;
}

static int clip_facet_by_plane(unsigned p)
{
	gpuPlane const *const plane = ctx.view.clipPlanes+p;
	// for each vector, compute H
	gpuVector *last_in = NULL;
	gpuVector *new_v[2];
	unsigned nb_new_v = 0;
	gpuVector *v = ctx.points.first_vector;
	do {
		compute_h(v, plane);
		v = v->next;
	} while (v != ctx.points.first_vector);
	do {
		gpuVector *next = v->next;
		if (! Fix_same_sign(v->h, next->h)) {
			new_v[nb_new_v++] = new_vec(v, next, p);
		}
		if (v->h > 0) {
			last_in = v;
		}
		v = next;
	} while (nb_new_v < 2 && v != ctx.points.first_vector);
	if (0 == nb_new_v) {
		return NULL != last_in;
	}
	assert(NULL != last_in && nb_new_v == 2);
	if (ctx.points.first_vector->h > 0) {
		new_v[0]->next = new_v[1];
		new_v[1]->prev = new_v[0];
	} else {
		new_v[0]->prev = new_v[1];
		new_v[1]->next = new_v[0];
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
	if (z > ctx.view.clipPlanes[0].origin[2]) {
		int32_t const dproj = ctx.view.dproj;
		int32_t inv_z = Fix_inv(z);
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

static void proj_new_vec(gpuVector *v)
{
	int32_t const x = v->cmd->u.geom.c3d[0];
	int32_t const y = v->cmd->u.geom.c3d[1];
	int32_t const z = v->cmd->u.geom.c3d[2];
	int32_t const dproj = ctx.view.dproj;
	int32_t c2d;
	int32_t inv_z = Fix_inv(z);
	switch (v->clipFlag & 0xa) {
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
	v->c2d[0] = c2d + (ctx.view.winWidth<<15);
	switch (v->clipFlag & 0x14) {
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
	v->c2d[1] = c2d + (ctx.view.winHeight<<15);
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
	// init vectors
	unsigned v;
	unsigned clipped = 0;
	ctx.points.first_vector = ctx.points.vectors + 0;
	ctx.points.nb_vectors = ctx.poly.cmd->size;
	for (v=0; v<ctx.points.nb_vectors; v++) {
		ctx.points.vectors[v].next = &ctx.points.vectors[v+1];
		ctx.points.vectors[v].prev = &ctx.points.vectors[v-1];
		ctx.points.vectors[v].clipFlag = 0;
		proj_cached(v);
		if (! have_user_clipPlanes()) {	// no user clip planes
			if (! ctx.points.vectors[v].proj) proj_given(v);
			clipped |= ctx.points.vectors[v].clipped;
		}
#		ifdef GP2X
		if (ctx.poly.cmd->use_intens) {
			ctx.points.vectors[v].cmd->u.text.i *= 55;
		}
#		endif
	}
	ctx.points.vectors[0].prev = &ctx.points.vectors[v-1];
	ctx.points.vectors[v-1].next = &ctx.points.vectors[0];
	if (!have_user_clipPlanes() && !clipped) {
		disp = 1;
		goto ret;
	}
	for (v=0; v<ctx.view.nb_clipPlanes; v++) {
		if (! clip_facet_by_plane(v)) goto ret;
	}
	// compute new size and project new vertexes
	new_size = 0;
	gpuVector *vp = ctx.points.first_vector;
	do {
		new_size ++;
		if (! vp->proj) {
			proj_new_vec(vp);
		}
		vp = vp->next;
	} while (vp != ctx.points.first_vector);
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
	gpuVector *v = ctx.points.first_vector;
	unsigned nb_v = 0;
	int32_t a = 0;
	do {
		gpuVector *v_next = nb_v < 2 ? v->next : ctx.points.first_vector;
		a += Fix_mul(v->c2d[0]>>1, v_next->c2d[1]>>1);	// FIXME: 2 may not be enought for greater screen size
		a -= Fix_mul(v_next->c2d[0]>>1, v->c2d[1]>>1);
		v = v_next;
		nb_v ++;
	} while (nb_v < 3);	
	if (a == 0) {
		goto cull_end;
	}
	ret = (a > 0 && ctx.poly.cmd->cull_mode == GPU_CW) || (a < 0 && ctx.poly.cmd->cull_mode == GPU_CCW);
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
