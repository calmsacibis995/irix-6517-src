/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#ident "$Revision: 1.51 $"

/* IP26.h -- CPU module specific defines for IP26.  We use IP22.h for system
 *	     board information since they use the same board.
 *
 *  The Power Indigo2, Power Challenge M and Indigo2 100000 systems implement
 * an ECC memory system that does not allow uncached writes to sytem memory
 * while the system is running in normal mode.  It is possible to write to
 * uncached memory by either:
 *
 *	* Using the 'ucmem' routines around uncached accesses:
 *
 *		long ucmem;
 *		...
 *		ucmem = ip26_enable_ucmem();
 *		... uncached access ...
 *		ip26_return_ucmem(ucmem);
 *
 *	* Setting the system to always run in slow mode by setting the
 *	  systune 'ip26_allow_ucmem' flag to 1.  This will result in
 *	  lower memory performance and overall system performance and
 *	  should only be used as a last resort.
 *
 *  Implications of these features impose mostly on device drivers that
 * access shared data structures (DMA descriptors, etc) uncached to
 * avoid race conditions.  Often this can often be fixed by aligning
 * data on cache line boundries and flushing the cache, or by using
 * the ucmem routines.
 *
 *  Lastly, user mode use of the cachectl(2) call with UNCACHEABLE is
 * not honored unless the global systune flag ip26_enable_ucmem is set
 * is set to true.  This may have an impact on user level device drivers.
 */

#ifndef __SYS_IP26_H__
#define __SYS_IP26_H__

#include <sys/IP22.h>

/* TCC address map:
 */
#define TCC_BASE		0x18000000	/* Base of TCC registers */
#define TCC_FIFO		0x18000000	/* FIFO Control Register */
#define TCC_GCACHE		0x18000008	/* GCache Control Register */
#define TCC_INTR		0x18000010	/* Interrupt Register */
#define TCC_BE_ADDR		0x18000018	/* Bus Error Addr Register */
#define TCC_ERROR		0x18000020	/* Error Info Register */
#define TCC_PARITY		0x18000028	/* Parity Control Register */
#define TCC_COUNT		0x18000030	/* Count Register */
#define TCC_COMPARE		0x18000038	/* Compare Register */
#define TCC_CAM0		0x18010000	/* Prefetch CAM 0 */
#define TCC_CAM1		0x18010008	/* Prefetch CAM 1 */
#define TCC_CAM2		0x18010010	/* Prefetch CAM 2 */
#define TCC_CAM3		0x18010018	/* Prefetch CAM 3 */
#define TCC_CAM4		0x18010020	/* Prefetch CAM 4 */
#define	TCC_PREFETCH_A		0x18010028	/* Prefetch Address A */
#define	TCC_PREFETCH_B		0x18010030	/* Prefetch Address B */
#define TCC_PREFETCH		0x18010038	/* Prefetch Control Register */
#define TCC_ETAG_DA		0x18100000	/* Even Tag Data Access */
#define TCC_OTAG_DA		0x18200000	/* Odd Tag Data Access */
#define TCC_ETAG_ST		0x18300000	/* Even Tag State Access */
#define TCC_OTAG_ST		0x18400000	/* Odd Tag State Access */

/* TCC_FIFO -- graphics FIFO Control Register
 */
#define FIFO_INPUT_EN		0x00000001	/* Gfx fifo input enable */
#define FIFO_OUTPUT_EN		0x00000002	/* Gfx fifo output enable */
#define FIFO_HW			0x000003f0	/* High water setting */
#define FIFO_HW_SHIFT		4
#define FIFO_LW			0x0003f000	/* Low water setting */
#define FIFO_LW_SHIFT		12
#define FIFO_LEVEL		0x03f00000
#define FIFO_LEVEL_SHIFT	20

/* TCC_GCACHE -- GCache Control Register
 */
