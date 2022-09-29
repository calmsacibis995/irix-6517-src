#ifndef __RACER_HEART_H__
#define __RACER_HEART_H__
/*
 * heart.h - heart chip header file
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "$Revision: 1.95 $"
/*
 * heart.h - heart chip header file
 */
#define	HEART_CHIP	1

/* Notes:  All Heart registers are 64 bits wide.  Heart is defined
 * to be hardwired to widget 0x8, see sys/xtalk/xbow.h ...
 */

#if LANGUAGE_C
#include "../iobus.h"
#include "../xtalk/xtalk.h"
#endif
#include "../xtalk/xwidget.h"

#if LANGUAGE_C
typedef __uint64_t heartreg_t;
#define HEARTCONST (heartreg_t)
#else
#define HEARTCONST
#endif

/* Widget part number of heart */
#define HEART_WIDGET_PART_NUM	0xc001

/* Widget manufacturer of heart */
#define HEART_WIDGET_MFGR	0x036

/* Revision numbers for known Heart revisions */
#define	HEART_REV_A		0x1
#define	HEART_REV_B		0x2
#define	HEART_REV_C		0x3
#define	HEART_REV_D		0x4

/* HEART internal register space */
#define HEART_PIU_BASE		0x0ff00000
#define	HEART_PIU_K1PTR	((heart_piu_t *)PHYS_TO_COMPATK1(HEART_PIU_BASE))

#define	HEART_MIN_PORT		0x0
#define	HEART_MAX_PORT		0xF

/* MAIN_IO_SPACE contains 16 address spaces for widgets 0x0
 * through 0xf, each 16M in size 
 */
#define MAIN_IO_SPACE		0x10000000
#define	MAIN_IO_SIZE		0x01000000L
#define	MAIN_WIDGET(x)		(MAIN_IO_SPACE+(x)*MAIN_IO_SIZE)
#define K1_MAIN_WIDGET(x)	PHYS_TO_K1(MAIN_WIDGET(x))
#define	IS_MAIN_IO_SPACE(x)	((x) >= MAIN_IO_SPACE &&	\
				 (x) < MAIN_WIDGET(HEART_MAX_PORT+1))
#define	MAIN_IO_TO_WIDGET_ID(x)	(((x)-MAIN_IO_SPACE)>>24) 

/* LARGE_IO_SPACE contains 16 address spaces for widgets 0x0 
 * through 0xf, each 2G in size
 */
#define LARGE_IO_SPACE		0x800000000
#define	LARGE_IO_SIZE		0x080000000L
#define	LARGE_WIDGET(x)		(LARGE_IO_SPACE+(x)*LARGE_IO_SIZE)
#define K1_LARGE_WIDGET(x)	PHYS_TO_K1(LARGE_WIDGET(x))
#define	IS_LARGE_IO_SPACE(x)	((x) >= LARGE_IO_SPACE &&	\
				 (x) < LARGE_WIDGET(HEART_MAX_PORT+1))
#define	LARGE_IO_TO_WIDGET_ID(x)	(((x)-LARGE_IO_SPACE)>>31) 

/* HUGE_IO_SPACE contains 15 address spaces for widgets 0x1 
 * through 0xf, each 64G in size
 */
#define HUGE_IO_SPACE		0x1000000000
#define	HUGE_IO_SIZE		0x1000000000
#define	HUGE_WIDGET(x)		(HUGE_IO_SPACE+((x)-1)*HUGE_IO_SIZE)
#define K1_HUGE_WIDGET(x)	PHYS_TO_K1(HUGE_WIDGET(x))
#define	IS_HUGE_IO_SPACE(x)	((x) >= HUGE_IO_SPACE &&	\
				 (x) < HUGE_WIDGET(HEART_MAX_PORT+1))
#define	HUGE_IO_TO_WIDGET_ID(x)	((((x)-HUGE_IO_SPACE)>>36)+1)

#define HEARTADDR(x)		((x) & ~0x7)
/* widget configuration registers */
/* offsets */
#define HEART_IOU_WID_ID	HEARTADDR(WIDGET_ID)
#define HEART_IOU_WID_STAT	HEARTADDR(WIDGET_STATUS)
#define HEART_IOU_WID_EUPPER	HEARTADDR(WIDGET_ERR_UPPER_ADDR)
#define HEART_IOU_WID_ELOWER	HEARTADDR(WIDGET_ERR_LOWER_ADDR)
#define HEART_IOU_WID_CTL	HEARTADDR(WIDGET_CONTROL)
#define HEART_IOU_REQ_TO	HEARTADDR(WIDGET_REQ_TIMEOUT)
#define HEART_IOU_ERR_CMD	HEARTADDR(WIDGET_ERR_CMD_WORD)
#define HEART_IOU_LLP		HEARTADDR(WIDGET_LLP_CFG)
#define HEART_IOU_TFLUSH	HEARTADDR(WIDGET_TFLUSH)
#define HEART_IOU_RSVD_058	HEARTADDR(0x58)
#define HEART_IOU_ETYPE		HEARTADDR(0x60)
#define HEART_IOU_EMASK		HEARTADDR(0x68)
#define HEART_IOU_PIO_EUPPER	HEARTADDR(0x70)
#define HEART_IOU_PIO_ELOWER	HEARTADDR(0x78)
#define HEART_IOU_ISR		HEARTADDR(0x80)
#define HEART_IOU_RSVD_088	HEARTADDR(0x88)
#define HEART_IOU_PIO_RTO	HEARTADDR(0x90)

/* xtalk-accessible widget register */
#define HEART_XTALK_ISR		(HEARTCONST 0x80)

/* full addresses */
#define HEART_WID_ID		(HEART_BASE+(WIDGET_ID&~0x7))
#define HEART_WID_STAT		(HEART_BASE+(WIDGET_STATUS&~0x7))
#define HEART_WID_ERR_UPPER	(HEART_BASE+(WIDGET_ERR_UPPER_ADDR&~0x7))
#define HEART_WID_ERR_LOWER	(HEART_BASE+(WIDGET_ERR_LOWER_ADDR&~0x7))
#define HEART_WID_CONTROL	(HEART_BASE+(WIDGET_CONTROL&~0x7))
#define HEART_WID_REQ_TIMEOUT	(HEART_BASE+(WIDGET_REQ_TIMEOUT&~0x7))
#define HEART_WID_ERR_CMDWORD	(HEART_BASE+(WIDGET_ERR_CMD_WORD&~0x7))
#define HEART_WID_LLP		(HEART_BASE+(WIDGET_LLP_CFG&~0x7))
#define HEART_WID_TARG_FLUSH	(HEART_BASE+(WIDGET_TFLUSH&~0x7))
#define HEART_WID_ERR_TYPE	(HEART_BASE+0x60)
#define HEART_WID_ERR_MASK	(HEART_BASE+0x68)
#define	HEART_WID_PIO_ERR_UPPER	(HEART_BASE+0x70)
#define	HEART_WID_PIO_ERR_LOWER	(HEART_BASE+0x78)
#define HEART_WID_ISR		(HEART_BASE+HEART_XTALK_ISR)
#define	HEART_WID_PIO_RTO_ADDR	(HEART_BASE+0x90)

/* control registers */
/* offsets */
#define HEART_PIU_MODE		(0x00000)
#define HEART_PIU_SDRAM_MODE	(0x00008)
#define HEART_PIU_MEM_REF	(0x00010)
#define HEART_PIU_MEM_REQ_ARB	(0x00018)
#define HEART_PIU_MEMCFG0	(0x00020)
#define HEART_PIU_MEMCFG(x)	(HEART_PIU_MEMCFG0 + (x) * 8)
#define HEART_PIU_FC_MODE	(0x00040)
#define HEART_PIU_FC_TIMER_LIM	(0x00048)
#define HEART_PIU_FC0_ADDR	(0x00050)
#define HEART_PIU_FC1_ADDR	(0x00058)
#define HEART_PIU_FC0_CR_CNT	(0x00060)
#define HEART_PIU_FC1_CR_CNT	(0x00068)
#define HEART_PIU_FC0_TIMER	(0x00070)
#define HEART_PIU_FC1_TIMER	(0x00078)
#define HEART_PIU_STATUS	(0x00080)
#define HEART_PIU_BERR_ADDR	(0x00088)
#define HEART_PIU_BERR_MISC	(0x00090)
#define HEART_PIU_MERR_ADDR	(0x00098)
#define HEART_PIU_MERR_DATA	(0x000a0)
#define HEART_PIU_EACC_REG	(0x000a8)
#define HEART_PIU_MLAN_CLK_DIV	(0x000b0)
#define HEART_PIU_MLAN_CTL	(0x000b8)

