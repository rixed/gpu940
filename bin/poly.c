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
static void draw_scanline(void) {
	int32_t const c_start = ctx.trap.side[ctx.trap.left_side].c >> 16;
	ctx.line.count = (ctx.trap.side[!ctx.trap.left_side].c >> 16) - c_start;
	if (unlikely(ctx.line.count <= 0)) return;	// may happen on some pathological cases
	ctx.line.w = ctx.location.out_start + c_start + ((ctx.poly.nc_declived>>16)<<ctx.location.buffer_loc[gpuOutBuffer].width_log);
	ctx.line.decliv = ctx.poly.decliveness * c_start;	// 16.16
	if (ctx.poly.scan_dir != 0) {
		ctx.line.w = ctx.location.out_start + (ctx.poly.nc_declived>>16) + (c_start<<ctx.location.buffer_loc[gpuOutBuffer].width_log);
	}
	int32_t const inv_dc = Fix_uinv(ctx.line.count<<16);
	for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
		ctx.line.dparam[p] = Fix_mul(ctx.trap.side[!ctx.trap.left_side].param[p] - ctx.line.param[p], inv_dc);
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

static void next_params_frac(unsigned side, int32_t dnc) {
	ctx.trap.side[side].c += Fix_mul(dnc, ctx.trap.side[side].dc);
	int32_t dalpha = ctx.poly.z_alpha - ctx.trap.side[side].z_alpha_start;
	for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
		ctx.trap.side[side].param[p] = Fix_mul(ctx.trap.side[side].param_alpha[p], dalpha) + ctx.trap.side[side].start_v->cmd->u.geom.param[p];
	}
}

static void draw_trapeze_frac(int32_t dnc) {
	dnc = Fix_abs(dnc);
	// compute next z_alpha
	ctx.poly.z_den += (int64_t)dnc*ctx.poly.z_dden;
	ctx.poly.z_num += (int64_t)dnc*ctx.poly.z_dnum;
	if (ctx.poly.z_den) ctx.poly.z_alpha = ctx.poly.z_num/(ctx.poly.z_den>>16);	// FIXME: use Fix_div
	// now compute next c and params
	next_params_frac(0, dnc);
	next_params_frac(1, dnc);
	// draw 'scanline'
	draw_scanline();	// will do another DIV
}

static void next_params_int(unsigned side) {
	ctx.trap.side[side].c += ctx.trap.side[side].dc;
	int32_t dalpha = ctx.poly.z_alpha - ctx.trap.side[side].z_alpha_start;
	for (unsigned p=sizeof_array(ctx.line.dparam); p--; ) {
		ctx.trap.side[side].param[p] = Fix_mul(ctx.trap.side[side].param_alpha[p], dalpha) + ctx.trap.side[side].start_v->cmd->u.geom.param[p];
	}
}

static void draw_trapeze_int(void) {
	// compute next z_alpha
	ctx.poly.z_den += (int64_t)ctx.poly.z_dden<<16;	// wrong if !complete_scan_line
	ctx.poly.z_num += (int64_t)ctx.poly.z_dnum<<16;
	if (ctx.poly.z_den) ctx.poly.z_alpha = ctx.poly.z_num/(ctx.poly.z_den>>16);
	// now compute next c and params
	next_params_int(0);
	next_params_int(1);
	// draw 'scanline'
	draw_scanline();	// will do another DIV
}

