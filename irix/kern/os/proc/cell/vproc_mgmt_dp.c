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

#ident "$Revision: 1.20 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/var.h>
#include <sys/errno.h>
#include <ksys/vproc.h>
#include <ksys/pid.h>
#include <ksys/exception.h>
#include "dproc.h"
#include "pid_private.h"
#include "pproc_private.h"
#include "vproc_private.h"
#include "proc_trace.h"

#include <sys/exec.h>
#include <sys/uthread.h>
#include <sys.s>
#include "vproc_migrate.h"

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dsproc_stubs.h"
#include "I_dcproc_stubs.h"

typedef struct proctable {
	kqueue_t	pt_queue;
	mrlock_t	pt_lock;
} proctab_t;

#define PROCTAB_UPDATE_LOCK(pq)	mrupdate(&(pq)->pt_lock);
#define PROCTAB_ACCESS_LOCK(pq)	mraccess(&(pq)->pt_lock);
#define PROCTAB_UNLOCK(pq)	mrunlock(&(pq)->pt_lock)

proctab_t	*proctab;
int		proctabsz;

#define	PROC_HASH(pid)	(pid%proctabsz)

void
vproc_init(void)
{
	int	i;

	vproc_startup();

	proctabsz = v.v_proc;;
	proctab = (proctab_t *)kern_malloc(sizeof(proctab_t) * proctabsz);

	for (i = 0; i < proctabsz; i++) {
		proctab_t	*pq;

                pq = &proctab[i];
                kqueue_init(&pq->pt_queue);
                mrinit(&pq->pt_lock, "pt");
        }

	vproc_obj_init();

	mesg_handler_register(dsproc_msg_dispatcher, DSPROC_SUBSYSID);
	mesg_handler_register(dcproc_msg_dispatcher, DCPROC_SUBSYSID);
}

int
vproc_create_remote(
	pid_t	pid,
	vproc_t	**vpp)
{
	vproc_t	*vp;
	int	error;

	vp = kmem_zone_zalloc(vproc_zone, KM_SLEEP);
	vp->vp_pid = pid;

	error = dcproc_create(vp);
	if (error) {
		kmem_zone_free(vproc_zone, vp);
		return(error);
	}

	VPROC_REFCNT_LOCK_INIT(vp, "rvproc");
	vp->vp_refcnt = 0;

	PROC_TRACE2(PROC_TRACE_EXISTENCE, "vproc_create_remote", pid);

	*vpp = vp;
	return(0);
}

#ifdef DEBUG
int	vp_maxretry = 0;
#endif

vproc_t *
vproc_lookup_remote(
	pid_t	pid,
	int	flag)
{
	proctab_t	*pq;
	vproc_t		*vp;
	kqueue_t	*kq;
	int		error;
	int		nocreate;
	int		retry_count = 0;

	if (pid < 0 || pid > MAXPID)
		return(VPROC_NULL);

	nocreate = flag & VPROC_NO_CREATE;
	flag &= ~VPROC_NO_CREATE;

retry:
	pq = &proctab[PROC_HASH(pid)];
	PROCTAB_ACCESS_LOCK(pq);

	kq = &pq->pt_queue;
	for (vp = (vproc_t *)kqueue_first(kq);
	     vp != (vproc_t *)kqueue_end(kq);
	     vp = (vproc_t *)kqueue_next(&vp->vp_queue)) {
		if (pid == vp->vp_pid) {
			/*
			 * Got it. Reference it if we can
			 */
			if (VPROC_HOLD_STATE(vp, flag) != 0)
				vp = VPROC_NULL;	/* No reference */
			PROCTAB_UNLOCK(pq);
			return(vp);
		}
	}

	PROCTAB_UNLOCK(pq);

	if (nocreate)
		return(VPROC_NULL);

	/*
	 * We don't have a local client. We have to try to create one
	 */
	error = vproc_create_remote(pid, &vp);
	if (error == EAGAIN) {
		/*
		 * We lost the race with someone else on this cell.
		 * Go back to Go, do not collect $200
		 */
		retry_count++;
		if (retry_count == 1000)
			panic("vproc_lookup_remote loop");
		goto retry;		/* someone else has beaten us to it */
	}

#ifdef DEBUG
	if (retry_count > vp_maxretry)
		vp_maxretry = retry_count;
#endif /* DEBUG */

	if (error) {
		/*
		 * The process really doesn't exist
		 */
		return(VPROC_NULL);
	}

	/*
	 * The process exists and we have a local object, try
	 * to reference it in the requested state
	 */
	error = VPROC_HOLD_STATE(vp, flag);

	/*
	 * Got it. put it in the hash table
	 */
	PROCTAB_UPDATE_LOCK(pq);
	kqueue_enter(&pq->pt_queue, &vp->vp_queue);
	PROCTAB_UNLOCK(pq);

	if (error)
		return(VPROC_NULL);
	else
		return(vp);
}

