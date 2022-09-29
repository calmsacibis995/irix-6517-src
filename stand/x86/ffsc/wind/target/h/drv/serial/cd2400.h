/* cd2400.h - Cirrus Logic CD2400 serial chip header */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01g,28sep92,ccc  removed LOCAL function declarations.
01f,22sep92,rrr  added support for c++
01e,01sep92,ccc  added function declarations.
01d,17jun92,ccc  added typedef structure.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01a,11jun91,ccc	 written for Motorola MVME167..
*/

/*
This file contains constants for the Cirrus Logic CD2400 serial chip.

The constants MPCC_BASE_ADRS must defined when
including this header file.
*/


#ifdef	DOC
#define	INCcd2400h
#endif	/* DOC */

#ifndef __INCcd2400h
#define __INCcd2400h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE

#include "tylib.h"

#define N_CHANNELS    4	/* number of serial channels on chip */

#define	CAST (char *)

/* device descriptors */

typedef struct			/* TY_CO_DEV */
    {
    TY_DEV	tyDev;
    BOOL	created;	/* true if device has already been created */
    int		chan_num;	/* save channel number here also */
    int		int_vec;	/* channel interrupt vector base */
    int		numChannels;	/* number of channels on device */
    } TY_CO_DEV;

/* on-board access, register definitions */

#ifndef	MPCC_REG_INTERVAL
#define MPCC_REG_INTERVAL	1
#endif	/* MPCC_REG_INTERVAL */

#ifndef	MPCC_ADRS	/* to permit alternative board addressing */
#define MPCC_ADRS(reg)	(CAST (MPCC_BASE_ADRS + (reg * MPCC_REG_INTERVAL)))
#endif	/* MPCC_ADRS */

/* MPCC -- cd2400 serial channel chip -- register definitions
 * Defined in this order which is the order of defined in
 * the CIRRUS LOGIC document
 */

/* GLOBAL REGISTERS */
#define	MPCC_GFRCR	MPCC_ADRS(0x81)	/* Global Firmware Rev Code Register */
#define	MPCC_TFTC	MPCC_ADRS(0x80)	/* Transmit FIFO Transfer Count */
#define	MPCC_MEOIR	MPCC_ADRS(0x86)	/* Modem End of Interrupt Register */
#define	MPCC_TEOIR	MPCC_ADRS(0x85)	/* Transmit End of Interrupt Register */
#define	MPCC_REOIR	MPCC_ADRS(0x84)	/* Receive End of Interrupt Register */
#define	MPCC_MISR	MPCC_ADRS(0x8b)	/* Modem (/Timer) Interrupt Status Re */
#define	MPCC_TISR	MPCC_ADRS(0x8a)	/* Transmit Interrupt Status Register */
#define	MPCC_RISR	MPCC_ADRS(0x88)	/* Receive Interrupt Stat Reg (WORD) */
#define	MPCC_RISRL	MPCC_ADRS(0x89)	/* Receive Interrupt Stat Reg Low */
#define	MPCC_RISRH	MPCC_ADRS(0x88)	/* Receive Interrupt Stat Reg High */
#define	MPCC_MIR	MPCC_ADRS(0xef)	/* Modem Interrupt Register */
#define	MPCC_TIR	MPCC_ADRS(0xec)	/* Transmit Interrupt Register */
#define	MPCC_RIR	MPCC_ADRS(0xed)	/* Receive Interrupt Register */
#define	MPCC_STK	MPCC_ADRS(0xe2)	/* Stack Register */
#define	MPCC_TPR	MPCC_ADRS(0xda)	/* Timer Period Register */
#define	MPCC_PILR1	MPCC_ADRS(0xe3)	/* Priority Interrupt Level Reg 1 */
#define	MPCC_PILR2	MPCC_ADRS(0xe0)	/* Priority Interrupt Level Reg 2 */
#define	MPCC_PILR3	MPCC_ADRS(0xe1)	/* Priority Interrupt Level Reg 3 */
#define	MPCC_CAR	MPCC_ADRS(0xee)	/* Channel Access Register */
#define	MPCC_RDR	MPCC_ADRS(0xf8)	/* Receive Data Register */
#define	MPCC_TDR	MPCC_ADRS(0xf8)	/* Transmit Data Register */
#define	MPCC_DMR	MPCC_ADRS(0xf6)	/* DMA Mode Register */

