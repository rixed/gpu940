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
#else
#	define CMDFILE "/dev/mem"
#endif
#define TOSTRX(X) #X
#define TOSTR(X) TOSTRX(X)
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define MAX_FACET_SIZE 16
#define GPU_DEFAULT_DPROJ 8
#define GPU_DEFAULT_CLIPMIN0 ((-SCREEN_WIDTH>>1)-1)
#define GPU_DEFAULT_CLIPMIN1 ((-SCREEN_HEIGHT>>1)-1)
#define GPU_DEFAULT_CLIPMAX0 ((SCREEN_WIDTH>>1)+1)
#define GPU_DEFAULT_CLIPMAX1 ((SCREEN_HEIGHT>>1)+1)
#define GPU_DEFAULT_WINPOS0 ((512-SCREEN_WIDTH)>>1)
#define GPU_DEFAULT_WINPOS1 3
#define GPU_NB_PARAMS 5 // at most r,g,b,i,z
#define GPU_NB_USER_CLIPPLANES 5
#define GPU_DISPLIST_SIZE 64
#define SHARED_PHYSICAL_ADDR 0x2100000	// this is from 920T or for the video controler.

#ifndef sizeof_array
#	define sizeof_array(x) (sizeof(x)/sizeof(*x))
#endif

typedef enum {	// also used for err_flags
	gpuOK = 0,
	gpuEINT = 1,
	gpuENOSPC = 2,
	gpuEPARAM = 4,
	gpuEPARSE = 8,
	gpuESYS = 16,
	gpuEDLIST = 32,
} gpuErr;

// Types

typedef struct {
	int32_t origin[3];
	int32_t normal[3];	// must be 16.16 normalized
} gpuPlane;

// Commands

extern struct gpuShared {
	uint32_t cmds[0x40000-5];	// 1Mbytes for commands and following volatiles.
	// All integer members are supposed to have the same property as sig_atomic_t.
	volatile int32_t cmds_begin;	// first word beeing actually used by the gpu. let libgpu read in there.
	volatile int32_t cmds_end;	// last word + 1 beeing actually used by the gpu. let libgpu write in there.
	// when cmds_begin == cmds_end, its empty
	volatile uint32_t error_flags;	// use a special swap instruction to read&reset it, as a whole or bit by bit depending of available hardware !
	volatile uint32_t frame_count;
	volatile uint32_t frame_miss;
	uint32_t buffers[0x740000];	// 29Mbytes for buffers
#ifdef GP2X
	uint32_t osd_head[3];
	uint8_t osd_data[SCREEN_WIDTH*SCREEN_HEIGHT/4];
#endif
} *shared;
// there must be enought room after this for the video console, and 64Kb before for code+stack

typedef enum {
	gpuRESET,
	gpuSETVIEW,
	gpuSETUSRCLIPPLANES,
	gpuSETBUF,
	gpuSHOWBUF,
	gpuPOINT,
	gpuLINE,
	gpuFACET,
	gpuRECT,
	gpuZMODE,
} gpuOpcode;

struct buffer_loc {
	uint32_t address;	// in words, from shared->buffers
	uint32_t width_log;	// width in pixels ; must be <= 18
	uint32_t height;
};

typedef struct {
	gpuOpcode opcode;
} gpuCmdReset;

typedef struct {
	gpuOpcode opcode;
	uint32_t dproj;
	int32_t clipMin[2], clipMax[2];	// (0,0) = center of projection
	int32_t winPos[2];	// coordinate of the lower left corner where to draw the window ((0,0) = center of screen)
} gpuCmdSetView;

typedef struct {
	gpuOpcode opcode;
	uint32_t nb_planes;
	gpuPlane planes[GPU_NB_USER_CLIPPLANES];
} gpuCmdSetUserClipPlanes;

typedef enum {
	gpuOutBuffer,
	gpuTxtBuffer,
	gpuZBuffer,
	GPU_NB_BUFFER_TYPES
} gpuBufferType;

typedef struct {
	gpuOpcode opcode;
	gpuBufferType type;
	struct buffer_loc loc;
} gpuCmdSetBuf;

typedef struct {
	gpuOpcode opcode;
	struct buffer_loc loc;
} gpuCmdShowBuf;

enum gpuRenderingType {
	rendering_flat,	// use facet color for flat rendering
	rendering_text,	// use u,v for textured rendering
	rendering_smooth,	// use vectors r,g,b and interpolate
	GPU_NB_RENDERING_TYPES
};
typedef struct {
	gpuOpcode opcode;
	uint32_t size;	// >=3
	uint32_t color;	// used for flat rendering or keyed textures
	uint32_t rendering_type:2;
	uint32_t use_key:1;
	uint32_t use_intens:1;
	uint32_t perspective:1;
	uint32_t blend_coef:4;	// if 0, opaque. If 15, much blend (16 would mean keep current value)
#	define GPU_CW 1
#	define GPU_CCW 2
	uint32_t cull_mode:2;	// bit 0 for direct (counter clock-wise), bit 1 for indirect (clock-wise)
	uint32_t write_out:1;
	uint32_t write_z:1;
} gpuCmdFacet;