void
vproc_bhv_write_lock(
	vproc_t	*vp)
{
	pid_t	pid;

	pid = vp->vp_pid;
        if (pid_is_local(pid)) {
		VPROC_RELE(vp);
                vp = pid_to_vproc_locked(pid);
        } else
		BHV_WRITE_LOCK(VPROC_BHV_HEAD(vp));
}

/* ARGSUSED */
vproc_t *
vproc_lookup_remote_locked(
	pid_t	pid)
{
	proctab_t	*pq;
	kqueue_t	*kq;
	vproc_t		*vp;

	pq = &proctab[PROC_HASH(pid)];
	PROCTAB_ACCESS_LOCK(pq);

	kq = &pq->pt_queue;
	for (vp = (vproc_t *)kqueue_first(kq);
	     vp != (vproc_t *)kqueue_end(kq);
	     vp = (vproc_t *)kqueue_next(&vp->vp_queue)) {
		if (pid == vp->vp_pid) {
			/* Dunno wadda do yet */
			panic("vproc_lookup_remote_locked");
			/*
			 * Got it. Reference it if we can
			 */
			if (VPROC_HOLD_STATE(vp, ZYES) != 0)
				vp = VPROC_NULL;	/* No reference */
			PROCTAB_UNLOCK(pq);
			return(vp);
		}
	}

	PROCTAB_UNLOCK(pq);

	return(VPROC_NULL);
}

vproc_t *
vproc_create_embryo(
	pid_t	pid)
{
	vproc_t		*vp;
	proctab_t	*pq;

	vp = vproc_create();
	VPROC_HOLD(vp);
	BHV_WRITE_LOCK(VPROC_BHV_HEAD(vp));
	vp->vp_pid = pid;

	pq = &proctab[PROC_HASH(pid)];
	PROCTAB_UPDATE_LOCK(pq);
	kqueue_enter(&pq->pt_queue, &vp->vp_queue);
	PROCTAB_UNLOCK(pq);
	return(vp);
}

vproc_t *
vproc_lookup(
	pid_t	pid,
	int	flag)
{
	vproc_t	*vp;

	if (pid_is_local(pid)) {
		/*
		 * pid_to_vproc doesn't understand VPROC_NO_CREATE
		 */
		flag &= ~VPROC_NO_CREATE;
		vp = pid_to_vproc(pid, flag);
	} else 
		vp = vproc_lookup_remote(pid, flag);
	return(vp);
}

void
vproc_lookup_prevent(
	vproc_t		*vp)
{
	PROC_TRACE2(PROC_TRACE_STATE, "vproc_lookup_prevent", vp->vp_pid);
	if (pid_is_local(vp->vp_pid))
		PID_PROC_FREE(vp->vp_pid);
	else {
		proctab_t	*pq;

		pq = &proctab[PROC_HASH(vp->vp_pid)];
		PROCTAB_UPDATE_LOCK(pq);
		kqueue_remove(&vp->vp_queue);
		PROCTAB_UNLOCK(pq);
	}
}

int
exec_complete(
	struct uarg	*args)
{
	int	error;
	extern int pproc_exec_complete(struct uarg *);

	error = pproc_exec_complete(args);
	return(error);
}

int
vproc_exec(
	vproc_t	*vp,
	struct uarg *args)
{
	int		error;


	if (args->ua_cell != cellid()) {
		migrate_info_t	emi;
		uthread_t	*ut;

		ut = curuthread;
		emi.mi_call = SYS_rexec_complete;
		emi.mi_arg = args;
		emi.mi_argsz = sizeof(*args);
		UT_TO_PROC(ut)->p_migrate = &emi;
		error = vproc_migrate(vp, args->ua_cell);
		/*
		 * Here only on failure.
		 */
		ASSERT(error);
		return(error);
	}

	/* Once we call vproc_exec, any subsequent errors are cleaned
	 * up on the 'physical' side - i.e. pproc_exec. Thus we clear
	 * out the uarg cleanup bits, to indicate to upper layers that
	 * necessary cleanup has been handled.
	 */
	args->ua_exec_cleanup = 0;
	VPROC_EXEC(vp, args, error);
	return(error);
}


