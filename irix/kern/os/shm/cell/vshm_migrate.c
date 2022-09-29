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
#ident "$Id: vshm_migrate.c,v 1.22 1997/06/05 21:55:38 sp Exp $"

/*
 * Vshm object relocation management.
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/shm.h>
#include <sys/systm.h>

#include <ksys/cell/tkm.h>
#include <ksys/vshm.h>
#include <ksys/cell/object.h>

#include "dshm.h"
#include "vshm_migrate.h"
#include "I_dsshm_stubs.h"
#include "invk_dsshm_stubs.h"
#include "invk_dcshm_stubs.h"
#include "vshm_mgr.h"
#include "vshm_private.h"

extern tkc_ifstate_t dcshm_tclient_iface;

/*
 * This is a prototype implementation for vshm object migration.
 * A push model is used via sysmp(MP_SHM_MIGRATE,...)
 * to relocate a local shared memory area to another cell.
 *
 * This implementation uses the Client Retargetting Model driven\
 * by the Common Object Relocation Engine - see dp/object.c.
 * In summary, this involves the following steps for each vshm
 * relocated:
 *   - As a prerequisite, a distributed behavior (ds) is interposed
 *     at the source cell.
 *   - A small bag of object information is assembled on the source
 *     cell to identify the object. The target cell is sent this
 *     information so that it can prepare to receive the object.
 *   - The target cell creates an embrionic ds behavior and returns
 *     a new object id to the source.
 *   - The source cell contacts all clients (other than the target)
 *     requesting them to drain pending operations from the source and
 *     to direct future operations to the target object.
 *   - Each client reports back to the source cell when it has retargetted.
 *   - When all clients have reported, the source moves its ds and p
 *     behaviors aside and installs a dc referencing the target object.
 *   - The source packages the object state (both physical level and server)
 *     for transfer to the target and destroy the physical object.
 *   - The target imports the object state and allows server operations
 *     to commence. Retargetted operations may have been blocked waiting.
 *  This sequence is discussed in greater detail in the "Object Relocation
 *  Model" document.
 */

/*
 * Quiesce local client operations-in-progress.
 */
void
vshm_client_quiesce(
	vshm_t	*vsp)
{
	bhv_head_t	*bhp = VSHM_BHV_HEAD_PTR(vsp);
	
	/*
	 * Quiesce this client.
	 * Note: other object types will need to wake-up interruptible ops;
	 * vshm has none though.
	 */
	BHV_WRITE_LOCK(bhp);
}

/*
 * Unquiesce local client operations-in-progress.
 */
void
vshm_client_unquiesce(
	vshm_t	*vsp)
{
	bhv_head_t	*bhp = VSHM_BHV_HEAD_PTR(vsp);
	BHV_WRITE_UNLOCK(bhp);
}

/*
 * Export a pshm.
 */
void
pshm_bag(
	pshm_t		*psp,
	obj_bag_t	bag)
{
	obj_bag_put(bag, OBJ_TAG_PSHM, (void *)psp, sizeof(pshm_t));
}

/*
 * Construct a new pshm and unbag the bagged state.
 */
void
pshm_unbag(
	vshm_t		*vsp,
	obj_bag_t	bag)
{
	/* REFERENCED */
	int		error;
	pshm_t		*psp = kern_malloc(sizeof(pshm_t));

	/*
	 * Blindly copy in the source state.
	 */
	ASSERT(psp);
	obj_bag_get_here(bag, OBJ_TAG_PSHM, psp, sizeof(pshm_t), error);
	ASSERT(error == OBJ_BAG_SUCCESS);

	/*
	 * Fully initialize destination copy and insert into
	 * behavior chain.
	 */
	mutex_init(&psp->psm_lock, MUTEX_DEFAULT, "pshmlk");
	bhv_desc_init(&psp->psm_bd, psp, vsp, &pshm_ops);
	bhv_insert(VSHM_BHV_HEAD_PTR(vsp), &psp->psm_bd);

	/*
	 * The pshm was removed.
	 * The vshm must be removed here too, although the region
	 * will already have been so we must finesse pshm_free.
	 */
	if ((psp->psm_perm.mode & IPC_ALLOC) == 0) {
		vshm_remove(vsp, 0);
		psp->psm_perm.mode |= IPC_ALLOC;
	}
}

