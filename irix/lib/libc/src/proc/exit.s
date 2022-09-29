/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: exit.s,v 1.10 1996/07/10 05:22:04 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

LOCALSZ=	3		# save ra, gp, a0
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)
A0OFF=		FRAMESZ-(3*SZREG)

NESTED(_exit, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_exit)
	SAVE_GP(GPOFF)
	REG_S	ra,RAOFF(sp)	 	# save ra (debugging)
	REG_S	a0,A0OFF(sp)	 	# save a0
	jal	__tp_exit		# trace point for perf package
	REG_L	a0,A0OFF(sp)	 	# restore a0
	.set	noreorder
	li	v0,SYS_exit
	syscall
	break	0		# exit should never return
	END(_exit)
