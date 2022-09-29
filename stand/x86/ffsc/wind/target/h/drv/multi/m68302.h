/* m68302.h - Motorola MC68302 CPU control registers */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,10aug92,caf  added TY_CO_DEV and function declarations for 5.1 upgrade.
		 for 5.0.x compatibility, define INCLUDE_TY_CO_DRV_50 when
		 including this header.
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,30sep91,caf  added _ASMLANGUAGE conditional.
01a,19sep91,jcf  written.
*/

/*
This file contains I/O address and related constants for the MC68302.
*/

#ifndef __INCm68302h
#define __INCm68302h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

#ifndef	INCLUDE_TY_CO_DRV_50
#include "tylib.h"
#endif	/* INCLUDE_TY_CO_DRV_50 */

/* System Configuration Registers */

#define IMP_BAR	((UINT16 *)0xf2)	/* Base Address Register */
#define IMP_SCR	((UINT32 *)0xf4)	/* System Control Register */

/* MC68302 parameter register addresses */

#define IMP_SCC1	((SCC *)    (IMP_BASE_ADRS + 0x400))
#define IMP_SCC2	((SCC *)    (IMP_BASE_ADRS + 0x500))
#define IMP_SCC3	((SCC3 *)   (IMP_BASE_ADRS + 0x600))
#define	IMP_CMR		((UINT16 *) (IMP_BASE_ADRS + 0x802))
#define	IMP_SAPR	((UINT32 *) (IMP_BASE_ADRS + 0x804))
#define	IMP_DAPR	((UINT32 *) (IMP_BASE_ADRS + 0x808))
#define	IMP_BCR		((UINT16 *) (IMP_BASE_ADRS + 0x80c))
#define	IMP_CSR		((UINT8 *)  (IMP_BASE_ADRS + 0x80e))
#define	IMP_FCR		((UINT8 *)  (IMP_BASE_ADRS + 0x810))
#define	IMP_GIMR	((UINT16 *) (IMP_BASE_ADRS + 0x812))
#define	IMP_IPR		((UINT16 *) (IMP_BASE_ADRS + 0x814))
#define	IMP_IMR		((UINT16 *) (IMP_BASE_ADRS + 0x816))
#define	IMP_ISR		((UINT16 *) (IMP_BASE_ADRS + 0x818))
#define IMP_PACNT	((UINT16 *) (IMP_BASE_ADRS + 0x81e))
#define IMP_PADDR	((UINT16 *) (IMP_BASE_ADRS + 0x820))
#define IMP_PADAT	((UINT16 *) (IMP_BASE_ADRS + 0x822))
#define IMP_PBCNT	((UINT16 *) (IMP_BASE_ADRS + 0x824))
#define IMP_PBDDR	((UINT16 *) (IMP_BASE_ADRS + 0x826))
#define IMP_PBDAT	((UINT16 *) (IMP_BASE_ADRS + 0x828))
#define IMP_BR0		((UINT16 *) (IMP_BASE_ADRS + 0x830))
#define IMP_OR0		((UINT16 *) (IMP_BASE_ADRS + 0x832))
#define IMP_BR1		((UINT16 *) (IMP_BASE_ADRS + 0x834))
#define IMP_OR1		((UINT16 *) (IMP_BASE_ADRS + 0x836))
#define IMP_BR2		((UINT16 *) (IMP_BASE_ADRS + 0x838))
#define IMP_OR2		((UINT16 *) (IMP_BASE_ADRS + 0x83a))
#define IMP_BR3		((UINT16 *) (IMP_BASE_ADRS + 0x83c))
#define IMP_OR3		((UINT16 *) (IMP_BASE_ADRS + 0x83e))
#define IMP_TMR1	((UINT16 *) (IMP_BASE_ADRS + 0x840))
#define IMP_TRR1	((UINT16 *) (IMP_BASE_ADRS + 0x842))
#define IMP_TCR1	((UINT16 *) (IMP_BASE_ADRS + 0x844))
#define IMP_TCN1	((UINT16 *) (IMP_BASE_ADRS + 0x846))
#define IMP_TER1	((UINT8 *)  (IMP_BASE_ADRS + 0x849))
#define IMP_WRR		((UINT16 *) (IMP_BASE_ADRS + 0x84a))
#define IMP_WCN		((UINT16 *) (IMP_BASE_ADRS + 0x84c))
#define IMP_TMR2	((UINT16 *) (IMP_BASE_ADRS + 0x850))
#define IMP_TRR2	((UINT16 *) (IMP_BASE_ADRS + 0x852))
#define IMP_TCR2	((UINT16 *) (IMP_BASE_ADRS + 0x854))
#define IMP_TCN2	((UINT16 *) (IMP_BASE_ADRS + 0x856))
#define IMP_TER2	((UINT8 *)  (IMP_BASE_ADRS + 0x859))
#define IMP_CR		((UINT8 *)  (IMP_BASE_ADRS + 0x860))
#define IMP_SCC1_REG	((SCC_REG *)(IMP_BASE_ADRS + 0x880))
#define IMP_SCC2_REG	((SCC_REG *)(IMP_BASE_ADRS + 0x890))
#define IMP_SCC3_REG	((SCC_REG *)(IMP_BASE_ADRS + 0x8a0))
#define IMP_SPMODE	((UINT16 *) (IMP_BASE_ADRS + 0x8b0))
#define IMP_SIMASK	((UINT16 *) (IMP_BASE_ADRS + 0x8b2))
#define IMP_SIMODE	((UINT16 *) (IMP_BASE_ADRS + 0x8b4))


