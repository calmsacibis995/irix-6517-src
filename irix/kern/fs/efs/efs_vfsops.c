/*
 * EFS filesystem operations.
 *
 * Copyright 1992, Silicon Graphics, Inc.
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
#ident	"$Revision: 1.117 $"

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
#include <fs/specfs/spec_lsnode.h>
#include "efs_clnt.h"
#include "efs_inode.h"
#include "efs_dir.h"		/* for EFS_MAXNAMELEN */
#include "efs_fs.h"
#include "efs_sb.h"

extern mutex_t efs_ancestormon;

int efs_fstype;
struct zone *efs_inode_zone;
struct zone *efs_extent_zones[8];

extern void efs_bmapinit(void);
extern int efs_builddsum(struct efs *, struct cg *);

/* data pipe function */
#ifdef DATAPIPE
extern int fspeinit(void);
#endif

/* ARGSUSED */
int
efs_init(struct vfssw *vswp, int fstype)
{
	efs_fstype = fstype;
	init_mutex(&efs_ancestormon, MUTEX_DEFAULT, "ancestr", METER_NO_SEQ);
	ihashinit();
	efs_inode_zone = kmem_zone_init(sizeof(struct inode), "EFS inodes");
	if (doexlist_trash & 8) {
		efs_extent_zones[0] = kmem_zone_init(sizeof(extent) * 8,
			"extent8");
		efs_extent_zones[1] = kmem_zone_init(sizeof(extent) * 16,
			"extent16");
		efs_extent_zones[2] = kmem_zone_init(sizeof(extent) * 32,
			"extent32");
		efs_extent_zones[3] = kmem_zone_init(sizeof(extent) * 64,
			"extent64");
		efs_extent_zones[4] = kmem_zone_init(sizeof(extent) * 128,
			"extent128");
		efs_extent_zones[5] = kmem_zone_init(sizeof(extent) * 256,
			"extent256");
		efs_extent_zones[6] = kmem_zone_init(sizeof(extent) * 512,
			"extent512");
	}
#ifdef DATAPIPE
	fspeinit();
#endif
	return 0;
}

#define whymount_t	whymountroot_t
#define NONROOT_MOUNT	ROOT_UNMOUNT

static char *whymount[] = { "initial mount", "remount", "unmount" };

static int efs_mountfs(struct vfs *, bhv_desc_t *, dev_t, whymount_t, 
			struct efs_args *, struct cred *);
extern int efs_quotactl(struct bhv_desc *, int, int, caddr_t);

static int
efs_mount(
	struct vfs *vfsp,
	bhv_desc_t *bdp,
	struct vnode *mvp,
	struct mounta *uap,
	struct cred *cr)
{
	struct efs_args args;		/* efs mount arguments */
	int error;
	struct pathname dpn;
	struct vnode *bvp;
	dev_t dev;

	if (!cap_able_cred(cr, CAP_MOUNT_MGT))
		return EPERM;
	if (mvp->v_type != VDIR)
		return ENOTDIR;
	if ((uap->flags & MS_REMOUNT) == 0
	    && (mvp->v_count != 1 || (mvp->v_flag & VROOT)))
		return EBUSY;

	/*
	 * Copy in EFS-specific arguments.
	 */
	bzero(&args, sizeof args);
	if (copyin(uap->dataptr, &args, MIN(uap->datalen, sizeof args)))
		return EFAULT;

	/*
	 * Get arguments.
	 */
	if (error = pn_get(uap->dir, UIO_USERSPACE, &dpn))
		return error;

	/*
	 * Resolve path name of special file being mounted.
	 */
	if (error =
	    lookupname(uap->spec, UIO_USERSPACE, FOLLOW, NULLVPP, &bvp, NULL)) {
		pn_free(&dpn);
		return error;
	}
	if (bvp->v_type != VBLK) {
		VN_RELE(bvp);
		pn_free(&dpn);
		return ENOTBLK;
	}
	dev = bvp->v_rdev;
	VN_RELE(bvp);

	/*
	 * Ensure that this device isn't already mounted,
	 * unless this is a REMOUNT request
	 */
	if (vfs_devsearch(dev, VFS_FSTYPE_ANY) == NULL) {
		ASSERT((uap->flags & MS_REMOUNT) == 0);
	} else {
		if ((uap->flags & MS_REMOUNT) == 0) {
			pn_free(&dpn);
			return EBUSY;
		}
	}

	/*
	 * Mount the filesystem.
	 */
	error = efs_mountfs(vfsp, bdp, dev, NONROOT_MOUNT, &args, cr);
	pn_free(&dpn);
	return error;
}

/* VFS_MOUNT */
/*ARGSUSED*/
static int
efs_vfsmount(
	struct vfs *vfsp,
        struct vnode *mvp,
        struct mounta *uap,
	char *attrs,
        struct cred *cr)
{
	return(efs_mount(vfsp, NULL, mvp, uap, cr));
}

/* VFS_REMOUNT */
static int
efs_mntupdate(
	bhv_desc_t *bdp,
        struct vnode *mvp,
        struct mounta *uap,
        struct cred *cr)
{
	return(efs_mount(bhvtovfs(bdp), bdp, mvp, uap, cr));
}


int efs_sbupdate(struct efs *);

/*
 * Clean the given filesystem.  Push the superblock and the bitmap
 * to disk, and update the filesystem dirty state appropriately.
 * Used for unmounting.
 */
