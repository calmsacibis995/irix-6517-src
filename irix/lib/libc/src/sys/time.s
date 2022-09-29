/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Id: time.s,v 1.10 1994/10/03 04:07:36 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

SYSCALL(time)
	beq	a0,zero,1f
	nop			/* SYSCALL implies noreorder, so nop needed */
	TIME_T_S v0,(a0)
1:
	RET(time)
