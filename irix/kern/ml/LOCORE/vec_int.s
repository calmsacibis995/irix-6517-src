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

#ident	"$Revision: 1.31 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

/*
 * VEC_int -- interrupt handler
 * Entry:
 *	k1 - exception pointer
 *	a0 - exception pointer
 *	a1 - exception code
 *	a3 - cause register
 *	s0 - sr at time of exception
 *	s7 - old timer mode
 */
VECTOR(VEC_int, M_EXCEPT)
EXL_EXPORT(locore_exl_1)
	/* noreorder since interrupts MUST be off before touching k0 */
#if TFP
	/* On TFP, we don't get an exception code for DBE/IBE, VCE, or
	 * FPE.  Instead we get an "interrupt" exception code with additional
	 * bits sets in the CAUSE register.
	 */
	li	s1,CAUSE_BE|CAUSE_VCI|CAUSE_FPI
	and	s1,a3
	beq	s1,zero, 1f
	j	tfp_special_int
1:
#endif	/* TFP */


#if EVEREST
	/* 
	 * The intr routine will handle C0_SR and CEL as appropriate, 
	 * and it will not return until all interrupts at or above the
	 * level of this interrupt are handled.
	 */
	lbu	s1,VPDA_CELSHADOW(zero)		# save old CEL value
	sd	s1,EF_CEL(a0)		# in eframe (upper half==0)
#endif /* EVEREST */
#if IP32
	lw	s4,VPDA_CURSPL(zero)	# save the current spl level
	dsll	s4,32
	li	s1,CRM_INTMASK|K1BASE	# save the current imask and spl val
	ld	k0,0(s1)		# in the eframe
	or	k0,s4
	sd	k0,EF_CRMMSK(k1)
	li	k0,3<<3			# set the current spl level to 7
	sw	k0,VPDA_CURSPL(zero)
	ld	k0,VPDA_SPLMASKS(k0)	# load the appropriate CRIME 
	sd	k0,0(s1)		# interrupt masks
	dsrl	s4,32
#endif
					# for the r4000, this clears EXL,
					# and sets the base mode to kernel.
#if R4000 || R10000
	lui	k0,(SR_DE >> 16)	# propagate the SR_DE bit
	ori	k0, SR_ERL
	and	k0,k0,s0
	or	k0,SR_IEC|SR_IMASK8|SR_KERN_SET
#endif	/* R4000 || R10000 */	
#if TFP
	and	a2,s0,SR_KERN_USRKEEP
	LI	k0,SR_IEC|SR_KERN_SET	# enable, but mask all interrupts
	or	k0,a2
#endif
EXL_EXPORT(elocore_exl_1)
	.set	noreorder
	MTC0(k0,C0_SR)
	move	a2,s0			# sr is arg3
	jal	intr			# intr(ef_ptr, code, sr, cause)
	move	s1,a0			# BDSLOT save ep
	move	s3,v0			# result of intr() - check_kp
#if IP32
	sw	s4,VPDA_CURSPL(zero)	# restore previous spl level
	ld	s2,VPDA_SPLMASKS(s4)	# fetch splmask for this spl
	la	k0,CRM_INTMASK|K1BASE
	sd	s2,0(k0)		# reload crime interrupt mask register
#endif
#if TFP
	and	s2,s0,SR_IMASK|SR_KERN_USRKEEP # set kernel base mode, low EXL
	or	s2,SR_KERN_SET
	MTC0( s2, C0_SR)		# and disabled interrupts
#endif

#if R4000 || R10000
	and	s2,s0,SR_IMASK|SR_KERN_SET|SR_DEFAULT # set kernel base mode, low EXL
	MTC0( s2, C0_SR)		# and disabled interrupts
	NOP_0_4
#endif
#if EVEREST
	ld	a0,EF_CEL(s1)		# restore saved value of CEL
#if TFP
	DMFC0(k1,C0_CEL)
#else
	LI	k1,EV_CEL
#endif
	FORCE_NEW_CEL(a0,k1)
#endif /* EVEREST */
	move	k1,s1			# restore ep
	.set	reorder

