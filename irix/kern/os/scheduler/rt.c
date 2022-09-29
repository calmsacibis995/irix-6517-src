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

/**************************************************************************

Notes on modifications made to the rt scheduler in 6.5.5.  These changes
correspond with BUG #615746.  The basic premise of these changes is to
make sure that if you have n processors, then the n highest priority
rt threads should be running, and these threads should start running
with as little latency as possible.  We guarantee a latency of 1 ms.
The problem that led to these changes is due to the rt threads being
placed on the local queues in the previous scheduler.  In this scenario,
a high priority thread could get stuck behind a higher priority thread
on a local q.  Another cpu could only "notice" this thread when a tick
happens, which is once every 10 ms.  Hence, we could not guarantee a
1 ms latency.

There are 2 basic parts to the solution that was implemented: 1) the "push"
and 2) the "pull".  "Pushing" deals with making a high-priority thread
run as soon as it is able to run, assuming it is one of the n highest
priority threads in the system.  We try to push the thread on a cpu
running the lowest priority thread.  "Pulling" deals with being able
to select the next highest priority thread that is not running when
a high priority thread that is running is no longer able to run.

The main new data structure added is an array.  The is an array of kthreads,
with one entry per cpu.  Each entry in the array has it's own cache line.
Currently this is implemented by creating a 2D array, with each element
of the array being an entire cacheline.  The actual entry is then in array
index pNT[cpu][0] (i.e., the cpu specifies the entry in the array, and the
secondary index is always 0).  (pNT stands for possible next thread.)
The global rt q still exists.  When there are more rt threads than cpus,
the other threads go on the global rt q.  (Note that mustrun and bound
kthreads are still placed on local queues.  We might be able eliminate
bindings when using this new rt scheduler...)

There is also another new array.  This is an array of priorities.  This
array has 1 entry per cpu.  It holds either the priority of the thread
currently running on the cpu, or the priority of the thread in the per-cpu
array.  It is this array that is used by rt_add_gq() to determine if it
should preempt another thread because it has a lower priority.  This array
is protected by the gqlock (as is rt_gq).  The name of this array is
pNTpri.  The synchonization of pNT is different from pNTpri.  pNT is
protected by atomic operations.  This was done to eliminate the need
for getting the gqlock when getting the thread from the run q.
NOTE: pNTpri entries take the value PIDLE for all non-rt threads.

Most of the work is done in rt_add_gq().  This is called from putrunq().
putrunq() calls rt_add_gq() for all rt threads that do not go on the local
q (see note above).  rt_add_gq() first attempts to find an idle cpu.  If
one cannot be found, it searches for the cpu with the lowest priority
thread.  It then places the kthread on the per-cpu rt array for the
cpu it found, and returns this cpu.  putrunq() will then send a cross-cpu
interrupt to this cpu, forcing the new thread to run.  If the thread passed
to rt_add_gq() is of lower priority than all other running threads, the
thread is placed on rt_gq, and no cross-cpu interrupt is sent.  For more
details, see rt_add_gq().

The putting of threads on the per-cpu rt array (pNT) and sending cross-cpu
interrupts implements the "push" objective.  Placing threads on rt_gq if
higher priority threads are running on all cpus accomplishes the "pull"
objective.  This can be seen in rt_thread_select().  This function is called
when a cpu is looking for a new thread to run.  If there is a thread in its
pNT entry, it will attempt to get that one.  Otherwise, it searches rt_gq.
See rt_thread_select() for more details.

The function rt_setpri() is used to keep pNTpri up to date with what is
happening in the system.  pNTpri is set when threads are placed in pNT.
However, it needs to be reset when other threads start running, such as
non-rt threads and rt threads that are on the local q.  rt_setpri() is
used for this purpose.  It is called wherever p_curpri is updated.  See
rt_setpri() for more details.

The compile constant RT_SCALE is used to compile in/out the new rt scheduler
changes.  At the time of this writing, is has been included in kcommondefs
for IP27 and IP30.

rtcpus is a new systune.  rtcpus is used to determine whether to use the
new rt scheduler.  Use of this scheduler can cause a performance impact on
larger cpu count machines.  The new rt scheduler is only used if the number
of cpus on the machine is <= rtcpus.  Note that this is a run-time check,
as opposed to RT_SCALE.  The kernel will ship with rtcpus set at some default
value (4 as of 6.5.5), but the user will have the option of changing it
if rt latency is more important than general system scalability.

****************************************************************************/

