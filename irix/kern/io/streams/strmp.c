/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1989-1993 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * STREAMS monitor for MP systems
 *
 * $Revision: 1.75 $
 */
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/idbgactlog.h>
#include <sys/kthread.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/strmp.h>
#include <sys/strsubr.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <ksys/sthread.h>

extern int tpihold(void *, void **, void *);


extern int nstrintr;			/* size of interrupt block pool */
struct strintr *strintrfree;		/* free list pointer */
struct strintr *strintrrsrv;		/* reserved list pointer */

int streams_intr_avail;			/* Number of available interrupts */
int streams_intr_lowat;			/* Danger level */
int streams_intr_ready = 0;

#ifdef DEBUG
int streams_intr_inuse = 0;		/* Count of interrupt blocks in use */
int streams_intr_inuse_max = 0;		/* Highest ever in use count */
int streams_intr_rsrvd = 0;		/* Count of interrupt blocks reserved */
int streams_intr_rsrvd_max = 0;		/* Highest ever reserved count */
int streams_intr_lost = 0;
int streams_timeout_lost = 0;
#endif /* DEBUG */

lock_t strhead_monp_lock;	/* Global stream head monitor pointer lock */
lock_t str_intr_lock;		/* Global streams interrupt lock */
lock_t str_resource_lock;	/* Global streams resource lock */

mon_t streams_mon;			/* STREAMS monitor */
mon_t *streams_monp = &streams_mon;	/* Pointer to the above */

mon_t *privmon_list;
int privmon_cnt;
extern int strpmonmax;

queue_t globmon_q;			/* Queue whose monpp is streams_monp */
queue_t *globmon_qp = &globmon_q;	/* Pointer to the above */

void	streams_service(void *);
void	streams_p_mon(struct monitor *);
void	streams_v_mon(struct monitor *);
void	streams_q_mon_sav(struct monitor *, void **);
void	streams_q_mon_rst(struct monitor *, void *);

/* STREAMS-specific monitor functions */

mon_func_t streams_mon_func = {
	NULL,
	streams_service,	/* processes queue entries on V */
	streams_p_mon,		/* called just after monitor P-ed */
	streams_v_mon,		/* called just before monitor V-ed */
	streams_q_mon_sav,
	streams_q_mon_rst,
	NULL,			/* called when monitor released for sleep */
	NULL			/* called when monitor acquired on wakeup */
};

struct streams_mon_data streams_globmon_data;
char qqrun = 0;			/* != 0 if queuerun has been queued */
sv_t joinwait;			/* wait here for other joins to complete */

void
str_mp_init()
{
	register int i, s;
	struct strintr *intr;

	spinlock_init(&str_intr_lock, "strintr");
	spinlock_init(&strhead_monp_lock, "strmonp");
	spinlock_init(&str_resource_lock, "strrsrc");

	STREAMS_BLOCK_INTR(s);
	strintrfree = NULL;

	for (i = 0; i < nstrintr; i++) {
		intr = kmem_zalloc(sizeof(struct strintr), KM_NOSLEEP);
		if (!intr)
			break;
		intr->next = strintrfree;
		strintrfree = intr;
	}

	if (i != nstrintr)
		cmn_err(CE_WARN, "!STREAMS interrupt block init failure.");

	strintrrsrv = NULL;
	streams_intr_avail = i;
	streams_intr_lowat = i/10;
	streams_intr_ready = 1;

	STREAMS_UNBLOCK_INTR(s);

	streams_globmon_data.smd_flags = 0;
	streams_globmon_data.smd_assoc_cnt = 0;
	streams_globmon_data.smd_headp = NULL;
	streams_globmon_data.smd_tailp = NULL;
	streams_globmon_data.smd_joining = NULL;

        initmonitor(&streams_mon, "STRGLOB", &streams_mon_func);
	streams_mon.mon_private = &streams_globmon_data;

	globmon_q.q_monpp = &streams_monp;
	privmon_list = NULL;
	privmon_cnt = 0;

	sv_init(&joinwait, SV_DEFAULT, "STRJOIN");

	return;
}

/*
 * Link the stream head to the monitor's association list.
 * The global monitor pointer lock must be held by the caller!
 */
static void
str_mon_link(struct stdata *stp, mon_t *mon)
{
	struct streams_mon_data *smd;

	ASSERT(stp);
	ASSERT(mon);

	smd = (struct streams_mon_data *)mon->mon_private;

	ASSERT(smd);

	if (smd->smd_headp == NULL)
		smd->smd_headp = stp;
	stp->sd_assoc_prev = smd->smd_tailp;
	stp->sd_assoc_next = NULL;
	if (smd->smd_tailp)
		smd->smd_tailp->sd_assoc_next = stp;
	smd->smd_tailp = stp;
	stp->sd_monp = mon;
	smd->smd_assoc_cnt++;
}

/*
 * Unlink the stream head from the monitor's association list.
 * The global monitor pointer lock must be held by the caller!
 */
