/*
 * rdwr.c -- Another disk test program.
 *
 * This disk test will read a specified portion of a disk or file once.
 * It will then write and read the file (according to options) until
 * it gets an error.  Any writing done will write out what was read in
 * originally, so the test is non-destructive.  It should not be run
 * on the block or character device corresponding to a mounted file
 * system, therefore, since it will re-write the data that it read
 * in originally.
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

char  *		pname;
char  *		buf1;
char  *		buf2;
int		readonly = 0;

void usage(char *name);

extern int	errno;

main(argc, argv)
	int argc;
	char *argv[];
{
	register int	size = 0;
	register int	rwsize = BBSIZE;
	register int	niterations = 1;
	register int	lsize, seekpoint;
	char *		fname = NULL;
	struct stat	sbuf;
	int		c, fd, debug;
	int		id = getpid();
	extern int	optind;
	extern char *	optarg;
	char *		malloc();

	pname = argv[0];

	while ((c=getopt(argc, argv, "drf:s:i:b:")) != EOF) {
		switch (c) {
		case 'r':
			readonly = 1;
			continue;
		case 'd':
			debug = 1;
			continue;
		case 's':
			size = strtol(optarg, 0, 0);
			while (size & (BBSIZE-1))
				size--;
			if (debug)
				fprintf(stderr, "size set to %d (0x%x)\n",
					size, size);
			/* argc--; */
			continue;
		case 'b':
			rwsize = strtol(optarg, 0, 0);
			while (rwsize & (BBSIZE-1))
				rwsize--;
			if (debug)
				fprintf(stderr, "rwsize max set to %d (0x%x)\n",
					rwsize, rwsize);
			/* argc--; */
			continue;
		case 'i':
			niterations = strtol(optarg, 0, 0);
			if (debug)
				fprintf(stderr, "niterations set to %d\n",
					niterations);
			/* argc--; */
			continue;
		case 'f':
			fname = optarg;
			if (debug)
				fprintf(stderr, "file is %s\n", fname);
				
			/* argc--; */
			continue;
		}
	}

	if (fname == NULL) {
		usage(pname);
		exit(1);
	}

	if (stat(pname, &sbuf)) {
		perror("stat");
		exit(1);
	}

	/*
	if (sbuf.st_size < size || size == 0) {
		size = sbuf.st_size;
		if (size || debug)
			fprintf(stderr, "WARNING: %s setting size to %d\n",
				pname, size);
		while (size & (BBSIZE-1))
			size--;
	}
	*/

	if (size == 0 || rwsize == 0) {
		fprintf(stderr, "%s: ERROR: 0 size!\n", pname);
		exit(1);
	}

	fd = open(fname, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: ERROR: device open failed\n", pname);
		perror("open");
		exit(1);
	}

	buf1 = malloc(size+rwsize);
	buf2 = malloc(size+rwsize);
	if (!buf1 || !buf2) {
		fprintf(stderr, "%s: ERROR: malloc failed\n", pname);
		exit(1);
	}

	c = size+rwsize;
	if (read(fd, buf1, c) != c) {
		fprintf(stderr, "%s: ERROR: unable to read copy 1\n", pname);
		exit(1);
	}

	srandom(time(0));

	while (--niterations >= 0) {

		seekpoint = (random() % size + 256) & ~(BBSIZE - 1);
		lsize = (random() % rwsize + 512) & ~(BBSIZE - 1);

		if (lseek(fd, seekpoint, SEEK_SET) < 0) {
			fprintf(stderr, "%s: ERROR: seekpoint at %d (0x%x)\n",
				pname, seekpoint, seekpoint);
			perror("lseek");
			exit(1);
		}

		if (readonly || (random() % 4) == 0) {
			/*
			 * Read
			 */
			if (debug) {
				fprintf(stderr, "(%d: rd %x,%x) ",
					id, seekpoint, lsize);
			}
			if ((c = read(fd, buf2 + seekpoint, lsize)) != lsize) {
				fprintf(stderr,
					"%s: ERROR: short read of 0x%x at 0x%x,0x%x!\n",
					pname, c, seekpoint, lsize);
				if (errno)
					perror("read");
				exit(1);
			}
			if (compare(seekpoint, lsize, 0)) {
				if (lseek(fd, seekpoint, SEEK_SET) < 0) {
					fprintf(stderr,
						"%s: ERROR: seekpoint at %d (0x%x)\n",
						pname, seekpoint, seekpoint);
					perror("lseek");
					exit(1);
				}
				if ((c = read(fd, buf2 + seekpoint, lsize)) != lsize) {
					fprintf(stderr,
						"%s: ERROR: short read of 0x%x at 0x%x,0x%x!\n",
						pname, c, seekpoint, lsize);
					if (errno)
						perror("read");
					exit(1);
				}
				compare(seekpoint, lsize, 1);
			}
		} else {
			/*
			 * Write
			 */
			if (debug) {
				fprintf(stderr, "(%d: wr %x,%x) ",
					id, seekpoint, lsize);
			}
			if ((c = write(fd, buf1 + seekpoint, lsize)) != lsize) {
				fprintf(stderr,
					"%s: ERROR: short write of 0x%x at 0x%x,0x%x!\n",
					pname, c, seekpoint, lsize);
				if (errno)
					perror("write");
				exit(1);
			}
		}
	}
}


int nzero;
int nones;

void
comparebits(c1, c2)
	register unsigned char c1, c2;
{
	while (c1 || c2) {
		if ((c1 & 1) && !(c2 & 1))
			nzero++;
		else if ((c2 & 1) && !(c1 & 1))
			nones++;
		c1 >>= 1, c2 >>= 1;
	}
}

compare(seekpt, sz, exit_on_fail)
{
	register unsigned char *p1, *p2;
	register int nerrors;

	nzero = nones = 0;
	p1 = (unsigned char *) buf1 + seekpt;
	p2 = (unsigned char *) buf2 + seekpt;
	if (memcmp(p1, p2, sz) == 0)
		return 0;

	fprintf(stderr, "%s: ERROR: time=%ld, try #%d, addr 0x%x count 0x%x\n   ",
		pname, (long) time(0), exit_on_fail+1, seekpt, sz);

	if (memcmp(p1, p2, sz) == 0) {
		fprintf(stderr,
		        "%s: ERROR: first memcmp found miscompare; second didn't\n",
			pname);
		return 0;
	}

	while (--sz >= 0) {
		if (*p1++ != *p2++) {
			nerrors = 1;
			fprintf(stderr, "%s: ERROR: error at 0x%x", pname, seekpt);
			fprintf(stderr, " %x %x;", *(p1-1), *(p2-1));
			comparebits(*(p1-1), *(p2-1));
			while (--sz >= 0 && *p1 != *p2) {
				nerrors++;
				fprintf(stderr, " %x %x;", *p1, *p2);
				comparebits(*p1, *p2);
				p1++, p2++;
			}
			fprintf(stderr, " 0x%x bits went 0, 0x%x went 1\n",
			        nzero, nones);
			if (exit_on_fail)
				exit(1);
			else
				return(1);
		}
		seekpt++;
	}
	return 0;
}

void
usage(char *name)
{
	fprintf(stderr,
		"Usage: %s [-d] [-r] [-s size] [-i niterations] [-b rwsize ] -f file\n",
		name);
}
