/*
 * FILE: irix/kern/os/scheduler/batch.c
 *
 * DESCRIPTION:
 *      Implements batch scheduling class and process batch jobs.
 */

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


#include <sys/space.h>
#include <sys/idbgentry.h>	/* qprintf */
#include <sys/sched.h>		/* SCHED_RR */
#include <sys/sema_private.h>	/* SV_WAITER */  

#ifdef NUMA_SCHED

#include <sys/SN/arch.h>
#include <sys/nodemask.h>
#include <sys/numa.h>		/* memaff_getbestnode, 
				   physmem_find_cpu_cluster */
#endif


/* Global Vaiables */
int 		batch_cpus;		/* max # for batch CPUs */
cpumask_t	batch_cpumask;		/* batch cpumask */
int		batch_freecpus = -1;	/* currently free batch CPUs */
int		batch_oldcpus  = -1;	/* last allocated batch CPUs */

kthread_t 	**batch_bindings;	/* array of kt indexed by CPU # */
static lock_t 	batch_bindingslock;	/* batch bindings lock */

int		batch_isolated;		/* # of isolated batch CPUs */ 
cpumask_t	batch_cpu_isolated;	/* isolated batch cpumask */
lock_t 		batch_isolatedlock;	/* isolated batch CPUs lock */

batchq_t 	batch_queue;		/* holds a list if batch jobs */

sv_t		batch_tick_sv;		/* batch tick sync variable */


/* MACRO Definitions */
#define HEAD(v)		((void *)(v) == (void *)&batch_queue)
#define FIRST_JOB	((job_t *)batch_queue.b_queue.qe_forw->qe_parent)

#define BATCH_SCHED_TICKS	HZ
#define	TIMER_FREQ_TO_LBOLT(t)	(t * 100)


#ifdef DEBUG
int batch_debug = 0;    /* 0 - off; 1 - on */

/* Turns on batch function call tracing on console */
#define BATCH_DEBUG(string) if (batch_debug) \
        { qprintf("BATCH: %s (cpu %d line %d)\n", string, cpuid(), __LINE__); }
#else
#define BATCH_DEBUG(void);
#endif


INLINE void
batch_alloc_cpus(job_t *job, int cpu)
/*
 * Update batch_cpumask and job->b_cpumask to indicate the cpu allocated
 * to the job.
 *
 * Called By:
 *	batch.c batch_find_binding
 *	batch.c find_cpus_in_node
 */
{
	BATCH_DEBUG("[batch_alloc_cpus]");

	ASSERT(batch_freecpus >= 0);

	CPUMASK_SETB(batch_cpumask, cpu);
	CPUMASK_SETB(job->b_cpumask, cpu);

} /* batch_alloc_cpus */


static int
batch_find_binding(job_t *job)
/*
 * Iterate through each CPU until one found.  Allocate the CPU to the job by
 * calling batch_alloc_cpus().
 *
 * Called By:
 * 	batch.c batch_set_binding
 *	batch.c batch_find_bindings
 */
{
	int i;

	BATCH_DEBUG("[batch_find_binding]");

	for (i = 0; i < maxcpus; i++) {

		/* Ignore CPUs that are not enabled, restricted, or batch */
		if (!cpu_enabled(i) || cpu_restricted(i) 
				|| CPUMASK_TSTB(batch_cpumask, i))  {
			continue;
		}

		batch_alloc_cpus(job, i);

		return 1;	/* found */
	}

	return 0;	/* not found */

} /* batch_find_binding */


void
batch_init_cpus(int cpus) 
/*
 * Initialize/reinitialize batch CPU counters based on the parameter cpus.
 *
 * Called By:
 *	miser.c miser_set_resources
 */
{
	BATCH_DEBUG("[batch_init_cpus]");

	nested_spinlock(&batch_bindingslock);
	batch_cpus = cpus;

	if (batch_oldcpus == -1) {
		batch_freecpus = batch_cpus;
	} else {
		batch_freecpus = cpus - batch_oldcpus + batch_freecpus; 
	}

	batch_oldcpus = cpus;
	nested_spinunlock(&batch_bindingslock);

} /* batch_init_cpus */
	

