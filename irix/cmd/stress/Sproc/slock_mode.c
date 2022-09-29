/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sync.h>
#include <task.h>
#include <strings.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include "stress.h"

spinlock_t lock1;
spinlock_trace_t lock1_tracer;

char pad[1000];

spinlock_t lock2;
spinlock_trace_t lock2_tracer;

int share_me = 0;

int up_mode = 0;
int global_count = 10000;
int verbose = 0;
int debugmode = 0;
int debugplus = 0;
int priorityq = 0;

extern int _ushlockdefspin;
 
static void child(void *);

main(int argc, char **argv)
{
	int pid;
	int x, ret;
	int parse;
	int i;
	while ((parse = getopt(argc, argv, "udvi:g:pq")) != EOF)
		switch (parse) {
		case 'i':
			global_count = atoi(optarg);
			break;
		case 'u':
			up_mode = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'd':
			debugmode = 1;
			break;
		case 'g':
			_ushlockdefspin = atoi(optarg);
			break;
		case 'p':
			debugplus = 1;
			break;
		case 'q':
			priorityq = 1;
			break;
		default:
			exit(1);
		}
#if 0
	/*
	 * XXX This test has been disabled until the POSIX spinlock
	 * XXX interfaces are officially exported by libc
	 */

	spin_init(&lock1);
	spin_init(&lock2);
	  
	if (up_mode) {
		spin_mode(&lock1, SPIN_MODE_UP);
		spin_mode(&lock2, SPIN_MODE_UP);
		printf("slock_mode: UP lock mode (%d iterations)...\n",
		       global_count);
	} else
		printf("slock_mode: MP lock mode (%d iterations)...\n",
		       global_count);

	if (priorityq) {
		spin_mode(&lock1, SPIN_MODE_QUEUEPRIO);
		spin_mode(&lock2, SPIN_MODE_QUEUEPRIO);
	}

	if (debugmode || debugplus) {
		spin_mode(&lock1, SPIN_MODE_TRACEINIT, &lock1_tracer);
		spin_mode(&lock2, SPIN_MODE_TRACEINIT, &lock2_tracer);

		if (debugmode) {
			spin_mode(&lock1, SPIN_MODE_TRACEON);
			spin_mode(&lock2, SPIN_MODE_TRACEON);
			printf("slock_mode: DEBUG enabled\n");
		}

		if (debugplus) {
			spin_mode(&lock1, SPIN_MODE_TRACEPLUSON);
			spin_mode(&lock2, SPIN_MODE_TRACEPLUSON);
			printf("slock_mode: DEBUG PLUS enabled\n");
		}
	}

	if (verbose) {
		spin_print(&lock1, stdout, "slock_mode: init lock1");
		spin_print(&lock2, stdout, "slock_mode: init lock2");
	}

	/*
	 * Grab both locks
	 */
	if (spin_lock(&lock1))
		errprintf(ERR_ERRNO_EXIT, "first spin_lock failed on lock1");

	if (spin_lock(&lock2))
		errprintf(ERR_ERRNO_EXIT, "first spin_lock failed on lock2");

	/*
	 * Spawn child
	 */
	if (sproc(child, PR_SALL) == -1) {
		perror("sproc failed");
		exit(1);
	}

	sleep(1); 
	
	/*
	 * Run producer routine
	 */
	for (x=0; x < global_count; x++) {
		share_me++;
		if (spin_unlock(&lock1))
			errprintf(ERR_ERRNO_EXIT,
			"parent spin_unlock failed on lock1 pass %d", x);

		if (spin_lock(&lock2))
			errprintf(ERR_ERRNO_EXIT,
			"parent spin_lock failed on lock2 pass %d", x);
	}

	wait(NULL);
	
	if (debugmode || debugplus) {
		spin_print(&lock1, stdout, "slock_mode: trace lock1");
		spin_print(&lock2, stdout, "slock_mode: trace lock2");
	}
#endif
	printf("slock_mode: PASSED\n");
}

/* ARGSUSED */
static void
child(void *a)
{
	int x;
	int ret;
	int y = 1;
#if 0
	/*
	 * XXX This test has been disabled until the POSIX spinlock
	 * XXX interfaces are officially exported by libc
	 */

	/*
	 * Run consumer routine
	 */

	for (x=0; x < global_count; x++) {
	 	if (spin_lock(&lock1))
			errprintf(ERR_ERRNO_EXIT,
			"child spin_lock failed on lock1 pass %d", x);

		if (verbose)
			printf("%d\n", share_me);

		if (y++ != share_me)
			errprintf(ERR_EXIT,
				  "failed: shared data is corrupt at pass %d", x);

		if (spin_unlock(&lock2))
			errprintf(ERR_ERRNO_EXIT,
			"child spin_unlock failed on lock2 pass %d", x);
	}

	spin_unlock(&lock1);
#endif
}
