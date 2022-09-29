/* vxLib.h - header file for vxLib.c */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
02d,04jul94,tpr	 added #include for mc68k/vx68kLib.h.
02d,02dec93,pme  added Am29K family support.
02d,26jul94,jwt  added vxMemProbeAsi() prototype for SPARC; copyright '94.
02c,24sep92,yao  added missing arg in vxTas() declaration.
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01f,30jun92,jmm  moved checksum() declarations to here from icmpLib.h
01e,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01d,21apr92,ccc  added vxTas.
01c,27feb92,wmd  added #include for i960/vx960Lib.h.
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCvxLibh
#define __INCvxLibh

#ifdef __cplusplus
extern "C" {
#endif

#if	(CPU_FAMILY == MC680X0)
#include "arch/mc68k/vx68klib.h"
#endif	/* (CPU_FAMILY == MC680X0) */

#if	CPU_FAMILY==I960
#include "arch/i960/vxi960lib.h"
#endif  /* CPU_FAMILY==I960 */

#if	CPU_FAMILY==AM29XXX
#include "arch/am29k/vxam29klib.h"
#endif  /* CPU_FAMILY==AM29XXX */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	sysMemProbe (char * adrs);
extern STATUS 	vxMemProbe (char * adrs, int mode, int length, char * pVal);
extern BOOL 	vxTas (void * address);
extern u_short	checksum (u_short * pAddr, int len);

#if	(CPU_FAMILY == SPARC)
extern STATUS 	vxMemProbeAsi (char * adrs, int mode, int length, char * pVal,
			       int asi);
#endif	/* CPU_FAMILY == SPARC */

#else	/* __STDC__ */

extern STATUS 	sysMemProbe ();
extern STATUS 	vxMemProbe ();
extern BOOL 	vxTas ();
extern u_short	checksum ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCvxLibh */
