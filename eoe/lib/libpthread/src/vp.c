/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * This module implements the scheduling, that is the mechanisms by which
 * the VP or kernel entity chooses a pthread context to run or suspend.
 *
 * Key points:
	* There are 3 queues: readyq (pt), idleq (vp) and execq (vp).
	* One big lock: schedq lock.
	* VP_RESCHED is a per-VP (PRDA) counter that is decremented by the
	  kernel clock interrupt, when it reaches zero the vp is sent the
	  SIGPTRESCHED signal - this counter is used for both RR quantum
	  and a less frequent heartbeat (FIFO).
	* When priority changes require preemption SIGPTRESCHED is sent
	  explicitly.
	* VP balancing is the goal of attaining a target level of running VPs.
	  Issues which affect this are changing workload, blocking system
	  calls and binding pthreads to VPs.
	* An attempt is made to retain a pthread/VP mapping if no better
	  scheduling choice is available when the pthread blocks.
 */

#include "common.h"
#include "asm.h"
#include "intr.h"
#include "pt.h"
#include "ptdbg.h"
#include "pthreadrt.h"
#include "mtx.h"
#include "mtxattr.h"
#include "q.h"
#include "sig.h"
#include "stk.h"
#include "sys.h"
#include "vp.h"

#include <errno.h>
#include <mutex.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/sysmp.h>
#include <sys/types.h>
#include <syslog.h>

#include <unistd.h>


/* Locking mechanisms:
 *
 *	Scheduler mask - Mask to block all signals (especially SIGPTRESCHED)
 *			 so we don't get interrupted while we're holding locks.
 *	Scheduling q lock - Control vp and pt scheduling queues.
 */
k_sigset_t	sched_mask;

static slock_t	schedq_lock;


/* Scheduling queues
 *
 * Idle queue: queue of vps who are ready to run but lack a pthread.
 *	Vps with associated pts are queued after those without.
 * Exec queue: queue of vps who are actively running pthreads.
 *	Vps are queued in order of pt priority.
 */
static Q_DECLARE(vp_idleq);
static Q_DECLARE(vp_execq);
#define vp_execq_last()			((vp_t *)Q_TAIL(&vp_execq))

#define vp_q_insert_head(q, vp)		Q_INSERT_HEAD(q, vp, vp_t, vp_priority)
#define vp_q_insert_tail(q, vp)		Q_INSERT_TAIL(q, vp, vp_t, vp_priority)
#define vp_q_reorder_head(q, vp)	Q_UNLINK(vp); vp_q_insert_head(q, vp)
#define vp_q_reorder_tail(q, vp)	Q_UNLINK(vp); vp_q_insert_tail(q, vp)


/* A bound vp does not reside on the exec or idle queues.
 * Instead we queue it to itself so that unlink works.
 * WARNING: some code "knows" this is a fake queue and skips the dequeue.
 */
#define vp_q_is_bound(vp)		Q_EMPTY((q_t *)(vp))
#define vp_q_insert_bound(vp)		Q_INIT((q_t *)(vp))


/* Preemption check.
 * Interrupt a vp if the pt should preempt it.
 */
#define PREEMPT_CHECK(cond)					\
MACRO_BEGIN							\
	ASSERT(lock_held(&schedq_lock));			\
	if ((cond)) {						\
		register vp_t *_vp = vp_execq_last();		\
		if (_vp->vp_priority < pt_readyq_first()->pt_priority	\
		    && !_vp->vp_stopping) {			\
			TRACE(T_VP, ("PREEMPT_CHECK(%#x, %#x)", \
				      _vp, pt_readyq_first())); \
			_vp->vp_stopping = TRUE;		\
			prctl(PR_THREAD_CTL, PR_THREAD_KILL,	\
				_vp->vp_pid, SIGPTRESCHED);	\
		}						\
	}							\
MACRO_END

#define PREEMPT_VP_COND(cond)	PREEMPT_CHECK((cond) && !Q_EMPTY(&pt_readyq))
#define PREEMPT_PT_COND(cond)	PREEMPT_CHECK((cond) && !Q_EMPTY(&vp_execq))


/* Ready queue: queue of pthreads who are ready to run but lack a vp.
 */
Q_DECLARE(pt_readyq);
static int	pt_readyq_len;
#define pt_readyq_first()		(Q_HEAD(&pt_readyq, pt_t*))

#define pt_q_reorder_tail(q, pt)	Q_UNLINK(pt); pt_q_insert_tail(q, pt)


/* Scheduling decisions frequency (quantum).
 */
#define RR_SCHEDPERSEC		20
#define FIFO_SCHEDPERSEC	5
unsigned short			sched_rr_quantum;	/* num ticks for rr */
unsigned short			sched_fifo_quantum;	/* num ticks for fifo */


/*
 * Vp count and dynamic vp balancing.
 */
int		sched_proc_count;	/* Number of processors */
static int	sched_balance;		/* Adjust vp target count */
static int	vp_target_count = 1;	/* Target number of vp's */

#define	SAMPLES_PER_DECISION	100
#define	VP_DECREMENT_THRESHOLD	-50
#define	VP_INCREMENT_THRESHOLD	50


static void	vp_setschedparam(pt_t *, int);
static vp_t	*vp_alloc(pt_t *);
static void	vp_start(void *, size_t);
static vp_t	*vp_dequeue_idle(void);
static void	vp_enqueue_idle(void);
static int      vp_wakeup_idle(pt_t *, vp_wakeup_t);
static void	vp_rebind(vp_t *, pt_t *);
static void	vp_idle(register pt_t *);

static void		sched_handler(int, int, struct sigcontext *);
static sched_resume_t	sched_select(sched_thread_types_t);
static int		sched_subtract_vp(sched_thread_types_t);


/*
 * sched_bootstrap(void)
 */
void
sched_bootstrap(void)
{
	char		*pt_itc = getenv("PT_ITC");
	sigset_t	sched_set;
	struct sigaction act;
	unsigned short	ticks_per_sec = (unsigned short)sysconf(_SC_CLK_TCK);
	extern int	_sigaction(int, struct sigaction *, struct sigaction *);

	sigfillset(&sched_set);
	sigdelset(&sched_set, SIGKILL);
	sigdelset(&sched_set, SIGSTOP);

#ifdef	DEBUG
	/* always core dump */
#else
	if (getenv("PT_CORE"))
#endif
	{
		const sigset_t core_sig = {
			sigmask(SIGILL)
			| sigmask(SIGSEGV)
			| sigmask(SIGBUS)
			| sigmask(SIGQUIT)
			| sigmask(SIGTRAP)
			| sigmask(SIGABRT)
			| sigmask(SIGEMT)
			| sigmask(SIGFPE)
			| sigmask(SIGSYS)
			| sigmask(SIGXCPU)
			| sigmask(SIGXFSZ) };
		sigdiffset(&sched_set, &core_sig);
	}
	sig_set_to_kset(&sched_set, &sched_mask);	/* sig_bootstrap() */

	/* Handler for scheduling interrupts.
	 *
	 * Resched signal delivery runs off the clock interrupt; we
	 * need to avoid blocking exception signals which may be in
	 * progress at that time.
	 */
	act.sa_handler = sched_handler;
	sigemptyset(&act.sa_mask);		/* do not block signals */
	act.sa_flags = SA_RESTART;		/* restart system calls */
	_sigaction(SIGPTRESCHED, &act, NULL);	/* call libc version */

	sched_proc_count = (int)sysconf(_SC_NPROC_ONLN);

	/*
	 * FIFO threads get a quantum on an MP so they can help with vp
	 * balancing.  Since we don't do vp balancing on an SP, no quantum
	 * is necessary.
	 */
	sched_fifo_quantum = (sched_proc_count > 1)
		? ticks_per_sec / FIFO_SCHEDPERSEC
		: 0;
	sched_rr_quantum = ticks_per_sec / RR_SCHEDPERSEC;

	if (pt_itc) {
		if ((vp_target_count = (int)strtol(pt_itc, 0, 0)) <= 0) {
			vp_target_count = sched_proc_count;
		}
		sched_balance = FALSE;
	} else {
		sched_balance = (sched_proc_count != 1);
	}
	vp_active = 1;	/* located in libc, default is 0 */
}


