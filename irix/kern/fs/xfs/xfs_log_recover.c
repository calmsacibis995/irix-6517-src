
#ident	"$Revision: 1.135 $"

#ifdef SIM
#define _KERNEL 1
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/sema.h>
#include <sys/vnode.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>

#ifdef SIM
#undef _KERNEL
#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#else
#include <sys/systm.h>
#include <sys/conf.h>
#endif
#include <limits.h>


#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/ktrace.h>
#include <sys/vfs.h>
#include <sys/uuid.h>
#include <stddef.h>

#include <sys/fs/xfs_macros.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_ag.h>		/* needed by xfs_sb.h */
#include <sys/fs/xfs_sb.h>		/* depends on xfs_types.h, xfs_inum.h */
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_mount.h>		/* depends on xfs_trans.h & xfs_sb.h */
#include <sys/fs/xfs_error.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_alloc.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_imap.h>
#include <sys/fs/xfs_inode_item.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_error.h>
#include <sys/fs/xfs_log_priv.h>	/* depends on all above */
#include <sys/fs/xfs_buf_item.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_log_recover.h>
#include <sys/fs/xfs_extfree_item.h>
#include <sys/fs/xfs_trans_priv.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_quota.h>
#include <sys/fs/xfs_dqblk.h>
#include <sys/fs/xfs_dquot_item.h>
#include <sys/fs/xfs_dquot.h>
#include <sys/fs/xfs_qm.h>
#include <sys/fs/xfs_rw.h>

#ifdef SIM
#include "sim.h"		/* must be last include file */
#endif

int		xlog_find_zeroed(struct log *log, daddr_t *blk_no);
int		xlog_find_cycle_start(struct log *log,
				      buf_t	*bp,	
				      daddr_t	first_blk,
				      daddr_t	*last_blk,
				      uint	cycle);

#ifndef SIM
STATIC int	xlog_clear_stale_blocks(xlog_t	*log, xfs_lsn_t tail_lsn);
#endif
STATIC void	xlog_recover_insert_item_backq(xlog_recover_item_t **q,
					       xlog_recover_item_t *item);
#ifndef SIM
STATIC void	xlog_recover_insert_item_frontq(xlog_recover_item_t **q,
						xlog_recover_item_t *item);
#endif /* !SIM */

#if defined(DEBUG) && !defined(SIM)
STATIC void	xlog_recover_check_summary(xlog_t *log);
STATIC void	xlog_recover_check_ail(xfs_mount_t *mp, xfs_log_item_t *lip,
				       int gen);
#else
#define	xlog_recover_check_summary(log)
#define	xlog_recover_check_ail(mp, lip, gen)
#endif /* DEBUG && !SIM */

#ifdef SIM
extern void 	xlog_recover_print_trans_head(xlog_recover_t *tr);
extern void	xlog_recover_print_logitem(xlog_recover_item_t *item);

STATIC void	xlog_recover_print_trans(xlog_recover_t	    *trans,
					 xlog_recover_item_t *itemq,
					 int		     print);
STATIC void	xlog_recover_print_item(xlog_recover_item_t *item);
#endif


buf_t *
xlog_get_bp(int num_bblks)
{
	buf_t   *bp;

	ASSERT(num_bblks > 0);
	bp = ngetrbuf(BBTOB(num_bblks));
	return bp;
}	/* xlog_get_bp */


void
xlog_put_bp(buf_t *bp)
{
	nfreerbuf(bp);
}	/* xlog_get_bp */


/*
 * nbblks should be uint, but oh well.  Just want to catch that 32-bit length.
 */
int
xlog_bread(xlog_t	*log,
	   daddr_t	blk_no,
	   int		nbblks,
	   buf_t	*bp)
{
	int error;

	ASSERT(nbblks > 0);
	ASSERT(BBTOB(nbblks) <= bp->b_bufsize);

	bp->b_blkno	= log->l_logBBstart + blk_no;
	bp->b_flags	= B_READ|B_BUSY;
	bp->b_bcount	= BBTOB(nbblks);
	bp->b_edev	= log->l_dev;
	bp->b_target	= &log->l_mp->m_logdev_targ;

#ifndef SIM
	bp_dcache_wbinval(bp);
#endif
	xfsbdstrat(log->l_mp, bp);
	if (error = iowait(bp)) {
		xfs_ioerror_alert("xlog_bread", log->l_mp, 
				  bp->b_edev, bp->b_blkno);
		return (error);
	}
	return error;
}	/* xlog_bread */


/*
 * Write out the buffer at the given block for the given number of blocks.
 * The buffer is kept locked across the write and is returned locked.
 * This can only be used for synchronous log writes.
 */
int
xlog_bwrite(
	xlog_t	*log,
	daddr_t	blk_no,
	int	nbblks,
	buf_t	*bp)
{
	int 	error;

	ASSERT(nbblks > 0);
	ASSERT(BBTOB(nbblks) <= bp->b_bufsize);

	bp->b_blkno	= log->l_logBBstart + blk_no;
	bp->b_flags	= B_BUSY | B_HOLD;
	bp->b_bcount	= BBTOB(nbblks);
	bp->b_edev	= log->l_dev;
	bp->b_target	= &log->l_mp->m_logdev_targ;

	if (error = xfs_bwrite(log->l_mp, bp)) 
		xfs_ioerror_alert("xlog_bwrite", log->l_mp, 
				  bp->b_edev, bp->b_blkno);
	
	return (error);
}	/* xlog_bwrite */

#ifndef SIM
STATIC void
xlog_recover_iodone(
	struct buf 	*bp)
{
	xfs_mount_t	*mp;
	ASSERT(bp->b_fsprivate);
	
	if (geterror(bp)) {
		/*
		 * We're not going to bother about retrying 
		 * this during recovery. One strike!
		 */
		mp = (xfs_mount_t *)bp->b_fsprivate;
		xfs_force_shutdown(mp, XFS_METADATA_IO_ERROR);
	}
	bp->b_fsprivate = NULL;
	bp->b_iodone = NULL;
	biodone(bp);
}
#endif

/*
 * This routine finds (to an approximation) the first block in the physical
 * log which contains the given cycle.  It uses a binary search algorithm.
 * Note that the algorithm can not be perfect because the disk will not
 * necessarily be perfect.
 */
int
xlog_find_cycle_start(xlog_t	*log,
		      buf_t	*bp,
		      daddr_t	first_blk,
		      daddr_t	*last_blk,
		      uint	cycle)
{
	daddr_t mid_blk;
	uint	mid_cycle;
	int	error;

	mid_blk = BLK_AVG(first_blk, *last_blk);
	while (mid_blk != first_blk && mid_blk != *last_blk) {
		if (error = xlog_bread(log, mid_blk, 1, bp))
			return error;
		mid_cycle = GET_CYCLE(bp->b_dmaaddr);
		if (mid_cycle == cycle) {
			*last_blk = mid_blk;
			/* last_half_cycle == mid_cycle */
		} else {
			first_blk = mid_blk;
			/* first_half_cycle == mid_cycle */
		}
		mid_blk = BLK_AVG(first_blk, *last_blk);
	}
	ASSERT((mid_blk == first_blk && mid_blk+1 == *last_blk) ||
	       (mid_blk == *last_blk && mid_blk-1 == first_blk));

	return 0;
}	/* xlog_find_cycle_start */


/*
 * Check that the range of blocks does not contain the cycle number
 * given.  The scan needs to occur from front to back and the ptr into the
 * region must be updated since a later routine will need to perform another
 * test.  If the region is completely good, we end up returning the same
 * last block number.
 *
 * Return -1 if we encounter no errors.  This is an invalid block number
 * since we don't ever expect logs to get this large.
 */
STATIC daddr_t
xlog_find_verify_cycle(caddr_t	*bap,		/* update ptr as we go */
		       daddr_t	start_blk,
		       int	nbblks,
		       uint	stop_on_cycle_no)
{
	int	i;
	uint	cycle;

	for (i=0; i<nbblks; i++) {
		cycle = GET_CYCLE(*bap);
		if (cycle != stop_on_cycle_no) {
			(*bap) += BBSIZE;
		} else {
			return (start_blk+i);
		}
	}
	return -1;
}	/* xlog_find_verify_cycle */


/*
 * Potentially backup over partial log record write.
 *
 * In the typical case, last_blk is the number of the block directly after
 * a good log record.  Therefore, we subtract one to get the block number
 * of the last block in the given buffer.  extra_bblks contains the number
 * of blocks we would have read on a previous read.  This happens when the
 * last log record is split over the end of the physical log.
 *
 * extra_bblks is the number of blocks potentially verified on a previous
 * call to this routine.
 */
STATIC int
xlog_find_verify_log_record(caddr_t	ba,	     /* update ptr as we go */
			    daddr_t	start_blk,
			    daddr_t	*last_blk,
			    int		extra_bblks)
{
    xlog_rec_header_t *rhead;
    daddr_t           i;

    ASSERT(start_blk != 0 || *last_blk != start_blk);

    ba -= BBSIZE;
    for (i=(*last_blk)-1; i>=0; i--) {
	if (i < start_blk) {
	    /* legal log record not found */
	    xlog_warn("XFS: xlog_find_verify_log_record: need to backup");
	    ASSERT(0);
	    return XFS_ERROR(EIO);
	}
	if (*(uint *)ba == XLOG_HEADER_MAGIC_NUM) {
	    break;
	}
	ba -= BBSIZE;
    }

    /*
     * We hit the beginning of the physical log & still no header.  Return
     * to caller.  If caller can handle a return of -1, then this routine
     * will be called again for the end of the physical log.
     */
    if (i == -1)
	    return -1;

    /*
     * We may have found a log record header before we expected one.
     * last_blk will be the 1st block # with a given cycle #.  We may end
     * up reading an entire log record.  In this case, we don't want to
     * reset last_blk.  Only when last_blk points in the middle of a log
     * record do we update last_blk.
     */
    rhead = (xlog_rec_header_t *)ba;
    if (*last_blk - i + extra_bblks != BTOBB(rhead->h_len)+1)
	    *last_blk = i;
    return 0;
}	/* xlog_find_verify_log_record */


/*
 * Head is defined to be the point of the log where the next log write
 * write could go.  This means that incomplete LR writes at the end are
 * eliminated when calculating the head.  We aren't guaranteed that previous
 * LR have complete transactions.  We only know that a cycle number of 
 * current cycle number -1 won't be present in the log if we start writing
 * from our current block number.
 *
 * last_blk contains the block number of the first block with a given
 * cycle number.
 *
 * Also called from xfs_log_print.c
 *
 * Return: zero if normal, non-zero if error.
 */
