/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#define _MUTEX_C

#include "sys/types.h"
#include "sys/atomic_ops.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/kmem.h"
#include "sys/ksignal.h"
#include "sys/param.h"
#include "sys/par.h"
#include "sys/pda.h"
#include "sys/proc.h"
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include "sys/runq.h"
#include "sys/systm.h"
#include "sys/prctl.h"
#include "sys/space.h"
#include "sys/splock.h"
#include "sys/timers.h"
#include "sys/clksupport.h"
#include "fs/procfs/prsystm.h"
#include "sys/callo.h"
#include "sys/rt.h"
#include "ksys/vproc.h"
#include "sys/ddi.h"
#include "ksys/behavior.h"
#include <sys/klstat.h>

/*
 * Forward procedure definitions.
 */
void mutex_wake(mutex_t *, int);
static void mutex_wait(mutex_t *, int);
static void mutex_queue(mutex_t *, kthread_t *);
static int indirect_wait(kthread_t *, kthread_t *, void *, char);

#ifdef SEMAMETER
static void mutex_wake_r(mutex_t *, int, inst_t *);
#endif

static zone_t	*mutex_zone;
static zone_t	*sv_zone;

extern int nproc;

#ifdef MP
#define MUTEX_SPINLOCKED (SPIN_INIT|SPIN_LOCK)
#else
#define MUTEX_SPINLOCKED (SPIN_INIT)
#endif

#define KT_LOCK_BITS(kt) (&kt->k_flags)

#define MUTEX_LOCK_BITLOCK -1

#define NOLOCK		0
#define SPINLOCK	1
#define BITLOCK		2
#define MUTEX		3
#define MRLOCK		4
#define VSEMA		5

#define SV_DQ_SETRUN	0
#define SV_DQ_SIGNAL	1
#define SV_DQ_TIMEOUT	2
#define SV_DQ_ABORT	3

#ifdef SEMAMETER
/*
 * [Potentially] inline versions of mutexmeter() and svmeter() for internal
 * use to keep code readable but fast under varying #ifdef's.
 */
static __inline mmeter_t *
_mutexmeter(mutex_t *mp)
{
#ifdef SEMAINFOP
	return (mmeter_t *)mp->m_info;
#else
	return mutexmeter(mp);
#endif
}

static __inline svmeter_t *
_svmeter(sv_t *svp)
{
#ifdef SEMAINFOP
	return (svmeter_t *)svp->sv_info;
#else
	return svmeter(svp);
#endif
}
#endif /* SEMAMETER */


/*
 * Lists of info structures for mutexes and synchronization variables.
 * These are always present so idbg can scan them.
 */
mpkqueuehead_t mutexinfolist[SEMABUCKETS];
mpkqueuehead_t svinfolist[SEMABUCKETS];

#ifdef SEMAINFO
static zone_t *mutexinfo_zone;
static zone_t *svinfo_zone;

#ifdef SEMAMETER
typedef mmeter_t	mutexinfo_t;
typedef svmeter_t	svinfo_t;
#else
typedef wchaninfo_t	mutexinfo_t;
typedef wchaninfo_t	svinfo_t;
#endif
#endif /* SEMAMETER */

void
thread_block(
	kthread_t	*kt,
	int		swtchflag,
	int		ktflag,
	int		trwake,
	int		s,
	long		timer)
{
	mon_t		*hadmonitor;
	mon_state_t	ms;
	int		s2;

	ASSERT(kt_islocked(kt));
	ASSERT(getsr() & SR_IE);
	ASSERT(!(kt->k_flags & KT_SYNCFLAGS));

	s2 = splprof();
	ktimer_switch(kt, timer ? timer : AS_SLEEP_WAIT);
	splx(s2);
	kt->k_flags |= ktflag;
	kt_nested_unlock(kt);

	/*
	 * If we own a monitor, then give it up now.
	 * MUST be after releasing all locks, since vmonitor can call
	 * routines which will vsema or wake us up.
	 */
	if (hadmonitor = kt->k_monitor)
		rmonitor(hadmonitor, &ms);

	/*
	 * If being traced, kick waiting parent.
	 */
	if (trwake)
		prasyncwake();
	
	swtch(swtchflag);

	if (kt->k_indirectwait) {
		kthread_t *nkt;

		kt_nested_lock(kt);
		nkt = kt->k_indirectwait;
		/* k_indirectwait could get zeroed if we are racing
		 * with a different processor. So, check for it 
		 * again before trying to grab lock.
		 */
		if (nkt) {
			kt_nested_lock(nkt);
			nkt->k_wchan = 0;
			kt->k_indirectwait = 0;
			kt_nested_unlock(kt);
			thread_unblock(nkt, KT_INDIRECTQ);
			kt_nested_unlock(nkt);
		} else {
			kt_nested_unlock(kt);
		}
	}

	/* FIX: why is this necessary? */
	ASSERT(swtchflag == MUTEXWAIT || swtchflag == MRWAIT ||
		kt->k_blink == kt && kt->k_flink == kt);

	kt_fclr(kt, KT_NWAKE);

	if (s)
		enableintr(s);

	/*
	 * If we released a monitor, reacquire it now.
	 * This of course can sleep - but CANNOT be breakable.
	 */
	if (hadmonitor)
		amonitor(&ms);
}

void
thread_unblock(
	kthread_t	*kt,
	int		flag)
{
	ASSERT(kt->k_flags & flag);
	kt->k_flags &= ~flag;
	putrunq(kt, CPU_NONE);
}

/*
 * System kernel mutex initialization routine.
 */
void
kmutei_init(void)
{
	/*
	 * Initialize zones...
	 */
	mutex_zone = kmem_zone_init(sizeof(mutex_t), "mutexes");
	sv_zone = kmem_zone_init(sizeof(sv_t), "sync-variables");

#ifndef SEMAINFO
	/*
	 * allocwchaninfo() will never be called so the 1 bucket hash lists
	 * will never get initialized.  Since idbg is the only entity outside
	 * the ksync code that accesses these lists, this simply keeps things
	 * prettier when various idbg commands are executed.
	 */
	mpkqueue_init(mutexinfolist);
	mpkqueue_init(svinfolist);
#endif
}

mutex_t *
mutex_alloc(int type, int flags, char *name, long sequence)
{
	register mutex_t *mp = (mutex_t *)kmem_zone_alloc(mutex_zone,
							flags & KM_NOSLEEP);
	if (mp)
		init_mutex(mp, type, name, sequence);

	return mp;
}

void
mutex_dealloc(mutex_t *mp)
{
	mutex_destroy(mp);
	kmem_zone_free(mutex_zone, mp);
}

/*ARGSUSED3*/
void
init_mutex(mutex_t *mp, int type, char *name, long sequence)
{
	ASSERT(type == MUTEX_DEFAULT);

	mp->m_bits = NULL;	/* skip metering for the mutex's spinlock */
	mp->m_queue = NULL;

#ifdef SEMAINFO
	_SEMAINFOP(mp->m_info = NULL;)
	allocwchaninfo(mp, name, sequence,
		       mutexinfolist, sizeof (mutexinfo_t),
		       &mutexinfo_zone, "mutexinfo");
#else
	_SEMAINFOP(mp->m_info = name;)
#endif

#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.mutex.current, 1);
	atomicAddInt(&syncinfo.mutex.initialized, 1);
#endif
}

/*ARGSUSED*/
void
mutex_destroy(mutex_t *mp)
{
	kthread_t *kt = private.p_curkthread;

	if (kt && kt->k_inherit == mp)
		panic("Freeing mutex while k_inherit still points to mutex");

	_SEMAINFO(freewchaninfo(mp, mutexinfolist, mutexinfo_zone);)

#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.mutex.current, -1);
#endif
}

/*
 * Really, only use this for printing debug stuff...
 */
void *
mutex_owner(register mutex_t *mp)
{
	return MUTEX_OWNER(mp->m_bits);
}

int
mutex_mine(register mutex_t *mp)
{
	return(MUTEX_OWNER(mp->m_bits) == private.p_curkthread);
}

int
mutex_owned(register mutex_t *mp)
{
	if (mutex_owner(mp))
		return 1;
	return 0;
}

/*
 * Called to conditionally acquire a mutex.
 * Returns:
 *	0 - couldn't acquire mutex
 *	1 - mutex acquired
 */