#ifdef STKDEBUG
	/* make sure interrupts disabled */
	.set noreorder
	MFC0(a0,C0_SR)
	NOP_1_0
	.set reorder
	and	a0,SR_IEC
	beq	a0,zero,noint2
	.set noreorder
	MFC0(a2,C0_SR)			#NOT in BDSLOT
	.set reorder
	move	a3,s0
	PANIC("interrupt exit with interrupts:cur SR:%x old SR:%x")
noint2:
#endif
EXPORT(tfp_special_int_exit)
	/*
	 * if we're nesting interrupts - just pop ICS and return
	 * else go through whole stack switch ordeal
	 */
	PTR_L	k0, VPDA_LINTSTACK(zero)
	/* if nested interrupts dont change KSTACK & gp */
#ifdef ULI
	bgeu	sp, k0, 1f #not nested intr

	# if real nested intr (returning to intr stack)
	lreg	s0, EF_SP(k1)
	ble	s0, k0, return_from_int

	PTR_L	t0, VPDA_CURULI(zero)
#if DEBUG
	bne	t0, zero, 3f
	PANIC("nested intr: CURULI not set")
3:
#endif

#if 0
	# This doesn't work when the ULI runs on the istack and the
	# clock interrupt runs on an ithread. I'll leave the code
	# here in case this is fixed some day.

	# check if the ULI handler has been running for more than
	# a second. If so, it must be caught in an infinite loop,
	# so let's abort it
	lw	t1, lbolt
	lw	t2, ULI_TSTMP(t0)
	dsubu	t1, t2
	li	t3, HZ
	dsubu	t1, t3
	bltz	t1, 4f

	# copy over current eframe to uli_eframe so the core
	# file will reflect what was happening when the SIGXCPU
	# was posted
	lw	a0, ULI_EFRAME_VALID(t0)
	bne	a0, zero, 5f

	move	a0, k1
	PTR_ADD a1, t0, ULI_EFRAME
	li	a2, EF_SIZE
	sw	a2, ULI_EFRAME_VALID(t0)
	jal	bcopy

5:
	# abort the ULI process with a SIGXCPU
	li	a0, SIGXCPU
	j	uli_return
4:
#endif

	# restore ULI handler's wired entries
#ifdef DEBUG
	bgtz	s0, 2f
	move	a2, k1
	PANIC("bad nested int return, ep 0x%x\n");
2:	
	INCREMENT(nested_return_to_uli, a0, a1)
#endif
	PTR_L	a1, ULI_UTHREAD(t0)
	PTR_S	a1, VPDA_CURUTHREAD(zero)

	lh	a0, ULI_NEW_ASIDS(t0)
	jal	uli_setasids

	li	a1, PDA_CURULISTK
	sb	a1,VPDA_KSTACK(zero)

#ifdef R4000
	# preset EXL to avoid EXL/KSU hazard
	ori	s2, SR_EXL
	.set	noreorder
	mtc0	s2, C0_SR
	.set	reorder
#endif

	lreg	k0,EF_EPC(k1)
	jal	tfi_restore
	.set	noreorder
	dmtc0	k0, C0_EPC
	.set	reorder
	RESTOREAREGS(k1)
#ifdef TFP
	.set	noreorder
	lreg	s0, EF_CONFIG(k1)
	dmtc0	s0, C0_CONFIG
	.set	reorder
#endif
	RESTORESREGS(k1)
	.set	noreorder
	lreg	k0,EF_SR(k1)
	.set	noat
	lreg	AT,EF_AT(k1)		# LDSLOT
#ifdef TFP
	dmtc0	k0,C0_SR
#else
	mtc0	k0,C0_SR
#endif
	lreg	sp,EF_SP(k1)
	lreg	gp,EF_GP(k1)
	lreg	ra,EF_RA(k1)
	eret
	.set	reorder
	.set	at
1:
#else	/* ULI */
	bltu	sp, k0, return_from_int	# nested
#endif	/* ULI */
	bne	sp, k0, bad_istack	# at end

/* offintstack:  */
	/*
	 * MCCHIP systems only --
	 *
	 * Restore ``run-mode'' bus burst and delay values.
	 */
#ifdef MCCHIP
#ifdef IP20			/* rev A MC only on IP20 */
	lw	a0,mc_rev_level
	beq	a0,zero,1f