#define HEART_PIU_IMR0		(0x10000)
#define HEART_PIU_IMR(x)	(HEART_PIU_IMR0 + (x) * 8)
#define HEART_PIU_SET_ISR	(0x10020)
#define HEART_PIU_CLR_ISR	(0x10028)
#define HEART_PIU_ISR		(0x10030)
#define HEART_PIU_IMSR		(0x10038)
#define HEART_PIU_CAUSE		(0x10040)

#define HEART_PIU_COUNT		(0x20000)
#define HEART_PIU_COMPARE	(0x30000)
#define HEART_PIU_TRIGGER	(0x40000)
#define HEART_PIU_PID		(0x50000)
#define HEART_PIU_SYNC		(0x60000)

/* full addresses */
#define HEART_MODE		HEART_PIU_BASE
#define HEART_SDRAM_MODE	(HEART_PIU_BASE+0x08)
#define HEART_MEM_REF		(HEART_PIU_BASE+0x10)
#define HEART_MEM_REQ_ARB	(HEART_PIU_BASE+0x18)
#define	HEART_MEMCFG0		(HEART_PIU_BASE+0x20)
#define	HEART_MEMCFG(x)		(HEART_MEMCFG0+(x)*0x8)
#define HEART_FC_MODE		(HEART_PIU_BASE+0x40)
#define HEART_FC_TIMER_LIMIT	(HEART_PIU_BASE+0x48)
#define HEART_FC0_ADDR		(HEART_PIU_BASE+0x50)
#define	HEART_FC_ADDR(x)	(HEART_FC0_ADDR+(x)*0x8)
#define HEART_FC0_CR_CNT	(HEART_PIU_BASE+0x60)
#define HEART_FC_CR_CNT(x)	(HEART_FC0_CR_CNT+(x)*0x8)
#define HEART_FC0_TIMER		(HEART_PIU_BASE+0x70)
#define HEART_FC_TIMER(x)	(HEART_FC0_TIMER+(x)*0x8)
#define HEART_STATUS		(HEART_PIU_BASE+0x80)
#define HEART_BERR_ADDR		(HEART_PIU_BASE+0x88)
#define HEART_BERR_MISC		(HEART_PIU_BASE+0x90)
#define HEART_MEMERR_ADDR	(HEART_PIU_BASE+0x98)
#define HEART_MEMERR_DATA	(HEART_PIU_BASE+0xa0)
#define HEART_PIUR_ACC_ERR	(HEART_PIU_BASE+0xa8)
#define	HEART_MLAN_CLK_DIV	(HEART_PIU_BASE+0xb0)
#define	HEART_MLAN_CTL		(HEART_PIU_BASE+0xb8)

/* Undefined register for diags: defined here to ensure it is reserved */
#define	HEART_PIU_UNDEF		(HEART_PIU_BASE+0x1000)

/* bits in the HEART_WID_ID register */
#define	HW_ID_REV_NUM_SHFT	28	
#define	HW_ID_REV_NUM_MSK	(HEARTCONST 0xf << HW_ID_REV_NUM_SHFT) 
#define	HW_ID_PART_NUM_SHFT	12
#define HW_ID_PART_NUM_MSK	(HEARTCONST 0xffff << HW_ID_PART_NUM_SHFT)
#define	HW_ID_MFG_NUM_SHFT	1
#define	HW_ID_MFG_NUM_MSK	(HEARTCONST 0x7ff << HW_ID_MFG_NUM_SHFT)	

/* bits in the HEART_MODE registers */
#define	HM_PROC_DISABLE_SHFT	60
#define	HM_PROC_DISABLE_MSK	(HEARTCONST 0xf << HM_PROC_DISABLE_SHFT)
#define	HM_PROC_DISABLE(x)	(HEARTCONST 0x1 << (x) + HM_PROC_DISABLE_SHFT)
#define	HM_MAX_PSR		(HEARTCONST 0x7 << 57)
#define	HM_MAX_IOSR		(HEARTCONST 0x7 << 54)
#define	HM_MAX_PEND_IOSR	(HEARTCONST 0x7 << 51)

#define	HM_TRIG_SRC_SEL_MSK	(HEARTCONST 0x7 << 48)
#define	HM_TRIG_HEART_EXC	(HEARTCONST 0x0 << 48)	/* power-up default */
#define	HM_TRIG_REG_BIT		(HEARTCONST 0x1 << 48)
#define	HM_TRIG_SYSCLK		(HEARTCONST 0x2 << 48)
#define	HM_TRIG_MEMCLK_2X	(HEARTCONST 0x3 << 48)
#define	HM_TRIG_MEMCLK		(HEARTCONST 0x4 << 48)
#define	HM_TRIG_IOCLK		(HEARTCONST 0x5 << 48)

#define	HM_PIU_TEST_MODE	(HEARTCONST 0xf << 40)

#define	HM_GP_FLAG_MSK		(HEARTCONST 0xf << 36)
#define	HM_GP_FLAG(x)		(HEARTCONST 0x1 << (x) + 36)

#define	HM_MAX_PROC_HYST	(HEARTCONST 0xf << 32)
#define	HM_LLP_WRST_AFTER_RST	(HEARTCONST 0x1 << 28)
#define	HM_LLP_LINK_RST		(HEARTCONST 0x1 << 27)
#define	HM_LLP_WARM_RST		(HEARTCONST 0x1 << 26)
#define	HM_COR_ECC_LCK		(HEARTCONST 0x1 << 25)
#define	HM_REDUCED_PWR		(HEARTCONST 0x1 << 24)
#define HM_COLD_RST		(HEARTCONST 0x1 << 23)
#define	HM_SW_RST		(HEARTCONST 0x1 << 22)
#define	HM_MEM_FORCE_WR		(HEARTCONST 0x1 << 21)
#define	HM_DB_ERR_GEN		(HEARTCONST 0x1 << 20)
#define	HM_SB_ERR_GEN		(HEARTCONST 0x1 << 19)
#define	HM_CACHED_PIO_EN	(HEARTCONST 0x1 << 18)
#define	HM_CACHED_PROM_EN	(HEARTCONST 0x1 << 17)
#define	HM_PE_SYS_COR_ERE	(HEARTCONST 0x1 << 16)
#define	HM_GLOBAL_ECC_EN	(HEARTCONST 0x1 << 15)
#define	HM_IO_COH_EN		(HEARTCONST 0x1 << 14)
#define	HM_INT_EN		(HEARTCONST 0x1 << 13)
#define	HM_DATA_CHK_EN		(HEARTCONST 0x1 << 12)
#define	HM_REF_EN		(HEARTCONST 0x1 << 11)
#define	HM_BAD_SYSWR_ERE	(HEARTCONST 0x1 << 10)
#define	HM_BAD_SYSRD_ERE	(HEARTCONST 0x1 << 9)
#define	HM_SYSSTATE_ERE		(HEARTCONST 0x1 << 8)
#define	HM_SYSCMD_ERE		(HEARTCONST 0x1 << 7)
#define	HM_NCOR_SYS_ERE		(HEARTCONST 0x1 << 6)
#define	HM_COR_SYS_ERE		(HEARTCONST 0x1 << 5)
#define	HM_DATA_ELMNT_ERE	(HEARTCONST 0x1 << 4)
#define	HM_MEM_ADDR_PROC_ERE	(HEARTCONST 0x1 << 3)
#define	HM_MEM_ADDR_IO_ERE	(HEARTCONST 0x1 << 2)
#define	HM_NCOR_MEM_ERE		(HEARTCONST 0x1 << 1)
#define	HM_COR_MEM_ERE		(HEARTCONST 0x1 << 0)

#define	HM_MEM_ERE	(HM_MEM_ADDR_PROC_ERE |	\
			 HM_MEM_ADDR_IO_ERE |	\
			 HM_NCOR_MEM_ERE |	\
			 HM_COR_MEM_ERE)