/*
 * This routine is called by a client cell when it has completed
 * its retargetting to the target server cell.
 */
void
I_dsshm_obj_retargetted(
	cell_t		client,
	int		vshm_id,
	objid_t		objid,
	void		*rt_sync)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	/* REFERENCED */
	vshm_t		*vsp = BHV_TO_VSHM(bdp);
	
	ASSERT(vshm_id == vsp->vsm_id);

	/*
	 * Count down and signal when all clients have reported.
	 */
	obj_retarget_deregister(rt_sync, client);
}

/*
 * Mobilize source.
 * Prepare vshm object for relocation if the server/physical object is local.
 * This is fraught with races: the vshm can may be coming or going
 * when we attempting to do this.
 */
int
vshm_obj_mobilize(
	vshm_t		*vsp)
{
	int		error = 0;
	bhv_head_t	*bhp = VSHM_BHV_HEAD_PTR(vsp);
	bhv_desc_t	*bdp;
	dsshm_t		*dsp;

	/*
	 * Standard sequence to check that a ds is present,
	 * or to install one if possible.
	 */
	BHV_READ_LOCK(bhp);
	bdp = VSHM_TO_FIRST_BHV(vsp);
	if (BHV_OPS(bdp) == &pshm_ops) {
		/*
		 * The object is local and non-distributed - we need
		 * to interpose a ds.
		 */
		BHV_READ_UNLOCK(bhp);
		BHV_WRITE_LOCK(bhp);
		bdp = VSHM_TO_FIRST_BHV(vsp);
		if (BHV_OPS(bdp) == &pshm_ops) {
			vshm_interpose(vsp);
			bdp = VSHM_TO_FIRST_BHV(vsp);
		}
		BHV_WRITE_TO_READ(bhp);
	}
	if (BHV_OPS(bdp) == &dcshm_ops) {
		/*
		 * The object isn't here.
		 */
		error = EMIGRATED;
		goto out;
	}

	/*
	 * Another thread may get here first. Check, then update
	 * the relocation state if we won.
	 */
	dsp = BHV_TO_DSSHM(bdp);
	error = obj_SR_source_begin(&dsp->dssm_obj_state);
	if (error == OBJ_SUCCESS) {
		/*
		 * After all that - has the vshm been removed?
		 * If so, there's no point in relocating it.
		 */
		if (vshm_is_removed(vsp)) {
			/* Reset the outbound state */
			obj_SR_source_abort(&dsp->dssm_obj_state);
			error = ENOENT; 
		} else {
			VSHM_TRACE4("vshm_obj_mobilize", vsp->vsm_id,
				    "on cell", cellid());
		}
	}
out: 
	BHV_READ_UNLOCK(bhp);
	return error;
}

/*
 * Relocate vshm object (server) from local source cell to target cell.
 */
int
vshm_obj_relocate(
	vshm_t		*vsp,
	cell_t		target_cell)
{
	int		error;
	obj_manifest_t	*mftp;

	if (target_cell == cellid())
		return 0;		/* That's easy */
	VSHM_TRACE6("vshm_obj_relocate", vsp->vsm_id,
		    "from", cellid(), "to", target_cell);
	
	/*
	 * Call the object relocator to transfer the object.
	 */
	mftp = obj_mft_create(target_cell);
	error = obj_mft_put(mftp, (void *) vsp, SVC_SVIPC, OBJ_RMODE_SERVER);
	if (error == OBJ_SUCCESS)
		error = obj_mft_ship(mftp);
	obj_mft_destroy(mftp);

	return error;
}

#define VSHM_TO_OBJID(vsp)	((objid_t) (vsp))
#define HANDLE_TO_VSHM(handle)	((vshm_t *) HANDLE_TO_OBJID(handle))

static void
vshm_obj_select_local(
	vshm_t          *vsp,
	void		*arg)
{
	service_t	svc;
	obj_list_t	obj_list = (obj_list_t) arg;
	obj_handle_t	obj_handle;
	bhv_desc_t	*bdp = vsp->vsm_bh.bh_first;
			/*
			 * Note: VSHM_TO_FIRST_BHV might assert here
			 * because object may be in flux, and
			 * we don't take the chain lock.
			 */

	if (bdp && vshm_islocal(bdp)) {
		vshm_ref(vsp);			/* Provide extra ref */
		SERVICE_MAKE(svc, cellid(), SVC_SVIPC);
		HANDLE_MAKE(obj_handle, svc, VSHM_TO_OBJID(vsp));
		obj_list_put(obj_list, &obj_handle);
		VSHM_TRACE4("vshm_obj_select", vsp->vsm_id, "cell", cellid());
	}
}


