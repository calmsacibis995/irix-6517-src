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

#ident	"$Revision: 1.24 $"

#include "ml/ml.h"

#ifdef IP19
#include <sys/EVEREST/evmp.h>	
#endif		
#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

#if R4000 || R10000

	/* $sp is not valid yet, but we run uncached so we are safe */
	AUTO_CACHE_BARRIERS_DISABLE

#ifndef _MEM_PARITY_WAR
#ifdef	R4000
/* ecc_exception: Build up a exception frame and
 * stack frame, then call the handler (written in C).
 * Handler arguments, a0: exception frame, a1, ecc frame,
 * a2: c0_cache_err, a3: c0_error_epc.
 * If the error is recoverable, the handler takes the appropriate
 * action, and returns 0. It it is not recoverable, the handler
 * plugs ECCF_PADDR in the ecc frame, with the contents of TAGLO
 * from the errored line, and returns non-zero. In that case
 * we print some error messages, and panic.
 */
VECTOR(ecc_exception, M_EXCEPT)
EXL_EXPORT(locore_exl_10)
	# Save all registers
	# We can't scratch any registers at all.
	# Put all the usual stuff onto the eframe
	sreg	k0,CACHE_ERR_EFRAME+(EF_K0)(zero)
	sreg	k1,CACHE_ERR_EFRAME+(EF_K1)(zero)
	.set	noreorder
	MFC0(k0,C0_SR)
	lui	k1, (SR_DE >> 16)	# block all interrupts & ecc exceptions
        or      k1, SR_ERL|SR_KERN_SET
	MTC0(k1, C0_SR)
	NOP_0_4
	.set	reorder
	li	k1,CACHE_ERR_EFRAME
	.set	noat
	sreg	AT,EF_AT(k1)
	.set	at
	sreg	k0,EF_SR(k1)
	SAVEAREGS(k1)
	.set	noreorder
	mfc0	a3,C0_CAUSE
	DMFC0(a0,C0_EPC)
	.set reorder
	sreg	a3,EF_CAUSE(k1)
	SAVESREGS(k1)
	sreg	a0,EF_EPC(k1)
	sreg	a0,EF_EXACT_EPC(k1)	
	.set noreorder
	DMFC0(a0,C0_BADVADDR)		# LDSLOT
	.set	reorder
	sreg	ra,EF_RA(k1)
	sreg	a0,EF_BADVADDR(k1)
	sreg	sp,EF_SP(k1)
	sreg	gp,EF_GP(k1)	# save previous gp and set kernel's
	LA	gp,_gp		# if ecc occurred in kernel gp'll be same

	# put sp onto the ecc handler stack.
#if (IP19) || (IP25)
	PTR_L	sp,CACHE_ERR_SP_PTR
	bne	sp, zero, good_sp
	li      sp, CACHE_ERR_SP_PTR
	subu	sp, sp, 0x10
good_sp:	      
#else        
	li	sp,CACHE_ERR_SP
#endif    
	jal	tfi_save		# Save tregs, v0, v1, mdlo, mdhi

#if IP19
	jal	get_mpconf		# only kills v0/v1 and tregs
	beq	v0,zero,1f		# shouldn't ever be zero
	lbu	a5, MP_VIRTID(v0)	# load logical cpuid
1:		
#endif /* IP19	*/	
	
	# save various r4k specific registers
	# Save C0_TAGLO, C0_ECC, C0_CACHE_ERR
	move	a0,k1
	.set	noreorder
	mfc0	a2,C0_CACHE_ERR
	DMFC0(a3,C0_ERROR_EPC)
	mfc0	k1,C0_TAGLO
	mfc0	k0,C0_ECC
	.set reorder
	li	a1,CACHE_ERR_ECCFRAME
	sreg	a2,ECCF_CACHE_ERR*SZREG(a1)
	sreg	a3,ECCF_ERROREPC*SZREG(a1)
	sreg	k1,ECCF_TAGLO*SZREG(a1)
	sreg	k0,ECCF_ECC*SZREG(a1)

	# If the ECC handler detects a fatal error, it plugs the
	# tag of the bad line (taglo format) into ECC_PADDR on
	# the ECCFRAME
	# clear it before calling the handler
	sreg	zero,ECCF_PADDR*SZREG(a1)
#if IP19 || IP32
	sreg	zero,ECCF_PADDRHI*SZREG(a1)
#endif

	# Dispatch to ecc error handler.
	# Args:	a0 - eframe pointer
	#	a1 - eccframe pointer
	#	a2 - C0_CACHE_ERR
	#	a3 - C0_ERROREPC
#ifdef IP19
	#	a4 - K1 addr of ecc_info (so compiler can't generate "gp" address)
	#	a5 - cpuid
	
	LA	a4, real_ecc_info
	dsll	a4, 24
	dsrl	a4, 24
	LI	v0, K1BASE
	or	a4, v0
