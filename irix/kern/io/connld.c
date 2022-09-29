/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)uts-comm:io/connld/connld.c	1.10" */

#ident	"$Revision: 1.36 $"

/*
 * This module establishes a unique connection on a STREAMS-based pipe.
 */
#include <fifofs/fifonode.h>
#include <sys/conf.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/vnode.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/strmp.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/idbgactlog.h>

/*
 * Define local and external routines.
 */
static int connopen(queue_t *rqp, dev_t *devp, int flag, int sflag, struct cred *crp);
static int connclose(queue_t *q, int flag, struct cred *crp);
static int connput(queue_t *q, mblk_t *bp);
static struct vnode *connvnode(queue_t *);

/*
 * Define STREAMS header information.
 */
static struct module_info conn_info = {
	1003, 
	"conn", 
	0, 
	INFPSZ, 
	STRHIGH, 
	STRLOW 
};
static struct qinit connrinit = { 
	connput, 
	NULL, 
	connopen, 
	connclose, 
	NULL, 
	&conn_info, 
	NULL 
};
static struct qinit connwinit = { 
	connput, 
	NULL, 
	NULL, 
	NULL, 
	NULL, 
	&conn_info, 
	NULL
};
struct streamtab conninfo = { 
	&connrinit, 
	&connwinit 
};

int conndevflag = 0;

/*
 * For each invokation of connopen(), create a new pipe. One end of the pipe 
 * is sent to the process on the other end of this STREAM. The vnode for 
 * the other end is returned to the open() system call as the vnode for 
 * the opened object.
 *
 * On the first invokation of connopen(), a flag is set and the routine 
 * returns 0, since the first open corresponds to the pushing of the module.
 *
 * NOTE:
 * This procedure is assumed to be called with the STREAMS monitor lock held.
 */
/*ARGSUSED*/
static int
connopen(queue_t *rqp, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	struct vnode *vp1, *vp2;
	struct vnode *streamvp = NULL; 
	struct vfile *filep;
	struct fifonode *fnp1, *fnp2;
	struct fifonode *streamfnp = NULL;
	struct fifonode *matefnp = NULL;
	fifo_lock_t *plp;
	int fd, rvalp;
	int s;
	/* REFERENCED */
	int locked;
	mon_t *mon;
	register int error = 0;
	bhv_desc_t *streambdp;

	if ((streamvp = connvnode(rqp)) == NULL)
		return(EINVAL);
	/*
	 * CONNLD is only allowed to be pushed onto a "pipe" that has both 
	 * of its ends open.
	 */
	if (streamvp->v_type != VFIFO)
		return(EINVAL);
	streambdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(streamvp),
					   &fifo_vnodeops);
	ASSERT(streambdp != NULL);
	streamfnp = BHVTOF(streambdp);
	fifo_lock(streamfnp);
	if (!(streamfnp->fn_flag & ISPIPE) || 
		(streamfnp->fn_mate == NULL)) {
			fifo_unlock(streamfnp);
			return(EPIPE);
	}

	/*
	 * If this is the first time CONNLD was opened while on this stream,
	 * it is being pushed. Therefore, set a flag and return 0.
	 */
	if ((__psint_t)rqp->q_ptr == 0) { 
		rqp->q_ptr = (caddr_t)1L;
		fifo_unlock(streamfnp);
		return (0);
	}
	fifo_unlock(streamfnp);
	/*
	 * Get two vnodes that will represent the pipe ends for the new pipe.
	 */
	if ((fnp1 = makepipe(FWRITE|FREAD)) == NULL) {
		return (ENOMEM);
	}
	vp1 = FTOV(fnp1);
	if ((fnp2 = makepipe(flag)) == NULL) {
		VN_RELE(vp1);
		return (ENOMEM);
	}
	vp2 = FTOV(fnp2);
	/*
	 * Get and link the common fifo lock
	 */
	if ((plp = fifo_cr_lock((long)vp1->v_number)) == NULL) {
		VN_RELE(vp1);
		VN_RELE(vp2);
		return ENOMEM;
	}

	/*
	 * Allocate a file descriptor and file pointer for one of the pipe 
	 * ends. The file descriptor will be used to send that pipe end to 
	 * the process on the other end of this stream.
	 */
	error = vfile_alloc(FWRITE|FREAD, &filep, &fd);

	if (error) {
		VN_RELE(vp1);
		VN_RELE(vp2);
		fifo_del_lock(plp);
		return (error);
	}

	/*
	 * fn_open used to be incremented at the top of fifo_stropen().
	 * That code was moved to fifo_open() to close a race condition,
	 * so we have to do the increments here ourselves.
	 */
	fnp1->fn_open++;
	fnp2->fn_open++;

	/*
	 * Unlock the global streams monitor to avoid deadlock with
	 * the fifo code which also acquires the lock.
	 */
	STRQUEUE_UNLOCK(rqp->q_monpp);
	/*
	 * Create two new stream heads and attach them to the two vnodes for
	 * the new pipe.
	 */
	if (((error = fifo_stropen(&vp1, 0, filep->vf_cred)) != 0) || 
		((error = fifo_stropen(&vp2, 0, filep->vf_cred)) != 0)) {
			if (vp1->v_stream)
				strclose(vp1, 0, filep->vf_cred);
			vfile_alloc_undo(fd, filep);
			VN_RELE(vp1);
			VN_RELE(vp2);
			fifo_del_lock(plp);
			STRQUEUE_LOCK(rqp->q_monpp, locked);
			ASSERT(locked);

			return(error);
	}

	/*
	 * Both stream heads must point to the same (private) monitor.
	 */
	LOCK_STRHEAD_MONP(s);
	mon = str_mon_detach(vp2->v_stream);
	str_privmon_attach(vp2->v_stream, vp1->v_stream->sd_monp);
	UNLOCK_STRHEAD_MONP(s);
	str_mon_free(mon);

	STRQUEUE_LOCK(rqp->q_monpp, locked);
	ASSERT(locked);
	/*
	 * Twist queue pointers so that write queue points to read queue.
	 */
	vp1->v_stream->sd_wrq->q_next = RD(vp2->v_stream->sd_wrq);
	vp2->v_stream->sd_wrq->q_next = RD(vp1->v_stream->sd_wrq);
	/*
	 * Tell each pipe end about its mate.
	 */
	fnp1->fn_mate = fnp2;
	fnp2->fn_mate = fnp1;
        /*
	 * Share the common pipe lock for synchronization
	 */
	fnp1->fn_lockp = fnp2->fn_lockp = plp;
	plp->fn_refcnt = 2;
	LOG_ACT(AL_FLREFINC, plp, plp->fn_refcnt, 0);

	/*
	 * Send one end of the new pipe to the process on the other 
	 * end of this pipe and block until the other process received it.
	 * If the other process exits without receiving it, fail this open
	 * request.
	 */
	fifo_lock(streamfnp);
	if (streamfnp->fn_mate == NULL) {
		error = ENXIO;
		goto out;
	}
	matefnp = streamfnp->fn_mate;
	ASSERT(matefnp);
	matefnp->fn_flag |= FIFOSEND;

	STRQUEUE_UNLOCK(rqp->q_monpp);
	/*
	 * as soon as we mark this 'ready' some other share group member
	 * sharing fds could find it. The most important thing to do
	 * is to keep them from closing it! We let I_SENDFD handle this error.
	 */
	vfile_ready(filep, vp1);
	fifo_unlock(streamfnp);
	error = strioctl(streamvp, I_SENDFD, (void *)(__psint_t)fd,
			 flag, K_TO_K, filep->vf_cred, &rvalp);

	/* relock streams now that strioctl is finished */
	STRQUEUE_LOCK(rqp->q_monpp, locked);

	fifo_lock(streamfnp);
	if (error != 0) {
		goto out;
	}

	while (matefnp->fn_flag & FIFOSEND) {

		if (sv_wait_sig(&matefnp->fn_ioctl, PPIPE,
			       &(matefnp->fn_lockp->fn_lock), 0) < 0)
		{
			/*
			 * no lock (note streamfnp and matefnp share
			 * same lock)
			 */
			fifo_lock(streamfnp);
			error = ENXIO;
			goto out;
		}

		fifo_lock(streamfnp);
		if (streamfnp->fn_mate == NULL) {
			error = ENXIO;
			goto out;
		}
	}
	/*
	 * all is okay...return new pipe end to user
	 */
	streamfnp->fn_unique = vp2;
	streamfnp->fn_flag |= FIFOPASS;

	/*
	 * Since vfile_ready has been done, there's a potential race with
	 * sprocs/threads sharing fd's.  One might have closed and 
	 * invalidated the fd already.  closefd will re-validate 'fd'.
	 */ 
	fifo_unlock(streamfnp);
	/* might call fifo_close and go down stream */
	STRQUEUE_UNLOCK(rqp->q_monpp);
	if (closefd(fd, &filep) == 0)
		vfile_close(filep);

	STRQUEUE_LOCK(rqp->q_monpp, locked);
	ASSERT(locked);
	return 0;
