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
#include <ksys/behavior.h>
#include <ksys/cell/tkm.h>
#include <ksys/cell/handle.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <ksys/cred.h>

#include "proc_trace.h"
#include "fs/procfs/prsystm.h"
#include "vproc_private.h"
#include "pid_private.h"
#include "pproc_private.h"
#include "dcproc.h"
#include "invk_dsproc_stubs.h"
#include "I_dcproc_stubs.h"
#include <ksys/vfile.h>
#include "os/file/cell/dfile_private.h"

#ifdef DEBUG
#define PRIVATE
#else
#define	PRIVATE	static
#endif

PRIVATE void dcproc_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
				tk_set_t *);
PRIVATE void dcproc_tcif_recall(tkc_state_t, void *, tk_set_t, tk_set_t,
				tk_disp_t);

PRIVATE tkc_ifstate_t dcproc_tclient_iface = {
                dcproc_tcif_obtain,
                dcproc_tcif_recall
};

PRIVATE void dcproc_destroy(bhv_desc_t *);
PRIVATE void dcproc_get_proc(bhv_desc_t *, struct proc **, int);
PRIVATE int dcproc_get_nice(bhv_desc_t *, int *, struct cred *);
PRIVATE int dcproc_set_nice(bhv_desc_t *, int *, struct cred *, int);
PRIVATE void dcproc_get_attr(bhv_desc_t *, int, vp_get_attr_t *);
PRIVATE void dcproc_get_xattr(bhv_desc_t *, int, vp_get_xattr_t *);
PRIVATE int dcproc_parent_notify(bhv_desc_t *, pid_t, int,
				int, struct timeval, struct timeval,
				pid_t, sequence_num_t, short);
PRIVATE void dcproc_reparent(bhv_desc_t *, int);
PRIVATE void dcproc_reap(bhv_desc_t *, int, struct rusage *,
			struct cpu_mon *, int *);
PRIVATE int dcproc_add_exit_callback(bhv_desc_t *, int,
				void (*)(void *), void *);
PRIVATE void dcproc_run_exitfuncs(bhv_desc_t *);
PRIVATE void dcproc_set_state(bhv_desc_t *, int);
PRIVATE int dcproc_thread_state(bhv_desc_t *, int);
PRIVATE void dcproc_set_dir(bhv_desc_t *, struct vnode *, struct vnode **,
			    int);
PRIVATE int dcproc_umask(bhv_desc_t *, short, short *);
PRIVATE int dcproc_sendsig(bhv_desc_t *, int sig, int flags, pid_t sid,
			struct cred *, struct k_siginfo *);
PRIVATE void dcproc_sig_threads(bhv_desc_t *, int sig, k_siginfo_t *info);
PRIVATE int dcproc_sig_thread(bhv_desc_t *, ushort_t id, int sig);
PRIVATE void dcproc_poll_wakeup(bhv_desc_t *, ushort_t, ushort_t);
PRIVATE void dcproc_prstop_threads(bhv_desc_t *);
PRIVATE int dcproc_prwstop_threads(bhv_desc_t *, struct prnode *pnp);
PRIVATE void dcproc_prstart_threads(bhv_desc_t *);
PRIVATE int dcproc_setpgid(bhv_desc_t *, pid_t pgid, pid_t callers_pid,
			pid_t callers_sid);
PRIVATE void dcproc_pgrp_linkage(bhv_desc_t *, pid_t ppgid, pid_t psid);
PRIVATE int dcproc_setuid(bhv_desc_t *, uid_t ruid, uid_t euid, int flags);
PRIVATE int dcproc_setgid(bhv_desc_t *, gid_t rgid, gid_t egid, int flags);
PRIVATE void dcproc_setgroups(bhv_desc_t *, int setsize, gid_t *newgids);
PRIVATE int dcproc_setsid(bhv_desc_t *, pid_t *new_pgid);
PRIVATE int dcproc_setpgrp(bhv_desc_t *, int flags, pid_t *new_pgid);
PRIVATE int dcproc_ctty_access(bhv_desc_t *, enum jobcontrol access);
PRIVATE int dcproc_ctty_clear(bhv_desc_t *);
PRIVATE void dcproc_ctty_hangup(bhv_desc_t *);
PRIVATE void dcproc_get_sigact(bhv_desc_t *, int sig, k_sigaction_t *oact);
PRIVATE void dcproc_set_sigact(bhv_desc_t *, int sig, void (*disp)(),
			k_sigset_t *mask, int flags, int (*sigtramp)());
