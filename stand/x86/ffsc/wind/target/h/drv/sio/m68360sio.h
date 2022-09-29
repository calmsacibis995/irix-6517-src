/* m68360Sio.h - SCC UART device structrue */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,15jun95,ms   updated for new driver.
01a,22may95,myz  written
*/


#ifndef __INCm68360Sioh
#define __INCm68360Sioh

#include "drv/multi/m68360.h"

#ifdef __cplusplus
extern "C" {
#endif


/* device and channel structures */

typedef struct          
    {
    /* always goes first */

    SIO_DRV_FUNCS *     pDrvFuncs;      /* driver functions */

    /* callbacks */

    STATUS      (*getTxChar) ();
    STATUS      (*putRcvChar) ();
    void *      getTxArg;
    void *      putRcvArg;

    UINT16              int_vec;        /* interrupt vector number */
    UINT16              channelMode;    /* SIO_MODE                */
    int                 baudRate;        
    int			clockRate;	/* CPU clock frequency (Hz) */
    int 		bgrNum;		/* number of BRG being used */
    UINT32 *		pBaud;		/* BRG registers */
    UINT32		regBase;	/* register/DPR base address */
    SCC_DEV		uart;		/* UART SCC device */
    } M68360_CHAN;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void m68360Int (M68360_CHAN *pDev);
extern void m68360DevInit (M68360_CHAN *pDev);

#else	/* __STDC__ */

extern void m68360Int ();
extern void m68360DevInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68360Sioh */
