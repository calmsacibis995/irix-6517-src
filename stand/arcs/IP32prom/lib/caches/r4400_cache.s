
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

#include <caches.h>
#include <sys/asm.h>
#include <sys/cpu.h>
#include <sys/regdef.h>
#include <sys/sbd.h>


/********************
 * cm_r4400_flush_all
 * ------------------
 * Initialize cache variables for primary caches.
 */
LEAF(cm_r4400_flush_all)
    subu    sp, framesize           /* create current stack frame.      */
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
 *  Flush 2nd cache - if there is one,
 */
40: move    t0, t3                  /* pass alone the scache size       */
    beqz    t3, 50f                 /* do we have a 2nd cache??         */
45: sub     t0, t4                  /* point to the last line in cache  */
    bltz    t0, 50f                 /* are we done yet??                */
    lui     t2, K0BASE>>16          /* load the KSEG0 base address      */
    or      t2, t0                  /* this is the cache line index     */
    .set    noreorder               /* disable assembler reordering     */
    b       45b                     /* go back for next line.           */
    cache   CACH_SD|C_IWBINV, 0(t2) /* use index writeback invalidate   */
    .set    reorder                 /* enable assembler reordering      */
/*
 *  Dcone!!
 */
50: ld      t0, regsave-0x08(sp)    /* load t0                          */
    ld      t1, regsave-0x10(sp)    /* load t1                          */
    ld      t2, regsave-0x18(sp)    /* load t2                          */
    ld      t3, regsave-0x20(sp)    /* load t3                          */
    ld      t4, regsave-0x28(sp)    /* load t4                          */
    addiu   sp, framesize           /* restore previous jstack frame    */
    jr      ra                      /* return                           */
    END(cm_r4400_flush_all)


/************************
 * cm_r4400_pcache_config
 * ----------------------
 * Initialize cache variables for primary caches.
 */
LEAF(cm_r4400_pcache_config)

/*
 *  Setup primary instruction cache stuff ...
 *  CacheSize - Hardware hardwired to 32K bytes, but we'll use config
 *              register to figure it out.
 *  LineSize  - Hardware hardwired to 64  bytes, this we have no way to do
 *              it in the run time.
 */
    andi    t1, a0, CONFIG_IC       /* Extract ln2(_picache_size/4096)  */
    srl	    t1, CONFIG_IC_SHFT      /* extract the pi cache size.       */
    li	    t2, 0x1000              /* load the base cache size.        */
    sllv    t2, t1                  /* we have _icache_size.            */
    sw	    t2, _icache_size        /* initialize _icache_size          */
    andi    t1, a0, CONFIG_IB       /* Extract picache blocksize bit    */
    li      t2, 32                  /* max r4k icache block size.       */
    bnez    t1, 10f                 /* Is it 32 bytes line size ??      */
    li      t2, 16                  /* no, its 16 bytes.                */
10: sw	    t2, _icache_linesize    /* initialize _icache_linesize      */
    sub     t1, t2, 1               /* setup icache line mask           */
    sw	    t1, _icache_linemask    /* mask is linesize - 1             */

/*
 *  Setup primary data cache stuff ...
 *  CacheSize - Hardware hardwired to 32K bytes, but we'll use config
 *              register to figure it out.
 *  LineSize  - Hardware hardwired to 32  bytes, this we have no way to do
 *              it in the run time.
 */
    andi    t1, a0, CONFIG_DC       /* Extract ln2(_picache_size/4096)  */
    srl	    t1, CONFIG_DC_SHFT      /* extract the pi cache size.       */
    li	    t2, 0x1000              /* load the base cache size.        */
    sllv    t2, t1                  /* we have _icache_size.            */
    sw	    t2, _dcache_size        /* initialize _icache_size          */
    andi    t1, a0, CONFIG_DB       /* Extract picache blocksize bit    */
    li      t2, 32                  /* max r4k icache block size.       */
    bnez    t1, 10f                 /* Is it 32 bytes line size ??      */
    li      t2, 16                  /* no, its 16 bytes.                */
