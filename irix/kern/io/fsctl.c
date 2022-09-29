/*
 * fsctl - EFS file system pseudo-device
 *
 * ioctl(2) interface presents EFS file system operations to user level
 *
 * Super-user only, single user device.  MP safe.
 *
 * User-level reorganizer does:
 *	fscopen()
 *	for each file
 *		fscioctl(ILOCK)
 *		efs_iget(), efs_getextents()
 *		fscioctl(BALLOC)                     // new location of data
 *		lseek(), read(), lseek(), write()    // copy data
 *		fsciotcl(BALLOC)	             // any new indirect blocks
 *		fscioctl(ICOMMIT)
 *		fscioctl(BFREE)                      // old location of data
 *		fscioctl(BFREE)			     // old indirect blocks
 *	fscclose()
 */
#ident	"$Revision: 1.45 $"

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/pfdat.h>		/* For pflushinvalvp() prototype */
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/capability.h>
#include <bstring.h>
#include <sys/buf.h>

#include <sys/fs/efs.h>		/* XXX grab-bag of all headers */
#include <sys/fsctl.h>
#include <sys/xlate.h>

int fscdevflag = D_MP;

static int exgetandverify(struct inode *ip, struct fscarg *fap);
static int invalidblocks(struct efs *fs, daddr_t bn, int len);
static void fsr_flush(struct inode *ip);
static void fsr_invalid(struct inode *ip, struct fscarg *fap);

static int irix5_to_fscarg(enum xlate_mode, void *, int, xlate_info_t *);
static int irix5_o32_to_fscarg(enum xlate_mode, void *, int, xlate_info_t *);
static int irix5_n32_to_fscarg(enum xlate_mode, void *, int, xlate_info_t *);

static unsigned long fsc_pid;	/* marks "in use" and user */
static lock_t fsc_lock;		/* protecting fsc_pid makes dev MP safe */
static struct inode *fsc_ip;	/* the ILOCK'ed inode */

void
fscinit(void)
{
	spinlock_init(&fsc_lock, "fsc_lock");
}

/* ARGSUSED */
int
fscopen(dev_t *dev, int flag, int otype, struct cred *crp)
{
	int s;

	if (!_CAP_CRABLE(crp, CAP_DEVICE_MGT))
		return EPERM;
	s = mp_mutex_spinlock(&fsc_lock);
	if (fsc_pid) {
		mp_mutex_spinunlock(&fsc_lock, s);
		return EBUSY;
	}
	drv_getparm(PPID, &fsc_pid);
	mp_mutex_spinunlock(&fsc_lock, s);
	return 0;
}

/*
 * Purpose in life is to clean up after an aborted user-level program:
 * release the locked vnode and this device
 * XXX watch for multiple closes on block devices: OTYPE_LYR
 */
/* ARGSUSED */
int
fscclose(dev_t dev, int flag, int otype, struct cred *crp)
{
	int s;
	unsigned long pid;

	drv_getparm(PPID, &pid);
	s = mp_mutex_spinlock(&fsc_lock);
	if (pid != fsc_pid) {
		mp_mutex_spinunlock(&fsc_lock, s);
		return 0;
	}
	mp_mutex_spinunlock(&fsc_lock, s);

	/* unlock the inode */
	if (fsc_ip) {
		efs_setcorrupt(itom(fsc_ip));
		ilock(fsc_ip);
		iput(fsc_ip);
		fsc_ip = 0;
	}

	/* release the device */
	s = mp_mutex_spinlock(&fsc_lock);
	fsc_pid = 0;
	mp_mutex_spinunlock(&fsc_lock, s);
	return 0;
}

