/*****************************************************************************
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 *****************************************************************************/

#ifndef __JPEG1_HW_H_
#define __JPEG1_HW_H_

#define	LOW_7(X)	(X & 0x7f)
#define	LOW_8(X)	(X & 0xff)
#define	LOW_10(X)	(X & 0x3ff)

#define COSMO_GIO_ID    0x9
#define	GIO_BUS_DELAY	15
extern int _jpeg_gio_bus_delay;

#define    JPEG_I2C_TIMEOUT          0x1000
#define    JPEG_I2C_TIMEOUT_DELAY    0x08
#define    JPEG_I2C_TIMEOUT_REPEAT   0x10  /* how often clients of I2C
                                           should attempt transfer. */
#define JPEG_I2C_READ(address)	(addr|1)
#define JPEG_I2C_WRITE(address)	(addr)
#define JPEG_I2C_MAX_SUBADDR	0xff
enum I2C_mode   {NO_I2C_MODE= 0, I2C_CTL, I2C_DATA, I2C_DATA_NP};

#ifndef KTRACE
#define TRACE0(T,S)
#define TRACE1(T,S,A)
#define TRACE2(T,S,A,B)
#define TRACE3(T,S,A,B,C)
#endif /* KTRACE */

/*
 * This is a software flag to indicate whether or not we
 * want to disable the hardware.   It does not make any
 * state change in the hardware.
 */
#define	HARDWARE_DISABLE(X)	{hardware_state = X;}

/*
 * Status return values.
 */
#define	STATUS_OK		0x00
#define	STATUS_ERR		-1
#define STATUS_NOTOK            0x01

#define STATUS_I2C_TIMEOUT	0x01
#if 0 
#define PHYS_TO_K1(x)   ((unsigned)(x)|0xA0000000)        /* physical to kseg1*/
#endif

/*
 * Base addresses of the various sections of JPEG board 
 * address space.
 */

#define	JPEGGFX_PHYS_ADDR			(0x1f000000)
#define JPEG0_PHYS_ADDR         		(0x1f400000)
#define JPEG1_PHYS_ADDR         		(0x1f600000)
#define JPEG_VIRT_ADDR(x)       		(PHYS_TO_K1(x))
#define JPEG_PHYS_ADDR  			(JPEG0_PHYS_ADDR)

#define JPEG_BOARD_ID_OFFSET			(0x00000000)
#define JPEG_BOARD_RESET_OFFSET			(0x00038000)
#define JPEG_560_REGS_OFFSET		    	(0x00140000)
#define JPEG_560_SWAP_REGS_OFFSET	    	(0x00148000) /* gone */
#define JPEG_TIMESTAMP_OFFSET			(0x00150008) 

/* TCR control group */
#define JPEG_NEXT_CC_TCR_ADDR_OFFSET		(0x00150020)
#define JPEG_NEXT_UCC_TCR_ADDR_OFFSET		(0x00150030)

/* DMA control group */
#define JPEG_DMA_BILL_SAID_JUNK_OFFSET		(0x00160000)
#define JPEG_DMA_CTRL_OFFSET			(0x00160004) /* all changed */
#define JPEG_DMA_INTERRUPT_OFFSET		(0x00160008) 
#define JPEG_DMA_INT_MASK_OFFSET		(0x0016000c)

#ifdef SINGLEWIDE
#define JPEG_SHORT_BURST_LENGTH_OFFSET		(0x00160010) /* new */   
#endif /* SINGLEWIDE */

#if defined(DMAC17) | defined(SINGLEWIDE)
#define JPEG_DMA_CC_REG_OFFSET			(0x00160014)
#define JPEG_DMA_UCC_REG_OFFSET			(0x00160018)
#endif /* DMAC17 | SINGLEWIDE */

#ifdef SINGLEWIDE
#define JPEG_GIO_HOLDOFF_OFFSET			(0x0016001c)
#endif /* SINGLEWIDE */

#define JPEG_VFC_REGS_OFFSET			(0x00170000)

#define JPEG_SAA7_CTL_OFFSET			(0x00180000)

#define JPEG_UIC_PIO_OFFSET			(0x00190000)
/* fifo control group */
#define JPEG_IDT_FIFO_OFFSET			(0x001A0000) /* Don't care */


#define GIO_DECOMP_HWM	0x100		/* may need to tune sometime */
#define GIO_COMP_LWM	0x65
#define GIO_COMP_HWM	0x1