PRIVATE struct sigvec_s *dcproc_get_sigvec(bhv_desc_t *, int flags);
PRIVATE void dcproc_put_sigvec(bhv_desc_t *);
PRIVATE int dcproc_trysig_thread(bhv_desc_t *);
PRIVATE void dcproc_flag_threads(bhv_desc_t *, uint_t flag, int op);
PRIVATE void dcproc_getvpagg(bhv_desc_t *, vpagg_t **);
PRIVATE void dcproc_setvpagg(bhv_desc_t *, vpagg_t *);
PRIVATE struct kaiocbhd *dcproc_getkaiocbhd(bhv_desc_t *);
PRIVATE struct kaiocbhd *dcproc_setkaiocbhd(bhv_desc_t *, struct kaiocbhd *);
PRIVATE void dcproc_getrlimit(bhv_desc_t *, int, struct rlimit *);
PRIVATE int dcproc_setrlimit(bhv_desc_t *, int, struct rlimit *);
PRIVATE int dcproc_exec(bhv_desc_t *, struct uarg *);
PRIVATE int dcproc_set_fpflags(bhv_desc_t *, u_char, int);
PRIVATE void dcproc_set_proxy(bhv_desc_t *, int, __psint_t, __psint_t);
PRIVATE void dcproc_times(bhv_desc_t *, struct tms *);
PRIVATE void dcproc_read_us_timers(bhv_desc_t *,
					struct timespec *, struct timespec *);
PRIVATE void dcproc_getrusage(bhv_desc_t *, int flags, struct rusage *);
PRIVATE void dcproc_get_extacct(bhv_desc_t *, struct proc_acct_s *);
PRIVATE void dcproc_get_acct_timers(bhv_desc_t *, struct acct_timers *);
PRIVATE void dcproc_sched_setparam(bhv_desc_t *, int pri, int *error);
PRIVATE void dcproc_sched_getparam(bhv_desc_t *, struct cred *,
				int *pri, int *error);
PRIVATE void dcproc_sched_setscheduler(bhv_desc_t *, int pri, int policy,
				__int64_t *rval, int *error);
PRIVATE void dcproc_sched_getscheduler(bhv_desc_t *, struct cred *,
				__int64_t *rval, int *error);
PRIVATE void dcproc_sched_rr_get_interval(bhv_desc_t *, struct timespec *);
PRIVATE void dcproc_setinfo_runq(bhv_desc_t *, int rqtype, int arg,
				__int64_t *rval);
PRIVATE void dcproc_getrtpri(bhv_desc_t *, __int64_t *rval);
PRIVATE void dcproc_setmaster(bhv_desc_t *, pid_t, int *error);
PRIVATE void dcproc_schedmode(bhv_desc_t *, int, __int64_t *rval, int *error);
PRIVATE void dcproc_sched_aff(bhv_desc_t *, int, __int64_t *rval);
PRIVATE void dcproc_prnode(bhv_desc_t *, int, struct vnode **);
PRIVATE void dcproc_exitwake(bhv_desc_t *);
PRIVATE int dcproc_procblk(bhv_desc_t *, int action, int count, struct cred *,
			  int isself);
PRIVATE int dcproc_prctl(bhv_desc_t *, int option, sysarg_t arg, int isself,
		struct cred *, pid_t callers_pid, union rval *);
PRIVATE int dcproc_set_unblkonexecpid(bhv_desc_t *, pid_t);
PRIVATE void dcproc_unblkpid(bhv_desc_t *);
PRIVATE int dcproc_unblock_thread(bhv_desc_t *, ushort_t id);
PRIVATE int dcproc_fdt_dup(bhv_desc_t *, vfile_t *, int *);
#ifdef CKPT
PRIVATE void dcproc_ckpt_shmget(bhv_desc_t *, int);
PRIVATE int dcproc_get_ckpt(bhv_desc_t *, int, void *);
#endif
PRIVATE int dcproc_getcomm(bhv_desc_t *, struct cred *, char *, size_t);

PRIVATE void dcproc_noop(void);

PRIVATE vproc_ops_t dcproc_ops = {
	BHV_IDENTITY_INIT_POSITION(VPROC_BHV_DC),
	dcproc_destroy,
	dcproc_get_proc,
	dcproc_get_nice,
	dcproc_set_nice,
	dcproc_set_dir,
	dcproc_umask,
	dcproc_get_attr,
	dcproc_parent_notify,
	dcproc_reparent,
	dcproc_reap,
	dcproc_add_exit_callback,
	dcproc_run_exitfuncs,
	dcproc_set_state,
	dcproc_thread_state,
	dcproc_sendsig,
	dcproc_sig_threads,
	dcproc_sig_thread,
	dcproc_prstop_threads,
	dcproc_prwstop_threads,
	dcproc_prstart_threads,
	dcproc_setpgid,
	dcproc_pgrp_linkage,
	dcproc_setuid,
	dcproc_setgid,
	dcproc_setgroups,
	dcproc_setsid,
	dcproc_setpgrp,
	dcproc_ctty_access,
	dcproc_ctty_clear,
	dcproc_ctty_hangup,
	dcproc_get_sigact,
	dcproc_set_sigact,
	dcproc_get_sigvec,
	dcproc_put_sigvec,
	dcproc_trysig_thread,
	dcproc_flag_threads,
	dcproc_getvpagg,
	dcproc_setvpagg,
	dcproc_getkaiocbhd,
	dcproc_setkaiocbhd,
	dcproc_getrlimit,
	dcproc_setrlimit,
	dcproc_exec,
	dcproc_set_fpflags,
	dcproc_set_proxy,
	dcproc_times,
	dcproc_read_us_timers,
	dcproc_getrusage,
	dcproc_get_extacct,
	dcproc_get_acct_timers,
	dcproc_sched_setparam,
	dcproc_sched_getparam,
	dcproc_sched_setscheduler,
	dcproc_sched_getscheduler,
	dcproc_sched_rr_get_interval,
	dcproc_setinfo_runq,
	dcproc_getrtpri,
	dcproc_setmaster,
	dcproc_schedmode,
	dcproc_sched_aff,
        dcproc_prnode,
	dcproc_exitwake,
	dcproc_procblk,
	dcproc_prctl,
	dcproc_set_unblkonexecpid,
	dcproc_unblkpid,
	dcproc_unblock_thread,
	dcproc_fdt_dup,
#ifdef CKPT
	dcproc_ckpt_shmget,
	dcproc_get_ckpt,
#endif
	dcproc_poll_wakeup,
	dcproc_getcomm,
	dcproc_get_xattr,

	/* Add new entries before dcproc_noop */
	dcproc_noop
};

