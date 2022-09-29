/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/fpsetmask.s,v 1.1 1992/10/01 14:10:38 gb Exp $ */
/*
 * fp_except fpsetmask(mask)
 * 	fp_except mask;
 * set exception masks as defined by user and return
 * previous setting
 * any sticky bit set whose corresponding mask is enabled
 * is cleared
 */

.weakext fpsetmask _fpsetmask
#include "synonyms.h"

#include <regdef.h>
#include <asm.h>


LEAF(fpsetmask)
	.set	noreorder
	cfc1	v0,$31		/* Load fp_csr from FP chip */
	andi	a0,0x1f		/* Use only valid mask bits */
	sll	t0,a0,7		/* t0 = new mask in proper position */

/* The following steps form a mask which can be and-ed with the current
 * fp_csr and which will clear the current exception mask as well as
 * clearing only those pending exceptions & sticky bits which are
 * being enabled.
 */	
	sll	t1,a0,2		/* t1 has new mask in sticky field */
	sll	a0,12		/* a0 has new mask in exception field */
	ori	a0,0x0f80	/* this is field for new mask */
	nor	a0,t1

/*
 * We have the correct mask.  Apply mask to current fp_csr value, then
 * "or" the new exception mask before actually setting the new fp_csr.
 */

	and	a0,v0		/* Clear necessary bits */
	or	a0,t0		/* Set the new exception mask */
	ctc1	a0,$31		/* Finally setup the fp_csr */

/*
 * Original fp_csr is still in v0.  Return previous exception mask.
 */
	
	srl	v0,7		/* Move mask bits to right */
	j	ra
	andi	v0,0x1f		/* Return only the mask bits */
	.set	reorder
END(fpsetmask)
