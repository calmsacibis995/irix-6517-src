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
#ident "$Revision: 1.27 $"

/*
 * plex_klabd - kernel code executed by the xlv_labd process.
 *
 * xlv_labd is a user level process that writes disk labels. It periodically
 * issues a system call into kernel mode to get the next request.
 *
 * NOTE that we assume there is only one xlv_labd daemon.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/kmem.h>
#include <sys/var.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_labd.h>
#include <sys/uuid.h>
#include <sys/bpqueue.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"
#include "xlv_mem.h"


/*
 * The xlv_labd request queue.
 */
extern bp_queue_t	xlv_labd_request_queue;
extern boolean_t	xlv_labd_queue_initialized;
extern bp_queue_t      	xlvd_work_queue;
extern int		xlvmajor;
extern int		xlv_plexing_support_present;
extern sema_t		xlv_cfgchange_notify_lock;

extern zone_t *xlv_io_context_zone;

void xlv_configchange_notification(void);

STATIC int do_offline (
	buf_t *bp, xlv_io_context_t *io_context, caddr_t arg);

STATIC int do_notify_ves_active (
	buf_t *bp, xlv_io_context_t *io_context, caddr_t arg);

STATIC int do_notify_ves_offline (
	buf_t *bp, xlv_io_context_t *io_context, caddr_t arg);

STATIC int do_notify_config_changes (
 	buf_t *bp, xlv_io_context_t *io_context, caddr_t arg);

/*
 * Called by user mode xlv_labd to wait for the next configuration
 * change request from the kernel xlv driver. This will block on
 * a semaphore until a request is available.
 */
unsigned
xlv_next_config_request (caddr_t arg)
{
	xlv_io_context_t	*io_context;
	buf_t			*bp;
	int			get_next_rqst;

	if (! _CAP_ABLE(CAP_DEVICE_MGT)) {
		return EPERM;
	}

	if (! xlv_labd_queue_initialized) {
		return EPIPE;
	}

	do {
		/*
		 * Get the next request for xlv_labd.
		 */
		bp = bp_dequeue_wait (&xlv_labd_request_queue);

		/*
		 * bp_dequeue_wait will return NULL if it caught a
		 * signal. We just return ENOMSG in this case and
		 * let the user daemon decide what to do.
		 */
		if (bp == NULL)
			return ENOMSG;

		io_context = (xlv_io_context_t *) bp->b_sort;
		ASSERT (emajor(bp->b_edev) == xlvmajor);

		xlv_trace("XLV_LABD Request", XLV_TRACE_USER_BUF, bp);

		switch (io_context->state) {
			case XLV_SM_WAIT_FOR_OFFLINE:
				get_next_rqst = do_offline (
					bp, io_context, arg);
				break;

			case XLV_SM_NOTIFY_VES_ACTIVE:
				get_next_rqst = do_notify_ves_active (
					bp, io_context, arg);
				break;

			case XLV_SM_NOTIFY_VES_OFFLINE:
				get_next_rqst = do_notify_ves_offline (
					bp, io_context, arg);
				break;

			case XLV_SM_NOTIFY_CONFIG_CHANGE:
				get_next_rqst = do_notify_config_changes (
					bp, io_context, arg);
				break;

			default:
				ASSERT (0);
				break;
		}

		/* XXX Need to check return status of action routines
		   e.g.., EFAULT. */

	} while (get_next_rqst);

	return 0;

} /* end of xlv_next_config_request() */


/*
 * This routine processes an offline request.
 * It will return 1 if the current request is a no-op. In this case,
 * the caller should just block for the next request.
 */
