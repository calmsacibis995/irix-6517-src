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
#ident "$Revision: 1.17 $"

#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <pathnames.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <xlv_error.h>
#include "xlv_make_int.h"
#include <sys/debug.h>


static int	xlv_mk_add_completed_obj_called=0;
xlv_vh_entry_t	*xlv_vh_list = NULL;


#include <tcl.h>

#define YES 1
#define NO 0

/*
 * We maintain a single table for all objects so that we can
 * search in one place should the user give us a name.
 */

Tcl_HashTable	xlv_obj_table;

/*
 * We also maintain an incore list of all the volume headers that we read
 * in.  As we modify the on-disk labels, this incore data structure is
 * updated. We will use it to generate the list of XLV objects during
 * init and also to write out the new list at the end of xlv_make.
 */


void
xlv_mk_add_completed_obj (xlv_mk_context_t *xlv_mk_context,int initflag)
{
	xlv_oref_t	*hash_item;	/* new object we want to add. */

	/*
	 * The hash key must be null-terminated.  So we allocate
	 * an extra byte for the NULL in case the top-level
	 * object name takes up XLV_NAME_LEN characters.
	 */

	char		hash_key [XLV_NAME_LEN+1];
	Tcl_HashTable	*hash_table;
	int		new_entry_created;

	Tcl_HashEntry	*curr_entry;	/* current entry hashed to 
					   same location. */

	/*
	 * Set global so quit knows that work was done
	 */
	if (!initflag)
		xlv_mk_add_completed_obj_called = 1;
	hash_item = (xlv_oref_t *) malloc (sizeof (xlv_oref_t));
	*hash_item = *xlv_mk_context;

	if (XLV_OREF_VOL (xlv_mk_context) != NULL) {
		strncpy (hash_key, XLV_OREF_VOL(xlv_mk_context)->name,
			 XLV_NAME_LEN);
	}
	else if (XLV_OREF_PLEX (xlv_mk_context) != NULL) {
		strncpy (hash_key, XLV_OREF_PLEX(xlv_mk_context)->name,
			 XLV_NAME_LEN);
	}
	else if (XLV_OREF_VOL_ELMNT (xlv_mk_context) != NULL) {
		strncpy (hash_key, XLV_OREF_VOL_ELMNT(xlv_mk_context)->veu_name,
			 XLV_NAME_LEN);
	}
	else {
		ASSERT (0);	/* This object is not complete */
	}

	hash_table = &xlv_obj_table;
	hash_key[XLV_NAME_LEN] = '\0';

	if (XLV_OREF_SUBVOL (xlv_mk_context) != NULL) {
		/*
		 * Set the subvolume depth here. This information is
		 * needed by disk-label-reading function to determine
		 * if a "unique" or "common" piece is missing when
		 * there are missing parts.
		 */
		XLV_OREF_SUBVOL(xlv_mk_context)->subvol_depth =
			(short) xlv_tab_subvol_depth(XLV_OREF_SUBVOL(xlv_mk_context));
	}

	/*
	 * Add this item to the hash table. 
	 * 
	 * We only want to add one entry under each hash key. Since
	 * there may be multiple objects with the same name, we 
	 * check for an existing entry & add the new one to the
	 * end if there is an entry already.
	 */
	curr_entry = Tcl_CreateHashEntry (
                        hash_table, hash_key, &new_entry_created);

	if (new_entry_created) {
		hash_item->next = NULL;
	}
	else {
		/*
		 * This key already exists in the hash table.
		 * See if it is the same object.
		 */
		xlv_oref_t *oref;

		oref = (xlv_oref_t *)Tcl_GetHashValue(curr_entry);
		while (oref) {
			if (XLV_OREF_VOL (hash_item)) {
				if (XLV_OREF_VOL(oref) ==
				    XLV_OREF_VOL(hash_item)) {
					free(hash_item);
					return;
				}
			}
			else if (XLV_OREF_PLEX (hash_item)) {
				if (XLV_OREF_PLEX(oref) ==
				    XLV_OREF_PLEX(hash_item)) {
					free(hash_item);
					return;
				}
			}
			else if (XLV_OREF_VOL_ELMNT (hash_item)) {
				if (XLV_OREF_VOL_ELMNT(oref) ==
				    XLV_OREF_VOL_ELMNT(hash_item)) {
					free(hash_item);
					return;
				}
			}
			else {
				ASSERT (0);	/* cannot be -- see above */
			}

			oref = oref->next;
		}

		/*
		 * So there will more than one object with this name.
		 * In this case, we link the new object in.
		 */
		hash_item->next = Tcl_GetHashValue (curr_entry);
	}

	Tcl_SetHashValue (curr_entry, hash_item);

} /* end of xlv_mk_add_completed_obj() */


void 
xlv_mk_init (void)
{
	xlv_oref_t		*oref;
	xlv_oref_t		*oref_list;
	int			status;
	xlv_vh_entry_t		*vh_list_ptr;
	xlv_vh_disk_label_t	*label;
	xlv_vh_entry_t		*tmp_vh_list;
	boolean_t		old_label_converted = B_FALSE;

	Tcl_InitHashTable (&xlv_obj_table, TCL_STRING_KEYS);

	if (xlv_vh_list == NULL) {
		/*
		 * The disk label cache file does not exist.
		 * Try reading label from the disk volume header.
		 */
		tmp_vh_list = NULL;
		xlv_lab2_create_vh_info_from_hinv (&tmp_vh_list,NULL,&status);
		for (vh_list_ptr = tmp_vh_list; vh_list_ptr != NULL;
		     vh_list_ptr = vh_list_ptr->next) {
			label = XLV__LAB1_PRIMARY(vh_list_ptr);
			if (label->header.version == 1) {
				xlv_convert_old_labels(vh_list_ptr, 0);
				old_label_converted = B_TRUE;
			}
		}

		/*
		 * xlv_convert_old_labels modifies the vh_list. So if it
		 * ran, read in a new vh list. Otherwise, just reuse the
		 * existing one.
		 */
		if (old_label_converted) {
			xlv_lab2_destroy_vh_info(&xlv_vh_list, &status);
			xlv_vh_list = NULL;
			xlv_lab2_create_vh_info_from_hinv (&xlv_vh_list,
				NULL, &status);
		}
		else
			xlv_vh_list = tmp_vh_list;
	}

	/*
	 * Generate the oref list but do not filter out
	 * any incomplete volumes. We want everything.
	 */
	xlv_vh_list_to_oref (
		xlv_vh_list, &oref_list, NULL, NULL, 0, XLV_READ_ALL);

	/*
	 * Add all the existing, complete objects to the hash table.
	 */
	for (oref = oref_list; oref != NULL; oref = oref->next) {
		xlv_mk_add_completed_obj (oref,YES);
	}

}


	
