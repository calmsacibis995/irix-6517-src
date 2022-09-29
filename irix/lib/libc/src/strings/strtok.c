/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/strtok.c	1.6"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#ifdef __STDC__
#ifndef  _LIBC_ABI
	#pragma weak strtok_r = _strtok_r
#endif /* _LIBC_ABI */
#endif
#include "synonyms.h"
#include <string.h>
#include <stddef.h>

/*
 * uses strpbrk and strspn to break string into tokens on
 * sequentially subsequent calls.  returns NULL when no
 * non-separator characters remain.
 * `subsequent' calls are calls with first argument NULL.
 */
/* move out of function scope so we get a global symbol for use with data cording */
static char	*savept = NULL;

char *
strtok(char *string, const char *sepset)
{
	register char	*q, *r;

	/*first or subsequent call*/
	if (string == NULL)
		string = savept;

	if(string == 0)		/* return if no tokens remaining */
		return(NULL);

	q = string + strspn(string, sepset);	/* skip leading separators */

	if(*q == '\0') {		/* return if no tokens remaining */
		savept = NULL;
		return(NULL);
	}

	if((r = strpbrk(q, sepset)) == NULL)	/* move past token */
		savept = 0;	/* indicate this is last token */
	else {
		*r = '\0';
		savept = r+1;
	}
	return(q);
}

char *
strtok_r(char *string, const char *sepset, char **lasts)
{
	register char	*q, *r;

	/*first or subsequent call*/
	if (string == NULL)
		string = *lasts;

	if(string == 0)		/* return if no tokens remaining */
		return(NULL);

	q = string + strspn(string, sepset);	/* skip leading separators */

	if(*q == '\0') {		/* return if no tokens remaining */
		*lasts = 0;	/* indicate this is last token */
		return(NULL);
	}

	if((r = strpbrk(q, sepset)) == NULL)	/* move past token */
		*lasts = 0;	/* indicate this is last token */
	else {
		*r = '\0';
		*lasts = r+1;
	}
	return(q);
}
