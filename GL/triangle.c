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
	cmdFacet.perspective = 0;
	cmdFacet.size = 3;
	return 0;
}

extern inline void gli_triangle_end(void);

void gli_triangle_array(enum gli_DrawMode mode, GLint first, unsigned count)
{
	if (count < 3) return;
	// for now, all in flat shading, copying color at each array
	GLfixed const *c = gli_color_get();
	unsigned r = c[0]>>8;
	unsigned g = c[1]>>8;
	unsigned b = c[2]>>8;
	CLAMP(r, 0, 0xFF);
	CLAMP(g, 0, 0xFF);
	CLAMP(b, 0, 0xFF);	
	cmdFacet.color = gpuColor(r, g, b);
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

void gli_clear(GLclampx colors[4])
{
	// Won't work with user clip planes
	// Better implement a true fill function
	static gpuCmdVector vec_bg[] = {
		{ .geom = { .c3d = { -10<<16, -10<<16, -257 } }, },
		{ .geom = { .c3d = {  10<<16, -10<<16, -257 } }, },
		{ .geom = { .c3d = {  10<<16,  10<<16, -257 } }, },
		{ .geom = { .c3d = { -10<<16,  10<<16, -257 } }, },
	};
	static gpuCmdFacet facet_bg = {
		.opcode = gpuFACET,
		.size = sizeof_array(vec_bg),
		.color = 0,
		.rendering_type = rendering_c,
		.perspective = 0,
	};
	static struct iovec cmdvec[1+4] = {
		{ .iov_base = &facet_bg, .iov_len = sizeof(facet_bg) },	// first facet to clear the background
		{ .iov_base = vec_bg+0, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+1, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+2, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+3, .iov_len = sizeof(*vec_bg) }
	};
	facet_bg.color = gpuColor((colors[0]>>8)&0xFF, (colors[1]>>8)&0xFF, (colors[2]>>8)&0xFF);
	gpuErr err = gpuWritev(cmdvec, sizeof_array(cmdvec), true);
	assert(gpuOK == err);
}
