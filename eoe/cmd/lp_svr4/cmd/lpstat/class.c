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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpstat/RCS/class.c,v 1.1 1992/12/14 11:35:34 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "stdio.h"

#include "lp.h"
#include "class.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"


#if	defined(__STDC__)
static void		putcline ( CLASS * );
#else
static void		putcline();
#endif

/**
 ** do_class()
 **/

void
#if	defined(__STDC__)
do_class (
	char **			list
)
#else
do_class (list)
	char			**list;
#endif
{
	register CLASS		*pc;

	printlist_setup ("\t", 0, 0, 0);
	while (*list) {
		if (STREQU(NAME_ALL, *list))
			while ((pc = getclass(NAME_ALL)))
				putcline (pc);

		else if ((pc = getclass(*list)))
			putcline (pc);

		else {
			LP_ERRMSG1 (ERROR, E_LP_NOCLASS, *list);
			exit_rc = 1;
		}
		list++;
	}
	printlist_unsetup ();
	return;
}

/**
 ** putcline()
 **/

static void
#if	defined(__STDC__)
putcline (
	CLASS *			pc
)
#else
putcline (pc)
	register CLASS		*pc;
#endif
{
                LP_OUTMSG1(INFO, E_STAT_MEMCLASS, pc->name);
		printlist (stdout, pc->members);
	return;
}
