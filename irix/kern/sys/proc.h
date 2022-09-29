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
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#ifndef __SYS_PROC_H__
#define __SYS_PROC_H__

#ident	"$Revision: 3.475 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/kabi.h>
#include <sys/extacct.h>

#if defined(_KERNEL) || defined(_KMEMUSER)

struct kusharena;

#if _KERNEL
#include <sys/kthread.h>
#include <sys/uthread.h>
#endif

/* p_itimerunit */
#define FAST_TICK	1
#define SLOW_TICK	0

/* p_flag codes */

#define	STATELOCK   0x00000001	/* Lock bit for flags, etc. */
#define	STRC	    0x00000002	/* Process is being traced.	*/
#define SIGNORED    0x00000004	/* process ignored by parent [see below] */
#define SBBLST	    0x00000008	/* it's been blasted by sched deadlock scan */
#define SPARSWTCH   0x00000010	/* tracing rescheduling via par */
#define SPROCTR     0x00000020	/* sysent/exit/fault/signal tracing via /proc */
#define SJSTOP      0x00000040	/* process in job control stop */
#define SPROPEN	    0x00000080	/* process is open via /proc	*/
#define	SPARSYS	    0x00000100	/* tracing sys calls via par */
#define	SPARINH	    0x00000200	/* par syscall tracing inherited by children */
#define	SPARPRIV    0x00000400	/* par syscall tracing by privileged user */
#define SPGJCL      0x00000800	/* process prevents its pgrp being orphaned */
#define SWSRCH	    0x00001000	/* process is scanning in wait */
#define	SCKPT	    0x00002000	/* process is being stopped for checkpoint  */
#define	SGRACE	    0x00004000  /* process has reached its CPU time limit   */
				/* SIGXCPU has already been send	    */
				/* after the grace time expires, SIGKILL    */
				/* will be send				    */
		 /* 0x00008000 unused */
#define	SNOCTTY	    0x00010000	/* no control terminal */
#define SFDT	    0x00020000	/* finish open file processing */
#define SPROFFAST   0x00040000	/* profiling process 10 times per tick */
#define SPROF32	    0x00080000	/* profiling process uses 32-bit buckets */
#define SPROF	    0x00100000	/* process is profiling */
#define SCEXIT	    0x00200000	/* process will be SIGHUP'd when parent exits */
#define SEXECED	    0x00400000	/* process successfully exec'ed (for setpgid) */
#define SCOREPID    0x00800000	/* when dumping core, add pid */
		 /* 0x01000000 unused */
#define SPARDIS	    0x02000000	/* par tracing disallowed */
#define	SWILLEXIT   0x04000000  /* share group member will exit */
		 /* 0x08000000 unused */
#define SPRPROTECT  0x10000000	/* process access protected */
#define SABORTSIG   0x40000000	/* signal on abort */
		 /* 0x80000000 unused */

#define PTRACED(p)	((p)->p_flag & (STRC|SPROCTR|SPROPEN))


/*
 * SIGNORED: parent has exited or is otherwise uninterested in
 * its child's exit status.  Set only by the parent when it exits,
 * or by the child when it begins to exit and finds that the parent
 * does not want SIGCLD.  Once set, this bit is never cleared.
 */

/*
 * The defines for prxy_abi have been moved to kabi.  This is to allow
 * kernel modules which do not include proc.h but include xlate.h,
 * which needs these defines, to compile.
 */

#if (_MIPS_SZPTR == 32)
#define	ABI_ICODE	ABI_IRIX5	/* ABI for icode init */
#else
#define	ABI_ICODE	ABI_IRIX5_64	/* ABI for icode init */
#endif

#define	_MIN_ABI	ABI_IRIX5	/* lower bound of syscallsw[] */
#define	_MAX_ABI	ABI_IRIX5_N32	/* upper bound of syscallsw[] */

/* flags for p_fp */
#define	P_FP_SIGINTR1	1
#define	P_FP_SIGINTR2	2
#endif /* _KERNEL || _KMEMUSER */

