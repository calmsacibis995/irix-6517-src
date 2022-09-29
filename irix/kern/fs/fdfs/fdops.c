/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/fdfs/fdops.c	1.7"*/
#ident	"$Revision: 1.72 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dnlc.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/pda.h>
#include <sys/resource.h>
#include <sys/sema.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/pvfs.h>
#include <sys/ddi.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/atomic_ops.h>
#include <ksys/vproc.h>
#include <string.h>
#include <fs/fdfs/fdfs_private.h>

#define	min(a,b)	((a) <= (b) ? (a) : (b))
#define	max(a,b)	((a) >= (b) ? (a) : (b))
#define	round(r)	(((r)+sizeof(off_t)-1)&(~(sizeof(off_t)-1)))
#define	fdfstoi(n)	((n)+100)

#define	FDFSDIRSIZE 14
struct fdfsdirect {
	short	d_ino;
	char	d_name[FDFSDIRSIZE];
};

#define	FDFSROOTINO	2
#define	FDFSSDSIZE	sizeof(struct fdfsdirect)
#define	FDFSNSIZE		10

int		fdfstype = 0;
zone_t          *fdfszone;

static int	fdfsget(fdfs_node_t *, char *, struct vnode **);

extern vfsops_t	fdfsvfsops;

/* ARGSUSED */
static int
fdfsopen(
	bhv_desc_t *bdp,
	struct vnode **vpp,
	mode_t mode,
	struct cred *cr)
{
	if ((*vpp)->v_type != VDIR)
		VN_FLAGSET(*vpp, VDUP);
	return 0;
}

/* ARGSUSED */
static int
fdfsread(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr,
	struct flid *fl)
{
	vnode_t	*vp;

	/*
	 * If you want to read the directory, use getdents().
	 */
	vp = BHV_TO_VNODE(bdp);
	if (vp->v_type == VDIR) {
		return EISDIR;
	} else {
		return ENOSYS;
	}
}

/* ARGSUSED */
static int
fdfsgetattr(
	register bhv_desc_t *bdp,
	register struct vattr *vap,
	int flags,
	struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
        fdfs_node_t *fnp = FDFS_BHVTON(bdp);

	if (vp->v_type == VDIR) {
		vap->va_nlink = 2;
		vap->va_size = (fdt_nofiles() + 2) * FDFSSDSIZE;
		vap->va_mode = 0555;
		vap->va_nodeid = FDFSROOTINO;
	} else {
		vap->va_nlink = 1;
		vap->va_size = 0;
		vap->va_mode = 0666;
		vap->va_nodeid = fdfstoi(getminor(vp->v_rdev));
	}
	vap->va_type = vp->v_type;
	vap->va_rdev = vp->v_rdev;
	vap->va_blksize = 1024;
	vap->va_nblocks = 0;
	nanotime_syscall(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = fnp->node_mount->mount_info.info_dev;
	vap->va_vcode = 0;
	vap->va_xflags = 0;
	vap->va_extsize = 0;
	vap->va_nextents = 0;
	vap->va_anextents = 0;
	vap->va_projid = 0;
	vap->va_gencount = 0;
	return 0;
}

/* ARGSUSED */
static int
fdfsaccess(
	register bhv_desc_t *bdp,
	register int mode,
	register struct cred *cr)
{
	return 0;
}

/* ARGSUSED */
static int
fdfslookup(
	bhv_desc_t *pdp,
	register char *comp,
	struct vnode **vpp,
	struct pathname *pnp,
	int flags,
	struct vnode *rdir,
	struct cred *cr)
{
	vnode_t *dp = BHV_TO_VNODE(pdp);
        fdfs_node_t *fnp = FDFS_BHVTON(pdp);

	if (comp[0] == 0 || strcmp(comp, ".") == 0 || strcmp(comp, "..") == 0) {
		VN_HOLD(dp);
		*vpp = dp;
		return 0;
	}
	return fdfsget(fnp, comp, vpp);
}

/* ARGSUSED */
static int
fdfscreate(
	bhv_desc_t *pdvp,
	char *comp,
	struct vattr *vap,
	int flags,
	int mode,
	struct vnode **vpp,
	struct cred *cr)
{
        fdfs_node_t *fnp = FDFS_BHVTON(pdvp);

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}

	return fdfsget(fnp, comp, vpp);
}

