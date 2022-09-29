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
 * Cached nonresident bitmap handling.
 */
#ident	"$Revision: 4.29 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/immu.h>		/* for NDPP */
#include <sys/param.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/vnode.h>
#include "efs_bitmap.h"
#include "efs_fs.h"
#include "efs_ino.h"
#include "efs_sb.h"

#ifdef DEBUG
#include <sys/cmn_err.h>
#endif

static void efs_bmrelse(struct buf *);
static struct buf *efs_bmread(dev_t, efs_daddr_t, int);

/*
 * Operations on the allocation bitmap for extent filesystems.
 * Complete bitmap is no longer kept in memory, thus to these functions
 * we pass the incore superblock of the filesystem (to find dev and
 * hence the bitmap on disk) instead of a pointer to an incore bitmap.
 *
 * Errors reading bitmap blocks cause a -1 return.
 */

/*
 * Defines for the number of file system blocks described by a chunk of bitmap.
 */

#define BITSPERBYTE	8
#define BBPERCHUNK	(NBPC * 4 / BBSIZE)
#define BITSPERBMCHUNK	(NBPC * 4 * BITSPERBYTE)

int 
efs_bitfunc(
	register struct efs *fs,
	register int b,
	register int len,
	int code)
{
	register struct buf *bp;
	register int bmchunk;	/* chunk of bitmap we're dealing with */
	register int llen;	/* 'local' length (ie in this map chunk) */
	register int bmbase;	/* starting block of bitmap */
	register int rblox;	/* number of blocks to read in current call */
	register int foundlen = 0;	/* total length to date for */
					/* search-type operations */
	register int lfoundlen;		/* length found in current map chunk */
	register int remlen; 	/* amt of original request still unsatisfied */
	register int offset;	/* offset in current chunk where op starts. */

	ASSERT(b > 0 && len > 0);
	ASSERT((b + len) <= (fs->fs_bmsize * BITSPERBYTE));

	/*
	 * Get bitmap starting block: bitmaps are mobile in 3.3 and beyond.
	 */
	bmbase = fs->fs_bmblock ? fs->fs_bmblock : EFS_BITMAPBB;

	/* Calculate the chunk of the bitmap which covers the requested block b,
	 * and find the bit offset in that chunk corresponding to the block.
	 */
	bmchunk = b / BITSPERBMCHUNK; 
	offset = b % BITSPERBMCHUNK;

	/*
	 * While request is unsatisfied, read bitmap chunks & operate on
	 * them until either the operation is satisfied, we hit end of bitmap,
	 * or (for seach operations) we hit the end of the run of 0s or 1s.
	 */
	remlen = len;

	/* Calculate number of blocks from start of current chunk to end of
	 * bitmap.  If that's more than a chunk, just read a chunk.  If not,
	 * there's at most a chunk of bitmap left.  Read only remaining
	 * blocks, since we don't want a buffer containing blocks beyond
	 * the end of the bitmap.  Also in this case we note that this is
	 * the last (maybe partial) chunk of the bitmap.
	 */
	while (remlen) {

		rblox = fs->fs_bmbbsize - (bmchunk * BBPERCHUNK);

		if (rblox > BBPERCHUNK)
			rblox = BBPERCHUNK;

		/*
		 * If remaining length starting from offset runs over
		 * the end of current (maybe partial) chunk, truncate
		 * 'local' length to end of this chunk.
		 */
		if ((offset + remlen) > (rblox * BBSIZE * BITSPERBYTE))
			llen = (rblox * BBSIZE * BITSPERBYTE) - offset;
		else
			llen = remlen;

		/*
		 * Get the buffer containing the required section of map.
		 */
		bp = efs_bmread(fs->fs_dev,
				bmbase + (bmchunk * BBPERCHUNK),
				rblox);

		if (bp->b_flags & B_ERROR) {
			brelse(bp);
#ifdef DEBUG
			prdev("bread error in efs_bitfunc",fs->fs_dev);
#endif
			return (foundlen ? foundlen : -1);
		}

		/*
		 * Depending on function code, call the appropriate common
		 * bitmap function to do the search or modification on the
		 * section of bitmap.  If it's a modify function we return
		 * the buffer as 'delayed write', or do the write
		 * synchronously if requested, otherwise just release.
		 */

		switch (code) {

	   	case EFS_TESTFREE:	
			lfoundlen = bbftstset(bp->b_un.b_addr, offset, llen);
			goto testresults;

	   	case EFS_TESTALLOC:
			lfoundlen = bbftstclr(bp->b_un.b_addr, offset, llen);
	testresults:
			foundlen += lfoundlen;
			efs_bmrelse(bp);
			/*
			 * If search came up short in local chunk or we've
			 * satisfied entire request, return results.
			 */
			if ((lfoundlen < llen) || (foundlen == len)) 
				return(foundlen);
			break;

	   	case EFS_BITFIELDSYNCFREE:
			bbfset(bp->b_un.b_addr, offset, llen);
			if (bwrite(bp))
				return (-1);
			break;

	   	case EFS_BITFIELDFREE:
			/*
			 * Bitmaps are regarded as critical system data,
			 * so we plug in the efs_asynchk function into the
			 * buffer b_relse field so that we will be warned
			 * and the filesystem marked bad if the write fails.
			 */
			bbfset(bp->b_un.b_addr, offset, llen);
			goto bitfieldresults;

	   	case EFS_BITFIELDALLOC:
			bbfclr(bp->b_un.b_addr, offset, llen);
	bitfieldresults:
			if (!(bp->b_flags & B_DELWRI)) {
				bp->b_start = lbolt;
				bp->b_flags |= B_DELWRI;
			}
			bp->b_relse = efs_asynchk;
			efs_bmrelse(bp);
			break;

#ifdef DEBUG
	   	default:
			cmn_err(CE_WARN, "Bad code to efs_bitfunc??!!\n");
			return 0;
#endif
		};

		/*
		 * Next pass. Decrement length yet to do by amount
		 * done in this chunk, bump bmchunk by 1, and set offset
		 * to 0 since in any subsequent chunk we are starting
		 * at beginning of chunk.
		 */

		remlen -= llen;
		bmchunk++;
		offset = 0;
	}

	/*
	 * For search functions, the result is foundlen;
	 * return value is irrelevent for modify functions.
	 */
	return (foundlen); 
}

