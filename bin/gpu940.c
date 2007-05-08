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
static struct buffer_loc displist[GPU_DISPLIST_SIZE+1];
static unsigned displist_begin = 0, displist_end = 0;	// same convention than for shared->cmds

/*
 * Private Functions
 */

static void display(struct buffer_loc const *loc) {
	// display current workingBuffer
	int previous_target = perftime_target();
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
	for (y = SCREEN_HEIGHT; y--; ) {
		Uint32 *restrict dst = (Uint32*)((Uint8*)sdl_screen->pixels + y*sdl_screen->pitch);
		uint32_t *restrict src = &shared->buffers[loc->address + ((y+ctx.view.winPos[1])<<loc->width_log) + ctx.view.winPos[0]];
		memcpy(dst, src, SCREEN_WIDTH<<2);
	}
	if (SDL_MUSTLOCK(sdl_screen)) SDL_UnlockSurface(sdl_screen);
	if (console_enabled) SDL_BlitSurface(sdl_console, NULL, sdl_screen, NULL);
	SDL_UpdateRect(sdl_screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
	perftime_enter(previous_target, NULL);
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
	console_write(25, 0, "MHz:");
	console_write(0, 1, "FrmCount:");
	console_write(20, 1, "FrmMiss :");
	console_write(0, 2, "ProjCach:");
	console_write(0, 3, "Perfmeter        \xb3  nb enter  \xb3 lavg");
	console_write(0, 4, "\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc5\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc5\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4");
}
static void console_stat(int y, int target) {
#	ifndef NDEBUG
	struct perftime_stat st;
	perftime_stat(target, &st);
	unsigned c = st.load_avg >= 400 ? 1:3;
	if (st.name) { console_setcolor(c); console_write(0, y, st.name); }
	console_setcolor(2); console_write(17, y, "\xb3");
	console_setcolor(c); console_write_uint(18, y, 11, st.nb_enter);
	console_setcolor(2); console_write(30, y, "\xb3");
	console_setcolor(c); console_write_uint(31, y, 5, (100*st.load_avg)>>10);
#	else
	(void)y;
	(void)target;
#	endif
}
static void update_console(void) {
	if (! console_enabled) return;
	console_setcolor(3);
	console_write_uint(20, 0, 3, shared->error_flags);
	console_write_uint(9, 1, 5, shared->frame_count);
	console_write_uint(29, 1, 5, shared->frame_miss);
	console_write_uint(9, 2, 5, proj_cache_ratio());
	console_stat(5, PERF_WAITCMD);
	console_stat(6, PERF_CMD);
	console_stat(7, PERF_CLIP);
	console_stat(8, PERF_CULL);
	console_stat(9, PERF_POLY);
	console_stat(10, PERF_POLY_DRAW);
	console_stat(11, PERF_RECTANGLE);
	console_stat(12, PERF_DISPLAY);
	console_stat(13, PERF_DIV);
	console_stat(14, PERF_OTHER);
}

static void vertical_interrupt(void) {
	if (displist_begin == displist_end) {
		if (shared->frame_count>0) shared->frame_miss ++;
	} else {
		display(displist+displist_begin);
		if (++ displist_begin >= sizeof_array(displist)) displist_begin = 0;
		shared->frame_count ++;
		static int skip_upd_console = 0;
		if ( ++skip_upd_console > 10 ) {
			update_console();
			skip_upd_console = 0;
		}
	}
	perftime_async_upd();
}

static void reset_clipPlanes(void) {
	int32_t d = 1<<ctx.view.dproj;
	my_memset(ctx.view.clipPlanes, 0, sizeof(ctx.view.clipPlanes[0])*5);
	ctx.view.clipPlanes[0].normal[2] = 1<<16;	// clipPlane 0 is z_near
	ctx.view.clipPlanes[0].origin[2] = 256;
	ctx.view.clipPlanes[1].normal[0] = -d<<16;	// clipPlane 1 is rightmost
	ctx.view.clipPlanes[1].normal[2] = ctx.view.clipMax[0]<<16;
	Fix_normalize(ctx.view.clipPlanes[1].normal);
	ctx.view.clipPlanes[2].normal[1] = -d<<16;	// clipPlane 2 is upper
	ctx.view.clipPlanes[2].normal[2] = ctx.view.clipMax[1]<<16;
	Fix_normalize(ctx.view.clipPlanes[2].normal);
	ctx.view.clipPlanes[3].normal[0] = d<<16;	// clipPlane 3 is leftmost
	ctx.view.clipPlanes[3].normal[2] = -ctx.view.clipMin[0]<<16;
	Fix_normalize(ctx.view.clipPlanes[3].normal);
	ctx.view.clipPlanes[4].normal[1] = d<<16;	// clipPlane 4 is bottom
	ctx.view.clipPlanes[4].normal[2] = -ctx.view.clipMin[1]<<16;
	Fix_normalize(ctx.view.clipPlanes[4].normal);
}

#if 0
static uint32_t next_power_of_2(uint32_t x) {
	// TODO: on ARM use CLZ
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}
#endif

static uint32_t next_log_2(uint32_t x) {
	uint32_t ret = 0;
	assert(! (x & (1U<<31)));
	while (x > (1U<<ret)) ret++;
	return ret;
}

static void ctx_code_buf_reset(void)
{
	for (unsigned b=0; b<sizeof_array(ctx.location.buffer_loc); b++) {
		ctx.code.buff_addr[b] = &shared->buffers[ctx.location.buffer_loc[b].address];
	}
	ctx.code.out2zb = ctx.location.buffer_loc[gpuZBuffer].address - ctx.location.buffer_loc[gpuOutBuffer].address;
}

static void ctx_code_reset(void)
{
	ctx_code_buf_reset();
	jit_invalidate();
}

static void ctx_reset(void) {
	my_memset(&ctx, 0, sizeof ctx);
	ctx.location.buffer_loc[gpuOutBuffer].width_log = next_log_2(SCREEN_WIDTH+6);
	ctx.location.buffer_loc[gpuOutBuffer].height = SCREEN_HEIGHT+6;
	ctx.view.winPos[0] = GPU_DEFAULT_WINPOS0;
	ctx.view.winPos[1] = GPU_DEFAULT_WINPOS1;
	ctx.view.clipMin[0] = GPU_DEFAULT_CLIPMIN0;
	ctx.view.clipMin[1] = GPU_DEFAULT_CLIPMIN1;
	ctx.view.clipMax[0] = GPU_DEFAULT_CLIPMAX0;
	ctx.view.clipMax[1] = GPU_DEFAULT_CLIPMAX1;
	ctx.view.winWidth = ctx.view.clipMax[0] - ctx.view.clipMin[0];
	ctx.view.winHeight = ctx.view.clipMax[1] - ctx.view.clipMin[1];
	ctx.view.dproj = GPU_DEFAULT_DPROJ;
	ctx.rendering.mode.named.z_mode = gpu_z_off;
	ctx.rendering.mode.named.rendering_type = rendering_flat;
	ctx.rendering.mode.named.use_key = 0;
	ctx.rendering.mode.named.use_intens = 0;
	ctx.rendering.mode.named.use_txt_blend = 0;
	ctx.rendering.mode.named.perspective = 1;	// so that we don't have to prepare a JIT
	ctx.rendering.mode.named.blend_coef = 0;
	ctx.rendering.mode.named.write_out = 1;
	ctx.rendering.mode.named.write_z = 1;
	reset_clipPlanes();
	ctx.view.nb_clipPlanes = 5;
	ctx_code_reset();
}

static void shared_soft_reset(void) {
	shared->frame_count = 0;
	shared->frame_miss = 0;
	shared->error_flags = 0;
}
static void shared_reset(void) {
	shared->cmds_begin = shared->cmds_end = 0;
#ifdef GP2X
	shared->osd_head[0] = 0;
	// FIXME: use SCREEN_WIDTH / SCREEN_HEIGHT
	shared->osd_head[1] = 0x868000efU;	// 1000 0110 1000 0000 0000 0000 1110 1111
	shared->osd_head[2] = 0x4000013fU;	// 0100 0000 0000 0000 0000 0001 0011 1111
#endif
	shared_soft_reset();
}

// All unsigned sizes are in words
static inline void copy32(uint32_t *restrict dest, uint32_t const *restrict src, unsigned size) {
	for ( ; size--; ) dest[size] = src[size];
}

static void flush_shared(void) {
#ifdef GP2X
	// drain the write buffer
	// reading from uncached memory is enought. IO for exemple...
	volatile uint32_t GCCunused dummy = gp2x_regs[0];
#endif
}

static void *get_cmd(void) {
	return shared->cmds+shared->cmds_begin;
}
static void next_cmd(size_t size_) {
	unsigned size = size_/sizeof(uint32_t);
	shared->cmds_begin += size;
	assert(shared->cmds_begin < sizeof_array(shared->cmds));
	flush_shared();
}

static void video_reset(void) {
#ifdef GP2X
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
//	gp2x_regs16[0x2934>>1] = 0x033f;	
#endif
}

static void reset_prepared_jit(void)
{
#	if defined(GP2X) || defined(TEST_RASTERIZER)
	if (! ctx.rendering.mode.named.perspective) {
		ctx.rendering.rasterizer = jit_prepare_rasterizer();
	}
#	endif
}

/*
 * Command processing
 */

static void do_setView(void)
{
	gpuCmdSetView const *const setView = (gpuCmdSetView *)get_cmd();
	ctx.view.dproj = setView->dproj;
	ctx.view.clipMin[0] = setView->clipMin[0];
	ctx.view.clipMin[1] = setView->clipMin[1];
	ctx.view.clipMax[0] = setView->clipMax[0];
	ctx.view.clipMax[1] = setView->clipMax[1];
	ctx.view.winPos[0] = setView->winPos[0];
	ctx.view.winPos[1] = setView->winPos[1];
	ctx.view.winWidth = ctx.view.clipMax[0] - ctx.view.clipMin[0];
	ctx.view.winHeight = ctx.view.clipMax[1] - ctx.view.clipMin[1];
	next_cmd(sizeof(*setView));
	reset_clipPlanes();
}
static void do_setUsrClipPlanes(void)
{
	gpuCmdSetUserClipPlanes const *const setCP = (gpuCmdSetUserClipPlanes *)get_cmd();
	ctx.view.nb_clipPlanes = 5 + setCP->nb_planes;
	for (unsigned cp=0; cp<setCP->nb_planes; cp++) {
		my_memcpy(ctx.view.clipPlanes+5+cp, setCP->planes+cp, sizeof(ctx.view.clipPlanes[0]));
	}
	next_cmd(sizeof(*setCP));
}
static void do_setBuf(void)
{
	gpuCmdSetBuf const *const setBuf = (gpuCmdSetBuf *)get_cmd();
	if (setBuf->type >= GPU_NB_BUFFER_TYPES) {
		set_error_flag(gpuEPARAM);
		goto dsb_quit;
	}
	my_memcpy(&ctx.location.buffer_loc[setBuf->type], &setBuf->loc, sizeof(*ctx.location.buffer_loc));
	if (ctx.location.buffer_loc[setBuf->type].width_log > 15) {
		set_error_flag(gpuEPARAM);
		goto dsb_quit;
	}
	if (setBuf->type == gpuTxtBuffer) {
		ctx.location.txt_width_mask = (1U<<setBuf->loc.width_log)-1;
		ctx.location.txt_height_mask = setBuf->loc.height-1;
		ctx.location.txt_height_log = next_log_2(setBuf->loc.height);
		if ((1U<<ctx.location.txt_height_log) != setBuf->loc.height) {
			set_error_flag(gpuEPARAM);
			goto dsb_quit;
		}
		reset_prepared_jit();
	} else if (setBuf->type == gpuOutBuffer) {
		ctx.location.out_start = location_winPos(gpuOutBuffer, 0, 0);
	}
	ctx_code_buf_reset();
dsb_quit:
	next_cmd(sizeof(*setBuf));
}
static void do_showBuf(void)
{
	gpuCmdShowBuf const *const showBuf = (gpuCmdShowBuf *)get_cmd();
	unsigned next_displist_end = displist_end + 1;
	if (next_displist_end >= sizeof_array(displist)) next_displist_end = 0;
	if (next_displist_end == displist_begin) {
		set_error_flag(gpuEDLIST);
		goto dwb_quit;
	}
	displist[displist_end] = showBuf->loc;
	displist_end = next_displist_end;
#ifndef GP2X
//	vertical_interrupt();
#endif
dwb_quit:
	next_cmd(sizeof(*showBuf));
}
static void do_point(void)
{
	gpuCmdPoint const *const point = (gpuCmdPoint *)get_cmd();
	ctx.points.vectors[0].cmd = (gpuCmdVector *)(point+1);
	if (clip_point()) {
		draw_point(point->color);
	}
	next_cmd(sizeof(*point) + sizeof(gpuCmdVector));
}
static void do_line(void)
{
	gpuCmdLine const *const line = (gpuCmdLine *)get_cmd();
	gpuCmdVector *const vec = (gpuCmdVector *)(line+1);
	ctx.points.vectors[0].cmd = vec;
	ctx.points.vectors[1].cmd = vec+1;
	if (clip_line()) {
		ctx.code.color = line->color;
		draw_line();
	}
	next_cmd(sizeof(*line) + 2*sizeof(*vec));
}
static void do_facet(void)
{
	// Warning: don't skip any vector here (without positionning err_flag) or future same_as hints will be wrong.
	ctx.poly.cmd = get_cmd();
	// sanity checks
	size_t to_skip = sizeof(gpuCmdFacet) + ctx.poly.cmd->size*sizeof(gpuCmdVector);	// we are about to change facet size
	if (ctx.poly.cmd->size > sizeof_array(ctx.points.vectors)) {
		set_error_flag(gpuEINT);
		goto df_quit;
	}
	if (ctx.poly.cmd->size < 3) {
		set_error_flag(gpuEPARAM);
		goto df_quit;
	}
	// fetch vectors informations
	for (unsigned v=0; v<ctx.poly.cmd->size; v++) {
		ctx.points.vectors[v].cmd = (gpuCmdVector *)(ctx.poly.cmd+1) + v;
	}
	if (clip_poly() && cull_poly()) {
		ctx.code.color = ctx.poly.cmd->color;
		if (ctx.rendering.mode.named.perspective) draw_poly_persp();
		else draw_poly_nopersp();
	}
df_quit:
	next_cmd(to_skip);
}
static void do_rect(void)
{
	int previous_target = perftime_target();
	perftime_enter(PERF_RECTANGLE, "rectangle");
	gpuCmdRect const *const rect = (gpuCmdRect *)get_cmd();
	// TODO: add clipping against winPos ?
	uint32_t *dst = rect->relative_to_window ?
		location_winPos(rect->type, rect->pos[0], rect->pos[1]) :
			location_pos(rect->type, rect->pos[0], rect->pos[1]);
	for (unsigned h=rect->height; h--; ) {
		my_memset_words(dst, rect->value, rect->width);
		dst += 1 << ctx.location.buffer_loc[rect->type].width_log;
	}
	next_cmd(sizeof(*rect));
	perftime_enter(previous_target, NULL);
}
static void do_mode(void)
{
	gpuCmdMode const *const mode = (gpuCmdMode *)get_cmd();
	ctx.rendering.mode.flags = mode->mode.flags;
	// To avoid some tests here and there and reduce the number of flags combinations, clear unused flags
	if (ctx.rendering.mode.named.rendering_type != rendering_text) {
		ctx.rendering.mode.named.use_txt_blend = 0;
		if (ctx.rendering.mode.named.rendering_type == rendering_smooth) {
			ctx.rendering.mode.named.use_intens = 0;
		}
	} else if (ctx.rendering.mode.named.use_txt_blend) {
		ctx.rendering.mode.named.blend_coef = 0;
	}
	next_cmd(sizeof(*mode));
	reset_prepared_jit();
}
static void do_dbg(void)
{
	gpuCmdDbg const *const dbg = (gpuCmdDbg *)get_cmd();
	if (dbg->console_enable) {
		console_enable();
	} else {
		console_disable();
	}
	next_cmd(sizeof(*dbg));
}
static void do_reset(void)
{
	(void)get_cmd();
	next_cmd(sizeof(gpuCmdReset));
	proj_cache_reset();
	ctx_reset();
	shared_soft_reset();
	video_reset();
	perftime_reset();
	perftime_enter(PERF_WAITCMD, "idle");
}
static void do_rewind(void)
{
	shared->cmds_begin = 0;
	flush_shared();
}

static void fetch_command(void)
{
	unsigned previous_target = perftime_target();
	perftime_enter(PERF_CMD, "cmd");
	switch ((gpuOpcode)shared->cmds[shared->cmds_begin]) {
		case gpuREWIND:
			do_rewind();
			break;
		case gpuRESET:
			do_reset();
			break;
		case gpuSETVIEW:
			do_setView();
			break;
		case gpuSETUSRCLIPPLANES:
			do_setUsrClipPlanes();
			break;
		case gpuSETBUF:
			do_setBuf();
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
		case gpuRECT:
			do_rect();
			break;
		case gpuMODE:
			do_mode();
			break;
		case gpuDBG:
			do_dbg();
			break;
		default:
			set_error_flag(gpuEPARSE);
	}
	perftime_enter(previous_target, NULL);
}

#define FULL_THROTTLE   8
#define VERY_FAST       7
#define REASONABLY_FAST 6
#define MODERABLY_FAST  5
#define ECONOMIC        4
#define MODERATED       3
#define QUITE_SLOW      2
#define BLOATED         1
#define SLOW_AS_HELL    0
void set_speed(unsigned s)
{
#ifdef GP2X
	static unsigned previous_speed = ~0U;
	static struct {
		unsigned mhz;
		unsigned magik;
	} speeds[FULL_THROTTLE+1] = {
		{ 50, 0x6503 },
		{ 75, 0x4902 },
		{ 100, 0x6502 },
		{ 125, 0x3c01 },
		{ 150, 0x4901 },
		{ 175, 0x3F04 },
		{ 200, 0x4904 },
		{ 225, 0x5304 },
		{ 250, 0x5D04 }
	};
	if (previous_speed == s) return;
	gp2x_regs16[0x0910>>1] = speeds[s].magik;
	while (gp2x_regs16[0x0902>>1] & 1) ;
	console_setcolor(3); console_write_uint(29, 0, 3, speeds[s].mhz);
	previous_speed = s;
#else
	(void)s;
#endif
}
static void run(void)
{
#ifdef GP2X
	unsigned wait = 0;
#endif
	perftime_enter(PERF_WAITCMD, "idle");
	while (1) {
#ifndef GP2X
		if (SDL_QuitRequested()) return;
#endif
		if (shared->cmds_end == shared->cmds_begin) {
#ifdef GP2X
#define TIME_FOR_A_BREAK 0x1000000
			if (wait == TIME_FOR_A_BREAK) {
				set_speed(SLOW_AS_HELL);
			}
			if (wait <= TIME_FOR_A_BREAK) {
				wait ++;
			}
			for (register volatile int i=100; i>0; i--) ;	// halt before reading RAM again
#else
			(void)sched_yield();
#endif
		} else {
#ifdef GP2X
			if (wait >= TIME_FOR_A_BREAK) {
				set_speed(FULL_THROTTLE);
			}
			wait = 0;
#endif
			fetch_command();
		}
	}
}

extern inline void set_error_flag(unsigned err_mask);
extern inline uint32_t *location_pos(gpuBufferType type, int32_t x, int32_t y);
extern inline uint32_t *location_winPos(gpuBufferType type, int32_t x, int32_t y);

#ifdef GP2X
void enable_irqs(void)
{
	__asm__ volatile (
		"mrs r0, cpsr\n"
		"bic r0, r0, #0x80\n"
		"msr cpsr_c, r0\n"
		"nop\n"
		:::"r0"
	);
}
void disable_irqs(void)
{
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
void irq_handler(void)
{
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

void mymain(void)
{
	if (-1 == perftime_begin()) goto quit;
	video_reset();
	// init the vertical interrupt
	gp2x_regs32[0x0808>>2] |= 1U;	// kernel don't want these
	gp2x_regs32[0x4504>>2] = 0;	// IRQs not FIQs
	gp2x_regs32[0x4508>>2] = ~1U;	// mask all but DISPINT
	gp2x_regs32[0x450c>>2] = 0;	// kernel does this
	gp2x_regs32[0x4500>>2] = -1;	// kernel does this
	gp2x_regs32[0x4510>>2] = -1;	// kernel does this
	// enable VSYNC IRQ in display controler
	gp2x_regs16[0x2846>>1] |= 0x20;
	// and now, enable IRQs
	enable_irqs();
	// Init datas
	ctx_reset();
	shared_reset();
	console_begin();
	console_setup();
	console_enable();
	set_speed(FULL_THROTTLE);
	run();
	console_end();
quit:;
	// TODO halt the 940
}
#else
static void alrm_handler(int dummy)
{
	(void)dummy;
	vertical_interrupt();
}

int main(void)
{
	if (-1 == perftime_begin()) return EXIT_FAILURE;
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
	sdl_screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_SWSURFACE);
	if (! sdl_screen) return EXIT_FAILURE;
	SDL_WM_SetCaption("gpu940", NULL);
	// use itimer for simulation of vertical interrupt
	if (0 != sigaction(SIGALRM, &(struct sigaction){ .sa_handler = alrm_handler }, NULL)) {
		perror("sigaction");
		return EXIT_FAILURE;
	}
#	if 1
	struct itimerval itimer = {
		.it_interval = {
			.tv_sec = 0,
			.tv_usec = 20000,	// for 50 FPS
		},
		.it_value = {
			.tv_sec = 0,
			.tv_usec = 20000,
		},
	};
	if (0 != setitimer(ITIMER_REAL, &itimer, NULL)) {
		perror("setitimer");
		return EXIT_FAILURE;
	}
#	endif
	console_begin();
	console_setup();
	run();
	console_end();
	SDL_Quit();
	perftime_end();
	return 0;
}
#endif