static void
str_mon_unlink(struct stdata *stp, mon_t *mon)
{
	struct streams_mon_data *smd;
#ifdef DEBUG
	struct stdata *stpfoo;
#endif

	ASSERT(stp);
	ASSERT(mon);

	smd = (struct streams_mon_data *)mon->mon_private;

	ASSERT(smd);
	ASSERT(smd->smd_assoc_cnt > 0);

#ifdef DEBUG
	stpfoo = smd->smd_headp;
	while (stp != stpfoo) {
		stpfoo = stpfoo->sd_assoc_next;
		ASSERT(stpfoo);
	}
#endif
	stp->sd_monp = NULL;
	if (--smd->smd_assoc_cnt == 0) {
		smd->smd_headp = smd->smd_tailp = NULL;
		stp->sd_assoc_prev = stp->sd_assoc_next = NULL;
		return;
	}
	if (smd->smd_headp == stp)
		smd->smd_headp = stp->sd_assoc_next;
	if (smd->smd_tailp == stp)
		smd->smd_tailp = stp->sd_assoc_prev;
	if (stp->sd_assoc_prev)
		stp->sd_assoc_prev->sd_assoc_next = stp->sd_assoc_next;
	if (stp->sd_assoc_next)
		stp->sd_assoc_next->sd_assoc_prev = stp->sd_assoc_prev;
}

/*
 * Attach the passed stream to the global monitor.
 * The global monitor pointer lock must be held by the caller!
 */
void
str_globmon_attach(struct stdata *stp)
{
	ASSERT(spinlock_islocked(&strhead_monp_lock));
	ASSERT(stp->sd_monp == NULL);
	str_mon_link(stp, streams_monp);
	return;
}

/*
 * The global monitor pointer lock must be held by the caller!
 */
int
str_privmon_attach(struct stdata *stp, mon_t *mon)
{
	ASSERT(spinlock_islocked(&strhead_monp_lock));
 
	/*
	 * Attach the given monitor to the given stream, and put the
	 * stream head into the association list;
	 */
	ASSERT(stp->sd_monp == NULL);
	str_mon_link(stp, mon);

	return 0;
}

mon_t *
str_mon_detach(struct stdata *stp)
{
	mon_t *mon;

	ASSERT(spinlock_islocked(&strhead_monp_lock));

	/*
	 * Detach the given stream head from its monitor and remove it from
	 * the monitor's association list.
	 */
	mon = stp->sd_monp;
	ASSERT(mon);
	str_mon_unlink(stp, mon);

	return mon;
}

/*
 * XXXrs - For now, just pre-allocate a fixed number of monitors.
 *         There should be a default number (based upon the number of
 *	   cpus) and a tunable parameter which overrides if set non-zero.
 *
 * The global monitor pointer lock must be held by the caller.
 */
mon_t *
str_privmon_alloc()
{
	mon_t *mon, *bestmon;
	int bestcnt;
	struct streams_mon_data *smd;

	/* XXXrs - change to pick the least *active* rather than
	 *	   the least crowded?  Or at least factor activity
	 *	   into the equation.
	 */
	/*
	 * Allocate a private monitor.
	 */
	if (!strpmonmax) /* Not allowed */
		return NULL;
	if (privmon_cnt < strpmonmax) {	/* Can we make a new one? */
        	if ((mon = allocmonitor("STRPRIV", &streams_mon_func)) == NULL)
			return NULL;
		if ((smd = kmem_alloc(sizeof(struct streams_mon_data),
				      KM_NOSLEEP)) == NULL) {
			freemonitor(mon);
			return NULL;
		}
		smd->smd_tailp = smd->smd_headp = NULL;
		smd->smd_assoc_cnt = 0;
		smd->smd_flags = SMD_PRIVATE;
		smd->smd_joining = NULL;
		mon->mon_private = (void *)smd;
		privmon_cnt++;
		if (privmon_list)
			privmon_list->mon_prev = mon;
		mon->mon_next = privmon_list;
		mon->mon_prev = NULL;
		privmon_list = mon;
		return(mon);
	}
	/*
	 * We've allocated the maximum allowed, select the least
	 * crowded and use that one.
	 */
	ASSERT(privmon_list);
	bestmon = privmon_list;
	smd = (struct streams_mon_data *)bestmon->mon_private;
	bestcnt = smd->smd_assoc_cnt;
	for (mon = bestmon->mon_next; mon; mon = mon->mon_next) {
		smd = (struct streams_mon_data *)mon->mon_private;
		if (smd->smd_assoc_cnt < bestcnt) {
			bestmon = mon;
			bestcnt = smd->smd_assoc_cnt;
		}
	}
	mon = bestmon;
	ASSERT(mon);
	return(mon);
}

