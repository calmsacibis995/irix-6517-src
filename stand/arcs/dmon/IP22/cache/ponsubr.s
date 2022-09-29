#ident	"IP22diags/pon/pon_subr.s:  $Revision: 1.1 $"

/*
 * pon_subr.s - miscelleous power-on diagnostic subroutines
 */

#include "asm.h"
#include "regdef.h"
#include "sys/sbd.h"

LEAF(run_cached)		/* sp using uncached K1SEG */
	and	ra,0x1fffffff
	or	ra,K0BASE
	j	ra
	END(run_cached)

LEAF(run_uncached)		/* sp using uncached K1SEG */
	and	ra,0x1fffffff
	or	ra,K1BASE
	j	ra
	END(run_uncached)

LEAF(cache_K0)
	.set	noreorder
	mfc0	t0,C0_CONFIG
	nop
	and	t0,~CONFIG_K0
	or	t0,CONFIG_NONCOHRNT
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
