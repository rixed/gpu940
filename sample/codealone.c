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
/*
 * Scene 1 : Grab gp2x menu and make a texture with it, after a little pause
 * so that the user may have the impression to get back to menu after a crash.
 * The menu is mapped on a cube's face, that also provides other faces
 * for some "get ready" messages. While the tune gets more and more power, the cube
 * flips horizontaly showing various messages ("and now on to something more
 * fun", "codeIsAll", "credits", "greetz", "one is never alone with a spinning cube").
 * Then bash into the cube (severall times) to mimick trying to enter it (make the
 * cube react like a bubble). Then **flash**, we are inside !
 * Technically, the cube is a good old 6 facets cube (no round at the edge because
 * we need to project shadows. This is a "male" cube).
 *
 * Scene 2 : The 6 walls are mapped with the special texture "tw" with white/grey
 * colors, moving fast. Some pics appear (use flash) floating inside the cube, with
 * shadows on the walls (moving slightly, fast, like if the light was shaking).
 * Severall pics with many hot colors of severall people with beards (like a gnu,
 * RMS, Ritchie, Thompson, Marx, ...) Shaddows are rendered using clipplanes.
 * 
 * Scene 3 : Still in the cube, launch a "cube compo" : how many cubes can fit ?
 * launch many cubes (lines only) and watch them bounce around the big cube that's now
 * quickly rotating in all directions.
 *
 * Scene 4 : End with a more complex object (with many faces), like a psychedelic cow.
 * Use a fancy rendering technic, with flying colored spots (key color) with shaddows,
 * and a more colored texture for the walls.
 *
 * Camera : to have good looking moves, we need to control the camera with simple
 * commands such as "look at this from here, then go look at this from there", 
 * and with an added low amplitude signal for translation and rotation.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <gpu940.h>
#include <fixmath.h>

/*
 * Various Helper Functions
 */

static struct gpuBuf *grab_menu_texture(void) {
	struct gpuBuf *menu_texture = gpuAlloc(9, 512);
	assert(menu_texture);
#ifdef GP2X
	int fbdev = open("/dev/fb0", O_RDONLY);
	assert(fbdev != -1);
#	define GP2X_MENU_SIZE 320*240*sizeof(uint16_t)
	uint16_t *menupic = mmap(NULL, GP2X_MENU_SIZE, PROT_READ, MAP_SHARED, fbdev, 0);
	assert(menupic != MAP_FAILED);
	uint8_t *resized = malloc(512*512*3);
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
			resized[((y<<9)+x)*3+2] = (c16>>11)<<3;
			resized[((y<<9)+x)*3+1] = ((c16>>5)&0x3f)<<2;
			resized[((y<<9)+x)*3+0] = (c16&0x1f)<<3;
		}
	}
	(void)munmap(menupic, GP2X_MENU_SIZE);
	(void)close(fbdev);
	if (gpuOK != gpuLoadImg(gpuBuf_get_loc(menu_texture), (void *)resized, 0)) {
		assert(0);
	}
	(void)free(resized);
#	undef GP2X_MENU_TEXTURE
#else
	uint8_t *txt = malloc(512*512*3);
	for (unsigned y = 512; y--; ) {
		for (unsigned x = 512; x--; ) {
			if (y > (512+240)/2 || y < (512-240)/2 || x > (512+320)/2 || x < (512-320)/2) {
				txt[((y<<9)+x)*3+2] = x+y;
				txt[((y<<9)+x)*3+1] = x-y;
				txt[((y<<9)+x)*3+0] = x*y;
			} else {
				txt[((y<<9)+x)*3+2] = y;
				txt[((y<<9)+x)*3+1] = x;
				txt[((y<<9)+x)*3+0] = x*y;
			}
		}
	}
	if (gpuOK != gpuLoadImg(gpuBuf_get_loc(menu_texture), (void *)txt, 0)) {
		assert(0);
	}
	(void)free(txt);
#endif
	return menu_texture;
}

static struct gpuBuf *outBuf;
static void next_out_buf(void) {
	while (1) {
		outBuf = gpuAlloc(9, 250);
		if (outBuf) break;
		(void)sched_yield();
	}
	while (1) {
		gpuErr err = gpuSetOutBuf(outBuf);
		if (gpuOK == err) break;
		else if (gpuENOSPC == err) (void)sched_yield();
		else assert(0);
	}
}

