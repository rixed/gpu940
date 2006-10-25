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

#define CONSTP_DW 0
#define CONSTP_DW_M (1<<CONSTP_DW)
#define CONSTP_DDECLIV 1
#define CONSTP_DDECLIV_M (1<<CONSTP_DDECLIV)
#define CONSTP_Z 2
#define CONSTP_Z_M (1<<CONSTP_Z)
#define CONSTP_DZ 3
#define CONSTP_DZ_M (1<<CONSTP_DZ)
#define CONSTP_DU 4
#define CONSTP_DU_M (1<<CONSTP_DU)
#define CONSTP_DV 5
#define CONSTP_DV_M (1<<CONSTP_DV)
#define CONSTP_DR 6
#define CONSTP_DR_M (1<<CONSTP_DR)
#define CONSTP_DG 7
#define CONSTP_DG_M (1<<CONSTP_DG)
#define CONSTP_DB 8
#define CONSTP_DB_M (1<<CONSTP_DB)
#define CONSTP_DI 9
#define CONSTP_DI_M (1<<CONSTP_DI)
#define CONSTP_KEY 10
#define CONSTP_KEY_M (1<<CONSTP_KEY)
#define CONSTP_COLOR 11
#define CONSTP_COLOR_M (1<<CONSTP_COLOR)
#define CONSTP_OUT2ZB 12
#define CONSTP_OUT2ZB_M (1<<CONSTP_OUT2ZB)
#define CONSTP_OUT 13
#define CONSTP_OUT_M (1<<CONSTP_OUT)
#define CONSTP_TEXT 14
#define CONSTP_TEXT_M (1<<CONSTP_TEXT)
#define CONSTP_SHADOW 15
#define CONSTP_SHADOW_M (1<<CONSTP_SHADOW)
#define MAX_CONSTP CONSTP_SHADOW
#define MIN_CONSTP CONSTP_DW

#define VARP_W 16
#define VARP_W_M (1<<VARP_W)
#define VARP_DECLIV 17
#define VARP_DECLIV_M (1<<VARP_DECLIV)
#define VARP_Z 18
#define VARP_Z_M (1<<VARP_Z)
#define VARP_U 19
#define VARP_U_M (1<<VARP_U)
#define VARP_V 20
#define VARP_V_M (1<<VARP_V)
#define VARP_R 21
#define VARP_R_M (1<<VARP_R)
#define VARP_G 22
#define VARP_G_M (1<<VARP_G)
#define VARP_B 23
#define VARP_B_M (1<<VARP_B)
#define VARP_I 24
#define VARP_I_M (1<<VARP_I)
#define VARP_CTX 25	// not really a VAR, but we need to keep in a dedicated register in some cases
#define VARP_CTX_M (1<<VARP_CTX)
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
		.needed_vars = CONSTP_COLOR_M,
	}, {	// no code neede here but we need to get the address
#		define BEGIN_PIXEL_LOOP 1
		.working_set = 0,
		.needed_vars = 0,
	}, {	// zbuffer test
#		define ZBUFFER_PERSP 2
		.working_set = 1,	// to read former z
		.needed_vars = CONSTP_Z_M,
	}, {	// zbuffer test
#		define ZBUFFER_NOPERSP 3
		.working_set = 1,	// to read former z
		.needed_vars = VARP_Z_M,
	}, {	// peek flat
#		define PEEK_FLAT 4
		.working_set = 0,
		.needed_vars = CONSTP_COLOR_M,
	}, {	// peek text without key
#		define PEEK_TEXT 5
		.working_set = 0,
		.needed_vars = VARP_COLOR_M|VARP_U_M|VARP_V_M|CONSTP_DU_M|CONSTP_DV_M|CONSTP_TEXT_M,
	}, {	// peed text with key
#		define KEY_TEST 6
		.working_set = 0,
		.needed_vars = CONSTP_KEY_M,
	}, {	// peek smooth
#		define PEEK_SMOOTH 7
		.working_set = 1,	// intermediate value
		.needed_vars = VARP_COLOR_M|VARP_R_M|VARP_G_M|VARP_B_M|CONSTP_DR_M|CONSTP_DG_M|CONSTP_DB_M,
	}, {	// intens
#		define INTENS 8
		.working_set = 1,	// intermediate value
		.needed_vars = VARP_COLOR_M|VARP_I_M|CONSTP_DI_M,
	}, {	// shadow
#		define SHADOW 9
		.working_set = 2,	// former color + intermediate value
		.needed_vars = VARP_COLOR_M|CONSTP_SHADOW_M,
	}, {
#		define POKE_OUT_PERSP 10
		.working_set = 0,
		.needed_vars = VARP_W_M|VARP_DECLIV_M,
	}, {
#		define POKE_OUT_NOPERSP 11
		.working_set = 0,
		.needed_vars = VARP_W_M,
	}, {
#		define POKE_Z_PERSP 12
		.working_set = 0,
		.needed_vars = VARP_W_M|CONSTP_Z_M|VARP_DECLIV_M,
	}, {
#		define POKE_Z_NOPERSP 13
		.working_set = 0,
		.needed_vars = VARP_W_M|VARP_Z_M|VARP_DECLIV_M,
	}, {
#		define NEXT_PERSP 14
		.working_set = 0,
		.needed_vars = VARP_W_M|VARP_DECLIV_M|CONSTP_DW_M|CONSTP_DDECLIV_M,
	}, {
#		define NEXT_NOPERSP 15
		.working_set = 0,
		.needed_vars = VARP_W_M,
	}, {
#		define NEXT_Z 16
		.working_set = 0,
		.needed_vars = VARP_Z_M|CONSTP_DZ_M,
	}
};
// Il faut toujours W et, si write_color=1, des registres pour les couleurs, et si write_z=1 des registres pour les Z

