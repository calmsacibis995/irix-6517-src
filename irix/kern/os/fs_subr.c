/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.98 $"

/*
 * Generic vnode operations.
 */
#include <sys/types.h>
#include <ksys/as.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/pfdat.h>
#include <string.h>
#include <limits.h>

#include <sys/sat.h>		/* all these for fs_mount */
#include <sys/mount.h>
#include <sys/kmem.h>
#include <sys/dnlc.h>
#include <sys/imon.h>
#include <ksys/cell.h>

static struct vfsops *getvfsops(struct mounta *);


/*
 * Cover a vnode.  Implementation routine for VOP_COVER.
 */
/*ARGSUSED3*/
int
fs_cover(bhv_desc_t *bdp, struct mounta *uap, char *attrs, struct cred *cred)
{
	struct vnode *cvp = BHV_TO_VNODE(bdp);
	register struct vfs *vfsp;
	int remount, ovflags;
	int s;
	int error;
	struct vfsops *vfsops;

	/*
	 * If anyone is mounting on this vnode or has just completed
	 * a mount, then fail.  Otherwise mark the fact that we are
	 * in the middle of a mount on this vnode.
	 *
	 * While we would normally set the VMOUNTING flag using
	 * VN_FLAGSET(), we need to check that it is not set before
	 * setting it.  Grabbing the vnode lock explicitly ensures
	 * coordination with those using VN_FLAGSET() and VN_FLAGCLR().
	 */
	s = VN_LOCK(cvp);
	if (cvp->v_flag & VMOUNTING ||
	    ((cvp->v_flag & VROOT || cvp->v_vfsmountedhere != NULL) &&
	     !(cvp->v_flag & VROOTMOUNTOK))) { 
		VN_UNLOCK(cvp, s);
		VN_RELE(cvp);
		_SAT_ACCESS(SAT_MOUNT, EBUSY);
		return EBUSY;
	}
	cvp->v_flag |= VMOUNTING;
	VN_UNLOCK(cvp, s);
	
	/*
	 * figuring out the base file system vfs ops to use
	 */
	vfsops = getvfsops(uap);
	if (vfsops == NULL) {
		VN_FLAGCLR(cvp, VMOUNTING);
                VN_RELE(cvp);
                _SAT_ACCESS(SAT_MOUNT, EINVAL);
                return EINVAL;
	}


	if ((uap->flags & MS_DATA) == 0) {
		uap->dataptr = NULL;
		uap->datalen = 0;
	}

	/*
	 * If this is a remount we don't want to create a new VFS.
	 * Instead we pass the existing one with a remount flag.
	 */
	remount = (uap->flags & MS_REMOUNT);
	if (remount) {
		/*
		 * Confirm that the vfsp associated with the mount point
		 * has already been mounted on.
		 */
		if ((cvp->v_flag & VROOT) == 0) {
			VN_FLAGCLR(cvp, VMOUNTING);
			VN_RELE(cvp);
			_SAT_ACCESS(SAT_MOUNT, ENOENT);
			return ENOENT;
		}
		/*
		 * Disallow making file systems read-only.  Ignore other flags.
		 */
		if (uap->flags & MS_RDONLY) {
			VN_FLAGCLR(cvp, VMOUNTING);
			VN_RELE(cvp);
			_SAT_ACCESS(SAT_MOUNT, EINVAL);
			return EINVAL;
		}
		vfsp = cvp->v_vfsp;
		s = vfs_spinlock();
		ovflags = vfsp->vfs_flag;
		vfsp->vfs_flag |= VFS_REMOUNT;
		vfsp->vfs_flag &= ~VFS_RDONLY;
		_MAC_MOUNT(vfsp, attrs);
		vfs_spinunlock(s);
		uap->flags &= ~MS_RDONLY;
		ASSERT(vfsp->vfs_dcount == 0);
		vfsp->vfs_dcount = 0;	/* just in case */
		dnlc_purge_vp(cvp);

		ASSERT(cvp->v_vfsmountedhere == NULL);
        	VFS_MNTUPDATE(vfsp, cvp, uap, cred, error);
		if (error)
		{
                	ASSERT(cvp->v_vfsmountedhere == NULL);
                	VN_FLAGCLR(cvp, VMOUNTING);
                	goto fail;
        	}
		
	} else {
	        vfsp = vfs_allocate();
#if CELL_IRIX 
		/*
	 	 * execute base file system VFSOPS_REALVFSOPS to figure out
	 	 * whether the server for this file system is remote.
		 * If the server for this file system is from a different
		 * cell then the base VFSOPS_REALVFSOPS will change vfsops to
		 * cfs_vfsops.
		 */
		VFSOPS_REALVFSOPS(vfsops, vfsp, uap, &vfsops, error);
		if (error) {
			VN_FLAGCLR(cvp, VMOUNTING);
			goto fail;
		}
#endif
		/*
		 * Do this before calling VFSOPS_MOUNT()
		 */
		_MAC_MOUNT(vfsp, attrs);
		if (uap->flags & MS_RDONLY)
			vfsp->vfs_flag |= VFS_RDONLY;
		dnlc_purge_vp(cvp);

		ASSERT(cvp->v_vfsmountedhere == NULL);
		vfsp->vfs_opsver = 0;
		vfsp->vfs_opsflags = 0;
		if (vfsops->vfs_ops_magic == VFSOPS_MAGIC) {
			vfsp->vfs_opsver = vfsops->vfs_ops_version;
			vfsp->vfs_opsflags = vfsops->vfs_ops_flags;
		}
		VFSOPS_MOUNT(vfsops, vfsp, cvp, uap, attrs, cred, error);
		if (error) {
			ASSERT(cvp->v_vfsmountedhere == NULL);
			VN_FLAGCLR(cvp, VMOUNTING);
			goto fail;
		}
	}


	if (remount) {
		vfs_clrflag(vfsp, VFS_REMOUNT);
		s = VN_LOCK(cvp);
		cvp->v_vfsmountedhere = vfsp;
		cvp->v_flag &= ~VMOUNTING;
		VN_UNLOCK(cvp, s);
		VN_RELE(cvp);
	} else {
		vfs_add(cvp, vfsp, uap->flags);
		cvp->v_vfsp->vfs_nsubmounts++;
		s = VN_LOCK(cvp);
		cvp->v_vfsmountedhere = vfsp;
		cvp->v_flag &= ~VMOUNTING;
		VN_UNLOCK(cvp, s);
		IMON_EVENT(cvp, cred, IMON_CONTENT);
	}

	_SAT_ACCESS(SAT_MOUNT, 0);
	return 0;

fail:
	VN_RELE(cvp);
	if (remount)
		vfsp->vfs_flag = ovflags;
	else 
	        vfs_deallocate(vfsp);

	_SAT_ACCESS(SAT_MOUNT, error);
	return error;
}

