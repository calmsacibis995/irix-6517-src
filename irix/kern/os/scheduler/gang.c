/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include "os/proc/pproc_private.h"	/* XXX bogus */
#include "sys/kmem.h"
#include "sys/par.h"
#include "sys/schedctl.h"
#include "sys/idbgentry.h"
#include "space.h"
#include <sys/uthread.h>
#include "sys/runq.h"
#include "sys/atomic_ops.h"
#include "sys/numa.h"
#include "sys/prctl.h"
#include <sys/cpumask.h>
#include <sys/sema_private.h>
#include <sys/pda.h>
#include <sys/numa.h>

#ifdef DEBUG
#define GANG_TRACING		1
#define GDB_HEAVY_SANITY	1
#endif /* DEBUG */

#define GDB_INACTIVE	0			/* gang uninitialized */
#define GDB_UNBLOCKED   1 			/* gang running */
#define GDB_BLOCKED     2			/* gang waiting */
#define GDB_SINGLE 	3			/* gang in SGS_SINGLE mode */

static struct q_element_s gangq;		/* gangs in gang_schedule() */
static lock_t           gangqlock;		/* protects gangq */
static cpumask_t	gang_bindings;		/* global cpu bindings */
static lock_t		gang_bindingslock;	/* protects gang_bindings */

#ifdef GANG_TRACING

typedef struct gang_trace_s {
	uthread_t	*gt_running;		/* running thread */
	uthread_t	*gt_target;		/* target thread */
	caddr_t		gt_pc;			/* caller pc */
	const char	*gt_action;		/* action */
	long		gt_data1;		/* action data1 */
	long		gt_data2;		/* action data2 */
} gang_trace_t;

#define GANG_NTRACE	100

static const char *_GT_GDB_SETSTATE = "gang state change";
static const char *_GT_THR_SETSTATE = "thread state change";
static const char *_GT_NREADY_INC = "nready++";
static const char *_GT_NREADY_DEC = "nready--";
static const char *_GT_SET_BLOCKED = "gdb->g_blocked = ";
static const char *_GT_SET_LAST = "gdb->g_last = ";
static const char *_GT_GANGQ_RMQ = "remove from gangq";
static const char *_GT_GANGQ_ADDQ = "add to gangq";

#else /* !GANG_TRACING */

#define GANG_TRACE(gdb, target_ut, action, data1, data2) /* null */

#endif /* !GANG_TRACING */

typedef struct gdb_s {
	int 		g_nready;	/* num non-blocked thrs */
	lock_t		g_lock;		/* protects gdb, ut->ut_gstate */
	int 		g_nthreads;	/* number of gang threads */
	struct q_element_s g_thread;	/* list of gang threads */
	cpumask_t	g_bindings;	/* cpu bindings for this gang */
	sv_t		g_sv;		/* sv for gang threads */
	sv_t		g_sv2;		/* sv for GANG_LAST thread */
	shaddr_t 	*g_sa;		/* gang shaddr */
	int 		g_state;	/* gang state */
	int		g_blocked;	/* GANG_LAST thread blocking? */
	uthread_t	*g_last;	/* GANG_LAST; waits on g_sv2 */
	struct q_element_s g_queue;	/* link on gangq */
#ifdef GANG_TRACING
	gang_trace_t	g_tbuf[GANG_NTRACE];	/* trace buffer */
	int		g_tbuf_cur;		/* ... current position */
#endif /* GANG_TRACING */
} gdb_t;

static int gang_find_bindings(gdb_t*);
static const char *gstate_to_string(char gs);
static const char *gdbstate_to_string(int gstate);
static void idbg_gdb(__psint_t);
#ifdef GANG_TRACING
static void idbg_gangtrace(__psint_t);
#endif /* GANG_TRACING */

#define _GANGQ_PARENT		((void*)1L)
#define GANG_FIRST 		((gdb_t *)(&gangq)->qe_forw->qe_parent)
#define GANG_NEXT(gdb) 		((gdb_t *)(gdb)->g_queue.qe_forw->qe_parent)
#define GANG_END(gdb) 		(gdb == (gdb_t *)_GANGQ_PARENT)

#define UT_ISGANG(ut)		((ut)->ut_gstate != GANG_UNDEF)
#define KT_ISGANG(kt)		UT_ISGANG(KT_TO_UT(kt))

#define UT_ISGANG_READY(ut)	\
	(ut->ut_gstate == GANG_RUNNING || ut->ut_gstate == GANG_NONE || \
		ut->ut_gstate == GANG_STARTED || ut->ut_gstate == GANG_LAST)

/*
 * Gang state manipulation functions - used to encapsulate tracing and
 * assertions.  Trace code expands to empty statements in a non-DEBUG
 * build.
 */

#define GANG_THR_SETSTATE(gdb, ut, state) \
	do { \
		GANG_TRACE((gdb), (ut), _GT_THR_SETSTATE, \
			(ut)->ut_gstate, (state)); \
		(ut)->ut_gstate = (state); \
	} while(0)
#define GANG_GDB_SETSTATE(gdb, state) \
	do { \
		GANG_TRACE((gdb), 0, _GT_GDB_SETSTATE, \
			(gdb)->g_state, (state)); \
		(gdb)->g_state = (state); \
	} while(0)
#define GANG_NREADY_INC(gdb, ut) \
	do { \
		(gdb)->g_nready++; \
		GANG_TRACE((gdb), (ut), _GT_NREADY_INC, gdb->g_nready, 0); \
		ASSERT((gdb)->g_nready > 0); \
		ASSERT((gdb)->g_nready <= (gdb)->g_nthreads); \
	} while(0)
#define GANG_NREADY_DEC(gdb, ut) \
	do { \
		(gdb)->g_nready--; \
		GANG_TRACE((gdb), (ut), _GT_NREADY_DEC, gdb->g_nready, 0); \
		ASSERT((gdb)->g_nready >= 0); \
		ASSERT((gdb)->g_nready <= (gdb)->g_nthreads); \
	} while(0)
#define GANG_GDB_SETBLOCKED(gdb, blocked) \
	do { \
		GANG_TRACE((gdb), 0, _GT_SET_BLOCKED, (long)(blocked), 0); \
		(gdb)->g_blocked = (blocked); \
	} while(0)
#define GANG_GDB_SETLAST(gdb, last_ut) \
	do { \
		GANG_TRACE((gdb), 0, _GT_SET_LAST, (long)(last_ut), 0); \
		(gdb)->g_last = (last_ut); \
	} while(0)
#define GANG_ADDGANGQ(gdb) \
	do { \
		GANG_TRACE((gdb), 0, _GT_GANGQ_ADDQ, 0, 0); \
		ASSERT(spinlock_islocked(&gangqlock)); \
		addgangq(gdb); \
		GANGQ_DEBUG_CHECK(); \
	} while(0)
#define GANG_RMGANGQ(gdb) \
	do { \
		GANG_TRACE((gdb), 0, _GT_GANGQ_RMQ, 0, 0); \
		ASSERT(spinlock_islocked(&gangqlock)); \
		rmq(&(gdb)->g_queue); \
		GANGQ_DEBUG_CHECK(); \
	} while(0)

#ifdef DEBUG

