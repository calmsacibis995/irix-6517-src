/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 3.81 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include "efs_inode.h"
#include "efs_sb.h"
#include "efs_fs.h"

#ifdef _VCE_AVOIDANCE
extern int vce_avoidance;
#endif
struct cred;
extern int efs_alloc(struct inode *, extent *, int, int, struct bmapval *);
static int efs_expand(struct inode *, int);
static int efs_clrbuf(bhv_desc_t *, extent *, struct bmapval *,
			int, struct cred *);
static void efs_lbmap(struct efs *,struct bmapval *, extent *, int);
static int efs_bmapLogicalBlock(bhv_desc_t *, off_t, ssize_t, int,
			struct cred *, struct bmapval *, int *);
static int efs_bmapExtent(bhv_desc_t *, off_t, ssize_t, int,
			struct bmapval *, int *);
extern int efs_grow(struct inode *, extent *, int, struct bmapval *);
extern int efs_move(struct inode *, extent *, int);
extern void efs_error(struct efs *, int, int);
extern void efs_free(struct inode *,extent *, int);

/*
 * efs_bmap:
 *	- translate a logical block # into a physical mapping
 */
int
efs_bmap(
	bhv_desc_t *bdp,
	off_t offset,
	ssize_t count,
	int rw,
	struct cred *cr,
	struct bmapval *iex,
	int *nex)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register struct inode *ip = bhvtoi(bdp);
	int error;
	int i;
	/* REFERENCED */
	u_short type;

	ilock(ip);

	if ((ip->i_flags & IINCORE) == 0) {
		if (efs_readindir(ip)) {
			error = EFBIG;
			goto out;
		}
		ASSERT(ip->i_flags & IINCORE);
	}
	ip->i_flags &= ~INOUPDAT;

	type = ip->i_mode & IFMT;
	ASSERT(type == IFREG || type == IFDIR || ISLINK(type));
	ASSERT(rw == B_READ || rw == B_WRITE);

	if (vp->v_type == VREG)
		error = efs_bmapLogicalBlock(bdp, offset, count, rw, cr,
					     iex, nex);
	else
		error = efs_bmapExtent(bdp, offset, count, rw, iex, nex);
out:
	/*
	 * Clear out the policy module pointers in the mappings we
	 * return since we have nothing interesting to put there.
	 */
	if (!error) {
		for (i = 0; i < *nex; i++) {
			iex[i].pmp = NULL;
		}
	}
	iunlock(ip);
	return error;
}

/*
 * efs_bmapExtent:
 *	- map a logical offset into an extent
 *	- if writing, allocate blocks if needed to extend the file
 */