/*
 * Search bitmap until bmap->offset free bits are found.
 * Return bit # in bmap->bn, length in bmap->length.
 */
int
efs_findfree(
	register struct efs *fs,
	register int b,
	register int len,
	struct bmapval *bmap)
{
	register struct buf *bp;
	register int bmchunk;	/* chunk of bitmap we're dealing with */
	register int llen;	/* 'local' length (ie in this map chunk) */
	register int bmbase;	/* starting block of bitmap */
	register int rblox;	/* number of blocks to read in current call */
	register int foundlen = 0;	/* total length to date for */
					/* search-type operations */
	register int lfoundlen;		/* length found in current map chunk */
	register int remlen; 	/* amt of original request still unsatisfied */
	register int offset;	/* offset in current chunk where op starts. */
	register int search_len;	/* max bits to examine at once */

	ASSERT(b > 0 && len > 0);
	ASSERT((b + len) <= (fs->fs_bmsize * BITSPERBYTE));

	/*
	 * Get bitmap starting block: bitmaps are mobile in 3.3 and beyond.
	 */
	bmbase = fs->fs_bmblock ? fs->fs_bmblock : EFS_BITMAPBB;

	/* Calculate the chunk of the bitmap which covers the requested block b,
	 * and find the bit offset in that chunk corresponding to the block.
	 */
	bmchunk = b / BITSPERBMCHUNK; 
	offset = b % BITSPERBMCHUNK;

	/*
	 * While request is unsatisfied, read bitmap chunks & operate on
	 * them until either the operation is satisfied, we hit end of bitmap,
	 * or (for seach operations) we hit the end of the run of 0s or 1s.
	 */
	remlen = len;

