#ident	"lib/libsk/ml/IP22asm.s:  $Revision: 1.13 $"

/*
 * IP22asm.s - IP22 specific assembly language functions
 */

#include "ml.h"
#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>

/*
 * wbflush() -- spin until write buffer empty
 */
LEAF(wbflush)
XLEAF(flushbus)
	lw      zero,PHYS_TO_COMPATK1(CPUCTRL0)
	j	ra
	END(wbflush)

/*
 * old_sr = spl(new_sr) -- set the interrupt level (really the sr)
 *	returns previous sr
 */
LEAF(spl)
	.set	noreorder
	mfc0	v0,C0_SR
	li	t0,~SR_IMASK
	and	t0,v0,t0
	or	t0,t0,a0
	mtc0	t0,C0_SR
	.set	reorder
	j	ra
	END(spl)


/*
 *
 * writemcreg (reg, val)
 *
 * Basically this does *(volatile uint *)(PHYS_TO_K1(reg)) = val;
 *      a0 - physical register address
 *      a1 - value to write
 *
 *  This was a workaround for a bug in the first rev MC chip, which is not
 * in fullhouse, but we make still need a routine by this name.
 */

LEAF(writemcreg)
        or      a0,K1BASE       # a0 = PHYS_TO_K1(a0)
        sw      a1,(a0)         # write val in a1 to MC register *a0
        j       ra
END(writemcreg)
