/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/space.h>
#include <sys/runq.h>
#include <sys/debug.h>
#include <sys/rtmon.h>
#include "utility.h"
#include "rt.h"
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/schedctl.h>
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#include <sys/sched.h>	/* SCHED_TS */
#endif /* _SHAREII */

extern int	maxcpus;
extern batchq_t	batch_queue;
#ifdef RT_SCALE 
extern kthread_t **pNT[];
#endif

#ifdef MP
INLINE static kpri_t
rt_gethigh(void)
{
#ifdef RT_SCALE
/* We must check the rt array (one entry per cpu) to see if
 * there is a thread waiting there.  If there is, return its
 * priority.  Otherwise, look at rt_gq.
 */
	register kthread_t *kt = pNT[cpuid()][0];
	if(!kt)
		kt = rt_gq;
#else
	register kthread_t *kt = rt_gq;
#endif
	return (!kt) ? PIDLE : kt->k_pri;
}

static void
cpu_clean_localq(cpu_t *cs)
{
	kthread_t *kt, *nkt;
	kthread_t *ckt = 0;
	int onq = 0;
	
	ASSERT(issplhi(getsr()));
	nested_spinlock(&cs->c_lock);
	while (kt = cs->c_threads) {
		if (KT_ISLOCAL(kt))
			break;
		if (KT_ISHELD(kt))
			kt_fclr(kt, KT_HOLD);
		if (!KT_ISPS(kt)) {
			onq = 1;
			break;
		}
		kt_nested_lock(kt);
		ASSERT(KT_ISPS(kt) && !KT_ISLOCAL(kt));
		if (kt == kt->k_rflink) {
			cs->c_threads = 0;
		} else {
			kt->k_rflink->k_rblink = kt->k_rblink;
			kt->k_rblink->k_rflink = kt->k_rflink;
			cs->c_threads = kt->k_rflink;
		}
		kt->k_rflink = ckt;
		ckt = kt;
	}
	if (kt) {
		nkt = kt->k_rflink;
		while (kt = nkt, kt != cs->c_threads) {
			nkt = kt->k_rflink;
			if (KT_ISLOCAL(kt))
				continue;
			if (KT_ISHELD(kt))
				kt_fclr(kt, KT_HOLD);
			if (!KT_ISPS(kt)) {
				onq = 1;
				continue;
			}
			kt_nested_lock(kt);
			ASSERT(KT_ISPS(kt) && !KT_ISLOCAL(kt));
			kt->k_rflink->k_rblink = kt->k_rblink;
			kt->k_rblink->k_rflink = kt->k_rflink;
			kt->k_rflink = ckt;
			ckt = kt;
		}
	}
	if (cs->c_onq != onq)
		cs->c_onq = onq;
	nested_spinunlock(&cs->c_lock);

	while (ckt) {
		kt = ckt;
		ckt = ckt->k_rflink;
		kt->k_rflink = kt;
		kt->k_rblink = kt;
		kt->k_onrq = CPU_NONE;
		kt->k_lastrun = CPU_NONE;
		putrunq(kt, CPU_NONE);
		kt_nested_unlock(kt);
	}
}

static kthread_t *
cpu_select_rq(cpu_t *cs, cpu_t *fromcs)
{
	kthread_t	*kt;
	int		cpuset;
	int		rest;
	int		onq;

	ASSERT(issplhi(getsr()));
	if (fromcs == cs || !cs->c_threads || !cs->c_onq ||
	    cs->c_restricted == 1 || (!nested_spintrylock(&cs->c_lock) &&
	    (!private.p_idler || (nested_spinlock(&cs->c_lock), 0))))
		return 0;

	onq = 0;
	kt = cs->c_threads;
	if (kt && kt->k_pri > rt_gethigh()) {
		cpuset = cpu_to_cpuset[cpuid()];
		rest = fromcs->c_restricted;
		do {
			if (/*KT_ISHELD(kt) || */KT_ISLOCAL(kt))
				continue;
			onq = 1;
			ASSERT(!KT_ISMR(kt));
			kt_nested_lock(kt);
			if (runcond_ok(kt) &&  
			    (!rest && kt->k_cpuset == CPUSET_GLOBAL ||
			    kt->k_cpuset == cpuset)) {

				kt->k_rflink->k_rblink = kt->k_rblink;
				kt->k_rblink->k_rflink = kt->k_rflink;
				if (cs->c_threads == kt)
					cs->c_threads = kt->k_rflink == kt
						? 0 : kt->k_rflink;
				kt->k_rblink = kt->k_rflink = kt;
				nested_spinunlock(&cs->c_lock);
				return kt; /* return kt locked */
			}
			kt_nested_unlock(kt);
		} while (kt = kt->k_rflink, kt != cs->c_threads);
	}
	if (!onq)
		cs->c_onq = 0;
	nested_spinunlock(&cs->c_lock);
	return 0;
}

