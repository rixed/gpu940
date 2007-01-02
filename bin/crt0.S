   .text
#ifdef GP2X
	.align
/*	.global _start
	.global _icache_flush
	.global _dcache_flush
	.global _icache_prefetch
	.global _dbuffer_drain*/
	.global my_memset_words
	.global unsigned_divide32
	.global unsigned_divide64
	.global __ashldi3

_start:
	b _reset @ Reset
	ldr pc, =undef_instr @//subs pc, r14, #4 @ Undefined instruction
	ldr pc, =swi @ Software Interrupt
	ldr pc, =prefetch_abrt @ Prefetch abort
	ldr pc, =data_abrt @ Data abort
	ldr pc, =reserved @ Reserved
	b _safe_irq_handler @ IRQ
	ldr pc, =fiq @ FIQ

_reset:
	// Here is the memory map we set up :
	// From 0 to 0x10000 is our code + all initialized datas + bss
	// From 0x10000 to 0x100000 is nothing, where we put our two stacks : at 0x80000 ours, at 0x100000 irq's.
	// From 0x100000 to the end lies our shared structure.
	// The first 0x10000 bytes are for commands and various datas : we set this uncached but buffered
	// All other memory (except IO) are cacheable.
	//
	// Data region 0 is the default for data.
	mov r0, #0x3F	// 4GB page starting at 0x0
	mcr p15, 0, r0, c6, c0, 0
	// Code region 0 is for our code. It's the first 64Kb.
	mov r0, #0x1F
	mcr p15, 0, r0, c6, c0, 1
	// Data region 1 is for the MP registers, at address 0xC0000000-0x2000000 to the end (the last gigabyte)
	orr r0, r0, #0xBE000000	// base at registers start
	mcr p15, 0, r0, c6, c1, 0
	// Data region 2 is for commands and volatiles
	mov r0, #0x27
	orr r0, r0, #0x100000
	mcr p15, 0, r0, c6, c2, 0
	// And that's all.
	// Data region 0 is cacheable/bufferable, data region 1 is uncacheable/unbufferable, date region 2 is uncacheable/bufferable
	mov r0, #0x1
	mcr p15, 0, r0, c2, c0, 0
	mov r0, #0x1
	mcr p15, 0, r0, c2, c0, 1	// code region 0 is cacheable also
	mov r0, #0x5
	mcr p15, 0, r0, c3, c0, 0
	// Full access for all runlevels to all these regions for datas
	mov r0, #0xFF
	mcr p15, 0, r0, c5, c0, 0
	// Only to region 0 for code
	mov r0, #0x3
	mcr p15, 0, r0, c5, c0, 1
	// Now enable all these nice features
	mov r0, #0xC0000000	// Asynchronous (+FastBus) clocking (don't know what's this, but sounds quite kewl)
	orr r0, r0, #0x1000	// ICache enable
	orr r0, r0, #0x7D		// DCache and Protection Unit enable
	mcr p15, 0, r0, c1, c0, 0

	mov r13, #0x80000	// Our stack lives here
	ldr r12, =0xDEADBEEF	// stamp a signature in stack so we know core is running and can detect stack underflow
	ldr r11, =0xFACE0000
	stmdb r13!, {r11,r12}
	bl mymain
	// TODO check for dead beef face
	b _reset

_undefi:
	// TODO : blink the battery LED when this situation arrises
	b _start

_safe_irq_handler:
	sub lr, lr, #4
	mov sp, #0x100000	// IRQ stack goes here
	stmfd sp!, {r0 - r12, lr}
	bl irq_handler
	ldmfd sp!, {r0 - r12, pc}^

_icache_flush:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0
	bx lr

_dcache_flush:
	mov r0, #0
	mcr p15, 0, r0, c7, c6, 0
	bx lr

// address to prefetch in R0
_icache_prefetch:
	mcr p15, 0, r0, c7, c13, 1
	bx lr

_dbuffer_drain:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 4	
	bx lr

my_memset_words:	// r0 = dst, r1 = value, r2 = count
	cmp r2, #11
	bls 1f
	stmfd sp!, {r4-r12}
	mov r3, r1
	mov r4, r1
	mov r5, r1
	mov r6, r1
	mov r7, r1
	mov r8, r1
	mov r9, r1
	mov r10, r1
	mov r11, r1
	mov r12, r1