/*
 * XXXrs - don't actually do anything.  We'll just allocate up to the
 *	   maximum number of monitors and then recycle.  Actually
 *	   freeing a monitor structure appears to cause problems because
 *	   it looks like there are race conditions between open/close
 *	   that result in processes waiting in open on monitors that are
 *	   about to be freed by close.  I would have thought that the global
 *	   monitor pointer lock would have closed this race, but there must
 *	   be some bug.  Anyway, just not ever freeing a monitor seems to
 *	   keep things safe for now until I can figure out what's really
 *	   going on.
 */
/* ARGSUSED */
void
str_mon_free(mon_t *mon)
{
#ifdef XXXrsNOT_YET
	int s;
	struct streams_mon_data *smd;

	LOCK_STRHEAD_MONP(s);
	smd = (struct streams_mon_data *)mon->mon_private;

	ASSERT(smd);

	if (!smd->smd_assoc_cnt && smd->smd_flags & SMD_PRIVATE) {
		if (smd->smd_joining != NULL) {
			smd->smd_flags |= SMD_DEAD;
		} else {
			if (mon->mon_prev) {
				mon->mon_prev->mon_next = mon->mon_next;
			} else {
				privmon_list = mon->mon_next;
			}
			if (mon->mon_next)
				mon->mon_next->mon_prev = mon->mon_prev;
			kmem_free(smd, sizeof(struct streams_mon_data));
			freemonitor(mon);
			privmon_cnt--;
		}
	}
	UNLOCK_STRHEAD_MONP(s);
#endif /* XXXrsNOT_YET */
	return;
}

/*
 * Caller holds global monitor pointer lock and both monitor locks.
 */
static void
str_swapmonitors(mon_t *srcmon, mon_t *dstmon)
{

	register void **p, **q;
	struct streams_mon_data *srcsmd, *dstsmd;
	register struct stdata *stp;

	ASSERT(spinlock_islocked(&strhead_monp_lock));
	ASSERT(spinlock_islocked(&srcmon->mon_lock));
	ASSERT(spinlock_islocked(&dstmon->mon_lock));

	srcsmd = (struct streams_mon_data *)srcmon->mon_private;
	dstsmd = (struct streams_mon_data *)dstmon->mon_private;

	/* srcmon must be private and have at least one associated stream */

	ASSERT(srcsmd->smd_flags & SMD_PRIVATE);
	ASSERT(srcsmd->smd_assoc_cnt);

	/* move all associated stream heads from source to destination */

	for (stp = srcsmd->smd_headp; stp; stp = stp->sd_assoc_next)
		stp->sd_monp = dstmon;
	dstsmd->smd_assoc_cnt += srcsmd->smd_assoc_cnt;

	if (dstsmd->smd_tailp)
		dstsmd->smd_tailp->sd_assoc_next = srcsmd->smd_headp;
	else
		dstsmd->smd_headp = srcsmd->smd_headp;
	srcsmd->smd_headp->sd_assoc_prev = dstsmd->smd_tailp;
	dstsmd->smd_tailp = srcsmd->smd_tailp;

	srcsmd->smd_headp = srcsmd->smd_tailp = 0;
	srcsmd->smd_assoc_cnt = 0;

	/* move all interrupt blocks from source to destination */

	/*
	 * With the global strhead_monp_lock held, interrupts can't queue
	 * for either monitor so shouldn't have to grab str_intr_lock
	 * or other locks.
	 */
	if ((p = dstmon->mon_queue) != NULL) {
		for ( ; p != NULL; p = (void **)*p)
			q = p;
		*q = srcmon->mon_queue;
	} else {
		dstmon->mon_queue = srcmon->mon_queue;
	}
	srcmon->mon_queue = NULL;

	srcsmd->smd_tailp = srcsmd->smd_headp = NULL;
	srcsmd->smd_assoc_cnt = 0;

	/*
	 * Unlock and wake everyone waiting for the source monitor.  Since
	 * we've moved the stream heads, all the waiters will requeue on the
	 * new monitor.
	 */
	srcmon->mon_id = 0;
	srcmon->mon_p_arg = NULL;
	srcmon->mon_monpp = NULL;
	srcmon->mon_monp_lock = NULL;
	srcmon->mon_lock_flags &= ~MON_LOCKED;
	if (srcmon->mon_lock_flags & MON_WAITING){
		srcmon->mon_lock_flags &= ~MON_WAITING;
		sv_broadcast(&srcmon->mon_wait);
	}

#ifdef XXXrsNOT_YET
	/* release and free the source monitor */

	if (srcmon->mon_prev) {
		srcmon->mon_prev->mon_next = srcmon->mon_next;
	} else {
		privmon_list = srcmon->mon_next;
	}
	if (srcmon->mon_next)
		srcmon->mon_next->mon_prev = srcmon->mon_prev;

	nested_spinunlock(&srcmon->mon_lock);

	kmem_free(srcsmd, sizeof(struct streams_mon_data));
	freemonitor(srcmon);
	privmon_cnt--;
#endif /* XXXrsNOT_YET */

}


