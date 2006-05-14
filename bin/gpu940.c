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
/* Stand alone program that mimics a GPU.
 * Communication with the client is done via shared memory.
 */

#include "gpu940i.h"
#include "gpu940.h"
#include "gcc.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#ifndef GP2X
#	include <stdlib.h>
#	include <stdio.h>
#	include <time.h>
#	include <string.h>
#	include <stdio.h>
#	include <sched.h>
#	include <SDL/SDL.h>
#endif

/*
 * Global Datas
 */

struct gpu940_shared *shared = (void *)(SHARED_PHYSICAL_ADDR-0x2000000U);	// from the 940T
#define screenLen (shared->screenSize[1]<<shared->screenWidth_log)
#define nbMaxVideoBuffers (sizeof_array(shared->videoBuffers)/ScreenLen)

struct ctx ctx;

#ifdef GP2X
static volatile uint32_t *gp2x_regs = (void *)(0xC0000000U-0x2000000U);	// 32-bit version of the MMSP2 registers, from the 940T
#	define gp2x_regs32 gp2x_regs
#	define gp2x_regs16 ((volatile uint16_t *)gp2x_regs)	// don't forgot volatile here or be prepared to strange GCC "optims"
#	define gp2x_regs8 ((volatile uint8_t *)gp2x_regs)	// don't forgot volatile here or be prepared to strange GCC "optims"
#else
static SDL_Surface *sdl_screen;
#endif

/*
 * Private Functions
 */

static void display(void) {
	// display current workingBuffer
//	perftime_enter(PERF_DISPLAY, "display");
#ifdef GP2X
	uint16_t vsync = gp2x_regs16[0x2804>>1] & 1;
	while (gp2x_regs16[0x1182>>1] & (1<<4) == !vsync) ;
	while ((gp2x_regs16[0x1182>>1]&(1<<4)) == vsync) ;
	uint32_t screen_addr = (uint32_t)&((struct gpu940_shared *)SHARED_PHYSICAL_ADDR)->videoBuffers[shared->workingBuffer+(ctx.view.winPos[1]<<shared->screenWidth_log)+ctx.view.winPos[0]];
	gp2x_regs16[0x290e>>1] = screen_addr&0xffff; gp2x_regs16[0x2912>>1] = screen_addr&0xffff;
	gp2x_regs16[0x2910>>1] = screen_addr>>16; gp2x_regs16[0x2914>>1] = screen_addr>>16;
#else
	int32_t y;
	if (SDL_MUSTLOCK(sdl_screen) && SDL_LockSurface(sdl_screen) < 0) {
		fprintf(stderr, "Cannot lock screen\n");
		return;
	}
	// draw...
	for (y = shared->screenSize[1]; y--; ) {
		Uint16 *restrict dst = (Uint16 *)((Uint8 *)sdl_screen->pixels + y*(shared->screenSize[0]<<1));
		uint16_t *restrict src = &shared->videoBuffers[shared->workingBuffer + ((y+ctx.view.winPos[1])<<shared->screenWidth_log) + ctx.view.winPos[0]];
		memcpy(dst, src, shared->screenSize[0]<<1);
	}
	if (SDL_MUSTLOCK(sdl_screen)) SDL_UnlockSurface(sdl_screen);
	SDL_UpdateRect(sdl_screen, 0, 0, shared->screenSize[0], shared->screenSize[1]);
#endif
//	perftime_leave();
}

static void reset_clipPlanes(void) {
	int32_t d = 1<<ctx.view.dproj;
	my_memset(ctx.view.clipPlanes, 0, sizeof(ctx.view.clipPlanes[0])*5);
	ctx.view.clipPlanes[0].normal[2] = -1<<8;	// clipPlane 0 is z_near
	ctx.view.clipPlanes[0].origin[2] = -1<<8;
	ctx.view.clipPlanes[1].normal[0] = -d;	// clipPlane 1 is rightmost
	ctx.view.clipPlanes[1].normal[2] = -ctx.view.clipMax[0];
	ctx.view.clipPlanes[2].normal[1] = -d;	// clipPlane 2 is upper
	ctx.view.clipPlanes[2].normal[2] = -ctx.view.clipMax[1];
	ctx.view.clipPlanes[3].normal[0] = d;	// clipPlane 3 is leftmost
	ctx.view.clipPlanes[3].normal[2] = ctx.view.clipMin[0];
	ctx.view.clipPlanes[4].normal[1] = d;	// clipPlane 4 is bottom
	ctx.view.clipPlanes[4].normal[2] = ctx.view.clipMin[1];
}

