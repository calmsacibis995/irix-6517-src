/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* ident  "$Revision: 1.4 $" */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/pfdat.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>

#include <fs/cell/dvn_tokens.h>
#include "cxfs.h"
#include "dcxvn.h"

zone_t	*dcxvn_zone;

void	dcxvn_init()
{
	dcxvn_zone = kmem_zone_init(sizeof(dcxvn_t), "dcxvn");
}


/* ARGSUSED */ 
dcxvn_t *
cxfs_dcxvn_make(
	dcvn_t		*dcp,
	dcxvfs_t	*dcxvfs,
	vattr_t		*vattrp,
	tkc_state_t	*tokens,
	int		cxfs_flags,
	char		*cxfs_buff,
	size_t		cxfs_count)
{
	dcxvn_t		*dcxp;

	dcxp = kmem_zone_alloc(dcxvn_zone, KM_SLEEP);
	dcxp->dcx_dcvn = dcp;
	dcxp->dcx_vfs = dcxvfs;
	dcxp->dcx_vap = vattrp;
	dcxp->dcx_tokens = tokens;
	if (cxfs_flags & CXFS_IFEXTENTS) {
		dcxp->dcx_extents = cxfs_buff;
		dcxp->dcx_ext_count = cxfs_count;
	} else {
		dcxp->dcx_extents = NULL;
	}

	return(dcxp);
}

/* ARGSUSED */
void
cxfs_dcxvn_obtain(
	dcxvn_t	*dcxp,
	tk_set_t	obtain_set,
	tk_set_t	grant_set,
	tk_set_t	already_obtset,
	tk_set_t	refused_set,
	int		cxfs_flags,
	char		*cxfs_buff,
	size_t		cxfs_count)
{
	if (cxfs_flags & CXFS_IFEXTENTS) {
		if (dcxp->dcx_extents) {
			kmem_free(dcxp->dcx_extents, dcxp->dcx_ext_count);
		}
		dcxp->dcx_extents = cxfs_buff;
		dcxp->dcx_ext_count = cxfs_count;
	}
}

void
cxfs_dcxvn_destroy(
	dcxvn_t	*dcxp)
{
	if (dcxp->dcx_extents) {
		kmem_free(dcxp->dcx_extents, dcxp->dcx_ext_count);
	}
	kmem_zone_free(dcxvn_zone, dcxp);
}


/* ARGSUSED */
tk_set_t
cxfs_dcxvn_recall(
	dcxvn_t 	*dcxp,
	tk_set_t        recset,
	tk_disp_t       why)
{
	/*
	 * This routine needs to check the CXFS specific tokens and
	 * determine if they can be recalled. If not, they should
	 * be refused via an RPC back to the server (i.e. invk_dsvn_refuse).
	 *
	 * If refuseed, they should be removed
	 * from the tk_set_t and NOT returned back to CFS.
	 * 
	 * the tkc_recall will be handled by CFS.
	 */
	return(recset);
}

/* ARGSUSED */
void
cxfs_dcxvn_return(
	dcxvn_t 	*dcxp,
	tk_set_t        retset,
	tk_set_t        unknownset,
	tk_disp_t       why)
{
	/*
	 * This routine is called when dcvn_return is invoked
	 * which is the client token callout invoked when a tkc_recall
	 * is called. The token is being "allowed to revoke" and we
	 * are guaranteed that no-one has the object (tkc_release has been called)
	 * so we can now throw it away knowing that noone else is looking at it.
	 *
	 * The tkc_recall in the cxfs_dcxvn_recall will cause this routine
	 * to be called.
	 */

	if (TK_IS_IN_SET(DVN_EXTENT_READ, retset)) {
		if (dcxp->dcx_extents) {
			kmem_free(dcxp->dcx_extents, dcxp->dcx_ext_count);
		}
		dcxp->dcx_extents = 0;
	}
	return;
}
