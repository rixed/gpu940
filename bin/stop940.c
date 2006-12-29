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
/* This program stops the 940, load a binary executable into its
 * memory core at specified offset, then starts the 940.
 */
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

int main(void)
{
	int mem = open("/dev/mem", O_RDWR);
	if (-1 == mem) {
		perror("open /dev/mem");
		return EXIT_FAILURE;
	}
	volatile uint16_t *mp_regs = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, mem, 0xC0000000U);
	if (MAP_FAILED == mp_regs) {
		perror("mmap /dev/mem");
		return EXIT_FAILURE;
	}
	// stops 940
	uint16_t sav = mp_regs[0x3B48>>1];
	mp_regs[0x3B48>>1] = (sav&0xff00) | 0x82;	// hold reset, set upper addr bits to 32 upper Mbytes.
	mp_regs[0x0904>>1] &= ~1;	// don't clock it
	return EXIT_SUCCESS;
}

