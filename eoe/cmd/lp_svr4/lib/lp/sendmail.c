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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/sendmail.c,v 1.1 1992/12/14 13:29:19 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* sendmail(user, msg) -- send msg to user's mailbox */

#include "stdio.h"
#include "stdlib.h"

#include "lp.h"

void
#if	defined(__STDC__)
sendmail (
	char *			user,
	char *			msg
)
#else
sendmail (user, msg)
	char			*user,
				*msg;
#endif
{
	FILE			*pfile;

	char			*mailcmd;

	if (isnumber(user))
		return;

	if ((mailcmd = Malloc(strlen(MAIL) + 1 + strlen(user) + 1))) {
		sprintf (mailcmd, "%s %s", MAIL, user);
		if ((pfile = popen(mailcmd, "w"))) {
			(void)fprintf (pfile, "%s\n", msg);
			pclose (pfile);
		}
		Free (mailcmd);
	}
	return;
}
