/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: ckpt_cerror.s,v 1.2 1996/07/26 01:56:19 beck Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>

	.text

/*
 * Strpped down version for use with librestart.so.  Invert the
 * error number
 */
LEAF(_cerror)
	sub	v0,zero,v0
	j	ra
	END(_cerror)
