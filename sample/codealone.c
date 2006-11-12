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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gpu940.h>
#include <fixmath.h>

/*
 * Various Helper Functions
 */

static struct gpuBuf *grab_menu_texture(void) {
	struct gpuBuf *menu_texture = gpuAlloc(9, 512, false);
	assert(menu_texture);
#ifdef GP2X
	int fbdev = open("/dev/fb0", O_RDONLY);
	assert(fbdev != -1);
#	define GP2X_MENU_SIZE 320*240*sizeof(uint16_t)
	uint16_t *menupic = mmap(NULL, GP2X_MENU_SIZE, PROT_READ, MAP_SHARED, fbdev, 0);
	assert(menupic != MAP_FAILED);
	uint32_t *resized = malloc(512*512*sizeof(*resized));
	for (unsigned y = 512; y--; ) {
		for (unsigned x = 512; x--; ) {
			uint16_t c16;
			unsigned ys = y - (512-240)/2, xs = x - (512-320)/2;
			if (y >= (512+240)/2) {
				ys = 239;
			} else if (y < (512-240)/2) {
				ys = 0;
			}
			if (x >= (512+320)/2) {
				xs = 319;
			} else if (x < (512-320)/2) {
				xs = 0;
			}
			c16 = menupic[ys*320+xs];
			resized[((y<<9)+x)*3] = 
				(((c16>>11)<<3) & 0xff) ||
				(((c16>>5)&0x3f)<<10) & 0xff00 ||
				((c16&0x1f)<<19) & 0xff0000;
		}
	}
	(void)munmap(menupic, GP2X_MENU_SIZE);
	(void)close(fbdev);
	if (gpuOK != gpuLoadImg(gpuBuf_get_loc(menu_texture), resized, 0)) {
		assert(0);
	}
	(void)free(resized);
#	undef GP2X_MENU_TEXTURE
#endif
	return menu_texture;
}

static struct gpuBuf *outBuf;
static void next_out_buf(void) {
	outBuf = gpuAlloc(9, 250, true);
	gpuErr err = gpuSetBuf(gpuOutBuffer, outBuf, true);
	assert(gpuOK == err);
}

static void show_out_buf(void) {
	gpuErr err = gpuShowBuf(outBuf, true);
	assert(gpuOK == err);
}

static void set_dproj(unsigned dproj) {
	static gpuCmdSetView setView = {
		.opcode = gpuSETVIEW,
		.clipMin = { GPU_DEFAULT_CLIPMIN0, GPU_DEFAULT_CLIPMIN1 },
		.clipMax = { GPU_DEFAULT_CLIPMAX0, GPU_DEFAULT_CLIPMAX1 },
		.winPos = { GPU_DEFAULT_WINPOS0, GPU_DEFAULT_WINPOS1 }
	};
	setView.dproj = dproj;
	gpuErr err = gpuWrite(&setView, sizeof(setView), true);
	assert(gpuOK == err);
}


/*
 * Camera
 */

#define LEN 2	// characteristic length

struct target {
	int32_t pos[3];
	int32_t lookat[3];
	int32_t Xy;
	int nb_steps;
};

static struct {
	FixMat transf;	// from world to camera (include camera's position)
	int32_t pos[3];
	struct target *target;
	int32_t acc_pos[3], acc_lookat[3], acc_Xy;
} camera = {
	.transf = {
		.rot = { { 1<<16,0,0 }, { 0,1<<16,0 }, { 0,0,1<<16 } },
		.ab = { 0, 0, 0 },
		.trans = { 0, 0, 0 },
	},
	.pos = { 0, 0, 2*LEN<<16 },
	.target = NULL,
	.acc_pos = { 0, 0, 0 },
	.acc_lookat = { 0, 0, 0 },
	.acc_Xy = 0,
};

static void closer_coord(int32_t *pos, int32_t target, int nb_steps, int32_t *acc) {
	if (nb_steps < 1) return;
	int32_t dep = (target - *pos)/nb_steps;
	*acc = (((int64_t)*acc<<4)+dep)/17;
	*pos += *acc;
}

static void normalize(int32_t *x, int32_t *y, int32_t *z) {
	int32_t norm = ( ((int64_t)*x**x) + ((int64_t)*y**y) + ((int64_t)*z**z)) >> 16;
	norm = Fix_sqrt(norm);
	*x = ((int64_t)*x<<16)/norm;
	*y = ((int64_t)*y<<16)/norm;
	*z = ((int64_t)*z<<16)/norm;
}

