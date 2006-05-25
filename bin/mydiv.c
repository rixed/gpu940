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
#include <stdint.h>
#include "gpu940i.h"
#include "../perftime/perftime.h"

static void unsigned_divide(uint64_t dividend, uint64_t divisor, uint64_t *pquotient, uint64_t *premainder) {
	uint64_t t, num_bits;
	uint64_t q, bit, d;
	uint64_t i;
	uint64_t remainder = 0;
	uint64_t quotient = 0;
	*pquotient = 0;
	*premainder = 0;
	if (divisor == 0) return;
	if (divisor > dividend) {
		*premainder = dividend;
		return;
	}
	if (divisor == dividend) {
		*pquotient = 1;
		return;
	}
	num_bits = 64;
	while (remainder < divisor) {
		bit = (dividend & 0x8000000000000000) >> 63;
		remainder = (remainder << 1) | bit;
		d = dividend;
		dividend = dividend << 1;
		num_bits--;
	}
	/* The loop, above, always goes one iteration too far.
		To avoid inserting an "if" statement inside the loop
		the last iteration is simply reversed. */
	dividend = d;
	remainder = remainder >> 1;
	num_bits++;
	for (i = 0; i < num_bits; i++) {
		bit = (dividend & 0x8000000000000000) >> 63;
		remainder = (remainder << 1) | bit;
		t = remainder - divisor;
		q = !((t & 0x8000000000000000) >> 63);
		dividend = dividend << 1;
		quotient = (quotient << 1) | q;
		if (q) {
			remainder = t;
		}
	}
	*premainder = remainder;
	*pquotient = quotient;
}


#define myabs(x)  ((x) < 0 ? -(x) : (x))

static void signed_divide(int64_t dividend, int64_t divisor, int64_t *pquotient, int64_t *premainder ) {
	uint64_t dend, dor;
	uint64_t q, r;
	dend = myabs(dividend);
	dor  = myabs(divisor);
	unsigned_divide( dend, dor, &q, &r );
	/* the sign of the remainder is the same as the sign of the dividend
		and the quotient is negated if the signs of the operands are
		opposite */
	*pquotient = q;
	if (dividend < 0) {
		*premainder = -((int64_t) r);
		if (divisor > 0)
			*pquotient = -((int64_t) q);
	} else { /* positive dividend */
		*premainder = r;
		if (divisor < 0)
			*pquotient = -((int64_t) q);
	}
}

int32_t __divsi3(int32_t n, int32_t d) {
	int64_t q, r;
	signed_divide(n, d, &q, &r);
	return q;
}

uint32_t __udivsi3(uint32_t n, uint32_t d) {
	uint64_t q, r;
	perftime_enter(PERF_DIV, "div");
	unsigned_divide(n, d, &q, &r);
	perftime_return();
	return q;
}

int64_t __divdi3(int64_t n, int64_t d) {
	int64_t q, r;
	perftime_enter(PERF_DIV, "div");
	signed_divide(n, d, &q, &r);
	perftime_return();
	return q;
}

uint64_t __udivdi3(uint64_t n, uint64_t d) {
	uint64_t q, r;
	perftime_enter(PERF_DIV, "div");
	unsigned_divide(n, d, &q, &r);
	perftime_return();
	return q;
}

