/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996-1998 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

	
#include "ml/ml.h"
#include "sys/traplog.h"

	.set	noat
	.set	noreorder
	
/**************************************************************************
 * utlbmiss -- offset 0x000 within page (use .align 12)
 *************************************************************************/
/*
 * This is the standard prologue for all utlbmiss handlers.  It will
 * be copied into locore and executed prior to executing the various
 * utlbmiss handlers.
 * The standard utlbmiss handler will be copied immediately following 
 * this prolog and VPDA_UTLBMISSHNDLR will be setup to "jump" to this
 * location in locore when using the standard utlbmiss handler.  This
 * avoids requiring an additional secondary cache line in machines
 * which have secondary caches.
 */
	/* Align to page boundary so we can just place this address
	 * into the TrapBase.
	 */
	.align 12
EXPORT(trap_table)


NESTED(utlbmiss, 0, k1)
	MFC0_BADVADDR(a1)
	DMFC0(a2, C0_EPC)
	move	a3, ra	
	.set	reorder
	PRINTF("utlbmiss: BADVADDR %llx EPC %llx RA %llx")
	.set	noreorder
EXPORT(eutlbmiss)	
	END(utlbmiss)	

/**************************************************************************
 * ktlbmiss_private -- offset 0x200 within page (use .align 9)
 *************************************************************************/
/* ktlbmiss_private - used for TLB miss on Kernel Private Region (KV0)
 * This shouldn't happen (for now), so panic.
 */
	.align	9
NESTED(ktlbmiss_private, 0, k1)

	MFC0_BADVADDR(a1)
	.set	reorder
	PANIC("ktlbmiss_private: BADVADDR %llx")
	.set	noreorder
	END(ktlbmiss_private)

/**************************************************************************
 * ktlbmiss -- offset 0x400 within page (use .align 9)
 *************************************************************************/
/* ktlbmiss - used for TLB miss on Kernel Global Region (KV1)
 * This implies that the region bits are both set.
 * This code currently assumes first level tlbmiss from kernel. Misses
 * at KPTE_BASE are not yet handled.
 */
	.align	9

NESTED(ktlbmiss, 0, k1)
	MFC0_BADVADDR(k0)
	dsll	k0, 2			# chop off upper 2 bits
	dsrl	k0, 2 + PNUMSHFT	# k0 now has the page number
	PTR_L	k1, kptbl
	dsll	k0, PDESHFT
	daddu	k0, k1
	ld	k0, 0(k0)
	.set    at
	and	k0, TLBLO_HWBITS	# mask off software bits
	.set    noat
	ori	k0, SYS_PGSIZE_BITS
					# set fixed pte pagesize
	DMTC0(k0, C0_TLBLO)
	c0	C0_WRITEI
	eret
	 nop
	MFC0_BADVADDR(a1)
	.set reorder
	PANIC("ktlbmiss: BADVADDR %llx")
	.set	noreorder
	END(ktlbmiss)
	


/**************************************************************************
 * syscall -- offset 0x600 within page (use .align 9)
 *************************************************************************/
/*
 *	syscall
 */	
	.align	9
LEAF(__j_syscall)
	eret	
	END(__j_syscall)

/**************************************************************************
 * interrupt -- offset 0x800 within page (use .align 9)
 *************************************************************************/
	
/*
 *	interrupt
 */
	.align 9
LEAF(__j_interrupt)
	PTR_SUBU	sp, 0x20
	sd	ra, 8(sp)
	DMFC0(a1, C0_EPC)
	DMFC0(a2, C0_CAUSE)
	move	a3, fp	
	.set	reorder
	jal	check_interrupt
	.set	noreorder
	DMFC0(a1, C0_SR)
	ld	ra, 8(sp)
	PTR_ADDU	sp, 0x20
	eret
	END(__j_interrupt)
	

/**************************************************************************
 * exception_jump -- offset 0xc00 with page (use .align 10)
 *************************************************************************/
	.align 10
LEAF(__j_exceptnorm)
	PTR_SUBU	sp, 0x20
	sd	ra, 8(sp)
	DMFC0(a1, C0_EPC)
	DMFC0(a2, C0_CAUSE)
	
	.set	reorder
	jal	check_norm_except
	.set	noreorder
	DMFC0(a1, C0_SR)
	ori	a1, SR_EXL
	xor	a1, SR_EXL
	DMTC0(a1, C0_SR)
	ld	ra, 8(sp)
	PTR_ADDU	sp, 0x20
	eret
	END(__j_exceptnorm)

	.set	reorder
	.set	at
	
