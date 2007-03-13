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
#define MAX_CONSTP CONSTP_TEXT
#define MIN_CONSTP CONSTP_DW

#define VARP_W 15
#define VARP_W_M (1<<VARP_W)
#define VARP_DECLIV 16
#define VARP_DECLIV_M (1<<VARP_DECLIV)
#define VARP_Z 17
#define VARP_Z_M (1<<VARP_Z)
#define VARP_U 18
#define VARP_U_M (1<<VARP_U)
#define VARP_V 19
#define VARP_V_M (1<<VARP_V)
#define VARP_R 20
#define VARP_R_M (1<<VARP_R)
#define VARP_G 21
#define VARP_G_M (1<<VARP_G)
#define VARP_B 22
#define VARP_B_M (1<<VARP_B)
#define VARP_I 23
#define VARP_I_M (1<<VARP_I)
#define VARP_COUNT 24
#define VARP_COUNT_M (1<<VARP_COUNT)
#define VARP_OUTCOLOR 25
#define VARP_OUTCOLOR_M (1<<VARP_OUTCOLOR)
#define MAX_VARP VARP_OUTCOLOR
#define MIN_VARP VARP_W

static void preload_flat(void);
static void begin_write_loop(void);
static void begin_pixel_loop(void);
static void end_write_loop(void);
static void ztest_persp(void);
static void ztest_nopersp(void);
static void peek_flat(void);
static void peek_text(void);
static void peek_text_cond(void);
static void peek_smooth(void);
static void peek_smooth_cond(void);
static void key_test(void);
static void intens(void);
static void poke_persp(void);
static void poke_nopersp(void);
static void poke_z_persp(void);
static void poke_z_nopersp(void);
static void combine_persp(void);
static void combine_nopersp(void);
static void next_persp(void);
static void next_nopersp(void);
static void next_z(void);
static void next_text(void);
static void next_smooth(void);

struct {
	unsigned working_set;	// how many regs are required for internal computations
	uint32_t needed_vars;	// which vars are needed
	void (*write_code)(void);	// write the code
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
		.working_set = 2,
		.needed_vars = CONSTP_Z_M|VARP_W_M|VARP_DECLIV_M|CONSTP_OUT2ZB_M,	// WHISH: we will need to compute w+decliv in the work_register. find a way to keep it for POKE?
		.write_code = ztest_persp,
	}, {	// zbuffer test
#		define ZBUFFER_NOPERSP 6
		.working_set = 1,
		.needed_vars = VARP_Z_M|CONSTP_OUT2ZB_M,
		.write_code = ztest_nopersp,
	}, {	// peek flat
#		define PEEK_FLAT 7
		.working_set = 0,
		.needed_vars = CONSTP_COLOR_M,
		.write_code = peek_flat,
	}, {	// peek texel
#		define PEEK_TEXT 8
		.working_set = 3,
		.needed_vars = VARP_OUTCOLOR_M|VARP_U_M|VARP_V_M|CONSTP_DU_M|CONSTP_DV_M|CONSTP_TEXT_M,
		.write_code = peek_text,
	}, {	// peek texel when peek is conditional
#		define PEEK_TEXT_COND 9
		.working_set = 3,
		.needed_vars = VARP_OUTCOLOR_M|VARP_U_M|VARP_V_M|CONSTP_TEXT_M,
		.write_code = peek_text_cond,
	}, {	// test against key color
#		define KEY_TEST 10
		.working_set = 0,
		.needed_vars = VARP_OUTCOLOR_M|CONSTP_KEY_M,
		.write_code = key_test,
	}, {	// peek smooth
#		define PEEK_SMOOTH 11
		.working_set = 1,	// intermediate value
		.needed_vars = VARP_OUTCOLOR_M|VARP_R_M|VARP_G_M|VARP_B_M|CONSTP_DR_M|CONSTP_DG_M|CONSTP_DB_M,
		.write_code = peek_smooth,
	}, {	// peek smooth when peek is conditional
#		define PEEK_SMOOTH_COND 12
		.working_set = 1,
		.needed_vars = VARP_OUTCOLOR_M|VARP_R_M|VARP_G_M|VARP_B_M,
		.write_code = peek_smooth_cond,
	}, {	// intens
#		define INTENS 13
		.working_set = 1,	// intermediate value
		.needed_vars = VARP_OUTCOLOR_M|VARP_I_M|CONSTP_DI_M,
		.write_code = intens,
	}, {	// combine
#		define COMBINE_PERSP 14
		.working_set = 5,
		.needed_vars = VARP_OUTCOLOR_M|VARP_W_M|VARP_DECLIV_M,
		.write_code = combine_persp,
	}, {
#		define COMBINE_NOPERSP 15
		.working_set = 4,
		.needed_vars = VARP_OUTCOLOR_M|VARP_W_M,
		.write_code = combine_nopersp,
	}, {
#		define POKE_OUT_PERSP 16
		.working_set = 1,
		.needed_vars = VARP_OUTCOLOR_M|VARP_W_M|VARP_DECLIV_M,
		.write_code = poke_persp,
	}, {
#		define POKE_OUT_NOPERSP 17
		.working_set = 0,
		.needed_vars = VARP_OUTCOLOR_M|VARP_W_M,
		.write_code = poke_nopersp,
	}, {
#		define POKE_Z_PERSP 18
		.working_set = 2,
		.needed_vars = VARP_W_M|CONSTP_Z_M|VARP_DECLIV_M,
		.write_code = poke_z_persp,
	}, {
#		define POKE_Z_NOPERSP 19
		.working_set = 1,
		.needed_vars = VARP_W_M|VARP_Z_M|VARP_DECLIV_M,
		.write_code = poke_z_nopersp,
	}, {
#		define NEXT_PERSP 20
		.working_set = 0,
		.needed_vars = VARP_W_M|VARP_DECLIV_M|CONSTP_DW_M|CONSTP_DDECLIV_M,
		.write_code = next_persp,
	}, {
#		define NEXT_NOPERSP 21
		.working_set = 0,
		.needed_vars = VARP_W_M,
		.write_code = next_nopersp,
	}, {
#		define NEXT_Z 22
		.working_set = 0,
		.needed_vars = VARP_Z_M|CONSTP_DZ_M,
		.write_code = next_z,
	}, {	// next u,v
#		define NEXT_TEXT 23
		.working_set = 1,
		.needed_vars = VARP_U_M|VARP_V_M|CONSTP_DU_M|CONSTP_DV_M,
		.write_code = next_text,
	}, {	// next r,g,b (actually, y,u,v)
#		define NEXT_SMOOTH 24
		.working_set = 1,
		.needed_vars = VARP_R_M|VARP_G_M|VARP_B_M|CONSTP_DR_M|CONSTP_DG_M|CONSTP_DB_M,
		.write_code = next_smooth,
	}
};

