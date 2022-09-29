#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/prctl.h>

/*
 * Unit/stress test for anonymous manager limit operations.
 *
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int Dbg = 0;
int Iterations = 10;
int pagesize;

#define BSS_SIZE	4000	/* pages */

main(int argc, char *argv[])
{
	int c;
	int errflg = 0;

	while ((c = getopt(argc, argv, "vdi:")) != -1)
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
		fprintf(stderr, "Usage: limit [-v] [-d] [-i iterations]\n");
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

	printf("limit: All tests passed\n");
	exit(0);
}


extern _fdata[];

stress_tests()
{
	int errors = 0;
	char *end, *addr, *newend;
	void *base;
	int i, incr;

	Message("Beginning Stress Tests...\n");

	/*
	 * Start off rounding up data+bss to a page boundary
	 */

	end = (char *) sbrk(0);
	incr = pagesize - ((unsigned long)end & (pagesize-1));
	newend = sbrk(incr);

	/*
	 * Get second mapping so we can make sanon pages, then unmap
	 * it so that the region refcnt is 1.  If it's greater than
	 * 1 then growreg won't actually shrink the region.
	 */

	if ((base = (void *)prctl(PR_ATTACHADDR, getpid(), _fdata)) == (void *) -1) {
		perror("prctl(PR_ATTACHADDR)");
		return FAIL;
	}

	base = (char *)((ptrdiff_t)base & ~(pagesize -1));
	if (munmap(base, newend - (char *)_fdata) == -1) {
		perror("munmap");
		return FAIL;
	}

#if 0
	mybrk(0x10000000);
	mybrk(0x10001000);
	*((char *)0x10000000) = 1;
	return PASS;
#endif
	
	/*
	 * Grow and shrink bss 1 page at a time.
	 */

	for (i = 0; i < BSS_SIZE; i++) {
		addr = (char *)sbrk(pagesize) + pagesize - 1;
		*addr = 1;
		sbrk(-pagesize);
		sbrk(pagesize);
		*addr = 1;
		sbrk(-pagesize);
		sbrk(pagesize);
	}

	return PASS;
}
