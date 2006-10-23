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
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <gpu940.h>
#include <stdlib.h>
#include "fixmath.h"

static uint8_t img[64*64][3];

static void satadd(uint8_t *v, int d) {
	d = *v + d;
	if (d > 255) *v = 255;
	else *v = d;
}
static void satsub(uint8_t *v, int d) {
	d = *v - d;
	if (d < 0) *v = 0;
	else *v = d;
}

static void pass(int u, int v, int cu, int cv, int dr, int dg, int db) {
	for (int y=64; y--; ) {
		for (int x=64; x--; ) {
			int yy = y-cu;
			int xx = x-cv;
			if (xx*u + yy*v > 0) {
				satadd(img[y*64+x]+0, dr);
				satadd(img[y*64+x]+1, dg);
				satadd(img[y*64+x]+2, db);
			} else {
				satsub(img[y*64+x]+0, dr);
				satsub(img[y*64+x]+1, dg);
				satsub(img[y*64+x]+2, db);
			}
		}
	}
}
static int randu(void) {
	return ((rand()>>5)&255);
}
static int randi(void) {
	return randu()-128;
}
static int randc(void) {
	return ((rand()>>5)&3);
}
static void gen_img(void) {
	memset(img, 128, 64*64*3);
	for (unsigned i=600; i--; ) {
		pass(randi(), randi(), randu(), randu(), randc(), randc(), randc());
	}
}

#define LEN (100<<16)
#define DL (70<<16)
#define LEN2 (10000<<16)
static FixVec vec3d[] = {
	{ .c = {  LEN, -LEN,  LEN+DL }, .xy = -LEN2 },
	{ .c = {  LEN,  LEN,  LEN+DL }, .xy = LEN2 },
	{ .c = { -LEN,  LEN,  LEN+DL }, .xy = -LEN2 },
	{ .c = { -LEN, -LEN,  LEN+DL }, .xy = LEN2 },

	{ .c = {  LEN,  LEN, -LEN+DL }, .xy = LEN2 },
	{ .c = {  LEN,  LEN,  LEN+DL }, .xy = LEN2 },
	{ .c = { -LEN,  LEN,  LEN+DL }, .xy = -LEN2 },
	{ .c = { -LEN,  LEN, -LEN+DL }, .xy = -LEN2 },
	
	{ .c = {  LEN,  LEN, -LEN+DL }, .xy = LEN2 },
	{ .c = {  LEN,  LEN,  LEN+DL }, .xy = LEN2 },
	{ .c = {  LEN, -LEN,  LEN+DL }, .xy = -LEN2 },
	{ .c = {  LEN, -LEN, -LEN+DL }, .xy = -LEN2 },
	
	{ .c = {  LEN, -LEN, -LEN+DL }, .xy = -LEN2 },
	{ .c = {  LEN,  LEN, -LEN+DL }, .xy = LEN2 },
	{ .c = { -LEN,  LEN, -LEN+DL }, .xy = -LEN2 },
	{ .c = { -LEN, -LEN, -LEN+DL }, .xy = LEN2 },

	{ .c = {  LEN, -LEN, -LEN+DL }, .xy = -LEN2 },
	{ .c = {  LEN, -LEN,  LEN+DL }, .xy = -LEN2 },
	{ .c = { -LEN, -LEN,  LEN+DL }, .xy = LEN2 },
	{ .c = { -LEN, -LEN, -LEN+DL }, .xy = LEN2 },
	
	{ .c = { -LEN,  LEN, -LEN+DL }, .xy = -LEN2 },
	{ .c = { -LEN,  LEN,  LEN+DL }, .xy = -LEN2 },
	{ .c = { -LEN, -LEN,  LEN+DL }, .xy = LEN2 },
	{ .c = { -LEN, -LEN, -LEN+DL }, .xy = LEN2 },
};
static gpuCmdVector vectors[sizeof_array(vec3d)] = {
	{ .same_as = 0, .u = { .text_params = { .u=0, .v=0, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=0, .i_zb=0<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=255<<16, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=0, .v=255<<16, .i_zb=250<<16 }, }, },

	{ .same_as = 0, .u = { .text_params = { .u=0, .v=0, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=0, .i_zb=0<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=255<<16, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=0, .v=255<<16, .i_zb=250<<16 }, }, },

	{ .same_as = 0, .u = { .text_params = { .u=0, .v=0, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=0, .i_zb=0<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=255<<16, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=0, .v=255<<16, .i_zb=250<<16 }, }, },
	
	{ .same_as = 0, .u = { .text_params = { .u=0, .v=0, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=0, .i_zb=0<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=255<<16, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=0, .v=255<<16, .i_zb=250<<16 }, }, },

	{ .same_as = 0, .u = { .text_params = { .u=0, .v=0, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=0, .i_zb=0<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=255<<16, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=0, .v=255<<16, .i_zb=250<<16 }, }, },

	{ .same_as = 0, .u = { .text_params = { .u=0, .v=0, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=0, .i_zb=0<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=255<<16, .v=255<<16, .i_zb=125<<16 }, }, },
	{ .same_as = 0, .u = { .text_params = { .u=0, .v=255<<16, .i_zb=250<<16 }, }, },
};

