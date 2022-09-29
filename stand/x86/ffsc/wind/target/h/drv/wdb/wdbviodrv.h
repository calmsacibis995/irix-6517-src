/* wdbVioDrv.h - header file for remote debug packet library */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,28jun95,ms  written.
*/

#ifndef __INCwdbVioDrvh
#define __INCwdbVioDrvh

/* includes */

#include "wdb/wdb.h"

/* function prototypes */

#if defined(__STDC__)

extern void    wdbVioDrv (char * devName);

#else   /* __STDC__ */

extern void    wdbPktDevInit ();

#endif  /* __STDC__ */

#endif  /* __INCwdbVioDrvh */