static int
efs_bmapExtent(
	bhv_desc_t *bdp,
	off_t offset,
	ssize_t count,
	int rw,
	register struct bmapval *iex,
	int *nex)
{
	register extent *ex;
	register int newposbb;
	struct inode *ip;
	ssize_t sz;
	ssize_t rem;
	int justgrew;
	int error;
	int i;
	timespec_t tv;

	ip = bhvtoi(bdp);

	ASSERT(!ISLINK(ip->i_mode & IFMT) || ip->i_size == 0 || ip->i_numextents > 0);
	ASSERT(ip->i_numextents >= 0 && ip->i_numextents <= EFS_MAXEXTENTS);

	iex->pbdev = ip->i_dev;

	/*
	 * newposbb is the bb position of the users i/o request. 
	 */
	newposbb = BTOBBT(offset);
	justgrew = 0;
	iex->length = iex->bsize = 0;

	/*
	 * Do a simple binary search through the extents since directories
	 * and symlinks tend to be small.
	 */
	for (i = ip->i_numextents, ex = &ip->i_extents[0]; --i >= 0; ex++) {
		if (newposbb < ex->ex_offset + ex->ex_length)
			goto gotit;
	}

	/*
	 * Unable to find the offset into the file.
	 * If reading, then EOF has been reached.
	 */
	if (rw == B_READ) {
		iex->pbsize = 0;
		*nex = 1;
		return 0;
	}

	/* Inode is being written, and needs to grow.
	 * First, just try to extend the last extent in the file.
	 * If this doesn't work, try to allocate space for a
	 * replacement extent within the same cylinder group.
	 * If that fails, allocate a new extent.
	 */
	ip->i_flags |= ITRUNC;		/* conservative */
	if (ip->i_numextents) {
		ex = &ip->i_extents[ip->i_numextents - 1];
		if (efs_grow(ip, ex, 1, (struct bmapval *)NULL)) {
			register struct buf *bp;
			bp = incore(ip->i_dev, ex->ex_bn, ex->ex_length-1, 1);
			if (bp) {
				bp->b_flags |= B_STALE;
				if (bp->b_flags & B_DELWRI) {
					bwrite(bp);
				} else {
					brelse(bp);
				}
			}
			goto grew;
		}

		/* Now try to allocate a larger extent within
		 * this cylinder group...
		 */
		if (efs_move(ip, ex, 1))
			goto grew;
	}

	/*	Couldn't extend the last extent.  Need to allocate a
	 *	new extent to hold the new allocation.  If this fails,
	 *	give up the ghost.  Must reset ip because it may have
	 *	been reallocated in efs_expand.
	 */
	if (error = efs_expand(ip, ip->i_numextents + 1)) {
		if (justgrew)
			(void) efs_itrunc(ip, ip->i_size, 0);
		return error;
	}

	/*	We now have room for the new extent.  Allocate one.
	 */
	ex = &ip->i_extents[ip->i_numextents];

	if (error = efs_alloc(ip, ex, 1, 0, (struct bmapval *)NULL)) {
		irealloc(ip, ip->i_numextents * sizeof(extent));
		if (justgrew)
			(void) efs_itrunc(ip, ip->i_size, 0);
		efs_error(itoefs(ip), 0, error);
		return error;
	}

	if (ip->i_numextents++ == 0)
		ex->ex_offset = 0;
	else
		ex->ex_offset = (ex-1)->ex_offset + (ex-1)->ex_length;

	ex->ex_magic = 0;

	/*	If the piece of extent we found doesn't overlap the
	 *	region of the file we are interested in, clear out the
	 *	piece we did allocate (ensuring zeros for un-written
	 *	pieces) and start searching the extent list over again.
	 */
grew:
	justgrew = 1;
	/* note that things changed */
	nanotime_syscall(&tv);
	ip->i_flags |= IMOD;
	ip->i_mtime = ip->i_ctime = tv.tv_sec;
	ip->i_umtime = tv.tv_nsec;

	ASSERT(ex->ex_offset <= newposbb && \
		ex->ex_offset + ex->ex_length > newposbb);

	/*	Found extent overlapping u_offset.  Procede to figure out
	 *	what kind of i/o to do.
	 */
gotit:
	iex->bn = ex->ex_bn;
	iex->offset = ex->ex_offset;
	iex->length = ex->ex_length;
	iex->bsize = BBTOB(iex->length);

	iex->pboff = offset - BBTOB(ex->ex_offset);
	sz = iex->bsize - iex->pboff;
	if (count < sz)
		sz = count;

	if (rw == B_READ) {
		rem = ip->i_size - offset;
		if (rem < 0)
			rem = 0;
		if (rem < sz)
			sz = rem;

		iex->pbsize = sz;

		/*
		 * If the user is reading, and the user is reading
		 * more than this extent or it looks like the read
		 * is sequential from the previous read, map the next
		 * extent as the readahead mapping.
		 */
		if ((*nex > 1)
		 && (ip->i_nextbn == newposbb || count > sz)
		 && (++ex < &ip->i_extents[ip->i_numextents])) {
			*nex = 2;
			iex++;
			iex->bn = ex->ex_bn;
			iex->offset = ex->ex_offset;
			iex->length = ex->ex_length;
			iex->bsize = BBTOB(iex->length);
		} else
			*nex = 1;

		ip->i_nextbn = newposbb + BTOBB(sz);
		return 0;
	}

	iex->pbsize = sz;
	*nex = 1;

	return 0;
}

/*
 * Logical block macros used for allocating extents and
 * partitioning them into pieces edible by the file system cache.
 */

