/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

/*#ident	"@(#)uts-comm:fs/vfs.c	1.18"*/
#ident	"$Revision: 1.136 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dnlc.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/fstyp.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/vnode_private.h>
#include <sys/buf.h>
#include <sys/quota.h>
#include <sys/xlate.h>
#include <sys/ddi.h>
#include <string.h>
#include <sys/kopt.h>
#include <sys/imon.h>
#if CELL_IRIX
#include <fs/cfs/cfs.h>
#endif
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
#include <sys/sat.h>
#include <ksys/vhost.h>


/*
 * VFS global data.
 */
vfs_t 		rootvfs_data;	/* root vfs */
vfs_t 		*rootvfs = &rootvfs_data; 	/* pointer to root vfs; */
                                                /*   head of VFS list. */
lock_t 		vfslock;	/* spinlock protecting rootvfs and vfs_flag */
lock_t 		vfsswlock;	/* protects vfssw table itself */
zone_t		*pn_zone;	/* pathname zone */

sema_t 		synclock;	/* sync in progress; initialized in sinit() */
                                
/*
 * Externs, etc.
 */
extern int efs_statdevvp(struct statvfs *, struct vnode *);
extern int xfs_statdevvp(struct statvfs *, struct vnode *);
extern void	vn_init(void);

void statvfs_socket(statvfs_t *svfs);
static int cquotasync(void);
static int statfs_to_irix5(struct statfs *, struct irix5_statfs *);
#if _MIPS_SIM == _ABI64
static void statfs_to_irix5_n32(struct statfs *, struct irix5_n32_statfs *);
#endif


/*
 * System calls.
 */

/*
 * "struct mounta" defined in sys/vfs.h.
 */

/* ARGSUSED */
int
mount(struct mounta *uap, rval_t *rvp)
{
	return (cmount(uap, rvp, NULL, UIO_USERSPACE));
}

/*
 * cmount is the code which is common to mount(2) and eag_mount(2).
 * eag_mount is found in os/eag.c
 */

/* ARGSUSED */
int
cmount(struct mounta *uap, rval_t *rvp, char *attrs, enum uio_seg seg)
{
	vnode_t *vp;
	register int error;

#if CELL_IRIX
	if (cellid() != CELL_GOLDEN) {
		/* Bug #504915 */
		return (EINVAL);
	}
#endif

	/*
	 * Resolve second path name (mount point).
	 */
	_SAT_PN_BOOK(SAT_MOUNT, curuthread);
	if (error = lookupname(uap->dir, seg, FOLLOW, NULLVPP, &vp, NULL))
		return error;

	/* Since lookupname returns top vp, check if it is a root */
	if (vp->v_flag & VROOT && !(vp->v_flag & VROOTMOUNTOK)) {
		VN_RELE(vp);
		_SAT_ACCESS(SAT_MOUNT, EBUSY);
		return EBUSY;
	}

	/*
	 * Cover this vnode.
	 */
	VOP_COVER(vp, uap, attrs, get_current_cred(), error);
	return(error);
}

/*
 * unmount system call
 */
struct umounta {
	char	*pathp;
};

int
umount(struct umounta *uap)
{
	vnode_t *fsrootvp;
	register struct vfs *vfsp;
	register int error;
	dev_t dev;

	/*
	 * Lookup user-supplied name.
	 */
	_SAT_PN_BOOK(SAT_MOUNT, curuthread);
	if (error = lookupname(uap->pathp, UIO_USERSPACE, FOLLOW,
			       NULLVPP, &fsrootvp, NULL))
		return error;
	/*
	 * Find the vfs to be unmounted.  The caller may have specified
	 * either the directory mount point (preferred) or else (for a
	 * disk-based file system) the block device which was mounted.
	 * Check to see which it is; if it's the device, search the VFS
	 * list to find the associated vfs entry.
         *
         * In each case in which we have a vfs, we must also have a 
         * reference on the the root vnode for that vfs.  This reference
         * prevents the vfs from going away during the window between
         * finding the vfs and locking it, preparatory to actually
         * effecting the unmount in the VFS_DOUNMOUNT routine.  The
         * vnode reference is handed off to the VFS_DOUNMOUNT routine,
         * whose job it is to release the reference once it is no longer
         * neded to keep the vfs around.
	 */
	if (fsrootvp->v_flag & VROOT) {
		vfsp = fsrootvp->v_vfsp;
		IMON_EVENT(fsrootvp, get_current_cred(), IMON_CONTENT);
	} else if (fsrootvp->v_type == VBLK) {
                vnode_t	*devvp = fsrootvp;

		vfsp = vfs_busydev(devvp->v_rdev, VFS_FSTYPE_ANY);
                error = 0;
		if (vfsp) {
			VFS_ROOT(vfsp, &fsrootvp, error);
			vfs_unbusy(vfsp);
		}
		VN_RELE(devvp);
		if (error)
		  	vfsp = NULL;
	} else {
		vfsp = NULL;
		VN_RELE(fsrootvp);
	}
	if (vfsp == NULL) {
		_SAT_ACCESS(SAT_MOUNT, EINVAL);
		return EINVAL;
	}
	dev = vfsp->vfs_dev;
	/*
	 * Perform the unmount.
	 */
	VFS_DOUNMOUNT(vfsp, 0, fsrootvp, get_current_cred(), error);
	if (!error) {
	    vfs_deallocate(vfsp);
	    /*
	     *  Now that we know the dismount succeeded, send imon
	     *  events for all files that were on the filesystem.
	     */

	    IMON_BROADCAST(dev, IMON_UNMOUNT);
	}

	_SAT_ACCESS(SAT_MOUNT, error);
	return error;
}

/*
 * Unmount by vfs_dev and not by pathname.
 * Currently handles only NFS2 and NFS3 filesystem types.
 */
int umount_dev(dev_t dev)
{
	register struct vfs *vfsp;
	int error;
	vnode_t *fsrootvp;
	extern int nfs3_fstype;
	extern int nfs_fstype;
	
	if (!dev) 
		return EINVAL;

	_SAT_PN_BOOK(SAT_MOUNT, curuthread);
	/* 
	 * Look up user supplied device number. If it is not NFS
	 * return EINVAL for now.
	 */
	vfsp = vfs_busydev(dev,nfs3_fstype);
	if (!vfsp) {
		vfsp = vfs_busydev(dev,nfs_fstype);
		if (!vfsp) {
			extern int autofs_fstype;
			vfsp = vfs_busydev(dev,autofs_fstype);
			if (!vfsp) {
				_SAT_ACCESS(SAT_MOUNT, EINVAL);
				return EINVAL;
			}
		}
	}
	VFS_ROOT(vfsp, &fsrootvp, error);
	vfs_unbusy(vfsp);
	
	if (error)
		return error;
	/*
	 * Perform the unmount.
	 */
	VFS_DOUNMOUNT(vfsp, 0, fsrootvp, get_current_cred(), error);
	if (!error) {
	    vfs_deallocate(vfsp);
	    /*
	     *  Now that we know the dismount succeeded, send imon
	     *  events for all files that were on the filesystem.
	     */

	    IMON_BROADCAST(dev, IMON_UNMOUNT);
	}

	_SAT_ACCESS(SAT_MOUNT, error);
	return error;
}

