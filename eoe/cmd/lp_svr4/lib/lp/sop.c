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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/sop.c,v 1.1 1992/12/14 13:29:31 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "stdlib.h"

#include "lp.h"

/**
 ** sop_up_rest() - READ REST OF FILE INTO STRING
 **/

char *
#if	defined(__STDC__)
sop_up_rest (
	FILE *			fp,
	char *			endsop
)
#else
sop_up_rest (fp, endsop)
	FILE			*fp;
	register char		*endsop;
#endif
{
	register int		size,
				add_size,
				lenendsop;

	register char		*str;

	char			buf[BUFSIZ];


	str = 0;
	size = 0;
	if (endsop)
		lenendsop = strlen(endsop);

	while (fgets(buf, BUFSIZ, fp)) {
		if (endsop && STRNEQU(endsop, buf, lenendsop))
			break;
		add_size = strlen(buf);
		if (str)
			str = Realloc(str, size + add_size + 1);
		else
			str = Malloc(size + add_size + 1);
		if (!str) {
			errno = ENOMEM;
			return (0);
		}
		strcpy (str + size, buf);
		size += add_size;
	}
	if (ferror(fp)) {
		Free (str);
		return (0);
	}
	return (str);
}
