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
#ident "$Revision: 1.54 $"

/*
 * Kernel routines to implement the syssgi(2) xlv calls.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/capability.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/sysmacros.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_lock.h>
#include <sys/xlv_attr.h>
#include <sys/xlv_stat.h>
#include <sys/xlate.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"
#include "xlv_xlate.h"
#include "xlv_mem.h"


#if (_MIPS_SIM == _ABI64)
#define	KERN_ABI	ABI_IRIX5_64
#elif (_MIPS_SIM == _ABIN32)
#define	KERN_ABI	ABI_IRIX5_N32
#elif (_MIPS_SIM == _ABIO32)
#define	KERN_ABI	ABI_IRIX5
#endif


#define	RDWR_VE_STATE(st)	\
	(((st) != XLV_VE_STATE_OFFLINE) && ((st) != XLV_VE_STATE_INCOMPLETE))

#define	NO_RDWR_VE_STATE(st)	\
	(((st) == XLV_VE_STATE_OFFLINE) || ((st) == XLV_VE_STATE_INCOMPLETE))


typedef struct xlv_obj_path {
	int	generation;
	int	vol;
	int	subvol;
	int	plex;
	int	vol_elmnt;
	int	disk_part;
} xlv_obj_path_t;


/*
 * Master incore database; shared by the driver.
 */
extern xlv_tab_vol_t	*xlv_tab_vol;
extern xlv_tab_t	*xlv_tab;
extern int		xlv_config_generation;
extern int		xlv_maxlocks;
extern int		xlv_maxunits;
extern int		xlv_plexing_support_present;
extern dev_t		xlv_rootdisk;

extern int		xlv_failsafe;

static __uint64_t	_xlv_sv_maxsize64 = 0x7FFFFFFFFFFFFFFFLL;
static __uint32_t	_xlv_sv_maxsize32 = 0x7FFFFFFF;
__uint64_t		xlv_sv_maxsize = 0;

/*
 * xlv_config_generation is set in xlv_tab_set().
 *
 * In this module, xlv_config_generation is only set when
 * holding the "config" lock.
 */

extern void xlv_configchange_notification(void);

static int _find_obj_in_subvol(
	uuid_t		*uuidp,		/* identifies the object itself */
	int		type,		/* object type */
	xlv_obj_path_t	*path);		/* coordinate of the object */

static int copyin_ve (xlv_tab_vol_elmnt_t *u_src, xlv_tab_vol_elmnt_t **k_dst);
static int copyin_plex    (xlv_tab_plex_t *u_src, xlv_tab_plex_t **k_dst);
static int copyin_subvol  (xlv_tab_subvol_t *u_src, xlv_tab_subvol_t **k_dst);
static int copyin_vol (xlv_tab_vol_entry_t *u_src, xlv_tab_vol_entry_t **k_dst);

static int copyout_subvol (xlv_tab_subvol_t *k_src, xlv_tab_subvol_t *u_dst);
static int copyout_vol (xlv_tab_vol_entry_t *k_src, xlv_tab_vol_entry_t *u_dst);

static int xlv_attr_cmd_add	(xlv_attr_req_t *req, xlv_obj_path_t *path);
static int xlv_attr_cmd_remove	(xlv_attr_req_t *req, xlv_obj_path_t *path);
static int xlv_attr_cmd_set	(xlv_attr_req_t *req, xlv_obj_path_t *path);
static int xlv_attr_cmd_notify	(void);

#if _MIPS_SIM == _ABI64
/*
 * ABI copyin/copyout translation routines.
 */
static int irix5_to_xlv_attr_req (
	enum xlate_mode mode, void *to, int count, xlate_info_t *info);
#endif

static void
_free_subvol(xlv_tab_subvol_t *svp)
{
 	int p;

	if (svp == NULL)
		return;

	for (p=0; p < XLV_MAX_PLEXES; p++) {
		if (svp->plex[p] == NULL)
			continue;
		kmem_free (svp->plex[p], 0);
	}

	kmem_free(svp, 0);
}

static void
_free_vol(xlv_tab_vol_entry_t *vp)
{
	if (vp == NULL)
		return;

	if (vp->log_subvol)
		_free_subvol(vp->log_subvol);
	if (vp->data_subvol)
		_free_subvol(vp->data_subvol);
	if (vp->rt_subvol)
		_free_subvol(vp->rt_subvol);

	kmem_free(vp, 0);
}




/*
 * Copy initial xlv table cursor information out to user space.
 */
int
xlv_attr_get_cursor(xlv_attr_cursor_t *u_cursor)
{
	xlv_attr_cursor_t	k_cursor;

	k_cursor.generation = xlv_config_generation;
	k_cursor.vol = -1;
	k_cursor.subvol = -1;
	k_cursor.plex = -1;
	k_cursor.ve = -1;

	if (copyout((caddr_t)&k_cursor, (caddr_t)u_cursor, sizeof(k_cursor)))
		return (EFAULT);

	return (0);
}

/*
 * xlv_attr_get() retrieves information for the "next" object.
 * The "cursor" is used to specify the current object. When
 * the enumeration is of all attribute objects (vol/subvol/plex/ve)
 * is complete, an error of ENFILE is returned. 
 *
 * Errors:
 *	EPERM  - user lack permission.
 *	EINVAL - cannot copyin user arguments to kernel space.
 *	ESTALE - configuration changed since last cursor update.
 *	ENOENT - XLV configuration has not been set/assembled.
 *	EFAULT - copyout to user space failed.
 *	ENFILE - reach pass end of XLV table (enumeration is done).
 */
