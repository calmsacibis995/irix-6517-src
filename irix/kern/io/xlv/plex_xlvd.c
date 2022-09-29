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
#ident "$Revision: 1.61 $"

/*
 * plex_xlvd - XLV kernel process.
 *
 * This daemon performs multi-step operations such as error retries and
 * read/write-backs during plex recovery.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/kmem.h>
#include <sys/var.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include "xlv_mem.h"
#include <sys/xlv_lock.h>
#include <sys/bpqueue.h>
#include <sys/cmn_err.h>
#include <sys/scsi.h>
#include <sys/major.h>
#include <sys/open.h>
#include <sys/conf.h>
#include <sys/schedctl.h>
#include <ksys/sthread.h>
#include <sys/hwgraph.h>
#include <sys/failover.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"

/*
 * The xlvd request queue.
 */
bp_queue_t	xlvd_work_queue;

/*
 * The xlv_labd request queue.
 */
boolean_t	xlv_labd_queue_initialized = B_FALSE;
bp_queue_t      xlv_labd_request_queue;

/*
 * Global that indicates whether plexing is supported.
 */
int xlv_plexing_support_present = 1;

extern int xlvmajor;

static lock_t			 fo_deferred_lock;
static buf_t			*fo_deferred = NULL;
extern void griostrategy(buf_t *bp);  /* prototype -- where to find it? */

STATIC void xlvd_process_request (buf_t *bp);

STATIC void xlvd_process_read_error (
        buf_t                   *bp,
        xlv_io_context_t        *io_context);

STATIC void xlvd_process_write_error (
        buf_t                   *bp,
        xlv_io_context_t        *io_context);

STATIC void xlvd_do_write_back (
        buf_t                   *bp,
        xlv_io_context_t        *io_context);

STATIC void xlvd_do_rewrite (
        buf_t                   *bp,
        xlv_io_context_t        *io_context);

STATIC void xlvd_take_disks_offline (
        xlv_io_context_t        *io_context);

STATIC void xlvd_complete_orig_request (
        xlv_io_context_t        *io_context,
	boolean_t		io_lock_in_update);

STATIC void xlvd_get_failed_blocks (
        xlv_tab_subvol_t        *xlv_p,
        xlv_io_context_t        *io_context,
        daddr_t                 *first_failed_block,
        daddr_t                 *last_failed_block);

STATIC boolean_t xlvd_get_retry_slices (
        xlv_io_context_t        *io_context,
        daddr_t                 first_failed_block,
        daddr_t                 last_failed_block,
        xlv_slice_t             retry_slices[],
	xlv_tab_subvol_t        *xlv_p);

#if 0
STATIC int can_satisfy_request_from_slice (
	xlv_block_map_t		*block_map,
	buf_t			*bp,
	unsigned		slice);
#endif

STATIC void xlvd_log_event (
	int			level,	/* CE_{DEBUG,CONT,NOTE,WARN,ALERT} */
        char                    *fmt_string,
        xlv_io_context_t        *io_context);

const int		  xlvd_n_classes = 2;

static void
xlvq(void)
{
	bpqueue_init(&xlvd_work_queue);
	bpqueuedaemon (&xlvd_work_queue, xlvd_process_request);
}

/*
 * This is called from main() to start the xlv daemons.
 * For now, we start xlvd and initialize the work queues for xlvd
 * and xlv_labd. 
 */
void
xlv_start_daemons(void)
{
	extern int xlvd_pri;

	bpqueue_init (&xlv_labd_request_queue);
	xlv_labd_queue_initialized = B_TRUE;

	spinlock_init(&fo_deferred_lock, "fo_deferred_lock");

	sthread_create("xlvd", 0, 0, 0, xlvd_pri, KT_PS,
			(st_func_t *)xlvq, 0, 0, 0, 0);
}


/*
 * get deferred bp's for this dp.
 * Also, set active_path to retry_path, signalling that this dp is
 * no longer undergoing failover.
 *
 * Note: it's important to do both these actions atomically (done here
 * under the aegis of fo_deferred_lock).
 */
/* XXX KK: Can we defer based on disk CTLR-TARG, rather than dp? */
buf_t *
fo_get_deferred(xlv_tab_disk_part_t *dp)
{
    buf_t		 *dbp, *prev_dbp, *dp_def = NULL, *next_dbp;
    register		  int s;

    FO_DEBUG(printf("get_def (%x): act: %d, ret: %d; deferred = %x.\n", dp,
		    dp->active_path, dp->retry_path, fo_deferred););

    s = mutex_spinlock(&fo_deferred_lock);
    /* find all deferred bps belonging to this dp */
    for (dbp = fo_deferred, prev_dbp = NULL;
	 dbp;
	 dbp = next_dbp) {
	/* remember next */
	next_dbp = FO_NEXT_DBP(dbp);

	if (FO_DISK_PART(dbp) == dp) {
	    /* found a deferred bp for this dp: remove it from deferred list */
	    if (prev_dbp) {
		FO_NEXT_DBP_L(prev_dbp) = FO_NEXT_DBP(dbp);
	    }
	    else {
		fo_deferred = FO_NEXT_DBP(dbp);
	    }

	    /* add it to our list */
	    FO_NEXT_DBP_L(dbp) = dp_def;
	    dp_def = dbp;
	}
	else {
	    prev_dbp = dbp;
	}
    }

    /* reset failover state of dp (should always be 0) */
    ASSERT(dp->active_path == 0);
    dp->retry_path = dp->active_path;
    mutex_spinunlock(&fo_deferred_lock, s);

    FO_DEBUG(printf("get_def (%x): deferred bps: %x.\n", dp, dp_def););

    return dp_def;
}


/*
 * Check if it's meaningful to queue a request for failover.
 * If not, return 0 (cleanup, if we had been trying to failover).
 * If so, queue request for failover; return 1.
 *
 * The bp is the same one that was (and will again be) headed for the
 * disk; hanging off bp->b_fsprivate is the info (xlv_tab_disk_part_t)
 * needed to failover and retry the request.
 * bp->b_flags's XLVD_FAILOVER bit is non-zero and bp's FO_STATE is not
 * FO_NORMAL from when it's successfully queued till failover is over.
 */
