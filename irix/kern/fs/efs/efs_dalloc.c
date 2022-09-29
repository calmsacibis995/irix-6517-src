/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * inode/data allocation for the efs.
 */
#ident "$Revision: 3.60 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/immu.h>		/* for NDPP */
#include <sys/param.h>
#include <sys/quota.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
#include "efs_bitmap.h"
#include "efs_fs.h"
#include "efs_inode.h"
#include "efs_sb.h"

extern int efs_findfree(struct efs *, int, int, struct bmapval *);
static void efs_btoss(struct inode *, extent *, int);

/*
 * efs_alloc:
 *	- allocate len worth of space on fs for inode ip
 *	- if we can't find len's worth, then return in ex
 *	  the amount we did find.
 *	- fill in ex with the new information
 *	- return 0 on success, errno on failure.
 *	- musthave means that we cannot compromise desiredlen
 */
int
efs_alloc(
	struct inode	*ip,
	extent		*ex,
	register int	desiredlen,
	int		musthave,
	struct bmapval	*wex)
{
	register struct cg *cg;
	struct cg	*lastcg;
	struct efs	*fs;
	register int	i;
	register int	firstbit;
	register int	bitlen;
	struct bmapval	save;
	int		firstbn, lastbn;
	int		wrapped;
	int		ncgs;
	int		quota_len;
	int		error;
	int		spaceleft;
#ifdef _SHAREII
	int		share_len;	
#endif /* _SHAREII */
	
	ASSERT(desiredlen > 0);

#ifdef _SHAREII
	/*
	 * Check with share and update user's lnode.  We may not
	 * actually get the blocks, but we'll fix that up later.
	 * Normally share does the check and allocation separately,
	 * but this leads to a race-condition, and makes us always
	 * call it twice, rather than just on failure.
	 *
	 * If the allocation fails, SHlimitDisk
	 * returns how much space is available for allocation
	 * which is then used for another try.
	 *
	 * Note that the amount of disk the user is allowed to
	 * have is the minimum of what share allows and what quotas
	 * allow, if they are both active.  Note that desiredlen is
	 * reduced to the Share limit if necessary, which is then
	 * passed to the quota system for validation (and possible
	 * further limiting).
	 */
	if (SHR_ACTIVE)
	{
		unsigned long remain;
		
		while(error = SHR_LIMITDISK(itovfs(ip),
					  ip->i_uid,
					  (u_long)desiredlen,
					  DEV_BSIZE,
					  LI_ALLOC | LI_ENFORCE | LI_UPDATE,
					  &remain,
					  NULL))
		{
			if (!musthave && remain != 0)
			{
				/*
				 * Make this a multiple of page size if possible.
				 */
				desiredlen = remain;
				if (desiredlen > NDPP)
					desiredlen -= desiredlen % NDPP;

				ASSERT(desiredlen <= remain);
			} else
				goto out_of_quota;
		}
		share_len = desiredlen;
		spaceleft = remain;
	}
#endif /* _SHAREII */

	while (error = qt_chkdq(ip, (long)desiredlen, 0, &spaceleft)) {
		/* 
		 * A smaller allocation may succeed.
		 */
		ASSERT(spaceleft < desiredlen);
		if (!musthave && spaceleft) {
			/*
			 * Make this a multiple of page size if possible.
			 */
			while (desiredlen > spaceleft)
				desiredlen -= NDPP;
			if (desiredlen <= 0)
				desiredlen = spaceleft;
		} else
			goto out_of_quota;
	}
	quota_len = desiredlen;

	fs = mtoefs(ip->i_mount);
	mutex_lock(fs->fs_lock, PINOD);

	if (fs->fs_tfree <= 0)
		goto no_more_room;

	lastcg = &fs->fs_cgs[fs->fs_ncg];

