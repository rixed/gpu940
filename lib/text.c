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

gpu940Err gpu940_load_texture(unsigned idx, uint8_t (*rgb)[3], unsigned nb_lod) {
	(void)nb_lod;
	if (idx >= 1<<GPU940_NB_MAX_TEXTURES_LOG) return gpu940_EPARAM;
	for (unsigned c=256*256; c--; ) {
		unsigned r = rgb[c][0]>>3;
		unsigned g = rgb[c][1]>>2;
		unsigned b = rgb[c][2]>>3;
		shared->textures[idx][c] = (b<<11)|(g<<5)|r;
	}
	return gpu940_OK;
}