#endif /* IP19 */		
	jal	ecc_handler

	# returns 0 if error was corrected - OK to resume execution
	bne	v0,zero,ecc_bad

	# Error was corrected
	# restore state and return

	# These registers are changed in cp0 by the handler.
	lreg	k0,CACHE_ERR_ECCFRAME+(ECCF_ECC*SZREG)(zero)
	lreg	a2,CACHE_ERR_ECCFRAME+(ECCF_TAGLO*SZREG)(zero)
	lreg	a1,CACHE_ERR_ECCFRAME+(ECCF_CACHE_ERR*SZREG)(zero)
	.set	noreorder
	mtc0	k0,C0_ECC
	mtc0	a2,C0_TAGLO
	mtc0	a1,C0_CACHE_ERR
	.set	reorder

	# have to restore k0 and k1 also.
	li	k1,CACHE_ERR_EFRAME
	jal	tfi_restore			# restore t regs
	RESTOREAREGS(k1)
	RESTORESREGS(k1)
	lreg	ra,EF_RA(k1)		# LDSLOT
	.set	noat
	lreg	AT,EF_AT(k1)
	.set	at
	lreg	sp,EF_SP(k1)
	lreg	gp,EF_GP(k1)		# restore previous gp
	lreg	k0,EF_SR(k1)
	.set	noreorder
	MTC0(k0,C0_SR)
	NOP_0_4
	NOP_0_1
	.set	reorder
	lreg	k0,EF_K0(k1)
	lreg	k1,EF_K1(k1)
	.set	noreorder
	NOP_0_4
	ERET_PATCH(locore_eret_8)	# patchable eret
	nop
	.set	reorder

ecc_bad:
	sw	sp,eccword		/* poke something for LAs if can't
					 * really print */

	/* make sure a good frame - this works except in VERY early cases */
	lreg	sp,VPDA_LBOOTSTACK(zero)
	.set    noreorder
	li      a2,PDA_CURIDLSTK        /* on kernel idle stack */
	sb      a2,VPDA_KSTACK(zero)
	.set    reorder

	/* "real" error_epc has been saved.  Setup a new one which will panic
	 * just in case the ecc_panic code causes an exception (if it did
	 * then the ERET from that exception returns to the ERROR_EPC which
	 * is the point of occurance of the original cache error).
	 */
	.set	noreorder
	LA	a2,ecc_panic_eret
	DMTC0(a2,C0_ERROR_EPC)
	.set	reorder
	
#ifdef IP19
	# Args to ecc_panic:
	#	a0 - K1 addr of ecc_info (so compiler can't generate "gp" addr)

	LA	a0, real_ecc_info
	dsll	a0, 24
	dsrl	a0, 24
	LI	a5, K1BASE
	or	a0, a5
	jal	ecc_panic
	
#else /* !IP19 */
	li	a2,CACHE_ERR_ECCFRAME
	lreg	a0,ECCF_CACHE_ERR*SZREG(a2)
	lreg	a1,ECCF_ERROREPC*SZREG(a2)

	# Args to ecc_panic:
	#	a0 - C0_CACHE_ERR
	#	a1 - C0_ERROREPC
	
	jal	ecc_panic
#endif	/* !IP19 */

	/* NOT REACHED, ecc_panic will never return */

ecc_panic_eret:
	PANIC("ecc_panic processing caused an exception")
	
EXPORT(eccword)
	.word	0

	.text
EXL_EXPORT(elocore_exl_10)
	END(ecc_exception)

#ifdef IP19
	/* a0 = cached address of secondary cacheline we can trash
	 * returns 0 if we can read back what we store, 1 otherwise
	 */
LEAF(ecc_check_cache)
	.set noreorder
	li	a1, 0x11112222
	sw	a1,(a0)
	addu	a1,a1
	sw	a1,4(a0)
	addu	a1,a1
	sw	a1,8(a0)
	addu	a1,a1
	sw	a1,12(a0)

	li	a2, 0x11112222
	lw	a1,(a0)
	bne	a1,a2,2f
	addu	a2, a2		/* BDSLOT */
	
	lw	a1,4(a0)
	bne	a1,a2,2f
	addu	a2,a2		/* BDSLOT */
	
	lw	a1,8(a0)
	bne	a1,a2,2f
	addu	a2,a2		/* BDSLOT */
	
	lw	a1,12(a0)
	bne	a1,a2,2f
	nop			/* BDSLOT */


	/* repeat with different pattern, so next time through we don't
	 * succeeed "accidently" due to leftover data.
	 */
	
	li	a1, 0x01020304
	sw	a1,(a0)
	addu	a1,a1
	sw	a1,4(a0)
	addu	a1,a1
	sw	a1,8(a0)
	addu	a1,a1
	sw	a1,12(a0)

	li	a2, 0x01020304
	lw	a1,(a0)
	bne	a1,a2,2f
	addu	a2, a2		/* BDSLOT */
	
	lw	a1,4(a0)
	bne	a1,a2,2f
	addu	a2,a2		/* BDSLOT */
	
	lw	a1,8(a0)
	bne	a1,a2,2f
	addu	a2,a2		/* BDSLOT */
	
	lw	a1,12(a0)
	bne	a1,a2,2f
	nop			/* BDSLOT */

	/* success */
	jr	ra
	move	v0,zero		/* BDSLOT */
	
	/* failure */