int
mutex_trylock(register mutex_t *mp)
{
	_SEMAMETER(mmeter_t *km = _mutexmeter(mp);)

	ASSERT(mp && (mp->m_bits != MUTEX_QUEUEBIT));
	ASSERT(private.p_kstackflag == PDA_CURKERSTK);

	if (compare_and_swap_ptr((void **)&mp->m_bits,
				(void *)0, (void *)private.p_curkthread)) {
		/*
		 * Got the mutex -- nothin' to do but update statistics.
		 */
#ifdef SEMAMETER
		if (km) {
			atomicAddUint(&km->mm_trylock, 1);
			km->mm_tryhit++;
			ASSERT(private.p_curkthread);
			km->mm_thread = private.p_curkthread;
			km->mm_id = private.p_curkthread->k_id;
			km->mm_lastpc = (inst_t *)__return_address;
			km->mm_ts = get_timestamp(); /* Start that ol' hold timer */
		}
#endif
		LSTAT_UPDATE(mp, LSTAT_ACT_NO_WAIT);
		return 1;
	}

#ifdef SEMAMETER
	if (km)
		atomicAddUint(&km->mm_trylock, 1);
#endif
	LSTAT_UPDATE(mp, LSTAT_ACT_REJECT);
	/*
	 * Another thread owns the mutex.
	 * Return failure.
	 */
	return 0;
}

/*
 * Called to release a mutex unconditionally
 * Returns nothing.
 */
void
mutex_unlock(register mutex_t *mp)
{
	ASSERT(private.p_curkthread == MUTEX_OWNER(mp->m_bits));

	/*
	 * If none of the mutex control bits (lock, queue) are set,
	 * atomically unlock this mutex.
	 * Otherwise leave the alone and let mutex_unlock handle
	 * unlock/wakeup/metering chores.
	 */
#ifdef SEMAMETER
	mutex_wake_r(mp, MUTEX_LOCK_BITLOCK, (inst_t *)__return_address);
#else
	if (curthreadp->k_inherit == mp ||
	    !compare_and_swap_ptr((void **)&mp->m_bits,
				  (void *)private.p_curkthread, (void *)0))
	{
		mutex_wake(mp, MUTEX_LOCK_BITLOCK);
	}
#endif
	/* The sync object has been unlocked at this point, making it possible
	 * for another thread to free the sync object's memory. Referencing
	 * the sync object after this point is not guaranteed safe.
	 */
}

/*
 * mutex_rmproc - remove a particular process from a mutex queue
 * Assumes mutex locked on entry, leaves it locked on exit.
 * Returns:
 *	1 - process not on queue
 *	0 - process on queue and removed
 */
int
mutex_rmproc(register mutex_t *mp, register kthread_t *kt)
{
	register kthread_t *tp, *fpp;

	if (fpp = mp->m_queue) {
		for (tp = fpp->k_flink ; ; tp = tp->k_flink) {
			ASSERT(tp);
			if (tp == kt) {
				tp = kt->k_flink;
				if (kt != tp) {
					tp->k_blink = kt->k_blink;
					kt->k_blink->k_flink = tp;
					mp->m_queue = tp;
				} else {
					mp->m_queue = NULL;
					mp->m_bits &= ~MUTEX_QUEUEBIT;
				}
				kt->k_flink = kt->k_blink = kt;
				return 0;
			}
			if (tp == fpp)
				break;
		}
	}

	return 1;
}

/*
 * Acquire mutual exclusion lock.
 * Returns nothing -- always succeeds (can never be broken).
 */
/*ARGSUSED*/
void
mutex_lock(register mutex_t *mp, int pri)
{
#ifdef SEMAMETER
	mmeter_t *km = _mutexmeter(mp);
	uint_t ts;
	int elapsed;
	ts = get_timestamp();
#endif


	ASSERT(mp && (mp->m_bits != MUTEX_QUEUEBIT));

	if (compare_and_swap_ptr((void **)&mp->m_bits,
				(void *)0, (void *)private.p_curkthread)) {
		/*
		 * Got the mutex now.
		 */
		ASSERT(MUTEX_OWNER(mp->m_bits) == private.p_curkthread);
#ifdef SEMAMETER
		if (km) {
			elapsed = get_timestamp() - ts;
			atomicAddUint(&km->mm_lock, 1);
			km->mm_lockhit++;
			km->mm_thread = private.p_curkthread;
			km->mm_id = private.p_curkthread->k_id;
			km->mm_lastpc = (inst_t *)__return_address;
			km->mm_ts = get_timestamp(); /* Start that ol' hold timer */
			km->mm_totalwaittime += elapsed;
			if (km->mm_waittime < (unsigned) elapsed)
			{
				km->mm_waittime = elapsed;
				km->mm_waiter = (inst_t *)__return_address;
			}

		}
#endif
		LSTAT_UPDATE(mp, LSTAT_ACT_NO_WAIT);
		return;
	}

	ASSERT(private.p_kstackflag == PDA_CURKERSTK ||
	       (private.p_kstackflag == PDA_CURIDLSTK &&
		curuthread && UT_TO_PROC(curuthread) &&
		UT_TO_PROC(curuthread)->p_pid == 0));

#ifdef SEMAMETER
	if (km)
		atomicAddUint(&km->mm_lock, 1);
	mutex_wait(mp, pri);
#elif SEMASTAT
	{
	uint64_t	start;
	start = get_timestamp();
	mutex_wait(mp, pri);
	LSTAT_UPDATE_TIME(mp, LSTAT_ACT_SLEPT, get_timestamp()-start);
	}
#else
	mutex_wait(mp, pri);
#endif
}

#define INHERIT_WFLAGS (KT_WMUTEX|KT_WMRLOCK)

static int
resource_raise_owner_pri(kthread_t *owner, void *res, char rtype)
{
	kthread_t *kt;

	if (owner->k_inherit && res != owner->k_inherit) {
		if (owner->k_flags & KT_MUTEX_INHERIT)
			kt = ((mutex_t *) owner->k_inherit)->m_queue;
		else 
			kt = mrlock_peek_hpri_waiter((mrlock_t *) owner->k_inherit);
		if (kt) {
			if (!kt_nested_trylock(kt))
				return 0;
			if (kt->k_flags & INHERIT_WFLAGS) {
				kt->k_wchan = 0;
				thread_unblock(kt,
					kt->k_flags & INHERIT_WFLAGS);
			}
			kt_nested_unlock(kt);
		}
	}
	kt = curthreadp;
	ASSERT(kt_islocked(owner));
	owner->k_inherit = res;
	if (rtype == RES_MUTEX)
		owner->k_flags |= KT_MUTEX_INHERIT;
	else 
		owner->k_flags &= (~KT_MUTEX_INHERIT);
	owner->k_pri = kt->k_pri;
	owner->k_flags |= kt->k_flags & KT_PS;
	owner->k_flags &= ~KT_NPRMPT;
	return 1;
}

void
drop_inheritance(void  *res, char rtype, kthread_t *kt)
{
	if (kt->k_inherit == res) {
		ASSERT(kt_islocked(kt));
		kt->k_flags &= ~KT_PS;
		kt->k_flags |= (kt->k_flags & KT_BASEPS ? KT_PS : 0);
		kt->k_flags |= (kt->k_flags & KT_NBASEPRMPT ? KT_NPRMPT : 0);
		kt->k_pri = kt->k_basepri;
		kt->k_inherit = 0;
		if (rtype == RES_MUTEX) {
			ASSERT(kt->k_flags & KT_MUTEX_INHERIT);
			kt->k_flags &= ~KT_MUTEX_INHERIT;
		} else
			ASSERT(!(kt->k_flags & KT_MUTEX_INHERIT));
		reset_pri(kt);
	}
}

static void
resource_resort_queue(kthread_t **qheadp)
{
	for (;;) {
		kthread_t *kt;
		kthread_t *ikt;
		for (kt = *qheadp;
		     kt->k_pri >= kt->k_flink->k_pri;
		     kt = kt->k_flink)
		{
			/* if the entire queue is sorted, return */
			if (kt->k_flink->k_flink == *qheadp)
				return;
		}

		kt = kt->k_flink;
		kt->k_flink->k_blink = kt->k_blink;
		kt->k_blink->k_flink = kt->k_flink;

		for (ikt = *qheadp;
		     ikt->k_pri >= kt->k_pri;
		     ikt = ikt->k_flink)
		{
			/*
			 * if we get to the end, the priorities on the
			 * queue must have changed. Break out, reinsert
			 * the thread, and start over.
			 */
			if (ikt->k_flink == *qheadp)
				break;
		}
		if (*qheadp == ikt)
			*qheadp = kt;
		kt->k_flink = ikt;
		kt->k_blink = ikt->k_blink;
		ikt->k_blink = kt;
		kt->k_blink->k_flink = kt;
	}
}

