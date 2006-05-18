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
#ifndef GPU940_h_051229
#define GPU940_h_051229

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/uio.h>
#include "gcc.h"
#include <fixmath.h>
#ifndef GP2X
#	define CMDFILE "/tmp/gpu940_buf"
#endif
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#ifndef sizeof_array
#	define sizeof_array(x) (sizeof(x)/sizeof(*x))
#endif

#define MAX_FACET_SIZE 16
#define GPU_DEFAULT_DPROJ 8

typedef enum {	// also used for err_flags
	gpuOK = 0,
	gpuEINT = 1,
	gpuENOSPC = 2,
	gpuEPARAM = 4,
	gpuEPARSE = 8,
	gpuESYS = 16,
	gpuEDLIST = 32,
} gpuErr;

#define GPU940_NB_PARAMS 4
#define GPU940_NB_CLIPPLANES (5+2)
#define GPU940_DISPLIST_SIZE 64

// Types

typedef struct {
	int32_t origin[3];
	int32_t normal[3];
} gpuPlane;

// Commands

extern struct gpuShared {
	// All integer members are supposed to have the same property as sig_atomic_t.
	volatile int32_t cmds_begin;	// first word beeing actually used by the gpu. let libgpu read in there.
	volatile int32_t cmds_end;	// last word + 1 beeing actually used by the gpu. let libgpu write in there.
	// when cmds_begin == cmds_end, its empty
	volatile uint32_t error_flags;	// use a special swap instruction to read&reset it, as a whole or bit by bit depending of available hardware !
	volatile uint32_t frame_count;
	volatile uint32_t frame_miss;
	uint32_t cmds[0x10000];	// 1Mbytes for commands
	uint16_t buffers[0xF00000];	// 30Mbytes for buffers
} *shared;
#define SHARED_PHYSICAL_ADDR 0x2010000	// this is from 920T or for the video controler.

typedef enum {
	gpuSETVIEW,
	gpuSETOUTBUF,	// change view parameters
	gpuSETTXTBUF,
	gpuSETZBUF,
	gpuSHOWBUF,	// show the buffer with that name
	gpuPOINT,
	gpuLINE,
	gpuFACET,
} gpuOpcode;

struct buffer_loc {
	uint32_t address;	// in words, from shared->buffers
	uint32_t width_log;	// width in bytes
	uint32_t height;
};

typedef struct {
	uint32_t dproj;
	int32_t clipMin[2], clipMax[2];	// (0,0) = center of projection
	int32_t winPos[2];	// coordinate of the lower left corner where to draw the window ((0,0) = center of screen)
} gpuCmdSetView;

typedef struct {
	gpuOpcode opcode;
	struct buffer_loc loc;
} gpuCmdSetOutBuf;

typedef struct {
	gpuOpcode opcode;
	struct buffer_loc loc;
} gpuCmdSetTxtBuf;

typedef struct {
	gpuOpcode opcode;
	struct buffer_loc loc;
} gpuCmdSetZBuf;

typedef struct {
	gpuOpcode opcode;
	struct buffer_loc loc;
} gpuCmdShowBuf;

typedef struct {
	gpuOpcode opcode;
	uint32_t size;	// >=3
	uint32_t texture;
	uint32_t color;
	enum {
		rendering_c,	// use color for flat rendering
		rendering_ci,	// use color and vectors param i
		rendering_uv,	// use vectors param u, v
		rendering_uvi,	// use vectors param u, v, i
		rendering_rgb,	// use vectors param r, g, b
		rendering_rgbi,	// use vectors param r, g, b, i
		NB_RENDERING_TYPES
	} rendering_type;
} gpuCmdFacet;

typedef union {
	// No opcode: it must follow a point/line/facet
	int32_t all_params[3+GPU940_NB_PARAMS];
	struct {
		int32_t x, y, z, u, v, i;
	} uvi_params;
	struct {
		int32_t x, y, z, r, b, g, i;
	} rgbi_params;
	struct {
		int32_t c3d[3], param[GPU940_NB_PARAMS];
	} geom;
} gpuCmdVector;

/* Client Functions */

typedef struct gpuBuffer gpuBuffer;

gpuErr gpuOpen(void);
void gpuClose(void);

gpuErr gpuWrite(void *cmd, size_t size);
gpuErr gpuWritev(const struct iovec *cmdvec, size_t count);

uint32_t gpuReadErr(void);
gpuErr gpuLoadImg(struct buffer_loc const *loc, uint8_t (*rgb)[3], unsigned lod);

struct gpuBuf *gpuAlloc(unsigned width_log, unsigned height);
void gpuFree(struct gpuBuf *buf);
void gpuFreeFC(struct gpuBuf *buf, unsigned fc);
gpuErr gpuSetOutBuf(struct gpuBuf *buf);
gpuErr gpuSetTxtBuf(struct gpuBuf *buf);
gpuErr gpuSetZBuf(struct gpuBuf *buf);
gpuErr gpuShowBuf(struct gpuBuf *buf);
struct buffer_loc const *gpuBuf_get_loc(struct gpuBuf const *buf);

#endif
