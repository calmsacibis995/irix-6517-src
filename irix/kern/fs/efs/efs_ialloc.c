/*
 * Inode/data allocation/de-allocation for the efs.
 *
 * $Revision: 3.42 $
 */
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/quota.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
#include "efs_bitmap.h"
#include "efs_fs.h"
#include "efs_inode.h"
#include "efs_sb.h"

extern void efs_error(struct efs *, int, int);
extern int  efs_sbupdate(struct efs *fs, int sync);

/*
 * flag values to control behaviour of scanchunk w.r.t cg summary
 */
enum updatehow { DONT_UPDATE_CG_LOWI, UPDATE_CG_LOWI };

/*
 * efs_goodcg:
 *	- given a cg which presumably has some free inodes on it,
 *	  figure out if this is a good place to put an inode
 */
static int
efs_goodcg(register struct efs *fs, register struct cg *cg, mode_t mode)
{
	switch (mode & IFMT) {
	  case IFREG:
	  case IFLNK:
		if (cg->cg_dfree >= fs->fs_minfree)
			return (1);
		break;
	  case IFDIR:
		/*
		 * Insure that there are at least fs_mindirfree blocks in
		 * the cg before placing a directory in the cg.  This tries
		 * to insure that a new directory will have room for its
		 * child inodes and child data files.
		 */
		if (cg->cg_dfree >= fs->fs_mindirfree)
			return (1);
		break;
	  default:
		/*
		 * For files which need no data blocks, don't bother checking
		 * to see if the cg has any.
		 */
		return (1);
	}
	return (0);
}

/*
 * efs_pickcg:
 *	- given the filesystem and the parent inode of an inode being
 *	  created, pick a good cg to place the inode in
 */
static struct cg *
efs_pickcg(register struct efs *fs, register struct inode *pip, mode_t mode)
{
	register int cgs_to_try;
	register struct cg *cg;
	register short cgnum, left, right;
	int ni = fs->fs_ipcg;

	/*
	 * Search radially outward in either direction for a cg to place
	 * the inode in.  This knows that we have already tried the parent's
	 * cg in efs_ialloc().  Given a cg which may have some inodes in
	 * it, try to see if its a good place to put a new inode based
	 * on external criteria.
	 */
	cgnum = EFS_ITOCG(fs, pip->i_number);
	cgs_to_try = fs->fs_ncg - 1;
	left = cgnum - 1;
	right = cgnum + 1;
	while (cgs_to_try) {
		if (left >= 0) {
			cg = &fs->fs_cgs[left];
			if (cg->cg_lowi < cg->cg_firsti + ni &&
			    efs_goodcg(fs, cg, mode))
				return (cg);
			cgs_to_try--;
			left--;
		}
		if (right < fs->fs_ncg) {
			cg = &fs->fs_cgs[right];
			if (cg->cg_lowi < cg->cg_firsti + ni &&
			    efs_goodcg(fs, cg, mode))
				return (cg);
			cgs_to_try--;
			right++;
		}
	}

	/*
	 * Okay.  We couldn't find any cylinder groups which had both free
	 * inodes and free data blocks.  Now search for a cylinder group
	 * that has at least one free inode.
	 */
	cg = &fs->fs_cgs[0];
	for (cgs_to_try = fs->fs_ncg; --cgs_to_try >= 0; cg++) {
		if (cg->cg_lowi < cg->cg_firsti + ni)
			return (cg);
	}

	/*
	 * Oh well. No inodes no-where-no-how.
	 */
	return (NULL);
}

/*
 * efs_scanchunk:
 *	- scan the given chunk trying to find a free inode
 *	- ino is assumed here to be at the base of a chunk boundary
 *	- return an errno if an error occurs, or if there are no free inodes
 *	  to be found in the chunk; return 0 otherwise
 *	- return a locked inode pointer via ipp if an inode is found free
 *	- the main reason we do this scanning, instead of just doing a
 *	  bunch of iget's is to avoid flushing the inode cache during
 *	  a file allocation
 */
static int
efs_scanchunk(
	struct inode *pip,
	register struct cg *cg,
	register efs_ino_t inum,
	enum updatehow how,
	struct inode **ipp)
{
	register struct efs_dinode *dp;
	register struct efs *fs;
	register struct buf *bp;
	register struct inode *ip;
	register int j, k;
	register efs_daddr_t bn;
	int len;
	int lasti;
	efs_ino_t lowi, highi;
	volatile unsigned gen = cg->cg_gen;
	int error;