static void show_out_buf(void) {
	while (1) {
		gpuErr err = gpuShowBuf(outBuf);
		if (gpuOK == err) break;
		else if  (gpuENOSPC == err) (void)sched_yield();
		else assert(0);
	}
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
	norm = Fix_sqrt(norm)<<8;
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
	.rendering_type = rendering_uvi,
};
static unsigned cube_facet[6][4] = {
	{ 0, 4, 5, 1 },
	{ 5, 6, 2, 1 },
	{ 7, 3, 2, 6 },
	{ 7, 4, 0, 3 },
	{ 3, 0, 1, 2 },
	{ 4, 7, 6, 5 }
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
static struct gpuBuf *facet_text[6] = { NULL, };

static void draw_facet(unsigned f, bool ext) {
	assert(f < sizeof_array(cube_facet));
	(void)ext;
	int32_t normal[3] = { 0, 0, 0 };
	static gpuCmdVector cmdVec[4];
	for (unsigned v=4; v--; ) {
		for (unsigned c=3; c--; ) {
			cmdVec[v].geom.c3d[c] = c3d[ cube_facet[f][v] ][c];
			normal[c] += cmdVec[v].geom.c3d[c] - camera.transf.trans[c];
		}
		cmdVec[v].uvi_params.u = uvs[f][v][0];
		cmdVec[v].uvi_params.v = uvs[f][v][1];
		cmdVec[v].uvi_params.i = ((2*LEN<<16)+c3d[ cube_facet[f][v] ][2])<<6;
	}
	int32_t scal = (
		((int64_t)normal[0]*c3d[ cube_facet[f][0] ][0]) +
		((int64_t)normal[1]*c3d[ cube_facet[f][0] ][1]) +
		((int64_t)normal[2]*c3d[ cube_facet[f][0] ][2])
	) >> 16;
	// is the facet backward ?
	if (ext && scal > 0) return;
	if (!ext && scal < 0) return;
	if (gpuOK != gpuSetTxtBuf(facet_text[f])) {
		assert(0);
	}
	static struct iovec cmdvec[4+1] = {
		{ .iov_base = &cube_cmdFacet, .iov_len = sizeof(cube_cmdFacet) },
		{ .iov_base = cmdVec+0, .iov_len = sizeof(*cmdVec) },
		{ .iov_base = cmdVec+1, .iov_len = sizeof(*cmdVec) },
		{ .iov_base = cmdVec+2, .iov_len = sizeof(*cmdVec) },
		{ .iov_base = cmdVec+3, .iov_len = sizeof(*cmdVec) },
	};
	while (1) {
		gpuErr err = gpuWritev(cmdvec, sizeof_array(cmdvec));
		if (gpuOK == err) break;
		else if (gpuENOSPC == err) (void)sched_yield();
		else assert(0);
	}
}

static void draw_cube(bool ext) {
	for (unsigned f=6; f--; ) {
		draw_facet(f, ext);
	}
}

static void clear_screen(void) {
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
	};
	facet_bg.color = gpuColor(0, 0, 0);
	static struct iovec cmdvec[1+4] = {
		{ .iov_base = &facet_bg, .iov_len = sizeof(facet_bg) },	// first facet to clear the background
		{ .iov_base = vec_bg+0, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+1, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+2, .iov_len = sizeof(*vec_bg) },
		{ .iov_base = vec_bg+3, .iov_len = sizeof(*vec_bg) }
	};
	while (1) {
		gpuErr err = gpuWritev(cmdvec, sizeof_array(cmdvec));
		if (gpuOK == err) break;
		else if (gpuENOSPC == err) (void)sched_yield();
		else assert(0);
	}
}

static void transform_cube(void) {
	// rotate all vecs
	for (unsigned v=sizeof_array(cube_vec); v--; ) {
		FixMat_x_Vec(c3d[v], &camera.transf, cube_vec+v, true);
	}
}

/*
 * Scenes
 */

#include "pics.c"

struct target targets_scene1[] = {
	{
		.pos = { 0, 0, (2*LEN<<16)+12000 },	// first facet (grabed gp2x menu)
		.lookat = { 0, 0, LEN<<16 },
		.Xy = 0,
		.nb_steps = 500,
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
		.nb_steps = 100,
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
	struct gpuBuf *txt = gpuAlloc(8, 256);
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
	facet_text[0] = uncomp(pic1, 24, gpuColor(180, 180, 160), gpuColor(100, 150, 200));
	facet_text[5] = uncomp(pic2, 10, gpuColor(123, 50, 200), gpuColor(30, 200, 200));
	facet_text[2] = uncomp(pic3, 10, gpuColor(180, 180, 200), gpuColor(200, 70, 230));
	do {
		// TODO : faire osciller les coordonnees de mapping pour les textures autres que menu
		camera_move();
		next_out_buf();
		clear_screen();
		transform_cube();
		draw_cube(true);
		show_out_buf();
		if (! --camera.target->nb_steps) {
			if (++target >= sizeof_array(targets_scene1)) break;
			camera.target = &targets_scene1[target];
			if (14 == target) {
				camera.acc_pos[0] = camera.acc_pos[1] = camera.acc_pos[2] = 0;
				printf("NOW!\n");
			}
		}
		// si on viens de tapper le cube, mettre a 0 l'inertie
	} while (1);
	gpuFree(facet_text[4]);
	gpuFree(facet_text[0]);
	gpuFree(facet_text[5]);
	gpuFree(facet_text[2]);
}

/*
 * Main
 */

int main(void) {
	if (gpuOK != gpuOpen()) {
		assert(0);
	}
	scene1();
	gpuClose();
	return EXIT_SUCCESS;	// do an exec instead
}

