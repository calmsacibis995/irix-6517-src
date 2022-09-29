/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.73 $"

/*
 * Kernel routines to manipulate the xlv_tab structure.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/sema.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/var.h>
#include <sys/capability.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/xlate.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include "xlv_mem.h"
#include <sys/xlv_lock.h>
#include <sys/xlv_stat.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"
#include "xlv_xlate.h"

#ifdef DEBUG_XLV_TAB
void print_xlv_tab_entry (xlv_tab_subvol_t *sv);
void print_xlv_tab_vol (xlv_tab_vol_t *k_vtab);
void print_xlv_tab_array (xlv_tab_t	*k_svtab);
#endif

static unsigned copyin_fill_subvol (
	    xlv_tab_vol_t *u_vtab, xlv_tab_vol_t *k_vtab, xlv_tab_t *k_svtab,
            unsigned *IO_max_top_bufs_needed, unsigned *IO_max_low_bufs_needed,
	    int);
static unsigned resolve_vol_p (xlv_tab_subvol_t *ksv,     
	    irix5_xlv_tab_vol_t *u_vtab, xlv_tab_vol_t *k_vtab); 
static unsigned copyin_plexes (xlv_tab_subvol_t *ksv, int);
static void xlv_tab_free_subvol (xlv_tab_subvol_t *sv);
static int irix5_to_xlv_tab( enum xlate_mode, void *, int, xlate_info_t *);
static int irix5_o32_to_xlv_tab( enum xlate_mode, void *, int, xlate_info_t *);
static int irix5_n32_to_xlv_tab( enum xlate_mode, void *, int, xlate_info_t *);
#if (_MIPS_SIM == _ABI64)
static int irix5_to_xlv_vol_tab( enum xlate_mode, void *, int, xlate_info_t *);
#endif

extern void xlv_start_daemons(void);
void xlv_configchange_notification(void);

/*
 * Master incore database; shared by the driver.
 */
xlv_tab_vol_t		*xlv_tab_vol = NULL;
xlv_tab_t		*xlv_tab = NULL;

/* Maximum number of entries in xlv_tab_vol. */
int xlv_maxvols = 0;
/* Maximum number of minor devices = max number of subvolumes */
int xlv_maxunits = 0;

/* Times the configuration has changed */
int     xlv_config_generation = 0;

extern int		xlv_maxlocks;

/* 
 * Copy a complete xlv_tab and xlv_tab_vol into the kernel.
 * Replace the master copy if it succeeds.
 */
unsigned
xlv_tab_set (xlv_tab_vol_t *u_vtab, xlv_tab_t *u_svtab, int user_space)
{
	xlv_tab_vol_t		*k_vtab, *old_xlv_tab_vol;
	xlv_tab_t		*k_svtab, *old_xlv_tab;
	unsigned		retval, s;
	int			old_xlv_maxunits, old_xlv_maxvols;
	static boolean_t	update_in_progress = B_FALSE;
	static boolean_t	xlvd_started = B_FALSE;
	unsigned		max_top_bufs_needed, max_low_bufs_needed;
	xlv_pdev_list_t		pdevs;
	int			error;
        boolean_t               configuration_change = B_FALSE;

	if (! _CAP_ABLE(CAP_DEVICE_MGT)) {
		return (EPERM);
	}

	if (update_in_progress) {
		return (EBUSY);
	}

	if ((u_vtab == NULL) || (u_svtab == NULL)) {
                return (EINVAL);
        }

	old_xlv_maxunits = 0;
	old_xlv_tab_vol = NULL;
	old_xlv_tab = NULL;

	update_in_progress = B_TRUE;

	if (user_space && !xlvd_started) {
		xlv_start_daemons();
		xlvd_started = B_TRUE;
	}

	/*
	 * Copy the new volume description into the kernel.
	 */
	if (retval = xlv_tab_copyin (u_vtab, u_svtab, &k_vtab, &k_svtab,
	   &max_top_bufs_needed, &max_low_bufs_needed, user_space)) {
		update_in_progress = B_FALSE;
		return (retval);
	}

	/* set maxvols earlier so that we can scale the reserved buffers */
	old_xlv_maxvols = xlv_maxvols;
	xlv_maxvols = k_vtab->max_vols;

	/*
	 * Increase the reserved memory buffer space and allocate memory
	 * for the pdev list now, before locking all subvolumes.
	 */
	error = xlv_tab_alloc_reserved_buffers (max_top_bufs_needed,
					max_low_bufs_needed, B_TRUE);

	/* couldn't allocate space */
	if (error) {
		xlv_maxvols = old_xlv_maxvols;

		/* Free memory used by the new data tables. */
		xlv_tab_free (k_vtab, k_svtab);

		update_in_progress = B_FALSE;
		return error;
	}

	pdevs = (xlv_pdev_list_t) kmem_zalloc(MAX_PDEV_NODES *
					      sizeof(xlv_pdev_node_t),
					      KM_SLEEP);
	/* 
	 * Acquire write locks on all the subvolumes. 
	 * We make an optimization here.  Only this routine expands
	 * the table, and this routine is single-threaded.  Thus, we
	 * only need to acquire locks for entries that currently exist 
	 * in the table. 
	 *
	 * Note that we acquire all the locks before modifying *any*
	 * of the subvolumes because a disk part might be moved from
	 * one subvolume to another.
	 *
	 * XXXjleong 10/5/95 Is there a race condition with xlv_maxunits
	 * since it is not be protected? Running multiple xlv_assemble(1m)
	 * at the same time can be a problem.
	 */
	for (s=0; s < xlv_maxunits; s++) {
		/* 
		 * We acquire the lock at PZERO, a non-breakable 
		 * priority. Thus we do not need to check the 
		 * return code of mr_lock.
		 */
		xlv_acquire_cfg_lock(s);
	}

	/*
	 * XXX WH 5/9/94 There is a problem here.
	 *
	 * We currently have the xlv data structures locked, blocking
	 * all I/O. 
	 * xlv_tab_merge and xlv_tab_alloc_reserved_buffers, however,
	 * need to do kmem_alloc's. If memory is low & the swap device
	 * is on a logical volume, we could deadlock.
	 *
	 * This is not very likely as xlv_assemble is typically called
	 * when the system is coming up. But could be a problem if
	 * we do dynamic changes.
	 * 
	 * We'll have to revisit this.
	 * Done -- KK.
	 */

	old_xlv_maxunits = xlv_maxunits;

	/*
	 * Copy and merge the state of the old and new xlv_tabs.
	 */
	if (xlv_tab) {
		retval = xlv_tab_merge (k_svtab, xlv_tab, xlv_maxunits, pdevs);
		if (retval) {
			goto cleanup;
		}
	}

	old_xlv_tab_vol = xlv_tab_vol;
	old_xlv_tab = xlv_tab;

	xlv_tab_vol = k_vtab;
	xlv_tab = k_svtab;

	xlv_maxvols = k_vtab->max_vols;
	xlv_maxunits = k_svtab->max_subvols;

	/*
	 * Statistics space.  Subvolumes that do not exist should have
	 * an empty statistic structure.
	 * 
	 * Since you cannot assemble out a volume from the running kernel
	 * and you want statistics consistent between multiple xlv_assemble
	 * calls, don't clear existing statistics.
	 *
	 * Volumes that are remove the kernel will have its stat
	 * cleared or freed.
	 */
	for (s=0; s < xlv_maxunits; s++) {
		if (xlv_io_lock[s].statp == NULL) {
			xlv_io_lock[s].statp = (xlv_stat_t *)
				kmem_zalloc(sizeof(xlv_stat_t), KM_SLEEP);
		}
	}
	if (old_xlv_maxunits > xlv_maxunits) {
		/* free excess stat structures */
		for( ; s < old_xlv_maxunits; s++) {
			if (!xlv_io_lock[s].statp) {
				ASSERT(0);
				continue;
			}
			kmem_free(xlv_io_lock[s].statp, 0);
			xlv_io_lock[s].statp = NULL;
		}
	}

	xlv_config_generation++;		/* configuration change */
        configuration_change = B_TRUE;

cleanup:
	/* free the pdev list memory */
	kmem_free(pdevs, 0);

	/*
	 * Release all the write locks.
	 */
	for (s=0; s < old_xlv_maxunits; s++) {
		xlv_release_cfg_lock(s);
	}

	if (!retval && old_xlv_tab_vol) {
		/* Free memory used by the old data tables. */
		xlv_tab_free (old_xlv_tab_vol, old_xlv_tab);
	}

	update_in_progress = B_FALSE;

	if (configuration_change)
		xlv_configchange_notification();

	return (retval);

} /* end of xlv_tab_set() */


