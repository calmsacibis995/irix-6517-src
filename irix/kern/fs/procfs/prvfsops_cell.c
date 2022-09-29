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

#ident	"$Revision: 1.6 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fs_subr.h>
#include <sys/sysmacros.h>
#include <sys/dnlc.h>
#include <sys/vfs.h>
#include <sys/pvfs.h>
#include <sys/vnode.h>
#include "prdata.h"
#include "prsystm.h"
#include "procfs_private.h"
#include "fs/cfs/cfs.h"
#include "fs/cell/cfs_intfc.h"

extern vfsops_t	procfs_dlc_vfsops;
extern vfsops_t	procfs_dls_vfsops;

extern vfsops_t cfs_vfsops;

/* ARGSUSED */
int 
procfs_realvfsops(
	vfs_t *vfsp,
        struct mounta *uap,
	vfsops_t **vfsop)
{
        if (PROCFS_CELL_HERE())
		return 0;
	*vfsop = &cfs_vfsops;
	vfsp->vfs_cell = PROCFS_CELL;
	return 0;
}

int
procfs_import(
	vfs_t *vfsp)
{
	bhv_desc_t *bhvm;
        int err;

	/*
         * Establish our new vfs.  There should be no conflict.
	 */
        err = procfs_newvfs(vfsp);
        ASSERT(err == 0);

	/*
         * Bring in the exported data.
         */
	ASSERT(vfsp->vfs_eisize == sizeof(procfs_info_t));
        bcopy(vfsp->vfs_expinfo, (char *) &procfs_info, sizeof(procfs_info_t));
        vfsp->vfs_expinfo = (char *) &procfs_info;

	/*
         * Insert our behavior as a local interposer
	 */
        bhvm = &procfs_bhv;
	bhv_desc_init(bhvm, NULL, vfsp, &procfs_dlc_vfsops);
        BHV_WRITE_LOCK(&vfsp->vfs_bh);
	bhv_insert(&vfsp->vfs_bh, bhvm);
        BHV_WRITE_UNLOCK(&vfsp->vfs_bh);

	/*
         * Now finish the initialization
	 */
        procfs_initnodes(vfsp);
        procfs_initvfs(vfsp);
	return 0;
}

static int
procfs_dlc_unmount(
	bhv_desc_t *bdp,
	int flags,
	struct cred *cr)
{
        int error;
        struct vfs *vfsp = bhvtovfs(bdp);
        bhv_desc_t *nbdp;

        PVFS_NEXT(bdp, nbdp);

	ASSERT(cr == NOCRED);
	ASSERT(flags & (UNMOUNT_FINAL | UNMOUNT_CKONLY));

	/*
	 * CFS makes sure that no new references will be created in
	 * the file system being unmounted.
	 */
	error = procfs_umcheck(vfsp);

	if (flags & UNMOUNT_FINAL)
		ASSERT(error == 0);
	if (flags & UNMOUNT_CKONLY) {
	        if (error == 0)
                        PVFS_UNMOUNT(nbdp, flags, cr, error);
		return error;
	}

	/*
         * Notice that we are removing the behavior here, even though VFS_UNMOUNT
         * get the behavior lock for read.  This cannot cause a problem because
         * the finalization call only occurs when the object can no longer be 
         * referenced.
         *
         * This is probably an indication that overloading VFS_UNMOUNT in this 
         * way is not such a good idea.
         */
	ASSERT(error == 0);
        procfs_umfinal(vfsp, bdp);
	PVFS_UNMOUNT(nbdp, flags, cr, error);
	ASSERT(error == 0);
	return 0;
}

/* ARGSUSED */
static int
procfs_dls_unmount(
	bhv_desc_t *bdp,
	int flags,
	struct cred *cr)
{
        int error;
        struct vfs *vfsp = bhvtovfs(bdp);
        bhv_desc_t *nbdp;

        PVFS_NEXT(bdp, nbdp);

	ASSERT(flags == 0);

	error = procfs_umcheck(vfsp);

	if (error == 0) 
		PVFS_UNMOUNT(nbdp, UNMOUNT_CKONLY, NOCRED, error);

	if (error)
		return (error);

        procfs_umfinal(vfsp, bdp);

	PVFS_UNMOUNT(nbdp, UNMOUNT_FINAL, NOCRED, error);
        VFS_REMOVEBHV(vfsp, nbdp);
        cfs_dcvfs_behavior_free(nbdp);
	return 0;
}


vfsops_t procfs_dlc_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_DIST_LC),
	fs_nosys,       /* mount */
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	vfs_passthru_vfs_dounmount,
	procfs_dlc_unmount,
	prroot,
	prstatvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,       /* realvfsops */
	fs_nosys,	/* import */
	fs_nosys,	/* quotactl */
};

vfsops_t procfs_dls_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_DIST_LS),
	fs_nosys,       /* mount */
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_dounmount,
	procfs_dls_unmount,
	prroot,
	prstatvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,       /* realvfsops */
	fs_nosys,	/* import */
	fs_nosys,       /* quotactl */
};


/*
 * Does special fixup for a procfs mount in the CELL configuration.
 * 
 * Because of the disributed nature of the process space, the home cell for
 * procfs will have both server and client vnodes.  Because of the existence
 * of client vnodes, there needs to be a cfs dc vfs behavior below our procfs
 * behavior.  This cfs client behavior keeps track of the set of client vnodes.
 * Our procfs vfs behavior takes the role of an interposer although only unmount
 * will be passed through.  For all other ops, the portion of the behavior chain
 * ending at the procfs behavior is all that is ever used.
 *
 * Typical procfs vfs behavior chain on the nominal server cell:
 *
 *    dsvfs: cfs_dsvfsops (Only if there is a remote client cell, deals with
 *                         export of procfs to other cells)
 *    procfs: procfs_dls_vfsops (Main procfs behavior)
 *    dcvfs: cfs_vfsops (Deals with client procfs vnodes exported by other
 *                       cells)
 *
 * Typical procfs vfs behavior chain on the nominal client cell:
 *
 *    procfs: procfs_dlc_vfsops (Main procfs behavior)
 *    dcvfs: cfs_vfsops (Deals with client procfs vnodes exported by other
 *                       cells and vfs identity exported by nominal server cell)
 */    
void
procfs_cellmount(
        vfs_t *vfsp,
        bhv_desc_t *bdp)
{
        bhv_desc_t *bdp_cfs;

        bdp->bd_ops = (void *) &procfs_dls_vfsops;
	bdp_cfs = cfs_dcvfs_behavior(vfsp, procfs_type);
        BHV_WRITE_LOCK(&vfsp->vfs_bh);
        bhv_insert(&vfsp->vfs_bh, bdp_cfs);
        BHV_WRITE_UNLOCK(&vfsp->vfs_bh);
}

/*
 * Export procfs handle to another cell
 *
 * This to make sure that the remote cell can instantiate the procfs vfs which
 * is necessary when creating a prnode remotely.
 */
void
procfs_export_fs(
	cfs_handle_t *vfsh)
{
        ASSERT(procfs_vfs);
        cfs_vfsexport(procfs_vfs, vfsh);
}

/*
 * Import procfs handle from another cell
 *
 * This is to make sure we have a procfs vfs when processing a message to 
 * instantiate a prnode.
 */
void
procfs_import_fs(
	cfs_handle_t *vfsh)
{
        while (procfs_vfs == NULL) {
		vfs_t *vfsp;

                cfs_vfsimport(vfsh, &vfsp);
		ASSERT(vfsp == procfs_vfs);
	}
}
