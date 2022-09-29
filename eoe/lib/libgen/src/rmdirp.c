/*
 * rmdirp.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.3 $"


#ifdef __STDC__
	#pragma weak rmdirp = _rmdirp
#endif
#include "synonyms.h"

/* Rmdirp() removes directories in path "d". Removal starts from the
** right most directory in the path and goes backward as far as possible.
** The remaining path, which is not removed for some reason, is stored in "d1".
** If nothing remains, "d1" is empty.

** Rmdirp()
** returns 0 only if it succeeds in removing every directory in d.
** returns -1 if removal stops because of errors other than the following.
** returns -2 if removal stops when "." or ".." is encountered in path.
** returns -3 if removal stops because it's the current directory.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

static int dotdot(char *);

int
rmdirp(char *d, char *d1)
{
	struct stat st, cst;
	int currstat;
	char *slash;
	
	slash = strrchr(d, '/');
	currstat = stat(".", &cst);

				    /* Starts from right most element */

	while (d) {			

					/* If it's under current directory */

		if (slash == NULL) {

						/* Stop if it's . or .. */

			if (dotdot(d)) {
				(void)strcpy(d1, d);
				return(-2);
			}

						/* Stop if can not stat it */

		}

					/* If there's a slash before it */

		else {

						/* If extra / in the end */

			if (slash != d) 
				if (++slash == strrchr(d,'\0')) {
					*(--slash) = '\0';
					slash = strrchr(d, '/');
					continue;
				}
				else
					slash--;

						/* Stop if it's . or .. */

			if (dotdot(++slash)) {
				(void)strcpy(d1, d);
				return(-2);
			}
			slash--;

						/* Stop if can not stat it */

			if (stat(d, &st) < 0) {
				(void)strcpy(d1, d);
				return(-1);
			}
			if (currstat == 0) {	

						/* Stop if it's current directory */

				if(st.st_ino==cst.st_ino && st.st_dev==cst.st_dev) {
					(void)strcpy(d1, d);
					return(-3);
				}
			}
		} /* End of else */


					/* Remove it */

		if (rmdir(d) != 0) {
			(void)strcpy(d1, d);
			return(-1);
		}

					/* Have reached left end, break*/

		if (slash == NULL || slash == d)
			break;

					/* Go backward to next directory */

		*slash = '\0';
		slash = strrchr(d, '/');
	}
	*d1 = '\0';
	return(0);
}


static int
dotdot(char *dir)
{
	if (strcmp(dir, ".") == 0 || strcmp(dir, "..") == 0)
		return(-1);
	return(0);
}
