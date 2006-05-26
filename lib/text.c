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

/*
 * Public Functions
 */

extern inline uint32_t gpuColor(unsigned r, unsigned g, unsigned b);

gpuErr gpuLoadImg(struct buffer_loc const *loc, uint8_t (*rgb)[3], unsigned nb_lod) {
	(void)nb_lod;
	for (unsigned c=loc->height<<loc->width_log; c--; ) {
		unsigned b = rgb[c][0];
		unsigned g = rgb[c][1];
		unsigned r = rgb[c][2];
		shared->buffers[loc->address + c] = gpuColor(r, g, b);
	}
	return gpuOK;
}

