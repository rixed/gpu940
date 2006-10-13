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

static struct gpuBuf *out_buffer[2];
static unsigned active_out_buffer;

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
		gli_triangle_begin,
	};
	for (unsigned i=0; i<sizeof_array(begins); i++) {
		if (0 != begins[i]()) {
			return GL_FALSE;
		}
	}
	if (gpuOK != gpuOpen()) {
		return GL_FALSE;
	}
	out_buffer[0] = gpuAlloc(9, 250, true);
	out_buffer[1] = gpuAlloc(9, 250, true);
	active_out_buffer = ~0U;
	return glSwapBuffers();
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
	(void)gli_triangle_end();
	gpuFree(out_buffer[0]);
	gpuFree(out_buffer[1]);
	gpuClose();
}

GLboolean glSwapBuffers(void)
{
	if (active_out_buffer == ~0U) {
		active_out_buffer = 0;
	} else {
		if (gpuOK != gpuShowBuf(out_buffer[active_out_buffer], true)) {
			return GL_FALSE;
		}
		// wait until this buffer is actually on display
		//gpuWaitDisplay();
		active_out_buffer = !active_out_buffer;
	}
	if (gpuOK != gpuSetOutBuf(out_buffer[active_out_buffer], true)) {
		return GL_FALSE;
	}
	return GL_TRUE;
}