/*
 * sched_handler(sig, code, sc)
 */
/* ARGSUSED */
static void
sched_handler(int sig, int code, struct sigcontext *sc)
{
	register pt_t	*pt_self = PT;

	TRACE(T_VP, ("sched_handler()"));
	ASSERT(sig == SIGPTRESCHED);
	ASSERT(PT);
	ASSERT(!sched_entered());

	/* Avoid preemptions during critical system call regions.
	 */
	if (pt_self->pt_blocked) {
		VP_RESCHED = 1;
		return;
	}

	sched_enter();	/* now block signals */

	(void)sched_block(SCHED_PREEMPTING);

	ASSERT(!sched_entered());
}


/*
 * sched_select(sched_thread_type)
 *
 * Handle scheduling decisions based on priority and policy.
 */
static sched_resume_t
sched_select(sched_thread_types_t sched_thread_type)
{
	register pt_t	*pt_cur = PT;
	register vp_t	*vp_self = VP;
	register pt_t	*pt;
	int		highest_ready_pri;
	int		vyield = vp_self->vp_vyield;
	int		vp_bound = FALSE;	/* assume unbound vp */
	int		r;		/* cache reads to VP_RESCHED */
	sched_resume_t	ret;

	TRACE(T_VP, ("sched_select(%d)", sched_thread_type));
	ASSERT(sched_entered());
	ASSERT(!pt_cur || !pt_is_bound(pt_cur));

	lock_enter(&schedq_lock);

	if (vp_self->vp_resume) {

		vp_self->vp_resume = FALSE;
		lock_leave(&schedq_lock);

		/* Our pthread has been made runnable again so
		 * we resume it.
		 */
		ASSERT(sched_thread_type == SCHED_READY);
		ASSERT(pt_cur->pt_state == PT_DISPATCH);
		ASSERT(pt_cur->pt_vp == vp_self);

		VP_RESCHED = SCHED_POLICY_QUANTUM(pt_cur->pt_policy);
		return (SCHED_RESUME_SAME);
	}

	vp_self->vp_stopping = FALSE;
	vp_self->vp_vyield = FALSE;

	/* Check to see if there is runnable pthread available.
	 * If there is no-one to run then either continue with the current
	 * pt or block if we do not have one.
	 */
	if (Q_EMPTY(&pt_readyq)) {
		switch (sched_thread_type) {

		case SCHED_NEW:
		case SCHED_READY:
			/*
			 * Can't run our current pthread -- wait for one to
			 * become available.
			 */
			if (!vp_q_is_bound(vp_self)) {
				/*
				 * May go to 0, but can't do anything about
				 * it since there are (currently) no other
				 * threads to run.
				 */
				(void)VP_SUB_ACTIVE();
			}

			Q_UNLINK(vp_self);	/* off the old queue */
			vp_enqueue_idle();	/* onto the idle queue */
			lock_leave(&schedq_lock);

			ret = SCHED_RESUME_LATER;
			break;

		case SCHED_PREEMPTING:
			/*
			 * Continue running the one we have.
			 */
			lock_leave(&schedq_lock);

			/* Boost the next quantum and reduce interruptions
			 */
			if (!VP_RESCHED) {
				VP_RESCHED = sched_rr_quantum * RR_SCHEDPERSEC;
			}

			ret = SCHED_RESUME_SAME;
			break;
		}

		return (ret);
	}

	/* There is someone who could run.
	 * If the blocking pthread is not runnable we take the first
	 * ready pt and run it.
	 * If this is a preemption then we make a scheduling decision
	 * and may resume the current pthread.
	 *
	 * If we change pthreads we set the quantum for reloading
	 * when it is resumed.
	 */
	switch (sched_thread_type) {

	case SCHED_NEW:
		/*
		 * Current pthread exited - find a new pthread.
		 */
		vp_bound = vp_q_is_bound(vp_self);
		pt = pt_dequeue(&pt_readyq);
		pt_readyq_len--;
		ret = SCHED_RESUME_NEW;
		break;

	case SCHED_READY:
		/*
		 * Current pthread waiting - find the first ready pthread.
		 */
		pt_cur->pt_resched = SCHED_POLICY_QUANTUM(pt_cur->pt_policy);

		pt = pt_dequeue(&pt_readyq);
		pt_readyq_len--;
		ret = SCHED_RESUME_SWAP;
		break;

	case SCHED_PREEMPTING:
		/*
		 * Current pthread making preemption check - find a higher
		 * priority pthread to run.
		 */
		highest_ready_pri = pt_readyq_first()->pt_priority;
		r = VP_RESCHED;

		switch (pt_cur->pt_policy) {

		case SCHED_TS:

			/* FALLTHROUGH */

		case SCHED_RR:
			/*
			 * Round Robin
			 *
			 * Schedule a ready pt if it is higher priority than
			 * the current one or if it is equal priority and our
			 * quantum has expired.
			 *
			 * If no suitable pt is found the current pt is resumed
			 * and the quantum is reloaded if it has expired.
			 */
			if (pt_cur->pt_priority > highest_ready_pri) {

				/* Next ready is lower priority.
				 * Reset quantum and continue current pt.
				 */
				lock_leave(&schedq_lock);

				if (!r) {
					VP_RESCHED = sched_rr_quantum;
				}

				return (SCHED_RESUME_SAME);
			}

			if (r && pt_cur->pt_priority == highest_ready_pri) {

				/* Next ready is same priority and we have
				 * some time left so continue current pt.
				 */
				lock_leave(&schedq_lock);

				return (SCHED_RESUME_SAME);
			}

			/* Next ready pt is selected.
			 * Dequeue next ready pt for dispatch.
			 * Save unexpired quantum or a new one.
			 * Enqueue current pt at head or tail depending
			 * on whether the quantum has expired.
			 */
			pt = pt_dequeue(&pt_readyq);
			ASSERT(pt->pt_state == PT_READY);

			pt_cur->pt_state = PT_READY;
			if (r) {
				pt_cur->pt_resched = r;
				pt_q_insert_head(&pt_readyq, pt_cur);
			} else {
				pt_cur->pt_resched = sched_rr_quantum;
				pt_q_insert_tail(&pt_readyq, pt_cur);
			}
			break;

		case SCHED_FIFO:
			/*
			 * First In First Out
			 *
			 * Schedule a ready pt if it is higher priority than
			 * the current one or if it is equal priority and our
			 * this is a voluntary yield.
			 *
			 * If no suitable pt is found the current pt is resumed.
			 */
			if (vyield) {
				highest_ready_pri++;
			}
			if (pt_cur->pt_priority >= highest_ready_pri) {

				/* Next ready is lower priority.
				 */
				lock_leave(&schedq_lock);

				if (!r) {
					VP_RESCHED = sched_fifo_quantum;
				}

				return (SCHED_RESUME_SAME);
			}

			/*
			 * Dequeue next ready pt for dispatch.
			 * Save unexpired quantum or a new one.
			 * Enqueue current pt at head or tail depending
			 * on whether the yield was forced or voluntary.
			 */
			pt = pt_dequeue(&pt_readyq);
			ASSERT(pt->pt_state == PT_READY);

			pt_cur->pt_state = PT_READY;
			pt_cur->pt_resched = (r) ? r : sched_fifo_quantum;
			if (vyield) {
				pt_q_insert_tail(&pt_readyq, pt_cur);
			} else {
				pt_q_insert_head(&pt_readyq, pt_cur);
			}
			break;
		}
		ret = SCHED_RESUME_SWAP;
		break;
	}

	ASSERT(ret == SCHED_RESUME_SWAP || ret == SCHED_RESUME_NEW);
	ASSERT(pt != pt_cur);

	/* We have a new pt to run.
	 * Update vp state in preparation for resuming new pt.
	 */
	pt->pt_state = PT_DISPATCH;

	/* Bind to the new pthread.
	 */
	vp_rebind(vp_self, pt);

	/*
	 * Ensure the vp is correctly queued and accounted.
	 *
	 * Make a preemption check for the current in case the most
	 * eligible pthread on the readyq should also run.
	 */
	if (vp_bound) {

		/* Vp currently bound.
		 * Join the execq if the new pt is not bound.
		 */
		vp_self->vp_priority = pt->pt_priority;
		if (!pt_is_bound(pt)) {
			vp_q_insert_head(&vp_execq, vp_self);
			(void)VP_ADD_ACTIVE();

			PREEMPT_VP_COND(vp_self != vp_execq_last());

		} else {

			PREEMPT_VP_COND(!Q_EMPTY(&vp_execq));
		}

	} else if (pt_is_bound(pt)) {

		/* Vp not currently bound; new pt is bound.
		 * Remove from the execq and make bound.
		 */
		vp_self->vp_priority = pt->pt_priority;
		Q_UNLINK(vp_self);
		vp_q_insert_bound(vp_self);

		PREEMPT_VP_COND(!Q_EMPTY(&vp_execq));

		/*
		 * This vp is now bound so we decrement the vp_active
		 * count.  This may push us below the number of vp's we
		 * want, so we try to add a new one to make up for it.
		 *
		 * XXXjt - is 0 right number?
		 */
		if (VP_SUB_ACTIVE() <= 0) {
			if (!Q_EMPTY(&pt_readyq)) {
				lock_leave(&schedq_lock);
				sched_add_vp();
				lock_enter(&schedq_lock);
			}
		}

	} else {

		/* Vp not bound; pt not bound.
		 */
		if (vp_self->vp_priority != pt->pt_priority) {

			/* Priority has changed.
			 * Reorder the vp on the execq.
			 */
			vp_self->vp_priority = pt->pt_priority;
			vp_q_reorder_head(&vp_execq, vp_self);
		}

		PREEMPT_VP_COND(vp_self != vp_execq_last());
	}

	lock_leave(&schedq_lock);

	VP_RESCHED = 0;

	SET_PT(pt);

	return (ret);
}


