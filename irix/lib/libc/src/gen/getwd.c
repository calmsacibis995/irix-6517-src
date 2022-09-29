/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/getcwd.c	1.24.1.1"


/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak getwd = _getwd
#endif
#include 	"synonyms.h"
#include 	"shlib.h"
#include        <sys/types.h>
#include	"priv_extern.h"

#define MAX_PATH 1024

char *
getwd(char *pathname)
{
	char	buf[MAX_PATH];
	char	*resp;

	resp = __getcwd(buf, MAX_PATH, GETWD);
	/*
	 * On success it will return a pointer to the pathname
	 * which will be in our 'buf' but not at offset 0.
	 * On failure it will copy an error message into buf
	 * and return NULL.
	 */
	if (resp != NULL) {
		strcpy(pathname, resp);
		resp = pathname;
	} else {
		strcpy(pathname, buf);
	}
	return resp;
}