#include <sys/kthread.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include "frame/frame_state.h"
#include "space.h"
#include "q.h"
#include "utility.h"
#include <sys/rt.h>
#include <sys/schedctl.h>
#include <sys/idbgentry.h>
#include "sys/runq.h"

kthread_t *rt_gq;
static lock_t	gqlock;

#ifdef MP
#ifdef RT_SCALE 
short pNTpri[MAXCPUS];
kthread_t **pNT[MAXCPUS];
extern int rtcpus;
#endif
#endif

kthread_t        	**bindings;
static lock_t           bindinglock;
int			rt_dither;
#define BINDING_TO_CPUID(b)      ((cpuid_t) ((b) - &bindings[0]))

void
init_rt(void)
{
	int i;

	bindings = kmem_zalloc(maxcpus * sizeof(kthread_t *), KM_NOSLEEP);
	spinlock_init(&bindinglock, "bindinglock");
	for (i = 0; i < maxcpus; i++) {
#ifdef RT_SCALE 
		/* Do this so each entry in the per-cpu rt array gets
		 * it's own cache line.  There is probably a better way
		 * to do thus such that you don't have to specify the
		 * secondary array index [0] when you access the array */
		pNT[i] = kmem_zalloc(128, KM_NOSLEEP | KM_CACHEALIGN);
		pNT[i][0] = NULL;
		/* Don't use array entries corresponding to disabled cpus */
		if(cpu_enabled(i))
			pNTpri[i] = PIDLE;
		else
			pNTpri[i] = 256;
#endif
		bindings[i] = NULL;
	}
	spinlock_init(&gqlock, "rtgqlck");
}

#ifdef MP

#ifdef RT_SCALE
/* This function is used by the rt scheduler to determine of a thread is
 * allowed to run on a particular cpu.  Reasons the thread couldn't run
 * include the thread belonging to a cpuset that doesn't include this cpu */
INLINE static int
rt_check_cpuset(int k_cpuset, int cpuid)
{
	if (cpuset_isglobal(k_cpuset) &&
		CPUMASK_TSTB(global_cpuset.cs_cpus, cpuid))
		return 0;	
	else if (!cpuset_runok(k_cpuset, cpu_to_cpuset[cpuid]))
		return 0;
	return 1;
}
#endif

kthread_t *
rt_thread_select()
{
	cpuid_t cpu = cpuid();
	kthread_t *kt;
#ifdef RT_SCALE 
	/* If there is a thread in the pNT entry for me, get it.  Also make sure
	 * it is runnable.  If it isn't, put it back on a runq by calling putrunq()
	 * and look for a thread in rt_gq. */
	if (pNT[cpu][0]) {
		kt = (kthread_t *)swap_ptr((void **)pNT[cpu], 0);
		if(kt) { /* Thread could be removed by removerunq() -> rt_remove_q() */	
			kt_nested_lock(kt);
			if (runcond_ok(kt) &&
			    cpuset_runok(kt->k_cpuset, cpu_to_cpuset[cpu])) {
					return kt;
			}
			else {
				kt->k_onrq = CPU_NONE;
				putrunq(kt, CPU_NONE);
			}
			kt_nested_unlock(kt);
		}
	}
#endif

	ASSERT(issplhi(getsr()));
	do {
		if (!rt_gq)
			return 0;
	} while (!nested_spintrylock(&gqlock));

	if (rt_gq) {
		kt = rt_gq;
		do {
			kthread_t *pkt;
			kt_nested_lock(kt);
			if (runcond_ok(kt) && 
				cpuset_runok(kt->k_cpuset, 
						cpu_to_cpuset[cpuid()])) {
				kt->k_rflink->k_rblink = kt->k_rblink;
				kt->k_rblink->k_rflink = kt->k_rflink;
				if (rt_gq == kt)
					rt_gq =
					  kt->k_rflink == kt ? 0 : kt->k_rflink;
				kt->k_rblink = kt->k_rflink = kt;
				nested_spinunlock(&gqlock);
				return kt; /* return kt locked */
			}
			pkt = kt;
			kt = kt->k_rflink;
			kt_nested_unlock(pkt);
		} while (kt != rt_gq);
	}
	nested_spinunlock(&gqlock);
	return 0;
}
#endif

int
rt_remove_q(kthread_t *kt)
{
	if (!nested_spintrylock(&gqlock))
		return 0;
#ifdef RT_SCALE	
	/* Check if thread is in pNT.  If not, just do the old thing, which is
	 * removing it from rt_gq */
	if(kt->k_onrq & RT_Q) {
		if(!compare_and_swap_kt(pNT[kt->k_onrq & RT_CPU], kt, 0)) {
			nested_spinunlock(&gqlock);
			return 0;
		}
		else {
			pNTpri[kt->k_onrq & RT_CPU] = pdaindr[kt->k_onrq & RT_CPU].pda->p_curpri;
			goto done;
		}
	}
#endif

	kt->k_rflink->k_rblink = kt->k_rblink;
	kt->k_rblink->k_rflink = kt->k_rflink;
	if (rt_gq == kt)
		rt_gq = kt->k_rflink == kt ? 0 : kt->k_rflink;
	kt->k_rblink = kt->k_rflink = kt;
done:
	kt->k_onrq = CPU_NONE;
	nested_spinunlock(&gqlock);
	return 1;
}

INLINE static kpri_t
rt_peekqueue(kthread_t *kt)
{
	if (!kt || KT_ISNPRMPT(kt))
		return PIDLE;
	else
		return(kt->k_pri);
}

static kpri_t
rt_queued_pri(void)
{
	int p1, p2;
	p1 = rt_peekqueue(rt_gq);
	p2 = rt_peekqueue(private.p_cpu.c_threads);
#ifdef RT_SCALE
	/* Must also check pNT */
	p1 = p1 > p2 ? p1 : p2;
	p2 = rt_peekqueue(pNT[cpuid()][0]);
#endif
	return(p1 > p2 ? p1 : p2);
}

#ifdef MP
void

/* rt_setpri() is used to update pNTpri.  It is called wherever p_curpri is set.
 * In a sense, pNTpri is kept as a copy of all the p_curpri's, because looping
 * looping through all of the pda's would be too expensive.  This is not strictly
 * true however since pNTpri is also updated when a thread is placed in pNT. */
rt_setpri(short pri)
{
#ifdef RT_SCALE
        cpuid_t cpu = cpuid();

        if (rtcpus >= maxcpus) {
                if (pri < 0)
                        pri = PIDLE;

                do {
                        /* Don't need to update if already the same */
                        if (pNTpri[cpu] == pri)
                                return;
                        /* Don't change if there is a thread waiting */
                        if (pNT[cpu][0])
                                return;
                } while (!nested_spintrylock(&gqlock));

                if (!pNT[cpu][0])
                        pNTpri[cpu] = pri;

                nested_spinunlock(&gqlock);
    }
#endif /* RT_SCALE */
}
#else
#define rt_setpri(x)
#endif /* MP */

void
reset_pri(kthread_t *kt)
{
	int qp = rt_queued_pri();

	private.p_curpri = kt->k_pri;
	rt_setpri(kt->k_pri);

	if (qp > private.p_curpri)
		private.p_runrun = 1; /* FIX: REVISIT */
}

#define RTPRI(kt)	(kt ? kt->k_pri : -1)

static kpri_t
get_lowest_bound(int cpuset)
{
	int i;
	kpri_t lowpri;

	for (i = 0, lowpri = 256; i < maxcpus; i++) {
		/* Check for holes in processor space */
		if (!cpu_running(i) || 
			pdaindr[i].pda->p_cpu.c_restricted == 1 ||
		    !cpuset_runok(cpuset, cpu_to_cpuset[i]))
			continue;
		if (!bindings[i]) 
			return -1;
		if (bindings[i]->k_pri < lowpri)
			lowpri = bindings[i]->k_pri;
	}
	return lowpri;
}