/* bits in the memory refresh register */
#define HEART_MEMREF_REFS(x)	(HEARTCONST (0xf & (x)) << 16)	
#define HEART_MEMREF_PERIOD(x)	(HEARTCONST (0xffff & (x)))
/*
 * The default reset values for REFS, number of lines refreshed in ref burst,
 * is 2, while the PERIOD, # of memclk ticks to count down, is 0x1000.
 * To work around the problem of rev's < E, where we can drop a refresh,
 * we extend the period by 4X and to compensate we also increase the number
 * of lines refreshed by 4X so the overall rate of refresh stay the same
 * but the chances of loosing a refresh is lessened because the refresh request
 * has to be shut out by (0x4000 * 1/66.6MHz), ~246 usecs, which is unlikely.
 * These values should good for rev E heart as well.
 */
#define HEART_MEMREF_REFS_VAL	HEART_MEMREF_REFS(8)
#define HEART_MEMREF_PERIOD_VAL HEART_MEMREF_PERIOD(0x4000)
#define HEART_MEMREF_VAL	(HEART_MEMREF_REFS_VAL | HEART_MEMREF_PERIOD_VAL)

/* bits in the memory request arbitrarion register */
#define	HEART_MEMARB_IODIS	(1<<20)
#define	HEART_MEMARB_MAXPMWRQS	(15<<16)
#define	HEART_MEMARB_MAXPMRRQS	(15<<12)
#define	HEART_MEMARB_MAXPMRQS	(15<<8)
#define	HEART_MEMARB_MAXRRRQS	(15<<4)
#define	HEART_MEMARB_MAXGBRRQS	(15)


/* bits in the memory configuration registers */
#define	HEART_MEMCFG_VLD	0x80000000	/* bank valid bit */
/* MSIZE[5:0]=DIMM size; MSIZE[8:6]=DIMM configuration */
#define	HEART_MEMCFG_RAM_MSK	0x003f0000	/* RAM mask */
#define	HEART_MEMCFG_DENSITY	0x01c00000	/* RAM density */
#define	HEART_MEMCFG_RAM_SHFT	16
#define	HEART_MEMCFG_ADDR_MSK	0x000001ff	/* base address mask */
#define	HEART_MEMCFG_UNIT_SHFT	25		/* 32 MB is the HEART's basic
						   memory unit */

/* bits in the status register */
#define HEART_STAT_HSTL_SDRV	(HEARTCONST 0x1 << 14)
#define	HEART_STAT_FC_CR_OUT(x)	(HEARTCONST 0x1 << (x) + 12)
#define	HEART_STAT_DIR_CNNCT	(HEARTCONST 0x1 << 11)
#define	HEART_STAT_TRITON	(HEARTCONST 0x1 << 10)
#define	HEART_STAT_R4K		(HEARTCONST 0x1 << 9)
#define	HEART_STAT_BIG_ENDIAN	(HEARTCONST 0x1 << 8)
#define	HEART_STAT_PROC_ACTIVE_SHFT	4
#define	HEART_STAT_PROC_ACTIVE_MSK	(HEARTCONST 0xf << HEART_STAT_PROC_ACTIVE_SHFT)
#define	HEART_STAT_PROC_ACTIVE(x)	(HEARTCONST 0x1 << (x) + HEART_STAT_PROC_ACTIVE_SHFT)
#define	HEART_STAT_WIDGET_ID	0xf

/* interrupt registers */
#define HEART_IMR0		(HEART_PIU_BASE+0x10000)
#define	HEART_IMR(x)		(HEART_IMR0+(x)*0x8)
#define HEART_SET_ISR		(HEART_PIU_BASE+0x10020)
#define HEART_CLR_ISR		(HEART_PIU_BASE+0x10028)
#define HEART_ISR		(HEART_PIU_BASE+0x10030)
#define HEART_IMSR		(HEART_PIU_BASE+0x10038)
#define HEART_CAUSE		(HEART_PIU_BASE+0x10040)

/* bits in the interrupt status register */
#define HEART_ISR_HEART_EXC	(HEARTCONST 0x1 << 63)	/* vector 63 */

/* miscellaneous registers */
#define HEART_COUNT		(HEART_PIU_BASE+0x20000)/* 52-bit counter */
#define HEART_COMPARE		(HEART_PIU_BASE+0x30000)
#define HEART_TRIGGER		(HEART_PIU_BASE+0x40000)
#define HEART_PRID		(HEART_PIU_BASE+0x50000)
#define HEART_SYNC		(HEART_PIU_BASE+0x60000)

/* Interrupt handling 
 */
#define HEART_VEC_TO_IBIT(vec)	(HEARTCONST 1 << (vec))
#define	HEART_INT_VECTORS	64			/* how many vectors we manage */

/* IP8 is the CPU count/compare, IP2 and IP1 are SW2 and SW1 */
#define HEART_INT_LEVEL4	0xfff8000000000000	/* IP7 */
#define HEART_INT_LEVEL3	0x0004000000000000	/* IP6 */
#define HEART_INT_LEVEL2	0x0003ffff00000000	/* IP5 */
#define HEART_INT_LEVEL1	0x00000000ffff0000	/* IP4 */
#define HEART_INT_LEVEL0	0x000000000000ffff	/* IP3 */
#define HEART_INT_L4SHIFT	51
#define HEART_INT_L4MASK	0x1fff
#define HEART_INT_L3SHIFT	50
#define HEART_INT_L3MASK	0x1
#define HEART_INT_L2SHIFT	32
#define HEART_INT_L2MASK	0x3ffff
#define HEART_INT_L1SHIFT	16
#define HEART_INT_L1MASK	0xffff
#define HEART_INT_L0SHIFT	0
#define HEART_INT_L0MASK	0xffff

/* after HEART_INT_LxSHIFT */
#define HEART_INT_EXC		(HEARTCONST 0x1000)	/* vector 63 */
#define HEART_INT_PBERR		(HEARTCONST 0x0f00)	/* vectors 62-59 */
#define	HEART_INT_CPUPBERRSHFT	8
#define	HEART_INT_PBERR_0	(HEARTCONST 0x0100)
#define HEART_INT_CPUBERR	(HEART_INT_PBERR_0<<cpuid())
#define HEART_INT_L4IP		(HEARTCONST 0x00ff)	/* vectors 58-51 */

#define HEART_INT_L3IP		(HEARTCONST 0x1)	/* vector 50 */
#define HEART_INT_TIMER		(HEART_INT_L3IP<<HEART_INT_L3SHIFT)
#define HEART_INT_IPI		0x3c000			/* vectors 49-46 */
#define HEART_INT_IPISHFT	14
#define HEART_INT_DEBUG		0x3c00			/* vectors 45-42 */
#define HEART_INT_DEBUGSHFT	10
#define HEART_INT_L2IP		0x3ff			/* vectors 41-32 */
#define HEART_INT_L1IP		HEART_INT_L1MASK	/* vectors 31-16 */
#define HEART_INT_L0IP		0xfff8			/* vectors 3-15 */
#define HEART_INT_HW1		0x0004			/* vector 2 */
#define HEART_INT_HW0		0x0002			/* vector 1 */
#define HEART_INT_IRQ		0x0001			/* vector 0 */

/* These bits depend on the cpu id */
#define	HEART_IMR_BERR_(cpu)	\
	(1L << (HEART_INT_L4SHIFT+HEART_INT_CPUPBERRSHFT+(cpu)))
#define HEART_IMR_IPI_(cpu)		\
	(1L << (HEART_INT_L2SHIFT+HEART_INT_IPISHFT+(cpu)))
#define HEART_IMR_DEBUG_(cpu)		\
	(1L << (HEART_INT_L2SHIFT+HEART_INT_DEBUGSHFT+(cpu)))

#define HEART_IMR_BERR	HEART_IMR_BERR_(cpuid())
#define HEART_ISR_BERR	HEART_IMR_BERR
#define HEART_IMR_IPI	HEART_IMR_IPI_(cpuid())
#define HEART_ISR_IPI	HEART_IMR_IPI
#define HEART_IMR_DEBUG	HEART_IMR_DEBUG_(cpuid())
#define HEART_ISR_DEBUG	HEART_IMR_DEBUG


