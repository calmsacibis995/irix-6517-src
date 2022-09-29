/* mb86940.h - Fujitsu SPARClite companion chip */

/* Copyright 1984-1991 Wind River Systems, Inc. */
/*
modification history
--------------------
01a,31jul95,myz  written
*/

/*
This file contains SIO data structures for the Fujitsu mb86940 companion chip.
*/

#ifndef __INCmb86940Sioh
#define __INCmb86940Sioh

#ifdef __cplusplus
extern "C" {
#endif

#include "arch/sparc/mb86940.h"
#include "siolib.h"


typedef struct MB86940_CHAN
    {
    /* always goes first */

    SIO_DRV_FUNCS *     pDrvFuncs;      /* driver functions */

    /* callbacks */

    STATUS      (*getTxChar) ();
    STATUS      (*putRcvChar) ();
    void *      getTxArg;
    void *      putRcvArg;

    USHORT          channelMode;
    USHORT          chan_num;
    int             baudRate;

    volatile int *cp;		/* control port I/O address */
    volatile int *dp;		/* data port I/O address */
    USHORT  txIntLevel;
    USHORT  rxIntLevel;
    USHORT  txIntVec;
    USHORT  rxIntVec;

    UINT    (*asiGeth) (void * );
    void    (*asiSeth) (void * , UINT );
    STATUS  (*baudSet) (int ); 
    STATUS  (*intEnable)(int);
    STATUS  (*intDisable)(int);
    } MB86940_CHAN;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)
extern void mb86940DevInit (MB86940_CHAN *);
extern void mb86940IntRx (MB86940_CHAN *);
extern void mb86940IntTx (MB86940_CHAN *);

#else	/* __STDC__ */
extern void mb86940DevInit ();
extern void mb86940IntRx ();
extern void mb86940IntTx ();

#endif	/* __STDC__ */


#ifdef __cplusplus
}
#endif

#endif /* __INCmb86940h */