/* sched_swtch(pt_old)
 *
 * Run on vp stk waiting to switch to new pthread.
 */
static void
sched_switch(pt_t *pt_old)
{
	ASSERT(PT);
	if (pt_old) {
		pt_old->pt_errno = oserror();
		pt_old->pt_occupied = FALSE;	/* release old pt stack */
	}
	VP_YIELD(PT->pt_occupied);		/* wait for new pt stack */
	VP_RESCHED = -1;
	longjmp(PT->pt_context, (__psunsigned_t)pt_old);
}

/*
 * sched_block(sched_thread_type)
 *
 * Call sched_block() whenever the current pthread is about to block for an
 * indeterminate period of time and we would like to give up the vp.
 */
int
sched_block(sched_thread_types_t sched_thread_type)
{
	register pt_t	*pt_cur = PT;
	pt_t		*occupied_pt;	/* pass pt_cur through longjmp() */
	int		error;
	static int	num_samples = 0;
	static int	vp_gauge = 0;

	TRACE(T_VP, ("sched_block(%d)", sched_thread_type));
	ASSERT(VP->vp_occlude == 1);

	if (vp_q_is_bound(VP)) {
		/*
		 * If VP is bound and it has an active pt, we just wait for
		 * it to wake up.
		 */
		if (pt_cur) {
			TRACE(T_VP, ("sched_block() bound block"));
			ASSERT(pt_is_bound(pt_cur));

			/*
			 * Don't block if we were preempted from an interrupt.
			 */
			if (pt_cur->pt_state != PT_EXEC) {
				prctl(PR_THREAD_CTL, PR_THREAD_BLOCK);
			}

			return (sched_resume(SCHED_RESUME_SAME));
		}

	} else if (add_if_greater(&vp_active, vp_target_count, -1u)) {
		/*
		 * If the VP is not bound and we are over our target number
		 * of vp's, we subtract out this one.
		 */
		return (sched_subtract_vp(sched_thread_type));

	} else if (sched_balance) {
		/*
		 * If we aren't bound and we are on an MP, we adjust the
		 * target vp population based on the ready queue behaviour.
		 *
		 * We accumulate the discrepancy between the current target
		 * and the ideal.  If this value exceeds a threshold
		 * within a number of samples we adjust the target count.
		 *
		 * Actual = vp_target
		 * Ideal  = vp_active + pt_readyq_len
		 *
		 * If the readyq is emptied the accumulator is reset.
		 */

		error = vp_active + pt_readyq_len - vp_target_count;

		switch (sched_thread_type) {
		case SCHED_READY :
			vp_gauge = (pt_readyq_len == 1 && vp_gauge > 0)
				? error
				: vp_gauge + error;
			break;
		case SCHED_NEW :
		case SCHED_PREEMPTING :
			vp_gauge = (pt_readyq_len == 0 && vp_gauge > 0)
				? error
				: vp_gauge + error;
			break;
		}
		if (error < 0) {
			num_samples += -error;
		} else {
			num_samples += error;
		}

		if (num_samples > SAMPLES_PER_DECISION) {
			num_samples = 0;
			if (vp_gauge < VP_DECREMENT_THRESHOLD) {
				(void)add_if_greater(&vp_target_count, 1, -1u);

				if (add_if_greater(&vp_active, 1, -1u)) {
					vp_gauge = 0;
					return (sched_subtract_vp(
							sched_thread_type));
				}
				vp_gauge -= VP_DECREMENT_THRESHOLD;
				num_samples = -vp_gauge;

			} else if (vp_gauge > VP_INCREMENT_THRESHOLD) {
				if (add_if_less(&vp_target_count,
					sched_proc_count, 1u)
					&& !Q_EMPTY(&pt_readyq)) {
							sched_add_vp();
				}
				vp_gauge -= VP_INCREMENT_THRESHOLD;
				num_samples = vp_gauge;

			} else {
				num_samples = 0;
				vp_gauge = 0;
			}
		}
	}

#ifdef STATS
	switch (sched_thread_type) {
	case SCHED_NEW		: STAT_INCR(STAT_BLOCK_NEW); break;
	case SCHED_READY	: STAT_INCR(STAT_BLOCK_READY); break;
	case SCHED_PREEMPTING	: STAT_INCR(STAT_BLOCK_PREEMPTING); break;
	}
#endif	/* STATS */

	switch (sched_select(sched_thread_type)) {

	case SCHED_RESUME_LATER:
		/*
		 * Nothing to run.
		 *
		 * When we are unblocked, there will be a pthread set up
		 * for us to run.
		 */
		TRACE(T_VP, ("sched_block() vp idled"));
		STAT_INCR(STAT_IDLE_PAUSE);

		prctl(PR_THREAD_CTL, PR_THREAD_BLOCK);

		/* If the new pthread is the old pthread just return.
		 * This is the "cached" case.
		 */
		if (VP->vp_pt == pt_cur) {

			TRACE(T_VP, ("sched_block() idle resume same"));

			/* We blocked so we get a new quantum.
			 */
			VP_RESCHED = SCHED_POLICY_QUANTUM(pt_cur->pt_policy);

			return (sched_resume(SCHED_RESUME_SAME));
		}

		/* The original pthread should get a fresh quantum
		 * when it is resumed.
		 */
		if (pt_cur) {
			pt_cur->pt_resched =
				SCHED_POLICY_QUANTUM(pt_cur->pt_policy);
		}
		SET_PT(VP->vp_pt);

		/* FALLTHROUGH */

	case SCHED_RESUME_NEW:
	case SCHED_RESUME_SWAP:
		/*
		 * Decided to run something else.
		 */
		TRACE(T_VP, ("sched_block() full switch"));
		break;

	case SCHED_RESUME_SAME:
		/*
		 * Decided to continue on the current selection.
		 */
		TRACE(T_VP, ("sched_block() resume same"));

		return (sched_resume(SCHED_RESUME_SAME));
	}

	/* If there is no old pthread then there is no need to save the
	 * old (current) context.
	 *
	 * Otherwise, save the old context using setjmp() and wait for the
	 * new context to be free.
	 */
	if (!pt_cur || !(occupied_pt = (pt_t *)setjmp(pt_cur->pt_context))) {

		/* Wait for our new pthread to be available in case someone
		 * is trying to get off it.
		 */
		pt_longjmp(pt_cur, sched_switch, VP->vp_stk);
	}

	/* Now in the context of the new pthread. */

	return (sched_resume(occupied_pt));
}


