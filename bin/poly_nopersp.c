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

// buffers are so lower coords have lower addresses.
static void draw_line(void)
{
	int32_t const c_start = ctx.trap.side[ctx.trap.left_side].c >> 16;
	ctx.line.count = (ctx.trap.side[!ctx.trap.left_side].c >> 16) - c_start;
	if (unlikely(ctx.line.count <= 0)) return;	// may happen on some pathological cases ?
	ctx.line.w = ctx.location.out_start + c_start + ((ctx.poly.nc_declived>>16)<<ctx.location.buffer_loc[gpuOutBuffer].width_log);
	if (unlikely(! ctx.trap.is_triangle)) {
		int32_t const inv_dc = Fix_uinv(ctx.line.count<<16);
		for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
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
	for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
		ctx.trap.side[side].param[p] += Fix_mul(dnc, ctx.trap.side[side].param_alpha[p]);
	}
}

static void draw_trapeze_frac(int32_t dnc)
{
	dnc = Fix_abs(dnc);
	// now compute next c and params
	next_params_frac(0, dnc);
	next_params_frac(1, dnc);
	// draw 'scanline'
	draw_line();	// will do another DIV
}

static void next_params_int(unsigned side)
{
	ctx.trap.side[side].c += ctx.trap.side[side].dc;
	for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
		ctx.trap.side[side].param[p] += ctx.trap.side[side].param_alpha[p];
	}
}

static void draw_trapeze_int(void)
{
	// now compute next c and params
	next_params_int(0);
	next_params_int(1);
	// draw 'scanline'
	draw_line();	// will do another DIV
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
	// Compute constant linear coefs for non-perspective mode if possible
	// We can use the same dparam for all the trapeze if the trapeze is in fact a triangle
	ctx.trap.is_triangle = ctx.trap.side[0].start_v == ctx.trap.side[1].start_v ||
		ctx.trap.side[0].end_v == ctx.trap.side[1].end_v;
	if (ctx.trap.is_triangle) {
		int32_t ddc = ctx.trap.side[0].dc - ctx.trap.side[1].dc;
		int32_t dc0 = ctx.trap.side[0].c - ctx.trap.side[1].c;
		if (Fix_abs(ddc) > Fix_abs(dc0)) {
			ddc = Fix_inv(ddc);
			for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
				ctx.line.dparam[p] = Fix_mul(ddc, ctx.trap.side[0].param_alpha[p] - ctx.trap.side[1].param_alpha[p]);
			}
		} else {
			if (dc0 != 0) {
				dc0 = Fix_inv(dc0);
				for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
					ctx.line.dparam[p] = Fix_mul(dc0, ctx.trap.side[0].param[p] - ctx.trap.side[1].param[p]);
				}
			} else {
				for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
					ctx.line.dparam[p] = 0;
				}
			}
		}
	}
	// First, we draw from nc_declived to next scanline boundary or end of trapeze
	if (ctx.poly.nc_declived & 0xffff) {
		bool quit = false;
		int32_t nc_declived_next = ctx.poly.nc_declived & 0xffff0000;
		nc_declived_next += 1<<16;
		// maybe we reached end of trapeze already ?
		for (unsigned side=2; side--; ) {
			if (ctx.trap.side[side].end_v->c2d[1] <= nc_declived_next) {
				nc_declived_next = ctx.trap.side[side].end_v->c2d[1];
				quit = true;
			}
		}
		draw_trapeze_frac(nc_declived_next - ctx.poly.nc_declived);
		ctx.poly.nc_declived = nc_declived_next;
		if (quit) return;
	}
	// Now draw integral scanlines
	int32_t last_nc_declived = ctx.trap.side[0].end_v->c2d[1];
	if (ctx.trap.side[1].end_v->c2d[1] <= last_nc_declived) {
		last_nc_declived = ctx.trap.side[1].end_v->c2d[1];
	}
	int32_t last_nc_declived_i = last_nc_declived & 0xffff0000;
	while (ctx.poly.nc_declived != last_nc_declived_i) {
		draw_trapeze_int();
		ctx.poly.nc_declived += 0x10000;
	}
	// And now finish the last subtrapeze
	draw_trapeze_frac(last_nc_declived - ctx.poly.nc_declived);
	ctx.poly.nc_declived = last_nc_declived;
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
	gpuVector const *v, *c_vec;
	v = ctx.points.first_vector;
	c_vec = v;
	do {
		if (v->c2d[1] < c_vec->c2d[1]) {
			c_vec = v;
		}
		v = v->next;
	} while (v != ctx.points.first_vector);	
	ctx.poly.nc_declived = c_vec->c2d[1];
	ctx.trap.side[0].start_v = ctx.trap.side[0].end_v = ctx.trap.side[1].start_v = ctx.trap.side[1].end_v = c_vec;
	// Now that all is initialized, build rasterized
#	if defined(GP2X) || defined(TEST_RASTERIZER)
	ctx.poly.rasterizer = jit_prepare_rasterizer();
#	endif
	// cut into trapezes
#	define DNC_MIN 0x800
	do {
		for (int side=2; side--; ) {
			bool dc_ok = true;
			int32_t dnc;
			while (1) {
				dnc = ctx.trap.side[side].end_v->c2d[1] - ctx.poly.nc_declived;
				if (dnc > DNC_MIN) break;	// not flat
				if (0 == ctx.poly.cmd->size--) goto end_poly;
				ctx.trap.side[side].start_v = ctx.trap.side[side].end_v;
				ctx.trap.side[side].end_v = 0==side ? ctx.trap.side[0].end_v->prev : ctx.trap.side[1].end_v->next;
				ctx.trap.side[side].c = ctx.trap.side[side].start_v->c2d[0];
				dc_ok = false;
			}
			if (! dc_ok) {
				int32_t const num = ctx.trap.side[side].end_v->c2d[0] - ctx.trap.side[side].c;
				dnc = Fix_abs(dnc);
				ctx.trap.side[side].dc = Fix_div(num, dnc);
				// compute alpha_params used for vector parameters
				int32_t const inv_dalpha = Fix_inv(dnc);
				for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
					int32_t const P0 = ctx.trap.side[side].start_v->cmd->u.geom.param[p];
					int32_t const PN = ctx.trap.side[side].end_v->cmd->u.geom.param[p];
					ctx.trap.side[side].param[p] = P0;
					ctx.trap.side[side].param_alpha[p] = Fix_mul(PN-P0, inv_dalpha);
				}
			}
		}
		// render trapeze (and advance nc_declived, c, and all params)
		draw_trapeze();
	} while (1);
end_poly:;
	perftime_enter(previous_target, NULL);
}