/* This is the current allocation of the 64 interrupt vectors
 * in heart:
 *
 *  IBIT8	CPU count/compare, R10000 performance counter.
 *
 *  IBIT7	Level 5 hardware interrupts
 *  63		Heart exception
 *  62-59	Processor bus errors (62-59 ~ P3-P0) (4)
 *  58-51	Crosstalk Widget errors (8)
 *	58	Widget 0x0:  Crossbow
 *	57	Widget 0xf:  Bridge
 *	56	Widget 0xe
 *	55	Widget 0xd
 *	54	Widget 0xc
 *	53	Widget 0xb
 *	52	Widget 0xa
 *	51	Widget 0x9
 *
 *  IBIT6	Level 3 hardware interrupts
 *  50		Heart counter/timer interrupt (fast clock)
 *
 *  IBIT5	Level 2 hardware interrupts
 *  49-46	Interprocessor interrupts (49-46 ~ P3-P0) (4)
 *  45-42	Debugger interrupts (45-42 ~ P3-P0) (4)
 *  41		Power switch interrupt
 *  40-32	Level 2 local interrupt vectors (10)
 *
 *  IBIT4	Level 1 hardware interrupts
 *  31-16	Level 1 local interrupt vectors (16)
 *
 *  IBIT3	Level 0 hardware interrupts
 *  15-3	Level 0 local interrupt vectors (13)
 *  2		Flow control high water 1
 *  1		Flow control high water 0
 *  0		IRQ - generic fifo-full type interrupt
 */

#define WIDGET_ERRVEC_BASE	51	/* first widget error vector */
#define WIDGET_ERRVEC_COUNT	8	/* number of widget error vectors */
#define	WIDGET_ERRVEC(p)	(WIDGET_ERRVEC_BASE + (((p)-1) & (WIDGET_ERRVEC_COUNT-1)))
#define	WIDGET_ERRVEC_MASK	(((1ull<<WIDGET_ERRVEC_COUNT)-1ull) << WIDGET_ERRVEC_BASE)
#define WIDGET_LCL2VEC_BASE	32	/* first lcl2 vector */
#define WIDGET_LCL2VEC_COUNT	8	/* number of lcl2 vectors */
#define WIDGET_LCL1VEC_BASE	16	/* first lcl1 vector */
#define WIDGET_LCL1VEC_COUNT	16	/* number of lcl1 vectors */
#define WIDGET_LCL0VEC_BASE	0	/* first lcl0 vector */
#define WIDGET_LCL0VEC_COUNT	16	/* number of lcl0 vectors */

#define SR_IBIT_PROF	(SR_IBIT8|SR_IBIT6)	/* R10K clk and sw prof clk */
#define SR_IBIT_BERR	SR_IBIT7

#define CAUSE_BERR_INTR	SR_IBIT_BERR

#define SR_HI_MASK	(SR_IBIT_PROF|SR_IBIT_BERR)
#define SR_SCHED_MASK	SR_HI_MASK
#define SR_PROF_MASK	SR_IBIT_BERR
#define SR_ALL_MASK	SR_IMASK8
#define SR_BERR_MASK	SR_ALL_MASK

/* bits in the HEART_BERR_ADDR register */
#define	HBA_ADDR	0x000000ffffffffff	/* address on SysAD */

/* bits in the HEART_BERR_MISC register */
#define	HBM_BAD_RD_WRBACK	(HEARTCONST 0x1 << 23)
#define	HBM_PROC_ID_SHFT	21
#define	HBM_PROC_ID		(HEARTCONST 0x3 << HBM_PROC_ID_SHFT)
#define HBM_SYNDROME_SHFT	13
#define HBM_SYNDROME		(HEARTCONST 0xff << HBM_SYNDROME_SHFT)
#define	HBM_SYSCMD_PAR		(HEARTCONST 0x1 << 12)	/* syscmd parity */
#define	HBM_SYSCMD		(HEARTCONST 0xfff << 0)	/* syscmd bus */
#define	HBM_SYSCMD_WR_CMD	(HEARTCONST 0x1 << 7)

/* bits in the HEART_MEM_ERR register */
#define	HME_REQ_SRC_SHFT	43
#define HME_REQ_SRC_MSK		(HEARTCONST 0x7 << HME_REQ_SRC_SHFT)
#define	HME_REQ_SRC_IO		(HEARTCONST 0x4 << HME_REQ_SRC_SHFT)
#define HME_ERR_TYPE_ADDR	(HEARTCONST 0x1 << 42)	/* address error */
#define	HME_SYNDROME_SHFT	34
#define HME_SYNDROME    	(HEARTCONST 0xff << HME_SYNDROME_SHFT)
#define HME_PHYS_ADDR   	0x00000003ffffffffL	/* fault address */

/* bits in the HEART_PIUR_ERR register (access error) */
#define HPE_ACC_TYPE_WR		(HEARTCONST 0x1 << 22)	/* write error */
#define	HPE_ACC_PROC_ID_SHFT	20
#define HPE_ACC_PROC_ID		(HEARTCONST 0x3 << HPE_ACC_PROC_ID_SHFT)
#define HPE_ACC_ERR_ADDR	(HEARTCONST 0xffff8 << 0)	/* address */
			/*  the erred access has the address 0x00_0FFx_xxxx. */
			/*  Hence, the upper address is redundant and omitted */

/* bits in the HEART_CAUSE register */
/* SysCorErr signal from the processor */
#define	HC_PE_SYS_COR_ERR_MSK	(HEARTCONST 0xf << 60)
#define	HC_PE_SYS_COR_ERR(x)	(HEARTCONST 0x1 << (x) + 60)
#define	HC_PIOWDB_OFLOW		(HEARTCONST 0x1 << 44)
#define	HC_PIORWRB_OFLOW	(HEARTCONST 0x1 << 43)
#define HC_PIUR_ACC_ERR		(HEARTCONST 0x1 << 42)
#define HC_BAD_SYSWR_ERR	(HEARTCONST 0x1 << 41)
#define HC_BAD_SYSRD_ERR	(HEARTCONST 0x1 << 40)
#define	HC_SYSSTATE_ERR_MSK	(HEARTCONST 0xf << 36)
#define	HC_SYSSTATE_ERR(x)	(HEARTCONST 0x1 << (x) + 36)
#define HC_SYSCMD_ERR_MSK	(HEARTCONST 0xf << 32)
#define HC_SYSCMD_ERR(x)	(HEARTCONST 0x1 << (x) + 32)
#define HC_NCOR_SYSAD_ERR_MSK	(HEARTCONST 0xf << 28)
#define HC_NCOR_SYSAD_ERR(x)	(HEARTCONST 0x1 << (x) + 28)
#define HC_COR_SYSAD_ERR_MSK	(HEARTCONST 0xf << 24)
#define HC_COR_SYSAD_ERR(x)	(HEARTCONST 0x1 << (x) + 24)
#define HC_DATA_ELMNT_ERR_MSK	(HEARTCONST 0xf << 20)
#define HC_DATA_ELMNT_ERR(x)	(HEARTCONST 0x1 << (x) + 20)
#define	HC_WIDGET_ERR		(HEARTCONST 0x1 << 16)
#define	HC_MEM_ADDR_ERR_PROC_MSK	(HEARTCONST 0xf << 4)
#define	HC_MEM_ADDR_ERR_PROC(x)	(HEARTCONST 0x1 << (x) + 4)
#define HC_MEM_ADDR_ERR_IO	(HEARTCONST 0x1 << 2)
#define HC_NCOR_MEM_ERR		(HEARTCONST 0x1 << 1)
#define HC_COR_MEM_ERR		(HEARTCONST 0x1 << 0)

/* per processor bus error, set PBErr(3:0) in the HEART ISR */
#define	HC_PBUS_ERR(x)		(HC_SYSSTATE_ERR(x) |		\
				 HC_SYSCMD_ERR(x) |		\
				 HC_NCOR_SYSAD_ERR(x) |		\
				 HC_COR_SYSAD_ERR(x) |		\
				 HC_DATA_ELMNT_ERR(x) |		\
				 HC_MEM_ADDR_ERR_PROC(x))

