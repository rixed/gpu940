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

/* TODO: 
 * Optimiser le type de rendu _c pour tracer en X, avec 2 pixels par write, afin d'avoir un memset rapide.
 * Ensuite, embarquer directement le perftime dans shared, et ajouter à la lib les fonctions pour le lire. Et faire
 *   disparaitre la dépendance sur libperftime.
 * Ajouter la possibilité d'avoir un zbuffer (valeurs sur 16 bits dans un buffer distinct, avec un type de rendu distinct, _z).
 *   Ce buffer ne serait pas 'displayable' -> il faut donc que l'utilisateur spécifie, pour chaque rendu, s'il est destiné
 *   au display. Ce serait un argument de la commande swap : dire si le buffer qu'on clos doit être afiché ou pas.
 *   Ensuite, ajouter à chaque routine de rendu un flag pour savoir si on utilise un zbuffer, et lequel (supposément de la même
 *   taille que le nouveau buffer)
 * Ensuite, optimiser ;>
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <gcc.h>
#include <fixmath.h>
#ifndef GP2X
#	define CMDFILE "/tmp/gpu940_buf"
#endif

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
} gpuErr;

#define GPU940_NB_PARAMS 4
#define GPU940_NB_CLIPPLANES (5+2)

// Types

typedef struct {
	int32_t origin[3];
	int32_t normal[3];
} gpuPlane;

// Commands

extern struct gpuShared {
	// All integer members are supposed to have the same property as sig_atomic_t.
	volatile uint32_t int_cmds;	// interrupt like pending commands flag (reset, chop)
	volatile uint32_t error_flags;	// use a special swap instruction to read&reset it, as a whole or bit by bit depending of available hardware !
	volatile int32_t cmds_begin;	// first word beeing actually used by the gpu. let libgpu read in there.
	volatile int32_t cmds_end;	// last word + 1 beeing actually used by the gpu. let libgpu write in there.
	volatile int32_t frame_count;
	uint8_t cmds[0x40000];	// 1Mbytes of asynchronous commands
	uint16_t buffers[0xF00000];	// 30Mbytes of buffers
} *shared;
#define SHARED_PHYSICAL_ADDR 0x2010000	// this is from 920T or for the video controler.

typedef enum {
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
	gpuOpcode opcode;
	struct buffer_loc loc;
	uint32_t dproj;
	int32_t clipMin[2], clipMax[2];	// (0,0) = center of projection
	int32_t winPos[2];	// coordinate of the lower left corner where to draw the window ((0,0) = center of screen)
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
} gpuCmdDrawFacet;

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

gpuErr gpuWrite(void *cmd, size_t size, unsigned syn);
gpuErr gpuWritev(const struct iovec *cmdvec, size_t count, unsigned syn);

uint32_t gpuRead_err(void);
gpuBuffer *gpuAlloc(unsigned width_loc, unsigned height);
gpuErr gpuFree(gpuBuffer *buf);
gpuErr gpuLoad_img(gpuBuffer *buf, uint8_t (*rgb)[3]);

#endif