out:
	streamfnp->fn_unique = NULL;
	streamfnp->fn_flag &= ~FIFOPASS;
	/* we can get here without matefnp being set */
	if (matefnp)
		matefnp->fn_flag &= ~FIFOSEND;
	fifo_unlock(streamfnp);
	STRQUEUE_UNLOCK(rqp->q_monpp);
	fifo_close(FTOBHV(fnp2), flag, L_TRUE, filep->vf_cred);

	VN_RELE(vp2);
	/*
	 * Since vfile_ready has been done, there's a potential race with
	 * sprocs/threads sharing fd's.  One might have closed and 
	 * invalidated the fd already.  closefd will re-validate 'fd'.
	 */
	if (closefd(fd, &filep) == 0)
		vfile_close(filep);

	STRQUEUE_LOCK(rqp->q_monpp, locked);
	ASSERT(locked);
	return(error);
}

/*ARGSUSED*/
static int
connclose(queue_t *q, int flag, struct cred *crp)
{
	return (0);
}

/*
 * Use same put procedure for write and read queues.
 */
static	int
connput(queue_t *q, mblk_t *bp)
{
	putnext(q, bp);
	return (0);
}

/*
 * Get the vnode for the stream connld is push onto. Follow the 
 * read queue until the stream head is reached. The vnode is taken 
 * from the stdata structure, which is obtained from the q_ptr field 
 * of the queue.
 */
static struct vnode *
connvnode(queue_t *qp)
{
	queue_t *tempqp;
	struct vnode *streamvp = NULL;
	
	for(tempqp = qp; tempqp->q_next; tempqp = tempqp->q_next)
		;
	if (tempqp->q_qinfo != &strdata)
		return (NULL);
	streamvp = ((struct stdata *)(tempqp->q_ptr))->sd_vnode;
	return (streamvp);
}