static kthread_t *
peek_time_share(cpu_t *fcs, kpri_t pri)
{
	cpuid_t cpu;
	kthread_t *kt;
	cpu_t *cs;

	if (fcs->c_restricted == 1)
		return 0;

	if ((cpu = fcs->c_peekrotor) == cpuid() || !cpu_running(cpu) ||
	    (cs = &pdaindr[cpu].pda->p_cpu, cs->c_restricted == 1) ||
	    (kt = cs->c_threads, !kt || kt->k_pri <= pri) ||
	    (kt = cpu_select_rq(cs, fcs)) == 0) {
		fcs->c_peekrotor = (cpu < maxcpus - 1) ? cpu + 1 : 0;
		return 0;
	}

	return kt;
}

#ifdef NUMA_SCHED
static void *
cpu_eval_target(cpuid_t tcpu, void *arg)
{
	kthread_t *tkt;
	kthread_t *kt = (kthread_t *)arg;
	pda_t *tpda = pdaindr[tcpu].pda;

	ASSERT(tcpu >= 0 && tcpu < maxcpus);
	if (!cpu_running(tcpu) || tpda->p_cpu.c_restricted == 1 ||
	    !cpuset_runok(kt->k_cpuset, cpu_to_cpuset[tcpu]))
		return 0;

	if (tcpu == kt->k_lastrun)
		return (void *)tpda;

	tkt = tpda->p_curkthread;

	if (!tkt || !KT_ISUTHREAD(tkt) || tkt == kt ||
		 (tpda->p_curpri <= kt->k_pri && 
		   memaff_getaff(tkt, cputocnode(tcpu)) <
			   memaff_getaff(kt, cputocnode(tcpu))))
	{
		return (void *)tpda;
	}
	return 0;
}

void
cpu_numa_affinity(kthread_t *kt)
{
	pda_t *pda;

	if (KT_ISMR(kt) || KT_ISKB(kt))
		return;
	pda = (pda_t *) memaff_getbestnode(kt, cpu_eval_target, kt, 0);
	if (pda && !pda->p_cpu.c_threads &&
			CPUID_TO_COMPACT_NODEID(kt->k_lastrun) != 
				CPUID_TO_COMPACT_NODEID(pda->p_cpuid)) {
		kt->k_lastrun = pda->p_cpuid;
		if (!KT_ISPS(kt) && KT_ISAFF(kt))
			kt->k_flags |= KT_HOLD;
	}
}

/*ARGSUSED*/
static void * 
cpu_selector(cnodeid_t cnode, void *cpu, void *arg2)
{
	cpuid_t base, i;
	kthread_t *kt;

	/* There are nodes with no cpus. */
	if (CNODE_NUM_CPUS(cnode) == 0) 
		return NULL;  
	base = CNODE_TO_CPU_BASE(cnode);
	for (i = base; i < base + CNODE_NUM_CPUS(cnode); i++)
		if (cpu_running(i) && (kt = cpu_select_rq(CPU_QUEUE(i), cpu)))
			return kt;
	return NULL;
}
#else  /* NUMA_SCHED */
#define physmem_select_neighbor_node(c, p, n, s, p0, p1) 0
#endif /* NUMA_SCHED */

static kthread_t *
global_find_work(cpu_t *cs)
{
        kthread_t *kt;
        cpuid_t cpu, index, base;
	cpuid_t curcpu;

	ASSERT(private.p_idler && cs->c_restricted != 1);
	curcpu = cpuid();
	base = CNODE_TO_CPU_BASE(cnodeid());
	for (index = 2; index < CPUS_PER_NODE + 1; index++) {
		cpu = curcpu + ((index & 1) ? index >> 1 : -(index >> 1));
		if (cpu < base)
			cpu += CPUS_PER_NODE;
		else if (cpu >= base + CPUS_PER_NODE)
			cpu -= CPUS_PER_NODE;
		if (cpu_running(cpu) && cpu_restricted(cpu) != 1 &&
		    (kt = cpu_select_rq(CPU_QUEUE(cpu), cs)))
			return kt;
	}
	kt = physmem_select_neighbor_node(cnodeid(), physmem_max_radius, 0,
					  cpu_selector, (void*)cs, 0);

	return kt;
}
#else  /* MP */
#define global_find_work(cs)	0
#endif /* MP */