int
xlog_find_head(xlog_t  *log,
	       daddr_t *return_head_blk)
{
    buf_t   *bp, *big_bp;
    daddr_t new_blk, first_blk, start_blk, last_blk, head_blk;
    int     num_scan_bblks;
    uint    first_half_cycle, last_half_cycle;
    uint    stop_on_cycle;
    caddr_t ba;
    int     error, log_bbnum = log->l_logBBsize;

    /* special case freshly mkfs'ed filesystem; return immediately */
    if ((error = xlog_find_zeroed(log, &first_blk)) == -1) {
	*return_head_blk = first_blk;
	return 0;
    } else if (error)
	return error;

    first_blk = 0;				/* get cycle # of 1st block */
    bp = xlog_get_bp(1);
    if (error = xlog_bread(log, 0, 1, bp))
	goto bp_err;
    first_half_cycle = GET_CYCLE(bp->b_dmaaddr);

    last_blk = head_blk = log_bbnum-1;		/* get cycle # of last block */
    if (error = xlog_bread(log, last_blk, 1, bp))
	goto bp_err;
    last_half_cycle = GET_CYCLE(bp->b_dmaaddr);
    ASSERT(last_half_cycle != 0);

    /*
     * If the 1st half cycle number is equal to the last half cycle number,
     * then the entire log is stamped with the same cycle number.  In this
     * case, head_blk can't be set to zero (which makes sense).  The below
     * math doesn't work out properly with head_blk equal to zero.  Instead,
     * we set it to log_bbnum which is an illegal block number, but this
     * value makes the math correct.  If head_blk doesn't changed through
     * all the tests below, *head_blk is set to zero at the very end rather
     * than log_bbnum.  In a sense, log_bbnum and zero are the same block
     * in a circular file.
     */
    if (first_half_cycle == last_half_cycle) {
	/*
	 * In this case we believe that the entire log should have cycle
	 * number last_half_cycle.  We need to scan backwards from the
	 * end verifying that there are no holes still containing
	 * last_half_cycle - 1.  If we find such a hole, then the start
	 * of that hole will be the new head.  The simple case looks like
	 *        x | x ... | x - 1 | x
	 * Another case that fits this picture would be
	 *        x | x + 1 | x ... | x
	 * In this case the head really is somwhere at the end of the
	 * log, as one of the latest writes at the beginning was incomplete.
	 * One more case is
	 *        x | x + 1 | x ... | x - 1 | x
	 * This is really the combination of the above two cases, and the
	 * head has to end up at the start of the x-1 hole at the end of
	 * the log.
	 * 
	 * In the 256k log case, we will read from the beginning to the
	 * end of the log and search for cycle numbers equal to x-1.  We
	 * don't worry about the x+1 blocks that we encounter, because
	 * we know that they cannot be the head since the log started with
	 * x.
	 */
	head_blk = log_bbnum;
	stop_on_cycle = last_half_cycle - 1;
    } else {
	/*
	 * In this case we want to find the first block with cycle number
	 * matching last_half_cycle.  We expect the log to be some
	 * variation on
	 *        x + 1 ... | x ...
	 * The first block with cycle number x (last_half_cycle) will be
	 * where the new head belongs.  First we do a binary search for
	 * the first occurrence of last_half_cycle.  The binary search
	 * may not be totally accurate, so then we scan back from there
	 * looking for occurrences of last_half_cycle before us.  If
	 * that backwards scan wraps around the beginning of the log,
	 * then we look for occurrences of last_half_cycle - 1 at the
	 * end of the log.  The cases we're looking for look like
	 *        x + 1 ... | x | x + 1 | x ...
	 *                               ^ binary search stopped here
	 * or
	 *        x + 1 ... | x ... | x - 1 | x
	 *        <---------> less than scan distance
	 */
	stop_on_cycle = last_half_cycle;
	if (error = xlog_find_cycle_start(log, bp, first_blk,
					  &head_blk, last_half_cycle))
	    goto bp_err;
    }

    /*
     * Now validate the answer.  Scan back some number of maximum possible
     * blocks and make sure each one has the expected cycle number.  The
     * maximum is determined by the total possible amount of buffering
     * in the in-core log.  The following number can be made tighter if
     * we actually look at the block size of the filesystem.
     */
    num_scan_bblks = BTOBB(XLOG_MAX_ICLOGS<<XLOG_MAX_RECORD_BSHIFT);
    big_bp = xlog_get_bp(num_scan_bblks);
    if (head_blk >= num_scan_bblks) {
	/*
	 * We are guaranteed that the entire check can be performed
	 * in one buffer.
	 */
	start_blk = head_blk - num_scan_bblks;
	if (error = xlog_bread(log, start_blk, num_scan_bblks, big_bp))
	    goto big_bp_err;
	ba = big_bp->b_dmaaddr;
	new_blk = xlog_find_verify_cycle(&ba, start_blk, num_scan_bblks,
					 stop_on_cycle);
	if (new_blk != -1)
	    head_blk = new_blk;
    } else {			/* need to read 2 parts of log */
        /*
	 * We are going to scan backwards in the log in two parts.  First
	 * we scan the physical end of the log.  In this part of the log,
	 * we are looking for blocks with cycle number last_half_cycle - 1.
	 * If we find one, then we know that the log starts there, as we've
	 * found a hole that didn't get written in going around the end
	 * of the physical log.  The simple case for this is
	 *        x + 1 ... | x ... | x - 1 | x
	 *        <---------> less than scan distance
	 * If all of the blocks at the end of the log have cycle number
	 * last_half_cycle, then we check the blocks at the start of the
	 * log looking for occurrences of last_half_cycle.  If we find one,
	 * then our current estimate for the location of the first
	 * occurrence of last_half_cycle is wrong and we move back to the
	 * hole we've found.  This case looks like
	 *        x + 1 ... | x | x + 1 | x ...
	 *                               ^ binary search stopped here	 
	 * Another case we need to handle that only occurs in 256k logs is
	 *        x + 1 ... | x ... | x+1 | x ...
	 *                   ^ binary search stops here
	 * In a 256k log, the scan at the end of the log will see the x+1
	 * blocks.  We need to skip past those since that is certainly not
	 * the head of the log.  By searching for last_half_cycle-1 we
	 * accomplish that.
	 */
	start_blk = log_bbnum - num_scan_bblks + head_blk;
	ASSERT(head_blk <= INT_MAX && (daddr_t) num_scan_bblks-head_blk >= 0);
	if (error = xlog_bread(log, start_blk,
			num_scan_bblks-(int)head_blk, big_bp))
	    goto big_bp_err;
	ba = big_bp->b_dmaaddr;
	new_blk= xlog_find_verify_cycle(&ba, start_blk,
		     num_scan_bblks-(int)head_blk, (stop_on_cycle - 1));
	if (new_blk != -1) {
	    head_blk = new_blk;
	    goto bad_blk;
	}

	/*
	 * Scan beginning of log now.  The last part of the physical log
	 * is good.  This scan needs to verify that it doesn't find the
	 * last_half_cycle.
	 */
	start_blk = 0;
	ASSERT(head_blk <= INT_MAX);
	if (error = xlog_bread(log, start_blk, (int) head_blk, big_bp))
	    goto big_bp_err;
	ba = big_bp->b_dmaaddr;
	new_blk = xlog_find_verify_cycle(&ba, start_blk, (int) head_blk,
					 stop_on_cycle);
	if (new_blk != -1)
	    head_blk = new_blk;
    }

bad_blk:
    /*
     * Now we need to make sure head_blk is not pointing to a block in
     * the middle of a log record.
     */
    num_scan_bblks = BTOBB(XLOG_MAX_RECORD_BSIZE);
    if (head_blk >= num_scan_bblks) {
	start_blk = head_blk - num_scan_bblks;  /* don't read head_blk */
	if (error = xlog_bread(log, start_blk, num_scan_bblks, big_bp))
	    goto big_bp_err;

	/* start ptr at last block ptr before head_blk */
	ba = big_bp->b_dmaaddr + XLOG_MAX_RECORD_BSIZE;
	if ((error = xlog_find_verify_log_record(ba,
						 start_blk,
						 &head_blk,
						 0)) == -1) {
	    error = XFS_ERROR(EIO);
	    goto big_bp_err;
	} else if (error)
	    goto big_bp_err;
    } else {
	start_blk = 0;
	ASSERT(head_blk <= INT_MAX);
	if (error = xlog_bread(log, start_blk, (int)head_blk, big_bp))
	    goto big_bp_err;
	ba = big_bp->b_dmaaddr + BBTOB(head_blk);
	if ((error = xlog_find_verify_log_record(ba,
						 start_blk,
						 &head_blk,
						 0)) == -1) {
	    /* We hit the beginning of the log during our search */
	    start_blk = log_bbnum - num_scan_bblks + head_blk;
	    new_blk = log_bbnum;
	    ASSERT(start_blk <= INT_MAX && (daddr_t) log_bbnum-start_blk >= 0);
	    if (error = xlog_bread(log, start_blk, log_bbnum - (int)start_blk,
				   big_bp))
		goto big_bp_err;
	    ba = big_bp->b_dmaaddr + BBTOB(log_bbnum - start_blk);
	    ASSERT(head_blk <= INT_MAX);
	    if ((error = xlog_find_verify_log_record(ba,
						     start_blk,
						     &new_blk,
						     (int)head_blk)) == -1) {
		error = XFS_ERROR(EIO);
		goto big_bp_err;
	    } else if (error)
		goto big_bp_err;
	    if (new_blk != log_bbnum)
		head_blk = new_blk;
	} else if (error)
	    goto big_bp_err;
    }

    xlog_put_bp(big_bp);
    xlog_put_bp(bp);
    if (head_blk == log_bbnum)
	    *return_head_blk = 0;
    else
	    *return_head_blk = head_blk;
    /*
     * When returning here, we have a good block number.  Bad block
     * means that during a previous crash, we didn't have a clean break
     * from cycle number N to cycle number N-1.  In this case, we need
     * to find the first block with cycle number N-1.
     */
    return 0;

big_bp_err:
	xlog_put_bp(big_bp);
bp_err:
	xlog_put_bp(bp);
	return error;
}	/* xlog_find_head */


/*
 * Find the sync block number or the tail of the log.
 *
 * This will be the block number of the last record to have its
 * associated buffers synced to disk.  Every log record header has
 * a sync lsn embedded in it.  LSNs hold block numbers, so it is easy
 * to get a sync block number.  The only concern is to figure out which
 * log record header to believe.
 *
 * The following algorithm uses the log record header with the largest
 * lsn.  The entire log record does not need to be valid.  We only care
 * that the header is valid.
 *
 * We could speed up search by using current head_blk buffer, but it is not
 * available.
 */
#ifdef SIM
/* ARGSUSED */
#endif
int
xlog_find_tail(xlog_t  *log,
	       daddr_t *head_blk,
	       daddr_t *tail_blk,
	       int readonly)
{
	xlog_rec_header_t	*rhead;
	xlog_op_header_t	*op_head;
	buf_t			*bp;
	int			error, i, found;
	daddr_t			umount_data_blk;
	daddr_t			after_umount_blk;
#ifdef SIM
	/* REFERENCED */
#endif
	xfs_lsn_t		tail_lsn;
	
	found = error = 0;
	/*
	 * Find previous log record 
	 */
	if (error = xlog_find_head(log, head_blk))
		return error;

	bp = xlog_get_bp(1);
	if (*head_blk == 0) {				/* special case */
		if (error = xlog_bread(log, 0, 1, bp))
			goto bread_err;
		if (GET_CYCLE(bp->b_dmaaddr) == 0) {
			*tail_blk = 0;
			/* leave all other log inited values alone */
			goto exit;
		}
	}

	/*
	 * Search backwards looking for log record header block
	 */
	ASSERT(*head_blk < INT_MAX);
	for (i=(int)(*head_blk)-1; i>=0; i--) {
		if (error = xlog_bread(log, i, 1, bp))
			goto bread_err;
		if (*(uint *)(bp->b_dmaaddr) == XLOG_HEADER_MAGIC_NUM) {
			found = 1;
			break;
		}
	}
	/*
	 * If we haven't found the log record header block, start looking
	 * again from the end of the physical log.  XXXmiken: There should be
	 * a check here to make sure we didn't search more than N blocks in
	 * the previous code.
	 */
	if (!found) {
		for (i=log->l_logBBsize-1; i>=(int)(*head_blk); i--) {
			if (error = xlog_bread(log, i, 1, bp))
				goto bread_err;
			if (*(uint*)(bp->b_dmaaddr) == XLOG_HEADER_MAGIC_NUM) {
				found = 2;
				break;
			}
		}
	}
	if (!found) {
		xlog_warn("XFS: xlog_find_tail: couldn't find sync record");
		ASSERT(0);
		return XFS_ERROR(EIO);
	}

	/* find blk_no of tail of log */
	rhead = (xlog_rec_header_t *)bp->b_dmaaddr;
	*tail_blk = BLOCK_LSN(rhead->h_tail_lsn);

	/*
	 * Reset log values according to the state of the log when we
	 * crashed.  In the case where head_blk == 0, we bump curr_cycle
	 * one because the next write starts a new cycle rather than
	 * continuing the cycle of the last good log record.  At this
	 * point we have guaranteed that all partial log records have been
	 * accounted for.  Therefore, we know that the last good log record
	 * written was complete and ended exactly on the end boundary
	 * of the physical log.
	 */
	log->l_prev_block = i;
	log->l_curr_block = (int)*head_blk;
	log->l_curr_cycle = rhead->h_cycle;
	if (found == 2)
		log->l_curr_cycle++;
	log->l_tail_lsn = rhead->h_tail_lsn;
	log->l_last_sync_lsn = rhead->h_lsn;
	log->l_grant_reserve_cycle = log->l_curr_cycle;
	log->l_grant_reserve_bytes = BBTOB(log->l_curr_block);
	log->l_grant_write_cycle = log->l_curr_cycle;
	log->l_grant_write_bytes = BBTOB(log->l_curr_block);

	/*
	 * Look for unmount record.  If we find it, then we know there
	 * was a clean unmount.  Since 'i' could be the last block in
	 * the physical log, we convert to a log block before comparing
	 * to the head_blk.
	 *
	 * Save the current tail lsn to use to pass to
	 * xlog_clear_stale_blocks() below.  We won't want to clear the
	 * unmount record if there is one, so we pass the lsn of the
	 * unmount record rather than the block after it.
	 */
	after_umount_blk = (i + 2) % log->l_logBBsize;
	tail_lsn = log->l_tail_lsn;
	if (*head_blk == after_umount_blk && rhead->h_num_logops == 1) {
		umount_data_blk = (i + 1) % log->l_logBBsize;
		if (error = xlog_bread(log, umount_data_blk, 1, bp)) {
			goto bread_err;
		}
		op_head = (xlog_op_header_t *)bp->b_dmaaddr;
		if (op_head->oh_flags & XLOG_UNMOUNT_TRANS) {
			/*
			 * Set tail and last sync so that newly written
			 * log records will point recovery to after the
			 * current unmount record.
			 */
			log->l_tail_lsn =
			     ((long long)log->l_curr_cycle << 32) |
			     ((uint)(after_umount_blk));
			log->l_last_sync_lsn =
			     ((long long)log->l_curr_cycle << 32) |
			     ((uint)(after_umount_blk));
			*tail_blk = after_umount_blk;
		}
	}

#ifndef SIM
	/*
	 * Make sure that there are no blocks in front of the head
	 * with the same cycle number as the head.  This can happen
	 * because we allow multiple outstanding log writes concurrently,
	 * and the later writes might make it out before earlier ones.
	 *
	 * We use the lsn from before modifying it so that we'll never
	 * overwrite the unmount record after a clean unmount.
	 *
	 * Do this only if we are going to recover the filesystem
	 */
	if (!readonly)
		error = xlog_clear_stale_blocks(log, tail_lsn);
#endif /* !SIM */

bread_err:
exit:
	xlog_put_bp(bp);

	return error;
}	/* xlog_find_tail */


/*
 * Is the log zeroed at all?
 *
 * The last binary search should be changed to perform an X block read
 * once X becomes small enough.  You can then search linearly through
 * the X blocks.  This will cut down on the number of reads we need to do.
 *
 * If the log is partially zeroed, this routine will pass back the blkno
 * of the first block with cycle number 0.  It won't have a complete LR
 * preceding it.
 *
 * Return:
 *	0  => the log is completely written to
 *	-1 => use *blk_no as the first block of the log
 *	>0 => error has occurred
 */
int
xlog_find_zeroed(struct log	*log,
		 daddr_t 	*blk_no)
{
	buf_t	*bp, *big_bp;
	uint	first_cycle, last_cycle;
	daddr_t	new_blk, last_blk, start_blk;
	daddr_t num_scan_bblks;
	caddr_t	ba;
	int	error, log_bbnum = log->l_logBBsize;

	error = 0;
	/* check totally zeroed log */
	bp = xlog_get_bp(1);
	if (error = xlog_bread(log, 0, 1, bp))
		goto bp_err;
	first_cycle = GET_CYCLE(bp->b_dmaaddr);
	if (first_cycle == 0) {		/* completely zeroed log */
		*blk_no = 0;
		xlog_put_bp(bp);
		return -1;
	}

	/* check partially zeroed log */
	if (error = xlog_bread(log, log_bbnum-1, 1, bp))
		goto bp_err;
	last_cycle = GET_CYCLE(bp->b_dmaaddr);
	if (last_cycle != 0) {		/* log completely written to */
		xlog_put_bp(bp);
		return 0;
	} else if (first_cycle != 1) {
		/*
		 * Hopefully, this will catch the case where someone mkfs's
		 * over a log partition.
		 */
	xlog_warn("XFS: (xlog_find_zeroed): last cycle = 0; first cycle != 1");
		ASSERT(first_cycle == 1);
		return XFS_ERROR(EINVAL);
	}

	/* we have a partially zeroed log */
	last_blk = log_bbnum-1;
	if (error = xlog_find_cycle_start(log, bp, 0, &last_blk, 0))
		goto bp_err;

	/*
	 * Validate the answer.  Because there is no way to guarantee that
	 * the entire log is made up of log records which are the same size,
	 * we scan over the defined maximum blocks.  At this point, the maximum
	 * is not chosen to mean anything special.   XXXmiken
	 */
	num_scan_bblks = BTOBB(XLOG_MAX_ICLOGS<<XLOG_MAX_RECORD_BSHIFT);
	ASSERT(num_scan_bblks <= INT_MAX);
	big_bp = xlog_get_bp((int)num_scan_bblks);
	if (last_blk < num_scan_bblks)
		num_scan_bblks = last_blk;
	start_blk = last_blk - num_scan_bblks;
	if (error = xlog_bread(log, start_blk, (int)num_scan_bblks, big_bp))
		goto big_bp_err;
	ba = big_bp->b_dmaaddr;
	/*
	 * We search for any instances of cycle number 0 that occur before
	 * our current estimate of the head.  What we're trying to detect is
	 *        1 ... | 0 | 1 | 0...
	 *                       ^ binary search ends here
	 */
	new_blk = xlog_find_verify_cycle(&ba, start_blk,
					 (int)num_scan_bblks, 0);
	if (new_blk != -1)
		last_blk = new_blk;

	/*
	 * Potentially backup over partial log record write.  We don't need
	 * to search the end of the log because we know it is zero.
	 */
	if (error = xlog_find_verify_log_record(ba, start_blk, &last_blk, 0))
	    goto big_bp_err;

	*blk_no = last_blk;
big_bp_err:
	xlog_put_bp(big_bp);
bp_err:
	xlog_put_bp(bp);
	if (error)
		return error;
	return -1;
}	/* xlog_find_zeroed */

#ifndef SIM
/*
 * This is simply a subroutine used by xlog_clear_stale_blocks() below
 * to initialize a buffer full of empty log record headers and write
 * them into the log.
 */
