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

#ident	"$Revision: 1.16 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

/*
 * Coprocessor unusable fault
 * we can use a1 since if we call soft_trap we reload it
 * don't touch k1, a0, s0 however
 * Entry:
 *	k1 - exception pointer
 *	a0 - exception pointer
 *	a1 - exception code
 *	a3 - cause register
 *	s0 - sr at time of exception
 */

VECTOR(VEC_cpfault, M_EXCEPT)
EXL_EXPORT(locore_exl_7)
	/*
	 * Don't enable interrupts yet to avoid races with pullfp
	 * cpuactions.
	 * Also so we can use k1
	 */
	# Save k1 here for later restoration.
	move	s5,k1

	and	a1,s0,SR_PREVMODE
	beq	a1,zero,coproc_panic	# kernel tried to use coprocessor

	and	a1,a3,CAUSE_CEMASK
	srl	a1,CAUSE_CESHIFT
	bne	a1,1,coproc_not1	# not coproc 1

	PTR_L	a1,VPDA_CURUTHREAD(zero)# current uthread executing
	PTR_L	t0,UT_EXCEPTION(a1)
	PTR_ADDIU t0,U_PCB
	sw	gp,PCB_OWNEDFP(t0)	# mark that fp has been touched
	or	a2,s0,SR_CU1		# enable coproc 1 for the user process
	sreg	a2,EF_SR(k1)

	PTR_L	a2,VPDA_FPOWNER(zero)	# current coproc 1 (fp) owner
	beq	a2,a1,coproc_done	# owned by the current user thread

	# Test if the current fpu owner has extended fpregs enabled.
	# The modes which support this are ABI_IRIX5_64 and ABI_IRIX5_N32
	# Test for null fpowner first -- XXX but how could it be null?
	# XXX We've already dereferenced uthread pointer, and then tested
	# XXX that it doesn't equal a2

	beq	a2,zero,fp_notowned
	PTR_L	t0,UT_PPROXY(a2)	# uthread -> proxy structure
	lb	t0,PRXY_ABI(t0)
	and	t0,ABI_IRIX5_64 | ABI_IRIX5_N32
	beq	t0,zero,1f
	li	t0,SR_FR		# Remember test result in t0
1:

#if TFP
	# put the processor into kernel mode, lower EXL, disable
	# interrupts, and enable coproc 1
	and	t3,s0,SR_IMASK|SR_KERN_USRKEEP
	and	t3,~SR_FR			# explicitly turn off FR
	or	t3,SR_CU1|SR_KERN_SET
#endif
#if R4000 || R10000
	# put the processor into kernel mode, lower EXL, disable
	# interrupts, and enable coproc 1
	and	t3,s0,SR_IMASK|SR_KERN_SET|SR_DEFAULT
	or	t3,SR_CU1
#endif
	or	t3,t0			# Possibly enable extended fp regs.

	.set 	noreorder
	MTC0(t3,C0_SR)
EXL_EXPORT(elocore_exl_7)
	NOP_1_2				# before cp1 is usable
	.set reorder

	/*
	 * Owned by someone other than the current process.
	 * Save state (into the fpowner) before taking possession.
	 * a2 has owning uthread pointer
	 */
	PTR_L	a3,UT_EXCEPTION(a2)

	/*
	 * The floating-point control and status register must be
	 * read first to force all fp operations to complete and insure
	 * that all fp interrupts for this process have been delivered
	 */
	.set	noreorder
	cfc1	a1,fpc_csr
	NOP_1_3				# no interlocks on CP1 regs
	sw	a1,PCB_FPC_CSR(a3)
#if TFP
	/* NOTE: Interrupts MUST be disabled at this point since changing
	 * the state of the DM bit can cause an extraneous FP interrupt
	 * since in one case (DM=1) the CAUSE bits  cause the interrupt
	 * and in the other case (DM=0) the FLAGS bits generate it.
	 */
	and	t3,~SR_DM		# Now that we've read and saved
	MTC0(t3,C0_SR)			# the csr, it's safe to clear DM.
#endif

	bne	t0,zero,exreg_save
	nop
	AUTO_CACHE_BARRIERS_DISABLE	# a3 is dependent on ld of UT_EXCEPTION
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
	AUTO_CACHE_BARRIERS_ENABLE
	b	fp_notowned
	nop				# BDSLOT

exreg_save:
	AUTO_CACHE_BARRIERS_DISABLE	# a3 is dependent on ld of UT_EXCEPTION
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
	AUTO_CACHE_BARRIERS_ENABLE