typedef struct {
	uint32_t same_as;	// x,y and z are the same that this last vector. 0 means this same as this one, which of course allways stands true.
	// Params are : x,y,z,  r?,g?,b?|u?,v?, i?,zb?
	union {
		// No opcode: it must follow a point/line/facet
		int32_t all_params[3+GPU_NB_PARAMS];
		struct {
			int32_t x, y, z, i_zb, zb;
		} flat_params;
		struct {
			int32_t x, y, z, u, v, i_zb, zb;
		} text_params;
		struct {
			int32_t x, y, z, r, g, b, i_zb, zb;
		} smooth_params;
		struct {
			int32_t c3d[3], param[GPU_NB_PARAMS];
		} geom;
	} u;
} gpuCmdVector;

typedef struct {
	gpuOpcode opcode;
	gpuBufferType type;
	int32_t pos[2];	// coordinate of the lower left corner of the rectangle, in buffer (32.0)
	uint32_t width, height;	// 32.0
	uint32_t relative_to_window:1;	// if set, pos are relative to window, not buffer
	uint32_t value;
} gpuCmdRect;

typedef enum { gpu_z_off, gpu_z_lt, gpu_z_eq, gpu_z_ne, gpu_z_lte, gpu_z_gt, gpu_z_gte } gpuZMode;
typedef struct {
	gpuOpcode opcode;
	gpuZMode mode;
} gpuCmdZMode;

/* Client Functions */

gpuErr gpuOpen(void);
void gpuClose(void);

gpuErr gpuWrite(void *cmd, size_t size, bool can_wait);
gpuErr gpuWritev(const struct iovec *cmdvec, size_t count, bool can_wait);

uint32_t gpuReadErr(void);
gpuErr gpuLoadImg(struct buffer_loc const *loc, uint8_t (*rgb)[3], unsigned lod);
// r, g, b are 16.16 ranging from 0 to 1
static inline int32_t Fix_gpuColor1(int32_t r, int32_t g, int32_t b) {
#ifdef GP2X
	unsigned r_ = r & 0xff00;
	unsigned g_ = g & 0xff00;
	unsigned b_ = b & 0xff00;
	uint32_t y = ((r_<<1)+g_+b_+(r_<<6)+(g_<<7)+(b_<<3)+(b_<<4)+0x108000U)&0xFF0000U;
	return y >> 8;
#else
	(void)g;
	(void)b;
	return r;
#endif
}
static inline int32_t Fix_gpuColor2(int32_t r, int32_t g, int32_t b) {
#ifdef GP2X
	unsigned r_ = r & 0xff00;
	unsigned g_ = g & 0xff00;
	unsigned b_ = b & 0xff00;
	uint32_t u = ((b_>>1)-(b_>>4)-(r_>>3)-(r_>>6)-(r_>>7)-(g_>>2)-(g_>>5)-(g_>>7)+0x8080U)&0xFF00U;
	return u;
#else
	(void)r;
	(void)b;
	return g;
#endif
}
static inline int32_t Fix_gpuColor3(int32_t r, int32_t g, int32_t b) {
#ifdef GP2X
	unsigned r_ = r & 0xff00;
	unsigned g_ = g & 0xff00;
	unsigned b_ = b & 0xff00;
	uint32_t v = ((r_<<15)-(r_<<12)-(g_<<13)-(g_<<14)+(g_<<9)-(b_<<12)-(b_<<9)+0x80800000U)&0xFF000000U;
	return v >> 16;
#else
	(void)r;
	(void)g;
	return b;
#endif
}
static inline uint32_t gpuColor(unsigned r, unsigned g, unsigned b) {
#ifdef GP2X
	// Riped from Rlyeh minilib
	uint32_t y = ((r<<9)+(g<<8)+(b<<8)+(r<<14)+(g<<15)+(b<<11)+(b<<12)+0x108000U)&0xFF0000U;
	uint32_t u = ((b<<7)-(b<<4)-(r<<5)-(r<<2)-(r<<1)-(g<<6)-(g<<3)-(g<<1)+0x8080U)&0xFF00U;
	uint32_t v = ((r<<23)-(r<<20)-(g<<21)-(g<<22)+(g<<17)-(b<<20)-(b<<17)+0x80800000U)&0xFF000000U;
	return (v|y|u|(y>>16));
#else
	return (r<<16)|(g<<8)|(b);
#endif
}

struct gpuBuf *gpuAlloc(unsigned width_log, unsigned height, bool can_wait);	// width is in pixels
void gpuFree(struct gpuBuf *buf);
void gpuFreeFC(struct gpuBuf *buf, unsigned fc);
gpuErr gpuSetBuf(gpuBufferType type, struct gpuBuf *buf, bool can_wait);
gpuErr gpuShowBuf(struct gpuBuf *buf, bool can_wait);
struct buffer_loc const *gpuBuf_get_loc(struct gpuBuf const *buf);
void gpuWaitDisplay(void);

#endif
