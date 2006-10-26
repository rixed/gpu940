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
#include <stddef.h>
#include <assert.h>
#include <limits.h>
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
#define VARP_OUTCOLOR 25
#define VARP_OUTCOLOR_M (1<<VARP_OUTCOLOR)
#define VARP_COUNT 26
#define VARP_COUNT_M (1<<VARP_COUNT)
#define MAX_VARP VARP_COUNT
#define MIN_VARP VARP_W

static void preload_flat(void);
static void begin_write_loop(void);
static void begin_pixel_loop(void);
static void end_write_loop(void);
static void zbuffer_persp(void);

struct {
	unsigned working_set;	// how many regs are required for internal computations
	uint32_t needed_vars;	// which vars are needed
	void (*write_code)(void);	// write the code to addr, return size
} const code_bloc_defs[] = {
	{	// preload definitive colors in some rare case
#		define PRELOAD_FLAT 0
		.working_set = 0,
		.needed_vars = CONSTP_COLOR_M,
		.write_code = preload_flat,
	}, {	// no code needed here but we need to get the address
#		define BEGIN_WRITE_LOOP 1
		.working_set = 0,
		.needed_vars = VARP_COUNT_M,
		.write_code = begin_write_loop,
	}, { // again
#		define BEGIN_PIXEL_LOOP 2
		.working_set = 0,
		.needed_vars = 0,
		.write_code = begin_pixel_loop,
	}, {
#		define END_PIXEL_LOOP 3
		.working_set = 0,
		.needed_vars = 0,
		.write_code = NULL,
	}, {
#		define END_WRITE_LOOP 4
		.working_set = 0,
		.needed_vars = VARP_COUNT_M,
		.write_code = end_write_loop,
	}, {	// zbuffer test
#		define ZBUFFER_PERSP 5
		.working_set = 1,	// to read former z
		.needed_vars = CONSTP_Z_M|VARP_W_M|VARP_DECLIV_M|CONSTP_OUT2ZB_M,	// WHISH: we will need to compute w+decliv in the work_register. find a way to keep it for POKE?
		.write_code = zbuffer_persp,
	}, {	// zbuffer test
#		define ZBUFFER_NOPERSP 6
		.working_set = 1,	// to read former z
		.needed_vars = VARP_Z_M,
		.write_code = NULL,
	}, {	// peek flat
#		define PEEK_FLAT 7
		.working_set = 0,
		.needed_vars = CONSTP_COLOR_M,
		.write_code = NULL,
	}, {	// peek text without key
#		define PEEK_TEXT 8
		.working_set = 0,
		.needed_vars = VARP_OUTCOLOR_M|VARP_U_M|VARP_V_M|CONSTP_DU_M|CONSTP_DV_M|CONSTP_TEXT_M,
		.write_code = NULL,
	}, {	// peed text with key
#		define KEY_TEST 9
		.working_set = 0,
		.needed_vars = CONSTP_KEY_M,
		.write_code = NULL,
	}, {	// peek smooth
#		define PEEK_SMOOTH 10
		.working_set = 1,	// intermediate value
		.needed_vars = VARP_OUTCOLOR_M|VARP_R_M|VARP_G_M|VARP_B_M|CONSTP_DR_M|CONSTP_DG_M|CONSTP_DB_M,
		.write_code = NULL,
	}, {	// intens
#		define INTENS 11
		.working_set = 1,	// intermediate value
		.needed_vars = VARP_OUTCOLOR_M|VARP_I_M|CONSTP_DI_M,
		.write_code = NULL,
	}, {	// shadow
#		define SHADOW 12
		.working_set = 2,	// former color + intermediate value
		.needed_vars = VARP_OUTCOLOR_M|CONSTP_SHADOW_M,
		.write_code = NULL,
	}, {
#		define POKE_OUT_PERSP 13
		.working_set = 0,
		.needed_vars = VARP_W_M|VARP_DECLIV_M,
		.write_code = NULL,
	}, {
#		define POKE_OUT_NOPERSP 14
		.working_set = 0,
		.needed_vars = VARP_W_M,
		.write_code = NULL,
	}, {
#		define POKE_Z_PERSP 15
		.working_set = 0,
		.needed_vars = VARP_W_M|CONSTP_Z_M|VARP_DECLIV_M,
		.write_code = NULL,
	}, {
#		define POKE_Z_NOPERSP 16
		.working_set = 0,
		.needed_vars = VARP_W_M|VARP_Z_M|VARP_DECLIV_M,
		.write_code = NULL,
	}, {
#		define NEXT_PERSP 17
		.working_set = 0,
		.needed_vars = VARP_W_M|VARP_DECLIV_M|CONSTP_DW_M|CONSTP_DDECLIV_M,
		.write_code = NULL,
	}, {
#		define NEXT_NOPERSP 18
		.working_set = 0,
		.needed_vars = VARP_W_M,
		.write_code = NULL,
	}, {
#		define NEXT_Z 19
		.working_set = 0,
		.needed_vars = VARP_Z_M|CONSTP_DZ_M,
		.write_code = NULL,
	}
};

