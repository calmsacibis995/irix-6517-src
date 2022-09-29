/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _SYS_KTIME_H
#define _SYS_KTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/time.h"
#include "sys/sema.h"
#include "sys/capability.h"
#include <sys/xlate.h>
/*
 * This header contains internal kernel time handling data structures
 * Do not use __mips macro here
 * Use ONLY definitions that are constant whether compiled mips2 or 3
 */

/*
 * IRIX4 versions of time interface structures
 *
 * IRIX4 and 5 timevals and itimervals are the same
 * IRIX4 has no timespec
 */

/*
 * IRIX5/Mips ABI versions of time interface structures
 */
struct irix5_itimerval {
	struct	irix5_timeval it_interval;	/* timer interval */
	struct	irix5_timeval it_value;	/* current value */
};


#define timeval_to_irix5(t,i5)	\
			(i5)->tv_sec = (app32_long_t)(t)->tv_sec; \
			(i5)->tv_usec = (app32_long_t)(t)->tv_usec;

#define irix5_to_timeval(t,i5)	\
			(t)->tv_sec = (i5)->tv_sec; \
			(t)->tv_usec = (i5)->tv_usec;
#define TIMESPEC_TO_IRIX5(t,i5)      \
			(i5)->tv_sec = (t)->tv_sec; \
			(i5)->tv_nsec = (t)->tv_nsec;

typedef app32_long_t	irix5_time_t;
typedef app64_int_t	irix5_64_time_t;
/* Irix5, 32bit ABI */
typedef struct irix5_timespec {
	irix5_time_t	tv_sec;		/* seconds */
	app32_long_t	tv_nsec;	/* and nanoseconds */
} irix5_timespec_t;

typedef struct irix5_64_timespec {
	app64_int_t  tv_sec;
    	app64_long_t tv_nsec;
} irix5_64_timespec_t;

#define	USEC_PER_SEC	1000000L	/* number of usecs for 1 second	*/
#define	NSEC_PER_SEC	1000000000L	/* number of nsecs for 1 second	*/
#define USEC_PER_TICK	(USEC_PER_SEC/HZ)
#define NSEC_PER_TICK	(NSEC_PER_SEC/HZ)
#define NSEC_PER_USEC	1000L

#ifdef _KERNEL

#define have_fastpriv(pp)	(KT_ISBASERT(UT_TO_KT(pp->p_proxy.prxy_threads)) && \
				 !KT_ISNBASEPRMPT(UT_TO_KT(pp->p_proxy.prxy_threads)))

#define kt_has_fastpriv(kt)	(KT_ISBASERT(kt) && !KT_ISNBASEPRMPT(kt))


	  /* 
	   * needs to be a realtime process.
	   */
#define need_fastimer(tvp, pp)	(have_fastpriv(pp))
#define kt_needs_fastimer(tvp, kt)	(kt_has_fastpriv(kt))

#define capable_of_fastimer(kt)	(kt_has_fastpriv(kt) || cap_able(CAP_TIME_MGT))
      

/*
 * Macro to round usec to sec in time timeval structure.
 */
#define	RNDTIMVAL(t) \
	{ \
             register struct timeval *tp = (t); \
             long round; \
	      if( tp->tv_usec >= USEC_PER_SEC ) \
		{ \
                   round = tp->tv_usec / USEC_PER_SEC;  \
			tp->tv_usec =tp->tv_usec - round * USEC_PER_SEC; \
			tp->tv_sec += round; \
		} \
	}

#define	RND_TIMESPEC_VAL(t) \
	{ \
		register timespec_t *tp = (t); \
                long round; \
                if(tp->tv_nsec >= NSEC_PER_SEC){ \
                  round = tp->tv_nsec / NSEC_PER_SEC; \
                  tp->tv_sec += round; \
                  tp->tv_nsec = tp->tv_nsec - round* NSEC_PER_SEC; \
                                              } \
	}

/*
 * macros to convert from timespec to timeval and vice-versa
 */
#define TIMESPEC_TO_TIMEVAL(ts, tv)	\
	{ \
		(tv)->tv_sec = (ts)->tv_sec; \
		(tv)->tv_usec = (ts)->tv_nsec/1000; \
	}
#define TIMEVAL_TO_TIMESPEC(tv, ts)	\
	{ \
		(ts)->tv_sec = (tv)->tv_sec; \
		(ts)->tv_nsec = (tv)->tv_usec*1000; \
	}

