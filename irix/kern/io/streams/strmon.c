/**************************************************************************
 *									  *
 * 		 Copyright (C) 1987-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#define SEMAMETER 1
#ident "$Revision: 1.11 $"

#include "sys/types.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/kthread.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/strmp.h"
#include "sys/ddi.h"
#include "sys/mon.h"
#include "sys/systm.h"
#include "ksys/sthread.h"

static void service_monq(mon_t *);
static int mon_runq(mon_t *);

/* XXXrs - easy to switch logging on and off... */
/* #define LOG_MON_ACT(a,b,c,d)	LOG_ACT(a,b,c,d) */
#define LOG_MON_ACT(a,b,c,d)

#ifdef DEBUG
#define NO_CPU_HOLDING -1
#endif /* DEBUG */

static int service_monq_cnt;	/* number of active service_monq */

/*
 * Initialize a monitor, should only be called once per monitor (no locking)
 */
void
initmonitor(mon_t *mon, char *name, mon_func_t *mf)
{
	char sname[32];
	extern int rmonqd_pri;

	/* Initialize locks */
	spinlock_init(&mon->mon_lock, name);
	sv_init(&mon->mon_wait, SV_DEFAULT, name);

	/* Assign the "private" functions */
	mon->mon_funcp = mf;

	/* Zero the queue */
	mon->mon_queue = NULL;

	mon->mon_lock_flags = 0;
	mon->mon_id = 0;
	mon->mon_trips = 0;
	mon->mon_monpp = NULL;
	mon->mon_monp_lock = NULL;

	/* Call private init routine */
	if (mon->mon_funcp->mf_init)
		(mon->mon_funcp->mf_init)(mon);

	sv_init(&mon->mon_sv, SV_DEFAULT, "monq_sv");
	sprintf(sname, "rmonqd(%d)", service_monq_cnt++);
	sthread_create(sname, 0, 0, 0, rmonqd_pri, KT_PS,
		(st_func_t *)service_monq, (void *) mon, 0, 0, 0);

	return;
}

/*
 * pmonitor - acquire monitor
 *	This will sleep waiting for the monitor
 * NOTE - if called at breakable priority, then monitor is automatically
 *	released
 * Returns 0 - if got monitor
 *	  -1 - if caught signal
 */
/* ARGSUSED */
int
pmonitor(register mon_t *monp, int pri, void *arg)
{
	uint64_t kid;
	kthread_t *k;
	int s;

	LOG_MON_ACT(AL_PMON, monp, 0, 0);
	k = curthreadp;
	if ((kid = k->k_id) == monp->mon_id) {
		ASSERT(k->k_monitor == monp);
		monp->mon_trips++;
		monp->mon_p_arg = arg;

		/* Call private p_mon */
		if (monp->mon_funcp->mf_p_mon)
			(*monp->mon_funcp->mf_p_mon)(monp);

		LOG_MON_ACT(AL_PMON_TRIP, monp, monp->mon_trips, 0);
		return 0;
	}

	ASSERT(k->k_monitor == NULL);

	LOCK_MONITOR(monp, s);
	while (monp->mon_lock_flags & (MON_LOCKED|MON_RUN|MON_DOSRV)) {
		/* monitor locked, wait for it to free */
		monp->mon_lock_flags |= MON_WAITING;
		sv_wait(&monp->mon_wait, PZERO|TIMER_SHIFT(AS_STRMON_WAIT),
			&monp->mon_lock, s);
		LOCK_MONITOR(monp, s);
	}
	monp->mon_lock_flags |= MON_LOCKED;
	UNLOCK_MONITOR(monp, s);

/* XXXnewlock - set the priority right above ???  Here's the old psema:
	if (psema(&monp->mon_sema, pri|TIMER_SHIFT(AS_STRMON_WAIT)) < 0)
		return(-1);
*/

	ASSERT(monp->mon_trips == 0);
	ASSERT(monp->mon_id == 0);
	ASSERT(monp->mon_monpp == NULL);
	ASSERT(monp->mon_monp_lock == NULL);
	ASSERT(k->k_monitor == NULL);
	k->k_monitor = monp;
	monp->mon_id = kid;
	monp->mon_p_arg = arg;

	/* Call private p_mon */
	if (monp->mon_funcp->mf_p_mon)
		(*monp->mon_funcp->mf_p_mon)(monp);

	LOG_MON_ACT(AL_PMON_COMP, monp, 0, 0);
	return(0);
}

