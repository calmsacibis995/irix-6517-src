 #*************************************************************************
 #									  *
 # 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #*************************************************************************

#ident "$Revision: 1.13 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys/syscall.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <sys.s>


	.weakext BSDsetjmp, _BSDsetjmp

LOCALSZ=	7		# save ra, a0, a1, t0, gp, 5th arg
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(4*SZREG)
A0OFF=		FRAMESZ-(5*SZREG)
A1OFF=		FRAMESZ-(6*SZREG)
T0OFF=		FRAMESZ-(7*SZREG)

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
GPARG=		((NARGSAVE+0)*SZREG)
#endif

UCTXT_UC_OFF=	0			# FIXME: we shouldn't do this!

NESTED(_BSDsetjmp,FRAMESZ,ra)
	move	t0, gp			# save entering gp
					# SIM_ABI64 has gp callee save
					# no harm for SIM_ABI32
	SETUP_GPX(t8)
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_BSDsetjmp)
	SAVE_GP(GPOFF)

	/* a0 pts to ucontext/sigjmpbuf, a1 determines whether to save mask */
	REG_S	ra,RAOFF(sp)
	REG_S	a0,A0OFF(sp)
	REG_S	a1,A1OFF(sp)
	REG_S	t0,T0OFF(sp)

	li	t1,UC_ALL
	REG_S	t1,UCTXT_UC_OFF(a0)	/* ucp->uc_off = UC_ALL; */

	.set	noreorder
	LI	v0,SYS_getcontext
	syscall				/* getcontext(ucp) */
	.set	reorder

	REG_L	ra,RAOFF(sp)
	REG_L	a0,A0OFF(sp)
	REG_L	a1,A1OFF(sp)
	REG_L	t0,T0OFF(sp)

	li	a1, 1
	move	a2, ra
	PTR_ADDIU a3, sp, FRAMESZ
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	sw	t0, GPARG(sp)
#else
	move	a4, t0
#endif
	jal	_sigstat_save		# _sigstat_save(ucontext *, save, ra, sp, gp)
	REG_L	ra,RAOFF(sp)
	LA	t9, _cerror
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	bne	v0,zero,err
	j	ra

err:
	j	t9

	END(_BSDsetjmp)
