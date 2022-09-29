#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/signal.h>

/*
 * Unit/stress test for mprotect.
 *
 * Things to add:
 *	Multiple contiguous regions
 *	Share groups (one mprotects, make sure others see it)
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int Dbg = 0;
int Iterations = 5;

int do_mprotect(char *, int, int, int);
int test_mem(char *, int, int, int);
void handler(int);

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
		fprintf(stderr, "Usage: mprotect [-v]\n");
		exit(1);
	}


	while (Iterations--) {
		if (Unit_tests() == FAIL) {
			printf("\n*** UNIT TESTS FAILED ***\n\n");
			exit(1);
		}
	}

	printf("mprotect: All tests passed\n");
	exit(0);
}

static char *
addr_to_page(char *addr)
{
	return((char *) ((__psunsigned_t) addr - 
			 ((__psunsigned_t) addr %
			  (__psunsigned_t) getpagesize())));
}


#define MAP_SIZE	(10*getpagesize())

Unit_tests()
{
	int errors = 0;
	int stack_var;
	char *addr;
	int fd;

	sigset(SIGSEGV, handler);

	Message("Beginning Unit Tests...\n");

	/*
	 *	Try some simple error cases first.
	 */

	Message("mis-aligned address...\n");
	errors += do_mprotect((char *)0x5L, getpagesize(), PROT_WRITE, EINVAL);

	Message("bad address...\n");
	errors += do_mprotect((char *)0x5000000L, getpagesize(), PROT_READ, ENOMEM);

	Message("range goes beyond KUSEG...\n");
	errors += do_mprotect(addr_to_page((char *)&stack_var), 1000 * getpagesize(), (PROT_WRITE | PROT_EXEC), ENOMEM);


	/*
	 *	Allocate a piece of memory with all permissions on and try
	 *	a few tests.
	 */

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("/dev/zero 1");
		return FAIL;
	}

	if ((addr = (char *)mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC,
				MAP_PRIVATE, fd, (off_t)0)) == MAP_FAILED) {
		perror("mmap 1");
		return FAIL;
	}

	/*
	 *	Should be able to turn on permissions that are already set.
	 */

	Message("Turn on permissions already enabled...\n");
	errors += do_mprotect(addr, MAP_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, 0);

	Message("Make sure we can still read/write the memory...\n");
	errors += test_mem(addr, MAP_SIZE, PROT_READ | PROT_WRITE, 0);

	/*
	 *	Now try really changing things.
	 */

	Message("Take away write permission...\n");
	errors += do_mprotect(addr, MAP_SIZE, PROT_READ, 0);

	Message("Make sure we can still read it...\n");
	errors += test_mem(addr, MAP_SIZE, PROT_READ, 0);

	Message("Try to write it...\n");
	errors += test_mem(addr, MAP_SIZE, PROT_WRITE, SIGSEGV);

	Message("Re-enable write on half of region...\n");
	errors += do_mprotect(addr, MAP_SIZE / 2, PROT_READ|PROT_WRITE, 0);

	Message("Read and write it...\n");
	errors += test_mem(addr, MAP_SIZE / 2, PROT_READ | PROT_WRITE, 0);
	errors += test_mem(addr + MAP_SIZE / 2, MAP_SIZE / 2, PROT_READ, 0);
	errors += test_mem(addr + MAP_SIZE / 2, MAP_SIZE / 2, PROT_WRITE, SIGSEGV);

	Message("Shut everything off in middle of region...\n");
	errors += do_mprotect(addr + getpagesize(), MAP_SIZE / 2, PROT_NONE, 0);
	errors += test_mem(addr, getpagesize(), PROT_READ|PROT_WRITE, 0);
	errors += test_mem(addr + getpagesize(), MAP_SIZE / 2, PROT_READ|PROT_WRITE, SIGSEGV);
	errors += test_mem(addr + getpagesize() + MAP_SIZE / 2, MAP_SIZE / 2 - getpagesize(), PROT_READ, 0);
	errors += test_mem(addr + getpagesize() + MAP_SIZE / 2, MAP_SIZE / 2 - getpagesize(), PROT_WRITE, SIGSEGV);

	Message("Turn everything back on...\n");
	errors += do_mprotect(addr, MAP_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, 0);
	errors += test_mem(addr, MAP_SIZE, PROT_READ|PROT_WRITE, 0);

	/*
	 *	Now some more complicated error cases where things
	 *	partially succeed then crap out.
	 */

	Message("Go past end of region...\n");
	errors += do_mprotect(addr, MAP_SIZE * 2, PROT_READ, ENOMEM);

	Message("Region should be all PROT_READ now...\n");
	errors += test_mem(addr, MAP_SIZE, PROT_READ, 0);
	errors += test_mem(addr, MAP_SIZE, PROT_WRITE, SIGSEGV);


	/*
	 * Now try mapping with less than the open permissions and try
	 * turning on permissions.
	 */

	if ((addr = (char *)mmap(NULL, MAP_SIZE, PROT_READ,
				MAP_PRIVATE, fd, (off_t)0)) == MAP_FAILED) {
		perror("mmap 1");
		return FAIL;
	}

	Message("Starting out with new mmap region with only PROT_READ\n");
	Message("Test that only read access is allowed...\n");
	errors += test_mem(addr, MAP_SIZE, PROT_READ, 0);
	errors += test_mem(addr, MAP_SIZE, PROT_WRITE, SIGSEGV);

	Message("Adding PROT_EXEC...\n");
	errors += do_mprotect(addr, MAP_SIZE, PROT_READ|PROT_EXEC, 0);
	errors += test_mem(addr, MAP_SIZE, PROT_READ, 0);
	errors += test_mem(addr, MAP_SIZE, PROT_WRITE, SIGSEGV);

	Message("Adding PROT_WRITE...\n");
	errors += do_mprotect(addr, MAP_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, 0);
	errors += test_mem(addr, MAP_SIZE, PROT_READ, 0);
	errors += test_mem(addr, MAP_SIZE, PROT_WRITE, 0);


	/*
	 * Now try making our own text writable.  Modify the text of this
	 * routine and then we'll see if this program still runs right the
	 * next time it gets called.
	 */

	Message("Adding PROT_WRITE to our text...\n");
	errors += do_mprotect(addr_to_page((char *)Unit_tests), getpagesize(),
				 PROT_READ|PROT_WRITE|PROT_EXEC, 0);
	errors += test_mem(addr_to_page((char *)Unit_tests), getpagesize(),
				 PROT_READ|PROT_WRITE, 0);

	if (errors)
		return FAIL;
	else
		return PASS;
}