	/*
	 * Determine the location of the inode chunk on the disk.
	 */
	fs = itoefs(pip);
	lasti = cg->cg_firsti + fs->fs_ipcg - 1;
	bn = EFS_ITOBB(fs, inum);
	len = fs->fs_inopchunkbb;
	if (bn + fs->fs_inopchunkbb > cg->cg_firstdbn)
		len = cg->cg_firstdbn - bn;
#ifdef	DEBUG
	if ((bn < cg->cg_firstbn) || (bn + len > cg->cg_firstdbn)) {
		printf(
		    "efs_scanchunk: bn=%d inumber=%d inopchunk=%d firsti=%d\n",
				    bn, inum, fs->fs_inopchunk,
				    cg->cg_firsti);
	}
#endif

	/*
	 * If none of the inodes in this inode chunk are free, skip it.
	 * It probably isn't worth checking each bb's bitmap bit in the
	 * loop below since we're reading in the entire chunk anyway.
	 */
	if (efs_tstalloc(fs, bn, len) == len) {
		if (cg->cg_lowi >= inum && cg->cg_lowi < inum+fs->fs_inopchunk)
			cg->cg_lowi = inum + fs->fs_inopchunk;
		return ENOSPC;
	}
again:
	/*
	 * Randomize inode within the chunk.  We don't want every
	 * process creating an entity to compete for the same
	 * inode -- only one wins, and the others must try again
	 * after iget'ing the iput'ing the inode.
	 */
	inum = lowi = EFS_ITOCHUNKI(fs, cg, inum);
	highi = lowi + fs->fs_inopchunk;
	inum += ++private.p_hand & (fs->fs_inopchunk - 1);

	/*
	 * Put this baby on a BB boundary to make bitmaps work correctly.
	 */
	inum &= ~EFS_INOPBBMASK;

	/*
	 * Start at inum, unless inum is < 2 (i.e., 0 or 1).  We never
	 * check 0 or 1, because those inodes are historically unusable.
	 */
	if (inum < 2)
		inum = 2;
	bp = NULL;
	/*
	 * Logically break scan of inode chunk into basic blocks
	 * since the bitmap operates at that level.
	 */
	for (j = 0; j < fs->fs_inopchunkbb; j++) {
		for (k = EFS_INOPBB; k > 0; k--) {
			/*
			 * If we're past current chunk or the cylinder group,
			 * go back to beginning of the chunk.
			 */
			if (inum >= highi || inum > lasti) {
				inum = lowi;
				if (inum < 2) {
					inum = 2;
					k = EFS_INOPBB - 2;
				}
			}
			if (bp == NULL) {
				IGETINFO.ig_iallocrd++;
				bp = bread(fs->fs_dev, bn, len);
				if (bp->b_flags & B_ERROR) {
					error = bp->b_error;
					brelse(bp);
					ASSERT(error);
					return error;
				}
				if (bp->b_flags & B_FOUND)
					IGETINFO.ig_iallocrdf++;
			}
			dp = (struct efs_dinode *)bp->b_un.b_addr + (inum-lowi);
			if (dp->di_mode == 0) {
				/*
				 * We have to release the buffer here,
				 * because iget will need it when it does
				 * an iread().  We release the file
				 * system lock, too: this is not a deadlock
				 * requirement but should improve concurrency.
				 */
				brelse(bp);
				bp = NULL;
				mutex_unlock(fs->fs_lock);
				error = iget(pip->i_mount, inum, ipp);
				ip = *ipp;
				if (error || ip->i_mode == 0) {
					mutex_lock(fs->fs_lock, PINOD);
					return error;
				}
				IGETINFO.ig_ialloccoll++;
				iput(ip);
				mutex_lock(fs->fs_lock, PINOD);
				goto again;
			}
			inum++;
		}

		/*
		 * No inodes found in this bb.  Check generation number:
		 * if it has changed, our info is not up to date, and we
		 * shouldn't set any summary info.
		 * Also, we should not set the summary info if called directly
		 * from efs_ialloc, since then we're not starting from cg_lowi.
		 * Since we randomised the starting inum, we must set the
		 * bitmap based on inum, not position in the outer loop.
		 * inum must point just past this bb.  If lowi had pointed
		 * into this bb we can set lowi to present inum.
		 */
		if (how == UPDATE_CG_LOWI && gen == cg->cg_gen) {
			efs_bitalloc(fs, bn + (inum - lowi - 1)/EFS_INOPBB, 1);
			if (cg->cg_lowi >= inum - EFS_INOPBB
			    && cg->cg_lowi < inum)
				cg->cg_lowi = inum;
		}
	}
	if (bp)
		brelse(bp);

