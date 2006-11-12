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

gpuErr gpuLoadImg(struct buffer_loc const *loc, uint32_t *rgb, unsigned nb_lod) {
	(void)nb_lod;
	for (unsigned c=loc->height<<loc->width_log; c--; ) {
		unsigned r = (rgb[c]>>16) & 0xff;
		unsigned g = (rgb[c]>>8) & 0xff;
		unsigned b = rgb[c] & 0xff;
		shared->buffers[loc->address + c] = gpuColor(r, g, b);
	}
	return gpuOK;
}

