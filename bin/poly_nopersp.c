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

/*
 * Data Definitions
 */

#define SWAP(type,a,b) do { \
	type dummy = a; \
	a = b; \
	b = dummy; \
} while(0)

/*
 * Private Functions
 */

// buffers are so lower coords have lower addresses. direct coord system was just a convention, right ?
static void draw_line(void)
{
	int32_t const c_start = ctx.trap.side[ctx.trap.left_side].c >> 16;
	ctx.line.count = ((ctx.trap.side[!ctx.trap.left_side].c + 0xffff) >> 16) - c_start;
	if (unlikely(ctx.line.count <= 0)) return;	// may happen on some pathological cases ?
	ctx.line.w = ctx.location.out_start + c_start + ((ctx.poly.nc_declived>>16)<<ctx.location.buffer_loc[gpuOutBuffer].width_log);
	if (unlikely(! ctx.trap.is_triangle) && likely(ctx.poly.nb_params > 0)) {
		int32_t const inv_dc = Fix_uinv(ctx.line.count<<16);
		for (unsigned p=ctx.poly.nb_params; p--; ) {
			ctx.line.dparam[p] = Fix_mul(ctx.trap.side[!ctx.trap.left_side].param[p] - ctx.line.param[p], inv_dc);
		}
	}
	unsigned const previous_target = perftime_target();
	perftime_enter(PERF_POLY_DRAW, "raster");
#ifdef GP2X
	jit_exec();
#else
	raster_gen();
#endif
	perftime_enter(previous_target, NULL);
}

static void next_params_frac(unsigned side, int32_t dnc)
{
	ctx.trap.side[side].c += Fix_mul(dnc, ctx.trap.side[side].dc);
	for (unsigned p=ctx.poly.nb_params; p--; ) {
		ctx.trap.side[side].param[p] += Fix_mul(dnc, ctx.trap.side[side].param_alpha[p]);
	}
}

static void draw_trapeze_frac(void)
{
	int32_t dnc = ctx.poly.nc_declived_next - ctx.poly.nc_declived;
	dnc = Fix_abs(dnc);
	// now compute some next c and params
	if (ctx.trap.side[0].is_growing) next_params_frac(0, dnc);
	if (ctx.trap.side[1].is_growing) next_params_frac(1, dnc);
	// draw 'scanline'
	draw_line();	// will do another DIV
	// compute others next c and params
	if (! ctx.trap.side[0].is_growing) next_params_frac(0, dnc);
	if (! ctx.trap.side[1].is_growing) next_params_frac(1, dnc);
	ctx.poly.nc_declived = ctx.poly.nc_declived_next;
}

static void next_params_int(unsigned side)
{
	ctx.trap.side[side].c += ctx.trap.side[side].dc;
	ctx.trap.side[side].param[0] += ctx.trap.side[side].param_alpha[0];
	ctx.trap.side[side].param[1] += ctx.trap.side[side].param_alpha[1];
	ctx.trap.side[side].param[2] += ctx.trap.side[side].param_alpha[2];
	ctx.trap.side[side].param[3] += ctx.trap.side[side].param_alpha[3];
}

static void draw_trapeze_int(void)
{
	// now compute some next c and params
	if (ctx.trap.side[0].is_growing) next_params_int(0);
	if (ctx.trap.side[1].is_growing) next_params_int(1);
	// draw 'scanline'
	draw_line();	// will do another DIV
	// now compute others c and params
	if (! ctx.trap.side[0].is_growing) next_params_int(0);
	if (! ctx.trap.side[1].is_growing) next_params_int(1);
	ctx.poly.nc_declived = ctx.poly.nc_declived_next;
}