/* Internal Register Defines */

/* CR - Command Registers */

#define CR_FLG		0x01		/* CP/IMP spinlock flag */
#define CR_GCI		0x40		/* GCI command */
#define CR_GCI_TAR	0x00		/* GCI - Transmit abort request */
#define CR_GCI_TOUT	0x10		/* GCI - Timeout */
#define CR_RST		0x80		/* software reset command */

/* GIMR - Global Interrupt Mode Registers */

#define GIMR_VEC_MASK 	0x00e0		/* vector bits 7 - 5 */
#define GIMR_EDGE1 	0x0100		/* IRQ1 edge triggered (else level) */
#define GIMR_EDGE6 	0x0200		/* IRQ6 edge triggered (else level) */
#define GIMR_EDGE7 	0x0400		/* IRQ7 edge triggered (else level) */
#define GIMR_EXT_VEC1 	0x1000		/* level 1 vector external */
#define GIMR_EXT_VEC6 	0x2000		/* level 6 vector external */
#define GIMR_EXT_VEC7 	0x4000		/* level 7 vector external */
#define GIMR_DISCRETE 	0x8000		/* IPL0-IPL2 are IRQ1/IRQ6/IRQ7 */

/* IPR/IMR/ISR - Interrupt Pending/Mask/In-Service Registers */

#define INT_ERR		0x0001		/* response to ext. level 4 */
#define INT_PB8		0x0002		/* parrallel port B (bit 8) */
#define INT_SMC2 	0x0004		/* serial management controller 2 */
#define INT_SMC1 	0x0008		/* serial management controller 1 */
#define INT_TMR3 	0x0010		/* timer 3 */
#define INT_SCP 	0x0020		/* serial comunication port */
#define INT_TMR2 	0x0040		/* timer 2 */
#define INT_PB9 	0x0080		/* parrallel port B (bit 9) */
#define INT_SCC3 	0x0100		/* serial comunication controller 3 */
#define INT_TMR1 	0x0200		/* timer 1 */
#define INT_SCC2 	0x0400		/* serial comunication controller 2 */
#define INT_IDMA 	0x0800		/* independent direct memory access */
#define INT_SDMA 	0x1000		/* serial direct memory access */
#define INT_SCC1 	0x2000		/* serial comunication controller 1 */
#define INT_PB10 	0x4000		/* parrallel port B (bit 10) */
#define INT_PB11 	0x8000		/* parrallel port B (bit 11) */

/* Interrupt Vectors */

#define INT_VEC_ERR	(0x0 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_PB8	(0x1 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_SMC2	(0x2 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_SMC1	(0x3 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_TMR3	(0x4 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_SCP	(0x5 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_TMR2	(0x6 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_PB9	(0x7 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_SCC3	(0x8 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_TMR1	(0x9 | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_SCC2	(0xa | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_IDMA	(0xb | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_SDMA	(0xc | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_SCC1	(0xd | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_PB10	(0xe | (*IMP_GIMR & GIMR_VEC_MASK))
#define INT_VEC_PB11	(0xf | (*IMP_GIMR & GIMR_VEC_MASK))

/* TMR1/TMR2 - Timer Mode Register */

#define TMR_ENABLE 	0x0001		/* enable timer */
#define TMR_STOP	0x0000		/* stop count */
#define TMR_CLK		0x0002		/* count source is master clock */
#define TMR_CLK16	0x0004		/* count source is master clock/16 */
#define TMR_CLK_EXT	0x0006		/* count source is falling edge TIN */
#define TMR_RESTART	0x0008		/* count is restarted upon ref cnt */
#define TMR_INT		0x0010		/* enable interrupt upon ref cnt */
#define TMR_TOGGLE	0x0020		/* toggle output */
#define TMR_CAP_DIS	0x0000		/* disable interrupt on capture event */
#define TMR_CAP_RISE	0x0040		/* enable int w/ capture rising edge */
#define TMR_CAP_FALL	0x0080		/* enable int w/ capture falling edge */
#define TMR_CAP_ANY	0x00c0		/* enable int w/ capture any edge */

/* TER1/TER2 - Timer Event Register */

