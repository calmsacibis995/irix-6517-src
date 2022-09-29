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
#ident "$Revision: 1.2 $"

#include <stdlib.h>
#include <bstring.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <tcl.h>

extern boolean_t	isTableInitialized(void);
extern void		init_admin();

/*
 * We maintain a single table for all objects so that we can
 * search in one place should the user give us a name.
 */
Tcl_HashTable   xlv_obj_table;

/*
 * Add the given object to the object hash table.
 *
 * INPUT: This function does not consume the oref structure but the
 * referenced xlv object is consumed so the caller must not free the
 * actual object.
 */
void
add_object_to_table (xlv_oref_t *object)
{
	xlv_oref_t	*hash_item;	/* new object we want to add. */
	/*
	 * The hash key must be null-terminated.
	 * So allocate an extra byte for the NULL
	 * in case the top-level object name takes
	 * up XLV_NAME_LEN characters.
	 */
	char		hash_key [XLV_NAME_LEN+1];
	Tcl_HashTable	*hash_table;
	int		new_entry_created;
	Tcl_HashEntry	*curr_entry;	/* current entry hashed to 
					   same location. */

	if(! isTableInitialized()) {
		init_admin();
	}

	hash_item = (xlv_oref_t *) malloc (sizeof(xlv_oref_t));
	*hash_item = *object;

	if (XLV_OREF_VOL (object) != NULL) {
		strncpy (hash_key, XLV_OREF_VOL(object)->name, XLV_NAME_LEN);
	}
	else if (XLV_OREF_PLEX (object) != NULL) {
		strncpy (hash_key, XLV_OREF_PLEX(object)->name, XLV_NAME_LEN);
	}
	else if (XLV_OREF_VOL_ELMNT (object) != NULL) {
		strncpy (hash_key, XLV_OREF_VOL_ELMNT(object)->veu_name,
			 XLV_NAME_LEN);
	}
	else {
		ASSERT (0);	/* This object is not complete */
	}

	hash_table = &xlv_obj_table;
	hash_key[XLV_NAME_LEN] = '\0';

	if (XLV_OREF_SUBVOL (object) != NULL) {
		/*
		 * Set the subvolume depth here. This information is
		 * needed by disk-label-reading function to determine
		 * if a "unique" or "common" piece is missing when
		 * there are missing parts.
		 */
		XLV_OREF_SUBVOL(object)->subvol_depth =
			(short) xlv_tab_subvol_depth(XLV_OREF_SUBVOL(object));
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
		xlv_oref_t	*oref;
		int		found = 0;

		oref = (xlv_oref_t *)Tcl_GetHashValue(curr_entry);
		while (oref) {
			if (XLV_OREF_VOL (hash_item)) {
				found = (XLV_OREF_VOL(oref) ==
					 XLV_OREF_VOL(hash_item));
			}
			else if (XLV_OREF_PLEX (hash_item)) {
				found = (XLV_OREF_PLEX(oref) ==
					 XLV_OREF_PLEX(hash_item));
			}
			else if (XLV_OREF_VOL_ELMNT (hash_item)) {
				found = (XLV_OREF_VOL_ELMNT(oref) ==
					 XLV_OREF_VOL_ELMNT(hash_item));
			}
			else {
				ASSERT (0);	/* cannot be -- see above */
			}

			if (found) {
				/*
				 * Same object, so just copy new
				 * flag value.
				 */
				oref->flag = object->flag; 
				free(hash_item);
				return;
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

} /* end of add_object_to_table() */


/*
 * Find the object in the hash table with the given name.
 * If a specific type is given, search for the object of that type. 
 * If the type is XLV_OBJ_TYPE_NONE then any object with that
 * name is okay.
 *
 * OUTPUT: Upon success a pointer to an oref identifying the object
 * is returned. The caller is responsible for freeing this oref structure.
 */
xlv_oref_t *
find_object_in_table (char *name, int type)
{
	xlv_oref_t	*oref, *new;
	Tcl_HashEntry	*entry;
	char		*oref_name;
	int		oref_type;

	if (!name || name[0] == '/0')
		return (0);

	if(! isTableInitialized()) {
		init_admin();
	}

	entry = Tcl_FindHashEntry(&xlv_obj_table, name);

	if (!entry)
		return (0);

	oref = (xlv_oref_t *)Tcl_GetHashValue(entry);
	for ( ; oref; oref = oref->next) {
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
			ASSERT(0);
			continue;
		}

		if ((type != XLV_OBJ_TYPE_NONE) && (type != oref_type))
			continue;

		/*
		 * XXXjleong Should also check the nodename.
		 */
		if (!strcmp(oref_name, name)) {
			/*
			 * Found it. Make copy of the oref and pass
			 * the copy back to caller. Don't want to
			 * message around with the original.
			 */
			new = (xlv_oref_t *) malloc(sizeof(xlv_oref_t));
			XLV_OREF_COPY (oref, new);
			new->next = NULL;
			oref = new;
			break;
		}
	}

	return (oref);

} /* end of find_object_in_table() */


/*
 * Remove the XLV object from the hash table.
 *
 * Return 0 on success and 1 on failure.
 *
 * Note: The oref removed from the hash table is free only if it is
 * not the same oref as the passed in <object>
 */
int
remove_object_from_table (xlv_oref_t *object)
{
	Tcl_HashEntry	*hash_entry;
	xlv_oref_t 	*mark;
	xlv_oref_t 	*oref;
	char		*oref_name;
	int		oref_type;
	char		*name;
	int		type;

	if(! isTableInitialized()) {
		init_admin();
	}

	/*
	 * Could use object->obj_type ...
	 */
	if (XLV_OREF_VOL(object)) {
		name = XLV_OREF_VOL(object)->name;
		type = XLV_OBJ_TYPE_VOL;
	}
	else if (XLV_OREF_PLEX(object)) {
		name = XLV_OREF_PLEX(object)->name;
		type = XLV_OBJ_TYPE_PLEX;
	}
	else if (XLV_OREF_VOL_ELMNT(object)) {
		name = XLV_OREF_VOL_ELMNT(object)->veu_name;
		type = XLV_OBJ_TYPE_VOL_ELMNT;
	}
	else {
		return (1);
	}
		
	hash_entry = Tcl_FindHashEntry (&xlv_obj_table, name);
	if (hash_entry == NULL) {
		return (1);		/* Why isn't it in the table? */
	}

	oref = (xlv_oref_t *) Tcl_GetHashValue(hash_entry);
	for (mark = NULL; oref != NULL; mark = oref, oref = oref->next) {
		if (XLV_OREF_VOL(oref)) {
			oref_type = XLV_OBJ_TYPE_VOL;
			oref_name = XLV_OREF_VOL(oref)->name;
		}
		else if (XLV_OREF_PLEX(oref)) {
			oref_type = XLV_OBJ_TYPE_PLEX;
			oref_name = XLV_OREF_PLEX(oref)->name;
		}
		else if (XLV_OREF_VOL_ELMNT(oref)) {
			oref_type = XLV_OBJ_TYPE_VOL_ELMNT;
			oref_name = XLV_OREF_VOL_ELMNT(oref)->veu_name;
		}
		else { ASSERT(0); continue; }

		/*
		 * Compare value to see if this is the requested object.
		 * Don't really need to compare names cuz the hash
		 * function should have returned the correct entry
		 * but double check by comparing names.
		 */
		if (oref_type == type && !strcmp(oref_name, name))
			break;		/* found it */
	}

	if (oref == NULL) {
		/* Named object is not in the table. */
		return (1);
	} else if (mark == NULL) {
		/* Named object is the first value in the bucket. */
		if (oref->next == NULL) {
			/* single entry so just delete the entry */
			Tcl_DeleteHashEntry (hash_entry);
		} else {
			/* multiple entries so change the hash value */
			mark = oref->next;
			Tcl_SetHashValue (hash_entry, mark);
		}
	} else {
		/* name object is in the mist of a linked list. */
		mark->next = oref->next;
	}

	if (object == oref) {
		/*
		 * The oref plucked from the hash table is the passed in
		 * object itself don't free the data structure. The calling
		 * function may have further use of the data structure.
		 */
		object->next = NULL;
	} else {
		free (oref);
	}

	return(0);

} /* end of remove_object_from_table() */