/*
 * sched_resume(occupied_pt)
 *
 * Common code for scheduling a new or ready pthread.
 */
int
sched_resume(pt_t *pt_old)
{
	register pt_t	*pt_self = PT;
	int		flags;
	int		wait_result;

	TRACE(T_VP, ("sched_resume()"));
	ASSERT(VP->vp_occlude == 1);
	ASSERT(VP->vp_pt == PT);

	if (VP->vp_priority != PT->pt_priority) {
		VP->vp_priority = PT->pt_priority;
	}

	switch ((__psunsigned_t)pt_old) {

	case SCHED_RESUME_SAME:
		/*
		 * Context unchanged.
		 * Fast path.
		 */
		ASSERT(pt_self->pt_state == PT_DISPATCH
		       || pt_self->pt_state == PT_EXEC);
		STAT_INCR(STAT_RESUME_SAME);
		break;

	case SCHED_RESUME_NEW:
		/*
		 * No old context.
		 * Free up the stack if the old pthread passed it to us.
		 */
		ASSERT(pt_self->pt_state == PT_DISPATCH);
		STAT_INCR(STAT_RESUME_NEW);
		setoserror(pt_self->pt_errno);
		pt_self->pt_occupied = TRUE;
		VP_RESCHED = pt_self->pt_resched;
		SS_SET_ID(pthread_self());
		break;

	default:
		/* Context swapped.
		 * Save the old pthread errno value and then
		 * make it available since we are not using it.
		 * Set errno from the new pthread context.
		 */
		ASSERT(pt_self->pt_state == PT_DISPATCH);
		STAT_INCR(STAT_RESUME_SWAP);
		if (VP_RESCHED != -1) {
			pt_old->pt_errno = oserror();
			pt_old->pt_occupied = FALSE;
		}

		setoserror(pt_self->pt_errno);
		pt_self->pt_occupied = TRUE;
		VP_RESCHED = pt_self->pt_resched;
		SS_SET_ID(pthread_self());
		break;
	}

	wait_result = pt_self->pt_wait;

	pt_self->pt_state = PT_EXEC;

	ASSERT(!pt_is_bound(pt_self) || VP_RESCHED == 0);

	sched_leave();

	/* Process pending interrupts.
	 * Pending cancellations are not handled unless asynchronous.
	 */
	if (pt_is_interrupted(pt_self)) {
		flags = pt_self->pt_cncl_deferred
			? PT_INTERRUPTS & ~PT_CANCELLED : PT_INTERRUPTS;
		if (pt_self->pt_flags & flags) {
			(void)intr_check(flags);
		}
	}

	return (wait_result);
}


/*
 * sched_add_vp()
 *
 * Called when the last active vp might block in the system.
 *
 * In order to make sure we can continue to make forward progress, we may
 * need to allocate an additional vp to run waiting threads.
 */
void
sched_add_vp(void)
{
	register pt_t	*pt;
	int		nap_ticks = 0;
	extern long	__sginap(long);

	sched_enter();
	lock_enter(&schedq_lock);

	while (pt = pt_dequeue(&pt_readyq)) {
		pt_readyq_len--;

		TRACE(T_VP, ("sched_add_vp() -- found pt %#x", pt));

		pt->pt_state = PT_DISPATCH;

		/* Try to wakeup an idle vp before creating a new one.
		 *
		 * vp_wakeup_idle() drops schedq_lock if successful.
		 */
		if (vp_wakeup_idle(pt, VP_WAKEUP_ANY)) {
			sched_leave();
			return;
		}

		lock_leave(&schedq_lock);

		if (!vp_create(pt, 0)) {
			sched_leave();
			return;
		}

		/* We were too aggressive.  We can't find or create a vp
		 * so we need to put pt back onto the readyq.
		 */
		lock_enter(&schedq_lock);

		pt->pt_state = PT_READY;
		pt_q_insert_head(&pt_readyq, pt);
		pt_readyq_len++;

		PREEMPT_PT_COND(pt == pt_readyq_first());
		lock_leave(&schedq_lock);
		sched_leave();

		/* If we're not starving then we give up.
		 */
		if (vp_active) {
			return;
		}

		/* We are starving.
		 * Wait a bit and retry, periodically whine to syslog().
		 * The values here are quite arbitrary.
		 */
		nap_ticks += 100;	/* +1 sec */
		__sginap(nap_ticks);
		if ((nap_ticks % 1000) == 0) {
			syslog(LOG_WARNING,
					"PID %d: Kernel thread limit reached\n",
					getpid());
		}
		sched_enter();
		lock_enter(&schedq_lock);
	}

	lock_leave(&schedq_lock);
	sched_leave();
}


/*
 * sched_subtract_vp(sched_thread_type)
 *
 * Called when we have too many vp's currently active in the system.
 */
