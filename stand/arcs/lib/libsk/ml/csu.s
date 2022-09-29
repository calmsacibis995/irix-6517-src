#ident "lib/libsk/ml/csu.s: $Revision: 1.34 $"

/*
 * csu.s -- standalone library startup code
 */

#include <ml.h>
#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys/sbd.h>
#include <fault.h>
#ifdef SABLE
#include <arcs/spb.h>
#endif

	/* 
	 * - the master thread of IDE requires a HUGE stack
	 * - SKSTKSZ and EZSTKSZ are defined in arcs/include/fault.h
	 * - the values in (globals) _fault_sp and initial_stack are
	 *   copied into vid 0's PDA by libsk/lib/fault.c:setup_gpdas()
	 * - _fault_sp is declared in libsk/ml/faultasm.s
	 * - initial_stack is declared in libsk/lib/fault.c
	 */

	BSS(skstack,SKSTKSZ)

	.text

STARTFRM=	EXSTKSZ			# leave room for fault stack
NESTED(start, STARTFRM, zero)
#if _K64PROM32
	.set	noreorder
	MFC0	(v0,C0_SR)		# make sure we are in 64 bit mode
	nop
	ori	v0,SR_KX
	MTC0	(v0,C0_SR)

	LA	v0,1f			# jump out of 64-bit compat space
	jr	v0
	nop
1:	nop
	.set	reorder
#endif
	move	v0,sp
	LA	sp,skstack		# beginning stack address
#if _K64PROM32
	LI	v1,COMPAT_K1BASE	# if K64PROM32 then stack must be
	or	sp,v1			# accessible in 32-bit mode by ARCSprom
#endif
	PTR_ADDU sp,SKSTKSZ
	PTR_SUBU	sp,4*BPREG		# leave room for argsaves

	sreg	sp,_fault_sp		# small stack for fault handling
	PTR_SUBU	sp,STARTFRM		# fault stack can grow to here + 16
	sreg	zero,STARTFRM-BPREG(sp)	# keep debuggers happy
	sreg	ra,STARTFRM-(2*BPREG)(sp)
	sreg	v0,STARTFRM-(3*BPREG)(sp)	# save caller's sp
	sreg	a0,STARTFRM(sp)		# home args
	sreg	a1,STARTFRM+BPREG(sp)
	sreg	a2,STARTFRM+(2*BPREG)(sp)
	sreg	sp,initial_stack	# save sp, since debugger may trash it
#if _K64PROM32
	jal	swizzle_SPB
#endif

#if _K64PROM32
#define	initenv		k64p32_initenv
#define initargv	k64p32_initargv
#endif

	/* copy env and argv to the client's memory
	 * from the caller's memory space, resulting new
	 * values are returned on the stack
	 */
	PTR_ADDU	a0,sp,STARTFRM+(2*BPREG)	# address of envp
	jal	initenv

	PTR_ADDU	a0,sp,STARTFRM		# address of argc
	PTR_ADDU	a1,sp,STARTFRM+BPREG	# address of argv
	jal	initargv

#if SABLE
	REG_L	a0,PHYS_TO_K1(SPBADDR)	# check if SPB initialized.
	bnez	a0,1f			# skip init if SPB exists
#ifdef SR_PROMBASE
	li	a0,SR_PROMBASE
	.set	noreorder
	MTC0(a0,C0_SR)
	.set	reorder
#endif
	jal	init_env		# initialize environ (no prom).
	li	a0,1
	sw	a0,_prom		# so SPB gets initialized
1:
#endif

	jal	config_cache		# sizes cache
#if EVEREST
	jal	ev_flush_caches		# 4 io tags
#endif
	jal	flush_cache		# just to be sure

#if !_K64PROM32				/* XXX skip this for now */
	lreg	a0,STARTFRM(sp)		# reload argc, argv, environ
	lreg	a1,STARTFRM+BPREG(sp)
	lreg	a2,STARTFRM+(2*BPREG)(sp)
	LA	a3,_dbgstart		# where to debugger starts client
	jal	_check_dbg		# check for debug request
	b	_nodbgmon

_dbgstart:				# stack has moved so resave these
	lreg	sp,initial_stack
	jal	idbg_init		# set up for debugging
	lw	v0,dbg_loadstop
	beq	zero,v0,_nodbgmon
	jal	quiet_debug

_nodbgmon:
	lreg	sp,initial_stack
#endif
	jal	_init_saio		# calls all initialization routines

	lreg	a0,STARTFRM(sp)		# reload argc, argv, environ
	lreg	a1,STARTFRM+BPREG(sp)
	lreg	a2,STARTFRM+(2*BPREG)(sp)

	jal	main

	lreg	ra,STARTFRM-(2*BPREG)(sp)
	lreg	sp,STARTFRM-(3*BPREG)(sp)	# restore caller's sp
	j	ra
	END(start)
