/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	actual or intended publication of such source code.	*/
#ifndef __SYS_UTHREAD_H__
#define __SYS_UTHREAD_H__

#ident	"$Revision: 1.82 $"

#include <sys/types.h>
#include <sys/ksignal.h>
#include <sys/flock.h>
#include <sys/kabi.h>
#include <sys/kthread.h>
#include <sys/resource.h>

#define curuthread	(private.p_curuthread)

#define KT_TO_UT(kt)	(ASSERT(KT_ISUTHREAD(kt)), (uthread_t *)(kt))
#define UT_TO_KT(ut)	((kthread_t *)(ut))
#define UT_TO_PROC(ut)	((ut)->ut_proc)
#define UT_TO_VPROC(ut)	((ut)->ut_vproc)

#define PSCOMSIZ	32	/* program being execed size */
#define PSARGSZ		80	/* exec arguments size */

union pde;

/*
 * values managed(indirectly) by the Address Space manager
 */
typedef unsigned char tlbpid_t;
typedef unsigned char icachepid_t;
#define tlbpid(u)		(*((u)->utas_tlbpid + cpuid()))
#if TFP
#define icachepid(u)		(*((u)->utas_icachepid + cpuid()))
#endif	/* TFP */
#if _MIPS_SIM == _ABI64
/* In 64-bit kernels all memory is directly addressable, so there's no need
 * to maintain a separate entry for the wired address of the segtbl(s).
 */
#define utas_segtbl_wired          utas_segtbl
#define utas_shrdsegtbl_wired      utas_shrdsegtbl
#endif

/* utas_segflags */
#define PSEG_SEG        0x1     /* use segment table */
#define PSEG_TRILEVEL   0x2     /* use tri-level segment table */

typedef struct utas_s {
	int64_t		utas_tlbid;	/* unique id for all time */
	tlbpid_t	*utas_tlbpid;	/* pointer to tlbpid array */
	icachepid_t	*utas_icachepid;/* pointer to itlbpid array */
	union pde	**utas_segtbl;	/* primary segment table pointer */
	union pde	**utas_shrdsegtbl;/* overlap segment table pointer */
#if _MIPS_SIM != _ABI64
	union pde	**utas_segtbl_wired;/* wired VA for p_segtbl */
	union pde	**utas_shrdsegtbl_wired;/* wired VA for p_shrdsegtbl */
#endif
	ushort		utas_segflags;	/* segment table flags */
	unsigned int	utas_tlbcnt;	/* count of random TLB dropins	*/
					/* of second-level misses	*/
	uint_t		utas_flag;	/* flags & bitlock */
	int		utas_utlbmissswtch;/* utlbmiss handler index */
	struct uthread_s *utas_ut;	/* XXX kill me */
} utas_t;

/* utas_flags */
#define UTAS_LCK	0x0001
#define	UTAS_LPG_TLBMISS_SWTCH   0x0002	/* swtch to large page tlbmiss hndlr */
				/* and flush tlbs for this thread */
#define	UTAS_TLBMISS	0x0004	/* has non-standard UTLBMISS handler */
#define UTAS_ULI	0x0008	/* utas has a ULI handler */

#define utas_lock(p)		mutex_bitlock(&(p)->utas_flag, UTAS_LCK)
#define utas_unlock(p,rv)	mutex_bitunlock(&(p)->utas_flag, UTAS_LCK, rv)
#define utas_flagset(p,b)	bitlock_set(&(p)->utas_flag, UTAS_LCK, b)
#define utas_flagclr(p,b)	bitlock_clr(&(p)->utas_flag, UTAS_LCK, b)

/*
 * tlbsync() flags
 */
#define TLB_NOSLEEP		0x01
#define STEAL_PHYSMEM		0x02
#define NO_VADDR		0x04	/* out of K2 seg space */
#define FLUSH_MAPPINGS		0x08
#define ISOLATED_ONLY		0x10
#define FLUSH_WIRED		0x20	/* flush wired entries as well */

