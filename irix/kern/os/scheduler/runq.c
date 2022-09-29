/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "sys/types.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "sys/proc.h"
#include "ksys/vproc.h"
#include "sys/par.h"
#include "sys/debug.h"
#include "sys/sema.h"
#include "sys/pda.h"
#include "sys/schedctl.h"
#include "sys/prctl.h"
#include "sys/kmem.h"
#include "sys/runq.h"
#include "sys/idbgentry.h"
#include "sys/cmn_err.h"
#include <stdarg.h>
#include "utility.h"
#include "space.h"
#include "rt.h"
#include <sys/frs.h>
#include "frame/frame_base.h"
#include <sys/sched.h>
#include <sys/cpumask.h>
#include <sys/sema_private.h>
#include <sys/rtmon.h>
#ifdef NUMA_SCHED
#include <sys/nodepda.h>
#endif
#include <sys/kthread.h>
#include <ksys/vproc.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */

#ifdef MP
kthread_t **nextthread[MAXCPUS];
#define NEXTTHREAD(cpu)	nextthread[cpu]

#ifdef RT_SCALE
void rt_setpri(short);
extern kthread_t **pNT[];
#endif /* RT_SCALE */
#else
#define NEXTTHREAD(cpu) &private.p_nextthread
#endif

extern int memaff_sched;
extern batchq_t batch_queue;
/*
 * Kernel globals we need access to.
 */
extern int	fasthz;			/* how fast is our clock? */
extern int	slice_size;		/* default time slice */
extern int	gfx_disp(uthread_t *);

pda_t *
findpda(cpuid_t cpu)
{
   int	i;

	for (i = 0; i < maxcpus; i++)
		if (pdaindr[i].CpuId == cpu)
			return(pdaindr[i].pda);
	ASSERT(0);
	/* NOTREACHED */
}

/*
 * Initialize the run queue data structures. Called once from the master
 * processor to initialize everything.
 */
void
makeRunq(void)
{
#ifdef MP
	int i;
	
	for (i = 0; i < maxcpus; i++)
		nextthread[i] = &pdaindr[i].pda->p_nextthread;
#endif

        /*
	 * Frame Scheduler
	 */
        frs_driver_init();
}

/*
 * Called by other processors when they wish to begin scheduling processes.
 */
void
joinrunq(void)
{
	global_cpuset.cs_lbpri = PIDLE;
	private.p_curpri = PIDLE;
	private.p_idler = 0; 
}


/*
 * Runnable exception handler. Called if any exception conditions are
 * outstanding. Returns non-zero if process is runnable.
 */
int
_condRunchecks(kthread_t *kt)
{
	int runcond = kt->k_runcond;

	if (runcond & RQF_GFX) {
		ASSERT(KT_ISUTHREAD(kt)); /* only processes can have gfx */
		if (!gfx_disp(KT_TO_UT(kt))) {
			private.p_idletkn |= NO_GFX;
			return(0);
		}
	}
	return(base_sched(kt));
}

extern int rtcpus;

#ifdef MP
INLINE static cpuid_t
pick_q(kthread_t *kt, cpuid_t cpu)
{
	ASSERT(!KT_ISMR(kt) || KT_ISKB(kt) || kt->k_binding == CPU_NONE ||
	       kt->k_mustrun == kt->k_binding);

	if (KT_ISMR(kt))
		return kt->k_mustrun;
	if (kt->k_flags & KT_PSPIN)
		return kt == curthreadp ? cpuid() : kt->k_lastrun;
#ifdef RT_SCALE
	if (rtcpus >= maxcpus)
		if (KT_ISPS(kt) && kt->k_binding < 0)
			return CPU_NONE;
#endif	
	if (cpu < 0)
		cpu = kt->k_binding >= 0 ? kt->k_binding : kt->k_lastrun;

	if (cpu >= 0) {
		int r = pdaindr[cpu].pda->p_cpu.c_restricted;

		if (r && (r == 1 || r != kt->k_cpuset))
			cpu = cpuset_idle(kt);
	}

	return cpu;
}
#else
#define pick_q(a,b)	0
#endif

