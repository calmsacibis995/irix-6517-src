/**************************************************************************
 *                                                                        *
 *             Copyright (C) 1993-1994, Silicon Graphics, Inc.            *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.24 $"

/*
 * This module is the XLV administration lower layer. The higher layer
 * deals with the in-core data structure. The lower layer handles the
 * interaction with the physical disk and kernel. The actual writing
 * of XLV disk labels to the physical disk volume header and the changing
 * of kernel configuration are performed here.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <bstring.h>
#include <errno.h>

#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <xlv_oref.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <xlv_utils.h>
#include <xlv_lower_admin.h>
#include <xlv_cap.h>

#ifdef DEBUG
#define	DBG_PERROR(s)		perror(s)
#else
#define	DBG_PERROR(s)
#endif

#define	XLV_LOWER_RET_ERROR(err)	{ xlv_lower_errno = err; return(-1); }

int	xlv_lower_errno;	/* lib errno; valid iff ret code < 0 */


/*
 * Notify the kernel that there has been a configuration change and the
 * generation count should be changed.
 *
 * This routine should be called when changing a standalone plex or ve
 * and when changing a volume not in the kernel.
 */
static int
_notify_kernel(xlv_attr_cursor_t *cursor)
{
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	local;

	if (cursor == NULL) {
		cursor = &local;
		if (0 > syssgi(SGI_XLV_ATTR_CURSOR, cursor)) {
			DBG_PERROR("Failed to get a XLV cursor");
			return(-1);
		}
	}

	req.attr = XLV_ATTR_GENERATION;
	req.cmd  = XLV_ATTR_CMD_NOTIFY;
	req.ar_aru.aru_buf = NULL;

	return(cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req));
}

/*
 * Common internal function to remove a plex from a subvolume.
 * Depending on the value of "purge", the referenced plex can
 * be purged from the disk label or its subvolume/plex state
 * information can be cleared,
 *
 * Note: This plex it should not be the last plex of a subvolume.
 * If plex is a standalone plex, <purge> should be set.
 *
 * Note: Strategy is to first modify the labels and then the kernel.
 */
static int
_common_detach_plex(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *sv_oref,		/* subvolume object */
	xlv_oref_t	  *p_oref,		/* plex object */
	int		  purge)
{
	xlv_tab_subvol_t  *svp;
	xlv_tab_plex_t	  *pp;
	int		  status = 0;
	xlv_attr_req_t	  req;
	int		  depth, ve;

	if ( (!p_oref || !(pp = XLV_OREF_PLEX (p_oref))) ||
	     (sv_oref && !(svp = XLV_OREF_SUBVOL(sv_oref))) ) {
		ASSERT (0);
		XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
	}

	/*
	 * A plex becomes stale when it is removed.
	 * XXX Should it be "empty"?
	 */
	for (ve = 0; ve < pp->num_vol_elmnts; ve++) {
		if ((XLV_VE_STATE_OFFLINE != pp->vol_elmnts[ve].state) &&
		    (XLV_VE_STATE_INCOMPLETE != pp->vol_elmnts[ve].state)) {
			pp->vol_elmnts[ve].state = XLV_VE_STATE_STALE;
		}
	}

	/*
	 * Stand-alone plex do not require kernel interaction
	 * since the kernel doesn't know about stand-alone plexes.
	 */

	if (sv_oref) {
		/*
		 * Check subvolume depth.
		 *
		 * XXX 10/4/94 Currently xlv__lab2_write_lab() calculates
		 * the depth. If the depth is set here, xlv__lab2_write_lab()
		 * can just copy it. This way the depth isn't re-calculated
		 * for every disk label.
		 */
		depth = xlv_tab_subvol_depth(svp);
		if (depth != svp->subvol_depth)
			svp->subvol_depth = depth;

		/*
		 * Clear volume/subvolume/plex state information.
		 * This is now a stand-alone plex.
		 *
		 * Update the disk labels of all sibling plexes.
		 */
		xlv_lab2_write_oref_component(
			vh_list, p_oref, XLV_OBJ_TYPE_PLEX,
			sv_oref, XLV_OBJ_TYPE_SUBVOL, &status,
			XLV_LAB_WRITE_FULL);
	}

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	else if (purge) {
		/*
		 * Remove plex entry from the disk label.
		 */
		xlv_lab2_remove_oref_component (
			vh_list, p_oref, XLV_OBJ_TYPE_PLEX,
			&status, XLV_LAB_WRITE_FULL);

		/* check status after kernel update */
	}
	
	/*
	 * All label work is done so notify kernel of new configuration.
	 */
	if (sv_oref) {
		req.attr = XLV_ATTR_PLEX;
		req.cmd  = XLV_ATTR_CMD_REMOVE;
		req.ar_pp = pp;
		if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
			/*
			 * We ignore ENOENT errors. Okay if the object
			 * does not exist in the kernel cuz labels
			 * have already been modified.
			 */
			if (errno != ENOENT) {
				DBG_PERROR("DBG: _common_detach_plex()");
				XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
					XLV_LOWER_ESTALECURSOR :
					XLV_LOWER_EKERNEL))
			}
			_notify_kernel(cursor);
		}
	} else {
		_notify_kernel(cursor);
	}

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	return (0);

} /* _common_detach_plex() */


