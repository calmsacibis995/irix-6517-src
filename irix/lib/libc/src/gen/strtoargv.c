/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/strtoargv.c	1.1.1.2"

#ifdef	__STDC__
	#pragma weak	strtoargv = _strtoargv
#endif

#include "synonyms.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
 * strtoargv:    Given a pointer to a command line string,
 *	parse it into an argv array.
 *
 */

#define BUNCH 25
static const char delim[] = { " \t'\"" };	/* delimiters */

char **
strtoargv(char *cmdp)
{
    int nargs = 0;
    char **cmdargv = NULL;
    int i = 0;
    char delch;
    register char *cp;

    if (!cmdp)
	return(NULL);

    /* skip leading white space */
    while (isspace(*cmdp))
	cmdp++;

    if (!nargs) {
	nargs += BUNCH;
    	cmdargv = (char **) malloc(sizeof(char *) * (size_t)(nargs + 1));
    	if (cmdargv == NULL)
		return(NULL);
    }

    while (*cmdp) {
	if ( i >= nargs ) {
	    nargs += BUNCH;
	    cmdargv = (char **) realloc(cmdargv, sizeof(char *) * (size_t)(nargs + 1));
	    if (cmdargv == NULL)
		return(NULL);
	}
	    
	cmdargv[i] = cmdp;
	if (cmdp = (char *) strpbrk(cmdp, delim)) {
	    switch (*cmdp) {
	    /* normal separators, space and tab */
	    case ' ':
	    case '\t':
		*cmdp++ = '\0';
		break;
	    /* quoted arguments using " or ' */
	    case '"':
	    case '\'':
		delch = *cmdp; /* remember the delimiter */
		if ( cmdargv[i] != cmdp )
			break;	/* quote in the middle means new token	*/

		*cmdp = '\0';	/* clear the delimiter	*/
		cmdargv[i] = ++cmdp; /* skip the quote char */

		/* we must skip escaped quote characters
		 * i.e. \" or \'. cp is the pointer to the
		 * the real (i.e. edited) string position.
		 */

		for (cp = cmdp;;) {
		    if (*cmdp == '\0')
			return(NULL);
		    if (*cmdp == delch) {
			if (*(cp - 1) == '\\') {
			    /* got \" or \' */
			    *(cp - 1) = *cmdp;
			    cmdp++;
		        } else { /* end of string */
			    *cp = '\0';
			    cmdp++;
			    break;
			}
		    } else {
			*cp++ = *cmdp++;
		    }
		}
		break;

	    default:
		return(NULL);
	    }
	} else {
	    /* no more delimiters */
	    i++;
	    break;
	}

	i++;

	/* skip trailing white space */

	while (isspace(*cmdp))
	    cmdp++;
    }

    cmdargv[i] = NULL;

    return(cmdargv);
}
