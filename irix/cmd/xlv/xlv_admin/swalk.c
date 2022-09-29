
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
#ident "$Revision: 1.20 $"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/syssgi.h>
#include <unistd.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include "xlv_make_int.h"
#include "xlv_admin.h"
#include <locale.h>
#include <tcl.h>

extern Tcl_HashTable    xlv_obj_table;
xlv_attr_cursor_t	cursor;
int			Plex_Licensing =0;
int			expert_mode =0;
int			verbose_mode =0;

static char		*rootname = NULL;
static int		display_menu;


/*
 *	This function walks the structures of the volume.
 *	It is used as a testing routine.
 */

int
main (int argc, char *argv[])
{
	Tcl_HashEntry	*hash_entry;
	Tcl_HashSearch	search_ptr;
	xlv_oref_t	*oref;
	char 		name[33];
	int		ch;
	char		buf[5];
	int		choice;
	int		retval;

	/*
	 *	Set up international items
	 */
	setlocale(LC_ALL, "C");
	setcat("xlv_admin");

	/*
	 *	Verify privilege
	 */
	if (getuid()) {
		pfmt(stderr, MM_HALT, 
		     ":: %s must be started by super-user\n", argv[0]);
		exit(1);
        }

	/*
	 *	See if a value was set for retrieving the license
	 */
	while ((ch = getopt(argc, argv, "r:xv")) != EOF) {
		switch((char)ch) {
		case 'r':
			rootname = optarg;	/* root directory */
			if (!xlv_setrootname(rootname)) {
				char msg[500];
				sprintf (msg,
			"xlv_admin cannot set \"%s\" as root dir",rootname);
				perror(msg);
				exit(1);
			}
			break;
		case 'x':
			expert_mode = 1;
			break;
		case 'v':
			verbose_mode = 1;
			break;
		default:
			pfmt(stderr, MM_NOSTD,
			     ":: usage: xlv_admin [-r root] [-v]\n");
		}
	}

	xlv_mk_init ();

	hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

	while (hash_entry != NULL) {
		oref = (xlv_oref_t *) Tcl_GetHashValue (hash_entry);
		ASSERT (oref != NULL);
		do {
			oref = oref->next;
		} while (oref != NULL);

		hash_entry = Tcl_NextHashEntry (&search_ptr);
	}

	if (0> syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		pfmt(stderr, MM_HALT, 
			":57: Failed to get cursor, cursor ensures consistency.\n");
	}


	/*
	 * Check to see if we are licensed for plexing. If not, disable
	 * all plexed volumes.
	 */
	Plex_Licensing = check_plexing_license (rootname);


	/*
	 *	forever provide the work menu.
	 */
	buf[0] = '\0';
	display_menu = 1;	/* alway display menu at least once */
	for(;;) {

		if (display_menu) {
			do_begin();
			display_menu = 0;
		}

		get_val(buf, VAL_COMMAND); /* ignore comments & blank lines */

		if (buf[0] == '?') {
			display_menu = 1;	/* display menu again */
			continue;
		}

		if ((0 == strcasecmp(buf, "quit")) ||
		    (0 == strcasecmp(buf, "exit"))) {
			exit(0);
		}
		
		choice = atoi(buf);

		if (choice == 99) {
			exit (0);
		}
		
		if (choice == 0) {
#ifdef DEBUG
			int i, len;
			len = strlen(buf);
			printf("DBG: read choice %d:", choice);
			for (i=0; i < len; i++)
				printf(" %2x\"%c\"", buf[i], buf[i]);
			printf("\n");
#endif
			display_menu = 1;	/* display menu again */
			continue;
		}

		if ((choice < 32 ) ||
		    (choice == 42) ||
		    (choice == 46) ||
		    (choice == 51)) {
			/*
			 * Command requires the name of the object
			 * to which the command applies.
			 */
			get_val (name, VAL_OBJECT_NAME);
		}

		retval = do_choice(choice, name);
		if (verbose_mode) {
			printf ("Command #%d %s\n", choice,
				(retval) ? "failed" : "succeeded");
		}
	}

	/* shouldn't be exiting here */
	return(1);
}

