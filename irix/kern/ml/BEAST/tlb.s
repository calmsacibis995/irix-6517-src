/**************************************************************************
 *									  *
 *		 Copyright (C) 1997-1998, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * This file has all the beast specific cacheops.
 */
#include "ml/ml.h"

/*
 * tlbwired(indx, tlbpid, vaddr, pte) -- setup wired ITLB entry
 * a0 -- indx -- tlb entry index
 * a1 -- tlbpidp -- pointer to array of context numbers to use(0..TLBHI_NPID-1)
 *                  indexed by cpuid.  EXCEPT if 0, tlbpid = 0.
 * a2 -- vaddr -- virtual address (could have offset bits)
 * a3 -- pte -- contents of pte
 */

LEAF(tlbwired)
	.set	noreorder

	mfc0	v0, C0_SR
	NOINTS(t1, C0_SR)
	beq	zero, a1, 1f
	 DMFC0(t0, C0_TLBHI)		#save current TLBPID
#if MP
	CPUID_L	t1, VPDA_CPUID(zero)
	PTR_ADD	a1, t1
#endif
	lbu	a1, (a1)
	sll	a1, TLBHI_PIDSHIFT
1:		
	and	a3, TLBLO_HWBITS	#chop software bits from pte
	and	a2, TLBHI_VPNMASK
	or	a1, a2
	DMTC0(a1, C0_TLBHI)
	DMTC0(a3, C0_TLBLO)

	or	a0, TLBSET_WIRED	# mark that this entry is wired
	DMTC0(a0, C0_TLBSET)
	c0	C0_WRITEI
	DMTC0(t0, C0_TLBHI)		#restore TLBPID
	DMTC0(zero, C0_TLBSET)
	j	ra
	 mtc0	v0, C0_SR
	.set	reorder
	END(tlbwired)

/*
 * getrand - return random number
 * This routine is here since on other machines we return the contents
 * of the C0_RANDOM register. On BEAST the C0_RANDOM counts from 0 to 7.
 * If that is sufficient, we could return the c0_random register
 */
LEAF(getrand)
	.set    noreorder
	DMFC0(v0,C0_COUNT)		# get current counts register
	.set 	reorder
	j	ra
	END(getrand)






