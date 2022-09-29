#ident	"IP30diags/pon/pon_subr.s:  $Revision: 1.2 $"

/*
 * pon_subr.s - miscelleous power-on diagnostic subroutines
 */

#include "asm.h"
#include "regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

LEAF(run_cached)		/* sp using uncached K1SEG */
	and	ra,TO_PHYS_MASK
	or	ra,K0BASE
	j	ra
	END(run_cached)

LEAF(run_uncached)		/* sp using uncached K1SEG */
	and	ra,TO_PHYS_MASK
	or	ra,K1BASE
	j	ra
	END(run_uncached)

LEAF(cache_K0)
	.set	noreorder
	mfc0	t0,C0_CONFIG
#if R10000 && !R4000
	CLI	v0,PHYS_TO_COMPATK1(HEART_STATUS)
	ld	v0,0(v0)
	and	v0,HEART_STAT_PROC_ACTIVE_MSK
	sub	v1,v0,1
	and	v0,v1
	beq	v0,zero,1f
	li	t1,CONFIG_COHRNT_EXL	# BDSLOT, uniprocessor
	li	t1,CONFIG_COHRNT_EXLWR	# multiprocessor
#else
	li	t1,CONFIG_NONCOHRNT
#endif
1:
	and	t0,~CONFIG_K0
	or	t0,t1
	mtc0	t0,C0_CONFIG
	nop;nop;nop;nop;nop;nop;nop;nop
	j	ra
	nop
	.set	reorder
	END(cache_K0)

LEAF(uncache_K0)
	.set	noreorder
	mfc0	t0,C0_CONFIG
	nop
	and	t0,~CONFIG_K0
	or	t0,CONFIG_UNCACHED
	mtc0	t0,C0_CONFIG
	nop;nop;nop;nop;nop;nop;nop;nop
	j	ra
	nop
	.set	reorder
	END(uncache_K0)

/*
 * Convert the stack pointer into a virtual address in the segment indicated
 * by base (a0).
 *
 *      void setstackseg(__psunsigned_t base)
 *
 */
LEAF(setstackseg)
	and	sp,TO_PHYS_MASK
	or	sp,a0
	j	ra
	END(setstackseg)

