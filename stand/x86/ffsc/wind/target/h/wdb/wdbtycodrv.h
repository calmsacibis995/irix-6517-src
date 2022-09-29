/* wdbTyCoDrv.h - header file for BSP serial drivers used by the agent */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,15jun95,ms	updated for new serial driver.
01b,05apr95,ms  new data types.
01a,20dec94,ms  written.
*/

#ifndef __INCwdbTyCoDrvh
#define __INCwdbTyCoDrvh

/* includes */

#include "siolib.h"

/* data types */

typedef struct
    {
    SIO_DRV_FUNCS *	pDrvFuncs;	/* driver functions */
    STATUS		(*getTxChar)(); /* tx callback */
    void *		getTxArg;	/* callback arg */
    STATUS		(*putRcvChar)(); /* receive callback */
    void *		putRcvArg;	/* callback arg */
    int			fd;		/* underlying tyCo file descriptor */
    } WDB_TYCO_SIO_CHAN;

/* function prototypes */

#if defined(__STDC__)

extern STATUS  wdbTyCoDevInit	(WDB_TYCO_SIO_CHAN *pDev, char *devName,
				 int baud);

#else   /* __STDC__ */

extern STATUS  wdbTyCoInit	();

#endif  /* __STDC__ */

#endif  /* __INCwdbTyCoDrvh */

