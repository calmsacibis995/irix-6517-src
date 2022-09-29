/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/next.c,v 1.1 1992/12/14 13:29:07 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

#include "string.h"
#include "errno.h"

#include "lp.h"

#if	defined(__STDC__)
static int		is ( char *, char *, unsigned int );
#else
static int		is();
#endif

/**
 ** next_x() - GO TO NEXT ENTRY UNDER PARENT DIRECTORY
 **/

char *
#if	defined(__STDC__)
next_x (
	char *			parent,
	long *			lastdirp,
	unsigned int		what
)
#else
next_x (parent, lastdirp, what)
	char			*parent;
	long			*lastdirp;
	unsigned int		what;
#endif
{
	DIR			*dirp;

	register char		*ret = 0;

	struct dirent		*direntp;


	if (!(dirp = Opendir(parent)))
		return (0);

	if (*lastdirp != -1)
		Seekdir (dirp, *lastdirp);

	do
		direntp = Readdir(dirp);
	while (
		direntp
	     && (
			STREQU(direntp->d_name, ".")
		     || STREQU(direntp->d_name, "..")
		     || !is(parent, direntp->d_name, what)
		)
	);

	if (direntp) {
		if (!(ret = Strdup(direntp->d_name)))
			errno = ENOMEM;
		*lastdirp = Telldir(dirp);
	} else {
		errno = ENOENT;
		*lastdirp = -1;
	}

	Closedir (dirp);

	return (ret);
}

static int
#if	defined(__STDC__)
is (
	char *			parent,
	char *			name,
	unsigned int		what
)
#else
is (parent, name, what)
	char			*parent;
	char			*name;
	unsigned int		what;
#endif
{
	char			*path;

	struct stat		statbuf;

	if (!(path = makepath(parent, name, (char *)0)))
		return (0);
	if (Stat(path, &statbuf) == -1) {
		Free (path);
		return (0);
	}
	Free (path);
	return ((statbuf.st_mode & S_IFMT) == what);
}