#define JPEG0_GIO_IDENTR 		(JPEG0_PHYS_ADDR + JPEG_BOARD_ID_OFFSET)/* 1x32     */
#define JPEG0_BOARD_RESET		(JPEG0_PHYS_ADDR + JPEG_BOARD_RESET_OFFSET)
#define JPEG0_560_REGS   		(JPEG0_PHYS_ADDR + JPEG_560_REGS_OFFSET)
#define JPEG0_560_SWAP_REGS  		(JPEG0_PHYS_ADDR + JPEG_560_SWAP_REGS_OFFSET)
#define JPEG0_TIMESTAMP_REG  		(JPEG0_PHYS_ADDR + JPEG_TIMESTAMP_OFFSET)
#define JPEG0_NEXT_CC_TCR_ADDR_REG	(JPEG0_PHYS_ADDR + JPEG_NEXT_CC_TCR_ADDR_OFFSET)
#define JPEG0_NEXT_UCC_TCR_ADDR_REG	(JPEG0_PHYS_ADDR + JPEG_NEXT_UCC_TCR_ADDR_OFFSET)
#define JPEG0_DMA_CTRL_REG		(JPEG0_PHYS_ADDR + JPEG_DMA_CTRL_OFFSET)
#define JPEG0_DMA_INTERRUPT_REG		(JPEG0_PHYS_ADDR + JPEG_DMA_INTERRUPT_OFFSET)
#define JPEG0_DMA_INT_MASK_REG		(JPEG0_PHYS_ADDR + JPEG_DMA_INT_MASK_OFFSET)

#ifdef  SINGLEWIDE
#define JPEG0_SHORT_BURST_LENGTH	(JPEG0_PHYS_ADDR + JPEG_SHORT_BURST_LENGTH_OFFSET) 
#endif  /* SINGLEWIDE */

#if defined(DMAC17) | defined(SINGLEWIDE)
#define JPEG0_DMA_CC_REG		(JPEG0_PHYS_ADDR + JPEG_DMA_CC_REG_OFFSET)
#define JPEG0_DMA_UCC_REG		(JPEG0_PHYS_ADDR + JPEG_DMA_UCC_REG_OFFSET)
#endif  /* DMAC17 | SINGLEWIDE */

#ifdef  SINGLEWIDE
#define JPEG0_GIO_HOLDOFF		(JPEG0_PHYS_ADDR + JPEG_GIO_HOLDOFF_OFFSET) 
#endif  /* SINGLEWIDE */

#define JPEG0_VFC_REGS			(JPEG0_PHYS_ADDR + JPEG_VFC_REGS_OFFSET)
#define JPEG0_IDT_FIFO_REGS		(JPEG0_PHYS_ADDR + JPEG_IDT_FIFO_OFFSET)
#define JPEG0_SAA7_CTL   		(JPEG0_PHYS_ADDR + JPEG_SAA7_CTL_OFFSET) /* 2x8      */
#define JPEG0_UIC_PIO_REG		(JPEG0_PHYS_ADDR + JPEG_UIC_PIO_OFFSET) /* 1x32     */


#define JPEG1_GIO_IDENTR 		(JPEG1_PHYS_ADDR + JPEG_BOARD_ID_OFFSET)/* 1x32     */
#define JPEG1_BOARD_RESET		(JPEG1_PHYS_ADDR + JPEG_BOARD_RESET_OFFSET)
#define JPEG1_560_REGS   		(JPEG1_PHYS_ADDR + JPEG_560_REGS_OFFSET)
#define JPEG1_560_SWAP_REGS  		(JPEG1_PHYS_ADDR + JPEG_560_SWAP_REGS_OFFSET)
#define JPEG1_TIMESTAMP_REG  		(JPEG1_PHYS_ADDR + JPEG_TIMESTAMP_OFFSET)
#define JPEG1_NEXT_CC_TCR_ADDR_REG	(JPEG1_PHYS_ADDR + JPEG_NEXT_CC_TCR_ADDR_OFFSET)
#define JPEG1_NEXT_UCC_TCR_ADDR_REG	(JPEG1_PHYS_ADDR + JPEG_NEXT_UCC_TCR_ADDR_OFFSET)
#define JPEG1_DMA_CTRL_REG		(JPEG1_PHYS_ADDR + JPEG_DMA_CTRL_OFFSET)
#define JPEG1_DMA_INTERRUPT_REG		(JPEG1_PHYS_ADDR + JPEG_DMA_INTERRUPT_OFFSET)
#define JPEG1_DMA_INT_MASK_REG		(JPEG1_PHYS_ADDR + JPEG_DMA_INT_MASK_OFFSET)

#ifdef  SINGLEWIDE
#define JPEG1_SHORT_BURST_LENGTH	(JPEG1_PHYS_ADDR + JPEG_SHORT_BURST_LENGTH_OFFSET) 
#endif  /* SINGLEWIDE */

