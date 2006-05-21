   .text
#ifdef GP2X
	.align
	.global _start
	.global _icache_flush
	.global _dcache_flush
	.global _icache_prefetch
	.global _dbuffer_drain

_start:
	LDR PC, =_xxinit @ Reset
	LDR PC, =_undefi @ Undefined instruction
	LDR PC, =_undefi @ Software Interrupt
	LDR PC, =_undefi @ Prefetch abort
	LDR PC, =_undefi @ Data abort
	LDR PC, =_undefi @ Reserved
	LDR PC, =_undefi @ Reserved
	LDR PC, =_irq @ IRQ
	LDR PC, =_undefi @ FIQ

_xxinit:
	// After reset, all bits 0 of register 6 are reset (all region disabled).
	// So we just enable what we need.
	// Region 0 is the default for data, where shared data will reside. We want uncacheable but bufferable
	mov r0, #0x3F	// 4GB page starting at 0x0
	mcr p15, 0, r0, c6, c0, 0
	// Region 1 is for the MP registers, at address 0xC0000000 to the end (the last gigabyte)
	mov r0, #0xC0000000	// base at registers start
	orr r0, r0, #0x3B	// size of 1Gb
	mcr p15, 0, r0, c6, c1, 0	// this is data region1
	// Region 2 is both for our code, datas and stack. It's the first 64Kb.
	mov r0, #0x1F
	mcr p15, 0, r0, c6, c2, 0
	mcr p15, 0, r0, c6, c2, 1
	// And that's all.
	// Only region 2 is cacheable, both for code and data
	mov r0, #0x4
	mcr p15, 0, r0, c2, c0, 0
	mcr p15, 0, r0, c2, c0, 1
	// Only regions 0 and 2 are bufferables (IO registers are not)
	mov r0, #0x5
	mcr p15, 0, r0, c3, c0, 0
	// Full access for all runlevels to all these regions for datas
	mov r0, #0x3F
	mcr p15, 0, r0, c5, c0, 0
	// Only to region 2 for instructions
	mov r0, #0x30
	mcr p15, 0, r0, c5, c0, 1
	// Now enable all these nice features
	mov r0, #0xC0000000	// Asynchronous (+FastBus) clocking (don't know what's this, but sounds quite kewl)
	orr r0, r0, #0x1000	// ICache enable
	orr r0, r0, #0x7D		// DCache and Protection Unit enable
	mcr p15, 0, r0, c1, c0, 0

	mov r13, #0x10000	// 64Kb should be enought for everyone
	ldr r12, =0xDEADBEEF	// stamp a signature in stack so we know core is running and can detect stack underflow
	ldr r11, =0xFACE0000
	stmdb r13!, {r11,r12}
	bl mymain
	// TODO sheck for dead beef face
	b _xxinit

_undefi:
	// TODO : blink the battery LED when this situation arrises
	b _start

_irq:
	sub r14, r14, #4
	stmdb sp!, {r0-r3, r14}
	bl vertical_interrupt
	ldmia sp!, {r0-r3, pc}^

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

	.pool
#endif
	.end
