/* wdbRtIfLib.h - header file for the WDB agents runtime interface */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,01jun95,ms	added taskLock/Unlock, removed timers
01b,05apr95,ms  new data types.
01a,23sep94,ms  written.
*/

#ifndef __INCwdbRtIfLibh
#define __INCwdbRtIfLibh

/* includes */

#include "wdb/wdb.h"
#include "wdb/wdbregs.h"
#include "netinet/in_systm.h"

/* data types */

typedef struct
    {
    /* this function must be provided always */

    void	(*rtInfoGet)	(WDB_RT_INFO *pRtInfo);

    /* function call to reboot machine */

    void	(*reboot)	(void);

    /* function to call after loading text */

    void	(*cacheTextUpdate)	(char *addr, u_int nBytes);

    /* function to call to write protect memory */

    STATUS	(*memProtect)		(
					 char *addr,
					 u_int nBytes,
					 BOOL protect
					);

    /* function needed to probe memory (required) */

    STATUS	(*memProbe)		(
					 char *addr,
					 int mode,
					 int len,
					 char *pVal
					);

    /* function to install an exception hook (for exc event notification) */

    void	(*excHookAdd)		(void (*hook)
						(
						WDB_CTX context,
						u_int vec,
						char *pESF,
						WDB_IU_REGS *pRegs
						)
					);

    /* functions needed to support task control */

    int		(*taskCreate)	(
				char *     name,	/* NULL = default */
				int	   pri,
				int	   opts,
				char *	   stackbase,
				int	   stackSize,
				char *     entryPoint,
				int        args[10],
				int	   fdIn,	/* 0 = no redirect */
				int	   fdOut,
				int	   fdErr
				);
    STATUS	(*taskResume)	(WDB_CTX *pContext);
    STATUS	(*taskSuspend)	(WDB_CTX *pContext);
    STATUS	(*taskDelete)	(WDB_CTX *pContext);

    /* functions needed to support task register manipulation */

    STATUS	(*taskRegsSet)	(WDB_CTX *pContext, WDB_REG_SET_TYPE regSetType,
				char * pRegSet);
    STATUS	(*taskRegsGet)	(WDB_CTX *pContext, WDB_REG_SET_TYPE regSetType,
				char ** ppRegSet);

    /* memory operations */

    void *	(*malloc)	(size_t nBytes);
    void 	(*free)		(void * pData);

    /* premption disable/reenable routine (for gopher interlocking) */

    void	(*taskLock)	(void);
    void	(*taskUnlock)	(void);

    /* binary sempahores. Required for task agent I/O */

    void *	(*semCreate)	(void);
    STATUS	(*semGive)	(void *semId);
    STATUS	(*semTake)	(void *semId, struct timeval * tv);

    /*
     * task object hooks.
     *
     * Delete hooks are required for context exit notification (and hence
     * for function calls).
     */

    STATUS	(*taskDeleteHookAdd)	(UINT32 ctxId, void (*hook)());
    STATUS	(*taskCreateHookAdd)	(void (*hook)());
    STATUS	(*taskSwitchHookAdd)	(void (*hook)());
    } WDB_RT_IF;

/* global variables */

extern WDB_RT_IF *      pWdbRtIf;

/* function prototypes */

#if defined(__STDC__)

extern void vxRtIfInit		(WDB_RT_IF *pRtIf);
extern void nullRtIfInit	(WDB_RT_IF *pRtIf);

#else   /* __STDC__ */

extern void vxRtIfInit		();
extern void nullRtIfInit	();

#endif  /* __STDC__ */

#endif  /* __INCwdbRtIfLibh */

