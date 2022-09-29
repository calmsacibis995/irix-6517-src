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

#ident "$Revision: 1.3 $"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "signal.h"
#include "termios.h"
#include "errno.h"
#include "stdarg.h"
#include "setjmp.h"
#include "wait.h"
#include "sys/stat.h"
#include "stress.h"
#include "sys/types.h"
#include "sys/prctl.h"

/*
 * If the exit of the process cause a process group to become orphaned,
 * and if any member of the newly-orphaned process group is stopped,
 * then a SIGHUP signal followed by a SIGCONT signal shall be sent to
 * each process in the newly-orphaned process group.
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


int     shup_cnt1;

void    
sig_hup1()
{
	printf("sig_hup1\n");
        shup_cnt1++;
}

int     sigcont_rcvd1;

void    
sig_cont1()
{
	printf("sig_cont1\n");
        sigcont_rcvd1++;
}

int
main(int argc, char **argv)
{
	pid_t fpid, ppid, spid, cspid, wpid;
	struct sigaction act1, act2;
	int stat;

	Cmd = errinit(argv[0]);

	ppid = getpid();
	if ((fpid = fork()) < 0) {
		errprintf(3, "fork of first group failed");
		exit(1);
	} else if (fpid == 0) {
		/* make us a process group */
		if (setpgid(0, 0) != 0) {
			errprintf(1, "setpgid failed");
			/* NOTREACHED */
		}
	        kill(getpid(),SIGSTOP);
		printf("%s: pid %d exit\n", Cmd, getpid());
		exit(0);
	}
	wpid = waitpid(fpid,&stat,WUNTRACED);
	if (WIFSTOPPED(stat))
		printf("%d is stopped, sig %d\n",
			wpid, WSTOPSIG(stat));
	else {
		printf("%d didn't stop, exiting\n", wpid);
		goto doerr;
	}
	if ((spid = fork()) < 0) {
		errprintf(3, "fork of second group failed");
		goto doerr;
	} else if (spid == 0) {
		if ((cspid = fork()) < 0) {
			errprintf(3, "fork of child of second group failed");
		} else if (cspid == 0) {

			sigemptyset(&act1.sa_mask);
			act1.sa_handler = sig_hup1;
			act1.sa_flags = 0;
			sigaction(SIGHUP, &act1, (struct sigaction *)0);
			sigemptyset(&act2.sa_mask);
			act2.sa_handler = sig_cont1;
			act2.sa_flags = 0;
			sigaction(SIGCONT, &act2, (struct sigaction *)0);
	
			/* change process group to first group */

			if (setpgid(0, fpid) != 0) {
				errprintf(3, "setpgid to first group failed");
				goto doerr;
			}

			unblockproc(getppid());
			/* send SIGSTOP to itself */

			SET_TIMEOUT(20)
			if (kill(getpid(), SIGSTOP) != 0) {
				if (oserror() != ESRCH)
					errprintf(3, "kill STOP failed");
				goto doerr;
			}
			CLEAR_ALARM

			/* should get SIGHUP and SIGCONT */

			if( ! shup_cnt1)
				printf("%s: SIGHUP not received by cspid.\n", Cmd);
			if( ! sigcont_rcvd1)
				printf("%s: SIGCONT not received by cspid\n.", Cmd);
			printf("%s: pid %d exit\n", Cmd, getpid());
			exit(0);
		}
	
		/* second group */
		blockproc(getpid());
		printf("%s: pid %d exit\n", Cmd, getpid());
		exit(0);
	}

	/* parent */
	wait(&stat);
	kill(fpid, SIGCONT);
	wait(&stat);
	printf("%s: pid %d exit\n", Cmd, getpid());
	return 0;
doerr:
	if (ppid)
		kill(ppid, SIGKILL);
	if (fpid)
		kill(fpid, SIGKILL);
	if (spid)
		kill(spid, SIGKILL);
	if (cspid)
		kill(cspid, SIGKILL);

	abort();
	/* NOTREACHED */
}