/* stat codes */
#define	SINVAL	0		/* Gone.  Forgotten.		*/
#define	SRUN	1		/* Running.			*/
#define	SZOMB	2		/* Process terminated but parent*/
				/* hasn't wait(2)ed for it yet.	*/

/* Reasons for stopping (values of ut_whystop) */
#define	REQUESTED	1
#define	SIGNALLED	2
#define	SYSENTRY	3
#define	SYSEXIT		4
#define FAULTED		5
#define JOBCONTROL	6
#define	CHECKPOINT	7

/* Reasons for stopping (p_whatstop) */
#define FAULTEDWATCH	1	/* whystop == FAULTED */
#define FAULTEDKWATCH	2	/* whystop == FAULTED */
#define FAULTEDPAGEIN	3	/* whystop == FAULTED */
#define FAULTEDSTACK	4	/* whystop == FAULTED */
#define FAULTEDSCWATCH	5	/* whystop == FAULTED */

/* flags for proc_fp_t pfp_fpflags */
#define	P_FP_IMPRECISE_EXCP	0x01	/* TFP imprecise expeptions */
#define	P_FP_PRESERVE		0x02	/* Preserve p_fpflags across exec */
#define	P_FP_FR			0x04	/* SR_FR bit is set for this process */
#define	P_FP_SMM		0x08	/* FPU is in Sequential Memory Mode */
#define	P_FP_SMM_HDR		0x10	/* SMM specified in elf header */
#define	P_FP_PRECISE_EXCP_HDR	0x20	/* prec exc specified in elf header */
#define P_FP_SPECULATIVE	0x40	/* process executing speculatively */

#ifdef _KERNEL
/* Special delivery instructions for issig() and fsig() */
#define SIG_ALLSIGNALS		0x0
#define SIG_NOSTOPDEFAULT	0x1	/* Don't deliver stopdefault signals */

extern int		npalloc;	/* Number of proc structs allocated */

union rval;
struct rusage;
struct vproc;
struct proc;
struct k_siginfo;
struct cred;

/*
 * To deliver a signal to a process, use sigtopid.
 *
 *	pid: obviously, the pid of the target process;
 *	signal: the signal to be delivered
 *	flags:	SIG_*, defined below
 *	sid: session id
 *	cred: credentials checked if not SIG_ISKERN|SIG_HAVEPERM
 *	siginfo: optional k_siginfo structure
 *
 * Note that sigtopid can sleep unless the SIG_NOSLEEP flag is passed.
 *
 * If SIG_NOSLEEP is passed, the signal will be delivered asynchronously
 * through an intermediary, and the only errors which can be returned are:
 *	EAGAIN: the intermediary's request buffer is full;
 *	EPERM: the SIG_ISKERN flag wasn't also specified.
 *
 * Also note that siginfos are dropped when SIG_NOSLEEP is specified.
 *
 *
 * To deliver a signal to a process thread, use sigtouthread.
 * The signal (and optional siginfo) will be delivered to the
 * specified uthread.  Note that this call could sleep.
 */

/* Flags for sigtopid */

#define SIG_HAVPERM		0x01	/* sending proc has appropriate perms */
#define SIG_ISKERN		0x02	/* kernel is sending sig */
#define SIG_SIGQUEUE		0x04	/* This is a sigqueue call */
#define SIG_HUPCONT		0x08	/* SIGHUP/SIGCONT POSIX combo */
#define SIG_NOSLEEP		0x10	/* delivery without sleeping */
#define	SIG_TIME		0x20	/* reserved for internal use */

extern int sigtopid(pid_t pid, int signal, int flags, pid_t sid,
			struct cred *cred, struct k_siginfo *siginfo);
extern void sigtouthread(struct uthread_s *uthread, int sig, k_siginfo_t *);

extern int newproc(void);
extern int procscan(int ((*)(struct proc *, void *, int)), void *);
extern int activeproccount(void);
extern int ancestor(pid_t, pid_t);

extern struct vproc *ptrsrch(struct proc *, pid_t);

struct sigqueue;
struct sigvec_s;
struct sigpend;
struct sysent;

extern struct sigqueue *sigdeq(struct sigpend *, int, struct sigvec_s *);
extern int ksigqueue(struct vproc *, int, int, const union sigval, int);
extern int issig(int, int);
extern void assign_cursighold(k_sigset_t *, k_sigset_t *);
extern int psig(int *, struct sysent *);
extern int checkstop(void);
extern int wait_checkstop(void);
extern void stop(uthread_t *, ushort, ushort, int);
extern int fsig(uthread_t *, struct sigvec_s *, int);
extern void checkfp(struct uthread_s *, int);
extern int vrelvm(void);
extern int procp_is_valid(struct proc *);
extern int uthreadp_is_valid(uthread_t *);

/* context switch routines */
extern void kpswtch(void);
extern void qswtch(int);
extern void swtch(int);
extern void user_resched(int);
extern void ut0exitswtch(void);
extern void utexitswtch(void);

extern int save(k_machreg_t *);
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 730))
#pragma unknown_control_flow (save)
#endif
#if R4000 || R10000
extern void resume(uthread_t *, kthread_t *, unsigned char);
#endif
#if TFP || PSEUDO_BEAST
extern void resume(uthread_t *, kthread_t *, unsigned char, unsigned char);
#endif
extern void resumeidle(kthread_t *);

/*
 * shared process descriptor
 * Each process that may have more than one thread of execution has one
 */
typedef struct pshare_s {

	/* semaphore for single threading open directory updating */
	mutex_t		ps_fupdsema;	/* wait for opening directory */

	struct vnode	*ps_cdir;	/* current directory */
	struct vnode	*ps_rdir;	/* root directory */

	/* lock for updating misc things that don't need a sleeping lock */
	lock_t		ps_rupdlock;	/* update lock */

	/* hold values for other sharing options */
	short		ps_cmask;	/* mask for file creation */
	struct cred	*ps_cred;	/* shared ID's */
	k_sigset_t	ps_sighold;	/* sighold used during u-creation */

        /* data for single-threading the single stepping fucntion */
        mutex_t         ps_stepmutex;   /* mutex for single step data. */
        int             ps_stepflags;   /* flags for single-step */
        sv_t            ps_stepsv;      /* for waiting to step */
        uthread_t       *ps_steput;     /* uthread with step right. */
} pshare_t;

#define PSSTEP_GOTTEN  0x01	/* right to stop obtained by a thread */
#define PSSTEP_WAIT    0x02     /* one or more threads waiting */

typedef struct proc_fp_s {
	caddr_t		pfp_nofpefrom;	/* disabled fp exception range */
	caddr_t		pfp_nofpeto;	/* disabled fp exception range */
	int		pfp_dismissed_exc_cnt;	/* how many speculative instr.
						 * have caused exceptions */
	u_char		pfp_fpflags;	/* fp emulation flags */
	char		pfp_fp;		/* generate SIGFPE on fp interrupts */
} proc_fp_t;

/*
 * Process stats accounting is accumulated per-uthread but is reported
 * (via VPROC_GET_EXTACCT) for the process by summing over uthreads.
 * When each uthread exits, it dumps its stats into the prxy_exit_acct
 * structure of the proc proxy.
 */
typedef struct proc_acct_s {
	struct ut_acct_s	utsum;
} proc_acct_t;
#define pr_mem			utsum.ua_mem
#define pr_ioch			utsum.ua_ioch
#define pr_bread		utsum.ua_bread
#define pr_bwrit		utsum.ua_bwrit
#define pr_sysc			utsum.ua_sysc
#define pr_syscr		utsum.ua_syscr
#define pr_syscw		utsum.ua_syscw
#define pr_syscps		utsum.ua_syscps
#define pr_sysci		utsum.ua_sysci
#define pr_graphfifo		utsum.ua_graphfifo
#define pr_tfaults		utsum.ua_tfaults
#define pr_vfaults		utsum.ua_vfaults
#define pr_ufaults		utsum.ua_ufaults
#define pr_kfaults		utsum.ua_kfaults