#define EFS_LBTOBBSHIFT(fs)	((fs)->fs_lbshift - BBSHIFT)
#define	EFS_BBSPERLB(fs)	(1 << EFS_LBTOBBSHIFT(fs))
#define	EFS_BBTOLBT(fs,bb)	(((bb) >> ((fs)->fs_lbshift - BBSHIFT)) \
					<< ((fs)->fs_lbshift - BBSHIFT))

static void
efs_lbmap(
	register struct efs *efs,
	register struct bmapval *iex,
	register extent *ex,
	register int newposbb)
{
	register int newposlb = EFS_BBTOLBT(efs, newposbb);
	register int roff;

	if ((roff = newposlb - ex->ex_offset) > 0) {
		iex->bn = ex->ex_bn + roff;
		iex->offset = ex->ex_offset + roff;
		iex->length = ex->ex_length - roff;
	} else {
		iex->bn = ex->ex_bn;
		iex->offset = ex->ex_offset;
		iex->length = ex->ex_length;
	}

	ASSERT(iex->length > 0 && newposbb >= iex->offset);

	/*	Trim mapping of blocks beyond this logical block.
	 */

	newposlb += EFS_BBSPERLB(efs);
	roff = iex->offset + iex->length - newposlb;
	if (roff > 0)
		iex->length -= roff;

	iex->bsize = BBTOB(iex->length);
}

/*
 * efs_bmapLogicalBlock:
 *	- map a logical offset into a logical block
 *	- if writing, allocate blocks if needed to extend the file
 */