static uint32_t needed_vars, needed_constp, nb_needed_regs;
static unsigned working_set;
static unsigned nb_pixels_per_loop;
static struct {
	uint32_t affected_vars;	// a single var or severall consts
} regs[15];
static struct {
	int rnum;	// number of register affected to this variable (-1 if none).
	uint32_t offset;	// offset to reach this var from ctx
	// several registers can be affected to the same var ; we only need to know one.
} vars[MAX_VARP+1] = {
	{ .offset = offsetof(struct ctx, line.dw), },
	{ .offset = offsetof(struct ctx, line.ddecliv), },
	{ .offset = 0 },
	{ .offset = 0 },
	{ .offset = offsetof(struct ctx, line.dparam[0]), },
	{ .offset = offsetof(struct ctx, line.dparam[1]), },
	{ .offset = offsetof(struct ctx, line.dparam[0]), },
	{ .offset = offsetof(struct ctx, line.dparam[1]), },
	{ .offset = offsetof(struct ctx, line.dparam[2]), },
	{ .offset = 0 },
	{ .offset = offsetof(struct ctx, poly.cmdFacet.color), },
	{ .offset = offsetof(struct ctx, poly.cmdFacet.color), },
	{ .offset = offsetof(struct ctx, code.out2zb), },
	{ .offset = offsetof(struct ctx, code.buff_addr[gpuOutBuffer]), },
	{ .offset = offsetof(struct ctx, code.buff_addr[gpuTxtBuffer]), },
	{ .offset = offsetof(struct ctx, poly.cmdFacet.shadow), },
	{ .offset = offsetof(struct ctx, line.w), },
	{ .offset = offsetof(struct ctx, line.decliv), },
	{ .offset = 0 },
	{ .offset = offsetof(struct ctx, line.param[0]), },
	{ .offset = offsetof(struct ctx, line.param[1]), },
	{ .offset = offsetof(struct ctx, line.param[0]), },
	{ .offset = offsetof(struct ctx, line.param[1]), },
	{ .offset = offsetof(struct ctx, line.param[2]), },
	{ .offset = 0 },
	{ .offset = offsetof(struct ctx, line.count) }
};
static uint32_t *gen_dst, *write_loop_begin, *pixel_loop_begin;
static uint32_t *patch_bh;
static uint32_t regs_pushed;

/*
 * Private Functions
 */

static void preload_flat(void)
{
	// We already know VARP_OUTCOLOR: it's CONSP_COLOR. Preload all.
	assert(vars[CONSTP_COLOR].rnum != -1);
	for (unsigned r=0; r<sizeof_array(regs); r++) {
		if (regs[r].affected_vars == 1<<VARP_OUTCOLOR) {
			*gen_dst++ = 0xe1a00000 | (r<<12) | vars[CONSTP_COLOR].rnum;	// 1110 0001 1010 0000 Rvar 0000 0000 Rcst ie "mov Rvar, Rconst"
		}
	}
}

static void begin_write_loop(void)
{
	if (nb_pixels_per_loop > 1) {	// test that varp_count >= nb_pixels_per_loop, or jump straight to the "bottom half"
		assert(nb_pixels_per_loop < (1<<8));
		assert(vars[VARP_COUNT].rnum != -1);
		*gen_dst++ = 0xe3500000 | (vars[VARP_COUNT].rnum<<16) | nb_pixels_per_loop;	// 1110 0011 0101 count 0000 0000 0000 0000 ie "cmp Rcount, nb_pixels_per_loop"
		//pacth_bh = gen_dst;
		*gen_dst++ = 0x3a000000;	// 0011 1010 0000 0000 0000 0000 0000 0000 ie "blo XXXX"
	}
	write_loop_begin = gen_dst;
}

static void begin_pixel_loop(void)
{
	pixel_loop_begin = gen_dst;
}