	/*
	 * If we've got to have it, look at all cgs.
	 * Otherwise, scan half the file
	 * system or 600,000 blocks, whichever is less.
	 */
	if (musthave) {
		ncgs = fs->fs_ncg;
	} else {
		ncgs = (fs->fs_ncg + 1) / 2;
		i = 600000 / fs->fs_cgfsize;
		ncgs = MIN(i, ncgs);
	}
	/*
	 * If we're having trouble finding things, then
	 * go right to the rotor rather than starting near
	 * our inode.
	 */
	if (fs->fs_diskfull) {
		firstbn = 0;
		lastbn = 0;
		wrapped = 0;
		save.bn = 0;
		save.length = 0;
		save.offset = desiredlen;
		goto rotor;
	}

	/*
	 * For the first cg, ignore blocks preceding the last
	 * extent's blocks.
	 */
	if (ip->i_numextents > 0) {
		extent	*lex = &ip->i_extents[ip->i_numextents-1];
		firstbit = lex->ex_bn;
		cg = &fs->fs_cgs[EFS_BBTOCG(fs, firstbit)];
		/*
		 * Have to do this *after* cg calculation so it
		 * doesn't overflow into nonexistent cg when
		 * extent maps the last blocks of the fs.
		 */
		firstbit += lex->ex_length;
		if (firstbit < cg->cg_firstdfree)
			firstbit = cg->cg_firstdfree;
		if (firstbit >= cg->cg_firstbn + fs->fs_cgfsize) {
			if (++cg >= lastcg)
				cg = &fs->fs_cgs[0];
			firstbit = cg->cg_firstdfree;
		}
		if (cg >= lastcg) {
			printf("botch: fs %x cg %x lastcg %x fb %x lex %x",
				fs, cg, lastcg, firstbit, lex);
			
		}
		ASSERT(cg < lastcg);
	} else {
		cg = &fs->fs_cgs[EFS_ITOCG(fs, ip->i_number)];
		firstbit = cg->cg_firstdfree;
		ASSERT(cg < lastcg);
	}
	firstbn = firstbit;

	save.bn = 0;
	save.length = 0;
	save.offset = desiredlen;

	/*
	 * In the degenerate case of a fs with one cylinder group,
	 * or the musthave case, we want the start cylinder group
	 * to be searched from cg_firstdfree also and not based on the
	 * lex->ex_bn since none of the free blocks may be between
	 * lex->ex_bn and (cg_firstbn+fs_cgfsize).
	 */
	i = ncgs + 1;
	while (--i >= 0) {
		if (cg->cg_dfree) {
			bitlen = cg->cg_firstbn + fs->fs_cgfsize - firstbit;
			if (bitlen > fs->fs_cgfsize - fs->fs_cgisize) {
				printf("botch: fs %x cg %x firstbit %x",
					fs, cg, firstbit);
			}
			ASSERT(bitlen <= fs->fs_cgfsize - fs->fs_cgisize);
			if (efs_findfree(fs, firstbit, bitlen, &save)) {
				firstbit = save.bn;
				ASSERT(save.length >= desiredlen);
				goto found;
			}
		}

		/*
		 * When cg is full, firstdfree it set to firstbn of NEXT cg.
		 * So set lastbn to firstdbn+cgfsize so there won't ever
		 * be an accidental match in ``wrapped'' calculation.
		 */
		lastbn = cg->cg_firstdbn + fs->fs_cgfsize;

		if (++cg >= lastcg)
			cg = &fs->fs_cgs[0];

		firstbit = cg->cg_firstdfree;
	};

	ASSERT(!musthave || ncgs == fs->fs_ncg);

	/*
	 * If we've looked at all the cgs and we found at least something,
	 * then go with what we've got without looking further.
	 */
	if (ncgs == fs->fs_ncg)
		goto searched_all;