/*ARGSUSED*/
INLINE static void
dispatch_q(kthread_t *kt, cpuid_t cpu)
{
	ASSERT(cpu >= 0 && cpu <= maxcpus);

#ifdef MP
	if (cpu == cpuid() || cpu == maxcpus) {
		if (kt->k_pri > private.p_curpri)
			private.p_runrun = 1;
	} else {
		if (kt->k_pri > pdaindr[cpu].pda->p_curpri)
			SEND_VA_FORCE_RESCHED(cpu);
	}
#else
	if (kt->k_pri > private.p_curpri)
		private.p_runrun = 1;
#endif
}

void
putrunq(struct kthread *kt, cpuid_t cpu)
{
	ASSERT(issplhi(getsr()) && kt_islocked(kt));
	ASSERT(kt->k_onrq == CPU_NONE && kt->k_wchan == 0);
	ASSERT(!KT_ISUTHREAD(kt) || !KT_TO_UT(kt)->ut_job ||
	       !KT_ISPS(kt) || kt->k_inherit || KT_ISKB(kt));

	cpu = pick_q(kt, cpu);
	ASSERT(cpu < maxcpus && (cpu < 0 || NEXTTHREAD(cpu)));
	if (!runcond_ok(kt) || cpu < 0 ||
	    !compare_and_swap_kt(NEXTTHREAD(cpu), 0, kt)) {

		int s = splprof();
		ktimer_switch(kt, AS_RUNQ_WAIT);
		splx(s);

#ifdef MP
		if (cpu >= 0 || (!KT_ISPS(kt) && 
					(cpu = cpuset_master(kt), 1))) {
			cpu_insert_localq(kt, cpu);
		} else {
			cpu = rt_add_gq(kt);
		}
#else
		cpu_insert_localq(kt, cpu);
#endif

#ifdef RT_SCALE
		/* A high bit in cpu is used to indicate that
		 * that a cross-cpu interrupt should not be sent */
		if (cpu & RT_NO_DSPTCH)
			cpu &= RT_NO_DSPTCH_NOT;
		else
#else
		ASSERT(cpu == kt->k_onrq);
#endif
			if (!KT_ISNPRMPT(kt))
				dispatch_q(kt, cpu);
	}

	if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK|RTMON_TASKPROC)) {
		#pragma mips_frequency_hint NEVER
		uthread_t *ut = KT_ISUTHREAD(kt) ? KT_TO_UT(kt) : NULL;

		if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK) ||
		    (ut && (UT_TO_PROC(ut)->p_flag & SPARSWTCH)))
			log_tstamp_event(TSTAMP_EV_EODISP, kt->k_id,
					 RTMON_PACKPRI(kt, cpu),
					 ut ? UT_TO_VPROC(ut)->vp_pid : 0,
					 NULL);
	}
}

/* ARGSUSED */
void
kt_endtslice(kthread_t *kt)
{
	uthread_t	*ut = KT_TO_UT(kt);
	job_t		*j = ut->ut_job;

	ASSERT(KT_ISUTHREAD(kt));
	private.p_lticks = 0;
	if (j && j->j_type == JOB_WT && j->w_acctime < 0) {
		kt->k_basepri = PTIME_SHARE_OVER;
		if (!KT_ISPS(kt))
			kt->k_pri = PTIME_SHARE_OVER;
	}
#ifdef	NUMA_SCHED
	if (memaff_sched)
		cpu_numa_affinity(kt);
#endif
}