/* ARGSUSED */
int
fscioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *cred, int *rval)
{
	int len;
	struct fscarg fa;
	struct inode *ip;
	struct vfs *vfsp;
	struct efs *fs;
	struct cg *cg;
	int s, error;
	unsigned long pid;
	dev_t bdev;
	extern int efs_fstype;

	drv_getparm(PPID, &pid);

	s = mp_mutex_spinlock(&fsc_lock);
	if (pid != fsc_pid) {		/* Don't permit descendants */
		mp_mutex_spinunlock(&fsc_lock, s);/* to inherit this dev. */
		return EPERM;
	}
	mp_mutex_spinunlock(&fsc_lock, s);

	/*
	 * The only time the structure matches the caller is for 64
	 * bit callers on 64 bit kernels.  By setting the kernel abi
	 * parameter to 5_64, we guarantee that the translate function
	 * will be called for all other cases.
	 */
	if (copyin_xlate((caddr_t)arg, &fa, sizeof(fa), irix5_to_fscarg,
				get_current_abi(), ABI_IRIX5_64, 1))
		return EFAULT;

	ip = fsc_ip;
	if (dev_is_vertex(fa.fa_dev))
		bdev = chartoblock(fa.fa_dev);
	else
		bdev = fa.fa_dev;

	switch (cmd) {
	case ILOCK:
	    {
		bhv_desc_t *bdp;

		if (ip)
			return EBUSY;

		/* find the mount point for this dev */
		vfsp = vfs_busydev(bdev, efs_fstype);
		if (vfsp == 0)
			return EINVAL;

		/* get the inode and lock it -- iget ilock()s */
		bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), &efs_vfsops);
		error = iget(bhvtom(bdp), fa.fa_ino, &ip);
		vfs_unbusy(vfsp);
		if (error)
			return error;

		if ((ip->i_mode & IFMT) != IFREG
		    && (ip->i_mode & IFMT) != IFDIR
		    && (ip->i_mode & IFMT) != IFLNK) {
			iput(ip);
			return EINVAL;
		}

		/* bring in indirect extents, if any */
		if ((ip->i_flags & IINCORE) == 0) {
			if (error = efs_readindir(ip)) {
				iput(ip);
				return error;
			}
			ASSERT(ip->i_flags & IINCORE);
		}
		/* sync file data to disk */
		fsr_flush(ip);

		/* sync the inode itself -- set the MOD bit so that any
		 * any data remaining from a prior asynch flush gets
		 * pushed to disk.
		 */
		ip->i_flags |= ISYN|IMOD;
		efs_itrunc(ip, ip->i_size, 0);
		if (ip->i_flags & (ICHG|IUPD|IMOD))
			error = efs_iupdat(ip);
		ip->i_flags |= INOUPDAT;
		iunlock(ip);
		fsc_ip = ip;
		break;
	    }

	case IUNLOCK:
	    {
		if (!ip || bdev != ip->i_dev || fa.fa_ino != ip->i_number)
			return EINVAL;

		ilock(ip);
		iput(ip);	/* release the inode */
		fsc_ip = 0;
		break;
	    }

	case ICOMMIT:
	    {

		if (!ip || bdev != ip->i_dev || fa.fa_ino != ip->i_number)
			return EINVAL;

		ilock(ip);

		if ((ip->i_flags & (INOUPDAT|IMOD|IUPD|ICHG)) != INOUPDAT) {
			iunlock(ip);
			return EBUSY;
		}
		fsr_invalid(ip, &fa);

		/* check flags again after invalidate to make sure that dirty
		 * pages moving from the page to the buffer cache are
		 * detected.
		 */
		if ((ip->i_flags & (INOUPDAT|IMOD|IUPD|ICHG)) != INOUPDAT) {
			iunlock(ip);
			return EBUSY;
		}
		ASSERT(!itov(ip)->v_dbuf);
		if (itov(ip)->v_dbuf) {
			iunlock(ip);
			return EBUSY;
		}

		/* verify extents before dorking inode */
		if (error = exgetandverify(ip, &fa)) {
			iunlock(ip);
			return error;
		}

		/* grow space for direct extents, and copy them right in */
		len = fa.fa_ne * sizeof(extent);
		irealloc(ip, len);

		/* slam them in -- overwriting old */
		bcopy(fa.fa_ex, ip->i_extents, len);
		kmem_free(fa.fa_ex, len);
		ip->i_numextents = fa.fa_ne;

		/* punt old indirect extents */
		if (ip->i_indir)
			kmem_free(ip->i_indir, ip->i_numindirs*sizeof(extent));
		if (fa.fa_ne <= EFS_DIRECTEXTENTS) {
			ip->i_numindirs = ip->i_indirbytes = 0;
			ip->i_indir = 0;
		} else {
			ip->i_numindirs = fa.fa_nie;
			ip->i_indir = fa.fa_ix;
			/* always a multi of BSIZE! see efs_expand() */
			ip->i_indirbytes = BBTOB(BTOBB(len));
		}

		/* push out to disk, synchronously and preserve times */
		/* XXX what if disk fails: bad spot, SCSI reset, etc */
		ip->i_flags |= ISYN|IMOD;
		if (!(error = efs_iupdat(ip)))
			ip->i_flags |= INOUPDAT;
		iunlock(ip);
		return error;
	    }

	case BALLOC:
	case BFREE:
	    {
		if (!ip || bdev != ip->i_dev)
			return EINVAL;

		fs = mtoefs(ip->i_mount);

		mutex_lock(fs->fs_lock, PINOD);
		/* require that bn+len be valid data blocks */
		if (invalidblocks(fs, fa.fa_bn, fa.fa_len)) {
			mutex_unlock(fs->fs_lock);
			return EINVAL;
		}

		/* remove/add blocks from/to free "list" */
		if (cmd == BALLOC) {
			/* require that bn+len be free */
			if (efs_tstfree(fs,fa.fa_bn,fa.fa_len) != fa.fa_len) {
				mutex_unlock(fs->fs_lock);
				return EEXIST;
			}
			cg = &fs->fs_cgs[EFS_BBTOCG(fs, fa.fa_bn)];
			if (fa.fa_bn == cg->cg_firstdfree)
				cg->cg_firstdfree += fa.fa_len;
			cg->cg_dfree -= fa.fa_len;
			fs->fs_tfree -= fa.fa_len;
			fs->fs_fmod = 1;
			efs_bitalloc(fs, fa.fa_bn, fa.fa_len);
		} else {  /* cmd == FREE */
			/* require that bn+len be allocated */
			if (efs_tstalloc(fs,fa.fa_bn,fa.fa_len) != fa.fa_len) {
				mutex_unlock(fs->fs_lock);
				return EEXIST;
			}
			cg = &fs->fs_cgs[EFS_BBTOCG(fs, fa.fa_bn)];
			cg->cg_dfree += fa.fa_len;
			if (cg->cg_firstdfree > fa.fa_bn)
				cg->cg_firstdfree = fa.fa_bn;
			fs->fs_tfree += fa.fa_len;
			fs->fs_fmod = 1;
			efs_bitfree(fs, fa.fa_bn, fa.fa_len);
		}
		mutex_unlock(fs->fs_lock);
		break;
	    }

	case TSTALLOC:
	case TSTFREE:
	    {
		if (ip && bdev == ip->i_dev) {
			vfsp = 0;
			fs = mtoefs(ip->i_mount);
		} else {
			bhv_desc_t *bdp;

			vfsp = vfs_busydev(bdev, efs_fstype);
			if (vfsp == 0)
				return EINVAL;
			bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), &efs_vfsops);
			fs = bhvtoefs(bdp);
		}

		/* 'len' is rest of data blocks in cg starting at 'fa.fa_bn' */
		len = EFS_CGIMIN(fs, EFS_BBTOCG(fs, fa.fa_bn) + 1) - fa.fa_bn;

		mutex_lock(fs->fs_lock, PINOD);
		if (vfsp)
			vfs_unbusy(vfsp);
		/* require that bn+len be valid data blocks */
		if (invalidblocks(fs, fa.fa_bn, len)) {
			mutex_unlock(fs->fs_lock);
			return EINVAL;
		}

		if (cmd == TSTALLOC)
			*rval = efs_tstalloc(fs, fa.fa_bn, len);
		else
			*rval = efs_tstfree(fs, fa.fa_bn, len);
		mutex_unlock(fs->fs_lock);
		break;
	    }

	default:
		return EINVAL;
	}
	return 0;
}