2:	jr	ra	
	li	v0,1		/* BDSLOT */
	END(ecc_check_cache)
	.set	reorder
#endif /* IP19 */
	
#ifdef _FORCE_ECC
	.align	7
LEAF(ecc_store_err)
	.set noreorder	
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
/* second cacheline */
	nop
	nop
	nop
	nop
	
	b	1f
	sw	a0,(a1)
1:	
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
/* at least another cacheline follows */

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	
	j	ra
	nop
	.set reorder
	END(ecc_store_err)	
#endif /* _FORCE_ECC */		
#endif /* R4000 */

#ifdef R10000
#ifndef R4000
#define	r10k_ecc_exception	ecc_exception
#define r10k_ecc_handler	ecc_handler
#define r10k_ecc_panic		ecc_panic
#endif /* !R4000 */

VECTOR(r10k_ecc_exception, M_EXCEPT)

	.set	noreorder
#if	EVEREST
	/*
	 * On entry, we have no registers to play with. We start
	 * by saving k1 in cp0 context and ll registers.
	 * save bottom 32 bits in lladdr and top 32 in context
	 */
	MTC0(k1, C0_LLADDR)
	DMTC0(k1, C0_CTXT)

	LI	k1,EV_SCRATCH
	sd	k0,0(k1)		/* k1 in scratch pad register */
	LI	k1,EV_SPNUM		/* Our physical id */
	ld	k1,0(k1)
	dsll	k1,3			/* 8-bytes per entry */
	LA	k0,cacheErr_frames
	dsll	k0,32			/* Convert to special paddr. */
	dsrl	k0,32
	PTR_L	k0,0(k0)		/* Pointer to array */
	PTR_ADD	k1,k0			/* address of our pointer */
	PTR_L	k1,0(k1)		/* Our save area /eframe */
#elif	IP28 || IP32 || IPMHSIM
	/*  No longer needs to do anything here.  AT/k0/k1 are saved in
	 * EFRAME @ k1 or tmp buffer (IP26/IP26+) @ k1.
	 */
#elif	IP30
	/*
	 * On entry, k1 is the stack pointer, registers AT and k0
	 * have already been saved on the stack.  register k1's
	 * original value is saved in C0 registers LLADR, WATCHHI,
	 * and WATCHLO
	 */
	.set	noat

	/* reconstruct k1 */
	MFC0(k0,C0_WATCHHI)
	dsll32	k0,29
	MFC0(AT,C0_WATCHLO)
	dsll32	AT,0				# remove sign extensions
	dsrl	AT,3				
	or	k0,AT
	MFC0(AT,C0_LLADDR)
	dsll32	AT,0				# remove sign extensions
	dsrl32	AT,0
	or	k0,AT
	sreg	k0,EF_K1(k1)
	.set	at
#elif	SN0
	/*
	 * AT, K0 and K1 already saved in ecc_errorvec (ecc_vec.s)
	 */
	LI	k1, CACHE_ERR_EFRAME
#else /* !SN0 */
#error	no ecc_exception code!
	LI	k1,CACHE_ERR_EFRAME
#endif
	SAVESREGS(k1)
	SAVEAREGS(k1)
#if !defined(IP28) && !defined(IP30) && !defined(SN0) && !defined(IP32)
	sreg	AT,EF_AT(k1)
#endif
	sreg	gp,EF_GP(k1)
	sreg	sp,EF_SP(k1)
	sreg	ra,EF_RA(k1)
	LA	gp,_gp		# if ecc occurred in kernel gp'll be same
	
	jal	tfi_save
	nop

#ifdef IP28 /* must be in slow mode for uncached C code (all doubles above) */
#if EF_SIZE > (CACHE_TMP_EFRAME2-CACHE_TMP_EFRAME1)
#error Need to increase IP28 CACHE_TMP_EFSIZE.
#endif
	jal	ip28_enable_ucmem	# turn of uncached memory access
	nop				# BDSLOT
	move	s2,v0			# save rc in callee saved

	/* On IP26/IP26+ we need to copy from the tmp to real EFRAME */
	CLI	v1,PHYS_TO_COMPATK1(HPC3_SYS_ID)# board revision info
	lw	v1,0(v1)
	andi	v1,BOARD_REV_MASK	# isolate board rev
	sub	v1,IP26_ECCSYSID	# IP26/IP26+
	bgtz	v1,19f			# skip if on IP28 bd
	nop				# BDSLOT

	LA	k0,cacheErr_frames	# ptr to ECC frame
	dsll	k0,8			# convert to physical
	dsrl	k0,8			# so we avoid the cache
	PTR_L	k0,0(k0)		# get ECCF ptr
	move	a0,k0

	li	v1,EF_SIZE		# copy full eframe