void
ut_endrun(uthread_t *ut)
{
	kthread_t	*kt = UT_TO_KT(ut);

	ASSERT(ut->ut_job || KT_ISPS(kt));
	ASSERT(kt_islocked(kt));
	if (is_bcritical(kt) || is_batch(kt))
		batch_thread_endrun(kt);
	else if (IS_SPROC(ut->ut_pproxy) && ut->ut_gstate != GANG_UNDEF &&
		ut->ut_gstate != GANG_NONE)
		gang_block(ut);
	if (!(ut->ut_rticks = private.p_lticks)) {
		job_t		*j = ut->ut_job;
		if (j && j->j_type == JOB_WT && j->w_acctime < 0) {
			kt->k_basepri = PTIME_SHARE_OVER;
			if (!KT_ISPS(kt))
				kt->k_pri = PTIME_SHARE_OVER;
		}
	} else if (!(kt->k_flags & KT_SLEEP))
		kt->k_preempt = 1;
	ASSERT(private.p_lticks >= 0);
	ASSERT(ut->ut_rticks <= ut->ut_tslice);
}

#ifdef DEBUG
void
idletest(kthread_t *kt, int index, int cpu)
{
	ASSERT(index < 1 ||  cpusets[index].cs_idler != cpu);
	ASSERT(cpusets[kt->k_cpuset].cs_idler != cpu ||
		!CPUMASK_TSTB(global_cpuset.cs_cpus, cpu));
}
#endif

void
kt_startrun(kthread_t *kt)
{
	cpuid_t cpu = private.p_cpuid;
	int idler = private.p_idler; 
	int s;

	ASSERT(kt->k_onrq == CPU_NONE);
	ASSERT(kt->k_sonproc == CPU_NONE);
	ASSERT(kt_islocked(kt));
	if (KT_ISUTHREAD(kt))  {
		uthread_t* ut = KT_TO_UT(kt);
		kt->k_flags &= ~(KT_SLEEP|KT_LTWAIT);
		private.p_lticks = ut->ut_rticks ? ut->ut_rticks : ut->ut_tslice;
	}	
	kt->k_preempt = 0;
	if (kt->k_lastrun != cpu)
		kt->k_lastrun = cpu;
	if (!KT_ISPS(kt) && KT_ISAFF(kt))
                kt->k_flags |= KT_HOLD;
	kt->k_sonproc = cpu;
	if (private.p_idler) {
		private.p_idler = 0;
		cpusets[idler].cs_idler = CPU_NONE;
	}
#ifdef DEBUG
	idletest(kt, idler, cpu);
#endif
	s = splprof();
	ktimer_switch(kt, AS_SYS_RUN);
	splx(s);

	private.p_curpri = kt->k_pri;
#ifdef RT_SCALE
	rt_setpri(kt->k_pri);
#endif
	kt_nested_unlock(kt);
#ifdef _SHAREII
	/*
	 * The thread has been removed from its cpu queue
	 * so we've done PCNTDN -- do a PCNTUP here, because 
	 * we're actually running, not sleeping or dead.
	 */
	if (KT_ISUTHREAD(kt) && KT_TO_UT(kt)->ut_policy == SCHED_TS)
		SHR_PCNTUP(KT_TO_UT(kt));
#endif /* _SHAREII */
}

