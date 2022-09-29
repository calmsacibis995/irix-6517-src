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

#ident	"$Revision: 1.13 $"

#include "ml/ml.h"
#include "sys/traplog.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

/*
 * End of exception processing.  Interrupts should be disabled.
 */
VECTOR(exception_exit, M_EXCEPT)
	/*
	 * ENTRY CONDITIONS:
	 *	Interrupts Disabled
	 *	s0 contains sr at time of exception
	 *	sp points to stack to use on reschedule
	 *	k1 point to eframe
	 *
	 * If we are returning to user mode, check to see if a resched is
	 * desired.  If so, fake a RESCHED cause bit and let trap save/restore
	 * our state for us.
	 */
#ifdef STKDEBUG
	.set noreorder
	MFC0(a0,C0_SR)
	NOP_1_0
	and	a0, SR_IEC
	beq	a0,zero,noint
	nop				# BDSLOT
	.set reorder
	ASMASSFAIL("exception_exit with interrupts")
noint:
#endif
#if TRAPLOG_DEBUG
	.set	noreorder
	move	a0, k1
	TRAPLOG(5)
	move	k1, a0
	.set	reorder
#endif
	and	k0,s0,SR_PREVMODE
	beq	k0,zero,exception_leave		# returning to kernel mode

#if defined(ULI) && defined(DEBUG)
	PTR_L	k0, VPDA_CURULI(zero)
	beq	k0, zero, 1f
	move	a2, k1
	PANIC("exception_exit during uli, ep 0x%x\n");
1:
#endif
	/* at this point, MUST have been on kernel stack and from USERMODE
	 * the state has been saved in u_eframe, the sp points to the
	 * kernel stack
	 */
	lb	k0,VPDA_RUNRUN(zero)
	bne	k0,zero,3f			# resched requested
	PTR_L	k0, VPDA_CURUTHREAD(zero)
	PTR_L	k0, UT_EXCEPTION(k0)
	PTR_ADDIU k0, U_PCB
	lw	k0,PCB_RESCHED(k0)		# check for softfp resched
	bne	k0,zero,1f

	j	backtouser			# resched == 0? - backtouser

1:
	/* reschedule */
3:	move	a0,k1
	li	a1,SEXC_RESCHED			# software exception
	lreg	a3,EF_CAUSE(k1)
	j	soft_trap

	/* returning to kernel mode - exception frame is ALWAYS on the
	 * current stack - though the stack could be the ICS
	 */

	/* exception_leave - common frame popping code
	 * k1 - points to exception area (may have NOTHING to do with stack)
	 * KSTACKFLAG must have already been set up
	 * Does NOT pop gp
	 */
EXPORT(exception_leave)
#if DEBUG && defined(ULI)
	sreg	t0,VPDA_T0SAVE(zero)	# another reg to use
	# if not 8 byte aligned, fail
	PTR_SRL	k0, k1, 3
	PTR_SLL	k0, 3
	bne	k0, k1, badk1

	# if we're on the idle stack, the end of the istack is a good eframe
	lb	k0, VPDA_KSTACK(zero)
	addi	k0, k0, -PDA_CURIDLSTK
	bne	k0, zero, 1f
	PTR_L	k0, VPDA_LINTSTACK(zero)
	beq	k1, k0, sanity

1:
	# if within int stack, ok
	PTR_L	k0, VPDA_INTSTACK(zero)
	blt	k1, k0, 1f
	PTR_L	k0, VPDA_LINTSTACK(zero)
	blt	k1, k0, sanity

1:
	# if within idle stack, ok
	PTR_L	k0, VPDA_BOOTSTACK(zero)
	blt	k1, k0, 1f
	PTR_L	k0, VPDA_LBOOTSTACK(zero)
	blt	k1, k0, sanity

1:
	# if on curthread kernel stack, ok
	PTR_L	k0, VPDA_CURKTHREAD(zero)
	beq	k0, zero, 2f
	PTR_L	t0, K_STACK(k0)
	blt	k1, t0, 1f
	lw	k0, K_STACKSIZE(k0)
	PTR_ADDU	k0, k0, t0
	blt	k1, k0, sanity

