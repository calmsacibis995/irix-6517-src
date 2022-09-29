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

#ident	"$Revision: 1.11 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

#ifdef ULI
/*
 * syscall from ULI is handled differently. The idea is to return to
 * interrupt mode and make the call from the interrupt stack. The
 * only allowable calls are calls that could normally be made at interrupt
 * level.
 */
LEAF(ULIsystrap)
	.set	reorder

	# only syssgi(SGI_ULI, ...) is allowed from ULI mode, all
	# other syscalls go to keruliframe and are treated as an
	# exception, aborting the handler and signalling the process
	bne	v0, SYS_syssgi, keruliframe
	bne	a0, SGI_ULI, keruliframe

	# fastpath the ULI_RETURN case here since we are trashing all
	# existing state anyway, no need to set up an eframe or wire
	# in the tlb entries
	bne	a1, ULI_RETURN, ulisystrap_longway

	# back on interrupt stack
	PTR_L	t0, VPDA_CURULI(zero)
	PTR_L	sp, ULI_INTSTK(t0)
	PTR_SUBU sp, (4*SZREG) # leave an arg save frame for uli_return()
#ifdef TFP
	# restore config register
	ld	t1, ULI_SAVED_CONFIG(t0)
	.set	noreorder
	dmtc0	t1, C0_CONFIG
	.set	reorder
#endif
	ori	sp, 7
	xori	sp, 7

	li	a0, 0
	# restore gp
	LA	gp, _gp
	j	uli_return
	/* NOTREACHED */

ulisystrap_longway:
	# non-fastpath kernel calls from ULI mode

	.set	reorder

	# return to interrupt stack, set up an eframe there
	PTR_L	k0, VPDA_CURULI(zero)
	PTR_L	k1, ULI_INTSTK(k0)

	PTR_SUBU k1, ULI_EF_SIZE
	ori	k1, 7
	xori	k1, 7
	
#ifdef TFP
	ld	t3, ULI_SAVED_CONFIG(k0)
#endif

#ifdef OVFLWDEBUG
	/* make a stab at detecting interrupt stack under/overflow
	 * do this BEFORE accessing memory
	 */
	PTR_L	k0, VPDA_INTSTACK(zero)
	PTR_ADDIU k0, k0, 100	# some slop
	bleu	k1, k0, uli_stkovflw
	PTR_L	k0, VPDA_LINTSTACK(zero)
	bgtu	k1, k0, uli_stkundflw
#endif OVFLWDEBUG

	# save system state in eframe
	.set	noreorder
#ifdef TFP
	dmfc0	t0, C0_SR
#else
	mfc0	t0, C0_SR
#endif
	sreg	a1, ULI_EF_A1(k1)
	sreg	a2, ULI_EF_A2(k1)
	sreg	a3, ULI_EF_A3(k1)

	dmfc0	t1, C0_EPC
	sreg	ra, ULI_EF_RA(k1)
	sreg	fp, ULI_EF_FP(k1)
	sreg	sp, ULI_EF_SP(k1)
#ifdef TFP
	dmfc0	t2, C0_CONFIG
#endif
	.set	reorder

	# skip over syscall instruction on return
	PTR_ADDIU t1, 4
	
	# save registers. NOTE: since we don't use the s registers
	# here, we don't need to save them. All other registers are
	# saved automatically by the function call semantics of the
	# system call
	sreg	t0, ULI_EF_SR(k1)
	sreg	t1, ULI_EF_EPC(k1)
#ifdef TFP
	sreg	t2, ULI_EF_CONFIG(k1)
	# restore kernel's config
	.set	noreorder
	dmtc0	t3, C0_CONFIG
	.set	reorder
#endif
	sreg	gp, ULI_EF_GP(k1)
	LA	gp, _gp
	move	sp, k1

	# back on interrupt stack
	li	t0, PDA_CURINTSTK
	sb	t0, VPDA_KSTACK(zero)

	# restore interrupted context
	PTR_L	t0, VPDA_CURULI(zero)
	PTR_L	t1, ULI_SAVED_CURUTHREAD(t0)
	PTR_S	t1, VPDA_CURUTHREAD(zero)

	lh	a0, ULI_SAVED_ASIDS(t0)
	jal	uli_setasids
1:
	lreg	t0, ULI_EF_SR(k1)
#ifdef TFP
	and	t0, SR_KERN_USRKEEP
	LI	t1,SR_IEC|SR_KERN_SET	# enable, but mask all interrupts
	or	t0, t1
	.set	noreorder
	DMTC0(	t0, C0_SR)	# interrupts enabled
	.set	reorder
