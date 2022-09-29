/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1994, Silicon Graphics, Inc.                  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.10 $"

/*
 * plex_mmap_write - XLV support routines for handling memory mapped
 * writes to plexes.
 *
 * When xlv gets a write-request for a memory-mapped file, it must first
 * make a private copy of the pages. This is to guard against the user
 * modifying the memory while I/O is in progress. If this were not done,
 * it might be possible for us to get inconsistent data among the plexes.
 *
 * Since the amount of data passed to us may be as large as max_dma,
 * we have to do the copy in chunks. Otherwise, the system could deadlock
 * as we try to kmem_alloc the additional memory as bdflush is trying
 * to push the pages out to an xlv subvolume.
 *
 * The basic strategy is to issue the write in chunks. The initial chunk
 * will be issued directly with subsequent chunks issued by the xlvd.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_lock.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"


extern zone_t  *xlv_io_context_zone;

unsigned xlv_max_mmap_write_bytes = 128 << BBSHIFT;	/* 64K bytes */

int
xlv_mmap_write_init (
	buf_t	*original_bp,	/* original buf passed to xlv. */
	buf_t	**new_bp)
{
	buf_t		*bp;
	iovec_t         iov;
	uio_t		uio;
	unsigned	bytes_to_xfer;
	caddr_t		data_buffer;
	int		error;

	ASSERT (original_bp->b_flags & (B_PAGEIO | B_WRITE));

	bytes_to_xfer = original_bp->b_bcount < xlv_max_mmap_write_bytes ?
                        original_bp->b_bcount : xlv_max_mmap_write_bytes;

	/* XXX Change this to nosleep later. */
	data_buffer = kmem_alloc (bytes_to_xfer, KM_SLEEP);

	bzero (&uio, sizeof(uio_t));
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
        uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = bytes_to_xfer;
	iov.iov_base = data_buffer;
	iov.iov_len = bytes_to_xfer;

	if (error = biomove (original_bp, 0, bytes_to_xfer, UIO_READ, &uio)) {
		ASSERT (0);
		kmem_free (data_buffer, 0);
		return error;
	}

	bp = getrbuf (KM_SLEEP);

	xlv_trace ("XLV_MMAP_INIT", XLV_TRACE_USER_BUF, original_bp);

	bp->b_flags = original_bp->b_flags & ~(B_PAGEIO|B_MAPPED);
        bp->b_edev = original_bp->b_edev;

        bp->b_sort = 0;			/* will be filled in by caller */
        bp->b_error = 0;

        bp->b_pages = NULL;
        bp->b_remain = 0;

        bp->b_blkno = original_bp->b_blkno;
        bp->b_bcount = bytes_to_xfer;
        bp->b_dmaaddr = data_buffer;

	*new_bp = bp;

	return 0;
}


/*
 * Routine called to write out the next chunk of data.
 */
int
xlv_mmap_write_next (
	buf_t		*bp)
{
	xlv_io_context_t	*io_context;
	buf_t			*mmap_bp;
	iovec_t         	iov;
        uio_t           	uio;
        unsigned        	bytes_to_xfer;
	int			error;
	/* REFERENCED */
	int			i;

	xlv_trace ("XLV_MMAP_NEXT", XLV_TRACE_USER_BUF, bp);

	io_context = (xlv_io_context_t *) bp->b_sort;
	mmap_bp = io_context->mmap_write_buf;

	ASSERT (ismrlocked(&(xlv_io_lock[minor(mmap_bp->b_edev)].lock),
                        MR_ACCESS|MR_UPDATE));

	ASSERT (mmap_bp != NULL);

	ASSERT (io_context->original_buf == bp);
        ASSERT ((io_context->state == XLV_SM_MMAP_WRITE) ||
		(io_context->state == XLV_SM_MMAP_WRITE_CONT));

        bytes_to_xfer = mmap_bp->b_bcount - io_context->bytes_transferred;
	if (bytes_to_xfer > xlv_max_mmap_write_bytes)
                bytes_to_xfer = xlv_max_mmap_write_bytes;
	ASSERT (bytes_to_xfer > 0);

        bzero (&uio, sizeof(uio_t));
        uio.uio_iov = &iov;
        uio.uio_iovcnt = 1;
        uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = bytes_to_xfer;
        iov.iov_base = bp->b_dmaaddr;
        iov.iov_len = bytes_to_xfer;


	/* XXX Change this to nosleep later. */

        if (error = biomove (mmap_bp, io_context->bytes_transferred,
		bytes_to_xfer, UIO_READ, &uio)) {

		ASSERT (0);

                kmem_free (bp->b_dmaaddr, 0);

                return error;
	}

#ifdef DEBUG
	/*
	 * XXX To be removed when Kompella's new lock scheme is used.
	 *
	 * Find the first byte that is zero.
	 */
	for (i=0; i < bytes_to_xfer; i++) {
		if (bp->b_dmaaddr[i] == (char)0) {
			xlv_trace ("XLV_MMAP_0", XLV_TRACE_USER_BUF, bp);
			break;
		}
	}
#endif

	bp->b_blkno = mmap_bp->b_blkno +
                        (io_context->bytes_transferred >> BBSHIFT);
	bp->b_bcount = bytes_to_xfer;
        io_context->bytes_transferred += bytes_to_xfer;

	/*
	 * We don't want to process any errors until we are completely
	 * done with the I/O. The errors are being saved in the io_context
	 * so we won't lose them.
	 */
	bp->b_error = 0;
	bp->b_flags = (bp->b_flags & ~(B_DONE|B_ERROR)) | B_BUSY;

	return 0;
}



