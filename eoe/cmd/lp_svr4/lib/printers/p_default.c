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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/printers/RCS/p_default.c,v 1.1 1992/12/18 18:11:46 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "stdlib.h"

#include "lp.h"

/**
 ** getdefault() - READ THE NAME OF THE DEFAULT DESTINATION FROM DISK
 **/

char *
#if	defined(__STDC__)
getdefault (
	void
)
#else
getdefault ()
#endif
{
	return (loadline(Lp_Default));
}

/**
 ** putdefault() - WRITE THE NAME OF THE DEFAULT DESTINATION TO DISK
 **/

int
#if	defined(__STDC__)
putdefault (
	char *			dflt
)
#else
putdefault (dflt)
	char			*dflt;
#endif
{
	register FILE		*fp;

	if (!dflt || !*dflt)
		return (deldefault());

	if (!(fp = open_lpfile(Lp_Default, "w", MODE_READ)))
		return (-1);

	fprintf (fp, "%s\n", dflt);

	close_lpfile (fp);
	return (0);
}

/**
 ** deldefault() - REMOVE THE NAME OF THE DEFAULT DESTINATION
 **/

int
#if	defined(__STDC__)
deldefault (
	void
)
#else
deldefault ()
#endif
{
	return (rmfile(Lp_Default));
}
