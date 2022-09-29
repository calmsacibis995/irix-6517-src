#ifndef __CACHES_H__
#define __CAHCES_H__    1
#endif

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

#define PIlcops     10          /* 10 entries in picache lcoptab    */
#define PDlcops     10          /* 10 entries in picache lcoptab    */
#define SIDlcops    10          /* 10 entries in sidcache lcoptab   */
#define	S512K       0x80000     /* 512K base size for 2nd cache     */

#define R4600_SCACHE_LINESIZE   (8*4)
#define	R4600_DCACHE_LINESIZE	(8*4)
#define	R4600_ICACHE_LINESIZE	(8*4)
#define R4600_DCACHE_LINEMASK	((8*4)-1)
#define R4600_ICACHE_LINEMASK	((8*4)-1)
#define CACHSZ_REG		0x11
#define R4600_SCACHE_LINESIZE	(8*4)
#define R4600_SCACHE_LINEMASK	((8*4)-1)
#define XKPHYS_UNCACHED_BASE	0x9000
#define XKPHYS_UNCACHED_SHIFT	0x20

/*
 * FAKE2ND      :   both r5k and r4600 family use a faked 2nd write through
 *                  2nd cache which do not support regular cache ops.
 * UNIFYWB2ND   :   unify write back 2nd cache.
 * TWOSETWB2ND  :   two set associated write back 2nd cache.
 * UNIFYWT2ND   :   unify write through 2nd cache.
 * TWOSETWT2ND  :   two set associated write through 2nd cache.
 */
#define FAKE2ND         0
#define UNIFYWB2ND      1
#define UF2SETWB2ND     2
#define UNIFYWT2ND      3
#define UF2SETWT2ND     4


#if defined(_LANGUAGE_ASSEMBLY)
/*
 *  Constant used for stack frames.
 */
#define framesize   176
#define inargs      176         
#define frameoffset  32         /* allow 4 doubleword.              */
#define localargs   168         /* sp+framesize-0x08, max 4  dwords */
#define regsave     136         /* sp+framesize-(4*8)-0x8, 16 regs  */
#define outargs       8         /* sp+framesize-(4*8)-(16*8)-0x8    */

/*
 * Cache ops index.
 */

/* PRIMARY INSTRUCTION CACHE.   */
#define pi_iinval     0         /* primary icache index invalidate. */
#define pi_ildtag     1         /* primary icache index load tag.   */
#define pi_isttag     2         /* primary icache index store tag.  */
#define pi_hinval     4         /* primary icache hit invalidate.   */
#define pi_fill       5         /* primary icache fill.             */
#define pi_hwb        6         /* primary icache hit write back.   */
                                /* Following are R10000 only.       */
#define pi_barrier    3         /* primary icache barrier.(r10k)    */
#define pi_nop1       7         /* primary icache index store data. */
#define pi_ilddata    8         /* primary icache index load data.  */
#define pi_istdata    9         /* primary icache index store data. */

/* PRIMARY DATA CACHE.          */
#define pd_iwbinval   10        /* primary dcache index wb inval.   */
#define pd_ildtag     11        /* primary dcache index load tag.   */
#define pd_isttag     12        /* primary dcache index store tag.  */
#define pd_cdx        13        /* primary dcache create dirty excl */
#define pd_hinval     14        /* primary dcache hit invalidate.   */
#define pd_hwbinval   15        /* primary dcache hit wb inval.     */
#define pd_hwb        16        /* primary dcache hit write back.   */
                                /* Following are R10000 only.       */
#define pd_nop1       17        /* primary dcache index store data. */
#define pd_ilddata    18        /* primary dcache index load data.  */
#define pd_istdata    19        /* primary dcache index store data. */

/* SECONDARY INSTRUCTION/DATA CACHE */
#define sd_iwbinval   20        /* 2nd dcache index wb inval.       */
#define sd_ildtag     21        /* 2nd dcache index load tag.       */
#define sd_isttag     22        /* 2nd dcache index store tag.      */
#define sd_cdx        23        /* 2nd dcache create dirty excl     */
#define sd_hinval     24        /* 2nd dcache hit invalidate.       */
#define sd_hwbinval   25        /* 2nd dcache hit wb inval.         */
#define sd_hwb        26        /* 2nd dcache hit write back.       */
#define sd_hsvil      27        /* 2nd dcache hit set virtual.      */
                                /* Following are R10000 only.       */
