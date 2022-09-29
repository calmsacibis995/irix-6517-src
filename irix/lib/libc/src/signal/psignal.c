/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/psignal.c	1.4"

/*
 * Print the name of the signal indicated by "sig", along with the
 * supplied message
 */

#ifdef __STDC__
	#pragma weak psignal = _psignal
#define CONST const
#else
#define CONST
#endif
#include	"synonyms.h"
#include	<signal.h>
#include	<pfmt.h>
#include	<string.h>
#include	<unistd.h>

#define MSG_OFFSET 4		/* Offset of signal messages in libc catalog */

static const char catalog[] = "uxlibc";

void
psignal(int sig, CONST char *s)
{
	register const char *c;
	register int n;

	if (sig < 0 || sig >= NSIG)
		sig = 0;
	c = __gtxt(catalog, sig + MSG_OFFSET, _sys_siglist[sig]);
	n = (int)strlen(s);
	if(n) {
		const char *colon ;

		colon = __gtxt(catalog, 3, ": ");
		(void) write(2, s, (unsigned)n);
		(void) write(2, colon, strlen(colon));
	}
	(void) write(2, c, (unsigned)strlen(c));
	(void) write(2, "\n", 1);
}
