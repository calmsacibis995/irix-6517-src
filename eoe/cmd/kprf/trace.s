#ident  "$Revision: 1.2 $"

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


#include <asm.h>
#include <regdef.h>
#include "assym.h"

#ifdef	EVEREST
#define	TR_NCPU		36
#define	RTC		0x9000000018020004	/* Real-time clock (low-order word) */
#elif	IP20 || IP22
#define	TR_NCPU		1
#define	RTC		0xbfa01004	/* Indy Real-time clock */
#endif


#define	REG_S	sd
#define	REG_L	ld
#define	PTR_L	ld
#define	PTR_ADDU	daddu
#define	PTR_SUBU	dsubu
#define	SZREG	8

#define	TR_NENTRIES	(8192)
#define	TR_TBLIMASK	(TR_NENTRIES-1)
#define	TR_TSIZE	(TR_NENTRIES*8)
#define	TR_TSHIFTL	16		/* * 64k */
#define	TR_ENABSZ	(1024)

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
	.data
	.lcomm	_trrectab	(TR_NCPU * TR_TSIZE)
	.lcomm	_trenabletab	(TR_ENABSZ)
	.lcomm	_trindextab	((TR_NCPU * SZREG) + MMMAPBUG)

	.text


/*
 * _trentry()
 *	function ordinal# in $AT
 *	ra in t0
 */
LEAF(_trentry)
	.set	noreorder
	.set	noat

	move	t8, t0
	/*
	 * Is tracing enabled for this function?
	 */
	move	t0,AT
	srl	t0,5
	sll	t0,2			# got enable word offset
	dla	t1,_trenabletab
	addu	t0,t1			# got enable word addr
	lw	t2,0(t0)		# got enable word
	move	t0,AT
	andi	t0,31			# modulo 32
	li	t1,1
	sll	t1,t0
	and	t2,t1			# test bit in enable word
	beq	t2,zero,1f		# no
	nop

	/*
	 * log event
	 */
	lw	t3,VPDA_CPUID(zero)
	dla	t0,_trrectab
	sll	t1,t3,TR_TSHIFTL
	addu	t0,t1			# got buf base address

	dla	t1,_trindextab
	sll	t2,t3,2
	addu	t1,t2
	lw	t2,0(t1)		# load index
	addu	t3,t2,1
	andi	t3,TR_TBLIMASK
	sw	t3,0(t1)		# index++

	sll	t2,3			# 8bytes/entry
	addu	t0,t2			# got entry addr

	sw	AT,0(t0)		# store ordinal
	dli	t1,RTC
	lw	t1,0(t1)
	sw	t1,4(t0)		# store timestamp

1:
	j	ra
	move	v0, t8

	END(_trentry)

/*
 * _trexit()
 *	function ordinal# in $AT
 *	ra in t0
 */
LEAF(_trexit)
	.set	noreorder
	.set	noat

	/*
	 * Is tracing enabled for this function?
	 */
	move	t3, AT
	move	t8, t0			# save ret. addr in t8
	move	t0,t3
	srl	t0,5
	sll	t0,2			# got enable word offset
	dla	t1,_trenabletab
	addu	t0,t1			# got enable word addr
	lw	t2,0(t0)		# got enable word
	move	t0,t3
	andi	t0,31			# modulo 32
	li	t1,1
	sll	t1,t0
	and	t2,t1			# test bit in enable word
	beq	t2,zero,1f		# no
	nop

	/*
	 * log event
	 */
	lw	t3,VPDA_CPUID(zero)
	dla	t0,_trrectab
	sll	t1,t3,TR_TSHIFTL
	addu	t0,t1			# got buf base address

	dla	t1,_trindextab
	sll	t2,t3,2			# each index is a word
	addu	t1,t2			# got address of index
	lw	t2,0(t1)		# load index
	addu	t3,t2,1
	andi	t3,TR_TBLIMASK
	sw	t3,0(t1)		# index++

	sll	t2,3			# 8bytes/tracerecord
	addu	t0,t2			# got entry addr

	move	t3, AT
	lui	t2,0x8000		# hi bit set="exit"
	or	t2,t3
	sw	t2,0(t0)		# store ordinal
	dli	t1,RTC
	lw	t1,0(t1)
	sw	t1,4(t0)		# store timestamp

	/*
	 * return to real caller (undo hidden 8 byte stack frame)
	 */
1:
	j	t8
	nop

	END(_trexit)
