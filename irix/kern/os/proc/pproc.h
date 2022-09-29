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

#ident "$Revision: 1.55 $"

#ifndef	_OS_PROC_PPROC_H_
#define	_OS_PROC_PPROC_H_

struct proc;
struct cred;
struct vproc;
struct k_siginfo;
struct rusage;
union rval;
struct vp_get_attr_s;
struct vp_get_xattr_s;
struct prnode;
struct sigvec_s;
struct vpagg_s;
struct rlimit;
struct tms;
struct timespec;
struct proc_acct_s;
struct acct_timers;
struct cpu_mon;
struct uarg;
enum jobcontrol;

extern struct proc 	*pproc_create(struct vproc *);
extern struct proc 	*pproc_recreate(struct vproc *);
extern void		pproc_return(struct proc *);
extern void		pproc_struct_init(struct proc *);

extern void pproc_destroy(bhv_desc_t *);
extern void pproc_get_proc(bhv_desc_t *, struct proc **, int);
extern int pproc_set_nice(bhv_desc_t *, int *nice, struct cred *, int flags);
extern int pproc_get_nice(bhv_desc_t *, int *nice, struct cred *);
extern void pproc_set_dir(bhv_desc_t *, struct vnode *vp, struct vnode **vpp,
				int flags);
extern int pproc_umask(bhv_desc_t *, short cmask, short *omask);
extern void pproc_get_attr(bhv_desc_t *, int flags, struct vp_get_attr_s *);
extern void pproc_get_xattr(bhv_desc_t *, int flags, struct vp_get_xattr_s *);
extern int pproc_parent_notify(bhv_desc_t *, pid_t, int wcode,
				int wdata, struct timeval utime,
				struct timeval stime, pid_t pgid,
				sequence_num_t pgseq, short xstat);
extern void pproc_reparent(bhv_desc_t *, int);
extern void pproc_reap(bhv_desc_t *, int flags, struct rusage *,
			struct cpu_mon *, int *rtflags);
extern int pproc_add_exit_callback(bhv_desc_t *, int flags,
					void (*)(void *), void *);
extern void pproc_run_exitfuncs(bhv_desc_t *);
extern void pproc_set_state(bhv_desc_t *, int);
extern int pproc_thread_state(bhv_desc_t *, int);
extern int pproc_sendsig(bhv_desc_t *, int sig, int flags, pid_t sid,
			struct cred *, struct k_siginfo *);
extern void pproc_get_sigact(bhv_desc_t *, int sig, k_sigaction_t *oact);

extern void pproc_set_sigact(bhv_desc_t *, int sig, void (*disp)(),
			k_sigset_t *mask, int flags, int (*sigtramp)());
extern struct sigvec_s *pproc_get_sigvec(bhv_desc_t *, int flags);
extern void pproc_put_sigvec(bhv_desc_t *);
extern void pproc_sig_threads(bhv_desc_t *, int sig, k_siginfo_t *info);
extern void pproc_poll_wakeup_thread(bhv_desc_t *, ushort_t id, ushort_t);
extern int pproc_sig_thread(bhv_desc_t *, ushort_t id, int sig);
extern int pproc_trysig_thread(bhv_desc_t *);
extern void pproc_flag_threads(bhv_desc_t *, uint_t flag, int op);
extern void pproc_prstop_threads(bhv_desc_t *);
extern int pproc_prwstop_threads(bhv_desc_t *, struct prnode *pnp);
extern void pproc_prstart_threads(bhv_desc_t *);
extern int pproc_setpgid(bhv_desc_t *, pid_t pgid, pid_t callers_pid,
			pid_t callers_sid);
