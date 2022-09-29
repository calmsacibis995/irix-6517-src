/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#ifndef _SYS_EVEREST_DANG_H_
#define _SYS_EVEREST_DANG_H_

#ident "$Revision: 1.21 $"

#ifdef _LANGUAGE_C

#include <sys/types.h>
#include <sys/reg.h>
#include <sys/giobus.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/io4.h>

#define	DANG_MAX_INDX	(16*8)	/* slots for IO4 * Adapters per IO4 */
#define	DANG_SLOT2INX(s,p)	((s * 8)+(p))
#define	DANG_INX2SLOT(i)	((i) / 8)
#define	DANG_INX2ADAP(i)	((i) % 8)

typedef	unsigned int	dangid_t	;

/*
 * byte address get and set
 */
#define DANG_GET(_r, _i, _reg) \
        (evreg_t)load_double((long long *)(SWIN_BASE((_r), (_i)) + (_reg)))
#define DANG_SET(_r, _i, _reg, _value) \
        store_double((long long *)(SWIN_BASE((_r),(_i)) + (_reg)), (long long)(_value))

/*
 * unswizzles the register offset bit shifts below - makes the array
 * indices back into pointers
 */
#define DANG_BADDR(_value) ((int)((_value) << 3))

/*
 * get and set routines that accept the swizzled array indices
 */
#define DANG_AGET(_r, _i, _reg) \
	DANG_GET((_r), (_i), DANG_BADDR(_reg))
#define DANG_ASET(_r, _i, _reg, _value) \
	DANG_SET((_r), (_i), DANG_BADDR(_reg), (_value))

#endif /* _LANGUAGE_C */


/* DANG chip register locations, relate to beginning of the DANG chip */
/* Refer to DANG chip hardware spec for details			      */
#define		DANG_IRS_ENABLE		0x0001	/* Enable the interrupt */
#define		DANG_IRS_CERR		0x0002	/* dang chip error */
#define		DANG_IRS_DMMC		0x0004	/* DMA master module complete */
#define		DANG_IRS_WGFL		0x0008	/* WG FIFO Low */
#define		DANG_IRS_WGFH		0x0010	/* WG FIFO High */
#define		DANG_IRS_WGFF		0x0020	/* WG FIFO Full */
#define		DANG_IRS_WGPERR		0x0040	/* WG privilege error */
#define		DANG_IRS_GIO0		0x0080	/* GIO Interrupt 0 */
#define		DANG_IRS_GIO1		0x0100	/* GIO Interrupt 1 */
#define		DANG_IRS_GIO2		0x0200	/* GIO Interrupt 2 */
#define		DANG_IRS_GIOSTAT	0x0400	/* GIO Status */
#define		DANG_IRS_DMASYNC	0x0800	/* DMA Sync */
#define		DANG_IRS_GFXDLY		0x1000	/* GFXDLY */
#define		DANG_IRS_GIOTIMEOUT	0x2000	/* GIO Timeout occured */

/* DANG Rev-B (and later) specific definitions */
#define	DANG_VERSION_MASK	0xF00	/* Portion of PIO_ERR reg */
#define	DANG_VERSION_SHFT	0x8	/* Shift 8 bits to right  */
#define	DANG_VERSION_B		0x2
#define	DANG_VERSION_C		0x3


#ifdef	DANG_REG_ADDRESS
#define	OLD_STYLE
#endif

#ifdef	DANG_REG_INDEX
#undef	OLD_STYLE
#endif

#ifdef	OLD_STYLE	/* As used in DANG network code */

#define	DANG_VERSION_REG	0x0000
#define DANG_PIO_ERR		0x0000
#define DANG_PIO_ERR_CLR	0x0008
#define DANG_BUS_ERR_CMD	0x0010		/* OBSOLETE - in for compat. */
#define DANG_UPPER_GIO_ADDR	0x0018		/* Upper 6 bits of GIO address,
						   default = 000111 */
#define DANG_MIDDLE_GIO_ADDR	0x0020		/* Middle 11 bits of GIO
						   address, default = 0 */
#define DANG_BIG_ENDIAN		0x0028		/* 1 for big endian,
						   default = 1 */
