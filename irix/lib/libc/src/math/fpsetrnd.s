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
#ident "$Id: fpsetrnd.s,v 1.5 1994/10/04 18:33:23 jwag Exp $"

.weakext fpsetround _fpsetround
#include "synonyms.h"

#include <regdef.h>
#include <asm.h>


LEAF(fpsetround)
	SETUP_GP
	USE_ALT_CP(t1)
	SETUP_GP64(t1,fpsetround)
	.set	noreorder
	cfc1	v0,$31		/* Load fp_csr from FP chip */
	andi	a0,0x03		/* mask the rounding mode bits */
	li	t0,0xfffffffc
	and	t0,v0		/* clear the rounding mode bits */
	or	t0,a0		/* and set the new value */
	ctc1	t0,$31		/* then put value into fp_csr */

	lb	a0,conv_rnd(a0)	/* convert rounding mode into ABI value */
	sw	a0,__flt_rounds	
	j	ra
	andi	v0,0x03		/* Return only the rounding mode bits */
	.set	reorder
	END(fpsetround)

/* Convert MIPS HW floating point rounding values to ABI values */
	.rdata
conv_rnd:
	.byte	1, 0, 2, 3

#if defined(_LIBCG0) || defined(_PIC)
	.data
#else
	.sdata
#endif
	.globl	__flt_rounds
__flt_rounds:
	.word	1
