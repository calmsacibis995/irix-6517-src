/*
 * UMFS vnode operations.
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

#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <ksys/ddmap.h>
#include <sys/dnlc.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <sys/iograph.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/mode.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/pfdat.h>		/* page flushing prototypes */
#include <sys/poll.h>
#include <sys/quota.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <sys/capability.h>
#include <sys/flock.h>
#include <sys/kfcntl.h>
#include <string.h>

/* ARGSUSED */
static int  umfs_open(
	bhv_desc_t	*bdp,
	struct vnode	**vpp,
	mode_t		flag,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_close(
	bhv_desc_t	*bdp,
	int		flag,
	lastclose_t	lastclose,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_read(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*cr,
	struct flid	*fl)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_write(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*cr,
	struct flid	*fl)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_ioctl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flag,
	struct cred	*cr,
	int		*rvalp,
        struct vopbd	*vbds)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_setfl(
	bhv_desc_t	*bdp,
	int		oflags,
	int		nflags,
	cred_t		*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_getattr(
	bhv_desc_t	*bdp, 
	struct vattr	*vap, 
	int		flags, 
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_setattr(
	bhv_desc_t	*bdp, 
	struct vattr	*vap, 
	int		flags, 
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_access(
	bhv_desc_t	*bdp, 
	int		mode, 
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_lookup(
	bhv_desc_t	*bdp, 
	char		*nm, 
	struct vnode	**vpp, 
	struct pathname	*pnp, 
	int		flags, 
	struct vnode	*rdir, 
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_create(
	bhv_desc_t	*bdp,
	char		*name,
	struct vattr	*vap,
	int		flags,
	int		mode,
	struct vnode	**vpp,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_remove(
	bhv_desc_t	*bdp,
	char		*nm,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_link(
	bhv_desc_t	*tbdp,
	struct vnode	*svp,
	char		*tnm,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_rename(
	bhv_desc_t	*sbdp,		/* old (source) parent vnode */
	char		*snm,		/* old (source) entry name */
	struct vnode	*tdvp,		/* new (target) parent vnode */
	char		*tnm,		/* new (target) entry name */
	struct pathname	*tpnp,		/* new (target) pathname or null */
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_mkdir(
	bhv_desc_t	*bdp,
	char		*dirname,
	struct vattr	*vap,
	struct vnode	**vpp,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_rmdir(
	bhv_desc_t	*bdp,
	char		*nm,
	struct vnode	*cdir,
	struct cred	*cr)
{
    return ENOSYS;
}
	
/* ARGSUSED */
static int  umfs_readdir(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	struct cred	*cr,		/* from file table entry */
	int		*eofp)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_symlink(
	bhv_desc_t	*bdp,		/* ptr to parent dir vnode */
	char		*linkname,	/* name of symbolic link */
	struct vattr	*vap,		/* attributes */
	char		*target,	/* target path */
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_readlink(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_fsync(
	bhv_desc_t	*bdp, 
	int		flag, 
	struct cred	*cr,
	off_t		start,
	off_t		stop)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_inactive(
	bhv_desc_t	*bdp, 
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_fid(
	bhv_desc_t	*bdp,
	struct fid	**fidpp)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_fid2(
	bhv_desc_t	*bdp,
	struct fid	*fidp)
{
    return ENOSYS;
}

/* ARGSUSED */
static void umfs_rwlock(
	bhv_desc_t	*bdp, 
	vrwlock_t	write_lock)
{
    return;
}

/* ARGSUSED */
static void umfs_rwunlock(
	bhv_desc_t	*bdp, 
	vrwlock_t	write_lock)

{
    return;
}

/* ARGSUSED */
static int  umfs_seek(
	bhv_desc_t	*bdp, 
	off_t		ooff, 
	off_t		*noffp)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_cmp(
	bhv_desc_t	*bdp, 
	vnode_t		*vp2)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_frlock(
	bhv_desc_t	*bdp,
	int		cmd,
	struct flock	*lfp,
	int		flag,
	off_t		offset,
	vrwlock_t	vrwlock,
	cred_t		*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_realvp(
	bhv_desc_t	*bdp,
	struct vnode	**vpp)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_bmap(
	bhv_desc_t	*bdp,
	off_t		offset,
	ssize_t		count,
	int		rw,
	struct cred	*cr,
	struct bmapval	*iex,
	int		*nex)
{
    return ENOSYS;
}

/* ARGSUSED */
static void umfs_strategy(
	bhv_desc_t	*bdp, 
	struct buf	*bp)

{
    return;
}

/* ARGSUSED */
static int  umfs_map(
	bhv_desc_t	*bdp,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	u_int		flags,
	struct cred	*cr,
	vnode_t		**nvp)
{
    return ENODEV;
}

/* ARGSUSED */
static int  umfs_addmap(
	bhv_desc_t	*bdp,
	vaddmap_t	op,
	vhandl_t	*vt,
	pgno_t		*pgno,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	struct cred	*cred)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_delmap(
	bhv_desc_t	*bdp,
	vhandl_t	*vt,
	size_t		len,
	struct cred	*cred)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_poll(
	bhv_desc_t	*bdp,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead **phpp,
	unsigned int	*genp)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_dump(
	bhv_desc_t	*bdp,
	caddr_t		addr,
	daddr_t		daddr,
	u_int		flag)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_pathconf(
	bhv_desc_t	*bdp,
	int		cmd,
	long		*valp,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_allocstore(
	bhv_desc_t	*bdp,
	off_t		off,
	size_t		len,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_fcntl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flags,
	off_t		offset,
	cred_t		*cr,
	rval_t		*rvp)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_reclaim(
	bhv_desc_t	*bdp,
	int		flag)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_attr_get(
	bhv_desc_t	*bdp,
	char		*name,
	char		*value,
	int		*valuelenp,
	int		flags,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_attr_set(
	bhv_desc_t	*bdp,
	char		*name,
	char		*value,
	int		valuelen,
	int		flags,
	struct cred	*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_attr_remove(
	bhv_desc_t	*bdp,
	char		*name,
	int		flags,
	struct cred	*c)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_attr_list(
	bhv_desc_t		*bdp,
	char			*buffer,
	int			bufsize,
	int			flags,
	attrlist_cursor_kern_t	*cursor,
	struct cred		*cr)
{
    return ENOSYS;
}

/* ARGSUSED */
static int  umfs_cover(
	bhv_desc_t	*bdp,
	struct mounta	*uap,
	char		*attrs,
	struct cred	*cred)
{
    return ENOSYS;
}

/* ARGSUSED */
static void umfs_link_removed(
	bhv_desc_t	*bdp,
	vnode_t		*dvp,
	int		linkzero)
{
    return;
}

/* ARGSUSED */
static void umfs_vnode_change(
	bhv_desc_t	*bdp,
	vchange_t	cmd,
	__psint_t	va)
{
    return;
}

vnodeops_t umfs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	umfs_open,
	umfs_close,
	umfs_read,
	umfs_write,
	umfs_ioctl,
	umfs_setfl,
	umfs_getattr,
	umfs_setattr,
	umfs_access,
	umfs_lookup,
	umfs_create,
	umfs_remove,
	umfs_link,
	umfs_rename,
	umfs_mkdir,
	umfs_rmdir,
	umfs_readdir,
	umfs_symlink,
	umfs_readlink,
	umfs_fsync,
	umfs_inactive,
	umfs_fid,
	umfs_fid2,
	umfs_rwlock,
	umfs_rwunlock,
	umfs_seek,
	umfs_cmp,
	umfs_frlock,
	umfs_realvp,
	umfs_bmap,
	umfs_strategy,
	umfs_map,
	umfs_addmap,
	umfs_delmap,
	umfs_poll,
	umfs_dump,
	umfs_pathconf,
	umfs_allocstore,
	umfs_fcntl,
	umfs_reclaim,
	umfs_attr_get,
	umfs_attr_set,
	umfs_attr_remove,
	umfs_attr_list,
	umfs_cover,
	umfs_link_removed,
	umfs_vnode_change,
	(vop_ptossvp_t)fs_noval,
	(vop_pflushinvalvp_t)fs_noval,
	(vop_pflushvp_t)fs_nosys,
	(vop_pinvalfree_t)fs_noval,
	(vop_sethole_t)fs_noval,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};
