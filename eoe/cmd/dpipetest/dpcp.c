/*
 * dpcp.c
 * Usage: dpcp file1 file2
 * This program accepts two arguments which are file names. file1 is
 * the source, file2 is the target.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/uio.h>
#include <dpipe.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

main(int argc, char **argv) {
	char           *source;            /* file name */
	char           *target;            /* file name */
	int            src, tgt;            /* source and destination file 
					       descriptors */
	int            pfd;                 /* pipe file descriptor */
	struct stat    buf;
	dpipe_lib_hdl_t src_h, tgt_h;  /* handlers for src and dest */
	struct dpipe_fspe_bind_list iov;
	dpipe_fspe_ctx_t fiov;
	__int64_t      trans_id;
	int            status;
	

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

	if (fstat(src, &buf) < 0) {
		perror ("fstat of source fail");
		exit(1);
	}

	src_h = dpipe_fspe_get_hdl(src);
	if (src_h == NULL) {
		perror("source handler can't be allocated");
		exit(1);
	}
	
	iov.offset = 1000;
	iov.size = buf.st_size - iov.offset;
	fiov.iovcnt = 1;
	fiov.iov = &iov;
	if (dpipe_fspe_set_ctx(src_h, fiov) < 0) {
		perror ("dpipe_fspe_set_ctx(source) fail");
		exit(1);
	}

	iov.offset = 0;
	tgt_h = dpipe_fspe_get_hdl(tgt);
	if (tgt_h == NULL) {
		perror("dest handler can't be allocated");
		exit(1);
	}

	if ( dpipe_fspe_set_ctx(tgt_h, fiov) < 0) {
		perror ("dpipe_fspe_set_ctx(target) fail");
		exit(1);
	}

	if ((trans_id = dpipeTransfer(pfd, src_h, tgt_h)) < 0) {
		perror ("dpipe transfer fail");
		exit(1);
	}

	while((status = dpipeStatus(pfd, trans_id)) != DPIPE_TRANS_COMPLETE) {
		switch(status) {
		      case DPIPE_TRANS_PENDING:
			printf("transfer is pending.\n");
			break;
		      case DPIPE_TRANS_CANCELLED:
			printf("transfer is stopped.\n");
			goto done;
		      case DPIPE_TRANS_ERROR:
			printf("transfer failed.\n");
			goto done;
		}
	}

/*	dpipeFlush(pfd);*/
	if (status == DPIPE_TRANS_COMPLETE)
	  printf("transfer completed.\n");

      done:
	dpipeDestroy(pfd);
	close(src);
	close(tgt);

	return 0;
}

	