#define DANG_GIO64		0x0030		/* 1 = GIO 64, default = 1 */
#define DANG_PIPELINED		0x0038		/* 1 = pipelined, default = 1 */
#define DANG_GIORESET		0x0040		/* 0 to reset GIO_RESET_L */
#define DANG_AUDIO_ACTIVE	0x0048		/* 1 = audio active,
						   default = 0 */
#define DANG_AUDIO_SLOT		0x0050		/* audio devices 4 bit PIO slot
						   number; default is 0xE */
#define DANG_PIO_WG_WRTHRU	0x0058		/* 1 enables PIO writes through
						   write gatherer FIFO; default
						   is 0 */
#define	DANG_DRIVE_GIO_PARITY	0x0060
#define	DANG_IGNORE_GIO_TIMEOUT 0x0068
#define DANG_CLR_TIMEOUT	0x0070
#define	DANG_MAX_GIO_TIMEOUT	0x0078



#define DANG_DMAM_STATUS	0x0800
#define DANG_DMAM_ERR		0x0808
#define DANG_DMAM_ERR_CLR	0x0810
#define DANG_DMAM_MAX_OUTST	0x0818
#define DANG_DMAM_CACHE_LINECNT	0x0820
#define DANG_DMAM_START		0x0828
#define DANG_DMAM_RD_TIMEOUT	0x0830
#define DANG_DMAM_IBUS_ADDR	0x0838
#define DANG_DMAM_GIO_ADDR	0x0840
#define DANG_DMAM_COMPLETE	0x0848
#define DANG_DMAM_COMPLETE_CLR	0x0850
#define DANG_DMAM_KILL		0x0858
#define DANG_DMAM_PTE_ADDR	0x0860
#define	DANG_DMAM_IGNORE_GIO_PARITY	0x0870
#define	DANG_DMAM_IGNORE_IBUS_PARITY	0x0878

#define DANG_DMAS_STATUS	0x1000
#define DANG_DMAS_ERR		0x1008
#define DANG_DMAS_ERR_CLR	0x1010
#define DANG_DMAS_MAX_OUTST	0x1018
#define DANG_DMAS_CACHE_LINECNT	0x1020
#define DANG_DMAS_RD_TIMEOUT	0x1030
#define DANG_DMAS_IBUS_ADDR	0x1038
#define	DANG_DMAS_UPPER8_IBUS	0x1040
#define	DANG_DMAS_USE_UPPER8	0x1048

#define DANG_DMAS_IGNORE_GIO_PARITY	0x1058
#define	DANG_DMAS_IGNORE_IBUS_PARITY	0X1060



#define DANG_INTR_STATUS	0x1800
#define DANG_INTR_ENABLE	0x1808
#define DANG_INTR_ERROR		0x1810
#define DANG_INTR_GIO_0		0x1818
#define DANG_INTR_GIO_1		0x1820
#define DANG_INTR_GIO_2		0x1828
#define DANG_INTR_DMAM_COMPLETE	0x1830
#define DANG_INTR_PRIV_ERR	0x1838
#define DANG_INTR_LOWATER	0x1840
#define DANG_INTR_HIWATER	0x1848
#define DANG_INTR_FULL		0x1850
#define DANG_INTR_PAUSE		0x1858
#define DANG_INTR_BREAK		0x1860



#define DANG_WG_STATUS		0x2000
#define DANG_WG_LOWATER		0x2008
#define DANG_WG_HIWATER		0x2010
#define DANG_WG_FULL		0x2018
#define DANG_WG_PRIV_ERR_CLR	0x2020
#define DANG_WG_PRIV_LOADDR	0x2028
#define DANG_WG_PRIV_HIADDR	0x2030
#define DANG_WG_GIO_UPPER	0x2038	/* Upper 17bits of relative GIO address */
#define DANG_WG_GIO_STREAM	0x2040
#define DANG_WG_WDCNT		0x2048
#define DANG_WG_STREAM_WDCNT	0x2050
#define DANG_WG_FIFO_RDADDR	0x2058
#define DANG_WG_PAUSE		0x2060	/* 1 to pause the external fifo */
#define DANG_WG_STREAM_ALWAYS	0x2068
#define	DANG_WG_POP_FIFO	0x2070
 

