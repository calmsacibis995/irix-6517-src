/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"$Revision: 1.2 $"

#include <stdio.h>
#include <string.h>

extern char *root, *basedir;

void
cvtpath(char *path, char *copy)
{
	*copy++ = '/';
	if(root || (basedir && (*path != '/'))) {
		if(root && ((basedir == NULL) || (path[0] == '/') ||
		  (basedir[0] != '/'))) {
			/* look in root */
			(void) strcpy(copy, root + (*root == '/'));
			copy += strlen(copy);
			if(copy[-1] != '/')
				*copy++ = '/';
		}
		if(basedir && (*path != '/')) {
			(void) strcpy(copy, basedir + (*basedir == '/'));
			copy += strlen(copy);
			if(copy[-1] != '/')
				*copy++ = '/';
		}
	}
	(void) strcpy(copy, path + (*path == '/'));
}