void
dcproc_return(
	dcproc_t	*dcp)
{
	kmem_free(dcp, sizeof(dcproc_t));
}

int
dcproc_create(
	vproc_t	*vp)
{
	dcproc_t	*dcp;
	int		error;
	service_t	origin;
	service_t	svc;

	origin = pid_to_service(vp->vp_pid);
	if (SERVICE_IS_NULL(origin))
		return(ESRCH);

	dcp = kmem_alloc(sizeof(dcproc_t), KM_SLEEP);
	ASSERT(dcp);

	/*
	 * Invoke the remote operation.
	 */
	svc = origin;
migrated:
	error = invk_dsproc_get_reference(svc, cellid(), vp->vp_pid,
				&dcp->dcp_handle, &vp->vp_state);
	ASSERT(error != ECELLDOWN);
	if (error == EMIGRATED) {
		/*
		 * If we have been told that the proc has migrated
		 * then we try the new location, unless it is here,
		 * in which case we will back out and try the lookup
		 * again from scratch and hopefully find the migrated
		 * server.
		 */
		svc = HANDLE_TO_SERVICE(dcp->dcp_handle);
		if (SERVICE_TO_CELL(svc) != cellid())
			goto migrated;
		error = EAGAIN;
	}
	if (error) {
		/*
		 * If we got an error from anyone other than the origin cell
		 * we go back to the origin to get a definitive answer
		 */
		if (error == ESRCH && !SERVICE_EQUAL(svc, origin)) {
			svc = origin;
			goto migrated;
		}
		/*
		 * No such process or we lost the race with someone else
		 */
		kmem_free(dcp, sizeof(dcproc_t));
		return(error);
	}

	tkc_create("dcproc", dcp->dcp_tclient, dcp,
			&dcproc_tclient_iface, VPROC_NTOKENS,
			VPROC_EXISTENCE_TOKENSET, VPROC_EXISTENCE_TOKENSET,
			(void *)(__psint_t)vp->vp_pid);

        bhv_head_init(VPROC_BHV_HEAD(vp), "vproc");
	bhv_desc_init(&dcp->dcp_bhv, dcp, vp, &dcproc_ops);
        bhv_insert_initial(VPROC_BHV_HEAD(vp), &dcp->dcp_bhv);

	ASSERT(dcp->dcp_handle.h_objid != 0);

	PROC_TRACE4(PROC_TRACE_EXISTENCE, "dcproc_create", vp->vp_pid, "state",
			vp->vp_state);

	return(0);
}

void
dcproc_alloc(
	vproc_t		*vp,
	obj_handle_t	*handle)
{
	dcproc_t	*dcp;

	dcp = kmem_alloc(sizeof(dcproc_t), KM_SLEEP);
	ASSERT(dcp);

	dcp->dcp_handle = *handle;
	tkc_create("dcproc", dcp->dcp_tclient, dcp,
			&dcproc_tclient_iface, VPROC_NTOKENS,
			VPROC_EXISTENCE_TOKENSET, VPROC_EXISTENCE_TOKENSET,
			(void *)(__psint_t)vp->vp_pid);

	bhv_desc_init(&dcp->dcp_bhv, dcp, vp, &dcproc_ops);
        bhv_insert(VPROC_BHV_HEAD(vp), &dcp->dcp_bhv);

	ASSERT(HANDLE_TO_OBJID(dcp->dcp_handle) != 0);
}

void
dcproc_get_handle(
	bhv_desc_t	*bhv,
	obj_handle_t	*handle)
{
	dcproc_t	*dcp;

	dcp = BHV_TO_DCPROC(bhv);
	*handle = dcp->dcp_handle;
}


/*
 **************************************************************************
 * dcproc token client callouts
 **************************************************************************
 */

/* ARGSUSED */
PRIVATE void
dcproc_tcif_obtain(
	void		*obj,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	why_returning,
	tk_set_t	*refused)
{
	panic("dcproc_tcif_obtain");
}

