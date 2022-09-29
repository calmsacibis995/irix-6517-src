/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/execvp.c	1.21"
/*LINTLIBRARY*/
/*
 *	execvp(name, argv)	(like execv, but does path search)
 */
#ifdef __STDC__
	#pragma weak execvp = _execvp
#endif
#include "synonyms.h"
#include <sys/errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <paths.h>
#include <unistd.h>
#include <string.h> 	/* prototype for strchr() */
#include <errno.h> 	/* prototype for errno */
#include <wait.h>
#include "proc_extern.h"


int
execvp(const char *name, char *const *argv)
{
	const char	*pathstr;
	char	fname[PATH_MAX+2];
	char	*newargs[256];
	char	**sargs, **pnewargs, **ma = NULL;
	int	i, nargs;
	register const char	*cp;
	register unsigned etxtbsy=1;
	register int eacces=0;

	if (*name == '\0') {
		setoserror(ENOENT);
		return(-1);
	}
	if((pathstr = getenv("PATH")) == NULL)
		pathstr = _PATH_USERPATH;
	cp = strchr(name, '/')? (const char *)"": pathstr;

	do {
		cp = __execat(cp, name, fname);
	retry:
		(void) execv(fname, argv);
		switch(oserror()) {
		case ENOEXEC:
			/* count # of args */
			for(nargs=0; argv[nargs]; ++nargs)
				;
			if (nargs < 254)
				pnewargs = newargs;
			else
				ma = pnewargs = malloc((size_t)(nargs+2) * sizeof(char *));
			if (pnewargs == NULL) {
				setoserror(ENOMEM);
				return(-1);
			}
			sargs = pnewargs;
			*pnewargs++ = "sh";
			if(*fname == '-')
				*pnewargs++ = "--";
			*pnewargs++ = fname;
			for(i=1; *pnewargs++=argv[i]; ++i)
				;
			(void) execv((const char *)_PATH_BSHELL, (char *const *)sargs);
			if (ma)
				free((void *)ma);
			return(-1);
		case ETXTBSY:
			if(++etxtbsy > 5)
				return(-1);
			(void) sleep(etxtbsy);
			goto retry;
		case EACCES:
			++eacces;
			break;
		case EBADRQC:
		case ENOMEM:
		case E2BIG:
		case EFAULT:
			return(-1);
		}
	} while(cp);
	if(eacces)
		setoserror(EACCES);
	return(-1);
}

const char *
__execat(
	register const char *s1,
	register const char *s2,
	char	*si)
{
	register char	*s;
	register int cnt = PATH_MAX + 1; /* number of characters in s2 */

	s = si;
	while(*s1 && *s1 != ':') {
		if (cnt > 0) {
			*s++ = *s1++;
			cnt--;
		} else
			s1++;
	}
	if(si != s && cnt > 0) {
		*s++ = '/';
		cnt--;
	}
	while(*s2 && cnt > 0) {
		*s++ = *s2++;
		cnt--;
	}
	*s = '\0';
	return(*s1? ++s1: 0);
}
