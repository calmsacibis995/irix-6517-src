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
 * #ident "$Revision: 1.4 $"
 *
 * dbedit database descriptions
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include "dbedit.h"

/*
 * Look for a database name in /etc/config/dbedit.conf
 *
 */

char *
find_database(char *requested)
{
	static char line[MAXPATHLEN];
	char *path;
	char *name;
	char *found;
	FILE *fp;
	int req_size = strlen(requested);

	if (fp = fopen(DBEDIT_CONF, "r")) {
		/*
		 * Look for the database in the configuration file.
		 */
		while (fgets(line, MAXPATHLEN-1, fp)) {
			if (strlen(line) <= req_size)
				continue;
			for (path = line; isspace(*path); path++)
				;
			/*
			 * Check for a path name having been requested.
			 */
			if (strncmp(requested, path, req_size) == 0 &&
			    isspace(path[req_size]))
				return requested;
			for (name = path; *name && !isspace(*name); name++)
				;
			for (*name++ = '\0'; isspace(*name); name++)
				;

			for (found = strstr(name, requested); found;
			    found = strstr(found+1, requested)) {
				if (isspace(found[-1]))
					return path;
			}
		}
	}
	fclose(fp);
	/*
	 * Didn't find it.
	 */
	return requested;
}