static void
GDB_DEBUG_CHECK(gdb_t *gdb)
{
#if GDB_HEAVY_SANITY
	struct q_element_s	*list;
	struct q_element_s	*qe;
	int			nready;
	int			nthreads;

	ASSERT(spinlock_islocked(&gdb->g_lock));

	list = &gdb->g_thread;
	nready = 0;
	nthreads = 0;
	for (qe = list->qe_forw; qe != list; qe = qe->qe_forw) {
		kthread_t	*kt = (kthread_t*) qe->qe_parent;
		uthread_t	*ut = KT_TO_UT(kt);
		if (UT_ISGANG_READY(ut))
			nready++;
		nthreads++;
	}
	ASSERT(nready == gdb->g_nready);
	ASSERT(nthreads == gdb->g_nthreads);
	ASSERT(gdb->g_nready <= gdb->g_nthreads);
	ASSERT(gdb->g_nready >= 0);
#endif /* GDB_HEAVY_SANITY */

	/* XXX: gangq isn't locked, but this field shouldn't change */
	ASSERT(gangq.qe_parent == (void*)_GANGQ_PARENT);
}

#define GANGQ_DEBUG_CHECK() \
	do { \
		ASSERT(issplhi(getsr())); \
		ASSERT(spinlock_islocked(&gangqlock)); \
		ASSERT(gangq.qe_parent == (void*)_GANGQ_PARENT); \
	} while(0)

#else /* !DEBUG */
#define GDB_DEBUG_CHECK(gdb)	/* nothing */
#define GANGQ_DEBUG_CHECK()	/* nothing */
#endif /* !DEBUG */

#ifdef GANG_TRACING
static void
GANG_TRACE(gdb_t *gdb, uthread_t *target_ut, const char *action,
	long data1, long data2)
{
	gang_trace_t *gt = &gdb->g_tbuf[gdb->g_tbuf_cur];

	ASSERT(gdb->g_tbuf_cur >= 0 && gdb->g_tbuf_cur < GANG_NTRACE);

	if (++gdb->g_tbuf_cur == GANG_NTRACE)
		gdb->g_tbuf_cur = 0;

	gt->gt_running = curuthread;
	gt->gt_target = target_ut;
	gt->gt_pc = (caddr_t)__return_address;
	gt->gt_action = action;
	gt->gt_data1 = data1;
	gt->gt_data2 = data2;
}
#endif /* GANG_TRACING */

__inline static struct q_element_s*
gang_thr_qe(gdb_t *gdb, kthread_t *kt)
{
	struct q_element_s	*list;
	struct q_element_s	*qe;

	ASSERT(issplhi(getsr()));
	ASSERT(spinlock_islocked(&gdb->g_lock));

	/* We don't want to unnecessarily add a member to the kthread
	 * struct.  This code should only execute in non-time-critical
	 * paths, e.g. gang_detach(), since it is a bit heavyweight.
	 */

	list = &gdb->g_thread;
	for (qe = list->qe_forw; qe != list; qe = qe->qe_forw)
		if (qe->qe_parent == (void*)kt)
			return qe;
	return NULL;
}

__inline static void
addgangq(gdb_t* gdb)
{
	ASSERT(spinlock_islocked(&gangqlock));
	ASSERT(!q_onq(&gdb->g_queue));
	GANGQ_DEBUG_CHECK();
	gangq.qe_back->qe_forw = &gdb->g_queue;
	gdb->g_queue.qe_forw = &gangq;
	gdb->g_queue.qe_back = gangq.qe_back;
	gangq.qe_back = &gdb->g_queue;
	GANGQ_DEBUG_CHECK();
}

__inline static void 
wakegang(gdb_t *gdb, int state)
{ 
	ASSERT(spinlock_islocked(&gdb->g_lock));

	/*
	 * Note that it is unsafe to set the GANG_LAST thread to
	 * anything other than GANG_LAST or GANG_NONE.  For example,
	 * if the thread is set to GANG_RUNNING there is a race where
	 * we might have caught the thread in sv_wait() between
	 * unlocking the gdb lock and calling gang_block().  Also, the
	 * new state must be a ready state, since we increment
	 * gdb->g_nready.
	 */
	ASSERT(state == GANG_LAST || state == GANG_NONE);

	if (gdb->g_blocked) {
		uthread_t *ut = gdb->g_last;

		ASSERT(gdb->g_last);
		ASSERT(ut->ut_gstate == GANG_EARN ||
			ut->ut_gstate == GANG_UNDEF);

		if (ut->ut_gstate == GANG_EARN) {
			GANG_THR_SETSTATE(gdb, ut, state);
			GANG_NREADY_INC(gdb, ut);
		}

		/*
		 * If we wake the GANG_LAST thread, then we're
		 * responsible for clearing gdb->g_blocked.  If we
		 * don't check that we're the one to wake the thread,
		 * there would be a race between clearing
		 * gdb->g_blocked and a signal interrupting the
		 * GANG_LAST thread, thus interfering with the proper
		 * cleanup in gang_clear().
		 */
		if (sv_signal(&gdb->g_sv2)) {
			GANG_GDB_SETBLOCKED(gdb, 0);
		}
	}
}

static void
unset_bindings(uthread_t *ut, gdb_t *gdb)
{
	nested_spinlock(&gang_bindingslock);
	CPUMASK_CLRB(gang_bindings, ut->ut_gbinding);
	nested_spinunlock(&gang_bindingslock);
	CPUMASK_CLRB(gdb->g_bindings, ut->ut_gbinding);
	ut->ut_gbinding = CPU_NONE;
}

void
init_gang()
{
	init_q_element(&gangq, _GANGQ_PARENT);
	spinlock_init(&gang_bindingslock, "gang_bindingslock");
	spinlock_init(&gangqlock, "gangqlock");
	CPUMASK_CLRALL(gang_bindings);

#ifdef GANG_TRACING
	idbg_addfunc("gangtrace", idbg_gangtrace);
#endif /* GANG_TRACING */
	idbg_addfunc("gdb", idbg_gdb);
}

__inline static void
gang_count(gdb_t *gdb, kthread_t *kt)
{
	ASSERT(gdb && kt);
	ASSERT(KT_ISGANG(kt));

	kt_nested_lock(kt);

	if (((kt->k_flags & KT_SLEEP) || 
		(kt->k_onrq == CPU_NONE &&
		 kt->k_sonproc == CPU_NONE)) && 
		KT_TO_UT(kt)->ut_gstate == GANG_NONE) {
		GANG_THR_SETSTATE(gdb, KT_TO_UT(kt), GANG_BLOCKED);
		GANG_NREADY_DEC(gdb, KT_TO_UT(kt));
	} else if (KT_TO_UT(kt)->ut_gstate == GANG_NONE) {
		GANG_THR_SETSTATE(gdb, KT_TO_UT(kt), GANG_STARTED);
	}

	kt_nested_unlock(kt);
}

static void 
gang_count_all(gdb_t *gdb)
{
	struct q_element_s	*list;
	struct q_element_s	*qe;
	kthread_t		*kt;

	ASSERT(issplhi(getsr()));
	ASSERT(spinlock_islocked(&gdb->g_lock));

	list = &gdb->g_thread;
	for (qe = list->qe_forw; qe != list; qe = qe->qe_forw) {
		kt = (kthread_t*) qe->qe_parent;
		gang_count(gdb, kt);
	}

	ASSERT(gdb->g_nready <= gdb->g_nthreads);
	ASSERT(gdb->g_nready >= 0);
}