/*
 * Relocated all local objects to another cell.
 * This involves scanning the vshm hash table to build a list of local
 * objects suitable for relocation and invoking the Common Object
 * Relocation Engine to drive the relocation.
 */
int
vshm_obj_evict(
	cell_t		target_cell)
{
	int		error;
	int		nobj;
	obj_list_t	local_vshm_list;
	obj_manifest_t	*mftp;
	obj_handle_t	obj_handle;

	if (target_cell == cellid())
		return EINVAL;		/* That's odd */

	local_vshm_list = obj_list_create(100);
	mftp = obj_mft_create(target_cell);

	/*
	 * Iterate over all vshms in the local hash table
	 * building a list of all local vshms (excluding client
	 * vshms).
	 */
	vshm_iterate(vshm_obj_select_local, (void *) local_vshm_list);

	/*
	 * Further filter only those objects not already
	 * undergoing relocation - in or out.
	 * Note a ds may get interposed so that we must ensure
	 * we quote the correct object id.
	 */
	obj_list_rewind(local_vshm_list);
	nobj = 0;
	while (obj_list_get(local_vshm_list, &obj_handle) == OBJ_SUCCESS) {
		error = obj_mft_put(mftp, (void *) HANDLE_TO_VSHM(obj_handle),
					  SVC_SVIPC, OBJ_RMODE_SERVER);
		if (error == OBJ_SUCCESS)
			nobj++;
	}

	if (nobj > 0)
		error = obj_mft_ship(mftp);
	else
		error = ENOENT;

	/*
	 * Dereference all selected objects, each was given a
	 * protective ref when selected for relocation.
	 */
	obj_list_rewind(local_vshm_list);
	while (obj_list_get(local_vshm_list, &obj_handle) == OBJ_SUCCESS) {
		vshm_rele(HANDLE_TO_VSHM(obj_handle));
	}

	obj_list_destroy(local_vshm_list);
	obj_mft_destroy(mftp);

	return error;
}
	

/*
 * The following routines implement object relocation call-outs which
 * CORE makes into the vshm subsystem to perform the vshm relocation.
 */
int
vshm_obj_source_prepare(
	obj_manifest_t	*mftp,		/* IN object manifest */
	void		*v)		/* IN vshm pointer */
{
	vshm_t		*vsp = (vshm_t *) v;
	bhv_desc_t	*bdp;
	dsshm_t		*dsp;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	held;
	vshm_src_prep_t	src_info;	
	int		error;
	service_t	svc;
	obj_mft_info_t	minfo;

	obj_mft_info_get(mftp, &minfo);

	error = vshm_obj_mobilize(vsp);
	if (error)
		return error;
	bdp = VSHM_TO_FIRST_BHV(vsp);
	dsp = BHV_TO_DSSHM(bdp);

	/*
	 * Here with local, distributed object marked for OUTBOUND relocation.
	 *
	 * The target is provided with an existence token for the
	 * object if necessary since the object may not exist there.
	 */
	tks_obtain(dsp->dssm_tserver, minfo.target_cell,
		   VSHM_EXISTENCE_TOKENSET,
		   &granted, &refused, &held);
	ASSERT((granted == TK_NULLSET && held == VSHM_EXISTENCE_TOKENSET) ||
	       (granted == VSHM_EXISTENCE_TOKENSET && held == TK_NULLSET));

	/*
	 * Construct the prepare info and put it into the manifest.
	 */
	SERVICE_MAKE(svc, cellid(), SVC_SVIPC);
	HANDLE_MAKE(minfo.source.hndl, svc, BHV_TO_OBJID(bdp));

	src_info.id  = vsp->vsm_id;
	src_info.key = vsp->vsm_key;
	src_info.granted = granted;
	minfo.source.tag = OBJ_TAG_SRC_PREP;
	minfo.source.infop = &src_info;
	minfo.source.info_size = sizeof(src_info);
	obj_mft_info_put(mftp, &minfo);

	VSHM_TRACE4("vshm_obj_source_prepare", src_info.id, "cell", cellid());
	return 0;
}

