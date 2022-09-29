/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Id: cxfs_bmap.c,v 1.1 1997/09/12 17:54:48 lord Exp $"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/pfdat.h>
#include <sys/kmem.h>
#include <sys/immu.h>
#include <sys/uuid.h>
#include <sys/errno.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>

#include <fs/cell/dvn_tokens.h>

#include "cxfs.h"
#include "dcxvn.h"

/*
 *    XFS_BMAPI_WRITE         write operation: allocate space
 *      XFS_BMAPI_DELAY         delayed write operation
 *      XFS_BMAPI_ENTIRE        return entire extent, not trimmed
 *      XFS_BMAPI_EXACT         allocate only to spec'd bounds
 */

extern xfs_bmbt_rec_t *		       /* pointer to found extent entry */
xfs_bmap_do_search_extents(
	xfs_bmbt_rec_t *,
	xfs_extnum_t,
	xfs_extnum_t,
	xfs_fileoff_t,			/* block number searched for */
	int *,				/* out: end of file found */
	xfs_extnum_t *,			/* out: last extent index */
	xfs_bmbt_irec_t *,		/* out: extent entry found */
	xfs_bmbt_irec_t *);		/* out: previous extent entry found */


xfs_bmbt_rec_t *		       /* pointer to found extent entry */
cxfs_bmap_search_extents(
	dcxvn_t * dcxp,			/* incore inode pointer */
	xfs_fileoff_t bno,		/* block number searched for */
	int *eofp,    			/* out: end of file found */
	xfs_extnum_t * lastxp,		/* out: last extent index */
	xfs_bmbt_irec_t * gotp,		/* out: extent entry found */
	xfs_bmbt_irec_t * prevp)	/* out: previous extent entry found */
{
	xfs_extnum_t lastx;	       /* last extent index used */
	xfs_extnum_t nextents;	       /* extent list size */
	xfs_bmbt_rec_t *base;	       /* base of extent list */

	lastx = dcxp->dcx_lastext;
	nextents = dcxp->dcx_ext_count / sizeof (xfs_bmbt_rec_t);
	base = (xfs_bmbt_rec_t *) dcxp->dcx_extents;

	return (xfs_bmap_do_search_extents(base, lastx, nextents, bno, eofp,
					   lastxp, gotp, prevp));
}


