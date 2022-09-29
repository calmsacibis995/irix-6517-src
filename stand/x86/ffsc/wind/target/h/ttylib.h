/* ttyLib.h - header file for terminal drivers on top of sio core drivers */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,15jun95,ms	updated for new serial driver.
01b,22may95,ms  removed unneded include file.
01a,21feb95,ms  written.
*/

#ifndef __INCttyLibh
#define __INCttyLibh

#include "types/vxtypes.h"
#include "siolib.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS ttyDrv	   ();
extern STATUS ttyDevCreate (char *name, SIO_CHAN *pChan, int rdBufSize,
			    int wrtBufSize);
#else

extern STATUS ttyDrv	   ();
extern STATUS ttyDevCreate ();

#endif

#ifdef __cplusplus
}
#endif

#endif  /* __INCttyLibh */