/*
 * Common internal function to remove a volume element.
 * The referenced volume element can be purged from the disk label
 * or its volume/subvolume/plex state information can be cleared,
 * depending on the value of "purge".
 */
static int
_common_detach_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *p_oref,		/* resulting plex object */
	xlv_oref_t	  *ve_oref,		/* ve object */
	int		  purge)		/* purge label or clear */
{
	xlv_tab_vol_elmnt_t	*vep;
	xlv_tab_plex_t		*pp;
	int			status = 0;
	xlv_attr_req_t		req;
	xlv_obj_type_t		level = XLV_OBJ_TYPE_PLEX;
	int			do_kernel_work = 0;

	if (ve_oref == NULL)
		return(0);

	vep = XLV_OREF_VOL_ELMNT (ve_oref);
	pp  = (p_oref) ? XLV_OREF_PLEX (p_oref) : NULL;

	if (!vep) {
		ASSERT (0);
		XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
	}

	/*
	 * Mark the volume element stale.
	 */
	if ((XLV_VE_STATE_OFFLINE != vep->state) &&
	    (XLV_VE_STATE_INCOMPLETE != vep->state)) {
		vep->state = XLV_VE_STATE_STALE;
	}

	/*
	 * Check for a stand-alone situation. This can be a
	 * stand-alone volume element or it can be a volume
	 * element in a stand-alone plex. A stand-alone situation
	 * do not require kernel interaction since the kernel
	 * doesn't know about stand-alone plex's and ve's.
	 */
	if (pp) {
		xlv_tab_subvol_t *svp;
		int		 depth;

		if (XLV_OREF_VOL(p_oref)) {
			/*
			 * This is part of a volume, so there must
			 * be a subvolume.
			 */
			if (!(svp = XLV_OREF_SUBVOL(p_oref))) {
				ASSERT (0);
				XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
			}

			do_kernel_work = 1;	/* do it after label update */

			depth = xlv_tab_subvol_depth(svp);
			if (depth != svp->subvol_depth) {
				/*
				 * When the depth changes, the whole
				 * subvolume must be made aware of the
				 * situation.
				 *
				 * XXX 10/4/94 See note above in
				 * _common_detach_plex() re: depth.
				 */
				svp->subvol_depth = depth;
				level = XLV_OBJ_TYPE_SUBVOL;
			}
		}

		/*
		 * Update this volume element's disk label and clear 
		 * volume/subvolume/plex state information. This is now
		 * a stand-alone volume element.
		 *
		 * Update the disk labels for any sibling volume elements.
		 */
		xlv_lab2_write_oref_component(
			vh_list, ve_oref, XLV_OBJ_TYPE_VOL_ELMNT,
			p_oref, level, &status, XLV_LAB_WRITE_FULL);
	}

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	else if (purge) {
		/*
		 * Purge volume element entry from disk labels.
		 */
		xlv_lab2_remove_oref_component (
			vh_list, ve_oref, XLV_OBJ_TYPE_VOL_ELMNT,
			&status, XLV_LAB_WRITE_FULL);

		/* check status after kernel update */
	}

	/*
	 * Notify kernel of new configuration
	 */
	if (do_kernel_work) {
		req.attr = XLV_ATTR_VE;
		req.cmd  = XLV_ATTR_CMD_REMOVE;
		req.ar_vep = vep;
		if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
			/*
			 * We ignore ENOENT errors. Could be operating
			 * on a shutdown'ed volume.
			 */
			if (errno != ENOENT) {
				DBG_PERROR("DBG: _common_detach_ve()");
				XLV_LOWER_RET_ERROR(((errno == ESTALE) ?
					XLV_LOWER_ESTALECURSOR :
					XLV_LOWER_EKERNEL))
			}
			_notify_kernel(cursor);
		}
	} else {
		_notify_kernel(cursor);
	}

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	return (0);

} /* _common_detach_ve() */



