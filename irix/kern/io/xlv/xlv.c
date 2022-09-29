#ident "$Revision: 1.146 $"

/**************************************************************************
 *									  *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/bpqueue.h>
#include <sys/mkdev.h>
#include <sys/scsi.h>
#include <sys/invent.h>
#include <sys/ktrace.h>
#include <sys/cred.h>

#include <sys/debug.h>
#include <sys/elog.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/dump.h>
#include <sys/open.h>
#include <sys/splock.h>
#include <sys/sema.h>
#include <ksys/vfile.h>
#include <sys/var.h>
#include <sys/major.h>
#include <sys/xlate.h>
#ifdef EVEREST
#include <sys/EVEREST/everest.h>
#endif

#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_labd.h>
#ifdef DEBUG
#include <sys/cmn_err.h>
#endif
#include <sys/dvh.h>
#include <sys/uuid.h>
#include <sys/grio.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"
#include "xlv_mem.h"
#include <sys/xlv_lock.h>
#include <sys/xlv_stat.h>
#include "xlv_xlate.h"

#include <sys/vfs.h>

extern int possibly_diff_io4(dev_t, dev_t);
extern int xlv_get_hw_scsi_info(dev_t, int *, int *, int *, int *);

/*
 * Driver is MP safe, we do our own semaphoring.
 * Write back cache before strategy routine is called.
 */
int xlvdevflag = D_MP|D_WBACK;

lock_t xlv_top_strat_lock;
lock_t xlv_low_strat_lock;
sema_t xlv_cfgchange_notify_lock;
sema_t xlv_memcfg;			/* protect re-configuring xlv memory */

lock_t xlv_init_read_slice_lock;

extern zone_t	*grio_buf_data_zone;

extern bp_queue_t	xlv_labd_request_queue;

ktrace_t        *xlv_trace_buf = NULL;  /* global xlv trace buffer */
ktrace_t        *xlv_lock_trace_buf = NULL;  /* global xlv lock trace buffer */

xlv_res_mem_t     xlv_ioctx_mem, xlv_top_mem, xlv_low_mem;

unsigned int	  xlv_ioctx_slots=5, xlv_topmem_slots=5, xlv_lowmem_slots=15;

unsigned int      xlv_ioctx_mem_scale=50, xlv_top_mem_scale = 50, 
		  xlv_low_mem_scale = 50;

unsigned int      xlv_ioctx_mem_threshold = 10, xlv_top_mem_threshold = 10, 
		  xlv_low_mem_threshold = 10;

unsigned int 	  xlv_top_mem_bufs, xlv_low_mem_bufs;


/*
 * Maximum size for a single request to xlv driver.
 *
 * It's typically 4 megabytes (400,000 hex) (4,194,304 decimal)
 */
#define MAXTRANS (ctob(v.v_maxdmasz))

#define	INVALID_VE(ve)	(((ve)->state == XLV_VE_STATE_OFFLINE) || \
			 ((ve)->state == XLV_VE_STATE_INCOMPLETE))

/* Prototypes */

extern void griostrategy(buf_t *bp);
extern int devavail(dev_t);

void xlv_low_strat_done (buf_t *bp);
static void xlv_top_strat_done (buf_t *bp);
static int xlv_lower_strategy (buf_t *buf, unsigned I_plex_vector);

/* Table for locating the xlv_tab structures, in the master lv file. */

extern int xlv_maxunits;
extern int xlv_maxlocks;
extern int xlvmajor;

extern int xlv_plexing_support_present;
int xlv_failsafe;

extern int xlv_boot_startup(dev_t);


/* devtopath(): given a dev, return the pathname for it. The dev is expected
 * to be a block disk device. 
 *
 * This relies on the hwgraph vertex to canonical name conversion routines.
 * If the device is an old style dev_t, then the string generated looks
 * like "/hw/major/%d/minor/%d'.
 */ 

char *
devtopath(dev_t dev)
{
	static char pathbuf[MAXDEVNAME];

	bzero(pathbuf, sizeof(pathbuf));
        dev_to_name(dev, pathbuf, sizeof(pathbuf));
	return (pathbuf);
}

void
xlvinit()
{
	unsigned		  sv;

	spinlock_init(&xlv_low_strat_lock, "xlv_flck");
	spinlock_init(&xlv_top_strat_lock, "xlv_top_strat_lck");
	spinlock_init(&xlv_init_read_slice_lock, "xlv_init_read_slice_lock");
	initnsema_mutex(&xlv_cfgchange_notify_lock,"xlv_cfgchange_notify_lock");
	initnsema(&xlv_memcfg, 1, "xlv_memcfg");

	if (xlv_plexing_support_present) {
	    xlv_ioctx_mem.size = sizeof(xlv_io_context_t);
	    xlv_ioctx_mem.slots = xlv_ioctx_slots;
	    xlv_ioctx_mem.scale = xlv_ioctx_mem_scale;
	    xlv_ioctx_mem.threshold = xlv_ioctx_mem_threshold;
	    xlv_res_mem_create(&xlv_ioctx_mem, "xlv_ioc");

	    xlv_top_mem.size = sizeof(xlvmem_t) + sizeof(buf_t);
	    xlv_top_mem.slots = xlv_topmem_slots;
	    xlv_top_mem.scale = xlv_top_mem_scale;
	    xlv_top_mem.threshold = xlv_top_mem_threshold;
	    xlv_res_mem_create(&xlv_top_mem, "xlv_top");
	}

	xlv_low_mem.size = sizeof(xlvmem_t) + sizeof(buf_t);
	xlv_low_mem.slots = xlv_lowmem_slots;
	xlv_low_mem.scale = xlv_low_mem_scale;
	xlv_low_mem.threshold = xlv_low_mem_threshold;
	xlv_res_mem_create(&xlv_low_mem, "xlv_low");

	/* Initialize the lock table. */
	for (sv = 0; sv < xlv_maxlocks; sv++) {
		xlv_io_lock_init(sv);
	}

#ifdef DEBUG
	xlv_trace_buf = ktrace_alloc (256, KM_SLEEP);
	xlv_lock_trace_buf = ktrace_alloc (512, KM_SLEEP);
#endif
}


/*
 * Notify xlv_labd of state change to offline.
 *
 * Note that xlv_labd will free the storage for the buf, io_context,
 * and the request itself.
 */
STATIC void
request_offline (xlv_config_rqst_t *rqst)
{
	buf_t			*bp;
	xlv_io_context_t	*io_context;

	/*
	 * Issue the request only if the labd queue exists,
	 * and the label daemon part of the plexing software.
	 */
	if (!xlv_plexing_support_present) {
		kmem_free(rqst, 0);
		return;
	}

	bp = kmem_zalloc (sizeof(buf_t), KM_SLEEP);
	io_context = kmem_zalloc(sizeof(xlv_io_context_t), KM_SLEEP);
	io_context->original_buf = bp;
	io_context->state = XLV_SM_NOTIFY_VES_OFFLINE;
	io_context->buffers = (xlvmem_t *) rqst;
	bp->b_sort = (__psunsigned_t) io_context;
	bp->b_edev = makedev(xlvmajor, 0);
#ifdef DEBUG
	/*
	 * only called from xlvopen()
	 */
	cmn_err(CE_DEBUG, "xlvopen: QUEUED offline request\n");
#endif
	bp_enqueue (bp, &xlv_labd_request_queue);
}


int
xlvopen(dev_t *devp, int flag, int otype, struct cred *cred)
{
	dev_t 			dev;
	xlv_tab_subvol_t 	*xlv_p;
        xlv_tab_plex_t  	*plex;
        xlv_tab_vol_elmnt_t     *vol_elmnt;
	xlv_tab_disk_part_t	*dpp;
	unsigned long 		sv;
	int 			errno = 0;
	int 			wasopen;
        int     		disk_count = 0;
        int     		p, ve, dp;
	int			error_count = 0;
	int			retval = 0;
	xlv_config_rqst_t	*rqst = NULL;

	dev = *devp;
	if (minor(dev) >= xlv_maxunits) {
		return ENXIO;
	}

	ASSERT (xlv_maxunits <= xlv_maxlocks);

	sv = minor(dev);
	xlv_acquire_io_lock(sv, NULL);
	xlv_lock_trace ("LOCK xlvopen", XLV_TRACE_ADMIN, (buf_t *)sv);

	if (! xlv_tab) {
		xlv_release_io_lock(sv);
		xlv_lock_trace ("UNLOCK xlvopen(1)", XLV_TRACE_ADMIN,
				(buf_t *)sv);
                return ENXIO;
	}

	xlv_p = &xlv_tab->subvolume[sv];
	if (! XLV_SUBVOL_EXISTS(xlv_p)) {
		xlv_release_io_lock(sv);
		xlv_lock_trace ("UNLOCK xlvopen(2)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
		return ENXIO;
	}

	/*
	 * If volume was already open, we must not pass further opens to lower
	 * level devices, since we are using OTYP_LYR and some lower-level
	 * drivers honor strict OTYP_LYR open/close pairing.
	 */
	retval = mutex_spinlock(&xlv_io_lock[sv].lock);

	wasopen = XLV_ISOPEN(xlv_p->flags);

	/* Keep track of OTYPs received so we can send a correct last close */
	switch (otype) {

		case OTYP_CHR : xlv_p->flags |= XLV_OPEN_CHR;
			break;

		case OTYP_MNT : xlv_p->flags |= XLV_OPEN_MNT;
			break;

		case OTYP_BLK : xlv_p->flags |= XLV_OPEN_BLK;
			break;

		case OTYP_SWP : xlv_p->flags |= XLV_OPEN_SWP;
			break;

		case OTYP_LYR : xlv_p->flags |= XLV_OPEN_LYR;
			break;

		default	      :
			mutex_spinunlock(&xlv_io_lock[sv].lock, retval);
			xlv_release_io_lock(sv);
			xlv_lock_trace ("UNLOCK xlvopen(3)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
			return EINVAL;
	};
	mutex_spinunlock(&xlv_io_lock[sv].lock, retval);
	xlv_release_io_lock(sv);

	if (wasopen) {

		FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): xlvopen for minor %d - already open\n", minor(dev)););

		xlv_lock_trace ("UNLOCK xlvopen(4)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
		return 0;
	}

	FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): xlvopen for minor %d\n",
			 minor(dev)););

	/* have to open underlying devices, possibly set ves offline */
	/*
	 * XXX need to allocate memory for block_map and ve_array
	 * It's probably okay, as this code path is for the first open.
	 * Since it hasn't been opened yet, this volume cannot be a
	 * swap volume.
	 */
	xlv_acquire_cfg_lock(sv);

	if (! xlv_tab) {
		xlv_release_cfg_lock(sv);
		xlv_lock_trace ("UNLOCK xlvopen(1)", XLV_TRACE_ADMIN,
				(buf_t *)sv);
                return ENXIO;
	}

	xlv_p = &xlv_tab->subvolume[sv];
	if (! XLV_SUBVOL_EXISTS(xlv_p)) {
		xlv_release_cfg_lock(sv);
		xlv_lock_trace ("UNLOCK xlvopen(2)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
		return ENXIO;
	}

	error_count = 0;
	disk_count = 0;	    /* Keep track of number of disks that
			       we've opened so we can close them on error. */
	for (p = 0; p < XLV_MAX_PLEXES; p++) {

		plex = xlv_p->plex[p];

		/*
		 * If a plex is offline (or missing), we can have a
		 * null entry.
		 */
		if (! plex)
			continue;

		for (ve = 0; ve < plex->num_vol_elmnts; ve++) {
			vol_elmnt = &plex->vol_elmnts[ve];
			if (INVALID_VE(vol_elmnt)) {
				/*
				 * Don't bother trying to open volume
				 * elements that cannot be read/write.
				 */
				error_count++;
				retval = ENODEV;
				continue;
			}
			for (dp = 0, dpp = vol_elmnt->disk_parts;
			     dp < vol_elmnt->grp_size;
			     dp++, dpp++) {
				errno = xlv_devopen(dpp, flag, OTYP_LYR, cred);
				FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): open returned %d for %s\n", errno, devtopath(DP_DEV(*dpp))););

				if (errno) {
					cmn_err_tag(136,CE_ALERT,
						"xlv: unable to open 0x%x",
						DP_DEV(*dpp));
					retval = errno;
					error_count++;
					break;
				}
			}
			if (errno == 0) {
				disk_count += vol_elmnt->grp_size;
				vol_elmnt->flags |= XLV_VE_OPEN;
				continue;	/* go to next ve to open */
			}

			/*
			 * There was an error in opening this ve,
			 * so go back and free all opened disks
			 * from this ve.
			 */
			for (dpp--; dp > 0; dp--, dpp--) {
				xlv_devclose(dpp, flag, OTYP_LYR, cred);
			}
			/*
			 * Take the volume element offline.
			 */
			vol_elmnt->state = XLV_VE_STATE_OFFLINE;
			if (xlv_plexing_support_present) {
				/*
				 * Build notification request to the xlv_labd.
				 */
				if (rqst == NULL) {
					/*
					 * Allocate the request message.
					 */
					rqst = kmem_zalloc(sizeof(
						xlv_ves_active_rqst_t),
						KM_SLEEP);
					rqst->operation = XLV_LABD_RQST_OFFLINE;
					rqst->num_vol_elmnts = 0;
					rqst->vol_uuid = xlv_p->vol_p->uuid;
				}
				rqst->vol_elmnt_uuid[rqst->num_vol_elmnts++]
					= vol_elmnt->uuid;
				if (rqst->num_vol_elmnts == XLV_MAX_FAILURES) {
					/*
					 * its full so sent it
					 */
					request_offline(rqst);
					rqst = NULL;
				}
			}

		} /* for every ve in plex */

	} /* for every plex */

	if (error_count) {
		xlv_block_map_t	*block_map = NULL;

		if (rqst && xlv_plexing_support_present) {
			request_offline(rqst);
			rqst = NULL;
		}

		if (xlv_p->num_plexes > 1) {
			block_map = xlv_tab_create_block_map(xlv_p, NULL, NULL);
		}
		if (xlv_p->block_map) {
			kmem_free (xlv_p->block_map, 0);
		}
		xlv_p->block_map = block_map;

		if (NULL == xlv_p->block_map)
			goto close_all;
	}

	xlv_p->subvol_size = xlv_tab_subvol_size(xlv_p);
	xlv_p->open_flag = flag;

	xlv_release_cfg_lock(sv);
	xlv_lock_trace ("UNLOCK xlvopen", XLV_TRACE_ADMIN, (buf_t *)sv);
	return (0);