/* ARGSUSED */
int
spinunlock_pmonitor(
	lock_t *lck,
	int ospl,
	register mon_t **monpp,
	int pri,
	void *arg)
{
	uint64_t kid;
	mon_t *monp;
	int s;
	kthread_t *k;

	k = curthreadp;

	monp = *monpp;
	ASSERT(monp);

	LOG_MON_ACT(AL_ULCK_PMON, monp, 0, 0);

	if ((kid = k->k_id) == monp->mon_id) {
		ASSERT(k->k_monitor == monp);
		monp->mon_trips++;
		monp->mon_p_arg = arg;
		mutex_spinunlock(lck, ospl);

		/* Call private p_mon */
		if (monp->mon_funcp->mf_p_mon)
			(*monp->mon_funcp->mf_p_mon)(monp);

		LOG_MON_ACT(AL_ULCK_PMON_TRIP, monp, monp->mon_trips, 0);
		return 0;
	}

	ASSERT(k->k_monitor == NULL);

	nested_spinlock(&monp->mon_lock);
	nested_spinunlock(lck);
	s = ospl;
	while (monp->mon_lock_flags & (MON_LOCKED|MON_RUN|MON_DOSRV)) {
		monp->mon_lock_flags |= MON_WAITING;
		sv_wait(&monp->mon_wait, PZERO|TIMER_SHIFT(AS_STRMON_WAIT),
			&monp->mon_lock, s);
		/*
		 * We might wake up on a new monitor so we have to
		 * reacquire the monitor pointer.
		 */
		if (pri == -1) return 1;
		s = mutex_spinlock(lck);
		monp = *monpp;
		ASSERT(monp);
		nested_spinlock(&monp->mon_lock);
		nested_spinunlock(lck);
	}
	monp->mon_lock_flags |= MON_LOCKED;
	UNLOCK_MONITOR(monp, s);

	ASSERT(monp->mon_trips == 0);
	ASSERT(monp->mon_id == 0);
	ASSERT(monp->mon_monpp == NULL);
	ASSERT(k->k_monitor == NULL);
	k->k_monitor = monp;
	monp->mon_id = kid;
	monp->mon_p_arg = arg;
	monp->mon_monpp = monpp;
	monp->mon_monp_lock = lck;

	/* Call private p_mon */
	if (monp->mon_funcp->mf_p_mon)
		(*monp->mon_funcp->mf_p_mon)(monp);

	LOG_MON_ACT(AL_ULCK_PMON_COMP, monp, 0, 0);
	return(0);
}