	if (how == UPDATE_CG_LOWI && gen == cg->cg_gen) {
		/*
		 * No inodes found in chunk. If gen hasn't changed, we know
		 * the lowest free inode is above this chunk, and can advance
		 * lowi accordingly -- unless we were called directly from
		 * efs_ialloc, in which case we didn't start from cg_lowi.
		 */
		cg->cg_lowi = highi;
	}
	return ENOSPC;
}

/*
 * efs_scancg:
 *	- scan an entire cg for a free inode, starting at the low point
 *	  kept up to date in cg->cg_lowi.
 *	- if an inode is found free, then a pointer into the inode table
 *	  is returned, and the inode is locked.
 */
static int
efs_scancg(struct inode *pip, register struct cg *cg, struct inode **ipp)
{
	register struct efs *fs;
	register int nchunks;
	register efs_ino_t lowi, highi;
	int error = ENOSPC;

	fs = itoefs(pip);
	lowi = EFS_ITOCHUNKI(fs, cg, cg->cg_lowi);
	highi = EFS_ITOCHUNKI(fs, cg, cg->cg_firsti + fs->fs_ipcg - 1)
			+ fs->fs_inopchunk - 1;

	/*
	 * Figure out how many chunks we will need to examine.
	 * Because lowi was rounded down to the base of a chunk, and
	 * highi was rounded up, we don't need to do any rounding
	 * here.  Do add one here, because highi is inclusive.
	 * We tell scanchunk to maintain cg_lowi here, since we're
	 * starting from current lowi.
	 */
	nchunks = (highi - lowi + 1) / fs->fs_inopchunk;

	while (--nchunks >= 0) {
		error = efs_scanchunk(pip, cg, lowi, UPDATE_CG_LOWI, ipp);
		if (!error)
			return 0;
		lowi += fs->fs_inopchunk;
	}

	return error;
}

/*
 * efs_ialloc:
 *	- allocate an inode on the given filesystem
 */
int
efs_ialloc(struct inode *pip, mode_t mode, short nlink, dev_t rdev,
	   struct inode **ipp, struct cred *cr)
{
	struct inode *ip = NULL;
	register struct efs *fs;
	register struct cg *cg;
	register efs_ino_t lowinum;
	int error;

	/* Prevent long user-ids from being silently truncated to 16bits */
	if (cr->cr_uid > 0xffff || cr->cr_gid > 0xffff)
		return(EOVERFLOW);


	if (error = qt_chkiq(pip->i_mount, (struct inode *)NULL, cr->cr_uid, 0))
		goto quota_inodes;

#ifdef _SHAREII
	/*
	 *	ShareII takes inodes into account; they get charged at a different
	 *	rate from whole disk blocks, but appear as normal disk usage
	 *	(there's no separate limit on inodes).
	 */
	    if (error = SHR_LIMITDISK(itovfs(pip), cr->cr_uid, 1UL, 0, LI_ALLOC|LI_UPDATE|LI_ENFORCE, NULL, cr))
		goto quota_inodes;
#endif /* _SHAREII */

	error = ENOSPC;

	fs = itoefs(pip);
	mutex_lock(fs->fs_lock, PINOD);

	if (fs->fs_tinode == 0)
		goto out_of_inodes;

	/*
	 * First try to use an inode that is in the same inode chunk
	 * as the parent inode.
	 */
	cg = &fs->fs_cgs[EFS_ITOCG(fs, pip->i_number)];