#if defined(DMAC17) | defined(SINGLEWIDE)
#define JPEG1_DMA_CC_REG		(JPEG1_PHYS_ADDR + JPEG_DMA_CC_REG_OFFSET)
#define JPEG1_DMA_UCC_REG		(JPEG1_PHYS_ADDR + JPEG_DMA_UCC_REG_OFFSET)
#endif  /* DMAC17 | SINGLEWIDE */

#ifdef  SINGLEWIDE
#define JPEG1_GIO_HOLDOFF		(JPEG1_PHYS_ADDR + JPEG_GIO_HOLDOFF_OFFSET) 
#endif  /* SINGLEWIDE */

#define JPEG1_VFC_REGS			(JPEG1_PHYS_ADDR + JPEG_VFC_REGS_OFFSET)
#define JPEG1_IDT_FIFO_REGS		(JPEG1_PHYS_ADDR + JPEG_IDT_FIFO_OFFSET)
#define JPEG1_SAA7_CTL   		(JPEG1_PHYS_ADDR + JPEG_SAA7_CTL_OFFSET) /* 2x8      */
#define JPEG1_UIC_PIO_REG		(JPEG1_PHYS_ADDR + JPEG_UIC_PIO_OFFSET) /* 1x32     */
#define JPEGGFX_GIO_IDENTR 		(JPEGGFX_PHYS_ADDR + JPEG_BOARD_ID_OFFSET)/* 1x32     */
#define JPEGGFX_BOARD_RESET		(JPEGGFX_PHYS_ADDR + JPEG_BOARD_RESET_OFFSET)
#define JPEGGFX_560_REGS   		(JPEGGFX_PHYS_ADDR + JPEG_560_REGS_OFFSET)
#define JPEGGFX_560_SWAP_REGS  		(JPEGGFX_PHYS_ADDR + JPEG_560_SWAP_REGS_OFFSET)
#define JPEGGFX_TIMESTAMP_REG  		(JPEGGFX_PHYS_ADDR + JPEG_TIMESTAMP_OFFSET)
#define JPEGGFX_NEXT_CC_TCR_ADDR_REG	(JPEGGFX_PHYS_ADDR + JPEG_NEXT_CC_TCR_ADDR_OFFSET)
#define JPEGGFX_NEXT_UCC_TCR_ADDR_REG	(JPEGGFX_PHYS_ADDR + JPEG_NEXT_UCC_TCR_ADDR_OFFSET)
#define JPEGGFX_DMA_CTRL_REG		(JPEGGFX_PHYS_ADDR + JPEG_DMA_CTRL_OFFSET)
#define JPEGGFX_DMA_INTERRUPT_REG	(JPEGGFX_PHYS_ADDR + JPEG_DMA_INTERRUPT_OFFSET)
#define JPEGGFX_DMA_INT_MASK_REG	(JPEGGFX_PHYS_ADDR + JPEG_DMA_INT_MASK_OFFSET)

#define JPEGGFX_SHORT_BURST_LENGTH	(JPEGGFX_PHYS_ADDR + JPEG_SHORT_BURST_LENGTH_OFFSET) 

#define JPEGGFX_DMA_CC_REG		(JPEGGFX_PHYS_ADDR + JPEG_DMA_CC_REG_OFFSET)
#define JPEGGFX_DMA_UCC_REG		(JPEGGFX_PHYS_ADDR + JPEG_DMA_UCC_REG_OFFSET)

#define JPEGGFX_GIO_HOLDOFF		(JPEGGFX_PHYS_ADDR + JPEG_GIO_HOLDOFF_OFFSET) 

#define JPEGGFX_VFC_REGS		(JPEGGFX_PHYS_ADDR + JPEG_VFC_REGS_OFFSET)
#define JPEGGFX_IDT_FIFO_REGS		(JPEGGFX_PHYS_ADDR + JPEG_IDT_FIFO_OFFSET)
#define JPEGGFX_SAA7_CTL   		(JPEGGFX_PHYS_ADDR + JPEG_SAA7_CTL_OFFSET) /* 2x8      */
#define JPEGGFX_UIC_PIO_REG		(JPEGGFX_PHYS_ADDR + JPEG_UIC_PIO_OFFSET) /* 1x32     */

#define DMA_DESC_STOP           0x80000000
#define DMA_DESC_INTEN          0x40000000
/*
 * Used in j_board->jpeg_intrcomm
 */
