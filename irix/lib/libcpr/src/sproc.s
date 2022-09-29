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
#ident "$Revision: 1.1 $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys/prctl.h>
#include <sys.s>
#include <rld_interface.h>
#include "sys/syscall.h"


LOCALSZ=	11	# save ra, s0, s1, s2, a2, gp, a3, type, a4, s3, s4
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
S4OFF=		FRAMESZ-(10*SZREG)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
/* for ABI32 bit the fifth/sixth arg must be a 16(sp)/20(sp) */
STKLEN=		FRAMESZ+16
PID=		FRAMESZ+20
A4OFF=		16
A5OFF=		20
#else
	/* #if (_MIPS_SIM == _MIPS_SIM_ABI64) or N32 */
/* for 64 bit we simply need to save the arg register */
A4OFF=		FRAMESZ-(11*SZREG)
#endif

/* for myentry */
A0OFF=		FRAMESZ-(2*SZREG)

	.weakext	pidsprocsp, _pidsprocsp

NESTED(_pidsprocsp, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_pidsprocsp)
	SAVE_GP(GPOFF)
	li	t0, SYS_pidsprocsp
	REG_S	ra,RAOFF(sp)	 	# save the ra to return after sproc
	REG_S	s0,S0OFF(sp)	 	# save s0 
	REG_S	s1,S1OFF(sp)	 	# save s1 
	REG_S	s2,S2OFF(sp)	 	# save s2 
	REG_S	s3,S3OFF(sp)	 	# save s3 
	REG_S	s4,S4OFF(sp)		# save s4
	move	s0, a0			# s0 hold real entry point
	move	s1, a1			# s1 holds mask
	REG_S	a2,A2OFF(sp)		# save 'arg'
	REG_S	a3,A3OFF(sp)		# save 'stkbase'
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	REG_L	s3,STKLEN(sp)		# save 'stklen'
	REG_L	s4,PID(sp)		# save 'pid'
#else
	/* #if (_MIPS_SIM == _MIPS_SIM_ABI64) or N32 */
	move	s3, a4			# save 'stklen'
	move	s4, a5			# save 'pid'
#endif
	REG_S	t0,TYPEOFF(sp)		# save whether sproc or sprocsp
#if !defined(_LIBC_NOMP)
	li	a0, _RLD_SPROC_NOTIFY
	#LA	a1, __uscasdata		obsolete
	li	a1, 0
	jal	_rld_new_interface

	/* only start up libc thread protection if PR_NOLIBC is not set */
	li	t0, PR_NOLIBC
	and	t0, s1
	bne	t0, zero, 8f
	/*
	 * cpr should only be calling with PR_NOLIBC set
	 */
	li	v0,-1
	REG_L	s3,S3OFF(sp)
	REG_L	s2,S2OFF(sp)
	REG_L	s1,S1OFF(sp)
	REG_L	s0,S0OFF(sp)
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	ra			# NOTE : error is set in _seminit
8:
#endif
	REG_L	a2, A2OFF(sp)		# restore arg
	REG_L	a3, A3OFF(sp)		# restore stkbase
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	REG_S	s3, A4OFF(sp)
	REG_S	s4, A5OFF(sp)
#else
	/* #if (_MIPS_SIM == _MIPS_SIM_ABI64) or N32 */
	move	a4, s3			# restore 'stklen'
	move	a5, s4			# restore 'pid'
#endif
	move	a1, s1			# restore mask
	LA	a0, myentry		# move pseudo entry into a0
	move	s2, a0			# pass to child
	.set	noreorder
	REG_L	v0,TYPEOFF(sp)		# whether sproc or sprocsp
	syscall
	.set	reorder
	REG_L	s4,S4OFF(sp)
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
	jal	__sproc_parent_handle
	move	v0, s0
	REG_L	s0,S0OFF(sp)
	REG_L	ra,RAOFF(sp)
	LA	t9, _cerror
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	t9
9:
	li	a0, 0
	jal	__sproc_parent_handle
	move	v0, s0
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

	END(_pidsprocsp)
