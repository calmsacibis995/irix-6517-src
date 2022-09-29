/* i534.h - Header file for the intel 534 serial board. */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01j,22sep92,rrr  added support for c++
01i,26may92,rrr  the tree shuffle
01h,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01g,05oct90,shl  added copyright notice.
01f,08jan87,jlf  added ifndef's to keep from being included twice.
01e,23jul84,ecs  Brought definition of IO_534 back here from hwConfig.h.
01d,02aug83,dnw  Removed I534_BASE, now IO_534 in ioLib.h.
		 Added PIC poll status definitions.
01c,16may83,jlf	 Put parens around address definitions.
		 Coerced I534_BASE to type (char *).
01b,29mar83,jlf	 Moved Buffer-size parameter to ty534Drv.c
01a,10mar83,jlf	 written
*/

#ifndef __INCi534h
#define __INCi534h

#ifdef __cplusplus
extern "C" {
#endif

/* Strapable parameters */

#define PIT_CLOCK_RATE	1228800		/* 1.2288 MHz into the PIT */

#define	N_534_CHANNELS	4		/* Number of serial channels on bd */

/* The following addresses can only be accessed with the data block
   selected */

#define IO_534	((char *) 0xef0020)	/* 0x10 bytes */
#define I534_U0_D	(IO_534)	/* UART 0 data */
#define I534_U0_S	(IO_534 + 1)	/* UART 0 status and command */
#define I534_U1_D	(IO_534 + 2)	/* UART 1 data */
#define I534_U1_S	(IO_534 + 3)	/* UART 1 status and command */
#define I534_U2_D	(IO_534 + 4)	/* UART 2 data */
#define I534_U2_S	(IO_534 + 5)	/* UART 2 status and command */
#define I534_U3_D	(IO_534 + 6)	/* UART 3 data */
#define I534_U3_S	(IO_534 + 7)	/* UART 3 status and command */
#define I534_PIC0_B0	(IO_534 + 8)	/* 8259, byte 0 - write ICW1, OCW2, */
					/*		  OCW3, read status */
#define I534_PIC0_B1	(IO_534 + 9)	/* 8259, byte 1 - write ICW2, OCW1, */
					/*		  read OCW1	    */
#define I534_PIC1_B0	(IO_534 + 0xa)	/* Unused 8259, byte 0 */
#define I534_PIC1_B1	(IO_534 + 0xb)	/* Unused 8259, byte 1 */

/* The following can only be accessed with the control block selected */

#define I534_PIT0_0	(IO_534)	/* 8253 PIT 0, counter 0 */
#define I534_PIT0_1	(IO_534 + 1)	/* 8253 PIT 0, counter 1 */
#define I534_PIT0_2	(IO_534 + 2)	/* 8253 PIT 0, counter 2 */
#define I534_PIT0_C	(IO_534 + 3)	/* 8253 PIT 0, control */
#define I534_PIT1_0	(IO_534 + 4)	/* 8253 PIT 1, counter 0 */
#define I534_PIT1_1	(IO_534 + 5)	/* 8253 PIT 1, counter 1 */
#define I534_PIT1_2	(IO_534 + 6)	/* 8253 PIT 1, counter 2 */
#define I534_PIT1_C	(IO_534 + 7)	/* 8253 PIT 1, control */
#define I534_PPI_A	(IO_534 + 8)	/* 8255 PPI, Port A in */
#define I534_PPI_B	(IO_534 + 9)	/* PPI, Port B in */
#define I534_PPI_C	(IO_534 + 0xa)	/* PPI, Port C out and stat */
#define I534_PPI_CTL	(IO_534 + 0xb)	/* PPI control */

/* The following can be accessed with either block selected */

#define I534_SEL_C	(IO_534 + 0xc)	/* Select Control Block */
#define I534_SEL_D	(IO_534 + 0xd)	/* Select Data Block */
#define I534_TEST	(IO_534 + 0xe)	/* Select test mode */
#define I534_RESET	(IO_534 + 0xf)	/* Board Reset */

/* USART Mode Word bits */

#define U_1_STOP	0x40
#define U_2_STOP	0xc0
#define U_PARITY_EVEN	0x20
#define U_PARITY_ENABLE	0x10
#define U_7_DBITS	0x08
#define U_8_DBITS	0x0c
#define U_X16		0x02
#define U_X1		0x01

/* USART Command Word bits */

#define U_RESET		0x40		/* 1 returns 8251 to mode word */
#define U_RTS		0x20		/* 1 forces RTS/ output to 0 */
#define U_ERR_RESET	0x10		/* Reset all error flags */
#define U_BREAK		0x08		/* 1 forces TxD low */
#define U_RX_ENABLE	0x04
#define U_DTR		0x02		/* 1 forces DTR/ output to 0 */
#define U_TX_ENABLE	0x01

/* USART Status Word bits */

#define U_DSR		0x80
#define U_FRAME_ERR	0x20
#define U_OVE_ERR	0x10
#define U_PARITY_ERR	0x08
#define U_TX_EMPTY	0x04
#define U_RX_READY	0x02
#define U_TX_READY	0x01

/* PIT Mode Word bits */

#define PIT_SEL_C_0	0x00
#define PIT_SEL_C_1	0x40
#define PIT_SEL_C_2	0x80
#define PIT_BOTH_BYTES	0x30
#define PIT_MODE_3	0x06

/* PIC ICW1 */

#define PIC_ICW1_ID	0x10		/* Must be used to identify ICW1 */
#define PIC_ADR_7_5	0xe0		/* Mask for bits 7-5 of lower */
					/* routine address */
#define PIC_INTERVAL_4	0x04
#define PIC_INTERVAL_8	0x00
#define PIC_SINGLE	0x02

/* PIC OCW2 */

#define PIC_ROT_PRI	0x80		/* Rotate priority */
#define PIC_SEOI	0x60		/* Specific end of interrupt */
#define PIC_EOI		0x20		/* Non-specific End of int */

/* PIC OCW3 */

#define PIC_OCW3_ID	0x08		/* Must be set to identify OCW3 */
#define PIC_SET_SMASK	0x60		/* Set special mask */
#define PIC_RST_SMASK	0x40		/* Reset special mask */
#define PIC_POLLING	0x04
#define PIC_READ_IR	0x02
#define PIC_READ_IS	0x03

/* PIC Poll status */

#define PIC_IL_REQUEST	0x80		/* interrupt requesting */
#define PIC_IL_MASK	0x07		/* interrupt level mask */
#define PIC_IL_CHAN0	0x00		/* channel 0 interrupt level */
#define PIC_IL_CHAN1	0x02		/* channel 1 interrupt level */
#define PIC_IL_CHAN2	0x04		/* channel 2 interrupt level */
#define PIC_IL_CHAN3	0x06		/* channel 3 interrupt level */
#define PIC_IL_TX	0x01		/* transmitter buffer empty interrupt */

/* Default baud rates for the 4 USARTs. */

#define BAUD_0		9600
#define BAUD_1		9600
#define BAUD_2		9600
#define BAUD_3		9600

#ifdef __cplusplus
}
#endif

#endif /* __INCi534h */
