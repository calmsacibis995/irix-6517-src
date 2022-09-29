/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Id: cfs_relocation.c,v 1.13 1997/08/22 18:30:55 mostek Exp $"

#include <ksys/cell/tkm.h>
#include "cfs_relocation.h"
#include "dvn.h"
#include "dvfs.h"
#include <fs/cell/cxfs_intfc.h>

#include <fs/specfs/spec_lsnode.h>


/*
 *	
 * CFS-KORE interface for vnode relocation.
 *
 * Since vnode server objects may have various behaviors
 * interposed over a range of different physical filesystem
 * behaviors, CFS introduces its own callout layer nested
 * within the standard KORE callouts.
 */

/*
 * Table of callouts to perform behavior-specific bagging
 * and unbagging etc.
 */
static cfs_vn_kore_if_t	*bhv_callout_table[VN_BHV_END]; 


/*
 * The following callouts are for CFS behaviors.
 */

/*
 * Create embryonic server for vnode.
 * Note: this is not a callout routine but one called directly.
 */
void
cfs_vn_target_prepare(
	vnode_t		*vp,		/* IN vnode */
	cfs_handle_t	*handlep)	/* OUT embryonic object handle */
{
	bhv_desc_t	*bdp;
	dcvn_t		*dcp;
	dsvn_t		*dsp;

	/*
	 * Here on the target cell, we must have a CFS DC behavior
	 * as the first (and only) behavior. This must be removed
	 * from the lookup table but not destroyed yet.
	 */
	bdp = VNODE_TO_FIRST_BHV(vp);
	dcp = BHV_TO_DCVN(bdp);
	dcvn_handle_remove(dcp);

	/*
	 * Create embryonic ds behavior. This is an empty
	 * shell for now, but the handle is required for the
	 * source cell to be able to retarget clients.
	 * After this we'll have a v/ds/dc chain.
	 */
	dsp = dsvn_alloc(vp);
	(void) vn_bhv_insert(VN_BHV_HEAD(vp), DSVN_TO_BHV(dsp));
	dsvn_handle_enter(dsp);

	/*
	 * Return info with the new server handle
	 * (XXX and later returning tokens).
	 */
	DSVN_HANDLE_MAKE(*handlep, dsp);
}

void
cfs_vn_source_retarget(
	vnode_t		*vp,		/* IN vnode */
	cfs_handle_t	src_handle,	/* IN source handle */
	cfs_handle_t	tgt_handle,	/* IN embryo's handle */
	tks_state_t	**tkstatepp)	/* OUT vnode token server state */
{
	bhv_desc_t	*bdp;
	dsvn_t		*dsp;
	dcvn_t		*dcp;
	dcvfs_t		*vfs_dcp;

	VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));

	/*
	 * The server vnode is quiesced. We can now install
	 * a dcvn pointing at the target cell.
	 * First, get the server token state for kore.
	 * If new existence tokens are hereafter granted for this
	 * vnode by this cell, the new clients must be given the
	 * new handle.
	 */
	bdp = CFS_HANDLE_TO_OBJID(src_handle);
	ASSERT(BHV_TO_VNODE(bdp) == vp); 
	dsp = BHV_TO_DSVN(bdp);
	*tkstatepp = dsp->dsv_tserver;

	/*
	 * Slice the current behavior chain from the head,
	 * and install a dcvn.
	 */
	VN_BHV_HEAD(vp)->bh_first = NULL;
	dcp = kmem_zone_alloc(dcvn_zone, KM_SLEEP);
	spinlock_init(&dcp->dcv_lock, "dcv_lock");
	dcp->dcv_handle = tgt_handle;
	dcp->dcv_error = 0;
	bhv_desc_init(DCVN_TO_BHV(dcp), dcp, vp, &cfs_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), DCVN_TO_BHV(dcp));

	/* Install dcvn in dcvfs */
	vfs_dcp = dcvfs_vfs_to_dcvfs(vp->v_vfsp);
	ASSERT(vfs_dcp);
	if ((vp->v_type == VREG) && DC_FS_IS_XFS(vfs_dcp)) {
		dcp->dcv_dcxvn = cxfs_dcxvn_make((void *)dcp,
				vfs_dcp->dcvfs_xvfs,
				&dcp->dcv_vattr, dcp->dcv_tclient,
				/*JIMJIM get the cxfs
				stuff into the bag of bits.
				cxfs_flags, cxfs_buffer, cxfs_count); */
				0, 0, 0);
	} else
		dcp->dcv_dcxvn = NULL;

	dcvn_newdc_install(dcp, TK_NULLSET, vfs_dcp);

        /*
	 * Establish the local client token state in the new dcvn.
	 * But before this we must obtain (cached) the existence token since
	 * the local client didn't have it but a remote client does.
	 */
	tkc_acquire1(dsp->dsv_tclient, DVN_EXIST_READ_1);
	tkc_copy(dsp->dsv_tclient, dcp->dcv_tclient);
	tkc_release1(dcp->dcv_tclient, DVN_EXIST_READ_1);

	/* Remove the dsvn from hash table */
	dsvn_handle_remove(dsp);

	VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));

	/*
	 * If the vnode is a special device give
	 * specfs a chance to insert its behavior.
	 */
