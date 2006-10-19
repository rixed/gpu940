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
#include "text.h"
#include "raster.h"

/*
 * Data Definitions
 */

int start_poly = 0;	// DEBUG

#define SWAP(type,a,b) do { \
	type dummy = a; \
	a = b; \
	b = dummy; \
} while(0)

/*
 * Private Functions
 */

typedef void (*draw_line_t)(void);

// buffers are so lower coords have lower addresses. direct coord system was just a convention, right ?
#define min(a,b) ((a)<=(b) ? (a):(b))
#define max(a,b) ((a)>(b) ? (a):(b))
static void draw_line(void) {
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_POLY_DRAW, "line");
	int32_t c_start, c_stop;
	c_start = ctx.trap.side[ctx.trap.left_side].c>>16;
	c_stop = ctx.trap.side[!ctx.trap.left_side].c>>16;	// TODO: +1 if fractionnal
	ctx.line.dw = 4;
	ctx.line.ddecliv = ctx.poly.decliveness;
	if ( c_stop < c_start ) {	// may happen on some pathological cases ?
		goto quit_dl;
	}
	ctx.line.count = c_stop - c_start;
	if (ctx.poly.nb_params > 0) {
		if (ctx.poly.cmdFacet.perspective || !ctx.trap.is_triangle) {	// We need to compute 
			int32_t inv_dc = 0;
			if (ctx.line.count) {
				inv_dc = Fix_inv(ctx.line.count<<16);
			}
			for (unsigned p=ctx.poly.nb_params; p--; ) {
				ctx.line.param[p] = ctx.trap.side[ctx.trap.left_side].param[p];	// starting value
				ctx.line.dparam[p] = Fix_mul(ctx.trap.side[!ctx.trap.left_side].param[p] - ctx.line.param[p], inv_dc);
			}
		} else {
			for (unsigned p=ctx.poly.nb_params; p--; ) {
				ctx.line.param[p] = ctx.trap.side[ctx.trap.left_side].param[p];	// starting value
				ctx.line.dparam[p] = ctx.trap.dparam[p];
			}
		}
	}
	ctx.line.decliv = ctx.poly.decliveness*c_start;	// 16.16
	if (ctx.poly.scan_dir == 0) {
		ctx.line.w = (uint8_t *)&shared->buffers[ctx.location.out.address + (((ctx.poly.nc_declived>>16)+ctx.view.winPos[1])<<ctx.location.out.width_log) + c_start+ctx.view.winPos[0]];
	} else {
		ctx.line.w = (uint8_t *)&shared->buffers[ctx.location.out.address + ((c_start+ctx.view.winPos[1])<<ctx.location.out.width_log) + (ctx.poly.nc_declived>>16) + ctx.view.winPos[0]];
		ctx.line.dw <<= ctx.location.out.width_log;
	}
	static draw_line_t const draw_lines[2][NB_RENDERING_TYPES] = {
		{ draw_line_c, draw_line_ci, draw_line_uv, draw_line_uvi_lin, draw_line_uvk, draw_line_cs, draw_line_shadow, draw_line_uvk_shadow },	// no perspective
		{ draw_line_c, draw_line_ci, draw_line_uv, draw_line_uvi, draw_line_uvk, draw_line_cs, draw_line_shadow, draw_line_uvk_shadow }	// perspective
	};
#ifdef GP2X
	{	// patch code
		bool patched = false;
		// nc_log
		extern uint16_t patch_uv_nc_log, patch_ci_nc_log, patch_uvi_nc_log;
		static uint32_t nc_log = ~0;
		if (ctx.poly.nc_log != nc_log) {
			patched = true;
			nc_log = ctx.poly.nc_log;
			patch_uv_nc_log = patch_ci_nc_log = patch_uvi_nc_log = 0x2001 | (nc_log<<7);
		}
		extern uint32_t patch_uv_dw, patch_ci_dw, patch_uvi_dw;
		static int32_t dw = ~0;
		if (ctx.line.dw != dw) {
			patched = true;
			dw = ctx.line.dw;
			int sign = 0x80;
			int dw_imm = dw;
			int dw_rot = 0;
			if (dw < 0) {
				sign = 0x40;
				dw_imm = -dw_imm;
			}
			while (dw_imm > 256) {
				dw_imm >>= 2;
				dw_rot ++;
			}
			if (dw_rot) dw_rot = 16-dw_rot;	// rotate right
			patch_uv_dw = patch_ci_dw = patch_uvi_dw = 0xe2000000 | (sign<<16) | (dw_rot<<8) | dw_imm;
		}
		if (patched) {
			__asm__ volatile (	// Drain write buffer then fush ICache
				"mov r0, #0\n"
				"mcr p15, 0, r0, c7, c10, 4\n"
				"mcr p15, 0, r0, c7, c5, 0\n"
				:::"r0"
			);
		}
	}
