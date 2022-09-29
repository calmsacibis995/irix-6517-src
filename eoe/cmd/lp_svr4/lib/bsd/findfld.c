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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/bsd/RCS/findfld.c,v 1.1 1992/12/14 13:24:40 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <string.h>
#include "lp.h"

/*
 * Return pointer to field in 'buf' beginning with 'ptn'
 * (Input string unmolested)
 */
char *
#if defined (__STDC__)
find_strfld(register char *ptn, register char *buf)
#else
find_strfld(ptn, buf)
register char	*ptn;
register char	*buf;
#endif
{
	char	**list;
	char	**pp;
	char	 *p = NULL;
	int 	  n = strlen(ptn);

	if (!(list = dashos(buf)))
		return(NULL);
	for (pp = list; *pp; pp++)
		if (STRNEQU(ptn, *pp, n)) {
			p = strdup(*pp);
			break;
		}
	freelist(list);
	return(p);
}

/*
 * Return pointer to list entry beginning with 'ptn'
 */
char *
#if defined (__STDC__)
find_listfld(register char *ptn, register char **list)
#else
find_listfld(ptn, list)
register char	 *ptn;
register char	**list;
#endif
{
	int	n = strlen(ptn);

	if (!list || !*list)
		return(NULL);
	for (; *list; ++list)
                if (STRNEQU(ptn, *list, n))
                        return (*list);
        return (NULL);
}