/*
 * Update every mounted file system.
 */
void
vfs_syncall(int flags)
{
	struct vfs *vfsp, *nextvfsp;
	register int s;
	/*REFERENCED*/
	int unused;

#ifdef _SHAREII
	/*
	 * ShareII normally keeps an in-core cache of lnodes, which are
	 * periodically synced to disk when other cached file data is.
	 * It is done here in case the Share update generates queued but
	 * unwritten filesystem data, which will be written below.
	 */
	SHR_FLUSH(0);
#endif /* _SHAREII */

	s = vfs_spinlock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = nextvfsp) {
		if (vfsp->vfs_fstype == 0) {
			nextvfsp = vfsp->vfs_next;
			continue;
		}
		if (vfsp->vfs_flag & VFS_MLOCK) {
			nextvfsp = vfsp->vfs_next;
			continue;
		}
		ASSERT(!(vfsp->vfs_flag & VFS_OFFLINE));
		if (flags & SYNC_CLOSE && !(vfsp->vfs_flag & VFS_LOCAL)) {
			nextvfsp = vfsp->vfs_next;
			continue;
		}
		if (flags & SYNC_PDFLUSH && (vfsp->vfs_dcount <= 0)) {
			nextvfsp = vfsp->vfs_next;
			continue;
		}
		ASSERT(vfsp->vfs_busycnt >= 0);
		/*
		 * skip filesystems that don't have a behavior
		 * pointer set.  This should only happen if
		 * a filesystem is unmountable and/or hasn't
		 * finished mounting yet when vfs_syncall runs.
		 */
		if (vfsp->vfs_fbhv != NULL) {
			vfsp->vfs_busycnt++;
			vfs_spinunlock(s);

			VFS_SYNC(vfsp, flags, get_current_cred(), unused);

			s = vfs_spinlock();
			nextvfsp = vfsp->vfs_next;

			ASSERT(vfsp->vfs_busycnt > 0);
			if (--vfsp->vfs_busycnt == 0)
				vfs_unbusy_wakeup(vfsp);
		} else
			nextvfsp = vfsp->vfs_next;
	}
	vfs_spinunlock(s);

	/*
	 * SYNC_CLOSE is only called by uadmin(SHUTDOWN) -- each
	 * file system should take care if its own blocks (don't
	 * want a hung remote file system to stop a shutdown).
	 * Neither is this necessary when called by {pd}flush.
	 */
	if (flags & SYNC_DELWRI)
		bflush(NODEV);
}

int
sync()
{
	if (!cpsema(&synclock))
		return 0;
	VHOST_SYNC(SYNC_FSDATA|SYNC_ATTR|SYNC_DELWRI|SYNC_NOWAIT);
	vsema(&synclock);
	return 0;
}

/*
 * Get file system statistics (statvfs and fstatvfs).
 */

struct statvfsa {
	char		*fname;
	statvfs_t	*sbp;
};

static int statvfsx(char *, statvfs_t *, int);

/* ARGSUSED */
int
statvfs(struct statvfsa *uap, rval_t *rvp)
{
	return statvfsx(uap->fname, uap->sbp, STATVFS_VERS_32);
}

/* ARGSUSED */
int
statvfs64(struct statvfsa *uap, rval_t *rvp)
{
	return statvfsx(uap->fname, uap->sbp, STATVFS_VERS_64);
}

static int cstatvfs(vfs_t *, statvfs_t *, vnode_t *, int, int);

/*
 * Common routine for statvfs and statvfs64.
 */
static int
statvfsx(char *fname, statvfs_t *sbp, int vers)
{
	int	error;
	vnode_t	*vp;

	_SAT_PN_BOOK(SAT_FILE_ATTR_READ, curuthread);
	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL))
		return error;
	error = cstatvfs(vp->v_vfsp, sbp, vp, vers, 0);
	VN_RELE(vp);
	_SAT_ACCESS(SAT_FILE_ATTR_READ, error);
	return error;
}

struct fstatvfsa {
	sysarg_t	fdes;
	statvfs_t	*sbp;
};

static int fstatvfsx(sysarg_t, statvfs_t *, int);

/* ARGSUSED */
int
fstatvfs(struct fstatvfsa *uap, rval_t *rvp)
{
	return fstatvfsx(uap->fdes, uap->sbp, STATVFS_VERS_32);
}

/* ARGSUSED */
int
fstatvfs64(struct fstatvfsa *uap, rval_t *rvp)
{
	return fstatvfsx(uap->fdes, uap->sbp, STATVFS_VERS_64);
}

/*
 * Common routine for fstatvfs and fstatvfs64.
 */
static int
fstatvfsx(sysarg_t fdes, statvfs_t *sbp, int vers)
{
	vfile_t		*vfp;
	vnode_t		*vp;
	int		error;

	if (error = getf(fdes, &vfp))
		return error;

	if (VF_IS_VSOCK(vfp))
		error = cstatvfs(NULL, sbp, NULL, vers, 1);
	else {
		vp = VF_TO_VNODE(vfp);
		error = cstatvfs(vp->v_vfsp, sbp, vp, vers, 0);
	}

	_SAT_FD_READ(fdes, error);
	return error;
}

/*
 * Translate kernel statvfs struct to O32 statvfs structure.
 */
static int
statvfs32_to_irix5(statvfs_t *ds, irix5_statvfs_t *ubp)
{
	irix5_statvfs_t	ub;

	ub.f_bsize = ds->f_bsize;
	ub.f_frsize = ds->f_frsize;
	ub.f_blocks = ds->f_blocks;
	ub.f_bfree = ds->f_bfree;
	ub.f_bavail = ds->f_bavail;
	/*
	 * NFS sets these to -1 even though they're unsigned!
	 */
	ub.f_files = ds->f_files;
	ub.f_ffree = ds->f_ffree;
	ub.f_favail = ds->f_favail;
	/*
	 * This can only fail in a 64-bit kernel, currently,
	 * but we'll make the check assuming that it could also
	 * fail in an N32 kernel if one existed.
	 */
	if (ub.f_blocks != ds->f_blocks || ub.f_bfree != ds->f_bfree ||
	    ub.f_bavail != ds->f_bavail)
		return EOVERFLOW;
	if ((ub.f_files != ds->f_files && ds->f_files != (fsfilcnt_t)-1) ||
	    (ub.f_ffree != ds->f_ffree && ds->f_ffree != (fsfilcnt_t)-1) ||
	    (ub.f_favail != ds->f_favail && ds->f_favail != (fsfilcnt_t)-1))
		return EOVERFLOW;
	ub.f_fsid = ds->f_fsid;
	bcopy(ds->f_basetype, ub.f_basetype, sizeof(ub.f_basetype));
	ub.f_flag = ds->f_flag;
	ub.f_namemax = ds->f_namemax;
	bcopy(ds->f_fstr, ub.f_fstr, sizeof(ub.f_fstr));
	if (copyout(&ub, ubp, sizeof(ub)))
		return EFAULT;
	return 0;
}

