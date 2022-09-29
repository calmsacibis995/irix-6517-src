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

#include <ml/ml.h>

/*
 * tfi_save -- save enough state so that C routines can be called
 *
 * R10000_SPECULATION_WAR:  $sp not valid on entry.  Called frequency, so we
 * must assume that it can be entered speculatively.
 */
LEAF(tfi_save)
	AUTO_CACHE_BARRIERS_DISABLE
	/* Note that for _MIPS_SIM_ABI64, we do not save tmp registers
	 * t4..t7. If the current process was a 64 bit process, or
	 * we are handling a kernel mode exception, then these are
	 * the correct registers. If we are coming from user mode,
	 * and the current process was a 32 bit process, then the user
	 * registers t0..t4, which correspond to the kernel register
	 * namespace a4..a7, have already been saved by SAVEAREGS.
	 *
	 * So proper operation of this code relies on SAVEAREGS being
	 * called first.
	 */
	ORD_CACHE_BARRIER_AT(EF_V0,k1)
	sreg	v0,EF_V0(k1)
	sreg	v1,EF_V1(k1)
	sreg	t0,EF_T0(k1)
	mflo	t0
	sreg	t1,EF_T1(k1)
	mfhi	t1
	sreg	t2,EF_T2(k1)
	sreg	t3,EF_T3(k1)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	sreg	t4,EF_T4(k1)
	sreg	t5,EF_T5(k1)
	sreg	t6,EF_T6(k1)
	sreg	t7,EF_T7(k1)
#endif
	sreg	t8,EF_T8(k1)
	sreg	t9,EF_T9(k1)
	sreg	fp,EF_FP(k1)
	sreg	t0,EF_MDLO(k1)
	sreg	t1,EF_MDHI(k1)
	j	ra
	AUTO_CACHE_BARRIERS_ENABLE
	END(tfi_save)

/*
 * tfi_restore -- restore state saved by tfi_save
 */
LEAF(tfi_restore)
	/* Note that, for _MIPS_SIM_ABI64, we do not restore tmp
	 * registers t4..t7. See comment in tfi_save for explanation.
	 */
	lreg	v0,EF_MDLO(k1)
	lreg	v1,EF_MDHI(k1)
	mtlo	v0
	mthi	v1
	lreg	v0,EF_V0(k1)
	lreg	v1,EF_V1(k1)
	lreg	t0,EF_T0(k1)
	lreg	t1,EF_T1(k1)
	lreg	t2,EF_T2(k1)
	lreg	t3,EF_T3(k1)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	lreg	t4,EF_T4(k1)
	lreg	t5,EF_T5(k1)
	lreg	t6,EF_T6(k1)
	lreg	t7,EF_T7(k1)
#endif
	lreg	t8,EF_T8(k1)
	lreg	t9,EF_T9(k1)
	lreg	fp,EF_FP(k1)
	j	ra
	END(tfi_restore)

/*
 * hack to get elocore_exl_11 & assfail to have different addresses
 */
LEAF(assbuffer)
	.set noreorder
	nop
	.set reorder
	END(assbuffer)

/*
 * assfail - save all registers in static assregs then call _assfail
 * NOTE - we still lose a0, a1, a2, pc BUT stack backtrace can get them
 * NOTE2 - we do not save a0, a1, a2 since we know they are
 *	simply the error, file, line - that way if someone wants to they
 *	can save a0-2 in _assregs and they won't be trashed (see invalid
 *	kpte)
 */
	.globl	_assregs
	.globl	_didass
	.globl	panic_cpu

NESTED(assfail, 0, zero)
#ifdef R10000_SPECULATION_WAR

	/* Do not want to assume $sp is good here.  That could be why we
	 * caught an ASSERTion.  Use an MFC0 to serialize execution.
	 */
	AUTO_CACHE_BARRIERS_DISABLE
	.set	noreorder
	MFC0(zero,C0_SR)
	.set	reorder
#endif
	# only save registers the first time (we'll lose v0 in a tight race)
	sreg	v0,_assregs+EF_V0

	lw	v0, panic_cpu
	bne	v0, -1, 1f		# some other cpu is already panicing 
	CPUID_L	v0,VPDA_CPUID(zero)  	# my cpuid
	sw	v0, panic_cpu		# set global panic_cpu

	sreg	gp,_assregs+EF_GP
	.set 	noreorder
	MFC0(v0,C0_SR)
	NOP_1_0
	.set 	reorder
	sreg	v0,_assregs+EF_SR
	/*sreg	AT,_assregs+EF_AT	hopeless */
	sreg	sp,_assregs+EF_SP
	sreg	v1,_assregs+EF_V1
	sreg	a3,_assregs+EF_A3
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	sreg	t0,_assregs+EF_T0
	sreg	t1,_assregs+EF_T1
	sreg	t2,_assregs+EF_T2
	sreg	t3,_assregs+EF_T3
	sreg	t4,_assregs+EF_T4
	sreg	t5,_assregs+EF_T5
	sreg	t6,_assregs+EF_T6
	sreg	t7,_assregs+EF_T7
