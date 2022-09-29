
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/asm.h>
#include <sys/cpu.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <caches.h>

#undef      SIM
/* #define     SIM 1 */

/********************
 * r5000_cache_config
 * ------------------
 * Initialize cache data structures
 */
NESTED(r5000_cache_config,framesize,ra)
    subu    sp, framesize           /* create current stack frame.      */
    .set    noat                    /* disable AT macro replacement.    */
    sd	    AT, regsave-0x00(sp)    /* save AT register.                */
    .set    at                      /* enable AT macro replacement.     */
    sd	    ra, regsave-0x08(sp)    /* save return address.             */
    sd	    a0, regsave-0x10(sp)    /* save a0                          */
    sd	    t0, regsave-0x18(sp)    /* save t0                          */
    sd	    t1, regsave-0x20(sp)    /* save t1                          */
    sd	    t2, regsave-0x28(sp)    /* save t2                          */
/*
 *  Configure the primary cache.
 */
    .set    noreorder               /* turn off assembler reordering    */
    mfc0    a0, C0_CONFIG           /* read the config register.        */
    nop                             /* one nop just in case.            */
    .set    reorder                 /* turn on  assembler reordering    */
    jal     cm_r4400_pcache_config  /* configure the pdcache.           */
/*
 *  Configure the 2 set primary cache.
 */
    lw      t2, _dcache_size        /* read the cache line size.        */
    srl     t2, 1                   /* one set is half of the cache     */
    sw      t2, _two_set_pcaches    /* reported in configuration reg.   */
    lw      t1, _icache_size        /* it also applied to icache.       */
    srl     t1, 1                   /* cut the size in half.            */
    sw      t1, _two_set_icaches    /* and save it.                     */
/*
 * Configure the secondary cache.
 * The SS field (11/25/95)  = 0 - 512K byte, 
 *                            1 - 1M   byte,
 *                            2 - 2M   byte, 
 *                            3 - Error, currently reserved 
 * NOTE:::
 *         For triton implementation the Config register bit 17 will 
 *     set to '1' if there is 'NO' secondary cache presented, and 
 *     set to '0' otherwise.
 *     Config[17] = 1  <--->  No secondary cache.
 *     Config[17] = 0  <--->  with secondary cache.
 */
    and     t0, a0, CONFIG_TR_SC    /* check 2nd cache.                 */
#if defined(SIM)
    beqz    t0, 10f                 /* skip the 2nd cache configuration */
#else
    bnez    t0, 10f                 /* skip the 2nd cache configuration */
#endif
    and     t1, a0, CONFIG_TR_SS    /* extract sidcache size.           */
    srl     t1, CONFIG_TR_SS_SHFT   /* move to bit 0                    */
    li      t2, 2                   /* Check for error boundary.        */
    bgt     t1, t2, 10f             /* Error case configure as no 2cache*/
    lui     t2, (0x80000 >> 16)     /* With 512K increment base.        */
    sllv    t2, t1                  /* This is scache size.             */
    sw      t2, _sidcache_size      /* Reports size in Triton Scache.   */
    sw      t2, _r4600sc_sidcache_size   /*  Reports size in 4600 size, */
    li      t1, R4600_SCACHE_LINESIZE /* Same linesize as cm_4600       */
    sw      t1, _scache_linesize    /* save the cache line size.        */
    subu    t1, 1                   /* Line mask is linesize -1         */
    sw      t1, _scache_linemask    /* save cache line mask.            */
    sw      t1, dmabuf_linemask     /* and dma buffer line mask.        */
/*
 *  Configure the logical cache op table.
 */
10: li      a0, PIlcops+PDlcops     /* ignore 2nd cache ops for now.    */
    jal     cm_r4400_config_lcoptab /* setup logical cache op table.    */
    la      t0, lcoptab             /* now initialize the 2nd cache ops */
    la      t1, unsupported_cacheop /* with unsupported cache op - nop  */
    addiu   t0, (PIlcops+PDlcops)<<2 /* the entry point of the 2nd      */
    li      t2, SIDlcops            /* loadup the sid lcops counter.    */