/*
 * Translate kernel statvfs struct to O32 statvfs64 structure.
 */
static int
statvfs64_to_irix5(statvfs_t *ds, irix5_statvfs64_t *ubp)
{
	irix5_statvfs64_t	ub;

	ub.f_bsize = ds->f_bsize;
	ub.f_frsize = ds->f_frsize;
	ub.f_blocks = ds->f_blocks;
	ub.f_bfree = ds->f_bfree;
	ub.f_bavail = ds->f_bavail;
	ub.f_files = ds->f_files;
	ub.f_ffree = ds->f_ffree;
	ub.f_favail = ds->f_favail;
	ub.f_fsid = ds->f_fsid;
	bcopy(ds->f_basetype, ub.f_basetype, sizeof(ub.f_basetype));
	ub.f_flag = ds->f_flag;
	ub.f_namemax = ds->f_namemax;
	bcopy(ds->f_fstr, ub.f_fstr, sizeof(ub.f_fstr));
	if (copyout(&ub, ubp, sizeof(ub)))
		return EFAULT;
	return 0;
}

/*
 * Common routine for statvfs* and fstatvfs*.
 */
static int
cstatvfs(vfs_t *vfsp, statvfs_t *ubp, vnode_t *vp, int vers, int is_socket)
{
	int		abi;
	statvfs_t	ds;
	int		error;

	bzero(&ds, sizeof(ds));
	if (is_socket) {
		statvfs_socket(&ds);
	} else {
		VFS_STATVFS(vfsp, &ds, vp, error);
		if (error)
			return error;
	}

	abi = get_current_abi();
	if (ABI_IS_IRIX5(abi)) {
		if (vers == STATVFS_VERS_32)
			error = statvfs32_to_irix5(&ds, (irix5_statvfs_t *)ubp);
		else if (vers == STATVFS_VERS_64)
			error = statvfs64_to_irix5(&ds,
						   (irix5_statvfs64_t *)ubp);
	} else if (ABI_IS_IRIX5_N32(abi)) {
#if _MIPS_SIM != _ABI64
		if (copyout(&ds, ubp, sizeof(*ubp)))
			error = EFAULT;
#else
		ASSERT(vers == STATVFS_VERS_32);
		/*
		 * This works because an irix5_statvfs64_t is the same
		 * as an irix5_n32_statvfs_t.
		 */
		error = statvfs64_to_irix5(&ds, (irix5_statvfs64_t *)ubp);
#endif
	}
#if _MIPS_SIM == _ABI64
	else {
		ASSERT(vers == STATVFS_VERS_32);
		if (copyout(&ds, ubp, sizeof(*ubp)))
			error = EFAULT;
	}
#endif
	return error;
}

/*
 * statfs(2) and fstatfs(2) have been replaced by statvfs(2) and
 * fstatvfs(2) and will be removed from the system in a near-future
 * release.
 */
struct statfsa {
	char	*fname;
	struct	statfs *sbp;
	sysarg_t	len;
	sysarg_t	fstyp;
};

static int cstatfs(struct vfs *, struct statfs *, int, int, struct vnode *,
		   int);

/* ARGSUSED */
int
statfs(struct statfsa *uap, rval_t *rvp)
{
	vnode_t *vp;
	register int error;

	_SAT_PN_BOOK(SAT_FILE_ATTR_READ, curuthread);
	if (error = lookupname(uap->fname, UIO_USERSPACE,
	  FOLLOW, NULLVPP, &vp, NULL))
		return error;
	if (uap->fstyp != 0)
		error = cstatfs(NULL, uap->sbp, uap->len, uap->fstyp, vp, 0);
	else
		error = cstatfs(vp->v_vfsp, uap->sbp, uap->len, 0, vp, 0);
	VN_RELE(vp);
	_SAT_ACCESS(SAT_FILE_ATTR_READ, error);
	return error;
}

struct fstatfsa {
	sysarg_t	fdes;
	struct	statfs *sbp;
	sysarg_t	len;
	sysarg_t	fstyp;
};

/* ARGSUSED */
int
fstatfs(struct fstatfsa *uap, rval_t *rvp)
{
	vfile_t 	*vfp;
	vnode_t 	*vp;
	int 		error;

	if (error = getf(uap->fdes, &vfp))
		return error;

	if (VF_IS_VSOCK(vfp))
		error = cstatfs(NULL, uap->sbp, uap->len, uap->fstyp, NULL, 1);
	else {
		vp = VF_TO_VNODE(vfp);
		error = cstatfs(uap->fstyp ? NULL : vp->v_vfsp, uap->sbp,
				uap->len, uap->fstyp, vp, 0);
	}
	_SAT_FD_READ(uap->fdes, error);
	return error;
}

extern int efs_fstype;
extern int xfs_fstype;
/*
 * Common routine for fstatfs and statfs.
 */
static int
cstatfs(struct vfs *vfsp, struct statfs *sbp, int len, int fstyp,
	struct vnode *vp, int is_socket)
{
	struct statfs sfs;
	struct statvfs svfs;
	register int error, i;
	char *cp, *cp2;
	register struct vfssw *vswp;
	int abi = get_current_abi();

	if (len < 0 || len > sizeof(struct statfs))
		return EINVAL;

	if (is_socket) {
		statvfs_socket(&svfs);
	} else if (vfsp == NULL) {
		/*
		 * hack for backward compat with statfs when they pass in 
		 * an fstype.  This only works for efs and xfs.
		 */
		if (fstyp == xfs_fstype) {
			if (error = xfs_statdevvp(&svfs, vp))
				return error;
		} else if (fstyp == efs_fstype) {
			if (error = efs_statdevvp(&svfs, vp))
				return error;
		} else
			return EINVAL;
	} else {
		VFS_STATVFS(vfsp, &svfs, vp, error);
		if (error)
			return error;
	}

	/*
	 * Map statvfs fields into the old statfs structure.
	 */
	bzero(&sfs, sizeof(sfs));
	sfs.f_bsize = svfs.f_bsize;
	if ((svfs.f_frsize != svfs.f_bsize) && (svfs.f_frsize != 0)) {
		sfs.f_frsize = svfs.f_frsize;
		/*
		 * Convert f_blocks and f_bfree to be in terms of f_frsize.
		 */
		ASSERT(svfs.f_bsize > svfs.f_frsize);
		sfs.f_blocks = svfs.f_blocks * (svfs.f_bsize / svfs.f_frsize);
		sfs.f_bfree = svfs.f_bfree * (svfs.f_bsize / svfs.f_frsize);
	} else {
		sfs.f_blocks = svfs.f_blocks;
		sfs.f_bfree = svfs.f_bfree;
	}
	sfs.f_files = svfs.f_files;
	sfs.f_ffree = svfs.f_ffree;