int
xlvd_queue_failover(buf_t *bp)
{
    register xlv_tab_disk_part_t *dp = FO_DISK_PART(bp);
    register int		  s;
#ifdef FO_STATS
    extern int n_fos, n_defs;
    extern time_t lbolt, fl_min, fl_max, fl_tot;
#endif /* FO_STATS */

    /* if there's only one path to disk, can't fail over! */
    if (fo_scsi_device_pathcount(dp->dev[dp->active_path]) <= 1) {
	return 0;
    }

    if (FO_STATE(bp) == FO_NORMAL) {
#ifdef FO_STATS
	s = lbolt - bp->b_start;
	if (s < fl_min) fl_min = s;
	if (s > fl_max) fl_max = s;
	fl_tot += s;
	bp->b_start = lbolt;
#endif /* FO_STATS */

	FO_DEBUG(printf("queue_fo(%x): new bp.\n", bp););
	s = mutex_spinlock(&fo_deferred_lock);
	if (dp->active_path == dp->retry_path) {
	    if (bp->b_edev != dp->dev[dp->active_path]) {
		/* bp used stale active_path -- treat as if deferred */
#ifdef FO_STATS
		n_defs++;
#endif /* FO_STATS */
		mutex_spinunlock(&fo_deferred_lock, s);
		FO_DEBUG(printf("queue_fo(%x): stale dev = %x -> %x.\n", bp,
				bp->b_edev, dp->dev[dp->active_path]););
		FO_NEXT_DBP_L(bp) = NULL;
		FO_SET_STATE(bp, FO_ISSUE_DEFERRED);
		XLVD_SET_FAILING_OVER(bp);
		bp_enqueue(bp, &xlvd_work_queue);

		return 1;
	    }

#ifdef FO_STATS
	    n_fos++;
#endif /* FO_STATS */

	    ASSERT(dp->active_path == 0); 
	    /* FO_NORMAL buffer */
	    /* force retry path to be different than active path */
	    /* previously this would start the path switch */
	    dp->retry_path = 1;

	    mutex_spinunlock(&fo_deferred_lock, s);

	    printf("XLV: i/o error encountered, attempting failover from device %d (%s)\n", minor(bp->b_edev), devtopath(bp->b_edev));

	    FO_SET_STATE(bp, FO_SWITCH);
	    XLVD_SET_FAILING_OVER(bp);
	    bp_enqueue(bp, &xlvd_work_queue);

	    return 1;
	}
	    
	/* failover already in progress on another bp: defer */
#ifdef FO_STATS
	n_defs++;
#endif /* FO_STATS */
	/* place onto head of deferred queue */
	FO_NEXT_DBP_L(bp) = fo_deferred;
	fo_deferred = bp;
	mutex_spinunlock(&fo_deferred_lock, s);

	FO_DEBUG(printf("queue_fo(%x): deferred!\n", bp););
	return 1;
    }

    /* if failover failed, give up */
    if (FO_STATE(bp) == FO_ABORT) {
	printf("XLV: could not failover on device %d (%s)\n", minor(bp->b_edev), devtopath(bp->b_edev));
	FO_SET_STATE(bp, FO_NORMAL);
	XLVD_UNSET_FAILING_OVER(bp);

	return 0;
    }

    /* continue failover -- queue up bp */
    bp_enqueue(bp, &xlvd_work_queue);
    FO_DEBUG(printf("queue_fo(%x): successfully queued.\n", bp););

    return 1;
}


/*
 * This is the "generic" routine to process failovers.
 *
 * This is organized as a state machine.  Each state is labelled with
 * the next task to perform.  A switch statement takes you to the
 * action appropriate to the current state.  In order to make
 * "internal" state transitions (i.e., without going through
 * bp_enqueue()), the switch statement is enclosed in an infinite
 * loop.  A "break" within the switch takes you to the next state; a
 * "return" is the only way out.
 *
 * The device-specific failover routines are encapsulated in the
 * macros INITIATE_SWITCH and FINISH_SWITCH.
 */
#define OPEN_RETRIES 5 /* number of times to retry bdopen */
STATIC void
xlvd_process_failover(buf_t *bp)
{
    register xlv_tab_disk_part_t *dp = FO_DISK_PART(bp);
    dev_t		  dev;
    dev_t                 new_dev;
    int			  err;
    char                  *etext;
#ifdef FO_STATS
    extern int n_opens, n_open_retries, n_closes, n_reissues, n_fails, n_aborts;
    extern time_t lbolt, fo_min, fo_max, fo_tot;
#endif /* FO_STATS */
    struct bdevsw	  *my_bdevsw;

    FO_DEBUG(printf("proc_fo (%x): state: %d, act: %d, ret: %d.\n", bp,
		    FO_STATE(bp), dp->active_path, dp->retry_path););

    do {
	switch (FO_STATE(bp)) {
	    buf_t	 *next;

	  case FO_TRY_NEXT:

	    /* if no more paths to try, failover failed; else try next path */
	    dp->retry_path = 1; 

	    FO_SET_STATE(bp, FO_SWITCH);
	    /* FALL THROUGH */

	  case FO_SWITCH:

	    FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): calling fo_scsi_device_switch\n"););
		    
	    if ((err = fo_scsi_device_switch(dp->dev[dp->active_path],
				     &new_dev)) != FO_SWITCH_SUCCESS) {

		/* switch failed, print out error */
	        switch (err) {
		case FO_SWITCH_FAIL:
			etext = "FO_SWITCH_FAIL";
			break;
		case FO_SWITCH_INVALID_PATH:
			etext = "FO_SWITCH_INVALID_PATH";
			break;
		case FO_SWITCH_NOPATH:
			etext = "FO_SWITCH_NOPATH";
			break;
		case FO_SWITCH_ONEPATHONLY:
			etext = "FO_SWITCH_ONEPATHONLY";
			break;
		case FO_SWITCH_PATHS_EXHAUSTED:
			etext = "FO_SWITCH_PATHS_EXHAUSTED";
			break;
		default:
			etext = "UNKNOWN FO ERROR";
			break;
		}
		cmn_err_tag(137,CE_WARN, "XLV: xlvd_process_failover: failover unsuccessful. "
			"fo_scsi_device_switch returned %s for dev: %d\n",
			etext, dp->dev[dp->active_path]);

		FO_SET_STATE(bp, FO_ABORT_ALL);
		break;
	    }
	    FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): failover successful\n"););

	    dp->dev[dp->active_path] = new_dev;

	    FO_SET_STATE(bp, FO_REISSUE);
	    /* fallthrough */

	  case FO_REISSUE:

	    /* open new path, close old one; reissue i/o request.
	     * New state is FO_TRY_NEXT: since that's what should
	     * happen if reissue fails (if it succeeds, we're done).
	     */

	    /* XXXsbarr: this assumes the new failover code doesn't delete
             * the old hwgraph node after failover is complete.  If that
             * policy ever chages we'll have to close first then open the
             * new device.
	     */
	    FO_SET_STATE(bp, FO_TRY_NEXT);

	    /* new active path received from successful failover */
	    dev = dp->dev[dp->active_path];
	    my_bdevsw = get_bdevsw(dev);
	    ASSERT(my_bdevsw != NULL);

	    if (!(err = bdhold(my_bdevsw))) {
		int	  tries = 0;

		for (tries = 0;
		     (err = bdopen(my_bdevsw, &dev, 0, OTYP_LYR, NULL)) != 0
		     && tries < OPEN_RETRIES;
		     tries++) {
		    delay(tries*HZ + HZ);
#ifdef FO_STATS
		    n_open_retries += 1 << (tries*6);
#endif /* FO_STATS */
		}

		FO_DEBUG(if (err)
			  printf("proc_fo (%x): bdopen %x returns %d (dp: %x).\n",
				 bp, dev, err, dp););
#ifdef FO_STATS
		if (!err) n_opens++;
#endif /* FO_STATS */

		bdrele(my_bdevsw);
	    }

	    if (err) {
		FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): failover for bp (%x): failed to open %x (%d)!\n", bp, dev, err););
		break; /* go on to state TRY_NEXT */
	    }

	    FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): failover device open succeeded.  dev: %d\n", dev););

	    my_bdevsw = get_bdevsw(bp->b_edev);
	    ASSERT(my_bdevsw != NULL);
	    err = bdclose(my_bdevsw, bp->b_edev, 0, OTYP_LYR, NULL);
	    if (err) {
		    FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): old device close failed.  dev: %d  error: %d\n", bp->b_edev, err););
	    } else {
		    FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): old device close succeeded. dev: %d  error: %d\n", bp->b_edev, err););
	    }

	    FO_SET_STATE(bp, FO_ISSUE_DEFERRED);
	    /* signal that failover is complete */
	    ASSERT(dp->active_path == 0); 
	    dp->retry_path = dp->active_path;
	    