1:	ld	v0,0(k1)		# copy loop
	sd	v0,0(k0)
	addi	v1,-8
	PTR_ADDIU k1,8
	bgtz	v1,1b
	PTR_ADDIU k0,8			# BDSLOT

	move	k1,a0			# a real eframe, finally...
19:
#endif	/* IP28 */

	MFC0(k0,C0_SR)
	sreg	k0,EF_SR(k1)
	MFC0(k0, C0_CAUSE)
	sreg	k0, EF_CAUSE(k1)
	DMFC0(k0,C0_BADVADDR)
	sreg	k0,EF_BADVADDR(k1)
	DMFC0(k0, C0_EPC)
	sreg	k0,EF_EPC(k1)
	sreg	k0,EF_EXACT_EPC(k1)	
	
#if EVEREST
	/*
	 * Pick up the values stored in CP0 and scratchpad
	 * register - save them away in eframe.
	 */
	LI	k0,EV_SCRATCH
	ld	k0,0(k0)
	sd	k0,EF_K0(k1)

	/*
	 * bottom 32 bits of k1 is in lladdr and top 32 bits is in context
	 */
	MFC0(v0,C0_LLADDR)
	dsll	v0,32
	dsrl	v0,32
	DMFC0(v1,C0_CTXT)	
	dsrl	v1,32
	dsll	v1,32
	or	k0,v0,v1
	sreg	k0,EF_K1(k1)

	/*
	 * Turn on cache error LED, we turn it off on the way out.
	 */
	dli	s0,EV_LED_BASE
	ld	s1,0(s0)
	ori	s1,LED_CACHERR
	sd	s1,0(s0)
#endif
	move	s0,k1			/* Eframe pointer in safe place */
	PTR_ADDI s1,k1,EF_SIZE		/* ECCF frame - in safe place */

	lb	k0,ECCF_STATUS(s1)	/* Pick up current status */
	bnez	k0,1f			/* Don't update if already set */
	li	k0,ECCF_STATUS_ACTIVE
	sb	k0,ECCF_STATUS(s1)	/* Mark it active */

1:	
	/*
	 * Save extra registers in the ECCF frame.
	 */
	DMFC0(k0, C0_ERROR_EPC)
	sreg	k0,ECCF_ERROREPC(s1)
	DMFC0(v0, C0_TAGHI)
	sw	v0,ECCF_TAGHI(s1)
	DMFC0(v0, C0_TAGLO)
	sw	v0,ECCF_TAGLO(s1)
	DMFC0(v0, C0_CACHE_ERR)
	sw	v0,ECCF_CACHE_ERR(s1)
#ifndef SN0	
	/*
	 * Our stack is at the end of the memory pointed to by eframe.
	 */
	daddu	sp,s0,ECCF_STACK_SIZE
#else /* SN0 */
	/*
	 * Provision for stack expansion. If the stack space is deemed
	 * insufficient, store an address at CACHE_ERR_SP_PTR. Note, for
	 * SN0 this is per processor, since we are looking at the
	 * ualias space.
	 */
	PTR_L	sp,CACHE_ERR_SP_PTR
	bne	sp, zero, 1f
	nop				# BD slot (noreorder set)
	li      sp, CACHE_ERR_SP
1:	
#endif /*SN0 */	

	move	a0,s0			/* Eframe pointer */
	jal	r10k_ecc_handler
	move	a1,s1			/* BDSLOT: ECCF pointer */

	/*
	 * Returned in v0 is either 0 (NULL) meaing return and continue,
	 * or a pointer to a panic string. To panic, we "try" to
	 * get to a stack that won't cause problems becuase it is out of
	 * range.
	 */

	beqz	v0,1f
	nop
	lreg	sp,VPDA_LBOOTSTACK(zero)
	LA	gp,_gp			/* Give him a gp to use */
	/*
	 * Make sure we are in kernel mode....
	 */
	MFC0(t0, C0_SR)
	and	t0,~SR_KSU_MSK		/* Kernel mode ... */
	MTC0(t0, C0_SR)
	move	a1,s0			/* Eframe pointer */
	move	a2,s1			/* ECC frame pointer */
	jal	r10k_ecc_panic
	move	a0,v0
1:
	/*
	 * If it returns, we attempt to recover. Restore CP0 registers
	 * from ECCF frame.
	 */
#if defined(EVEREST)
	dli	a0,EV_LED_BASE		/* Turn off cache error LED */
	ld	a1,0(a0)
	xori	a1,LED_CACHERR
	sd	a1,0(a0)
#endif
#ifdef IP28
	move	a0,s2			# restore old ucmem mode
	sb	zero,ECCF_STATUS(s1)	# really too early, but it's uncached
	jal	ip28_return_ucmem	# return to previous memory mode
	nop				# BDSLOT
