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

/*
 * Data Definitions
 */

#define SWAP(type,a,b) do { \
	type dummy = a; \
	a = b; \
	b = dummy; \
} while(0)

enum { MIN, MAX };

/*
 * Private Functions
 */

static int start_poly = 0;	// DEBUG

typedef void (*draw_line_t)(void);

#ifdef GP2X
extern void draw_line_c(void);
#else
static void draw_line_c(void) {
	do {
//		if (start_poly) color |= ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		*w = ctx.poly.cmdFacet.color;
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}
#endif
static void draw_line_ci(void) {
	do {
		uint32_t color = ctx.poly.cmdFacet.color;
//		if (start_poly) color |= ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t const i = ctx.line.param[0]>>16;	// we use 32 bits to get a carry flag in bit 16
		uint32_t color_i = color + i;
		if ((color_i^color)&0x20) color_i |= 0x1f;	// saturate blue
		color_i += i<<6;
		if ((color_i^color)&0x800) color_i |= 0x7e0;	// saturate green
		color_i += i<<11;
		if ((color_i^color)&0x10000) color_i |= 0xf800;	// saturate red
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		*w = color_i;
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}

static void draw_line_uv(void) {
	do {
		uint32_t color = texture_color(&ctx.location.txt, ctx.line.param[0], ctx.line.param[1]);
//		if (start_poly) color |= ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		*w = color;
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}

#define SAT8(x) do { if ((x)>=256) (x)=255; else if ((x)<0) (x)=0; } while(0)
static void draw_line_uvi(void) {
	do {
		uint32_t color = texture_color(&ctx.location.txt, ctx.line.param[0], ctx.line.param[1]);
//		if (start_poly) color |= ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		int32_t i = ctx.line.param[2]>>16;	// we use 32 bits to get a carry flag in bit 16
#ifdef GP2X	// gp2x uses YUV
		int y = color&0xff;
		y += (220*i)>>8;
		SAT8(y);
		*w = (color&0xff00ff00) | (y<<16) | y;
#else
		int r = (color>>16)+i;
		int g = ((color>>8)&255)+i;
		int b = (color&255)+i;
		SAT8(r);
		SAT8(g);
		SAT8(b);
		*w = (r<<16)|(g<<8)|b;
#endif
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.param[2] += ctx.line.dparam[2];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}

// buffers are so lower coords have lower addresses. direct coord system was just a convention, right ?
static void draw_line(void) {
	int32_t const c_start = ctx.trap.side[0].c>>16;
	ctx.line.dw = 4;
	ctx.line.ddecliv = ctx.poly.decliveness;
	ctx.line.count = (ctx.trap.side[1].c>>16) - c_start;
	if (ctx.line.count < 0) {
		ctx.line.count = -ctx.line.count;
		ctx.line.dw = -4;
		ctx.line.ddecliv = -ctx.line.ddecliv;
	}
	int32_t inv_dc = 0;
	if (ctx.poly.nb_params > 0) {
		if (ctx.line.count) inv_dc = Fix_inv(ctx.line.count<<16);
		for (unsigned p=ctx.poly.nb_params; p--; ) {
			ctx.line.param[p] = ctx.trap.side[0].param[p];	// starting value
			ctx.line.dparam[p] = ((int64_t)(ctx.trap.side[1].param[p]-ctx.line.param[p])*inv_dc)>>16;
		}
	}
	ctx.line.decliv = ctx.poly.decliveness*c_start;	// 16.16
	if (ctx.poly.scan_dir == 0) {
		ctx.line.w = (uint8_t *)&shared->buffers[ctx.location.out.address + ((ctx.poly.nc_declived+ctx.view.winPos[1])<<ctx.location.out.width_log) + c_start+ctx.view.winPos[0]];
	} else {
		ctx.line.w = (uint8_t *)&shared->buffers[ctx.location.out.address + ((c_start+ctx.view.winPos[1])<<ctx.location.out.width_log) + ctx.poly.nc_declived + ctx.view.winPos[0]];
		ctx.line.dw <<= ctx.location.out.width_log;
	}
	static draw_line_t const draw_lines[NB_RENDERING_TYPES] = {
		draw_line_c, draw_line_ci, draw_line_uv, draw_line_uvi,
	};
	perftime_enter(PERF_POLY_DRAW, "poly_draw");
	draw_lines[ctx.poly.cmdFacet.rendering_type]();
	perftime_enter(PERF_POLY, NULL);
	if (start_poly) start_poly --;
}

/*
 * Public Functions
 */

// nb DIVs = 2 + 3*nb_sizes+2*nb_scan_lines
void draw_poly(void) {
	perftime_enter(PERF_POLY, "poly");
	start_poly = 6;
	ctx.poly.decliveness = 0;
	ctx.poly.scan_dir = 0;
	ctx.poly.nc_log = ctx.location.out.width_log+2;
	// bounding box
	{
		unsigned v=ctx.poly.first_vector;
		ctx.poly.bbox[0][MIN] = ctx.poly.bbox[1][MIN] = ctx.poly.bbox[2][MIN] = ctx.poly.bbox[0][MAX] = ctx.poly.bbox[1][MAX] = ctx.poly.bbox[2][MAX] = v;
		do {
			for (unsigned c=2; c--; ) {
				if (ctx.poly.vectors[v].c2d[c] < ctx.poly.vectors[ctx.poly.bbox[c][MIN]].c2d[c])
					ctx.poly.bbox[c][MIN] = v;
				if (ctx.poly.vectors[v].c2d[c] > ctx.poly.vectors[ctx.poly.bbox[c][MAX]].c2d[c])
					ctx.poly.bbox[c][MAX] = v;
			}
			if (ctx.poly.vectors[v].cmdVector.geom.c3d[2] < ctx.poly.vectors[ctx.poly.bbox[2][MIN]].cmdVector.geom.c3d[2])
				ctx.poly.bbox[2][MIN] = v;
			if (ctx.poly.vectors[v].cmdVector.geom.c3d[2] > ctx.poly.vectors[ctx.poly.bbox[2][MAX]].cmdVector.geom.c3d[2])
				ctx.poly.bbox[2][MAX] = v;
			v = ctx.poly.vectors[v].next;
		} while (v != ctx.poly.first_vector);
	}
	// compute decliveness (2 DIVs)
	if (ctx.poly.nb_params > 0) {	// else we keep default values : we don't need decliveness.
		int32_t m[2];
		// FIXME: with clipping, A can get very close from A or B.
#		define B_VEC (ctx.poly.vectors[ctx.poly.bbox[2][MIN]])
#		define C_VEC (ctx.poly.vectors[ctx.poly.bbox[2][MAX]])
		int a_vec = ctx.poly.first_vector;
		while (a_vec == ctx.poly.bbox[2][MIN] || a_vec == ctx.poly.bbox[2][MAX]) a_vec = ctx.poly.vectors[a_vec].next;
#		define A_VEC (ctx.poly.vectors[a_vec])
		if (B_VEC.cmdVector.geom.c3d[2] == C_VEC.cmdVector.geom.c3d[2]) {	// BC is Z-const
			m[0] = C_VEC.c2d[0]-B_VEC.c2d[0];
			m[1] = C_VEC.c2d[1]-B_VEC.c2d[1];
		} else {
			int32_t alpha = ((int64_t)(A_VEC.cmdVector.geom.c3d[2]-B_VEC.cmdVector.geom.c3d[2])<<16)/(C_VEC.cmdVector.geom.c3d[2]-B_VEC.cmdVector.geom.c3d[2]);
			for (unsigned c=2; c--; ) {
				m[c] = B_VEC.cmdVector.geom.c3d[c]-A_VEC.cmdVector.geom.c3d[c]+((int64_t)alpha*(C_VEC.cmdVector.geom.c3d[c]-B_VEC.cmdVector.geom.c3d[c])>>16);
			}
		}
		if (Fix_abs(m[0]) < Fix_abs(m[1])) {
			ctx.poly.scan_dir = 1;
			ctx.poly.nc_log = 2;
			SWAP(int32_t, m[0], m[1]);
		}
		if (m[0]) ctx.poly.decliveness = (((int64_t)m[1])<<16)/m[0];
	}
	// init trapeze. start at Z min.
	int64_t z_num, z_den;	/*24.16*/
	int32_t z_dden, z_dnum; /*16.16*/
	int32_t Z0 = 0, ZN = 0;
	{
		ctx.poly.nc_declived = INT32_MAX;
		int32_t max_nc_declived = INT32_MIN;
		unsigned start_v=0, end_v=0;	// to please GCC
		ctx.poly.nc_dir = 1;
		unsigned v = ctx.poly.first_vector;
		do {
			int32_t const my_nc_declived = ctx.poly.vectors[v].c2d[!ctx.poly.scan_dir] - ((ctx.poly.decliveness*ctx.poly.vectors[v].c2d[ctx.poly.scan_dir])>>16);
			ctx.poly.vectors[v].nc_declived = my_nc_declived;
			if (my_nc_declived <= ctx.poly.nc_declived) {
				ctx.poly.nc_declived = my_nc_declived;
				Z0 = ctx.poly.vectors[v].cmdVector.geom.c3d[2];
				start_v = v;
			}
			if (my_nc_declived >= max_nc_declived) {
				max_nc_declived = my_nc_declived;
				ZN = ctx.poly.vectors[v].cmdVector.geom.c3d[2];
				end_v = v;
			}
			v = ctx.poly.vectors[v].next;
		} while (v != ctx.poly.first_vector);
		if (ZN > Z0) {
			SWAP(int32_t, Z0, ZN);
			SWAP(int32_t, ctx.poly.nc_declived, max_nc_declived);
			SWAP(unsigned, start_v, end_v);
			ctx.poly.nc_dir = -1;
		}
		// init z-interpolation along non-scanlines (nc)
		{
			z_num = 0;
			int32_t const DZ = ZN - Z0 /*16.16*/;
			z_dden = ctx.poly.nc_dir == 1 ? -DZ:DZ;
			z_dnum = ctx.poly.nc_dir == 1 ? Z0:-Z0;
			int32_t const dnc = max_nc_declived-ctx.poly.nc_declived;
			z_den = (int64_t)ZN*dnc;
			ctx.trap.side[0].start_v = ctx.trap.side[0].end_v = ctx.trap.side[1].start_v = ctx.trap.side[1].end_v = start_v;
			ctx.poly.z_alpha = 0;
		}
	}
	// render trapeze
	do {
		// advance trapeze and compute dc if needed
		for (int side=2; side--; ) {
			bool dc_ok = true;
			while (
				(ctx.poly.nc_dir == 1 && ctx.poly.vectors[ ctx.trap.side[side].end_v ].nc_declived <= ctx.poly.nc_declived) ||
				(ctx.poly.nc_dir == -1 && ctx.poly.vectors[ ctx.trap.side[side].end_v ].nc_declived >= ctx.poly.nc_declived)
			) {	// skip "flat" trapezes
				if (0 == ctx.poly.cmdFacet.size--) goto end_poly;
				ctx.trap.side[side].start_v = ctx.trap.side[side].end_v;
				ctx.trap.side[side].end_v = 0==side ? ctx.poly.vectors[ctx.trap.side[side].end_v].prev:ctx.poly.vectors[ctx.trap.side[side].end_v].next;
				ctx.trap.side[side].c = ctx.poly.vectors[ ctx.trap.side[side].start_v ].c2d[ctx.poly.scan_dir]<<16;
				dc_ok = false;
			}
			if (! dc_ok) {	// 3 DIVs / sides
				int32_t const num = ctx.poly.vectors[ ctx.trap.side[side].end_v ].c2d[ctx.poly.scan_dir]-ctx.poly.vectors[ ctx.trap.side[side].start_v ].c2d[ctx.poly.scan_dir];
				int32_t dnc = ctx.poly.vectors[ ctx.trap.side[side].end_v ].nc_declived - ctx.poly.nc_declived;
				dnc = Fix_abs(dnc);
				ctx.trap.side[side].dc = ((int64_t)num<<16)/dnc;
				// compute alpha_params used for vector parameters
				if (ctx.poly.nb_params > 0) {
					// first, compute the z_alpha of end_v (and store it for later comparaison)
					int64_t n = z_num + (int64_t)z_dnum*dnc;	// 25.16
					int64_t d = z_den + (int64_t)z_dden*dnc;	// 25.16
					ctx.trap.side[side].z_alpha_start = ctx.poly.z_alpha;
					ctx.trap.side[side].z_alpha_end = ctx.poly.z_alpha;
					if (d) ctx.trap.side[side].z_alpha_end = ((int64_t)n<<16)/d;
					// then alpha_params
					int32_t const dalpha = ctx.trap.side[side].z_alpha_end - ctx.trap.side[side].z_alpha_start;
					int32_t inv_dalpha = 0;
					if (dalpha) inv_dalpha = Fix_inv(dalpha);
					for (unsigned p=ctx.poly.nb_params; p--; ) {
						int32_t const P0 = ctx.poly.vectors[ ctx.trap.side[side].start_v ].cmdVector.geom.param[p];
						int32_t const PN = ctx.poly.vectors[ ctx.trap.side[side].end_v ].cmdVector.geom.param[p];
						ctx.trap.side[side].param[p] = P0;
						ctx.trap.side[side].param_alpha[p] = (((int64_t)PN-P0)*inv_dalpha)>>16;
					}
				}
				start_poly ++;
			}
		}
		// draw 'scanline'
		draw_line();	// will do another DIV
		// next scanline (1 DIV)
		ctx.poly.nc_declived += ctx.poly.nc_dir;
		if (ctx.poly.nb_params > 0) {
			z_den += z_dden;
			z_num += z_dnum;
			if (z_den) ctx.poly.z_alpha = ((int64_t)z_num<<16)/z_den;
		}
		for (unsigned side=2; side--; ) {
			int32_t dalpha = ctx.poly.z_alpha - ctx.trap.side[side].z_alpha_start;
			for (unsigned p=ctx.poly.nb_params; p--; ) {
				ctx.trap.side[side].param[p] = (((int64_t)ctx.trap.side[side].param_alpha[p]*dalpha)>>16) + ctx.poly.vectors[ ctx.trap.side[side].start_v ].cmdVector.geom.param[p];
			}
			ctx.trap.side[side].c += ctx.trap.side[side].dc;
		}
	} while (1);
end_poly:;
	perftime_leave();
}