#define sd_ilddata    28        /* 2nd dcache index load data.      */
#define sd_istdata    29        /* 2nd dcache index store data.     */

/*
 * Cache configuration data
 *
 * The following public cache configuation data is used/maintained by this
 * module:
 *
 *    _icache_size	Size of L1 I cache in bytes
 *    _dcache_size      Size of L1 D cache in bytes
 *    _sidcache_size    Size of L2 I/D cache in bytes
 *    _icache_linesize  Block size of L1 I cache in bytes
 *    _dcache_linesize  Block size of L1 D cache in bytes
 *    _icache_linemask  _icache_linesize-1
 *    _dcache_linemask  _dcache_linesize-1
 *    _scache_linesize  Block size of L2 I/D cache in bytes
 *    _scache_linemask  _scache_linesize-1
 *    _r4600sc_sidcache_size  Size of add-on external cache ala' Indy 4600SC.
 *    _two_set_pcaches  Non-zero if 2way primary, value == dcache_size/2.
 */

/* Caches size  */
.extern _icache_size
.extern _dcache_size
.extern _sidcache_size
.extern _r4600sc_sidcache_size

/* Cache line size. */
.extern _icache_linesize
.extern _dcache_linesize
.extern _scache_linesize

/* Cache line mask. */
.extern _icache_linemask
.extern _dcache_linemask
.extern _scache_linemask

/* Sets in cache.   */
.extern _two_set_pcaches
.extern _two_set_icaches
.extern _two_set_scaches

/* Type of secondary cache. */
.extern _scache_type

/* Run time cache op table. */
.extern lcoptab
.extern _flushAllCache
.extern _validCacheAddr

/* Function prototype   */
.extern __dcache_inval
.extern __dcache_wb
.extern __dcache_wb_inval
.extern __icache_inval
.extern __cache_wb_inval

/* Logical cache ops macro  */
#define CacheOP(OPS,ADDR,R1,R2)     \
        la      R2, lcoptab;        \
        lw      R2, (OPS<<2)(R2);   \
        la      R1, ADDR;           \
        jalr    R2

#endif  /* end of _LANGUAGE_ASSEMBLY */


#if defined(_LANGUAGE_C)
/*
 * Cache ops index.
 */

/* Caches   */
#define PICACHE         0x0
#define PDCACHE         0x1
#define SIDCACHE        0x2

/* Codes    */
#define IDX_WB_INVAL    0x0
#define IDX_INVAL       0x0
#define IDX_LOAD_TAG    0x1
#define IDX_STORE_TAG   0x2
#define CR_DIRTY_EXCL   0x3
#define I_BARRIER       0x3
#define HIT_INVAL       0x4
#define I_FILL          0x5
#define HIT_WB_INVAL    0x5
#define HIT_WRITEBACK   0x6
#define HIT_SET_VIRTUAL 0x7

/* R10000 only. */
#define IDX_LOAD_DATA   0x8
#define IDX_STORE_DATA  0x9

/*
 * Some funny typedef
 */
typedef long                ADDR_t;
typedef int                 SIZE_t;
typedef int                 CACH_t;
typedef                     int  (*cop)(ADDR_t);

/*
 * function defines
 */
extern void r10000_cache_config(void);
extern void r5000_cache_config(void);
extern void r4600_cache_config(void);
extern void r4700_cache_config(void);
extern void r4400_cache_config(void);

/*
 * Function prototype.
 */
extern void __dcache_inval(char*,int);
extern void __dcache_wb(char*,int);
extern void __dcache_wb_inval(char*,int);
extern void __icache_inval(char*,int);
extern void __cache_wb_inval(char*,int);

/*
 * macro for access low level logical cache ops through
 * lcoptab.
 */
#define CacheOP(CACHE,CODE,ADDR) (lcoptab[CACHE][CODE])(ADDR);

#endif  /* END of _LANGUAGE_C   */