#elif (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
	sreg	a4,_assregs+EF_A4
	sreg	a5,_assregs+EF_A5
	sreg	a6,_assregs+EF_A6
	sreg	a7,_assregs+EF_A7
	sreg	t0,_assregs+EF_T0
	sreg	t1,_assregs+EF_T1
	sreg	t2,_assregs+EF_T2
	sreg	t3,_assregs+EF_T3
#else
<<BOMB>>
#endif /* _MIPS_SIM */
	sreg	s0,_assregs+EF_S0
	sreg	s1,_assregs+EF_S1
	sreg	s2,_assregs+EF_S2
	sreg	s3,_assregs+EF_S3
	sreg	s4,_assregs+EF_S4
	sreg	s5,_assregs+EF_S5
	sreg	s6,_assregs+EF_S6
	sreg	s7,_assregs+EF_S7
	sreg	t8,_assregs+EF_T8
	sreg	t9,_assregs+EF_T9
	sreg	k0,_assregs+EF_K0
	sreg	k1,_assregs+EF_K1
	sreg	fp,_assregs+EF_FP
	sreg	ra,_assregs+EF_RA
	mflo	v0
	sreg	v0,_assregs+EF_MDLO
	mfhi	v0
	sreg	v0,_assregs+EF_MDHI
	.set noreorder
	MFC0(v0,C0_CAUSE)
	NOP_1_0
	sreg	v0,_assregs+EF_CAUSE
	MFC0_BADVADDR(v0)
	NOP_1_0
	sreg	v0,_assregs+EF_BADVADDR
	MFC0(v0,C0_SR)
	NOP_1_0
	sreg	v0,_assregs+EF_SR
	DMFC0(v0,C0_EPC)
	NOP_1_0
	.set reorder
	sreg	v0,_assregs+EF_EPC
	sreg	v0,_assregs+EF_EXACT_EPC
1:
	j	_assfail
	AUTO_CACHE_BARRIERS_ENABLE
	END(assfail)

/*
 * Primitives
 */ 

/*
 * getsr() -- return contents of status register
 */
LEAF(getsr)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_SR)		# either delayed enough or interlocked
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(getsr)
/*
 * setsr() -- set contents of status register
 */
LEAF(setsr)
	.set	noreorder
	or	a0,SR_KERN_SET		# don't let anyone touch pagesize
	MTC0(a0,C0_SR)
	NOP_0_4
	NOP_0_1
	.set reorder
	j	ra
	END(setsr)

/*
 * getpc() -- return caller's PC.
 */
LEAF(getpc)
	move	v0,ra
	j	ra
	END(getpc)
/*
 * getsp() -- return contents of stack pointer
 */
LEAF(getsp)
	move	v0,sp
	j	ra
	END(getsp)
/*
 * void getpcsp(__psunsigned_t *pc, __psunsigned_t *sp)
 * -- return caller's PC and stack pointer.
 */
LEAF(getpcsp)
	PTR_S	ra,0(a0)
	PTR_S	sp,0(a1)
	j	ra
	END(getpcsp)
/*
 * Provide the kernel GP value.
 */
LEAF(getgp)
	move	v0,gp
	j	ra
	END(getgp)

/*
 * getcause() -- return contents of cause register
 */
LEAF(getcause)
	.set	noreorder
	MFC0_NO_HAZ(v0,C0_CAUSE)
	NOP_1_0
	j	ra
	nop
	.set	reorder
	END(getcause)

/*
 * setcause() -- set contents of cause register
 */
LEAF(setcause)
	.set	noreorder
	DMTC0(a0,C0_CAUSE)
	NOP_1_0
	j	ra
	nop
	.set	reorder
	END(setcause)

LEAF(getbadvaddr)
	.set	noreorder
	DMFC0(v0,C0_BADVADDR)
	NOP_0_4
	.set	reorder
	j	ra
	END(getbadvaddr)

LEAF(get_sr)
	.set	noreorder
	MFC0(v0,C0_SR)
	NOP_0_4
	.set	reorder
	j	ra
	END(get_sr)

