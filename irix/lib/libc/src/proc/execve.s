/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: execve.s,v 1.7 1995/09/13 06:50:19 ack Exp $"

#include <sgidefs.h>
#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

LOCALSZ=	6		# save ra, gp, s0, a0, a1, a2
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)
S0OFF=		FRAMESZ-(3*SZREG)
A0OFF=		FRAMESZ-(4*SZREG)
A1OFF=		FRAMESZ-(5*SZREG)
A2OFF=		FRAMESZ-(6*SZREG)
#define SZPTR	_MIPS_SZPTR/8
	.weakext	execve, _execve

/*
 * execve(char *file, char **argv, char **arge)
 */
NESTED(_execve, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_execve)
	SAVE_GP(GPOFF)

	REG_S	ra,RAOFF(sp)
	REG_S	a0,A0OFF(sp)
	REG_S	a1,A1OFF(sp)
	REG_S	a2,A2OFF(sp)
	jal	__tp_execve
	beq	v0, zero, 1f
	PTR_L	a0, 0(v0)
	PTR_L	a1, (1*SZPTR)(v0)
	PTR_L	a2, (2*SZPTR)(v0)
	b	2f
1:
	REG_L	a0,A0OFF(sp)
	REG_L	a1,A1OFF(sp)
	REG_L	a2,A2OFF(sp)
2:
	.set	noreorder
	li	v0, SYS_execve
	syscall
	.set	reorder
	bne     a3,zero,9f
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	ra
9:
	REG_S	s0,S0OFF(sp)
	move	s0, v0			# save v0 == errno
	move	a0, v0			# pass errno
	jal	__tp_execve_error
	move	v0, s0
	REG_L	s0,S0OFF(sp)
	REG_L	ra,RAOFF(sp)
	LA	t9, _cerror
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	t9
	END(_execve)