#ifdef _KERNEL
#define	ITIMER_UTNORMALIZE 1	/* ITIMER_XXX to index into ut_timer[] */
#define	UT_ITIMER_VIRTUAL	(ITIMER_VIRTUAL - ITIMER_UTNORMALIZE)
#define	UT_ITIMER_PROF		(ITIMER_PROF - ITIMER_UTNORMALIZE)

/* tlb routines */
extern void new_tlbpid(struct utas_s *, int);
extern int64_t newtlbid(void);

#if TFP
extern void new_icachepid(struct utas_s *, int);
extern void icacheinfo_init(void);
extern int icachepid_is_usable(struct utas_s *);
extern void icachepid_use(struct utas_s *, tlbpid_t);
extern icachepid_t *icachepid_memalloc(void);
extern void icachepid_memfree(icachepid_t *);
#endif

extern void flushtlb_current(void);
extern void tlbsync(uint64_t, cpumask_t, int);
extern void tlbdirty(struct utas_s *);
extern void set_tlbpids(struct utas_s *, unsigned char);
extern void tlbinfo_init(void);
extern int tlbpid_is_usable(struct utas_s *);
extern void tlbpid_use(struct utas_s *, tlbpid_t);
extern void tlb_onesec_actions(void);
extern void tlb_tick_actions(void);
extern tlbpid_t *tlbpid_memalloc(void);
extern void tlbpid_memfree(tlbpid_t *);
extern void tlbinval_skip_my_cpu(struct utas_s *);
extern void zapsegtbl(struct utas_s *);


#ifdef ULI
tlbpid_t alloc_private_tlbpid(void);
void free_private_tlbpid(tlbpid_t);

#ifdef TFP
icachepid_t alloc_private_icachepid(void);
void free_private_icachepid(icachepid_t);
#endif
#endif /* ULI */
#endif /* _KERNEL */

/*
 * synchronized sleep struct
 */

typedef struct ssleep_s {
	short	s_waitcnt;	/* # of things to wait for before being woken */
	short	s_slpcnt;	/* # things sleeping (to be woken) */
	sv_t	s_wait;		/* sync variable to sleep on */
} ssleep_t;

typedef ushort_t uthreadid_t;

typedef struct ut_acct_s {
	u_long		ua_mem;
	u_long		ua_ioch;
	__uint64_t	ua_bread;	/* # of bytes of input */
	__uint64_t	ua_bwrit;	/* # of bytes of output */
	u_long		ua_sysc;	/* # of syscalls */
	u_long		ua_syscr;	/* # of read system calls */
	u_long		ua_syscw;	/* # of write system calls */
	u_long		ua_syscps;	/* # of poll/select system calls */
	u_long		ua_sysci;	/* # of ioctl system calls */
	u_long		ua_graphfifo;	/* # of graphics pipeline stalls */
	u_long		ua_tfaults;	/* # of 2nd level tfaults */
	u_long		ua_vfaults;	/* # of vfaults */
	u_long		ua_ufaults;	/* # of utlb misses */
	u_long		ua_kfaults;	/* # of kernel tlb misses */
} ut_acct_t;

#define MAX_IOCH	((u_long)(((u_long)(1L<<(sizeof(u_long)*NBBY-1)))-1))
#define INCR_IOCH(ioch,incr)		\
	((ioch) += ((((ioch) + (incr)) > MAX_IOCH) ? 0 : (incr)))
#define UPDATE_IOCH(ut,incr)		\
	INCR_IOCH((ut)->ut_acct.ua_ioch, incr)

