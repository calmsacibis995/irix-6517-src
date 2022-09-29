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
#ident "$Revision: 1.3 $"

#include <stdio.h>
#include <stdlib.h>
#include <bstring.h>
#include <locale.h>

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
#include <tcl.h>

xlv_vh_entry_t		*xlv_vh_list = NULL,
			*nxlv_vh_list = NULL;

xlv_tab_vol_t		*tab_vol = NULL;
xlv_tab_t		*tab_sv = NULL;

xlv_attr_cursor_t	xlv_cursor;
xlv_attr_cursor_t	*xlv_cursor_p = &xlv_cursor;

int                     Plex_Licensing =0;
static char             *rootname = NULL;
extern Tcl_HashTable	xlv_obj_table;

static boolean_t	initialized = B_FALSE;


/*
 * Back-end routine for xlv_mgr.  These routines do not
 * deal with any TCL user interface data structures and only 
 * return status codes.  This way, they can be used with other
 * front-ends.
 * Note that we do use the TCL supplied hash table routines.
 * But these can be used with other user interfaces also.
 */

static void
xlv_init (void)
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

	if (nxlv_vh_list) {
		xlv_lab2_free_vh_list(&nxlv_vh_list);
	}

	/*
	 * Read label from the disk volume header.
	 */
	xlv_lab2_create_vh_info_from_hinv(&xlv_vh_list, &nxlv_vh_list, &status);

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

} /* end of xlv_init() */

void
init_admin()
{
	if(! initialized) {
		setlocale(LC_ALL, "C");
		setcat("xlv_admin");
		Plex_Licensing = check_plexing_license(rootname);
		initialized = B_TRUE;
	}

	xlv_init();
}

boolean_t
isTableInitialized()
{
	return(initialized);
}