/* Per-Channel Registers */

#define	MPCC_LICR	MPCC_ADRS(0x26)	/* Local Interrupting Channel Reg */
#define	MPCC_LIVR	MPCC_ADRS(0x09)	/* Local Interrupt Vector Register */
#define	MPCC_CCR	MPCC_ADRS(0x13)	/* Channel Command Register */
#define MPCC_STCR	MPCC_ADRS(0x12)	/* Special Transmit Command Register */
#define	MPCC_IER	MPCC_ADRS(0x11)	/* Interrupt Enable Register */
#define	MPCC_COR1	MPCC_ADRS(0x10)	/* Channel Option Register 1 */
#define	MPCC_COR2	MPCC_ADRS(0x17)	/* Channel Option Register 2 */
#define	MPCC_COR3	MPCC_ADRS(0x16)	/* Channel Option Register 3 */
#define	MPCC_COR4	MPCC_ADRS(0x15)	/* Channel Option Register 4 */
#define	MPCC_COR5	MPCC_ADRS(0x14)	/* Channel Option Register 5 */
#define	MPCC_COR6	MPCC_ADRS(0x18)	/* Channel Option Register 6 */
#define	MPCC_COR7	MPCC_ADRS(0x07)	/* Channel Option Register 7 */
#define	MPCC_CMR	MPCC_ADRS(0x1b)	/* Channel Mode Register */
#define	MPCC_CSR	MPCC_ADRS(0x1a)	/* Channel Status Reg (Sync Modes) */
#define	MPCC_DMABSTS	MPCC_ADRS(0x19)	/* DMA Buffer Status */
#define	MPCC_LNXT	MPCC_ADRS(0x2e)	/* LNext Character */
#define	MPCC_SCHR1	MPCC_ADRS(0x1f)	/* Special Character Register 1 */
#define	MPCC_RFAR1	MPCC_ADRS(0x1f)	/* Receive Frame Address Register 1 */
#define	MPCC_SCHR2	MPCC_ADRS(0x1e)	/* Special Character Register 2 */
#define	MPCC_RFAR2	MPCC_ADRS(0x1e)	/* Receive Frame Address Register 2 */
#define	MPCC_SCHR3	MPCC_ADRS(0x1d)	/* Special Character Register 3 */
#define MPCC_RFAR3	MPCC_ADRS(0x1d)	/* Receive Frame Address Register 3 */
#define	MPCC_SCHR4	MPCC_ADRS(0x1c)	/* Special Character Register 4 */
#define	MPCC_RFAR4	MPCC_ADRS(0x1c)	/* Receive Frame Address Register 4 */
#define	MPCC_SCRL	MPCC_ADRS(0x23) /* Special Character Range Low */
#define	MPCC_SCRH	MPCC_ADRS(0x22)	/* Special Character Range High */
#define	MPCC_RTPR	MPCC_ADRS(0x24)	/* Receive Timeout Period Register */
#define	MPCC_RTPRL	MPCC_ADRS(0x25)	/* Receive Timeout Period Reg Low */
#define	MPCC_RTPRH	MPCC_ADRS(0x24)	/* Receive Timeout Period Reg High */
#define	MPCC_GT1	MPCC_ADRS(0x2a)	/* General Timer 1 */
#define	MPCC_GT1L	MPCC_ADRS(0x2b)	/* General Timer 1 Low */
#define	MPCC_GT1H	MPCC_ADRS(0x2a)	/* General Timer 1 High */
#define	MPCC_GT2	MPCC_ADRS(0x29)	/* General Timer 2 */
#define	MPCC_TTR	MPCC_ADRS(0x29)	/* Transmit Timer Register */
#define	MPCC_RFOC	MPCC_ADRS(0x30)	/* Receive FIFO Output Count */
#define	MPCC_TCBADRL	MPCC_ADRS(0x3a)	/* Transmit Current Buffer Addr Lower */
#define	MPCC_TCBADRH	MPCC_ADRS(0x38)	/* Transmit Current Buffer Addr Upper */
#define	MPCC_RCBADRL	MPCC_ADRS(0x3e)	/* Receive Current Buffer Addr Lower */
#define	MPCC_RCBADRH	MPCC_ADRS(0x3c)	/* Receive Current Buffer Addr Upper */
#define	MPCC_ARBADRL	MPCC_ADRS(0x42)	/* A Receive Buffer Address Lower */
#define MPCC_ARBADRU	MPCC_ADRS(0x40)	/* A Receive Buffer Address Upper */
#define	MPCC_BRBADRL	MPCC_ADRS(0x46)	/* B Receive Buffer Address Lower */
#define	MPCC_BRBADRU	MPCC_ADRS(0x44)	/* B Receive Buffer Address Upper */
#define	MPCC_ARBCNT	MPCC_ADRS(0x4a)	/* A Receive Buffer Count */
#define	MPCC_BRBCNT	MPCC_ADRS(0x48)	/* B Receive Buffer Count */
#define	MPCC_ARBSTS	MPCC_ADRS(0x4f)	/* A Receive Buffer Status */
#define	MPCC_BRBSTS	MPCC_ADRS(0x4e)	/* B Receive Buffer Status */
#define	MPCC_ATBADRL	MPCC_ADRS(0x52)	/* A Transmit Buffer Address Lower */
#define	MPCC_ATBADRU	MPCC_ADRS(0x50)	/* B Transmit Buffer Address Upper */
#define	MPCC_BTBADRL	MPCC_ADRS(0x56)	/* B Transmit Buffer Address Lower */
#define	MPCC_BTBADRU	MPCC_ADRS(0x54)	/* B Transmit Buffer Address Upper */
#define	MPCC_ATBCNT	MPCC_ADRS(0x5a)	/* A Buffer Transmit Byte Count */
#define	MPCC_BTBCNT	MPCC_ADRS(0x58)	/* B Buffer Transmit Byte Count */
#define	MPCC_ATBSTS	MPCC_ADRS(0x5f)	/* A Transmit Buffer Status */
#define	MPCC_BTBSTS	MPCC_ADRS(0x5e)	/* B Transmit Buffer Status */
#define	MPCC_MSVRRTS	MPCC_ADRS(0xde)	/* Modem Signal Value Register - RTS */
#define MPCC_MSVRDTR	MPCC_ADRS(0xdf)	/* Modem Signal Value Register - DTR */
#define	MPCC_TBPR	MPCC_ADRS(0xc3)	/* Transmit Baud Rate Period Register */
#define	MPCC_TCOR	MPCC_ADRS(0xc0)	/* Transmit Clock Option Register */
#define	MPCC_RBPR	MPCC_ADRS(0xcb)	/* Receive Baud Rate Period Register */
#define	MPCC_RCOR	MPCC_ADRS(0xc8)	/* Receive Clock Option Register */
#define	MPCC_CPSR	MPCC_ADRS(0xd6)	/* CRC Polynomial Select Register */

