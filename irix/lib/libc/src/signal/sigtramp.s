/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  sigtramp.s 1.2 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include <sys/signal.h>
#include <sys/errno.h>
#include "sys/syscall.h"
		
/*
 * Sigtramp is called by the kernel as:
 * 	sigtramp(signal, code, sigcontext_ptr, sighandler)
 * OR
 * 	sigtramp(signal, siginfo_ptr, ucontext_ptr, sighandler)
 * depends on whether the user has requested SA_SIGINFO for this signal.
 * On entry to sigtramp the sp is equal to the u/sigcontext_ptr.
 * Sigtramp should build a frame appropriate to the language calling
 * conventions and then call the sighandler.  When the sighandler
 * returns, sigtramp does a sigreturn system call passing 
 * a flag that if set indicates that the 2nd argument points to the address of 
 * a sigcontext struct or a ucontext struct otherwise.
 * When the kernel calls sigtramp it sets the top bit of 'signal' if 
 * the 3rd argument points to a ucontext. sigtramp will remember that 
 * and passes it back to the kernel when it makes the sigreturn call.
 * To maintain binary compatibility, for the ucontext case, the 1st argument
 * will be 0 and the 2nd argument will point to a ucontext.
 */
LOCALSZ=	6		# save ctxt ptr, which type, errno
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
CTXTPTROFF=	FRAMESZ-(1*SZREG)
WHICHOFF=	FRAMESZ-(2*SZREG)
SIGOFF=		FRAMESZ-(3*SZREG)	
ERROFF=		FRAMESZ-(4*SZREG)
ERRADDROFF=	FRAMESZ-(5*SZREG)	
GPOFF=		FRAMESZ-(6*SZREG)
			
NESTED(_sigtramp, FRAMESZ, ra)
	PTR_SUBU sp,FRAMESZ
	REG_S	a0,SIGOFF(sp)		# save signal #
	REG_S	a2,CTXTPTROFF(sp)	# save address of u/sigcontext
	lui	t0,0x8000		# XXX use macro
	and	t0,a0,t0
	beq	t0,zero,1f		# is a2 a sigcontext?
	REG_S	zero,WHICHOFF(sp)	# yes
	not	t0
	and	a0,t0			# mask off the top bit to signal
	b	2f
1:	/* sigcontext */
	REG_S	a2,WHICHOFF(sp)
2:
	/* setup gp to access errno */
	SETUP_GPX(t8)
	USE_ALT_CP(t0)
	SETUP_GPX64(GPOFF,t8)

	/* retrieve errno */
	PTR_L	t0,__errnoaddr
	lw	t1,0(t0)
	REG_S	t1,ERROFF(sp)		# save errno
	REG_S	t0,ERRADDROFF(sp)	# save address of errno

	/* call handler */	
	move	t9, a3			# required by PIC calling convention
	jal	t9

	/* restore errno */
	REG_L	t1,ERROFF(sp)
	REG_L	t0,ERRADDROFF(sp)
	sw	t1,0(t0)

	REG_L	a0,WHICHOFF(sp)		# sigreturn(0, &ucontext) OR
	REG_L	a1,CTXTPTROFF(sp)	# sigreturn(&sigcontext, &sigcontext)
	REG_L	a2,SIGOFF(sp)		# arg3 is signal #
	.set	noreorder
	li	v0,SYS_sigreturn
	syscall
	.set	reorder
	END(_sigtramp)
