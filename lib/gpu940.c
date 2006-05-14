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
#include "gpu940.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>

/*
 * Data Definitions
 */

struct gpu940_shared *shared;
static int shared_fd;
static struct bufPrm {
	struct cmdPtrs *ptrs;
	uint32_t *start, *stop;
} bufPrms[2];

/*
 * Private Functions
 */

// All unsigned sizes are in words
static inline void copy32(uint32_t *restrict dest, uint32_t const *restrict src, unsigned size) {
	for ( ; size--; ) dest[size] = src[size]
}

// we do not check for available space
static int32_t append_to_cmdbuf(int32_t end, uint32_t const *restrict src, unsigned size, struct bufPrm const *buf) {
	int overrun = &buf->start[dst+size] - buf->stop;
	if (overrun < 0) {
		copy_32(buf->start+end, src, size);
		return end+size;
	} else {
		size_t chunk = size - overrun;
		copy_32(buf->start+end, src, chunk);
		copy_32(buf->start, src+chunk, overrun);
		return overrun;
	}
}

static inline unsigned avail_cmd_space(struct bufPrm const *buf) {
	int begin = buf->ptrs->begin;	// this is not a problem if begin is incremented while we use this value,
	// but this would be a problem if it's incremented and wrap around between the if and the computation of available space !
	// as for cmds_end, we are the only one that increment it.
	if (begin > buf->ptrs->end) {
		return begin - buf->ptrs->end -1;
	} else {
		return size - buf->ptrs->end + begin -1;
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

gpu940Err gpu940_open(void) {
#	ifdef GP2X
	shared_fd = open("/dev/mem", O_RDWR);
#	else
	shared_fd = open(CMDFILE, O_RDWR);
#	endif
	if (-1 == shared_fd) return gpu940_ESYS;
	int pg_size = getpagesize();
	shared = mmap(NULL, ((sizeof(*shared)/pg_size)+1)*pg_size, PROT_READ|PROT_WRITE, MAP_SHARED, devmem_fd, SHARED_PHYSICAL_ADDR);
	if (MAP_FAILED == shared) {
		perror("mmap /dev/mem");
		fprintf(stderr, "  @%x\n", (unsigned)SHARED_PHYSICAL_ADDR);
		return gpu940_ESYS;
	}
	bufPrms[0].ptrs = &shared->asyn_ptrs;
	bufPrms[0].start = &shared->asyn_cmds;
	bufPrms[0].stop= &shared->asyn_cmds+sizeof_array(shared->asyn_cmds);
	bufPrms[1].ptrs = &shared->syn_ptrs;
	bufPrms[1].start = &shared->syn_cmds;
	bufPrms[1].stop= &shared->syn_cmds+sizeof_array(shared->syn_cmds);
	return gpu940_OK;
}

void gpu940_close(void) {
	(void)munmap(shared, sizeof(*shared));
	(void)close(shared_fd);
}

gpu940Err gpu940_write(void *cmd, size_t size, unsigned buf) {
	assert(buf<sizeof_array(bufPrms));
	assert(!(size&3));
	size >>= 2;
	if (avail_cmd_space(bufPrms+buf) < size) return gpu940_ENOSPC;
	bufPrms[buf].ptrs->end = append_to_cmdbuf(bufPrms[buf].ptrs->end, cmd, size);
	flush_writes();
	return gpu940_OK;
}

gpu940Err gpu940_writev(const struct iovec *cmdvec, size_t count, unsigned buf) {
	assert(buf<sizeof_array(bufPrms));
	size_t size = 0;
	for (unsigned c=0; c<count; c++) size += cmdvec[c].iov_len;
	assert(!(size&3));
	if (avail_cmd_space(bufPrms+buf) < size>>2) return gpu940_ENOSPC;
	int end = bufPrms[buf].ptrs->end;
	for (unsigned c=0; c<count; c++) {
		end = append_to_cmdbuf(end, cmdvec[c].iov_base, cmdvec[c].iov_len>>2);
	}
	bufPrms[buf].ptrs->end = end;
	flush_writes();
	return gpu940_OK;
}

uint32_t gpu940_read_err(void) {
	uint32_t err = shared->error_flags;	// do both atomically (xchg)
	shared->error_flags = 0;
	return err;
}