int
xlv_attr_get(xlv_attr_cursor_t *u_cursor, xlv_attr_req_t *u_req)
{
	xlv_attr_cursor_t	k_cursor;
	xlv_attr_req_t		k_req;
	uint_t			status;
	int			error = 0;
	int			i;

#if 0
	/*
	 * Should not need to be super user to get information.
	 *
	 * if (! _CAP_ABLE(CAP_DEVICE_MGT)) {
	 * 	return (EPERM);
	 * }
	 */
#endif

	if ((u_cursor == NULL) || (u_req == NULL)) {
		return (EFAULT);
	}
	if (copyin((caddr_t)u_cursor, (caddr_t)&k_cursor, sizeof(k_cursor)) ||
	    COPYIN_XLATE((caddr_t)u_req, (caddr_t)&k_req,
			 sizeof(k_req), irix5_to_xlv_attr_req,
			 get_current_abi(), 1)) {
			 /*
			  * Note: called macro instead of copyin_xlate()
			  * becasuse o32 and n32 are the same.
			  */
		return (EINVAL);
	}
	if (k_cursor.generation != xlv_config_generation) {
		/*
		 * The XLV volume configuration has changed since the
		 * cursor's last update. User will need to get a new
		 * cursor.
		 */
		return (ESTALE);
	}

	/*
	 * XXX  Should we acquire the lock in access mode to block
	 * configuration changes?
	 */

	/*
	 * k_cursor.cmd == XLV_ATTR_CMD_QUERY
	 *
	 * Some attribute requires that the XLV tables be set.
	 * i.e. xlv_assemble(1m) has been invoked
	 */

	switch (k_req.attr) {
	case XLV_ATTR_FLAGS:
		k_req.ar_flag1 = (xlv_stats_enabled) ? XLV_FLAG_STAT : 0;
		k_req.ar_flag2 = (xlv_failsafe) ? XLV_FLAG2_FAILSAFE : 0;
		if (copyout((caddr_t)&k_req, (caddr_t)u_req, sizeof(k_req)))
			error = EFAULT;
		break;
	case XLV_ATTR_STATS:
		/*
		 * Only return statistics for existing subvolumes
		 */
		if (xlv_tab == NULL || xlv_tab_vol == NULL) {
			return(ENOENT);
		} else if (k_cursor.subvol >= xlv_maxunits) {
			return(ENFILE);
		} else if (!xlv_tab->subvolume[k_cursor.subvol].vol_p) {
			return(ENOENT);
		}
		ASSERT(xlv_io_lock[k_cursor.subvol].statp);
		if (copyout(xlv_io_lock[k_cursor.subvol].statp,
			    k_req.ar_statp, sizeof(xlv_stat_t))) {
			return(EFAULT);
		}
		return(0);	/* nothing else to do */
	case XLV_ATTR_TAB_SIZE:
		if (xlv_tab == NULL || xlv_tab_vol == NULL) {
			return (ENOENT);
		}
		k_req.ar_tab_size = xlv_tab->num_subvols;
		k_req.ar_tab_vol_size = xlv_tab_vol->num_vols;
		if (copyout((caddr_t)&k_req, (caddr_t)u_req, sizeof(k_req)))
			error = EFAULT;
		break;

	case XLV_ATTR_LOCKS:
		k_req.ar_max_locks = xlv_maxlocks;
		k_req.ar_num_locks = xlv_maxunits;
		if (copyout((caddr_t)&k_req, (caddr_t)u_req, sizeof(k_req)))
			error = EFAULT;
		break;

	case XLV_ATTR_SUPPORT:
		k_req.ar_supp_plexing = xlv_plexing_support_present;
		k_req.ar_aru.aru_support.pad = 0;
		if (copyout((caddr_t)&k_req, (caddr_t)u_req, sizeof(k_req)))
			error = EFAULT;
		break;

	case XLV_ATTR_MEMORY:
	{
		xlv_attr_memory_t usr_mem;
		xlv_res_mem_t	  *res_mem;

		switch(k_req.cmd) {
		case XLV_ATTR_CMD_PLEXMEM:
			res_mem = &xlv_top_mem;
			break;
		case XLV_ATTR_CMD_VEMEM:
			res_mem = &xlv_low_mem;
			break;
		default:
			error = EINVAL;
			break;
		}

		if (!error) {
			usr_mem.size = res_mem->size;
			usr_mem.slots = res_mem->slots;
			usr_mem.hits = res_mem->totalhits + res_mem->hits;
			usr_mem.misses = res_mem->totalmisses + res_mem->misses;
			usr_mem.waits = res_mem->waits;
			usr_mem.resized = res_mem->resized;
			usr_mem.scale = res_mem->scale;
			usr_mem.threshold = res_mem->threshold;

			if (copyout((caddr_t)&usr_mem, (caddr_t)k_req.ar_mem, 
				    sizeof(xlv_attr_memory_t)))
				error = EFAULT;
		}
		break;
	}
	case XLV_ATTR_ROOTDEV:
		k_req.ar_rootdev = xlv_rootdisk;
		k_req.ar_aru.aru_rootdev.pad = 0;
		if (copyout((caddr_t)&k_req, (caddr_t)u_req, sizeof(k_req)))
			error = EFAULT;
		break;


	case XLV_ATTR_SV_MAXSIZE:
		if (0 == xlv_sv_maxsize) {
			xlv_sv_maxsize = (4 == sizeof(daddr_t)) ?
				_xlv_sv_maxsize32 : _xlv_sv_maxsize64;
		}
		k_req.ar_sv_maxsize = xlv_sv_maxsize;
		if (copyout((caddr_t)&k_req, (caddr_t)u_req, sizeof(k_req)))
			error = EFAULT;
		break;

	case XLV_ATTR_VOL:
		if (xlv_tab == NULL || xlv_tab_vol == NULL) {
			return (ENOENT);
		}
		for (i = k_cursor.vol+1; i < xlv_tab_vol->max_vols; i++) {
			if (! uuid_is_nil(&xlv_tab_vol->vol[i].uuid, &status))
				break;
		}
		if (i >= xlv_tab_vol->max_vols) {
			error = ENFILE;
			goto done;
		}
		error = copyout_vol(&xlv_tab_vol->vol[i], k_req.ar_vp);
		k_cursor.vol = i;
		break;

	case XLV_ATTR_SUBVOL:
		if (xlv_tab == NULL || xlv_tab_vol == NULL) {
			return (ENOENT);
		}
		for (i = k_cursor.subvol+1; i < xlv_tab->max_subvols; i++) {
			if (xlv_tab->subvolume[i].vol_p)
				break;
		}
		if (i >= xlv_tab->max_subvols) {
			error = ENFILE;
			goto done;
		}
		error = copyout_subvol(&xlv_tab->subvolume[i], k_req.ar_svp);
		k_cursor.subvol = i;
		break;

	case XLV_ATTR_PLEX:
		if (xlv_tab == NULL || xlv_tab_vol == NULL) {
			return (ENOENT);
		}
		error = ENOSYS;		/* XXX TBD Plex enumeration needed? */
		break;
	case XLV_ATTR_VE:
		if (xlv_tab == NULL || xlv_tab_vol == NULL) {
			return (ENOENT);
		}
		error = ENOSYS;		/* XXX TBD Ve enumeration needed? */
		break;
	default:
		error = EINVAL;
	}

	if (!error) {
		/*
		 * Update cursor. If the cursor is not updated, user
		 * can go into an infinite loop doing the enumeration.
		 */
		copyout((caddr_t)&k_cursor, (caddr_t)u_cursor,
			sizeof(k_cursor));
	}

done:
	return (error);

} /* end of xlv_attr_get() */


/*
 * _find_obj_in_subvol() finds the coordinate of the object with
 * the given uuid. The coordinate of the object is put into "path".
 *
 * Return 1 if the object (of the specified type and uuid) is found,
 * otherwise return 0.
 *
 * Note: The object cannot be a volume.
 */
static int
_find_obj_in_subvol(
	uuid_t		*uuidp,		/* identifies the object itself */
	int		type,		/* object type */
	xlv_obj_path_t	*path)		/* coordinate of the object */
{
	int			sv, p, ve;
	int			found;
	unsigned int		st;
	xlv_tab_subvol_t	*svp;
	xlv_tab_plex_t		*pp;
	xlv_tab_vol_elmnt_t	*vep;

	found = 0;
	p = ve = -1;

	for (sv=0; (sv < xlv_maxunits); sv++) {
		svp = &(xlv_tab->subvolume[sv]);
		xlv_acquire_io_lock(sv, NULL);
		if (XLV_SUBVOL_EXISTS(svp)) {
			switch (type) {
			case XLV_OBJ_TYPE_SUBVOL:
			case XLV_OBJ_TYPE_LOG_SUBVOL:
			case XLV_OBJ_TYPE_DATA_SUBVOL:
			case XLV_OBJ_TYPE_RT_SUBVOL:
				if (uuid_equal(&svp->uuid, uuidp, &st))
					found++;
				break;

			case XLV_OBJ_TYPE_PLEX:
				for (p=0; p < XLV_MAX_PLEXES; p++) {
					if (!(pp = svp->plex[p]))
						continue;
					if (uuid_equal (&pp->uuid,
							uuidp, &st)) {
						found++;
						goto done;
					}
				}
				break;

			case XLV_OBJ_TYPE_VOL_ELMNT:
				for (p=0; p < XLV_MAX_PLEXES; p++) {
					if (!(pp = svp->plex[p]))
						continue;
					for (ve=0;ve<pp->num_vol_elmnts;ve++) {
						vep = &pp->vol_elmnts[ve];
						if (uuid_equal (&vep->uuid,
								uuidp, &st)) {
							found++;
							goto done;
						}
					}
				}
				break;

			} /* switch */ 
		} 
done:
		xlv_release_io_lock(sv);

		if (found)
			break;
	}

	if (found) {
		path->generation = xlv_config_generation;
		path->subvol = sv;
		path->plex = p;
		path->vol_elmnt = ve;
		path->disk_part = -1;
		return (1);
	} else
		return (0);

} /* end of _find_obj_in_subvol() */


/*
 * The block_map and ve_array can be pre-allocated by the caller by passing
 * non NULL "block_map_p" and "ve_array_p" arguments.
 *
 * N.B. This routine has a deadlock potential since it is called with locks
 * held and may need to do a kmem_alloc().  It prevents swap volumes from
 * working.
 */
int
xlv_recalculate_blockmap(
	xlv_tab_subvol_t	*svp,
	xlv_block_map_t		**block_map_p,	/* point to blockmap ptr */
	ve_entry_t		**ve_array_p)	/* point to ve_array */
{
	unsigned	i;
	xlv_block_map_t	*block_map;
	ve_entry_t	*ve_array;

	if (block_map_p) {		/* pre-allocated memory */
		block_map = *block_map_p;
		*block_map_p = NULL;
	} else {
		block_map = NULL;
	}

	if (ve_array_p) {		/* pre-allocated memory */
		ve_array = *ve_array_p;
		*ve_array_p = NULL;
	} else {
		ve_array = NULL;
	}

	/*
	 * Recalculate block map.
	 */
	svp->subvol_size = xlv_tab_subvol_size(svp);

	if (svp->num_plexes > 1) {
		/* xlv_tab_create_block_map() consumes ve_array */
		block_map = xlv_tab_create_block_map(svp, block_map, ve_array);
		if (block_map == NULL) {
			return(EINVAL);	/* gotta have a block map! */
		}

		/*
                 * Go through the block map and see if there are any
		 * block ranges for which there are no active volume
		 * elements. If so, make one of them active.
		 */
                for (i=0; i < block_map->entries; i++) {
                        if (block_map->map[i].read_plex_vector == 0) {
                                block_map->map[i].read_plex_vector =
                                        FIRST_BIT_SET (block_map->
					    map[i].write_plex_vector);
                        }
                }

	} else {	/* svp->num_plexes == 1 */
		/*
		 * Single plex so there is no block map.
		 */
		if (block_map) {
			kmem_free(block_map, 0);
			block_map = NULL;
		}
		if (ve_array) {
			kmem_free(ve_array, 0);
			ve_array = NULL;
		}
	}

	if (svp->block_map)
		kmem_free(svp->block_map, 0);
	svp->block_map = block_map;

	return(0);
} /* xlv_recalculate_blockmap() */


