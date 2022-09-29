#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <getopt.h>

/*
 * Test to make sure that read/write systems and mmaps of the same file
 * stay consistent.  We also try to detect stale data cache entries by
 * limiting the amount of data movement done during this program and
 * in particular, when checking buffers.  The more loads and stores
 * that are done, the more likely it is that we'll wipe out the cache
 * entries that we're interested in.  Even turning on verbose output
 * is usually enough to mask data cache consistency problems.
 */

#define PASS	1
#define FAIL	0

int verbose = 0;
int iterations = 100;

#define Message if (verbose) printf

char FileName[] = "/usr/tmp/mcacheXXXXXX";

/*
 * Patterns are limited to 32 bytes so that we can use a bitmask in a
 * register when checking for mismatches.  A register must be used to
 * limit data references which could perturb the resets (see Chk()).
 * Patterns must be statically defined since generating them on the fly
 * disturbs the cache.
 */

char Pattern1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
char Pattern2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
char Pattern3[] = "cccccccccccccccccccccccccccccccc";

#define PatternLen 32	/* must be the number of bits in an int */

void writebuf(int, char *, int);
void readbuf(int, char *, int);
int Chk(char *, char, int);

main(int argc, char *argv[])
{
	int c;
	int errflg = 0;

	while ((c = getopt(argc, argv, "vi:")) != -1)
		switch (c) {
		case 'v':
			fprintf(stderr, "Warning: turning on -v can alter results of the test since the\nextra printfs change the cache state!\n");
			verbose++;
			break;
		case 'i':
			iterations = atoi(optarg);
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, "Usage: mcache [-v] [-i iterations]\n");
		exit(1);
	}

	mktemp(FileName);

	while (iterations--)
		if (DoTest() == FAIL)
			exit(1);

	exit(0);
}


/*
 * Create a temp file and do reads and writes via mmap and system
 * calls to make sure data stays in sync.
 */

DoTest()
{
	int fd;
	char *addr;
	char iobuf[PatternLen];

	Message("Start Test: creating file %s\n", FileName);

	if ((fd = open(FileName, O_CREAT |O_TRUNC | O_RDWR)) == -1) {
		perror(FileName);
		exit(1);
	}

	/*
	 * Fill the file with the initial pattern, then read it back
	 * through the mapped file to see if it's the same.
	 */

	Message("Writting Pattern1\n");
	writebuf(fd, Pattern1, PatternLen);

	if ((addr = (char *)mmap((void *)0, PatternLen, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, (off_t) 0)) == (char *) -1) {
		perror("Dotest mmap1");
		exit(1);
	}

	Message("Checking for Pattern1 in mapping\n");

	if (Chk(addr, Pattern1[0], PatternLen) == FAIL)
		return FAIL;

	/*
	 * Now write to the mapped file and make sure we get the same
	 * thing by doing a read system call.
	 */

	Message("Filling mapping Pattern2\n");
	strcpy(addr, Pattern2);
	readbuf(fd, iobuf, PatternLen);
	Message("Checking for Pattern2 in iobuf\n");
	if (Chk(iobuf, Pattern2[0], PatternLen) == FAIL)
		return FAIL;

	/*
	 * Again, write through file and check the contents of the mapping.
	 * Since we've already referenced the old pattern once, the old
	 * pattern should still be in the data cache.  Therefore, this
	 * is where we hope to detect any data cache consistency problems.
	 */

	Message("Writting Pattern3\n");
	writebuf(fd, Pattern3, PatternLen);
	Message("Checking for Pattern3 in mapping\n");
	if (Chk(addr, Pattern3[0], PatternLen) == FAIL)
		return FAIL;

	/*
	 * Clean up
	 */

	Message("Done\n");
	if (munmap((void *) addr, PatternLen) == -1) {
		perror("DoTest munmap1");
		exit(1);
	}

	close(fd);
	unlink(FileName);
	return PASS;
}

/*
 * Write to the beginning of a file, checking for errors
 */

void
writebuf(int fd, char *buf, int len)
{
	int rval;

	lseek(fd, (off_t) 0, SEEK_SET);

	if ((rval = write(fd, buf, len)) != len) {
		if (rval == -1)
			perror("write");
		else
			fprintf(stderr, "short write, returned %d instead of %d\n",
				rval, len);

		exit(1);
	}
}

/*
 * Read from the beginning of a file, checking for errors
 */

void
readbuf(int fd, char *buf, int len)
{
	int rval;

	lseek(fd, (off_t) 0, SEEK_SET);

	if ((rval = read(fd, buf, len)) != len) {
		if (rval == -1)
			perror("read");
		else
			fprintf(stderr, "short read, returned %d instead of %d\n",
				rval, len);

		exit(1);
	}
}


/*
 * Check to see if a buffer is completely filled with the given char.  It is
 * important to limit loads and stores to as few as possible.  That's why we
 * don't print any errors until after we've checked the whole buffer 
 * (hopefully the compiler we use only registers during the while loop).  It's
 * also necessary to record the errors in a register since any stores could
 * change the cache state and make other mismatches appear to go away.
 */

int
Chk(register char *buf, register char c, register int len)
{
	register char *bufstart = buf;
	register unsigned int Errors = 0;
	register int pos = 1;

	while (len--) {
		if (*buf != c)
			Errors |= pos;

		pos = pos << 1;
		buf++;
	}

	if (Errors) {
		fprintf(stderr, "Chk: mmap consistency errors when checking for \'%c\' at address 0x%x\nat offsets: ",
			c, bufstart);

		for (pos = 0; pos < PatternLen; pos++)
			if (Errors & (1 << pos))
				fprintf(stderr, "%d,", pos);

		fprintf(stderr, "\n");
		return FAIL;
	}

	return PASS;
}
