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

#ident "$Id: vproc_migrate.c,v 1.12 1997/10/06 14:47:52 cp Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ksa.h>

#include <ksys/cell.h>
#include <ksys/vproc.h>
#include <ksys/cell/relocation.h>

#include "proc_trace.h"
#include "pid_private.h"
#include "vproc_private.h"
#include "dsproc.h"
#include "dcproc.h"
#include "vproc_migrate.h"
#include "pproc_migrate.h"
#include "pproc.h"

#include "invk_dcproc_stubs.h"
#include "invk_dsproc_stubs.h"

#ifdef DEBUG
#define PRIVATE
#else
#define PRIVATE static
#endif

PRIVATE int vproc_obj_source_prepare(obj_manifest_t *, void *);
PRIVATE int vproc_obj_target_prepare(obj_manifest_t *, void **);
PRIVATE int vproc_obj_source_bag(obj_manifest_t *, obj_bag_t);
PRIVATE int vproc_obj_target_unbag(obj_manifest_t *, obj_bag_t);
PRIVATE int vproc_obj_source_end(obj_manifest_t *); 
PRIVATE int vproc_obj_source_abort(obj_manifest_t *); 
PRIVATE int vproc_obj_target_abort(obj_manifest_t *);

obj_relocation_if_t vproc_obj_iface = {
	vproc_obj_source_prepare,
	vproc_obj_target_prepare,
	NULL,				/* vproc_obj_source_retarget */
	NULL,				/* vproc_obj_client_retarget */
	vproc_obj_source_bag,
	vproc_obj_target_unbag,
	vproc_obj_source_end,
	vproc_obj_source_abort,
	vproc_obj_target_abort
};

typedef struct vproc_src_prepare {
	pid_t		vsp_pid;
	tk_set_t	vsp_tokens;
} vproc_src_prepare_t;

void
vproc_obj_init()
{
	service_t	svc;

	SERVICE_MAKE(svc, cellid(), SVC_PROCESS_MGMT);
	obj_service_register(svc, NULL, &vproc_obj_iface);
}

int
vproc_migrate(
	vproc_t	*vp,
	cell_t	target_cell)
{
	int		error;
	obj_manifest_t	*mfst;

	if (target_cell == cellid())
		return(0);

	PROC_TRACE2(PROC_TRACE_MIGRATE, "vproc_migrate", vp->vp_pid);
	mfst = obj_mft_create(target_cell);
	error = obj_mft_put(mfst, (void *)vp, SVC_PROCESS_MGMT,
			OBJ_RMODE_SERVER);
	if (error == OBJ_SUCCESS)
		error = obj_mft_ship(mfst);
	obj_mft_destroy(mfst);

	if (!error) {
		/*
		 * Terminate exec'ing uthread and destroy pproc/unthread.
		 */
		uthread_t	*ut = curuthread;

		/*
		 * utas_segtbl NULLing avoids an assertion failure in exit.
		 */
		ut->ut_as.utas_segtbl = NULL;

		splhi();

		BHV_WRITE_UNLOCK(VPROC_BHV_HEAD(vp));
		VPROC_RELE(vp);
		ut->ut_vproc = NULL;
		pproc_destroy(&UT_TO_PROC(ut)->p_bhv);
		ut->ut_proc = NULL;	/* XXX */

		utexitswtch();
		ASSERT(0);
		/* NOTREACHED */
	}

	return(error);
}

/*
 * CORE Callout functions
 */
PRIVATE int
vproc_obj_source_prepare(
	obj_manifest_t	*mfst,
	void		*vobj)
{
	vproc_t			*vp;
	dsproc_t		*dsp;
	int			error;
	tk_set_t		granted;
	tk_set_t		refused;
	tk_set_t		held;
	service_t		svc;
	vproc_src_prepare_t	src_prepare;
	obj_mft_info_t		minfo;

	vp = vobj;
	PROC_TRACE2(PROC_TRACE_MIGRATE, "vproc_obj_source_prepare", vp->vp_pid);

	VPROC_HOLD(vp);
	error = vproc_to_dsproc(vp, &dsp);
	if (error)
		return(error);
	dsproc_lock(dsp);
	if ((dsp->dsp_flags&VPROC_EMIGRATING) ||
	    (dsp->dsp_flags&VPROC_IMMIGRATING)) {
		dsproc_unlock(dsp);
		return(EMIGRATING);
	}
	dsp->dsp_flags |= VPROC_EMIGRATING;
	dsproc_unlock(dsp);
	
	obj_mft_info_get(mfst, &minfo);		/* for target_cell */
	tks_obtain(dsp->dsp_tserver, minfo.target_cell,
		   VPROC_EXISTENCE_TOKENSET, &granted, &refused, &held);
        ASSERT((granted == TK_NULLSET && held == VPROC_EXISTENCE_TOKENSET) ||
               (granted == VPROC_EXISTENCE_TOKENSET && held == TK_NULLSET));


	src_prepare.vsp_pid = vp->vp_pid;
	src_prepare.vsp_tokens = granted;
	SERVICE_MAKE(svc, cellid(), SVC_PROCESS_MGMT);
	HANDLE_MAKE(minfo.source.hndl, svc, vp);
	minfo.source.tag = VPROC_OBJ_TAG_SRC_PREP;
	minfo.source.info_size = sizeof(src_prepare);
	minfo.source.infop = &src_prepare;
	obj_mft_info_put(mfst, &minfo);

	error = pproc_source_prepare(BHV_NEXT(&dsp->dsp_bhv),
					minfo.target_cell, mfst);
	ASSERT(!error);

	return(0);
}

PRIVATE int
vproc_obj_target_prepare(
	obj_manifest_t	*mfst,
	void		**vobjp)
{
	vproc_t			*vp;
	dsproc_t		*dsp;
	dcproc_t		*dcp;
	vproc_src_prepare_t	src_prepare;
	service_t		svc;
	obj_mft_info_t		minfo;
	int			loc_npalloc;
	cred_t			*cr = get_current_cred();
	extern int nproc;

	minfo.source.tag = VPROC_OBJ_TAG_SRC_PREP;
	minfo.source.info_size = sizeof(src_prepare);
	minfo.source.infop = &src_prepare;
	obj_mft_info_get(mfst, &minfo);

	PROC_TRACE2(PROC_TRACE_MIGRATE, "vproc_obj_target_prepare", 
			src_prepare.vsp_pid);

	/*
	 * Before any ado ... check we can accept another proc here.
	 *
	 * Track number of currently allocated proc structures.
	 * This enforces the notion of 'nproc'.
	 */
	loc_npalloc = atomicAddInt(&npalloc, 1);

	/* Enforce system nproc limits: If we are not privileged, we
	 * cannot take the last process (npalloc == nproc). If npalloc
	 * is > than nproc, then we have gone over the limit.
	 */
	if (loc_npalloc >= nproc) {
		if (loc_npalloc > nproc || (cr->cr_uid && cr->cr_ruid)) {
			(void) atomicAddInt(&npalloc, -1);
			SYSERR.procovf++;
			return EAGAIN;
		}
	}

	if (src_prepare.vsp_tokens == TK_NULLSET) {
		int	retries = 0;

		while ((vp = VPROC_LOOKUP(src_prepare.vsp_pid)) == VPROC_NULL)
			cell_backoff(++retries);

		BHV_WRITE_LOCK(VPROC_BHV_HEAD(vp));
		dcp = BHV_TO_DCPROC(VPROC_BHV_FIRST(vp));
		bhv_remove(VPROC_BHV_HEAD(vp), VPROC_BHV_FIRST(vp));

		/*
		 * We are only using the existence token, otherwise we need
		 * to return all other tokens at this point.
		 */
		tkc_free(dcp->dcp_tclient);
		kmem_free(dcp, sizeof(dcproc_t));
	} else
		vp = vproc_create_embryo(src_prepare.vsp_pid);
	
	/*
	 * create the dsproc structure
	 */
	dsp = dsproc_alloc(vp);
	dsp->dsp_flags |= VPROC_IMMIGRATING;

	SERVICE_MAKE(svc, cellid(), SVC_PROCESS_MGMT);
	HANDLE_MAKE(minfo.target.hndl, svc, vp);

	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_put(mfst, &minfo);

	pproc_obj_target_prepare(vp, mfst);

	*vobjp = vp;
	VPROC_RELE(vp);
	ASSERT(BHV_IS_WRITE_LOCKED(VPROC_BHV_HEAD(vp)));
	return(0);
}