fp_notowned:
EXL_EXPORT(locore_exl_21)
	/*
	 * restore coprocessor state (from the current process)
	 */
	PTR_L	t0,VPDA_CURUTHREAD(zero)	# Current uthread
	PTR_L	a3,UT_EXCEPTION(t0)

	# Test if the new fpu owner has extended fpregs enabled.
	# The modes which support this are ABI_IRIX5_64 and ABI_IRIX5_N32
	PTR_L	t0,UT_PPROXY(t0)	# uthread -> proxy structure
	lb	t0,PRXY_ABI(t0)
	PTR_ADDIU a3, U_PCB
	and	t0,ABI_IRIX5_64 | ABI_IRIX5_N32
	beq	t0,zero,1f
	nop				# BDSLOT
	li	t0,SR_FR		# Remember test result in t0
1:
#if R4000 || R10000
	and	a2,s0,SR_IMASK|SR_KERN_SET
#endif
#if TFP
	and	a2,s0,SR_IMASK|SR_KX|SR_KERN_USRKEEP
	or	a2,SR_KERN_SET
	and	a2,~(SR_FR|SR_DM)	# Clear FR and DM bits
#endif
	or	a2,SR_CU1
	or	a2,t0			# Possibly enable extended fp regs.
	MTC0(a2,C0_SR)
	NOP_1_2				# before cp1 is usable
	bne	t0,zero,exreg_restore
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
	lw	t0,PCB_FPREGS+4+5*8(a3)	# move low word from $f5 in PCB
	sw	t0,PCB_FPREGS+4*8(a3)	# to high word of $f4 in PCB	
#endif
					RESTCP1REG(6);  RESTCP1REG(7)
	RESTCP1REG(8);  RESTCP1REG(9);  RESTCP1REG(10); RESTCP1REG(11)
	RESTCP1REG(12); RESTCP1REG(13); RESTCP1REG(14); RESTCP1REG(15) 
	RESTCP1REG(16); RESTCP1REG(17); RESTCP1REG(18); RESTCP1REG(19)
	RESTCP1REG(20); RESTCP1REG(21); RESTCP1REG(22); RESTCP1REG(23)
	RESTCP1REG(24); RESTCP1REG(25); RESTCP1REG(26); RESTCP1REG(27)
	RESTCP1REG(28); RESTCP1REG(29); RESTCP1REG(30); RESTCP1REG(31)

	b	restore_done
	nop

exreg_restore:
	EXREG_RESTCP1REG(0);  EXREG_RESTCP1REG(1)
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
restore_done:

	ctc1	zero,fpc_csr
	lw	a2,PCB_FPC_CSR(a3)
	nop
#if TFP
	PTR_L   a3,VPDA_CURUTHREAD(zero)
	PTR_L	a3,UT_PPROXY(a3)
	lbu	a3,PRXY_FPFLAGS(a3)
	andi	a3,P_FP_IMPRECISE_EXCP		# see if we're in precise mode
        beqz    a3,1f                           # if so, use r4k code
        nop

        # we're in performance mode, so we need to use flags bits not
        # cause bits (because flag bits generate interrupts in performance
        # mode). We can't turn off the flags bits since they're supposed
	# to be sticky, so we'll turn off the enable bits. Any ptogram
	# running in performance mode that takes an fp interrupt will have
	# to re-enable the bits in the signal handler.

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
#endif

1:
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

	ctc1	a2,fpc_csr
	PTR_L	a3,VPDA_CURUTHREAD(zero)
	nop				# LDSLOT
	PTR_S	a3,VPDA_FPOWNER(zero)	# we now own fp
#if R4000 || R10000
	and	a2,s0,SR_IMASK|SR_KERN_SET	# retain kernel mode, low EXL,
	MTC0(a2,C0_SR)			# disable interrupt and clear SR_CU1
EXL_EXPORT(elocore_exl_21)
	NOP_0_4
#endif
#if TFP
	and	a2,s0,SR_IMASK|SR_KX|SR_KERN_USRKEEP # retain kernel mode, low EXL,
	or	a2,SR_KERN_SET
	DMTC0(a2,C0_SR)			# disable interrupt and clear SR_CU1
#endif
	.set	reorder
	move	k1,s5
	j	exception_exit

EXL_EXPORT(locore_exl_8)
coproc_done:
	j	exception_exit

coproc_not1:
	li	a1,SEXC_CPU		# handle as software trap
	j	VEC_trap

coproc_panic:
	lreg	a2,EF_EPC(k1)
	move	a3, k1
	PANIC("kernel used coprocessor. PC=0x%x ep:0x%x")
EXL_EXPORT(elocore_exl_8)
	END(VEC_cpfault)