static void camera_move() {
	int32_t lookat[3] = {
		camera.target->lookat[0]-camera.pos[0],
		camera.target->lookat[1]-camera.pos[1],
		camera.target->lookat[2]-camera.pos[2]
	};
	normalize(&lookat[0], &lookat[1], &lookat[2]);
	for (unsigned c=3; c--; ) {
		closer_coord(&camera.pos[c], camera.target->pos[c], camera.target->nb_steps, &camera.acc_pos[c]);
		closer_coord(&camera.transf.rot[c][2], -lookat[c], camera.target->nb_steps, &camera.acc_lookat[c]);	// Z must points the other way round
	}
	closer_coord(&camera.transf.rot[1][0], camera.target->Xy, camera.target->nb_steps, &camera.acc_Xy);
	// renormalize Z
	normalize(&camera.transf.rot[0][2], &camera.transf.rot[1][2], &camera.transf.rot[2][2]);
	// re-orthogonalize
	int32_t norm = (	// compute X.Z
		((int64_t)camera.transf.rot[0][0]*camera.transf.rot[0][2]) +
		((int64_t)camera.transf.rot[1][0]*camera.transf.rot[1][2]) +
		((int64_t)camera.transf.rot[2][0]*camera.transf.rot[2][2])
	) >> 16;
	for (unsigned c=3; c--; ) {	// remove norm*Z from X
		camera.transf.rot[c][0] -= ((int64_t)camera.transf.rot[c][2]*norm)>>16;
	}
	// renormalize X
	normalize(&camera.transf.rot[0][0], &camera.transf.rot[1][0], &camera.transf.rot[2][0]);
	// now rebuild Y from Z and X
	for (unsigned c=3; c--; ) {
		camera.transf.rot[c][1] = (
			((int64_t)camera.transf.rot[(c+1)%3][2]*camera.transf.rot[(c+2)%3][0]) -
			((int64_t)camera.transf.rot[(c+2)%3][2]*camera.transf.rot[(c+1)%3][0])
		) >> 16;
	}
	// recompute ab
	for (unsigned c=3; c--; ) {
		camera.transf.ab[c] = ((int64_t)camera.transf.rot[0][c]*camera.transf.rot[1][c]) >> 16;
	}
	// Once matrix is fixed, compute trans from pos
	FixVec mPos = { { -camera.pos[0], -camera.pos[1], -camera.pos[2] }, ((int64_t)camera.pos[0]*camera.pos[1])>>16 };
	FixMat_x_Vec(camera.transf.trans, &camera.transf, &mPos, false);
}

/*
 * The Geometry
 */

#define LEN2 (LEN*LEN)
static FixVec cube_vec[8] = {
	{ .c = {  LEN<<16, -LEN<<16,  LEN<<16 }, .xy = -LEN2<<16 },
	{ .c = {  LEN<<16,  LEN<<16,  LEN<<16 }, .xy = LEN2<<16 },
	{ .c = { -LEN<<16,  LEN<<16,  LEN<<16 }, .xy = -LEN2<<16 },
	{ .c = { -LEN<<16, -LEN<<16,  LEN<<16 }, .xy = LEN2<<16 },
	{ .c = {  LEN<<16, -LEN<<16, -LEN<<16 }, .xy = -LEN2<<16 },
	{ .c = {  LEN<<16,  LEN<<16, -LEN<<16 }, .xy = LEN2<<16 },
	{ .c = { -LEN<<16,  LEN<<16, -LEN<<16 }, .xy = -LEN2<<16 },
	{ .c = { -LEN<<16, -LEN<<16, -LEN<<16 }, .xy = LEN2<<16 },
};
static int32_t c3d[8][3];
static gpuCmdFacet cube_cmdFacet = {
	.opcode = gpuFACET,
	.size = 4,
	.color = 0xF08080,
	.rendering_type = rendering_text,
	.use_key = 0,
	.use_intens = 1,
	.blend_coef = 0,
	.perspective = 1,
	.cull_mode = 0,
	.write_out = 1,
	.write_z = 0,
};
static unsigned cube_facet[6][4] = {
	{ 0, 4, 5, 1 },
	{ 5, 6, 2, 1 },
	{ 7, 3, 2, 6 },
	{ 7, 4, 0, 3 },
	{ 3, 0, 1, 2 },
	{ 4, 7, 6, 5 }
};
static struct {
	unsigned notnull;
	FixVec normal;
} cube_normal[6] = {
	{ 0, { .c = { -1<<16,  0<<16,  0<<16 }, .xy = 0 } },
	{ 1, { .c = {  0<<16, -1<<16,  0<<16 }, .xy = 0 } },
	{ 0, { .c = { +1<<16,  0<<16,  0<<16 }, .xy = 0 } },
	{ 1, { .c = {  0<<16, +1<<16,  0<<16 }, .xy = 0 } },
	{ 2, { .c = {  0<<16,  0<<16, -1<<16 }, .xy = 0 } },
	{ 2, { .c = {  0<<16,  0<<16, +1<<16 }, .xy = 0 } }
};
static int32_t uvs[6][4][2] = {
	{	// facet 0
		{   0<<16,   0<<16 },
		{ 256<<16,   0<<16 },
		{ 256<<16, 256<<16 },
		{   0<<16, 256<<16 }
	}, {	// facet 1
		{   0<<16,   0<<16 },
		{ 256<<16,   0<<16 },
		{ 256<<16, 256<<16 },
		{   0<<16, 256<<16 }
	}, {	// facet 2
		{   0<<16,   0<<16 },
		{ 256<<16,   0<<16 },
		{ 256<<16, 256<<16 },
		{   0<<16, 256<<16 }
	}, {	// facet 3
		{   0<<16,   0<<16 },
		{ 256<<16,   0<<16 },
		{ 256<<16, 256<<16 },
		{   0<<16, 256<<16 }
	}, {	// facet 4
		{   0<<16,   0<<16 },
		{ 512<<16,   0<<16 },
		{ 512<<16, 512<<16 },
		{   0<<16, 512<<16 }
	}, {	// facet 5
		{   0<<16,   0<<16 },
		{ 256<<16,   0<<16 },
		{ 256<<16, 256<<16 },
		{   0<<16, 256<<16 }
	}
};
static int facet_intens[6] = { 1,1,1,1,1,1 };
static struct gpuBuf *facet_text[6];