	/*
	 * Don't bother to look at cgs that were examined
	 * during the first loop.
	 */
	if (lastbn > firstbn)
		wrapped = 0;
	else {
		wrapped = 1;
		i = lastbn;
		lastbn = firstbn;
		firstbn = i;
	}

rotor:
	cg = fs->fs_cgrotor;
	firstbit = cg->cg_firstdfree;
	i = fs->fs_ncg;
	while (--i >= 0) {
		/*
		 * Don't bother to look at cgs that were examined
		 * during the first loop.
		 */
		if (wrapped) {
			if (firstbit < firstbn || firstbit >= lastbn)
				goto nextcg;
		} else {
			if (firstbit < lastbn && firstbit >= firstbn)
				goto nextcg;
		}

		if (cg->cg_dfree) {
			bitlen = cg->cg_firstbn + fs->fs_cgfsize - firstbit;
			ASSERT(bitlen <= fs->fs_cgfsize - fs->fs_cgisize);
			if (efs_findfree(fs, firstbit, bitlen, &save)) {
				firstbit = save.bn;
				ASSERT(save.length >= desiredlen);
				/*
				 * Set the rotor to where we found
				 * what we need.
				 */
				fs->fs_cgrotor = cg;
				goto found;
			}

			/*
			 * Stop if we've searched ncgs cylinder groups
			 * and have found anything.
			 */
			if (--ncgs <= 0 && save.length)
				break;
		}
nextcg:
		if (++cg >= lastcg)
			cg = &fs->fs_cgs[0];

		firstbit = cg->cg_firstdfree;
	};

searched_all:
	/*
	 * If we're here we know we did not find as much as
	 * we were looking for.  Set fs_diskfull so the fs
	 * will take whatever it can get for a while.  This should
	 * speed up allocations in a highly fragmented fs.
	 */
	if (fs->fs_diskfull == 0)
		fs->fs_diskfull = lbolt;

	if (musthave || save.length == 0)
		goto no_more_room;

	/*
	 * Found a piece to use, but it isn't the right length.
	 * Truncate desiredlen so the new block will map up to a page
	 * boundary, if possible.  We have to assume that the caller
	 * is requesting enough blocks to map to a page boundary.
	 */
	ASSERT(save.length < desiredlen);
	while (desiredlen > save.length)
		desiredlen -= NDPP;
	if (desiredlen <= 0)
		desiredlen = save.length;
	firstbit = save.bn;
	cg = &fs->fs_cgs[EFS_BBTOCG(fs, firstbit)];
	fs->fs_cgrotor = cg;

found:
	ASSERT((firstbit - fs->fs_firstcg) % fs->fs_cgfsize >= fs->fs_cgisize);
	ASSERT(desiredlen == efs_tstfree(fs, firstbit, desiredlen));

	efs_bitalloc(fs, firstbit, desiredlen);
	if (firstbit == cg->cg_firstdfree)
		cg->cg_firstdfree += desiredlen;
	cg->cg_dfree -= desiredlen;
	ASSERT(cg->cg_dfree >= 0);
	fs->fs_tfree -= desiredlen;
	fs->fs_fmod = 1;
	ASSERT(fs->fs_dirty == EFS_ACTIVEDIRT ||  \
		fs->fs_ncg != 1 || \
		fs->fs_tfree == cg->cg_dfree);
	mutex_unlock(fs->fs_lock);
	if ((doexlist_trash & 1) && (firstbit == 0 || desiredlen == 0)) {
		prdev("efs_alloc: zero length or block I=%d",
			ip->i_dev, ip->i_number);
		cmn_err(CE_PANIC, "efs_alloc: exlist trash");
	}

	ex->ex_bn = firstbit;
	ex->ex_length = desiredlen;