static void
batch_no_more(kthread_t *kt, job_t *j)
/*
 * Clear/reset flags/valiables to detatch a job from batch to weightless. 
 *
 * Called By:
 *	batch.c batch_to_weightless
 */
{
	int s = kt_lock(kt);

	BATCH_DEBUG("[batch_no_more]");

	prs_flagclr(&KT_TO_UT(kt)->ut_pproxy->prxy_sched, PRSBATCH);

	if (is_bcritical(kt)) { 
		batch_unset_binding(kt);
		kt->k_flags &= ~KT_HOLD;
	}

	KT_TO_UT(kt)->ut_policy = SCHED_RR;
	kt->k_flags |= KT_BASEPS|KT_PS;
	kt_initialize_pri(kt, PWEIGHTLESS);

	if (kt->k_wchan == (caddr_t)&j->j_sv) {
		setrun(kt);
	}

	kt_unlock(kt, s);

} /* batch_no_more */


void 
batch_to_weightless(job_t *j)
/*
 * Set the job type to JOB_WT, take it off the j_queue, and call 
 * batch_no_more to take appropriate actions to remove the job from 
 * batch to weightless.
 *
 * Called By:
 *	miser.c miser_to_weightless
 */
{
	int s = mutex_spinlock(&batch_queue.b_lock);

	BATCH_DEBUG("[batch_to_weightless]");

	if (j->j_queue.qe_parent == &batch_queue) {
		rmq(&j->j_queue);
	}

	mutex_spinunlock(&batch_queue.b_lock, s);

	s = job_lock(j);
	job_for_each_thread(j, batch_no_more);
	job_reset_type(j, JOB_WT);
	job_unlock(j, s);

} /* batch_to_weightless */


/*ARGSUSED*/
static void
batch_verify_bindings(kthread_t *kt)
/*
 * Verify the sanity of the batch_bindings before and after batch_set_binding
 * and batch_unset_binding in the debug kernel.
 *
 * Called By:
 *	batch.c batch_set_binding
 *	batch.c batch_unset_binding
 */
{

#ifdef DEBUG
	int i;

	job_t *j = KT_TO_UT(kt)->ut_job;

	if (j->b_deadline <= lbolt) {	/* have not reached deadline */ 
		return;	
	}

	if (kt->k_binding == CPU_NONE) {
		for(i = 0; i < batch_cpus; i++) {
			ASSERT(batch_bindings[i] != kt);
		}
	} else {
		ASSERT(batch_bindings[kt->k_binding] == kt);
	}
#endif

} /* batch_verify_bindings */


static int
batch_is_free(int cpu, job_t* j, kthread_t *kt)
/*
 * If CPU is allocated to job, then initialize : 
 *
 * Called By:
 *	batch.c batch_is_numa_free
 *	batch.c batch_set_binding
 */
{
	/* BATCH_DEBUG("[batch_is_free]"); */

	if (!CPUMASK_TSTB(j->b_cpumask, cpu) 
			|| batch_bindings[cpu] == (kthread_t *) -1L) {
		return 0; /* not free */

	} else if (batch_bindings[cpu] == 0) {
		batch_bindings[cpu] = kt;
		KT_TO_UT(kt)->ut_gbinding = cpu;
		kt->k_binding = cpu;
		atomicAddInt(&j->b_bindings, 1);

		return 1;	/* free */
	}				

	return 0;	/* not free */

} /* batch_is_free */


#ifdef NUMA_SCHED

static void *
batch_is_numa_free(cpuid_t cpu, void *arg)
/*
 * Called By:
 *	batch.c batch_set_binding
 */
{
	kthread_t *kt = (kthread_t *)arg;

	/* BATCH_DEBUG("[batch_is_numa_free]"); */

	if (batch_is_free(cpu, KT_TO_UT(kt)->ut_job, kt)) {
		return (void*) kt;
	}

	return 0;	/* not free */

} /* batch_is_numa_free */

#endif


static int 
batch_set_binding(kthread_t *kt)
/*
 * Called By:
 *	batch.c batch_add_binding
 *	batch.c batch_critical_meter
 */
{
	int s;
	int cpu;
	int retcode = 0;

	job_t *j = KT_TO_UT(kt)->ut_job;

	BATCH_DEBUG("[batch_set_binding]");

	ASSERT(job_islocked(j));

	if (j->b_bindings == j->b_ncpus || j->b_deadline <= lbolt) {
		return 0;	/* error */
	}

	ASSERT(j->b_bindings < j->b_ncpus);
	
	s = mutex_spinlock(&batch_bindingslock);

	/* 
	 * Did a cpu get isolated? If so we need to clear
	 * a binding, and find another one. If we are doing
         * something funky with memory, then we are hosed
         * but then again a site adminstrator should not be doing 
	 * RT and Batch on the same machine 
	 */
	if (CPUMASK_TSTM(j->b_cpumask, batch_cpu_isolated)) {

		for (cpu = 0; cpu < maxcpus; cpu++) {

			if (CPUMASK_TSTB(batch_cpu_isolated, cpu) &&
					CPUMASK_TSTB(j->b_cpumask, cpu)) {
				CPUMASK_CLRB(j->b_cpumask, cpu);	
				batch_find_binding(j);
			}
		}
	}
	
	ASSERT(kt_islocked(kt));
	batch_verify_bindings(kt);

#ifdef NUMA_SCHED

	if (kt->k_lastrun != KT_TO_UT(kt)->ut_gbinding)	{
		retcode = (memaff_getbestnode (kt, batch_is_numa_free, 
					(void*) kt, 0) == 0);
	}
 
#endif
	if (retcode && KT_TO_UT(kt)->ut_gbinding != CPU_NONE) { 
		batch_is_free(KT_TO_UT(kt)->ut_gbinding, j, kt);
	}

	if (kt->k_binding == CPU_NONE) {

#ifdef NUMA_SCHED
		if (memaff_getbestnode(kt, batch_is_numa_free, (void*) kt, 0)) {
			retcode = 1;
		} else { 
#endif
			for (cpu = maxcpus - 1; cpu >= 0; cpu--) { 
				if ((retcode = batch_is_free(cpu, j, kt)) 
					== 1) {
					break;
				}
			} /* for */
#ifdef NUMA_SCHED
		}
#endif
	} else {
		retcode = 1;
	}

	batch_verify_bindings(kt);
	mutex_spinunlock(&batch_bindingslock, s);
	
	return retcode;

} /* batch_set_binding */

	
void 
batch_unset_binding(kthread_t *kt)
/*
 * Give up the binding and do the appropriate cleanup.
 * Unset b_bindings and k_binding for the job and the kthread respectively.
 * Update batch_bindings as well.
 *
 * Called By:
 *	batch.c batch_no_more
 *	batch.c batch_critical_meter
 *	batch.c batch_run_time
 *	runq.c  exitrunq
 */
{
	ASSERT(kt_islocked(kt));

	nested_spinlock(&batch_bindingslock);
	batch_verify_bindings(kt);

	if (kt->k_binding != CPU_NONE && batch_bindings[kt->k_binding] == kt) {
		job_t *j = KT_TO_UT(kt)->ut_job;
		atomicAddInt(&j->b_bindings, -1);
		batch_bindings[kt->k_binding] = 0;
	}

	kt->k_binding = CPU_NONE;
	batch_verify_bindings(kt);
	nested_spinunlock(&batch_bindingslock); 

} /* batch_unset_binding */


void
batch_pass_binding(kthread_t *kt, sv_t *sv)
/*
 * Called By:
 *	batch.c batch_thread_endrun
 *	batch.c batch_critical_meter
 *	runq.c  exitrunq
 */
{
	job_t *j = KT_TO_UT(kt)->ut_job;
	int s = job_lock(j);

	BATCH_DEBUG("[batch_pass_binding]");

	ASSERT(kt_islocked(kt));

	if (SV_WAITER(sv) && kt != SV_WAITER(sv)) {

		if (kt_nested_trylock(SV_WAITER(sv))) {
			SV_WAITER(sv)->k_binding = kt->k_binding;
			batch_bindings[kt->k_binding] = SV_WAITER(sv);
			kt_nested_unlock(SV_WAITER(sv)); 

		} else {
		   	atomicAddInt(&j->b_bindings, -1);
			batch_bindings[kt->k_binding] = 0;
		}

		sv_signal(sv);

	} else {
		atomicAddInt(&j->b_bindings, -1);
		batch_bindings[kt->k_binding] = 0;
	}

	job_unlock(j, s);
	kt->k_binding = CPU_NONE;

} /* batch_pass_binding */


