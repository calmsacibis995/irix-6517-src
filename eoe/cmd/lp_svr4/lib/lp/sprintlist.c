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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/sprintlist.c,v 1.1 1992/12/14 13:29:35 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"

#include "lp.h"

/**
 ** sprintlist() - FLATTEN (char **) LIST INTO (char *) LIST
 **/

char *
#if	defined(__STDC__)
sprintlist (
	char **			list
)
#else
sprintlist (list)
	char			**list;
#endif
{
	register char		**plist,
				*p,
				*q;

	char			*ret;

	int			len	= 0;


	if (!list || !*list)
		return (0);

	for (plist = list; *plist; plist++)
		len += strlen(*plist) + 1;

	if (!(ret = Malloc(len))) {
		errno = ENOMEM;
		return (0);
	}

	q = ret;
	for (plist = list; *plist; plist++) {
		p = *plist;
		while (*q++ = *p++)
			;
		q[-1] = ' ';
	}
	q[-1] = 0;

	return (ret);
}
