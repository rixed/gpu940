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
#if defined(GP2X) || SOFT_DIVS == 1
#include <stdint.h>
#include "gpu940i.h"
#include "../perftime/perftime.h"

/*
 * Private Functions
 */

// TODO : unloop and jump into the unrolled code to avoid testing n==0
static int nbbit(uint64_t v, int n) {
	uint64_t single = 1;
	single <<= (n-1);
	while (n && 0 == (v&single)) {
		n--;
		single >>= 1;
	}
	return n;
}

static uint64_t unsigned_divide(uint64_t R, uint64_t D) {
	uint64_t Q = 0, normD;
	int d = nbbit(D, (D>>32) ? 64:32);	// many times we come here with R and/or D beeing 32 bits only
	int r = (R>>32) ? 64:32, e;
	while (R >= D) {
		r = nbbit(R, r);
		e = r-d;
		normD = D << e;
		if (normD > R) {
			normD >>= 1;
			e --;
		}
		R -= normD;
		Q += (uint64_t)1<<e;
	}
	return Q;
}

static int64_t signed_divide(int64_t n, int64_t d) {
	uint64_t dd = Fix_abs64(n);
	uint64_t dr = Fix_abs64(d);
	uint64_t q = unsigned_divide(dd, dr);
	if (n>>63 == d>>63) return q;
	return -q;
}

/*
 * Public Functions (GCC)
 */

int32_t __divsi3(int32_t n, int32_t d) {
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div", true);
	int32_t q = signed_divide(n, d);
	perftime_enter(previous_target, NULL, false);
	return q;
}

uint32_t __udivsi3(uint32_t n, uint32_t d) {
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div", true);
	uint32_t q = unsigned_divide(n, d);
	perftime_enter(previous_target, NULL, false);
	return q;
}

int64_t __divdi3(int64_t n, int64_t d) {
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div", true);
	int64_t q = signed_divide(n, d);
	perftime_enter(previous_target, NULL, false);
	return q;
}

uint64_t __udivdi3(uint64_t n, uint64_t d) {
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div", true);
	uint64_t q = unsigned_divide(n, d);
	perftime_enter(previous_target, NULL, false);
	return q;
}

#endif
