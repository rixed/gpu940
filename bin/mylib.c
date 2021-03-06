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
#include <math.h>
#include "mylib.h"

void my_memset(void *dst, int value, size_t size)
{
	while (size >= sizeof(int)) {
		int *restrict d = dst;
		*d = value;
		dst = d+1;
		size -= sizeof(int);
	}
	while (size--) {
		char *restrict d = dst;
		*d = value;
		dst = d+1;
	}
}

#ifndef GP2X
void my_memset_words(uint32_t *dst, uint32_t value, unsigned nb_words)
{
	while (nb_words--) {
		*dst++ = value;
	}
}
#endif

void my_memcpy(void *restrict dst, void const *restrict src, size_t size)
{
	while (size >= sizeof(int)) {
		int *restrict d = dst;
		int const *restrict s = src;
		*d = *s;
		dst = d+1;
		src = s+1;
		size -= sizeof(int);
	}
	while (size--) {
		char *restrict d = dst;
		char const *restrict s = src;
		*d = *s;
		dst = d+1;
		src = s+1;
	}
}