int
vshm_obj_target_prepare(
	obj_manifest_t	*mftp,			/* IN object manifest */
	void		**v)			/* OUT virtual object */
{
	vshm_t		*vsp;
	bhv_head_t	*bhp;
	bhv_desc_t	*bdp;
	vshm_src_prep_t	src_info;
	service_t	svc;
	obj_mft_info_t	minfo;

	/*
	 * Extract object info from the manifest.
	 */
	minfo.source.tag = OBJ_TAG_SRC_PREP;
	minfo.source.infop = &src_info;
	minfo.source.info_size = sizeof(src_info);
	obj_mft_info_get(mftp, &minfo);

	/*
	 * Lookup the object.
	 * If the incoming bag includes an existence token,
	 * then the object can't exist here and we must create it.
	 * Otherwise, it already exists or it is being
	 * created by another thread and will exist soon.
	 */
	if (src_info.granted == TK_NULLSET) {
		dcshm_t		*dcp;

		/*
		 * Object is or will be found locally.
		 */
		int	retries = 0;
		while (vshm_lookup_id_local(src_info.id, &vsp) == ENOENT)
			cell_backoff(++retries);
		vshm_client_quiesce(vsp);

		/*
		 * Remove and destroy the dc.
		 * Vshm doesn't use tokens other than the existence
		 * token - if it did, these would be returned here.
		 */
		bhp = VSHM_BHV_HEAD_PTR(vsp);
		bdp = VSHM_TO_FIRST_BHV(vsp);
		dcp = BHV_TO_DCSHM(bdp);
		bhv_remove(bhp, bdp);
		tkc_free(dcp->dcsm_tclient);
		kern_free(dcp);

	} else {
		/*
		 * Allocate the virtual object for the shared memory segment
		 * for this cell
		 */
		vsp = (vshm_t *) kmem_zone_alloc(vshm_zone, KM_SLEEP);
		ADDVSHM();
		vsp->vsm_id = src_info.id;
		vsp->vsm_key = src_info.key;
		vsp->vsm_refcnt = 1;

		bhp = VSHM_BHV_HEAD_PTR(vsp);
		bhv_head_init(bhp, "vshm");

		/* Write-lock the chain so that no operation can commence. */
		BHV_WRITE_LOCK(bhp);

		/* Add id into hash table */
		vshm_enter(vsp, B_TRUE);
	}

	/* Add ds behavior */
	ASSERT(BHV_IS_WRITE_LOCKED(bhp));
	vshm_interpose(vsp);
	bdp = VSHM_TO_FIRST_BHV(vsp);
	obj_SR_target_prepared(&BHV_TO_DSSHM(bdp)->dssm_obj_state);

	/*
	 * Return the hanlde to the embrionic ds and
	 * we leave the behavior chain write-lock.
	 * Returned token info would also be loaded into the reply bag -
	 * though vshm has none..
	 */
	SERVICE_MAKE(svc, cellid(), SVC_SVIPC);
	HANDLE_MAKE(minfo.target.hndl, svc, BHV_TO_OBJID(bdp));
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_put(mftp, &minfo);

	*v = (void *) vsp;
	vshm_rele(vsp);

	VSHM_TRACE6("vshm_obj_target_prepare", src_info.id, "cell", cellid(),
		    "embrionic_objid", BHV_TO_OBJID(bdp));
	return 0;
}