	cp = svfs.f_fstr;
	cp2 = sfs.f_fname;
	i = 0;
	while (i++ < sizeof(sfs.f_fname))
		if (*cp != '\0')
			*cp2++ = *cp++;
		else
			*cp2++ = '\0';
	while (*cp != '\0'
	  && i++ < (sizeof(svfs.f_fstr) - sizeof(sfs.f_fpack)))
		cp++;
	cp++;
	cp2 = sfs.f_fpack;
	i = 0;
	while (i++ < sizeof(sfs.f_fpack))
		if (*cp != '\0')
			*cp2++ = *cp++;
		else
			*cp2++ = '\0';
	if ((vswp = vfs_getvfssw(svfs.f_basetype)) == NULL)
		sfs.f_fstyp = 0;
	else
		sfs.f_fstyp = vswp - vfssw;

#if _MIPS_SIM == _ABI64
	if (ABI_IS_IRIX5_64(abi))
		return copyout(&sfs, sbp, len);
#endif
	if (ABI_IS_IRIX5_N32(abi)) {
#if _MIPS_SIM == _ABI64
		struct irix5_n32_statfs t;
		statfs_to_irix5_n32(&sfs, &t);
		return copyout(&t, sbp, len);
#else
		return copyout(&sfs, sbp, len);
#endif
	}
	if (ABI_IS_IRIX5(abi)) {
		struct irix5_statfs t;
		if (error = statfs_to_irix5(&sfs, &t))
			return error;
		return copyout(&t, sbp, len);
	}
	ASSERT(0);
	/*NOTREACHED*/
}

/*
 * System call to map fstype numbers to names, and vice versa.
 */
struct fsa {
	sysarg_t opcode;
};

struct	fsinda {
	sysarg_t opcode;
	char *fsname;
};

struct fstypa {
	sysarg_t opcode;
	sysarg_t index;
	char *cbuf;
};

static int sysfsind(struct fsinda *, rval_t *);
static int sysfstyp(struct fstypa *, rval_t *);

int
sysfs(struct fsa *uap, rval_t *rvp)
{
	int error;

	switch (uap->opcode) {
	case GETFSIND:
		error = sysfsind((struct fsinda *) uap, rvp);
		break;
	case GETFSTYP:
		error = sysfstyp((struct fstypa *) uap, rvp);
		break;
	case GETNFSTYP:
		/*
		 * Return number of fstypes configured in the system.
		 */
		rvp->r_val1 = nfstype - 1;
		error = 0;
		break;
	default:
		error = EINVAL;
	}

	return error;
}

static int
sysfsind(struct fsinda *uap, rval_t *rvp)
{
	/*
	 * Translate fs identifier to an index into the vfssw structure.
	 */
	register struct vfssw *vswp;
	char fsbuf[FSTYPSZ];
	int error;
	size_t len = 0;
	int s;

	error = copyinstr(uap->fsname, fsbuf, sizeof fsbuf, &len);
	if (error) {
		if (error == ENAMETOOLONG)	/* XXX */
			return EINVAL;		/* XXX */
		return error;
	}
	if (len == 1)			/* includes null byte */
		return EINVAL;
	/*
	 * Search the vfssw table for the fs identifier
	 * and return the index.
	 */
	if (strcmp(fsbuf, "dbg") == 0)
		strcpy(fsbuf, "proc");
	s = vfssw_spinlock();
	for (vswp = &vfssw[1]; vswp < &vfssw[nfstype]; vswp++) {
		if (vswp->vsw_name == NULL || vswp->vsw_name[0] == '\0')
			continue;
		if (strcmp(vswp->vsw_name, fsbuf) == 0) {
			rvp->r_val1 = vswp - vfssw;
			vfssw_spinunlock(s);
			return 0;
		}
	}
	vfssw_spinunlock(s);
	return EINVAL;
}

/* ARGSUSED */
static int
sysfstyp(struct fstypa *uap, rval_t *rvp)
{
	/*
	 * Translate fstype index into an fs identifier.
	 */
	register char *src;
	register int index;
	register struct vfssw *vswp;
	char *osrc;

	if ((index = uap->index) <= 0 || index >= nfstype)
		return EINVAL;
	vswp = &vfssw[index];
	src = vswp->vsw_name ? vswp->vsw_name : "";
	for (osrc = src; *src++; )
		;
	if (copyout(osrc, uap->cbuf, src - osrc))
		return EFAULT;
	return 0;
}

/*
 * External routines.
 */

/*
 * vfs_mountroot is called by main() to mount the root filesystem.
 */
void
vfs_mountroot(void)
{
	register int error;
	struct vfssw *vsw;
	vfs_t *rp = &rootvfs_data;
	int s;
	extern int diskless;

	/*
	 * There are 3 root file system styles keyed from the "diskless="
	 * kernel arg (cracked in devinit()):
	 *
	 * 0	not diskless, root is local rw disk
	 * 1	normal diskless, root is private rw NFS (clinst, etc)
	 * 2	bootable read-only distribution media, root is ro CD-ROM or
	 *	ro NFS as determined by tapedevice
	 */

	if (diskless == 2) {	/* bootable CD-ROM or remote (NFS) */
		char *td = kopt_find("tapedevice");

		if (!td)
			cmn_err(CE_PANIC,
			"vfs_mountroot: diskless=2, but no tapedevice= arg");
		
		if (strncmp(td, "dksc", 4) == 0)	/* CD-ROM */
			vsw = vfs_getvfssw("efs");
		else if (strncmp(td, "bootp", 5) == 0)	/* remote CD-ROM */
			vsw = vfs_getvfssw("nfs");
		else if (strncmp(td, "network", 7) == 0) /* ARCS */
			vsw = vfs_getvfssw("nfs");
		else {
			cmn_err(CE_PANIC,
			    "vfs_mountroot diskless=2 unknown tapedevice: %s\n",
			    td);
		}
		rp->vfs_flag |= VFS_RDONLY;
		VFS_INIT(rp);
		VFSOPS_ROOTINIT(vsw->vsw_vfsops, rp, error);
	} else if (vsw = vfs_getvfssw(rootfstype)) {
		/*
		 * "rootfstype" will ordinarily have been initialized to
		 * contain the name of the fstype of the root file system
		 * (this is user-configurable). Use the name to find the
		 * vfssw table entry.
		 */

		VFS_INIT(rp);
		VFSOPS_ROOTINIT(vsw->vsw_vfsops, rp, error);
	} else {
		/*
		 * If rootfstype is not set or not found, step through
		 * all the fstypes until we find one that will accept
		 * a mountroot() request.
		 */
		VFS_INIT(rp);
		s = vfssw_spinlock();
		for (vsw = &vfssw[1]; vsw < &vfssw[nfstype]; vsw++) {
			if (vsw->vsw_vfsops) {
				/* releasing and re-aquiring the lock here   */
				/* is safe because mountroot is only ever    */
				/* called under circumstances which prohibit */
				/* change to the vfssw anyway. The locks     */
				/* could be removed from here entirely; but  */
				/* since the world may change someday, this  */
				/* seemed safer (and faster than changing it */
				/* into a mutex)                             */
				vfssw_spinunlock(s);
				VFSOPS_ROOTINIT(vsw->vsw_vfsops, rp, error);
				s = vfssw_spinlock();
				if (error == 0)
					break;
			}
		}
		vfssw_spinunlock(s);
	}

	if (error == EROFS) {	/* won't happen if diskless=2 */
		cmn_err(CE_PANIC,
			"vfs_mountroot: root on %V is read-only\n",
			rootdev);
	}
	
	if (error) {
		delay(15*HZ); /* delay so any earlier (probably driver) messages
			* can be seen before screen is cleared (when on gfx) */
		cmn_err(CE_PANIC, "vfs_mountroot: no root found\n");
	}

	strcpy(rootfstype, vsw->vsw_name);

	/*
	 * Get vnode for '/'.  Set up rootdir to point to it.
	 * These are used by lookuppn() so that it knows where to start from
	 * ('/' or '.').
	 */
	VFS_ROOT(rp, &rootdir, error);
	if (error)
		cmn_err(CE_PANIC, "vfs_mountroot: no root vnode");

	/*
	 * Set the default MAC attributtes, if appropriate
	 */
	_MAC_MOUNTROOT(rootvfs);
}

