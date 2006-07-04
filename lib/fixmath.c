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
#include "fixmath.h"

/*
 * Data Definitions
 */

/*
 * Public Functions
 */

static inline bool Fix_same_sign(int32_t v0, int32_t v1);
static inline int32_t Fix_abs(int32_t v);

void FixMat_x_Vec(int32_t dest[3], FixMat const *mat, FixVec const *src, bool translate) {
	for (unsigned i=0; i<3; i++) {
		dest[i] = (((int64_t)mat->rot[0][i] + src->c[1])*(mat->rot[1][i] + src->c[0])>>16) + (((int64_t)mat->rot[2][i]*src->c[2])>>16) - mat->ab[i] - src->xy;
		if (translate) dest[i] += mat->trans[i];
	}
}

void FixMatT_x_Vec(int32_t dest[3], FixMat const *mat, int32_t const src[3], bool translate) {
	int32_t transT[3];
	if (translate) {
		FixMatT_x_Vec(transT, mat, mat->trans, false);
	}
	for (unsigned i=0; i<3; i++) {
		dest[i] = 0;
		for (unsigned j=0; j<3; j++) {
			dest[i] += ((int64_t)mat->rot[i][j] * src[j])>>16;
		}
		if (translate) dest[i] -= transT[i];
	}
}
 
extern inline int64_t Fix_scalar(int32_t v1[3], int32_t v2[3]);
extern inline bool Fix_same_sign(int32_t v0, int32_t v1);
extern inline int32_t Fix_abs(int32_t v);
extern inline int64_t Fix_abs64(int64_t v);
extern inline int32_t Fix_inv(int32_t v);

void Fix_normalize(int32_t v[3]) {
	int32_t norm;
	do {	// FIXME
		int64_t scal = Fix_scalar(v, v);
		if (scal < INT32_MAX) {
			norm = scal;
			break;
		}
		for (unsigned c=3; c--; ) {
			v[c] >>= 1;
		}
	} while ( 1 );
	norm = Fix_sqrt(norm)<<8;
	if (norm) {
		for (unsigned c=3; c--; ) {
			v[c] = ((int64_t)v[c]<<16)/norm;
		}
	}
}

void Fix_proj(int32_t c2d[2], int32_t const c3d[3], int dproj) {
	c2d[0] = (((int64_t)c3d[0]<<16)/(-c3d[2]))<<dproj;
	c2d[1] = (((int64_t)c3d[1]<<16)/(-c3d[2]))<<dproj;
}

int64_t Fix_mul64x64(int64_t v, int64_t w) {
	int64_t sign = 0;
	if (v < 0) {
		v = -v;
		sign ^= 0xffffffffffffffffULL;
	}
	if (w < 0) {
		w = -w;
		sign ^= 0xffffffffffffffffULL;
	}
	int32_t a = v>>32;
	int32_t b = v;
	int32_t c = w>>32;
	int32_t d = w;
	int64_t db = (int64_t)d*b;
	int64_t da = (int64_t)d*a;
	int64_t cb = (int64_t)c*b;
	int64_t res = db + (da<<32) + (cb<<32);
	res ^= sign;
	res -= sign;
	return res;
}


#define NB_STEPS 65536	// must be a power of 4
static int32_t sincos[NB_STEPS+NB_STEPS/4];
void Fix_trig_init(void) {
	unsigned n;
#	define BITS_PREC 30
	int64_t c=1<<BITS_PREC;
	int64_t s=0;
	int64_t ku1 = 1073741819;
	int64_t ku2 = 102944;
	for (n=0; n<NB_STEPS+NB_STEPS/4; n++) {
		sincos[n] = c>>(BITS_PREC-16);
		int64_t new_c = (Fix_mul64x64(c,ku1) - Fix_mul64x64(s,ku2))>>BITS_PREC;
		s = (Fix_mul64x64(s,ku1) + Fix_mul64x64(c,ku2))>>BITS_PREC;
		c = new_c;
	}
#	undef BITS_PREC
}

int32_t Fix_cos(int32_t ang) {
	return sincos[ang&(NB_STEPS-1)];
}
int32_t Fix_sin(int32_t ang) {
	return sincos[NB_STEPS/4 + (ang&(NB_STEPS-1))];
}

// The following sqrt function was posted by Dijkstra on newsgroups
#define iter1(N) \
	try = root + (1 << (N)); \
	if (n >= try << (N)) { \
		n -= try << (N); \
		root |= 2 << (N); \
	}

int32_t Fix_sqrt(int32_t n) {
	int32_t root = 0, try;
	iter1(15); iter1(14); iter1(13); iter1(12);
	iter1(11); iter1(10); iter1( 9); iter1( 8);
	iter1( 7); iter1( 6); iter1( 5); iter1( 4);
	iter1( 3); iter1( 2); iter1( 1); iter1( 0);
	return root >> 1;
}


#undef NB_STEPS