#define DMA_UCC_IN_PROGRESS	(1<<0)
#define DMA_CC_IN_PROGRESS	(1<<1)
#define DMA_UCC_COMPLETE	(1<<2)
#define DMA_CC_COMPLETE		(1<<3)
#define DMA_UCC_ERROR		(1<<4)
#define DMA_CC_ERROR		(1<<5)
#define VFC_INTERRUPT		(1<<6)
#define DMA_FIFO_PRIMED		(1<<7)
#define INTRCOMM_WANTED		(DMA_UCC_ERROR| \
				 DMA_UCC_COMPLETE| \
				 DMA_FIFO_PRIMED| \
				 DMA_CC_ERROR| \
				 DMA_CC_COMPLETE| \
				 VFC_INTERRUPT)


#define DEFAULT_GIO_FLAGS       0x0
#define DMA_SHORT_BURST_SIZE    96
#define CC_UCC_DMA_SIZE         0x400
#define MAX_TCR_DMA_SIZE        96 /* 0x800 is brok */

#define DMA_MAX_BURST_SIZE      0x4000 /* 0x800 */
#define DMA_BURST_SIZE(x)	(min(dma_max_burst_size, x))
#define RELAX_TIME		(1000)

#define min(x,y)                ( x < y ? x : y )

/* defaults for decompression */
#define D_DEFAULTQFactor		20
#define D_DEFAULTLeftBlanking   	0
#define D_DEFAULTRightBlanking	0
#define D_DEFAULTTopBlanking	10
#define D_DEFAULTBottomBlanking	16
#define D_DEFAULTCL560ImageWidth	640
#define D_DEFAULTCL560ImageHeight	240
/* defaults for compression */
#define C_DEFAULTQFactor		10
#define C_DEFAULTLeftBlanking   	0
#define C_DEFAULTRightBlanking	1
#define C_DEFAULTTopBlanking	10
#define C_DEFAULTBottomBlanking	11
#define C_DEFAULTCL560ImageWidth	640
#define C_DEFAULTCL560ImageHeight	240

/*
 ************************** Board registers ********************************
 */
#define _GIO_IDENTR             0
/*
 * TODO: add other registers here 
 */
#define num_board_register    19


/* TODO: must make dynamic */
#define CL560BaseAddress        (PHYS_TO_K1(JPEG0_560_REGS))


#define FIELD_BUFFER_COUNT 4
#define FIELD_BUFFER_SIZE  262224
#define FIELD_BUFFER_INIT_SIZE  100
#define FB_PTR_0	(1<<0)
#define FB_PTR_1	(1<<1)
#define FB_PTR_2	(1<<2)
#define FB_PTR_3	(1<<3)


/*
 *         V F C    R E G I S T E R S  
 */

typedef struct vfc_regs {
	unsigned	vfc_mode;
	unsigned	vfc_read;
	unsigned	vfc_read_ctl;
	unsigned	vfc_write;
	unsigned	vfc_write_ctl;
	unsigned	vfc_pix_width;
	unsigned	vfc_pix_height;
	unsigned	vfc_intr;
	unsigned	vfc_intr_mask;
#ifdef  SINGLEWIDE                      /* SINGLE WIDE ONLY */
	unsigned	vfc_d1_config;
	unsigned	vfc_rev;
#endif  /* SINGLEWIDE */
} VFC_REGS;

/*
 * VFC vfc_read_ctl, Image Control READ Register bit definitions
 */
#define R_ADV           0x00000001
#define R_RST           0x00000002
#define R_SKIP          0x00000004
#define R_AUTO          0x00000008
#define R_MANUAL        0x00000000

/*
 * VFC vfc_write_ctl Image Control WRITE Register bit definitions
 */
#define W_ADV           0x00000001
#define W_RST           0x00000002
#define W_FLUSH		0x4


/*
 * VFC vfc_mode Register bit definitions
 */
#define MODE_SIZE_32	0x00000001
#define FIELD30		0x00000002

#define VFC_MODE_DEST   0x0000001c
#define VFC_DEST_GIO	0x00000004
#define VFC_DEST_D1	0x00000008
#define VFC_DEST_CL560	0x00000010

#define VFC_MODE_SRC    0x000000E0
#define VFC_SRC_GIO	0x00000020
#define VFC_SRC_D1	0x00000040
#define VFC_SRC_CL560	0x00000080
#define VFC_SRC_SCALE   0x000000A0


#define JPEG_VFC_MODE_SRC_DEST_MASK   (VFC_MODE_DEST|VFC_MODE_SRC)