static void draw_facet(unsigned f, bool ext, int32_t i_dec) {
	assert(f < sizeof_array(cube_facet));
	int32_t normal[3] = { 0, 0, 0 };
	static gpuCmdVector cmdVec[4];
	for (unsigned v=4; v--; ) {
		for (unsigned c=3; c--; ) {
			cmdVec[v].same_as = 0,
			cmdVec[v].u.geom.c3d[c] = c3d[ cube_facet[f][v] ][c];
			normal[c] += cmdVec[v].u.geom.c3d[c] - camera.transf.trans[c];
		}
		cmdVec[v].u.text_params.u = uvs[f][v][0];
		cmdVec[v].u.text_params.v = uvs[f][v][1];
		cmdVec[v].u.text_params.i_zb = (i_dec+c3d[ cube_facet[f][v] ][2])<<6;
	}
	Fix_normalize(normal);
	int64_t scal = Fix_scalar(normal, c3d[ cube_facet[f][0] ]);
	// is the facet backward ?
	if (ext && scal > 0) return;
	if (!ext && scal < 0) return;
	if (gpuOK != gpuSetBuf(gpuTxtBuffer, facet_text[f], true)) {
		assert(0);
	}
	cube_cmdFacet.use_intens = facet_intens[f];
	static struct iovec cmdvec[4+1] = {
		{ .iov_base = &cube_cmdFacet, .iov_len = sizeof(cube_cmdFacet) },
		{ .iov_base = cmdVec+0, .iov_len = sizeof(*cmdVec) },
		{ .iov_base = cmdVec+1, .iov_len = sizeof(*cmdVec) },
		{ .iov_base = cmdVec+2, .iov_len = sizeof(*cmdVec) },
		{ .iov_base = cmdVec+3, .iov_len = sizeof(*cmdVec) },
	};
	gpuErr err = gpuWritev(cmdvec, sizeof_array(cmdvec), true);
	assert(gpuOK == err);
}

static void draw_cube(bool ext, int32_t i_dec) {
	for (unsigned f=6; f--; ) {
		draw_facet(f, ext, i_dec);
	}
}

static void clear_screen(void) {
	static gpuCmdRect clear_rect = {
		.opcode = gpuRECT,
		.type = gpuOutBuffer,
		.pos = { 0, 0 },
		.width = SCREEN_WIDTH,
		.height = SCREEN_HEIGHT,
		.relative_to_window = 1,
		.value = 0,
	};
	clear_rect.value = gpuColor(0, 0, 0);
	gpuErr err = gpuWrite(&clear_rect, sizeof(clear_rect), true);
	assert(gpuOK == err);
}

