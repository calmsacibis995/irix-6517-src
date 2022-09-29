
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * #ident "$Revision: 1.3 $"
 *
 * save_db - save text database file.
 */

#include <stdio.h>
#include "dbedit.h"

int
confirm(char *prompt, char *choices)
{
	char dotnew[256];	/* buffer */
	char *cp;

	/*
	 * Ask the user 
	 */
	for (printf("%s", prompt); fgets(dotnew, 256, stdin) != NULL;
	    printf("%s", prompt)) {
		for (cp = choices; *cp; cp++)
			if (*cp == dotnew[0])
				return (*cp);
	}
	return (0);
}
