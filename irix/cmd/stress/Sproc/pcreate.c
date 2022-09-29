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

#ident "$Revision: 1.4 $"

#include "stdlib.h"
#include "unistd.h"
#include "stdio.h"
#include "malloc.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "sys/wait.h"
#include "errno.h"
#include "task.h"

char *gargv[10];
char *Cmd;
/*
 * test out pcreate - which tests bloc on sproc/unblock on exec
 */

int
main(int argc, char **argv)
{
	char cloops[10];
	int *pids;
	register int i, nprocs;
	pid_t pid;
	auto int status;

	Cmd = argv[0];
	for (i = 0; i < 10; i++)
		gargv[i] = argv[i];
	gargv[9] = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage:%s nprocs\n", argv[0]);
		exit(-1);
	}
	if (argc == 2) {
		nprocs = atoi(argv[1]);
		if (nprocs > (int)prctl(PR_MAXPROCS))
			fprintf(stderr, "%s:nprocs:%d too large; max:%d\n",
				Cmd, nprocs, prctl(PR_MAXPROCS));
		pids = (int *)malloc(nprocs * sizeof(int));

		/* the origional */
		printf("pcreate %d processes\n", nprocs);
		for (i = 0; i < nprocs; i++) {
			sprintf(cloops, "%d", i);
			/* change arg each time to be sure that each process
			 * gets correct args
			 */
			/* args: name nprocs pnum */
			gargv[2] = cloops;
			gargv[3] = NULL;
			pids[i] = pcreatev(argv[0], gargv);
			if (pids[i] < 0) {
				perror("pcreate");
				exit(-2);
			}
		}
		/* wait for all processes and check there exit status */
		while ((pid = wait(&status)) >= 0) {
			if (status & 0xff) {
				fprintf(stderr,
					"%s:ERROR:process %d exitted abnormally:%x\n",
					Cmd, pid, status);
				continue;
			}
			/* search for pid in array */
			for (i = 0; i < nprocs; i++) {
				if (pids[i] == pid) {
					/* found it make sure exit status ok */
					if ((status >> 8) != i)
						fprintf(stderr,
						 "%s:ERROR:wrong status:%d wanted:%d\n",
						 Cmd, status >> 8, i);
					break;
				}
			}
			if (i >= nprocs)
				fprintf(stderr, "%s:ERROR:process %d not in list!\n",
					Cmd, pid);
		}
		if (errno != ECHILD)
			perror("pcreate:ERROR:wait");

	} else {
		/* a pcreate'd program */
		exit(atoi(argv[2]));
	}
	return 0;
}
