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
#ident  "$Revision: 1.14 $"

#include "dv.h"
#include "dvfs.h"
#include "ksys/cell/tkm.h"
#include <sys/pvfs.h>
#include <sys/fs_subr.h>


/*
 * Server-side vfs ops for the Cell File System.
 */

static int
dsvfs_unmount(
	bhv_desc_t	*bdp,
	int		flags,
	struct cred	*cr)
{
	bhv_desc_t	*nbdp;
	int 		error;
	dsvfs_t		*dsp;

	PVFS_NEXT(bdp, nbdp);
	PVFS_UNMOUNT(nbdp, flags, cr, error);
	if (error)
		return(error);
	/* return the traverse token */
	dsp = BHV_TO_DSVFS(bdp);
	tkc_release1(dsp->dsvfs_tclient, DVFS_TRAVERSE_WR_SNGL);
	/* initiates phase 2 of the unmount by revoking the exist token */
	error = tkc_acquire1(dsp->dsvfs_tclient, DVFS_EXIST_WR_SNGL);
	ASSERT(!error);
	tkc_release1(dsp->dsvfs_tclient, DVFS_EXIST_WR_SNGL);
	/* tear down the ds structure */
	tkc_destroy_local(dsp->dsvfs_tclient);
	tks_destroy(dsp->dsvfs_tserver);
	VFS_REMOVEBHV(bhvtovfs(bdp), DSVFS_TO_BHV(dsp));
	spinlock_destroy(&dsp->dsvfs_lock);
        dsp->dsvfs_gen = 0;
	kmem_zone_free(dsvfs_zone, dsp);
	return(0);
	
}


/*
 * dsvfs_mntupdate
 */
/*ARGSUSED*/
static int
dsvfs_mntupdate(
	bhv_desc_t *bdp,
        struct vnode *mvp,
        struct mounta *uap,
        struct cred *cr)
{
	/* XXX later */
	panic("dsvfs_mntupdate: not yet implemented");
	return(0);
}

/*
 * dsvfs_dounmount
 */
static int
dsvfs_dounmount(
	bhv_desc_t	*bdp,
	int		flags,
        struct vnode    *rootvp,
	cred_t		*cr)
{
     	return (dsvfs_dounmount_subr(bdp, flags, rootvp, cr, cellid()));
}


/*
 * dsvfs_sync
 * until there's vnode caching on client cells, there's nothing to be done
 * with sync on client cell. All client sync calls should be function
 * shipped to device cell and the DS VFS_SYNC is just a pass-through op.
 */
/*ARGSUSED*/
static int
dsvfs_sync(
	bhv_desc_t	*bdp,
	int		flags,
	cred_t		*cred)
{
	int             error;
        bhv_desc_t      *nbdp;

        PVFS_NEXT(bdp, nbdp);
        PVFS_SYNC(nbdp, flags, cred, error);

        return error;
}

int
dsvfs_force_pinned (bhv_desc_t      *bdp)
{
        bhv_desc_t      *nbdp;
	int		error;

        PVFS_NEXT(bdp, nbdp);
        PVFS_FORCE_PINNED(nbdp, error);
	return(error);
}
 

vfsops_t cfs_dsvfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_CFS_DS),
	fs_nosys,			/* mount */
	fs_nosys,			/* rootinit */
	dsvfs_mntupdate,		/* mntupdate */
	dsvfs_dounmount,		/* dounmount */
	dsvfs_unmount,			/* unmount */
	vfs_passthru_vfs_root,		/* root */
	vfs_passthru_vfs_statvfs,	/* statvfs */
	dsvfs_sync,			/* sync */
	vfs_passthru_vfs_vget,		/* vget */
	fs_nosys,			/* mountroot */
	fs_nosys,			/* realvfsops */
	fs_nosys,  		        /* import */
	fs_quotactl,             	/* quotactl */
	dsvfs_force_pinned		/* force_pinned */
};
	
	
	

