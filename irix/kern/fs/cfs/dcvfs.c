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
#ident  "$Revision: 1.7 $"

#include "dv.h"
#include "dvn.h"
#include "dvfs.h"
#include "invk_dsvfs_stubs.h"
#include "ksys/cell/tkm.h"
#include <sys/dnlc.h>

/*
 * VFS client implementation of remote function calls
 */

/*
 * device server tells us to abort phase 1 of unmount
 * - reacquire token, undo the unmount preparation
 */	
void
I_dcvfs_abort_unmount(
	cfs_handle_t		*rvp_hdlp,
	cfs_handle_t		*vfs_hdlp,
	cell_t                  unmounter)
{
	dcvfs_t		*dcp;
	vfs_t		*vfsp; 
	vnode_t		*rvp;
	/* REFERENCED */
	int		error;
	
	dcp = dcvfs_handle_lookup(vfs_hdlp);
	ASSERT(dcp);
	vfsp = DCVFS_TO_VFS(dcp);
	error = tkc_acquire1(dcp->dcvfs_tclient, DVFS_TRAVERSE_RD_SNGL);
	ASSERT(!error);

	/*
         * In the normal case in which the root is a CFS vnode, we need to
         * import the root vnode back into this cell.
	 */
	if (!(vfsp->vfs_flag & VFS_CELLROOT)) {
		/* 
		 * Import the root vnode back into this cell, and set its
		 * VROOT flag.  Note that the vfs lock is still held so no one
		 * can traverse into this FS to find this vnode yet.
		 */
	        cfs_vnimport(rvp_hdlp, &rvp);
		VN_FLAGSET(rvp, VROOT);
		ASSERT(dcp->dcvfs_rootvp == NULL);
		dcp->dcvfs_rootvp = rvp;
		
		/* restore rootdir */
		if (rootvfs == vfsp) {
		        ASSERT(rootdir == NULL);
			rootdir = rvp;
		}
	}

	/*
         * If we are the issuer, responsibility for unlocking in the error
	 * case is the responsibility of dcvfs_dounmount.
	 */
	if (unmounter != cellid())
		vfs_unlock(vfsp);
}

/*
 * I_dcvfs_vfsrecall() 
 */
void
I_dcvfs_vfsrecall(
	cfs_handle_t		*vfs_hdlp,	/* vfs to be unmounted */
	tk_set_t		tset_recall,	/* token to be recalled */
	tk_disp_t		why,
	cell_t                  unmounter,
	int			*errorp)
{
	dcvfs_t 	*dcp;

	dcp = dcvfs_handle_lookup(vfs_hdlp);
	ASSERT(dcp);

	/* figure out which phase of unmount we're in */
	if (TK_IS_IN_SET(DVFS_TRAVERSE_RD, tset_recall)) {
		/* 
		 * phase 1 of the unmount, client prepare for unmount 
		 * basically prevent processes on this cell from traversing
		 * this file system.
		 */
		*errorp = dcvfs_prepare_unmount(dcp, unmounter);
		if (*errorp) {
			return;
		}
			
	} else if (TK_IS_IN_SET(DVFS_EXIST_RD, tset_recall)) {
		/* REFERENCED */
	  	int             error;

		/* phase 2 of unmount */
	        VFS_UNMOUNT(DCVFS_TO_VFS(dcp), UNMOUNT_FINAL, NOCRED, error);
		ASSERT(error == 0);

		dcvfs_commit_unmount(dcp, unmounter);
	}
	else
		panic("I_dcvfs_vfsrecall");
	tkc_release(dcp->dcvfs_tclient, tset_recall);
	tkc_recall(dcp->dcvfs_tclient, tset_recall, why);
	*errorp = 0;
}
	
	
