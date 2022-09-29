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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/bsd/RCS/log.c,v 1.1 1992/12/14 13:24:50 suresh Exp $"
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
#include "logMgmt.h"
#include "lpd.h"

/*
 * Format and log message
 */
/*VARARGS2*/
void
#if defined (__STDC__)
logit(int type, char *msg, ...)
#else
logit(type, msg, va_alist)
int	 type;
char	*msg;
va_dcl
#endif
{
	va_list		 argp;
	static char	*buf;

	if (!(type & LOG_MASK))
		return;
	/*
	 * Use special buffer for log activity to avoid overwriting
	 * general buffer, Buf[], which may otherwise, be in use.
	 */
	if (!buf && !(buf = (char *)malloc(LOGBUFSZ)))
		return;			/* We're in trouble */
#if defined (__STDC__)
	va_start(argp, msg);
#else
	va_start(argp);
#endif
	(void)vsprintf(buf, msg, argp);
	va_end(argp);
	WriteLogMsg(buf);
}