/*
 * Adjust the block map to account for stale plexes and set up
 * the read/write-back regions. This routine is called by both
 * xlv_tab_set as well as xlv_copy_plexes.
 *
 * Note: When we load a new configuration into the kernel, we do
 * not clobber the current read/write-back region settings. This
 * is because a plex revive may already be in progress. In that case,
 * we'll just let it finish. 
 * Eventually, xlv_plexd would process the plex revive for the new
 * configuration and it will recompute the read/write-back region.
 * Hence, the need for this call here.
 */
void
xlv_setup_plex_rd_wr_back_region (
	xlv_tab_subvol_t	*sv)
{
	xlv_block_map_t		*block_map;
	daddr_t			start_blkno;
	boolean_t		rd_wr_back_started = B_FALSE;
	int			i;

	ASSERT (sv->block_map);
	block_map = sv->block_map;

	/*
	 * If one of the plexes contain good data while the others contain
	 * stale data, then xlv_tab_create_block_map() would have set the
	 * read_plex_vector appropriately.
	 *
	 * If, however, all the plexes are stale, then none of the 
	 * read_plex_vector bits would be set. This would may it impossible
	 * for us to read from this subvolume.
	 * Therefore, during plex startup, if all the plexes are stale, 
	 * we'll just make them all available since the
	 * read/write-back will take care of making them consistent.
	 *
	 * As we go through the block map, also figure out if we need
	 * to do a plex recovery on the plexes in this subvolume.
	 * Basically, if all the read_plex_vectors match the 
	 * write_plex_vectors, then there is no need to revive the
	 * plex since all the entries are not stale.
	 * The place where we need to start reviving is the smallest
	 * block number at which we encounter a stale plex - i.e.,
	 * where read_plex_vector != write_plex_vector.
	 *
	 * Since we only maintain a single read/write-back region per
	 * subvolume and each volume element has its own state, it is
	 * possible for a clean volume element to be sandwiched between
	 * two stale volume elements.  In this case, we would just put
	 * the entire region into read/write-back mode. This allows us
	 * to save memory and code in the common case.
	 */

	/*
	 * Initialize the read/write-back area to be after the end
	 * of the subvolume; i.e., disable it first.
	 */
	sv->rd_wr_back_start = sv->rd_wr_back_end = sv->subvol_size;


	start_blkno = 0;	/* keep track of the starting block number
				   for the current block map entry. */
	for (i=0; i < block_map->entries; i++) {

		if (block_map->map[i].read_plex_vector == 0) {

			/*
			 * Pick one of the stale plexes for reading.
			 */
			block_map->map[i].read_plex_vector =
				FIRST_BIT_SET (block_map->map[i].
					write_plex_vector);

			/*
			 * Need to include the current range of blocks
			 * in the read/write-back region.
			 */
			if (! rd_wr_back_started) {

				if (sv->rd_wr_back_start == sv->rd_wr_back_end){

					/*
					 * We are starting the first read/
					 * write-back region.
					 */
					sv->rd_wr_back_start = start_blkno;
					rd_wr_back_started = B_TRUE;
				}
				else {
					/*
					 * We already have a read/write-back
					 * region. We then hit a clean
					 * vol element which caused us to
					 * terminate it. But now we've hit
					 * another stale vol elmnt. We want
					 * to extend the previous region
					 * instead of creating a new one.
					 */
					rd_wr_back_started = B_TRUE;
				}
			}
		}
		else if (block_map->map[i].read_plex_vector !=
			 block_map->map[i].write_plex_vector) {

			/*
			 * Some plexes are stale, but there is at least
			 * one non-stale plex. In this case, only read
			 * from the non-stale plex.
			 */
			ASSERT (block_map->map[i].write_plex_vector);

                        /*
                         * Need to include the current range of blocks
                         * in the read/write-back region.
                         */
                        if (! rd_wr_back_started) {

                                if (sv->rd_wr_back_start == sv->rd_wr_back_end){
                                        /*
                                         * We are starting the first read/
                                         * write-back region.
                                         */
                                        sv->rd_wr_back_start = start_blkno;
                                        rd_wr_back_started = B_TRUE;
                                }       
                                else {
                                        /*
                                         * We already have a read/write-back
                                         * region. We then hit a clean
                                         * vol element which caused us to
                                         * terminate it. But now we've hit
                                         * another stale vol elmnt. We want
                                         * to extend the previous region
                                         * instead of creating a new one.
                                         */
                                        rd_wr_back_started = B_TRUE;
                                }
                        }

		}
		else {
			/*
			 * The plexes are consistent.
			 */
			start_blkno = block_map->map[i].end_blkno + 1;

			/*
			 * If we have marked the start of a read/
			 * write-back region, end it just before this
			 * point.
			 */
			if (rd_wr_back_started) {
				sv->rd_wr_back_end = start_blkno-1;
				rd_wr_back_started = B_FALSE;
			}
		}

	}

	/*
	 * If we have not terminated the read/write-back region, then
	 * set it to the end of the subvolume.
	 */
	if (rd_wr_back_started)
		sv->rd_wr_back_end = sv->subvol_size - 1;

	/*
	 * We initialize the critical region to be zero-length for now.
	 * This means that we'll do normal reads from blkno 0-start_blkno
	 * and read/write-backs from start_blkno to <size of subvol>.
	 */
}


/*
 * Copy a complete xlv_tab and xlv_tab vol into the kernel.
 *
 * This function also computes the maximum number of buffers that
 * the xlv driver  should preallocate so that we won't deadlock
 * when memory is low.
 *
 * Note: This entrypoint is exported because the user mode test program
 * uses it.
 */