/* ARGSUSED */
int
spinunlock_qmonitor(
	register lock_t *lck,
	int ospl,
	register mon_t **monpp,
	register void *qentry,
	void *arg,
	int flag)
{
	register int s2;
	register mon_t *monp;
	void *priv;
	register mon_t *savemonp;

	monp = *monpp;
	LOG_MON_ACT(AL_ULCK_QMON, monp, qentry, arg);

	nested_spinlock(&monp->mon_lock);
	nested_spinunlock(lck);

	if (monp->mon_lock_flags & (MON_LOCKED|MON_RUN|MON_DOSRV)) {
		if (qentry) {
			/* Didn't get it, queue the entry to end */
			register void **p, **q;

			*(void **)qentry = NULL;
			if ((p = monp->mon_queue) != NULL) {
				for ( ; p != NULL; p = (void **)*p)
					q = p;

				*q = qentry;
			} else
				monp->mon_queue = (void **)qentry;
		}

		UNLOCK_MONITOR(monp, ospl);
		LOG_MON_ACT(AL_ULCK_QMON_Q, monp, qentry, arg);
		return(0);
	} else if (monp == streams_monp) {
		/*
		 * force anything on the global monitor to be processed by
		 * the monitor service thread.
		 */
		ASSERT(qentry);
		/*
		 * if we are trying to use the global monitor, then
		 * we will force the request to be serviced by the
		 * service_monq thread.
		 */
		if (qentry) {
			/* Didn't get it, queue the entry to end */
			register void **p, **q;

			*(void **)qentry = NULL;
			if ((p = monp->mon_queue) != NULL) {
				for ( ; p != NULL; p = (void **)*p)
					q = p;

				*q = qentry;
			} else
				monp->mon_queue = (void **)qentry;
		}
	}

	/* Got it */
	monp->mon_lock_flags |= MON_LOCKED;
	/*
	 * Save current thread monitor in case we are trying to use another
	 * monitor. We will restore it when we are done with the other
	 * monitor. We keep the monitor locked. If we recurse, we will
	 * see the monitor is locked and put the request on the monitor
	 * run queue.
	 */
	savemonp = curthreadp->k_monitor;
	ASSERT(!savemonp || (savemonp->mon_lock_flags & MON_LOCKED));
	curthreadp->k_monitor = monp;
	monp->mon_id = curthreadp->k_id;
	UNLOCK_MONITOR(monp, ospl);

	monp->mon_p_arg = arg;
	monp->mon_monpp = monpp;
	monp->mon_monp_lock = lck;

	/* Call private q_mon_sav */
	if (monp->mon_funcp->mf_q_mon_sav)
		(*monp->mon_funcp->mf_q_mon_sav)(monp, &priv);

	/*
	 * if it is the global monitor then we'll be service the request
	 * by the monitor service thread.
	 */
	if (monp != streams_monp) {
		/* Call the service routine with this entry */
		(*monp->mon_funcp->mf_service)(qentry);
	}

	/* run all queued requests - this leaves monitor lock locked */
	/*
	 * if we are the streams monitor, then we can not run any pending
	 * monitor requests. This is because they might try to obtain
	 * locks that are already held by the caller of streams_interrupt.
	 * We defer execution of them until after we are finished by firing
	 * off a timeout event that will call spinunlock_qmonitor from a
	 * different thread/process context.
	 */
	/*
	 * if we are a streams server thread, then we don't want to do
	 * this since we are already servicing the mon_runq.
	 */
	LOCK_MONITOR(monp, s2);
	if ((curthreadp->k_flags & KT_SERVER) == NULL ||
	   ((monp->mon_lock_flags & (MON_RUN|MON_DOSRV)) == NULL) ||
	   ((monp == streams_monp) && !(curthreadp->k_flags & KT_SERVER))) {
		if (!monp->mon_queue) {
			/*
			 * Nothing to do ... don't bother with a sv_signal
			 * give up the monitor, signal waiters and return
			 */

			/* Call private q_mon_rst */
			if (monp->mon_funcp->mf_q_mon_rst)
				(*monp->mon_funcp->mf_q_mon_rst)(monp, priv);

			monp->mon_p_arg = NULL;
			monp->mon_monpp = NULL;
			monp->mon_monp_lock = NULL;

			monp->mon_lock_flags &= ~MON_LOCKED;
			curthreadp->k_monitor = savemonp;
			monp->mon_id = 0;
			if (monp->mon_lock_flags & MON_WAITING) {
				monp->mon_lock_flags &= ~MON_WAITING;
				sv_broadcast(&monp->mon_wait);
			}
			UNLOCK_MONITOR(monp, s2);
			return (1);
		}

		/* Call private q_mon_rst */
		if (monp->mon_funcp->mf_q_mon_rst)
			(*monp->mon_funcp->mf_q_mon_rst)(monp, priv);

		monp->mon_lock_flags |= MON_DOSRV;
		curthreadp->k_monitor = savemonp;
		monp->mon_id = 0;

		/* if server thread already running don't try starting it */
		if (!(monp->mon_lock_flags & MON_RUN))
			sv_signal(&monp->mon_sv);

		UNLOCK_MONITOR(monp, s2);
		return (1);
	}

	/* Release spinlock and monitor.
	 *
	 * Hold the spin lock until after releasing the monitor so no one will
	 * queue something up after we look at the list and before we actually
	 * release the monitor.
	 */

	/* Call private q_mon_rst */
	if (monp->mon_funcp->mf_q_mon_rst)
		(*monp->mon_funcp->mf_q_mon_rst)(monp, priv);

	monp->mon_p_arg = NULL;
	monp->mon_monpp = NULL;
	monp->mon_monp_lock = NULL;

	monp->mon_lock_flags &= ~MON_LOCKED;
	curthreadp->k_monitor = savemonp;
	monp->mon_id = 0;
	if (monp->mon_lock_flags & MON_WAITING) {
		monp->mon_lock_flags &= ~MON_WAITING;
		sv_broadcast(&monp->mon_wait);
	}
	UNLOCK_MONITOR(monp, s2);
	return(1);
}