STATIC int
xlog_write_log_records(
	xlog_t	*log,
	int	cycle,
	int	start_block,
	int	blocks,
	int	tail_cycle,
	int	tail_block,		       
	buf_t	*bp)
{
	xlog_rec_header_t	*recp;
	int			i;
	int			curr_block;
	int			error;

	recp = (xlog_rec_header_t*)(bp->b_un.b_addr);
	curr_block = start_block;
	for (i = 0; i < blocks; i++) {
		recp->h_magicno = XLOG_HEADER_MAGIC_NUM;
		recp->h_cycle = cycle;
		recp->h_version = 1;
		recp->h_len = 0;
		ASSIGN_ANY_LSN(recp->h_lsn, cycle, curr_block);
		ASSIGN_ANY_LSN(recp->h_tail_lsn, tail_cycle, tail_block);
		recp->h_chksum = 0;
		recp->h_prev_block = 0;	/* unused */
		recp->h_num_logops = 0;
		
		curr_block++;
		recp = (xlog_rec_header_t*)(((char *)recp) + BBSIZE);
	}

	error = xlog_bwrite(log, start_block, blocks, bp);

	return error;
}

/*
 * This routine is called to blow away any incomplete log writes out
 * in front of the log head.  We do this so that we won't become confused
 * if we come up, write only a little bit more, and then crash again.
 * If we leave the partial log records out there, this situation could
 * cause us to think those partial writes are valid blocks since they
 * have the current cycle number.  We get rid of them by overwriting them
 * with empty log records with the old cycle number rather than the
 * current one.
 *
 * The tail lsn is passed in rather than taken from
 * the log so that we will not write over the unmount record after a
 * clean unmount in a 512 block log.  Doing so would leave the log without
 * any valid log records in it until a new one was written.  If we crashed
 * during that time we would not be able to recover.
 */
STATIC int
xlog_clear_stale_blocks(
	xlog_t		*log,
	xfs_lsn_t	tail_lsn)
{
	int			tail_cycle;
	int			head_cycle;
	int			tail_block;
	int			head_block;
	int			tail_distance;
	int			max_distance;
	int			distance;
	buf_t			*bp;
	int			error;

	tail_cycle = CYCLE_LSN(tail_lsn);
	tail_block = BLOCK_LSN(tail_lsn);
	head_cycle = log->l_curr_cycle;
	head_block = log->l_curr_block;

	/*
	 * Figure out the distance between the new head of the log
	 * and the tail.  We want to write over any blocks beyond the
	 * head that we may have written just before the crash, but
	 * we don't want to overwrite the tail of the log.
	 */
	if (head_cycle == tail_cycle) {
		/*
		 * The tail is behind the head in the physical log,
		 * so the distance from the head to the tail is the
		 * distance from the head to the end of the log plus
		 * the distance from the beginning of the log to the
		 * tail.
		 */
		if (head_block < tail_block || head_block >= log->l_logBBsize)
			return XFS_ERROR(EFSCORRUPTED);
		tail_distance = tail_block +
				(log->l_logBBsize - head_block);
	} else {
		/*
		 * The head is behind the tail in the physical log,
		 * so the distance from the head to the tail is just
		 * the tail block minus the head block.
		 */
		if (head_block >= tail_block || head_cycle != (tail_cycle + 1))
			return XFS_ERROR(EFSCORRUPTED);
		tail_distance = tail_block - head_block;
	}

	/*
	 * If the head is right up against the tail, we can't clear
	 * anything.
	 */
	if (tail_distance <= 0) {
		ASSERT(tail_distance == 0);
		return 0;
	}

	max_distance = BTOBB(XLOG_MAX_ICLOGS << XLOG_MAX_RECORD_BSHIFT);
	/*
	 * Take the smaller of the maximum amount of outstanding I/O
	 * we could have and the distance to the tail to clear out.
	 * We take the smaller so that we don't overwrite the tail and
	 * we don't waste all day writing from the head to the tail
	 * for no reason.
	 */
	max_distance = MIN(max_distance, tail_distance);
	bp = xlog_get_bp(max_distance);
	
	if ((head_block + max_distance) <= log->l_logBBsize) {
		/*
		 * We can stomp all the blocks we need to without
		 * wrapping around the end of the log.  Just do it
		 * in a single write.  Use the cycle number of the
		 * current cycle minus one so that the log will look like:
		 *     n ... | n - 1 ...
		 */
		error = xlog_write_log_records(log, (head_cycle - 1),
				head_block, max_distance, tail_cycle,
				tail_block, bp);
		if (error) {
			xlog_put_bp(bp);
			return error;
		}
	} else {
		/*
		 * We need to wrap around the end of the physical log in
		 * order to clear all the blocks.  Do it in two separate
		 * I/Os.  The first write should be from the head to the
		 * end of the physical log, and it should use the current
		 * cycle number minus one just like above.
		 */
		distance = log->l_logBBsize - head_block;
		error = xlog_write_log_records(log, (head_cycle - 1),
				head_block, distance, tail_cycle,
				tail_block, bp);
		if (error) {
			xlog_put_bp(bp);
			return error;
		}

		/*
		 * Now write the blocks at the start of the physical log.
		 * This writes the remainder of the blocks we want to clear.
		 * It uses the current cycle number since we're now on the
		 * same cycle as the head so that we get:
		 *    n ... n ... | n - 1 ...
		 *    ^^^^^ blocks we're writing
		 */
		distance = max_distance - (log->l_logBBsize - head_block);
		error = xlog_write_log_records(log, head_cycle, 0, distance,
				tail_cycle, tail_block, bp);
		if (error) {
			xlog_put_bp(bp);
			return error;
		}
	}

	xlog_put_bp(bp);

	return 0;
}
#endif /* SIM */

/******************************************************************************
 *
 *		Log recover routines
 *
 ******************************************************************************
 */

STATIC xlog_recover_t *
xlog_recover_find_tid(xlog_recover_t *q,
		      xlog_tid_t     tid)
{
	xlog_recover_t *p = q;

	while (p != NULL) {
		if (p->r_log_tid == tid)
		    break;
		p = p->r_next;
	}
	return p;
}	/* xlog_recover_find_tid */


STATIC void
xlog_recover_put_hashq(xlog_recover_t **q,
		       xlog_recover_t *trans)
{
	trans->r_next = *q;
	*q = trans;
}	/* xlog_recover_put_hashq */


STATIC void
xlog_recover_add_item(xlog_recover_item_t **itemq)
{
	xlog_recover_item_t *item;

	item = kmem_zalloc(sizeof(xlog_recover_item_t), 0);
	xlog_recover_insert_item_backq(itemq, item);
}	/* xlog_recover_add_item */


STATIC int
xlog_recover_add_to_cont_trans(xlog_recover_t	*trans,
			       caddr_t		dp,
			       int		len)
{
	xlog_recover_item_t	*item;
	caddr_t			ptr, old_ptr;
	int			old_len;
	
	item = trans->r_itemq;
	if (item == 0) {
		/* finish copying rest of trans header */
		xlog_recover_add_item(&trans->r_itemq);
		ptr = (caddr_t)&trans->r_theader+sizeof(xfs_trans_header_t)-len;
		bcopy(dp, ptr, len); /* s, d, l */
		return 0;
	}
	item = item->ri_prev;

	old_ptr = item->ri_buf[item->ri_cnt-1].i_addr;
	old_len = item->ri_buf[item->ri_cnt-1].i_len;

	ptr = kmem_realloc(old_ptr, len+old_len, 0);
	bcopy(dp , &ptr[old_len], len);			/* s, d, l */
	item->ri_buf[item->ri_cnt-1].i_len += len;
	item->ri_buf[item->ri_cnt-1].i_addr = ptr;
	return 0;
}	/* xlog_recover_add_to_cont_trans */


/* The next region to add is the start of a new region.  It could be
 * a whole region or it could be the first part of a new region.  Because
 * of this, the assumption here is that the type and size fields of all
 * format structures fit into the first 32 bits of the structure.
 *
 * This works because all regions must be 32 bit aligned.  Therefore, we
 * either have both fields or we have neither field.  In the case we have
 * neither field, the data part of the region is zero length.  We only have
 * a log_op_header and can throw away the header since a new one will appear
 * later.  If we have at least 4 bytes, then we can determine how many regions
 * will appear in the current log item.
 */
STATIC int
xlog_recover_add_to_trans(xlog_recover_t	*trans,
			  caddr_t		dp,
			  int			len)
{
	xfs_inode_log_format_t	 *in_f;			/* any will do */
	xlog_recover_item_t	 *item;
	caddr_t			 ptr;

	if (!len)
		return 0;
	ptr = kmem_zalloc(len, 0);
	bcopy(dp, ptr, len);
	
	in_f = (xfs_inode_log_format_t *)ptr;
	item = trans->r_itemq;
	if (item == 0) {
		ASSERT(*(uint *)dp == XFS_TRANS_HEADER_MAGIC);
		if (len == sizeof(xfs_trans_header_t))
			xlog_recover_add_item(&trans->r_itemq);
		bcopy(dp, &trans->r_theader, len); /* s, d, l */
		return 0;
	}
	if (item->ri_prev->ri_total != 0 &&
	     item->ri_prev->ri_total == item->ri_prev->ri_cnt) {
		xlog_recover_add_item(&trans->r_itemq);
	}
	item = trans->r_itemq;
	item = item->ri_prev;

	if (item->ri_total == 0) {		/* first region to be added */
		item->ri_total	= in_f->ilf_size;
		ASSERT(item->ri_total <= XLOG_MAX_REGIONS_IN_ITEM);
		item->ri_buf = kmem_zalloc((item->ri_total *
					    sizeof(xfs_log_iovec_t)), 0);
	}
	ASSERT(item->ri_total > item->ri_cnt);
	/* Description region is ri_buf[0] */
	item->ri_buf[item->ri_cnt].i_addr = ptr;
	item->ri_buf[item->ri_cnt].i_len  = len;
	item->ri_cnt++;
	return 0;
}	/* xlog_recover_add_to_trans */


STATIC void
xlog_recover_new_tid(xlog_recover_t	**q,
		     xlog_tid_t		tid,
		     xfs_lsn_t		lsn)
{
	xlog_recover_t	*trans;

	trans = kmem_zalloc(sizeof(xlog_recover_t), 0);
	trans->r_log_tid   = tid;
	trans->r_lsn	   = lsn;
	xlog_recover_put_hashq(q, trans);
}	/* xlog_recover_new_tid */


STATIC int
xlog_recover_unlink_tid(xlog_recover_t	**q,
			xlog_recover_t	*trans)
{
	xlog_recover_t	*tp;
	int		found = 0;

	ASSERT(trans != 0);
	if (trans == *q) {
		*q = (*q)->r_next;
	} else {
		tp = *q;
		while (tp != 0) {
			if (tp->r_next == trans) {
				found = 1;
				break;
			}
			tp = tp->r_next;
		}
		if (!found) {
			xlog_warn(
			     "XFS: xlog_recover_unlink_tid: trans not found");
			ASSERT(0);
			return XFS_ERROR(EIO);
		}
		tp->r_next = tp->r_next->r_next;
	}
	return 0;
}	/* xlog_recover_unlink_tid */

STATIC void
xlog_recover_insert_item_backq(xlog_recover_item_t **q,
			       xlog_recover_item_t *item)
{
	if (*q == 0) {
		item->ri_prev = item->ri_next = item;
		*q = item;
	} else {
		item->ri_next		= *q;
		item->ri_prev		= (*q)->ri_prev;
		(*q)->ri_prev		= item;
		item->ri_prev->ri_next	= item;
	}
}	/* xlog_recover_insert_item_backq */


#ifndef SIM
STATIC void
xlog_recover_insert_item_frontq(xlog_recover_item_t **q,
				xlog_recover_item_t *item)
{
	xlog_recover_insert_item_backq(q, item);
	*q = item;
}	/* xlog_recover_insert_item_frontq */


/*ARGSUSED*/
STATIC int
xlog_recover_reorder_trans(xlog_t	  *log,
			   xlog_recover_t *trans)
{
    xlog_recover_item_t *first_item, *itemq, *itemq_next;

    first_item = itemq = trans->r_itemq;
    trans->r_itemq = NULL;
    do {
	itemq_next = itemq->ri_next;
	switch (ITEM_TYPE(itemq)) {
	    case XFS_LI_BUF:
	    case XFS_LI_6_1_BUF:
	    case XFS_LI_5_3_BUF: {
		xlog_recover_insert_item_frontq(&trans->r_itemq, itemq);
		break;
	    }
	    case XFS_LI_INODE:
	    case XFS_LI_6_1_INODE:
	    case XFS_LI_5_3_INODE:
	    case XFS_LI_DQUOT:
	    case XFS_LI_QUOTAOFF:
	    case XFS_LI_EFD:
	    case XFS_LI_EFI: {
		xlog_recover_insert_item_backq(&trans->r_itemq, itemq);
		break;
	    }
	    default: {
		xlog_warn(
	"XFS: xlog_recover_reorder_trans: unrecognized type of log operation");
		ASSERT(0);
		return XFS_ERROR(EIO);
	    }
	}
	itemq = itemq_next;
    } while (first_item != itemq);
    return 0;
}	/* xlog_recover_reorder_trans */


/*
 * Build up the table of buf cancel records so that we don't replay
 * cancelled data in the second pass.  For buffer records that are
 * not cancel records, there is nothing to do here so we just return.
 *
 * If we get a cancel record which is already in the table, this indicates
 * that the buffer was cancelled multiple times.  In order to ensure
 * that during pass 2 we keep the record in the table until we reach its
 * last occurrence in the log, we keep a reference count in the cancel
 * record in the table to tell us how many times we expect to see this
 * record during the second pass.
 */
STATIC void
xlog_recover_do_buffer_pass1(xlog_t			*log,
			     xfs_buf_log_format_t	*buf_f)
{
	xfs_buf_cancel_t	*bcp;
	xfs_buf_cancel_t	*nextp;
	xfs_buf_cancel_t	*prevp;
	xfs_buf_cancel_t	**bucket;
	xfs_buf_log_format_v1_t	*obuf_f;
	daddr_t			blkno;
	uint			len;
	ushort			flags;

	switch (buf_f->blf_type) {
	case XFS_LI_BUF:
		blkno = buf_f->blf_blkno;
		len = buf_f->blf_len;
		flags = buf_f->blf_flags;
		break;
	case XFS_LI_6_1_BUF:
	case XFS_LI_5_3_BUF:
		obuf_f = (xfs_buf_log_format_v1_t*)buf_f;
		blkno = (daddr_t) obuf_f->blf_blkno;
		len = obuf_f->blf_len;
		flags = obuf_f->blf_flags;
		break;
	}

	/*
	 * If this isn't a cancel buffer item, then just return.
	 */
	if (!(flags & XFS_BLI_CANCEL)) {
		return;
	}

	/*
	 * Insert an xfs_buf_cancel record into the hash table of
	 * them.  If there is already an identical record, bump
	 * its reference count.
	 */
	bucket = &log->l_buf_cancel_table[(__uint64_t)blkno %
					  XLOG_BC_TABLE_SIZE];
	/*
	 * If the hash bucket is empty then just insert a new record into
	 * the bucket.
	 */
	if (*bucket == NULL) {
		bcp = (xfs_buf_cancel_t*)kmem_alloc(sizeof(xfs_buf_cancel_t),
						    KM_SLEEP);
		bcp->bc_blkno = blkno;
		bcp->bc_len = len;
		bcp->bc_refcount = 1;
		bcp->bc_next = NULL;
		*bucket = bcp;
		return;
	}

	/*
	 * The hash bucket is not empty, so search for duplicates of our
	 * record.  If we find one them just bump its refcount.  If not
	 * then add us at the end of the list.
	 */
	prevp = NULL;
	nextp = *bucket;
	while (nextp != NULL) {
		if (nextp->bc_blkno == blkno && nextp->bc_len == len) {
			nextp->bc_refcount++;
			return;
		}
		prevp = nextp;
		nextp = nextp->bc_next;
	}
	ASSERT(prevp != NULL);
	bcp = (xfs_buf_cancel_t*)kmem_alloc(sizeof(xfs_buf_cancel_t),
					    KM_SLEEP);
	bcp->bc_blkno = blkno;
	bcp->bc_len = len;
	bcp->bc_refcount = 1;
	bcp->bc_next = NULL;
	prevp->bc_next = bcp;

	return;
}