#ifdef DEBUG
void
testidler(int index, int cpu)
{
	ASSERT(PRIMARY_IDLE != cpu || index);
}
#endif
int 
idler(void)
{
	int cpu = cpuid();
	int index = cpu_to_cpuset[cpu];
	int restricted = pdaindr[cpu].pda->p_cpu.c_restricted;

	if (restricted == 1)
		return 0;
#ifdef DEBUG
	testidler(private.p_idler, cpu);
#endif

	if (private.p_idler == index
		&& (!cpusets[index].cs_count
		    || PRIMARY_IDLE == CPU_NONE && !restricted))
	{
		private.p_intr_resumeidle = 0;
		cpusets[index].cs_idler = CPU_NONE;
		private.p_idler = 0;
		private.p_intr_resumeidle = 1;
	}
	if (!restricted && !private.p_idler && PRIMARY_IDLE == CPU_NONE) {
		private.p_intr_resumeidle = 0;
		if (compare_and_swap_int(&PRIMARY_IDLE, CPU_NONE, cpu))
			private.p_idler = 1;
		private.p_intr_resumeidle = 1;
	}
	/* don't try to setup idler for the cpuset if there is no process
	 * running in the cpuset. Otherwise, all the cpus in the set will
	 * try to do this, and it saturates the interconnect.
	 */
	if ((cpusets[index].cs_count>0) && !private.p_idler && 
	    (cpusets[index].cs_idler == CPU_NONE)) {
		private.p_intr_resumeidle = 0;
		if (compare_and_swap_int(&cpusets[index].cs_idler,
							CPU_NONE, cpu))
			private.p_idler = index;
		private.p_intr_resumeidle = 1;
	}
#ifdef DEBUG
	testidler(private.p_idler, cpu);
#endif
	return private.p_idler;
}

int
local_idle(void)
{
	cpu_t *cs	= &private.p_cpu;
#if MP
	if (cs->c_onq)
		cs->c_onq = 0;
	return !private.p_nextthread && !cs->c_threads
#ifdef RT_SCALE 
	 && !pNT[cpuid()][0]
#endif
	;
#else

	return !private.p_nextthread && !cs->c_threads;
#endif
}

int
idlerunq()
{
#if MP
	cpuid_t cpu;
#endif
	if (!local_idle()) 
		return 0;
	
	if (!idler())
		return 1;
#if MP
	for (cpu = 0; cpu < maxcpus; cpu++)
		if (!local_idle() || rt_gq || !batch_queue.b_empty ||
		    (pdaindr[cpu].pda && pdaindr[cpu].pda->p_cpu.c_onq))
			return 0;
#endif
	return 1;
}

static __inline kthread_t *
kt_register_wait(kthread_t *kt)
{
	int sqselfspin = 0;

	while (kt->k_sqself && kt != curthreadp) {
		if (sqselfspin++ > 10) {
			private.p_nextthread = 0;
			putrunq(kt, CPU_NONE);
			kt_nested_unlock(kt);
			return 0;
		}
		DELAY(1);
		SYNCHRONIZE();
	}
	return kt;
}

struct kthread *
getrunq()
{
	kthread_t	*kt;

	ASSERT(issplhi(getsr()));
	if (private.p_nextthread) {
		if (!private.p_runrun)
			return 0;
	} else {
		private.p_idletkn = 0;
	}

	do {
		private.p_runrun = 0;
		kt = private.p_frs_flags
			? frs_fsched_dispatch() : cpu_thread_search();
		if (!kt) {
			private.p_curpri = PIDLE;
#ifdef RT_SCALE
			rt_setpri(PIDLE);
#endif
			private.p_idletkn |= EMPTY;
			return 0;
		}
		ASSERT(private.p_nextthread != kt);
		ASSERT(kt->k_onrq == CPU_NONE);
		if (!compare_and_swap_kt(&private.p_nextthread, 0, kt))  {
			
			if (kt->k_pri < private.p_nextthread->k_pri) {
				putrunq(kt, CPU_NONE);
				kt_nested_unlock(kt);
				return 0;
			}
			kt_nested_lock(private.p_nextthread);
#ifdef DEBUG
			/* undo previous/aborted kt_startrun */
			kt->k_sonproc = CPU_NONE;
#endif
			putrunq(private.p_nextthread, CPU_NONE);
			kt_nested_unlock(private.p_nextthread);
			private.p_nextthread = kt;
		}
		ASSERT(issplhi(getsr()));
		kt = kt_register_wait(kt);
		if (!kt && curthreadp)
			return 0;
	} while (!kt);
	ASSERT(kt == curthreadp || !kt->k_sqself);
	kt_startrun(kt);
	return(kt);
}

