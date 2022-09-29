/*
 * dpcp.c
 * Usage: dpcp file1 file2
 * This program accepts two arguments which are file names. file1 is
 * the source, file2 is the target.
 */

#include <stdio.h>
#include <sys/uio.h>
#include <dpipe.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

main(int argc, char **argv) {
	char *source;
	char *target;
	int src, tgt;
	int pfd;
	struct dpipelist iov;
	struct stat buf;
	struct dpipexfer xfer;
	int src_c, tgt_c;

	if (argc != 3) {
		printf("Usage: dpcp source target\n");
		exit(1);
	}

	source = argv[1];
	target = argv[2];

	if ((src = open(source, O_RDONLY)) < 0) {
		perror("source can't open");
		exit(1);
	}

	/* need preallocation for target file? */
	if ((tgt = open (target, O_WRONLY | O_CREAT)) < 0) {
		perror ("target can't open");
		exit(1);
	}

	/* create pipe */
	if ((pfd = dpipeCreate(src, tgt)) < 0) {
		perror ("pipe create failure");
		exit(1);
	}

	/* create cookies */
	if (fstat(src, &buf) < 0) {
		perror ("fstat of source fail");
		exit(1);
	}
	iov.offset = 0;
	iov.size = buf.st_size;
	xfer.iovcnt = 1;
	xfer.iov = (__uint64_t)&iov;
	xfer.fd = src;
	if (fcntl(src, F_SETTRANSFER, &xfer) < 0) {
		perror ("fcntl F_SETTRANSFER source fail");
		exit(1);
	}
	src_c = xfer.cookie;

	xfer.fd = tgt;
	if (fcntl(tgt, F_SETTRANSFER, &xfer) < 0) {
		perror ("fcntl F_SETTRANSFER target fail");
		exit(1);
	}
	tgt_c = xfer.cookie;

	if (dpipeTransfer(pfd, src_c, tgt_c) < 0) {
		perror ("dpipe transfer fail");
		exit(1);
	}

	dpipeFlush(pfd);
	dpipeDestroy(pfd);
	close(src);
	close(tgt);
}

	