/* ARGSUSED */
PRIVATE void
dcproc_tcif_recall(
	tkc_state_t	tclient,
	void		*obj,
	tk_set_t	to_be_recalled,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	dcproc_t	*dcp;
	vproc_t		*vp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = (dcproc_t *)obj;

	ASSERT(to_be_recalled == VPROC_EXISTENCE_TOKENSET);
	ASSERT(unknown == TK_NULLSET);

	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);
	msgerr = invk_dsproc_return(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
					cellid(),
					to_be_recalled, TK_NULLSET,
					unknown, why);
	ASSERT(!msgerr);
	tkc_returned(dcp->dcp_tclient, to_be_recalled, TK_NULLSET);
	vp = BHV_TO_VPROC(&dcp->dcp_bhv);
	vproc_destroy(vp);
}

/*
 *****************************************************************************
 * dcproc incoming message implementation
 *****************************************************************************
 */
void
I_dcproc_recall(
	obj_handle_t	*handle,
	pid_t		pid,
	pid_t		ppid,
	tk_set_t	to_recall,
	tk_disp_t	why)

{
	vproc_t		*vp;
	dcproc_t	*dcp;
	/* REFERENCED */
	int		newstate;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	ASSERT(to_recall == VPROC_EXISTENCE_TOKENSET);

	vp = VPROC_LOOKUP_STATE(pid, ZYES|VPROC_NO_CREATE);
	if (vp == VPROC_NULL) {
		svc = HANDLE_TO_SERVICE(*handle);
		PROC_TRACE2(PROC_TRACE_EXISTENCE, "I_dcproc_recall unknown",
				pid);
		msgerr = invk_dsproc_return_unknown(svc,
				HANDLE_TO_OBJID(*handle),
				cellid(), to_recall, why);
		ASSERT(!msgerr);
		return;
	}

	switch (vp->vp_state) {
	case	SRUN:
		newstate = SZOMB;
		break;
	case	SZOMB:
		newstate = 0;
		break;
	default:
		panic("I_dcproc_recall: bad state");
	}

	dcp = BHV_TO_DCPROC(VPROC_BHV_FIRST(vp));

	/*
	 * If we have the parent here then we will need the vproc
	 * to reap the child so refuse the existence token.
	 * We also refuse the revoke if we are the origin node
	 * for the process.
	 */
	if (newstate == SZOMB &&
	     ((ppid != 1 && pid_is_local(ppid)) ||
	      pid_is_local(vp->vp_pid))) {
		VPROC_SET_STATE(vp, newstate);
		svc = HANDLE_TO_SERVICE(dcp->dcp_handle);
		PROC_TRACE2(PROC_TRACE_EXISTENCE, "I_dcproc_recall refuse",
				pid);
		msgerr = invk_dsproc_return(svc,
					HANDLE_TO_OBJID(dcp->dcp_handle),
					cellid(), TK_NULLSET, to_recall,
					TK_NULLSET, why);
		ASSERT(!msgerr);
		VPROC_RELE(vp);
	} else {
		/*
		 * Prevent the vproc from being looked up
		 */
		PROC_TRACE2(PROC_TRACE_EXISTENCE, "I_dcproc_recall return",
				pid);
		vproc_lookup_prevent(vp);
		vproc_quiesce(vp);
		tkc_release1(dcp->dcp_tclient, VPROC_EXISTENCE_TOKEN);
		tkc_recall(dcp->dcp_tclient, to_recall, why);
		/*
		 * The vproc may be deleted at this point
		 */
	}
}


/*
 **************************************************************************
 * dcproc ops
 **************************************************************************
 */

PRIVATE void
dcproc_destroy(
	bhv_desc_t	*bhv)
{
	dcproc_t	*dcp;
	vproc_t		*vp;

	dcp = BHV_TO_DCPROC(bhv);
	vp = BHV_TO_VPROC(bhv);

	HANDLE_MAKE_NULL(dcp->dcp_handle);
	tkc_destroy(dcp->dcp_tclient);
	bhv_remove(VPROC_BHV_HEAD(vp), &dcp->dcp_bhv);
#if THIS_WORKED
	ASSERT(VPROC_BHV_FIRST(vp) == NULL);
#else
	ASSERT(vp->vp_bhvh.bh_first == NULL);
#endif
	kmem_free(dcp, sizeof(dcproc_t));
}

/* ARGSUSED */
PRIVATE void
dcproc_get_proc(
	bhv_desc_t	*bhv,
	proc_t		**pp,
	int		local_only)
{
	if (local_only) {
		*pp = NULL;
		return;
	}

	panic("inter-cell VPROC_GET_PROC not valid");
}

/* ARGSUSED */
PRIVATE int
dcproc_get_nice(
	bhv_desc_t	*bhv,
	int		*nice,
	struct cred	*credp)
{
	panic("dcproc_get_nice");
	return(0);
}