/*
 * getvfsops(struct mounta *)
 * find vfsops from file system type specified by user
 * if that fails then assumes the fstype of the root file system.
 */
static struct vfsops *
getvfsops(struct mounta *uap)
{
	struct vfssw *vswp;
	struct vfsops *vfsops = NULL;

	/*
	 * Backward compatibility: require the user program to
	 * supply a flag indicating a new-style mount, otherwise
	 * assume the fstype of the root file system and zero
	 * values for dataptr and datalen.  MS_FSS indicates an
	 * SVR3 4-argument mount; MS_DATA is the preferred way
	 * and indicates a 6-argument mount.
	 */
	if (uap->flags & (MS_DATA|MS_FSS)) {
		__psunsigned_t fstype;
		size_t n;
		char fsname[FSTYPSZ];

		/*
		 * Even funnier: we want a user-supplied fstype name here,
		 * but for backward compatibility we have to accept a
		 * number if one is provided.  The heuristic used is to
		 * assume that a "pointer" with a numeric value of less
		 * than 256 is really an int.
		 */
		if ((fstype = (__psunsigned_t)uap->fstype) < 256) {
			if (fstype == 0 || fstype >= nfstype) 
				return NULL;
			vfsops = vfssw[fstype].vsw_vfsops;
		} else {
			if (copyinstr(uap->fstype, fsname,
					      sizeof fsname, &n)) {
				return NULL;
			}
			if ((vswp = vfs_getvfssw(fsname)) == NULL) {
				return NULL;
			}
			vfsops = vswp->vsw_vfsops;
		}
	} else {
		bhv_desc_t *bdp = bhv_base_unlocked(VFS_BHVHEAD(rootvfs));
		vfsops = (struct vfsops *)BHV_OPS(bdp);
	}
	ASSERT(vfsops);
	return(vfsops);
}