/*
 * Called by xlv_top_strat_done() whenever an mmap write buffer is
 * completed.
 */
void
xlv_mmap_write_done (
	buf_t		*bp)
{
	xlv_io_context_t	*io_context;
	buf_t			*mmap_bp;

	io_context = (xlv_io_context_t *) bp->b_sort;

        ASSERT (io_context->original_buf == bp);
	ASSERT (ismrlocked(&(xlv_io_lock[minor(bp->b_edev)].lock),
                        MR_ACCESS|MR_UPDATE));

	mmap_bp = io_context->mmap_write_buf;
	ASSERT (mmap_bp);

	if (mmap_bp->b_bcount == io_context->bytes_transferred) {
		/*
		 * We are done!.
		 */

		xlv_trace ("XLV_MMAP_WRITE_FREE", XLV_TRACE_USER_BUF, bp);

		/*
		 * Unconditionally free our copy of the user data.
		 */
                kmem_free (bp->b_dmaaddr, 0);

		freerbuf (bp);

		/*
		 * Now let's switch back to using the original buffer
		 * that the user passed to us.
		 */
		io_context->original_buf = mmap_bp;
		io_context->mmap_write_buf = NULL;


		if (io_context->num_failures == 0) {
			/*
			 * No errors.
			 */

			ASSERT (ismrlocked(
				&(xlv_io_lock[minor(mmap_bp->b_edev)].
				lock), MR_ACCESS|MR_UPDATE));

			/*
			 * Release xlv_io_lock and complete the original
			 * buffer.  This routine will observe the locking
			 * protocol and hang on to the lock if wait_count
			 * is > 1 for those mmap_writes that involved
			 * the xlvd.
			 */
			xlvd_complete_mmap_write_request (io_context);
			
		}
		else {
			/*
			 * We had a write failure. Let the xlvd take over
			 * for error recovery. Since this is a write,
			 * the only thing that xlvd will do is determine
			 * whether we can mask the error & take the
			 * bad volume element(s) offline.
			 * 
			 * Note that xlvd will not retry this request.
			 * This is why we don't need to hang on to the
			 * copy of the buffer any more & can just use
			 * mmap_bp.
			 */

			/*
			 * If the request was done in one chunk, then we
			 * set the new state to XLV_SM_WRITE and handle
			 * it like any write failure.
			 * 
			 * If the request was done in several chunks, then
			 * we set a different state because the locking
			 * is different. (See xlvd_queue_request.)
			 */
			if (io_context->state == XLV_SM_MMAP_WRITE)
				io_context->state = XLV_SM_WRITE;
			else if (io_context->state == XLV_SM_MMAP_WRITE_CONT)
				io_context->state =XLV_SM_MMAP_WRITE_CONT_ERROR;
			else {
				ASSERT (0);
			}

			xlv_trace ("XLV_MMAP_WRT_ERROR", XLV_TRACE_USER_BUF,
				   mmap_bp);
			mmap_bp->b_flags |= B_ERROR;

			xlvd_queue_request (mmap_bp, io_context);
		}
	}
	else {
		/*
		 * Need to copy and write another chunk. Ask xlvd to do it.
		 */
		xlvd_queue_request (bp, io_context);
	}

	
}
