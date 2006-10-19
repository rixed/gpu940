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

static uint64_t unsigned_divide(uint64_t R, uint64_t D)
{
	if (R < D) return 0;
	uint64_t Q = 0, normD;
	int e = -1;// uint64_t e = 1;
	uint64_t next_normD = D;
	do {
		normD = next_normD;
		next_normD = normD<<1;
		e ++;//<<= 1;
	} while (next_normD <= R);
	//e >>= 1;
	do {
		R -= normD;
		Q += (uint64_t)1<<e;
		if (R < D) return Q;
		do {
			normD >>= 1;
			e --;//>>= 1;
		} while (normD > R);
	} while (1);
}

static int64_t signed_divide(int64_t n, int64_t d)
{
	uint64_t dd = Fix_abs64(n);
	uint64_t dr = Fix_abs64(d);
	uint64_t q = unsigned_divide(dd, dr);
	if (n>>63 == d>>63) return q;
	return -q;
}

/*
 * Public Functions (GCC)
 */

int32_t __divsi3(int32_t n, int32_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	int32_t q = signed_divide(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

uint32_t __udivsi3(uint32_t n, uint32_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	uint32_t q = unsigned_divide(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

int64_t __divdi3(int64_t n, int64_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	int64_t q = signed_divide(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

uint64_t __udivdi3(uint64_t n, uint64_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	uint64_t q = unsigned_divide(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

int64_t __divti3(int64_t n, int64_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	int64_t q = signed_divide(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

uint64_t __udivti3(uint64_t n, uint64_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	uint64_t q = unsigned_divide(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

#endif