/*
 * Force the streams associated with ourq and theirq to use the same
 * monitor.
 *
 * We must already hold the monitor associated with ourq.
 */
int
joinstreams(queue_t *ourq, queue_t **theirqp)
{
	int s;
	queue_t *theirq;
	mon_t *ourmon, *theirmon;
	struct streams_mon_data *oursmd, *theirsmd;

retry:

	theirq = *theirqp;
	if (theirq == NULL)
		return(-1);

	LOCK_STRHEAD_MONP(s);
	
	if ((ourmon = *ourq->q_monpp) == NULL ||
	    (oursmd = (struct streams_mon_data *)ourmon->mon_private)
	     == NULL) {
		UNLOCK_STRHEAD_MONP(s);
		return(-1);
	}
	if ((theirmon = *theirq->q_monpp) == NULL ||
	    (theirsmd = (struct streams_mon_data *)theirmon->mon_private)
	     == NULL) {
		UNLOCK_STRHEAD_MONP(s);
		return(-1);
	}

	/*
	 * If the two streams are already using the same monitor, just
	 * return.
	 */
	if (ourmon == theirmon) {
		UNLOCK_STRHEAD_MONP(s);
		return(0);
	}
	if (oursmd->smd_joining != NULL || theirsmd->smd_joining != NULL) {

		/* Other joins are in progress, wait until later... */
		sv_wait(&joinwait, PZERO|TIMER_SHIFT(AS_STRMON_WAIT),
			&strhead_monp_lock, s);
		goto retry;	/* XXXrs ugh!  I *hate* gotos */
	}

	/*
	 * Mark both monitors so that other joins will wait until this
	 * one is done.
	 */
	oursmd->smd_joining = theirmon;
	theirsmd->smd_joining = ourmon;

	/*
	 * Mark their monitor as locked.  Note that we automagically
	 * reacquire our monitor after any sleep.
	 */
	nested_spinlock(&theirmon->mon_lock);
	while (theirmon->mon_lock_flags & MON_LOCKED) {
		theirmon->mon_lock_flags |= MON_WAITING;
		nested_spinunlock(&strhead_monp_lock);
		sv_wait(&theirmon->mon_wait, PZERO|TIMER_SHIFT(AS_STRMON_WAIT) /* XXXnewlock */,
			&theirmon->mon_lock, s);
		LOCK_STRHEAD_MONP(s);
		nested_spinlock(&theirmon->mon_lock);
	}
	theirmon->mon_lock_flags |= MON_LOCKED;

	/* for locking, this is an inlined ASSERT(STREAM_LOCKED(ourq)) */
	ASSERT(curthreadp->k_activemonpp &&
	    *(ourq)->q_monpp == *curthreadp->k_activemonpp);

	/*
	 * We've got our monitor and their monitor locked.
	 * Since we could have slept locking thier monitor, make
	 * sure theirq is still valid.
	 */
	if ((theirq = *theirqp) == NULL) {
		/*
		 * Theirq is no longer valid.  The stream we are trying
		 * to join with seems to have gone away.  Clean up.
		 */
		ASSERT(theirsmd->smd_joining == ourmon);
		ASSERT(oursmd->smd_joining == theirmon);
		/* Unmark */
		theirsmd->smd_joining = NULL;
		oursmd->smd_joining = NULL;
		theirmon->mon_lock_flags &= ~MON_LOCKED;
		if (theirmon->mon_lock_flags & MON_WAITING) {
			theirmon->mon_lock_flags &= ~MON_WAITING;
			sv_broadcast(&theirmon->mon_wait);
		}
		nested_spinunlock(&strhead_monp_lock);
		UNLOCK_MONITOR(theirmon, s);
		/*
		 * Their mointor is still valid.  Since we had it marked
		 * for joining, other joins might be waiting.  Wake them.
		 */
		sv_broadcast(&joinwait);
		return(-1);
	}
	nested_spinlock(&ourmon->mon_lock);

	/*
	 * We've got our monitor, their monitor, and the global monitor
	 * pointer lock.  Call str_swapmonitors() to do the gory work.
	 */

	if (theirmon != streams_monp) {
		str_swapmonitors(theirmon, ourmon);
		oursmd->smd_joining = theirsmd->smd_joining = NULL;
		nested_spinunlock(&strhead_monp_lock);
		nested_spinunlock(&ourmon->mon_lock);
		UNLOCK_MONITOR(theirmon, s);

	} else { /* The global monitor always takes precedence. */

		/*
		 * We are just about to swap off our own monitor.  Need
		 * to adjust things so that we are officially holding
		 * the global monitor so that when we vmonitor we don't
		 * trip any ASSERTs or otherwise break stuff on the way out.
		 */
		ASSERT(ourmon->mon_trips == 0);		/* XXXrs ??? */
		ASSERT(theirmon->mon_trips == 0);
		ASSERT(theirmon->mon_id == 0);
		ASSERT(theirmon->mon_monpp == NULL);
		ASSERT(theirmon->mon_monp_lock == NULL);
		curthreadp->k_monitor = theirmon;
		theirmon->mon_id = ourmon->mon_id;
		theirmon->mon_p_arg = ourmon->mon_p_arg;
		theirmon->mon_monpp = ourmon->mon_monpp;
		theirmon->mon_monp_lock = ourmon->mon_monp_lock;

		str_swapmonitors(ourmon, theirmon);
		theirsmd->smd_joining = oursmd->smd_joining = NULL;
		nested_spinunlock(&strhead_monp_lock);
		nested_spinunlock(&ourmon->mon_lock);
		UNLOCK_MONITOR(theirmon, s);
	}


	sv_broadcast(&joinwait); /* Wake up any waiters. */

	return(0);
}

