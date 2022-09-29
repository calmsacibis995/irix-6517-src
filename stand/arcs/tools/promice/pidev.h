/*	PiDev.h - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - definitions for serial or parallel port operation
*/

#ifdef MAC						/* MAC-II with parallel port card */
#define	unigatePP	276		/* unigate board id number */
#define	PP_BUSY		(0x08)	/* busy bit on parallel port */
#define	PP_PAPER	(0X01)	/* busy bit on second parallel port */
#define	PP_STRON	(0x0e)	/* strobe bit on for parallel port */
#define	PP_STROFF	(0x0f)	/* strobe bit off for parallel port */
#define	PP_AUTOON	(0x00)	/* strobe bit on for second parallel port pin 14 */
#define	PP_AUTOOFF	(0x01)	/* strobe bit off for second parallel port */
#define	PP_BACK		(0x02)	/* back channel ack signal pin 17*/
#define	PP_BACKOFF	(0x03)	/* back channel ack signal*/
#define	PP_INITS	(0x09)	/* init signal for pp */
#define	PP_INITC	(0x08)	/* remove init */

/* translation tables for data from the parallel port */

unsigned char PIPT1[32] = {
	0x00,0x04,0x02,0x06,0x01,0x05,0x03,0x07, 
	0x00,0x04,0x02,0x06,0x01,0x05,0x03,0x07,
	0x08,0x0c,0x0a,0x0e,0x09,0x0d,0x0b,0x0f, 
	0x08,0x0c,0x0a,0x0e,0x09,0x0d,0x0b,0x0f
	};
#endif

#ifdef MSDOS					/* for MSDOS - IBM/PC systems */
#define	BUSY    (0x80)          /* busy bit on parallel port */
#define	PAPER   (0X20)          /* busy bit on second parallel port */
#define	ACK     (0x40)          /* ack bit on parallel port */
#define	STRON	(0x05)			/* strobe bit on for parallel port */
#define	STROFF	(0x04)			/* strobe bit off for parallel port */
#define	AUTOON  (0x06)          /* strobe bit on for second parallel port */
#define	AUTOOFF (0x04)          /* strobe bit off for second parallel port */
#define	B_ACK	(0x0C)			/* back channel ack signal*/
#define	PP_INITS	(0x00)			/* init signal for pp */
#define	PP_INITC	(0x04)			/* remove init signal */
#define	TXBUF	(pxlink.saddr)		/* transmitt buffer */
#define	RXBUF	(pxlink.saddr)		/* receive buffer */
#define	DLLSB	(pxlink.saddr)		/* BRG divisor low */
#define	DLMSB	(pxlink.saddr+1)	/* BRG divisor high */
#define	IEREG	(pxlink.saddr+1)	/* interrupt reg */
#define	LCREG	(pxlink.saddr+3)	/* line control reg */
#define	LC1STP	3					/* 8 data bits 1 stop bit */
#define	LCBRK	0x40				/* do a break */
#define	LC2STP	7					/* 8 data bits 2 stop bits */
#define	MCREG	(pxlink.saddr+4)	/* modem control reg */
#define	LSREG	(pxlink.saddr+5)	/* line status reg */
#define	CDIV	(115200/pxlink.brate) /* divisor for BRG */
#define	MCRSTS	(0x9)				/* assert reset to unit */
#define	MCRSTC	(0x8)				/* remove reset to unit */
#define	TSRMT	0x40				/* shift reg is mt */
#define	BUFMT	0x20				/* TBMT bit in LSREG */
#define	RDA		0x01				/* RDA bit in LSREG */
#define	OVR		0x02				/* data overrun */
#endif

#ifdef	UNIX
#define	BUSY    (0x80)          /* busy bit on parallel port */
#define	PAPER   (0X20)          /* busy bit on second parallel port */
#define	ACK     (0x40)          /* ack bit on parallel port */
#define	STRON	(0x0D)			/* strobe bit on for parallel port */
#define	STROFF	(0x0C)			/* strobe bit off for parallel port */
#define	AUTOON  (0x06)          /* strobe bit on for second parallel port */
#define	AUTOOFF (0x04)          /* strobe bit off for second parallel port */
#define	B_ACK	(0x04)			/* back channel ack signal*/
#define	PP_STS  1               /* status register */
#define	PP_CTL  2               /* printer control register */
#define	PP_DAT  0               /* data register */
#define	PP_INITS	(0x08)		/* init signal for pp */
#define	PP_INITC	(0x0C)		/* remove init signal */
#endif
