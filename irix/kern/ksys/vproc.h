/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.87 $"

#ifndef _VPROC_VPROC_H_
#define _VPROC_VPROC_H_

#include <ksys/kqueue.h>
#include <ksys/pid.h>
#include <ksys/behavior.h>
#include <sys/ksignal.h>
#include <ksys/vpgrp.h>
#include <sys/uthread.h>                /* XXX for ut_vproc */

/*
 * Vproc structure.
 */
typedef struct vproc {
	kqueue_t		vp_queue; 	/* Hash queue link */
	pid_t			vp_pid;		/* vproc process identifier */
	int			vp_refcnt;	/* vproc reference count */
	short			vp_state;	/* State of process */
	short			vp_needwakeup;	/* if someone is waiting */
	lock_t			vp_refcnt_lock; /* lock for ref. count */
	sv_t			vp_cond;	/* condition variable */
	bhv_head_t		vp_bhvh;	/* Behavior head */
} vproc_t;

#define VPROC_NULL	((vproc_t *)0)
#define curvprocp (private.p_curuthread ? UT_TO_VPROC(private.p_curuthread) : 0)
extern pid_t kthread_get_pid(void);
#define	current_pid()	(curvprocp ? curvprocp->vp_pid : \
				kthread_get_pid())
#define INIT_PID	(1)		/* pid of init process */

/*
 * Flags to VPROC_LOOKUP()
 */
#define	ZNO		1	/* Fail on encountering a zombie process. */
#define	ZYES		2	/* Allow zombies. */

#define	VPROC_LOOKUP(pid)		vproc_lookup(pid, ZNO)
#define	VPROC_LOOKUP_STATE(pid, flags)	vproc_lookup(pid, flags)
#define VPROC_HOLD(v)			vproc_hold(v, ZNO)
#define VPROC_HOLD_STATE(v, f)		vproc_hold(v, f)
#define VPROC_RELE(v)			vproc_release(v)
#define VPROC_ASSERT_LOCAL(vpr)         ASSERT(vproc_is_local(vpr))

extern void	vproc_startup(void);
extern void	vproc_init(void);
extern vproc_t *vproc_create(void);
extern void	vproc_return(vproc_t *);
extern void	vproc_destroy(vproc_t *);
extern void	vproc_release(vproc_t *);
extern int	vproc_hold(vproc_t *, int);
extern vproc_t *vproc_lookup(pid_t, int);
extern void	vproc_lookup_prevent(vproc_t *);
extern void	vproc_set_state(vproc_t *, int);
extern int	vproc_thread_state(vproc_t *, int);
extern vproc_t *idbg_vproc_lookup(pid_t);
struct uarg;
extern int	vproc_exec (vproc_t *, struct uarg *);
extern int      vproc_is_local(vproc_t *); 

/* Struct for VPROC_GET_ATTR */
typedef struct vp_get_attr_s {
	pid_t	va_ppid;		/* Parents pid */
	pid_t	va_pgid;		/* process group id */
	pid_t	va_sid;			/* Session id */
	int	va_has_ctty;		/* proc has controlling tty */
	uid_t	va_uid;			/* process uid */
	uid_t	va_gid;			/* process gid */
	time_t	va_signature;		/* Process signature */
	asid_t	va_asid;		/* Process address space id */
        cell_t  va_cellid;		/* Process cell id */
} vp_get_attr_t;

/* Struct for VPROC_GET_XATTR */
typedef struct vp_get_xattr_s {
	vp_get_attr_t	xa_basic;
	char		xa_name[PSCOMSIZ];
	char		xa_args[PSARGSZ];
	struct timeval  xa_utime;
	struct timeval  xa_stime;
	char		xa_stat;
	char		xa_nice;
	cpuid_t		xa_lcpu;
	uint		xa_pflag;
	uint		xa_uflag;
	long		xa_pri;
	caddr_t		xa_slpchan;
	uid_t		xa_realuid;
	uid_t		xa_saveuid;
	pgno_t		xa_rss;
	size_t		xa_stksz;
	pgno_t		xa_size;
} vp_get_xattr_t;

/*
 * job control access type for VPROC_ACCESS_CTTY
 */
enum jobcontrol {
	JCTLREAD,                        /* read data on a ctty */
	JCTLWRITE,                       /* write data to a ctty */
	JCTLGST                          /* get ctty parameters */
};

struct cred;
struct proc;
struct vnode;
struct rusage;
union rval;
struct k_siginfo;
struct prnode;
struct sigvec_s;
struct vpagg_s;
struct rlimit;
struct tms;
struct timespec;
struct proc_acct_s;
struct acct_timers;
struct cpu_mon;
struct vfile;