/* errors reported by the HEART exception interrupt, for all processors */
#define	HC_HEART_EXC		(HC_PE_SYS_COR_ERR_MSK |	\
				 HC_PIOWDB_OFLOW |		\
				 HC_PIORWRB_OFLOW |		\
				 HC_PIUR_ACC_ERR |		\
				 HC_BAD_SYSWR_ERR |		\
				 HC_BAD_SYSRD_ERR |		\
				 HC_WIDGET_ERR |		\
				 HC_MEM_ADDR_ERR_IO |		\
				 HC_NCOR_MEM_ERR |		\
				 HC_COR_MEM_ERR)

/* bits in the HEART_WID_ERR_TYPE register */
#define	ERRTYPE_XBAR_CREDIT_OVER	(HEARTCONST 0x1 << 29)
#define	ERRTYPE_XBAR_CREDIT_UNDER	(HEARTCONST 0x1 << 28)
#define	ERRTYPE_IO_NONHEAD		(HEARTCONST 0x1 << 27)
#define	ERRTYPE_IO_BAD_FORMAT		(HEARTCONST 0x1 << 26)
#define	ERRTYPE_IO_UNEXPECTED_RESP_ERR	(HEARTCONST 0x1 << 25)
#define	ERRTYPE_IORWRB_OFLOW_ERR	(HEARTCONST 0x1 << 24)
#define	ERRTYPE_IOR_CMD_ERR		(HEARTCONST 0x1 << 23)
#define	ERRTYPE_IOR_CMD_WARN		(HEARTCONST 0x1 << 22)
#define	ERRTYPE_IOR_INT_VEC_ERR		(HEARTCONST 0x1 << 21)
#define	ERRTYPE_IOR_INT_VEC_WARN	(HEARTCONST 0x1 << 20)
#define	ERRTYPE_LLP_RCV_WARM_RST	(HEARTCONST 0x1 << 18)
#define	ERRTYPE_LLP_RCV_LNK_RST		(HEARTCONST 0x1 << 17)
#define	ERRTYPE_LLP_RCV_SN_ERR		(HEARTCONST 0x1 << 16)
#define	ERRTYPE_LLP_RCV_CB_ERR		(HEARTCONST 0x1 << 15)
#define	ERRTYPE_LLP_RCV_SQUASH_DATA	(HEARTCONST 0x1 << 14)
#define	ERRTYPE_LLP_TX_RETRY_TIMEOUT	(HEARTCONST 0x1 << 13)
#define	ERRTYPE_LLP_TX_RETRY		(HEARTCONST 0x1 << 12)
#define	ERRTYPE_LLP_RCV_CNT_255		(HEARTCONST 0x1 << 11)
#define	ERRTYPE_LLP_TX_CNT_255		(HEARTCONST 0x1 << 10)
#define	ERRTYPE_PIO_RD_TIMEOUT		(HEARTCONST 0x1 << 3)
#define	ERRTYPE_PIO_WR_TIMEOUT		(HEARTCONST 0x1 << 2)
#define	ERRTYPE_PIO_XTLK_ACC_ERR	(HEARTCONST 0x1 << 1)
#define	ERRTYPE_PIO_WCR_ACC_ERR		(HEARTCONST 0x1 << 0)

#define	ERRTYPE_ALL			(ERRTYPE_XBAR_CREDIT_OVER |	\
					 ERRTYPE_XBAR_CREDIT_UNDER |	\
					 ERRTYPE_IO_NONHEAD |		\
					 ERRTYPE_IO_BAD_FORMAT |	\
					 ERRTYPE_IO_UNEXPECTED_RESP_ERR | \
					 ERRTYPE_IORWRB_OFLOW_ERR |	\
					 ERRTYPE_IOR_CMD_ERR |		\
					 ERRTYPE_IOR_CMD_WARN |		\
					 ERRTYPE_IOR_INT_VEC_ERR |	\
					 ERRTYPE_IOR_INT_VEC_WARN |	\
					 ERRTYPE_LLP_RCV_WARM_RST |	\
					 ERRTYPE_LLP_RCV_LNK_RST |	\
					 ERRTYPE_LLP_RCV_SN_ERR |	\
					 ERRTYPE_LLP_RCV_CB_ERR |	\
					 ERRTYPE_LLP_RCV_SQUASH_DATA |	\
					 ERRTYPE_LLP_TX_RETRY_TIMEOUT |	\
					 ERRTYPE_LLP_TX_RETRY |		\
					 ERRTYPE_LLP_RCV_CNT_255 |	\
					 ERRTYPE_LLP_TX_CNT_255 |	\
					 ERRTYPE_PIO_RD_TIMEOUT |	\
					 ERRTYPE_PIO_WR_TIMEOUT |	\
					 ERRTYPE_PIO_XTLK_ACC_ERR |	\
					 ERRTYPE_PIO_WCR_ACC_ERR)

/* bits in the HEART_WID_PIO_ERR_UPPER register */
#define HW_PIO_ERR_PROC_ID_SHFT		22
#define HW_PIO_ERR_PROC_ID		(HEARTCONST 0x3 << HW_PIO_ERR_PROC_ID_SHFT)
#define HW_PIO_ERR_UNC_ATTR		(HEARTCONST 0x3 << 20)
#define HW_PIO_ERR_SYSCMD		(HEARTCONST 0x7ff << 8)
#define HW_PIO_ERR_SYSCMD_WR_CMD	(HEARTCONST 0x1 << 15)
#define HW_PIO_ERR_ADDR			(HEARTCONST 0xff << 0)

/* bits in the HEART_WID_PIO_ERR_LOWER register */
#define	HW_PIO_ERR_LOWER_ADDR	(HEARTCONST 0xffffffff << 0)    /* PhysAddr */

/* bits in the HEART_WID_PIO_RTO_ADDR register */
#define	HW_PIO_RTO_ERR_PROC_ID_SHFT	18		
#define	HW_PIO_RTO_ERR_PROC_ID		(HEARTCONST 0x3 << HW_PIO_RTO_ERR_PROC_ID_SHFT)
#define	HW_PIO_RTO_ERR_IOSPACE		(HEARTCONST 0x3 << 16)
#define	HW_PIO_RTO_ERR_DIDN		(HEARTCONST 0xf << 12)
#define	HW_PIO_RTO_ERR_ADDR		(HEARTCONST 0xfff << 0)

/* System memory -- 31.5 GB to 08 0000 0000 */
#define	SYSTEM_MEMORY_ALIAS_SIZE	0x00004000
#define SYSTEM_MEMORY_BASE		0x20000000

#define	ABS_SYSTEM_MEMORY_BASE		0x20000000
#define	ABS_SYSTEM_MEMORY_TOP		0x800000000

#define PHYS_RAMBASE            SYSTEM_MEMORY_BASE
#define K0_RAMBASE              PHYS_TO_K0(PHYS_RAMBASE)
#define K1_RAMBASE              PHYS_TO_K1(PHYS_RAMBASE)
#define PHYS_TO_K0_RAM(x)       PHYS_TO_K0((x)+PHYS_RAMBASE)
#define PHYS_TO_K1_RAM(x)       PHYS_TO_K1((x)+PHYS_RAMBASE)

#define	TID_MEMORY		HEART_ID	/* local memory target id */

/* The Heart chip is the system interrupt controller.  Direct all
 * interrupts to its target id, and interrupt address.
 */
#define INT_DEST_TARGET		HEART_ID
#define INT_DEST_ADDR		HEART_XTALK_ISR

#define INT_DEST_UPPER(vec) \
	(((vec)<<24) | (INT_DEST_TARGET<<16) |(int)((INT_DEST_ADDR>>32)&0xffff))
#define INT_DEST_LOWER 		(INT_DEST_ADDR&0xffffffff)

#define	HEART_CREDIT	4

/* Xtalk is normally 400 Mhz (4*IO clock).  Heart updates every 8 IO clock
 * cycles.
 */
#if EMULATION || SABLE
#define X_CLK_RATE		((1 * 1000 * 1000) * 2)
#else
#define X_CLK_RATE		((100 * 1000 * 1000) * 4)
#endif
#define HEART_IOCLK_FREQ        (X_CLK_RATE / 4)
#define HEART_COUNT_RATE	(HEART_IOCLK_FREQ / 8)
/* Heart count is 80 ns */
#define HEART_COUNT_NSECS ((1000 * 1000 * 1000)/HEART_COUNT_RATE)

