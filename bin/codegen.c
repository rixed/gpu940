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
#include "gpu940i.h"

/*
 * Data Definitions
 */

#define CONSTP_DW 0x1U
#define CONSTP_DDECLIV 0x2U
#define CONSTP_Z 0x4U
#define CONSTP_DZ 0x8U
#define CONSTP_DU 0x10U
#define CONSTP_DV 0x20U
#define CONSTP_DR 0x40U
#define CONSTP_DG 0x80U
#define CONSTP_DB 0x100U
#define CONSTP_DI 0x200U
#define CONSTP_KEY 0x400U
#define CONSTP_COLOR 0x800U
#define CONSTP_OUT2ZB 0x1000U
#define CONSTP_OUT 0x2000U
#define CONSTP_TEXT 0x4000U
#define CONSTP_SHADOW 0x8000U
#define MAX_CONSTP CONSTP_SHADOW
#define MIN_CONSTP CONSTP_DW

#define VARP_W 0x10000U
#define VARP_DECLIV 0x20000U
#define VARP_Z 0x40000U
#define VARP_U 0x80000U
#define VARP_V 0x100000U
#define VARP_R 0x200000U
#define VARP_G 0x400000U
#define VARP_B 0x800000U
#define VARP_I 0x1000000U
#define VARP_CTX 0x2000000U	// not really a VAR, but we need to keep in a dedicated register in some cases
#define MAX_VARP VARP_CTX
#define MIN_VARP VARP_W

struct {
	unsigned working_set;	// how many regs are required for internal computations
	uint32_t needed_vars;	// which vars are needed
	void (*write_code)(void);	// write the code to addr, return size
} const code_bloc_defs[] = {
	{	// preload definitive colors in some rare case
#		define PRELOAD_FLAT 0
		.working_set = 0,
		.needed_vars = CONSTP_COLOR,
	}, {	// no code neede here but we need to get the address
#		define BEGIN_PIXEL_LOOP 1
		.working_set = 0,
		.needed_vars = 0,
	}, {	// zbuffer test
#		define ZBUFFER_PERSP 2
		.working_set = 1,	// to read former z
		.needed_vars = CONSTP_Z,
	}, {	// zbuffer test
#		define ZBUFFER_NOPERSP 3
		.working_set = 1,	// to read former z
		.needed_vars = VARP_Z,
	}, {	// peek flat
#		define PEEK_FLAT 4
		.working_set = 0,
		.needed_vars = CONSTP_COLOR,
	}, {	// peek text without key
#		define PEEK_TEXT 5
		.working_set = 0,
		.needed_vars = VARP_COLOR|VARP_U|VARP_V|CONSTP_DU|CONSTP_DV|CONSTP_TEXT,
	}, {	// peed text with key
#		define KEY_TEST 6
		.working_set = 0,
		.needed_vars = CONSTP_KEY,
	}, {	// peek smooth
#		define PEEK_SMOOTH 7
		.working_set = 1,	// intermediate value
		.needed_vars = VARP_COLOR|VARP_R|VARP_G|VARP_B|CONSTP_DR|CONSTP_DG|CONSTP_DB,
	}, {	// intens
#		define INTENS 8
		.working_set = 1,	// intermediate value
		.needed_vars = VARP_COLOR|VARP_I|CONSTP_DI,
	}, {	// shadow
#		define SHADOW 9
		.working_set = 2,	// former color + intermediate value
		.needed_vars = VARP_COLOR|CONSTP_SHADOW,
	}, {
#		define POKE_OUT_PERSP 10
		.working_set = 0,
		.needed_vars = VARP_W|VARP_DECLIV,
	}, {
#		define POKE_OUT_NOPERSP 11
		.working_set = 0,
		.needed_vars = VARP_W,
	}, {
#		define POKE_Z_PERSP 12
		.working_set = 0,
		.needed_vars = VARP_W|CONSTP_Z|VARP_DECLIV,
	}, {
#		define POKE_Z_NOPERSP 13
		.working_set = 0,
		.needed_vars = VARP_W|VARP_Z|VARP_DECLIV,
	}, {
#		define NEXT_PERSP 14
		.working_set = 0,
		.needed_vars = VARP_W|VARP_DECLIV|CONSTP_DW|CONSTP_DDECLIV,
	}, {
#		define NEXT_NOPERSP 15
		.working_set = 0,
		.needed_vars = VARP_W,
	}, {
#		define NEXT_Z 16
		.working_set = 0,
		.needed_vars = VARP_Z|CONSTP_DZ,
	}
};
// Il faut toujours W et, si write_color=1, des registres pour les couleurs, et si write_z=1 des registres pour les Z