static void transform_cube(void) {
	// rotate all vecs
	for (unsigned v=sizeof_array(cube_vec); v--; ) {
		FixMat_x_Vec(c3d[v], &camera.transf, cube_vec+v, true);
	}
}

/*
 * Scene 1
 */

#include "pics.h"

static struct target targets_scene1[] = {
	{
		.pos = { 0, 0, (2*LEN<<16)+12000 },	// first facet (grabed gp2x menu)
		.lookat = { 0, 0, LEN<<16 },
		.Xy = 0,
		.nb_steps = 50,
	}, {
		.pos = { 30000, 5000, (2*LEN<<16)+35000 },
		.lookat = { 0, 0, LEN<<16 },
		.Xy = 30000,
		.nb_steps = 50,
	}, {
		.pos = { -6000, -5000, (2*LEN<<16)-20000 },
		.lookat = { 600, 100, LEN<<15 },
		.Xy = -20000,
		.nb_steps = 50,
	}, {
		.pos = { 2*LEN<<16, 5000, 2*LEN<<16 },	// second facet (title)
		.lookat = { 0, 0, 0 },
		.Xy = -3000,
		.nb_steps = 100,
	}, {
		.pos = { 3*LEN<<16, 5000, 0 },
		.lookat = { 1*LEN<<16, 0, 0 },
		.Xy = 3000,
		.nb_steps = 100,
	}, {
		.pos = { (2*LEN<<16)+50000, -20000, 30000 },
		.lookat = { 1*LEN<<16, 0, 0 },
		.Xy = 0,
		.nb_steps = 90,
	}, {
		.pos = { (2*LEN<<16)+60000, 30000, 0 },
		.lookat = { 1*LEN<<16, 0, 0 },
		.Xy = -3000,
		.nb_steps = 70,
	}, {
		.pos = { 2*LEN<<16, 20000, -2*LEN<<16 },
		.lookat = { 1*LEN<<16, 0, 0 },
		.Xy = 10000,
		.nb_steps = 100,
	}, {
		.pos = { 0, 0, (-3*LEN<<16) },	// third facet (greetz)
		.lookat = { 0, 0, 0 },
		.Xy = 0,
		.nb_steps = 70,
	}, {
		.pos = { 1*LEN<<16, 10000, (-3*LEN<<16) },
		.lookat = { 0, 0, -LEN<<16 },
		.Xy = 30000,
		.nb_steps = 100,
	}, {
		.pos = { -1*LEN<<16, -10000, (-3*LEN<<16) },
		.lookat = { 0, 0, -LEN<<16 },
		.Xy = -30000,
		.nb_steps = 100,
	}, {
		.pos = { -3*LEN<<16, 0, 0 },	// fourth facet (though)
		.lookat = { 0, 0, 0 },
		.Xy = 0,
		.nb_steps = 200,
	}, {
		.pos = { -4*LEN<<16, -40000, 10000 },
		.lookat = { -LEN<<16, 0, 0 },
		.Xy = 1000,
		.nb_steps = 200,
	}, {
		.pos = { 0 /*(-1*LEN<<16)+50000*/, -20000, 15000 },
		.lookat = { 0, 0, 0 },
		.Xy = -10000,
		.nb_steps = 20,
	}, {
		.pos = { (-2*LEN<<16)-20000, -20000, 15000 },
		.lookat = { 0, 0, 0 },
		.Xy = -8000,
		.nb_steps = 20,
	}, {
		.pos = { (-2*LEN<<16)-30000, -10000, -5000 },
		.lookat = { 0, 0, 0 },
		.Xy = 38000,
		.nb_steps = 10,
	}, {
		.pos = { (-2*LEN<<16)-50000, -60000, -3000 },
		.lookat = { 0, 0, 0 },
		.Xy = -38000,
		.nb_steps = 20,
	}, {
		.pos = { (-2*LEN<<16)-60000, -50000, -2000 },
		.lookat = { 0, 0, 0 },
		.Xy = 40000,
		.nb_steps = 10,
	}, {
		.pos = { (-2*LEN<<16)-70000, -40000, -1000 },
		.lookat = { 0, 0, 0 },
		.Xy = -40000,
		.nb_steps = 10,
	}, {
		.pos = { (-2*LEN<<16)-80000, -30000, 0 },
		.lookat = { 0, 0, 0 },
		.Xy = 30000,
		.nb_steps = 10,
	}, {
		.pos = { (-2*LEN<<16)-90000, -20000, 0 },
		.lookat = { 0, 0, 0 },
		.Xy = -20000,
		.nb_steps = 10,
	}, {
		.pos = { (-2*LEN<<16)-100000, -10000, 0 },
		.lookat = { 0, 0, 0 },
		.Xy = 10000,
		.nb_steps = 10,
	}, {
		.pos = { (-5*LEN<<16), 0, 0 },
		.lookat = { 0, 0, 0 },
		.Xy = 0,
		.nb_steps = 200,
	}, {
		.pos = { 0, 0, 0 },
		.lookat = { 0, 0, 0 },
		.Xy = -50000,
		.nb_steps = 15,
	}
};