#ifdef FO_STATS
	    if (!err) n_closes++;
	    bp->b_start = lbolt - bp->b_start;
	    if (bp->b_start < fo_min) fo_min = bp->b_start;
	    if (bp->b_start > fo_max) fo_max = bp->b_start;
	    fo_tot += bp->b_start;
#endif /* FO_STATS */

	    bp->b_edev = dev;
	    bp->b_flags &= ~B_ERROR;
#if defined(SIM) || defined(USER_MODE)
	    my_bdevsw = get_bdevsw(dev);
	    ASSERT(my_bdevsw != NULL);
	    bdstrat(my_bdevsw, bp);
#else
	    griostrategy(bp);
#endif
	    return;

	  case FO_ISSUE_DEFERRED:

	    /* only the first of the deferred bps will have failover state */
	    FO_SET_STATE(bp, FO_NORMAL);
	    XLVD_UNSET_FAILING_OVER(bp);
	    for (; bp; bp = next) {
#ifdef FO_STATS
		n_reissues++;
#endif /* FO_STATS */
		/* issue bp with b_edev = active dev */
		next = FO_NEXT_DBP(bp);
		bp->b_edev = dp->dev[dp->active_path];
		FO_DEBUG(printf("proc_fo (%x): issuing (next: %x).\n", bp, next););
		bp->b_flags &= ~B_ERROR;
#if defined(SIM) || defined(USER_MODE)
	        my_bdevsw = get_bdevsw(dev);
	        ASSERT(my_bdevsw != NULL);
	        bdstrat(my_bdevsw, bp);
#else
		griostrategy(bp);
#endif
	    }
	    FO_DEBUG(printf("proc_fo (%x): issued all deferred.\n", bp););

	    return;

	  case FO_ABORT_ALL:
	    /* couldn't failover -- clean up: add deferred bps and abort all */
	    FO_NEXT_DBP_L(bp) = fo_get_deferred(dp);
#ifdef FO_STATS
	    n_fails++;
	    n_aborts--;
#endif /* FO_STATS */
	    for (; bp; bp = next) {
#ifdef FO_STATS
		n_aborts++;
#endif /* FO_STATS */
		next = FO_NEXT_DBP(bp);
		FO_DEBUG(printf("proc_fo (%x): iodone (dp: %x; next: %x).\n",
				 bp, FO_DISK_PART(bp), next););
		FO_SET_STATE(bp, FO_ABORT);
		iodone(bp);
	    }
	    FO_DEBUG(printf("proc_fo (%x): iodoned all deferred.\n", bp););

	    return;

	  default:
	    /* should never happen: abort failover */
	    ASSERT(0);
	    FO_SET_STATE(bp, FO_ABORT);
	    iodone(bp);

	    return;
	}
    } while (1);
}


/*
 * This routine is called by bpqueuedaemon() at PZERO to process
 * a single queued request.
 */