static int32_t next_power_of_2(int32_t x) {
	int32_t ret = 0;
	while ((1<<ret) < x) ret ++;
	return ret;
}

static void ctx_reset(void) {
#	define SCREEN_WIDTH 320
#	define SCREEN_HEIGHT 240
	my_memset(&ctx, 0, sizeof ctx);
	shared->screenSize[0] = SCREEN_WIDTH;
	shared->screenSize[1] = SCREEN_HEIGHT;
	shared->screenWidth_log = next_power_of_2(shared->screenSize[0]);
	shared->workingBuffer = 0;
	ctx.view.winPos[0] = ((1<<shared->screenWidth_log)-shared->screenSize[0])>>1;
	ctx.view.winPos[1] = 1;
	ctx.view.clipMin[0] = (-shared->screenSize[0]>>1)-1;
	ctx.view.clipMin[1] = (-shared->screenSize[1]>>1);
	ctx.view.clipMax[0] = (shared->screenSize[0]>>1);
	ctx.view.clipMax[1] = (shared->screenSize[1]>>1)-2;
	ctx.view.dproj = GPU_DEFAULT_DPROJ;
	reset_clipPlanes();
	ctx.view.nb_clipPlanes = 5;
}

static void cmd_reset(void) {
	shared->cmds_begin = shared->cmds_end = 0;
}

static void copy_32(void *restrict dest_, void const *restrict src_, size_t size) {
	int32_t *restrict dest = (int32_t *)dest_;
	int32_t const *restrict src = (int32_t const*)src_;
	assert(0 == (size&3));
	if (! size) return;
	for (unsigned i=(size>>2); i--; ) {
		dest[i] = src[i];
	}
}

static void flush_shared(void) {
#ifdef GP2X
	// drain the write buffer
	// reading from uncached memory is enought. shared, for exemple...
	volatile uint32_t __unused dummy = shared->error_flags;
#endif
}

static void read_from_cmdBuf(void *dest, size_t size) {
	ssize_t overrun = (shared->cmds_begin+size) - sizeof(shared->cmds);
	if (overrun < 0) {
		copy_32(dest, shared->cmds+shared->cmds_begin, size);
		shared->cmds_begin += size;
	} else {
		size_t chunk = size - overrun;
		copy_32(dest, shared->cmds+shared->cmds_begin, chunk);
		copy_32((char *)dest+chunk, shared->cmds, overrun);
		shared->cmds_begin = overrun;
	}
	flush_shared();
}

/*
 * Command processing
 */

static void do_reconfigure(void) {
	static gpu940_cmdCfg cmd;
	read_from_cmdBuf(&cmd, sizeof(cmd));
	// TODO
}

static void do_swap_buffers(void) {
	// TODO: distinguer le swap de buffer et l'affichage : il doit y avoir un working buffer et un displayed buffer,
	// displayed buffer avancant au rithme des VBLs (si possible)
	static gpu940_cmdSwap cmd;
	read_from_cmdBuf(&cmd, sizeof(cmd));
	display();
	uint32_t new_wb = shared->workingBuffer + screenLen;
	if (new_wb+screenLen > sizeof_array(shared->videoBuffers)) new_wb = 0;
	shared->workingBuffer = new_wb;
}

static void do_draw_facet(void) {
	read_from_cmdBuf(&ctx.poly.cmdFacet, sizeof ctx.poly.cmdFacet);
	// sanity checks
	if (ctx.poly.cmdFacet.size > sizeof_array(ctx.poly.vectors)) {
		set_error_flag(gpu940_EINT);
		return;
	}
	// fetch vectors informations
	for (unsigned v=0; v<ctx.poly.cmdFacet.size; v++) {
		read_from_cmdBuf(&ctx.poly.vectors[v].cmdVector, sizeof(gpu940_cmdVector));
	}
	if (clip_poly()) {
		draw_poly();
	}
}

static void fetch_command(void) {
	uint32_t first_word;
	copy_32(&first_word, shared->cmds+shared->cmds_begin, sizeof(first_word));
	if (0 == first_word) {
		do_reconfigure();
	} else if (1 == first_word) {
		do_swap_buffers();
	} else if (first_word < MAX_FACET_SIZE) {	// it's a facet
		do_draw_facet();
	} else {	// WTF ?
		set_error_flag(gpu940_EPARSE);
		// we stay in place : gpu must be reseted.
	}
}

