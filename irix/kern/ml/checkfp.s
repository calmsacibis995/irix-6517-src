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

#ident	"$Revision: 1.19 $"

#include "ml/ml.h"


#ifdef USE_PTHREAD_RSA
#define RSA_SAVECP1REG(reg)	swc1	$f/**/reg,RSA_FPREGS+4+reg*8(a1)
#define RSA_RESTCP1REG(reg)	lwc1	$f/**/reg,RSA_FPREGS+4+reg*8(a1)

#define EXREG_RSA_SAVECP1REG(reg)	sdc1	$f/**/reg,RSA_FPREGS+reg*8(a1)
#define EXREG_RSA_RESTCP1REG(reg)	ldc1	$f/**/reg,RSA_FPREGS+reg*8(a1)

	
/*
 * checkfp_rsa(ut, rsa )
 *	ut = uthread pointer for process forking or exiting.
 *	rsa = pointer to RSA save area
 *
 *	Called to release FP ownership by saving any FP state.
 *	By definition, if FPOWNER is us (ut) then we must also think
 *	that we own the FP for the current cpu.
 *
 *	Always called at splhi() from swtch().
 *	RETURNS zero if no registers unloaded.	
 * NOTE: This routine differs from checkfp() in that it is only invoked from
 *	 swtch() when leaving a nanothread-ed process and it saves the FP
 *	 state directly in the RSA instead of the PCB, avoiding a copy.
 */
LEAF(checkfp_rsa)

	PTR_L	v0,VPDA_FPOWNER(zero)
	beq	v0,zero,2f		# no one owns it!
	bne	a0,v0,2f		# not owned by us, just return

	# Test if the current fpu owner has extended fpregs enabled.
	# The modes which support this are ABI_IRIX5_64 and ABI_IRIX5_N32
	
	PTR_L	t0,UT_PPROXY(a0)	# uthread -> proxy structure
	
	lb	t0,PRXY_ABI(t0)
	and	t0,ABI_IRIX5_64 | ABI_IRIX5_N32
	beq	t0,zero,1f
	li	t0,SR_FR		# Remember test result in t0
1:
	/*
	 * The floating-point control and status register must be
	 * read first so to stop the floating-point coprocessor.
	 */
	.set 	noreorder
	MFC0(v1,C0_SR)			# enable coproc 1 for the kernel
	NOP_1_0
#if TFP
	/* CAUTION: Do NOT clear SR_DM, since changing the DM flag changes
	 * the bits in the C0_SR which generate an FP interrupt.  An
	 * FP interrupt here will cause us to improperly save the state
	 * of the fpc_csr (since an FP interrupt will unload the FP and zero
	 * the fpc_csr and then we will read a zero fpc_csr here).
	 */
	and	v1,~(SR_FR)		# Clear FR
#endif
	or	v0,v1,SR_CU1		
	or	v0,t0			# Possibly enable extended fp regs.
	MTC0(v0,C0_SR)
	NOP_1_3				# before cp1 is usable
	NOP_1_0

	AUTO_CACHE_BARRIERS_DISABLE	# MTC0 above serializes and makes a1 ok
	cfc1	v0,fpc_csr
	NOP_1_3				# no interlocks on CP1 regs
	sw	v0,RSA_FPC_CSR(a1)

	.set	reorder
	bne	t0,zero,checkfp_rsa_exreg_save

	RSA_SAVECP1REG(31); RSA_SAVECP1REG(30)
	RSA_SAVECP1REG(29); RSA_SAVECP1REG(28)
	RSA_SAVECP1REG(27); RSA_SAVECP1REG(26)
	RSA_SAVECP1REG(25); RSA_SAVECP1REG(24)
	RSA_SAVECP1REG(23); RSA_SAVECP1REG(22)
	RSA_SAVECP1REG(21); RSA_SAVECP1REG(20)
	RSA_SAVECP1REG(19); RSA_SAVECP1REG(18)
	RSA_SAVECP1REG(17); RSA_SAVECP1REG(16)
	RSA_SAVECP1REG(15); RSA_SAVECP1REG(14)
	RSA_SAVECP1REG(13); RSA_SAVECP1REG(12)
	RSA_SAVECP1REG(11); RSA_SAVECP1REG(10)
	RSA_SAVECP1REG(9);  RSA_SAVECP1REG(8)
	RSA_SAVECP1REG(7);  RSA_SAVECP1REG(6)