	/*
	 * Return info if caller is interested.
	 */
	if (wex) {
		wex->bn = firstbit;
		wex->length = desiredlen;
	}
#ifdef _SHAREII
	/*
	 * We've already committed the change to the lnode at the start,
	 * but if we ended up allocating less than that we give it back
	 * to the lnode for further allocation.
	 */
	if (SHR_ACTIVE && (share_len - desiredlen) != 0)
	{
		SHR_LIMITDISK(itovfs(ip),
			    ip->i_uid,
			    (u_long)(share_len - desiredlen),
			    DEV_BSIZE,
			    LI_UPDATE | LI_FREE,
			    NULL,
			    NULL);
	}
#endif /* _SHAREII */
	if ((quota_len - desiredlen) != 0)
		(void) qt_chkdq(ip, (long)-(quota_len - desiredlen), 0, NULL);

	return (0);

	/*
	 * Out of space.  Thats the way the partition crumbles.
	 */
no_more_room:
	(void) qt_chkdq(ip, (long)(-desiredlen), 0, NULL);
#ifdef _SHAREII
	/*
	 * We relinquish all the disk previously claimed so the user
	 * can fill up another partition.
	 */
	SHR_LIMITDISK(itovfs(ip), ip->i_uid, (u_long)share_len,
			    DEV_BSIZE, LI_FREE|LI_UPDATE, NULL, NULL);
#endif /* _SHAREII */
	error = ENOSPC;
	mutex_unlock(fs->fs_lock);

out_of_quota:
	return error;
}

/*
 * efs_move:
 *	- reallocate ``ex->ex_length + add'' worth of space on fs
 *	  for inode ip
 *	- if we can't find room in the same cylinder group as ex's,
 *	  return 0, otherwise return 1
 *	- copy data from old backing store to new and purge old buffer
 */
int
efs_move(
	register struct inode *ip,
	register extent *ex,
	register int add)
{
	register struct cg *cg;
	struct efs	*fs;
	register int	firstbit;
	register int	bitlen;
	register buf_t	*obp, *nbp;
	register int	want;
	struct bmapval	save;

	ASSERT(itov(ip)->v_type != VREG);

	if (ex->ex_length + add > EFS_MAXEXTENTLEN ||
#ifdef _SHAREII
	    /*
	     * Check with share that this is OK, and claim the space
	     * from the user's lnode.  If we don't end up using it all
	     * later we'll give it back.
	     */
	    SHR_LIMITDISK(itovfs(ip),
					  ip->i_uid,
			  (u_long)add,
					  DEV_BSIZE,
			  LI_ALLOC|LI_ENFORCE|LI_UPDATE,
					  NULL,
			  NULL)
	    ||
#endif /* _SHAREII */
	    qt_chkdq(ip, (long)add, 0, (int *)NULL)
	    )
		return 0;

	fs = mtoefs(ip->i_mount);
	cg = &fs->fs_cgs[EFS_BBTOCG(fs, ex->ex_bn)];

	/*
	 * No reason to grab filesystem lock when just looking,
	 * since the superblock layout isn't going to change.
	 * If we miss cg_dfree increasing, tough.
	 */
	if (cg->cg_dfree < (want = ex->ex_length + add))
		goto quota_out;

	save.offset = want;

	mutex_lock(fs->fs_lock, PINOD);

	firstbit = cg->cg_firstdfree;
	bitlen = cg->cg_firstbn + fs->fs_cgfsize - firstbit;
	ASSERT(bitlen <= fs->fs_cgfsize - fs->fs_cgisize);

	if (!(efs_findfree(fs, firstbit, bitlen, &save))) {
		mutex_unlock(fs->fs_lock);
		goto quota_out;
	}

	firstbit = save.bn;
	ASSERT(save.length >= want);

#ifdef EFSDEBUG
	if (efs_badextent(fs, ex, ip)) {
		mutex_unlock(fs->fs_lock);
		goto quota_out;
	}
#endif

	efs_bitalloc(fs, firstbit, want);
	if (firstbit == cg->cg_firstdfree)
		cg->cg_firstdfree += want;

	cg->cg_dfree -= want;
	ASSERT(cg->cg_dfree >= 0);
	fs->fs_tfree -= want;
	fs->fs_fmod = 1;
	ASSERT(fs->fs_dirty == EFS_ACTIVEDIRT ||  \
		fs->fs_ncg != 1 || \
		fs->fs_tfree == cg->cg_dfree);

	mutex_unlock(fs->fs_lock);

	obp = bread(ip->i_dev, ex->ex_bn, ex->ex_length);
	nbp = getblk(ip->i_dev, firstbit, want);

	bcopy(obp->b_un.b_addr, nbp->b_un.b_addr, BBTOB(ex->ex_length));
	obp->b_flags |= B_STALE;
	obp->b_flags &= ~B_DELWRI;
	brelse(obp);
	bwrite(nbp);
	ip->i_flags |= IUPD | ICHG | ISYN;
	efs_iupdat(ip);

	/*
	 * Free ex's old blocks.
	 */

	mutex_lock(fs->fs_lock, PINOD);
	if (cg->cg_firstdfree > ex->ex_bn)
		cg->cg_firstdfree = ex->ex_bn;
	fs->fs_tfree += ex->ex_length;			/* update total free */
	cg->cg_dfree += ex->ex_length;
	fs->fs_fmod = 1;
	efs_bitfree(fs, (int) ex->ex_bn, (int) ex->ex_length);
	ASSERT(fs->fs_dirty == EFS_ACTIVEDIRT ||  \
		fs->fs_ncg != 1 || \
		fs->fs_tfree == cg->cg_dfree);
	mutex_unlock(fs->fs_lock);
	if ((doexlist_trash & 1) && (firstbit == 0 || want == 0)) {
		prdev("efs_alloc: zero length or block I=%d",
			ip->i_dev, ip->i_number);
		cmn_err(CE_PANIC, "efs_move: exlist trash");
	}

	ex->ex_bn = firstbit;
	ex->ex_length = want;
	return 1;

quota_out:
#ifdef _SHAREII
	/*
	 * We didn't use the disk claimed from the user's lnode before, so
	 * we'll give it back.
	 */
	SHR_LIMITDISK(itovfs(ip), ip->i_uid, (u_long)add, DEV_BSIZE, LI_UPDATE|LI_FREE, NULL, NULL);
#endif /* _SHAREII */
	(void) qt_chkdq(ip, (long)-add, 0, NULL);
	return 0;
}

