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

#ident "$Revision: 1.3 $"

#include <stdlib.h>
#include <unistd.h>
#include "stdio.h"
#include "sys/types.h"
#include "ulocks.h"

/*
 * test out execing oneself
 */

int
main(int argc, char **argv)
{
	char cloops[10];
	register int i, nloops, nprocs, maxloops;
	pid_t pid;

	if (argc < 3) {
		fprintf(stderr, "Usage:%s nloops nprocs\n", argv[0]);
		exit(-1);
	} else if (argc == 3) {
		/* the origional */
		nprocs = atoi(argv[2]);
		nloops = atoi(argv[1]);
		printf(" forking %d processes and execing myself %d times\n",
			nprocs, nloops);
		for (i = 0; i < nprocs-1; i++) {
			if ((pid = fork()) < 0) {
				perror("fork");
				exit(-2);
			} else if (pid == 0) {
				/* child */
				break;
			}
		}

		/* args: loop_num nloops nprocs */
		execlp(argv[0], argv[0], "1", argv[1], argv[2], NULL);
		perror("1st execlp failed");
	} else {
		/* an execed program */
		nloops = atoi(argv[1]) + 1;
		maxloops = atoi(argv[2]);
		if (nloops > maxloops)
			exit(0);
		if ((nloops % 4) == 0)
			printf("pid:%d loop:%d\n", get_pid(), nloops);
		sprintf(cloops, "%d", nloops);
		execlp(argv[0], argv[0], cloops, argv[2], argv[3], NULL);
		perror("execlp failed");
	}
	return 0;
}
