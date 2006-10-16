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
#ifndef FIXMAT_H_060330
#define FIXMAT_H_060330

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	int32_t c[3], xy ;  // 3d coordinates
} FixVec;

typedef struct {
	int32_t rot[3][3]; // openGl like order -> [row][line]
	int32_t ab[3];  // rot[0][i]*rot[1][i]
	int32_t trans[3];  // translation T[i]
} FixMat;

extern FixMat const matrix_id;

void FixMat_x_Vec(int32_t dest[3], FixMat const *mat, FixVec const *src, bool translate);
void FixMatT_x_Vec(int32_t dest[3], FixMat const *mat, int32_t const src[3], bool translate);
static inline int64_t Fix_scalar(int32_t v1[3], int32_t v2[3]) {
	int64_t res = (int64_t)v1[0]*v2[0] + (int64_t)v1[1]*v2[1] + (int64_t)v1[2]*v2[2];
	return res>>16;
}
void Fix_normalize(int32_t v[3]);

static inline bool Fix_same_sign(int32_t v0, int32_t v1) {
	if (v0 == 0 || v1 == 0) return 1;	// 0 have both signs
	return (v0&0x80000000)==(v1&0x80000000);
}
static inline int32_t Fix_abs(int32_t v) {
	int32_t m=v>>31;
	return (v^m)-m;
}
static inline int64_t Fix_abs64(int64_t v) {
	int64_t m=v>>63;
	return (v^m)-m;
}
static inline int32_t Fix_inv(int32_t v) {
	if (v>0) return ((uint32_t)~0U)/(uint32_t)v;
	else return -((uint32_t)~0U)/(uint32_t)-v;
}

static inline int32_t Fix_mul(int32_t a, int32_t b) {
	return ((int64_t)a*b)>>16;
}
static inline int32_t Fix_div(int32_t a, int32_t b) {
	return (((int64_t)a)<<16)/b;
}

void Fix_proj(int32_t c2d[2], int32_t const c3d[3], int dproj);

int64_t Fix_mul64x64(int64_t v, int64_t w);

#define FIXED_PI 32768	// 2*PI is 1<<16 for these functions
void Fix_trig_init(void);
// These returns 16.16 as usual
int32_t Fix_cos(int32_t ang);
int32_t Fix_sin(int32_t ang);

int32_t my_sqrt(int32_t n);
static inline int32_t Fix_sqrt(int32_t n) {
	return my_sqrt(n)<<8;
}

#define is_power_of_2(v) (0 == ((v)&((v)-1)))

#endif
