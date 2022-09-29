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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpstat/RCS/device.c,v 1.1 1992/12/14 11:35:37 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "sys/types.h"
#include "string.h"

#include "lp.h"
#include "printers.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"


#if	defined(__STDC__)
static void		putdline ( PRINTER * );
#else
static void		putdline();
#endif

/**
 ** do_device()
 **/

void
#if	defined(__STDC__)
do_device (
	char **			list
)
#else
do_device (list)
	char			**list;
#endif
{
	register PRINTER	*pp;


	while (*list) {
		if (STREQU(NAME_ALL, *list))
			while ((pp = getprinter(NAME_ALL)))
				putdline (pp);

		else if ((pp = getprinter(*list)))
			putdline (pp);

		else {
			LP_ERRMSG1 (ERROR, E_LP_NOPRINTER, *list);
			exit_rc = 1;
		}

		list++;
	}
	return;
}

/**
 ** putdline()
 **/

static void
#if	defined(__STDC__)
putdline (
	PRINTER *		pp
)
#else
putdline (pp)
	register PRINTER	*pp;
#endif
{
        char *			msg;
        char *			retmsg(int, long int);

	if (!pp->device && !pp->dial_info && !pp->remote) {
		LP_ERRMSG1 (ERROR, E_LP_PGONE, pp->name);

	} else if (pp->remote) {
		char *			cp = strchr(pp->remote, BANG_C);


		if (cp)
			*cp++ = 0;

		if (cp)
                   LP_OUTMSG3(INFO, E_STAT_SYSAS, pp->name, pp->remote, cp);
                else
                   LP_OUTMSG2(INFO, E_STAT_SYS, pp->name, pp->remote);

	} else if (pp->dial_info) {
             if (pp->device)
                LP_OUTMSG3(INFO, E_STAT_TOKENON, pp->name, pp->dial_info,
		                                           pp->device);
	     else
                LP_OUTMSG2(INFO, E_STAT_TOKEN, pp->name, pp->dial_info);

	} else {
		LP_OUTMSG2(INFO, E_STAT_DEVICE, pp->name, pp->device);
#if	defined(CAN_DO_MODULES)
		if (verbosity & V_MODULES)
			if (
				emptylist(pp->modules)
			     || STREQU(NAME_NONE, pp->modules[0])
			)
			  {
				msg = retmsg(E_STAT_NOMOD);
				(void)printf (msg);
			  }
			else if (STREQU(NAME_KEEP, pp->modules[0]))
			  {
				msg = retmsg(E_STAT_KEEP);
				(void)printf (msg);
			  }
			else if (STREQU(NAME_DEFAULT, pp->modules[0]))
			  {
				msg = retmsg(E_STAT_DEF);
				(void)printf (msg,  DEFMODULES);
			  }
			else {
				(void)printf (" ");
				printlist_setup ("", 0, ",", "");
				printlist (stdout, pp->modules);
				printlist_unsetup ();
			}
#endif
		(void)printf ("\n");
	}

	return;
}