static void
cpu_resort(cpu_t *cs)
{
	kthread_t	*kt;
	kthread_t	*nkt;
#ifdef DEBUG
	int		count = 10000000;
#endif
	ASSERT(issplhi(getsr()));
	if (!cs->c_threads) {
#if MP
		if (cs->c_onq)
			cs->c_onq = 0;
#endif
		return;
	}
#if MP
	else
		cpu_clean_localq(cs);
#endif

	nested_spinlock(&cs->c_lock);
	if (!cs->c_threads) {
#if MP
		if (cs->c_onq)
			cs->c_onq = 0;
#endif
		nested_spinunlock(&cs->c_lock);
		return;
	}

	kt = cs->c_threads;
	nkt = kt->k_rflink;
	for (; nkt != cs->c_threads; nkt = kt->k_rflink) {

		kthread_t	*ikt;
		int		qmore;

		ASSERT( count-- );
		if (nkt->k_pri <= kt->k_pri) {
			kt = nkt;
			continue;
		}

		ikt = kt;
		nkt->k_rflink->k_rblink = nkt->k_rblink;
		nkt->k_rblink->k_rflink = nkt->k_rflink;

		do {
			ASSERT( count-- );
			qmore = (ikt != cs->c_threads) ||
				(cs->c_threads = nkt, 0);
			ikt = ikt->k_rblink;
		} while (qmore && nkt->k_pri > ikt->k_pri);

		nkt->k_rblink = ikt;
		nkt->k_rflink = ikt->k_rflink;
		ikt->k_rflink->k_rblink = nkt;
		ikt->k_rflink = nkt;
	}
	if (cs->c_threads->k_pri > private.p_curpri &&
	    !KT_ISNPRMPT(cs->c_threads))
		private.p_runrun = 1;
	nested_spinunlock(&cs->c_lock);
}

void
cpu_sched_tick(kthread_t *kt)
{
	uthread_t	*ut = ((kt && KT_ISUTHREAD(kt)) ? KT_TO_UT(kt) : NULL);
	cpu_t		*cs = &private.p_cpu;

        if (ut && ut->ut_job) {
		kt_nested_lock(kt);
		if (ut->ut_job)
			ut->ut_job->j_ops.jo_run_time(kt);
		kt_nested_unlock(kt);
	}
	if (cpuid() == masterpda->p_cpuid)
		gang_sched_tick();
	wtree_sched_tick();
	cpu_resort(cs);
}

INLINE static kthread_t *
cpu_select_q(cpu_t *cs)
{
	kthread_t *kt;
#if MP
	kpri_t gqpri;
#endif
	ASSERT(issplhi(getsr()));
	if (!cs->c_threads)
#if MP
		return 0;
#else
		return 0;
#endif

#if MP
	gqpri = cs->c_restricted == 1 ? PIDLE : rt_gethigh();
	nested_spinlock(&cs->c_lock);
	kt = cs->c_threads;
        if (!kt) {
            nested_spinunlock(&cs->c_lock);
            return 0;
        }
        /* If there is a higher priority thread on the global q, get it.
         * rt_thread_select() could still return 0 if no threads on the
         * global q pass the run condtions test.  If that happens, we
         * continue looking on the local q */
        if (gqpri > kt->k_pri ||
            (gqpri == kt->k_pri && (rt_dither++ & 1))) {
                ASSERT(gqpri != PIDLE);
                nested_spinunlock(&cs->c_lock);
                if(kt = rt_thread_select())
                    return kt;
                else {
                    nested_spinlock(&cs->c_lock);
                    if (!(kt = cs->c_threads)) {
                        nested_spinunlock(&cs->c_lock);
                        return 0;
                    }
                }
	}
	ASSERT(spinlock_islocked(&cs->c_lock));
	if (kt->k_pri <= PTIME_SHARE_OVER) {
		nested_spinunlock(&cs->c_lock);
		if (kt = peek_time_share(cs, kt->k_pri))
			return kt;
		nested_spinlock(&cs->c_lock);
		if (!(kt = cs->c_threads) || gqpri > kt->k_pri) {
			nested_spinunlock(&cs->c_lock);
			return 0;
		}
	}
#else  /* MP */
	nested_spinlock(&cs->c_lock);
	if (!(kt = cs->c_threads)) {
		nested_spinunlock(&cs->c_lock);
		return 0;
	}
#endif /* MP */
	ASSERT(spinlock_islocked(&cs->c_lock) && kt);
	do {
		/*REFERENCED*/
		kthread_t *okt;

		kt_nested_lock(kt);
		if (runcond_ok(kt)) {
			kt->k_rflink->k_rblink = kt->k_rblink;
			kt->k_rblink->k_rflink = kt->k_rflink;
			if (cs->c_threads == kt)
				cs->c_threads = kt->k_rflink == kt ?
						0 : kt->k_rflink;
			kt->k_rblink = kt->k_rflink = kt;
			nested_spinunlock(&cs->c_lock);
			return kt; /* return kt locked */
		}
		okt = kt;
		kt = kt->k_rflink;
		kt_nested_unlock(okt);
	} while (kt != cs->c_threads);

	nested_spinunlock(&cs->c_lock);
	return 0;
}

/*
 * Find a suitable thread to run: check local q, if not pick from vmp(s).
 */
struct kthread *
cpu_thread_search(void)
{
	kthread_t *kt;
	cpu_t *cs = &private.p_cpu;

	if (!(kt = cpu_select_q(cs)) && cs->c_restricted != 1 &&
	    !(kt = rt_thread_select()) && private.p_idler)
		kt = global_find_work(cs);

	if ((!kt || kt->k_pri < PBATCH) && !batch_queue.b_empty)  {
		if (batch_generate_work() && kt) {
			putrunq(kt, kt->k_onrq = CPU_NONE);
			kt_nested_unlock(kt);
			return 0;
		}
	}
	ASSERT(!kt || kt_islocked(kt));
	if (kt)
		kt->k_onrq = CPU_NONE;

	return kt;
}

void
cpu_insert_localq(kthread_t *kt, cpuid_t cpu)
{
	cpu_t		*cs = &pdaindr[cpu].pda->p_cpu;
	kthread_t	*ikt;
	short		pri;

	ASSERT(kt_islocked(kt) && kt->k_wchan == 0);
	ASSERT(kt == kt->k_rflink && kt == kt->k_rblink);
	ASSERT(!KT_ISMR(kt) || kt->k_mustrun == cpu);

	pri = kt->k_preempt ? kt->k_pri + 1 : kt->k_pri;
	nested_spinlock(&cs->c_lock);
	if (ikt = cs->c_threads) {
		do {
			if (pri > ikt->k_pri)
				break;
			ikt = ikt->k_rflink;
		} while (ikt != cs->c_threads);
		kt->k_rflink = ikt;
		kt->k_rblink = ikt->k_rblink;
		kt->k_rblink->k_rflink = kt;
		ikt->k_rblink = kt;
		if (ikt == cs->c_threads && pri > ikt->k_pri)
			cs->c_threads = kt;
	} else {
		cs->c_threads = kt;
	}
#if MP
	if (!KT_ISHELD(kt) && !KT_ISLOCAL(kt) && !cs->c_onq)
		cs->c_onq = 1;
#endif
	kt->k_onrq = cpu;
	nested_spinunlock(&cs->c_lock);
#ifdef _SHAREII
	if (KT_ISUTHREAD(kt) && KT_TO_UT(kt)->ut_policy == SCHED_TS)
		SHR_PCNTUP(KT_TO_UT(kt));
#endif /* _SHAREII */
}

int
cpu_remove_localq(cpuid_t cpu, kthread_t *kt)
{
	cpu_t *cs = &pdaindr[cpu].pda->p_cpu;

	if (!nested_spintrylock(&cs->c_lock))
		return 0;
	kt->k_rflink->k_rblink = kt->k_rblink;
	kt->k_rblink->k_rflink = kt->k_rflink;
	if (cs->c_threads == kt)
		cs->c_threads = kt->k_rflink == kt ? 0 : kt->k_rflink;
	kt->k_rblink = kt->k_rflink = kt;
	kt->k_onrq = CPU_NONE;
	nested_spinunlock(&cs->c_lock);
#ifdef _SHAREII
	if (KT_ISUTHREAD(kt) && KT_TO_UT(kt)->ut_policy == SCHED_TS)
		SHR_PCNTDN(KT_TO_UT(kt));
#endif /* _SHAREII */
	return 1;
}

