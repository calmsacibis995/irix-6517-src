/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.2 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys/prctl.h>
#include <sys.s>
#include <rld_interface.h>
#include "sys/syscall.h"


LOCALSZ=	10		# save ra, s0, s1, s2, a2, gp, a3, type, a4, s3
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
S0OFF=		FRAMESZ-(2*SZREG)
S1OFF=		FRAMESZ-(3*SZREG)
A2OFF=		FRAMESZ-(4*SZREG)
GPOFF=		FRAMESZ-(5*SZREG)
S2OFF=		FRAMESZ-(6*SZREG)
A3OFF=		FRAMESZ-(7*SZREG)
TYPEOFF=	FRAMESZ-(8*SZREG)
S3OFF=		FRAMESZ-(9*SZREG)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
/* for ABI32 bit the fifth arg must be a 16(sp) */
STKLEN=		FRAMESZ+16
A4OFF=		16
#else
	/* #if (_MIPS_SIM == _MIPS_SIM_ABI64) or N32 */
/* for 64 bit we simply need to save the arg register */
A4OFF=		FRAMESZ-(10*SZREG)
#endif

/* for myentry */
A0OFF=		FRAMESZ-(2*SZREG)

	.weakext	nsproctid, _nsproctid

NESTED(_nsproctid, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_nsproctid)
	SAVE_GP(GPOFF)
	li	t0, SYS_nsproctid
	REG_S	ra,RAOFF(sp)	 	# save the ra to return after sproc
	REG_S	s0,S0OFF(sp)	 	# save s0 
	REG_S	s1,S1OFF(sp)	 	# save s1 
	REG_S	s2,S2OFF(sp)	 	# save s2 
	REG_S	s3,S3OFF(sp)	 	# save s3 
	move	s0, a0			# s0 hold real entry point
	move	s1, a1			# s1 holds mask
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	REG_L	s3,STKLEN(sp)		# save 'stklen'
#else
	/* #if (_MIPS_SIM == _MIPS_SIM_ABI64) or N32 */
	move	s3, a4			# save 'stklen'
#endif
	REG_S	t0,TYPEOFF(sp)		# save whether sproc or sprocsp
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	REG_S	s3, A4OFF(sp)
#else
	/* #if (_MIPS_SIM == _MIPS_SIM_ABI64) or N32 */
	move	a4, s3			# restore 'stklen'
#endif
	move	a1, s1			# restore mask
	LA	a0, myentry		# move pseudo entry into a0
	move	s2, a0			# pass to child
	.set	noreorder
	REG_L	v0,TYPEOFF(sp)		# whether sproc or sprocsp
	syscall
	.set	reorder
	REG_L	s3,S3OFF(sp)
	REG_L	s2,S2OFF(sp)
	REG_L	s1,S1OFF(sp)
	move	s0, v0			# save errno/pid
	move	a1, v0			# pass errno/pid
	beq 	a3,zero,9f
	/*
	 * error case
	 * v0 = errno
	 */
	li	a0, -1
	REG_L	s0,S0OFF(sp)
	REG_L	ra,RAOFF(sp)
	LA	t9, _cerror
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	t9
9:
	REG_L	s0,S0OFF(sp)
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	ra

	/*
	 * New process starts here
	 * a0 & s* - inherited from parent (set in sproc call)
	 * We use s0 to hold real function entry point to jump to
	 *	  s1 holds mask
	 *	  s2 hold &myentry
	 *	  s3 hold stklen
	 *	  a0 holds user passed arg
	 */
myentry:
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	.set	noreorder
	.cpload	s2			# set up gp for KPIC
	.set	reorder
#endif
	PTR_SUBU sp,FRAMESZ
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
	.cpsetup	s2, GPOFF, myentry
#endif
	SAVE_GP(GPOFF)

	move	a1, s3			# set up 2nd arg - stklen
	move	t9, s0			# required by PIC calling convention
	jal	t9			# realentry(arg, stklen)
	move	a0,zero
	jal	exit			# call exit if return
	break	0			# should never return!
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ

	END(_nsproctid)