/*
 * Check to see whether the buffer being recovered has a corresponding
 * entry in the buffer cancel record table.  If it does then return 1
 * so that it will be cancelled, otherwise return 0.  If the buffer is
 * actually a buffer cancel item (XFS_BLI_CANCEL is set), then decrement
 * the refcount on the entry in the table and remove it from the table
 * if this is the last reference.
 *
 * We remove the cancel record from the table when we encounter its
 * last occurrence in the log so that if the same buffer is re-used
 * again after its last cancellation we actually replay the changes
 * made at that point.
 */
STATIC int
xlog_recover_do_buffer_pass2(xlog_t			*log,
			     xfs_buf_log_format_t	*buf_f)
{
	xfs_buf_cancel_t	*bcp;
	xfs_buf_cancel_t	*prevp;
	xfs_buf_cancel_t	**bucket;
	xfs_buf_log_format_v1_t	*obuf_f;
	daddr_t			blkno;
	ushort			flags;
	uint			len;


	switch (buf_f->blf_type) {
	case XFS_LI_BUF:
		blkno = buf_f->blf_blkno;
		flags = buf_f->blf_flags;
		len = buf_f->blf_len;
		break;
	case XFS_LI_6_1_BUF:
	case XFS_LI_5_3_BUF:
		obuf_f = (xfs_buf_log_format_v1_t*)buf_f;
		blkno = (daddr_t) obuf_f->blf_blkno;
		flags = obuf_f->blf_flags;
		len = (daddr_t) obuf_f->blf_len;
		break;
	}
	if (log->l_buf_cancel_table == NULL) {
		/*
		 * There is nothing in the table built in pass one,
		 * so this buffer must not be cancelled.
		 */
		ASSERT(!(flags & XFS_BLI_CANCEL));
		return 0;
	}

	bucket = &log->l_buf_cancel_table[(__uint64_t)blkno %
					  XLOG_BC_TABLE_SIZE];
	bcp = *bucket;
	if (bcp == NULL) {
		/*
		 * There is no corresponding entry in the table built
		 * in pass one, so this buffer has not been cancelled.
		 */
		ASSERT(!(flags & XFS_BLI_CANCEL));
		return 0;
	}

	/*
	 * Search for an entry in the buffer cancel table that
	 * matches our buffer.
	 */
	prevp = NULL;
	while (bcp != NULL) {
		if (bcp->bc_blkno == blkno && bcp->bc_len == len) {
			/*
			 * We've go a match, so return 1 so that the
			 * recovery of this buffer is cancelled.
			 * If this buffer is actually a buffer cancel
			 * log item, then decrement the refcount on the
			 * one in the table and remove it if this is the
			 * last reference.
			 */
			if (flags & XFS_BLI_CANCEL) {
				bcp->bc_refcount--;
				if (bcp->bc_refcount == 0) {
					if (prevp == NULL) {
						*bucket = bcp->bc_next;
					} else {
						prevp->bc_next = bcp->bc_next;
					}
					kmem_free(bcp,
						  sizeof(xfs_buf_cancel_t));
				}
			}
			return 1;
		}
		prevp = bcp;
		bcp = bcp->bc_next;
	}
	/*
	 * We didn't find a corresponding entry in the table, so
	 * return 0 so that the buffer is NOT cancelled.
	 */
	ASSERT(!(flags & XFS_BLI_CANCEL));
	return 0;
}


/*
 * Perform recovery for a buffer full of inodes.  In these buffers,
 * the only data which should be recovered is that which corresponds
 * to the di_next_unlinked pointers in the on disk inode structures.
 * The rest of the data for the inodes is always logged through the
 * inodes themselves rather than the inode buffer and is recovered
 * in xlog_recover_do_inode_trans().
 *
 * The only time when buffers full of inodes are fully recovered is
 * when the buffer is full of newly allocated inodes.  In this case
 * the buffer will not be marked as an inode buffer and so will be
 * sent to xlog_recover_do_reg_buffer() below during recovery.
 */
STATIC int
xlog_recover_do_inode_buffer(xfs_mount_t		*mp,
			     xlog_recover_item_t	*item,
			     buf_t			*bp,
			     xfs_buf_log_format_t	*buf_f)
{
	int			i;
	int			item_index;
	int			bit;
	int			nbits;
	int			reg_buf_offset;
	int			reg_buf_bytes;
	int			next_unlinked_offset;
	int			inodes_per_buf;
	xfs_agino_t		*logged_nextp;
	xfs_agino_t		*buffer_nextp;
	xfs_buf_log_format_v1_t	*obuf_f;
	unsigned int		*data_map;
	unsigned int		map_size;

	switch (buf_f->blf_type) {
	case XFS_LI_BUF:
		data_map = buf_f->blf_data_map;
		map_size = buf_f->blf_map_size;
		break;
	case XFS_LI_6_1_BUF:
	case XFS_LI_5_3_BUF:
		obuf_f = (xfs_buf_log_format_v1_t*)buf_f;
		data_map = obuf_f->blf_data_map;
		map_size = obuf_f->blf_map_size;
		break;
	}
	/*
	 * Set the variables corresponding to the current region to
	 * 0 so that we'll initialize them on the first pass through
	 * the loop.
	 */
	reg_buf_offset = 0;
	reg_buf_bytes = 0;
	bit = 0;
	nbits = 0;
	item_index = 0;
	inodes_per_buf = bp->b_bcount >> mp->m_sb.sb_inodelog;
	for (i = 0; i < inodes_per_buf; i++) {
		next_unlinked_offset = (i * mp->m_sb.sb_inodesize) +
			offsetof(xfs_dinode_t, di_next_unlinked);

		while (next_unlinked_offset >=
		       (reg_buf_offset + reg_buf_bytes)) {
			/*
			 * The next di_next_unlinked field is beyond
			 * the current logged region.  Find the next
			 * logged region that contains or is beyond
			 * the current di_next_unlinked field.
			 */
			bit += nbits;
			bit = xfs_buf_item_next_bit(data_map, map_size, bit);

			/*
			 * If there are no more logged regions in the
			 * buffer, then we're done.
			 */
			if (bit == -1) {
				return 0;
			}

			nbits = xfs_buf_item_contig_bits(data_map, map_size,
							 bit);
			reg_buf_offset = bit << XFS_BLI_SHIFT;
			reg_buf_bytes = nbits << XFS_BLI_SHIFT;
			item_index++;
		}

		/*
		 * If the current logged region starts after the current
		 * di_next_unlinked field, then move on to the next
		 * di_next_unlinked field.
		 */
		if (next_unlinked_offset < reg_buf_offset) {
			continue;
		}

		ASSERT(item->ri_buf[item_index].i_addr != NULL);
		ASSERT((item->ri_buf[item_index].i_len % XFS_BLI_CHUNK) == 0);
		ASSERT((reg_buf_offset + reg_buf_bytes) <= bp->b_bcount);

		/*
		 * The current logged region contains a copy of the
		 * current di_next_unlinked field.  Extract its value
		 * and copy it to the buffer copy.
		 */
		logged_nextp = (xfs_agino_t *)
			       ((char *)(item->ri_buf[item_index].i_addr) +
				(next_unlinked_offset - reg_buf_offset));
		if (*logged_nextp == 0)  {
			xfs_fs_cmn_err(CE_ALERT, mp,
				"bad inode buffer log record (ptr = 0x%p, bp = 0x%p).  XFS trying to replay bad (0) inode di_next_unlinked field",
				item, bp);
			return XFS_ERROR(EFSCORRUPTED);
		}

		buffer_nextp = (xfs_agino_t *)((char *)(bp->b_un.b_addr) +
					      next_unlinked_offset);
		*buffer_nextp = *logged_nextp;
	}

	return 0;
}	/* xlog_recover_do_inode_buffer */

/*
 * Perform a 'normal' buffer recovery.  Each logged region of the
 * buffer should be copied over the corresponding region in the
 * given buffer.  The bitmap in the buf log format structure indicates
 * where to place the logged data.
 */
/*ARGSUSED*/
STATIC void
xlog_recover_do_reg_buffer(xfs_mount_t		*mp,
			   xlog_recover_item_t	*item,
			   buf_t		*bp,
			   xfs_buf_log_format_t	*buf_f)
{
	int			i;
	int			bit;
	int			nbits;
	xfs_buf_log_format_v1_t	*obuf_f;
	unsigned int		*data_map;
	unsigned int		map_size;
	int                     error;

	switch (buf_f->blf_type) {
	case XFS_LI_BUF:
		data_map = buf_f->blf_data_map;
		map_size = buf_f->blf_map_size;
		break;
	case XFS_LI_6_1_BUF:
	case XFS_LI_5_3_BUF:
		obuf_f = (xfs_buf_log_format_v1_t*)buf_f;
		data_map = obuf_f->blf_data_map;
		map_size = obuf_f->blf_map_size;
		break;
	}
	bit = 0;
	i = 1;  /* 0 is the buf format structure */
	while (1) {
		bit = xfs_buf_item_next_bit(data_map, map_size, bit);
		if (bit == -1)
			break;
		nbits = xfs_buf_item_contig_bits(data_map, map_size, bit);
		ASSERT(item->ri_buf[i].i_addr != 0);
		ASSERT(item->ri_buf[i].i_len % XFS_BLI_CHUNK == 0);
		ASSERT(bp->b_bcount >=
		       ((uint)bit << XFS_BLI_SHIFT)+(nbits<<XFS_BLI_SHIFT));
		
		/*
		 * Do a sanity check if this is a dquot buffer. Just checking the 
		 * first dquot in the buffer should do. XXXThis is
		 * probably a good thing to do for other buf types also.
		 */
		error = 0;
		if (buf_f->blf_flags & (XFS_BLI_UDQUOT_BUF|XFS_BLI_PDQUOT_BUF)) {
			/* OK, if this returns ENOPKG */
			error = xfs_qm_dqcheck((xfs_disk_dquot_t *)
					       item->ri_buf[i].i_addr,
					       -1, 0, XFS_QMOPT_DOWARN,
					       "dquot_buf_recover");
		}
		if (!error)
		    bcopy(item->ri_buf[i].i_addr,	           /* source */
		      bp->b_un.b_addr+((uint)bit << XFS_BLI_SHIFT),  /* dest */
		      nbits<<XFS_BLI_SHIFT);			   /* length */
		i++;
		bit += nbits;
	}

	/* Shouldn't be any more regions */
	ASSERT(i == item->ri_total);
}	/* xlog_recover_do_reg_buffer */


/*
 * Perform a dquot buffer recovery.
 * Simple algorithm: if we have found a QUOTAOFF logitem of the same type
 * (ie. USR or PRJ), then just toss this buffer away; don't recover it.
 * Else, treat it as a regular buffer and do recovery.
 */
STATIC void
xlog_recover_do_dquot_buffer(
	xfs_mount_t		*mp,
	xlog_t			*log,
	xlog_recover_item_t 	*item,
	buf_t			*bp,
	xfs_buf_log_format_t	*buf_f)
{
	uint type;

	/*
	 * Non-root filesystems are required to send in quota flags
	 * at mount time. However, we may also get QUOTA_MAYBE flag set,
	 * indicating that quota should stay on (and stay consistent),
	 * if it already is. (so, we have to replay dquot log records
	 * when MAYBE flag's set).
	 */
	if (mp->m_qflags == 0 &&
	    mp->m_dev != rootdev) {
		return;
	}
	
	type = 0;
	if (buf_f->blf_flags & XFS_BLI_UDQUOT_BUF)
		type |= XFS_DQ_USER;
	if (buf_f->blf_flags & XFS_BLI_PDQUOT_BUF)
		type |= XFS_DQ_PROJ;
	/*
	 * This type of quotas was turned off, so ignore this buffer
	 */
	if (log->l_quotaoffs_flag & type) 
		return;

	xlog_recover_do_reg_buffer(mp, item, bp, buf_f);
}

/*
 * This routine replays a modification made to a buffer at runtime.
 * There are actually two types of buffer, regular and inode, which
 * are handled differently.  Inode buffers are handled differently
 * in that we only recover a specific set of data from them, namely
 * the inode di_next_unlinked fields.  This is because all other inode
 * data is actually logged via inode records and any data we replay
 * here which overlaps that may be stale.
 *
 * When meta-data buffers are freed at run time we log a buffer item
 * with the XFS_BLI_CANCEL bit set to indicate that previous copies
 * of the buffer in the log should not be replayed at recovery time.
 * This is so that if the blocks covered by the buffer are reused for
 * file data before we crash we don't end up replaying old, freed
 * meta-data into a user's file.
 *
 * To handle the cancellation of buffer log items, we make two passes
 * over the log during recovery.  During the first we build a table of
 * those buffers which have been cancelled, and during the second we
 * only replay those buffers which do not have corresponding cancel
 * records in the table.  See xlog_recover_do_buffer_pass[1,2] above
 * for more details on the implementation of the table of cancel records.
 */
STATIC int
xlog_recover_do_buffer_trans(xlog_t		 *log,
			     xlog_recover_item_t *item,
			     int		 pass)
{
	xfs_buf_log_format_t	*buf_f;
	xfs_buf_log_format_v1_t	*obuf_f;
	xfs_mount_t	     	*mp;
	buf_t		     	*bp;
	int		     	error;
	int		     	cancel;
	daddr_t			blkno;
	int			len;
	ushort			flags;

	buf_f = (xfs_buf_log_format_t *)item->ri_buf[0].i_addr;

	if (pass == XLOG_RECOVER_PASS1) {
		/*
		 * In this pass we're only looking for buf items
		 * with the XFS_BLI_CANCEL bit set.
		 */
		xlog_recover_do_buffer_pass1(log, buf_f);
		return 0;
	} else {
		/*
		 * In this pass we want to recover all the buffers
		 * which have not been cancelled and are not
		 * cancellation buffers themselves.  The routine
		 * we call here will tell us whether or not to
		 * continue with the replay of this buffer.
		 */
		cancel = xlog_recover_do_buffer_pass2(log, buf_f);
		if (cancel) {
			return 0;
		}
	}
	switch (buf_f->blf_type) {
	      case XFS_LI_BUF:
		blkno = buf_f->blf_blkno;
		len = buf_f->blf_len;
		flags = buf_f->blf_flags;
		break;
	      case XFS_LI_6_1_BUF:
	      case XFS_LI_5_3_BUF:
		obuf_f = (xfs_buf_log_format_v1_t*)buf_f;
		blkno = obuf_f->blf_blkno;
		len = obuf_f->blf_len;
		flags = obuf_f->blf_flags;
		break;
	      default:
		xfs_fs_cmn_err(CE_ALERT, log->l_mp,
			"xfs_log_recover: unknown buffer type 0x%x, dev 0x%x", 
			buf_f->blf_type, log->l_dev);
		return XFS_ERROR(EFSCORRUPTED);
	}

	mp = log->l_mp;
	bp = read_buf_targ(mp->m_ddev_targp, blkno, len, 0);
	if (bp->b_flags & B_ERROR) {
		xfs_ioerror_alert("xlog_recover_do..(read)", log->l_mp, 
				  mp->m_dev, blkno);
		error = bp->b_error;
		brelse(bp);
		return error;
	}

	error = 0;
	if (flags & XFS_BLI_INODE_BUF) {
		error = xlog_recover_do_inode_buffer(mp, item, bp, buf_f);
	} else if (flags & (XFS_BLI_UDQUOT_BUF | XFS_BLI_PDQUOT_BUF)) {
		xlog_recover_do_dquot_buffer(mp, log, item, bp, buf_f);
	} else {
		xlog_recover_do_reg_buffer(mp, item, bp, buf_f);
	}
	if (error)
		return XFS_ERROR(error);

	/*
	 * Perform delayed write on the buffer.  Asynchronous writes will be
	 * slower when taking into account all the buffers to be flushed.
	 *
	 * Also make sure that only inode buffers with good sizes stay in
	 * the buffer cache.  The kernel moves inodes in buffers of 1 block
	 * or XFS_INODE_CLUSTER_SIZE bytes, whichever is bigger.  The inode
	 * buffers in the log can be a different size if the log was generated
	 * by an older kernel using unclustered inode buffers or a newer kernel
	 * running with a different inode cluster size.  Regardless, if the
	 * the inode buffer size isn't MAX(blocksize, XFS_INODE_CLUSTER_SIZE)
	 * for *our* value of XFS_INODE_CLUSTER_SIZE, then we need to keep
	 * the buffer out of the buffer cache so that the buffer won't
	 * overlap with future reads of those inodes.
	 */
	error = 0;
	if ((*((__uint16_t *)(bp->b_un.b_addr)) == XFS_DINODE_MAGIC) &&
	    (bp->b_bcount != MAX(log->l_mp->m_sb.sb_blocksize,
				 XFS_INODE_CLUSTER_SIZE(log->l_mp)))) { 
		bp->b_flags |= B_STALE;
	        error = xfs_bwrite(mp, bp);
	} else {
		ASSERT(bp->b_fsprivate == NULL || bp->b_fsprivate == mp);
		bp->b_fsprivate = mp;
		bp->b_iodone = xlog_recover_iodone;
		xfs_bdwrite(mp, bp);
	}

	return (error);
}	/* xlog_recover_do_buffer_trans */

STATIC int
xlog_recover_do_inode_trans(xlog_t		*log,
			    xlog_recover_item_t *item,
			    int			pass)
{
	xfs_inode_log_format_t	*in_f;
	xfs_mount_t		*mp;
	buf_t			*bp;
	xfs_imap_t		imap;
	xfs_dinode_t		*dip;
	xfs_ino_t		ino;
	int			len;
	caddr_t			src;
	caddr_t			dest;
	int			error;
	int			attr_index;
	uint			fields;
	xfs_dinode_core_t	*dicp;

	if (pass == XLOG_RECOVER_PASS1) {
		return 0;
	}

	in_f = (xfs_inode_log_format_t *)item->ri_buf[0].i_addr;
	ino = in_f->ilf_ino;
	mp = log->l_mp;
	if (ITEM_TYPE(item) == XFS_LI_INODE) {
		imap.im_blkno = (daddr_t)in_f->ilf_blkno;
		imap.im_len = in_f->ilf_len;
		imap.im_boffset = in_f->ilf_boffset;
	} else {
		/*
		 * It's an old inode format record.  We don't know where
		 * its cluster is located on disk, and we can't allow
		 * xfs_imap() to figure it out because the inode btrees
		 * are not ready to be used.  Therefore do not pass the
		 * XFS_IMAP_LOOKUP flag to xfs_imap().  This will give
		 * us only the single block in which the inode lives
		 * rather than its cluster, so we must make sure to
		 * invalidate the buffer when we write it out below.
		 */
		imap.im_blkno = 0;
		xfs_imap(log->l_mp, 0, ino, &imap, 0);
	}
	bp = read_buf_targ(mp->m_ddev_targp, imap.im_blkno, imap.im_len, 0);
	if (bp->b_flags & B_ERROR) {
		xfs_ioerror_alert("xlog_recover_do..(read)", mp, 
				  mp->m_dev, imap.im_blkno);
		error = bp->b_error;
		brelse(bp);
		return error;
	}
	error = 0;
	xfs_inobp_check(mp, bp);
	ASSERT(in_f->ilf_fields & XFS_ILOG_CORE);
	dip = (xfs_dinode_t *)(bp->b_un.b_addr+imap.im_boffset);

	/*
	 * Make sure the place we're flushing out to really looks
	 * like an inode!
	 */
	if (dip->di_core.di_magic != XFS_DINODE_MAGIC) {
		brelse(bp);
		xfs_fs_cmn_err(CE_ALERT, mp,
			"xfs_inode_recover: Bad inode magic number, dino ptr = 0x%p, dino bp = 0x%p, ino = %lld",
			dip, bp, ino);
		return XFS_ERROR(EFSCORRUPTED);
	}
	dicp = (xfs_dinode_core_t*)(item->ri_buf[1].i_addr);
	if (dicp->di_magic != XFS_DINODE_MAGIC) {
		brelse(bp);
		xfs_fs_cmn_err(CE_ALERT, mp,
			"xfs_inode_recover: Bad inode log record, rec ptr 0x%p, ino %lld",
			item, ino);
		return XFS_ERROR(EFSCORRUPTED);
	}
	if ((dicp->di_mode & IFMT) == IFREG) {
		if ((dicp->di_format != XFS_DINODE_FMT_EXTENTS) &&
		    (dicp->di_format != XFS_DINODE_FMT_BTREE)) {
			brelse(bp);
			xfs_fs_cmn_err(CE_ALERT, mp,
				"xfs_inode_recover: Bad regular inode log record, rec ptr 0x%p, ino ptr = 0x%p, ino bp = 0x%p, ino %lld",
				item, dip, bp, ino);
			return XFS_ERROR(EFSCORRUPTED);
		}
	} else if ((dicp->di_mode & IFMT) == IFDIR) {
		if ((dicp->di_format != XFS_DINODE_FMT_EXTENTS) &&
		    (dicp->di_format != XFS_DINODE_FMT_BTREE) &&
		    (dicp->di_format != XFS_DINODE_FMT_LOCAL)) {
			brelse(bp);
			xfs_fs_cmn_err(CE_ALERT, mp,
				"xfs_inode_recover: Bad dir inode log record, rec ptr 0x%p, ino ptr = 0x%p, ino bp = 0x%p, ino %lld",
				item, dip, bp, ino);
			return XFS_ERROR(EFSCORRUPTED);
		}
	}
	if (dicp->di_nextents + dicp->di_anextents > dicp->di_nblocks) {
		brelse(bp);
		xfs_fs_cmn_err(CE_ALERT, mp,
			"xfs_inode_recover: Bad inode log record, rec ptr 0x%p, dino ptr 0x%p, dino bp 0x%p, ino %lld, total extents = %d, nblocks = %lld",
			item, dip, bp, ino,
			dicp->di_nextents + dicp->di_anextents,
			dicp->di_nblocks);
		return XFS_ERROR(EFSCORRUPTED);
	}
	if (dicp->di_forkoff > mp->m_sb.sb_inodesize) {
		brelse(bp);
		xfs_fs_cmn_err(CE_ALERT, mp,
			"xfs_inode_recover: Bad inode log rec ptr 0x%p, dino ptr 0x%p, dino bp 0x%p, ino %lld, forkoff 0x%x",
			item, dip, bp, ino, dicp->di_forkoff);
		return XFS_ERROR(EFSCORRUPTED);
	}
	if (item->ri_buf[1].i_len > sizeof(xfs_dinode_core_t)) {
		brelse(bp);
		xfs_fs_cmn_err(CE_ALERT, mp,
			"xfs_inode_recover: Bad inode log record length %d, rec ptr 0x%p",
			item->ri_buf[1].i_len, item);
		return XFS_ERROR(EFSCORRUPTED);
	}

	ASSERT((caddr_t)dip+item->ri_buf[1].i_len <= bp->b_dmaaddr+bp->b_bcount);
	bcopy(item->ri_buf[1].i_addr, dip, item->ri_buf[1].i_len);

	if (in_f->ilf_size == 2)
		goto write_inode_buffer;
	len = item->ri_buf[2].i_len;
	src = item->ri_buf[2].i_addr;
	fields = in_f->ilf_fields;
	ASSERT(in_f->ilf_size <= 4);
	ASSERT((in_f->ilf_size == 3) || (fields & XFS_ILOG_AFORK));
	ASSERT(!(fields & XFS_ILOG_DFORK) ||
	       (len == in_f->ilf_dsize));

	switch (fields & (XFS_ILOG_DFORK | XFS_ILOG_DEV | XFS_ILOG_UUID)) {
	case XFS_ILOG_DDATA:
	case XFS_ILOG_DEXT:
		ASSERT((caddr_t)&dip->di_u+len <= bp->b_dmaaddr+bp->b_bcount);
		bcopy(src, &dip->di_u, len);
		break;

	case XFS_ILOG_DBROOT:
		xfs_bmbt_to_bmdr((xfs_bmbt_block_t *)src, len,
				 &(dip->di_u.di_bmbt),
				 XFS_DFORK_DSIZE(dip, mp));
		break;

	case XFS_ILOG_DEV:
		dip->di_u.di_dev = in_f->ilf_u.ilfu_rdev;
		break;

	case XFS_ILOG_UUID:
		dip->di_u.di_muuid = in_f->ilf_u.ilfu_uuid;
		break;

	default:
		/*
		 * There are no data fork, dev or uuid flags set.
		 */
		ASSERT((fields &
			(XFS_ILOG_DFORK|XFS_ILOG_DEV|XFS_ILOG_UUID)) == 0);
		break;
	}



	/*
	 * If we logged any attribute data, recover it.  There may or
	 * may not have been any other non-core data logged in this
	 * transaction.
	 */
	if (in_f->ilf_fields & XFS_ILOG_AFORK) {
		if (in_f->ilf_fields & XFS_ILOG_DFORK) {
			attr_index = 3;
		} else {
			attr_index = 2;
		}
		len = item->ri_buf[attr_index].i_len;
		src = item->ri_buf[attr_index].i_addr;
		ASSERT(len == in_f->ilf_asize);

		switch (in_f->ilf_fields & XFS_ILOG_AFORK) {
		case XFS_ILOG_ADATA:
		case XFS_ILOG_AEXT:
			dest = XFS_DFORK_APTR(dip);
			ASSERT(dest+len <= bp->b_dmaaddr+bp->b_bcount);
			ASSERT(len <= XFS_DFORK_ASIZE(dip, mp));
			bcopy(src, dest, len);
			break;

		case XFS_ILOG_ABROOT:
			dest = XFS_DFORK_APTR(dip);
			xfs_bmbt_to_bmdr((xfs_bmbt_block_t *)src, len,
					 (xfs_bmdr_block_t*)dest,
					 XFS_DFORK_ASIZE(dip, mp));
			break;

		default:
			xlog_warn("XFS: xlog_recover_do_inode_trans: Illegal flag");
			ASSERT(0);
			brelse(bp);
			return XFS_ERROR(EIO);
		}
	}


write_inode_buffer:
#if 0
	/*
	 * Can't do this if the transaction didn't log the current
	 * contents, e.g. rmdir.
	 */
	XFS_DIR_SHORTFORM_VALIDATE_ONDISK(mp, dip);
#endif
	xfs_inobp_check(mp, bp);
	if (ITEM_TYPE(item) == XFS_LI_INODE) {
		ASSERT(bp->b_fsprivate == NULL || bp->b_fsprivate == mp);
		bp->b_fsprivate = mp;
		bp->b_iodone = xlog_recover_iodone;
		xfs_bdwrite(mp, bp);
	} else {
		bp->b_flags |= B_STALE;
		error = xfs_bwrite(mp, bp);
	}

	return (error);
}	/* xlog_recover_do_inode_trans */


/*
 * Recover QUOTAOFF records. We simply make a note of it in the xlog_t
 * structure, so that we know not to do any dquot item or dquot buffer recovery,
 * of that type.
 */
STATIC int
xlog_recover_do_quotaoff_trans(xlog_t			*log,
			       xlog_recover_item_t	*item,
			       int			pass)
{
	xfs_qoff_logformat_t *qoff_f;

	if (pass == XLOG_RECOVER_PASS2) {
		return (0);
	}
	
	qoff_f = (xfs_qoff_logformat_t *)item->ri_buf[0].i_addr;
	ASSERT(qoff_f);

	/*
	 * The logitem format's flag tells us if this was user quotaoff, 
	 * project quotaoff or both. 
	 */
	if (qoff_f->qf_flags & XFS_UQUOTA_ACCT) 
		log->l_quotaoffs_flag |= XFS_DQ_USER;
	if (qoff_f->qf_flags & XFS_PQUOTA_ACCT)
		log->l_quotaoffs_flag |= XFS_DQ_PROJ;
	
	return (0);
}


/*
 * Recover a dquot record
 */