static __inline int
batch_add_binding(job_t *j)
/*
 * Called By:
 *	batch.c batch_bind_threads
 */
{
	int retcode = 1;	/* success */
	int s = job_lock(j);

	BATCH_DEBUG("[batch_add_binding]");

	if (j->b_deadline >= lbolt && j->b_bindings < j->b_ncpus 
			&& SV_WAITER(&j->j_sv)) {
		kthread_t *kt = SV_WAITER(&j->j_sv);
		kt_nested_lock(kt);

		if (batch_set_binding(kt)) { 
			kt_nested_unlock(kt);	
			sv_signal(&j->j_sv);
		} else {
			kt_nested_unlock(kt);
			retcode = 0;	/* error */
		}
	}

	job_unlock(j, s);

	return retcode;

} /* batch_add_binding */


static __inline void
batch_bind_threads(job_t *j)
/*
 * Called By:
 *	batch.c batch_critical_meter
 *	batch.c batchd
 */
{
	BATCH_DEBUG("[batch_bind_threads]");

	while (j->b_deadline >= lbolt  
		&& j->b_bindings < j->b_ncpus 
		&& SV_WAITER(&j->j_sv)) {

		if (!batch_add_binding(j)) {
			break;
		}
	}

} /* batch_bind_threads */


/*ARGSUSED*/
static void
batch_initialize(kthread_t *kt, job_t *j)
/*
 * Set k_flags to KT_PS|KT_BASEPS|KT_BIND and assign priority PBATCH to the
 * k_pri and k_basepri by calling kt_initialize_pri.
 *
 * Called By:
 *	batch.c batch_set_params -> job_for_each_thread
 */
{
	BATCH_DEBUG("[batch_initialize]");

	kt_nested_lock(kt);
	kt->k_flags &= ~(KT_PS|KT_BASEPS|KT_BIND);
	kt_initialize_pri(kt, PBATCH);
	kt_nested_unlock(kt);

} /* batch_initialize */


/*ARGSUSED*/
void
batch_make_critical(kthread_t *kt, job_t *j)
/*
 * Assign priority PBATCH_CRITICAL to the kthread's k_pri and k_basepri 
 * by calling kt_initialize_pri.
 *
 * Called By:
 *	batch.c batchd	-> job_for_each_thread
 */
{
	BATCH_DEBUG("[batch_make_critical]");

	kt_nested_lock(kt);
	kt_initialize_pri(kt, PBATCH_CRITICAL);

#ifdef NUMA_SCHED
	if(!CNODEMASK_IS_ZERO(j->b_cnodemask))
		CNODEMASK_CPY(kt->k_nodemask, j->b_cnodemask);
#endif

	kt_nested_unlock(kt);

} /* batch_make_critical */


void 
batch_thread_endrun(kthread_t *kt)
/*
 * Clear binding by calling batch_pass_binding.
 *
 * Called By:
 *	runq.c ut_endrun
 */
{
	job_t *j = KT_TO_UT(kt)->ut_job;

	if (kt->k_binding != CPU_NONE) {
		ASSERT(j->b_bindings <= j->b_ncpus);
		batch_pass_binding(kt, &j->j_sv);	
	}

} /* batch_thread_endrun */


/*ARGSUSED*/
void
batch_meter(kthread_t *kt, int s)
/*
 * Create a batch job_t - only current thread as its member.
 *
 * Called By:
 *	swtch.c user_resched
 */
{
	BATCH_DEBUG("[batch_meter]");

	ASSERT(KT_ISUTHREAD(kt) && is_batch(kt) && KT_TO_UT(kt)->ut_job);

	sv_bitlock_wait_sig(&KT_TO_UT(kt)->ut_job->j_sv, PZERO+1, &kt->k_flags, 
					KT_LOCK, s);
} /* batch_meter */