/*
 * Implementation for VFS_DOUNMOUNT.
 */
int
fs_dounmount(
	bhv_desc_t 	*bdp, 
        int 		flags, 
        vnode_t 	*rootvp, 
        cred_t 		*cr)
{
	struct vfs 	*vfsp = bhvtovfs(bdp);
	vnode_t 	*coveredvp;
	int 		error;
	extern void 	nfs_purge_vfsp(struct vfs*);

	/*
         * Wait for sync to finish and lock vfsp.  This also sets the
         * VFS_OFFLINE flag.  Once we do this we can give up reference
         * the root vnode which we hold to avoid the another unmount
         * ripping the vfs out from under us before we get to lock it.
         * The VFS_DOUNMOUNT calling convention is that the reference
         * on the rot vnode is released whether the call succeeds or 
         * fails.
	 */
	error = vfs_lock_offline(vfsp);	
	if (rootvp)
		VN_RELE(rootvp);
	if (error)
		return error;

	/*
	 * Get covered vnode after vfs_lock.
	 */
	coveredvp = vfsp->vfs_vnodecovered;

	/*
	 * Purge all dnlc entries for this vfs.
	 */
	(void) dnlc_purge_vfsp(vfsp, 0);

	nfs_purge_vfsp(vfsp);

	VFS_SYNC(vfsp, SYNC_ATTR|SYNC_DELWRI|SYNC_NOWAIT, cr, error);
	if (error == 0)
		VFS_UNMOUNT(vfsp, flags, cr, error);

	if (error) {
		vfs_unlock(vfsp);	/* clears VFS_OFFLINE flag, too */
	} else {
		--coveredvp->v_vfsp->vfs_nsubmounts;
		ASSERT(vfsp->vfs_nsubmounts == 0);
		vfs_remove(vfsp);
		IMON_EVENT(coveredvp, cr, IMON_CONTENT);
		VN_RELE(coveredvp);
	}
	return error;
}

/*
 * fs_realvfsop()
 * base file system vfs_realvfsops() uses this.
 */
#if CELL_IRIX

#if DEBUG
cell_t dev_cellid = 0;
#endif 

/*ARGSUSED*/
int
locate_device_cell(char *path, cell_t *devcell)
{
#if DEBUG
  	if (dev_cellid) {
	        *devcell = dev_cellid;
		return(0);
	}
#endif         
	*devcell = golden_cell;
	return(0);
}

/*
 * fs_realvfsops()
 * For xfs and efs, find the cell serving this device and let that be
 * the device server for this file system.
 * For nfs and other file systems, it's not clear that the algorithm will be
 * the same. That's why this needs to be a vfs op.
 */
int
fs_realvfsops(vfs_t *vfsp, struct mounta *uap, vfsops_t **vfsop)
{
	int		error;
	cell_t 		devcell;
	extern vfsops_t cfs_vfsops;

	error = locate_device_cell(uap->spec, &devcell);
	if (error)
		return(error);
	if (devcell != cellid()) {
		*vfsop = &cfs_vfsops;
		/* cfs vfs_mount() needs this information */
		vfsp->vfs_cell = devcell;	
	}
	return(0);
}
#else	/* !CELL_IRIX */
/* ARGSUSED */
int
fs_realvfsops(vfs_t *vfsp, struct mounta *uap, vfsops_t **vfsop)
{
	return(0);
}
#endif	/* CELL_IRIX */
	
/*
 * Stub for no-op vnode operations that return error status.
 */