/*
 * vmonitor - release monitor
 * It also runs any queued requests
 */
void
vmonitor(register mon_t *monp)
{
	kthread_t *k = curthreadp;
	register int s2;

	ASSERT(k->k_monitor == monp);
	ASSERT(k->k_id == monp->mon_id);

	LOG_MON_ACT(AL_VMON, monp, 0, 0);

	if (monp->mon_trips > 0) {
		monp->mon_trips--;
		LOG_MON_ACT(AL_VMON_TRIP, monp, monp->mon_trips, 0);
		return;
	}

	/*
	 * if we are a streams server thread, then we don't want to
	 * try to process the mon_runq since we're doing it already.
	 */
	LOCK_MONITOR(monp, s2);
	if ((curthreadp->k_flags & KT_SERVER) == NULL ||
	    (monp->mon_lock_flags & (MON_RUN|MON_DOSRV)) == NULL) {
		if (!monp->mon_queue) {
			/*
			 * Nothing to do ... don't bother with a sv_signal
			 * give up the monitor, signal waiters and return
			 */

			/* Call private v_mon */
			if (monp->mon_funcp->mf_v_mon)
				(*monp->mon_funcp->mf_v_mon)(monp);

			monp->mon_p_arg = NULL;
			monp->mon_monpp = NULL;
			monp->mon_monp_lock = NULL;

			monp->mon_lock_flags &= ~MON_LOCKED;
			monp->mon_id = 0;
			k->k_monitor = NULL;
			if (monp->mon_lock_flags & MON_WAITING) {
				monp->mon_lock_flags &= ~MON_WAITING;
				sv_broadcast(&monp->mon_wait);
			}
			UNLOCK_MONITOR(monp, s2);
			return;
		}

		/* Call private v_mon */
		if (monp->mon_funcp->mf_v_mon)
			(*monp->mon_funcp->mf_v_mon)(monp);

		monp->mon_lock_flags |= MON_DOSRV;
		k->k_monitor = NULL;
		monp->mon_id = 0;

		/* if server thread already running don't try starting it */
		if (!(monp->mon_lock_flags & MON_RUN))
			sv_signal(&monp->mon_sv);

		UNLOCK_MONITOR(monp, s2);
		return;
	}

	/* Release spinlock and monitor.
	 *
	 * Hold the spin lock until after releasing the monitor so no one will
	 * queue something up after we look at the list and before we actually
	 * release the monitor.
	 */

	/* Call private v_mon */
	if (monp->mon_funcp->mf_v_mon)
		(*monp->mon_funcp->mf_v_mon)(monp);

	monp->mon_id = 0;
	monp->mon_p_arg = NULL;
	monp->mon_monpp = NULL;
	monp->mon_monp_lock = NULL;
	k->k_monitor = NULL;

	monp->mon_lock_flags &= ~MON_LOCKED;
	if (monp->mon_lock_flags & MON_WAITING) {
		monp->mon_lock_flags &= ~MON_WAITING;
		sv_broadcast(&monp->mon_wait);
	}
	UNLOCK_MONITOR(monp, s2);
	return;
}

/*
 * rmonqd's are sv_signal-ed to run service_monq on a monitor. A
 * separate thread runs the monitor queue, so that it can safely
 * sleep in servicing the queue.
 */
