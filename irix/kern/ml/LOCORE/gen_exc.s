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

#ident	"$Revision: 3.525 $"

#include "ml/ml.h"
#include "sys/traplog.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

#if SW_FAST_CACHE_SYNCH || SWASH
#define BRK_PSEUDO_OPS	1
#endif 

#if _EXCEPTION_TIMER_CHECK
	SETUP_TIMER_CHECK_DATA()
#endif /* _EXCEPTION_TIMER_CHECK */

/* Default to intrnorm for new machines, so we do not have to touch
 * this for every new machine, as we should try to avoid intrfast
 * when possible.
 */
#if IP20 || IP22 || IP26 || IP28
#define INTR_NORM_OR_FAST	intrfast
#else
#define	INTR_NORM_OR_FAST	intrnorm
#endif

/* R10000_SPECULATION_WAR: $sp is not valid yet, but there are no non $0
 * stores (for IP28) here -- early mfc0 breaks any chance of spec entry.
 */
	.align	7	/* for performance, start on cachline boundary */
VECTOR(exception, M_EXCEPT)
	.globl	kptbl
	.set	noat
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE
#ifdef ULI_TSTAMP1
	ULI_GET_TS(k0, k1, TS_EXCEPTION, uli_tstamps)
	.set	noreorder
#endif
#if TRAPLOG_DEBUG && !TFP
	TRAPLOG(4)
#endif
#ifdef _EXCEPTION_TIMER_CHECK
	DO_EXCEPTION_TIMER_CHECK()
#endif
	MFC0(k0,C0_CAUSE)
#ifdef R4000_LOG_EXC
	DO_R4000_LOG_EXC()
#endif
	and	k0,CAUSE_EXCMASK	# isolate exception cause
	beq	k0,zero,is_intr
	xori	k0,EXC_SYSCALL		# BDSLOT
#if R4000 && (! defined(_NO_R4000))
	bne	k0,zero,1f
	xori	k0,EXC_VCED^EXC_SYSCALL	# BDSLOT - restore k0
	
	j	systrap			# System call
	nop				# BDSLOT (AT contexts still in reg!)
1:		
	bnel	k0,zero,1f
	xori	k0,EXC_VCEI^EXC_VCED	# BDSLOT

2:	
	/* Both VCEI and VCED use this jump point to conserve code space
	 * and use minimal icache lines for exception decode.
	 * Note that VCED will end up with zero in k0, VCEI non-zero
	 * (in fact, VCEI will end up with k0 == EXC_RMISS).
	 */
	j	handle_vce		# data VCE
	move	k1,k0
1:
	beq	k0,zero,2b
	xori	k0,EXC_RMISS^EXC_VCEI	# BDSLOT - check for RMISS

	#
	# In order for VCE handler to work properly, we must not 
	# do ANY cached stores before this point!
	#
	
#else /* !R4000 || _NO_R4000 */	
	bne	k0,zero,not_intr
	xori	k0,EXC_RMISS^EXC_SYSCALL	# BDSLOT - restore k0

	j	systrap			# System call
	nop				# BDSLOT (AT contexts still in reg!)

#endif	/* !R4000 || _NO_R4000 */

not_intr:
	bne	k0,zero,1f		# if not RMISS, go check for WMISS
	xori	k0,EXC_WMISS^EXC_RMISS	# BDSLOT

handle_kmiss:
	
	j	kmiss			# TLB r/w misses
	sreg	AT,VPDA_ATSAVE(zero)	# BDSLOT save AT for user mode
1:
	beq	k0,zero,handle_kmiss	# goto kmiss for EXC_WMISS
#if IP32 && defined(FAST_TEXTURE_LOAD)
	xori	k0,EXC_WADE^EXC_WMISS	# BDSLOT - Handle write address error

	bne	k0,zero,1f		# Handle write address error
	xori	k0,EXC_WATCH^EXC_WADE	# BDSLOT

	j	handle_wade
	sreg	AT,VPDA_ATSAVE(zero)	# BDSLOT save AT for user mode
1:
#else /* !IP32 || !FAST_TEXTURE_LOAD */	
#if R4000 || R10000
	xori	k0,EXC_WATCH^EXC_WMISS	# BDSLOT
#endif
#if TFP
	xori	k0,EXC_BREAK^EXC_WMISS	# BDSLOT
#endif
#endif	/* !IP32 || !FAST_TEXTURE_LOAD */

#if R4000 || R10000
	beq	k0,zero,is_watch
	xori	k0,EXC_BREAK^EXC_WATCH	# BDSLOT
#endif	/* R4000 || R10000 */

	beq	k0,zero,is_brkpt	# breakpoint exc.
	sreg	AT,VPDA_ATSAVE(zero)	# BDSLOT save AT for user mode
	