/*
 * Find the given volume element in the plex and extract it.
 * Shrink the plex by decrementing the volume element count.
 *
 * Note: Watch out for removing the only volume element of a plex.
 */
static int
_rem_ve_from_plex(xlv_tab_plex_t *plexp, int ve_idx)
{
	int last, count;
	
	last = plexp->num_vol_elmnts - 1;
	count = (last - ve_idx) * sizeof(xlv_tab_vol_elmnt_t);

	if (count) {
		/*
		 * Move all trailing volume elements up the array.
		 */
		bcopy ( &plexp->vol_elmnts[ve_idx + 1],
			&plexp->vol_elmnts[ve_idx],
			count);
	}
	plexp->num_vol_elmnts--;		/* one less volume element */

	ASSERT(plexp->num_vol_elmnts > 0);

	return (0);
}

/*
 * Add a volume element to a plex. The plex structure is grown if
 * it cannot accommodate another volume element. The caller creates
 * new "grown" plex which is passed in via the "new_plexpp" argrument.
 *
 * The new volume element is initialized to the "stale" state so
 * it can be updated.
 *
 * Note: This assumes that the given volume element has been check
 * that it is the same size as other volume elements in the same
 * row in different plexes.
 *
 * On success, "new_plexpp" is consumed (used or freed) and the caller
 * plex pointer is set to NULL -- don't want a dangling reference to
 * the pre-allocate plex memory.
 */
static int
_add_ve_to_plex(
	xlv_tab_plex_t      **plexpp,		/* plex ptr can change */
	xlv_tab_vol_elmnt_t *vep,
	xlv_tab_plex_t      **new_plexpp)	/* pre-allocated memory */
{
	xlv_tab_plex_t		*pp;
	xlv_tab_vol_elmnt_t	*pvep;
	int			count, idx, last;
	int			max_ve;
	xlv_tab_plex_t		*new_pp;

	new_pp = *new_plexpp;
	pp = *plexpp;
	last = pp->num_vol_elmnts - 1;

	/*
	 * find the insertion point
	 */
	pvep = &pp->vol_elmnts[0];
	for (idx = 0; idx < pp->num_vol_elmnts; idx++, pvep++ ) {
		if (vep->start_block_no < pvep->start_block_no) {
			break;		/* found correct idx */
		}
	}

	/*
	 * Check for overlaps with the surrounding volume elements.
	 */
	if ((idx && ((pvep-1)->end_block_no > vep->start_block_no)) ||
	    ((idx <= last) && (vep->end_block_no > pvep->start_block_no))) {
		ASSERT(0);		/* overlapping ve's */
		return(EINVAL);
	}

	if (pp->max_vol_elmnts == pp->num_vol_elmnts) {
		/*
		 * No space in the current plex: use the new plex.
		 * Make sure that the new plex is big enough.
		 */
		if (pp->num_vol_elmnts >= new_pp->max_vol_elmnts) {
			return ENOMEM;
		}

		max_ve = new_pp->max_vol_elmnts;	/* save max */
		bcopy(pp, new_pp, sizeof(xlv_tab_plex_t) +
		      ((pp->max_vol_elmnts-1) * sizeof(xlv_tab_vol_elmnt_t))); 
		new_pp->max_vol_elmnts = max_ve;	/* restore max */
		kmem_free(pp, 0);
		*plexpp = pp = new_pp;
	} else {
		/*
		 * There is space in the current plex for the new ve
		 * so free the pre-allocated plex.
		 */
		kmem_free(new_pp, 0);
	}
	*new_plexpp = NULL;	/* consumed plex so set caller's ptr to NULL */

	/*
	 * insert the volume element
	 */
	count = (last - idx + 1) * sizeof(xlv_tab_vol_elmnt_t);
	if (count) {
		/*
		 * Move all trailing volume elements down the array.
		 */
		bcopy (&pp->vol_elmnts[idx], &pp->vol_elmnts[idx+1], count);
	}
	bcopy(vep, &pp->vol_elmnts[idx], sizeof(xlv_tab_vol_elmnt_t));
	pp->num_vol_elmnts++;		/* one more volume element */

	return(0);

} /* end of _add_ve_to_plex() */


/*
 * Add a component to an exisiting object:
 *
 *	add a volume element to a plex -- pass in plex
 *	add a plex to a subvolume -- pass in subvol
 *
 * An add operation is really a merge operation on the parent object.
 */
