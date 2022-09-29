/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident	"$Revision: 1.18 $"

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fs_subr.h>
#include <sys/hwgraph.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include "hwgnode.h"


/*
** hardware graph pseudo-filesystem file system operations.
**
** This file system reflects the hardware graph into a filesystem name
** space.  The end result is that when devices are added/removed from a
** system, corresponding file system names are automatically added/removed
** with no need for any mknod.
**
** Currently, this file system is viewed as somethine useful to the 
** administrator when dealing with disks, and other special files that are 
** accessed infrequently by root.  At this time, it is NOT intended to be a 
** general-purpose replacement for the /dev directory!
*/


extern void hwginit(struct vfssw *, int);
static int hwgmount(struct vfs *, struct vnode *, struct mounta *, char *, struct cred *);
static int hwgunmount(bhv_desc_t *, int, struct cred *);
static int hwgroot(bhv_desc_t *, struct vnode **);
static int hwgstatvfs(bhv_desc_t *, struct statvfs *, struct vnode *);
static int hwgsync( bhv_desc_t *, int, struct cred *);

vfsops_t hwgvfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	hwgmount,
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_dounmount,
	hwgunmount,
	hwgroot,
	hwgstatvfs,
	hwgsync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_noerr,	/* realvfsops */
	fs_import,	/* import */
	fs_quotactl     /* quotactl */
};

dev_t hwgdev;
int hwgfstype = 0;

/*
** hwgmount makes the hardware graph accessible as a file system rooted
** at the given mount point.
*/
/* ARGSUSED */
static int
hwgmount(struct vfs *vfsp, struct vnode *mvp, struct mounta *uap, char *attrs, struct cred *cr)
{
	hwgnode_t *hwgp;
	hwgmount_t *hwgmnt;

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return(EPERM);

	if (!_CAP_CRABLE(cr, CAP_DEVICE_MGT))
		return(EPERM);

	if (mvp->v_type != VDIR)
		return(ENOTDIR);

	if ((mvp->v_count > 1) || 
	    (mvp->v_flag & VROOT))
		return(EBUSY);

	ASSERT(hwgraph_root != GRAPH_VERTEX_NONE);
	hwgraph_vertex_ref(hwgraph_root);
	hwgmnt = kmem_alloc(sizeof(*hwgmnt), KM_SLEEP);
	hwgmnt->hwgmnt_count = 0;
	hwgmnt->hwgmnt_head = NULL;
	hwgmnt->hwgmnt_cursor = NULL;
	vfs_insertbhv(vfsp, &hwgmnt->hwgmnt_vfsbhv, &hwgvfsops, hwgmnt);

	hwgp = hwgfs_allocnode(hwgraph_root, VDIR, hwgmnt);
	hwgmnt->hwgmnt_root = HWG_TO_VNODEBHV(hwgp);
	VN_FLAGSET(HWG_TO_VNODE(hwgp), VROOT);

	vfsp->vfs_fstype = hwgfstype;
	vfsp->vfs_dev = hwgdev;
	vfsp->vfs_fsid.val[0] = hwgdev;
	vfsp->vfs_fsid.val[1] = hwgfstype;
	vfsp->vfs_bsize = 1024;
	vfs_setflag(vfsp, VFS_NOTRUNC | VFS_CELLULAR);

	return(0);
}


/*
** hwgunmount unmount's the hardware graph from the given mount point.
*/
/* ARGSUSED */
static int
hwgunmount(bhv_desc_t *bdp, int flags, struct cred *cr)
{
	hwgmount_t *hwgmnt = VFSBHV_TO_HWGMNT(bdp);
	struct vnode *hwgrootvp = HWGMNT_TO_ROOTVP(hwgmnt);

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return(EPERM);

	if (!_CAP_CRABLE(cr, CAP_DEVICE_MGT))
		return(EPERM);

	if (hwgmnt->hwgmnt_count > 1)
		return(EBUSY);

	if (hwgrootvp->v_count > 1)
		return(EBUSY);

	VN_RELE(hwgrootvp);
	VFS_REMOVEBHV(bhvtovfs(bdp), bdp);
	hwgfs_freenode(hwgmnt->hwgmnt_root);
	kmem_free(hwgmnt, sizeof(*hwgmnt));
	hwgraph_vertex_unref(hwgraph_root);

	return(0);
}


/*
** hwgroot returns the root of the hardware graph file system.
*/
/* ARGSUSED */
static int
hwgroot(bhv_desc_t *bdp, struct vnode **vpp)
{
	hwgmount_t *hwgmnt = VFSBHV_TO_HWGMNT(bdp);
	struct vnode *hwgrootvp = HWGMNT_TO_ROOTVP(hwgmnt);

	VN_HOLD(hwgrootvp);
	*vpp = hwgrootvp;
	return(0);
}




/*
** hwgstatvfs returns status information for the hardware graph file system.
*/
/* ARGSUSED */
static int
hwgstatvfs(
	bhv_desc_t *bdp,
	struct statvfs *sp,
	struct vnode *vp)
{
	graph_error_t rv;
	graph_attr_t hwgsumm;
	struct vfs *vfsp = bhvtovfs(bdp);

	if (bdp == NULL)
		return EINVAL;

	rv = hwgraph_summary_get(&hwgsumm);
	if (rv != GRAPH_SUCCESS)
		cmn_err(CE_PANIC, "hwgstatvfs could not get summary");

	bzero((caddr_t)sp, sizeof(*sp));
	sp->f_bsize	= NBPP;
	sp->f_frsize	= NBPP;
	sp->f_blocks	= 0;
	sp->f_bfree	= 0;
	sp->f_bavail	= 0;
	sp->f_files	= hwgsumm.ga_num_vertex;
	sp->f_ffree	= hwgsumm.ga_num_vertex_max;
	sp->f_favail	= 0;
	sp->f_fsid	= vfsp->vfs_dev;
	strcpy(sp->f_basetype, vfssw[hwgfstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = LABEL_LENGTH_MAX;
	strcpy(sp->f_fstr, "/hw");
	strcpy(&sp->f_fstr[6], "/hw");
	return(0);
}


/*
** hwgsync is useful only in the rare case that we unmount hwgfs.
** We attempt to purge all hwgfs vnodes associated with the mount.
*/
/* ARGSUSED */
static int
hwgsync(
	bhv_desc_t	*bdp,
	int		flags,
	struct cred	*cr)
{
	hwgmount_t *hwgmnt = VFSBHV_TO_HWGMNT(bdp);
	hwgnode_t *cur_hwgp;
	vnode_t *vp;
	vmap_t vmap;

	mrupdate(&hwgfs_node_mrlock);

	/*
	 * Iterate through all hwgfs nodes associated with this mount, 
	 * and attempt to vn_purge every vnode that's not currently in use.
	 */
	cur_hwgp = hwgmnt->hwgmnt_head;
	while (cur_hwgp) {
		hwgmnt->hwgmnt_cursor = cur_hwgp->hwg_next_on_fs;
		vp = HWG_TO_VNODE(cur_hwgp);
		if (vp->v_count == 0) {
			VMAP(vp, vmap);
			mrunlock(&hwgfs_node_mrlock);
			vn_purge(vp, &vmap);
			mrupdate(&hwgfs_node_mrlock);
		}
		cur_hwgp = hwgmnt->hwgmnt_cursor;
	}

	mrunlock(&hwgfs_node_mrlock);

	return(0);
}