/*
 *	A d d   S e l e c t i o n s
 */

/*
 * Add a volume.
 */
/* ARGSUSED0 */
int
xlv_lower_add_vol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref)		/* volume object */
{
	xlv_lower_errno = XLV_LOWER_ENOSUPPORT;
	return (-1);
}

/*
 * Add a subvolume to an existing volume.
 *
 * Cannot add a subvolume to an existing volume. To do so
 * implies stand-alone subvolumes support. Adding a subvolume
 * does not make sense from a file system perspective because
 * one must do a mkfs offline.
 */
/* ARGSUSED0 */
int
xlv_lower_add_subvol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref)		/* volume object */
{
	xlv_lower_errno = XLV_LOWER_ENOSUPPORT;
	return (-1);
} /* xlv_lower_add_subvol() */


/*
 * Add a plex. The plex may be a stand-alone plex or it may be
 * joining an existing subvolume which is part of a volume. The
 * passed-in oref must resolve to a plex or a volume.
 */
int
xlv_lower_add_plex(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *I_oref)		/* resulting plex/vol object */
{
	xlv_tab_plex_t		*pp;
	xlv_tab_subvol_t	*svp;
	xlv_oref_t		oref;
	int			status = 0;
	xlv_obj_type_t          level;
	xlv_attr_req_t		req;
	int			depth;
	int			do_kernel_work = 0;
	int 			retrycnt = 0;

	pp = XLV_OREF_PLEX (I_oref);
	svp = XLV_OREF_SUBVOL (I_oref);

	/*
	 * When a plex joins a subvolume, the plex should not have
	 * its standalone name. This plex's name must be relative
	 * to the volume/subvolume.
	 */
#if 0
	if (pp->name[0]) {
		ASSERT(0);
		pp->name[0] = NULL;
	}
#endif

	if (svp) {
		if (! XLV_OREF_VOL(I_oref)) { ASSERT(0); goto invalid; }
		/*
		 * Check subvolume depth.
		 */
		depth = xlv_tab_subvol_depth(svp);
		if (depth != svp->subvol_depth) {
			/*
			 * XXX 10/4/94 See note above in
			 * _common_detach_plex() re: depth.
			 */
			svp->subvol_depth = depth;
		}
		do_kernel_work = 1;		/* do it after label update */
		level = XLV_OBJ_TYPE_SUBVOL;	/* update sibling plexes */
	}
	else {
		if (!pp) { ASSERT (0); goto invalid; }
		/*
		 * This is a stand-alone plex and since the
		 * kernel doesn't know about stand-alone plexes,
		 * no kernel action required.
		 */
		level = XLV_OBJ_TYPE_PLEX;
	}

	/*
	 * Write new disk labels.
	 */
	XLV_OREF_COPY(I_oref, &oref);
	oref.next = NULL;
	xlv_lab2_write_oref_component (
		vh_list, &oref, level, NULL, NULL,
		&status, XLV_LAB_WRITE_FULL);

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	/*
	 * Notify kernel of new configuration.
	 */
	if (do_kernel_work) {
		/*
		 * Pass to the kernel the new subvolume. The kernel
		 * merges in the new plex.
		 */
		req.attr = XLV_ATTR_SUBVOL;
		req.cmd  = XLV_ATTR_CMD_ADD;
		req.ar_svp = svp;
		while (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
			if (errno == ENOMEM) {
				/* 
				 * Retry if the kernel couldn't allocate mem
				 * on our behalf.  This lets the user see
				 * what's going on, and terminate the command
				 * if desired
				 */
				fprintf(stderr, "xlv_lower_add_plex: no kernel "
				"memory retrying again %d\n", retrycnt++);
				sleep(2);
				continue;
			} else if (errno != ENOENT) {
				/*
				 * We ignore ENOENT errors. Could be operating
				 * on a shutdown'ed volume.
				 */
				DBG_PERROR("DBG: xlv_lower_add_plex()");
				XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
					XLV_LOWER_ESTALECURSOR :
					XLV_LOWER_EKERNEL))
			}
			_notify_kernel(cursor);
			break;
		}
	} else {
		_notify_kernel(cursor);
	}

	return (0);

