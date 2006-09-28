#ifndef N
#	define N 1000
#endif

   .text
	.align
_start:
	// We run straight from here, ignoring other interrupts
	// First, setup the protection unit : we disable dcache and write buffer altogether, and enable icache everywhere.
	mov r0, #0x3F	// 4GB page starting at 0x0
	mcr p15, 0, r0, c6, c0, 0	// this is data region 0
	mcr p15, 0, r0, c6, c0, 1	// this is code region 0
	mov r0, #0x0
	mcr p15, 0, r0, c2, c0, 0	// data region 0 is not cacheable
	mov r0, #0x1
	mcr p15, 0, r0, c2, c0, 1	// code region 0 is cacheable
	mov r0, #0x0
	mcr p15, 0, r0, c3, c0, 0	// no write buffer anywhere
	// Full access for all runlevels to everything
	mov r0, #0xFF
	mcr p15, 0, r0, c5, c0, 0	// data
	mcr p15, 0, r0, c5, c0, 1	// and code
	// Now do it
	mov r0, #0xC0000000	// Asynchronous (+FastBus) clocking (don't know what's this, but sounds quite kewl)
	orr r0, r0, #0x1000	// ICache enable
	orr r0, r0, #0x79		// DCache disabled, Protection Unit enabled
	
	mcr p15, 0, r0, c1, c0, 0
	
	mov r13, #0x10000		// Just in case we add subfunctions in this little test, locate our stack there

	// Set 940 to 200MHz
	// Without this, every noticed oddities are the same, merely slower
	ldr r1, gp2x_regs
	mov r0, #0x4900
	orr r0, r0, #0x4
	mov r2, #0x0910
	strh r0, [r1, r2]
	sub r2, r2, #0xE
0:	// wait for magick to happen
	ldrh r0, [r1, r2]
	tst r0, #0x1
	bne 0b
	
	// Now perform the test
	ldr r0, =1000000	// repetition count
	mov r1, #0x0200000	// base address = 2Mb
	mov r2, #0x0	// offset
	ldr r3, =0xDEADBEAF	// the value we write
	ldr r4, =N	// the distance between 2 writes
	ldr r5, =0xFFFFF	// Mask for 1Mb
0:
	str r3, [r1, r2]	// write a word
	add r2, r2, r4, LSL #2	// increment offset by N words
	and r2, r2, r5	// make offset loop after 16Mb
	subs r0, r0, #0x1	// dec counter
	bne 0b
end:
	// Poke 0 into addr 0 so that the monitor knows it's finished
	mov r0, #0x0
	str r0, [r0]
0:
	b 0b	// stay here, sucking life out of battery
	
gp2x_regs: .word 0xC0000000-0x2000000
	
