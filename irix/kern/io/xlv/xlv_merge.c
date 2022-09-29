/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.30 $"

/*
 * Subroutine to merge a new volume configuration into the running
 * system.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/sysmacros.h>
#include <sys/open.h>
#include <sys/debug.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"


/* 
 * The driver opens all the underlying physical devices when a
 * logical volume is opened.
 * When we replace an open subvolume with a new configuration,
 * we need to compute an intersection of the physical devices that
 * make up both subvolumes.  Those devices in the current subvolume
 * that are no longer part of the configuration must be closed, 
 * newly added devices must be opened, and the rest can remain
 * open. 
 *
 * We do this by enumerating and sorting the physical devices that 
 * make up the current and new configurations.
 */

static void enumerate_devices (xlv_tab_subvol_t *new_sv, 
			       xlv_pdev_list_t	pdev_list,
			       boolean_t	in_current_config,
			       int		*n_pdev);

void qsort (void* base, size_t nel, size_t width,
	    int (*compar)(const void *, const void *));

#if !defined(SIM) && !defined(USER_MODE)

extern int grio_purge_vdisk_cache( dev_t );

#endif

static int xlv_tab_pdev_subvol (xlv_tab_subvol_t *new_sv, 
				xlv_tab_subvol_t *old_sv,
				xlv_pdev_list_t  pdev_list);

/*
 * Merge the each subvolume of the current configuration into
 * the new configuration.
 */
int
xlv_tab_merge (
	xlv_tab_t	*new_xlv_tab,
	xlv_tab_t	*old_xlv_tab,
	int		num_subvols,
	xlv_pdev_list_t	pdev_list)
{
	int                 	s, error;
	xlv_tab_subvol_t    	*sv;

	/*
	 * Go through the current subvolumes.  If a subvolume is known
	 * to the kernel, but not in the new subvolume table, then fail
	 * since we don't want to remove kernel configurations this way.
	 *
	 * If the current subvolumes are closed, then there is no state 
	 * that we need to capture, the new subvol entry will do.  If the 
	 * old subvolume is currently open, copy its state into the new 
	 * subvol entry.
	 *
	 * Must check that current subvolume is in the new table.
	 * The subvolume could have been implicitly removed by the
	 * user setting a new configuration which does not have an
	 * entry for that index. Also take into account the old and
	 * new table size.
	 */

	error = 0;
	for (s=0; s < num_subvols && s < new_xlv_tab->max_subvols; s++) {

		sv = &(old_xlv_tab->subvolume[s]);

		/* old exists, but not the new */
		if (XLV_SUBVOL_EXISTS(sv) && 
		    !XLV_SUBVOL_EXISTS(&new_xlv_tab->subvolume[s])) {
			error = XLVE_SUBVOL_INCOMPLETE;
			cmn_err(CE_WARN, "Existing volume %s not found.", 
				sv->vol_p->name);
			goto end_of_loop;
		}

		/* old and new exist, but the old is open */
		if (XLV_SUBVOL_EXISTS(sv) && XLV_ISOPEN(sv->flags)) {

			/*
			 * We need to merge the state.
			 */

			error = xlv_tab_pdev_subvol (
					&(new_xlv_tab->subvolume[s]),
					sv, pdev_list);
			if (error) {
				goto end_of_loop;
			}

	    	}

#if !defined(SIM) && !defined(USER_MODE)
		else if (sv->dev != 0) {
			grio_purge_vdisk_cache(sv->dev);
		}
#endif

	}

end_of_loop:
	return (error);
}


/*
 * Iterator function that closes all the underlying devices 
 * associated with a volume element.
 */
static int
ve_close_dev_func (xlv_tab_vol_elmnt_t *ve, unsigned *open_flag)
{
	int		d;
	dev_t		dev;
	struct bdevsw	*my_bdevsw;

	for (d = 0; d < ve->grp_size; d++) {
		dev = DP_DEV(ve->disk_parts[d]);
		binval(dev);
		my_bdevsw = get_bdevsw(dev);
		ASSERT(my_bdevsw != NULL);
		bdclose (my_bdevsw, dev, *open_flag, OTYP_LYR, get_current_cred());
	}

	return 0;	/* continue with next volume element */
}


/*
 * Close all the underlying devices associated with the volumes.
 */
int
xlv_tab_close_devs (xlv_tab_t *old_xlv_tab,
                    int       num_subvols)
{
        int                     s;
        xlv_tab_subvol_t        *sv;

	/*
	 * Notice how we skip over the root volume (minor dev 0).
	 */
        for (s=1; s < num_subvols; s++) {

                sv = &(old_xlv_tab->subvolume[s]);
                if (XLV_SUBVOL_EXISTS(sv) && XLV_ISOPEN(sv->flags)) {
			xlv_for_each_ve (sv, (XLV_VE_FP)ve_close_dev_func,
					 (void *) &sv->open_flag);

			grio_purge_vdisk_cache(sv->dev);
                }
	}
	return( 0 );
}

/*
 * Iterator function that sees if the volume element UUID matches
 * that of the input volume element. If so, copy the flag bits.
 *
 * ve is the old (currently configured and running) volume element
 * target_ve is the new volume element
 */
static int
ve_uuid_eq_func (xlv_tab_vol_elmnt_t *ve, xlv_tab_vol_elmnt_t *target_ve)
{
	unsigned	st;

	if (uuid_equal (&ve->uuid, &target_ve->uuid, &st)) {
		target_ve->flags = ve->flags;
			
		/*
		 * If the kernel thinks the volume element is "offline"
		 * then the volume element should stay offline. Don't
		 * want xlv_assemble(1m) to accidently change the volume
		 * element state to "active".
		 *
		 * 10/27/95	Since xlv can now do partial label updates,
		 * all the other volume elements should have newer timestamps
		 * and this offline'd ve could not be made/chosen "active"
		 * by xlv_assemble(1m). Let just be safe and explicitly
		 * check the state.
		 */ 
		if (XLV_VE_STATE_OFFLINE == ve->state) {
			/*
			 * Keep ve offline if the new subvol has this
			 * ve as active. Any other state is okay.
			 * By doing this, one can unplug and replug
			 * the disk and just do an xlv_assemble(1m)
			 * to get things going again.
			 */
			if (XLV_VE_STATE_ACTIVE == target_ve->state) {
				target_ve->state = XLV_VE_STATE_OFFLINE;
			}
		}

		return 1;	/* found it, discontinue search */
	}
	else
		return 0;
}

/*
 * Iterator function that is applied to every volume element in the
 * new xlv_sv. It searches the old xlv_sv to see if there is a
 * volume element whose uuid matches that of the [new/target] volume
 * element.  If so, copy over the flags bits.
 *
 * Note that we implement this as a nested iterator function. This
 * is order n squared performance, but given the number of expected
 * volume elements, this should be ok.
 */
static int
ve_copy_state_func (xlv_tab_vol_elmnt_t *target_ve, xlv_tab_subvol_t *sv)
{
	xlv_for_each_ve (sv, (XLV_VE_FP)ve_uuid_eq_func, target_ve);

	return 0;
}


/*
 * Comparator function for quicksort().
 * Sorts on the device number.
 */
static int
dev_cmpr (xlv_pdev_node_t *a, xlv_pdev_node_t *b)
{
	if (a->dev < b->dev)
		return (-1);
	if (a->dev > b->dev)
		return (1);
	return (0);
}

/*
 * Merge the state for a single subvolume.
 */
static int
xlv_tab_pdev_subvol (
	xlv_tab_subvol_t	*new_sv,
	xlv_tab_subvol_t	*old_sv,
	xlv_pdev_list_t		pdev_list)
{
	int	n_pdev = 0, changed = 0;
	int	i, error;
	int	flag = old_sv->open_flag;
	struct bdevsw *my_bdevsw;

	/*
	 * Preserve the current subvol state.
	 */
	new_sv->flags = old_sv->flags;
	new_sv->open_flag = old_sv->open_flag;
	new_sv->critical_region = old_sv->critical_region;
	new_sv->err_pending_rqst = old_sv->err_pending_rqst;
	new_sv->rd_wr_back_start = old_sv->rd_wr_back_start;
	new_sv->rd_wr_back_end = old_sv->rd_wr_back_end;

	/*
         * Preserve the existing volume element state.
         */
	/*
	 * XXX Wei. We don't preserve the existing volume element
	 * state.  (vol_elmnt->state & flags). This means that
	 * if a plex revive is in progress, you'd loose all partial
	 * results.
	 * That's ok as the new configuration will likely trigger
	 * a plex revive any way.
	 */
	xlv_for_each_ve (new_sv, (XLV_VE_FP)ve_copy_state_func, old_sv);

	/*
	 * Now, open/close the underlying devices as required.
	 */
	enumerate_devices (new_sv, pdev_list, B_FALSE, &n_pdev);
	enumerate_devices (old_sv, pdev_list, B_TRUE, &n_pdev);
	pdev_list[n_pdev].dev = 0;
	qsort (pdev_list, n_pdev, sizeof(xlv_pdev_node_t), 
		(int (*)(const void *, const void *))dev_cmpr);

	i = 0;
	while (i < n_pdev) {
		if (pdev_list[i].dev == pdev_list[i+1].dev) {
			/*
			 * This physical disk is part of the current and new
			 * configurations.  Leave it open, no change needed.
			 */
			i += 2;
			continue;
		}
		if (pdev_list[i].in_current_config) {
			/*
			 * This physical disk is part of the current (running)
			 * configuration but not part of the new.  Close it.
			 */
			binval(pdev_list[i].dev);
			my_bdevsw = get_bdevsw(pdev_list[i].dev);
			ASSERT(my_bdevsw != NULL);
			error = bdclose (my_bdevsw,
					 pdev_list[i].dev, flag,
					 OTYP_LYR, get_current_cred());
			if (error) {
				goto backout;
			}
			changed = 1;
		}
		else {
			/*
			 * This physical disk is not currently part of the 
			 * subvolume but is being added.  Open it.
			 */
			my_bdevsw = get_bdevsw(pdev_list[i].dev);

			if (error = bdhold(my_bdevsw)) {
				goto backout;
			}

			error = bdopen (my_bdevsw,
				        &(pdev_list[i].dev), flag, 
					OTYP_LYR, get_current_cred());

			bdrele(my_bdevsw);
			if (error) {
				goto backout;
			}
			changed = 1;
		}
		i++;
	} /* while (i < n_pdev) */

#if !defined(SIM) && !defined(USER_MODE)

	if (changed) {
	   	grio_purge_vdisk_cache(new_sv->dev);
	}

#endif
	return (0);

backout:
	/*
	 * XXX We should step through pdev array and back out any changes
	 * we've made.  Defer this for now.
	 */
	/* ASSERT (0); */

	return (error);

} /* end of xlv_tab_pdev_subvol() */