static struct dists {
	uint16_t dx2, dy2, dx, dy;
} (*dists)[256][256];

static uint32_t distance(struct dists *dist) {
	return (uint32_t)dist->dx2 + dist->dy2;
}

static void minimize_dist_x(struct dists *restrict dist, struct dists *restrict dist2) {
	uint32_t min_dist = distance(dist);
	uint32_t odist = distance(dist2);
	if (odist < min_dist) {
		dist->dx = dist2->dx + 1;
		dist->dy = dist2->dy;
		dist->dx2 = dist2->dx2 + dist->dx;
		dist->dy2 = dist2->dy2;
	}
}

static void minimize_dist_y(struct dists *restrict dist, struct dists *restrict dist2) {
	uint32_t min_dist = distance(dist);
	uint32_t odist = distance(dist2);
	if (odist < min_dist) {
		dist->dx = dist2->dx;
		dist->dy = dist2->dy + 1;
		dist->dx2 = dist2->dx2;
		dist->dy2 = dist2->dy2 + dist->dy;
	}
}

static void calc_dists(void) {
	for (int y=0; y<256; y++) {
		for (int x=1; x<256; x++) {
			minimize_dist_x(&(*dists)[y][x], &(*dists)[y][x-1]);
		}
	}
	for (int x=255; x>=0; x--) {
		for (int y=254; y>=0; y--) {
			minimize_dist_y(&(*dists)[y][x], &(*dists)[y+1][x]);
		}
	}
	for (int y=0; y<256; y++) {
		for (int x=254; x>=0; x--) {
			minimize_dist_x(&(*dists)[y][x], &(*dists)[y][x+1]);
		}
	}
	for (int x=255; x>=0; x--) {
		for (int y=1; y<256; y++) {
			minimize_dist_y(&(*dists)[y][x], &(*dists)[y-1][x]);
		}
	}
}

static struct gpuBuf *uncomp(uint8_t *data, uint32_t thresold, uint32_t col1, uint32_t col2) {
	struct gpuBuf *txt = gpuAlloc(8, 256, true);
	assert(txt);
	dists = malloc(sizeof(*dists));
	assert(dists);
	memset(dists, 0xffff, sizeof(*dists));
	for (unsigned y=0; y<256; y++) {
		int col = 0;
		unsigned count = 0;
		unsigned x = 0;
		do {
			count = *(data++);
			if (col) while (count > 0) {
				(*dists)[y][x].dx = (*dists)[y][x].dy = (*dists)[y][x].dx2 = (*dists)[y][x].dy2 = 0;
				count --;
				x ++;
			} else {
				x += count;
				count = 0;
			}
			col = !col;
		} while (x < 255);
		assert(x == 255);
	}
	calc_dists();
	calc_dists();
	for (unsigned y=0; y<256; y++) {
		uint32_t *p = &shared->buffers[gpuBuf_get_loc(txt)->address + y*256];
		for (unsigned x=0; x<256; x++) {
			uint32_t d = distance(&(*dists)[y][x]);
			if (d < thresold) {
				*p = col1;
			} else if (d < thresold*8) {
				*p = col2;
			} else if (d < thresold*8+20) {
				*p = gpuColor(0x0, 0x0, 0x0);
			} else {
				*p = col1;
			}
			p ++;
		}
	} 
	free(dists);
	return txt;
}