static void
cpu_purge_localq(cpu_t *cs)
{
	kthread_t *kt, *nkt;
	kthread_t *ckt = 0;
	
	ASSERT(issplhi(getsr()));
	while (kt = cs->c_threads) {
		if (KT_ISKB(kt) || KT_ISMR(kt))
			break;
		kt_nested_lock(kt);
		ASSERT(!KT_ISKB(kt) && !KT_ISMR(kt));
		kt->k_flags &= ~KT_HOLD;
		if (kt == kt->k_rflink) {
			cs->c_threads = 0;
		} else {
			kt->k_rflink->k_rblink = kt->k_rblink;
			kt->k_rblink->k_rflink = kt->k_rflink;
			cs->c_threads = kt->k_rflink;
		}
		kt->k_rflink = ckt;
		ckt = kt;
	}
	if (kt) {
		nkt = kt->k_rflink;
		while (kt = nkt, kt != cs->c_threads) {
			nkt = kt->k_rflink;
			if (KT_ISKB(kt) || KT_ISMR(kt))
				break;
			kt_nested_lock(kt);
			ASSERT(!KT_ISKB(kt) && !KT_ISMR(kt));
			kt->k_flags &= ~KT_HOLD;
			kt->k_rflink->k_rblink = kt->k_rblink;
			kt->k_rblink->k_rflink = kt->k_rflink;
			kt->k_rflink = ckt;
			ckt = kt;
		}
	}
	while (ckt) {
		kt = ckt;
		ckt = ckt->k_rflink;
		kt->k_rflink = kt;
		kt->k_rblink = kt;
		kt->k_onrq = CPU_NONE;
		kt->k_lastrun = CPU_NONE;
		putrunq(kt, CPU_NONE);
		kt_nested_unlock(kt);
	}
}

/*
 * Scan the local cpu queue for the frame scheduler and 
 * check if the specified kthread is in cs ready to run.
 *
 * Returns: 1 if kt is ready to run and 0 if not.
 */
int
cpu_frs_dequeue(cpu_t *cs, kthread_t *kt)
{
	ASSERT (issplhi(getsr()));

	kt_nested_lock(kt);
	if (kt->k_onrq == cpuid() && runcond_ok(kt)) {

		if (!(kt->k_flink == kt && kt->k_blink == kt)) {
			/*
			 * The thread is runnable, but on a wait queue.
			 *
			 * This means the thread is stuck attempting to
			 * acquire an unavailabe mutex or mrlock, but never
			 * really blocks because it is trying to boost the
			 * owners priority. Skip it.
			 *
			 * XXX If the thread is alone on the wait queue
			 *     we won't get here. 
			 */
			kt_nested_unlock(kt);
			return 0;
		}

		nested_spinlock(&cs->c_lock);

		kt->k_rflink->k_rblink = kt->k_rblink;
		kt->k_rblink->k_rflink = kt->k_rflink;
		if (cs->c_threads == kt)
			cs->c_threads = kt->k_rflink == kt ? 0 : kt->k_rflink;
		kt->k_rblink = kt->k_rflink = kt;

		ASSERT(kt->k_onrq == cpuid());
		kt->k_onrq = CPU_NONE;

		nested_spinunlock(&cs->c_lock);
		/* return with thread locked */
		return 1;
	}

	kt_nested_unlock(kt);
	return 0;
}

void
cpu_restrict(cpuid_t cpu, int cpuset)
{
	cpu_t *cs = &pdaindr[cpu].pda->p_cpu;
		
	int s = splhi();
	nested_spinlock(&cs->c_lock);
	cs->c_restricted = cpuset;
	nested_spinunlock(&cs->c_lock);
	rt_restrict(cpu);
	DELAY(100);
	nested_spinlock(&cs->c_lock);
	cpu_purge_localq(cs);
	nested_spinunlock(&cs->c_lock);
	splx(s);
}

void
cpu_unrestrict(cpuid_t cpu)
{
	cpu_t *cs = &pdaindr[cpu].pda->p_cpu;
	int s = mutex_spinlock(&cs->c_lock);
	cs->c_restricted = 0;
	rt_unrestrict(cpu);
	mutex_spinunlock(&cs->c_lock, s);
}

void
init_cpus(void)
{
	int i;
	cpu_t *cpu;

	for (i = 0; i < maxcpus; i++) {
		if (cpu_enabled(i)) {
			cpu = &pdaindr[i].pda->p_cpu;
			cpu->c_threads = 0;
			spinlock_init(&cpu->c_lock, "cpu_lock");
#if MP
			cpu->c_peekrotor = i;
			cpu->c_onq = 0;
#endif
		}
	}
}