#if !TFP
	/* TFP saves this register in SAVEFPREGS */
	RSA_SAVECP1REG(5); 	RSA_SAVECP1REG(4)
#else
	/* The sdc1 which saved $f4 will also save $f5 if we're in 32-bit
	 * FP mode (FR bit clear in C0_SR), so we move the value from
	 * the 64-bit $f4 slot into the 32-bit $f5 location in the PCB.
	 */
	PTR_L	a3,UT_EXCEPTION(a0)	# uthread -> exception structure
	lw	t0,PCB_FPREGS+4*8(a3)	# $f5 is in high word of $f4 in PCB
	sw	t0,RSA_FPREGS+4+5*8(a1)	# move to low word of $f5 in RSA
	lw	t0,PCB_FPREGS+4+4*8(a3)	# $f4 is in low word of $f4 in PCB
	sw	t0,RSA_FPREGS+4+4*8(a1)	# move to low word of $f4 in RSA
#endif
	RSA_SAVECP1REG(3);  RSA_SAVECP1REG(2)
	RSA_SAVECP1REG(1);  RSA_SAVECP1REG(0)

	b	checkfp_rsa_save_done

checkfp_rsa_exreg_save:
	EXREG_RSA_SAVECP1REG(31); EXREG_RSA_SAVECP1REG(30)
	EXREG_RSA_SAVECP1REG(29); EXREG_RSA_SAVECP1REG(28)
	EXREG_RSA_SAVECP1REG(27); EXREG_RSA_SAVECP1REG(26)
	EXREG_RSA_SAVECP1REG(25); EXREG_RSA_SAVECP1REG(24)
	EXREG_RSA_SAVECP1REG(23); EXREG_RSA_SAVECP1REG(22)
	EXREG_RSA_SAVECP1REG(21); EXREG_RSA_SAVECP1REG(20)
	EXREG_RSA_SAVECP1REG(19); EXREG_RSA_SAVECP1REG(18)
	EXREG_RSA_SAVECP1REG(17); EXREG_RSA_SAVECP1REG(16)
	EXREG_RSA_SAVECP1REG(15); EXREG_RSA_SAVECP1REG(14)
	EXREG_RSA_SAVECP1REG(13); EXREG_RSA_SAVECP1REG(12)
	EXREG_RSA_SAVECP1REG(11); EXREG_RSA_SAVECP1REG(10)
	EXREG_RSA_SAVECP1REG(9);  EXREG_RSA_SAVECP1REG(8)
	EXREG_RSA_SAVECP1REG(7);  EXREG_RSA_SAVECP1REG(6)
	EXREG_RSA_SAVECP1REG(5)
#if !TFP
	/* TFP saves this register in SAVEFPREGS */
	EXREG_RSA_SAVECP1REG(4)
#else	
	PTR_L	a3,UT_EXCEPTION(a0)	# uthread -> exception structure
	ld	t0,PCB_FPREGS+4*8(a3)	# move $f4 from PCB 
	sd	t0,RSA_FPREGS+4+5*8(a1)	#	to RSA
#endif
	EXREG_RSA_SAVECP1REG(3);  EXREG_RSA_SAVECP1REG(2)
	EXREG_RSA_SAVECP1REG(1);  EXREG_RSA_SAVECP1REG(0)
	AUTO_CACHE_BARRIERS_ENABLE	# done with a1 stores
