/**************************************************************************
 *									  *
 *		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"Revision: $"

#include "dv.h"
#include "dvn.h"
#include "dvfs.h"
#include <fs/cell/cfs_intfc.h>
#include <fs/cell/mount_import.h>
#include <sys/atomic_ops.h>
#include <sys/vfs.h>
#include <sys/pvfs.h>
#include "invk_dcvfs_stubs.h"


/************************************************************************
 * Routines applied to server side of DVFS				*
 ************************************************************************/

static void     dsvfs_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
static void     dsvfs_tsif_recalled(void *, tk_set_t, tk_set_t);

tks_ifstate_t dsvfs_tsif = {
                dsvfs_tsif_recall,
                dsvfs_tsif_recalled,
                NULL,                   /* tsif_idle */
};

/*
 * Token callout for recalling a token
 */
/* ARGSUSED */
static void
dsvfs_tsif_recall(
        void            *dsobj,
        tks_ch_t        client,
        tk_set_t        recset,
        tk_disp_t       why)
{
	service_t	service;
	dsvfs_t 	*dsp = (dsvfs_t *)dsobj;
	cfs_handle_t	vfs_hdl;
	int		error;
	/* REFERENCED */
	int		msgerr;

	/* sends recall message to client cell.
	 * Client cell will let us know the result of the recall via &error
	 */
	if (client == cellid()) {
		/* local client */
		tkc_recall(dsp->dsvfs_tclient, recset, why);
	} else {
#if CELL_IRIX
		SERVICE_MAKE(service, (cell_t)client, SVC_CFS);
		cfs_vfsexport(DSVFS_TO_VFS(dsp), &vfs_hdl);
		msgerr = invk_dcvfs_vfsrecall(service, &vfs_hdl, recset, why, 
				     dsp->dsvfs_unmounter, &error);
		ASSERT(!msgerr);
		if (error) {
			/* client refuses the revoke */
			ASSERT(error == EBUSY);
			tks_return(dsp->dsvfs_tserver, client, 
				TK_NULLSET, recset, TK_NULLSET, why); 
		}	
#else /* CELL_IRIX */
	        tks_return(dsp->dsvfs_tserver, client, 
			   TK_NULLSET, recset, TK_NULLSET, why); 
#endif /* CELL_IRIX */
	}
			
}

/*
 * Token callout upon completion of a recall.
 */
/* ARGSUSED */
static void
dsvfs_tsif_recalled(
        void            *dsobj,
        tk_set_t        recset,
        tk_set_t        failset)
{
	return;
}

/*
 * Given a vfs and rvp, fill in a mount_import structure for the given
 * client.  If the VFS_CELLROOT flag is set in the vfs struct, then
 * only fill in information for the vfs.
 *
 * In the case, where an existence token is taken for the root vnode, an
 * additional vnode ref. is taken on the token's behalf.
 *
 * Return 0 if successful, 1 otherwise.  Either way, the vfs_hdl in the
 * mount_import structure will be set.
 */
int
cfs_mountpoint_export(
	vfs_t		*vfsp,		/* vfs to be exported */
	vnode_t		*rvp,		/* rvp to be exported */
	cell_t		client,		/* export to this cell */
	mount_import_t	*mnti)		/* mount point data */
{
	dsvfs_t		*vfs_dsp;
	dsvn_t		*rvp_dsp;
	tk_set_t	vfs_refset, rvp_refset, hadset;
	int		error = 0;

	/*
	 * Get handle and tokens for the vfs.
	 * Note that even if can't get the tokens, we still need
	 * to return the handle as a hint.
	 */
	cfs_vfsexport(vfsp, &mnti->vfs_hdl);
	vfs_dsp = BHV_TO_DSVFS(vfsp->vfs_fbhv);
	tks_obtain(vfs_dsp->dsvfs_tserver, client, DVFS_EXIST_TRAVERSE_RD,
		&mnti->vfs_tkset, &vfs_refset, &hadset);

	if (mnti->vfs_tkset == TK_NULLSET) {
		error = 1;  	
		goto out;
	}
	mnti->vfs_flags = vfsp->vfs_flag & ~VFS_CELL_BITS;
	mnti->vfs_dev = vfsp->vfs_dev;
	mnti->vfs_bsize = vfsp->vfs_bsize;
	mnti->vfs_fsid = vfsp->vfs_fsid;
	mnti->vfs_altfsid = vfsp->vfs_altfsid;
	mnti->vfs_cell = vfsp->vfs_cell;
        mnti->vfs_fstype = vfsp->vfs_fstype;
	ASSERT(vfsp->vfs_eisize <= VFS_EILIMIT);
        mnti->vfs_eisize = vfsp->vfs_eisize;
	bcopy(vfsp->vfs_expinfo, mnti->vfs_expinfo,
	      vfsp->vfs_eisize <= VFS_EILIMIT ? vfsp->vfs_eisize : VFS_EILIMIT);

	/* 
	 * Obtain token for root vp.
	 */
	if (mnti->vfs_flags & VFS_CELLROOT) {
		mnti->rvp_tkset = TK_NULLSET;
	} else {
 	        rvp_dsp = dsvn_vnode_to_dsvn(rvp);
		DSVN_HANDLE_MAKE(mnti->rvp_hdl, rvp_dsp);
		tks_obtain(rvp_dsp->dsv_tserver, client, DVN_EXIST_READ,
                           &mnti->rvp_tkset, &rvp_refset, &hadset);

		if (mnti->rvp_tkset & DVN_EXIST_READ) {
			mnti->rvp_vtype = rvp->v_type;
			mnti->rvp_rdev = rvp->v_rdev;
			DSVN_CLIENT_FLAGS(rvp_dsp, rvp, mnti->rvp_flags);
		} else {
		        /* return the vfs tokens acquired earlier */
		        tks_return(vfs_dsp->dsvfs_tserver, client, 
				   mnti->vfs_tkset, TK_NULLSET, 
				   TK_NULLSET, TK_DISP_CLIENT_ALL);
			error = 1;
		}
	}

	/*
	 * If we got an existence token, get a corresponding vref.
	 */
	if (mnti->rvp_tkset != TK_NULLSET)
		VN_HOLD(rvp);

 out:
	return error;
}

/*
 * dsvfs_abort_unmount()
 * obtain token for client, send mesg to client to undo the prepare unmount
 * phase.
 */
/*ARGSUSED*/
tks_iter_t
dsvfs_abort_unmount(
	void		*dsobj,
	tks_ch_t	client,
	tk_set_t	tset_owned,
	va_list		args)
{
	service_t	service;
	dsvfs_t		*dsp = (dsvfs_t *)dsobj;
	cfs_handle_t	vfs_hdl, rvp_hdl;
	vfs_t		*vfsp;
	vnode_t		*rvp;
	/*REFERENCED*/
	int 		error;
	/* REFERENCED */
	int		msgerr;
	tk_set_t	tset_granted, tset_refused, tset_had;

	ASSERT(client != cellid());
	tks_obtain(dsp->dsvfs_tserver, client, DVFS_TRAVERSE_RD,
			&tset_granted, &tset_refused, &tset_had);
	SERVICE_MAKE(service, (cell_t)client, SVC_CFS);
	vfsp = DSVFS_TO_VFS(dsp);
	cfs_vfsexport(vfsp, &vfs_hdl);
	VFS_ROOT(vfsp, &rvp, error);
	cfs_vnexport(rvp, &rvp_hdl);
	VN_RELE(rvp);
	msgerr = invk_dcvfs_abort_unmount(service, &rvp_hdl, &vfs_hdl, 
				 dsp->dsvfs_unmounter);
	ASSERT(!msgerr);
	return TKS_CONTINUE;
}
			
/*
 * dsvfs_dounmount_subr
 */
