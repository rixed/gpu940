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
#include "console/console.h"
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
#	include <sys/time.h>
#	include <signal.h>
#endif

/*
 * Global Datas
 */

struct gpuShared *shared = (void *)(SHARED_PHYSICAL_ADDR-0x2000000U);	// from the 940T
struct ctx ctx;

#ifdef GP2X
volatile uint32_t *gp2x_regs = (void *)(0xC0000000U-0x2000000U);	// 32-bit version of the MMSP2 registers, from the 940T
#else
static SDL_Surface *sdl_screen;
#endif
static struct buffer_loc displist[GPU940_DISPLIST_SIZE+1];
static unsigned displist_begin = 0, displist_end = 0;	// same convention than for shared->cmds

/*
 * Private Functions
 */

static void display(struct buffer_loc const *loc) {
	// display current workingBuffer
	perftime_enter(PERF_DISPLAY, "display");
#ifdef GP2X
	static unsigned previous_width = 0;
	unsigned width = 1<<(1+loc->width_log);
	if (width != previous_width) {
//		gp2x_regs16[0x290c>>1] = width;
		gp2x_regs16[0x288a>>1] = width&0xffff;	// V scale ratio
		gp2x_regs16[0x288c>>1] = width>>16;
		gp2x_regs16[0x2892>>1] = width;
		previous_width = width;
	}
	uint32_t screen_addr = (uint32_t)&((struct gpuShared *)SHARED_PHYSICAL_ADDR)->buffers[loc->address + (ctx.view.winPos[1]<<loc->width_log) + ctx.view.winPos[0]];
//	gp2x_regs16[0x28a0>>1] = screen_addr&0xffff;	// odd	seams useless
//	gp2x_regs16[0x28a2>>1] = screen_addr>>16;	// odd
	gp2x_regs16[0x28a4>>1] = screen_addr&0xffff;	// even
	gp2x_regs16[0x28a6>>1] = screen_addr>>16;	// even
#else
	int32_t y;
	if (SDL_MUSTLOCK(sdl_screen) && SDL_LockSurface(sdl_screen) < 0) {
		fprintf(stderr, "Cannot lock screen\n");
		return;
	}
	// draw...
	for (y = ctx.view.winHeight; y--; ) {
		Uint32 *restrict dst = (Uint32*)((Uint8*)sdl_screen->pixels + y*sdl_screen->pitch);
		uint32_t *restrict src = &shared->buffers[loc->address + ((y+ctx.view.winPos[1])<<loc->width_log) + ctx.view.winPos[0]];
		memcpy(dst, src, ctx.view.winWidth<<2);
	}
	if (SDL_MUSTLOCK(sdl_screen)) SDL_UnlockSurface(sdl_screen);
	SDL_BlitSurface(sdl_console, NULL, sdl_screen, NULL);
	SDL_UpdateRect(sdl_screen, 0, 0, ctx.view.winWidth, ctx.view.winHeight);
#endif
	perftime_leave();
}

static void console_setup(void) {
	console_clear();
/*
	static char str[3] = { 'X', ':', 0 };
	for (unsigned y=0; y<32; y++) {
		for (unsigned x=0; x<8; x++) {
			str[0] = y*8+x;
			console_write(x*5, y, str);
			console_write_uint(x*5+2, y, 3, x+y*8);
		}
	}
	while(1) ; */
	console_setcolor(2);
	console_write(0, 0, "GPU940 v" VERSION " ErrFlg:");
	console_write(0, 1, "FrmCount:");
	console_write(0, 2, "FrmMiss :");
	console_write(0, 3, "Perfmeter        \xb3 nb calls \xb3 secs \xb3  %");
	console_write(0, 4, "\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc5\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc5\xc4\xc4\xc4\xc4\xc4\xc4\xc5\xc4\xc4\xc4\xc4\xc4");
}
static void console_stat(int y, int target) {
	struct perftime_stat st;
	perftime_stat(target, &st);
	unsigned c = st.average >= 40 ? 1:3;
	if (st.name) { console_setcolor(c); console_write(0, y, st.name); }
	console_setcolor(2); console_write(17, y, "\xb3");
	console_setcolor(c); console_write_uint(18, y, 9, st.nb_enter);
	console_setcolor(2); console_write(28, y, "\xb3");
	console_setcolor(c); console_write_uint(29, y, 5, st.cumul_secs>>10);
	console_setcolor(2); console_write(35, y, "\xb3");
	console_setcolor(c); console_write_uint(36, y, 3, st.average);
}
static void update_console(void) {
	static unsigned skip = 0;
	skip = (skip+1)&0x7;
	if (skip) return;
	console_setcolor(3);
	console_write_uint(20, 0, 3, shared->error_flags);
	console_write_uint(9, 1, 5, shared->frame_count);
	console_write_uint(9, 2, 5, shared->frame_miss);
	console_stat(5, PERF_DISPLAY);
	console_stat(6, PERF_POLY);
	console_stat(7, PERF_POLY_DRAW);
	console_stat(8, PERF_DIV);
}