static void
gang_wakeup(kthread_t *kt, void *arg)
{
	gdb_t		*gdb = (gdb_t*) arg;
	uthread_t	*ut = KT_TO_UT(kt);

	if (!UT_ISGANG(ut))
		return;

	ASSERT(ut->ut_gstate == GANG_EARN);

	GANG_THR_SETSTATE(gdb, ut, GANG_RUNNING);
	GANG_NREADY_INC(gdb, ut);
}

static void
gang_endrun_wakeup(kthread_t *kt, void *arg)
{
	gdb_t		*gdb = (gdb_t*) arg;
	uthread_t	*ut = KT_TO_UT(kt);

	if (!UT_ISGANG(ut))
		return;

	ASSERT(ut->ut_gstate == GANG_EARN);

	GANG_THR_SETSTATE(gdb, ut, GANG_NONE);
	GANG_NREADY_INC(gdb, ut);
}

__inline static int
available(cpuid_t cpu, int cpuset)
{
	return !CPUMASK_TSTB(gang_bindings, cpu) && cpu_enabled(cpu)
		&& !CPUMASK_TSTB(global_cpuset.cs_cpus, cpu)
		&& cpuset_runok(cpuset, cpu_to_cpuset[cpu]);
}

void
gang_free(shaddr_t *sa)
{
	gdb_t           *gdb = sa->s_gdb;

	if (is_batch(curthreadp) || is_bcritical(curthreadp))
		return;

	ASSERT(gdb->g_nthreads == 0);
	ASSERT(gdb->g_nready == 0);
	ASSERT(curuthread->ut_gstate == GANG_UNDEF);

	ASSERT(!gdb->g_blocked);
	ASSERT(!gdb->g_last);

	ASSERT(!SV_WAITER(&gdb->g_sv));
	ASSERT(!SV_WAITER(&gdb->g_sv2));

	ASSERT(!q_onq(&gdb->g_queue));

	spinlock_destroy(&gdb->g_lock);
	sv_destroy(&gdb->g_sv);
	sv_destroy(&gdb->g_sv2);

	kmem_free((void *) gdb, sizeof(*gdb));
	sa->s_gdb = NULL;
}

__inline static int
gang_kick(gdb_t *gdb) 
{
	ASSERT(spinlock_islocked(&gdb->g_lock));

	if (gdb->g_blocked && gang_find_bindings(gdb)) {
		wakegang(gdb, GANG_LAST);
		GDB_DEBUG_CHECK(gdb);
		nested_spinunlock(&gdb->g_lock);
		return 1;
	}

	GDB_DEBUG_CHECK(gdb);
	nested_spinunlock(&gdb->g_lock);
	return 0;
}

static void
gang_newlast(gdb_t *gdb, uthread_t *ignore_ut)
{
	struct q_element_s 	*list;
	struct q_element_s	*thrqe;

	/*
	 * Choose a new GANG_LAST thread from the remaining gang
	 * threads.
	 */

	ASSERT(!gdb->g_blocked);
	ASSERT(!gdb->g_last);
	ASSERT(gdb->g_nthreads);
	ASSERT(!gdb->g_nready);

	for (list = &gdb->g_thread, thrqe = list->qe_forw;
		thrqe != list; thrqe = thrqe->qe_forw) {
		kthread_t *kt = (kthread_t*) thrqe->qe_parent;
		uthread_t *ut = KT_TO_UT(kt);

		ASSERT(UT_ISGANG(ut));

		/*
		 * Ignore ignore_ut, since it is having its gang state
		 * cleared.
		 */
		if (ut == ignore_ut)
			continue;

		/* Don't wait for blocked threads. */
		if (ut->ut_gstate == GANG_BLOCKED)
			continue;

		/*
		 * Since gdb->g_nready is zero, all the threads in the
		 * gang must either be GANG_BLOCKED or GANG_EARN.
		 */
		ASSERT(ut->ut_gstate == GANG_EARN);

		/*
		 * Wake the thread if it's waiting on the gang sv.
		 */
		kt_nested_lock(kt);
		if (kt->k_wchan) {
			/*
			 * GANG_EARN threads must be waiting on the
			 * gang sv, if they're waiting at all.
			 */
			ASSERT(kt->k_wchan == (caddr_t)&gdb->g_sv);
			setrun(kt);
		}
		kt_nested_unlock(kt);

		GANG_THR_SETSTATE(gdb, ut, GANG_LAST);
		GANG_NREADY_INC(gdb, ut);
		break;
	}
}

/*
 * Gang thread transitioning to a non-gang-runnable state.
 */
static void
gang_clear(gdb_t *gdb, uthread_t *ut, int state)
{
	ASSERT(UT_ISGANG(ut));

	if (ut->ut_gstate == GANG_RUNNING || ut->ut_gstate == GANG_NONE ||
		ut->ut_gstate == GANG_STARTED || ut->ut_gstate == GANG_LAST) {
		GANG_NREADY_DEC(gdb, ut);
	}

	if (ut->ut_gbinding != CPU_NONE) 
		unset_bindings(ut, gdb);

	GANG_THR_SETSTATE(gdb, ut, state);

	/*
	 * The new state must be a non-runnable state, otherwise
	 * there's going to be an error with the nready count and the
	 * gang won't be scheduled.
	 */
	ASSERT(!UT_ISGANG_READY(ut));

	/*
	 * If we're clearing the gang state of the GANG_LAST thread,
	 * clear gdb->g_blocked.  This also eliminates the problem of
	 * running wakegang() below on a thread which is about to
	 * terminate.
	 */
	if (gdb->g_last == ut) {
		#pragma mips_frequency_hint NEVER
		if (q_onq(&gdb->g_queue)) {
			nested_spinlock(&gangqlock);
			GANG_RMGANGQ(gdb);
			nested_spinunlock(&gangqlock);
		}
		GANG_GDB_SETLAST(gdb, 0);
		GANG_GDB_SETBLOCKED(gdb, 0);
	}

	if (gdb->g_nthreads && !gdb->g_nready) {
		if (gdb->g_blocked) {
			if (gang_find_bindings(gdb))
				wakegang(gdb, GANG_LAST);
		}
		else {
			gang_newlast(gdb, ut);
		}
	}
}	

static void
gang_detach(gdb_t *gdb, uthread_t *ut)
{
	pid_t			pid;
	struct q_element_s	*qe;

	ASSERT(issplhi(getsr()));
	ASSERT(spinlock_islocked(&gdb->g_lock));

	if (!UT_ISGANG(ut))
		return;

	pid = UT_TO_VPROC(ut)->vp_pid;

	if (gdb->g_sa->s_master == pid && gdb->g_state == GDB_SINGLE) {
		GANG_GDB_SETSTATE(gdb, GDB_UNBLOCKED);
		sv_foreach_wake(&gdb->g_sv, gang_endrun_wakeup, gdb);
		wakegang(gdb, GANG_NONE);
	}	

	gdb->g_nthreads--;
	ASSERT(gdb->g_nthreads >= 0);

	qe = gang_thr_qe(gdb, UT_TO_KT(ut));
	ASSERT(qe);
	ASSERT(qe->qe_parent == (void*)UT_TO_KT(ut));
	rmq(qe);
	kmem_free(qe, sizeof(*qe));

	gang_clear(gdb, ut, GANG_UNDEF);
	ASSERT(gdb->g_nready <= gdb->g_nthreads); 

	if (gdb->g_sa->s_master == pid)
		gdb->g_sa->s_master = 0;
}