/*
 * vfs_add is called by mount to add the new vfs into the vfs list and to
 * cover the mounted-on vnode.  The mounted-on vnode's v_vfsmountedhere link
 * to vfsp must have already been set, protected by VN_LOCK and VN_UNLOCK.
 * The vfs must already have been locked by the caller.
 *
 * coveredvp is zero if this is the root.
 */
void
vfs_add(vnode_t *coveredvp, struct vfs *vfsp, int mflag)
{
	struct vfs *vfsq;
	int s;

	s = vfs_spinlock();
	if (coveredvp != NULL) {
		if (vfsq = rootvfs->vfs_next)
			vfsq->vfs_prevp = &vfsp->vfs_next;
		rootvfs->vfs_next = vfsp;
		vfsp->vfs_next = vfsq;
		vfsp->vfs_prevp = &rootvfs->vfs_next;
	} else {
		/*
		 * This is the root of the whole world.
		 * Some filesystems might already be 'on the list'
		 * since they aren't really mounted and add themselves
		 * at init time.
		 * This essentially replaces the static 'root'
		 * that rootvfs is initialized to
		 */
		if (vfsp != rootvfs) {
			vfsp->vfs_next = rootvfs->vfs_next;
			vfsp->vfs_prevp = NULL;
			if (vfsq = rootvfs->vfs_next)
				vfsq->vfs_prevp = &vfsp->vfs_next;
			
			rootvfs = vfsp;
		}
	}
	vfsp->vfs_vnodecovered = coveredvp;

	if (mflag != VFS_FLAGS_PRESET) {
		vfsp->vfs_flag &= ~(VFS_RDONLY|VFS_NOSUID|VFS_NODEV|VFS_GRPID);

		if (mflag & MS_RDONLY)
			vfsp->vfs_flag |= VFS_RDONLY;

		if (mflag & MS_NOSUID)
			vfsp->vfs_flag |= VFS_NOSUID;

		if (mflag & MS_NODEV)
			vfsp->vfs_flag |= VFS_NODEV;

		if (mflag & MS_GRPID)
			vfsp->vfs_flag |= VFS_GRPID;

		if (mflag & MS_DEFXATTR)
			vfsp->vfs_flag |= VFS_DEFXATTR;

		if (mflag & MS_DOXATTR)
			vfsp->vfs_flag |= VFS_DOXATTR;
	}

	vfs_spinunlock(s);
}

void
vfs_simpleadd(struct vfs *vfsp)
{
	struct vfs *vfsq;
	int s;

	s = vfs_spinlock();
	if (vfsq = rootvfs->vfs_next)
		vfsq->vfs_prevp = &vfsp->vfs_next;
	rootvfs->vfs_next = vfsp;
	vfsp->vfs_next = vfsq;
	vfsp->vfs_prevp = &rootvfs->vfs_next;
	vfs_spinunlock(s);
}

/*
 * Remove a vfs from the vfs list, and destroy pointers to it.
 * Called by umount after it determines that an unmount is legal but
 * before it destroys the vfs.
 */
void
vfs_remove(struct vfs *vfsp)
{
	register vnode_t *vp;
	register struct vfs *vfsq;
	int s;

	/*
	 * Can't unmount root.  Should never happen because fs will
	 * be busy.
	 */
	ASSERT(vfsp != rootvfs);

	/*
	 * Clear covered vnode pointer.  No thread can traverse vfsp's
	 * mount point while vfsp is locked.
	 */
	vp = vfsp->vfs_vnodecovered;
	s = VN_LOCK(vp);
	vp->v_vfsmountedhere = NULL;
	VN_UNLOCK(vp, s);

	/*
	 * Unlink vfsp from the rootvfs list.
	 */
	s = vfs_spinlock();
	if (vfsq = vfsp->vfs_next)
		vfsq->vfs_prevp = vfsp->vfs_prevp;
	*vfsp->vfs_prevp = vfsq;
	vfs_spinunlock(s);

	/*
	 * Release lock and wakeup anybody waiting.
	 * Turns off VFS_OFFLINE bit.
	 */
	vfs_unlock(vfsp);
}

/*
 * Allocate and initialize a new vfs
 */
vfs_t *
vfs_allocate(void)
{
	vfs_t	*vfsp;

        vfsp = kmem_zalloc(sizeof(vfs_t), KM_SLEEP);
	ASSERT(vfsp);
	VFS_INIT(vfsp);
	return (vfsp);
}

/*
 * Allocate and initialize a new dummy vfs.  Dummy vfs's are those
 * set up during initialization to represent vnodes which are not 
 * really part of the file name space.  They have no covered vnode
 * and usually are not part of the vfs list.
 */
vfs_t *
vfs_allocate_dummy(
	int	fstype,
	dev_t   dev)
{
        vfs_t	*vfsp; 

	vfsp = kmem_zalloc(sizeof(struct vfs), KM_NOSLEEP);
	if (vfsp == NULL)
	        return (NULL);

	VFS_INIT(vfsp);
	vfsp->vfs_flag = VFS_CELLULAR;
	vfsp->vfs_bsize = 1024;
	vfsp->vfs_fstype = fstype;
	vfsp->vfs_fsid.val[0] = dev;
	vfsp->vfs_fsid.val[1] = fstype;
	vfsp->vfs_dev = dev;
	vfsp->vfs_bcount = 0;
#if CELL_IRIX
        cfs_dummy_vfs(vfsp);
#endif
        return (vfsp);
}