int
removerunq(kthread_t *kt)
{
	int result = 0;
	ASSERT(kt_islocked(kt));
	if (kt->k_onrq == CPU_NONE) {
		return 0;
	}

	/* Check for >= maxcpus.  If it is equal to maxcpus, it is on
	 * the rt global q.  If it is greater, it is in one of the
         * per-cpu rt entries.  This is because a high bit along with
	 * the cpu num is stored in the k_onrq field. */
	if (kt->k_onrq >= maxcpus)
		result = rt_remove_q(kt);
	else 
		result = cpu_remove_localq(kt->k_onrq, kt);
	return result;
}

/*
 * Hook called by clock to determine if a process has used up its timeslice.
 * We assume that ut is the current process thread.
 * Can be called from any processor, at any time.
 */
void
tschkRunq(uthread_t *ut)
{
	if (private.p_flags & PDAF_NONPREEMPTIVE)
		return;
	if (ut->ut_policy == SCHED_FIFO)
		return;
	if (private.p_lticks > 1) {
		private.p_lticks--;
	} else {
		private.p_lticks = 0;
		private.p_runrun = 1;
	}
}

/*
 * A hook to provide cleanup processing for those that want it.
 */
void
exitrunq(kthread_t *kt)
{
	int		sl;
	job_t *j;
   
	ASSERT(KT_ISUTHREAD(kt));

	sl = kt_lock(kt);
	nested_spinlock(&cpuset_lock(kt));
	if (kt->k_cpuset > 1)
		(cpuset_count(kt))--;
	nested_spinunlock(&cpuset_lock(kt));

#ifdef _SHAREII
	/*
	 * One fewer thread to schedule....
	 */
	if (KT_TO_UT(kt)->ut_policy == SCHED_TS)
		SHR_PCNTDN(KT_TO_UT(kt));
#endif /* _SHAREII */

	if (is_bcritical(kt)) {
		j = KT_TO_UT(kt)->ut_job;
		/* 
		 * If someone is waiting pass the binding.
	 	 */
		if ((kt->k_binding != CPU_NONE) && (SV_WAITER(&j->j_sv)))
			batch_pass_binding(kt, &j->j_sv);

		/* 
		 * Now give up the binding, call this function
		 * even if you gave up the binding before because
		 * there is some associate clean up to be done.
		 */
		batch_unset_binding(kt);
	}
	if (KT_TO_UT(kt)->ut_job) 
		job_unlink_thread(kt);
	if (KT_ISBASEPS(kt))
		end_rt(kt);
	kt_unlock(kt, sl);

	job_free();
}

/*
 * Called to cleanup when a process leaves a share-group (through exec).
 */
void
leaveshaddrRunq(uthread_t *ut)
{
	int s;
	kthread_t *kt = UT_TO_KT(ut);

	if (!ut->ut_job) {
		ASSERT(KT_ISPS(kt));
		return;
	}

	if (IS_SPROC(ut->ut_pproxy))
		gang_unlink(kt);

	s = kt_lock(kt);
	if (is_batch(kt) || is_bcritical(kt)){
		kt_unlock(kt, s);
		return;
	}
	job_unlink_thread(kt);
	job_create(JOB_WT, kt);
	kt_unlock(kt, s);
}

void
uthreadrunq(kthread_t *ckt, int cpuset, proc_sched_t *prs)
{
	cpuset_t 	*cs;

	ckt->k_cpuset = cpuset;
	if (ckt->k_cpuset > 1) {
		cs = &cpusets[ckt->k_cpuset];
		nested_spinlock(&cs->cs_lock);
		cs->cs_count++;
		nested_spinunlock(&cs->cs_lock);
	}

	if (prs->prs_flags & PRSNOAFF)
		ckt->k_flags |= KT_NOAFF;
	else if (!KT_ISPS(ckt))
		ckt->k_flags |= KT_HOLD;
}

