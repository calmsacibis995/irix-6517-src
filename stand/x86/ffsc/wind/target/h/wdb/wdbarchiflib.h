/* wdbArchIfLib.h - header file for arch-specific routines needed by wdb */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,15jun95,ms	removed _sigCtxIntLock prototype
01b,05apr95,ms	new data types.
01a,21dec94,ms  written.
*/

#ifndef __INCwdbArchIfLibh
#define __INCwdbArchIfLibh

/* includes */

#include "wdb/wdb.h"
#include "wdb/wdbregs.h"
#include "intlib.h"

/* function prototypes */

#if defined(__STDC__)

extern int   	_sigCtxSave	(WDB_IU_REGS *pContext);
extern void   	_sigCtxLoad	(WDB_IU_REGS *pContext);
extern void   	_sigCtxSetup	(WDB_IU_REGS *pContext, char *pStackBase,
				 void (*entry)(), int *pArgs);
extern void	_sigCtxRtnValSet (WDB_IU_REGS *pContext, int val);

#else   /* __STDC__ */

extern int   	_sigCtxSave	();
extern void   	_sigCtxLoad	();
extern void   	_sigCtxSetup	();
extern void	_sigCtxRtnValSet ();

#endif  /* __STDC__ */

#endif  /* __INCwdbArchIfLibh */

