/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/idbgentry.h>
#include <sys/debug.h>
#include <sys/atomic_ops.h>
#include <sys/proc.h>
#include <sys/uthread.h>
#include <sys/space.h>
#include <sys/atomic_ops.h>
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#include <sys/sched.h>		/* for SCHED_TS */
#endif /*_SHAREII*/


wtree_t		wtree;
#ifdef MP
#pragma fill_symbol (wtree, L2cacheline)
#endif

#define WT_BALSHFT		24

#define wtree_nested_lock()	nested_spinlock(&wtree.wt_lock)
#define wtree_nested_trylock()  nested_spintrylock(&wtree.wt_lock)
#define wtree_nested_unlock()	nested_spinunlock(&wtree.wt_lock)
#define wtree_islocked()	spinlock_islocked(&wtree.wt_lock)

#define get_weight(__pri)	wtree.wt_weights[__pri]

void	wt_dbgp			(job_t *);
void	wt_run_time		(kthread_t *);
void	wt_detach		(job_t *);

schedvec_t	wt_schedvec = {
	"wt",
	wt_dbgp,
	wt_run_time,
	wt_detach
};


__inline int	wt_active	(job_t *j)	{ return q_onq(&j->j_queue); }


__inline void
wt_activate(job_t *j)
{
	ASSERT( wtree_islocked() && job_islocked(j) );
	ASSERT( j && j->j_type == JOB_WT );
	ASSERT( !wt_active(j) );
	pushq(&wtree.wt_ajobq, &j->j_queue);
}


__inline void
wt_deactivate(job_t *j)
{
	ASSERT( wtree_islocked() && job_islocked(j) );
	ASSERT( j && j->j_type == JOB_WT );
	ASSERT( wt_active(j) );
	rmq(&j->j_queue);
}


void
wt_init(job_t *j, kthread_t *kt)
{
	ASSERT( issplhi(getsr()) );
	ASSERT( KT_ISUTHREAD(kt) );
	j->j_ops = wt_schedvec;
	j->w_acctime = 0;
#ifdef DEBUG
	j->w_lastacc = 0;
	j->w_lastinc = 0;
#endif
#ifdef _SHAREII
	if ((j->w_weight = SHR_GET_WEIGHT(KT_TO_UT(kt))) < 0)
#endif
	j->w_weight = get_weight(KT_TO_UT(kt)->ut_pproxy->prxy_sched.prs_nice);
#ifdef _SHAREII
	/*
	 * Share needs to know about the change.
	 * this is usually covered in uthread_sched_setscheduler,
	 * but also called by nsproc(). Hooking here is a compromise.
	 */
	SHR_SETSCHEDULER(KT_TO_UT(kt), SCHED_TS,
		KT_TO_UT(kt)->ut_pproxy->prxy_sched.prs_nice);
#endif /* _SHAREII */
	wtree_nested_lock();
	job_nested_lock(j);
	wt_activate(j);
	wtree_nested_unlock();
	job_nested_unlock(j);
}


int
wt_get_priority(kthread_t *kt)
{
	ASSERT( KT_ISUTHREAD(kt) );
	ASSERT( KT_TO_UT(kt)->ut_job );
	return KT_TO_UT(kt)->ut_pproxy->prxy_sched.prs_nice;
}


void
wt_set_weight(kthread_t *kt, int pri)
{
	job_t *		j;
	weight_t	w;

	ASSERT( pri <= 2*NZERO );
	ASSERT( KT_ISUTHREAD(kt) );
	ASSERT( kt->k_basepri <= 2*NZERO );

	j = KT_TO_UT(kt)->ut_job;
	if (j && j->j_type == JOB_WT) {
#ifdef _SHAREII
		/* 
		 * when ShareII is weighting dynamically,
		 * the weight is tweaked, and the pri ignored.
		 * Returns -1 if Share non-operative.
		 */
		w = SHR_GET_WEIGHT(KT_TO_UT(kt));
		if (w < 0)
#endif /*!_SHAREII*/
		w = get_weight(pri);
		j->w_weight = w;
	}
}


void
wt_dbgp(job_t *j)
{
	qprintf("          weight %lld acctime %lld\n",
		j->w_weight, j->w_acctime);
#ifdef DEBUG
	qprintf("          lastacc %lld lastinc %lld\n",
		j->w_lastacc, j->w_lastinc);
#endif
}


void
wt_run_time(kthread_t *kt)
{
	job_t *		j	= KT_TO_UT(kt)->ut_job;

	ASSERT( issplhi(getsr()) );
#if (MAXCPUS > 128)
	{
	extern int sched_tick_mask;
	atomicAddInt64Hot2(&j->w_acctime, -(10000*(sched_tick_mask+1)), &j->w_pref);
	}
#else
	atomicAddInt64Hot2(&j->w_acctime, -10000, &j->w_pref);
#endif
	if (!wt_active(j)) {
		wtree_nested_lock();
		job_nested_lock(j);
		if (!wt_active(j))
			wt_activate(j);
		job_nested_unlock(j);
		wtree_nested_unlock();
	}
}