static void draw_trapeze(void) {
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
	// First, we draw from nc_declived to next scanline boundary or end of trapeze
	if (ctx.poly.nc_declived & 0xffff) {
		bool quit = false;
		int32_t nc_declived_next = ctx.poly.nc_declived & 0xffff0000;
		if (ctx.poly.nc_dir > 0) {
			nc_declived_next += 1<<16;
		}
		// maybe we reached end of trapeze already ?
		for (unsigned side=2; side--; ) {
			if (
					(ctx.poly.nc_dir > 0 && ctx.trap.side[side].end_v->nc_declived <= nc_declived_next) ||
					(ctx.poly.nc_dir < 0 && ctx.trap.side[side].end_v->nc_declived >= nc_declived_next)
				) {
				nc_declived_next = ctx.trap.side[side].end_v->nc_declived;
				quit = true;
			}
		}
		draw_trapeze_frac(nc_declived_next - ctx.poly.nc_declived);
		ctx.poly.nc_declived = nc_declived_next;
		if (quit) return;
	}
	// Now draw integral scanlines
	int32_t d_nc_declived, last_nc_declived_i;
	int32_t last_nc_declived = ctx.trap.side[0].end_v->nc_declived;
	if (ctx.poly.nc_dir > 0) {
		if (ctx.trap.side[1].end_v->nc_declived <= last_nc_declived) {
			last_nc_declived = ctx.trap.side[1].end_v->nc_declived;
		}
		last_nc_declived_i = last_nc_declived & 0xffff0000;
		d_nc_declived = 0x10000;
	} else {
		if (ctx.trap.side[1].end_v->nc_declived >= last_nc_declived) {
			last_nc_declived = ctx.trap.side[1].end_v->nc_declived;
		}
		last_nc_declived_i = (last_nc_declived & 0xffff0000) + 0x10000;
		d_nc_declived = -0x10000;
	}
	while (ctx.poly.nc_declived != last_nc_declived_i) {
		draw_trapeze_int();
		ctx.poly.nc_declived += d_nc_declived;
	}
	// And now finish the last subtrapeze
	draw_trapeze_frac(last_nc_declived - ctx.poly.nc_declived);
	ctx.poly.nc_declived = last_nc_declived;
}

/*
 * Public Functions
 */