void
vfs_deallocate(vfs_t *vfsp)
{
        VFS_FREE(vfsp);
	if (vfsp->vfs_mac != NULL)
		kern_free(vfsp->vfs_mac);
        kmem_free(vfsp, sizeof(vfs_t));
}

/*
 * Implement a simplified multi-access, single-update protocol for vfs.
 *
 * Only one update-lock (mount/unmount) can be held at a time; if another
 * process holds the vfs stucture with update capabilities, or is waiting
 * to acquire update rights, other update calls fail.
 *
 * Multiple accessors are allowed: vfs_busycnt tracks the number of
 * concurrent accesses.  Update permission sleeps until the last access
 * has finished, but leaves the VFS_MWANT flag to hold (if called via
 * vfs_busy) or reject (called via vfs_busydev or vfs_lock) subsequent
 * accesses/updates.
 * Note that traverse understands the vfs locking model and waits for
 * any update to complete, and retries the mount-point traversal.
 *
 * Accessors include: vfs_syncall, traverse, VFS_STATVFS, and quota checking.
 */
static int
vfs_lock_flags(struct vfs *vfsp, int flags)
{
	register int error;
	int s;

	s = vfs_spinlock();
	if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
		vfs_spinunlock(s);
		return EBUSY;
	}

	while (vfsp->vfs_busycnt) {
		ASSERT(vfsp->vfs_busycnt > 0);
		ASSERT(!(vfsp->vfs_flag & (VFS_MLOCK|VFS_OFFLINE)));
		vfsp->vfs_flag |= VFS_MWAIT|VFS_MWANT;
		error = vfsp_waitsig(vfsp, PVFS, s);
		s = vfs_spinlock();
		if (error) {
			ASSERT(vfsp->vfs_flag & VFS_MWANT);
			vfsp->vfs_flag &= ~VFS_MWANT;
			if (vfsp->vfs_flag & VFS_MWAIT) {
				vfsp->vfs_flag &= ~VFS_MWAIT;
				sv_broadcast(&vfsp->vfs_wait);
			}
			vfs_spinunlock(s);
			return EINTR;
		}
		ASSERT(vfsp->vfs_flag & VFS_MWANT);
		vfsp->vfs_flag &= ~VFS_MWANT;
	}

	ASSERT((vfsp->vfs_flag & (VFS_MLOCK|VFS_OFFLINE)) != VFS_OFFLINE);
	if (vfsp->vfs_flag & VFS_MLOCK) {
		error = EBUSY;
	} else {
		vfsp->vfs_flag |= VFS_MLOCK|flags;
		error = 0;
	}
	vfs_spinunlock(s);
	return error;
}

/*
 * Lock a filesystem to prevent access to it while mounting.
 * Returns error if already locked.
 */
int
vfs_lock(struct vfs *vfsp)
{
	return (vfs_lock_flags(vfsp, 0));
}

/*
 * Lock a filesystem and mark it offline,
 * to prevent access to it while unmounting.
 * Returns error if already locked.
 */
int
vfs_lock_offline(struct vfs *vfsp)
{
	return (vfs_lock_flags(vfsp, VFS_OFFLINE));
}

/*
 * Unlock a locked filesystem.
 */
void
vfs_unlock(register struct vfs *vfsp)
{
	int s = vfs_spinlock();
	ASSERT((vfsp->vfs_flag & (VFS_MWANT|VFS_MLOCK)) == VFS_MLOCK);
	vfsp->vfs_flag &= ~(VFS_MLOCK|VFS_OFFLINE);

	/*
	 * Wake accessors (traverse() or vfs_syncall())
	 * waiting for the lock to clear.
	 */
	if (vfsp->vfs_flag & VFS_MWAIT) {
		vfsp->vfs_flag &= ~VFS_MWAIT;
		sv_broadcast(&vfsp->vfs_wait);
	}
	vfs_spinunlock(s);
}

/*
 * Get access permission for vfsp.
 */
int
vfs_busy(struct vfs *vfsp)
{
	int s = vfs_spinlock();
	ASSERT((vfsp->vfs_flag & (VFS_MLOCK|VFS_OFFLINE)) != VFS_OFFLINE);
	while (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
		ASSERT(vfsp->vfs_flag & VFS_MWANT || vfsp->vfs_busycnt == 0);
		if (vfsp->vfs_flag & VFS_OFFLINE) {
			vfs_spinunlock(s);
			return EBUSY;
		}
		vfsp->vfs_flag |= VFS_MWAIT;
		if (vfsp_waitsig(vfsp, PVFS, s))
			return EINTR;
		s = vfs_spinlock();
	}

	ASSERT(vfsp->vfs_busycnt >= 0);
	vfsp->vfs_busycnt++;
	vfs_spinunlock(s);

	return 0;
}

/*
 * The following routines are only needed for CELL configurations but 
 * there is nothing iherently cell-specific about them.  They have
 * been placed under #if CELL, to avoid their incorporation in 6.5
 * even though they are checked into kudzu.  After the tree-split
 * the #if should go away.
 *
 * Paranoia strikes deep.
 */
#if CELL
int
vfs_busy_trywait_vnlock(
	vfs_t		*vfsp,
        vnode_t         *vp,
	int             s,
        int             *errorp)
{
	nested_spinlock(&vfslock);
	NESTED_VN_UNLOCK(vp);
	if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
	        ASSERT(vfsp->vfs_flag & VFS_MWANT || vfsp->vfs_busycnt == 0); 
		vfsp->vfs_flag |= VFS_MWAIT;

		if (errorp)
		        *errorp = sv_wait_sig(&vfsp->vfs_wait, PZERO, 
					      &vfslock, s);
		else
               		sv_wait(&vfsp->vfs_wait, PZERO, &vfslock, s);
		return 0;
	} 
        else {
	        ASSERT(vfsp->vfs_busycnt >= 0);
		vfsp->vfs_busycnt++;
		mutex_spinunlock(&vfslock, s);
		return 1;
	}
}

int 
vfs_busy_trywait_mrlock(
	vfs_t		*vfsp,
        mrlock_t        *mrp,
	int             *errorp)
{
        int 		s;

	s = vfs_spinlock();
	mrunlock(mrp);
	if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
	        ASSERT(vfsp->vfs_flag & VFS_MWANT || vfsp->vfs_busycnt == 0); 
		vfsp->vfs_flag |= VFS_MWAIT;

		if (errorp)
		        *errorp = sv_wait_sig(&vfsp->vfs_wait, PZERO, 
					      &vfslock, s);
		else
               		sv_wait(&vfsp->vfs_wait, PZERO, &vfslock, s);
		return 0;
	} 
        else {
	        ASSERT(vfsp->vfs_busycnt >= 0);
		vfsp->vfs_busycnt++;
		vfs_spinunlock(s);
		return 1;
	}
}