STATIC int
xlog_recover_do_dquot_trans(xlog_t		*log,
			    xlog_recover_item_t *item,
			    int			pass)
{
	xfs_mount_t		*mp;
	buf_t			*bp;
	struct xfs_disk_dquot	*ddq, *recddq;
	int			error;
	xfs_dq_logformat_t	*dq_f;
	uint			type;

	if (pass == XLOG_RECOVER_PASS1) {
		return 0;
	}
	mp = log->l_mp;

	/*
	 * Non-root filesystems are required to send in quota flags
	 * at mount time. However, we may also get QUOTA_MAYBE flag set,
	 * indicating that quota should stay on (and stay consistent),
	 * if it already is. (so, we have to replay dquot log records
	 * when MAYBE flag's set).
	 */
	if (mp->m_qflags == 0 &&
	    mp->m_dev != rootdev) {
		return (0);
	}
	
	recddq = (xfs_disk_dquot_t *)item->ri_buf[1].i_addr;
	ASSERT(recddq);
	/*
	 * This type of quotas was turned off, so ignore this record.
	 */
	type = recddq->d_flags & (XFS_DQ_USER|XFS_DQ_PROJ);
	ASSERT(type);
	if (log->l_quotaoffs_flag & type) 
		return (0);

	/*
	 * At this point we know that if we are recovering a root filesystem
	 * then quota was _not_ turned off. Since there is no other flag
	 * indicate to us otherwise, this must mean that quota's on, 
	 * and the dquot needs to be replayed. Remember that we may not have
	 * fully recovered the superblock yet, so we can't do the usual trick
	 * of looking at the SB quota bits.
	 *
	 * The other possibility, of course, is that the quota subsystem was
	 * removed since the last mount (ENOPKG).
	 */
	dq_f = (xfs_dq_logformat_t *)item->ri_buf[0].i_addr;
	ASSERT(dq_f);
	if (error = xfs_qm_dqcheck(recddq, 
			   dq_f->qlf_id,
			   0, XFS_QMOPT_DOWARN,
			   "xlog_recover_do_dquot_trans (log copy)")) {
		if (error == ENOPKG)
			return (0);
		return XFS_ERROR(EIO);
	}
	ASSERT(dq_f->qlf_len == 1);
	
	error = xfs_read_buf(mp, mp->m_ddev_targp,
			     dq_f->qlf_blkno, 
			     XFS_FSB_TO_BB(mp, dq_f->qlf_len),
			     0, &bp);
	if (error) {
		xfs_ioerror_alert("xlog_recover_do..(read)", mp, 
				  mp->m_dev, dq_f->qlf_blkno);
		return error;
	}
	ASSERT(bp);
	ddq = (xfs_disk_dquot_t *) ((char *)bp->b_un.b_addr + 
				    dq_f->qlf_boffset);
	
	/* 
	 * At least the magic num portion should be on disk because this
	 * was among a chunk of dquots created earlier, and we did some
	 * minimal initialization then.
	 */
	if (xfs_qm_dqcheck(ddq, dq_f->qlf_id, 0, XFS_QMOPT_DOWARN,
			   "xlog_recover_do_dquot_trans")) {
		brelse(bp);
		return XFS_ERROR(EIO);
	}
	ASSERT((caddr_t)ddq + item->ri_buf[1].i_len <=
	       bp->b_dmaaddr + bp->b_bcount);

	bcopy(recddq, ddq, item->ri_buf[1].i_len);

	ASSERT(dq_f->qlf_size == 2);
	ASSERT(bp->b_fsprivate == NULL || bp->b_fsprivate == mp);
	bp->b_fsprivate = mp;
	bp->b_iodone = xlog_recover_iodone;
	xfs_bdwrite(mp, bp);

	return (0);
}	/* xlog_recover_do_dquot_trans */

/*
 * This routine is called to create an in-core extent free intent
 * item from the efi format structure which was logged on disk.
 * It allocates an in-core efi, copies the extents from the format
 * structure into it, and adds the efi to the AIL with the given
 * LSN.
 */
STATIC void
xlog_recover_do_efi_trans(xlog_t		*log,
			  xlog_recover_item_t	*item,
			  xfs_lsn_t		lsn,
			  int			pass)	  
{
	xfs_mount_t		*mp;
	xfs_efi_log_item_t	*efip;
	xfs_efi_log_format_t	*efi_formatp;
	SPLDECL(spl);

	if (pass == XLOG_RECOVER_PASS1) {
		return;
	}

	efi_formatp = (xfs_efi_log_format_t *)item->ri_buf[0].i_addr;
	ASSERT(item->ri_buf[0].i_len ==
	       (sizeof(xfs_efi_log_format_t) +
		((efi_formatp->efi_nextents - 1) * sizeof(xfs_extent_t))));

	mp = log->l_mp;
	efip = xfs_efi_init(mp, efi_formatp->efi_nextents);
	bcopy((char *)efi_formatp, (char *)&(efip->efi_format),
	      sizeof(xfs_efi_log_format_t) +
	      ((efi_formatp->efi_nextents - 1) * sizeof(xfs_extent_t)));
	efip->efi_next_extent = efi_formatp->efi_nextents;
	efip->efi_flags |= XFS_EFI_COMMITTED;

	AIL_LOCK(mp,spl);
	/*
	 * xfs_trans_update_ail() drops the AIL lock.
	 */
 	xfs_trans_update_ail(mp, (xfs_log_item_t *)efip, lsn, spl);
}	/* xlog_recover_do_efi_trans */


/*
 * This routine is called when an efd format structure is found in
 * a committed transaction in the log.  It's purpose is to cancel
 * the corresponding efi if it was still in the log.  To do this
 * it searches the AIL for the efi with an id equal to that in the
 * efd format structure.  If we find it, we remove the efi from the
 * AIL and free it.
 */
STATIC void
xlog_recover_do_efd_trans(xlog_t		*log,
			  xlog_recover_item_t	*item,
			  int			pass)
{
	xfs_mount_t		*mp;
	xfs_efd_log_format_t	*efd_formatp;
	xfs_efi_log_item_t	*efip;
	xfs_log_item_t		*lip;
	int			gen;
	int			nexts;
	__uint64_t		efi_id;
	SPLDECL(spl);

	if (pass == XLOG_RECOVER_PASS1) {
		return;
	}

	efd_formatp = (xfs_efd_log_format_t *)item->ri_buf[0].i_addr;
	ASSERT(item->ri_buf[0].i_len ==
	       (sizeof(xfs_efd_log_format_t) +
		((efd_formatp->efd_nextents - 1) * sizeof(xfs_extent_t))));
	efi_id = efd_formatp->efd_efi_id;

	/*
	 * Search for the efi with the id in the efd format structure
	 * in the AIL.
	 */
	mp = log->l_mp;
	AIL_LOCK(mp,spl);
	lip = xfs_trans_first_ail(mp, &gen);
	while (lip != NULL) {
		if (lip->li_type == XFS_LI_EFI) {
			efip = (xfs_efi_log_item_t *)lip;
			if (efip->efi_format.efi_id == efi_id) {
				/*
				 * xfs_trans_delete_ail() drops the
				 * AIL lock.
				 */
				xfs_trans_delete_ail(mp, lip, spl);
				break;
			}
		}
		lip = xfs_trans_next_ail(mp, lip, &gen, NULL);
	}
	if (lip == NULL) {
		AIL_UNLOCK(mp, spl);
	}

	/*
	 * If we found it, then free it up.  If it wasn't there, it
	 * must have been overwritten in the log.  Oh well.
	 */
	if (lip != NULL) {
		nexts = efip->efi_format.efi_nextents;
		if (nexts > XFS_EFI_MAX_FAST_EXTENTS) {
			kmem_free(lip, sizeof(xfs_efi_log_item_t) +
				  ((nexts - 1) * sizeof(xfs_extent_t)));
		} else {
			kmem_zone_free(xfs_efi_zone, efip);
	}
	}
}	/* xlog_recover_do_efd_trans */
#endif /* !SIM */


/*
 * Perform the transaction
 *
 * If the transaction modifies a buffer or inode, do it now.  Otherwise,
 * EFIs and EFDs get queued up by adding entries into the AIL for them.
 */
#ifndef _KERNEL
/* ARGSUSED */
#endif
STATIC int
xlog_recover_do_trans(xlog_t	     *log,
		      xlog_recover_t *trans,
		      int	     pass)
{
	int		    error = 0;
#ifdef _KERNEL
	xlog_recover_item_t *item, *first_item;
#endif
#ifdef SIM
	if (pass == XLOG_RECOVER_PASS1)
		xlog_recover_print_trans(trans, trans->r_itemq, xlog_debug);
#endif

#ifdef _KERNEL
	if (error = xlog_recover_reorder_trans(log, trans))
		return error;
#ifdef SIM
	if (pass == XLOG_RECOVER_PASS1)
		xlog_recover_print_trans(trans, trans->r_itemq, xlog_debug+1);
#endif
	first_item = item = trans->r_itemq;
	do {
		/*
		 * we don't need to worry about the block number being
		 * truncated in > 1 TB buffers because in user-land,
		 * we're now n32 or 64-bit so daddr_t is 64-bits so
		 * the blkno's will get through the user-mode buffer
		 * cache properly.  The only bad case is o32 kernels
		 * where daddr_t is 32-bits but mount will warn us
		 * off a > 1 TB filesystem before we get here.
		 */
		if ((ITEM_TYPE(item) == XFS_LI_BUF) ||
		    (ITEM_TYPE(item) == XFS_LI_6_1_BUF) ||
		    (ITEM_TYPE(item) == XFS_LI_5_3_BUF)) {
			if (error = xlog_recover_do_buffer_trans(log, item,
								 pass))
				break;
		} else if ((ITEM_TYPE(item) == XFS_LI_INODE) ||
			   (ITEM_TYPE(item) == XFS_LI_6_1_INODE) ||
			   (ITEM_TYPE(item) == XFS_LI_5_3_INODE)) {
			if (error = xlog_recover_do_inode_trans(log, item,
								pass))
				break;
		} else if (ITEM_TYPE(item) == XFS_LI_EFI) {
			xlog_recover_do_efi_trans(log, item, trans->r_lsn,
						  pass);
		} else if (ITEM_TYPE(item) == XFS_LI_EFD) {
			xlog_recover_do_efd_trans(log, item, pass);
		} else if (ITEM_TYPE(item) == XFS_LI_DQUOT) {
			if (error = xlog_recover_do_dquot_trans(log, item,
								   pass))
					break;
		} else if ((ITEM_TYPE(item) == XFS_LI_QUOTAOFF)) {
			if (error = xlog_recover_do_quotaoff_trans(log, item,
								   pass))
					break;
		} else {
			xlog_warn("XFS: xlog_recover_do_trans");
			ASSERT(0);
			error = XFS_ERROR(EIO);
			break;
		}
		item = item->ri_next;
	} while (first_item != item);
#endif /* _KERNEL */

	return error;
}	/* xlog_recover_do_trans */


/*
 * Free up any resources allocated by the transaction
 *
 * Remember that EFIs, EFDs, and IUNLINKs are handled later.
 */
STATIC void
xlog_recover_free_trans(xlog_recover_t      *trans)
{
	xlog_recover_item_t *first_item, *item, *free_item;
	int i;

	item = first_item = trans->r_itemq;
	do {
		free_item = item;
		item = item->ri_next;
		 /* Free the regions in the item. */
		for (i = 0; i < free_item->ri_cnt; i++) {
			kmem_free(free_item->ri_buf[i].i_addr,
				  free_item->ri_buf[i].i_len);
		}
		/* Free the item itself */
		kmem_free(free_item->ri_buf,
			  (free_item->ri_total * sizeof(xfs_log_iovec_t)));
		kmem_free(free_item, sizeof(xlog_recover_item_t));
	} while (first_item != item);
	/* Free the transaction recover structure */
	kmem_free(trans, sizeof(xlog_recover_t));
}	/* xlog_recover_free_trans */


STATIC int
xlog_recover_commit_trans(xlog_t	 *log,
			  xlog_recover_t **q,
			  xlog_recover_t *trans,
			  int		 pass)
{
	int error;

	if (error = xlog_recover_unlink_tid(q, trans))
		return error;
	if (error = xlog_recover_do_trans(log, trans, pass))
		return error;
	xlog_recover_free_trans(trans);			/* no error */
	return 0;
}	/* xlog_recover_commit_trans */


/*ARGSUSED*/
STATIC int
xlog_recover_unmount_trans(xlog_recover_t *trans)
{
	/* Do nothing now */
	xlog_warn("XFS: xlog_recover_unmount_trans: Unmount LR");
	return( 0 );
}	/* xlog_recover_unmount_trans */


/*
 * There are two valid states of the r_state field.  0 indicates that the
 * transaction structure is in a normal state.  We have either seen the
 * start of the transaction or the last operation we added was not a partial
 * operation.  If the last operation we added to the transaction was a 
 * partial operation, we need to mark r_state with XLOG_WAS_CONT_TRANS.
 *
 * NOTE: skip LRs with 0 data length.
 */
STATIC int
xlog_recover_process_data(xlog_t	    *log,
			  xlog_recover_t    *rhash[],
			  xlog_rec_header_t *rhead,
			  caddr_t	    dp,
			  int		    pass)
{
    caddr_t		lp	   = dp+rhead->h_len;
    int			num_logops = rhead->h_num_logops;
    xlog_op_header_t	*ohead;
    xlog_recover_t	*trans;
    xlog_tid_t		tid;
    int			error;
    unsigned long	hash;
    uint		flags;

#ifdef SIM
    printf("LOG REC AT LSN cycle 0x%x blkno 0x%x\n",
	   CYCLE_LSN(rhead->h_lsn), BLOCK_LSN(rhead->h_lsn));
#endif
    while (dp < lp) {
	ASSERT(dp + sizeof(xlog_op_header_t) <= lp);
	ohead = (xlog_op_header_t *)dp;
	dp += sizeof(xlog_op_header_t);
	if (ohead->oh_clientid != XFS_TRANSACTION &&
	    ohead->oh_clientid != XFS_LOG) {
	    xlog_warn("XFS: xlog_recover_process_data: bad clientid ");
	    ASSERT(0);
	    return (XFS_ERROR(EIO));
        }
	tid = ohead->oh_tid;
	hash = XLOG_RHASH(tid);
	trans = xlog_recover_find_tid(rhash[hash], tid);
	if (trans == NULL) {			   /* not found; add new tid */
	    if (ohead->oh_flags & XLOG_START_TRANS)
		xlog_recover_new_tid(&rhash[hash], tid, rhead->h_lsn);
	} else {
	    ASSERT(dp+ohead->oh_len <= lp);
	    flags = ohead->oh_flags & ~XLOG_END_TRANS;
	    if (flags & XLOG_WAS_CONT_TRANS)
		flags &= ~XLOG_CONTINUE_TRANS;
	    switch (flags) {
		case XLOG_COMMIT_TRANS: {
		    error = xlog_recover_commit_trans(log, &rhash[hash],
						      trans, pass);
		    break;
		}
		case XLOG_UNMOUNT_TRANS: {
		    error = xlog_recover_unmount_trans(trans);
		    break;
		}
		case XLOG_WAS_CONT_TRANS: {
		    error = xlog_recover_add_to_cont_trans(trans, dp,
							   ohead->oh_len);
		    break;
		}
		case XLOG_START_TRANS : {
		    xlog_warn("XFS: xlog_recover_process_data: bad transaction");
		    ASSERT(0);
		    error = XFS_ERROR(EIO);
		    break;
		}
		case 0:
		case XLOG_CONTINUE_TRANS: {
		    error = xlog_recover_add_to_trans(trans, dp,
						      ohead->oh_len);
		    break;
		}
		default: {
		    xlog_warn("XFS: xlog_recover_process_data: bad flag");
		    ASSERT(0);
		    error = XFS_ERROR(EIO);
		    break;
		}
	    } /* switch */
	    if (error)
		return error;
	} /* if */
	dp += ohead->oh_len;
	num_logops--;
    }
    return( 0 );
}	/* xlog_recover_process_data */


#ifndef SIM
/*
 * Process an extent free intent item that was recovered from
 * the log.  We need to free the extents that it describes.
 */