static void scene1(void) {
	unsigned target = 0;
	camera.target = &targets_scene1[target];
	facet_text[4] = grab_menu_texture();
	facet_intens[4] = 0;	// all others stay with intensity enabled
	facet_text[0] = uncomp(pic1, 24, gpuColor(180, 180, 160), gpuColor(100, 150, 200));
	facet_text[5] = uncomp(pic2, 10, gpuColor(123, 50, 200), gpuColor(30, 200, 200));
	facet_text[2] = uncomp(pic3, 10, gpuColor(180, 180, 200), gpuColor(200, 70, 230));
	do {
		// TODO : faire osciller les coordonnees de mapping pour les textures autres que menu
		camera_move();
		next_out_buf();
		clear_screen();
		transform_cube();
		draw_cube(true, 2*LEN<<16);
		show_out_buf();
		if (! --camera.target->nb_steps) {
			if (++target >= sizeof_array(targets_scene1)) break;
			camera.target = &targets_scene1[target];
			if (14 == target) {
				camera.acc_pos[0] = camera.acc_pos[1] = camera.acc_pos[2] = 0;
			}
		}
	} while (1);
	gpuFreeFC(facet_text[4], 1);
	gpuFreeFC(facet_text[0], 1);
	gpuFreeFC(facet_text[5], 1);
	gpuFreeFC(facet_text[2], 1);
	facet_intens[4] = 1;	// restore default
}

/*
 * Scene 2
 */

#define DROP_DIAMETER 128
static uint8_t ink_drop[DROP_DIAMETER][DROP_DIAMETER];

static void draw_drop(void) {
	memset(ink_drop, 0, sizeof(ink_drop));
	unsigned x, y;
	unsigned thresold = DROP_DIAMETER*DROP_DIAMETER / 36;
	for (y=0; y<DROP_DIAMETER; y++) {
		int dy = DROP_DIAMETER/2 - y;
		unsigned dy2 = dy*dy;
		for (x=0; x<DROP_DIAMETER; x++) {
			int dx = DROP_DIAMETER/2 - x;
			unsigned dx2 = dx*dx;
			unsigned d2 = dy2 + dx2;
			if (d2 < thresold) {
				ink_drop[y][x] = 255;
			} else {
				unsigned ang = 2*my_sqrt(d2 - thresold);
				if (ang < DROP_DIAMETER) {
					int32_t c = 0x80+(Fix_cos((ang*32768)/DROP_DIAMETER)>>9);
					ink_drop[y][x] = c<0 ? 0 : c>255 ? 255 : c;
				} else {
					ink_drop[y][x] = 0;
				}
			}
		}
	}
}

static void add_drop(uint8_t (*map)[256][256], unsigned x, unsigned y) {
	unsigned dx, dy;
	for (dy=0; dy<DROP_DIAMETER; dy++) {
		for (dx=0; dx<DROP_DIAMETER; dx++) {
			unsigned i = (*map)[(y+dy)&0xff][(x+dx)&0xff] + ink_drop[dy][dx];
			(*map)[(y+dy)&0xff][(x+dx)&0xff] = i > 255 ? 255 : i;
		}
	}
}

static void splash_colors(uint8_t (*map)[256][256], unsigned nb_drops) {
	draw_drop();
	memset(map, 0, sizeof(*map));
	for (unsigned s=0; s<nb_drops; s++) {
		int r = rand();
		int sx = (r>>8);
		int sy = (r>>16);
		add_drop(map, sx, sy);
	}
}

static void ink2text(uint32_t *text, uint8_t (*map)[256][256]) {
	static int a1, a2, a3, a4, a5;
	unsigned ax = (41*Fix_sin(a1))>>16;
	a1 += 399;
	unsigned ay = (18*Fix_cos(a2))>>16;
	a2 += 141;
	unsigned bx = (21*Fix_sin(a3))>>16;
	a3 += 197;
	unsigned by = (28*Fix_cos(a4))>>16;
	a4 += 451;
	unsigned thresold = 80 + ((70*Fix_sin(a5))>>16);
	a5 += 753;
	unsigned thresold2 = thresold + 90;
	uint32_t col1 = gpuColor(0xc0, 0xc0, 0xb0);
	uint32_t col2 = gpuColor(0x80, 0xa8, 0xb0);
	for (unsigned y=0; y<256; y++) {
		for (unsigned x=0; x<256; x++) {
			unsigned v = (unsigned)(*map)[(y+by)&0xff][(x+bx)&0xff]+(unsigned)(*map)[(ay-y)&0xff][(ax-x)&0xff];
			if (v < thresold) {
				*text = col1;
			} else if (v < thresold2) {
				*text = col2;
			} else {
				*text = col1;
			}
			text ++;
		}
	}
}

static struct target targets_scene2[] = {
	{
		.pos = { 0, 0, 12000 },	// first facet (grabed gp2x menu)
		.lookat = { 0, 0, 0 },
		.Xy = 1000,
		.nb_steps = 150,
	}, {
		.pos = { 300, 45000, 5000 },
		.lookat = { 0, 0, 0 },
		.Xy = -30000,
		.nb_steps = 150,
	}, {
		.pos = { 300, -35000, 45000 },
		.lookat = { 0, 0, 0 },
		.Xy = 20000,
		.nb_steps = 150,
	}
};

