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
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/fpgetrnd.s,v 1.1 1992/10/01 14:10:29 gb Exp $ */

.weakext fpgetround _fpgetround
#include "synonyms.h"

#include <regdef.h>
#include <asm.h>

/*
 * Return current fp exception mask
 */

LEAF(fpgetround)
	.set	noreorder
	cfc1	v0,$31		/* Load fp_csr from FP chip */
	j	ra
	andi	v0,0x03		/* Return only the rounding mode bits */
	.set	reorder
END(fpgetround)