STATIC int
do_offline (buf_t		*bp,
	    xlv_io_context_t	*io_context,
	    caddr_t		arg)
{
        xlv_config_rqst_t       config_rqst;
        unsigned                i;
        unsigned                failed_ves;
        xlv_tab_vol_elmnt_t     *vol_elmnt;
        xlv_tab_subvol_t        *xlv_p;

#ifdef XLV_SWAP
	int			dp;
	xlv_tab_disk_part_t	*dpp;
#endif

	ASSERT (io_context->state == XLV_SM_WAIT_FOR_OFFLINE);

	/*
	 * The failure record is in terms of disks. We need to
	 * convert them into volume elements. We take an entire
	 * volume element offline if any of its disk parts fail.
	 */
	failed_ves = 0;
	xlv_p = &(xlv_tab->subvolume[minor(bp->b_edev)]);

	for (i=0; i < io_context->num_failures; i++) {
		vol_elmnt = xlv_vol_elmnt_from_disk_part (
			xlv_p, io_context->failures[i].dev);

		/*
		 * See if this volume element is already offline.
		 * Note that this might result in our not having
		 * any requests at all for this iteration.
		 */
		if (vol_elmnt->state != XLV_VE_STATE_OFFLINE) {

			/*
			 * Mark the volume element offline in
			 * the xlv_tab.
			 * We can do this since we hold the io_lock
			 * in update mode and the config lock in
			 * access mode.
			 */
			vol_elmnt->state = XLV_VE_STATE_OFFLINE;

#ifdef XLV_SWAP
			/*
			 * XXXjleong Close devices associated w/ offline ve
			 */
			for (dp = 0, dpp = vol_elmnt->disk_parts;
			     dp < vol_elmnt->grp_size;
			     dp++, dpp++) {
				(void)xlv_devclose(dpp, xlv_p->open_flag, OTYP_LYR, get_current_cred());
			}
#endif

			config_rqst.vol_elmnt_uuid[failed_ves] =
				vol_elmnt->uuid;
			failed_ves++;

		}
	}

	/*
	 * If there's no work to be done, return now.
	 * We still need to send a notification because the sender
	 * is waiting for us.
	 */
	if (failed_ves == 0) {
		bp_enqueue (bp, &xlvd_work_queue);
		return 1;
	}

	/*
         * Update the block map for this subvolume.
	 * The new block map must spans the same space.
         */
	if (xlv_p->block_map) {
		int depth = xlv_p->block_map->entries;

        	kmem_free (xlv_p->block_map, 0);
        	xlv_p->block_map = xlv_tab_create_block_map (xlv_p, NULL, NULL);

		if (xlv_p->block_map) {
			/*
	 		 * A block map can be still created even
			 * if the very last volume element is bad.
			 * So must check that the depth of the block
			 * map has not changed.
			 *
			 * XXX Cannot use the field "subvol_depth"
			 * in the subvolume because it is not kepth
			 * up to date in xlv_attr.c.
			 */
			if (xlv_p->block_map->entries < depth) {
				kmem_free (xlv_p->block_map, 0);
				xlv_p->block_map = NULL;
			}
		}
	} else {
		/*
		 * The block map would not exist if the volume is
		 * already offline.
		 */
		ASSERT(XLV_VOL_STATE_MISS_UNIQUE_PIECE == xlv_p->vol_p->state);
		ASSERT(XLV_SUBVOL_OFFLINE & xlv_p->flags);
	}

        if (xlv_p->block_map) {
		/*
                 * Go through the block map and see if there are any
                 * block ranges for which there are no active volume
                 * elements. If so, make one of them active.
		 */
		for (i=0; i < xlv_p->block_map->entries; i++) {

			if (xlv_p->block_map->map[i].read_plex_vector == 0) {
				xlv_p->block_map->map[i].read_plex_vector =
					FIRST_BIT_SET (xlv_p->block_map->
					    map[i].write_plex_vector);
			}
		}
	}
	else {
                /*
                 * All the plexes failed. Change the state of the
                 * volume (incore) to offline so that new requests
                 * will get EIO.  (Requests that are in the process
                 * of being retried will still get the original error.)
                 */
                xlv_p->vol_p->state = XLV_VOL_STATE_MISS_UNIQUE_PIECE;
                xlv_p->flags |= XLV_SUBVOL_OFFLINE;
        }

	/*
	 * We report completion before we ask the user-mode
	 * code to write the disk labels. This can cause disks to be out of
	 * sync if we crash now. But lets us go on if all the buffers are
	 * exhausted.
	 */
	bp_enqueue (bp, &xlvd_work_queue);

        /*
         * Copy the request to user mode so the daemon can
         * update the disk labels.
         *
         * It's ok for us to update the kernel structures ahead of
         * time since this is all atomic with respect to both I/O
         * and configuration changes.  (We have all the locks.)
         */
        config_rqst.operation = XLV_LABD_RQST_OFFLINE;
	config_rqst.vol_uuid = xlv_p->vol_p->uuid;
        config_rqst.num_vol_elmnts = failed_ves;

        if (copyout ((caddr_t)&config_rqst, (caddr_t) arg,
                     sizeof (xlv_config_rqst_t)))
                return EFAULT;

        return 0;

} /* end of do_offline() */


/*
 * Let the user-mode xlv_labd know that the state of a volume element
 * has changed to ACTIVE. (from STALE).
 */