static uint32_t needed_vars, needed_constp;
static unsigned working_set;
static unsigned nb_pixels_per_loop;
static struct {
	uint32_t affected_vars;	// a single var or severall consts
} regs[15];
static struct {
	int rnum;	// number of register affected to this variable (-1 if none).
	// several registers can be affected to the same var ; we only need to know one.
} vars[MAX_VARP+1];
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
	// If we will need to reload some constp, better keep a ctx with us
	unsigned nb_needed_regs = nbbit(needed_vars) + working_set;
	if (nb_needed_regs > sizeof_array(regs)) {
		if (! (needed_vars & VARP_CTX_M) ) {
			needed_vars |= VARP_CTX_M;
			nb_needed_regs ++;
		}
	}
	for (var = MIN_VARP, r = 0; var <= MAX_VARP; var ++) {
		if (! (needed_vars & (1<<var))) continue;
		regs[r].affected_vars = 1<<var;
		vars[var].rnum = r;
		r ++;
	}
	unsigned nb_vars = r;
	assert(nb_vars < sizeof_array(regs) - working_set);
	for (var = MIN_CONSTP; var <= MAX_CONSTP; var ++) {
		if (! (needed_vars & (1<<var))) continue;
		if (r > sizeof_array(regs) - working_set) {
			r = nb_vars;
		}
		regs[r].affected_vars |= 1<<var;
		vars[var].rnum = r;
		r ++;
	}
	nb_pixels_per_loop = 1;
	if (nb_needed_regs < sizeof_array(regs)) {
		// use remaining regs to write several pixels in the loop
		if (!ctx.poly.cmdFacet.perspective && !ctx.poly.cmdFacet.use_key && ctx.rendering.z_mode != gpu_z_off) {
			// we can read several values and poke them all at once
			while (nb_needed_regs + ctx.poly.cmdFacet.write_out + ctx.poly.cmdFacet.write_z <= sizeof_array(regs)) {
				if (ctx.poly.cmdFacet.write_out) {
					regs[r].affected_vars |= VARP_COLOR_M;
					nb_needed_regs ++;
				}
				if (ctx.poly.cmdFacet.write_z) {
					regs[r].affected_vars |= VARP_Z_M;
					nb_needed_regs ++;
				}
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
	// If we have a VARP_CTX to affect to a register, do it now.
	if (vars[VARP_CTX].rnum != -1) {
		// TODO poke "ldr this_reg,[ctx_addr_addr]" (provided by caller)
	} else {
		// If not, use the last affected registers (varp or constp) to store a temp ctx_ptr.
	}
	// We always need a ctx_ptr, anyway.
	*gen_dst++ = 0xea000000;	// 1110 1010 0000 0000 0000 0000 0000 0000 ie "b here+8"
	*gen_dst++ = &ctx;
	// TODO: choose a register where to store this...
	//*gen_dst++ = "mov reg,.ctx";
}

static void write_var_init(void)
{
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
	my_memset(regs, 0, sizeof(regs));
	my_memset(vars, -1, sizeof(vars));
	gen_dst = dst;
	loop_begin = NULL;
	bloc_def_func(look_regs);
	alloc_regs();
	write_all();
	return gen_dst - (char *)dst;
}