/* Now let's define some register bit values */

/*******************************************************************
* MEOIR		0x86		Modem End of Interrupt Register    *
* TEOIR		0x85		Transmit End of Interrupt Register *
* REOIR		0x84		Receive End of Interrupt Register  *
********************************************************************/
#define	EOIR_SET_TIMER_2	0x20	/* load MISR to timer 2 */
#define	EOIR_SET_TIMER_1	0x10	/* load MISR to timer 1 */
#define	EOIR_TERMINATE_BUF	0x80	/* 1=Terminate buffer in DMA Mode */
#define	EOIR_END_OF_FRAME	0x40	/* 0 = no, 1 = yes */
#define	EOIR_NO_TRANSFER	0x08	/* no data transferred */
#define EOIR_DIS_EXCEP_CHAR	0x40	/* 0 = no, 1 = yes */
#define	EOIR_TRANSFER		0x00	/* data was transferred */

/*************************************************************************
* MISR		0x8b		Modem (/Timer) Interrupt Status Register *
**************************************************************************/
#define	MISR_DSR_CHANGED	0x80	/* 0 = no, 1 = yes */
#define	MISR_CD_CHANGED		0x40	/* 0 = no, 1 = yes */
#define	MISR_CTS_CHANGED	0x20	/* 0 = no, 1 = yes */
#define	MISR_TIMER2_TIMEOUT	0x02	/* 0 = no, 1 = yes */
#define MISR_TIMER1_TIMEOUT	0x01	/* 0 = no, 1 = yes */

