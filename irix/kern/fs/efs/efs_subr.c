/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988-1994, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Miscellaneous efs subroutines
 */
#ident	"$Revision: 3.52 $"

/*#include <values.h>*/
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include "efs_bitmap.h"
#include "efs_fs.h"
#include "efs_ino.h"
#include "efs_sb.h"

/*
 * efs_builddsum:
 *	- scan the data bitmap, counting free blocks within the cg
 *	- cg_firstdfree is a rotor we use to allow partial scanning
 *	  of the cylinder group
 *
 *	Returns error in the event of I/O errors from the bitmap functions.
 *	Returns 0 normally.
 */
int
efs_builddsum(
	register struct efs *fs,
	register struct cg *cg)
{
	register long	firstbit;
	register int	bitlen;
	register int	length;
	register int	dfree = 0;
	register int	lookfor;

	/*
	 * Don't bother scanning the data bitmap if the fs is corrupted
	 * or the cylinder group is corrupted, or if the filesystem
	 * is read only.
	 */
	 
	if (fs->fs_readonly)
		return 0;

	/*
	 * Set inode bitmap portion to appear that all inodes are free.
	 * The state will be corrected for those blocks whose inodes
	 * are all allocated, as a side effect of inode allocation.
	 * The write for the first cg needs to be synchronous, to detect
	 * readonly disks. Thereafter, it can be async, for speed.
	 */
	firstbit = EFS_ITOBB(fs, cg->cg_firsti);
	bitlen = fs->fs_ipcg / EFS_INOPBB;
	ASSERT(firstbit + bitlen <= cg->cg_firstdbn);

	if (cg == &fs->fs_cgs[0]) {
		if (efs_bitfreesync(fs, firstbit, bitlen) == -1)
			return EIO;
	} else
		efs_bitfree(fs, firstbit, bitlen);

	firstbit = cg->cg_firstdbn;
	bitlen = (cg->cg_firstbn + fs->fs_cgfsize) - firstbit;

	/*
	 * Make sure we are scanning an appropriate amount of the bitmap, at
	 * the right place.
	 */
	ASSERT(bitlen > 0);
	ASSERT(bitlen <= fs->fs_cgfsize - fs->fs_cgisize);
	ASSERT(((cg - &fs->fs_cgs[0]) >= 0) && \
		((cg - &fs->fs_cgs[0]) < fs->fs_ncg));
	ASSERT(cg->cg_firstdbn == fs->fs_firstcg + \
		(cg - &fs->fs_cgs[0]) * fs->fs_cgfsize + fs->fs_cgisize);

	/*
	 * Now scan bitmap.
	 * Compute cg_firstdfree and cg_dfree.
	 */

	length = efs_tstalloc(fs, firstbit, bitlen);
	if (length < 0)		/* Bitmap I/O error */
		return (EIO);
	cg->cg_firstdfree = firstbit + length;

	if (length == bitlen) {
		cg->cg_dfree = 0;
		return 0;
	}

	firstbit += length;
	bitlen -= length;

	dfree = 0;

	lookfor = 1;
	while (bitlen) {
		if (lookfor) {
			length = efs_tstfree(fs, firstbit, bitlen);
			if (length < 0)		/* Bitmap I/O error */
				return (EIO);
			dfree += length;
			ASSERT(length);
			lookfor = 0;
		} else {
			length = efs_tstalloc(fs, firstbit, bitlen);
			if (length < 0)		/* Bitmap I/O error */
				return (EIO);
			lookfor = 1;
		}
		firstbit += length;
		bitlen -= length;
	}

	cg->cg_dfree = dfree;
	return 0;
}

#ifdef EFSDEBUG
#include "efs_inode.h"

/*
 * efs_badextent:
 *	- make sure that the extent doesn't overlap anything except
 *	  data blocks
 */