static void end_write_loop(void)
{
	if (nb_pixels_per_loop > 1) {
		assert(nb_pixels_per_loop < (1<<8));
		*gen_dst++ = 0xe2400000 | (vars[VARP_COUNT].rnum<<16) | (vars[VARP_COUNT].rnum<<12) | nb_pixels_per_loop;	// 1110 0010 0100 _Rn_ _Rd_ 0000 nbpix loop ie "sub rcount, rcount, #nb_pixels_per_loop"
		*gen_dst++ = 0xe3500000 | (vars[VARP_COUNT].rnum<<16) | nb_pixels_per_loop;	// 1110 0011 0101 _Rn_ 0000 0000 nbpix loop ie "cmp rcount, #nb_pixels_per_loop"
		int32_t begin_offset =  (write_loop_begin - (gen_dst+2)) & 0xffffff;
		*gen_dst++ = 0x2a000000 | (begin_offset);	// 0010 1010 0000 0000 0000 0000 0000 0000 ie "bhs write_loop_begin"
	} else {
		*gen_dst++ = 0xe2500001 | (vars[VARP_COUNT].rnum<<16) | (vars[VARP_COUNT].rnum<<12);	// 1110 0010 0101 _Rn_ _Rd_ 0000 0000 0001 ie "subs rcount, rcount, #1"
		int32_t begin_offset =  (write_loop_begin - (gen_dst+2)) & 0xffffff;
		*gen_dst++ = 0x2a000000 | (begin_offset);	// ie "bhs wrote_loop_begin"
	}
}

static uint32_t offset12(unsigned v)
{
	uint32_t offset = (uint8_t *)(gen_dst+2) - (uint8_t *)((char *)&ctx + vars[v].offset);
	assert(offset < (1<<12));
	return offset;
}

static unsigned load_constp(unsigned v, unsigned rtmp)
{
	if (regs[ vars[v].rnum ].affected_vars == 1<<v) {
		// we have a dedicated reg
		return vars[v].rnum;
	} else {
		// we need to load into register rtmp
		uint32_t offset = offset12(v);
		*gen_dst++ = 0xe51f0000 | (rtmp<<12) | offset;	// 1110 0101 0001 1111 Rtmp offt var_ v__ ie "ldr Rtmp, [r15, #-offset_v]"
		return rtmp;
	}
}

static void zbuffer_persp(void)
{
	// compute W+DECLIV
	unsigned rtmp1 = get_tmp_reg();
	*gen_dst++ = 0xe1a00840 | (rtmp1<<12) | vars[VARP_DECLIV].rnum;	// 1110 0001 1010 0000 Tmp1 1000 0100 Decliv  ie "mov Rtmp1, Rdecliv, asr #16"
	// ie "add Rtmp2, Rout2zb, Rw" oubien "ldr Rtmp2, [r15, #-offset_out2zb]" d'abord et Rtmp2 a la place de Rout2zb
	// read Z
	// ie "ldr Rtmp1, [Rtmp2, Rtmp1, asl #nc_log]"
	// compare
	// ie "cmp Rz, Rtmp1" oubien "ldr Rtmp2, [r15, #-offset_constp_z]" d'abord et Rtmp2 a la place de Rz
	// ie "bZOP XXXXX" branch to next_pixel
}

static void bloc_def_func(void (*cb)(unsigned))
{	
	if (
		ctx.poly.cmdFacet.rendering_type == rendering_flat &&
		!ctx.poly.cmdFacet.use_intens &&
		ctx.poly.cmdFacet.write_out
	) {
		cb(PRELOAD_FLAT);
	}
	cb(BEGIN_WRITE_LOOP);
	cb(BEGIN_PIXEL_LOOP);
	// ZBuffer
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
	cb(END_PIXEL_LOOP);
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
	cb(END_WRITE_LOOP);
}

static void look_regs(unsigned block)
{
	needed_vars |= code_bloc_defs[block].needed_vars;
	if (code_bloc_defs[block].working_set >= working_set) {
		working_set = code_bloc_defs[block].working_set;
	}
}

static int nbbit(uint32_t v)
{
	unsigned r = 0;
	uint32_t m = 1;
	do {
		if (v & m) r++;
		m <<= 1;
	} while (m);
	return r;
}

