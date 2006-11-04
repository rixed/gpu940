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
} c2d_cache[CACHE_SIZE];
static unsigned cache_end = 0;
static unsigned cache_depth = 0;
static unsigned cache_hit = 0;
static unsigned cache_miss = 0;

/*
 * Private Functions
 */

// prev and next must have h <0 and >0 (or vice versa)
static unsigned new_vec(unsigned prev, unsigned next, unsigned p)
{
	static gpuCmdVector cmdVectors[MAX_FACET_SIZE+2*GPU_NB_CLIPPLANES];	// used as a place to store cmds that are not in cmdbuf
	int32_t ha = Fix_abs(ctx.poly.vectors[prev].h);
	int32_t hb = Fix_abs(ctx.poly.vectors[next].h);
	int32_t ratio = 1<<16, hahb = ha+hb;
	if (hahb) ratio = ((int64_t)ha<<16)/(ha+hb);
	assert(ctx.poly.nb_vectors < sizeof_array(ctx.poly.vectors));
	ctx.poly.vectors[ctx.poly.nb_vectors].cmd = cmdVectors + ctx.poly.nb_vectors;
	for (unsigned u=ctx.poly.nb_params+3; u--; ) {
		ctx.poly.vectors[ctx.poly.nb_vectors].cmd->u.all_params[u] =
			ctx.poly.vectors[prev].cmd->u.all_params[u] +
			(((int64_t)ratio*(ctx.poly.vectors[next].cmd->u.all_params[u]-ctx.poly.vectors[prev].cmd->u.all_params[u]))>>16);
	}
	ctx.poly.vectors[ctx.poly.nb_vectors].prev = prev;
	ctx.poly.vectors[ctx.poly.nb_vectors].next = next;
	ctx.poly.vectors[prev].next = ctx.poly.vectors[next].prev = ctx.poly.nb_vectors;
	ctx.poly.vectors[ctx.poly.nb_vectors].clipFlag = (ctx.poly.vectors[prev].clipFlag&ctx.poly.vectors[next].clipFlag) | (1<<p);
	return ctx.poly.nb_vectors++;
}

// return true if something is left
static int clip_facet_by_plane(unsigned p)
{
	gpuPlane const *const plane = ctx.view.clipPlanes+p;
	// for each vector, compute H
	unsigned last_in = ~0U;
	unsigned new_v[2], nb_new_v = 0;
	unsigned v = ctx.poly.first_vector;
	// We spend most of our time in the following loop, computing h.
	// In fact we barely need h but it's sign.
	do {
		ctx.poly.vectors[v].h = 0;
		for (unsigned c=3; c--; ) {
			if (0 == plane->normal[c]) continue;	// frequent case
			int32_t const ov = ctx.poly.vectors[v].cmd->u.geom.c3d[c] - plane->origin[c];
			ctx.poly.vectors[v].h += Fix_mul(ov, plane->normal[c]);
		}
		if (0 == ctx.poly.vectors[v].h) ctx.poly.vectors[v].h = 1;
		v = ctx.poly.vectors[v].next;
	} while (v != ctx.poly.first_vector);
	do {
		unsigned next = ctx.poly.vectors[v].next;
		if (! Fix_same_sign(ctx.poly.vectors[v].h, ctx.poly.vectors[next].h)) {
			new_v[nb_new_v++] = new_vec(v, next, p);
		}
		if (ctx.poly.vectors[v].h > 0) {
			last_in = v;
		}
		v = next;
	} while (nb_new_v < 2 && v != ctx.poly.first_vector);
	if (0 == nb_new_v) {
		return ~0U != last_in;
	}
	assert(~0U != last_in && nb_new_v == 2);
	if (ctx.poly.vectors[ctx.poly.first_vector].h > 0) {
		ctx.poly.vectors[new_v[0]].next = new_v[1];
		ctx.poly.vectors[new_v[1]].prev = new_v[0];
	} else {
		ctx.poly.vectors[new_v[0]].prev = new_v[1];
		ctx.poly.vectors[new_v[1]].next = new_v[0];
		ctx.poly.first_vector = last_in;
	}
	return 1;
}

static void next_cache(void)
{
	if (++ cache_end >= CACHE_SIZE) cache_end = 0;
	if (cache_depth < CACHE_SIZE-1) cache_depth++;
}

