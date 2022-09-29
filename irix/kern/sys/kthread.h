/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef	_KTHREAD_H
#define	_KTHREAD_H

#include <sys/types.h>
#include <sys/pcb.h>
#include <sys/reg.h>
#include <sys/pda.h>
#include <sys/q.h>
#include <sys/timers.h>

#if CELL
#include <ksys/cell/service.h>
#endif

#define	KT_STACK_MALLOC		0x00000001
#define	KT_WMUTEX		0x00000002 /* waiting on mutex */
#define KT_LOCK			0x00000004 /* lock for k_flags */
#define KT_WACC			0x00000008 /* waiting for mr(acc) */
#define KT_STARVE		0x00000010 /* lock starvation */
#define	KT_WSV			0x00000020 /* waiting on sv */
#define	KT_WSVQUEUEING		0x00000040 /* about to go on an SV */
#define	KT_INDIRECTQ		0x00000080 /* about to go on an SV */
#define	KT_WSEMA		0x00000100 /* waiting on a sema */
#define	KT_WMRLOCK		0x00000200 /* waiting on a mrlock */
#define	KT_ISEMA		0x00000400 /* waiting on an interrupt sema */
#define	KT_WUPD			0x00000800 /* waiting for mr(upd)lock */
#define	KT_PS			0x00001000 /* priority scheduled */
#define	KT_BIND			0x00002000 /* eligible for binding */
#define	KT_NPRMPT		0x00004000 /* do not kick scheduler */
#define	KT_BASEPS		0x00008000 /* orig priority scheduled */
#define KT_NBASEPRMPT		0x00010000 /* orig non-preemptive */
#define	KT_PSMR			0x00020000 /* ps because kernel mustrun */
#define	KT_HOLD			0x00040000 /* keep on this cpu */
#define	KT_PSPIN		0x00080000 /* ps because kernel pin_thread */
#define KT_INTERRUPTED		0x00100000 /* thread was interrupted */
#define	KT_SLEEP		0x00200000 /* thread asleep */
#define KT_NWAKE		0x00400000 /* thread cannot be interrupted */
#define KT_LTWAIT		0x00800000 /* long term wait */
#define KT_BHVINTR		0x01000000 /* thread interrupted for BHV lock */
#define	KT_SERVER		0x02000000 /* server thread */
#define KT_NOAFF		0x04000000 /* thread has no affinity */
#define KT_MUTEX_INHERIT	0x08000000 /* thread inherited through mutex */

#define KT_SYNCFLAGS \
  (KT_WSEMA|KT_WSVQUEUEING|KT_WSV|KT_WMUTEX|KT_INDIRECTQ|KT_WMRLOCK)

/* stack management */
#define	KTHREAD_DEF_STACKSZ	(1024*sizeof(void*))
#define ARGSAVEFRM	(((18*sizeof(k_machreg_t))+15) & ~0xf)

/*
 * environment types that kthread can support
 */
#define KT_XTHREAD	1
#define KT_STHREAD	2
#define KT_UTHREAD	3

/*
 * general kernel thread stucture 
 *
 * Locking:
 *  we use a bitlock on k_flags for all locking in the sync routines.
 *  This is used to protect, k_{f,b}link, k_wchan, k_prtn, k_flags.
 *
 */

/*
 * virtual kthread ops - used to switch out to the various environment
 * specific operations
 */
typedef struct kthread kthread_t;

typedef struct kt_ops_s {
	struct cred *	(*kt_get_cred)(kthread_t *);
	void		(*kt_set_cred)(kthread_t *, struct cred *);
	char *		(*kt_get_name)(kthread_t *);
	void		(*kt_update_inblock)(kthread_t *, long);
	void		(*kt_update_oublock)(kthread_t *, long);
	void		(*kt_update_msgsnd)(kthread_t *, long);
	void		(*kt_update_msgrcv)(kthread_t *, long);
} kt_ops_t;


/* 
 * BLA - used to remember what behavior locks are currently owned
 * by a thread.
 */
#ifdef CELL
#define KB_MAX_LOCKS	20		/* Max behavior locks ever held at a time.*/

typedef struct k_bla {
	kthread_t	*kb_flink;	/* link for doubly linked list */
	kthread_t	*kb_blink;	/* link for doubly linked list */
	mrlock_t	**kb_lockpp;	/* pointer to next slot in kb_lockp.
					   Kept in PDA while processing running */
	uint_t		kb_starvearg;	/* used to prevent starvation */
	uchar_t		kb_in_gbla;	/* indicates if BLA in is GBLA list */
	uchar_t		kb_growing;	/* Indicates if list is growing - 
					   last op was: 0=pop, 1 =push */
	mrlock_t	*kb_lockp[KB_MAX_LOCKS]; /* locks currently held */
#ifdef BLALOG
	char		kb_rwi[KB_MAX_LOCKS];     /* 0=access, 1=update, 2 |= intr */ 
	void		*kb_pc[KB_MAX_LOCKS];    /* PC that first set lock */
#endif
} k_bla_t;

#define KBLA_SAVE(kt) 						\
	if (private.p_blaptr) { 				\
		(kt)->k_bla.kb_lockpp = private.p_blaptr; 	\
		KBLA_VERIFY(kt);				\
	}
#define KBLA_CSAVE(kt) 						\
	if (private.p_blaptr && (kt)) {				\
		(kt)->k_bla.kb_lockpp = private.p_blaptr; 	\
		KBLA_VERIFY(kt);				\
	}
#define KBLA_RESUME(kt) {					\
	private.p_blaptr = (kt)->k_bla.kb_lockpp;		\
	KBLA_VERIFY(kt);					\
}
#define KBLA_CRESUME(kt) 					\
	if ((kt)) {	                 			\
		private.p_blaptr = (kt)->k_bla.kb_lockpp;	\
		KBLA_VERIFY(kt);				\
	}							\
        else                                                    \
		private.p_blaptr = NULL;
#define KBLA_VERIFY(kt)                                         \
	ASSERT(bla_klocksheld(kt)>=0 &&                         \
	       bla_klocksheld(kt) <= KB_MAX_LOCKS);
#define KBLA_EMPTY(kt)                                          \
        ASSERT(private.p_blaptr == (kt)->k_bla.kb_lockp);
#else  /* CELL */
#define KBLA_SAVE(kt)
#define KBLA_CSAVE(kt)
#define KBLA_RESUME(kt)
#define KBLA_CRESUME(kt)
#define KBLA_EMPTY(kt)
#endif /* CELL */

struct mrlock_s;

/*
 * Determines # of simultaneously held mrlocks recorded by one mria chunk.
 * By default every kthread gets one mria, so the following also determines the
 * number of mrlocks that can be locked w/o having to allocate extra chunks.
 */
#define MRI_MAXLINKS	8

typedef struct mri_s {
	struct mri_s	 *mri_flink;
	struct mri_s	 *mri_blink;
	struct mrlock_s  *mri_mrlock;
	uchar_t		 mri_index;  /* index of this mri in mria_mril  */
	uchar_t  	 mri_count;  /* # of recursive locks   		*/
	short		 mri_pri;    /* basepri at which queued 	*/
} mri_t;

typedef struct mria_s {
	struct kthread 	*mria_kt;   	/* needs to be before mria_mril */
	mri_t 		mria_mril[MRI_MAXLINKS];
	struct mria_s 	*mria_next; /* next chunk on overflow */ 
#ifdef DEBUG
	int		mria_count; /* # of distinct locks being held */
#endif
} mria_t;

#define MRI_TO_MRIA_KT_P(mrip) \
	(kthread_t  **)(((char *)(mrip)) \
			  - ((mrip)->mri_index * sizeof(mri_t))\
			  - sizeof(kthread_t *))

#define KTOP_GET_CURRENT_CRED() \
	(*curthreadp->k_ops->kt_get_cred)(curthreadp)
#define KTOP_GET_CRED(kt) \
	(*(kt)->k_ops->kt_get_cred)(kt)
#define KTOP_SET_CURRENT_CRED(cr) \
	(*curthreadp->k_ops->kt_set_cred)(curthreadp, cr)
#define KTOP_SET_CRED(kt,cr) \
	(*(kt)->k_ops->kt_set_cred)(kt, cr)
#define KTOP_GET_NAME(kt) \
	(*(kt)->k_ops->kt_get_name)(kt)
#define KTOP_UPDATE_CURRENT_INBLOCK(v) \
	(*curthreadp->k_ops->kt_update_inblock)(curthreadp, v)
#define KTOP_UPDATE_CURRENT_OUBLOCK(v) \
	(*curthreadp->k_ops->kt_update_oublock)(curthreadp, v)
#define KTOP_UPDATE_CURRENT_MSGSND(v) \
	(*curthreadp->k_ops->kt_update_msgsnd)(curthreadp, v)
#define KTOP_UPDATE_CURRENT_MSGRCV(v) \
	(*curthreadp->k_ops->kt_update_msgrcv)(curthreadp, v)


/*
 * Kthread data
 */
struct sthread_s;
struct bvinfo_s;
struct cpu_s;
struct job_s;

