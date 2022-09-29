#ident	"$Id: duart.h,v 1.2 1995/10/15 18:22:28 benf Exp $"
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


/*
 *  DUART  -  Signetics SCN2681
 *
 *  DUART registers offset independent of machine type
 */

/*------------------------------------------------------------------------+
|  (r/w) == read/write register                                           |
|  (r) == read-only register                                              |
|  (w) == write-only register                                             |
+------------------------------------------------------------------------*/
#define DOFF_MR		0x00		/* channel mode reg (1 or 2)     */
#define D_MRA		DOFF_MR		/* Mode Register A (r/w)         */
#define DOFF_MRA	DOFF_MR		/* channel mode reg (1 or 2)     */

#define DOFF_SR		0x04		/* channel status reg (read)     */
#define D_SRA		DOFF_SR		/* Status Register A (r)         */
#define DOFF_SRA	DOFF_SR		/* channel status reg (read)     */

#define	DOFF_CSR	0x04		/* Clock Select Register (A/B)   */
#define D_CSRA		DOFF_CSR	/* Clock Select Reg. A (w)       */
#define	DOFF_CSRA	DOFF_CSR	/* channel A Clock Select Reg    */

#define DOFF_CR		0x08		/* generic channel command reg   */
#define D_CRA		DOFF_CR		/* Command Register A (w)        */
#define DOFF_CRA	DOFF_CR		/* channel command register      */

#define DOFF_RHR	0x0c		/* channel recv/hold reg (read)  */
#define D_RHRA		DOFF_RHR	/* Rx Holding Register A (r)     */
#define DOFF_RHRA	DOFF_RHR	/* channel recv/hold reg (read)  */

#define	DOFF_THR	0x0c		/* tX Holding Register (A/B)     */
#define D_THRA		DOFF_THR	/* Tx Holdint Register A (w)     */
#define	DOFF_THRA	DOFF_RHR	/* tX Holding Register (A/B)     */

#define D_IPCR		0x10		/* Input Port Change Reg (r)     */
#define DOFF_IPCR	D_IPCR		/* input port change register    */

#define D_ACR		0x10		/* Aux. Control Reg. (w)         */
#define	DOFF_ACR	DOFF_IPCR	/* Aux Control Register          */

#define DOFF_ISR	0x14		/* interrupt status register     */
#define D_ISR		DOFF_ISR	/* Interrupt Status Reg. (r)     */
#define	DOFF_IMR	0x14		/* Interrupt Mask Register       */
#define D_IMR		DOFF_IMR	/* Interrupt Mask Reg. (w)       */

#define DOFF_CTU	0x18		/* counter/timer upper           */
#define D_CTU		DOFF_CTU	/* Counter/Timer Upper (r/w)     */
#define	DOFF_CTUR	DOFF_CTU	/* C/T Upper Register            */

#define DOFF_CTL	0x1c		/* counter/timer lower           */
#define D_CTL		DOFF_CTL	/* Counter/Timer Lower (r/w)     */
#define	DOFF_CTLR	DOFF_CTL	/* C/T Lower Register            */

#define DOFF_MRB	0x20		/* channel B mode reg (1 or 2)   */
#define D_MRB		DOFF_MRB	/* Mode Register B (r/w)         */

#define DOFF_SRB	0x24		/* channel B status reg (read)   */
#define D_SRB		DOFF_SRB	/* Status Register B (r)         */

#define	DOFF_CSRB	0x24		/* channel B Clock Select Reg    */
#define D_CSRB		DOFF_CSRB	/* Clock Select Reg. B (w)       */

#define DOFF_CRB	0x28		/* channel B command register    */
#define D_CRB		DOFF_CRB	/* Command Register B (w)        */

#define DOFF_RHRB	0x2c		/* channel B recv/hold reg (read)*/
#define D_RHRB		DOFF_RHRB	/* Rx Holding Register B (r)     */

#define DOFF_THRB	0x2c		/* channel B recv/hold reg (read)*/
#define D_THRB		DOFF_THRB	/* Tx Holding Register B (w)     */

#define DOFF_IPORT	0x34		/* input port                    */
#define D_IPORT		DOFF_IPORT	/* Input Port Register (r)       */

#define	DOFF_OPCR	0x34		/* Output Port Configuration Reg */
#define D_OPCR		DOFF_OPCR	/* Output Port Config. Reg. (w)  */

#define DOFF_CCGO	0x38		/* start counter command         */
#define D_CCGO		DOFF_CCGO	/* Start Counter Command (r)     */

#define	DOFF_SOPBC	0x38		/* Set Output Port Bits Command  */
#define D_SOPBC		DOFF_SOPBC	/* Set Output Port Bits Command  */