#if CELL_IRIX
	if (vp->v_type == VBLK || vp->v_type == VCHR) {
		vnode_t	*newvp;

		newvp = spec_vnimport(vp, get_current_cred(), 0);

#ifdef JTK_DEBUG
		printf(
	  "cfs_vn_source_retarget[%d]: newvp/0x%x = vp/0x%x, rdev(%d/%d, %d)\n",
							cellid(),
							newvp, vp,
							major(vp->v_rdev),
							emajor(vp->v_rdev),
							minor(vp->v_rdev));
#endif	/* JTK_DEBUG */

		ASSERT(newvp);
		ASSERT(newvp == vp);

		VN_RELE(vp);		/* Release xtra ref taken by spec */
	}
#endif /* CELL_IRIX */
}

void
cfs_vn_client_retarget(
	vnode_t		*vp,		/* IN vnode */
	dcvn_t		*dcp,		/* IN dcvn */
	cfs_handle_t	tgt_handle)	/* IN new handle */
{
	/*
	 * Take the current handle out of the lookup table,	
	 * update it and re-register.
	 * Get write lock on chain before updating.
	 */
	VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));
	dcvn_handle_remove(dcp);
	dcp->dcv_handle = tgt_handle;
	dcvn_handle_enter(dcp);
	VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));
}

/*
 * Bag given dsvn behavior.
 */
int
cfs_bhv_source_bag(
	bhv_desc_t	*bdp,		/* IN bhv descriptor */
	obj_bag_t	bag)		/* IN object bag */
{
	dsvn_t		*dsp;
	/*REFERENCED*/
	int		error;
	dsvn_bag_t	dsvn_bag;

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_bhv_source_bag", BHV_TO_VNODE(bdp),
		"bhv", bdp);

	ASSERT(BHV_IDENTITY(bdp)->bi_id == VN_BHV_CFS);
	ASSERT(BHV_IDENTITY(bdp)->bi_position == VNODE_POSITION_CFS_DS);
	dsp = BHV_TO_DSVN(bdp);

	/*
	 * Construct data item containing dsvn fields and plac in bag.
	 */
	dsvn_bag.flags		= dsp->dsv_flags;
	dsvn_bag.vrgen_flags	= dsp->dsv_vrgen_flags;
	error = obj_bag_put(bag, OBJ_TAG_CFS_DSVN,
			    &dsvn_bag, sizeof(dsvn_bag_t));
	ASSERT(!error);

	/* Get the token server state bagged also. */
	tks_bag(dsp->dsv_tserver, bag);

	return 0;
}

/*
 * Unbag and recreate behavior for given vnode from given bag.
 */
