/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA. */
/*
 * $Source: /proj/irix6.5.7m/isms/irix/kern/io/RCS/disksub.c,v $
 * $Revision: 3.29 $
 * $Date: 1998/04/23 23:36:58 $
 *
 *  Misc subroutines used by disk drivers
 */
#include "sys/param.h"
#include "sys/types.h"
#include "sys/buf.h"
#include "sys/iobuf.h"
#include "sys/dvh.h"
#include "sys/debug.h"
#include "sys/systm.h"
#include "sys/grio.h"

#define SORTDEBUG 0
#if SORTDEBUG
int sortdebug = 0;
#endif


#ifdef DEBUG
/*
 * Check for two bufs with overlapping I/O to the disk.
 * One of them must be a write to cause a real problem.
 */
static void
check_overlap(
	struct buf *ap,
	struct buf *bp)
{
	if (bp->b_sort < ap->b_sort + BTOBB(ap->b_bcount) &&
	    bp->b_sort + BTOBB(bp->b_bcount) > ap->b_sort)
	{
		if (!(ap->b_flags & B_READ) || !(bp->b_flags & B_READ))
			printf("WARNING: overlapped writes");
		else
			printf("NOTE: overlapped reads");
		printf("; dev %x, buf 0x%x flags 0x%x bn %d len %d, buf 0x%x flags 0x%x bn %d len %d\n",
		       ap->b_edev, ap, ap->b_flags, ap->b_sort, ap->b_bcount/NBPSCTR,
				  bp, bp->b_flags, bp->b_sort, bp->b_bcount/NBPSCTR);
	}
}
#else
#define check_overlap(a,b)
#endif

/*
 * Seek sort for disks.  The driver has already decomposed the disk address
 * into b_sort.
 *
 * The argument dp structure holds a av_forw activity chain pointer
 * on which we keep two queues, sorted in ascending cylinder order.
 * The first queue holds those requests which are positioned after
 * the current cylinder (in the first request); the second holds
 * requests which came in after their cylinder number was passed.
 * Thus we implement a one way scan, retracting after reaching the
 * end of the drive to the first request on the second queue,
 * at which time it becomes the first queue.
 *
 * A one-way scan is natural because of the way UNIX read-ahead
 * blocks are allocated.
 */
void disksort(register struct iobuf *dp, register struct buf *bp)
{
	register struct buf *ap;

	/*
	 * If nothing on the activity queue, then
	 * we become the only thing.
	 */
	ap = dp->io_head;
	if (ap == NULL) {
		dp->io_head = bp;
		dp->io_tail = bp;
		bp->av_forw = NULL;
		return;
	}

	/*
	 * If we lie after the first (currently active)
	 * request, then we must locate the second request list
	 * and add ourselves to it.
	 */
	check_overlap(ap, bp);
	if (bp->b_sort < ap->b_sort) {
		while (ap->av_forw) {
			/*
			 * Check for an ``inversion'' in the
			 * normally ascending cylinder numbers,
			 * indicating the start of the second request list.
			 */
			check_overlap(ap->av_forw, bp);
			if (ap->av_forw->b_sort < ap->b_sort) {
				/*
				 * Search the second request list
				 * for the first request at a larger
				 * cylinder number.  We go before that;
				 * if there is no such request, we go at end.
				 */
				do {
					check_overlap(ap->av_forw, bp);
					if (bp->b_sort < ap->av_forw->b_sort)
						goto insert;
					ap = ap->av_forw;
				} while (ap->av_forw);
				goto insert;		/* after last */
			}
			ap = ap->av_forw;
		}
		/*
		 * No inversions... we will go after the last, and
		 * be the first request in the second request list.
		 */
		goto insert;
	}

	/*
	 * Request is at/after the current request...
	 * sort in the first request list.
	 */
	while (ap->av_forw) {
		/*
		 * We want to go after the current request
		 * if there is an inversion after it (i.e. it is
		 * the end of the first request list), or if
		 * the next request is a larger cylinder than our request.
		 */
		check_overlap(ap->av_forw, bp);
		if (ap->av_forw->b_sort < ap->b_sort ||
		    bp->b_sort < ap->av_forw->b_sort)
			goto insert;
		ap = ap->av_forw;
	}
	/*
	 * Neither a second list nor a larger
	 * request... we go at the end of the first list,
	 * which is the same as the end of the whole schebang.
	 */
insert:
	/* insert bp after ap */
	/* update io_tail */
	bp->av_forw = ap->av_forw;
	ap->av_forw = bp;
	if (ap == dp->io_tail)
		dp->io_tail = bp;
}