#endif
	lw	k1,ECCF_TAGHI(s1)
	MTC0(k1, C0_TAGHI)
	lw	k1,ECCF_TAGLO(s1)
	MTC0(k1, C0_TAGLO)
	jal	tfi_restore
	move	k1,s0			/* DELAY: EFRAME */

	/*
	 * reload the error epc from the ecc eframe and store it into
	 * the error_epc register
	 */
	lreg	k0,ECCF_ERROREPC(s1)
	DMTC0(k0, C0_ERROR_EPC)

	move	k0,s1			/* ECCF frame */	
	RESTOREAREGS(k1)
	RESTORESREGS(k1)
	lreg	ra,EF_RA(k1)
	.set	noat
	lreg	AT,EF_AT(k1)
	lreg	sp,EF_SP(k1)
	lreg	gp,EF_GP(k1)

	
#ifndef IP28	/* done previously when we can access uncached memory */
	sb	zero,ECCF_STATUS(k0)
#endif
	
	lreg	k0,EF_K0(k1)
	lreg	k1,EF_K1(k1)

	/* Back to whatever we were doing I guess */

#ifdef R4000
	eret
#else
	ERET_PATCH(locore_eret_8)	# patchable eret
#endif
	.set	at
	.set	reorder

	END(r10k_ecc_exception)

#endif	/* R10000 */

#else /* _MEM_PARITY_WAR */
/***********************************************************************
     The following cache error exception handler is a more 
extensive implementation, to support those machines which require
extended main memory error recovery.  At this writing (April 1994),
the IP20 and IP22 kernels are the only such systems.
 
Background:

     Cache and SysAD read response data parity or ECC errors are
reported as a cache error exception, via a dedicate cache error
exception vector.  On such an exception, the ERL bit is set in the
processor status, and the old program counter is stored in the
$errorepc register.  Moreover, when ERL is set, KUSEG is redefined as
a direct uncached map to physical memory (2 GB of memory in 32-bit
mode, all possible memory in 64-bit mode).  Note that nested
exceptions, as might be caused by getting a cache error exception in
the cache error exception handler, are non-reentrant, leading to an
exception loop.  Further, since the cache error exception can take
place in the middle of the general exception handler code, the cache
error exception handler may not in general modify any data structures
which are not owned by it (since there is no way to synchronize with
the owners of other structures), and it must assume that any data
structure not owned by it may be in middle of an update.

     We have two general classes of actions to take on a cache error
exception.  The first class includes those actions which fully and
transparently recover from the exception.  These include allowing
stores to overwrite words with bad parity and correcting errors in
kernel text pages using a software ECC calculation.  The second class
includes those actions which must, at least in part, take place in a
process context, since they involve discarding the contents of a page,
and either rereading the page from disk or killing the processes which
are accessing the page.

     The first class of actions must be handled mostly
in the cache error handler itself.  Since they can apply to all
instructions, we get the best coverage if we allow them to work
regardless of the interrupted context (unless the interrupted context
is itself the cache error handler).  Further, we cannot safely switch
to any other context, since we may have interrupted code which had all
interrupts disabled (either by having EXL set in SR or by having IE
reset in SR).  Also, we should avoid any cache access until we have
corrected any errors in cache tags, to avoid accidentally corrupting
memory with bad cache writebacks, and we should avoid any TLB misses,
to avoid reentering the utlbmiss and general exception handlers.
All correction in the first class is done in
the cache error exception handler, and any logging of errors is done,
at interrupt or process level.

     The second class of actions represent errors which occur in
either a kernel "nofault" routine or in a user context.  In either
case, it is safe to convert the cache error exception context to
a regular general exception context, and then proceed just as we
might with a vfault.  We must clean up any cache tag problems while
still at cache error exception level, but the actual correction
may be deferred.

     When we get a fatal error in an unsafe context (as in the
middle of the general exception handler), we cannot reasonably
reenter the general kernel.  In such a case, we really need to
simply reset and reboot the machine.  I propose we do just that,
after storing a small amount of error status in NVRAM.  We can
then check for any such status on the next reboot, and, if present,
log the data and flag it as having been logged in NVRAM.  In the
current handler, a number of cases wind up producing an exception
loop, which tells the user nothing about what is wrong.

     When we get a fatal error in a safe context, we can simply
convert our context to a general exception context, and panic.
It is important to switch to a general exception context, so that
we can detect exception loops and handle the error as above.

     The implication of the above is that kevinm's instruction
interpretation logic really needs to run at cache error exception
level, and avoid using general kernel routines and data structures.

Data structures:

1.  First level save area, just below 0x1000
	-- saves a few registers, plus $errorepc
	-- records cache error exception nesting level
	-- addressed as 0x1000 (KUSEG physical) or 0xa0001000 (K1SEG)
	   when ERL is set, as 0xa0001000 (K1SEG) otherwise
		-- address 0x1000 allows zero-relative addressing, so
		   that all registers may be saved

2.  Cache error exception stack area
	-- dedicated page or pages
	-- only accessed uncached, via K1SEG

3.  Cache error log area (ecc_info)
	-- uncached
	-- any given field owned for write either by exception handler
	   or by exception logger, never written by both
	-- ring of log entries, indexed by monotonically increaseing
	   fill and empty counters, both take modulo the ring size to
	   index the ring
		-- allows lock-free updates

**********************************************************************/
VECTOR(ecc_exception, M_EXCEPT)
EXL_EXPORT(locore_exl_10)

