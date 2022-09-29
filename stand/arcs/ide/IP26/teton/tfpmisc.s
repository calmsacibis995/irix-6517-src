/* Misc low-level routines.
 */

#ident "$Revision: 1.4 $"

#include <sys/cpu.h>
#include <sys/tfp.h>
#include <sys/regdef.h>
#include <sys/asm.h>

LEAF(DoEret)
	.set	noreorder
	DMTC0	(ra,C0_EPC)
	eret
	ssnop
1:	nada
	b	1b
	nada
	.set	reorder
	END(DoEret)

/*  Set teton BEV handler register.  A non-zero address will be jumped to
 * given an exception.  This simulates the boot exception vector found on
 * the R4000 (to some extent).
 */
LEAF(set_BEV)
	.set	noreorder
	DMTC0	(a0,TETON_BEV)
	.set	reorder
	j	ra
	END(set_BEV)

/* prefetch address in shared state */
LEAF(prefetch)
	.set	noat
	pref	1, 0(a0)
	.set	at
	j	ra
	END(prefetch)