static int
invalidblocks(struct efs *fs, daddr_t bn, int len)
{
	if (/* just plain wacko */
	    bn < 0 || len < 1 ||
	    /* beyond last data block in file system */
	    bn >= EFS_CGIMIN(fs, fs->fs_ncg) ||
	    /* 'bn' is in an i-list */
	    bn < EFS_CGIMIN(fs, EFS_BBTOCG(fs, bn)) + fs->fs_cgisize ||
	    /* bn+len extends out of data block area of cg */
	    EFS_BBTOCG(fs, bn) != EFS_BBTOCG(fs, bn + len - 1))
		return 1;
	return 0;
}

/*
 * Copy in fap->fa_ex and fap->fa_ix to kmem_alloc'ed space.
 * Return with fap->fa_ex,fa_ix pointing to copyed-in extents
 * in kmem_alloc'ed space.
 */
static int
exget(struct fscarg *fap)
{
	extent *ex;
	int dirbytes, indirbytes;

	/* check that numextents and num indirects not totally bogus */
	if (fap->fa_ne < 1 || fap->fa_ne > EFS_MAXEXTENTS ||
	     fap->fa_nie < 0 || fap->fa_nie > EFS_DIRECTEXTENTS) {
		return EINVAL;
	}

	dirbytes = fap->fa_ne * sizeof(extent);
	ex = kmem_alloc(dirbytes, KM_SLEEP);
	if (copyin(fap->fa_ex, ex, dirbytes)) {
		kmem_free(ex, dirbytes);
		return EFAULT;
	}
	fap->fa_ex = ex;

	if (fap->fa_nie == 0) {
		fap->fa_ix = 0;
	} else {
		indirbytes = fap->fa_nie * sizeof(extent);
		ex = kmem_alloc(indirbytes, KM_SLEEP);
		if (copyin(fap->fa_ix, ex, indirbytes)) {
			kmem_free(fap->fa_ex, dirbytes);
			kmem_free(ex, indirbytes);
			return EFAULT;
		}
		fap->fa_ix = ex;
	}
	return 0;
}

