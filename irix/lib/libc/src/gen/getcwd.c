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
	#pragma weak getcwd = _getcwd
#endif
#include 	"synonyms.h"
#include 	"shlib.h"
#include        <sys/types.h>
#include	"priv_extern.h"


char *
getcwd(char *str, size_t size)
{
#if _SGIAPI
	return __getcwd(str, size, GETCWD);
#else
	return NULL;
#endif
}
