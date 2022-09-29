#ident "$Header: /proj/irix6.5.7m/isms/eoe/lib/libdpipe/src/RCS/dpipe_lib.c,v 1.13 1996/11/25 19:33:45 lguo Exp $"

/*
 * This file provides API for data pipe in SpeedRacer.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <dpipe.h>
#include <strings.h>                        /* bcopy */
#include <sys/syssgi.h>                     /* SGI_DPIPE_FSPE_BIND */
#include <sys/fsid.h>                       /* fs type identifiers */

static int get_ops(int fd, __uint64_t *ops) 
{
	struct stat buf;
	struct statvfs statbuf;
	
	if (fstat(fd, &buf) < 0)
	  return -1;
	if (S_ISREG(buf.st_mode)) {
		fstatvfs(fd, &statbuf);
		if (strcasecmp(statbuf.f_basetype, FSID_XFS) &&
		    strcasecmp(statbuf.f_basetype, FSID_EFS) &&
		    strcasecmp(statbuf.f_basetype, FSID_NFS) &&
		    strcasecmp(statbuf.f_basetype, FSID_NFS2) &&
		    strcasecmp(statbuf.f_basetype, FSID_NFS3)) {
			fprintf(stderr, 
				"Error: only xfs, efs, nfs, and nfs3 support "
			       "data pipe\n");
			errno = EOPNOTSUPP;
			return -1;
		}

		if (fcntl(fd, F_GETOPS, ops) < 0)
		  return -1;
	} else {
		if (ioctl(fd, DPIOCGETOPS, ops) < 0)
		  return -1;
	}
	
	return 0;
}

/*
 * dpipeCreate: return file descriptor for the pipe.
 *      source_fd: source file descriptor;
 *      sink_fd: sink file descriptor.
 * 
 * ERRORS:
 *      EDEVNOPIPE: one or more of the devices does not support
 *                  data pipe functions
 *      EMISMATCH:  file descriptors not bus master and slave
 */
int dpipeCreate(int source_fd, int sink_fd)
{
	int pipefd;
	dpipe_create_ioctl_t create_info;
	
	if ((pipefd = open("/dev/dpipe", O_RDONLY)) < 0)
	   return -1;
	
	create_info.src_fd = source_fd;
	create_info.sink_fd = sink_fd;
	if (get_ops(source_fd, &(create_info.dpipe_src_ops)) < 0) {
		printf("get_ops source fail\n");
		return -1;
	} 
	if (get_ops(sink_fd, &(create_info.dpipe_sink_ops)) < 0) {
		printf("get_ops sink fail\n");
	     return -1;
	} 
	if (ioctl(pipefd, DPIPE_CREATE, &create_info) < 0) { 
		close (pipefd);
		return -1;
	}
	
	return pipefd;
}


/* dpipeDestroy: destroy a pipe connection
 *
 * ERRORS:
 *      ENOPIPE: the file descriptor is not a data pipe
 */
int dpipeDestroy(int pipefd)
{

	if (close(pipefd) < 0)
	  return -1;
	else 
	  return 0;
}

/* dpipeTransfer
 * Argument: file descriptor of the data pipe,
 *           source handler, sink handler.
 * Return : >=0 successful and the return value is the transfer id
 *          -1  failure 
 */
__int64_t 
dpipeTransfer(int pipefd, dpipe_lib_hdl_t source_hdl, 
			dpipe_lib_hdl_t sink_hdl) 
{
	dpipe_pretrans_ioctl_t pretrans;
        dpipe_transfer_ioctl_t transfer;
	
	/* Get the datapipe id and transfer id.
	 * In buffered pipe case, two pipes are created so each pipefd
	 * has two corresponding pipeids, source and sink.
	 */
	if (ioctl(pipefd, DPIPE_PRETRANS, &pretrans) < 0)
	    return -1;

	/* source bind transfer */
	if ((transfer.src_cookie = (source_hdl->bind_transfer) 
	    (source_hdl, pretrans.src_pipe_id, pretrans.transfer_id,
	     DPIPE_SOURCE_CAPABLE)) == (__uint64_t)(-1)) 
		return -1;

	/* sink bind transfer */
	if ((transfer.sink_cookie = (sink_hdl->bind_transfer)
	  (sink_hdl, pretrans.sink_pipe_id, pretrans.transfer_id,
	   DPIPE_SINK_CAPABLE)) == (__uint64_t)(-1)) 
		return -1;

	/* start the transfer */
	transfer.transfer_id = pretrans.transfer_id;
	if (ioctl(pipefd, DPIPE_TRANSFER, &transfer) < 0)
	  return -1;
	
	return pretrans.transfer_id;
}

int dpipeStop(int pipefd, __int64_t transfer_id)
{
	if (ioctl(pipefd, DPIPE_STOP, &transfer_id) < 0)
	  return -1;
	
	return 0;
}

/* dpipeReset
 * Argument: file descriptor of the datapipe
 * Return: 0 successful
 *        -1 fail and errno is set
 */