checkfp_rsa_save_done:

	.set	noreorder
	ctc1	zero,fpc_csr		# clear any pending interrupts
	MTC0(v1,C0_SR)			# disable kernel fp access
	NOP_0_4

	.set	reorder
	PTR_S	zero,VPDA_FPOWNER(zero)	# Mark FP as unowned
	/*
	 * if we're the current process, turn off FP usable bit
	 */
	PTR_L	t1,VPDA_CURUTHREAD(zero)
	beq	t1,zero,2f		# no current process thread
	PTR_L	t1,UT_EXCEPTION(t1)
	PTR_ADDIU t1,U_EFRAME		# t1 = curuthread->ut_exception->u_eframe
	lreg	a1,EF_SR(t1)
	and	a1,~SR_CU1		# clear fp coprocessor usable bit
	sreg	a1,EF_SR(t1)
	li	v0, 1			/* indicate that FP is now in RSA */
	j	ra
2:
	move	v0,zero
	j	ra
	END(checkfp_rsa)

#endif /* USE_PTHREAD_RSA */
/*
 * checkfp(ut, exiting)
 *	ut = uthread pointer for process forking or exiting.
 *	exiting = 1 if exiting.
 *	exiting = 2 if process has been signaled. See below.
 *	Called to release FP ownership by saving any FP state.
 *	By definition, if FPOWNER is us (ut) then we must also think
 *	that we own the FP for the current cpu.
 */
LEAF(checkfp)
#ifndef IP26
	/* Raise splhi, so we don't get preempted in the middle
	 * of checkfp, which could possibly cause us to lose the
	 * fp unit.
	 */
	PTR_ADDIU sp,sp,-48		# At least big enough.
	REG_S	ra,32(sp)
	REG_S	a0,16(sp)		# If splmetering is on, a0/a1 get
	REG_S	a1,24(sp)		# clobbered
	jal	splhi
	REG_S	v0,8(sp)
	REG_L	a0,16(sp)
	REG_L	a1,24(sp)
#endif	/* !IP26 */
	PTR_L	v0,VPDA_FPOWNER(zero)
	beq	v0,zero,2f		# no one owns it!
	bne	a0,v0,2f		# not owned by us, just return
	beq	a1,1,is_exit		# exiting, don't save state

	PTR_L	t0,UT_PPROXY(a0)	# uthread -> proxy structure
	PTR_L	a3,UT_EXCEPTION(a0)	# uthread -> exception structure

	# Test if the current fpu owner has extended fpregs enabled.
	# The modes which support this are ABI_IRIX5_64 and ABI_IRIX5_N32
	lb	t0,PRXY_ABI(t0)
	and	t0,ABI_IRIX5_64 | ABI_IRIX5_N32
	beq	t0,zero,1f
	li	t0,SR_FR		# Remember test result in t0
1:
	/*
	 * The floating-point control and status register must be
	 * read first so to stop the floating-point coprocessor.
	 */
	.set 	noreorder
	MFC0(v1,C0_SR)			# enable coproc 1 for the kernel
	NOP_1_0
#if TFP
	/* CAUTION: Do NOT clear SR_DM, since changing the DM flag changes
	 * the bits in the C0_SR which generate an FP interrupt.  An
	 * FP interrupt here will cause us to improperly save the state
	 * of the fpc_csr (since an FP interrupt will unload the FP and zero
	 * the fpc_csr and then we will read a zero fpc_csr here).
	 */
	and	v1,~(SR_FR)		# Clear FR
#endif
	or	v0,v1,SR_CU1		
	or	v0,t0			# Possibly enable extended fp regs.
	MTC0(v0,C0_SR)
	NOP_1_3				# before cp1 is usable
	NOP_1_0

	# If softfp returns non-zero to fp_intr, then fp_intr calls
	# checkfp with an argument of 2. This indicates to checkfp
	# that the fpc_csr was already stored into the PCB rather
	# than softfp stuffing it back into the fpu. The
	# reason this is done is that on the R4000, floating point
	# exceptions are exceptions, not interrupts, and therefore
	# not maskable. Putting the updated csr back into the fpu
	# would cause an immediate exception.
	beq	a1,2,3f
	nop
	AUTO_CACHE_BARRIERS_DISABLE	# MTC0 above serializes and makes a3 ok
	cfc1	v0,fpc_csr
	NOP_1_3				# no interlocks on CP1 regs
	sw	v0,PCB_FPC_CSR(a3)