static int
efs_cleanfs(struct efs *fs)
{
	if (fs->fs_readonly)
		return 0;
	/*
	 * If filesystem state is EFS_ACTIVE, then when it was
	 * mounted it was EFS_CLEAN, and thus we can restore
	 * it to cleanliness.  Otherwise, it must have been
	 * a dirty root, and we make sure it goes out to disk
	 * dirty so that fsck will be used.
	 */
	if (fs->fs_dirty == EFS_ACTIVE)
		fs->fs_dirty = EFS_CLEAN;
	else
		fs->fs_dirty = EFS_DIRTY;
	fs->fs_fmod = 1;
	return efs_sbupdate(fs);
}


/*
 * Mount root file system.
 * "why" is ROOT_INIT on initial call, ROOT_REMOUNT if called to
 * remount the root file system, and ROOT_UNMOUNT if called to
 * unmount the root (e.g., as part of a system shutdown).
 */
static int
efs_mountroot(struct vfs *vfsp, bhv_desc_t *bdp, enum whymountroot why)
{
	int error;
	static int efsrootdone;
	struct cred *cr = get_current_cred();

	switch (why) {
	  case ROOT_INIT:
		/* called by efs_rootinit() before fs is mounted so there 
		 * should be no valid bhv_desc_t */
		ASSERT(!bdp);
		if (efsrootdone++)
			return EBUSY;
		if (rootdev == NODEV)
			return ENODEV;
		vfsp->vfs_dev = rootdev;
		break;
	  case ROOT_REMOUNT:
		ASSERT(bdp);
		vfs_setflag(vfsp, VFS_REMOUNT);
		break;
	  case ROOT_UNMOUNT:
		/*
		 * Can be called twice -- by efs_sync(CLOSE),
		 * and by uadmin.  efs_cleanfs gets confused
		 * if called twice.
		 */
		ASSERT(bdp);
		if (bhvtoefs(bdp)->fs_dirty != EFS_CLEAN)
			error = efs_cleanfs(bhvtoefs(bdp));
		binval(vfsp->vfs_dev);
		/*
		 * All pflushinvalvfsp does is to toss the pages mapped
		 * to the vnodes belonging to this vfsp. Why bother..
		 */
		/* pflushinvalvfsp(vfsp); */
		return error;
	}
	error = vfs_lock(vfsp);
	if (error)
		goto bad;
	error = efs_mountfs(vfsp, bdp, rootdev, why, NULL, cr);
	if (error) {
		vfs_unlock(vfsp);
		goto bad;
	}
	vfs_add(NULL, vfsp, (vfsp->vfs_flag & VFS_RDONLY) ? MS_RDONLY : 0);
	vfs_unlock(vfsp);
	return 0;
bad:
	cmn_err_tag(76,CE_WARN, "%s of root device %V failed with errno %d\n",
		    whymount[why], rootdev, error);
	return error;
}


/* VFS_ROOTINIT */
static int
efs_rootinit(struct vfs *vfsp)
{
        return(efs_mountroot(vfsp, NULL, ROOT_INIT));
}

/* VFS_MOUNTROOT */
static int
efs_vfsmountroot(bhv_desc_t *bdp, enum whymountroot why)
{
        return(efs_mountroot(bhvtovfs(bdp), bdp, why));
}


static void
efs_mounterror(dev_t dev, int error, char *msg)
{
	prdev("cannot mount: %s", dev,
	      (error == EROFS) ? "write-protected device" : msg);
}

static int efs_checksb(struct efs *);

static int
efs_mountfs(
	struct vfs *vfsp, 
	bhv_desc_t *bdp, 
	dev_t dev, 
	whymount_t why, 
	struct efs_args *ap,
	struct cred *cr)
{
	struct vnode *devvp = 0;
	struct efs *fs;
	struct mount *mp = 0;
	struct buf *bp = 0;
	int error;
	int i;
	int needclose = 0;
	struct inode *rip;
	struct vnode *rvp = 0;

	daddr_t devsize;
	int lbsize, lbshift;
	size_t mountsize;
	efs_ino_t inum;
	efs_daddr_t bn;
	struct cg *cg;
	static int mntno;

