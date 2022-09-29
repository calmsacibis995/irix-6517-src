/*
 * Copyright (c) 1988, 1991 by Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/pathname.h>
#include <sys/kmem.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/fs/lofs_info.h>
#include <sys/fs/lofs_node.h>
#include <sys/mount.h>
#include <sys/atomic_ops.h>
#include "sys/fs_subr.h"

/*
 * This is the loadable module wrapper.
 */
#include <sys/mload.h>

extern vfsops_t lofs_vfsops;
extern vnodeops_t lofs_vnodeops;

extern int lofs_init(struct vfssw *, int);
extern void lofs_subrinit(void);
vnode_t *makelonode(struct vnode *, struct loinfo *);

#ifdef	LODEBUG
extern int lodebug;
#endif

kmutex_t li_lock;		/* protects lofs info structs */
static dev_t lofs_rdev;
int lofsfstype;
uint lofs_mountid = 0;          /* unique mount id for each lofs FS mounted */

int
lofs_init(struct vfssw *vswp, int fstyp)
{
	vswp->vsw_vfsops = &lofs_vfsops;
	lofsfstype = fstyp;
	lofs_rdev = 1;
	mutex_init(&li_lock, MUTEX_DEFAULT, "lofs info struct lock");
	lofs_subrinit();

	return (0);
}

/*
 * lofs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
static int
lofs_mount(struct vfs *vfsp,
	struct vnode *vp,
	struct mounta *uap,
	char *attrs,
	struct cred *cr)
{
	int error;
	struct vnode *srootvp = NULL;	/* the server's root */
	struct vnode *realrootvp;
	struct loinfo *li;

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT)) {
		return (EPERM);
	}

	/*
	 * Find real root, and make vfs point to real vfs
	 */
	if (error = lookupname(uap->spec, UIO_USERSPACE, FOLLOW, NULLVPP,
	    &realrootvp, NULL)) {
		return (error);
	}

	/*
	 * If the real vp is autofs (ie. ourself) then don't allow the mount.
	 */
	if (realrootvp == vp) {
		return (EBUSY);
	}

	/*
	 * allocate a lofs info struct and attach it
	 */
	li = (struct loinfo *)kmem_zalloc(sizeof (*li), KM_SLEEP);
	li->li_realvfs = realrootvp->v_vfsp;
	li->li_mountvfs = vfsp;
	/*
	 * Set mount flags to be inherited by loopback vfs's
	 */
	if (uap->flags & MS_RDONLY)
		li->li_mflag |= VFS_RDONLY;
	if (uap->flags & MS_NOSUID)
		li->li_mflag |= VFS_NOSUID;

	/*
	 * Propagate inheritable mount flags from the real vfs.
	 */
	if (li->li_realvfs->vfs_flag & VFS_RDONLY)
		uap->flags |= MS_RDONLY;
	if (li->li_realvfs->vfs_flag & VFS_NOSUID)
		uap->flags |= MS_NOSUID;

	li->li_refct = 0;
	mutex_enter(&li_lock);
	li->li_rdev = lofs_rdev++;
	mutex_exit(&li_lock);
	vfs_insertbhv(vfsp, &li->li_bhv, &lofs_vfsops, li);
	vfsp->vfs_bcount = 0;
	vfsp->vfs_fstype = lofsfstype;
	vfsp->vfs_bsize = li->li_realvfs->vfs_bsize;
	vfsp->vfs_dev = li->li_realvfs->vfs_dev;
	vfsp->vfs_fsid.val[0] = li->li_realvfs->vfs_fsid.val[0];
	vfsp->vfs_fsid.val[1] = li->li_realvfs->vfs_fsid.val[1];
	/*
	 * Make the root vnode
	 */
	srootvp = makelonode(realrootvp, li);
	if ((srootvp->v_flag & VROOT) &&
	    ((uap->flags & MS_OVERLAY) == 0)) {
		VN_RELE(srootvp);
		kmem_free(li, sizeof (*li));
		return (EBUSY);
	}
	VN_FLAGSET(srootvp, VROOT);
	li->li_rootvp = srootvp;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4,
	    "lofs_mount: vfs %x realvfs %x root %x realroot %x li %x\n",
	    vfsp, li->li_realvfs, srootvp, realrootvp, li);
#endif
	return (0);
}

/*
 * Undo loopback mount
 */
/*ARGSUSED*/
static int
lofs_unmount(bhv_desc_t *bdp, int flags, struct cred *cr)
{
	struct loinfo *li;

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return (EPERM);

	li = vfs_bhvtoli(bdp);
#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_unmount(0x%x) li 0x%x\n", bdp, li);
#endif
	if (li->li_refct != 1 || li->li_rootvp->v_count != 1) {
#ifdef LODEBUG
		lofs_dprint(lodebug, 3, "refct %d v_ct %d\n", li->li_refct,
		    li->li_rootvp->v_count);
#endif
		return (EBUSY);
	}
	VN_RELE(li->li_rootvp);
	VFS_REMOVEBHV(bhvtovfs(bdp), &li->li_bhv);
	kmem_free(li, sizeof (*li));
	return (0);
}

/*
 * find root of lo
 */
static int
lofs_root(bhv_desc_t *bdp, struct vnode **vpp)
{
	*vpp = (struct vnode *)vfs_bhvtoli(bdp)->li_rootvp;
#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_root(0x%x) = 0x%x\n", bdp, *vpp);
#endif
	VN_HOLD(*vpp);
	return (0);
}

/*
 * Get file system statistics.
 */
static int
lofs_statvfs(register bhv_desc_t *bdp, struct statvfs *sbp, struct vnode *vp)
{
	int error;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_statvfs 0x%x\n", bdp);
#endif
	VFS_STATVFS(lofs_realvfs(bhvtovfs(bdp), vfs_bhvtoli(bdp)), sbp, vp, error); 
	return(error);
}

/*
 * Flush any pending I/O.
 */
static int
lofs_sync(bhv_desc_t *bdp,
	int flag,
	struct cred *cr)
{
	int error;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_sync: 0x%x\n", bdp);
#endif
	if (bdp != (bhv_desc_t *)NULL) {
		VFS_SYNC(lofs_realvfs(bhvtovfs(bdp), vfs_bhvtoli(bdp)), flag, cr, error);
		return(error);
	}
	return (0);
}

/*
 * lo vfs operations vector.
 */
vfsops_t lofs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	lofs_mount,
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_dounmount,
	lofs_unmount,
	lofs_root,
	lofs_statvfs,
	lofs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_noerr,	/* realvfsops */
	fs_import,	/* import */
	fs_quotactl     /* quotactl */
};