10: sw	    t2, _dcache_linesize    /* initialize _icache_linesize      */
    sub     t1, t2, 1               /* setup icache line mask           */
    sw	    t1, _dcache_linemask    /* mask is linesize - 1             */
    sw	    t1, dmabuf_linemask     /* Use prmary mask as dma mask      */

/*
 *  Done!
 */
    jr      ra                      /* return                           */
    END(cm_r4400_pcache_config)


/************************
 * cm_r4400_scache_config
 * ----------------------
 * Initialize cache variables for secondary cache.
 */
LEAF(cm_r4400_scache_config)

/*
 *  Switch to Kseg1 address space.
 */
    la      t1, 9f                  /* load the target address.         */
    lui     t2, K1BASE>>16          /* load 0xa0000000                  */
    or      t1, t1, t2              /* do the switch                    */
    jr      t1                      /* after the jr we in kseg1         */
    
/*
 *  Setup secondary cache stuff.
 *  following code have run uncached.
 *  CacheSize  - use configuration register to figure out scache size.
 *  LineSize   - use configuration register to figure out scache line size.
 */
9:  li      t1, CONFIG_SC           /* check for the secondary cache    */
    and     t1, a0                  /* Is there a secondary cache.      */
    bnez    t1, 13f                 /* No secondary cache.              */
    lui     t2, CONFIG_SB>>16       /* Check the 2nd cache line size.   */
    and     t2, a0, t2              /* got it.                          */
    srl     t2, CONFIG_SB_SHFT      /* shift in right place.            */
    li      t1, 4                   /* the base block size.             */
    sllv    t1, t2                  /* got the 2nd line size in t1.     */
    sw	    t1, _scache_linesize    /* initialize _scache_linesize.     */
    sub     t2, t1, 1               /* create the line mask.            */
    sw      t2, _scache_linemask    /* initialize it.                   */
    sw	    t2, dmabuf_linemask     /* set dmabuf_linemask              */
    .set    noreorder               /* turn off assembler reordering    */
    mtc0    zero, C0_TAGHI          /* initialize taghi register.       */
    lui	    t1, K0BASE>>16          /* setup the base address.          */
    cache   CACH_PI|C_IINV, 0(t1)   /* invalid the indexed line.        */
    nop                             /* one nop is required.             */
    cache   CACH_PD|C_IWBINV, 0(t1) /* invalid the indexed line.        */
    srl     t2, t1, 12              /* create TagLo                     */
    cache   CACH_SD|C_IWBINV, 0(t1) /* invalid the indexed line.        */
    sll     t2, 8                   /* move to bit<31:8>                */
    mtc0    t2, C0_TAGLO            /* initialize TagLo register.       */
    nop                             /* one nop is required.             */
    cache   CACH_SD|C_IST, 0(t1)    /* write pattern to cache tag ram.  */
    .set    noat                    /* disable AT macro replacement.    */
    lui     AT, S512K>>16           /* the base size of scache.         */
10: addu    t1, AT                  /* Create running index.            */
    nop                             /* one nop is required.             */
    .set    at                      /* enable AT macro replacement.     */
    cache   CACH_PD|C_IWBINV, 0(t1) /* invalid the indexed line.        */
    srl     t0, t1, 17              /* create TagLo                     */
    cache   CACH_SD|C_IWBINV, 0(t1) /* invalid the indexed line.        */
    sll     t0, 13                  /* move to bit<31:8>                */
    mtc0    t0, C0_TAGLO            /* initialize TagLo register.       */
    nop                             /* one nop is required.             */
    nop                             /* two nop is required.             */
    cache   CACH_SD|C_IST, 0(t1)    /* write pattern to cache tag ram.  */
    nop                             /* one nop is required.             */
    nop                             /* 2nd nop is required.             */
    .set    noat                    /* disable AT macro replacement.    */
    lui     AT, K0BASE>>16          /* read the first tag entry.        */
    cache   CACH_SD|C_ILT, 0(AT)    /* at 0x80000000                    */
    nop                             /* one nop is required.             */
    nop                             /* 2nd nop is required.             */
    mfc0    t2, C0_TAGLO            /* read from TagLo register.        */
    nop                             /* one nop is required.             */
    sltu    t2, t2, t0              /* scache size in t1 when following */
    bnez    t2, 10b                 /* branch is not taken.             */
    lui     AT, S512K>>16           /* the base size of scache.         */
    .set    at                      /* enable AT macro replacement.     */
    .set    reorder                 /* enable assembler reordering.     */
    lui     t2, K0BASE>>16          /* load 0x80000000                  */
    xor     t1, t2                  /* extract scache size.             */
    sw      t1, _sidcache_size      /* initialize scache size variable. */

