
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
#ident "$Revision: 1.3 $"


#include <stdlib.h>
#include <string.h>
#include <bstring.h>

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <xlv_error.h>
#include "xlv_make_int.h"
#include <sys/debug.h>
#include <sys/xlv_attr.h>
#include "xlv_admin.h"

#include <tcl.h>


extern Tcl_HashTable   xlv_obj_table;


/*
 *	For a given Volume name return the volume information
 *
 *	TODO:  this is currently only comparing against volume names.
 *		we need to this for all items (plexes, ves, & partitions).
 */

xlv_oref_t *
xlv_find_gen(char *name)
{
	Tcl_HashEntry	*hash_entry;
	Tcl_HashSearch	search_ptr;
	xlv_oref_t 	* oref;

	if (*name == '\0') {
		return (NULL);
	}
		
	/*
	 * Find requested object.
	 */
	hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

	while (hash_entry != NULL) {
		oref = (xlv_oref_t *)  Tcl_GetHashValue (hash_entry);
		ASSERT (oref != NULL);
		do {

			/* 
			 *	Compare value to see if this is the requested 
			 *	volume.
			 */

			switch (oref->obj_type) {
				case XLV_OBJ_TYPE_VOL:
					if (strcmp (XLV_OREF_VOL(oref)->name, name) == 0) 
					{
						return(oref);
					}
					break;
				
				case XLV_OBJ_TYPE_PLEX:
					if (strcmp (XLV_OREF_PLEX(oref)->name, 
						name) == 0) 
					{
						return(oref);
					}
					break;

				case XLV_OBJ_TYPE_VOL_ELMNT:
					if (strcmp (XLV_OREF_VOL_ELMNT
						(oref)->veu_name, name) == 0) 
					{
						return(oref);
					}
					break;
			}
			/*
			 * When there are multiple objects with the
			 * same name -- name is the hash key -- the
			 * "next" field points to the next object.
			 * At most, only one object of each type
			 * (vol, subvol, plex, ve) should be on this
			 * linked list.
			 */
			oref = oref->next; /* ?? what is next on this?? */
		} while (oref != NULL);

		hash_entry = Tcl_NextHashEntry (&search_ptr);
	}
	return(NULL);

} /* end of xlv_find_gen() */


xlv_oref_t *
xlv_find_vol(char *name)
{
	Tcl_HashEntry	*hash_entry;
	xlv_oref_t 	*oref;

	if (*name == '\0') {
		return (NULL);
	}
		
	hash_entry = Tcl_FindHashEntry(&xlv_obj_table,name);

	if (hash_entry == NULL){
		return (NULL);
	}

	oref = (xlv_oref_t *)  Tcl_GetHashValue (hash_entry);
	if (oref == NULL){
		return (NULL);
	}

	if( oref->obj_type == XLV_OBJ_TYPE_VOL) {
		return (oref);
	}

	else {		/* see if it is in the list */
		if (oref->next == NULL) {
			return (NULL);
		}
		do {
			if (oref->next != NULL) {
				oref = oref->next;
				if ( oref->obj_type == XLV_OBJ_TYPE_VOL) {
					return (oref);
				}
			}
		} while (oref != NULL);
	}
	return (NULL);
}


xlv_oref_t *
xlv_find_plex(char *name)
{
	Tcl_HashEntry	*hash_entry;
	xlv_oref_t 	*oref;

	if (*name == '\0') {
		return (NULL);
	}
		
	hash_entry = Tcl_FindHashEntry(&xlv_obj_table,name);
	if (hash_entry == NULL) {
		return (NULL);
	}
	oref = (xlv_oref_t *) Tcl_GetHashValue (hash_entry);
	if (oref == NULL) {
		return (NULL);
	}

	if (oref->obj_type == XLV_OBJ_TYPE_PLEX) {
#ifdef DEBUG
		printf("DBG: xlv_find_plex(%s)\n", XLV_OREF_PLEX(oref)->name);
#endif 
		return (oref);
	}
	else {		/* see if it is in the list */
		if (oref->next == NULL) {
			return (NULL);
		}
		do {
			if (oref->next != NULL) {
#ifdef DEBUG
				if (oref->obj_type == XLV_OBJ_TYPE_PLEX)
					printf("DBG: searching plex %s\n",
						XLV_OREF_PLEX(oref)->name);
				if (oref->obj_type == XLV_OBJ_TYPE_VOL)
					printf("DBG: searching vol %s\n",
						XLV_OREF_VOL(oref)->name);
#endif
				oref = oref->next;
				if ( oref->obj_type == XLV_OBJ_TYPE_PLEX) {
					return (oref);
				}
			}
		} while (oref != NULL);
	}
	return (NULL);
}