#ifdef _R5000_CVT_WAR
	xori	k0,EXC_II^EXC_BREAK	# BDSLOT
	beq	k0,zero,3f		# Check for possible emulation
	nop
	xori	k0,EXC_FPE^EXC_II	# BDSLOT
	bne	k0,zero,2f		# Check for possible emulation
	nop

3:	
	j	handle_ii
	nop
2:
#endif	/* _R5000_CVT_WAR */

	/*
	 * At this point some exception
	 */
go_longway:		
	j	longway
	nop				# BDSLOT

#if R10000 && (_MIPS_SIM == _ABI64)
no_wgflush:
	j	INTR_NORM_OR_FAST	# mach dep interrupt/fast path interrupt
	sreg	AT,VPDA_ATSAVE(zero)	# BDSLOT - save AT for user mode
	
is_intr:		
	/* write gather workaround to prevent partial cacheline writes */

	daddiu  k0,zero,-1775	        
	bnel    k0,sp,no_wgflush	# check for atomic lock
	nop				# BDSLOT
	j	handle_intr_wgflush
	nop
#else	/* !R10000 || !_ABI64 */	
	
is_intr:
	j	INTR_NORM_OR_FAST	# mach dep interrupt/fast path interrupt
	sreg	AT,VPDA_ATSAVE(zero)	# BDSLOT - save AT for user mode
#endif /* !R10000 || !_ABI64 */
	
/* These jump points may "fall off" the performance critical first
 * cacheline, but that's OK since this is not the common case
 * (breakpoints and watchpoints only occur running symmon and
 * even then only occur when you hit them).
 */
	
is_brkpt:	
	j	handle_brkpt
	sreg	AT,VPDA_ATSAVE(zero)	# BDSLOT save AT for user mode
is_watch:	
	j	handle_watch
	sreg	AT,VPDA_ATSAVE(zero)	# BDSLOT - save AT for user mode
XLEAF(exception_endfast)

	
/* Here starts code which is NEVER copied into the locore exception
 * trap location.  Code prior to exception_endfast may be copied
 * to the low memory exception entry point.
 */
	
#if R10000 && (_MIPS_SIM == _ABI64)
XLEAF(handle_intr_wgflush)	
	sreg	AT,VPDA_ATSAVE(zero)	# save AT for user mode

	LA      k1,wgflush_start	# prepare to continue 16 sd's
	daddu   sp,AT,zero		# restore sp / clear lock
					
	dsll    a1,a1,20		# mask off the upper bits of the
	dsrl    a1,a1,20		# address for security reasons
	
	DMFC0(AT, C0_EPC)		# load PC into k0
	dsll    k0,AT,57		# user code is cacheline aligned
	dsrl    k0,k0,57		# extract cacheline offset
	
	daddu   k1,k1,k0		# add offset to wgflush_start
	jr      k1			# jump to where we left off
	daddiu  k0,k0,-68		# prepare to increment PC
			
EXPORT(wgflush_start)
	nop				# we should never hit this nop
	sd      a2,0(a1)
	sd      a3,8(a1)
	sd      a4,16(a1)
	sd      a5,24(a1)
	sd      a6,32(a1)
	sd      a7,40(a1)
	sd      t0,48(a1)
	sd      t1,56(a1)
	sd      t2,64(a1)
	sd      t3,72(a1)
	sd      t8,80(a1)
	sd      t9,88(a1)
	sd      v0,96(a1)
	sd      v1,104(a1)
	sd      s2,112(a1)
	sd      a0,120(a1)

	dsubu   AT,AT,k0		# increment passed critical section
	DMTC0(AT, C0_EPC)		# store PC

	/* replicate no_wgflush code here for performance */		
	
	j	INTR_NORM_OR_FAST	# mach dep interrupt/fast path interrupt
	nop				# BDSLOT
#endif /* R10000 && _ABI64 */

XLEAF(handle_brkpt)
	.set	at
	
	/*
	 * Handle kernel breakpoints now so don't get into stack trouble
	 * also provides slightly better information
	 */
	MFC0(k0,C0_SR)				# see if from user land
	NOP_1_1					# mfc0 LDSLOT
	and	k0,SR_PREVMODE
#if BRK_PSEUDO_OPS
	.data 
brk_pseudo_op_table:
	/* If you add an #if'ed entry to this table, you MUST include an
	 * #else clause with an equal number of entries.  That's because 
	 * the offsets are computed from signal.h constants that are the
	 * same regardless of compile-time flags.
	 *
	 * Also, note that EPC is in k0 when the pseudo-op handler is
	 * called.  It's the handler's responsibility to step over the
	 * break instruction before doing an ERET.  If you are sure that
	 * the pseudo-op will NEVER occur in a branch delay slot, that
	 * can be as simple as:
	 *	PTR_ADDIU k0, 4
	 * 	DMTC0(k0,C0_EPC)
	 */