tks_iter_t
vproc_client_retarget(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		ap)
{
	dsproc_t	*dsp		= (dsproc_t *)obj;
	cell_t		target_cell	= va_arg(ap, cell_t);
	objid_t		target_objid	= va_arg(ap, objid_t);
	obj_retarget_t	*rt_syncp	= va_arg(ap, void *);
        service_t	svc;
	vproc_t		*vp;
	/* REFERENCED */
	int		msgerr;

	if (tokens_owned != VPROC_EXISTENCE_TOKENSET)
		panic("vproc_client_retarget");

	vp = BHV_TO_VPROC(&dsp->dsp_bhv);

	if (client != cellid() && client != target_cell) {
		PROC_TRACE4(PROC_TRACE_MIGRATE,
				"vproc_client_retarget", vp->vp_pid,
				"retargeting cell", client);
		SERVICE_MAKE(svc, (cell_t)client, SVC_PROCESS_MGMT);
		obj_retarget_register(rt_syncp, client);
		msgerr = invk_dcproc_client_retarget(svc, vp->vp_pid,
				target_cell, target_objid, rt_syncp);
		ASSERT(!msgerr);
	}
	return(TKS_CONTINUE);
}


void
vproc_obj_client_retarget(
	dsproc_t	*dsp,
	cell_t		target_cell,
	objid_t		target_objid)
{
	obj_retarget_t	rt_sync;

	obj_retarget_begin(&rt_sync);

	(void) tks_iterate(dsp->dsp_tserver,
			VPROC_EXISTENCE_TOKENSET,
			TKS_STABLE,
			vproc_client_retarget,
			target_cell,
			target_objid,
			PHYS_TO_K0(kvtophys(&rt_sync)));

	obj_retarget_wait(&rt_sync);
	obj_retarget_end(&rt_sync);
}

PRIVATE int
vproc_obj_source_bag(
	obj_manifest_t	*mfst,
	obj_bag_t	source_bag)
{
	int		error;
	vproc_t		*vp;
	dsproc_t	*dsp;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mfst, &minfo);

	vp = HANDLE_TO_OBJID(minfo.source.hndl);
	PROC_TRACE2(PROC_TRACE_MIGRATE, "vproc_obj_source_bag", vp->vp_pid);

	/*
	 * We still have a reference on this vproc from source_prepare
	 */
	error = vproc_to_dsproc(vp, &dsp);
        ASSERT(!error);

	vproc_obj_client_retarget(dsp, minfo.target_cell,
			HANDLE_TO_OBJID(minfo.target.hndl));

	/*
	 * Due to the way set_state works, we have to somehow drop this
	 * reference, take the behavior chain write lock and regain the
	 * reference.
	 */
	vproc_bhv_write_lock(vp);

	dcproc_alloc(vp, &minfo.target.hndl);

	/*
	 * Although we have the dc in place, we cannot unlock things.
	 * At this stage we'd like to go
	 * 	BHV_WRITE_UNLOCK(VPROC_BHV_HEAD(vp));
	 *	VPROC_RELE(vp);
	 * but that would allow the target cell to complete its exec
	 * and potentially go on to do another back to this cell. Then
	 * we might stumble over the tail of this first migration. By
	 * removing the ds/p portion on the chain, we could avoid this.
	 * Unfortunately, doing this would break the link from the v
	 * object to the ds/p which subsequent KORE steps depend on.
	 */

	tks_bag(dsp->dsp_tserver, source_bag);

	pproc_source_bag(BHV_NEXT(&dsp->dsp_bhv),
			minfo.target_cell, source_bag);

	return(0);
}