#endif /* CELL */
/*
 * Given a <dev, filesystem-type> pair, return the vfs-entry for it.
 * If the type sent in is VFS_FSTYPE_ANY, then this'll only try to
 * match the device number.
 *
 * This extra parameter was necessary since duplicate vfs entries with
 * the same vfs_dev are possible because of lofs.
 */
struct vfs *
vfs_busydev(dev_t dev, int type)
{
	int s;
	struct vfs *vfsp;
again:
	s = vfs_spinlock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next) {
		if (vfsp->vfs_dev == dev &&
		    (type == VFS_FSTYPE_ANY || type == vfsp->vfs_fstype)) {

			if (vfsp->vfs_flag & VFS_OFFLINE) {
				vfsp = NULL;
				break;
			}
			if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
				ASSERT(vfsp->vfs_flag & VFS_MWANT ||
				       vfsp->vfs_busycnt == 0);
				vfsp->vfs_flag |= VFS_MWAIT;
				vfsp_wait(vfsp, PZERO, s);
				goto again;
			}

			ASSERT(vfsp->vfs_busycnt >= 0);
			vfsp->vfs_busycnt++;
			break;
		}
	}
	vfs_spinunlock(s);
	return vfsp;
}

void
vfs_unbusy(struct vfs *vfsp)
{
	int s = vfs_spinlock();
	ASSERT(!(vfsp->vfs_flag & (VFS_MLOCK|VFS_OFFLINE)));
	ASSERT(vfsp->vfs_busycnt > 0);
	if (--vfsp->vfs_busycnt == 0)
		vfs_unbusy_wakeup(vfsp);
	vfs_spinunlock(s);
}

void
vfs_unbusy_wakeup(register struct vfs *vfsp)
{
	/*
	 * If there's an updater (mount/unmount) waiting for the vfs lock,
	 * wake up only it.  Updater should be the first on the sema queue.
	 *
	 * Otherwise, wake all accessors (traverse() or vfs_syncall())
	 * waiting for the lock to clear.
	 */
	if (vfsp->vfs_flag & VFS_MWANT) {
		sv_signal(&vfsp->vfs_wait);
	} else
	if (vfsp->vfs_flag & VFS_MWAIT) {
		vfsp->vfs_flag &= ~VFS_MWAIT;
		sv_broadcast(&vfsp->vfs_wait);
	}
}

struct vfs *
getvfs(fsid_t *fsid)
{
	register struct vfs *vfsp;

	int s = vfs_spinlock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next) {
		if (vfsp->vfs_fsid.val[0] == fsid->val[0]
		    && vfsp->vfs_fsid.val[1] == fsid->val[1]
		    && !(vfsp->vfs_flag & VFS_OFFLINE)) {
			break;
		}
	}
	vfs_spinunlock(s);
	return vfsp;
}

/*
 * Search the vfs list for a specified device.  Returns a pointer to it
 * or NULL if no suitable entry is found.
 *
 * Any calls to this routine (as opposed to vfs_busydev) should
 * considered extremely suspicious.  Once the vfs_spinunlock is done,
 * there is likely to be nothing guaranteeing that the vfs pointer
 * returned continues to point to a vfs.  There are numerous bugs
 * which would quickly become intolerable if the frequency of unmount
 * was to rise above its typically low level.
 */
struct vfs *
vfs_devsearch(dev_t dev, int fstype)
{
	register struct vfs *vfsp;

	int s = vfs_spinlock();
	vfsp = vfs_devsearch_nolock(dev, fstype);
	vfs_spinunlock(s);
	return vfsp;
}

/*
 * Same as vfs_devsearch without locking the list.
 * Useful for debugging code, but put it here anyway.
 */
struct vfs *
vfs_devsearch_nolock(dev_t dev, int fstype)
{
	register struct vfs *vfsp;

	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if ((vfsp->vfs_dev == dev) &&
		    (fstype == VFS_FSTYPE_ANY || fstype == vfsp->vfs_fstype))
			break;
	return vfsp;
}

/*
 * Find a vfssw entry given a file system type name.
 */
struct vfssw *
vfs_getvfssw(char *type)
{
	register int i;
	struct vfssw *vswp;
	int s;

	if (type == NULL || *type == '\0')
		return NULL;
	if (strcmp(type, "dbg") == 0)
		type = "proc";

	s = vfssw_spinlock();
	for (i = 1; i < nfstype; i++) {
		vswp = &vfssw[i];
		if (vswp->vsw_name == NULL || vswp->vsw_name[0] == '\0')
			continue;
		if (strcmp(type, vswp->vsw_name) == 0) {
		    vfssw_spinunlock(s);
		    return vswp;
		}
	}
	vfssw_spinunlock(s);
	return NULL;
}

/*
 * Map VFS flags to statvfs flags.  These shouldn't really be separate
 * flags at all.
 */
u_long
vf_to_stf(u_long vf)
{
	u_long stf = 0;

	if (vf & VFS_RDONLY)
		stf |= ST_RDONLY;
	if (vf & VFS_NOSUID)
		stf |= ST_NOSUID;
	if (vf & VFS_NOTRUNC)
		stf |= ST_NOTRUNC;
	if (vf & VFS_DMI)
		stf |= ST_DMI;
	if (vf & VFS_NODEV)
		stf |= ST_NODEV;
	if (vf & VFS_GRPID)
		stf |= ST_GRPID;
	if (vf & VFS_LOCAL)
		stf |= ST_LOCAL;

	return stf;
}

/*
 * Entries for (illegal) fstype 0.
 */

static int vfsstray();

vfsops_t vfs_strayops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_INVALID),
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	(int (*)(bhv_desc_t *, int, struct cred *))vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray
};

static int
vfsstray()
{
	cmn_err(CE_PANIC, "stray vfs operation");
	/* NOTREACHED */
}

void
vfsinit()
{
	register int i;
	extern int vfsmax;

	/*
	 * fstype 0 is (arbitrarily) invalid.
	 */
	vfssw[0].vsw_vfsops = &vfs_strayops;
	vfssw[0].vsw_name = "BADVFS";

	/*
	 * Set nfstype to size of vfssw table to permit searching
	 * of loaded file systems.  Searchers should make sure that 
	 * an entry is valid:  vsw_name not '\0'
	 */
	nfstype = vfsmax;

	/*
	 * Initialize vfs stuff.
	 */
	spinlock_init(&vfslock, "vfslock");
	spinlock_init(&vfsswlock, "vfsswlock");
	pn_zone = kmem_zone_init(MAXPATHLEN, "pathnames");
	ASSERT(pn_zone);

	/*
	 * Initialize vnode stuff.
	 */
	vn_init();

	/*
	 * Initialize the name cache.
	 */
	dnlc_init();

	/*
	 * Call all the init routines.
	 */
	for (i = 1; i < nfstype; i++)
		if (vfssw[i].vsw_init)
			(*vfssw[i].vsw_init)(&vfssw[i], i);
	
}