#endif
#if defined(R4000) || defined(R10000)
	# set base kernel mode, EXL low, interrupts enabled
	and	t0, SR_IMASK | SR_KX
	ori	t0, SR_IE
	.set	noreorder
	mtc0	t0, C0_SR	# interrupts enabled
	.set	reorder
#endif
	# sp now has eframe

	PTR_ADDIU a0, sp, ULI_EF_RETURN
	lreg	a1, ULI_EF_A1(sp)
	lreg	a2, ULI_EF_A2(sp)
	lreg	a3, ULI_EF_A3(sp)
	jal	ULI_syscall

	# disable interrupts
	lreg	t0, ULI_EF_SR(sp)
#ifdef TFP
	and	t0, SR_IMASK|SR_KERN_USRKEEP # set kernel base mode, low EXL
	or	t0, SR_KERN_SET
#else
	and	t0, SR_IMASK | SR_KX
#endif
	.set	noreorder
	MTC0(	t0, C0_SR)

	# instructions from here to next mtc0 serve as nops
	# while interrupts are disabled (R4000)
	# On TFP, the MTC0 macro provides the necessary ssnops
	bnel	v0, zero, 1f
	li	a3, 1

	# otherwise no error, return value in v0
	lreg	v0, ULI_EF_RETURN(sp) # sp has eframe
	move	a3, zero
1:
	.set	reorder # so the R4000 code below can interleave
	li	t1, PDA_CURULISTK
	sb	t1, VPDA_KSTACK(zero)

#ifdef R4000
	ori	t0, SR_EXL
	.set	noreorder
	# interrupts disabled
	# preset the EXL bit to avoid the KSU/EXL hazard
	MTC0(	t0, C0_SR)
#endif
	.set	reorder
	move	k1, sp

	# k1 has eframe again

	# restore ULI context
	PTR_L	t2, VPDA_CURULI(zero)
	PTR_L	a1, ULI_UTHREAD(t2)
	PTR_S	a1, VPDA_CURUTHREAD(zero)

	lh	a0, ULI_NEW_ASIDS(t2)
	jal	uli_setasids
	
	# restore prev machine state
	.set	noreorder
#ifdef TFP
	lreg	t0, ULI_EF_CONFIG(k1)
	dmtc0	t0, C0_CONFIG
#endif
	lreg	t2, ULI_EF_SR(k1)

	lreg	t1, ULI_EF_EPC(k1)
	lreg	ra, ULI_EF_RA(k1)
	dmtc0	t1, C0_EPC
	lreg	fp, ULI_EF_FP(k1)
	lreg	sp, ULI_EF_SP(k1)
	MTC0(	t2, C0_SR)
	lreg	gp, ULI_EF_GP(k1)
	eret
	.set	reorder

EXPORT(keruliframe)
	# abort if the ULI handler tries to get into the kernel
	# for any reason
	.set	reorder

	PTR_L	k0, VPDA_CURULI(zero)

	# store debug data at most once
	lw	k1, ULI_EFRAME_VALID(k0)
	bne	k1, zero, 1f

	# get ULI eframe
	PTR_ADDU k1, k0, ULI_EFRAME

	sreg	sp, EF_SP(k1)
	sreg	gp, EF_GP(k1)
	.set	noat
	lreg	AT, VPDA_ATSAVE(zero)	# recall saved AT
	sreg	AT, EF_AT(k1)

	.set	noreorder
#ifdef TFP
	dmfc0	AT, C0_BADVADDR
	SAVEAREGS(k1)
	SAVESREGS_GETCOP0(k1)		# save sregs and get COP0 regs
#endif
#if defined(R4000) || defined(R10000)
	SAVEAREGS(k1)
	SAVESREGS(k1)
	mfc0	s0, C0_SR
	mfc0	s2, C0_CAUSE
	dmfc0	s3, C0_EPC
	dmfc0	AT, C0_BADVADDR
#endif
	.set	reorder
	
	sreg	ra, EF_RA(k1)
	jal	tfi_save		# save tregs, v0,v1, mdlo,mdhi

#ifdef TFP
	ld	t0, ULI_SAVED_CONFIG(k0)
	sreg	s1, EF_CONFIG(k1)
	# restore kernel's config reg
	.set	noreorder
	dmtc0	t0, C0_CONFIG
	.set	reorder
#endif

	sreg	s0, EF_SR(k1)
	sreg	s2, EF_CAUSE(k1)
	sreg	s3, EF_EPC(k1)
	sreg	s3, EF_EXACT_EPC(k1)
	sreg	AT, EF_BADVADDR(k1)
	.set	at

	sw	k0, ULI_EFRAME_VALID(k0)

