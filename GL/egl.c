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

static struct {
	struct gpuBuf *out;
	struct gpuBuf *depth;
} buffers[3];
static unsigned active_buffer;
bool gli_with_depth_buffer;

/*
 * Private Functions
 */

/*
 * Public Functions
 */

GLboolean glOpen(unsigned attribs)
{
	typedef int (*begin_func)(void);
	gli_with_depth_buffer = attribs & DEPTH_BUFFER;
	static begin_func const begins[] = {
		gli_state_begin,
		gli_arrays_begin,
		gli_transfo_begin,
		gli_colors_begin,
		gli_raster_begin,
		gli_modes_begin,
		gli_framebuf_begin,
		gli_texture_begin,
		gli_cmd_begin,
		gli_begend_begin,
	};
	for (unsigned i=0; i<sizeof_array(begins); i++) {
		if (0 != begins[i]()) {
			return GL_FALSE;
		}
	}
	if (gpuOK != gpuOpen()) {
		return GL_FALSE;
	}
	for (unsigned i=0; i<sizeof_array(buffers); i++) {
		buffers[i].out = gpuAlloc(9, 250, true);
		if (gli_with_depth_buffer) {
			buffers[i].depth = gpuAlloc(9, 250, true);
		} else {
			buffers[i].depth = NULL;
		}
	}
	active_buffer = ~0U;
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
	(void)gli_cmd_end();
	(void)gli_begend_end();
	for (unsigned i=0; i<sizeof_array(buffers); i++) {
		gpuFree(buffers[i].out);
		if (buffers[i].depth) gpuFree(buffers[i].depth);
	}
	gpuClose();
}

GLboolean glSwapBuffers(void)
{
	if (active_buffer == ~0U) {
		active_buffer = 0;
	} else {
		if (gpuOK != gpuShowBuf(buffers[active_buffer].out, true)) {
			return GL_FALSE;
		}
		// wait until this buffer is actually on display
		gpuWaitDisplay();
		if (++ active_buffer >= sizeof_array(buffers)) {
			active_buffer = 0;
		}
	}
	if (gpuOK != gpuSetBuf(gpuOutBuffer, buffers[active_buffer].out, true)) {
		return GL_FALSE;
	}
	if (buffers[active_buffer].depth && gpuOK != gpuSetBuf(gpuZBuffer, buffers[active_buffer].depth, true)) {
		return GL_FALSE;
	}
	return GL_TRUE;
}

