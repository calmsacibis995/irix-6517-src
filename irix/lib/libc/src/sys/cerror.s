/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: cerror.s,v 1.20 1994/10/03 03:58:25 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>

	.globl	errno
#if defined(_PIC)
	.data
errno:	.word	0
#else
	.comm	errno,4
#endif
	.text

/*
 * Always store in global errno
 * If program has initialized a per-thread errno, set it there also
 * We assume that we can be called (jumped to) from outside libc
 */
LEAF(_cerror)
	SETUP_GP
	USE_ALT_CP(t2)
	SETUP_GP64(t2, _cerror)
	sw	v0, errno

#ifndef _LIBC_ABI
	PTR_L	t0, __errnoaddr
	LA	t1, errno
	beq	t0, t1, 1f
	sw	v0, 0(t0)
#endif /* _LIBC_ABI */
1:
	li	v0,-1
	j	ra
	END(_cerror)