typedef struct vproc_ops {
	bhv_position_t	vpr_position;	/* position within behavior chain */
	void (*vpop_destroy)
		(bhv_desc_t *b);	/* behavior */

	void (*vpop_get_proc)           /* XXX temporary interface */
		(bhv_desc_t *b,		/* behavior */
		struct proc **p,	/* returned proc pointer */
		int flag);		/* 1 = local only */

	int (*vpop_get_nice)
		(bhv_desc_t *b,		/* behavior */
		 int *nice,		/* returned nice value */
		 struct cred *);	/* Callers credentials */

	int (*vpop_set_nice)
		(bhv_desc_t *b,		/* behavior */
		 int *nice,		/* nice value */
		 struct cred *scred,	/* calling process information */
		 int flag);		/* VNICE_INCR, when called */
					/* to implement 'nice' semantics. */
	void (*vpop_proc_set_dir)
		(bhv_desc_t *b,		/* behavior */
		 struct vnode *vp,	/* directory vnode */
		 struct vnode **dir,	/* address of vnode pointer */
		 int flag);		/* VPROC_DIR_ROOT for root directory, */
					/* VPROC_DIR_CURRENT otherwise */
	int (*vpop_proc_umask)
		(bhv_desc_t *b,		/* behavior */
		 short	cmask,		/* new file creation mask */
		 short *omask);		/* address to store old mask */

	void (*vpop_get_attr)
		(bhv_desc_t *b,		/* behavior */
		 int flags,		/* mask of attributes wanted */
		 vp_get_attr_t *attr);	/* returned attributes */

	int (*vpop_parent_notify)
		(bhv_desc_t *b,		/* behavior */
		 pid_t pid,		/* pid that is exiting */
		 int wcode,		/* wcode of exiting proc. */
		 int wdata,		/* wdata of exiting proc. */
		 struct timeval utime,	/* current u and s time of process */
		 struct timeval stime,
		 pid_t pgid,		/* Current pgrp id of process */
		 sequence_num_t pgseq,	/* Current pgrp signal sequence */
		 short xstat);		/* exit status of child */

	void (*vpop_reparent)
		(bhv_desc_t *b,		/* behavior */
		 int to_detach);	/* perform orphan pgrp jazz */

	void (*vpop_reap)
		(bhv_desc_t *b,		/* behavior */
		 int flags,		/* flags */
		 struct rusage *,	/* rusage stats from child */
		 struct cpu_mon *,	/* R10000 only - hw perf counters */
		 int *rtflags);		/* Returned flags from server */

	int (*vpop_add_exit_callback)
		(bhv_desc_t *b,		/* behavior */
		 int flags,		/* flags */
		 void (* func)(void *),	/* function to call */
		 void *arg);		/* argument to call func with */

	void (*vpop_run_exitfuncs)
		(bhv_desc_t *b);	/* behavior */

	void (*vpop_set_state)
		(bhv_desc_t *b,		/* behavior */
		 int state);		/* state */

	int (*vpop_thread_state)
		(bhv_desc_t *b,		/* behavior */
		 int state);		/* state */

	int (*vpop_sendsig)
		(bhv_desc_t *b,		/* behavior */
		 int sig,		/* signal to post */
		 int flags,		/* input flags */
		 pid_t sid,		/* session id of caller */
		 struct cred *,		/* credentials of caller */
		 struct k_siginfo *);	/* Associated siginfo struct to post */

	void (*vpop_sig_threads)
		(bhv_desc_t *b,		/* behavior */
		 int sig,		/* signal to post */
		 struct k_siginfo *);	/* associated siginfo struct to post */

	int (*vpop_sig_thread)
		(bhv_desc_t *b,		/* behavior */
		 ushort_t id,		/* thread id */
		 int sig);		/* signal to post */

	void (*vpop_prstop_threads)
		(bhv_desc_t *b);	/* behavior */

	int (*vpop_prwstop_threads)
		(bhv_desc_t *b,		/* behavior */
		 struct prnode *);	/* prnode */

	void (*vpop_prstart_threads)
		(bhv_desc_t *b);	/* behavior */

	int (*vpop_setpgid)
		(bhv_desc_t *b,		/* behavior */
		 pid_t pgid,		/* pgrp id to set to */
		 pid_t,			/* Callers pid */
		 pid_t sid);		/* Callers session id */

	void (*vpop_pgrp_linkage)
		(bhv_desc_t *b,		/* behavior */
		 pid_t parent_pgid,	/* parent's new pgrp */
		 pid_t parent_sid);	/* parent's new session id */
		
	int (*vpop_setuid)
		(bhv_desc_t *b,		/* behavior */
		 uid_t ruid,		/* real uid */
		 uid_t euid,		/* eff uid */
		 int flags);		/* flags */

	int (*vpop_setgid)
		(bhv_desc_t *b,		/* behavior */
		 gid_t rgid,		/* real gid */
		 gid_t egid,		/* eff gid */
		 int flags);		/* flags */

	void (*vpop_setgroups)
		(bhv_desc_t *b,		/* behavior */
		 int setsize,		/* # entries of new set */
		 gid_t *newgids);	/* new group set */

	int (*vpop_setsid)
		(bhv_desc_t *b,		/* behavior */
		 pid_t *new_pgid);	/* output parameter - proc's new pgid */

	int (*vpop_setpgrp)
		(bhv_desc_t *b,		/* behavior */
		 int flags,		/* flags */
		 pid_t *new_pgid);	/* output parameter - proc's new pgid */

	int (*vpop_ctty_access)
		(bhv_desc_t *b,		/* behavior */
		 enum jobcontrol acc);	/* access type */

	int (*vpop_ctty_clear)
		(bhv_desc_t *b);	/* behavior */

	void (*vpop_ctty_hangup)
		(bhv_desc_t *b);	/* behavior */

	void (*vpop_get_sigact)
		(bhv_desc_t *b,		/* behavior */
		int sig,		/* signal */
		k_sigaction_t *oact);	/* old action, returned */

	void (*vpop_set_sigact)
		(bhv_desc_t *b,		/* behavior */
		int sig,		/* signal */
		void (*disp)(),		/* signal disposition */
		k_sigset_t *mask,	/* signal mask */
		int flags,		/* guess */
		int (*sigtramp)());	/* user trampoline function */

	struct sigvec_s *(*vpop_get_sigvec)
		(bhv_desc_t *b,		/* behavior */
		int flags);		/* access mode */

	void (*vpop_put_sigvec)
		(bhv_desc_t *b);	/* behavior */

	int (*vpop_trysig_thread)
		(bhv_desc_t *b);	/* behavior */

	void (*vpop_flag_threads)
		(bhv_desc_t *b,		/* behavior */
		 uint_t flag,		/* flag bits to set/clear */
		 int op);		/* set or clear */

	void (*vpop_getvpagg)
		(bhv_desc_t *b,		/* behavior */
		 struct vpagg_s **);	/* pointer to store result in */

	void (*vpop_setvpagg)
		(bhv_desc_t *b,		/* behavior */
		 struct vpagg_s *);	/* vpagg_t to set */

	struct kaiocbhd *(*vpop_getkaiocbhd)
		(bhv_desc_t *b);	/* behavior */

	struct kaiocbhd *(*vpop_setkaiocbhd)
		(bhv_desc_t *b,		/* behavior */
		 struct kaiocbhd *);	/* new kaiocbhd to set */

	void (*vpop_getrlimit)
		(bhv_desc_t *b,		/* behavior */
		 int which,		/* which limit to retrieve */
		 struct rlimit *rlim);	/* result */

	int (*vpop_setrlimit)
		(bhv_desc_t *b,		/* behavior */
		 int which,		/* which limit to set */
		 struct rlimit *rlim);	/* new value */

	int (*vpop_exec)
		(bhv_desc_t *b,		/* behavior */
		 struct uarg *);	/* exec uarg */

	int (*vpop_set_fpflags)
		(bhv_desc_t *b,		/* behavior */
		 u_char fpflag,		/* fp flag to set */
		 int flags);		/* directives */

	void (*vpop_set_proxy)
		(bhv_desc_t *b,		/* behavior */
		 int,			/* attribute */
		 __psint_t,		/* arg1 */
		 __psint_t);		/* arg1 */

	void (*vpop_times)
		(bhv_desc_t *b,		/* behavior */
		 struct tms *);		/* times structure */

	void (*vpop_read_us_timers)
		(bhv_desc_t *b,		/* behavior */
		 struct timespec *,	/* timespec for utime */
		 struct timespec *);	/* timespec for stime */

	void (*vpop_getrusage)
		(bhv_desc_t *b,		/* behavior */
		 int flags,		/* childs rusage or own rusage? */
		 struct rusage *);

	void (*vpop_get_extacct)
		(bhv_desc_t *b,		/* behavior */
		 struct proc_acct_s *);	/* counters, etc, returned here */

	void (*vpop_get_acct_timers)
		(bhv_desc_t *b,		/* behavior */
		 struct acct_timers *);	/* The timers that ext. acct. wants */

	void (*vpop_sched_setparam)
		(bhv_desc_t *b,		/* behavior */
		 int pri,
		 int *error);

	void (*vpop_sched_getparam)
		(bhv_desc_t *b,		/* behavior */
		 struct cred *,		/* callers creds */
		 int *pri,		/* return value */
		 int *error);

	void (*vpop_sched_setscheduler)
		(bhv_desc_t *b,		/* behavior */
		 int pri,
		 int policy,
		 __int64_t *rval,
		 int *error);

	void (*vpop_sched_getscheduler)
		(bhv_desc_t *b,		/* behavior */
		 struct cred *,		/* callers creds */
		 __int64_t *sched,	/* return value */
		 int *error);

	void (*vpop_sched_rr_get_interval)
		(bhv_desc_t *b,		/* behavior */
		 struct timespec *);	/* returned timeslice */

	void (*vpop_setinfo_runq)
		(bhv_desc_t *b,		/* behavior */
		 int rqtype,
		 int arg,
		 __int64_t *rval);

	void (*vpop_getrtpri)
		(bhv_desc_t *b,		/* behavior */
		 __int64_t *rval);

	void (*vpop_setmaster)
		(bhv_desc_t *b,		/* behavior */
		 pid_t callers_pid,
		 int *error);

	void (*vpop_schedmode)
		(bhv_desc_t *b,		/* behavior */
		 int arg,
		 __int64_t *rval,
		 int *error);

	void (*vpop_sched_aff)
		(bhv_desc_t *b,		/* behavior */
		 int flag,
		 __int64_t *rval);

        void (*vpop_prnode)
		(bhv_desc_t *b,		/* behavior */
                 int flag,
                 struct vnode **vpp);

	void (*vpop_exitwake)		/* op deleted */
		(void);

	int (*vpop_procblk)
		(bhv_desc_t *b,
		 int action,
		 int count,
		 struct cred *cr,
		 int isself);

	int (*vpop_prctl)
		(bhv_desc_t *b,
		 int option,
		 sysarg_t arg,
		 int isself,
		 struct cred *cr,
		 pid_t callers_pid,
		 union rval *rvp);

	int (*vpop_set_unblkonexecpid)
		(bhv_desc_t *b,
		 pid_t unblkpid);

	void (*vpop_unblkpid)
		(bhv_desc_t *b);

	int (*vpop_unblock_thread)
		(bhv_desc_t *b,		/* behavior */
		 ushort_t id);		/* exec uarg */

	int (*vpop_fdt_dup)		/* all this for attachsema */
		(bhv_desc_t *,		/* behavior */
		 struct vfile *,	/* open file */
		 int *);		/* new file descriptor */

#ifdef CKPT
	void (*vpop_ckpt_shmget)	/* so we can log shmids */
		(bhv_desc_t *,		/* behavior */
		 int );			/* shmid */

	int (*vpop_get_ckpt)		/* so we can retrieve proc ckpt info */
		(bhv_desc_t *,		/* behavior */
		 int,			/* code */
		 void *arg);
#endif
	void (*vpop_poll_wakeup)	/* thread part of pollwakeup */
		(bhv_desc_t *,		/* behavior */
		 uthreadid_t,		/* tid */
		 ushort_t);		/* rotor hint */

	int (*vpop_getcomm)		/* fetch command line */
		(bhv_desc_t *,		/* behavior */
		 struct cred *,		/* callers creds */
		 char *,		/* string buffer */
		 size_t);		/* buffer length */
		
	void (*vpop_get_xattr)
		(bhv_desc_t *b,		/* behavior */
		 int flags,		/* mask of xattributes wanted */
		 vp_get_xattr_t *xattr);/* returned xattributes */

	void (*vpop_read_all_timers)
		(bhv_desc_t *b,		/* behavior */
		 struct timespec *);	/* timespec for stime */

	/* ADD all new entries before vpop_noop.
	 * Null routine, to inhibit compilation due to
	 * prototype mismatch if op-tables do not match.
	 */
	void (*vpop_noop)
		(void);
} vproc_ops_t;

/* Flags for VPROC_SET_NICE */
#define VNICE_INCR		0x1	/* Priority passed to VPROC_SET_NICE
					 * is an increment - to implement
					 * the nice() syscall.
					 */
/* Flags for VPROC_GET_ATTR */
#define VGATTR_PPID		0x1	/* Return proc ppid */
#define VGATTR_PGID		0x2	/* Return proc pgid */
#define VGATTR_SID		0x4	/* Return proc session id */
#define VGATTR_JCL		VGATTR_PGID|VGATTR_SID
#define VGATTR_HAS_CTTY		0x8	/* proc has controlling tty */
#define VGATTR_UID		0x10	/* return proc uid */
#define VGATTR_GID		0x20	/* return proc gid */
#define VGATTR_SIGNATURE	0x40	/* return proc 'signature' */
#define VGATTR_CELLID		0x100	/* return process's cell id */

/* Additional flags for VPROC_GET_XATTR */
#define	VGXATTR_NICE		0x00000200	/* process nice value */
#define	VGXATTR_WCHAN		0x00000400	/* thread sleep chan */
#define VGXATTR_PRI		0x00000800	/* thread priority */
#define VGXATTR_UID		0x00001000	/* real/eff/saved uid */
#define VGXATTR_LCPU		0x00002000	/* thread last cpu */
#define VGXATTR_RSS		0x00004000	/* thread rss */
#define VGXATTR_SIZE		0x00008000	/* thread as size */
#define VGXATTR_UST		0x00010000	/* thread aggregrate time */
#define VGXATTR_NAME		0x00020000	/* process name/args */
#define VGXATTR_STAT		0x00040000	/* process/thread flags */

