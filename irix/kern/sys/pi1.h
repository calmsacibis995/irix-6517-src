/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "sys/PI1.h: $Revision: 1.2 $"

/* PI1 register definitions */
#define PI1_DATA	(HPC3_PAR_DATA+3)	/* Data register */	
#define PI1_CTRL	(HPC3_PAR_CONTROL+3)	/* control register */
#define PI1_STAT	(HPC3_PAR_STAT+3)	/* status register */
#define PI1_DMA_CTRL	(HPC3_PAR_DMA_CONTROL+3)/* dma control register */
#define PI1_INT_STAT	(HPC3_PAR_INT_STAT+3)	/* interrupt status register */
#define PI1_INT_MASK	(HPC3_PAR_INT_MASK+3)	/* interrupt mask register */
#define PI1_TIME1	(HPC3_PAR_TIMER1+3)	/* Tds+Tsw+Tdh */
#define PI1_TIME2	(HPC3_PAR_TIMER2+3)	/* Tds-data strobe assertion */
#define PI1_TIME3	(HPC3_PAR_TIMER3+3)	/* Tsw- strobe pulse width */
#define PI1_TIME4	(HPC3_PAR_TIMER4+3)

/* control register bit definitions-Read/Write */
#define PI1_CTRL_STROBE_N	0x01	/* 0=asserted,reset val=1 */
#define PI1_CTRL_AFD_N		0x02	/* 0=asserted,reset val=1 */
#define PI1_CTRL_INIT_N		0x04	/* 0=printer init,reset val=1 */
#define PI1_CTRL_SLIN_N		0x08	/* 0=asserted, reset val=1 */
#define PI1_CTRL_SEL		0x40	/* 1=AD to PBUS bus, reset val=1 */

/* status register bit definitions-Read only */
#define PI1_STAT_DEVID		0x03	/* Bits 0-1 */
#define PI1_STAT_NOINK		0x04	/* 1=noink, SGI MODE only */
#define PI1_STAT_ERROR_N	0x08	/* 0=err */
#define PI1_STAT_ONLINE		0x10	/* 1=online */
#define PI1_STAT_PE		0x20	/* 1=PE */
#define PI1_STAT_ACK_N		0x40	/* 0=ack */
#define PI1_STAT_BUSY		0x80	/* 0=busy */

/* dma control register bit definitions-WO/RO(Write/Read Only) */
#define PI1_DMA_CTRL_FEMPTY	0x01	/* RO:1=fempty */
#define PI1_DMA_CTRL_ABORT	0x02	/* WO:1=reset DMA, interna; fifo */
#define PI1_DMA_CTRL_STDMODE	0x00	/* bits 2-3:Auto seq DMA supported */
#define PI1_DMA_CTRL_SGIMODE	0x04	/* bits 2-3:DMA Writes supported */
#define PI1_DMA_CTRL_RICOHMODE	0x08	/* bits 2-3:DMA R/W supported */
#define PI1_DMA_CTRL_HPMODE	0x0c	/* bits 2-3:DMA R/W supported */
#define PI1_DMA_CTRL_BLKMODE	0x10	/* 1=blk, 0=reg mode, reset val=0 */
#define PI1_DMA_CTRL_FIFOCLR	0x20	/* WO:1=clears fifo, reset val =0 */
#define PI1_DMA_CTRL_DMAREAD	0x40	/* 1=DMA-READ, reset val=0 */
#define PI1_DMA_CTRL_DMARUN	0x80	/* 1=start dma, 0=stops dma */

/* interrupt status register bit definitions-Read Only */
#define PI1_INT_STAT_ACKINT	0x04	/* 1=rising edge of ack */
#define PI1_INT_STAT_FEMPTYINT	0x08	/* 1=fifo is empty */
#define PI1_INT_STAT_NOINKINT	0x10	/* 1=no ink, reset val=0 */
#define PI1_INT_STAT_ONLINEINT	0x20	/* 1=online, reset val=0 */
#define PI1_INT_STAT_ERRINT	0x40	/* 1=err, reset val=0 */
#define PI1_INT_STAT_PEINT	0x80	/* 1=pe, reset val=0 */

/* interrupt mask register bit definitions-Write/Read */
#define PI1_INT_MASK_ACKINT	0x04	/* 0=interrupt enabled, reset val=1 */ 
#define PI1_INT_MASK_FEMPTYINT	0x08	/* 0=interrupt enabled, reset val=1 */
#define PI1_INT_MASK_NOINKINT	0x10	/* 0=interrupt enabled, reset val=1 */
#define PI1_INT_MASK_ONLINEINT	0x20	/* 0=interrupt enabled, reset val=1 */
#define PI1_INT_MASK_ERRINT	0x40	/* 0=interrupt enabled, reset val=1 */
#define PI1_INT_MASK_PEINT	0x80	/* 0=interrupt enabled, reset val=1 */

/* Default values for tval1,tval2 and tval3 */
#define PI1_TVAL1	0x27
#define PI1_TVAL2	0x13
#define PI1_TVAL3	0x10
#define PI1_TVAL4	0x00

#ifdef _LANGUAGE_C
/* PI1's register structure */
typedef struct pi1 {
        char pad0[3];
        volatile unsigned char data;
        char pad1[3];
        volatile unsigned char ctrl;
        char pad2[3];
        volatile unsigned char stat;
        char pad3[3];
        volatile unsigned char dmactrl;
        char pad4[3];
        volatile unsigned char intstat;
        char pad5[3];
        volatile unsigned char intmask;
        char pad6[3];
        volatile unsigned char tval1;
        char pad7[3];
        volatile unsigned char tval2;
        char pad8[3];
        volatile unsigned char tval3;
        char pad9[3];
        volatile unsigned char tval4;
} pi1_t;

#endif
