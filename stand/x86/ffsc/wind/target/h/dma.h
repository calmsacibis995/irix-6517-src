/* dma.h - DMA (direct memory access) header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01a,20dec90,jcc  written.
*/

#ifndef __INCdmah
#define __INCdmah

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "private/semlibp.h"

typedef struct dmaCtrl		/* DMA_CTRL - generic DMA controller info */
    {
    SEMAPHORE ctrlMutexSem;	/* semaphore for exclusive access */
    SEMAPHORE ctrlSyncSem;	/* semaphore for waiting on I/O interrupt */
    BOOL intOnCompletion;	/* whether ctrl can interrupt when done */
    VOIDFUNCPTR dmaReset;	/* function for resetting the DMA bus */
    FUNCPTR dmaTransact;	/* function for managing a DMA transaction */
    FUNCPTR dmaBytesIn;		/* function for DMA input */
    FUNCPTR dmaBytesOut;	/* function for DMA output */
    FUNCPTR dmaBusPhaseGet;	/* function returning the current bus phase */
    UINT maxBytesPerXfer;	/* upper bound of ctrl. tansfer counter */
    UINT clkPeriod;		/* period of the controller clock (nsec) */
    int dmaPriority;		/* priority of task when doing DMA I/O */
    int dmaBusPhase;		/* current phase of DMA */
    } DMA_CTRL;

/* structure to pass to ioctl call to execute a DMA command */

typedef struct  /* DMA_TRANSACTION - information about a DMA transaction */
    {
    UINT8 *srcAddress;		/* address of source data buffer */
    UINT8 *destAddress;		/* address of destination data buffer */
    int dataLength;		/* length of data buffer in bytes (0=no data) */
    UINT8 statusByte;		/* status byte returned from target */
    } DMA_TRANSACTION;

#ifdef __cplusplus
}
#endif

#endif /* __INCdmah */