/*
 * A hook to propagate scheduling information across a fork.
 */
void
forkrunq(
	uthread_t *puth,
	uthread_t *cuth)
{
	proc_proxy_t *cproxy = cuth->ut_pproxy;
	proc_sched_t *csched = &cproxy->prxy_sched;
	proc_sched_t *psched = &puth->ut_pproxy->prxy_sched;
	kthread_t	*pkt = UT_TO_KT(puth);
	kthread_t	*ckt = UT_TO_KT(cuth);
	int		s;

	/*
	 * Caller's scheduling priority and bindings are only
	 * inherited through fork/sproc calls, so we set the
	 * kthread's scheduling bits here instead of uthread_attachrunq.
	 * Shouldn't need target kthread's kt_lock -- it hasn't been
	 * put on the runq yet.
	 */
	ASSERT(!issplhi(getsr()));
	job_allocate();
	s = kt_lock(pkt);

	ckt->k_flags = pkt->k_flags & (KT_BASEPS|KT_NPRMPT|KT_NBASEPRMPT|KT_BIND);
	kt_initialize_pri(ckt, kt_basepri(pkt));

	if (ckt->k_flags & KT_BASEPS) 
		ckt->k_flags |= KT_PS;

	csched->prs_nice = psched->prs_nice;

	ASSERT(csched->prs_job == 0);
	if (psched->prs_flags & PRSNOAFF) 
		csched->prs_flags |= PRSNOAFF;

	if (psched->prs_flags & PRSBATCH)
		csched->prs_flags |= PRSBATCH;
	uthreadrunq(ckt, pkt->k_cpuset, psched);
	uthread_attachrunq(UT_TO_KT(cuth),
	    puth->ut_flags & UT_NOMRUNINH ? PDA_RUNANYWHERE : pkt->k_mustrun,
	    ((cproxy->prxy_shmask & (PR_SPROC|PR_THREADS)) ||
	     psched->prs_flags & PRSBATCH) ? puth->ut_job: 0);
	kt_unlock(pkt, s);
}

void
uthread_attachrunq(
	kthread_t *	ckt,
	cpuid_t		mustrun,
	job_t *		pjob
	)
{
	int s;


	s = kt_lock(ckt);

	ckt->k_lastrun = private.p_cpuid;
	ckt->k_mustrun = mustrun;

	if (KT_ISPS(ckt)) {
		if (ckt->k_flags & KT_BIND)
			start_rt(ckt, ckt->k_pri);
	} else if (pjob) {
		job_attach_thread(pjob, ckt);
	} else {
		(void) job_create(JOB_WT, ckt);
	}

	kt_unlock(ckt, s);
}

#ifdef MP
#define INIT_COOKIE(cookie) cookie.must_run = 0; cookie.cpu = PDA_RUNANYWHERE
#define IS_MR(cookie) (cookie.must_run)
#define SET_MR(cookie) cookie.must_run = 1
#define SET_CPU(cookie,newcpu) cookie.cpu = newcpu 
#define GET_CPU(cookie) (cookie.cpu)
cpu_cookie_t
setmustrun(cpuid_t newcpuid)
{
	register cpuid_t was_running;
	register cpu_cookie_t cookie;
	int s;
	INIT_COOKIE(cookie);
	ASSERT(curthreadp);
	ASSERT(cpu_running(newcpuid));
	ASSERT(!KT_ISUTHREAD(curthreadp) || curthreadp == UT_TO_KT(curuthread));
	s = kt_lock(curthreadp);
	was_running = curthreadp->k_mustrun;
	curthreadp->k_mustrun = newcpuid;
	if (KT_ISPS(curthreadp) && curthreadp->k_binding != CPU_NONE &&
		curthreadp->k_mustrun != curthreadp->k_binding)
		rt_rebind(curthreadp);
	if (curthreadp->k_flags & KT_PSMR)
		SET_MR(cookie);
	else
		curthreadp->k_flags |= KT_PSMR;
	kt_unlock(curthreadp, s);
	if (private.p_cpuid != newcpuid)
		qswtch(MUSTRUNCPU);
	SET_CPU(cookie, was_running);
	return(cookie);
}

void
restoremustrun(cpu_cookie_t cookie)
{
	int s;
	cpuid_t runon = GET_CPU(cookie);
	ASSERT(curthreadp);
	ASSERT(!KT_ISUTHREAD(curthreadp) || curthreadp == UT_TO_KT(curuthread));
	s = kt_lock(curthreadp);
	if (!IS_MR(cookie))
		curthreadp->k_flags &= ~KT_PSMR;
	curthreadp->k_mustrun = runon;
	if (KT_ISPS(curthreadp) && curthreadp->k_mustrun != runon)
		rt_rebind(curthreadp);
	kt_unlock(curthreadp, s);
	if (runon != PDA_RUNANYWHERE) {
		if (runon != private.p_cpuid)
		       qswtch(MUSTRUNCPU);
	} else if (!(private.p_flags & PDAF_ENABLED)) {
		/* This processor is restricted */
		qswtch(MUSTRUNCPU);
	}
}
#endif

/*
 * Return the number of processors available to the given process. For now,
 * we ignore the fact that the scheduling queues may also restrict where
 * a process can run, since that is an externally imposed restriction.
 */
int
runprunq(kthread_t *kt) 
{
	int i = 0;
	int retval = 0;

	if (KT_ISMR(kt))
		return 1;

	for (i = 0; i < maxcpus; i++) {
		/* test cpu_enabled first, since if the cpu is not
		 * enabled, no pda is allocated
		 */
		if (!cpu_enabled(i))
			continue;
		if (cpu_restricted(i))
			continue;
		if (cpuset_runok(kt->k_cpuset, cpu_to_cpuset[i]))
			retval++;
	}
	return retval;
}

/*
 * Insure that the given process kthread takes a trip through the
 * trap handler to check for kernel generated events.
 * May interrupt another CPU if the process thread is active.
 */
void
evpostRunq(kthread_t *kt)
{
   int	cpu;

	if (kt == curthreadp)
		private.p_runrun = 1;
	else if ((cpu = kt->k_sonproc) >= 0)
		SEND_VA_FORCE_RESCHED(cpu)
	else
		private.p_runrun = 1;
}

/*
 * Old pri of 38 must be above all system threads (but not interrupts)
 * for backward compatibility.  Therefore all system threads must have
 * new priorities below 110 (and above or equal to 90).
 */
int rtpri[] = {118, 117, 116, 115, 114, 113, 112, 111, 110, 90};

/*
 * Convert a kthread priority to an old-style schedctl() rtpri priority.
 */
int
kt_rtpri(kthread_t *kt)
{
	kpri_t		ktpri;
	int		ii;

	/* There are four classes of scheduling, according to schedctl(),
	 * and here are their associated kthread priorities:
	 *
	 *    HI range:  SCHED_RR round-robin realtime
	 *  NORM range:  SCHED_NP non-degrading priority
	 *    LO range:  SCHED_RR weightless
	 *           0:  SCHED_TS timeshare
	 */

	if (KT_ISBASEPS(kt)) {
		ktpri = kt_basepri(kt);

		/* LO range */
		if (ktpri <= PWEIGHTLESS)
			return PWEIGHTLESS + NDPLOMAX - ktpri;

		/* HI range */
		for (ii = 0; ii < 10; ii++) {
			if (ktpri == rtpri[ii])
				return (NDPHIMAX + ii);
		}

		/* NORM range */
		return INVERT_PRI(ktpri);
	}

	/* Timeshare */
	return 0;
}

/*
 * System calls that change scheduling parameters call this hook.
 */
int
setinfoRunq(kthread_t *kt, proc_sched_t *prs, int a1, ...)
{
	va_list ap;
	int s, dequeued = 0;
	kpri_t pri;
	int rval;

	ASSERT(a1 == RQSLICE || a1 == RQRTPRI);

	va_start(ap, a1);
	if (a1 == RQSLICE) {
		rval = KT_TO_UT(kt)->ut_tslice;
		KT_TO_UT(kt)->ut_tslice = va_arg(ap, int);
		return rval;
	}

	if (a1 != RQRTPRI)
		return 0;

	pri = va_arg(ap, int);

	/*
	 * Gang threads which are changing to realtime must be removed
	 * from the gang.
	 */
	if (pri && IS_SPROC(KT_TO_UT(kt)->ut_pproxy))
		gang_unlink(kt);

retry:
	s = kt_lock(kt);
	if (kt->k_onrq != CPU_NONE)
		if (removerunq(kt))
			dequeued++;
		else {
			kt_unlock(kt, s);
			goto retry;
		}

	rval = kt_rtpri(kt);

	if (pri == 0) {
		if (KT_ISBASEPS(kt)) {
			/*
			 * Downgrading from realtime to normal priority,
			 * make process credit scheduled.
			 * FIX: if sproc, do we join parent's job?
			 */
			kt->k_flags &= ~(KT_BASEPS|KT_PS|KT_BIND);
			kt->k_flags &= ~(KT_NPRMPT|KT_NBASEPRMPT);
			(void) job_create(JOB_WT, kt);
			end_rt(kt);
			KT_TO_UT(kt)->ut_policy = SCHED_TS;
			kt_initialize_pri(kt, PTIME_SHARE);
		}
		else {
			ASSERT(KT_TO_UT(kt)->ut_job);
			ASSERT(KT_TO_UT(kt)->ut_policy == SCHED_TS);
			wt_set_weight(kt, prs->prs_nice);
		}
	}
	else {
		if (!KT_ISBASEPS(kt)) {
			ASSERT(KT_TO_UT(kt)->ut_job);
			ASSERT(KT_TO_UT(kt)->ut_policy == SCHED_TS);
			job_unlink_thread(kt);
			kt->k_flags |= KT_BASEPS|KT_PS;
		}
		ASSERT(KT_TO_UT(kt)->ut_job == 0);
		if (pri >= NDPHIMAX && pri <= NDPHIMIN) {
			/* hi band: bind it */
			KT_TO_UT(kt)->ut_policy = SCHED_RR;
			kt_initialize_pri(kt, rtpri[pri-NDPHIMAX]);
			kt->k_flags &= ~(KT_NPRMPT|KT_NBASEPRMPT);
			if (!(kt->k_flags & KT_BIND)) {
				kt->k_flags |= KT_BIND;
				start_rt(kt, kt->k_pri);
			} else 
				redo_rt(kt, kt->k_pri);	
		} else if (pri >= NDPNORMMAX && pri <= NDPNORMMIN) {
			/* norm band: no binding */
			KT_TO_UT(kt)->ut_policy = SCHED_NP;
			kt_initialize_pri(kt, INVERT_PRI(pri));
			kt->k_flags |= KT_NPRMPT|KT_NBASEPRMPT;
			kt->k_flags &= ~KT_BIND;
			end_rt(kt);
		} else {
			/* weightless */
			ASSERT(pri >= NDPLOMAX && pri <= NDPLOMIN);
			KT_TO_UT(kt)->ut_policy = SCHED_RR;
			kt_initialize_pri(kt, PWEIGHTLESS + NDPLOMAX - pri);
			kt->k_flags &= ~(KT_BIND|KT_NPRMPT|KT_NBASEPRMPT);
			end_rt(kt);
		}
	}

	if (dequeued) 
		putrunq(kt, CPU_NONE);
	kt_unlock(kt, s);
	return rval;
}