unsigned
xlv_tab_copyin (xlv_tab_vol_t *u_vtab, xlv_tab_t *u_svtab, 
		xlv_tab_vol_t **O_k_vtab, xlv_tab_t **O_k_svtab,
		unsigned *O_max_top_bufs_needed,
		unsigned *O_max_low_bufs_needed,
		int	user_space)
{

	xlv_tab_vol_t	*k_vtab = NULL, localxlvtabvol;
	xlv_tab_t 	*k_svtab = NULL, localxlvtab;
	unsigned	retval, num_vols, max_subvols, s;
	unsigned	max_top_bufs_needed, max_low_bufs_needed;
#if (_MIPS_SIM == _ABI64)
	int kern_abi = ABI_IRIX5_64;
#elif (_MIPS_SIM == _ABIN32)
	int kern_abi = ABI_IRIX5_N32;
#elif (_MIPS_SIM == _ABIO32)
	int kern_abi = ABI_IRIX5;
#endif

	/* Copy the xlv_tab_vol structure in */

	if (user_space) {
		/*
		 * Note: don't call copyin_xlate() directly because
		 * o32 and n32 xlv_tab_vol_t are the same; call the macro.
		 */
		if ( COPYIN_XLATE( (caddr_t)u_vtab,
				   (caddr_t)&localxlvtabvol,
				   sizeof(xlv_tab_vol_t),
				   irix5_to_xlv_vol_tab,
				   get_current_abi(),
				   1   ) ) {
			return EFAULT;
		}
		num_vols = localxlvtabvol.num_vols;
	} else {
		num_vols = u_vtab->num_vols;
	}

	if ((num_vols < 1) || (num_vols > (xlv_maxlocks/XLV_SUBVOLS_PER_VOL)) )
		return E2BIG;

	/*
	 * Check the volume entry version now before any allocation.
	 * The assumption is that the volume table is dense and all
	 * volume entries are of the same version.
	 */
	if (user_space &&
	    (localxlvtabvol.vol[0].version != XLV_TAB_VOL_ENTRY_VERS))
		return XLVE_VOL_VERS_INVAL;

	s = sizeof(xlv_tab_vol_t) + ((num_vols-1)*sizeof(xlv_tab_vol_entry_t));
	k_vtab = kmem_zalloc (s, KM_SLEEP);

	if (user_space) {
		if  (COPYIN_XLATE( (caddr_t)u_vtab,
				   (caddr_t)k_vtab,
				   s,
				   irix5_to_xlv_vol_tab,
			           get_current_abi(),
				   num_vols) ) {
			kmem_free (k_vtab, 0);
			return EFAULT;
		}
	} else {
		bcopy(u_vtab, k_vtab, s);
	}
	k_vtab->max_vols = num_vols;		/* set true max size */

#ifdef DEBUG_XLV_TAB
	print_xlv_tab_vol (k_vtab);
#endif

	/*
	 * Copy the xlv_tab array in.  Since xlv_tab is sparse, we have
	 * to copy the maximum number allocated in the structure.
	 *
	 * Note: Since the table can be sparse, checking version number
	 * can only be done on the first element. When the size of the
	 * subvol struct changes, all subsequent subvol entries have
	 * the version field at a different offset. Must figure out
	 * another way to check version. The workaround is to bump
	 * both the subvol and volume version number at the same time.
	 */
	if (user_space) {
		if ( copyin_xlate( (caddr_t)u_svtab,
				   (caddr_t)&localxlvtab,
				   sizeof(xlv_tab_t),
				   irix5_to_xlv_tab,
				   get_current_abi(), kern_abi,
				   1 ) ) {
			kmem_free (k_vtab, 0);
			return EFAULT;
		}
		max_subvols = localxlvtab.max_subvols;
	} else {
		max_subvols = u_svtab->max_subvols;
	}

	/*
	 * We can only support as many subvolumes as there are entries in
	 * the lock table.
	 */
	if ((max_subvols < 1) || (max_subvols > xlv_maxlocks) ) {
		kmem_free (k_vtab, 0);
		return E2BIG;
	}

	s = sizeof(xlv_tab_t) + ((max_subvols-1) * sizeof(xlv_tab_subvol_t));
	k_svtab = kmem_zalloc (s, KM_SLEEP);

	if (user_space) {
		if ( copyin_xlate( (caddr_t)u_svtab,
				   (caddr_t)k_svtab,
				   s,
				   irix5_to_xlv_tab,
				   get_current_abi(), kern_abi,
				   max_subvols) ) {
			kmem_free (k_vtab, 0);
			kmem_free (k_svtab, 0);
	    		return EFAULT;
		}
	} else {
		bcopy( u_svtab, k_svtab, s);
	}

#ifdef DEBUG_XLV_TAB
	print_xlv_tab_array (k_svtab);
#endif

	max_top_bufs_needed = 0;
	max_low_bufs_needed = 0;
	retval = copyin_fill_subvol (u_vtab, k_vtab, k_svtab,
		&max_top_bufs_needed, &max_low_bufs_needed, user_space);
	if (retval) {
	    	kmem_free (k_vtab, 0);
	    	kmem_free (k_svtab, 0);
		return (retval);
	}

	*O_k_vtab = k_vtab;
	*O_k_svtab = k_svtab;
	*O_max_top_bufs_needed = max_top_bufs_needed;
	*O_max_low_bufs_needed = max_low_bufs_needed;

	return (0);

} /* end of xlv_tab_copyin() */


/*
 * Fill in the xlv_tab and xlv_tab_vol structures.  This routine
 * assumes that the array itself has been copyin'ed but that the
 * references that come off the array have not.
 */
static unsigned
copyin_fill_subvol (
	xlv_tab_vol_t	*u_vtab,
	xlv_tab_vol_t	*k_vtab,
	xlv_tab_t	*k_svtab,
	unsigned	*IO_max_top_bufs_needed,
	unsigned	*IO_max_low_bufs_needed,	
	int		user_space)
{
	int			s, v;
	xlv_tab_subvol_t	*ksv;
	xlv_tab_vol_entry_t	*vol;
	unsigned		retval;
	unsigned		max_top_bufs_needed, max_low_bufs_needed;

	/*
	 * This routine will set up the subvolume->volume and volume->
	 * subvolume pointers for those subvolumes that exist.
	 * This means that there may be junk in the pointer to 
	 * non-existent subvolumes.  Since xlv_tab_free_subvol()
	 * relies on a NULL pointer to indicate the absence of a 
	 * subvolume, it might inadvertently use a bad pointer.  
	 * So we clear them all first.
	 */
	for (v=0, vol=k_vtab->vol; v < k_vtab->num_vols; v++, vol++) {
		vol->log_subvol = NULL;
	    	vol->data_subvol = NULL;
	    	vol->rt_subvol = NULL;

		/*
		 * Currently the kernel does not know anything about
		 * the nodename associated with a volume. Set it to NULL.
		 */
		vol->nodename = NULL;
	}

	/*
	 * Because xlv_tab is sparse, we need to iterate through the 
	 * entire xlv_tab, skipping any null entries.
	 */ 
	for (s=0, ksv=k_svtab->subvolume; s < k_svtab->max_subvols; 
		 s++, ksv++) {

		if (! XLV_SUBVOL_EXISTS(ksv))
		    continue;

		if (user_space && (ksv->version != XLV_TAB_SUBVOL_VERS)) {
			retval =  XLVE_SUBVOL_VERS_INVAL;
	        	goto free_memory;
		}

	    	ksv->flags = 0;
	    	retval = resolve_vol_p (ksv, (irix5_xlv_tab_vol_t *)u_vtab,
			k_vtab);
	    	if (retval) {
			ksv->num_plexes = 0;
			ksv->block_map = NULL;
	        	goto free_memory;
	    	}

		retval = copyin_plexes (ksv,user_space);
		if (retval) {
			ksv->block_map = NULL;
		    	goto free_memory;
		}

		/*
		 * We need to compute the subvol size before the
		 * block_map since xlv_tab_create_block_map() needs
		 * the subvol size for its checks.
		 */
		ksv->subvol_size = xlv_tab_subvol_size (ksv);

		/*
		 * If the subvolume is plexed (and the volume is not
		 * explicitly marked incomplete), compute the block map.
		 */

		/*
		 * XXX Check volume state.
		 */
		if (ksv->num_plexes > 1) {
			ksv->block_map =
				xlv_tab_create_block_map (ksv, NULL, NULL);
			if (ksv->block_map == NULL)
				return XLVE_BLOCK_MAP_INVAL;

			xlv_setup_plex_rd_wr_back_region (ksv);
		}
		
		/*
		 * Compute the number of buffers needed by the lower
		 * and upper xlv drivers.
		 */
		max_low_bufs_needed = xlv_tab_max_low_bufs (ksv);
		max_top_bufs_needed = xlv_tab_max_top_bufs (ksv);

		if (max_low_bufs_needed > *IO_max_low_bufs_needed)
			*IO_max_low_bufs_needed = max_low_bufs_needed;
		if (max_top_bufs_needed > *IO_max_top_bufs_needed)
			*IO_max_top_bufs_needed = max_top_bufs_needed;

	}
	return (0);

free_memory:

#if 0
	/*
	 * We assume that if copyin_plexes discovered an error,
	 * it would free any memory allocated during that invocation.
	 * Thus, we only need to free the memory allocated for
	 * subvolumes whose indices range from 0 to s-1.
	 * copyin_block_map() also observes this rule.
	 *
	 * Note that in the loop above, if resolve_vol_p fails,
	 * it would be before the call to copyin_plexes, thus
	 * memory would not be allocated for this plex even in
	 * that case.
	 */
	for (s--, ksv--; s >= 0; s--, ksv--) {

		for (p=0; p < XLV_MAX_PLEXES; p++) {
			if (ksv->plex[p] == NULL)
				continue;
		        kmem_free ((void *)ksv->plex[p], 0);
		}

		if (ksv->block_map != NULL)
			kmem_free (ksv->block_map, 0);
	}
#endif

	return (retval);
}


/*
 * Resolves the cross-links between a subvol entry and the volume
 * to which it belongs.
 */
static unsigned
resolve_vol_p (
	xlv_tab_subvol_t	*ksv,
	irix5_xlv_tab_vol_t	*u_vtab,
	xlv_tab_vol_t		*k_vtab)
{
	xlv_tab_vol_entry_t	*vp;
	unsigned long		offset;

	/*
	 * Compute the offset of the vol_p within the user-mode
	 * xlv_tab_vol and then add it to the base of the 
	 * kernel-mode xlv_tab_vol to get the address of the
	 * xlv_tab_vol_entry in kernel mode.
	 */
#if 0
	offset = (irix5_xlv_tab_vol_entry_t *)to64bit((__psint_t)ksv->vol_p) -
		 &(u_vtab->vol[0]);
#endif
	offset = (irix5_xlv_tab_vol_entry_t *) ksv->vol_p -
		 &(u_vtab->vol[0]);

	vp = &(k_vtab->vol[offset]);

	switch (ksv->subvol_type) {
		case XLV_SUBVOL_TYPE_LOG:
		    vp->log_subvol = ksv;
		    break;
		case XLV_SUBVOL_TYPE_DATA:
		    vp->data_subvol = ksv;
		    break;
		case XLV_SUBVOL_TYPE_RT:
		    vp->rt_subvol = ksv;
		    break;
		default: 
		    return (XLVE_SUBVOL_TYPE_INVAL);
	}
	ksv->vol_p = vp;

	return (0);
}


#if 0
/*
 * Copy a block map into kernel memory.
 */
static unsigned 
copyin_block_map (xlv_tab_subvol_t *ksv, int user_space)
{
	int			entries, sizeof_block_map;
	xlv_block_map_t		*block_map;
	
	if (user_space) {
		if (copyin (&(ksv->block_map->entries), 
					&entries, sizeof(entries))) {
			return EFAULT;
		}
	} else {
		entries = ksv->block_map->entries;
	}

	sizeof_block_map = sizeof (xlv_block_map_t) + ((entries-1) * sizeof
                        (xlv_block_map_entry_t));
	block_map = (xlv_block_map_t *) kmem_zalloc(sizeof_block_map, KM_SLEEP);

	if (user_space) {
		if (copyin (ksv->block_map, block_map, sizeof_block_map)) {
			kmem_free (block_map, 0);
			return EFAULT;
		}
	} else {
		bcopy(ksv->block_map, block_map, sizeof_block_map);
	}

	ksv->block_map = block_map;

	return 0;
}
#endif /* 0 */


/*
 * Copy the plex information into kernel memory.
 */
static unsigned
copyin_plexes (xlv_tab_subvol_t *ksv, int user_space) 
{
	xlv_tab_plex_t *localplex_p;
	    /*
	     * XXX Only used for getting field num_vol_elmnts from the
	     * user space structure. Should really directly copy from the
	     * structure to the local variable num_vol_elmnts.
	     */
	long num_vol_elmnts;
	long size_of_plex;
	unsigned retval;
	xlv_tab_plex_t *plex_p;
	int p;

	if ((ksv->num_plexes < 1) || (ksv->num_plexes > XLV_MAX_PLEXES))
		return (XLVE_NUM_PLEXES_INVAL);

	if (user_space) {
		localplex_p = (xlv_tab_plex_t *)
			kmem_zalloc(sizeof(xlv_tab_plex_t), KM_SLEEP);
	} else {
		localplex_p = NULL;
	}

	for (p=0; p < XLV_MAX_PLEXES; p++) {
		if (NULL == ksv->plex[p]) {
			continue;
		}
		if (user_space) {
			/*
		 	 * Note: don't call copyin_xlate() directly
			 * because o32 and n32 xlv_tab_plex_t are the
			 * same; call the macro.
			 */
			if ( COPYIN_XLATE( (caddr_t)(ksv->plex[p]), 
				   (caddr_t)localplex_p,
				   sizeof(xlv_tab_plex_t),
				   irix5_to_xlv_tab_plex,
				   get_current_abi(), 
				   1   ) ) {
					retval =  EFAULT;
					goto free_memory;
			}
			num_vol_elmnts = localplex_p->num_vol_elmnts;
		} else {
			num_vol_elmnts = ksv->plex[p]->num_vol_elmnts;
		}

		if ((num_vol_elmnts < 1) || 
		    (num_vol_elmnts > XLV_MAX_VE_PER_PLEX)) {
           	 	retval = XLVE_NUM_VES_INVAL;
            		goto free_memory;
		}
		size_of_plex = sizeof(xlv_tab_plex_t) +
	    		((num_vol_elmnts-1) * sizeof(xlv_tab_vol_elmnt_t));
		plex_p = ksv->plex[p];	/* save the user-mode address */
		ksv->plex[p] = (xlv_tab_plex_t *) 
			kmem_alloc(size_of_plex, KM_SLEEP);

		if (user_space) {
			if ( COPYIN_XLATE((caddr_t)plex_p,
					(caddr_t)ksv->plex[p],
					size_of_plex,
					irix5_to_xlv_tab_plex,
					get_current_abi(),
					num_vol_elmnts) ) {
			    /* 
			     * We free the current plex memory inline here 
			     * because the cleanup code will do from 0 to p-1. 
			     */
				    kmem_free((void *)ksv->plex[p], 0);
				    retval = EFAULT;
				    goto free_memory;
			}
		} else {
			bcopy(plex_p, ksv->plex[p], size_of_plex);
		}
	} /* for each plex */

	if (localplex_p)
		kmem_free(localplex_p, 0);
	return (0);

free_memory:
	while (--p >= 0) {
		if (ksv->plex[p]) {
			kmem_free((void *)ksv->plex[p], 0);
			ksv->plex[p] = NULL;
		}
	}
	if (localplex_p)
		kmem_free(localplex_p, 0);
	return(retval);

} /* end of copyin_plexes() */


