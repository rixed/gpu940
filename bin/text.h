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
#ifndef GPU940_TEXTURE_H_060409
#define GPU940_TEXTURE_H_060409

static inline uint32_t texture_color(struct buffer_loc const *loc, int32_t u, int32_t v) {
	// height of a texture location must be a power of two
	return shared->buffers[loc->address + ((u>>16)&ctx.location.txt_mask) + (((v>>16)&ctx.location.txt_mask)<<loc->width_log)];
}

#endif
