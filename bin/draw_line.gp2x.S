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
.align 4
.globl draw_line_c
draw_line_c:
	stmfd sp!, {r4-r6, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.count]
	ldr r1, [lr, #OFF_ctx.poly.nc_scale]
	ldr r2, [lr, #OFF_ctx.line.w]
	ldr r3, [lr, #OFF_ctx.line.dw]
	ldr r4, [lr, #OFF_ctx.line.decliv]
	ldr r5, [lr, #OFF_ctx.line.ddecliv]
	ldrh lr, [lr, #OFF_ctx.poly.cmdFacet.color]
0:
	mov r6, r4, asr #16	// r6 = decliv>>16
	mov r6, r6, asl r1	// r6 <<= nc_scale
	strh lr, [r2, r6]		// write color into that addy
	add r2, r2, r3			// w += dw
	add r4, r4, r5			// decliv += ddecliv
	subs r0, r0, #1		// count--
	bge 0b
	ldmfd	sp!, {r4-r6, pc}
	
.align 4
.globl draw_line_uv
draw_line_uv:
	stmfd	sp!, {r4-r11, lr}
	ldr lr, .ctx
	ldr r0, [lr, #OFF_ctx.line.count]
	ldr r1, [lr, #OFF_ctx.poly.nc_scale]
	ldr r2, [lr, #OFF_ctx.line.w]
	ldr r3, [lr, #OFF_ctx.line.dw]
	ldr r4, [lr, #OFF_ctx.line.decliv]
	ldr r5, [lr, #OFF_ctx.line.ddecliv]	
	ldr r6, [lr, #OFF_ctx.line.param]	// u
	ldr r7, [lr, #OFF_ctx.line.dparam]	// du
	ldr r8, [lr, #(OFF_ctx.line.param+4)]	// v
	ldr r9, [lr, #(OFF_ctx.line.dparam+4)]	// dv
	ldrb lr, [lr, #OFF_ctx.poly.cmdFacet.texture]
	ldr r11, .shared
	ldr r11, [r11]
	add r11, r11, #OFF_shared.textures
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
	mov lr, lr, asl r1		// lr <<= nc_scale					2 (register controled shift)
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