	/*
	 * Check whether dev is already mounted.
	 */
	if (vfsp->vfs_flag & VFS_REMOUNT) {
		mp = bhvtom(bdp);
		(void) iflush(mp, FLUSH_ALL);
		devvp = mp->m_devvp;
	} else {
		vnode_t *openvp;

		/*
		 * Get a special vnode.
		 */
		openvp = devvp = make_specvp(dev, VBLK);

		/*
		 * Open the block device we are mounting.
		 */
		VOP_OPEN(openvp, &devvp,
	(vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE, cr, error);
		if (error) {
			VN_RELE(devvp);
			return error;
		}
		needclose = 1;

#ifdef _IRIX_LATER
		/*
		 * Refuse to go any further if this
		 * device is being used for swapping.
		 */
		if (IS_SWAPVP(devvp)) {
			error = EBUSY;
			goto bad;
		}
#endif
	}

	/*
	 * Read in superblock and check for the EFS magic number.
	 */
	binval(dev);
	bp = bread(dev, EFS_SUPERBB, BTOBB(sizeof(struct efs)));
	if (bp->b_flags & B_ERROR) {
		error = geterror(bp);
		goto bad;
	}

	fs = (struct efs *) bp->b_un.b_addr;
	if (!IS_EFS_MAGIC(fs->fs_magic)) {
		error = EWRONGFS;
		goto bad;
	}

	/*
	 * Check that the size of the fs is not greater than the device's
	 * size in basic blocks, to guard against mounting a component of
	 * a logical volume.
	 *
	 * XXX a little slop (250 blocks) is tolerated because some older
	 *     disks had formatting problems, resulting in an fs slightly
	 *     larger than its partition.
	 */
	ASSERT(devvp);

	/*
	 * Ask specfs to return the ps_size field from the common snode.
	 */
	if ((devsize = spec_devsize(devvp)) == -1) {
		error = EINVAL;
		goto bad;
	}

	if (devsize > 0 && fs->fs_size > devsize + 250) {
		error = EINVAL;
		goto  bad;
	}

	/*
	 * Check superblock consistency unless mounting root.  Remount
	 * of root has already checked dirtyness.  Return ENOSPC to clue
	 * mount into the need for an fsck.
	 * XXX shouldn't remount of root do whole efs_checksb?
	 */
	if (why == NONROOT_MOUNT && !efs_checksb(fs)) {
		error = ENOSPC;
		goto bad;
	}

	/*
	 * Allocate VFS private data.
	 */
	if (!(vfsp->vfs_flag & VFS_REMOUNT)) {
		mountsize = sizeof *mp + (fs->fs_ncg - 1) * sizeof(struct cg);
		if ((mp = kmem_zalloc(mountsize, KM_SLEEP)) == NULL) {
			error = ENOMEM;
			goto bad;
		}
		vfsp->vfs_bcount = 0;
		vfs_insertbhv(vfsp, &mp->m_bhv, &efs_vfsops, mp);
		vfsp->vfs_fstype = efs_fstype;
		vfsp->vfs_dev = dev;
		vfsp->vfs_flag |= VFS_NOTRUNC|VFS_LOCAL|VFS_CELLULAR;
		vfsp->vfs_fsid.val[0] = dev;
		vfsp->vfs_fsid.val[1] = efs_fstype;
		mp->m_devvp = devvp;
		init_mutex(&mp->m_lock, MUTEX_DEFAULT, "efsmnt", mntno);
		/*
		 * The superblock's lock is not embedded in the data
		 * structure because the superblock is read in and
		 * written out to disk; reading it in (for instance,
		 * bread just above) would clobber the old lock value.
		 */
		fs->fs_lock = mutex_alloc(MUTEX_DEFAULT, KM_SLEEP,
			"efsmnt", mntno++);
	}

	/*
	 * Currently we allow a remount to change from read-only
	 * to read-write, but not vice-versa.
	 */
	if (vfsp->vfs_flag & VFS_RDONLY) {
		ASSERT((vfsp->vfs_flag & VFS_REMOUNT) == 0);
		fs->fs_readonly = 1;
		fs->fs_fmod = 0;
	} else {
		fs->fs_readonly = 0;
	}

	/*
	 * Copy the superblock into the mount structure and compute some
	 * resident members.  Don't copy in the volatile state on remounts.
	 * If this is a remount then we don't want to count on fsck not
	 * having overwritten any of it.  However, if this is not a remount,
	 * then we need to make sure that we get the fs_semlock field 
	 * copied into the mount structure.
	 */
	if (vfsp->vfs_flag & VFS_REMOUNT) {
		bcopy(fs, mtoefs(mp), offsetof(struct efs, fs_readonly));
	} else {
		bcopy(fs, mtoefs(mp), sizeof(struct efs) - sizeof(struct cg));
	}
	brelse(bp);
	bp = 0;
	fs = mtoefs(mp);
	fs->fs_corrupted = 0;
	fs->fs_ipcg = EFS_INOPBB * fs->fs_cgisize;
	fs->fs_dirty = (why != NONROOT_MOUNT && fs->fs_dirty != EFS_CLEAN) ?
			EFS_ACTIVEDIRT : EFS_ACTIVE;
	fs->fs_dev = dev;
	fs->fs_lastinum = fs->fs_ipcg * fs->fs_ncg - 1;
	if (ap != NULL && ap->flags & EFSMNT_LBSIZE) {
		lbsize = ap->lbsize;
		if (lbsize < NBPC
		    || lbsize > EFS_MAXLBSIZE
		    || lbsize & (lbsize-1)) {
			error = EINVAL;
			goto bad;
		}
	} else {
		lbsize = BBTOB(fs->fs_sectors+1);
		lbsize = MIN(lbsize, EFS_MAXLBSIZE);
		lbsize = MAX(lbsize, NBPC);
	}

	for (lbshift = 0; (lbsize >>= 1) != 0; lbshift++)
		;
	fs->fs_lbshift = MAX(lbshift, BPCSHIFT);
	vfsp->vfs_bsize = 1 << fs->fs_lbshift;
	fs->fs_inopchunk = (EFS_INOPCHUNK + EFS_INOPBB-1) & ~EFS_INOPBBMASK;
	fs->fs_inopchunkbb = (fs->fs_inopchunk + EFS_INOPBB-1) >> EFS_INOPBBSHIFT;
	fs->fs_minfree = EFS_MINFREEBB;
	fs->fs_mindirfree = EFS_MINDIRFREEBB;
	fs->fs_bmbbsize = BTOBB(fs->fs_bmsize);

#ifdef EFS_DEBUG
	printf("inopchunk=%d inopchunkbb=%d minfree=%d mindirfree=%d\n",
		fs->fs_inopchunk, fs->fs_inopchunkbb,
		fs->fs_minfree, fs->fs_mindirfree);
#endif

	/*
	 * Initialize summary information for the cylinder groups.
	 */
	inum = 0;
	bn = fs->fs_firstcg;
	cg = &fs->fs_cgs[0];
	fs->fs_cgrotor = cg;
	for (i = fs->fs_ncg; --i >= 0; cg++) {
		cg->cg_firsti = inum;
		cg->cg_lowi = cg->cg_firsti;
		cg->cg_firstbn = bn;
		cg->cg_firstdbn = bn + fs->fs_cgisize;
		if (error = efs_builddsum(fs, cg)) {
			efs_mounterror(dev, error, "bitmap read error");
			goto bad;
		}
		inum += fs->fs_ipcg;
		bn += fs->fs_cgfsize;
	}

	/*
	 * Get and sanity-check the root inode.
	 */
	if (error = iget(mp, EFS_ROOTINO, &rip))
		goto bad;
	rvp = itov(rip);
	if ((rip->i_mode & IFMT) != IFDIR) {
		error = EINVAL;
		goto bad;
	}
	VN_FLAGSET(rvp,VROOT);
	mp->m_rootip = rip;
	iunlock(rip);

	/*
	 * Set time from root's superblock on first mount.
	 */
	if (why == ROOT_INIT)
		clkset(fs->fs_time);

	/*
	 * Last, push superblock to disk with the dirty bit set, so as
	 * to require an fsck if the fs wasn't unmounted cleanly.
	 */
	if ((vfsp->vfs_flag & VFS_RDONLY) == 0) {
		fs->fs_fmod = 1;
		if (error = efs_sbupdate(fs)) {
			efs_mounterror(dev, error, "superblock write error");
			goto bad;
		}
	}

	spec_mounted(devvp);

	return 0;

bad:
	if (error == 0)
		error = EIO;
	if (rvp) {
		vmap_t vmap;
		VMAP(rvp, vmap);
		VN_RELE(rvp);
		vn_purge(rvp, &vmap);
	}
	if (bp) {
		bp->b_flags |= B_AGE;
		brelse(bp);
	}
	if (!(vfsp->vfs_flag & VFS_REMOUNT)) {
		if (mp) {
			/* 
			 * no locking is needed for bhv_remove()
			 * since the new vfs is not on
			 * the global vfs list yet
			 */
			VFS_REMOVEBHV(vfsp, &mp->m_bhv);
			mutex_dealloc(fs->fs_lock);
			fs->fs_lock = 0;
			mutex_destroy(&mp->m_lock);
			kmem_free(mp, mountsize);
		}
	}
	if (needclose) {
		/*REFERENCED*/
		int unused;

		VOP_CLOSE(devvp, (vfsp->vfs_flag & VFS_RDONLY) ?
		      FREAD : FREAD|FWRITE, L_TRUE, cr, unused);
		binval(dev);
		VN_RELE(devvp);
	}
	return error;
}

static __int32_t efs_checksum(struct efs *);
static __int32_t efs_oldchecksum(struct efs *);

/*
 * Sanity check on superblock. Returns 1 if OK, 0 otherwise.  Note that
 * the check of fs_heads is now omitted; it was always a bogus field, not
 * used for anything.  Bitmap size limit has gone up too: it now allows
 * for approximately 50 GB.
 */
static int
efs_checksb(register struct efs *fs)
{
	register int d_area;

	if (efs_checksum(fs) != fs->fs_checksum &&
	    efs_oldchecksum(fs) != fs->fs_checksum ||
	    fs->fs_dirty != EFS_CLEAN ||
	    fs->fs_ncg <= 0 ||
	    fs->fs_sectors <= 0 ||
	    fs->fs_bmsize <= 0 ||
	    fs->fs_bmsize > 100 * (1024*1024 / BITSPERBYTE) ||
	    fs->fs_tinode < 0 ||
	    fs->fs_tinode > fs->fs_cgisize * EFS_INOPBB * fs->fs_ncg ||
	    fs->fs_tfree < 0 ||
	    fs->fs_tfree > (fs->fs_cgfsize - fs->fs_cgisize) * fs->fs_ncg)
		return 0;

	/* size parameterization is different in 3.3 & pre-3.3 */

	d_area = fs->fs_firstcg + fs->fs_cgfsize*fs->fs_ncg;

	if (fs->fs_magic == EFS_MAGIC) {
	     	if (fs->fs_size != d_area)
			return 0;
	} else if (fs->fs_magic == EFS_NEWMAGIC) {
		if ((d_area > fs->fs_size) || (d_area > fs->fs_bmblock))
			return 0;
	}
	return 1;
}

/*
 * efs_checksum:
 *	- compute the checksum of the read in portion of the filesystem
 *	  structure
 */
static __int32_t
efs_checksum(register struct efs *fs)
{
	register ushort *sp;
	register __int32_t checksum;

	checksum = 0;
	sp = (ushort *)fs;
	while (sp < (ushort *)&fs->fs_checksum) {
		checksum ^= *sp++;
		checksum = (checksum << 1) | (checksum < 0);
	}
	return (checksum);
}

/*
 * efs_oldchecksum:
 *	- this checksum suffers from shifting rather than rotating, thus
 *	  discarding information from all superblock members 32 half-words
 *	  or more before the checksum; since a zero-filled spare array lies
 *	  just before the checksum, much of the computed sum is 0
 */
static __int32_t
efs_oldchecksum(register struct efs *fs)
{
	register ushort *sp;
	register __int32_t checksum;

	checksum = 0;
	sp = (ushort *)fs;
	while (sp < (ushort *)&fs->fs_checksum) {
		checksum ^= *sp++;
		checksum <<= 1;
	}
	return (checksum);
}


int
efs_sbupdate(register struct efs *fs)
{
	register struct buf *bp;
	int error;
	extern mutex_t *mutex_trace;

	if (!fs->fs_fmod || fs->fs_readonly)
		return 0;

	/*
	 * Get the superblock buffer, then try to lock the filesystem
	 * so we can muck with checksum.
	 */
	bp = getblk(fs->fs_dev, EFS_SUPERBB, BTOBB(sizeof(struct efs)));
	if ((bp->b_flags & B_ERROR) || !mutex_trylock(fs->fs_lock)) {
		error = (bp->b_flags & B_ERROR) ? geterror(bp) : 0;
		if (!(bp->b_flags & B_DONE))
			bp->b_flags |= B_STALE;
		brelse(bp);
		return error;
	}

	/*
	 * Force dirty if the filesystem is corrupted.  This ensures that
	 * fsck will be run after the next unmount.
	 */
	if (fs->fs_corrupted)
		fs->fs_dirty = EFS_ACTIVEDIRT;
	fs->fs_fmod = 0;
	fs->fs_time = time;
	fs->fs_checksum = efs_checksum(fs);

	/*
	 * Turn off disk full mode if any blocks have been freed and at least
	 * a minute per 400 MB of file system has passed since disk full
	 * mode began.  fs_diskfull contains the lbolt contents at the time the
	 * disk was observed to be getting full.
	 */
	if (fs->fs_diskfull) {
		if (fs->fs_freedblock &&
		    ((lbolt - fs->fs_diskfull) > fs->fs_size >> 7)) {
			fs->fs_freedblock = 0;
			fs->fs_diskfull = 0;
		}
	} 

	/*
	 * Update the superblock.
	 */
	bcopy(fs, bp->b_un.b_addr, sizeof(struct efs));
	bzero((struct efs *)bp->b_un.b_addr + 1,
	      BBTOB(BTOBB(sizeof(struct efs))) - sizeof(struct efs));
	mutex_unlock(fs->fs_lock);

	return bwrite(bp);
}

/*
 * Unmount vfsp
 */
/* ARGSUSED */
static int
efs_unmount(bhv_desc_t *bdp, int flags, struct cred *cr)
{
	struct mount *mp;
	struct inode *rip;
	int error;
	struct vnode *rvp;
	vmap_t vmap;
	struct efs *fs;
	struct vnode *bvp;
	struct vfs *vfsp = bhvtovfs(bdp);

	if (!cap_able_cred(cr, CAP_MOUNT_MGT))
		return EPERM;

	mp = bhvtom(bdp);
	if (!iflush(mp, 0))
		return EBUSY;
	/*
	 * Clean up quota structures.
	 */
	if (mp->m_quotip)
		(void) qt_closedq(mp, 1);

	/*
	 * Flush root inode to disk.
	 */
	rip = mp->m_rootip;
	ilock(rip);
	rip->i_flags |= ISYN;
	if (error = efs_iupdat(rip)) {
		iunlock(rip);
		return error;
	}

	/*
	 * Get rid of root inode, if we can.
	 */
	rvp = itov(rip);
	if (rvp->v_count != 1) {
		iunlock(rip);
		return EBUSY;
	}
	VMAP(rvp, vmap);
	VN_RELE(rvp);
	vn_purge(rvp, &vmap);
	ASSERT(mp->m_inodes == NULL);

	/*
	 * Clean the fs only after potential writes done indirectly by
	 * vn_purge(rvp).  Invalidate all cached buffers.
	 */
	fs = mtoefs(mp);
	(void) efs_cleanfs(fs);
	binval(fs->fs_dev);

	/*
	 * Mark device unmounted before freeing in-core superblock, so
	 * devvptoefs, below, does not look at free store.
	 */
	bvp = mp->m_devvp;

	spec_unmounted(bvp);

	mutex_dealloc(fs->fs_lock);
	fs->fs_lock = 0;
	/* the behavior chain lock is held by the VFS_UNMOUNT macro */
	VFS_REMOVEBHV(vfsp, &mp->m_bhv);
	mutex_destroy(&mp->m_lock);
	kmem_free(mp, sizeof *mp + (fs->fs_ncg - 1) * sizeof(struct cg));

	/*
	 * Close and release the device vnode.
	 */
	VOP_CLOSE(bvp, (vfsp->vfs_flag & VFS_RDONLY) ? FREAD
			 : FREAD|FWRITE, L_TRUE, cr, error);
	VN_RELE(bvp);
	return 0;
}

static int
efs_root(bhv_desc_t *bdp, struct vnode **vpp)
{
	struct vnode *vp;
	
	vp = itov(bhvtom(bdp)->m_rootip);
	VN_HOLD(vp);
	*vpp = vp;
	return 0;
}

/*
 * Get a buffer containing the superblock from an EFS given its
 * device vnode pointer.
 * Used by statfs ...
 */
static int
devvptoefs(struct vnode *devvp, struct buf **bpp, struct efs **fsp,
	   struct cred *cr)
{
	int error;
	int	retval;
	dev_t dev;
	struct buf *bp;
	struct efs *fs;
	vnode_t *openvp;
	