static uint32_t needed_vars, needed_constp;
static unsigned working_set, used_set;
static unsigned nb_pixels_per_loop;
static uint32_t outcolors_mask, outz_mask;
static bool in_bh;
static struct {
	int var;
} regs[15];
static struct {
	int rnum;	// number of register affected to this variable (-1 if none).
	uint32_t offset;	// offset to reach this var from ctx
	uint32_t offset2;	// if previous offset leads only a pointer to the var, nb_words from this pointer to the var
	// several registers can be affected to the same var ; we only need to know one.
} vars[MAX_VARP+1] = {
	{ .offset = offsetof(struct ctx, line.dw), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, poly.decliveness), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, line.param), .offset2 = 3, },	// CONSTP_Z
	{ .offset = offsetof(struct ctx, line.dparam[3]), .offset2 = ~0U, },	// CONSTP_DZ
	{ .offset = offsetof(struct ctx, line.dparam[0]), .offset2 = ~0U, },	// CONSTP_DU
	{ .offset = offsetof(struct ctx, line.dparam[1]), .offset2 = ~0U, },	// CONSTP_DV
	{ .offset = offsetof(struct ctx, line.dparam[0]), .offset2 = ~0U, },	// CONSTP_DR
	{ .offset = offsetof(struct ctx, line.dparam[1]), .offset2 = ~0U, },	// CONSTP_DG
	{ .offset = offsetof(struct ctx, line.dparam[2]), .offset2 = ~0U, },	// CONSTP_DB
	{ .offset = offsetof(struct ctx, line.dparam[2]), .offset2 = ~0U, },	// CONST_DI = 9
	{ .offset = offsetof(struct ctx, code.color), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, code.color), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, code.out2zb), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, code.buff_addr[gpuOutBuffer]), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, code.buff_addr[gpuTxtBuffer]), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, line.w), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, line.decliv), .offset2 = ~0U, },
	{ .offset = offsetof(struct ctx, line.param), .offset2 = 3, },	// VARP_Z = 17
	{ .offset = offsetof(struct ctx, line.param), .offset2 = 0, },	// VARP_U
	{ .offset = offsetof(struct ctx, line.param), .offset2 = 1, },	// VARP_V
	{ .offset = offsetof(struct ctx, line.param), .offset2 = 0, },	// VARP_R
	{ .offset = offsetof(struct ctx, line.param), .offset2 = 1, },	// VARP_G
	{ .offset = offsetof(struct ctx, line.param), .offset2 = 2, },	// VARP_B
	{ .offset = offsetof(struct ctx, line.param), .offset2 = 2, },	// VARP_I = 23
	{ .offset = offsetof(struct ctx, line.count), .offset2 = ~0U,},
	{ .offset = 0, .offset2 = ~0U, },	// VARP_OUTCOLOR, no offset
};
static uint32_t *gen_dst, *write_loop_begin, *pixel_loop_begin;
static unsigned nb_patches;
struct patches {
	uint32_t *addr;
	enum patch_type { offset_24 } type;
	enum patch_target { next_pixel, bottom_half, restore_quit } target;
} patches[5];

/*
 * Private Functions
 */

static bool may_skip_peek(void)
{
	return ctx.rendering.mode.named.z_mode != gpu_z_off;
}

static bool may_skip_poke(void)
{
	return may_skip_peek() || ctx.rendering.mode.named.use_key;
}

static bool may_have_multi_pixels_per_loop(void)
{
	return
		! ctx.rendering.mode.named.perspective && // because we do not write in scanlines then
		! may_skip_peek() &&	// because there may be holes
		! may_skip_poke() &&	// same
		! ctx.rendering.mode.named.blend_coef;	// because it would be too hard
}

static void add_patch(enum patch_type type, enum patch_target target)
{
	assert(nb_patches < sizeof_array(patches));
	patches[nb_patches].addr = gen_dst;
	patches[nb_patches].type = type;
	patches[nb_patches].target = target;
	nb_patches++;
}

static void do_patch(enum patch_target target)
{
	for (unsigned p=0; p<nb_patches; p++) {
		if (patches[p].target == target) {
			int32_t offset = gen_dst - patches[p].addr;	// in words!
			switch (patches[p].type) {
				case offset_24:
					*patches[p].addr |= (offset-2) & 0xffffff;
					break;
			}
		}
	}
}

static void preload_flat(void)
{
	if (in_bh) return;
	// We already know VARP_OUTCOLOR: it's CONSP_COLOR. Preload all.
	assert(vars[CONSTP_COLOR].rnum != -1);
	for (unsigned r=0; r<sizeof_array(regs); r++) {
		if (regs[r].var == VARP_OUTCOLOR) {
			// 1110 0001 1010 0000 Rvar 0000 0000 Rcst ie "mov Rvar, Rconst"
			*gen_dst++ = 0xe1a00000 | (r<<12) | vars[CONSTP_COLOR].rnum;
		}
	}
}

static void begin_write_loop(void)
{
	if (!in_bh && nb_pixels_per_loop > 1) {	// test that varp_count >= nb_pixels_per_loop, or jump straight to the "bottom half"
		assert(nb_pixels_per_loop < 16);
		assert(vars[VARP_COUNT].rnum != -1);
		// 1110 0011 0101 count 0000 0000 0000 0000 ie "cmp Rcount, nb_pixels_per_loop"
		*gen_dst++ = 0xe3500000 | (vars[VARP_COUNT].rnum<<16) | nb_pixels_per_loop;
		add_patch(offset_24, bottom_half);
		// 0011 1010 0000 0000 0000 0000 0000 0000 ie "blo XXXX"
		*gen_dst++ = 0x3a000000;
	}
	write_loop_begin = gen_dst;
}

static void begin_pixel_loop(void)
{
	pixel_loop_begin = gen_dst;
}

static void end_write_loop(void)
{
	if (!in_bh && nb_pixels_per_loop > 1) {
		assert(nb_pixels_per_loop < (1<<8));
		// 1110 0010 0100 _Rn_ _Rd_ 0000 nbpix loop ie "sub rcount, rcount, #nb_pixels_per_loop"
		*gen_dst++ = 0xe2400000 | (vars[VARP_COUNT].rnum<<16) | (vars[VARP_COUNT].rnum<<12) | nb_pixels_per_loop;
		// 1110 0011 0101 _Rn_ 0000 0000 nbpix loop ie "cmp rcount, #nb_pixels_per_loop"
		*gen_dst++ = 0xe3500000 | (vars[VARP_COUNT].rnum<<16) | nb_pixels_per_loop;
		int32_t begin_offset =  (write_loop_begin - (gen_dst+2)) & 0xffffff;
		// 0010 1010 0000 0000 0000 0000 0000 0000 ie "bhs write_loop_begin"
		*gen_dst++ = 0x2a000000 | (begin_offset);
		// 1110 0011 0101 _Rn_ 0000 0000 0000 0000 ie "cmp rcount, #0"
		*gen_dst++ = 0xe3500000 | (vars[VARP_COUNT].rnum<<16);
		// 0000 1010 0000 0000 0000 0000 0000 0000 ie "beq ret"
		add_patch(offset_24, restore_quit);
		*gen_dst++ = 0x0a000000;
		do_patch(bottom_half);
	} else {
		// 1110 0010 0101 _Rn_ _Rd_ 0000 0000 0001 ie "subs rcount, rcount, #1"
		*gen_dst++ = 0xe2500001 | (vars[VARP_COUNT].rnum<<16) | (vars[VARP_COUNT].rnum<<12);
		int32_t begin_offset =  (write_loop_begin - (gen_dst+2)) & 0xffffff;
		// ie "bhs write_loop_begin"
		*gen_dst++ = 0x2a000000 | (begin_offset);
	}
}

