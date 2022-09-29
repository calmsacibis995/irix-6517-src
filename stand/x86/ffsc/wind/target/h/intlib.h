/* intLib.h - interrupt library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01o,15jun95,ms	 added intRegsLock, intRegsUnlock prototypes
01n,02dec93,tpr	 removed am29200Intr3Connect() and am29200Intr3Drv().
	    tpr  added am29200Intr3DeMuxConnect().
	    tpr  added am29200Intr3Connect() and am29200Intr3Drv() for am29200.
	    pad  added am29k family support.
01m,15oct92,jwt  added intVecTableWriteEnable() prototype for SPARC.
01l,22sep92,rrr  added support for c++
01k,27jul92,rdc  added S_intLib_VEC_TABLE_WP_UNAVAILABLE and
		 intVecTableWriteProtect ().
01j,04jul92,jcf  cleaned up.
01i,26may92,rrr  the tree shuffle
01h,10jan92,jwt  added intAckConfig(), intTBRSet() prototypes for SPARC.
01g,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01f,08apr91,jdi  added NOMANUAL to prevent mangen.
01e,23oct90,shl  fixed misspelling in intUnlock().
		 changed intConnect() to take a VOIDFUNCPTR.
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01c,10aug90,dnw  added declaration of intVecSet().
01b,15jul90,dnw  added INT_CONTEXT()
01a,11apr90,jcf  written.
*/

#ifndef __INCintLibh
#define __INCintLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "regs.h"


/* status codes */

#define S_intLib_NOT_ISR_CALLABLE		(M_intLib | 1)
#define S_intLib_VEC_TABLE_WP_UNAVAILABLE	(M_intLib | 2)


/* variable declarations */

IMPORT int intCnt;		/* count of nested interrupt service routines */


/* macros */

#define INT_CONTEXT()	(intCnt > 0)	/* same as intContext() in intLib.c */

/*******************************************************************************
*
* INT_RESTRICT - restict access of a routine from interrupt use
*
* RETURNS: OK if called from task level, or ERROR if called from interrupt
*          level.
*
* NOMANUAL
*/

#define INT_RESTRICT()							\
    (									\
    ((intCnt > 0) ? errno = S_intLib_NOT_ISR_CALLABLE, ERROR : OK)	\
    )

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	intConnect (VOIDFUNCPTR *vector, VOIDFUNCPTR routine,
	       	    	    int parameter);
extern FUNCPTR 	intHandlerCreate (FUNCPTR routine, int parameter);
extern void 	intLockLevelSet (int newLevel);
extern int 	intLockLevelGet (void);
extern BOOL 	intContext (void);
extern int 	intCount (void);
extern void 	intVecBaseSet (FUNCPTR *baseAddr);
extern FUNCPTR *intVecBaseGet (void);
extern void 	intVecSet (FUNCPTR *vector, FUNCPTR function);
extern FUNCPTR 	intVecGet (FUNCPTR *vector);
extern int 	intLevelSet (int level);
extern int 	intLock (void);
extern int 	intUnlock (int oldSR);
extern int 	intRegsLock (REG_SET *pRegs);
extern void 	intRegsUnlock (REG_SET *pRegs, int lockKey);
extern STATUS   intVecTableWriteProtect (void);

#if	(CPU_FAMILY == SPARC)
extern void 	intAckConfig ();
extern void 	intTBRSet (FUNCPTR *baseAddr);
extern STATUS   intVecTableWriteEnable (void);
#endif

#if	(CPU_FAMILY == AM29XXX)
extern FUNCPTR	intHandlerCreateAm29k (FUNCPTR *vector, FUNCPTR routine,
				       int parameter);
#endif /* (CPU_FAMILY == AM29XXX) */

#if	(CPU == AM29200)
extern STATUS	am29200Intr3DeMuxConnect (VOIDFUNCPTR deMuxFct, int parameter);
#endif /* (CPU == AM29200) */

#else

extern STATUS 	intConnect ();
extern FUNCPTR 	intHandlerCreate ();
extern void 	intLockLevelSet ();
extern int 	intLockLevelGet ();
extern BOOL 	intContext ();
extern int 	intCount ();
extern void 	intVecBaseSet ();
extern FUNCPTR *intVecBaseGet ();
extern void 	intVecSet ();
extern FUNCPTR 	intVecGet ();
extern int 	intLevelSet ();
extern int 	intLock ();
extern int 	intUnlock ();
extern int 	intRegsLock ();
extern void 	intRegsUnlock ();
extern STATUS 	intVecTableWriteProtect ();

#if	(CPU_FAMILY == AM29XXX)
extern FUNCPTR	intHandlerCreateAm29k ();
#endif /* (CPU_FAMILY == AM29XXX) */

#if	(CPU == AM29200)
extern STATUS	am29200Intr3DeMuxConnect () ;
#endif /* (CPU == AM29200) */

#endif	/* __STDC__ */


#ifdef __cplusplus
}
#endif

#endif /* __INCintLibh */