static int
efs_bmapLogicalBlock(
	bhv_desc_t *bdp,
	off_t offset,
	ssize_t count,
	int rw,
	struct cred *cr,
	register struct bmapval *iex,
	int *nex)
{
	register extent *ex;
	register int newposbb;
	register struct efs *efs;
	register int bbsperlb;
	register struct inode *ip;
	register ssize_t sz;
	register ssize_t rem;
	int nmaps;
	struct bmapval wex;
	struct bmapval *rex;
	extent *lastex;
	int justgrew;
	int error;
	timespec_t tv;

	ip = bhvtoi(bdp);
	efs = itoefs(ip);
	bbsperlb = EFS_BBSPERLB(efs);

	ASSERT(BHV_TO_VNODE(bdp)->v_type == VREG);
	ASSERT(ip->i_numextents >= 0 && ip->i_numextents <= EFS_MAXEXTENTS);

	iex->pbdev = ip->i_dev;

	/*
	 * newposbb is the bb position of the users i/o request. 
	 */
	newposbb = BTOBBT(offset);

	/*
	 * have we seeked since the last write, or are autogrowing?
	 */
	if (offset > ip->i_size && rw == B_WRITE && ip->i_size != 0) {
		/*
		 * yes, zero bytes from i_size through end
		 * of last extent
		 */
		ASSERT(ip->i_numextents > 0);
		ex = &ip->i_extents[ip->i_numextents - 1];

		/* Do nothing if i_size isn't within the last extent
		 * or if extent ends exactly at i_size.
		 * First case shouldn't happen if file is trimmed when
		 * efs_expand fails, but we shouldn't count on it.
		 * And if file isn't within the last extent, the
		 * extents beyond i_size should have been zeroed.
		 */
		if (ip->i_size >= BBTOB(ex->ex_offset) &&
		    ip->i_size < BBTOB(ex->ex_offset + ex->ex_length)) {

			wex.offset = BTOBBT(ip->i_size);
			wex.bn = ex->ex_bn + (wex.offset - ex->ex_offset);

			/* zero up thru min of newposbb and whole extent
			 */
			if (newposbb < ex->ex_offset + ex->ex_length)
				wex.length = newposbb - wex.offset + 1;
			else
				wex.length = ex->ex_length -
					     (wex.offset - ex->ex_offset);

			if (error = efs_clrbuf(bdp, ex, &wex,
					 ip->i_size & BBMASK, cr))
				return error;
		}
	}

	justgrew = 0;
	wex.bsize = 0;	/* XXX unnecessary? */

	/*
	 * Do a binary search through the extents based on the file size
	 * and the extent offsets.
	 * XXX Using holes in files will mess this code up something fierce
	 */
	if (ip->i_numextents) {
		ex = &ip->i_extents[ip->i_numextents - 1];
		if (newposbb < ex->ex_offset + ex->ex_length) {
			register int i, j, k;

			k = ip->i_numextents;
			j = ip->i_numextents >> 1;
			i = j;
			for (;;) {

				ex = &ip->i_extents[i];

				/*
				 * See if the extent overlaps the start of the
				 * request.  If so, use the extent, otherwise
				 * keep looping.
				 */
				if ((ex->ex_offset <= newposbb)
				 && (ex->ex_offset + ex->ex_length > newposbb))
					goto gotit;

				/*
				 * Figure out next spot to look.
				 */
				if ((j >>= 1) == 0)
					j = 1;		/* make sure we move */ 
				if (ex->ex_offset < newposbb)
					i += j;		/* advance */
				else
					i -= j;		/* retreat */

				if ((i < 0) || (i >= ip->i_numextents))
					break;

				/*
				 * Avoid infinite loops by tracking the
				 * number of extents we've viewed.
				 */
				if (k-- < 0) {
					prdev("Extent list trashed, inode %d",
					      ip->i_dev, ip->i_number);
					return EIO;
				}
			}
		}
	}

	/*
	 * Unable to find the offset into the file.
	 * If reading, then EOF has been reached.
	 */
	if (rw == B_READ) {
		iex->pbsize = 0;
		iex->length = iex->bsize = 0;
		*nex = 1;
		return 0;
	}

	/* Inode is being written, and needs to grow.
	 * First, just try to extend the last extent in the file.  
	 * Compute in rem the number of bb's to allocate to extend
	 * the file out to the end of the current logical block.
	 * (efs_grow will truncate rem if it would overflow the extent.)
	 */
	ip->i_flags |= ITRUNC;		/* conservative */

#define EFS_MAXEXLEN 192
#define EFS_MAXLB 128

start_over:
	if (ip->i_numextents) {
		ex = &ip->i_extents[ip->i_numextents - 1];
		rem = bbsperlb - ((ex->ex_offset+ex->ex_length) & (bbsperlb-1));

		/*	Largest possible extent is 248 BBS, but only one
		 *	128-block logical block could fit in one extent.
		 *	This would make a 760Mb file go indirect.
		 *	Compromise then and (try to) allocate one 64-block
		 *	and one 128-block lb in each extent.  The current
		 *	fsr thinks that 64-block filesystems are the max,
		 *	so these extents won't be perturbed.
		 */
		if (rem == EFS_MAXLB && rem + ex->ex_length > EFS_MAXEXLEN)
			sz = rem/2;
		else
			sz = rem;

		if (ex->ex_length + sz <= EFS_MAXEXTENTLEN &&
		    efs_grow(ip, ex, sz, &wex)) {
			ASSERT(wex.bn >= ex->ex_bn);
			wex.offset = wex.bn - ex->ex_bn + ex->ex_offset;
			goto grew;
		}
	} else {
		rem = bbsperlb;
	}
#undef EFS_MAXEXLEN
#undef EFS_MAXLB

	/*	Couldn't extend the last extent.  Need to allocate a
	 *	new extent to hold the new allocation.  If this fails,
	 *	give up the ghost.  Must reset ip because it may have
	 *	been reallocated in efs_expand.
	 */
	if (error = efs_expand(ip, ip->i_numextents + 1)) {
		if (justgrew)
			(void) efs_itrunc(ip, ip->i_size, 0);
		return error;
	}

	/*	We now have room for the new extent.  Allocate one.
	 */
	ex = &ip->i_extents[ip->i_numextents];

	if (error = efs_alloc(ip, ex, rem, 0, &wex)) {
		irealloc(ip, ip->i_numextents * sizeof(extent));
		if (justgrew)
			(void) efs_itrunc(ip, ip->i_size, 0);
		efs_error(efs, 0, error);
		return (error);
	}

	if (ip->i_numextents++ == 0)
		ex->ex_offset = 0;
	else
		ex->ex_offset = (ex-1)->ex_offset + (ex-1)->ex_length;

	ex->ex_magic = 0;
	wex.offset = ex->ex_offset;

	/*	If the piece of extent we found doesn't overlap the
	 *	region of the file we are interested in, clear out the
	 *	piece we did allocate (ensuring zeros for un-written
	 *	pieces) and start searching the extent list over again.
	 * XXX	This is in lieu of correctly supporting holes.
	 */
grew:
	justgrew = 1;
	/* note that things changed */
	nanotime_syscall(&tv);
	ip->i_flags |= IMOD;
	ip->i_mtime = ip->i_ctime = tv.tv_sec;
	ip->i_umtime = tv.tv_nsec;

	ASSERT(ex->ex_offset <= newposbb);
	if (ex->ex_offset + ex->ex_length <= newposbb) {
		ASSERT(wex.offset >= ex->ex_offset);
		if (error = efs_clrbuf(bdp, ex, &wex, 0, cr)) {
			(void) efs_itrunc(ip, ip->i_size, 0);
			return error;
		}
		goto start_over;
	}

	/*	Found extent overlapping u_offset.
	 *	Determine how much i/o to do.
	 */
gotit:

	efs_lbmap(efs, iex, ex, newposbb);

	ASSERT(iex->length > 0 && offset >= BBTOB(iex->offset));

	lastex = &ip->i_extents[ip->i_numextents - 1];

	if (ex == lastex &&
	    iex->offset + iex->length == ex->ex_offset + ex->ex_length)
		iex->eof = 1;
	else
		iex->eof = 0;

	nmaps = *nex - 1;
	*nex = 1;

	iex->pboff = offset - BBTOB(iex->offset);
	sz = iex->bsize - iex->pboff;
	if (count < sz)
		sz = count;

	if (rw == B_READ) {
		rem = ip->i_size - offset;
		if (rem < 0)
			rem = 0;
		if (rem < sz)
			sz = rem;

		iex->pbsize = sz;

		/*
		 * If the caller is reading and is interested in readahead...
		 */
		if (nmaps > 0) {
			/*
			 * If it looks like the read is sequential from
			 * the previous read, initiate readahead on the
			 * remaining bmapvals passed only if the caller
			 * has reached the last logical readahead block.
			 */
			if (ip->i_nextbn == newposbb) {
				if (newposbb < ip->i_reada)
					goto fastout;
			} else
			/*
			 * If the caller isn't reading sequentially, but
			 * wants more than this block, start up readahead.
			 */
			if (count <= sz)
				goto out;

			rex = iex + 1;
			while (--nmaps >= 0) {
				rex->pbdev = ip->i_dev;
				newposbb = iex->offset + iex->length;
				if (newposbb < ex->ex_offset + ex->ex_length)
					efs_lbmap(efs, rex, ex, newposbb);
				else
				if (++ex <= lastex)
					efs_lbmap(efs, rex, ex, ex->ex_offset);
				else
					break;

				(*nex)++;
				iex = rex;
				rex++;

				if (ex == lastex &&
				    iex->offset + iex->length ==
						ex->ex_offset + ex->ex_length)
					iex->eof = 1;
				else
					iex->eof = 0;
			}

			/*
			 * Set readahead block to the beginning block
			 * of the second half of the bmap set.  Thus,
			 * if efs_readi passes four bmapvals, the
			 * readahead block is set to the beginning of
			 * the third bmap, if six, the fourth, etc.
			 */
			nmaps = *nex;
			if (nmaps > 1) {
				rex -= nmaps / 2;
				ip->i_reada = rex->offset;
			} else
				/* There isn't anything more to read here,
				 * so reset readahead for the next attempt.
				 */
				ip->i_reada = ip->i_nextbn = 0;
		}
fastout:
		ip->i_nextbn = BTOBBT(offset) + BTOBB(sz);
		return 0;
out:
		ip->i_nextbn = ip->i_reada = BTOBBT(offset) + BTOBB(sz);
		return 0;
	}

	/* If we just grew the file, check and see if write request
	 * starts beyond the newly allocated area.  If it does,
	 * then clear out the area through the area to be written.
	 * Either way, make sure that if extent growth would cause
	 * a larger buffer to be bmap'd, the old buffer is written
	 * to disk and tossed.
	 */
	if (justgrew) {
		/*
		 * This only happens when process has seeked and written
		 * beyond i_size.  Zero out blocks preceeding offset.
		 */
		if (BBTOB(wex.offset) < offset) {
			ASSERT(wex.offset >= ex->ex_offset);
			if (error = efs_clrbuf(bdp, ex, &wex, 0, cr))
				(void) efs_itrunc(ip, ip->i_size, 0);
		}
	}

	iex->pbsize = sz;

	return 0;
}

