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
usema_t *sema;
usema_t *sema_alive;

int shared_value = 0;

int nthreads = 20;
int rlevel = 100;
int verbose = 0;
int meter = 0;

void kid(int);

main(int argc, char *argv[])
{
	pthread_t kid_handle;
	int parse;
	int i;
	int shared_value_max;

	while ((parse = getopt(argc, argv, "t:r:vm")) != EOF)
		switch (parse) {
		case 't':
			nthreads = atoi(optarg);
			break;
		case 'r':
			rlevel = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'm':
			meter = 1;
			break;
		}

	shared_value_max = nthreads * rlevel * 2;

	/*
	 * Initialize Arena
	 */

	afile = tempnam(NULL, "usemarecurse_pt");

	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	if ((handle = usinit(afile)) == NULL) {
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */
	}

	/*
	 * Create semaphores
	 */

	if ((sema = usnewsema(handle, 1)) == NULL) {
		errprintf(1, "newsema failed\n");
		/* NOTREACHED */
	}

	if ((sema_alive = usnewsema(handle, 1)) == NULL) {
		errprintf(1, "newsema (sema_alive) failed\n");
		/* NOTREACHED */
	}

	if (usctlsema(sema, CS_RECURSIVEON)) {
		errprintf(1, "usctlsema CS_RECURSIVEON failed\n");
		/* NOTREACHED */
	}	

	if (meter)
		usctlsema(sema, CS_METERON);

	if (uspsema(sema) != 1) {
		errprintf(1, "parent uspsema failed\n");
		/* NOTREACHED */
	}

	printf("usemarecurse_pt: %d pthreads with recursion depth of %d\n",
	       nthreads, rlevel);

	/*
	 * Spawn pthreads
	 */

	for (i=1; i<= nthreads; i++) {
		if (pthread_create(&kid_handle, NULL, (void *)kid, (void *)i)) {
			errprintf(1, "pthread_create failed i=%d\n", i);
			/* NOTREACHED */
		}	    
	}

	if (usvsema(sema) != 0) {
		errprintf(1, "parent usvsema failed\n");
		/* NOTREACHED */
	}

	while (1) {
		uspsema(sema_alive);
		if (nthreads == 0)
			break;
		usvsema(sema_alive);
		sleep(2);
	}

	if (shared_value == shared_value_max)
		printf("usemarecurse_pt: PASSED\n");
	else
		printf("usemarecurse_pt: FAILED\n");

	exit(0);
}

void
kid(int id)
{
	int i;
	int local_value = 0;
	int pid = getpid();
	int pthread_id = pthread_self();
	char buf1[50];
	char buf2[50];

	sprintf(buf1,"KID pid %d pthread %d usPsema", pid, pthread_id);
 	sprintf(buf2,"KID pid %d pthread %d usVsema", pid, pthread_id);

	for (i=1; i<= rlevel; i++) {
		if (uspsema(sema) != 1) {
			errprintf(1, "child %d uspsema failed\n", id);
			/* NOTREACHED */
		}

		if (local_value == 0)
			local_value = shared_value;

		shared_value++;
		local_value++;

		if (shared_value != local_value) {
			errprintf(1, "child %d pid = %d:%d (P) FAILED\n",
				  id, pid, pthread_id);
			/* NOTREACHED */
		}

		if (verbose)
			sem_print(sema, stdout, buf1);
	}

	for (i=1; i<= rlevel; i++) {

		shared_value++;
		local_value++;

		if (shared_value != local_value) {
			errprintf(1, "child %d pid = %d:%d (V) FAILED\n",
				  id, pid, pthread_id);
			/* NOTREACHED */
		}

		if (usvsema(sema) != 0) {
			errprintf(1, "child %d usvsema failed\n", id);
			/* NOTREACHED */
		}

		if (verbose)
			sem_print(sema, stdout, buf2);
	}

	uspsema(sema_alive);
	nthreads--;
	usvsema(sema_alive);

	pthread_exit((void*) NULL);
}