#define TER_CAPTURE 	0x01		/* counter value has been captured */
#define TER_REF_CNT	0x02		/* counter value reached ref. cnt */

/* CS_BR - Chip Select Base Register */

#define CS_BR_ENABLE	0x0001		/* chip select pin is enabled */
#define CS_BR_WRITE	0x0002		/* chip select pin write only */

/* CS_OR - Chip Select Options Register */

#define CS_OR_CFC	0x0001		/* function code of CS_BR compared */
#define CS_OR_UNMASK_RW	0x0002		/* r/w bit of CS_BR is not masked */

/* SCR - System Control Register */

#define SCR_LPEN 	0x00000020	/* low power mode enabled */
#define SCR_LPP16 	0x00000040	/* low power clock/16 prescale enable */
#define SCR_LPREC 	0x00000080	/* destructive low power recovery */
#define SCR_HWDEN 	0x00000800	/* hardware watchdog enable */
#define SCR_SAM 	0x00001000	/* synchronous access mode */
#define SCR_FRZ1 	0x00002000	/* freeze timer 1 */
#define SCR_FRZ2 	0x00004000	/* freeze timer 2 */
#define SCR_FRZW 	0x00008000	/* freeze watchdog timer */
#define SCR_BCLM 	0x00010000	/* BCLR derived from IPEND */
#define SCR_ADCE 	0x00020000	/* BERR on address decode conflict */
#define SCR_EMWS 	0x00040000	/* external master wait state */
#define SCR_RMCST 	0x00080000	/* RMC cycle special treatment */
#define SCR_WPVE 	0x00100000	/* BERR on write protect violation */
#define SCR_VGE 	0x00200000	/* vector generation enable (discpu) */
#define SCR_ERRE 	0x00400000	/* external RISC request enable */
#define SCR_ADC 	0x01000000	/* address decode conflict */
#define SCR_WPV 	0x02000000	/* write protect violation */
#define SCR_HWT 	0x04000000	/* hardware watchdog timeout */
#define SCR_IPA 	0x08000000	/* interrupt priority active */

/* SCC - Serial Comunications Controller */

typedef struct		/* SCC_BUF */
    {
    UINT16	statusMode;		/* status/mode protocal specific */
    UINT16	dataLength;		/* length of buffer in bytes */
    char *	dataPointer;		/* points to data buffer */
    } SCC_BUF;

typedef struct		/* SCC_PARAM */
    {
    UINT8	rfcr;			/* receive function code */
    UINT8	tfcr;			/* transmit function code */
    UINT16	mrblr;			/* maximum receive buffer length */
    UINT16	res1;			/* reserved/internal */
    UINT8	res2;			/* reserved/internal */
    UINT8	rbd;			/* receive internal buffer number */
    UINT16	res3[5];		/* reserved/internal */
    UINT8	res4;			/* reserved/internal */
    UINT8	tbd;			/* transmit internal buffer number */
    UINT16	res5[4];		/* reserved/internal */
    } SCC_PARAM;

typedef struct		/* SCC */
    {
    SCC_BUF	rxBd[8];		/* receive buffer descriptors */
    SCC_BUF	txBd[8];		/* transmit buffer descriptors */
    SCC_PARAM	param;			/* SCC parameters */
    char	prot[64];		/* protocol specific area */
    } SCC;

typedef struct		/* SCC3 */
    {
    SCC_BUF	rxBd[8];		/* receive buffer descriptors */
    SCC_BUF	txBd[4];		/* transmit buffer descriptors */
    UINT16	res1[3];		/* reserved/internal */
    UINT16	smc1RxBd;		/* SMC1 receive buffer descriptor */
    UINT16	smc1TxBd;		/* SMC1 transmit buffer descriptor */
    UINT16	smc2RxBd;		/* SMC2 receive buffer descriptor */
    UINT16	smc2TxBd;		/* SMC2 transmit buffer descriptor */
    UINT16	res2[6];		/* reserved/internal */
    UINT16	scpBd;			/* SCP buffer descriptor */
    UINT16	sccBerr;		/* SCC channel bus error status */
    UINT16	rev;			/* MC68302 revision number */
    SCC_PARAM	param;			/* SCC parameters */
    char	prot[64];		/* protocol specific area */
    } SCC3;

typedef struct		/* SCC_REG */
    {
    UINT16	res1;			/* reserved/internal */
    UINT16	scon;			/* configuration register */
    UINT16	scm;			/* mode register */
    UINT16	dsr;			/* data sync. register */
    UINT8	scce;			/* event register */
    UINT8	res2;			/* reserved/internal */
    UINT8	sccm;			/* mask register */
    UINT8	res3;			/* reserved/internal */
    UINT8	sccs;			/* status register */
    UINT8	res4;			/* reserved/internal */
    UINT16	res5;			/* reserved/internal */
    } SCC_REG;

/* SCC Configuration Register */