STATIC void
xlvd_process_request (buf_t *original_bp)
{
	xlv_io_context_t	*io_context;
	buf_t			*retry_bp;
	unsigned		subvol_index;
	static boolean_t	initialized = B_FALSE;

	xlv_trace ("XLVD_PROCESS_RQST", XLV_TRACE_USER_BUF, original_bp);

	if (! initialized) {
		initialized = B_TRUE;
	}

	if (XLVD_FAILING_OVER(original_bp)) {
		/* this is not mine -- hand it over to xlvd_process_failover */
		xlvd_process_failover(original_bp);
		return;
	}

	subvol_index = minor(original_bp->b_edev);
	ASSERT (emajor(original_bp->b_edev) == xlvmajor);

	if ((original_bp->b_flags & B_XLV_IOC)) {
		original_bp->b_flags &= ~B_XLV_HBUF;
		xlvstrategy(original_bp);
		return;
	}

	/*
	 * The bp is always the original request that was issued to
	 * xlv. In the case of retries, the io_context will point to
	 * the buffer used for the retry.
	 */
	ASSERT ((original_bp->b_flags & B_XLV_HBUF) && 
		(~(original_bp->b_flags & B_XLVD_BUF)));

	io_context = (xlv_io_context_t *) original_bp->b_sort;
	retry_bp = &io_context->retry_buf;

#ifdef DEBUG
	if (io_context->state != XLV_SM_UNBLOCK_DRIVER)
		ASSERT(XLV_IO_LOCKED_OR_RECOVERING(subvol_index));
#endif

	switch (io_context->state) {

		case XLV_SM_READ:

			/*
			 * If we're in the READ state, then we could
			 * not have initiated any retries.
			 */
			ASSERT (retry_bp->b_edev == 0);

			ASSERT (original_bp->b_flags & B_ERROR);

			/*
			 * Got a read error, attempt recovery.
			 * next state can be:
			 *	XLV_SM_REREAD, XLV_SM_END, or
			 *      XLV_SM_WAIT_FOR_OFFLINE.
			 */
			if (xlv_block_io(subvol_index, original_bp) == 0) {
				/* must wait for current ios to drain */
				return;
			}

			xlvd_process_read_error (original_bp, io_context);

			break;


		case XLV_SM_REREAD:

			/*
			 * xlvd does not use the original bp for
			 * retries; it allocates its own.
			 */
			ASSERT (retry_bp->b_edev != 0
				&& (retry_bp->b_flags & B_XLVD_BUF));

			if (retry_bp->b_flags & B_ERROR) {
				/*
				 * The reread also encountered an error.
				 * Handle this just like a read error.
				 */
				xlvd_process_read_error (retry_bp, io_context);
			}
			else {
				/*
				 * The re-read was successful so clear
				 * the error flag from the original buffer
				 * and try to update the problem plex.
				 */
				xlvd_log_event ( CE_WARN,
				    "xlv: masked read failure on %s",
				    io_context);
				original_bp->b_error = 0;
				original_bp->b_flags &= ~B_ERROR;
				io_context->state = XLV_SM_REWRITE;
				xlvd_do_rewrite (retry_bp, io_context);
			}
			break;


		case XLV_SM_REWRITE:

			/*
                         * xlvd does not use the original bp for
                         * retries; it allocates its own.
                         */
			ASSERT (retry_bp->b_edev != 0
				&& (retry_bp->b_flags & B_XLVD_BUF));

			if (retry_bp->b_flags & B_ERROR) {

				/*
				 * Encountered an error during a rewrite
				 * operation. Need to take failed volume
				 * elements offline.
				 */

				xlvd_log_event ( CE_ALERT,
                                    "xlv: unable to rewrite %s",
                                    io_context);

				xlvd_process_write_error (original_bp,
							  io_context);

				/*
				 * Since we were able to read good data
				 * from some plex, xlvd_process_write_error()
				 * must have concluded that we masked the
				 * error.  i.e., we could not have failed
				 * on all the volume elements across a row.
				 */
				ASSERT ( ! original_bp->b_error);
			}
			else {

				xlvd_log_event ( CE_WARN,
				    "xlv: corrected read failure on %s",
				    io_context);
				io_context->state = XLV_SM_END;

				xlvd_complete_orig_request (io_context, B_TRUE);
			}

			break;


                case XLV_SM_WRITE:

                        /*
                         * Got a write error.
                         */

                        ASSERT (io_context->num_failures);
                        xlvd_process_write_error (original_bp, io_context);

                        break;


		case XLV_SM_DISK_OFFLINE_DONE:
		case XLV_SM_WAIT_FOR_OFFLINE:

			/*
			 * Disks have been taken offline.
			 */
			io_context->state = XLV_SM_END;

			/*
			 * original_bp->b_error etc should have
                         * already been set.
                         */
                        xlvd_complete_orig_request (io_context, B_TRUE);

                        break;


                case XLV_SM_READ_WRITEBACK:

                        /*
                         * We just completed the read part of a
                         * read/write-back sequence. This happens
                         * during plex revives.
                         */
                        ASSERT (retry_bp->b_edev == 0);

                        if (original_bp->b_flags & B_ERROR) {

                                /*
                                 * We cannot use the normal read error
                                 * recovery because we want to make sure
                                 * that we write back to every plex, not
				 * just the one that failed.
				 */

				/*
				 * If we're in the READ_WRITEBACK state, 
				 * then we could not have initiated any 
				 * retries yet.
				 */
				ASSERT (retry_bp->b_edev == 0);

				ASSERT (original_bp->b_flags & B_ERROR);

				/*
				 * Next state can be:
				 *	XLV_SM_REREAD_WRITEBACK,
				 *	XLV_SM_END,
				 *	XLV_SM_WAIT_FOR_OFFLINE.
				 */
				if (xlv_block_io(subvol_index,
						 original_bp) == 0) {
					/* wait for current ios to drain */
					return;
				}
				xlvd_process_read_error(original_bp,io_context);

				break;

                        }
                        else {
				/*
				 * Next state will be XLV_SM_WRITEBACK.
				 */
                                xlvd_do_write_back (original_bp, io_context);
                        }

                        break;


		case XLV_SM_REREAD_WRITEBACK:

			/*
			 * Encountered a failure on the read portion of a
			 * read/write-back operation.
			 * This is the reread of the data from an alternate
			 * plex. If this succeeds, we want to write the
			 * data back to all the plexes.
			 */

                        /*
                         * xlvd does not use the original bp for
                         * retries; it allocates its own.
                         */
                        ASSERT (retry_bp->b_edev != 0
				&& (retry_bp->b_flags & B_XLVD_BUF));

                        if (retry_bp->b_flags & B_ERROR) {
                                /*
                                 * The reread also encountered an error.
                                 * Handle this just like a read error.
                                 */
                                xlvd_process_read_error (retry_bp, io_context);
                        }
                        else {
				xlv_unblock_io(subvol_index);

				xlv_lock_trace ("DEMOTE (xlvd reread/wrtback)",
                                        XLV_TRACE_USER_BUF, original_bp);

				/*
				 * We've masked the original error.
				 */
				original_bp->b_error = 0;
                                original_bp->b_flags &= ~B_ERROR;

				/*
				 * Next state will be XLV_SM_WRITEBACK.
				 */
                                xlvd_do_write_back (original_bp, io_context);
                        }
                        break;

                case XLV_SM_WRITEBACK:

                        /*
                         * Completed the write-back portion of a
                         * read/write-back sequence.
                         */
                        ASSERT (retry_bp->b_edev != 0);

                        if (retry_bp && retry_bp->b_flags & B_ERROR) {
				if (xlv_block_io(minor(original_bp->b_edev),
						 original_bp) == 0) {
					/*
					 * We have been queued until all others
					 * doing io have finished.
					 */
					return;
				}

				xlv_lock_trace ("LOCK UPDATE (xlvd: wrtback)",
                                        XLV_TRACE_USER_BUF, original_bp);

                                /*
                                 * Got an error during the write-back.
                                 * Take the failed volume elements offline.
                                 */
                                xlvd_log_event ( CE_ALERT,
                                    "xlv: write error trying to revive %s",
                                    io_context);

                                xlvd_process_write_error (original_bp, io_context);

                                /*
                                 * We won't do a write-back unless we've
                                 * read data, so xlvd_process_write_error
                                 * must have cleared original_bp->b_error to
				 * indicate that we've masked the error.
                                 */
                                ASSERT (! original_bp->b_error);

                        }
                        else {
                                io_context->state = XLV_SM_END;
                                xlvd_complete_orig_request(io_context, B_FALSE);
                        }

                        break;


		case XLV_SM_UNBLOCK_DRIVER:
			ASSERT (0);
			break;

		default:
			ASSERT (0);
			break;

	}

}



/*
 * This routine is used to reissue a read that failed. The failed
 * read can be either from a regular read, a read/write-back, or
 * another reread.
 */