12: sw      t1, 0(t0)               /* initialize the table entries.    */
    sub     t2, 1                   /* decrement the lcop counter.      */
    addiu   t0, 4                   /* point to next entries.           */
    bgtz    t2, 12b                 /* loop for SIDlcops entries.       */
/*
 *  Initialize the flush all pointer
 */
    la      t2, cm_r5000_flush_all  /* initialize the flush all pointer */
    sw      t2, _flushAllCache      /* initialize the variable.         */
    la      t1, cm_r5000_cache_valid /* load the validate check func    */
    sw      t1, _validCacheAddr     /* and initialize the func pointer  */
    li      t2, FAKE2ND             /* r5k has a faked 2nd cache.       */
    sw      t2, _scache_type        /* initialize the cache type.       */
/*
 *  Done!!
 */
    .set    noat                    /* disable AT macro replacement.    */
    ld	    AT, regsave-0x00(sp)    /* save AT register.                */
    .set    at                      /* enable AT macro replacement.     */
    ld	    ra, regsave-0x08(sp)    /* save return address.             */
    ld	    a0, regsave-0x10(sp)    /* save a0                          */
    ld	    t0, regsave-0x18(sp)    /* save t0                          */
    ld	    t1, regsave-0x20(sp)    /* save t1                          */
    ld	    t2, regsave-0x28(sp)    /* save t2                          */
    addiu   sp, framesize           /* create current stack frame.      */
    jr      ra
    END(r5000_cache_config)


/********************
 * cm_r5000_flush_all
 * ------------------
 * Initialize cache variables for primary caches.
 */
LEAF(cm_r5000_flush_all)
    sub     sp, framesize           /* create current stack frame.      */
    sd      t0, regsave-0x08(sp)    /* save t0                          */
    sd      t1, regsave-0x10(sp)    /* save t1                          */
    sd      t2, regsave-0x18(sp)    /* save t2                          */
    sd      t3, regsave-0x20(sp)    /* save t3                          */
    sd      t4, regsave-0x28(sp)    /* save t4                          */
/*
 *  jump to kseg1 base
 */
    la      t0, 20f                 /* load target address.             */
    lui     t1, K1BASE>>16          /* load the Kseg Base address       */
    or      t0, t1                  /* form the uncache address.        */
    jr      t0                      /* branch to uncached addr sequence */
/*
 *  Flush icache
 */
20: lw      t0, _icache_size        /* load the icache size.            */
    lw      t1, _icache_linesize    /* load the icache line size.       */
25: sub     t0, t1                  /* point to previous line.          */
    bltz    t0, 30f                 /* are we done yet ??               */
    lui     t2, K0BASE>>16          /* load the KSEG0 base address      */
    or      t2, t0                  /* this is the cache line index     */
    .set    noreorder               /* disable assembler reordering     */
    b       25b                     /* go back for next line.           */
    cache   CACH_PI|C_IINV, 0(t2)   /* use index writeback invalidate   */
    .set    reorder                 /* enable assembler reordering      */
/*
 *  Flush dcache
 */
30: lw      t0, _dcache_size        /* load the dcache size.            */
    lw      t1, _dcache_linesize    /* load the dcache line size        */
    lw      t3, _sidcache_size      /* load the 2nd cache size          */
    lw      t4, _scache_linesize    /* and 2nd cache line size.         */
35: sub     t0, t1                  /* point to last line in dcache.    */
    bltz    t0, 40f                 /* are we done yet??                */
    lui     t2, K0BASE>>16          /* load the KSEG0 base address      */
    or      t2, t0                  /* this is the cache line index     */
    .set    noreorder               /* disable assembler reordering     */
    b       35b                     /* go back for next line.           */
    cache   CACH_PD|C_IWBINV, 0(t2) /* use index writeback invalidate   */
    .set    reorder                 /* enable assembler reordering      */
/*
 *  Dcone!!
 */
40: ld      t0, regsave-0x08(sp)    /* load t0                          */
    ld      t1, regsave-0x10(sp)    /* load t1                          */
    ld      t2, regsave-0x18(sp)    /* load t2                          */
    ld      t3, regsave-0x20(sp)    /* load t3                          */
    ld      t4, regsave-0x28(sp)    /* load t4                          */
    addiu   sp, framesize           /* restore previous jstack frame    */
    jr      ra                      /* return                           */
    END(cm_r5000_flush_all)

