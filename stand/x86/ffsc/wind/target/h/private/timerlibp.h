/* timerLibP.h - private POSIX 1003.4 Clocks & Timers header */

/* Copyright 1991-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
03e,29nov93,dvs  updated for POSIX draft 14
03d,23sep92,kdl  changed name to timerLibP.h
03c,22sep92,rrr  added support for c++
03b,31jul92,gae  added sigevent.h, time.h and timers.h includes.
03a,22jul92,gae  Draft 12 revision; removed object stuff; moved
		 some prototype definitions to timers.h.
02a,04jul92,jcf  cleaned up.
01d,26may92,rrr  the tree shuffle
01c,30apr92,rrr  some preparation for posix signals.
01a,16oct91,gae  written.
*/

/*
DESCRIPTION
This file provides header information for the
POSIX 1003.4 Clocks & Timers interface per Draft 12.
*/

#ifndef __INCtimerLibPh
#define __INCtimerLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/types.h"
#include "time.h"
#include "signal.h"
#include "sigevent.h"
#include "timers.h"
#include "private/siglibp.h"
#include "wdlib.h"


typedef struct __timer
    {
    WDOG_ID		wdog;		/* expiry mechanism */
    int			active;		/* wdog is set */
    int			taskId;		/* "owner" of timer */
    clock_t		clock_id;	/* always CLOCK_REALTIME */
    struct sigpend	sigpend;
    struct sigevent	sevent;		/* user's signal event */
    struct itimerspec	exp;		/* time to go off plus interval */
    VOIDFUNCPTR		routine;	/* user's routine */
    int			arg;		/* argument to user's routine */
    struct timespec     timeStamp;	/* timestamp when timer is armed */
    } TIMER;

typedef struct clock
    {
    int hz;				/* cycles per second */
    int resolution;			/* nanoseconds/hz */
    int tickBase;			/* vxTicks when time set */
    struct timespec timeBase;		/* time set */
    } CLOCK;

extern struct clock _clockRealtime;

/* macros for "time value" manipulation */

#define BILLION         1000000000	/* 1000 million nanoseconds / second */

#define TV_EQ(a,b)	\
        ((a).tv_sec == (b).tv_sec && (a).tv_nsec == (b).tv_nsec)
#define TV_GT(a,b)	\
        ((a).tv_sec  > (b).tv_sec ||	\
        ((a).tv_sec == (b).tv_sec && (a).tv_nsec > (b).tv_nsec))
#define TV_LT(a,b)	\
        ((a).tv_sec  < (b).tv_sec ||	\
        ((a).tv_sec == (b).tv_sec && (a).tv_nsec < (b).tv_nsec))
#define TV_ISZERO(a)	\
        ((a).tv_sec == 0 && (a).tv_nsec == 0)
#define TV_SET(a,b)	\
        (a).tv_sec = (b).tv_sec; (a).tv_nsec = (b).tv_nsec
#define TV_ADD(a,b)	\
        (a).tv_sec += (b).tv_sec; (a).tv_nsec += (b).tv_nsec; TV_NORMALIZE(a)
#define TV_SUB(a,b)	\
        (a).tv_sec -= (b).tv_sec; (a).tv_nsec -= (b).tv_nsec; TV_NORMALIZE(a)
#define	TV_NORMALIZE(a)	\
	if ((a).tv_nsec >= BILLION)	\
	    { (a).tv_sec++; (a).tv_nsec -= BILLION; }	\
	else if ((a).tv_nsec < 0)	\
	    { (a).tv_sec--; (a).tv_nsec += BILLION; }
#define TV_VALID(a)	\
        ((a).tv_nsec >= 0 && (a).tv_nsec < BILLION)
#define TV_ZERO(a)	\
        (a).tv_sec = 0; (a).tv_nsec = 0

#define TV_CONVERT_TO_SEC(a,b)  \
	(a).tv_sec  = (b) / _clockRealtime.hz;   \
	(a).tv_nsec = ((b) % _clockRealtime.hz) * _clockRealtime.resolution

#define TV_CONVERT_TO_TICK(a,b) \
	(a) = (b).tv_sec * _clockRealtime.hz + \
	      (b).tv_nsec / _clockRealtime.resolution


/* non-standard function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int 	clock_setres (clockid_t clock_id, struct timespec *res);

extern int 	timer_cancel (timer_t timerid);
extern int 	timer_connect (timer_t timerid, VOIDFUNCPTR routine, int arg);
extern int 	timer_show (timer_t timerid);

#else

extern int 	clock_setres ();

extern int 	timer_cancel ();
extern int 	timer_connect ();
extern int 	timer_show ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtimerLibPh */
