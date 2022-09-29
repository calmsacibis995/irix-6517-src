/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.2 $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#define CHUNKSIZE 1000000
#define NCHUNKS   5

int chunksize	= CHUNKSIZE;
int nchunks	= NCHUNKS;
int sleeptime	= 0;
int iterations	= 3;
int debug;
char *paddr, *p;
int j;

extern char *optarg;
extern int optind, opterr;
extern int errno;

main(argc, argv)
char **argv;
{
	int fd;
	struct stat statbuf;
	register int h, i;
	char *filename;
	char c;

	while ((i = getopt(argc, argv, "s:i:c:n:d")) != EOF)
	switch (i) {
	case 's':
		sleeptime = atoi(optarg);
		break;
	case 'i':
		iterations = atoi(optarg);
		break;
	case 'c':
		chunksize = atoi(optarg);
		break;
	case 'n':
		nchunks = atoi(optarg);
		break;
	case 'd':
		debug++;
		break;
	}

	filename = tempnam(NULL, "ma");

	if (debug) {
		fprintf(stderr, "file %s, %d iterations; chunksize %d, %d chunks; sleep %d seconds after each chunk\n", filename, iterations, chunksize, nchunks, sleeptime);
		exit(0);
	}

	stat(filename, &statbuf);
	if (errno && errno != ENOENT) {
		perror("stat");
		exit(1);
	}

	fd = open(filename, O_RDWR|O_CREAT, 0666);
	if (fd < 0) {
		perror("open");
		unlink(filename);
		exit(1);
	}
	if (lseek(fd, chunksize * nchunks, 0) < 0) {
		perror("lseek");
		unlink(filename);
		exit(1);
	}

	if (write (fd, &c, 1) != 1) {
		perror("write");
		unlink(filename);
		exit(1);
	}

	/* fsync(fd); */
	sync();

	if (unlink(filename)) {
		perror("unlink");
		exit(1);
	}

	paddr = mmap(0, chunksize * nchunks, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);

	if (paddr == (caddr_t) -1L) {
		perror("mmap");
		exit(1);
	}

	for (c = '!', h = iterations; h > 0; h--) {
		for (i = 0; i < nchunks; i++) {
			p = i * chunksize + paddr;
			fprintf(stderr, "poking @ %x for %x.. ",
				p, chunksize / 2);
			fflush(stderr);
			for (j = chunksize / 2; j > 0; j--)
				*p++ = c;
			fprintf(stderr, "madvise(0x%x, 0x%x)\n",
				i * chunksize + paddr, chunksize);
			fflush(stderr);
			if (madvise(i * chunksize + paddr,
					chunksize, MADV_DONTNEED)) {
				perror("madvise DONTREED");
				exit(1);
			}
			sleep(sleeptime);
			c++;
			if (c > '~')
				c = '!';
		}
	}
}
