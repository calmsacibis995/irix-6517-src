/* taskArchLib.h - header file for taskArchLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,10dec93,hdn  added support for i86.
01j,02dec93,pme  added am29K family stack support.
01i,22sep92,rrr  added support for c++
01h,03jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,19mar92,yao  moved ANSI prototype for taskStackAllot() to taskLib.h.
01e,12mar92,yao  removed ANSI prototype for taskRegsShow().  added ANSI
		 prototype for taskRegsInit(), taskArgs{S,G}et(),
		 taskRtnValueSet().
01d,10jan92,jwt  added ANSI prototype for taskStackAllot().
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01b,05oct90,dnw deleted private functions.
01a,05oct90,shl created.
*/

#ifndef __INCtaskArchLibh
#define __INCtaskArchLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "tasklib.h"

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

#if (CPU_FAMILY==I80X86)
extern STATUS 	taskSRSet (int tid, UINT sr);
#else
extern STATUS 	taskSRSet (int tid, UINT16 sr);
#endif /* (CPU_FAMILY==I80X86) */

#if (CPU_FAMILY == AM29XXX)
extern void 	taskRegsInit (WIND_TCB *pTcb, char *pStackBase, int stackSize);
#else /* (CPU_FAMILY == AM29XXX) */
extern void 	taskRegsInit (WIND_TCB *pTcb, char *pStackBase);
#endif /* (CPU_FAMILY == AM29XXX) */
extern void 	taskArgsSet (WIND_TCB *pTcb, char *pStackBase,int pArgs[]);
extern void 	taskArgsGet (WIND_TCB *pTcb, char *pStackBase,int pArgs[]);
extern void 	taskRtnValueSet (WIND_TCB *pTcb, int returnValue);

#else

extern STATUS 	taskSRSet ();
extern void 	taskRegsInit ();
extern void 	taskArgsSet ();
extern void 	taskArgsGet ();
extern void 	taskRtnValueSet ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtaskArchLibh */
