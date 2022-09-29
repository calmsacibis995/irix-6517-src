/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.27 $"

#include <sys/types.h>
#include <ksys/cred.h>
#include <sys/proc.h>
#include <sys/prctl.h>
#include <ksys/vproc.h>
#include <sys/errno.h>
#include <sys/runq.h>
#include "os/scheduler/rt.h"
#include "os/scheduler/sched.h"
#include "os/scheduler/space.h"
#include "pproc.h"
#include "pproc_private.h"
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#endif /* _SHAREII */



static int
uthread_sched_setparam(uthread_t *ut, int pri)
{
	kthread_t *kt = UT_TO_KT(ut);
	int	s;
	int	dequeued = 0;
	cpuid_t	cpu;
	int	error = 0;

retry:
	s = kt_lock(kt);
	if (kt->k_onrq != CPU_NONE) {
		if (removerunq(kt))
			dequeued++;
		else {
			kt_unlock(kt, s);
			goto retry;
		}
	}

	switch(ut->ut_policy) {
	case SCHED_FIFO:
	case SCHED_RR:
	case SCHED_NP:
		if (pri < PX_PRIO_MIN || pri > PX_PRIO_MAX) {
			error = EINVAL;
			break;
		}
		kt_initialize_pri(kt, pri);
		break;
	case SCHED_TS:
		if (pri < TS_PRIO_MIN || pri > TS_PRIO_MAX) {
			error = EINVAL;
			break;
		}
		kt_initialize_pri(kt, PTIME_SHARE);	
		ut->ut_pproxy->prxy_sched.prs_nice = pri;
		wt_set_weight(kt, pri);
		break;
	default:
		error = EINVAL;
		break;
	}

	if (dequeued) {
		putrunq(kt, CPU_NONE);
		kt_unlock(kt, s);
	} else if ((cpu = kt->k_sonproc) != CPU_NONE) {
		kt_unlock(kt, s);
		CPUVACTION_RESCHED(cpu)
	} else
		kt_unlock(kt, s);

	return (error);
}

void
pproc_sched_setparam(
	bhv_desc_t	*bhv,
	int		pri,
	int		*error)
{
	proc_t		*p;
	proc_proxy_t	*prxy;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);
	prxy = &p->p_proxy;

	if (prxy->prxy_sched.prs_flags & PRSBATCH) {
		*error = EINVAL;
		return;
	}

	uscan_update(prxy);
	if (IS_THREADED(prxy)) {
		uthread_t	*ut;

		/* For threaded apps change all the process scope uthreads
		 */
		uscan_forloop(prxy, ut) {
			if (!(ut->ut_flags & UT_PTPSCOPE)) {
				continue;
			}
			if (*error = uthread_sched_setparam(ut, pri)) {
				break;
			}
		}
		if (!*error) {
			if (prxy->prxy_sched.prs_policy == SCHED_TS) {
				prxy->prxy_sched.prs_nice = pri; 
			} else {
				prxy->prxy_sched.prs_priority = pri; 
			}
		}
	} else {
		/* Change the (only) uthread.
		 */
		*error = uthread_sched_setparam(prxy_to_thread(prxy), pri);
	}
	uscan_unlock(prxy);
}

void
pproc_sched_getparam(
	bhv_desc_t	*bhv,
	cred_t		*cr,
	int		*pri,
	int		*error)
{
	proc_t		*p;
	proc_proxy_t	*prxy;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);
	prxy = &p->p_proxy;

	if (!_CAP_CRABLE(cr, CAP_SCHED_MGT)) {
		cred_t *pcr = pcred_access(p);
		if (cr->cr_ruid != pcr->cr_ruid) {
			*error = EPERM;
			pcred_unaccess(p);
			return;
		}
		pcred_unaccess(p);
	}

	uscan_access(prxy);
	if (IS_THREADED(prxy)) {
		 *pri = (prxy->prxy_sched.prs_policy == SCHED_TS)
			? prxy->prxy_sched.prs_nice
			: prxy->prxy_sched.prs_priority;
	} else {
		uthread_t *ut = prxy_to_thread(prxy);

		*pri = (ut->ut_policy == SCHED_TS)
			? prxy->prxy_sched.prs_nice
			: UT_TO_KT(ut)->k_basepri;
	}
	uscan_unlock(prxy);
}


