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
#ifndef GPU940_I_H_051231
#define GPU940_I_H_051231

#include "../perftime/perftime.h"
#include "gpu940.h"
#include "poly.h"
#include "clip.h"
#include "mylib.h"
#include "raster.h"
#ifdef GP2X
#	define assert(x)
#else
#	include <assert.h>
#endif

#define GPU_NB_CLIPPLANES (5+GPU_NB_USER_CLIPPLANES)

#ifdef GP2X
extern volatile uint32_t *gp2x_regs;	// 32-bit version of the MMSP2 registers, from the 940T
#	define gp2x_regs32 gp2x_regs
#	define gp2x_regs16 ((volatile uint16_t *)gp2x_regs)	// don't forgot volatile here or be prepared to strange GCC "optims"
#	define gp2x_regs8 ((volatile uint8_t *)gp2x_regs)	// don't forgot volatile here or be prepared to strange GCC "optims"
#endif

enum {
	PERF_DISPLAY = 1,
	PERF_POLY,
	PERF_POLY_DRAW,
	PERF_DIV,
	PERF_CLIP,
	PERF_WAITCMD,
	PERF_CMD,
};

typedef struct {
	gpuCmdVector cmdVector;
	int32_t c2d[2];
	int32_t h;	// dist with a clipPlane
	int32_t nc_declived;
	uint32_t next;
	uint32_t prev;
	uint32_t clipFlag;	// bit 0..3 indicate that the point is on this plane (do not project corresponding coordinate)
	int32_t used;
} gpuVector;

extern struct ctx {
	// View parameters
	struct {
		int32_t clipMin[2];
		int32_t clipMax[2];
		int32_t winPos[2];	// position of the clipped window in the internal image buffer (0,0 meaning lower left corner in bottom left)
		int32_t winHeight;	// computed from clipMin/Max
		int32_t winWidth;
		gpuPlane clipPlanes[GPU_NB_CLIPPLANES];
		uint32_t nb_clipPlanes;
		uint32_t dproj;
	} view;
	// Buffers
	struct {
		struct buffer_loc buffer_loc[GPU_NB_BUFFER_TYPES];
		uint32_t txt_mask;
	} location;
	// Current Polygon
	struct {
		gpuCmdFacet cmdFacet;
		gpuVector vectors[MAX_FACET_SIZE+2*GPU_NB_CLIPPLANES];
		int32_t nc_declived, nc_declived_next;
		int32_t decliveness;
		uint32_t scan_dir;
		int32_t nc_dir;
		uint32_t first_vector;
		uint32_t nb_params;	// from cmdFacet rendering_type
		uint32_t nc_log;	// used to shift-left dw in line drawing routines.
		uint32_t nb_vectors;	// gives how many vectors are in vectors. For the size of facet, see cmdFacet.size.
		int32_t z_alpha;
		int64_t z_num, z_den;	// 32.32
		int32_t z_dden, z_dnum;	// 16.16
	} poly;
	// Current trapeze
	struct {
		struct {
			int32_t c;	// 16.16
			int32_t dc;	// 16.16
			int32_t param[GPU_NB_PARAMS];	// 16.16. Params are : U, V, Illum
			int32_t param_alpha[GPU_NB_PARAMS];	// 16.16
			uint32_t start_v;
			uint32_t end_v;
			int32_t z_alpha_start, z_alpha_end;
			uint32_t is_growing:1;
		} side[2];	// side dec, side inc
		int32_t dparam[GPU_NB_PARAMS];	// 16.16. Used when dparam are constant for all poly (non-perspective mode)
		uint32_t left_side:1;
		uint32_t is_triangle:1;
	} trap;
	// Current line
	struct {
		int32_t count;
		uint32_t *restrict w;
		int32_t dw;
		int32_t decliv;
		int32_t ddecliv;
		int32_t param[GPU_NB_PARAMS];
		int32_t dparam[GPU_NB_PARAMS];
	} line;
} ctx;

static inline void set_error_flag(unsigned err_mask) {
	shared->error_flags |= err_mask;	// TODO : use a bit atomic set instruction
}
static inline uint32_t *location_pos(gpuBufferType type, int32_t x, int32_t y) {
	return &shared->buffers[
		ctx.location.buffer_loc[type].address +
		(y << ctx.location.buffer_loc[type].width_log) + x
	];
}
static inline uint32_t *location_winPos(gpuBufferType type, int32_t x, int32_t y) {
	return &shared->buffers[
		ctx.location.buffer_loc[type].address +
		((y + ctx.view.winPos[1])<<ctx.location.buffer_loc[type].width_log) +
		x + ctx.view.winPos[0]
	];
}

#endif