/*
 * We hold the monitor associated with ourq.
 */
int
useglobalmon(queue_t *ourq)
{
	return(joinstreams(ourq, &globmon_qp));
}

/*
 * STREAMS interrupt routine called to do timeouts and from interrupt routines
 * who want to do STREAMSy things
 *
 * Return:	0 = couldn't get semaphore, action was queued
 *		1 = did it
 *	       -1 = no interrupt blocks available or other error, action
 *		    was dropped
 */
int
streams_interrupt(
	strintrfunc_t func,
	void *arg1,
	void *arg2,
	void *arg3)
{
	/* just in case something bad happens from tpisocket */
	if (func == 0) {
		return -1;
	}

	return(mp_streams_interrupt(&streams_monp, 0,
				    func, arg1, arg2, arg3));
}

int
mp_streams_interrupt(
	mon_t **monpp,
	uint flags,
	strintrfunc_t func,
	void *arg1,
	void *arg2,
	void *arg3)
{
	register struct strintr *intr;
	register int s;

	/* Protect against early calls */
	if (!streams_intr_ready)
		return(-1);

	/* just in case something bad happens from tpisocket */
	if (func == 0) {
		return -1;
	}

	if (tpihold((void *)func, &arg2, arg3))
		return 0;

	STREAMS_BLOCK_INTR(s);

	/* Set up a STREAMS interrupt structure */

        if ((intr = strintrfree) == NULL) {
		/*
	 	 * All interrupt blocks are in use!  Record the incident
		 * and return indicating error.
	 	 */
		STREAMS_UNBLOCK_INTR(s);
		/*
		 * Printing to the console might need interrupt blocks so
		 * send warning to syslog only.
		 */
		cmn_err(CE_WARN, "!STREAMS interrupt block unavailable.");
		return(-1);
	}
	strintrfree = strintrfree->next;

#ifdef DEBUG
	streams_intr_inuse++;
	if (streams_intr_inuse > streams_intr_inuse_max)
		streams_intr_inuse_max = streams_intr_inuse;
#endif /* DEBUG */

	if (--streams_intr_avail < streams_intr_lowat)
		/*
		 * Printing to the console might need interrupt blocks so
		 * send warning to syslog only.  Avoids any possible
		 * deadlock on the interrupt block pool lock.
		 */
		cmn_err(CE_WARN,
#ifndef DEBUG
			"!STREAMS interrupt blocks scarce.  Only %d left.",
			streams_intr_avail);
#else
		        "!strintr low: %d total, %d inuse, %d rsrv, %d avail",
			nstrintr, streams_intr_inuse, streams_intr_rsrvd,
			streams_intr_avail);
#endif /* DEBUG */

	/* It's all ours now... */
	STREAMS_UNBLOCK_INTR(s);

	intr->flags = flags;
	intr->func = func;
	intr->arg1 = arg1;
	intr->arg2 = arg2;
	intr->arg3 = arg3;
	intr->monpp = monpp;

	LOCK_STRHEAD_MONP(s);
	ASSERT(monpp);
	if (monpp == NULL || *monpp == NULL) {
		UNLOCK_STRHEAD_MONP(s);

		/* Free the interrupt block */

		STREAMS_BLOCK_INTR(s);
		intr->next = strintrfree;
		strintrfree = intr;
#ifdef DEBUG
		streams_intr_inuse--;
		streams_intr_lost++;
#endif /* DEBUG */
		streams_intr_avail++;
		STREAMS_UNBLOCK_INTR(s);

		return(-1);
	}
	return(spinunlock_qmonitor(&strhead_monp_lock, s, monpp,
				   (void *)intr, (void **)monpp, 1));
}

/*
 * STREAMS interrupt routine called via streams_timeout to do interrupt after
 * timeout.  Identical to streams_interrupt except that it uses a previously
 * reserved interrupt block so that it can never fail (we hope!)
 *
 * Return:	0 = couldn't get semaphore, action was queued
 *		1 = did it
 */