#define GCACHE_SETALLOW		0x0000000f	/* Bitmap of enabled sets */
#define GCACHE_SET0		0x00000001	/* Enable set 0 */
#define GCACHE_SET1		0x00000002	/* Enable set 1 */
#define GCACHE_SET2		0x00000004	/* Enable set 2 */
#define GCACHE_SET3		0x00000008	/* Enable set 3 */
#define GCACHE_FORCE_SET3	0x00000010	/* Force set 3 */
#define GCACHE_WBACK_INH	0x00000020	/* Inhibits dirty writeback */
#define GCACHE_RR_FULL		0x000003c0	/* Read response full level */
#define GCACHE_RR_FULL_SHIFT	6
#define GCACHE_WB_RESTART	0x00003c00	/* Writeback restart level */
#define GCACHE_WB_RESTART_SHIFT	10
#define GCACHE_REL_DELAY	0x0003c000	/* Release delay */
#define GCACHE_REL_DELAY_SHIFT	14
#define GCACHE_REV		0x003c0000	/* TCC revision */
#define GCACHE_REV_SHIFT	18

/* TCC_INTR -- Interrupt Register
 */
#define INTR_PENDING		0x0000003f	/* Intrrupts pending IP<7:2> */
#define INTR_FIFO_HW		0x00000040	/* Gfx FIFO high water intr */
#define INTR_FIFO_LW		0x00000080	/* Gfx FIFO low water intr */
#define INTR_TIMER		0x00000100	/* TCC Timer Interrupt */
#define INTR_BUSERROR		0x00000200	/* TCC detected Bus Error */
#define INTR_MACH_CHECK		0x00000400	/* TCC detected Machine Check */
#define INTR_FIFO_HW_EN		0x00000800	/* Gfx FIFO HW Enabled */
#define INTR_FIFO_LW_EN		0x00001000	/* Gfx FIFO LW Enabled */
#define INTR_TIMER_EN		0x00002000	/* TCC Timer Intr Enable */
#define INTR_BUSERROR_EN	0x00004000	/* TCC Bus Error Enable */
#define INTR_MACH_CHECK_EN	0x00008000	/* TCC Mach Check Enable */

/* TCC_ERROR -- Error Info Register
 */
#define ERROR_NESTED_BE		0x00000001	/* Second bus error */
#define ERROR_NESTED_MC		0x00000002	/* Second machine check */
#define ERROR_TYPE		0x0000001c	/* Error type */
#define ERROR_TYPE_SHIFT	2
#define ERROR_OFFSET		0x000001e0	/* GCache parity err word */
#define ERROR_OFFSET_SHIFT	5
#define ERROR_INDEX		0x001ffe00	/* GCache parity err index */
#define ERROR_INDEX_SHIFT	9
#define ERROR_PARITY_SET	0x00C00000	/* GCache parity err set */
#define ERROR_PARITY_SET_SHIFT	22
#define ERROR_SYSCMD		0xff000000	/* SysCmd<7:0> sysAD UFO */
#define ERROR_SYSCMD_SHIFT	24

/* TCC_ERROR -- ERROR_TYPE
 */
#define ERROR_SYSAD_TDB		0x00		/* sysAD data parity (TDB) */
#define ERROR_SYSAD_TCC		0x01		/* sysAD data parity (TCC) */
#define ERROR_SYSAD_CMD		0x02		/* sysAD commad parity */
#define ERROR_SYSAD_UFO		0x03		/* sysAD unknown command */
#define ERROR_GCACHE_PARITY	0x04		/* GCache data parity error */
#define ERROR_TBUS_UFO		0x05		/* T-Bus unknown command */

/* TCC_PARITY -- Parity Control Register
 */
