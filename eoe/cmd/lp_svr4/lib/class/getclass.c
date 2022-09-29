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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/class/RCS/getclass.c,v 1.1 1992/12/14 13:25:25 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "sys/types.h"

#include "lp.h"
#include "class.h"

/**
 ** getclass() - READ CLASS FROM TO DISK
 **/

CLASS *
#if	defined(__STDC__)
getclass (
	char *			name
)
#else
getclass (name)
	char			*name;
#endif
{
	static long		lastdir		= -1;

	static CLASS		clsbuf;

	char			*file,
				buf[BUFSIZ];

	FILE			*fp;


	if (!name || !*name) {
		errno = EINVAL;
		return (0);
	}

	/*
	 * Getting ``all''? If so, jump into the directory
	 * wherever we left off.
	 */
	if (STREQU(NAME_ALL, name)) {
		if (!Lp_A_Classes) {
			getadminpaths (LPUSER);
			if (!Lp_A_Classes)
				return (0);
		}
		if (!(name = next_file(Lp_A_Classes, &lastdir)))
			return (0);
	} else
		lastdir = -1;

	/*
	 * Get the class list.
	 */

	if (!(file = getclassfile(name)))
		return (0);

	if (!(fp = open_lpfile(file, "r", 0))) {
		Free (file);
		return (0);
	}
	Free (file);

	if (!(clsbuf.name = Strdup(name))) {
		close_lpfile (fp);
		errno = ENOMEM;
		return (0);
	}

	clsbuf.members = 0;
	while (fgets(buf, BUFSIZ, fp)) {
		buf[strlen(buf) - 1] = 0;
		addlist (&clsbuf.members, buf);
	}
	if (ferror(fp)) {
		int			save_errno = errno;

		freelist (clsbuf.members);
		Free (clsbuf.name);
		close_lpfile (fp);
		errno = save_errno;
		return (0);
	}
	close_lpfile (fp);

	if (!clsbuf.members) {
		Free (clsbuf.name);
		errno = EBADF;
		return (0);
	}

	return (&clsbuf);
}