int
vshm_obj_source_retarget(
	obj_manifest_t	*mftp,			/* IN object manifest */
	tks_state_t	**tks_statep)		/* OUT token state */
{
	bhv_desc_t	*bdp;
	vshm_t		*vsp;
	dsshm_t		*dsp;
	bhv_head_t	*bhp;
	dcshm_t		*dcp;
	obj_mft_info_t	minfo;

	/*
	 * Get source and target info from manifest.
	 */
	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.source.hndl));
	vsp = BHV_TO_VSHM(bdp);
	dsp = BHV_TO_DSSHM(bdp);
	bhp = VSHM_BHV_HEAD_PTR(vsp);

	/*
	 * Need the write-lock now. But there should be no need
	 * to check that the chain is as we left it.
	 * At this point, new clients seeking an existence token
	 * will be blocked until a dcshm is installed and this will
	 * cause them to be redirected to the new cell.
	 */
	vshm_client_quiesce(vsp);

	/*
	 * If the object is being removed we'll forget the relocation.
	 */
	if (vshm_is_removed(vsp)) {
		obj_SR_source_abort(&dsp->dssm_obj_state);
		vshm_client_unquiesce(vsp);
		VSHM_TRACE4("vshm_obj_source_retarget abort", vsp->vsm_id,
			    "cell", cellid());
		return ESRCH;
	}

	/*
	 * Unlink the dsshm/pshm combination from the behavior head and 
	 * create and install the embrionic dc.
	 * The new remote-client token state comprises only the
	 * EXISTENCE token since any others were recalled before bagging.
	 * Note that the target handle is saved at this point.
	 */
	dcp = (dcshm_t *) kern_malloc(sizeof(dcshm_t));
	ASSERT(dcp);
	tkc_create("dshm", dcp->dcsm_tclient, dcp, &dcshm_tclient_iface,
		   VSHM_NTOKENS,
		   VSHM_EXISTENCE_TOKENSET, VSHM_EXISTENCE_TOKENSET,
		   (void *)(__psint_t)vsp->vsm_id);
	dcp->dcsm_handle = minfo.target.hndl;
	bhp->bh_first = NULL;					/* rude */
	bhv_desc_init(&dcp->dcsm_bd, dcp, vsp, &dcshm_ops);
	bhv_insert_initial(&vsp->vsm_bh, &dcp->dcsm_bd);

	*tks_statep = dsp->dssm_tserver;

	VSHM_TRACE6("vshm_obj_source_retarget", vsp->vsm_id, "cell", cellid(),
		    "new handle", HANDLE_TO_OBJID(dcp->dcsm_handle));
	return 0;
}

/*
 * This routine is called from the source server cell to redirect
 * a client to the target server cell.
 */
int
vshm_obj_client_retarget(
	obj_manifest_t	*mftp)			/* IN object manifest */
{
	vshm_t		*vsp;
	bhv_desc_t	*bdp;
	dcshm_t		*dcp;
	vshm_src_prep_t	src_info;
	obj_mft_info_t	minfo;
	/* REFERENCED */
	cell_t		old_server;
	/* REFERENCED */
	cell_t		new_server;

	/*
	 * Extract object info from the manifest.
	 */
	minfo.target.tag = OBJ_TAG_NONE;
	minfo.source.tag = OBJ_TAG_SRC_PREP;
	minfo.source.infop = &src_info;
	minfo.source.info_size = sizeof(src_info);
	obj_mft_info_get(mftp, &minfo);

	/*
	 * Lookup the object by id. There's a race possible if
	 * we're releasing a recently created object which has
	 * not yet been registered.
	 */
	if (vshm_lookup_id_local(src_info.id, &vsp) != 0) {
		return ENOENT;
	}
	bdp = VSHM_TO_FIRST_BHV(vsp);
	ASSERT(BHV_OPS(bdp) == &dcshm_ops);
	dcp = BHV_TO_DCSHM(bdp);

	/*
	 * Quiesce the local client.
	 */
	vshm_client_quiesce(vsp);

	/*
	 * Update the object handle to point to the new server (and objid).
	 */
	old_server = SERVICE_TO_CELL(HANDLE_TO_SERVICE(dcp->dcsm_handle));
	dcp->dcsm_handle = minfo.target.hndl;
	new_server = SERVICE_TO_CELL(HANDLE_TO_SERVICE(dcp->dcsm_handle));

	/*
	 * Unquiesce - allow OPs to go the new server.
	 */
	vshm_client_unquiesce(vsp);

	vshm_rele(vsp);

	VSHM_TRACE8("vshm_obj_client_retarget", vsp->vsm_id,
		    "retargetted from", old_server, "to", new_server,
		    "new handle", HANDLE_TO_OBJID(dcp->dcsm_handle));
	return 0;
}