extern void syscall_rewind(int, eframe_t *);

void
rexec_immigrate_bootstrap(
	migrate_info_t	*mip)
{
	eframe_t	*ep = &curexceptionp->u_eframe;
	int		syscall;

	ep->ef_a0 = (__psint_t) mip->mi_arg; 
	ep->ef_a1 = (__psint_t) mip->mi_argsz;
	syscall = mip->mi_call;

	kmem_free(mip, sizeof(migrate_info_t));

	syscall_rewind(syscall, ep);
	/* NOT_REACHED */
}

struct rexec_completea {
	struct uarg	*argp;
	size_t		argsz;
};

/* ARGSUSED */
int
rexec_complete(
	struct rexec_completea *uap,
	rval_t *rvp)
{
	int	error;

	error = exec_complete(uap->argp);

	kmem_free(uap->argp, uap->argsz);
 
	return error;
}

int
vproc_is_local(
	vproc_t		*vp)
{
	bhv_desc_t	*bhv;

	BHV_READ_LOCK(VPROC_BHV_HEAD(vp)); 
	for (bhv = VPROC_BHV_FIRST(vp); bhv != NULL; bhv = bhv->bd_next) {
	        if (BHV_POSITION(bhv) == VPROC_BHV_PP) {
		        BHV_READ_UNLOCK(VPROC_BHV_HEAD(vp)); 
			return (1);
		}
	}
	BHV_READ_UNLOCK(VPROC_BHV_HEAD(vp)); 
        return (0);
}

int
remote_procscan(
	int	(*pfunc)(proc_t *, void *, int),
	void	*arg)
{
	int	i;

	for (i = 0; i < proctabsz; i++) {
		proctab_t	*pq;
		kqueue_t	*kq;
		vproc_t		*vp;
		proc_t		*p;
		int		stop_scan = 0;

		pq = &proctab[i];
		kq = &pq->pt_queue;
		PROCTAB_ACCESS_LOCK(pq);
		for (vp = (vproc_t *)kqueue_first(kq);
		     vp != (vproc_t *)kqueue_end(kq) && !stop_scan;
		     vp = (vproc_t *)kqueue_next(&vp->vp_queue)) {

			if (VPROC_HOLD_STATE(vp, ZYES) == 0) {
				PROCTAB_UNLOCK(pq);
				VPROC_GET_PROC_LOCAL(vp, &p);
				if (p != NULL)
					stop_scan = (*pfunc)(p, arg, 1);
				PROCTAB_ACCESS_LOCK(pq);
				VPROC_RELE(vp);
			}
		}
		PROCTAB_UNLOCK(pq);
		if (stop_scan)
			return(stop_scan);
	}
	return(0);
}

vproc_t *
idbg_vproc_lookup_remote(
	pid_t	pid)
{
	proctab_t	*pq;
	kqueue_t	*kq;
	vproc_t		*vp;

	pq = &proctab[PROC_HASH(pid)];
	kq = &pq->pt_queue;

	for (vp = (vproc_t *)kqueue_first(kq);
	     vp != (vproc_t *)kqueue_end(kq);
	     vp = (vproc_t *)kqueue_next(&vp->vp_queue)) {
		if (pid == vp->vp_pid)
			return(vp);
	}

	return(VPROC_NULL);
}


vproc_t *
idbg_vproc_lookup(
	pid_t	pid)
{
	vproc_t	*vpr;

	if (pid_is_local(pid))
		vpr = idbg_pid_to_vproc(pid);
	else 
		vpr = idbg_vproc_lookup_remote(pid);
	return(vpr);
}

int
idbg_remote_procscan(
	int	(*pfunc)(proc_t *, void *, int),
	void	*arg)
{
	int	i;

	for (i = 0; i < proctabsz; i++) {
		proctab_t	*pq;
		kqueue_t	*kq;
		vproc_t		*vp;
		proc_t		*p;

                pq = &proctab[i];
		kq = &pq->pt_queue;
		for (vp = (vproc_t *)kqueue_first(kq);
		     vp != (vproc_t *)kqueue_end(kq);
		     vp = (vproc_t *)kqueue_next(&vp->vp_queue)) {
			int	stop_scan;

			IDBG_VPROC_GET_PROC(vp, &p);
			if (p == NULL)
				continue;
			stop_scan = (*pfunc)(p, arg, 1);
			if (stop_scan)
				return(stop_scan);
		}
	}
	return(0);
}
