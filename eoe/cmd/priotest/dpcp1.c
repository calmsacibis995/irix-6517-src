/*
 * dpcp.c
 * Usage: dpcp file1 file1iov file2 file2iov
 * This program accepts four arguments which are source filename,
 * source iov description file name, sink filename, sink iov 
 * description file name. It's used for testing data pipe transfers.
 *
 * format of the iov file:
 * number of iovs
 * offset, size
 * offset, size
 * ....
 */

#include <stdio.h>
#include <sys/uio.h>
#include <dpipe.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

main(int argc, char **argv) {
	char *source, *sourceiov;
	char *target, *targetiov;
	int src, tgt;
	FILE *srci, *tgti;
	int pfd;
	struct dpipelist *srclist, *tgtlist;
	struct dpipexfer xfer;
	int src_c, tgt_c;
	int i;

	if (argc != 5) {
		printf("Usage: dpcp source target\n");
		exit(1);
	}

	source = argv[1];
	sourceiov = argv[2];
	target = argv[3];
	targetiov = argv[4];

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

	/* source */
	if ((srci = fopen(sourceiov, "r")) == NULL) {
		perror ("sourceiov can't open");
		exit(1);
	}

	fscanf(srci, "%d", &(xfer.iovcnt));
	if ((srclist = (struct dpipelist *) malloc (xfer.iovcnt * sizeof (struct dpipelist))) == NULL) {
		printf("no memory\n");
		exit(1);
	}
	for (i = 0; i < xfer.iovcnt; i++) {
		fscanf(srci, "%ld", &(srclist[i].offset));
		fscanf(srci, "%ld", &(srclist[i].size));
	}
	fclose(srci);

	xfer.iov = srclist;
	xfer.fd = src;
	if (fcntl(src, F_SETTRANSFER, &xfer) < 0) {
		perror ("fcntl F_SETTRANSFER source fail");
		exit(1);
	}
	src_c = xfer.cookie;

	/* target */
	if ((tgti = fopen(targetiov, "r")) == NULL) {
		perror ("targetiov can't open");
		exit(1);
	}

	fscanf(tgti, "%d", &(xfer.iovcnt));
	if ((tgtlist = (struct dpipelist *) malloc (xfer.iovcnt * sizeof (struct dpipelist))) == NULL) {
		printf("no memory\n");
		exit(1);
	}

	for (i = 0; i < xfer.iovcnt; i++) {
		fscanf(srci, "%ld", &(tgtlist[i].offset));
		fscanf(srci, "%ld", &(tgtlist[i].size));
	}
	fclose(tgti);

	xfer.iov = tgtlist;
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

	dpipeDestroy(pfd);
	close(src);
	close(tgt);
	free(srclist);
	free(tgtlist);
}

	
