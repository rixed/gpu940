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
#include "gpu940.h"

#ifdef GP2X
static uint32_t rgb2yuv(unsigned r, unsigned g, unsigned b) {
	uint32_t y = ((r<<9)+(g<<8)+(b<<8)+(r<<14)+(g<<15)+(b<<11)+(b<<12)+0x108000U)&0xFF0000U;
	uint32_t u = ((b<<7)-(b<<4)-(r<<5)-(r<<2)-(r<<1)-(g<<6)-(g<<3)-(g<<1)+0x8080U)&0xFF00U;
	uint32_t v = ((r<<23)-(r<<20)-(g<<21)-(g<<22)+(g<<17)-(b<<20)-(b<<17)+0x80800000U)&0xFF000000U;
	return (v|y|u|(y>>16));
}
#endif

/*
 * Public Functions
 */

gpuErr gpuLoadImg(struct buffer_loc const *loc, uint8_t (*rgb)[3], unsigned nb_lod) {
	(void)nb_lod;
	for (unsigned c=loc->height<<loc->width_log; c--; ) {
		unsigned b = rgb[c][0];
		unsigned g = rgb[c][1];
		unsigned r = rgb[c][2];
#ifdef GP2X
		shared->buffers[loc->address + c] = rgb2yuv(r, g, b);
#else
		shared->buffers[loc->address + c] = (r<<16)|(g<<8)|b;
#endif
	}
	return gpuOK;
}