void
service_monq(mon_t *monp)
{
	register int s2;

	/*
	 * mark thread as a server so we don't call
	 * mon_runq from inside rmonitor.
	 */
	curthreadp->k_flags |= KT_SERVER;

	LOCK_MONITOR(monp, s2);
	for (;;) {
		ASSERT(curthreadp->k_monitor == NULL);
		if (!(monp->mon_lock_flags & MON_DOSRV)) {
			sv_wait(&monp->mon_sv, PZERO|TIMER_SHIFT(AS_STRMON_WAIT),
				&monp->mon_lock, s2);
			LOCK_MONITOR(monp, s2);
		}

		ASSERT(monp->mon_lock_flags & MON_DOSRV);
		ASSERT(!(monp->mon_lock_flags & MON_RUN));
		monp->mon_lock_flags &= ~MON_DOSRV;
		monp->mon_lock_flags |= MON_RUN;
		UNLOCK_MONITOR(monp, s2);

		ASSERT(monp);	/* there should be something to do */

		curthreadp->k_monitor = monp;
		monp->mon_id = curthreadp->k_id;
		monp->mon_monpp = NULL;
		monp->mon_monp_lock = NULL;

		if (monp->mon_funcp->mf_p_mon)
			(*monp->mon_funcp->mf_p_mon)(monp);

		ASSERT(monp->mon_lock_flags & MON_LOCKED);
		ASSERT(!monp->mon_trips);	/* already cleared */

		s2 = mon_runq(monp);

		monp->mon_id = 0;
		monp->mon_p_arg = NULL;
		curthreadp->k_monitor = NULL;

		monp->mon_lock_flags &= ~MON_LOCKED;
		monp->mon_lock_flags &= ~MON_RUN;
		if (monp->mon_lock_flags & MON_WAITING) {
			monp->mon_lock_flags &= ~MON_WAITING;
			sv_broadcast(&monp->mon_wait);
		}
	}
}

/*
 * rmonitor - release monitor - this is called by those places that put
 * processes to sleep or wish to totally release the
 * monitor. It fills in all the state required and
 * releases the monitor
 */
void
rmonitor(mon_t *monp, mon_state_t *msp)
{
	int s;

	ASSERT(curthreadp->k_id == monp->mon_id);
	ASSERT(curthreadp->k_monitor == monp);

	/*
	 * if we are a rmonqd(), then we kept the monitor
	 * over the sleep.
	 */
	if ((curthreadp->k_flags & KT_SERVER) != NULL) {
		return;
	}

	msp->ms_id = monp->mon_id;
	msp->ms_trips = monp->mon_trips;
	msp->ms_p_arg = monp->mon_p_arg;
	if (msp->ms_monp_lock = monp->mon_monp_lock) {
		msp->ms_monpp = monp->mon_monpp;
	} else {
		msp->ms_monpp = NULL;
		msp->ms_mon = monp;
	}
	monp->mon_trips = 0;

	/* Call private r_mon */
	if (monp->mon_funcp->mf_r_mon)
		(*monp->mon_funcp->mf_r_mon)(monp, msp);

	LOG_MON_ACT(AL_RMON, monp, 0, 0);

	/* Call private v_mon */
	if (monp->mon_funcp->mf_v_mon)
		(*monp->mon_funcp->mf_v_mon)(monp);

	monp->mon_id = 0;
	curthreadp->k_monitor = NULL;

	LOCK_MONITOR(monp, s);
	/*
	 * If we are the mon_runq server thread then we don't want
	 * to fire off another server thread since we are already
	 * processing the mon_runq.
	 */
	if ((!monp->mon_queue) ||
	    (curthreadp->k_flags & KT_SERVER) != NULL ||
	    (monp->mon_lock_flags & (MON_RUN|MON_DOSRV)) != NULL) {
		/*
		 * Nothing to do ... don't bother with a sv_signal
		 * give up the monitor, signal waiters and return
		 */
		monp->mon_p_arg = NULL;
		monp->mon_monpp = NULL;
		monp->mon_monp_lock = NULL;

		monp->mon_lock_flags &= ~MON_LOCKED;
		if (monp->mon_lock_flags & MON_WAITING) {
			monp->mon_lock_flags &= ~MON_WAITING;
			sv_broadcast(&monp->mon_wait);
		}
		UNLOCK_MONITOR(monp, s);
		return;
	}

	monp->mon_lock_flags |= MON_DOSRV;

	/* if server thread already running don't try starting it */
	if (!(monp->mon_lock_flags & MON_RUN))
		sv_signal(&monp->mon_sv);

	UNLOCK_MONITOR(monp, s);
}