#ifdef  SINGLEWIDE			/* SINGLE WIDE ONLY */
#define HLmode			0x00000100
#define VFC_ODDFIELD    	0x00000200
#else   /* SINGLEWIDE */		/* DOUBLE WIDE ONLY */
#define VFC_ODDFIELD    0x00001000
#define VFC_D1_FORMAT_PAL	0x400	/* d1 activity is PAL  */
#define VFC_D1_FORMAT_NTSC	0x0	/* d1 activity is NTSC */
#define VFC_D1_FORMAT_ENABLE	0x800	/* enables d1 activity */
#define VFC_RESET		0x400	/* enables d1 activity */
#define PXMODE			0x00000300
#define GIO_INTERFACE   	0x001
#define D1_INTERFACE    	0x010
#define CL560_INTERFACE 	0x100


/*
 * VFC vfc_intr, and vfc_intr_mask  register definition
 */
#define D1_EOF		0x00000001
#define READ_ADV	0x00000002
#define WRITE_ADV	0x00000004
#define DECMP_EOF	0x00000008
#define ALL_VFC_INTERRUPTS	(D1_EOF|READ_ADV|WRITE_ADV|DECMP_EOF)
#endif  /* SINGLEWIDE */

#ifdef  SINGLEWIDE
/*
 * VFC vfc_intr, and vfc_intr_mask  register definition
 */
#define D1_EVENEOF		0x00000001
#define READ_ADV		0x00000002
#define WRITE_ADV		0x00000004
#define DECMP_EOF		0x00000008
#define D1_ODDEOF		0x00000010
#define ALL_VFC_INTERRUPTS	(D1_EVENEOF|READ_ADV|WRITE_ADV|\
				 DECMP_EOF|D1_ODDEOF)
/*
 * VFC vfc_config  register definition
 */

/*
 * PORT0 Definitions
 */
#define D1_PORT0		0x00000003
#define D1_PORT0_NULL		0x00000000
#define D1_PORT0_DBOB		0x00000002
#define D1_PORT0_COSMO		0x00000003


/*
 * PORT1 Definitions
 */
#define D1_PORT1		0x00000003
#define D1_PORT1_NULL		0x00000000
#define D1_PORT1_DBOB		0x00000008
#define D1_PORT1_COSMO		0x0000000c


/*
 * Input Source Definitions; 
 *	0::= port 0 is input
 *	1::= port 1 is input
 */
#define D1_INPUTSOURCE		0x00000010
#define D1_INPUTSOURCE_PORT0	0x00000000
#define D1_INPUTSOURCE_PORT1	0x00000010


/*
 * Pixel Replication mode Definitions
 *	00 - no scale or zoom on D1 port
 *	01 - scale in Y on input, Zoom in Y on output
 *	10 - scale in X on input, Zoom in Y on output
 *	11 - scale in both X and Y on input, Zoom 
 *	     in both x and y on output
 */
#define D1_PXMODE		0x00000060
#define D1_NOSCALE_ZOOM		0x0
#define D1_SCALE_ZOOM_Y		0x10
#define D1_SCALE_ZOOM_X		0x20
#define D1_SCALE_ZOOM_XY	0x30

/*
 * D1 Format definitions
 *	00 - no mode selected, interpret timimg from d1 
 *	01 - no mode selected, interpret timing from d1
 *	10 - NTSC mode
 *	11 - PAL mode
 */
#define D1_FORMAT		0x00000180
#define D1_FORMAT_NULL		0x00000000
#define D1_FORMAT_NTSC		0x00000100
#define D1_FORMAT_PAL		0x00000180

/*
 * ECC Enable, '1' ::= Enable, '0' ::= disable
 */
#define D1_ECC		0x00000200


/*
 * Carob Revision Register
 * 	PXP_RESET ::= 1, resets PXP section
 *	VFC_REST  ::= 1, resets VFC section
 */
#define CRB_REV_PXPRESET	0x00000001
#define CRB_REV_VFCRESET	0x00000002
#define CRB_REV			0x0000001c

#endif  /* SINGLEWIDE */
/*
 *          D M A    C O N T R O L   R E G I S T E R S 
 */
typedef struct dma_regs{ 
        unsigned        ctrl;
	unsigned	intr;
	unsigned	int_mask;
} DMA_REGS;

typedef struct dma_ic_regs{ 
	unsigned	cc_tcr_addr;
	unsigned	ucc_tcr_addr;
} DMA_IC_REGS;

/* 
 * front-end fifo to 560 control registers
 *
 * Direction       Water-Mark      [A4,A3,A2,A1,A0]
 * --------------------------------------------
 * Gio -> 560      low	ab_almost_empty		10000
 * Gio -> 560      high	ab_almost_full		10100 (used for decompression)
 * 560 -> Gio      low	ba_almost_empty		11000 (used for compression)
 * 560 -> Gio      high	ba_almost_full		11100
 * ---------------------------------------------
 */