#define DANG_WG_U_STATUS	0x3000
#define DANG_WG_U_WDCNT		0x3008

#else	/* OLD_STYLE  -- used by Dang graphics people */

/* Note that these constants are used as index into a 64-bit array, hence */
/* they are the chip register's byte address >> 3 			*/

#define	DANG_VERSION_REG	(0x0000>>3)
#define DANG_PIO_ERR		(0x0000>>3)
#define DANG_PIO_ERR_CLR	(0x0008>>3)
#define DANG_BUS_ERR_CMD	(0x0010>>3)	/* OBSOLETE - in for compat. */
#define DANG_UPPER_GIO_ADDR	(0x0018>>3)	/* Upper 6 bits of GIO address,
						   default = 000111 */
#define DANG_MIDDLE_GIO_ADDR	(0x0020>>3)	/* Middle 11 bits of GIO
						   address, default = 0 */
#define DANG_BIG_ENDIAN		(0x0028>>3)	/* 1 for big endian,
						   default = 1 */
#define DANG_GIO64		(0x0030>>3)	/* 1 = GIO 64, default = 1 */
#define DANG_PIPELINED		(0x0038>>3)	/* 1 = pipelined, default = 1 */
#define DANG_GIORESET		(0x0040>>3)	/* 0 to reset GIO_RESET_L */
#define DANG_AUDIO_ACTIVE	(0x0048>>3)	/* 1 = audio active,
						   default = 0 */
#define DANG_AUDIO_SLOT		(0x0050>>3)	/* audio devices 4 bit PIO slot
						   number; default is 0xE */
#define DANG_PIO_WG_WRTHRU	(0x0058>>3)	/* 1 enables PIO writes through
						   write gatherer FIFO; default
						   is 0 */
#define	DANG_DRIVE_GIO_PARITY	(0x0060>>3)
#define	DANG_IGNORE_GIO_TIMEOUT (0x0068>>3)
#define DANG_CLR_TIMEOUT	(0x0070>>3)
#define	DANG_MAX_GIO_TIMEOUT	(0x0078>>3)



#define DANG_DMAM_STATUS	(0x0800>>3)
#define DANG_DMAM_ERR		(0x0808>>3)
#define DANG_DMAM_ERR_CLR	(0x0810>>3)
#define DANG_DMAM_MAX_OUTST	(0x0818>>3)
#define DANG_DMAM_CACHE_LINECNT	(0x0820>>3)
#define DANG_DMAM_START		(0x0828>>3)
#define DANG_DMAM_RD_TIMEOUT	(0x0830>>3)
#define DANG_DMAM_IBUS_ADDR	(0x0838>>3)
#define DANG_DMAM_GIO_ADDR	(0x0840>>3)
#define DANG_DMAM_COMPLETE	(0x0848>>3)
#define DANG_DMAM_COMPLETE_CLR	(0x0850>>3)
#define DANG_DMAM_KILL		(0x0858>>3)
#define DANG_DMAM_PTE_ADDR	(0x0860>>3)
#define	DANG_DMAM_IGNORE_GIO_PARITY	(0x0870>>3)
#define	DANG_DMAM_IGNORE_IBUS_PARITY	(0x0878>>3)



#define DANG_DMAS_STATUS	(0x1000>>3)
#define DANG_DMAS_ERR		(0x1008>>3)
#define DANG_DMAS_ERR_CLR	(0x1010>>3)
#define DANG_DMAS_MAX_OUTST	(0x1018>>3)
#define DANG_DMAS_CACHE_LINECNT	(0x1020>>3)
#define DANG_DMAS_RD_TIMEOUT	(0x1030>>3)
#define DANG_DMAS_IBUS_ADDR	(0x1038>>3)
#define	DANG_DMAS_UPPER8_IBUS	(0x1040>>3)
#define	DANG_DMAS_USE_UPPER8	(0x1048>>3)

#define DANG_DMAS_IGNORE_GIO_PARITY	(0x1058>>3)
#define	DANG_DMAS_IGNORE_IBUS_PARITY	(0X1060>>3)

