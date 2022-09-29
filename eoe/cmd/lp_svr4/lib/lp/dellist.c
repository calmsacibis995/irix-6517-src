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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/dellist.c,v 1.1 1992/12/14 13:28:10 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "string.h"
#include "stdlib.h"

#include "lp.h"

/**
 ** dellist() - REMOVE ITEM FROM (char **) LIST
 **/

int
#if	defined(__STDC__)
dellist (
	char ***		plist,
	char *			item
)
#else
dellist (plist, item)
	register char		***plist,
				*item;
#endif
{
	register char		**pl;

	register int		n;

	if (*plist) {

		n = lenlist(*plist);

		for (pl = *plist; *pl; pl++)
			if (STREQU(*pl, item))
				break;

		if (*pl) {
			Free (*pl);
			for (; *pl; pl++)
				*pl = *(pl+1);
			if (--n == 0) {
				Free ((char *)*plist);
				*plist = 0;
			} else {
				*plist = (char **)Realloc(
					(char *)*plist,
					(n + 1) * sizeof(char *)
				);
				if (!*plist) {
					errno = ENOMEM;
					return (-1);
				}
/*				(*plist)[n] = 0; /* done in "for" loop */
			}
		}

	}

	return (0);
}