typedef struct uthread_s {
	kthread_t	ut_kthread;	/* kernel thread common fields */
	struct proc *	ut_proc;	/* this thread's proc -- XXX killme */
	struct vproc	*ut_vproc;	/* corresponding vproc */
	struct uthread_s *ut_prev;	/* proc's thread linkage */
	struct uthread_s *ut_next;	/* proc's thread linkage */
	struct vfile	*ut_openfp;	/* hack for usema */
	struct proc_proxy_s *ut_pproxy;	/* 'proxy' struct for this uthread */
	flid_t		ut_flid;	/* File locking id - fl_pid and sysid */

	uint_t		ut_flags;	/* uthread state flags */
	uint_t		ut_update;	/* uthread state update flags */
	uthreadid_t	ut_id;		/* this thread's id */

	short		ut_cmask;	/* mask for file creation */

	struct prda	*ut_prda;	/* K0 addr of locked-down prda */

	/*
	 * parameters used by the scheduler. try to keep together, so that
	 * hitting one of them fetches a whole cache line containing what
	 * we need.
	 */
	struct job_s	*ut_job;
	ushort		ut_tslice;	/* time slice size, in ticks */
	ushort		ut_rticks;	/* remaining ticks at preemption */
	uchar_t		ut_policy;	/* scheduling policy */

	/*
	 * Frame Scheduler fields
	 */
	lock_t		ut_frslock;	/* lock to protect frame fields */
	ushort		ut_frsflags;	/* flags for the frame scheduler */
	ushort		ut_frsrefs;	/* references from frs queues */
	void		*ut_frs;	/* frame scheduler for this thread */

	/* interval timers */
	struct itimerval ut_timer[ITIMER_MAX-ITIMER_UTNORMALIZE];

#ifdef _SHAREII
	struct ShadThread_t *ut_shareT;	/* pointer to shadow thread entry */
#endif /* _SHAREII */
	
	/*
	 * Signal state specific to the current uthread.
	 */
#define ut_sig		ut_sigpend.s_sig
#define ut_sigqueue	ut_sigpend.s_sigqueue

	u_int		ut_cursig;	/* current signal */
	struct sigqueue *ut_curinfo;	/* siginfo for current signal */
	sigpend_t	ut_sigpend;	/* pending signals and siginfos */
	k_sigset_t *	ut_sighold;	/* pointer to hold signal bit mask */
	k_sigset_t	ut_sigwait;	/* sigwait() signal set */
	k_sigset_t	ut_suspmask;	/* real mask, swapped in sigsuspend */

	struct exception *ut_exception;	/* address for exception area */
	pde_t		ut_kstkpgs[KSTKSIZE+EXTKSTKSIZE]; /* pdes for kstk */

	u_int		ut_syscallno;	/* current system call number */
	sysarg_t	ut_scallargs[8]; /* and arguments */
	struct polldat *ut_polldat;	/* polldat for system call select */
	lock_t		ut_pollrotorlock;/* lock for pollrotor */
	ushort		ut_pollrotor;	/* wakeup rotor for poll() */

	ushort		ut_vnlock;	/* for filesystem/VM lock recursion */

	struct sat_proc	*ut_sat_proc;	/* T-irix sat_proc ptr */
	struct sat_ev_mask *ut_satmask;	/* process-local audit event mask */

	struct cred *	ut_cred;	/* snapshot of proc credentials */
	struct vnode *	ut_cdir;	/* current directory */
	struct vnode *	ut_rdir;	/* root directory */
	time_t		ut_prftime;	/* counts ticks - used by profiling */
#if R10000
	perf_mon_t	ut_perfmon;	/* performance monitoring structure */
#define ut_cpumon ut_perfmon.pm_cpu_mon
#endif
	ut_acct_t	ut_acct;	/* acct and extacct info */
	
	int		ut_code;	/* fault code */
	union {
		int	whystopwhatstop;
		struct {
			ushort whystop;
			ushort whatstop;
		} ut_wwst;
	} ut_wwun;

	int		ut_fdinuse;	/* current fd in use if any */
	int		*ut_fdmany;	/* if more than one current fd */
	int		ut_fdmanysz;	/* number on fdmany */
	int		ut_fdmanymax;	/* length (in cnt_t*) of fdmany */

	/*
	 * idbg communication area.
	 */
	k_machreg_t	ut_flt_cause;	/* cause of ext. memory fault */
	k_machreg_t	ut_flt_badvaddr;	/* vaddr of ext. mem. fault */

#ifdef ULI
	struct uli	*ut_uli;	/* chain of ULIs for this thread */
#endif
	struct exit_callback *ut_ecblist; /* funcs to call on exit */

	/*
	 * Values used by softfp package
	 */
	void		*ut_excpt_fr_ptr;
	int32_t		ut_fp_enables;	/* used by u_nofpefrom/u_nofpeto */
	int32_t		ut_fp_csr;
	inst_t		ut_epcinst;	/* instruction at epc */
	inst_t		ut_bdinst;	/* instruction in bd slot */
	char		ut_fp_csr_rm;	/* rounding mode from csr */
	char		ut_softfp_flags;
	char		ut_rdnum;
	char		ut_gstate;	/* state of uthread w.r.t gang */
	int16_t		ut_gbinding;	/* gang binding */
	utas_t		ut_as;		/* AS managed values */
	asid_t		ut_asid;	/* address space id */
	struct pwatch_s	*ut_watch;	/* watchpoint state/list */

	struct ssleep_s ut_pblock;	/* block struct for blockproc() */

	/* Following fields used by pthread RSA library (libnanothread) */

	struct kusharena *ut_sharena;	/* shared arena pointer (K0 address) */
	uint16_t	ut_maxrsaid;	/* max rsaid */
	char		ut_rsa_runable;	/* uthread runable if saved in RSA */
	char		ut_rsa_npgs;	/* npgs in sharena */
	char		ut_rsa_locore;	/* save & restore RSA in locore */
	char		ut_rsa_pad[3];	/* SPACE FOR RENT */

#if JUMP_WAR
#define JUMP_WAR_SETS	3
#define NWIREDJUMP	3
	u_char 		ut_jump_war_set;
	u_char 		ut_max_jump_war_wired;
#ifdef DEBUG
	uint 		ut_jump_war_stolen;
	uint 		ut_jump_war_stats[NWIREDJUMP*2];
	uint 		ut_jump_war_set_stats[JUMP_WAR_SETS];
#endif
#endif /* JUMP_WAR */

	/*
         * Information for syscall exit and entry traps for the
         * debuggers (through procfs).  This must be per-uthread
	 * in 6.5 and not per-proceess.  This was in pr_tracemasks
         * in 6.5MR, which is not correct.
	 */
	int		ut_errno;	/* Error number from syscall */
	__int64_t       ut_rval1;       /* return value one from syscall */
        __int64_t       ut_rval2;       /* return value two from syscall */
} uthread_t;