int
dsvfs_dounmount_subr(
	bhv_desc_t	*bdp,
	int		flags,
	vnode_t         *rootvp,
	cred_t		*cr,
        cell_t          issuer)
{
	dsvfs_t 	*dsp = BHV_TO_DSVFS(bdp);
	int		error;
	bhv_desc_t	*nbdp;
	tk_set_t	tset_refused;
	int		s;

	/*
         * Arbitrate for right to actually do the unmount.  Once we have
         * established this right, we can store the id of the original
         * issuing cell.
	 */
	s = mutex_spinlock(&dsp->dsvfs_lock);
	if (dsp->dsvfs_flags & DSVFS_UNMOUNTING) {
		mutex_spinunlock(&dsp->dsvfs_lock, s);
		return EBUSY;
	} else {
		dsp->dsvfs_flags |= DSVFS_UNMOUNTING;
		mutex_spinunlock(&dsp->dsvfs_lock, s);
	}
	dsp->dsvfs_unmounter = issuer;

	/*
 	 * Acquire the 'traverse' token for write. This will revoke the
	 * 'traverse' token on all clients.
	 */
	tks_recall(dsp->dsvfs_tserver, DVFS_TRAVERSE_RD, &tset_refused);
	if (TK_IS_IN_SET(DVFS_TRAVERSE_RD, tset_refused)) {
		/* unable to recall the 'traverse' token
		 * this happens when client is unable to prepare for unmount
		 * usually because of a busy mount point.
		 */
		s = mutex_spinlock(&dsp->dsvfs_lock);
		dsp->dsvfs_flags &= ~DSVFS_UNMOUNTING;
		mutex_spinunlock(&dsp->dsvfs_lock, s);
		return(EBUSY);
	}
	/* make sure no other client can fault in this mount point */
	error = tkc_acquire1(dsp->dsvfs_tclient, DVFS_TRAVERSE_WR_SNGL);

	/*
	 * no lookup can occur on client cells.
	 * now lets try to unmount this FS from device cell.
	 */
	PVFS_NEXT(bdp, nbdp);
	PVFS_DOUNMOUNT(nbdp, flags, rootvp, cr, error);
	if (error) {
		/* has to release the write token first for the step below */
		tkc_release1(dsp->dsvfs_tclient, DVFS_TRAVERSE_WR_SNGL);

		/* failed the unmount, at this level we just try to
		 * notify the client cells that the unmount has been
		 * aborted so they can try to reacquire the token.
		 */
		tks_iterate(dsp->dsvfs_tserver, DVFS_EXIST_RD, 
				TKS_STABLE, dsvfs_abort_unmount);
		dsp->dsvfs_unmounter = CELL_NONE;
		s = mutex_spinlock(&dsp->dsvfs_lock);
		dsp->dsvfs_flags &= ~DSVFS_UNMOUNTING;
		mutex_spinunlock(&dsp->dsvfs_lock, s);
	}
	return(error);
}

dsvfs_t *
dsvfs_allocate(vfs_t *vfsp)
{
        dsvfs_t		*dsp;
	bhv_desc_t      *bdp;

	dsp = kmem_zone_alloc(dsvfs_zone, KM_SLEEP);
	bdp = &dsp->dsvfs_bhv;
	spinlock_init(&dsp->dsvfs_lock, "dsvf_lok");
	dsp->dsvfs_flags = 0;
	bhv_desc_init(bdp, dsp, vfsp, &cfs_dsvfsops);
	return (dsp);
}

#if CELL_IRIX
void
dsvfs_dummy_setup(void)
{
	int	i;
	vfs_t   *vfsp;

	for (i = 0; i < nfstype; i++) {
		vfsp = dvfs_dummytab[i];

		if (vfsp != NULL) {
			cfs_handle_t    vfs_hdl;

			ASSERT(dcvfs_dummytab[i] == NULL);
			ASSERT(dsvfs_dummytab[i] == NULL);
			dsvfs_dummytab[i] = dsvfs_allocate(vfsp);
			DSVFS_MKHANDLE(vfs_hdl, dsvfs_dummytab[i]);
			dcvfs_dummy(i, &vfs_hdl);
		}
	}
}
#endif /* CELL_IRIX */