PRIVATE int
vproc_obj_target_unbag(
	obj_manifest_t	*mfst,
	obj_bag_t	bag)
{
	vproc_t		*vp;
	dsproc_t	*dsp;
	int		error;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mfst, &minfo);

	vp = HANDLE_TO_OBJID(minfo.target.hndl);
	PROC_TRACE2(PROC_TRACE_MIGRATE, "vproc_obj_target_unbag", vp->vp_pid);
	ASSERT(BHV_IS_WRITE_LOCKED(VPROC_BHV_HEAD(vp)));
	dsp = BHV_TO_DSPROC(VPROC_BHV_FIRST(vp));
	ASSERT(dsp->dsp_flags & VPROC_IMMIGRATING);

	error = tks_unbag(dsp->dsp_tserver, bag);
	if (error)
		return(error);

	pproc_target_unbag(BHV_NEXT(&dsp->dsp_bhv), bag);

	dsproc_lock(dsp);
	dsp->dsp_flags &= ~VPROC_IMMIGRATING;
	sv_broadcast(&dsp->dsp_wait);
	dsproc_unlock(dsp);
	BHV_WRITE_UNLOCK(VPROC_BHV_HEAD(vp));

	return(0);
}

PRIVATE int
vproc_obj_source_end(
	obj_manifest_t	*mfst)
{
	vproc_t		*vp;
	dsproc_t	*dsp;
	bhv_desc_t	*bhv;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mfst, &minfo);

	vp = HANDLE_TO_OBJID(minfo.source.hndl);
	PROC_TRACE2(PROC_TRACE_MIGRATE, "vproc_obj_source_end", vp->vp_pid);
	bhv = VPROC_BHV_FIRST(vp);		/* the dcproc */
	bhv = BHV_NEXT(bhv);			/* the dsproc */
	dsp = BHV_TO_DSPROC(bhv);
	bhv = BHV_NEXT(bhv);			/* the pproc */

	dsproc_free(vp, dsp);

	pproc_source_end(bhv);

	return(0);
}

/* ARGSUSED */
PRIVATE int
vproc_obj_source_abort(
	obj_manifest_t	*mfst)
{
	panic("vproc_obj_source_abort");
	return(0);
}

/* ARGSUSED */
PRIVATE int
vproc_obj_target_abort(
	obj_manifest_t	*mfst)
{
	panic("vproc_obj_target_abort");
	return(0);
}

PRIVATE void
dcproc_client_retarget_bhv_callout(
	bhv_head_t	*bhvh,
	void		*arg1,
	void		*arg2,
	void		*arg3,
	void		*retarget_id)
{
	service_t	svc;
	service_t	oldsvc;
	dcproc_t	*dcp;
	vproc_t		*vp = (vproc_t *) arg1;
	cell_t		new_server = (cell_t) (__psint_t) arg2;
	objid_t		new_objid = (objid_t) arg3;
	/* REFERENCED */
	int		msgerr;

	ASSERT(VPROC_BHV_HEAD(vp) == bhvh);
	PROC_TRACE4(PROC_TRACE_MIGRATE, "I_dcproc_client_retarget", vp->vp_pid,
			"vproc", vp);

	dcp = BHV_TO_DCPROC(VPROC_BHV_FIRST(vp));

	oldsvc = HANDLE_TO_SERVICE(dcp->dcp_handle);

	SERVICE_MAKE(svc, new_server, SVC_PROCESS_MGMT);
	HANDLE_MAKE(dcp->dcp_handle, svc, new_objid);

	VPROC_RELE(vp);

	msgerr = invk_dsproc_client_retargetted(oldsvc, cellid(), retarget_id);
	ASSERT(!msgerr);
}

/*
 * Client side migration messages
 */
void
I_dcproc_client_retarget(
	pid_t	pid,
	cell_t	new_server,
	objid_t	new_objid,
	void	*retarget_id)
{
	int		lookup_retry = 0;
	int		n;
	vproc_t		*vp;

	while ((vp = VPROC_LOOKUP_STATE(pid, ZYES)) == VPROC_NULL) {
		lookup_retry++;
		cell_backoff(lookup_retry);
	}

	n = BHV_WRITE_LOCK_CALLOUT(VPROC_BHV_HEAD(vp),
			&dcproc_client_retarget_bhv_callout,
			(void *) vp,
			(void *) (__psint_t) new_server,
			(void *) new_objid,
			retarget_id);

	PROC_TRACE4(PROC_TRACE_MIGRATE, "I_dcproc_client_retarget", pid,
			"callout req", n);
}

/*
 * Server side migration messages
 */
void
I_dsproc_client_retargetted(
	cell_t	client,
	void	*rt_sync)
{
	obj_retarget_deregister(rt_sync, client);
}