#define IS_TIMESHARE_PRI(pri)  \
	(((pri) == PTIME_SHARE) || ((pri) == PTIME_SHARE_OVER))

int
resource_test_owner_pri(void *res, char rtype)
{
	kthread_t *kt = curthreadp;
	kthread_t *owner;
	mutex_t *mp = NULL;
	mrlock_t *mrp = NULL;

	switch (rtype) {
	case RES_MUTEX:
		mp = (mutex_t *) res;
		if (kt != mp->m_queue)
			return BREAK;
		owner = MUTEX_OWNER(mp->m_bits);
		if (!kt_nested_trylock(owner))
			return SPIN;
		break;
	case RES_MRLOCK:
		mrp = (mrlock_t *) res;
		if (kt != mrlock_hpri_waiter(mrp))
			return BREAK;
		owner = mrlock_lpri_owner(mrp);
		if (!owner) /* FIX: owner may not have had to register yet */
			return BREAK;
		if (!kt_nested_trylock(owner))
			return SPIN;
		break;
	default:
		panic("Priority Inheritance on Unknown Resource");
	}
	/*
	 * Define STRICT_INHERITANCE for testing purposes to kick
	 * inheritance on broader conditions; otherwise TIMERSHARE_OVER
	 * aging will be sufficient to getout of "inversions".
	 */
#ifndef STRICT_INHERITANCE
	/*
	 * If both owner's basepri & waiter's effective (potentially inherited)
	 * priority are timeshare don't do anything
	 */
	if (IS_TIMESHARE_PRI(owner->k_basepri) && IS_TIMESHARE_PRI(kt->k_pri)) {
		kt_nested_unlock(owner);
		return BREAK;
	}
#endif
	switch (owner->k_flags & KT_SYNCFLAGS) {
	{
		cpuid_t cpu;
	case 0:
		cpu = CPU_NONE;
		/*
		 * If the owner has a higher base priority, there can be
		 * no inversion.
		 */
		if (owner->k_basepri >= kt->k_pri) {
			kt_nested_unlock(owner);
			return BREAK;
		}
		/*
		 * The owner has a higher pri, but it's inherited: keep
		 * an eye on it.
		 */
		if (owner->k_pri > kt->k_pri)
			break;
		/*
		 * The owner has the same priority.  Yield to guarantee
		 * forward progress.
		 */
		if (owner->k_pri == kt->k_pri) {
			if (kt->k_mustrun == CPU_NONE 
			    || owner->k_mustrun != CPU_NONE) 
			{
				kt_nested_unlock(owner);
				return YIELD;
			} else
				cpu = cpuid();
		}
		/*
		 * The owner has a lower priority than us and is
		 * running.  Raise it's priority so that it can do the
		 * same for any process it might be waiting for.
		 */
		if (owner->k_onrq == -1) {
			if (resource_raise_owner_pri(owner, res, rtype)) {
				kt_nested_unlock(owner);
				return BREAK;
			}
			break;
		}
		/*
		 * The owner has a lower priority than us and is on the
		 * runq somewhere.  Raise it's priority and put it back
		 * on the runq so that it can get it's job done.
		 */
		if (removerunq(owner)) {
			/*
			 * we have to remove the process from
			 * the run queue before raising its priority,
			 * because we may need to put another thread on
			 * the runq, and we don't want a deadlock.
			 */
			if (resource_raise_owner_pri(owner, res, rtype)) {
				putrunq(owner, cpu);
				kt_nested_unlock(owner);
				return BREAK;
			} else {
				putrunq(owner, CPU_NONE);
				kt_nested_unlock(owner);
				return SPIN;
			}
		}
		break;
		/* NOTREACHED */
	}
	case KT_WMUTEX:
	case KT_WMRLOCK:
		/*
		 * The owner has a higher priority.  Let it do the work.
		 */
		if (owner->k_pri >= kt->k_pri) {
			/*
			 * If the owner's priority can't change, just
			 * queue normally.
			 */
			if (KT_ISBASEPS(owner)
			    && owner->k_basepri >= kt->k_pri)
			{
				kt_nested_unlock(owner);
				return BREAK;
			} 
			/*
			 * His priority might change, but won't drop
			 * until he runs.  Wait for him to do so, and
			 * then execute the runnable owner code above.
			 */
			if (indirect_wait(owner, kt, res, rtype))
				return RETRY;
			break;
		}
		/*
		 * Raise the owner's priority and have it re-execute the
		 * mutex code.
		 */
		if (resource_raise_owner_pri(owner, res, rtype)) {

			mrlock_t *waitmr = NULL;
			mutex_t *waitmp = NULL;

			if ((owner->k_flags & KT_SYNCFLAGS) == KT_WMUTEX)
				waitmp = (mutex_t *)owner->k_wchan;
			else
				waitmr = (mrlock_t *)owner->k_wchan;

			if (waitmp) {
				nested_psbitlock(&waitmp->m_bits, 1);
				if (owner->k_wchan == (caddr_t)waitmp) {
					if (!waitmp->m_queue) /* owner has to be there! */
						panic("Owner on wchan, but not on queue!");
					owner->k_wchan = 0;
					resource_resort_queue(&waitmp->m_queue);
					nested_psbitunlock(&waitmp->m_bits, 1);
					thread_unblock(owner, KT_WMUTEX);
					kt_nested_unlock(owner);
					return BREAK;
				}
				ASSERT(!owner->k_wchan);
				nested_psbitunlock(&waitmp->m_bits, 1);
			} else if (waitmr) {
				if (waitmr == (mrlock_t *)res) {
					/* Has been granted ownership,
					 * but flags not cleared yet.
					 */
					kt_nested_unlock(owner);
					return RETRY;
				}
				if (!mr_nested_trylock(waitmr)) {
					kt_nested_unlock(owner);
					return SPIN;
				}
				if (owner->k_wchan == (caddr_t)waitmr) {
					owner->k_wchan = 0;
					mrlock_resort_queue(waitmr, owner);
					mr_nested_unlockq(waitmr);
					thread_unblock(owner, KT_WMRLOCK);
					kt_nested_unlock(owner);
					return BREAK;
				}
				ASSERT(!owner->k_wchan);
				mr_nested_unlockq(waitmr);
			}
		}
		break;
		/* NOTREACHED */
	case KT_WSV:
	case KT_WSEMA:
	case KT_INDIRECTQ:
		/*
		 * No need to do anything until the owner gets back.
		 * Wait for it.
		 */
		if (indirect_wait(owner, kt, res, rtype))
			return RETRY;
		break;
	default:
		ASSERT(0);
	}
	kt_nested_unlock(owner);
	return SPIN;
}

/*
 * Wait for mutex.  Not breakable.
 */
static void
mutex_wait(register mutex_t *mp, int flags)
{
	kthread_t *kt = private.p_curkthread;
	int trwake;
	int s;
	mon_t           *hadmonitor;
	mon_state_t     ms;


	s = mutex_psbitlock(&mp->m_bits, 1);
	ASSERT(issplhi(getsr()));
	ASSERT(getsr() & SR_IE);

	/*
	 * If mutex is no longer being held, claim the lock
	 * and get out.
	 */
	if (!MUTEX_OWNER(mp->m_bits)) {
		ASSERT(!(mp->m_bits & MUTEX_QUEUEBIT));
		ASSERT(!(mp->m_queue));
		mp->m_bits = (__psunsigned_t)kt;	/* unlocks mp */
		splx(s);
#ifdef SEMAMETER
		{
			mmeter_t *km = _mutexmeter(mp);
			if (km) {
				km->mm_lockhit++;
				km->mm_thread = kt;
				km->mm_id = kt->k_id;
				km->mm_lastpc = (inst_t *)__return_address;
			}
		}
#endif
		return;
	}

	ASSERT(MUTEX_OWNER(mp->m_bits) != kt);
	ASSERT(!(kt->k_flags & KT_SYNCFLAGS));

	/*
	 * We link pp onto mutex and release it before setting up
	 * kt's state -- race is avoided by mutex_wake spinning
	 * until kt->k_flags & KT_WMUTEX is set.
	 */
	mutex_queue(mp, kt);

	if (hadmonitor = kt->k_monitor)
		rmonitor(hadmonitor, &ms);

 retry:
	if (KT_ISPS(kt)) {
		int retval;
		while ((retval=resource_test_owner_pri(mp,RES_MUTEX)) != BREAK){
			if (retval == RETRY) {
				continue;
			}
			nested_psbitunlock(&mp->m_bits, 1);
			/*
			 * drop spl to 0 to allow interrupts/context
			 * switches during spins.  This shouldn't be
			 * necessary, but there are cases where we call
			 * mutex_lock with interrupts disabled.
			 */
			spl0();
			if (retval == SPIN) {
				/* this delay is arbitrary */
				DELAY(3);
			} else {
				ASSERT(retval == YIELD);
				qswtch(RESCHED_Y);
			}
			(void)mutex_psbitlock(&mp->m_bits, 1);
		}
	}
	if (MUTEX_OWNER(mp->m_bits) == kt) {
		mutex_psbitunlock(&mp->m_bits, 1, s);
		if (hadmonitor)
			amonitor(&ms);
		return;
	}
	kt_nested_lock(kt);
	kt->k_wchan = (caddr_t)mp;
	nested_psbitunlock(&mp->m_bits, MUTEX_LOCKBIT);

	trwake = thread_blocking(kt, flags & PLTWAIT ? KT_LTWAIT|KT_NWAKE : KT_NWAKE);
	thread_block(kt, MUTEXWAIT, KT_WMUTEX, trwake, s, 0);

	if ((mp->m_bits & ~MUTEX_QUEUEBIT) != (__psunsigned_t)kt) {
		s = mutex_psbitlock(&mp->m_bits, 1);
		goto retry;
	}
	if (hadmonitor)
		amonitor(&ms);
	ASSERT(kt->k_blink == kt && kt->k_flink == kt);
}

