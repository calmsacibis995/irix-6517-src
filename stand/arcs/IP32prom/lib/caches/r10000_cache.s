
/**************************************************************************
 *                                                                        *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/asm.h>
#include <sys/cpu.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <caches.h>

#define C0_IMP_R10000		0x9
#define	C0_IMPMASK		0xff00
#define	C0_IMPSHIFT		8
#define	R10KPIBLK		64
#define	R10KPDBLK		32
#define	R10KPCACH		0x8000
#define	R10KPCSET		0x4000
#define	R10K_PI_SHIFT		29
#define	R10K_PI_MASK		7
#define	R10K_PD_SHIFT		26
#define	R10K_PD_MASK		7
#define	R10K_SS_SHIFT		16
#define	R10K_SS_MASK		7
#define	R10K_SB_SHIFT		13
#define	R10K_SB_MASK 		1
#define	C_BARRIER               0x14
#define	C_ILD                   0x18
#define	C_ISD                   0x1c

/* #undef DEBUGR10K		*/
/* #define DEBUGR10K 1          */
#if defined(DEBUGR10K)
/*******************
 * Save the scache set size.
 */
LEAF(SaveSETsize)
    .set    noreorder               /* disable reordering.              */
    mtc0    a0, C0_LLADDR           /* clear taghi reg.                 */
    nop
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(SaveSETsize)
    
/*******************
 * Retrive saved scache set size.
 */
LEAF(RetriveSETsize)
    .set    noreorder               /* disable reordering.              */
    mfc0    v0, C0_LLADDR           /* clear taghi reg.                 */
    nop
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(RetriveSETsize)
    
/*******************
 * Invalidate the cache line
 */
LEAF(ClearTagHiLo)
    .set    noreorder               /* disable reordering.              */
    mtc0    zero, C0_TAGHI          /* clear taghi reg.                 */
    mtc0    zero, C0_TAGLO          /* clear taglo reg.                 */
    nop
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(Pinvalid)
    
/*******************
 * Invalidate the cache line
 */
LEAF(Pinvalid)
    .set    noreorder               /* disable reordering.              */
    cache   CACH_PD|C_IWBINV,0(a0)  /* done!                            */
    nop
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(Pinvalid)
    
/*******************
 * Initialize secondary cache tags
 */
LEAF(InitData)
    .set    noreorder               /* disable reordering.              */
    mtc0    a0, C0_TAGLO            /* initialize TagLo register.       */
    mtc0    a1, C0_TAGHI            /* initialize TagHi register.       */
    mtc0    zero, C0_ECC            /* 0 nop                            */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    nop                             /* 3 nop                            */
    cache   CACH_SD|C_ISD, 0(a2)    /* initialize cache tag.            */
    nop                             /* 4 nop                            */
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(InitData)
    
/*******************
 * Load secondary cache data
 */
LEAF(LoadData)
    .set    noreorder               /* disable reordering.              */
    cache   CACH_SD|C_ILD, 0(a0)    /* initialize cache tag.            */
    nop                             /* 4 nop                            */
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(InitData)
    
/*******************
 * Initialize secondary cache tags
 */
LEAF(InitTag)
    .set    noreorder               /* disable reordering.              */
    mtc0    a0, C0_TAGLO            /* initialize TagLo register.       */
    mtc0    a1, C0_TAGHI            /* initialize TagHi register.       */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    nop                             /* 3 nop                            */
    cache   CACH_SD|C_IST, 0(a2)    /* initialize cache tag.            */
    nop                             /* 4 nop                            */
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(InitTag)
    
/*******************
 * Initialize secondary cache tags
 */
LEAF(InitPTag)
    .set    noreorder               /* disable reordering.              */
    mtc0    a0, C0_TAGLO            /* initialize TagLo register.       */
    mtc0    a1, C0_TAGHI            /* initialize TagHi register.       */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    nop                             /* 3 nop                            */
    cache   CACH_PD|C_IST, 0(a2)    /* initialize cache tag.            */
    nop                             /* 4 nop                            */
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(InitPTag)
    
/*******************
 * Load secondary cache tags
 */
LEAF(LoadTag)
    .set    noreorder               /* disable reordering.              */
    cache   CACH_SD|C_ILT, 0(a0)    /* initialize cache tag.            */
    nop                             /* 1 nop                            */
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(LoadTag)
    
/*******************
 * Load secondary cache tags
 */
LEAF(LoadPTag)
    .set    noreorder               /* disable reordering.              */
    cache   CACH_PD|C_ILT, 0(a0)    /* initialize cache tag.            */
    nop                             /* 1 nop                            */
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(LoadPTag)
    
/***********
 * readTagLo
 */
LEAF(readTagLo)
    .set    noreorder               /* disable reordering.              */
    mfc0    v0, C0_TAGLO            /* read TagLo register.             */
    nop                             /* 1 nop                            */
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(readTagLo)
    
/***********
 * readTagHi
 */
