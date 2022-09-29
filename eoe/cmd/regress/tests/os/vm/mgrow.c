#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/signal.h>

/*
 * Unit/stress test for mmap of a file that grows.
 *
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int Dbg = 0;
int Iterations = 5;

int do_mmap(char *, char *, int, int, int, off_t, int, char **);
int fill_mapping(int, char *, int, char *, int);
int test_mem(char *, int, int, int);
void handler(int);

static int pagesize;
#undef _PAGESZ
#define _PAGESZ		pagesize

main(int argc, char *argv[])
{
	int c;
	int errflg = 0;

	pagesize = getpagesize();
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
		fprintf(stderr, "Usage: mgrow [-v]\n");
		exit(1);
	}


	while (Iterations--) {
		if (Unit_tests() == FAIL) {
			printf("\n*** UNIT TESTS FAILED ***\n\n");
			exit(1);
		}
	}

	printf("mgrow: All tests passed\n");
	exit(0);
}

#define MAP_SIZE	(10*NBPP)

Unit_tests()
{
	int errors = 0;
	int stack_var;
	char *addr;
	int fd;
	char file[] = "/usr/tmp/mgrow.XXXXXX";

	sigset(SIGSEGV, handler);
	sigset(SIGBUS, handler);

	Message("Beginning Unit Tests...\n");

	/*
	 *	Try some simple error cases first.
	 */

	Message("mis-aligned address...\n");
	errors += do_mmap("/dev/zero", (char *)0x5, NBPP, PROT_WRITE, 
				MAP_FIXED|MAP_PRIVATE, 0, EINVAL, NULL);

	Message("bad address...\n");
	errors += do_mmap("/dev/zero", (char *)0x80000000, NBPP, PROT_WRITE, 
				MAP_FIXED|MAP_PRIVATE, 0, ENOMEM, NULL);

	Message("range goes beyond KUSEG...\n");
	errors += do_mmap("/dev/zero", (char *)ctob(btoct(&stack_var)), 
				1000 * NBPP, PROT_READ, MAP_FIXED|MAP_PRIVATE, 0, 
				ENOMEM, NULL);


	/*
	 * Start with an empty file, then append to it
	 */

	mktemp(file);

	if ((fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0666)) == -1) {
		perror(file);
		return FAIL;
	}

	if ((addr = (char *)mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
				fd, (off_t)0)) == (char *)-1) {
		fprintf(stderr, "mmap of file %s failed, errno %d\n",
			file, errno);
		return FAIL;
	}

	Message("Start with an empty file and fill it...\n");
	errors += fill_mapping(fd, addr, MAP_SIZE, addr, MAP_SIZE);

	Message("Map the same file again, with a bigger range...\n");

	if ((addr = (char *)mmap(NULL, MAP_SIZE*2, PROT_READ|PROT_WRITE, MAP_SHARED,
				fd, (off_t)0)) == (char *)-1) {
		fprintf(stderr, "mmap 2 of file %s failed, errno %d\n",
			file, errno);
		return FAIL;
	}

	errors += fill_mapping(fd, addr, MAP_SIZE*2, addr + MAP_SIZE, MAP_SIZE);

	Message("Truncate the file and make sure the mapping comes up empty...\n");

	if (ftruncate(fd, (off_t)0) == -1) {
		perror("ftruncate");
		return FAIL;
	}

	errors += test_mem(addr, MAP_SIZE*2, PROT_READ|PROT_WRITE, SIGBUS);

	Message("Now fill it up again...\n");
	(void) lseek(fd, (off_t)0, SEEK_SET);
	errors += fill_mapping(fd, addr, MAP_SIZE*2, addr, MAP_SIZE*2);

	close(fd);
	unlink(file);

	if (errors)
		return FAIL;
	else
		return PASS;
}

int
fill_mapping(int fd, char *base, int size, char *start, int len)
{
	int errors = 0;
	char buf;

	Message("	make sure the part that's already filled is there...\n");
	errors += test_mem(base, size - len, PROT_READ|PROT_WRITE, 0);
	Message("	make sure the empty part is empty...\n");
	errors += test_mem(start, len, PROT_READ|PROT_WRITE, SIGBUS);

	Message("	add a page at a time to mapping and test it...\n");

	while (len) {
		write(fd, &buf, NBPP); /* doesn't matter what we write */
		start += NBPP;
		len -= NBPP;
		errors += test_mem(base, size - len, PROT_READ|PROT_WRITE, 0);
		errors += test_mem(start, len, PROT_READ|PROT_WRITE, SIGBUS);
	}

	return errors;
}


int
do_mmap(char *file, char *start, int len, int prot, int flags, off_t off, 
	int expect, char **return_addr)
{
	int fd;
	char *addr;

	if ((fd = open(file, O_RDWR)) == -1) {
		perror(file);
		exit(1);	/* can't continue test */
	}

	if ((addr = (char *)mmap(start, len, prot, flags, fd, off)) == (char *) -1) {
		if (expect) {
			if (errno != expect) {
				fprintf(stderr, "mmap failed with errno %d when error %d was expected\n",
					errno, expect);
				close(fd);
				return 1;
			}

			close(fd);
			return 0;	/* failed as expected */

		} else {
			fprintf(stderr, "mmap failed with errno %d\n", errno);
			close(fd);
			return 1;
		}
	}

	if (expect) {
		fprintf(stderr, "mmap succeeded when errno %d was expected\n",
				expect);
		close(fd);
		return 1;
	}

	close(fd);
	*return_addr = addr;
	return 0;
}

jmp_buf env;
int sig_recvd;
char *addr;
int len;

int
test_mem(char *addr_arg, int len_arg, int prot, int sig_expected)
{
	int x;
	int errors = 0;

	sig_recvd = 0;
	addr = addr_arg;	/* save these in globals, otherwise the games */
	len = len_arg;		/* I use here won't work when the code is     */
				/* optimized				      */

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

		addr += NBPP;
		len -= NBPP;
		sig_recvd = 0;
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

		addr += NBPP;
		len  -= NBPP;
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
