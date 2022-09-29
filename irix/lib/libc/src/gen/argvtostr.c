/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/argvtostr.c	1.4"
#ifdef	__STDC__
	#pragma weak	argvtostr = _argvtostr
#endif

#include "synonyms.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
 * argvtostr:    Given a pointer to an array of character
 *	strings, change it into a command line type string.
 *	Remember to escape special characters.
 *
 */

#define BUNCH 25
static const char delim[] = { " \t'\"" };	/* delimiters */

char *
argvtostr(char **argvp)
{
    char **temp;
    char escape;
    char *cmdp, *strp;
    char *cmdstr;
    size_t length = 0;
	
    if ((temp = argvp) == NULL )
	return(NULL);

    while (*temp != NULL) {

	length += strlen(*temp) + sizeof(char);

	/* if no special characters (white space, ', ") just copy */

	if ( strpbrk(*temp, delim) != NULL) {
		length += 2 * sizeof(char);	/* quote the string */

		escape = '\"';
	
		strp = *temp;
		while (*strp != '\0')	/* escape quotes within string */
			if (*strp++ == escape)
				length += sizeof(char);
	}
	temp++;

    }

    if ((! *argvp) || ((cmdp = cmdstr = (char *) malloc(length)) == NULL))
		return(NULL);

    while (*argvp != NULL) {

	/* if no special characters (white space, ', ") just copy */

	if ( strpbrk(*argvp, delim) == NULL)
		escape = '\0';
	else
		escape = '\"';
	
	strp = *argvp;
	if (escape != '\0')
		*cmdp++ = escape;
	while (*strp != '\0') {
		if (*strp == escape)
			*cmdp++ = '\\';
		*cmdp++ = *strp++;
	}
	if (escape != '\0')
		*cmdp++ = escape;
	*cmdp++ = ' ';
	argvp++;
    }

    *(--cmdp) = '\0';

    return(cmdstr);
}