/*
 * Free the xlv_tab and xlv_tab_vol.
 */
void
xlv_tab_free (xlv_tab_vol_t *vtab, xlv_tab_t *svtab)
{
	int                  v;
	uint_t              status;
	xlv_tab_vol_entry_t *vol;

	if (vtab == NULL)
		return;

	/*
	 * Go thru the the volume table and free all associated structures.
	 *
	 * NOTE: Use the "max_vols" counter instead of "num_vols" counter
	 * because the shutdown path could have removed volume and not
	 * compacted the volume table.
	 */
	for (v=0, vol=vtab->vol; v < vtab->max_vols; v++, vol++) {
		if (uuid_is_nil(&vol->uuid, &status)) {
			continue;
		}
		xlv_tab_free_subvol(vol->log_subvol);
		xlv_tab_free_subvol(vol->data_subvol);
		xlv_tab_free_subvol(vol->rt_subvol);	
	}

	kmem_free(svtab, 0);
	kmem_free(vtab, 0);
}


static void
xlv_tab_free_subvol (xlv_tab_subvol_t *sv)
{
	int p;
	
	if (sv == NULL)
		return;

	for (p=0; p < XLV_MAX_PLEXES; p++) {
		if (sv->plex[p] == NULL)
			continue;
		kmem_free(sv->plex[p], 0);
    }
}


/*
 * xlv_tab_subvol_size exists in both user-mode code and kernel
 * code.  To prevent duplicate symbols when we link this module
 * in user-mode for testing, we use the following preprocessor symbol.
 */
#ifndef _XLV_TAB_SUBVOL_SIZE

daddr_t
xlv_tab_subvol_size (xlv_tab_subvol_t *subvol_entry)
{
        xlv_tab_plex_t	*plex;
        int		p;
        int		index;
        daddr_t		last_block = 0;

        /*
         * This code assumes that all the volume elements across a
         * "row" are the same size and volume elements in a plex
	 * are in increasing block number order. Therefore, the last
	 * volume element of each plex potentially has last block
	 * address.
         */
        for (p = 0; p < XLV_MAX_PLEXES; p++) {

                if ((plex = subvol_entry->plex[p]) == NULL)
			continue;


		/*
		 * XXXjleong Need check for good plex?
		 */
		if (! (plex->num_vol_elmnts > 0))
			continue;
		index = plex->num_vol_elmnts - 1;
		if (plex->vol_elmnts[index].end_block_no > last_block) {
			last_block = plex->vol_elmnts[index].end_block_no;
		}
	}

        return (last_block+1);
}

#endif /* _XLV_TAB_SUBVOL_SIZE */

/*
 * Return the tracksize for the subvolume in blocks.
 *
 * For now, we use the track size of the first disk in the
 * subvolume.
 */
unsigned
xlv_tab_subvol_trksize (xlv_tab_subvol_t *subvol_entry)
{
	unsigned i;

	for (i=0; i < XLV_MAX_PLEXES; i++) {
		if (subvol_entry->plex[i] == NULL)
			continue;

		return (subvol_entry->plex[i]->vol_elmnts[0].
			stripe_unit_size);
	}
	return( 0 );
}


/*
 * Maximum size for a single request to the lower xlv driver.
 */
#ifdef USER_MODE
#define MAXTRANS 0x400000
#else
#define MAXTRANS (ctob(v.v_maxdmasz))
#endif /* USER_MODE */


/*
 * Calculate the number of low bufs needed by this plex.
 * See comments in xlv_tab_max_low_bufs().
 */
unsigned
xlv_tab_low_bufs_for_plex (xlv_tab_plex_t *plexp)
{
	xlv_tab_vol_elmnt_t	*vep;
	int			ve;
	unsigned int		bufs_needed;
	unsigned int		max_bufs_in_plex = 0;
	int			max_trans;

	if (plexp == NULL)
		return(0);

	max_trans = MAXTRANS >> BBSHIFT;

	for (ve = 0; ve < plexp->num_vol_elmnts; ve++) {
		vep = &plexp->vol_elmnts[ve];

		if (vep->type == XLV_VE_TYPE_STRIPE) {
			/*
			 * If the transfer matches the stripe boundary
			 * exactly, then we need
			 * max_trans/stripe_unit_size buffers.
			 * We add one because we might start slightly
			 * before or end slightly after the stripe.
			 *
			 * We add another one because we might be
			 * concatentating 2 stripe groups each of
			 * which is misaligned.
			 * 
			 * We could expend some effort to find out if
			 * we are indeed concatenating stripe groups &
			 * fine tune things if they are striped
			 * differently. But this is simpler. (The
			 * reduced code size makes up for the extra
			 * buffers.) 
			 */
			bufs_needed = (max_trans /
				       vep->stripe_unit_size) + 2;
		}
		else {
			/*
			 * The starting block address might start on
			 * one disk and wind up on the next one.
			 */
			bufs_needed = 2;
		}

		if (bufs_needed > max_bufs_in_plex)
			max_bufs_in_plex = bufs_needed;
	}

	return(max_bufs_in_plex);
}


unsigned
xlv_tab_max_low_bufs (xlv_tab_subvol_t *subvol_entry)
{
        xlv_tab_plex_t          *plex;
        int                     p;
        int                     max_bufs;

	max_bufs = 0;

	/*
	 * We need to be able to write to all the plexes in parallel.
	 * So the max number of buffers we need to allocate is roughly the max
	 * of the sum of the max's from each plex.
	 * 
	 * This number is actually conservative as it assumes that the maximum
	 * stripe group size occurs at the same "row" across all the plexes.
	 *
	 * Note that we assume that the individual partitions are at least
	 * MAXTRANS bytes in size.  If they are smaller than that, then it
	 * is possible for a single request to span more than 2 partitions.
	 * This should never occur for real disks.
	 */
        for (p = 0; p < XLV_MAX_PLEXES; p++) {
                if ((plex = subvol_entry->plex[p]) == NULL)
			continue;

		max_bufs += xlv_tab_low_bufs_for_plex(plex);
	}

	return (max_bufs);
}


/*
 * Compute the number of buffers needed by the upper
 * xlv driver.
 */
unsigned
xlv_tab_max_top_bufs (xlv_tab_subvol_t *sv)
{
	unsigned	max_top_bufs_needed;

	if (sv->num_plexes == 1)
		max_top_bufs_needed = 0;
	else {
		max_top_bufs_needed = sv->num_plexes;

		/*
		 * If the subvolume is not contiguous, allocate
		 * twice as much for cases where we span regions.
		 */
		if (sv->block_map && sv->block_map->entries > 1) {
			max_top_bufs_needed <<= 1;
		}
	}

	return max_top_bufs_needed;
}


/*
 * See if the number of preallocated write buffers we need is different
 * from that currently reserved. If so, allocate the new buffers and
 * free the old ones.
 * 
 * Note that both the upper and lower xlv driver maintain their own
 * private buffers.
 */
int
xlv_tab_alloc_reserved_buffers (
	unsigned	max_top_bufs_needed,
	unsigned	max_low_bufs_needed,
	const boolean_t can_shrink)
{
	extern unsigned xlv_ioctx_slots, xlv_topmem_slots, xlv_lowmem_slots;
	extern int xlv_plexing_support_present;
	extern sema_t xlv_memcfg;
	unsigned	slots;
	int		error = 0;

	/*
	 * Allocate the memory first.
	 * Note that we allocate an additional buffer because this
	 * simplifies the inner loop pointer arithmetic in xlv. 
	 *
	 * Fail if we cannot get the memory.
	 */
	max_top_bufs_needed++;
	max_low_bufs_needed++;


	/* only allow one memory resize operation at a time */
	psema(&xlv_memcfg, PRIBIO);

	/*
	 * Allow the number of buffers per slot to shrink when running
	 * xlv_assemble since it "knows" the current max buffers.
	 */
	if (xlv_plexing_support_present && 
	   (max_top_bufs_needed > xlv_top_mem_bufs || can_shrink)) {
		/* 
		 * ensure that we have at least 1 reserved pool 
		 * slot for every 8 vols 
		 */
		if (xlv_topmem_slots < xlv_maxvols/8) {
			slots = xlv_maxvols/8 + 1;

			/* Only grow the number of slots for the io context,
			 * not the size per slot
			 */
			error = xlv_res_mem_resize(&xlv_ioctx_mem, 
					sizeof(xlv_io_context_t), slots, 
					B_FALSE);
			if (error)
				goto done;

			xlv_ioctx_slots = xlv_ioctx_mem.slots;

		} else {
			slots = xlv_topmem_slots;
		}

		error = xlv_res_mem_resize(&xlv_top_mem, sizeof(struct xlvmem) +
				     (max_top_bufs_needed * sizeof(struct buf)),
				     slots, B_FALSE);
		if (error) 
			goto done;

		xlv_topmem_slots = xlv_top_mem.slots;
		xlv_top_mem_bufs = max_top_bufs_needed;
	}
	if (max_low_bufs_needed > xlv_low_mem_bufs || can_shrink) {

		/* 
		 * ensure that we have at least 1 reserved pool 
		 * slot for every 3 vols 
		 */
		if (xlv_lowmem_slots < xlv_maxvols/3) {
			slots = xlv_maxvols/3 + 1;
		} else {
			slots = xlv_lowmem_slots;
		}

		error = xlv_res_mem_resize(&xlv_low_mem, sizeof(struct xlvmem) +
				   (max_low_bufs_needed * sizeof(struct buf)),
				   slots, B_FALSE);
		if (error)
			goto done;

		xlv_lowmem_slots = xlv_low_mem.slots;
		xlv_low_mem_bufs = max_low_bufs_needed;
	}
done:
	vsema(&xlv_memcfg);
	return error;
}

#if (_MIPS_SIM == _ABI64)
/*
 * This is used with copyin_xlate() to copy a xfs_args structure
 * in from user space from a 32 bit application into a 64 bit kernel.
 */
static int
irix5_to_xlv_vol_tab(
        enum xlate_mode mode,
        void            *to,
        int             count,
        xlate_info_t    *info)
{
	int i = 0;

        COPYIN_XLATE_VARYING_PROLOGUE(irix5_xlv_tab_vol_s, xlv_tab_vol_s, 
		sizeof(irix5_xlv_tab_vol_t) +
                ((count-1)*sizeof(irix5_xlv_tab_vol_entry_t)));

	ASSERT (count > 0);

        target->num_vols = source->num_vols;
        target->max_vols = source->max_vols;

	while (count) {

		target->vol[i].version		= source->vol[i].version;
		target->vol[i].uuid		= source->vol[i].uuid;
		bcopy( source->vol[i].name, target->vol[i].name,
			sizeof(xlv_name_t));

		target->vol[i].flags		= source->vol[i].flags;
		target->vol[i].state 		= source->vol[i].state;
		target->vol[i].rsvd 		= source->vol[i].rsvd;
		target->vol[i].sector_size	= source->vol[i].sector_size;

		target->vol[i].log_subvol =
			(struct xlv_tab_subvol_s *)to64bit((__psint_t)
				source->vol[i].log_subvol);
		target->vol[i].data_subvol =
			(struct xlv_tab_subvol_s *)to64bit((__psint_t)
				source->vol[i].data_subvol);
		target->vol[i].rt_subvol =
			(struct xlv_tab_subvol_s *)to64bit((__psint_t)
				source->vol[i].rt_subvol);

		target->vol[i].nodename =
			(void *)to64bit((__psint_t) source->vol[i].nodename);
	
		i++;
		count--;
	}

        return 0;
}

#endif


#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
static int
irix5_to_xlv_tab(
        enum xlate_mode mode,
        void            *to,
        int             count,
        xlate_info_t    *info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));
	if (ABI_IS_IRIX5_N32(info->abi))
		return irix5_n32_to_xlv_tab(mode, to, count, info);
	else
		return irix5_o32_to_xlv_tab(mode, to, count, info);
}
#else
static int
irix5_to_xlv_tab(
        enum xlate_mode mode,
        void            *to,
        int             count,
        xlate_info_t    *info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi));
	return irix5_n32_to_xlv_tab(mode, to, count, info);
}
#endif


#if (_MIPS_SIM == _ABI64)
static int
irix5_o32_to_xlv_tab(
        enum xlate_mode mode,
        void            *to,
        int             count,
        xlate_info_t    *info)
{
	int		i = 0, j;
	
        COPYIN_XLATE_VARYING_PROLOGUE(irix5_o32_xlv_tab_s, xlv_tab_s, 
		sizeof(irix5_o32_xlv_tab_t) +
                ((count-1) * sizeof(irix5_o32_xlv_tab_subvol_t)));

	ASSERT (count > 0);

        target->num_subvols = source->num_subvols;
        target->max_subvols = source->max_subvols;
	
	while (count) {
		target->subvolume[i].version =
			source->subvolume[i].version;
		target->subvolume[i].flags =
			source->subvolume[i].flags;
		target->subvolume[i].open_flag =
			source->subvolume[i].open_flag;
		target->subvolume[i].vol_p =
			(struct xlv_tab_vol_entry_s *)to64bit((__psint_t)
			     source->subvolume[i].vol_p);
		target->subvolume[i].uuid = source->subvolume[i].uuid;
		target->subvolume[i].subvol_type =
			source->subvolume[i].subvol_type;

#ifdef DEBUG_XLV_TAB
printf("version  %d, addr = %x \n",source->subvolume[i].version,(caddr_t)&(source->subvolume[i].version));

printf("flags  %d, addr = %x \n",source->subvolume[i].flags,(caddr_t)&(source->subvolume[i].flags));

printf("open_flag  %d, addr = %x \n",source->subvolume[i].open_flag,(caddr_t)&(source->subvolume[i].open_flag));

printf("vol_p  %d, addr = %x \n",source->subvolume[i].vol_p,(caddr_t)&(source->subvolume[i].vol_p));

printf("uuid  0x%x, addr = %x \n",source->subvolume[i].uuid,
(caddr_t)&(source->subvolume[i].uuid));

printf("subvol_type  %d, addr = %x \n",source->subvolume[i].subvol_type,(caddr_t)&(source->subvolume[i].subvol_type));

printf("subvol_depth  %d, addr = %x \n",source->subvolume[i].subvol_depth,(caddr_t)&(source->subvolume[i].subvol_depth));
printf("subvol_size  %d, addr = %x \n",source->subvolume[i].subvol_size,(caddr_t)&(source->subvolume[i].subvol_size));
printf("subvol dev  %d, addr = %x \n",source->subvolume[i].dev,(caddr_t)&(source->subvolume[i].dev));
#endif

		target->subvolume[i].subvol_depth =
			source->subvolume[i].subvol_depth;
		target->subvolume[i].subvol_size =
			source->subvolume[i].subvol_size;
		target->subvolume[i].dev =
			source->subvolume[i].dev;

		target->subvolume[i].critical_region.excluded_rqst =
			(caddr_t)to64bit((__psint_t)
			   source->subvolume[i].critical_region.excluded_rqst);
		target->subvolume[i].critical_region.start_blkno =
			source->subvolume[i].critical_region.start_blkno;
		target->subvolume[i].critical_region.end_blkno =
			source->subvolume[i].critical_region.end_blkno;
		target->subvolume[i].critical_region.pending_requests =
			(struct buf *)to64bit((__psint_t)
			 source->subvolume[i].critical_region.pending_requests);

		target->subvolume[i].err_pending_rqst =
			(struct buf *)to64bit((__psint_t)
			   source->subvolume[i].err_pending_rqst);
		target->subvolume[i].rd_wr_back_start =
			source->subvolume[i].rd_wr_back_start;
		target->subvolume[i].rd_wr_back_end =
			source->subvolume[i].rd_wr_back_end;
		target->subvolume[i].initial_read_slice =
			source->subvolume[i].initial_read_slice;
		target->subvolume[i].num_plexes =
			source->subvolume[i].num_plexes;

		for ( j = 0; j < XLV_MAX_PLEXES; j ++) {
			target->subvolume[i].plex[j] =
			     (xlv_tab_plex_t *)to64bit((__psint_t)
				source->subvolume[i].plex[j]);
		}

		target->subvolume[i].block_map =
			(xlv_block_map_t *)to64bit((__psint_t)
			   source->subvolume[i].block_map);
		i++;
		count--;
	}
        return 0;
}

#elif (_MIPS_SIM == _ABIN32)
static int
irix5_o32_to_xlv_tab(
        enum xlate_mode mode,
        void            *to,
        int             count,
        xlate_info_t    *info)
{
	int		i = 0, j;
	
        COPYIN_XLATE_VARYING_PROLOGUE(irix5_o32_xlv_tab_s, xlv_tab_s, 
		sizeof(irix5_o32_xlv_tab_t) +
                ((count-1) * sizeof(irix5_o32_xlv_tab_subvol_t)));

	ASSERT (count > 0);

        target->num_subvols = source->num_subvols;
        target->max_subvols = source->max_subvols;
	
	while (count) {
		target->subvolume[i].version =
			source->subvolume[i].version;
		target->subvolume[i].flags =
			source->subvolume[i].flags;
		target->subvolume[i].open_flag =
			source->subvolume[i].open_flag;
		target->subvolume[i].vol_p =
			(struct xlv_tab_vol_entry_s *)source->subvolume[i].vol_p;
		target->subvolume[i].uuid = source->subvolume[i].uuid;
		target->subvolume[i].subvol_type =
			source->subvolume[i].subvol_type;

#ifdef DEBUG_XLV_TAB
printf("version  %d, addr = %x \n",source->subvolume[i].version,(caddr_t)&(source->subvolume[i].version));

printf("flags  %d, addr = %x \n",source->subvolume[i].flags,(caddr_t)&(source->subvolume[i].flags));

printf("open_flag  %d, addr = %x \n",source->subvolume[i].open_flag,(caddr_t)&(source->subvolume[i].open_flag));

printf("vol_p  %d, addr = %x \n",source->subvolume[i].vol_p,(caddr_t)&(source->subvolume[i].vol_p));

printf("uuid  0x%x, addr = %x \n",source->subvolume[i].uuid,
(caddr_t)&(source->subvolume[i].uuid));

printf("subvol_type  %d, addr = %x \n",source->subvolume[i].subvol_type,(caddr_t)&(source->subvolume[i].subvol_type));

printf("subvol_depth  %d, addr = %x \n",source->subvolume[i].subvol_depth,(caddr_t)&(source->subvolume[i].subvol_depth));
printf("subvol_size  %d, addr = %x \n",source->subvolume[i].subvol_size,(caddr_t)&(source->subvolume[i].subvol_size));
printf("subvol dev  %d, addr = %x \n",source->subvolume[i].dev,(caddr_t)&(source->subvolume[i].dev));
#endif

		target->subvolume[i].subvol_depth =
			source->subvolume[i].subvol_depth;
		target->subvolume[i].subvol_size =
			source->subvolume[i].subvol_size;
		target->subvolume[i].dev =
			source->subvolume[i].dev;

		target->subvolume[i].critical_region.excluded_rqst =
			(caddr_t) source->subvolume[i].critical_region.excluded_rqst;
		target->subvolume[i].critical_region.start_blkno =
			source->subvolume[i].critical_region.start_blkno;
		target->subvolume[i].critical_region.end_blkno =
			source->subvolume[i].critical_region.end_blkno;
		target->subvolume[i].critical_region.pending_requests =
			(struct buf *)source->subvolume[i].critical_region.pending_requests;

		target->subvolume[i].err_pending_rqst =
			(struct buf *)source->subvolume[i].err_pending_rqst;
		target->subvolume[i].rd_wr_back_start =
			source->subvolume[i].rd_wr_back_start;
		target->subvolume[i].rd_wr_back_end =
			source->subvolume[i].rd_wr_back_end;
		target->subvolume[i].initial_read_slice =
			source->subvolume[i].initial_read_slice;
		target->subvolume[i].num_plexes =
			source->subvolume[i].num_plexes;

		for ( j = 0; j < XLV_MAX_PLEXES; j ++) {
			target->subvolume[i].plex[j] =
			     (xlv_tab_plex_t *)source->subvolume[i].plex[j];
		}

		target->subvolume[i].block_map =
			(xlv_block_map_t *)source->subvolume[i].block_map;
		i++;
		count--;
	}
        return 0;
}

#endif


