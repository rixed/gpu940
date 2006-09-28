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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include "gpu940.h"

/*
 * Data Definitions
 */

struct gpuShared *shared;
static int shared_fd;
static size_t mmapsize;

/*
 * Private Functions
 */

// All unsigned sizes are in words
static inline void copy32(uint32_t *restrict dest, uint32_t const *restrict src, unsigned size) {
	for ( ; size--; ) dest[size] = src[size];
}

// we do not check for available space
static int32_t append_to_cmdbuf(unsigned end, uint32_t const *restrict src, unsigned size) {
	int overrun = end+size - sizeof_array(shared->cmds);
	if (overrun < 0) {
		copy32(shared->cmds+end, src, size);
		return end+size;
	} else {
		size_t chunk = size - overrun;
		copy32(shared->cmds+end, src, chunk);
		copy32(shared->cmds, src+chunk, overrun);
		return overrun;
	}
}

static inline unsigned avail_cmd_space(void) {
	int begin = shared->cmds_begin;	// this is not a problem if begin is incremented while we use this value,
	// but this would be a problem if it's incremented and wrap around between the if and the computation of available space !
	// as for cmds_end, we are the only one that increment it.
	if (begin > shared->cmds_end) {
		return begin - shared->cmds_end -1;
	} else {
		return sizeof_array(shared->cmds) - shared->cmds_end + begin -1;
	}
}

static void flush_writes(void) {
	// we must ensure that our writes are seen by the other peer in that order, that is data first, then pointer update
#ifdef GP2X
	// on th eGP2X, this mean drain the write buffer (reading uncached memory is enought)
	volatile uint32_t __unused dummy = shared->error_flags;
#endif
}

/*
 * Public Functions
 */

gpuErr gpuOpen(void) {
#	ifdef GP2X
#		define MMAP_OFFSET SHARED_PHYSICAL_ADDR 
#	else
#		define MMAP_OFFSET 0
#	endif
	shared_fd = open(CMDFILE, O_RDWR);
	if (-1 == shared_fd) return gpuESYS;
	int pg_size = getpagesize();
	mmapsize = ((sizeof(*shared)/pg_size)+1)*pg_size;
	shared = mmap(NULL, mmapsize, PROT_READ|PROT_WRITE, MAP_SHARED, shared_fd, MMAP_OFFSET);
	if (MAP_FAILED == shared) {
		perror("mmap " CMDFILE);
		fprintf(stderr, "  @%x\n", (unsigned)SHARED_PHYSICAL_ADDR);
		return gpuESYS;
	}
	static gpuCmdReset reset = { .opcode = gpuRESET };
	gpuErr err = gpuWrite(&reset, sizeof(reset), true);
	if (gpuOK != err) return err;
	while (shared->frame_count != 0) ;
	return gpuOK;
#	undef MMAP_OFFSET
}

void gpuClose(void) {
	(void)munmap(shared, mmapsize);
	(void)close(shared_fd);
}

gpuErr gpuWrite(void *cmd/* must be word aligned*/, size_t size, bool can_wait) {
	assert(!(size&3));
	size >>= 2;
	while (avail_cmd_space() < size) {
		if (! can_wait) return gpuENOSPC;
		(void)sched_yield();
	}
	shared->cmds_end = append_to_cmdbuf(shared->cmds_end, cmd, size);
	flush_writes();
	return gpuOK;
}

gpuErr gpuWritev(const struct iovec *cmdvec, size_t count, bool can_wait) {
	size_t size = 0;
	for (unsigned c=0; c<count; c++) size += cmdvec[c].iov_len;
	assert(!(size&3));
	while (avail_cmd_space() < size>>2) {
		if (! can_wait) return gpuENOSPC;
		(void)sched_yield();
	}
	int end = shared->cmds_end;
	for (unsigned c=0; c<count; c++) {
		end = append_to_cmdbuf(end, cmdvec[c].iov_base, cmdvec[c].iov_len>>2);
	}
	shared->cmds_end = end;
	flush_writes();
	return gpuOK;
}

uint32_t gpuReadErr(void) {
	uint32_t err = shared->error_flags;	// do both atomically (xchg)
	shared->error_flags = 0;
	return err;
}