close_all:
	/*
	 * Come here in case we failed to open all the underlying devices.
	 */
        for (p = 0; p < XLV_MAX_PLEXES; p++) {
                plex = xlv_p->plex[p];
		if (! plex)
			continue;

                for (ve = 0; ve < plex->num_vol_elmnts; ve++) {
                        vol_elmnt = &plex->vol_elmnts[ve];
			if (INVALID_VE(vol_elmnt) ||
			   (!(vol_elmnt->flags & XLV_VE_OPEN))) {
				/*
				 * Don't close volume elements we didn't open.
				 */
				continue;
			}
			for (dp = 0, dpp = vol_elmnt->disk_parts;
			     dp < vol_elmnt->grp_size;
			     dp++, dpp++) {
				if (disk_count-- <= 0)
					goto done;
				xlv_devclose(dpp, flag, OTYP_LYR, cred);
                        }
			/* clear the ve open flag */
			vol_elmnt->flags &= ~XLV_VE_OPEN;
                } /* for every ve in plex */
        } /* for every plex */

done:
	xlv_p->flags &= ~XLV_OPENMASK;
	xlv_release_cfg_lock(sv);
	xlv_lock_trace ("UNLOCK xlvopen(5)", XLV_TRACE_ADMIN, (buf_t *)sv);
	return (retval);

} /* end of xlvopen() */


