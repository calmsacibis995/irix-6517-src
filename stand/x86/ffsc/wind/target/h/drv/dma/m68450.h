/* m68450.h - Tadpole TP32V system dependent library */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,14mar95,vin  included in the h/drv/dma directory. included copyright.
01c,07sep93,vin  added dma channel macros and definitions made more generic;
		 changed the name of the file from scsiDma.h to m68450.h
01b,22jan91,tad  rewritten to conform to Wind River coding standards.
01a,01oct87,tad  written.
*/

/*
DESCRIPTION
This module contains definitions needed when using the 68450 DMA controller
on the Tadpole TP32V. The following macros need to be defined in target.h.
DMAC_ADRS,DMAC_CHANNEL_0_OFFSET,DMAC_CHANNEL_1_OFFSET,DMAC_CHANNEL_2_OFFSET
DMAC_CHANNEL_3_OFFSET,DMA_GCR

*/

#ifndef INCm68450h
#define INCm68450h

#ifndef _ASMLANGUAGE

/* This structure defines the register layout of the device in memory */
struct dmaRegisters
    {
    UINT8 csr;		/* Channel status register */
    UINT8 cer;		/* Channel error register */
    UINT8 pad1[2];
    UINT8 dcr;		/* Device control register */
    UINT8 ocr;		/* Operation control register */
    UINT8 scr;		/* Sequence control register */
    UINT8 ccr;		/* Channel control register */
    UINT8 pad2[2];
    UINT16 mtcr;	/* Memory transfer count register */
    UINT32 mar;		/* Memory address register */
    UINT8 pad3[4];	
    UINT32 dar;		/* Device address register  */
    UINT8 pad4[2];
    UINT16 btcr;	/* Base transfer count register */
    UINT32 bar;		/* Base address register */
    UINT8 pad5[5];
    UINT8 nivr;		/* Normal interrupt vector reg */
    UINT8 pad6[1];
    UINT8 eivr;		/* Error interrupt vector reg */
    UINT8 pad7[1];
    UINT8 mfcr;		/* Memory function code register */
    UINT8 pad8[3];
    UINT8 cpr;		/* Channel priority register */
    UINT8 pad9[3];
    UINT8 dfcr;		/* Device function code register */
    UINT8 pad10[7];
    UINT8 bfcr;		/* Base function code register */
    };
	
/* Bitfield masks for registers in the dmaRegisters structure */

/* DCR */

#define DMA_XRM		0xC0	/* External request mode */
#define DMA_DTYP	0x30	/* Device type */
#define	DMA_DPS		0x08	/* Device port size */
#define DMA_PCL		0x07	/* Peripheral control line function */

/* OCR */

#define	DMA_DIR		0x80	/* Transfer direction */
#define	DMA_SIZE	0x30	/* Operand size */
#define	DMA_CHAIN	0x0C	/* Chain operation */
#define DMA_REQG	0x03	/* Request generation */

/* SCR */

#define	DMA_MAC		0x0C	/* Memory address count */
#define	DMA_DAC		0x03	/* Device address count */

/* CCR */

#define	DMA_STR		0x80	/* Start operation */
#define	DMA_CNT		0x40	/* Continue operation */
#define	DMA_HLT		0x20	/* Halt operation */
#define	DMA_SAB		0x10	/* Software abort */
#define	DMA_INT		0x08	/* Interrupt enable */

/* CSR */

#define	DMA_COC		0x80	/* Channel operation complete */
#define DMA_BTC		0x40	/* Block transfer complete */
#define DMA_NDT		0x20	/* Normal device termination */
#define DMA_ERR		0x10	/* Error */
#define DMA_ACT		0x08	/* Channel active */
#define DMA_PCT		0x02	/* PCL transition */
#define DMA_PCS		0x01	/* PCL state */

/* GCR */

#define	DMA_BT		0x0C	/* Burst transfer time */
#define DMA_BR		0x03	/* Bandwidth available to DMAC */

/* Bitfield definitions for DCR */

/* XRM */

#define	DMA_BURST	0x00	/* Burst transfer mode */
#define	DMA_CYCNH	0x80	/* Cycle steal without hold */
#define	DMA_CYCWH	0xC0	/* Cycle stel with hold */

/* DTYP */

#define DMA_THOU	0x00	/* 68000 compatible, explicitly addressed */
#define	DMA_HUN		0x10	/* 6800 compatible, explicitly addressed */
#define	DMA_ACK		0x20	/* Device with ACK */
#define DMA_ACKRDY	0x30	/* Device with ACK & RDY */