	/* Calculate number of blocks from start of current chunk to end of
	 * bitmap.  If that's more than a chunk, just read a chunk.  If not,
	 * there's at most a chunk of bitmap left.  Read only remaining
	 * blocks, since we don't want a buffer containing blocks beyond
	 * the end of the bitmap.  Also in this case we note that this is
	 * the last (maybe partial) chunk of the bitmap.
	 */
	while (remlen) {

		rblox = fs->fs_bmbbsize - (bmchunk * BBPERCHUNK);

		if (rblox > BBPERCHUNK)
			rblox = BBPERCHUNK;

		/*
		 * If remaining length starting from offset runs over
		 * the end of current (maybe partial) chunk, truncate
		 * 'local' length to end of this chunk.
		 */
		if ((offset + remlen) > (rblox * BBSIZE * BITSPERBYTE))
			llen = (rblox * BBSIZE * BITSPERBYTE) - offset;
		else
			llen = remlen;

		remlen -= llen;

		/*
		 * Get the buffer containing the required section of map.
		 */
		bp = efs_bmread(fs->fs_dev,
				bmbase + (bmchunk * BBPERCHUNK),
				rblox);

		if (bp->b_flags & B_ERROR) {
			brelse(bp);
#ifdef DEBUG
			prdev("bread error in efs_findfree",fs->fs_dev);
#endif
			return(0);
		}

		while (llen) {
			/*
			 * test free
			 * We don't want to look at more bits than
			 * we need, so limit the test to the min of
			 * the number we need and the number left in
			 * this chunk.
			 */
			search_len = MIN(llen, bmap->offset);
			lfoundlen = bbftstset(bp->b_un.b_addr, offset,
					     search_len);
			
			if (foundlen + lfoundlen >= bmap->offset) {
				ASSERT(foundlen < bmap->offset);
				offset -= foundlen;
				bmap->bn = bmchunk * BITSPERBMCHUNK + offset;
				bmap->length = foundlen + lfoundlen;
				efs_bmrelse(bp);
				return(1);
			}

			foundlen += lfoundlen;
			offset += lfoundlen;

			/*
			 * Keep track of largest piece that is smaller
			 * than requested size.
			 */
			if (foundlen > bmap->length) {
				register int rem = bmap->offset % NDPP;
				register int length = bmap->length;
				register int endbn = bmchunk * BITSPERBMCHUNK + offset;
				/*
				 * If this is a continuation from last
				 * chunk, just adjust the length field.
				 */
				if (bmap->bn + length == endbn - lfoundlen)
					bmap->length = foundlen;
				/*
				 * Don't bother if piece is larger than
				 * previous saved block but it wouldn't be
				 * large enough to extend to next logical
				 * page (rem is part of a page needed to
				 * expand file to a logical page boundary).
				 */
				else
				if (length == 0 ||
				    (length < rem && foundlen >= rem) ||
				    (foundlen-rem)/NDPP > (length-rem)/NDPP) {
					bmap->length = foundlen;
					bmap->bn = endbn - foundlen;
				}
			}

			llen -= lfoundlen;
			if (llen == 0)
				break;

			/* test allocs */
			lfoundlen = bbftstclr(bp->b_un.b_addr, offset, llen);
			
			offset += lfoundlen;
			llen -= lfoundlen;
			foundlen = 0;
		}

		efs_bmrelse(bp);

		bmchunk++;
		offset = 0;
	}

	return (0); 
}


/*
 * Read a section of bitmap.
 */
static struct buf *
efs_bmread(register dev_t dev, register efs_daddr_t blkno, register int len)
{
	register struct buf *bp;

	IGETINFO.ig_bmaprd++;

	bp = bread(dev, blkno, len);
	bp->b_ref = EFS_BMREF;

	if (bp->b_flags & B_FOUND)
		IGETINFO.ig_bmapfbc++;

	return bp;
}

/*
 * Release a section of bitmap:
 *	Release with a high enough reference value so it
 *	won't be reused quickly.
 */

#define EFS_BITMAPREF	3

static void
efs_bmrelse(register struct buf *bp)
{
	bp->b_ref = EFS_BITMAPREF;
	brelse(bp);
	return;
}

