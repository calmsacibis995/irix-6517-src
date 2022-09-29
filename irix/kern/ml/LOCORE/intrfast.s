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

#ident	"$Revision: 1.9 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

#if defined(IP20) || defined(IP22) || defined(IP26) || defined(IP28)
/* whole file */

VECTOR(intrfast, M_EXCEPT)
/*
 * inttrap - Interrupt exception fast path handler
 *
 * Some interrupts may be handled by an inline handler.  On all machines,
 * the high speed clock is handled here.  On IP2[028], some FIFO full
 * interrupts are also handled.
 *
 * Since we know this not a second level TLB miss, it is safe to use
 * k1 in addition to k0, and the already-saved AT.
 *
 * R10000_SPECULATION_WAR: $sp is not defined yet.
 */
	AUTO_CACHE_BARRIERS_DISABLE
#if defined(IP20) || defined(IP22) || defined(IP28)
	/*
	 * IP20/IP22/28 FIFO Full fast path handler
	 *
	 *
	 * The IP20/IP22 fifo full interrupt is latched in the INT2
	 * chip whenever the graphics board signals that fifo is full.
	 * The fifo full is not latched in the local 0 interrupt
	 * status register however.  The interrupt is unlatched by
	 * masking it and then re-enabling it.  This code tries
	 * twice to unlatch the fifo full interrupt.
	 */
	.set	noreorder
	/*
	 * Check if the interrupt was a local 0 and local 0 was enabled.
	 */
	DMFC0(k0,C0_CAUSE)
	DMFC0_NO_HAZ(k1,C0_SR)
	and	k0,CAUSE_IP3			# LDSLOT
	and	k0,k1
	beqz	k0,inttrapcont			# Skip fast path

	/*
	 * If fifo full is not enabled, skip the fast path.  This is likely
	 * to happen if we are getting a nested interrupt while servicing
	 * a fifo full.
	 */

	/* try #1 */
	.set noat
#if IP22
	nop					# BDSLOT
	CLI	k0, PHYS_TO_COMPATK1(HPC3_SYS_ID)
	lw	k0, 0(k0)
	CLI	AT, PHYS_TO_COMPATK1(HPC3_INT3_ADDR)	# assume IOC1/INT3
	andi	k0, CHIP_IOC1
	bnez	k0, 1f				# branch if IOC1/INT3
	nop					# BDSLOT
	PTR_SUBU	AT, HPC3_INT3_ADDR-HPC3_INT2_ADDR
1:
#elif IP28
	nop					# BDSLOT
	CLI	AT, PHYS_TO_COMPATK1(HPC3_INT2_ADDR)
#else /* IP20 */
#define	LIO_0_MASK_OFFSET	LIO_0_MASK_ADDR&0x7fff
#define	LIO_0_ISR_OFFSET	LIO_0_ISR_ADDR&0x7fff

	LI	AT,(K1BASE|LIO_0_MASK_ADDR)&(~0xffff) # BDSLOT
#if LIO_0_MASK_ADDR & 32768
	ori	AT,0x8000
#endif
#endif /* IP20 */
	lb	k0,LIO_0_MASK_OFFSET(AT)	# save orignal lio0 mask
	nop					# LDSLOT
	and	k1,k0,LIO_FIFO
	beqz	k1,inttrapcont			# fifo not enabled

	/*
	 * Unlatch FIFO full by masking it
	 */
	and	k1,k0,~LIO_FIFO & 0xff		# BDSLOT
	sb	k1,LIO_0_MASK_OFFSET(AT)	# mask to unlatch
	sb	k0,LIO_0_MASK_OFFSET(AT)	# restore original lio0 mask
	lb	k1,LIO_0_ISR_OFFSET(AT)	# flush wb and read status
	NOP_0_4

	DMFC0(k0,C0_CAUSE)
#ifndef R10000
	nop					# MFSLOT
