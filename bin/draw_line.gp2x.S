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
.globl patch_uv_width_code
.align 4
draw_line_c:
	ldr r3, .ctx
	ldr r0, [r3, #OFF_ctx.line.count]
	ldr r1, [r3, #OFF_ctx.line.w]
	ldr r2, [r3, #OFF_ctx.line.dw]
	ldr r3, [r3, #OFF_ctx.poly.cmdFacet.color]
0:
	subs r0, r0, #2
	str r3, [r1], r2
	strpl r3, [r1], r2
	bhi 0b
	mov pc, lr

draw_line_ci:
	stmfd sp!, {r4-r10, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.w]
	ldr r1, [lr, #OFF_ctx.line.dw]
	ldr r2, [lr, #OFF_ctx.poly.nc_log]
	ldr r3, [lr, #OFF_ctx.line.decliv]
	ldr r4, [lr, #OFF_ctx.line.ddecliv]
	ldr r5, [lr, #OFF_ctx.line.param]	// r5 = p
	add r9, r5, r5, lsl #1	// r9 = p*3
	add r5, r9, r5, lsl #3	// r5 = p*3+p*8 = p*11
	add r5, r5, r5, lsl #2	// r5 = p*11+(p*11)*4 = p*55
	ldr r6, [lr, #OFF_ctx.line.dparam]
	add r9, r6, r6, lsl #1
	add r6, r9, r6, lsl #3
	add r6, r6, r6, lsl #2
	ldr r7, [lr, #OFF_ctx.poly.cmdFacet.color]
	bic r7, r7, #0x0000ff
	bic r7, r7, #0xff0000	// color without Y
	ldrb r8, [lr, #OFF_ctx.poly.cmdFacet.color] // Y
	ldr lr, [lr, #OFF_ctx.line.count]
0:
	// compute color
	adds r9, r8, r5, asr #22	// r9 = Y + intens
	movmi r9, #0	// saturate
	cmp r9, #0x100
	movpl r9, #0xff
	orr r9, r9, r9, lsl #16
	orr r9, r7, r9
	// poke it
	mov r10, r3, asr #16	// r10 = decliv>>16
	mov r10, r10, asl r2	// r10 <<= nc_log
	str r9, [r0, r10]	// write color into that addy
	// loop
	add r0, r0, r1	// w += dw
	add r3, r3, r4	// decliv += ddecliv
	add r5, r5, r6	// param += dparam
	subs lr, lr, #1	// count--
	bge 0b
	// quit
	ldmfd	sp!, {r4-r10, pc}
	
draw_line_uv:
	stmfd	sp!, {r4-r12, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.w]
	ldr r1, [lr, #OFF_ctx.line.dw]
	ldr r2, [lr, #OFF_ctx.poly.nc_log]
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
	and r12, r9, r5, asr #16	// r12 = U
	and r11, r9, r7, asr #16	// r11 = V
	add r5, r5, r6	// U += DU
patch_uv_width_code:
	add r12, r12, r11, lsl #6	// r12 = VU*4 (this #8 to be patched when txt width change)
	add r7, r7, r8	// V += DV
	ldr r11, [r10, r12, lsl #2]	// r11 = texel
	// poke
	mov r12, r3, asr #16	// r12 = decliv>>16
	mov r12, r12, asl r2	// r12 <<= nc_log
	add r3, r3, r4	// decliv += ddecliv
	subs lr, lr, #1	// count--
	str r11, [r0, r12]	// write color into that addy
	// loop
	add r0, r0, r1	// w += dw
	bge 0b
	// quit
	ldmfd	sp!, {r4-r12, pc}

.shared: .word shared
.ctx: .word ctx

#endif