#ifdef notdef
/*
 * Exchange ex's blocks for some in ``better position''.
 */
efs_realloc(
	register struct efs	*fs,
	register int	desiredlen,
	register int	stopbn)
{
	register struct cg *cg;
	register int	length;
	register int	firstbit;
	register int	bitlen;

	cg = &fs->fs_cgs[EFS_BBTOCG(fs, stopbn)];
	mutex_lock(fs->fs_lock, PINOD);
	ASSERT(stopbn >= fs->fs_firstcg && stopbn >= cg->cg_firstdbn);

	if (cg->cg_dfree <= desiredlen)
		goto out;

	firstbit = cg->cg_firstdfree;
	if ((bitlen = stopbn - firstbit) <= 0)
		goto out;

	ASSERT(bitlen <= fs->fs_cgfsize - fs->fs_cgisize);
	while (bitlen) {
		length = efs_tstfree(fs, firstbit, bitlen);

		if (length < 0)		/* Bitmap I/O error */
			break;

		if (length >= desiredlen) {
			ASSERT((firstbit - fs->fs_firstcg) % fs->fs_cgfsize >= \
				fs->fs_cgisize);
			efs_bitalloc(fs, firstbit, desiredlen);
			if (firstbit == cg->cg_firstdfree)
				cg->cg_firstdfree += desiredlen;
			cg->cg_dfree -= desiredlen;
			ASSERT(cg->cg_dfree >= 0);
			fs->fs_tfree -= desiredlen;
			fs->fs_fmod = 1;
			ASSERT(fs->fs_dirty == EFS_ACTIVEDIRT ||  \
				fs->fs_ncg != 1 || \
				fs->fs_tfree == cg->cg_dfree);
			mutex_unlock(fs->fs_lock);
			return(firstbit);
		}

		firstbit += length;
		bitlen -= length;
		if (bitlen == 0)
			break;

		length = efs_tstalloc(fs, firstbit, bitlen);

		if (length < 0)		/* Bitmap I/O error */
			break;

		firstbit += length;
		bitlen -= length;
	}

	/*
	 * No chunk available.
	 */
out:
	mutex_unlock(fs->fs_lock);
	return(0);
}
#endif

