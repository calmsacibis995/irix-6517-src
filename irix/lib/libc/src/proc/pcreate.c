/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.17 $ $Author: jwag $"


#ifdef __STDC__
	#pragma weak pcreateve = _pcreateve
	#pragma weak pcreatev = _pcreatev
	#pragma weak pcreatevp = _pcreatevp
#endif
#include "synonyms.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "stdio.h"
#include "paths.h"
#include "errno.h"
#include <stdarg.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <stdlib.h> 	/* prototype for getenv() */
#include <string.h> 	/* prototype for strchr() */
#include <wait.h>
#include "proc_extern.h"
#include "priv_extern.h"
#include "libc_interpose.h"

static void pcreateslave(void *, size_t);

/*
 * pcreate - create a process w/o consuming virtual space
 * a combined fork/exec
 */
pid_t
pcreateve(const char *path, char * const* argv, char * const* envp)
{
	pid_t pid;
	ptrdiff_t ap[4];
	char **newargs;

	if (newargs = __tp_pcreateve(path, argv, envp)) {
		/* put args in a simple place */
		ap[0] = (ptrdiff_t)newargs[0];
		ap[1] = (ptrdiff_t)newargs[1];
		ap[2] = (ptrdiff_t)newargs[2];
	} else {
		/* put args in a simple place */
		ap[0] = (ptrdiff_t)path;
		ap[1] = (ptrdiff_t)argv;
		ap[2] = (ptrdiff_t)envp;
	}
	ap[3] = 0L;

	/* create new process, sharing virtual space */
	if ((pid = sprocsp(pcreateslave, PR_NOLIBC|PR_SADDR|PR_SFDS|PR_BLOCK,
			(void *)ap, NULL, 32*1024)) < 0)
		return(pid);

	/* we will block until child execs */
	if (ap[3]) {
		/* exec failed */
		setoserror((int)(ap[3]));
		return -1;
	} else
		return(pid);
}

/* ARGSUSED1 */
static void
pcreateslave(void *a, size_t slen)
{
	char **ap = (char **)a;
	/* set us up to unblock parent on exit */
	if (prctl(PR_UNBLKONEXEC, getppid()) < 0) {
		/* uh-oh - lets just exit */
		_exit(-1);
	}
	execve(ap[0], (char *const *)ap[1], (char *const *)ap[2]);
	((volatile char **)ap)[3] = (char *)(ptrdiff_t)errno;
	_exit(-2);
}

/*
 * other members of the exec family
 */
pid_t
pcreatev(const char *path, char *const *argv)
{
	return(pcreateve(path, argv, _environ));
}

/*
 *	pcreatelp(name, arg,...,0)	(like pcreatel, but does path search)
 *	pcreatevp(name, argv)		(like pcreatev, but does path search)
 */
pid_t
pcreatevp(const char *name, char *const*argv)
{
	char	*pathstr;
	char	fname[PATH_MAX+2];
	char	*newargs[256];
	char	**sargs, **pnewargs, **ma = NULL;
	int	i, nargs;
	pid_t	pid;
	register const char	*cp;
	register unsigned etxtbsy=1;
	register int eacces=0;

	if((pathstr = getenv("PATH")) == NULL)
		pathstr = _PATH_USERPATH;
	cp = strchr(name, '/')? "": pathstr;

	do {
		cp = __execat(cp, name, fname);
	retry:
		if ((pid = pcreatev(fname, argv)) > 0)
			/* worked */
			return(pid);
		switch(errno) {
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
			*pnewargs++ = fname;
			for(i=1; *pnewargs++=argv[i]; ++i)
				;
			pid = pcreatev((const char *)_PATH_BSHELL, (char *const *)sargs);
			if (ma)
				free(ma);
			if (pid > 0)
				return(pid);
			return(-1);
		case ETXTBSY:
			if(++etxtbsy > 5)
				return(-1);
			(void) sleep(etxtbsy);
			goto retry;
		case EACCES:
			++eacces;
			break;
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
