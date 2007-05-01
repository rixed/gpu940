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
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <gpu940.h>
#include <stdlib.h>
#include "fixmath.h"

static uint32_t pascale[256*256];

int main(void) {
	if (gpuOK != gpuOpen()) {
		fprintf(stderr, "Cannot open gpu940.\n");
		return EXIT_FAILURE;
	}
	int fd = open("deftext.tga", O_RDONLY);
	if (-1 == fd) {
		perror("Cannot open deftext");
		return EXIT_FAILURE;
	}
	lseek(fd, 18, SEEK_SET);
	read(fd, pascale, sizeof(pascale));
	for (unsigned p=256*256; p--; ) {	// B, G, R -> R, G, B
		uint8_t (*tga)[3] = (void *)pascale;
		uint8_t r = tga[p][2];
		uint8_t g = tga[p][1];
		uint8_t b = tga[p][0];
		pascale[p] = (r<<16)|(g<<8)|(b);
	}
	close(fd);
	struct gpuBuf *txtBuf = gpuAlloc(8, 256, false);
	if (! txtBuf) {
		fprintf(stderr, "Cannot alloc buf for texture\n");
		return EXIT_FAILURE;
	}
	if (gpuOK != gpuLoadImg(gpuBuf_get_loc(txtBuf), pascale)) {
		fprintf(stderr, "Cannot load texture.\n");
		return EXIT_FAILURE;
	}
	if (gpuOK != gpuSetBuf(gpuTxtBuffer, txtBuf, false)) {
		fprintf(stderr, "Cannot set texture buffer.\n");
		return EXIT_FAILURE;
	}
#define LEN (30<<16)
#define LEN2 (900<<16)
	FixVec vec3d[] = {
		{ .c = { LEN, -LEN, LEN/4 }, .xy = -LEN2 },
		{ .c = { LEN, LEN, LEN/4 }, .xy = LEN2 },
		{ .c = { -LEN, LEN, LEN/4 }, .xy = -LEN2 },
		{ .c = { -LEN, -LEN, LEN/4 }, .xy = LEN2 },
	};
	gpuCmdVector vectors[sizeof_array(vec3d)] = {
		{ .same_as = 0, .u = { .text = { .i=16<<16, .u=0, .v=0, .zb=0 }, }, },
		{ .same_as = 0, .u = { .text = { .i=0<<16, .u=1<<16, .v=0, .zb=0 }, }, },
		{ .same_as = 0, .u = { .text = { .i=16<<16, .u=1<<16, .v=1<<16, .zb=0 }, }, },
		{ .same_as = 0, .u = { .text = { .i=32<<16, .u=0, .v=1<<16, .zb=0 }, }, },
	};
	gpuCmdMode mode = {
		.opcode = gpuMODE,
		.mode = {
			.named = {
				.rendering_type = rendering_text,
				.z_mode = gpu_z_off,
				.use_key = 0,
				.use_intens = 0,
				.perspective = 1,
				.blend_coef = 0,
				.write_out = 1,
				.write_z = 0,
			},
		},
	};
	gpuCmdFacet facet = {
		.opcode = gpuFACET,
		.size = sizeof_array(vectors),
		.color = gpuColor(20, 30, 180),
		.cull_mode = 0,//GPU_CCW,
	};
	gpuCmdRect clear_rect = {
		.opcode = gpuRECT,
		.type = gpuOutBuffer,
		.pos = { 0, 0 },
		.width = SCREEN_WIDTH,
		.height = SCREEN_HEIGHT,
		.relative_to_window = 1,
		.value = gpuColor(0, 0, 0),
	};
	struct iovec cmdvec[] = {
		{ .iov_base = &clear_rect, .iov_len = sizeof(clear_rect) },
		{ .iov_base = &mode, .iov_len = sizeof(mode) },
		{ .iov_base = &facet, .iov_len = sizeof(facet) },
		{ .iov_base = vectors+0, .iov_len = sizeof(*vectors) },
		{ .iov_base = vectors+1, .iov_len = sizeof(*vectors) },
		{ .iov_base = vectors+2, .iov_len = sizeof(*vectors) },
		{ .iov_base = vectors+3, .iov_len = sizeof(*vectors) },
	};
	Fix_trig_init();
	int32_t ang1 = 123;
	int32_t ang2 = 245;
	for ( ; ang1<=100<<16; ang1+=33, ang2+=51) {
		// build matrix
		int32_t c1 = Fix_cos(ang1);
		int32_t s1 = Fix_sin(ang1);
		int32_t c2 = Fix_cos(ang2);
		int32_t s2 = Fix_sin(ang2);
		FixMat mat = {
			.rot = {
				{ c1, 0, -s1 },
				{ ((int64_t)s1*s2)>>16, c2, ((int64_t)c1*s2)>>16 },
				{ ((int64_t)s1*c2)>>16, -s2, ((int64_t)c1*c2)>>16 },
			},
			.ab = { (c1*(int64_t)(((int64_t)s1*s2)>>16))>>16, 0, (-s1*(((int64_t)c1*s2)>>16))>>16 },
			.trans = { 0, 0, 2*LEN, },
		};
		// rotate
		for (unsigned v=0; v<sizeof_array(vec3d); v++) {
			FixMat_x_Vec(vectors[v].u.geom.c3d, &mat, vec3d+v, true);
		}
		struct gpuBuf *outBuf;
		gpuErr err;
		outBuf = gpuAlloc(9, 250, true);
		err = gpuSetBuf(gpuOutBuffer, outBuf, true);
		assert(gpuOK == err);
		err = gpuWritev(cmdvec, sizeof_array(cmdvec), true);
		assert(gpuOK == err);
		err = gpuShowBuf(outBuf, true);
		gpuFreeFC(outBuf, 1);
		assert(gpuOK == err);
		if (shared->error_flags) printf("ERROR: %u\n", shared->error_flags);
//		printf("frame %u, missed %u\n", shared->frame_count, shared->frame_miss);
	}
	gpuFree(txtBuf);
	gpuClose();
	return EXIT_SUCCESS;
}