/* Flags for VPROC_REAP */
#define VREAP_IGNORE		0x1	/* Parent ignoring exited child */

/* Return flags for VPROC_REAP */
#define VWC_RUSAGE		0x1	/* child returned rusage stats */

/* Flags for VPROC_SET_DIR */
#define VDIR_ROOT		0x1	/* changing root directory */
#define VDIR_CURRENT		0x2	/* changing current directory */

/* Flags for VPROC_GET_SIGVEC */
#define VSIGVEC_UPDATE		0x1
#define VSIGVEC_ACCESS		0x2
#define VSIGVEC_PEEK		0x4


/* Flags for CURVPROC_SETUID, CURVPROC_SETGID */
#define VSUID_SYSV		0x1	/* SysV semantics */
#define VSUID_XPG4		0x2	/* xpg4 setregid semantics */
#define	VSUID_CKPT		0x4	/* ckpt setuid semantics */

/* Flags for CURVPROC_SETPGRP */
#define VSPGRP_SYSV		0x1	/* SysV semantics */

/* Flags for CURVPROC_SET_FPFLAGS */
#define	VFP_SINGLETHREADED	0x1	/* Proc must be singlethreaded to */
					/* change flag */
#define	VFP_FLAG_ON		0x2	/* ON */
#define	VFP_FLAG_OFF		0x4	/* OFF */