/* ARGSUSED */
efs_badextent(
	register struct efs *fs,
	register extent *ex,
	register struct inode *ip)
{
	register efs_daddr_t bn;
	register int len;
	int foundlen;
	char	*errstr = NULL;

#ifdef	DEBUG
	if ((ex->ex_bn == 0) || (ex->ex_length == 0)) {
		prdev("ip=%x ex=%x", fs->fs_dev, ip, ex);
		cmn_err(CE_PANIC, "efs oops");
	}
#endif

	/* ensure that bn,len are reasonable */
	if ((ex->ex_bn < fs->fs_firstcg) || (ex->ex_length == 0) ||
	    (ex->ex_bn > fs->fs_size) || (ex->ex_length > EFS_MAXEXTENTLEN)) {
		prdev("bn/len bad: bad extent, inum=%d, ip=%x bn=%d len=%d",
		    fs->fs_dev,
		    ip ? ip->i_number : 0, ip, ex->ex_bn, ex->ex_length);
		goto bad;
	}

	/* make sure extent doesn't overlap inodes */
	bn = ex->ex_bn - fs->fs_firstcg;
	len = ex->ex_length - 1;
	if ((bn % fs->fs_cgfsize) < fs->fs_cgisize) {
		prdev("inode overlap: bad extent, inum=%d, ip=%x bn=%d len=%d",
		    fs->fs_dev,
		    ip ? ip->i_number : 0, ip, ex->ex_bn, ex->ex_length);
		goto bad;
	}

	/* make sure that extent doesn't cross a cg boundary */
	if ((bn / fs->fs_cgfsize) != ((bn + len) / fs->fs_cgfsize)) {
		prdev("crossing cg: bad extent, inum=%d, ip=%x bn=%d len=%d",
		    fs->fs_dev,
		    ip ? ip->i_number : 0, ip, ex->ex_bn, ex->ex_length);
		goto bad;
	}

	/*
	 * Check bn against bitmap.  Ensure that the extent doesn't overlap
	 * some free blocks.
	 */

	foundlen = efs_tstalloc(fs, (int) ex->ex_bn, (int) ex->ex_length);

	if (foundlen != ex->ex_length) {
		prdev("bitmap messed up, foundlen=%d bitlen=%d: bad extent, inum=%d, ip=%x bn=%d len=%d",
		    fs->fs_dev, foundlen, ex->ex_length,
		    ip ? ip->i_number : 0, ip, ex->ex_bn, ex->ex_length);
		goto bad;
	}
	return (0);

bad:
	/*
	 * Mark the filesystem as corrupted so that the user will be
	 * forced to unmount and fsck it.
	 */
	fs->fs_corrupted = 1;
	efs_error(fs, 0, EIO);
	return (1);
}
#endif

/*
 * print out an error describing the problem with the fs
 */
void
efs_error(struct efs *fs, int type, int error)
{
	register dev_t dev;

	dev = fs->fs_dev;
	if (fs->fs_corrupted)
		prdev("Corrupt filesystem", dev);

	switch (type) {
	  case 0:
		if (fs->fs_tfree) {
			if (error != EDQUOT)
				prdev("Process [%s] ran "
					"out of contiguous space",
					dev, get_current_name());
		} else
			prdev("Process [%s] ran out of disk space",
				dev, get_current_name());
		break;
	  case 1:
		if (fs->fs_tinode)
			prdev("Inode corruption", dev);
		else
			prdev("Out of inodes", dev);
		break;
	}
}

/*
 * efs_setcorrupt: called to mark the efs filesystem on dev corrupt.
 * It is assumed that the caller is holding some fs resource (e.g., an
 * inode or buffer) which will prevent unmounting of the fs while this
 * is in progress, so no need to lock the mount table.
 */
void
efs_setcorrupt(struct mount *mp)
{
	if (mp)
		mtoefs(mp)->fs_corrupted = 1;
}

/*
 * Check function to do appropriate things if an asynchronous write of
 * meta-data fails. It is inserted as the b_relse field of a 
 * buffer by any efs code which considers that a failure writing that
 * buffer would compromise the filesystem.
 * Actions: it locates the superblock and marks it corrupted. 
 * It prints a warning message on the console. Also the buffer is set back to 
 * B_DELWRI status: this ensures that the system's view of the data remains 
 * consistent & ensures another retry.
 */
void
efs_asynchk(register struct buf *bp)
{
	register dev_t dev;
	static dev_t lastdev = 0;
	static time_t lasttime = 0;
	extern int	efs_fstype;

	if ((bp->b_flags & B_STALE) || 
	    (bp->b_flags & (B_ERROR | B_DELWRI)) == 0) {
		/*
		 * We throw stale bufs, error or not; otherwise junk
		 * could be left in the cache after unmounting.
		 * And if this is an actual write completion
		 * (DELWRI not set) with NO error, we no longer 
		 * need the check function.
		 */
		bp->b_relse = NULL;
		brelse(bp);
		return;
	}

	if (bp->b_flags & B_ERROR) {
		struct vfs *vfsp;
		bhv_desc_t *bdp;

		dev = bp->b_edev;
		if (dev != lastdev || lbolt - lasttime > 500) {
			prdev("Write error.\nFilesystem on device may be corrupted: unmount and fsck it.", dev);
			lasttime = lbolt;
		}
		lastdev = dev;

		vfsp = vfs_devsearch(dev, efs_fstype);
		bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), &efs_vfsops);
		efs_setcorrupt(bhvtom(bdp));

		bp->b_error = 0;
		bp->b_flags |= B_DELWRI;
		bp->b_flags &= ~B_ERROR;
	}

	bp->b_flags |= B_RELSE;
	brelse(bp);
}