/****************************************************************************
* TISR		0x8a		Transmit Interrupt Status Register          *
*****************************************************************************/
#define	TISR_TRANS_BERR		0x80	/* 0 = no, 1 = yes (Bus error) */
#define	TISR_TRANS_EOF		0x40	/* 0 = no, 1 = yes (End Of Frame) */
#define	TISR_TRANS_EOB		0x20	/* 0 = no, 1 = yes (End Of trans Buf) */
#define	TISR_TRANS_UNDERRUN	0x10	/* 0 = no, 1 = yes */
#define	TISR_TRANS_A_OR_B	0x08	/* 0 = Buffer A, 1 = Buffer B */
#define	TISR_TX_EMPTY		0x02	/* 0 = no, 1 = yes (idle condition) */
#define	TISR_TX_DATA		0x01	/* 0 = no, 1 = yes (below threshold) */

/************************************************************************
* RISR		0x88		Receive Interrupt Status Register       *
* RISRL		0x89		Receive Interrupt Status Register Low   *
*************************************************************************/
/* HDLC Mode */
#define	RISR_REC_EOF		0x40	/* 0 = no, 1 = yes */
#define	RISR_REC_ABORT		0x20	/* 0 = no, 1 = yes (term frame) */
#define	RISR_REC_CRC		0x10	/* 0 = no, 1 = yes (CRC error frame) */
#define	RISR_REC_OVERRUN	0x08	/* 0 = no, 1 = yes (Overrun occured) */
#define	RISR_REC_RESIDUAL	0x04	/* 0 = no, 1 = yes (char was partial */
#define	RISR_REC_CLEAR_DCT	0x01	/* 0 = no, 1 = yes (X.21 Clr signal) */
/* Asynchronous Mode */
#define	RISR_REC_TIMEOUT	0x80	/* 0 = no, 1 = yes (No char rec'd) */
#define	RISR_REC_PARITY_ERR	0x04	/* 0 = no, 1 = yes (Parity error) */
#define	RISR_REC_FRAME_ERR	0x02	/* 0 = no, 1 = yes (Frame error) */
#define RISR_REC_BREAK		0x01	/* 0 = no, 1 = yes (Break detected) */

/***********************************************************************
* RISRH		0x88		Receive Interrupt Status Register High *
************************************************************************/
#define	RISR_REC_BERR		0x80	/* 0 = no, 1 = yes (Bus error) */
#define	RISR_REC_EOB		0x20	/* 0 = no, 1 = yes (End Of Buffer) */
#define	RISR_REC_A_OR_B		0x08	/* 0 = Buffer A, 1 = Buffer B */

/****************************************************************
* MIR		0xef		Modem Interrupt Register 	*
*****************************************************************/
#define	MIR_MODEM_ENABLE	0x80	/* 0 = no, 1 = yes (Modem inter) */
#define	MIR_MODEM_ACTIVE	0x40	/* 0 = no, 1 = yes (Modem Active) */
#define	MIR_MODEM_EOI		0x20	/* 0 = no, 1 = yes (Modem End Of Int) */

/****************************************************************
* TIR		0xec		Transmit Interrupt Register 	*
*****************************************************************/
#define	TIR_TRANS_ENABLE	0x80	/* 0 = no, 1 = yes (Trans inter) */
#define	TIR_TRANS_ACTIVE	0x40	/* 0 = no, 1 = yes (Trans Active) */
#define	TIR_TRANS_EOI		0x20	/* 0 = no, 1 = yes (Trans End of Int) */

