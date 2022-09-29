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

#ident	"$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fs_subr.h>
#include <sys/sysmacros.h>
#include <sys/dnlc.h>
#include <sys/vfs.h>
#include <sys/pvfs.h>
#include <sys/vnode.h>
#include <ksys/cell.h>
#include <fs/fdfs/fdfs_private.h>

#if CFS_TEST
#define         FDFS_CELL	1
#else
#define         FDFS_CELL	golden_cell
#endif 

extern vfsops_t	dlfdfs_vfsops;

extern vfsops_t cfs_vfsops;

/* ARGSUSED */
int 
fdfs_realvfsops(
	vfs_t *vfsp,
        struct mounta *uap,
	vfsops_t **vfsop)
{
        if (FDFS_CELL == cellid())
		return 0;
	*vfsop = &cfs_vfsops;
	vfsp->vfs_cell = FDFS_CELL;
	return 0;
}

int
fdfs_import(
	vfs_t *vfsp)
{
	bhv_desc_t *bhvm;
        fdfs_mount_t *fmp;
        int dev;

	/*
         * Set up the fdfs mount structure
         */
	fmp = (fdfs_mount_t *) kmem_alloc(sizeof(fdfs_mount_t), 0);
        bhvm = FDFS_MTOBHV(fmp);
	bhv_desc_init(bhvm, fmp, vfsp, &dlfdfs_vfsops);
        BHV_WRITE_LOCK(&vfsp->vfs_bh);
	bhv_insert(&vfsp->vfs_bh, bhvm);
        BHV_WRITE_UNLOCK(&vfsp->vfs_bh);
	ASSERT(vfsp->vfs_eisize == sizeof(fdfs_info_t));
        bcopy(vfsp->vfs_expinfo, (char *) &fmp->mount_info, sizeof(fdfs_info_t));
        vfsp->vfs_expinfo = (char *) &fmp->mount_info;
	dev = major(fmp->mount_info.info_dev);
	fmp->mount_refcnt = 0;

	/* 
         * Create a root vnode
	 */
	fdfs_newroot(vfsp, fmp);

	/*
	 * Finish filling in the vfs
	 */
	vfsp->vfs_fstype = fdfstype;
	vfsp->vfs_dev = dev;
	vfsp->vfs_fsid.val[0] = dev;
	vfsp->vfs_fsid.val[1] = fdfstype;
	vfsp->vfs_bsize = 1024;
        vfsp->vfs_flag |= (VFS_CELLROOT|VFS_CELLULAR);
        vfsp->vfs_expinfo = (char *) &fmp->mount_info;
        vfsp->vfs_eisize = sizeof(fdfs_info_t);
	return 0;
}

/* ARGSUSED */
static int
dlfdfs_unmount(
	bhv_desc_t *bdp,
	int flags,
	struct cred *cr)
{
        int error = 0;
        fdfs_mount_t *fmp = FDFS_BHVTOM(bdp);
        struct vfs *vfsp = bhvtovfs(bdp);
        bhv_desc_t *nbdp;

        PVFS_NEXT(bdp, nbdp);

	ASSERT(cr == NOCRED);
	ASSERT(flags & (UNMOUNT_FINAL | UNMOUNT_CKONLY));

	/*
	 * CFS makes sure that no new references will be created in
	 * the file system being unmounted.
	 * What if fdfs is busy currently ??
	 * How do we keep track of vnodes without cache and FS_INACTIVE ?
	 * Assumption here is that the created vnode just represents another
	 * vnode in some other file system and its association with fdfs
	 * is not important. Use mount_refcnt anyway.
	 */
	if (fmp->mount_root->v_count > 1)
	        error = EBUSY;
	else if (fmp->mount_refcnt != 0) 
	        error = EBUSY;

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
	VN_RELE(fmp->mount_root);
	fmp->mount_root = NULL;
	VFS_REMOVEBHV(vfsp, bdp);
	PVFS_UNMOUNT(nbdp, flags, cr, error);
	ASSERT(error == 0);
	return 0;
}


vfsops_t dlfdfs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_DIST_LC),
	fs_nosys,       /* mount */
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	vfs_passthru_vfs_dounmount,
	dlfdfs_unmount,
	fdfs_root,
	fdfs_statvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,       /* realvfsops */
	fs_nosys,	/* import */
	fs_quotactl     /* quotactl */
};