static void locate_pic(FixVec *pic_vec, int32_t ang1, int32_t ang2) {
	int32_t c1 = Fix_cos(ang1);
	int32_t s1 = Fix_sin(ang1);
	int32_t c2 = Fix_cos(ang2);
	int32_t s2 = Fix_sin(ang2);
#	define R 60000
	static FixVec vec[4] = {
		{ .c = { -1<<15, -1<<15, -R }, .xy =  1<<14 },
		{ .c = {  1<<15, -1<<15, -R }, .xy = -1<<14 },
		{ .c = {  1<<15,  1<<15, -R }, .xy =  1<<14 },
		{ .c = { -1<<15,  1<<15, -R }, .xy = -1<<14 }
	};
	FixMat m = {
		.rot = {
			{ c2, -((int64_t)s1*s2)>>16, -((int64_t)c1*s2)>>16 },
			{ 0, c1, -s1 },
			{ s2, ((int64_t)s1*c2)>>16, ((int64_t)c1*c2)>>16 },
		},
	};
	for (unsigned i=0; i<3; i++) {
		m.ab[i] = ((int64_t)m.rot[0][i]*m.rot[1][i])>>16;
	}
	for (unsigned v=0; v<4; v++) {
		FixMat_x_Vec(pic_vec[v].c, &m, vec+v, false);
		pic_vec[v].xy = ((int64_t)pic_vec[v].c[0]*pic_vec[v].c[1])>>16;
	}
#	undef R
}

enum draw_what { PIC, SHADOWS };
static void transf_draw_pic(struct gpuBuf *pic_txt, FixVec *pic_vec, enum draw_what draw_what) {
	static gpuCmdFacet pic_facet = {
		.opcode = gpuFACET,
		.size = 4,
		.rendering_type = rendering_text,
		.use_key = 1,
		.use_intens = 0,
		.blend_coef = 0,
		.cull_mode = 0,
		.write_out = 1,
		.write_z = 0,
	};
	pic_facet.color = gpuColor(0, 0, 255);	// key color of the texture
	static gpuCmdVector vecs[4] = {
		{ .same_as = 0, .u = { .text_params = { .u =   0<<16, .v =   0<<16 } }, },
		{ .same_as = 0, .u = { .text_params = { .u = 255<<16, .v =   0<<16 } }, },
		{ .same_as = 0, .u = { .text_params = { .u = 255<<16, .v = 255<<16 } }, },
		{ .same_as = 0, .u = { .text_params = { .u =   0<<16, .v = 255<<16 } }, }
	};
	static struct iovec cmdvec[4+1] = {
		{ .iov_base = &pic_facet, .iov_len = sizeof(pic_facet) },
		{ .iov_base = vecs+0, .iov_len = sizeof(*vecs) },
		{ .iov_base = vecs+1, .iov_len = sizeof(*vecs) },
		{ .iov_base = vecs+2, .iov_len = sizeof(*vecs) },
		{ .iov_base = vecs+3, .iov_len = sizeof(*vecs) },
	};
	if (gpuOK != gpuSetBuf(gpuTxtBuffer, pic_txt, true)) {
		assert(0);
	}
	if (draw_what == PIC) {	// Simply rotate and draw
		pic_facet.perspective = 1;
		pic_facet.blend_coef = 0;
		for (unsigned v=4; v--; ) {
		//	FixMat_x_Vec(vecs[v].geom.c3d, &camera.transf, pic_vec+v, true);
			for (unsigned c=0; c<3; c++)
				vecs[v].u.geom.c3d[c] = pic_vec[v].c[c];
		}
		gpuErr err = gpuWritev(cmdvec, sizeof_array(cmdvec), true);
		assert(gpuOK == err);
	} else {	// SHADOWS
		pic_facet.blend_coef = 8;
		pic_facet.perspective = 0;
		int32_t L[3] = {
			camera.pos[0] + camera.transf.rot[0][2] + camera.transf.rot[0][1],
			camera.pos[1] + camera.transf.rot[1][2] + camera.transf.rot[1][1],
			camera.pos[2] + camera.transf.rot[2][2] + camera.transf.rot[2][1]
		};
		static gpuCmdSetUserClipPlanes setCpCmd = {
			.opcode = gpuSETUSRCLIPPLANES,
		};
		// we need pic coordinate in world basis
		int32_t abs_vecs[4][3];
		for (unsigned v=4; v--; ) {
			FixMatT_x_Vec(abs_vecs[v], &camera.transf, pic_vec[v].c, true);
		}
		setCpCmd.nb_planes = 5;
		for (unsigned f=0; f<sizeof_array(cube_facet); f++) {
			unsigned cp = 0;
			for (unsigned ff=0; ff<sizeof_array(cube_facet); ff++) {
				if (ff == f) continue;
				FixMat_x_Vec(setCpCmd.planes[cp].origin, &camera.transf, &cube_vec[cube_facet[ff][0]], true);
				FixMat_x_Vec(setCpCmd.planes[cp].normal, &camera.transf, &cube_normal[ff].normal, false);
				cp ++;
			}
			for (unsigned v=0; v<4; v++) {
				FixVec M;
				unsigned c = cube_normal[f].notnull;
				if (Fix_same_sign(cube_normal[f].normal.c[c], abs_vecs[v][c]-L[c])) {
					goto skip_shad;
				}
				M.c[c] = cube_vec[cube_facet[f][0]].c[c];
				for (unsigned i = 1; i < sizeof_array(M.c); i ++) {
					unsigned c_prev = c;
					if (++c >= sizeof_array(M.c)) c = 0;
					int64_t a = ((int64_t)L[c]-abs_vecs[v][c])*(M.c[c_prev]-abs_vecs[v][c_prev]);
					int32_t l_v = (L[c_prev]-abs_vecs[v][c_prev]);
					if (! l_v) goto skip_shad;
					a /= l_v;
					a += abs_vecs[v][c];
					M.c[c] = a;
				}
				M.xy = ((int64_t)M.c[0]*M.c[1])>>16;
				FixMat_x_Vec(vecs[v].u.geom.c3d, &camera.transf, &M, true);
			}
			gpuErr err;
			err = gpuWrite(&setCpCmd, sizeof(setCpCmd), true);
			assert(gpuOK == err);
			err = gpuWritev(cmdvec, sizeof_array(cmdvec), true);
			assert(gpuOK == err);
skip_shad:;
		}
		setCpCmd.nb_planes = 0;
		gpuErr err = gpuWrite(&setCpCmd, sizeof(setCpCmd), true);
		assert(gpuOK == err);
	}
}

