/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.84 $"

#ifndef	_OS_PROC_PPROC_PRIVATE_H_
#define	_OS_PROC_PPROC_PRIVATE_H_	1

#include <sys/types.h>
#include <ksys/behavior.h>
#include <sys/debug.h>
#include <sys/poll.h>
#include <sys/profil.h>
#include <sys/ptimers.h>
#include <sys/resource.h>
#include <sys/sema.h>
#include <sys/time.h>
#include <sys/uthread.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/nodemask.h>

extern int procnew(struct proc **, pid_t *, uint);

#define VPROC_BHV_PP	BHV_POSITION_BASE
#define BHV_TO_PROC(b) \
		(ASSERT(BHV_POSITION(b) == VPROC_BHV_PP), \
		(struct proc *)(BHV_PDATA(b)))

#define PROC_TO_VPROC(p)	((struct vproc *)BHV_VOBJ(&p->p_bhv))

#define VPROC_GET_PROC(v, p) \
	{ \
	BHV_READ_LOCK(VPROC_BHV_HEAD(v)); \
	_VPOP_(vpop_get_proc, v) (VPROC_BHV_FIRST(v), p, 0); \
	BHV_READ_UNLOCK(VPROC_BHV_HEAD(v)); \
	}

#define IDBG_VPROC_GET_PROC(v, p) \
        _VPOP_(vpop_get_proc, v) (VPROC_BHV_FIRST(v), p, 1);

#ifdef CELL_IRIX
#define VPROC_GET_PROC_LOCAL(v, p) \
	{ \
	BHV_READ_LOCK(VPROC_BHV_HEAD(v)); \
	_VPOP_(vpop_get_proc, v) (VPROC_BHV_FIRST(v), p, 1); \
	BHV_READ_UNLOCK(VPROC_BHV_HEAD(v)); \
	}
#else
#define VPROC_GET_PROC_LOCAL	VPROC_GET_PROC
#endif

#define curprocp (private.p_curuthread ? UT_TO_PROC(private.p_curuthread) : 0)

struct cipso_proc;
struct prnode;
struct shacct;
struct fdt;		
struct kusharena;
struct avltree_desc;

typedef struct proc {
	proc_proxy_t	p_proxy;	/* the proxy structure */

	uint		p_flag;		/* generic process flags */
	char		p_acflag;	/* accounting flags? */
	char		p_stat;		/* process state */
	pid_t		p_pgid;		/* name of process group leader */
	pid_t		p_pid;		/* process id */
	pid_t		p_ppid;		/* process id of parent*/
	pid_t		p_sid;		/* session id */

	struct vnode	*p_script;	/* script executed by #!/path/intp */

	struct fdt	*p_fdt;		/* file descriptor table */

	/* Signal information */
	k_sigset_t	p_sigtrace;	/* tracing signal mask for /proc */
	struct sigvec_s p_sigvec;	/* pointer to signal vector info */

	struct child_pidlist *p_childpids; /* ptr to first child process */
	mutex_t		p_childlock;	/* protects process child list */
	sv_t		p_wait;		/* waiting for something to happen */
	sv_t		p_wexit;	/* Unused cf: 576784 & 656452 */
	time_t		p_start;	/* Time of process creation */
	time_t		p_ticks;	/* lbolt at process creation */
	struct rusage	p_cru;		/* sum of stats for reaped children */
	struct itimer_info {
		struct	itimerval realtimer; /* time to next real alarm */
		lock_t	itimerlock;	/* ITIMER_REAL handling lock */
		__uint64_t interval_tick;
		__uint64_t next_timeout;
		int	interval_unit;
		toid_t	next_toid;
	} p_ii;

	struct user_info {
		char		u_comm[PSCOMSIZ];
		char		u_psargs[PSARGSZ];	 /* args from exec */
		struct rlimit	u_rlimit[RLIM_NLIMITS]; /* resource limits */
	} p_ui;

#define	p_comm		p_ui.u_comm
#define	p_rlimit 	p_ui.u_rlimit
#define	p_psargs 	p_ui.u_psargs

	struct proc	*p_slink;	/* share group link */
	struct shaddr_s	*p_shaddr;	/* pointer to shared group desc */
	char		p_exitsig;	/* signal to send on exit of sharegrp */

	mrlock_t	p_who;		/* process personality lock -- */
					/* protects p_vpgrp, etc*/
	struct proc 	*p_pgflink;	/* process group chain */
	struct proc 	*p_pgblink;	/* process group chain */
	struct vpgrp	*p_vpgrp;	/* virtual process group structure */
	mrlock_t	p_credlock;	/* process credentials lock */
	struct cred	*p_cred;	/* process credentials */
	struct prnode	*p_trace;	/* pointer to /proc prnode */
	struct prnode	*p_pstrace;	/* pointer to /proc/pinfo prnode */
#ifdef PRCLEAR
	int		 p_prnode_refcnt; /* prnode reference count 	*/
					  /* for p_trace and p_pstrace	*/
#else
	int		 p_notused;	/* make sure that we do not change */
					/* the size of the structure	   */
#endif
	uint		p_exec_cnt;	/* exec generation number */
	struct vnode	*p_exec;	/* vnode for original exec */
	struct vpagg_s	*p_vpagg;	/* process aggregate info */
	struct cipso_proc	*p_cipso_proc;	/* T-irix cipso_proc ptr */
	struct pr_tracemasks	*p_trmasks;	/* Procfs syscall/fault trace */
	u_int		p_dbgflags;	/* flags for low-freq /proc events */

	pid_t		p_unblkonexecpid; /* pid to unblock on exec */

#ifdef CKPT
	mrlock_t	p_ckptlck;
	struct avltree_desc *p_ckptshm;
#endif
	struct exit_callback *p_ecblist; /* funcs to call on exit */
	ptimer_info_t	*p_ptimers;	/* array of posix timers */
	bhv_desc_t	p_bhv;		/* Base behavior */

	struct prof	*p_profp;	/* ptr to list of prof structures */
	short		p_profn;	/* number of prof structures in list */
	inst_t		*p_fastprof[PROF_FAST_CNT+1]; /* space to save pc's */
	int		p_fastprofcnt;	/* index into p_fastprof */

	struct kaiocbhd	*p_kaiocbhd;	/* KAIO */
	/* More miscellany */
	struct shacct	*p_shacct;	/* Shadow accounting data */
	__int64_t	p_parcookie;	/* par syscall access token */
	void		*p_migrate;	/* what to send when migrated/rexec'd */
#ifdef _SHAREII
	struct ShadProc_t      *p_shareP;	/* shadow proc entry */
#endif /* _SHAREII */
} proc_t;