/*
 *  Done!!
 */
13: jr      ra 
    END(cm_r4400_scache_config)
    
    
/*************************
 * cm_r4400_config_lcoptab
 * -----------------------
 * Initialize cache variables for secondary cache.
 */
LEAF(cm_r4400_config_lcoptab)

/*
 *  Setup primary icache logical cache ops.
 */
    .set    noat                    /* Disable AT macro replacement.    */
    la      t0, lcoptab             /* load the logical cache table.    */
    la      t1, r4400_picache_tab   /* Setup primary icache.            */
    move    t2, a0                  /* How many cop entries in lcoptab  */
10: lw      AT, 0(t1)               /* read the cacheops routine ptr    */
    sub     t2, 1                   /* descrment cacheop counter.       */
    sw      AT, 0(t0)               /* write to logical cacheop table   */
    .set    at                      /* enable AT macro replacement      */
    addiu   t1, 4                   /* point to next cache ops.         */
    addiu   t0, 4                   /* point to next entry in the table */
    bgtz    t2, 10b                 /* loop for 7 entries.              */

/*
 *  Done!!
 */
    jr      ra
    END(cm_r4400_config_lcoptab)


/********************
 * r4400_cache_config
 * ------------------
 * Initialize cache data structures
 */
.globl  r4400_cache_config
.ent	r4400_cache_config
r4400_cache_config:
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
 *  Loading up lcop table.
 */
    .set    noreorder               /* turn off assembler reordering    */
    mfc0    a0, C0_CONFIG           /* read the config register.        */
    nop                             /* one nop just in case.            */
    .set    reorder                 /* turn on  assembler reordering    */
    bal     cm_r4400_pcache_config  /* configure the pdcache.           */
    bal     cm_r4400_scache_config  /* configure the 2nd cache.         */
    li      a0, PIlcops+PDlcops+SIDlcops /* loadup all the cache ops    */
    bal     cm_r4400_config_lcoptab /* setup logical cache op table.    */
    la      t2, cm_r4400_flush_all  /* initialize the flush all pointer */
    sw      t2, _flushAllCache      /* initialize the variable.         */
    la      t1, cm_r4400_cache_valid /* load the cache validate check   */
    sw      t1, _validCacheAddr     /* and initialize the func pointer. */
    li      t2, UNIFYWB2ND          /* r4k has a writeback unified 2nd  */
    sw      t2, _scache_type        /* initialize the cache type.       */
/*
 *  Done
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
    END(r4400_cache_config)


/***********************
 * cm_r4400_cache_valid
 * ---------------------
 * Return 1 if there is a valid cache line match the given VA
 *        0 if there is no match in all 4 caches.
 * Note: We don't care about the secondary cache in R5000 because
 *  its a write back cache, there is no conflict between memory/2nd
 *  cache.
 */
NESTED(cm_r4400_cache_valid,framesize,ra)
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
 *  Check primary cache
 */
    lw      t0, _dcache_linemask    /* read the line size.              */
    not     t0                      /* create the line address mask     */
    and     t1, a0, t0              /* create line address.             */
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_PD|C_ILT, 0(t1)    /* read the Tags                    */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    mfc0    t0, C0_TAGLO            /* read the taglo register.         */
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
 *  Check secondary cache if there is one.
 */
