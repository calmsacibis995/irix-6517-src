/*
 * Test raw i/o support.  This program will do raw i/o to/from a disk
 * verifying the results by doing the same i/o from the cooked disk.
 *
 * Written by: Kipp Hickman
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/efs/test/RCS/dma.c,v $
 * $Revision: 1.1 $
 * $Date: 1987/02/12 17:06:08 $
 */
#include <sys/param.h>
#include <stdio.h>
#include <fcntl.h>
#if defined(IP2) || defined(PM2)
#define	NBPSCTR	BBSIZE
#endif

#ifdef	DEBUG
extern	void _assert();
#define ASSERT(EX) if (EX) ; else _assert("EX", __FILE__, __LINE__)
#else
#define	ASSERT(EX)
#endif

extern	int errno;
extern	char *sbrk();

#ifdef	DEBUG
unsigned char *lastendbrk;
#endif
char	*progname;
int	forever;
int	repeat;
int	rawfd, cookedfd;
char	*raw, *cooked;
struct	test *tp;			/* current test in progress */

/* arg to getbuffer */
#define	ALIGNED		1		/* aligned on given boundary */
#define	UNALIGNED	2		/* unaligned on given boundary */

/*
 * Tests to run.
 */
struct test {
	int	flag;
	int	count;
	int	boundary;
} tests[] = {
	{ ALIGNED, NBPSCTR*1, NBPSCTR },
	{ ALIGNED, NBPSCTR*2, NBPSCTR },
	{ ALIGNED, NBPSCTR*3, NBPSCTR },
	{ ALIGNED, NBPSCTR*4, NBPSCTR },
	{ ALIGNED, NBPSCTR*5, NBPSCTR },
	{ ALIGNED, NBPSCTR*6, NBPSCTR },
	{ ALIGNED, NBPSCTR*7, NBPSCTR },
	{ ALIGNED, NBPSCTR*8, NBPSCTR },
	{ ALIGNED, NBPSCTR*9, NBPSCTR },
	{ ALIGNED, NBPSCTR*13, NBPSCTR },
	{ ALIGNED, NBPSCTR*15, NBPSCTR },
	{ ALIGNED, NBPSCTR*25, NBPSCTR },
	{ ALIGNED, NBPSCTR*45, NBPSCTR },
	{ ALIGNED, NBPSCTR*99, NBPSCTR },
	{ UNALIGNED, NBPSCTR*1, NBPSCTR },
	{ UNALIGNED, NBPSCTR*2, NBPSCTR },
	{ UNALIGNED, NBPSCTR*3, NBPSCTR },
	{ UNALIGNED, NBPSCTR*4, NBPSCTR },
	{ UNALIGNED, NBPSCTR*5, NBPSCTR },
	{ UNALIGNED, NBPSCTR*6, NBPSCTR },
	{ UNALIGNED, NBPSCTR*7, NBPSCTR },
	{ UNALIGNED, NBPSCTR*8, NBPSCTR },
	{ UNALIGNED, NBPSCTR*9, NBPSCTR },
	{ UNALIGNED, NBPSCTR*13, NBPSCTR },
	{ UNALIGNED, NBPSCTR*15, NBPSCTR },
	{ UNALIGNED, NBPSCTR*25, NBPSCTR },
	{ UNALIGNED, NBPSCTR*45, NBPSCTR },
	{ UNALIGNED, NBPSCTR*99, NBPSCTR },
	{ ALIGNED, NBPSCTR-1, NBPSCTR },
	{ UNALIGNED, NBPSCTR-1, NBPSCTR },
};
#define	NTESTS	(sizeof(tests) / sizeof(struct test))

/*
 * Fail the current test.
 */
void
fail()
{
	fprintf(stderr,
		"%s: test failed, parameters: %d bytes %s on %d boundary\n",
		progname, tp->count,
		tp->flag == ALIGNED ? "aligned" : "unaligned", tp->boundary);
	exit(-1);
}

/*
 * Interface to sbrk
 */
unsigned char *
mysbrk(n)
	int n;
{
	register unsigned char *rv;
	register unsigned char *cp;
	register int i;

	ASSERT(n > 0);
	rv = (unsigned char *) sbrk(n);
	if (!rv) {
		fprintf(stderr, "%s: out of memory\n", progname);
		fail();
	}
	/*
	 * Fill the buffer with some known data.
	 */
	cp = rv;
	for (i = n; --i >= 0; )
		*cp++ = 0x33;
#ifdef	DEBUG
	lastendbrk = rv + n;
#endif
	return (rv);
}

/*
 * Buffer allocator.  Allocate n bytes of data, aligned or unaligned.
 */