void
streams_timeout_interrupt(struct strintr *intr, int gen)
{
	mon_t **monpp;
	register int s;

	ASSERT(intr != NULL);

	STREAMS_BLOCK_INTR(s);

	if (intr->gen != gen) {
		/*
		 * Somebody did an untimeout before we got here.
		 * The interrupt block isn't ours anymore.  Bail.
		 */
		STREAMS_UNBLOCK_INTR(s);
		return;
	}

	/* Remove the reserved strintr struct from the reserved list */
	if (intr->prev != NULL)
		intr->prev->next = intr->next;
	else
		strintrrsrv = intr->next;
	if (intr->next != NULL)
		intr->next->prev = intr->prev;

#ifdef DEBUG
	streams_intr_rsrvd--;
	streams_intr_inuse++;
	if (streams_intr_inuse > streams_intr_inuse_max)
		streams_intr_inuse_max = streams_intr_inuse;
#endif /* DEBUG */

	/* It's all ours now... */
	STREAMS_UNBLOCK_INTR(s);

	monpp = intr->monpp;

	LOCK_STRHEAD_MONP(s);
	if (*monpp == NULL) {
		UNLOCK_STRHEAD_MONP(s);

		/* Free the interrupt block */

		STREAMS_BLOCK_INTR(s);
		intr->next = strintrfree;
		strintrfree = intr;
#ifdef DEBUG
		streams_intr_inuse--;
		streams_timeout_lost++;
#endif /* DEBUG */
		streams_intr_avail++;
		STREAMS_UNBLOCK_INTR(s);

		return;
	}
	spinunlock_qmonitor(&strhead_monp_lock, s, monpp,
			    (void *)intr, (void **)monpp, 0);
	return;
}


/*
 * STREAMS service routine called to run queued interrupt requests.
 */
void
streams_service(void *a)
{
	register queue_t *q;	/* XXXrs - kludgy */
	register qband_t *qbp;	/* XXXrs - kludgy */

	register strintrfunc_t func;
	register void *arg1,*arg2,*arg3;
	register int s;
	register mon_t **monpp, *mon;
	uint flags;
	register struct strintr *intr = (struct strintr *)a;

	flags = intr->flags;
	func = (strintrfunc_t)intr->func;
	arg1 = intr->arg1;
	arg2 = intr->arg2;
	arg3 = intr->arg3;
	monpp = intr->monpp;

	/* Get spinlock for mucking with interrupt list */
	STREAMS_BLOCK_INTR(s);

	/* Free the interrupt block */
	intr->next = strintrfree;
	strintrfree = intr;

#ifdef DEBUG
	streams_intr_inuse--;
#endif /* DEBUG */
	streams_intr_avail++;

	STREAMS_UNBLOCK_INTR(s);

	/* XXXrs */

	LOCK_STRHEAD_MONP(s);
	mon = *monpp;
	UNLOCK_STRHEAD_MONP(s);

	/*
	 * XXXrs - This should be unnecessary.  Interrupt blocks should
	 *	   get moved when the monitor is unlinked.  Right?
	 *
	 * Check if entry has been removed or if there's no monitor
	 * associated.
	 */
	if (func == 0 || mon == NULL)
		return;

	ASSERT(monpp);
	curthreadp->k_activemonpp = monpp;

	/*
	 * Special case for _queuerun:  if we are the CPU that put queuerun on
	 * the list, then we must be at low enough spl for the soft clock to
	 * come in, then we can run _queuerun.  If we are not that CPU, then
	 * we could be at higher spl and do not want to run queues now, so
	 * just set a soft clock interrupt to run it when we are at lower spl.
	 */

	if (func == (strintrfunc_t)_queuerun) {
		if (cpuid() == (__psint_t) arg1)
			_queuerun();
		else {
			qrunflag = 0;
			qqrun = 0;
			setqsched();
		}

		return;
	}

	/*
	 * Special case.  If SI_QENAB flag is set in the interrupt block
	 * flags, the function we are about to call is a service procedure
	 * queued by qenable().  That means that arg1 is a queue pointer
	 * and we have to clear the QENAB flag in the queue.  This used to
	 * happen just prior to the call to the service procedure in
	 * _queuerun(), but now that _queuerun() is obsolete, we do it here.
	 */
	if (flags & SI_QENAB) {
		ASSERT(arg1);
		q = (queue_t *)arg1;
		LOCK_STR_RESOURCE(s);
		ASSERT(q->q_pflag & QENAB);
		q->q_pflag &= ~QENAB;
		UNLOCK_STR_RESOURCE(s);
	}

	/* Call the function */
	(void) (*func)(arg1,arg2,arg3);

	/*
	 * More special casing.  If SI_QENAB flag is set in the interrupt
	 * block flags, the function we just called was a service procedure
	 * scheduled via qenable and we have some post-service-procedure
	 * bookkeeping to do.  We have to reset the QBACK and QB_BACK flags.
	 * This used to be done just after the call to the service procedure
	 * in _queuerun().
	 */

	if (flags & SI_QENAB) {
		ASSERT(arg1);
		q = (queue_t *)arg1;
		q->q_flag &= ~QBACK;
		for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next)
			qbp->qb_flag &= ~QB_BACK;
	}
	return;
}


