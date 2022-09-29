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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/syntax.c,v 1.1 1992/12/14 13:29:41 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "ctype.h"
#include "string.h"

#include "lp.h"

int
#if	defined(__STDC__)
syn_name (
	char *			str
)
#else
syn_name (str)
	char			*str;
#endif
{
	register char		*p;

	if (!*str)
	  	return(0);

	if (strlen(str) > 14)
		return (0);

	for (p = str; *p; p++)
		if (!isalnum(*p) && *p != '_')
			return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_type (
	char *			str
)
#else
syn_type (str)
	char			*str;
#endif
{
	register char		*p;

	if (strlen(str) > 14)
		return (0);

	for (p = str; *p; p++)
		if (!isalnum(*p) && *p != '-')
			return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_text (
	char *			str
)
#else
syn_text (str)
	char			*str;
#endif
{
	register char		*p;

	for (p = str; *p; p++)
		if (!isgraph(*p) && *p != '\t' && *p != ' ')
			return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_comment (
	char *			str
)
#else
syn_comment (str)
	char			*str;
#endif
{
	register char		*p;

	for (p = str; *p; p++)
		if (!isgraph(*p) && *p != '\t' && *p != ' ' && *p != '\n')
			return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_machine_name (
	char *			str
)
#else
syn_machine_name (str)
	char			*str;
#endif
{
	if (strlen(str) > 8)
		return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_option (
	char *			str
)
#else
syn_option (str)
	char			*str;
#endif
{
	register char		*p;

	for (p = str; *p; p++)
		if (!isprint(*p))
			return (0);

	return (1);
}
