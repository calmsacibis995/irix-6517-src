/* wdbUdpSockLib.h - header file for remote debug agents UDP library */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,05apr95,ms  new data types.
01a,08mar95,ms  written.
*/

#ifndef __INCwdbUdpSockLibh
#define __INCwdbUdpSockLibh

/* includes */

#include "wdb/wdbcommiflib.h"

/* function prototypes */

#if defined(__STDC__)

extern STATUS  wdbUdpSockIfInit (WDB_COMM_IF *pCommIf);

#else   /* __STDC__ */

extern STATUS  wdbUdpSockIfInit ();

#endif  /* __STDC__ */

#endif  /* __INCwdbUdpSockLibh */

