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
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/fpgetsticky.s,v 1.1 1992/10/01 14:10:34 gb Exp $ */

.weakext fpgetsticky _fpgetsticky
#include "synonyms.h"

#include <regdef.h>
#include <asm.h>

/*
 * Return current fp sticky bits
 */

LEAF(fpgetsticky)
	.set	noreorder
	cfc1	v0,$31		/* Load fp_csr from FP chip */
	nop
	srl	v0,2		/* Move sticky bits to right */
	j	ra
	andi	v0,0x1f		/* Return only the sticky bits */
	.set	reorder
END(fpgetsticky)