int
vshm_obj_source_bag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	source_bag)		/* IN object state */
{
	bhv_desc_t	*bdp;
	vshm_t		*vsp;
	dsshm_t		*dsp;
	pshm_t		*psp;
	obj_mft_info_t	minfo;

	/*
	 * Get source and discard target info from manifest.
	 */
	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.source.hndl));
	vsp = BHV_TO_VSHM(bdp);
	dsp = BHV_TO_DSSHM(bdp);
	psp = BHV_TO_PSHM(BHV_NEXT(bdp));

	/*
	 * Handle premature eradication (removal) during retargetting.
	 * Although a remove during preparation is coped with by sending
	 * a null bag to the target (causing it to destroy its embrionic
	 * ds), we can't prevent a remote op removing the shm while
	 * retargetting. So, now as a dc we reverse our demise
	 * until contacted by the new server (to recall our existence
	 * token).
	 */
	if (vshm_is_removed(vsp))
		vshm_enter(vsp, B_TRUE); /* XXX - tiny chance id is reused */

	/*
	 * This cell has been been converted into a client and
	 * targetted at the target cell. So we can unlock our
	 * chain at this point and let new ops go remote.
	 */
	vshm_client_unquiesce(vsp);

	/*
	 * Bag the object state.
	 *
	 * Collect the token state. In R1, this does nothing
	 * expect mark the start of migration to disallow further
	 * grants.
	 * If the local client had any tokens, these would be recalled
	 * at this point so that the server's state is complete.
	 */
	tks_bag(dsp->dssm_tserver, source_bag);

	/*
	 * For R1, we simply pass a reference to the object and
	 * let the new server cell have at it.
	 */
	pshm_bag(psp, source_bag);

	VSHM_TRACE4("vshm_obj_source_bag", vsp->vsm_id, "cell", cellid());
	return 0;
}

int
vshm_obj_target_unbag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	bag)
{
	bhv_desc_t	*bdp;
	vshm_t		*vsp;
	dsshm_t		*dsp;
	/* REFERENCED */
	bhv_head_t	*bhp;
	int		error;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.target.hndl));
	vsp = BHV_TO_VSHM(bdp);
	dsp = BHV_TO_DSSHM(bdp);
	bhp = VSHM_BHV_HEAD_PTR(vsp);

	ASSERT(BHV_IS_WRITE_LOCKED(bhp));
	ASSERT(obj_SR_target_is_inbound(&dsp->dssm_obj_state));

	/*
	 * Transfer the server token state from bag.
	 */
	error = tks_unbag(dsp->dssm_tserver, bag);
	if (error)
		return error;

	/*
	 * Create local pshm and copy in the "remote" contents.
	 * The physical behavior will be inserted into the
	 * chain and will be made visible to retargetted ops.
	 * Note that the pshm may have been removed and is doomed here
	 * - hence the extra reference below to defer its death.
	 */
	vshm_ref(vsp);			/* ensure vshm persists ... */
	pshm_unbag(vsp, bag);
	vshm_client_unquiesce(vsp);
	obj_SR_target_end(&dsp->dssm_obj_state);
	vshm_rele(vsp);			/* ... at least until now */
	VSHM_TRACE4("vshm_obj_target_unbag", vsp->vsm_id, "cell", cellid());

	return 0;
}

int
vshm_obj_source_end(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	bhv_desc_t	*bdp;
	dsshm_t		*dsp;
	pshm_t		*psp;
	/* REFERENCED */
	vshm_t		*vsp;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.source.hndl));
	dsp = BHV_TO_DSSHM(bdp);
	psp = BHV_TO_PSHM(BHV_NEXT(bdp));
	vsp = BHV_TO_VSHM(bdp);

	/*
	 * Teardown.
	 *
	 * Everything is set up waiting. All we need do is tear down
	 * the old dsshm and pshm after marking the end of token
	 * migration and destroying the token server state.
	 */
	obj_SR_source_end(&dsp->dssm_obj_state);
	OBJ_STATE_DESTROY(&dsp->dssm_obj_state);
	tks_free(dsp->dssm_tserver);
	tkc_free(dsp->dssm_tclient);
	kern_free(dsp);
	pshm_free(psp);

	VSHM_TRACE6("vshm_obj_source_end", vsp->vsm_id,
		    "from", cellid(), "to", minfo.target_cell);
	return 0;
}

