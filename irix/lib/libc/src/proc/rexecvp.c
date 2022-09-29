/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	rexecvp(name, argv)	(like rexecv, but does path search)
 */
#ifdef __STDC__
	#pragma weak rexecvp = _rexecvp
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
rexecvp(cell_t cell, const char *name, char *const *argv)
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
		(void) rexecv(cell, fname, argv);
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
			(void) rexecv(cell, (const char *)_PATH_BSHELL, (char *const *)sargs);
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
