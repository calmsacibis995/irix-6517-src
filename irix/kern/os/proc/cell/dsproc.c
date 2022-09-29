
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/systm.h>
#include <ksys/vpag.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/hwperftypes.h>
#include <sys/kaio.h>
#include <sys/proc.h>
#include <ksys/cell.h>
#include <ksys/cell/tkm.h>
#include <ksys/cell/handle.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <ksys/cred.h>

#include "proc_trace.h"
#include "fs/procfs/prsystm.h"
#include "vproc_private.h"
#include "pproc_private.h"
#include "pid_private.h"
#include "pproc.h"
#include "dsproc.h"
#include "I_dsproc_stubs.h"
#include "invk_dcproc_stubs.h"

#include <ksys/vfile.h>
#include <os/file/cell/dfile_private.h>

#ifdef DEBUG
#define PRIVATE
#else
#define PRIVATE static
#endif

PRIVATE void dsproc_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
PRIVATE void dsproc_tsif_recalled(void *, tk_set_t, tk_set_t);
PRIVATE void dsproc_tsif_idle(void *, tk_set_t);

PRIVATE tks_ifstate_t dsproc_tserver_iface = {
	dsproc_tsif_recall,
	dsproc_tsif_recalled,
	dsproc_tsif_idle
};

PRIVATE void dsproc_destroy(bhv_desc_t *);
PRIVATE void dsproc_get_proc(bhv_desc_t *, struct proc **, int);
PRIVATE int dsproc_get_nice(bhv_desc_t *, int *, struct cred *);
PRIVATE int dsproc_set_nice(bhv_desc_t *, int *, struct cred *, int);
PRIVATE void dsproc_get_attr(bhv_desc_t *, int, vp_get_attr_t *);
PRIVATE void dsproc_get_xattr(bhv_desc_t *, int, vp_get_xattr_t *);
PRIVATE int dsproc_parent_notify(bhv_desc_t *, pid_t, int, int,
				struct timeval, struct timeval, pid_t,
				sequence_num_t, short);
PRIVATE void dsproc_reparent(bhv_desc_t *, int);
PRIVATE void dsproc_reap(bhv_desc_t *, int, struct rusage *,
			struct cpu_mon *, int *);
PRIVATE int dsproc_add_exit_callback(bhv_desc_t *, int,
				void (*)(void *), void *);
PRIVATE void dsproc_run_exitfuncs(bhv_desc_t *);
PRIVATE void dsproc_set_state(bhv_desc_t *, int);
PRIVATE int dsproc_thread_state(bhv_desc_t *, int);
PRIVATE void dsproc_set_dir(bhv_desc_t *, struct vnode *, struct vnode **,
			    int);
PRIVATE int dsproc_umask(bhv_desc_t *, short, short *);
PRIVATE int dsproc_sendsig(bhv_desc_t *, int sig, int flags, pid_t sid,
			struct cred *, struct k_siginfo *);
PRIVATE void dsproc_sig_threads(bhv_desc_t *, int sig, k_siginfo_t *info);
PRIVATE int dsproc_sig_thread(bhv_desc_t *, ushort_t id, int sig);
PRIVATE void dsproc_poll_wakeup(bhv_desc_t *, ushort_t, ushort_t);
PRIVATE void dsproc_prstop_threads(bhv_desc_t *);
PRIVATE int dsproc_prwstop_threads(bhv_desc_t *, struct prnode *pnp);
PRIVATE void dsproc_prstart_threads(bhv_desc_t *);
PRIVATE int dsproc_setpgid(bhv_desc_t *, pid_t pgid, pid_t callers_pid,
			pid_t callers_sid);
PRIVATE void dsproc_pgrp_linkage(bhv_desc_t *, pid_t ppgid, pid_t psid);
PRIVATE int dsproc_setuid(bhv_desc_t *, uid_t ruid, uid_t euid, int flags);
PRIVATE int dsproc_setgid(bhv_desc_t *, gid_t rgid, gid_t egid, int flags);
PRIVATE void dsproc_setgroups(bhv_desc_t *, int setsize, gid_t *newgids);
PRIVATE int dsproc_setsid(bhv_desc_t *, pid_t *new_pgid);
PRIVATE int dsproc_setpgrp(bhv_desc_t *, int flags, pid_t *new_pgid);
PRIVATE int dsproc_ctty_access(bhv_desc_t *, enum jobcontrol access);
PRIVATE int dsproc_ctty_clear(bhv_desc_t *);
PRIVATE void dsproc_ctty_hangup(bhv_desc_t *);
PRIVATE void dsproc_get_sigact(bhv_desc_t *, int sig, k_sigaction_t *oact);
PRIVATE void dsproc_set_sigact(bhv_desc_t *, int sig, void (*disp)(),
			k_sigset_t *mask, int flags, int (*sigtramp)());
PRIVATE struct sigvec_s *dsproc_get_sigvec(bhv_desc_t *, int flags);
PRIVATE void dsproc_put_sigvec(bhv_desc_t *);
PRIVATE int dsproc_trysig_thread(bhv_desc_t *);
PRIVATE void dsproc_flag_threads(bhv_desc_t *, uint_t flag, int op);
PRIVATE void dsproc_getvpagg(bhv_desc_t *, vpagg_t **);
PRIVATE void dsproc_setvpagg(bhv_desc_t *, vpagg_t *);
PRIVATE struct kaiocbhd *dsproc_getkaiocbhd(bhv_desc_t *);
PRIVATE struct kaiocbhd *dsproc_setkaiocbhd(bhv_desc_t *, struct kaiocbhd *);
PRIVATE void dsproc_getrlimit(bhv_desc_t *, int, struct rlimit *);
PRIVATE int dsproc_setrlimit(bhv_desc_t *, int, struct rlimit *);
PRIVATE int dsproc_exec(bhv_desc_t *, struct uarg *);
PRIVATE int dsproc_set_fpflags(bhv_desc_t *, u_char, int);
PRIVATE void dsproc_set_proxy(bhv_desc_t *, int, __psint_t, __psint_t);
PRIVATE void dsproc_times(bhv_desc_t *, struct tms *);
PRIVATE void dsproc_read_us_timers(bhv_desc_t *,
					struct timespec *, struct timespec *);
