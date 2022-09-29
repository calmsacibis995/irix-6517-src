/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1994, Silicon Graphics, Inc.                  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.29 $"

/*
 * plex_cpy - XLV plex copy support code.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/kmem.h>
#include <sys/immu.h>
#include <sys/time.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/bpqueue.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include "xlv_mem.h"
#include <sys/xlv_lock.h>
#include <sys/xlv_labd.h>
#ifdef DEBUG
#include <sys/cmn_err.h>
#endif
#include "xlv_ioctx.h"
#include "xlv_procs.h"

extern zone_t		*xlv_io_context_zone;
extern bp_queue_t	xlv_labd_request_queue;
extern int		xlvmajor;

STATIC void reissue_requests (buf_t *bp);

#if 0
STATIC void xlv_critical_region_init (xlv_tab_subvol_t *xlv_p);
#endif
STATIC void xlv_critical_region_begin (xlv_tab_subvol_t *xlv_p,
	daddr_t critical_region_start_blkno, daddr_t critical_region_size,
	caddr_t excluded_rqst);
STATIC void xlv_critical_region_next (xlv_tab_subvol_t *xlv_p,
	daddr_t critical_region_size);
STATIC void xlv_critical_region_end (xlv_tab_subvol_t *xlv_p,
	unsigned subvol_index);


/*
 * Mark all the ve's that are part of this plex revive operation.
 */
typedef struct {
	daddr_t		start_blkno;
	daddr_t		end_blkno;
} ve_arg_t;

STATIC int
ve_init_func (
	xlv_tab_vol_elmnt_t     *ve,
        ve_arg_t         	*ve_arg)
{
	/*
	 * Note we exclude active vol elments because there's
	 * no need to change them to active state; they already are.
	 */
        if ((ve->start_block_no >= ve_arg->start_blkno) &&
            (ve->end_block_no <= ve_arg->end_blkno) &&
            (ve->state != XLV_VE_STATE_OFFLINE) &&
            (ve->state != XLV_VE_STATE_INCOMPLETE) &&
	    (ve->state != XLV_VE_STATE_ACTIVE))
                ve->flags |= XLV_VE_REVIVING;

        return 0;       /* continue searching */
}

/*
 * Unmark all the ve's that are part of this revive operation.
 */
STATIC int
ve_undo_func (
        xlv_tab_vol_elmnt_t     *ve,
        ve_arg_t         *ve_arg)
{
        if ((ve->start_block_no >= ve_arg->start_blkno) &&
            (ve->end_block_no <= ve_arg->end_blkno) &&
            (ve->state != XLV_VE_STATE_INCOMPLETE) &&
            (ve->state != XLV_VE_STATE_OFFLINE))
                ve->flags &= ~XLV_VE_REVIVING;

        return 0;       /* continue searching */
}



/*
 * xlv_copy_plexes is called by the xlvioctl() routine in the xlv
 * driver to make all the plexes within a subvolume consistent.
 * It does so by reading down the entire plex. The read/write-back
 * loop will take care of the plex copies.
 *
 * Errors:
 *	ENXIO : if the state of the subvolume has changed during
 *		the plex copy.
 *	EBUSY : if someone else has already initiated a plex copy
 *		on some part of the same subvolume.
 */
int
xlv_copy_plexes (dev_t		dev,
		 daddr_t	start_blkno,
		 daddr_t	end_blkno,
		 unsigned	chunksize,	/* in blocks */
		 unsigned	sleep_intvl_msec)
{
	xlv_tab_subvol_t 	*xlv_p;
	uio_t			uio;
	iovec_t			iov;
	daddr_t			blkno;
	unsigned		subvol_index, p;
	int			error;
        xlv_tab_vol_elmnt_t	*vol_elmnt;
	buf_t			*bp;
	xlv_io_context_t	*io_context;
	ve_arg_t         	ve_arg;
	caddr_t			raw_buf;
	unsigned		page_cnt;
	xlv_ves_active_rqst_t	*rqst_ptr[XLV_MAX_PLEXES] = {0};
	xlv_io_context_t	*rqst_ioctx[XLV_MAX_PLEXES];
	buf_t			*rqst_buf[XLV_MAX_PLEXES];

	bzero (&uio, sizeof(uio_t));
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
        uio.uio_segflg = UIO_SYSSPACE;

        /*
         * Allocate uncached pages. We will be dma into these pages
         * and then dma out. There's no reason for them to be cached.
         * If they were cached, we'd also need to invalidate the
         * cache on the write-back.  Note: we need to do the allocation
	 * before locking the subvol, as the subvol may be the swap
	 * device, in which case the kvpalloc may deadlock.
         */
        page_cnt = io_btoc(chunksize << BBSHIFT);
        raw_buf = kvpalloc (page_cnt, VM_UNCACHED, KM_SLEEP);
	if (raw_buf == 0) {
		return ENOMEM;
	}

	subvol_index = minor (dev);

	xlv_acquire_cfg_lock(subvol_index);
	/*
	 * Note: at this point, xlv_io_lock[subvol_index].xlvd_lock_count
	 * could be non-zero since the xlvd could have dropped the
	 * io_lock temporarily in order to get it back in update mode.
	 */

	if (! xlv_tab) {
		xlv_release_cfg_lock(subvol_index);
                error = ENXIO;
                goto free_mem_done;
        }

	xlv_p = &xlv_tab->subvolume[subvol_index];
	if ( !XLV_SUBVOL_EXISTS(xlv_p) || (xlv_p->num_plexes <= 1) ) {
		xlv_release_cfg_lock(subvol_index);
		error = 0;
		goto free_mem_done;
	}
	
	if (xlv_p->flags & XLV_SUBVOL_PLEX_CPY_IN_PROGRESS) {
		xlv_release_cfg_lock(subvol_index);
		error = EBUSY;
		goto free_mem_done;
	}

	/*
	 * Indicate that a plex copy is in progress. The user mode
	 * process driving this should ensure that only one plex
	 * copy is in progress.
	 */
	xlv_p->flags |= XLV_SUBVOL_PLEX_CPY_IN_PROGRESS;

	/*
	 * Set up the read/write-back region to include those
	 * volume elements that are stale or empty.
	 * Note that this is a redundant operation in the case of
	 * a plex revive that immediately followed the first
	 * xlv_tab_set(). But we need it for subsequent plex
	 * revives that were issued in response to configuration
	 * changes.
	 */
	xlv_setup_plex_rd_wr_back_region (xlv_p);

	if (xlv_p->rd_wr_back_start == xlv_p->subvol_size) {
		/*
		 * Nothing needs to be revived.
		 */
		xlv_p->flags &= ~XLV_SUBVOL_PLEX_CPY_IN_PROGRESS;
		xlv_release_cfg_lock(subvol_index);
                error = 0;
                goto free_mem_done;
	}



	/*
	 * XXX NOTE: For now, we always revive whatever is in the
	 * critical region, regardless of what the caller wants.
	 */
	start_blkno = xlv_p->rd_wr_back_start;
	end_blkno = xlv_p->rd_wr_back_end;

	ASSERT (end_blkno < xlv_p->subvol_size);

	/*
	 * Mark all the volume elements that are being revived.
	 * This way, we know which ones to bring online after the
	 * plex revive has completed.
	 * If this were not done, you can have:
	 *   1) start reviving blocks 0 to 1000 on ve's 1 and 2.
	 *   2) we've revived blocks 0 to 500 and are still reviving.
	 *   3) someone puts a plex containing ve 3 at block 0.
	 *   4) we finish reviving blocks 501 to 1000. 
	 *   5) now we mark all volume elements between 0 and 1000
	 *      online when ve 3 is still not revived.
	 */
	ve_arg.start_blkno = start_blkno;
        ve_arg.end_blkno = end_blkno;
        xlv_for_each_ve (xlv_p, (XLV_VE_FP)ve_init_func, &ve_arg);

	/*
	 * Note that we call critical_region_begin after getting the
	 * buffer address so that this request won't be queued on
	 * the critical region.
	 */
	xlv_critical_region_begin (xlv_p, start_blkno, chunksize, raw_buf);

	xlv_release_cfg_lock(subvol_index);

	for (blkno = start_blkno; blkno <= end_blkno; blkno += chunksize) {

		iov.iov_base = raw_buf;
		iov.iov_len = chunksize << BBSHIFT;

		/*
		 * If we're going beyond the end of the region to be 
		 * copied, adjust the transfer size.
		 */
		if ((blkno + chunksize) > end_blkno)
			chunksize = end_blkno - blkno + 1;
		iov.iov_len = chunksize << BBSHIFT;

		/*
		 * Read data from that region into the buffer. Since the
		 * plexes are in read/write-back mode, this will cause
		 * all the plexes to be updated with the same data.
		 */
		error = biophysio (xlvstrategy, NULL, dev, B_READ, blkno, &uio);

		/*
                 * Move the read/write-back region ahead.
                 */
		xlv_acquire_cfg_lock(subvol_index);

		if (error) {
                        xlv_critical_region_end (xlv_p, subvol_index);
                        goto free_mem_done;
                }

		if (! xlv_tab) {
			error = ENXIO;
			xlv_release_cfg_lock(subvol_index);
			goto free_mem_done;
		}
		xlv_p = &xlv_tab->subvolume[subvol_index];
                if (!XLV_SUBVOL_EXISTS(xlv_p)) {
			error = ENXIO;
			xlv_critical_region_end (xlv_p, subvol_index);
			goto free_mem_done;
		}

		/*
		 * If we are down to a single plex because of a dynamic
		 * change operation, stop now.
		 */
		if (xlv_p->num_plexes == 1) {
			xlv_p->rd_wr_back_start = xlv_p->rd_wr_back_end =
				xlv_p->subvol_size;
			xlv_release_cfg_lock(subvol_index);
			break;
		}

		xlv_p->rd_wr_back_start += chunksize;
		if (xlv_p->rd_wr_back_start >= xlv_p->rd_wr_back_end) {
			/*
			 * We've reached the end of the read/write-back
			 * region. Done.
			 *
			 * NOTE that we assume that there is only a
			 * single read/write-back region per subvolume.
			 */
			xlv_p->rd_wr_back_start = xlv_p->rd_wr_back_end =
				xlv_p->subvol_size;
		}

		/*
		 * Delay some configurable amount of time before 
		 * issuing the next I/O so we don't chew up all the
		 * available disk bandwidth.
		 */
		if (sleep_intvl_msec) {
			xlv_critical_region_end (xlv_p, subvol_index);
			/* XLV_IO_UNLOCK(subvol_index) 
			   done by xlv_critical_region_end */

			delay (sleep_intvl_msec);

			xlv_acquire_cfg_lock(subvol_index);

			if (! xlv_tab) {
				error = ENXIO;
				xlv_release_cfg_lock(subvol_index);
				goto free_mem_done;
			}

			xlv_p = &xlv_tab->subvolume[subvol_index];
			if (!XLV_SUBVOL_EXISTS(xlv_p)) {
				error = ENXIO;
				xlv_release_cfg_lock(subvol_index);
				goto free_mem_done;
			}

			xlv_critical_region_begin (xlv_p, blkno,
				chunksize, raw_buf);
			xlv_release_cfg_lock(subvol_index);
		}
		else {	
			xlv_critical_region_next (xlv_p, chunksize);
			/* XLV_IO_UNLOCK(subvol_index) 
			   done by xlv_critical_region_next */
		}
	}

	/* preallocate memory for rqsts *before* getting the IO lock */
	for (p = 0; p < XLV_MAX_PLEXES; p++) {
		rqst_ptr[p] = kmem_zalloc(sizeof(xlv_ves_active_rqst_t),
					 KM_SLEEP);
		rqst_ioctx[p] = kmem_zalloc(sizeof(xlv_io_context_t), KM_SLEEP);
		rqst_buf[p] = kmem_zalloc(sizeof(buf_t), KM_SLEEP);
	}

	xlv_acquire_cfg_lock(subvol_index);

	if (! xlv_p) {
		error = ENXIO;
		xlv_release_cfg_lock(subvol_index);
		goto free_mem_done;
	}

	xlv_p = &xlv_tab->subvolume[subvol_index];
	if (!XLV_SUBVOL_EXISTS(xlv_p)) {
		error = ENXIO;
		xlv_release_cfg_lock(subvol_index);
		goto free_mem_done;
	}

	/*
	 * Note down the plexes that are being revived so that
	 * xlv_labd can mark them online.
	 *
	 * XXX WH Note that there's a hole here. We can start a plex
	 * plex revive with a partial plex. During the revive, we
	 * add a vol elmnt to that plex. The current interface would
	 * mark that vol elmnt online even though it may not have
	 * been revived yet. We'll fix this by issuing multiple
	 * online commands.
	 */
	for (p = 0; p < XLV_MAX_PLEXES; p++) {

		xlv_tab_plex_t		*plex;

		if (plex = xlv_p->plex[p]) {
			unsigned		v;
			xlv_ves_active_rqst_t	*rqst;

			rqst = NULL;

			/*
			 * If any volume element in this plex is
			 * being revived and hasn't changed state, then 
			 * add this plex to the revived set.
			 */
			for (v = 0; v < plex->num_vol_elmnts; v++) {

				vol_elmnt = &plex->vol_elmnts[v];
				if ((vol_elmnt->flags & XLV_VE_REVIVING) &&
				   vol_elmnt->state != XLV_VE_STATE_OFFLINE &&
				   vol_elmnt->state != XLV_VE_STATE_INCOMPLETE){

					ASSERT (vol_elmnt->start_block_no >= 
						start_blkno);
					ASSERT (vol_elmnt->end_block_no <=
						end_blkno);

					vol_elmnt->flags &= ~XLV_VE_REVIVING;
					vol_elmnt->state = XLV_VE_STATE_ACTIVE;

					if (rqst == NULL) {
						rqst = rqst_ptr[p];
						rqst_ptr[p] = NULL;
						rqst->num_vol_elmnts = 0;
						rqst->operation = 
						    XLV_LABD_RQST_VES_ACTIVE;
						rqst->plex_uuid = plex->uuid;
					}
					rqst->vol_elmnt_uuid
						[rqst->num_vol_elmnts] =
						vol_elmnt->uuid;
					rqst->num_vol_elmnts++;
				}
			}

			/*
			 * If necessary, notify xlv_labd that the state
			 * has changed.
			 * Note that xlv_labd will free the storage for
			 * the buf, io_context, and the request itself.
			 */
			if (rqst) {
				io_context = rqst_ioctx[p];
				bp = rqst_buf[p];
                                io_context->original_buf = bp;
                                io_context->state = XLV_SM_NOTIFY_VES_ACTIVE;
				io_context->buffers = (xlvmem_t *) rqst;
				bp->b_sort = (__psunsigned_t) io_context;
				bp->b_edev = makedev(xlvmajor, 0);
#ifdef DEBUG
				/* for the tracing code */
				bp->b_dmaaddr = NULL;
				cmn_err (CE_DEBUG,
            "xlv_copy_plexes: QUEUED online request");
#endif
				bp_enqueue (bp, &xlv_labd_request_queue);
			}
		}
	}


	/*
	 * Now recompute the block map based upon the new volume
	 * element state.
	 */
	(void) xlv_recalculate_blockmap (xlv_p, NULL, NULL);

	/*
	 * We end the critical region here so that if there are any
	 * queued I/O, they would be reissued using the new, presumably
	 * more efficient configuration.
	 */
	xlv_critical_region_end (xlv_p, subvol_index);
	/* XLV_IO_UNLOCK done by xlv_critical_region_end */

	error = 0;


free_mem_done:

	/* free unused memory for rqsts */
	for (p = 0; p < XLV_MAX_PLEXES; p++) {
		if (rqst_ptr[p]) {
			kmem_free(rqst_ptr[p], 0);
			kmem_free(rqst_ioctx[p], 0);
			kmem_free(rqst_buf[p], 0);
		}
	}

	if ( error && XLV_SUBVOL_EXISTS(xlv_p) )
		xlv_for_each_ve (xlv_p, (XLV_VE_FP)ve_undo_func, &ve_arg);

	xlv_p->flags &= ~XLV_SUBVOL_PLEX_CPY_IN_PROGRESS;
	kvpfree (raw_buf, page_cnt);

	return error;
}