/*
 * STREAMS queuerun procedure for MP systems using STREAMS_interrupt
 */
void
queuerun(void)
{
	if (qqrun == 0) {
		int rv;
		qqrun = 1;
		rv = MP_STREAMS_INTERRUPT(&streams_monp, _queuerun,
					 (__psint_t)cpuid(), NULL, NULL);
		if (rv < 0)
			qqrun = 0;
	}
}


void
streams_p_mon(struct monitor *mon)
{
	/* The passed monitor has just been P-ed.  Do any private stuff. */

	curthreadp->k_activemonpp = (mon_t **)mon->mon_p_arg;
	return;
}


/* ARGSUSED */
void
streams_v_mon(struct monitor *mon)
{
	/* The passed monitor has just been V-ed.  Do any private stuff. */

	curthreadp->k_activemonpp = NULL;
	return;
}


void
streams_q_mon_sav(struct monitor *mon, void **privp)
{
	*privp = (void *)curthreadp->k_activemonpp;
	curthreadp->k_activemonpp = (mon_t **)mon->mon_p_arg;
}


/* ARGSUSED */
void
streams_q_mon_rst(struct monitor *mon, void *priv)
{
	curthreadp->k_activemonpp = (mon_t **)priv;
}


/*
 * STREAMS set a timeout to call a function which will run holding
 * the monitor.
 */
toid_t
streams_timeout(
	mon_t **monpp,
	strtimeoutfunc_t func,
	int tm,
	void *arg1,
	void *arg2,
	void *arg3)
{
	register int s;
	register toid_t id;
	register struct strintr *intr;

	STREAMS_BLOCK_INTR(s);

	/*
	 * Reserve an interrupt block by moving it from the free list to
	 * the reserved list.
	 */

	/* Pull it off the free list */
	if ((intr = strintrfree) == NULL) {
		STREAMS_UNBLOCK_INTR(s);
		return((toid_t)0);
	}
	strintrfree = strintrfree->next;

	/* Put it onto the front of the reserved list */
	intr->next = strintrrsrv;
	strintrrsrv = intr;
	if (intr->next != NULL)
		intr->next->prev = intr;
	intr->prev = NULL;

#ifdef DEBUG
	streams_intr_rsrvd++;
	if (streams_intr_rsrvd > streams_intr_rsrvd_max)
		streams_intr_rsrvd_max = streams_intr_rsrvd;
#endif /* DEBUG */


	if (--streams_intr_avail < streams_intr_lowat) {
		/*
		 * Printing to the console might need interrupt blocks so
		 * send warning to syslog only.  Avoids any possible
		 * deadlock on the interrupt block pool lock.
		 */
		cmn_err(CE_WARN,
#ifndef DEBUG
			"!STREAMS interrupt blocks scarce.  Only %d left.",
			streams_intr_avail);
#else
		        "!strintr low: %d total, %d inuse, %d rsrv, %d avail",
			nstrintr, streams_intr_inuse, streams_intr_rsrvd,
			streams_intr_avail);
#endif /* DEBUG */
	}

	/*
	 * Fill in the function pointer and the arguments.
	 */
	intr->flags = 0;
	intr->func = func;
	intr->arg1 = arg1;
	intr->arg2 = arg2;
	intr->arg3 = arg3;
	intr->monpp = monpp;

	/* Set the timeout */
	id = itimeout(streams_timeout_interrupt, intr, tm,
		      plbase, intr->gen);
	ASSERT(id);

	if (!id) {
		/*
		 * Ugh. The timeout failed.  Move the block back to the
		 * free list.
		 */
		if ((strintrrsrv = strintrrsrv->next) != NULL)
			strintrrsrv->prev = NULL;
		intr->next = strintrfree;
		strintrfree = intr;
		streams_intr_avail++;
#ifdef DEBUG
		streams_intr_rsrvd--;
#endif /* DEBUG */
		STREAMS_UNBLOCK_INTR(s);
		return(id);
	}

	/*
	 * Stash the timeout id into the interrupt block so we can
	 * unreserve this block on untimeouts.
	 */
	intr->id = id;

	STREAMS_UNBLOCK_INTR(s);
	return(id);
}

/*
 * MP STREAMS routine called to remove any interrupt block reserved
 * for the given timeout id from the list of reserved interrupt blocks.
 */
