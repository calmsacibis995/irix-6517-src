/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.16 $"

#include <stdio.h>
#include <stdlib.h>
#include <bstring.h>

#include <sys/debug.h>
#include <sys/syssgi.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <xlv_lab.h>
#include <xlv_lower_admin.h>

#include "xlv_mgr.h"

extern int xlv_tab_revive_subvolume(xlv_tab_subvol_t *xlv_p);

xlv_vh_entry_t	*xlv_vh_list = NULL;

xlv_tab_vol_t   *tab_vol = NULL;
xlv_tab_t       *tab_sv = NULL;

xlv_attr_cursor_t	xlv_cursor;
xlv_attr_cursor_t	*xlv_cursor_p = &xlv_cursor;


/*
 * Messages:
 */
#define	FMT_NONE                 0
#define	FMT_OBJ_NOTFOUND         1
#define	FMT_OBJ_EXISTS           2
#define	FMT_CANT_PARSE_TO_SUBVOL 3
#define	FMT_CANT_PARSE_TO_PLEX   4
#define	FMT_CANT_PARSE_TO_VE     5
#define	FMT_BAD_NAME             6
#define	FMT_UNKNOWN              7

static char *formats[] = {
  /* FMT_NONE */                 "format: NONE\n",
  /* FMT_OBJ_NOTFOUND */         "Object \"%s\" was not found.\n",
  /* FMT_OBJ_EXISTS */           "Object \"%s\" already exists.\n",
  /* FMT_CANT_PARSE_TO_SUBVOL */ "Cannot parse \"%s\" to a SUBVOL pathname.\n",
  /* FMT_CANT_PARSE_TO_PLEX */   "Cannot parse \"%s\" to a PLEX pathname.\n",
  /* FMT_CANT_PARSE_TO_VE */     "Cannot parse \"%s\" to a VE pathname.\n",
  /* FMT_BAD_NAME */             "Bad name \"%s\": Object name cannot have dots.\n",
  /* FMT_UNKNOWN */              "format: UNKNOWN\n"
};


static char *obj_type_str[] = {
	/* XLV_OBJ_TYPE_NONE */         "none",	
	/* XLV_OBJ_TYPE_VOL */          "vol",
	/* XLV_OBJ_TYPE_SUBVOL */       "subvol",
	/* XLV_OBJ_TYPE_PLEX */         "plex",
	/* XLV_OBJ_TYPE_VOL_ELMNT */    "ve",
	/* XLV_OBJ_TYPE_DISK_PART */    "dp",
	/* XLV_OBJ_TYPE_LOG_SUBVOL */   "log_sv",
	/* XLV_OBJ_TYPE_DATA_SUBVOL */  "datat_sv",
	/* XLV_OBJ_TYPE_RT_SUBVOL */    "rt_sv"
};



/*
 * Back-end routine for xlv_mgr.  These routines do not
 * deal with any TCL user interface data structures and only 
 * return status codes.  This way, they can be used with other
 * front-ends.
 * Note that we do use the TCL supplied hash table routines.
 * But these can be used with other user interfaces also.
 */