#define DANG_INTR_STATUS	(0x1800>>3)
#define DANG_INTR_ENABLE	(0x1808>>3)
#define DANG_INTR_ERROR		(0x1810>>3)
#define DANG_INTR_GIO_0		(0x1818>>3)
#define DANG_INTR_GIO_1		(0x1820>>3)
#define DANG_INTR_GIO_2		(0x1828>>3)
#define DANG_INTR_DMAM_COMPLETE	(0x1830>>3)
#define DANG_INTR_PRIV_ERR	(0x1838>>3)
#define DANG_INTR_LOWATER	(0x1840>>3)
#define DANG_INTR_HIWATER	(0x1848>>3)
#define DANG_INTR_FULL		(0x1850>>3)
#define DANG_INTR_PAUSE		(0x1858>>3)
#define DANG_INTR_BREAK		(0x1860>>3)



#define DANG_WG_STATUS		(0x2000>>3)
#define DANG_WG_LOWATER		(0x2008>>3)
#define DANG_WG_HIWATER		(0x2010>>3)
#define DANG_WG_FULL		(0x2018>>3)
#define DANG_WG_PRIV_ERR_CLR	(0x2020>>3)
#define DANG_WG_PRIV_LOADDR	(0x2028>>3)
#define DANG_WG_PRIV_HIADDR	(0x2030>>3)
#define DANG_WG_GIO_UPPER	(0x2038>>3)	/* Upper 17bits of relative GIO address */
#define DANG_WG_GIO_STREAM	(0x2040>>3)
#define DANG_WG_WDCNT		(0x2048>>3)
#define DANG_WG_STREAM_WDCNT	(0x2050>>3)
#define DANG_WG_FIFO_RDADDR	(0x2058>>3)
#define DANG_WG_PAUSE		(0x2060>>3)	/* 1 to pause the external fifo */
#define DANG_WG_STREAM_ALWAYS	(0x2068>>3)
#define	DANG_WG_POP_FIFO	(0x2070>>3) 


#define DANG_WG_U_STATUS	(0x3000>>3)
#define DANG_WG_U_WDCNT		(0x3008>>3)
#endif	/* OLD_STYLE */

#if	defined(DANG_REG_ADDRESS ) || defined(DANG_REG_INDEX)
/* Donot continue the definition */
#undef	OLD_STYLE
#endif

#define DANG_PIO_ERR_IDATA	0x1		/* 1 if ibus data err */
#define DANG_PIO_ERR_ICOM	0x2		/* 1 if ibus command err */
#define DANG_PIO_ERR_ISUPR	0x4		/* ibus suprise - read resp */
#define	DANG_PIO_ERR_BERR	0x8		/* command or data bus err */
#define	DANG_PIO_ERR_BERRC	0x10		/* bus err on command */
#define	DANG_PIO_ERR_VERSION	0xf00		/* version number field */

/***************************************************************************/

/* DANG chip DMA registers bitfield defines, added by danac
   When possible, common stuff between DMA master and slave modules,
   etc will be defined once only, with the DANG_DMA_STAT_X names.
   Stuff peculiar to one module will get DANG_DMAM_STAT_X or DANG_DMAS_STAT_X
*/


/***************************************************************************/

#define DANG_DMA_STAT_BUSY	0x1		/* 1 if DMA module busy */
#define DANG_DMA_STAT_DIR	0x2		/* set if Ibus to GIO bus,
						   clear if GIO to Ibus */

#define DANG_DMAS_STAT_PRE	0x4		/* set if slave DMA was
						   preempted */

#define DANG_DMAM_STAT_IFETCH	0x4		/* fetching initial 4 words
						   of control block if set */
#define DANG_DMAM_STAT_SFETCH	0x8		/* fetching subsequent 4 words
						   of control block */
#define DANG_DMAM_STAT_CMP	0x10		/* DMA transaction complete;
						   duplicates DMA complete reg
						   data */

/* use as
status = DANG_DMA_IF_STATUS(interface_number, raw_status);
*/
#define DANG_DMA_IF_STATUS(_i, _s) \
	(long long)(((_s) >> (4 + (_i))) & 0x7)

