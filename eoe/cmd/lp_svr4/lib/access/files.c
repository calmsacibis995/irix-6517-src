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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/access/RCS/files.c,v 1.1 1992/12/14 13:24:15 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "errno.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "stdlib.h"

#include "lp.h"

/**
 ** getaccessfile() - BUILD NAME OF ALLOW OR DENY FILE
 **/

char *
#if	defined(__STDC__)
getaccessfile (
	char *			dir,
	char *			name,
	char *			prefix,
	char *			base
)
#else
getaccessfile (dir, name, prefix, base)
	char			*dir,
				*name,
				*prefix,
				*base;
#endif
{
	register char		*parent,
				*file,
				*f;

	/*
	 * It makes no sense talking about the access files if
	 * the directory for the form or printer doesn't exist.
	 */
	parent = makepath(dir, name, (char *)0);
	if (!parent)
		return (0);
	if (Access(parent, F_OK) == -1)
		return (0);

	if (!(f = makestr(prefix, base, (char *)0))) {
		errno = ENOMEM;
		return (0);
	}
	file = makepath(parent, f, (char *)0);
	Free (f);
	Free (parent);

	return (file);
}
