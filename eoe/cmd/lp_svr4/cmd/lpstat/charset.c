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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpstat/RCS/charset.c,v 1.1 1992/12/14 11:35:30 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "string.h"
#include "sys/types.h"
#include "stdlib.h"

#include "lp.h"
#include "form.h"
#include "access.h"
#include "printers.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"


extern char		*tparm();

#if	defined(__STDC__)
static void		putsline ( MOUNTED * );
#else
static void		putsline();
#endif

/**
 ** do_charset()
 **/

void
#if	defined(__STDC__)
do_charset (
	char **			list
)
#else
do_charset (list)
	char **			list;
#endif
{
	register MOUNTED *	pm;

	register int		found;


	while (*list) {
		if (STREQU(NAME_ALL, *list))
			for (pm = mounted_pwheels; pm->name; pm = pm->forward)
				putsline (pm);

		else {
			found = 0;
			for (pm = mounted_pwheels; pm->name; pm = pm->forward)
				if (
					pm->name[0] == '!'
				     && STREQU(pm->name + 1, *list)
				     || STREQU(pm->name, *list)
				) {
					putsline (pm);
					found = 1;
				}
			if (!found) {
				LP_ERRMSG1 (ERROR, E_STAT_BADSET, *list);
				exit_rc = 1;
			}
		}
		list++;
	}
	return;
}

/**
 ** putsline()
 **/

static void
#if	defined(__STDC__)
putsline (
	MOUNTED *		pm
)
#else
putsline (pm)
	register MOUNTED	*pm;
#endif
{
	register char **	pp;
	register char *		sep;
	char *			retmsg(int, long int);


	if (pm->name[0] != '!') {

		pfmt(stdout,INFO,NULL);
		sep = retmsg(E_STAT_PWHEEL);
		(void)printf ("%s %s", sep, pm->name);

		if ((pp = pm->printers))
			if (verbosity & V_LONG) {
				sep = retmsg(E_STAT_AVAIL);
				(void)printf ("%s", sep);
				while (*pp) {
					if ((*pp)[0] == '!') {
                                           sep = retmsg(E_STAT_MOUNT);
						(void)printf (
						       "\n\t\t%s %s",
							*pp + 1,
							sep
						);
					}
					else
						(void)printf ("\n\t\t%s", *pp);
					pp++;
				}
			} else {
				sep = retmsg(E_STAT_MOUNTON);
				while (*pp) {
					if ((verbosity & V_LONG) || (*pp)[0] == '!') {
						(void)printf (
							"%s%s",
							sep,
					((*pp)[0] == '!'? *pp + 1 : *pp)
						);
						sep = ",";
					}
					pp++;
				}
			}

		(void)printf ("\n");

	} else {

		sep = retmsg(E_STAT_CHARSET);
		(void)printf ("%s %s\n", sep, pm->name + 1);

		if ((verbosity & V_LONG) && (pp = pm->printers)) {
 			sep = retmsg(E_STAT_AVAIL);
			(void)printf ("%s\n", sep);
			while (*pp) {
				sep = retmsg(E_STAT_AS);
				(void)printf (
					"\t\t%s (%s %s)\n",
					strtok(*pp, "="),
					sep,
					strtok((char *)0, "=")
				);
				pp++;
			}
		}

	}
	return;
}

/**
 ** get_charsets() - CONSTRUCT (char **) LIST OF CHARSETS FROM csnm
 **/

char **
#if	defined(__STDC__)
get_charsets (
	PRINTER *		prbufp,
	int			addcs
)
#else
get_charsets (prbufp, addcs)
	PRINTER			*prbufp;
	register int		addcs;
#endif
{
	register int		cs		= 0;

	register char *		name;

	register char **	pt;

	char *			csnm;
	char **			list		= 0;


	if (
		prbufp->printer_types
	     && !STREQU(*(prbufp->printer_types), NAME_UNKNOWN)
	     && !prbufp->daisy
	)
	  for (pt = prbufp->printer_types; *pt; pt++)
	    if (tidbit(*pt, "csnm", &csnm) != -1 && csnm && *csnm) {
		for (cs = 0; cs <= 63; cs++)
			if ((name = tparm(csnm, cs)) && *name) {

				if (addcs) {
					register char	 *nm = Malloc(
						2+2+1 + strlen(name) + 1
					);

					sprintf (nm, "cs%d=%s", cs, name);
					name = nm;
				}

				if (addlist(&list, name) == -1) {
					LP_ERRMSG (ERROR, E_LP_MALLOC);
					done (1);
				}

			} else
				/*
				 * Assume that a break in the
				 * numbers means we're done.
				 */
				break;
	    }

	return (list);
}