int
fs_noerr()
{
	return 0;
}

/*
 * The associated operation is not supported by the file system.
 */
int
fs_nosys()
{
	return ENOSYS;
}

/*
 * For VOP's, like VOP_MAP, that wish to return ENODEV.
 */
int
fs_nodev()
{
	return ENODEV;
}

/*
 * Stub for inactive, strategy, and read/write lock/unlock.  Does nothing.
 */
/* ARGSUSED */
void
fs_noval()
{
}

/*
 * Compare two vnodes.
 */
int
fs_cmp(bhv_desc_t * bdp, vnode_t * vp2)
{
	vnode_t *vp1 = BHV_TO_VNODE(bdp);
	return vp1 == vp2;
}

/*
 * File and record locking.  Handles the three cases of:
 * - cleaning locks
 * - checking for locks
 * - getting/setting locks
 * 
 * NOTE:  this routine should only be called when the implementing
 * file system has locked its file system-dependent vnode.  Thus,
 * file systems should NOT call fs_frlock directly from their ops vectors.
 * The 'vrwlock' arg indicates the type of lock held on the file
 * system-dependent vnode.
 */
/* ARGSUSED */
int
fs_frlock(
	register bhv_desc_t *bdp,
	int cmd,
	struct flock *bfp,
	int flag,
	off_t offset,
	vrwlock_t vrwlock,
	cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int 	frcmd, error;

	/*
	 * Determine if we're:
	 * - cleaning locks, or
	 * - checking for locks, or
	 * - getting/setting locks.
	 *
	 * Note:  for both checking and getting/setting locks, the assumption
	 * is that the calling file system has locked its file system-dependent
	 * vnode.  Hence, INOFLCK is set in those cases.
	 */

	if (cmd == F_CLNLK) {
		cleanlocks(vp, bfp->l_pid, bfp->l_sysid);
		error = 0;

	} else if (cmd == F_CHKLK || cmd == F_CHKLKW) {
		/* 
		 * reclock assumes the type of INOFLCK is VRWLOCK_WRITE, 
		 * unless told otherwise.
		 */
		ASSERT(vrwlock == VRWLOCK_READ || vrwlock == VRWLOCK_WRITE);
		frcmd = INOFLCK;
		if (vrwlock == VRWLOCK_READ)
			frcmd |= INOFLCK_READ;	
		if (cmd == F_CHKLKW)
			frcmd |= SLPFLCK;
		error = reclock(vp, bfp, frcmd, flag, offset, cr);

	} else {
		switch (cmd) {
		case F_GETLK:
			frcmd = 0;
			break;

		case F_RGETLK:
			frcmd = RCMDLCK;
			break;

		case F_SETLK:
			frcmd = SETFLCK;
			break;

		case F_SETBSDLK:
			frcmd = SETBSDFLCK;
			break;

		case F_RSETLK:
			frcmd = SETFLCK|RCMDLCK;
			break;

		case F_SETLKW:
			frcmd = SETFLCK|SLPFLCK;
			break;

		case F_SETBSDLKW:
			frcmd = SETBSDFLCK|SLPFLCK;
			break;

		case F_RSETLKW:
			frcmd = SETFLCK|SLPFLCK|RCMDLCK;
			break;

		default:
			return EINVAL;
		}

		/* 
		 * reclock assumes the type of INOFLCK is VRWLOCK_WRITE, 
		 * unless told otherwise.
		 */
		ASSERT(vrwlock == VRWLOCK_READ || vrwlock == VRWLOCK_WRITE);
		frcmd |= INOFLCK;
		if (vrwlock == VRWLOCK_READ)
			frcmd |= INOFLCK_READ;	
		error = reclock(vp, bfp, frcmd, flag, offset, cr);
	}

	return error;
}

/*
 * Enforce record locking protocol on vp.
 * 
 * NOTE:  this routine should only be called when the implementing
 * file system has locked its file system-dependent vnode.
 */