/* Flags for CURVPROC_SET_PROXY */
#define VSETP_NOFPE		0x1	/* Set nofpefrom/nofpeto */
#define VSETP_EXCCNT		0x2	/* Set dismissed exception count */
#define VSETP_SPIPE             0x4     /* enable streams pipe */
#define VSETP_USERVME           0x8     /* process has vme space mapped */

/* Flags for VPROC_GETRUSAGE */
#define VRUSAGE_SELF		0x1	/* Get own rusage */
#define VRUSAGE_CHLDRN		0x2	/* Get rusage of waited-on children */

/* Flags for VPROC_SCHED_AFF */
#define VSCHED_AFF_ON		1
#define VSCHED_AFF_OFF		2
#define VSCHED_AFF_GET		3

/* Flags for VPROC_PRNODE */
#define VPRNODE_PINFO           0x1 	/* Get a pinfo vnode. */

#define VPROC_BHV_HEAD(v)	(&(v)->vp_bhvh)
#define VPROC_BHV_FIRST(v)	BHV_HEAD_FIRST(VPROC_BHV_HEAD(v))
#define _VPOP_(op, v)	(*((vproc_ops_t *)BHV_OPS(VPROC_BHV_FIRST(v)))->op)

#define _VPROC_SIMPLE_(vpop, v, parms)					\
        {								\
	_VPOP_(vpop, v) parms;						\
	}

#define _VPROC_(vpop, v, parms)						\
        {								\
	BHV_READ_LOCK(VPROC_BHV_HEAD(v));				\
	_VPOP_(vpop, v) parms;						\
	BHV_READ_UNLOCK(VPROC_BHV_HEAD(v));				\
	}

#define _VPROC_CONSUME_(vpop, v, parms)					\
        {								\
	BHV_READ_LOCK(VPROC_BHV_HEAD(v));				\
	_VPOP_(vpop, v) parms;						\
	}

#define _VPROC_RET_(vpop, v, ret, parms)				\
        {								\
	BHV_READ_LOCK(VPROC_BHV_HEAD(v));				\
	ret = _VPOP_(vpop, v) parms;					\
	BHV_READ_UNLOCK(VPROC_BHV_HEAD(v));				\
	}

#define VPROC_DESTROY(v)						\
	_VPROC_SIMPLE_(vpop_destroy, v, (VPROC_BHV_FIRST(v)))

#define VPROC_GET_NICE(v, _nice, _cred, ret)				\
	_VPROC_RET_(vpop_get_nice, v, ret,				\
		(VPROC_BHV_FIRST(v), _nice, _cred))

#define VPROC_SET_NICE(v, _nice, _cred, _flag, ret)			\
	_VPROC_RET_(vpop_set_nice, v, ret,				\
		(VPROC_BHV_FIRST(v), _nice, _cred, _flag))

#define CURVPROC_SET_DIR(_vp, _dir, _flag)				\
        _VPROC_(vpop_proc_set_dir, curvprocp,				\
		(VPROC_BHV_FIRST(curvprocp), _vp, _dir, _flag))

#define CURVPROC_UMASK(_cmask, _omask, ret)				\
        _VPROC_RET_(vpop_proc_umask, curvprocp, ret,			\
		(VPROC_BHV_FIRST(curvprocp), _cmask, _omask))

#define VPROC_GET_ATTR(v, _flags, _attr) \
	_VPROC_(vpop_get_attr, v, (VPROC_BHV_FIRST(v), _flags, _attr))