void
start_rt(kthread_t *kt, kpri_t pri)
{
	int i;
	kthread_t **lowbinding, *victim;
	kpri_t lowpri;
	int cpuset = kt->k_cpuset == -1 ? CPUSET_GLOBAL : kt->k_cpuset;
	ASSERT(kt_islocked(kt) && issplhi(getsr()));
	ASSERT((kt->k_flags & KT_BIND) && kt->k_binding == CPU_NONE);
retry:
	nested_spinlock(&bindinglock);
	ASSERT(cpuset >= 1);
	lowbinding = NULL;
	if (KT_ISMR(kt)) {
		if (RTPRI(bindings[kt->k_mustrun]) < pri)
			lowbinding = &bindings[kt->k_mustrun];
		if (!lowbinding) {
			nested_spinunlock(&bindinglock);
			return;
		}
	} else {
		if (pri <= cpuset_lbpri(cpuset)) {
			nested_spinunlock(&bindinglock);
			return;
		}
		for (i = 0, lowpri = pri; i < maxcpus; i++) {
			/* Check for holes in processor space */
			if (!cpu_running(i))
				continue;
			if (!cpuset_runok(cpuset, cpu_to_cpuset[i]))
				continue;
			if (pdaindr[i].pda->p_cpu.c_restricted == 1)
				continue;
			if (!bindings[i]) {
				lowbinding = &bindings[i];
				break;
			} else if (RTPRI(bindings[i]) < lowpri) {
				lowbinding = &bindings[i];
				lowpri = bindings[i]->k_pri;
			}
		}
	}
	if (!lowbinding) { 
		nested_spinunlock(&bindinglock);
		return;
	}
	if (victim = *lowbinding) {
		if (kt_nested_trylock(victim)) {
			victim->k_binding = CPU_NONE;
			kt_nested_unlock(victim);
		} else {
			nested_spinunlock(&bindinglock);
			/* victim might also be trying to get the
			 * bindinglock, so give it a chance to get it */
			DELAY(1);
			goto retry;
		}
	}
	*lowbinding = kt;
	kt->k_binding = BINDING_TO_CPUID(lowbinding);
	cpuset_lbpri(cpuset) = get_lowest_bound(cpuset);
	nested_spinunlock(&bindinglock);
}

void
rt_rebind(kthread_t *kt)
{
	ASSERT(kt_islocked(kt) && (kt->k_flags & KT_BIND) &&
		(kt->k_binding != CPU_NONE || KT_ISMR(kt)));
	if (kt->k_binding != CPU_NONE) {
		/* 
		 * undo binding and try to re-establish
		 */
		nested_spinlock(&bindinglock);
		bindings[kt->k_binding] = NULL;
		ASSERT(kt->k_cpuset >= 1);
		if (pdaindr[kt->k_binding].pda->p_cpu.c_restricted != 1)
			cpuset_lbpri(kt->k_cpuset) = -1;
		kt->k_binding = CPU_NONE;
		nested_spinunlock(&bindinglock);
	}
	start_rt(kt, kt->k_pri);
}

void
end_rt(kthread_t *kt)
{
	ASSERT(kt_islocked(kt) && issplhi(getsr()));
	nested_spinlock(&bindinglock);
	if (kt->k_binding != CPU_NONE) {
		bindings[kt->k_binding] = NULL;
		ASSERT(kt->k_cpuset >= 1);
		if (pdaindr[kt->k_binding].pda->p_cpu.c_restricted != 1)
			cpuset_lbpri(kt->k_cpuset) = -1;
		kt->k_binding = CPU_NONE;
	}
	nested_spinunlock(&bindinglock);
}

/* ARGSUSED */
void
redo_rt(kthread_t *kt, kpri_t newpri)
{
	/* if kthread bound, may need to redo binding */
	/* needs the schedlock */
}

#ifdef MP