typedef struct {
	int		n_pdev;
	xlv_pdev_node_t	*pdev_list;
	boolean_t	in_current_config;
} list_devs_arg_t;

/* 
 * Function to process each disk part that make up a volume
 * element. 
 * Designed to be called in conjunction with xlv_for_each_ve.
 */
static int 
list_devs_func (xlv_tab_vol_elmnt_t *ve, list_devs_arg_t *arg)
{
	int d, n;

	/*
	 * XXXjleong If this ve is offline or incomplete, the disks in
	 * the ve may be closed so we may want to ignore these disks.
	 */

	n = arg->n_pdev;

	for (d=0; d < ve->grp_size; d++) {
		arg->pdev_list[n].in_current_config = 
			arg->in_current_config;
		arg->pdev_list[n].dev = DP_DEV(ve->disk_parts[d]);
		n++;
	}

	arg->n_pdev = n;

	return 0;		/* continue with next vol element */
}


/* 
 * Copy all the disk parts that make up a subvolume.
 * The device numbers are copied into pdev_list starting
 * at offset n_pdev.  Every pdev_list entry will have 
 * in_current_config set to input parameter.
 * Upon exit, n_pdev will point to the next available
 * slot in pdev_list.
 */
static void
enumerate_devices (xlv_tab_subvol_t *new_sv,
		   xlv_pdev_list_t  pdev_list,
		   boolean_t	    in_current_config,
		   int		    *n_pdev)
{
	list_devs_arg_t	e_arg;

	e_arg.pdev_list = pdev_list;
	e_arg.in_current_config = in_current_config;
	e_arg.n_pdev = *n_pdev;
	xlv_for_each_ve (new_sv, (XLV_VE_FP)list_devs_func, &e_arg);

	*n_pdev = e_arg.n_pdev;
}


/*
 * An iterator function that goes through a subvolume and
 * applies a caller-supplied function to each volume element
 * until either: 1) we've gone through all the volume elements,
 * or, 2) the caller-supplied function returns 1.  (This latter
 * feature allows us to terminate a search after we find a
 * particular volume element.)
 */
void
xlv_for_each_ve (xlv_tab_subvol_t *sv,	/* subvolume to iterate over */
		 XLV_VE_FP operation,	/* func to apply to each ve */
		 void *arg)		/* argument to pass to func */
{
	
	int 		p, ve;
	xlv_tab_plex_t	*plex;

	for (p=0; p < XLV_MAX_PLEXES; p++) {
		if ((plex = sv->plex[p]) == NULL) 
			continue;

		for (ve=0; ve < plex->num_vol_elmnts; ve++) {
			if ((*operation)(&(plex->vol_elmnts[ve]), arg))
				goto done;
		}
	}

done:
	;
}

/*
 * Return the volume element to which a physical device belongs.
 */

typedef struct {
	dev_t			dev;	/* input */
	xlv_tab_vol_elmnt_t	*ve;	/* output */
} ve_from_dev_arg_t;


STATIC int
ve_from_dev_func (
	xlv_tab_vol_elmnt_t	*ve,
	ve_from_dev_arg_t	*ve_from_dev_arg)
{
	int	d;

	for (d = 0; d < ve->grp_size; d++) {
		if (ve->disk_parts[d].dev[ve->disk_parts[d].active_path]
		    == ve_from_dev_arg->dev) {
			ve_from_dev_arg->ve = ve;
			return 1;	/* terminate search */
		}
	}

	return 0;	/* continue searching */
}


xlv_tab_vol_elmnt_t *
xlv_vol_elmnt_from_disk_part (
	xlv_tab_subvol_t	*sv,
	dev_t			dev)
{
	ve_from_dev_arg_t	ve_from_dev_arg;

	ve_from_dev_arg.dev = dev;
	ve_from_dev_arg.ve = NULL;

	xlv_for_each_ve (sv, (XLV_VE_FP)ve_from_dev_func, &ve_from_dev_arg);

	return (ve_from_dev_arg.ve);
}
