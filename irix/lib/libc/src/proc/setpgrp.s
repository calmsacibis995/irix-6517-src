/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: setpgrp.s,v 1.10 1994/09/27 18:59:04 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

	.weakext	setpgrp, _setpgrp
	.weakext	getpgrp, _getpgrp
LEAF(_setpgrp)
	li	a0,1
	.set	noreorder
	li	v0,SYS_setpgrp
	syscall
	bne	a3,zero,9f
	nop
	RET(setpgrp)

LEAF(_getpgrp)
	li	a0,0
	.set	noreorder
	li	v0,SYS_setpgrp
	syscall
	bne	a3,zero,9f
	nop
	RET(getpgrp)
