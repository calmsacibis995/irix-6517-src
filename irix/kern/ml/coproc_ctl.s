
/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * This file contains routines to get and set the coprocessor point control
 * registers.
 */

#ident "$Revision: 3.26 $"

#include "ml/ml.h"

/*
 * get_fpc_csr returns the fpc_csr.
 */
LEAF(get_fpc_csr)
	.set    noreorder
	MFC0(v1,C0_SR)
	NOP_1_0
	.set	reorder
#if TFP
	and	a0,v1,SR_KERN_USRKEEP
	or	a0,SR_CU1|SR_KERN_SET
#else
	LI	a0,SR_CU1|SR_KERN_SET
#endif
	.set    noreorder
	MTC0(a0,C0_SR)
	NOP_1_3				# before we can really use cp1
	.set	reorder
	cfc1	v0,fpc_csr
	.set    noreorder
	nop				# before we can really use cp1
	.set	reorder
	.set    noreorder
	MTC0(v1,C0_SR)
	.set	reorder
	j	ra
	END(get_fpc_csr)

/*
 * set_fpc_csr sets the fpc_csr and returns the old fpc_csr.
 */
LEAF(set_fpc_csr)
	.set    noreorder
	MFC0(v1,C0_SR)
	NOP_1_0
	.set	reorder
#if TFP
	and	a1,v1,SR_KERN_USRKEEP
	or	a1,SR_CU1|SR_KERN_SET
#else
	LI	a1,SR_CU1|SR_KERN_SET
#endif
	.set    noreorder
	MTC0(a1,C0_SR)
	NOP_1_3				# before we can really use cp1
	NOP_1_0
	.set	reorder
	cfc1	v0,fpc_csr
	.set    noreorder
	nop
	ctc1	a0,fpc_csr
	nop
	.set	reorder
	.set    noreorder
	MTC0(v1,C0_SR)
	NOP_1_0
	.set    reorder
	j	ra
	END(set_fpc_csr)

/*
 * get_cpu_irr -- returns cpu revision id word
 * returns zero for cpu chips without a PRID register
 */
LEAF(get_cpu_irr)
#if R4000 || R10000
	.set	noreorder
	MFC0(v0,C0_PRID)
	NOP_0_4
	.set	reorder
	j	ra
#endif	/* R4000 || R10000 */
#if TFP
	.set	noreorder
	DMFC0(v0,C0_PRID)
	andi	t0, v0, 0xff
	bne	t0, zero, 9f	# if revision non-zero, go report it
	nop
	
	/* Revision field is zero, use bits in IC field of CONFIG register
	 * to correctly determine revision.
	 */
	DMFC0(t0,C0_CONFIG)
	and	t1, t0, CONFIG_IC
	dsrl	t1, CONFIG_IC_SHFT	# IC field now isolated
	lb	t2, revtab(t1)		# load minor revision
	daddu	v0, t2			# add into implementation
9:
	.set	reorder
	j	ra
	.rdata
	/* Sequence of minor revisions from IC field of CONFIG register:
	 * 	IC field:  0 1 2 3 4 5 6 7
	 *  	major rev  0 0 0 1 2 3 2 1
	 *      minor rev  0 0 0 1 2 0 1 2
	 */
revtab:
	.word	0x00000011
	.word	0x22302112
	.text
#endif	/* TFP */
	END(get_cpu_irr)

/*
 * get_fpc_irr -- returns fp chip revision id
 * NOTE: should not be called if no fp chip is present
 */
LEAF(get_fpc_irr)
	.set    noreorder
	MFC0(a1,C0_SR)			# save sr
	.set	reorder
	.set    noreorder
	NOINTS(a0,C0_SR)
	.set	reorder
	li	a0,NF_REVID		# set-up nofault handling
	sw	a0,VPDA_NOFAULT(zero)
#if TFP
	and	v0,a1,SR_KERN_USRKEEP
	or	v0,SR_CU1|SR_KERN_SET
#else
	LI	v0,SR_CU1|SR_KERN_SET
#endif
	.set    noreorder
	MTC0(v0,C0_SR)			# set fp usable
	NOP_1_3				# before we can really use cp1
	NOP_1_0
	.set	reorder
	cfc1	v0,fpc_irr		# get revision id
	sw	zero,VPDA_NOFAULT(zero)
	.set    noreorder
	MTC0(a1,C0_SR)
	.set    reorder
	.set    noreorder
	j	ra
	nop
	.set    reorder
	END(get_fpc_irr)

LEAF(reviderror)
	.set    noreorder
	MTC0(a1,C0_SR)			# restore sr
	move	v0,zero
	.set    reorder
	j	ra
	END(reviderror)


#if defined(SN0)

LEAF(get_cpu_cnf)
        .set    noreorder
        mfc0    v0,C0_CONFIG
        j       ra
        nop
        .set    reorder
        END(get_cpu_cnf)


/* *** WARNING ***
 *
 * These defines must match the declarations of 
 *
 *	r10k_il_t
 *	r10k_dl_t
 *	r10k_sl_t
 *
 * in ml/SN/error_private.h
 */


#define         DL_TAG          0
#define         DL_DATA         8
#define         DL_ECC          (DL_DATA+CACHE_DLINE_SIZE)

#define         IL_TAG          0
#define         IL_DATA         8
#define         IL_PARITY       (IL_DATA+CACHE_ILINE_SIZE *2)

#define         SL_TAG          0
#define         SL_DATA         8
#define         SL_ECC          (SL_DATA+CACHE_SLINE_SIZE)