STATIC void
xlvd_process_read_error (
        buf_t                   *bp,
        xlv_io_context_t        *io_context)
{
	xlv_tab_subvol_t	*xlv_p;
	xlv_slice_t		retry_slices[XLV_MAX_ROWS_PER_IO];	
	daddr_t			first_failed_block, last_failed_block;
	unsigned		subvol_index;
	unsigned char		state;

	xlv_trace ("XLVD_PROCESS_READ_ERROR", XLV_TRACE_USER_BUF, bp);

	state = io_context->state;
	ASSERT ((state == XLV_SM_READ) || (state == XLV_SM_REREAD) ||
		(state == XLV_SM_READ_WRITEBACK) ||
		(state == XLV_SM_REREAD_WRITEBACK));
	
	subvol_index = minor(bp->b_edev);
	xlv_p = &xlv_tab->subvolume[subvol_index];
	ASSERT (XLV_SUBVOL_EXISTS(xlv_p) && XLV_ISOPEN(xlv_p->flags));

	if ((state == XLV_SM_READ) || (state == XLV_SM_READ_WRITEBACK)) {
		ASSERT(XLV_IO_LOCKED_OR_RECOVERING(subvol_index));
	}
	else {
		/*
		 * This is a retry after a failed retry.
		 */
		ASSERT(XLV_RECOVERING(subvol_index));
	}

	/*
	 * Figure out which portion of the read failed.  We don't want
	 * to retry the whole I/O if we don't have to.
	 */
	xlvd_get_failed_blocks (xlv_p, io_context, &first_failed_block,
        			&last_failed_block);

	/*
	 * Attempt to retry the request if we're not already offline
	 */
	if (xlv_p->block_map != NULL && 
	    xlvd_get_retry_slices (io_context, first_failed_block,
               		           last_failed_block, retry_slices, xlv_p)) {
		/*
		 * Reissue the read.
		 */

		if (! (bp->b_flags & B_XLVD_BUF)) {
			/*
			 * If the buffer we have is the original buffer
			 * issued to xlv, then we need to initialize the
			 * retry buffer.
			 */
			buf_t	 *retry_bp = &io_context->retry_buf;

			retry_bp->b_flags = bp->b_flags & ~(B_ERROR|B_XLV_HBUF)
					     | B_BUSY | B_XLVD_BUF;
			retry_bp->b_edev = makedev (XLV_LOWER_MAJOR,
						    minor(bp->b_edev));

			retry_bp->b_sort = bp->b_sort;
			retry_bp->b_error = 0;

			retry_bp->b_bufsize = bp->b_bufsize;
			retry_bp->b_pages = bp->b_pages;
			retry_bp->b_remain = bp->b_remain;

			retry_bp->b_blkno = bp->b_blkno;
			retry_bp->b_bcount = bp->b_bcount;
			retry_bp->b_dmaaddr = bp->b_dmaaddr;

			bp = retry_bp;
		}

		ASSERT (bp->b_flags & B_XLVD_BUF);

		if ((state == XLV_SM_READ_WRITEBACK) ||
		    (state == XLV_SM_REREAD_WRITEBACK))
			io_context->state = XLV_SM_REREAD_WRITEBACK;
		else
			io_context->state = XLV_SM_REREAD;

		xlv_trace ("XLVD_REREAD", XLV_TRACE_USER_BUF, bp);
		xlv_split_among_plexes (xlv_p, bp, retry_slices);

	}
	else {
                /*
                 * We can't find an alternate source to read the failed data.
                 *
                 * We leave the b_error & B_ERROR set in the original
                 * bp since we were unable to mask the read error.
                 */

		xlvd_log_event (CE_ALERT,
			"xlv: uncorrectable read failure on %s",
			io_context);

                xlvd_take_disks_offline (io_context);
        }


}


/*
 * Do the write-back portion of the read-write-back loop that we
 * go through during plex revives.
 * Writes data back to all slices except for the one we read from.
 */
STATIC void
xlvd_do_write_back (
        buf_t                   *bp,
        xlv_io_context_t        *io_context)
{
	xlv_tab_subvol_t        *xlv_p;
	xlv_block_map_entry_t   *map_entry;
	xlv_block_map_entry_t   *last_entry;
	buf_t			*retry_bp;
        xlv_slice_t             *slices_used;
	xlv_slice_t		slices_to_write_back[XLV_MAX_ROWS_PER_IO];
	unsigned		i;
	boolean_t		need_to_writeback;

	xlv_trace ("XLVD_DO_WRITEBACK", XLV_TRACE_USER_BUF,
                   io_context->original_buf);

        xlv_p = &xlv_tab->subvolume[minor(bp->b_edev)];
        ASSERT (XLV_SUBVOL_EXISTS(xlv_p) && XLV_ISOPEN(xlv_p->flags));

	ASSERT (! (bp->b_flags & B_XLVD_BUF)); 

	/*
	 * We assume that we now hold the io lock in access mode.
	 * If we encounter an error, we will upgrade it to update 
	 * mode.
	 */
	ASSERT(XLV_IO_LOCKED(minor(bp->b_edev)));


	/*
	 * See which slices we need to write back to. In the presence
	 * of failed disks, there may not be any to write back.
	 * 
	 * Note that we dereference the entry again using first_block_map_index
	 * because we may be using a new block map if this is an error
	 * retry during which the configuration was changed by a previous
	 * error.
	 */
	if (NULL == xlv_p->block_map) {		/* subvolume is offline */
		goto no_writeback;
	}
	map_entry = &(xlv_p->block_map->map[io_context->current_slices.
			first_block_map_index]);
	last_entry = &(xlv_p->block_map->map[xlv_p->block_map->entries - 1]);
	ASSERT (map_entry);
	ASSERT (last_entry);

	/*
	 * XXX Should the request be terminated here if the subvolume
	 * goes offline (xlv_p->flags & XLV_SUBVOL_OFFLINE)?
	 */

	slices_used = io_context->current_slices.slices_used;

	for (i=0; i < XLV_MAX_ROWS_PER_IO; i++) 
		slices_to_write_back[i] = 0;

	/*
	 * Now mask out the slice that we read from; we don't want
	 * to write to it since it has the right data already.
	 */
	
	need_to_writeback = B_FALSE;
	for (i=0; i < io_context->current_slices.num_entries; i++) {
		if (&map_entry[i] > last_entry)
			break;	/* don't fall off end of block map */
		slices_to_write_back[i] = map_entry[i].write_plex_vector &
			~ slices_used[i];
		if (slices_to_write_back[i] != 0)
			need_to_writeback = B_TRUE;
	}

	if (! need_to_writeback) {
no_writeback:	/*
		 * It's possible for there to be no slices to write
		 * back. i.e., the only other slice has been taken offline.
		 * In this case, just complete the request.
		 */
		io_context->state = XLV_SM_END;
		xlvd_complete_orig_request (io_context, B_FALSE);
		return;
	}

	/*
	 * If necessary, allocate our own bp for the rewrite operation.
	 */
	retry_bp = &io_context->retry_buf;
	if (retry_bp->b_edev == 0) {
		ASSERT (io_context->state == XLV_SM_READ_WRITEBACK);
	}
	else {
		/*
		 * We should only have a retry buf if the original read
		 * failed & we had to retry the operation.
		 */
		ASSERT (io_context->state == XLV_SM_REREAD_WRITEBACK);
	}
	io_context->state = XLV_SM_WRITEBACK;


	retry_bp->b_flags = bp->b_flags & ~(B_ERROR|B_READ|B_XLV_HBUF)
			     | B_BUSY | B_WRITE | B_XLVD_BUF;
        retry_bp->b_edev = makedev (XLV_LOWER_MAJOR, minor(bp->b_edev));

        retry_bp->b_sort = (__psunsigned_t) io_context;
        retry_bp->b_error = 0;

        retry_bp->b_bufsize = bp->b_bufsize;
        retry_bp->b_pages = bp->b_pages;
        retry_bp->b_remain = bp->b_remain;

        retry_bp->b_blkno = bp->b_blkno;
        retry_bp->b_bcount = bp->b_bcount;
        retry_bp->b_dmaaddr = bp->b_dmaaddr;

        /*
         * We rewrite the data to all the slices that cover the
	 * range of block numbers, excluding the one that we just
	 * read from.
         */
        xlv_split_among_plexes (xlv_p, retry_bp, slices_to_write_back);

}