#define ut_whystop	ut_wwun.ut_wwst.whystop
#define ut_whatstop	ut_wwun.ut_wwst.whatstop
#define ut_whywhatstop	ut_wwun.whystopwhatstop

extern uthreadid_t get_current_utid(void);
#define current_utid()   (curuthread ? curuthread->ut_id : \
                                get_current_utid())

extern void get_current_flid(flid_t *);

/* ut_flags */

#define UT_SRIGHT       0x00000002      /* has got the right to do a step */
                                        /* in a multi-threaded environment. */
#define UT_PRSTOPX	0x00000004	/* /proc wants stop */
#define UT_SXBRK	0x00000008	/* thread stopped for lack of memory */
#define UT_STOP		0x00000010	/* uthread jctl stopped */
#define UT_PRSTOPJ	0x00000020	/* requested stop for joint stop */
#define UT_STEP		0x00000040	/* thread is single-stepping */
#define UT_WSYS		0x00000080	/* uthread got watchpoint in system */
#define UT_SIGSUSPEND	0x00000100	/* using sigsuspend sig hold mask */
#define UT_PTHREAD	0x00000200	/* POSIX 1003.1c (pthread) process */
					/* thread */
#define UT_INKERNEL	0x00000400	/* is pthread in kernel? */
#define UT_TRWAIT	0x00000800	/* tracer waiting for uthread stop */
#define UT_INACTIVE	0x00001000	/* uthread not scheduable */
#define UT_NULL3	0x00002000	/* used to be UT_ACCTJS */
#define UT_EVENTPR	0x00004000	/* thread has pending event to be */
					/* seen through procfs */