/* ARGSUSED */
static int
fdfsreaddir(
	bhv_desc_t *bdp,
	register struct uio *uiop,
	struct cred *cr,
	int *eofp)
{
	vproc_t	*vpr;
	/* bp holds one dirent structure */
	void *bp[((round(sizeof(dirent64_t)-1+FDFSNSIZE+1)) / sizeof(void *)) + 1];
	
	int reclen, nentries;
	register int n;
	int oresid;
	off_t off;

	ino_t d_ino;
	char d_name[FDFSNSIZE+1];
	int (*dirent_func)(void *, ino_t, off_t, char *);
	int target_abi;
	struct rlimit rlim;

	target_abi = GETDENTS_ABI(get_current_abi(), uiop);
	switch (target_abi) {
	case ABI_IRIX5_64:
	case ABI_IRIX5_N32:
		dirent_func = fs_fmtdirent;
		break;

	case ABI_IRIX5:
		dirent_func = irix5_fs_fmtdirent;
		break;
	}

	if (uiop->uio_offset < 0 || uiop->uio_resid <= 0
	  || (uiop->uio_offset % FDFSSDSIZE) != 0)
		return ENOENT;

	oresid = uiop->uio_resid;
	vpr = VPROC_LOOKUP(current_pid());
	VPROC_GETRLIMIT(vpr, RLIMIT_NOFILE, &rlim);
	VPROC_RELE(vpr);
	nentries = min(fdt_nofiles(), rlim.rlim_cur);

	for (; uiop->uio_resid > 0; uiop->uio_offset = off + FDFSSDSIZE) {
		if ((off = uiop->uio_offset) == 0) {	/* "." */
			d_ino = FDFSROOTINO;
			d_name[0] = '.';
			d_name[1] = '\0';
		} else if (off == FDFSSDSIZE) {		/* ".." */
			d_ino = FDFSROOTINO;
			d_name[0] = '.';
			d_name[1] = '.';
			d_name[2] = '\0';
		} else {
			/*
			 * Return entries corresponding to the allowable
			 * number of file descriptors for this process.
			 */
			if ((n = (off-2*FDFSSDSIZE)/FDFSSDSIZE) >= nentries)
				break;
			d_ino = fdfstoi(n);
			numtos(n, d_name);
		}
		reclen = (*dirent_func)(bp, d_ino,
					uiop->uio_offset + FDFSSDSIZE, d_name);
		if (reclen > uiop->uio_resid) {
			/*
			 * Error if no entries have been returned yet.
			 */
			if (uiop->uio_resid == oresid)
				return EINVAL;
			break;
		}
		/*
		 * uiomove() updates both resid and offset by the same
		 * amount.  But we want offset to change in increments
		 * of FDFSSDSIZE, which is different from the number of bytes
		 * being returned to the user.  So we set uio_offset
		 * separately, ignoring what uiomove() does.
		 */
		if (uiomove((caddr_t) bp, reclen, UIO_READ, uiop))
			return EFAULT;
	}
	if (eofp)
		*eofp = ((uiop->uio_offset-2*FDFSSDSIZE)/FDFSSDSIZE >= nentries);
	return 0;
}

/* ARGSUSED */
static int
fdfsinactive(
	register bhv_desc_t *bdp,
	struct cred	*cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
        fdfs_node_t *fnp = FDFS_BHVTON(bdp);
        fdfs_mount_t *fmp = fnp->node_mount;

	if (vp->v_type != VDIR)
		atomicAddInt(&fmp->mount_refcnt, -1);
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
	kmem_zone_free(fdfszone, fnp);

	/* the inactive_teardown flag must have been set at vn_alloc time */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	return VN_INACTIVE_NOCACHE;
}

static int
fdfsget(
        fdfs_node_t *dfnp,
        register char *comp, 
        struct vnode **vpp)
{
	register int n = 0;
	register struct vnode *vp;
	bhv_desc_t *bdp;
        struct fdfs_mount *fmp = dfnp->node_mount;
	struct fdfs_node *fnp;
	extern vnodeops_t fdfsvnodeops;