int
batch_critical_meter(kthread_t *kt, int s)
/*
 * Called By:
 *	swtch.c user_resched
 */
{
	sv_t  *sv;
	job_t *j;
	int   t;

	BATCH_DEBUG("[batch_critical_meter]");

	ASSERT(KT_ISUTHREAD(kt) && is_bcritical(kt) && KT_TO_UT(kt)->ut_job);

	j = KT_TO_UT(kt)->ut_job;
	sv = &j->j_sv;

	/* first, see whether anyone needs a binding */
	batch_bind_threads(j);

	/* next, see whether another thread needs this binding */
	if (kt->k_binding != CPU_NONE && j->b_bindings == j->b_ncpus 
			&& SV_WAITER(sv)) {
		batch_pass_binding(kt, sv);
	}

#ifdef NUMA_SCHED

	if (kt->k_lastrun != KT_TO_UT(kt)->ut_gbinding 
			&& kt->k_binding != CPU_NONE) {
		batch_unset_binding(kt);
	}

#endif

	/* last, get a new binding for ourselves if necessary */
	while (kt->k_binding == CPU_NONE && is_bcritical(kt) 
			&& j->b_deadline >= lbolt) {

		t = job_lock(j);

		if (!batch_set_binding(kt)) {
			job_unlock(j, t);
			sv_bitlock_wait(&j->j_sv, 0, &kt->k_flags, KT_LOCK, s);
			s = kt_lock(kt);
		} else {
			ASSERT(kt->k_binding != CPU_NONE);
			job_unlock(j, t);

			return 0;
		}
	
	} /* while */
		
	return 0;	/* success */ 

} /* batch_critical_meter */

			
#ifdef NUMA_SCHED

static int 
find_cpus_in_node(job_t *j, int node, int remaining)
/*
 * Called By:
 *	batch.c batch_find_bindings
 */
{
	int jj;
	int found = 0;
	cpuid_t cpu = 0;

	int base = get_cnode_cpu(node);

	BATCH_DEBUG("[find_cpus_in_node]");

	for (jj = 0; jj < CPUS_PER_NODE; jj++) {
		cpu = base + jj;	

		if (cpu_running(cpu) && !cpu_restricted(cpu) 
				&& !CPUMASK_TSTB(batch_cpumask, cpu)) {

			if (remaining > 0) {
				batch_alloc_cpus(j, cpu);
				found++;
				remaining--;
			}
		}		

	} /* for */

	return found;	/* # of CPUs in the node */

} /* find_cpus_in_node */


int
batch_find_bindings(job_t *job)
/*
 * Called By:
 *	batch.c batchd
 */
{
	int jj;
	cnodemask_t nodes;
	int cpus = job->b_ncpus;

	BATCH_DEBUG("[batch_find_bindings]");

	CNODEMASK_CLRALL(nodes);

	nested_spinlock(&batch_bindingslock);

	if (cpus > batch_freecpus) {
		nested_spinunlock(&batch_bindingslock);
		return 0;	/* error */
	}
	
	nodes = physmem_find_cpu_cluster(batch_cpumask, cpus);
	CNODEMASK_SETM(job->b_cnodemask, nodes);

	if (!CNODEMASK_IS_ZERO(nodes)) {

		/* find CPUs from the selected nodes first */
		for (jj = 0; jj < maxnodes; jj++) {

			if (CNODEMASK_TSTB(nodes, jj)) {
				cpus -= find_cpus_in_node(job, jj, cpus);
			}
		}
	} /* if */	

	batch_freecpus -= job->b_ncpus;
	nested_spinunlock(&batch_bindingslock);

	return 1;	/* success */

} /* batch_find_bindings */


#else


int
batch_find_bindings(job_t *job)
/*
 * Called By:
 *	batch.c batchd
 */
{
	int j;

	BATCH_DEBUG("[batch_find_bindings]");

	nested_spinlock(&batch_bindingslock);

	if (job->b_ncpus > batch_freecpus) {
		nested_spinunlock(&batch_bindingslock);
		return 0;	/* error */
	}

	for (j = 0; j < job->b_ncpus; j++) {
		batch_find_binding(job);
	}

	batch_freecpus -= job->b_ncpus;
	nested_spinunlock(&batch_bindingslock);

	return 1;	/* success */

} /* batch_find_bindings */	