#endif
	draw_lines[ctx.poly.cmdFacet.perspective][ctx.poly.cmdFacet.rendering_type]();
	if (start_poly) start_poly --;
quit_dl:
	perftime_enter(previous_target, NULL);
}

static void next_params_frac(unsigned side, int32_t dnc) {
	ctx.trap.side[side].c += Fix_mul(dnc, ctx.trap.side[side].dc);
	if (ctx.poly.cmdFacet.perspective) {
		int32_t dalpha = ctx.poly.z_alpha - ctx.trap.side[side].z_alpha_start;
		for (unsigned p=ctx.poly.nb_params; p--; ) {
			ctx.trap.side[side].param[p] = Fix_mul(ctx.trap.side[side].param_alpha[p], dalpha) + ctx.poly.vectors[ ctx.trap.side[side].start_v ].cmdVector.u.geom.param[p];
		}
	} else {
		for (unsigned p=ctx.poly.nb_params; p--; ) {
			ctx.trap.side[side].param[p] += Fix_mul(dnc, ctx.trap.side[side].param_alpha[p]);
		}
	}	
}

static void draw_trapeze_frac(void) {
	int32_t dnc = ctx.poly.nc_declived_next - ctx.poly.nc_declived;
	dnc = Fix_abs(dnc);
	// compute next z_alpha
	if (ctx.poly.cmdFacet.perspective && ctx.poly.nb_params > 0) {
		ctx.poly.z_den += (int64_t)dnc*ctx.poly.z_dden;
		ctx.poly.z_num += (int64_t)dnc*ctx.poly.z_dnum;
		if (ctx.poly.z_den) ctx.poly.z_alpha = ctx.poly.z_num/(ctx.poly.z_den>>16);	// FIXME: use Fix_div
	}
	// now compute some next c and params
	for (unsigned side=2; side--; ) {
		if (ctx.trap.side[side].is_growing) {
			next_params_frac(side, dnc);
		}
	}
	// draw 'scanline'
	draw_line();	// will do another DIV
	// compute others next c and params
	for (unsigned side=2; side--; ) {
		if (! ctx.trap.side[side].is_growing) {
			next_params_frac(side, dnc);
		}
	}
	ctx.poly.nc_declived = ctx.poly.nc_declived_next;
}

static void next_params_int(unsigned side) {
	ctx.trap.side[side].c += ctx.trap.side[side].dc;
	if (ctx.poly.cmdFacet.perspective) {
		int32_t dalpha = ctx.poly.z_alpha - ctx.trap.side[side].z_alpha_start;
		for (unsigned p=ctx.poly.nb_params; p--; ) {
			ctx.trap.side[side].param[p] = Fix_mul(ctx.trap.side[side].param_alpha[p], dalpha) + ctx.poly.vectors[ ctx.trap.side[side].start_v ].cmdVector.u.geom.param[p];
		}
	} else {
		for (unsigned p=ctx.poly.nb_params; p--; ) {
			ctx.trap.side[side].param[p] += ctx.trap.side[side].param_alpha[p];
		}
	}
}

