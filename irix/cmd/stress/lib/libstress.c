/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.7 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "stress.h"
#include "ulocks.h"

int exitonerror = 1;
char *Errcmd = "UNKNOWN";

char *
errinit(char *s)
{
	char *t;

	/* strsave to get it off of parents stack */
	if ((t = strrchr(s, '/')) == NULL)
		Errcmd = strdup(s);
	else
		Errcmd = strdup(t+1);
	setlinebuf(stderr);
	setlinebuf(stdout);
	return(Errcmd);
}

/*
 * errprintf - error printing
 *	err = 0 - print and obey exitonerror flag
 *	    = 1 - print plus errno and obey exitonerror flag
 *	    = 2 - print and return
 *	    = 3 - print plus errno and return
 *
 */
/* VARARGS 2 */
 int
errprintf(int err, char *fmt, ...)
{
	va_list ap;
	char buf[512];

	va_start(ap, fmt);
	strcpy(buf, Errcmd);
	strcat(buf, ":ERROR:");
	vsprintf(&buf[strlen(buf)], fmt, ap);
	if (err == 1 || err == 3) {
		strcat(buf, ":");
		if (strerror(oserror()))
			strcat(buf, strerror(oserror()));
		else
			sprintf(&buf[strlen(buf)], "%d", oserror());
	}
	strcat(buf, "\n");
	fprintf(stderr, "%s", buf);
	/*
	 * tcdrain caused background jobs to get SIGTTOU
	tcdrain(fileno(stderr));
	*/
	if (err == 2 || err == 3)
		return(0);
	if (exitonerror) {
		/* to avoid a race where a child exitting sends a signal
		 * while we're exitting, we ignore signals
		 */
		sigignore(SIGCLD);
		/* XXX some others?? use sigprocmask? */
		abort();
	}
	return(0);
}

 void
wrout(char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	sprintf(buf, "%s:pid %d: ", Errcmd, get_pid());
	vsprintf(&buf[strlen(buf)], fmt, ap);
	strcat(buf, "\n");
	write(2, buf, strlen(buf));
}

/*
 * Handle parent exitting - make sure that children go away.
 * We don't like to use prctl(PR_TERMCHILD) since it requires that
 * SIGHUP be set to default. Instead we usurp SIGUSR1 and use TERMSIG
 */
static int exflags;

static void
sigex(int sig)
{
	if ((exflags & EXONANY) || getppid() == 1)
		_exit(0);
}

void
parentexinit(int flags)
{
	struct sigaction sa;
	sigset_t sm;

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sigex;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);
	/*
	 * in case parent decides to exit - this makes sure that all
	 * kids will be killed
	 * Note that kids will inherit the signal setting and mask
	 *
	 * Parent holds signal - assumption is that parent will
	 * find other ways to determine if child exits..
	 */
	prctl(PR_SETEXITSIG, SIGUSR1);
	sigemptyset(&sm);
	sigaddset(&sm, SIGUSR1);
	sigprocmask(SIG_BLOCK, &sm, NULL);

	exflags = flags;
}

void
slaveexinit(void)
{
	sigset_t sm;

	sigemptyset(&sm);
	sigaddset(&sm, SIGUSR1);
	sigprocmask(SIG_UNBLOCK, &sm, NULL);
}