#endif	/* NUMA_SCHED */

	
void 
batch_free_binding(kthread_t *kt)
/*
 * Reset b_bindings and batch_bindings for the kthread passed in.
 *
 * Called By:
 *	batch.c batch_release_resources
 */
{
	BATCH_DEBUG("[batch_free_binding]");

	if (kt->k_binding != CPU_NONE 
			&& batch_bindings[kt->k_binding] == kt) {
		job_t *j = KT_TO_UT(kt)->ut_job;
		atomicAddInt(&j->b_bindings, -1);
		batch_bindings[kt->k_binding] = 0;
	}

} /* batch_free_binding */


void 
batch_release_resources(job_t *j)
/*
 * Release resources held by the job.
 *
 * Called By:
 *	miser.c miser_time_lock_expire
 *	miser.c miser_exit
 */
{
	kthread_t *kt;

	BATCH_DEBUG("[batch_release_resources]");

	ASSERT(job_islocked(j));

	nested_spinlock(&batch_bindingslock);

	for (kt = j->j_thread; kt; kt = kt->k_link) {
		batch_free_binding(kt);
	}

	ASSERT(!batch_cpus || batch_freecpus <= batch_cpus);

	if (!CPUMASK_IS_ZERO(j->b_cpumask)) {
		CPUMASK_CLRM(batch_cpumask, j->b_cpumask);
		batch_freecpus += j->b_ncpus;
		CPUMASK_CLRALL(j->b_cpumask);

#ifdef NUMA_SCHED

		CNODEMASK_CLRALL(j->b_cnodemask);

#endif
	}

	nested_spinunlock(&batch_bindingslock);

} /* batch_release_resources */


void
batchd(void)
/*
 * Called By:
 *	miser.c miser_set_quantum
 */
{
	int s;
	int ii;
	job_t *job;
	timespec_t tw, tr;

	BATCH_DEBUG("[batchd]");

	tw.tv_sec  = 1;
	tw.tv_nsec = 0;
	tr.tv_sec  = 0;
	tr.tv_nsec = 0;

	batch_freecpus = batch_cpus;
	
	s = mutex_spinlock(&batch_isolatedlock);

        for (ii = 0; ii < maxcpus; ii++) {
		if (!cpu_enabled(ii)) {
			batch_isolated++;
			CPUMASK_SETB(batch_cpu_isolated, ii);
			batch_bindings[ii] = (kthread_t *) -1L;
			continue;
		}
	}

	mutex_spinunlock(&batch_isolatedlock, s);

	for (;;) {
		miser_purge();

		s = mutex_spinlock(&batch_queue.b_lock);

		while (!q_empty(&batch_queue.b_queue)
				&& (job = FIRST_JOB, job->b_time <= lbolt)) {
		/*pv 763559:batch_make_critical() takes kt_lock, do it first*/
			job_for_each_thread(job, batch_make_critical);
			job_nested_lock(job);
			rmq(&job->j_queue);

			if (!batch_find_bindings(job)) {
				pushq(&batch_queue.b_queue, &job->j_queue);
				job_nested_unlock(job);
				break;
			}

			vpag_transfer_vm_pool(job->b_vpagg, miser_pool);
			job_nested_unlock(job);
			batch_bind_threads(job);
		}

		batch_queue.b_empty = q_empty(&batch_queue.b_queue);
		mutex_spinunlock(&batch_queue.b_lock, s);
		sv_timedwait(&batch_tick_sv, 0, 0, 0, 
			     kt_has_fastpriv(curthreadp) ? SVTIMER_FAST : 0,
			     &tw, &tr);	
	} /* for */

} /* batchd */


int
batch_generate_work(void)
/*
 * Called By:
 *	cpu.c cpu_thread_search
 */
{
	job_t *job;

	/* BATCH_DEBUG("[batch_generate_work]"); */

	if (batch_queue.b_empty) {
		return 0;	/* error */
	}

	nested_spinlock(&batch_queue.b_lock);

	for (job = FIRST_JOB; !HEAD(job); job = NEXT_JOB(job)) {

		if ((job->b_segment_flags & MISER_SEG_DESCR_FLAG_STATIC)
				&& (job->b_deadline > lbolt)) {
			continue;
		}

		if (sv_signal(&job->j_sv)) {
			nested_spinunlock(&batch_queue.b_lock);
			return 1;	/* success */
		}
	}

	nested_spinunlock(&batch_queue.b_lock);

	return 0;	/* error */
	
} /* batch_generate_work */


