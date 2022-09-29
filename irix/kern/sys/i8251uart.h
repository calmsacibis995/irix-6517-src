/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef _SYS_i8251UART_H_
#define _SYS_i8251UART_H_

#ident "$Revision: 1.2 $"

/*									
 *	Contains the definitions of the bits used by the Intel 8251
 *	USART. 
 */

/*
 * 	Mode Selection bits.  The following bits are used
 * 	to build up the mode character for the USART.  The 
 *	USART can run in either synchronously (I8251_SYNCMODE)
 * 	or asynchronously (if one of the I8251_ASYNCxxx modes are
 *	selected).
 */
#define I8251_SYNCMODE	0x00	/* Run in synchronous mode */
#define I8251_ASYNC1X	0x01	/* Run in Async mode at baud rate of 1x clk */
#define I8251_ASYNC16X	0x02	/* Run in Async mode at baud rate of 16x clk */
#define I8251_ASYNC64X	0x03	/* Run in Async mode at baud rate of 64x clk */

/* Parity selection bits.  These are applicable to both sync and async mode */
#define I8251_EVENPAR	0x30	/* Use even parity in Async mode */
#define I8251_ODDPAR	0x10	/* Use odd parity in Async mode */
#define I8251_NOPAR	0x00	/* Don't check parity at all */	

/* Character length bits. Applicable to both sync and async modes */
#define I8251_5BITS	0x00	/* Character length = 5 bits */
#define I8251_6BITS	0x04	/* Character length = 6 bits */
#define I8251_7BITS	0x08	/* Character length = 7 bits */
#define I8251_8BITS	0x0c	/* Character length = 8 bits */

/* Asynchronous mode only mode flags. Use one of these when in ASYNC mode */
#define I8251_STOPB1	0x40	/* 1 Stop Bit in Async mode */
#define I8251_STOPB1X	0x80	/* 1X Stop bits in Async mode */
#define I8251_STOPB2	0xc0	/* 2 stop bits in Async mode */

/* Synchronous Mode only definitions. Select SYNDET and SYNC type. */
#define I8251_SYNDETIN	0x40	/* Extern Sync detect is an input */
#define	I8251_SYNDETOUT	0x00	/* Extern Sync detect is an output */

#define I8251_SYNC1	0x80	/* Single Sync character */
#define I8251_SYNC2	0x00	/* Double Sync character */


/*
 * 	Command register control values.
 * 	The following bits are OR'ed together to form the Command
 * 	character value.  This value can be written into the USART's
 * 	command register to alter its configuration.
 */
#define I8251_TXENB	0x01	/* Enable transmission of characters */
#define I8251_DTR	0x02	/* Switch on Data Terminal Ready signal */
#define I8251_RXENB	0x04	/* Enable reception of characters */
#define I8251_SBRK	0x08	/* Send a break char. in Async mode */
#define I8251_RESETERR	0x10	/* Reset the error bits in status reg */
#define I8251_RTS	0x20	/* Turn on Request To Send signal */
#define I8251_RESET	0x40	/* Reset the UART */
#define I8251_HUNT	0x80	/* Enable search for sync characters */


/*
 * 	Status register bit definitions.
 * 	By ANDING the following with the value read from the command register,
 * 	one can determine the current state of the USART.
 */
#define I8251_TXRDY	0x01	/* Bit indicating a readiness to transmit */
#define I8251_RXRDY	0x02	/* Bit indicating a byte has been received */
#define I8251_TXEMPTY	0x04	/* Bit indicating empty transmit buffer */
#define I8251_PE	0x08	/* Set when a Parity error is detected */
#define I8251_OE	0x10	/* Set when UART overruns and loses data */
#define I8251_FE	0x20	/* Set when a framing error occurs */
#define I8251_SYNDET	0x40	/* In Sync mode, set when sync byte(s) found */	
#define I8251_BRKDET	0x40	/* In Async mode, set when break asserted */
#define I8251_DSR	0x80	/* Indicates DSR has been asserted */ 


#endif /* _SYS_i8251UART_H_ */