void
xlv_mgr_init (void)
{
	xlv_oref_t		*oref;
	xlv_oref_t		*oref_list;
	int			status;
#ifdef DEBUG
	xlv_vh_entry_t		*vh_list_ptr;
	xlv_vh_disk_label_t	*label;
#endif

	if (xlv_vh_list) {
		/*
		 * Already have a vh list, so free
		 * the current vh list before getting
		 * a new one.
		 */
		xlv_lab2_free_vh_list(&xlv_vh_list);

		/*
		 * Free up current hash table.
		 */
		Tcl_DeleteHashTable(&xlv_obj_table);

		if (tab_vol) {
			free(tab_vol);
			tab_vol = NULL;
		}
		if (tab_sv) {
			free(tab_sv);
			tab_sv = NULL;
		}
	}

	/*
	 * Read label from the disk volume header.
	 */
	xlv_lab2_create_vh_info_from_hinv (&xlv_vh_list, NULL, &status);

	/*
	 * Create the hash table.
	 */
	Tcl_InitHashTable (&xlv_obj_table, TCL_STRING_KEYS);

#ifdef DEBUG
	for (vh_list_ptr = xlv_vh_list; vh_list_ptr != NULL;
	     vh_list_ptr = vh_list_ptr->next) {
		label = XLV__LAB1_PRIMARY(vh_list_ptr);
		ASSERT(label->header.version != 1);
	}
#endif

	/*
	 * Generate the oref list but do not filter out
	 * any incomplete volumes. We want everything.
	 */
	xlv_vh_list_to_oref (
		xlv_vh_list, &oref_list, &tab_vol, &tab_sv, 0, XLV_READ_ALL);

	/*
	 * Add all the existing, complete objects to the hash table.
	 */
	for (oref = oref_list; oref != NULL; oref = oref->next) {
		oref->flag = 0;		/* Initialize flag -- not dirty */
		add_object_to_table (oref);
	}

	/*
	 * Get a XLV cursor to mark our context.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &xlv_cursor)) {
		printf ("Failed to get cursor to ensure consistency.\n");
	}

} /* end of xlv_mgr_init() */


/*
 ******************************************************************************
 *			I N T E R N A L   C O M M A N D S
 ******************************************************************************
 */

/*
 * xlv_mgr_attach
 */
int
xlv_mgr_attach (
	int	type,			/* add object of this type */
	char	*src_name,		/* standalone object of above type */
	char	*dst_name,		/* location of where to put object */
	int	mode,			/* insert or append */
	int	expert)			/* user is expert */
{
	xlv_mgr_cursor_t	cursor;
	xlv_tab_vol_entry_t	*vp;
	xlv_oref_t		*oref;		/* object to be added */
	int			retval = XLV_MGR_STATUS_OKAY;
	int	 		search_type;

	cursor.oref = NULL;
	oref = NULL;

	if (type != XLV_OBJ_TYPE_PLEX && type != XLV_OBJ_TYPE_VOL_ELMNT) {
		ASSERT (0);
		return (XLV_MGR_STATUS_FAIL);
	}

	/*
	 * Check that the source object is good. The object is good if
	 * it exists as configured, ie it is not missing any pieces.
	 */
	oref = find_object_in_table(src_name, type, NULL);
	if (oref == NULL) {
		retval = XLV_MGR_STATUS_FAIL;
		printf ("%s object \"%s\" does not exists.\n",
			(type == XLV_OBJ_TYPE_PLEX) ? "PLEX" : "VE",
			src_name);
		goto error;
	} else if (xlv_is_incomplete_oref(oref)) {
		retval = XLV_MGR_STATUS_FAIL;
		printf ("%s object \"%s\" is missing pieces.\n",
			(type == XLV_OBJ_TYPE_PLEX) ? "PLEX" : "VE",
			src_name);
		goto error;
	}

	/*
	 * Determine where the object is being added.
	 */
	if (mode == ATTACH_MODE_APPEND) {
		/*
		 * When appending to a standalone object to an
		 * existing object, the object being appended
		 * must be next in the hierarchy.
		 */
		ASSERT(XLV_OBJ_TYPE_SUBVOL == XLV_OBJ_TYPE_PLEX-1);
		ASSERT(XLV_OBJ_TYPE_PLEX == XLV_OBJ_TYPE_VOL_ELMNT-1);
		search_type = type - 1;
	} else {
		/*
		 * Must specify the actual location when inserting.
		 */
		ASSERT(mode == ATTACH_MODE_INSERT);
		search_type = type;
	}
	retval = parse_name (dst_name, search_type, &cursor);

	if (retval != XLV_MGR_STATUS_OKAY || cursor.oref == NULL) {
		int fmttype;

		if (search_type == XLV_OBJ_TYPE_VOL_ELMNT) {
			fmttype = FMT_CANT_PARSE_TO_VE;
		} else if (search_type == XLV_OBJ_TYPE_PLEX) {
			fmttype = FMT_CANT_PARSE_TO_PLEX;
		} else {
			fmttype = FMT_CANT_PARSE_TO_SUBVOL;
		}
		printf(formats[fmttype], dst_name);
		goto error;
	}

	/*
	 * If we are not an expert and this volume is missing pieces,
	 * the only valid operation is "delete object".
	 */
	if ((vp = XLV_OREF_VOL(cursor.oref)) && !expert) {
		if ((vp->state == XLV_VOL_STATE_MISS_UNIQUE_PIECE) ||
		    (vp->state == XLV_VOL_STATE_MISS_COMMON_PIECE)) {
			retval = E_VOL_MISS_PIECE;
			print_error (retval);
			goto error;
		}
	}

	if (type == XLV_OBJ_TYPE_PLEX) {
		if (vp && vp->flags & XLV_VOL_READONLY) {
			printf("Cannot attach plex to readonly volume %s\n",
			       vp->name);
			retval = XLV_MGR_STATUS_FAIL;
			goto error;
		}
		retval = xlv_attach_plex (&cursor, oref);
	} else {
		retval = xlv_attach_ve (&cursor, oref);
	}

	if (retval == XLV_MGR_STATUS_OKAY && XLV_OREF_SUBVOL(cursor.oref)) {
		/*
		 * Attached a ve or plex to a subvolume so 
		 * revive the subvolume to get everyboby to
		 * the same page.
		 */
		xlv_tab_revive_subvolume(XLV_OREF_SUBVOL(cursor.oref));
	}

	/*FALLTHROUGH*/
error:
	if (cursor.oref)
		free (cursor.oref);
	if (oref)
		free (oref);

	return (retval);

} /* end of xlv_mgr_attach() */


/*
 * xlv_mgr_detach() detaches a ve or plex from its parent object.
 */
/*ARGSUSED4*/
int
xlv_mgr_detach (
	int	type,
	char	*part_name,
	char	*dst_name,
	int	delete,
	int	expert,
	int	force)
{
	xlv_mgr_cursor_t	cursor;
	xlv_tab_vol_entry_t	*vp;
	xlv_oref_t		*oref;
	int			retval = XLV_MGR_STATUS_OKAY;

	cursor.oref = NULL;	/* initialize */

	if (type != XLV_OBJ_TYPE_PLEX && type != XLV_OBJ_TYPE_VOL_ELMNT) {
		ASSERT (0);
		return (XLV_MGR_STATUS_FAIL);
	}

	if (dst_name) {
		/*
		 * Check that the destination name isn't in use.
		 */
		oref = find_object_in_table(dst_name, XLV_OBJ_TYPE_NONE, NULL);
		if (oref) {
			retval = XLV_MGR_STATUS_FAIL;
			printf(formats[FMT_OBJ_EXISTS], dst_name);
			free(oref);	/* not used again, so free it */
			return(XLV_MGR_STATUS_FAIL);
		}
	}

	/*
	 * Determine what is being operated on.
	 */
	retval = parse_name (part_name, type, &cursor);

	if (retval != XLV_MGR_STATUS_OKAY || cursor.oref == NULL) {
		int fmttype;
		
		fmttype = (type == XLV_OBJ_TYPE_PLEX) ?
				FMT_CANT_PARSE_TO_PLEX : FMT_CANT_PARSE_TO_VE;
		printf(formats[fmttype], part_name);
		goto error;
	}

	/*
	 * If this volume is missing pieces and we are not "forcing" it,
	 * the only valid operation is "delete object".
	 */
	if ((vp = XLV_OREF_VOL(cursor.oref)) && !force) {
		if ((vp->state == XLV_VOL_STATE_MISS_UNIQUE_PIECE) ||
		    (vp->state == XLV_VOL_STATE_MISS_COMMON_PIECE)) {
			retval = E_VOL_MISS_PIECE;
			print_error (retval);
			goto error;
		}
	}

	if (type == XLV_OBJ_TYPE_PLEX) {
		retval = xlv_detach_plex (&cursor, dst_name, delete, force);
	} else {
		retval = xlv_detach_ve (&cursor, dst_name, delete, force);
	}

	/*
	 * XXX If a missing component has just been detached, should
	 * re-compute the volume state.
	 */

	/*FALLTHROUGH*/
error:
	if (cursor.oref)
		free (cursor.oref);
	return (retval);

} /* end of xlv_mgr_detach() */


/*
 * xlv_mgr_delete_object()
 *
 * Similar to xlv_admin/do.c$xlv_delete()
 */
int
xlv_mgr_delete_object (char *object_name)
{
	xlv_oref_t	*oref;
	int		retval = XLV_MGR_STATUS_OKAY;
	char		*nname = NULL;		/* nodename */
	char		*oname = NULL;		/* object name */

	oref = find_object_in_table(object_name, XLV_OBJ_TYPE_NONE, NULL);

	if ((oref == NULL) && (oname = strstr(object_name, "."))) {
		/*
		 * Cannot find object with that name.
		 * Check for the nodename.objectname syntax.
		 */
		nname = object_name;
		oname[0] = '\0';		/* terminate nodename string */
		oname++;
		oref = find_object_in_table(oname, XLV_OBJ_TYPE_NONE, nname);
	}

	if (oref == NULL) {
		if (nname) { oname[-1] = '.'; }	/* undo edit */
		printf(formats[FMT_OBJ_NOTFOUND], object_name);
		return(retval = XLV_MGR_STATUS_FAIL);
	}

	switch (oref->obj_type) {
	case XLV_OBJ_TYPE_VOL: {
		/*
		 * Volume must not be in use.
		 */
		retval = xlv_delete_vol(oref);
		break;
	}

	case XLV_OBJ_TYPE_PLEX: {
		/*
		 * Don't need to check usage because a standalone plex
		 * cannot be active.
		 */
		retval = xlv_delete_plex(oref, 1, 0);
		break;
	}

	case XLV_OBJ_TYPE_VOL_ELMNT: {
		/*
		 * Don't need to check usage because a standalone ve
		 * cannot be active.
		 */
		retval = xlv_delete_ve(oref, 1, 0);
		break;
	}

	case XLV_OBJ_TYPE_DISK_PART:
		/*
		 * Standalone disk parts are not supported.
		 */
		retval = XLV_MGR_STATUS_FAIL;
		break;

	case XLV_OBJ_TYPE_LOG_SUBVOL:
	case XLV_OBJ_TYPE_DATA_SUBVOL:
	case XLV_OBJ_TYPE_RT_SUBVOL:
		/*
		 * Standalone subvolumes are not supported.
		 */
		retval = XLV_MGR_STATUS_FAIL;
		break;

	default:
		ASSERT(0);
		retval = XLV_MGR_STATUS_FAIL;
	}

	free (oref);

	if (XLV_MGR_STATUS_OKAY == retval)
		printf("Object %s deleted.\n", object_name);

	return (retval);

} /* end of delete_object() */


/*
 * Set the nodename in the disk label header for every disk
 * that makes up this object.
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 */
int
xlv_mgr_set_nodename (
	char	*nodename,		/* new node name to be assigned */
	char	*object)		/* for this object */
{
	xlv_oref_t	*oref;	/* context for object */
	int		retval = XLV_MGR_STATUS_OKAY;

	oref = find_object_in_table (object, XLV_OBJ_TYPE_NONE, NULL);
	if (oref == NULL) {
		printf(formats[FMT_OBJ_NOTFOUND], object);
		retval = XLV_MGR_STATUS_FAIL;
	}
	else {
		oref->next = NULL;		/* be safe */
		if (0 > xlv_lower_set_nodename(
				xlv_cursor_p, &xlv_vh_list, oref, nodename)) {
			retval = XLV_MGR_STATUS_FAIL;
		}
		free (oref);

		/* XXXjleong  Change nodename in oref and vol entry.
		 * At this point, the nodename associated with the object
		 * has been changed.  But the oref in the hash table still
		 * has the old nodename.
		 */
	}

	return (retval);

} /* end of xlv_mgr_set_nodename() */


/*
 * Change a volume element state from "offline" to "empty"
 * and kick off a revive.
 */
int
xlv_mgr_online_ve (char *vename)
{
	xlv_mgr_cursor_t	cursor;
	xlv_oref_t		*oref;	/* context for object */
	xlv_tab_vol_elmnt_t	*vep;
	int			retval = XLV_MGR_STATUS_OKAY;
	int			plexcount;

	/*
	 * Determine what is being operated on.
	 */
	cursor.oref = NULL;	/* initialize */
	retval = parse_name (vename, XLV_OBJ_TYPE_VOL_ELMNT, &cursor);

	if (retval != XLV_MGR_STATUS_OKAY) {
		printf (formats[FMT_CANT_PARSE_TO_VE], vename);
		/* retval = XLV_MGR_STATUS_FAIL; */
		return (retval);
	}
	if (cursor.oref == NULL) {
		printf (formats[FMT_CANT_PARSE_TO_VE], vename);
		retval = XLV_MGR_STATUS_FAIL;
		goto done;
	}

	/*
	 * Now set the volume element.
	 */
	retval = xlv_ve_oref_from_cursor (&cursor);
	if (retval != XLV_MGR_STATUS_OKAY) {
		print_error(retval);
		goto done;
	}
	oref = cursor.oref;
	oref->next = NULL;		/* be safe */
	vep = XLV_OREF_VOL_ELMNT(oref);

	plexcount = (NULL == XLV_OREF_SUBVOL(oref)) ? 1 :
				XLV_OREF_SUBVOL(oref)->num_plexes;

	if (XLV_VE_STATE_INCOMPLETE == vep->state) {
		/*
		 * Cannot activate an incomplete ve.
		 */
		printf ("Cannot take incomplete ve \"%s\" online.\n", vename);
		retval = XLV_MGR_STATUS_FAIL;
		goto done;

	} else if (XLV_VE_STATE_ACTIVE == vep->state) {
		/*
		 * Currently the ve is already marked active.  Don't
		 * change the ve state.
		 */
		printf ("Ve \"%s\" is already online and active.\n", vename);
		goto done;

	} else {
		printf ("Change ve \"%s\" to empty for revival.\n", vename);
	}

	vep->state = XLV_VE_STATE_EMPTY;

	if (0 > xlv_lower_set_ve(xlv_cursor_p, &xlv_vh_list, oref)) {
		print_error(xlv_lower_errno);
		retval = XLV_MGR_STATUS_FAIL;
		goto done;
	}

	/*
	 * Revive the subvolume to get everyboby on the same page.
	 */
	if (plexcount > 1) {
		xlv_tab_revive_subvolume(XLV_OREF_SUBVOL(oref));
	}

	/*FALLTHROUGH*/
done:
	if (cursor.oref)
		free (cursor.oref);

	return (retval);

} /*end of xlv_mgr_online_ve() */


/*
 * Change a volume element usability from "online" to "offline".
 */
int
xlv_mgr_offline_ve (char *vename)
{
	xlv_mgr_cursor_t	cursor;
	xlv_oref_t		*oref;	/* context for object */
	xlv_tab_vol_elmnt_t	*vep;
	int			retval = XLV_MGR_STATUS_OKAY;

	/*
	 * Determine what is being operated on.
	 */
	cursor.oref = NULL;	/* initialize */
	retval = parse_name (vename, XLV_OBJ_TYPE_VOL_ELMNT, &cursor);

	if (retval != XLV_MGR_STATUS_OKAY) {
		printf(formats[FMT_CANT_PARSE_TO_VE], vename);
		/* retval = XLV_MGR_STATUS_FAIL; */
		return(retval);
	}
	if (cursor.oref == NULL) {
		printf(formats[FMT_CANT_PARSE_TO_VE], vename);
		retval = XLV_MGR_STATUS_FAIL;
		goto done;
	}

	/*
	 * Now set the volume element.
	 */
	retval = xlv_ve_oref_from_cursor (&cursor);
	if (retval != XLV_MGR_STATUS_OKAY) {
		print_error(retval);
		goto done;
	}
	oref = cursor.oref;
	oref->next = NULL;		/* be safe */
	vep = XLV_OREF_VOL_ELMNT(oref);

	if (XLV_VE_STATE_INCOMPLETE == vep->state) {
		/*
		 * Cannot deactivate an incomplete ve.
		 */
		printf ("Cannot change incomplete ve \"%s\"\n", vename);
		retval = XLV_MGR_STATUS_FAIL;
		goto done;

	} else {
		printf ("Change ve \"%s\" to offline.\n", vename);
	}

	vep->state = XLV_VE_STATE_OFFLINE;

	if (0 > xlv_lower_set_ve(xlv_cursor_p, &xlv_vh_list, oref)) {
		print_error(xlv_lower_errno);
		retval = XLV_MGR_STATUS_FAIL;
	}

	/*FALLTHROUGH*/
done:
	if (cursor.oref)
		free (cursor.oref);

	return (retval);

} /*end of xlv_mgr_offline_ve() */


/*
 * Change a standalone volume element's start address  
 */
int
xlv_mgr_start_ve (__int64_t start_blkno, char *vename)
{
	xlv_oref_t		*oref;	/* context for object */
	__int64_t		delta_blkno;
	xlv_tab_vol_elmnt_t	*vep;
	int			retval = XLV_MGR_STATUS_OKAY;

	oref = find_object_in_table(vename, XLV_OBJ_TYPE_VOL_ELMNT, NULL);
	if (oref == NULL) {
		printf(formats[FMT_OBJ_NOTFOUND], vename);
		return(retval = XLV_MGR_STATUS_FAIL);
	}

	vep = XLV_OREF_VOL_ELMNT(oref);

	delta_blkno = start_blkno - vep->start_block_no;
	vep->start_block_no += delta_blkno;
	vep->end_block_no += delta_blkno;
		

	if (0 > xlv_lower_writelab(xlv_cursor_p, &xlv_vh_list, oref))
		retval = XLV_MGR_STATUS_FAIL;

	return (retval);

} /*end of xlv_mgr_start_ve() */

int
get_oref_name_type(xlv_oref_t *oref, char **name, int *typep)
{
	char	*oref_name;
	int	oref_type;

	if (XLV_OREF_VOL(oref)) {
		oref_name = XLV_OREF_VOL(oref)->name;
		oref_type = XLV_OBJ_TYPE_VOL;
		
	} else if (XLV_OREF_PLEX(oref)) {
		oref_name = XLV_OREF_PLEX(oref)->name;
		oref_type = XLV_OBJ_TYPE_PLEX;

	} else if (XLV_OREF_VOL_ELMNT(oref)) {
		oref_name = XLV_OREF_VOL_ELMNT(oref)->veu_name;
		oref_type = XLV_OBJ_TYPE_VOL_ELMNT;

	} else {
		return(1);
	}

	*name = oref_name;
	*typep = oref_type;

	return(0);
}


/*
 * Change the name of an object.
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 */
int
xlv_mgr_rename (
	char	*old,		/* current object name */
	char	*new)		/* new name for object */
{
	xlv_oref_t	*oref, *new_oref;
	int		retval = XLV_MGR_STATUS_OKAY;
	char		*nname = NULL;		/* nodename */
	char		*oname = NULL;		/* object name */

	oref = find_object_in_table(old, XLV_OBJ_TYPE_NONE, NULL);

	if ((oref == NULL) && (oname = strstr(old, "."))) {
		/*
		 * Cannot find object with that name.
		 * Check for the nodename.objectname syntax.
		 */
		nname = old;
		oname[0] = '\0';		/* terminate nodename string */
		oname++;
		oref = find_object_in_table(oname, XLV_OBJ_TYPE_NONE, nname);
	}

	if (oref == NULL) {
		if (nname) {oname[-1] = '.';}	/* undo edit */
		printf(formats[FMT_OBJ_NOTFOUND], old);
		return(retval = XLV_MGR_STATUS_FAIL);
	}

	/*
	 * Check that syntax of the new name is okay. 
	 */
	if (strstr(new, ".")) {
		printf(formats[FMT_BAD_NAME], new);
		return(retval = XLV_MGR_STATUS_FAIL);
	}

	/*
	 * Check that the destination name isn't in use.
	 */
	new_oref = find_object_in_table(new, XLV_OBJ_TYPE_NONE, nname);
	if (new_oref) {
		retval = XLV_MGR_STATUS_FAIL;
		printf(formats[FMT_OBJ_EXISTS], new);
		free(oref);
		free(new_oref);
		return(retval = XLV_MGR_STATUS_FAIL);
	}

	/*
	 * Delete the entry with the old name from the hash table
	 * and rename the object.
	 */
	(void)remove_object_from_table(oref, 1);
	if (0 > xlv_lower_rename(xlv_cursor_p, &xlv_vh_list, oref, new)) {
		retval = XLV_MGR_STATUS_FAIL;
	}

	/*
	 * Whether or not the rename happened, oref should have the
	 * current name. Add the oref back into the table.
	 */
	add_object_to_table(oref);

	free(oref);

	return(retval);

} /* end of xlv_mgr_rename() */



/*
 * Change the type of a standalone object.
 *
 * <type> can only be [ vol | plex | ve ]
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 *
 * Note: There is a slow memory leak from not dealloc'ing objects
 * that have changed type.
 */
int
xlv_mgr_retype (
	int	type,		/* new type */
	char	*object)	/* new name for object */
{
	int		retval = XLV_MGR_STATUS_OKAY;
	int		oldtype;
	int		idx;
	char		*name;
	char		*new_name;
	char		*new_str, *old_str;
	xlv_oref_t	*oref;

	xlv_tab_vol_entry_t *volp = NULL;
	xlv_tab_subvol_t    *svp = NULL;
	xlv_tab_plex_t      *plexp = NULL;
	xlv_tab_vol_elmnt_t *vep = NULL;

	xlv_oref_t	new_oref_store;
	xlv_oref_t	*new_oref;

#define	GOOD_VE(ve)	(((ve)->state != XLV_VE_STATE_OFFLINE) && \
			 ((ve)->state != XLV_VE_STATE_INCOMPLETE))
	
	new_oref = &new_oref_store;
	XLV_OREF_INIT(new_oref);

	oref = find_object_in_table(object, XLV_OBJ_TYPE_NONE, NULL);
	if (oref == NULL) {
		printf(formats[FMT_OBJ_NOTFOUND], object);
		return(retval = XLV_MGR_STATUS_FAIL);
	}

	/*
	 * Check that the object can change type. Promotion is okay,
	 * but demotion requires some checking.
	 */
	if (0 != get_oref_name_type(oref, &name, &oldtype)) {
		retval = XLV_MGR_STATUS_FAIL;
		goto noretype;
	}
	new_str = obj_type_str[type];
	old_str = obj_type_str[oldtype];
	if (oldtype == type) {
		printf("Object %s is already of type %s.\n", name, new_str);
		goto noretype;

	} else if (oldtype < type) {
		/*
		 * Demotion
		 */
		switch(oldtype) {
		    case XLV_OBJ_TYPE_VOL:		/* vol ==> plex | ve */
		    {
			int  found = 0;
			char *errfmt = NULL;

			volp = XLV_OREF_VOL(oref);
			svp = volp->data_subvol;

			retval = xlv_vol_isfree(oref, &found);
			if (XLV_MGR_STATUS_OKAY != retval) {
				print_error(retval);
			} else if (found) {
				errfmt = "Vol %s is in the kernel.\n";
			} else if (volp->log_subvol) {
				errfmt = "Vol %s has a log subvolume.\n";
			} else if (volp->rt_subvol) {
				errfmt = "Vol %s has a rt subvolume.\n";
			} else if (svp == NULL) {
				errfmt = "Vol %s has a NULL data subvolume.\n";
			} else if (svp->num_plexes != 1) {
				errfmt = "Vol %s has than 1 plex.\n";
			} else if (NULL == (plexp = svp->plex[0])) {
				errfmt = "Vol %s has a NULL plex.\n";
			} else if (XLV_OBJ_TYPE_VOL_ELMNT == type
					&& plexp->num_vol_elmnts != 1) {
				errfmt = "Plex %s.data.0 has than 1 ve.\n";
			} else {
				/*
				 * Passed all tests so demote.
				 *
				 * Note that the vol, subvol, plex structures
				 * are not being freed. There are possible
				 * leaks.
				 */
				if (XLV_OBJ_TYPE_PLEX == type) {
					/* vol ==> plex */
					new_name = plexp->name;
					for(idx=0, vep=&plexp->vol_elmnts[0];
					    idx<plexp->num_vol_elmnts;
					    idx++, vep++) {
						if (!GOOD_VE(vep)) {
							continue;
						}
						vep->state = XLV_VE_STATE_EMPTY;
					}
					XLV_OREF_SET_PLEX(new_oref, plexp);
				} else {
					/* vol ==> ve */
					vep = (xlv_tab_vol_elmnt_t *)
							malloc(sizeof(*vep));
					bcopy(plexp->vol_elmnts,
					      vep, sizeof(*vep));
					new_name = vep->veu_name;
					if (GOOD_VE(vep)) {
						vep->state = XLV_VE_STATE_EMPTY;
					}
					XLV_OREF_SET_VOL_ELMNT(new_oref, vep);
				}
				bcopy(name, new_name, sizeof(xlv_name_t));
				break;
			}

			/* At this point, there must have been a failure */
			if (errfmt) {
				printf(errfmt, name);
			}
			retval = XLV_MGR_STATUS_FAIL;
			goto noretype;
		    }

		    case XLV_OBJ_TYPE_PLEX:		/* plex ==> ve */
		    {
			ASSERT(type == XLV_OBJ_TYPE_VOL_ELMNT);
			plexp = XLV_OREF_PLEX(oref);
			if (plexp->num_vol_elmnts != 1) {
				printf("Plex %s has more than 1 ve.\n", name);
				retval = XLV_MGR_STATUS_FAIL;
				goto noretype;
			}
			vep = (xlv_tab_vol_elmnt_t *)malloc(sizeof(*vep));
			bcopy(plexp->vol_elmnts, vep, sizeof(*vep));
			bcopy(name, vep->veu_name, sizeof(xlv_name_t));
			if (GOOD_VE(vep)) {
				vep->state = XLV_VE_STATE_EMPTY;
			}
			XLV_OREF_SET_VOL_ELMNT(new_oref, vep);
			break;
		    }
		}

	} else /* if (oldtype > type) */ {
		/*
		 * Promotion:
		 */
		unsigned	status;

		plexp = XLV_OREF_PLEX(oref);	/* initialized */

		switch(oldtype) {
		    case XLV_OBJ_TYPE_VOL_ELMNT:	/* ve ==> vol | plex */
		    {
			vep = XLV_OREF_VOL_ELMNT(oref);

			/* create the plex */
			plexp = (xlv_tab_plex_t *)malloc(sizeof(*plexp));
			bzero(plexp, sizeof(*plexp));
			uuid_create(&plexp->uuid, &status);
			if (status != 0) {
				printf("WARNING: failed to create uuid.\n");
			}
			plexp->num_vol_elmnts = 1;
			plexp->max_vol_elmnts = 1;
			bcopy(vep, plexp->vol_elmnts, sizeof(*vep));

			if (XLV_OBJ_TYPE_PLEX == type) {
				bcopy(name, plexp->name, sizeof(xlv_name_t));
				if (GOOD_VE(vep)) {
					vep->state = XLV_VE_STATE_EMPTY;
				}
				XLV_OREF_SET_PLEX(new_oref, plexp);
				break;
			}
			/*FALLTHRU*/
		    }

		    case XLV_OBJ_TYPE_PLEX: 		/* plex ==> vol */
		    {
			volp = (xlv_tab_vol_entry_t *)malloc(sizeof(*volp));
			svp = (xlv_tab_subvol_t *)malloc(sizeof(*svp));
			bzero(volp, sizeof(*volp));
			bzero(svp, sizeof(*svp));

			uuid_create(&volp->uuid, &status);
			if (status != 0) {
				printf("WARNING: failed to create uuid.\n");
			}
			volp->data_subvol = svp;

			uuid_create(&svp->uuid, &status);
			if (status != 0) {
				printf("WARNING: failed to create uuid.\n");
			}

			for(idx=0, vep=&plexp->vol_elmnts[0];
			    idx<plexp->num_vol_elmnts;
			    idx++, vep++) {
				if (GOOD_VE(vep)) {
					vep->state = XLV_VE_STATE_EMPTY;
				}
			}

			svp->vol_p = volp;
			svp->subvol_type = XLV_SUBVOL_TYPE_DATA; 
			svp->num_plexes = 1;
			svp->plex[0] = plexp;
			svp->subvol_size = xlv_tab_subvol_size(svp);
			svp->subvol_depth = (short)xlv_tab_subvol_depth(svp);

			bcopy(name, volp->name, sizeof(xlv_name_t));
			XLV_OREF_SET_VOL(new_oref, volp);
			break;
		    }
		} /* switch */
	}

	/*
	 * Delete the entry with the old name from the hash table
	 * and change the type of the object.
	 */
	(void)remove_object_from_table(oref, 1);
	if (0 > xlv_lower_writelab(xlv_cursor_p, &xlv_vh_list, new_oref)) {
		retval = XLV_MGR_STATUS_FAIL;
	}

	/*
	 * Add the "good" oref back into the hash table.
	 */
	add_object_to_table((retval == XLV_MGR_STATUS_OKAY) ? new_oref : oref);

noretype:
	if (XLV_MGR_STATUS_FAIL == retval) {
		printf("Cannot change %s %s to a %s.\n",
			old_str, name, new_str);
	}
	free(oref);

	return(retval);

} /* end of xlv_mgr_retype() */

