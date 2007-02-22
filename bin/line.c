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

static void draw_scanline(void)
{
	int left_vec = 0;
	int32_t c_start = ctx.points.vectors[0].c2d[ctx.poly.scan_dir] >> 16;
	int32_t c_stop = ctx.points.vectors[1].c2d[ctx.poly.scan_dir] >> 16;
	if (c_start > c_stop) {
		left_vec = 1;
		SWAP(int32_t, c_start, c_stop);
	}
	ctx.line.count = c_stop - c_start;
	ctx.line.decliv = 0;
	int32_t const nc_start = ctx.points.vectors[left_vec].c2d[!ctx.poly.scan_dir] >> 16;
	if (ctx.poly.scan_dir == 0) {
		ctx.line.w = ctx.location.out_start + c_start + (nc_start<<ctx.location.buffer_loc[gpuOutBuffer].width_log);
	} else {
		ctx.line.w = ctx.location.out_start + nc_start + (c_start<<ctx.location.buffer_loc[gpuOutBuffer].width_log);
	}
	ctx.line.param = ctx.points.vectors[left_vec].cmd->u.geom.param;
	if (ctx.line.count) {
		int32_t const inv_dc = Fix_uinv(ctx.line.count<<16);
		for (unsigned p=GPU_NB_PARAMS; p--; ) {
			ctx.line.dparam[p] = Fix_mul(ctx.points.vectors[!left_vec].cmd->u.geom.param[p] - ctx.line.param[p], inv_dc);
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

/*
 * Public Function
 */

void draw_line(void)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_POLY, "poly");
	// compute 'decliveness' related paramters in a way similar to poly, so that we can use the JIT
	ctx.poly.decliveness = 0;
	ctx.poly.scan_dir = 0;
	ctx.line.dw = 1;
	ctx.poly.nc_log = ctx.location.buffer_loc[gpuOutBuffer].width_log;
	int32_t m[2];	// 16.16
	m[0] = ctx.points.vectors[1].c2d[0] - ctx.points.vectors[0].c2d[0];
	m[1] = ctx.points.vectors[1].c2d[1] - ctx.points.vectors[0].c2d[1];
	if (Fix_abs(m[0]) < Fix_abs(m[1])) {
		ctx.poly.scan_dir = 1;
		ctx.poly.nc_log = 0;
		SWAP(int32_t, m[0], m[1]);
		ctx.line.dw <<= ctx.location.buffer_loc[gpuOutBuffer].width_log;
	}	
	if (m[0]) ctx.poly.decliveness = (((int64_t)m[1])<<16)/m[0];
	// To fake the rasterizer, we must make it think we are in perspective mode.
	int old_persp = ctx.rendering.mode.named.perspective;
	ctx.rendering.mode.named.perspective = 1;
	// Now that nc_log is known, prepare the rasterizer
#	if defined(GP2X) || defined(TEST_RASTERIZER)
	ctx.rendering.rasterizer = jit_prepare_rasterizer();
#	endif
	draw_scanline();
	ctx.rendering.mode.named.perspective = old_persp;
	perftime_enter(previous_target, NULL);
}
	