.extern	mem_parity_nofault

/*
 *	Save a few registers, including $errorepc and
 *	$cacheerr, in the first level save area, and
 *	increment the nesting level.  Early on, check a
 *	busy flag in the first level save area.  If it is
 *	on, go to the fatal error logic in step 8.
 *	Otherwise, mark the save area busy.
 */
	.set	noat
	sreg	k0,CACHE_ERR_EFRAME+(EF_K0)(zero)
	sreg	k1,CACHE_ERR_EFRAME+(EF_K1)(zero)
	lw	k0,CACHE_ERR_ECCFRAME+(ECCF_BUSY*SZREG)(zero)
	addu	k1,k0,1
	sw	k1,CACHE_ERR_ECCFRAME+(ECCF_BUSY*SZREG)(zero)
	bnez	k0,ecc_bad
/*
 *	check for nofault and simple memory error
 */
	.set	noreorder
	mfc0	k1,C0_CACHE_ERR
	li	k0,CACHERR_ER|CACHERR_ED|CACHERR_ET|CACHERR_EE
	.set	reorder
	and	k1,k0
	li	k0,CACHERR_ER|CACHERR_ED|CACHERR_EE
	bne	k0,k1,1f
	lw	k0,mem_parity_nofault
	blez	k0,1f
	subu	k1,k0,NF_NENTRIES
	bgez	k1,1f
	sll	k0,2
	la	k1,nofault_pc
	addu	k1,k0
	lw	k0,0(k1)
	lreg	k1,CACHE_ERR_EFRAME+(EF_K1)(zero)
	.set	noreorder
	mtc0	k0,C0_ERROR_EPC
	lreg	k0,CACHE_ERR_EFRAME+(EF_K0)(zero)
	.set	reorder
	sw	zero,CACHE_ERR_ECCFRAME+(ECCF_BUSY*4)(zero)
	b	ecc_do_eret
	
1:
	lw	k0,CACHE_ERR_ECCFRAME+(ECCF_SECOND_PHASE*SZREG)(zero)
	addu	k1,k0,1
	sw	k1,CACHE_ERR_ECCFRAME+(ECCF_SECOND_PHASE*SZREG)(zero)
	sreg	sp,CACHE_ERR_EFRAME+(EF_SP)(zero)
	bnez	k0,1f
	PTR_L	sp,CACHE_ERR_ECCFRAME+(ECCF_STACK_BASE*SZREG)(zero)
1:
/*
 *	Allocate a new exception frame, either relative
 *	to $sp, if the nesting level is above 1, or at the
 *	top of the exception stack, if the nesting level
 *	is 1.  Save all registers in the exception frame,
 *	including those the first level save area, and
 *	then clear the busy flag in the first level save
 *	area.
 */
	PTR_SUBU	sp,EF_EXTENDED_SIZE
	sreg	AT,EF_AT(sp)
	.set	at
	SAVEAREGS(sp)
	lreg	a0,CACHE_ERR_EFRAME+(EF_K0)(zero)
	lreg	a1,CACHE_ERR_EFRAME+(EF_K1)(zero)
	lreg	a2,CACHE_ERR_EFRAME+(EF_SP)(zero)
	sreg	a0,EF_K0(sp)
	sreg	a1,EF_K1(sp)
	sreg	a2,EF_SP(sp)
	.set	noreorder
	mfc0	a0,C0_SR
	DMFC0(a1,C0_ERROR_EPC)
	mfc0	a2,C0_CAUSE
	DMFC0(a3,C0_EPC)
	.set	reorder
	sreg	a0,EF_SR(sp)
	sreg	a1,EF_ERROR_EPC(sp)
	sreg	a2,EF_CAUSE(sp)
	sreg	a3,EF_EPC(sp)
	sreg	a3,EF_EXACT_EPC(sp)	
	.set	noreorder
	DMFC0(a0,C0_BADVADDR)
	DMFC0(a1,C0_TLBHI)
	DMFC0(a2,C0_TLBLO_0)
	DMFC0(a3,C0_TLBLO_1)
	.set reorder
	sreg	a0,EF_BADVADDR(sp)
	sreg	a1,EF_TLBHI(sp)
	sreg	a2,EF_TLBLO_0(sp)
	sreg	a3,EF_TLBLO_1(sp)
	.set	noreorder
	mfc0	a0,C0_TAGLO
	mfc0	a1,C0_TAGHI
	mfc0	a2,C0_CACHE_ERR
	mfc0	a3,C0_ECC
	.set reorder
	sreg	a0,EF_TAGLO(sp)
	sreg	a1,EF_TAGHI(sp)
	sreg	a2,EF_CACHE_ERR(sp)
	sreg	a3,EF_ECC(sp)
	SAVESREGS(sp)
	sreg	ra,EF_RA(sp)
	sreg	gp,EF_GP(sp)	# save previous gp and set kernel's
	LA	gp,_gp		# if ecc occurred in kernel gp'll be same
	move	k1,sp
	jal	tfi_save		# Save tregs, v0, v1, mdlo, mdhi

	sw	zero,CACHE_ERR_ECCFRAME+(ECCF_BUSY*SZREG)(zero)