/* ARGSUSED */
PRIVATE int
dcproc_set_nice(
	bhv_desc_t	*bhv,
	int		*nice,
	struct cred	*cred,
	int		flag)
{
	panic("dcproc_set_nice");
	return(0);
}

/* ARGSUSED */
PRIVATE void
dcproc_set_dir(
	bhv_desc_t	*bhv,
	struct vnode	*vp,
	struct vnode	**vpp,
	int		flag)
{
	panic("dcproc_set_dir");
}

/* ARGSUSED */
PRIVATE int
dcproc_umask(
	bhv_desc_t	*bhv,
	short		cmask,
	short		*omask)
{
	panic("dcproc_umask");
	return(0);
}


/* ARGSUSED */
PRIVATE void
dcproc_get_attr(
	bhv_desc_t	*bhv,
	int		flags,
	vp_get_attr_t	*attr)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_get_attr(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
				flags, attr);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE void
dcproc_get_xattr(
	bhv_desc_t	*bhv,
	int		flags,
	vp_get_xattr_t	*xattr)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_get_xattr(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
				flags, xattr);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE int
dcproc_parent_notify(
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
	dcproc_t	*dcp;
	service_t	svc;
	int		ignore = 0xdead;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	PROC_TRACE4(PROC_TRACE_OPS, "dcproc_parent_notify", pid,
				"parent", BHV_TO_VPROC(bhv)->vp_pid);

	msgerr = invk_dsproc_parent_notify(svc,
			HANDLE_TO_OBJID(dcp->dcp_handle), 
			pid, wcode, wdata, &utime, &stime, pgid, pgseq,
			xstat, &ignore);
	ASSERT(!msgerr);
	ASSERT(ignore != 0xdead);
	return(ignore);
}

/* ARGSUSED */
PRIVATE void
dcproc_reparent(
	bhv_desc_t	*bhv,
	int		to_detach)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_reparent(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
				to_detach);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE void
dcproc_reap(
	bhv_desc_t	*bhv,
	int		flags,
	struct rusage	*rusage,
	struct cpu_mon	*hw_events,
	int		*rtflags)
{
	dcproc_t	*dcp;
	vproc_t		*vp;
	service_t	svc;
	tk_set_t	retset;
	tk_disp_t	why;
	/* REFERENCED */
	int		msgerr;
	size_t		nhw_events = 1;

	dcp = BHV_TO_DCPROC(bhv);
	vp = BHV_TO_VPROC(bhv);

	ASSERT(HANDLE_TO_OBJID(dcp->dcp_handle) != 0);

	vproc_lookup_prevent(vp);
	vproc_quiesce(vp);

	tkc_release1(dcp->dcp_tclient, VPROC_EXISTENCE_TOKEN);
	tkc_returning(dcp->dcp_tclient, VPROC_EXISTENCE_TOKENSET, &retset,
			&why, 0);
	ASSERT(retset == VPROC_EXISTENCE_TOKENSET);
	tkc_returned(dcp->dcp_tclient, retset, TK_NULLSET);

	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	ASSERT(HANDLE_TO_OBJID(dcp->dcp_handle) != 0);
	msgerr = invk_dsproc_reap(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
			cellid(), retset, why, flags, rusage,
			hw_events == NULL ? NULL : &hw_events, &nhw_events,
			rtflags);
	ASSERT(!msgerr);
	vproc_destroy(vp);
}

/* ARGSUSED */
PRIVATE int
dcproc_add_exit_callback(
	bhv_desc_t	*bhv,
	int		flags,
	void		(* func)(void *),
	void		*arg)
{
	panic("dcproc_add_exit_callback");
	return(0);
}

/* ARGSUSED */
PRIVATE void
dcproc_run_exitfuncs(bhv_desc_t *bhv)
{
	panic("dcproc_run_exitfuncs");
}

/* ARGSUSED */
PRIVATE void
dcproc_set_state(
	bhv_desc_t	*bhv,
	int		state)
{
	vproc_t		*vp;

	vp = BHV_TO_VPROC(bhv);

	vproc_set_state(vp, state);
}

/* ARGSUSED */
PRIVATE int
dcproc_thread_state(
	bhv_desc_t	*bhv,
	int		state)
{
	panic("dcproc_run_exitfuncs");
	return 0;
}

/* ARGSUSED */
PRIVATE int
dcproc_sendsig(
	bhv_desc_t	*bhv,
	int		sig,
	int		flags,
	pid_t		sid,
	struct cred	*cred,
	struct k_siginfo *infop)
{
	dcproc_t	*dcp;
	service_t	svc;
	int		error;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	error = invk_dsproc_sendsig(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
			sig, flags, sid, CRED_GETID(cred),
			infop, infop ? 1 : 0);
	ASSERT(error != ECELLDOWN);
	return(error);
}

/* ARGSUSED */
PRIVATE void
dcproc_sig_threads(
	bhv_desc_t	*bhv,
	int		sig,
	k_siginfo_t	*info)
{
	panic("dcproc_sig_threads");
}

