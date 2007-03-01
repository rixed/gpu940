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

static unsigned offset = 0;

static void syntax(void) {
	printf("load940 file [offset [bench]]\n");
}

int main(int nb_args, char **args) {
	int bench = 0;
	if (nb_args <= 1 || nb_args > 4) {
		syntax();
		return EXIT_FAILURE;
	}
	if (nb_args >= 3) {
		offset = strtoul(args[2], NULL, 0);
	}
	bench = nb_args == 4;
	printf("Load file '%s' at offset %u\n", args[1], offset);
	int prog = open(args[1], O_RDONLY);
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
	uint32_t *uppermem = mmap(NULL, progsize, PROT_READ|PROT_WRITE, MAP_SHARED, mem, 0x2000000U + offset);
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
	char *dest = (void *)uppermem;
	while ( 0 < (err=read(prog, dest, progsize)) ) {
		printf("  writing %zd bytes...\n", err);
		dest += err;
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
	if (! bench) return EXIT_SUCCESS;
	// Wait until termination and display time
	struct timeval start;
	if (0 != gettimeofday(&start, NULL)) {
		perror("gettimeofday start");
		return EXIT_FAILURE;
	}
	while (uppermem[0]) {
		struct timespec const ts = { .tv_sec = 0, .tv_nsec = 1000 /* 1microsec */ };
		(void)nanosleep(&ts, NULL);
	}
	struct timeval end;
	if (0 != gettimeofday(&end, NULL)) {
		perror("gettimeofday end");
		return EXIT_FAILURE;
	}
	uint64_t delay = (end.tv_sec*1000000 + end.tv_usec) - (start.tv_sec*1000000 + start.tv_usec);
	printf("940 code run for approx %"PRIu64" usec\n", delay);
	return EXIT_SUCCESS;
}