/*
 * Write good data back to the failed slices.
 */
STATIC void
xlvd_do_rewrite (
        buf_t                   *retry_bp,
        xlv_io_context_t        *io_context)
{
	xlv_tab_subvol_t        *xlv_p;
	xlv_block_map_entry_t   *map_entry;
	xlv_slice_t		slices_to_rewrite[XLV_MAX_ROWS_PER_IO];
	boolean_t		need_to_rewrite;
	unsigned		i;

	xlv_trace ("XLVD_DO_REWRITE", XLV_TRACE_USER_BUF,
		   io_context->original_buf);

        xlv_p = &xlv_tab->subvolume[minor(retry_bp->b_edev)];
        ASSERT (XLV_SUBVOL_EXISTS(xlv_p) && XLV_ISOPEN(xlv_p->flags));

	ASSERT(XLV_RECOVERING(minor(retry_bp->b_edev)));

	/* subvolume is offline */
	if (xlv_p->block_map == NULL)
		goto norewrite;

	/*
	 * xlvd does not use the original bp for
	 * retries; it allocates its own.
	 */
	ASSERT (retry_bp->b_flags & B_XLVD_BUF);
 
	/*
	 * Note that retry_bp is the buf allocated by xlvd.
	 * We assume that its address range has been set up to
	 * include only the range of blocks that failed.
	 */
	retry_bp->b_flags &= ~(B_ERROR | B_READ);
	retry_bp->b_flags |= (B_WRITE | B_BUSY);

	/*
	 * We need to check against the block map again
	 * in case one of the failed disks has been taken offline
	 * by some other process?
	 */

	/*
	 * NOTE here that we assume that the failed slices and current
	 * slices start with the same block map entry. This holds
	 * because we retry whole requests.
	 */
	map_entry = &(xlv_p->block_map->map[io_context->current_slices.
                        first_block_map_index]);
        ASSERT (map_entry);

	for (i=0; i < XLV_MAX_ROWS_PER_IO; i++) 
                slices_to_rewrite[i] = 0;

        /*
         * Now mask out the slice that we read from; we don't want
         * to write to it since it has the right data already.
	 * Also, figure out if we need to write the good data back
	 * to the failed plex(es). If the failed plex has already
	 * been taken offline, then there is no need to rewrite it.
         */

	need_to_rewrite = B_FALSE;
        for (i=0; i < io_context->current_slices.num_entries; i++) {
		slices_to_rewrite[0] = io_context->failed_slices.slices_used[0]
		 & map_entry[0].write_plex_vector;
		if (slices_to_rewrite[0])
			need_to_rewrite = B_TRUE;
	}

	if (need_to_rewrite)
		xlv_split_among_plexes (xlv_p, retry_bp, slices_to_rewrite);
	else {
norewrite:
		/*
		 * Nothing to write back. The failed plex has probably
		 * been taken offline already as a result of an earlier
		 * failure. Just complete this request.
		 */
		io_context->state = XLV_SM_END;
		xlvd_complete_orig_request (io_context, B_TRUE);
		return;
	}

}


/*
 * Mark all the disk parts specified in the io_context->failure
 * record offline.
 */
STATIC void
xlvd_take_disks_offline (
        xlv_io_context_t        *io_context)
{
	buf_t		*original_bp;

	/*
	 * Request that the disk labels be updated.
	 */
	io_context->state = XLV_SM_WAIT_FOR_OFFLINE;
	original_bp = io_context->original_buf;

	xlv_trace ("XLVD_TAKE_DISK_OFFLINE", XLV_TRACE_USER_BUF, original_bp);

	ASSERT(XLV_RECOVERING(minor(original_bp->b_edev)));
	bp_enqueue (original_bp, &xlv_labd_request_queue);

	/*
	 * xlv_labd will modify the incore data structures.
	 * xlv_labd will also detect offline requests for volume
	 * elements that are already offline.
	 */
}


/*
 * Mark original request complete.
 *
 * io_blocked says whether io is blocked on this subvolume, i.e., error
 * recovery is happening.  If true, this routine will unblock io before
 * releasing the io lock.
 */
STATIC void
xlvd_complete_orig_request (
	xlv_io_context_t	*io_context,
	boolean_t		io_blocked)
{
	buf_t		*bp;

	bp = io_context->original_buf;

	ASSERT ((bp->b_flags & B_XLV_HBUF) && !(bp->b_flags & B_XLVD_BUF));

	ASSERT(io_blocked ? XLV_RECOVERING(minor(bp->b_edev))
			  : XLV_IO_LOCKED(minor(bp->b_edev)));

	xlv_trace ("XLVD_COMPLETED_ERROR_PROCESS", XLV_TRACE_USER_BUF, bp);

	xlv_res_mem_free(&xlv_ioctx_mem, io_context);

	bp->b_sort = 0;		/* io_context */
	bp->b_flags &= ~B_XLV_HBUF;

	if (io_blocked) xlv_unblock_io(minor(bp->b_edev));
	else xlv_release_io_lock(minor(bp->b_edev));

	xlv_lock_trace("UNLOCK (xlvd_cmplt_orig_rqst)", XLV_TRACE_USER_BUF, bp);


	/*
	 * For now, mark entire original request as failed
	 * if we get any errors.
	 */
	if (bp->b_flags & B_ERROR) 
		bp->b_resid = bp->b_bcount;

	buftrace("XLVd_DONE", bp);

        iodone(bp);
}



/*
 * Queues a request for xlvd.
 * This is called by the upper xlv strategy routine.
 */