typedef struct dma_fifo_regs { 
	unsigned	idt_fifo_data;
	unsigned	idt_fifo_bypass_data;
	unsigned	idt_fifo_pad1;
	unsigned	idt_fifo_pad2;
	unsigned	idt_fifo_ab_almost_empty;
	unsigned	idt_fifo_ab_almost_full;
	unsigned	idt_fifo_ba_almost_empty;
	unsigned	idt_fifo_ba_almost_full;
} IDT_FIFO_REGS;

/*
 * NEW (Rev C17 and earlier includeing Single Wide ) 
 * DMA_CTRL_REG bit definitions
 */
#if defined(DMAC17) | defined(SINGLEWIDE)
/*
 *	 DMA Compressed Channel Register
 */
#define CC_RUN			0x00000001
#define CC_DIR			0x00000002
/*
 *	DMA Uncompressed Channel Register
 */
#define UCC_RUN			0x00000001
#define UCC_DIR			0x00000002
/*
 * 	MISC DMA Rest bits
 */
#define FIFO_RST_N		0x00000001
#define CL560_RST_N		0x00000002
#define IIC_RST_N		0x00000004

#ifdef  SINGLEWIDE
#define TIMESTAMP_RST_N		0x00000008
#define ALL_DMA_RESETS  (FIFO_RST_N|CL560_RST_N|IIC_RST_N|TIMESTAMP_RST_N)
/*
 * 	IDT FIFO Status registers
 */
#define FIFO_EMPTY_A		0x00000010
#define FIFO_FULL_A		0x00000020
#define FIFO_ALMOST_EMPTY_A	0x00000040
#define FIFO_ALMOST_FULL_A	0x00000080
#define FIFO_EMPTY_B		0x00000100
#define FIFO_FULL_B		0x00000200
#define FIFO_ALMOST_EMPTY_B	0x00000400
#define FIFO_ALMOST_FULL_B	0x00000800
#else  /* SINGLEWIDE */ 
#define ALL_DMA_RESETS  (FIFO_RST_N|CL560_RST_N|IIC_RST_N)
#endif  /* SINGLEWIDE */ 
#endif  /* DMAC17 | SINGLEWIDE */

/*
 * OLD (Rev C15 and earlier) DMA_CTRL_REG bit definitions
 */
#if !defined(DMAC17) & !defined(SINGLEWIDE)
#define CC_RUN			0x00000001
#define UCC_RUN			0x00000002
#define CC_DIR			0x00000004
#define UCC_DIR			0x00000008
#define FIFO_RST_N		0x00000010
#define CL560_RST_N		0x00000020
#define IIC_RST_N		0x00000040
#endif /* !DMAC17 & !SINGLEWIDE */


/*
 * CC_RUN: Compressed channel dma run bit.
 *       CC_RUN = 1, CC_DIR = 1 => initiate dma from main memory to CL560.
 *       CC_RUN = 1, CC_DIR = 0 => initiate dma from CL560 to main memory.
 *               bit is set by PIO to initiate dma and reset by
 *               dma_engine at completion of transfer.
 *
 * UCC_RUN: UnCompressed channel dma run bit.
 * UCC_RUN = 1, UCC_DIR = 1 => initiate dma from main memory to field buffers.
 * UCC_RUN = 1, UCC_DIR = 0 => initiate dma from field buffers to main memory.
 *              bit is set by PIO to initiate dma and reset by
 *              dma_engine at completion of transfer.
 *
 * CC_DIR: Compressed channel dma direction bit (see above).
 * UCC_DIR: UnCompressed channel dma direction bit (see above).
 * FIFO_RST_N: reset signal sent to external image data fifo.
 * CL560_RST_N: reset signal sent to CL560.
 * IIC_RST_N: reset signal sent to field buffers.
 */


/*
 * DMA_INTERRUPT_REG interrupt register bit definition 
 */
#define CC_INT			0x00000001
#define UCC_INT			0x00000002

#ifndef DMAC17
#define FIFO_UR			0x00000004
#endif /* DMAC17 */

#define FIFO_XI			0x00000008

#define CL560_IRQ1		0x00000010
#define CL560_IRQ2		0x00000020
#define CL560_FRMEND		0x00000040
/* bit 31 - 7 reserved */
#ifndef DMAC17
#define DMA_ALL_INTS		(CC_INT|\
				 UCC_INT|\
				 FIFO_UR|\
				 FIFO_XI|\
				 CL560_IRQ1|\
				 CL560_IRQ2|\
				 CL560_FRMEND)