static void write_load_var(unsigned r, unsigned v)
{
	uint32_t offset = (uint8_t *)(gen_dst+2) - (uint8_t *)((char *)&ctx + vars[v].offset);
	assert(offset < (1<<12));
	// 1110 0101 0001 1111 Reg offt var_ v__ ie "ldr Rtmp, [r15, #-offset_v]"
	*gen_dst++ = 0xe51f0000 | (r<<12) | offset;
	if (vars[v].offset2 != ~0U) {	// another hop is required
		// 1110 0101 1001 Reg Reg __of fset 12__ ie "ldr Rtmp, [Rtmp, #+offset2*4]"
		*gen_dst++ = 0xe5900000 | (r<<16) | (r<<12) | ((vars[v].offset2<<2)&0xfff);
	}
}

static unsigned load_constp(unsigned v, int rtmp)
{
	if (vars[v].rnum != -1) {
		assert(regs[vars[v].rnum].var == (int)v);
		// we have a dedicated reg
		return vars[v].rnum;
	} else {
		// we need to load into register rtmp, or r14 if not given
		if (rtmp == -1) rtmp = sizeof_array(regs)-1;
		write_load_var(rtmp, v);
		return rtmp;
	}
}

static uint32_t z_mode_cond(void)
{
	// we want to branch when the test _fails_
	switch ((gpuZMode)ctx.rendering.mode.named.z_mode) {
		case gpu_z_gte:
			return 11<<28;
		case gpu_z_ne:
			return 0;
		case gpu_z_eq:
			return 1<<28;
		case gpu_z_gt:
			return 13<<28;
		case gpu_z_lte:
			return 12<<28;
		case gpu_z_lt:
			return 10<<28;
		default:
			assert(0);
	}
	return 0;
}

static void ztest_persp(void)
{
	assert(! in_bh);
	assert(nb_pixels_per_loop == 1);
	unsigned const tmp1 = 0, tmp2 = 1;
	// 1110 0001 1010 0000 tmp1 1000 0100 Decliv  ie "mov tmp1, decliv, asr #16"
	*gen_dst++ = 0xe1a00840 | (tmp1<<12) | vars[VARP_DECLIV].rnum;
	unsigned const constp_out2zb = load_constp(CONSTP_OUT2ZB, tmp2);
	// 1110 0000 1000 _RW_ tmp2 0001 0000 out2zb ie "add tmp2, Rw, out2zb, lsl #2"
	*gen_dst++ = 0xe0800100 | (vars[VARP_W].rnum<<16) | (tmp2<<12) | constp_out2zb;
	// 1110 0111 1001 tmp2 tmp1 nclo g000 tmp1  ie "ldr tmp1, [tmp2, tmp1, lsl #(nc_log+2)]"
	*gen_dst++ = 0xe7900000 | (tmp2<<16) | (tmp1<<12) | ((ctx.poly.nc_log+2)<<7) | tmp1;
	unsigned const constp_z = load_constp(CONSTP_Z, tmp1);
	// 1110 0001 0101 _rZ_ 0000 0000 0000 tmp1 ie "cmp constz, tmp1"
	*gen_dst++ = 0xe1500000 | (constp_z<<16) | tmp1;
	add_patch(offset_24, next_pixel);
	// ie "bZOP XXXXX" branch to next_pixel
	*gen_dst++ = 0x0a000000 | z_mode_cond();
}

static void ztest_nopersp(void)
{
	assert(! in_bh);
	assert(nb_pixels_per_loop == 1);
	unsigned const tmp1 = 0;
	unsigned const constp_out2zb = load_constp(CONSTP_OUT2ZB, tmp1);
	// 1110 0111 1001 _RW_ tmp1 0001 0000 out2zb ie "ldr tmp1, [Rw, out2zb, lsl #2]"
	*gen_dst++ = 0xe7900100 | (vars[VARP_W].rnum<<16) | (tmp1<<12) | constp_out2zb;
	// 1110 0001 0101 _RZ_ 0000 0000 0000 tmp1 ie "cmp rz, tmp1"
	*gen_dst++ = 0xe1500000 | (vars[VARP_Z].rnum<<16) | tmp1;
	add_patch(offset_24, next_pixel);
	// ie "bZOP XXXXX" branch to next_pixel
	*gen_dst++ = 0x0a000000 | z_mode_cond();
}

static void write_mov_immediate(unsigned r, uint32_t imm)
{
	// 1110 0011 1010 0000 tmp2 0000 mask mask ie "mov r, mask"
	*gen_dst++ = 0xe3a00000 | (r<<12) | (imm&0xff);
	imm >>= 8;
	int rot = -8;
	while (imm) {
		uint8_t b = imm&0xff;
		if (b) {
			// 1110 0011 1000 RegR RegR rotI mask mask ie "orr r, r, mask<<8"
			*gen_dst++ = 0xe3800000 | (r<<16) | (r<<12) | (((32+rot)/2)<<8) | b;
		}
		imm >>= 8;
		rot -= 8;
	}
}

static unsigned outcolor_rnum(unsigned pix)
{
	// FIXME: speed this up
	for (unsigned r=0; r<used_set; r++) {
		if (regs[r].var == VARP_OUTCOLOR) {
			if (pix == 0) return r;
			pix --;
		}
	}
	assert(0);
	return 0;
}