static void scene2(void) {
	uint8_t (*ink_map)[256][256] = malloc(sizeof(*ink_map));
	assert(ink_map);
	splash_colors(ink_map, 8);
	unsigned target = 0;
	camera.target = &targets_scene2[target];
	camera.acc_pos[0] /= 8;
	camera.acc_pos[1] /= 2;
	camera.acc_pos[2] /= 2;
	camera.pos[0] = -50000;
	struct gpuBuf *txt = uncomp(pic4, 18, gpuColor(0, 0, 255), gpuColor(80, 20, 70));
	int32_t ang1 = 2000;
	int32_t ang2 = 0;
	set_dproj(7);
	cube_cmdFacet.perspective = 0;
	do {
		// build a new texture
		struct gpuBuf *text;
		text = gpuAlloc(8, 256, true);
		gpuFreeFC(text, 1);
		for (unsigned f=0; f<sizeof_array(facet_text); f++) {
			facet_text[f] = text;
		}
		ink2text(shared->buffers+gpuBuf_get_loc(text)->address, ink_map);
		camera_move();
		next_out_buf();
		transform_cube();
		draw_cube(false, LEN<<16);
		static FixVec pic_vec[4];
		locate_pic(pic_vec, 3000+(Fix_sin(ang1)>>5), Fix_sin(ang2)>>4);
		ang2 += 207;
		ang1 += 103;
		transf_draw_pic(txt, pic_vec, SHADOWS);
		transf_draw_pic(txt, pic_vec, PIC);
		show_out_buf();
		if (! --camera.target->nb_steps) {
			static unsigned run = 6;
			if (++target >= sizeof_array(targets_scene2)) target=0;//break;
			camera.target = &targets_scene2[target];
			camera.target->nb_steps = 150;
			if (0 == run--) break;
		}
	} while (1);
	gpuFreeFC(txt, 1);
	free(ink_map);
#	undef ANG2_LIM
}

/*
 * Main
 */

int main(void) {
	srand(time(NULL));
	Fix_trig_init();
	if (gpuOK != gpuOpen()) {
		assert(0);
	}
	scene1();
	scene2();
	gpuClose();
	return EXIT_SUCCESS;	// do an exec instead
}

