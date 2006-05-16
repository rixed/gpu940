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
#include "gpu940i.h"

// TODO : a c2d cache ?

/*
 * Private Functions
 */

// prev and next must have h <0 and >0 (or vice versa)
static unsigned new_vec(unsigned prev, unsigned next, unsigned p) {
	int32_t ha = Fix_abs(ctx.poly.vectors[prev].h);
	int32_t hb = Fix_abs(ctx.poly.vectors[next].h);
	int32_t ratio = ((int64_t)ha<<16)/(ha+hb);
	assert(ctx.poly.nb_vectors < sizeof_array(ctx.poly.vectors));
	for (unsigned u=ctx.poly.nb_params+3; u--; ) {
		ctx.poly.vectors[ctx.poly.nb_vectors].cmdVector.all_params[u] =
			ctx.poly.vectors[prev].cmdVector.all_params[u] +
			(((int64_t)ratio*(ctx.poly.vectors[next].cmdVector.all_params[u]-ctx.poly.vectors[prev].cmdVector.all_params[u]))>>16);
	}
	ctx.poly.vectors[ctx.poly.nb_vectors].prev = prev;
	ctx.poly.vectors[ctx.poly.nb_vectors].next = next;
	ctx.poly.vectors[prev].next = ctx.poly.vectors[next].prev = ctx.poly.nb_vectors;
	ctx.poly.vectors[ctx.poly.nb_vectors].clipFlag = (ctx.poly.vectors[prev].clipFlag&ctx.poly.vectors[next].clipFlag) | (1<<p);
	return ctx.poly.nb_vectors++;
}

// return true if something is left
static int clip_facet_by_plane(unsigned p) {
	// for each vector, compute H
	unsigned v = ctx.poly.first_vector;
	unsigned last_in = ~0U;
	unsigned new_v[2], nb_new_v = 0;
	do {
		ctx.poly.vectors[v].h = 0;
		for (unsigned c=3; c--; ) {
			int32_t const ov = ctx.poly.vectors[v].cmdVector.geom.c3d[c] - ctx.view.clipPlanes[p].origin[c];
			ctx.poly.vectors[v].h += ((int64_t)ov*ctx.view.clipPlanes[p].normal[c])>>8;	// we suppose normal is approx 8 bits wide
		}
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

static void proj_vec(unsigned v) {
	int32_t const x = ctx.poly.vectors[v].cmdVector.geom.c3d[0];
	int32_t const y = ctx.poly.vectors[v].cmdVector.geom.c3d[1];
	int32_t const z = ctx.poly.vectors[v].cmdVector.geom.c3d[2];
	int32_t const dproj = ctx.view.dproj;
	int32_t c2d;
	switch (ctx.poly.vectors[v].clipFlag & 0xa) {
		case 0x2:	// right
			c2d = ctx.view.clipMax[0];
			break;
		case 0x8:	// left
			c2d = ctx.view.clipMin[0];
			break;
		default:
			c2d = ((int64_t)x<<dproj)/-z;
			break;
	}
	ctx.poly.vectors[v].c2d[0] = c2d + ctx.view.winWidth/2;
	switch (ctx.poly.vectors[v].clipFlag & 0x14) {
		case 0x4:	// up
			c2d = ctx.view.clipMax[1];
			break;
		case 0x10:	// bottom
			c2d = ctx.view.clipMin[1];
			break;
		default:
			c2d = ((int64_t)y<<dproj)/-z;
			break;
	}
	ctx.poly.vectors[v].c2d[1] = c2d + ctx.view.winHeight/2;
}

/*
 * Public Functions
 */

// When we arrive here, only cmdVectors and cmdFacets are set.
// We must first init other members of facet and vectors, then clip, updating facet size as we add/remove vectors.
// return true if something is left to be displayed.
int clip_poly(void) {
	// init facet
	switch (ctx.poly.cmdFacet.rendering_type) {
		case rendering_c:
			ctx.poly.nb_params = 0;
			break;
		case rendering_ci:
			ctx.poly.nb_params = 1;
			break;
		case rendering_uv:
			ctx.poly.nb_params = 2;
			break;
		case rendering_uvi:
			ctx.poly.nb_params = 3;
			break;
		case rendering_rgb:
			ctx.poly.nb_params = 3;
			break;
		case rendering_rgbi:
			ctx.poly.nb_params = 4;
			break;
		default:
			set_error_flag(gpuEPARAM);
			return 0;
	}
	// init vectors
	unsigned v;
	ctx.poly.first_vector = 0;
	ctx.poly.nb_vectors = ctx.poly.cmdFacet.size;
	for (v=0; v<ctx.poly.nb_vectors; v++) {
		ctx.poly.vectors[v].next = v+1;
		ctx.poly.vectors[v].prev = v-1;
		ctx.poly.vectors[v].clipFlag = 0;
	}
	ctx.poly.vectors[0].prev = v-1;
	ctx.poly.vectors[v-1].next = 0;
	for (v=0; v<ctx.view.nb_clipPlanes; v++) {
		if (! clip_facet_by_plane(v)) return 0;
	}
	ctx.poly.cmdFacet.size = 0;
	v = ctx.poly.first_vector;
	do {
		proj_vec(v);
		ctx.poly.cmdFacet.size ++;
		v = ctx.poly.vectors[v].next;
	} while (v != ctx.poly.first_vector);
	return 1;
}