static void peek_text(void)	// we always peek, so we can increment our params here (and must, for nb_pixels_per_loop>1)
{
	unsigned const tmp1 = 0, tmp2 = 1, tmp3 = 2;
	unsigned const max_p = in_bh ? 1 : nb_pixels_per_loop;
	for (unsigned p=0; p < max_p; p++) {
		unsigned const rcol = outcolor_rnum(p);
		write_mov_immediate(tmp2, ctx.location.txt_mask);	// TODO: out of loop ?
		// 1110 0000 0000 tmp2 tmp1 1000 0100 varU ie "and tmp1, tmp2, varp_u, asr #16" ie tmp1 = U
		*gen_dst++ = 0xe0000840 | (tmp2<<16) | (tmp1<<12) | vars[VARP_U].rnum;
		// 1110 0000 0000 tmp2 tmp3 1000 0100 varV ie "and tmp3, tmp2, varp_v, asr #16" ie tmp3 = V
		*gen_dst++ = 0xe0000840 | (tmp2<<16) | (tmp3<<12) | vars[VARP_V].rnum;
		// load constp_du, possibly in tmp2
		unsigned const constp_du = load_constp(CONSTP_DU, tmp2);	// TODO: out of loop ?
		// 1110 0000 1000 varU varU 0000 0000 _DU_ ie "add varp_u, varp_u, constp_du"
		*gen_dst++ = 0xe0800000 | (vars[VARP_U].rnum<<16) | (vars[VARP_U].rnum<<12) | constp_du;
		// 1110 0000 1000 tmp1 tmp1 txtw w000 tmp3 ie "add tmp1, tmp1, tmp3, lsl #txt_width" tmp1 = VU
		*gen_dst++ = 0xe0800000 | (tmp1<<16) | (tmp1<<12) | (ctx.location.buffer_loc[gpuTxtBuffer].width_log<<7) | tmp3;
		// load constp_txt, possibly in tmp2
		unsigned const constp_txt = load_constp(CONSTP_TEXT, tmp2);	// TODO: out of loop ?
		// 1110 0111 1001 rtxt outc 0001 0000 tmp1 ie "ldr outcolor, [constp_txt, tmp1, lsl #2]"
		*gen_dst++ = 0xe7900100 | (constp_txt<<16) | (rcol<<12) | tmp1;
		// load constp_dv, possibly in tmp2
		unsigned const constp_dv = load_constp(CONSTP_DV, tmp2);
		// 1110 0000 1000 varV varV 0000 0000 _DV_ ie "add varp_v, varp_v, constp_dv"
		*gen_dst++ = 0xe0800000 | (vars[VARP_V].rnum<<16) | (vars[VARP_V].rnum<<12) | constp_dv;
	}
}

static void peek_text_cond(void)	// this peek is conditionnal : inc our params elsewhere
{
	assert(! in_bh);
	unsigned const tmp1 = 0, tmp2 = 1, tmp3 = 2;
	unsigned const rcol = outcolor_rnum(0);
	write_mov_immediate(tmp2, ctx.location.txt_mask);
	// 1110 0000 0000 tmp2 tmp1 1000 0100 varU ie "and tmp1, tmp2, varp_u, asr #16" ie tmp1 = U
	*gen_dst++ = 0xe0000840 | (tmp2<<16) | (tmp1<<12) | vars[VARP_U].rnum;
	// 1110 0000 0000 tmp2 tmp3 1000 0100 varV ie "and tmp3, tmp2, varp_v, asr #16" ie tmp3 = V
	*gen_dst++ = 0xe0000840 | (tmp2<<16) | (tmp3<<12) | vars[VARP_V].rnum;
	// 1110 0000 1000 tmp1 tmp1 txtw w000 tmp3 ie "add tmp1, tmp1, tmp3, lsl #txt_width" tmp1 = VU
	*gen_dst++ = 0xe0800000 | (tmp1<<16) | (tmp1<<12) | (ctx.location.buffer_loc[gpuTxtBuffer].width_log<<7) | tmp3;
	// load constp_txt, possibly in tmp2
	unsigned const constp_txt = load_constp(CONSTP_TEXT, tmp2);
	// 1110 0111 1001 rtxt outc 0001 0000 tmp1 ie "ldr outcolor, [constp_txt, tmp1, lsl #2]"
	*gen_dst++ = 0xe7900100 | (constp_txt<<16) | (rcol<<12) | tmp1;
}

static void peek_smooth(void)
{
	// For GP2X, r,g,b are y,u,v, and must be stored : VYUY (that is : BRGR)
	// we omit the second 'R', giving B0GR
	unsigned const tmp1 = 0;
	unsigned const max_p = in_bh ? 1 : nb_pixels_per_loop;
	for (unsigned p=0; p < max_p; p++) {
		unsigned const rcol = outcolor_rnum(p);
		// 1110 0010 0000 varG rcol 1100 1111 1111 ie "and rcol, varG, #0xff00"
		*gen_dst++ = 0xe2000cff | (vars[VARP_G].rnum<<16) | (rcol<<12);
		// 1110 0001 1000 rcol rcol 0100 0010 varR ie "orr rcol, rcol, varR, lsr #8"
		*gen_dst++ = 0xe1800420 | (rcol<<16) | (rcol<<12) | vars[VARP_R].rnum;
		// 1110 0010 0000 varG rcol 1100 1111 1111 ie "and tmp1, varB, #0xff00"
		*gen_dst++ = 0xe2000cff | (vars[VARP_B].rnum<<16) | (tmp1<<12);
		// 1110 0001 1000 rcol rcol 1000 0000 tmp1 ie "orr rcol, rcol, tmp1, lsl #16"
		*gen_dst++ = 0xe1800800 | (rcol<<16) | (rcol<<12) | tmp1;
		unsigned const constp_dr = load_constp(CONSTP_DR, tmp1);
		// 1110 0000 1000 varV varV 0000 0000 _DV_ ie "add varR, varR, constp_DR"
		*gen_dst++ = 0xe0800000 | (vars[VARP_R].rnum<<16) | (vars[VARP_R].rnum<<12) | constp_dr;
		unsigned const constp_dg = load_constp(CONSTP_DG, tmp1);
		// 1110 0000 1000 varV varV 0000 0000 _DV_ ie "add varG, varG, constp_DG"
		*gen_dst++ = 0xe0800000 | (vars[VARP_G].rnum<<16) | (vars[VARP_G].rnum<<12) | constp_dg;
		unsigned const constp_db = load_constp(CONSTP_DB, tmp1);
		// 1110 0000 1000 varV varV 0000 0000 _DV_ ie "add varB, varB, constp_DB"
		*gen_dst++ = 0xe0800000 | (vars[VARP_B].rnum<<16) | (vars[VARP_B].rnum<<12) | constp_db;
	}
}

static void peek_smooth_cond(void)	// this peek is conditionnal : inc our params elsewhere
{
	// For GP2X, r,g,b are y,u,v, and must be stored : VYUY (that is : BRGR)
	// we omit the second 'R', giving B0GR
	assert(! in_bh);
	unsigned const tmp1 = 0;
	unsigned const rcol = outcolor_rnum(0);
	// 1110 0010 0000 varG rcol 1100 1111 1111 ie "and rcol, varG, #0xff00"
	*gen_dst++ = 0xe2000cff | (vars[VARP_G].rnum<<16) | (rcol<<12);
	// 1110 0001 1000 rcol rcol 0100 0010 varR ie "orr rcol, rcol, varR, lsr #8"
	*gen_dst++ = 0xe1800420 | (rcol<<16) | (rcol<<12) | vars[VARP_R].rnum;
	// 1110 0010 0000 varG rcol 1100 1111 1111 ie "and tmp1, varB, #0xff00"
	*gen_dst++ = 0xe2000cff | (vars[VARP_B].rnum<<16) | (tmp1<<12);
	// 1110 0001 1000 rcol rcol 1000 0000 tmp1 ie "orr rcol, rcol, tmp1, lsl #16"
	*gen_dst++ = 0xe1800800 | (rcol<<16) | (rcol<<12) | tmp1;
}