/* DPS */

#define	DMA_PORT8	0x00	/* 8-bit port */
#define DMA_PORT16	0x08	/* 16-bit port */

/* PCL */

#define DMA_STATUSINP	0x00	/* Status input (level read in CSR) */
#define	DMA_STATINPINT	0x01	/* Status input with interrupt */
#define	DMA_SPULSE	0x02	/* Start pulse output */
#define	DMA_ABINP	0x03	/* Abort input */

/* Bitfield definitions for OCR */

/* DIR */

#define	DMA_MEMDEV	0x00	/* Memory to device */
#define DMA_DEVMEM	0x80	/* Device to memory */

/* SIZE */

#define	DMA_SZBYTE	0x00	/* Byte */
#define	DMA_SZWORD	0x10	/* Word */
#define	DMA_SZLWORD	0x20	/* Long word */
#define	DMA_SZBYTENP	0x30	/* Byte, no packing */

/* CHAIN */

#define	DMA_CHAINDIS	0x00	/* Chaining disabled */
#define DMA_CHAINARR	0x08	/* Array chaining enabled */
#define DMA_CHAINARL	0x0C	/* Linked array chaining enabled */

/* REQG */

#define	DMA_INTLIM	0x00	/* Internal request at limited rate */
#define	DMA_INTMAX	0x01	/* Internal request at maximum rate */
#define	DMA_EXTERN	0x02	/* External request */
#define DMA_EXTAUTO	0x03	/* External request, auto-start */

/* Bitfield definitions for SCR */
/* MAC */

#define	DMA_MNOCNT	0x00	/* MAR does not count */
#define	DMA_MUPCNT	0x04	/* MAR counts up */
#define	DMA_MDNCNT	0x80	/* MAR counts down */

/* DAC */

#define DMA_DNOCNT	0x00	/* DAR does not count */
#define DMA_DUPCNT	0x01	/* DAR counts up */
#define DMA_DDNCNT	0x20	/* DAR counts down */

/* Bitfield definitions for CER */
/* ERROR CODE */

#define	DMA_NOERR	0x00	/* No error */
#define	DMA_CONFIGERR	0x01	/* Configuration error */
#define DMA_TIMEERR	0x02	/* Operation timing error */
#define DMA_AMERR	0x05	/* Address error, MAR or MTCR */
#define	DMA_ADERR	0x06	/* Address error, DAR */
#define DMA_ABERR	0x07	/* Address error, BAR or BTCR */
#define DMA_BMERR	0x09	/* Bus error, MAR or MTCR */
#define DMA_BDERR	0x0A	/* Bus error, DAR */
#define DMA_BBERR	0x0B	/* Bus error, BAR or BTCR */
#define DMA_CMERR	0x0D	/* Count error, MAR or MTCR */
#define	DMA_CDERR	0x0E	/* Count error, DAR */
#define DMA_CBERR	0x0F	/* Count error, BAR or BTCR */
#define DMA_EXTAB	0x10	/* External abort */
#define	DMA_SFTAB	0x11	/* Software abort */

/* Bitfield definitions for GCR */
/* BT */

#define	DMA_BT16	0x00	/* 16 clocks */
#define	DMA_BT32	0x04	/* 32 clocks */
#define DMA_BT64	0x08	/* 64 clocks */
#define DMA_BT128	0x0C	/* 128 clocks */

/* BR */

#define DMA_BW50	0x00	/* 50.00% bandwidth */
#define DMA_BW25	0x01	/* 25.00% bandwidth */
#define DMA_BW12	0x02	/* 12.50% bandwidth */
#define DMA_BW6		0x03	/* 6.25% bandwidth */

#define DMA_0		((struct dmaRegisters *) DMAC_ADRS + \
			 DMAC_CHANNEL_0_OFFSET)
#define DMA_1           ((struct dmaRegisters *) DMAC_ADRS + \
			 DMAC_CHANNEL_1_OFFSET)
#define DMA_2           ((struct dmaRegisters *) DMAC_ADRS + \
			 DMAC_CHANNEL_2_OFFSET)
#define DMA_3           ((struct dmaRegisters *) DMAC_ADRS + \
			 DMAC_CHANNEL_3_OFFSET)

#endif  /* _ASMLANGUAGE */

#endif /* INCm68450h  */