/* ARGSUSED */
PRIVATE int
dcproc_sig_thread(
	bhv_desc_t	*bhv,
	ushort_t	id,
	int		sig)
{
	panic("dcproc_sig_thread");
	return 0;
}

/* ARGSUSED */
PRIVATE void
dcproc_prstop_threads(
	bhv_desc_t	*bhv)
{
	panic("dcproc_prstop_threads");
}

/* ARGSUSED */
PRIVATE int
dcproc_prwstop_threads(
	bhv_desc_t	*bhv,
	struct prnode	*pnp)
{
	panic("dcproc_prwstop_threads");
	return(0);
}

/* ARGSUSED */
PRIVATE void
dcproc_prstart_threads(
	bhv_desc_t	*bhv)
{
	panic("dcproc_prstart_threads");
}

PRIVATE int
dcproc_setpgid(
	bhv_desc_t	*bhv,
	pid_t		pgid,
	pid_t		callers_pid,
	pid_t		callers_sid)
{
	dcproc_t	*dcp = BHV_TO_DCPROC(bhv);
	service_t	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);
	int		error;

	error = invk_dsproc_setpgid(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
			pgid, callers_pid, callers_sid);
	ASSERT(error != ECELLDOWN);
	return(error);
}

PRIVATE void
dcproc_pgrp_linkage(
	bhv_desc_t	*bhv,
	pid_t		parent_pgid,
	pid_t		parent_sid)
{
	/* REFERENCED */
	int		msgerr;
	dcproc_t	*dcp = BHV_TO_DCPROC(bhv);
	service_t	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_pgrp_linkage(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
				 parent_pgid, parent_sid);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE int
dcproc_setuid(
	bhv_desc_t	*bhv,
	uid_t		ruid,
	uid_t		euid,
	int		flags)
{
	panic("dcproc_setuid");
	return(0);
}

/* ARGSUSED */
PRIVATE int
dcproc_setgid(
	bhv_desc_t	*bhv,
	gid_t		rgid,
	gid_t		egid,
	int		flags)
{
	panic("dcproc_setgid");
	return(0);
}

/* ARGSUSED */
PRIVATE void
dcproc_setgroups(
	bhv_desc_t	*bhv,
	int		setsize,
	gid_t		*newgids)
{
	panic("dcproc_setgroups");
}

/* ARGSUSED */
PRIVATE int
dcproc_setsid(
	bhv_desc_t	*bhv,
	pid_t		*new_pgid)
{
	panic("dcproc_setsid");
	return(0);
}

/* ARGSUSED */
PRIVATE int 
dcproc_setpgrp(
	bhv_desc_t	*bhv,
	int		flags,
	pid_t		*new_pgid)
{
	panic("dcproc_setpgrp");
	return 1;
}

/* ARGSUSED */
PRIVATE int
dcproc_ctty_access(
	bhv_desc_t	*bhv,
	enum jobcontrol	access)
{
	dcproc_t	*dcp;
	service_t	svc;
	int		error;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	error = invk_dsproc_ctty_access(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
					 access);
	ASSERT(error != ECELLDOWN);
	return(error);
}

/* ARGSUSED */
PRIVATE int
dcproc_ctty_clear(
	bhv_desc_t	*bhv)
{
	dcproc_t	*dcp;
	service_t	svc;
	int		error;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	error = invk_dsproc_ctty_clear(svc, HANDLE_TO_OBJID(dcp->dcp_handle));
	ASSERT(error != ECELLDOWN);
	return(error);
}

/* ARGSUSED */
PRIVATE void
dcproc_ctty_hangup(
	bhv_desc_t	*bhv)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_ctty_hangup(svc, HANDLE_TO_OBJID(dcp->dcp_handle));
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE void
dcproc_get_sigact(
	bhv_desc_t	*bhv,
	int		sig,
	k_sigaction_t	*oact)
{
	panic("dcproc_get_sigact");
}

/* ARGSUSED */
PRIVATE void
dcproc_set_sigact(
	bhv_desc_t	*bhv,
	int		sig,
	void		(*disp)(),
	k_sigset_t	*mask,
	int		flags,
	int		(*sigtramp)())
{
	panic("dcproc_set_sigact");
}

/* ARGSUSED */
PRIVATE struct sigvec_s *
dcproc_get_sigvec(
	bhv_desc_t	*bhv,
	int		flags)
{
	panic("dcproc_get_sigvec");
	return(0);
}

/* ARGSUSED */
PRIVATE void
dcproc_put_sigvec(
	bhv_desc_t	*bhv)
{
	panic("dcproc_put_sigvec");
}

/* ARGSUSED */
PRIVATE int
dcproc_trysig_thread(
	bhv_desc_t	*bhv)
{
	panic("dcproc_trysig_thread");
	return(0);
}

/* ARGSUSED */
PRIVATE void
dcproc_flag_threads(
	bhv_desc_t	*bhv,
	uint_t		flag,
	int		op)
{
	panic("dcproc_flag_threads");
}

PRIVATE void
dcproc_getvpagg(
	bhv_desc_t	*bhv,
	vpagg_t		**vpagp)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_getvpagg(svc, &dcp->dcp_handle, vpagp);
	ASSERT(!msgerr);
}