static void key_test(void)
{
	assert(! in_bh);
	assert(nb_pixels_per_loop == 1);
	unsigned const constp_key = load_constp(CONSTP_KEY, -1);
	// 1110 0001 0101 rcol 0000 0000 0000 rkey ie "cmp rcol, rkey"
	*gen_dst++ = 0xe1500000 | (vars[VARP_OUTCOLOR].rnum<<16) | constp_key;
	add_patch(offset_24, next_pixel);
	// 0000 1010 0000 0000 0000 0000 0000 0000 ie "beq XXX"
	*gen_dst++ = 0x0a000000;
}

static void intens(void)
{
	unsigned const tmp1 = 0;
	unsigned const max_p = in_bh ? 1 : nb_pixels_per_loop;
	for (unsigned p=0; p < max_p; p++) {
		unsigned const rcol = outcolor_rnum(p);
		// 1110 0010 0000 rcol tmp1 0000 1111 1111 ie "and tmp1, rcol, #0xff" ie tmp1 = Y component
		*gen_dst++ = 0xe20000ff | (rcol<<16) | (tmp1<<12);
		// 1110 0000 1001 tmp1 tmp1 1011 0100 varI ie "adds tmp1, tmp1, varI, asr #22"	// tmp1 = Y + intens
		*gen_dst++ = 0xe0900b40 | (tmp1<<16) | (tmp1<<12) | vars[VARP_I].rnum;
		// 0100 0011 1010 0000 tmp1 0000 0000 0000 ie "movmi tmp1, #0" ie saturate
		*gen_dst++ = 0x43a00000 | (tmp1<<12);
		// 1110 0011 0101 tmp1 0000 1100 0000 0001 ie "cmp tmp1, #0x100"
		*gen_dst++ = 0xe3500c01 | (tmp1<<16);
		// 0101 0011 1010 0000 tmp1 0000 1111 1111 ie "movpl tmp1, #0xff"
		*gen_dst++ = 0x53a000ff | (tmp1<<12);
		// 1110 0011 1100 rcol rcol 0000 1111 1111 ie "bic rcol, rcol, #0xff" ie put new Y into color
		*gen_dst++ = 0xe3c000ff | (rcol<<16) | (rcol<<12);
		// 1110 0001 1000 rcol rcol 0000 0000 tmp1 ie "orr rcol, rcol, tmp1"
		*gen_dst++ = 0xe1800000 | (rcol<<16) | (rcol<<12) | tmp1;
		unsigned const constp_di = load_constp(CONSTP_DI, tmp1);	// TODO: if nb_pixels_per_loop>1, use another tmp and put this out of loop
		// 1110 0000 1000 varI varI 0000 0000 _DI_ ie "add varI, varI, constDI" FIXME: when we can skip pixels, increment later (similar to u,v)
		*gen_dst++ = 0xe0800000 | (vars[VARP_I].rnum<<16) | (vars[VARP_I].rnum<<12) | constp_di;
	}
}

static void poke_persp(void)
{
	assert(! in_bh);
	assert(nb_pixels_per_loop == 1);
	unsigned const tmp1 = 0;
	// 1110 0001 1010 0000 tmp1 1000 0100 decliv ie "mov tmp1, decliv, asr #16" ie tmp1 = decliv>>16
	*gen_dst++ = 0xe1a00840 | (tmp1<<12) | vars[VARP_DECLIV].rnum;
	// 1110 0111 1000 varW rcol nclo g000 tmp1 ie "str rcol, [varW, tmp1, lsl #(nc_log+2)]
	*gen_dst++ = 0xe7800000 | (vars[VARP_W].rnum<<16) | (vars[VARP_OUTCOLOR].rnum<<12) | ((ctx.poly.nc_log+2)<<7) | tmp1;
}

static void poke_z_persp(void)
{
	assert(! in_bh);
	assert(nb_pixels_per_loop == 1);
	unsigned const tmp1 = 0, tmp2 = 1;
	// 1110 0001 1010 0000 tmp1 1000 0100 decliv ie "mov tmp1, decliv, asr #16" ie tmp1 = decliv>>16
	*gen_dst++ = 0xe1a00840 | (tmp1<<12) | vars[VARP_DECLIV].rnum;
	unsigned constp_out2zb = load_constp(CONSTP_OUT2ZB, tmp2);
	// 1110 0000 1000 varW tmp2 0001 0000 out2zb ie "add tmp2, varW, constp_out2zb, lsl #2"
	*gen_dst++ = 0xe0800100 | (vars[VARP_W].rnum<<16) | (tmp2<<12) | constp_out2zb;
	// 1110 0111 1000 tmp2 _RZ_ nclo g000 tmp1 ie "str rz, [tmp2, tmp1, lsl #(nc_log+2)]
	*gen_dst++ = 0xe7800000 | (tmp2<<16) | (vars[VARP_Z].rnum<<12) | ((ctx.poly.nc_log+2)<<7) | tmp1;
}

static void poke_nopersp(void)
{
	if (!ctx.rendering.mode.named.use_key && ctx.rendering.mode.named.z_mode == gpu_z_off) {
		// we increment VARP_W on the go, so that we have nothing left for NEXT_NOPERSP
		if (in_bh || nb_pixels_per_loop == 1) {
			// 1110 0100 1000 varW rcol 0000 0000 0100 ie "str rcol, [rW], #0x4"
			*gen_dst++ = 0xe4800004 | (vars[VARP_W].rnum<<16) | (vars[VARP_OUTCOLOR].rnum<<12);
		} else {
			// 1110 1000 1010 varW regi ster list 16bt ie "smtia rW!, {rcol1-rcolN}"
			*gen_dst++ = 0xe8a00000 | (vars[VARP_W].rnum<<16) | outcolors_mask;
		}
	} else {	// we must not inc VARP_W here
		assert(! in_bh);
		assert(nb_pixels_per_loop == 1);
		// 1110 0101 1000 varW rcol 0000 0000 0000 ie "str rcol, [rW]"
		*gen_dst++ = 0xe5800000 | (vars[VARP_W].rnum<<16) | (vars[VARP_OUTCOLOR].rnum<<12);
	}
}

static void poke_z_nopersp(void)
{
	unsigned const tmp1 = 0;
	unsigned const constp_out2dz = load_constp(CONSTP_OUT2ZB, tmp1);
	if (in_bh || nb_pixels_per_loop == 1) {
		// 1110 0111 1000 varW _RZ_ 0001 0000 out2zb ie "str rz, [rW, out2zb, lsl #2]"
		*gen_dst++ = 0xe7800100 | (vars[VARP_W].rnum<<16) | (vars[VARP_Z].rnum<<12) | constp_out2dz;
	} else {
		// 1110 0000 1000 _RW_ tmp1 0001 0000 out2zb ie "add tmp1, rW, out2zb, lsl #2"
		*gen_dst++ = 0xe0800100 | (vars[VARP_W].rnum<<16) | (tmp1<<12) | constp_out2dz;
		// 1110 1000 1000 tmp1 regi ster list 16bt ie "smtia tmp1, {rz1-rzN}"
		*gen_dst++ = 0xe8800000 | (tmp1<<16) | outz_mask;
	}
}

