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
#ident "$Revision: 1.7 $"

#include <stdlib.h>
#include <bstring.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>

#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>

#include "xlv_mgr.h"

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
	 * The hash key must be null-terminated, so allocate an extra
	 * byte for the NULL in case the top-level object name takes
	 * up XLV_NAME_LEN characters.
	 */
	char		hash_key[XLV_NAME_LEN+1];	/* object name */
	Tcl_HashTable	*hash_table;
	int		new_entry_created;
	Tcl_HashEntry	*curr_entry;	/* current entry hashed to 
					   same location. */
	int		hash_type;

	hash_item = (xlv_oref_t *) malloc (sizeof(xlv_oref_t));
	*hash_item = *object;

	/*
	 * Set the hash key to the name of the object.
	 */
	if (XLV_OREF_VOL(object) != NULL) {
		strncpy(hash_key, XLV_OREF_VOL(object)->name, XLV_NAME_LEN);
		hash_type = XLV_OBJ_TYPE_VOL;
	}
	else if (XLV_OREF_PLEX(object) != NULL) {
		strncpy(hash_key, XLV_OREF_PLEX(object)->name, XLV_NAME_LEN);
		hash_type = XLV_OBJ_TYPE_PLEX;
	}
	else if (XLV_OREF_VOL_ELMNT(object) != NULL) {
		strncpy(hash_key, XLV_OREF_VOL_ELMNT(object)->veu_name,
			XLV_NAME_LEN);
		hash_type = XLV_OBJ_TYPE_VOL_ELMNT;
	}
	else { 
		hash_type = hash_type;
		ASSERT(0); /* This object is not complete */
	}

	hash_table = &xlv_obj_table;
	hash_key[XLV_NAME_LEN] = '\0';

	if (XLV_OREF_SUBVOL(object) != NULL) {
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
		 * An entry already exsist in the hash table for this key.
		 * See if it is the same object.  Need to check the object
		 * name because different names can hash to the same bucket.
		 * Also check the associated nodename name and object type. 
		 */
		xlv_oref_t	*oref;
		int		found = 0;
		char		*name;

#ifdef DEBUG
fprintf (stderr, "DBG: Hash table has entry for key='%s'\n", hash_key);
#endif

		oref = (xlv_oref_t *)Tcl_GetHashValue(curr_entry);
		while (oref != NULL && !found) {
			/*
			 * Check both nodename and object type.
			 * Should not need to check the uuid.
			 */
			if (hash_item->nodename[0] != '\0' &&
			    oref->nodename[0] != '\0' &&
			    strncmp(hash_item->nodename,
				    oref->nodename, XLV_NODENAME_LEN) != 0) {
				oref = oref->next;
				continue;
			}
			if (XLV_OREF_VOL(hash_item)) {
				found = (XLV_OREF_VOL(oref) ==
					 XLV_OREF_VOL(hash_item));
				name = XLV_OREF_VOL(hash_item)->name;
			}
			else if (XLV_OREF_PLEX(hash_item)) {
				found = (XLV_OREF_PLEX(oref) ==
					 XLV_OREF_PLEX(hash_item));
				name = XLV_OREF_PLEX(hash_item)->name;
			}
			else if (XLV_OREF_VOL_ELMNT(hash_item)) {
				found = (XLV_OREF_VOL_ELMNT(oref) ==
					 XLV_OREF_VOL_ELMNT(hash_item));
				name = XLV_OREF_VOL_ELMNT(hash_item)->veu_name;
			}
			else {
				ASSERT (0);	/* cannot be -- see above */
			}

			if (!found) {
				/* data pointers are different but what
				 * about the object name.
				 *
				 * Note that the uuid is not compared.
				 * This implies that when two objects have
				 * the same nodename and objectname only
				 * first one in the hash table wins.
				 */
				if (!strncmp(hash_key, name, XLV_NAME_LEN)) {
					found = 1;
					fprintf(stderr,
				"Ignore node=%s object=%s: already exists\n",
						oref->nodename, name);
				}
			}

			if (!found)
				oref = oref->next;

		} /* while-loop searching hash collisions */

		if (found) {
			free(hash_item);
			return;
		}

		/*
		 * So there will more than one object with this key
		 * and possibly with the same object name. Just link
		 * the new object into the list.
		 */
#ifdef DEBUG
fprintf (stderr, "DBG: Link collision on key '%s'\n", hash_key);
#endif
		hash_item->next = Tcl_GetHashValue(curr_entry);
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
find_object_in_table (char *name, int type, char *nodename)
{
	xlv_oref_t	*oref, *new;
	Tcl_HashEntry	*entry;
	char		*oref_name;
	int		oref_type;
	char		*owner;

	extern xlv_vh_entry_t *xlv_vh_list;

	if (!name || name[0] == '/0')
		return (0);

	if (xlv_vh_list == NULL)	/* no hash table */
		return (0);

	entry = Tcl_FindHashEntry(&xlv_obj_table, name);

	if (!entry)
		return (0);

	oref = (xlv_oref_t *)Tcl_GetHashValue(entry);
	for ( ; oref; oref = oref->next) {
		if (XLV_OREF_VOL(oref)) {
			oref_name = XLV_OREF_VOL(oref)->name;
			oref_type = XLV_OBJ_TYPE_VOL;
			/* owner = oref->nodename; */
			owner = XLV_OREF_VOL(oref)->nodename;

		} else if (XLV_OREF_PLEX(oref)) {
			oref_name = XLV_OREF_PLEX(oref)->name;
			oref_type = XLV_OBJ_TYPE_PLEX;
			owner = oref->nodename;

		} else if (XLV_OREF_VOL_ELMNT(oref)) {
			oref_name = XLV_OREF_VOL_ELMNT(oref)->veu_name;
			oref_type = XLV_OBJ_TYPE_VOL_ELMNT;
			owner = oref->nodename;

		} else {
			ASSERT(0);
			continue;
		}

		if ((type != XLV_OBJ_TYPE_NONE) && (type != oref_type)) {
			continue;
		}

		if (nodename && owner[0] != '\0'
		    && (strncmp(nodename, owner, XLV_NODENAME_LEN) != 0))  {
			continue;
		}
		if (!strcmp(oref_name, name)) {
			/*
			 * Found it. Make copy of the oref and pass
			 * the copy back to caller. Don't want to
			 * message around with the original.
			 */
			new = (xlv_oref_t *) malloc(sizeof(xlv_oref_t));
			XLV_OREF_COPY (oref, new);
			new->next = NULL;
			oref = new;	/* XXXjleong Want the original? */
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
remove_object_from_table (xlv_oref_t *object, int logerror)
{
	Tcl_HashEntry	*hash_entry;
	xlv_oref_t 	*mark;
	xlv_oref_t 	*oref;
	int		oref_type;
	char		*oref_name;		/* oref object name */
	char		*oref_nname = NULL;	/* oref nodename */
	char		*nname = NULL;		/* nodename */
	char		*name;			/* object name */
	int		type;

	/*
	 * Could use object->obj_type ...
	 */
	if (XLV_OREF_VOL(object)) {
		name = XLV_OREF_VOL(object)->name;
		type = XLV_OBJ_TYPE_VOL;
		nname = XLV_OREF_VOL(object)->nodename;
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
		if (logerror) {
			 fprintf(stderr,
	"Internal error: remove_object_from_table(): unknown object.\n");
		}
		return (1);
	}
		
	hash_entry = Tcl_FindHashEntry (&xlv_obj_table, name);
	if (hash_entry == NULL) {
		if (logerror) {
			 fprintf(stderr,
	"Internal error: remove_object_from_table(): no hash entry for %s.\n",
				name);
		}
		return (1);		/* Why isn't it in the table? */
	}

	oref = (xlv_oref_t *) Tcl_GetHashValue(hash_entry);
	for (mark = NULL; oref != NULL; mark = oref, oref = oref->next) {
		if (XLV_OREF_VOL(oref)) {
			oref_type = XLV_OBJ_TYPE_VOL;
			oref_name = XLV_OREF_VOL(oref)->name;
			oref_nname = XLV_OREF_VOL(oref)->nodename;
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
		 * XXX If nodename in oref is set correctly:
		 *
		 * if (strcmp(object->nodename, oref->nodename) != 0)
		 *	continue;
		 */

		/*
		 * Compare value to see if this is the requested object.
		 * Need to compare object names cuz the different names
		 * can hash to the same bucket.
		 *
		 * Also check the associated nodename for a volume. 
		 */
		if ((oref_type == type) && !strcmp(oref_name, name)) {
			if (nname && (strcmp(nname, oref_nname) != 0)) {
				continue;
			}
			break;		/* found it */
		}
	} /* for-loop thru collision chain */

	if (oref == NULL) {
		/* Named object is not in the table. */
		if (logerror) {
			 fprintf(stderr,
	"Internal error: remove_object_from_table(): no entry for %s.\n",
				name);
		}
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