#define prxy_exit_mem		prxy_exit_acct.pr_mem
#define prxy_exit_ioch		prxy_exit_acct.pr_ioch
#define prxy_exit_bread		prxy_exit_acct.pr_bread
#define prxy_exit_bwrit		prxy_exit_acct.pr_bwrit
#define prxy_exit_sysc		prxy_exit_acct.pr_sysc
#define prxy_exit_syscr		prxy_exit_acct.pr_syscr
#define prxy_exit_syscw		prxy_exit_acct.pr_syscw
#define prxy_exit_syscps	prxy_exit_acct.pr_syscps
#define prxy_exit_sysci		prxy_exit_acct.pr_sysci
#define prxy_exit_graphfifo	prxy_exit_acct.pr_graphfifo
#define prxy_exit_tfaults	prxy_exit_acct.pr_tfaults
#define prxy_exit_vfaults	prxy_exit_acct.pr_vfaults
#define prxy_exit_ufaults	prxy_exit_acct.pr_ufaults
#define prxy_exit_kfaults	prxy_exit_acct.pr_kfaults

typedef struct proc_sched_s {
	uint		prs_flags;	/* Scheduling flags */
	char		prs_nice;	/* nice for cpu usage */
	uchar_t		prs_policy;	/* process scheduling policy */
	short		prs_priority;	/* process scheduling priority */
	struct job_s	*prs_job;	/* default job for process */
} proc_sched_t;

#define	PRSLOCK			0x1	/* Lock bit for prs_flags */
#define	PRSNOAFF		0x2	/* No affinity */
#define PRSBATCH		0x4	/* Batch process */

#define prs_lock(prs)		mutex_bitlock(&(prs)->prs_flags, PRSLOCK)
#define prs_unlock(prs, rv)	mutex_bitunlock(&(prs)->prs_flags, PRSLOCK, rv)
#define prs_flagset(prs, b)	bitlock_set(&(prs)->prs_flags, PRSLOCK, b)
#define	prs_flagclr(prs, b)	bitlock_clr(&(prs)->prs_flags, PRSLOCK, b)

/*
 * The 'proc proxy' structure. This structure represents a local copy
 * of some of the data found in the proc structure. It falls into
 * 2 catagories: read-only or write-mostly. The read-only data requires
 * no global synchronization. The write-mostly data is synchronized when
 * required. For instance, the accounting fields represent the resource
 * usage of the uthreads attached to this proc proxy. When those counters
 * are required, they are summed across all of the proc proxies.
 */

typedef struct proc_proxy_s {
	struct syscallsw *prxy_syscall;	/* pointer to syscall handler info */
	pshare_t	*prxy_utshare;	/* pointer to shared uthread desc */

	/* thread management */
	mrlock_t	prxy_thrdlock;	/* lock for uthread list */
	sv_t		prxy_thrdwait;	/* wait for last exit */
	uthread_t	*prxy_threads;	/* threads attached to this proxy */
	uthreadid_t	prxy_nthreads;	/* # of uthreads attached to this prxy*/
	uthreadid_t	prxy_utidlow;	/* start allocating uthread ids here */
        int             prxy_jscount;   /* count for joint stop logic */
	int		prxy_nlive;	/* # of uthreads not exiting */

	/* etc */
	int		prxy_flags;	/* flags */
	int		prxy_hold;	/* prevent uthreads from detaching */
	u_char		prxy_abi;	/* Our current abi */
	uint		prxy_shmask;	/* share group share mask */

	/* Accounting stuff */
#if R10000
	perf_mon_t	prxy_perfmon;	/* performance monitoring structure */
#define prxy_cpumon prxy_perfmon.pm_cpu_mon
#endif
	struct rusage	prxy_ru;	/* proc stats for this proxy */
	proc_acct_t	prxy_exit_acct;	/* acct and extacct info at exit */
	acct_timers_t	prxy_exit_accum; /* uthreads dump timers here at exit */

	/* sched stuff */
	proc_sched_t	prxy_sched;	/* per-process sched info */

	proc_fp_t	prxy_fp;	/* floating point stuff */

	/*
	 * sigstack and sigaltstack fields.
	 * None of these fields are subject to global synchronization -
	 * they are treated as execution context, and thus could be in
	 * the uthread. Since their use is not defined on multi-threaded
	 * processes, these fields are placed in the proxy structure
	 * rather than the uthread to save some space.
	 */
	caddr_t		prxy_siglb;	/* lower bound of sigstack */
	stack_t		prxy_sigstack;	/* sp & on stack state variable */
#define prxy_ssflags	prxy_sigstack.ss_flags
#define prxy_sigsp	prxy_sigstack.ss_sp
#define prxy_spsize	prxy_sigstack.ss_size
	void		*prxy_oldcontext;	/* previous user context */
	k_sigset_t	prxy_sigonstack;	/* use sigstack for signals */

	int		(*prxy_sigtramp)();	/* addr of user's signal return
						 * handler: _sigtramp in libc */

	struct vnode	*prxy_coredump_vp;	/* corefile vp */
	off_t		prxy_coredump_fileoff;	/* next thread's data offset */
						/* in file */
	mutex_t		prxy_coredump_mutex;	/* held while thread */
						/* is dumping */
	int		prxy_coredump_nthreads;	/* next thread's index */
						/* in corefile */

	/*
	 * Information to undo system V semaphore operations on exit
	 */
	mutex_t		prxy_semlock;
	void		*prxy_semundo;	/* sem undo information for semexit */
	uthread_t	*prxy_sigthread;/* event signals recipient */
} proc_proxy_t;

