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
#include "gli.h"

/*
 * Data Definitions
 */

/*
 * Private Functions
 */

/*
 * Public Functions
 */

GLboolean glOpen(void)
{
	typedef int (*begin_func)(void);
	static begin_func const begins[] = {
		gli_state_begin,
		gli_arrays_begin,
		gli_transfo_begin,
		gli_colors_begin,
		gli_raster_begin,
		gli_modes_begin,
		gli_framebuf_begin,
		gli_texture_begin,
	};
	for (unsigned i=0; i<sizeof_array(begins); i++) {
		if (0 != begins[i]()) {
			return -1;
		}
	}
	return 0;
}

void glClose(void)
{
	(void)gli_state_end();
	(void)gli_arrays_end();
	(void)gli_transfo_end();
	(void)gli_colors_end();
	(void)gli_raster_end();
	(void)gli_modes_end();
	(void)gli_framebuf_end();
	(void)gli_texture_end();
}

