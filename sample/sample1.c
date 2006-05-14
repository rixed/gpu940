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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <gpu940.h>
#include <stdlib.h>
#include <sched.h>
#include "fixmath.h"

uint8_t pascale[256*256][3];

int main(void) {
	if (gpu940_OK != gpu940_open()) {
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
	close(fd);
	if (gpu940_OK != gpu940_load_img(0, pascale)) {
		fprintf(stderr, "Cannot load texture.\n");
		return EXIT_FAILURE;
	}
#define LEN (300<<16)
#define LEN2 (90000<<16)
	FixVec vec3d[] = {
		{ .c = { LEN, -LEN, LEN/4 }, .xy = -LEN2 },
		{ .c = { LEN, LEN, LEN/4 }, .xy = LEN2 },
		{ .c = { -LEN, LEN, LEN/4 }, .xy = -LEN2 },
		{ .c = { -LEN, -LEN, LEN/4 }, .xy = LEN2 },
	};
	gpu940_cmdVector vectors[sizeof_array(vec3d)] = {
		{ .uvi_params = { .u=0, .v=0, .i=16<<16 }, },
		{ .uvi_params = { .u=255<<16, .v=0, .i=0<<16 }, },
		{ .uvi_params = { .u=255<<16, .v=255<<16, .i=16<<16 }, },
		{ .uvi_params = { .u=0, .v=255<<16, .i=32<<16 }, },
/*		{ .rgbi_params = { .r=0, .g=0, .b=255<<16, .i=16<<16 }, },
		{ .rgbi_params = { .r=255<<16, .g= 0, .b=255<<16, .i=0<<16 }, },
		{ .rgbi_params = { .r=255<<16, .g= 255<<16, .b=0, .i=16<<16 }, },
		{ .rgbi_params = { .r=0, .g=255<<16, .b=0, .i=32<<16 }, },*/
	};
	gpu940_cmdFacet facet = {
		.size = sizeof_array(vectors),
		.texture = 0,
		.color = 0x4567,
		.rendering_type = rendering_uv,
	};
	gpu940_cmdSwap swap = { .one = 1, };
	struct iovec cmdvec[1+sizeof_array(vectors)+1] = {
		{ .iov_base = &facet, .iov_len = sizeof(facet) },
		{ .iov_base = vectors+0, .iov_len = sizeof(*vectors) },
		{ .iov_base = vectors+1, .iov_len = sizeof(*vectors) },
		{ .iov_base = vectors+2, .iov_len = sizeof(*vectors) },
		{ .iov_base = vectors+3, .iov_len = sizeof(*vectors) },
		{ .iov_base = &swap, .iov_len = sizeof(swap) },
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
			.trans = { 0, 0, -2*LEN, },
		};
		// rotate
		for (unsigned v=0; v<sizeof_array(vec3d); v++) {
			FixMat_x_Vec(vectors[v].geom.c3d, &mat, vec3d+v, true);
		}
//		if (gpu940_OK != gpu940_writev(cmdvec, sizeof_array(cmdvec))) break;
		while (gpu940_OK != gpu940_writev(cmdvec, sizeof_array(cmdvec))) {
			(void)sched_yield();
		}
	}
	gpu940_close();
	return EXIT_SUCCESS;
}