LEAF(readTagHi)
    .set    noreorder               /* disable reordering.              */
    mfc0    v0, C0_TAGHI            /* read TagLo register.             */
    nop                             /* 1 nop                            */
    .set    reorder                 /* disable reordering.              */
    jr      ra                      /* return                           */
    END(readTagHi)
    

/******************
 * get_scache_ssize
 */
LEAF(get_scache_ssize)
    .set    noreorder               /* disable reordering.              */
    mfc0    a0, C0_CONFIG           /* read the config register.        */
    nop                             /* give 2 nops.                     */
    nop                             /* give 2 nops.                     */
    srl	    a0, a0, R10K_SS_SHIFT   /* extract scache size.             */
    andi    a0, R10K_SS_MASK        /* got the scache size.             */
    sltiu   v0, a0, 6               /* check the unsupported cache size.*/
11: beqz    v0, 11b                 /* the mode bits are wrong.         */
    sub     a0, 1                   /* branch delay slot.               */
    lui	    v0, S512K>>16           /* load the base cache size.        */
    jr	    ra                      /* Return with cache set size.      */
    sllv    v0, v0, a0              /* done! scache size in t1          */
    .set    reorder                 /* enable  reordering.              */
    END(get_scache_ssize)
    
#endif
#undef DEBUGR10K

/*******************
 * r10k_config_cache
 * -----------------
 * Initialize cache data structures
 */
NESTED(r10000_cache_config,framesize,ra)
    subu    sp, framesize           /* create current stack frame.      */
    .set    noat                    /* disable AT macro replacement.    */
    sd	    AT, regsave-0x00(sp)    /* save AT register.                */
    .set    at                      /* enable AT macro replacement.     */
    sd	    ra, regsave-0x08(sp)    /* save return address.             */
    sd	    t0, regsave-0x10(sp)    /* save t0                          */
    sd	    t1, regsave-0x18(sp)    /* save t1                          */
    .set    noreorder               /* disable assembler reordering     */
    mfc0    t0, C0_CONFIG           /* read the configuration register  */
    .set    reorder                 /* enable assembler reordering.     */
    sd	    t2, regsave-0x20(sp)    /* save t2                          */
    sd	    a0, regsave-0x28(sp)    /* save a0                          */
    
cm_r10000:
/*
 *  Setup primary instruction cache stuff ...
 *  CacheSize - Hardware hardwired to 32K bytes, but we'll use config
 *              register to figure it out.
 *  LineSize  - Hardware hardwired to 64  bytes, this we have no way to do
 *              it in the run time.
 */
    srl	    t1, t0, R10K_PI_SHIFT   /* extract the pi cache size.       */
    andi    t1, R10K_PI_MASK        /* got the size.                    */
    li	    t2, 0x1000              /* load the base cache size.        */
    sllv    t2, t1                  /* we have _icache_size.            */
    sw	    t2, _icache_size        /* initialize _icache_size          */
    li	    t1, R10KPIBLK           /* setup icache line size.          */
    sw	    t1, _icache_linesize    /* initialize _icache_linesize      */
    subu    t2, t1, 1               /* setup icache line mask           */
    sw	    t2, _icache_linemask    /* mask is linesize - 1             */
    
/*
 *  Setup primary data cache stuff ...
 *  CacheSize - Hardware hardwired to 32K bytes, but we'll use config
 *              register to figure it out.
 *  LineSize  - Hardware hardwired to 32  bytes, this we have no way to do
 *              it in the run time.
 */
    srl	    t1, t0, R10K_PD_SHIFT   /* extract the pi cache size.       */
    andi    t1, R10K_PD_MASK        /* got the size.                    */
    li	    t2, 0x1000              /* load the base cache size.        */
    sllv    t2, t1                  /* we have _dcache_size.            */
    sw	    t2, _dcache_size        /* initialize _dcache_size          */
    li	    t1, R10KPDBLK           /* setup dcache line size.          */
    sw	    t1, _dcache_linesize    /* initialize _dcache_linesize      */
    subu    t2, t1, 1               /* setup icache line mask           */
    sw	    t2, _dcache_linemask    /* mask is linesize - 1             */
    
/*
 *  Setup secondary cache stuff.
 *  CacheSize  - use configuration register to figure out scache size.
 *  LineSize   - use configuration register to figure out scache line size.
 */
    srl	    t1, t0, R10K_SS_SHIFT   /* extract scache size.             */
    andi    t1, R10K_SS_MASK        /* got the scache size.             */
    sltiu   t2, t1, 6               /* check the unsupported cache size.*/
11: beqz    t2, 11b                 /* the mode bits are wrong.         */
    lui	    t2, S512K>>16           /* load the base cache size.        */
    sllv    t1, t2, t1              /* done! scache size in t1          */
    sw	    t1, _sidcache_size      /* initialize _sidcache_size.       */
    sw	    t1, _r4600sc_sidcache_size /* I don't know why but ...      */
    srl	    t2, t0, R10K_SB_SHIFT   /* extract scache block size.       */
    andi    t2, R10K_SB_MASK        /* got the scache block size.       */
    bgtz    t2, 12f                 /* only two sizes -                 */
    li	    t1, 128                 /*    128 byes or                   */
    li	    t1, 64                  /*    64  bytes.                    */
