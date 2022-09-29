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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/bsd/RCS/rsendjob.c,v 1.1 1992/12/14 13:25:03 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#if defined(__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "msgs.h"
#include "lpd.h"

extern MESG	*lp_Md;

/*
 * Package and send R_SEND_JOB to lpexec
 */
void
#if defined (__STDC__)
r_send_job(int status, char *msg)
#else
r_send_job(status, msg)
int	 status;
char	*msg;
#endif
{
	if (!msg)			/* overly cautious! */
		status = MTRANSMITERR;
	(void)mputm(lp_Md, R_SEND_JOB, Rhost, status, msg ? msize(msg) : 0, msg);
}