struct kthread {
	k_machreg_t	k_regs[NPCBREGS];
	uint64_t	k_id;		/* unique id */
	unchar		k_type;		/* type of thread environment */
	/* --- data elements used for synchronization */
	char		k_prtn;		/* semaphore return value */
	char		k_sqself;	/* If I'm using my stack */
	unchar		k_runcond;	/* Runq condition checks */
	/* rt enqueueing */
	cpuid_t		k_onrq;		/* runq we are on, -1 otherwise */
	cpuid_t		k_sonproc;	/* processor thread is running on */
	cpuid_t		k_binding; 	/* processor thread is bound to (rt) */
	cpuid_t		k_mustrun;	/* thread has to run only on this cpu */
	cpuid_t		k_lastrun;	/* cpu thread last ran on */
	uint_t		k_preempt;	/* is preempted */
	uint_t		k_flags;	/* thread flags */
	struct kthread	*k_flink;	/* linked list of sleeping threads */
	struct kthread	*k_blink;	/* linked list of sleeping threads */
	struct kthread	*k_rflink;	/* linked list of running threads */
	struct kthread	*k_rblink;	/* linked list of running threads */
	caddr_t		k_wchan;	/* Sleeping thread's sync object */
	caddr_t		k_w2chan;	/* Wait addr for sleeping thread.   */
	sv_t		k_timewait;	/* delay()... */
	struct monitor	*k_monitor;	/* pointer to current monitor */	
	mon_t		**k_activemonpp;/* active STREAMS monitor */	
	void		*k_inherit;	/* mutex or mrlock we inherited through */
	kthread_t	*k_indirectwait;/* threads blocked indirectly */
	caddr_t		k_stack;	/* stack area */
	caddr_t		k_stktop;	/* k_stack + k_stacksize - ARGSAVEFRM */
	uint_t		k_stacksize;	/* in bytes */ 
	short		k_cpuset;	/* cpuset id */
	short		k_nofault;	/* index into table of functions to */
					/* handle user space external memory */
					/* faults encountered in kernel */

	short		k_pri;		/* scheduling pri (maybe inherited) */
	short		k_basepri;	/* original thread priority */
	mria_t		k_mria;		/* Links for lists of holders of mrlocks */
	short		k_mrqpri;	/* Priority at which thread slept on mrlock */
	struct kthread	*k_link;	/* runq link */
	kt_ops_t	*k_ops;		/* ops for environment */
	eframe_t	*k_eframe;	/* exception frame if interrupted */
#if CELL_PREPARE
        __int64_t       k_cprepspace[6]; /* space for cell stuff: bla ptr, svc
                                            mesg_stack ptr, callback stash ptr
                                            plus 2 64-bit and 1 32-bit fields,
                                            just in case. */
#elif CELL
	k_bla_t		k_bla;		/* BLA for remembering behavior locks held*/
	service_t	k_remote_svc;	/* Where this thread has gone */
	void		*k_mesg_stack;	/* Stack of above if callbacks occur */
#endif /* CELL */
        /*
         * Memory affinity
         */
        struct mld      *k_mldlink;     /* Preferred MLD to run this kt */
	cnodeid_t	k_affnode;
        cnodemask_t	k_maffbv;       /* Other reasonable nodes to run this kt */
        cnodemask_t	k_nodemask;	/* Nodes to get memory from or run this kt */
	ktimerpkg_t	k_timers;	/* accounting state timers */
	int    		k_qkey;   	/* sort key used by sync variables */
	short		k_copri;	/* Callout base timein thread priority */
};
#pragma set field attribute struct kthread k_pri align=128


/* Prototype for kernel thread function
 * This is a a function taking a (void *) as an arg, and returning a void.
 */
typedef void (kt_func_t)(void *);

/* some macros */
#define curthreadp (private.p_curkthread)

/*
 * Map kernel thread ID (uint64_t) into process ID (pid_t == int32_t) space
 * for use in exporting a kernel thread ID outside the kernel.  This is
 * needed because most user level applications aren't yet prpared to cope
 * with anything other than a pid_t for an execution context ID.  Moreover,
 * many applications only pay attention to the low 16 bits.  For right now,
 * since MAXPID is currently 30000 and we're not going to have more than
 * 32767 kernel threads, we can get away with simply using the low portion
 * of the kernel thread ID (they start at 0x100000001 and go upwards) and
 * negate it.  All of this will obviously need to be fixed in a real manner
 * in the future (probably by getting all these applications to accept
 * 64-bit IDs) but this gets us by for now.
 */
#define kidtopid(kid)	((pid_t)(-(int16_t)(kid)))
#define pidtokid(pid)	(0x100000000LL + (uint64_t)(-(int16_t)(pid)))
#define iskpid(pid)	((int16_t)(pid) < 0)

/*
 * lock k_flags */
