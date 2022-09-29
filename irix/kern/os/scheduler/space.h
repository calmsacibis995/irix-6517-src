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
#ifndef _SPACE_H
#define _SPACE_H

#include <sys/ktime.h>
#include <sys/timers.h>
#include <sys/sema.h>
#include <sys/pda.h>
#include <sys/kmem.h>
#include <sys/q.h>
#include <sys/miser_public.h>
#include <ksys/vpag.h>
#include <sys/cpumask.h>
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <ksys/vfile.h>
#include <sys/wtree.h>

#ifdef NUMA_SCHED
#include <sys/nodemask.h>
#endif

extern micro_t basictomicro(basictime_t);

#define cpu_running(cpu) (pdaindr[cpu].CpuId == cpu)

#define NEXT_JOB(j) ((job_t *)(j)->j_queue.qe_forw->qe_parent)
#define PREV_JOB(j) ((job_t *)(j)->j_queue.qe_back->qe_parent)
typedef short kpri_t;

typedef struct batchacct_s {
	micro_t		b_urgency; /* time remaining - b_ratio */
	micro_t		b_deadline; /* in lbolt when the segment must end */
	micro_t		b_time; /* amount of total cpu time needed */
	uint32_t 	b_segment_flags;	
	cpumask_t	b_cpumask;
#ifdef NUMA_SCHED
	cnodemask_t	b_cnodemask;
#endif
	pid_t		b_bid;  
	vpagg_t		*b_vpagg;  
	ncpus_t		b_ncpus; /* number of cpus for current segment */
	ncpus_t		b_bindings; /* number of threads on queues or running */
	memory_t	b_memory; /* memory needed for current segment */
} batchacct_t;


typedef struct wtacct_s {
	weight_t	w_weight;
	credit_t	w_acctime;
	char		w_pref;	/* write-only - used to preload cached line */
#ifdef DEBUG
	credit_t	w_lastacc;
	credit_t	w_lastinc;
#endif
} wtacct_t;

/*
 * The w_acctime variable is updated every clocktick. If a large number of
 * threads share the same wtacct_t structure (eg., SPROC), then the line
 * containing this variable becomes highly contended. 
 *
 * We use the atomicAddIntHot2 routines to update this variable. This routine takes
 * an additional pointer to a byte in the same cacheline to use to force a
 * prefetch-exclusive. This substantially reduces thrashing of the cacheline.
 * See the atomicAddIntHot routines for more information.
 *
 * Aligning w_acctime on a 16 byte boundary will guarantee that w_acctime & w_pref
 * are in the same cache line. 
 *
 * NOTE: we really should use the hotIntCounter_t struct here - but it would
 * require a more radical change to a potentially exported data structure.
 */
#pragma set field attribute wtacct_t w_acctime align=16


typedef enum { JOB_NOTYPE = 0, JOB_WT = 1, JOB_BATCH = 2 } job_type_t;

/*
 * job_flags
 */
#define JOB_LOCK	0x00000001
#define JOB_CACHED	0x00000002

struct job_s;
struct creditrec;
struct gdb_s;
typedef struct schedvec_s {
	char	*jo_str;
	void	(*jo_dbgp)(struct job_s *);
	void	(*jo_run_time)(struct kthread *);
	void	(*jo_detach)(struct job_s *);
} schedvec_t;

typedef struct job_s {
	struct q_element_s j_queue;
	sv_t		j_sv;
	struct kthread  *j_thread;
	int		j_nthreads;
	job_type_t	j_type;
	schedvec_t	j_ops;
	uint_t		j_flags;
	struct gdb_s	*j_xxx_gdb;	/* XXX: obsolete */
	union {
		wtacct_t	ju_wt;
		batchacct_t	ju_batch;
	} j_un;
} job_t;

#define w_weight	j_un.ju_wt.w_weight
#define w_acctime	j_un.ju_wt.w_acctime
#define w_pref		j_un.ju_wt.w_pref
#ifdef DEBUG
#define w_lastacc	j_un.ju_wt.w_lastacc
#define w_lastinc	j_un.ju_wt.w_lastinc
#endif

#define b_cpumask	j_un.ju_batch.b_cpumask
#define b_urgency       j_un.ju_batch.b_urgency
#define b_bindings	j_un.ju_batch.b_bindings
#define b_time          j_un.ju_batch.b_time
#define b_deadline      j_un.ju_batch.b_deadline
#define b_bid           j_un.ju_batch.b_bid
#define b_memory	j_un.ju_batch.b_memory
#ifdef NUMA_SCHED
#define b_cnodemask	j_un.ju_batch.b_cnodemask
#endif
#define b_ncpus		j_un.ju_batch.b_ncpus
#define b_segment_flags j_un.ju_batch.b_segment_flags
#define b_vpagg		j_un.ju_batch.b_vpagg

#define job_lock(j)		(mutex_bitlock(&(j)->j_flags, JOB_LOCK))
#define job_unlock(j,rv)	(mutex_bitunlock(&(j)->j_flags, JOB_LOCK, rv))
#define job_nested_lock(j)	(nested_bitlock(&(j)->j_flags, JOB_LOCK))
#define job_nested_unlock(j)	(nested_bitunlock(&(j)->j_flags, JOB_LOCK))
#define job_islocked(j) 	(bitlock_islocked(&(j)->j_flags, JOB_LOCK))

#define job_fset(j,b)		(bitlock_set(&(j)->j_flags, JOB_LOCK, b))
#define job_fclr(j,b)		(bitlock_clr(&(j)->j_flags, JOB_LOCK, b))

#define JOB_ISCACHED(j)		((j)->j_flags & JOB_CACHED)