1:
	# if in exception area, ok
	PTR_L	k0, VPDA_CURUTHREAD(zero)
	beq     k0, zero, 2f
	PTR_L	k0, UT_EXCEPTION(k0)
	PTR_ADDIU k0, U_EFRAME		# address of exception frame
	beq	k0, k1, sanity

2:
	# not in any valid area
	lreg	t0, VPDA_T0SAVE(zero)	# restore t0
	b	badk1

sanity:
	lreg	t0,VPDA_T0SAVE(zero)	# restore t0
	# some basic sanity checking on exception area:
	# If returning to usermode, EPC and SP had better be in userspace
	lreg	k0, EF_SR(k1)
	and	k0, SR_PREVMODE
	beq	k0, zero, 1f
	lreg	k0, EF_EPC(k1)
	blez	k0, badk1
	lreg	k0, EF_SP(k1)
	blez	k0, badk1
	b	2f
1:
	# else we're returning to kernel mode, so EPC and SP had better
	# be in kernel space
	lreg	k0, EF_EPC(k1)
	bgez	k0, badk1
	lreg	k0, EF_SP(k1)
	bgez	k0, badk1

2:
	b	goodk1
	
badk1:
	# bogus k1!
	move	a2, k1
	PANIC("exception_leave: bad k1 0x%x\n");
goodk1:
#endif /* DEBUG && ULI */
#if TFP
	jal	tfi_restore			# restore t regs
	RESTOREAREGS(k1)
	RESTOREFPREGS_TOKERNEL(k1)
	.set 	noreorder
	lreg	s1,EF_CONFIG(k1)		# dmtc0 haz
	lreg	ra,EF_RA(k1)			# dmtc0 haz
	lreg	sp,EF_SP(k1)			# dmtc0 haz
	.set	noat
	lreg	AT,EF_AT(k1)
	.set	at
	dmtc0	s1,C0_CONFIG
	RESTORESREGS_SR_EPC(k1)
	eret
	.set	reorder
#else	/* !TFP */
#if R4000_FAST_SYSCALL
	.set	noreorder
	jal	tfi_restore			# restore t regs
	mfc0	s2, C0_SR		# BDSLOT load C0_SR for later
	.set	reorder
	RESTOREAREGS(k1)
	RESTOREFPREGS_TOKERNEL(k1)
	lreg	ra,EF_RA(k1)		# LDSLOT
	.set	noat
	lreg	AT,EF_AT(k1)
	AUTO_CACHE_BARRIERS_DISABLE	# $sp now is user sp
	lreg	sp,EF_SP(k1)
	/* 
	 * s2 == current C0_SR (so macro can set SR_EXL)
	 */
	.set 	noreorder
	RESTORESREGS_SR_EPC(k1, s2)
EXL_EXPORT(locore_exl_18)
	ERET_PATCH(locore_eret_9)	# patchable eret
EXL_EXPORT(elocore_exl_18)
	.set	reorder
	.set	at
#else	/* !R4000_FAST_SYSCALL */
	jal	tfi_restore			# restore t regs
	RESTOREAREGS(k1)
	RESTORESREGS(k1)
	RESTOREFPREGS_TOKERNEL(k1)
	.set 	noreorder
	lreg	k0,EF_SR(k1)		# always restore SR - CU bits etc chg
	lreg	ra,EF_RA(k1)		# LDSLOT
	NOP_0_1
	MTC0(k0,C0_SR)
EXL_EXPORT(locore_exl_18)
	NOP_0_4
	.set	noat
	lreg	k0,EF_EPC(k1)
	lreg	AT,EF_AT(k1)
	NOP_0_3
#if R4000 || R10000
	DMTC0(k0,C0_EPC)
#endif
	NOP_0_3
	AUTO_CACHE_BARRIERS_DISABLE	# $sp now is user sp
	lreg	sp,EF_SP(k1)
	NOP_0_4				# LDSLOT to set EPC before eret
	ERET_PATCH(locore_eret_9)	# patchable eret
EXL_EXPORT(elocore_exl_18)
	.set	reorder
	.set	at
#endif	/* !R4000_FAST_SYSCALL */
#endif	/* !TFP */
	AUTO_CACHE_BARRIERS_ENABLE
	END(exception_exit)
/*
 * This string is used by ASMASSFAIL().
 */
	.data
lmsg:	.asciiz "exception_exit.s"