	if (devvp->v_type != VBLK)
		return ENOTBLK;
	openvp = devvp;
	VOP_OPEN(openvp, &devvp, FREAD, cr, error);
	if (error)
		return error;

	dev = devvp->v_rdev;
	VOP_RWLOCK(devvp, VRWLOCK_WRITE);

	/*
	 * Ask specfs to check for the SMOUNTED flag in the common snode.
	 */
	if ((retval = spec_ismounted(devvp)) == -1) {

		VOP_RWUNLOCK(devvp, VRWLOCK_WRITE);
		return ENODEV;
	}

	if (retval) {
		bhv_desc_t *vfsbdp;
		struct vfs *vfsp;

		/*
		 * Device is mounted.  Get an empty buffer to hold a
		 * copy of its superblock, so we don't have to worry
		 * about racing with unmount.  Hold devvp's lock to
		 * block unmount here.
		 */
		bp = ngeteblk(sizeof *fs);
		fs = (struct efs *) bp->b_un.b_addr;
		vfsp = vfs_devsearch(dev, efs_fstype);
		if (vfsp == NULL) {
			VOP_RWUNLOCK(devvp, VRWLOCK_WRITE);
			return EINVAL;
		}
		vfsbdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), &efs_vfsops);
		bcopy(bhvtoefs(vfsbdp), fs, sizeof *fs);
	} else {
		/*
		 * If the buffer is already in core, it might be stale.
		 * User might have been doing block reads, then mkfs.
		 * Very unlikely, but would cause a buffer to contain
		 * stale information.
		 * If buffer is marked DELWRI, then use it since it reflects
		 * what should be on the disk.
		 */
		bp = incore(dev, BLKDEV_LTOP(BLKDEV_LBN(EFS_SUPERBOFF)),
			    BLKDEV_BB, 1);
		if (bp && !(bp->b_flags & B_DELWRI)) {
			bp->b_flags |= B_STALE;
			brelse(bp);
			bp = 0;
		}
		if (!bp)
			bp = bread(dev, BLKDEV_LTOP(BLKDEV_LBN(EFS_SUPERBOFF)),
				BLKDEV_BB);
		if (bp->b_flags & B_ERROR) {
			error = bp->b_error;
			brelse(bp);
		} else {
			fs = (struct efs *)
			      (bp->b_un.b_addr + BLKDEV_OFF(EFS_SUPERBOFF));
		}
	}
	VOP_RWUNLOCK(devvp, VRWLOCK_WRITE);

	*bpp = bp;
	*fsp = fs;
	return error;
}