LEAF(dLine)
        .set    noreorder

        DCACHE(C_ILT, 0(a0))            /* Read and store tag */
        MFC0(v0, C0_TAGHI)
        MFC0(v1, C0_TAGLO)
        dsll    v0,32
        dsll    v1,32
        dsrl    v1,32
        or      v0,v1
        sd      v0,DL_TAG(a1)

        daddiu  a2,a0,CACHE_DLINE_SIZE
        daddiu  t0,a1,DL_DATA           /* Data pointer */
        daddiu  t1,a1,DL_ECC            /* ECC pointer */

1:    
        DCACHE(C_ILD, 0(a0))		/* 32 bit data	*/
        MFC0(v0, C0_TAGLO)
        sw      v0,0(t0)

        MFC0(v0, C0_ECC)
        sb      v0,0(t1)
        daddiu  a0,4                    /* Next word - vaddr */
        daddiu  t0,4                    /* Next word - buffer */
        blt     a0,a2,1b
        daddiu  t1,1                    /* DELAY: Next ECC field */

        j       ra
        nop

        .set    reorder
        END(dLine)

LEAF(dCacheSize)
        .set    noreorder
        MFC0(v1, C0_CONFIG)             # Pick up config register value
        and     v1,CONFIG_DC            # isolate cache size
        srl     v1,CONFIG_DC_SHFT
        add     v1,CONFIG_PCACHE_POW2_BASE
        li      v0,1
        j       ra
         sll    v0,v1                   # Calculate # bytes
        .set    reorder
        END(dCacheSize)


/* ***************************************************************************
 * Function:    iLine
 * Purpose:     Fetch an instruction line from the icache.
 * Parameters:  a0 - vaddr (low order bit signals way)
 *              a1 - ptr to il_t buffer
 * Returns:     a1-> filled in.
 */
LEAF(iLine)
        .set    noreorder

        ICACHE(C_ILT, 0(a0))            /* Store Tag */
        MFC0(v0, C0_TAGLO)
        MFC0(v1, C0_TAGHI)
        dsll    v1,32
        dsll    v0,32
        dsrl    v0,32
        or      v0,v1
        sd      v0,IL_TAG(a1)

        daddiu  t0,a1,IL_DATA           /* Data pointer   */
        daddiu  t1,a1,IL_PARITY         /* Parity pointer */

        daddiu  a2,a0,CACHE_ILINE_SIZE
1:
        ICACHE(C_ILD, 0(a0))
        MFC0(v0, C0_TAGLO)
        MFC0(v1, C0_TAGHI)
        dsll    v1,32
        dsll    v0,32
        dsrl    v0,32
        or      v0,v1
        sd      v0,0(t0)

        MFC0(v0, C0_ECC)
        sb      v0,0(t1)
        daddiu  t0,8                    /* 8-bytes stored */
        daddiu  a0,4
        blt     a0, a2, 1b
        daddiu  t1,1                    /* 1-bit of parity */

        j       ra
        nop
        .set    reorder
        END(iLine)


/*
 * Function:    iCacheSize
 * Purpose:     determine the size of the primary instruction cache
 * Returns:     primary cache size in bytes in v0
 */
LEAF(iCacheSize)
        .set    noreorder
        mfc0    v1,C0_CONFIG
        and     v1,CONFIG_IC
        dsrl    v1,CONFIG_IC_SHFT
        dadd    v1,CONFIG_PCACHE_POW2_BASE
        li      v0,1
        j       ra
        dsll    v0,v1                   # cache size in byte
        .set    reorder
        END(iCacheSize)


/*
 * Function:    sLine
 * Purpose:     Fetch a cache line from the secondary cache.
 * Parameters:  a0 - vaddr (low order bit signals way)
 *              a1 - pointer to sl_t area.
 * Returns:     nothing
 */
LEAF(sLine)

        .set    noreorder

        SCACHE(C_ILT, 0(a0))            /* Pick up T5 TAG */
        MFC0(v0, C0_TAGHI)
        MFC0(v1, C0_TAGLO)
        dsll    v0,32
        dsll    v1,32                   /* Clear high order 32 bits */
        dsrl    v1,32
        or      v0,v1
        sd      v0, SL_TAG(a1)

        move    t0,ra                   /* Pick up CC tag */
        jal     sCacheSize
        nop
        move    ra,t0

        /* OK - lets get the data */

        li      v0,CACHE_SLINE_SIZE/16  /* # fetches */
        daddiu  t0,a1,SL_DATA
        daddiu  t1,a1,SL_ECC
2:
        SCACHE(C_ILD, 0(a0))
        MFC0(v1, C0_TAGLO)              /* Store 8-bytes of data */
        sw      v1,0(t0)
        MFC0(v1, C0_TAGHI)
        sw      v1,4(t0)
        MFC0(v1, C0_ECC)
        sh      v1,0(t1)                /* Store ECC for this 8 bytes */

        SCACHE(C_ILD, 8(a0))
        MFC0(v1, C0_TAGLO)              /* Store 8-bytes of data */
        sw      v1,8(t0)
        MFC0(v1, C0_TAGHI)
        sw      v1,12(t0)

        sub     v0,1
        daddu   a0,16                   /* Increment address */
        daddu   t0,16                   /* Increment data array. */
        bgt     v0,zero,2b
        daddiu  t1,2                    /* DELAY: Increment ECC array */
1:
        /* All done ... */
        j       ra
        nop
        .set    reorder
        END(sLine)


LEAF(sCacheSize)
        .set    noreorder
        mfc0    v1,C0_CONFIG
        and     v1,CONFIG_SS
        dsrl    v1,CONFIG_SS_SHFT
        dadd    v1,CONFIG_SCACHE_POW2_BASE
        li      v0,1
        j       ra
        dsll    v0,v1                   # cache size in byte
        .set    reorder
        END(sCacheSize)


#endif