mutex_check_queue_order(mutex_t *mp)
{
	kthread_t *kt = mp->m_queue;

	if (!kt)
		return 1;

	while (kt->k_flink != mp->m_queue) {
		if (kt->k_pri < kt->k_flink->k_pri && KT_ISPS(kt->k_flink))
			return 0;
		kt = kt->k_flink;
	}
	return 1;
}
/*
 * mutex_queue - add a thread to a mutex's queue.
 * Mutex must be locked on entry, it is left locked on exit.
 * Threads are queued in priority order, but the queue can become
 * (slighlty?) unordered as a result of priority aging.
 * XXX	Would be good to have a way to specify "queue first" (high priority).
 */
static void
mutex_queue(
	register mutex_t *mp,
	register kthread_t *tp)
{
	register kthread_t *pkt = mp->m_queue;
	ASSERT(MUTEX_ISLOCKED(mp));

	ASSERT(tp->k_flink == tp);

	if (!pkt) {
		mp->m_queue = tp;
		mp->m_bits |= MUTEX_QUEUEBIT;
		return;
	}

	ASSERT(mp->m_bits & MUTEX_QUEUEBIT);

	if (KT_ISPS(tp)) {
		if (pkt->k_pri < tp->k_pri)
			mp->m_queue = tp;
		else {
			pkt = pkt->k_flink;
			while (pkt != mp->m_queue) {
				if (pkt->k_pri < tp->k_pri)
					break;
				pkt = pkt->k_flink;
			}
		}
	}
	tp->k_flink = pkt;
	tp->k_blink = pkt->k_blink;
	pkt->k_blink->k_flink = tp;
	pkt->k_blink = tp;

#ifdef SEMAMETER
	{
		mmeter_t *km = _mutexmeter(mp);
		if (km && ++km->mm_nwait > km->mm_maxnwait)
			km->mm_maxnwait = km->mm_nwait;
	}
#endif
}

/*
 * Called when a process needs to be awoken from sleeping on the mutex.
 * The thread that is awakened gets ownership of the mutex.
 *
 * Kernel mutexes are not actually spinlock-held on entry, but the only
 * caller is mutex_unlock -- there must be a thread sleeping on this lock,
 * since only mutex_unlock can get a thread off the wait-queue.
 */
void
mutex_wake(register mutex_t *mp, register int s)
{
#ifdef SEMAMETER
	mutex_wake_r(mp, s, (inst_t *)__return_address);
}

static void
mutex_wake_r(register mutex_t *mp, register int s, inst_t *raddr)
{
	int elapsed;
	mmeter_t *km = _mutexmeter(mp);
#endif
	register kthread_t *pkt, *xp;
	int onwchan = 0;

	if (s == MUTEX_LOCK_BITLOCK)
		s = mutex_psbitlock(&mp->m_bits, MUTEX_LOCKBIT);

	ASSERT_MP(MUTEX_ISLOCKED(mp));

	pkt = curthreadp;
	if (pkt->k_inherit == mp) {
		kt_nested_lock(pkt);
		drop_inheritance(mp, RES_MUTEX, pkt);
		kt_nested_unlock(pkt);
	}

	if (!(mp->m_bits & MUTEX_QUEUEBIT)) {
#ifdef SEMAMETER
		if (km) {
			atomicAddUint(&km->mm_unlock, 1);
			km->mm_unoslp++;
			km->mm_lastpc = raddr;
			elapsed = get_timestamp() - km->mm_ts;
			km->mm_totalholdtime += elapsed;
			if (km->mm_holdtime < (unsigned) elapsed)
			{
				km->mm_holdtime = elapsed;
				km->mm_holder = (inst_t *)__return_address;
			}

		}
#endif
		ASSERT(!mp->m_queue);
		mp->m_bits = 0;
		splx(s);
		goto resched_chk;
	}

	pkt = mp->m_queue;
	ASSERT(pkt);
	xp = pkt->k_flink;

#ifdef SEMAMETER
	if (km) {
		atomicAddUint(&km->mm_unlock, 1);
		km->mm_thread = xp;
		km->mm_id = xp ? xp->k_id : -1; 
		km->mm_nwait--;
	}
#endif
	if (pkt->k_wchan == (caddr_t)mp) {
		pkt->k_wchan = 0;
		onwchan++;
	}
	if (pkt != xp) {
		xp->k_blink = pkt->k_blink;
		pkt->k_blink->k_flink = xp;
		pkt->k_flink = pkt->k_blink = pkt;
		mp->m_queue = xp;
		/* unlock mp */
		SYNCHRONIZE();
		mp->m_bits = (__psunsigned_t)pkt | MUTEX_QUEUEBIT;
	} else {
		mp->m_queue = NULL;
		SYNCHRONIZE();
		mp->m_bits = (__psunsigned_t)pkt;	/* unlocks mp */
	}
	kt_nested_lock(pkt);

	/*
	 * If the thread owns the mutex, we need to make it runnable.
	 */
	if (onwchan && (pkt->k_flags & KT_WMUTEX)) {
		if (pkt->k_lastrun != CPU_NONE
		    && !(pkt->k_flags & KT_PSPIN) && !KT_ISPS(pkt))
		{
			pkt->k_inherit = mp;
			pkt->k_flags |= KT_MUTEX_INHERIT;
			pkt->k_flags |= KT_PS;
			pkt->k_flags &= ~KT_NPRMPT;	
			pkt->k_pri = 0;
		}
		ASSERT(pkt != curthreadp);
		thread_unblock(pkt, KT_WMUTEX);
	}
	kt_unlock(pkt, s);

	/* If drop_inheritance() has decreased our priority, we need to
	 * perform a preemption check.
	 * NOTE: If process also has a spinlock, we won't be at spl0()
	 * until we unlock the spinlock - we don't handle that case yet.
	 */
resched_chk:
	if (private.p_runrun && isspl0(getsr())) {
		int s;
		
		s = disableintr();
		kpswtch();
		enableintr(s);
	}
}

/*
 * Forward procedure declarations.
 */
typedef	int unlock_func_t;
static void sv_queue(sv_t *, int flags, unlock_func_t,  void *, uint, int);

/*
 * Private lock definitions.
 */
#define	sv_lock(svp)		mutex_psbitlock(&(svp)->sv_queue, SV_LOCK)
#define	sv_unlock(svp, s)	mutex_psbitunlock(&(svp)->sv_queue, SV_LOCK, s)
#define _sv_lock(svp)		nested_psbitlock(&(svp)->sv_queue, SV_LOCK)
#define _sv_unlock(svp)		((svp)->sv_queue &= ~SV_LOCK)

/*ARGSUSED*/
sv_t *
sv_alloc(int type, int flags, char *name)
{
	register sv_t *svp = (sv_t *)kmem_zone_alloc(sv_zone,
							flags & KM_NOSLEEP);
	if (svp)
		sv_init(svp, type, name);

	return svp;
}

void
sv_dealloc(sv_t *svp)
{
	sv_destroy(svp);
	kmem_zone_free(sv_zone, svp);
}

/*
 * Initialize the given sync variable.
 */