static uint32_t needed_vars, needed_constp
static unsigned working_set;
static unsigned nb_vars, nb_consts, nb_used_regs, nb_pixels_per_loop;
static struct {
	uint32_t affected_vars;	// a single var or severall consts
} regs[15];
static char *gen_dst, *loop_begin;
static uint32_t regs_pushed;
static uint32_t sp_save;
static const uint32_t sp_save_addr = &sp_save;
static char *r13_save_addr;

/*
 * Private Functions
 */

static void bloc_def_func(void (*cb)(unsigned))
{	
	// ZBuffer
	if (
		ctx.poly.cmdFacet.rendering_type == rendering_flat &&
		!ctx.poly.cmdFacet.use_intens &&
		ctx.poly.cmdFacet.write_out
	) {
		cb(PRELOAD_FLAT);
	}
	cb(BEGIN_PIXEL_LOOP);
	if (ctx.rendering.z_mode != gpu_z_off) {
		if (ctx.poly.cmdFacet.perspective) cb(ZBUFFER_PERSP);
		else cb(ZBUFFER_NOPERSP);
	}
	// Peek color
	switch (ctx.poly.cmdFacet.rendering_type) {
		case rendering_flat:
			if (ctx.poly.cmdFacet.write_out && ctx.poly.cmdFacet.use_intens) cb(PEEK_FLAT);
			break;
		case rendering_shadow:
			if (! ctx.poly.cmdFacet.use_key) break;
		case rendering_text:
			if (ctx.poly.cmdFacet.write_out || ctx.poly.cmdFacet.use_key) cb(PEEK_TEXT);
			if (ctx.poly.cmdFacet.use_key) cb(KEY_TEST);
			break;
		case rendering_smooth:
			if (ctx.poly.cmdFacet.write_out) cb(PEEK_SMOOTH);
			break;
	}
	// Intens
	if (ctx.poly.cmdFacet.use_intens && ctx.poly.cmdFacet.write_out) cb(INTENS);
	// Shadow
	if (ctx.poly.cmdFacet.rendering_type == rendering_shadow && ctx.poly.cmdFacet.write_out) cb(SHADOW);
	// Poke
	if (ctx.poly.cmdFacet.write_out) {
		if (ctx.poly.cmdFacet.perspective) cb(POKE_OUT_PERSP);
		else cb(POKE_OUT_NOPERSP);
	}
	if (ctx.poly.cmdFacet.write_z) {
		if (ctx.poly.cmdFacet.perspective) cb(POKE_Z_PERSP);
		else cb(POKE_Z_NOPERSP);
	}
	// Next pixel
	if (ctx.poly.cmdFacet.perspective) {
		cb(NEXT_PERSP);
	} else {
		cb(NEXT_NOPERSP);
		if (ctx.rendering.z_mode != gpu_z_off) {
			cb(NEXT_Z);
		}
	}
}

static void look_regs(unsigned block)
{
	needed_vars |= code_bloc_defs[block].needed_vars;
	if (code_bloc_defs[block].working_set >= working_set) {
		working_set = code_bloc_defs[block].working_set;
	}
}