#define p_lock(p)		mutex_bitlock(&(p)->p_flag, STATELOCK)
#define p_lock_spl(p,f)		mutex_bitlock_spl(&(p)->p_flag, STATELOCK,f)
#define p_unlock(p,rv)		mutex_bitunlock(&(p)->p_flag, STATELOCK, rv)
#define p_nested_lock(p)	nested_bitlock(&(p)->p_flag, STATELOCK)
#define p_nested_unlock(p)	nested_bitunlock(&(p)->p_flag, STATELOCK)
#define p_flagset(p,b)		bitlock_set(&(p)->p_flag, STATELOCK, b)
#define p_flagclr(p,b)		bitlock_clr(&(p)->p_flag, STATELOCK, b)
#define p_sleep(p,s,f,rv)	sv_bitlock_wait(s, f, &(p)->p_flag, STATELOCK, rv)
#define p_sleepsig(p,s,f,rv) \
			sv_bitlock_wait_sig(s, f, &(p)->p_flag, STATELOCK, rv)
#define p_islocked(p) 		bitlock_islocked(&(p)->p_flag, STATELOCK)
#define p_bitlock		p_flag

#define p_realtimer	p_ii.realtimer
#define p_itimerlock	p_ii.itimerlock
#define p_itimer_tick	p_ii.interval_tick
#define p_itimer_unit	p_ii.interval_unit
#define p_itimer_next	p_ii.next_timeout
#define p_itimer_toid	p_ii.next_toid

struct cred;
struct sigqueue;
extern int	proc_set_nice(proc_t *, int *nice, struct cred *, int flags);
extern int	proc_get_nice(proc_t *, int *nice, struct cred *);
extern dev_t	cttydev(struct proc *);
extern void	sigtoproc(proc_t *, int sig, k_siginfo_t *info);
extern void	sigtouthread_common(uthread_t *, int s, int sig, sigvec_t *sigp,
				struct sigqueue *sqp);
extern void	freechildren(struct proc *);
extern int	anydead(struct proc *, struct k_siginfo *);
extern void	reparent_children(struct proc *);
extern void	pproc_exit(uthread_t *, int, int, int *);
extern int	allocshaddr(struct proc *);
extern void	attachshaddr(struct shaddr_s *, struct proc *);
extern int	detachshaddr(struct proc *, int);

#ifdef CKPT
extern int	getxstat(proc_t *, pid_t, short *);
#endif
#endif	/* _OS_PROC_PPROC_PRIVATE_H_ */