extern void pproc_pgrp_linkage(bhv_desc_t *, pid_t ppgid, pid_t psid);
extern int pproc_setuid(bhv_desc_t *, uid_t ruid, uid_t euid, int flags);
extern int pproc_setgid(bhv_desc_t *, gid_t rgid, gid_t egid, int flags);
extern void pproc_setgroups(bhv_desc_t *, int setsize, gid_t *);
extern int pproc_setsid(bhv_desc_t *, pid_t *new_pgid);
extern int pproc_setpgrp(bhv_desc_t *, int flags, pid_t *new_pgid);
extern int pproc_ctty_access(bhv_desc_t *, enum jobcontrol access);
extern int pproc_ctty_clear(bhv_desc_t *);
extern void pproc_ctty_hangup(bhv_desc_t *);
extern void pproc_getpagg(bhv_desc_t *,  struct vpagg_s **);
extern void pproc_setpagg(bhv_desc_t *, struct vpagg_s *);
extern struct kaiocbhd *pproc_getkaiocbhd(bhv_desc_t *);
extern struct kaiocbhd *pproc_setkaiocbhd(bhv_desc_t *, struct kaiocbhd *);
extern int pproc_setrlimit(bhv_desc_t *, int, struct rlimit *);
extern void pproc_getrlimit(bhv_desc_t *, int, struct rlimit *);
extern int pproc_exec(bhv_desc_t *, struct uarg *);
extern int pproc_set_fpflags(bhv_desc_t *, u_char, int);
extern void pproc_set_proxy(bhv_desc_t *, int, __psint_t, __psint_t);
extern void pproc_times(bhv_desc_t *, struct tms *);
extern void pproc_read_us_timers(bhv_desc_t *,
				struct timespec *, struct timespec *);
extern void pproc_getrusage(bhv_desc_t *, int flags, struct rusage *);
extern void pproc_get_extacct(bhv_desc_t *, struct proc_acct_s *);
extern void pproc_get_acct_timers(bhv_desc_t *, struct acct_timers *);
extern void pproc_sched_setparam(bhv_desc_t *, int pri, int *error);
extern void pproc_sched_getparam(bhv_desc_t *, struct cred *,
				int *pri, int *error);
extern void pproc_sched_setscheduler(bhv_desc_t *, int pri, int policy,
				__int64_t *rval, int *error);
extern void pproc_sched_getscheduler(bhv_desc_t *, struct cred *,
				__int64_t *rval, int *error);
extern void pproc_sched_rr_get_interval(bhv_desc_t *, struct timespec *);
extern void pproc_setinfo_runq(bhv_desc_t *, int rqtype, int arg,
				__int64_t *rval);
extern void pproc_getrtpri(bhv_desc_t *, __int64_t *rval);
extern void pproc_setmaster(bhv_desc_t *, pid_t, int *error);
extern void pproc_schedmode(bhv_desc_t *, int, __int64_t *rval, int *error);
extern void pproc_sched_aff(bhv_desc_t *, int, __int64_t *rval);
extern void pproc_prnode(bhv_desc_t *, int, struct vnode **);
extern void pproc_exitwake(bhv_desc_t *);
extern int pproc_procblk(bhv_desc_t *, int action, int count, struct cred *,
			  int isself);
extern int pproc_prctl(bhv_desc_t *, int option, sysarg_t arg, int isself,
		struct cred *, pid_t callers_pid, union rval *);
extern int pproc_set_unblkonexecpid(bhv_desc_t *, pid_t unblkpid);
extern void pproc_unblkpid(bhv_desc_t *);
extern int pproc_unblock_thread(bhv_desc_t *, ushort_t id);
extern int pproc_fdt_dup(bhv_desc_t *, struct vfile *, int *);
#ifdef CKPT
extern void pproc_ckpt_shmget(bhv_desc_t *, int);
extern int pproc_get_ckpt(bhv_desc_t *, int, void *);
#endif
extern int pproc_getcomm(bhv_desc_t *, struct cred *, char *, size_t);
extern void pproc_read_all_timers(bhv_desc_t *,
				struct timespec *); 

#endif	/* _OS_PROC_PPROC_H_ */
