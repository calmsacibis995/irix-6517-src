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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpstat/RCS/form.c,v 1.1 1992/12/14 11:35:50 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "stdio.h"

#include "string.h"

#include "lp.h"
#include "form.h"
#include "access.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"


#if	defined(__STDC__)
static void		putfline ( FORM * );
#else
static void		putfline();
#endif

/**
 ** do_form()
 **/

void
#if	defined(__STDC__)
do_form (
	char **			list
)
#else
do_form (list)
	char			**list;
#endif
{
	FORM			form;

	while (*list) {
		if (STREQU(NAME_ALL, *list))
			while (getform(NAME_ALL, &form, (FALERT *)0, (FILE **)0) != -1)
				putfline (&form);

		else if (getform(*list, &form, (FALERT *)0, (FILE **)0) != -1) {
			putfline (&form);

		} else {
			LP_ERRMSG1 (ERROR, E_LP_NOFORM, *list);
			exit_rc = 1;
		}

		list++;
	}
	printsdn_unsetup ();
	return;
}

/**
 ** putfline()
 **/

static void
#if	defined(__STDC__)
putfline (
	FORM *			pf
)
#else
putfline (pf)
	FORM			*pf;
#endif
{
	register MOUNTED	*pm;

        char 			*msg;
	char			*retmsg(int, long int);

	if (is_user_allowed_form(getname(), pf->name))
 	   msg = retmsg(E_STAT_FORMAVAIL);
	else
	   msg = retmsg(E_STAT_FORMNOTAV);

	pfmt(stdout, INFO, NULL);
	(void) printf(msg, pf->name);

	for (pm = mounted_forms; pm->forward; pm = pm->forward)
		if (STREQU(pm->name, pf->name)) {
			if (pm->printers) {
				msg = retmsg(E_STAT_FM);
				(void) printf(msg);
				printlist_setup (0, 0, ",", "");
				printlist (stdout, pm->printers);
				printlist_unsetup();
			}
			break;
		}

	(void) printf("\n");

	if (verbosity & V_LONG) {

		msg = retmsg(E_STAT_PL);
		printsdn_setup (msg, 0, 0);
		printsdn (stdout, pf->plen);

		msg = retmsg(E_STAT_PW);
		printsdn_setup (msg, 0, 0);
		printsdn (stdout, pf->pwid);

		msg = retmsg(E_STAT_NOP);
		(void) printf(msg, pf->np);

		msg = retmsg(E_STAT_LPITCH);
		printsdn_setup (msg, 0, 0);
		printsdn (stdout, pf->lpi);

		msg = retmsg(E_STAT_CHARPITCH);
		(void) printf(msg);
		if (pf->cpi.val == N_COMPRESSED)
			(void) printf(" %s\n", NAME_COMPRESSED);
		else {
			printsdn_setup (" ", 0, 0);
			printsdn (stdout, pf->cpi);
		}

		msg = retmsg(E_STAT_CMAN);
		LP_OUTMSG2(INFO|MM_NOSTD, E_STAT_SETCHOICE,
			(pf->chset? pf->chset : NAME_ANY),
			(pf->mandatory ? msg : "")
		);

		LP_OUTMSG1(INFO|MM_NOSTD, E_STAT_RIBCOL,
			(pf->rcolor? pf->rcolor : NAME_ANY)
		);

		if (pf->comment)
		        LP_OUTMSG1(INFO|MM_NOSTD, E_STAT_COMMENT,
			                                    pf->comment);

	}
	return;
}