/*
 * Slightly different version of disksort for controllers and drives which
 * have more than one command active (tagged commands, command queueing,
 * MACSI mode, etc.).  The main difference is that macsisort puts a command
 * into the second sort queue if its logical block number is less than the
 * logical block number of the last command sent to the controller (stored
 * in av_back by the driver).
 * The idea is to have two lists, one following the other.  In the first list
 * are the bufs whose starting block numbers are greater than the starting
 * block number of the command last sent to the controller.  In the second list
 * are those commands whose starting block numbers are less than the starting
 * block number of the last command sent to the controller.  The last command
 * on the first list points to the first command on the second list.
 */
void
macsisort(register struct iobuf *utab, register struct buf *bp)
{ 
	register struct buf *ap;
	register unsigned int bn;
	register unsigned int fbn;

	if (BUF_IS_IOSPL(bp)) {
#if 0
		short queueid;

		switch (bp->b_flags & (B_GR_ISD|B_PRV_BUF)) {
			case B_GR_ISD : queueid = 1;	/* lowest hi pri q */
					break;
			case B_PRV_BUF: queueid = bp->b_iopri;
					break;
			default :	/* B_GR_ISD | B_PRV_BUF */
					GET_GRIO_IOPRI(bp, queueid);
					break;
		}

		printf("macsisort : q %d\n", queueid);
#endif
		/* We use strict FIFO order for the priority queue */
		ap = utab->prio_head;
		/*
		 * empty queue case
		 */
		if (ap == NULL)
		{
			utab->prio_head = bp;
			utab->prio_tail = bp;
			bp->av_forw = NULL;
			goto macsi_sort_end;
		} else {
			utab->prio_tail->av_forw = bp;
			bp->av_forw = NULL;
			utab->prio_tail = bp;
			goto macsi_sort_end;
		}
			
	} else {
		ap = utab->io_head;
		/*
		 * empty queue case
		 */
		if (ap == NULL)
		{
			utab->io_head = bp;
			utab->io_tail = bp;
			bp->av_forw = NULL;
			goto macsi_sort_end;
		}

		bn = bp->b_sort;
		/*
		 * if block number < bn of last command to be submitted, AND
		 *         the first buf has a bn >= bn of last command,
		 *   go forward until we hit buf with bn < bn of first buf
		 *   then insert somewhere in second list
		 * else if we are at beginning of list,
		 *   insert there.
		 * else
		 *   insert somewhere in first list
		 */
		if (bn < utab->io_state.prevblk && ap->b_sort >= utab->io_state.prevblk)
		{
			fbn = ap->b_sort;
			while (ap->av_forw != NULL &&
			       ap->av_forw->b_sort >= fbn)
				ap = ap->av_forw;
			while (ap->av_forw != NULL &&
			       bn > ap->av_forw->b_sort)
				ap = ap->av_forw;
			goto macsi_sort_insert;
		}
		else if (bn <= ap->b_sort)
		{
			bp->av_forw = ap;
			utab->io_head = bp;
			goto macsi_sort_end;
		}
		else
		{
			fbn = ap->b_sort;
			while (ap->av_forw != NULL &&
			       fbn <= ap->av_forw->b_sort &&
			       bn > ap->av_forw->b_sort)
				ap = ap->av_forw;
		}

		/*
		 * Insert in list.
		 */
macsi_sort_insert:
		/* insert bp after ap */
		/* update io_tail if necessary */
		bp->av_forw = ap->av_forw;
		ap->av_forw = bp;
		if (bp->av_forw == NULL)
			utab->io_tail = bp;
	}

macsi_sort_end:
#ifdef GRIO_DEBUG
		if (BUF_IS_IOSPL(bp))
			printf("macsisort: bp->b_bcount %d\n", bp->b_bcount);
#endif
		bp->b_resid = bp->b_bcount;
#if SORTDEBUG
		bn = 0;
		fbn = utab->io_head->b_sort;

		for (ap = utab->io_head; ap->av_forw != NULL; ap = ap->av_forw)
		{
			if (ap->av_forw->b_sort < ap->b_sort)
				break;
		}
		for (; ap->av_forw != NULL; ap = ap->av_forw)
		{
			if (ap->av_forw->b_sort < ap->b_sort)
				bn++;
			if (ap->av_forw->b_sort > fbn)
				bn++;
		}

		if (bn > 1 || sortdebug != 0)
		{
			ap = utab->io_head;
			printf("macsisort (%x):", utab->io_state.prevblk);
			while (ap != NULL)
			{
				printf(" %x", ap->b_sort);
				ap = ap->av_forw;
			}
			printf("\n");
			ASSERT(bn < 2);
		}
#endif
}