	while (*comp) {
		if (*comp < '0' || *comp > '9')
			return ENOENT;
		n = 10 * n + *comp++ - '0';
	}
	vp = vn_alloc(bhvtovfs(&fmp->mount_bhv), VCHR, 
		      makedevice(fmp->mount_info.info_rdev, n));
	fnp = (fdfs_node_t *)kmem_zone_zalloc(fdfszone, 0);
        fnp->node_mount = fmp;
	bdp = FDFS_NTOBHV(fnp);
	bhv_desc_init(bdp, fnp, vp, &fdfsvnodeops); 
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), bdp);
	atomicAddInt(&fmp->mount_refcnt, 1);
	/*
	 * No locks needed because these vnodes are not cached when active.
 	 */
	VN_FLAGSET(vp, VINACTIVE_TEARDOWN);
	*vpp = vp;
	return 0;
}

/*ARGSUSED*/
static int
fdfsattrget(
	register bhv_desc_t *bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	int i;

	if ((i = _MAC_FDFS_ATTR_GET(bdp,name,value,valuelenp,flags,cred)) >= 0)
		return i;

	return ENOATTR;
}

vnodeops_t fdfsvnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	fdfsopen,
	(vop_close_t)fs_nosys,
	fdfsread,	/* read */
	(vop_write_t)fs_nosys,
	(vop_ioctl_t)fs_nosys,
	fs_setfl,	/* setfl */
	fdfsgetattr,
	(vop_setattr_t)fs_nosys,
	fdfsaccess,
	fdfslookup,
	fdfscreate,
	(vop_remove_t)fs_nosys,
	(vop_link_t)fs_nosys,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	fdfsreaddir,
	(vop_symlink_t)fs_nosys,
	(vop_readlink_t)fs_nosys,
	(vop_fsync_t)fs_nosys,
	fdfsinactive,
	(vop_fid_t)fs_nosys,
	(vop_fid2_t)fs_nosys,
	fs_rwlock,	/* rwlock */
	fs_rwunlock,	/* rwunlock */
	(vop_seek_t)fs_nosys,
	fs_cmp,
	(vop_frlock_t)fs_nosys,
	(vop_realvp_t)fs_nosys,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_noval,
	(vop_map_t)fs_nodev,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	(vop_poll_t)fs_nosys,
	(vop_dump_t)fs_nosys,
	fs_pathconf,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	(vop_attr_get_t)fdfsattrget,
	(vop_attr_set_t)fs_nosys,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	(vop_cover_t)fs_nosys,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
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


/* ARGSUSED */
void
fdfsinit(
	register struct vfssw *vswp,
	int fstype)
{
	ASSERT(fstype != 0);

	fdfstype = fstype;
        fdfszone = kmem_zone_init(sizeof(fdfs_node_t), "fdfszone");
}

/* ARGSUSED */
static int
fdfs_mount(
	struct vfs *vfsp,
	struct vnode *mvp,
	struct mounta *uap,
	char *attrs,
	struct cred *cr)
{
	bhv_desc_t *bhvm;
        fdfs_mount_t *fmp;
	dev_t dev; 
        int rdev;

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return EPERM;
	/*
	 * No locking needed for mvp because mvp state is protected by
	 * v_vfsmountedhere
	 */
	if (mvp->v_type != VDIR)
		return ENOTDIR;
	if (mvp->v_count > 1 || (mvp->v_flag & VROOT))
		return EBUSY;

	/*
	 * Assign unique "device" numbers (reported by stat(2)).
	 */
	dev = getudev();
	rdev = getudev();
	if (dev == -1 || rdev == -1) {
		cmn_err(CE_WARN, "fdfsinit: can't get unique device numbers");
		if (dev == -1)
			dev = 0;
		if (rdev == -1)
			rdev = 0;
	}