int
cfs_bhv_target_unbag(
	vnode_t		*vp,		/* IN vnode */
	obj_bag_t	bag)		/* IN object bag */
{
	bhv_desc_t	*bdp;
	dsvn_t		*dsp;
	dcvn_t		*dcp;
	int		error;
	int		cell;
	dsvn_bag_t	dsvn_bag;

	/*
	 * We need to fully instantiate the embryonic dsvn already
	 * in place. First, let's find it - it should be first - and
	 * verify it. The second behavior is the left-over dcvn which
	 * still contains the local client state.
	 */
	ASSERT(VN_BHV_IS_WRITE_LOCKED(VN_BHV_HEAD(vp)));
	bdp = VNODE_TO_FIRST_BHV(vp);
	ASSERT(BHV_IDENTITY(bdp)->bi_id == VN_BHV_CFS);
	ASSERT(BHV_IDENTITY(bdp)->bi_position == VNODE_POSITION_CFS_DS);
	dsp = BHV_TO_DSVN(bdp);
	bdp = BHV_NEXT(bdp);
	dcp = BHV_TO_DCVN(bdp);

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_bhv_target_unbag", vp, "bhv", bdp);

	/*
	 * Extract the expected data item from the bag.
	 */
	obj_bag_get_here(bag, OBJ_TAG_CFS_DSVN,
			 &dsvn_bag, sizeof(dsvn_bag_t), error);
	ASSERT(!error);
	dsp->dsv_flags		= dsvn_bag.flags;
	dsp->dsv_vrgen_flags	= dsvn_bag.vrgen_flags;

	/*
	 * Get token server state from the bag 
	 * and copy local client state from the dcvn.
	 * At this point, we drop the cached existence token for the local
	 * client.
	 */
	tks_unbag(dsp->dsv_tserver, bag);
	tkc_copy(dcp->dcv_tclient, dsp->dsv_tclient);
	tkc_recall(dsp->dsv_tclient, DVN_EXIST_READ, TK_DISP_CLIENT_ALL);

	/*
	 * Take an additional vnode reference for each remote client.
	 */
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (cell == cellid())
			continue;
		if (tks_isclient(dsp->dsv_tserver, cell))
			VN_HOLD(vp);
	}
	
	/*
	 * Destroy the client behavior.
	 */
	dcvfs_dcvn_delete(dcp, dcp->dcv_dcvfs);
        tkc_free(dcp->dcv_tclient);
	tkc_destroy(dcp->dcv_tclient);
	vn_bhv_remove(VN_BHV_HEAD(DCVN_TO_VNODE(dcp)), DCVN_TO_BHV(dcp));
	spinlock_destroy(&dcp->dcv_lock);
	if (dcp->dcv_dcxvn)
		cxfs_dcxvn_destroy(dcp->dcv_dcxvn);
	kmem_zone_free(dcvn_zone, dcp);

	return error;
}

/*
 * Relocation complete for given behavior - destroy it.
 */
int
cfs_bhv_source_end(
	bhv_desc_t	*bdp)		/* IN bhv descriptor */
{
	dsvn_t	*dsp = BHV_TO_DSVN(bdp);
	int	cell;

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_bhv_source_end", BHV_TO_VNODE(bdp),
		"bhv", bdp);

	ASSERT(BHV_IDENTITY(bdp)->bi_id == VN_BHV_CFS);
	ASSERT(BHV_IDENTITY(bdp)->bi_position == VNODE_POSITION_CFS_DS);

	/*
	 * For each remote client, the vnode had an addition reference
	 * count - these extra references need removing.
	 */
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (cell == cellid())
			continue;
		if (tks_isclient(dsp->dsv_tserver, cell))
			VN_RELE(BHV_TO_VNODE(bdp));
	}

	/*
	 * Toss the dsvn. Note that the frees clears token state
	 * and destroys.
	 */
	tks_free(dsp->dsv_tserver);
	tkc_free(dsp->dsv_tclient);
	mutex_destroy(&dsp->dsv_mutex);
	if (dsp->dsv_dsxvn)
		cxfs_dsxvn_destroy(dsp->dsv_dsxvn);
	kmem_zone_free(dsvn_zone, dsp);

	return 0;
}

/*
 * Relocation of given behavior failed - fix it up.
 */
int
cfs_bhv_source_abort(
	bhv_desc_t	*bdp)		/* IN bhv descriptor */
{
	panic("cfs_bhv_source_abort: bdp 0x%x", bdp);
	/* NOTREACHED */
}

/*
 * Relocation of given behavior failed - destroy it.
 */
int
cfs_bhv_target_abort(
	bhv_desc_t	*bdp)		/* IN bhv descriptor */
{
	panic("cfs_bhv_target_abort: bdp 0x%x", bdp);
	/* NOTREACHED */
}

void
cfs_vn_bhv_register(
	cfs_bhv_t 		bhv_id,
	cfs_vn_kore_if_t	*table)
{
	ASSERT(bhv_id < VN_BHV_END);
	bhv_callout_table[bhv_id] = table;
}

