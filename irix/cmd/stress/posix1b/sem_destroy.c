#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sys/schedctl.h>
#include <ulocks.h>

#define CHILD_VALUE 5

sem_t unnamed_sema1;
sem_t unnamed_sema2;

int error = 0;
volatile int child_value = CHILD_VALUE;

void child();

main(int argc, char *argv[])
{
	int x;
	int verbose = 0;

	if (argc > 1)
		verbose = 1;

	if (sem_init(&unnamed_sema1, 0, 0)) {
		perror("FAILED: sem_init");
		exit(1);
	}

	if (sem_init(&unnamed_sema2, 0, 1)) {
		perror("FAILED: sem_init 2");
		exit(1);
	}

	for (x=CHILD_VALUE; x > 0; x--) {
		if (sproc(child, PR_SALL) == -1) {
			perror("FAILED: sproc failed");
			exit(1);
		}
	}

	sleep(1);

	while (sem_destroy(&unnamed_sema1)) {
		if (verbose)
			printf ("busy posting...\n");
		sem_post(&unnamed_sema1);
	}

	while(child_value)
		sleep(1);

	if (!error)
		printf("sem_destroy: PASSED\n");

	exit(0);
}

void
child()
{
	if (sem_wait(&unnamed_sema1) < 0) {
		printf("FAILED: child sem_wait operation failed\n");
		error++;
	}

	if (sem_wait(&unnamed_sema2) < 0) {
		printf("FAILED: child sem_wait operation failed - 2\n");
		error++;
	}

	child_value--;

	if (sem_post(&unnamed_sema2) < 0) {
		printf("FAILED: child sem_post operation failed\n");
		error++;
	}

	exit(0);
}