10: lw      t0, _sidcache_size      /* check if there is a 2nd cache    */
    beqz    t0, 20f                 /* return 0, there is no 2nd cache  */
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_PD|C_ILT, 1(t1)    /* read the set1 Tags.              */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    mfc0    t0, C0_TAGLO            /* read the taglo register.         */
    .set    reorder                 /* enable assembler reordering      */
    srl     t0, 10                  /* check the SState                 */
    andi    t2, t0, 3               /* isolate the State bits.          */
    beqz    t2, 20f                 /* check 2nd set 0 if invalid       */
    srl     t0, 2                   /* Physical addr<35:12>             */
    sll     t2, a0 3                /* remove upper 3 bits of the VA    */
    srl     t2, 20                  /* and extract ADDR<35:17>          */
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
    END(cm_r4400_cache_valid)

/********************************************************************\
|*       LOW LEVEL CACHE OPS                                        *|
\********************************************************************/

/* UNSUPPORTED CACHES OP        */

/*********************
 * unsupported_cacheop
 * -------------------
 */
LEAF(unsupported_cacheop)
    add     zero, zero, zero
    jr      ra                     
    END(unsupported_cacheop)


/* PRIMARY INSTRUCTION CACHE    */

/*********************
 * pi_index_invalidate
 * -------------------
 * Primary instruction cache index invalidate
 */
LEAF(pi_index_invalidate)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _icache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PI|C_IINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pi_index_invalidate)

/*******************   
 * pi_index_load_tag  
 * -----------------  
 * Primary instruction cache index load tag   
 */
LEAF(pi_index_load_tag)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _icache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PI|C_ILT, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pi_index_load_tag)

/********************   
 * pi_index_store_tag  
 * ------------------  
 * Primary instruction cache index store tag   
 */
LEAF(pi_index_store_tag)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _icache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PI|C_IST, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pi_index_store_tag)

/*******************
 * pi_hit_invalidate
 * -----------------
 * Primary instruction cache hit invalidate
 */
LEAF(pi_hit_invalidate)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _icache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PI|C_HINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pi_hit_invalidate)

/**********
 * pi_ifill
 * --------
 * Primary instruction cache fill
 */
LEAF(pi_ifill)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _icache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PI|C_FILL, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pi_ifill)

/******************
 * pi_hit_writeback
 * ----------------
 * Primary instruction cache hit writeback
 */
LEAF(pi_hit_writeback)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _icache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PI|C_FILL, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pi_hit_writeback)



/* PRIMARY DATA CACHE           */

/***********************
 * pd_index_wb_invaldate
 * ---------------------
 * Primary data cache hit writeback invalidate
 */
LEAF(pd_index_wb_invaldate)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _dcache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PD|C_IWBINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pd_index_wb_invaldate)

/*******************
 * pd_index_load_tag
 * -----------------
 * Primary data cache index load tag
 */
LEAF(pd_index_load_tag)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _dcache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PD|C_ILT, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pd_index_load_tag)

/********************
 * pd_index_store_tag
 * ------------------
 * Primary data cache index store tag
 */
LEAF(pd_index_store_tag)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _dcache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PD|C_IST, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pd_index_store_tag)

/***************************
 * pd_create_dirty_exclusive
 * -------------------------
 * Primary data cache create dirty exclusive
 */
LEAF(pd_create_dirty_exclusive)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _dcache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PD|C_CDX, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pd_create_dirty_exclusive)

/*******************
 * pd_hit_invalidate
 * -----------------
 * Primary data cache hit invalidate
 */
LEAF(pd_hit_invalidate)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _dcache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PD|C_HINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pd_hit_invalidate)

/**********************
 * pd_hit_wb_invalidate
 * --------------------
 * Primary data cache hit writeback invalidate
 */
LEAF(pd_hit_wb_invalidate)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _dcache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PD|C_HWBINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pd_hit_wb_invalidate)

/******************
 * pd_hit_writeback
 * ----------------
 * Primary data cache hit writeback
 */