void 
gang_unlink(kthread_t *kt) 
{
	gdb_t			*gdb;
	uthread_t		*ut = KT_TO_UT(kt);
	int			ss;
	shaddr_t		*sa;

	/*
	 * The locking in this function is somewhat obscene, but it is
	 * important to notice that we can't attempt to take the
	 * kthread lock while holding the gang lock, since there would
	 * be a deadlock with gang_block().  Note that the shaddr
	 * can't go away, since we're either in the context of a gang
	 * thread or we have it held via VPROC_LOOKUP().
	 */

	ss = kt_lock(kt);

	if (is_batch(kt) || is_bcritical(kt)) {
		kt_unlock(kt, ss);
		return;
	}

	if (!ut->ut_job || !(sa = UT_TO_PROC(ut)->p_shaddr)) {
		kt_unlock(kt, ss);
		return;
	}

	gdb = sa->s_gdb;

	kt_unlock(kt, ss);
	ss = mutex_spinlock(&gdb->g_lock);

	if (KT_ISGANG(kt))
		gang_detach(gdb, ut);

	GDB_DEBUG_CHECK(gdb);
	mutex_spinunlock(&gdb->g_lock, ss);
	ss = kt_lock(kt);

	if (!KT_ISBASEPS(kt))
		kt->k_binding = CPU_NONE;

	/*
	 * Testing the kthread's wchan vs. the gang sync objects is
	 * safe here, since the gang state can't be freed unless this
	 * thread is destroyed.
	 */

	if (kt->k_wchan == (caddr_t)&gdb->g_sv ||
		kt->k_wchan == (caddr_t)&gdb->g_sv2) {
		setrun(kt);
	}

	kt_unlock(kt, ss);
}

void 
gang_create(shaddr_t *sa)
{
	gdb_t *gdb;
	int s;
	if (is_batch(curthreadp) || is_bcritical(curthreadp))
		return;

	s = mutex_spinlock(&sa->s_rupdlock);
	if ((gdb = sa->s_gdb) == NULL) {
		gdb = sa->s_gdb = (gdb_t *) kmem_zalloc(sizeof(gdb_t),
						KM_SLEEP|KM_CACHEALIGN);
		init_q_element(&gdb->g_queue, gdb);
		init_q_element(&gdb->g_thread, NULL);
		sv_init(&gdb->g_sv, SV_DEFAULT, "gangsv");
		sv_init(&gdb->g_sv2, SV_DEFAULT, "gangsv2");
		spinlock_init(&gdb->g_lock, "gdb_lock");
		gdb->g_sa = sa;
		CPUMASK_CLRALL(gdb->g_bindings);
		gdb->g_state = GDB_INACTIVE;
		gdb->g_nready = 0;
		gdb->g_nthreads = 0;
		gdb->g_blocked = 0;
		gdb->g_last = 0;

#ifdef GANG_TRACING
		gdb->g_tbuf_cur = 0;
		bzero(gdb->g_tbuf, sizeof(gdb->g_tbuf));
#endif /* GANG_TRACING */
	}
	mutex_spinunlock(&sa->s_rupdlock, s);
}

void 
gang_link(shaddr_t *sa, uthread_t *ut)
{
	gdb_t			*gdb = sa->s_gdb;
	int			s;
	struct q_element_s	*qe;

	if (is_batch(UT_TO_KT(ut)) || is_bcritical(UT_TO_KT(ut)))
		return;

	ASSERT(isspl0(getsr()));

	/* Preallocate, since kmem_zalloc() could sleep.
	*/ 
	qe = (struct q_element_s*) kmem_zalloc(sizeof(*qe),
		KM_SLEEP|KM_CACHEALIGN);
	init_q_element(qe, (void*)UT_TO_KT(ut));

	s = mutex_spinlock(&gdb->g_lock);

	ASSERT(ut);
	ASSERT(ut->ut_gstate == GANG_UNDEF);

	if (ut->ut_job) {
		gdb->g_nthreads++;
		GANG_NREADY_INC(gdb, ut);
		if (curuthread->ut_gstate != GANG_NONE) {
			GANG_THR_SETSTATE(gdb, ut, GANG_STARTED);
		}
		else {
			GANG_THR_SETSTATE(gdb, ut, GANG_NONE);
		}
		pushq(&gdb->g_thread, qe);
		qe = NULL;
	}

	GDB_DEBUG_CHECK(gdb);
	mutex_spinunlock(&gdb->g_lock, s);

	if (qe)
		kmem_free((void*)qe, sizeof(*qe));
}

static void
set_bindings(kthread_t * kt, gdb_t* gdb, int index) 
{
	ASSERT(index > CPU_NONE && index < maxcpus);
	CPUMASK_SETB(gang_bindings, index);
	CPUMASK_SETB(gdb->g_bindings, index);
	KT_TO_UT(kt)->ut_gbinding = index;
	kt->k_binding = index;
}	

static void
clear_bindings(gdb_t *g)
{
	struct q_element_s *qe;
	struct q_element_s *list;

	ASSERT(issplhi(getsr()));
	ASSERT(spinlock_islocked(&g->g_lock));

	CPUMASK_CLRM(gang_bindings, g->g_bindings);
	CPUMASK_CLRALL(g->g_bindings);

	list = &g->g_thread;
	for (qe = list->qe_forw; qe != list; qe = qe->qe_forw) {
		kthread_t *kt = (kthread_t*)qe->qe_parent;
		KT_TO_UT(kt)->ut_gbinding = CPU_NONE;
	}
}

#ifdef NUMA_SCHED
static void * 
check_bindings(cpuid_t cpu, void *arg)
{
	kthread_t *kt = (kthread_t *) arg;
	if (available (cpu, kt->k_cpuset)) 
		return (void *) pdaindr[cpu].pda;
	else
		return 0;
}
#endif

static int
find_a_cpu(kthread_t *kt, gdb_t *gdb, int iszero)
{
	int i;
	for (i = maxcpus - 1; i > CPU_NONE; i--) {
		if (!available(i, kt->k_cpuset)) 
			continue;
		else {
			set_bindings(kt, gdb, i);
			return 1;
		}
	}
	if (!iszero) 
		return 0;
	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(gdb->g_bindings, i)) {
			set_bindings(kt, gdb, i);
			return 1;
		}
	}
	return 1;
}