/* interface status defines - again, most states are common to both the 
   DMA master and DMA slave modules.  These status numbers, not bit flags.
*/
#define DANG_DMA_IF_IDLE	0x0
#define DANG_DMA_IF_WSTART	0x1		/* waiting for valid PTE (M) or
						   address strobe (S) */
#define DANG_DMA_IF_RDEL	0x2		/* write mode, waiting for read
						   interface to finish.
						   read mode, undefined */
#define DANG_DMA_IF_WIBUS	0x3		/* waiting for Ibus */
#define DANG_DMA_IF_WGACK	0x4		/* waiting for GIO arb. ack (M)
						   undefined (S) */
#define DANG_DMA_IF_WIACK	0x5		/* waiting for Ibus arb. ack */
#define DANG_DMA_IF_STALLED	0x6
#define DANG_DMA_IF_BUSY	0x7

/* DMA Master Error flags */
#define DANG_DMAM_ERR_PIOKILL	0x1		/* xaction killed by a PIO */
#define DANG_DMAM_ERR_IRTOUT	0x2		/* Ibus read timeout */
#define DANG_DMAM_ERR_IPAR	0x4		/* Ibus parity error */
#define DANG_DMAM_ERR_GPAR	0x8		/* GIO parity error */
#define DANG_DMAM_ERR_CPAR	0x10		/* Control block parity error */

/* DMA Slave Error flags */
#define DANG_DMAS_ERR_IPAR	0x1		/* Ibus parity error */
#define DANG_DMAS_ERR_GCOM	0x2		/* GIO command parity error */
#define DANG_DMAS_ERR_GBYTE	0x4		/* GIO byte count parity err */
#define DANG_DMAS_ERR_GDATA	0x8		/* GIO data parity error */
#define DANG_DMAS_ERR_IRTOUT	0x10		/* Ibus read timeout */

/***************************************************************************/

/*
    DANG Interrupt module definitions
*/

/* DANG Interrupt status flags */
#define DANG_ISTAT_DCHIP	0x2		/* DANG chip error */
#define DANG_ISTAT_DMAM_CMP	0x4		/* DMA Master xaction complete
						   duplicates DMA comp reg */
#define DANG_ISTAT_WG_FLOW	0x8		/* WG FIFO reached low mark */
#define DANG_ISTAT_WG_FHI	0x10		/* WG FIFO reached high mark */
#define DANG_ISTAT_WG_FULL	0x20		/* WG FIFO full */
#define DANG_ISTAT_WG_PRIV	0x40		/* WG Privilege Error */
#define DANG_ISTAT_GIO0		0x80		/* GIO Int 0 */
#define DANG_ISTAT_GIO1		0x100		/* GIO Int 1 */
#define DANG_ISTAT_GIO2		0x200		/* GIO Int 2 */
#define DANG_ISTAT_GIOSTAT	0x400		/* GIO status */
#define DANG_ISTAT_DSYNC	0x800		/* DMA Sync */
#define DANG_ISTAT_GFXDLY	0x1000		/* GFXDLY asserted */

/* DANG Interrupt enables - OR the status flags, above, with one of the flags
   below and write to the Interrupt enable register */
#define DANG_IENA_SET		0x1		/* enable all 1's bits */
#define DANG_IENA_CLEAR		0x0		/* clear all 1's bits */

/***************************************************************************/

/*
    DANG Write Gatherer module definitions
*/

/* Write Gatherer Status register bits */
#define DANG_WGSTAT_IDLE	0x1		/* clear if busy, set if idle */
#define DANG_WGSTAT_WDATA	0x2		/* WG waiting for data word
						   clear means not waiting
						   set means WG has received an
						   address w/o data, so next
						   word will be used as data */
#define DANG_WGSTAT_PRIV	0x4		/* WG Privilege Error */

/* These Write Gatherer Status register bitfields are for debug only */
#define DANG_WGSTAT_FILL	0x18		/* WG fill state bit mask */
#define DANG_WGSTAT_WEXT	0xE0		/* WG write extern bit mask */
#define DANG_WGSTAT_DRAIN	0x300		/* WG drain state bit mask */

