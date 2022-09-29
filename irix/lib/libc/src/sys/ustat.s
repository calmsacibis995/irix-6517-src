/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Id: ustat.s,v 1.11 1994/10/03 04:33:50 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

	.weakext	ustat, _ustat
LEAF(_ustat)
	move	t0,a0
	move	a0,a1
	move	a1,t0
	li	a2,2
	.set	noreorder
	li	v0,SYS_utssys
	syscall
	bne	a3,zero,9f
	nop
	move	v0,zero
	RET(ustat)