/*
 * Change read only bit on volume.
 */
int
xlv_mgr_readonly_vol (
	char    *name,		/* Volume name. */
	int     flag )		/* 1 = on, 0 = off */
{
	xlv_oref_t		*oref;	/* context for object */
	xlv_tab_vol_entry_t	*vol_p;
	int			retval = XLV_MGR_STATUS_OKAY;

	oref = find_object_in_table(name, XLV_OBJ_TYPE_VOL, NULL);
	if (oref == NULL) {
		printf(formats[FMT_OBJ_NOTFOUND], name);
		return(retval = XLV_MGR_STATUS_FAIL);
	}
	vol_p = XLV_OREF_VOL(oref);
	/*
	 * A volume may not be set to read-only if it is mirrored.
	 */
	if ((vol_p->log_subvol  && (vol_p->log_subvol->num_plexes  > 1)) ||
	    (vol_p->data_subvol && (vol_p->data_subvol->num_plexes > 1)) ||
	    (vol_p->rt_subvol   && (vol_p->rt_subvol->num_plexes   > 1)))
	{
		printf("Cannot change readonly bit for plexed volume %s.\n",
		       vol_p->name);
		return(XLV_MGR_STATUS_FAIL);
	}

	if (flag)
		XLV_OREF_VOL(oref)->flags |= XLV_VOL_READONLY;
	else
		XLV_OREF_VOL(oref)->flags &= ~XLV_VOL_READONLY;

	if (0 > xlv_lower_vol(xlv_cursor_p, &xlv_vh_list, oref))
		retval = XLV_MGR_STATUS_FAIL;

	return(retval);

} /* end of xlv_mgr_readonly_vol() */