/*
 * Get file system statistics - from a device vp - used by
 * statfs only
 */
int
efs_statdevvp(struct statvfs *sp, struct vnode *devvp)
{
        int error;
	/*REFERENCED*/
	int unused;
        struct buf *bp;
        struct efs *fs;

	if (error = devvptoefs(devvp, &bp, &fs, get_current_cred()))
		return error;

	if (IS_EFS_MAGIC(fs->fs_magic)) {
		sp->f_bsize = BBSIZE;
		sp->f_frsize = BBSIZE;
		sp->f_blocks = fs->fs_ncg * (fs->fs_cgfsize - fs->fs_cgisize);
		sp->f_bfree = sp->f_bavail = fs->fs_tfree;
		sp->f_files = (fs->fs_ncg * fs->fs_cgisize * EFS_INOPBB) - 2;
		sp->f_ffree = sp->f_favail = fs->fs_tinode;
		sp->f_fsid = fs->fs_dev;
		(void) strcpy(sp->f_basetype, vfssw[efs_fstype].vsw_name);
		sp->f_flag = 0;
		sp->f_namemax = EFS_MAXNAMELEN;
		bzero(sp->f_fstr, sizeof sp->f_fstr);
	} else
		error = EINVAL;

	brelse(bp);
	VOP_CLOSE(devvp, FREAD, L_TRUE, get_current_cred(), unused);
        return error;
}