/*
 * xlv_cr - plex revive critical region support.
 *
 * The critical region is the portion of a plex's block range that is
 * actively been sync'ed. All requests made to a block within the 
 * critical region are queued and processed after the plex synch
 * has completed.
 */


/*
 * Initialize a critical region. Called when a new xlv_tab entry is
 * created. We don't acquire any locks since the entry is, by
 * definition, not actively being used.
 */
#if 0
STATIC void
xlv_critical_region_init (
	xlv_tab_subvol_t        *xlv_p)
{
        xlv_p->critical_region.pending_requests = NULL;

        xlv_p->critical_region.start_blkno = 0;
	xlv_p->critical_region.end_blkno = 0;

	xlv_p->critical_region.excluded_rqst = NULL;
}
#endif


/*
 * Create an active critical region on a subvolume.
 */
STATIC void
xlv_critical_region_begin (
	xlv_tab_subvol_t        *xlv_p,
	daddr_t			critical_region_start_blkno,
        daddr_t                 critical_region_size,
	caddr_t			excluded_rqst)
{
        daddr_t                 end_blkno;

	ASSERT(XLV_CFG_LOCKED(xlv_p - xlv_tab->subvolume));

	/*
	 * Critical region must be inactive (i.e., starts after the
	 * end of the subvolume.
	 */
	ASSERT (xlv_p->critical_region.end_blkno == 0);
	ASSERT (! xlv_p->critical_region.pending_requests);
	ASSERT (! xlv_p->critical_region.excluded_rqst);

        xlv_p->critical_region.start_blkno = critical_region_start_blkno;
	end_blkno = critical_region_start_blkno + critical_region_size - 1;
        ASSERT (xlv_p->subvol_size);
        if (end_blkno >= xlv_p->subvol_size)
                end_blkno = xlv_p->subvol_size - 1;
        xlv_p->critical_region.end_blkno = end_blkno;

	xlv_p->critical_region.excluded_rqst = excluded_rqst;

}