/*ARGSUSED3*/
void
init_sv(register sv_t *svp, int type, char *name, long sequence)
{
	/*
	 * The sync variable's bitlock doesn't get metered.
	 */
	svp->sv_queue = type & SV_TYPE;

#ifdef SEMAINFO
	_SEMAINFOP(svp->sv_info = NULL;)
	allocwchaninfo(svp, name, sequence,
		       svinfolist, sizeof (svinfo_t),
		       &svinfo_zone, "svinfo");
#else
	_SEMAINFOP(svp->sv_info = name;)
#endif

#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.sv.current, 1);
	atomicAddInt(&syncinfo.sv.initialized, 1);
#endif
}

/*ARGSUSED*/
void
sv_destroy(register sv_t *svp)
{
	ASSERT(!SV_WAITER(svp));
	_SEMAINFO(freewchaninfo(svp, svinfolist, svinfo_zone);)

#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.sv.current, -1);
#endif
}

int
sv_waitq(sv_t *svp)
{
	int s;
	register kthread_t *tp, *pp;
	register int rv;

	if (!SV_WAITER(svp))
		return 0;

	rv = 0;
	s = sv_lock(svp);
	if (pp = SV_WAITER(svp)) {
		for (tp = pp->k_flink ; ; tp = tp->k_flink) {
			rv++;
			if (tp == pp)
				break;
		}
	}

	sv_unlock(svp, s);
	return rv;
}

#define SV_PRIMASK	0x00ffffff
#define	SV_BREAKABLE	0x01000000

#define SV_LOCKMASK	0xf0000000
#define	SV_MUTEXLOCK	0x10000000
#define	SV_SPINLOCK	0x20000000
#define	SV_BITLOCK	0x30000000
#define	SV_MRLOCK	0x40000000
#define	SV_SEMALOCK	0x50000000

void
sv_bitlock_wait(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	uint		*lock,		/* lock to be released */
	register uint	lockbit,	/* lock bit */
	register int	ospl)		/* bitlock return value */
{
	sv_bitlock_timedwait(svp, flags, lock, lockbit, ospl, 0, 0, 0);
}

#ifdef SEMAMETER
static void
svmeter_sigbrk(sv_t *svp)
{
	svmeter_t *km = _svmeter(svp);
	if (km) {
		atomicAddInt((int *)&km->sm_waitsig, 1);
		atomicAddInt((int *)&km->sm_sigbrk, 1);
	}
}
#else
#define svmeter_sigbrk(svp)
#endif

int
sv_bitlock_wait_sig(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	uint		*lock,		/* lock to be released */
	register uint	lockbit,		/* lock bit */
	register int	s)		/* bitlock return value */
{
	return sv_bitlock_timedwait_sig(svp, flags, lock, lockbit, s, 0, 0, 0);
}

void
sv_mrlock_wait(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register mrlock_t *mrp)		/* lock to be released */
{
	/*REFERENCED(!MP)*/
	kthread_t	*kt = private.p_curkthread;
	int s;

	ASSERT(mrp);

	s = kt_lock(kt);
	sv_queue(svp, flags, MRLOCK, mrp, 0, s);
}

int
sv_mrlock_wait_sig(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register mrlock_t *mrp)		/* lock to be released */
{
	kthread_t	*kt = private.p_curkthread;
	int s;

	ASSERT(mrp);


	s = kt_lock(kt);

	bla_isleep(&s);

	if (thread_interrupted(kt)) {
		mrunlock(mrp);
		kt_unlock(kt, s);
		bla_iunsleep();
		svmeter_sigbrk(svp);
		return -1;
	}
	sv_queue(svp, flags|SV_BREAKABLE, MRLOCK, mrp, 0, s);

	bla_iunsleep();

	if (!kt->k_prtn && thread_interrupted(kt))
		return -1;
	return 0;
}

void
sv_sema_wait(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register sema_t *sp)		/* lock to be released */
{
	/*REFERENCED(!MP)*/
	kthread_t	*kt = private.p_curkthread;
	int s;

	ASSERT(sp);
	ASSERT((flags & PMASK) <= PZERO);

	s = kt_lock(kt);
	sv_queue(svp, flags, VSEMA, sp, 0, s);
}

int
sv_sema_wait_sig(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register sema_t *sp)		/* lock to be released */
{
	kthread_t	*kt = private.p_curkthread;
	int s;

	ASSERT(sp);
	ASSERT((flags & PMASK) > PZERO);

	s = kt_lock(kt);

	bla_isleep(&s);

	if (thread_interrupted(kt)) {
		kt_unlock(kt, s);
		bla_iunsleep();
		vsema((sema_t *)sp);
		svmeter_sigbrk(svp);
		return -1;
	}
	sv_queue(svp, flags | SV_BREAKABLE, VSEMA, sp, 0, s);

	bla_iunsleep();

	if (!kt->k_prtn && thread_interrupted(kt))
		return -1;
	return 0;
}
	
/*
 * Put thread to sleep on sync variable queue.
 */
void
sv_wait(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register void	*mp,		/* mutex to be released */
	register int	ospl)		/* return value from mutex_spinlock */
{
	sv_timedwait(svp, flags, mp, ospl, 0, 0, 0);
}

/*
 * Put thread to sleep on sync variable queue.
 * Breakable by a signal.
 */
int
sv_wait_sig(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register void	*mp,		/* mutex to be released */
	register int	ospl)		/* return value from mutex_spinlock */
{
	return sv_timedwait_sig(svp, flags, mp, ospl, 0, 0, 0);
}

static int
indirect_wait(kthread_t *owner, kthread_t *kt, void *res, char rtype)
{
	int trwake;

	/* FIX: if owner & kt are -2/-3 then don't raising */
	if (owner->k_pri < kt->k_pri && !resource_raise_owner_pri(owner, res, rtype))
		return 0;

	while (owner->k_pri > kt->k_pri && owner->k_indirectwait) {
		kthread_t *next = owner->k_indirectwait;
		kt_nested_lock(next);
		kt_nested_unlock(owner);
		owner = next;
	}

	kt->k_indirectwait = owner->k_indirectwait;
	owner->k_indirectwait = kt;
	kt_nested_lock(kt);
	kt_nested_unlock(owner);

	kt->k_wchan = (caddr_t)owner;

	if (rtype == RES_MUTEX)
		((mutex_t *)res)->m_bits &= ~MUTEX_LOCKBIT;
	else
		mr_nested_unlockq((mrlock_t *)res);

	trwake = thread_blocking(kt, KT_NWAKE|KT_LTWAIT);
	thread_block(kt, MUTEXWAIT, KT_INDIRECTQ, trwake, 0, 0);

	if (rtype == RES_MUTEX)
		nested_psbitlock(&((mutex_t *)res)->m_bits, MUTEX_LOCKBIT);
	else
		mr_nested_lockq((mrlock_t *)res);
	return 1;
}

/*
 * sv_queue - add a thread to a sync variable's queue.
 * Sv must be locked on entry, it is left locked on exit.
 * If the sv is of type SV_PRIO, the threads are queued
 * in priority order, but the queue can become (slighlty?)
 * unordered as a result of priority aging.
 */