/*
 * Get file system statistics.
 */
/* ARGSUSED */
static int
efs_statvfs(bhv_desc_t *bdp, struct statvfs *sp, struct vnode *vp)
{
	struct efs *fs;
	struct vfs *vfsp = bhvtovfs(bdp);

	fs = bhvtoefs(bdp);

	sp->f_bsize = BBSIZE;
	sp->f_frsize = BBSIZE;
	sp->f_blocks = fs->fs_ncg * (fs->fs_cgfsize - fs->fs_cgisize);
	sp->f_bfree = sp->f_bavail = fs->fs_tfree;
	sp->f_files = (fs->fs_ncg * fs->fs_cgisize * EFS_INOPBB) - 2;
	sp->f_ffree = sp->f_favail = fs->fs_tinode;
	sp->f_fsid = fs->fs_dev;
	(void) strcpy(sp->f_basetype, vfssw[efs_fstype].vsw_name);
	sp->f_flag = vfsp ? vf_to_stf(vfsp->vfs_flag) : 0;
	if (vp && vp->v_flag & VISSWAP && vp->v_type == VREG)
		sp->f_flag &= ~ST_LOCAL;
	sp->f_namemax = EFS_MAXNAMELEN;
	bzero(sp->f_fstr, sizeof sp->f_fstr);

	return 0;
}

