/* wdbUlipPktDrv.h - header file for WDB agents ULIP packet driver */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,28jun95,ms  written.
*/

#ifndef __INCwdbUlipPktLibh
#define __INCwdbUlipPktLibh

/* includes */

#include "wdb/wdb.h"
#include "wdb/wdbcommiflib.h"

/* defines */

#define ULIP_MTU          1500

/* bimodal ULIP device */

typedef struct		/* hidden */
    {
    WDB_DRV_IF  wdbDrvIf;               /* must come first for wdbUdpLib */
    int         ulipFd;                 /* UNIX fd of ULIP device */
    bool_t      inBufLocked;            /* state of input buffer */
    char        inBuf[ULIP_MTU];        /* input buffer */
    } WDB_ULIP_PKT_DEV;

/* function prototypes */

#if defined(__STDC__)

extern void    wdbUlipPktDevInit (WDB_ULIP_PKT_DEV *pPktDev, char * ulipDev,
				void (*inputRtn)());

#else   /* __STDC__ */

extern void    wdbUlipPktDevInit ();

#endif  /* __STDC__ */

#endif  /* __INCwdbUlipPktLibh */

