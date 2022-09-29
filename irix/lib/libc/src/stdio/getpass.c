/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/getpass.c	1.20"
/*	3.0 SID #	1.4	*/
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak getpass = _getpass
#endif
#include "synonyms.h"
#include <limits.h>
#include <stdio.h>
#include <signal.h>
#include <termio.h>
#include <sys/types.h>
#include <unistd.h>

static void catch(void);
static int intrupt;
/* move out of function scope so we get a global symbol for use with data cording */
static char pbuf[ PASS_MAX + 1 ];

char *
getpass(const char *prompt)
{
	struct termio ttyb;
	tcflag_t flags;
	register char *p;
	register int c;
	FILE	*fi;
	struct	sigaction oldact,newact;

	if((fi = fopen("/dev/tty","r")) == NULL)
		return((char*)NULL);
	setbuf(fi, (char*)NULL);

	intrupt = 0;
	/* fetch user's previous SIGINT handling and mask */
	newact.sa_handler = catch;	/* new handler address */
	sigemptyset(&newact.sa_mask);	/* don't block any signals */
	newact.sa_flags = 0;
	if (sigaction(SIGINT, &newact, &oldact) == -1) {
		if (fi != stdin)
			(void) fclose(fi);
		return((char *)NULL);
	}
			
	(void) ioctl(fileno(fi), TCGETA, &ttyb);
	flags = ttyb.c_lflag;
	ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	(void) ioctl(fileno(fi), TCSETAF, &ttyb);
	(void) fputs(prompt, stderr);
	p = pbuf;
	while( !intrupt  &&
		(c = getc(fi)) != '\n'  &&  c != '\r'  &&  c != EOF ) {
		if(p < &pbuf[ PASS_MAX ])
			*p++ = (char)c;
	}
	*p = '\0';
	ttyb.c_lflag = flags;
	(void) ioctl(fileno(fi), TCSETAW, &ttyb);
	(void) putc('\n', stderr);

	/* reinstall user's previous handler */
	(void) sigaction(SIGINT, &oldact, NULL);
	if (fi != stdin)
		(void) fclose(fi);
	if(intrupt)
		(void) kill(getpid(), SIGINT);
	return(pbuf);
}

static void
catch(void)
{
	++intrupt;
}