#define	PRXY_LOCK	0x00000001	/* Lock bit for prxy_flags */
#define	PRXY_EXIT	0x00000002	/* Process exiting */
#define	PRXY_EXEC	0x00000004	/* Process execing */
#define PRXY_SPROC	0x00000008	/* This process is in a share group */
#define PRXY_SPIPE	0x00000010	/* turn on stream pipe */
#define PRXY_USERVME	0x00000020	/* process has/had VME space mapped */
#define PRXY_WAIT	0x00000040	/* waiting for prxy_rele */
#define PRXY_JSTOP      0x00000080      /* threads of process stop together */
#define PRXY_JPOLL      0x00000100      /* poll waits for thread to stop */
#define PRXY_PWAIT      0x00000200      /* waiting for all threads to stop */
#define PRXY_PSTOP      0x00000400      /* trying to stop all threads */
#define PRXY_JSARMED    0x00000800      /* joint stop operation begun */
#define PRXY_JSTOPPED   0x00001000      /* set when joint stop finishes */
#define PRXY_NOHANG	0x00002000	/* Don't hang on dead NFS */
#define PRXY_RLCKDEBUG	0x00004000	/* For testing NOHANG */
#define PRXY_RWLCKDEBUG	0x00008000	/* For testing NOHANG */
#define PRXY_LONEWT	0x00010000	/* thrd waiting to be alone */
#define PRXY_CREATWT	0x00020000	/* make new thrd creators wait */

#define prxy_lock(prxy) \
		mutex_bitlock((uint *)(&(prxy)->prxy_flags), PRXY_LOCK)
#define prxy_unlock(prxy, rv)	\
		mutex_bitunlock((uint *)(&(prxy)->prxy_flags), PRXY_LOCK, rv)
#define prxy_wait(prxy,sv,rv)	\
		sv_bitlock_wait(sv, 0, (uint *)(&(prxy)->prxy_flags), \
				PRXY_LOCK, rv)
#define	prxy_stoplock		prxy_lock
#define	prxy_stopunlock		prxy_unlock
#define	prxy_hlock		prxy_lock
#define	prxy_hunlock		prxy_unlock
#define	nuscan_hold(prxy)	prxy->prxy_hold++;

#define prxy_flagset(prxy, b) \
		bitlock_set((uint *)(&(prxy)->prxy_flags), PRXY_LOCK, b)
#define	prxy_flagclr(prxy, b) \
		bitlock_clr((uint *)(&(prxy)->prxy_flags), PRXY_LOCK, b)
#define	prxy_nflagset(prxy, b)	prxy->prxy_flags |= (b)
#define	prxy_nflagclr(prxy, b)	prxy->prxy_flags &= ~(b)