int
xlvclose(dev_t dev, int flag, int otype, struct cred *cred)
{
        xlv_tab_subvol_t        *xlv_p;
        xlv_tab_plex_t          *plex;
        xlv_tab_vol_elmnt_t     *vol_elmnt;
	unsigned long		sv;
        int                     errno = 0, error, s, still_open;
        int                     p, ve, dp;

	sv = minor(dev);
	if (sv >= xlv_maxunits)
		return 0;

        xlv_acquire_io_lock(sv, NULL);
	xlv_lock_trace ("LOCK xlvclose", XLV_TRACE_ADMIN, (buf_t *)sv);

	if (! xlv_tab) {
		xlv_release_io_lock(sv);
		xlv_lock_trace ("UNLOCK xlvclose(1)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
                return ENXIO;
        }

        xlv_p = &xlv_tab->subvolume[sv];
        if (! XLV_SUBVOL_EXISTS(xlv_p)) {
		xlv_release_io_lock(sv);
		xlv_lock_trace ("UNLOCK xlvclose(2)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
                return 0;
        }

	/* Also allow for the case where we opened it, did DIOCSETLV, and
	 * immediately close it again: lower level devices were never opened
	 * so should not be closed.
	 */

	if (!XLV_ISOPEN(xlv_p->flags)) {
		xlv_release_io_lock(sv);
		xlv_lock_trace ("UNLOCK xlvclose(3)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
		return 0;
	}

	/* Tick off the OTYP. We must assume that last-close protocol
	 * is correctly maintained by calling routines within each OTYP */

	FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): xlvclose for minor %d\n", minor(dev)););

	s = mutex_spinlock(&xlv_io_lock[sv].lock);
	switch (otype) {

		case OTYP_CHR : xlv_p->flags &= ~XLV_OPEN_CHR;
			break;

		case OTYP_MNT : xlv_p->flags &= ~XLV_OPEN_MNT;
			break;

		case OTYP_BLK : xlv_p->flags &= ~XLV_OPEN_BLK;
			break;

		case OTYP_SWP : xlv_p->flags &= ~XLV_OPEN_SWP;
			break;

		case OTYP_LYR : xlv_p->flags &= ~XLV_OPEN_LYR;
			break;

		default	      :
			mutex_spinunlock(&xlv_io_lock[sv].lock, s);
			xlv_release_io_lock(sv);
			xlv_lock_trace ("UNLOCK xlvclose(4)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
			return EINVAL;
	};

	/* having done the above administration, if it's still open it's
	 * not the last close of any type, so don't close lower devices. */
	still_open = XLV_ISOPEN(xlv_p->flags);
	mutex_spinunlock(&xlv_io_lock[sv].lock, s);
	xlv_release_io_lock(sv);

	if (still_open) {
		xlv_lock_trace ("UNLOCK xlvclose(5)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
		return 0;
	}

	xlv_acquire_cfg_lock(sv);

	if (! xlv_tab) {
		xlv_release_cfg_lock(sv);
		xlv_lock_trace ("UNLOCK xlvclose(1)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
                return ENXIO;
        }

        xlv_p = &xlv_tab->subvolume[sv];
        if (! XLV_SUBVOL_EXISTS(xlv_p)) {
		xlv_release_cfg_lock(sv);
		xlv_lock_trace ("UNLOCK xlvclose(2)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
                return 0;
        }

	/*
	 * If someone opened this subvolume while we were getting the cfg lock,
	 * just return without closing any disks.
	 */
	if (XLV_ISOPEN(xlv_p->flags)) {
		xlv_release_cfg_lock(sv);
		xlv_lock_trace ("UNLOCK xlvclose(3)", XLV_TRACE_ADMIN,
                                (buf_t *)sv);
		return 0;
	}

        for (p = 0; p < XLV_MAX_PLEXES; p++) {
                plex = xlv_p->plex[p];
		if (! plex)
			continue;

		for (ve = 0, vol_elmnt = plex->vol_elmnts; 
		     ve < plex->num_vol_elmnts; 
		     ve++, vol_elmnt++) {
			xlv_tab_disk_part_t *dpp;

			if (INVALID_VE(vol_elmnt)) {
				/*
				 * Don't close volume elements that are
				 * not read/write. They shouldn't have
				 * open disks.
				 */
				continue;
			}

			if (!(vol_elmnt->flags & XLV_VE_OPEN)) {
				/*
				 * Don't close volume elements we didn't open 
				 */
				continue;
			}

			for (dp = 0, dpp = vol_elmnt->disk_parts;
			     dp < vol_elmnt->grp_size;
			     dp++, dpp++) {
				error = xlv_devclose(dpp, flag, OTYP_LYR, cred);
				FO_DEBUG(cmn_err(CE_DEBUG, "XLV(dbg): close returned %d for %s\n", error, devtopath(DP_DEV(*dpp))););
				if (error && !errno)
					errno = error;
			}
			/* clear the ve open flag */
			vol_elmnt->flags &= ~XLV_VE_OPEN;
		} /* for every ve in plex */
	} /* for every plex */

	xlv_release_cfg_lock(sv);
	xlv_lock_trace ("UNLOCK xlvclose", XLV_TRACE_ADMIN, (buf_t *)sv);

	return errno;

} /* end of xlvclose() */


/*
 * xlvsize64(): return the size of the volume in blocks.
 * If there are multiple plexes, return the largest size.
 */
int
xlvsize64(dev_t dev, daddr_t *size)
{
        xlv_tab_subvol_t        *xlv_p;
	unsigned long		sv;
	daddr_t			sz;

	sv = minor(dev);
	if (sv >= xlv_maxunits) {
		*size = -1;
		return (ENXIO);
	}

	xlv_acquire_io_lock(sv, NULL);
	xlv_lock_trace ("LOCK xlvsize", XLV_TRACE_ADMIN, (buf_t *)sv);

	if (! xlv_tab) {
		xlv_release_io_lock(sv);
		xlv_lock_trace ("UNLOCK xlvsize(1)", XLV_TRACE_ADMIN,
				(buf_t *)sv);
		*size = -1;
                return (ENXIO);
        }

        xlv_p = &xlv_tab->subvolume[sv];
        if ( XLV_SUBVOL_EXISTS(xlv_p)) 
		sz = xlv_p->subvol_size;
	else
		sz = -1;

	xlv_release_io_lock(sv);
	xlv_lock_trace ("UNLOCK xlvsize", XLV_TRACE_ADMIN, (buf_t *)sv);
	if ((*size = sz) < 0)
		return (ENXIO);
	else {
		return (0);
	}
}

/*
 * old version of size interface
 */
int
xlvsize(dev_t dev)
{
	daddr_t s;
	int errno;

	errno = xlvsize64(dev, &s);
	if (errno || s > 0x7fffffff)
		return (-1);
	return (s);
}

/*
 * Function that returns the dev_t's for the data, log and real-time
 * subvolumes of a volume, given the dev_t of a subvolume in that volume.
 * If the log subvolume doesn't exist, return the data subvolume's dev_t.
 *
 * Return 0 if all okay; return ENXIO otherwise.
 */
int
xlv_get_subvolumes(dev_t xlv_dev, dev_t *ddev, dev_t *logdev, dev_t *rtdev)
{
	xlv_tab_subvol_t *xlv_p;
	xlv_tab_subvol_t *sv_p;
	unsigned	  sv;

	ASSERT(emajor(xlv_dev) == XLV_MAJOR);
	sv = minor(xlv_dev);

	if ((xlv_tab == NULL) || (xlv_tab->num_subvols == 0) ||
	    (sv >= xlv_tab->max_subvols))
		return ENXIO;

	xlv_acquire_io_lock(sv, NULL);
	xlv_p = &xlv_tab->subvolume[sv];
	if (! XLV_SUBVOL_EXISTS(xlv_p)) {
		xlv_release_io_lock(sv);
		return ENXIO;
	}
	*ddev   = (sv_p = xlv_p->vol_p->data_subvol) ? sv_p->dev : 0;
	*logdev = (sv_p = xlv_p->vol_p->log_subvol)  ? sv_p->dev : *ddev;
	*rtdev  = (sv_p = xlv_p->vol_p->rt_subvol)   ? sv_p->dev : 0;

	xlv_release_io_lock(sv);

	return 0;
}


/*******************************************************************
 *
 * The xlv strategy routine is implemented as two layers. The upper
 * strategy routine breaks up a request into subrequests each of which
 * are contiguous within a plex. This allows the lower xlv strategy
 * routine to work with requests that are guaranteed to be contiguous
 * and contained in a single plex.
 *
 * The upper strategy routine is responsible for acquiring the xlv io
 * lock.
 *
 *******************************************************************/

/*******************************************************************
 *
 *            Upper XLV Strategy Support Routines.
 *
 *******************************************************************/


/*
 * Number of bits set in an int. This only works with the lower 4 bits.
 */
static unsigned bits_set_in [] =
	{ 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

#define BITS_SET_IN(c) (bits_set_in[((unsigned)c & 0xf)])

/*
 * Lookup table that returns a bitmask with only the lowest bit set.
 *  e.g., first_bit_set_in (0x3) is 0x1. First is taken from least
 *  significant bit.
 */
unsigned xlv_first_bit_set_in [] =
	{ 0, 0x1, 0x2, 0x1, 0x4, 0x1, 0x2, 0x1, 0x8,
	  0x1, 0x2, 0x1, 0x4, 0x1, 0x2, 0x1 };


/*
 * Compute the number of buffers that the upper strategy routine
 * needs.
 * For reads, we return the number of contiguous extents that are
 * needed to satisfy the request. We will look first in the current
 * slice.
 * For writes, we do the same except that we do it for all slices.
 *
 * This routine also returns the address of the first block map
 * entry that covers the requested range of disk blocks.
 */
static unsigned 
top_strat_count_bufs (
	xlv_block_map_t		*block_map,	/* the block map.*/
	daddr_t			start_blkno,	/* starting and ending */
	daddr_t			end_blkno,	/* blkno of transfer. */
	int			operation,	/* B_READ or B_WRITE. */
	xlv_slice_t 		slices_to_use[XLV_MAX_ROWS_PER_IO],
						/* slices to read from
						   or write to. */
	xlv_block_map_entry_t	**first_block_map_entry,
	unsigned		*first_block_map_index) /* output */
{
	xlv_block_map_entry_t	*map_entry = NULL;
	xlv_slice_t 		plex_vector, prev_plex_vector, changed_slices;
	xlv_slice_t		slice_bits;
	unsigned		buf_count;
	int			b;
	unsigned		slices_to_use_index = 0;

	/* 
	 * Find the block map entry in which the start of the requested
	 * transaction falls. 
	 */
	for (b = 0; b < block_map->entries; b++) {
		if (block_map->map[b].end_blkno >= start_blkno) {
			map_entry = &(block_map->map[b]);
			break;
		}
	}

	ASSERT (map_entry);

	*first_block_map_entry = map_entry;
	*first_block_map_index = b;


	/*
	 * We use slices_to_use[] as a mask to select the slices we
	 * want to use. 
	 *
	 * For read operations, only one bit should be set in the 
	 * slice_bits vector at any time. If we encounter gaps, then
	 * we would step down to the next entry in slices_to_use[].
	 *
	 * For normal writes, slices_to_use[] should be set to indicate
	 * all the plexes to which we want to write. Normally, this
	 * would be all 1's. It would not be all 1's if this were
	 * a rewrite after failure or a read/write-back. This
	 * makes the AND operation below a no-op and allows us to
	 * use the same code for reads and writes.
	 */

	/*
	 * For writes, we need to allocate a buffer for
	 * each slice. Whenever an extent of blocks start or
	 * end, we'd need an additional buffer. We can detect
	 * these cases by using xor's.
	 *
	 * For reads, we allocate one buffer. For writes we allocate
	 * a buffer for each slice. This is controlled by ANDing
	 * with slice_bits and then counting the number of 1's.
	 */

	prev_plex_vector = 0;
	buf_count = 0;

	/*
	 * While we've not run beyong the end of the block map.
	 * (That can happen if the request starts within a valid
	 * block number but extends beyond the end of the logical
	 * volume. The lower level strategy routine carefully
	 * maps b_resid to account for it.)
	 */
	while (map_entry < &(block_map->map[block_map->entries]) ) {
		plex_vector = (operation == B_READ)?
			map_entry->read_plex_vector :
			map_entry->write_plex_vector;

		slice_bits = slices_to_use[slices_to_use_index];
		ASSERT (slice_bits);

#ifdef DEBUG
		/*
		 * For reads, only one bit should be set in each
		 * entry in slices_to_use[].
		 * For normal writes, each entry in slices_to_use[]
		 * should be all 1's - indicating that we should
		 * write to all plexes.
		 * For a rewrite after failure or a writeback (of
		 * a read/write-back), it's possible for there to
		 * be less than all bits set in each entry within
		 * slices_to_use[].
		 */

		if (operation == B_READ) 
			ASSERT (BITS_SET_IN (slice_bits) == 1);
		else
			ASSERT (BITS_SET_IN (slice_bits) != 0);
#endif

		if (! (plex_vector & slice_bits)) {

			/*
			 * We've encountered a gap and need to switch
			 * to another slice.
			 * We'll use the one specified in the next
			 * entry in slices_to_use[], if possible.
			 */

			slices_to_use_index++;
			ASSERT (slices_to_use_index < XLV_MAX_ROWS_PER_IO);

			slice_bits = slices_to_use[slices_to_use_index];

			if (! (plex_vector & slice_bits))
				slice_bits = FIRST_BIT_SET(plex_vector);
		}

		plex_vector &= slice_bits;
		ASSERT (plex_vector);

		if (changed_slices = plex_vector ^ prev_plex_vector) {
			/*
			 * changed_slices gives us all the slices
			 * that have changed.  However, we only need
			 * to allocate a new buffer if we changed
			 * from a 0 to 1.  So we AND changed_slices
			 * with plex_vector to get only those changes
			 * which resulted in a 1.
			 *
			 * Changing from 1 to 0 means that the 
			 * slice did not extend into the new range of
			 * blocks. We don't need to add a new buffer
			 * in this case until we get the next 0 to 1
			 * transition.
			 */
			buf_count += BITS_SET_IN (
				changed_slices & plex_vector);
			prev_plex_vector = plex_vector;
		}

		if (end_blkno <= map_entry->end_blkno)
			break;

		map_entry++;
	}


	return buf_count;

} /* end of top_strat_count_bufs() */


/*
 * We use the addr_array_entry to keep track of the bp's that we
 * are creating for the lower xlv strategy routine. One entry exists
 * per plex. All the bp's are doubly linked, starting at buf_list.
 *
 * The basic algorithm is:

 *  1 whenever we start an extent of blocks on a plex, call set_start_bufs()
 *    to record the starting blkno for the affected plexes.
 *  2 when an extent of blocks end (either because we reached a gap
 *    or we reached the last block that needs to be transferred), call
 *    call set_end_bufs which will build the bp's with and link them
 *    into the addr_array[plex] for each plex that is affected.
 *  3 at the end, just issue all the bp's for each plex.
 */

typedef struct {
	daddr_t		blkno;
	daddr_t 	blocks;
	buf_t		*buf_list;
} addr_array_entry_t;


#if 0
#ifdef DEBUG
/*
 * Print out, for each plex, the starting and ending block numbers
 * of request buffers.
 */
static void
list_bufs_addrs (
	addr_array_entry_t addr_array[])
{
	int	plex;
	buf_t	*bp;

	for (plex=0; plex < XLV_MAX_PLEXES; plex++) {

		bp = addr_array[plex].buf_list;
		if (! bp)
			continue;

		printf ("\t\tPlex %d\n", plex);
		do {
			printf ("\t\t[bp] start: %d, count(blocks): %d\n",
				bp->b_blkno, (bp->b_bcount>>BBSHIFT));
			bp = bp->b_forw;
		} while (bp != NULL);
	}
}
#else
#define list_bufs_addrs(x) ;
#endif
#endif /* 0 */


/*
 * Record the starting blkno in the addr_array for all the plexes
 * specified.
 */
static void
set_start_bufs (
	addr_array_entry_t addr_array[],
	daddr_t		   blkno,
	unsigned 	   plexes)
{
	if (plexes & 0x1)
		addr_array[0].blkno = blkno;
	if (plexes & 0x2)
		addr_array[1].blkno = blkno;
	if (plexes & 0x4)
		addr_array[2].blkno = blkno;
	if (plexes & 0x8)
		addr_array[3].blkno = blkno;
}

/*
 * Actually write the start and bytes_to_xfer fields in a buffer.
 * The start blkno's are assumed to have been set already by
 * set_start_bufs().
 * Once the start & bytes_to_xfer fields are written, the buffer
 * is linked to the appropriate entry in the addr_array using b_forw.
 */
static void
set_end_bufs (
	addr_array_entry_t addr_array[],
	daddr_t		   start_blkno,
	daddr_t		   end_blkno,
	caddr_t		   base_addr,
	buf_t		   **buf_list,
	unsigned 	   plex_vector)
{
	unsigned	bit_pos, i;
	buf_t		*bp;

	bp = *buf_list;

	for (i=0, bit_pos = 0x1; i < XLV_MAX_PLEXES; i++, bit_pos <<= 1) {

		if (plex_vector & bit_pos) {

			/*
			 * Set the transfer addresses for both the
			 * incore buffer as well as the block offsets on
			 * 'disk'.
			 */
			bp->b_dmaaddr = base_addr +
			    ((addr_array[i].blkno - start_blkno) << BBSHIFT);
			bp->b_blkno = addr_array[i].blkno;
			bp->b_bcount = (end_blkno - bp->b_blkno + 1) << BBSHIFT;

			/*
			 * Link the bp into the list for this plex using
			 * b_forw.
			 */
			bp->b_forw = addr_array[i].buf_list;
			addr_array[i].buf_list = bp;

			bp++;
		}
	}

	*buf_list = bp;
}



/*
 * Fills in the b_blkno and b_bcount fields of a series of buf's 
 * allocated continguously within a struct xlvmem.
 * 
 * The output is the addr_array[] which sorts the list of bp's by
 * plex number.
 */
static void
top_strat_format_bufs (
	xlv_block_map_t         *block_map,     /* the block map.*/
        xlv_block_map_entry_t   *map_entry,	/* the entry in block map
					that contains blocks in this range.*/
	buf_t			*buf_list,	/* first buf in xlvmem struct */
        daddr_t                 start_blkno,    /* starting block number */
	daddr_t			end_blkno,	/* ending block number. */
	caddr_t			base_addr,	/* base of (incore) user data.*/
        int                     operation,      /* B_READ or B_WRITE. */
        xlv_slice_t 	        slices_to_use[XLV_MAX_ROWS_PER_IO],
	xlv_slice_t		slices_used[XLV_MAX_ROWS_PER_IO],
	unsigned		*rows_in_slices_used,
	addr_array_entry_t      addr_array[])
{

	daddr_t			blkno;
	unsigned		end_blkno_for_buf, start_blkno_for_buf;
	unsigned		slices_to_use_index = 0;

	/* The following are all bitvectors */
	xlv_slice_t		slice_bits, changed_slices,
				prev_plex_vector, plex_vector;
	unsigned		start_bufs, end_bufs;

	/*
	 * On input, map_entry points to the first block map entry
	 * that covers this I/O.
	 */
	ASSERT (map_entry);

	bzero (addr_array, XLV_MAX_PLEXES * sizeof(addr_array_entry_t));

	/*
	 * For writes, we need to allocate a buffer for
	 * each slice. Whenever an extent of blocks start or
	 * end, we'd need an additional buffer. We can detect
	 * these cases by using xor's.
	 *
	 * For reads, we allocate one buffer. For writes we allocate
	 * a buffer for each slice. This is controlled by ANDing
	 * with slice_bits and then counting the number of 1's.
	 */

	prev_plex_vector = 0;

	/*
	 * Set blkno to the starting blk number of current (starting)
	 * block map entry.
	 */
	if (map_entry == block_map->map)
		blkno = 0;
	else
		blkno = (map_entry-1)->end_blkno + 1;

	slices_used[0] = slices_to_use[0];

	/*
         * If the request extends beyond the end of this subvolume's
         * address range, trim it.
         */
        if (end_blkno > block_map->map[block_map->entries-1].end_blkno)
		end_blkno = block_map->map[block_map->entries-1].end_blkno;

	while (map_entry < &(block_map->map[block_map->entries]) ) {

		plex_vector = (operation == B_READ) ?
			map_entry->read_plex_vector :
			map_entry->write_plex_vector;

		slice_bits = slices_to_use[slices_to_use_index];

		if ( ! (plex_vector & slice_bits)) {

			/*
                         * We've encountered a gap and need to switch
                         * to another slice.
                         * We'll use the one specified in the next
                         * entry in slices_to_use[], if possible.
                         */

                        slices_to_use_index++;
                        ASSERT (slices_to_use_index < XLV_MAX_ROWS_PER_IO);

                        slice_bits = slices_to_use[slices_to_use_index];

			if (! (plex_vector & slice_bits))
                                slice_bits = FIRST_BIT_SET(plex_vector);

			/*
                         * Record the slice that we actually used.
                         */
                        slices_used[slices_to_use_index] = slice_bits;
                }

		plex_vector &= slice_bits;
		ASSERT (plex_vector);

		if (changed_slices = plex_vector ^ prev_plex_vector) {
			/*
			 * changed_slices gives us all the slices
			 * that have changed.
			 *
			 * If a bit changes from 0 -> 1, that means
			 * we need to start a new buffer.
			 * If a bit changes from 1 -> 0, that means
			 * we need to end the previous buffer.
			 */

			start_bufs = changed_slices & plex_vector;
			if (start_bufs) {
				start_blkno_for_buf = blkno;
				if (start_blkno > blkno)
					start_blkno_for_buf = start_blkno;
				set_start_bufs (addr_array,
					start_blkno_for_buf, start_bufs);
			}

			end_bufs = changed_slices & prev_plex_vector;
			if (end_bufs) {
				end_blkno_for_buf = (map_entry-1)->end_blkno;
				if (end_blkno < end_blkno_for_buf)
					end_blkno_for_buf = end_blkno;

				set_end_bufs (addr_array, start_blkno, 
					end_blkno_for_buf, base_addr,
					&buf_list, end_bufs);
			}

			prev_plex_vector = plex_vector;
		}

		if (end_blkno <= map_entry->end_blkno)
			break;

		blkno = map_entry->end_blkno + 1;
		map_entry++;
	}

	ASSERT (end_blkno <= map_entry->end_blkno);

	set_end_bufs (addr_array, start_blkno, end_blkno, base_addr,
		      &buf_list, plex_vector);

#if 0
	list_bufs_addrs (addr_array);
#endif

	*rows_in_slices_used = slices_to_use_index+1;

} /* end of top_strat_format_bufs() */


/*
 * Check whether the current request falls within a critical region.
 * If the request is within a critical region, return 1. The caller
 * should then return since the bp has been queued for later processing.
 */
static int
check_critical_region (
	xlv_tab_subvol_t  	*xlv_p,
	buf_t			*bp,
	daddr_t			start_blkno,
	daddr_t			end_blkno)
{
	daddr_t			cregion_start, cregion_end;
	int			s;

	/*
	 * We assume that we are only called if we are within
	 * the read/write-back region.
	 */

	ASSERT (! ((end_blkno < xlv_p->rd_wr_back_start) ||
                    (start_blkno > xlv_p->rd_wr_back_end)) );

	/*
	 * See if the request should override the critical region.
	 * This is necessary to allow I/Os initiated by the process
	 * that set up the critical region to do its own I/O (e.g.,
	 * the I/Os that do the plex copies.)
	 */
	if (bp->b_dmaaddr == xlv_p->critical_region.excluded_rqst)
		return 0;

	cregion_start = xlv_p->critical_region.start_blkno;
	cregion_end = xlv_p->critical_region.end_blkno;

	/*
	 * Inactive critical region.
	 */
	if ((cregion_end == 0) && (cregion_start == 0))
		return 0;

	if (! ((start_blkno > cregion_end) ||
	       (end_blkno < cregion_start))) {

		buf_t		*p;

		/*
		 * We are in the critical region, queue the request in
		 * fifo order using av_forw & av_back.
		 */

		s = mutex_spinlock(&xlv_top_strat_lock);
		if (p = xlv_p->critical_region.pending_requests) {
			p->av_back->av_forw = bp;
			bp->av_back = p->av_back;
			p->av_back = bp;
			bp->av_forw = p;
		}
		else {
			xlv_p->critical_region.pending_requests = bp;
			bp->av_forw = bp->av_back = bp;
		}
		mutex_spinunlock(&xlv_top_strat_lock,s);

		/*
		 * The caller should then release the XLV_IO_LOCK.
		 */
		return 1;
	}

	return 0;

} /* end of check_critical_region() */


#define xlv_do_mmap_copy 0


/*
 * Split a request into multiple subrequests such that each subrequest
 * has no holes and can be serviced by a single plex.
 * This routine will also rotate the 'read' slice for load-balancing.
 *
 * This entrypoint is called by the upper xlv strategy routine as well
 * as the error retry code in xlvd.
 *
 * The 'slice' parameter is for error retries where we want to direct
 * the I/O to specific slices.
 */
int
xlv_split_among_plexes (xlv_tab_subvol_t        *xlv_p,
			buf_t			*original_bp,
			xlv_slice_t		slices_to_use[])
{
	xlv_io_context_t	*io_context;
	buf_t			*bp, *bp_list, *next_bp;
	unsigned		plex_vector, operation, bufs_needed;
	unsigned		io_context_state, first_slice;
	daddr_t			start_blkno, end_blkno;
	addr_array_entry_t      addr_array[XLV_MAX_PLEXES];
	buf_t			*tlvb, *nlvb;
	xlvmem_t                *lvm;
	xlv_slice_t		slices;
	int			s, i, plex, b;
	int			ioc_alloced = B_FALSE;
	xlv_block_map_entry_t   *first_block_map_entry;

	ASSERT(minor(original_bp->b_edev) == xlv_p - xlv_tab->subvolume);

	ASSERT(XLV_IO_LOCKED_OR_RECOVERING(minor(original_bp->b_edev)));

        operation = (original_bp->b_flags & B_READ)? B_READ : B_WRITE;
        start_blkno = original_bp->b_blkno;
        end_blkno = start_blkno + (original_bp->b_bcount >> BBSHIFT) - 1;

	/*
	 * If this is the head buffer (i.e., the original request),
	 * set up the io_context for this request and point bp->b_sort
	 * to it. 
	 */
	if ((original_bp->b_flags & B_XLV_HBUF) &&
	    ( !(original_bp->b_flags & B_XLVD_BUF)) ) {
		/*
		 * See if we need to do a read/write-back or a
		 * normal read. If we are in the read/write-back
		 * region, also check to see if we are in a
		 * critical region.
		 *
		 * check_critical_region() returns TRUE iff
                 * the request falls within a critical region
                 * and was queued. In this case, we should just
                 * return.  The process that dequeues the bp will
                 * call xlvstrategy again.
		 */
		if ((end_blkno < xlv_p->rd_wr_back_start) ||
		    (start_blkno > xlv_p->rd_wr_back_end)) {

			/*
			 * We are either before or after the read/write-back
			 * region; the plexes are consistent. Do a normal read.
			 */
			io_context_state = (operation == B_READ) ? XLV_SM_READ:
					XLV_SM_WRITE;
		}
		else {

			if (check_critical_region (xlv_p, original_bp,
			    start_blkno, end_blkno) ) {

				io_context_state = XLV_SM_INVALID;

				original_bp->b_flags &= ~B_XLV_HBUF;

				/*
				 * We need to drop the xlv_io_lock[] 
				 * because suspended requests will be
				 * reissued via xlvstrategy() which
				 * will reacquire the locks.
				 */
				xlv_release_io_lock(minor(xlv_p->dev));
				xlv_lock_trace ("UNLOCK (xlv_split cregion)",
                                        XLV_TRACE_USER_BUF, original_bp);

				return 0;
			}
			io_context_state = (operation == B_READ) ? 
                                XLV_SM_READ_WRITEBACK : XLV_SM_WRITE;
		}

		/*
		 * We allocate the io_context only after we know that
		 * this request won't be deferred because it's in the
		 * critical region. Remember, deferred requests are
		 * reissued at xlvstrategy() because we need to reaquire
		 * locks.
		 */

		if (original_bp->b_sort == NULL) {
		    io_context = (xlv_io_context_t *)
			     xlv_res_mem_alloc(&xlv_ioctx_mem, original_bp,
						sizeof(xlv_io_context_t));
		    /* request was queued for processing later */
		    if (io_context == NULL) {
			xlv_release_io_lock(minor(xlv_p->dev));
			return 0;
		    }
		    ioc_alloced = B_TRUE;
		}
		else {
		    io_context = (xlv_io_context_t *) original_bp->b_sort;
		}

                io_context->original_buf = original_bp;
		io_context->state = io_context_state;

		if (operation == B_READ) {

			/*
			 * We bump initial_read_slice under a spinlock.
			 * Note that the way the increment & checks are done
			 * ensures that we'll get reasonable values even if
			 * xlv_p->initial_read_slice was never initialized.
			 */
			s = mutex_spinlock(&xlv_init_read_slice_lock);

			do {
				xlv_p->initial_read_slice++;
				if (xlv_p->initial_read_slice >= XLV_MAX_PLEXES)
					xlv_p->initial_read_slice = 0;
			} while (! (xlv_p->plex[xlv_p->initial_read_slice]));

			first_slice = xlv_p->initial_read_slice;

			mutex_spinunlock(&xlv_init_read_slice_lock, s);

			/*
			 * Set up our slices_to_use[] such that we'd
			 * read from the initial slice if possible and
			 * any available one otherwise.
			 */
			for (b=0; b < xlv_p->block_map->entries; b++) {
				plex_vector = xlv_p->block_map->map[b].
					read_plex_vector;

				slices = 1 << first_slice;

				if (! (slices & plex_vector))
					slices = FIRST_BIT_SET(plex_vector);

				slices_to_use[b] = slices;
			}
		}
		else {
			/*
			 * Write to all the slices.
			 */
			slices_to_use[0] = -1;
			slices_to_use[1] = -1;
			slices_to_use[2] = -1;
		}

		original_bp->b_sort = (__psunsigned_t) io_context;
	}
	else {
		ASSERT (original_bp->b_flags & B_XLVD_BUF);

		/*
                 * On a retry, always use the slice(s) specified by
                 * the caller.
                 */

		io_context = (xlv_io_context_t *)original_bp->b_sort;
		ASSERT (io_context);
	}

	xlv_trace("XLV_SPLIT", XLV_TRACE_USER_BUF, original_bp);

	/*
	 * Figure out how many buffers we need to complete this request.
	 */
	bufs_needed = top_strat_count_bufs (
		xlv_p->block_map, start_blkno, end_blkno, operation,
		slices_to_use,
		&first_block_map_entry,
		&io_context->current_slices.first_block_map_index);

	/* if the read bit vector gets set to zero for some reason (bug in
	 * software), then do the sane thing here and return an error so
	 * we don't hang the system.
	 */
	if (!bufs_needed) {
		if (ioc_alloced)
			xlv_res_mem_free(&xlv_ioctx_mem, io_context);
		return ENOSPC;
	}

	/* Allocate the buffers from reserved memory. */
	ASSERT(xlv_top_mem.size >= sizeof(xlvmem_t) + bufs_needed*sizeof(buf_t));
	lvm = xlv_res_mem_alloc(&xlv_top_mem, NULL, 
			        sizeof(xlvmem_t) + bufs_needed*sizeof(buf_t));
	lvm->bufs = bufs_needed;

        /*
         * If more than one buffer is needed, i.e., the driver has
         * allocated buffers, make sure that the user's original
         * buffer is mapped to kernel virtual memory.  All the
         * buffers we've allocated are.
	 *
	 * We know that the lower xlv driver will allocate its own
	 * buffers. Therefore, the lower xlv driver will not map
	 * this buffer in or unmap it. We will unmap it in the
	 * completion routine.
         */
	if ((bufs_needed > 1) && (!(original_bp->b_flags & B_MAPPED))) {
                (void)bp_mapin(original_bp);
		lvm->mapped = 1;
	} else {
		lvm->mapped = 0;
	}


	bp_list = lvm->buf;

	/*
	 * Store a pointer to the buffers that we've allocated in
	 * the io_context so that we can free it later.
	 */
	io_context->buffers = lvm;

	/*
	 * Set up all the common fields.
	 */
	for (i = 0, bp = bp_list; i < bufs_needed; i++, bp++) {

		bp->b_flags = original_bp->b_flags & ~(B_XLV_HBUF|B_XLVD_BUF);

		if ( BUF_IS_IOSPL( original_bp ) ) {
			ASSERT( BUF_IS_IOSPL(bp) );
			ASSERT( BUF_GRIO_PRIVATE(original_bp) );
			bp->b_grio_private = original_bp->b_grio_private;
		}

		/*
		 * We change the major number to reflect a call from
		 * the upper driver into the lower driver.
		 */
		bp->b_edev = makedev (XLV_LOWER_MAJOR,
				      minor(original_bp->b_edev));
		bp->b_error = 0;
		bp->b_iodone = xlv_top_strat_done;
		bp->b_private = original_bp;

		/*
		 * We map in original_bp if we need more than 1 buffer
		 * to complete the I/O.
		 * The original_bp could have also been mapped when 
		 * passed to the driver.
		 *
		 * In any case, if the original_bp was not mapped and is 
		 * a pageio buf, the lower xlv driver may map it in.
		 * This can happen if the request actually spans several
		 * disks, for example. We have to propage b_bufsize since
		 * bp_mapin needs it. Also, if the lower xlv driver maps
		 * this buffer, we would need to unmap it when we are done.
		 *
	         * How to determine whether a bp_mapout is needed
	         * in xlv_top_strat_done?
	         * If the B_MAPPED bit from a mapped
	         * upper-level buf is copied into the lvbuf, there
	         * is no way to tell if that lvbuf has has a
	         * bp_mapin done on it by the lower XLV driver.
	         * So, if the upper-level buf is mapped, we should
                 * NOT propagate the B_MAPPED bit to the lvbufs.
                 * However, just turning off the B_MAPPED bit would
                 * fool lower-level drivers into doing a spurious
                 * bp_mapin on pageio bufs. Thus we must turn the
                 * B_PAGEIO bit off too.
                 */

                if (BP_ISMAPPED(bp)) 
                        bp->b_flags &= ~(B_MAPPED | B_PAGEIO);

		bp->b_bufsize = original_bp->b_bufsize;
		bp->b_pages = original_bp->b_pages;
		bp->b_remain = 0;
		bp->b_dmaaddr = 0;
	}

	/* 
	 * b_blkno, b_bcount, and b_forw are set below.
	 */

	top_strat_format_bufs (
		xlv_p->block_map,
		first_block_map_entry, 
        	bp_list, start_blkno, end_blkno, original_bp->b_dmaaddr,
		operation, slices_to_use,
		io_context->current_slices.slices_used,
		&io_context->current_slices.num_entries,
		addr_array);

	/*
	 * The buffers are currently singly linked via b_forw in
	 * per-plex lists.
	 * We now want to doubly link all of them into a single list 
	 * using b_fsprivate and b_fsprivate2 so that the upper xlv
	 * completion routine can determine when they have all
	 * completed.
	 */
	tlvb = bp_list;
	for (i=1; i < bufs_needed; i++) {
		nlvb = tlvb + 1;
		tlvb->b_fsprivate = nlvb;
		nlvb->b_fsprivate2 = tlvb;
		tlvb = nlvb;
	}
	bp_list->b_fsprivate2 = tlvb;
	tlvb->b_fsprivate = bp_list;

	/*
	 * Now issue the requests to the lower strategy routine.
	 * These requests are sorted by plexes since we use b_forw.
	 */

	for (plex=0; plex < XLV_MAX_PLEXES; plex++) {

                bp = addr_array[plex].buf_list;
		while (bp) {
			/*
			 * Note that the lower xlv strategy routine
			 * overwrites b_forw. But it's ok as we've
			 * copied it before we call xlv_lower_strategy.
			 * The lower xlv strategy routine also does 
			 * not follow b_forw.
			 */
                        next_bp = bp->b_forw;

#if 0
			xlv_trace("XLV_SPLIT_CALL_LOW_STRAT",
				  XLV_TRACE_TOP_BUF, bp);
#endif
			if (xlv_lower_strategy(bp, plex)) {
				/*
				 * bufs_needed is not zero.
				 * Did top_strat_format_bufs() correctly
				 * set the addr_array[]?  The ve's in the
				 * plex could have gone offline.
				 */
				ASSERT(0);
			}
			bp = next_bp;
		}
	}

	return 0;
} /* end of xlv_split_among_plexes() */


#if defined(INDUCE_IO_ERROR)
/* 
 * To generate an error, set insert_error to 1, and error_dev2 to the dev_t
 * This is mainly used for XFS filesystem error handling testing.
 */
int	xlv_insert_error = 0;	/* whether to enable fault insertion. */
dev_t	xlv_error_dev2 = 0;	/* device to fail */
int	xlv_error_freq = 20;	/* frequency at which errors are induced */

int	xlv_error_count = 0;	/* counter to determine which request to
				   fail (if xlv_insert_error is 1). */

int	xlv_errno = 0;		/* flag used by completion routine to
				   determine when to fail. 
				   This is a one-shot. */

dev_t	xlv_err_dev = 0;	/* device to fail */
#endif

int
xlvstrategy(register buf_t *bp)
{
	xlv_tab_subvol_t	*xlv_p;
	daddr_t			start_blkno, end_blkno;
	xlv_slice_t		slices_to_use[XLV_MAX_ROWS_PER_IO];
	unsigned		blocks;
	unsigned		minor_dev;
	int			error = 0;

	buftrace("XLV_STRAT", bp);

#if defined(INDUCE_IO_ERROR)	
	if (xlv_insert_error && 
	    bp->b_edev == xlv_error_dev2) {
		xlv_error_count++;
		if (xlv_error_count > xlv_error_freq) {
			xlv_error_count = 0;
			xlv_errno = 0x22;
		}
	}
#endif

	minor_dev = minor(bp->b_edev);
	if (minor_dev >= xlv_maxunits) {
		error = ENXIO;
		goto serror;
	}

	if (!(bp->b_flags & B_XLV_IOC)) {
	    bp->b_sort = 0;
	}
        if (!xlv_acquire_io_lock(minor_dev, bp)) {
		/* can't get the lock now: bp will be enqueued */
		return 0;
	}

	ASSERT(!(bp->b_flags & B_XLV_HBUF));
	bp->b_flags &= ~B_XLV_IOC;
	bp->b_flags |= B_XLV_HBUF;

#if 0
	xlv_lock_trace ("LOCK (strat)", XLV_TRACE_USER_BUF, bp);
#endif

	if (! xlv_tab) {
                error = ENXIO;
		goto serror;
        }

	xlv_p = &xlv_tab->subvolume[minor_dev];
	if (! XLV_SUBVOL_EXISTS(xlv_p))  {
		error = ENXIO;
		goto serror;
	}

	if (! XLV_ISOPEN(xlv_p->flags)) {
		error = ENXIO;
		goto serror;
	}

	if (xlv_p->flags & XLV_SUBVOL_OFFLINE) {
		error = EIO;
		goto serror;
	}

	/*
	 * Disable writes for read only volumes.
	 */
	if ( !(bp->b_flags & B_READ) &&
	     (xlv_p->vol_p->flags & XLV_VOL_READONLY)) {
		error = EROFS;
		goto serror;
	}

	/* 
	 * verify request is within volume.
	 */

	if ((start_blkno = bp->b_blkno) < 0) {
		error = EINVAL;
		goto serror;
	}

	if (start_blkno >= xlv_p->subvol_size) {
		if (start_blkno > xlv_p->subvol_size ||
		    !(bp->b_flags & B_READ)) {
			error = ENOSPC ;
		}
		else {
			error = 0;
		}
		goto serror;
	}

	blocks = bp->b_bcount >> BBSHIFT;
	ASSERT((bp->b_bcount % BBSIZE) == 0);
	if (bp->b_bcount > MAXTRANS) {
		error = EINVAL;
		goto serror;
	}

	/*
	 * If the request starts from a valid range but extends
	 * past, set b_resid to account for the bytes beyond the
	 * end.
	 */
	end_blkno = start_blkno + blocks - 1;
	if (end_blkno >= xlv_p->subvol_size) {
                bp->b_resid = ((end_blkno+1) -
                                xlv_p->subvol_size) << BBSHIFT;
        }
        else
                bp->b_resid = 0;

	bp->b_error = 0;
	bp->b_flags &= ~B_ERROR;	/* make sure we start clean... */

	if (xlv_plexing_support_present && (xlv_p->num_plexes > 1)) {
		if (error = xlv_split_among_plexes (xlv_p, bp, slices_to_use))
			goto serror;
	}
	else {
		CHECK_GRIO_TIMESTAMP( bp, 40 );
		if (error = xlv_lower_strategy(bp, 0))
			goto serror;
	}

	return 0;

serror:
	/*
	 * We only set B_XLV_HBUF after we've acquired the 
	 * xlv io lock above.
	 */
        if (bp->b_flags & B_XLV_HBUF) {
                bp->b_flags &= ~B_XLV_HBUF;
                xlv_release_io_lock(minor_dev);
		xlv_lock_trace ("UNLOCK (strat)", XLV_TRACE_USER_BUF, bp);
        }
        bp->b_error = error;
	if (error) {
        	bp->b_flags |= B_ERROR;
	}
	else {
        	bp->b_flags &= ~B_ERROR;
	}
        bp->b_resid = bp->b_bcount;

	buftrace("XLV_DONE_ERROR", bp);
        iodone(bp);
        return 0;

} /* end of xlvstrategy() */


/*******************************************************************
 *
 *            Lower XLV Strategy Support Routines.
 *
 *******************************************************************/

/*
 * Compute the number of buffers needed to complete the request for
 * a given plex.  This is called by the strategy routine.
 *
 * first_vol_elmnt will be set to the first volume element (within
 * this plex) that intersects the user request.  It may be NULL.
 */
static unsigned 
low_strat_count_bufs (
	xlv_tab_plex_t	*plex,
	struct buf	*bp, 	/* original request. */
	daddr_t		daddr,	/* starting blkno for transfer. */
	xlv_tab_vol_elmnt_t **first_vol_elmnt,	/* OUT */
				/* set to first vol elmnt within
				 * plex that contains disk parts
				 * that intersect the request.
				 */
	unsigned	*first_disk_part_of_first_vol_elmnt)
{
	daddr_t blocks_for_plex;    /* number of blocks from the request
				       that are in this plex's address space */
	daddr_t blocks_for_ve;
	daddr_t blocks_for_stripe_set;
	daddr_t last_block_in_plex = 0;
	daddr_t	initial_blocks;
	daddr_t daddr_in_ve;
	unsigned	ve, d;
	unsigned xlvbufs = 0;
	xlv_tab_vol_elmnt_t *vol_elmnt;
	unsigned first_disk_part;	/* within current vol elmnt */


	/* 
	 * Make sure that the transfer will fit within this plex's
	 * address space. 
	 */

	blocks_for_plex = bp->b_bcount >> BBSHIFT;

	/* 
	 * Scan backwards thru the valid ve's looking for the last valid
	 * block number.
	 */
	for (ve = plex->num_vol_elmnts, vol_elmnt = &plex->vol_elmnts[ve-1]; 
	     ve; ve--, vol_elmnt--) {
		/* skip if we can't read or write the ve */
		if (INVALID_VE(vol_elmnt))
			continue;
		last_block_in_plex = vol_elmnt->end_block_no;
		break;
	}
	if (last_block_in_plex == 0) {
		*first_vol_elmnt = NULL;
		return(0);	/* there are no valid ve's in the plex */
	}

	/* This check is only meaningful if there are no holes in the range.
	 * The block map should take care of making sure that no I/O
	 * gets issued to a ve which is incomplete
	 */
	if ((daddr + blocks_for_plex) > last_block_in_plex) {
        	blocks_for_plex = last_block_in_plex - daddr + 1;
        }

	/*
	 * We know now that the transfer will not extend beyond the end 
	 * of this plex.
	 */

	/* 
	 * Find the volume element in which the start of the requested
	 * transaction falls. 
	 */

	for (ve = 0, vol_elmnt = plex->vol_elmnts; 
	     ve < plex->num_vol_elmnts; 
	     ve++, vol_elmnt++) {
		/* skip if we can't read or write the ve */
		if (INVALID_VE(vol_elmnt))
			continue;
		if (vol_elmnt->start_block_no <= daddr &&
		    vol_elmnt->end_block_no >= daddr)
			break;
	}

	/* 
	 * Save the first volume element so that we don't need to
	 * search for it again.
	 */

	*first_vol_elmnt = vol_elmnt;

	/* 
	 * The request may extend beyond the end of the current
	 * volume element.  So we need to process the request in 
	 * chunks that fit the sizes of the available volume 
	 * elements.  We expect that the request will span no 
	 * more than 2 volume elements.  (Since each volume
	 * element may be striped, this means that each volume
	 * element may consume up to 2*stripe_unit_size buffers.)
	 * However, for completeness in the ridiculous but not
	 * actually illegal case of a logical volume including
	 * tiny partitions... 
	 */

	while (blocks_for_plex) {

		/* 
		 * blocks_for_ve = number of blocks from the request 
	         * that will be transferred to disks belonging to the
	         * current volume element. 
		 */

		blocks_for_ve = vol_elmnt->end_block_no - daddr + 1;
		if (blocks_for_ve > blocks_for_plex) 
	        	blocks_for_ve = blocks_for_plex;

		if (vol_elmnt->grp_size == 1) {
			xlvbufs++;
		}
		else if (vol_elmnt->type == XLV_VE_TYPE_CONCAT) {
			int		blocks_left_in_ve;
			unsigned	curchunk;
			daddr_t		start_blkno_of_next_disk_part;
			daddr_t		last_blkno_in_disk_part;


			/*
			 * The request may not start within the first
			 * disk part of this volume element.
			 */
			start_blkno_of_next_disk_part = vol_elmnt->start_block_no;
			for (d=0; d < vol_elmnt->grp_size; d++) {
				start_blkno_of_next_disk_part +=
					vol_elmnt->disk_parts[d].part_size;
				if (start_blkno_of_next_disk_part > daddr) {
					break;
				}
			}
			first_disk_part = d;

			/*
			 * Save off the first disk part of the first vol
			 * elmnt.  All the other (subsequent) volume elements
			 * should start off at disk_part 0.
			 */
			if (vol_elmnt == *first_vol_elmnt)
				*first_disk_part_of_first_vol_elmnt = 
					first_disk_part;
			last_blkno_in_disk_part = start_blkno_of_next_disk_part
				- 1;

			curchunk = last_blkno_in_disk_part - daddr + 1;
			if (curchunk > blocks_for_ve)
				curchunk = blocks_for_ve;


			blocks_left_in_ve = blocks_for_ve;
			for (d=first_disk_part; blocks_left_in_ve > 0; d++) {

				ASSERT (d < vol_elmnt->grp_size);

				xlvbufs++;
				blocks_left_in_ve -= curchunk;
				curchunk = vol_elmnt->disk_parts[d].part_size;
			}
		}
	        else {

			/* 
			 * Striping case. The request is to be divided 
			 * into multiple I/Os each of whose transfer 
			 * size is equal to the stripe width of the 
			 * current volume element.  
			 * Note that the first and last transfer
		   	 * may be less than the stripe width. 
			 */

			blocks_for_stripe_set = blocks_for_ve;

			/* 
			 * Get the starting blockno of current "chunk"
		   	 * relative to the start of this volume element. 
			 */

			daddr_in_ve = daddr - vol_elmnt->start_block_no;

			if (daddr_in_ve % vol_elmnt->stripe_unit_size) {

		    		/* 
				 * Transfer is not aligned on a stripe 
				 * boundary.
				 */
		    		initial_blocks = vol_elmnt->stripe_unit_size -
				    (daddr_in_ve % vol_elmnt->stripe_unit_size);

		    		/* 
				 * If the last block is also in this stripe. 
				 */

		    		if (initial_blocks > blocks_for_stripe_set) {
		        		initial_blocks = blocks_for_stripe_set;
		    		}

		    		blocks_for_stripe_set -= initial_blocks;
		    		xlvbufs++;
			} /* end if */

			/* 
			 * Some number of transfers equal to the stripe size.
			 */

			xlvbufs += (blocks_for_stripe_set / 
				    vol_elmnt->stripe_unit_size);

			/* 
			 * Is there a leftover at the end? 
			 */
			if (blocks_for_stripe_set % vol_elmnt->stripe_unit_size)
		    		xlvbufs++;
		    
		} /* end else */

		vol_elmnt++;
		blocks_for_plex -= blocks_for_ve;
		daddr += blocks_for_ve;

	} /* end while (blocks_for_plex) */

	return (xlvbufs);

} /* end of low_strat_count_bufs() */


/*
 * Format the I/O request buffers for a striped volume element.
 */
static void 
do_striped_ve (
	xlv_tab_vol_elmnt_t *vol_elmnt,
	buf_t      *bp,			/* original request buffer */
	caddr_t    caddr,
	daddr_t    daddr_in_ve,		/* starting blockno of transfer,
					   relative to start of this ve. */
	daddr_t    blocks_for_ve,	/* number of blocks from the
					   request that fall within this
					   ve's address space. */
	buf_t      **buf_list,		/* linked list of buffers
					   (IN/OUT) */
	xlv_stat_t *statp)		/* subvol statistics */
{
        xlv_tab_disk_part_t *disk_part;
	buf_t *tlvb, *nlvb;
        daddr_t stripe_unit_size;
        unsigned curchunk, striperotor;

	/*
	 * Initial adjustment: if daddr_in_ve is not an exact multiple 
	 * of the stripe-width, then there's a smaller first chunk.
	 * Subsequent chunksizes (except perhaps a short last one) are 
	 * all of stripe-width; this is set in the while loop below.  
	 */

	stripe_unit_size = vol_elmnt->stripe_unit_size;
	ASSERT(stripe_unit_size > 0);

	if (daddr_in_ve % stripe_unit_size)
		curchunk = stripe_unit_size - (daddr_in_ve % stripe_unit_size);
	else
		curchunk = stripe_unit_size;

	/* 
	 * Striperotor is the physical device within the volume element 
	 * that the starting block number of the transfer maps to.  
	 * Striperotor is an integer ranging from 0 to stripe_grp_size-1. 
	 * disk_part will point to the disk part data structure that 
	 * describes the device.
	 */

	striperotor = (daddr_in_ve / stripe_unit_size) % vol_elmnt->grp_size;
	disk_part = &vol_elmnt->disk_parts[striperotor];

	/*
	 * Gather some statistics.
	 */
	if (xlv_stats_enabled) {
		unsigned lastchunk;	/* size of last io; could be 0 */
		unsigned units;		/* count of involved stripe units */
		unsigned oddunits;	/* leftover from a full stripe width */

		if (blocks_for_ve <= curchunk) {
			/* fits in one stripe unit */
			lastchunk = blocks_for_ve;
			units = 1;
			oddunits = 1;
		} else {
			/* multiple stripe units */
			lastchunk = (blocks_for_ve-curchunk)%stripe_unit_size;
			units = ((blocks_for_ve-curchunk)/stripe_unit_size)+1;
			if (lastchunk) {
				units++;	/* for the last unit */
			}
			oddunits = units % vol_elmnt->grp_size;
		}

		statp->xs_stripe_io++;
		statp->xs_su_total += units;
		if (statp->xs_max_su_len == units) {
			statp->xs_max_su_cnt++;
		} else if (statp->xs_max_su_len < units) {
			statp->xs_max_su_len = units;
			statp->xs_max_su_cnt = 1;
		}

		if (curchunk == stripe_unit_size) {
			/* begins on a stripe unit boundary */
			if (lastchunk != 0 && lastchunk != stripe_unit_size) {
				/* not a full stripe unit io */
				statp->xs_align_part_su++;
			} else if (oddunits == 0) {
				/* multiple of a stripe width */
				ASSERT((units % vol_elmnt->grp_size) == 0);
				statp->xs_align_full_sw++;
			} else if (units < vol_elmnt->grp_size) {
				/* less than a full stripe width */
				statp->xs_align_less_sw++;
			} else {
				/* more than a full stripe width */
				statp->xs_align_more_sw++;
			}

		} else {
			/* unaligned, begins in the midst of a stripe unit */
			if (units == 1) {
				statp->xs_unalign_part_su++;
			} else if ((oddunits == 1)
				  && (curchunk+lastchunk == stripe_unit_size)) {
				statp->xs_unalign_full_sw++;
			} else if (lastchunk != 0) {
				/* does not end on a aligned boundary */
				statp->xs_unalign_part_su++;
			} else if (units > vol_elmnt->grp_size) {
				/* more than a stripe width */
				statp->xs_unalign_more_sw++;
			} else {
				/* less than a stripe width */
				statp->xs_unalign_less_sw++;
			}
		}
	} /* stop gathering statistics */


	tlvb = *buf_list;

	/* All the blocks to be allocated to this volume element
	   are to be distributed across the stripe set. */
	while (blocks_for_ve) {

		if (curchunk > blocks_for_ve)
			curchunk = blocks_for_ve;

		tlvb->b_flags = bp->b_flags;
	    	if ( BUF_IS_IOSPL( bp ) ) {
			ASSERT( BUF_IS_IOSPL(tlvb) );
			ASSERT( BUF_GRIO_PRIVATE(bp) );
			tlvb->b_grio_private = bp->b_grio_private;
	    	} else {
			ASSERT( BUF_GRIO_PRIVATE(bp) == 0);
	        }

		/* XXX KK */
		ASSERT(disk_part->active_path < disk_part->n_paths);
		tlvb->b_edev = disk_part->dev[disk_part->active_path];
		FO_DISK_PART_L(tlvb) = disk_part;

		/*
		 * daddr_in_ve / (stripe_grp_size * stripe_unit_size) 
		 * gives us the row within the entire stripe set that 
		 * the starting block number falls.
		 *
		 * we multiply it by stripe_unit_size to get the block 
		 * number within the single disk that the stripe falls.  
		 * Then we need to add the remainder to get to the proper 
		 * starting offset within that row.
	         */

                tlvb->b_blkno = ((daddr_in_ve / 
			(vol_elmnt->grp_size * stripe_unit_size)) * 
			stripe_unit_size) + (daddr_in_ve % stripe_unit_size);

	    	tlvb->b_bcount = curchunk << BBSHIFT;
	    	tlvb->b_error = 0;
	    	tlvb->b_iodone = xlv_low_strat_done;

	    	if (BP_ISMAPPED(bp)) {
			tlvb->b_flags &= ~(B_MAPPED | B_PAGEIO);
			tlvb->b_pages = NULL;
			tlvb->b_dmaaddr = caddr;
			caddr += tlvb->b_bcount;
	    	}
	    	else {
			tlvb->b_bufsize = bp->b_bufsize;
			tlvb->b_pages = bp->b_pages;
			tlvb->b_remain = 0;
			tlvb->b_dmaaddr = 0;
	    	}

		blocks_for_ve -= curchunk;
		daddr_in_ve += curchunk;

		nlvb = (tlvb + 1);
		tlvb->b_forw = nlvb;

	        /* 
		 * double link async buffers, see later note. 
		 */

		nlvb->b_back = tlvb;
		tlvb = nlvb;

		/* 
		 * advance to next disk in stripe set. 
		 */

		striperotor++;
		disk_part++;
		if (striperotor >= vol_elmnt->grp_size) {
			/* stripe boundary crossing */
	        	striperotor = 0;
	        	disk_part = &vol_elmnt->disk_parts[0];
	        }


		/* 
		 * So that we'd always be transferring the full 
		 * stripe_unit_size after the first stripe that may 
		 * be smaller. 
		 */

		curchunk = stripe_unit_size;

	}	/* end while (blocks_for_ve) */

	/* 
	 * Point buf_list to the last buffer that we've formatted. 
	 */

	*buf_list = tlvb;

} /* end of do_striped_ve() */


/*
 * Format the I/O request buffers for a concatenated volume element.
 */
static void 
do_concatenated_ve (
	xlv_tab_vol_elmnt_t	*vol_elmnt,
	unsigned		first_disk_part,/* first disk part within
						   volume element that the
						   request starts in. */
	buf_t			*bp,		/* original request buffer */
	caddr_t			caddr,
	daddr_t			daddr_in_ve,	/* starting blockno of transfer,
					   	   relative to start of this ve. */
	daddr_t			blocks_for_ve,	/* number of blocks from the
					   	   request that fall within this
					   	   ve's address space. */
	buf_t			**buf_list)	/* linked list of buffers
					   	   (IN/OUT) */
{
        xlv_tab_disk_part_t	*disk_part;
	buf_t			*tlvb, *nlvb;
        unsigned		curchunk, d;
	daddr_t			daddr_in_first_disk_part;
	
	ASSERT(first_disk_part < XLV_MAX_DISK_PARTS_PER_VE);

	tlvb = *buf_list;

	/*
	 * Compute the block offset relative to the start of the
	 * first disk part.
	 */
	daddr_in_first_disk_part = daddr_in_ve;
	for (d = 0; d < first_disk_part; d++) {
		daddr_in_first_disk_part -= vol_elmnt->disk_parts[d].part_size;
	}

	/*
	 * Now fill in the buffers.
	 */
	for (d = first_disk_part, disk_part = &vol_elmnt->disk_parts[d]; 
	     blocks_for_ve;
	     d++, disk_part++) {

		ASSERT (d < vol_elmnt->grp_size);

		tlvb->b_flags = bp->b_flags;
	    	if ( BUF_IS_IOSPL( bp ) ) {
			ASSERT( BUF_IS_IOSPL(tlvb) );
			ASSERT( BUF_GRIO_PRIVATE(bp) );
			tlvb->b_grio_private = bp->b_grio_private;
		} else {
			ASSERT( BUF_GRIO_PRIVATE(bp) == 0 );
			ASSERT( BUF_GRIO_PRIVATE(tlvb) == 0 );
		}

		/* XXX KK */
		ASSERT(disk_part->active_path < disk_part->n_paths);
		tlvb->b_edev = disk_part->dev[disk_part->active_path];
		FO_DISK_PART_L(tlvb) = disk_part;

		if (d == first_disk_part) 
			tlvb->b_blkno = daddr_in_first_disk_part;
		else
			tlvb->b_blkno = 0;

		curchunk = disk_part->part_size - tlvb->b_blkno;
		if (curchunk > blocks_for_ve)
			curchunk = blocks_for_ve;
	    	tlvb->b_bcount = curchunk << BBSHIFT;

	    	tlvb->b_error = 0;
	    	tlvb->b_iodone = xlv_low_strat_done;

	    	if (BP_ISMAPPED(bp)) {
			tlvb->b_flags &= ~(B_MAPPED | B_PAGEIO);
			tlvb->b_pages = NULL;
			tlvb->b_dmaaddr = caddr;
			caddr += tlvb->b_bcount;
	    	}
	    	else {
			tlvb->b_bufsize = bp->b_bufsize;
			tlvb->b_pages = bp->b_pages;
			tlvb->b_remain = 0;
			tlvb->b_dmaaddr = 0;
	    	}

		blocks_for_ve -= curchunk;

		nlvb = (tlvb + 1);
		tlvb->b_forw = nlvb;

	        /* 
		 * double link async buffers, see later note. 
		 */

		nlvb->b_back = tlvb;
		tlvb = nlvb;

	}	/* end while (blocks_for_ve) */

	/* 
	 * Point buf_list to the last buffer that we've formatted. 
	 */

	*buf_list = tlvb;

} /* end of do_concatenated_ve() */


/*
 * Fill in all the I/O request buffers for the underlying disk drivers
 * in a given plex. This is internal to the strategy routine.
 */
static void 
format_bufs (xlv_tab_plex_t	*plex,		/* current plex */
	     buf_t		*bp,		/* original I/O request */
	     xlv_tab_vol_elmnt_t *first_vol_elmnt,
	     unsigned		first_disk_part_in_first_ve,
	     buf_t		**buf_list,	/* list of buffers that
						 * we've created; doubly
						 * linked.
						 */
	     xlv_stat_t		*statp)		/* sv statistic pointer */
{
        daddr_t blocks_for_plex;   /* number of blocks from the request
                                      that are in this plex's address space */
	daddr_t blocks_for_ve;
        daddr_t last_block_in_plex = 0;
        xlv_tab_vol_elmnt_t *vol_elmnt;
	buf_t *tlvb, *nlvb;
	daddr_t daddr;
	daddr_t	daddr_in_ve;
	caddr_t caddr;
	unsigned ve, bytes_for_ve;

        daddr = bp->b_blkno;
        caddr = bp->b_dmaaddr;

        blocks_for_plex = bp->b_bcount >> BBSHIFT;

	/* 
	 * Scan backwards thru the valid ve's looking for the last valid
	 * block number.
	 */
	for (ve = plex->num_vol_elmnts, vol_elmnt = &plex->vol_elmnts[ve-1]; 
	     ve; ve--, vol_elmnt--) {
		/* skip if we can't read or write the ve */
		if (INVALID_VE(vol_elmnt))
			continue;
		last_block_in_plex = vol_elmnt->end_block_no;
		break;
	}
	if (last_block_in_plex == 0) {
		/*
		 * There are no valid ve in this plex!
		 */
		return;
	}

	/* This check is only meaningful if there are no holes in the range.
	 * The block map should take care of making sure that no I/O
	 * gets issued to a ve which is incomplete
	 */
        if ((daddr + blocks_for_plex) > last_block_in_plex) {
        	blocks_for_plex = last_block_in_plex - daddr + 1;
        }

	/* 
	 * First buffer that we can use.  It is assumed that these
	 * buffers are allocated from the xlvm structure. 
	 */

	tlvb = *buf_list;

	/* 
	 * Step through all the volume elements involved in this
	 * transfer until all of blocks_for_plex have been allocated. 
	 */

	for (vol_elmnt = first_vol_elmnt;
	     blocks_for_plex;
	     vol_elmnt++, blocks_for_plex -= blocks_for_ve,
			  daddr += blocks_for_ve,
			  caddr += bytes_for_ve) {

		/* 
		 * daddr_in_ve is the starting block of the portion
		 * of the transfer that falls within the current 
		 * volume element.  Unlike daddr, which is relative
		 * to the start of the logical volume, daddr_in_ve
		 * is relative to the start of the volume element. 
		 */

		daddr_in_ve = daddr - vol_elmnt->start_block_no;

		/* 
		 * blocks_for_ve is the number of blocks from the
		 * request that fall within this volume element. 
		 */

		blocks_for_ve = vol_elmnt->end_block_no - daddr + 1;
	        if (blocks_for_ve > blocks_for_plex) {
	        	blocks_for_ve = blocks_for_plex;
	        }
		bytes_for_ve = blocks_for_ve << BBSHIFT;

		ASSERT(vol_elmnt->grp_size > 0);


		if (vol_elmnt->grp_size == 1) {
		    xlv_tab_disk_part_t *disk_part = vol_elmnt->disk_parts;

		    /* 
		     * Non-striping case.
		     */

            	    tlvb->b_bcount = bytes_for_ve;

		    /* 
		     * In the non-striping case, there is only a
		     * single disk part in the volume element. 
		     */
		    /* XXX KK */
		    ASSERT(disk_part->active_path < disk_part->n_paths);
                    tlvb->b_edev = disk_part->dev[disk_part->active_path];
		    FO_DISK_PART_L(tlvb) = disk_part;

		    tlvb->b_flags = bp->b_flags;

		    if ( BUF_IS_IOSPL( bp ) ) {
			ASSERT( BUF_IS_IOSPL(tlvb) );
			ASSERT( BUF_GRIO_PRIVATE(bp) );
			tlvb->b_grio_private = bp->b_grio_private;
		    } else {
			ASSERT( BUF_GRIO_PRIVATE(bp) == 0);
			ASSERT( BUF_GRIO_PRIVATE(tlvb) == 0);
		    }

                    tlvb->b_blkno = daddr_in_ve;
                    tlvb->b_error = 0;

		    /* 
		     * Call the low-level completion routine when I/O is done. 
		     */

		    tlvb->b_iodone = xlv_low_strat_done;

	       	    /* 
		     * If the buf is not mapped, is a pageio buf and
		     * there is only one lvbuf, lower level drivers may
		     * map the lvbuf. b_bufsize must be propagated
		     * to the lvbuf to allow this, since bp_mapin
		     * needs it. Also in that case we need to unmap it
		     * when we're done with it.
		     * How to determine whether a bp_mapout is needed
		     * in xlv_low_strat_done?
		     * If the B_MAPPED bit from a mapped
		     * upper-level buf is copied into the lvbuf, there
		     * is no way to tell if that lvbuf has has a
		     * bp_mapin done on it by the lower-level driver.
		     * So, if the upper-level buf is mapped, we should
		     * NOT propagate the B_MAPPED bit to the lvbufs.
		     * However, just turning off the B_MAPPED bit would
		     * fool lower-level drivers into doing a spurious
		     * bp_mapin on pageio bufs. Thus we must turn the
		     * B_PAGEIO bit off too.
		     */

		    if (BP_ISMAPPED(bp)) {
			    tlvb->b_flags &= ~(B_MAPPED | B_PAGEIO);
			    tlvb->b_pages = NULL;
			    tlvb->b_dmaaddr = caddr;
		    } 
		    else {
			    tlvb->b_bufsize = bp->b_bufsize;
			    tlvb->b_pages = bp->b_pages;
			    tlvb->b_dmaaddr = NULL;
		    }

		    /* 
		     * Note that we assume buffers are allocated
		     * sequentially from the xlvm structure. 
		     */

		    nlvb = (tlvb + 1);
		    tlvb->b_forw = nlvb;

		    /* double link async buffers, see later note. */

		    nlvb->b_back = tlvb;
		    tlvb = nlvb;


	        }	/* if vol_elmnt->grp_size == 1 */

		else if (vol_elmnt->type == XLV_VE_TYPE_CONCAT) {

		    do_concatenated_ve (vol_elmnt, first_disk_part_in_first_ve,
			bp, caddr, daddr_in_ve, blocks_for_ve, &tlvb);

		    /*
		     * If there are multiple volume elements, all the
		     * subsequent volume elements should start off at 
		     * the first disk part.
		     *
		     * Given reasonable sized disk partitions, this should
		     * only occur if the request spans 2 partitions; the
		     * first partition is the last disk part of one
		     * volume element and the second partition is the 
		     * first disk part of the next volume element.
		     */
		    first_disk_part_in_first_ve = 0;
		}
	        else {

		    /* 
		     * Striping case 
		     */

		    do_striped_ve (vol_elmnt, bp, caddr, daddr_in_ve,
			blocks_for_ve, &tlvb, statp);

	        } /* endif striping case */

	} /* end for */

	/* 
	 * Return the address of the last buffer this routine
	 * used so that the caller can link it with any buffers
	 * allocated for other plexes. 
	 */

	*buf_list = tlvb;

} /* end of format_bufs() */


/*
 * Lower strategy routine.
 * 
 * Perform the given I/O operation on the specified set of plexes. 
 * The range of disk blocks specified by the bp is guaranteed to not have
 * holes and be available from the specified plexes.
 */
int
xlv_lower_strategy (struct buf 	*bp,	
		    unsigned	plex_num)
{
        xlv_tab_subvol_t        *xlv_p;
        xlv_tab_plex_t          *plex;
        xlv_tab_vol_elmnt_t     *first_vol_elmnt;
	xlv_stat_t		*statp;
        unsigned                xlvbufs;
        unsigned                i;
        struct buf              *lvb, *tlvb, *nlvb, *buf_list;
        daddr_t                 daddr;
        unsigned                blocks;
        unsigned                minor_dev;
        xlvmem_t		*lvm;
	unsigned		first_disk_part_in_first_ve = 0;
#ifdef EVEREST
	int			syncme;		/* true when ia work-around tripped */
#endif

	ASSERT (IS_MASTERBUF(bp));

	CHECK_GRIO_TIMESTAMP( bp, 40 );

        minor_dev = minor(bp->b_edev);
	ASSERT (minor_dev < xlv_maxunits);

        xlv_p = &xlv_tab->subvolume[minor_dev];
	statp = xlv_io_lock[minor_dev].statp;
	ASSERT (XLV_SUBVOL_EXISTS(xlv_p) && XLV_ISOPEN(xlv_p->flags));

	daddr = bp->b_blkno;
        ASSERT ((daddr >= 0) && (daddr < xlv_p->subvol_size)); 
        ASSERT ((bp->b_bcount % BBSIZE == 0) && (bp->b_bcount <= MAXTRANS));

	ASSERT (bp->b_error == 0);
	ASSERT (!(bp->b_flags & B_ERROR));

	if (xlv_stats_enabled) {
		if (bp->b_flags & B_READ) {
			statp->xs_ops_read++;
			statp->xs_io_rblocks += (bp->b_bcount >> BBSHIFT);
		} else {
			statp->xs_ops_write++;
			statp->xs_io_wblocks += (bp->b_bcount >> BBSHIFT);
		}
	}


	/*
	 * Now calculate how many buffers will be needed for the transactions
	 * on the real devices. It is more efficient to work this out & 
	 * get the required memory all at once rather than doing lots of
	 * kmem_allocs (and later frees)...
	 */

	plex = xlv_p->plex[plex_num];
	xlvbufs = low_strat_count_bufs (plex, bp, daddr, &first_vol_elmnt,
			&first_disk_part_in_first_ve);

	if (!xlvbufs || !first_vol_elmnt) {
		/*
		 * For some reason this plex cannot do the i/o.
		 * Could be that there are not valid ve's in here.
		 *
		 * Note: Use same error as in xlv_split_among_plexes(). 
		 */
		return(ENOSPC);
	}

	/*
	 * Now we know how many buffers are needed. Get memory for these;
	 * the memory is viewed as a struct xlvmem containing an
	 * array of bufs... 
	 *
	 * Note that we get one more buffer than we need so that the
	 * linked list operations in the inner-loop would not need
	 * to worry about end-of-list - they can always link to the
	 * next element.
	 */

	ASSERT(xlv_low_mem.size >= sizeof(xlvmem_t) + (xlvbufs+1)*sizeof(buf_t));
	lvm = xlv_res_mem_alloc(&xlv_low_mem, NULL,
				sizeof(xlvmem_t) + (xlvbufs+1)*sizeof(buf_t));
	lvm->bufs = xlvbufs+1;
	lvb = lvm->buf;

	/* 
	 * If more than one buffer is needed, i.e., the driver has
	 * allocated buffers, make sure that the user's original
	 * buffer is mapped to kernel virtual memory.  All the 
	 * buffers we've allocated are. 
	 */
	if ((xlvbufs > 1) && (!(bp->b_flags & B_MAPPED))) { 
		(void)bp_mapin(bp);
		lvm->mapped = 1;
	} else {
		lvm->mapped = 0;
	}

	/*
	 * We now have the necessary buffers. Next, we fill them in with the
	 * parameters for the transactions on the physical devices.
	 * Each of the bufs will be linked via b_forw and b_back.
	 */

	buf_list = lvm->buf;

	plex = xlv_p->plex[plex_num];
	format_bufs (plex, bp, first_vol_elmnt, first_disk_part_in_first_ve,
		     &buf_list, statp);

	if (buf_list == &lvm->buf[0]) {
		/*
		 * No buffers were formatted so something must have
		 * happened to the ve's since calculating the number
		 * of required buffers.
		 */
		xlv_res_mem_free(&xlv_low_mem, lvm);
		return(ENOSPC);
	}

	/*
	 *  We have the buffers filled with the info for the physical 
	 *  device transactions.
	 *
	 *  We then link all the bufs into a circle starting
	 *  & ending with the ORIGINAL buf, and fire them off.
	 *  iodone calls xlv_low_done (via b_iodone) with the buffer address. 
	 *  xlv_low_done unlinks the lvbuf from the circle, and if it was
	 *  the last one in the transaction, completes the original buffer.
	 *  To allow easy unlinking, they have to be double-linked in the 
	 *  async case.
	 *  Also, to allow us to free the dynamically alloc'd lvbufs, we need
	 *  to keep a note of the alloc'd memory. (The buf av_forw pointer may
	 *  not still point to the first buf once everything is complete,
	 *  depending on the order of completions)! Somewhat arbitrarily,
	 *  we use the b_sort field, since we know this is not used by
	 *  any higher level.
	 */

	/*  Note that the last buffer in the list is a dummy buffer,
	 *  used only to make the inner-loop code that links buffers
	 *  easier.  So we need to back up one element.
	 */

	/* 
	 * Make sure that we are currently pointing to the dummy
	 * buffer at the end. 
	 */

	ASSERT (buf_list == &lvm->buf[xlvbufs]);

	bp->av_forw = lvb;
	bp->av_back = buf_list->b_back;  
	buf_list->b_back->b_forw = bp;
	lvb->b_back = bp;
	bp->b_sort = (__psunsigned_t)lvm;

        /* 
	 * Account for any untransferred bytes 
	 */

        blocks = bp->b_bcount >> BBSHIFT;

	daddr = bp->b_blkno;
        if ((daddr + blocks) > xlv_p->subvol_size) {
                bp->b_resid = ((daddr + blocks) - 
			        xlv_p->subvol_size) << BBSHIFT;
        }
        else
		bp->b_resid = 0;

	/*
	 * Send the buffers to their appropriate drivers. 
	 * Note that the strategy call may cause an iodone & thus an
	 * xlv_low_strat_done arbitrarily quickly and these may break in
	 * at interrupt level. Thus, we MUST save the forward pointer
	 * before the strategy call, since the strategy call may cause
	 * it to change (in the async case)! 
	 */

	tlvb = lvb;
	CHECK_GRIO_TIMESTAMP( bp, 40 );
#ifdef EVEREST
	/*
	 * 11/16/95  Bug 323277 - Challenge IO4 IA chip bug workaround.
	 * Check for cache line alignment. There is a bug where
	 * two different IO4 may not write to the same cache line.
	 */
	blocks = (paddr(lvb) & 0x7F) && (lvb->b_flags & B_READ) && io4ia_war;
#endif
	for (i = 0; i < xlvbufs; i++) {
		nlvb = tlvb->b_forw;
#ifdef EVEREST
		/*
		 * If not last buffer and not cache aligned, look ahead
		 * to the next io to see if it is from another IO4.
		 *
		 * Note: Check for end of buffer chain could have
		 * been (nlvb != bp) instead of (i < (xlvbufs - 1))
		 */
		syncme = ((i < (xlvbufs - 1)) && blocks &&
		    possibly_diff_io4(nlvb->b_edev, tlvb->b_edev));
		if (syncme) {
			initnsema(&tlvb->b_iodonesema, 0, "xlv_lower_strategy");
			tlvb->b_flags |= B_XLV_ACK;
		}
#endif
#if 0
		xlv_trace ("XLV_LOW_STRAT_ISSUE", XLV_TRACE_LOW_BUF, tlvb);
#endif

#if defined(SIM) || defined(USER_MODE)
		bdstrat(major(tlvb->b_edev), tlvb);
#else
		griostrategy( tlvb );
#endif
#ifdef EVEREST
		if (syncme) {
			/*
			 * Next io is from another IO4. Don't start
			 * the next io until this last io is done.
			 */
			psema(&tlvb->b_iodonesema, PRIBIO);
			freesema(&tlvb->b_iodonesema);
			/*
			 * Since the buffer will be freed and re-init'ed
			 * on re-used, we don't need to turn off the
			 * B_XLV_ACK bit. If "syncme" is false, the buffer
			 * could already be recycled, so cannot just look at
			 * the tlvb->b_flags bit.
			 */
		}
#endif
		tlvb = nlvb;
	}

	return(0);

} /* end of xlv_lower_strategy() */



/*
 *  read from logical volume (character device).
 *  To limit size of individual transactions, we loop with multiple
 *  physio calls if necessary.
 */

int
xlvread(register dev_t dev, uio_t *uio)
{
	unsigned thistrans;
	unsigned resid = uio->uio_resid;
	int error;

	if (minor(dev) >= xlv_maxunits) 
		return ENXIO;

	/* NOTE: We defer our usual sanity checks on the xlv
	   device to the strategy() routine so that we don't 
	   need to acquire the xlv_tab lock here. */

	/* Additional sanity checks: we don't allow non-sector-multiple 
	   read sizes, and some underlying controllers won't transfer 
	   to odd addresses. */

	if (((long)uio->uio_iov->iov_base & 1) || (resid % NBPSCTR)) 
		return EINVAL;

	while (resid)
	{
		thistrans = (resid > MAXTRANS) ? MAXTRANS : resid;
		uio->uio_resid = thistrans;
		error = uiophysio(xlvstrategy, NULL, dev, B_READ, uio);

                if (error) {
                        uio->uio_resid = resid;
                        break;
                }

                /* The strategy() routine will transfer whatever fits
                   into logical volume, and record the untransferred
		   count in bp->b_resid.  This value is then copied
		   to uio->uio_resid by uiophysio(). */
                if (uio->uio_resid != 0)
                        break;

                resid -= thistrans;
	}
	return error;

} /* end of xlvread() */


/*
 * write to logical volume (character device).
 */

int
xlvwrite(register dev_t dev, uio_t *uio)
{
	unsigned thistrans;
	unsigned resid = uio->uio_resid;
	int error;

        if (minor(dev) >= xlv_maxunits)
                return ENXIO;

        /* NOTE: We defer our usual sanity checks on the xlv
           device to the strategy() routine so that we don't
           need to acquire the xlv_tab lock here. */

        /* Additional sanity checks: we don't allow non-sector-multiple
           read sizes, and some underlying controllers won't transfer
           to odd addresses. */

	if ((long)uio->uio_iov->iov_base & 1)
		return EINVAL;
	if (resid % NBPSCTR)
		return EINVAL;

	while(resid)
	{
		thistrans = (resid > MAXTRANS) ? MAXTRANS : resid;
		uio->uio_resid = thistrans;
		error = uiophysio(xlvstrategy, NULL, dev, B_WRITE, uio);

		if (error) {
			uio->uio_resid = resid;
			break;
		}

                /* The strategy() routine will transfer whatever fits
                   into logical volume, and record the untransferred
                   count in bp->b_resid.  This value is then copied
                   to uio->uio_resid by uiophysio(). */
		if (uio->uio_resid != 0)
			break;

		resid -= thistrans;
	}

	return error;

} /* end of xlvwrite() */


/* Unload device driver.
   This should return EBUSY if device is in use.
   Otherwise, free all resources and return 0.
   We should release all the locks, etc.
   Just return 0 for now. */
int xlvunload (void)
{
/*
	int i;

	for (i=0; i < xlv_maxlocks; i++) {
		mrfree (&(xlv_io_lock[i].lock));
		mrfree (&(xlv_config_lock[i].lock));
	}
*/
	return (0);
}


/*******************************************************************
 *
 *            Upper and Lower Completion Routines
 *
 *******************************************************************/

/*
 * xlv_top_strat_done(): completion routine for the upper xlv strategy
 *			 routine.
 */

static void
xlv_top_strat_done (buf_t	*lvb)
{
	xlv_io_context_t	*io_context;
	buf_t			*tlvb, *bp;
	struct xlvmem		*lvm;
	int			s, mapped;
	unsigned		lvm_bufs;

	s = mutex_spinlock(&xlv_top_strat_lock);

	ASSERT(XLV_IO_LOCKED_OR_RECOVERING(minor(lvb->b_edev)));
	ASSERT(emajor(lvb->b_edev) == XLV_LOWER_MAJOR);

	/*
	 * If the lower XLV driver mapped this buffer in. Map it out.
	 */
	if (lvb->b_flags & B_MAPPED)
                bp_mapout(lvb);

	if (lvb->b_flags & B_ERROR) {
		bp = lvb->b_private;
		io_context = (xlv_io_context_t *) bp->b_sort;

		/*
		 * Record the error in the retry buf or the original
		 * buf, depending on what we've got.
		 */
		bp = io_context->retry_buf.b_edev == 0 ?
			io_context->original_buf : &io_context->retry_buf;

		bp->b_flags |= B_ERROR;
		bp->b_error = lvb->b_error;
	}


	if ( BUF_IS_IOSPL(lvb) ) {
		ASSERT( BUF_GRIO_PRIVATE(lvb) );
		lvb->b_grio_private = NULL;
	}


	if (((buf_t *)lvb->b_fsprivate) == lvb) {
                /*
                 * This is the last lvbuf in the upper strategy transaction.
                 */
		xlv_trace ("XLV_TOP_STRAT_DONE_LAST", XLV_TRACE_TOP_BUF, lvb);

                bp = lvb->b_private;

                /*
                 * Get the io_context.
		 */
		io_context = (xlv_io_context_t *) bp->b_sort;
		bp = io_context->original_buf;

		lvm = (struct xlvmem *) io_context->buffers;
		lvm_bufs = lvm->bufs;	/* save this for later check. */
		mapped   = lvm->mapped;

		xlv_res_mem_free(&xlv_top_mem, lvm);

		mutex_spinunlock(&xlv_top_strat_lock, s);

		if (io_context->num_failures == 0 &&
		    ((io_context->state == XLV_SM_READ) ||
		     (io_context->state == XLV_SM_WRITE))) {
			/*
			 * If the I/O completed with no errors,
			 * release the io_context, unlock XLV_IO_LOCK,
			 * and complete the original buffer.
			 */
			bp->b_sort = 0;
			xlv_res_mem_free(&xlv_ioctx_mem, io_context);
			xlv_release_io_lock(minor(bp->b_edev));

			bp->b_flags &= ~B_XLV_HBUF;

			xlv_lock_trace ("UNLOCK (top_strat)",
                                        XLV_TRACE_USER_BUF, bp);

			/*
			 * Do a bp_mapout() on the original buffer if we
			 * called bp_mapin() on this buffer. Skip the call
			 * if the buffer's offset field is not page aligned.
			 * In this case, we leave it to the buffer cache
			 * to remove the mapping. This will change when we
			 * come up with a better scheme for handling non
			 * page-aligned buffers.
			 *
			 * xlvd will free the mapping if we had an error
			 * or if this were a retry buffer.
			 */
			if ((lvm_bufs > 1) && (mapped))
				bp_mapout(bp);

			xlv_trace("XLV_TOP_STRAT_IODONE",XLV_TRACE_USER_BUF,bp);

			buftrace("XLV_DONE", bp);

			iodone(bp);
		}
		else {
			/*
			 * We've either got an error or this is the
			 * normal completion of a retry.
			 * In either case, queue the original_bp for
			 * the xlvd to process further. 
			 * In the case of errors, the list of
			 * failures were recorded in the io context
			 * by the lower xlv driver.
			 *
			 * Note that we've already freed up any
			 * buffers we've allocated (except for the
			 * the io context) and we still have the
			 * XLV_IO_LOCK.
			 */

			xlv_trace ("XLV_TOP_STRAT_QUEUE",XLV_TRACE_USER_BUF,bp);
			xlvd_queue_request (bp, io_context);

		}
        }
        else {

		xlv_trace ("XLV_TOP_STRAT_DONE",
			XLV_TRACE_TOP_BUF, lvb);

                /*
                 * Unlink this lvbuf from the chain.
		 * fsprivate = forw, fsprivate2 = back
		 */
                tlvb = lvb->b_fsprivate;
		tlvb->b_fsprivate2 = lvb->b_fsprivate2;
                tlvb = lvb->b_fsprivate2;
		tlvb->b_fsprivate = lvb->b_fsprivate;

		/*
		 * Note that we don't free tlvb here. We will
		 * free all the buffers when we complete the
		 * last buffer in the list.
		 */

		mutex_spinunlock(&xlv_top_strat_lock, s);

	}

} /* end of xlv_top_strat_done() */


/*
 * xlv_low_strat_done(): completion routine for the lower xlv strategy
 * 			 routine.
 *
 *  This is called with a buf from iodone via b_iodone.
 *
 *  This is the equivalent of the "interrupt routine" for this driver.
 *
 *  If the xlvbuf is mapped, indicating that it was bp_mapin'd by a lower
 *  level driver, we must release the mapped K2seg memory.
 *
 *  If error, we have to find the real buf to record it. Otherwise we just
 *  unlink the returned xlvbuf, and if it was the last one, complete the real 
 *  buffer. If the subvolume were plexed or has holes, then the "real"
 *  buffer is likely also an xlv buffer kmem_alloc'ed by the upper
 *  xlv strategy routine. Completing that buffer would cause 
 *  xlv_top_strat_done() to be called.
 *
 *  NOTE that we can call kern_free here because kmem_free is guaranteed to
 *  not sleep.
 *
 *  On MP systems, more than one processor could execute this simultaneously.
 *  So we must protect against list pointer corruption with a spinlock.
 */

void
xlv_low_strat_done(register struct buf *lvb)
{
	register struct buf *tlvb;
	register struct buf *bp;
	register xlvmem_t *lvm;
	register int s;

	s = mutex_spinlock(&xlv_low_strat_lock);

#if 0
	xlv_trace ("XLV_LOW_STRAT_DONE", XLV_TRACE_LOW_BUF, lvb);
#endif

#ifdef EVEREST
	if (lvb->b_flags & B_XLV_ACK) {
		ASSERT(!(lvb->b_flags & B_MAPPED));
		vsema(&lvb->b_iodonesema);
	}
#endif

	if (lvb->b_flags & B_MAPPED)   /* actually a redundant test since
					* bp_mapout also tests this, but it 
					* saves a function call in most
					* cases. */
		bp_mapout(lvb);

#if defined(INDUCE_IO_ERROR)

	if (lvb->b_edev == xlv_err_dev) {
		xlv_errno = 23;
	}

	if (xlv_errno) {
		/*
		 * Simulate an error.
		 */
		lvb->b_flags |= B_ERROR;
		lvb->b_error = xlv_errno;

		/*
		 * Turn off subsequent errors so that retries would succeed.
		 */
		xlv_errno = 0;
	}
#endif

	if (lvb->b_flags & B_ERROR)
	{

#if 0
		xlv_trace ("XLV_LOW_STRAT_DONE_ERROR", XLV_TRACE_LOW_BUF, lvb);
#endif

FO_DEBUG(printf("ls_error(%x): %x, bl: %x/%x, fl: %x/%x; dp: %x, %d/%d.\n", lvb,
		lvb->b_edev, lvb->b_blkno, lvb->b_bcount, lvb->b_flags,
		lvb->b_flags, FO_DISK_PART(lvb),
		FO_DISK_PART(lvb)->active_path,
		FO_DISK_PART(lvb)->retry_path););

		mutex_spinunlock(&xlv_low_strat_lock, s);

		/* if possible, hand off to xlvd for retry */
		if (xlvd_queue_failover(lvb)) {
		        /* successfully queued -- nothing more to do now */
			return;
		}

		s = mutex_spinlock(&xlv_low_strat_lock);

		/* exhausted all paths -- pass error up to top_strat routine */

		/* step along chain to find the real buf. */
		for (bp = lvb->b_forw; !IS_MASTERBUF(bp); bp = bp->b_forw) ;

	/* handle the error in the returned lvbuf. See error notes in
	   strategy above */

		ASSERT(XLV_IO_LOCKED_OR_RECOVERING(minor(bp->b_edev)));

		bp->b_flags |= B_ERROR;
		if (!bp->b_error) bp->b_error = lvb->b_error;
		bp->b_resid = bp->b_bcount;

		/*
		 * If we are plexed, then record the failed dev_t in
		 * the io_context so we know which operations to retry.
		 * 
		 * Just recording the device number is sufficient as 
		 * each disk partition has a different dev_t and we
		 * only retry on a volume element basis.
		 *
		 * In the future, we can fine-tune this by recording
		 * the actual range of block numbers that failed.
		 * Since this would be the block number relative to
		 * the physical device that failed, we would need to
		 * map it back to the logical block numbers corresponding
		 * to the logical volume before we can retry.
		 */
		if (emajor(bp->b_edev) == XLV_LOWER_MAJOR) {

			buf_t			*original_bp;
			xlv_io_context_t	*io_context;
			int			i;

			/*
			 * bp currently points to a buffer issued by
			 * the upper xlv driver. We need to follow it
			 * to find the original buffer's io_context.
			 */
			original_bp = (buf_t *) bp->b_private;
			io_context = (xlv_io_context_t *) original_bp->b_sort;

			/*
			 * Record the errno.
			 */
			bp->b_flags |= B_ERROR;
			if (!bp->b_error) bp->b_error = lvb->b_error;

			/*
			 * Make sure that we have not already reported
			 * an error on this device.  This can happen 
			 * if we are striping across several disks and
			 * wrapped around to the next stripe unit on
			 * the same disk.
			 */
			for (i=0; i < io_context->num_failures; i++) {
				if (io_context->failures[i].dev ==
				    lvb->b_edev) {
					goto done;
				}
			}

			/*
			 * Record the error.
			 */
			bp->b_flags |= B_ERROR;
			if (!bp->b_error) bp->b_error = lvb->b_error;

			/*
			 * Record the failed dev_t.
			 */
			if (io_context->num_failures < XLV_MAX_FAILURES) {
				io_context->failures[io_context->num_failures].
					dev = lvb->b_edev;
				io_context->num_failures++;
			}
			/* else { XXX don't record failed dev_t? } */
done:
			;
			
		} else if (xlv_failsafe) {
			/*
			 * On failure, Failsafe wants non-mirror ve
			 * to be marked offline so it can fail over
			 * to the peer system.
			 */
			xlv_tab_vol_elmnt_t	*vep;
			xlv_tab_subvol_t	*svp;

			ASSERT(emajor(bp->b_edev) == XLV_MAJOR);

			svp = &(xlv_tab->subvolume[minor(bp->b_edev)]);
			vep = xlv_vol_elmnt_from_disk_part(svp, lvb->b_edev);

			vep->state = XLV_VE_STATE_OFFLINE;
		}

	}
	else if (XLVD_FAILING_OVER(lvb)) {
			extern bp_queue_t	  xlvd_work_queue;
			buf_t			 *dbp;
#ifdef FO_STATS
			extern int n_succs;
			n_succs++;
#endif /* FO_STATS */
			/* we failed over (successfully) -- finish up */
			FO_DEBUG(printf("low_done(%x): successful failover!\n",
					lvb););

			printf("XLV: successfully failed over to device: %d\n",
			       minor(lvb->b_edev));

			/* the next two actions should be unnecessary */
			FO_SET_STATE(lvb, FO_NORMAL);
			XLVD_UNSET_FAILING_OVER(lvb);

			dbp = fo_get_deferred(FO_DISK_PART(lvb));
			if (dbp) {
				XLVD_SET_FAILING_OVER(dbp);
				FO_SET_STATE(dbp, FO_ISSUE_DEFERRED);
				bp_enqueue(dbp, &xlvd_work_queue);
			}
	}

	if ( BUF_IS_IOSPL(lvb) ) {
		ASSERT( BUF_GRIO_PRIVATE(lvb) );
		lvb->b_grio_private = NULL;
	} else {
		ASSERT( BUF_GRIO_PRIVATE(lvb)  == 0);
	}

	if (lvb->b_forw == lvb->b_back)
	{
		/*
		 * This is the last lvbuf in the lower strategy transaction. 
		 * Since there are no other outstanding completions for
		 * this transaction, we can release the spinlock
		 * immediately.
		 *
		 * Note that when if there are plexes or holes, there
		 * may be multiple lower transactions for a single higher
		 * level strategy call.
		 */
		mutex_spinunlock(&xlv_low_strat_lock, s);

		bp = lvb->b_forw;

		/*
		 * If this is the head buffer, then we can release
		 * the xlv io lock and do the bp_mapout().
		 */
	        lvm = (xlvmem_t *)bp->b_sort;

		if (bp->b_flags & B_XLV_HBUF) {
			bp->b_flags &= ~B_XLV_HBUF;
			xlv_release_io_lock(minor(bp->b_edev));

#if 0
			xlv_lock_trace ("UNLOCK (low_strat)",
                                        XLV_TRACE_TOP_BUF, bp);
#endif

			if ((lvm->bufs > 1) && (lvm->mapped))
				bp_mapout(bp);
		}
		else {
			ASSERT (emajor(bp->b_edev) == XLV_LOWER_MAJOR);
		}

		xlv_res_mem_free(&xlv_low_mem, lvm);

		buftrace("XLV(low)_DONE", bp);
		iodone(bp);
	}
	else
	{
		/*
		 * Unlink this lvbuf from the chain. Because the lvbufs 
		 * link thru b_forw & b_back (which are guaranteed not to 
		 * be modified by the underlying drivers) and the real
		 * buf is connected via av_forw and av_back (so as not to 
		 * break the hash chains): we have to handle the corner 
		 * case of the lvbuf being adjacent to the real buf. 
		 */

		tlvb = lvb->b_forw;
		if (IS_MASTERBUF(tlvb))
			tlvb->av_back = lvb->b_back;
		else
			tlvb->b_back = lvb->b_back;
		tlvb = lvb->b_back;
		if (IS_MASTERBUF(tlvb))
			tlvb->av_forw = lvb->b_forw;
		else
			tlvb->b_forw = lvb->b_forw;

		mutex_spinunlock(&xlv_low_strat_lock, s);
	}

} /* end of xlv_low_strat_done() */


/*ARGSUSED3*/
int
xlvioctl ( 
	dev_t        dev, 
	unsigned int cmd, 
	caddr_t      arg, 
	int          mode, 
	cred_t       *crp, 
	int          *rvalp)
{
	xlv_tab_subvol_t        *xlv_p, subvol;
	xlv_tab_subvol_t	*sv_p;
	xlv_tab_plex_t		*plexp, *tplexp;
	xlv_disk_geometry_t	geometry;
	xlv_getdev_t		getdev;
	xlv_plex_copy_param_t	plex_copy_param;
	int			error = 0;
	int			i, plexsize;
#if (_MIPS_SIM == _ABI64)
	int kern_abi = ABI_IRIX5_64;
#elif (_MIPS_SIM == _ABIN32)
	int kern_abi = ABI_IRIX5_N32;
#elif (_MIPS_SIM == _ABIO32)
	int kern_abi = ABI_IRIX5;
#endif


        if (minor(dev) >= xlv_maxunits)
                return ENXIO;

	xlv_acquire_io_lock(minor(dev), NULL);
	xlv_lock_trace ("LOCK ioctl", XLV_TRACE_ADMIN, ((buf_t *)((unsigned long)minor(dev))));

	if (! xlv_tab) {
                xlv_release_io_lock(minor(dev));
		xlv_lock_trace ("UNLOCK ioctl", XLV_TRACE_ADMIN,
				(buf_t *)((unsigned long)(minor(dev))));
                return ENXIO;
        }

        xlv_p = &xlv_tab->subvolume[minor(dev)];

        if (!XLV_SUBVOL_EXISTS(xlv_p) || !XLV_ISOPEN(xlv_p->flags)) {
		error = ENXIO;
		goto done;
	}

        switch(cmd) {

        	case DIOCGETXLV:

			geometry.version = XLV_DISK_GEOMETRY_VERS;
			geometry.subvol_size = xlv_p->subvol_size;
			geometry.trk_size = xlv_tab_subvol_trksize(xlv_p);

			if (xlate_copyout((char *)&geometry,
					  (char *)arg,
					  sizeof(xlv_disk_geometry_t),
					  xlv_geom_to_irix5,
					  get_current_abi(), kern_abi, 1)) {
                        	error = EFAULT;
			}
                	break;

		case DIOCGETXLVDEV:

			getdev.version = XLV_GETDEV_VERS;
			getdev.data_subvol_dev = makedev(0,0);
			getdev.log_subvol_dev = makedev(0,0);
			getdev.rt_subvol_dev = makedev(0,0);

			sv_p = xlv_p->vol_p->data_subvol;
			if (sv_p != 0) {
				getdev.data_subvol_dev = sv_p->dev;
			}
			sv_p = xlv_p->vol_p->log_subvol;
			if (sv_p != 0) {
				getdev.log_subvol_dev = sv_p->dev;
			}
			sv_p = xlv_p->vol_p->rt_subvol;
			if (sv_p != 0) {
				getdev.rt_subvol_dev = sv_p->dev;
			}

                        if (copyout ((caddr_t)&getdev, (caddr_t)arg, 
                            sizeof(xlv_getdev_t)))
                                error = EFAULT;

			break;

		case DIOCGETSUBVOLINFO:

			/*
 			 * Copyin user xfs_tab_subvol_t structure to gain
 			 * access to pointers.
			 */
			if (copyin_xlate((caddr_t)arg,
					 (caddr_t)&subvol,
					 sizeof(xlv_tab_subvol_t),
					 irix5_to_xlv_tab_subvol,
					 get_current_abi(), kern_abi, 1)) {
				error = EFAULT;
				break;
			}

			/* 
			 * For each plex, copy info into local buffer
			 * before copying it to user space.
			 */
			for ( i = 0; i < XLV_MAX_PLEXES; i++ ) {

				if ((tplexp = xlv_p->plex[i]) == NULL)
					continue;

				ASSERT(tplexp->num_vol_elmnts > 0);

				plexsize = sizeof(xlv_tab_plex_t) +
					(sizeof(xlv_tab_vol_elmnt_t) * 
						(tplexp->num_vol_elmnts - 1));
				plexp = (xlv_tab_plex_t *)
					kmem_zalloc(plexsize, KM_SLEEP);
				bcopy(tplexp, plexp, plexsize);
				plexp->max_vol_elmnts = plexp->num_vol_elmnts;

				/*
				 * XXX Check the destination user space
				 * plex is big enough to hold everything.
				 */
				if (XLATE_COPYOUT((caddr_t)plexp,
						  (caddr_t)subvol.plex[i],
						  plexsize,
						  xlv_tab_plex_to_irix5,
						  get_current_abi(),
						  plexp->num_vol_elmnts)) {
					error = EFAULT;
				}

				kmem_free(plexp, 0);
				plexp = NULL;
				if (error)
					goto done; /* no use processing more */
			}

			/*
			 * Only copy over the necessary fields. 
			 * (NOT THE POINTERS !)
			 */
			subvol.flags 		=  xlv_p->flags;
			subvol.open_flag 	=  xlv_p->open_flag;

			COPY_UUID(xlv_p->uuid, subvol.uuid);

			subvol.subvol_type 	=  xlv_p->subvol_type;
			subvol.subvol_depth 	=  xlv_p->subvol_depth;
			subvol.subvol_size 	=  xlv_p->subvol_size;
			subvol.dev	 	=  xlv_p->dev;
			subvol.num_plexes 	=  xlv_p->num_plexes;

			if (xlate_copyout((caddr_t)&subvol, (caddr_t)arg,
					  sizeof(xlv_tab_subvol_t),
					  xlv_tab_subvol_to_irix5,
					  get_current_abi(), kern_abi, 1)) {
				error = EFAULT;
			}
			break;

		case DIOCXLVPLEXCOPY:

			error = copyin_xlate ((caddr_t)arg,
					      (caddr_t)&plex_copy_param,
					      sizeof(xlv_plex_copy_param_t),
					      irix5_to_xlv_plex_copy_param,
					      get_current_abi(), kern_abi, 1);
			if (error == 0 &&
			    plex_copy_param.version != XLV_PLEX_COPY_PARAM_VERS) {
				error = EINVAL;
			}

                        /*
                         * We release the lock now since xlv_copy_plexes
                         * will reacquire its own locks.
                         */
                        xlv_release_io_lock(minor(dev));
			xlv_lock_trace ("UNLOCK ioctl/plex_cpy",
				XLV_TRACE_ADMIN, 
				(buf_t *)((unsigned long)minor(dev)));

			if (error == 0)
				error = xlv_copy_plexes (dev,
					plex_copy_param.start_blkno,
					plex_copy_param.end_blkno,
					plex_copy_param.chunksize, 
					plex_copy_param.sleep_intvl_msec);

			xlv_lock_trace ("NONE ioctl/plex_cpy_done",
				XLV_TRACE_ADMIN, 
				(buf_t *)((unsigned long)minor(dev)));

			return error;

            	default:
                	error = EINVAL;
        }

done:
	xlv_release_io_lock(minor(dev));
	xlv_lock_trace ("UNLOCK ioctl.end", XLV_TRACE_ADMIN,
                        (buf_t *)((unsigned long)minor(dev)));

        return error;

} /* end of xlvioctl() */