#endif
	and	k0,CAUSE_IPMASK
	beqz	k0,fifoout0			# fifo gone

	/* try #2 */
	lb	k0,LIO_0_MASK_OFFSET(AT)	# save orignal lio0 mask
	nop					# LDSLOT
	and	k1,k0,LIO_FIFO
	beqz	k1,inttrapcont			# fifo not enabled

	/*
	 * Unlatch FIFO full by masking it
	 */
	and	k1,k0,~LIO_FIFO & 0xff
	
	sb	k1,LIO_0_MASK_OFFSET(AT)	# mask to unlatch
	sb	k0,LIO_0_MASK_OFFSET(AT)	# restore original lio0 mask
	lb	k1,LIO_0_ISR_OFFSET(AT)	# flush wb and read status
	NOP_0_4
	.set	at

	DMFC0_NO_HAZ(k0,C0_CAUSE)	# check if any interrupts still pending
#ifndef R10000
	nop					# MFSLOT
#endif
	and	k0,CAUSE_IPMASK
	bnez	k0,inttrapcont			# something is still pending
	nop					# BDSLOT

#ifdef FIFODEBUG
	.data
	.globl	fifofast0
fifofast0:
	.word	0
	.globl	fifofast1
fifofast1:
	.word	0
	.text
fifoout1:
	lw	k1,fifofast1			# BDSLOT
	addiu	k1,1
	CACHE_BARRIER_PDA			# barrier for debug store
	sw	k1,fifofast1
	j	fiforfe
	nop					# BDSLOT

fifoout0:
	lw	k1,fifofast0
	addiu	k1,1
	CACHE_BARRIER_PDA			# barrier for debug store
	sw	k1,fifofast0
	j	fiforfe
	nop					# BDSLOT
#else
fifoout1:
fifoout0:
#endif	/* FIFODEBUG */

fiforfe:
	.set	noat
	lreg	AT,VPDA_ATSAVE(zero)	# restore AT
	NOP_0_4
	ERET_PATCH(locore_eret_4)	# patchable eret
	.set	at

inttrapcont:
#endif	/* IP20 || IP22 || IP28 */

#ifdef IP26
	/*
	 * IP26 FIFO Full fast path handler
	 *
	 * The IP26 TCC HW interrupt is latched in the TCC INTERRUPT register
	 * whenever the graphics board signals that fifo is full.
	 * The interrupt is unlatched by writing one to the status bit.
	 * This code tries six time to unlatch the fifo full interrupt.
	 * TCC overlays TCCHW onto IP3, aka HQ2 hiwater
	 */
#define	TCC_INTR_OFFSET		(TCC_INTR-TCC_BASE)
#define	TCC_FIFO_OFFSET		(TCC_FIFO-TCC_BASE)
	.set	noreorder
	/*
	 * Check if the interrupt was a local 0 and local 0 was enabled.
	 */
	dmfc0	k0,C0_CAUSE
	ssnop					# C0 hazzard
	dmfc0	k1,C0_SR
	and	k0,CAUSE_IP3|CAUSE_IP7		# any lcl0/BE interrupt?
	and	k0,k1				# is it enabled?
	beqz	k0,inttrapcont			# no lcl0/BE interrupts

	and	k1,k0,CAUSE_IP7			# BDSLOT: Bus error or lcl0?
	bnez	k1,sync_fastpath
	nada					# BDSLOT

	/*
	 * If fifo full is not enabled, skip the fast path.  This is likely
	 * to happen if we are getting a nested interrupt while servicing
	 * a fifo full.
	 */

	/* try #1 */
	.set noat
	LI	AT,(K1BASE|TCC_BASE)		# base of tcc registers
	ld	k0,TCC_INTR_OFFSET(AT)		# save orignal tcc intr value
	andi	k1,k0,INTR_FIFO_HW_EN|INTR_FIFO_HW
	xori	k1,k1,INTR_FIFO_HW_EN|INTR_FIFO_HW
	bnez	k1,inttrapcont			# HW not enabled and on
	nada					# BDSLOT

	/*
	 * Unlatch TCC HW full by writing 1 to ISR bit.
	 */
	li	k1,INTR_FIFO_LW|INTR_TIMER|INTR_BUSERROR|INTR_MACH_CHECK
	not	k1
	and	k1,k0				# calc value to write back
	sd	k1,TCC_INTR_OFFSET(AT)		# write one to unlatch
	ssnop; ssnop; ssnop;			# wait for intr to propegate

	dmfc0	k0,C0_CAUSE			# ck if any intrs still pending
	li	k1,CAUSE_IPMASK
	ssnop					# C0 hazzard
	and	k0,k1
	beqz	k0,fifoout0			# fifo gone
	nada					# BDSLOT

	/*  Check INT2 to see if there are other lcl0 interrupts.  This
	 * also gives the fifo a bit of time to drain as the TCC PIOs
	 * are relatively fast.  Cheat and only check ISR, as they
	 * probably need to be serviced as IP3 is enabled in C0_SR.
	 */
	LI	k0,K1BASE|HPC3_INT2_ADDR|LIO_0_ISR_OFFSET
	lb	k1,0(k0)
	andi	k1,(~LIO_FIFO)&0xff		# all but fifo
	bnez	k1,inttrapcont
	nada					# BDSLOT

	/* try #2 */
	/*
	 * Unlatch TCC HW full by writing 1 to ISR bit.
	 */
	ld	k0,TCC_INTR_OFFSET(AT)		# save orignal tcc intr
	li	k1,INTR_FIFO_LW|INTR_TIMER|INTR_BUSERROR|INTR_MACH_CHECK
	not	k1
	and	k1,k0				# calc value to write back
	sd	k1,TCC_INTR_OFFSET(AT)		# write one to unlatch
	ssnop; ssnop; ssnop;			# wait for intr to propegate

	dmfc0	k0,C0_CAUSE			# ck if any intrs ending
	li	k1,CAUSE_IPMASK
	ssnop					# C0 hazzard
	and	k0,k1
	bnez	k0,inttrapcont			# interrupt still pending
	nada					# BDSLOT
	.set	at