static int
sched_subtract_vp(sched_thread_types_t sched_thread_type)
{
	register vp_t	*vp_self = VP;
	register pt_t	*pt_self = PT;
	pt_t		*occupied_pt;	/* pass pt_cur through longjmp() */
	int		vyield;

	TRACE(T_VP, ("sched_subtract_vp(%#x)", sched_thread_type));
	ASSERT(sched_entered());
	ASSERT(!pt_self || !pt_is_bound(pt_self));

	if (pt_self) {
		if (occupied_pt = (pt_t *)setjmp(pt_self->pt_context)) {
			return (sched_resume(occupied_pt));
		}
		vyield = vp_self->vp_vyield;	/* may be voluntary */

		lock_enter(&schedq_lock);
		vp_self->vp_vyield = FALSE;

		if (sched_thread_type == SCHED_PREEMPTING) {

			/* If this thread has been preempted then we need to
			 * put it onto the readyq.
			 *
			 * Check for race with idling vps - if we are about
			 * to add a pthread to the readyq with no-one to run it
			 * we resume instead.
			 *
			 * The schedq_lock doesn't protect vp_active, but is a
			 * barrier against adding to the readyq before the
			 * last blocking vp checks the queue (sched_add_vp()).
			 */
			if (vp_active <= 0) {
				lock_leave(&schedq_lock);
				(void)VP_ADD_ACTIVE();
				if (!VP_RESCHED) {
				  VP_RESCHED =
				    SCHED_POLICY_QUANTUM(pt_self->pt_policy);
				}
				sched_leave();
				return (0);	/* wait is successful */
			}

			if (pt_self->pt_policy == SCHED_FIFO) {
				pt_self->pt_resched = (VP_RESCHED)
					? VP_RESCHED
					: sched_fifo_quantum;
				/* voluntary yields enqueue at tail */
				if (vyield) {
					pt_q_insert_tail(&pt_readyq, pt_self);
				} else {
					pt_q_insert_head(&pt_readyq, pt_self);
				}
			} else {
				if (VP_RESCHED) {
					pt_self->pt_resched = VP_RESCHED;
					pt_q_insert_head(&pt_readyq, pt_self);
				} else {
					pt_self->pt_resched = sched_rr_quantum;
					pt_q_insert_tail(&pt_readyq, pt_self);
				}
			}
			pt_readyq_len++;
			pt_self->pt_state = PT_READY;

		} else if (vp_self->vp_resume) {

			vp_self->vp_resume = FALSE;
			lock_leave(&schedq_lock);

			/* Our pthread has been made runnable again so
			 * we resume it.
			 */
			ASSERT(sched_thread_type == SCHED_READY);
			ASSERT(pt_self->pt_state == PT_DISPATCH);
			ASSERT(pt_self->pt_vp == vp_self);

			(void)VP_ADD_ACTIVE();
			VP_RESCHED = SCHED_POLICY_QUANTUM(pt_self->pt_policy);
			return (sched_resume(SCHED_RESUME_SAME));
		}

		pt_self->pt_errno = oserror();

		CLR_PT(0);
		vp_self->vp_pt = 0;
	} else {
		lock_enter(&schedq_lock);
	}

	VP_RESCHED = 0;
	vp_self->vp_stopping = FALSE;

	Q_UNLINK(vp_self);	/* off the execq */
	vp_enqueue_idle();	/* onto the idleq */

	lock_leave(&schedq_lock);

	/*
	 * Jump to vp_idle() running on the vp's stack.  We'll need to clear
	 * the current thread's occupied bit, so pass that as arg 0.
	 */
	pt_longjmp(pt_self, vp_idle, vp_self->vp_stk);

	panic("sched_subtract_vp", "returned from vp_idle longjmp");

	/* NOTREACHED */
}


/*
 * sched_dispatch(pt)
 *
 * Call this function when pt is ready to run.
 */
void
sched_dispatch(register pt_t *pt)
{
	TRACE(T_VP, ("sched_dispatch(%#x)", pt));
	ASSERT(pt->pt_state == PT_DISPATCH);
	ASSERT(sched_entered());

	/* If the waking pt is bound then we just wake up its vp.
	 */
	if (pt->pt_vp && pt_is_bound(pt)) {
		ASSERT(vp_q_is_bound(pt->pt_vp));

		prctl(PR_THREAD_CTL, PR_THREAD_UNBLOCK, pt->pt_vp->vp_pid);
		return;
	}

	lock_enter(&schedq_lock);

	if (vp_active < vp_target_count) {
		/*
		 * If there are any idle vp's, grab them first.
		 *
		 * vp_wakeup_idle() drops schedq_lock if successful.
		 */
		if (vp_wakeup_idle(pt, VP_WAKEUP_ANY)) {
			return;
		}

		lock_leave(&schedq_lock);

		if (!vp_create(pt, 0)) {
			return;
		}

		/*
		 * Since we dropped the lock, retest idleq before pt goes
		 * to readyq.
		 */
		lock_enter(&schedq_lock);
		if (!Q_EMPTY(&vp_idleq) && vp_wakeup_idle(pt, VP_WAKEUP_ANY)) {
			return;
		}
	} else {
		/*
		 * Before we decide to place this pthread onto the ready
		 * queue, we need to make sure it doesn't already have an
		 * idle vp associated with it.  If it does, we need to wake
		 * the vp so the thread can run.  This means that the vp
		 * count will be temporarily elevated, but as soon as one
		 * of the vp's enters sched_block, the count will be
		 * corrected.
		 */
		if (vp_wakeup_idle(pt, VP_WAKEUP_CACHED)) {
			return;
		}
	}

	/*
	 * No vp to run on -- put thread onto the ready queue.  Check to
	 * see if a running vp should be preempted.
	 */
	pt->pt_state = PT_READY;
	pt_q_insert_tail(&pt_readyq, pt);
	pt_readyq_len++;

	PREEMPT_PT_COND(pt == pt_readyq_first());
	lock_leave(&schedq_lock);
}


/*
 * vp_fixup_initial(pt)
 *
 * The first thread of execution needs to look like a vp, too.  Fix it up.
 */
void
vp_fixup_initial(register pt_t *pt_self)
{
	vp_t	*vp_self;

	if (!(vp_self = vp_alloc(pt_self))) {
		panic("vp_fixup_initial", "unable to allocate vp");
	}

	vp_self->vp_pid		= (pid_t)VP_PID;	/* kernel id */
	vp_self->vp_occlude	= 0;			/* not in scheduler */

	SET_VP(vp_self);
	vp_self->vp_priority = pt_self->pt_priority;
	if (pt_is_bound(pt_self)) {
		vp_q_insert_bound(vp_self);
	} else {
		Q_ADD_FIRST(&vp_execq, vp_self);
		VP_RESCHED = SCHED_POLICY_QUANTUM(pt_self->pt_policy);
	}
}


/*
 * vp_alloc(pt)
 *
 * Allocate vp and associate with pt.
 */
static vp_t *
vp_alloc(pt_t *pt)
{
#ifdef DEBUG
#	define	STK_VP_SIZE	0xe00	/* sprintf() is very expensive */
#else
#	define	STK_VP_SIZE	0x800
#endif
	vp_t	*vp = _malloc(sizeof(vp_t));
	char	*stk_base;

	if (!vp) {
		return (0);
	}
	if (pt->pt_system) {
		stk_base = PRDA->unused;	/* use the PRDA */

		if (PRDALIB->pixie_data_ptr) {	/* pixie may use PRDA->unused */
			stk_base -= 128;
		}

	} else if (!(stk_base = _malloc(STK_VP_SIZE))) {
		_free(vp);
		return (0);
	}

	TRACE(T_VP, ("vp_alloc(%#x, %#x)", vp, pt));

	vp->vp_state	= VP_EXEC;
	vp->vp_stk	= stk_base + STK_VP_SIZE;
	vp->vp_occlude	= 1;	/* new vps start out in the scheduler */
	vp->vp_stopping	= FALSE;
	vp->vp_vyield	= FALSE;
	vp->vp_resume	= FALSE;
	vp->vp_pid	= -1;	/* no kernel entity yet */

	vp_rebind(vp, pt);
	return (vp);
}