#endif	/* IP20 */
	lhu	a0,dma_burst		/* this is smaller in 64-bit mode */
	lhu	a1,dma_delay
	CLI	k0,PHYS_TO_COMPATK1(CPUCTRL0)	/* MC base */
	sw	a0,LB_TIME-CPUCTRL0(k0)
	sw	a1,CPU_TIME-CPUCTRL0(k0)
	lw	zero,0(k0)		# wbflushm sans nops
1:
#endif
	/*
	 * If we are returning to user mode, check to see if a resched is
	 * desired.  If so, fake a RESCHED cause bit and let trap save/restore
	 * our state for us.
	 */
	and	a0,s0,SR_PREVMODE
	beq	a0,zero,4f			# returning to kernel mode
	lb	a0,VPDA_RUNRUN(zero)
	bne	a0,zero,3f			# resched requested
	PTR_L	a0, VPDA_CURUTHREAD(zero)
	PTR_L	a0, UT_EXCEPTION(a0)
	PTR_ADDIU a0, U_PCB
	lw	a0,PCB_RESCHED(a0)		# check for softfp resched
	bne	a0, zero, 1f

	j	backtouser		# backtouser assumes s1 == eframe
1:

	/* rescheduling */
3:
	li	k0,PDA_CURKERSTK		# mark as kernel stack
	sb	k0,VPDA_KSTACK(zero)
	LI	sp,KERNELSTACK-ARGSAVEFRM

	/* switch to system CPU timer */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	LI	a1,AS_SYS_RUN
	jal	ktimer_switch

	lreg	s0,EF_SR(k1)			# get SR
	move	a0,k1				# exception frame pointer
	li	a1,SEXC_RESCHED			# software exception
	lreg	a3,EF_CAUSE(k1)
	j	soft_trap
	/* NOTREACHED */

4:
	/*
	 * returning to kernel mode -
	 * either returning to  kernel stack or IDLE stack
	 * If p_idlstkdepth is non-zero then returning to idle
	 * else kernels eframe is in k_depth
	 */
	/*
	 * Switch back to timer we interrupted.  See comments in <sys/timers.h>
	 * and ecommon_allsaved() describing this hackery.
	 */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	move	a1,s7
	beq	a0,zero,1f
	jal	ktimer_switch
1:
#if defined(ULI) && defined(DEBUG)
	# can't be returning to kern if there's a ULI running
	PTR_L	k0, VPDA_CURULI(zero)
	beq	k0, zero, 1f
	move	a2, k1
	PANIC("inttokern during ULI, ep 0x%x\n");
1:
#endif
	lb	k0, VPDA_IDLSTKDEPTH(zero)
	beq	k0, zero, inttokernel
	/* back to idle stack - eframe is in p_idlstkdepth */
	sb	zero, VPDA_IDLSTKDEPTH(zero)
	li	k0, PDA_CURIDLSTK		# mark as idle stack
	sb	k0, VPDA_KSTACK(zero)
#ifdef IDLESTACK_FASTINT
	lb	k0, VPDA_INTR_RESUMEIDLE(zero)
	beq	k0, zero, return_from_int
	PTR_L	sp, VPDA_LBOOTSTACK(zero)
#ifdef EVEREST
	/*
	 * splhi - start code which knows how to set splhi given that we "know"
	 * we're at spl0() since we were on the idle stack
	 */
	.set noreorder
	li	t2,EVINTR_LEVEL_HIGH+1
	or	t1,s0,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK|SR_EXL|SR_IE
	.set	reorder
	xor	t1,SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK|SR_EXL

#if TFP
	.set	noreorder
	DMFC0(k1,C0_CEL)
	.set	reorder
#else
	LI	k1,EV_CEL
#endif
	FORCE_NEW_CEL(t2,k1)
	.set noreorder
	MTC0_NO_HAZ(t1,C0_SR)
	/* END - splhi() */
	.set reorder
#else /* !EVEREST */
	jal	splhi
	.set noreorder
	/* need to turn back on IE and make sure EXL is clear */
	MFC0(t1,C0_SR)
	or	t1, SR_EXL|SR_IE
	xori	t1, SR_EXL
	MTC0(t1,C0_SR)
	.set reorder