/*
 *	Call the C-coded ecc_handler() to try recovering from the
 *	cache error.
 */

	move	a0,sp			# a0 = (extended) exception frame
	PTR_ADDU	a1,sp,EF_ECCFRAME
	lreg	a2,EF_CACHE_ERR(sp)
	sreg	a2,(ECCF_CACHE_ERR*SZREG)(a1)
	lreg	a3,EF_ERROR_EPC(sp)
	sreg	a3,(ECCF_ERROREPC*SZREG)(a1)
	sreg	zero,(ECCF_TAGLO*SZREG)(a1)
	sreg	zero,(ECCF_ECC*SZREG)(a1)
	sreg	zero,(ECCF_PADDR*SZREG)(a1)
	jal	ecc_handler

	# returns 0 if error was corrected -- resume execution
	# returns 1 if error was fatal -- panic now
	# returns -1 if error needs process-level recovery
	bgtz	v0,ecc_bad
	bltz	v0,ecc_send_exception

	# Error was corrected
	# restore state and return

	# 	restore special registers
	move	k1,sp
	jal	ecc_restore_specials

	.set reorder
	lreg	a0,EF_SR(sp)
	.set	noreorder
	mtc0	a0,C0_SR
	NOP_0_4
	NOP_0_1
	.set	reorder

	li	a0,1
	sw	a0,CACHE_ERR_ECCFRAME+(ECCF_BUSY*SZREG)(zero)
	lw	a0,CACHE_ERR_ECCFRAME+(ECCF_SECOND_PHASE*SZREG)(zero)
	subu	a0,1
	sw	a0,CACHE_ERR_ECCFRAME+(ECCF_SECOND_PHASE*SZREG)(zero)

	move	k1,sp
	jal	tfi_restore			# restore t regs
	RESTOREAREGS(k1)
	RESTORESREGS(k1)
	lreg	ra,EF_RA(k1)		# LDSLOT
	.set	noat
	lreg	AT,EF_AT(k1)
	.set	at
	lreg	sp,EF_SP(k1)
	lreg	gp,EF_GP(k1)		# restore previous gp
	lreg	k0,EF_K0(k1)
	lreg	k1,EF_K1(k1)
	sw	zero,CACHE_ERR_ECCFRAME+(ECCF_BUSY*SZREG)(zero)
	.set	noreorder
ecc_do_eret:
	NOP_0_4
	ERET_PATCH(locore_eret_8)	# patchable eret
	nop
	.set	reorder

/*
 *	At this point, we try to simulate a trap and let the process-level
 *	trap code deal with it, as by killing the process or re-reading a
 *	clean page.  We have to verify that we did not trap inside
 *	an exception handler.
 */
ecc_send_exception:
	lreg	t0,EF_SR(sp)		# in exception handler?
	and	t1,t0,SR_EXL
	bnez	t1,ecc_bad		# yes-->fatal error

	li	a0,1
	sw	a0,CACHE_ERR_ECCFRAME+(ECCF_BUSY*SZREG)(zero)
	lw	a0,CACHE_ERR_ECCFRAME+(ECCF_SECOND_PHASE*SZREG)(zero)
	subu	a0,1
	sw	a0,CACHE_ERR_ECCFRAME+(ECCF_SECOND_PHASE*SZREG)(zero)

	move	a0,sp
	jal	ecc_create_exception	# set up new exception frame

	#   	returns v0 == new stack frame
	#	first word of new stack frame contains new exception frame address

	# 	restore special registers
	move	k1,sp
	jal	ecc_restore_specials

	move	sp,v0			# get new SP
	sw	zero,CACHE_ERR_ECCFRAME+(ECCF_BUSY*SZREG)(zero)
	PTR_L	a0,0(sp)		# a1: exception frame
	move	s1,a0			# save new exception frame

	# simulate trap via new frame
	lreg	a1,EF_ERROR_EPC(k1)	# copy $errorepc to $epc
	sreg	a1,EF_EPC(s1)
	sreg	a1,EF_EXACT_EPC(s1)	
	lreg	s0,EF_SR(a0)		# get new SR
	and	s0,~SR_ERL
	or	s0,SR_EXL		# s0: original SR, with fake SR_EXL
	sreg	s0,EF_SR(a0)
	and	a2,s0,SR_KERN_SET	
	or	a2,SR_EXL|SR_ERL
	LA	a1,1f
	or	a1,SEXT_K1BASE		# stay in K1 space
	.set	noreorder
	DMTC0(a1,C0_ERROR_EPC)
	NOP_0_1
	mtc0	a2,C0_SR
	NOP_0_4
	NOP_0_1
	eret				# go to EXL level from ERL level
					# (just changing C0_SR results in
					# a cache error exception on R4000)
	nop				