static void vertical_interrupt(void) {
	if (displist_begin == displist_end) {
		if (shared->frame_count>0) shared->frame_miss ++;
	} else {
		display(displist+displist_begin);
		if (++ displist_begin >= sizeof_array(displist)) displist_begin = 0;
		shared->frame_count ++;
	}
	update_console();
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
	// TODO: on ARM use CLZ
	int32_t ret = 0;
	while ((1<<ret) < x) ret ++;
	return ret;
}

static void ctx_reset(void) {
	my_memset(&ctx, 0, sizeof ctx);
	ctx.location.out.width_log = next_power_of_2(SCREEN_WIDTH+2);
	ctx.location.out.height = SCREEN_HEIGHT+2;
	ctx.view.winPos[0] = ((1<<ctx.location.out.width_log)-SCREEN_WIDTH)>>1;
	ctx.view.winPos[1] = 1;
	ctx.view.clipMin[0] = (-SCREEN_WIDTH>>1)-1;
	ctx.view.clipMin[1] = (-SCREEN_HEIGHT>>1)-1;
	ctx.view.clipMax[0] = (SCREEN_WIDTH>>1)+1;
	ctx.view.clipMax[1] = (SCREEN_HEIGHT>>1)+1;
	ctx.view.winWidth = SCREEN_WIDTH;
	ctx.view.winHeight = SCREEN_HEIGHT;
	ctx.view.dproj = GPU_DEFAULT_DPROJ;
	reset_clipPlanes();
	ctx.view.nb_clipPlanes = 5;
}

static void shared_reset(void) {
	shared->cmds_begin = shared->cmds_end = 0;
	shared->frame_count = 0;
	shared->frame_miss = 0;
	shared->error_flags = 0;
#ifdef GP2X
	shared->osd_head[0] = 0;
	// FIXME: use SCREEN_WIDTH / SCREEN_HEIGHT
	shared->osd_head[1] = 0x868000efU;	// 1000 0110 1000 0000 0000 0000 1110 1111
	shared->osd_head[2] = 0x4000013fU;	// 0100 0000 0000 0000 0000 0001 0011 1111
#endif
	my_memset(shared->buffers, 0, sizeof(shared->buffers));
}

// All unsigned sizes are in words
static inline void copy32(uint32_t *restrict dest, uint32_t const *restrict src, unsigned size) {
	for ( ; size--; ) dest[size] = src[size];
}

static void flush_shared(void) {
#ifdef GP2X
	// drain the write buffer
	// reading from uncached memory is enought. shared, for exemple...
	volatile uint32_t __unused dummy = shared->error_flags;
#endif
}

static void read_from_cmdBuf(void *dest, size_t size_) {
	unsigned size = size_/sizeof(uint32_t);
	int overrun = (shared->cmds_begin+size) - sizeof_array(shared->cmds);
	if (overrun < 0) {
		copy32(dest, shared->cmds+shared->cmds_begin, size);
		shared->cmds_begin += size;
	} else {
		size_t chunk = size - overrun;
		copy32(dest, shared->cmds+shared->cmds_begin, chunk);
		copy32((uint32_t*)dest+chunk, shared->cmds, overrun);
		shared->cmds_begin = overrun;
	}
	flush_shared();
}

/*
 * Command processing
 */

static union {
	gpuCmdSetView setView;
	gpuCmdSetOutBuf setOutBuf;
	gpuCmdSetTxtBuf setTxtBuf;
	gpuCmdSetZBuf setZBuf;
	gpuCmdShowBuf showBuf;
} allCmds;