#else /* DMAC17 */
#define DMA_ALL_INTS		(CC_INT|\
				 UCC_INT|\
				 FIFO_XI|\
				 CL560_IRQ1|\
				 CL560_IRQ2|\
				 CL560_FRMEND)
#endif /* DMAC17 */



/*
 * DMA_INT_MASK_REG bit definition
 */
#define CC_INT_MSK		0x00000001
#define UCC_INT_MSK		0x00000002

#ifndef DMAC17
#define FIFO_UR_MSK		0x00000004
#endif /* DMAC17 */

#define FIFO_XI_MSK		0x00000008

#define CL560_IRQ1_MSK		0x00000010
#define CL560_IRQ2_MSK		0x00000020
#define CL560_FRMEND_MSK	0x00000040
/* bit 31 - 7 reserved */


/*
 * NEXT_CC_TCR_ADDR_REG: COMPRESSED CHANNEL NEXT DESCRIPTOR ADDRESS:
 * NEXT_UCC_TCR_ADDR_REG: UNCOMPRESSED CHANNEL NEXT DESCRIPTOR ADDRESS:
 *       bits(31:0)  Next Descriptor Address
 * 0x0016xxxx    all r/w?? 
 * Should be loaded with the location of the first descriptor in
 * compressed channel descriptor chain.
 */

/*
 * IMAGE_FIFO_W_READY:  IMAGE FIFO WRITE READY FLAG  REGISTER
 *         (PAE_to_cl560)
 *        bits(7:0) offset value
 * Register contains offset value for image fifo flag register.
 * Flag indicates there is sufficient space available in fifo to handle
 * a descriptors worth of dma transfers from gio to the 560.
 *
 * IMAGE_FIFO_R_READY: IMAGE FIFO READ READY FLAG  REGISTER
 *         (PAE_to_cl560)
 *        bits(7:0) offset value
 * Flag indicates there is data available in fifo to provide
 * a descriptors worth of dma transfers from gio to the 560.
 *
 * FIFO_DEPTH - offset value must be greater than largest word count in descriptor
 * chain.
 */


/*
 * IMAGE FIFO HW FLAG1
 *        (PAF_to_cl560)
 *                 bits(7:0) offset value
 * flag used by hardware. fixed valua,e should be set to 0x4.
 * IMAGE FIFO HW FLAG2
 *         (PAF_from_cl560)
 *                bits(7:0) offset value
 *
 * fifo flag used  hardware. fixed value, should be set to 0x4.
 */





/*
 * PCD8584 I2c C O N T R O L   R E G I S T E R S 
 * 0x0018xxxx 12C Bus Controlle
 *
 * Register select input (internal pull-up); this input selects 
 * between the control/status register and other registers.
 * Logic 1 selects Register S1, logic 0 selects one of the other 
 * registerss depending on bits loaded in ESO, ES1 and ES2
 * of Register S1."
 *
 *                            A0    ESO ES1  ES2  IACK
 *     Data shift register    0      1   x   0    H
 *     Own ADDRESS Register   0      0   0   0    x
 *     Interrupt Vector reg   0      0   0   1    x
 *     S2 Clock Reg           0      0   1   0    x
 *     S1 Ctrl/status         1      0   x   x    x
 *     S1 Ctrl/status         1      1   x   x    H 
 *
 *
 * The reason that the iack vector is at 0ffset 0x8 is because it requires a 
 * hardwaired signal (/IACK) to be activated in order to read out the interrupt
 * vector, and to clear the interrupt pin.  This signal is activated in 
 * microprocessor controlled systems by typically a application specific
 * interrupt acknowledge pin.  In our case we create one by generating an 
 * additional address decode.  In fact the only real use for this is to clear the
 * int pin as the interrupt vector is avialable to us as register S3 (also
 * we have bit specific interrupt indicationsi and dont care what is in S3 or
 * on the data bus during IACK ).
 /


/* 
 * S2 Clock Register 
 */
#define	JPEG_8584_CLK_REG_S20	0x01	
#define	JPEG_8584_CLK_REG_S21	0x02	
#define	JPEG_8584_CLK_REG_S22	0x04	
#define	JPEG_8584_CLK_REG_S23	0x08	
#define	JPEG_8584_CLK_REG_S24	0x10	

/* 
 * S1 Control / Status Register 
 */
#define	JPEG_8584_S1_ACK	0x01	
#define	JPEG_8584_S1_STO	0x02	
#define	JPEG_8584_S1_STA	0x04	
#define	JPEG_8584_S1_ENI	0x08	
#define	JPEG_8584_S1_ES2	0x10	
#define	JPEG_8584_S1_ES1	0x20	
#define	JPEG_8584_S1_ESO	0x40	
#define JPEG_8584_S1_X		0x80