PRIVATE void
dcproc_setvpagg(
	bhv_desc_t	*bhv,
	vpagg_t		*vpag)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_setvpagg(svc, &dcp->dcp_handle, vpag);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE struct kaiocbhd *
dcproc_getkaiocbhd(
	bhv_desc_t	*bhv)
{
	panic("dcproc_getkaiocbhd");
	/* NOTREACHED */
	return NULL;
}

/* ARGSUSED */
PRIVATE struct kaiocbhd *
dcproc_setkaiocbhd(
	bhv_desc_t		*bhv,
	struct kaiocbhd	*kbhd)
{
	panic("dcproc_setkaiocbhd");
	/* NOTREACHED */
	return NULL;
}

/* ARGSUSED */
PRIVATE void
dcproc_getrlimit(
	bhv_desc_t	*bhv,
	int		which,
	struct rlimit	*lim)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_getrlimit(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
				which, lim);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE int
dcproc_setrlimit(
	bhv_desc_t	*bhv,
	int		which,
	struct rlimit	*lim)
{
	panic("dcproc_setrlimit");
	/* NOTREACHED */
	return 0;
}

/* ARGSUSED */
PRIVATE int
dcproc_exec(
	bhv_desc_t	*bhv,
	struct uarg	*args)
{
	panic("dcproc_exec");
	/* NOTREACHED */
	return 0;
}

/* ARGSUSED */
PRIVATE int
dcproc_set_fpflags(
	bhv_desc_t	*bhv,
	u_char		fpflag,
	int		flags)
{
	panic("dcproc_set_fpflags");
	/* NOTREACHED */
	return 0;
}