#define SCON_DIV4		0x0001	/* SCC clock prescaler divide by 4 */
#define SCON_EXT_RX_CLK		0x1000	/* RCLK pin is rx baud rate source */
#define SCON_EXT_TX_CLK		0x2000	/* RCLK pin is tx baud rate source */
#define SCON_EXT_BAUD		0x4000	/* TIN1 pin is baud rate clk source */
#define SCON_WIRED_OR		0x8000	/* TXD pin is placed in open drain */

/* UART Protocol - SCC protocol specific parameters */

typedef struct		/* PROT_UART */
    {
    UINT16	maxIdl;			/* maximum idle characters */
    UINT16	idlc;			/* temporary receive idle counter */
    UINT16	brkcr;			/* break count register */
    UINT16	parec;			/* receive parity error counter */
    UINT16	frmec;			/* receive framing error counter */
    UINT16	nosec;			/* receive noise counter */
    UINT16	brkec;			/* receive break character counter */
    UINT16	uaddr1;			/* uart address character 1 */
    UINT16	uaddr2;			/* uart address character 2 */
    UINT16	rccr;			/* receive control character register */
    UINT16	cntChar1;		/* control character 1 */
    UINT16	cntChar2;		/* control character 2 */
    UINT16	cntChar3;		/* control character 3 */
    UINT16	cntChar4;		/* control character 4 */
    UINT16	cntChar5;		/* control character 5 */
    UINT16	cntChar6;		/* control character 6 */
    UINT16	cntChar7;		/* control character 7 */
    UINT16	cntChar8;		/* control character 8 */
    } PROT_UART;

/* UART Protocol - SCC Mode Register */

#define UART_SCM_HDLC		0x0000	/* hdlc mode */
#define UART_SCM_ASYNC		0x0001	/* asynchronous mode (uart/ddcmp) */
#define UART_SCM_SYNC		0x0002	/* synchronous mode (ddcmp/v.110) */
#define UART_SCM_BISYNC		0x0003	/* bisync/promiscuous mode */
#define UART_SCM_ENT		0x0004	/* enable transmitter */
#define UART_SCM_ENR		0x0008	/* enable receiver */
#define UART_SCM_LOOPBACK	0x0010	/* transmitter hooked to receiver */
#define UART_SCM_ECHO		0x0020	/* transmitter echoes received chars */
#define UART_SCM_MANUAL		0x0030	/* cts/cd under software control */
#define UART_SCM_STOP_2		0x0040	/* two stop bits (one otherwise) */
#define UART_SCM_RTSM		0x0080	/* 0:rts w/ tx char; 1:rts w/ tx enbl */
#define UART_SCM_8BIT		0x0100	/* 8 bit char. length (7 otherwise) */
#define UART_SCM_FRZ_TX		0x0200	/* freeze transmitter */
#define UART_SCM_DROP1		0x0400	/* multidrop w/o adrs. recognition */
#define UART_SCM_DDCMP		0x0800	/* ddcmp protocol selected */
#define UART_SCM_DROP2		0x0c00	/* multidrop w adrs. recognition */
#define UART_SCM_PARITY		0x1000	/* parity is enabled */
#define UART_SCM_RX_EVEN	0x2000	/* rx even parity */
#define UART_SCM_TX_ODD		0x0000	/* tx odd parity */
#define UART_SCM_TX_SPACE	0x4000	/* tx space parity */
#define UART_SCM_TX_EVEN	0x8000	/* tx even parity */
#define UART_SCM_TX_MARK	0xc000	/* tx mark parity */

/* UART Protocol - SCC Event Register */

#define UART_SCCE_RX 		0x01	/* buffer received */
#define UART_SCCE_TX 		0x02	/* buffer transmitted */
#define UART_SCCE_BSY 		0x04	/* character discarded, no buffers */
#define UART_SCCE_CCR 		0x08	/* control character received */
#define UART_SCCE_BRK 		0x10	/* break character received */
#define UART_SCCE_IDL 		0x20	/* idle sequence status changed */
#define UART_SCCE_CD  		0x40	/* carrier detect status changed */
#define UART_SCCE_CTS 		0x80	/* clear to send status changed */

/* UART Protocol - Receive Buffer Descriptor */

#define UART_RX_BD_CD		0x0001	/* carrier detect loss during rx */
#define UART_RX_BD_OV		0x0002	/* receiver overrun */
#define UART_RX_BD_PR		0x0008	/* parity error */
#define UART_RX_BD_FR		0x0010	/* framing error */
#define UART_RX_BD_BR		0x0020	/* break received */
#define UART_RX_BD_ID		0x0100	/* buf closed on reception of IDLES */
#define UART_RX_BD_UADDR1	0x0200	/* matched UADDR1 (UADDR2 otherwise) */
#define UART_RX_BD_ADDR		0x0400	/* buffer contained address */
#define UART_RX_BD_CNT		0x0800	/* control character in last byte */
#define UART_RX_BD_INT		0x1000	/* interrupt will be generated */
#define UART_RX_BD_WRAP		0x2000	/* wrap back to first BD */
#define UART_RX_BD_EXT		0x4000	/* buffer in external memory */
#define UART_RX_BD_EMPTY	0x8000	/* buffer is empty (available to CP) */

