#ident "$Id: rexecve.s,v 1.1 1996/03/16 01:51:31 jwag Exp $"

#include <sgidefs.h>
#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

LOCALSZ=	7		# save ra, gp, s0, a0, a1, a2, a3
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)
S0OFF=		FRAMESZ-(3*SZREG)
A0OFF=		FRAMESZ-(4*SZREG)
A1OFF=		FRAMESZ-(5*SZREG)
A2OFF=		FRAMESZ-(6*SZREG)
A3OFF=		FRAMESZ-(7*SZREG)
#define SZPTR	_MIPS_SZPTR/8
	.weakext	rexecve, _rexecve

/*
 * rexecve(cell, char *file, char **argv, char **arge)
 */
NESTED(_rexecve, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_rexecve)
	SAVE_GP(GPOFF)

	REG_S	ra,RAOFF(sp)
	REG_S	a0,A0OFF(sp)
	REG_S	a1,A1OFF(sp)
	REG_S	a2,A2OFF(sp)
	REG_S	a3,A3OFF(sp)
	move	a0, a1		# don't pass cell id
	move	a1, a2
	move	a2, a3
	jal	__tp_execve
	beq	v0, zero, 1f
	PTR_L	a1, 0(v0)
	PTR_L	a2, (1*SZPTR)(v0)
	PTR_L	a3, (2*SZPTR)(v0)
	b	2f
1:
	REG_L	a1,A1OFF(sp)
	REG_L	a2,A2OFF(sp)
	REG_L	a3,A3OFF(sp)
2:
	REG_L	a0,A0OFF(sp)
	.set	noreorder
	li	v0, SYS_rexec
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
	END(_rexecve)
