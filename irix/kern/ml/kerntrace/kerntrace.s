/**************************************************************************
 *									  *
 *		 Copyright (C) 1989-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.2 $"


#include <asm.h>
#include <regdef.h>
#include "assym.h"
#include "ml/ml.h"	

#ifdef	EVEREST
#define	TR_NCPU		36
#elif SN0
	/* WARNING: Setting TR_NCPU to 128 produces a kernel image which
	 * is "too large" due to the total space required by _trrectab.
	 * For now we work around this by only allowing tracing on 64
	 * processors or less.
	 */
#define	TR_NCPU		64
#elif	IP20 || IP22
#define	TR_NCPU		1
#endif

#define	TR_NENTRIES	(8192)
#define	TR_TBLIMASK	(TR_NENTRIES-1)
#define	TR_TSIZE	(TR_NENTRIES*8)	/* MUST be 64K to match TR_TSHIFTL */
#define	TR_TSHIFTL	16		/* * 64k */
#define	TR_ENABSZ	(2048)

/*
 * bugworkaround:
 * mmap() of /dev/kmem of .sbss causes SIGSEGV on reference
 * so we add a few bytes to ensure that the symbol
 * goes into .bss and not .sbss
 */
#define	MMMAPBUG	8

/*
 * Hidden trampoline stack frame.
 */
#define	TR_LOCALSZ	2
TR_TRAMPFRM=	(TR_LOCALSZ*SZREG)
TR_ORDOFF=	TR_TRAMPFRM-(1*SZREG)
TR_RAOFF=	TR_TRAMPFRM-(2*SZREG)

	.globl	_trrectab
	.globl	_trenabletab
	.globl	_trindextab
	.globl	_trreg
	.globl	_trmagic
	.data
	.lcomm	_trrectab	(TR_NCPU * TR_TSIZE)
	.lcomm	_trenabletab	(TR_ENABSZ)
	.lcomm	_trindextab	((TR_NCPU * SZREG) + MMMAPBUG)
	.lcomm	_trreg		4
_trmagic:		
	.word	0xdead0110

	/* _trreg
	 *	== 0	log RTC into tracerec
	 *	== 1	log PRFCNT0 into tracerec (R10K only)
	 *	== 2	log PRFCNT1 into tracerec (R10K only)
	 */
	.text

/*
 * _trexit()
 *	function ordinal# in $AT
 *	ra in ra
 * NOTE: This routine is entered via a branch/jump instruction.
 * NOTE2: This routine appears first since this keeps the (hopefully)
 *	common code path for disabled routines all in one cacheline.
 */
LEAF(_trexit)
	/* force alignment to cacheline to minimize total number of
	 * cachelines needed for this tracing code.
	 */
	.align	7
	.set	noreorder
	.set	noat

	/* We pre-bias RA by -8 so when _trentry adds 8 we get a NOP.
	 * This is better than using a "br" instruction to avoid the PTR_ADDI.
	 */
	
	lui	t3, 0x8000		# hi bit set = "exit"
	or	AT, t3			# set "exit" bit in ordinal #
	PTR_ADDI ra, -8			# pre-bias RA 

/*
 * _trentry()
 *	function ordinal# in $AT
 *	t8 = invoking routine's RA
 */
XLEAF(_trentry)
	PTR_ADDI ra, 8			# add 2 insts (trexit post-amble)

	/*
	 * Is tracing enabled for this function?
	 */
	
	srl	t0,AT,3			# t0 = byte offset of ordinal #
	andi	t0,0xffff		# mask off "exit" bit, if set
	LA	t1,_trenabletab
	PTR_ADDU t0,t1			# got enable byte addr
	lb	t2,0(t0)		# got enable byte
	andi	t0,AT,7			# modulo 7
	li	t1,1
	sll	t1,t0
	and	t2,t1			# test bit in enable word
	bne	t2,zero,_trlogevent
	nada				# BDSLOT

	/* enable bit not set, just return */
	j	ra
	move	ra, t8			# BDSLOT - restore "ra"

_trlogevent:	
	/*
	 * log event
	 */
	CPUID_L	t3,VPDA_CPUID(zero)
	LA	t0,_trrectab
	PTR_SLL	t1,t3,TR_TSHIFTL
	PTR_ADDU t0,t1			# got buf base address

	LA	t1,_trindextab
	PTR_SLL	t2,t3,2
	PTR_ADDU t1,t2
	lw	t2,0(t1)		# load index
	addu	t3,t2,1
	andi	t3,TR_TBLIMASK
	sw	t3,0(t1)		# index++

	PTR_SLL	t2,3			# 8bytes/entry
	PTR_ADDU t0,t2			# got entry addr

	sw	AT,0(t0)		# store ordinal

#ifdef R10000
	LA	t1, _trreg
	lw	t1,(t1)
	bne	t1, zero, _trprfcnt
	addi	t1,-1			# BDSLOT
#endif /* R10000 */	
	_GET_TIMESTAMP(t1)
savereg:
	sw	t1,4(t0)		# store timestamp

	/* Following code sequence returns to "ra".  For _trentry it also
	 * restores the invoking routine's correct "ra" value.  For _trexit
	 * it ends up trashing the "ra" but we're already returned.
	 */
	j	ra
	move	ra, t8			# BDSLOT - restore "real" RA

#ifdef R10000
_trprfcnt:		
	beql	t1, zero, savereg
	MFPC(t1,PRFCNT0)		# BDSLOT (PRFCNT0)

	b	savereg
	MFPC(t1,PRFCNT1)		# BDSLOT (PRFCNT1)
#endif /* R10000 */	
	END(_trexit)