static void draw_trapeze(void)
{
	// we have ctx.trap.side[], nc_declived, all params.
	// we must render until first ending side (and return with nc_declived = this ending nc coord)
	ctx.trap.left_side = 0;
	if (
		(ctx.trap.side[1].c < ctx.trap.side[0].c) ||
		(ctx.trap.side[1].c == ctx.trap.side[0].c && ctx.trap.side[1].dc < ctx.trap.side[0].dc)
	) {
		ctx.trap.left_side = 1;
	}
	ctx.line.param = ctx.trap.side[ ctx.trap.left_side ].param;
	ctx.trap.side[ctx.trap.left_side].is_growing = ctx.trap.side[ctx.trap.left_side].dc < 0;
	ctx.trap.side[!ctx.trap.left_side].is_growing = ctx.trap.side[!ctx.trap.left_side].dc > 0;
	// Compute constant linear coefs for non-perspective mode if possible
	// We can use the same dparam for all the trapeze if the trapeze is in fact a triangle
	ctx.trap.is_triangle = ctx.trap.side[0].start_v == ctx.trap.side[1].start_v ||
		ctx.trap.side[0].end_v == ctx.trap.side[1].end_v;
	if (ctx.trap.is_triangle) {
		int32_t ddc = ctx.trap.side[0].dc - ctx.trap.side[1].dc;
		int32_t dc0 = ctx.trap.side[0].c - ctx.trap.side[1].c;
		if (Fix_abs(ddc) > Fix_abs(dc0)) {
			ddc = Fix_inv(ddc);
			for (unsigned p=ctx.poly.nb_params; p--; ) {
				ctx.line.dparam[p] = Fix_mul(ddc, ctx.trap.side[0].param_alpha[p] - ctx.trap.side[1].param_alpha[p]);
			}
		} else {
			if (dc0 != 0) {
				dc0 = Fix_inv(dc0);
				for (unsigned p=ctx.poly.nb_params; p--; ) {
					ctx.line.dparam[p] = Fix_mul(dc0, ctx.trap.side[0].param[p] - ctx.trap.side[1].param[p]);
				}
			} else {
				for (unsigned p=ctx.poly.nb_params; p--; ) {
					ctx.line.dparam[p] = 0;
				}
			}
		}
	}
	// First, we draw from nc_declived to next scanline boundary or end of trapeze
	if (ctx.poly.nc_declived & 0xffff) {
		bool quit = false;
		ctx.poly.nc_declived_next = ctx.poly.nc_declived & 0xffff0000;
		ctx.poly.nc_declived_next += 1<<16;
		// maybe we reached end of trapeze already ?
		for (unsigned side=2; side--; ) {
			if (ctx.poly.vectors[ ctx.trap.side[side].end_v ].c2d[1] <= ctx.poly.nc_declived_next) {
				ctx.poly.nc_declived_next = ctx.poly.vectors[ ctx.trap.side[side].end_v ].c2d[1];
				quit = true;
			}
		}
		draw_trapeze_frac();
		if (quit) return;
	}
	// Now draw integral scanlines
	int32_t d_nc_declived, last_nc_declived_i;
	int32_t last_nc_declived = ctx.poly.vectors[ ctx.trap.side[0].end_v ].c2d[1];
	if (ctx.poly.vectors[ ctx.trap.side[1].end_v ].c2d[1] <= last_nc_declived) {
		last_nc_declived = ctx.poly.vectors[ ctx.trap.side[1].end_v ].c2d[1];
	}
	last_nc_declived_i = last_nc_declived & 0xffff0000;
	d_nc_declived = 0x10000;
	while (ctx.poly.nc_declived != last_nc_declived_i) {
		ctx.poly.nc_declived_next = ctx.poly.nc_declived + d_nc_declived;
		draw_trapeze_int();
	}
	// And now finish the last subtrapeze
	ctx.poly.nc_declived_next = last_nc_declived;
	draw_trapeze_frac();
}

/*
 * Public Functions
 */

// nb DIVs = 2 + 3*nb_sizes+2*nb_scan_lines
void draw_poly_nopersp(void)
{
	unsigned previous_target = perftime_target();
	// TODO: disable use_intens if rendering_smooth
	perftime_enter(PERF_POLY, "poly");
	ctx.poly.nc_log = ctx.location.buffer_loc[gpuOutBuffer].width_log;
	// bounding box
	unsigned b_vec, c_vec, v;
	v = c_vec = b_vec = ctx.poly.first_vector;
	do {
		if (ctx.poly.vectors[v].c2d[1] < ctx.poly.vectors[c_vec].c2d[1]) {
			c_vec = v;
		}
		if (ctx.poly.vectors[v].c2d[1] > ctx.poly.vectors[b_vec].c2d[1]) {
			b_vec = v;
		}
		v = ctx.poly.vectors[v].next;
	} while (v != ctx.poly.first_vector);	
	ctx.poly.nc_declived = ctx.poly.vectors[c_vec].c2d[1];
	ctx.trap.side[0].start_v = ctx.trap.side[0].end_v = ctx.trap.side[1].start_v = ctx.trap.side[1].end_v = c_vec;
	// Now that all is initialized, build rasterized
#	if defined(GP2X) || defined(TEST_RASTERIZER)
	ctx.poly.rasterizer = jit_prepare_rasterizer();
#	endif
	// cut into trapezes
#	define DNC_MIN 0x8000
	do {
		for (int side=2; side--; ) {
			bool dc_ok = true;
			int32_t dnc;
			while (1) {
				dnc = ctx.poly.vectors[ ctx.trap.side[side].end_v ].c2d[1] - ctx.poly.nc_declived;
				if (dnc > DNC_MIN) break;	// not flat
				if (0 == ctx.poly.cmd->size--) goto end_poly;
				ctx.trap.side[side].start_v = ctx.trap.side[side].end_v;
				ctx.trap.side[side].end_v = 0==side ? ctx.poly.vectors[ctx.trap.side[side].end_v].prev:ctx.poly.vectors[ctx.trap.side[side].end_v].next;
				ctx.trap.side[side].c = ctx.poly.vectors[ ctx.trap.side[side].start_v ].c2d[0];
				dc_ok = false;
			}
			if (! dc_ok) {
				int32_t const num = ctx.poly.vectors[ ctx.trap.side[side].end_v ].c2d[0] - ctx.trap.side[side].c;
				dnc = Fix_abs(dnc);
				ctx.trap.side[side].dc = ((int64_t)num<<16)/dnc;	// FIXME: use Fix_div
				// compute alpha_params used for vector parameters
				if (ctx.poly.nb_params > 0) {
					int32_t const inv_dalpha = Fix_inv(dnc);
					for (unsigned p=ctx.poly.nb_params; p--; ) {
						int32_t const P0 = ctx.poly.vectors[ ctx.trap.side[side].start_v ].cmd->u.geom.param[p];
						int32_t const PN = ctx.poly.vectors[ ctx.trap.side[side].end_v ].cmd->u.geom.param[p];
						ctx.trap.side[side].param[p] = P0;
						ctx.trap.side[side].param_alpha[p] = Fix_mul(PN-P0, inv_dalpha);
					}
				}
			}
		}
		// render trapeze (and advance nc_declived, c, and all params)
		draw_trapeze();
	} while (1);
end_poly:;
	perftime_enter(previous_target, NULL);
}
