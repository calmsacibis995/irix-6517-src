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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/class/RCS/delclass.c,v 1.1 1992/12/14 13:25:13 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "sys/types.h"
#include "string.h"

#include "lp.h"
#include "class.h"

#if	defined(__STDC__)
static int		_delclass ( char * );
#else
static int		_delclass();
#endif

/**
 ** delclass() - WRITE CLASS OUT TO DISK
 **/

static int		_delclass();

int
#if	defined(__STDC__)
delclass (
	char *			name
)
#else
delclass (name)
	char			*name;
#endif
{
	long			lastdir;


	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	if (STREQU(NAME_ALL, name)) {
		lastdir = -1;
		if (!Lp_A_Classes) {
			getadminpaths (LPUSER);
			if (!Lp_A_Classes)
				return (0);
		}
		while ((name = next_file(Lp_A_Classes, &lastdir)))
			if (_delclass(name) == -1)
				return (-1);
		return (0);
	} else
		return (_delclass(name));
}

/**
 ** _delclass()
 **/

static int
#if	defined(__STDC__)
_delclass (
	char *			name
)
#else
_delclass (name)
	char			*name;
#endif
{
	char			*path;

	if (!(path = getclassfile(name)))
		return (-1);
	if (rmfile(path) == -1) {
		Free (path);
		return (-1);
	}
	Free (path);
	return (0);
}