int
cfs_obj_source_prepare(
	obj_manifest_t	*mftp,		/* IN/OUT object manifest */
	void		*v)		/* IN vnode */
{
	service_t	svc;
	vnode_t		*vp = (vnode_t *)v;
        cfs_handle_t	handle;
	obj_mft_info_t	minfo;

	obj_mft_info_get(mftp, &minfo);		/* gets only rmode */

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_obj_source_prepare", vp,
		    "rmode", minfo.rmode);

	if (vp) {
		cfs_vnexport(vp, &handle);
	} else {
		CFS_HANDLE_TO_OBJID(handle) = NULL;
	}

	/*
	 * Note that source prepare is the same for
	 * both reference and server relocation. In both cases,
	 * we export a reference.
	 */

	SERVICE_MAKE(svc, cellid(), SVC_CFS);
	HANDLE_MAKE(minfo.source.hndl, svc, vp);
	minfo.source.tag = OBJ_TAG_CFS_HANDLE;
	minfo.source.infop = &handle;
	minfo.source.info_size = sizeof(cfs_handle_t);

	obj_mft_info_put(mftp, &minfo);

	return 0;
}

int
cfs_obj_target_prepare(
	obj_manifest_t	*mftp,			/* IN object manifest */
	void		**v)			/* OUT virtual object */
{
	vnode_t		*vp;
	service_t	svc;
        cfs_handle_t	handle;
	obj_mft_info_t	minfo;
	/*REFERENCED*/
	int		error;

	minfo.source.tag = OBJ_TAG_CFS_HANDLE;
	minfo.source.infop = &handle;
	minfo.source.info_size = sizeof(cfs_handle_t);

	obj_mft_info_get(mftp, &minfo);

	if (CFS_HANDLE_TO_OBJID(handle) == NULL) {
		vp = NULL;
	} else {
		cfs_vnimport(&handle, &vp);
		ASSERT(vp);
	}
	*v = vp;

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_obj_target_prepare", vp,
		    "rmode", minfo.rmode);

	if (minfo.rmode & OBJ_RMODE_SERVER) {

		/*
		 * Grab the vnode firmly by the behavior head.
		 * This remains locked until it's fully instantiated at the
		 * end of the unbagging step.
		 */
		VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));

		/*
		 * Prepare the embryo and return its CFS handle.
		 */
		cfs_vn_target_prepare(vp, &handle);

		minfo.target.infop = &handle;
		minfo.target.tag = OBJ_TAG_CFS_HANDLE;
		minfo.target.info_size = sizeof(cfs_handle_t);

	} else {
		minfo.target.tag = OBJ_TAG_NONE;
	}
	SERVICE_MAKE(svc, cellid(), SVC_CFS);
	HANDLE_MAKE(minfo.target.hndl, svc, vp);
	obj_mft_info_put(mftp, &minfo);

	return 0;
}

int
cfs_obj_source_retarget(
	obj_manifest_t	*mftp,		/* IN object manifest */
	tks_state_t	**tkstatepp)	/* OUT vnode token server state */
{
	obj_mft_info_t	minfo;
	cfs_handle_t	src_handle;
	cfs_handle_t	tgt_handle;
	vnode_t		*vp;

	minfo.source.tag = OBJ_TAG_CFS_HANDLE;
	minfo.source.infop = &src_handle;
	minfo.source.info_size = sizeof(cfs_handle_t);
	minfo.target.tag = OBJ_TAG_CFS_HANDLE;
	minfo.target.infop = &tgt_handle;
	minfo.target.info_size = sizeof(cfs_handle_t);
	obj_mft_info_get(mftp, &minfo);

	vp = (vnode_t *) HANDLE_TO_OBJID(minfo.source.hndl);

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_obj_source_retarget", vp,
		    "rmode", minfo.rmode);

	if (minfo.rmode & OBJ_RMODE_SERVER) {
		/*
		 * We're moving the server, get to the dsvn
		 * and return the token state to kore.
		 * Note: only the existence token is used
		 * to visit each client cell and retarget dcvns.
		 * Then install a dcvn for redirection to the target cell.
		 */
		cfs_vn_source_retarget(vp, src_handle, tgt_handle, tkstatepp);
	} else {
		*tkstatepp = NULL;
	}
	return 0;
}