/* UART Protocol - Transmit Buffer Descriptor */

#define UART_TX_BD_CT		0x0001	/* cts was lost during tx */
#define UART_TX_BD_PREAMBLE	0x0200	/* enable preamble */
#define UART_TX_BD_ADDR		0x0400	/* buffer contained address char */
#define UART_TX_BD_CTSR		0x0800	/* normal cts error reporting */
#define UART_TX_BD_INT		0x1000	/* interrupt will be generated */
#define UART_TX_BD_WRAP		0x2000	/* wrap back to first BD */
#define UART_TX_BD_EXT		0x4000	/* buffer in external memory */
#define UART_TX_BD_READY	0x8000	/* buffer is being sent (don't touch) */


/* HDLC Protocol - SCC protocol specific parameters */

typedef struct		/* PROT_HDLC */
    {
    UINT16	rcrcLow;		/* temporary receive crc low */
    UINT16	rcrcHigh;		/* temporary receive crc high */
    UINT16	cMaskLow;		/* constant (0xf0b8:16b; 0xdebb:32b) */
    UINT16	cMaskHigh;		/* constant (0xf0b8:16b; 0x20e3:32b) */
    UINT16	tCrcLow;		/* temporary transmit crc low */
    UINT16	tCrcHigh;		/* temporary transmit crc high */
    UINT16	disfc;			/* discard frame counter */
    UINT16	crcec;			/* crc error counter */
    UINT16	abtsc;			/* abort sequence counter */
    UINT16	nmarc;			/* nonmatching adrs received counter */
    UINT16	retrc;			/* frame retransmission counter */
    UINT16	mflr;			/* max frame length register */
    UINT16	maxCnt;			/* max length counter */
    UINT16	hmask;			/* user-defined frame address mask */
    UINT16	haddr1;			/* user-defined frame address */
    UINT16	haddr2;			/* user-defined frame address */
    UINT16	haddr3;			/* user-defined frame address */
    UINT16	haddr4;			/* user-defined frame address */
    } PROT_HDLC;

/* HDLC Protocol - SCC Mode Register */

#define HDLC_SCM_HDLC		0x0000	/* hdlc mode */
#define HDLC_SCM_ASYNC		0x0001	/* asynchronous mode (uart/ddcmp) */
#define HDLC_SCM_SYNC		0x0002	/* synchronous mode (ddcmp/v.110) */
#define HDLC_SCM_BISYNC		0x0003	/* bisync/promiscuous mode */
#define HDLC_SCM_ENT		0x0004	/* enable transmitter */
#define HDLC_SCM_ENR		0x0008	/* enable receiver */
#define HDLC_SCM_LOOPBACK	0x0010	/* transmitter hooked to receiver */
#define HDLC_SCM_ECHO		0x0020	/* transmitter echoes received chars */
#define HDLC_SCM_MANUAL		0x0030	/* cts/cd under software control */
#define HDLC_SCM_NRZI		0x0040	/* nrzi tx encoding (nrz otherwise) */
#define HDLC_SCM_FLG		0x0080	/* send flags between frames */
#define HDLC_SCM_RTE		0x0100	/* retransmit enable */
#define HDLC_SCM_FSE		0x0400	/* flag sharing enable */
#define HDLC_SCM_C32		0x0800	/* 32 bit CCITT crc */

/* HDLC Protocol - SCC Event Register */

#define HDLC_SCCE_RXB 		0x01	/* incomplete buffer received */
#define HDLC_SCCE_TXB 		0x02	/* buffer transmitted */
#define HDLC_SCCE_BSY 		0x04	/* frame discarded, no buffers */
#define HDLC_SCCE_RXF 		0x08	/* complete frame received */
#define HDLC_SCCE_TXE 		0x10	/* transmitter error */
#define HDLC_SCCE_IDL 		0x20	/* idle sequence status changed */
#define HDLC_SCCE_CD  		0x40	/* carrier detect status changed */
#define HDLC_SCCE_CTS 		0x80	/* clear to send status changed */

/* HDLC Protocol - Receive Buffer Descriptor */