int
fs_checklock(
	vnode_t 	*vp,
	int 		iomode,
	off_t 		offset,
	off_t 		len,
	int 		fmode,
	cred_t 		*cr,
	flid_t 		*fl,
	vrwlock_t	vrwlock)
{
	struct flock 	bf;
	int 		cmd, error;

	if (!fl)
		return 0;
	bf.l_type = (iomode & FWRITE) ? F_WRLCK : F_RDLCK;
	bf.l_whence = 0;
	bf.l_start = offset;
	bf.l_len = len;
	bf.l_pid = fl->fl_pid;
	bf.l_sysid = fl->fl_sysid;

	if (fmode & (FNDELAY|FNONBLOCK))
		cmd = F_CHKLK;
	else
		cmd = F_CHKLKW;
	    
	VOP_FRLOCK(vp, cmd, &bf, 0, offset, vrwlock, cr, error);
	if (!error && bf.l_type != F_UNLCK)
		error = EAGAIN;

	return error;
}

/*
 * Clean locks on vp.  Typically called at close time.
 */
void
fs_cleanlock(
	vnode_t 	*vp,
	flid_t 		*fl,
	cred_t		*cr)
{
	struct flock 	bf;
	/*REFERENCED*/
	int 		error;

	ASSERT(fl);
	bf.l_pid = fl->fl_pid;
	bf.l_sysid = fl->fl_sysid;

	VOP_FRLOCK(vp, F_CLNLK, &bf, 0, 0, VRWLOCK_NONE, cr, error);

	/*
	 * We ignore errors from lock cleaning.  They can't happen
	 * locally (the underlying cleanlocks() routine is guaranteed
	 * to succeed), so the case to be concerned about is NFS.
	 * But, for it we choose to consider an inability to clean remote
	 * locks to be a non-fatal condition and hence just ignore it rather
	 * than return an error to the close() system call.
	 */
}

/*
 * Allow any flags.
 */
/* ARGSUSED */
int
fs_setfl(bhv_desc_t *bdp, int oflags, int nflags, cred_t *cr)
{
	/* direct IO only happens on efs */
	if (nflags & FDIRECT)
		return EINVAL;

	return 0;
}

/*
 * Change state of vnode itself.
 * 
 * This routine may or may not require that the caller(s) prohibit 
 * simultaneous changes to a given piece of state.  This depends
 * on the particular 'cmd' - and individual commands should assert
 * appropriately if they so desire.
 */
void
fs_vnode_change(
        bhv_desc_t	*bdp, 
        vchange_t 	cmd, 
        __psint_t 	val)
{
	vnode_t 	*vp = BHV_TO_VNODE(bdp);

	switch (cmd) {
	case VCHANGE_FLAGS_FRLOCKS:
		ASSERT(val == 0 || val == 1);

		if (val == 1) {
			VN_FLAGSET(vp, VFRLOCKS);
		} else if (val == 0) {
			VN_FLAGCLR(vp, VFRLOCKS);
		}
		break;

	case VCHANGE_FLAGS_ENF_LOCKING:
		ASSERT(val == 0 || val == 1);

		/* Callers prevent simultaneous changes to ENF_LOCKING */
		if (val == 1) {
			ASSERT(!(vp->v_flag & VENF_LOCKING));
			VN_FLAGSET(vp, VENF_LOCKING);
		} else if (val == 0) {
			ASSERT(vp->v_flag & VENF_LOCKING);
			VN_FLAGCLR(vp, VENF_LOCKING);
		}
		break;

	default:
		break;
	}
}


/*
 * vnode pcache layer for vnode_tosspages.
 */
void
fs_tosspages(
        bhv_desc_t	*bdp,
	off_t		first,
	off_t		last,
	int		fiopt)
{
	vnode_t 	*vp = BHV_TO_VNODE(bdp);

	if (fiopt == FI_REMAPF_LOCKED)
		VN_FLAGSET(vp, VREMAPPING);

	vnode_tosspages(vp, first, last);

	if ((fiopt == FI_REMAPF || fiopt == FI_REMAPF_LOCKED) && VN_MAPPED(vp))
		remapf(vp, first, 0);

	if (fiopt == FI_REMAPF_LOCKED)
		VN_FLAGCLR(vp, VREMAPPING);
}