PRIVATE void dsproc_getrusage(bhv_desc_t *, int flags, struct rusage *);
PRIVATE void dsproc_get_extacct(bhv_desc_t *, struct proc_acct_s *);
PRIVATE void dsproc_get_acct_timers(bhv_desc_t *, struct acct_timers *);
PRIVATE void dsproc_sched_setparam(bhv_desc_t *, int pri, int *error);
PRIVATE void dsproc_sched_getparam(bhv_desc_t *, struct cred *,
				int *pri, int *error);
PRIVATE void dsproc_sched_setscheduler(bhv_desc_t *, int pri, int policy,
				__int64_t *rval, int *error);
PRIVATE void dsproc_sched_getscheduler(bhv_desc_t *, struct cred *,
				__int64_t *rval, int *error);
PRIVATE void dsproc_sched_rr_get_interval(bhv_desc_t *, struct timespec *);
PRIVATE void dsproc_setinfo_runq(bhv_desc_t *, int rqtype, int arg,
				__int64_t *rval);
PRIVATE void dsproc_getrtpri(bhv_desc_t *, __int64_t *rval);
PRIVATE void dsproc_setmaster(bhv_desc_t *, pid_t, int *error);
PRIVATE void dsproc_schedmode(bhv_desc_t *, int, __int64_t *rval, int *error);
PRIVATE void dsproc_sched_aff(bhv_desc_t *, int, __int64_t *rval);
PRIVATE void dsproc_prnode(bhv_desc_t *, int, struct vnode **);
PRIVATE void dsproc_exitwake(bhv_desc_t *);
PRIVATE int dsproc_procblk(bhv_desc_t *, int action, int count, struct cred *,
			  int isself);
PRIVATE int dsproc_prctl(bhv_desc_t *, int option, sysarg_t arg, int isself,
		struct cred *, pid_t callers_pid, union rval *);
PRIVATE int dsproc_set_unblkonexecpid(bhv_desc_t *, pid_t);
PRIVATE void dsproc_unblkpid(bhv_desc_t *);
PRIVATE int dsproc_unblock_thread(bhv_desc_t *, ushort_t id);
PRIVATE int dsproc_fdt_dup(bhv_desc_t *, vfile_t *, int *);
#ifdef CKPT
PRIVATE void dsproc_ckpt_shmget(bhv_desc_t *, int);
PRIVATE int dsproc_get_ckpt(bhv_desc_t *, int, void *Z);
#endif
PRIVATE int dsproc_getcomm(bhv_desc_t *, struct cred *, char *, size_t);
PRIVATE void dsproc_read_all_timers(bhv_desc_t *,
					struct timespec *);
PRIVATE void dsproc_noop(void);

vproc_ops_t dsproc_ops = {
	BHV_IDENTITY_INIT_POSITION(VPROC_BHV_DS),
	dsproc_destroy,
	dsproc_get_proc,
	dsproc_get_nice,
	dsproc_set_nice,
	dsproc_set_dir,
	dsproc_umask,
	dsproc_get_attr,
	dsproc_parent_notify,
	dsproc_reparent,
	dsproc_reap,
	dsproc_add_exit_callback,
	dsproc_run_exitfuncs,
	dsproc_set_state,
	dsproc_thread_state,
	dsproc_sendsig,
	dsproc_sig_threads,
	dsproc_sig_thread,
	dsproc_prstop_threads,
	dsproc_prwstop_threads,
	dsproc_prstart_threads,
	dsproc_setpgid,
	dsproc_pgrp_linkage,
	dsproc_setuid,
	dsproc_setgid,
	dsproc_setgroups,
	dsproc_setsid,
	dsproc_setpgrp,
	dsproc_ctty_access,
	dsproc_ctty_clear,
	dsproc_ctty_hangup,
        dsproc_get_sigact,
	dsproc_set_sigact,
        dsproc_get_sigvec,
        dsproc_put_sigvec,
        dsproc_trysig_thread,
        dsproc_flag_threads,
	dsproc_getvpagg,
	dsproc_setvpagg,
	dsproc_getkaiocbhd,
	dsproc_setkaiocbhd,
	dsproc_getrlimit,
	dsproc_setrlimit,
	dsproc_exec,
	dsproc_set_fpflags,
	dsproc_set_proxy,
	dsproc_times,
	dsproc_read_us_timers,
	dsproc_getrusage,
	dsproc_get_extacct,
	dsproc_get_acct_timers,
	dsproc_sched_setparam,
	dsproc_sched_getparam,
	dsproc_sched_setscheduler,
	dsproc_sched_getscheduler,
	dsproc_sched_rr_get_interval,
	dsproc_setinfo_runq,
	dsproc_getrtpri,
	dsproc_setmaster,
	dsproc_schedmode,
	dsproc_sched_aff,
        dsproc_prnode,
	dsproc_exitwake,
	dsproc_procblk,
	dsproc_prctl,
	dsproc_set_unblkonexecpid,
	dsproc_unblkpid,
	dsproc_unblock_thread,
	dsproc_fdt_dup,
#ifdef CKPT
	dsproc_ckpt_shmget,
	dsproc_get_ckpt,
#endif
	dsproc_poll_wakeup,
	dsproc_getcomm,
	dsproc_get_xattr,
 	dsproc_read_all_timers,
	/* Add new entries before dsproc_noop */
	dsproc_noop
};

