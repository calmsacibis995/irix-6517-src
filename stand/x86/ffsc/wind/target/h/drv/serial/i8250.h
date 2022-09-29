/* i8250.h - Intel 8250 UART (Universal Asynchronous Receiver Transmitter) */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,17feb94,hdn	 added two new members to structure TY_CO_DEV.
01b,16jun93,hdn	 updated to 5.1.
01a,15may92,hdn	 written.
*/

#ifndef	__INCi8250h
#define	__INCi8250h

#ifdef __cplusplus
extern "C" {
#endif


#ifndef _ASMLANGUAGE

#include "tylib.h"

typedef struct			/* TY_CO_DEV */
    {
    TY_DEV tyDev;
    BOOL created;	/* true if this device has really been created */
    USHORT lcr;		/* UART line control register */
    USHORT lst;		/* UART line status register */
    USHORT mdc;         /* UART modem control register */
    USHORT msr;         /* UART modem status register */
    USHORT ier;		/* UART interrupt enable register */
    USHORT iid;		/* UART interrupt ID register */
    USHORT brdl;	/* UART baud rate register */
    USHORT brdh;	/* UART baud rate register */
    USHORT data;	/* UART data register */
    } TY_CO_DEV;

typedef struct			/* BAUD */
    {
    int rate;		/* a baud rate */
    int preset;		/* counter preset value to write to DUSCC_CTPR[HL]A */
    } BAUD;

/*
 * The macro UART_REG_ADDR_INTERVAL must be defined
 * when including this header.
 */

/* default definitions */

#define UART_ADRS(base,reg)   (CAST (base+(reg*UART_REG_ADDR_INTERVAL)))

/* register definitions */

#define UART_THR(base)	UART_ADRS(base,0x00)	/* Transmitter holding reg. */
#define UART_RDR(base)	UART_ADRS(base,0x00)	/* Receiver data reg.       */
#define UART_BRDL(base)	UART_ADRS(base,0x00)	/* Baud rate divisor (LSB)  */
#define UART_BRDH(base)	UART_ADRS(base,0x01)	/* Baud rate divisor (MSB)  */
#define UART_IER(base)	UART_ADRS(base,0x01)	/* Interrupt enable reg.    */
#define UART_IID(base)	UART_ADRS(base,0x02)	/* Interrupt ID reg.        */
#define UART_LCR(base)	UART_ADRS(base,0x03)	/* Line control reg.        */
#define UART_MDC(base)	UART_ADRS(base,0x04)	/* Modem control reg.       */
#define UART_LST(base)	UART_ADRS(base,0x05)	/* Line status reg.         */
#define UART_MSR(base)	UART_ADRS(base,0x06)	/* Modem status reg.        */


#if defined(__STDC__) || defined(__cplusplus)

IMPORT  void    tyCoInt (TY_CO_DEV *pTyCoDv);

#else

IMPORT  void    tyCoInt ();

#endif  /* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif	/* __INCi8250h */