/*
 * amonitor - acquire monitor - this is called by those places that put
 * processes to sleep. It sets monitor back to state passed in.
 */
int
amonitor(mon_state_t *msp)
{
	int rv, s;
	mon_t *monp;

	/*
	 * if we are a rmonqd(), then we kept the monitor
	 * over the sleep.
	 */
	if ((curthreadp->k_flags & KT_SERVER) != NULL) {
		return 0;
	}

	ASSERT(msp->ms_id == curthreadp->k_id);

	if (msp->ms_monp_lock) {
		s = mutex_spinlock(msp->ms_monp_lock);
		monp = *msp->ms_monpp;
		LOG_MON_ACT(AL_AMON, monp, msp->ms_monp_lock, msp->ms_p_arg);
		rv = spinunlock_pmonitor(msp->ms_monp_lock, s,
					 msp->ms_monpp, PZERO,
					 msp->ms_p_arg);
		/*
		 * Can get swapped during the above so we need to reacquire
		 * the monitor pointer.
		 */
		monp = *msp->ms_monpp;
	} else {
		monp = msp->ms_mon;
		LOG_MON_ACT(AL_AMON, monp, 0, msp->ms_p_arg);
		rv = pmonitor(monp, PZERO, msp->ms_p_arg);
	}

	/* XXXrs - Note to self: the following assert is temporary for
	 *         testing.  If you don't remember what it's for, it's
	 *         safe to remove it.
	 */
	ASSERT(monp);
	ASSERT(monp->mon_funcp);
	ASSERT(!(monp->mon_funcp->mf_a_mon));

	if (rv == 0) {
		/* Call private a_mon */
		if (monp->mon_funcp->mf_a_mon)
			(*monp->mon_funcp->mf_a_mon)(monp, msp);
		monp->mon_trips = msp->ms_trips;
	}
	return rv;
}

/*
 * allocmonitor - called to allocate a monitor structure.
 */
mon_t *
allocmonitor(char *name, mon_func_t *mf)
{
	mon_t *mon;

	ASSERT(mf);
	ASSERT(name);

	if (!(mon = kmem_alloc(sizeof(struct monitor), KM_NOSLEEP)))
		return(NULL);

	initmonitor(mon, name, mf);
	return(mon);
}

/*
 * freemonitor - called to free a monitor structure.
 */
/*
 * XXXrs - Should this routine take care of anyone who might be sleeping on
 *         the monitor?  Should it be possible for anyone to be
 *	   sleeping on the monitor??
 *
 * 	   Yes and yes.
 *
 */
void
freemonitor(mon_t *mon)
{
	spinlock_destroy(&mon->mon_lock);
	sv_broadcast(&mon->mon_wait);
	sv_destroy(&mon->mon_wait);
	kmem_free(mon, sizeof(struct monitor));
	return;
}

/*
 * Run queued requests.  Must be called with the monitor
 * held.  Returns with monitor's lock locked.
 */
int
mon_runq(register mon_t *monp)
{
	register void **qentry;
	register int s;

	LOG_MON_ACT(AL_MON_RUNQ, monp, 0, 0);

	/* Get spinlock for mucking with interrupt list */
	LOCK_MONITOR(monp, s);

	/*
	 * XXXrs - have to update the per-cpu array with each entry's
	 *         stream head pointer from the queue stashed in each entry.
	 *	   This will be done by the service routine that knows
	 *	   how to decode the entry...
	 */
	while ((qentry = monp->mon_queue) != NULL) {
		/* Remove first entry */
		monp->mon_queue = (void **)*qentry;

		/* Release lock and call function, obtaining lock on return */
		UNLOCK_MONITOR(monp, s);

		(*monp->mon_funcp->mf_service)((void *)qentry);

		LOCK_MONITOR(monp, s);
	}
	return(s);
}
