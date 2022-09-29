/* kernelLibP.h - private header file for kernelLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,04jul92,jcf  created.
*/

#ifndef __INCkernelLibPh
#define __INCkernelLibPh

#ifdef __cplusplus
extern "C" {
#endif
#include "kernellib.h"
#include "qlib.h"

/* variable declarations */

extern BOOL	kernelState;		/* mutex to enter kernel state */
extern BOOL	kernelIsIdle;		/* boolean reflecting idle state */
extern BOOL	roundRobinOn;		/* state of round robin scheduling */
extern ULONG	roundRobinSlice;	/* round robin task slice in ticks */
extern int 	rootTaskId;		/* root task id */
extern char *	pRootMemStart;		/* bottom of root task's memory */
extern unsigned rootMemNBytes;		/* actual root task memory size */
extern Q_HEAD	tickQHead;		/* queue for timeouts/delays/wdogs */
extern Q_HEAD	readyQHead;		/* queue for task ready queue */
extern Q_HEAD	activeQHead;		/* task active queue head */

#ifdef __cplusplus
}
#endif

#endif /* __INCkernelLibPh */