/* ARGSUSED */
PRIVATE void
dcproc_set_proxy(
	bhv_desc_t	*bhv,
	int		attribute,
	__psint_t	arg1,
	__psint_t	arg2)
{
	panic("dcproc_set_proxy");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_times(
	bhv_desc_t	*bhv,
	struct tms	*rtime)
{
	panic("dcproc_times");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_read_us_timers(
	bhv_desc_t	*bhv,
	struct timespec	*utimep,
	struct timespec	*stimep)
{
	panic("dcproc_read_us_timers");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_getrusage(
	bhv_desc_t	*bhv,
	int		flags,
	struct rusage	*rup)
{
	panic("dcproc_getrusage");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_get_extacct(
	bhv_desc_t		*bhv,
	struct proc_acct_s	*acct)
{
	panic("dcproc_get_extacct");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_get_acct_timers(
	bhv_desc_t		*bhv,
	struct acct_timers	*timers)
{
	panic("dcproc_get_acct_timers");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_sched_setparam(
	bhv_desc_t	*bhv,
	int		pri,
	int		*error)
{
	panic("dcproc_sched_setparam");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_sched_getparam(
	bhv_desc_t	*bhv,
	struct cred	*cr,
	int		*pri,
	int		*error)
{
	panic("dcproc_sched_getparam");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_sched_setscheduler(
	bhv_desc_t	*bhv,
	int		pri,
	int		policy,
	__int64_t	*rval,
	int		*error)
{
	panic("dcproc_sched_setscheduler");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_sched_getscheduler(
	bhv_desc_t	*bhv,
	struct cred	*cr,
	__int64_t	*rval,
	int		*error)
{
	panic("dcproc_sched_getscheduler");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_sched_rr_get_interval(
	bhv_desc_t	*bhv,
	struct timespec	*timespec)
{
	panic("dcproc_sched_rr_get_interval");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_setinfo_runq(
	bhv_desc_t	*bhv,
	int		rqtype,
	int		arg,
	__int64_t	*rval)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_setinfo_runq(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
				rqtype, arg, rval);
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE void
dcproc_getrtpri(
	bhv_desc_t	*bhv,
	__int64_t	*rval)
{
	panic("dcproc_getrtpri");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_setmaster(
	bhv_desc_t	*bhv,
	pid_t		callers_pid,
	int		*error)
{
	panic("dcproc_setmaster");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_schedmode(
	bhv_desc_t	*bhv,
	int		arg,
	__int64_t	*rval,
	int		*error)
{
	panic("dcproc_schedmode");
	/* NOTREACHED */
}

/* ARGSUSED */
PRIVATE void
dcproc_sched_aff(
	bhv_desc_t	*bhv,
	int		flag,
	__int64_t	*rval)
{
	panic("dcproc_sched_aff");
	/* NOTREACHED */
}

PRIVATE void
dcproc_prnode(
	bhv_desc_t	*bhv,
	int		flag,
        struct vnode    **vnpp)
{
        dcproc_t        *dcp;
        service_t       svc;
	/* REFERENCED */
        int             msgerr;
        cfs_handle_t    vnhandle;
        cfs_handle_t    vfshandle;
        int             try_cnt = 0;

        dcp = BHV_TO_DCPROC(bhv);
        svc = HANDLE_TO_SERVICE(dcp->dcp_handle);
	procfs_export_fs(&vfshandle);
        *vnpp = NULL;
        while (1) {
                msgerr = invk_dsproc_getprnode(svc, 
					       HANDLE_TO_OBJID(dcp->dcp_handle),
					       flag, &vfshandle, &vnhandle);
		ASSERT(!msgerr);

		if (CFS_HANDLE_IS_NULL(vnhandle))
			return;
		cfs_vnimport(&vnhandle, vnpp);
		if (*vnpp != NULL)
			return;
		cell_backoff(++try_cnt);
	}
}

/* ARGSUSED */
PRIVATE void
dcproc_exitwake(
	bhv_desc_t	*bhv)
{
	panic("dcproc_exitwake");
	/* NOTREACHED */
}

PRIVATE int
dcproc_procblk(
	bhv_desc_t	*bhv,
	int		action,
	int		count,
	struct cred	*cr,		/* Callers credentials */
	int		isself)
{
	dcproc_t	*dcp;
	service_t	svc;
	int		error;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	error = invk_dsproc_procblk(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
			 action, count, CRED_GETID(cr), isself);
	ASSERT(error != ECELLDOWN);
	return(error);
}

PRIVATE int
dcproc_prctl(
	bhv_desc_t	*bhv,
	int		option,
	sysarg_t	arg,
	int		isself,
	struct cred	*cr,			/* Callers credentials */
	pid_t		callers_pid,
	rval_t		*rvp)
{
	dcproc_t	*dcp;
	service_t	svc;
	int		error;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	error = invk_dsproc_prctl(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
			option, arg, isself, CRED_GETID(cr),
			callers_pid, rvp);
	ASSERT(error != ECELLDOWN);
	return(error);
}

PRIVATE int
dcproc_set_unblkonexecpid(
	bhv_desc_t	*bhv,
	pid_t		unblkpid)
{
	dcproc_t	*dcp;
	service_t	svc;
	int		error;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	error = invk_dsproc_set_unblkonexecpid(svc,
			HANDLE_TO_OBJID(dcp->dcp_handle), unblkpid);
	ASSERT(error != ECELLDOWN);
	return(error);
}

PRIVATE void
dcproc_unblkpid(
	bhv_desc_t	*bhv)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_unblkpid(svc, HANDLE_TO_OBJID(dcp->dcp_handle));
	ASSERT(!msgerr);
}

/* ARGSUSED */
PRIVATE int
dcproc_unblock_thread(
	bhv_desc_t	*bhv,
	ushort_t	id)
{
	panic("dcproc_unblock_thread");
	return 0;
	/* NOTREACHED */
}

PRIVATE int
dcproc_fdt_dup(
	bhv_desc_t	*bhv,
	vfile_t		*vf,
	int		*newfd)
{
	dcproc_t	*dcp;
	vfile_export_t	vfe;
	int		error;

	dcp = BHV_TO_DCPROC(bhv);

	vfile_export(vf, &vfe);

	error = invk_dsproc_fdt_dup(HANDLE_TO_OBJID(dcp->dcp_handle),
			&vfe, newfd);
	ASSERT(error != ECELLDOWN);

	vfile_export_end(vf);

	return(error);
}

#ifdef CKPT
/* ARGSUSED */
PRIVATE void
dcproc_ckpt_shmget(
	bhv_desc_t	*bhv,
	int		shmid)
{
	/*
	 * TBD...but no ned to panic
	 *
	panic("dcproc_ckpt_shmget");
	 */
}

/* ARGSUSED */
PRIVATE int
dcproc_get_ckpt(
	bhv_desc_t	*bhv,
	int		code,
	void	*arg)
{
	/*
	 * TBD...but no ned to panic
	 *
	panic("dcproc_get_ckpt");
	 */
	return (-1);
}

#endif

PRIVATE void
dcproc_poll_wakeup(
	bhv_desc_t	*bhv,
	ushort_t	id,
	ushort_t	rotor)
{
	dcproc_t	*dcp;
	service_t	svc;
	/* REFERENCED */
	int		msgerr;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	msgerr = invk_dsproc_poll_wakeup(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
			id, rotor);
	ASSERT(!msgerr);

	return;
}

PRIVATE int
dcproc_getcomm(
	bhv_desc_t	*bhv,
	cred_t		*cr,
	char		*bufp,
	size_t		len)
{
	dcproc_t	*dcp;
	service_t	svc;
	int		error;

	dcp = BHV_TO_DCPROC(bhv);
	svc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	ASSERT(bufp);
	ASSERT(0 < len && len <= PSCOMSIZ);
	error = invk_dsproc_getcomm(svc, HANDLE_TO_OBJID(dcp->dcp_handle),
					CRED_GETID(cr), &bufp, &len);
	ASSERT(error != ECELLDOWN);
	return error;
}

PRIVATE void
dcproc_noop(void)
{
	panic("dcproc_noop");
}