/*
 * Move the critical region down the subvolume. Process any requests
 * that were blocked on the last critical region area.
 */
STATIC void
xlv_critical_region_next (
	xlv_tab_subvol_t        *xlv_p,
	daddr_t			critical_region_size)
{
	daddr_t			end_blkno;
	buf_t			*q_head;

        unsigned                subvol_index;

        subvol_index = xlv_p - xlv_tab->subvolume;

        ASSERT(XLV_CFG_LOCKED(subvol_index));

	ASSERT (xlv_p->critical_region.end_blkno);

	/*
	 * Save off requests that were pending on blocks within
	 * this critical region.
	 */
	q_head = xlv_p->critical_region.pending_requests;
	xlv_p->critical_region.pending_requests = NULL;

	/*
	 * Advance the critical region by critical_region_size.
	 */
	xlv_p->critical_region.start_blkno = xlv_p->critical_region.
			end_blkno + 1;

	end_blkno = xlv_p->critical_region.start_blkno + 
			critical_region_size - 1;
	ASSERT (xlv_p->subvol_size);
	if (end_blkno >= xlv_p->subvol_size)
		end_blkno = xlv_p->subvol_size - 1;
	xlv_p->critical_region.end_blkno = end_blkno;

	/*
	 * Release the io_lock now so that we won't block our
	 * reissues.
	 */
	xlv_release_cfg_lock(subvol_index);

	/*
	 * Reissue all the pending requests.
	 *
	 * Note that we issue them to the upper xlv strategy routine
	 * because we need to reacquire the xlv_io_lock.
	 */

        if (q_head) {

                /*
                 * Make the circular list singly linked.
                 */
		if (q_head->av_back)
			q_head->av_back->av_forw = NULL;

                reissue_requests (q_head);
        }
}


