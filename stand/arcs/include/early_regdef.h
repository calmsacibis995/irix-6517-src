#ifndef __EARLY_REGDEF_H__
#define __EARLY_REGDEF_H__

#ident	"stand/include/early_regdef.h:  $Revision: 1.2 $"

/*
 * Power-on IDE diagnostic register definitions.
 *
 * Early in the game we wish to use general purpose routines without a run-time
 * stack (memory integrity is unknown).  Therefore we define abstract register
 * names for return address chaining and local variables for each level of
 * code that must run register-bound.  The lowest level is the (true) leaf,
 * pon_putc, level 0.  The structure from the top down is:
 *	4	pon_diags
 *	3		pon_addruniq, pon_walkingbit
 *	2			pon_showstat, pon_memerr
 *	1				pon_puts, pon_puthex
 *	0					pon_putc
 *
 */

#include <sys/regdef.h>

/* return address registers */
#define RA1	t9
#define RA2	t8
#if !IP26
#define RA3	k1
#define RA4	k0
#else
/* IP26 needs k0/k1 for GCache exception handling. -- RA4 handled specially */
#define RA3	s8
#endif

/* general purpose registers */
#define T00	t0	/* C-callable level */
#define T01	t1
#define T02	t2
#define T10	t3	/* C-callable level */
#define T11	ta0
#define T20	ta1	/* not C-callable */
#define T21	ta2
#define T22	ta3
#define T23	s0
#define T30	s1	/* not C-callable */
#define T31	s2
#define T32	s3
#define T33	s4
#define T34	s5
#define T40	s6	/* not C-callable */
#define T41	s7

#endif	/* __EARLY_REGDEF_H__ */
