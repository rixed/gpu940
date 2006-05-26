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
.align 4
draw_line_c:
	ldr r3, .ctx
	ldr r0, [r3, #OFF_ctx.line.count]
	ldr r1, [r3, #OFF_ctx.line.w]
	ldr r2, [r3, #OFF_ctx.line.dw]
	ldr r3, [r3, #OFF_ctx.poly.cmdFacet.color]
0:
	subs r0, r0, #1		// count--
	str r3, [r1]		// write color into that addy
	add r1, r1, r2			// w += dw
	bge 0b
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
	
	
draw_line_uv_OLD:
	stmfd	sp!, {r4-r11, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.count]
	ldr r1, [lr, #OFF_ctx.poly.nc_log]
	ldr r2, [lr, #OFF_ctx.line.w]
	ldr r3, [lr, #OFF_ctx.line.dw]
	ldr r4, [lr, #OFF_ctx.line.decliv]
	ldr r5, [lr, #OFF_ctx.line.ddecliv]	
	ldr r6, [lr, #OFF_ctx.line.param]	// u
	ldr r7, [lr, #OFF_ctx.line.dparam]	// du
	ldr r8, [lr, #(OFF_ctx.line.param+4)]	// v
	ldr r9, [lr, #(OFF_ctx.line.dparam+4)]	// dv
//	ldr lr, [lr, #OFF_ctx.poly.cmdFacet.texture]
	ldr r11, .shared
	ldr r11, [r11]
//	add r11, r11, #OFF_shared.textures
	add r11, r11, lr, lsl #9	// points to the chosen texture
	// lr is free
0:
	// peek
	and r10, r6, #0xff0000	//											1
	and lr, r8, #0xff0000	//											1
	add r10, r10, lr, lsl #8	// r10 = VU00							1
	mov r10, r10, lsr #15		// offset into texture				1
	ldrh r10, [r11, r10]			// r10 = texel							1 or 2 ?
	add r6, r6, r7				// u += du								1
	add r8, r8, r9				// v += dv								1
	// poke
	mov lr, r4, asr #16		// lr = decliv>>16					1
	mov lr, lr, asl r1		// lr <<= nc_log					2 (register controled shift)
	strh r10, [r2, lr]			// write color into that addy		1
	add r2, r2, r3				// w += dw								1
	add r4, r4, r5				// decliv += ddecliv					1
	// loop
	subs r0, r0, #1			// count--								1
	bge 0b						//											3
	ldmfd	sp!, {r4-r11, pc}	//			total = 17 or 18 per pixel

.shared: .word shared
.ctx: .word ctx

#endif