static void do_setView(void) {
	read_from_cmdBuf(&allCmds.setView, sizeof(allCmds.setView));
	ctx.view.dproj = allCmds.setView.dproj;
	ctx.view.clipMin[0] = allCmds.setView.clipMin[0];
	ctx.view.clipMin[1] = allCmds.setView.clipMin[1];
	ctx.view.clipMax[0] = allCmds.setView.clipMax[0];
	ctx.view.clipMax[1] = allCmds.setView.clipMax[1];
	ctx.view.winPos[0] = allCmds.setView.winPos[0];
	ctx.view.winPos[1] = allCmds.setView.winPos[1];
	ctx.view.winWidth = ctx.view.clipMax[0] - ctx.view.clipMin[0];
	ctx.view.winHeight = ctx.view.clipMax[1] - ctx.view.clipMin[1];
}
static void do_setOutBuf(void) {
	read_from_cmdBuf(&allCmds.setOutBuf, sizeof(allCmds.setOutBuf));
	my_memcpy(&ctx.location.out, &allCmds.setOutBuf.loc, sizeof(ctx.location.out));
}
static void do_setTxtBuf(void) {
	read_from_cmdBuf(&allCmds.setTxtBuf, sizeof(allCmds.setTxtBuf));
	my_memcpy(&ctx.location.txt, &allCmds.setTxtBuf.loc, sizeof(ctx.location.txt));
}
static void do_setZBuf(void) {
	read_from_cmdBuf(&allCmds.setZBuf, sizeof(allCmds.setZBuf));
	my_memcpy(&ctx.location.z, &allCmds.setZBuf.loc, sizeof(ctx.location.z));
}
static void do_showBuf(void) {
	read_from_cmdBuf(&allCmds.showBuf, sizeof(allCmds.showBuf));
	unsigned next_displist_end = displist_end + 1;
	if (next_displist_end >= sizeof_array(displist)) next_displist_end = 0;
	if (next_displist_end == displist_begin) {
		set_error_flag(gpuEDLIST);
		return;
	}
	displist[displist_end] = allCmds.showBuf.loc;
	displist_end = next_displist_end;
}
static void do_point(void) {}
static void do_line(void) {}
static void do_facet(void) {
	read_from_cmdBuf(&ctx.poly.cmdFacet, sizeof(ctx.poly.cmdFacet));
	// sanity checks
	if (ctx.poly.cmdFacet.size > sizeof_array(ctx.poly.vectors)) {
		set_error_flag(gpuEINT);
		return;
	}
	// fetch vectors informations
	for (unsigned v=0; v<ctx.poly.cmdFacet.size; v++) {
		read_from_cmdBuf(&ctx.poly.vectors[v].cmdVector, sizeof(gpuCmdVector));
	}
	if (clip_poly()) {
		draw_poly();
	}
}

static void fetch_command(void) {
	uint32_t first_word;
	copy32(&first_word, shared->cmds+shared->cmds_begin, 1);
	switch (first_word) {
		case gpuSETVIEW:
			do_setView();
			break;
		case gpuSETOUTBUF:
			do_setOutBuf();
			break;
		case gpuSETTXTBUF:
			do_setTxtBuf();
			break;
		case gpuSETZBUF:
			do_setZBuf();
			break;
		case gpuSHOWBUF:
			do_showBuf();
			break;
		case gpuPOINT:
			do_point();
			break;
		case gpuLINE:
			do_line();
			break;
		case gpuFACET:
			do_facet();
			break;
		default:
			set_error_flag(gpuEPARSE);
	}
}

static void __unused play_nodiv_anim(void) {
	int x = ctx.view.clipMin[0], y = ctx.view.clipMin[1];
	int dx = 1, dy = 1;
	while (1) {
		uint32_t *w = &shared->buffers[ctx.location.out.address + ((y+ctx.view.winPos[1]+ctx.view.winHeight/2)<<ctx.location.out.width_log) + x+ctx.view.winPos[0]+ctx.view.winWidth/2];
		*w = 0xffff;
		display(&ctx.location.out);
		x += dx;
		y += dy;
		if (x >= ctx.view.clipMax[0] || x <= ctx.view.clipMin[0]) dx = -dx;
		if (y >= ctx.view.clipMax[1] || y <= ctx.view.clipMin[1]) dy = -dy;
	}
}

static void __unused play_div_anim(void) {
	for (int64_t x = ctx.view.clipMin[0]; x<ctx.view.clipMax[0]; x++) {
		int64_t y = x? ctx.view.clipMax[1]/x : 0;
		uint32_t *w = &shared->buffers[ctx.location.out.address + ((y+ctx.view.winPos[1]+ctx.view.winHeight/2)<<ctx.location.out.width_log) + x+ctx.view.winPos[0]+ctx.view.winWidth/2];
		*w = 0xffff;
		display(&ctx.location.out);
	}
}

