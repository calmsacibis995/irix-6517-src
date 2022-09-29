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

#if R4000 || R10000
/* ECC error vector
 *
 * R10000_SPECULATION_WAR: $sp not valid here and running uncached.
 */
LEAF(_ecc_errorvec)
	AUTO_CACHE_BARRIERS_DISABLE
	.set    noreorder
	NOP_0_4
#if IP28
	j	ECC_SPRING_BOARD		# low memory code to handle err
	nop					# BDSLOT
#elif IP30
	/*
	 * On entry, we have no registers to play with. We start
	 * by saving k1 in cp0 watchli/lo and ll registers. we don't
	 * use cp0 context register since four of its bits are hardwired
	 * to 0 and in case we come here via the 32-bits tlb miss exception
	 * handler, the value in the register may be important
	 */
	.set	noat
	MTC0(k1,C0_LLADDR)			# save the 32 LSBs of k1
	dsrl32	k1,0
	dsll	k1,3				# make sure the 3 LSBs are 0's
	MTC0(k1,C0_WATCHLO)			# save the middle 29 bits of k1
	dsrl32	k1,0
	MTC0(k1,C0_WATCHHI)			# save the 3 MSBs of k1

	/* determine where the ECC stack located */
	CLI	k1,PHYS_TO_COMPATK1(HEART_PRID)
	ld	k1,0(k1)			# processor id
	dsll	k1,3				# *8 to get offset
	PTR_L	k1,CACHE_ERR_FRAMEPTR(k1)	# ECC handler stack	

	sreg	k0,EF_K0(k1)			# save k0
	sreg	AT,EF_AT(k1)			# save AT

	/*
	 * can't just 'j ecc_exception' since the code crosses the
	 * the 256MB segment boundary
	 */
	LA	k0,ecc_exception
	LI	AT,TO_PHYS_MASK
	and	k0,AT
	LI	AT,K1BASE
	or	k0,AT

	j	k0
	nop
	.set	at

#else
#if defined (SN0)
	/*
	 * On SN0, we are in the uncached ualias space. Any relative
	 * branches will not work correctly, so for now switch to
	 * uncached space.
	 */
	sreg	k0, (CACHE_ERR_EFRAME + (EF_K0))(zero)
	sreg	k1, (CACHE_ERR_EFRAME + (EF_K1))(zero)
	sreg	AT, (CACHE_ERR_EFRAME + (EF_AT))(zero)
        LA      k0, 2f
        and     k0, ~COMPAT_K1BASE

	PTR_L	k1,CACHE_ERR_IBASE_PTR
	bne	k1, zero, 1f
	 nop

	/*
         * If we take the cache error exception so early that the ibase pointer
	 * is not setup, we assume the kernel is replicated on all nodes
	 * and jump to our local copy. If this becomes an issue, we should
	 * lookup the tlb entry to figure out where the kernel copy resides.
	 */
	LI	k1, UALIAS_BASE
1:		
        or      k0, k1
        jr      k0
	 nop
2:
#endif /* SN0 */
	j	ecc_exception
	nop			# BDSLOT
#endif
	.set 	reorder
EXPORT(_ecc_end)
	AUTO_CACHE_BARRIERS_ENABLE
	END(_ecc_errorvec)

#if (R4000 && R10000)
EXPORT(_r10k_ecc_errorvec)
	.set	noreorder
	NOP_0_4
	j	r10k_ecc_exception
	nop			# BDSLOT
	.set 	reorder
EXPORT(_r10k_ecc_end)
#endif /* (R4000 && R10000) */

#endif /* R4000 || R10000 */


#if defined (SN)

LEAF(cache_error_exception_vec)

	AUTO_CACHE_BARRIERS_DISABLE
	.set    noreorder
	NOP_0_4

        sreg    k0, (CACHE_ERR_EFRAME + (EF_K0))(zero)
        sreg    k1, (CACHE_ERR_EFRAME + (EF_K1))(zero)
        sreg    AT, (CACHE_ERR_EFRAME + (EF_AT))(zero)
        LA      k0, 2f
        and     k0, ~COMPAT_K1BASE

        PTR_L   k1,CACHE_ERR_IBASE_PTR
        bne     k1, zero, 1f
        nop

        /*
         * If we take the cache error exception so early that the ibase pointer
         * is not setup, we assume the kernel is replicated on all nodes
         * and jump to our local copy. If this becomes an issue, we should
         * lookup the tlb entry to figure out where the kernel copy resides.
         */
        LI      k1, UALIAS_BASE
1:
        or      k0, k1
        jr      k0
        nop
2:
        j       cache_error_exception
        nop                     # BDSLOT
        .set    reorder

EXPORT(cache_error_exception_vec_end)
        AUTO_CACHE_BARRIERS_ENABLE
        END(cache_error_exception_vec)

#endif /* SN */