/*
 * verify that:
 *	total length of direct blocks consistent with file size
 *	ex_offset valid for direct extents
 *	check that bn+len within range for data block area of cg
 *	direct and indirect extent blocks are allocated
 *	direct extent offsets are valid
 *	indirect extents proper size for direct extents
 * Free's fap->fa_ex,fa_ix on any error.
 */
static int
exgetandverify(struct inode *ip, struct fscarg *fap)
{
	extent *ex;
	struct efs *fs = mtoefs(ip->i_mount);
	int error, offset, indirbbs, n0;

	/* fap->fa_ex,fa_ix are in the user's address space on entry */
	if (error = exget(fap))
		return error;
	ASSERT(fap->fa_ex);
	ASSERT(fap->fa_nie ? fap->fa_ix != 0 : fap->fa_ix == 0);
	/* fap->fa_ex,fa_ix are now addr's of kmem_alloced space
 	 * fap->fa_ix may be 0 */
	
	/* make sure direct extents are allocated, ex_offset valid */
	offset = 0;
	for (ex = fap->fa_ex, n0 = fap->fa_ne; n0--; ex++) {
		mutex_lock(fs->fs_lock, PINOD);
		if (invalidblocks(fs, ex->ex_bn, ex->ex_length ) ||
		     efs_tstalloc(fs, ex->ex_bn, ex->ex_length) !=
				ex->ex_length) {
			mutex_unlock(fs->fs_lock);
			goto einval;
		}
		mutex_unlock(fs->fs_lock);
		if (offset != ex->ex_offset)
			goto einval;
		offset += ex->ex_length;
	}

	/* # blocks described by extents must be consistent with i_size */
	/* (Note that we're re-using 'offset' from above!) */
	if (BTOBB(ip->i_size) != offset)
		goto einval;
	
	if (fap->fa_ne <= EFS_DIRECTEXTENTS) {
		if (fap->fa_nie != 0) /* be strict */
			goto einval;
		return 0;
	}

	/* there should be indirect extents */
	if (fap->fa_nie < 1 || fap->fa_nie > EFS_DIRECTEXTENTS)
		goto einval;

	/* validate indirect blocks */
	indirbbs = 0;
	for (ex = fap->fa_ix, n0 = fap->fa_nie; n0--; ex++) {
		mutex_lock(fs->fs_lock, PINOD);
		if (invalidblocks(fs, ex->ex_bn, ex->ex_length) ||
		     efs_tstalloc(fs, ex->ex_bn, ex->ex_length) !=
				ex->ex_length) {
			mutex_unlock(fs->fs_lock);
			goto einval;
		}
		mutex_unlock(fs->fs_lock);
		indirbbs += ex->ex_length;
	}

	/* proper number of indirect blocks to hold direct extents */
	if (BTOBB(fap->fa_ne * sizeof(extent)) != indirbbs)
		goto einval;
	
	return 0;
einval:
	kmem_free(fap->fa_ex, fap->fa_ne * sizeof(extent));
	if (fap->fa_ix)
		kmem_free(fap->fa_ix, fap->fa_nie * sizeof(extent));
	return EINVAL;
}

