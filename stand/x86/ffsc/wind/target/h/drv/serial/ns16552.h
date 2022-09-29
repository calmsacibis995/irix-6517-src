
/* ns16552.h - National Semiconductor 16552 DUART header file */

/*
modification history
--------------------
01c,10dec96,db	 added elements options and mcr to NS_16550 structure to
		 implement hardware options & modem control(SPRs #7570, #7082).
01b,01nov95,myz  removed #if CPU=I960XX ... in NS16550_CHAN structure
01a,24oct95,myz  written from ns16552.h.
*/

#ifndef __INCns16552Sioh 
#define __INCns16552Sioh 

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * 		Copyright (c) 1990,1991 Intel Corporation
 *
 * Intel hereby grants you permission to copy, modify, and
 * distribute this software and its documentation.  Intel grants
 * this permission provided that the above copyright notice
 * appears in all copies and that both the copyright notice and
 * this permission notice appear in supporting documentation.  In
 * addition, Intel grants this permission provided that you
 * prominently mark as not part of the original any modifications
 * made to this software or documentation, and that the name of
 * Intel Corporation not be used in advertising or publicity
 * pertaining to distribution of the software or the documentation
 * without specific, written prior permission.
 *
 * Intel Corporation does not warrant, guarantee or make any
 * representations regarding the use of, or the results of the use
 * of, the software and documentation in terms of correctness,
 * accuracy, reliability, currentness, or otherwise; and you rely
 * on the software, documentation and results solely at your own risk.
 *********************************************************************/


/******************************************************************************
 *
 * REGISTER DESCRIPTION OF NATIONAL 16552 DUART
 *
 * $Id: ns16552.h,v 1.4 1997/08/19 22:56:43 morrow Exp $
 ******************************************************************************/

#ifndef _ASMLANGUAGE

#define	UART_XTAL	1843200

/* Register offsets from base address */

#define UART_DATA	0x00
#define UART_RDR	0x00
#define UART_THR	0x00
#define UART_DLL	0x00
#define UART_IER	0x01
#define UART_DLM	0x01
#define UART_ISR	0x02
#define UART_FCR	0x02
#define UART_LCR	0x03
#define UART_MCR	0x04
#define UART_LSR	0x05
#define UART_MSR	0x06
#define UART_SPR	0x07

#define BAUD_LO(baud)  ((XTAL/(16*baud)) & 0xff)
#define BAUD_HI(baud)  (((XTAL/(16*baud)) & 0xff00) >> 8)

/* Line Control Register */

#define CHAR_LEN_5	0x00
#define CHAR_LEN_6	0x01
#define CHAR_LEN_7	0x02
#define CHAR_LEN_8	0x03
#define LCR_STB		0x04
#define ONE_STOP	0x00
#define LCR_PEN		0x08
#define PARITY_NONE	0x00
#define LCR_EPS		0x10
#define LCR_SP		0x20
#define LCR_SBRK	0x40
#define LCR_DLAB	0x80
#define DLAB		LCR_DLAB

/* Line Status Register */

#define LSR_DR		0x01
#define RxCHAR_AVAIL	LSR_DR
#define LSR_OE		0x02
#define LSR_PE		0x04
#define LSR_FE		0x08
#define LSR_BI		0x10
#define LSR_THRE	0x20
#define LSR_TEMT	0x40
#define LSR_FERR	0x80

/* Interrupt Identification Register */

#define IIR_IP		0x01
#define IIR_ID		0x0e
#define IIR_RLS		0x06
#define Rx_INT		IIR_RLS
#define IIR_RDA		0x04
#define RxFIFO_INT	IIR_RDA
#define IIR_THRE	0x02
#define TxFIFO_INT	IIR_THRE
#define IIR_MSTAT	0x00
#define IIR_TIMEOUT	0x0c

/* Interrupt Enable Register */

#define IER_ERDAI	0x01
#define RxFIFO_BIT	IER_ERDAI
#define IER_ETHREI	0x02
#define TxFIFO_BIT	IER_ETHREI
#define IER_ELSI	0x04
#define Rx_BIT		IER_ELSI
#define IER_EMSI	0x08

/* Modem Control Register */

#define MCR_DTR		0x01
#define DTR		MCR_DTR
#define MCR_RTS		0x02
#define MCR_OUT1	0x04
#define MCR_OUT2	0x08
#define MCR_LOOP	0x10

/* Modem Status Register */

#define MSR_DCTS	0x01
#define MSR_DDSR	0x02
#define MSR_TERI	0x04
#define MSR_DDCD	0x08
#define MSR_CTS		0x10
#define MSR_DSR		0x20
#define MSR_RI		0x40
#define MSR_DCD		0x80

/* FIFO Control Register */

#define FCR_EN		0x01
#define FIFO_ENABLE	FCR_EN
#define FCR_RXCLR	0x02
#define RxCLEAR		FCR_RXCLR
#define FCR_TXCLR	0x04
#define TxCLEAR		FCR_TXCLR
#define FCR_DMA		0x08
#define FCR_RXTRIG_L	0x40
#define FCR_RXTRIG_H	0x80

typedef struct NS16550_CHAN
    {
    /* always goes first */

    SIO_DRV_FUNCS *     pDrvFuncs;      /* driver functions */

    /* callbacks */

	STATUS      (*getTxChar) ();
	STATUS      (*putRcvChar) ();
	void *      getTxArg;
	void *      putRcvArg;

	UINT8 *regs;        /* NS16552 registers */
	UINT8 level;        /* 8259a interrupt level for this device */

	/* copy of register write only register contents */
	UINT8 ier;        /* copy of ier */
	UINT8 fcr;        /* copy of lcr, not used by ns16552 driver */
	UINT8 lcr;			/* copy of modem control register */
	ULONG	mcr;			/* copy Modem Control Register  */
	ULONG	dll;			/* copy LSB of Divisor Latch  */
	ULONG	dlm;			/* copy MSB of Divisor Latch */

	/* functions for accessing i/o space */
	UCHAR	(*inByte) (int);				/* Routine to read byte from register */
	void	(*outByte) (int,char);		/* Routine to write byte to register */

	/* address of registers */
	ULONG	data_add;		/* UART data Register (R/W) */
	ULONG	ier_add;			/* UART Interrupt Enable Register (W) */
	ULONG	fcr_add;			/* UART FIFO Control Register (W) */ 
	ULONG	isr_add;			/* UART Interrupt Status Register (R) */
	ULONG	lcr_add;			/* UART Line Control Register (W) */
	ULONG	mcr_add;			/* UART Modem Control Register (W) */
	ULONG	lsr_add;			/* UART Line Status Register (R) */
	ULONG	msr_add;			/* UART Modem Status Register (R) */
	ULONG	spr_add;			/* UART Scratch Pad Register (not used) */
	ULONG	dll_add;			/* UART LSB of Divisor Latch (W) */
	ULONG	dlm_add;			/* UART MSB of Divisor Latch (W) */
	
	UINT16          channelMode;   /* such as INT, POLL modes */
	UINT16          regDelta;      /* register address spacing */
	int             baudRate;
	UINT32          xtal;          /* UART clock frequency     */     
	UINT32	    options;	   /* hardware setup options */

	} NS16550_CHAN;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void ns16550Int (NS16550_CHAN *);
extern void ns16550DevInit (NS16550_CHAN *);

#else

extern void ns16550Int ();
extern void ns16550DevInit ();

#endif  /* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif
 
#endif /* __INCns16552Sioh */