#define uscan_update(prxy)	mrupdate(&(prxy)->prxy_thrdlock)
#define uscan_access(prxy)	mraccess(&(prxy)->prxy_thrdlock)
#define uscan_tryupdate(prxy)	mrtryupdate(&(prxy)->prxy_thrdlock)
#define uscan_tryaccess(prxy)	mrtryaccess(&(prxy)->prxy_thrdlock)
#define uscan_update_held(prxy)	mrislocked_update(&(prxy)->prxy_thrdlock)
#define uscan_access_held(prxy)	mrislocked_access(&(prxy)->prxy_thrdlock)
#define uscan_unlock(prxy)	mrunlock(&(prxy)->prxy_thrdlock)
#define uscan_wait(prxy, s)	sv_bitlock_wait(&(prxy)->prxy_thrdwait,\
				0, (uint *)(&(prxy)->prxy_flags), \
				PRXY_LOCK, s)
#define uscan_wake(prxy)	sv_broadcast(&(prxy)->prxy_thrdwait)
#define	lone_wait(prxy)		sv_mrlock_wait(&(prxy)->prxy_thrdwait, \
				0, &(prxy)->prxy_thrdlock)
#define	lone_wake(prxy)		sv_broadcast(&(prxy)->prxy_thrdwait)
#define create_hold(prxy)	prxy_flagset(prxy, PRXY_CREATWT) /*no ref count*/
#define create_rele(prxy)	{ \
					if (prxy->prxy_flags & PRXY_CREATWT) { \
						prxy_flagclr(prxy, PRXY_CREATWT);\
						sv_broadcast(&(prxy)-> \
							prxy_thrdwait); \
					} \
				}
#define create_wait(prxy)	sv_mrlock_wait(&(prxy)->prxy_thrdwait, 0, \
				&(prxy)->prxy_thrdlock)
#define uscan_held(prxy)	(uscan_update_held(prxy) || \
						uscan_access_held(prxy))
#define prxy_to_thread(prxy)	(ASSERT((!(IS_THREADED((prxy)))) || \
				((prxy)->prxy_hold) || (uscan_held((prxy)))), \
				(prxy)->prxy_threads)
#define uscan_forloop(prxy, ut) for ((ut) = (prxy)->prxy_threads; \
                                     (ut) != NULL; \
                                     (ut) = (ut)->ut_next)
extern void uscan_hold(proc_proxy_t *);
extern void uscan_rele(proc_proxy_t *);

/*
 * shared group descriptor
 * Each process that sproc()s has one of these
 *
 * Locks/Semaphores:
 *
 *	s_listlock - also for modifying/accessing the list s_plink: use
 *		shared lock for long term access, or use this for short term
 *
 * 	s_detachsem - a mutex semaphore that serializes access to parts of
 *		detachshaddr(). If a P() on this semaphore is successful,
 *		then, guaranteed that no member of the share group may
 *		detach regions of the address space.
 */

typedef struct shaddr_s {
	/* generic fields for handling share groups */
	pid_t		s_orig;		/* creater's pid (or 0) */
	struct proc	*s_plink;	/* link to processes in share group */
	mutex_t		s_listlock;	/* protects s_plink */
	ushort		s_refcnt;	/* # process in list */
	/* semaphore for single threading open directory updating */
	mutex_t		s_fupdsema;	/* wait for opening directory */
	/* lock for updating misc things that don't need a sleeping lock */
	lock_t		s_rupdlock;	/* update lock */
	struct fdt	*s_fdt;		/* file descriptor table */
	struct vnode	*s_cdir;	/* current directory */
	struct vnode	*s_rdir;	/* root directory */
	/* hold values for other sharing options */
	unchar		s_sched;	/* scheduling mode of share gp */
	short		s_cmask;	/* mask for file creation */
	off_t		s_limit;	/* maximum write address */
	struct cred	*s_cred;	/* shared ID's */
	/* share group scheduling information (protected by s_rupdlock) */
	pid_t		s_master;	/* proc that runs when in master mode */
	struct gdb_s	*s_gdb;		/* gang descriptor block */
	/* mutex for single threading parts of detachshaddr() */
	mutex_t		s_detachsem;
	sv_t		s_detached;	/* wait for last detach */
#ifdef CKPT
	struct ckpt_shm	*s_ckptshm;	/* shared mem tracking */
#endif
} shaddr_t;