#define HDLC_RX_BD_CD		0x0001	/* carrier detect loss during rx */
#define HDLC_RX_BD_OV		0x0002	/* receiver overrun */
#define HDLC_RX_BD_CRC		0x0008	/* crc error */
#define HDLC_RX_BD_ABORT	0x0010	/* abort sequence */
#define HDLC_RX_BD_NO		0x0020	/* non-octet frame received */
#define HDLC_RX_BD_LG		0x0040	/* rx frame length violation */
#define HDLC_RX_BD_FIRST	0x0400	/* buffer first in frame */
#define HDLC_RX_BD_LAST		0x0800	/* buffer last in frame */
#define HDLC_RX_BD_INT		0x1000	/* interrupt will be generated */
#define HDLC_RX_BD_WRAP		0x2000	/* wrap back to first BD */
#define HDLC_RX_BD_EXT		0x4000	/* buffer in external memory */
#define HDLC_RX_BD_EMPTY	0x8000	/* buffer is empty (available to CP) */

/* HDLC Protocol - Transmit Buffer Descriptor */

#define HDLC_TX_BD_CT		0x0001	/* cts was lost during tx */
#define HDLC_TX_BD_UN		0x0002	/* underrun during tx */
#define HDLC_TX_BD_CRC		0x0400	/* transmit crc after last byte */
#define HDLC_TX_BD_LAST		0x0800	/* last buffer in current frame */
#define HDLC_TX_BD_INT		0x1000	/* interrupt will be generated */
#define HDLC_TX_BD_WRAP		0x2000	/* wrap back to first BD */
#define HDLC_TX_BD_EXT		0x4000	/* buffer in external memory */
#define HDLC_TX_BD_READY	0x8000	/* buffer is being sent (don't touch) */


/* BISYNC Protocol - SCC protocol specific parameters */

typedef struct		/* PROT_BISYNC */
    {
    UINT16	rcrc;			/* temporary receive crc */
    UINT16	crcc;			/* crc constant */
    UINT16	prcrc;			/* preset receiver crc16/lrc */
    UINT16	tcrc;			/* temporary transmit crc */
    UINT16	ptcrc;			/* preset transmit crc16/lrc */
    UINT16	res1;			/* reserved */
    UINT16	res2;			/* reserved */
    UINT16	parec;			/* receive parity error counter */
    UINT16	bsync;			/* bisync sync character */
    UINT16	bdle;			/* bisync dle character */
    UINT16	cntChar1;		/* control character 1 */
    UINT16	cntChar2;		/* control character 2 */
    UINT16	cntChar3;		/* control character 3 */
    UINT16	cntChar4;		/* control character 4 */
    UINT16	cntChar5;		/* control character 5 */
    UINT16	cntChar6;		/* control character 6 */
    UINT16	cntChar7;		/* control character 7 */
    UINT16	cntChar8;		/* control character 8 */
    } PROT_BISYNC;

/* BISYNC Protocol - SCC Mode Register */

#define BISYNC_SCM_HDLC		0x0000	/* hdlc mode */
#define BISYNC_SCM_ASYNC	0x0001	/* asynchronous mode (uart/ddcmp) */
#define BISYNC_SCM_SYNC		0x0002	/* synchronous mode (ddcmp/v.110) */
#define BISYNC_SCM_BISYNC	0x0003	/* bisync/promiscuous mode */
#define BISYNC_SCM_ENT		0x0004	/* enable transmitter */
#define BISYNC_SCM_ENR		0x0008	/* enable receiver */
#define BISYNC_SCM_LOOPBACK	0x0010	/* transmitter hooked to receiver */
#define BISYNC_SCM_ECHO		0x0020	/* transmitter echoes received chars */
#define BISYNC_SCM_MANUAL	0x0030	/* cts/cd under software control */
#define BISYNC_SCM_NRZI		0x0040	/* nrzi tx encoding (nrz otherwise) */
#define BISYNC_SCM_SYNF		0x0080	/* send SYN1-SYN2 pairs between msgs */
#define BISYNC_SCM_RBCS		0x0100	/* enable receive BCS */
#define BISYNC_SCM_RTR		0x0200	/* enable receiver transparent mode */
#define BISYNC_SCM_BCS		0x0800	/* CRC16 (LRC otherwise) */
#define BISYNC_SCM_REVD		0x1000	/* reverse data (msb first) */
#define BISYNC_SCM_NTSYN	0x2000	/* no transmit sync */
#define BISYNC_SCM_EXSYN	0x4000	/* external sync mode */
#define BISYNC_SCM_EVEN		0x8000	/* even parity */

/* BISYNC Protocol - SCC Event Register */

#define BISYNC_SCCE_RX		0x01	/* buffer received */
#define BISYNC_SCCE_TX		0x02	/* buffer transmitted */
#define BISYNC_SCCE_BSY		0x04	/* character discarded, no buffers */
#define BISYNC_SCCE_RCH		0x08	/* character received, put in buffer */
#define BISYNC_SCCE_TXE		0x10	/* transmitter error */
#define BISYNC_SCCE_CD 		0x40	/* carrier detect status changed */
#define BISYNC_SCCE_CTS		0x80	/* clear to send status changed */

/* BISYNC Protocol - Receive Buffer Descriptor */

