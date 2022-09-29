/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.4 $"

#include "unistd.h"
#include "stdlib.h"
#include "wait.h"
#include "stdio.h"
#include "errno.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "signal.h"
#include "task.h"
#include "string.h"

#define MAXPROC 1000
char **gargv;
pid_t ppid;
pid_t cpid;
pid_t gpgrp;
pid_t gpid;
void dounblock(int);
/*
 * Test process group, especial race condition in setsid, setpgid, fork, and
 * exit.
 */

int
main(int argc, char **argv)
{
	extern void slave();
	char cloops[10];
	register int i, j, nloops, nprocs, maxloops;
	pid_t pid[MAXPROC];
	auto int child;
	pid_t pg;

	gargv = argv;
	if (argc < 3) {
		fprintf(stderr, "Usage:%s nloops nprocs\n", argv[0]);
		exit(-1);
	} else if (argc == 3) {
		/* the origional */
		gpid = getpid();
		gpgrp = getpgrp();
		printf("spgrp:gpgrp:%d \n", gpgrp);
		nprocs = atoi(argv[2]);
		nloops = atoi(argv[1]);
		printf("spgrp:sprocing %d processes and execing %d times\n",
			nprocs, nloops);
		usconfig(CONF_INITUSERS, nprocs+1);
		ppid = fork();
		if  (ppid == 0) {
			ppid = getpid();
			setpgid(0, 0);
			for (i = 0; i < nprocs; i++) {
				if ((pid[i] = sproc(slave, PR_SALL)) < 0) {
					perror("spgrp:sproc");
					for (j = 0; j < i; j++)
						kill(pid[j], SIGKILL);
					exit(-2);
				} else if (pid == 0) {
					/* child */
					break;
				}
			}

			/* start up all slaves */
			for (i = 0; i < nprocs; i++) 
				dounblock(pid[i]);

			/* now wait for all slaves to exec then exit */
			for (i = 0; i < nprocs; i++) {
				blockproc(ppid);
				blockproc(gpid);
			}
		} else if (ppid >0) {
			wait(&child);
			printf("spgrp:main:%d exit\n", gpid);
		}
	} else {
		/* an execed program */
		nloops = atoi(argv[1]) + 1;
		maxloops = atoi(argv[2]);
		if (nloops > maxloops) {
			dounblock(atoi(argv[5]));
			dounblock(atoi(argv[3]));
			printf("spgrp:pid:%d exit\n", getpid());
			exit(0);
		}
		cpid = getpid();
		if ((cpid % 2) == 0) {
			if ((nloops %2) == 0) {
				pg = atoi(argv[4]);
			} else {
				pg = getpid();
			}
			if (setpgid(0, pg) < 0) {
				perror("spgrp:setpgid");
				printf("spgrp:pid %d: expect pgrp:%d \n", cpid, pg);
			}
			if (getpgrp() != pg)
				printf("spgrp:ERROR:pid:%d expect pgrp:%d real pgrp:%d\n", cpid, pg, getpgrp());
		} else  {
			printf("spgrp:pid:%d exit\n", cpid);
			dounblock(atoi(argv[5]));
			dounblock(atoi(argv[3]));
			exit(0);
		}
		sprintf(cloops, "%d", nloops);
		/* args: loop_num nloops ppid gpgrp */
		execlp(argv[0], argv[0], cloops, argv[2], argv[3], argv[4],
		argv[5], NULL);
		perror("spgrp:execlp failed");
		dounblock(atoi(argv[5]));
		dounblock(atoi(argv[3]));
	}
	return 0;
}

void
slave()
{
	char parentpid[10];
	char grandppid[10];
	char parentpg[10];

	blockproc(getpid());
	/* args: loop_num nloops nprocs parentpid parentpg grandparentpid*/
	sprintf(parentpid, "%d", ppid);
	sprintf(grandppid, "%d", gpid);
	sprintf(parentpg, "%d", gpgrp);
	execlp(gargv[0], gargv[0], "1", gargv[1], parentpid, parentpg,
	grandppid, NULL);
	perror("spgrp:1st execlp failed");
	printf("spgrp:ppid %s gpid %s gpgrp %s\n", parentpid, grandppid,parentpg);
	dounblock(ppid);
	dounblock(gpid);
	exit(0);
}

void
dounblock(int id)
{
	if (unblockproc(id) != 0) {
		fprintf(stderr, "spgrp:pid %d unblock failed %s\n",
			getpid(), strerror(errno));
		abort();
	}
}