cpuid_t
rt_add_gq(kthread_t *kt)
{
	kthread_t	*tkt;
	short		pri;
	cpuid_t		i;
	cpuid_t		cpu = maxcpus;
	cpuid_t		min = maxcpus;	
	short		minpri = 255;

	ASSERT(kt_islocked(kt) && KT_ISPS(kt));
	ASSERT(kt == kt->k_rflink && kt == kt->k_rblink);

	nested_spinlock(&gqlock);
#ifdef RT_SCALE
    if (rtcpus >= maxcpus) {
	/* First try to put thread on cpu where it just ran.  k_sonproc is
	 * is used because k_lastrun is changed by some NUMA thread migration
	 * code.  Thus, k_lastrun does not always reflect where the thread
	 * last ran.  This case is really just an optimization so that a thread
	 * that does very little work and switches out and back in again will
	 * run quickly and on the same cpu. */

	i = kt->k_sonproc;
	if (i != CPU_NONE && 
	    rt_check_cpuset(kt->k_cpuset, i) &&
	    kt->k_pri == pNTpri[i] &&
	    !pNT[i][0] &&
	    (!rt_gq || kt->k_pri > rt_gq->k_pri)) {
		ASSERT(pNT[i][0] == 0);
		pNT[i][0] = kt;
		kt->k_onrq = i | RT_Q;
		nested_spinunlock(&gqlock);
		return(i);
	}

	/* This is another optimization similar to the above.  This is the more
	 * general case where we just check the pNT entry specified by k_lastrun
	 * before we try the long case of looping through all of the pNTpri's. If
	 * the pNTpri entry is PIDLE, we push the rt thread onto this cpu. */
	if (kt->k_lastrun != CPU_NONE &&
	    rt_check_cpuset(kt->k_cpuset, kt->k_lastrun))
	        if (pNTpri[kt->k_lastrun] == PIDLE) {
				ASSERT(pNT[kt->k_lastrun][0] == 0);
				pNTpri[kt->k_lastrun] = kt->k_pri;
				pNT[kt->k_lastrun][0] = kt;
				kt->k_onrq = kt->k_lastrun | RT_Q;
				nested_spinunlock(&gqlock);
				return(kt->k_lastrun);
		}
		/* If the thread has affinity, and it has a higher pri than
		 * the thread currently on the cpu, bump the currently running
		 * thread.  (This case was added because of PV 698773) */
		else if (KT_ISHELD(kt) && kt->k_pri > pNTpri[kt->k_lastrun]) {
			tkt = (kthread_t *)swap_ptr((void **)pNT[kt->k_lastrun], kt);
			kt->k_onrq = kt->k_lastrun | RT_Q;
			pNTpri[kt->k_lastrun] = kt->k_pri;
                	if(!tkt) {
				nested_spinunlock(&gqlock);
				return(kt->k_lastrun);
			}
                        /* If we're here, there was another thread in pNT[].
                         * Assign it to kt so it can be handled by following code.
                         * Also set RT_NO_DSPTCH flag, which will cause kt to
                         * be unlocked later (since we must lock it now). */
			cpu = kt->k_lastrun | RT_NO_DSPTCH;
			kt = tkt;
			kt_nested_lock(kt);
                }

	/* This is the most general case of looping through all of the pNTpri's.
	 * The goal is for the previous 2 cases to be hit much more often than
	 * this case.  If a cpu having pNTpri entry equal to PIDLE is found, we
	 * immediately break out of the loop.  Otherwise, we keep track of which 
	 * cpu has the lowest priority thread.  We then compare the priority of
	 * our thread to the lowest priority thread.  If we have a higher priority
	 * thread, we push our thread onto the pNT entry for that cpu.  That may
	 * include swapping places with the lowest priority thread if it is still
	 * in the pNT entry (i.e., has not yet been dispatched).  In this case,
	 * we put the lowest priority thread onto rt_gq.  We also return a cpu
	 * value with a bit turned on to indicate that the target cpu does not
	 * need to be sent a cross-cpu interrupt.  This is because one must have
	 * already been sent if there was a thread waiting there--the thread just
	 * wasn't dequeued quite yet.  The other case is that our thread has a
	 * lower priority than any of the pNTpri entries.  If that is the case,
	 * we put the thread on rt_gq and do nothing else */
	for (i = 0; i < maxcpus; i++) {
		ASSERT(kt->k_mustrun == CPU_NONE);
		if (!rt_check_cpuset(kt->k_cpuset, i))
			continue;
		if (pNTpri[i] == PIDLE) {
			ASSERT(pNT[i][0] == 0);
			pNTpri[i] = kt->k_pri;
			pNT[i][0] = kt;
			kt->k_onrq = i | RT_Q;
			if (cpu & RT_NO_DSPTCH)
				kt_nested_unlock(kt);
			nested_spinunlock(&gqlock);
			return(i);
		}

		if (pNTpri[i] < minpri) {
			minpri = pNTpri[i];
			min = i;
		}
	}
	
	if (kt->k_pri > minpri && min < maxcpus) {
		tkt = (kthread_t *)swap_ptr((void **)pNT[min], kt);
		kt->k_onrq = min | RT_Q;
		pNTpri[min] = kt->k_pri;
		if (cpu & RT_NO_DSPTCH)
			kt_nested_unlock(kt);
		if(tkt) {
			/* If we're here, there was another thread in pNT[min].
			 * We must put in on rt_gq.  We assign this thread to
			 * kt to be compatible with the rt_gq code below. */
			kt = tkt;
			cpu = min | RT_NO_DSPTCH;
			kt_nested_lock(kt);
			goto global;
		}
		nested_spinunlock(&gqlock);
		return(min);
	}
    }
#endif
global:

	pri = kt->k_preempt ? kt->k_pri + 1 : kt->k_pri;

	if (rt_gq) {
		tkt = rt_gq;
		do {
			if (tkt->k_pri < pri)
				break;
			tkt = tkt->k_rflink;
		} while (tkt != rt_gq);
		kt->k_rflink = tkt;
		kt->k_rblink = tkt->k_rblink;
		kt->k_rblink->k_rflink = kt;
		tkt->k_rblink = kt;
		if (rt_gq->k_pri < pri)
			rt_gq = kt;
	} else
		rt_gq = kt;

	kt->k_onrq = maxcpus;
	if(cpu & RT_NO_DSPTCH)
		kt_nested_unlock(kt);
done:
	nested_spinunlock(&gqlock);
	return(cpu);
}
#endif