static int
gang_find_bindings(gdb_t *gdb)
{
	kthread_t		*kt;
	int			iszero = 0;
	struct q_element_s	*thrlist;
	struct q_element_s	*qe;

#ifdef NUMA_SCHED
	pda_t *pda;
#endif

	ASSERT(gdb);
	ASSERT(spinlock_islocked(&gdb->g_lock));

	if (!CPUMASK_IS_ZERO(gdb->g_bindings)) 
		return 0;

	if (gdb->g_state == GDB_SINGLE)
		return 1;

	nested_spinlock(&gang_bindingslock); 

	iszero = CPUMASK_IS_ZERO(gang_bindings);

	thrlist = &gdb->g_thread;

	for (qe = thrlist->qe_forw; qe != thrlist; qe = qe->qe_forw) {
		kt = (kthread_t*) qe->qe_parent;

		ASSERT(KT_ISGANG(kt));

		if (KT_TO_UT(kt)->ut_gstate == GANG_NONE || 
			KT_TO_UT(kt)->ut_gstate == GANG_STARTED ||
			KT_TO_UT(kt)->ut_gstate == GANG_BLOCKED) {
			continue;
		}

		kt_nested_lock(kt);

		/* Thread could be in GANG_EARN state, but just kicked
		 * onto a processor due to signal delivery.
		 */
		if (kt->k_onrq != CPU_NONE) {
			#pragma mips_frequency_hint NEVER
			ASSERT(KT_TO_UT(kt)->ut_gstate == GANG_EARN);
			kt_nested_unlock(kt);
			continue;
		}

		ASSERT(kt == curthreadp || 
			KT_TO_UT(kt)->ut_gstate == GANG_EARN);
		ASSERT(KT_TO_UT(kt)->ut_gbinding == CPU_NONE);

		if (kt->k_mustrun != CPU_NONE) {
			#pragma mips_frequency_hint NEVER
			if (!CPUMASK_TSTB(gang_bindings, kt->k_mustrun)
				|| CPUMASK_TSTB(gdb->g_bindings,
							kt->k_mustrun)) {
				set_bindings(kt, gdb, kt->k_mustrun);
				kt_nested_unlock(kt);
				continue;
			} else {
				clear_bindings(gdb);
				kt_nested_unlock(kt);
				nested_spinunlock(&gang_bindingslock);
				return 0;
			}
		}

		ASSERT(kt->k_mustrun == CPU_NONE);
		if (kt->k_binding != CPU_NONE &&
			available(kt->k_binding, kt->k_cpuset)) {
			set_bindings(kt, gdb, kt->k_binding);
			kt_nested_unlock(kt);
			continue;
		}	

#ifdef NUMA_SCHED
		pda = (pda_t *) memaff_getbestnode(kt, check_bindings,
							 (void *) kt, 0);
		if (pda) {
			set_bindings(kt, gdb, pda->p_cpuid);
			kt_nested_unlock(kt);
			continue;
		}
#endif
		if (!find_a_cpu(kt, gdb, iszero)) {
			#pragma mips_frequency_hint NEVER
			clear_bindings(gdb);
			kt_nested_unlock(kt);
			nested_spinunlock(&gang_bindingslock);
			return 0;
		}
		kt_nested_unlock(kt);
	}
	nested_spinunlock(&gang_bindingslock);

	GANG_GDB_SETSTATE(gdb, GDB_UNBLOCKED);
	sv_foreach_wake(&gdb->g_sv, gang_wakeup, gdb);

	return 1;		
}

void
gang_sched_tick(void)
{
	int s = mutex_spinlock(&gangqlock);
	gdb_t *gdb;

	GANGQ_DEBUG_CHECK();

	if (q_empty(&gangq)) {
		GANGQ_DEBUG_CHECK();
		mutex_spinunlock(&gangqlock, s);
		return;
	}

	gdb = GANG_FIRST;
	ASSERT(!GANG_END(gdb));
	if (!nested_spintrylock(&gdb->g_lock) || !gang_kick(gdb)) {
		GANGQ_DEBUG_CHECK();
		mutex_spinunlock(&gangqlock, s);
		return;
	}

	gdb = GANG_NEXT(gdb);

	for (; !GANG_END(gdb); gdb = GANG_NEXT(gdb)) {
		if (!nested_spintrylock(&gdb->g_lock)) {
			GANGQ_DEBUG_CHECK();
			mutex_spinunlock(&gangqlock, s);
			return;
		}
		gang_kick(gdb);
	}

	GANGQ_DEBUG_CHECK();
	mutex_spinunlock(&gangqlock, s);	
}

static void
gang_signaled(gdb_t *gdb, uthread_t *ut)
{
	/* Note that when a gang thread is interrupted by signal
	 * delivery the GANG_LAST thread might be scheduling the gang.
	 * The GANG_LAST thread expects that there are no gang threads
	 * running in user mode (i.e. gdb->g_nready == 0).  When a
	 * thread is signaled, we want it to run for a short time, but
	 * then rejoin the gang.  GANG_BLOCKED is appropriate for
	 * threads which temporarily leave the gang, but return at
	 * their next timeslice.
	 */

	ASSERT(issplhi(getsr()));
	ASSERT(spinlock_islocked(&gdb->g_lock));
	ASSERT(ut == curuthread);

#ifdef NUMA_SCHED
	/*
	 * We might have been signaled before blocking in
	 * sv_wait_sig(), thus potentially having k_lastrun set to a
	 * different cpu by cpu_numa_affinity() and facing an
	 * assertion failure in utexitswtch().  We can't be preempted
	 * right now, so testing k_lastrun should be
	 * raceless.
	 */
	if (UT_TO_KT(ut)->k_lastrun != cpuid()) {
		kthread_t *kt = UT_TO_KT(ut);
		kt_nested_lock(kt);
		kt->k_lastrun = cpuid();
		kt_nested_unlock(kt);
	}
#endif /* NUMA_SCHED */

	gang_clear(gdb, ut, GANG_BLOCKED);
	ASSERT(gdb->g_nready < gdb->g_nthreads);
}