1:
	nop
	.set	reorder
	/* switch to system CPU timer if curthreadp != NULL */
	PTR_L	a0,VPDA_CURKTHREAD(zero)
	li	a1,AS_SYS_RUN
	beq	a0,zero,1f
	jal	ktimer_switch
1:
	move	a0,s1			# a0: exception frame
	move	k1,a0			# k1: exception frame
	lreg	s0,EF_SR(a0)		# s0: original SR, with fake SR_EXL
	li	a1,SEXC_ECC_EXCEPTION	# a1: software exception code
	lreg	a3,EF_CAUSE(a0)		# a3: fake cause register value
	LA	a2,VEC_trap		# back to K0 space
	jr	a2
	
/*
 *	At this point, we cannot continue, and we can trust very little.
 *	We try to print some kind of message before giving up.
 */
ecc_bad:
	sw	sp,eccword		/* poke something for LAs if can't
					 * really print */

	lreg	a0,EF_CACHE_ERR(sp)
	lreg	a1,EF_ERROR_EPC(sp)
	jal	ecc_panic

	.data
EXPORT(eccword)
	.word	0

	.text
EXL_EXPORT(elocore_exl_10)
	END(ecc_exception)
#endif /* _MEM_PARITY_WAR */

	AUTO_CACHE_BARRIERS_ENABLE

#endif	/* R4000 || R10000 */



/*
 * *************************************************************************
 *
 *			New Cache Error Handler
 *
 * *************************************************************************
 */

#if defined (SN)
	LEAF(cache_error_exception)
        AUTO_CACHE_BARRIERS_DISABLE
        .set    noreorder
        NOP_0_4

        LI      k1, CACHE_ERR_EFRAME
        SAVESREGS(k1)
        SAVEAREGS(k1)

        sreg    gp,EF_GP(k1)
        sreg    sp,EF_SP(k1)
        sreg    ra,EF_RA(k1)
        LA      gp,_gp          # if ecc occurred in kernel gp'll be same

        jal     tfi_save	# prepare to call a C funtion
        nop

        MFC0(k0,C0_SR)
        sreg    k0,EF_SR(k1)
        MFC0(k0, C0_CAUSE)
        sreg    k0, EF_CAUSE(k1)
        DMFC0(k0,C0_BADVADDR)
        sreg    k0,EF_BADVADDR(k1)
        DMFC0(k0, C0_EPC)
        sreg    k0,EF_EPC(k1)
        sreg    k0,EF_EXACT_EPC(k1)

        move    s0,k1                   /* Eframe pointer in safe place */
        PTR_ADDI s1,k1,EF_SIZE          /* ECCF frame - in safe place */

        /*
         * Save registers in the ECCF frame.
         */
        DMFC0(k0, C0_ERROR_EPC)
        sreg    k0,SN_ECCF_ERROREPC(s1)
        DMFC0(v0, C0_TAGHI)
        sw      v0,SN_ECCF_TAGHI(s1)
        DMFC0(v0, C0_TAGLO)
        sw      v0,SN_ECCF_TAGLO(s1)
        DMFC0(v0, C0_CACHE_ERR)
        sw      v0,SN_ECCF_CACHE_ERR(s1)

        /*
         * Provision for stack expansion. If the stack space is deemed
         * insufficient, store an address at CACHE_ERR_SP_PTR. Note, for
         * SN0 this is per processor, since we are looking at the
         * ualias space.
         */
        PTR_L   sp,CACHE_ERR_SP_PTR
        bne     sp, zero, 1f
        nop                             # BD slot (noreorder set)
        li      sp, CACHE_ERR_SP
1:



        move    a0,s0                   /* Eframe pointer */
        jal     cache_error_exception_handler
        move    a1,s1                   /* BDSLOT: ECCF pointer */

        /*
         * If it returns, we attempt to recover. Restore CP0 registers
         * from ECCF frame.
         */
        lw      k1,ECCF_TAGHI(s1)
        MTC0(k1, C0_TAGHI)
        lw      k1,ECCF_TAGLO(s1)
        MTC0(k1, C0_TAGLO)
        jal     tfi_restore
        move    k1,s0                   /* DELAY: EFRAME */

        /*
         * reload the error epc from the ecc eframe and store it into
         * the error_epc register
         */
        lreg    k0,SN_ECCF_ERROREPC(s1)
        DMTC0(k0, C0_ERROR_EPC)

        move    k0,s1                   /* ECCF frame */
        RESTOREAREGS(k1)
        RESTORESREGS(k1)
        lreg    ra,EF_RA(k1)
        .set    noat
        lreg    AT,EF_AT(k1)
        lreg    sp,EF_SP(k1)
        lreg    gp,EF_GP(k1)


        lreg    k0,EF_K0(k1)
        lreg    k1,EF_K1(k1)

        ERET_PATCH(locore_eret_8)       # patchable eret
        .set    at
        .set    reorder

	.set   	reorder
	END(cache_error_exception)



#endif



