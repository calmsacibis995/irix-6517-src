/* time.h - POSIX time header */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,14jul94,dvs  changed def of CLOCKS_PER_SEC to sysClkRateGet() (SPR# 2486).
01f,29nov93,dvs  folded in timers.h, made draft 14 POSIX compliant
01e,15oct92,rrr  silenced warnings.
01d,22sep92,rrr  added support for c++
01c,31jul92,gae  added undef of _TYPE_timer_t.
01b,27jul92,gae  added _TYPE_timer_t, CLOCK_REALTIME, TIMER_ABSTIME.
01a,22jul92,smb  written
           +rrr
*/

#ifndef __INCtimeh
#define __INCtimeh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"
#include "sigevent.h"

#define CLOCKS_PER_SEC	sysClkRateGet()

#ifndef NULL
#define NULL	((void *)0)
#endif

#ifdef  _TYPE_timer_t
_TYPE_timer_t;
#undef  _TYPE_timer_t
#endif

#ifdef _TYPE_clock_t
_TYPE_clock_t;
#undef _TYPE_clock_t
#endif

#ifdef _TYPE_size_t
_TYPE_size_t;
#undef _TYPE_size_t
#endif

#ifdef _TYPE_time_t
_TYPE_time_t;
#undef _TYPE_time_t
#endif

typedef int clockid_t;

#define CLOCK_REALTIME	0x0	/* system wide realtime clock */
#define TIMER_ABSTIME	0x1	/* absolute time */


struct tm
	{
	int tm_sec;	/* seconds after the minute	- [0, 61] */
	int tm_min;	/* minutes after the hour	- [0, 59] */
	int tm_hour;	/* hours after midnight		- [0, 23] */
	int tm_mday;	/* day of the month		- [1, 31] */
	int tm_mon;	/* months since January		- [0, 11] */
	int tm_year;	/* years since 1900	*/
	int tm_wday;	/* days since Sunday		- [0, 6] */
	int tm_yday;	/* days since January 1		- [0, 365] */
	int tm_isdst;	/* Daylight Saving Time flag */
	};

struct timespec
    {
    					/* interval = tv_sec*10**9 + tv_nsec */
    time_t tv_sec;			/* seconds */
    long tv_nsec;			/* nanoseconds (0 - 1,000,000,000) */
    };

struct itimerspec
    {
    struct timespec it_interval;	/* timer period (reload value) */
    struct timespec it_value;		/* timer expiration */
    };

#define	_POSIX_CLOCKRES_MIN	20	/* milliseconds (1/50th second) */
#define	_POSIX_INTERVAL_MAX	1092	/* max. seconds */
#define	_POSIX_TIMER_MAX	32	/* max. per task */
#define	_POSIX_DELAYTIMER_MAX	32	/* max. expired timers */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern uint_t      _clocks_per_sec(void);
extern char *	   asctime (const struct tm *_tptr);
extern clock_t	   clock (void);
extern char *	   ctime (const time_t *_cal);
extern double	   difftime (time_t _t1, time_t _t0);
extern struct tm * gmtime (const time_t *_tod);
extern struct tm * localtime (const time_t *_tod);
extern time_t	   mktime (struct tm *_tptr);
extern size_t	   strftime (char *_s, size_t _n, const char *_format,
		   	      const struct tm *_tptr);
extern time_t	   time (time_t *_tod);

extern int 	clock_gettime (clockid_t clock_id, struct timespec *tp);
extern int 	clock_settime (clockid_t clock_id, const struct timespec *tp);
extern int 	clock_getres (clockid_t clock_id, struct timespec *res);

extern int 	timer_create (clockid_t clock_id, struct sigevent *evp,
			      timer_t *ptimer);
extern int 	timer_delete (timer_t timerid);
extern int 	timer_gettime (timer_t timerid, struct itimerspec *value);
extern int 	timer_settime (timer_t timerid, int flags,
		               const struct itimerspec *value,
			       struct itimerspec *ovalue);
extern int 	timer_getoverrun (timer_t timerid);

extern int      timer_connect (timer_t timerid, VOIDFUNCPTR routine, int arg);
extern int      timer_cancel (timer_t timerid);
extern int      timer_show (timer_t timerid);

extern int 	nanosleep (const struct timespec *rqtp, struct timespec *rmtp);


#if _EXTENSION_POSIX_REENTRANT		/* undef this for ANSI */

extern int	   asctime_r(const struct tm *_tm, char *_buffer,
			     size_t *_buflen);
extern char *	   ctime_r (const time_t *_cal, char *_buffer, size_t *_buflen);
extern int	   gmtime_r (const time_t *_tod, struct tm *_result);
extern int	   localtime_r (const time_t *_tod, struct tm *_result);

#endif	/*  _EXTENSION_POSIX_REENTRANT */

#else /* __STDC__ */

extern uint_t      _clocks_per_sec();
extern char *	   asctime ();
extern clock_t	   clock ();
extern char *	   ctime ();
extern double	   difftime ();
extern struct tm * gmtime ();
extern struct tm * localtime ();
extern time_t	   mktime ();
extern size_t	   strftime ();
extern time_t	   time ();

extern int 	clock_settime ();
extern int 	clock_gettime ();
extern int 	clock_getres ();

extern int 	timer_create ();
extern int 	timer_delete ();
extern int 	timer_gettime ();
extern int 	timer_settime ();
extern int 	timer_getoverrun ();

extern int 	timer_connect ();
extern int 	timer_cancel ();
extern int 	timer_show ();

extern int 	nanosleep ();

#if _EXTENSION_POSIX_REENTRANT		/* undef this for ANSI */

extern int	   asctime_r();
extern char *	   ctime_r ();
extern int	   gmtime_r ();
extern int	   localtime_r ();

#endif	/*  _EXTENSION_POSIX_REENTRANT */

#endif /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtimeh */
