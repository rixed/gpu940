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
/* This handle memory (de)allocation of shared->buffers memory region.
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <sched.h>
#include "gpu940.h"
#include "kernlist.h"
#include "fixmath.h"

/*
 * Data Definitions
 */

#ifndef NDEBUG
#	define MEM_DEBUG(txt, buf) do { printf("gpumm: "txt" @%u [%u]\n", (buf)->loc.address, ((buf)->loc.height << (buf)->loc.width_log)); } while (0)
#else
#	define MEM_DEBUG
#endif
#define ADDRESS_MAX sizeof_array(shared->buffers)

struct gpuBuf {
	struct list_head list;
	struct list_head fc_list;
	unsigned free_after_fc;	// when framecount > this, the buffer can be freed. yes, we suppose fc never loops.
	struct buffer_loc loc;
};
static LIST_HEAD(list);
static LIST_HEAD(fc_list);

static struct buf_cache {
	struct list_head cache_list;
	struct gpuBuf buf;
} *buf_cache = NULL;
static LIST_HEAD(cache_list);
static unsigned cache_size = 0;	// in struct buf_cache unit

static unsigned my_frame_count = 0;

/*
 * Private Functions
 */

static struct gpuBuf *buf_new(void) {
	if (list_empty(&cache_list)) {	// must alloc more cache
		unsigned new_cache_size = (cache_size + 64)*4;
		void *new_cache = realloc(buf_cache, new_cache_size*sizeof(*buf_cache));
		assert(!buf_cache || new_cache == buf_cache);
		// hopefully, the cache_list was empty : no need to update pointer if buf_cache moves
		buf_cache = new_cache;
		for (unsigned i=cache_size; i<new_cache_size; i++) {
			list_add_tail(&buf_cache[i].cache_list, &cache_list);
		}
		cache_size = new_cache_size;
	}
	struct buf_cache *bc = list_entry(cache_list.next, struct buf_cache, cache_list);
	list_del(&bc->cache_list);
	return &bc->buf;
}

static void buf_del(struct gpuBuf *buf) {
	assert(buf);
	MEM_DEBUG("delete", buf);
	struct buf_cache *bc = (struct buf_cache *)((char *)buf - offsetof(struct buf_cache, buf));
	list_add_tail(&bc->cache_list, &cache_list);
}

static void free_buf(struct gpuBuf *buf) {
	assert(buf);
	list_del(&buf->list);
	buf_del(buf);
}

static void free_fc(void) {
	// free all possible buffers based on frame count
	struct gpuBuf *buf, *next;
	unsigned fc = shared->frame_count;
	list_for_each_entry_safe(buf, next, &fc_list, fc_list) {
		if (fc > buf->free_after_fc) {
			free_buf(buf);
			list_del(&buf->fc_list);
		} else {
			break;	// free_after_fc are in ascending order
		}
	}
}

struct gpuBuf *gpuAlloc_(unsigned width_log, unsigned height) {
	struct gpuBuf *buf = NULL;
	unsigned size = height << width_log;
	unsigned next_free = 0;
	free_fc();
	list_for_each_entry(buf, &list, list) {
		assert(buf->loc.address >= next_free);	// supposed to be sorted in ascending order
		if (buf->loc.address - next_free >= size) {
			break;
		}
		next_free = buf->loc.address + (buf->loc.height<<buf->loc.width_log);
	}
	if (next_free + size > ADDRESS_MAX) return NULL;
	struct gpuBuf *new = buf_new();
	new->loc.address = next_free;
	new->loc.width_log = width_log;
	new->loc.height = height;
	new->free_after_fc = ~0;	// if FC never loops, we will never free this one (until requested bu Free()/FreeFC()
	list_add_tail(&new->list, &buf->list);	// add before buf
	MEM_DEBUG("new", new);
	return new;
}

/*
 * Public Functions
 */

struct gpuBuf *gpuAlloc(unsigned width_log, unsigned height, bool can_wait) {
	struct gpuBuf *buf;
	do {
		buf = gpuAlloc_(width_log, height);
		if (buf || !can_wait) return buf;
		unsigned fc = shared->frame_count;
		do {
			(void)sched_yield();
		} while (shared->frame_count == fc);
	} while (1);
}
		
void gpuFree(struct gpuBuf *buf) {
	assert(buf);
	free_buf(buf);
}

void gpuFreeFC(struct gpuBuf *buf, unsigned fc) {
	assert(buf);
	buf->free_after_fc = my_frame_count + fc;
	list_add_tail(&buf->fc_list, &fc_list);
}

gpuErr gpuSetBuf(gpuBufferType type, struct gpuBuf *buf, bool can_wait) {
	gpuCmdSetBuf setBuf = {
		.opcode = gpuSETBUF,
		.type = type,
	};
	assert(buf);
	if (type == gpuTxtBuffer && !is_power_of_2(buf->loc.height)) return gpuEPARAM;
	setBuf.loc = buf->loc;
	return gpuWrite(&setBuf, sizeof(setBuf), can_wait);
}
gpuErr gpuShowBuf(struct gpuBuf *buf, bool can_wait) {
	static gpuCmdShowBuf show = {
		.opcode = gpuSHOWBUF,
	};
	assert(my_frame_count >= shared->frame_count);
	if (my_frame_count-shared->frame_count >= GPU_DISPLIST_SIZE) return gpuENOSPC;
	show.loc = buf->loc;
	gpuErr err = gpuWrite(&show, sizeof(show), can_wait);
	if (gpuOK != err) goto sb_quit;
	my_frame_count ++;
sb_quit:	
	return err;
}

struct buffer_loc const *gpuBuf_get_loc(struct gpuBuf const *buf) {
	return &buf->loc;
}

void gpuWaitDisplay(void) {
	while (shared->frame_count < my_frame_count-1) {
		(void)sched_yield();
	}
}