3:
	.set	reorder
	bne	t0,zero,checkfp_exreg_save

	SAVECP1REG(31); SAVECP1REG(30); SAVECP1REG(29); SAVECP1REG(28)
	SAVECP1REG(27); SAVECP1REG(26); SAVECP1REG(25); SAVECP1REG(24)
	SAVECP1REG(23); SAVECP1REG(22); SAVECP1REG(21); SAVECP1REG(20)
	SAVECP1REG(19); SAVECP1REG(18); SAVECP1REG(17); SAVECP1REG(16)
	SAVECP1REG(15); SAVECP1REG(14); SAVECP1REG(13); SAVECP1REG(12)
	SAVECP1REG(11); SAVECP1REG(10); SAVECP1REG(9);  SAVECP1REG(8)
	SAVECP1REG(7);  SAVECP1REG(6)
#if !TFP
	/* TFP saves this register in SAVEFPREGS */
	SAVECP1REG(5); 	SAVECP1REG(4)
#else
	/* The sdc1 which saved $f4 will also save $f5 if we're in 32-bit
	 * FP mode (FR bit clear in C0_SR), so we move the value from
	 * the 64-bit $f4 slot into the 32-bit $f5 location in the PCB.
	 */
	lw	t0,PCB_FPREGS+4*8(a3)	# $f5 is in high word of $f4 in PCB
	sw	t0,PCB_FPREGS+4+5*8(a3)	# move to low word of $f5 in PCB
#endif
	SAVECP1REG(3);  SAVECP1REG(2);  SAVECP1REG(1);  SAVECP1REG(0)

	b	checkfp_save_done

checkfp_exreg_save:
	EXREG_SAVECP1REG(31); EXREG_SAVECP1REG(30)
	EXREG_SAVECP1REG(29); EXREG_SAVECP1REG(28)
	EXREG_SAVECP1REG(27); EXREG_SAVECP1REG(26)
	EXREG_SAVECP1REG(25); EXREG_SAVECP1REG(24)
	EXREG_SAVECP1REG(23); EXREG_SAVECP1REG(22)
	EXREG_SAVECP1REG(21); EXREG_SAVECP1REG(20)
	EXREG_SAVECP1REG(19); EXREG_SAVECP1REG(18)
	EXREG_SAVECP1REG(17); EXREG_SAVECP1REG(16)
	EXREG_SAVECP1REG(15); EXREG_SAVECP1REG(14)
	EXREG_SAVECP1REG(13); EXREG_SAVECP1REG(12)
	EXREG_SAVECP1REG(11); EXREG_SAVECP1REG(10)
	EXREG_SAVECP1REG(9);  EXREG_SAVECP1REG(8)
	EXREG_SAVECP1REG(7);  EXREG_SAVECP1REG(6)
	EXREG_SAVECP1REG(5)
#if !TFP
	/* TFP saves this register in SAVEFPREGS */
	EXREG_SAVECP1REG(4)
#endif
	EXREG_SAVECP1REG(3);  EXREG_SAVECP1REG(2)
	EXREG_SAVECP1REG(1);  EXREG_SAVECP1REG(0)
	AUTO_CACHE_BARRIERS_ENABLE	# done with a3 stores
checkfp_save_done:

	.set	noreorder
	ctc1	zero,fpc_csr		# clear any pending interrupts
	MTC0(v1,C0_SR)			# disable kernel fp access
	NOP_0_4
#if TFP
	b	is_exit2		# don't zero fpc_csr twice
	nop
#endif
	.set 	reorder

