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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/access/RCS/dumpaccess.c,v 1.1 1992/12/14 13:24:12 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "errno.h"
#include "stdlib.h"

#include "lp.h"
#include "access.h"

#if	defined(__STDC__)
static int		_dumpaccess ( char * , char ** );
#else
static int		_dumpaccess();
#endif

/**
 ** dumpaccess() - DUMP ALLOW OR DENY LISTS
 **/

int
#if	defined(__STDC__)
dumpaccess (
	char *			dir,
	char *			name,
	char *			prefix,
	char ***		pallow,
	char ***		pdeny
)
#else
dumpaccess (dir, name, prefix, pallow, pdeny)
	char			*dir,
				*name,
				*prefix,
				***pallow,
				***pdeny;
#endif
{
	register char		*allow_file	= 0,
				*deny_file	= 0;

	int			ret;

	if (
		!(allow_file = getaccessfile(dir, name, prefix, "allow"))
	     || _dumpaccess(allow_file, *pallow) == -1 && errno != ENOENT
	     || !(deny_file = getaccessfile(dir, name, prefix, "deny"))
	     || _dumpaccess(deny_file, *pdeny) == -1 && errno != ENOENT
	)
		ret = -1;
	else
		ret = 0;

	if (allow_file)
		Free (allow_file);
	if (deny_file)
		Free (deny_file);

	return (ret);
}

/**
 ** _dumpaccess() - DUMP ALLOW OR DENY FILE
 **/

static int
#if	defined(__STDC__)
_dumpaccess (
	char *			file,
	char **			list
)
#else
_dumpaccess (file, list)
	char			*file,
				**list;
#endif
{
	register char		**pl;

	register int		ret;

	FILE			*fp;

	if (list) {
		if (!(fp = open_lpfile(file, "w", MODE_READ)))
			return (-1);
		for (pl = list; *pl; pl++)
			fprintf (fp, "%s\n", *pl);
		if (ferror(fp))
			ret = -1;
		else
			ret = 0;
		close_lpfile (fp);
	} else
		ret = Unlink(file);

	return (ret);
}