LEAF(pd_hit_writeback)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _dcache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_PD|C_HWBINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(pd_hit_writeback)



/* SECONDART DATA/INSTRUCTION CACHE           */

/***********************
 * sd_index_wb_invaldate
 * ---------------------
 * Primary data cache hit writeback invalidate
 */
LEAF(sd_index_wb_invaldate)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _scache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_SD|C_IWBINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(sd_index_wb_invaldate)

/*******************
 * sd_index_load_tag
 * -----------------
 * Primary data cache index load tag
 */
LEAF(sd_index_load_tag)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _scache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_SD|C_ILT, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(sd_index_load_tag)

/********************
 * sd_index_store_tag
 * ------------------
 * Primary data cache index store tag
 */
LEAF(sd_index_store_tag)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _scache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_SD|C_IST, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(sd_index_store_tag)

/***************************
 * sd_create_dirty_exclusive
 * -------------------------
 * Primary data cache create dirty exclusive
 */
LEAF(sd_create_dirty_exclusive)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _scache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_SD|C_CDX, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(sd_create_dirty_exclusive)

/*******************
 * sd_hit_invalidate
 * -----------------
 * Primary data cache hit invalidate
 */
LEAF(sd_hit_invalidate)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _scache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_SD|C_HINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(sd_hit_invalidate)

/**********************
 * sd_hit_wb_invalidate
 * --------------------
 * Primary data cache hit writeback invalidate
 */
LEAF(sd_hit_wb_invalidate)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _scache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_SD|C_HWBINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(sd_hit_wb_invalidate)

/******************
 * sd_hit_writeback
 * ----------------
 * Primary data cache hit writeback
 */
LEAF(sd_hit_writeback)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _scache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_SD|C_HWBINV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(sd_hit_writeback)

/********************
 * sd_hit_set_virtual
 * ------------------
 * Primary data cache hit set virtual index
 */
LEAF(sd_hit_set_virtual)
    subu    sp, framesize
    sd      t0, regsave-0x08(sp)
    lw      t0, _scache_linemask
    not     t0
    and     t0, a0
    .set    noreorder
    cache   CACH_SD|C_HSV, 0x0(t0)
    .set    reorder
    ld      t0, regsave-0x08(sp)
    addiu   sp, framesize
    jr      ra                     
    END(sd_hit_set_virtual)
    .set    reorder


/********************************************************************\
|*       Logical CACHE op tables.                                   *|
\********************************************************************/

/* PRIMARY INSTRUCTION CACHE    */
.data
.globl r4400_picache_tab
r4400_picache_tab:
    .word  pi_index_invalidate
    .word  pi_index_load_tag
    .word  pi_index_store_tag
    .word  unsupported_cacheop
    .word  pi_hit_invalidate
    .word  pi_ifill
    .word  pi_hit_writeback
    .word  unsupported_cacheop
    .word  unsupported_cacheop
    .word  unsupported_cacheop
    
/* PRIMARY DATA CACHE           */
.globl r4400_pdcache_tab
r4400_pdcache_tab:
    .word  pd_index_wb_invaldate
    .word  pd_index_load_tag
    .word  pd_index_store_tag
    .word  pd_create_dirty_exclusive
    .word  pd_hit_invalidate
    .word  pd_hit_wb_invalidate
    .word  pd_hit_writeback
    .word  unsupported_cacheop
    .word  unsupported_cacheop
    .word  unsupported_cacheop

/* SECONDARY DATA/INSTRUCTION CACHE */
.globl r4400_sidcache_tab
r4400_sidcache_tab:
    .word  sd_index_wb_invaldate
    .word  sd_index_load_tag
    .word  sd_index_store_tag
    .word  sd_create_dirty_exclusive
    .word  sd_hit_invalidate
    .word  sd_hit_wb_invalidate
    .word  sd_hit_writeback
    .word  sd_hit_set_virtual
    .word  unsupported_cacheop
    .word  unsupported_cacheop
