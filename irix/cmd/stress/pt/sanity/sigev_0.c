/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <mutex.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

volatile unsigned long	threadsStarted;

/* ------------------------------------------------------------------ */

/* Caution: Alter these values with care.
 * Since only one notification may be outstanding at a time we
 * need to throttle the timer expirations down to the timer
 * granularity/ frequency of checking for signals.
 * We assume 10ms will be low enough.
 */
#define INTERVAL_NS	(100*1000*1000)	/* 100ms */
#define TIMERS		10		/* timeouts 1/10ms */
#define TST_DURATION_S	10


void
timer_thread(union sigval sv)
{
	int	*ctr = (int *)sv.sival_ptr;

	TstTrace("Running %#x %p\n", pthread_self(), sv.sival_ptr);
	test_then_add((unsigned long *)&threadsStarted, 1);
	(*ctr)++;
	return;
}


TST_BEGIN(timerSigev)
{
	int		p;
	timer_t		*tmrs;
	int		*ctrs;
	int		*lost;
	sigevent_t	sigev;
	itimerspec_t	it;
	timespec_t	now;
	int		events;
	int		events_lost = 0;

	ChkPtr( tmrs = calloc(TIMERS, sizeof(timer_t)), != 0 );
	ChkPtr( ctrs = calloc(TIMERS, sizeof(int)), != 0 );
	ChkPtr( lost = calloc(TIMERS, sizeof(int)), != 0 );

	/* Create timers with thread notification.
	 */
	sigev.sigev_notify = SIGEV_THREAD;
	sigev.sigev_notify_function = timer_thread;
	sigev.sigev_notify_attributes = 0;
	sigev.sigev_signo = 0;
	for (p = 0; p < TIMERS; p++) {
		sigev.sigev_value.sival_ptr = ctrs + p;
		CHKInt( timer_create(CLOCK_REALTIME, &sigev, &tmrs[p]),
			== 0 );
	}

	/* Arm timers and set reload value.
	 * Wait to allow several expirations.
	 */
	CHKInt( clock_gettime(CLOCK_REALTIME, &now), == 0 );
	it.it_value = now;
	it.it_value.tv_sec++;
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_nsec = INTERVAL_NS;

	threadsStarted = 0;
	for (p = 0; p < TIMERS; p++) {
		/* stagger timeouts */
		it.it_value.tv_nsec = p * (INTERVAL_NS/TIMERS);
		CHKInt( timer_settime(tmrs[p], TIMER_ABSTIME, &it, 0),
			== 0 );
	}
	CHKInt( sleep(TST_DURATION_S+2), == 0 );

	/* Attempt to handle overruns
	 */
	for (p = 0; p < TIMERS; p++) {
		lost[p] = timer_getoverrun(tmrs[p]);
		events_lost += lost[p];
	}

	/* Disarm and destroy timers.
	 */
	it.it_value.tv_sec = 0;
	it.it_value.tv_nsec = 0;
	for (p = 0; p < TIMERS; p++) {
		CHKInt( timer_settime(tmrs[p], TIMER_ABSTIME, &it, 0),
			== 0 );
		CHKInt( timer_delete(tmrs[p]), == 0 );
	}

	/* Check total events and per-timer events.
	 * Warning:
	 *	Overrun counts (lost events) are only counted for the
	 *	first DELAYTIMER_MAX (32).
	 */
	events = TIMERS * (TST_DURATION_S*(1000000000/INTERVAL_NS));
	TstInfo("Total events %d [%d-%d]\n",
		threadsStarted, events, events_lost);
	ChkExp( threadsStarted >= events - events_lost,
		("Missing total events %d [%d-%d]\n",
		threadsStarted, events, events_lost) );

	events = TST_DURATION_S*(1000000000/INTERVAL_NS);
	for (p = 0; p < TIMERS; p++) {
		TstInfo("Events timer-%d %d [%d-%d]\n",
			p, ctrs[p], events, lost[p]);
		ChkExp( ctrs[p] >= events - lost[p],
			("Missing events timer-%d %d [%d-%d]\n",
			 p, ctrs[p], events, lost[p]) );
	}

	free(tmrs);
	free(ctrs);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise SIGEV_THREAD events" )

	TST( timerSigev, "Create thread based on POSIX timers", 0 ),

TST_FINISH