12: sw	    t1, _scache_linesize    /* initialize _scache_linesize.     */
    subu    t2, t1, 1               /* line mask = linesize -1          */
    sw	    t2, _scache_linemask    /* initialize _scache_linemask.     */
    sw	    t2, dmabuf_linemask     /* set dmabuf_linemask              */

/*
 *  Setup two set pcache variables
 *  SetSize   = cachesize / 2
 */
    lw	    t1, _dcache_size        /* setup dcache first.              */
    srl	    t1, 1                   /* size / 2                         */
    sw	    t1, _two_set_pcaches    /* initialize _two_set_pcaches      */
    lw	    t2, _dcache_size        /* setup dcache first.              */
    srl	    t2, 1                   /* size / 2                         */
    sw	    t2, _two_set_icaches    /* initialize _two_set_icaches      */
    lw	    t1, _sidcache_size      /* setup dcache first.              */
    srl	    t1, 1                   /* size / 2                         */
    sw	    t1, _two_set_scaches    /* initialize _two_set_scaches      */

/*
 * Set lcopTable to point to proper low level cache op table.
 */
cm_r10000_config_lcoptab:
    li      a0, PIlcops+PDlcops+SIDlcops /* copy the complete lcop tabcle */
    jal     cm_r4400_config_lcoptab /* copy the lcop tab to memory.     */
                                    /* FIX PRIMARY INSTRUCTION CACHE    */
    la      t1, lcoptab             /* load the logical cop table.      */
    la      t2, r10000_pi_iinvl     /* fix index invalidate cache op    */
    sw      t2, (pi_iinval <<2)(t1) /* write to the lcoptab             */
    la      t2, r10000_cache_barrier/* fix cache barrier slot.          */
    sw      t2, (pi_barrier<<2)(t1) /* write to the lcoptab             */
    la      t2, r10000_pi_ildata    /* fix index load data slot         */
    sw      t2, (pi_ilddata<<2)(t1) /* write to lcoptab                 */
    la      t2, r10000_pi_isdata    /* fix index store data.            */
    sw      t2, (pi_istdata<<2)(t1) /* write to lcoptab                 */
    la      t2, r10000_pi_fill      /* replace the PI fill cache op     */
    sw      t2, (pi_fill<<2)(t1)    /* write to lcoptab                 */
    la      t2, r10000_pi_hitwb     /* replace the PI hit writeback op  */
    sw      t2, (pi_hwb <<2)(t1)    /* write to lcoptab                 */
                                    /* FIX PRIMARY DATA CACHE           */
    la      t2, r10000_pd_iwbinv    /* replace the create dirty xsive   */
    sw      t2, (pd_iwbinval<<2)(t1)/* write to lcoptab                 */
    la      t2, r10000_pd_cdx       /* replace the create dirty xsive   */
    sw      t2, (pd_cdx <<2)(t1)    /* write to lcoptab                 */
    la      t2, r10000_pd_hitwb     /* replace the hit writeback op     */
    sw      t2, (pd_hwb <<2)(t1)    /* write to lcoptab                 */
    la      t2, r10000_pd_ildata    /* fix index load data slot         */
    sw      t2, (pd_ilddata<<2)(t1) /* write to lcoptab                 */
    la      t2, r10000_pd_isdata    /* fix index store data.            */
    sw      t2, (pd_istdata<<2)(t1) /* write to lcoptab                 */
                                    /* FIX SECONDARY CACHE              */
    lw      a0, _sidcache_size      /* see if we have wnd cache.        */
    beqz    a0, 10f                 /* we do not have 2nd cache.        */
    la      t2, r10000_sid_iwbinv   /* replace the create dirty xsive   */
    sw      t2, (sd_iwbinval<<2)(t1)/* write to lcoptab                 */
    la      t2, r10000_sid_cdx      /* replace the create dirty xsive   */
    sw      t2, (sd_cdx <<2)(t1)    /* write to lcoptab                 */
    la      t2, r10000_sid_hitwb    /* replace the hit writeback op     */
    sw      t2, (sd_hwb <<2)(t1)    /* write to lcoptab                 */
    la      t2, r10000_sid_ildata   /* fix index load data slot         */
    sw      t2, (sd_ilddata<<2)(t1) /* write to lcoptab                 */
    la      t2, r10000_sid_isdata   /* fix index store data.            */
    sw      t2, (sd_istdata<<2)(t1) /* write to lcoptab                 */
    b       12f                     /* done.                            */
10: addiu   t1, PIlcops+PDlcops     /* point to 2nd cache lcop entries. */
    la      t0, unsupported_cacheop /* load the nop address.            */
    li      t2, SIDlcops            /* load the 2nd cache entry counter */