static int
xlv_attr_cmd_add(xlv_attr_req_t *req, xlv_obj_path_t *path)
{
	int		 error, sv, i, j;
	unsigned int	 st;
	xlv_tab_plex_t	 *pp, *k_pp, *new_pp = NULL;
	xlv_tab_subvol_t *svp, *k_svp;
	unsigned int	 max_top_bufs_needed;
	unsigned int	 max_low_bufs_needed;
	xlv_block_map_t *block_map = NULL;
	ve_entry_t	*ve_array = NULL;
	boolean_t	cfglocked = B_FALSE;

	error = 0;
	sv = path->subvol;
	ASSERT(sv >= 0 && sv <= xlv_maxunits);
        xlv_acquire_io_lock(sv, NULL);
	k_svp = &xlv_tab->subvolume[sv];

	/*
	 * Before acquiring the config lock, check for valid
	 * operation and do all required malloc's.
	 */
	if (XLV_ATTR_SUBVOL == req->attr) {
		/*
		 * Must have plexing support to add a plex to a volume.
		 */
		if (!xlv_plexing_support_present) {
			xlv_release_io_lock(sv);
			return(ENOPKG);		/* not supported w/o plexing */
		}

		svp = req->ar_svp;
		pp = k_pp = NULL;

		/*
		 * Calculate the reserved memory required.
		 * Rely on the user to pass a good subvolume.
		 */
		ASSERT(svp->block_map == 0);
		svp->block_map = xlv_tab_create_block_map(svp, NULL, NULL);
		if (svp->block_map == NULL) {
			/*
			 * Adding a plex to a subvol must produce a block map.
			 */
			xlv_release_io_lock(sv);
			return(EINVAL);
		}
		max_top_bufs_needed = xlv_tab_max_top_bufs(svp);
		max_low_bufs_needed = xlv_tab_max_low_bufs(svp);
		block_map = svp->block_map;		/* save this memory */
		svp->block_map = NULL;

	} else if (XLV_ATTR_PLEX == req->attr) {
		/*
		 * Adding ve to plex.
		 */
		svp = NULL;
		pp = req->ar_pp;
		k_pp = k_svp->plex[path->plex];

		/*
		 * Depending the volume element being added, the low
		 * reserved memory may need to be increased.
		 */
		max_top_bufs_needed = 0;
		max_low_bufs_needed = xlv_tab_max_low_bufs(k_svp);
		/*
		 * Calculate low buf delta between old and new plex
		 * and add the delta to the max low buf for the subvol 
		 */
		i = xlv_tab_low_bufs_for_plex(k_pp);	/* old plex */
		j = xlv_tab_low_bufs_for_plex(pp);	/* new plex */
		if (j > i) {
			max_low_bufs_needed += (j - i);
		}
		/*
		 * Need to pre-allocate the new plex in case there isn't
		 * enough place in the current plex: can't do mem allocs
		 * once the config lock is taken.  Increase the current
		 * plex size by 5 volume elements.
		 */
		new_pp = kmem_zalloc(sizeof(xlv_tab_plex_t) +
				     ((k_pp->max_vol_elmnts-1+5) *
				      sizeof(xlv_tab_vol_elmnt_t)), KM_SLEEP);
		new_pp->max_vol_elmnts = k_pp->max_vol_elmnts + 5;

		/*
		 * allocate a new block map with one more entry in it
		 * so we use k_svp->block_map->entries (the -1 and +1
		 * cancel out), and set the new number of entries.
		 */
		if (k_svp->block_map == NULL) {
			block_map = NULL;	/* single plex so no blockmap */
		} else {
			block_map = (xlv_block_map_t *) kmem_zalloc(
			    sizeof(xlv_block_map_t) +
			    (k_svp->block_map->entries *
				sizeof(xlv_block_map_entry_t)),
			    KM_SLEEP);
			block_map->entries = k_svp->block_map->entries + 1;
		}
	} else {
		/*
		 * Only adding a plex or a volume element is supported. 
		 */
		xlv_release_io_lock(sv);
		return(EINVAL);
	}

	xlv_release_io_lock(sv);
	/*
	 * Increase the reserve memory needed before acquiring the
	 * locks.  Also, allocate memory for ve_array, used in
	 * calculating the block map.  Both of these are done now so
	 * that we don't do a kmem_alloc with the locks held.  This is
	 * because if the subvolume is used for swap, we could get
	 * into a memory deadlock.
	 * Note: the ve_array will be freed by xlv_tab_create_block_map().
	 */
	error = xlv_tab_alloc_reserved_buffers(max_top_bufs_needed,
				       max_low_bufs_needed, B_FALSE);

	if (error)
		goto done;

	ve_array = (ve_entry_t *) kmem_zalloc(
			sizeof(ve_entry_t)*XLV_MAX_VE_PER_PLEX, KM_SLEEP);

	/*
	 * Acquire the cfg lock for this subvolume.  There is a small
	 * window where the configuration can change.  But this is
	 * okay because we check the generation count.
	 */
	xlv_acquire_cfg_lock(sv);
	cfglocked = B_TRUE;

	/* Check generation count to see if things changed */
	if (path->generation != xlv_config_generation) {
		error = ESTALE;
		goto done;
	}

	if (XLV_ATTR_SUBVOL == req->attr) {
#if 0
		if (!uuid_equal(&svp->uuid, &k_svp->uuid, &st)) {
			error = ESTALE;
			goto done;
		}
#endif
		/*
		 * Add a plex to the subvolume. This code assumes
		 * the plex array can be sparse.
		 *
		 * XXX Some part of the xlv driver expects the table
		 * to be dense.
		 * XXX 10/5/94 WH That's no longer the case. The only
		 * place where the kernel assumes the table is dense
		 * is if the subvolume has only a single plex.
		 */
		ASSERT(k_svp->num_plexes < svp->num_plexes);
		for (i = 0; i < XLV_MAX_PLEXES; i++ ) {
			pp = svp->plex[i];
			k_pp = k_svp->plex[i];
			if (!pp)
				continue;	/* no plex in user space */
			if (k_pp) {
				/*
				 * There is a plex here in the kernel.
				 * Make sure this is the same as the
				 * one in user space.
				 */
				if (uuid_equal(&k_pp->uuid, &pp->uuid, &st))
					continue;
				error = EINVAL;		/* different plexes */
				goto done;
			}
			else {
				/*
				 * If this plex does not exist in the kernel,					 * mark all of its volume elements stale.
				 */
				for (j = 0; j < pp->num_vol_elmnts; j++)
					pp->vol_elmnts[j].state =
						XLV_VE_STATE_STALE;
			}

			/*
			 * Don't copy the plex information. Move the
			 * allocated plex over to the kernel subvol
			 * entry so there won't be another malloc.
			 *
			 * XXX WH  We may want to merge the current
			 * volume element state into the new one if
			 * the plexes are the same.
			 *
			 * 9/12/96 jleong This is a new plex to be added to
			 * the subvolume so there is no ve state to merge.
			 */
			k_svp->plex[i] = pp;	/* use it */
			bzero (pp->name, sizeof(xlv_name_t));
			svp->plex[i] = NULL;	/* can't free it later */
			k_svp->num_plexes++;

			/*
			 * Don't "break" here, just roll the loop.
			 * This allows us to add more than one plex
			 * per function call. 
			 */
		} /* for every plex */

		/*
		 * k_svp->block_map could be NULL (currently single plex)
		 * block_map is memory just malloc from above
		 */
	} /* XLV_ATTR_SUBVOL == req->attr */

	else {	/* XLV_ATTR_PLEX == req->attr */
		xlv_tab_vol_elmnt_t *vep, *k_vep;
#if 0
		if (!uuid_equal(&pp->uuid, &k_pp->uuid, &st)) {
			error = ESTALE;
			goto done;
		}
#endif
		/*
		 * Add a volume element to the plex.
		 * Find the new volume element and insert it.
		 * The new ve should be the first corresponding
		 * ve with a different uuid (Remember the user
		 * space table must match the kernel's)
		 */
		if (!(pp->num_vol_elmnts > k_pp->num_vol_elmnts)) {
			ASSERT(0);
			error = EINVAL;
			goto done;
		}
		vep = &pp->vol_elmnts[0];
		k_vep = &k_pp->vol_elmnts[0];
		for (i = 0; i < k_pp->num_vol_elmnts; i++, vep++, k_vep++) {
			if (!uuid_equal(&vep->uuid, &k_vep->uuid, &st))
				break;
		}
		/*
		 * XXXjleong -- TBD Know what the index should be.
		 * Can optimize by passing this info along to
		 * _add_ve_to_plex().
		 *
		 * Passing in the pre-allocated plex structure, which
		 * _add_ve_to_plex() must free on a successful return.
		 */
		if (error = _add_ve_to_plex(&k_svp->plex[path->plex], vep,
					    &new_pp)) {
			goto done;
		}
		/* successful return: new_pp is NULL */
		ASSERT(new_pp == NULL);
	} /* XLV_ATTR_PLEX == req->attr */

	ASSERT(error == 0);
	xlv_config_generation++;
	xlv_configchange_notification();
	error = xlv_recalculate_blockmap(k_svp, &block_map, &ve_array);
	ASSERT(ve_array == NULL);
	ASSERT(block_map == NULL);

done:
	if (block_map) kmem_free(block_map, 0);
	if (ve_array) kmem_free(ve_array, 0);
	if (new_pp) kmem_free(new_pp, 0);
	if (cfglocked) xlv_release_cfg_lock(sv);

	return error;

} /* end of xlv_attr_cmd_add() */


/*
 * Acquire config lock for each subvolume associated with the volume.
 */
static void
_lock_vol(xlv_tab_vol_entry_t *vp)
{
	int	sv;

	if (vp == NULL) {
		ASSERT(0);
		return;
	}

	if (vp->log_subvol) {
		sv = vp->log_subvol - &xlv_tab->subvolume[0];
		ASSERT(sv >= 0);
		ASSERT(vp->log_subvol == &xlv_tab->subvolume[sv]);
		xlv_acquire_cfg_lock(sv);
	}

	if (vp->data_subvol) {
		sv = vp->data_subvol - &xlv_tab->subvolume[0];
		ASSERT(sv >= 0);
		ASSERT(vp->data_subvol == &xlv_tab->subvolume[sv]);
		xlv_acquire_cfg_lock(sv);
	}

	if (vp->rt_subvol) {
		sv = vp->rt_subvol - &xlv_tab->subvolume[0];
		ASSERT(sv >= 0);
		ASSERT(vp->rt_subvol == &xlv_tab->subvolume[sv]);
		xlv_acquire_cfg_lock(sv);
	}
}


/*
 * Release up all locks associated with the volume.
 */
static void
_unlock_vol(xlv_tab_vol_entry_t *vp)
{
	int	sv;

	if (vp == NULL) {
		ASSERT(0);
		return;
	}

	if (vp->log_subvol) {
		sv = vp->log_subvol - &xlv_tab->subvolume[0];
		ASSERT(!(sv < 0));
		ASSERT(vp->log_subvol == &xlv_tab->subvolume[sv]);
		xlv_release_cfg_lock(sv);
	}
	if (vp->data_subvol) {
		sv = vp->data_subvol - &xlv_tab->subvolume[0];
		ASSERT(!(sv < 0));
		ASSERT(vp->data_subvol == &xlv_tab->subvolume[sv]);
		xlv_release_cfg_lock(sv);
	}
	if (vp->rt_subvol) {
		sv = vp->rt_subvol - &xlv_tab->subvolume[0];
		ASSERT(!(sv < 0));
		ASSERT(vp->rt_subvol == &xlv_tab->subvolume[sv]);
		xlv_release_cfg_lock(sv);
	}
}


/*
 * Notify the kernel label daemon that the user has changed the
 * configuration. Most likely, some standalone object has changed.
 * Need to bump up the generation count so the user level label
 * daemon would pick up the disk label changes.
 */
static int
xlv_attr_cmd_notify(void)
{
	/*
	 * Don't really need locks around this because we just
	 * want the user level daemon to know there are changes.
	 */
	xlv_config_generation++;
	xlv_configchange_notification();
	
	return 0;
}


/*
 * Remove a volume
 */
static int
xlv_attr_cmd_remove_vol(xlv_attr_req_t *req, xlv_obj_path_t *path)
{
	xlv_tab_vol_entry_t	*vp, vol_entry;
	xlv_tab_subvol_t	*svp;
	int			idx;
	int			error = 0;
	
	if (req->attr != XLV_ATTR_VOL && req->attr != XLV_ATTR_PATH)
		return (EINVAL);

	vp = &xlv_tab_vol->vol[path->vol];
	/*
	 * Make a copy of the volume entry so that we would
	 * remember the lock entries that need to be unlocked
	 * after the original vol entry has been removed.
	 */
	bcopy (vp, &vol_entry, sizeof(xlv_tab_vol_entry_t));

	_lock_vol(vp);

	/* Check generation count to see if things changed */
	if (path->generation != xlv_config_generation) {
		error = ESTALE;
		goto done;
	}

	if (svp = vp->log_subvol) {
		for (idx = 0; idx < XLV_MAX_PLEXES; idx++) {
			if (svp->plex[idx])
				kmem_free(svp->plex[idx], 0);
		}
		/*
		 * The statp can be freed or cleared.  We choose to
		 * just cleared so the get function don't have to check
		 * for a valid statp.
		 */
		bzero(xlv_io_lock[minor(svp->dev)].statp, sizeof(xlv_stat_t));
		bzero(svp, sizeof(xlv_tab_subvol_t));
		xlv_tab->num_subvols--;
	}
	if (svp = vp->data_subvol) {
		for (idx = 0; idx < XLV_MAX_PLEXES; idx++) {
			if (svp->plex[idx])
				kmem_free(svp->plex[idx], 0);
		}
		bzero(xlv_io_lock[minor(svp->dev)].statp, sizeof(xlv_stat_t));
		bzero(svp, sizeof(xlv_tab_subvol_t));
		xlv_tab->num_subvols--;
	}
	if (svp = vp->rt_subvol) {
		for (idx = 0; idx < XLV_MAX_PLEXES; idx++) {
			if (svp->plex[idx])
				kmem_free(svp->plex[idx], 0);
		}
		bzero(xlv_io_lock[minor(svp->dev)].statp, sizeof(xlv_stat_t));
		bzero(svp, sizeof(xlv_tab_subvol_t));
		xlv_tab->num_subvols--;
	}

	bzero(vp, sizeof(xlv_tab_vol_entry_t));
	xlv_tab_vol->num_vols--;
	xlv_config_generation++;
	xlv_configchange_notification();
done:
	_unlock_vol(&vol_entry);

	return (error);

} /* end of xlv_attr_cmd_remove_vol() */


/*
 * Remove a component from an existing object.
 *
 *	remove a volume
 *	remove a subvolume from a volume
 *	remove a plex from a subvolume
 *	remove a ve from a plex
 */
static int
xlv_attr_cmd_remove(xlv_attr_req_t *req, xlv_obj_path_t *path)
{
	int			error, sv, idx;
	int			attr;
	xlv_tab_subvol_t	*k_svp;

	if (XLV_ATTR_PATH == req->attr) {
		if (0 > path->vol_elmnt)
			attr = XLV_ATTR_PLEX;
		else if (0 > path->plex)
			attr = XLV_ATTR_SUBVOL;
		else if (0 > path->subvol)
			attr = XLV_ATTR_VOL;
		else
			attr = XLV_ATTR_VE;
	} else {
		attr = req->attr;
	}

	/*
	 * Removing a volume is handle differently. There
	 * are multiple locks to acquire.
	 */
	if (attr == XLV_ATTR_VOL)
		return (error = xlv_attr_cmd_remove_vol(req, path));

	error = 0;
	sv = path->subvol;
	k_svp = &xlv_tab->subvolume[sv];

	/* Acquire the io lock in exclusive mode. */
	xlv_acquire_cfg_lock(sv);

	/* Check generation count to see if things changed */
	if (path->generation != xlv_config_generation) {
		error = ESTALE;
		goto done;
	}

	switch (attr) {
	/* case XLV_ATTR_VOL: see above */
	case XLV_ATTR_SUBVOL:
	{
		/*
		 * Free which plex and clear out the subvolume.
		 */
		for (idx = 0; idx < XLV_MAX_PLEXES; idx++) {
			if (k_svp->plex[idx])
				kmem_free(k_svp->plex[idx], 0);
		}
		bzero(k_svp, sizeof(xlv_tab_subvol_t));
		xlv_tab->num_subvols--;
		k_svp = NULL;	/* freed so don't xlv_recalculate_blockmap() */
		break;
	}
	case XLV_ATTR_PLEX:
	{
		idx = path->plex;
		if (k_svp->plex[idx] != NULL) {
			kmem_free(k_svp->plex[idx], 0);
			k_svp->plex[idx] = NULL;
		}
		k_svp->num_plexes--;		/* plex is gone */

		/*
		 * Keep plex array dense. Move plexes in the
		 * higher slots down.
		 *
		 * Note: Libxlv must compact the plex array in the
		 * same fashion!
		 */
		for (; idx < (XLV_MAX_PLEXES-1); idx++) {
			k_svp->plex[idx] = k_svp->plex[idx + 1];
			k_svp->plex[idx + 1] = NULL;
		}
		break;
	}
	case XLV_ATTR_VE:
	{
		if (_rem_ve_from_plex(
		    k_svp->plex[path->plex], path->vol_elmnt) < 0) {
			error = EINVAL;
			goto done;
		}
		break;
	}

	/* case XLV_ATTR_TAB_SIZE: */
	default:
		error = EINVAL;
	}

	if (!error) {
		xlv_config_generation++;
		xlv_configchange_notification();
		if (k_svp) {
			/*
			 * xlv_recalculate_blockmap() might do a kmem_alloc:
			 * need to (a) rewrite it not to kmem_alloc, i.e.,
			 * change the block map in place,
			 * or (b) pre-allocate memory.
			 */
			error = xlv_recalculate_blockmap(k_svp, NULL, NULL);
			/* keep the same reserved memory */
		}
	}

done:
	xlv_release_cfg_lock(sv);

	return error;

} /* end of xlv_attr_cmd_remove() */

/*
 * Set some user configurable field.
 */
static int
xlv_attr_cmd_set(xlv_attr_req_t *req, xlv_obj_path_t *path)
{
	int			error, sv;
	xlv_tab_subvol_t	*k_svp;

	error = 0;

	if (req->attr == XLV_ATTR_VOL && 0 > path->subvol) {
		/*
		 * No volume lock but can use the data subvol lock
		 * (everyone gotta have a data subvol) to synchronize.
		 */
		xlv_tab_vol_entry_t *volp;
		volp = &xlv_tab_vol->vol[path->vol];
		sv = volp->data_subvol - &xlv_tab->subvolume[0];
	} else {
		sv = path->subvol;
	}

	if (0 > sv) {
		ASSERT(0);
		return(error = EINVAL);
	}
	k_svp = &xlv_tab->subvolume[sv];

	/*
	 * Acquire the io lock in exclusive mode.
	 * There is a small window where the configuration
	 * can change. But this should be okay because
	 * libxlv should be serializing calls (probably
	 * with a lock file) into the kernel.
	 */
	xlv_acquire_cfg_lock(sv);

	/* Check generation count to see if things changed */
	if (path->generation != xlv_config_generation) {
		error = ESTALE;
		goto done;
	}

	switch (req->attr) {
	case XLV_ATTR_VOL:
	{
		xlv_tab_vol_entry_t *new_vp, *k_vp;

		new_vp = req->ar_vp;
		k_vp = &xlv_tab_vol->vol[path->vol];

		/* k_vp->uuid */
		bcopy (new_vp->name, k_vp->name, sizeof(xlv_name_t));
		k_vp->flags = new_vp->flags;
		k_vp->state = new_vp->state;
		k_vp->rsvd  = new_vp->rsvd;
		/* k_vp->sector_size */
		/* k_vp->log_subvol */
		/* k_vp->data_subvol */
		/* k_vp->rt_subvol */
		break;
	}
	case XLV_ATTR_SUBVOL:
	{
		xlv_tab_subvol_t *new_svp = req->ar_svp;

		k_svp->flags = new_svp->flags;
		/* k_svp->vol_p */
		/* k_svp->uuid */
		/* k_svp->subvol_type */
		/* k_svp->subvol_depth */
		/* k_svp->subvol_size */
		/* k_svp->dev */
		/*
		 * The following fields are only referenced if this subvolume
		 * is plexed.
		 */
		/* k_svp->critical_region */
		/* k_svp->rd_wr_back_start */
		/* k_svp->rd_wr_back_end */
		/* k_svp->initial_read_slice */
		/* k_svp->num_plexes */
		/* k_svp->plex[XLV_MAX_PLEXES] */
		/* k_svp->block_map */
		break;
	}
	case XLV_ATTR_PLEX:
	{
		xlv_tab_plex_t *new_pp, *k_pp;

		new_pp = req->ar_pp;
		k_pp = k_svp->plex[path->plex];

		k_pp->flags = new_pp->flags;
		/* k_pp->uuid */
		/* k_pp->name */
		/* k_pp->num_vol_elmnts */
		/* k_pp->max_vol_elmnts */
		/* k_pp->vol_elmnts */
		break;
	}
	case XLV_ATTR_VE:
	{
		xlv_tab_vol_elmnt_t *new_vep, *k_vep;
		int	oldstate;
		
		new_vep = req->ar_vep;
		k_vep = &k_svp->plex[path->plex]->vol_elmnts[path->vol_elmnt];

		/* k_vep->uuid */
		/* k_vep->name */
		/* k_vep->type */
		/* k_vep->grp_size */
		/* k_vep->stripe_unit_size */
		/* k_vep->reserve */
		/* k_vep->start_block_no */
		/* k_vep->end_block_no */
		/* k_vep->disk_parts */

		if (k_vep->state == new_vep->state) {
			goto done;	/* NOP */
		} else if (k_vep->state == XLV_VE_STATE_INCOMPLETE) {
			/*
			 * Currently the ve is missing some parts so
			 * there is no permission to make any changes.
			 */
			error = EACCES;
			break;
		}

		/*
		 * Doing a state change. Watch out for ve which
		 * changes read/write availability.
		 */
		oldstate = k_vep->state;
		k_vep->state = new_vep->state;
		if (XLV_VE_STATE_OFFLINE == oldstate
		    && RDWR_VE_STATE(new_vep->state)) {
			/*
			 * Going from the offline to a "online" state
			 * requires block map recomputation.
			 *
			 * This ve must have been mirrored or else there
			 * would have been a hole in the address space.
			 * The high mem buf depends on the number of
			 * block map entries and that did not change.
			 * The low mem buf depends on the max of the sum
			 * of the max's from each plex and that did not
			 * change. So there is no need to adjust the
			 * reserved memory.
			 */
			error = xlv_recalculate_blockmap(k_svp, NULL, NULL);
			break;
		} else if (RDWR_VE_STATE(oldstate)
			   && NO_RDWR_VE_STATE(new_vep->state)) {
			/*
			 * Going from a usable to non-usable state
			 * requires block map recomputation. But
			 * the reserved memory should not shrink.
			 */
			error = xlv_recalculate_blockmap(k_svp, NULL, NULL);
			/* keep the same reserved memory */
			break;
		} else {
			/*
			 * The volume element's usability has not changed
			 * so don't change generation.
			 */
			goto done;
		}
	}

	/* case XLV_ATTR_TAB_SIZE: */
	default:
		error = EINVAL;
	}

	if (!error) {
		xlv_config_generation++;
		xlv_configchange_notification();
	}

done:
	xlv_release_cfg_lock(sv);

	return error;

} /* end of xlv_attr_cmd_set() */


/*
 * xlv_attr_set()
 */
int
xlv_attr_set(xlv_attr_cursor_t *u_cursor, xlv_attr_req_t *u_req)
{
	xlv_attr_cursor_t	k_cursor;
	xlv_attr_req_t		k_req;
	int			error;
	int			found;
	int			idx;
	unsigned		st;
	xlv_obj_path_t		path;
	xlv_attr_memory_t 	xlvmem;

	if (! _CAP_ABLE(CAP_DEVICE_MGT)) {
		return (EPERM);
	}
	if ((u_cursor == NULL) || (u_req == NULL)) {
		return (EFAULT);
	}
	if (copyin((caddr_t)u_cursor, (caddr_t)&k_cursor, sizeof(k_cursor)) ||
	    COPYIN_XLATE((caddr_t)u_req, (caddr_t)&k_req,
			 sizeof(k_req), irix5_to_xlv_attr_req,
			 get_current_abi(), 1)) {
			 /*
			  * Note: called macro instead of copyin_xlate()
			  * becasuse o32 and n32 are the same.
			  */
		return (EINVAL);
	}

	/*
	 * Some commands do not need to do much pre-processing of
	 * the user request so let's just do those commands now.
	 */
	if (XLV_ATTR_FLAGS == k_req.attr) {
		/*
		 * enable|disable statistics gathering
		 */
		xlv_stats_enabled = (k_req.ar_flag1 & XLV_FLAG_STAT);
		xlv_failsafe = (k_req.ar_flag2 & XLV_FLAG2_FAILSAFE);
		return(0);
	} else if (XLV_ATTR_STATS == k_req.attr) {
		/*
		 * reset subvolume statistics
		 */
		xlv_stat_t	mystat;
		idx = k_cursor.subvol;
		if (idx >= xlv_maxunits) {
			return(ENFILE);
		}
		if (!xlv_io_lock[idx].statp) {
			return(ENOENT);
		}
		if (copyin((caddr_t)k_req.ar_statp,
			   (caddr_t)&mystat, sizeof(xlv_stat_t))) {
			return(EFAULT);
		}
		bcopy(&mystat, xlv_io_lock[idx].statp, sizeof(xlv_stat_t));
		return(0);
	}

	if (xlv_tab == NULL || xlv_tab_vol == NULL) {
		/*
		 * The XLX tables do not exist (xlv_assemble has not called).
		 */
		return (ENOENT);
	}
	if (k_cursor.generation != xlv_config_generation)
		return (ESTALE);

	path.generation = k_cursor.generation;
	path.vol = -1;
	path.subvol = -1;
	path.plex = -1;
	path.vol_elmnt = -1;
	path.disk_part = -1;

	/*
	 * Copy the user request into the kernel.
	 */
	switch (k_req.attr) {
	/* case XLV_ATTR_TAB_SIZE: */
	case XLV_ATTR_VOL:
	{ 
		if (error = copyin_vol(k_req.ar_vp, &k_req.ar_vp)) {
			break;
		}
		if (uuid_is_nil(&k_req.ar_vp->uuid, &st)) {
			error = EINVAL;	 /* passed in a null vol uuid. */
			break;
		}
		found = 0;
		for (idx = 0; idx < xlv_tab_vol->max_vols; idx++) {
			if (! uuid_equal(&k_req.ar_vp->uuid,
					 &xlv_tab_vol->vol[idx].uuid, &st))
				continue;
			found++;
			path.vol = idx;
			break;
		}
		break;
	}
	case XLV_ATTR_SUBVOL:
		if (error = copyin_subvol(k_req.ar_svp, &k_req.ar_svp))
			break;
		found = _find_obj_in_subvol(&k_req.ar_svp->uuid,
					    XLV_OBJ_TYPE_SUBVOL, &path);
		break;
	case XLV_ATTR_PLEX:
		if (error = copyin_plex(k_req.ar_pp, &k_req.ar_pp))
			break;
		found = _find_obj_in_subvol(&k_req.ar_pp->uuid,
					    XLV_OBJ_TYPE_PLEX, &path);
		break;
	case XLV_ATTR_VE:
		if (error = copyin_ve(k_req.ar_vep, &k_req.ar_vep))
			break;
		found = _find_obj_in_subvol(&k_req.ar_vep->uuid,
					    XLV_OBJ_TYPE_VOL_ELMNT, &path);
		break;
	case XLV_ATTR_PATH:
	{
		xlv_attr_cursor_t usr_path;

		if (copyin((caddr_t)k_req.ar_path,
			   (caddr_t)&usr_path,
			   sizeof(xlv_attr_cursor_t))) {
			error = EFAULT;
		} else {
			/*
			 * path.generation = k_cursor.generation;
			 *
			 * Don't bother copying the generation number
			 * from the user's path -- use the one from the
			 * user's cursor. This was already set up above.
			 */
			path.vol = usr_path.vol;
			path.subvol = usr_path.subvol;
			path.plex = usr_path.plex;
			path.vol_elmnt = usr_path.ve;
			path.disk_part = -1;
			error = 0;
			found = 1;
		}
		break;
	}
	case XLV_ATTR_GENERATION:
		/*
		 * really nothing to do
		 */
		found = 1;	/* nothing to find, but set it so continue */
		error = 0;
		break;
	case XLV_ATTR_MEMORY:

		if (copyin((caddr_t)k_req.ar_mem, (caddr_t)&xlvmem,
			   sizeof(xlv_attr_memory_t))) {
			error = EFAULT;
		} else {
			found = 1;	
			error = 0;
		}
			break;
	/* case XLV_ATTR_STATS: */
	/* case XLV_ATTR_FLAGS: */
	default:
		error = EINVAL;

	} /* switch of k_req.attr */

	if (error)
		return(error);

	if (found == 0) {
		error = ENOENT;
	}
	else {
		/*
		 * Process the command request.
		 */
		switch (k_req.cmd) {
		case XLV_ATTR_CMD_ADD:
			error = xlv_attr_cmd_add(&k_req, &path);
			break;
		case XLV_ATTR_CMD_REMOVE:
			error = xlv_attr_cmd_remove(&k_req, &path);
			break;
		case XLV_ATTR_CMD_SET:
			error = xlv_attr_cmd_set(&k_req, &path);
			break;
		/* case XLV_ATTR_CMD_QUERY: */
		case XLV_ATTR_CMD_NOTIFY:
			error = xlv_attr_cmd_notify();
			break;
		case XLV_ATTR_CMD_PLEXMEM:
			error = xlv_res_mem_adjust(&xlvmem, &xlv_top_mem);
			break;
		case XLV_ATTR_CMD_VEMEM:
			error = xlv_res_mem_adjust(&xlvmem, &xlv_low_mem);
			break;
		default:
			error = EINVAL;
		}
	}

	if (!error) {
		/*
		 * Configuration changed, so return a new cursor.
		 */
		error = xlv_attr_get_cursor(u_cursor);
	}

	/*
	 * Free allocated memory
	 */
	switch (k_req.attr) {
	/* case XLV_ATTR_TAB_SIZE: */
	/* case XLV_ATTR_GENERATION: */
	case XLV_ATTR_VOL:
		_free_vol(k_req.ar_vp);
		break;
	case XLV_ATTR_SUBVOL:
		_free_subvol(k_req.ar_svp);
		break;
	case XLV_ATTR_PLEX:
		kmem_free(k_req.ar_pp, 0);
		break;
	case XLV_ATTR_VE:
		kmem_free(k_req.ar_vep, 0);
		break;
	}

	return (error);

} /* end of xlv_attr_set() */


/*
 * Copy in a plex struct from user to system space.
 * 
 * The "src" is a pointer to the plex entry in user space.
 * The "dst" is a pointer to a pointer to plex entry. This
 * functions allocates the plex entry in kernel space.
 */
static int
copyin_ve (xlv_tab_vol_elmnt_t *u_src, xlv_tab_vol_elmnt_t **k_dst)
{
	xlv_tab_vol_elmnt_t	*k_vep;

	if (u_src == NULL)
		return(EFAULT);

	k_vep = (xlv_tab_vol_elmnt_t *) kmem_alloc(sizeof(*k_vep), KM_SLEEP);

	/*
	 * Copy volume element info from user to kernel space.
	 */
	/* XXXjleong -- okay */
	if (copyin((caddr_t)u_src, (caddr_t)k_vep, sizeof(*k_vep))) { 
		kmem_free(k_vep, 0);
		return(EFAULT);
	}
	*k_dst = k_vep;

	return(0);
} /* end of copyin_ve() */


/*
 * Copy in a plex struct from user to system space.
 * 
 * The "src" is a pointer to the plex entry in user space.
 * The "dst" is a pointer to a pointer to plex entry. This
 * functions allocates the plex entry in kernel space.
 */
static int
copyin_plex (xlv_tab_plex_t *u_src, xlv_tab_plex_t **k_dst)
{
	xlv_tab_plex_t	*k_pp;
	xlv_tab_plex_t	*plexheader;
		/*
		 * XXX Only used for getting field num_vol_elmnts from the
		 * user space structure. Should really directly copy from
		 * the structure to the local variable num_vol_elmnts.
		 */
	int		num_vol_elmnts;
	int		plexsize;

	if (u_src == NULL)
		return(EFAULT);

	/*
	 * Copy in plex header info and figure how big a plex
	 * structure to allocate.
	 */
	plexheader = (xlv_tab_plex_t *)
			kmem_zalloc(sizeof(xlv_tab_plex_t), KM_SLEEP);
	if (COPYIN_XLATE((caddr_t)u_src, (caddr_t)plexheader,
			  sizeof(xlv_tab_plex_t), irix5_to_xlv_tab_plex,
			  get_current_abi(), 1)) {
		kmem_free(plexheader, 0);
		return(EFAULT);
	}
	num_vol_elmnts = plexheader->num_vol_elmnts;
	if (num_vol_elmnts == 0) {
		/*
		 * XXX The count being at least 1. This is
		 * because one is subtracted from the count
		 * when computing the plex size.
		 */
		num_vol_elmnts = 1;
	}
	kmem_free (plexheader, 0);	/* Done with header, so free it */

	plexsize = sizeof(xlv_tab_plex_t) +
		   (sizeof(xlv_tab_vol_elmnt_t) * (num_vol_elmnts-1));
	k_pp = (xlv_tab_plex_t *) kmem_alloc(plexsize, KM_SLEEP);

	/*
	 * Copy plex info from user to kernel space.
	 */
	if (COPYIN_XLATE((caddr_t)u_src, (caddr_t)k_pp,
			 plexsize, irix5_to_xlv_tab_plex,
			 get_current_abi(), num_vol_elmnts)) {
		kmem_free(k_pp, 0);
		return(EFAULT);
	}
	*k_dst = k_pp;

	return(0);
} /* end of copyin_plex() */


/* 
 * Copy in a subvolume struct from user to system space.
 * 
 * The "src" is a pointer to the subvolume entry in user space.
 * The "dst" is a pointer to a pointer to subvolume entry. This
 * functions allocates the subvolume entry in kernel space.
 */
static int
copyin_subvol (xlv_tab_subvol_t *u_src, xlv_tab_subvol_t **k_dst)
{
	xlv_tab_plex_t		*u_pp;
	xlv_tab_subvol_t	*subvol_p;
	int			i;

	if (u_src == NULL) {
		return (EFAULT);
	}

	if ( !(subvol_p = kmem_alloc(sizeof(xlv_tab_subvol_t), KM_SLEEP)) ) {
		return (ENOMEM);
	}

	/*
	 * Copyin user xfs_tab_subvol_t structure and gain
	 * access to pointers.
	 */
	if (copyin_xlate((caddr_t)u_src, (caddr_t)subvol_p,
			 sizeof(*subvol_p), irix5_to_xlv_tab_subvol,
			 get_current_abi(), KERN_ABI, 1)) {
		goto error;
	}

	/*
	 * Copy valid plex info from user space.
	 */
	for ( i = 0; i < XLV_MAX_PLEXES; i++ ) {
		if (NULL == (u_pp = subvol_p->plex[i])) {
			continue;	/* user space plex is missing */
		}
		copyin_plex(u_pp, &subvol_p->plex[i]);
	} /* finish copying plex */

	/*
	 * Only copy over the necessary fields. (NOT THE POINTERS !)
	 * Zero the other fields.
	 */
	subvol_p->vol_p = NULL;
	bzero(&subvol_p->critical_region, sizeof(subvol_p->critical_region));
	/* subvol_p->plex[] */
	subvol_p->block_map = NULL;

	*k_dst = subvol_p;
	return (0);
error:
	kmem_free(subvol_p, 0);
	return (EFAULT);

} /* end of copyin_subvol() */


static int
copyin_vol (xlv_tab_vol_entry_t *u_src, xlv_tab_vol_entry_t **k_dst)
{
	xlv_tab_vol_entry_t	*vol_p;

	if (u_src == NULL)
		return (EFAULT);

	if ( !(vol_p = kmem_alloc(sizeof(xlv_tab_vol_entry_t), KM_SLEEP)) )
		return (ENOMEM);

	/*
	 * Copyin user xlv_tab_vol_entry_t structure and gain
	 * access to pointers.
	 */
	if (COPYIN_XLATE((caddr_t)u_src, (caddr_t)vol_p,
			 sizeof(*vol_p), irix5_to_xlv_tab_vol_entry,
			 get_current_abi(), 1)) {
		return (EFAULT);
	}

	/*
	 * Copy valid subvolume from user space.
	 */
	if (vol_p->log_subvol) {
		if ( copyin_subvol(vol_p->log_subvol, &vol_p->log_subvol) ) {
			goto error;
		}
	}
	if (vol_p->data_subvol) {
		if ( copyin_subvol(vol_p->data_subvol, &vol_p->data_subvol) ) {
			if (vol_p->log_subvol)
				kmem_free(vol_p->log_subvol, 0);
			goto error;
		}
	}
	if (vol_p->rt_subvol) {
		if ( copyin_subvol(vol_p->rt_subvol, &vol_p->rt_subvol) ) {
			if (vol_p->log_subvol)
				kmem_free(vol_p->log_subvol, 0);
			if (vol_p->data_subvol)
				kmem_free(vol_p->data_subvol, 0);
			goto error;
		}
	}

	*k_dst = vol_p;
	return (0);

error:
	kmem_free(vol_p, 0);
	return (EFAULT);
} /* end of copyin_vol() */


/* 
 * Copy a subvolume struct from system to user space.
 * 
 * The "src" is a pointer to the subvolume entry in kernel space.
 * The "dst" is a pointer to an allocated subvolume entry in user space.
 */