STATIC void
xlog_recover_process_efi(xfs_mount_t		*mp,
			 xfs_efi_log_item_t	*efip)
{
	xfs_efd_log_item_t	*efdp;
	xfs_trans_t		*tp;
	int			i;
	xfs_extent_t		*extp;
	xfs_fsblock_t		startblock_fsb;

	ASSERT(!(efip->efi_flags & XFS_EFI_RECOVERED));

	/*
	 * First check the validity of the extents described by the
	 * EFI.  If any are bad, then assume that all are bad and
	 * just toss the EFI.
	 */
	for (i = 0; i < efip->efi_format.efi_nextents; i++) {
		extp = &(efip->efi_format.efi_extents[i]);
		startblock_fsb = XFS_BB_TO_FSB(mp,
				   XFS_FSB_TO_DADDR(mp, extp->ext_start));
		if ((startblock_fsb == 0) ||
		    (extp->ext_len == 0) ||
		    (startblock_fsb >= mp->m_sb.sb_dblocks) ||
		    (extp->ext_len >= mp->m_sb.sb_agblocks)) {
			/*
			 * This will pull the EFI from the AIL and
			 * free the memory associated with it.
			 */
			xfs_efi_release(efip, efip->efi_format.efi_nextents);
			return;
		}
	}

	tp = xfs_trans_alloc(mp, 0);
	xfs_trans_reserve(tp, 0, XFS_ITRUNCATE_LOG_RES(mp), 0, 0, 0);
	efdp = xfs_trans_get_efd(tp, efip, efip->efi_format.efi_nextents);

	for (i = 0; i < efip->efi_format.efi_nextents; i++) {
		extp = &(efip->efi_format.efi_extents[i]);
		xfs_free_extent(tp, extp->ext_start, extp->ext_len);
		xfs_trans_log_efd_extent(tp, efdp, extp->ext_start,
					 extp->ext_len);
	}

	efip->efi_flags |= XFS_EFI_RECOVERED;
	xfs_trans_commit(tp, 0, NULL);
}	/* xlog_recover_process_efi */
#endif	/* !SIM */


/*
 * Verify that once we've encountered something other than an EFI
 * in the AIL that there are no more EFIs in the AIL.
 */
#if defined(DEBUG) && !defined(SIM)
STATIC void
xlog_recover_check_ail(xfs_mount_t	*mp,
		       xfs_log_item_t	*lip,
		       int		gen)
{
	int	orig_gen;

	orig_gen = gen;
	do {
		ASSERT(lip->li_type != XFS_LI_EFI);
		lip = xfs_trans_next_ail(mp, lip, &gen, NULL);
		/*
		 * The check will be bogus if we restart from the
		 * beginning of the AIL, so ASSERT that we don't.
		 * We never should since we're holding the AIL lock
		 * the entire time.
		 */
		ASSERT(gen == orig_gen);
	} while (lip != NULL);
}
#endif	/* DEBUG && !SIM */


#ifndef SIM
/*
 * When this is called, all of the EFIs which did not have
 * corresponding EFDs should be in the AIL.  What we do now
 * is free the extents associated with each one.
 *
 * Since we process the EFIs in normal transactions, they
 * will be removed at some point after the commit.  This prevents
 * us from just walking down the list processing each one.
 * We'll use a flag in the EFI to skip those that we've already
 * processed and use the AIL iteration mechanism's generation
 * count to try to speed this up at least a bit.
 *
 * When we start, we know that the EFIs are the only things in
 * the AIL.  As we process them, however, other items are added
 * to the AIL.  Since everything added to the AIL must come after
 * everything already in the AIL, we stop processing as soon as
 * we see something other than an EFI in the AIL.
 */
STATIC void
xlog_recover_process_efis(xlog_t	*log)
{
	xfs_log_item_t		*lip;
	xfs_efi_log_item_t	*efip;
	int			gen;
	xfs_mount_t		*mp;
	SPLDECL(spl);

	mp = log->l_mp;
	AIL_LOCK(mp,spl);

	lip = xfs_trans_first_ail(mp, &gen);
	while (lip != NULL) {
		/*
		 * We're done when we see something other than an EFI.
		 */
		if (lip->li_type != XFS_LI_EFI) {
			xlog_recover_check_ail(mp, lip, gen);
			break;
		}

		/*
		 * Skip EFIs that we've already processed.
		 */
		efip = (xfs_efi_log_item_t *)lip;
		if (efip->efi_flags & XFS_EFI_RECOVERED) {
			lip = xfs_trans_next_ail(mp, lip, &gen, NULL);
			continue;
		}

		AIL_UNLOCK(mp, spl);
		xlog_recover_process_efi(mp, efip);
		AIL_LOCK(mp,spl);
		lip = xfs_trans_next_ail(mp, lip, &gen, NULL);
	}
	AIL_UNLOCK(mp, spl);
}	/* xlog_recover_process_efis */


/*
 * This routine performs a transaction to null out a bad inode pointer
 * in an agi unlinked inode hash bucket.
 */
STATIC void
xlog_recover_clear_agi_bucket(
	xfs_mount_t	*mp,
	xfs_agnumber_t	agno,			      
	int		bucket)
{
	xfs_trans_t	*tp;
	xfs_agi_t	*agi;
	daddr_t		agidaddr;
	buf_t		*agibp;
	int		offset;
	int		error;

	tp = xfs_trans_alloc(mp, XFS_TRANS_CLEAR_AGI_BUCKET);
	xfs_trans_reserve(tp, 0, XFS_CLEAR_AGI_BUCKET_LOG_RES(mp), 0, 0, 0);

	agidaddr = XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR);
	error = xfs_trans_read_buf(mp, tp, mp->m_dev, agidaddr, 1, 0, &agibp);
	if (error) {
		xfs_trans_cancel(tp, XFS_TRANS_ABORT);
		return;
	}
	agi = XFS_BUF_TO_AGI(agibp);
	if (agi->agi_magicnum != XFS_AGI_MAGIC) {
		xfs_trans_cancel(tp, XFS_TRANS_ABORT);
		return;
	}
	ASSERT(agi->agi_magicnum == XFS_AGI_MAGIC);

	agi->agi_unlinked[bucket] = NULLAGINO;
	offset = offsetof(xfs_agi_t, agi_unlinked) +
		 (sizeof(xfs_agino_t) * bucket);
	xfs_trans_log_buf(tp, agibp, offset,
			  (offset + sizeof(xfs_agino_t) - 1));

	(void) xfs_trans_commit(tp, 0, NULL);
}	/* xlog_recover_clear_agi_bucket */


/*
 * xlog_iunlink_recover
 *
 * This is called during recovery to process any inodes which
 * we unlinked but not freed when the system crashed.  These
 * inodes will be on the lists in the AGI blocks.  What we do
 * here is scan all the AGIs and fully truncate and free any
 * inodes found on the lists.  Each inode is removed from the
 * lists when it has been fully truncated and is freed.  The
 * freeing of the inode and its removal from the list must be
 * atomic.
 */
STATIC void
xlog_recover_process_iunlinks(xlog_t	*log)
{
	xfs_mount_t	*mp;
	xfs_agnumber_t	agno;
	xfs_agi_t	*agi;
	daddr_t		agidaddr;
	buf_t		*agibp;
	xfs_inode_t	*ip;
	xfs_agino_t	agino;
	xfs_ino_t	ino;
	int		bucket;
	int		error;
	xfs_ino_t	last_ino;

	mp = log->l_mp;
	last_ino = 0;
	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++) {
		/*
		 * Find the agi for this ag.
		 */
		agidaddr = XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR);
		agibp = read_buf_targ(mp->m_ddev_targp, agidaddr, 1, 0);
		agi = XFS_BUF_TO_AGI(agibp);
		ASSERT(agi->agi_magicnum == XFS_AGI_MAGIC);

		bucket = 0;
		while (bucket < XFS_AGI_UNLINKED_BUCKETS) {
			/*
			 * If there is nothing in the current bucket,
			 * then continue on to the next one.
			 */
			agino = agi->agi_unlinked[bucket];
			if (agino == NULLAGINO) {
				bucket++;
				continue;
			}

			/*
			 * Release the agi buffer so that it can
			 * be acquired in the normal course of the
			 * transaction to truncate and free the inode.
			 */
			brelse(agibp);

			ino = XFS_AGINO_TO_INO(mp, agno, agino);
			error = xfs_iget(mp, NULL, ino, 0, &ip, 0);

			/*
			 * This inode is messed up.  Just
			 * ditch this bucket of inodes.  We
			 * will lose some inodes and space,
			 * but at least we won't hang.
			 */
			if (!error &&
			    (ino != last_ino) &&
			    (ip->i_d.di_nlink == 0) &&
			    (ip->i_d.di_mode != 0)) {
				ASSERT(ip != NULL);
				ASSERT(ip->i_d.di_nlink == 0);
				ASSERT(ip->i_d.di_mode != 0);
				ASSERT(XFS_ITOV(ip)->v_count == 1);

				/*
				 * Drop our reference to the inode.  This
				 * will send the inode to xfs_inactive()
				 * which will truncate the file and free
				 * the inode.
				 */
				VN_RELE(XFS_ITOV(ip));
				last_ino = ino;
			} else {
				/*
				 * Skip this bucket if we can't read in
				 * the inode it points to.  Call
				 * xlog_recover_clear_agi_bucket()
				 * to perform a transaction to clear
				 * the inode pointer in the bucket.
				 */
				if (!error) {
					VN_RELE(XFS_ITOV(ip));
				}
				xlog_recover_clear_agi_bucket(mp, agno,
							      bucket);
				bucket++;
			}

			/*
			 * Reacquire the agibuffer and continue around
			 * the loop.
			 */
			agidaddr = XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR);
			agibp = read_buf_targ(mp->m_ddev_targp,
					 agidaddr, 1, 0);
			agi = XFS_BUF_TO_AGI(agibp);
			ASSERT(agi->agi_magicnum == XFS_AGI_MAGIC);
		}

		/*
		 * Release the buffer for the current agi so we can
		 * go on to the next one.
		 */
		brelse(agibp);
	}
}	/* xlog_recover_process_iunlinks */
#endif /* !SIM */


/*
 * Stamp cycle number in every block
 *
 * This routine is also called in xfs_log.c
 */
/*ARGSUSED*/
void
xlog_pack_data(xlog_t *log, xlog_in_core_t *iclog)
{
	int	i;
	caddr_t dp;
#ifdef DEBUG
	uint	*up;
	uint	chksum = 0;

	up = (uint *)iclog->ic_data;
	/* divide length by 4 to get # words */
	for (i=0; i<iclog->ic_offset >> 2; i++) {
		chksum ^= *up;
		up++;
	}
	iclog->ic_header.h_chksum = chksum;
#endif /* DEBUG */

	dp = iclog->ic_data;
	for (i = 0; i<BTOBB(iclog->ic_offset); i++) {
		iclog->ic_header.h_cycle_data[i] = *(uint *)dp;
		*(uint *)dp = CYCLE_LSN(iclog->ic_header.h_lsn);
		dp += BBSIZE;
	}
}	/* xlog_pack_data */


/*ARGSUSED*/
STATIC void
xlog_unpack_data(xlog_rec_header_t *rhead,
		 caddr_t	   dp,
		 xlog_t		   *log)
{
	int i;
#if defined(DEBUG) && defined(XFS_LOUD_RECOVERY)
	uint *up = (uint *)dp;
	uint chksum = 0;
#endif

	for (i=0; i<BTOBB(rhead->h_len); i++) {
		*(uint *)dp = *(uint *)&rhead->h_cycle_data[i];
		dp += BBSIZE;
	}
#if defined(DEBUG) && defined(XFS_LOUD_RECOVERY)
	/* divide length by 4 to get # words */
	for (i=0; i < rhead->h_len >> 2; i++) {
		chksum ^= *up;
		up++;
	}
	if (chksum != rhead->h_chksum) {
	    if (rhead->h_chksum != 0 ||
		((log->l_flags & XLOG_CHKSUM_MISMATCH) == 0)) {
		    cmn_err(CE_DEBUG,
		        "XFS: LogR chksum mismatch: was (0x%x) is (0x%x)\n",
			    rhead->h_chksum, chksum);
		    cmn_err(CE_DEBUG,
"XFS: Disregard message if filesystem was created with non-DEBUG kernel\n");
		    log->l_flags |= XLOG_CHKSUM_MISMATCH;
	    }
        }
#endif /* DEBUG && XFS_LOUD_RECOVERY */
}	/* xlog_unpack_data */


/*
 * Read the log from tail to head and process the log records found.
 * Handle the two cases where the tail and head are in the same cycle
 * and where the active portion of the log wraps around the end of
 * the physical log separately.  The pass parameter is passed through
 * to the routines called to process the data and is not looked at
 * here.
 */
STATIC int
xlog_do_recovery_pass(xlog_t	*log,
		      daddr_t	head_blk,
		      daddr_t	tail_blk,
		      int	pass)
{
    xlog_rec_header_t	*rhead;
    daddr_t		blk_no;
    caddr_t		bufaddr;
    buf_t		*hbp, *dbp;
    int			error;
    int		  	bblks, split_bblks;
    xlog_recover_t	*rhash[XLOG_RHASH_SIZE];

    error = 0;
    hbp = xlog_get_bp(1);
    dbp = xlog_get_bp(BTOBB(XLOG_MAX_RECORD_BSIZE));
    bzero(rhash, sizeof(rhash));
    if (tail_blk <= head_blk) {
	for (blk_no = tail_blk; blk_no < head_blk; ) {
	    if (error = xlog_bread(log, blk_no, 1, hbp))
		goto bread_err;
	    rhead = (xlog_rec_header_t *)hbp->b_dmaaddr;
	    ASSERT(rhead->h_magicno == XLOG_HEADER_MAGIC_NUM);
	    ASSERT(BTOBB(rhead->h_len <= INT_MAX));
	    bblks = (int) BTOBB(rhead->h_len);	/* blocks in data section */
	    if (bblks > 0) {
		if (error = xlog_bread(log, blk_no+1, bblks, dbp))
		    goto bread_err;
		xlog_unpack_data(rhead, dbp->b_dmaaddr, log);
		if (error = xlog_recover_process_data(log, rhash,
						      rhead, dbp->b_dmaaddr,
						      pass))
			goto bread_err;
	    }
	    blk_no += (bblks+1);
	}
    } else {
	/*
	 * Perform recovery around the end of the physical log.  When the head
	 * is not on the same cycle number as the tail, we can't do a sequential
	 * recovery as above.
	 */
	blk_no = tail_blk;
	while (blk_no < log->l_logBBsize) {

	    /* Read header of one block */
	    if (error = xlog_bread(log, blk_no, 1, hbp))
		goto bread_err;
	    rhead = (xlog_rec_header_t *)hbp->b_dmaaddr;
	    ASSERT(rhead->h_magicno == XLOG_HEADER_MAGIC_NUM);
	    ASSERT(BTOBB(rhead->h_len <= INT_MAX));
	    bblks = (int) BTOBB(rhead->h_len);

	    /* LR body must have data or it wouldn't have been written */
	    ASSERT(bblks > 0);
	    blk_no++;			/* successfully read header */
	    ASSERT(blk_no <= log->l_logBBsize);

	    if ((rhead->h_magicno != XLOG_HEADER_MAGIC_NUM) ||
		(BTOBB(rhead->h_len > INT_MAX)) ||
		(bblks <= 0) ||
		(blk_no > log->l_logBBsize)) {
		    error = EFSCORRUPTED;
		    goto bread_err;
	    }
		    
	    /* Read in data for log record */
	    if (blk_no+bblks <= log->l_logBBsize) {
		if (error = xlog_bread(log, blk_no, bblks, dbp))
		    goto bread_err;
	    } else {
		/* This log record is split across physical end of log */
		split_bblks = 0;
		if (blk_no != log->l_logBBsize) {

		    /* some data is before physical end of log */
		    ASSERT(blk_no <= INT_MAX);
		    split_bblks = log->l_logBBsize - (int)blk_no;
		    ASSERT(split_bblks > 0);
		    if (error = xlog_bread(log, blk_no, split_bblks, dbp))
			goto bread_err;
		}
		bufaddr = dbp->b_dmaaddr;
		dbp->b_dmaaddr += BBTOB(split_bblks);
		if (error = xlog_bread(log, 0, bblks - split_bblks, dbp))
		    goto bread_err;
		dbp->b_dmaaddr = bufaddr;
	    }
	    xlog_unpack_data(rhead, dbp->b_dmaaddr, log);
	    if (error = xlog_recover_process_data(log, rhash,
						  rhead, dbp->b_dmaaddr,
						  pass))
		goto bread_err;
	    blk_no += bblks;
	}

	ASSERT(blk_no >= log->l_logBBsize);
	blk_no -= log->l_logBBsize;

	/* read first part of physical log */
	while (blk_no < head_blk) {
	    if (error = xlog_bread(log, blk_no, 1, hbp))
		goto bread_err;
	    rhead = (xlog_rec_header_t *)hbp->b_dmaaddr;
	    ASSERT(rhead->h_magicno == XLOG_HEADER_MAGIC_NUM);
	    ASSERT(BTOBB(rhead->h_len <= INT_MAX));
	    bblks = (int) BTOBB(rhead->h_len);
	    ASSERT(bblks > 0);
	    if (error = xlog_bread(log, blk_no+1, bblks, dbp))
		goto bread_err;
	    xlog_unpack_data(rhead, dbp->b_dmaaddr, log);
	    if (error = xlog_recover_process_data(log, rhash,
						  rhead, dbp->b_dmaaddr,
						  pass))
		goto bread_err;
	    blk_no += (bblks+1);
        }
    }

bread_err:
    xlog_put_bp(dbp);
    xlog_put_bp(hbp);

    return error;
}