int
vproc_to_dsproc(
	vproc_t		*vp,
	dsproc_t	**dspp)
{
	bhv_desc_t	*bhv;

	BHV_READ_LOCK(VPROC_BHV_HEAD(vp));
	bhv = VPROC_BHV_FIRST(vp);

	if (BHV_POSITION(bhv) == VPROC_BHV_PP) {
		pid_t		pid;
		dsproc_t	*dsp;
		tk_set_t	granted;
		tk_set_t	refused;
		tk_set_t	already;

		/*
		 * create the dsproc structure
		 */
		dsp = kmem_alloc(sizeof(dsproc_t), KM_SLEEP);

		tks_create("dproc", dsp->dsp_tserver, dsp,
			&dsproc_tserver_iface, VPROC_NTOKENS,
			(void *)(__psint_t)vp->vp_pid);

		dsproc_lock_init(dsp);
		sv_init(&dsp->dsp_wait, SV_DEFAULT, "dsproc");
		dsp->dsp_flags = 0;
		dsp->dsp_fsync_cnt = 0;

		/*
		 * Lock the behavior chain for update. To do this we must
		 * release our reference on the vproc to avoid a deadlock
		 * with VPROC_SET_STATE and get a new reference once we
		 * know we can get the update lock, which we take at the
		 * same time.
		 */
		BHV_READ_UNLOCK(VPROC_BHV_HEAD(vp));
		pid = vp->vp_pid;
		VPROC_RELE(vp);
		vp = pid_to_vproc_locked(pid);
		if (vp == VPROC_NULL) {
			/*
			 * We couldn't get the reference, the proc
			 * went away
			 */
			tks_destroy(dsp->dsp_tserver);
			dsproc_lock_destroy(dsp);
			sv_destroy(&dsp->dsp_wait);
			kmem_free(dsp, sizeof(dsproc_t));
			return(ESRCH);
		}
		bhv = VPROC_BHV_FIRST(vp);
		/*
		 * If after all that, we still need to interpose, now
		 * would be a good time
		 */
		if (BHV_POSITION(bhv) == VPROC_BHV_PP) {
			tks_obtain(dsp->dsp_tserver, (tks_ch_t)cellid(),
				VPROC_EXISTENCE_TOKENSET, &granted, &refused,
				&already);
			ASSERT(granted == VPROC_EXISTENCE_TOKENSET);
			ASSERT(already == TK_NULLSET);
			ASSERT(refused == TK_NULLSET);
			tkc_create_local("dproc", dsp->dsp_tclient,
				dsp->dsp_tserver, VPROC_NTOKENS, granted,
				granted, (void *)(__psint_t)vp->vp_pid);
			bhv_desc_init(&dsp->dsp_bhv, dsp, vp, &dsproc_ops);
			bhv_insert(VPROC_BHV_HEAD(vp), &dsp->dsp_bhv);
		} else {
			/*
			 * Someone beat us to it. Undo what we have done
			 * so far
			 */
			tks_destroy(dsp->dsp_tserver);
			dsproc_lock_destroy(dsp);
			sv_destroy(&dsp->dsp_wait);
			kmem_free(dsp, sizeof(dsproc_t));
		}

		bhv = VPROC_BHV_FIRST(vp);

		BHV_WRITE_UNLOCK(VPROC_BHV_HEAD(vp));
	} else {
		BHV_READ_UNLOCK(VPROC_BHV_HEAD(vp));
	}

	if (BHV_POSITION(bhv) == VPROC_BHV_DC) {
		VPROC_RELE(vp);
		return(EMIGRATED);
	}

	/*
	 * we can drop the lock as the behavior won't change again because
	 * we already have the ds inserted. The vproc isn't going anywhere
	 * either because of our reference.
	 */
	*dspp = BHV_TO_DSPROC(bhv);
	return(0);
}

dsproc_t *
dsproc_alloc(
	vproc_t	*vp)
{
	int		error;
	dsproc_t	*dsp;

	dsp = kmem_alloc(sizeof(dsproc_t), KM_SLEEP);

	tks_create("dproc", dsp->dsp_tserver, dsp,
		&dsproc_tserver_iface, VPROC_NTOKENS,
		(void *)(__psint_t)vp->vp_pid);
	tkc_create_local("dproc", dsp->dsp_tclient,
		dsp->dsp_tserver, VPROC_NTOKENS, VPROC_EXISTENCE_TOKENSET,
		VPROC_EXISTENCE_TOKENSET, (void *)(__psint_t)vp->vp_pid);

	dsproc_lock_init(dsp);
	sv_init(&dsp->dsp_wait, SV_DEFAULT, "dsproc");
	dsp->dsp_flags = 0;
	dsp->dsp_fsync_cnt = 0;
	bhv_desc_init(&dsp->dsp_bhv, dsp, vp, &dsproc_ops);
	error = bhv_insert(VPROC_BHV_HEAD(vp), &dsp->dsp_bhv);
	ASSERT(!error);
	return(dsp);
}

void
dsproc_free(
	vproc_t		*vp,
	dsproc_t	*dsp)
{
	bhv_remove(VPROC_BHV_HEAD(vp), &dsp->dsp_bhv);
	tkc_free(dsp->dsp_tclient);
	tks_free(dsp->dsp_tserver);
	dsproc_lock_destroy(dsp);
	sv_destroy(&dsp->dsp_wait);
	kmem_free(dsp, sizeof(dsproc_t));
}

int
dsproc_flagsync_start(
	dsproc_t	*dsp,
	int		flagset,
	int		flagwait)
{
	dsproc_lock(dsp);
	if (dsp->dsp_flags & flagwait) {
		dsproc_unlock(dsp);
		return(EAGAIN);
	}
	if (!(dsp->dsp_flags & flagset))
		dsp->dsp_flags |= flagset;
	dsp->dsp_fsync_cnt++;
	dsproc_unlock(dsp);
	return(0);
}

void
dsproc_flagsync_end(
	dsproc_t	*dsp,
	int		flagset)
{
	dsproc_lock(dsp);
	ASSERT(dsp->dsp_flags & flagset);
	dsp->dsp_fsync_cnt--;
	if (dsp->dsp_fsync_cnt == 0)
		dsp->dsp_flags &= ~flagset;
	dsproc_unlock(dsp);
}

/*
 *****************************************************************************
 * dsproc incoming message implementation
 *****************************************************************************
 */

int
I_dsproc_get_reference(
	cell_t		sender,
	pid_t		pid,
	obj_handle_t	*handle,
	short		*state)
{
	int		err;
	vproc_t		*vp;
	dsproc_t	*dsp;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already;
	service_t	myservice;
	int		retries = 0;

restart:
	err = 0;
	if (pid_is_local(pid)) {

		vp = pid_to_vproc(pid, ZYES);
		if (vp == VPROC_NULL)
			return(ESRCH);

		if (BHV_POSITION(VPROC_BHV_FIRST(vp)) == VPROC_BHV_DC) {
			/*
			 * Process has migrated. Pass back the
			 * new location in the handle
			 */
			dcproc_get_handle(VPROC_BHV_FIRST(vp), handle);
			err = EMIGRATED;
			goto out;
		}
	} else {
		/*
		 * We looking for a processes that has migrated here
		 */
		vp = vproc_lookup_remote(pid, ZYES|VPROC_NO_CREATE);
		if (vp == VPROC_NULL)
			return(ESRCH);

		if (BHV_POSITION(VPROC_BHV_FIRST(vp)) == VPROC_BHV_DC) {
			/*
			 * Not here any more. We give back ESRCH to
			 * force a return to the origin cell
			 */
			err = ESRCH;
			goto out;
		}
	}

	err = vproc_to_dsproc(vp, &dsp);
	if (err == ESRCH)
		return(err);
	if (err == EMIGRATED)
		goto restart;

	err = dsproc_client_create_start(dsp);
	if (err != 0) {
		VPROC_RELE(vp);
		cell_backoff(++retries);
		goto restart;
	}

	tks_obtain(dsp->dsp_tserver, (tks_ch_t)sender,
                   VPROC_EXISTENCE_TOKENSET, &granted, &refused,
                   &already);
	if (already == VPROC_EXISTENCE_TOKENSET) {
		dsproc_client_create_end(dsp);
		err = EAGAIN;
		goto out;
	}
	ASSERT(granted == VPROC_EXISTENCE_TOKENSET);
	ASSERT(refused == TK_NULLSET);
	SERVICE_MAKE(myservice, cellid(), SVC_PROCESS_MGMT);
	HANDLE_MAKE(*handle, myservice, vp);

	*state = vp->vp_state;

	dsproc_client_create_end(dsp);

out:
	PROC_TRACE4(PROC_TRACE_EXISTENCE, "I_dsproc_get_reference", pid,
			"error", err);
	VPROC_RELE(vp);
	return(err);
}

void
I_dsproc_get_proc(
	objid_t		objid,
	proc_t		**p)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_get_proc(&dsp->dsp_bhv, p, 1);
}

void
I_dsproc_get_attr(
	objid_t		objid,
	int		flags,
	vp_get_attr_t	*attr)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_get_attr(&dsp->dsp_bhv, flags, attr);
}

void
I_dsproc_get_xattr(
	objid_t		objid,
	int		flags,
	vp_get_xattr_t	*xattr)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_get_xattr(&dsp->dsp_bhv, flags, xattr);
}

void
I_dsproc_parent_notify(
	objid_t		objid,
	pid_t		pid,
	int		wcode,
	int		wdata,
	struct timeval	*utime,
	struct timeval	*stime,
	pid_t		pgid,
	sequence_num_t	pgseq,
	short		xstat,
	int		*ignore)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	PROC_TRACE4(PROC_TRACE_OPS, "I_dsproc_parent_notify", vp->vp_pid,
			"child", pid);

	*ignore = dsproc_parent_notify(&dsp->dsp_bhv, pid, wcode, wdata,
			*utime, *stime, pgid, pgseq, xstat);
}

/* ARGSUSED */
void
I_dsproc_reap(
	objid_t		objid,
	cell_t		sender,
	tk_set_t	retset,
	tk_disp_t	why,
	int		flags,
	struct rusage	*rusage,
	struct cpu_mon	**hw_events,
	size_t		*nhw_events,
	int		*rtflags,
	void		**bufdesc)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	struct cpu_mon	*hw_eventsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	tks_return(dsp->dsp_tserver, sender, retset, TK_NULLSET,
			TK_NULLSET, why);

	PROC_TRACE4(PROC_TRACE_OPS, "I_dsproc_reap", vp->vp_pid,
			"flags", flags);

	VPROC_HOLD_STATE(vp, ZYES);
	BHV_READ_LOCK(VPROC_BHV_HEAD(vp));
	if (hw_events) {
		ASSERT(*nhw_events == 1);
		*hw_events = kmem_alloc(sizeof(struct cpu_mon), KM_SLEEP);
		hw_eventsp = *hw_events;
	} else
		hw_eventsp = NULL;
	dsproc_reap(&dsp->dsp_bhv, flags, rusage, hw_eventsp, rtflags);
}

/* ARGSUSED */
void
I_dsproc_reap_done(
	struct cpu_mon	*hw_events,
	size_t		nhw_events,
	void		*bufdesc)
{
	if (hw_events)
		kmem_free(hw_events, nhw_events * sizeof(struct cpu_mon));
}

void
I_dsproc_return(
	objid_t		objid,
	cell_t		sender,
	tk_set_t	to_return,
	tk_set_t	to_refuse,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);

	tks_return(dsp->dsp_tserver, sender, to_return, to_refuse,
			unknown, why);
}

void
I_dsproc_return_unknown(
	objid_t		objid,
	cell_t		sender,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);

	tks_return(dsp->dsp_tserver, sender, TK_NULLSET, TK_NULLSET,
			unknown, why);
}

void
I_dsproc_reparent(
	objid_t		objid,
	int		to_detach)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	PROC_TRACE2(PROC_TRACE_OPS, "I_dsproc_reparent", vp->vp_pid);

	dsproc_reparent(&dsp->dsp_bhv, to_detach);
}

int
I_dsproc_sendsig(
	objid_t		objid,
	int		sig,
	int		flags,
	pid_t		sid,
	credid_t	credid,
	struct k_siginfo *info,
	size_t		ninfo)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	int		error;
	cred_t		*cred;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	PROC_TRACE6(PROC_TRACE_OPS, "I_dsproc_sendsig", vp->vp_pid,
			"signal", sig, "flags", flags);

	cred = CREDID_GETCRED(credid);
	error = dsproc_sendsig(&dsp->dsp_bhv, sig, flags, sid,
			cred, ninfo ? info : NULL);
	if (cred)
		crfree(cred);

	return(error);
}

int
I_dsproc_setpgid(
	objid_t		objid,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	int		error;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	error = dsproc_setpgid(&dsp->dsp_bhv, pgid, callers_pid, callers_sid);
	return(error);
}

void
I_dsproc_pgrp_linkage(
	objid_t		objid,
	pid_t		parent_pgid,
	pid_t		parent_sid)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_pgrp_linkage(&dsp->dsp_bhv, parent_pgid, parent_sid);
}

int
I_dsproc_ctty_access(
	objid_t		objid,
	enum jobcontrol	access)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	int		error;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	error = dsproc_ctty_access(&dsp->dsp_bhv, access);
	return(error);
}

int
I_dsproc_ctty_clear(
	objid_t		objid)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	int		error;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	error = dsproc_ctty_clear(&dsp->dsp_bhv);
	return(error);
}

void
I_dsproc_ctty_hangup(
	objid_t		objid)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_ctty_hangup(&dsp->dsp_bhv);
}