static int
statfs_to_irix5(struct statfs *source, struct irix5_statfs *target)
{
	target->f_fstyp = source->f_fstyp;
	target->f_bsize = source->f_bsize;
	target->f_frsize = source->f_frsize;
	target->f_blocks = source->f_blocks;
	target->f_bfree = source->f_bfree;
	target->f_files = source->f_files;
	target->f_ffree = source->f_ffree;
	if ((target->f_blocks != source->f_blocks 
		&& target->f_blocks != (app32_long_t)(-1)) ||
	    (target->f_bfree  != source->f_bfree  
		&& target->f_bfree != (app32_long_t)(-1)) ||
	    (target->f_files  != source->f_files  
		&& target->f_files != (app32_long_t)(-1)) ||
	    (target->f_ffree  != source->f_ffree
		&& target->f_ffree != (app32_long_t)(-1))) 
		return EOVERFLOW;
	bcopy(&source->f_fname, &target->f_fname, sizeof(target->f_fname));
	bcopy(&source->f_fpack, &target->f_fpack, sizeof(target->f_fpack));
	return 0;
}

#if _MIPS_SIM == _ABI64
static void
statfs_to_irix5_n32(struct statfs *source, struct irix5_n32_statfs *target)
{
	target->f_fstyp = source->f_fstyp;
	target->f_bsize = source->f_bsize;
	target->f_frsize = source->f_frsize;
	target->f_blocks = source->f_blocks;
	target->f_bfree = source->f_bfree;
	target->f_files = source->f_files;
	target->f_ffree = source->f_ffree;
	bcopy(&source->f_fname, &target->f_fname, sizeof(target->f_fname));
	bcopy(&source->f_fpack, &target->f_fpack, sizeof(target->f_fpack));
}
#endif

/*
 * Called by fs dependent VFS_MOUNT code to link the VFS base file system 
 * dependent behavior with the VFS virtual object.
 */
void
vfs_insertbhv(
	vfs_t *vfsp, 
	bhv_desc_t *bdp, 
	vfsops_t *vfsops, 	
	void *mount)
{
	/* 
	 * Initialize behavior desc with ops and data and then
	 * attach it to the vfs.
	 */
	bhv_desc_init(bdp, mount, vfsp, vfsops);
	bhv_insert_initial(&vfsp->vfs_bh, bdp);
}

void
vfs_mounthwgfs()
{
	struct mounta mounta;
	rval_t rval;
	int error;
	extern int hwgfstype;
	
	mounta.spec = "/hw";
	mounta.dir = "/hw";
	mounta.flags = 6;
	mounta.fstype = (char *)(__psint_t)hwgfstype;
	mounta.dataptr = NULL;
	mounta.datalen = 0;
	error = cmount(&mounta, &rval, NULL, UIO_SYSSPACE);
	if (error)
		printf("Unable to mount hwgfs error = %d \n", error);
}

/*
 * Fill in a statvfs structure for an open socket.  This is an attempt 
 * to return something semi-sane, given that the open socket isn't part
 * of a vfs.
 */
void 
statvfs_socket(statvfs_t *svfs)
{
	svfs->f_bsize = NBPC;
	svfs->f_frsize = NBPC;
	svfs->f_files = 0;
	svfs->f_ffree = svfs->f_favail = -1;
	svfs->f_fsid = NODEV;
	strcpy(svfs->f_basetype, "");
	svfs->f_flag = 0;
	svfs->f_namemax = MAXNAMELEN - 1;
}


/*
 * Sys call to allow users to find out
 * their current position wrt quota's
 * and to allow super users to alter it.
 */

struct quotactla {
	sysarg_t	cmd;
	caddr_t		fdev;
	sysarg_t	uid;
	caddr_t		addr;
};

/*
 * Iterate over all filesystems, and call quotactl(Q_SYNC) on them.
 */
int
cquotasync()
{
	struct vfs 	*vfsp, *nextvfsp;
	int 		s;
	int 		error;

	s = vfs_spinlock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = nextvfsp) {
		if (vfsp->vfs_fstype == 0) {
			nextvfsp = vfsp->vfs_next;
			continue;
		}
		if (vfsp->vfs_flag & VFS_MLOCK) {
			nextvfsp = vfsp->vfs_next;
			continue;
		}
		ASSERT(!(vfsp->vfs_flag & VFS_OFFLINE));
		ASSERT(vfsp->vfs_busycnt >= 0);
		vfsp->vfs_busycnt++;
		vfs_spinunlock(s);
		
		VFS_QUOTACTL(vfsp, Q_SYNC, 0, NULL, error);
		error = error;	/* silence a compiler message */

		s = vfs_spinlock();
		nextvfsp = vfsp->vfs_next;

		ASSERT(vfsp->vfs_busycnt > 0);
		if (--vfsp->vfs_busycnt == 0)
			vfs_unbusy_wakeup(vfsp);
	}
	vfs_spinunlock(s);
	return (0);
}


int
quotactl(struct quotactla *uap)
{
	int 		error;
	struct vnode 	*vp;
	struct vfs 	*vfsp;

	/*
	 * Q_SYNC is old-baggage and is done here only
	 * because the man page advertises that it syncs
	 * quota info in all filesystems, if invoked without
	 * a fsdev argument.
	 */
	if (uap->fdev == NULL) {
		if (uap->cmd == Q_SYNC)
			return cquotasync();
		else
			return EINVAL;
	}

	if (uap->uid < 0)
		return EINVAL;

	if (error = lookupname(uap->fdev, UIO_USERSPACE, FOLLOW, NULLVPP, 
			       &vp, NULL))
		return (error);
	if (vp->v_type != VBLK) {
		VN_RELE(vp);
		return (ENOTBLK);
	}

	/*
	 * xfs and efs filesystems don't share the same device,
	 * but lofs does.
	 */
	vfsp = vfs_busydev(vp->v_rdev, xfs_fstype);
	if (vfsp == NULL)
		vfsp = vfs_busydev(vp->v_rdev, efs_fstype);
		
	VN_RELE(vp);
	if (vfsp == NULL)
		return(ENODEV);

	VFS_QUOTACTL(vfsp, uap->cmd, uap->uid, uap->addr, error);

	vfs_unbusy(vfsp);
	return (error);
	
}

int
vfs_force_pinned(dev)
dev_t	dev;
{
	struct vfs 	*vfsp;
	int		ret_val = 0;

	vfsp = vfs_busydev(dev, xfs_fstype);
	if (vfsp != NULL) {
		VFS_FORCE_PINNED(vfsp, ret_val)
		vfs_unbusy(vfsp);
	}
	return ret_val;
}
