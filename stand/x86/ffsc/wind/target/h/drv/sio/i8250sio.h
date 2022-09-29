/* i8250Sio.h - header file for binary interface Intel 8250 UART driver */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,15jun95,ms   updated for new driver.
01a,15mar95,myz  written (from i8250Serial.h)
*/

#ifndef	__INCi8250Sioh
#define	__INCi8250Sioh

#ifdef __cplusplus
extern "C" {
#endif


#ifndef _ASMLANGUAGE

#include "vxworks.h"
#include "siolib.h"

/* channel data structure  */

typedef struct I8250_CHAN
    {
    SIO_DRV_FUNCS * pDrvFuncs;       /* driver functions */

    STATUS      (*getTxChar) ();
    STATUS      (*putRcvChar) ();
    void *      getTxArg;
    void *      putRcvArg;

    UINT16  int_vec;                 /* interrupt vector number */
    UINT16  channelMode;             /* SIO_MODE_[INT | POLL] */
    UCHAR   (*inByte) (int);         /* routine to read a byte from register */
    void    (*outByte)(int,char);    /* routine to write a byte to register */

    ULONG   lcr;                     /* UART line control register */
    ULONG   lst;                     /* UART line status register */
    ULONG   mdc;                     /* UART modem control register */
    ULONG   msr;                     /* UART modem status register */
    ULONG   ier;                     /* UART interrupt enable register */
    ULONG   iid;                     /* UART interrupt ID register */
    ULONG   brdl;                    /* UART baud rate register */
    ULONG   brdh;                    /* UART baud rate register */
    ULONG   data;                    /* UART data register */
    } I8250_CHAN;



typedef struct			/* BAUD */
    {
    int rate;		/* a baud rate */
    int preset;		/* counter preset value to write to DUSCC_CTPR[HL]A */
    } BAUD;

/* register definitions */

#define UART_THR        0x00	/* Transmitter holding reg. */
#define UART_RDR        0x00	/* Receiver data reg.       */
#define UART_BRDL       0x00	/* Baud rate divisor (LSB)  */
#define UART_BRDH       0x01	/* Baud rate divisor (MSB)  */
#define UART_IER        0x01	/* Interrupt enable reg.    */
#define UART_IID        0x02	/* Interrupt ID reg.        */
#define UART_LCR        0x03	/* Line control reg.        */
#define UART_MDC        0x04	/* Modem control reg.       */
#define UART_LST        0x05	/* Line status reg.         */
#define UART_MSR        0x06	/* Modem status reg.        */


#if defined(__STDC__) || defined(__cplusplus)

extern void i8250HrdInit(I8250_CHAN *pDev);
extern void i8250Int (I8250_CHAN  *pDev);

#else

extern void i8250HrdInit();
extern void i8250Int();
	     
#endif  /* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif	/* __INCi8250h */