#define VPROC_GET_XATTR(v, _flags, _xattr) \
	_VPROC_(vpop_get_xattr, v, (VPROC_BHV_FIRST(v), _flags, _xattr))

#define VPROC_PARENT_NOTIFY(v, _pid, _wcode, _wdata, _utime, _stime,	\
			_pgid, _pgseq, _xstat, ret)			\
	_VPROC_RET_(vpop_parent_notify, v, ret,				\
		(VPROC_BHV_FIRST(v), _pid, _wcode, _wdata, _utime,	\
				_stime, _pgid, _pgseq, _xstat))

#define VPROC_REPARENT(v, to_detach)					\
	_VPROC_(vpop_reparent, v, (VPROC_BHV_FIRST(v), to_detach))

#define VPROC_REAP(v, _flags, _rusage, _hwevents, _rtflags)		\
	_VPROC_CONSUME_(vpop_reap, v,					\
		(VPROC_BHV_FIRST(v), _flags, _rusage, _hwevents, _rtflags))

#define VPROC_ADD_EXIT_CALLBACK(v, _flags, _func, _arg, _error)		\
	_VPROC_RET_(vpop_add_exit_callback, v, _error,			\
		(VPROC_BHV_FIRST(v), _flags, _func, _arg))

#define VPROC_RUN_EXITFUNCS(v)						\
	_VPROC_(vpop_run_exitfuncs, v, (VPROC_BHV_FIRST(v)))

#define VPROC_SET_STATE(v, _state)					\
	_VPROC_(vpop_set_state, v, (VPROC_BHV_FIRST(v), _state))

#define VPROC_THREAD_STATE(v, _state, _oldstate)			\
	_VPROC_RET_(vpop_thread_state, v, _oldstate,			\
		(VPROC_BHV_FIRST(v), _state))

#define VPROC_SENDSIG(v, _sig, _flags, _sid, _cred, _info, _rv)		\
	_VPROC_RET_(vpop_sendsig, v, _rv,				\
		(VPROC_BHV_FIRST(v), _sig, _flags, _sid, _cred, _info))

#define VPROC_SIG_THREADS(v, _sig, _info)				\
        _VPROC_(vpop_sig_threads, v, (VPROC_BHV_FIRST(v), _sig, _info))

#define CURVPROC_SIG_THREAD(_id, _sig, _rv)				\
        _VPROC_RET_(vpop_sig_thread, curvprocp, _rv,			\
		    (VPROC_BHV_FIRST(curvprocp), _id, _sig))

#define VPROC_PRSTOP_THREADS(v)						\
	_VPROC_(vpop_prstop_threads, v, (VPROC_BHV_FIRST(v)))

#define VPROC_PRWSTOP_THREADS(v, _pnp, _rv)				\
	_VPROC_RET_(vpop_prwstop_threads, v, _rv, (VPROC_BHV_FIRST(v), _pnp))

#define VPROC_PRSTART_THREADS(v)					\
	_VPROC_(vpop_prstart_threads, v, (VPROC_BHV_FIRST(v)))

#define VPROC_SETPGID(v, _pgid, _pid, _sid, _rv)			\
	_VPROC_RET_(vpop_setpgid, v, _rv,				\
		    (VPROC_BHV_FIRST(v), _pgid, _pid, _sid))

#define VPROC_PGRP_LINKAGE(v, _pgid, _sid)				\
	_VPROC_(vpop_pgrp_linkage, v, (VPROC_BHV_FIRST(v), _pgid, _sid))

#define CURVPROC_SETUID(_ruid, _euid, _flags, _rv)			\
	_VPROC_RET_(vpop_setuid, curvprocp, _rv,			\
		(VPROC_BHV_FIRST(curvprocp), _ruid, _euid, _flags))

#define CURVPROC_SETGID(_rgid, _egid, _flags, _rv)			\
	_VPROC_RET_(vpop_setgid, curvprocp, _rv,			\
		(VPROC_BHV_FIRST(curvprocp), _rgid, _egid, _flags))

#define CURVPROC_SETGROUPS(_setsize, _newgids)				\
	_VPROC_(vpop_setgroups, curvprocp,				\
		(VPROC_BHV_FIRST(curvprocp), _setsize, _newgids))

#define CURVPROC_SETSID(_newpgid, _error)				\
	_VPROC_RET_(vpop_setsid, curvprocp, _error,			\
		(VPROC_BHV_FIRST(curvprocp), _newpgid))

#define CURVPROC_SETPGRP(_flag, _newpgid, _error)			\
	_VPROC_RET_(vpop_setpgrp, curvprocp, _error,			\
		(VPROC_BHV_FIRST(curvprocp), _flag, _newpgid))

#define VPROC_CTTY_ACCESS(v, _access, _rv)				\
	_VPROC_RET_(vpop_ctty_access, v, _rv, (VPROC_BHV_FIRST(v), _access))

#define VPROC_CTTY_CLEAR(v, _rv)					\
	_VPROC_RET_(vpop_ctty_clear, v, _rv, (VPROC_BHV_FIRST(v)))

#define CURVPROC_CTTY_HANGUP()						\
	_VPROC_(vpop_ctty_hangup, curvprocp, (VPROC_BHV_FIRST(curvprocp)))

#define CURVPROC_GET_SIGACT(_sig, _oact)				\
	_VPROC_(vpop_get_sigact, curvprocp,				\
		(VPROC_BHV_FIRST(curvprocp), _sig, _oact))

#define CURVPROC_SET_SIGACT(_sig, _hndlr, _mask, _flags, _sigtramp)	\
	_VPROC_(vpop_set_sigact, curvprocp,				\
		(VPROC_BHV_FIRST(curvprocp),				\
			_sig, _hndlr, _mask, _flags, _sigtramp))

#define VPROC_GET_SIGVEC(v, _flags, _sigvp)				\
	_VPROC_RET_(vpop_get_sigvec, v, _sigvp, (VPROC_BHV_FIRST(v), _flags))

#define VPROC_PUT_SIGVEC(v)						\
	_VPROC_(vpop_put_sigvec, v, (VPROC_BHV_FIRST(v)))

#define CURVPROC_GETKAIOCBHD(_whd)					\
	_VPROC_RET_(vpop_getkaiocbhd, curvprocp, _whd,			\
		     (VPROC_BHV_FIRST(curvprocp)))

#define CURVPROC_SETKAIOCBHD(_kaiocbhd, _n_kaiocbhd)			\
	_VPROC_RET_(vpop_setkaiocbhd, curvprocp, _n_kaiocbhd,		\
		    (VPROC_BHV_FIRST(curvprocp), _kaiocbhd))

#define VPROC_GETRLIMIT(v, _which, _rlimp)				\
	_VPROC_(vpop_getrlimit, v, (VPROC_BHV_FIRST(v), _which, _rlimp))

#define CURVPROC_SETRLIMIT(_which, _rlimp, _error)			\
	_VPROC_RET_(vpop_setrlimit, curvprocp, _error,			\
		    (VPROC_BHV_FIRST(curvprocp), _which, _rlimp))

#define VPROC_TRYSIG_THREAD(v, _rv)					\
	_VPROC_RET_(vpop_trysig_thread, v, _rv, (VPROC_BHV_FIRST(v)))

#define VPROC_FLAG_THREADS(v, _flag, _op)				\
	_VPROC_(vpop_flag_threads, v, (VPROC_BHV_FIRST(v), _flag, _op))

#define VPROC_GETVPAGG(v, _vpagg)					\
	_VPROC_(vpop_getvpagg, v, (VPROC_BHV_FIRST(v), _vpagg))

#define VPROC_SETVPAGG(v, _vpagg)					\
	_VPROC_(vpop_setvpagg, v, (VPROC_BHV_FIRST(v), _vpagg))

#define VPROC_EXEC(v, _uarg, _rv)					\
	_VPROC_RET_(vpop_exec, v, _rv, (VPROC_BHV_FIRST(v), _uarg))

#define CURVPROC_SET_FPFLAGS(_fpflag, _flag, _error)			\
	_VPROC_RET_(vpop_set_fpflags, curvprocp, _error,		\
		(VPROC_BHV_FIRST(curvprocp), _fpflag, _flag))

#define CURVPROC_SET_PROXY(_attr, _from, _to)				\
	_VPROC_(vpop_set_proxy, curvprocp,				\
		(VPROC_BHV_FIRST(curvprocp), _attr, _from, _to))

#define CURVPROC_TIMES(_tms)						\
	_VPROC_(vpop_times, curvprocp, (VPROC_BHV_FIRST(curvprocp), _tms))

#define VPROC_READ_US_TIMERS(v, _utime, _stime)				\
	_VPROC_(vpop_read_us_timers, v, (VPROC_BHV_FIRST(v), _utime, _stime))

#define VPROC_GETRUSAGE(v, _flags, _rusage)				\
	_VPROC_(vpop_getrusage, v, (VPROC_BHV_FIRST(v), _flags, _rusage))

#define VPROC_GET_EXTACCT(v, _acct)					\
	_VPROC_(vpop_get_extacct, v, (VPROC_BHV_FIRST(v), _acct))

#define VPROC_GET_ACCT_TIMERS(v, _timers)				\
	_VPROC_(vpop_get_acct_timers, v, (VPROC_BHV_FIRST(v), _timers))

#define VPROC_SCHED_SETPARAM(v, _pri, _error)				\
	_VPROC_(vpop_sched_setparam, v, (VPROC_BHV_FIRST(v), _pri, _error))

