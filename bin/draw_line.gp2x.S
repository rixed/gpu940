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

#ifdef GP2X
#include "offsets.S"

.text
.globl draw_line_c
.globl draw_line_ci
.globl draw_line_uv
.globl draw_line_uvi
.globl draw_line_uvi_lin
.globl patch_uv_width
.globl patch_uvi_width
.globl patch_uvi_lin_width
.globl patch_uv_nc_log
.globl patch_ci_nc_log
.globl patch_uvi_nc_log
.globl patch_uv_dw
.globl patch_ci_dw
.globl patch_uvi_dw
.align 4
draw_line_c:
	ldr r3, .ctx
	ldr r0, [r3, #OFF_ctx.line.count]
	ldr r1, [r3, #OFF_ctx.line.w]
	ldr r2, [r3, #OFF_ctx.line.dw]
	ldr r3, [r3, #OFF_ctx.poly.cmdFacet.color]
	cmp r0, #8
	bls 1f
	stmfd sp!, {r4-r10}
	mov r4, r3
	mov r5, r3
	mov r6, r3
	mov r7, r3
	mov r8, r3
	mov r9, r3
	mov r10, r3
0:
	subs r0, r0, #8
	stmhiia r1!, {r3-r10}
	bhi 0b
	add r0, r0, #8
	ldmfd sp!, {r4-r10}
1:
	subs r0, r0, #2
	str r3, [r1], r2
	strpl r3, [r1], r2
	bhi 1b
	mov pc, lr

draw_line_ci:
	stmfd sp!, {r4-r8, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.w]
	ldr r3, [lr, #OFF_ctx.line.decliv]
	ldr r4, [lr, #OFF_ctx.line.ddecliv]
	ldr r5, [lr, #OFF_ctx.line.param]	// r5 = p
	add r2, r5, r5, lsl #1	// r2 = p*3
	add r5, r2, r5, lsl #3	// r5 = p*3+p*8 = p*11
	add r5, r5, r5, lsl #2	// r5 = p*11+(p*11)*4 = p*55
	ldr r6, [lr, #OFF_ctx.line.dparam]
	add r2, r6, r6, lsl #1
	add r6, r2, r6, lsl #3
	add r6, r6, r6, lsl #2
	ldr r7, [lr, #OFF_ctx.poly.cmdFacet.color]
	bic r7, r7, #0x0000ff
	bic r7, r7, #0xff0000	// color without Y
	ldrb r8, [lr, #OFF_ctx.poly.cmdFacet.color] // Y
	ldr lr, [lr, #OFF_ctx.line.count]
0:
	// compute color
	adds r2, r8, r5, asr #22	// r2 = Y + intens
	movmi r2, #0	// saturate
	cmp r2, #0x100
	movpl r2, #0xff
	orr r2, r2, r2, lsl #16
	orr r2, r7, r2
	// poke it
	mov r1, r3, asr #16	// r1 = decliv>>16
patch_ci_nc_log:
	str r2, [r0, r1, asl #2]	// write color into that addy (shifter by nc_log, patched)
	// loop
patch_ci_dw:
	add r0, r0, #0x12	// w += dw
	add r3, r3, r4	// decliv += ddecliv
	add r5, r5, r6	// param += dparam
	subs lr, lr, #1	// count--
	bge 0b
	// quit
	ldmfd	sp!, {r4-r8, pc}
	
draw_line_uv:
	stmfd	sp!, {r4-r10, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.w]
	ldr r3, [lr, #OFF_ctx.line.decliv]
	ldr r4, [lr, #OFF_ctx.line.ddecliv]
	ldr r5, [lr, #OFF_ctx.line.param]	// U
	ldr r6, [lr, #OFF_ctx.line.dparam]	// DU
	ldr r7, [lr, #OFF_ctx.line.param+4]	// V
	ldr r8, [lr, #OFF_ctx.line.dparam+4]	// DV
	ldr r9, [lr, #OFF_ctx.location.txt_mask]
	ldr r10, [lr, #OFF_ctx.location.txt.address]
	mov r10, r10, lsl #2
	add r10, r10, #BUFFERS_ADDR
	ldr lr, [lr, #OFF_ctx.line.count]
0:
	// peek
	and r1, r9, r5, asr #16	// r1 = U
	and r2, r9, r7, asr #16	// r2 = V
	add r5, r5, r6	// U += DU
patch_uv_width:
	add r1, r1, r2, lsl #8	// r1 = VU (this #8 to be patched when txt width change)
	add r7, r7, r8	// V += DV
	ldr r2, [r10, r1, lsl #2]	// r2 = texel
	// poke
	mov r1, r3, asr #16	// r1 = decliv>>16
	add r3, r3, r4	// decliv += ddecliv
	subs lr, lr, #1	// count--
patch_uv_nc_log:
	str r2, [r0, r1, asl #2]	// write color into that addy (shifted by nc_log, patched)
	// loop
patch_uv_dw:
	add r0, r0, #0x12	// w += dw
	bge 0b
	// quit
	ldmfd	sp!, {r4-r10, pc}

draw_line_uvi:
	stmfd	sp!, {r4-r12, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.w]
	ldr r3, [lr, #OFF_ctx.line.decliv]
	ldr r4, [lr, #OFF_ctx.line.ddecliv]
	ldr r5, [lr, #OFF_ctx.line.param]	// U
	ldr r6, [lr, #OFF_ctx.line.dparam]	// DU
	ldr r7, [lr, #OFF_ctx.line.param+4]	// V
	ldr r8, [lr, #OFF_ctx.line.dparam+4]	// DV
	ldr r11, [lr, #OFF_ctx.line.param+8]	// I
	add r9, r11, r11, lsl #1	// r9 = I*3
	add r11, r9, r11, lsl #3	// r11 = I*3+I*8 = I*11
	add r11, r11, r11, lsl #2	// r11 = I*11+(I*11)*4 = I*55
	ldr r12, [lr, #OFF_ctx.line.dparam+8]	// DI
	add r9, r12, r12, lsl #1
	add r12, r9, r12, lsl #3
	add r12, r12, r12, lsl #2
	ldr r9, [lr, #OFF_ctx.location.txt_mask]
	ldr r10, [lr, #OFF_ctx.location.txt.address]
	mov r10, r10, lsl #2
	add r10, r10, #BUFFERS_ADDR
	ldr lr, [lr, #OFF_ctx.line.count]
0:
	// peek
	and r1, r9, r5, asr #16	// r1 = U
	and r2, r9, r7, asr #16	// r2 = V
	add r5, r5, r6	// U += DU
patch_uvi_width:
	add r1, r1, r2, lsl #8	// r1 = VU (this #8 to be patched when txt width change)
	ldr r2, [r10, r1, lsl #2]	// r2 = texel
	add r7, r7, r8	// V += DV
	// transform Y
	and r1, r2, #0xff	// r1 = Y
	adds r1, r1, r11, asr #22	// r2 = Y + intens
	movmi r1, #0	// saturate
	cmp r1, #0x100
	movpl r1, #0xff
	bic r2, r2, #0xff	// we only put Y here, because the upper copy (bits 16-23) seams useless by now
	add r11, r11, r12	// I += DI
	orr r2, r2, r1
	// poke
	mov r1, r3, asr #16	// r1 = decliv>>16
	add r3, r3, r4	// decliv += ddecliv
	subs lr, lr, #1	// count--
patch_uvi_nc_log:
	str r2, [r0, r1, asl #2]	// write color into that addy (shifted by nc_log, patched)
	// loop
patch_uvi_dw:
	add r0, r0, #0x12	// w += dw
	bge 0b
	// quit
	ldmfd	sp!, {r4-r12, pc}

draw_line_uvi_lin:
	stmfd	sp!, {r4-r12, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.w]
	ldr r5, [lr, #OFF_ctx.line.param]	// U
	ldr r6, [lr, #OFF_ctx.line.dparam]	// DU
	ldr r7, [lr, #OFF_ctx.line.param+4]	// V
	ldr r8, [lr, #OFF_ctx.line.dparam+4]	// DV
	ldr r11, [lr, #OFF_ctx.line.param+8]	// I
	add r9, r11, r11, lsl #1	// r9 = I*3
	add r11, r9, r11, lsl #3	// r11 = I*3+I*8 = I*11
	add r11, r11, r11, lsl #2	// r11 = I*11+(I*11)*4 = I*55
	ldr r12, [lr, #OFF_ctx.line.dparam+8]	// DI
	add r9, r12, r12, lsl #1
	add r12, r9, r12, lsl #3
	add r12, r12, r12, lsl #2
	ldr r9, [lr, #OFF_ctx.location.txt_mask]
	ldr r10, [lr, #OFF_ctx.location.txt.address]
	mov r10, r10, lsl #2
	add r10, r10, #BUFFERS_ADDR
	ldr lr, [lr, #OFF_ctx.line.count]
0:
	// peek
	and r1, r9, r5, asr #16	// r1 = U
	and r2, r9, r7, asr #16	// r2 = V
	add r5, r5, r6	// U += DU
patch_uvi_lin_width:
	add r1, r1, r2, lsl #8	// r1 = VU (this #8 to be patched when txt width change)
	ldr r2, [r10, r1, lsl #2]	// r2 = texel
	add r7, r7, r8	// V += DV
	// transform Y
	and r1, r2, #0xff	// r1 = Y
	adds r1, r1, r11, asr #22	// r2 = Y + intens
	movmi r1, #0	// saturate
	cmp r1, #0x100
	movpl r1, #0xff
	bic r2, r2, #0xff	// we only put Y here, because the upper copy (bits 16-23) seams useless by now
	add r11, r11, r12	// I += DI
	orr r2, r2, r1
	// poke
	subs lr, lr, #1	// count--
	str r2, [r0], #0x4	// write color into that addy, and add 4 to r0
	// loop
	bge 0b
	// quit
	ldmfd	sp!, {r4-r12, pc}

.ctx: .word ctx

#endif
