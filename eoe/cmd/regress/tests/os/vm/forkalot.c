#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/signal.h>

/*
 * Unit test for anon manager tree pruning code.  This code kicks in when
 * a process forks faster than its children exit or exec.  This test
 * sets up this situation to test the pruning code.
 *
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int Dbg = 0;
int Iterations = 10000;

int pagesize;

main(int argc, char *argv[])
{
	int c;
	int errflg = 0;

	while ((c = getopt(argc, argv, "vdi:n:")) != -1)
		switch (c) {
		case 'v':
			Verbose++;
			break;
		case 'd':
			Dbg++;
			break;
		case 'i':
			Iterations = atoi(optarg);
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, 
"Usage: %s [-v] [-d] [-i iterations] \n", argv[0]);
		exit(1);
	}

	pagesize = getpagesize();

	/*
	 * Do the tests
	 */

	if (stress_tests() == FAIL) {
		printf("\n*** STRESS TESTS FAILED ***\n\n");
		exit(1);
	}

	printf("%s: All tests passed\n", argv[0]);
	exit(0);
}


stress_tests()
{
	int errors = 0;
	int i;

	sigset(SIGCHLD, SIG_IGN);

	Message("Beginning Stress Tests...\n");

	for (i = 0; i < Iterations; i++) {

		if (fork() == 0)
			child();
	}

	if (Dbg) {
		Debug("forks complete, pausing...\n");
		pause();
	}

	if (errors)
		return FAIL;

	return PASS;
}


/*
 * 
 */

#define SIZE	(50 * pagesize)

child()
{
	char *arena, *end;

	arena = (char *) malloc(SIZE);
	end = arena + SIZE;

	for (; arena < end; arena += pagesize)
		*arena = 35;

	exit(0);
}
