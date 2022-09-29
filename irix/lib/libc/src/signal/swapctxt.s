/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident "$Revision: 1.6 $"

.weakext swapcontext _swapcontext

#include <regdef.h>
#include <asm.h>
#include <sys.s>
#include "sys/syscall.h"

	PICOPT

#if (_MIPS_SZLONG == 32)
MCTXTOFF=	40	# ucontext offset to uc_mcontext is really 
			#  (4+4+16+12 = 36, but is not doubleword 
			#  aligned, thus is 40)
#else /* (_MIPS_SZLONG == 64) */
MCTXTOFF=	48	# ucontext off set to uc_mcontext is
			#  (8+8+16+12 = 48)
#endif

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
CTXV0OFF=	8	# v0 offset for uc_mcontext.gregs[CTX_V0]
CTXGPOFF=	112	# gp offset for uc_mcontext.gregs[CTX_GP]
CTXSPOFF=	116	# sp offset for uc_mcontext.gregs[CTX_SP]
CTXRAOFF=	124	# ra offset for uc_mcontext.gregs[CTX_RA]
CTXEPCOFF=	140	# epc offset for uc_mcontext.gregs[CTX_EPC]
#define	FRAMSIZE	8
#define	OUCPOFF		0
#define	NUCPOFF		4

#else	/* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_ABIN32) */
CTXV0OFF=	16	# v0 offset for uc_mcontext.gregs[CTX_V0]
CTXGPOFF=	224	# gp offset for uc_mcontext.gregs[CTX_GP]
CTXSPOFF=	232	# sp offset for uc_mcontext.gregs[CTX_SP]
CTXRAOFF=	248	# ra offset for uc_mcontext.gregs[CTX_RA]
CTXEPCOFF=	280	# epc offset for uc_mcontext.gregs[CTX_EPC]
#define	FRAMSIZE	16
#define	OUCPOFF		0
#define	NUCPOFF		8
#endif

NESTED(_swapcontext, FRAMSIZE, ra)
	PTR_SUBU sp,FRAMSIZE
	REG_S	a0,OUCPOFF(sp)
	REG_S	a1,NUCPOFF(sp)

	.set	noreorder
	LI	v0,SYS_getcontext
	syscall				/* getcontext(oucp) */
	.set	reorder
	REG_L	t0,OUCPOFF(sp)		/* t0 = oucp */
	REG_L	a0,NUCPOFF(sp)		/* a0 = nucp (for setcontext call) */
	PTR_ADDU sp,FRAMSIZE

	/* At this point we've saved the current context, except that we
	 * need to fixup some of the register (SP & EPC) for the subsequent
	 * restart (through another swapcontext() or setcontext()).
	 * We must remove the stack frame since the setcontext() call
	 * will NOT return here if successful.  We use the following END
	 * & LEAF directives to keep stack backtrace operational.
	 */

	END(_swapcontext)		/* these labels keep stack trace OK */
LEAF(_swapcontext2)

	bne	a3, zero,9f		/* test return from getcontext() */

	PTR_ADDIU t0,t0,MCTXTOFF	# Point to the uc_mcontext of oucp.
	REG_S 	sp,CTXSPOFF(t0)		# Store reg[CTX_SP] with current $sp
	REG_S	ra,CTXEPCOFF(t0)	# Store reg[CTX_EPC] with current $ra
	REG_S	zero,CTXV0OFF(t0)	# Store reg[CTX_V0] with 0 - clear return value
	
	.set	noreorder
	LI	v0,SYS_setcontext
	syscall				/* setcontext(nucp) */
	bne	a3, zero,9f
	nop
	RET(swapcontext2)		/* defines 9f, etc */
