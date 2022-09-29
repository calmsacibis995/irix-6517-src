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
 * Unit/stress test for autogrow anonymous regions (those that reserve
 * availsmem on demand).
 *
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int Dbg = 0;
int Iterations = 50;

int do_mprotect(char *, int, int, int);
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
		fprintf(stderr, "Usage: autoanon [-d] [-i #_of_iterations] [-v]\n");
		exit(1);
	}


	while (Iterations--) {
		if (Unit_tests() == FAIL) {
			printf("\n*** UNIT TESTS FAILED ***\n\n");
			exit(1);
		}
	}

	printf("autoanon: All tests passed\n");
	exit(0);
}

#define MAP_SIZE	(500*1024*1024)
#define SMALL_MAP_SIZE	(500*1024)

Unit_tests()
{
	int errors = 0;
	char *addr;
	int fd;

	sigset(SIGBUS, handler);

	Message("Beginning Unit Tests...\n");

	/*
	 *	Allocate a big hunk of /dev/zero mapped with autogrow.
	 *	This is intended to be way larger than there could possibly
	 *	be enough availsmem to support.  Big memory systems will
	 *	have to run multiple copies of this test in parallel to
	 *	achieve the same effect, since there aren't enough places
	 *	in kuseg to put really big regions.
	 */

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("/dev/zero 1");
		return FAIL;
	}

	if ((addr = (char *)mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC,
	     MAP_PRIVATE|MAP_AUTORESRV, fd, (off_t)0)) == (char *) -1) {
		perror("mmap of autogrow, private /dev/zero region failed");
		return FAIL;
	}

	Message("Randomly touching pages in big /dev/zero mapping...\n");

	if (touch_rand(addr, MAP_SIZE, 1000) == FAIL)
		return FAIL;

	Message("Done.  Unmapping...\n");

	if (munmap(addr, MAP_SIZE) == -1) {
		perror("munmap 1");
		return FAIL;
	}

	close(fd);

	/*
	 *	Let's try that again, except with a regular file mapped as
	 * 	private and autogrow.
	 */

	if ((fd = open("/bin/sh", O_RDONLY)) == -1) {
		perror("/bin/sh");
		return FAIL;
	}

	if ((addr = (char *)mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC,
	 MAP_PRIVATE|MAP_AUTOGROW|MAP_AUTORESRV, fd, (off_t)0)) == (char *) -1){
		perror("mmap of autogrow, private reg file failed");
		return FAIL;
	}

	Message("Randomly touching pages in regular file mapping...\n");

	if (touch_rand(addr, MAP_SIZE, 1000) == FAIL)
		return FAIL;

	Message("Done.  Unmapping...\n");

	if (munmap(addr, MAP_SIZE) == -1) {
		perror("munmap 2");
		return FAIL;
	}

	close(fd);

	/*
	 * Try a small region, too.  This way we'll get lots of repeated 
	 * access to the same pages.
	 */

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("/dev/zero 3");
		return FAIL;
	}

	if ((addr = (char *)mmap(NULL, SMALL_MAP_SIZE, 
	     PROT_READ|PROT_WRITE|PROT_EXEC,
	     MAP_PRIVATE|MAP_AUTORESRV, fd, (off_t)0)) == (char *) -1) {
		perror("mmap of small autogrow, private /dev/zero region failed");
		return FAIL;
	}

	Message("Randomly touching pages in small /dev/zero mapping...\n");

	if (touch_rand(addr, SMALL_MAP_SIZE, 1000) == FAIL)
		return FAIL;

	Message("Done.  Unmapping...\n");

	if (munmap(addr, SMALL_MAP_SIZE) == -1) {
		perror("munmap 3");
		return FAIL;
	}

	close(fd);

	/*
	 * Try it with a shared /dev/zero mapping now.  This way we'll get
	 * lots of repeated access to the same pages.
	 */

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("/dev/zero 4");
		return FAIL;
	}

	if ((addr = (char *)mmap(NULL, MAP_SIZE, 
	     PROT_READ|PROT_WRITE|PROT_EXEC,
	     MAP_SHARED|MAP_AUTORESRV, fd, (off_t)0)) == (char *) -1) {
		perror("mmap of large autoresrv, shared /dev/zero region failed");
		return FAIL;
	}

	Message("Randomly touching pages in shared /dev/zero mapping...\n");

	if (touch_rand(addr, MAP_SIZE, 1000) == FAIL)
		return FAIL;

	Message("Done.  Unmapping...\n");

	if (munmap(addr, MAP_SIZE) == -1) {
		perror("munmap 4");
		return FAIL;
	}

	close(fd);

	if (errors)
		return FAIL;
	else
		return PASS;
}


jmp_buf env;
int sig_recvd;

/*
 * Randomly touch 'count' pages in the address range 'base' to 'base+len'
 * We don't expect to run out of availsmem here, so if we get segv, we
 * abort.
 */

int
touch_rand(char *base, int len, int count)
{
	volatile char *addr;
	int loops, do_read;
	unsigned int seed;

	sig_recvd = 0;

	if (setjmp(env)) {
		printf("got sig %d\n", sig_recvd);
		return FAIL;
	}

	seed = (unsigned int) time(NULL);

	/*
	 * Randomly read and write the specified number of pages 
	 */

	srand(seed);
	Debug("pass 1\n");

	for (loops = count; loops; loops--) {
		addr = base + (rand() % (len / NBPP)) * NBPP;
		do_read = rand() % 2;

		Debug("trying 0x%x for %s\n", addr, do_read ? "read" : "write");

		if (do_read)
			*addr;
		else
			*addr = 1;
	}

	if (madvise(base, len, MADV_DONTNEED) == -1) {
		perror("madvise 1");
		return FAIL;
	}

	/*
	 * Go back over the same pages again.  This will hopefully uncover
	 * any availsmem leaks if pages are vfault'ed/pfault'ed on more
	 * then once.
	 */

	srand(seed);
	Debug("pass 2\n");

	for (loops = count; loops; loops--) {
		addr = base + (rand() % (len / NBPP)) * NBPP;
		do_read = rand() % 2;

		Debug("trying 0x%x for %s\n", addr, do_read ? "read" : "write");

		if (do_read)
			*addr;
		else
			*addr = 1;
	}

	if (madvise(base, len, MADV_DONTNEED) == -1) {
		perror("madvise 1");
		return FAIL;
	}

	/*
	 * Go back over half the pages and write to them.  This way we
	 * will change the state of some of the pages from read-only to
	 * to read/write.  Hopefully this will uncover availsmem leaks.
	 * We don't want to write to all the pages since availsmem is
	 * only allocated to writable pages.  Again, we're looking for
	 * availsmem leaks.
	 */

	srand(seed);
	Debug("pass 3\n");

	for (loops = count / 2; loops; loops--) {
		addr = base + (rand() % (len / NBPP)) * NBPP;
		do_read = rand() % 2;

		Debug("trying 0x%x for write\n", addr);

		*addr = 1;
	}

	return PASS;
}


void
handler(int sig)
{
	Debug("got sig %d\n", sig);
	sig_recvd = sig;
	sigrelse(sig);
	longjmp(env, 1);
}