#if LANGUAGE_C

typedef volatile struct heart_cfg_s {	/* at HEART_BASE (0x00_1w00_0000) */
	widget_cfg_t		h_widget;		/* +0x00 */

#define	h_wid_id		h_widget.w_id			/* +0x04 */
#define	h_wid_status		h_widget.w_status		/* +0x0c */
#define	h_wid_err_upper_addr	h_widget.w_err_upper_addr	/* +0x14 */
#define	h_wid_err_lower_addr	h_widget.w_err_lower_addr	/* +0x1c */
#define	h_wid_control		h_widget.w_control		/* +0x24 */
#define	h_wid_req_timeout	h_widget.w_req_timeout		/* +0x2c */
    /* no wid_intdest_upper	h_widget.w_intdest_upper_addr	   +0x34 */
    /* no wid_intdest_lower	h_widget.w_intdest_lower_addr	   +0x3c */
#define	h_wid_err_cmd_word	h_widget.w_err_cmd_word		/* +0x44 */
#define	h_wid_llp_cfg		h_widget.w_llp_cfg		/* +0x4c */
#define	h_wid_tflush		h_widget.w_tflush		/* +0x54 */

	heartreg_t		h_wid_reserved_0;	/* +0x58 */
	heartreg_t		h_wid_err_type;		/* +0x60 */
	heartreg_t		h_wid_err_mask;		/* +0x68 */
	heartreg_t		h_wid_pio_err_upper;	/* +0x70 */
	heartreg_t		h_wid_pio_err_lower;	/* +0x78 */
	heartreg_t		h_wid_isr;		/* +0x80 */
	heartreg_t		h_wid_reserved_1;	/* +0x88 */
	heartreg_t		h_wid_pio_rdto_addr;	/* +0x90 */
} heart_cfg_t;

typedef volatile struct heart_piu_s {	/* at HEART_PIU_BASE (0x00_0FF0_0000) */
	heartreg_t		h_mode;			/* +0x00000 */
	heartreg_t		h_sdram_mode;		/* +0x00008 */
	heartreg_t		h_mem_ref;		/* +0x00010 */
	heartreg_t		h_mem_req_arb;		/* +0x00018 */

	union {
		heartreg_t	reg[4];		/* by 64-bit hw registers */
		__uint32_t	bank[8];	/* per-bank data */
	}			h_memcfg;		/* +0x00020 */

	heartreg_t		h_fc_mode;		/* +0x00040 */
	heartreg_t		h_fc_timer_limit;	/* +0x00048 */
	heartreg_t		h_fc_addr[2];		/* +0x00050 */
	heartreg_t		h_fc_cr_cnt[2];		/* +0x00060 */
	heartreg_t		h_fc_timer[2];		/* +0x00070 */
	heartreg_t		h_status;		/* +0x00080 */
	heartreg_t		h_berr_addr;		/* +0x00088 */
	heartreg_t		h_berr_misc;		/* +0x00090 */
	heartreg_t		h_memerr_addr;		/* +0x00098 */
	heartreg_t		h_memerr_data;		/* +0x000a0 */
	heartreg_t		h_piur_acc_err;		/* +0x000a8 */
	heartreg_t		h_mlan_clk_div;		/* +0x000b0 */
	heartreg_t		h_mlan_ctl;		/* +0x000b8 */
	char __pad0[0x00F40];
	heartreg_t		h_piu_undef;		/* +0x01000 undefined reg, for diagnostics */
	char __pad1[0x0EFF8];
	heartreg_t		h_imr[4];		/* +0x10000 */
	heartreg_t		h_set_isr;		/* +0x10020 */
	heartreg_t		h_clr_isr;		/* +0x10028 */
	heartreg_t		h_isr;			/* +0x10030 */
	heartreg_t		h_imsr;			/* +0x10038 */
	heartreg_t		h_cause;		/* +0x10040 */
	char __pad2[0x0FFB8];
	heartreg_t		h_count;		/* +0x20000 */
	char __pad3[0x0FFF8];
	heartreg_t		h_compare;		/* +0x30000 */
	char __pad4[0x0FFF8];
	heartreg_t		h_trigger;		/* +0x40000 */
	char __pad5[0x0FFF8];
	heartreg_t		h_prid;			/* +0x50000 */
	char __pad6[0x0FFF8];
	heartreg_t		h_sync;			/* +0x60000 */
} heart_piu_t;

typedef struct {
    unsigned		pad	:12;
    unsigned		cpu	:2;
    unsigned		ios	:2;
    unsigned		didn	:4;
    unsigned		addr	:12;
}	h_wid_pio_rdto_addr_f;

typedef union {
    widgetreg_t			r;
    h_wid_pio_rdto_addr_f	f;
}	h_wid_pio_rdto_addr_u;

#if LANGUAGE_ASSEMBLY || _STANDALONE	/* use struct in kernel */
/*
 * HEARTREG takes as a parameter one of the heart register name macros
 * supplied above. The resulting construct is a volatile lvalue
 * representing the specified register on the heart chip addressed
 * through the COMPATK1 address space.
 */
#define	HEARTREG(phys)	(*((volatile heartreg_t *) PHYS_TO_COMPATK1((phys))))
#endif

#ifdef _STANDALONE
void init_heart(heart_piu_t *heart, heart_cfg_t *heartcfg);
void heart_clearnofault(void);
#elif _KERNEL
void heart_mp_intr_init(cpuid_t);
#endif	/* _STANDALONE/_KERNEL */

/* ========================================================================
 */

#if defined(MACROFIELD_LINE)
/*
 * This table forms a relation between the byte offset macros normally
 * used for ASM coding and the calculated byte offsets of the fields
 * in the C structure.
 *
 * See heart_check.c heart_html.c for further details.
 */
#ifndef MACROFIELD_LINE_BITFIELD
#define MACROFIELD_LINE_BITFIELD(m)     /* ignored */
#endif