/*
 * vnode pcache layer for vnode_flushinval_pages.
 */
void
fs_flushinval_pages(
        bhv_desc_t	*bdp,
	off_t		first,
	off_t		last,
	int		fiopt)
{
	vnode_t 	*vp = BHV_TO_VNODE(bdp);

	if (fiopt == FI_REMAPF_LOCKED)
		VN_FLAGSET(vp, VREMAPPING);
	if ((fiopt == FI_REMAPF || fiopt == FI_REMAPF_LOCKED) && VN_MAPPED(vp))
		remapf(vp, first, 1);

	vnode_flushinval_pages(vp, first, last);

	if (fiopt == FI_REMAPF_LOCKED)
		VN_FLAGCLR(vp, VREMAPPING);
}



/*
 * vnode pcache layer for vnode_flush_pages.
 */
int
fs_flush_pages(
        bhv_desc_t	*bdp,
	off_t		first,
	off_t		last,
	uint64_t	flags,
	int		fiopt)
{
	vnode_t 	*vp = BHV_TO_VNODE(bdp);
	int		ret;

	if (fiopt == FI_REMAPF_LOCKED)
		VN_FLAGSET(vp, VREMAPPING);
	if ((fiopt == FI_REMAPF || fiopt == FI_REMAPF_LOCKED) && VN_MAPPED(vp))
		remapf(vp, first, 1);

	ret = vnode_flush_pages(BHV_TO_VNODE(bdp), first, last, flags);

	if (fiopt == FI_REMAPF_LOCKED)
		VN_FLAGCLR(vp, VREMAPPING);

	return(ret);
}


/*
 * vnode pcache layer for vnode_invalfree_pages.
 */
void
fs_invalfree_pages(
        bhv_desc_t	*bdp,
	off_t		filesize)
{
	vnode_t 	*vp = BHV_TO_VNODE(bdp);

	if (filesize > 0) {
		if (VN_MAPPED(vp))
			remapf(vp, 0, 1);
		vnode_invalfree_pages(BHV_TO_VNODE(bdp), filesize);
	}
}

/*
 * vnode pcache layer for vnode_pages_sethole.
 */
void
fs_pages_sethole(
        bhv_desc_t	*bdp,
	pfd_t		*pfd,
	int		cnt,
	int		doremap,
	off_t		remap_offset)
{
	vnode_t 	*vp = BHV_TO_VNODE(bdp);

	vnode_pages_sethole(vp, pfd, cnt);
	if (doremap && VN_MAPPED(vp))
		remapf(vp, remap_offset, 1);
}


/*
 * Return the answer requested to poll() for non-device files.
 * Only POLLIN, POLLRDNORM, and POLLOUT are recognized.
 */
/* ARGSUSED */
int
fs_poll(bhv_desc_t *bdp,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	/* dopoll() initializes **phpp to NULL so no need to set it or *genp */
	int revents = 0;
	if (events & POLLIN)
		revents |= POLLIN;
	if (events & POLLRDNORM)
		revents |= POLLRDNORM;
	if (events & POLLOUT)
		revents |= POLLOUT;
	*reventsp = revents;
	return 0;
}

/*
 * POSIX pathconf() support.
 */
/* ARGSUSED */
int
fs_pathconf1(bhv_desc_t *bdp, int cmd, long *valp, struct cred *cr)
{
	register long val;
	register int error = 0;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct statvfs vfsbuf;

	switch (cmd) {

	case _PC_NAME_MAX:
		bzero(&vfsbuf, sizeof(vfsbuf));
		VFS_STATVFS(vp->v_vfsp, &vfsbuf, vp, error);
		if (error)
			break;
		val = vfsbuf.f_namemax;
		break;

	case _PC_NO_TRUNC:
		if (vp->v_vfsp->vfs_flag & VFS_NOTRUNC)
			val = 1;	/* NOTRUNC is enabled for vp */
		else
			val = 0;
		break;

	default:
	    	error= EINVAL;
		break;
	}
	if (error == 0)
		*valp = val;
	return error;
}