/*
 * efs_expand: - expand the inode.
 *
 *	- If the number of extents is going to overflow the direct
 *	  extents, allocate an indirect extent. 
 *
 *	- If the number of extents is going to overflow the current last
 *	  indirect extent:
 *
 *	  	- If that extent is < EFS_MAXINDIRBBS, attempt to grow it.
 *
 *	  	- If it won't grow, try to allocate a larger extent to
 *		  replace it.
 *
 *	  	- If that fails, OR if it's already the maximum allowed size,
 *		  allocate a new indirect extent.
 */
static int
efs_expand(
	register struct inode *ip,
	register int numextents)
{
	register int nblks;
	register int numindirs;
	register extent *ex;
	register int len;
	register struct buf *bp;
	int blks;
	int error;
	struct extent nindir;

	if (numextents > EFS_MAXEXTENTS || numextents < 0)
		return (EFBIG);

	while (numextents > EFS_DIRECTEXTENTS &&
	    (nblks = BTOBB(numextents * sizeof(extent) - ip->i_indirbytes))) {
		/*
		 * Must expand this indirect extent if possible,
		 * otherwise allocate a new one.
		 * Note that i_indir could be non-zero but numindirs zero
		 * if we had allocated i_indir previously but aborted
		 * because we couldn't allocate disk space for the extents.
		 */
		numindirs = ip->i_numindirs;
		ASSERT(numindirs >= 0);

		if (ip->i_indir == 0) {
			ASSERT(numindirs == 0);
			ip->i_indir = kmem_zalloc(sizeof(extent), KM_SLEEP);
		}

		if (numindirs == 0)
			goto firstindir;

		ex = &ip->i_indir[numindirs - 1];
		len = ex->ex_length; 

		ASSERT(len <= EFS_MAXINDIRBBS);

		/* If already at size limit, don't try to expand. 
		 */
		if (len + nblks > EFS_MAXINDIRBBS)
			goto newindir;

		/* Attempt to grow the last indirect extent.  
		 */
		if (efs_grow(ip, ex, nblks, (struct bmapval *)NULL)) {
			bp = incore(ip->i_dev, ex->ex_bn, len, 1);
			if (bp) {
				bp->b_flags |= B_STALE;
				bp->b_flags &= ~B_DELWRI;
				brelse(bp);
			}

			/*
			 * If nblks was > 1, efs_grow could have grown
			 * ex by a smaller value if it saw fit.
			 */
			ip->i_indirbytes += BBTOB(ex->ex_length - len); 

			/*
			 * If blocks aren't initialized and written to
			 * disk now, non-ISYN, non-INODELAY efs_iupdat
			 * will ``push'' both the indirect blocks and the
			 * inode delayed-write, so the inode can very
			 * well get out to disk before the indirect block
			 * information.  If the system crashes at this
			 * time, fsck will paw through unitialized indirect
			 * blocks and get very confused.
			 * Note that pushing the indirect block(s) async
			 * serializes future indir-block/inode pushes
			 * initiated via efs_iupdat.
			 *
			 * Only zero the new indirect blocks, not the ones
			 * which are already there on disk.
			 */
			nblks = ex->ex_length;
			bp = bread(ip->i_dev, ex->ex_bn, nblks);
			bzero(bp->b_un.b_addr + BBTOB(len),
			      BBTOB(nblks - len));
			bp->b_relse = efs_asynchk;
			bawrite(bp);

			/*
			 * Check again, in case we got fewer blocks
			 * than we asked for.
			 */
			continue;
		}

		/*
		 * If that fails, try to allocate an indirect extent
		 * of the required length to replace the existing one.
		 */

		nindir = *ex;

		if (efs_alloc(ip, ex, len + nblks, 1, NULL) == 0) {
			/*
			 * We now have an indirect extent of the desired size.
			 * Throw away the old indirect extent & replace it
			 * with the new one.  Before freeing the old blocks,
			 * force out the inode -- if the system crashed
			 * after freeing the old blocks, and another file
			 * grabbed the blocks and wrote out its inode _before_
			 * this inode got pushed, fsck would find duplicate
			 * blocks (and likely remove _both_ files).
			 * We zero the part of the indirect extent that
			 * will not be filled in by the immediate call
			 * to efs_iupdat().  Just release it delwri so
			 * that efs_iupdat() can grab it and fill in the
			 * rest.
			 *
	  		 * We re-write the inode synchronously so that we can
			 * be sure that the new indirect block makes it out
			 * to disk before the inode.  Not doing so could leave
			 * us with an inode pointing to an unitialized
			 * indirect block if we crash in between the inode
			 * hitting the disk and the indirect block making it.
			 */
			ASSERT(ex->ex_length == len + nblks);
			blks = ex->ex_length;
			bp = getblk(ip->i_dev, ex->ex_bn, blks);
			bzero(bp->b_un.b_addr + BBTOB(len),
			      BBTOB(blks - len));
			bp->b_relse = efs_asynchk;
			bdwrite(bp);
			ip->i_flags |= ISYN;
			efs_iupdat(ip);
			efs_free(ip, &nindir, len);
		} else {
			/*
			 * If that fails, try to allocate another indirect.
			 */
newindir:
			/*
			 * Is there more room in inode for indirect extents?
			 */
			if (numindirs == EFS_DIRECTEXTENTS)
				return (EFBIG);
firstindir:
		 	/*
			 * Grow the indirect extent vector.
		 	 */
			ip->i_indir = kmem_realloc(ip->i_indir,
		    			(numindirs + 1) * sizeof(extent),
					KM_SLEEP);

			ex = &ip->i_indir[numindirs];
			if (error = efs_alloc(ip, ex, nblks, 1, NULL)) {
				efs_error(itoefs(ip), 0, error);
				return (error);
			}
			ex->ex_magic = 0;
			ip->i_numindirs++;

			ASSERT(ex->ex_length == nblks);
			bp = getblk(ip->i_dev, ex->ex_bn, nblks);
			bzero(bp->b_un.b_addr, BBTOB(nblks));
			bp->b_relse = efs_asynchk;
			bawrite(bp);
		}

		ip->i_indirbytes += BBTOB(nblks); 
		continue;
	}

	/*
	 * Allocate space for the new incore information.
	 */
	irealloc(ip, numextents * sizeof(extent));
	return (0);
}