#define PAR_TDB_EN		0x00000001	/* Enable TDB sysAD check */
#define PAR_TCC_EN		0x00000002	/* Enable TCC sysAD check */
#define PAR_DATA_TDB_EN		0x00000004	/* Enable TDB store check */
#define PAR_SYSAD_PS_TCC	0x00000008	/* TCC sysad parity gen */
#define PAR_SYSAD_PS_DB0	0x00000010	/* DB0 sysad parity gen */
#define PAR_SYSAD_PS_DB1	0x00100000	/* DB1 sysad parity gen */
#define PAR_SYSAD_SYND_TCC	0x000f0000	/* TCC sysad parity err byte */
#define PAR_SYSAD_SYND_DB0	0x0000f000	/* DB0 sysad parity err byte */
#define PAR_SYSAD_SYND_DB1	0xf0000000	/* DB1 sysad parity err byte */
#define PAR_DATA_PS_DB0		0x00000020	/* DB0 data parity gen */
#define PAR_DATA_PS_DB1		0x00200000	/* DB1 data parity gen */
#define PAR_DATA_SYND_DB0	0x00000c00	/* DB0 data parity err byte */
#define PAR_DATA_SYND_DB1	0x0c000000	/* DB1 data parity err byte */
#define PAR_REV_DB0		0x000003c0	/* DB0 revision */
#define PAR_REV_DB1		0x03c00000	/* DB1 revision */


/* TCC_CAMX, TCC_PREFETCH_Y -- Prefetch CAM X, Prefetch Address Y
 */
#define PRE_VALID		0x00000001	/* Valid prefetch entry */
#define	PRE_ADDRESS		0xffffff80	/* Address of prefetch */

/* TCC_PREFETCH -- Prefetch Control Register
 */
#define PRE_INV			0x00000001	/* Invalidate prefetch cache */
#define PRE_TIMEOUT		0x0000001e	/* Prefetch ageing control */
#define PRE_TIMEOUT_SHIFT	1
#define PRE_A_EN		0x00000020	/* Prefetch buffer A enable */
#define PRE_B_EN		0x00000040	/* Prefetch buffer B enable */
#define PRE_CAM_DEPTH		0x00000380	/* Prefetch queue depth */
#define PRE_CAM_DEPTH_SHIFT	7
#define PRE_A_VALID		0x00001000	/* Prefetch buffer A valid */
#define PRE_B_VALID		0x00002000	/* Prefetch buffer B valid */
#define PRE_CAM0_VALID		0x00010000	/* CAM buffer 0 valid */
#define PRE_CAM1_VALID		0x00020000	/* CAM buffer 1 valid */
#define PRE_CAM2_VALID		0x00040000	/* CAM buffer 2 valid */
#define PRE_CAM3_VALID		0x00080000	/* CAM buffer 3 valid */
#define PRE_CAM4_VALID		0x00100000	/* CAM buffer 4 valid */
#define PRE_A_AGE		0x0f000000	/* Prefetch buffer A age */
#define PRE_B_AGE		0xf0000000	/* Prefetch buffer B age */
#define PRE_DEFAULT	((3<<PRE_TIMEOUT_SHIFT)|PRE_A_EN|PRE_B_EN|PRE_CAM_DEPTH)

/* Tag Data
 */
#define GCACHE_TAG		0xfff80000	/* Valid bits of tag data */
#define GCACHE_INDEX		0x0003ff80
#define GCACHE_OFFSET		0x0000007f
#define GCACHE_TAG_SHIFT	19
#define GCACHE_INDEX_SHIFT	7

/* Tag State
 */
#define GCACHE_DIRTY		0x00100000	/* Dirty bit */
#define GCACHE_VSYN_EN		0x00200000	/* Virtual synonym enable */
#define GCACHE_STATE_EN		0x00400000	/* State/Dirty write enable */
#define GCACHE_STATE		0x01800000	/* GCache state bits */
#define GCACHE_STATE_SHIFT	23
#define GCACHE_VSYN		0x1e000000	/* Virtual synonym */
#define GCACHE_INITSTATE	(GCACHE_VSYN_EN|GCACHE_STATE_EN)

/* Tag State -- GCACHE_STATE
 */
#define GCACHE_INVALID		0
#define GCACHE_VALID		2

/* Tag Data/State address generation
 */
#define TAGADDR_INDEX_SHIFT	7
#define TAGADDR_SET_SHIFT	5
#define tcc_tagaddr(base,index,set)		\
	(base | (index << TAGADDR_INDEX_SHIFT) | (set << TAGADDR_SET_SHIFT))

