/*
 * mkdirp.c
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

#ident "$Revision: 1.5 $"


#ifdef __STDC__
	#pragma weak mkdirp = _mkdirp
#endif
#include "synonyms.h"

/* Creates directory and it's parents if the parents do not
** exist yet.
**
** Returns -1 if fails for reasons other than non-existing
** parents.
** Does NOT compress pathnames with . or .. in them.
**
** Does create directories even if parent exists after mkdirp
** creates them. Eg. mkdirp a/b/../b/c will create a/b/c
** it used to fail as .. and b would both exist during traversal
** although not at the start.
*/
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>

static char *compress(const char *);

int
mkdirp(const char *d, mode_t mode)
{
	char  *endptr, *ptr, *slash, *str, *savestr;

	str=compress(d);

	/* If space couldn't be allocated for the compressed names, return. */

	if ( str == NULL )
		return(-1);
	ptr = str;


	/* Try to make the directory */

	if ((mkdir(str, mode) == 0) || (errno == EEXIST)) {
		free(str);
		return(0);
	}
	if (errno != ENOENT) {
		free(str);
		return(-1);
	}
	endptr = strrchr(str, '\0');
	ptr = endptr;
	slash = strrchr(str, '/');

	/* Search upward for the non-existing parent */
	/* The mode of each directory created is initially set */
	/* to 0777 so the subdirectories can be created - the */
	/* pathname of the parent is saved in savestr and then */
	/* the mode is changed to what the caller requested */
	/* after the subdirectory is created */
	savestr = (char*)malloc(strlen(str) + 1);
	if (savestr == NULL) {
		free(str);
		return (-1);
	}
	*savestr = '\0';

	while (slash != NULL) {

		ptr = slash;
		*ptr = '\0';

		/* If reached an existing parent, break */

		if (access(str, 00) ==0)
			break;

		/* If non-existing parent*/

		else {
			slash = strrchr(str,'/');

			/* If under / or current directory, make it. */

			if (slash == NULL || slash == str) {
				if (mkdir(str, 0777)) {
					free(str);
					free(savestr);
					return(-1);
				}
				strcpy(savestr, str);
				break;
			}
		}
	}
	/* Create directories starting from upmost non-existing parent*/

	while ((ptr = strchr(str, '\0')) != endptr) {
		*ptr = '/';
		if (mkdir(str, 0777) && (errno != EEXIST)) {
			if ((*savestr != '\0') && (mode != 0777)) {
				chmod(savestr, mode);
			}
			free(str);
			free(savestr);
			return(-1);
		}
		if ((*savestr != '\0') && (mode != 0777)) {
			chmod(savestr, mode);
		}
		strcpy(savestr, str);
	}
	if ((*savestr != '\0') && (mode != 0777)) {
		chmod(savestr, mode);
	}
	free(str);
	free(savestr);
	return(0);
}

static char *
compress(const char *str)
{

	char *tmp;
	char *front;

	tmp=(char *)malloc(strlen(str)+1);
	if ( tmp == NULL )
		return(NULL);
	front = tmp;
	while ( *str != '\0' ) {
		if ( *str == '/' ) {
			*tmp++ = *str++;
			while ( *str == '/' )
				str++;
			if ( *str == '\0' )
				break;
		}
		*tmp++ = *str++;
	}
	*tmp = '\0';
	return(front);
}
