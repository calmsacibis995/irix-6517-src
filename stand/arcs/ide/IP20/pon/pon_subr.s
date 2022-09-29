#ident	"IP20diags/pon/pon_subr.s:  $Revision: 1.7 $"

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