/*
 * efs_grow:
 *	- attempt to extend the given extent attached to the given inode
 */
int
efs_grow(
	register struct inode *ip,
	register extent	*ex,
	int		growlen,
	struct bmapval	*wex)
{
	register int firstbit;
	register struct cg *cg;
	register struct efs *fs;
	register efs_daddr_t bn;
	register int exlen;
	register int len = growlen;
	long quota_len;

	fs = mtoefs(ip->i_mount);
#ifdef EFSDEBUG
	mutex_lock(fs->fs_lock, PINOD);
	if (efs_badextent(fs, ex, ip)) {
		mutex_unlock(fs->fs_lock);
		return (0);
	}
	mutex_unlock(fs->fs_lock);
#endif
	/*
	 * Ensure that we don't extend across a cylinder group
	 * nor that we attempt to make an extent too large.
	 */
	bn = ex->ex_bn;
	exlen = ex->ex_length;
	cg = &fs->fs_cgs[EFS_BBTOCG(fs, bn)];
	firstbit = bn + exlen;

	if (firstbit + len >= cg->cg_firstbn + fs->fs_cgfsize)
		len = cg->cg_firstbn + fs->fs_cgfsize - firstbit;
	if (exlen + len > EFS_MAXEXTENTLEN)
		len = EFS_MAXEXTENTLEN - exlen;
	if (len == 0)
		return (0);

	quota_len = len;	/* can we grow by len bb's */
#ifdef _SHAREII
	/*
	 * Check with ShareII that this growth won't exceed the user's
	 * disk limit.  This also allocates the disk from the user's
	 * lnode.  If it's not used because the allocation ultimately
	 * fails, we'll give it back later.
	 */
	if (SHR_LIMITDISK(itovfs(ip),
			ip->i_uid,
			(u_long)quota_len,
			DEV_BSIZE,
			LI_ALLOC|LI_UPDATE|LI_ENFORCE,
			NULL,
			NULL))
		return 0;
#endif /* _SHAREII */

	if (qt_chkdq(ip, (long)(quota_len), 0, NULL))
	    return(0);

	/*
	 * Scan bitmap starting at bb just following last extent, counting
	 * the number of free bb's directly connected to the last extent.
	 * Stop when either len is exhausted or an allocated bb is found.
	 */
	mutex_lock(fs->fs_lock, PINOD);
	len = efs_tstfree(fs, firstbit, len);

	/* Bitmap I/O error */
	if (len <= 0) {
		len = 0;
		goto out;
	}

	/*
	 * If wex is passed, caller is growing a regular file and is
	 * attemping to grow extent to logical block boundary.  If we
	 * can't grow to a logical block boundary, grow to a page boundary.
	 */
	if (wex)
		while (growlen > len)
			growlen -= NDPP;
	if (growlen > 0)
		len = growlen;
	else {
		len = 0;
		goto out;
	}

	ASSERT(len > 0);

	efs_bitalloc(fs, firstbit, len);
	fs->fs_tfree -= len;
	fs->fs_fmod = 1;
	ex->ex_length += len;
	cg->cg_dfree -= len;
	if (firstbit == cg->cg_firstdfree)
		cg->cg_firstdfree += len;
	ASSERT(cg->cg_dfree >= 0);
	ASSERT(fs->fs_dirty == EFS_ACTIVEDIRT ||  \
		fs->fs_ncg != 1 || \
		fs->fs_tfree == cg->cg_dfree);

