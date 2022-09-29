/* s2691.h Signetics S2691 uart header file */

/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,09jul92,ajm  5.1 merge
01a,11nov91,ajm  written from Omnibyte header
*/

/*
This file contains constants for the Signetics S2691 serial chip.
*/

#ifndef __INCs2691h
#define __INCs2691h

#ifdef __cplusplus
extern "C" {
#endif

#define N_CHANNELS	1

#ifndef	_ASMLANGUAGE
typedef struct
    {
    unsigned char mr12;
    unsigned char pad0[3];
    unsigned char csr;
    unsigned char pad1[3];
    unsigned char cr;
    unsigned char pad2[3];
    unsigned char datar;
    unsigned char pad3[3];
    unsigned char acr;
    unsigned char pad4[3];
    unsigned char imr;
    unsigned char pad5[3];
    unsigned char ctur;
    unsigned char pad6[3];
    unsigned char ctlr;
    unsigned char pad7[3];
    } S2691;
#endif	/* _ASMLANGUAGE */

/* equates for mode reg. mode 1 */

#define	MR1_RX_RTS		0x80		/* 0 = no, 1 = yes */
#define	MR1_RX_INT		0x40		/* 0 = RxRDY, 1 = FFULL */
#define	MR1_ERR_MODE		0x20		/* 0 = char, 1 = block */
#define	MR1_PAR_MODE_MULTI	0x18		/* multi_drop mode */
#define	MR1_PAR_MODE_NO		0x10		/* no parity mode */
#define	MR1_PAR_MODE_FORCE	0x08		/* force parity mode */
#define	MR1_PAR_MODE_YES	0x00		/* parity mode */
#define	MR1_PARITY_TYPE		0x04		/* 0 = even, 1 = odd */
#define	MR1_BITS_CHAR_8		0x03		/* 8 bits */
#define	MR1_BITS_CHAR_7		0x02		/* 7 bits */
#define	MR1_BITS_CHAR_6		0x01		/* 6 bits */
#define	MR1_BITS_CHAR_5		0x00		/* 5 bits */

/* equates for mode reg. mode 2 */

#define	MR2_CHAN_MODE_RL	0xc0		/* remote loop */
#define	MR2_CHAN_MODE_LL	0x80		/* local loop */
#define	MR2_CHAN_MODE_AECHO	0x40		/* auto echo */
#define	MR2_CHAN_MODE_NORM	0x00		/* normal */
#define	MR2_TX_RTS		0x20		/* 0 = no, 1 = yes */
#define	MR2_CTS_ENABLE		0x10		/* 0 = no, 1 = yes */
#define	MR2_STOP_BITS_2		0x0f		/* 2 */
#define	MR2_STOP_BITS_1		0x07		/* 1 */

/* equates for clock select reg. */

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

/* equates for status reg. */

#define	SR_RXD_BREAK	0x80		/* 0 = no, 1 = yes */
#define	SR_FRAMING_ERR	0x40		/* 0 = no, 1 = yes */
#define	SR_PARITY_ERR	0x20		/* 0 = no, 1 = yes */
#define	SR_OVERRUN_ERR	0x10		/* 0 = no, 1 = yes */
#define	SR_TXENT	0x08		/* 0 = no, 1 = yes */
#define	SR_TXRDY	0x04		/* 0 = no, 1 = yes */
#define	SR_FFULL	0x02		/* 0 = no, 1 = yes */
#define	SR_RXRDY	0x01		/* 0 = no, 1 = yes */

/* equates for command reg. */
/* miscellaneous commands: 0x70 */

#define CMD_RST_MPI_INT 0xc0            /* Reset MPI change interrupt */
#define CMD_NEG_RTS     0xb0            /* Negate RTS command */
#define CMD_ASRT_RTS    0xa0            /* Assert RTS command */
#define CMD_STP_CT      0x90            /* Stop Counter command */
#define CMD_STR_CT      0x80            /* Start Counter command */
#define	CMD_STP_BREAK	0x70		/* stop break command */
#define	CMD_STR_BREAK	0x60		/* start break command */
#define	CMD_RST_BRK_INT	0x50		/* reset break int. command */
#define	CMD_RST_ERR_STS	0x40		/* reset error status command */
#define	CMD_RST_TX	0x30		/* reset transmitter command */
#define	CMD_RST_RX	0x20		/* reset receiver command */
#define	CMD_RST_MR_PTR	0x10		/* reset mr pointer command */
#define	CMD_NO_COMMAND	0x00		/* no command */
#define	CMD_TX_DISABLE	0x08		/* 0 = no, 1 = yes */
#define	CMD_TX_ENABLE	0x04		/* 0 = no, 1 = yes */
#define	CMD_RX_DISABLE	0x02		/* 0 = no, 1 = yes */
#define	CMD_RX_ENABLE	0x01		/* 0 = no, 1 = yes */

/* equates for auxiliary control reg. (timer and counter clock selects) */

#define	ACR_BRG_SELECT		0x80	/* baud rate generator select */
					/* 0 = set 1; 1 = set 2 */
					/* NOTE above equates are set 2 ONLY */
#define	ACR_TMR_EXT_CLK_16	0x70	/* external clock divided by 16 */
#define	ACR_TMR_EXT_CLK		0x60	/* Timer mode ; external clock */
#define	ACR_TMR_IP2_16		0x50	/* Timer mode ; mpi divided by 16 */
#define	ACR_TMR_IP2		0x40	/* Timer mode ; MPI pin */
#define	ACR_CTR_EXT_CLK_16	0x30	/* external clock divided by 16 */
#define	ACR_CTR_TXC 		0x20	/* Counter mode ; transmitter clock */
#define	ACR_CTR_IP_16		0x10	/* Counter mode ; mpi pin div by 16*/
#define ACR_PWR_UP          	0x08    /* 0 = power-down mode */
#define ACR_MP0_RXRDY       	0x07    /* MP0 pin = RxRDY / Fifo FULL */
#define ACR_MP0_TXRDY       	0x06    /* MP0 pin TxRdy */
#define ACR_MP0_RXCLKX16    	0x05    /* MP0 RxCLK X 16 */
#define ACR_MP0_RXCLKX1     	0x04    /* MP0 RxClk X 1 */
#define ACR_MP0_TXCLKX16    	0x03    /* MP0 Tx Clk X 16 */
#define ACR_MP0_TXCLKX1     	0x02    /* MP0 Tx Clk X 1 */
#define ACR_MP0_CTOUT       	0x01    /* MP0 Counter / Timer Output */
#define ACR_MP0_RTSN        	0x00    /* MP0 RTS control */
#define	ACR_CTR_IP 		0x00	/* Counter mode ; mpi pin */

/* equates for int. mask reg. */

#define	IM_MPI_CHANGE	0x80		/* 0 = off, 1 = on */
#define	IM_MPI_LEVEL	0x40		/* 0 = off, 1 = on */
#define	IM_CTR_RDY	0x10		/* 0 = off, 1 = on */
#define	IM_BREAK	0x08		/* 0 = off, 1 = on */
#define	IM_RX_RDY	0x04		/* 0 = off, 1 = on */
#define	IM_TX_MT	0x02		/* 0 = off, 1 = on */
#define	IM_TX_RDY	0x01		/* 0 = off, 1 = on */
#define	IM_MSK_ALL	0xff		/* 0 = off, 1 = on */

/* equates for int. status reg. */

#define	IS_INPUT_DELTA	0x80		/* 0 = no, 1 = yes */
#define	IS_INPUT_STATE	0x40		/* 0 = no, 1 = yes */
#define	IS_CTR_RDY	0x10		/* 0 = no, 1 = yes */
#define	IS_BREAK	0x08		/* 0 = no, 1 = yes */
#define	IS_RX_RDY  	0x04		/* 0 = no, 1 = yes */
#define	IS_TX_MT   	0x02		/* 0 = no, 1 = yes */
#define	IS_TX_RDY  	0x01		/* 0 = no, 1 = yes */

#ifdef __cplusplus
}
#endif

#endif /* __INCs2691h */