/*
 * vp_rebind(vp, pt)
 *
 * Break old vp/pt association and forge a new one.
 */
static void
vp_rebind(vp_t *vp, pt_t *pt)
{
	TRACE(T_VP, ("vp_rebind(%#x, %#x) old pt %#x", vp, pt, vp->vp_pt));
	vp->vp_pt = pt;
	pt->pt_vp = vp;
}


/*
 * vp_wakeup_idle(pt, vp_wakeup_type)
 *
 * If the vp_wakeup_type is VP_WAKEUP_CACHED and pt has a cached association,
 * schedule that vp.
 *
 * If the vp_wakeup_type is VP_WAKEUP_ANY, schedule any idle vp (but still
 * give preference to one with a cached association).
 *
 * If we find a vp we drop the lock.
 *
 * Returns TRUE	if a vp was found, FALSE otherwise.
 */
static int
vp_wakeup_idle(pt_t *pt, vp_wakeup_t vp_wakeup_type)
{
	register vp_t	*vp = pt->pt_vp;

	TRACE(T_VP, ("vp_wakeup_idle(%#x, %d)", pt, vp_wakeup_type));
	ASSERT(lock_held(&schedq_lock));
	ASSERT(pt->pt_state == PT_DISPATCH);

	if (vp && vp->vp_pt == pt) {
		if (vp->vp_state == VP_IDLE) {
			ASSERT(!pt_is_bound(pt));

			/*
			 * Could have been given a new priority by
			 * pthread_mutex_unlock(), then dispatched.
			 */
			vp->vp_priority = pt->pt_priority;
			vp->vp_state = VP_EXEC;

			Q_UNLINK(vp);

			vp_q_insert_head(&vp_execq, vp);
			(void)VP_ADD_ACTIVE();

			lock_leave(&schedq_lock);

			TRACE(T_VP, ("vp_wakeup_idle() cached vp %#x", vp));

			prctl(PR_THREAD_CTL, PR_THREAD_UNBLOCK, vp->vp_pid);

			return (TRUE);
		}

		ASSERT(!vp->vp_resume);
		vp->vp_resume = TRUE;
		lock_leave(&schedq_lock);

		TRACE(T_VP, ("vp_wakeup_idle() resume vp %#x", vp));

		return (TRUE);
	}

	/*
	 * If we are only looking for a cached vp, bail out here.
	 */
	if (vp_wakeup_type == VP_WAKEUP_CACHED) {
		TRACE(T_VP, ("vp_wakeup_idle() no cached vp"));
		return (FALSE);
	}

	/*
	 * Try to grab the first vp off the idle queue.  If we find one,
	 * associate vp and pt and start vp running.
	 */
	if (!(vp = vp_dequeue_idle())) {
		TRACE(T_VP, ("vp_wakeup_idle() no idle vps"));
		return (FALSE);
	}

	vp->vp_priority = pt->pt_priority;

	/*
	 * Bound vp's aren't put on the execq and aren't marked as active.
	 */
	if (pt_is_bound(pt)) {
		vp_q_insert_bound(vp);
	} else {
		vp_q_insert_head(&vp_execq, vp);
		(void)VP_ADD_ACTIVE();
	}

	vp_rebind(vp, pt);
	lock_leave(&schedq_lock);

	TRACE(T_VP, ("vp_wakeup_idle() idle vp %#x", vp));

	prctl(PR_THREAD_CTL, PR_THREAD_UNBLOCK, vp->vp_pid);

	return (TRUE);
}


/*
 * vp_create(pt)
 *
 * Create a new vp to run pt on.  If the creation is successful, pt will
 * start running on the new vp.
 *
 * Returns 0 if a vp was successfully created, error otherwise.
 */
int
vp_create(pt_t *pt, int flags)
{
	vp_t		*vp_new;
	prthread_t	prt;
	prsched_t	prs;
	prsched_t	*prsp = 0;

	if (!(vp_new = vp_alloc(pt))) {
		return (oserror());
	}
	TRACE(T_VP, ("vp_create(%#x) new vp %#x", pt, vp_new));

	/* The new vp starts on the target pthread stack beyond the last
	 * frame until it can longjmp() to the correct spot.
	 *
	 * The pt stack is used by the new vp so it must be vacant.
	 */
	VP_YIELD(pt->pt_occupied);

	prt.prt_entry = vp_start;
	prt.prt_arg = (caddr_t)vp_new;
	prt.prt_flags = flags|PR_THREADS|PR_NOLIBC|PR_SALL;
	prt.prt_stkptr = (caddr_t)pt->pt_context[JB_SP];
	prt.prt_stklen = 0;

	if (pt->pt_system) {
		prs.prs_policy = (pt->pt_policy == SCHED_TS) ?
					SCHED_RR : pt->pt_policy;
		prs.prs_priority = pt->pt_priority;
		prsp = &prs;
	}
	if ((vp_new->vp_pid = nsproc(vp_start, prt.prt_flags, &prt, prsp)) < 0){
		pt->pt_vp = 0;
		_free(vp_new);
		TRACE(T_VP, ("vp_create() failed %d", oserror()));

		return (oserror());
	}

	return (0);
}


/*
 * vp_start(arg, stacksize)
 *
 * Start routine of new vp.
 */
/* ARGSUSED */
static void
vp_start(void *arg, size_t stacksize)
{
	register struct vp	*vp = (struct vp *)arg;

	ASSERT(vp->vp_pt->pt_state == PT_DISPATCH);

	SET_VP(vp);
	SET_PT(vp->vp_pt);
	vp->vp_pid = (pid_t)VP_PID;

	TRACE(T_VP, ("vp_start()"));
	ASSERT(sched_entered());	/* do this _after_ VP is set */

	/* Put this vp on the execq.
	 * If we become the lowest priority we check for preemption.
	 */
	lock_enter(&schedq_lock);
	vp->vp_priority = vp->vp_pt->pt_priority;

	if (pt_is_bound(vp->vp_pt)) {
		vp_q_insert_bound(vp);
	} else {
		vp_q_insert_head(&vp_execq, vp);
		(void)VP_ADD_ACTIVE();

		PREEMPT_VP_COND(vp == vp_execq_last());
	}
	lock_leave(&schedq_lock);

	/* Jump to the new context created by setjmp() or pthread_create().
	 */
	longjmp(PT->pt_context, SCHED_RESUME_NEW);
}


/*
 * vp_exit()
 */

void
vp_exit(void)
{
	vp_t	*old_vp;
	vp_t	vp_copy;

	TRACE(T_VP, ("vp_exit()"));
	ASSERT(sched_entered());
	/* 
	 * We do this to keep the sched_leave in __libc_unlockmalloc from
	 * tampering with the (by then-free'ed) VP; the assignments have the
	 * effect of pointing VP at a copy of the current vp in vp_copy.
	 */
	old_vp = VP;
	vp_copy = *VP;
	SET_VP(&vp_copy);
	_free(old_vp);

	(void)prctl(PR_THREAD_CTL, PR_THREAD_EXIT);
	/* NOTREACHED */
}


/*
 * vp_idle(pt_old)
 *
 * This is where vp's with no associated thread jump to.  Note that we're
 * running on the vp's private stack.
 */
static void
vp_idle(register pt_t *pt_old)
{
	TRACE(T_VP, ("vp_idle(%#x)", pt_old));
	STAT_INCR(STAT_IDLE);
	ASSERT(PT == NULL);

	/*
	 * We're no longer on pt_old's stack so clear the occupied bit.
	 */
	if (pt_old) {
		pt_old->pt_occupied = FALSE;
	}

	prctl(PR_THREAD_CTL, PR_THREAD_BLOCK);

	SET_PT(VP->vp_pt);

	VP_YIELD(PT->pt_occupied);

	longjmp(PT->pt_context, SCHED_RESUME_NEW);
}