static void alloc_regs(void)
{
	unsigned var, r;
	for (var = MIN_VARP, r = 0; var <= MAX_VARP; var <<= 1) {
		regs[r].affected_vars = var;
	}
	nb_vars = r;
	assert(nb_vars < sizeof_array(regs) - working_set);
	bool need_ctx = false;
	for (var = MIN_CONSTP, nb_consts = 0; var <= MAX_CONSTP; var << = 1) {
		if (r > sizeof_array(regs) - working_set) {
			r = nb_vars;
			if (! need_ctx) {	// we will need to reload some constp, so we'd better keep a ctx with us.
				regs[r+1].affected_vars |= regs[r].affected_vars;
				regs[r].affected_vars = VARP_CTX;
				nb_vars++;
				r++;
				need_ctx = true;
			}
		}
		regs[r].affected_vars |= var;
		nb_consts ++;
	}
	nb_used_regs = nb_vars + nb_consts + working_set;
	nb_pixels_per_loop = 1;
	if (nb_used_regs >= sizeof_array(regs)) {
		nb_used_regs = sizeof_array(regs);
	} else {	// use remaining regs to write several pixels in the loop
		if (!ctx.poly.cmdFacet.perspective && !ctx.poly.cmdFacet.use_key && ctx.rendering.z_mode != gpu_z_off) {
			// we can read several values and poke them all at once
			while (nb_vars + ctx.poly.cmdFacet.write_out + ctx.poly.cmdFacet.write_z <= sizeof_array(regs) - nb_consts - working_set) {
				if (ctx.poly.cmdFacet.write_out) regs[r].affected_vars |= VARP_COLOR;
				if (ctx.poly.cmdFacet.write_z) regs[r].affected_vars |= VARP_Z;
				nb_pixels_per_loop ++;
			}
		}
	}
}

static void write_save(void)
{
	bool need_save = false;
	regs_pushed = 0;
	for (unsigned r=4; r<sizeof_array(regs); r++) {
		if (regs[r].affected_vars) {
			if (r == 13) continue;	// stack pointer deserve special treatment
			regs_pushed |= 1U<<r;
			need_save = true;
		}
	}
	if (! need_save) return;
	if (regs_pushed) {
		uint32_t const stm = 0xe8ad0000; // 1110 1001 0010 1101 0000 0000 0000 0000 ie "stmdb r13!,{...}
		*gen_dst++ = stm | regs_pushed;
	}
	if (regs[13].affected_vars) {
		*gen_dst++ = 0xe58fd000;	// 1110 0101 1000 1111 1101 0000 0000 0000 ie "str r13,[r15, #+0]"
		*gen_dst++ = 0xea000000;	// 1110 1010 0000 0000 0000 0000 0000 0000 ie "b here+8"
		r13_saved_addr = gen_dst;
		gen_dst++;	// space where r13 is saved
		
	}
}

static void write_restore(void)
{
	if (regs[13].affected_vars) {
		uint32_t ldr = 0xe51fd000;	// 1110 0101 0001 1111 1101 0000 0000 0000 "ldr r13,[r15, #-r13_saved_addr_offset]
		uint32_t offset = gen_dst+8 - r13_saved_addr;
		assert(offset < (1<<12));
		*gen_dst++ = ldr|offset;
	}
	if (regs_pushed) {
		uint32_t const ldm = 0xe8bd0000;	// 1110 1000 1011 1101 0000 0000 0000 0000 ie "ldmia r13!,{...}"
		if (regs_pushed & (1<<14)) {	// we pushed the back-link, restore it in pc
			regs_pushed &= ~(1<<14);
			regs_pushed |= 1<<15;
			*gen_dst++ = ldm|regs_pushed;
			return;
		}
		*gen_dst++ = ldm|regs_pushed;	// we still need to get out of here
	}
	*gen_dst++ = 0xe1a0f00e;	// 1110 0001 1010 0000 1111 0000 0000 1110 ie "mov r15, r14"
}

static void write_const_init(void)
{
	char *ctx_ptr = NULL
	// TODO: If we have a VARP_CTX to affect to a register, do it now.
	// If not, use the last afected registers (varp or constp) to store a temp ctx_ptr.
	// We always need a ctx_ptr, anyway.
	*gen_dst++ = 0xea000000;	// 1110 1010 0000 0000 0000 0000 0000 0000 ie "b here+8"
	*gen_dst++ = &ctx;
	// TODO: choose a register where to store this...
	//*gen_dst++ = "mov reg,.ctx";
}

static void write_blocks(unsigned block)
{
	code_bloc_defs[block].write_code();
}

static void write_all(void)
{
	// entry_point : save used registers
	write_save();
	write_const_init();
	write_var_init();
	bloc_def_func(write_block);
	write_restore();
}

/*
 * Public Functions
 */

int build_code(void *dst)
{
	needed_vars = needed_constp = 0;
	working_set = 0;
	nb_vars = nb_consts = nb_used_regs = 0;
	my_memset(regs, 0, sizeof(regs));
	gen_dst = dst;
	loop_begin = NULL;
	bloc_def_func(look_regs);
	alloc_regs();
	write_all();
	return gen_dst - (char *)dst;
}

