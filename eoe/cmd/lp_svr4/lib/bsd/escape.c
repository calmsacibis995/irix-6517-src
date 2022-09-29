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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/bsd/RCS/escape.c,v 1.1 1992/12/14 13:24:34 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "string.h"

/*
 * Return 1 if character in string is escaped by \, 0 otherwise
 */
#if defined (__STDC__)
escaped(char *cp)
#else
escaped(cp)
char	*cp;
#endif
{
	register char	*cp2 = cp;

	while (*--cp2 == '\\')
		;
	return((cp - ++cp2) & 1);
}

/*
 * Remove excape characters from string
 */
void
#if defined (__STDC__)
rmesc(register char *cp)
#else
rmesc(cp)
register char	*cp;
#endif
{
	register char	*cp2 = cp;

	for (; *cp2; cp++, cp2++) {
		if (*cp2 == '\\' || *cp2 == '\'')
			++cp2;
		*cp = *cp2;
	}
	*cp = NULL;
}

/*
 * Copy string, escaping special characters
 */
void
#if defined (__STDC__)
canonize(register char *to, register char *from, char *echars)
#else
canonize(to, from, echars)
register char	*to;
register char	*from;
char		*echars;
#endif
{
	register char	*p;

	for (; *from; from++, to++) {
		for (p = echars; *p; p++)
			if (*p == *from) {
				*to++ = '\\';
				break;
			}
		*to = *from;
	}
	*to = NULL;
}