static void write_combine(void)	// come here with previous color in r0
{
	unsigned const tmp1 = 0, tmp2 = 1, tmp3 = 2, tmp4 = 3;
	write_mov_immediate(tmp3, 0xFF00FCFFU);
	// 1110 0000 0000 tmp1 tmp1 0000 0000 tmp3 ie "and tmp1, tmp1, tmp3" ie tmp1 = tmp1 & mask
	*gen_dst++ = 0xe0000000 | (tmp1<<16) | (tmp1<<12) | tmp3;
	// 1110 0000 0000 rcol tmp4 0000 0000 tmp3 ie "and rcol, rcol, tmp3" ie rcol = rcol & mask
	*gen_dst++ = 0xe0000000 | (vars[VARP_OUTCOLOR].rnum<<16) | (vars[VARP_OUTCOLOR].rnum<<12) | tmp3;
	// FIXME: of course, as blend_coef can be 1, 2 or 3 there are many faster way to compute rcol !
	write_mov_immediate(tmp3, ctx.rendering.mode.named.blend_coef);
	// 1110 0000 1000 tmp4 tmp2 tmp3 1001 tmp1 ie "umull tmp2, tmp4, tmp1, tmp3" ie (tmp2(lo),tmp4(hi)) = tmp1*tmp3
	*gen_dst++ = 0xe0800090 | (tmp4<<16) | (tmp2<<12) | (tmp3<<8) | tmp1;
	write_mov_immediate(tmp3, 4-ctx.rendering.mode.named.blend_coef);
	// 1110 0000 1010 tmp4 tmp2 tmp3 1001 rcol ie "umlal tmp2, tmp4, rcol, tmp3" ie (tmp2(lo),tmp4(hi)) += rcol*tmp3
	*gen_dst++ = 0xe0a00090 | (tmp4<<16) | (tmp2<<12) | (tmp3<<8) | vars[VARP_OUTCOLOR].rnum;
	// 1110 0001 1010 0000 rcol 0001 0010 tmp2 ie "mov rcol, tmp2, lsr #2" ie rcol = (tmp2(lo),tmp4(hi)) >> 2
	*gen_dst++ = 0xe1a00120 | (vars[VARP_OUTCOLOR].rnum<<12) | tmp2;
	// 1110 0001 1000 rcol rcol 1111 0000 tmp4 ie "orr rcol, rcol, tmp4, lsl #30" continuing
	*gen_dst++ = 0xe1800f00 | (vars[VARP_OUTCOLOR].rnum<<16) | (vars[VARP_OUTCOLOR].rnum<<12) | tmp4;
}

static void combine_persp(void)
{
	assert(! in_bh);
	assert(nb_pixels_per_loop == 1);
	unsigned const tmp1 = 0, tmp5 = 4;
	// 1110 0001 1010 0000 tmp5 1000 0100 decliv ie "mov tmp5, decliv, asr #16" ie tmp5 = decliv>>16
	*gen_dst++ = 0xe1a00840 | (tmp5<<12) | vars[VARP_DECLIV].rnum;
	// 1110 0111 1001 varW rcol nclo g000 tmp5 ie "ldr tmp1, [varW, tmp5, lsl #(nc_log+2)]
	*gen_dst++ = 0xe7900000 | (vars[VARP_W].rnum<<16) | (tmp1<<12) | ((ctx.poly.nc_log+2)<<7) | tmp5;
	write_combine();
	// 1110 0111 1000 varW rcol nclo g000 tmp5 ie "str rcol, [varW, tmp5, lsl #(nc_log+2)]
	*gen_dst++ = 0xe7800000 | (vars[VARP_W].rnum<<16) | (vars[VARP_OUTCOLOR].rnum<<12) | ((ctx.poly.nc_log+2)<<7) | tmp5;
}

static void combine_nopersp(void)
{
	assert(! in_bh);
	assert(nb_pixels_per_loop == 1);
	unsigned const tmp1 = 0;
	// 1110 0101 1001 varW tmp1 0000 0000 0000 ie "ldr tmp1, [varW]
	*gen_dst++ = 0xe5900000 | (vars[VARP_W].rnum<<16) | (tmp1<<12);
	write_combine();
	if (!ctx.rendering.mode.named.use_key && ctx.rendering.mode.named.z_mode == gpu_z_off) {
		// 1110 0100 1000 varW rcol 0000 0000 0100 ie "str rcol, [rW], #0x4"
		*gen_dst++ = 0xe4800004 | (vars[VARP_W].rnum<<16) | (vars[VARP_OUTCOLOR].rnum<<12);
	} else {
		// 1110 0101 1000 varW rcol 0000 0000 0000 ie "str rcol, [rW]"
		*gen_dst++ = 0xe5800000 | (vars[VARP_W].rnum<<16) | (vars[VARP_OUTCOLOR].rnum<<12);
	}
}

static void next_persp(void)
{
	assert(! in_bh);
	assert(nb_pixels_per_loop == 1);
	do_patch(next_pixel);
	unsigned const constp_ddecliv = load_constp(CONSTP_DDECLIV, -1);
	// 1110 000c0 1000 decliv decliv 0000 0000 ddecliv ie "add decliv, decliv, ddecliv"
	*gen_dst++ = 0xe0800000 | (vars[VARP_DECLIV].rnum<<16) | (vars[VARP_DECLIV].rnum<<12) | constp_ddecliv;
	unsigned const constp_dw = load_constp(CONSTP_DW, -1);
	// 1110 0000 1000 varW varW 0001 0000 _DW_ ie "add varW, varW, constpDW lsl #2
	*gen_dst++ = 0xe0800100 | (vars[VARP_W].rnum<<16) | (vars[VARP_W].rnum<<12) | constp_dw;
}

static void next_nopersp(void)
{
	do_patch(next_pixel);
	if (ctx.rendering.mode.named.use_key || ctx.rendering.mode.named.z_mode != gpu_z_off) {	// we still have not incremented VARP_W
		assert(nb_pixels_per_loop = 1);
		// 1110 0010 1000 varW varW 0000 0000 0100 ie "add varW, varW, #4"
		*gen_dst++ = 0xe2800004 | (vars[VARP_W].rnum<<16) | (vars[VARP_W].rnum<<12);
	}
}

static void next_z(void)
{
	unsigned const constp_dz = load_constp(CONSTP_DZ, -1);
	// 1110 0000 1000 varZ varZ 0000 0000 _DZ_ ie "add varZ, varZ, rDZ"
	*gen_dst++ = 0xe0800000 | (vars[VARP_Z].rnum<<16) | (vars[VARP_Z].rnum<<12) | constp_dz;
}