#if SW_FAST_CACHE_SYNCH
	PTR_WORD cache_sync
#else
	PTR_WORD 0	
#endif
#ifdef SWASH	
	PTR_WORD swash_tlbflush
	PTR_WORD swash_swtch
#else
	PTR_WORD 0
	PTR_WORD 0
#endif
ebrk_pseudo_op_table:
	.text	
#define BRK_SHIFT 16		  		/* see note in trap.c */
#define PSEUDO_OP_FLAG 	(BRK_PSEUDO_OP_BIT<<PTR_SCALESHIFT)
	beq	k0, zero, 2f			# zero --> kernel mode
	DMFC0(k0,C0_EPC)			# k0= addr of BREAK instruction
	nop					# LDSLOT
	lw  	k1, 0(k0)			# k1= bits of BREAK instruction
	nop
	srl	k1, BRK_SHIFT-PTR_SCALESHIFT	# k1= CODE*sizeof(word)
	.set	noat
	andi	AT, k1, PSEUDO_OP_FLAG		# AT= set if it's a pseudo_op
	beq	AT, zero, go_longway		# not a pseudo-op
	nop

	andi	k1, (~PSEUDO_OP_FLAG)&0xffff	# k1= table offset for this op
	slti	AT, k1, BRK_PSEUDO_OP_MAX<<PTR_SCALESHIFT # AT= set if in bounds
	beq	AT, zero, go_longway		# illegal BREAK code
	nop

	LA	AT, brk_pseudo_op_table		# BDSLOT
	PTR_ADDU k1, AT			# k1= pointer to our table entry
	.set	at
	PTR_L	k1, 0(k1)		# k1= handler for this pseudo-op
	jr	k1			# jump to the handler
	nop
2:
#else /* !BRK_PSEUDO_OPS */
	bne	k0,zero,go_longway		# zero --> kernel mode
	nop					# BDSLOT
#endif	/* BRK_PSEUDO_OPS */

	MFC0(k0,C0_CAUSE)			# BDSLOT
	.set	noat
	DMFC0(AT,C0_EPC)			# read EPC (mfc0 LDSLOT)
	bgez	k0,nobd				# not delay slot (dmfc0 LDSLOT)
	nop					# BDSLOT
	PTR_ADDU AT,4				# advance pc to bdslot
nobd:
	move	k0,AT				# mfc0 LDSLOT (a lie!)
	lw	k0,0(k0)			# read faulting instruction
	lw	AT,kernelbp			# what a kernel bp looks like
	bne	AT,k0,go_longway		# test for kernel bp inst
	nop					# BDSLOT

XLEAF(handle_watch)
	
#if MP
	CPUID_L	k0, VPDA_CPUID(zero)
	sll	k0, 2				# get word offset
	LA	AT, flag_at_kernelbp
	PTR_ADDU AT, k0
	li	k0, 1	
	sw	k0, (AT)			# set kernelbp flag
#endif /* MP */
#if	defined(SN0) || defined(CELL)
	/*
	 * For SN0, when entering the debugger, if the debugger_update
	 * pointer is !0, that value is updated with the value in
	 * debugger_stopped. 
	 */
	.comm	debugger_update,8
	.comm	debugger_stopped,8
	LA	AT, debugger_update
	PTR_L	AT, 0(AT)
	beq	AT, zero, 2f
	nop
	LA	k0, debugger_stopped
	ld	k0, 0(k0)
	sd	k0, 0(AT)
2:		
#endif
#if	CELL
	.globl	hb_system_in_debugger_addr
	LA	AT, hb_system_in_debugger_addr
	PTR_L	AT, 0(AT)
	beq	AT, zero, 2f
	 li	k0, 1				# BDSLOT
	sw	k0, (AT)			# set hb system in debug flag
2:		
#endif

	# ARCS prom
	LI	AT,SPB_DEBUGADDR		# address of debug block
	PTR_L	AT,0(AT)
	beq	AT,zero,go_longway		# debug block exists?
	nop					# BDSLOT

	PTR_L	AT,DB_BPOFF(AT)			# breakpoint handler
	beq	AT,zero,go_longway		# handler in debug block?
	lreg	k0,VPDA_ATSAVE(zero)		# BDSLOT (brkpt handler)

#ifdef _EXCEPTION_TIMER_CHECK
	la	k0,exception_timer_bypass
	sw	zero,0(k0)
	lreg	k0,VPDA_ATSAVE(zero)		# XXX needed by brkpt handler?
#endif /* _EXCEPTION_TIMER_CHECK */
	j	AT				# enter breakpoint handler
	nop					# BDSLOT

	.set 	reorder
	.set	at
	AUTO_CACHE_BARRIERS_ENABLE
	END(exception)