#define kt_lock(k)	(mutex_bitlock(&(k)->k_flags, KT_LOCK))
#define kt_unlock(k,rv)	(mutex_bitunlock(&(k)->k_flags, KT_LOCK, rv))
#define kt_islocked(k) 	(bitlock_islocked(&(k)->k_flags, KT_LOCK))
#define kt_nested_trylock(k) (nested_bittrylock(&(k)->k_flags, KT_LOCK))

#define kt_fset(k,b)		bitlock_set(&(k)->k_flags, KT_LOCK, b)
#define kt_fclr(k,b)		bitlock_clr(&(k)->k_flags, KT_LOCK, b)

#define kt_timedwait(kt,f,rv,svtimer_flags,ts,rts) \
	 sv_bitlock_timedwait(&(kt)->k_timewait, f, &kt->k_flags, KT_LOCK, \
				rv, svtimer_flags, ts, rts)

#ifdef MP
#define kt_nested_lock(k)	(nested_bitlock(&(k)->k_flags, KT_LOCK))
#define kt_nested_unlock(k)	(nested_bitunlock(&(k)->k_flags, KT_LOCK))
#else
#define kt_nested_lock(k)
#define kt_nested_unlock(k)
#endif

/*
 * Environment predicates - each kthread has an associated environment when
 * it runs. Currently that is either a unix process or a service thread.
 */
#define KT_CUR_ISUTHREAD()	(curthreadp->k_type == KT_UTHREAD)
#define KT_CUR_ISSTHREAD()	(curthreadp->k_type == KT_STHREAD)
#define KT_CUR_ISXTHREAD()	(curthreadp->k_type == KT_XTHREAD)
#define KT_ISUTHREAD(k)		((k)->k_type == KT_UTHREAD)
#define KT_ISSTHREAD(k)		((k)->k_type == KT_STHREAD)
#define KT_ISXTHREAD(k)		((k)->k_type == KT_XTHREAD)

#define	KT_ISMR(kt)		((kt)->k_mustrun != PDA_RUNANYWHERE)
#define	KT_ISBOUND(kt)		((kt)->k_binding != CPU_NONE)
#define	KT_PSKERN		(KT_PSMR|KT_PSPIN)

#define KT_ISBASERT(kt)		((kt)->k_basepri >= 0)
#define KT_ISRT(kt)		((kt)->k_pri >= 0)
#define	KT_ISBASEPS(kt)		((kt)->k_flags & KT_BASEPS)
#define	KT_ISPS(kt)		((kt)->k_flags & KT_PS)
#define	KT_ISNPRMPT(kt)		((kt)->k_flags & KT_NPRMPT)
#define KT_ISNBASEPRMPT(kt)	((kt)->k_flags & KT_NBASEPRMPT)
#define	KT_ISKB(kt)		((kt)->k_flags & KT_PSKERN)
#define KT_ISAFF(kt)		(!((kt)->k_flags & KT_NOAFF))
#define	KT_ISHELD(kt)		((kt)->k_flags & KT_HOLD)
#define	KT_ISLOCAL(kt)		(KT_ISMR(kt) || KT_ISKB(kt) || KT_ISBOUND(kt))


#define	kt_pri(kt)		((kt)->k_pri)
#define	kt_basepri(kt)		((kt)->k_basepri)
#define	kt_initialize_pri(kt,p)	((kt)->k_pri = (kt)->k_basepri = (p))
#define	is_weightless(kt)	((kt)->k_basepri <= PWEIGHTLESS)
#define is_batch(kt)		((kt)->k_basepri == PBATCH)
#define is_bcritical(kt)	((kt)->k_basepri == PBATCH_CRITICAL)

#define	INVERT_PRI(p)		(128 - (p))

#define thread_interrupted(kt)	((kt)->k_flags & KT_INTERRUPTED)
#define thread_interruptible(kt) (((kt)->k_flags&(KT_SLEEP|KT_NWAKE))==KT_SLEEP)

#ifdef _KERNEL
static __inline void
thread_interrupt_clear(
	struct kthread	*kt,
	int		locked)
{
	if (locked)
		kt->k_flags &= ~KT_INTERRUPTED;
	else
		kt_fclr((kt), KT_INTERRUPTED);
}
#endif

/* function prototypes */
extern int	thread_interrupt(struct kthread *, int *);
extern void	kthread_init(kthread_t *, caddr_t, uint_t, int, pri_t,
				uint_t, kt_ops_t *);
extern void	kthread_destroy(kthread_t *);
extern uint64_t	kthread_getid(void);
extern int	kt_getcfg(kthread_t *, int, int);

extern void	setrun(kthread_t *);

extern cnodemask_t get_effective_nodemask(kthread_t *);

#endif
