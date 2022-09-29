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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/class/RCS/putclass.c,v 1.1 1992/12/14 13:25:32 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "errno.h"
#include "sys/types.h"
#include "string.h"

#include "lp.h"
#include "class.h"

/**
 ** putclass() - WRITE CLASS OUT TO DISK
 **/

int
#if	defined(__STDC__)
putclass (
	char *			name,
	CLASS *			clsbufp
)
#else
putclass (name, clsbufp)
	char			*name;
	CLASS			*clsbufp;
#endif
{
	char			*file;

	FILE			*fp;


	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	if (STREQU(NAME_ALL, name)) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Open the class file and write out the class members.
	 */

	if (!(file = getclassfile(name)))
		return (-1);

	if (!(fp = open_lpfile(file, "w", MODE_READ))) {
		Free (file);
		return (-1);
	}
	Free (file);

	printlist (fp, clsbufp->members);

	if (ferror(fp)) {
		close_lpfile (fp);
		return (-1);
	}
	close_lpfile (fp);

	return (0);
}