// FIXME: aucun interet d'avoir plusieurs vars afectees au meme registre.
// Il faudrait plutot affecter une seule var a tous les registres sauf un, et en garder
// un pour charger les constp que l'on n'a pas pu affecter. c'est plus simple (tous les
// registres sauf ceux du working set et éventuelement un de plus sont dédiés) et plus
// rapide (moins de reload de constp).
// -> simplifier les structures : vars[r].rnum contient le numero de la variable affecté
// (dans le cas ou plusieurs reg recoivent la variables (cas de CONSTP_COLOR dans certains
// cas), seulement une d'entre elles), et regs[r].var contient le numero de la variable
// actuelement chargée dans le registre, et non plus un masque. Un numéro spécial (~0 par
// exemple) signifie 'garbage'.
// load_consp() peut ainsi utiliser ce membre (regs[r].var) pour savoir si une variable
// contient déjà ce qu'on veut. Sinon, il utilise le registre dédié au chargement des constp
// (le registre no r14).
// les registres sont alloués comme suis :
// - d'abord le working set
// - ensuite les varp
// - ensuite des constp (dans la suite, choisir les constp les plus utilisés)
// - ensuite,
//   - si r0 à r13 sont remplis et qu'il reste plus d'une constp, r14 sert de consp
//     rechargeable pour les constp restantes.
//   - si r0 à r13 sont remplis et qu'il reste une seule constp, r14 est dédié à cette constp.
//   - s'il n'y a plus de const avant que r13 ne soit rempli, affecter les autres registres
//     au VARP_OUTCOLOR et VARP_Z nécéssaire pour écrire plusieurs pixels d'un coup.
static void alloc_regs(void)
{
	nb_needed_regs = nbbit(needed_vars) + working_set;
	unsigned r = working_set;	// from 0 to working_set are tmp regs
	unsigned var;
	for (var = MIN_VARP; var <= MAX_VARP; var ++) {
		if (! (needed_vars & (1<<var))) continue;
		regs[r].affected_vars = 1<<var;
		vars[var].rnum = r;
		r ++;
	}
	unsigned first_constp = r;
	assert(first_constp < sizeof_array(regs));
	for (var = MIN_CONSTP; var <= MAX_CONSTP; var ++) {
		if (! (needed_vars & (1<<var))) continue;
		if (r >= sizeof_array(regs)) {
			r = nb_vars;
		}
		regs[r].affected_vars |= 1<<var;
		vars[var].rnum = r;
		r ++;
	}
	nb_pixels_per_loop = 1;
	if (nb_needed_regs < sizeof_array(regs)) {
		// use remaining regs to write several pixels in the loop
		if (!ctx.poly.cmdFacet.perspective && !ctx.poly.cmdFacet.use_key && ctx.rendering.z_mode == gpu_z_off) {
			// we can read several values and poke them all at once
			// notice : that z_mode is off does not mean that we do not want to write Z !
			while (nb_needed_regs + ctx.poly.cmdFacet.write_out + ctx.poly.cmdFacet.write_z <= sizeof_array(regs)) {
				if (ctx.poly.cmdFacet.write_out) {
					regs[r].affected_vars |= VARP_OUTCOLOR_M;
					nb_needed_regs ++;
				}
				if (ctx.poly.cmdFacet.write_z) {
					regs[r].affected_vars |= VARP_Z_M;
					nb_needed_regs ++;
				}
				nb_pixels_per_loop ++;
			}
		}
	} else {
		nb_needed_regs = sizeof_array(regs);
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
		uint32_t sp_save_offset = (uint8_t *)(gen_dst+2) - (uint8_t *)&ctx.code.sp_save;
		assert(sp_save_offset < 1<<12);
		*gen_dst++ = 0xe50fd000 | sp_save_offset;	// 1110 0101 0000 1111 1101 0000 0000 0000 ie "str r13,[r15, #-sp_save_offset]"
		
	}
}

