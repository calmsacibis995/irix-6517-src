#ifndef __SYS_TIMERS_H__
#define __SYS_TIMERS_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Copyright 1992-1997 Silicon Graphics, Inc. 
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
 * rights reserved under the Copyright Lanthesaws of the United States.
 */
#ident "$Revision: 1.47 $ $Author: leedom $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#ifndef _KERNEL
#include <time.h>
#endif /* _KERNEL */
#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */


/*
 * Thread per-state timer array.  Exported via PIOCGETPTIMER.
 */
#define	AS_USR_RUN	0		/* running in user mode */
#define	AS_SYS_RUN	1		/* running in system mode */
#define AS_INT_RUN	2		/* running in interrupt mode */
#define	AS_BIO_WAIT	3	 	/* wait for block I/O */
#define AS_MEM_WAIT	4		/* waiting for memory */
#define	AS_SELECT_WAIT	5	 	/* wait in select */
#define	AS_JCL_WAIT	6	 	/* stop because of job control */
#define	AS_RUNQ_WAIT	7	 	/* waiting to run on run queue */ 
#define	AS_SLEEP_WAIT	8	 	/* sleep waiting for resource */
#define AS_STRMON_WAIT	9		/* sleep for the stream monitor */
#define	AS_PHYSIO_WAIT	10	 	/* wait for raw I/O */

#define MAX_PROCTIMER	11		/* max # of per process timers */

#if defined(_KERNEL) || defined(_KMEMUSER)
/*
 * Locore interrupt entry and exit code needs to change the current thread's
 * timer mode to AS_INT_RUN on entry and then restore the previous mode on
 * exit.  This logically calls for the previous mode to be stored on the
 * stack -- perhaps even in the interrupt exception frame.  However, this
 * isn't how it's being done.  We will revisit this code in the future to
 * make it simpler.  In the mean time ...
 *
 * The locore interrupt entry code passes the new timer mode into the common
 * exception code.  In addition to the new timer to switch to, a flag may be
 * passed which indicates that the current timer mode should not be changed.
 * This appears to be used in sections of code which have determined that
 * we're already at interrupt level.  Or maybe that we want to charge the
 * interrupt to the current mode.  Or maybe ...  Who knows, it wasn't well
 * documented to begin with.  As above, we'll revisit this later ...
 */
#define MAX_PROCTIMERMASK	0x0ff	/* mask to cover ktimer mode values */
#define AS_DONT_SWITCH		0x100	/* don't switch current timer */
#endif /* _KERNEL || _KMEMUSER */


#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#if defined(_KERNEL) || defined(_KMEMUSER)
/*
 * Definitions for per-thread high-resolution timers.  These timers consist
 * of an array of accumulators indicating how much time has been spent in
 * various states like running in user mode, running in system mode, waiting
 * on I/O, etc.  At any particular time a thread is in one of the timer
 * states.  When we switch from one state to another, we snapshot the current
 * high-resolution time, compute the delta from when the last state was
 * entered and add that to that state's accumulator, and save the time
 * snapshot.
 *
 * On some low-end platforms the hardware timers used for the high-resolution
 * time snapshots will wrap in a fairly short amount of time.  In order to
 * support timing across long intervals for these platforms we have to do some
 * very ugly stuff.  On the other hand we don't want to penalize the other
 * platforms with this ugliness, so we have two different implementations:
 * a simple one that simply accumulates timer snapshot deltas for platforms
 * with large high-resolution timers and a more complex one for other
 * platforms.
 *
 * In the more complex code for systems with small high-resolution timers, we
 * snapshot the current value of "time" (seconds since 1970, etc.) and the
 * high-resolution timer when we start a timer.  When we stop a timer we look
 * at "time" to see if more than timer_maxsec has passed since we started the
 * timer.  If so, we simply accumulate the number of seconds since the timer
 * was started, otherwise we use the delta of the high-resolution timer
 * snapshots.
 */
#if defined(CLOCK_CTIME_IS_ABSOLUTE)
typedef uint64_t ktimersnap_t;		/* value of RTC at snapshot */
#else
typedef struct {
	time_t		secs;		/* value of "time" at snapshot */
	uint32_t	rtc;		/* value of RTC at snapshot */
} ktimersnap_t;
#endif

typedef struct {
	uint64_t	kp_timer[MAX_PROCTIMER];
					/* thread state timers */
	ktimersnap_t	kp_snap;	/* time we started the current timer */
	int		kp_curtimer;	/* currently active timer */
} ktimerpkg_t;


#include <sys/ktime.h>			/* irix5_64_time_t, irix5_time_t */
#include <sys/timespec.h>

#define TIMER_MAX	0xffffffff	/* assumed 32 bits timer for now */

#endif /* _KERNEL || _KMEMUSER */


#ifdef _KERNEL

/*
 * Machine dependent setting based on hardware support.
 */
extern unsigned timer_freq;		/* freq of timestamping source */
extern unsigned timer_unit;		/* duration in nsec of timer tick */
extern unsigned timer_high_unit;	/* number of low bits in a high_bit */
extern unsigned timer_maxsec;		/* duration in sec before wrapping */

/*
 * get_timestamp()
 * This returns the value of the timestamping source.
 */
#define	get_timestamp()	_get_timestamp()

#if defined(MP)
#include <sys/clksupport.h>
#define absolute_rtc_current_time()	GET_LOCAL_RTC
#define _get_timestamp()		(unsigned int)GET_LOCAL_RTC
#else /* !MP */
extern __int64_t absolute_rtc_current_time(void);
extern time_t _get_timestamp(void);
#endif

/*
 *	Convert a kernel timer value into an accum_t, which for timers
 *	amounts to a 64-bit value in units of nanoseconds. This is assumed
 *	to be done at exit time or when the timer is otherwise guaranteed
 *	to be unchanging, so the ktimer_t is used directly (i.e. without
 *	saving it via TIMER_GRAB).
 *
 *	NOTE - The casting of tv_sec to accum_t is needed to force 64-bit
 *	calculation on 32-bit kernel.  Otherwise, an overflow will occur for
 *	long-running commands.
 */
#define TIMESPEC_TO_ACCUM(ts)	((ts)->tv_nsec + \
                                 (NSEC_PER_SEC * (accum_t) (ts)->tv_sec))

/*
 * Macros to combine and extract timer enumeration values with priorities and
 * some flags (see for instance the use of SV_BREAKABLE).
 */
#define TIMER_SHIFT(x)	((x&0xff)<<16) /* shift before OR in with pri */
#define TIMER_SHIFTBACK(x)	(((x)>>16)&0xff)
#define TIMER_MASK	0xffff

/*
 *	Exported kernel interface to timers
 */
struct timespec;
struct kthread;

extern void ktimer_init(struct kthread *, int);
extern void ktimer_switch(struct kthread *, int);
extern uint64_t ktimer_readticks(const struct kthread *, int);
extern void ktimer_read(const struct kthread *, int, struct timespec *);
extern long ktimer_slptime(const struct kthread *);
#if defined(CELL_IRIX)
extern uint64_t ktimer_accum(struct kthread *, int, uint64_t);
#endif

#include <sys/xlate.h>
int irix5_to_timespec(enum xlate_mode, void *, int, xlate_info_t *);
int timespec_to_irix5(void *, int, xlate_info_t *);
#endif /* defined(_KERNEL) */

#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */

#ifdef __cplusplus
}
#endif
#endif /* !__SYS_TIMERS_H__ */