void
I_dsproc_getvpagg(
	obj_handle_t	*handle,
	vpagg_t	**vpagp)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = HANDLE_TO_OBJID(*handle);
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_getvpagg(&dsp->dsp_bhv, vpagp);
}

void
I_dsproc_setvpagg(
	obj_handle_t	*handle,
	vpagg_t		*vpagg)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = HANDLE_TO_OBJID(*handle);
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_setvpagg(&dsp->dsp_bhv, vpagg);
}

void 
I_dsproc_getprnode(
	objid_t		objid,
	int		flag,
        cfs_handle_t    *vfshandle,
        cfs_handle_t    *vnhandle)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
        proc_t          *p;
        vnode_t         *vnp;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

        procfs_import_fs(vfshandle);

	dsproc_get_proc(&dsp->dsp_bhv, &p, 1);
        vnp = prbuild(p, flag);
	if (vnp == NULL)
	        CFS_HANDLE_MAKE_NULL(*vnhandle);
	else
		cfs_vnexport(vnp, vnhandle);
}

int
I_dsproc_procblk(
	objid_t		objid,
	int		action,
	int		count,
	credid_t	crid,
	int		isself)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	int		error;
	struct cred	*cr;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	cr = CREDID_GETCRED(crid);
	error = dsproc_procblk(&dsp->dsp_bhv, action, count, cr, isself);
	if (cr)
		crfree(cr);
	return(error);
}

int
I_dsproc_prctl(
	objid_t		objid,
	int		option,
	sysarg_t 	arg,
	int		isself,
	credid_t	crid,
	pid_t		callers_pid,
	rval_t		*rvp)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	int		error;
	struct cred	*cr;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	cr = CREDID_GETCRED(crid);
	error = dsproc_prctl(&dsp->dsp_bhv, option, arg, isself, cr,
			callers_pid, rvp);
	if (cr)
		crfree(cr);
	return(error);
}

int
I_dsproc_set_unblkonexecpid(
	objid_t		objid,
	pid_t		unblkpid)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	int		error;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	error = dsproc_set_unblkonexecpid(&dsp->dsp_bhv, unblkpid);
	return(error);
}

void
I_dsproc_unblkpid(
	objid_t		objid)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_unblkpid(&dsp->dsp_bhv);
}

void
I_dsproc_getrlimit(
	objid_t		objid,
	int		which,
	struct rlimit	*lim)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_getrlimit(&dsp->dsp_bhv, which, lim);
}

void
I_dsproc_setinfo_runq(
	objid_t		objid,
	int		rqtype,
	int		arg,
	__int64_t	*rval)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	dsproc_setinfo_runq(&dsp->dsp_bhv, rqtype, arg, rval);
}

int
I_dsproc_fdt_dup(
	objid_t		objid,
	vfile_export_t	*vfe,
	int		*newfd)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	vfile_t		*vf;
	int		error;

	vp  = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	vfile_import(vfe, &vf);
	error = dsproc_fdt_dup(&dsp->dsp_bhv, vf, newfd);
	vfile_close(vf);
	return(error);
}

/*
 *****************************************************************************
 * dsproc ops
 *****************************************************************************
 */

PRIVATE void
dsproc_destroy(
	bhv_desc_t	*bhv)
{
	dsproc_t	*dsp;
	vproc_t		*vp;
	tk_set_t	refused;

	dsp = BHV_TO_DSPROC(bhv);
	vp = BHV_TO_VPROC(bhv);

	/*
	 * Tear down the local client
	 */
	tks_recall(dsp->dsp_tserver, VPROC_EXISTENCE_TOKENSET, &refused);
	ASSERT(refused == TK_NULLSET);
	tkc_destroy_local(dsp->dsp_tclient);

	/*
	 * Now tear down the server and behavior
	 */
	tks_destroy(dsp->dsp_tserver);
	bhv_remove(VPROC_BHV_HEAD(vp), &dsp->dsp_bhv);
	dsproc_lock_destroy(dsp);
	sv_destroy(&dsp->dsp_wait);
	kmem_free(dsp, sizeof(dsproc_t));

	/* 
	 * Get physical and final.
	 */
	ASSERT(BHV_POSITION(VPROC_BHV_FIRST(vp)) == VPROC_BHV_PP);
	pproc_destroy(VPROC_BHV_FIRST(vp));

}

PRIVATE void
dsproc_get_proc(
	bhv_desc_t	*bhv,
	struct proc	**pp,
	int		local_only)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_get_proc(BHV_NEXT(bhv), pp, local_only);
}

/* ARGSUSED */
PRIVATE int
dsproc_get_nice(
	bhv_desc_t	*bhv,
	int		*nice,
	struct cred	*credp)
{
	int	err;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	err = pproc_get_nice(BHV_NEXT(bhv), nice, credp);
	return(err);
}

/* ARGSUSED */
PRIVATE int
dsproc_set_nice(
	bhv_desc_t	*bhv,
	int		*nice,
	struct cred	*cred,
	int		flag)
{
	int	err;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	err = pproc_set_nice(BHV_NEXT(bhv), nice, cred, flag);
	return(err);
}

/* ARGSUSED */
PRIVATE void
dsproc_set_dir(
	bhv_desc_t	*bhv,
	struct vnode	*vp,
	struct vnode	**vpp, 
	int		flag)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_set_dir(BHV_NEXT(bhv), vp, vpp, flag);
}

/* ARGSUSED */
PRIVATE int
dsproc_umask(
	bhv_desc_t	*bhv,
	short		cmask,
	short		*omask)
{
	int	err;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	err = pproc_umask(BHV_NEXT(bhv), cmask, omask);
	return(err);
}

PRIVATE void
dsproc_get_attr(
	bhv_desc_t	*bhv,
	int		flags,
	vp_get_attr_t	*attr)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_get_attr(BHV_NEXT(bhv), flags, attr);
}

PRIVATE void
dsproc_get_xattr(
	bhv_desc_t	*bhv,
	int		flags,
	vp_get_xattr_t	*xattr)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_get_xattr(BHV_NEXT(bhv), flags, xattr);
}

PRIVATE int
dsproc_parent_notify(
	bhv_desc_t	*bhv,
	pid_t		pid,
	int		wcode,
	int		wdata,
	struct timeval	utime,
	struct timeval	stime,
	pid_t		pgid,
	sequence_num_t	pgseq,
	short		xstat)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_parent_notify(BHV_NEXT(bhv), pid, wcode, wdata,
			utime, stime, pgid, pgseq, xstat);
}