/****************************************************************
* RIR		0xed		Receive Interrupt Register 	*
*****************************************************************/
#define	RIR_REC_ENABLE		0x80	/* 0 = no, 1 = yes (Rec inter) */
#define	RIR_REC_ACTIVE		0x40	/* 0 = no, 1 = yes (Rec Active) */
#define	RIR_REC_EOI		0x20	/* 0 = no, 1 = yes (Rec End of Int) */

/****************************************************************
* CAR		0xee		Channel Access Register		*
*****************************************************************/
#define	CAR_CHANNEL0		0x00	/* Channel 0			1-0 */
#define CAR_CHANNEL1		0x01	/* Channel 1			1-0 */
#define	CAR_CHANNEL2		0x02	/* Channel 2			1-0 */
#define	CAR_CHANNEL3		0x03	/* Channel 3			1-0 */

/****************************************************************
* DMR		0xf6		DMA Mode Register		*
*****************************************************************/
#define	DMR_BYTE_DMA		0x08	/* Do BYTE DMA always		3 */
#define	DMR_WORD_DMA 		0x00	/* Do WORD DMA whenever poss	4 */

/****************************************************************
* CCR		0x13		Channel Command Register	*
*****************************************************************/
#define	CCR_CLEAR_CHANNEL	0x40	/* clear channel		7,6 */
#define	CCR_INIT_CHANNEL	0x20	/* initialize channel		7,5 */
#define	CCR_RESET_ALL		0x10	/* on-chip init ALL CHANNELS	7,4 */
#define	CCR_ENABLE_TRANS	0x08	/* enable transmitter		7,3 */
#define	CCR_DISABLE_TRANS	0x04	/* disable transmitter		7,2 */
#define	CCR_ENABLE_REC		0x02	/* enable receiver		7,1 */
#define CCR_DISABLE_REC		0x01	/* disable receiver		7,0 */
#define	CCR_CLEAR_TIMER1	0xc0	/* clear general timer 1	7,6 */
#define	CCR_CLEAR_TIMER2	0xa0	/* clear general timer 2	7,5 */

/******************************************************************
* STCR		0x12		Special Transmit Command Register *
*******************************************************************/
#define	STCR_ABORT_TX		0x40	/* abort transmiss (HDLC)	6 */
#define	STCR_APPD_CMP		0x20	/* append complete (async DMA)	5 */
#define	STCR_SND_SPC		0x08	/* send special character	3 */
#define	STCR_SPECIAL_CHAR1	0x01	/* send special char 1		2-0 */
#define	STCR_SPECIAL_CHAR2	0x02	/* send special char 2		2-0 */
#define	STCR_SPECIAL_CHAR3	0x03	/* send special char 3		2-0 */
#define	STCR_SPECIAL_CHAR4	0x04	/* send special char 4		2-0 */

/****************************************************************
* IER		0x11		Interrupt Enable Register	*
*****************************************************************/
#define	IER_MODEM		0x80	/* modem pin change detect	7 */
#define	IER_RET			0x20	/* Receive Exception Timeout	5 */
#define	IER_RXDATA		0x08	/* Receive Data Interrupt	3 */
#define	IER_TIMER		0x04	/* general timer timeout	2 */
#define	IER_TXEMPTY		0x02	/* transmitter empty		1 */
#define	IER_TXDATA		0x01	/* Transmit Data Interrupt	0 */

/****************************************************************
* COR1		0x10		Channel Option Register 1	*
*****************************************************************/
#define	COR1_ODD_PARITY		0x80	/* odd parity			7 */
#define	COR1_EVEN_PARITY	0x00	/* even parity			7 */
#define	COR1_NO_PARITY		0x00	/* no parity mode		6-5 */
#define	COR1_FORCE_PARITY	0x20	/* force parity (odd=1/even=0)	6-5 */
#define	COR1_PARITY		0x40	/* normal parity		6-5 */
#define	COR1_IGNORE_PARITY	0x10	/* do not evaluate parity	4 */
#define	COR1_5BITS		0x04	/* 5 data bits			3-0 */
#define	COR1_6BITS		0x05	/* 6 data bits			3-0 */
#define	COR1_7BITS		0x06	/* 7 data bits			3-0 */
#define	COR1_8BITS		0x07	/* 8 data bits			3-0 */