static void next_text(void)	// we did not inc our params. Do it now.
{
	assert(! in_bh);
	unsigned const tmp1 = 0;
	// load constp_du, possibly in tmp1
	unsigned const constp_du = load_constp(CONSTP_DU, tmp1);
	// 1110 0000 1000 varU varU 0000 0000 _DU_ ie "add varp_u, varp_u, constp_du"
	*gen_dst++ = 0xe0800000 | (vars[VARP_U].rnum<<16) | (vars[VARP_U].rnum<<12) | constp_du;
	// load constp_dv, possibly in tmp1
	unsigned const constp_dv = load_constp(CONSTP_DV, tmp1);
	// 1110 0000 1000 varV varV 0000 0000 _DV_ ie "add varp_v, varp_v, constp_dv"
	*gen_dst++ = 0xe0800000 | (vars[VARP_V].rnum<<16) | (vars[VARP_V].rnum<<12) | constp_dv;
}

static void next_smooth(void)	// we did not inc our params. Do it now.
{
	assert(! in_bh);
	unsigned const tmp1 = 0;
	unsigned const constp_dr = load_constp(CONSTP_DR, tmp1);
	// 1110 0000 1000 varV varV 0000 0000 _DV_ ie "add varR, varR, constp_DR"
	*gen_dst++ = 0xe0800000 | (vars[VARP_R].rnum<<16) | (vars[VARP_R].rnum<<12) | constp_dr;
	unsigned const constp_dg = load_constp(CONSTP_DG, tmp1);
	// 1110 0000 1000 varV varV 0000 0000 _DV_ ie "add varG, varG, constp_DG"
	*gen_dst++ = 0xe0800000 | (vars[VARP_G].rnum<<16) | (vars[VARP_G].rnum<<12) | constp_dg;
	unsigned const constp_db = load_constp(CONSTP_DB, tmp1);
	// 1110 0000 1000 varV varV 0000 0000 _DV_ ie "add varB, varB, constp_DB"
	*gen_dst++ = 0xe0800000 | (vars[VARP_B].rnum<<16) | (vars[VARP_B].rnum<<12) | constp_db;
}