int
uthread_sched_setscheduler(uthread_t *ut, void *arg)
{
	kthread_t *kt = UT_TO_KT(ut);
	int	s;
	int	dequeued = 0;
	int	pri = ((prsched_t *)arg)->prs_priority;
	int	policy = ((prsched_t *)arg)->prs_policy;
	cpuid_t	cpu;
	job_t	*job;
	int	error = 0;
#ifdef _SHAREII
	int	oldpolicy = ut->ut_policy;
#endif /* _SHAREII */

	/*
	 * Gang threads which are changing to realtime must be removed
	 * from the gang.
	 */
	if (IS_SPROC(ut->ut_pproxy)) {
		switch (policy) {
		case SCHED_FIFO:
		case SCHED_RR:
		case SCHED_NP:
			gang_unlink(kt);
		}
	}

retry:
	s = kt_lock(kt);
	if (kt->k_onrq != CPU_NONE) {
		if (removerunq(kt))
			dequeued++;
		else {
			kt_unlock(kt, s);
			goto retry;
		}
	}

	switch(policy) {
	case SCHED_FIFO:
	case SCHED_RR:
	case SCHED_NP:
		if (pri < PX_PRIO_MIN || pri > PX_PRIO_MAX) {
			error = EINVAL;
			break;
		}
		if (ut->ut_job) {
			job_unlink_thread(kt);
			ut->ut_policy = policy;
			kt_initialize_pri(kt, pri);
			kt->k_flags |= KT_BASEPS | KT_PS;
			if (policy == SCHED_NP) {
				kt->k_flags &= ~KT_BIND;
				kt->k_flags |= KT_NPRMPT|KT_NBASEPRMPT;
			} else {
				kt->k_flags |= KT_BIND;
				kt->k_flags &= ~(KT_NPRMPT|KT_NBASEPRMPT);
				start_rt(kt, kt->k_pri);
			}
		} else {
			ut->ut_policy = policy;
			kt_initialize_pri(kt, pri);
			kt->k_flags |= KT_BASEPS | KT_PS;
		}	
		break;
	case SCHED_TS:
		if (pri < TS_PRIO_MIN || pri > TS_PRIO_MAX) {
			error = EINVAL;
			break;
		}
		ut->ut_policy = policy;
		if (!ut->ut_job) {
			kt->k_flags &= ~(KT_PS|KT_BASEPS|KT_BIND);
			kt->k_flags &= ~(KT_NPRMPT|KT_NBASEPRMPT);

			if (!(ut->ut_flags & UT_PTHREAD)) {
				(void) job_create(JOB_WT, kt);
			} else if (job = ut->ut_pproxy->prxy_sched.prs_job) {
				job_attach_thread(job, kt);
			} else {
				job = job_create(JOB_WT, kt);
				job_cache(job);
				ut->ut_pproxy->prxy_sched.prs_job = job;
			}
			end_rt(kt);
		}
		kt_initialize_pri(kt, PTIME_SHARE);	
		ut->ut_pproxy->prxy_sched.prs_nice = pri;
		wt_set_weight(kt, pri);
		break;
	default:
		error = EINVAL;
		break;
	}


	if (dequeued) {
		putrunq(kt, CPU_NONE);
		kt_unlock(kt, s);
	} else if ((cpu = kt->k_sonproc) != CPU_NONE) {
		kt_unlock(kt, s);
		CPUVACTION_RESCHED(cpu)
	} else
		kt_unlock(kt, s);

#ifdef _SHAREII
	/* shareII needs to notice the change
	 * if either old or new policy is SCHED_TS.
	 * Simpler to tell it always.
	 * Called outside the scope of the kt_lock()
	 * to avoid possible deadlock problems.
	 */
	if (error == 0)
		SHR_SETSCHEDULER(ut, oldpolicy, pri);
#endif /* _SHAREII */
	return (error);
}

#ifdef CKPT
void
uthread_sched_getscheduler(uthread_t *ut, void *arg)
{
	prsched_t *prsched = (prsched_t *)arg;

	prsched->prs_policy = ut->ut_policy;

	switch (ut->ut_policy) {
	case SCHED_FIFO:
	case SCHED_RR:
	case SCHED_NP:
		prsched->prs_priority = kt_basepri(UT_TO_KT(ut));
		break;
	case SCHED_TS:
		prsched->prs_priority = wt_get_priority(UT_TO_KT(ut));
		break;
	}
}
#endif

void
pproc_sched_setscheduler(
	bhv_desc_t	*bhv,
	int		pri,
	int		policy,
	__int64_t	*rval,
	int		*error)
{
	proc_t		*p;
	proc_proxy_t	*prxy;
	prsched_t	arg;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);
	prxy = &p->p_proxy;

	if (prxy->prxy_sched.prs_flags & PRSBATCH) {
		*error = EINVAL;
		return;
	}
	arg.prs_priority = pri;
	arg.prs_policy = policy;

	uscan_update(prxy);
	if (IS_THREADED(prxy)) {
		uthread_t	*ut;

		/* For threaded apps change all the process scope uthreads
		 */
		uscan_forloop(prxy, ut) {
			if (!(ut->ut_flags & UT_PTPSCOPE)) {
				continue;
			}
			if (*error = uthread_sched_setscheduler(ut, &arg)) {
				break;
			}
		}
		if (!*error) {
			*rval = prxy->prxy_sched.prs_policy;
			if (policy == SCHED_TS) {
				prxy->prxy_sched.prs_nice = pri; 
			} else {
				prxy->prxy_sched.prs_priority = pri; 
			}
			prxy->prxy_sched.prs_policy = policy; 
		}
	} else {
		/* Change the (only) uthread.
		 */
		*error = uthread_sched_setscheduler(prxy_to_thread(prxy), &arg);
	}
	uscan_unlock(prxy);
}

void
pproc_sched_getscheduler(
	bhv_desc_t	*bhv,
	cred_t		*cr,
	__int64_t	*pol,
	int		*error)
{
	proc_t		*p;
	proc_proxy_t	*prxy;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);
	prxy = &p->p_proxy;

	if (!_CAP_CRABLE(cr, CAP_SCHED_MGT)) {
		cred_t *pcr = pcred_access(p);
		if (cr->cr_ruid != pcr->cr_ruid) {
			*error = EPERM;
			pcred_unlock(p);
			return;
		}
		pcred_unaccess(p);
	}

	uscan_access(prxy);

	if (IS_THREADED(prxy))
		*pol = prxy->prxy_sched.prs_policy;
	else if (prxy_to_thread(prxy)) {
		*pol = prxy_to_thread(prxy)->ut_policy;
	} else {
		*error = ESRCH;
	}	
		
	uscan_unlock(prxy);
}

void
pproc_sched_rr_get_interval(
	bhv_desc_t	*bhv,
	timespec_t	*timeslice)
{
	proc_t		*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* assumes all time slices are the same */
	uscan_hold(&p->p_proxy);
	tick_to_timespec(prxy_to_thread(&p->p_proxy)->ut_tslice, timeslice,
								NSEC_PER_TICK);
	uscan_rele(&p->p_proxy);
}