	/*
         * Set up the fdfs mount structure
         */
	fmp = (fdfs_mount_t *) kmem_alloc(sizeof(fdfs_mount_t), 0);
        bhvm = FDFS_MTOBHV(fmp);
	vfs_insertbhv(vfsp, bhvm, &fdfsvfsops, fmp);
	fmp->mount_info.info_dev = makedevice(dev, 0);
	fmp->mount_info.info_rdev = rdev;
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
        VFS_EXPINFO(vfsp, &fmp->mount_info, sizeof(fdfs_info_t));
	return 0;
}

/* ARGSUSED */
static int
fdfs_unmount(
	bhv_desc_t *bdp,
	int flags,
	struct cred *cr)
{
        fdfs_mount_t *fmp = FDFS_BHVTOM(bdp);
        struct vfs *vfsp = bhvtovfs(bdp);

	ASSERT((flags & (UNMOUNT_FINAL | UNMOUNT_CKONLY)) == 0);
	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return EPERM;

	/*
	 * VFS layer makes sure that no new references will be created in
	 * the file system being unmounted.
	 * What if fdfs is busy currently ??
	 * How do we keep track of vnodes without cache and FS_INACTIVE ?
	 * Assumption here is that the created vnode just represents another
	 * vnode in some other file system and its association with fdfs
	 * is not important. Use fdfsrefcnt anyway.
	 */
	if (fmp->mount_root->v_count > 1)
	        return EBUSY;
	else if (fmp->mount_refcnt != 0) 
	        return EBUSY;

	VN_RELE(fmp->mount_root);
	fmp->mount_root = NULL;
	VFS_REMOVEBHV(vfsp, bdp);
	return 0;
}

int
fdfs_root(
	bhv_desc_t *bdp,
	struct vnode **vpp)
{
        fdfs_mount_t *fmp = FDFS_BHVTOM(bdp);
	struct vnode *vp = fmp->mount_root;

	VN_HOLD(vp);
	*vpp = vp;
	return 0;
}


/* ARGSUSED */
int
fdfs_statvfs(
	bhv_desc_t *bdp,
	register struct statvfs *sp,
	struct vnode *vp)
{
	struct vfs *vfsp = bhvtovfs(bdp);
	struct rlimit rlim;
	vproc_t	*vpr;

	bzero((caddr_t)sp, sizeof(*sp));
	sp->f_bsize = 1024;
	sp->f_frsize = 1024;
	sp->f_blocks = 0;
	sp->f_bfree = 0;
	sp->f_bavail = 0;
	vpr = VPROC_LOOKUP(current_pid());
	VPROC_GETRLIMIT(vpr, RLIMIT_NOFILE, &rlim);
	VPROC_RELE(vpr);
	sp->f_files = min(fdt_nofiles(), rlim.rlim_cur) + 2;
	sp->f_ffree = 0;
	sp->f_favail = 0;
	sp->f_fsid = vfsp->vfs_dev;
	strcpy(sp->f_basetype, vfssw[fdfstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = FDFSNSIZE;
	strcpy(sp->f_fstr, "/dev/fd");
	strcpy(&sp->f_fstr[8], "/dev/fd");
	return 0;
}


void
fdfs_newroot(
        vfs_t *vfsp,
        fdfs_mount_t *fmp)
{
	struct vnode *vp;
	bhv_desc_t *bhvn;
        fdfs_node_t *fnp;

	vp = vn_alloc(vfsp, VDIR, 0);
	fnp = (fdfs_node_t *)kmem_zone_zalloc(fdfszone, 0);
	fnp->node_mount = fmp;
	bhvn = FDFS_NTOBHV(fnp);
	bhv_desc_init(bhvn, fnp, vp, &fdfsvnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), bhvn);
	VN_FLAGSET(vp, VROOT | VINACTIVE_TEARDOWN);
        fmp->mount_root = vp;
}         

vfsops_t fdfsvfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	fdfs_mount,
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_dounmount,
	fdfs_unmount,
	fdfs_root,
	fdfs_statvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
#if	CELL_IRIX
	fdfs_realvfsops,/* realvfsops */
	fdfs_import,	/* import */
#else	/* CELL_IRIX */
	fs_realvfsops,  /* realvfsops */
	fs_import,	/* import */
#endif	/* CELL_IRIX */
	fs_quotactl     /* quotactl */
};