/* ARGSUSED */
PRIVATE void
dsproc_reparent(
	bhv_desc_t	*bhv,
	int		to_detach)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_reparent(BHV_NEXT(bhv), to_detach);
}

/* ARGSUSED */
PRIVATE void
dsproc_reap(
	bhv_desc_t	*bhv,
	int		flags,
	struct rusage	*rusage,
	struct cpu_mon	*hw_events,
	int		*rtflags)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_reap(BHV_NEXT(bhv), flags, rusage, hw_events, rtflags);
}

/* ARGSUSED */
PRIVATE int
dsproc_add_exit_callback(
	bhv_desc_t	*bhv,
	int		flags,
	void		(* func)(void *),
	void		*arg)
{
	int	err;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	err = pproc_add_exit_callback(BHV_NEXT(bhv), flags, func, arg);
	return(err);
}

/* ARGSUSED */
PRIVATE void
dsproc_run_exitfuncs(bhv_desc_t *bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_run_exitfuncs(BHV_NEXT(bhv));
}

PRIVATE void
dsproc_set_state(
	bhv_desc_t	*bhv,
	int		state)
{
	dsproc_t	*dsp;
	tk_set_t	refused;

	dsp = BHV_TO_DSPROC(bhv);
	
	if (state == SINVAL)
		tkc_release1(dsp->dsp_tclient, VPROC_EXISTENCE_TOKEN);

	dsproc_state_change_start(dsp);

	tks_recall(dsp->dsp_tserver, VPROC_EXISTENCE_TOKENSET, &refused);
	ASSERT(!((state == 0) && (refused == VPROC_EXISTENCE_TOKENSET)));
	ASSERT((refused == TK_NULLSET) || (refused == VPROC_EXISTENCE_TOKENSET));

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);

	pproc_set_state(BHV_NEXT(bhv), state);

	dsproc_state_change_end(dsp);

}

PRIVATE int
dsproc_thread_state(
	bhv_desc_t	*bhv,
	int		state)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_thread_state(BHV_NEXT(bhv), state);
}

/* ARGSUSED */
PRIVATE int
dsproc_sendsig(
	bhv_desc_t	*bhv,
	int		sig,
	int		flags,
	pid_t		sid,
	struct cred	*cred,
	struct k_siginfo *info)
{
	int	err;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	err = pproc_sendsig(BHV_NEXT(bhv), sig, flags, sid, cred, info);
	return(err);
}

/* ARGSUSED */
PRIVATE void
dsproc_sig_threads(
	bhv_desc_t	*bhv,
	int		sig,
	k_siginfo_t	*info)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_sig_threads(BHV_NEXT(bhv), sig, info);
}

/* ARGSUSED */
PRIVATE int
dsproc_sig_thread(
	bhv_desc_t	*bhv,
	ushort_t	id,
	int		sig)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_sig_thread(BHV_NEXT(bhv), id, sig);
}

/* ARGSUSED */
PRIVATE void
dsproc_prstop_threads(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_prstop_threads(BHV_NEXT(bhv));
}

/* ARGSUSED */
PRIVATE int
dsproc_prwstop_threads(
	bhv_desc_t	*bhv,
	struct prnode	*pnp)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return(pproc_prwstop_threads(BHV_NEXT(bhv), pnp));
}

/* ARGSUSED */
PRIVATE void
dsproc_prstart_threads(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_prstart_threads(BHV_NEXT(bhv));
}


PRIVATE int
dsproc_setpgid(
	bhv_desc_t	*bhv,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid)
{
	int	err;
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	err = pproc_setpgid(BHV_NEXT(bhv), pgid, callers_pid, callers_sid);
	return(err);
}

PRIVATE void
dsproc_pgrp_linkage(
	bhv_desc_t	*bhv,
	pid_t		parent_pgid,
	pid_t		parent_sid)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_pgrp_linkage(BHV_NEXT(bhv), parent_pgid, parent_sid);
}

PRIVATE int
dsproc_setuid(
	bhv_desc_t	*bhv,
	uid_t		ruid,
	uid_t		euid,
	int		flags)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return(pproc_setuid(BHV_NEXT(bhv), ruid, euid, flags));
}

PRIVATE int
dsproc_setgid(
	bhv_desc_t	*bhv,
	gid_t		rgid,
	gid_t		egid,
	int		flags)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return(pproc_setgid(BHV_NEXT(bhv), rgid, egid, flags));
}

PRIVATE void
dsproc_setgroups(
	bhv_desc_t	*bhv,
	int		setsize,
	gid_t		*newgids)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_setgroups(BHV_NEXT(bhv), setsize, newgids);
}

PRIVATE int
dsproc_setsid(
	bhv_desc_t	*bhv,
	pid_t		*new_pgid)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return(pproc_setsid(BHV_NEXT(bhv), new_pgid));
}

PRIVATE int 
dsproc_setpgrp(
	bhv_desc_t	*bhv,
	int		flags,
	pid_t		*new_pgid)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return (pproc_setpgrp(BHV_NEXT(bhv), flags, new_pgid));
}

PRIVATE int
dsproc_ctty_access(
	bhv_desc_t	*bhv,
	enum jobcontrol	access)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return(pproc_ctty_access(BHV_NEXT(bhv), access));
}

PRIVATE int
dsproc_ctty_clear(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return(pproc_ctty_clear(BHV_NEXT(bhv)));
}

PRIVATE void
dsproc_ctty_hangup(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_ctty_hangup(BHV_NEXT(bhv));
}

PRIVATE void
dsproc_get_sigact(
	bhv_desc_t	*bhv,
	int		sig,
	k_sigaction_t	*oact)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_get_sigact(BHV_NEXT(bhv), sig, oact);
}

PRIVATE void
dsproc_set_sigact(
	bhv_desc_t	*bhv,
	int		sig,
	void		(*disp)(),
	k_sigset_t	*mask,
	int		flags,
	int		(*sigtramp)())
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_set_sigact(BHV_NEXT(bhv), sig, disp, mask, flags, sigtramp);
}

/* ARGSUSED */
PRIVATE struct sigvec_s *
dsproc_get_sigvec(
	bhv_desc_t	*bhv,
	int		flags)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return(pproc_get_sigvec(BHV_NEXT(bhv), flags));
}