#define BISYNC_RX_BD_CD		0x0001	/* carrier detect loss during rx */
#define BISYNC_RX_BD_OV		0x0002	/* receiver overrun */
#define BISYNC_RX_BD_BCS_ERR	0x0004	/* bcs error */
#define BISYNC_TX_BD_PARITY	0x0008	/* parity error */
#define BISYNC_RX_BD_DLE	0x0010	/* DLE follow character error */
#define BISYNC_RX_BD_BCS	0x0400	/* BCS received */
#define BISYNC_RX_BD_CNT	0x0800	/* last byte is control character */
#define BISYNC_RX_BD_INT	0x1000	/* interrupt will be generated */
#define BISYNC_RX_BD_WRAP	0x2000	/* wrap back to first BD */
#define BISYNC_RX_BD_EXT	0x4000	/* buffer in external memory */
#define BISYNC_RX_BD_EMPTY	0x8000	/* buffer is empty (available to CP) */

/* BISYNC Protocol - Transmit Buffer Descriptor */

#define BISYNC_TX_BD_CT		0x0001	/* cts was lost during tx */
#define BISYNC_TX_BD_UN		0x0002	/* underrun during tx */
#define BISYNC_TX_BD_TR		0x0040	/* transparent mode */
#define BISYNC_TX_BD_TX_DLE 	0x0080	/* transmit DLE character */
#define BISYNC_TX_BD_BR 	0x0100	/* reset BCS accumulation */
#define BISYNC_TX_BD_BCS	0x0200	/* include buffer in BCS accumulation */
#define BISYNC_TX_BD_BCS_TX	0x0400	/* transmit BCS after last byte */
#define BISYNC_TX_BD_LAST	0x0800	/* last buffer in current frame */
#define BISYNC_TX_BD_INT	0x1000	/* interrupt will be generated */
#define BISYNC_TX_BD_WRAP	0x2000	/* wrap back to first BD */
#define BISYNC_TX_BD_EXT	0x4000	/* buffer in external memory */
#define BISYNC_TX_BD_READY	0x8000	/* buffer is being sent (don't touch) */


/* DDCMP Protocol - SCC protocol specific parameters */

typedef struct		/* PROT_DDCMP */
    {
    UINT16	rcrc;			/* temporary receive crc */
    UINT16	crcc;			/* crc constant */
    UINT16	pcrc;			/* preset receiver crc16 */
    UINT16	tcrc;			/* temporary transmit crc */
    UINT16	dsoh;			/* ddcmp soh character */
    UINT16	denq;			/* ddcmp enq character */
    UINT16	ddle;			/* ddcmp dle character */
    UINT16	crc1ec;			/* crc1 error counter */
    UINT16	crc2ec;			/* crc2 error counter */
    UINT16	nmarc;			/* nonmatching adrs received counter */
    UINT16	dismc;			/* discard message counter */
    UINT16	rmlg;			/* received message length */
    UINT16	rmlgCnt;		/* received message length counter */
    UINT16	dmask;			/* user-defined frame address mask */
    UINT16	daddr1;			/* user-defined frame address */
    UINT16	daddr2;			/* user-defined frame address */
    UINT16	daddr3;			/* user-defined frame address */
    UINT16	daddr4;			/* user-defined frame address */
    } PROT_DDCMP;

/* DDCMP Protocol - SCC Mode Register */

#define DDCMP_SCM_HDLC		0x0000	/* hdlc mode */
#define DDCMP_SCM_ASYNC		0x0001	/* asynchronous mode (uart/ddcmp) */
#define DDCMP_SCM_SYNC		0x0002	/* synchronous mode (ddcmp/v.110) */
#define DDCMP_SCM_BISYNC	0x0003	/* bisync/promiscuous mode */
#define DDCMP_SCM_ENT		0x0004	/* enable transmitter */
#define DDCMP_SCM_ENR		0x0008	/* enable receiver */
#define DDCMP_SCM_LOOPBACK	0x0010	/* transmitter hooked to receiver */
#define DDCMP_SCM_ECHO		0x0020	/* transmitter echoes received chars */
#define DDCMP_SCM_MANUAL	0x0030	/* cts/cd under software control */
#define DDCMP_SCM_NRZI		0x0040	/* nrzi tx encoding (nrz otherwise) */
#define DDCMP_SCM_SYNF		0x0080	/* send SYN1-SYN2 pairs between msgs */
#define DDCMP_SCM_V110		0x0400	/* V.110 protocol selected */

/* DDCMP Protocol - SCC Event Register */

#define DDCMP_SCCE_RBD 		0x01	/* incomplete buffer received */
#define DDCMP_SCCE_TX 		0x02	/* buffer transmitted */
#define DDCMP_SCCE_BSY 		0x04	/* data byte discarded, no buffers */
#define DDCMP_SCCE_RBK 		0x08	/* complete block received */
#define DDCMP_SCCE_TXE 		0x10	/* transmitter error */
#define DDCMP_SCCE_CD  		0x40	/* carrier detect status changed */
#define DDCMP_SCCE_CTS 		0x80	/* clear to send status changed */

