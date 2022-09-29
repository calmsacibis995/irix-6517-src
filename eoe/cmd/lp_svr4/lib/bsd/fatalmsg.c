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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/bsd/RCS/fatalmsg.c,v 1.1 1992/12/14 13:24:36 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <stdio.h>
#include <string.h>
#if defined(__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#define WHO_AM_I	I_AM_OZ		/* to get oam.h to unfold */
#include "oam.h"
#include "lpd.h"

/*
 * Report fatal error and exit
 */
/*VARARGS1*/
void
#if defined (__STDC__)
fatal(char *fmt, ...)
#else
fatal(fmt, va_alist)
char	*fmt;
va_dcl
#endif
{
	va_list	argp;

	if (Rhost)
		(void)printf("%s: ", Lhost);
	printf("%s: ", Name);
	if (Printer)
		(void)printf("%s: ", Printer);
#if defined (__STDC__)
	va_start(argp, fmt);
#else
	va_start(argp);
#endif
	(void)vprintf(fmt, argp);
	va_end(argp);
	putchar('\n');
	fflush(stdout);
	done(1);		/* defined by invoker */
	/*NOTREACHED*/
}

/*
 * Format lp error message to stderr
 * (this will probably change to remain compatible with LP)
 */
/*VARARGS1*/
void
#if defined (__STDC__)
_lp_msg(int seqnum, long msgid, int logind, va_list argp)
#else
_lp_msg(seqnum, msgid, logind, argp)
int     seqnum;
long	msgid;
int     logind;
va_list	argp;
#endif
{
	char	 label[20];

	strcpy(label, "UX:");
	strcat(label, basename(Name));
	fmtmsg(label, 
	       ERROR, 
               seqnum,
               msgid,
               logind,
               argp);
}

/*
 * Format lp error message to stderr
 */
/*VARARGS1*/
void
#if defined (__STDC__)
lp_msg(int seqnum, long msgid, int logind, ...)
#else
lp_msg(seqnum, msgid, logind, va_alist)
int     seqnum;
long	msgid;
int     logind;
va_dcl
#endif
{
	va_list	argp;

#if defined (__STDC__)
	va_start(argp, msgid);
#else
	va_start(argp);
#endif
        _lp_msg(seqnum, msgid, logind, argp);
	va_end(argp);
}

/*
 * Report lp error message to stderr and exit
 */
/*VARARGS1*/
void
#if defined (__STDC__)
lp_fatal(int seqnum, long msgid, int logind, ...)
#else
lp_fatal(seqnum, msgid, logind, va_alist)
int     seqnum;
long	msgid;
int     logind;
va_dcl
#endif
{
	va_list	argp;

#if defined (__STDC__)
	va_start(argp, msgid);
#else
	va_start(argp);
#endif
        _lp_msg(seqnum, msgid, logind, argp);
	va_end(argp);

	done(1);			/* Supplied by caller */
	/*NOTREACHED*/
}