/* ARGSUSED */
PRIVATE void
dsproc_put_sigvec(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_put_sigvec(BHV_NEXT(bhv));
}

/* ARGSUSED */
PRIVATE int
dsproc_trysig_thread(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return(pproc_trysig_thread(BHV_NEXT(bhv)));
}

/* ARGSUSED */
PRIVATE void
dsproc_flag_threads(
	bhv_desc_t	*bhv,
	uint_t		flag,
	int		op)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_flag_threads(BHV_NEXT(bhv), flag, op);
}

/* ARGSUSED */
PRIVATE void
dsproc_getvpagg(
	bhv_desc_t	*bhv,
	vpagg_t	**vpagp)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_getpagg(BHV_NEXT(bhv), vpagp);
}

/* ARGSUSED */
PRIVATE void
dsproc_setvpagg(
	bhv_desc_t	*bhv,
	vpagg_t		*vpag)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_setpagg(BHV_NEXT(bhv), vpag);
}

PRIVATE struct kaiocbhd *
dsproc_getkaiocbhd(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_getkaiocbhd(BHV_NEXT(bhv));
}

PRIVATE struct kaiocbhd *
dsproc_setkaiocbhd(
	bhv_desc_t		*bhv,
	struct kaiocbhd	*kbhd)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_setkaiocbhd(BHV_NEXT(bhv), kbhd);
}

PRIVATE void
dsproc_getrlimit(
	bhv_desc_t	*bhv,
	int		which,
	struct rlimit	*lim)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_getrlimit(BHV_NEXT(bhv), which, lim);
}

PRIVATE int
dsproc_setrlimit(
	bhv_desc_t	*bhv,
	int		which,
	struct rlimit	*lim)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_setrlimit(BHV_NEXT(bhv), which, lim);
}

PRIVATE int
dsproc_exec(
	bhv_desc_t	*bhv,
	struct uarg	*args)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_exec(BHV_NEXT(bhv), args);
}

PRIVATE int
dsproc_set_fpflags(
	bhv_desc_t	*bhv,
	u_char		fpflag,
	int		flags)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_set_fpflags(BHV_NEXT(bhv), fpflag, flags);
}

PRIVATE void
dsproc_set_proxy(
	bhv_desc_t	*bhv,
	int		attribute,
	__psint_t	arg1,
	__psint_t	arg2)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_set_proxy(BHV_NEXT(bhv), attribute, arg1, arg2);
}

PRIVATE void
dsproc_times(
	bhv_desc_t	*bhv,
	struct tms	*rtime)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_times(BHV_NEXT(bhv), rtime);
}

PRIVATE void
dsproc_read_us_timers(
	bhv_desc_t	*bhv,
	struct timespec	*utimep,
	struct timespec	*stimep)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_read_us_timers(BHV_NEXT(bhv), utimep, stimep);
}

PRIVATE void
dsproc_getrusage(
	bhv_desc_t	*bhv,
	int		flags,
	struct rusage	*rup)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_getrusage(BHV_NEXT(bhv), flags, rup);
}

PRIVATE void
dsproc_get_extacct(
	bhv_desc_t		*bhv,
	struct proc_acct_s	*acct)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_get_extacct(BHV_NEXT(bhv), acct);
}

PRIVATE void
dsproc_get_acct_timers(
	bhv_desc_t		*bhv,
	struct acct_timers	*timers)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_get_acct_timers(BHV_NEXT(bhv), timers);
}

PRIVATE void
dsproc_sched_setparam(
	bhv_desc_t	*bhv,
	int		pri,
	int		*error)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_sched_setparam(BHV_NEXT(bhv), pri, error);
}

PRIVATE void
dsproc_sched_getparam(
	bhv_desc_t	*bhv,
	struct cred	*cr,
	int		*pri,
	int		*error)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_sched_getparam(BHV_NEXT(bhv), cr, pri, error);
}

PRIVATE void
dsproc_sched_setscheduler(
	bhv_desc_t	*bhv,
	int		pri,
	int		policy,
	__int64_t	*rval,
	int		*error)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_sched_setscheduler(BHV_NEXT(bhv), pri, policy, rval, error);
}

PRIVATE void
dsproc_sched_getscheduler(
	bhv_desc_t	*bhv,
	struct cred	*cr,
	__int64_t	*rval,
	int		*error)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_sched_getscheduler(BHV_NEXT(bhv), cr, rval, error);
}

PRIVATE void
dsproc_sched_rr_get_interval(
	bhv_desc_t	*bhv,
	struct timespec	*timespec)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_sched_rr_get_interval(BHV_NEXT(bhv), timespec);
}

PRIVATE void
dsproc_setinfo_runq(
	bhv_desc_t	*bhv,
	int		rqtype,
	int		arg,
	__int64_t	*rval)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_setinfo_runq(BHV_NEXT(bhv), rqtype, arg, rval);
}

PRIVATE void
dsproc_getrtpri(
	bhv_desc_t	*bhv,
	__int64_t	*rval)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_getrtpri(BHV_NEXT(bhv), rval);
}

PRIVATE void
dsproc_setmaster(
	bhv_desc_t	*bhv,
	pid_t		callers_pid,
	int		*error)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_setmaster(BHV_NEXT(bhv), callers_pid, error);
}

PRIVATE void
dsproc_schedmode(
	bhv_desc_t	*bhv,
	int		arg,
	__int64_t	*rval,
	int		*error)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_schedmode(BHV_NEXT(bhv), arg, rval, error);
}

PRIVATE void
dsproc_sched_aff(
	bhv_desc_t	*bhv,
	int		flag,
	__int64_t	*rval)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_sched_aff(BHV_NEXT(bhv), flag, rval);
}

PRIVATE void
dsproc_prnode(
	bhv_desc_t	*bhv,
	int		flag,
        struct vnode    **vpp)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
        pproc_prnode(BHV_NEXT(bhv), flag, vpp);
}

PRIVATE void
dsproc_exitwake(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_exitwake(BHV_NEXT(bhv));
}

PRIVATE int
dsproc_procblk(
	bhv_desc_t	*bhv,
	int		action,
	int		count,
	struct cred	*cr,		/* Callers credentials */
	int		isself)
{
	int		error;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	error = pproc_procblk(BHV_NEXT(bhv), action, count, cr, isself);
	return(error);
}