int
cfs_obj_client_retarget(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;
	cfs_handle_t	src_handle;
	cfs_handle_t	tgt_handle;

	minfo.source.tag = OBJ_TAG_CFS_HANDLE;
	minfo.source.infop = &src_handle;
	minfo.source.info_size = sizeof(cfs_handle_t);
	minfo.target.tag = OBJ_TAG_CFS_HANDLE;
	minfo.target.infop = &tgt_handle;
	minfo.target.info_size = sizeof(cfs_handle_t);
	obj_mft_info_get(mftp, &minfo);

	if (minfo.rmode & OBJ_RMODE_SERVER) {
		vnode_t		*vp;
		dcvn_t		*dcp;

		/*
		 * If moving the server, lookup the dcvn,
		 * quiesce and retarget.
		 */
		dcp = dcvn_handle_lookup(&src_handle);
		vp = DCVN_TO_VNODE(dcp);
		DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_obj_client_retarget", vp,
			    "dcvn", dcp);
		if (dcp == NULL) {
			/*
			 * Must have raced with set-up or tear-down
			 * so we'll have to recheck.
			 */
			return ESRCH;
		}

		cfs_vn_client_retarget(vp, dcp, tgt_handle);
	}
	return 0;
}

int
cfs_obj_source_bag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	bag)			/* IN/OUT object state */
{
	obj_mft_info_t	minfo;
        cfs_handle_t	src_handle;
        cfs_handle_t	tgt_handle;

	minfo.source.tag = OBJ_TAG_CFS_HANDLE;
	minfo.source.infop = &src_handle;
	minfo.source.info_size = sizeof(cfs_handle_t);
	minfo.target.tag = OBJ_TAG_CFS_HANDLE;
	minfo.target.infop = &tgt_handle;
	minfo.target.info_size = sizeof(cfs_handle_t);
	obj_mft_info_get(mftp, &minfo);

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_obj_source_bag", 
		    (vnode_t *) HANDLE_TO_OBJID(minfo.source.hndl),
		    "rmode", minfo.rmode);

	if (minfo.rmode & OBJ_RMODE_SERVER) {
		bhv_desc_t	*bdp;
		/*REFERENCED*/
		int		error;

		/*
		 * Source handle is vnode pointer, info bag contains
		 * the cfs handle which includes the dsvn bhv address.
		 * Note that the all server behaviors have moved
		 * aside when the local vnode was retargetted. 
		 *
		 * Pass down the chain calling the appropriate
		 * bagging code.
		 */
		for (bdp = CFS_HANDLE_TO_OBJID(src_handle);
		     bdp != NULL;
		     bdp = bdp->bd_next) {
			cfs_vn_kore_if_t	*bhv_if;
			bhv_identity_t		*bhv_idp;

			bhv_idp = BHV_IDENTITY(bdp);
			error = obj_bag_put(bag, OBJ_TAG_CFS_BHV_ID,
					    (void *) bhv_idp,
					    sizeof(bhv_identity_t));
			ASSERT(!error);

			ASSERT(VN_BHV_UNKNOWN < bhv_idp->bi_id &&
					        bhv_idp->bi_id < VN_BHV_END);
			bhv_if = bhv_callout_table[bhv_idp->bi_id];
			ASSERT(bhv_if);

			error = (*(bhv_if)->cfs_vn_bhv_source_bag)(bdp, bag);
			ASSERT(!error);
		}

		/*
		 * Add a delimiting tag.
		 */
		error = obj_bag_put(bag, OBJ_TAG_CFS_BHV_END, NULL, 0);
		ASSERT(!error);

	}
	return 0;
}

/* ARGSUSED */
int
cfs_obj_target_unbag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	bag)			/* IN/OUT object state */
{
	obj_mft_info_t	minfo;
	vnode_t		*vp;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	vp = (vnode_t *)HANDLE_TO_OBJID(minfo.target.hndl);

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_obj_target_unbag", vp,
		    "rmode", minfo.rmode);

	if (minfo.rmode & OBJ_RMODE_SERVER) {
		obj_tag_t	bhv_id_tag = OBJ_TAG_CFS_BHV_ID;
		obj_bag_size_t	bhv_id_size = sizeof(bhv_identity_t);
		bhv_identity_t	bhv_id;
		bhv_identity_t	*bhv_idp = &bhv_id;
		/*REFERENCED*/
		int		error;
		
		while (obj_bag_get(bag, &bhv_id_tag, (void **) &bhv_idp,
				   &bhv_id_size) == OBJ_SUCCESS) {
			cfs_vn_kore_if_t	*bhv_if;

			ASSERT(VN_BHV_UNKNOWN < bhv_idp->bi_id &&
					        bhv_idp->bi_id < VN_BHV_END);
			bhv_if = bhv_callout_table[bhv_id.bi_id];
			ASSERT(bhv_if);

			error = (*(bhv_if)->cfs_vn_bhv_target_unbag)(vp, bag);
			ASSERT(!error);
		}

		/* Skip delimiter */
		error = obj_bag_skip(bag, OBJ_TAG_CFS_BHV_END);
		ASSERT(!error);

		/*
		 * Release the new vnode server.
		 */
		VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));
	}

	return 0;
}