#define	JPEG_8584_S1_BB		0x01	
#define	JPEG_8584_S1_LAB	0x02	
#define	JPEG_8584_S1_AAS	0x04	
#define	JPEG_8584_S1_AD0	0x08	
#define	JPEG_8584_S1_BER	0x10	
#define	JPEG_8584_S1_STS	0x20	
#define	JPEG_8584_S1_0		0x40	
#define	JPEG_8584_S1_PIN	0x80	
#define JPEG_8584_SERIAL_BUSY	JPEG_8584_S1_PIN
#define JPEG_8584_SEND_STOP	(JPEG_8584_S1_ESO| \
				 JPEG_8584_S1_STO| \
				 JPEG_8584_S1_X)
#define JPEG_8485_MASTER_RECV	(JPEG_8584_S1_ESO| \
				 JPEG_8584_S1_ACK)
#define JPEG_8584_ESO		(JPEG_8584_S1_ESO| \
				 JPEG_8584_S1_STA)

#define JPEG_8584_OWNADDR_REG      0x00
#define JPEG_8584_INT_VECT_REG     0x10
#define JPEG_8584_CLK_REG          0x20

#define JPEG_8584_SEND_START_PLUS_ADDR	(JPEG_8584_S1_ESO| \
					 JPEG_8584_S1_STA)
#define JPEG_8584_CLK_SPEED	(JPEG_8584_CLK_REG_S24 | JPEG_8584_CLK_REG_S23)	
				/* 90 kHz SCL, 8 MHz prescaler */
#define JPEG_8584_SLAVE_ADDR	0x55	/* 8584 slave addr, not used in JPEG */

#define JPEG_8584_INT_VECT_80XX	0	/* 80XX bus interface */


typedef struct JPEG_8584_CHIP {
        volatile long    jpeg8584_data;		/* S0, S0', S2, S3 */
        volatile long    jpeg8584_ctrl_stat;	/* S1 Ctrl and status */
} JPEG_8584_REG;

/*
 *  
 */
typedef struct jpeg_regs {
        volatile long 		*jpeg_id_reg;
        volatile long 		*jpeg_brd_reset;  /* needs a C11 GIO FPGA */
	volatile long		*jpeg_cl560;
	volatile long		*jpeg_swap_cl560;
	volatile long		*jpeg_timestamp;
	volatile long		*jpeg_cc_tcr;
	volatile long		*jpeg_ucc_tcr;
	volatile long		*jpeg_dma_ctrl;
	volatile long		*jpeg_dma_int;
	volatile long		*jpeg_dma_int_mask;
#if defined(DMAC17) | defined(SINGLEWIDE)
	volatile long		*jpeg_dma_cc_reg;
	volatile long		*jpeg_dma_ucc_reg;
#endif /* DMAC17 | SINGLEWIDE */
	volatile VFC_REGS	*jpeg_vfc;
	volatile IDT_FIFO_REGS	*jpeg_idt_fifo;
	volatile long		*jpeg_8584_SAA7;
	volatile long		*jpeg_uic_pio;
	volatile long		*jpeg_reserve1;
	volatile long		*jpeg_reserve2;
	volatile long		*jpeg_reserve3;
	volatile long		*jpeg_reserve4;
} JPEG_Regs;

void
reg_read(short Reg, unsigned long *Data, short Offset);

void
reg_write(short Reg, unsigned long Data, short Offset);
void
CRreg_read(short Reg, unsigned long *Data, short Offset);

void
CRreg_write(short Reg, unsigned long Data, short Offset);

#define DMA_BLOCK(x)		(x->j_path->src_node->dma_block)
#define DMA_CC_TCR(x)		(x->jpeg_board->jpeg_regs->jpeg_cc_tcr)
#define DMA_UCC_TCR(x)		(x->jpeg_board->jpeg_regs->jpeg_ucc_tcr)
#define DMA_CTRL(x)		(x->jpeg_board->jpeg_regs->jpeg_dma_ctrl)
#define DMA_INT_MASK(x)		(x->jpeg_board->jpeg_regs->jpeg_dma_int_mask)
#define DMA_SRS_DIRECTION(x)	(x->j_path->src_node->dma_block->direction)
#define DMA_DEST_DIRECTION(x)	(x->j_path->dest_node->dma_block->direction)
#define DMA_SRC_CHANNEL(x)	(x->j_path->src_node->dma_block->dma_channel)
#define DMA_DEST_CHANNEL(x)	(x->j_path->dest_node->dma_block->dma_channel)



#endif /* __JPEG1_HW_H_ */


