/* wd8250.h - Western Digital uart header */

/*
modification history
--------------------
01f,28sep92,caf  changed "recieve" to "receive", fixed #else and #endif.
		 added TY_CO_DEV and function declarations for 5.1 upgrade.
		 for 5.0.x compatibility, define INCLUDE_TY_CO_DRV_50 when
		 including this header.
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,30aug90,ajm  added mask field to IID structure
01a,20apr90,ajm  written.
*/

/*
 * define INCLUDE_TY_CO_DRV_50 when using this header if you are using
 * a VxWorks 5.0-style serial driver (tyCoDrv.c).
 */

/*
 * Templates onto the IO space of the WD8250 uart.
 * See the Western Digital data book for information.
 */

/*
 * The per channel IO template.
 */

#ifndef	__INCwd8250h
#define	__INCwd8250h

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#ifndef	_ASMLANGUAGE
#ifndef	INCLUDE_TY_CO_DRV_50

#include "tylib.h"

typedef struct		/* TY_CO_DEV */
    {
    TY_DEV tyDev;
    int numChannels;	/* number of channels to support */
    int stateChange;	/* count of state change errors */
    int lastState;	/* last stateChange error */
    int breakError;	/* count of break or serialization errors */
    int lastBreak;	/* last breakError */
    BOOL created;	/* true if this device has really been created */
    volatile unsigned char *receive_trans;
    volatile unsigned char *int_enable;
    volatile unsigned char *int_id;
    volatile unsigned char *line_ctl;
    volatile unsigned char *modem_ctl;
    volatile unsigned char *line_stat;
    volatile unsigned char *modem_stat;
    volatile unsigned char *reserved;
    } TY_CO_DEV;

#endif	/* INCLUDE_TY_CO_DRV_50 */
#endif	/* _ASMLANGUAGE */

#define PAD 3
#define N_CHANNELS	1	/* number of channels per chip */

#ifndef	_ASMLANGUAGE
typedef struct		/* UART_CHANNEL */
    {
    unsigned char pad0[PAD]; unsigned char	receive_trans;
    unsigned char pad1[PAD]; unsigned char	int_enable;
    unsigned char pad2[PAD]; unsigned char	int_id;
    unsigned char pad3[PAD]; unsigned char	line_ctl;
    unsigned char pad4[PAD]; unsigned char	modem_ctl;
    unsigned char pad5[PAD]; unsigned char	line_stat;
    unsigned char pad6[PAD]; unsigned char	modem_stat;
    unsigned char pad7[PAD]; unsigned char	reserved;
    } UART_CHANNEL;

#endif	/* _ASMLANGUAGE */

/*
 *	Definitions for Western Digital 8250 Uart use on Star Card
 */

/*
 *	Line control register defs
 */

#define LC_LENGTH_8 	0x3		/* 8 bit io */
#define LC_LENGTH_7 	0x2		/* 7 bit io */
#define LC_LENGTH_6 	0x1		/* 6 bit io */
#define LC_LENGTH_5 	0x0		/* 5 bit io */
#define LC_STOP_2 	0x4		/* 2 stop bits gened for 6..8 length */
#define LC_STOP_1 	0x0		/* 1 stop bit generated */
#define LC_PARITY_ON	0x8		/* Parity enable  */
#define LC_PARITY_OFF	0x0		/* Parity disable */
#define LC_PARITY_EVEN	0x10		/* Even Parity */
#define LC_PARITY_ODD	0x00		/* Odd Parity  */
#define LC_STK_PRTY_ON	0x20		/* Detect in opp state */
#define LC_STK_PRTY_OFF	0x00		/* Don't Detect in opp state */
#define LC_BRK_CNTL	0x40		/* Break control is set */
#define LC_NO_BRK_CNTL	0x00		/* Break control is off */
#define LC_DLAB_ENABLE	0x80		/* Divisor Latch enable */
#define LC_DLAB_DISABLE	0x00		/* Divisor Latch disable */

/*
 *	Line status register defs
 */

#define LS_DATA_RDY	0x1		/* Recieved data ready */
#define LS_OVFL_ERR	0x2		/* Overflow error */
#define LS_PRTY_ERR	0x4		/* Parity error  */
#define LS_FRAM_ERR	0x8		/* Framing error */
#define LS_BRK_INT	0x10		/* Break interrupt */
#define LS_THRE		0x20		/* Transmitt holding reg empty */
#define LS_TSRE		0x40		/* Transmitt shift reg empty */

#define LS_ERR	(LS_PRTY_ERR|LS_OVFL_ERR|LS_FRAM_ERR)
#define LS_CLR	0x00

/*
 *	Interrupt Enable register defs
 */

#define IE_DATA_RDY	0x1		/* Interrupt on Data Ready */
#define IE_THRE_EMPTY	0x2		/* Interrupt on TH register empty */
#define IE_RCV_LS	0x4		/* Interrupt on rcv line status   */
#define IE_MODEM_STAT	0x8		/* Interrupt on modem status */

#define MASK_UART_INTS 	0x00		/* Rom monitor does not use ints */

/*
 *	Interrupt ID register defs
 */

#define IID_NONE_PEND	0x1		/* No interrupt pending */
#define IID_RCV_LS	0x6		/* Recieve line stat int */
#define IID_DATA_RDY	0x4		/* Data Ready int */
#define IID_THRE	0x2		/* TH register empty int */
#define IID_MODEM_STAT	0x0		/* Modem status int */
#define IID_MASK	(IID_NONE_PEND|IID_RCV_LS|IID_DATA_RDY|IID_THRE|IID_MODEM_STAT)

/*
 *	Modem status register defs
 */

#define MS_DCTS		0x1		/* Delta clear to send */
#define MS_DDSR		0x2		/* Delta data set ready */
#define MS_TERI		0x4		/* Trailing edge ring indicator */
#define MS_DRLSD	0x8		/* Delta received line signal detected*/
#define MS_CTS		0x10		/* Clear to send */
#define MS_DSR		0x20		/* Data set ready */
#define MS_RI		0x40		/* Ring indicator */
#define MS_RLSD		0x80		/* Recieved line signal detect */

/*
 *	Modem Control register defs
 */

#define MC_DTR		0x1		/* Data terminal ready */
#define MC_RTS		0x2		/* Request to send     */
#define MC_OUT1		0x4		/* */
#define MC_OUT2		0x8		/* */
#define MC_LOOPBACK	0x10 		/* Diagnostic Loopback */

/* equates for Western Digital baud rates		*/

#define WD_BAUD_50		2304	/* transmit speed */
#define WD_BAUD_75		1536	/* transmit speed */
#define WD_BAUD_110		1074	/* transmit speed */
#define WD_BAUD_134_5		857	/* transmit speed */
#define WD_BAUD_150		768	/* transmit speed */
#define WD_BAUD_300		384	/* transmit speed */
#define WD_BAUD_600		192	/* transmit speed */
#define WD_BAUD_1200		96	/* transmit speed */
#define WD_BAUD_1800		64	/* transmit speed */
#define WD_BAUD_2000		58	/* transmit speed */
#define WD_BAUD_2400		48	/* transmit speed */
#define WD_BAUD_3600		32	/* transmit speed */
#define WD_BAUD_4800		24	/* transmit speed */
#define WD_BAUD_7200		16	/* transmit speed */
#define WD_BAUD_9600		12	/* transmit speed */
#define WD_BAUD_19200		6	/* transmit speed */
#define WD_BAUD_38400		3	/* transmit speed */
#define WD_BAUD_56000		2	/* transmit speed */

/* function declarations */

#ifndef	INCLUDE_TY_CO_DRV_50
#ifndef	_ASMLANGUAGE
#if	defined(__STDC__) || defined(__cplusplus)

IMPORT	void	tyCoInt (FAST TY_CO_DEV * pTyCoDv);

#else	/* __STDC__ */

IMPORT	void    tyCoInt ();

#endif	/* __STDC__ */
#endif	/* _ASMLANGUAGE */
#endif	/* INCLUDE_TY_CO_DRV_50 */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* __INCwd8250h */