int
cxfs_bmapi(
	dcxvn_t * dcxp,
	xfs_fileoff_t bno,		/* starting file offs. mapped */
	xfs_filblks_t len,		/* length to map in file */
	int flags,			/* XFS_BMAPI_... */
	xfs_fsblock_t * firstblock,	/* first allocated block
					   controls a.g. for allocs */
	xfs_extlen_t total,		/* total blocks needed */
	xfs_bmbt_irec_t * mval,		/* output: map values */
	int *nmap,			/* i/o: mval size/count */
	xfs_bmap_free_t * flist)	/* i/o: list extents to free */
{
	xfs_bmbt_irec_t got;	       /* current extent list record */
	xfs_extnum_t lastx;	       /* last useful extent number */
	xfs_bmbt_irec_t prev;	       /* previous extent list record */
	int     eof;		       /* we've hit the end of extent list */
	xfs_bmbt_rec_t *ep;	       /* extent list entry pointer */
	xfs_extnum_t nextents;	       /* number of extents in file */
	xfs_fileoff_t end;	       /* end of mapped file region */
	xfs_fileoff_t obno;	       /* old block number (offset) */
	char    inhole;		       /* current location is hole in file */
	char    trim;		       /* output trimmed to match range */
	char    exact;		       /* don't do all of wasdelayed extent */
	char    wr;		       /* this is a write request */
	char    delay;		       /* this request is for delayed alloc */
	int     n;		       /* current extent index */
	int     nallocs;	       /* number of extents alloc\'d */

	ASSERT(*nmap >= 1);

	/* For now only support reads - flags must == 0 */

	if (flags & (XFS_BMAPI_METADATA | XFS_BMAPI_DELAY | XFS_BMAPI_WRITE)) {
		return (EIO);
	}
	/* Ensure we have the extents and that they stay put whilst we
	 * do this.
	 */
	tkc_acquire1(dcxp->dcx_tokens, DVN_EXTENT_READ_1);

	/* There is potentially an error set in the dsvn at this point,
	 * we need a way to get to it here....
	 */


	wr = 0;
	trim = (flags & XFS_BMAPI_ENTIRE) == 0;
	exact = (flags & XFS_BMAPI_EXACT) != 0;
	delay = 0;

	ep = cxfs_bmap_search_extents(dcxp, bno, &eof, &lastx, &got, &prev);
	nextents = dcxp->dcx_ext_count / sizeof (xfs_bmbt_rec_t);
	n = 0;
	end = bno + len;
	obno = bno;
	nallocs = 0;

	while (bno < end && n < *nmap) {
		/*
		 * Reading past eof, act as though there's a hole
		 * up to end.
		 */
		if (eof)
			got.br_startoff = end;
		inhole = eof || got.br_startoff > bno;
		if (inhole) {
			/*
			 * Reading in a hole.
			 */
			mval->br_startoff = bno;
			mval->br_startblock = HOLESTARTBLOCK;
			mval->br_blockcount =
				XFS_FILBLKS_MIN(len, got.br_startoff - bno);
			bno += mval->br_blockcount;
			len -= mval->br_blockcount;
			mval++;
			n++;
			continue;
		}
		/*
		 * Then deal with the allocated space we found.
		 */
		ASSERT(ep != NULL);
		if (trim && (got.br_startoff + got.br_blockcount > obno)) {
			if (obno > bno)
				bno = obno;
			ASSERT((bno >= obno) || (n == 0));
			ASSERT(bno < end);
			mval->br_startoff = bno;
			if (ISNULLSTARTBLOCK(got.br_startblock)) {
				ASSERT(!wr || delay);
				mval->br_startblock = DELAYSTARTBLOCK;
			} else
				mval->br_startblock =
					got.br_startblock +
					(bno - got.br_startoff);
			/*
			 * Return the minimum of what we got and what we
			 * asked for for the length.  We can use the len
			 * variable here because it is modified below
			 * and we could have been there before coming
			 * here if the first part of the allocation
			 * didn't overlap what was asked for.
			 */
			mval->br_blockcount =
				XFS_FILBLKS_MIN(end - bno, got.br_blockcount -
						(bno - got.br_startoff));
			ASSERT(mval->br_blockcount <= len);
		} else {
			*mval = got;
			if (ISNULLSTARTBLOCK(mval->br_startblock)) {
				ASSERT(!wr || delay);
				mval->br_startblock = DELAYSTARTBLOCK;
			}
		}
		ASSERT(!trim ||
		       ((mval->br_startoff + mval->br_blockcount) <= end));
		ASSERT(!trim || (mval->br_blockcount <= len) ||
		       (mval->br_startoff < obno));
		bno = mval->br_startoff + mval->br_blockcount;
		len = end - bno;
		if (n > 0 && mval->br_startoff == mval[-1].br_startoff) {
			ASSERT(mval->br_startblock == mval[-1].br_startblock);
			ASSERT(mval->br_blockcount > mval[-1].br_blockcount);
			mval[-1].br_blockcount = mval->br_blockcount;
		} else if (n > 0 && mval->br_startblock != DELAYSTARTBLOCK &&
			   mval[-1].br_startblock != DELAYSTARTBLOCK &&
			   mval[-1].br_startblock != HOLESTARTBLOCK &&
			   mval->br_startblock ==
			   mval[-1].br_startblock + mval[-1].br_blockcount) {
			ASSERT(mval->br_startoff ==
			       mval[-1].br_startoff + mval[-1].br_blockcount);
			mval[-1].br_blockcount += mval->br_blockcount;
		} else if (n > 0 &&
			   mval->br_startblock == DELAYSTARTBLOCK &&
			   mval[-1].br_startblock == DELAYSTARTBLOCK &&
			   mval->br_startoff ==
			   mval[-1].br_startoff + mval[-1].br_blockcount) {
			mval[-1].br_blockcount += mval->br_blockcount;
		} else if (!((n == 0) &&
			     ((mval->br_startoff + mval->br_blockcount) <=
			      obno))) {
			mval++;
			n++;
		}
		/*
		 * If we're done, stop now.  Stop when we've allocated
		 * XFS_BMAP_MAX_NMAP extents no matter what.  Otherwise
		 * the transaction may get too big.
		 */
		if (bno >= end || n >= *nmap || nallocs >= *nmap)
			break;
		/*
		 * Else go on to the next record.
		 */
		ep++;
		lastx++;
		if (lastx >= nextents) {
			eof = 1;
			prev = got;
		} else
			xfs_bmbt_get_all(ep, &got);
	}

	dcxp->dcx_lastext = lastx;
	*nmap = n;

	/* Let go the extent token */
	tkc_release1(dcxp->dcx_tokens, DVN_EXTENT_READ_1);

	return (0);
}