struct macrofield_s heart_piu_macrofield[] = {

/* PIU BASE */
MACROFIELD_LINE(HEART_PIU_MODE, piu_t,		h_mode)
MACROFIELD_LINE_BITFIELD(HM_PROC_DISABLE_MSK)
MACROFIELD_LINE_BITFIELD(HM_MAX_PSR)
MACROFIELD_LINE_BITFIELD(HM_MAX_IOSR)
MACROFIELD_LINE_BITFIELD(HM_MAX_PEND_IOSR)
MACROFIELD_LINE_BITFIELD(HM_TRIG_SRC_SEL_MSK)
MACROFIELD_LINE_BITFIELD(HM_TRIG_HEART_EXC)
MACROFIELD_LINE_BITFIELD(HM_TRIG_REG_BIT)
MACROFIELD_LINE_BITFIELD(HM_TRIG_SYSCLK)
MACROFIELD_LINE_BITFIELD(HM_TRIG_MEMCLK_2X)
MACROFIELD_LINE_BITFIELD(HM_TRIG_MEMCLK)
MACROFIELD_LINE_BITFIELD(HM_TRIG_IOCLK)
MACROFIELD_LINE_BITFIELD(HM_PIU_TEST_MODE)
MACROFIELD_LINE_BITFIELD(HM_GP_FLAG_MSK)
MACROFIELD_LINE_BITFIELD(HM_MAX_PROC_HYST)
MACROFIELD_LINE_BITFIELD(HM_LLP_WRST_AFTER_RST)
MACROFIELD_LINE_BITFIELD(HM_LLP_LINK_RST)
MACROFIELD_LINE_BITFIELD(HM_LLP_WARM_RST)
MACROFIELD_LINE_BITFIELD(HM_COR_ECC_LCK)
MACROFIELD_LINE_BITFIELD(HM_REDUCED_PWR)
MACROFIELD_LINE_BITFIELD(HM_COLD_RST)
MACROFIELD_LINE_BITFIELD(HM_SW_RST)
MACROFIELD_LINE_BITFIELD(HM_MEM_FORCE_WR)
MACROFIELD_LINE_BITFIELD(HM_DB_ERR_GEN)
MACROFIELD_LINE_BITFIELD(HM_SB_ERR_GEN)
MACROFIELD_LINE_BITFIELD(HM_CACHED_PIO_EN)
MACROFIELD_LINE_BITFIELD(HM_CACHED_PROM_EN)
MACROFIELD_LINE_BITFIELD(HM_PE_SYS_COR_ERE)
MACROFIELD_LINE_BITFIELD(HM_GLOBAL_ECC_EN)
MACROFIELD_LINE_BITFIELD(HM_IO_COH_EN)
MACROFIELD_LINE_BITFIELD(HM_INT_EN)
MACROFIELD_LINE_BITFIELD(HM_DATA_CHK_EN)
MACROFIELD_LINE_BITFIELD(HM_REF_EN)
MACROFIELD_LINE_BITFIELD(HM_BAD_SYSWR_ERE)
MACROFIELD_LINE_BITFIELD(HM_BAD_SYSRD_ERE)
MACROFIELD_LINE_BITFIELD(HM_SYSSTATE_ERE)
MACROFIELD_LINE_BITFIELD(HM_SYSCMD_ERE)
MACROFIELD_LINE_BITFIELD(HM_NCOR_SYS_ERE)
MACROFIELD_LINE_BITFIELD(HM_COR_SYS_ERE)
MACROFIELD_LINE_BITFIELD(HM_DATA_ELMNT_ERE)
MACROFIELD_LINE_BITFIELD(HM_MEM_ADDR_PROC_ERE)
MACROFIELD_LINE_BITFIELD(HM_MEM_ADDR_IO_ERE)
MACROFIELD_LINE_BITFIELD(HM_NCOR_MEM_ERE)
MACROFIELD_LINE_BITFIELD(HM_COR_MEM_ERE)
MACROFIELD_LINE_BITFIELD(HM_MEM_ERE)

MACROFIELD_LINE(HEART_PIU_SDRAM_MODE, piu_t,	h_sdram_mode)
MACROFIELD_LINE(HEART_PIU_MEM_REF, piu_t,	h_mem_ref)
MACROFIELD_LINE_BITFIELD(HEART_MEMREF_REFS(0xf))
MACROFIELD_LINE_BITFIELD(HEART_MEMREF_PERIOD(0xffff))
MACROFIELD_LINE(HEART_PIU_MEM_REQ_ARB, piu_t,	h_mem_req_arb)
MACROFIELD_LINE_BITFIELD(HEART_MEMARB_IODIS)
MACROFIELD_LINE_BITFIELD(HEART_MEMARB_MAXPMWRQS)
MACROFIELD_LINE_BITFIELD(HEART_MEMARB_MAXPMRRQS)
MACROFIELD_LINE_BITFIELD(HEART_MEMARB_MAXPMRQS)
MACROFIELD_LINE_BITFIELD(HEART_MEMARB_MAXRRRQS)
MACROFIELD_LINE_BITFIELD(HEART_MEMARB_MAXGBRRQS)
MACROFIELD_LINE(HEART_PIU_MEMCFG0, piu_t,	h_memcfg.reg[0])
MACROFIELD_LINE_BITFIELD(HEART_MEMCFG_VLD)
MACROFIELD_LINE_BITFIELD(HEART_MEMCFG_RAM_MSK)
MACROFIELD_LINE_BITFIELD(HEART_MEMCFG_ADDR_MSK)

MACROFIELD_LINE(HEART_PIU_MEMCFG0, piu_t,	h_memcfg.reg[0])
MACROFIELD_LINE(HEART_PIU_MEMCFG(3), piu_t,	h_memcfg.reg[3])

MACROFIELD_LINE(HEART_PIU_FC_MODE, piu_t,	h_fc_mode)
MACROFIELD_LINE(HEART_PIU_FC_TIMER_LIM, piu_t,	h_fc_timer_limit)
MACROFIELD_LINE(HEART_PIU_FC0_ADDR, piu_t,	h_fc_addr[0])
MACROFIELD_LINE(HEART_PIU_FC1_ADDR, piu_t,	h_fc_addr[1])
MACROFIELD_LINE(HEART_PIU_FC0_CR_CNT, piu_t,	h_fc_cr_cnt[0])
MACROFIELD_LINE(HEART_PIU_FC1_CR_CNT, piu_t,	h_fc_cr_cnt[1])
MACROFIELD_LINE(HEART_PIU_FC0_TIMER, piu_t,	h_fc_timer[0])
MACROFIELD_LINE(HEART_PIU_FC1_TIMER, piu_t,	h_fc_timer[1])
MACROFIELD_LINE(HEART_PIU_STATUS, piu_t,	h_status)
MACROFIELD_LINE_BITFIELD(HEART_STAT_HSTL_SDRV)
MACROFIELD_LINE_BITFIELD(HEART_STAT_FC_CR_OUT(3))
MACROFIELD_LINE_BITFIELD(HEART_STAT_DIR_CNNCT)
MACROFIELD_LINE_BITFIELD(HEART_STAT_TRITON)
MACROFIELD_LINE_BITFIELD(HEART_STAT_R4K)
MACROFIELD_LINE_BITFIELD(HEART_STAT_BIG_ENDIAN)
MACROFIELD_LINE_BITFIELD(HEART_STAT_PROC_ACTIVE_MSK)
MACROFIELD_LINE_BITFIELD(HEART_STAT_WIDGET_ID)

MACROFIELD_LINE(HEART_PIU_BERR_ADDR, piu_t,	h_berr_addr)
MACROFIELD_LINE_BITFIELD(HBA_ADDR)

MACROFIELD_LINE(HEART_PIU_BERR_MISC, piu_t,	h_berr_misc)
MACROFIELD_LINE_BITFIELD(HBM_BAD_RD_WRBACK)
MACROFIELD_LINE_BITFIELD(HBM_PROC_ID)
MACROFIELD_LINE_BITFIELD(HBM_SYNDROME)
MACROFIELD_LINE_BITFIELD(HBM_SYSCMD_PAR)
MACROFIELD_LINE_BITFIELD(HBM_SYSCMD)
MACROFIELD_LINE_BITFIELD(HBM_SYSCMD_WR_CMD)

MACROFIELD_LINE(HEART_PIU_MERR_ADDR, piu_t,	h_memerr_addr)
MACROFIELD_LINE_BITFIELD(HME_REQ_SRC_MSK)
MACROFIELD_LINE_BITFIELD(HME_REQ_SRC_IO)
MACROFIELD_LINE_BITFIELD(HME_ERR_TYPE_ADDR)
MACROFIELD_LINE_BITFIELD(HME_SYNDROME)
MACROFIELD_LINE_BITFIELD(HME_PHYS_ADDR)

MACROFIELD_LINE(HEART_PIU_MERR_DATA, piu_t,	h_memerr_data)
MACROFIELD_LINE(HEART_PIU_EACC_REG, piu_t,	h_piur_acc_err)
MACROFIELD_LINE_BITFIELD(HPE_ACC_TYPE_WR)
MACROFIELD_LINE_BITFIELD(HPE_ACC_PROC_ID)
MACROFIELD_LINE_BITFIELD(HPE_ACC_ERR_ADDR)

MACROFIELD_LINE(HEART_PIU_MLAN_CLK_DIV, piu_t,	h_mlan_clk_div)
MACROFIELD_LINE(HEART_PIU_MLAN_CTL, piu_t,	h_mlan_ctl)

MACROFIELD_LINE(HEART_PIU_IMR0, piu_t,		h_imr[0])
MACROFIELD_LINE(HEART_PIU_IMR(3), piu_t,	h_imr[3])
MACROFIELD_LINE(HEART_PIU_SET_ISR, piu_t,	h_set_isr)
MACROFIELD_LINE(HEART_PIU_CLR_ISR, piu_t,	h_clr_isr)
MACROFIELD_LINE(HEART_PIU_ISR, piu_t,		h_isr)
MACROFIELD_LINE_BITFIELD(HEART_ISR_HEART_EXC)
MACROFIELD_LINE_BITFIELD(HEART_INT_LEVEL4)
MACROFIELD_LINE_BITFIELD(HEART_INT_LEVEL3)
MACROFIELD_LINE_BITFIELD(HEART_INT_LEVEL2)
MACROFIELD_LINE_BITFIELD(HEART_INT_LEVEL1)
MACROFIELD_LINE_BITFIELD(HEART_INT_LEVEL0)

MACROFIELD_LINE(HEART_PIU_IMSR, piu_t,		h_imsr)

MACROFIELD_LINE(HEART_PIU_CAUSE, piu_t,		h_cause)
MACROFIELD_LINE_BITFIELD(HC_HEART_EXC)
MACROFIELD_LINE_BITFIELD(HC_PE_SYS_COR_ERR_MSK)
MACROFIELD_LINE_BITFIELD(HC_PIOWDB_OFLOW)
MACROFIELD_LINE_BITFIELD(HC_PIORWRB_OFLOW)
MACROFIELD_LINE_BITFIELD(HC_PIUR_ACC_ERR)
MACROFIELD_LINE_BITFIELD(HC_BAD_SYSWR_ERR)
MACROFIELD_LINE_BITFIELD(HC_BAD_SYSRD_ERR)
MACROFIELD_LINE_BITFIELD(HC_SYSSTATE_ERR_MSK)
MACROFIELD_LINE_BITFIELD(HC_SYSCMD_ERR_MSK)
MACROFIELD_LINE_BITFIELD(HC_SYSCMD_ERR(0))
MACROFIELD_LINE_BITFIELD(HC_SYSCMD_ERR(1))
MACROFIELD_LINE_BITFIELD(HC_SYSCMD_ERR(2))
MACROFIELD_LINE_BITFIELD(HC_SYSCMD_ERR(3))
MACROFIELD_LINE_BITFIELD(HC_NCOR_SYSAD_ERR_MSK)
MACROFIELD_LINE_BITFIELD(HC_COR_SYSAD_ERR_MSK)
MACROFIELD_LINE_BITFIELD(HC_DATA_ELMNT_ERR_MSK)
MACROFIELD_LINE_BITFIELD(HC_WIDGET_ERR)
MACROFIELD_LINE_BITFIELD(HC_MEM_ADDR_ERR_PROC_MSK)
MACROFIELD_LINE_BITFIELD(HC_MEM_ADDR_ERR_IO)
MACROFIELD_LINE_BITFIELD(HC_NCOR_MEM_ERR)
MACROFIELD_LINE_BITFIELD(HC_COR_MEM_ERR)
MACROFIELD_LINE_BITFIELD(HC_PBUS_ERR(0))
MACROFIELD_LINE_BITFIELD(HC_PBUS_ERR(1))
MACROFIELD_LINE_BITFIELD(HC_PBUS_ERR(2))
MACROFIELD_LINE_BITFIELD(HC_PBUS_ERR(3))

MACROFIELD_LINE(HEART_PIU_COUNT, piu_t,		h_count)
MACROFIELD_LINE(HEART_PIU_COMPARE, piu_t,	h_compare)
MACROFIELD_LINE(HEART_PIU_TRIGGER, piu_t,	h_trigger)
MACROFIELD_LINE(HEART_PIU_PID, piu_t,		h_prid)
MACROFIELD_LINE(HEART_PIU_SYNC, piu_t,		h_sync)

};      /* heart_piu_macrofield[] */