11: sw      t0, 0(t1)               /* zap the cache op pointer.        */
    sub     t2, 1                   /* decrement the counter.           */
    addiu   t1, 4                   /* point to next lcop entry         */
    bgtz    t2, 11b                 /* loop for SIDlcops entries.       */
/*
 *  FIX THE FLUSH POINTER and some other fatel important infos.
 */
12: la      t1, cm_r10000_flush_all /* load the flush all pointer.      */
    sw      t1, _flushAllCache      /* initialize flushall pointer.     */
    la      t2, cm_r10000_cache_valid /* load the validatate check func */
    sw      t2, _validCacheAddr     /* initialize the func pointer.     */
    li      t1, UF2SETWB2ND         /* r10k has a 2 set wb 2nd cache.   */
    sw      t1, _scache_type        /* initialize the cache type.       */
/*
 *  Done!!
 */
    .set    noat                    /* disable AT macro replacement.    */
    ld	    AT, regsave-0x00(sp)    /* load AT register.                */
    .set    at                      /* enable AT macro replacement.     */
    ld	    ra, regsave-0x08(sp)    /* load return address.             */
    ld	    t0, regsave-0x10(sp)    /* load t0                          */
    ld	    t1, regsave-0x18(sp)    /* load t1                          */
    ld	    t2, regsave-0x20(sp)    /* load t2                          */
    ld	    a0, regsave-0x28(sp)    /* load a0                          */
    addiu   sp, framesize           /* restore caller's stack frame.    */
    jr      ra                      /* return                           */
    END(r10000_cache_config)


/*********************
 * cm_r10000_flush_all
 * -------------------
 * Initialize cache data structures
 */
LEAF(cm_r10000_flush_all)
    subu    sp, framesize           /* create current stack frame       */
    sd      t0, regsave-0x08(sp)    /* save t0                          */
    sd      t1, regsave-0x10(sp)    /* save t1                          */
    sd      t2, regsave-0x18(sp)    /* save t2                          */
    sd      t3, regsave-0x20(sp)    /* save t3                          */
    sd      t4, regsave-0x28(sp)    /* save t4                          */
    sd      t5, regsave-0x30(sp)    /* save t5                          */
    sd      t6, regsave-0x38(sp)    /* save t6                          */
/*
 *  run in uncached address space
 */
    la      t0, 9f                  /* load the target address.         */
    lui     t1, K1BASE>>16          /* load the K1BASE address          */
    or      t0, t1                  /* form uncached address            */
    .set    noreorder               /* disable assembler reordering.    */
    cache   CACH_PI|C_BARRIER,0(t0) /* sync here                        */
    .set    reorder                 /* enable assembler reordering      */
    jr      t0                      /* switch to Kseg1 address.         */
/*
 *  Flush icache
 */
9:  lw      t0, _two_set_icaches    /* read the icache size.            */
    lw      t1, _icache_linesize    /* read the icache line size        */
    sub     t2, t0, t1              /* point to last line in set 0      */
10: lui     t4, K0BASE>>16          /* load the Kseg0 base address      */
    or      t4, t2                  /* this is icache line index.       */
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_PI|C_IINV, 0(t4)   /* invalidate the current line      */
    sub     t2, t1                  /* point to next line in the set.   */
    nop                             /* one nop just in case.            */
    cache   CACH_PI|C_IINV, 1(t4)   /* invalidate the current line      */
    nop	                            /* another just in case.            */
    bgez    t2, 10b                 /* loop back for whole set.         */
    cache   CACH_PI|C_BARRIER,0(t4) /* sync here                        */
    .set    reorder                 /* enable assembler reordering      */
/*
 *  Flush dcache
 */
    lw      t0, _two_set_pcaches    /* read the icache size.            */
    lw      t1, _dcache_linesize    /* read the icache line size        */
    lw      t5, _two_set_scaches    /* read the scahce set size         */
    lw      t6, _scache_linesize    /* and save the scache line size    */
    sub     t2, t0, t1              /* point to last line in the set    */
20: lui     t4, K0BASE>>16          /* load the Kseg0 base address      */
    or      t4, t2                  /* this is icache line index.       */
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_PD|C_IWBINV, 0(t4) /* invalidate the current line      */
    sub     t2, t1                  /* point to next line in the set.   */
    nop                             /* one nop just in case.            */
    cache   CACH_PD|C_IWBINV, 1(t4) /* invalidate the current line      */
    nop                             /* one nop just in case.            */
    bgez    t2, 20b                 /* are we done with all sets ??     */
    cache   CACH_PI|C_BARRIER,0(t4) /* sync here                        */
    .set    reorder                 /* enable assembler reordering      */
/*
 *  Flush 2nd cache if there is one
 */
    beqz    t5, 40f                 /* is there a 2nd cache??           */
    sub     t2, t5, t6              /* save the scache set size.        */