static int
gang_schedule(gdb_t *gdb, int retval, int ss)
{
	kthread_t	*kt = curthreadp;
	uthread_t	*ut = KT_TO_UT(kt);

	/* This function is run by the GANG_LAST thread in order to
	 * schedule the gang.  Only one thread per gang may be
	 * executing this code, except in the special case of a thread
	 * handling a signal (see below).
	 */

	ASSERT(issplhi(getsr()));
	ASSERT(spinlock_islocked(&gdb->g_lock));
	ASSERT(ut->ut_gbinding == CPU_NONE);
	ASSERT(ut->ut_gstate == GANG_LAST);
	ASSERT(!gdb->g_blocked);
	ASSERT(!gdb->g_last);
	ASSERT(gdb->g_nready > 0);
	ASSERT(gdb->g_nready <= gdb->g_nthreads);

	GANG_GDB_SETLAST(gdb, ut);

	do {
		int sig = 0;

		nested_spinlock(&gangqlock); 
		GANGQ_DEBUG_CHECK();

		if (!q_empty(&gangq) || ut->ut_gstate == GANG_RUNNING) {
			retval = 1;

			ASSERT(ut->ut_gbinding == CPU_NONE);
			ASSERT(!gdb->g_blocked);

			GANG_THR_SETSTATE(gdb, ut, GANG_EARN);
			GANG_NREADY_DEC(gdb, ut);

			ASSERT(gdb->g_nready == 0);

			if (!q_onq(&gdb->g_queue)) {
				GANG_ADDGANGQ(gdb);
			}

			GANG_GDB_SETBLOCKED(gdb, 1);

			/* Wait for wakegang(). */
			GDB_DEBUG_CHECK(gdb);
			GANGQ_DEBUG_CHECK();
			nested_spinunlock(&gangqlock);
			sig = sv_wait_sig(&gdb->g_sv2, PZERO, &gdb->g_lock, ss);
			ss = mutex_spinlock(&gdb->g_lock);

			/* Priority might have changed. */
			if (!UT_ISGANG(ut)) {
				#pragma mips_frequency_hint NEVER
				ASSERT(gdb->g_last != ut);
				goto out_no_rmq;
			}

			/*
			 * After waking from the sv, the thread will
			 * either be GANG_EARN (interrupted),
			 * GANG_LAST (by gang_kick()), or GANG_NONE
			 * (by gang_detach()).
			 */
			ASSERT(ut->ut_gstate == GANG_EARN ||
				ut->ut_gstate == GANG_NONE ||
				ut->ut_gstate == GANG_LAST);

			/* Interrupted. */
			if (sig) {
				#pragma mips_frequency_hint NEVER
				gang_signaled(gdb, ut);
				goto out_no_rmq;
			}

			/*
			 * If we've been taken off of the gang sv by
			 * wakegang(), then it must have cleared
			 * g_blocked.
			 */
			ASSERT(!gdb->g_blocked);
			ASSERT(gdb->g_last == ut);
		} else {
			GANGQ_DEBUG_CHECK();
			nested_spinunlock(&gangqlock);
		}

		ASSERT(ut->ut_gstate != GANG_BLOCKED);
		GANG_THR_SETSTATE(gdb, ut, GANG_RUNNING);
	} while (ut->ut_gbinding == CPU_NONE && !gang_find_bindings(gdb));

	ASSERT(ut->ut_gstate != GANG_LAST && ut->ut_gstate != GANG_EARN);

	GANG_GDB_SETLAST(gdb, 0);

	if (q_onq(&gdb->g_queue)) {
		nested_spinlock(&gangqlock);	
		GANG_RMGANGQ(gdb);
		nested_spinunlock(&gangqlock);	
	}

out_no_rmq:
	GDB_DEBUG_CHECK(gdb);
	nested_spinunlock(&gdb->g_lock);

	if (retval)
		splx(ss);
	else
		kt_nested_lock(kt);
	return retval;
}

int
gang_startrun(kthread_t *kt, int ss)
{
	shaddr_t 	*sa;
	gdb_t		*gdb;
	int		mode;
	uthread_t	*ut = KT_TO_UT(kt);
	proc_t		*p = UT_TO_PROC(ut);
	int 		retval = 0;

	sa = p->p_shaddr;
	ASSERT(sa);
	ASSERT(issplhi(getsr()));

	if (ut->ut_prda && ((mode = ut->ut_prda->t_sys.t_hint) != sa->s_sched))
		if (mode == SGS_FREE || mode == SGS_GANG || mode == SGS_SINGLE)
			sa->s_sched = mode;
	gdb = sa->s_gdb;

	if (is_bcritical(kt) || is_batch(kt) || ut->ut_gstate == GANG_UNDEF)
		return 0;

	/* A thread entering gang_startrun() should not be GANG_LAST
	 * or GANG_EARN, since such threads should not have left the
	 * gang_startrun() path without a state transition.
	 */

	mode = sa->s_sched;
	switch (mode) {
	case SGS_FREE:
		if (ut->ut_gstate == GANG_NONE)
			break;

		kt_nested_unlock(kt);
		nested_spinlock(&gdb->g_lock);

		ASSERT(ut->ut_gstate != GANG_EARN &&
			ut->ut_gstate != GANG_LAST);

		if (ut->ut_gstate == GANG_NONE || !UT_ISGANG(ut)) {
			goto out_swtch;
		}

		if (ut->ut_gstate == GANG_BLOCKED) {
			GANG_NREADY_INC(gdb, ut);
		}

		sv_foreach_wake(&gdb->g_sv, gang_endrun_wakeup, gdb);
		wakegang(gdb, GANG_NONE);
		GANG_THR_SETSTATE(gdb, ut, GANG_NONE);

		GDB_DEBUG_CHECK(gdb);
		nested_spinunlock(&gdb->g_lock);
		kt_nested_lock(kt);
		break;

	case SGS_GANG:
	case SGS_SINGLE:
		/*
		 * N.B. This code is optimized for the case where
		 * we're a thread running through the gang scheduler
		 * in SGS_GANG mode.  SGS_SINGLE mode isn't as
		 * performance critical.
		 */
		if (mode == SGS_SINGLE && sa->s_master) {
			#pragma mips_frequency_hint NEVER
			int sig;

			kt_nested_unlock(kt);
			nested_spinlock(&gdb->g_lock);

			ASSERT(ut->ut_gstate != GANG_EARN &&
				ut->ut_gstate != GANG_LAST);

			if (!UT_ISGANG(ut)) {
				#pragma mips_frequency_hint NEVER
				goto out_swtch;
			}

			if (ut->ut_gstate != GANG_BLOCKED)
				GANG_NREADY_DEC(gdb, ut);

			if (gdb->g_state == GDB_BLOCKED) {
				/* oops got to get everyone running
				   again! */
				sv_foreach_wake(&gdb->g_sv, gang_wakeup, gdb);
				wakegang(gdb, GANG_LAST);
			}	

			GANG_GDB_SETSTATE(gdb, GDB_SINGLE);

			if (p->p_pid == sa->s_master) {
				GANG_THR_SETSTATE(gdb, ut, GANG_RUNNING);
				GANG_NREADY_INC(gdb, ut);
				GDB_DEBUG_CHECK(gdb);
				nested_spinunlock(&gdb->g_lock);
				kt_nested_lock(kt);
				return 0;
			}

			GANG_THR_SETSTATE(gdb, ut, GANG_EARN);

			if (ut->ut_gbinding != CPU_NONE)
				unset_bindings(ut, gdb);

			retval = 1;
			sig = sv_wait_sig(&gdb->g_sv, PZERO, &gdb->g_lock, ss);
			ss = mutex_spinlock(&gdb->g_lock);

			/* Priority might have changed. */
			if (!UT_ISGANG(ut)) {
				#pragma mips_frequency_hint NEVER
				goto out_to_user;
			}

			/* Interrupted. */
			if (sig) {
				#pragma mips_frequency_hint NEVER
				ASSERT(ut->ut_gstate == GANG_EARN ||
					ut->ut_gstate == GANG_LAST);
				gang_signaled(gdb, ut);
				goto out_to_user;
			}

			/* We might have been woken up as GANG_LAST,
			 * however if s_schedmode changes to
			 * SGS_SINGLE we might leave gang_startrun()
			 * as GANG_LAST.  No problem setting this to a
			 * running state.
			 */
			if (ut->ut_gstate == GANG_LAST) {
				GANG_THR_SETSTATE(gdb, ut, GANG_RUNNING);
			}

			GDB_DEBUG_CHECK(gdb);
			nested_spinunlock(&gdb->g_lock);
			kt_nested_lock(kt);
		} 

		ASSERT(issplhi(getsr()));
		ASSERT(kt_islocked(kt));

		/* So anybody that gets to here is a gang now. */
		if (sa->s_sched == SGS_GANG) {
			kt_nested_unlock(kt);
			nested_spinlock(&gdb->g_lock);

			/* Priority might have changed. */
			if (!UT_ISGANG(ut)) {
				#pragma mips_frequency_hint NEVER
				goto out_swtch;
			}

			if (gdb->g_state == GDB_INACTIVE) {
				#pragma mips_frequency_hint NEVER
				GANG_GDB_SETSTATE(gdb, GDB_BLOCKED);
				gang_count_all(gdb);
			}

			if (sa->s_master == p->p_pid && 
				gdb->g_state == GDB_SINGLE) {
				sv_foreach_wake(&gdb->g_sv, gang_wakeup, gdb);
			}		

			if (ut->ut_gbinding != CPU_NONE) 
				unset_bindings(ut, gdb);

			/*
			 * At this point, everyone blocks except for
			 * the last thread through, which schedules
			 * the gang.
			 */
			if (ut->ut_gstate != GANG_BLOCKED)
				GANG_NREADY_DEC(gdb, ut);

			if (gdb->g_nready || q_onq(&gdb->g_queue)) {
				int sig;

				GANG_GDB_SETSTATE(gdb, GDB_BLOCKED);
				GANG_THR_SETSTATE(gdb, ut, GANG_EARN);
				ASSERT(ut->ut_gbinding == CPU_NONE);

				retval = 1;
				sig = sv_wait_sig(&gdb->g_sv, PZERO,
					&gdb->g_lock, ss);
				ss = mutex_spinlock(&gdb->g_lock);

				/* Priority might have changed. */
				if (!UT_ISGANG(ut)) {
					#pragma mips_frequency_hint NEVER
					goto out_to_user;
				}

				/* Interrupted. */
				if (sig) {
					#pragma mips_frequency_hint NEVER
					ASSERT(ut->ut_gstate == GANG_EARN ||
						ut->ut_gstate == GANG_LAST);
					gang_signaled(gdb, ut);
					goto out_to_user;
				}

				/* Thread now running with the gang.
				*/
				if (ut->ut_gstate != GANG_LAST) {
					ASSERT(ut->ut_gstate != GANG_EARN);
					GDB_DEBUG_CHECK(gdb);
					mutex_spinunlock(&gdb->g_lock, ss);
					return 1;
				}
			} else { 
				GANG_THR_SETSTATE(gdb, ut, GANG_LAST);
				GANG_NREADY_INC(gdb, ut);
			}

			/* The GANG_LAST thread takes this path to
			 * schedule the gang.
			 */

			return gang_schedule(gdb, retval, ss);
		}
	} /* switch */

	if (retval)
		kt_unlock(kt, ss);
	return retval;

out_to_user:
	/*
	 * Common code to return to user-mode.
	 */
	ASSERT(retval);
	ASSERT(issplhi(getsr()));
	ASSERT(spinlock_islocked(&gdb->g_lock));
	GDB_DEBUG_CHECK(gdb);
	mutex_spinunlock(&gdb->g_lock, ss);
	return 1;

out_swtch:
	/*
	 * Common code to return through the regular user_resched() path.
	 */
	ASSERT(!retval);
	ASSERT(issplhi(getsr()));
	ASSERT(spinlock_islocked(&gdb->g_lock));
	nested_spinunlock(&gdb->g_lock);
	kt_nested_lock(kt);
	return 0;
}