STATIC int
do_notify_ves_active (buf_t                  *bp,
                      xlv_io_context_t       *io_context,
		      caddr_t		     arg)
{
	xlv_ves_active_rqst_t	*config_rqst;

	io_context = (xlv_io_context_t *) bp->b_sort;
	config_rqst = (xlv_ves_active_rqst_t *) io_context->buffers;

	if (copyout ((caddr_t)config_rqst, (caddr_t) arg,
                     sizeof (xlv_ves_active_rqst_t))) {
		cmn_err (CE_WARN, "xlv_klabd: ves_active copyout failed");
		return 1;
	}

	/*
	 * Now free the arguments.
	 */
	kmem_free (config_rqst, 0);
	kmem_free(io_context, 0);
	kmem_free(bp, 0);

	return 0;

} /* end of do_notify_ves_active() */


/*
 * Let the user-mode xlv_labd know that the state of a volume element
 * has changed to OFFLINE.
 *
 * The xlv driver transitioned the ve to the offline state when it could
 * not open the volume element as oppose to taking a ve offline because
 * I/O failures.
 */
STATIC int
do_notify_ves_offline (buf_t                  *bp,
                      xlv_io_context_t       *io_context,
		      caddr_t		     arg)
{
	xlv_config_rqst_t	*config_rqst;	/* XXXjleong right type? */

	io_context = (xlv_io_context_t *) bp->b_sort;
	config_rqst = (xlv_config_rqst_t *) io_context->buffers;

	if (copyout ((caddr_t)config_rqst, (caddr_t) arg,
                     sizeof (xlv_config_rqst_t))) {
		cmn_err (CE_WARN, "xlv_klabd: ves_offline copyout failed");
		return 1;
	}

	/*
	 * The config request should have been set up already.
	 *
         * config_rqst.operation = XLV_LABD_RQST_OFFLINE;
	 * config_rqst.vol_uuid = xlv_p->vol_p->uuid;
	 * config_rqst.num_vol_elmnts = failed_ves;
	 */

	/*
	 * Now free the arguments.
	 */
	kmem_free (config_rqst, 0);
	kmem_free (io_context, 0);
	kmem_free (bp, 0);

	return 0;

} /* end of do_notify_ves_offline() */


/*
 * Let the user-mode xlv_labd know that the xlv configuration has
 * changed.
 */
/* ARGSUSED */
STATIC int
do_notify_config_changes (
	buf_t                   *bp,
	xlv_io_context_t        *io_context,
	caddr_t                 arg)
{
 	static xlv_config_rqst_t config_rqst;
 	int retval = 0;
 
	config_rqst.operation = XLV_LABD_RQST_CONFIG_CHANGE;
 
 	if (copyout((caddr_t)&config_rqst, (caddr_t)arg,
 		    sizeof(xlv_config_rqst_t))) {
 
 		cmn_err(CE_WARN, "xlv_klabd: config_change copyout failed");
 		retval = 1;
 	}
 
 	/* data copied, we can release the lock on the static buffer */
 	vsema(&xlv_cfgchange_notify_lock);
 
 	return(retval);
}

/* xlv_configchange_notification()
 *
 * Queue a configuration change notification to be delivered to the
 * user-level labd.  (initiates a disk label re-read when received).
 *
 * This routine uses a mutex to lock the static buffer.  If another
 * config change request is received before this one has been issued
 * to labd, then just drop the request.
 */

void
xlv_configchange_notification(void)
{
	static buf_t buf;
	static xlv_io_context_t io_context;
 
 	/*
	 * User-level labd only runs with plexing support.
	 * The kernel labd queue must exist.
	 */
 	if (!xlv_plexing_support_present || !xlv_labd_queue_initialized) {
 		return;
	}
 
 	/*
 	 * if we aquire the mutex then send the config change request
	 * otherwise, just drop the request since the current request
	 * hasn't made it to user space labd yet (effectively a duplicate)
	 */

 	if (cpsema(&xlv_cfgchange_notify_lock)) {
		/* test & set 'in use' flag
		   if set, mark send more flag */

		io_context.original_buf = &buf;
 		io_context.state = XLV_SM_NOTIFY_CONFIG_CHANGE;
 		io_context.buffers = 0;
 		buf.b_sort = (__psunsigned_t) &io_context;
 		buf.b_edev = makedev(xlvmajor, 0);

 		bp_enqueue(&buf, &xlv_labd_request_queue);
 	}
}