void
init_batchsys()
/*
 * Initialize batch system and its variables and components.
 *
 * Called By:
 *	utility.c schedinit
 */
{
	int i;

	BATCH_DEBUG("[init_batchsys]");

	batch_bindings = kmem_zalloc(maxcpus * sizeof(kthread_t *), 	
				KM_NOSLEEP);

        spinlock_init(&batch_bindingslock, "bindinglock");

	for (i = 0; i < maxcpus; i++) {
		batch_bindings[i] = NULL;
	}

	spinlock_init(&batch_queue.b_lock, "batch-lock");
	init_q_element(&batch_queue.b_queue, &batch_queue);
	sv_init(&batch_tick_sv, SV_DEFAULT, "batch-ticksv");

	batch_cpus          = 0;
	batch_isolated      = 0;
	batch_queue.b_empty = 1; 

	CPUMASK_CLRALL(batch_cpumask);
	CPUMASK_CLRALL(batch_cpu_isolated);

	miser_init();
	cpuset_init();

} /* init_batchsys */


void
batch_push_queue_locked(job_t* job)
/*
 * Called By:
 *	batch.c batch_push_queue
 *	miser.c miser_reset_job
 */
{
        job_t *ijob, *tjob;

	BATCH_DEBUG("[batch_push_queue_locked]");

        if (q_empty(&batch_queue.b_queue)) {
                pushq(&batch_queue.b_queue, &job->j_queue);
                return;
        }

        for (ijob = FIRST_JOB;;) {

                if (ijob->b_time > job->b_time) {
                        q_insert_before(&ijob->j_queue, &job->j_queue);
                        break;
                }

                tjob = ijob;
                ijob = NEXT_JOB(ijob);

                if (HEAD(ijob)) {
                        q_move_after(&tjob->j_queue, &job->j_queue);
                        break;
                }
        }

#ifdef DEBUG

        for (ijob = FIRST_JOB;;) {
                tjob = NEXT_JOB(ijob);

                if (HEAD(tjob)) {
                        break;
		}

                ASSERT(ijob->b_time <= tjob->b_time);
                ijob = tjob;
        }

#endif

} /* batch_push_queue_locked */


void
batch_push_queue(job_t* job)
/*
 * Puts a job on the batch_queue.
 *
 * Called By:
 *	miser.c miser_send_request_scall
 */
{
        ASSERT(issplhi(getsr()));

	BATCH_DEBUG("[batch_push_queue]");

        nested_spinlock(&batch_queue.b_lock);
        batch_push_queue_locked(job);
        nested_spinunlock(&batch_queue.b_lock);

} /* batch_push_queue */


void
batch_set_params(job_t *job, miser_seg_t *segment)
/*
 * Set ncpus, memory, deadline, time, etc.
 *
 * Note: This function expects the job to be locked coming in.
 *
 * Called By:
 *	miser.c miser_reset_job
 *	miser.c miser_send_request_scall
 *	miser.c miser_time_lock_expire
 */
{
	micro_t cpu_time;

	BATCH_DEBUG("[batch_set_params]");

	ASSERT(job_islocked(job));

	job->b_ncpus    = segment->ms_resources.mr_cpus;
	job->b_memory   = segment->ms_resources.mr_memory;

	cpu_time = segment->ms_rtime * HZ / job->b_ncpus;

	job->b_deadline = segment->ms_etime;
	job->b_time     = job->b_deadline - cpu_time;
	job->b_urgency  = -lbolt + job->b_time;

	CPUMASK_CLRALL(job->b_cpumask);

#ifdef NUMA_SCHED

	CNODEMASK_CLRALL(job->b_cnodemask);

#endif /* NUMA_SCHED */

	job->b_segment_flags = segment->ms_flags;

	job_for_each_thread(job, batch_initialize);

} /* batch_set_params */