int dpipeReset(int pipefd) {
	
	if (ioctl(pipefd, DPIPE_RESET, &pipefd) < 0)
	   return -1;
	 return 0;
}

/* dpipeStatus
 * checks the current status of a transfer
 * 
 * Return: -1 fail and errno is set
 *         non-negative: see sys/dpipe.h or datapipe(3x)
 */
 int dpipeStatus(int pipefd, __int64_t tag)
{
	if (ioctl(pipefd, DPIPE_STATUS, &tag) < 0)
	  return -1;
	 return 0;
}

int dpipeCancel(int pipefd, __int64_t tag) 
{
	if (ioctl(pipefd, DPIPE_CANCEL, &tag) < 0)
	   return -1;
	return 0;
}

/* dpipeFlush
 * doesn't return until all the transfers within this data pipe
 * are done.
 * 
 * Return: 0 success
 *         -1 failure and errno is set.
 */
int dpipeFlush(int pipefd)
{
	if (ioctl(pipefd, DPIPE_FLUSH, NULL) < 0)
	  return -1;
	return 0;
}

/* The following functions play the file system pipe end role.
 * It is here now because it's still in the early development stage
 * and we don't want to make revisions to other relatively stable
 * piece of code in the system. 
 */

dpipe_fspe_hdl_t *fspe_hdl_table = NULL;

/* ARGSUSED */
dpipe_end_trans_ctx_hdl_t 
dpipe_fspe_bind_transfer(dpipe_lib_hdl_t ctx, int pipe_id, 
			 __int64_t xfer_id, int role)
{
	dpipe_fspe_hdl_t *fh = (dpipe_fspe_hdl_t *)ctx;
	struct sgi_dpipe_fspe_bind arg;
	
	if (fh->fd < 0) {
		errno = EBADFD;
		return -1;
	}

	if (fh->ctx == NULL || fh->ctx->iov == NULL) {
		errno = EINVAL;
		return -1;
	}

	/* set the handle to file descriptor table */
	arg.pipe_id = pipe_id;
	arg.transfer_id = xfer_id;
	arg.role = role;
	arg.iovcnt = fh->ctx->iovcnt;
	arg.sglist = (__uint64_t)fh->ctx->iov;
	if (syssgi(SGI_DPIPE_FSPE_BIND, &arg) < 0)
	  return -1;

	return 0;
}

dpipe_lib_hdl_t dpipe_fspe_get_hdl(int fd)
{
	dpipe_fspe_hdl_t *fh;

	fh = (dpipe_fspe_hdl_t *) malloc (sizeof(dpipe_fspe_hdl_t));
	if (fh == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	fh->bind_transfer = dpipe_fspe_bind_transfer;
	fh->ctx = NULL;
	fh->fd = fd;
	if (fspe_hdl_table == NULL) {
		if ((fspe_hdl_table = (dpipe_fspe_hdl_t *) 
		     malloc (sizeof (dpipe_fspe_hdl_t))) == NULL) {
			free(fh);
			return NULL;
		}
		fh->next = NULL;
		fspe_hdl_table = fh;
	} else {
		fh->next = fspe_hdl_table;
		fspe_hdl_table = fh;
	}

	return (dpipe_lib_hdl_t)fh;
}

int dpipe_fspe_set_ctx (dpipe_lib_hdl_t hdl, dpipe_fspe_ctx_t ctx)
{
	dpipe_fspe_hdl_t *fh = (dpipe_fspe_hdl_t *)hdl;
	struct dpipe_fspe_bind_list *iov;
	dpipe_fspe_ctx_t *newctx;
	int i;

	if (ctx.iovcnt <= 0 || ctx.iov == 0) {
		fprintf(stderr, "ctx.iovcnt is %d, ctx.iov is 0x%x\n",
		       ctx.iovcnt, ctx.iov);
		errno = EINVAL;
		return -1;
	}

	for (i = 0; i < ctx.iovcnt; i++) {
		if ((ctx.iov[i].offset < 0) || (ctx.iov[i].size <= 0)) {
			fprintf(stderr, "iov %d has wrong setting: "
				"offset %lld\tsize %lld\n",
				i, ctx.iov[i].offset, ctx.iov[i].size);
			errno = EINVAL;
			return -1;
		}
	}

	newctx = (dpipe_fspe_ctx_t *) malloc (sizeof(dpipe_fspe_ctx_t));
	if (newctx == NULL) {
		errno = ENOMEM;
		return -1;
	}

	iov = (struct dpipe_fspe_bind_list *) malloc 
	  (ctx.iovcnt * sizeof (struct dpipe_fspe_bind_list));
	if (iov == NULL) {
		free(newctx);
		errno = ENOMEM;
		return -1;
	}

	if ((fh->ctx != NULL) && (fh->ctx->iov != NULL))
		free(fh->ctx->iov);
	newctx->iovcnt = ctx.iovcnt;
	bcopy(ctx.iov, iov, ctx.iovcnt * sizeof (struct dpipe_fspe_bind_list));
	newctx->iov = iov;
	fh->ctx = newctx;
	return 0;
}