/****************************************************************
* COR2		0x17		Channel Option Register 2	*
*****************************************************************/
#define	COR2_IXM		0x80	/* xon with any char		7 */
#define COR2_TXIBE		0x40	/* tx in-band xon/xoff control	6 */
#define	COR2_ETC		0x20	/* embedded tx command enable	5 */
#define	COR2_REMOTE_LOOP	0x08	/* enable remote loopback	3 */
#define	COR2_AUTO_RTS		0x04	/* RTS automatic output enable	2 */
#define	COR2_AUTO_CTS		0x02	/* CTS automatic enable		1 */
#define	COR2_AUTO_DSR		0x01	/* DSR automatic enable		0 */

/****************************************************************
* COR3		0x16		Channel Option Register 3	*
*****************************************************************/
#define	COR3_ESCDE		0x80	/* extnd spec char det enable	7 */
#define	COR3_RANGE_DETECT	0x40	/* generate special char inter	6 */
#define	COR3_FCT		0x20	/* flow control not passed	5 */
#define	COR3_SCDE		0x10	/* special character detecton	4 */
#define	COR3_1STOP_BIT		0x02	/* 1 stop bit			2-0 */
#define	COR3_1_5STOP_BIT	0x03	/* 1.5 stop bits		2-0 */
#define	COR3_2STOP_BIT		0x04	/* 2 stop bits			2-0 */

/****************************************************************
* COR4		0x15		Channel Option Register 4	*
*****************************************************************/
#define	COR4_DSR_ZD		0x80	/* 1->0 detect on the DSR*	7 */
#define	COR4_CD_ZD		0x40	/* 1->0 detect on the CD*	6 */
#define	COR4_CTS_ZD		0x20	/* 1->0 detect on the CTS*	5 */

/****************************************************************
* COR5		0x14		Channel Option Register 5	*
*****************************************************************/
#define	COR5_DSR_OD		0x80	/* 0->1 detect on the DSR*	7 */
#define	COR5_CD_OD		0x40	/* 0->1 detect on the CD*	6 */
#define	COR5_CTS_OD		0x20	/* 0->1 detect on the CTS*	5 */

/****************************************************************
* COR6		0x18		Channel Option Register 6	*
*****************************************************************/
#define	COR6_NORMAL_CR_NL	0x00	/* no special action on CR/NL	7-5 */
#define	COR6_NL_TO_CR		0x20	/* NL translated to CR		7-5 */
#define	COR6_CR_TO_NL		0x40	/* CR translated to NL		7-5 */
#define	COR6_SWAP_CR_NL		0x60	/* CR->NL, NL->CR		7-5 */
#define	COR6_NO_CR		0x80	/* CR discarded			7-5 */
#define	COR6_NO_CR_NL_TO_CR	0xa0	/* CR discarded, NL->CR		7-5 */
#define	COR6_BRK_EXCEPTION	0x00	/* on break: generate exception	3-4 */
#define	COR6_BRK_TO_NULL	0x08	/*	     trans to NULL	3-4 */
#define	COR6_BRK_DISCARD	0x18	/*	     discard character	3-4 */
/*					  On Parity/Framing Error:	    */
#define	COR6_PARITY_EXCEPTION	0x00	/* generate exception interrupt	2-0 */
#define	COR6_PARITY_TO_NULL	0x01	/* translated to a NULL char	2-0 */
#define	COR6_PARITY_IGNORE	0x02	/* ignore error,pass as good	2-0 */
#define	COR6_PARTIY_DISCARD	0x03	/* discard error character	2-0 */
#define	COR6_PARITY_FF_NULL	0x05	/* trans to FF NULL pass as gd	2-0 */

