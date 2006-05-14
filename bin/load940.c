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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "gpu940.h"

static unsigned offset = 0;

static void syntax(void) {
	printf("load940 [offset] file\n");
}

int main(int nb_args, char **args) {
	int opt = 1;
	if (1 >= nb_args) {
		syntax();
		return EXIT_FAILURE;
	}
	if (3 == nb_args) {
		offset = strtoul(args[opt++], NULL, 0);
	}
	printf("Load file '%s'\n", args[opt]);
	int prog = open(args[opt], O_RDONLY);
	if (-1 == prog) {
		perror("open program");
		return EXIT_FAILURE;
	}
	off_t progsize = lseek(prog, 0, SEEK_END);
	if ((off_t)-1 == progsize || (off_t)-1 == lseek(prog, 0, SEEK_SET)) {
		perror("lseek");
		return EXIT_FAILURE;
	}
	int mem = open("/dev/mem", O_RDWR);
	if (-1 == mem) {
		perror("open /dev/mem");
		return EXIT_FAILURE;
	}
	char *uppermem = mmap(NULL, progsize, PROT_WRITE, MAP_SHARED, mem, 0x2000000U + offset);
	volatile uint16_t *mp_regs = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, mem, 0xC0000000U);
	if (MAP_FAILED == uppermem || MAP_FAILED == mp_regs) {
		perror("mmap /dev/mem");
		return EXIT_FAILURE;
	}
	// stops 940
	uint16_t sav = mp_regs[0x3B48>>1];
	mp_regs[0x3B48>>1] = (sav&0xff00) | 0x82;	// hold reset, set upper addr bits to 32 upper Mbytes.
	mp_regs[0x0904>>1] &= ~1;	// don't clock it
	// copy
	ssize_t err;
	while ( 0 < (err=read(prog, uppermem, progsize)) ) {
		printf("  writing %d bytes...\n", err);
		uppermem += err;
		progsize -= err;
	}
	if (-1 == err) {
		perror("read");
		return EXIT_FAILURE;
	}
	// Start 940. It will now execute handler of reset exception, located at address zero.
	mp_regs[0x3B40>>1] = 0;
	mp_regs[0x3B42>>1] = 0;
	mp_regs[0x0904>>1] |= 1;	// clock it
	mp_regs[0x3B48>>1] &= ~(1<<7);	// clear reset bits
	// OK, your program should be crashed now ;-)
	puts("Ok");
	return EXIT_SUCCESS;
}