#define IS_BATCH(ut) ((ut)->ut_job ? ((ut)->ut_job->j_type == JOB_BATCH) : 0) 
#define IS_WT(ut) ((ut)->ut_job->j_type == JOB_WT)

void null_func();

void init_batch(job_t *, kthread_t *);

typedef struct batch_queue_s {
	lock_t	b_lock;
	struct q_element_s b_queue;
	job_t	*b_rotor;
	short	b_work_onq;
	int 	b_empty;
} batchq_t;

typedef struct cpu_set_s {
        int		cs_idler;
	cpuid_t		cs_mastercpu;
        id_type_t       cs_name;
        int             cs_count;
        lock_t          cs_lock;
        uint32_t        cs_flags;
        cpumask_t       cs_cpus;
	kpri_t		cs_lbpri;
	vnode_t*	cs_file;
	cnodemask_t	cs_nodemask;
	cpumask_t	cs_node_cpumask;
	cnodemask_t	*cs_nodemaskptr;
} cpuset_t;



extern int batch_cpus;
extern lock_t batch_isolatedlock;
extern int batch_isolated;
#define CPUSET_GLOBAL 1
extern int16_t*	cpu_to_cpuset;
extern cpuset_t *cpusets;
extern zone_t	*job_zone_private;
extern zone_t   *job_zone_public;
#define global_cpuset 	cpusets[CPUSET_GLOBAL]
#define PRIMARY_IDLE 	global_cpuset.cs_idler
#define	lbpri		global_cpuset.cs_lbpri 

#define cpuset_master(kt) cpusets[(kt)->k_cpuset].cs_mastercpu
#define cpuset_idle(kt)	cpusets[(kt)->k_cpuset].cs_idler != CPU_NONE ?  \
				cpusets[(kt)->k_cpuset].cs_idler :  \
				cpuset_master((kt))
#define	cpuset_lock(kt)	cpusets[(kt)->k_cpuset].cs_lock
#define cpuset_count(kt) cpusets[(kt)->k_cpuset].cs_count
#define cpuset_lbpri(index) cpusets[(index)].cs_lbpri

#define cpuset_open(index)	((cpusets[(index)].cs_flags&MISER_CPUSET_CPU_EXCLUSIVE) == 0)
#define cpuset_isglobal(index)	((index) == CPUSET_GLOBAL) 

#define cpuset_runok(k_cpuset, cpuset)  (((k_cpuset) == cpuset) || \
				(cpuset_isglobal(k_cpuset) &&  \
					cpuset_open(cpuset)))

struct kthread *cpu_thread_search	(void);
int		cpu_frs_dequeue		(cpu_t *, kthread_t *);
void		init_cpus		(void);
void 		cpu_sched_tick		(kthread_t *);
void 		cpu_insert_localq	(kthread_t *, cpuid_t);
int 		cpu_remove_localq	(cpuid_t, kthread_t *);
void		cpu_restrict		(cpuid_t,  int);
void		cpu_numa_affinity	(kthread_t *);

job_t *		job_create		(job_type_t, struct kthread *);
void		job_reset_type		(job_t*, job_type_t);
void		job_attach_thread	(job_t *, struct kthread *);
void		job_destroy		(job_t *job);
void		job_unlink_thread	(kthread_t *kt);
void		job_locked_unlink_thread(job_t *j, kthread_t *kt);
void		job_cache		(job_t *job);
void		job_uncache		(job_t *job);
void		job_for_each_thread(job_t *, void (*)(kthread_t *, job_t *));
#define		job_allocate()	kmem_zone_free(job_zone_private, \
				kmem_zone_alloc(job_zone_public, KM_SLEEP))
#define		job_free()	kmem_zone_free(job_zone_public, \
				kmem_zone_alloc(job_zone_private, KM_SLEEP))

int		batch_critical_meter(kthread_t *, int);
void		batch_dump(void);
struct kthread *batch_get_critical(void);
void		batch_isolate(cpuid_t cpu);
void		batch_meter(kthread_t *, int);
void		batch_push_queue(job_t *);
void		batch_release_resources(job_t *);
void		batchd(void);
int		batch_generate_work(void);
void		batch_set_params(job_t *, miser_seg_t*);
void		batch_thread_endrun(kthread_t *);
void		batch_to_weightless(job_t *);
void		batch_unisolate(cpuid_t cpu);
void		batch_unsched_job(job_t *);
void		batch_unset_binding(kthread_t *);
void		batch_pass_binding(kthread_t *, sv_t *);
void		batch_release_resources(job_t *);
void		init_batchsys(void);
void		miser_purge(void);
void		miser_cleanup(vpagg_t *);
struct kthread;
struct uthread_s;
struct shaddr_s;
struct gdb_s;

void 		gang_block(struct uthread_s *);
void 		gang_create(struct shaddr_s *);
void 		gang_leave(job_t *);
void 		gang_unlink(kthread_t *);
void 		gang_link(struct shaddr_s *, struct uthread_s *);
int 		gang_startrun(kthread_t *, int );
void 		gang_sched_tick(void);
void 		gang_endrun(struct uthread_s *, struct gdb_s *gdb);
void 		gang_free(struct shaddr_s *);
void 		gang_exit(struct shaddr_s *);
void 		init_gang(void);

/* Values for ut_gstate */
#define GANG_NONE	0
#define GANG_RUNNING	1
#define GANG_EARN	2
#define GANG_BLOCKED	3
#define GANG_LAST	4
#define GANG_STARTED	6
#define GANG_UNDEF      7

void	print_ssthread	(struct kthread *);

#endif /* _SPACE_H */
