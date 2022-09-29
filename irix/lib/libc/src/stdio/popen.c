/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/popen.c	1.30"
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak pclose = _pclose
	#pragma weak popen = _popen
#endif
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <wait.h>
#include <paths.h>

#define	tst(a,b) (*mode == 'r'? (b) : (a))
#define	RDR	0
#define	WTR	1

#define BIN_SH _PATH_BSHELL
#define SH "sh"
#define SHFLG "-c"
static pid_t *popen_pid _INITBSS;
static int popen_pid_max  _INITBSS;

FILE *
popen(const char *cmd, const char *mode)
{
	int	p[2];
	register int myside, yourside;
	pid_t pid;

	if (pipe(p) < 0)
		return(NULL);
	myside = tst(p[WTR], p[RDR]);
	yourside = tst(p[RDR], p[WTR]);

	if (myside >= popen_pid_max) {
		LOCKDECLINIT(l, LOCKOPEN);
		if ((popen_pid = realloc(popen_pid,
				(myside + 1) * sizeof(pid_t))) == NULL) {
			UNLOCKOPEN(l);
			close(p[0]);
			close(p[1]);
			return(NULL);
		}
		memset(popen_pid + popen_pid_max, '\0',
				(myside + 1 - popen_pid_max) * sizeof(pid_t));
		/*
	 	 * Adding +1 to popen_pid_max so the next possible iteration 
		 * through popen() won't trash the last value in the array
		 * popen_pid.  This value was set from the previous call to
		 * popen().
		 */
		popen_pid_max = myside + 1;
		UNLOCKOPEN(l);
	}
	if((pid = fork()) == 0) {
		/* myside and yourside reverse roles in child */
		int	i, stdio;

		/* close all pipes from other popen's */
		for (i = 0; i < popen_pid_max; i++)
			if (popen_pid[i])
				close(i);
		stdio = tst(0, 1);
		(void) close(myside);
		if(yourside != stdio) {
			(void) close(stdio);
			(void) fcntl(yourside, F_DUPFD, stdio);
			(void) close(yourside);
		}
		(void) execl(BIN_SH, SH, SHFLG, cmd, (char *)0);
		_exit(127);     /* POSIX.2 value */
	}
	if (pid == -1) {
		close(p[0]);
		close(p[1]);
		return(NULL);
	}
	popen_pid[myside] = pid;
	(void) close(yourside);
	return(fdopen(myside, mode));
}

int
pclose(FILE *ptr)
{
	register int f;
	pid_t p;
	int status = 0;

	if (!popen_pid)
		return -1;
	f = fileno(ptr);
	(void) fclose(ptr);
	/* grab pid and mark closed before waiting - this makes
	 * it unnecessary to single thread this stuff
	 */
	p = popen_pid[f];
	/* mark this pipe closed */
	popen_pid[f] = 0;

	while (waitpid(p, &status, 0) < 0)
		if (errno != EINTR) {
			status = -1;
			break;
		}

	return(status);
}
