#ident	"IP30diags/pon/pon_subr.s:  $Revision: 1.4 $"

/*
 * pon_subr.s - miscelleous power-on diagnostic subroutines
 */

#include "asm.h"
#include "regdef.h"
#include "sys/sbd.h"

/*
 * run_cached() and run_uncached() MUST NOT be called when
 * running in compatibility spaces
 */
LEAF(run_cached)
	and	ra,TO_PHYS_MASK
	or	ra,K0BASE
	j	ra
	END(run_cached)

LEAF(run_uncached)
	and	ra,TO_PHYS_MASK
	or	ra,K1BASE
	j	ra
	END(run_uncached)

/*
 * Convert the stack pointer into a virtual address in the segment indicated
 * by base (a0).  sp MUST NOT be in compatibility spaces
 *
 *      void setstackseg(__psunsigned_t base)
 *
 */
LEAF(setstackseg)
	and	sp,TO_PHYS_MASK
	or	sp,a0
	j	ra
	END(setstackseg)
