/*
 * Copyright 1994 by Silicon Grapics Incorporated
 */

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: cerror64.s,v 1.4 1997/01/27 07:24:41 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>

	.globl	errno

/*
 * Always store in global errno
 * If program has initialized a per-thread errno, set it there also
 * We assume that we can be called (jumped to) from outside libc
 */
LEAF(_cerror64)
	SETUP_GP
	USE_ALT_CP(t2)
	SETUP_GP64(t2, _cerror64)
	sw	v0, errno

#ifndef _LIBC_ABI
	PTR_L	t0, __errnoaddr
	LA	t1, errno
	beq	t0, t1, 1f
	sw	v0, 0(t0)
#endif
1:
	li	v0,-1
	li	v1,-1
	j	ra
	END(_cerror64)
