/* wdbSvcLib.h - header file for remote debug server */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,05apr95,ms  new data types.
01b,06feb95,ms	added XPORT handle to dispatch routine.
01a,20sep94,ms  written.
*/

#ifndef __INCwdbSvcLibh
#define __INCwdbSvcLibh

/* includes */

#include "wdb/wdb.h"
#include "wdb/wdbrpclib.h"

/* data types */

typedef struct			/* hidden */
    {
    u_int	serviceNum;
    UINT32	(*serviceRtn)();
    xdrproc_t	inProc;
    xdrproc_t	outProc;
    } WDB_SVC;

/* function prototypes */

#if defined(__STDC__)

extern void		wdbSvcLibInit	(WDB_SVC *wdbSvcArray, u_int size);
extern STATUS		wdbSvcAdd	(u_int procNum, UINT32 (*rout)(),
					 BOOL (*xdrIn)(), BOOL (*xdrOut)());
extern void		wdbSvcDispatch	(WDB_XPORT *pXport, uint_t procNum);

#else   /* __STDC__ */

extern void		wdbSvcLibInit	();
extern STATUS		wdbSvcAdd	();
extern void		wdbSvcDispatch	();

#endif  /* __STDC__ */

#endif  /* __INCwdbSvcLibh */

