/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  fork.s 1.1 */
#ident "$Id: fork.s,v 1.28 1997/10/12 08:32:04 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"
#include <mplib.h>
#include <rld_interface.h>

LOCALSZ=	3		# save ra, gp, s0
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)
S0OFF=		FRAMESZ-(3*SZREG)

#if !defined(_LIBC_NOMP)
#ifdef _PIC
        .sdata
run_only_once:
        .byte 0
        .text
#endif
#endif

#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
/*
 * BB3.0 wants vfork - but we can give to them ONLY in the link-time
 * libc - not the runtime since some PD configure programs look at symbols
 * in libc to determine if vfork (and they want BSD vfork) is available.
 * We do offer _vfork. In unistd.h we define vfork to be _vfork  in XPG &
 * ABI mode - this will permit things compiled on SGI to run on other
 * ABI platforms.
 */
	.weakext	fork, _fork
	.weakext	__vfork, _fork
	.weakext	_vfork, _fork

NESTED(_fork, FRAMESZ, ra)
#else

NESTED(__fork, FRAMESZ, ra)
#endif
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
	SETUP_GP64(GPOFF,_fork)
#else
	SETUP_GP64(GPOFF,__fork)
#endif
	SAVE_GP(GPOFF)
	REG_S	ra,RAOFF(sp)	 	# save the ra
	jal	__fork_pre_handle	# registered trace point 
#if !defined(_LIBC_NOMP)
#ifdef _PIC
	/*
	 * Do a dummy OP to _rld_new_interface. Loads GOT in the 
	 * parent and avoids a copy-on-write fault in the child.
	 * We use the run_only_once variable to determine if the call
	 * has been made once before.
	 */
	LA      a0,run_only_once
	lb      a0,0(a0)
	bne     zero,a0,1f
	li      a1,1
	sb	a1, run_only_once       # turn off 1-time switch
	li	a0, _RLD_NOP
	jal	_rld_new_interface
1f:
#endif
#endif
	.set	noreorder
	li	v0, SYS_fork
	syscall
	.set	reorder
	REG_S	s0, S0OFF(sp)	 	# save s0
	bne	a3, zero, err
	bne	v1, zero, child
	/* parent */
	move	s0, v0			# save v0 == pid
	move	a1, v0
	li	a0, 0
	jal	__fork_parent_handle
	move	v0, s0			# restore pid
	REG_L	s0,S0OFF(sp)	 	# save s0
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	ra

child:
#if !defined(_LIBC_NOMP)
#ifdef _PIC
	/*
	 * inform rld that we are no longer part of a share group.
	 * This does:
	 * 1) improve performance since rld doesn't have to do locking
	 * 2) resets any locks that might have been held at the time of
	 *	the fork - another thread might have been in dlopen/close
	 *	and therefore have the lock that prevents any LTRs
	 */
	li	a0, _RLD_SPROC_FINI
	jal	_rld_new_interface
#endif
	/* do fork processing for children of multi-threaded apps */
	INT_L	t0, __multi_thread
	li	t1, MT_SPROC
	bne	t0,t1,7f
	jal	_semadd
7:
	/* there is no aio in nomp versions of the library */
	PTR_L	t0, _aioinfo
	beq	t0,zero,8f
	jal	__aio_reset		# free _aioinfo structure
	
8:	
#endif /* _LIBC_NOMP */
	jal	__fork_child_handle		# trace point for perf tools
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	move	v0,zero
	j	ra

err:
	li	a0, -1
	move	s0, v0			# save v0 == errno
	move	a1, v0			# pass errno
	jal	__fork_parent_handle
	move	v0, s0
	REG_L	s0,S0OFF(sp)
	REG_L	ra,RAOFF(sp)
	LA	t9, _cerror
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	t9
#if defined(_LIBC_ABI) || defined(_LIBC_NOMP)
	END(_fork)
#else
	END(__fork)
#endif
