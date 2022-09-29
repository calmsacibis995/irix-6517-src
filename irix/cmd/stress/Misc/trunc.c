#include "math.h"
#include "time.h"
#include "stdlib.h"
#include "wait.h"
#include "errno.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#define BSZ 0x42000
#define SEEKMASK 0x3ffff
#define WRITEMASK 0x1fff
#define CHARMASK 0x3f
#define SLEEPMASK 0x1

char inbuf[BSZ+1];
char outbuf[BSZ+1];
int verbose;

int
main(int argc, char **argv)
{
	int fd;
	ssize_t writesz, readsz, truncsz;
	off_t result, seekpoint;
	register int iterations = 1000;
	register ssize_t i;
	char *filename;
	char *in, *out;
	char c;

	if (argc > 1)
		iterations = atoi(argv[1]);
	
	if (argc > 2)
		verbose = 1;

	srandom(time(0));

	filename = tempnam(NULL, "seekr");
	fd = open(filename, O_CREAT|O_TRUNC|O_RDWR, 0666);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while (--iterations >= 0) {
		seekpoint = random() & SEEKMASK;
		result = lseek(fd, seekpoint, SEEK_SET);
		if (result != seekpoint) {
			perror("lseek");
			unlink(filename);
			exit(1);
		}

		writesz = random() & WRITEMASK;
		if (writesz + seekpoint > BSZ)
			writesz = BSZ - seekpoint;

		c = (char)(random() & CHARMASK) + 0x20;

		out = outbuf + seekpoint;
		i = writesz;
		while (--i >= 0)
			*out++ = c;

		if (verbose)
			fprintf(stderr,
				"writing %d bytes starting at %d\n",
				writesz, seekpoint);

		result = write(fd, outbuf + seekpoint, writesz);
		if (result != writesz) {
			perror("write");
			unlink(filename);
			exit(1);
		}

		sleep((unsigned int)random() & SLEEPMASK);

		result = lseek(fd, 0, SEEK_SET);
		if (result != 0) {
			perror("lseek");
			unlink(filename);
			exit(1);
		}
		readsz = read(fd, inbuf, BSZ);
		if (verbose)
			fprintf(stderr, "read %d bytes\n", readsz);
		out = outbuf;
		in = inbuf;

		for (i = 0; i < readsz; i++) {
			if (*out != *in) {
				int rfd;
				fprintf(stderr, "data mismatch at %d\n", i);
				filename = tempnam(NULL, "seekw");
				rfd = open(filename,
					   O_CREAT|O_TRUNC|O_RDWR, 0666);
				if (rfd < 0) {
					perror("open compare file");
					exit(1);
				}
				fprintf(stderr, "dumping to compare file %s\n",
					filename);
				result = write(rfd, outbuf, readsz);
				if (result != readsz) {
					perror("write compare file");
					unlink(filename);
				}

				exit(1);
			}
			out++, in++;
		}

		truncsz = random() & SEEKMASK;
		if (truncsz < readsz) {
			if (ftruncate(fd, truncsz)) {
				perror("truncate");
				unlink(filename);
				exit(1);
			}
			if (verbose)
				fprintf(stderr,
					"truncated to %d bytes\n", truncsz);

			out = outbuf + truncsz;
			i = BSZ - truncsz;
			while (--i >= 0)
				*out++ = 0;
		}

		if (random() & 1) {
			close(fd);
			fd = open(filename, O_RDWR);
			if (fd < 0) {
				perror("open");
				unlink(filename);
				exit(1);
			}
		}

	}

	unlink(filename);
	return 0;
}