/*
 * Dang GIO PIO address window defines and macros
 */
#define	GIO_SWIN_SIZE		(SWIN_SIZE>>1)	/* half of little window avail */
						/* for gio bus addresses */
#define	GIO_SWIN_OFFSET		(SWIN_SIZE>>1)	/* offset to gio addresses in */
						/* dang little window */
#define	GIO_SWIN_BADDR_MASK	0x000007ff	/* GIO board address mask */
#define	GIO_SWIN_SLOT_MASK	0x03c00000	/* GIO slot mask */
#define	GIO_SWIN_SLOT_SHFT	11		/* shift to align slot address */
#define	GIO_BUS_MASK		0x03ffffff	/* GIO bus address mask */
#define	GIO_MADDR_MASK		0x003ff800	/* GIO middle adddress mask */
#define	GIO_MADDR_SHIFT		11		/* GIO middle adddress shift */
#define	GIO_UADDR_MASK		0xfc000000	/* GIO upper address mask */
#define	GIO_UADDR_SHIFT		26		/* GIO upper address mask */

#define	GIO_SWIN_SLOT(X)	(((X)&GIO_SWIN_SLOT_MASK)>>GIO_SWIN_SLOT_SHFT)
#define	GIO_SWIN_BOARD(X)	((X)&GIO_SWIN_BADDR_MASK)
#define	GIO_MIDDLE(X)		(((X)&GIO_MADDR_MASK)>>GIO_MADDR_SHIFT)
#define	GIO_UPPER(X)		(((X)&GIO_UADDR_MASK)>>GIO_UADDR_SHIFT)

#define	GIO_SLOT0_OFFSET	0x1f400000	/* offset to GIO slot 0 */
#define	GIO_SLOT0_PFN		((GIO_SLOT0_OFFSET&GIO_BUS_MASK)>>PTE_PFNSHFT)
#define	DANG_LWIN_MAXSIZE	0x00800000	/* map 8 Mbytes */

/*
 * Useful Macros to get Dang Adapter number 
 * IBUS_* picked from edt.h
 */
#define DANG_PADAP2XADAP(s,p)   ((s << IBUS_SLOTSHFT) + (p & IBUS_IOAMASK))

/*
 * Overloading 'slot' parameter in setgiovector routine..
 * 
 * Dang slot no  = DANG External Adapter no << 3 + GIO slot no 
 */
#define	DANG_EXADAP_SHFT		3
#define	DANG_EXADAP_MASK		0xff
#define	DANG_GIOSLOT_MASK		0x7

#define	DANG_SLOT(s,a)		(a << DANG_EXADAP_SHFT | s)

#define	DANG_GIO_SLOT(s)	(s & DANG_GIOSLOT_MASK)
#define	DANG_EXT_ADAP(s)	((s >> DANG_EXADAP_SHFT) & DANG_EXADAP_MASK)

/* GIO related defines */
#define PGSPERSHOT	32		/* Allow each DMA to be up to 32 pages */
#define	GIO_ADDR_STATIC	1
#define GIO_ADDR_INCR	0x0
#define GDMA_GTOH	0		/* DMA Mode: = Gfx to Host */
#define GDMA_HTOG	0x2		/* DMA Mode: = Host to Gfx */
#define DANG_DMA_NOINTR	0
#define DANG_DMA_INTR	0x4
#define DANG_NODMA_SYNC	0
#define DANG_DMA_SYNC	0x8

/*
 * Graphics DMA channel descriptor array element.  This is the hardware
 * set up used by the DANG chip.  Note that it must be in BIG endian
 * format.  The code will need to be different when indexing into ptrbuf
 * if we want it to work in LITTLE endian machines.
 */
