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
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/fpsetsticky.s,v 1.1 1992/10/01 14:10:48 gb Exp $ */

.weakext fpsetsticky _fpsetsticky
#include "synonyms.h"

#include <regdef.h>
#include <asm.h>

/* fp_except fpsetsticky (fp_except sticky)
 *
 * Sets the FPC sticky bits to the values specified by the argument.
 * Returns the previous contents of the sticky bits.
 */
LEAF(fpsetsticky)
	.set	noreorder
	cfc1	v0,$31		/* Load fp_csr from FP chip */
	andi	a0,0x1f		/* mask the caller's sticky bits */
	li	t0,0xffffff83
	and	t0,v0		/* clear the sticky bits in fp_csr*/
	sll	a0,2
	or	t0,a0		/* and set the new value */
	ctc1	t0,$31		/* then put value into fp_csr */

	srl	v0,2		/* right justify old sticky bits */
	j	ra
	andi	v0,0x1f		/* return only the sticky bits */
	.set	reorder
END(fpsetsticky)