static void proj_vec(unsigned v)
{
	// Do we have this vertex in cache ?
	unsigned const same_as = ctx.poly.vectors[v].cmd->same_as;
	if (v < ctx.poly.cmd->size && same_as > 0 && same_as <= cache_depth) {
		cache_hit ++;
		int cache_idx = cache_end - same_as;
		if (cache_idx < 0) cache_idx += CACHE_SIZE;
		ctx.poly.vectors[v].c2d[0] = c2d_cache[cache_idx].x;
		ctx.poly.vectors[v].c2d[1] = c2d_cache[cache_idx].y;
	} else {
		cache_miss ++;
		int32_t const x = ctx.poly.vectors[v].cmd->u.geom.c3d[0];
		int32_t const y = ctx.poly.vectors[v].cmd->u.geom.c3d[1];
		int32_t const z = ctx.poly.vectors[v].cmd->u.geom.c3d[2];
		int32_t const dproj = ctx.view.dproj;
		int32_t c2d;
		int32_t inv_z = Fix_inv(-z);
		switch (ctx.poly.vectors[v].clipFlag & 0xa) {
			case 0x2:	// right
				c2d = ctx.view.clipMax[0]<<16;
				break;
			case 0x8:	// left
				c2d = ctx.view.clipMin[0]<<16;
				break;
			default:
				c2d = Fix_mul(x<<dproj, inv_z);//((int64_t)x<<(16+dproj))/-z;
				break;
		}
		ctx.poly.vectors[v].c2d[0] = c2d + (ctx.view.winWidth<<15);
		switch (ctx.poly.vectors[v].clipFlag & 0x14) {
			case 0x4:	// up
				c2d = ctx.view.clipMax[1]<<16;
				break;
			case 0x10:	// bottom
				c2d = ctx.view.clipMin[1]<<16;
				break;
			default:
				c2d = Fix_mul(y<<dproj, inv_z);//((int64_t)y<<(16+dproj))/-z;
				break;
		}
		ctx.poly.vectors[v].c2d[1] = c2d + (ctx.view.winHeight<<15);
	}
	if (v < ctx.poly.cmd->size) {
		// store it in cache
		c2d_cache[cache_end].x = ctx.poly.vectors[v].c2d[0];
		c2d_cache[cache_end].y = ctx.poly.vectors[v].c2d[1];
		next_cache();
	}
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
	if (ctx.poly.cmd->rendering_type >= GPU_NB_RENDERING_TYPES) {
		set_error_flag(gpuEPARAM);
		goto ret;
	}
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
	ctx.poly.first_vector = 0;
	ctx.poly.nb_vectors = ctx.poly.cmd->size;
	for (v=0; v<ctx.poly.nb_vectors; v++) {
		ctx.poly.vectors[v].next = v+1;
		ctx.poly.vectors[v].prev = v-1;
		ctx.poly.vectors[v].clipFlag = 0;
	}
	ctx.poly.vectors[0].prev = v-1;
	ctx.poly.vectors[v-1].next = 0;
	for (v=0; v<ctx.view.nb_clipPlanes; v++) {
		if (! clip_facet_by_plane(v)) goto ret;
	}
	// compute new size and tag used vectors so that we can then loop over vectors array
	v = ctx.poly.first_vector;
	unsigned new_size = 0;
	static int32_t magick = 0;
	do {
		new_size ++;
		ctx.poly.vectors[v].used = magick;
		v = ctx.poly.vectors[v].next;
	} while (v != ctx.poly.first_vector);
	// We must enter vectors in incoming order into the cache (same_as significance)
	v = 0;
	for (unsigned nb_v = 0; nb_v < new_size; v++) {
		if (ctx.poly.vectors[v].used == magick) {
			proj_vec(v);
			nb_v ++;
#			ifdef GP2X
			if (i_param != -1) {	// use this scan to premult i param for YUV illumination
				ctx.poly.vectors[v].cmd->u.geom.param[i_param] *= 55;
			}
#			endif
		} else if (v < ctx.poly.cmd->size) {
			next_cache();	// skip this entry (leave it blank: we won't use it)
		}
	}
	ctx.poly.cmd->size = new_size;
	magick ++;
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
	unsigned v = ctx.poly.first_vector;
	unsigned nb_v = 0;
	int32_t a = 0;
	do {
		unsigned v_next = nb_v < 2 ? ctx.poly.vectors[v].next : ctx.poly.first_vector;
		a += Fix_mul(ctx.poly.vectors[v].c2d[0]>>1, ctx.poly.vectors[v_next].c2d[1]>>1);	// FIXME: 2 may not be enought for greater screen size
		a -= Fix_mul(ctx.poly.vectors[v_next].c2d[0]>>1, ctx.poly.vectors[v].c2d[1]>>1);
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
	return (100*cache_hit)/(cache_hit+cache_miss);
}

void proj_cache_reset(void)
{
	cache_end = 0;
	cache_depth = 0;
	cache_hit = 0;
	cache_miss = 0;
}
