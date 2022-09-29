/*
 * UMFS filesystem operations.
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
 
#ident	"$Revision: 1.3 $"

#include <values.h>
#include <strings.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/pfdat.h>		/* page flushing prototypes */
#include <sys/quota.h>
#include <sys/sema.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/capability.h>

/* ARGSUSED */
int
umfs_init(struct vfssw *vswp, int fstype)
{
	return 0;
}

/* ARGSUSED */
static int  umfs_mount(
	struct vfs	*vfsp,
        struct vnode	*mvp,
        struct mounta	*uap,
	char		*attrs,
        struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_rootinit(
	struct vfs	*vfsp)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_mntupdate(
	bhv_desc_t	*bdp,
        struct vnode	*mvp,
        struct mounta	*uap,
        struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_dounmount(
	bhv_desc_t 	*bdp, 
        int 		flags, 
        vnode_t 	*rootvp, 
        cred_t 		*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_unmount(
	bhv_desc_t	*bdp, 
	int		flags, 
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_root(
	bhv_desc_t	*bdp, 
	struct vnode	**vpp)

{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_statvfs(
	bhv_desc_t	*bdp, 
	struct statvfs	*sp, 
	struct vnode	*vp)

{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_sync(
	bhv_desc_t	*bdp, 
	register int	flags, 
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_vget(
	bhv_desc_t	*bdp, 
	struct vnode	**vpp, 
	struct fid	*fidp)

{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_mountroot(
	bhv_desc_t	    *bdp, 
	enum whymountroot   why)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_quotactl(
	struct bhv_desc *bdp,
	int		cmd,
	int		id,
	caddr_t		addr)
{
    return ENOSYS;
}

vfsops_t umfs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	umfs_mount,
	umfs_rootinit,		/*NEW*/
	umfs_mntupdate,
	umfs_dounmount,
	umfs_unmount,
	umfs_root,
	umfs_statvfs,
	umfs_sync,
	umfs_vget,
	umfs_mountroot,		/*NEW*/
	fs_realvfsops,
	fs_import,
	umfs_quotactl
};
