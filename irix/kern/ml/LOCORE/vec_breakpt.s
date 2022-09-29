/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.5 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

/*
 * Breakpoint -- determine if breakpoint is for prom monitor, else
 * call trap.
 *	s0 -- SR register at time of exception
 *	a0 -- exception frame pointer
 *	k1 -- exception frame pointer
 *	a1 -- software exception code
 * BEWARE - k1, a1, a0, s0 are implicit when calling VEC_trap
 *	we don't use k0 so can single step!
 *	t1 - scratch
 *	t2 - scratch (EPC and faulting instr)
 */
VECTOR(VEC_breakpoint, M_EXCEPT)
EXL_EXPORT(locore_exl_20)
	j	VEC_trap
EXL_EXPORT(elocore_exl_20)

EXPORT(kernelbp)
	break	BRK_KERNELBP
	END(VEC_breakpoint)

	.globl kbdstate
	.globl kbdcntrlstate

#define DEBUGFRM	FRAMESZ(1*SZREG)	/* save ra */
#define RA_OFFSET	DEBUGFRM-(1*SZREG)
NESTED(debug, DEBUGFRM, zero)

	# Execute break point only if kdebug is set.
	LA 	t0, kdebug
	lh	t0, 0(t0)
	ble	t0, zero, 1f
	
	PTR_SUBU sp,DEBUGFRM
	REG_S	ra,RA_OFFSET(sp)
#ifdef _EXCEPTION_TIMER_CHECK
	la	ra,exception_timer_bypass
	sw	zero,0(ra)
	REG_L	ra,RA_OFFSET(sp)
#endif	/* _EXCEPTION_TIMER_CHECK */
	break	BRK_KERNELBP
	REG_L	ra,RA_OFFSET(sp)
	PTR_ADDU sp,DEBUGFRM
	sh	zero,kbdstate		# reinitialize these since we really
	sb	zero,kbdcntrlstate	# don't know where we are after return
1:
	j	ra
	END(debug)
#undef RA_OFFSET

LEAF(sstepbp)
	break	BRK_SSTEPBP
	END(sstepbp)