static void bloc_def_func(void (*cb)(unsigned))
{	
	if (
		ctx.rendering.mode.named.rendering_type == rendering_flat &&
		!ctx.rendering.mode.named.use_intens &&
		ctx.rendering.mode.named.write_out
	) {
		cb(PRELOAD_FLAT);
	}
	cb(BEGIN_WRITE_LOOP);
	cb(BEGIN_PIXEL_LOOP);
	// ZBuffer
	if (ctx.rendering.mode.named.z_mode != gpu_z_off) {
		if (ctx.rendering.mode.named.perspective) cb(ZBUFFER_PERSP);
		else cb(ZBUFFER_NOPERSP);
	}
	// Peek color
	switch ((gpuRenderingType)ctx.rendering.mode.named.rendering_type) {
		case rendering_flat:
			// if intens or blend, we modify the rcols, so that we have to load them at each start of loop
			if (ctx.rendering.mode.named.write_out && (ctx.rendering.mode.named.use_intens || ctx.rendering.mode.named.blend_coef)) cb(PEEK_FLAT);
			break;
		case rendering_text:
			if (ctx.rendering.mode.named.write_out || ctx.rendering.mode.named.use_key) {
				if (may_skip_peek()) cb(PEEK_TEXT_COND);
				else cb(PEEK_TEXT);
			}
			if (ctx.rendering.mode.named.use_key) cb(KEY_TEST);
			break;
		case rendering_smooth:
			if (ctx.rendering.mode.named.write_out) {
				if (may_skip_peek()) cb(PEEK_SMOOTH_COND);
				else cb(PEEK_SMOOTH);
			}
			break;
	}
	// Intens
	if (ctx.rendering.mode.named.use_intens && ctx.rendering.mode.named.write_out) cb(INTENS);
	cb(END_PIXEL_LOOP);
	// Poke
	if (ctx.rendering.mode.named.write_z) {
		if (ctx.rendering.mode.named.perspective) cb(POKE_Z_PERSP);
		else cb(POKE_Z_NOPERSP);
	}
	if (ctx.rendering.mode.named.write_out) {
		if (ctx.rendering.mode.named.perspective) {
			if (ctx.rendering.mode.named.blend_coef) {
				cb(COMBINE_PERSP);
			} else {
				cb(POKE_OUT_PERSP);
			}
		} else {
			if (ctx.rendering.mode.named.blend_coef) {
				cb(COMBINE_NOPERSP);
			} else {
				cb(POKE_OUT_NOPERSP);
			}
		}
	}
	// Next pixel
	if (ctx.rendering.mode.named.perspective) {
		cb(NEXT_PERSP);
	} else {
		cb(NEXT_NOPERSP);
		if (ctx.rendering.mode.named.z_mode != gpu_z_off || ctx.rendering.mode.named.write_z) {
			cb(NEXT_Z);
		}
	}
	switch ((gpuRenderingType)ctx.rendering.mode.named.rendering_type) {
		case rendering_flat:
			break;
		case rendering_text:
			if (ctx.rendering.mode.named.write_out && may_skip_peek()) cb(NEXT_TEXT);
			break;
		case rendering_smooth:
			if (ctx.rendering.mode.named.write_out && may_skip_peek()) cb(NEXT_SMOOTH);
			break;
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

static void alloc_regs(void)
{
	unsigned v, r;
	for (r=0; r<working_set; r++) {	// from 0 to working_set are tmp regs
		regs[r].var = -1;
	}
	unsigned last_v = sizeof_array(vars);
	for (v = MAX_VARP+1; v--; ) {
		if (needed_vars & (1<<v)) {
			if (r >= sizeof_array(regs)) {	// we keep the last reg for tmp loading of constp
				assert(last_v < sizeof_array(vars));
				vars[last_v].rnum = -1;
				regs[sizeof_array(regs)-1].var = -1;
				break;
			} else {
				last_v = v;
				vars[v].rnum = r;
				regs[r].var = v;
				r ++;
			}
		}
	}
	nb_pixels_per_loop = 1;
	outcolors_mask = vars[VARP_OUTCOLOR].rnum != -1 ? 1U<<vars[VARP_OUTCOLOR].rnum:0;	// may be null if !write_color
	outz_mask = vars[VARP_Z].rnum != -1 ? 1U<<vars[VARP_Z].rnum:0;
	if (r < sizeof_array(regs)) {
		// use remaining regs to write several pixels in the loop
		if (may_have_multi_pixels_per_loop()) {
			// we can read several values and poke them all at once
			// notice : that z_mode is off does not mean that we do not want to write Z !
			while (r + ctx.rendering.mode.named.write_out + ctx.rendering.mode.named.write_z <= sizeof_array(regs)) {
				if (ctx.rendering.mode.named.write_out) {
					regs[r].var = VARP_OUTCOLOR;
					outcolors_mask |= 1U<<r;
					r ++;
				}
				if (ctx.rendering.mode.named.write_z) {
					regs[r].var = VARP_Z;
					outz_mask |= 1U<<r;
					r ++;
				}
				nb_pixels_per_loop ++;
			}
		}
	}
	used_set = r;
	while (r < sizeof_array(regs)) {
		regs[r++].var = -1;
	}
}

static void write_save(void)
{
	if (used_set > 4) {
		uint32_t const stm = 0xe92d0000; // 1110 1001 0010 1101 0000 0000 0000 0000 ie "stmdb r13!,{...}
		uint32_t const reg_mask = ((1U<<used_set)-1) & 0x5ff0;
		*gen_dst++ = stm | reg_mask;
	}
	if (used_set >= 13) {
		uint32_t sp_save_offset = (uint8_t *)(gen_dst+2) - (uint8_t *)&ctx.code.sp_save;
		assert(sp_save_offset < 1U<<12);
		// 1110 0101 0000 1111 1101 0000 0000 0000 ie "str r13,[r15, #-sp_save_offset]"
		*gen_dst++ = 0xe50fd000 | sp_save_offset;
	}
}

static void write_restore(void)
{
	do_patch(restore_quit);
	if (used_set >= 13) {
		uint32_t sp_save_offset = (uint8_t *)(gen_dst+2) - (uint8_t *)&ctx.code.sp_save;
		assert(sp_save_offset < 1<<12);
		// 1110 0101 0001 1111 1101 0000 0000 0000 ie "ldr r13,[r15, #-sp_save_offset]
		*gen_dst++ = 0xe51fd000 | sp_save_offset;
	}
	if (used_set > 4) {
		// 1110 1000 1011 1101 0000 0000 0000 0000 ie "ldmia r13!,{...}"
		uint32_t const ldm = 0xe8bd0000;
		uint32_t reg_mask;
		if (used_set > 14) {	// we pushed the back-link, restore it in pc
			reg_mask = 0x9ff0;
		} else {
			reg_mask = ((1U<<used_set)-1) & 0x5ff0;
		}
		*gen_dst++ = ldm | reg_mask;
		if (used_set > 14) return;
	}
	// 1110 0001 1010 0000 1111 0000 0000 1110 ie "mov r15, r14"
	*gen_dst++ = 0xe1a0f00e;
}

static void write_reg_preload(void)
{
	// Now load all required vars into their affected regs
	for (unsigned v = 0; v < sizeof_array(vars); v++) {
		if (vars[v].rnum != -1) {
			if (v == VARP_OUTCOLOR) continue;
			write_load_var(vars[v].rnum, v);
		}
	}
}

static void peek_flat(void)
{
	// We have a reg for the flat color : CONSTP_COLOR. Set all VARP_OUTCOLOR to this.
	assert(vars[CONSTP_COLOR].rnum != -1);
	for (unsigned r=0; r<sizeof_array(regs); r++) {
		if (regs[r].var == VARP_OUTCOLOR) {
			// 1110 0001 1010 0000 Rvar 0000 0000 Rcst ie "mov Rvar, Rconst"
			*gen_dst++ = 0xe1a00000 | (r<<12) | vars[CONSTP_COLOR].rnum;
		}
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
	write_save();
	write_reg_preload();
	in_bh = false;
	bloc_def_func(write_block);
	if (nb_pixels_per_loop > 1) {
		in_bh = true;
		bloc_def_func(write_block);
	}
	write_restore();
}

static uint64_t get_rendering_key(void)
{
	uint32_t key_lo = ctx.rendering.mode.flags;
	uint32_t key_hi = 0x80000000U;	// so a used key is never 0
	if (ctx.rendering.mode.named.perspective) key_hi |= ctx.poly.nc_log == 0;
	if (ctx.rendering.mode.named.rendering_type == rendering_text) key_hi |= ctx.location.txt_mask << 1;	// need 18 bits
	return ((uint64_t)key_hi<<32) | key_lo;
}

#if defined(TEST_RASTERIZER) && !defined(GP2X)
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
	working_set = used_set = 0;
	for (unsigned v=0; v<sizeof_array(vars); v++) {
		vars[v].rnum = -1;
	}
	// init other global vars
	gen_dst = ctx.code.caches[cache].buf;
	write_loop_begin = pixel_loop_begin = NULL;
	nb_patches = 0;
	bloc_def_func(look_regs);
	alloc_regs();
	write_all();
#	if defined(TEST_RASTERIZER) && !defined(GP2X)
	unsigned nb_words = gen_dst - ctx.code.caches[cache].buf;
	assert(nb_words <= sizeof_array(ctx.code.caches[cache].buf));
	static char fname[PATH_MAX];
	snprintf(fname, sizeof(fname), "/tmp/codegen_%"PRIx64, ctx.code.caches[cache].rendering_key);
	int fd = open(fname, O_WRONLY|O_CREAT, 0644);
	if (fd == -1) {
		fprintf(stderr, "Cannot open %s : %s\n", fname, strerror(errno));
	}
	assert(fd != -1);
	write(fd, ctx.code.caches[cache].buf, nb_words * sizeof(uint32_t));
	close(fd);
#	endif
}

void flush_cache(void)
{
#	ifdef GP2X
	// TODO: don't clean all dcache lines
	__asm__ volatile (	// Drain write buffer then fush ICache and DCache
		"MOV r1,#0\n"             // Initialize line counter, r1
		"0:\n"
		"MOV r0,#0\n"             // Initialize segment counter, r0
		"1:\n"
		"ORR r2,r1,r0\n"          // Make segment and line address
		"MCR p15,0,r2,c7,c14,2\n" // Clean and flush that line
		"ADD r0,r0,#0x10\n"       // Increment segment counter
		"CMP r0,#0x40\n"          // Complete all 4 segments?
		"BNE 1b\n"        // If not, branch back to inner_loop
		"ADD r1,r1,#0x04000000\n" // Increment line counter
		"CMP r1,#0x0\n"           // Complete all lines?
		"BNE 0b\n"        // If not, branch back to outer_loop

		"mov r0, #0\n"
		"mcr p15, 0, r0, c7, c10, 4\n"
		"mcr p15, 0, r0, c7, c5, 0\n"
		:::"r0","r1"
	);
#	endif
}

/*
 * Public Functions
 */

struct jit_cache *jit_prepare_rasterizer(void)
{
	uint64_t key = get_rendering_key();
	int r_dest = -1;
	for (unsigned r=0; r<sizeof_array(ctx.code.caches); r++) {
		if (ctx.code.caches[r].rendering_key == key) {
			ctx.code.caches[r].use_count ++;
			return ctx.code.caches+r;
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
	flush_cache();
	return ctx.code.caches+r_dest;
}

void jit_invalidate(void)	// TODO: give hint as to what invalidate
{
	for (unsigned r=0; r<sizeof_array(ctx.code.caches); r++) {
		ctx.code.caches[r].rendering_key = 0;	// meaning : not set
	}
}

extern inline void jit_exec(void);