int
vshm_obj_source_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	bhv_desc_t	*bdp;
	/* REFERENCED */
	vshm_t		*vsp;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.source.hndl));
	vsp = BHV_TO_VSHM(bdp);

	/* Nothing to do. */

	VSHM_TRACE2("vshm_obj_source_abort", vsp->vsm_id);
	return 0;
}

int
vshm_obj_target_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	bhv_desc_t	*bdp;
	vshm_t		*vsp;
	dsshm_t		*dsp;
	bhv_head_t	*bhp;
	dcshm_t		*dcp;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.target.hndl));
	vsp = BHV_TO_VSHM(bdp);
	dsp = BHV_TO_DSSHM(bdp);
	bhp = VSHM_BHV_HEAD_PTR(vsp);

	ASSERT(BHV_IS_WRITE_LOCKED(bhp));
	ASSERT(obj_SR_target_is_inbound(&dsp->dssm_obj_state));

	/* Out with the new */
	bhv_remove(bhp, bdp);
	tks_free(dsp->dssm_tserver);
	tkc_free(dsp->dssm_tclient);

	/* And in with the old */
	dcp = (dcshm_t *) kern_malloc(sizeof(dcshm_t));
	ASSERT(dcp);
	tkc_create("dshm", dcp->dcsm_tclient, dcp, &dcshm_tclient_iface,
		   VSHM_NTOKENS, VSHM_EXISTENCE_TOKENSET,
		   VSHM_EXISTENCE_TOKENSET,
		   (void *)(__psint_t)vsp->vsm_id);
	dcp->dcsm_handle = minfo.source.hndl;
	bhv_desc_init(&dcp->dcsm_bd, dcp, vsp, &dcshm_ops);
	bhv_insert_initial(&vsp->vsm_bh, &dcp->dcsm_bd);

	vshm_client_unquiesce(vsp);

	VSHM_TRACE2("vshm_obj_target_abort", vsp->vsm_id);
	return 0;
}

obj_relocation_if_t vshm_obj_iface = {
	vshm_obj_source_prepare,
	vshm_obj_target_prepare,
	vshm_obj_source_retarget,
	vshm_obj_client_retarget,
	vshm_obj_source_bag,
	vshm_obj_target_unbag,
	vshm_obj_source_end,
	vshm_obj_source_abort,
	vshm_obj_target_abort
};

int
vshm_svr_lookup(
	void		*id,
	service_t	*svc)
{
	vshm_t		*vsp;
	bhv_desc_t	*bdp;
	bhv_head_t	*bhp;
	dcshm_t		*dcp;
	__psint_t	shmid = (__psint_t) id;
	int		error;

	error = vshm_lookup_id((int) shmid, &vsp);
	if (error)
		return error;

	bhp = VSHM_BHV_HEAD_PTR(vsp);
	BHV_READ_LOCK(bhp);
	bdp = VSHM_TO_FIRST_BHV(vsp);
	switch (BHV_POSITION(bdp)) {
	case VSHM_POSITION_PSHM:
	case VSHM_POSITION_DS:
		/*
		 * Here be the server.
		 */
		SERVICE_MAKE(*svc, cellid(), SVC_SVIPC);
		break;
	case VSHM_POSITION_DC:
		dcp = BHV_TO_DCSHM(bdp);
		*svc = HANDLE_TO_SERVICE(dcp->dcsm_handle);
		break;
	}
	
	BHV_READ_UNLOCK(bhp);
	vshm_rele(vsp);
	return 0;
}

int
vshm_svr_evict(
	void		*id,
	cell_t		target)
{
	vshm_t		*vsp;
	int		error;
	__psint_t	shmid = (__psint_t) id;

	if (shmid != 0) {
		error = vshm_lookup_id((int) shmid, &vsp);
		if (error)
			return error;
		error = vshm_obj_relocate(vsp, target);
		vshm_rele(vsp);
	} else {
		error = vshm_obj_evict(target);
	}
	return error;
}

obj_service_if_t vshm_obj_svr_iface = {
	vshm_svr_lookup,
	vshm_svr_evict,
	NULL,
};

void
vshm_obj_init(void)
{
	service_t	svc;

	/* 
	 * Register our callout vector with the central object management.
	 */
	SERVICE_MAKE(svc, cellid(), SVC_SVIPC);
	obj_service_register(svc, &vshm_obj_svr_iface, &vshm_obj_iface); 
}