static void draw_trapeze_int(void) {
	// compute next z_alpha
	if (ctx.poly.cmdFacet.perspective && ctx.poly.nb_params > 0) {
		ctx.poly.z_den += (int64_t)ctx.poly.z_dden<<16;	// wrong if !complete_scan_line
		ctx.poly.z_num += (int64_t)ctx.poly.z_dnum<<16;
		if (ctx.poly.z_den) ctx.poly.z_alpha = ctx.poly.z_num/(ctx.poly.z_den>>16);
	}
	// now compute some next c and params
	for (unsigned side=2; side--; ) {
		if (ctx.trap.side[side].is_growing) {
			next_params_int(side);
		}
	}
	// draw 'scanline'
	draw_line();	// will do another DIV
	// now compute others c and params
	for (unsigned side=2; side--; ) {
		if (! ctx.trap.side[side].is_growing) {
			next_params_int(side);
		}
	}
	ctx.poly.nc_declived = ctx.poly.nc_declived_next;
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
	ctx.trap.side[ctx.trap.left_side].is_growing = ctx.trap.side[ctx.trap.left_side].dc < 0;
	ctx.trap.side[!ctx.trap.left_side].is_growing = ctx.trap.side[!ctx.trap.left_side].dc > 0;
	// Compute constant linear coefs for non-perspective mode if possible
	if (!ctx.poly.cmdFacet.perspective && ctx.poly.nb_params>0) {
		// We can use the same dparam for all the trapeze if the trapeze is in fact a triangle
		ctx.trap.is_triangle = ctx.trap.side[0].start_v == ctx.trap.side[1].start_v ||
			ctx.trap.side[0].end_v == ctx.trap.side[1].end_v;
		if (ctx.trap.is_triangle) {
			int32_t ddc = ctx.trap.side[0].dc - ctx.trap.side[1].dc;
			int32_t dc0 = ctx.trap.side[0].c - ctx.trap.side[1].c;
			if (Fix_abs(ddc) > Fix_abs(dc0)) {
				ddc = Fix_inv(ddc);
				for (unsigned p=ctx.poly.nb_params; p--; ) {
					ctx.trap.dparam[p] = Fix_mul(ddc, ctx.trap.side[0].param_alpha[p] - ctx.trap.side[1].param_alpha[p]);
				}
			} else {
				if (dc0 != 0) {
					dc0 = Fix_inv(dc0);
					for (unsigned p=ctx.poly.nb_params; p--; ) {
						ctx.trap.dparam[p] = Fix_mul(dc0, ctx.trap.side[0].param[p] - ctx.trap.side[1].param[p]);
					}
				} else {
					for (unsigned p=ctx.poly.nb_params; p--; ) {
						ctx.trap.dparam[p] = 0;
					}
				}
			}
		}
	}
	// First, we draw from nc_declived to next scanline boundary or end of trapeze
	if (ctx.poly.nc_declived & 0xffff) {
		bool quit = false;
		ctx.poly.nc_declived_next = ctx.poly.nc_declived & 0xffff0000;
		if (ctx.poly.nc_dir > 0) {
			ctx.poly.nc_declived_next += 1<<16;
		}
		// maybe we reached end of trapeze already ?
		for (unsigned side=2; side--; ) {
			if (
					(ctx.poly.nc_dir > 0 && ctx.poly.vectors[ ctx.trap.side[side].end_v ].nc_declived <= ctx.poly.nc_declived_next) ||
					(ctx.poly.nc_dir < 0 && ctx.poly.vectors[ ctx.trap.side[side].end_v ].nc_declived >= ctx.poly.nc_declived_next)
				) {
				ctx.poly.nc_declived_next = ctx.poly.vectors[ ctx.trap.side[side].end_v ].nc_declived;
				quit = true;
			}
		}
		draw_trapeze_frac();
		if (quit) return;
	}
	// Now draw integral scanlines
	int32_t d_nc_declived, last_nc_declived_i;
	int32_t last_nc_declived = ctx.poly.vectors[ ctx.trap.side[0].end_v ].nc_declived;
	if (ctx.poly.nc_dir > 0) {
		if (ctx.poly.vectors[ ctx.trap.side[1].end_v ].nc_declived <= last_nc_declived) {
			last_nc_declived = ctx.poly.vectors[ ctx.trap.side[1].end_v ].nc_declived;
		}
		last_nc_declived_i = last_nc_declived & 0xffff0000;
		d_nc_declived = 0x10000;
	} else {
		if (ctx.poly.vectors[ ctx.trap.side[1].end_v ].nc_declived >= last_nc_declived) {
			last_nc_declived = ctx.poly.vectors[ ctx.trap.side[1].end_v ].nc_declived;
		}
		last_nc_declived_i = (last_nc_declived & 0xffff0000) + 0x10000;
		d_nc_declived = -0x10000;
	}
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
void draw_poly(void) {
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_POLY, "poly");
	start_poly = 6;
	// compute decliveness related parameters
	ctx.poly.decliveness = 0;
	ctx.poly.scan_dir = 0;
	ctx.poly.nc_log = ctx.location.out.width_log+2;
	// bounding box
	unsigned b_vec, c_vec;
	c_vec = b_vec = ctx.poly.first_vector;
	if (ctx.poly.cmdFacet.perspective && ctx.poly.nb_params > 0) {
		// compute decliveness (2 DIVs)
		// FIXME: with clipping, A can get very close from B or C.
		// we want b = zmin, c = zmax (closer)
		unsigned v = ctx.poly.first_vector;
		do {
			if (ctx.poly.vectors[v].cmdVector.u.geom.c3d[2] < ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[2]) {
				b_vec = v;
			}
			if (ctx.poly.vectors[v].cmdVector.u.geom.c3d[2] >= ctx.poly.vectors[c_vec].cmdVector.u.geom.c3d[2]) {
				c_vec = v;
			}
			v = ctx.poly.vectors[v].next;
		} while (v != ctx.poly.first_vector);
		int32_t m[2];	// 16.16
		unsigned a_vec;
		do {
			if (v != b_vec && v != c_vec) {
				a_vec = v;
				break;
			}
			v = ctx.poly.vectors[v].next;
		} while (1);
		if (ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[2] == ctx.poly.vectors[c_vec].cmdVector.u.geom.c3d[2]) {	// BC is Z-const
			m[0] = ctx.poly.vectors[c_vec].c2d[0]-ctx.poly.vectors[b_vec].c2d[0];
			m[1] = ctx.poly.vectors[c_vec].c2d[1]-ctx.poly.vectors[b_vec].c2d[1];
		} else {
			int64_t alpha = ((int64_t)(ctx.poly.vectors[a_vec].cmdVector.u.geom.c3d[2]-ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[2])<<31)/(ctx.poly.vectors[c_vec].cmdVector.u.geom.c3d[2]-ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[2]);
			for (unsigned c=2; c--; ) {
				m[c] = ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[c]-ctx.poly.vectors[a_vec].cmdVector.u.geom.c3d[c]+(alpha*(ctx.poly.vectors[c_vec].cmdVector.u.geom.c3d[c]-ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[c])>>31);
			}
		}
		if (Fix_abs(m[0]) < Fix_abs(m[1])) {
			ctx.poly.scan_dir = 1;
			ctx.poly.nc_log = 2;
			SWAP(int32_t, m[0], m[1]);
		}
		if (m[0]) ctx.poly.decliveness = (((int64_t)m[1])<<16)/m[0];
	}
	// compute all nc_declived
	{	// TODO : include this in the previous if, else decliveness=0 so nc_declived = nc.
		unsigned v = ctx.poly.first_vector;
		do {
			ctx.poly.vectors[v].nc_declived = ctx.poly.vectors[v].c2d[!ctx.poly.scan_dir] - (((int64_t)ctx.poly.decliveness*ctx.poly.vectors[v].c2d[ctx.poly.scan_dir])>>16);
			v = ctx.poly.vectors[v].next;
		} while (v != ctx.poly.first_vector);	
	}
	// init trapeze. start at Z min.
	{
		ctx.poly.z_num = 0;
		ctx.poly.nc_dir = 1;
		int32_t dz = ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[2] - ctx.poly.vectors[c_vec].cmdVector.u.geom.c3d[2];
		if (0 == dz) {	// if dz is 0, b_vec and c_vec are random or not set yet, and so would be dnc
			unsigned v = ctx.poly.first_vector;
			do {
				if (ctx.poly.vectors[v].nc_declived < ctx.poly.vectors[c_vec].nc_declived) {
					c_vec = v;
				}
				if (ctx.poly.vectors[v].nc_declived > ctx.poly.vectors[b_vec].nc_declived) {
					b_vec = v;
				}
				v = ctx.poly.vectors[v].next;
			} while (v != ctx.poly.first_vector);		
			if (ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[2] > ctx.poly.vectors[c_vec].cmdVector.u.geom.c3d[2]) {
				SWAP(unsigned, b_vec, c_vec);
			}
		}
		ctx.poly.nc_declived = ctx.poly.vectors[c_vec].nc_declived;
		int32_t dnc, dnc_ = ctx.poly.vectors[b_vec].nc_declived - ctx.poly.vectors[c_vec].nc_declived;
		if (dnc_ > 0) {
			dnc = dnc_;
		} else {
			dnc = -dnc_;
			ctx.poly.nc_dir = -1;
		}
		if (ctx.poly.cmdFacet.perspective) {
			ctx.poly.z_den = ((int64_t)ctx.poly.vectors[b_vec].cmdVector.u.geom.c3d[2]*dnc_);
			ctx.poly.z_dnum = ctx.poly.nc_dir == 1 ? ctx.poly.vectors[c_vec].cmdVector.u.geom.c3d[2]:-ctx.poly.vectors[c_vec].cmdVector.u.geom.c3d[2];
			ctx.poly.z_dden = ctx.poly.nc_dir == 1 ? -dz:dz;
			ctx.poly.z_alpha = 0;
		}
		ctx.trap.side[0].start_v = ctx.trap.side[0].end_v = ctx.trap.side[1].start_v = ctx.trap.side[1].end_v = c_vec;
	}
	// cut into trapezes
#	define DNC_MIN 0x8000
	do {
		for (int side=2; side--; ) {
			bool dc_ok = true;
			int32_t dnc;
			while (1) {
				dnc = ctx.poly.vectors[ ctx.trap.side[side].end_v ].nc_declived - ctx.poly.nc_declived;
				if (
						(ctx.poly.nc_dir > 0 && dnc > DNC_MIN) ||
						(ctx.poly.nc_dir < 0 && dnc < -DNC_MIN)
					) break;	// not flat
				if (0 == ctx.poly.cmdFacet.size--) goto end_poly;
				ctx.trap.side[side].start_v = ctx.trap.side[side].end_v;
				ctx.trap.side[side].end_v = 0==side ? ctx.poly.vectors[ctx.trap.side[side].end_v].prev:ctx.poly.vectors[ctx.trap.side[side].end_v].next;
				ctx.trap.side[side].c = ctx.poly.vectors[ ctx.trap.side[side].start_v ].c2d[ctx.poly.scan_dir];
				dc_ok = false;
			}
			if (! dc_ok) {
				int32_t const num = ctx.poly.vectors[ ctx.trap.side[side].end_v ].c2d[ctx.poly.scan_dir] - ctx.trap.side[side].c;
				dnc = Fix_abs(dnc);
				ctx.trap.side[side].dc = ((int64_t)num<<16)/dnc;
				// compute alpha_params used for vector parameters
				if (ctx.poly.nb_params > 0) {
					int32_t inv_dalpha = 0;
					if (ctx.poly.cmdFacet.perspective) {
						// first, compute the z_alpha of end_v
						int64_t n = ctx.poly.z_num + (((int64_t)ctx.poly.z_dnum*dnc));	// 32.32
						int64_t d = ctx.poly.z_den + (((int64_t)ctx.poly.z_dden*dnc));	// 32.32
						ctx.trap.side[side].z_alpha_start = ctx.poly.z_alpha;
						ctx.trap.side[side].z_alpha_end = ctx.poly.z_alpha;
						if (d) ctx.trap.side[side].z_alpha_end = n/(d>>16);
						int32_t const dalpha = ctx.trap.side[side].z_alpha_end - ctx.trap.side[side].z_alpha_start;
						if (dalpha) inv_dalpha = Fix_inv(dalpha);
					} else {
						inv_dalpha = Fix_inv(dnc);
					}
					for (unsigned p=ctx.poly.nb_params; p--; ) {
						int32_t const P0 = ctx.poly.vectors[ ctx.trap.side[side].start_v ].cmdVector.u.geom.param[p];
						int32_t const PN = ctx.poly.vectors[ ctx.trap.side[side].end_v ].cmdVector.u.geom.param[p];
						ctx.trap.side[side].param[p] = P0;
						ctx.trap.side[side].param_alpha[p] = Fix_mul(PN-P0, inv_dalpha);
					}
				}
				start_poly += 2;
			}
		}
		// render trapeze (and advance nc_declived, c, and all params)
		draw_trapeze();
	} while (1);
end_poly:;
	perftime_enter(previous_target, NULL);
}
