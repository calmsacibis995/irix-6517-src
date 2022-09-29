/*
 * pon_subr.s - miscelleous power-on diagnostic subroutines
 */

#ident "$Revision: 1.4 $"

#include "sys/sbd.h"
#include "regdef.h"
#include "asm.h"

LEAF(run_cached)		/* return to K0 */
	LI	v0,K0BASE
	b	2f

XLEAF(run_chandra)			/* return to ICached uncache */
	LI	v0,0x8000000000000000

2:	and	ra,0x1fffffff		# get phys addresses
	or	ra,v0
	j	ra
	END(run_cached)

/*
 * Convert the stack pointer into a virtual address in the segment indicated
 * by base (a0).
 *      void
 *      setstackseg(__psunsigned_t base)
 */
LEAF(setstackseg)
	and	sp,0x1fffffff
	or	sp,a0
	j	ra
	END(setstackseg)