/*ARGSUSED4 (!MP)*/
static void
sv_queue(
	register sv_t *svp,
	int flags,
	unlock_func_t unlock_func,
	 void *lock,
	uint lockbit,
	int  ospl)
{
	_SEMAMETER(svmeter_t *km;)
	kthread_t *tp = private.p_curkthread;
	kthread_t *kt;
	kthread_t *qhead;
	int	trwake;
	int	ktflags;

	ASSERT(kt_islocked(tp));
	ASSERT(tp->k_flink == tp && tp->k_blink == tp);
	_sv_lock(svp);	/* already at splhi */

	kt = SV_WAITER(svp);

#ifdef SEMAMETER
	km = _svmeter(svp);
	if (km) {
		if (flags & SV_BREAKABLE)
			atomicAddInt((int *)&km->sm_waitsig, 1);
		else
			km->sm_wait++;
		km->sm_nwait++;
		if (km->sm_nwait > km->sm_maxnwait)
			km->sm_maxnwait = km->sm_nwait;
	}
#endif

	tp->k_wchan = (caddr_t)svp;

	if (!kt) {
		/* unlocks svp */
		SYNCHRONIZE();
		svp->sv_queue = (__psunsigned_t)tp | (svp->sv_queue & SV_TYPE);
	} else {
		switch (svp->sv_queue & SV_TYPE) {
		case SV_FIFO :
			break;
		case SV_LIFO :
			SYNCHRONIZE();
			svp->sv_queue = (__psunsigned_t)tp | SV_LOCK | SV_LIFO;
			break;
		case SV_PRIO :
			if (KT_ISPS(kt)) {
				if (kt->k_pri < tp->k_pri) {
					SYNCHRONIZE();
					svp->sv_queue = ((__psunsigned_t)tp
							 | SV_LOCK | SV_PRIO);
				} else {
					qhead = kt;
					kt = kt->k_flink;
					while (kt != qhead) {
						if (kt->k_pri < tp->k_pri)
							break;
						kt = kt->k_flink;
					}
				}
			}
			break;
		case SV_KEYED :
			if (kt->k_qkey < tp->k_qkey) {
				SYNCHRONIZE();
				svp->sv_queue = ((__psunsigned_t)tp
						 | SV_LOCK | SV_KEYED);
			} else {
				qhead = kt;
				kt = kt->k_flink;
				while (kt != qhead) {
					if (kt->k_qkey < tp->k_qkey)
						break;
					kt = kt->k_flink;
				}
			}
			break;		
		}

		tp->k_flink = kt;
		tp->k_blink = kt->k_blink;
		tp->k_blink->k_flink = tp;
		kt->k_blink = tp;

		SYNCHRONIZE();
		svp->sv_queue &= ~SV_LOCK; /* unlocks svp */
	}

	switch (unlock_func) {
	case SPINLOCK:
		nested_spinunlock((lock_t *)lock);
		break;
	case BITLOCK:
		if (KT_LOCK_BITS(tp) != (uint *)lock)
			nested_bitunlock((uint *)lock, lockbit);
		break;
	case MUTEX:
		drop_inheritance((mutex_t *)lock, RES_MUTEX, tp);
		mutex_unlock((mutex_t *)lock);
		break;
	case MRLOCK:
		drop_inheritance((mrlock_t *)lock, RES_MRLOCK, tp);
		mrunlock((mrlock_t *)lock);
		break;
	case VSEMA:
		vsema((sema_t *)lock);
		break;
	case NULL:
		break;
	}

	if (flags & SV_BREAKABLE)
		ktflags = 0;
	else
		ktflags = KT_NWAKE;
	if (flags & PLTWAIT)
		ktflags |= KT_LTWAIT;
	trwake = thread_blocking(tp, ktflags);
	thread_block(tp, SVWAIT, KT_WSV, trwake, ospl, TIMER_SHIFTBACK(flags));
}

#ifndef SEMAMETER
#define sv_rmproc(sv,t,v,r) sv_rmproc(sv,t,v)
#endif

/*
 * sv_rmproc - delete a thread from the queue on a particular sync variable.
 *		called with the kt_lock set.
 * Note that this requires a lock ordering:
 *	kt_lock
 *	sv_lock
 * Returns:
 *	0 - thread not on queue
 *	1 - thread on queue and removed
 */
static int
sv_rmproc(
	register sv_t *svp,
	register kthread_t *kt,
	int	 prtn,
	inst_t		*callpc)
{
	register kthread_t	*fkt;
	_SEMAMETER(svmeter_t *km = _svmeter(svp);)

	/*
	 * The sync-variable lock protects the kthread's k_wchan.
	 * By the time we've gotten here, kt might have gotten an
	 * sv_broadcast and have been taken off sv's queue, but the
	 * broadcaster will spin waiting for kt's lock -- k_wchan
	 * will be off, though.
	 */
	_sv_lock(svp);

	if ((sv_t *)kt->k_wchan == svp) {

		kt->k_wchan = 0;
		fkt = kt->k_flink;

#ifdef SEMAMETER
		if (km) {
			km->sm_wsyncv++;
			km->sm_wsyncvd++;
			km->sm_nwait--;
			km->sm_thread = private.p_curkthread;
			km->sm_id = private.p_curkthread ?
				private.p_curkthread->k_id : -1;
			km->sm_lastpc = callpc;
		}
#endif
		/*
		 * fkt == kt means there are no more threads on the list
		 */
		if (fkt != kt) {
			ASSERT(fkt && (sv_t *)fkt->k_wchan == svp);
			fkt->k_blink = kt->k_blink;
			fkt->k_blink->k_flink = fkt;

			/* unlock svp */
			SYNCHRONIZE();
			if (SV_WAITER(svp) == kt)
				svp->sv_queue = ((__psunsigned_t)fkt
						 | (svp->sv_queue & SV_TYPE));
			else
				svp->sv_queue &= ~SV_LOCK;
		} else {
			SYNCHRONIZE();
			/* unlocks svp */
			svp->sv_queue &= SV_TYPE;
		}

		kt->k_flink = kt->k_blink = kt;
		kt->k_prtn = prtn;

		thread_unblock(kt, KT_WSV);

		return(1);
	}

#ifdef SEMAMETER
	if (km)
		km->sm_wsyncv++;
#endif
	_sv_unlock(svp);
	return(0);
}

/*
 * Called to wakeup a particular process/thread sleeping on a sync variable.
 * Returns:
 *	0 - pp not queued on mutex
 *	1 - pp de-queued
 *
 * This is an inherently racy routine - it is assumed that the caller
 * is synchronizing this and the target. The safest thing is to hold the
 * mutex that the target held when it called sv_wait, before calling wsyncv
 */
int
wsyncv_proc(register sv_t *svp, register proc_t *pp)
{
	return wsyncv(svp, UT_TO_KT(prxy_to_thread(&pp->p_proxy)));
}

int
wsyncv_uthread(register sv_t *svp, register uthread_t *ut)
{
	return wsyncv(svp, UT_TO_KT(ut));
}

int
wsyncv(register sv_t *svp, register kthread_t *kt)
{
	int s;
	int rc;

	s = kt_lock(kt);
	/*
	 * kt_lock is acquired in sv_timewait before setting up the
	 * wsyncv timeout, and is held until the thread is safely
	 * queued on the sync variable.
	 */
	if (kt->k_flags & KT_WSV)
		rc = sv_rmproc(svp, kt, SV_DQ_ABORT,
			       (inst_t *) __return_address);
	else
		rc = 0;
	kt_unlock(kt, s);
	return(rc);
}

/*
 * Wake up thread if it is sleeping on a wait-object.
 * 
 * This is very racy - another thread could be waking up the target thread
 * via more conventional means (i.e. via the object).
 * It is up to the callers to synchronize the higher level process/sthread
 * Its important that the state of the encompassing process/sthread cannot
 * change since otherwise we could easily unwchan() on the wrong object.
 *
 * Another problem is object reuse - since we are getting a handle to the
 * sync object via the wchan, it's possible that object has been free'd.
 * This is resolved since we hold kt_lock when looking at the wchan.
 * a sv_wake() or semawake() cannot complete w/o  acquiring kt_lock()
 * to NULL out wchan. As soon as we release kt_lock, the underlying
 * object can go away.
 */
void
setrun(kthread_t *kt)
{
	caddr_t chan;

	ASSERT((kt->k_flags & KT_WMUTEX) == 0);
	ASSERT(issplhi(getsr()));
	ASSERT(kt_islocked(kt));

	if ((chan = kt->k_wchan) != NULL) {
		switch (kt->k_flags & KT_SYNCFLAGS) {
		case KT_WSEMA:
			unsema((sema_t *)chan, kt, (inst_t *)__return_address);
			break;
		case KT_WSV:
			sv_rmproc((sv_t *)chan, kt, SV_DQ_SETRUN,
				  (inst_t *)__return_address);
			break;
		default:
			ASSERT(0);
		}
	}
}

static __inline kthread_t *
sv_get(sv_t *svp)
{
	register kthread_t *kt, *tp;

	kt = SV_WAITER(svp);
	ASSERT(kt);

	tp = kt->k_flink;
	kt->k_wchan = 0;

	if (tp != kt) {
		tp->k_blink = kt->k_blink;
		kt->k_blink->k_flink = tp;
		SYNCHRONIZE();
		svp->sv_queue = (__psunsigned_t)tp | (svp->sv_queue & SV_TYPE);
		kt->k_flink = kt->k_blink = kt;
	} else {
		SYNCHRONIZE();
		svp->sv_queue &= SV_TYPE;
	}

	kt_nested_lock(kt);
	return kt;
}

/*
 * Set runnable, at most, one thread waiting on sync variable.
 * Returns # of threads set runnable (0 or 1).
 */