#define UT_HOLDJS	0x00008000	/* hold further attempts to sleep */
					/* part of stopping all threads */
#define UT_NULL1	0x00010000	/* used to be UT_PRESTOP */
#define UT_CLRHALT	0x00020000	/* clear current halt reason */
#define UT_ISOLATE	0x00080000	/* is mustrun on an isolated cpu */
#define UT_MUSTRUNLCK	0x00100000	/* uthread locked to mustrun cpu */
#define UT_NOMRUNINH	0x00200000	/* children shouldn't inherit mustrun */
#define UT_OWEUPC	0x00400000	/* owe user a profiling tick */
#define UT_FIXADE	0x00800000	/* fixup unaligned address errors */
#define UT_NULL2	0x01000000      /* used to be UT_WILLJS */
#define UT_SYSABORT     0x02000000      /* abort current system call */
#define UT_BLOCK	0x04000000	/* uthread may need to block itself */
#define UT_PTPSCOPE	0x08000000	/* process scope scheduling */
#define	UT_BLKONENTRY	0x10000000	/* uthread blocked at syscall entry */
#define	UT_CKPT		0x20000000	/* stop for checkpoint */
#ifdef _SHAREII
#define UT_RELOADFP	0x40000000	/* preload fp in switch */
#endif
#define UT_PTSTEP       0x80000000      /* dummy step due to running into a */
                                        /* step breakpoint for another thread. */
#define UT_PRSTOPBITS   (UT_PRSTOPX | UT_PRSTOPJ)  /* all requested stop bits */

/* ut_update */

#define UT_UPDDIR	0x00000001	/* thread needs to sync current dir */
#define UT_UPDUID	0x00000002	/* thread needs to sync uids	*/
#define UT_UPDULIMIT	0x00000004	/* thread needs to sync ulimit	*/
#define UT_UPDUMASK	0x00000008	/* thread needs to sync umask	*/
#define UT_UPDCRED	0x00000010	/* thread needs to update credentials */
#define UT_UPDSIG	0x00000020	/* thread needs to sync sigs	*/
#define UT_UPDSIGVEC	0x00000040	/* thread needs to sync w/ sigvec */
#define UT_SAT_CWD	0x00000100	/* thread needs to sync cwd for sat */
#define UT_SAT_CRD	0x00000200	/* thread needs to sync root for sat */
#define UT_UPDLOCK	0x80000000	/* lock for ut_update field */

#define UT_UPDATE(ut)	((ut)->ut_update)
#define UTSYNCFLAGS \
	(UT_UPDDIR|UT_UPDUID|UT_UPDULIMIT|UT_UPDUMASK|UT_UPDSIG|UT_UPDSIGVEC|UT_UPDCRED|UT_SAT_CWD|UT_SAT_CRD)

/* ut_vnlock */

#define UT_FSNESTED		0x8000
#define UT_FSNESTED_MAX 	0xFFFF

#define	ut_lock(ut)		kt_lock(UT_TO_KT(ut))
#define	ut_unlock(ut,s)		kt_unlock(UT_TO_KT(ut),s)

#define	ut_nested_lock(ut)	kt_nested_lock(UT_TO_KT(ut))
#define	ut_nested_unlock(ut)	kt_nested_unlock(UT_TO_KT(ut))
#define ut_islocked(ut) 	kt_islocked(UT_TO_KT(ut))

#define ut_sleep(ut,s,f,rv) \
		sv_bitlock_wait(s, f, &(UT_TO_KT(ut))->k_flags, KT_LOCK, rv)