/* per widget registers */
struct macrofield_s heart_iou_macrofield[] = {

MACROFIELD_LINE(WIDGET_ID, cfg_t,		h_wid_id)
MACROFIELD_LINE(WIDGET_STATUS, cfg_t,		h_wid_status)
MACROFIELD_LINE(WIDGET_ERR_UPPER_ADDR, cfg_t,	h_wid_err_upper_addr)
MACROFIELD_LINE(WIDGET_ERR_LOWER_ADDR, cfg_t,	h_wid_err_lower_addr)
MACROFIELD_LINE(WIDGET_CONTROL, cfg_t,		h_wid_control)
MACROFIELD_LINE(WIDGET_REQ_TIMEOUT, cfg_t,	h_wid_req_timeout)
MACROFIELD_LINE(WIDGET_ERR_CMD_WORD, cfg_t,	h_wid_err_cmd_word)
MACROFIELD_LINE(WIDGET_LLP_CFG, cfg_t,		h_wid_llp_cfg)
MACROFIELD_LINE(WIDGET_TFLUSH, cfg_t,		h_wid_tflush)

MACROFIELD_LINE(HEART_IOU_ETYPE, cfg_t,		h_wid_err_type)
MACROFIELD_LINE_BITFIELD(ERRTYPE_ALL)
MACROFIELD_LINE_BITFIELD(ERRTYPE_XBAR_CREDIT_OVER)
MACROFIELD_LINE_BITFIELD(ERRTYPE_XBAR_CREDIT_UNDER)
MACROFIELD_LINE_BITFIELD(ERRTYPE_IO_NONHEAD)
MACROFIELD_LINE_BITFIELD(ERRTYPE_IO_BAD_FORMAT)
MACROFIELD_LINE_BITFIELD(ERRTYPE_IO_UNEXPECTED_RESP_ERR)
MACROFIELD_LINE_BITFIELD(ERRTYPE_IORWRB_OFLOW_ERR)
MACROFIELD_LINE_BITFIELD(ERRTYPE_IOR_CMD_ERR)
MACROFIELD_LINE_BITFIELD(ERRTYPE_IOR_CMD_WARN)
MACROFIELD_LINE_BITFIELD(ERRTYPE_IOR_INT_VEC_ERR)
MACROFIELD_LINE_BITFIELD(ERRTYPE_IOR_INT_VEC_WARN)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_RCV_WARM_RST)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_RCV_LNK_RST)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_RCV_SN_ERR)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_RCV_CB_ERR)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_RCV_SQUASH_DATA)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_TX_RETRY_TIMEOUT)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_TX_RETRY)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_RCV_CNT_255)
MACROFIELD_LINE_BITFIELD(ERRTYPE_LLP_TX_CNT_255)
MACROFIELD_LINE_BITFIELD(ERRTYPE_PIO_RD_TIMEOUT)
MACROFIELD_LINE_BITFIELD(ERRTYPE_PIO_WR_TIMEOUT)
MACROFIELD_LINE_BITFIELD(ERRTYPE_PIO_XTLK_ACC_ERR)
MACROFIELD_LINE_BITFIELD(ERRTYPE_PIO_WCR_ACC_ERR)

MACROFIELD_LINE(HEART_IOU_EMASK, cfg_t,		h_wid_err_mask)
MACROFIELD_LINE(HEART_IOU_PIO_EUPPER, cfg_t,	h_wid_pio_err_upper)
MACROFIELD_LINE_BITFIELD(HW_PIO_ERR_PROC_ID)
MACROFIELD_LINE_BITFIELD(HW_PIO_ERR_UNC_ATTR)
MACROFIELD_LINE_BITFIELD(HW_PIO_ERR_SYSCMD)
MACROFIELD_LINE_BITFIELD(HW_PIO_ERR_SYSCMD_WR_CMD)
MACROFIELD_LINE_BITFIELD(HW_PIO_ERR_ADDR)

MACROFIELD_LINE(HEART_IOU_PIO_ELOWER, cfg_t,	h_wid_pio_err_lower)

MACROFIELD_LINE(HEART_IOU_ISR, cfg_t,		h_wid_isr)

MACROFIELD_LINE(HEART_IOU_PIO_RTO, cfg_t,	h_wid_pio_rdto_addr)
MACROFIELD_LINE_BITFIELD(HW_PIO_RTO_ERR_PROC_ID)
MACROFIELD_LINE_BITFIELD(HW_PIO_RTO_ERR_IOSPACE)
MACROFIELD_LINE_BITFIELD(HW_PIO_RTO_ERR_DIDN)
MACROFIELD_LINE_BITFIELD(HW_PIO_RTO_ERR_ADDR)

MACROFIELD_LINE_BITFIELD(XBOW_BASE)
MACROFIELD_LINE_BITFIELD(HEART_BASE)
MACROFIELD_LINE_BITFIELD(BRIDGE_BASE)

};      /* heart_iou_macrofield[] */

#endif  /* MACROFIELD_LINE */

void	heart_mlreset(void);
int	heart_rev(void);
heartreg_t	heart_imr_bits_rmw(cpuid_t, heartreg_t, heartreg_t);

/* gfx flow control routines */
void heart_flow_control_connect(int, intr_func_t);
void heart_flow_control_disconnect(int);
heartreg_t heart_flow_control_enable(int);
heartreg_t heart_flow_control_disable(int);

#endif	/* LANGUAGE_C */

#endif /* __RACER_HEART_H__ */