invalid:
	XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)

} /* xlv_lower_add_plex() */


/*
 * Add a volume element to an existing plex.
 */
int
xlv_lower_add_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t 	  *I_oref)		/* resulting plex/vol object */
{
	xlv_tab_vol_elmnt_t	*vep;
	xlv_tab_plex_t		*pp;
	xlv_oref_t		oref;
	int			status = 0;
	xlv_attr_req_t		req;
	xlv_obj_type_t		level = XLV_OBJ_TYPE_PLEX;
	int			do_kernel_work = 0;
	int 			retrycnt = 0;

	vep = XLV_OREF_VOL_ELMNT (I_oref);
	pp  = XLV_OREF_PLEX (I_oref);
	if (!vep || !pp) {
		ASSERT (vep && pp);
		xlv_lower_errno = XLV_LOWER_EINVALID;
		return (-1);
	}

	/*
	 * When a volume element joins a plex, the ve should not
	 * have its standalone name. This ve's name must be relative
	 * to the volume/subvolume.
	 */
#if 0
	if (vep->veu_name[0]) {
		ASSERT(0);
		vep->veu_name[0] = 0;
	}
#endif

	/*
	 * No kernel action is required for a stand-alone plex.
	 */
	if (pp && XLV_OREF_VOL(I_oref)) {
		xlv_tab_subvol_t *svp;
		int		 depth;

		/*
		 * This is part of a volume, so there must
		 * be a subvolume.
		 */
		if (!(svp = XLV_OREF_SUBVOL(I_oref))) {
			ASSERT (0);
			XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
		}

		do_kernel_work = 1;	 /* do it after label update */

		depth = xlv_tab_subvol_depth(svp);
		if (depth != svp->subvol_depth) {
			/*
			 * When the depth changes, the whole
			 * subvolume must be made aware of the
			 * situation.
			 *
			 * XXX 10/4/94 See note above in
			 * _common_detach_plex() re: depth.
			 */
			svp->subvol_depth = depth;
			level = XLV_OBJ_TYPE_SUBVOL;
		}
	}

	/*
	 * Write new disk label.
	 */
	XLV_OREF_COPY(I_oref, &oref);
	oref.next = NULL;
	xlv_lab2_write_oref_component (
		vh_list, &oref, level, NULL, NULL,
		&status, XLV_LAB_WRITE_FULL);

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	/*
	 * Notify kernel of new plex configuration. The
	 */
	if (do_kernel_work) {
		/*
		 * The kernel merges in the new volume element.
		 */
		req.attr = XLV_ATTR_PLEX;
		req.cmd  = XLV_ATTR_CMD_ADD;
		req.ar_pp = pp;
		while (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
			if (errno == ENOMEM) {
				/* 
				 * Retry if the kernel couldn't allocate mem
				 * on our behalf.  This lets the user see
				 * what's going on, and terminate the command
				 * if desired
				 */
				fprintf(stderr, "xlv_lower_add_ve: no kernel "
				"memory retrying again %d\n", retrycnt++);
				sleep(2);
				continue;
			} else if (errno != ENOENT) {
				/*
				 * We ignore ENOENT errors. If the object does 
				 * not exist in the kernel, go ahead and 
				 * modify the labels any way.
				 */
				DBG_PERROR("DBG: xlv_lower_add_ve()");
				XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
					XLV_LOWER_ESTALECURSOR :
					XLV_LOWER_EKERNEL))
			}
			 _notify_kernel(cursor);
			 break;
		}
	} else {
		_notify_kernel(cursor);
	}

	return (0);

} /* xlv_lower_add_ve() */


/*
 * Set volume element specific values.
 */
int
xlv_lower_set_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t 	  *ve_oref)		/* ve being set */
{
	xlv_tab_vol_elmnt_t	*vep;
	xlv_oref_t		oref;
	int			status = 0;
	xlv_attr_req_t		req;

	vep = XLV_OREF_VOL_ELMNT (ve_oref);
	if (!vep) {
		ASSERT (0);
		XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
	}

	/*
	 * Update disk label.
	 */
	XLV_OREF_COPY(ve_oref, &oref);
	oref.next = NULL;
	xlv_lab2_write_oref_component (
		vh_list, &oref, XLV_OBJ_TYPE_VOL_ELMNT, NULL, NULL,
		&status, XLV_LAB_WRITE_FULL);

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	/*
	 * Stand-alone volume element does not require direct
	 * kernel action because the kernel does not know it.
	 * But should notify kernel that there were label changes.
	 */
	if (XLV_OREF_VOL(ve_oref)) {
		/*
		 * Notify kernel of new settings.
		 */
		req.attr = XLV_ATTR_VE;
		req.cmd  = XLV_ATTR_CMD_SET;
		req.ar_vep = vep;
		if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
			 /*
			  * We ignore ENOENT errors. If the object does not
			  * exist in the kernel, go ahead and modify the
			  * labels any way.
			  */
			if (errno != ENOENT) {
				DBG_PERROR("DBG: xlv_lower_set_ve()");
				XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
					XLV_LOWER_ESTALECURSOR :
					XLV_LOWER_EKERNEL))
			}
			_notify_kernel(cursor);
		}
	} else {
		_notify_kernel(cursor);
	}

	return (0);

} /* xlv_lower_set_ve() */


/*
 * Add a partition to an existing volume element.
 *
 * Volume element must be offline.
 */
/* ARGSUSED0 */
int
xlv_lower_add_partition(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *ve_oref)
{
	XLV_LOWER_RET_ERROR(XLV_LOWER_ENOSUPPORT)
}


/*
 *	D e t a c h   S e l e c t i o n s
 *
 * The object is removed from the kernel's configuration and
 * the disk label is clear of (sub)volume state information.
 * The object is now a stand-alone item. Caller does not free
 * the in-core object.
 */

/*
 * Detaching a subvolume from an existing volume is not a supported
 * function.
 */
/* ARGSUSED0 */
int
xlv_lower_detach_subvol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref,
	xlv_oref_t	  *sv_oref)
{
	XLV_LOWER_RET_ERROR(XLV_LOWER_ENOSUPPORT)
}

/*
 * Detach a plex from an existing subvol.
 * The passed-in oref must resolve to a plex or a volume.
 */
int
xlv_lower_detach_plex(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *sv_oref,		/* resulting vol object */
	xlv_oref_t 	  *p_oref)		/* removed plex object */
{
	return ( _common_detach_plex(cursor, vh_list, sv_oref, p_oref, 0) );
}

/*
 * Detach a volume element from an existing plex. That plex
 * may be part of a subvolume (thus volume) or it may be
 * a stand-alone plex. Either case, the plex object must be
 * resolved to the corresponding level.
 *
 * Note: If this is the last volume element in the plex,
 * all saved knowledge of the plex would be destroy when
 * the new label is written onto the disk.
 */
int
xlv_lower_detach_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *p_oref,		/* resulting plex object */
	xlv_oref_t	  *ve_oref)		/* removed ve object */
{
	return ( _common_detach_ve(cursor, vh_list, p_oref, ve_oref, 0) );
}

/*
 *	R e m o v e   S e l e c t i o n s
 *
 * The object is removed from the kernel's configuration and
 * the disk label is purge from the disk. The caller should free
 * the in-core object.
 */


/*
 * Remove a volume.
 */
int
xlv_lower_rm_vol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref)		/* volume object */
{
	xlv_oref_t		oref;
	int			status = 0;
	xlv_attr_req_t		req;

	if (!XLV_OREF_VOL(v_oref)) {
		ASSERT (0);
		XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
	}

	/*
	 * Purge all volume element disk label entries belonging
	 * to the volume.
	 */
	XLV_OREF_COPY(v_oref, &oref);
	xlv_lab2_remove_oref_component (
		vh_list, &oref, XLV_OBJ_TYPE_VOL,
		&status, XLV_LAB_WRITE_FULL);

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	/*
	 * Change kernel configuration by removing volume.
	 */
	req.attr = XLV_ATTR_VOL;
	req.cmd  = XLV_ATTR_CMD_REMOVE;
	req.ar_vp = XLV_OREF_VOL(v_oref);
	if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
		/*
		 * We ignore ENOENT errors. If the object does not
		 * exist in the kernel, could be operating on a
		 * shutdown'ed volume. 
		 */
		if (errno != ENOENT) {
			DBG_PERROR("DBG: xlv_lower_rm_vol()");
			XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
				XLV_LOWER_ESTALECURSOR : XLV_LOWER_EKERNEL))
			
		}
		_notify_kernel(cursor);
	}

	return (0);

} /* end of xlv_lower_rm_vol() */


/*
 * Remove a subvolume from an existing volume.
 *
 * Note: This is should not be the last thing in the volume.
 */
int
xlv_lower_rm_subvol(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *v_oref,		/* volume object */
	xlv_oref_t	  *sv_oref)		/* subvolume object */
{
	xlv_tab_subvol_t	*svp;
	xlv_oref_t		oref;
	int			status = 0;
	xlv_attr_req_t		req;

	if (!XLV_OREF_VOL(v_oref) || !(svp = XLV_OREF_SUBVOL(sv_oref))) {
		ASSERT (0);
		XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
	}

	/*
	 * Change disk labels.
	 *
	 * Remove subvolume state information from other
	 * subvolumes in the volume.
	 */
	XLV_OREF_COPY(v_oref, &oref);
	oref.next = NULL;
	xlv_lab2_write_oref_component (
		vh_list, &oref, XLV_OBJ_TYPE_VOL, NULL, NULL,
		&status, XLV_LAB_WRITE_FULL);
	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	/*
	 * Purge all volume element disk label entries belonging
	 * to the subvolume.
	 */
	XLV_OREF_INIT(&oref);
	XLV_OREF_SET_SUBVOL(&oref, svp);
	xlv_lab2_remove_oref_component (
		vh_list, &oref, XLV_OBJ_TYPE_SUBVOL,
	  	&status, XLV_LAB_WRITE_FULL);

	if (status)
		XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	/*
	 * Change kernel configuration by removing subvolume.
	 */
	req.attr = XLV_ATTR_SUBVOL;
	req.cmd  = XLV_ATTR_CMD_REMOVE;
	req.ar_svp = svp;
	if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
		/*
		 * We ignore ENOENT errors. Could be operating
		 * on a shutdown'ed volume.
		 */
		if (errno != ENOENT) {
			DBG_PERROR("DBG: xlv_lower_rm_subvol()");
			XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
				XLV_LOWER_ESTALECURSOR : XLV_LOWER_EKERNEL))
		}
		_notify_kernel(cursor);
	}
 
	return (0);
} /* xlv_lower_rm_subvol() */

/*
 * Remove a plex from an existing subvolume.
 */
int
xlv_lower_rm_plex(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *sv_oref,		/* volume object */
	xlv_oref_t	  *p_oref)		/* plex object */
{
	return ( _common_detach_plex(cursor, vh_list, sv_oref, p_oref, 1) );
}

/*
 * Remove a volume element. The volume element may be a stand-alone
 * volume element or it may be part of a plex. "I_oref" is nil if this
 * is a stand_alone volume element.
 *
 * Note: This code replies on the caller to check that this ve is not
 * the last ve of a plex. If this is the last ve then either the remove
 * plex or remove subvolume function should be called instead.
 */
int
xlv_lower_rm_ve(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *I_oref,	/* resulting plex; nil if stand-alone */
	xlv_oref_t	  *ve_oref)
{
	return ( _common_detach_ve(cursor, vh_list, I_oref, ve_oref, 1) );
}


/*
 * Remove the component at the given path. The component can be a plex
 * or a volume element.
 *
 * This routine handles missing pieces. The component has missing pieces
 * or the component itself is missing.
 *
 * Notes:
 * (1) The <it_oref> may be NULL. That is the case when the component
 * being removed is missing. In such a situation, only the parent object
 * is updated. If <it_oref> in not NULL, it can be missing some parts.
 *
 * (2) The parent object cannot be missing other pieces. 
 */
int
xlv_lower_rm_path(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t    **vh_list,
	xlv_oref_t        *oref,		/* resulting obj */
	xlv_oref_t        *it_oref,		/* component removed */
	xlv_attr_cursor_t *path)		/* path of component removed */
{
	xlv_attr_req_t	req;
	int		status = 0;
	int		parent_type, it_type;
	int		do_kernel_work = 0;

	xlv_tab_subvol_t *svp;

	if (!path || !oref || 
	    (!(svp=XLV_OREF_SUBVOL(oref)) && !(XLV_OREF_PLEX(oref)))) {
		ASSERT(0);
		XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
	}

	if (svp) {
		do_kernel_work = 1;

		if (path->ve >= 0) {
			/*
			 * Update disk labels of all sibling ve's in the plex.
			 */
			parent_type = XLV_OBJ_TYPE_PLEX;
			it_type = XLV_OBJ_TYPE_VOL_ELMNT;
		} else {
			/*
			 * Update disk labels of all sibling plexes.
			 */
			parent_type = XLV_OBJ_TYPE_SUBVOL;
			it_type = XLV_OBJ_TYPE_PLEX;
		}

	} else {
		/*
		 * Removing a volume element from a standalone plex.
		 */
		parent_type = XLV_OBJ_TYPE_PLEX;
		it_type = XLV_OBJ_TYPE_VOL_ELMNT;
	}
	/*
	 * Clear volume/subvolume/plex state information.
	 *
	 * XXX See note above in _common_detach_plex() re: depth.
	 *
	 * XXX Should this really be XLV_LAB_WRITE_PARTIAL so the
	 * resulting parent object can be missing other pieces?
	 */
	xlv_lab2_write_oref_component(
		vh_list, oref, parent_type, NULL, NULL,
		&status, XLV_LAB_WRITE_FULL);

	if (!status && !it_oref) {
		/*
		 * The component can be missing pieces.
		 */
		xlv_lab2_remove_oref_component (
			vh_list, it_oref, it_type,
			&status, XLV_LAB_WRITE_PARTIAL);
	}

	if (status)
		 XLV_LOWER_RET_ERROR(XLV_LOWER_EIO)

	/*
	 * Notify kernel of new configuration
	 */
	if (do_kernel_work) {
		/*
		 * Removing a ve or plex from a subvolume.
		 */
		req.attr = XLV_ATTR_PATH;
		req.cmd  = XLV_ATTR_CMD_REMOVE;
		req.ar_path = path;
		if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
			/*
			 * We ignore ENOENT errors. Could be operatin
			 * on a shutdown'ed volume.
			 */
			if (errno != ENOENT) {
				DBG_PERROR("DBG: xlv_lower_rm_path()");
				XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
					XLV_LOWER_ESTALECURSOR :
					XLV_LOWER_EKERNEL))
			}
			_notify_kernel(cursor);
		}
	} else {
		_notify_kernel(cursor);
	}

	return(0);

} /* end of xlv_lower_rm_path() */


/*
 * Remove a partition from an existing volume element.
 *
 * Volume element must be offline.
 */
/* ARGSUSED0 */
int
xlv_lower_rm_partition(
	xlv_attr_cursor_t *cursor,
	xlv_vh_entry_t	  **vh_list,
	xlv_oref_t	  *ve_oref)
{
	XLV_LOWER_RET_ERROR(XLV_LOWER_ENOSUPPORT)
}


/*
 * Set the nodename in the XLV disk labels of all disks associated
 * with the given object(s)
 */
int
xlv_lower_set_nodename(
	xlv_attr_cursor_t	*cursor,
	xlv_vh_entry_t		**vh_list,
	xlv_oref_t		*oref_list,
	xlv_name_t		nodename)
{
	int status;

	xlv_lab2_write_nodename_oref(vh_list, oref_list, nodename, &status);

	_notify_kernel(cursor);

	return(status);
}


/*
 * Change the type of the object.
 */