#define VPROC_SCHED_GETPARAM(v, _cr, _pri, _error)			\
	_VPROC_(vpop_sched_getparam, v,					\
		(VPROC_BHV_FIRST(v), _cr, _pri, _error))

#define VPROC_SCHED_SETSCHEDULER(v, _pri, _policy, _rval, _error)	\
	_VPROC_(vpop_sched_setscheduler, v,				\
		(VPROC_BHV_FIRST(v), _pri, _policy, _rval, _error))

#define VPROC_SCHED_GETSCHEDULER(v, _cr, _sched, _error)		\
	_VPROC_(vpop_sched_getscheduler, v,				\
		(VPROC_BHV_FIRST(v), _cr, _sched, _error))

#define VPROC_SCHED_RR_GET_INTERVAL(v, _slice)				\
	_VPROC_(vpop_sched_rr_get_interval, v, (VPROC_BHV_FIRST(v), _slice))

#define VPROC_SETINFORUNQ(v, _rqtype, _arg, _rval)			\
	_VPROC_(vpop_setinfo_runq, v,					\
		(VPROC_BHV_FIRST(v), _rqtype, _arg, _rval))

#define VPROC_GETRTPRI(v, _rval)					\
	_VPROC_(vpop_getrtpri, v, (VPROC_BHV_FIRST(v), _rval))

#define VPROC_SETMASTER(v, _callers_pid, _error)			\
	_VPROC_(vpop_setmaster, v,					\
		(VPROC_BHV_FIRST(v),  _callers_pid, _error))

#define CURVPROC_SCHEDMODE(_arg, _rval, _error)				\
	_VPROC_(vpop_schedmode, curvprocp,				\
		(VPROC_BHV_FIRST(curvprocp), _arg, _rval, _error))

#define VPROC_SCHEDMODE(v, _arg, _rval, _error)				\
	_VPROC_(vpop_schedmode, v,				\
		(VPROC_BHV_FIRST(v), _arg, _rval, _error))

#define VPROC_SCHED_AFF(v, _flag, _rval)				\
	_VPROC_(vpop_sched_aff, v, (VPROC_BHV_FIRST(v), _flag, _rval))

#define VPROC_PRNODE(v, _flag, _vpp)					\
        _VPROC_(vpop_prnode, v, (VPROC_BHV_FIRST(v), _flag, _vpp))

#define VPROC_PROCBLK(v, _action, _count, _cr, _isself, _error)		\
	_VPROC_RET_(vpop_procblk, v, _error,				\
		(VPROC_BHV_FIRST(v), _action, _count, _cr, _isself))

#define VPROC_PRCTL(v, _option, _arg, _isself, _cr, _callers_pid, _rvp, _err) \
	_VPROC_RET_(vpop_prctl, v, _err,				\
		(VPROC_BHV_FIRST(v), _option, _arg, _isself, _cr,	\
		 _callers_pid, _rvp))

#define CURVPROC_SET_UNBLKONEXECPID(_unblkpid, _error)			\
	_VPROC_RET_(vpop_set_unblkonexecpid, curvprocp, _error,		\
		(VPROC_BHV_FIRST(curvprocp), _unblkpid))

#define VPROC_UNBLKPID(v)						\
	_VPROC_SIMPLE_(vpop_unblkpid, v, (VPROC_BHV_FIRST(v)))

#define CURVPROC_UNBLOCK_THREAD(_id, _error)				\
	_VPROC_RET_(vpop_unblock_thread, curvprocp, _error,		\
		(VPROC_BHV_FIRST(curvprocp), _id))

#define VPROC_FDT_DUP(v, _vf, _newfd, _error)				\
	_VPROC_RET_(vpop_fdt_dup, v, _error, (VPROC_BHV_FIRST(v), _vf, _newfd))

#define CURVPROC_CKPT_SHMGET(_shmid)					\
	_VPROC_SIMPLE_(vpop_ckpt_shmget, curvprocp, (VPROC_BHV_FIRST(curvprocp), _shmid))

#define	VPROC_GET_CKPT(v, _code, _arg, _rv)				\
	_VPROC_RET_(vpop_get_ckpt, v, _rv, (VPROC_BHV_FIRST(v), _code, _arg))

#define VPROC_POLL_WAKEUP(v, _tid, _rotor)				\
	_VPROC_(vpop_poll_wakeup, v, (VPROC_BHV_FIRST(v),_tid, _rotor))

#define VPROC_GETCOMM(v, _cr, _bufp, _len, _rv)				\
	_VPROC_RET_(vpop_getcomm, v, _rv,				\
		(VPROC_BHV_FIRST(v), _cr, _bufp, _len))

#define VPROC_READ_ALL_TIMERS(v, _timers)                         \
	_VPROC_(vpop_read_all_timers, v, (VPROC_BHV_FIRST(v), _timers))

#endif	/* _VPROC_VPROC_H_ */

