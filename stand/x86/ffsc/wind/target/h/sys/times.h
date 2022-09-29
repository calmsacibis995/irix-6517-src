/* times.h - UNIX-style timeval structure defenition */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02d,22sep92,rrr  added support for c++
02c,19aug92,smb  moved from systime.h
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

#ifndef __INCtimesh
#define __INCtimesh

#ifdef __cplusplus
extern "C" {
#endif

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* the following conditional is to avoid conflict with SunOS header <sys/time.h>
 * (_TIME_ is used in SunOS 4.0.3 and _sys_time_h is used in 4.1)
 */
#if !defined(_TIME_) && !defined(_sys_time_h)

struct timeval
    {
    long tv_sec;	/* seconds */
    long tv_usec;	/* microseconds */
    };

struct timezone
    {
    int	tz_minuteswest;	/* minutes west of Greenwich */
    int	tz_dsttime;	/* type of dst correction */
    };

#endif	/* !_TIME_ && !_sys_time_h */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCtimesh */
