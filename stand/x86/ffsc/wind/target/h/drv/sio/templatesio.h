/* template.h - header file for template serial driver */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,02aug95,ms  written.
*/

#ifndef __INCtemplateSioh
#define __INCtemplateSioh

#include "siolib.h"

/* device and channel structures */

typedef struct
    {
    /* always goes first */

    SIO_DRV_FUNCS *	pDrvFuncs;	/* driver functions */

    /* callbacks */

    STATUS	        (*getTxChar) ();
    STATUS	        (*putRcvChar) ();
    void *	        getTxArg;
    void *	        putRcvArg;

    /* register addresses */

    volatile char *     cr;             /* channel control register */
    volatile char *     dr;             /* channel data register */
    volatile char *     sr;             /* channel status register */
    volatile short *    br;             /* channel baud constant register */

    /* misc */

    int                 mode;           /* current mode (interrupt or poll) */
    int                 baudFreq;       /* input clock frequency */
    } TEMPLATE_CHAN;

typedef struct
    {
    TEMPLATE_CHAN	portA;          /* DUSRAT has two channels */
    TEMPLATE_CHAN	portB;
    volatile char *     masterCr;        /* master control register */
    } TEMPLATE_DUSART;

/* definitions */

	/* master control register definitions */

#define TEMPLATE_RESET_CHIP	0x1

	/* channel control register definitions */

#define TEMPLATE_RESET_TX	0x1	/* reset the transmitter */
#define TEMPLATE_RESET_ERR	0x2	/* reset error condition */
#define TEMPLATE_RESET_INT	0x4	/* acknoledge the interrupt */
#define	TEMPLATE_INT_ENABLE	0x8	/* enable interrupts */

	/* channel status register definitions */

#define TEMPLATE_TX_READY	0x1	/* txmitter ready for another char */
#define TEMPLATE_RX_AVAIL	0x2	/* character has arrived */

/* function prototypes */

#if defined(__STDC__)

extern void 	templateDevInit	(TEMPLATE_DUSART *pDusart); 
extern void	templateIntRcv	(TEMPLATE_CHAN *pChan);
extern void	templateIntTx	(TEMPLATE_CHAN *pChan);
extern void	templateIntErr	(TEMPLATE_CHAN *pChan);

#else   /* __STDC__ */

extern void 	templateDevInit	();
extern void	templateIntRcv	();
extern void	templateIntTx	();
extern void	templateIntErr	();

#endif  /* __STDC__ */

#endif  /* __INCtemplateSioh */