#ifdef FIFODEBUG
	.data
	.globl	fifofast0
fifofast0:
	.word	0
	.globl	fifofast1
fifofast1:
	.word	0
	.text
fifoout1:
	lw	k1,fifofast1
	addiu	k1,1
	sw	k1,fifofast1
	j	fiforfe
	nop					# BDSLOT

fifoout0:
	lw	k1,fifofast0
	addiu	k1,1
	sw	k1,fifofast0
	j	fiforfe
	nop					# BDSLOT
#else
fifoout1:
fifoout0:
#endif	/* FIFODEBUG */

fiforfe:
	.set	noat
	lreg	AT,VPDA_ATSAVE(zero)		# restore AT
	NOP_0_4
	eret

sync_fastpath:
	LI	k0,PHYS_TO_K1(TCC_INTR)		# machine check?
	ld	k1,0(k0)
	and	k1,INTR_MACH_CHECK
	beqz	k1,inttrapcont
	nada					# BDSLOT

	LI	k0,PHYS_TO_K1(TCC_ERROR)	# type == ERROR_TBUS_UFO?
	ld	k1,0(k0)
	and	k1,ERROR_TYPE
	srl	k1,ERROR_TYPE_SHIFT
	sub	k1,ERROR_TBUS_UFO
	bnez	k1,inttrapcont
	nada					# BDSLOT

	LI	k0,PHYS_TO_K1(TCC_INTR)		# cleartccerrors
	ld	k1,0(k0)
	and	k1,INTR_MACH_CHECK|0xf800
	sd	k1,0(k0)
	LI	k0,PHYS_TO_K1(TCC_ERROR)
	li	k1,INTR_MACH_CHECK>>9;
	sd	k1,0(k0)
	ld	zero,0(k0)			# flushbus

	LA	k0,_tbus_fast_ufos		# maintain count of errors.
	ld	k1,0(k0)
	daddi	k1,1
	sd	k1,0(k0)

	eret					# keep going

	.set	at

inttrapcont:
#endif /* IP26 */

	/* COMMON CODE for IP20, 22, 26, 28, 30 */

	#
	# Fast path for fast clock handling
	# On machine that has no HW counter, we use the fast clock
	# to implement a free running counter at FASTHZ.
	# To reduce overhead in handling fast itimer we check to see
	# if the fast callout list is non-empty before saving machine context.
	# 

	.set	noreorder
#if IP20 || IP22 || IP28
	mfc0	k0,C0_CAUSE
	NOP_1_1
#ifdef IP22
	# skip clock fast path for bad IOC
	LA	k1, is_fullhouse_flag	# is this a fullhouse
	lw	k1, 0(k1)
	bne	k1, zero, 1f
	nop				# BDSLOT

	j	intrnorm		# skip for guiness
	nop