/* ARGSUSED */
int
fs_pathconf(bhv_desc_t *bdp, int cmd, long *valp, struct cred *cr)
{
	register long val;
	register int error = 0;

	switch (cmd) {

	case _PC_LINK_MAX:
		val = MAXLINK;
		break;

	case _PC_MAX_CANON:
		val = MAX_CANON;
		break;

	case _PC_MAX_INPUT:
		val = MAX_INPUT;
		break;

	case _PC_PATH_MAX:
		val = MAXPATHLEN;
		break;

	case _PC_PIPE_BUF:
		val = PIPE_BUF;
		break;

	case _PC_VDISABLE:
		val = _POSIX_VDISABLE;
		break;

	case _PC_CHOWN_RESTRICTED:
		if (restricted_chown)
			val = restricted_chown;
		else
			val = 0;
		break;
        case _PC_ASYNC_IO:
        case _PC_ABI_ASYNC_IO:
		val = 1;
		break;
		
        case _PC_ABI_AIO_XFER_MAX:
#if _MIPS_SIM == _ABI64
	    	if (!ABI_IS_IRIX5_64(get_current_abi())) 
		    val = INT_MAX;
		else
#endif /* _ABI64 */
		    val = LONG_MAX;
		break;
		
	case _PC_SYNC_IO:
	    	val = 1;
		break;
		
	case _PC_PRIO_IO:
	    	error = EINVAL;
		break;
		
	case _PC_FILESIZEBITS:
		/*
		 * this doesn't really have meaning for anything
		 * but regular files, but this seems like a nice default
		 */
		val = 32;
		break;

	case _PC_NAME_MAX:
	case _PC_NO_TRUNC:
		return (fs_pathconf1(bdp, cmd, valp, cr));

	default:
	    	error= EINVAL;
		break;
	}

	if (error == 0)
		*valp = val;
	return error;
}

/* ARGSUSED */
int
fs_map_subr(
	struct vnode *vp,
	off_t filesize,
	mode_t mode,
	off_t off,
	void *pprp,
	addr_t addr,
	size_t len,
	u_int prot, u_int maxprot,
	u_int flags,
	struct cred *cr)
{
	panic("fs_map_subr");
	/* NOTREACHED */
}

#define	round(r)	(((r)+sizeof(off_t)-1)&(~(sizeof(off_t)-1)))

int
fs_fmtdirent(void *bp, ino_t d_ino, off_t offset, char *d_name)
{
	register int i;
	register struct dirent *dirent = (struct dirent *)bp;
	register int reclen;
	int dsize;

	dirent->d_off = offset;
	dirent->d_ino = d_ino;
	strcpy(dirent->d_name, d_name);
	dsize = (char *)dirent->d_name - (char *)dirent;
	reclen = dsize + strlen(d_name) + 1;
	/*
	 * Pad to nearest word boundary (if necessary).
	 */
	for (i = reclen; i < round(reclen); i++)
		dirent->d_name[i-dsize] = '\0';
	dirent->d_reclen = reclen = round(reclen);
	return reclen;
}

#include <sys/ktypes.h>

#define	irix5_round(r)	(((r)+sizeof(irix5_off_t)-1)&(~(sizeof(irix5_off_t)-1)))

int
irix5_fs_fmtdirent(void *bp, ino_t d_ino, off_t offset, char *d_name)
{
	register int i;
	register struct irix5_dirent *dirent = bp;
	register int reclen;
	int dsize;

	dirent->d_off = offset;
	dirent->d_ino = d_ino;
	strcpy(dirent->d_name, d_name);
	dsize = (char *)dirent->d_name - (char *)dirent;
	reclen = dsize + strlen(d_name) + 1;
	/*
	 * Pad to nearest word boundary (if necessary).
	 */
	for (i = reclen; i < irix5_round(reclen); i++)
		dirent->d_name[i-dsize] = '\0';
	dirent->d_reclen = reclen = irix5_round(reclen);
	return reclen;
}