/*
 * Flush any pending I/O to file system vfsp.
 *
 * Flags:
 *	SYNC_BDFLUSH - do NOT sleep waiting for an inode
 *	SYNC_ATTR    - sync attributes - note that ordering considerations
 *			dictate that we also flush dirty pages
 *	SYNC_WAIT    - do synchronouse writes - inode & delwri
 *	SYNC_DELWRI  - look at inodes w/ delwri pages. Other flags
 *			decide how to deal with them.
 *	SYNC_CLOSE   - flush delwri and invalidate all.
 *	SYNC_FSDATA  - push fs data (e.g. superblocks)
 */
#define PREEMPT_MASK	0x7f

/*
 * This is the minimum amount that will be incrementally synced from
 * efs_sync() on an inode with dirty buffers so that we can
 * be sure to keep moving the permanent part of the file forward
 * even when the user keeps writing and allocating.
 *
 * For now the min will be set at 512 KB.
 */
#define	EFS_MIN_REM_CHUNK	524288

/*ARGSUSED*/
static int
efs_sync(bhv_desc_t *bdp, register int flags, struct cred *cr)
{
	struct mount *mp;
	int error, lasterr, mntlock;
	unsigned int preempt;
	struct inode *ip;
	struct vnode *vp;
	vmap_t vmap;
	u_long ireclaims;
	scoff_t push_len;
	int skip;
#ifdef DEBUG
	bhv_desc_t *vnbdp;
#endif

	mp = bhvtom(bdp);
	error = lasterr = preempt = 0;
loop:
	mlock(mp);
	mntlock = 1;
	for (ip = mp->m_inodes; ip != NULL; ip = ip->i_mnext) {

		/*
		 * Five (exclusive) modes:
		 *
		 * SYNC_CLOSE from uadmin:shutdown
		 * SYNC_DELWRI from sync, umount
		 * SYNC_BDFLUSH from bdflush (or friend)
		 * SYNC_PDFLUSH from pdflush daemon
		 * SYNC_UNMOUNT this sync is part of an unmount
		 *
		 * Only (try to) grab inode if:
		 *
		 * SYNC_CLOSE
		 *   -- or --
		 * SYNC_DELWRI and
		 *	VN_DIRTY or
		 *	ip->i_remember needs updating or
		 *	any IMOD-ish inode bits set
		 *   -- or --
		 * SYNC_BDFLUSH and
		 *	vp->v_dpages or
		 *	ip->i_remember needs updating or
		 *	any IMOD-ish inode bits set
		 *   -- or --
		 * SYNC_PDFLUSH and
		 *	vp->v_dpages
		 */

		skip = 0;
		vp = itov(ip);

		if (flags & SYNC_BDFLUSH) {
			if (!(ip->i_flags & (IMOD|IACC|IUPD|ICHG)) &&
			    (((ip->i_mode & IFMT) != IFREG) ||
			     (ip->i_remember == ip->i_size)))
				skip = 1;
		} else if (flags & SYNC_PDFLUSH) {
			if (!vp->v_dpages)
				skip = 1;
		} else if (flags & SYNC_DELWRI) {
			if (!(ip->i_flags & (IMOD|IACC|IUPD|ICHG)) &&
			    (((ip->i_mode & IFMT) != IFREG) ||
			     (ip->i_remember == ip->i_size)) &&
			    !VN_DIRTY(vp))
				skip = 1;
		}

		/*
		 * don't diddle with swap files - too dangerous since
		 * we can sleep waiting for memory holding the inode locked
		 */
		if (vp->v_flag & VISSWAP)
			skip = 1;

		if (skip && !(doexlist_trash & 4))
			continue;

		/*
		 * Try to lock ip without sleeping.  If we can't, we must
		 * unlock mp, get vp (potentially off the vnode freelist),
		 * and sleep for ip.  
		 */
		if (ilock_nowait(ip) == 0) {
			if (skip)
				continue;
			if (flags & (SYNC_BDFLUSH|SYNC_PDFLUSH))
				continue;
			mntlock = 0;
			VMAP(vp, vmap);
			ireclaims = mp->m_ireclaims;
			munlock(mp);
			if (!(vp = vn_get(vp, &vmap, 0)))
				goto loop;
#ifdef DEBUG
			vnbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), 
						       &efs_vnodeops);
			ASSERT(vnbdp != NULL);
			ASSERT(ip == bhvtoi(vnbdp));
#endif
			ilock(ip);
			VN_RELE(vp);
		}
		if (doexlist_trash & 4)
			efs_inode_check(ip, "efs_sync");
		if (skip) {
			iunlock(ip);
			continue;
		}

		/*
		 * SYNC_CLOSE only called from shutdown.
		 */
		if (flags & SYNC_CLOSE) {
			VOP_FLUSHINVAL_PAGES(vp, 0, ip->i_size - 1, FI_NONE);
			ip->i_remember = ip->i_size;
		} else

		/*
		 * If want to flush DELWRI pages respect SYNC_WAIT flag.
		 * If we flush the whole file then move i_remember
		 * all the way up to the file size.  This will
		 * ensure that the full file size is recorded in
		 * the efs_iupdat() call below.
		 */
		if (flags & SYNC_DELWRI) {
			if (VN_DIRTY(vp)) {
				if (mntlock) {
					ireclaims = mp->m_ireclaims;
					munlock(mp);
					mntlock = 0;
				}
				VOP_FLUSH_PAGES(vp, (off_t)0, ip->i_size - 1,
					    flags & SYNC_WAIT ? 0 : B_ASYNC, FI_NONE, error);
			}

			/*
			 * If all dirty buffers/pages have been pushed,
			 * push up i_remember.
			 */
			if (((ip->i_mode & IFMT) == IFREG) &&
			    (ip->i_remember != ip->i_size)) {
				ip->i_remember = ip->i_size;
				ip->i_flags |= IMOD;
			}
		}

		if (flags & SYNC_PDFLUSH) {
			if (vp->v_dpages) {
				if (mntlock) {
					ireclaims = mp->m_ireclaims;
					munlock(mp);
					mntlock = 0;
				}
				pdflush(vp, B_DELWRI);
			}
		}
		else if (flags & SYNC_BDFLUSH) {
			/*
			 * Push inodes if necessary.
			 * If called from bdflush() and the file has allocated
			 * new blocks and still has dirty buffers in memory,
			 * then sync a portion of the file and make that
			 * portion permanent.  The permanance is achieved
			 * by incrementing ip->i_remember before calling
			 * efs_iupdat().
			 */
			ASSERT(ip->i_remember <= ip->i_size);

			if (ip->i_flags & (IMOD|IACC|IUPD|ICHG) &&
			    (!(ip->i_flags & ITRUNC) || !VN_DIRTY(vp))) {
				if ((ip->i_mode & IFMT) == IFREG) {
					ip->i_remember = ip->i_size;
				}
				goto push;
			} else if (((ip->i_mode & IFMT) == IFREG) &&
				   (ip->i_remember < ip->i_size)) {

				if (mntlock) {
					ireclaims = mp->m_ireclaims;
					munlock(mp);
					mntlock = 0;
				}

				/*
				 * If i_remember is more than min-push-len,
				 * push larger of the minimum and 1/8 the
				 * distance -- don't want i_size to get too
				 * far ahead when writing a lot.
				 */
				push_len = ip->i_size - ip->i_remember;
				if (push_len > EFS_MIN_REM_CHUNK)
					push_len = MAX(EFS_MIN_REM_CHUNK,
							push_len / 8);
				chunkpush(vp,
					  (off_t)ip->i_remember,
					  ((off_t)(ip->i_remember + push_len))-1,
					  B_ASYNC|B_BDFLUSH);

				ip->i_remember += push_len;
				ASSERT(ip->i_remember <= ip->i_size);
				error = efs_iupdat(ip);
			}

		} else if (ip->i_flags & (IMOD|IACC|IUPD|ICHG)) {
			if (flags & SYNC_WAIT)
				ip->i_flags |= ISYN;
	push:
                        if (mntlock) {
                                ireclaims = mp->m_ireclaims;
                                munlock(mp);
                                mntlock = 0;
                        }
			error = efs_iupdat(ip);
		}

		iunlock(ip);
		if (error)
			lasterr = error;

		if (mntlock == 0) {
			preempt = 0;
			mntlock = 1;
			goto done_preempt;
		} else
		if ((++preempt & PREEMPT_MASK) == 0)  {
			/* release mount point lock */
			ireclaims = mp->m_ireclaims;
			munlock(mp);
	done_preempt:
			mlock(mp);
			if (mp->m_ireclaims != ireclaims) {
				munlock(mp);
				goto loop;
			}
		}
	}
	munlock(mp);

	/*
	 * Done with inodes and their data.  Push the superblock.
	 * Notice how we emulate the old u.u_error side-effecting code,
	 * by charging on past errors, but remembering the last one.
	 */

	if (flags & SYNC_CLOSE) {
		if (error = efs_mountroot(bhvtovfs(bdp), bdp, ROOT_UNMOUNT))
			lasterr = error;
	} else if (flags & SYNC_FSDATA) {
		if (error = efs_sbupdate(mtoefs(mp)))
			lasterr = error;
	}

	return lasterr;
}

static int
efs_vget(bhv_desc_t *bdp, struct vnode **vpp, struct fid *fidp)
{
	struct efid *efid;
	int error;
	struct inode *ip;

	efid = (struct efid *)fidp;
	if (error = iget(bhvtom(bdp), efid->efid_ino, &ip)) {
		*vpp = NULL;
		return error;
	}
	if (ip->i_gen != efid->efid_gen) {
		iput(ip);
		*vpp = NULL;
		return 0;
	}
	iunlock(ip);
	*vpp = itov(ip);
#ifdef _IRIX_LATER
	if ((ip->i_mode & ISVTX) && !(ip->i_mode & (IEXEC | IFDIR))
	    && efs_stickyhack) {
		VN_FLAGSET(*vpp, VISSWAP);
	}
#endif
	return 0;
}


vfsops_t efs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	efs_vfsmount,
	efs_rootinit,
	efs_mntupdate,
	fs_dounmount,
	efs_unmount,
	efs_root,
	efs_statvfs,
	efs_sync,
	efs_vget,
	efs_vfsmountroot,
	fs_realvfsops,	/* realvfsops */
	fs_import,	/* import */
	efs_quotactl,
};