/***********************
 * cm_r5000_cache_valid
 * ---------------------
 * Return 1 if there is a valid cache line match the given VA
 *        0 if there is no match in all 4 caches.
 * Note: We don't care about the secondary cache in R5000 because
 *  its a write back cache, there is no conflict between memory/2nd
 *  cache.
 */
NESTED(cm_r5000_cache_valid,framesize,ra)
    subu    sp, framesize           /* create current stack frame.      */
    .set    noat                    /* disable AT macro replacement.    */
    sd	    AT, regsave-0x00(sp)    /* save AT register.                */
    .set    at                      /* enable AT macro replacement.     */
    sd	    ra, regsave-0x08(sp)    /* save return address.             */
    sd	    t0, regsave-0x10(sp)    /* save t0                          */
    sd	    t1, regsave-0x18(sp)    /* save t1                          */
    sd	    t2, regsave-0x20(sp)    /* save t2                          */
    sd	    a0, regsave-0x28(sp)    /* save a0                          */
/*
 *  Check primary cache set 0
 */
    lw      t0, _dcache_linemask    /* read the line size.              */
    not     t0                      /* create the line address mask     */
    and     t1, a0, t0              /* create line address.             */
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_PD|C_ILT, 0(t1)    /* read the set0 Tags.              */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    mfc0    t0, C0_TAGLO            /* read the taglo register.         */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    .set    reorder                 /* enable assembler reordering      */
    srl     t0, 6                   /* check the PState                 */
    andi    t2, t0, 3               /* isolate the State bits.          */
    beqz    t2, 10f                 /* check set 1 if invalid           */
    srl     t0, 2                   /* Physical addr<35:12>             */
    sll     t2, a0 3                /* remove upper 3 bits of the VA    */
    srl     t2, 15                  /* and extract ADDR<35:12>          */
    bne     t0, t2, 10f             /* does not match check set 1       */
    li      v0, 1                   /* there is a match in set 0        */
    b       30f                     /* done, return to caller.          */
/*
 *  Check primary cache set 1
 */
10: lw      t0, _two_set_pcaches    /* get ther set size                */
    or      t1, t0                  /* create 2nd set index.            */
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_PD|C_ILT, 0(t1)    /* read the set1 Tags.              */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    mfc0    t0, C0_TAGLO            /* read the taglo register.         */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    .set    reorder                 /* enable assembler reordering      */
    srl     t0, 6                   /* check the PState                 */
    andi    t2, t0, 3               /* isolate the State bits.          */
    beqz    t2, 20f                 /* check 2nd set 0 if invalid       */
    srl     t0, 2                   /* Physical addr<35:12>             */
    sll     t2, a0 3                /* remove upper 3 bits of the VA    */
    srl     t2, 15                  /* and extract ADDR<35:12>          */
    bne     t0, t2, 20f             /* does not match check set 1       */
    li      v0, 1                   /* there is a match in set 0        */
    b       30f                     /* done, return to caller.          */
                                    /* There is no matched valid cache  */
20: move    v0, zero                /* line in neither cahce.           */
                                    /* return 0                         */
/*
 *  We are done, v0 should have the return value
 */
    .set    noat                    /* disable AT macro replacement.    */
30: ld	    AT, regsave-0x00(sp)    /* save AT register.                */
    .set    at                      /* enable AT macro replacement.     */
    ld	    ra, regsave-0x08(sp)    /* save return address.             */
    ld	    t0, regsave-0x10(sp)    /* save t0                          */
    ld	    t1, regsave-0x18(sp)    /* save t1                          */
    ld	    t2, regsave-0x20(sp)    /* save t2                          */
    ld	    a0, regsave-0x28(sp)    /* save a0                          */
    addiu   sp, framesize           /* create current stack frame.      */
    jr      ra                      /* return                           */
    END(cm_r5000_cache_valid)

/********************************************************************\
|*       LOW LEVEL CACHE OPS                                        *|
\********************************************************************/

/* END OF FILE */
/* SECONDART DATA/INSTRUCTION CACHE		*/
/* PROM NEVER TURN ON R5000 2ND CACHE ANYWAY -  */
/* LATER					*/
/* END OF FILE */
