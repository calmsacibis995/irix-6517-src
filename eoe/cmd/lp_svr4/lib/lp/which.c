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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/which.c,v 1.1 1992/12/14 13:29:53 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "ctype.h"
#include "string.h"
#include "stdlib.h"
#include "unistd.h"

#include "lp.h"

/**
 ** isprinter() - SEE IF ARGUMENT IS A REAL PRINTER
 **/

int
#if	defined(__STDC__)
isprinter (
	char *			str
)
#else
isprinter (str)
	char			*str;
#endif
{
	char			*path	= 0;

	int			bool;

	bool = (
		str
	     && *str
	     && (path = getprinterfile(str, CONFIGFILE))
	     && Access(path, F_OK) == 0
	);
	if (path)
		Free (path);
	return (bool);
}

/**
 ** isclass() - SEE IF ARGUMENT IS A REAL CLASS
 **/

int
#if	defined(__STDC__)
isclass (
	char *			str
)
#else
isclass (str)
	char			*str;
#endif
{
	char			*path	= 0;

	int			bool;

	bool = (
		str
	     && *str
	     && (path = getclassfile(str))
	     && Access(path, F_OK) == 0
	);
	if (path)
		Free (path);
	return (bool);
}

/**
 ** isrequest() - SEE IF ARGUMENT LOOKS LIKE A REAL REQUEST
 **/

int
#if	defined(__STDC__)
isrequest (
	char *			str
)
#else
isrequest (str)
	char			*str;
#endif
{
	char			*dashp;

	/*
	 * Valid print requests have the form
	 *
	 *	dest-NNN
	 *
	 * where ``dest'' looks like a printer or class name.
	 * An earlier version of this routine checked to see if
	 * the ``dest'' was an EXISTING printer or class, but
	 * that caused problems with valid requests moved from
	 * a deleted printer or class (the request ID doesn't
	 * change in the new LP).
	 */

	if (!str || !*str)
		return (0);

	if (!(dashp = strchr(str, '-')))
		return (0);

	if (dashp == str)
	    return(0);

	*dashp = 0;
	if (!syn_name(str)) {
		*dashp = '-';
		return (0);
	}
	*dashp++ = '-';

	if (!isnumber(dashp))
		return (0);

	return (1);
}

int
#if	defined(__STDC__)
isnumber (
	char *			s
)
#else
isnumber (s)
	char			*s;
#endif
{
	register int		c;

	if (!s || !*s)
		return (0);
	while ((c = *(s++)) != '\0')
		if (!isdigit(c))
			return (0);
	return (1);
}
