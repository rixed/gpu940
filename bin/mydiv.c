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

#ifdef GP2X
extern uint32_t unsigned_divide32(uint32_t R, uint32_t D);
#else
static uint32_t unsigned_divide32(uint32_t R, uint32_t D)
{
	if (R < D) return 0;
	uint32_t normD;
	uint64_t next_normD = D;
	uint32_t Q = 0;
	uint32_t e = 0, next_e = 1;
	do {
		normD = next_normD;
		e = next_e;
		next_normD <<= 1U;
		next_e <<= 1U;
	} while (next_normD <= R);
	do {
		// can not be done more than 32 times
		if (normD <= R) {
			Q += e;
			R -= normD;
		}
		if (R < D) return Q;
		normD >>= 1U;
		e >>= 1U;
	} while (1);
}
#endif

#ifdef GP2X
extern uint64_t unsigned_divide64(uint64_t R, uint64_t D);
#else
static uint64_t unsigned_divide64(uint64_t R, uint64_t D)
{
	if (R < D) return 0;
	uint64_t normD, next_normD = D;
	uint64_t Q = 0;
	uint64_t e = 0, next_e = 1;
	do {
		normD = next_normD;
		e = next_e;
		next_normD <<= 1U;
		next_e <<= 1U;
	} while (next_normD <= R);
	do {
		// can not be done more than 64 times
		// after 32 iterations, R, normD, Q and e are 32 bits max
		if (normD <= R) {
			Q += e;
			R -= normD;
		}
		if (R < D) return Q;
		normD >>= 1U;
		e >>= 1U;
	} while (1);
}
#endif

static int32_t signed_divide32(int32_t n, int32_t d)
{
	uint32_t dd = Fix_abs(n);
	uint32_t dr = Fix_abs(d);
	uint32_t q = unsigned_divide32(dd, dr);
	if (n>>31 == d>>31) return q;
	return -q;
}

static int64_t signed_divide64(int64_t n, int64_t d)
{
	uint64_t dd = Fix_abs64(n);
	uint64_t dr = Fix_abs64(d);
	uint64_t q = unsigned_divide64(dd, dr);
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
	int32_t q = signed_divide32(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

uint32_t __udivsi3(uint32_t n, uint32_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	uint32_t q = unsigned_divide32(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

int64_t __divdi3(int64_t n, int64_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	int64_t q = signed_divide64(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

uint64_t __udivdi3(uint64_t n, uint64_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	uint64_t q = unsigned_divide64(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

int64_t __divti3(int64_t n, int64_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	int64_t q = signed_divide64(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

uint64_t __udivti3(uint64_t n, uint64_t d)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_DIV, "div");
	uint64_t q = unsigned_divide64(n, d);
	perftime_enter(previous_target, NULL);
	return q;
}

#endif
