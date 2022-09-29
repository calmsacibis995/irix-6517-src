/* systime.h - obsolete vxWorks 5.0 header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02d,22sep92,rrr  added support for c++
02c,19sep92,smb  contents have been moved to sys/times.h
02b,31jul92,gae  changed INCtimeh to INCsystimeh.
02a,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01e,10jun91.del  added pragma for gnu960 alignment.
01d,25oct90,dnw  changed name from utime.h to systime.h.
		 added test for SunOS headers already included.
01c,05oct90,shl  added copyright notice.
01b,04nov87,dnw  removed unnecessary stuff.
01a,15oct87,rdc  written
*/

#ifndef __INCsystimeh
#define __INCsystimeh

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/times.h"

#ifdef __cplusplus
}
#endif

#endif /* __INCsystimeh */