int
cfs_obj_source_end(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;
        cfs_handle_t	src_handle;
	vnode_t		*vp;

	minfo.source.tag = OBJ_TAG_CFS_HANDLE;
	minfo.source.infop = &src_handle;
	minfo.source.info_size = sizeof(cfs_handle_t);
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	
	vp = (vnode_t *)HANDLE_TO_OBJID(minfo.source.hndl);
	DVN_KTRACE4(DVN_TRACE_RELOCATE, "cfs_obj_source_end", vp,
		    "rmode", minfo.rmode);
	if (vp) {
		/*
		 * release original ref
		 */
		VN_RELE(vp);
	}

	if (minfo.rmode & OBJ_RMODE_SERVER) {
		bhv_desc_t	*bdp;
		/*REFERENCED*/
		int		error;

		/*
		 * Source handle is vnode pointer, info bag contains
		 * the cfs handle which includes the dsvn bhv address.
		 * Note that the all server behaviors were moved
		 * aside when the local vnode was retargetted. 
		 *
		 * Pass down the chain calling the appropriate
		 * source_end code taking care to note that this
		 * will destroy each bhv.
		 */
		bdp = (bhv_desc_t *) CFS_HANDLE_TO_OBJID(src_handle);
		while (bdp != NULL) {
			cfs_vn_kore_if_t	*bhv_if;
			bhv_identity_t		*bhv_idp;
			bhv_desc_t		*this_bdp;

			bhv_idp = BHV_IDENTITY(bdp);
			ASSERT(bhv_idp->bi_id < VN_BHV_END);
			bhv_if = bhv_callout_table[bhv_idp->bi_id];
			ASSERT(bhv_if);

			/* step to possible next before destroying this one */
			this_bdp = bdp;
			bdp = bdp->bd_next;

			error = (*(bhv_if)->cfs_vn_bhv_source_end)(this_bdp);
			ASSERT(!error);
		}
		
	}

	return 0;
}

/* ARGSUSED */
int
cfs_obj_source_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;
        cfs_handle_t	src_handle;
        cfs_handle_t	tgt_handle;

	minfo.source.tag = OBJ_TAG_CFS_HANDLE;
	minfo.source.infop = &src_handle;
	minfo.source.info_size = sizeof(cfs_handle_t);
	minfo.target.tag = OBJ_TAG_CFS_HANDLE;
	minfo.target.infop = &tgt_handle;
	minfo.target.info_size = sizeof(cfs_handle_t);
	obj_mft_info_get(mftp, &minfo);

	if (minfo.rmode & OBJ_RMODE_SERVER)
		panic("cfs_obj_source_abort: server relocation not handled");

	return 0;
}

/* ARGSUSED */
int
cfs_obj_target_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	/*
	 * The vnode import took one extra reference, so ...
	 *
	 * Do nothing in this case .... any required VN_RELE 
	 * will be done by the object that holds the reference
	 * (otherwise all those reference holders need to know to
	 * skip their VN_RELE iff they are doing a target abort)
	 */
	return 0;
}

obj_relocation_if_t cfs_obj_iface = {
	cfs_obj_source_prepare,
	cfs_obj_target_prepare,
	cfs_obj_source_retarget,
	cfs_obj_client_retarget,
	cfs_obj_source_bag,
	cfs_obj_target_unbag,
	cfs_obj_source_end,
	cfs_obj_source_abort,
	cfs_obj_target_abort
};

cfs_vn_kore_if_t cfs_obj_bhv_iface = {
	cfs_bhv_source_bag,
	cfs_bhv_target_unbag,
	cfs_bhv_source_end,
	cfs_bhv_source_abort,
	cfs_bhv_target_abort
};

void
cfs_obj_init()
{
	/* Register our own CFS behavior handling */
	cfs_vn_bhv_register(VN_BHV_CFS, &cfs_obj_bhv_iface);

#ifdef	CELL_IRIX
	/* Register prnode behavior handling */
	cfs_vn_bhv_register(VN_BHV_PR, &prnode_obj_bhv_iface);
#endif

	obj_service_register(cfs_service_id, NULL, &cfs_obj_iface); 
}