/*
 * nosort()
 *      Add request to queue. Queue is manged in first in, first out order.
 *
 */
void
nosort(register struct iobuf *utab, register struct buf *bp)
{
	if (utab->io_head == NULL)
	{
		utab->io_head = bp;
		utab->io_tail = bp;
	}
	else
	{
		utab->io_tail->av_forw = bp;
		utab->io_tail = bp;
	}
	bp->b_resid = bp->b_bcount;
}


struct buf *
fair_disk_dequeue(struct iobuf *utab)
{
	struct buf	*bp;
	struct buf	*nbp;

	/* Try the priority queue first. No fairness imposed here which means
	 * that priority requests could starve out other requests. We also do
	 * not currently re-sort the other queues based on the bn of the
	 * request at the head of the priority queue.
	 */
	bp = utab->prio_head;
	if (bp != NULL) {
		utab->prio_head = bp->av_forw;
		if (bp->av_forw == NULL)
			utab->prio_tail = NULL;
#ifdef GRIO_DEBUG
		printf("fair_disk_dequeue: dequeued %llx\n", bp);
#endif
		return bp;
	}

	/* If priority queue is empty, try the normal queue. Fairness *is*
	 * imposed here
	 */
	bp = utab->io_head;
	if (bp == NULL)
	{
		return NULL;
	}

	if (bp->b_sort >= utab->io_state.prevblk && bp->b_sort <= utab->seq_next)
	{
		/*
		 * Only push this sequence to the end if it's passed the
		 * limit and we haven't moved this buffer before.  Only
		 * moving the buffer once prevents starvation.
		 */
		if ((utab->seq_count >= FAIR_DEQUEUE_LIMIT) &&
		    !(bp->b_flags & B_SHUFFLED))
		{
			/*
			 * We've processed too many requests from one
			 * sequential stream in a row.  Find the next
			 * non-sequential request in the queue if there
			 * is one and make that the current head instead.
			 */
			while ((nbp = bp->av_forw) != NULL)
			{
				if ((bp->b_sort + BTOBB(bp->b_bcount)) ==
				    nbp->b_sort)
				{
					bp = nbp;
				}
				else
				{
					break;
				}
			}
			if (nbp != NULL)
			{
				utab->io_tail->av_forw = utab->io_head;
				/*
				 * bp at this point is the buffer just ahead
				 * of nbp in the queue.  Mark it with the
				 * B_SHUFFLED flag so that it will not be
				 * moved again.  This prevents starvation
				 * of requests that always appear to be
				 * at the end of long sequential sequences.
				 */
				utab->io_tail = bp;
				bp->av_forw = NULL;
				bp->b_flags |= B_SHUFFLED;
				utab->io_head = nbp;
			}
			/*
			 * Here we've either shuffled the current sequential
			 * stream to the end of the queue or there were
			 * no requests outside the stream in the queue so
			 * it remained the same.  Either way, we reset the
			 * sequential request count to 0.
			 */
			utab->seq_count = 0;
			bp = utab->io_head;
		}
		else
		{
			/*
			 * We haven't reached the limit yet, so just bump
			 * the sequential count.
			 */
			utab->seq_count++;
		}
	}
	else
	{
		/*
		 * The current request is not sequential with the previous
		 * one, so reset the sequential count field.
		 */
		utab->seq_count = 0;
	}

	utab->seq_next = bp->b_sort + BTOBB(bp->b_bcount);

	utab->io_head = bp->av_forw;
	if (bp->av_forw == NULL)
		utab->io_tail = NULL;

	/*
	 * We set io_state.prevblk field in iobuf for macsisort --
	 * macsisort needs to know disk address of last command sent.
	 */
	utab->io_state.prevblk = bp->b_sort;

	/*
	 * We don't know whether we set B_SHUFFLED while the buffer
	 * was in the queue, but always clear it just in case.
	 */
	bp->b_flags &= ~B_SHUFFLED;

	return bp;
}

/*	compute volume header checksum.  */
int
vh_checksum(struct volume_header *vh)
{
	register int csum;
	register int *ip;

	csum = 0;
	for (ip = (int *)vh; ip < (int *)(vh + 1); ip++)
		csum += *ip;
	return (csum);
}

/*
 * check for valid volume header
 */
int
is_vh(register struct volume_header *vhp)
{
	if (vhp->vh_magic != VHMAGIC)
		return (0);

	return vh_checksum(vhp) == 0;
}