LEAF(set_sr)
	.set	noreorder
	MTC0(a0,C0_SR)
	NOP_0_4
	.set	reorder
	j	ra
	END(set_sr)

#if R4000 || R10000
LEAF(get_lladdr)
	.set	noreorder
	MFC0(v0, C0_LLADDR)
	j	ra
	 nop
	.set	reorder
	END(get_lladdr)

LEAF(set_lladdr)
	.set	noreorder
	MTC0(a0, C0_LLADDR)
	j	ra
	 nop
	.set	reorder
	END(set_lladdr)
#endif

#ifndef IP26		/* IP26 has hardware failures with sync instr */
/*
 * do_sync_instruction()
 */
LEAF(do_sync_instruction)
	sync
	j	ra
	END(do_sync_instruction)
#endif


/*
 * siron(level) -- make software interrupt request
 */
LEAF(siron)
	.set noreorder
	MFC0(v0,C0_SR)			# LDSLOT
#if TFP
	# First we disable all interrupts in the SR

	ori	t0,v0,SR_IE
	xori	t0,SR_IE		# this turns off IE bit
	DMTC0(t0,C0_SR)

	# Then we enable SW interrupt request in CAUSE register

	MFC0(v1,C0_CAUSE)
	or	v1,a0
	MTC0(v1,C0_CAUSE)

	# Finally, restore contents of SR

	DMTC0(v0,C0_SR)
	j	ra
	nada
#else	/* !TFP */
#if R4000 || R10000
	lui	v1,(SR_DE >> 16)	# propagate DE and ERL bits in SR
	or	v1,SR_ERL|SR_KERN_SET
	and	v1,v1, v0
	MTC0(v1,C0_SR)			# disable all interrupts
#else
	MTC0(zero,C0_SR)		# disable all interrupts
#endif
	NOP_1_4
	MFC0(v1,C0_CAUSE)
	NOP_1_4
	or	v1,a0
	MTC0(v1,C0_CAUSE)
	NOP_1_0				# TODO: needed? (not on R4000)
	j	ra
	MTC0(v0,C0_SR)
#endif	/* !TFP */
	.set 	reorder
	END(siron)

/*
 * siroff(level) -- acknowledge software interrupt request
 */
LEAF(siroff)
	.set 	noreorder
	MFC0(v0,C0_SR)
#if TFP
	# First we disable all interrupts in the SR

	ori	t0,v0,SR_IE
	xori	t0,SR_IE		# this turns off IE bit
	DMTC0(t0,C0_SR)

	# Then we disable SW interrupt request in CAUSE register

	MFC0(v1,C0_CAUSE)
	not	a0			# LDSLOT
	and	v1,a0
	MTC0(v1,C0_CAUSE)

	# Finally, restore contents of SR

	DMTC0(v0,C0_SR)
	j	ra
	nop
#else /* !TFP */
#if R4000 || R10000
	lui	v1,(SR_DE >> 16)
	or	v1,SR_ERL|SR_KERN_SET
	and	v1,v1, v0
	MTC0(v1,C0_SR)			# disable all interrupts
#else
	MTC0(zero,C0_SR)		# disable all interrupts
#endif
	NOP_1_4
	MFC0(v1,C0_CAUSE)
	not	a0			# LDSLOT
	NOP_0_2
	and	v1,a0
	MTC0(v1,C0_CAUSE)
	NOP_1_0				# TODO: needed? (not on R4000)
	j	ra
	MTC0(v0,C0_SR)
#endif /* !TFP */
	.set 	reorder
	END(siroff)

/*
 * we need to be able to guarentee that a required DELAY is relative to
 * the bus rather than the CPU. The old wbflush was used both for
 * delay'ing, as well as for the old write buffers that did byte gathering.
 * DELAY is not sufficient since that measures processor time, not bus time
 * the DELAYBUS/us_delaybus routines call this function that should
 * guarantee that the write really went out
 */
LEAF(flushbus)
	wbflushm
	.set noreorder
	nop	# 2.10 assembler says it is needed....
	j	ra
	nop				# BDSLOT
	.set reorder
	END(flushbus)

#ifndef EVEREST
LEAF(wbflush)
/* no one else really needs to flush the write buffer */
#if IP32
	j	__sysad_wbflush
#endif
	j	ra
	END(wbflush)	
#endif

#ifndef _NO_UNCACHED_MEM_WAR	/* IP26 + IP28 use hack ECC board */
LEAF(ip26_enable_ucmem)
XLEAF(ip26_return_ucmem)
XLEAF(ip28_enable_ucmem)
XLEAF(ip28_return_ucmem)
	j	ra
	END(ip26_enable_ucmem)
#endif