/*
 * Deactive the last critical region on this subvolume.
 * This will process any queued requests and then then reinitialize
 * the critical region.
 */
STATIC void
xlv_critical_region_end (
	xlv_tab_subvol_t        *xlv_p,
	unsigned		subvol_index)
{
        buf_t                   *q_head;

        ASSERT(XLV_CFG_LOCKED(subvol_index));

	if ((xlv_tab == NULL) || !XLV_SUBVOL_EXISTS(xlv_p)) {
		xlv_release_cfg_lock(subvol_index);
                return;
	}


        ASSERT (xlv_p->critical_region.end_blkno);

        /*
         * Save off requests that were pending on blocks within
         * this critical region.
         */
        q_head = xlv_p->critical_region.pending_requests;

	xlv_p->critical_region.pending_requests = NULL;
	xlv_p->critical_region.start_blkno = 0;
	xlv_p->critical_region.end_blkno = 0;
	xlv_p->critical_region.excluded_rqst = NULL;

	xlv_release_cfg_lock(subvol_index);

	if (q_head) {

                /*
                 * Make the circular list singly linked.
                 */
                q_head->av_back->av_forw = NULL;

		reissue_requests (q_head);
	}
}


/*
 * Reissues all the requests that were pending because their block
 * range intersected the critical region.
 */
STATIC void
reissue_requests (
	buf_t		*bp)
{
	buf_t		*first, *next;

        /*
         * Reissue all the pending requests.
         *
         * Note that we issue them to the upper xlv strategy routine
         * because we need to reacquire the xlv_io_lock.
         */

	first = bp;
	while (bp) {
		next = bp->av_forw;
		xlvstrategy (bp);
		bp = next;
		if (bp == first)
			break;
        }
}