static void send_facet(unsigned v0) {
	static gpuCmdFacet facet = {
		.opcode = gpuFACET,
		.size = 4,
		.rendering_type = rendering_text,
		.use_key = 0,
		.use_intens = 1,
		.perspective = 1,
		.cull_mode = 0,
		.write_out = 1,
		.write_z = 0,
	};
	static struct iovec cmdvec[4+1] = {
		{ .iov_base = &facet, .iov_len = sizeof(facet) },
		{ .iov_len = sizeof(*vectors) },
		{ .iov_len = sizeof(*vectors) },
		{ .iov_len = sizeof(*vectors) },
		{ .iov_len = sizeof(*vectors) },
	};
	cmdvec[1].iov_base = vectors+v0+0;
	cmdvec[2].iov_base = vectors+v0+1;
	cmdvec[3].iov_base = vectors+v0+2;
	cmdvec[4].iov_base = vectors+v0+3;
	gpuErr err = gpuWritev(cmdvec, sizeof_array(cmdvec), true);
	assert(gpuOK == err);
}

int main(void) {
	if (gpuOK != gpuOpen()) {
		fprintf(stderr, "Cannot open gpu940.\n");
		return EXIT_FAILURE;
	}
	gen_img();
	struct gpuBuf *txtGenBuf = gpuAlloc(6, 64, false);
	if (! txtGenBuf) {
		fprintf(stderr, "Cannot alloc buf for texture\n");
		return EXIT_FAILURE;
	}
	if (gpuOK != gpuLoadImg(gpuBuf_get_loc(txtGenBuf), img, 0)) {
		fprintf(stderr, "Cannot load texture.\n");
		return EXIT_FAILURE;
	}
	Fix_trig_init();
	int32_t ang1 = 123;
	int32_t ang2 = 245;
	for ( ; ang1<=100<<16; ang1+=73, ang2+=151) {
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
			.trans = { 0, 0, -LEN/10, },
		};
		// rotate all vecs
		for (unsigned v=0; v<sizeof_array(vectors); v++) {
			FixMat_x_Vec(vectors[v].u.geom.c3d, &mat, vec3d+v, true);
			vectors[v].u.text_params.i_zb = -(vectors[v].u.geom.c3d[2]-mat.trans[2]);
		}
		struct gpuBuf *outBuf;
		gpuErr err;
		outBuf = gpuAlloc(9, 250, true);
		err = gpuSetBuf(gpuOutBuffer, outBuf, true);
		assert(gpuOK == err);
		if (gpuOK != gpuSetBuf(gpuTxtBuffer, txtGenBuf, false)) {
			fprintf(stderr, "Cannot set texture buffer.\n");
			return EXIT_FAILURE;
		}
		for (unsigned f=0; f<6; f++) send_facet(f*4);
		err = gpuShowBuf(outBuf, false);
		assert(gpuOK == err);
	}
	gpuFree(txtGenBuf);
	gpuClose();
	return EXIT_SUCCESS;
}