is_exit:
#if TFP
	/*
	 * Need to clear fpc_csr here to avoid stray fp interrupts when
	 * a performance mode process exits.
	 */
	.set	noreorder
	MFC0(v1,C0_SR)			# enable coproc 1 for the kernel
	or	v0,v1,SR_CU1		
	MTC0(v0,C0_SR)
	ctc1	zero,fpc_csr		# clear any pending interrupts
	MTC0(v1,C0_SR)			# disable kernel fp access
	.set	reorder
is_exit2:
#endif
	PTR_S	zero,VPDA_FPOWNER(zero)	# Mark FP as unowned
	/*
	 * if we're the current process, turn off FP usable bit
	 */
	PTR_L	t1,VPDA_CURUTHREAD(zero)
	beq	t1,zero,2f		# no current process thread
	PTR_L	t1,UT_EXCEPTION(t1)
	PTR_ADDIU t1,U_EFRAME		# t1 = curuthread->ut_exception->u_eframe
	lreg	a1,EF_SR(t1)
	and	a1,~SR_CU1		# clear fp coprocessor usable bit
	sreg	a1,EF_SR(t1)
2:
#ifndef IP26
	/* restore spl */
	REG_L	a0,8(sp)
	jal	splx
	REG_L	ra,32(sp)
	PTR_ADDIU sp,48	
#endif	/* !IP26 */
	j	ra
	END(checkfp)

#if TFP_MOVC_CPFAULT_FAST_WAR || _SHAREII

LEAF(restorefp)
#if TFP
	/* On TFP we really need to zero the fpc_csr before possibly
	 * modifying the state of the DM field, since modifying DM
	 * causes different bits in the fpc_csr to cause an interrupt.
	 * fpc_csr is probably zero at this point anyway since the FP
	 * is unloaded, but let's be safe (besides subsequent code in
	 * this procedure zeros the fpc_csr so maybe it is necessary).
	 */
	ctc1	zero,fpc_csr
#endif /* TFP */		
	.set 	noreorder
	MFC0(t0, C0_SR)
	or	a3,t0,SR_CU1
	and	a3,~(SR_FR|SR_DM)
	or	a3,a0
	MTC0(a3,C0_SR)

	PTR_L	a3,VPDA_CURUTHREAD(zero)
	PTR_L	a3,UT_EXCEPTION(a3)
	PTR_ADDIU a3,U_PCB
	bnez	a0,1f		# 64 bit process
	nop

	RESTCP1REG(0);  RESTCP1REG(1);  RESTCP1REG(2);  RESTCP1REG(3)
#if !TFP
	/* TFP restores in RESTOREFPREGS_TOUSER or RESTOREFPREGS_TOKERNEL */
	RESTCP1REG(4); 	RESTCP1REG(5)
#else
	/* The ldc1 which restores $f4 will end up restoring $f5 if we're
	 * in 32-bit Fp mode (FR bit clear in C0_SR), so move 32-bit $f5
	 * to the correct location for that ldc1 to restore the correct value.
	 */
	lw	a2,PCB_FPREGS+4+5*8(a3)	# move low word from $f5 in PCB
	sw	a2,PCB_FPREGS+4*8(a3)	# to high word of $f4 in PCB	
#endif
					RESTCP1REG(6);  RESTCP1REG(7)
	RESTCP1REG(8);  RESTCP1REG(9);  RESTCP1REG(10); RESTCP1REG(11)
	RESTCP1REG(12); RESTCP1REG(13); RESTCP1REG(14); RESTCP1REG(15) 
	RESTCP1REG(16); RESTCP1REG(17); RESTCP1REG(18); RESTCP1REG(19)
	RESTCP1REG(20); RESTCP1REG(21); RESTCP1REG(22); RESTCP1REG(23)
	RESTCP1REG(24); RESTCP1REG(25); RESTCP1REG(26); RESTCP1REG(27)
	RESTCP1REG(28); RESTCP1REG(29); RESTCP1REG(30); RESTCP1REG(31)
	b	2f
	nop

1:	EXREG_RESTCP1REG(0);  EXREG_RESTCP1REG(1)
	EXREG_RESTCP1REG(2);  EXREG_RESTCP1REG(3)