/****************************************************************
* COR7		0x07		Channel Option Register 7	*
*****************************************************************/
#define	COR7_ISTRIP		0x80	/* strip to 7 bits on input	7 */
#define	COR7_LNEXT		0x40	/* char after LNXT no spec process 6 */
#define	COR7_FCERROR		0x20	/* frlow control on error char	5 */
#define	COR7_NORMAL_CR_NL	0x00	/* on tx CR/NL: no special act	1-0 */
#define	COR7_CR_TO_NL		0x01	/*		CR -> NL	1-0 */
#define	COR7_NL_TO_CR_NL	0x02	/*		NL -> CR/NL	1-0 */
#define	COR7_CR_TO_NL_NL_TO_CR_NL 0x03	/*		CR->NL,NL->CR/NL 1-0 */

/****************************************************************
* CMR		0x1b		Channel Mode Register		*
*****************************************************************/
#define	CMR_RX_INTERRUPT	0x00	/* rx tran mode = interrupt	7 */
#define	CMR_RX_DMA		0x80	/* rx tran mode = DMA		7 */
#define	CMR_TX_INTERRUPT	0x00	/* tx tran mode = interrupt	6 */
#define	CMR_TX_DMA		0x40	/* tx tran mode = DMA		6 */
#define	CMR_HDLC_MODE		0x00	/* protocal:	HDLC		2-0 */
#define	CMR_BISYNC_MODE		0x01	/*		BISYNC		2-0 */
#define	CMR_ASYNC_MODE		0x02	/*		ASYNC		2-0 */
#define	CMR_X_21_MODE		0x03	/*		X.21		2-0 */

/****************************************************************
* CSR		0x1a		Channel Status Register		*
*****************************************************************/
#define	CSR_RX_ENABLE		0x80	/* receiver is enabled		7 */
#define	CSR_RX_FLOFF		0x40	/* receive flow off sent	6 */
#define	CSR_RX_FLON		0x20	/* receive flow on sent		5 */
#define	CSR_TX_ENABLE		0x08	/* transmitter is enabled	3 */
#define	CSR_TX_FLOFF		0x04	/* trans remote stop request	2 */
#define	CSR_TX_FLON		0x02	/* trans remote start request	1 */

/****************************************************************
* DMABSTS	0x19		DMA Buffer Status		*
*****************************************************************/
#define	DMABSTS_RST_APD		0x40	/* reset append mode		6 */
#define	DMABSTS_APPEND		0x10	/* append buffer is in use	4 */
#define	DMABSTS_B_NTBUF		0x08	/* B is next tx buffer		3 */
#define	DMABSTS_TBSY		0x04	/* current tx buffer in use	2 */
#define	DMABSTS_B_NRBUF		0x02	/* B is next rx buffer		1 */
#define	DMABSTS_RBUSY		0x01	/* current rec buffer in use	0 */

/****************************************************************
* ARBSTS	0x4f		A Receive Buffer Status		*
* BRBSTS	0x4e		B Receive Buffer Status		*
*****************************************************************/
#define	RBSTS_BUS_ERR		0x80	/* bus error occurred		7 */
#define	RBSTS_EOF		0x40	/* this buffer term frame	6 */
#define	RBSTS_EOB		0x20	/* buffer complete		5 */
#define	RBSTS_2400_OWN		0x01	/* buffer can be used by CD2400	0 */

/****************************************************************
* ATBSTS	0x5f		A Transmit Buffer Status	*
* BTBSTS	0x5E		B Transmit Buffer Status	*
*****************************************************************/
#define	TBSTS_BUS_ERR		0x80	/* bus error occurred		7 */
#define	TBSTS_EOF		0x40	/* last in frame/block		6 */
#define	TBSTS_EOB		0x20	/* end of tx buffer reached	5 */
#define	TBSTS_APPEND		0x08	/* data may be appded during tx	3 */
#define	TBSTS_INTR		0x02	/* interrupt required		1 */
#define	TBSTS_2400_OWN		0x01	/* buffer ready to transmit	0 */