PRIVATE int
dsproc_prctl(
	bhv_desc_t	*bhv,
	int		option,
	sysarg_t	arg,
	int		isself,
	struct cred	*cr,			/* Callers credentials */
	pid_t		callers_pid,
	rval_t		*rvp)
{
	int		error;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	error = pproc_prctl(BHV_NEXT(bhv), option, arg, isself, cr,
			callers_pid, rvp);
	return(error);
}

PRIVATE int
dsproc_set_unblkonexecpid(
	bhv_desc_t	*bhv,
	pid_t		unblkpid)
{
	int		error;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	error = pproc_set_unblkonexecpid(BHV_NEXT(bhv), unblkpid);
	return(error);
}

PRIVATE void
dsproc_unblkpid(
	bhv_desc_t	*bhv)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_unblkpid(BHV_NEXT(bhv));
}

PRIVATE int
dsproc_unblock_thread(
	bhv_desc_t	*bhv,
	ushort_t	id)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_unblock_thread(BHV_NEXT(bhv), id);
}

PRIVATE int
dsproc_fdt_dup(
	bhv_desc_t	*bhv,
	vfile_t		*vf,
	int		*newfd)
{
	int		error;

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	error = pproc_fdt_dup(BHV_NEXT(bhv), vf, newfd);
	return(error);
}

#ifdef CKPT
PRIVATE void
dsproc_ckpt_shmget(
	bhv_desc_t	*bhv,
	int		shmid)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_ckpt_shmget(BHV_NEXT(bhv), shmid);
}

PRIVATE int
dsproc_get_ckpt(
	bhv_desc_t	*bhv,
	int		code,
	void		*arg)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_get_ckpt(BHV_NEXT(bhv), code, arg);
	return(0);
}
#endif

PRIVATE int
dsproc_getcomm(
	bhv_desc_t	*bhv,
	cred_t		*cr,
	char		*bufp,
	size_t		len)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	return pproc_getcomm(BHV_NEXT(bhv), cr, bufp, len);
}

PRIVATE void
dsproc_read_all_timers(
	bhv_desc_t	*bhv,
	struct timespec	*timep)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_read_all_timers(BHV_NEXT(bhv), timep);
}

PRIVATE void
dsproc_noop(void)
{
	panic("dsproc_noop");
}

/*
 *****************************************************************************
 * dsproc token server callouts
 *****************************************************************************
 */

/* ARGSUSED */
PRIVATE void
dsproc_tsif_recall(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	to_recall,
	tk_disp_t	why)
{
	dsproc_t	*dsp;
	vproc_t		*vp;
	
	dsp = (dsproc_t *)obj;
	vp = BHV_TO_VPROC(&dsp->dsp_bhv);

	if (client == cellid()) {
		if (vp->vp_state == SZOMB)
			tkc_recall(dsp->dsp_tclient, to_recall, why);
		else
			tks_return(dsp->dsp_tserver, client, TK_NULLSET,
					to_recall, TK_NULLSET, why);
	} else {
		service_t	svc;
		obj_handle_t	handle;
		vp_get_attr_t	attr;
		/* REFERENCED */
		int		msgerr;

		VPROC_GET_ATTR(vp, VGATTR_PPID, &attr);
		SERVICE_MAKE(svc, cellid(), SVC_PROCESS_MGMT);
		HANDLE_MAKE(handle, svc, vp);
		SERVICE_MAKE(svc, client, SVC_PROCESS_MGMT);
		PROC_TRACE4(PROC_TRACE_EXISTENCE, "dsproc_tsif_recall",
				vp->vp_pid, "client", client);
		msgerr = invk_dcproc_recall(svc, &handle, vp->vp_pid,
				attr.va_ppid, to_recall, why);
		ASSERT(!msgerr);
	}
}

/* ARGSUSED */
PRIVATE void
dsproc_tsif_recalled(
	void		*obj,
	tk_set_t	recalled,
	tk_set_t	refused)
{
	panic("dsproc_tsif_recalled");
}

/* ARGSUSED */
PRIVATE void
dsproc_tsif_idle(
	void		*obj,
	tk_set_t	idle)
{
	panic("dsproc_tsif_idle");
}

PRIVATE void
dsproc_poll_wakeup(
	bhv_desc_t	*bhv,
	ushort_t	id,
	ushort_t	rotor)
{
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_poll_wakeup_thread(BHV_NEXT(bhv), id, rotor);
}

void
I_dsproc_poll_wakeup(
	objid_t		objid,
	ushort_t	id,
	ushort_t	rotor)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	PROC_TRACE4(PROC_TRACE_OPS, "I_dsproc_poll_wakeup", vp->vp_pid,
			(void *)(__psint_t)id, rotor);

	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	pproc_poll_wakeup_thread(BHV_NEXT(bhv), id, rotor);
}

/* ARGSUSED */
int
I_dsproc_getcomm(
	objid_t		objid,
	credid_t	crid,
	char		**bufpp,
	size_t		*lenp,
	void		**desc)
{
	vproc_t		*vp;
	bhv_desc_t	*bhv;
	dsproc_t	*dsp;
	struct cred	*cr;
	int		error;
	char		*bufp;

	vp = objid;
	bhv = VPROC_BHV_FIRST(vp);
	dsp = BHV_TO_DSPROC(bhv);
	VPROC_MIGRATION_CHECK(dsp);

	PROC_TRACE6(PROC_TRACE_OPS, "I_dsproc_getcomm", vp->vp_pid,
			"bufpp", bufpp, "lenp", lenp);

	/*
	 * We must allocate a temporary buffer until the done
	 * callout is received.
	 */
	ASSERT(*lenp);
	bufp = (char *) kmem_alloc(*lenp, KM_SLEEP);
	
	ASSERT(BHV_POSITION(BHV_NEXT(bhv)) == VPROC_BHV_PP);
	cr = CREDID_GETCRED(crid);
	error = pproc_getcomm(BHV_NEXT(bhv), cr, bufp, *lenp);
	if (cr)
		crfree(cr);

	if (error == 0) {
		*bufpp = bufp;
	} else {
		kmem_free((void *) bufp, *lenp);
		*bufpp = NULL;
		*lenp = 0;
	}
	return error;
}

/* ARGSUSED */
void
I_dsproc_getcomm_done(
	char	*bufp,
	size_t	len,
	void	*desc)
{
	/*
	 * Free any allocated bufer.
	 */
	if (len > 0)
		kmem_free((void *) bufp, len);
}
