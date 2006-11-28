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

#define NB_STEPS 65536	// must be a power of 4
static int32_t sincos[NB_STEPS+NB_STEPS/4];

/*
 * Public Functions
 */

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
	return -sincos[NB_STEPS/4 + (ang&(NB_STEPS-1))];
}