int
do_mprotect(char *addr, int len, int prot, int expect)
{
	int rval;
	int *myerrno = &errno;
	rval = mprotect(addr, len, prot);

	if (expect) {
		if (rval != -1) {
			fprintf(stderr, "mprotect(0x%x, 0x%x, 0x%x) succeeded when expected to fail with errno %d\n",
				addr, len, prot, expect);
			return 1;
		}

		if (errno != expect) {
			fprintf(stderr, "mprotect(0x%x, 0x%x, 0x%x) failed with errno %d when errno %d was expected\n",
				addr, len, prot, errno, expect);
			return 1;
		}

		return 0;	/* test failed as expected */

	} else {
		if (rval == -1) {
			fprintf(stderr, "mprotect(0x%x, 0x%x, 0x%x) failed with errno %d\n",
				addr, len, prot, errno);
			return 1;
		}

		return 0;
	}
}

jmp_buf env;
int sig_recvd;

int
test_mem(char *addr, int len, int prot, int sig_expected)
{
	int x;
	int errors = 0;

	sig_recvd = 0;

restart:
	if (setjmp(env)) {
		if (sig_expected) {
			if (sig_recvd != sig_expected) {
				fprintf(stderr, "Expected sig %d, got %d instead. addr 0x%x\n",
					sig_expected, sig_recvd, addr);
				errors++;
			}
		} else {
			fprintf(stderr, "Unexpected signal %d.  addr = 0x%x\n",
				sig_recvd, addr);
			errors++;
		}

		addr += getpagesize();
		len -= getpagesize();
		sig_recvd = 0;

		/*
		 * Must go back and do the setjmp again since the compiler
		 * could be keeping addr and len in registers only.  Old
		 * values could therefore be in 'env', so we have to update
		 * it.
		 */

		goto restart;
	}

	while (len) {
		Debug("trying 0x%x\n", addr);

		if (prot & PROT_READ)
			x = *addr;

		if (prot & PROT_WRITE)
			*addr = x;

		if (sig_expected && !sig_recvd) {
			fprintf(stderr, "Access succeeded when signal %d was expected.  addr 0x%x\n",
				sig_expected, addr);
			errors++;
		}

		addr += getpagesize();
		len  -= getpagesize();
	}

	return errors;
}


void
handler(int sig)
{
	Debug("got sig %d\n", sig);
	sig_recvd = sig;
	sigrelse(sig);
	longjmp(env, 1);
}
