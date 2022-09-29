#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h> 
#include <signal.h> 
#include <ulocks.h>
#include <errno.h>
#include <getopt.h>
#include <wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/usync.h>
#include "stress.h"

char *afile;
usptr_t *handle;
ulock_t *lock_a;
ulock_t *lock_b;

int share_me = 0;

int iterations = 5000;
int verbose = 0;

void kid(int);

main(int argc, char *argv[])
{
	pthread_t kid_handle;
	int parse;
	int x;

	while ((parse = getopt(argc, argv, "i:v")) != EOF)
		switch (parse) {
		case 'r':
			iterations = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		}

	/*
	 * Initialize Arena
	 */

	afile = tempnam(NULL, "locktest_pt");

	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	if ((handle = usinit(afile)) == NULL) {
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */
	}

	/*
	 * Create locks
	 */

	if ((lock_a = usnewlock(handle)) == NULL) {
		errprintf(1, "usnewlock (lock_a) failed\n");
		/* NOTREACHED */
	}

	if ((lock_b = usnewlock(handle)) == NULL) {
		errprintf(1, "usnewlock (lock_b) failed\n");
		/* NOTREACHED */
	}

	/*
	 * Grab both locks
	 */

	if (ussetlock(lock_a) != 1) {
		errprintf(1, "parent ussetlock (lock_a) failed\n");
		/* NOTREACHED */
	}

	if (ussetlock(lock_b) != 1) {
		errprintf(1, "parent ussetlock (lock_b) failed\n");
		/* NOTREACHED */
	}

	printf("locktest_pt: %d iterations...\n", iterations);

	/*
	 * Spawn child pthread
	 */

	if (pthread_create(&kid_handle, NULL, (void *)kid, (void *)0)) {
		errprintf(1, "pthread_create failed\n");
		/* NOTREACHED */
	}	    

	sleep(1);

	/*
	 * Run producer routine
	 */
	for (x=0; x < iterations; x++) {
		share_me++;
		if (usunsetlock(lock_a) != 0)
			errprintf(ERR_ERRNO_EXIT,
			"parent spin_unlock failed on lock1 pass %d", x);

		if (ussetlock(lock_b) != 1)
			errprintf(ERR_ERRNO_EXIT,
			"parent spin_lock failed on lock2 pass %d", x);
	}

	printf("locktest_pt: PASSED\n");
	exit(0);
}

void
kid(int id)
{
	int x;
	int ret;
	int y = 1;

	/*
	 * Run consumer routine
	 */

	for (x=0; x < iterations; x++) {
	 	if (ussetlock(lock_a) != 1)
			errprintf(ERR_ERRNO_EXIT,
			"child ussetlock failed on lock_a pass %d", x);

		if (verbose)
			printf("%d\n", share_me);

		if (y++ != share_me)
			errprintf(ERR_EXIT,
				  "failed: shared data is corrupt at pass %d",
				  x);

		if (usunsetlock(lock_b) != 0)
			errprintf(ERR_ERRNO_EXIT,
			"child spin_unlock failed on lock2 pass %d", x);
	}

	usunsetlock(lock_a);
}