void
xlvd_queue_request (
	buf_t			*original_bp,
	xlv_io_context_t	*io_context)
{
	/*REFERENCED*/
	unsigned                minor_dev;

	minor_dev = minor(original_bp->b_edev);

	/*
	 * If this is the initial request (rather than a retry),
	 * then we may have to release the xlv_io_lock if there
	 * is already another request for the same subvolume in
	 * the queue.  Otherwise, we'd deadlock.
	 */
	switch (io_context->state) {

		case XLV_SM_READ:
		case XLV_SM_WRITE:

			/*
			 * We should have the lock in access mode.
			 */
			ASSERT(XLV_IO_LOCKED(minor_dev));

			break;

		case XLV_SM_READ_WRITEBACK:

			/*
			 * Keep the io_lock in access mode. We don't need to
			 * upgrade to update mode unless we encounter an error.
			 *
			 * Note that we still need to release our lock and use
			 * the lock_count to keep track because the xlvd may
			 * be blocked waiting for this lock. If the xlvd is
			 * blocked, then we'd never make progress & we'd
			 * deadlock.
			 */
			xlv_lock_trace (
				"INCR_LCK_CNT (xlvd_q_rqst_rdwb_mmap_wrt)",
				XLV_TRACE_USER_BUF, original_bp);

			break;

		case XLV_SM_WRITEBACK:

			/*
			 * In the second stage of a multi-stage operation that
			 * does not have an error, we would normally have the
			 * lock in access mode.
			 *
			 * We can have it in update mode if we are in the
			 * writeback state because we may have encountered
			 * an error during the read. (In which case xlvd
			 * would have upgraded the lock.)
			 * READ_WRITEBACK -> REREAD_WRITEBACK -> WRITEBACK.
			 */
			ASSERT(XLV_IO_LOCKED(minor_dev));

			break;


		default:

			/*
			 * This is one of the other retry cases; we must have
			 * blocked io on this subvolume
			 */
			ASSERT(XLV_RECOVERING(minor_dev));
			break;
	}

	bp_enqueue (original_bp, &xlvd_work_queue);
}


/*
 * Analyze the failure record in the io_context and determine the
 * portion that failed.
 * This determines which blocks we need to retry.
 *
 * XXX The current version retries the entire I/O.
 */
/*ARGSUSED*/
STATIC void
xlvd_get_failed_blocks (
	xlv_tab_subvol_t        *xlv_p,
	xlv_io_context_t	*io_context,
	daddr_t			*first_failed_block,
	daddr_t			*last_failed_block)
{
	buf_t			*original_bp;
#if 0 /* see below */
        daddr_t                 min_start_blkno, max_end_blkno;
        unsigned                bad_bytecount, f;
        xlv_tab_vol_elmnt_t     *vol_elmnt;
#endif

	original_bp = io_context->original_buf;
        *first_failed_block = original_bp->b_blkno;
        *last_failed_block = original_bp->b_blkno +
                original_bp->b_bcount >> BBSHIFT;

#if 0
        min_start_blkno = xlv_p->subvol_size + 1;
        max_end_blkno = 0;

        /*
         * Go through the failures in the io_context.
         * These failures are expressed in terms of the underlying
         * physical devices. Map them back to the logical blocks.
         */
        for (f=0; f < io_context->num_failures; f++) {
                vol_elmnt = xlv_vol_elmnt_from_disk_part (
                        xlv_p, io_context->failures[f].dev);
                if (vol_elmnt->start_block_no < min_start_blkno)
                        min_start_blkno = vol_elmnt->start_block_no;
                if (vol_elmnt->end_block_no > max_end_blkno)
                        max_end_blkno = vol_elmnt->end_block_no;
        }

	/*
         * At this point, the range (min_start_blkno..max_end_blkno)
         * gives the block range that spans the failed disk parts.
         * Note that if we had 2 bad disks with a good disk in between
         * them, we would count the good disk in between as bad also.
         * This is ok as all it means is that we do more reads during
         * retry.
         */

        /*
         * Since the failed I/O may not have been aligned on volume
         * element boundaries, adjust the start and ending block numbers.
         */
        if (original_bp->b_blkno > min_start_blkno)
                min_start_blkno = original_bp->b_blkno;

	*first_failed_block = min_start_blkno;
        *last_failed_block = max_end_blkno;
#endif
}


/*
 * Compute the slices from which we need to do the reread.
 * If the retry is possible, this function return B_TRUE.
 *
 * This routine assumes that first_failed_block starts on a volume
 * element boundary.
 */
/* ARGSUSED2 */
STATIC boolean_t
xlvd_get_retry_slices (
	xlv_io_context_t	*io_context,
	daddr_t			first_failed_block,
	daddr_t			last_failed_block,
	xlv_slice_t		retry_slices[],
	xlv_tab_subvol_t        *xlv_p)
{
	unsigned		i;
	xlv_blk_map_history_t	*current_slices, *failed_slices;
	xlv_block_map_entry_t	*block_map_entry;

        current_slices = &io_context->current_slices;
	failed_slices = &io_context->failed_slices;

	/* if the block map isn't present, there's nothing to retry */
	if (!xlv_p->block_map)
		return B_FALSE;

	/*
	 * Update failed_slices[] to include the current set that
	 * failed.   Also zero out retry_slices[].
	 */
	for (i=0; i < XLV_MAX_ROWS_PER_IO; i++) {
		failed_slices->slices_used[i] |= current_slices->slices_used[i];
		retry_slices[i] = 0;
	}

	/*
	 * NOTE: For now, we retry the whole I/O to keep the
	 * bookkeeping simple.
	 */
	block_map_entry = &(xlv_p->block_map->map[io_context->current_slices.
                        first_block_map_index]);

	ASSERT (block_map_entry->end_blkno >= first_failed_block);

	/*
	 * Now see if we can read the same set of data from an 
	 * alternate slice.
	 */

	for (i=0; i < current_slices->num_entries; i++) {

		/*
		 * Figure out which slices we can read from after
		 * eliminating those that failed, then pick one of them.
		 */
		retry_slices[i] = FIRST_BIT_SET (
			  block_map_entry->read_plex_vector &
			  ~failed_slices->slices_used[i]);

		if (retry_slices[i] == 0)
			return B_FALSE;

                if (last_failed_block <= block_map_entry->end_blkno) 
                        goto done;

		block_map_entry++;
        }
        ASSERT (0);

done:
	return B_TRUE;
}


/*
 * Returns all the volume elements across a "row".
 */

typedef struct {
	daddr_t			start_blkno;		/* input */
	unsigned		num_vol_elmnts;		/* input/output */
	xlv_tab_vol_elmnt_t	*vol_elmnt[XLV_MAX_PLEXES]; /* output */
} ve_in_row_arg_t;


STATIC int
online_ve_in_row_func (
        xlv_tab_vol_elmnt_t     *ve,
        ve_in_row_arg_t         *ve_in_row_arg)
{
	unsigned	i;

	if ((ve->start_block_no == ve_in_row_arg->start_blkno) &&
	    (ve->state != XLV_VE_STATE_INCOMPLETE) &&
	    (ve->state != XLV_VE_STATE_OFFLINE)) {
		i = ve_in_row_arg->num_vol_elmnts++;
		ve_in_row_arg->vol_elmnt[i] = ve;
	}

        return 0;       /* continue searching */
}


