/******************************************************************
 *
 *  SpiderX25 - Timing Module
 *
 *  Copyright 1991 Spider Systems Limited
 *
 *  TIMER.C
 *
 *    Fast multiple time-out module.
 *
 ******************************************************************/

/*
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/sys/timer/0/s.timer.c
 *	@(#)timer.c	1.25
 *
 *	Last delta created	10:07:35 10/26/92
 *	This file extracted	14:55:47 11/13/92
 *
 */



#define STATIC static
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/strmp.h>
#include <sys/snet/timer.h>

#define NTICKS	(HZ/20)

/* Length of tick ring */
#define TCYCLE  100

STATIC x25_timer_t *tring[TCYCLE];
STATIC int curtinx, ticking;
/* STATIC int cur_timeout; */
STATIC void do_set_timer(register x25_timer_t *,ushort);

STATIC int s_xtimer_spl;

extern activate_timer(register x25_timer_t *, register ushort);
extern attach_timer(register x25_timer_t *, register x25_timer_t **);

/*
 * 'Init_timers' initialises the set of associated timers.
 * The timers concerned must either never have been used, or
 * be guaranteed not to be attached to the main timing ring.
 * Function 'expfunc(exparg)' will be called whenever any timer
 * expires and is moved on to the 'expired' list.
 */
init_timers(theadp, tmtab, n, expfunc, exparg)
thead_t *theadp;
x25_timer_t  tmtab[];
int      n;
void   (*expfunc)();
caddr_t  exparg;
{
	int i;
	void step_time();

	/* Set up timer header */
	theadp->th_expired = NULL;
	theadp->th_expfunc = expfunc;
	theadp->th_exparg = exparg;

	/* Set up individual timers */
	for (i = 0; i<n; i++) {
		register x25_timer_t *tp = &tmtab[i];
		int offset = (char *)tp - (char *)theadp;

		if (offset>255)
			cmn_err(CE_PANIC, "Net timer offset > 255\n");

		tp->tm_id = i;
		tp->tm_offset = offset;
	}

	if (!ticking) {   /* Start clock ticking on first entry */
/*		cur_timeout = STREAMS_TIMEOUT(step_time, NULL, NTICKS); */
		(void) STREAMS_TIMEOUT(step_time, NULL, NTICKS);
		ticking = 1;
	}
	return 0;
}

/*
 * 'Stop_timers' stops the entire set of associated timers.
 */
/* ARGSUSED */
int
stop_timers(theadp, tmtab, n)
thead_t *theadp;
x25_timer_t  tmtab[];
int      n;
{
	int i;

	/* Stop individual timers */
	for (i = 0; i<n; i++)
		set_timer(&tmtab[i], 0);
	return 0;
}

/*
 * Set_timer stops the specified timer (time == 0) or (re)activates it
 *	to expire at the given time (time > 0, in ticks).
 *
 * If the timer was already running, or its expiry has not yet processed,
 *	it will be stopped or re-started.
 */
set_timer(register x25_timer_t *tp, ushort time)
{
	s_xtimer_spl = splclock();

	do_set_timer(tp, time);

	splx(s_xtimer_spl);
	return 0;
}

/*
 * Do the work of set_timer().
 */
static void
do_set_timer(register x25_timer_t *tp, ushort time)
{
	if (tp->tm_back) {
		/* Timer is attached to a ring slot or expired list: detach it */
		register x25_timer_t *nextp;

		if (nextp = tp->tm_next)
			nextp->tm_back = tp->tm_back;
		*tp->tm_back = nextp;
	}

	if (time)
		activate_timer(tp, time);
	else {   /* Leave timer tidily detached */
		tp->tm_next = NULL;
		tp->tm_back = NULL;
	}
}

/*
 * 'Run_timer' starts the specified timer to expire at the given time
 *  (time > 0, in ticks).
 * If the timer was already running, or its expiry has not yet processed,
 *  its status will not be changed.
 */
run_timer(x25_timer_t *tp, ushort_t time)
{
	s_xtimer_spl = splclock();

	if (time && !tp->tm_back)
		activate_timer(tp, time);

	splx(s_xtimer_spl);
	return 0;
}

/*
 * 'Step_time' is called at regular intervals to record the passage of time.
 *
 * On each call the current time variable is stepped, and any
 *  expired timers moved on to their own 'expired' lists.
 *
 * The function 'expfunc' specified to 'init_timers' is called to
 *  schedule processing for any expired timers.
 *
 * 'Step_time' is normally called indirectly from the clock ISR
 *  by way of the 'timeout' function.
 *
 */
void step_time()
{   /* Obeyed within the clock ISR: steps time by one tick */
	register x25_timer_t *tp;

	/* Step curtinx round ring */
	if (++curtinx >= TCYCLE)
		curtinx = 0;

	if (tp = tring[curtinx]) {
		/* There are timers to be dealt with */
		x25_timer_t *nextp;

		/* We clear the ring entry now as it may be used below */
		tring[curtinx] = NULL;

		do {   /* Deal with the chain of timers in order */
			nextp = tp->tm_next;
			tp->tm_back = NULL;

			/* Not really expired, so restart for time left */
			if (tp->tm_left)
				do_set_timer(tp, tp->tm_left);
			else {   /* Timer has really expired */
				register thead_t *theadp =
				(thead_t *)((char *)tp - tp->tm_offset);

				/* Push expired timer on to its own 'expired' chain */
				attach_timer(tp, &theadp->th_expired);

				/* Tell owner about expiry */
				(*theadp->th_expfunc)(theadp->th_exparg);

#ifdef DEVMT
				/* Count expiries */
				expired++;
#endif
			}
		} while (tp = nextp);
	}
	/* Request next step */
/* 	cur_timeout = STREAMS_TIMEOUT(step_time, NULL, NTICKS); */
	(void) STREAMS_TIMEOUT(step_time, NULL, NTICKS);
}

/*
 * 'Attach_timer' is an inner function which is used to attach a timer cell
 * either to one of the slots on the main timing ring, or to its expired
 * list.
 */
attach_timer(tp, tlistp)
register x25_timer_t *tp;
register x25_timer_t **tlistp;
{
	if (*tlistp)
		(*tlistp)->tm_back = &tp->tm_next;
	tp->tm_next = *tlistp;
	tp->tm_back = tlistp;
	*tlistp = tp;
	return 0;
}


/*
 * 'Activate_timer' is an inner procedure which chooses the
 * correct time ring entry for expiry of the given timer
 * (which must be unattached) at the specified time,
 * then attaches the cell to this entry.
 * If the time to expiry is more than the length of the
 * time ring, the cell is attached to the "time=0" entry
 * which implies one complete cycle of the ring; the 'time_left'
 * field is then set to the remaining time.
 * After the ring cycle is complete, such a cell will be processed
 * at the same time as those cells which have expired, but will
 * be restarted for the remaining time instead of being
 * transferred to the expired list.
 * This "full-cycle" action will be repeated until the time
 * left is within normal range of the time ring, at which point
 * the cell will be processed for the remaining time as a normal
 * expiring cell.
 */
activate_timer(register x25_timer_t *tp, register ushort time)
{
	if (time<TCYCLE)   /* In range of time ring */
		tp->tm_left = 0;
	else {   /* Not in range: set up full cycle */
		tp->tm_left = time - TCYCLE;
		time = 0;
	}

	if ((time += curtinx) >= TCYCLE)
		time -= TCYCLE;

	attach_timer(tp, &tring[time]);
	return 0;
}