/*
 * Clear the wex portion of ex.
 */
static int
efs_clrbuf(
	bhv_desc_t *bdp,
	struct extent *ex,
	struct bmapval *wex,
	int byteoff,
	struct cred *cred)
{
	register struct buf *bp;
	register off_t	offset;
	struct inode *ip;
	vnode_t *vp;
	struct bmapval	bmap;
	int error = 0;

	ASSERT(BHV_TO_VNODE(bdp)->v_type == VREG);
	ASSERT(byteoff < BBSIZE);

	ip = bhvtoi(bdp);
	efs_lbmap(itoefs(ip), &bmap, ex, wex->offset);

	bmap.pbdev = ip->i_dev;
	bmap.eof = 1;	/* always true... */
	ASSERT(bmap.bsize == BBTOB(bmap.length));
	bmap.pboff = bmap.pbsize = 0;
	bmap.pmp = NULL;

	offset = BBTOB(wex->offset - bmap.offset) + byteoff;

	vp = BHV_TO_VNODE(bdp);
	if (offset == 0)
		bp = getchunk(vp, &bmap, cred);
	else
		bp = chunkread(vp, &bmap, 1, cred);

	offset = BBTOB(wex->offset - bmap.offset) + byteoff;

#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
		extern void biozero(struct buf *,u_int,int);

		biozero(bp,offset,bmap.bsize - offset);
	} else
#endif
	{
	(void)bp_mapin(bp);
	bzero(bp->b_un.b_addr + offset, bmap.bsize - offset);
	}
	bdwrite(bp);

	return error;
}