/*
 * vp_enqueue_idle(void)
 *
 * Put vp onto idle queue.
 */
static void
vp_enqueue_idle(void)
{
	TRACE(T_VP, ("vp_enqueue_idle()"));
	ASSERT(lock_held(&schedq_lock));

	VP->vp_state = VP_IDLE;

	if (PT) {
		ASSERT(!pt_is_bound(PT));

		Q_ADD_LAST(&vp_idleq, VP);
	} else {
		/*
		 * Handle vp with dead pt first.
		 */
		Q_ADD_FIRST(&vp_idleq, VP);
	}
}


/*
 * vp_dequeue_idle(void)
 *
 * Pull the first vp from queue.
 */
static vp_t *
vp_dequeue_idle(void)
{
	register vp_t	*vp;

	ASSERT(lock_held(&schedq_lock));

	Q_REM_FIRST(&vp_idleq, vp, vp_t *);

	if (vp) {
		ASSERT(vp->vp_state == VP_IDLE);
		vp->vp_state = VP_EXEC;
	}

	TRACE(T_VP, ("vp_dequeue_idle() vp %#x", vp));

	return (vp);
}


/*
 * vp_yield(retry)
 */
void
vp_yield(int retry)
{
	volatile int	i;
	struct timespec nano;
	extern int	__nanosleep(const struct timespec *, struct timespec *);

	if (sched_proc_count == 1 || retry) {
		nano.tv_nsec = 200000 * (1 << (retry % 13));
		nano.tv_sec = retry / 13;
		(void)__nanosleep(&nano, 0);
	} else {
		for (i = 0; i < 1000; i++) ;
	}
}


/*
 * vp_fork_child()
 *
 * Called in child after fork()
 */
void
vp_fork_child(void)
{
	vp_t	*vp_self = VP;

	ASSERT(sched_entered());

	vp_self->vp_pt = PT;
	vp_self->vp_pid = VP_PID;
	VP_RESCHED = SCHED_POLICY_QUANTUM(vp_self->vp_pt->pt_policy);

	/* XXX fork() leaks
	 *	vp idle and exec q
	 */
	vp_active = 1;
	vp_target_count = 1;
	lock_init(&schedq_lock);
	Q_INIT(&pt_readyq);
	Q_INIT(&vp_idleq);
	Q_INIT(&vp_execq);
	pt_readyq_len = 0;
	vp_q_insert_head(&vp_execq, vp_self);
}


/*
 * pthread_getconcurrency()
 */
int
pthread_getconcurrency(void)
{
	TRACE(T_PT, ("pthread_getconcurrency()"));
	return ((sched_balance) ? 0 : (int)vp_target_count);
}


/*
 * pthread_setconcurrency(degree)
 */
int
pthread_setconcurrency(int degree)
{
	q_t	dispatchq;
	pt_t	*pt;
	int	increase;

	TRACE(T_PT, ("pthread_setconcurrency(%d)", degree));
	if (degree < 0) {
		return (EINVAL);
	}

	/* Zero means reset to our own balancing.
	 */
	if (degree == 0) {
		if (vp_target_count > sched_proc_count) {
			vp_target_count = sched_proc_count;
		}
		sched_balance = TRUE;
		return (0);
	}

	/* Otherwise turn off balancing and make the change.
	 *
	 * If more vps became available we need to dispatch additional
	 * pthreads from the readyq immediately.
	 */
	sched_balance = FALSE;	/* no more balancing */
	sched_enter();
	lock_enter(&schedq_lock);
	increase = degree - vp_target_count;
	vp_target_count = degree;

	/* determine if we can use new vps */
	if (increase <= 0 || !(pt = pt_dequeue(&pt_readyq))) {
		lock_leave(&schedq_lock);
		sched_leave();
		return (0);
	}

	/* create temp list of candidates from readyq */
	Q_INIT(&dispatchq);
	do {
		pt_q_insert_tail(&dispatchq, pt);
		pt->pt_state = PT_DISPATCH;
	} while (--increase && (pt = pt_dequeue(&pt_readyq)));

	lock_leave(&schedq_lock);

	/* dispatch temp list */
	while (pt = pt_dequeue(&dispatchq)) {
		sched_dispatch(pt);
	}
	sched_leave();
	return (0);
}


/*
 * pthread_getrunon_np(cpu)
 *
 * Report binding of calling pthread to cpu if any.
 */
int
pthread_getrunon_np(int *cpu)
{
	TRACE(T_PT, ("pthread_getrunon_np()"));

	if (!PT->pt_system) {
		return (EPERM);
	}
	if ((*cpu = (int)sysmp(MP_GETMUSTRUN)) == -1) {
		return (oserror());
	}
	return (0);
}


/*
 * pthread_setrunon_np(cpu)
 *
 * Establish binding of calling pthread to cpu.
 */
int
pthread_setrunon_np(int cpu)
{
	TRACE(T_PT, ("pthread_setrunon_np(%d)", cpu));

	if (!PT->pt_system) {
		return (EPERM);
	}
	if (sysmp(MP_MUSTRUN, cpu) != 0) {
		return (oserror());
	}
	return (0);
}


/*
 * pthread_getschedparam(pt_id, policy, sched)
 */
int
pthread_getschedparam(pthread_t pt_id, int *policy, struct sched_param *sched)
{
	register pt_t	*pt;

	if ((pt = pt_ref(pt_id)) == NULL) {
		return (ESRCH);
	}

	sched_enter();
	lock_enter(&pt->pt_lock);

	*policy = pt->pt_policy;
	sched->sched_priority = pt->pt_schedpriority;

	lock_leave(&pt->pt_lock);
	sched_leave();

	TRACE(T_PT, ("pthread_getschedparam(%#x, %d, %d)",
		     pt, *policy, sched->sched_priority));

	pt_unref(pt);
	return (0);
}


/*
 * pthread_setschedparam(pt_id, policy, sched)
 */
int
pthread_setschedparam(pthread_t pt_id, int policy,
		      const struct sched_param *sched)
{
	register pt_t	*pt;
	int		pri;

	if ((pt = pt_ref(pt_id)) == NULL) {
		return (ESRCH);
	}
	TRACE(T_PT, ("pthread_setschedparam(%#x, %d, %d)",
		     pt, policy, sched->sched_priority));

	sched_enter();
	lock_enter(&pt->pt_lock);
	lock_enter(&schedq_lock);

	pri = MTX_INHERITPRI(pt);
	if (pri < sched->sched_priority) {
		pri = sched->sched_priority;
	}

	if (pt->pt_system && pt->pt_state != PT_DEAD) {
		prsched_t	prs;
		prs.prs_policy = (policy == SCHED_TS) ?  SCHED_RR : policy;
		prs.prs_priority = pri;

		if (prctl(PR_THREAD_CTL, PR_THREAD_SCHED,
			  pt->pt_vp->vp_pid, &prs, 0)) {
			lock_leave(&schedq_lock);
			lock_leave(&pt->pt_lock);
			sched_leave();
			pt_unref(pt);
			return (oserror());
		}
	} else if (!pt_is_bound(pt)) {
		pt->pt_resched = SCHED_POLICY_QUANTUM(policy);
	}
	pt->pt_policy = policy;
	pt->pt_priority = pri;
	pt->pt_schedpriority = sched->sched_priority;

	/*
	 * Update pt scheduling state.  Releases pt_lock and schedq_lock.
	 */
	vp_setschedparam(pt, 0);

	sched_leave();

	pt_unref(pt);
	return (0);
}


