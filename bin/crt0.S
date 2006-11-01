   .text
#ifdef GP2X
	.align
/*	.global _start
	.global _icache_flush
	.global _dcache_flush
	.global _icache_prefetch
	.global _dbuffer_drain*/
	.global my_memset_words

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

	.pool
#endif
	.end
