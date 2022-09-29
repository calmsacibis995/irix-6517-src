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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/printlist.c,v 1.1 1992/12/14 13:29:10 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "string.h"

#include "lp.h"

#define	DFLT_PREFIX	0
#define	DFLT_SUFFIX	0
#define	DFLT_SEP	"\n"
#define	DFLT_NEWLINE	"\n"

int			printlist_qsep	= 0;

static char		*print_prefix	= DFLT_PREFIX,
			*print_suffix	= DFLT_SUFFIX,
			*print_sep	= DFLT_SEP,
			*print_newline	= DFLT_NEWLINE;

#if	defined(__STDC__)
static void		q_print ( FILE * , char * , char * );
#else
static void		q_print();
#endif

/**
 ** printlist_setup() - ARRANGE FOR CUSTOM PRINTING
 ** printlist_unsetup() - RESET STANDARD PRINTING
 **/

void
#if	defined(__STDC__)
printlist_setup (
	char *			prefix,
	char *			suffix,
	char *			sep,
	char *			newline
)
#else
printlist_setup (prefix, suffix, sep, newline)
	char			*prefix,
				*suffix,
				*sep,
				*newline;
#endif
{
	if (prefix)
		print_prefix = prefix;
	if (suffix)
		print_suffix = suffix;
	if (sep)
		print_sep = sep;
	if (newline)
		print_newline = newline;
	return;
}

void
#if	defined(__STDC__)
printlist_unsetup (
	void
)
#else
printlist_unsetup ()
#endif
{
	print_prefix = DFLT_PREFIX;
	print_suffix = DFLT_SUFFIX;
	print_sep = DFLT_SEP;
	print_newline = DFLT_NEWLINE;
	return;
}

/**
 ** printlist() - PRINT LIST ON OPEN CHANNEL
 **/

int
#if	defined(__STDC__)
printlist (
	FILE *			fp,
	char **			list
)
#else
printlist (fp, list)
	register FILE		*fp;
	register char		**list;
#endif
{
	register char		*sep;

	if (list)
	    for (sep = ""; *list; *list++, sep = print_sep) {

		(void)fprintf (fp, "%s%s", sep, NB(print_prefix));
		if (printlist_qsep)
			q_print (fp, *list, print_sep);
		else
			(void)fprintf (fp, "%s", *list);
		(void)fprintf (fp, "%s", NB(print_suffix));
		if (ferror(fp))
			return (-1);

	    }
	(void)fprintf (fp, print_newline);

	return (0);
}

/**
 ** q_print() - PRINT STRING, QUOTING SEPARATOR CHARACTERS
 **/

static void
#if	defined(__STDC__)
q_print (
	FILE *			fp,
	char *			str,
	char *			sep
)
#else
q_print (fp, str, sep)
	FILE			*fp;
	register char		*str,
				*sep;
#endif
{
	while (*str) {
		if (strchr(sep, *str))
			putc ('\\', fp);
		putc (*str, fp);
		str++;
	}
	return;
}
