/* s2681.h - Signetics S2681 serial chip header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,09jul92,ajm  5.1 merge
01b,28apr92,ajm  updated for more that one duart per board
01a,10feb92,ajm  written from version 01d m68681.h
*/

/*
This file contains constants for the Signetics S2681 serial chip.

The constants DUART_REG_OFFSET and SIO must defined when
including this header file.
*/

#ifndef __INCs2681h
#define __INCs2681h

#ifdef __cplusplus
extern "C" {
#endif

#define N_CHANNELS    2	/* number of serial channels on chip */

#define SIO_ADRS(reg, base)	(base + (reg * DUART_REG_OFFSET))

/* SIO -- S2681 serial channel chip -- register definitions */

#define DUART_MRA(base)		SIO_ADRS(0x00,base)	/* mode reg. A */
#define DUART_CSRA(base)	SIO_ADRS(0x01,base)	/* clock sel. reg. A */
#define DUART_SRA(base)		DUART_CSRA(base)	/* status reg. A */
#define DUART_CRA(base)		SIO_ADRS(0x02,base)	/* command reg. A */
#define DUART_THRA(base)	SIO_ADRS(0x03,base)	/* transmit buffer A */
#define DUART_RHRA(base)	DUART_THRA(base)	/* receive buffer A */
#define DUART_ACR(base)		SIO_ADRS(0x04,base)	/* aux control reg. */
#define DUART_IPCR(base)	DUART_ACR(base)		/* input port change */
#define DUART_IMR(base)		SIO_ADRS(0x05,base)	/* int. mask reg. */
#define DUART_ISR(base)		DUART_IMR(base)		/* int. status reg. */
#define DUART_CTUR(base)	SIO_ADRS(0x06,base)	/* counter upper reg. */
#define DUART_CTLR(base)	SIO_ADRS(0x07,base)	/* counter lower reg. */
#define DUART_MRB(base)		SIO_ADRS(0x08,base)	/* mode reg. B */
#define DUART_CSRB(base)	SIO_ADRS(0x09,base)	/* clock sel. reg. B */
#define DUART_SRB(base)		DUART_CSRB(base)	/* status reg. B */
#define DUART_CRB(base)		SIO_ADRS(0x0a,base)	/* command reg. B */
#define DUART_THRB(base)	SIO_ADRS(0x0b,base)	/* transmit buffer B */
#define DUART_RHRB(base)	DUART_THRB(base)	/* receive buffer B */
#define DUART_IVR(base)		SIO_ADRS(0x0c,base)	/* int. vector reg. */
#define DUART_OPCR(base)	SIO_ADRS(0x0d,base)	/* output port config */
#define DUART_IP(base)		DUART_OPCR(base)	/* input port */
#define DUART_SOPBC(base)	SIO_ADRS(0x0e,base)	/* set output port */
#define DUART_CTRON(base)	DUART_SOPBC(base)	/* counter on */
#define DUART_ROPBC(base)	SIO_ADRS(0x0f,base)	/* reset output port */
#define DUART_CTROFF(base)	DUART_ROPBC(base)	/* counter off */

/* equates for mode reg. A or B, mode 1 */
#define	RX_RTS		0x80		/* 0 = no, 1 = yes */
#define	RX_INT		0x40		/* 0 = RxRDY, 1 = FFULL */
#define	ERR_MODE	0x20		/* 0 = char, 1 = block */
#define	PAR_MODE_MULTI	0x18		/* multi_drop mode */
#define	PAR_MODE_NO	0x10		/* no parity mode */
#define	PAR_MODE_FORCE	0x08		/* force parity mode */
#define	PAR_MODE_YES	0x00		/* parity mode */
#define	PARITY_TYPE	0x04		/* 0 = even, 1 = odd */
#define	BITS_CHAR_8	0x03		/* 8 bits */
#define	BITS_CHAR_7	0x02		/* 7 bits */
#define	BITS_CHAR_6	0x01		/* 6 bits */
#define	BITS_CHAR_5	0x00		/* 5 bits */
/* equates for mode reg. A or B, mode 2 */
#define	CHAN_MODE_RL	0xc0		/* remote loop */
#define	CHAN_MODE_LL	0x80		/* local loop */
#define	CHAN_MODE_AECHO	0x40		/* auto echo */
#define	CHAN_MODE_NORM	0x00		/* normal */
#define	TX_RTS		0x20		/* 0 = no, 1 = yes */
#define	CTS_ENABLE	0x10		/* 0 = no, 1 = yes */
#define	STOP_BITS_2	0x0f		/* 2 */
#define	STOP_BITS_1	0x07		/* 1 */
/* equates for clock select reg. A or B */
#define	RX_CLK_SEL	0xf0		/* receiver clock select */
#define	RX_CLK_19200	0xc0		/* 19200 */
#define	RX_CLK_9600	0xb0		/* 9600 */
#define	RX_CLK_1800	0xa0		/* 1800 */
#define	RX_CLK_4800	0x90		/* 4800 */
#define	RX_CLK_2400	0x80		/* 2400 */
#define	RX_CLK_2000	0x70		/* 2000 */
#define	RX_CLK_1200	0x60		/* 1200 */
#define	RX_CLK_600	0x50		/* 600 */
#define	RX_CLK_300	0x40		/* 300 */
#define	RX_CLK_150	0x30		/* 150 */
#define	RX_CLK_134_5	0x20		/* 134.5 */
#define	RX_CLK_110	0x10		/* 110 */
#define	RX_CLK_75	0x00		/* 75 */
#define	TX_CLK_SEL	0x0f		/* transmitter clock select */
#define	TX_CLK_19200	0x0c		/* 19200 */
#define	TX_CLK_9600	0x0b		/* 9600 */
#define	TX_CLK_1800	0x0a		/* 1800 */
#define	TX_CLK_4800	0x09		/* 4800 */
#define	TX_CLK_2400	0x08		/* 2400 */
#define	TX_CLK_2000	0x07		/* 2000 */
#define	TX_CLK_1200	0x06		/* 1200 */
#define	TX_CLK_600	0x05		/* 600 */
#define	TX_CLK_300	0x04		/* 300 */
#define	TX_CLK_150	0x03		/* 150 */
#define	TX_CLK_134_5	0x02		/* 134.5 */
#define	TX_CLK_110	0x01		/* 110 */
#define	TX_CLK_75	0x00		/* 75 */
/* equates for status reg. A or B */
#define	RXD_BREAK	0x80		/* 0 = no, 1 = yes */
#define	FRAMING_ERR	0x40		/* 0 = no, 1 = yes */
#define	PARITY_ERR	0x20		/* 0 = no, 1 = yes */
#define	OVERRUN_ERR	0x10		/* 0 = no, 1 = yes */
#define	TXENT		0x08		/* 0 = no, 1 = yes */
#define	TXRDY		0x04		/* 0 = no, 1 = yes */
#define	FFULL		0x02		/* 0 = no, 1 = yes */
#define	RXRDY		0x01		/* 0 = no, 1 = yes */
/* equates for command reg. A or B */
/* miscellaneous commands: 0x70 */
#define	STP_BREAK_CMD	0x70		/* stop break command */
#define	STR_BREAK_CMD	0x60		/* start break command */
#define	RST_BRK_INT_CMD	0x50		/* reset break int. command */
#define	RST_ERR_STS_CMD	0x40		/* reset error status command */
#define	RST_TX_CMD	0x30		/* reset transmitter command */
#define	RST_RX_CMD	0x20		/* reset receiver command */
#define	RST_MR_PTR_CMD	0x10		/* reset mr pointer command */
#define	NO_COMMAND	0x00		/* no command */
#define	TX_DISABLE	0x08		/* 0 = no, 1 = yes */
#define	TX_ENABLE	0x04		/* 0 = no, 1 = yes */
#define	RX_DISABLE	0x02		/* 0 = no, 1 = yes */
#define	RX_ENABLE	0x01		/* 0 = no, 1 = yes */
/* equates for auxiliary control reg. (timer and counter clock selects) */
#define	BRG_SELECT	0x80		/* baud rate generator select */
					/* 0 = set 1; 1 = set 2 */
					/* NOTE above equates are set 2 ONLY */
#define	TMR_EXT_CLK_16	0x70		/* external clock divided by 16 */
#define	TMR_EXT_CLK	0x60		/* external clock */
#define	TMR_IP2_16	0x50		/* ip2 divided by 16 */
#define	TMR_IP2		0x40		/* ip2 */
#define	CTR_EXT_CLK_16	0x30		/* external clock divided by 16 */
#define	CTR_TXCB	0x20		/* channel B transmitter clock */
#define	CTR_TXCA	0x10		/* channel A transmitter clock */
#define	CTR_IP2		0x00		/* ip2 */
#define	DELTA_IP3_INT	0x08		/* delta ip3 int. */
#define	DELTA_IP2_INT	0x04		/* delta ip2 int. */
#define	DELTA_IP1_INT	0x02		/* delta ip1 int. */
#define	DELTA_IP0_INT	0x01		/* delta ip0 int. */
/* equates for input port change reg. */
#define	DELTA_IP3	0x80		/* 0 = no, 1 = yes */
#define	DELTA_IP2	0x40		/* 0 = no, 1 = yes */
#define	DELTA_IP1	0x20		/* 0 = no, 1 = yes */
#define	DELTA_IP0	0x10		/* 0 = no, 1 = yes */
#define	IP3		0x08		/* 0 = low, 1 = high */
#define	IP2		0x04		/* 0 = low, 1 = high */
#define	IP1		0x02		/* 0 = low, 1 = high */
#define	IP0		0x01		/* 0 = low, 1 = high */
/* equates for int. mask reg. */
#define	INPUT_DELTA_INT	0x80		/* 0 = off, 1 = on */
#define	BREAK_B_INT	0x40		/* 0 = off, 1 = on */
#define	RX_RDY_B_INT	0x20		/* 0 = off, 1 = on */
#define	TX_RDY_B_INT	0x10		/* 0 = off, 1 = on */
#define	CTR_RDY_INT	0x08		/* 0 = off, 1 = on */
#define	BREAK_A_INT	0x04		/* 0 = off, 1 = on */
#define	RX_RDY_A_INT	0x02		/* 0 = off, 1 = on */
#define	TX_RDY_A_INT	0x01		/* 0 = off, 1 = on */
/* equates for int. status reg. */
#define	INPUT_DELTA	0x80		/* 0 = no, 1 = yes */
#define	BREAK_B		0x40		/* 0 = no, 1 = yes */
#define	RX_RDY_B	0x20		/* 0 = no, 1 = yes */
#define	TX_RDY_B	0x10		/* 0 = no, 1 = yes */
#define	CTR_RDY		0x08		/* 0 = no, 1 = yes */
#define	BREAK_A		0x04		/* 0 = no, 1 = yes */
#define	RX_RDY_A	0x02		/* 0 = no, 1 = yes */
#define	TX_RDY_A	0x01		/* 0 = no, 1 = yes */
/* equates for output port config. reg. */
#define	OP7		0x80		/* 0 = OPCR[7], 1 = TxRDYB */
#define	OP6		0x40		/* 0 = OPCR[6], 1 = TxRDYA */
#define	OP5		0x20		/* 0 = OPCR[5], 1 = RxRDYB */
#define	OP4		0x10		/* 0 = OPCR[4], 1 = RxRDYA */
#define	RXCB_1X		0x0c		/* RCXB_1X */
#define	TXCB_1X		0x08		/* TXCB_1X */
#define	C_T_OUTPUT	0x04		/* C_T_OUTPUT */
#define	OP3		0x00		/* 0 = OPCR[3] */
#define	RXCA_1X		0x03		/* RXCA_1X */
#define	TXCA_1X		0x02		/* TXCA_1X */
#define	TXCA_16X	0x01		/* TXCA_16X */
#define	OP2		0x00		/* 0 = OPCR[2] */

/* Global output port bit definitions.  OP0 and OP1 only available. */
#define CNOP_DTR_B              0x01    /* assert DTR for channel B */
#define CNOP_RTS_B              0x02    /* assert RTS for channel B */

/* Global input port bit definitions.  IP2 is the only one available. */
#define CNIP_CTS_B              0x04    /* maybe hooked to CTS or.. */
#define CNIP_CD_B               0x04    /* .. maybe to CD. */

#ifdef __cplusplus
}
#endif

#endif /* __INCs2681h */
