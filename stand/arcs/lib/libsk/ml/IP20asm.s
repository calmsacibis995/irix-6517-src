#ident	"lib/libsk/ml/IP20asm.s:  $Revision: 1.4 $"

/*
 * IP20asm.s - IP20 specific assembly language functions
 */

#include	"ml.h"
#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>

/*
 * wbflush() -- spin until write buffer empty
 */
LEAF(wbflush)
XLEAF(flushbus)
	.set	noat
	li	AT,PHYS_TO_K1(CPUCTRL0)
	lw	AT,0(AT)
	.set	at
	j	ra
	END(wbflush)

/*
 * old_sr = spl(new_sr) -- set the interrupt level (really the sr)
 *	returns previous sr
 */
LEAF(spl)
	.set	noreorder
	mfc0	v0,C0_SR
	j	ra
	mtc0	a0,C0_SR
	.set	reorder
	END(spl)


/*
 *
 * writemcreg (reg, val)
 *
 * Basically this does *(volatile uint *)(PHYS_TO_K1(reg)) = val;
 *      a0 - physical register address
 *      a1 - value to write
 *
 * This is a workaround for a bug in the first rev MC chip.
 */


LEAF(writemcreg)

/*
 * Reserve a cacheline.
 */
        .set    noreorder
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop
        la      v0,1f
        or      v0,K1BASE
        j       v0              # run uncached
        nop
1:
        or      a0,K1BASE       # a0 = PHYS_TO_K1(a0)
#ifdef MIPSEB
        lw      v0,0xbfc00004   # dummy read from prom to flush mux fifo
#else
        lw      v0,0xbfc00000   # dummy read from prom to flush mux fifo
#endif
        sw      a1,(a0)         # write val in a1 to MC register *a0

        j       ra
/*
 * Reserve a cacheline.
 */
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop;
        nop; nop; nop; nop

        .set    reorder
END(writemcreg)

