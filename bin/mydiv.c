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
//#ifdef GP2X
#include <stdint.h>
#include "gpu940i.h"
#include "../perftime/perftime.h"

static int nbbit(uint64_t v, int n) {
	while (n && 0 == (v&((uint64_t)1<<(n-1)))) n--;
	return n;
}

static uint64_t unsigned_divide(uint64_t dividend, uint64_t divisor) {
	uint64_t R = dividend;
	uint64_t const D = divisor;
	uint64_t Q = 0;
	int d = nbbit(D, 64);
	int e, r = 64;
	while (R >= D) {
		r = nbbit(R, r);
		e = r-d-1;
		if (e<0) e=0;
		R -= D<<e;
		Q += (uint64_t)1<<e;
	}
	return Q;
}

#define myabs(x)  ((x) < 0 ? -(x) : (x))

static int64_t signed_divide(int64_t n, int64_t d) {
	uint64_t dd = Fix_abs64(n);
	uint64_t dr = Fix_abs64(d);
	uint64_t q = unsigned_divide(dd, dr);
	if (n>>63 == d>>63) return q;
	return -q;
}

int32_t __divsi3(int32_t n, int32_t d) {
	perftime_enter(PERF_DIV, "div");
	int32_t q = signed_divide(n, d);
	perftime_return();
	return q;
}

uint32_t __udivsi3(uint32_t n, uint32_t d) {
	perftime_enter(PERF_DIV, "div");
	uint32_t q = unsigned_divide(n, d);
	perftime_return();
	return q;
}

int64_t __divdi3(int64_t n, int64_t d) {
	perftime_enter(PERF_DIV, "div");
	int64_t q = signed_divide(n, d);
	perftime_return();
	return q;
}

uint64_t __udivdi3(uint64_t n, uint64_t d) {
	perftime_enter(PERF_DIV, "div");
	uint64_t q = unsigned_divide(n, d);
	perftime_return();
	return q;
}
//#endif