void
wt_detach(job_t *j)
{
	ASSERT( issplhi(getsr()) );
	if (wt_active(j)) {
		wtree_nested_lock();
		job_nested_lock(j);
		if (wt_active(j))
			wt_deactivate(j);
		wtree_nested_unlock();
		job_nested_unlock(j);
	}
}


void
wtree_init()
{
	uint	i;

	spinlock_init(&wtree.wt_lock, "wtree-lock");
#if MP
	wtree.wt_rotor = masterpda->p_cpuid;
#endif
	init_q_element(&wtree.wt_ajobq, &wtree);

	wtree.wt_weights[0] = 10;
	wtree.wt_weights[1] = 1000;
	for (i = 1; i < NZERO * 2; i++)
		wtree.wt_weights[i + 1] = wtree.wt_weights[i] +
					  wtree.wt_weights[i] / 10;
}


#define BOTH_SHARE	((((unsigned short)PTIME_SHARE) << 16) |	\
			   ((unsigned short)PTIME_SHARE))
#define BOTH_SHARE_OVER	((((unsigned short)PTIME_SHARE_OVER) << 16) |	\
			   ((unsigned short)PTIME_SHARE_OVER))
void
wtree_sched_tick()
{
	job_t *		j;
	job_t *		nj;
	weight_t	twt	= 0;
	credit_t	bal	= 0;
	credit_t	mul;

	ASSERT( issplhi(getsr()) );
#if MP
	if (cpuid() == masterpda->p_cpuid) {
		cpuid_t	cpu = wtree.wt_rotor;
		ASSERT(cpu_running(cpuid()) && !cpu_restricted(cpuid()));
		do {
			cpu = (cpu < maxcpus - 1) ? cpu + 1 : 0;
		} while (!cpu_running(cpu) || cpu_restricted(cpu) == 1);
		wtree.wt_rotor = cpu;
	}
	if (cpuid() != wtree.wt_rotor)
		return;
#endif

	if (wtree_nested_trylock() == 0)
		return;
#ifdef DEBUG
	wtree.wt_lastcnt = 0;
	wtree.wt_lasttwt = 0;
	wtree.wt_lastbal = 0;
	wtree.wt_lastacc = 0;
	wtree.wt_lastmul = 0;
#endif
	for (j = JOB_FIRST(&wtree); j != JOB_ENDQ(&wtree); j = JOB_NEXT(j)) {
#ifdef DEBUG
		wtree.wt_lastcnt++;
#endif
		twt += j->w_weight;
		bal -= j->w_acctime;
	}
#ifdef DEBUG
	wtree.wt_lasttwt = twt;
	wtree.wt_lastbal = bal;
#endif
	if (twt == 0) {
		wtree_nested_unlock();
		return;
	}

	mul = (bal << WT_BALSHFT) / twt;
#ifdef DEBUG
	wtree.wt_lastmul = mul;
#endif

	for (j = JOB_FIRST(&wtree); j != JOB_ENDQ(&wtree); j = nj) {
		credit_t	acc;
		kthread_t	*kt;
		int		pri;
		int		onq;

		job_nested_lock(j);
		nj = JOB_NEXT(j);
		if (!j->j_nthreads) {
			job_nested_unlock(j);
			continue;
		}
#ifdef _SHAREII
		if (SHR_ACTIVE)
			j->w_weight = SHR_GET_WEIGHT(KT_TO_UT(j->j_thread));
#endif /* _SHAREII */

		acc = (j->w_weight * mul) >> WT_BALSHFT;
#ifdef DEBUG
		j->w_lastacc = j->w_acctime;
		j->w_lastinc = acc;
		wtree.wt_lastacc += acc;
#endif
		acc = atomicAddInt64(&j->w_acctime, acc);
		pri = acc < 0 ? BOTH_SHARE_OVER : BOTH_SHARE;
		kt = j->j_thread;
		onq = 0;
		do {
			ASSERT(kt->k_basepri > PWEIGHTLESS);
			onq |= kt->k_onrq >= 0 || kt->k_sonproc >= 0;
			if (kt->k_basepri != ((short)pri) && ((!kt->k_preempt &&
			    kt->k_sonproc < 0) || (short)pri == PTIME_SHARE)) {
				if (!KT_ISPS(kt))
					*((int *)&kt->k_pri) = pri;
				else
					kt->k_basepri = (short)pri;
			}
		} while (kt = kt->k_link);

		if (acc > 0) {
			credit_t	max;

			max = 2 * j->j_nthreads * USEC_PER_TICK *
			      KT_TO_UT(j->j_thread)->ut_tslice;
			if (acc > max)
				j->w_acctime = max;
			if (!onq)
				wt_deactivate(j);
		}

		job_nested_unlock(j);
	}

	wtree_nested_unlock();
}