1:
#endif /* IP22 */
	and	k1,k0,CAUSE_IP8		# Count/compare intr
	beq	k1,zero,fastclk_chk	# On to the next
	nop				# BDSLOT
	and	k0,~CAUSE_IP8
	# Clear the count interrupt, then fall through
	# to the fast clock check code
	mtc0	zero,C0_COMPARE
	# For SP machine with HW counter(R4K COUNT reg)
	# Fast clock intr is for profiling and fast itimer 
	# We will try to reduce overhead
	# by making sure that the fast callout list is not empty
	# before going through intrnorm.
fastclk_chk:
	and	k0,CAUSE_IP6 
	bne	k0,zero,1f	# test for fastclock int
	nop

	j	intrnorm	# cause reg=>not a fastclock int
	nop

1:
#endif	/* IP20 || IP22 || IP28 */

#if IP26
	dmfc0	k0,C0_CAUSE
	andi	k0,CAUSE_IP6
	bne	k0,zero,1f
	nada				# BDSLOT + C0 

	j	intrnorm		# not a fastclock int
	nada
1:
	# falls through to fastitimer__check below
#endif

	.set	reorder
fastitimer__check:
	# At this point, know that there's a fast clock intr pending.
	# we now check to see if any fast callouts are pending.
	# don't even bother to get calock since not updating
	# the list.
	PTR_L	k0,fastcatodo
	PTR_L	k0,C_NEXT(k0)
	
	# check if anything is on fastcallout list
	beq	k0,zero,1f

	j	intrnorm	# need to handle fast itimer
1:
	# check if we need to take fastick intr
prof_check:
	
	/*
	** certain services (profiling, kdsp for example) require a 
	** callback on each fast timer tick.  Those services indicate 
	** whether the callback is currently needed by setting a bit in  
	** fastick_callback_required_flags.  We need to perform only one 
	** test of this flag here in order to see whether we need to jump 
	** to intrnorm for any of those services.
	**
	** NOTE any code attempting to set or clear a bit of this flag
	** must do so atomically using atomicSetUint or atomicClearUint.
	** existing services can access this flag from any processor
	** and from any interrupt level.
	*/
	.extern	fastick_callback_required_flags 0
	lw	k0,fastick_callback_required_flags
	beq	k0,zero,1f

	j	intrnorm
1:
	
	# there is nothing left to do
	# ack the clock then return
#if IP20 || IP22 || IP28
	wbflushm
#elif IP26
	wbflushm_setup(k1)
	wbflushm_reg(k1)
#endif
	FASTCLK_ACK(k0)			/* ack the clock interrupt	*/
#if IP20 || IP22 || IP28
	wbflushm
#elif IP26
	.set noreorder
	wbflushm_reg(k1)
	ssnop;ssnop;ssnop		/* wait for interrupt to clear */
	.set reorder
#endif
	.set	noreorder
#ifdef IP22
	# skip clock fast path for bad IOC
	LA	k1, is_fullhouse_flag		# is this a fullhouse
	lw	k1, 0(k1)
	bne	k1, zero, 1f		# test for guinness
	nop

	j	intrnorm		# skip for guiness
	nop
1:
#endif /* IP22 */

	/*
	** check for other enabled interrupts before exiting
	**
	** An anomaly of the FPU-CPU interrupt interface requires
	** that FPU interrupts be handled in the first exception
	** after they occur.  The problem is that the branch delay
	** bit in the cause register is lost otherwise.
	*/
	MFC0_NO_HAZ(k0,C0_CAUSE)
	MFC0_NO_HAZ(k1,C0_SR)
	and	k0,CAUSE_IPMASK
	and	k0,k1
	beq	k0,zero,1f
	nop				# BDSLOT

	j	intrnorm
	nop
1:
	.set	noat
	lreg	AT,VPDA_ATSAVE(zero)	/* restore AT		*/
	NOP_0_4
	ERET_PATCH(locore_eret_5)	# patchable eret

	AUTO_CACHE_BARRIERS_ENABLE

	END(intrfast)

#endif	/* IP20 || IP22 || IP26 || IP28 */