STATIC void
all_online_vol_elmnt_in_row (
	xlv_tab_subvol_t	*sv,
	xlv_tab_vol_elmnt_t	*ve,
	unsigned		*num_vol_elmnts,	/* output */
	xlv_tab_vol_elmnt_t	*ve_table[])		/* output */
{
	ve_in_row_arg_t		ve_in_row_arg;
	unsigned		i;

	/*
	 * Find all the online volume elements across the same "row"
	 * as the current volume element. These the all the
	 * volume elements that cover the same range of logical
	 * blocks.
	 */
	ve_in_row_arg.start_blkno = ve->start_block_no;
	ve_in_row_arg.num_vol_elmnts = 0;

	xlv_for_each_ve (sv, (XLV_VE_FP)online_ve_in_row_func, &ve_in_row_arg);

	/*
	 * At least current volume element must be in the row.
	 */
	ASSERT (ve_in_row_arg.num_vol_elmnts > 0);

	*num_vol_elmnts = ve_in_row_arg.num_vol_elmnts;
	for (i=0; i < ve_in_row_arg.num_vol_elmnts; i++)
		ve_table[i] = ve_in_row_arg.vol_elmnt[i];
}


typedef struct {
        daddr_t                 start_blkno;
        int                     num_failed_ve;
        xlv_tab_vol_elmnt_t     *failed_vol_elmnts[XLV_MAX_PLEXES];
} failed_vol_elmnt_t;


STATIC void
xlvd_process_write_error (
        buf_t                   *original_bp,
        xlv_io_context_t        *io_context)
{
        xlv_tab_subvol_t        *xlv_p;
	xlv_tab_vol_elmnt_t	*ve;
	unsigned		rows_that_have_failures = 0;
	failed_vol_elmnt_t	failures_by_row[XLV_MAX_FAILURES];
	unsigned		i, n, num_vol_elmnts, good_vol_elmnts;
	xlv_tab_vol_elmnt_t     *ve_in_this_row[XLV_MAX_PLEXES];
	unsigned		subvol_index;
	boolean_t		masked_error = B_TRUE;


	subvol_index = minor(original_bp->b_edev);
        xlv_p = &xlv_tab->subvolume[subvol_index];
        ASSERT (XLV_SUBVOL_EXISTS(xlv_p) && XLV_ISOPEN(xlv_p->flags));

        if (io_context->state == XLV_SM_WRITE) { 

                /*
                 * Acquire config lock in access mode to block
                 * configuration changes.
                 * We only grab the lock on the initial error,
                 * not on failed retry reads.
                 */
		if (xlv_block_io(subvol_index, original_bp) == 0) {
			/* must wait for current ios to finish */
			return;
		}

		xlv_lock_trace ("UPGRADE (xlvd_proc_wrt_err)",
                                        XLV_TRACE_USER_BUF, original_bp);

        }

	/* subvolume is already offline */
	if (xlv_p->block_map == NULL) {
		masked_error = B_FALSE;
		goto nowrite;
	}

	/*
	 * Get the list of all volume elements that failed. Put those
	 * that are in the same row (i.e., that cover the same range
	 * of disk blocks) together.
	 */
	for (i=0; i < io_context->num_failures; i++) {

		ve = xlv_vol_elmnt_from_disk_part (xlv_p,
			io_context->failures[i].dev);
		ASSERT (ve);

		/*
		 * See if this volume element is already in the
		 * list of failures or if it is on the same row
		 * as another failed volume element.
		 */
		for (n=0; n < rows_that_have_failures; n++) {
			unsigned	v;


			for (v = 0; v < failures_by_row[n].num_failed_ve; v++)
			{
				if (ve == failures_by_row[n].
				    failed_vol_elmnts[v]) {
					/*
					 * More than one disk in a volume
					 * element failed.
					 */
					goto next_failure;
				}
			}

			if (ve->start_block_no == 
			    failures_by_row[n].start_blkno) {
				failures_by_row[n].failed_vol_elmnts[
				    failures_by_row[n].num_failed_ve++] = ve;
				goto next_failure;
			}
		}

		/*
		 * If we reach here, then this is the first volume element
		 * (in the io_context) for a range of logical blocks (i.e.,
		 * in a row) that failed.
		 */
		failures_by_row[rows_that_have_failures].start_blkno =
			ve->start_block_no;
		failures_by_row[rows_that_have_failures].num_failed_ve = 1;
		failures_by_row[rows_that_have_failures].
			failed_vol_elmnts[0] = ve;
		rows_that_have_failures++;

next_failure:
		;

	} /* end for */

	/*
	 * At this point, failures_by_row[] should contain a list of rows
	 * that have failed volume elements.
	 */

	/*
	 * Count the number of good volume elements left in those
	 * rows that had failures.
	 *
	 * If the row has at least one good volume element left,
	 * then we have written the data successfully to at least one
	 * plex and have masked the failure.
	 */
	masked_error = B_TRUE;

	for (n = 0; n < rows_that_have_failures; n++) {

		all_online_vol_elmnt_in_row (xlv_p,
			failures_by_row[n].failed_vol_elmnts[0],
        		&num_vol_elmnts, ve_in_this_row);

		/*
		 * We want to know if there are still good volume
		 * elements left across this row, after eliminating
		 * the failed volume element. 
		 */
		good_vol_elmnts = num_vol_elmnts -
			failures_by_row[n].num_failed_ve;

		/*
		 * Make sure that the failed volume element itself is not
		 * already incomplete or offline. Otherwise, it would not
		 * be counted in the total returned by all_online_vol_elmnt..
		 *
		 * This can happen if we are processing the 2nd write
		 * error after the first write error has already taken
		 * the affected volume element offline. (The 2nd one was
		 * queued while the first was being processed.)
		 */
		if (good_vol_elmnts <= 0) {
			int		v;
			unsigned char   state;

			for (v=0; v < failures_by_row[n].num_failed_ve; v++) {
				state = failures_by_row[n].
				    failed_vol_elmnts[v]->state;

				if ((state == XLV_VE_STATE_INCOMPLETE) ||
				    (state == XLV_VE_STATE_OFFLINE)) {
					good_vol_elmnts++;
#ifdef DEBUG
					xlvd_log_event (CE_WARN,
				"xlv: adjusted for offline failure on %s",
                                		io_context);
#endif

				}

			}
		}

		if (good_vol_elmnts == 0) {

			/*
			 * If we managed to get an error on all the
			 * volume elements within a row, then we were
			 * unable to mask the write error.
			 */
			masked_error = B_FALSE;
		}
	}

nowrite:
	xlvd_log_event (CE_ALERT, "xlv: write failure on %s", io_context);
	if (masked_error) {
		original_bp->b_flags &= ~B_ERROR;
		original_bp->b_error = 0;
	}
	else {
		xlvd_log_event (CE_ALERT,
				"xlv: unable to mask write failure on %s",
				io_context);
	}

	/*
	 * Need to take all the disks within an affected volume
	 * element offline.
	 */
	xlvd_take_disks_offline (io_context);

}


/*
 * Send an error message to syslog.
 */
STATIC void
xlvd_log_event (
	int			level,
	char			*fmt_string,
        xlv_io_context_t        *io_context)
{
	int	i;

	for (i=0; i < io_context->num_failures; i++) {
		cmn_err(level, fmt_string, 
			devtopath (io_context->failures[i].dev));
	}
}
