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
 * Public Function
 */

void draw_point(uint32_t color)
{
	int32_t const x = ctx.points.vectors[0].c2d[0] >> 16;
	int32_t const y = ctx.points.vectors[0].c2d[1] >> 16;
	uint32_t *w = ctx.location.out_start + x + (y << ctx.location.buffer_loc[gpuOutBuffer].width_log);
	*w = color;
}