#endif /* !EVEREST */		
	sb	zero,VPDA_INTR_RESUMEIDLE(zero)
	/*
	 * Now return to idle stack via resumeidle()
	 */
	move	a0,zero
	j	resumeidle
	
#else
	b	return_from_int
#endif	/* !IDLESTACK_FASTINT */	
inttokernel:
	PTR_L	k0,VPDA_CURKTHREAD(zero)
	PTR_S	zero,K_EFRAME(k0)
	li	k0, PDA_CURKERSTK		# mark as kernel stack
	sb	k0, VPDA_KSTACK(zero)
	/* FALL THROUGH */

kernelpreemption:
#ifndef _MP_NETLOCKS
	# can't do kernel preemption unless MP_NETLOCKS are turned on
	b	return_from_int
#endif
	# see if interrupt state is OK (i.e. intr() returned TRUE)
	# return value from intr() is in s3
#ifdef SPLDEBUG
	lreg	a0,EF_SR(k1)
	jal	spldebug_log_event
#endif /* SPLDEBUG */	
	beq	s3,zero,return_from_int

#ifdef DEBUG
	lb	v0,VPDA_SWITCHING(zero)
	beq	v0,zero,1f
	move	a2, k1
	move	a3, s3
	PANIC("got preemption interrupt while p_switching == 1, ep 0x%x, s3 %d\n");
1:
#endif

	# resched requested?
	lb	v0,VPDA_RUNRUN(zero)
	beq	v0,zero,return_from_int

#if DEBUG
	# log the context switch just for debugging
	PTR_L	v0,VPDA_CURUTHREAD(zero)
	PTR_S	v0,VPDA_SWITCHUTHREAD(zero)
#endif

	/* We can't enter qswtch with IE disabled since it's not good to
	 * execute potentially long blocks of code with error interrupts
	 * off (EVEREST, SN0, etc).  On larger MP configs we can spin
	 * in qswtch and need to be able to process hi-priority interrupts.
	 * So we restore the spl level which was in affect when this
	 * interrupt occured.
	 * NOTE: The only way to get here is if we interrupts a kernel
	 * process so all of the other SR bits should be OK for kernel
	 * operation.
	 */
	move	sp,s1			# back to KSTACK (below eframe)
#ifdef SPLMETER
	jal	splhi_nosplmeter
#else
	jal	splhi
#endif
	.set noreorder
	/* need to turn back on IE and make sure EXL is clear */
	MFC0(t1,C0_SR)
	or	t1, SR_EXL|SR_IE
	xori	t1, SR_EXL
	MTC0(t1,C0_SR)
	
	/* this symbol used by idbg for backtracing */
EXPORT(kpreemption)
	.set reorder
	jal	kpswtch

	# fall through
#if R4000 || R10000
	lui	a0,(SR_DE >> 16)	# propagate the SR_DE bit
	ori	a0, SR_ERL
	and	a0,a0,s0
	or	a0,SR_IEC|SR_IMASK8|SR_KERN_SET
#endif	/* R4000 || R10000 */	
#if TFP
	and	a2,s0,SR_KERN_USRKEEP
	LI	a0,SR_IEC|SR_KERN_SET	# enable, but mask all interrupts
	or	a0,a2
#endif
	.set	noreorder
	MTC0(a0,C0_SR)
	NOP_0_4

#if IP32
	sw	s4,VPDA_CURSPL(zero)
	ld	a0,VPDA_SPLMASKS(s4)
	la	k1,CRM_INTMASK|K1BASE
	sd	a0,0(k1)
#endif /* IP32 */
#if EVEREST
	ld	a0,EF_CEL(s1)		# restore saved value of CEL
#if TFP
	DMFC0(k1,C0_CEL)
#else
	LI	k1,EV_CEL
#endif
	FORCE_NEW_CEL(a0,k1)
#endif /* EVEREST */
	move	k1,s1			# restore eframe ptr

return_from_int:
	.set	reorder
	j	exception_leave

bad_istack:
	move	a2, sp
	SPPANIC("bad istack sp:%x")
	END(VEC_int)
