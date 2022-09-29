/* wdbNetcomPktDrv.h - header file for debug agents NETROM packet driver */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,25oct95,ms  changed params to wdbNetromPktDevInit.
01b,28jun95,tpr added pollDelay argument in wdbNetromPktDevInit().
01a,28jun95,ms  written.
*/

#ifndef __INCwdbNetcomPktLibh
#define __INCwdbNetcomPktLibh

/* includes */

#include "wdb/wdb.h"
#include "wdb/wdbcommiflib.h"

/* defines */

#define NETROM_MTU           500

/* data types */

typedef struct		/* hidden */
    {
    WDB_DRV_IF	wdbDrvIf;		/* first field for any packet device */
    char	inBuf [NETROM_MTU];	/* input buffer */
    BOOL	inBufFull;		/* input buffer is full */
    int		bytesRead;		/* bytes read into input buffer */
    } WDB_NETROM_PKT_DEV;

/* function prototypes */

#if defined(__STDC__)

extern void    wdbNetromPktDevInit (WDB_NETROM_PKT_DEV *pPktDev,
				caddr_t dpBase,
    				int     width,
				int     index,
				int     numAccess,
				void	(*stackRcv)(),
				int	pollDelay);

#else   /* __STDC__ */

extern void    wdbNetromPktDevInit ();

#endif  /* __STDC__ */

#endif  /* __INCwdbNetcomPktLibh */