xlv_oref_t *
xlv_find_ve(char *name)
{
	Tcl_HashEntry	*hash_entry;
	xlv_oref_t 	*oref;

	if (*name == '\0') {
		return (NULL);
	}
		
	hash_entry = Tcl_FindHashEntry(&xlv_obj_table,name);
	if (hash_entry == NULL){
		return (NULL);
	}
	oref = (xlv_oref_t *) Tcl_GetHashValue (hash_entry);
	if (oref == NULL) {
		return (NULL);
	}

	if( oref->obj_type == XLV_OBJ_TYPE_VOL_ELMNT) {
#ifdef DEBUG
		printf("DBG: xlv_find_ve(%s)\n",
			XLV_OREF_VOL_ELMNT(oref)->veu_name);
#endif 
		return (oref);
	}

	else {		/* see if it is in the list */
		if (oref->next == NULL) {
			return (NULL);
		}
		do {
#ifdef DEBUG
			printf("TAC found... \n");
#endif
			if (oref->next != NULL) {
				oref = oref->next;
				if (oref->obj_type == XLV_OBJ_TYPE_VOL_ELMNT) {
#ifdef DEBUG
					printf("DBG: xlv_find_ve() found match\n");
#endif
					return (oref);
				}
			}
		} while (oref != NULL);
	}
	return (NULL);
}



xlv_oref_t *
xlv_find_prev(char *name)
{
	Tcl_HashEntry	*hash_entry;
	Tcl_HashSearch	search_ptr;
	xlv_oref_t 	*oref;
	xlv_oref_t 	*prev = NULL;


#if DEBUG
	printf("DBG: xlv_find_prev(%s)\n", name);
#endif

	if (*name == '\0') {
		return (NULL);
	}
		
	/*
	 * Find requested object.
	 */

	hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

	while (hash_entry != NULL) {
		oref = (xlv_oref_t *)  Tcl_GetHashValue (hash_entry);
		ASSERT (oref != NULL);
		do {

			/* 
			 *	Compare value to see if this is the requested 
			 *	volume.
			 */

			if (oref->vol != NULL)
			if (strcmp (XLV_OREF_VOL(oref)->name, name) == 0) 
			{
				
				/*
				 *	Found desired volume.
				 */
#ifdef DEBUG
				printf("DBG: ***** found %s ******** \n",name);
				xlv_oref_print (oref, XLV_PRINT_ALL);
#endif
				return(prev);
			}
			prev = oref;
			oref = oref->next; /* ?? what is next on this?? */
		} while (oref != NULL);

		hash_entry = Tcl_NextHashEntry (&search_ptr);
	}

	return(NULL);

}



int 
xlv_del_hashentry(char *name)
{
	Tcl_HashEntry	*hash_entry;
	Tcl_HashSearch	search_ptr;
	xlv_oref_t 	*oref;


#if DEBUG
	printf("DBG: xlv_del_hashentry(%s) \n", name);
#endif

	if (*name == '\0') {
		return (NULL);
	}
		
	/*
	 * Find requested object.
	 */
	hash_entry = Tcl_FindHashEntry (&xlv_obj_table, name);

	while (hash_entry != NULL) {
		oref = (xlv_oref_t *)  Tcl_GetHashValue (hash_entry);
		ASSERT (oref != NULL);
		do {

			/* 
			 *	Compare value to see if this is the requested 
			 *	volume.
			 */

			switch (oref->obj_type) {
				case XLV_OBJ_TYPE_VOL:
					if (strcmp (XLV_OREF_VOL(oref)->name, 
						name) == 0) {
					if (oref->next == NULL) {
#ifdef DEBUG
			printf("DBG: xlv_del_hashentry() oref->next NULL\n");
#endif
					}
					Tcl_DeleteHashEntry (hash_entry);
					return(TCL_OK);
				}
				break;

				case XLV_OBJ_TYPE_PLEX:
					if (strcmp (XLV_OREF_PLEX(oref)->name, 
						name) == 0) {
						if (oref->next == NULL) {
#ifdef DEBUG
			printf("DBG: xlv_del_hashentry() oref->next NULL\n");
#endif
						}
					Tcl_DeleteHashEntry (hash_entry);
					return(TCL_OK);
				}
				break;

				case XLV_OBJ_TYPE_VOL_ELMNT:
					if (strcmp (XLV_OREF_VOL_ELMNT
						(oref)->veu_name, name) == 0) {
						if (oref->next == NULL) {
#ifdef DEBUG
			printf("DBG: xlv_del_hashentry() oref->next NULL\n");
#endif
						}
					Tcl_DeleteHashEntry (hash_entry);
					return(TCL_OK);
					}
					break;
			}
			oref = oref->next; /* ?? what is next on this?? */
		} while (oref != NULL);

		hash_entry = Tcl_NextHashEntry (&search_ptr);
	}

	return(0);

}
