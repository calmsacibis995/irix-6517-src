/* trcLib.h - header file for trcLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,22sep92,rrr  added support for c++
01g,18sep92,jcf  added include of regs.h.
01f,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,07apr92,yao  made ANSI prototype of trcStack() consistent for all
		 architecutres.
01c,09jan92,jwt  modified trcStack() prototype for SPARC.
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCtrcLibh
#define __INCtrcLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "regs.h"

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	trcStack (REG_SET *pRegs, FUNCPTR printRtn, int tid);

#else

extern void 	trcStack ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtrcLibh */
