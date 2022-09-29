
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.7 $"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/debug.h>
#include <sys/types.h>

#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_error.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <xlv_lab.h>
#include <xlv_lower_admin.h>
#include <tcl.h>
#include "xlv_mgr.h"

extern xlv_vh_entry_t   *xlv_vh_list;

/*
 ******************************************************************
 *
 * Destroy the volume specified by <oref> but the oref structure
 * itself is not freed.
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 *
 ******************************************************************
 */
int
xlv_delete_vol (xlv_oref_t *oref)
{
	int	rval = XLV_MGR_STATUS_OKAY;
	int	found;

	/*
	 * Verify that this is a volume.
	 */
	if (oref->obj_type != XLV_OBJ_TYPE_VOL) {
#ifdef DEBUG
fprintf (stderr, "DBG: xlv_delete_vol() called without a vol oref\n");
#endif
		fprintf(stderr, "Internal error: object is not a volume.\n");
		return (INVALID_OPER_ENTITY);
	}

	/*
	 * Verify that volume is not in use.
	 */
	rval = xlv_vol_isfree(oref, &found);
	if (rval != XLV_MGR_STATUS_OKAY) {
		print_error(rval);
		return(rval);
	}

	/*
	 * Remove the volume information from the kernel and disk labels.
	 */
	if (0 != xlv_lower_rm_vol (xlv_cursor_p, &xlv_vh_list, oref)) {
#ifdef DEBUG
fprintf (stderr, "DBG: xlv_delete_vol(): failed in xlv_lower_rm_vol\n");
#endif
		rval = xlv_lower_errno;		/* XXXjleong Check error */
		print_error(rval);
		return(rval);
	}

	/*
	 * Remove the entry from the hash table.
	 */
	if (0 != remove_object_from_table(oref, 1)) {
		return (XLV_MGR_STATUS_FAIL);	/* XXXjleong better error */
	}

	free_vol_space(oref->vol);
	oref->vol = NULL;

	return(XLV_MGR_STATUS_OKAY);

} /* end of xlv_delete_vol() */


/*
 * xlv_vol_isfree(xlv_oref_t *oref, int *found)
 *
 * Searches through kernel table until
 *	1) ENOENT is returned - kernel table does not exists.
 *		This is ok. There are valid reasons for none
 *		of the volumes being assembled.	(ex. all volumes
 *		are plexed & plexing was disabled)
 *	2) ENFILE is returned - volume is not in the kernel table.
 *		This is ok. For some reason the volume may not have
 *		been assembled (ex. plexing was disabled)
 *	3) Volume is found.
 *
 *	If the volume is found then check if any of its subvolumes are
 *	busy.  If they are busy we set rval to E_SUBVOL_BUSY
 *	
 * Return codes:
 *	If ENOENT or ENFILE is encountered return XLV_MGR_STATUS_OKAY
 *	that implies that the (non-existent) volume is not busy. If the
 *	volume is located and its subvolumes are not busy return
 *	XLV_MGR_STATUS_OKAY.
 *
 *	If any other error is encountered, including E_SUBVOL_BUSY, 
 *	return the error code.
 */
int
xlv_vol_isfree (xlv_oref_t *oref, int *found)
{
	int			rval = XLV_MGR_STATUS_OKAY;
	xlv_attr_req_t 		req;
	xlv_tab_vol_entry_t	*vol;
	unsigned		st;

	/*
	 * Allocate space for a full sized volume.
	 */
	vol = get_vol_space();
	if (NULL == vol)
		return (ENOMEM);

	req.attr = XLV_ATTR_VOL;
	req.ar_vp = vol;

	/*
	 * Must clear the xlv cursor so the kernel query
	 * can start at the beginning.
	 */
	xlv_cursor_p->vol = -1;
	xlv_cursor_p->subvol = -1;
	xlv_cursor_p->plex = -1;
	xlv_cursor_p->ve = -1;

	*found = 0;		/* initialized as not found in kernel */

	/*
	 * Find the named volume in the kernel volume table.
	 */
	for (;;) {
		if (syssgi(SGI_XLV_ATTR_GET, xlv_cursor_p, &req) < 0) {
			/*
			 * Can't get information from kernel.
			 */
			rval = oserror();
			if ((rval == ENFILE) || (rval == ENOENT)) {
				/*
				 * Could be end of the kernel table.
				 * In which case, the volume was not found.
				 */
				rval = XLV_MGR_STATUS_OKAY;
			} else if (rval == ESTALE) {
				/*
				 * Cursor is stale so the kernel config
				 * changed. Cannot re-init and continue
				 * cuz the oref has a reference to an
				 * existing data structure. Re-init'ing
				 * here leaves a dangling reference in
				 * the oref (bug 382630). It is best to
				 * just return the stale error code.
				 */
				rval = XLV_LOWER_ESTALECURSOR; 
			}
			goto done;
		}

		/*
		 * Got a volume from the kernel, is it the one we want?
		 */
		if (!strcmp(oref->vol->name, vol->name)) {
			*found = 1;
			break;		/* this is the one */
		}
	}

	/*
	 * At this point, found the volume in the kernel.
	 * Check if the subvolumes are busy (mounted).
	 */
	if ((vol->log_subvol
		&& !(uuid_is_nil (&(vol->log_subvol->uuid), &st))
		&& XLV_ISOPEN(vol->log_subvol->flags)) ||
	    (vol->data_subvol
		&& !(uuid_is_nil (&(vol->data_subvol->uuid), &st))
		&& XLV_ISOPEN(vol->data_subvol->flags)) ||
   	    (vol->rt_subvol
		&& !(uuid_is_nil (&(vol->rt_subvol->uuid), &st))
		&& XLV_ISOPEN(vol->log_subvol->flags))) {
			rval = E_SUBVOL_BUSY;
	}

	/*FALLTHRU*/
done:
	free_vol_space (vol);
	return (rval);

} /* end of xlv_vol_isfree() */