/*
 * vp_setpri(pt, pri, flags)
 *
 * Wrapper to avoid exporting schedq_lock.
 */
void
vp_setpri(register pt_t *pt, register schedpri_t pri, int flags)
{
	ASSERT(lock_held(&pt->pt_lock));
	lock_enter(&schedq_lock);

	if (pt->pt_system && pt->pt_state != PT_DEAD) {
		prsched_t	prs;
		prs.prs_policy = (pt->pt_policy == SCHED_TS) ?
					SCHED_RR : pt->pt_policy;
		prs.prs_priority = pri;

		(void)prctl(PR_THREAD_CTL, PR_THREAD_SCHED,
			    pt->pt_vp->vp_pid, &prs, 0);
	}
	vp_setschedparam(pt, flags);
}


/*
 * vp_setschedparam(pt, flags)
 *
 * Called to update pt's new priority and policy into vp.
 */
static void
vp_setschedparam(register pt_t *pt, int flags)
{
	pt_states_t	old_state;
	register vp_t	*vp;
	pthread_mutex_t	*mutex;
	register int	preempt = (flags & SETPRI_NOPREEMPT) == 0;

	ASSERT(sched_entered());
	ASSERT(lock_held(&schedq_lock));
	ASSERT(lock_held(&pt->pt_lock));

	intr_destroy_disable();

retry_update:
	switch (old_state = pt->pt_state) {

	case PT_EXEC:

		/*
		 * Target pt is running.
		 * Change the vp priority and reorder the execq.
		 * If the policy changed then the quantum counter
		 * needs to be reset so we preempt it.
		 * If the vp is last it may be preempted.
		 */
		vp = pt->pt_vp;
		vp->vp_priority = pt->pt_priority;
		lock_leave(&pt->pt_lock);	/* no more need to hold pt */

		/* Bound vps do not run on the execq and cannot
		 * preempt or be preempted.
		 */
		if (pt_is_bound(pt)) {
			lock_leave(&schedq_lock);
			break;
		}

		vp_q_reorder_tail(&vp_execq, vp);

		if (pt == PT && preempt) {
			vp->vp_stopping = TRUE;
			vp->vp_vyield = TRUE;
			lock_leave(&schedq_lock);

			intr_destroy_enable();

			VP_RESCHED = 0;
			(void)sched_block(SCHED_PREEMPTING);

			/* sched_block consumed our sched_enter */
			sched_enter();

			return;
		}

		if (preempt) {
			PREEMPT_VP_COND(vp == vp_execq_last());
		}

		lock_leave(&schedq_lock);

		break;

	case PT_READY:
		/*
		 * Target pt is runnable.
		 * Reorder the readyq and if the pt is first it may preempt
		 * a running pt.
		 */
		lock_leave(&pt->pt_lock);	/* no need to hold pt */
		pt_q_reorder_tail(&pt_readyq, pt);

		PREEMPT_PT_COND(pt == pt_readyq_first());

		lock_leave(&schedq_lock);

		break;

	case PT_CVWAIT:
	case PT_CVTWAIT:
	case PT_MWAIT:
	case PT_RLWAIT:
	case PT_SWAIT:
	case PT_WLWAIT:
		/*
		 * Reorder the waitq so wakeups follow ordering.
		 */
		lock_leave(&schedq_lock);

		ASSERT(&pt->pt_sync_slock);
		lock_enter(&pt->pt_sync_slock);

		if (pt->pt_state != old_state) {
			lock_leave(&pt->pt_sync_slock);
			lock_enter(&schedq_lock);
			goto retry_update;
		}

		if (pt->pt_state != PT_WLWAIT) {
			pt_q_reorder_tail(&pt->pt_sync_waitq1, pt);
		} else {
			pt_q_reorder_tail(&pt->pt_sync_waitq2, pt);
		}

		lock_leave(&pt->pt_lock);

		mutex = (mtx_t *)pt->pt_sync;
		if (pt->pt_state == PT_MWAIT
		    && mutex->mtx_attr.ma_protocol == PTHREAD_PRIO_INHERIT) {
			/*
			 * releases pt->pt_sync_slock
			 */
			mtx_giveinheritance(mutex, pt->pt_priority);
		} else {
			lock_leave(&pt->pt_sync_slock);
		}

		break;

	case PT_JWAIT:
		/*
		 * Wakeup order not defined so we skip it.
		 */
		/* FALLTHROUGH */

	case PT_DEAD:
	case PT_DISPATCH:
		/* FALLTHROUGH */

	default:
		lock_leave(&schedq_lock);
		lock_leave(&pt->pt_lock);

		break;
	}

	intr_destroy_enable();
}


/*
 * libc_sched_yield()
 *
 * POSIX 1B interface: relinquish the (virtual) processor.
 */
int
libc_sched_yield(void)
{
	register pt_t		*pt_self = PT;
	extern int		__sched_yield(void);

	TRACE(T_VP, ("libc_sched_yield()"));
	if (pt_is_bound(pt_self)) {

		/* If we are bound we tell the kernel to run someone else.
		 */
		__sched_yield();
		return (0);
	}

	sched_enter();
	lock_enter(&schedq_lock);

	/* If there is nothing to yield to in this process then we just return.
	 */
	if (Q_EMPTY(&pt_readyq)
	    || pt_readyq_first()->pt_priority < pt_self->pt_priority) {
		lock_leave(&schedq_lock);
		sched_leave();
		return (0);
	}
	/* Yield to anyone of greater or equal priority.
	 */
	VP->vp_vyield = TRUE;
	lock_leave(&schedq_lock);

	VP_RESCHED = 0;		/* expire quantum to permit preemption */
	(void)sched_block(SCHED_PREEMPTING);
	return (0);
}


/*
 * libc_threadbind()
 *
 * Dynamically bind the calling pthread to its vp.
 */
void
libc_threadbind(void)
{
	vp_t	*vp_self;
	pt_t	*pt_self = PT;

	if (pt_is_bound(pt_self)) {
		return;		/* already bound */
	}

	sched_enter();
	lock_enter(&pt_self->pt_lock);
	pt_self->pt_glued = TRUE;
	lock_leave(&pt_self->pt_lock);
	VP_RESCHED = 0;
	vp_self = VP;		/* sched_entered() */

	/* Move to bound queue.
	 */
	lock_enter(&schedq_lock);
	Q_UNLINK(vp_self);
	vp_q_insert_bound(vp_self);
	lock_leave(&schedq_lock);
	sched_leave();

	VP_RESERVE();		/* adjust active population */
}


/*
 * libc_threadunbind()
 *
 * Reverse a bind for the calling pthread.
 */
void
libc_threadunbind(void)
{
	pt_t	*pt_self = PT;

	if (!pt_self->pt_glued) {
		return;		/* NOT bound */
	}

	sched_enter();
	lock_enter(&pt_self->pt_lock);
	pt_self->pt_glued = FALSE;
	lock_leave(&pt_self->pt_lock);
	VP_RESCHED = SCHED_POLICY_QUANTUM(pt_self->pt_policy);

	/* Move to exec queue.
	 */
	lock_enter(&schedq_lock);
	vp_q_insert_head(&vp_execq, VP);
	lock_leave(&schedq_lock);
	sched_leave();

	VP_RELEASE();		/* adjust active population */
}
