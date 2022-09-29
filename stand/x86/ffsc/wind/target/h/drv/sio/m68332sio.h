/* m68332Sio.h - SCC UART device structrue */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,22aug95,myz  written
*/


#ifndef __INCm68332Sioh
#define __INCm68332Sioh

#include "drv/multi/m68332.h"

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

    UINT16              channelMode;    /* SIO_MODE                */
    unsigned char       intVec;
    int                 baudRate;        
    int			clockRate;	/* CPU clock frequency (Hz) */
    volatile UINT16 *   sccr0;
    volatile UINT16 *   sccr1;
    volatile UINT16 *   scsr;
    volatile UINT16 *   scdr;
    volatile char *     qilr;
    } M68332_CHAN;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void m68332Int (M68332_CHAN *pDev);
extern void m68332DevInit (M68332_CHAN *pDev);

#else	/* __STDC__ */

extern void m68332Int ();
extern void m68332DevInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68332Sioh */