#define DOFF_CCSTP	0x3c		/* stop counter command          */
#define D_CCSTP		DOFF_CCSTP	/* Stop Counter Command (r)      */

#define	DOFF_ROPBC	0x3c		/* Reset Output Port Bits Cmd    */
#define D_ROPBC		DOFF_ROPBC	/* Reset Output Port Bits Cmd (w)*/


/*------------------------------------------------------------------------+
| Bit values for interrupt status register (d_isr)                        |
+------------------------------------------------------------------------*/
#define	IPC		0x80	/* inport status change */
#define	B_BREAK		0x40	/* change in channel b break state */
#define	B_RXRDY		0x20	/* channel b receiver ready */
#define	B_TXRDY		0x10	/* channel b transmitter ready */
#define	CTR_RDY		0x08	/* counter done */
#define	A_BREAK		0x04	/* change in channel a break state */
#define	A_RXRDY		0x02	/* channel a receiver ready */
#define	A_TXRDY		0x01	/* channel a transmitter ready */


/*------------------------------------------------------------------------+
|  Interrupt Mask Register bits control whether the setting of the  cor-  |
|  responding bit in  the ISR generates an interrupt.  If the bit in the  |
|  IMR is reset (off), then no interrupt is generated.                    |
+------------------------------------------------------------------------*/
#define	DNOINT		0x00	/* IMR value to disable interrupts */


/*------------------------------------------------------------------------+
| Status register bits                                                    |
+------------------------------------------------------------------------*/
#define	SRERRORS	0xf0	/* some kind of error occurred */
#define	SRBREAK		0x80	/* received break on this char */
#define	SRFRM		0x40	/* framing error on receive */
#define	SRPAR		0x20	/* parity error on receive */
#define	SROVR		0x10	/* overflow on receive */
#define	SREMT		0x08	/* all chars sent */
#define	SRTXRDY		0x04	/* can take char to send */
#define	SRFULL		0x02	/* fifo is full */
#define	SRRXRDY		0x01	/* char available */


/*------------------------------------------------------------------------+
| Commands for command register                                           |
+------------------------------------------------------------------------*/
#define	STOP_BRK	0x70	/* stop break output on channel */
#define	START_BRK	0x60	/* start break output on channel */
#define	RES_BRK		0x50	/* reset break indication on channel */
#define	RES_ERR		0x40	/* reset error flags in status register */
#define	RES_TX		0x30	/* reset transmitter */
#define	RES_RX		0x20	/* reset receiver */
#define	SEL_MR1		0x10	/* select mr1 register for next mr action */
#define	DIS_TX		0x08	/* disable transmitter */
#define	EN_TX		0x04	/* enable transmitter */
#define	DIS_RX		0x02	/* disable receiver */
#define	EN_RX		0x01	/* enable receiver */


/*------------------------------------------------------------------------+
| Input port change register bits                                         |
+------------------------------------------------------------------------*/
#define	DIP3		0x80	/* delta input 3 */
#define	DIP2		0x40	/* delta input 2 */
#define	DIP1		0x20	/* delta input 1 */
#define	DIP0		0x10	/* delta input 0 */
#define	IP5		0x20	/* input 5 */
#define	IP4		0x10	/* input 4 */
#define	IP3		0x08	/* input 3 */
#define	IP2		0x04	/* input 2 */
#define	IP1		0x02	/* input 1 */
#define	IP0		0x01	/* input 0 */


/*------------------------------------------------------------------------+
| Clock select register                                                   |
+------------------------------------------------------------------------*/
#define	b110		0x11	/* 110 baud */
#define	b1200		0x66	/* 1200 baud */
#define	b134		0x22	/* 134 baud */
#define	b150		0x33	/* 150 baud */
#define	b1800		0xaa	/* 1800 baud */
#define	b19200		0xcc	/* 19200 baud */
#define	b2400		0x88	/* 2400 baud */
#define	b300		0x44	/* 300 baud */
#define	b4800		0x99	/* 4800 baud */
#define	b600		0x55	/* 600 baud */
#define	b75		0x00	/* 75 baud */
#define	b9600		0xbb	/* 9600 baud */


/*------------------------------------------------------------------------+
| Auxillary control register bits                                         |
+------------------------------------------------------------------------*/
#define	SET2		0x80	/* Enable baud rate set 2 */


/*------------------------------------------------------------------------+
| Auxillary control register bits                                         |
+------------------------------------------------------------------------*/
#define	stop2		0x0f	/* 2 stop bits */
#define	stop1p5		0x08	/* 1.5 stop bits */
#define	stop1		0x07	/* 1 stop bit */
