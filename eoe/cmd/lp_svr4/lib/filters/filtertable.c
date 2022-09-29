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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/filters/RCS/filtertable.c,v 1.1 1992/12/14 13:25:53 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "errno.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#include "lp.h"
#include "filters.h"

/**
 ** get_and_load() - LOAD REGULAR FILTER TABLE
 **/

int
#if	defined(__STDC__)
get_and_load (
	void
)
#else
get_and_load ()
#endif
{
	register char		*file;

	if (!(file = getfilterfile(FILTERTABLE)))
		return (-1);
	if (loadfilters(file) == -1) {
		Free (file);
		return (-1);
	}
	Free (file);
	return (0);
}

/**
 ** open_filtertable()
 **/

FILE *
#if	defined(__STDC__)
open_filtertable (
	char *			file,
	char *			mode
)
#else
open_filtertable (file, mode)
	char			*file,
				*mode;
#endif
{
	int			freeit;

	FILE			*fp;

	if (!file) {
		if (!(file = getfilterfile(FILTERTABLE)))
			return (0);
		freeit = 1;
	} else
		freeit = 0;
	
	fp = open_lpfile(file, mode, MODE_READ);

	if (freeit)
		Free (file);

	return (fp);
}

/**
 ** close_filtertable()
 **/

void
#if	defined(__STDC__)
close_filtertable (
	FILE *			fp
)
#else
close_filtertable (fp)
	FILE			*fp;
#endif
{
	(void)close_lpfile (fp);
	return;
}
