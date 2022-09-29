/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1995, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <bstring.h>
#include <ctype.h>
#include <stdio.h>
#include <pfmt.h>
#include "xlv_mgr.h"

/*
 * This function prompts the user for his wishes
 * and reads in his response.
 *
 * Note that blank lines and comments (lines with leading "#")
 * are ignored.
 */
int
get_val(char *buffer, int choice)
{
	char *prompt;
	char buf[500], *ch;

	/*
	 * Ask for the user's desire..
	 */
	switch (choice) {
		case VAL_COMMAND:			/* choice */
			pfmt(stderr, MM_NOSTD, 
	"::Please select choice...\n");
			prompt = "xlv_mgr> ";
			break;

		case VAL_OBJECT_NAME:
			pfmt(stdout, MM_NOSTD, 
	"::Please enter name of object to be operated on.\n");
			prompt = "  object name> ";
			break;
 		case VAL_SUBVOL_NUMBER:
			pfmt(stdout, MM_NOSTD, 
	"::Please select subvol type... log(1) data(2) rt(3).\n");
			prompt = "  subvol(1/2/3)> ";
                        break;

		case VAL_NEW_OBJECT_NAME:
			pfmt(stdout, MM_NOSTD, 
	"::Please enter name of new object.\n");
			prompt = "  object name> ";
			break;

		case VAL_PLEX_NUMBER:
			pfmt(stdout, MM_NOSTD, 
	"::Please select plex number (0-3).\n");
			prompt = "  plex(0-3)> ";
			break;

		case VAL_CREATED_OBJECT_NAME:
			pfmt(stdout, MM_NOSTD, 
	"::Please enter name of created object.\n");
			prompt = "  object name> ";
			break;

		case VAL_PART_NAME:
			pfmt(stdout, MM_NOSTD, 
	"::Please enter name of partition you wish to use.\n");
			prompt = "partition name> ";
			break;

		case VAL_VE_NUMBER:
			pfmt(stdout, MM_NOSTD, 
	"::Please enter ve number.\n");
			prompt = "  ve number> ";
			break;

		case VAL_ADD_OBJECT:
			pfmt(stdout, MM_NOSTD, 
	"::Please enter the object you wish to add to the target.\n");
			prompt = "  object name> ";
			break;

		case VAL_DESTROY_DATA:
			if (Tclflag == 1) {
				prompt = NULL;
				break;
			}
			pfmt(stdout, MM_NOSTD,
	"::Do you want to forcibly destroy this data. (yes/no)\n");
			prompt = "  confirm(yes/no)> ";
			break;

		case VAL_DELETE_LABELS:
			if (Tclflag == 1) {
				prompt = NULL;
				break;
			}
			pfmt(stdout, MM_NOSTD,
	"::Deleting the disk labels will cause all volumes to be lost!!\n"
	"Are you sure you want to do this? (yes/no)\n");
			prompt = "  confirm(yes/no)> ";
			break;

		case VAL_NODENAME:
			pfmt(stdout, MM_NOSTD,
	"::Please enter the node name.\n");
			prompt = "  node name> ";
			break;
	}

	/*
	 * Don't need confirmation when running a script.
	 */
	if (Tclflag == 1 && prompt == NULL) {
		strcpy(buffer, "yes");
		return(0);
	}

	/*
	 * Get the user's response.
	 * Ingore white space, blank lines and comments.
	 */
	do {
		printf(prompt);
		buf[0] = '\0';
		gets(buf);		/* XXX should check buf's size */

		/* skip all leading white space */
		for (ch = buf; *ch != '\0'; ch++) {
			if ( ! isspace((unsigned) *ch) )
				break;		/* found a real character */
		}

	} while (*ch == '\0' || *ch == '#' );

	strcpy(buffer, ch);

	return(0);

} /* end of get_val() */