/* DDCMP Protocol - Receive Buffer Descriptor */

#define DDCMP_RX_BD_CD		0x0001	/* carrier detect loss during rx */
#define DDCMP_RX_BD_OV		0x0002	/* receiver overrun */
#define DDCMP_RX_BD_CRC_ERR	0x0004	/* crc error */
#define DDCMP_TX_BD_PARITY	0x0008	/* parity error */
#define DDCMP_RX_BD_FR		0x0010	/* framing error */
#define DDCMP_RX_BD_CF		0x0020	/* crc follow error */
#define DDCMP_RX_BD_DATA	0x0000	/* data message */
#define DDCMP_RX_BD_CNT		0x0100	/* control message */
#define DDCMP_RX_BD_MAINT	0x0200	/* maintenance message */
#define DDCMP_RX_BD_HEADER	0x0400	/* buffer contains header */
#define DDCMP_RX_BD_LAST	0x0800	/* last byte is control character */
#define DDCMP_TX_BD_INT		0x1000	/* interrupt will be generated */
#define DDCMP_RX_BD_WRAP	0x2000	/* wrap back to first BD */
#define DDCMP_RX_BD_EXT		0x4000	/* buffer in external memory */
#define DDCMP_RX_BD_EMPTY	0x8000	/* buffer is empty (available to CP) */

/* DDCMP Protocol - Transmit Buffer Descriptor */

#define DDCMP_TX_BD_CT		0x0001	/* cts was lost during tx */
#define DDCMP_TX_BD_UN		0x0002	/* underrun during tx */
#define DDCMP_TX_BD_OPTLAST	0x0200	/* optional last */
#define DDCMP_TX_BD_TX_CRC	0x0400	/* transmit crc16 after last byte */
#define DDCMP_TX_BD_LAST	0x0800	/* last buffer in current frame */
#define DDCMP_TX_BD_INT		0x1000	/* interrupt will be generated */
#define DDCMP_TX_BD_WRAP	0x2000	/* wrap back to first BD */
#define DDCMP_TX_BD_EXT		0x4000	/* buffer in external memory */
#define DDCMP_TX_BD_READY	0x8000	/* buffer is being sent (don't touch) */


/* V.110 Protocol - SCC protocol specific parameters */

/* V.110 Protocol - SCC Event Register */

#define V110_SCCE_TX 		0x02	/* buffer transmitted */
#define V110_SCCE_BSY 		0x04	/* data byte discarded, no buffers */
#define V110_SCCE_RXF 		0x08	/* complete frame received */
#define V110_SCCE_TXE 		0x10	/* transmitter error */

/* V110 Protocol - Receive Buffer Descriptor */

#define V110_RX_BD_OV		0x0002	/* receiver overrun */
#define V110_RX_BD_SYNC_ERR	0x0008	/* synchronization error */
#define V110_RX_BD_WRAP		0x2000	/* wrap back to first BD */
#define V110_RX_BD_EXT		0x4000	/* buffer in external memory */
#define V110_RX_BD_EMPTY	0x8000	/* buffer is empty (available to CP) */

/* V110 Protocol - Transmit Buffer Descriptor */

#define V110_TX_BD_UN		0x0002	/* underrun during tx */
#define V110_TX_BD_TR		0x0040	/* transparent mode */
#define V110_TX_BD_LAST		0x0800	/* last buffer in current frame */
#define V110_TX_BD_INT		0x1000	/* interrupt will be generated */
#define V110_TX_BD_WRAP		0x2000	/* wrap back to first BD */
#define V110_TX_BD_EXT		0x4000	/* buffer in external memory */
#define V110_TX_BD_READY	0x8000	/* buffer is being sent (don't touch) */

#ifndef	INCLUDE_TY_CO_DRV_50

typedef struct          /* TY_CO_DEV */
    {
    TY_DEV		tyDev;	    /* tyLib will handle this portion */
    BOOL		created;    /* true if this device has been created */
    char      		numChannels; /* number of channels to support */
    int      		clockRate;  /* CPU clock frequency (Hz) */
    volatile SCC *	pScc;	    /* serial communication cntrlr */
    volatile SCC_REG *	pSccReg;    /* serial communication cntrlr registers */
    int       		rxBdNext;   /* next rcv buffer descriptor to fill */
    UINT16    		intAck;	    /* interrupt acknowledge mask */
    } TY_CO_DEV;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

IMPORT	void	tyCoInt (TY_CO_DEV * pDv);

#else	/* __STDC__ */

IMPORT	void	tyCoInt ();

#endif	/* __STDC__ */
#endif	/* INCLUDE_TY_CO_DRV_50 */
#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68302h */
