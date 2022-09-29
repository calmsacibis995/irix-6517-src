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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/printers/RCS/delprinter.c,v 1.1 1992/12/14 13:33:17 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "errno.h"
#include "sys/types.h"
#include "stdlib.h"

#include "lp.h"
#include "printers.h"

#if	defined(__STDC__)
static int		_delprinter ( char * );
#else
static int		_delprinter();
#endif

/**
 ** delprinter()
 **/

int
#if	defined(__STDC__)
delprinter (
	char *			name
)
#else
delprinter (name)
	char			*name;
#endif
{
	long			lastdir;


	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	if (!Lp_A_Printers || !Lp_A_Interfaces) {
		getadminpaths (LPUSER);
		if (!Lp_A_Printers || !Lp_A_Interfaces)
			return (0);
	}

	if (STREQU(NAME_ALL, name)) {
		lastdir = -1;
		while ((name = next_dir(Lp_A_Printers, &lastdir)))
			if (_delprinter(name) == -1)
				return (-1);
		return (0);
	} else
		return (_delprinter(name));
}

/**
 ** _delprinter()
 **/

static int
#if	defined(__STDC__)
_delprinter (
	char *			name
)
#else
_delprinter (name)
	char			*name;
#endif
{
	register char		*path;

#define RMFILE(X)	if (!(path = getprinterfile(name, X))) \
				return (-1); \
			if (rmfile(path) == -1) { \
				Free (path); \
				return (-1); \
			} \
			Free (path)
	RMFILE (COMMENTFILE);
	RMFILE (CONFIGFILE);
	RMFILE (FALLOWFILE);
	RMFILE (FDENYFILE);
	RMFILE (UALLOWFILE);
	RMFILE (UDENYFILE);
	RMFILE (STATUSFILE);

	delalert (Lp_A_Printers, name);

	if (!(path = makepath(Lp_A_Interfaces, name, (char *)0)))
		return (-1);
	if (rmfile(path) == -1) {
		Free (path);
		return (-1);
	}
	Free (path);

	if (!(path = getprinterfile(name, (char *)0)))
		return (-1);
	if (Rmdir(path) == -1) {
		Free (path);
		return (-1);
	}
	Free (path);

	return (0);
}