	/*
	 * Pass back block numbers if the caller is interested.
	 */
	if (wex) {
		wex->bn = bn + exlen;
		wex->length = len;
	}
out:
	mutex_unlock(fs->fs_lock);

	/*
	 * ## of bb's we thought we were going to use, but are not due to
	 * boundary and availability conditions.
	 */
	quota_len -= len;
	if (quota_len)
	{
#ifdef _SHAREII
		/*
		 * We ended up not using the allocation above, so we'll
		 * give it back.
		 */
	  SHR_LIMITDISK(itovfs(ip),
				    ip->i_uid,
			(u_long)quota_len,
				    DEV_BSIZE,
			LI_FREE|LI_UPDATE,
				    NULL,
				    NULL);
#endif /* _SHAREII */
		(void)qt_chkdq(ip, (long)-quota_len, 0, NULL);
	}
	return(len);
}

/*
 * efs_free:
 *	- free the given extent
 */
void
efs_free(
	register struct inode *ip,
	register extent *ex,
	register int	tosslen)
{
	register struct efs	*fs;
	register struct cg	*cg;
	register efs_daddr_t bn;
	register int	len;

	fs = itoefs(ip);
#ifdef EFSDEBUG
	/* Check and see if the filesystem is corrupted.  If it is
	 * or if the extent is somehow messed up, throw it away.
	 */
	mutex_lock(fs->fs_lock, PINOD);
	if (efs_badextent(fs, ex, ip)) {
		mutex_unlock(fs->fs_lock);
		return;
	}
	mutex_unlock(fs->fs_lock);
#endif

	/* Must toss buffer blocks before we free up the disk blocks.
	 */
	if (tosslen)
		efs_btoss(ip, ex, tosslen);

	len = ex->ex_length;
	bn = ex->ex_bn;

	mutex_lock(fs->fs_lock, PINOD);
	cg = &fs->fs_cgs[EFS_BBTOCG(fs, bn)];
	cg->cg_dfree += len;
	if (cg->cg_firstdfree > bn)
		cg->cg_firstdfree = bn;
	fs->fs_tfree += len;			/* update total free */
	fs->fs_fmod = 1;
	/*
	 * If space is tight, indicate that some has been freed up.
	 */
	if (fs->fs_diskfull) {
		fs->fs_freedblock = 1;
	}
#ifdef _SHAREII
	/*
	 * Update lnode with free blocks
	 */
	SHR_LIMITDISK(itovfs(ip),
			    ip->i_uid,
		      (u_long)len,
			    DEV_BSIZE,
		      LI_FREE|LI_UPDATE,
			    NULL,
			    NULL);
#endif /* _SHAREII */
	(void) qt_chkdq(ip, (int)-len, 0, NULL);
	
	efs_bitfree(fs, (int) bn, (int) len);
	mutex_unlock(fs->fs_lock);
}

/*
 * Remove individual buffers from buffer cache.
 *
 * Any blocks for which efs_btoss is called map the entire
 * extent into one buffer.  Tosslen is the size of the
 * extent before being trimmed/removed.  If it is not the same
 * as the ``tossed'' extent size, the extent is being trimmed,
 * not totally removed, so push out dirty data before
 * invalidating the buffer.
 */
static void
efs_btoss(register struct inode *ip, register extent *ex, int oldlen)
{
	register buf_t	*bp;
	register dev_t	dev = ip->i_dev;
	register long	bn;
	register int	len;

	ASSERT(ex->ex_length);
	bn = ex->ex_bn;
	len = ex->ex_length;

	bp = incore(dev, bn, oldlen, 1);
	if (bp) {
		bp->b_flags |= B_STALE;
		bp->b_flags &= ~B_ASYNC;
		if (oldlen != len) {
			if (bp->b_flags & B_DELWRI) {
				bwrite(bp);
				return;
			}
		}
		bp->b_flags &= ~B_DELWRI;
		brelse(bp);
	}
}
