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
#include <sys/prctl.h>

/*
 * Unit/stress test for munmap.
 *
 * Things to add:
 *	Fill mmap'ed memory with known pattern and test for it
 *	unmapping pieces of contiguous regions
 *	mmap causing an unmap of a piece of an existing region
 *	Test all different types of regions:
 *		stack
 *		data
 *		text
 *		MAP_SHARED files
 *		MAP_PRIVATE files
 *		/dev/zero
 *		shm
 *		prda
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int Dbg = 0;
int Iterations = 5;

static int pagesize;
#undef _PAGESZ
#define _PAGESZ		pagesize

char *do_mmap(char *, char *, int, int, int);
int do_munmap(char *, int, int);
int munmap_forw(char *, int);
int munmap_back(char *, int);
int munmap_rand(char *, int);
int mmap_rand(char *, int);

int getstart(char *, int);
int getlen(char *, int, int);
void clrmap(char *, int, int);

int fill_mem(char *, int);
int test_mem(char *, int, int);
void handler(int);
void child();

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
		fprintf(stderr, "Usage: munmap [-v] [-i <# of iterations>]\n");
		exit(1);
	}


	while (Iterations--) {
		if (Unit_tests() == FAIL) {
			printf("\n*** UNIT TESTS FAILED ***\n\n");
			exit(1);
		}
	}

	printf("munmap: All tests passed\n");
	exit(0);
}

#define MAP_SIZE	(10*NBPP)
#define BIG_MAP_SIZE	(500*NBPP)

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
	errors += do_munmap((char *)0x5, NBPP, EINVAL);

	Message("bad address...\n");
	errors += do_munmap((char *)0x5000000, NBPP, 0);

	Message("range goes beyond KUSEG...\n");
	errors += do_munmap((char *)ctob(btoct(&stack_var)), 1000 * NBPP, EINVAL);


	/*
	 *	Try some /dev/zero memory
	 */

	Message("unmapping /dev/zero forward...\n");
	addr = do_mmap("/dev/zero", NULL, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	errors += munmap_forw(addr, MAP_SIZE);

	Message("unmapping /dev/zero backward...\n");
	addr = do_mmap("/dev/zero", NULL, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	errors += munmap_back(addr, MAP_SIZE);

	Message("unmapping /dev/zero from middle to end...\n");
	addr = do_mmap("/dev/zero", NULL, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	errors += munmap_forw(addr + MAP_SIZE / 2, MAP_SIZE / 2);

	Message("unmapping /dev/zero from middle to beginning...\n");
	errors += munmap_back(addr, MAP_SIZE / 2);

	Message("random unmapping of /dev/zero...\n");
	addr = do_mmap("/dev/zero", NULL, BIG_MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	errors += munmap_rand(addr, BIG_MAP_SIZE);

	Message("random mmap MAP_FIXED over existing mapping...\n");
	addr = do_mmap("/dev/zero", NULL, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	errors += mmap_rand(addr, MAP_SIZE);

	Message("unmap two regions with a hole between them...\n");
	addr = do_mmap("/dev/zero", NULL, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	(void) do_mmap("/dev/zero", addr + MAP_SIZE * 2, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	errors += fill_mem(addr, MAP_SIZE);
	errors += fill_mem(addr + MAP_SIZE * 2, MAP_SIZE);
	errors += do_munmap(addr, MAP_SIZE * 3, 0);
	errors += test_mem(addr, MAP_SIZE * 3, SIGSEGV);

	Message("unmap two regions with holes before, between, and after them...\n");
	addr = do_mmap("/dev/zero", NULL, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	(void) do_mmap("/dev/zero", addr + MAP_SIZE * 2, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	errors += fill_mem(addr, MAP_SIZE);
	errors += fill_mem(addr + MAP_SIZE * 2, MAP_SIZE);
	errors += do_munmap(addr - MAP_SIZE, MAP_SIZE * 5, 0);
	errors += test_mem(addr - MAP_SIZE, MAP_SIZE * 5, SIGSEGV);

	Message("unmap both shared and private regions...\n");
	(void) sproc(child, PR_SADDR);
	addr = do_mmap("/dev/zero", NULL, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE);
	(void) do_mmap("/dev/zero", addr + NBPP, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE|MAP_LOCAL);
	(void) do_mmap("/dev/zero", addr + MAP_SIZE * 2, MAP_SIZE, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE|MAP_LOCAL);
	errors += fill_mem(addr, MAP_SIZE + NBPP);
	errors += fill_mem(addr + MAP_SIZE * 2, MAP_SIZE);
	errors += do_munmap(addr - MAP_SIZE, MAP_SIZE * 5, 0);
	errors += test_mem(addr - MAP_SIZE, MAP_SIZE * 5, SIGSEGV);

	/*
	 *	Now some more complicated error cases where things
	 *	partially succeed then crap out.
	 */

/*
	Message("Go past end of region...\n");
*/

	if (errors)
		return FAIL;
	else
		return PASS;
}


/*
 * A dummy sproc child process that does nothing.  We only need to
 * call sproc to get both a shared and private region list.  We don't
 * actually need two processes around.
 */

void
child()
{
	exit(0);
}


/*
 * Munmap a region a piece at a time from the bottom to the top
 */

int
munmap_forw(char *start_addr, int start_len)
{
	char *addr = start_addr;
	int len = start_len;
	int errors = 0;

	errors += fill_mem(start_addr, start_len);

	while (len) {
		errors += do_munmap(addr, NBPP, 0);
		addr += NBPP;
		len -= NBPP;
		errors += test_mem(start_addr, start_len - len, SIGSEGV);
		errors += test_mem(addr, len, 0);
	}

	return errors;
}


/*
 * Munmap a region a piece at a time from top to bottom
 */

int
munmap_back(char *base_addr, int total_len)
{
	char *addr = base_addr + total_len - NBPP;
	int len = 0;
	int errors = 0;

	errors += fill_mem(base_addr, total_len);

	while (len != total_len) {
		len += NBPP;
		errors += do_munmap(addr, NBPP, 0);
		errors += test_mem(addr, len, SIGSEGV);
		addr -= NBPP;
		errors += test_mem(base_addr, total_len - len, 0);
	}

	return errors;
}

/*
 * Randomly unmap pages from the given region.
 */

#define MAPPED	1
#define UNMAPPED 0

int
munmap_rand(char *base_addr, int total_len)
{
	char *map;
	int pg, len;
	char *addr;
	int errors = 0;
	int npgs = total_len / NBPP;

	srand(getpid());

	/*
	 * Allocate a map to tell us which pages are mapped and which
	 * are holes.   Initialize it to all pages mapped.
	 */

	map = (char *)malloc(npgs * sizeof(char));

	for (pg = 0; pg < npgs; pg++)
		map[pg] = MAPPED;


	while (total_len) {
		pg = getstart(map, npgs);
		len = getlen(map, pg, npgs);
		Debug("unmapping page %d, len %d\n", pg, len);
		clrmap(map, pg, len);
		len = ctob(len);
		errors += do_munmap(base_addr + ctob(pg), len, 0);
		total_len -= len;
	}

	free(map);
	return errors;
}

/*
 * Randomly choose the address that still exists in the map
 */

int
getstart(char *map, int npgs)
{
	int pg;

	pg = rand() % npgs;

	while (map[pg] != MAPPED) {
		pg++;

		if (pg >= npgs)
			pg = 0;		/* wrap around */
	}

	return pg;
}


/*
 * Randomly choose the len based on how many contiguous pages are mapped
 * starting at pg.
 */

#define	MAX_CONTIG	(npgs / 10)	/* never take more than 10% at a time */

int
getlen(char *map, int pg, int npgs)
{
	int contig, len;
	int end = pg + 1;

	while (end < npgs && map[end] == MAPPED)
		end++;

	contig = end - pg;	/* # of contig pages left starting at pg */
	contig = MIN(contig, MAX_CONTIG);
	len = rand() % contig;	/* random value in range 0..contig-1 */
	len++;			/* random value in range 1..contig */

	return len;
}

/*
 * Clear the specified entries in the map
 */

void
clrmap(char *map, int pg, int len)
{
	while (len--)
		map[pg++] = UNMAPPED;
}


/*
 * Do a series of random mmap's over an existing region of memory
 */

#define MMAP_RAND_TRIES	100

int
mmap_rand(char *base, int base_len)
{
	int i;
	int fd;
	int len;
	int errors = 0;
	char *start;
	char *addr;

	errors += fill_mem(base, base_len);

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("/dev/zero");
		return 1;	/* can't continue test */
	}

	for (i = 0; i < MMAP_RAND_TRIES; i++) {
		start = (char *)ctob(btoct(base + rand() % base_len));
		len = ctob(btoc(rand() % (base_len - (start-base))));
		Debug("mmaping at 0x%x, len 0x%x\n", start, len);

		if ((addr = (char *)mmap((void *)start, len, PROT_READ|PROT_WRITE,
				MAP_PRIVATE|MAP_FIXED, fd, (off_t)0)) == NULL) {
			fprintf(stderr, "can't mmap file, ");
			perror("/dev/zero");
			fprintf(stderr, "base = 0x%x, base_len = 0x%s, start = 0x%x, len = 0x%x", 
				base, base_len, start, len);
			return 1;	/* can't continue test */
		}

		errors += fill_mem(start, len);
	}

	close(fd);
	errors += test_mem(base, base_len, 0);
	errors += do_munmap(base, base_len, 0);
	errors += test_mem(base, base_len, SIGSEGV);
	return errors;
}


char *
do_mmap(char *file, char *start, int len, int prot, int flags)
{
	int fd;
	char *addr;

	if ((fd = open(file, O_RDWR)) == -1) {
		perror(file);
		exit(1);	/* can't continue test */
	}

	if ((addr = (char *)mmap(start, len, prot, flags, fd, (off_t)0)) == NULL) {
		fprintf(stderr, "can't mmap file, ");
		perror(file);
		exit(1);	/* can't continue test */
	}

	close(fd);
	return addr;
}

int
do_munmap(char *addr, int len, int expect)
{
	int rval;

	rval = munmap(addr, len);

	if (expect) {
		if (rval != -1) {
			fprintf(stderr, "munmap(0x%x, 0x%x) succeeded when expected to fail with errno %d\n",
				addr, len, expect);
			return 1;
		}

		if (errno != expect) {
			fprintf(stderr, "munmap(0x%x, 0x%x) failed with errno %d when errno %d was expected\n",
				addr, len, errno, expect);
			return 1;
		}

		return 0;	/* test failed as expected */

	} else {
		if (rval == -1) {
			fprintf(stderr, "munmap(0x%x, 0x%x) failed with errno %d\n",
				addr, len, errno);
			return 1;
		}

		return 0;
	}
}

jmp_buf env;
int sig_recvd;

int
fill_mem(char *addr, int len)
{
	int errors = 0;

	if (setjmp(env)) {
		fprintf(stderr, "Unexpected signal %d when writing addr = 0x%x\n",
			sig_recvd, addr);
		errors++;
		addr += NBPP;
		len -= NBPP;
	}

	while (len) {
		*(int *)addr = (int)addr;
		addr += NBPP;
		len  -= NBPP;
	}

	return errors;
}

char *addr;
int len;

int
test_mem(char *addr_arg, int len_arg, int sig_expected)
{
	int x;
	int errors = 0;

	sig_recvd = 0;
	addr = addr_arg;	/* these have to be in global variables or */
	len = len_arg;		/* else the game I play here won't work    */
				/* when optimized			   */

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

		x = *(int *)addr;

		if (sig_expected && !sig_recvd) {
			fprintf(stderr, "Access succeeded when signal %d was expected.  addr 0x%x\n",
				sig_expected, addr);
			errors++;
		}

		if (x != (int)addr) {
			fprintf(stderr, "Found 0x%x at 0x%x\n", x, addr);
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
	longjmp(env, 0);
}