static void __unused play_nodiv_anim(void) {
	int x = ctx.view.clipMin[0], y = ctx.view.clipMin[1];
	int dx = 1, dy = 1;
	while (1) {
		uint16_t *w = &shared->videoBuffers[shared->workingBuffer + ((y+ctx.view.winPos[1]+(shared->screenSize[1]>>1))<<shared->screenWidth_log) + x+ctx.view.winPos[0]+(shared->screenSize[0]>>1)];
		*w = 0xffff;
		display();
		x += dx;
		y += dy;
		if (x >= ctx.view.clipMax[0] || x <= ctx.view.clipMin[0]) dx = -dx;
		if (y >= ctx.view.clipMax[1] || y <= ctx.view.clipMin[1]) dy = -dy;
	}
	
}

static void __unused play_div_anim(void) {
	for (int64_t x = ctx.view.clipMin[0]; x<ctx.view.clipMax[0]; x++) {
		int64_t y = x? ctx.view.clipMax[1]/x : 0;
		uint16_t *w = &shared->videoBuffers[shared->workingBuffer + ((y+ctx.view.winPos[1]+(shared->screenSize[1]>>1))<<shared->screenWidth_log) + x+ctx.view.winPos[0]+(shared->screenSize[0]>>1)];
		*w = 0xffff;
		display();
	}
}

static void run(void) {
	cmd_reset();
	//play_nodiv_anim();
	//play_div_anim();
	while (1) {
#ifndef GP2X
		if (SDL_QuitRequested()) return;
#endif
#ifdef GP2X
		if (! (gp2x_regs16[0x1184>>1] & 0x0100)) {
//			perftime_stat_print_all(1);
		}
#endif
		if (shared->cmds_end == shared->cmds_begin) {
#ifdef GP2X
			// sleep, downclock, whatever. TODO
#else
			(void)sched_yield();
#endif
		} else {
			fetch_command();
		}
	}
}

void set_error_flag(unsigned err_mask) {
	shared->error_flags |= err_mask;	// TODO : use a bit atomic set instruction
}
//static inline void set_error_flag(unsigned err_mask);

#ifdef GP2X
void mymain(void) {	// to please autoconf, we call this 'main'
	ctx_reset();
	// set video mode
	gp2x_regs16[0x28da>>1] = 0x004AB; // 16bpp, only region 1 activated
	gp2x_regs16[0x290c>>1] = 1<<(1+shared->screenWidth_log);	// width
//	gp2x_regs16[0x2906>>1] = 1024;
//	gp2x_regs32[0x2908>>2] = 1<<(shared->screenWidth_log+1);	// aparently, scale-factors are reset to width ?
	my_memset(shared->videoBuffers, 0x84108410, sizeof(shared->videoBuffers));
//	if (-1 == perftime_begin(0, NULL, 0)) goto quit;
	run();
//quit:;
	// halt the 940
}
#if 0
dans un autre fichier objet, faire un main qui lui sera exécuté sur le 920, avec dans un section à part le code
complet du 940,avec un symbole donnant le début et la fin
quit:; // quit
	perftime_stat_print_all(1);
	perftime_end();
	char *menuDir = "/usr/gp2x";
	char *menuCmd = "/usr/gp2x/gp2xmenu";
	__asm__ volatile (
		"mov r0, %0\n"      // directory
		"swi #0x90000C\n"   // chdir
		"mov r0, %1\n"      // program to execute
		"mov r1, #0\n"      // arg 2 = NULL
		"mov r2, #0\n"      // arg 3 = NULL
		"swi #0x90000B\n"   // execve
		:   // no output
		: "r"(menuDir), "r"(menuCmd)  // %0 and %1 inputs
		: "r0", "r1", "r2"     // registers we clobber
	);	
}
#endif
#else
int main(void) {
	if (-1 == perftime_begin(0, NULL, 0)) return EXIT_FAILURE;
	int fd = open(CMDFILE, O_RDWR|O_CREAT|O_TRUNC, 0644);
	if (-1 == fd ||
		-1 == lseek(fd, sizeof(*shared)-1, SEEK_SET) ||
		-1 == write(fd, &shared, 1)
	) {
		perror("Cannot create mmaped file");
		return EXIT_FAILURE;
	}
	shared = mmap(NULL, sizeof(*shared), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (MAP_FAILED == shared) {
		perror("mmap");
		return EXIT_FAILURE;
	}
	ctx_reset();
	// Open SDL screen of default window size
	if (0 != SDL_Init(SDL_INIT_VIDEO)) return EXIT_FAILURE;
	sdl_screen = SDL_SetVideoMode(shared->screenSize[0], shared->screenSize[1], 16, SDL_SWSURFACE);
	if (! sdl_screen) return EXIT_FAILURE;
	run();
	SDL_Quit();
	perftime_stat_print_all(1);
	perftime_end();
	return 0;
}
#endif