static void
batch_dbgp(job_t *j)
/*
 * Called By:
 *	batch.c	batch_schedvec
 */
{
	int i = 0;

	qprintf( "          ncpus %d bid %d urgency 0x%llx dline %d time %d \n"
			"bindings %d \n", j->b_ncpus, j->b_bid, j->b_urgency,
			j->b_deadline, j->b_time, j->b_bindings);

	qprintf("BOUND CPUS\n");

	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(j->b_cpumask, i)) {
			qprintf(" %d ", i);
		} else { 
			qprintf(" X ");
		}
	}

	qprintf("\n");

#ifdef NUMA_SCHED

	qprintf("BOUND NODES\n");

	for (i = 0; i < maxnodes; i++) {
		if (CNODEMASK_TSTB(j->b_cnodemask, i)) {
			qprintf(" %d ", i);
		} else {
			qprintf(" X ");
		}
	}

	qprintf("\n");

#endif

} /* batch_dbgp */
	

/*ARGSUSED*/
static void
batch_run_time(kthread_t *kt)
/*
 * Called By:
 *	batch.c	batch_schedvec
 */
{
	job_t *j = KT_TO_UT(kt)->ut_job;

	if (j->b_deadline <= lbolt) {
		if (is_bcritical(kt)) { 
			batch_unset_binding(kt);	
		}
	}

} /* batch_run_time */


static void
batch_detach(job_t * job)
/*
 * Called By:
 *	batch.c	batch_schedvec
 */
{
	nested_spinlock(&batch_queue.b_lock);
	job_nested_lock(job);

	if (q_onq(&job->j_queue)) {
		rmq(&job->j_queue);
	}

	nested_spinunlock(&batch_queue.b_lock);

	miser_exit(job);

} /* batch_detach */  


schedvec_t batch_schedvec = {
	"batch",
	batch_dbgp,
	batch_run_time,
	batch_detach
};


/*ARGSUSED*/
void
init_batch(job_t *j, kthread_t *kt)
/*
 * Called By:
 *	job.c	sched_init
 */
{
	j->j_ops      = batch_schedvec;
	j->b_urgency  = -1;
	j->b_deadline = 0;
	j->b_time     = 0;
	j->b_bindings = 0;

} /* init_batch */


int
batch_create()
/*
 * Called By:
 *	miser.c  miser_send_request_scall
 */
{
	BATCH_DEBUG("[batch_create]");

	ASSERT(issplhi(getsr()));

	if (!KT_ISPS(curthreadp)) {
		job_unlink_thread(curthreadp);
	}

	job_create(JOB_BATCH, curthreadp);

	return 0;	/* success */

} /* batch_create */


void     
batch_isolate(cpuid_t cpu)
/*
 * Called By:
 *	miser.c miser_send_request_scall
 */
{
	job_t *j;	

	int s = mutex_spinlock(&batch_bindingslock);
	kthread_t *kt = batch_bindings[cpu];

	BATCH_DEBUG("[batch_isolate]");

	CPUMASK_SETB(batch_cpu_isolated, cpu);
	batch_bindings[cpu] = (kthread_t *) -1L;

	if (kt && kt != (kthread_t *) -1L) {
		j = KT_TO_UT(kt)->ut_job;
		kt_nested_lock(kt);

		if (kt->k_binding == cpu) {
			atomicAddInt(&j->b_bindings, -1);
			kt->k_binding = CPU_NONE;	
		}

		kt_nested_unlock(kt);
	} 

	mutex_spinunlock(&batch_bindingslock, s);

} /* batch_isolate */


void
batch_unisolate(cpuid_t cpu)
/*
 * Called By:
 *	miser.c unrestrict_cpuset
 */
{
	int isolate = 0 ;
	int s = mutex_spinlock(&batch_bindingslock);

	BATCH_DEBUG("[batch_unisolate]");

	CPUMASK_CLRB(batch_cpu_isolated, cpu);

	if (batch_bindings[cpu] == (kthread_t *) -1L) {
		isolate++;
		batch_bindings[cpu] = 0;
	}

	mutex_spinunlock(&batch_bindingslock, s);
	s = mutex_spinlock(&batch_isolatedlock);

	if (isolate) {
		batch_isolated--;
	}

	ASSERT(batch_isolated >= 0);
        mutex_spinunlock(&batch_isolatedlock, s);

} /* batch_unisolate */