#ifdef	_KERNEL
#if _LANGUAGE_C
typedef struct gdmada {
	unsigned short stride;		/* 16 bits */
	unsigned short bytecount;	/* 14 bits */
	unsigned short linecount;	/* 12 bits */
	unsigned short lsb_addr;	/* Lower 12 bits of 40 bit everest physical addr */
	unsigned int control;		/* Control bits */
					/* bit0 == 1 GIO addr is static, 
					        == 0 GIO addr increments */
					/* bit1 == 1 from host to GIO, 
					        == 0 from GIO to host */
					/* bit2 == 1 intr on DMA completion, 
					        == 0 don't interrupt */
					/* bit3 == 1 use DMA_SYNC signal, 
					        == 0 don't use DMA_SYNC signal */
	unsigned int GIOaddr;		/* Address of GIO for the DMA */
	unsigned int ptrbuf[PGSPERSHOT];/* array of 32bit pointers to buffers,  */
} gdmada_t;

/* A gdmada with much more space for ptrbuf so that we can treat each DMA line
 * as a separate buffer, and have PTEs to map each line.
 */
typedef struct gdmada_u {
	unsigned short stride;		/* 16 bits */
	unsigned short bytecount;	/* 14 bits */
	unsigned short linecount;	/* 12 bits */
	unsigned short lsb_addr;	/* Lower 12 bits of 40 bit everest physical addr */
	unsigned int control;		/* Control bits */
					/* bit0 == 1 GIO addr is static, 
					        == 0 GIO addr increments */
					/* bit1 == 1 from host to GIO, 
					        == 0 from GIO to host */
					/* bit2 == 1 intr on DMA completion, 
					        == 0 don't interrupt */
					/* bit3 == 1 use DMA_SYNC signal, 
					        == 0 don't use DMA_SYNC signal */
	unsigned int GIOaddr;		/* Address of GIO for the DMA */
	unsigned int ptrbuf[6*4096];	/* 4096 lines, each line can use up to 6 ptes */
} gdmada_u_t;

typedef	struct dang {
	__psunsigned_t	swin;		/* Location of small window */
	char		window;
	char		slot;
	char		adapter;
} dang_t;


/*
 * Macros to Calculate the Small window addresses
 * DANG_REGBASE - accepts slot/padap as the parameters, and evaluates the
 *		small window address which can be used to access dang regs
 * DANG_SLOT2GIOBASE - accepts slot/padap/gio-bus-address, and evaluates
 *		the small window address used to access GIO registers at
 *		'gio-bus-address'. gio-bus-address normally takes the values
 *		0x1f000000, 0x1f400000, and 0x1f600000
 *
 * DANG_INX2BASE - takes an index (evaluated as IO4-slot * Ibus-padap).
 *		 Return value similar to DANG_REGBASE
 *
 * DANG_INX2GIOBASE - takes an index (explained above) , and the gio-bus-addr.
 *		  Return value similar to DANG_SLOT2GIOBASE
 */

/* Macro accepting Dang slot,padap to calculate base address */

#define	DANG_SLOT2WIN(s)    (uint)((EV_GET_CONFIG((s), IO4_CONF_SW) >> 8) & 7)
#define	DANG_REGBASE(s, p)  SWIN_BASE(DANG_SLOT2WIN(s), (p))
#define	DANG_SLOT2GIOBASE(s, p, gioaddr) \
			(DANG_REGBASE(s, p) + GIO_SWIN_OFFSET + \
			GIO_SWIN_SLOT(gioaddr) + GIO_SWIN_BOARD(gioaddr))



/* Macro accpting Dang index to calculate base address */
#define	DANG_INX2BASE(inx)    DANG_REGBASE(DANG_INX2SLOT(inx), DANG_INX2ADAP(inx))
#define	DANG_INX2GIOBASE(inx, gioaddr)	\
			DANG_SLOT2GIOBASE(DANG_INX2SLOT(inx), \
					  DANG_INX2ADAP(inx), gioaddr)

#define	MAX_DANG_ID	((EV_MAX_SLOTS << IBUS_SLOTSHFT) + IO4_MAX_PADAPS)

/*
 * Extern declarations 
 */

extern int dang_error_intr(eframe_t *, void *);
extern int dang_error_intr_init(int);
extern int dang_probegio(int);
extern int dang_intr_conn(evreg_t *, int , EvIntrFunc, void *);
extern dangid_t	dang_id[];

#endif	/* _LANGUAGE_C */

#endif	/* _KERNEL */

#endif	/* _SYS_EVEREST_DANG_H_ */