// nb DIVs = 2 + 3*nb_sizes+2*nb_scan_lines
void draw_poly_persp(void) {
	unsigned previous_target = perftime_target();
	// TODO: disable use_intens if rendering_smooth
	perftime_enter(PERF_POLY, "poly");
	// compute decliveness related parameters
	ctx.poly.decliveness = 0;
	ctx.poly.scan_dir = 0;
	ctx.line.dw = 1;
	ctx.poly.z_num = 0;
	ctx.poly.nc_dir = 1;
	ctx.poly.nc_log = ctx.location.buffer_loc[gpuOutBuffer].width_log;
	// bounding box
	gpuVector const *b_vec, *c_vec;
	c_vec = b_vec = ctx.points.first_vector;
	// compute decliveness (2 DIVs)
	// FIXME: with clipping, A can get very close from B or C.
	// we want b = zmax, c = zmin (closer)
	gpuVector *v = ctx.points.first_vector;
	do {
		if (v->cmd->u.geom.c3d[2] > b_vec->cmd->u.geom.c3d[2]) {
			b_vec = v;
		}
		if (v->cmd->u.geom.c3d[2] <= c_vec->cmd->u.geom.c3d[2]) {
			c_vec = v;
		}
		v = v->next;
	} while (v != ctx.points.first_vector);
	int32_t m[2];	// 16.16
	gpuVector *a_vec;
	do {
		if (v != b_vec && v != c_vec) {
			a_vec = v;
			break;
		}
		v = v->next;
	} while (1);
	if (b_vec->cmd->u.geom.c3d[2] == c_vec->cmd->u.geom.c3d[2]) {	// BC is Z-const
		m[0] = c_vec->c2d[0] - b_vec->c2d[0];
		m[1] = c_vec->c2d[1] - b_vec->c2d[1];
	} else {
		int64_t alpha = ((int64_t)(a_vec->cmd->u.geom.c3d[2] - b_vec->cmd->u.geom.c3d[2])<<31) / (c_vec->cmd->u.geom.c3d[2] - b_vec->cmd->u.geom.c3d[2]);
		for (unsigned c=2; c--; ) {
			m[c] = b_vec->cmd->u.geom.c3d[c] - a_vec->cmd->u.geom.c3d[c]+(alpha*(c_vec->cmd->u.geom.c3d[c] - b_vec->cmd->u.geom.c3d[c])>>31);
		}
	}
	if (Fix_abs(m[0]) < Fix_abs(m[1])) {
		ctx.poly.scan_dir = 1;
		ctx.poly.nc_log = 0;
		SWAP(int32_t, m[0], m[1]);
		ctx.line.dw <<= ctx.location.buffer_loc[gpuOutBuffer].width_log;
	}
	if (m[0]) ctx.poly.decliveness = (((int64_t)m[1])<<16)/m[0];
	// compute all nc_declived
	v = ctx.points.first_vector;
	do {
		v->nc_declived = v->c2d[!ctx.poly.scan_dir] - Fix_mul(ctx.poly.decliveness, v->c2d[ctx.poly.scan_dir]);
		v = v->next;
	} while (v != ctx.points.first_vector);
	// init trapeze. start at Z min (closer) (or nc_decliv min if !dz)
	int32_t dz = b_vec->cmd->u.geom.c3d[2] - c_vec->cmd->u.geom.c3d[2];
	if (0 == dz) {	// if dz is 0, b_vec and c_vec are random or not set yet, and so would be dnc
		v = ctx.points.first_vector;
		do {
			if (v->nc_declived < c_vec->nc_declived) {
				c_vec = v;
			}
			if (v->nc_declived > b_vec->nc_declived) {
				b_vec = v;
			}
			v = v->next;
		} while (v != ctx.points.first_vector);		
		if (b_vec->cmd->u.geom.c3d[2] > c_vec->cmd->u.geom.c3d[2]) {
			SWAP(gpuVector const *, b_vec, c_vec);
		}
	}
	int32_t dnc, dnc_ = b_vec->nc_declived - c_vec->nc_declived;
	if (dnc_ > 0) {
		dnc = dnc_;
	} else {
		dnc = -dnc_;
		ctx.poly.nc_dir = -1;
	}
	ctx.poly.z_den = ((int64_t)b_vec->cmd->u.geom.c3d[2]*dnc_);
	ctx.poly.z_dnum = ctx.poly.nc_dir == 1 ? c_vec->cmd->u.geom.c3d[2] : -c_vec->cmd->u.geom.c3d[2];
	ctx.poly.z_dden = ctx.poly.nc_dir == 1 ? -dz:dz;
	ctx.poly.z_alpha = 0;
	ctx.poly.nc_declived = c_vec->nc_declived;
	ctx.trap.side[0].start_v = ctx.trap.side[0].end_v = ctx.trap.side[1].start_v = ctx.trap.side[1].end_v = c_vec;
	// Now that nc_log is known, prepare the rasterizer
#	if defined(GP2X) || defined(TEST_RASTERIZER)
	ctx.rendering.rasterizer = jit_prepare_rasterizer();
#	endif
	// cut into trapezes
#	define DNC_MIN 0x800
	do {
		for (int side=2; side--; ) {
			bool dc_ok = true;
			int32_t dnc;
			while (1) {
				dnc = ctx.trap.side[side].end_v->nc_declived - ctx.poly.nc_declived;
				if (
						(ctx.poly.nc_dir > 0 && dnc > DNC_MIN) ||
						(ctx.poly.nc_dir < 0 && dnc < -DNC_MIN)
					) break;	// not flat
				if (0 == ctx.poly.cmd->size--) goto end_poly;
				ctx.trap.side[side].start_v = ctx.trap.side[side].end_v;
				ctx.trap.side[side].end_v = 0==side ? ctx.trap.side[side].end_v->prev : ctx.trap.side[side].end_v->next;
				ctx.trap.side[side].c = ctx.trap.side[side].start_v->c2d[ctx.poly.scan_dir];
				dc_ok = false;
			}
			if (! dc_ok) {
				int32_t const num = ctx.trap.side[side].end_v->c2d[ctx.poly.scan_dir] - ctx.trap.side[side].c;
				dnc = Fix_abs(dnc);
				ctx.trap.side[side].dc = ((int64_t)num<<16)/dnc;
				// compute alpha_params used for vector parameters
				int32_t inv_dalpha = 0;
				// first, compute the z_alpha of end_v
				int64_t n = ctx.poly.z_num + (((int64_t)ctx.poly.z_dnum*dnc));	// 32.32
				int64_t d = ctx.poly.z_den + (((int64_t)ctx.poly.z_dden*dnc));	// 32.32
				ctx.trap.side[side].z_alpha_start = ctx.poly.z_alpha;
				int32_t z_alpha_end = d ? n/(d>>16) : ctx.poly.z_alpha;
				int32_t const dalpha = z_alpha_end - ctx.trap.side[side].z_alpha_start;
				if (dalpha) inv_dalpha = Fix_inv(dalpha);
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