static int
copyout_subvol (xlv_tab_subvol_t *src, xlv_tab_subvol_t *dst)
{
	xlv_tab_subvol_t	*subvol_p;
	xlv_tab_plex_t		*plexp, *pp, *zero_plex;
	int			i, j, plexsize, num_vol_elmnts;

	if ((src == NULL) && (dst == NULL)) {
		return (0);
	}
	if (dst == NULL) {
		/*
		 * subvol exists but user space location isn't specified
		 */
		return (EFAULT);
	}
	if (src == NULL) {
		/*
		 * no subvol so user space subvol uuid should be nil
		 */
		uuid_t  null_uuid;
		uint_t  status;
		void	*dst_uuid_addr;		/* addr of uuid in user space */

		uuid_create_nil(&null_uuid, &status);
		/* XXXjleong -- addr of subvol uuid depends on abi */
#if _MIPS_SIM == _ABI64
		if (ABI_IS_IRIX5_64(get_current_abi()))
			dst_uuid_addr = &dst->uuid;
		else if (ABI_IS_IRIX5_N32(get_current_abi()))
			dst_uuid_addr = &((irix5_n32_xlv_tab_subvol_t *)dst)->uuid;
		else
			dst_uuid_addr = &((irix5_o32_xlv_tab_subvol_t *)dst)->uuid;
#else
		dst_uuid_addr = &dst->uuid;
#endif
		if (copyout (&null_uuid, dst_uuid_addr, sizeof(null_uuid)))
			return (EFAULT);
		return (0);
	}

	/*
	 * Copyin user xfs_tab_subvol_t structure to gain
	 * access to pointers.
	 */
	if ( !(subvol_p = kern_malloc(sizeof(xlv_tab_subvol_t))) ) {
		return (ENOMEM);
	}
	if (copyin_xlate((caddr_t)dst, (caddr_t)subvol_p,
			 sizeof(*subvol_p), irix5_to_xlv_tab_subvol,
			 get_current_abi(), KERN_ABI, 1)) {
		goto error;
	}

	/*
	 * For each plex, copy info into local buffer
	 * before copying it to user space.
	 */
	for ( i = 0; i < XLV_MAX_PLEXES; i++ ) {
		if (NULL == subvol_p->plex[i]) {
			/*
			 * No destination plex so cannot copy
			 * kernel plex data out to the user.
			 */
			continue;
		}
		if (NULL == (pp = src->plex[i])) {
			/*
			 * Source plex is missing. We should not change
			 * the user space pointer to NULL but we cannot
			 * assume that the destination plex is zero'ed.
			 */
			zero_plex = (xlv_tab_plex_t *) kmem_zalloc (
				sizeof(xlv_tab_plex_t), KM_SLEEP);
			zero_plex->max_vol_elmnts = 1;
			zero_plex->num_vol_elmnts = 1; /* XXX */
			/*
	 		 * Note: o32 and n32 xlv_tab_plex_t is the same so
			 * call the macro instead of xlate_copyout().
			 */
			if (XLATE_COPYOUT((caddr_t)zero_plex,
					  (caddr_t)subvol_p->plex[i],
					  sizeof(xlv_tab_plex_t),
					  xlv_tab_plex_to_irix5,
					  get_current_abi(), 1)) {
				kmem_free (zero_plex, 0);
				goto error;
			}
			kmem_free (zero_plex, 0);
			continue;
		}

		num_vol_elmnts = pp->num_vol_elmnts;
		ASSERT(num_vol_elmnts > 0);
		plexsize = sizeof(xlv_tab_plex_t) +
			   (sizeof(xlv_tab_vol_elmnt_t)*(num_vol_elmnts-1));
		plexp = (xlv_tab_plex_t *) kmem_alloc(plexsize, KM_SLEEP);
		bcopy(pp, plexp, sizeof(xlv_tab_plex_t));

		for ( j = 1; j < pp->num_vol_elmnts; j++ ) {
			bcopy (&(pp->vol_elmnts[j]), &(plexp->vol_elmnts[j]),
				sizeof(xlv_tab_vol_elmnt_t));
		}

		if (XLATE_COPYOUT((caddr_t)plexp,
				  (caddr_t)subvol_p->plex[i], plexsize,
				  xlv_tab_plex_to_irix5, get_current_abi(),
				  num_vol_elmnts)) {
			kmem_free(plexp, 0);
			goto error;
		}
		kmem_free(plexp, 0);
	} /* finish copying plex */

	/*
	 * Only copy over the necessary fields. (NOT THE POINTERS !)
	 * Zero the other fields.
	 */
	subvol_p->flags		=  src->flags;
	subvol_p->open_flag	=  src->open_flag;
	subvol_p->vol_p		=  NULL;
	COPY_UUID(src->uuid, subvol_p->uuid);
	subvol_p->subvol_type	=  src->subvol_type;
	subvol_p->subvol_depth	=  src->subvol_depth;
	subvol_p->subvol_size	=  src->subvol_size;
	subvol_p->dev		=  src->dev;
	bcopy(&src->critical_region, &subvol_p->critical_region,
		sizeof(subvol_p->critical_region));
	subvol_p->initial_read_slice = src->initial_read_slice;
	subvol_p->num_plexes	=  src->num_plexes;
	/* subvol_p->plex[] */
	subvol_p->block_map	= NULL;

	if (xlate_copyout((caddr_t)subvol_p, (caddr_t)dst,
			  sizeof(*subvol_p), xlv_tab_subvol_to_irix5,
			  get_current_abi(), KERN_ABI, 1)) {
		goto error;
	}

	kern_free (subvol_p);
	return (0);

error:
	kern_free (subvol_p);
	return (EFAULT);

} /* end of copyout_subvol() */


/* 
 * Copy a volume struct from system to user space.
 * 
 * The "src" is a pointer to the volume entry in kernel space.
 * The "dst" is a pointer to an allocated volume entry in user space.
 */
static int
copyout_vol(xlv_tab_vol_entry_t *src, xlv_tab_vol_entry_t *dst)
{
	xlv_tab_vol_entry_t	*vp;
	int			error = 0;

	/*
	 * Copyin user xlv_tab_vol_entry_t structure to gain
	 * access to pointers.
	 */
	if ( !(vp = kern_malloc(sizeof(xlv_tab_vol_entry_t))) ) {
		return(ENOMEM);
	}

	/*
	 * Note: o32 and n32 xlv_tab_vol_entry_t is the same so
	 * call the macro instead of copyin_xlate() & xlate_copyout().
	 */

	if (COPYIN_XLATE((caddr_t)dst, (caddr_t)vp,
			 sizeof(*vp), irix5_to_xlv_tab_vol_entry,
			 get_current_abi(), 1)) {
		error = EFAULT;
		goto done;
	}

	COPY_UUID(src->uuid, vp->uuid);
	COPY_NAME(src->name, vp->name);
	vp->flags = src->flags;
	vp->state = src->state;
	vp->rsvd  = src->rsvd;
	vp->sector_size = src->sector_size;
	/*
	 * XXX Kernel does not keep track of the nodename
	 * so if the caller passed in a non NULL pointer,
	 * copyout an empty string.
	 */
	if (NULL != vp->nodename) {
		char nullchar = '\0';
		copyout(&nullchar, vp->nodename, sizeof(char));
	}

	error = copyout_subvol(src->log_subvol, vp->log_subvol);
	if (error)
		goto done;

	error = copyout_subvol(src->data_subvol, vp->data_subvol);
	if (error)
		goto done;

	error = copyout_subvol(src->rt_subvol, vp->rt_subvol);
	if (error)
		goto done;

	if (XLATE_COPYOUT((caddr_t)vp, (caddr_t)dst,
			  sizeof(*vp), xlv_tab_vol_entry_to_irix5,
			  get_current_abi(), 1)) {
		error = EFAULT;
	}

done:
	kern_free (vp);
	return (error);

} /* end of copyout_vol() */


#if _MIPS_SIM == _ABI64
/*
 * Used with copyin_xlate() to copy a xlv attr request structure
 * from a 32 (o32 or n32) bit user space application to a 64 bit kernel.
 */
/*ARGSUSED*/
static int
irix5_to_xlv_attr_req (
	 enum xlate_mode mode,
	 void		 *to,
	 int		 count,
	 xlate_info_t	 *info)
{
	COPYIN_XLATE_PROLOGUE (irix5_xlv_attr_req, xlv_attr_req);

	target->attr = source->attr;
	target->cmd = source->cmd;

	/*
	 * The type of argument depends on the attribute.
	 * But basically, it is either a pointer or a set of int's.
	 */
	switch(source->attr) {
	case XLV_ATTR_TAB_SIZE:
	case XLV_ATTR_LOCKS:
	case XLV_ATTR_SUPPORT:
	case XLV_ATTR_ROOTDEV:
	/*case XLV_ATTR_PATH:*/
	case XLV_ATTR_GENERATION:
	case XLV_ATTR_SV_MAXSIZE:
	case XLV_ATTR_FLAGS:
		target->ar_flag1 = source->ar_flag1;
		target->ar_flag2 = source->ar_flag2;
		break;
	default:
		target->ar_aru.aru_buf = (void *)
			to64bit((__psint_t) source->ar_aru.aru_buf);
	}

	return (0);
}
#endif