struct proc;
struct eframe_s;
struct rusage;

struct timespec;
struct timeval;
struct itimerval;
struct callout;
struct uthread_s;
struct callout_info;

extern int fastick;
extern time_t lastadjtime;	/* HZ since last adjtime(2) */
extern time_t lastadjtod;	/* for 1 hour after last adjtime(2) */
#define DIDADJTIME (3600*HZ)    /* keep TOD chip from fixing time */
	
extern void realitexpire(struct proc *);
extern long doadjtime(long);
extern void nanotime(struct timespec *);
extern void nanotime_syscall(struct timespec *);
extern void microtime(struct timeval *);
extern void irix5_microtime(struct irix5_timeval *);
extern void tick_to_timeval(int, struct timeval *, int);
extern void tick_to_timespec(int64_t, struct timespec *, int);
extern time_t fasthzto(struct timeval *);
extern time_t hzto(struct timeval *);
extern __int64_t timespec_to_tick(struct timespec *, int);
extern void timevalfix(struct timeval *);
extern void timevalsub(struct timeval *, struct timeval *);
extern void timevaladd(struct timeval *, struct timeval *);
extern int itimerfix(struct timeval *);
extern int itimerdecr(struct itimerval *, int);
extern void timespec_add(struct timespec *, struct timespec *);
extern void timespec_sub(struct timespec *, struct timespec *);

extern __psint_t query_cyclecntr(uint *);
extern int query_cyclecntr_size(void);
extern int query_fasttimer(void);
extern __int64_t chktimeout_tick(int, toid_t, void (*)(), void *);
extern __int64_t do_chktimeout_tick(struct callout_info *, toid_t , void (*)(), void *);
extern void	chktimedrift(void);
extern void	ackrtclock(void);
extern void	ackkgclock(void);
extern void	bump_leds(void);
extern int	loclkok(struct eframe_s *);
extern int	set_timer_intr(processorid_t, __int64_t, long);
extern void	settime(long, long);
extern void	local_settime(long, long);
extern void	enable_fastclock(void);
extern int	dosetitimer(int, struct itimerval *, struct itimerval *,
				struct uthread_s *);
extern int	setitimer_value(int, struct timeval *, struct timeval *);

extern time_t	get_realtime_ticks(void);
extern int	callout_time_to_hz(__int64_t, int, int);
extern int	settimetrim(int);
extern int	gettimetrim(void);
extern __int64_t tvtoclock(struct timeval *tv, int);
extern __int64_t tstoclock(struct timespec *, int);
extern void     delete_ptimers(void);
extern void	nano_delay(struct timespec *);
/* for tvtoclock */
#define TIME_REL 0		/* Convert the stucture to clock cycles*/
#define TIME_ABS 1		/* Convert the strucure to clock cycles and
				 * add in the current value of the clock */
extern void	startprfclk(void);
extern void	stopprfclk(void);
/*
 * Declarations for Unadjusted System Time (UST).
 * See ml/ust.s, ml/ust_conv.c
 */
extern unsigned long long ust_val;

extern unsigned int ust_freq;
extern unsigned int update_ust(void);
extern unsigned int update_ust_i(void);
extern void get_ust_nano(unsigned long long *);
extern void get_ust(unsigned long long *);
extern void ust_to_nano(unsigned long long, unsigned long long *);

extern void	setkgvector(void (*)());

#if _MIPS_SIM == _ABI64
extern int irix5_to_timeval_xlate(enum xlate_mode, void *,
				  int, struct xlate_info_s *);
extern int timeval_to_irix5_xlate(void *, int, struct xlate_info_s *);
#endif

/*
 * fastick_callback_required_flags and bit assignments (see locore.s)
 * if you update this, you must update it atomically using
 * atomicSetUint or atomicClearUint.
 */
extern unsigned fastick_callback_required_flags;

#define FASTICK_CALLBACK_REQUIRED_PRF_MASK       0x00000001
#define FASTICK_CALLBACK_REQUIRED_KDSP_MASK      0x00000002
#define FASTICK_CALLBACK_REQUIRED_ISDNAUDIO_MASK 0x00000004
#define FASTICK_CALLBACK_REQUIRED_MIDI_MASK      0x00000008
#define FASTICK_CALLBACK_REQUIRED_TSERIALIO_MASK 0x00000010

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_KTIME_H */
