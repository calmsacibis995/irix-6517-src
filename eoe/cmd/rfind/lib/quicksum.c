#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "fsdump.h"
#include "fenv.h"

#define INT(p) (*(int *)(p))

/*
 * Computes a sparse checksum, of just the first couple ints
 * from each basic block (512 bytes).  Intended to
 * help track down some data corruption that results
 * in misplaced basic blocks.
 */

char *quicksum (char *path) {
	int fd = -1;			/* open file descriptor on file to sum */
	struct stat64 sbuf;		/* stat of file to sum */
	static char rbuf[32];		/* build ASCII string representation of qsum here */
	char *p;			/* scans blocks of file being sum'd */
	uint64_t qsum = 0;			/* sum of blocks scanned so far */
	char *mp = (char *)MAP_FAILED, *mpend;	/* mmap'd buffer (start + end) of file to sum */

	if ((fd = open (path, O_RDONLY)) < 0)
		goto fail;
	if (fstat64 (fd, &sbuf) < 0)
		goto fail;
	if (sbuf.st_size == 0)
		goto fail;
	mp = (char *)mmap((void *)0, sbuf.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (mp == (char *)MAP_FAILED)
		goto fail;

	qsum = hash (sbuf.st_ino);
	mpend = mp + sbuf.st_size;
	for (p = mp; p < mpend; p += BLOCKSIZE)
		qsum = hash (qsum + INT(p) + 2*INT(p+sizeof(int)));

fail:
	sprintf(rbuf, "%lld", qsum);
	if (mp != (char *)MAP_FAILED) {
		if (munmap (mp, sbuf.st_size) < 0)
			error ("munmap");
	}
	if (fd >= 0)
		close (fd);
	return rbuf;
}
