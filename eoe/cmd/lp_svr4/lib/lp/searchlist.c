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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/searchlist.c,v 1.1 1992/12/14 13:29:16 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "string.h"

#include "lp.h"

/**
 ** searchlist() - SEARCH (char **) LIST FOR ITEM
 **/

int
#if	defined(__STDC__)
searchlist (
	char *			item,
	char **			list
)
#else
searchlist (item, list)
	register char		*item;
	register char		**list;
#endif
{
	if (!list || !*list)
		return (0);

	else if (STREQU(item, NAME_ANY) || STREQU(item, NAME_ALL))
		return (1);

	/*
	 * This is a linear search--we believe that the lists
	 * will be short.
	 */
	while (*list) {
		if (
			STREQU(*list, item)
		     || STREQU(*list, NAME_ANY)
		     || STREQU(*list, NAME_ALL)
		)
			return (1);
		list++;
	}
	return (0);
}

/**
 ** searchlist_with_terminfo() - SEARCH (char **) LIST FOR ITEM
 **/

int
#if	defined(__STDC__)
searchlist_with_terminfo (
	char *			item,
	char **			list
)
#else
searchlist_with_terminfo (item, list)
	register char		*item;
	register char		**list;
#endif
{
	if (!list || !*list)
		return (0);

	else if (STREQU(item, NAME_ANY) || STREQU(item, NAME_ALL))
		return (1);

	/*
	 * This is a linear search--we believe that the lists
	 * will be short.
	 */
	while (*list) {
		if (
			STREQU(*list, item)
		     || STREQU(*list, NAME_ANY)
		     || STREQU(*list, NAME_ALL)
		     || (
				STREQU(*list, NAME_TERMINFO)
			     && isterminfo(item)
			)
		)
			return (1);
		list++;
	}
	return (0);
}