/****************************************************************
* MSVR_RTS	0xde		Modem Signal Value Register DTR	*
* MSVR_DTR	0xdf		Modem Signal Value Register DTR	*
*****************************************************************/
#define	MSVR_DSR		0x80	/* state of the DSR input	7 */
#define	MSVR_CD			0x40	/* state of the CD input	6 */
#define	MSVR_CTS		0x20	/* state of the CTS input	5 */
#define	MSVR_DTR		0x02	/* state of the DTR output	1 */
#define	MSVR_RTS		0x01	/* state of the RTS output	0 */

/****************************************************************
* TCOR		0xc0		Transmit Clock Option Register	*
*****************************************************************/
#define	TCOR_CLK0		0x00	/* select clock 0		7-5 */
#define	TCOR_CLK1		0x20	/* select clock 1		7-5 */
#define	TCOR_CLK2		0x40	/* select clock 2		7-5 */
#define	TCOR_CLK3		0x60	/* select clock 3		7-5 */
#define	TCOR_CLK4		0x80	/* select clock 4		7-5 */
#define	TCOR_EXTERNAL_CLK	0xc0	/* External clock		7-5 */
#define	TCOR_RECEIVE_CLK	0xe0	/* Receive clock		7-5 */
#define	TCOR_EXT_1X		0x08	/* times 1 extern clock		3 */
#define	TCOR_LLM		0x02	/* enable local loopback	1 */

/****************************************************************
* TBPR		0xc3		Transmit Baud Rate Period Reg	*
* RBPR		0xcb		Receive Baud Rate Period Reg	*
*****************************************************************/
#define	BPR_64000		0x26	/* with clk0 */
#define	BPR_56000		0x2c	/* with clk0 */
#define	BPR_38400		0x40	/* with clk0 */
#define	BPR_19200		0x81	/* with clk0 */
#define	BPR_9600		0x40	/* with clk1 */
#define	BPR_7200		0x56	/* with clk1 */
#define	BPR_4800		0x81	/* with clk1 */
#define	BPR_3600		0xad	/* with clk1 */
#define	BPR_2400		0x40	/* with clk2 */
#define	BPR_1200		0x81	/* with clk2 */
#define	BPR_600			0x40	/* with clk3 */
#define	BPR_300			0x81	/* with clk3 */
#define	BPR_150			0x40	/* with clk4 */
#define	BPR_110			0x58	/* with clk4 */
#define	BPR_50			0xc2	/* with clk4 */

/****************************************************************
* RCOR		0xc8		Receive Clock Option Register	*
*****************************************************************/
#define	RCOR_TLVAL		0x80	/* transmit line value		7 */
#define	RCOR_DPLL_ENABLE	0x20	/* enable DPLL			5 */
#define	RCOR_DPLL_DISABLE	0x00	/* disable DPLL			5 */
#define	RCOR_DPLL_NRZ		0x00	/* data encoding:	NRZ	4-3 */
#define	RCOR_DPLL_NRZI		0x08	/*			NRZI	4-3 */
#define	RCOR_DPLL_MANCHESTER	0x10	/*			Manches	4-3 */
#define	RCOR_CLK0		0x00	/* select clock 0		2-0 */
#define	RCOR_CLK1		0x01	/* select clock 1		2-0 */
#define	RCOR_CLK2		0x02	/* select clock 2		2-0 */
#define	RCOR_CLK3		0x03	/* select clock 3		2-0 */
#define	RCOR_CLK4		0x04	/* select clock 4		2-0 */

/****************************************************************
* CPSR		0xd6		CRC Polynomial Select Register	*
*****************************************************************/
#define	CPSR_CRC_16		0x01	/* CRC-16 polynomial (Bisync)	0 */
#define	CPSR_CRC_V41		0x00	/* CRC V.41 polynomial (HDLC)	0 */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

IMPORT  void    tyCoIntRx (TY_CO_DEV *pTyCoDv);
IMPORT  void    tyCoIntTx (TY_CO_DEV *pTyCoDv);
IMPORT  void    tyCoInt (TY_CO_DEV *pTyCoDv);

#else	/* __STDC__ */

IMPORT  void    tyCoIntRx ();
IMPORT  void    tyCoIntTx ();
IMPORT  void    tyCoInt ();

#endif  /* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCcd2400h */
