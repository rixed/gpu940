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
#include <stdbool.h>
#include <assert.h>

/*
 * Data Definitions
 */

static gpuCmdFacet cmdFacet;
static gpuCmdVector cmdVec[3];
static struct iovec const iov_triangle[] = {
	{ .iov_base = &cmdFacet, .iov_len = sizeof(cmdFacet) },
	{ .iov_base = cmdVec+0, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+1, .iov_len = sizeof(*cmdVec) },
	{ .iov_base = cmdVec+2, .iov_len = sizeof(*cmdVec) },
};
static bool facet_is_direct;

/*
 * Private Functions
 */

static void prepare_vertex(GLint arr_idx, unsigned vec_idx)
{
	int32_t c[4];
	gli_vertex_get(arr_idx, c);
	gli_multmatrix(GL_MODELVIEW, cmdVec[vec_idx].geom.c3d, c);
}

static void send_triangle()
{
	gpuErr err = gpuWritev(iov_triangle, sizeof_array(iov_triangle), true);
	assert(gpuOK == err);
}

/*
 * Public Functions
 */

int gli_triangle_begin(void)
{
	cmdFacet.opcode = gpuFACET;
	cmdFacet.size = 3;
	cmdFacet.perspective = 0;
	// TODO: Init a OutBuffer
	return 0;
}

extern inline void gli_triangle_end(void);

void gli_triangle_array(enum gli_DrawMode mode, GLint first, unsigned count)
{
	if (count < 3) return;
	// for now, all in flat shading, copying color at each array
	cmdFacet.color = gli_color_get();
	cmdFacet.rendering_type = rendering_c;
	GLint const last = first + count;
	facet_is_direct = true;
	do {
		prepare_vertex(first++, 0);
		prepare_vertex(first++, 1);
		prepare_vertex(first++, 2);
		send_triangle();
	} while (mode == GL_TRIANGLES && first < last);
	unsigned repl_idx = 0;
	if (mode == GL_TRIANGLE_FAN) repl_idx = 1;
	while (first < last) {
		facet_is_direct = !facet_is_direct;
		prepare_vertex(first++, repl_idx);
		send_triangle();
		if (++ repl_idx > 2) repl_idx = mode == GL_TRIANGLE_FAN ? 1:0;
	}
}
