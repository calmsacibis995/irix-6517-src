/*
 * Copyright 1990-1997 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
/*
 * Copyright (c) 1982 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)time.h	6.4 (Berkeley) 6/24/85
 */

#ifndef _SYS_TIME_H
#define _SYS_TIME_H
#ident	"$Revision: 3.53 $"

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>
#include <sgidefs.h>

/*
 * NOTE: this is a POSIX/XPG header - watch for name space pollution
 */

#ifndef _CLOCK_T
#define _CLOCK_T
#if _MIPS_SZLONG == 32
typedef long	clock_t;	/* Type returned by clock(3C) */
#endif
#if _MIPS_SZLONG == 64
typedef int	clock_t;	/* Type returned by clock(3C) */
#endif
#endif /* !_CLOCK_T */

/* needed here according to MIPS ABI draft */
#ifndef _TIME_T
#define _TIME_T
#if _MIPS_SZLONG == 32
typedef	long		time_t;		/* <time> type */
#endif
#if _MIPS_SZLONG == 64
typedef	int		time_t;		/* <time> type */
#endif
#endif /* !_TIME_T */

/* this also defined in dmcommon.h */
/* stamp_t: SGI-specific type for USTs and MSCs */
/* there is no "ust_t": use stamp_t ; there is no "msc_t": check library */
#ifndef _STAMP_T
#define _STAMP_T
typedef __int64_t stamp_t; /* used for USTs and often MSCs */
#if _SGIAPI
typedef struct USTMSCpair
{
  stamp_t ust; /* a UST value at which an MSC hit an input or output jack */
  stamp_t msc; /* the MSC which is being described */
} USTMSCpair;
#endif /* _SGIAPI */
#endif /* !_STAMP_T */

#if _XOPEN4UX || defined(_BSD_TYPES) || defined(_BSD_COMPAT)
/*
 * Structure returned by gettimeofday(2) system call,
 * and used in other calls.
 * Note this is also defined in sys/resource.h
 */
#ifndef _TIMEVAL_T
#define _TIMEVAL_T
struct timeval {
#if _MIPS_SZLONG == 64
	__int32_t :32;
#endif
	time_t	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

/* 
 * Since SVID API explicitly gives timeval struct to contain longs ...
 * Thus need a second timeval struct for 64bit binaries to correctly 
 * access specific files (ie. struct utmpx in utmpx.h).
 */
struct __irix5_timeval {
        __int32_t    tv_sec;         /* seconds */
        __int32_t    tv_usec;        /* and microseconds */
};
#endif /* _TIMEVAL_T */

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2
#define	ITIMER_MAX	3

struct	itimerval {
	struct	timeval it_interval;	/* timer interval */
	struct	timeval it_value;	/* current value */
};

#endif	/* _XOPEN4UX || BSD */

#if _SGIAPI || defined(_BSD_TYPES) || defined(_BSD_COMPAT)
/* for backwards compat */
#define irix5_timeval __irix5_timeval

/*
 * Operations on timevals.
 *
 * NB: timercmp does not work for >= or <=.
 */
#define	timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define	timercmp(tvp, uvp, cmp)	\
	((tvp)->tv_sec cmp (uvp)->tv_sec || \
	 (tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_usec cmp (uvp)->tv_usec)
#define	timerclear(tvp)		(tvp)->tv_sec = (time_t)0, (tvp)->tv_usec = 0L

struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};

#define	DST_NONE	0	/* not on dst */
#define	DST_USA		1	/* USA style dst */
#define	DST_AUST	2	/* Australian style dst */
#define	DST_WET		3	/* Western European dst */
#define	DST_MET		4	/* Middle European dst */
#define	DST_EET		5	/* Eastern European dst */
#define	DST_CAN		6	/* Canada */
#define DST_GB          7       /* Great Britain and Eire */
#define DST_RUM         8       /* Rumania */
#define DST_TUR         9       /* Turkey */
#define DST_AUSTALT     10      /* Australian style with shift in 1986 */

#endif	/* _SGIAPI || BSD */

#if _POSIX93 
#include <sys/timespec.h>
#endif

#if !defined(_KERNEL)

#if _XOPEN4UX

extern int getitimer(int, struct itimerval *);
extern int setitimer(int, const struct itimerval *, struct itimerval *);
extern int utimes(const char *, const struct timeval [2]);
#if !_SGIAPI && !defined(_BSD_TYPES) && !defined(_BSD_COMPAT)
extern int gettimeofday(struct timeval *, void *);
#endif

#include <sys/select.h>
#if _NO_XOPEN4 || _ABIAPI
extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#else
/* Hack to get xpg4 semantics for those desiring such. */
extern int __xpg4_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

/*REFERENCED*/
static int select(
	int nfds,
	fd_set *readfds,
	fd_set *writefds,
	fd_set *exceptfds,
	struct timeval *timeout)
{
	return __xpg4_select(nfds, readfds, writefds, exceptfds, timeout);
}
#endif	/* _NO_XOPEN4 */


#endif /* _XOPEN4UX */

#if _SGIAPI || defined(_BSD_TYPES) || defined(_BSD_COMPAT)

extern int adjtime(struct timeval *, struct timeval *);
extern int BSDgettimeofday(struct timeval *, struct timezone *);
extern int BSDsettimeofday(struct timeval *, struct timezone *);
extern int gettimeofday(struct timeval *,...);
extern int settimeofday(struct timeval *,...);

/*
 * In order to use the BSD versions of the timeofday calls, you may either:
 *	(1) explicitly call BSDgettimeofday() and BSDsettimeofday()
 *	    in the program,
 * or
 *	(2) set one of the _BSD_TIME or _BSD_COMPAT cpp constants before
 *	    including the time header file. There are 2 methods:
 *		(a) modify the source file, e.g.,
 *			#ifdef sgi
 *			#define _BSD_TIME
 *			#endif
 *			#include <sys/time.h>
 *		(b) use the cc(1) option -D_BSD_TIME or -D_BSD_COMPAT.
 *		   e.g., cc -D_BSD_TIME foo.c -o foo
 * Note that _BSD_COMPAT affects other header files that deal with BSD
 * compatibility.
 */
#if defined(_BSD_TIME) || defined(_BSD_COMPAT)
#define gettimeofday	BSDgettimeofday
#define settimeofday	BSDsettimeofday
#endif	/* _BSD_TIME || _BSD_COMPAT */

#include <time.h>

#endif /* _SGIAPI */
#endif 	/* !_KERNEL */

#if defined(_KERNEL)
#include "sys/types.h"
#include "sys/ktime.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TIME_H */