void
gang_endrun(uthread_t *ut, gdb_t *gdb)
{
	int		ss;

	if (is_bcritical(UT_TO_KT(ut)) || is_batch(UT_TO_KT(ut)))
		return;

	if (ut->ut_gbinding != CPU_NONE) {
		ss = mutex_spinlock(&gdb->g_lock);
		unset_bindings(ut, gdb);
		GDB_DEBUG_CHECK(gdb);
		mutex_spinunlock(&gdb->g_lock, ss);
	}
}

void
gang_block(uthread_t *ut)
{
	gdb_t		*gdb;

	ASSERT(!is_batch(curthreadp) && !is_bcritical(curthreadp));
	ASSERT(IS_SPROC(ut->ut_pproxy));
	ASSERT(issplhi(getsr()));
	ASSERT(kt_islocked(UT_TO_KT(ut)));
	ASSERT(ut->ut_job);

	gdb = UT_TO_PROC(ut)->p_shaddr->s_gdb;

	if (ut->ut_gstate == GANG_RUNNING || ut->ut_gstate == GANG_STARTED) {
		nested_spinlock(&gdb->g_lock);

		if (!UT_ISGANG(ut) || !(ut->ut_gstate == GANG_RUNNING ||
			ut->ut_gstate == GANG_STARTED)) {
			GDB_DEBUG_CHECK(gdb);
			nested_spinunlock(&gdb->g_lock);
			return;
		}

		gang_clear(gdb, ut, GANG_BLOCKED);
		ASSERT(gdb->g_nready < gdb->g_nthreads);

		GDB_DEBUG_CHECK(gdb);
		nested_spinunlock(&gdb->g_lock);
	}
}

void
gang_exit(shaddr_t *sa)
{
	uthread_t	*ut = curuthread;
	int		s;
	gdb_t		*gdb = sa->s_gdb;

	if (is_bcritical(curthreadp) || is_batch(curthreadp))
		return;

	s = mutex_spinlock(&gdb->g_lock);

	ASSERT(gdb->g_nready >= 0);
	ASSERT(gdb->g_nready <= gdb->g_nthreads);

	if (sa->s_master == current_pid() && 
		(gdb->g_state == GDB_SINGLE || sa->s_sched == SGS_SINGLE))
		sa->s_sched = SGS_FREE;

	gang_detach(gdb, ut);

	GDB_DEBUG_CHECK(gdb);
	mutex_spinunlock(&gdb->g_lock, s);
}

static const char *
schedmode_to_string(int state)
{
	switch (state) {
	case SGS_FREE:		return("FREE");
	case SGS_SINGLE:	return("SINGLE");
	case SGS_GANG:		return("GANG");
	default:		return("***UNKNOWN SCHEDMODE***");
	}
}

static const char *
gdbstate_to_string(int state)
{
	switch (state) {
	case GDB_INACTIVE:	return("INACTIVE");
	case GDB_UNBLOCKED:	return("UNBLOCKED");
	case GDB_BLOCKED:	return("BLOCKED");
	case GDB_SINGLE:	return("SINGLE");
	default:		return("**UNKNOWN GDB STATE**");
	}
}

static const char *
gstate_to_string(char gs)
{ 
	switch (gs) {
	case GANG_NONE: 	return("NONE");
	case GANG_RUNNING: 	return("RUNNING");
	case GANG_BLOCKED:	return("BLOCKED");
	case GANG_EARN: 	return("EARN");
	case GANG_LAST:		return("LAST");
	case GANG_STARTED:	return("STARTED");
	case GANG_UNDEF:	return("UNDEF");
	default:		return("**UNKNOWN GANG STATE**");
	}
}

static void
dogangthread(gdb_t *gdb, uthread_t *ut)
{
	kthread_t	*kt = UT_TO_KT(ut);

	qprintf("    0x%x [%d] %s", KT_TO_UT(kt), ut->ut_proc->p_pid,
		gstate_to_string(ut->ut_gstate));

	if (gdb->g_last == ut) {
		qprintf(" (GANG_LAST)");
	}

	if (kt->k_binding != CPU_NONE) {
		qprintf(" binding %d", kt->k_binding);
	}
	if (ut->ut_gbinding != CPU_NONE) {
		qprintf(" gbinding %d", ut->ut_gbinding);
	}

	if (kt->k_basepri != PTIME_SHARE &&
		kt->k_basepri != PTIME_SHARE_OVER) {
		qprintf(" basepri %d", kt->k_basepri);
	}

	if (kt->k_onrq == maxcpus) {
		qprintf(" on rt Q");
	} else if (kt->k_onrq != CPU_NONE) {
		qprintf(" on Q %d", kt->k_onrq);
	}

	if (kt->k_sonproc != CPU_NONE) {
		qprintf(" on CPU %d", kt->k_sonproc);
	}

	if (kt->k_wchan == (caddr_t)&gdb->g_sv) {
		qprintf(" on gang sv");
	}
	else if (kt->k_wchan == (caddr_t)&gdb->g_sv2) {
		qprintf(" on gang sv2");
	}
	else if (kt->k_wchan) {
		qprintf(" wchan 0x%x", kt->k_wchan);
	}

	qprintf("\n");
}

static void
dogang(gdb_t *gdb)
{
	proc_t			*p;
	int			i;
	struct q_element_s	*list;
	struct q_element_s	*qe;

	qprintf("gdb 0x%x %s on gangq\n", gdb,
		(q_onq(&gdb->g_queue) ? "is" : "not"));

	qprintf("  gang bound cpus: ");
	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(gdb->g_bindings, i))
			qprintf(" %d ", i);
		else
			qprintf(" X ");
	}
	qprintf("\n");

	qprintf("  nthreads %d nready %d sv 0x%x sv2 0x%x\n",
		gdb->g_nthreads, gdb->g_nready, &gdb->g_sv, &gdb->g_sv2);
	qprintf("  master %d schedmode %s state %s last 0x%x blocked %d\n",
		gdb->g_sa->s_master, schedmode_to_string(gdb->g_sa->s_sched),
		gdbstate_to_string(gdb->g_state), gdb->g_last, gdb->g_blocked);
	qprintf("  shaddr 0x%x shaddr->gdb 0x%x\n", gdb->g_sa,
		gdb->g_sa->s_gdb);
	qprintf("  lock 0x%x %s\n", &gdb->g_lock,
		spinlock_islocked(&gdb->g_lock) ? "LOCKED" : "unlocked");

	qprintf("  share group uthreads:\n");
	for (p = gdb->g_sa->s_plink; p; p = p->p_slink) {
		uthread_t *ut = prxy_to_thread(&p->p_proxy);
		dogangthread(gdb, ut);
	}

	qprintf("  gang uthreads:\n");
	list = &gdb->g_thread;
	for (qe = list->qe_forw; qe != list; qe = qe->qe_forw) {
		kthread_t *kt = (kthread_t*) qe->qe_parent;
		dogangthread(gdb, KT_TO_UT(kt));
	}
}

static int
trygangthread(kthread_t *kt, int verbose)
{
	if (!kt || kt->k_type != KT_UTHREAD) {
		if (verbose)
			qprintf("0x%x: not a uthread\n", kt);
	}
	else if (!UT_TO_PROC(KT_TO_UT(kt))->p_shaddr) {
		if (verbose)
			qprintf("uthread 0x%x: not a sproc\n", kt);
	}
	else if (!UT_TO_PROC(KT_TO_UT(kt))->p_shaddr->s_gdb) {
		if (verbose)
			qprintf("uthread 0x%x: not a gang thread\n", kt);
	}
	else {
		qprintf("Gang state for uthread 0x%x:\n", KT_TO_UT(kt));
		dogang(UT_TO_PROC(KT_TO_UT(kt))->p_shaddr->s_gdb);
		return 1;
	}

	return 0;
}

void
idbg_gang(__psint_t x)
{
	kthread_t *kt;
	gdb_t *gdb;
	int i;

	qprintf("global bound cpus: ");
	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(gang_bindings, i))
			qprintf(" %d ", i);
		else
			qprintf(" X ");
	}
	qprintf("\n");

	if (x != -1L) {
		kt = (kthread_t*) x;
		trygangthread(kt, 1);
	}
	else if (q_onq(&gangq)) {
		qprintf("Dumping gangq:\n");
		for (gdb = GANG_FIRST; !GANG_END(gdb); gdb = GANG_NEXT(gdb)) 
			dogang(gdb);
	} 
	else if (!trygangthread(curthreadp, 0)) {
		qprintf("No current gang.\n");
	}
}

static void
idbg_gdb(__psint_t x)
{
	if (x != -1L) {
		dogang((gdb_t*)x);
	}
	else {
		idbg_gang(-1);
	}
}

#ifdef GANG_TRACING

static void
do_gt(gang_trace_t *gt)
{
	qprintf("  cur 0x%x targ 0x%x pc 0x%x\n",
		gt->gt_running, gt->gt_target, gt->gt_pc);

	qprintf("    %s", gt->gt_action);

	if (gt->gt_action == _GT_NREADY_INC ||
		gt->gt_action == _GT_NREADY_DEC) {
		qprintf(", now %ld", gt->gt_data1);
	}
	else if (gt->gt_action == _GT_SET_LAST) {
		qprintf("0x%x", gt->gt_data1);
	}
	else if (gt->gt_action == _GT_SET_BLOCKED) {
		qprintf("%d", (int)gt->gt_data1);
	}
	else if (gt->gt_action == _GT_THR_SETSTATE) {
		qprintf(": %s -> %s", gstate_to_string((char)gt->gt_data1),
			gstate_to_string((char)gt->gt_data2));
	}
	else if (gt->gt_action == _GT_GDB_SETSTATE) {
		qprintf(": %s -> %s", gdbstate_to_string((int)gt->gt_data1),
			gdbstate_to_string((int)gt->gt_data2));
	}
	else if (gt->gt_action == _GT_GANGQ_ADDQ ||
		gt->gt_action == _GT_GANGQ_RMQ) {
		/* no data */
	}
	else {
		qprintf(" 0x%lx 0x%lx", gt->gt_data1, gt->gt_data2);
	}

	qprintf("\n");
}

static void
do_gangtrace(gdb_t *gdb)
{
	int	ii;

	qprintf("Gang trace for gdb 0x%x (proceeding backwards in time)\n",
		gdb);
	for (ii = gdb->g_tbuf_cur - 1; ii != gdb->g_tbuf_cur; ii--) {
		gang_trace_t	*gt;
		if (ii < 0)
			ii = GANG_NTRACE - 1;
		gt = &gdb->g_tbuf[ii];
		if (!gt->gt_action)
			break;
		do_gt(gt);
	}
}

static void
idbg_gangtrace(__psint_t x)
{
	gdb_t		*gdb;

	if (x == (__psint_t)-1) {
		if (q_onq(&gangq)) {
			for (gdb = GANG_FIRST; !GANG_END(gdb);
				gdb = GANG_NEXT(gdb)) {
				do_gangtrace(gdb);
			}
		}
		else if ((gdb = UT_TO_PROC(curuthread)->p_shaddr->s_gdb)) {
			do_gangtrace(gdb);
		}
		else {
			qprintf("usage: gangtrace <gdb>\n");
		}
	}
	else {
		do_gangtrace((gdb_t*)x);
	}
}

#endif /* GANG_TRACING */
