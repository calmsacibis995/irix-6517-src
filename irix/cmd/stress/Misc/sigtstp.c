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

#ident "$Revision: 1.2 $"
#include "stdlib.h"
#include "unistd.h"
#include "stdio.h"
#include "signal.h"
#include "termios.h"
#include "errno.h"
#include "stdarg.h"
#include "setjmp.h"
#include "wait.h"
#include "sys/stat.h"

/*
 * A process that is a member of an orphaned
 * process group should not ignore SIGTSTP if
 * there is a signal handler installed.
 */

#define SET_TIMEOUT(time) { \
        int     status; \
        signal(SIGALRM, sigalrm); \
        sigalrm_rcvd = 0; \
        alarm(time); \
        status = sigsetjmp(sigenv, 1); \
        if (status == 0) {

#define CLEAR_ALARM } \
        alarm(0); \
        signal(SIGALRM, SIG_DFL); \
        if (sigalrm_rcvd != 0) \
		printf("%s: Timed out.\n", Cmd); \
        }

char *Cmd;

sigjmp_buf sigenv;
int sigalrm_rcvd;

void
sigalrm()
{
	printf("sigalrm\n");
        siglongjmp(sigenv, ++sigalrm_rcvd);
}

int     shup_tstp;

void    
sig_tstp()
{
	printf("sig_tstp\n");
        shup_tstp++;
}

int
main(int argc, char **argv)
{
	pid_t cpid;
	struct sigaction act;
	int stat;

	Cmd = argv[0];

	/* become orphaned process group */
	setsid();
	if ((cpid = fork()) < 0) {
		printf("fork of child failed\n");
		exit(1);
	} else if (cpid == 0) {
		sigemptyset(&act.sa_mask);
		act.sa_handler = sig_tstp;
		act.sa_flags = 0;
		sigaction(SIGTSTP, &act, (struct sigaction *)0);
		SET_TIMEOUT(20)
	        kill(getpid(),SIGTSTP);
		CLEAR_ALARM
		if( ! shup_tstp)
			printf("%s: SIGTSTP not received .\n", Cmd);
		printf("%s: pid %d exit\n", Cmd, getpid());
		exit(0);
	}
	wait(&stat);
	return 0;
}