0:
	subs r2, r2, #11
	stmhsia r0!, {r1,r3-r12}
	bhi 0b
	add r2, r2, #11
	ldmfd sp!, {r4-r12}
1:
	subs r2, r2, #1
	strhs r1, [r0], #4
	bhi 1b
	mov pc, lr

unsigned_divide32:
	// in : r0 = R
	//      r1 = D
	stmdb sp!, {r4, lr}
	mov r2, r0	// r2 = R   (keep r0 for result)
	mov r0, #0
	// if (R < D) return 0
	cmp r2, r1
	blo 0f
	// r4 = e = 0
	mov r4, #0
	// r3 = R>>1  (R/2>=1, D>=1)
	mov r3, r2, lsr #1
1:
	add r4, r4, #1
	cmp r3, r1, lsl r4
	bhs 1b
	mov r3, r1, lsl r4	// r3 = normD
	mov lr, #1
	mov r4, lr, lsl r4	// r4 = 1 << e
3:	// do {
	cmp r3, r2
	// 	Q += e
	addls r0, r0, r4
	// 	R -= normD
	subls r2, r2, r3
	// 	normD >>= 1
	mov r3, r3, lsr #1
	// 	e >>= 1
	mov r4, r4, lsr #1
	// 	if (R < D) return 0
	cmp r2, r1
	bhs 3b
	// } while (1);
0:	ldmia sp!, {r4, pc}

unsigned_divide64:
	// in : (r0, r1) = R (r0 = low)
	//      (r2, r3) = D (r2 = low)
	stmdb sp!, {r4-r12, lr}
	mov r4, r0	// (r4, r10) = R   (keep r0 for result)
	mov r10, r1
	mov r0, #0	// (r0, r1) = Q
	mov r1, #0
	// if (R < D) return 0
	cmp r10, r3
	blo 0f
	cmpeq r4, r2
	blo 0f
	// (r5, r6) = normD
	// (r0, r1) = Q
	// (r7, r11) = e
	// (r8, r9) = next_normD
	mov r7, #1
	mov r11, #0
	mov r8, r2
	mov r9, r3
1:	// do {
	// 	normD = next_normD
	mov r5, r8
	mov r6, r9
	// 	next_normD <<= 1;
	adds r8, r8, r8
	adc r9, r9, r9
	// 	e <<= 1;
	adds r7, r7, r7
	adc r11, r11, r11
	// } while (next_normD <= R)
	cmp r9, r10
	blo 1b
	bhi 2f
	cmp r8, r4
	bls 1b
2:	// e >>= 1
	movs r11, r11, lsr #1
	mov r7, r7, rrx
3:	// do {
	// 	(r8, r9) = temp_normD
	// 	(lr, r12) = temp_e
	mov r8, r5
	mov r9, r6
	mov lr, r7
	mov r12, r11
	cmp r6, r10
	movhi r8, #0
	movhi r9, #0
	movhi lr, #0
	movhi r12, #0
	cmpeq r5, r4
	movhi r8, #0
	movhi r9, #0
	movhi lr, #0
	movhi r12, #0
	// 	Q += temp_e
	adds r0, r0, lr
	adc r1, r1, r12
	// 	R -= temp_normD
	subs r4, r4, r8
	sbc r10, r10, r9
	// 	if (R < D) return 0
	cmp r10, r3
	blo 0f
	cmpeq r4, r2
	blo 0f
	// 	normD >>= 1
	movs r6, r6, lsr #1
	mov r5, r5, rrx
	// 	e >>= 1
	movs r11, r11, lsr #1
	mov r7, r7, rrx
	// } while (1);
	b 3b
0:	ldmia sp!, {r4-r12, pc}

// Ripped from libgcc.a, used by code compiled with official devkit's gcc
__ashldi3:
	cmp     r2, #0
	stmdb   sp!, {r4, r5, r6, lr}
	rsb     ip, r2, #32
	mov     r4, r1
	mov     r3, r0
	mov     r6, #0
	mov     r5, #0
	beq     0f
	cmp     ip, #0
	mov     r3, r1, lsl r2
	rsb     lr, ip, #0
	movle   r5, #0
	movle   r6, r0, lsl lr
	movgt   r5, r0, lsl r2
	orrgt   r6, r3, r0, lsr ip
	mov     r4, r6
	mov     r3, r5
0:
	mov     r1, r4
	mov     r0, r3
	ldmia   sp!, {r4, r5, r6, pc}

	.pool
#endif
	.end