/*
 * Do the recovery of the log.  We actually do this in two phases.
 * The two passes are necessary in order to implement the function
 * of cancelling a record written into the log.  The first pass
 * determines those things which have been cancelled, and the
 * second pass replays log items normally except for those which
 * have been cancelled.  The handling of the replay and cancellations
 * takes place in the log item type specific routines.
 *
 * The table of items which have cancel records in the log is allocated
 * and freed at this level, since only here do we know when all of
 * the log recovery has been completed.
 */
STATIC int
xlog_do_log_recovery(xlog_t	*log,
		     daddr_t	head_blk,
		     daddr_t	tail_blk)
{
	int		error;
#ifdef DEBUG
	int		i;
#endif

	/*
	 * First do a pass to find all of the cancelled buf log items.
	 * Store them in the buf_cancel_table for use in the second pass.
	 */
	log->l_buf_cancel_table =
		(xfs_buf_cancel_t **)kmem_zalloc(XLOG_BC_TABLE_SIZE *
						 sizeof(xfs_buf_cancel_t*),
						 KM_SLEEP);
	error = xlog_do_recovery_pass(log, head_blk, tail_blk,
				      XLOG_RECOVER_PASS1);
	if (error != 0) {
		kmem_free(log->l_buf_cancel_table,
			  XLOG_BC_TABLE_SIZE * sizeof(xfs_buf_cancel_t*));
		log->l_buf_cancel_table = NULL;
		return error;
	}
	/*
	 * Then do a second pass to actually recover the items in the log.
	 * When it is complete free the table of buf cancel items.
	 */
	error = xlog_do_recovery_pass(log, head_blk, tail_blk,
				      XLOG_RECOVER_PASS2);
#ifdef	DEBUG
	for (i = 0; i < XLOG_BC_TABLE_SIZE; i++) {
		ASSERT(log->l_buf_cancel_table[i] == NULL);
	}
#endif	/* DEBUG */
	kmem_free(log->l_buf_cancel_table,
		  XLOG_BC_TABLE_SIZE * sizeof(xfs_buf_cancel_t*));
	log->l_buf_cancel_table = NULL;

	return error;
}

/*
 * Do the actual recovery
 */
STATIC int
xlog_do_recover(xlog_t	*log,
		daddr_t head_blk,
		daddr_t tail_blk)
{
#ifdef _KERNEL
	buf_t		*bp;
	xfs_sb_t	*sbp;
#endif
	int		error;

	/*
	 * First replay the images in the log.
	 */
	error = xlog_do_log_recovery(log, head_blk, tail_blk);
	if (error) {
		return error;
	}

#ifdef _KERNEL
	bflush(log->l_mp->m_dev);    /* Flush out the delayed write buffers */
	/*
	 * If IO errors happened during recovery, bail out.
	 */
	if (XFS_FORCED_SHUTDOWN(log->l_mp)) {
		return (EIO);
	}

	/*
	 * We now update the tail_lsn since much of the recovery has completed
	 * and there may be space available to use.  If there were no extent
	 * or iunlinks, we can free up the entire log and set the tail_lsn to
	 * be the last_sync_lsn.  This was set in xlog_find_tail to be the
	 * lsn of the last known good LR on disk.  If there are extent frees
	 * or iunlinks they will have some entries in the AIL; so we look at
	 * the AIL to determine how to set the tail_lsn.
	 */
	xlog_assign_tail_lsn(log->l_mp, 0);

	/*
	 * Now that we've finished replaying all buffer and inode
	 * updates, re-read in the superblock.
	 */
	bp = xfs_getsb(log->l_mp, 0);
	bp->b_flags &= ~B_DONE;
	bp->b_flags |= B_READ;
#ifndef SIM
	bp_dcache_wbinval(bp);
#endif
	xfsbdstrat(log->l_mp, bp);
	if (error = iowait(bp)) {
		ASSERT(0);
		brelse(bp);
		return error;
	}
	sbp = XFS_BUF_TO_SBP(bp);
	ASSERT(sbp->sb_magicnum == XFS_SB_MAGIC);
	ASSERT(XFS_SB_GOOD_VERSION(sbp));
	log->l_mp->m_sb = *sbp;
	brelse(bp);

	xlog_recover_check_summary(log);

	/* Normal transactions can now occur */
	log->l_flags &= ~XLOG_ACTIVE_RECOVERY;
#endif /* _KERNEL */
	return 0;
}	/* xlog_do_recover */


/*
 * Perform recovery and re-initialize some log variables in xlog_find_tail.
 *
 * Return error or zero.
 */
int
xlog_recover(xlog_t *log, int readonly)
{
	daddr_t head_blk, tail_blk;
	int	error;

	if (error = xlog_find_tail(log, &head_blk, &tail_blk, readonly))
		return error;
	if (tail_blk != head_blk) {
#ifdef SIM
		extern daddr_t HEAD_BLK, TAIL_BLK;
		head_blk = HEAD_BLK;
		tail_blk = TAIL_BLK;
#endif
		/*
		 * disallow recovery on read-only mounts.  note -- mount
		 * checks for ENOSPC and turns it into an intelligent
		 * error message.
		 */
		if (readonly)
			return ENOSPC;
#ifdef _KERNEL
#if defined(DEBUG) && defined(XFS_LOUD_RECOVERY)
		cmn_err(CE_NOTE,
			"Starting XFS recovery on filesystem: %s (dev: %d/%d)",
			log->l_mp->m_fsname, emajor(log->l_dev),
			eminor(log->l_dev));
#else
		cmn_err(CE_NOTE,
			"!Starting XFS recovery on filesystem: %s (dev: %d/%d)",
			log->l_mp->m_fsname, emajor(log->l_dev),
			eminor(log->l_dev));
#endif
#endif
		error = xlog_do_recover(log, head_blk, tail_blk);
		log->l_flags |= XLOG_RECOVERY_NEEDED;
	}
	return error;
}	/* xlog_recover */


/*
 * In the first part of recovery we replay inodes and buffers and build
 * up the list of extent free items which need to be processed.  Here
 * we process the extent free items and clean up the on disk unlinked
 * inode lists.  This is separated from the first part of recovery so
 * that the root and real-time bitmap inodes can be read in from disk in
 * between the two stages.  This is necessary so that we can free space
 * in the real-time portion of the file system.
 */
int
xlog_recover_finish(xlog_t *log)
{
	/*
	 * Now we're ready to do the transactions needed for the
	 * rest of recovery.  Start with completing all the extent
	 * free intent records and then process the unlinked inode
	 * lists.  At this point, we essentially run in normal mode
	 * except that we're still performing recovery actions
	 * rather than accepting new requests.
	 */
	if (log->l_flags & XLOG_RECOVERY_NEEDED) {
#ifdef _KERNEL
		xlog_recover_process_efis(log);
		/*
		 * Sync the log to get all the EFIs out of the AIL.
		 * This isn't absolutely necessary, but it helps in
		 * case the unlink transactions would have problems
		 * pushing the EFIs out of the way.
		 */
		xfs_log_force(log->l_mp, (xfs_lsn_t)0,
			      (XFS_LOG_FORCE | XFS_LOG_SYNC));
		xlog_recover_process_iunlinks(log);
		xlog_recover_check_summary(log);
#endif /* _KERNEL */
#if defined(DEBUG) && defined(XFS_LOUD_RECOVERY)
		cmn_err(CE_NOTE,
			"Ending XFS recovery for filesystem: %s (%V)",
			log->l_mp->m_fsname, log->l_dev);
#else
		cmn_err(CE_NOTE,
			"!Ending XFS recovery for filesystem: %s (%V)",
			log->l_mp->m_fsname, log->l_dev);
#endif
		log->l_flags &= ~XLOG_RECOVERY_NEEDED;
	} else {
		cmn_err(CE_NOTE,
			"!Ending clean XFS mount for filesystem: %s",
			log->l_mp->m_fsname);
	}
	return 0;
}	/* xlog_recover_finish */


#if defined(DEBUG) && !defined(SIM)
/*
 * Read all of the agf and agi counters and check that they
 * are consistent with the superblock counters.
 */
void
xlog_recover_check_summary(xlog_t	*log)
{
	xfs_mount_t	*mp;
	xfs_agf_t	*agfp;
	xfs_agi_t	*agip;
	buf_t		*agfbp;
	buf_t		*agibp;
	daddr_t		agfdaddr;
	daddr_t		agidaddr;
	buf_t		*sbbp;
#ifdef XFS_LOUD_RECOVERY
	xfs_sb_t	*sbp;
#endif
	xfs_agnumber_t	agno;
	__uint64_t	freeblks;
	__uint64_t	itotal;
	__uint64_t	ifree;

	mp = log->l_mp;
	freeblks = 0LL;
	itotal = 0LL;
	ifree = 0LL;
	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++) {
		agfdaddr = XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR);
		agfbp = read_buf_targ(mp->m_ddev_targp, agfdaddr, 1, 0);
		agfp = XFS_BUF_TO_AGF(agfbp);
		ASSERT(agfp->agf_magicnum == XFS_AGF_MAGIC);
		ASSERT(XFS_AGF_GOOD_VERSION(agfp->agf_versionnum));
		ASSERT(agfp->agf_seqno == agno);

		freeblks += agfp->agf_freeblks + agfp->agf_flcount;
		brelse(agfbp);

		agidaddr = XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR);
		agibp = read_buf_targ(mp->m_ddev_targp, agidaddr, 1, 0);
		agip = XFS_BUF_TO_AGI(agibp);
		ASSERT(agip->agi_magicnum == XFS_AGI_MAGIC);
		ASSERT(XFS_AGI_GOOD_VERSION(agip->agi_versionnum));
		ASSERT(agip->agi_seqno == agno);

		itotal += agip->agi_count;
		ifree += agip->agi_freecount;
		brelse(agibp);
	}

	sbbp = xfs_getsb(mp, 0);
#ifdef XFS_LOUD_RECOVERY
	sbp = XFS_BUF_TO_SBP(sbbp);
	cmn_err(CE_NOTE,
		"xlog_recover_check_summary: sb_icount %llu itotal %llu",
		sbp->sb_icount, itotal);
	cmn_err(CE_NOTE,
		"xlog_recover_check_summary: sb_ifree %llu itotal %llu",
		sbp->sb_ifree, ifree);
	cmn_err(CE_NOTE,
		"xlog_recover_check_summary: sb_fdblocks %llu freeblks %llu",
		sbp->sb_fdblocks, freeblks);
#if 0
	/*
	 * This is turned off until I account for the allocation
	 * btree blocks which live in free space.
	 */
	ASSERT(sbp->sb_icount == itotal);
	ASSERT(sbp->sb_ifree == ifree);
	ASSERT(sbp->sb_fdblocks == freeblks);
#endif
#endif
	brelse(sbbp);
}
#endif /* DEBUG && !SIM */


#ifdef SIM

STATIC void
xlog_recover_print_item(xlog_recover_item_t *item)
{
	int i;

	switch (ITEM_TYPE(item)) {
	    case XFS_LI_BUF: {
		printf("BUF");
		break;
	    }
	    case XFS_LI_INODE: {
		printf("INO");
		break;
	    }
	    case XFS_LI_EFD: {
		printf("EFD");
		break;
	    }
	    case XFS_LI_EFI: {
		printf("EFI");
		break;
	    }
	    case XFS_LI_6_1_BUF:  {
		printf("6.1 BUF");
		break;
	    }
	    case XFS_LI_5_3_BUF: {
		printf("5.3 BUF");
		break;
	    }
	    case XFS_LI_6_1_INODE: {
		printf("6.1 INO");
		break;
	    }
	    case XFS_LI_5_3_INODE: {
		printf("5.3 INO");
		break;
	    }
	    case XFS_LI_DQUOT: {
		printf("DQ ");
		break;
	    }
	    case XFS_LI_QUOTAOFF: {
		printf("QOFF");
		break;
	    } 
	    default: {
		cmn_err(CE_PANIC, "xlog_recover_print_item: illegal type\n");
		break;
	    }
	}

/*	type isn't filled in yet
	printf("ITEM: type: %d cnt: %d total: %d ",
	       item->ri_type, item->ri_cnt, item->ri_total);
*/
	printf(": cnt:%d total:%d ", item->ri_cnt, item->ri_total);
	for (i=0; i<item->ri_cnt; i++) {
		printf("a:0x%x len:%d ",
		       item->ri_buf[i].i_addr, item->ri_buf[i].i_len);
	}
	printf("\n");
	xlog_recover_print_logitem(item);
}	/* xlog_recover_print_item */

/*ARGSUSED*/
STATIC void
xlog_recover_print_trans(xlog_recover_t	     *trans,
			 xlog_recover_item_t *itemq,
			 int		     print)
{
	xlog_recover_item_t *first_item, *item;

	if (print < 3)
		return;

	printf("======================================\n");
	xlog_recover_print_trans_head(trans);
	item = first_item = itemq;
	do {
		xlog_recover_print_item(item);
		item = item->ri_next;
	} while (first_item != item);
}	/* xlog_recover_print_trans */
#endif
