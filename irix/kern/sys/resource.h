/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)resource.h	7.1 (Berkeley) 6/4/86
 */
#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <standards.h>
#include <sgidefs.h>
#include <sys/types.h>

/*
 * from sys/time.h
 */
#ifndef _TIMEVAL_T
#define _TIMEVAL_T
struct timeval {
#if _MIPS_SZLONG == 64
	__int32_t :32;
#endif
        time_t  tv_sec;         /* seconds */
        long    tv_usec;        /* and microseconds */
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
 * Process priority specifications to get/setpriority.
 */
#define	PRIO_MIN	-20
#define	PRIO_MAX	20

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2

/*
 * Resource utilization information.
 */

#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	-1

struct	rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
#if _ABIAPI
	long	ru_pad[14];
#else
	long	ru_maxrss;
#define	ru_first	ru_ixrss
	long	ru_ixrss;		/* integral shared memory size */
	long	ru_idrss;		/* integral unshared data " */
	long	ru_isrss;		/* integral unshared stack " */
	long	ru_minflt;		/* page reclaims */
	long	ru_majflt;		/* page faults */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
#define	ru_last		ru_nivcsw
#endif	/* _ABIAPI */
};

/*
 * Resource limits
 */
#define	RLIMIT_CPU	0		/* cpu time in seconds */
#define	RLIMIT_FSIZE	1		/* maximum file size */
#define	RLIMIT_DATA	2		/* data size */
#define	RLIMIT_STACK	3		/* stack size */
#define	RLIMIT_CORE	4		/* core file size */
#define RLIMIT_NOFILE	5		/* file descriptors */
#define RLIMIT_VMEM	6		/* maximum mapped memory */
#define	RLIMIT_RSS	7		/* resident set size */
#define RLIMIT_AS	RLIMIT_VMEM
#define	RLIMIT_PTHREAD	8		/* pthreads */

#define	RLIM_NLIMITS	9		/* number of resource limits */

#ifdef _KERNEL
#define RLIM_INFINITY	0x7fffffffffffffffLL
#define	RLIM32_INFINITY	0x7fffffff
#elif (_MIPS_SZLONG == 64) || (_MIPS_SIM == _ABIN32)
#define	RLIM_INFINITY	0x7fffffffffffffffLL
#else
#define	RLIM_INFINITY	0x7fffffff
#endif /* _KERNEL */

#if _LFAPI
#define	RLIM64_SAVED_CUR	0x7ffffffffffffffdLL
#define	RLIM64_SAVED_MAX	0x7ffffffffffffffeLL
#define	RLIM64_INFINITY		0x7fffffffffffffffLL
#endif	/* _LFAPI */

#if _LFAPI || _XOPEN5
#define	RLIM_SAVED_CUR		0x7ffffffd
#define	RLIM_SAVED_MAX		0x7ffffffe
#endif

#ifdef _KERNEL
typedef __uint64_t rlim_t;
#elif _MIPS_SIM == _ABIN32
typedef __uint64_t rlim_t;
#else
typedef unsigned long rlim_t;
#endif

struct rlimit {
	rlim_t	rlim_cur;		/* current (soft) limit */
	rlim_t	rlim_max;		/* maximum value for rlim_cur */
};

#if _LFAPI
typedef __uint64_t rlim64_t;

struct rlimit64 {
	rlim64_t	rlim_cur;
	rlim64_t	rlim_max;		
};		
#endif	/* _LFAPI */

#ifdef _KERNEL

#include <sys/types.h>

/*
 * Convert the given 32 bit limit spec to RLIM_INFINITY if it
 * is specified as RLIM32_INFINITY.
 */
#define	RLIM32_CONV(x)	(((x) == RLIM32_INFINITY) ? RLIM_INFINITY : (x))

struct uthread_s;

extern void ruadd(struct rusage *, struct rusage *);
extern int setrlimitcommon(usysarg_t, struct rlimit *);
extern rlim_t getaslimit(struct uthread_s *, int);
extern rlim_t getfsizelimit(void);
extern struct rlimit rlimits[];

struct irix5_rlimit {
	app32_ulong_t	rlim_cur;
	app32_ulong_t	rlim_max;
};

#else	/* !_KERNEL */

extern int getrlimit(int, struct rlimit *);
extern int setrlimit(int, const struct rlimit *);
extern int getpriority(int, id_t);
extern int setpriority(int, id_t, int);
extern int getrusage(int, struct rusage *);

#if _LFAPI
extern int getrlimit64(int, struct rlimit64 *);
extern int setrlimit64(int, const struct rlimit64 *);
#endif	/* _LFAPI */

#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_RESOURCE_H */
