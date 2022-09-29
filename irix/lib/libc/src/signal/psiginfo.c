/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/psiginfo.c	1.6"

/*
 * Print the name of the siginfo indicated by "sig", along with the
 * supplied message
 */

#ifdef __STDC__
	#pragma weak psiginfo = _psiginfo
#define CONST const
#else
#define CONST
#endif
#include	"synonyms.h"
#include	<signal.h>
#include	<siginfo.h>
#include	<pfmt.h>
#include	<string.h>
#include	<unistd.h>
#include	<stdio.h>
#include 	"sig_extern.h"

#define MSG_OFFSET 788	   /* Offset of signal messages in uxsgicore catalog */

static const char catalog[] = "uxsgicore";

void
psiginfo(siginfo_t *sip, const char *s)
{
	char buf[16];
	register const char *c;
	struct siginfolist *listp;
	const char *colon;

	if (sip == 0)
		return;

	(void) write(2, s, (unsigned)strlen(s));
	colon = __gtxt(catalog, 787, ": ");
	(void) write(2, colon, strlen(colon));
	c = __gtxt(catalog, sip->si_signo + MSG_OFFSET, 
			_sys_siglist[sip->si_signo]);
	(void) write(2, c, (unsigned)strlen(c));
	if (SI_FROMUSER(sip)) {
		c = __gtxt(catalog, 888, " ( from process %d )");
		(void) sprintf(buf, c, sip->si_pid);
		(void) write(2, buf, (unsigned)strlen(buf));
	}
	else if ((listp = &_sys_siginfolist[sip->si_signo-1]) 
	  && sip->si_code > 0 && sip->si_code <= listp->nsiginfo) {
		c = __gtxt(catalog, _siginfo_msg_offset[sip->si_signo-1]
			+ sip->si_code - 1, listp->vsiginfo[sip->si_code-1]);
		(void) write(2, " (", 2);
		switch (sip->si_signo) {
			case SIGSEGV:
			case SIGBUS:
			case SIGILL:
			case SIGFPE:
				(void) sprintf(buf," [%p] ",sip->si_addr);
				(void) write(2, buf, (unsigned)strlen(buf));
				break;
		}
		(void) write(2, c, (unsigned)strlen(c));
		(void) write(2, ")", 1);
	}
	(void) write(2, "\n", 1);
}
