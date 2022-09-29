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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/cs_strncmp.c,v 1.1 1992/12/14 13:28:00 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "ctype.h"

/*
 * Compare strings (at most n bytes) ignoring case
 *	returns: s1>s2; >0  s1==s2; 0  s1<s2; <0
 */

int
#if	defined(__STDC__)
cs_strncmp(
	char *			s1,
	char *			s2,
	int			n
)
#else
cs_strncmp(s1, s2, n)
register char *s1, *s2;
register n;
#endif
{
	if(s1 == s2)
		return(0);
	while(--n >= 0 && toupper(*s1) == toupper(*s2++))
		if(*s1++ == '\0')
			return(0);
	return((n < 0)? 0: (toupper(*s1) - toupper(*--s2)));
}