	if (cg->cg_lowi < cg->cg_firsti + fs->fs_ipcg) {
		/*
		 * Figure out low inode number in the chunk that the
		 * parents inode number is in.  If the low allocation
		 * mark for the cylinder group is smaller than the end
		 * of the chunk + 1, then there might be some free inodes
		 * in the chunk.  Otherwise, we know for sure that there
		 * are no free inodes in the chunk, and we don't bother
		 * to scan the chunk.
		 * Since we're not starting from current lowi, scanchunk
		 * should not touch cg_lowi here.
		 */
		lowinum = EFS_ITOCHUNKI(fs, cg, pip->i_number);
		if (cg->cg_lowi < lowinum + fs->fs_inopchunk) {
			error = efs_scanchunk(pip, cg, lowinum,
					      DONT_UPDATE_CG_LOWI, &ip);
		}
	}

	if (error) {
		/*
		 * Sigh.  Optimal placement didn't work.  Try to place the
		 * inode in the same cg as the parent.  If the parents cg is
		 * full, then pick a good neighboring cg to use.
		 */
		do {
			if (cg->cg_lowi >= cg->cg_firsti + fs->fs_ipcg
			    && (cg = efs_pickcg(fs, pip, mode)) == NULL) {
				ASSERT(fs->fs_tinode == 0);
				goto out_of_inodes;
			}
		} while (error = efs_scancg(pip, cg, &ip));
	}

	/*
	 * We found an inode.  Initialize the inode in a generic manner.
	 */
	fs->fs_tinode--;
	fs->fs_fmod = 1;
	mutex_unlock(fs->fs_lock);
	if (error = efs_icreate(ip, mode, nlink, rdev, pip, cr)) {
		iput(ip);
#ifdef _SHAREII
		/*
		 * Note we didn't actually allocate the inode -- undo the 
		 * charge for it.
		 * Where does the efs undo its actions?
		 */
		(void)SHR_LIMITDISK(itovfs(pip), cr->cr_uid, 1UL, 0, LI_FREE|LI_UPDATE, NULL, cr);
#endif /* _SHAREII */
		return error;
	}
	*ipp = ip;
	return 0;

out_of_inodes:
	/*
	 * >LOOK
	 * I see no inodes here.
	 * >QUIT
	 * Your score is 0 out of a possible 560.
	 */
	fs->fs_fmod = 1;
	efs_error(fs, 1, error);
	if (!error)
		error = ENOSPC;
	mutex_unlock(fs->fs_lock);

#ifdef _SHAREII
	/*
	 * Note we didn't actually allocate the inode -- undo the 
	 * charge for it.
	 */
	(void)SHR_LIMITDISK(itovfs(pip), cr->cr_uid, 1UL, 0, LI_UPDATE|LI_FREE, NULL, cr);
#endif /* _SHAREII */

quota_inodes:
	return error;
}

/*
 * efs_ifree:
 *	- free the specified I node on the specified filesystem
 *	- if the inode being free'd is earlier than the current
 *	  lowest numbered inode, then note that in cg_lowi.
 *	  This attempts to keep inodes packed in the front of the
 *	  cylinder group, so as to improve the inode caching done
 *	  via the buffer cache
 *	  Also advance the cg "generation number" to avoid the danger
 *	  of concurrent iallocs overwriting this & so losing inodes.
 */
void
efs_ifree(register struct inode *ip)
{
	register struct efs *fs;
	register struct cg *cg;

#ifdef _SHAREII
	/*
	 *	ShareII takes inodes into account; they get charged
	 *	at a different rate from whole disk blocks.
	 */
	SHR_LIMITDISK(itovfs(ip), ip->i_uid, 1UL, 0, LI_FREE|LI_UPDATE, NULL, NULL);
#endif /* _SHAREII */

	(void) qt_chkiq(ip->i_mount, ip, (u_int)ip->i_uid, 0);
	qt_dqrele(ip->i_dquot);
	ip->i_dquot = NULL;
	fs = itoefs(ip);
	mutex_lock(fs->fs_lock, PINOD);
	fs->fs_fmod = 1;
	fs->fs_tinode++;
	cg = &fs->fs_cgs[EFS_ITOCG(fs, ip->i_number)];
	if (ip->i_number < cg->cg_lowi)
		cg->cg_lowi = ip->i_number;
	cg->cg_gen++;
	efs_bitfree(fs, EFS_ITOBB(fs, ip->i_number), 1);
	mutex_unlock(fs->fs_lock);
}
