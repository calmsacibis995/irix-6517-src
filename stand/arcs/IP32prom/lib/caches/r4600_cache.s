
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


/********************
 * r4600_cache_config
 * ------------------
 * Initialize cache data structures
 */
NESTED(r4600_cache_config,framesize,ra)
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
                                    /* we do not support 2nd cache in   */
                                    /* R4600.                           */
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
 *  initialize the flash all pointer
 */
    la      t2, cm_r5000_flush_all  /* initialize the flush all pointer */
    sw      t2, _flushAllCache      /* initialize the global pointer    */
    la      t1, cm_r5000_cache_valid/* load the cache validate check    */
    sw      t1, _validCacheAddr     /* func and initialize the func ptr */
    li      t2, FAKE2ND             /* r4600 has a faked 2nd cache.     */
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
    END(r4600_cache_config)


/********************************************************************\
|*       LOW LEVEL CACHE OPS                                        *|
\********************************************************************/

/* SECONDART DATA/INSTRUCTION CACHE           */
/* LATER    */
/* END OF FILE */