void
streams_untimeout(toid_t id)
{
	register int s;
	register struct strintr *intr;

	/* XXXrs - race is benign */
	if (strintrrsrv == NULL || !id)	/* Nothing to do */
		return;

	/*
	 * Find any block reserved for the given id.
	 */
	STREAMS_BLOCK_INTR(s);
	intr = strintrrsrv;
	while (intr != NULL && intr->id != id)
		intr = intr->next;
	if (intr == NULL) {
		/* No block reserved for this id */
		STREAMS_UNBLOCK_INTR(s);
		return;
	}

	/* Remove the struct from the reserved list */
	if (intr->prev != NULL)
		intr->prev->next = intr->next;
	else
		strintrrsrv = intr->next;
	if (intr->next != NULL)
		intr->next->prev = intr->prev;

	intr->gen++;	/* Leave some fingerprints */

	/* Put it onto the free list */
	intr->next = strintrfree;
	strintrfree = intr;

	streams_intr_avail++;
#ifdef DEBUG
	streams_intr_rsrvd--;
#endif /* DEBUG */

	STREAMS_UNBLOCK_INTR(s);
}

/*
 * MP STREAMS routine called from qdetach that removes any pending qenables
 * for the queues about to be freed.
 */
void
streams_mpdetach(queue_t *rq)
{
	register struct strintr *tmplst, *prev, *cur, *next;
	mon_t *monp;
	queue_t *wq = WR(rq);
	int i, s;

	LOCK_STRHEAD_MONP(s);
	if (!(monp = *rq->q_monpp)) {
		UNLOCK_STRHEAD_MONP(s);
		return;
	}
	/* Get spinlock and spl for mucking with monitor interrupt list */
	/* XXXrs - still holding strhead_monp_lock - too long??? */
	nested_spinlock(&monp->mon_lock);

	tmplst = prev = NULL;
	cur = (struct strintr *)monp->mon_queue;
	while (cur != NULL) {
		next = cur->next;
		if (cur->arg1 == (void *)rq || cur->arg1 == (void *)wq) {
			/* Unlink from monitor queue */
			if (prev != NULL)
				prev->next = next;
			else
				monp->mon_queue = (void **)next;

			/* Attach to temp. list */
			cur->next = tmplst;
			tmplst = cur;

		} else {
			prev = cur;
		}
		cur = next;
	}
	/* Release monitor spinlock */
	nested_spinunlock(&monp->mon_lock);

	/* XXXrs - held too long? */
	UNLOCK_STRHEAD_MONP(s);

	/*
	 * The tmplst now contains any obsolete qenable blocks.  These
	 * must be returned to the interrupt block pool.
	 */
	if ((cur = tmplst) == NULL)
		return;

	i = 1;
	while((next = cur->next) != NULL) {
		i++;
		cur = next;
	}

	/*
	 * tmplst points to the first block in the list, cur to the last.
	 * i holds the count of blocks in the list.
	 */

	/* Get interrupt block pool spinlock and spl */
	STREAMS_BLOCK_INTR(s);

	cur->next = strintrfree;
	strintrfree = tmplst;

	streams_intr_avail += i;
#ifdef DEBUG
	streams_intr_inuse -= i;
#endif /* DEBUG */

	/* Release spinlock and spl */
	STREAMS_UNBLOCK_INTR(s);
}

#define	PDELAY	(PZERO-1)

int
streams_delay(int ticks)
{
	return(mp_streams_delay(globmon_qp, ticks));
}

/*
 * MP-safe version of delay() in clock.c for STREAMS modules
 */
int
mp_streams_delay(queue_t *q, int ticks)
{
	register int s;

	if (ticks <= 0)
		return 0;

	STR_LOCK_SPL(s);
	MP_STREAMS_TIMEOUT(q->q_monpp, wakeup, (caddr_t)curthreadp+1, ticks);
	sleep((caddr_t)curthreadp+1, PDELAY);
	STR_UNLOCK_SPL(s);

	return 0;
}

/*
 * STREAMS copyin copyout and uiomove routines that release the monitor
 */
int
streams_copyin(void *s, void *d, unsigned int l, char *f, int c)
{
	register int rc;

	rc = strcopyin(s,d,l,f,c);
	return(rc);
}


int
streams_copyout(void *s, void *d, unsigned int l, char *f, int c)
{
	register int rc;

	rc = strcopyout(s,d,l,f,c);
	return(rc);
}

int
streams_uiomove(void *c, int s, enum uio_rw f, struct uio *p)
{

	register int rc;

	rc = uiomove(c,s,f,p);
	return(rc);
}

int
stream_locked(queue_t *q)
{
	int i, s;

	LOCK_STRHEAD_MONP(s);
	i = (curthreadp->k_activemonpp && *(q)->q_monpp == *curthreadp->k_activemonpp);
	UNLOCK_STRHEAD_MONP(s);
	return i;
}


/*
 * run-time versions of STREAMS_LOCK() and STREAMS_UNLOCK
 * for CPU independent code that needs access to STREAMS.
 */
/*
 * XXXrs - apparently only called from gfx/kern/gfx.c.  For now, the
 * entire gfx subsystem will use the global monitor so it can use these
 * calls.  Think about what to do in the future.
 */
void
streams_lock(void)
{
	STREAMS_LOCK();
}

void
streams_unlock(void)
{
	STREAMS_UNLOCK();
}