static void
fsr_flush(struct inode *ip)
{
	int e;

	if ((ip->i_mode & IFMT) == IFREG) {
		VOP_FLUSH_PAGES(itov(ip), (off_t)0, ip->i_size - 1, B_STALE, 
			FI_NONE, e);
	} else
	for (e = 0; e < ip->i_numextents; e++) {
		register buf_t	*bp;
		extent *ex = &ip->i_extents[e];

		bp = incore(ip->i_dev, ex->ex_bn, ex->ex_length, 1);
		if (bp)
			if (bp->b_flags & B_DELWRI)
				bwrite(bp);
			else
				brelse(bp);
	}
}

static void
fsr_inval_buf(struct inode *ip, extent *ex)
{
	register buf_t	*bp;
	bp = incore(ip->i_dev, ex->ex_bn, ex->ex_length, 1);
	if (!bp)
		return;

	/* If there's a DELWRI buffer, we need to know that the file has
	 * been modified */
	ASSERT(!(bp->b_flags & B_DELWRI) || 
		((ip->i_flags & (INOUPDAT|IMOD|IUPD|ICHG)) != INOUPDAT));

	/* if we find a DELWRI buf, the file must have been updated.  */
	if (bp->b_flags & B_DELWRI) {
		ip->i_flags &= ~INOUPDAT;
	} else {
		bp->b_flags |= B_STALE;
	}
	brelse(bp);
}

/*ARGSUSED*/
static void
fsr_invalid(struct inode *ip, struct fscarg *fap)
{
	int e;
	int i;

	ASSERT(ip->i_flags & IINCORE);
	for (i = 0; i < ip->i_numindirs; i++)
		fsr_inval_buf(ip, &ip->i_indir[i]);

	if ((ip->i_mode & IFMT) == IFREG) {
		VOP_FLUSH_PAGES(itov(ip), (off_t)0, ip->i_size - 1, B_STALE, 
			FI_NONE, e);
	} else
		for (e = 0; e < ip->i_numextents; e++)
			fsr_inval_buf(ip, &ip->i_extents[e]);
}

static int
irix5_to_fscarg(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));
	if (ABI_IS_IRIX5_N32(info->abi))
		return irix5_n32_to_fscarg(mode, to, count, info);
	else
		return irix5_o32_to_fscarg(mode, to, count, info);
}

/* ARGSUSED3 */
static int
irix5_o32_to_fscarg(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_o32_fscarg, fscarg);
	ASSERT(count == 1);
	target->fa_dev = (dev_t)source->fa_dev;
	target->fa_ino = (ino_t)source->fa_ino;
	target->fa_ne = (int)source->fa_ne;
	target->fa_nie = (int)source->fa_nie;
	target->fa_ex = (extent *)(__psint_t)source->fa_ex;
	target->fa_ix = (extent *)(__psint_t)source->fa_ix;
	target->fa_len = (int)source->fa_len;
	target->fa_bn = (daddr_t)source->fa_bn;

	return 0;
}

/* ARGSUSED3 */
static int
irix5_n32_to_fscarg(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_n32_fscarg, fscarg);
	ASSERT(count == 1);
	target->fa_dev = (dev_t)source->fa_dev;
	target->fa_ino = (ino_t)source->fa_ino;
	target->fa_ne = (int)source->fa_ne;
	target->fa_nie = (int)source->fa_nie;
	target->fa_ex = (extent *)(__psint_t)source->fa_ex;
	target->fa_ix = (extent *)(__psint_t)source->fa_ix;
	target->fa_len = (int)source->fa_len;
	target->fa_bn = (daddr_t)source->fa_bn;

	return 0;
}

