/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Id: stime.s,v 1.11 1994/10/03 04:06:43 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

	.weakext	stime, _stime
/*
 * stime(time_t *) which is a long
 */
LEAF(_stime)
	TIME_T_L a0,(a0)		/* Go indirect off of *time */
	.set	noreorder
	li	v0,SYS_stime
	syscall
	bne	a3,zero,9f
	nop
	move	v0,zero
	RET(stime)
