/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/putenv.c	1.11"
/*	LINTLIBRARY	*/
/*	putenv - change environment variables

	input - char *change = a pointer to a string of the form
			       "name=value"

	output - 0, if successful
		 1, otherwise
*/
#ifdef __STDC__
	#pragma weak putenv = _putenv
#endif
#include "synonyms.h"
#include "shlib.h"
#include <string.h>
#include <stdlib.h>
#include "priv_extern.h"

static int reall = 0;		/* flag to reallocate space, if putenv is called
				   more than once */
static int find(const char *), match(char *, const char *);

int
putenv(const char *change)
{
	char **newenv;		    /* points to new environment */
	register int which;	    /* index of variable to replace */

	if (change == NULL)
		return -1;
	if ((which = find(change)) < 0)  {
		/* if a new variable */
		/* which is negative of table size, so invert and
		   count new element */
		which = (-which) + 1;
		if (reall)  {
			/* we have expanded environ before */
			newenv = (char **)realloc((void *)environ,
				  ((size_t)which*sizeof(char *)));
			if (newenv == NULL)  return -1;
			/* now that we have space, change environ */
			environ = (char **)newenv;
		} else {
			/* environ points to the original space */
			reall++;
			newenv = (char **)malloc(((size_t)which*sizeof(char *)));
			if (newenv == NULL)  return -1;
			(void)memcpy((char *)newenv, (char *)environ,
 				((size_t)(which-1)*sizeof(char *)));
			environ = (char **)newenv;
		}
		environ[which-2] = (char *)change;
		environ[which-1] = NULL;
	}  else  {
		/* we are replacing an old variable */
		environ[which] = (char *)change;
	}
	return 0;
}

/*	find - find where s2 is in environ
 *
 *	input - str = string of form name=value
 *
 *	output - index of name in environ that matches "name"
 *		 -size of table, if none exists
*/
static int
find(const char *str)
{
	register int ct = 0;	/* index into environ */

	while(environ[ct] != NULL)   {
		if (match(environ[ct], str)  != 0)
			return ct;
		ct++;
	}
	return -(++ct);
}
/*
 *	s1 is either name, or name=value
 *	s2 is name=value
 *	if names match, return value of 1,
 *	else return 0
 */

static int
match(register char *s1, const char *s2)
{
	while(*s1 == *s2++)  {
		if (*s1 == '=')
			return 1;
		s1++;
	}
	return 0;
}
