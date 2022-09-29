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
 * process group should not be allowed to stop in
 * response to the SIGTSTP signal
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

int
main(int argc, char **argv)
{
	pid_t cpid;
	int stat;

	Cmd = argv[0];

	/* become orphaned process group */
	setpgrp();
	if ((cpid = fork()) < 0) {
		printf("fork of child failed\n");
		exit(1);
	} else if (cpid == 0) {
		/* 
		 * ignore signal  SIGHUP and set signal SIGTSTP 
		 * to SIG_DFL 
		 */
		signal(SIGHUP, SIG_IGN);
		signal(SIGTSTP, SIG_DFL);
	        kill(getpid(),SIGTSTP);
		exit(0);
	}
	SET_TIMEOUT(20)
	waitpid(cpid,&stat,WUNTRACED);
	CLEAR_ALARM
		
	if (WIFSTOPPED(stat)) {
 		printf("A process that is a member of an orphaned process "
			"group should not be allowed to stop in response "
			"to the SIGTSTP signal\n");
		kill(cpid,SIGCONT);
		exit(1);
	}
	return 0;
}
