#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <sys/lock.h>

/*
 * Unit test for plock.  This should be compiled for both coff and elf
 * to make both the old and new versions of plock work correctly.
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int Dbg = 0;
int Iterations = 5;
int Pause = 0;

int do_plock(int, int);

main(int argc, char *argv[])
{
	int c;
	int errflg = 0;

	while ((c = getopt(argc, argv, "vdi:p")) != -1)
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
		case 'p':
			Pause = 1;
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, "Usage: plock [-v] [-d] [-i <# itertaions>] [-p]\n");
		exit(1);
	}


	while (Iterations--) {
		if (Unit_tests() == FAIL) {
			printf("\n*** UNIT TESTS FAILED ***\n\n");
			exit(1);
		}
	}

	printf("plock: All tests passed\n");
	exit(0);
}

int
do_plock(int op, int expect)
{
	int rval;

	rval = plock(op);

	if (expect) {
		if (rval != -1) {
			fprintf(stderr, "plock(0x%x) succeeded when expected to fail with errno %d\n",
				op, expect);
			return 1;
		}

		if (errno != expect) {
			fprintf(stderr, "plock(0x%x) failed with errno %d when errno %d was expected\n",
				op, errno, expect);
			return 1;
		}

		return 0;	/* test failed as expected */

	} else {
		if (rval == -1) {
			fprintf(stderr, "plock(0x%x) unexpectedly failed with errno %d\n",
				op, errno);
			return 1;
		}

		return 0;
	}
}


int
Unit_tests()
{
	int errors = 0;

	Message("Try each type of lock once. These should all succeed...\n");
	errors += do_plock(PROCLOCK, 0);
	errors += do_plock(UNLOCK, 0);
	errors += do_plock(TXTLOCK, 0);
	errors += do_plock(UNLOCK, 0);
	errors += do_plock(DATLOCK, 0);
	errors += do_plock(UNLOCK, 0);

	Message("Make sure we can add a TXTLOCK after a DATLOCK...\n");
	errors += do_plock(DATLOCK, 0);
	errors += do_plock(TXTLOCK, 0);
	errors += do_plock(UNLOCK, 0);

	Message("Make sure we can add a DATLOCK after a TXTLOCK...\n");
	errors += do_plock(TXTLOCK, 0);
	errors += do_plock(DATLOCK, 0);
	errors += do_plock(UNLOCK, 0);

	/*
	 * Try error cases
	 */

	Message("Error cases...\n");
	Message("Unlocking when no locks present\n");
	errors += do_plock(UNLOCK, EINVAL);

	Message("Adding TXTLOCK, DATLOCK, PROCLOCK after PROCLOCK already exists\n");
	errors += do_plock(PROCLOCK, 0);
	errors += do_plock(TXTLOCK, EINVAL);
	errors += do_plock(DATLOCK, EINVAL);
	errors += do_plock(PROCLOCK, EINVAL);
	errors += do_plock(UNLOCK, 0);

	Message("Adding TXTLOCK, PROCLOCK after TXTLOCK already exists\n");
	errors += do_plock(TXTLOCK, 0);
	errors += do_plock(TXTLOCK, EINVAL);
	errors += do_plock(PROCLOCK, EINVAL);
	errors += do_plock(UNLOCK, 0);

	Message("Adding DATLOCK, PROCLOCK after DATLOCK already exists\n");
	errors += do_plock(DATLOCK, 0);
	errors += do_plock(DATLOCK, EINVAL);
	errors += do_plock(PROCLOCK, EINVAL);
	errors += do_plock(UNLOCK, 0);

	if (errors)
		return FAIL;
	else
		return PASS;
}