static void run(void) {
	//play_div_anim();
	//play_nodiv_anim();
	while (1) {
#ifndef GP2X
		if (SDL_QuitRequested()) return;
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

static inline void set_error_flag(unsigned err_mask);

#ifdef GP2X
void enable_irqs(void) {
	__asm__ volatile (
		"mrs r0, cpsr\n"
		"bic r0, r0, #0x80\n"
		"msr cpsr_c, r0\n"
		"nop\n"
		:::"r0"
	);
}
void disable_irqs(void) {
	__asm__ volatile (
		"mrs r0, cpsr\n"
		"orr r0, r0, #0x80\n"
		"msr cpsr_c, r0\n"
		"nop\n"
		:::"r0"
	);
}
void undef_instr(void) { }
void swi(void) { }
void prefetch_abrt(void) { }
void data_abrt(void) { }
void reserved(void) { }
void fiq(void) { }
void irq_handler(void) {
	int irq;
	for (irq=0; 0 == (gp2x_regs32[0x4510>>2]&(1U<<irq)) && irq < 32; irq++) ;
	if (irq == 32) {
		return;
	}
	if (irq == 0) {	// DISPINT
		// reset intflag
		vertical_interrupt();
		gp2x_regs16[0x2846>>1] |= 2;
	}
	gp2x_regs32[0x4500>>2] = 1U<<irq;
	gp2x_regs32[0x4510>>2] = 1U<<irq;
}
void mymain(void) {	// to please autoconf, we call this 'main'
	if (-1 == perftime_begin(0, NULL, 0)) goto quit;
	ctx_reset();
	shared_reset();
	// MLC_OVLAY_CNTR
	uint16_t v = gp2x_regs16[0x2880>>1];
	v &= 0x0400;	// keep reserved bit
	v |= 0x1001;	// bypath RGB gamma table and enable YUV region A
	gp2x_regs16[0x2880>>1] = v;
	// MLC_YUV_EFECT
	gp2x_regs16[0x2882>>1] = 0x0;	// no division between top and bottom
	// MLC_YUV_CNTL
	v = gp2x_regs16[0x2884>>1];
	v &= 0x3f;	// keep lower 10 bits, reserved
//	v |= 0x2000;	// skip alpha blending of region A
	gp2x_regs16[0x2884>>1] = v;
	// H Scaling of top region A
	gp2x_regs16[0x2886>>1] = 1024<<1;
	// Coordinates of region A
	gp2x_regs16[0x2896>>1] = 0;
	gp2x_regs16[0x2898>>1] = 319;
	gp2x_regs16[0x289a>>1] = 0;
	gp2x_regs16[0x289c>>1] = 239;
	gp2x_regs16[0x28e9>>1] = 239;	// not sure if it uses top's or bottom's vertical end
	// Disable all RGB layers
	v = gp2x_regs16[0x28da>>1];
	v &= 0x80;
	v |= 0x2a;
	gp2x_regs16[0x28da>>1] = v;
	// enable dithering
	gp2x_regs8[0x2946] = 1;	// this does nothing (??)
	// enhance contrast and brightness
	gp2x_regs16[0x2934>>1] = 0x033f;	
	// init the vertical interrupt
	gp2x_regs32[0x0808>>2] |= 1U;	// kernel don't want these
	gp2x_regs32[0x4504>>2] = 0;	// IRQs not FIQs
	gp2x_regs32[0x4508>>2] = ~1U;	// mask all but DISPINT
	gp2x_regs32[0x450c>>2] = 0;	// kernel does this
	gp2x_regs32[0x4500>>2] = -1;	// kernel does this
	gp2x_regs32[0x4510>>2] = -1;	// kernel does this
	gp2x_regs16[0x3b42>>1] = -1;	// DUALINT940
	// enable VSYNC IRQ in display controler
	gp2x_regs16[0x2846>>1] |= 0x20;
	// and now, enable IRQs
	enable_irqs();
	console_begin();
	console_setup();
	run();
	console_end();
quit:;
	// TODO halt the 940
}
#else
static void alrm_handler(int dummy) {
	(void)dummy;
	vertical_interrupt();
}
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
	shared_reset();
	// Open SDL screen of default window size
	if (0 != SDL_Init(SDL_INIT_VIDEO)) return EXIT_FAILURE;
	sdl_screen = SDL_SetVideoMode(ctx.view.winWidth, ctx.view.winHeight, 32, SDL_SWSURFACE);
	if (! sdl_screen) return EXIT_FAILURE;
	// use itimer for simulation of vertical interrupt
	if (0 != sigaction(SIGALRM, &(struct sigaction){ .sa_handler = alrm_handler }, NULL)) {
		perror("sigaction");
		return EXIT_FAILURE;
	}
	struct itimerval itimer = {
		.it_interval = {
			.tv_sec = 0,
			.tv_usec = 50000,
		},
		.it_value = {
			.tv_sec = 0,
			.tv_usec = 50000,
		},
	};
	if (0 != setitimer(ITIMER_REAL, &itimer, NULL)) {
		perror("setitimer");
		return EXIT_FAILURE;
	}
	console_begin();
	console_setup();
	run();
	console_end();
	SDL_Quit();
	perftime_end();
	return 0;
}
#endif

