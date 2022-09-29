/* systm.h - system header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * and the VxWorks Software License Agreement specify the terms and
 * conditions for redistribution.
 *
 *	@(#)systm.h	7.1 (Berkeley) 6/4/86
 */

/*
modification history
01k,22sep92,rrr  added support for c++
--------------------
01j,26may92,rrr  the tree shuffle
01i,26may92,ajm  got rid of HOST_DEC def's (new compiler)
                  updated copyright
01k,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01j,07mar91,ajm  ifdef'd out _remque, and _insque for mips compiler
01i,05oct90,shl  added copyright notice.
02h,20mar90,jcf  changed semTake operations to include timeout of WAIT_FOREVER.
02g,22jun88,dnw  name tweaks to spl macros.
		 added macros for ovbcopy(), copyin(), copyout(), and imin().
02f,04mar88,jcf  changed splSem to splSemId, and changed sem calls accordingly.
02e,27jan88,rdc  included taskLib.h instead of vxLib.h
02d,23jan88,rdc  changed names of processor level macros because of
		 size issues.
02c,08jan88,rdc  added processor level macros.
02b,28aug87,dnw  removed unnecessary stuff.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCsystmh
#define __INCsystmh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "tasklib.h"
#include "semlib.h"

IMPORT SEM_ID splSemId;
IMPORT int splTid;

/* casts to keep lint happy */

#define insque(q,p)     _insque((caddr_t)q,(caddr_t)p)
#define remque(q)       _remque((caddr_t)q)

/* macros for Unix routines */

#define ovbcopy		bcopy			/* overlapped buffer copy */
#define copyout(from, to, len)	(bcopy (from, to, len), OK)
#define copyin(from, to, len)	(bcopy (from, to, len), OK)
#define imin(a,b)	(((a) < (b)) ? (a) : (b))	/* interger min */

/* processor level macro -
 * these are identical to the routines spl...() and can be used where
 * speed is more important than code size.
 */

#define SPLNET()	(((ULONG)taskIdCurrent == splTid) ?	\
		    	   1 : (semTake(splSemId, WAIT_FOREVER),  \
			       splTid = (ULONG)taskIdCurrent, 0))

#define SPLIMP()	SPLNET()

#define SPLX(x)		if ((x) == 0) (splTid = 0, semGive (splSemId))

#ifdef __cplusplus
}
#endif

#endif /* __INCsystmh */