30: lui     t4, K0BASE>>16          /* load the Kseg0 base address      */
    or      t4, t2                  /* this is scache line index.       */
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_SD|C_IWBINV, 0(t4) /* invalidate the current line      */
    sub     t2, t6                  /* point to next line in the set.   */
    nop                             /* one nop just in case.            */
    cache   CACH_SD|C_IWBINV, 1(t4) /* invalidate the current line      */
    nop                             /* one nop just in case.            */
    bgez    t2, 30b                 /* are we done with all sets ??     */
    cache   CACH_PI|C_BARRIER,0(t4) /* sync here                        */
    .set    reorder                 /* enable assembler reordering      */
/*
 *  Done
 */
40: ld      t0, regsave-0x08(sp)    /* load t0                          */
    ld      t1, regsave-0x10(sp)    /* load t1                          */
    ld      t2, regsave-0x18(sp)    /* load t2                          */
    ld      t3, regsave-0x20(sp)    /* load t3                          */
    ld      t4, regsave-0x28(sp)    /* load t4                          */
    ld      t5, regsave-0x30(sp)    /* load t5                          */
    ld      t6, regsave-0x38(sp)    /* load t6                          */
    addiu   sp, framesize           /* create current stack frame       */
    jr      ra                      /* return                           */
    END(cm_r10000_flush_all)


/********************************************************************\
|*       LOW LEVEL CACHE OPS                                        *|
\********************************************************************/

/* PRIMARY INSTRUCTION CACHE OP */

/*****************
 * r10000_pi_iinvl
 * ---------------
 * index invalidate for both set.
 */
LEAF(r10000_pi_iinvl)
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_PI|C_IINV, 1(a0)   /* invalidate set 0                 */
    nop                             /* one nop                          */
    nop                             /* 2   nops                         */
    cache   CACH_PI|C_IINV, 0(a0)   /* invalidate set 1                 */
    .set    reorder                 /* enable assembler reordering      */
    jr      ra                      /* done.                            */
    END(r10000_pi_iinvl)
    
    
/****************
 * r10000_pi_fill
 * --------------
 * We assume address in a0 is the K0BASE virtual address.
 */
#define VADDR   localargs-0x0
#define PADDR   localargs-0x4
NESTED(r10000_pi_fill,framesize,ra)
    subu    sp, framesize           /* create current stack frame       */
    sd      ra, regsave-0x00(sp)    /* save return address.             */
    sd      t0, regsave-0x08(sp)    /* save t0 register.                */
    sd      t1, regsave-0x10(sp)    /* save t1 register.                */
    sd      t2, regsave-0x18(sp)    /* save t2 register.                */
    sd      t3, regsave-0x20(sp)    /* save t3 register.                */
/*
 *  Fix up the target address.
 */
    lw      t0, _icache_linesize    /* read the icache line size.       */
    sub     t2, t0, 1               /* create line size mask            */
    not     t2                      /* invert the mask.                 */
    and     t1, a0                  /* the target line address.         */
    sw      t1, VADDR(sp)           /* set the virtual address.         */
    lui     t2, K1BASE>>16          /* create K1BASED physical addr     */
    or      t1, t2                  /* done                             */
    sw      t1, PADDR(sp)           /* and save it in stack.            */
/*
 *  run uncached.
 */
    la      t1, 9f                  /* load the target address.         */
    or      t2, t1                  /* xfer into K1 base code.          */
    jr      t2                      /* switch to uncached code sequence */
/*
 *  use index store data to fill the icache data ram.
 */
    .set    noreorder               /* turn off assembler reordering    */
9:  mtc0    zero, C0_TAGHI          /* set inst<35:32> = 0              */
    lw      t1, VADDR(sp)           /* read the Virtual addr again      */
    lw      t3, PADDR(sp)           /* read the physical addr.          */
    cache   CACH_PI|C_IINV, 0(t1)   /* invalidate the line in icache    */
10: lw      t2, 0(t3)               /* read the first inst.             */
    mtc0    t2, C0_TAGLO            /* initialize TagLo register.       */
    sub     t0, 4                   /* descrment word count.            */
    addi    t3, 4                   /* point to next inst in memory     */
    cache   CACH_PI|C_ISD, 0(t1)    /* write icache                     */
    bgtz    t0, 10b                 /* loop for a icache line.          */
    addi    t1, 4                   /* point to next inst in cache.     */
/*
 *  make the target icache line valid.
 */
    sll     t0, a0, 3               /* remove the upper 3 bits.         */
    srl     t0, 15                  /* create PTag0<31:8> by extract    */
    sll     t0, 8                   /* PA<35:12> and set the valid bit  */
    or      t0, 0x48                /* PState of Tag0<6> and LRU Tag0<3>*/
    mtc0    t0, C0_TAGLO            /* initialize the TagLo register.   */
    lw      t1, VADDR(sp)           /* read the virtual line address    */
    cache   CACH_PI|C_IST, 0(t1)    /* Done.                            */
    .set    reorder                 /* enable assembler reordering.     */
/*
 *  Return
 */
    ld      ra, regsave-0x00(sp)    /* load ra register.                */
    ld      t0, regsave-0x08(sp)    /* load t0 register.                */
    ld      t1, regsave-0x10(sp)    /* load t1 register.                */
    ld      t2, regsave-0x18(sp)    /* load t2 register.                */
    ld      t3, regsave-0x20(sp)    /* load t3 register.                */
    addiu   sp, framesize           /* create current stack frame       */
    jr      ra                      /* return                           */
    END(r10000_pi_fill)

/**********************
 * r10000_cache_barrier
 * --------------------
 */
LEAF(r10000_cache_barrier)
    .set    noreorder
    jr      ra                     
    cache   CACH_PI|C_BARRIER, 0(a0)
    .set    reorder
    END(r10000_cache_barrier)

/******************
 * r10000_pi_ildata
 * ----------------
 */
LEAF(r10000_pi_ildata)
    .set    noreorder
    jr      ra                     
    cache   CACH_PI|C_ILD, 0(a0)
    .set    reorder
    END(r10000_pi_ildata)

/******************
 * r10000_pi_isdata
 * ----------------
 */
LEAF(r10000_pi_isdata)
    .set    noreorder
    jr      ra                     
    cache   CACH_PI|C_ISD, 0(a0)
    .set    reorder
    END(r10000_pi_isdata)

/*****************
 * r10000_pi_hitwb
 * ---------------
 * do nothing, not supported.
 */
LEAF(r10000_pi_hitwb)
    jr      ra
    END(r10000_pi_hitwb)


/* PRIMARY DATA CACHE OP        */

/******************
 * r10000_pd_iwbinv
 * ----------------
 */
LEAF(r10000_pd_iwbinv)
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_PD|C_IWBINV, 1(a0) /* do set 0                         */
    nop                             /* 1st nop                          */
    nop                             /* 2nd nop                          */
    cache   CACH_PD|C_IWBINV, 0(a0) /* do set 1                         */
    .set    reorder                 /* enable assembler reordering.     */
    jr      ra                      /* done!!                           */
    END(r10000_pd_iwbinv)

/***************
 * r10000_pd_cdx
 * -------------
 */
LEAF(r10000_pd_cdx)
    subu    sp, framesize           /* create current stack frame       */
    sd      t0, regsave-0x08(sp)    /* save t0                          */
    sd      t1, regsave-0x10(sp)    /* save t1                          */
/*
 *  use index writeback invalidate to clear the cache line in both set.
 */
    .set    noreorder               /* disable assemble reordering      */
    cache   CACH_PD|C_IWBINV, 1(a0) /* invalidate set 0                 */
    sll     t1, a0, 3               /* remove the K0BASE                */
    cache   CACH_PD|C_IWBINV, 0(a0) /* invalidate set 1                 */
    lui     t0, 0x4000              /* set the state modifier to 010    */
    mtc0    t0, C0_TAGHI            /* set the TagHi register.          */
    srl     t1, 15                  /* extract paddr<35:12>             */
    sll     t1, 8                   /* and move it to Ptag0<31:8>       */
    ori     t1, 0xc8                /* set LRU and Dirty Exclusive bit  */
    mtc0    t1, C0_TAGLO            /* 2nd cache set 0 has backup       */
    nop                             /* at least 2 nop is required       */
    nop                             /* at least 2 nop is required       */
    cache   CACH_PD|C_IST, 0(a0)    /* initialize the dcache tags.      */
    .set    reorder                 /* enable assembler reordering      */
/*
 *  Done!!
 */
    sd      t0, regsave-0x08(sp)    /* load t0                          */
    sd      t1, regsave-0x10(sp)    /* load t1                          */
    addiu   sp, framesize           /* restore previous stack frame.    */
    jr      ra                      /* return                           */
    END(r10000_pd_cdx)

/*****************
 * r10000_pd_hitwb
 * ---------------
 */
LEAF(r10000_pd_hitwb)
    subu    sp, framesize           /* create current stack frame.      */
    sd      t0, regsave-0x08(sp)    /* save t0                          */
    sd      t1, regsave-0x10(sp)    /* save t1                          */
    sd      t2, regsave-0x18(sp)    /* save t2                          */
    sd      t3, regsave-0x20(sp)    /* save t3                          */
    sll     t0, a0, 3               /* remove K0BASE                    */
    srl     t0, 15                  /* remove the lower order bits.     */
/*
 *  check set 0
 */
    .set    noreorder               /* turn off reordering              */
    cache   CACH_PD|C_ILT, 0(a0)    /* read set0                        */
    li      t1, 0xff                /* create the PTag0 mask            */
    not     t1                      /* done                             */
    mfc0    t2, C0_TAGLO            /* read the Taglo register.         */
    .set    reorder                 /* turn on reorder                  */
    and     t1, t2                  /* extract the PTag0<31:8>          */
    bne     t0, t1, 10f             /* does it match??                  */
    and     t2, t2, 0xc0            /* check the valid bit.             */
    bnez    t2, 20f                 /* yes, its valid.                  */
/*
 *  check set 1
 */
    .set    noreorder               /* turn off reordering              */
10: cache   CACH_PD|C_ILT, 1(a0)    /* read set 1                       */
    li      t1, 0xff                /* create the PTag0 mask            */
    not     t1                      /* done                             */
    mfc0    t2, C0_TAGLO            /* read the Taglo register.         */
    .set    reorder                 /* turn on reorder                  */
    and     t1, t2                  /* extract the PTag0<31:8>          */
    bne     t0, t1, 30f             /* does it match??                  */
    and     t2, t2, 0xc0            /* check the valid bit.             */
    beqz    t2, 30f                 /* No, no valid line was found      */
/*
 *  we have valid line in the primary cache.
 */
    .set    noreorder               /* disable assembler reordering     */
20: cache   CACH_PD|C_HWBINV, 0(a0) /* do hit write back invalidate     */
    nop                             /* give one nop just in case.       */
    nop                             /* give 2nd nop just in case.       */
    .set    reorder                 /* enable assembler reordering      */
    lw      t1, 0(a0)               /* make the line valid again.       */
/*
 *  Done!!
 */
30: ld      t0, regsave-0x08(sp)    /* load t0                          */
    ld      t1, regsave-0x10(sp)    /* load t1                          */
    ld      t2, regsave-0x18(sp)    /* load t2                          */
    ld      t3, regsave-0x20(sp)    /* load t3                          */
    addiu   sp, framesize           /* restore previous stack frame     */
    jr      ra                      /* return                           */
    END(r10000_pd_hitwb)

/******************
 * r10000_pd_ildata
 * ----------------
 */
LEAF(r10000_pd_ildata)
    .set    noreorder
    jr      ra                     
    cache   CACH_PD|C_ILD, 0(a0)
    .set    reorder
    END(r10000_pd_ildata)

/******************
 * r10000_pd_isdata
 * ----------------
 */
LEAF(r10000_pd_isdata)
    .set    noreorder
    jr      ra                     
    cache   CACH_PD|C_ISD, 0(a0)
    .set    reorder
    END(r10000_pd_isdata)


/* SECONDARY INST/DATA CACHE OP */

/*******************
 * r10000_sid_iwbinv
 * -----------------
 */
LEAF(r10000_sid_iwbinv)
    .set    noreorder               /* disable assembler reordering     */
    cache   CACH_SD|C_IWBINV, 1(a0) /* do set 0                         */
    nop                             /* 1st nop                          */
    nop                             /* 2nd nop                          */
    cache   CACH_SD|C_IWBINV, 0(a0) /* do set 1                         */
    .set    reorder                 /* enable assembler reordering.     */
    jr      ra                      /* done!!                           */
    END(r10000_sid_iwbinv)

/****************
 * r10000_sid_cdx
 * --------------
 */
LEAF(r10000_sid_cdx)
    subu    sp, framesize           /* create current stack frame       */
    sd      ra, regsave-0x08(sp)    /* save ra                          */
    sd      t0, regsave-0x10(sp)    /* save t0                          */
    sd      t1, regsave-0x18(sp)    /* save t1                          */
    sd      t2, regsave-0x20(sp)    /* save t2                          */
/*
 *  use index writeback invalidate to clear the cache line in both set.
 */
    bal     r10000_pd_cdx           /* create the line in dcache        */
    andi    t2, a0, 0x3000          /* extract vaddr<13:12>             */
    .set    noreorder               /* disable assemble reordering      */
    cache   CACH_SD|C_IWBINV, 1(a0) /* invalidate set 0                 */
    sll     t1, a0, 3               /* remove the K0BASE                */
    cache   CACH_SD|C_IWBINV, 0(a0) /* invalidate set 1                 */
    lui     t0, 0x8000              /* set the MRU bit in TagHi reg     */
    mtc0    t0, C0_TAGHI            /* set the TagHi register.          */
    srl     t1, 21                  /* extract paddr<35:18>             */
    sll     t1, 13                  /* and move it to Ptag0<31:13>      */
    ori     t1, 0xc00               /* set Dirty Exclusive bit          */
    or      t1, t2                  /* and Vindex.                      */
    mtc0    t1, C0_TAGLO            /* 2nd cache set 0 has backup       */
    nop                             /* at least 2 nop is required       */
    nop                             /* at least 2 nop is required       */
    cache   CACH_SD|C_IST, 0(a0)    /* initialize the dcache tags.      */
    .set    reorder                 /* enable assembler reordering      */
/*
 *  Done!!
 */
    sd      ra, regsave-0x08(sp)    /* load ra                          */
    sd      t0, regsave-0x10(sp)    /* load t0                          */
    sd      t1, regsave-0x18(sp)    /* load t1                          */
    sd      t2, regsave-0x20(sp)    /* load t2                          */
    addiu   sp, framesize           /* restore previous stack frame.    */
    jr      ra                      /* return                           */
    END(r10000_sid_cdx)

/******************
 * r10000_sid_hitwb
 * ----------------
 */
LEAF(r10000_sid_hitwb)
    sub     sp, framesize           /* create current stack frame.      */
    sd      t0, regsave-0x08(sp)    /* save t0                          */
    sd      t1, regsave-0x10(sp)    /* save t1                          */
    .set    noreorder               /* disable reordering.              */
    cache   CACH_SD|C_HWBINV, 0(a0) /* use hit writeback invalidate     */
    nop                             /* give one nop just in             */
    mfc0    t0, C0_SR               /* read the status register.        */
    .set    reorder                 /* enable reordering.               */
    lui     t1, 0x4                 /* check the CH bit.                */
    and     t0, t1                  /* is it a hit or what??            */
    beqz    t0, 1f                  /* its a miss, do nothing.          */
    lw      t1, 0(a0)               /* its a hit, read it back from 2nd */
1:  ld      t0, regsave-0x08(sp)    /* restore t0                       */
    ld      t1, regsave-0x10(sp)    /* restore t1                       */
    addiu   sp, framesize           /* restore previous stack frame.    */
    jr      ra                      /* return                           */
    END(r10000_sid_hitwb)

/*******************
 * r10000_sid_ildata
 * -----------------
 */
LEAF(r10000_sid_ildata)
    .set    noreorder
    jr      ra                     
    cache   CACH_SD|C_ILD, 0(a0)
    .set    reorder
    END(r10000_sid_ildata)

/*******************
 * r10000_sid_isdata
 * -----------------
 */
LEAF(r10000_sid_isdata)
    .set    noreorder
    jr      ra                     
    cache   CACH_SD|C_ISD, 0(a0)
    .set    reorder
    END(r10000_sid_isdata)

/***********************
 * cm_r10000_cache_valid
 * ---------------------
 * Return 1 if there is a valid cache line match the given VA
 *        0 if there is no match in all 4 caches.
 */
NESTED(cm_r10000_cache_valid,framesize,ra)
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
    b       40f                     /* done, return to caller.          */
/*
 *  Check primary cache set 1
 */
    .set    noreorder               /* disable assembler reordering     */
10: cache   CACH_PD|C_ILT, 1(t1)    /* read the set1 Tags.              */
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
    b       40f                     /* done, return to caller.          */
/*
 *  Check secondary cache set 0
 */
    .set    noreorder               /* disable assembler reordering     */
20: cache   CACH_SD|C_ILT, 0(t1)    /* read the set0 Tags.              */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    mfc0    t0, C0_TAGLO            /* read the taglo register.         */
    .set    reorder                 /* enable assembler reordering      */
    srl     t0, 10                  /* check the PState                 */
    andi    t2, t0, 3               /* isolate the State bits.          */
    beqz    t2, 30f                 /* check 2nd set 1 if invalid       */
    srl     t0, 2                   /* Physical addr<35:12>             */
    sll     t2, a0 3                /* remove upper 3 bits of the VA    */
    srl     t2, 21                  /* and extract ADDR<35:18>          */
    bne     t0, t2, 30f             /* does not match check set 0       */
    li      v0, 1                   /* there is a match in set 0        */
    b       40f                     /* done, return to caller.          */
/*
 *  Check secondary cache set 1
 */
    .set    noreorder               /* disable assembler reordering     */
30: cache   CACH_SD|C_ILT, 1(t1)    /* read the set0 Tags.              */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    mfc0    t0, C0_TAGLO            /* read the taglo register.         */
    nop                             /* 1 nop                            */
    nop                             /* 2 nop                            */
    .set    reorder                 /* enable assembler reordering      */
    srl     t0, 10                  /* check the PState                 */
    andi    t2, t0, 3               /* isolate the State bits.          */
    beqz    t2, 35f                 /* check 2nd set 1 if invalid       */
    srl     t0, 2                   /* Physical addr<35:12>             */
    sll     t2, a0 3                /* remove upper 3 bits of the VA    */
    srl     t2, 21                  /* and extract ADDR<35:18>          */
    bne     t0, t2, 35f             /* does not match check set 0       */
    li      v0, 1                   /* there is a match in set 0        */
    b       40f                     /* done, return to caller.          */
                                    /* THERE IS NO MATCH                */
35: move    v0, zero                /* return 0                         */

/*
 *  We are done, v0 should have the return value
 */
    .set    noat                    /* disable AT macro replacement.    */
40: ld	    AT, regsave-0x00(sp)    /* save AT register.                */
    .set    at                      /* enable AT macro replacement.     */
    ld	    ra, regsave-0x08(sp)    /* save return address.             */
    ld	    t0, regsave-0x10(sp)    /* save t0                          */
    ld	    t1, regsave-0x18(sp)    /* save t1                          */
    ld	    t2, regsave-0x20(sp)    /* save t2                          */
    ld	    a0, regsave-0x28(sp)    /* save a0                          */
    addiu   sp, framesize           /* create current stack frame.      */
    jr      ra                      /* return                           */
    END(cm_r10000_cache_valid)

/* END OF FILE */