unsigned char *
getbuffer(flag, n, boundary)
	int flag;
	int n;
	int boundary;
{
	unsigned char *rv;
	long remainder;

	switch (flag) {
	  case ALIGNED:
		/*
		 * Insure that buffer is aligned on the given boundary
		 */
		remainder = (long) sbrk(0) % boundary;
		if (remainder == 0) {
			rv = mysbrk(n);
			return (rv);
		}
		(void) mysbrk(boundary - remainder);
		return (mysbrk(n));
	  case UNALIGNED:
		/*
		 * Insure that buffer is not aligned on the given
		 * boundary.
		 */
		remainder = (long) sbrk(0) % boundary;
		if (remainder == 0) {
			(void) mysbrk(boundary - 1);
			return (mysbrk(n));
		}
		return (mysbrk(n));
	}
}

/*
 * Interface to read
 */
void
myread(fd, buf, count, dev)
	int fd;
	unsigned char *buf;
	int count;
	char *dev;
{
	int nb;
	int old_errno;

	if ((nb = read(fd, buf, count)) != count) {
		if (nb < 0) {
			old_errno = errno;
			fprintf(stderr, "%s: ", progname);
			errno = old_errno;
			perror(dev);
			fail();
		} else {
			fprintf(stderr,
				"%s: expected %d bytes from %s, got %d\n",
				progname, count, dev, nb);
			fail();
		}
	}
}

/*
 * Interface to write
 */
void
mywrite(fd, buf, count, dev)
	int fd;
	unsigned char *buf;
	int count;
	char *dev;
{
	int nb;
	int old_errno;

	if ((nb = write(fd, buf, count)) != count) {
		if (nb < 0) {
			old_errno = errno;
			fprintf(stderr, "%s: ", progname);
			errno = old_errno;
			perror(dev);
			fail();
		} else {
			fprintf(stderr,
				"%s: tried to write %d bytes to %s, wrote %d\n",
				progname, count, dev, nb);
			fail();
		}
	}
}

/*
 * Compare two buffers, printing out an error if they are different
 */
void
compare(buf1, buf2, len)
	register unsigned char *buf1;
	register unsigned char *buf2;
	register int len;
{
	unsigned char *oldbuf1, *oldbuf2;

	oldbuf1 = buf1;
	oldbuf2 = buf2;
	while (--len >= 0) {
		if (*buf1 != *buf2) {
			fprintf(stderr, "%s: raw/cooked comparison mismatch\n",
					progname);
			fprintf(stderr,
			 "Addresses: raw=%x cooked=%x Data: raw=%x cooked=%x\n",
			 buf1, buf2, *buf1, *buf2);
			fprintf(stderr, "Buffers: raw=%x cooked=%x\n",
					oldbuf1, oldbuf2);
			fail();
		}
		buf1++;
		buf2++;
	}
}

/*
 * Run tests
 */
void
test_rawio()
{
	unsigned char *oldbrk;
	unsigned char *rawbuf, *cookedbuf;
	int i;

	tp = &tests[0];
	for (i = 0; i < NTESTS; i++, tp++) {
		oldbrk = (unsigned char *) sbrk(0);
		rawbuf = getbuffer(tp->flag, tp->count, tp->boundary);
		cookedbuf = getbuffer(tp->flag, tp->count, tp->boundary);
		/* read raw disk */
		lseek(rawfd, 0L, 0);
		myread(rawfd, rawbuf, tp->count, raw);

		/* read cooked disk */
		lseek(cookedfd, 0L, 0);
		myread(cookedfd, cookedbuf, tp->count, cooked);

		/* compare raw vs cooked */
		compare(rawbuf, cookedbuf, tp->count);

		/* write raw back */
		lseek(rawfd, 0L, 0);
		mywrite(rawfd, rawbuf, tp->count, raw);

		brk(oldbrk);
	}
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int old_errno;
	int pass;

	progname = argv[0];
	if ((argc < 3) || (argc > 4)) {
		fprintf(stderr, "usage: %s rawdev blockdev [repeat-count]\n",
				progname);
		exit(-1);
	}

	/* open special device */
	raw = argv[1];
	cooked = argv[2];
	if ((rawfd = open(raw, O_RDWR)) < 0) {
		old_errno = errno;
		fprintf(stderr, "%s: ", progname);
		errno = old_errno;
		perror(raw);
		exit(-1);
	}
	if ((cookedfd = open(cooked, O_RDONLY)) < 0) {
		old_errno = errno;
		fprintf(stderr, "%s: ", progname);
		errno = old_errno;
		perror(cooked);
		exit(-1);
	}

	if (argc == 3)
		forever = 1;
	else
		repeat = atoi(argv[2]);

	for (pass = 0; ; pass++) {
		test_rawio();
		if (!forever) {
			if (--repeat <= 0)
				break;
		}
	}
	exit(0);
}