static void write_restore(void)
{
	if (regs[13].affected_vars) {
		uint32_t sp_save_offset = (uint8_t *)(gen_dst+2) - (uint8_t *)&ctx.code.sp_save;
		assert(sp_save_offset < 1<<12);
		*gen_dst++ = 0xe51fd000 | sp_save_offset;	// 1110 0101 0001 1111 1101 0000 0000 0000 "ldr r13,[r15, #-sp_save_offset]
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
	// Now load all required constp into their affected regs
	for (unsigned v = MIN_CONSTP; v <= MAX_CONSTP; v++) {
		if (vars[v].rnum != -1 && regs[vars[v].rnum].affected_vars == 1U<<v) {
			// the reg is only for this constp, preload it !
			uint32_t offset = offset12(v);
			*gen_dst++ = 0xe51f0000 | (vars[v].rnum<<12) | offset;	// 1110 0101 0001 1111 _Rd_ 0000 0000 0000 ie "ldr Rd,[r15, #-offset]"
		}
	}
}

static void write_var_init(void)
{
	// All vars have a unique dedicated reg, except for varp_outcolor which is special (computed during loop).
	for (unsigned v = MIN_VARP; v <= MAX_VARP; v++) {
		if (v == VARP_OUTCOLOR) continue;
		if (vars[v].rnum == -1) continue;
		uint32_t offset = offset12(v);
		*gen_dst++ = 0xe51f0000 | (vars[v].rnum<<12) | offset;	// 1110 0101 0001 1111 _Rd_ 0000 0000 0000 ie "ldr Rd,[r15, #-offset]"
	}
}

static void write_block(unsigned block)
{
	if (NULL != code_bloc_defs[block].write_code) {
		code_bloc_defs[block].write_code();
	}
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

static uint32_t get_rendering_key(void)
{
	uint32_t key = ctx.poly.cmdFacet.rendering_type;
	key |= ctx.poly.cmdFacet.use_key<<2;
	key |= ctx.poly.cmdFacet.use_intens<<3;
	key |= ctx.poly.cmdFacet.perspective<<4;
	key |= ctx.poly.cmdFacet.write_out<<5;
	key |= ctx.poly.cmdFacet.write_z<<6;
	key |= 1<<7;	// meaning : key is set
	key |= ctx.rendering.z_mode<<8;
	return key;
}

#if TEST_RASTERIZER
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <fcntl.h>
#	include <unistd.h>
#	include <stdio.h>
#	include <string.h>
#	include <errno.h>
#	include <inttypes.h>
#endif

void build_code(unsigned cache)
{
	needed_vars = needed_constp = 0;
	working_set = 0;
	my_memset(regs, 0, sizeof(regs));
	for (unsigned v=0; v<sizeof_array(vars); v++) {
		vars[v].rnum = -1;
	}
	// adjust offsets
	unsigned i_idx = ctx.poly.nb_params;
	if (ctx.poly.cmdFacet.use_intens) i_idx --;
	unsigned z_idx = i_idx;
	if (ctx.rendering.z_mode != gpu_z_off) i_idx --;
	vars[CONSTP_Z].offset = offsetof(struct ctx, line.param[z_idx]);
	vars[CONSTP_DZ].offset = offsetof(struct ctx, line.dparam[z_idx]);
	vars[CONSTP_DI].offset = offsetof(struct ctx, line.dparam[i_idx]);
	vars[VARP_Z].offset = offsetof(struct ctx, line.param[z_idx]);
	vars[VARP_I].offset = offsetof(struct ctx, line.param[i_idx]);
	// init other global vars
	gen_dst = ctx.code.caches[cache].buf;
	write_loop_begin = pixel_loop_begin = NULL;
	patch_bh = NULL;
	bloc_def_func(look_regs);
	alloc_regs();
	write_all();
	unsigned nb_words = gen_dst - ctx.code.caches[cache].buf;
	assert(nb_words < sizeof_array(ctx.code.caches[cache].buf));
#	ifdef TEST_RASTERIZER
	static char fname[PATH_MAX];
	snprintf(fname, sizeof(fname), "/tmp/codegen_%"PRIu32, ctx.code.caches[cache].rendering_key);
	int fd = open(fname, O_WRONLY|O_CREAT);
	if (fd == -1) {
		fprintf(stderr, "Cannot open %s : %s\n", fname, strerror(errno));
	}
	assert(fd != -1);
	write(fd, ctx.code.caches[cache].buf, nb_words * sizeof(uint32_t));
	close(fd);
#	endif
}

/*
 * Public Functions
 */

unsigned get_rasterizer(void)
{
	uint32_t key = get_rendering_key();
	int r_dest = -1;
	for (unsigned r=0; r<sizeof_array(ctx.code.caches); r++) {
		if (ctx.code.caches[r].rendering_key == key) {
			ctx.code.caches[r].use_count ++;
			return r;
		}
		if (ctx.code.caches[r].rendering_key == 0) {
			r_dest = r;
		}
	}
	// not found
	if (r_dest == -1) {
		// free the least used
		uint32_t min_use_count = UINT32_MAX;
		for (unsigned r = 0; r<sizeof_array(ctx.code.caches); r++) {
			if (ctx.code.caches[r].use_count < min_use_count) {
				min_use_count = ctx.code.caches[r].use_count;
				r_dest = r;
			}
			ctx.code.caches[r].use_count = 0;
		}
	}
	ctx.code.caches[r_dest].use_count = 1;
	ctx.code.caches[r_dest].rendering_key = key;
	build_code(r_dest);
	return r_dest;
}