/* TCC GCache flushing
 */
#define TCC_INDEX_OP		0x00000004	/* Index/Hit operation */
#define TCC_DIRTY_WB		0x00000008	/* Dirty Writeback enable */
#define TCC_INVALIDATE		0x00000010	/* Invalidate Enable */
#define TCC_CACHE_SET		0x00000060	/* Cache set for index ops */
#define TCC_CACHE_SET_SHIFT	5
#define TCC_CACHE_INDEX		0x0007ff80	/* Cache index for index ops */
#define TCC_CACHE_INDEX_SHIFT	7
#define TCC_PHYSADDR		0x7fffff80	/* Physical address */
#define TCC_CACHE_OP		0x100000000	/* Flag load as cache op */
#define TCC_LINESIZE		128		/* IP26 GCache line size */

#define CACHE_SLINE_SIZE	TCC_LINESIZE	/* compat with other cpus */

/* Undefined TCC registers used clear the TFP Store Address Queue.
 */
#define SAQ_INIT_ADDRESS	0x18180000
#define SAQ_DEPTH		32

#define OSPL_SPDBG		0x0080		/* undefined in TFP SR */

/*  Defines to make uncached processor ordered access easier.  Stores
 * to this mode go through the TCC fifo, which is primarily used for
 * graphics.  Hence we call it KG, or 'kernel graphics' space.
 *
 *  When mapping with the TLB we should use TLBLO_UNC_PROCORD.
 */
#define KGBASE			0x8000000000000000
#define KG_TO_K1(x)		PHYS_TO_K1(x)
#define KG_TO_PHYS(x)		KDM_TO_PHYS(x)
#if _LANGUAGE_ASSEMBLY
#define PHYS_TO_KG(x)		((x) | KGBASE)
#define K1_TO_KG(x)		PHYS_TO_KG((x) & TO_PHYS_MASK)
#else
#define PHYS_TO_KG(x)		((__psunsigned_t)(x) | KGBASE)
#define K1_TO_KG(x)		PHYS_TO_KG((__psunsigned_t)(x) & TO_PHYS_MASK)
#endif

/*  Additional VECTOR_ defines for setlclvector() for the TCC managed
 * interrupts.
 */
#define VECTOR_TCCHW		1000		/* TCC fifo High Water */
#define VECTOR_TCCLW		1001		/* TCC fifo Low Water */

/* Prototypes for IP26 specific routines.
 */
#if _LANGUAGE_C
void settccfifos(int lw, int hw);
void enabletccintrs(int mask);
void cleartccintrs(int mask);
long tune_tcc_gcache(void);			/* internal */
#endif

#if _STANDALONE

#if LANGUAGE_ASSEMBLY
/* The IP26 PROM uses some of the TFP misc CP0 registers.
 */
#define TETON_BEV		C0_WORK0	/* early handler ptr */
#define TETON_GERROR		C0_WORK1	/* early GCache err count */
#endif

#define GERROR_PARITY		0x000000000000ffff
#define GERROR_TAG		0x00000000ffff0000
#define GERROR_DATA		0x0000ffff00000000
#define GERROR_SET0		0x1000000000000000
#define GERROR_SET1		0x2000000000000000
#define GERROR_SET2		0x4000000000000000
#define GERROR_SET3		0x8000000000000000
#define GERROR_MAXPONERR	50

#define GERROR_TAG_SHIFT	16
#define GERROR_DATA_SHIFT	32
#define GERROR_SET_SHIFT	60

/*  Base PROM SR settings.  Most of prom runs with SR_IBIT4 (for soft power)
 * enabled as well.
 */
#define SR_PROMBASE SR_CU0|SR_CU1|SR_FR|SR_PAGESIZE|SR_IE|SR_IBIT7|SR_IBIT9|SR_IBIT10

#endif /* _STANDALONE */

#endif /* __SYS_IP26_H__ */
