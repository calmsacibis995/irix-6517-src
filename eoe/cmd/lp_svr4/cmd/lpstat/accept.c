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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpstat/RCS/accept.c,v 1.1 1992/12/14 11:35:15 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "stdio.h"

#include "lp.h"
#include "class.h"
#include "msgs.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"

#if	defined(__STDC__)
char *retmsg(int, long int);
char *check_reason(char *);
#else
char *retmsg();
char *check_reason();
#endif

/**
 ** do_accept()
 **/

void
#if	defined(__STDC__)
do_accept (
	char **			list
)
#else
do_accept (list)
	char			**list;
#endif
{
	while (*list) {
		if (STREQU(*list, NAME_ALL)) {
			send_message (S_INQUIRE_CLASS, "");
			(void)output (R_INQUIRE_CLASS);
			send_message (S_INQUIRE_PRINTER_STATUS, "");
			(void)output (R_INQUIRE_PRINTER_STATUS);

		} else if (isclass(*list)) {
			send_message (S_INQUIRE_CLASS, *list);
			(void)output (R_INQUIRE_CLASS);

		} else {
			send_message (S_INQUIRE_PRINTER_STATUS, *list);
			switch (output(R_INQUIRE_PRINTER_STATUS)) {
			case MNODEST:
				LP_ERRMSG1 (ERROR, E_LP_BADDEST, *list);
				exit_rc = 1;
				break;
			}

		}
		list++;
	}
	return;
}

/**
 ** putqline()
 **/

void
#if	defined(__STDC__)
putqline (
	char *			dest,
	int			rejecting,
	char *			reject_date,
	char *			reject_reason
)
#else
putqline (dest, rejecting, reject_date, reject_reason)
	char			*dest;
	int			rejecting;
	char			*reject_date,
				*reject_reason;
#endif
{
	if (!rejecting)
            LP_OUTMSG2(INFO, E_STAT_ACCEPTING, dest, reject_date);
	else
            LP_OUTMSG3(INFO, E_STAT_NOTACCEPTING, dest,
			reject_date,
			check_reason(reject_reason)
		);
	return;
}

char *
check_reason(reason)
char	*reason;
{
	if (strcmp(reason, CUZ_NEW_PRINTER) == 0)
		return(retmsg(E_STAT_NEWPRINTER));
	else if (strcmp(reason, CUZ_NEW_DEST) == 0)
		return(retmsg(E_STAT_NODEST));
	else if (strcmp(reason, CUZ_STOPPED) == 0)
		return(retmsg(E_STAT_PSTOPPED));
	else if (strcmp(reason, CUZ_FAULT) == 0)
		return(retmsg(E_STAT_PFAULT));
	else if (strcmp(reason, CUZ_LOGIN_PRINTER) == 0)
		return(retmsg(E_STAT_LOGPRINTER));
	else if (strcmp(reason, CUZ_MOUNTING) == 0)
		return(retmsg(E_STAT_MOUNTFORM));
	else if (strcmp(reason, CUZ_NOFORK) == 0)
		return(retmsg(E_STAT_NOFORK));
	else if (strcmp(reason, CUZ_NOREMOTE) == 0)
		return(retmsg(E_STAT_NOREMOTE));
	else if (strcmp(reason, "unknown reason") == 0)
		return(retmsg(E_STAT_UNKNWNREASN));
	else
		return(reason);
}