static int
irix5_n32_to_xlv_tab(
        enum xlate_mode mode,
        void            *to,
        int             count,
        xlate_info_t    *info)
{
	int		i = 0, j;
	
        COPYIN_XLATE_VARYING_PROLOGUE(irix5_n32_xlv_tab_s, xlv_tab_s, 
		sizeof(irix5_n32_xlv_tab_t) +
                ((count-1) * sizeof(irix5_n32_xlv_tab_subvol_t)));

	ASSERT (count > 0);

        target->num_subvols = source->num_subvols;
        target->max_subvols = source->max_subvols;
	
	while (count) {
		target->subvolume[i].version =
			source->subvolume[i].version;
		target->subvolume[i].flags =
			source->subvolume[i].flags;
		target->subvolume[i].open_flag =
			source->subvolume[i].open_flag;
#if (_MIPS_SIM == _ABI64)
		target->subvolume[i].vol_p =
			(struct xlv_tab_vol_entry_s *)to64bit((__psint_t)
			     source->subvolume[i].vol_p);
#else
		target->subvolume[i].vol_p =
			(struct xlv_tab_vol_entry_s *)
			     source->subvolume[i].vol_p;
#endif
		target->subvolume[i].uuid = source->subvolume[i].uuid;
		target->subvolume[i].subvol_type =
			source->subvolume[i].subvol_type;

#ifdef DEBUG_XLV_TAB
printf("version  %d, addr = %x \n",source->subvolume[i].version,(caddr_t)&(source->subvolume[i].version));

printf("flags  %d, addr = %x \n",source->subvolume[i].flags,(caddr_t)&(source->subvolume[i].flags));

printf("open_flag  %d, addr = %x \n",source->subvolume[i].open_flag,(caddr_t)&(source->subvolume[i].open_flag));

printf("vol_p  %d, addr = %x \n",source->subvolume[i].vol_p,(caddr_t)&(source->subvolume[i].vol_p));

printf("uuid  0x%x, addr = %x \n",source->subvolume[i].uuid,
(caddr_t)&(source->subvolume[i].uuid));

printf("subvol_type  %d, addr = %x \n",source->subvolume[i].subvol_type,(caddr_t)&(source->subvolume[i].subvol_type));

printf("subvol_depth  %d, addr = %x \n",source->subvolume[i].subvol_depth,(caddr_t)&(source->subvolume[i].subvol_depth));
printf("subvol_size  %d, addr = %x \n",source->subvolume[i].subvol_size,(caddr_t)&(source->subvolume[i].subvol_size));
printf("subvol dev  %d, addr = %x \n",source->subvolume[i].dev,(caddr_t)&(source->subvolume[i].dev));
#endif

		target->subvolume[i].subvol_depth =
			source->subvolume[i].subvol_depth;
		target->subvolume[i].subvol_size =
			source->subvolume[i].subvol_size;
		target->subvolume[i].dev =
			source->subvolume[i].dev;

#if (_MIPS_SIM == _ABI64)
		target->subvolume[i].critical_region.excluded_rqst =
			(caddr_t)to64bit((__psint_t)
			   source->subvolume[i].critical_region.excluded_rqst);
#else
		target->subvolume[i].critical_region.excluded_rqst =
			(caddr_t)
			   source->subvolume[i].critical_region.excluded_rqst;
#endif
		target->subvolume[i].critical_region.start_blkno =
			source->subvolume[i].critical_region.start_blkno;
		target->subvolume[i].critical_region.end_blkno =
			source->subvolume[i].critical_region.end_blkno;
#if (_MIPS_SIM == _ABI64)
		target->subvolume[i].critical_region.pending_requests =
			(struct buf *)to64bit((__psint_t)
			 source->subvolume[i].critical_region.pending_requests);
#else
		target->subvolume[i].critical_region.pending_requests =
			(struct buf *)
			 source->subvolume[i].critical_region.pending_requests;
#endif

#if (_MIPS_SIM == _ABI64)
		target->subvolume[i].err_pending_rqst =
			(struct buf *)to64bit((__psint_t)
			   source->subvolume[i].err_pending_rqst);
#else
		target->subvolume[i].err_pending_rqst =
			(struct buf *)source->subvolume[i].err_pending_rqst;
#endif
		target->subvolume[i].rd_wr_back_start =
			source->subvolume[i].rd_wr_back_start;
		target->subvolume[i].rd_wr_back_end =
			source->subvolume[i].rd_wr_back_end;
		target->subvolume[i].initial_read_slice =
			source->subvolume[i].initial_read_slice;
		target->subvolume[i].num_plexes =
			source->subvolume[i].num_plexes;

		for ( j = 0; j < XLV_MAX_PLEXES; j ++) {
#if (_MIPS_SIM == _ABI64)
			target->subvolume[i].plex[j] =
			     (xlv_tab_plex_t *)to64bit((__psint_t)
				source->subvolume[i].plex[j]);
#else
			target->subvolume[i].plex[j] =
			     (xlv_tab_plex_t *)source->subvolume[i].plex[j];
#endif
		}

#if (_MIPS_SIM == _ABI64)
		target->subvolume[i].block_map =
			(xlv_block_map_t *)to64bit((__psint_t)
			   source->subvolume[i].block_map);
#else
		target->subvolume[i].block_map =
			(xlv_block_map_t *)source->subvolume[i].block_map;
#endif
		i++;
		count--;
	}
        return 0;

} /* end of irix5_n32_to_xlv_tab() */



#ifdef DEBUG_XLV_TAB
static char * xfs_fmtuuid(uuid_t *uu);

void
print_xlv_tab_vol (xlv_tab_vol_t *k_vtab)
{
	xlv_tab_vol_entry_t	*v;

	v = &k_vtab->vol[0];
	printf ("version: %d, name: %s, uuid: %s\n",
		v->version, v->name, xfs_fmtuuid(&(v->uuid)));
	printf ("flags: %d, state: %d, rsvd: %d, sector_sz: %d\n",
		v->flags, v->state, v->rsvd, v->sector_size);
	printf ("log: 0x%x, data: 0x%x, rt: 0x%x, nodename: 0x%x\n",
		v->log_subvol, v->data_subvol, v->rt_subvol, v->nodename);

}

void
print_xlv_tab_entry (xlv_tab_subvol_t *sv)
{
	if (sv->vol_p == NULL) {
		printf ("EMPTY SUBVOL ENTRY\n");
		return;
	}
	printf ("version: %d, flags: %d, open_flags: %d, vol_p: 0x%x\n",
		sv->version, sv->flags, sv->open_flag, sv->vol_p);
	printf ("uuid: %s, type: %d, depth: %d, size: %d, dev: %x\n",
		xfs_fmtuuid(&(sv->uuid)), sv->subvol_type,
		sv->subvol_depth, sv->subvol_size, sv->dev);
	/*
	 * Critical region stuff.
	 */
	printf ("num_plexes: %d, plex0: 0x%x, plex1: 0x%x, plex2: 0x%x\n",
		sv->num_plexes, sv->plex[0], sv->plex[1], sv->plex[2]);

}

void
print_xlv_tab_array (xlv_tab_t	*k_svtab)
{
	int	i;

	for (i=0; i<k_svtab->max_subvols; i++) {
		printf ("-- subvol[%d] --\n", i);
		print_xlv_tab_entry (&k_svtab->subvolume[i]);
	}
}

static char *
xfs_fmtuuid(uuid_t *uu)
{
        static char rval[40];
        uint *ip = (uint *)uu;

        ASSERT(sizeof(*uu) == 16);
        sprintf(rval, "%8x:%8x:%8x:%8x", ip[0], ip[1], ip[2], ip[3]);
        return rval;
}

#endif /* DEBUG_XLV_TAB */