void
rt_restrict(cpuid_t cpu)
{
	kthread_t *kt;

#ifdef RT_SCALE
	/* Must clear out local rt entry */
        if (pNT[cpu][0]) {
                kt = (kthread_t *)swap_ptr((void **)pNT[cpu], 0);
                if(kt) { /* Thread could be removed by removerunq() -> rt_remove_q() */
                        kt_nested_lock(kt);
                        kt->k_onrq = CPU_NONE;
                        putrunq(kt, CPU_NONE);
                        kt_nested_unlock(kt);
                }
	}
#endif
	if ((kt = bindings[cpu]) == NULL) {
		return;
	}
		

	ASSERT(issplhi(getsr()));
	kt_nested_lock(kt);
	if ((kt->k_flags & KT_BIND) && kt->k_binding == cpu &&
		kt->k_mustrun != cpu)
	{
		rt_rebind(kt);
	}
	kt_nested_unlock(kt);
}

void
rt_unrestrict(cpuid_t cpu)
{
	ASSERT(issplhi(getsr()));
	nested_spinlock(&bindinglock);
	ASSERT(cpu >= 0);
	if (bindings[cpu])
		cpuset_lbpri(cpu_to_cpuset[cpu]) = get_lowest_bound(cpu_to_cpuset[cpu]);
	else
		cpuset_lbpri(cpu_to_cpuset[cpu]) = -1;
	nested_spinunlock(&bindinglock);
}

#ifdef MP
void *
rt_pin_thread(void)
{
	__psint_t pspin;
	kthread_t *kt;
	int s;

	if ((kt = curthreadp) == NULL)
		return 0;

	s = kt_lock(kt);
	pspin = kt->k_flags & KT_PSPIN;
	kt->k_flags |= KT_PSPIN;
	kt_unlock(kt, s);
	return (void *)pspin;
}

void
rt_unpin_thread(void *arg)
{
	kthread_t *kt;
	int s;
	int resched = 0;

	if ((kt = curthreadp) == NULL)
		return;

	s = kt_lock(kt);
	if (!arg) {
		kt->k_flags &= ~KT_PSPIN;
		if (private.p_cpu.c_restricted == 1 && cpuid() != kt->k_mustrun)
			resched = 1;
	}
	kt_unlock(kt, s);
	if (resched)
		qswtch(MUSTRUNCPU);
}
#endif

void idbg_rtpri(__psint_t x)
{
#ifdef RT_SCALE
	int i;

	for(i = 0; i < maxcpus; i++)
		qprintf("%4d ", pNTpri[i]);
	qprintf("\n");
#endif
}

void idbg_rtq(__psint_t x)
{
#ifdef RT_SCALE
	int i;

	for(i = 0; i < maxcpus; i++)
		qprintf("0x%x\n", pNT[i][0]);
#endif
}
