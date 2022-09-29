/* wdbLib.h - header file for remote debug server */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,20jun95,tpr added wdbMemCoreLibInit() prototype.
01d,19jun95,ms	added prototypes for wdbCtxExitLibInit and wdbSioTest
01c,01jun95,ms	changed prototype for wdbGopherLibInit.
01b,05apr95,ms  new data types.
01a,20sep94,ms  written.
*/

#ifndef __INCwdbLibh
#define __INCwdbLibh

/* includes */

#include "wdb/wdb.h"
#include "wdb/wdbrtiflib.h"
#include "wdb/wdbcommiflib.h"
#include "wdb/wdbrpclib.h"
#include "wdb/wdbbplib.h"
#include "siolib.h"

/* function prototypes */

#ifdef	__STDC__

extern STATUS	wdbTaskInit		(int pri, int opts, caddr_t stackBase,
					 int stackSize);
extern STATUS	wdbExternInit		(void * stackBase);
extern void	wdbInstallRtIf		(WDB_RT_IF *);
extern void	wdbInstallCommIf	(WDB_COMM_IF *, WDB_XPORT *);
extern void	wdbConnectLibInit	(void);
extern void	wdbMemLibInit		(void);
extern void	wdbMemCoreLibInit	(void);
extern void	wdbCtxLibInit		(void);
extern void	wdbCtxExitLibInit	(void);
extern void	wdbRegsLibInit		(void);
extern void	wdbEventLibInit		(void);
extern void	wdbFuncCallLibInit	(void);
extern void	wdbDirectCallLibInit	(void);
extern void	wdbGopherLibInit	(void);
extern void	wdbVioLibInit		(void);
extern void	wdbExcLibInit		(void);
extern void	wdbSysBpLibInit		(struct breakpoint *, int);
extern void	wdbTaskBpLibInit	(void);
extern void	wdbSioTest		(SIO_CHAN *pChan, int mode, char eof);

#else	/* __STDC__ */

extern STATUS	wdbTaskInit		();
extern STATUS	wdbExternInit		();
extern void	wdbInstallRtIf		();
extern void	wdbInstallCommIf	();
extern void	wdbMemLibInit		();
extern void	wdbMemCoreLibInit	();
extern void	wdbCtxLibInit		();
extern void	wdbCtxExitLibInit	();
extern void	wdbRegsLibInit		();
extern void	wdbAddServiceLibInit	();
extern void	wdbDirectCallLibInit	();
extern void	wdbEventLibInit		();
extern void	wdbFuncCallLibInit	();
extern void	wdbGopherLibInit	();
extern void	wdbVioLibInit		();
extern void	wdbExcLibInit		();
extern void	wdbSysBpLibInit		();
extern void	wdbTaskBpLibInit	();
extern void	wdbSioTest		();

#endif	/* __STDC__ */

#endif /* __INCwdbLibh */