int
sv_signal(register sv_t *svp)
{
	kthread_t *kt;
	int s;
#ifdef SEMAMETER
	svmeter_t *km = _svmeter(svp);

	if (km)
		atomicAddInt((int *)&km->sm_signal, 1);
#endif
	if (!SV_WAITER(svp))
		return 0;

	s = sv_lock(svp);
	if (SV_WAITER(svp)) {
#ifdef SEMAMETER
		if (km) {
			km->sm_signalled++;
			km->sm_nwait--;
			km->sm_thread = private.p_curkthread;
			km->sm_id = private.p_curkthread ?
				private.p_curkthread->k_id : -1;
			km->sm_lastpc = (inst_t *)__return_address;
		}
#endif
		kt = sv_get(svp);
		thread_unblock(kt, KT_WSV);
		kt_unlock(kt, s);
		return 1;
	}

	sv_unlock(svp, s);
	return 0;
}

/*
 * Set runnable any threads waiting on sync variable.
 * Returns # of threads set runnable.
 */
int
sv_broadcast(register sv_t *svp)
{
	kthread_t *kt;
	int s;
	int rv = 0;
#ifdef SEMAMETER
	svmeter_t *km = _svmeter(svp);

	if (km)
		atomicAddInt((int *)&km->sm_broadcast, 1);
#endif
	while (SV_WAITER(svp))
	{
		s = sv_lock(svp);
		if (SV_WAITER(svp)) {
#ifdef SEMAMETER
			if (km) {
				km->sm_broadcasted++;
				km->sm_nwait--;
				km->sm_thread = private.p_curkthread;
				km->sm_id = private.p_curkthread ?
					private.p_curkthread->k_id : -1;
				km->sm_lastpc = (inst_t *)__return_address;
			}
#endif
			kt = sv_get(svp);
			thread_unblock(kt, KT_WSV);
			kt_unlock(kt, s);
			rv++;
		} else {
			sv_unlock(svp, s);
			break;
		}
	}

	return rv;
}

/*
 * Set runnable any threads waiting on sync variable.
 *
 * Number of threads started is limited to the number waiting on initial 
 * entry.  This ensures that this routine will complete in situations 
 * in which it is not certain that some of the threads started will make
 * progress, rather than requeueing for the wait.
 *
 * This routine is not useful for non-FIFO sv's, since the set of threads
 * started may be different than the set waiting at entry, in unpredictable
 * ways.  Therefore, we assert that the sv passed is FIFO.
 *
 * Returns # of threads set runnable.
 */
int
sv_broadcast_bounded(register sv_t *svp)
{
	kthread_t *kt;
	int s;
	int rv = 0;
        int limit = 0;
#ifdef SEMAMETER
	svmeter_t *km = _svmeter(svp);

	if (km)
		atomicAddInt((int *)&km->sm_broadcast, 1);
#endif

	ASSERT((svp->sv_queue & SV_TYPE) == SV_FIFO);
	if (SV_WAITER(svp)) {
                register kthread_t *khead, *kcur;
		s = sv_lock(svp);

		khead = SV_WAITER(svp);
		if (khead) {
		        kcur = khead;
			do {
				limit++;
				kcur = kcur->k_flink;
			} while (kcur != khead);
		}

		sv_unlock(svp, s);
	}

	while (SV_WAITER(svp) && rv < limit)
	{
		s = sv_lock(svp);
		if (SV_WAITER(svp)) {
#ifdef SEMAMETER
			if (km) {
				km->sm_broadcasted++;
				km->sm_nwait--;
				km->sm_thread = private.p_curkthread;
				km->sm_id = private.p_curkthread ?
					private.p_curkthread->k_id : -1;
				km->sm_lastpc = (inst_t *)__return_address;
			}
#endif
			kt = sv_get(svp);
			thread_unblock(kt, KT_WSV);
			kt_unlock(kt, s);
			rv++;
		} else {
			sv_unlock(svp, s);
			break;
		}
	}

	return rv;
}

int
sv_foreach_wake(sv_t *svp, void (*func)(kthread_t *, void *), void *arg)
{
	int s;
	int rv = 0;
#ifdef SEMAMETER
	svmeter_t *km = _svmeter(svp);

	if (km)
		atomicAddInt((int *)&km->sm_broadcast, 1);
#endif
	while (SV_WAITER(svp))
	{
		s = sv_lock(svp);
		if (SV_WAITER(svp)) {
			kthread_t *kt;
#ifdef SEMAMETER
			if (km) {
				km->sm_broadcasted++;
				km->sm_nwait--;
				km->sm_thread = private.p_curkthread;
				km->sm_id = private.p_curkthread ?
					private.p_curkthread->k_id : -1;
				km->sm_lastpc = (inst_t *)__return_address;
			}
#endif
			kt = sv_get(svp);
			func(kt, arg);
			thread_unblock(kt, KT_WSV);
			kt_unlock(kt, s);
			rv++;
		} else {
			sv_unlock(svp, s);
			break;
		}
	}

	return rv;
}

int
sv_threshold(register sv_t *svp, int threshold)
{
	if (sv_waitq(svp) > threshold)
		return sv_signal(svp);
	return -1;
}

static void
sv_timedout(register sv_t *svp, register kthread_t *kt)
{
	int s;

	s = kt_lock(kt);
	if (kt->k_flags & KT_WSV)
		(void) sv_rmproc(svp, kt, SV_DQ_TIMEOUT,
				 (inst_t *) __return_address);
	kt_unlock(kt, s);
}

/*ARGSUSED*/
static toid_t
sv_set_timeout(int svtimer_flags, sv_t *svp, timespec_t *ts)
{
	kthread_t *kt = private.p_curkthread;
	extern int fasthz;
	int dofast = 0;
	__int64_t time;

#ifdef CLOCK_CTIME_IS_ABSOLUTE
	if (svtimer_flags & SVTIMER_FAST) {
		time = tstoclock(ts, TIME_ABS);
		return clock_prtimeout_nothrd(cpuid(),
					      (void (*)())sv_timedout,
					      (void *)svp,
					      time, kt);
	}
#else

	if ((svtimer_flags & SVTIMER_FAST) && ts->tv_nsec)
		dofast = 1;
	else if (ts->tv_nsec && (ts->tv_nsec < NSEC_PER_TICK)) {
		ts->tv_nsec = NSEC_PER_TICK;
	}

#endif
	if (dofast && ts->tv_nsec && (ts->tv_nsec < (fastick * NSEC_PER_USEC)))
		ts->tv_nsec = fastick * NSEC_PER_USEC;

	/*
	 * Use the nothrd timeout interface here since all sv_timedout is
	 * going to do is make another thread runnable anyway (i.e.
	 * avoid the wake a thread to wake a thread overhead).
	 */
	if (dofast) 
		time = timespec_to_tick(ts, NSEC_PER_SEC / fasthz);
	else
		time = timespec_to_tick(ts, NSEC_PER_TICK);
	if (!(svtimer_flags & SVTIMER_TRUNC)) {
		/*
		 * We need to add one to the timeout to make sure that
		 * we do not return early. We do not know
		 * where in the current tick we currently are.
		 */
		time++;
	}
	/*
	 * If someone is asking for more then the timeout function can
	 * handle then give them MAXTIME (same as MAXINT). They most likly
	 * did not want that much to start with, and it was a mistake.
	 */
#define MAXTIME 2147483647
	if (time > MAXTIME) 
		time = MAXTIME;
	return dofast ? fast_itimeout_nothrd((void (*)())sv_timedout,
					     (void *)svp,
					     time,
					     pltimeout, kt)
		      : itimeout_nothrd((void (*)())sv_timedout,
					(void *)svp,
					time,
					pltimeout, kt);
}

static void
sv_clear_timeout(toid_t id, timespec_t *rts, char semret)
{
	union	c_tid c_tid;	/* pick this name to use macro in callo.h */
	int64_t early = 0;
	extern int fasthz;
	c_id = id;
	if (semret != SV_DQ_TIMEOUT)
		early = untimeout(c_id);
	if (rts)
		if (c_fast)
			tick_to_timespec(early, rts, NSEC_PER_SEC/fasthz);
		else
			tick_to_timespec(early, rts, NSEC_PER_TICK);
}

/*
 * timed wait
 */
void
sv_bitlock_timedwait(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	uint		*lock,		/* lock to be released */
	register uint	bit,		/* lock bit */
	register int	s,		/* bitlock return value */
	int		svtimer_flags,	/* fast timeouts and/or truncate */
	timespec_t	*ts,		/* time to wait */
	timespec_t	*rts)		/* remaining time if woken early */
{
	toid_t		id;
	kthread_t	*kt = private.p_curkthread;

	ASSERT(lock && s);
	ASSERT(bitlock_islocked(lock, bit));

	if (KT_LOCK_BITS(kt) != (uint *)lock)
		kt_nested_lock(kt);

	if (ts) {
		id = sv_set_timeout(svtimer_flags, svp, ts);
		/* If the time to wait is longer than 0.5 seconds,
		 * indicate a long term wait so the sleeping thread
		 * is not counted toward the system load average. */
		if (ts->tv_sec > 0 || ts->tv_nsec > 500000000)
			flags |= PLTWAIT;
	}
	sv_queue(svp, flags, BITLOCK, lock, bit, s);
	if (ts)
		sv_clear_timeout(id, rts, kt->k_prtn);
}

int
sv_bitlock_timedwait_sig(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	uint		*lock,		/* lock to be released */
	register uint	bit,		/* lock bit */
	int		s,		/* bitlock return value */
	int		svtimer_flags,	/* fast timeouts and/or truncate */
	timespec_t	*ts,		/* time to wait */
	timespec_t	*rts)		/* remaining time if woken early */
{
	kthread_t	*kt = private.p_curkthread;
	toid_t		id;

	ASSERT(lock && s);
	ASSERT(bitlock_islocked(lock, bit));

	if (KT_LOCK_BITS(kt) != (uint *)lock)
		kt_nested_lock(kt);

	bla_isleep(&s);

	if (thread_interrupted(kt)) {
		if (KT_LOCK_BITS(kt) != (uint *)lock)
			nested_bitunlock(lock, bit);
		kt_unlock(kt, s);
		bla_iunsleep();
		svmeter_sigbrk(svp);
		/*
		 * Need to set rts to reflect that we did not sleep.
		 */
		if (ts && rts)
			*rts = *ts;
		return -1;
	}

	if (ts)
		id = sv_set_timeout(svtimer_flags, svp, ts);
	sv_queue(svp, flags|SV_BREAKABLE, BITLOCK, lock, bit, s);
	if (ts)
		sv_clear_timeout(id, rts, kt->k_prtn);

	bla_iunsleep();

	if (!kt->k_prtn && thread_interrupted(kt))
		return -1;
	return 0;
}

void
sv_timedwait(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register void	*mp,		/* mutex to be released */
	register int	ospl,		/* return value from mutex_spinlock */
	int		svtimer_flags,	/* fast timeouts and/or truncate */
	timespec_t	*ts,		/* time to wait */
	timespec_t	*rts)		/* remaining time if woken early */
{
	/*REFERENCED(!MP)*/
	kthread_t	*kt = private.p_curkthread;
	unlock_func_t	unlock_func;
	toid_t		id;

	ASSERT(!mp || !ospl ||
		((*((lock_t *)mp) & MUTEX_SPINLOCKED) == MUTEX_SPINLOCKED));

	unlock_func = !mp ? NOLOCK : ospl ? SPINLOCK : MUTEX;

	if (!ospl)
		ospl = kt_lock(kt);
	else
		kt_nested_lock(kt);
	if (ts)
		id = sv_set_timeout(svtimer_flags, svp, ts);
	sv_queue(svp, flags, unlock_func, mp, 0, ospl);
	if (ts)
		sv_clear_timeout(id, rts, kt->k_prtn);
}

/*
 * Same as sv_timedwait_sig except the caller is also notified
 * when it wakes-up due to a timeout.
 *
 * Returns:  0 normal sv_signal or sv_broadcast operation
 *          -1 interrupted by a signal
 *          -2 timeout expired
 */
int
sv_timedwait_sig_notify(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register void	*mp,		/* mutex to be released */
	int		ospl,		/* return value from mutex_spinlock */
	int		svtimer_flags,	/* fast timeouts and/or truncate */
	timespec_t	*ts,		/* time to wait */
	timespec_t	*rts)		/* remaining time if woken early */
{
	int ret;
	kthread_t *kt = private.p_curkthread;

	ret = sv_timedwait_sig(svp, flags, mp, ospl, svtimer_flags, ts, rts);
	
	if (ret == 0 && kt->k_prtn == SV_DQ_TIMEOUT)
		ret = -2; /* timed out */

	return (ret);
}

/*
 * Put thread to sleep on sync variable queue.
 * Breakable by a signal.
 */
int
sv_timedwait_sig(
	register sv_t	*svp,		/* sync queue */
	register int	flags,		/* pri, timer, etc. */
	register void	*mp,		/* mutex to be released */
	int		ospl,		/* return value from mutex_spinlock */
	int		svtimer_flags,	/* fast timeouts and/or truncate */
	timespec_t	*ts,		/* time to wait */
	timespec_t	*rts)		/* remaining time if woken early */
{
	kthread_t	*kt = private.p_curkthread;
	unlock_func_t	unlock_func;
	toid_t		id;

	ASSERT(!mp || !ospl ||
		((*((lock_t *)mp) & MUTEX_SPINLOCKED) == MUTEX_SPINLOCKED));

	unlock_func = !mp ? NOLOCK : ospl ? SPINLOCK : MUTEX;

	if (!mp || !ospl)
		ospl = kt_lock(kt);
	else
		kt_nested_lock(kt);

	bla_isleep(&ospl);

	if (thread_interrupted(kt)) {
		if (unlock_func == MUTEX) {
			kt_unlock(kt, ospl);
			mutex_unlock(mp);
		} else {
			if (unlock_func == SPINLOCK)
				nested_spinunlock(mp);
			kt_unlock(kt, ospl);
		}
		bla_iunsleep();
		svmeter_sigbrk(svp);
		/*
		 * Need to set rts to reflect that we did not sleep.
		 */
		if (ts && rts)
			*rts = *ts;
		return -1;
	}
	if (ts)
		id = sv_set_timeout(svtimer_flags, svp, ts);
	sv_queue(svp, flags | SV_BREAKABLE, unlock_func, mp, 0, ospl);
	if (ts)
		sv_clear_timeout(id, rts, kt->k_prtn);

	bla_iunsleep();

	if (kt->k_prtn == 0 && thread_interrupted(kt))
		return -1; /* signalled */

	return 0;
}


#ifdef DEBUG

#define NTESTMUTEX 100
mutex_t	test_mutex[NTESTMUTEX];
int mutex_test_initialized;
#include <sys/idbgentry.h>

void
mutex_test_init()
{
	int i;

	if (!compare_and_swap_int(&mutex_test_initialized, 0, 1))
		return;
	for (i = 0; i < NTESTMUTEX; i++)
		init_mutex(&test_mutex[i], MUTEX_DEFAULT, "test_mutex", i);
}

void
mutex_clear_range(int low, int high)
{
	while (low <= high) {
		if (mutex_mine(&test_mutex[low]))
			mutex_unlock(&test_mutex[low]);
		low++;
	}
}

int
mutex_test(int distance)
{
	int level = NTESTMUTEX - 1;
	int target;

	if (!mutex_test_initialized)
		return -1;

	while (level >= 0) {
		if (mutex_mine(&test_mutex[level]))
			break;
		level--;
	}
	level++;

	target = level + distance;
	if (target >= NTESTMUTEX)
		target = NTESTMUTEX - 1;
	else if (target < 0)
		target = 0;
	mutex_clear_range(target, level);
	if (target > 0) {
		for (level = target; level < NTESTMUTEX; level++)
			ASSERT(!mutex_mine(&test_mutex[level]));
		mutex_lock(&test_mutex[target], 0);
	}
	return target;
}

void
mutex_test_clear()
{
	mutex_clear_range(0, NTESTMUTEX - 1);
}

void
mutex_tester_init()
{
	add_exit_callback(current_pid(), 0,
			(void (*)(void *))mutex_test_clear, 0);
}
#endif

mutex_t sv_context_mutex;
sv_t    sv_context_sv;

void
sv_context_switch(int count)
{
	static int ctx_init;

	if (compare_and_swap_int(&ctx_init, 0, 1)) {
		init_mutex(&sv_context_mutex, MUTEX_DEFAULT, "ctxsw", 0);
		init_sv(&sv_context_sv, SV_DEFAULT, "ctxsw", 0);
		ctx_init = 2;
	}
	while (ctx_init != 2)
		SYNCHRONIZE();

	mutex_lock(&sv_context_mutex, PZERO);
	while (count--) {
		sv_signal(&sv_context_sv);
		sv_wait_sig(&sv_context_sv, PPIPE, &sv_context_mutex, 0);
		mutex_lock(&sv_context_mutex, PZERO);
	}
	sv_signal(&sv_context_sv);
	mutex_unlock(&sv_context_mutex);
}
