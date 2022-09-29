
#include "ml.h"
#include <asm.h>
#include <sys/sbd.h>
#include <regdef.h>
#include "sl.h"

#define	C0_IMP_R4000	0x400
#define	C0_IMP_R8000	0x800
#define	C0_IMP_R10000	0x900

LEAF(entry)
	.set noreorder
	
	nop; nop; nop; nop
	mfc0	a0, C0_PRID
	nop; nop; nop; nop

 	andi	a0, C0_IMPMASK
 	ori	a1, zero, C0_IMP_R8000

	# Is the CPU an R4000/R10000

	beq	a0, a1, 1f
	nop

	# if it _is_ an R4000/R10000, set the KX bit.

	nop; nop; nop; nop
	nop; nop; nop; nop
	mfc0	a0, C0_SR
	nop; nop; nop; nop
	nop; nop; nop; nop

	li	a1, R4K_SR_KX | R4K_SR_BEV
	or	a0, a1

	nop; nop; nop; nop
	nop; nop; nop; nop
	nop; nop; nop; nop
	mtc0	a0, C0_SR
	nop; nop; nop; nop
	nop; nop; nop; nop
	nop; nop; nop; nop

1:
	# Set up sp
	dla	a0, _fbss	# clear bss and stack
	dla	a1, _end
	daddu	a1, SEG0STACK_SIZE
	dli	t0, 0x9fffffff
	and	v0, a0, t0
	and	v1, a1, t0
	dli	t0, K0BASE
	or	v0, t0
	or	v1, t0
	move	sp, v1
	and	sp, ~0xf	# make sure it's aligned properly

	dla	a0, 3f
	nop
	
	jr	a0
	nop
3:	nop; nop; nop; nop
	nop; nop; nop; nop

	# Get ready to jump to main.
	dla	a0, main

	# Go!
	j	a0
	nop

	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	mtc2	zero, $5
	END(entry)