#define ut_sleepsig(ut,s,f,rv) \
		sv_bitlock_wait_sig(s, f, &(UT_TO_KT(ut))->k_flags, KT_LOCK, rv)
#define ut_timedsleep(ut,s,f,rv,svtimer_flags,ts) \
		sv_bitlock_timedwait(s, f, &(UT_TO_KT(ut))->k_flags, KT_LOCK, \
				rv, svtimer_flags, ts, NULL)
#define ut_timedsleepsig(ut,s,f,rv,svtimer_flags,ts,rts) \
		sv_bitlock_timedwait_sig(s, f, \
					&(UT_TO_KT(ut))->k_flags, KT_LOCK, \
					rv, svtimer_flags, ts, rts)
/*
 * uthread ut_flags manipulations
 */
#define ut_flagset(ut,b)    {	int _s = ut_lock(ut); \
				(ut)->ut_flags |= (b); \
				ut_unlock(ut, _s); }

#define ut_flagclr(ut,b)    {	int _s = ut_lock(ut); \
				(ut)->ut_flags &= ~(b); \
				ut_unlock(ut, _s); }

#define ut_updset(ut,b)		bitlock_set(&(ut)->ut_update, UT_UPDLOCK, b);
#define ut_updclr(ut,b)		bitlock_clr(&(ut)->ut_update, UT_UPDLOCK, b);


/* VPROC_FLAG_THREADS */

#define	UTHREAD_FLAG_OFF	0x0
#define	UTHREAD_FLAG_ON		0x1

/* VPROC_THREAD_STATE */

#define THRD_EXIT		1
#define THRD_EXEC		2

/* Flags to uthread_exit */
#define UT_DETACHSTK		0x1
#define UT_VPROC_DESTROY	0x2

/* Null uthread id */
#define UT_ID_NULL	((uthreadid_t)-1)

#if EXTKSTKSIZE == 1
/* Flags for stackext_alloc() */
#define STACKEXT_NON_CRITICAL	0x1	/* Don't use the reserved space */
#define STACKEXT_CRITICAL	0x2	/* Use the reserved space */
#endif

#ifdef _KERNEL
struct proc;
struct sigvec_s;

extern int	uthread_create(struct proc *p, uint_t shmash, uthread_t **utp,
				uthreadid_t);
extern void	uthread_destroy(uthread_t *ut);
extern int	uthread_dup(uthread_t *, uthread_t **, struct proc *, uint,
				uthreadid_t);
extern void	uthread_exit(uthread_t *ut, int flags);
extern void	uthread_reap(void);
extern void	uthread_setsigmask(uthread_t *, k_sigset_t *);
extern int	uthread_kill(ushort_t, int);
extern int	uthreads_kill(int);
extern int	uthread_sched_setscheduler(uthread_t *, void *);
#ifdef CKPT
extern void	uthread_sched_getscheduler(uthread_t *, void *);
#endif
extern uthread_t* uthread_lookup(struct proc_proxy_s *, uthreadid_t);
extern int	uthread_apply(struct proc_proxy_s *, uthreadid_t,
			      int (*)(uthread_t *, void *), void *);

extern void	setup_wired_tlb(int);
extern void	setup_wired_tlb_notme(utas_t *, int);
extern void	cursiginfofree(uthread_t *);
extern int	isfatalsig(uthread_t *, struct sigvec_s *);
extern void	dump_timers_to_proxy(uthread_t *);
extern int	uthread_acct(uthread_t *, void *);
extern void	blockme(void);
extern void	blockset(uthread_t *, int, int, int);
extern void	blockcnt(uthread_t *, short *, unsigned char *);

#if EXTKSTKSIZE == 1
extern uint	stackext_alloc(int);
extern void	stackext_free(uint);
extern int	setup_stackext(uthread_t *);
#endif
#endif /* _KERNEL */

#endif /* __SYS_UTHREAD_H__ */
