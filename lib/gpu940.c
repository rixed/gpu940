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
static inline void copy32(uint32_t *restrict dest, uint32_t const *restrict src, unsigned size)
{
	for ( ; size--; ) dest[size] = src[size];
}

// we do not check for available space
static int32_t append_to_cmdbuf(unsigned end, uint32_t const *restrict src, unsigned size)
{
	copy32(shared->cmds+end, src, size);
	return end+size;
}

static bool can_write(unsigned count, bool can_wait)
{
	do {
		uint32_t begin = shared->cmds_begin;	// cmds_begin could change during our computation
		if (begin > shared->cmds_end) {
			if (begin - shared->cmds_end > count + 1) {
				return true;
			}
		} else {
			if (sizeof_array(shared->cmds) - shared->cmds_end > count + 1 /* keep space for a future rewind */) {
				return true;
			}
			// write a rewind if possible
			static gpuCmdRewind const cmdRewind = { .opcode = gpuREWIND };
			if (begin > 0) {	// otherwise we can not set end to 0
				(void)append_to_cmdbuf(shared->cmds_end, (void const *)&cmdRewind, sizeof(cmdRewind)>>2);
				shared->cmds_end = 0;
			}
		}
		// now wait
		(void)sched_yield();
	} while (can_wait);
	return false;
}

static void flush_writes(void)
{
	// we must ensure that our writes are seen by the other peer in that order, that is data first, then pointer update
#ifdef GP2X
	// on the GP2X, this mean drain the write buffer (reading uncached memory is enought)
	// (note: shared is uncached on the 920T)
	volatile uint32_t GCCunused dummy = shared->error_flags;
#endif
}

/*
 * Public Functions
 */

gpuErr gpuOpen(void)
{
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

void gpuClose(void)
{
	(void)munmap(shared, mmapsize);
	(void)close(shared_fd);
}

gpuErr gpuWrite(void const *cmd/* must be word aligned*/, size_t size, bool can_wait)
{
	assert(!(size&3));
	size /= sizeof(uint32_t);
	if (! can_write(size, can_wait)) {
		return gpuENOSPC;
	}
	shared->cmds_end = append_to_cmdbuf(shared->cmds_end, cmd, size);
	flush_writes();
	return gpuOK;
}

gpuErr gpuWritev(struct iovec const *cmdvec, size_t count, bool can_wait)
{
	size_t size = 0;
	for (unsigned c=0; c<count; c++) {
		size += cmdvec[c].iov_len;
	}
	assert(!(size&3));
	size /= sizeof(uint32_t);
	if (! can_write(size, can_wait)) {
		return gpuENOSPC;
	}
	int end = shared->cmds_end;
	for (unsigned c=0; c<count; c++) {
		end = append_to_cmdbuf(end, cmdvec[c].iov_base, cmdvec[c].iov_len>>2);
	}
	shared->cmds_end = end;
	flush_writes();
	return gpuOK;
}

uint32_t gpuReadErr(void)
{
	uint32_t err = shared->error_flags;	// do both atomically (xchg)
	shared->error_flags = 0;
	return err;
}

