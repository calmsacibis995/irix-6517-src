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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/bsd/RCS/sndrcv.c,v 1.1 1992/12/14 13:25:06 suresh Exp $"
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
#include "lp.h"
#include "msgs.h"
#define WHO_AM_I	I_AM_OZ		/* to get oam.h to unfold */
#include "oam_def.h"
#include "oam.h"
#include "lpd.h"

/*
 * Format and send message to lpsched
 * (die if any errors occur)
 */
/*VARARGS1*/
void
#if defined (__STDC__)
snd_msg(int type, ...)
#else
snd_msg(type, va_alist)
int	type;
va_dcl
#endif
{
	va_list	ap;

#if defined (__STDC__)
	va_start(ap, type);
#else
	va_start(ap);
#endif
	(void)_putmessage (Msg, type, ap);
	va_end(ap);
	if (msend(Msg) == -1) {
		lp_fatal(E_LP_MSEND, NOLOG); 
		/*NOTREACHED*/
	}
}

/*
 * Recieve message from lpsched
 * (die if any errors occur)
 */
void
#if defined (__STDC__)
rcv_msg(int type, ...)
#else
rcv_msg(type, va_alist)
int	type;
va_dcl
#endif
{
	va_list ap;
	int rc;

	if ((rc = mrecv(Msg, MSGMAX)) != type) {
		if (rc == -1)
			lp_fatal(E_LP_MRECV, NOLOG); 
		else
			lp_fatal(E_LP_BADREPLY, NOLOG, rc); 
		/*NOTREACHED*/
	}
#if defined (__STDC__)
	va_start(ap, type);
#else
	va_start(ap);
#endif
	rc = _getmessage(Msg, type, ap);
	va_end(ap);
	if (rc < 0) {
		lp_fatal(E_LP_GETMSG, NOLOG, PERROR); 
		/*NOTREACHED*/ 
	} 
}