1:
	# restore interrupt stack pointer
	PTR_L	sp, ULI_INTSTK(k0)
	PTR_SUBU sp, (4*SZREG) # leave an arg save frame
	ori	sp, 7
	xori	sp, 7

	LA	gp, _gp

#ifdef EPRINTF
	# get the address of the pte on which we faulted, a la utlbmiss
	.set	noreorder
#ifdef R4000
	# get PDE pointer
	dmfc0	t0, C0_CTXT
	nop
#if	NBPP == 16384
#define CTXT_TO_PTE_ALIGN(reg)	PTR_SRA	reg, KPTE_TLBPRESHIFT+PGSHFTFCTR+3
#define CTXT_TO_PTE_CLEAN(reg)	PTR_SLL	reg, 3
#endif
#if (R4000 || R10000) && _MIPS_SIM == _ABI64
#define	CTXT_REGION_ADD(scratch,target)				\
					li	scratch,3 ;	\
					dsll	scratch,62 ;	\
					or	target, scratch
#else
#define	CTXT_REGION_ADD(scratch,target)
#endif

	CTXT_TO_PTE_ALIGN(t0)		# Convert for 4 byte ptes
	CTXT_TO_PTE_CLEAN(t0)
	CTXT_REGION_ADD(k1,t0)
#endif
#ifdef TFP
	MFC0_BADVADDR_NOHAZ(t0)
	NOP_NADA			# alignment op (for C0_WRITER)
	dsra	t0,PNUMSHFT		# Convert to VPN - FORCE NEXT CYCLE
	dmfc0	k1,C0_KPTEBASE		# Load KPTEBASE
	dsll	t0,2			# Convert VPN for 4 byte pte
	dadd	t0,k1			# address is in KV0 space
#endif
	PTR_S	t0, ULI_BUNK(k0)
	.set	reorder
#endif
	j	uli_badtrap

	/* NOTREACHED */

	/*
	 * Stack Under/Overflow and other stk anomalies
	 * must get into debugger without causing exception
	 */
uli_stkovflw:
	LA	a0,8f
	b	uli_stk1
uli_stkundflw:
	LA	a0,7f

uli_stk1:
	sw	sp,uli_ovflwword	/* poke something for LAs if can't
					 * really print
					 */
	.set noreorder
	DMFC0(a1,C0_EPC)
	.set reorder
	move	a2,sp
	move	a3,k1			# in case from utlbmiss
	move	s0,ra
	/* make sure a good frame - this works except in VERY early cases */
	lb	s1,VPDA_KSTACK(zero)
	li	k0,PDA_CURIDLSTK	# on kernel stack now
	sb	k0,VPDA_KSTACK(zero)
	PTR_L	sp,VPDA_LBOOTSTACK(zero)
	jal	printf
	LA	a0,3f
	move	a1,s0
	move	a2,s1
	jal	printf
#if R4000 || R10000
	/*
	 * must turn off EXL since jumping to debug() causes a bkpnt exc
	 * and it wants to look at EPC
	 */

	.set noreorder
	MFC0(a0,C0_SR)
	NOP_1_0
	lui	k0,(SR_DE >> 16)	# propagate the SR_DE bit
	ori	k0,SR_ERL
	and	a0,k0,a0
	or	a0,SR_IEC|SR_IMASK8|SR_KERN_SET
	MTC0(a0,C0_SR)
	.set reorder
#endif

#if TFP
	/*
	 * must turn off EXL since jumping to debug() causes a bkpnt exc
	 * and it wants to look at EPC
	 */
	.set	noreorder
	DMFC0(k0,C0_SR)
	li	k1,SR_EXL|SR_IEC
	not	k1
	and	k0,k1
	DMTC0(k0,C0_SR)
	.set	reorder
#endif
	LA	a0,2f
	jal	panic

	.data
8:	.asciiz	"Kernel/Interrupt Stack Overflow @0x%x sp:0x%x k1:0x%x\12\15"
7:	.asciiz	"Kernel/Interrupt Stack Underflow @0x%x sp:0x%x k1:0x%x\12\15"
6:	.asciiz	"kernel mode on exception but not KERSTK! @0x%x sp:0x%x k1:0x%x\12\15"
5:	.asciiz	"kernel mode on interrupt but not KERSTK! @0x%x sp:0x%x k1:0x%x\12\15"
4:	.asciiz	"KERSTK on interrupt but not kernel mode! @0x%x sp:0x%x k1:0x%x\12\15"
3:	.asciiz "ra:0x%x stkflag:%d\n"
2:	.asciiz "stack underflow/overflow"

uli_ovflwword:
	.word	0

	.text
	END(ULIsystrap)

#ifdef R10000_SPECULATION_WAR
#error Need R10000_SPECULATION_WAR for ULI.
#endif

#endif /* ULI */