int
xlv_lower_writelab(
	xlv_attr_cursor_t	*cursor,
	xlv_vh_entry_t		**vh_list,
	xlv_oref_t		*oref)
{
	int	status;

	/* Write to disk */
	xlv_lab2_write_oref(vh_list, oref, NULL, &status, XLV_LAB_WRITE_FULL);

	_notify_kernel(cursor);

	return(status);
}


/*
 * Change the name of the given object.
 *
 * Note: Don't need to check that the new name is unique because
 * the kernel only cares about uuid's.
 */
int
xlv_lower_rename(
	xlv_attr_cursor_t 	*cursor,
	xlv_vh_entry_t		**vh_list,
	xlv_oref_t		*oref,
	char			*newname)
{
	int			st;
	xlv_name_t		oldname;
	char			*namep;
	xlv_tab_vol_entry_t	*volp = NULL;
	xlv_tab_vol_elmnt_t	*vep = NULL;
	xlv_tab_plex_t		*plexp = NULL;
	xlv_attr_req_t		req;

	if (volp = XLV_OREF_VOL(oref)) {
		namep = volp->name;
	} else if (plexp = XLV_OREF_PLEX(oref)) {
		namep = plexp->name;
	} else if (vep = XLV_OREF_VOL_ELMNT(oref)) {
		namep = vep->veu_name;
	} else {
		XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
	}

	strncpy(oldname, namep, sizeof(xlv_name_t));
	strncpy(namep, newname, sizeof(xlv_name_t));

	/* Write to disk */
	xlv_lab2_write_oref(vh_list, oref, NULL, &st, XLV_LAB_WRITE_FULL);

	if (st) {
		/* Name didn't make it to disk so put back original name. */
		strncpy(namep, oldname, sizeof(xlv_name_t));
		return(st);

	}

	if (volp) {
		/*
		 * Rename the volume name in the kernel.
		 */
		req.attr = XLV_ATTR_VOL;
		req.cmd  = XLV_ATTR_CMD_SET;
		req.ar_vp = volp;
		if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
			/*
			 * We ignore ENOENT errors. If the object does not
			 * exist in the kernel, go ahead and change the labels. 
			 */
			if (errno != ENOENT) {
				DBG_PERROR("DBG: xlv_lower_rename()");
				strncpy(namep, oldname, sizeof(xlv_name_t));
				XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
				    XLV_LOWER_ESTALECURSOR : XLV_LOWER_EKERNEL))
			}
			_notify_kernel(cursor);
		}
	} else {
		_notify_kernel(cursor);
	}

	return(0);

} /* end of xlv_lower_rename() */

/*
 * Use to change to name, flags, stat, or rsvd field in a given volume's
 * xlv_tab_vol_entry_t data structure.  See io/xlv/xlv_attr.c for the
 * fields the can currently be set by syssgi call.
 *
 * Assumes volume fields have been changed by caller.
 */
int
xlv_lower_vol(
	xlv_attr_cursor_t 	*cursor,
	xlv_vh_entry_t		**vh_list,
	xlv_oref_t		*oref)
{
	int			st;
	xlv_tab_vol_entry_t	*volp = NULL;
	xlv_attr_req_t		req;

	if (!(volp = XLV_OREF_VOL(oref))) {
		XLV_LOWER_RET_ERROR(XLV_LOWER_EINVALID)
	}

	/* Write to disk */
	xlv_lab2_write_oref(vh_list, oref, NULL, &st, XLV_LAB_WRITE_FULL);

	if (st) {
		return(st);
	}

	/*
	 * Change the kernel view of the volume.
	 */
	req.attr = XLV_ATTR_VOL;
	req.cmd  = XLV_ATTR_CMD_SET;
	req.ar_vp = volp;
	if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req)) {
		/*
		 * We ignore ENOENT errors. If the object does not
		 * exist in the kernel, go ahead and change the labels. 
		 */
		if (errno != ENOENT) {
			DBG_PERROR("DBG: xlv_lower_vol()");
			XLV_LOWER_RET_ERROR (((errno == ESTALE) ?
			    XLV_LOWER_ESTALECURSOR : XLV_LOWER_EKERNEL))
		}
		_notify_kernel(cursor);
	}

	return(0);
} /* end of xlv_lower_vol() */