#if !TFP
	/* TFP restores in RESTOREFPREGS_TOUSER or RESTOREFPREGS_TOKERNEL */
	EXREG_RESTCP1REG(4)
#endif
	EXREG_RESTCP1REG(5)
	EXREG_RESTCP1REG(6);  EXREG_RESTCP1REG(7)
	EXREG_RESTCP1REG(8);  EXREG_RESTCP1REG(9)
	EXREG_RESTCP1REG(10); EXREG_RESTCP1REG(11)
	EXREG_RESTCP1REG(12); EXREG_RESTCP1REG(13)
	EXREG_RESTCP1REG(14); EXREG_RESTCP1REG(15)
	EXREG_RESTCP1REG(16); EXREG_RESTCP1REG(17)
	EXREG_RESTCP1REG(18); EXREG_RESTCP1REG(19)
	EXREG_RESTCP1REG(20); EXREG_RESTCP1REG(21)
	EXREG_RESTCP1REG(22); EXREG_RESTCP1REG(23)
	EXREG_RESTCP1REG(24); EXREG_RESTCP1REG(25)
	EXREG_RESTCP1REG(26); EXREG_RESTCP1REG(27)
	EXREG_RESTCP1REG(28); EXREG_RESTCP1REG(29)
	EXREG_RESTCP1REG(30); EXREG_RESTCP1REG(31)

2:	ctc1	zero,fpc_csr
	lw	a2,PCB_FPC_CSR(a3)

#if TFP
	PTR_L   a3,VPDA_CURUTHREAD(zero)
	PTR_L	a3,UT_PPROXY(a3)
	lbu	a3,PRXY_FPFLAGS(a3)
	andi	a3,P_FP_IMPRECISE_EXCP		# see if we're in precise mode
        beqz    a3,1f                           # if so, use r4k code
        nop
	#
        # we're in performance mode, so we need to use flags bits not
        # cause bits (because flag bits generate interrupts in performance
        # mode). We can't turn off the flags bits since they're supposed
	# to be sticky, so we'll turn off the enable bits. Any ptogram
	# running in performance mode that takes an fp interrupt will have
	# to re-enable the bits in the signal handler.
	#
        and     a1,a2,CSR_ENABLE                # isolate the Enable bits
        or      a1,(UNIMP_EXC >> 5)             # fake an enable for Unimple.
        srl     a1,5                            # and align with flag bits
        and     a3,a2,CSR_FLAGS			# isolate the flag bits
        and     a3,a1                           # isolate the matching bits
	sll	a3,5				# shift left onto enables
        xor     a2,a3                           # and turn them off so we
                                                # don't cause fp exceptions.
	b	2f
	nop

1:
#endif	/* TFP */
	# When a process takes a fp exception, we store the cause
	# bits directly into the pcb, before signaling the process.
	# If the process is continued, we have to make sure that
	# any enabled cause bits are masked off.
	and	a1,a2,CSR_ENABLE		# isolate the Enable bits
	or	a1,(UNIMP_EXC >> 5)		# fake an enable for Unimple.
	sll	a1,5				# and align with Cause bits
	and	a3,a2,CSR_EXCEPT		# isolate the Cause bits
	and	a3,a1				# isolate the matching bits
	xor	a2,a3				# and turn them off so we
						# don't cause fp exceptions.
2:
	/*
	 * Restoring fpc_csr may generate an fp interrupt. Make us
	 * the fp owner now so that it doesn't look like a stray fp
	 * interrupt.
	 */
	PTR_L	a3,VPDA_CURUTHREAD(zero)
	PTR_S	a3,VPDA_FPOWNER(zero)		# we now own fp

	ctc1	a2,fpc_csr
	MTC0(t0,C0_SR)				# restore sr
	j	ra
	nop
	.set	reorder
	END(restorefp)

#endif /* TFP_MOVC_CPFAULT_FAST_WAR || _SHAREII */