#define	IS_SPROC(prxy)	((prxy)->prxy_flags & PRXY_SPROC)
#define IS_THREADED(prxy)	((prxy)->prxy_shmask & PR_THREADS)

#define ISSHDFD(prxy)	((prxy)->prxy_shmask & PR_SFDS)

/* flags for detachshaddr */

#define DETACH_REASON(x)	((x) & 0xff)

#define EXIT_CODE(x)	((x)<<8)	/* siginfo code passed with signal if */
#define EXIT_DECODE(x)	((x)>>8)	/* PR_SETEXITSIG or PR_SETABORTSIG */

/* reason for detachshaddr */
#define SHDEXEC		1	/* process is execing */
#define SHDEXIT		2	/* process is exiting */

extern void setshdsync(shaddr_t *, struct proc *, int, int);
extern void setpsync(struct proc_proxy_s *, int);
extern int getshdpids(struct proc *, pid_t *, int);
extern void dousync(int);
extern void credsync(void);

extern void allocpshare(void);
extern void detachpshare(struct proc_proxy_s *);

struct cred;
extern int hasprocperm(struct proc *, struct cred *);

#include <sys/ksignal.h>		/* for NUMSIGS, sigpend */
typedef struct sigvec_s {
	mrlock_t	sv_lock;	/* access/update lock */
	uint		sv_flags;	/* assorted process bits */
	sigpend_t	sv_sigpend;	/* pending signals and siginfos */
	k_sigset_t	sv_sigign;	/* signals to be ignored */
	k_sigset_t	sv_sigcatch;	/* signals to be caught */
	k_sigset_t	sv_sigrestart;	/* signals to restart sys calls */
	k_sigset_t	sv_signodefer;	/* signals deferred when caught */
	k_sigset_t	sv_sigresethand;/* signals reset when caught */
	k_sigset_t	sv_sainfo;	/* signals delivered w/ siginfo */
	k_sigset_t	sv_sighold;	/* ut_sighold points here or to prda */
	void		(*sv_hndlr[NUMSIGS])();	/* signal dispositions */
	k_sigset_t	sv_sigmasks[NUMSIGS];	/* mask during handlers */
	int		sv_pending;	/* procs current # of pending signals */
} sigvec_t;

#define sv_sig		sv_sigpend.s_sig
#define sv_sigqueue	sv_sigpend.s_sigqueue

/* sv_flags */

#define SNOCLDSTOP	0x00000002	/* SIGCHLD not sent when child stops */
#define SNOWAIT		0x00000004	/* children never become zombies */

#define sigvec_lock(sv)		mrupdate(&(sv)->sv_lock)
#define sigvec_acclock(sv)	mraccess(&(sv)->sv_lock)
#define sigvec_unlock(sv)	mrunlock(&(sv)->sv_lock)
#define sigvec_is_locked(sv) 	mrislocked_any(&(sv)->sv_lock)

/*
 * Exit callback mechanism:
 * register a function to be called when a particular thread/process exits
 */
struct exit_callback {
	struct exit_callback *ecb_next;
	void (* ecb_func)(void *);
	void *ecb_arg;
};

/*
 * Asynchronous daemon -- queued service requests.
 *
 * Callers which want to register a call for asynchronous delivery
 * set the function address in the async_vec_s structure and push
 * argument data into the async_arg union.
 */

typedef struct _async_sig {	/* deliver async signal */
	pid_t	s_pid;
	int 	s_sig;
	time_t	s_time;
} _async_sig_t;

typedef struct async_vec_s {
	void	(*async_func)(struct async_vec_s *);	/* service routine */
	union {
		_async_sig_t	async_signal;	/* sigtopid */
		long		async_null;	/* nada (prawake) */
	} async_arg;
} async_vec_t;

typedef void (*async_func_t)(struct async_vec_s *);

extern int async_call(async_vec_t *);	/* lodge async request */

#endif /* _KERNEL */

#endif /* __SYS_PROC_H__ */
