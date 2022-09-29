/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/fifofs/fifovnops.c	1.18"*/
#ident	"$Revision: 1.99 $"

/*
 * FIFOFS file system vnode operations.  This file system
 * type supports STREAMS-based pipes and FIFOs.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/flock.h>
#include <sys/sema.h>
#include <sys/signal.h>
#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/kthread.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pda.h>
#include <sys/immu.h>
#include <sys/mac_label.h>
#include <fs/fifofs/fifonode.h>
#include <namefs/namenode.h>

/*
 * Define the routines/data structures external to this file.
 */
extern	dev_t	fifodev;

extern mutex_t		fifoallocmon;
extern sv_t		fifowaitclose;
extern vnodeops_t 	fifo_vnodeops;
extern struct zone	*fifozone;

struct  streamtab fifoinfo = { &strdata, &stwdata, NULL, NULL };

/*
 * Open and stream a FIFO.
 * If this is the first open of the file (FIFO is not streaming),
 * initialize the fifonode and attach a stream to the vnode.
 */
int
fifo_open(bhv_desc_t *bdp, struct vnode **vpp, mode_t flag, struct cred *crp)
{
	struct vnode *vp = BHV_TO_VNODE(bdp);
	register struct fifonode *fnp = BHVTOF(bdp);
	register int error = 0;
	int firstopen;
	dev_t pdev = 0;
	struct stdata *stp;
	struct queue *wqp;
	bhv_desc_t *newbdp;
	int rwakeup = 0;
	int wwakeup = 0;

	flag &= ~FCREAT;		/* paranoia */
	fifo_lock(fnp);

	if ((flag & FREAD) && (fnp->fn_numr++ == 0) &&
	    sv_waitq(&fnp->fn_rwait)) {
		rwakeup = 1;
	}

	if (flag & FWRITE) {
		if ((flag & (FNDELAY|FNONBLOCK)) && fnp->fn_numr == 0) {
			fifo_unlock(fnp);
			return (ENXIO);
		}
		if ((fnp->fn_numw++ == 0) && sv_waitq(&fnp->fn_wwait)) {
			wwakeup = 1;
		}
	}

	/*
	 * Before potentially releasing fifo_lock, we bump fn_open.
	 * This closes a race where we go to sleep waiting for writer,
	 * the writer open/write/closes before we run again. We don't
	 * want the writer to blow away the pipe.
	 * Also, this guarentees that noone will blow away the pipe no
	 * matter what.
	 * On an interrupted open, we must call fifo_close() since we
	 * might be the last one using it
	 */
	fnp->fn_open++;

	if ((flag & FREAD) && (fnp->fn_numw == 0)) {
		if (flag & (FNDELAY|FNONBLOCK))
			goto str;
		/*
		 * wait for writer - note that once one writer
		 * shows up, everyone waiting will be woken
		 * since we all vsema
		 */
		if (sv_wait_sig(&fnp->fn_wwait, PPIPE|PCATCH,
				&fnp->fn_lockp->fn_lock,0)) {
			/* was interrupted */
			(void)fifo_close(bdp, flag, L_TRUE, crp);
			return (EINTR);
		}
		fifo_lock(fnp);
	}

	if ((flag & FWRITE) && (fnp->fn_numr == 0)) {
		if (sv_wait_sig(&fnp->fn_rwait, PPIPE|PCATCH,
				&fnp->fn_lockp->fn_lock,0)) {
			/* was interrupted */
			(void)fifo_close(bdp, flag & FMASK, L_TRUE, crp);
			return (EINTR);
		}
		fifo_lock(fnp);
	}

str:
	/*
	 * make sure we don't run into someone closing (remember, closers
	 * must release fifo_lock also)
	 * This could have happened since we had to release fifo_lock
	 * while waiting for a reader/writer
	 */
	while (fnp->fn_flag & FIFOCLOSING) {
		sv_wait(&fifowaitclose, PZERO, &(fnp->fn_lockp->fn_lock), 0);
		fifo_lock(fnp);
	}

	ASSERT(fnp->fn_open > 0);

	while (fnp->fn_flag & FIFOWOPEN) {
		if (flag & (FNDELAY|FNONBLOCK)) {
			if (flag & FREAD)
				fnp->fn_numr--;
			if (flag & FWRITE)
				fnp->fn_numw--;
			fnp->fn_open--;
			fifo_unlock(fnp);
			return (EAGAIN);
		}
		if (sv_wait_sig(&fnp->fn_openwait, PPIPE|PCATCH,
		    &fnp->fn_lockp->fn_lock,0)) {
			(void)fifo_close(bdp, flag & FMASK, L_TRUE, crp);
			return (EINTR);
		}
		fifo_lock(fnp);
	}
	fnp->fn_flag |= FIFOWOPEN;

	/*
	 * If successful stream and first open, twist the queues.
	 * We rely on fnp's lock to keep other threads from chasing
	 * the stream queues.  As this is first open, no one else
	 * can get at our stream.
	 * If this is an open of a fifo with CONNLD on it, then we may
	 * end up with a new vp at the end of this - we need to know
	 * whether this happens so we can do the proper unlocking.
	 * So rather than call fifo_stropen, call stropen directly..
	 */
	firstopen = (vp->v_stream == NULL);

	fifo_unlock(fnp);

	if ((error = stropen(vp, &pdev, NULL, flag, crp)) != 0) {
		(void)fifo_close(bdp, flag, L_TRUE, crp);
		fifo_lock(fnp);
		fnp->fn_flag &= ~FIFOWOPEN;
		if (sv_waitq(&fnp->fn_openwait)) {
			fifo_unlock(fnp);
			sv_signal(&fnp->fn_openwait);
		}
		else
			fifo_unlock(fnp);

		return (error);
	}

	fifo_lock(fnp);

	STRHEAD_LOCK(&vp->v_stream, stp);
	if (firstopen)
		stp->sd_wrq->q_next = RD(stp->sd_wrq);
	/*
	 * If the vnode was switched (connld on the pipe), return the
	 * new vnode (in fn_unique field) to the upper layer and
	 * release the old/original one.
	 */
	if (fnp->fn_flag & FIFOPASS) {
		*vpp = fnp->fn_unique;
		fnp->fn_flag &= ~FIFOPASS;
		fnp->fn_flag &= ~FIFOWOPEN;
		if (sv_waitq(&fnp->fn_openwait)) {
			fifo_unlock(fnp); /* unlock old fifonode */
			sv_signal(&fnp->fn_openwait);
		}
		else
			fifo_unlock(fnp); /* unlock old node*/

		STRHEAD_UNLOCK(stp);

		(void) fifo_close(bdp, flag & FMASK, L_TRUE, crp);
		VN_RELE(vp);

		/*
		 * Find the fifo behavior descriptor for this vnode.
		 */
		newbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(*vpp),
						&fifo_vnodeops);
		ASSERT(newbdp != NULL);
		fnp = BHVTOF(newbdp);
		fifo_lock(fnp);		/* lock the new */

		STRHEAD_LOCK(&(*vpp)->v_stream, stp);
	}
	/*
	 * Set up the stream head in order to maintain compatibility.
	 * Check the hi-water, low-water and packet sizes to ensure
	 * the user can at least write PIPE_BUF bytes to the stream
	 * head and that a message at least PIPE_BUF bytes can be
	 * packaged and placed on the stream head's read queue
	 * (atomic writes).
	 */
	stp->sd_flag |= OLDNDELAY;
	wqp = stp->sd_wrq;
	if (wqp->q_hiwat < PIPE_BUF) {
		wqp->q_hiwat = PIPE_BUF;
		RD(wqp)->q_hiwat = PIPE_BUF;
	}
	wqp->q_lowat = PIPE_BUF -1;
	RD(wqp)->q_lowat = PIPE_BUF -1;

	if (wqp->q_minpsz > 0) {
		wqp->q_minpsz = 0;
		RD(wqp)->q_minpsz = 0;
	}
	if (wqp->q_maxpsz < PIPE_BUF) {
		wqp->q_maxpsz = PIPE_BUF;
		RD(wqp)->q_maxpsz = PIPE_BUF;
	}
	STRHEAD_UNLOCK(stp);

	fnp->fn_flag &= ~FIFOWOPEN;
	if (sv_waitq(&fnp->fn_openwait)) {
		fifo_unlock(fnp);
		sv_signal(&fnp->fn_openwait);
	} else
		fifo_unlock(fnp);

	if (rwakeup) {
		sv_broadcast(&fnp->fn_rwait);
	}
	if (wwakeup) {
		sv_broadcast(&fnp->fn_wwait);
	}

	return (error);

}

/*
 * Close down a fifo stream.
 * We call strclean() on every close.
 * If last close (count is <= 1 and fifo open count is 1 then we
 * call strclose() to close down the stream.
 *
 * If we are closing a pipe then we:
 *  - send a hangup message
 *  - awaken any sleeping reader or writer processes
 *  - force other end to be unmounted.
 */
/*ARGSUSED*/
int
fifo_close(
	bhv_desc_t *bdp,
	int flag,
	lastclose_t lastclose,
	struct cred *crp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register struct fifonode *fnp = BHVTOF(bdp);
	struct fifonode *fnp2;
	struct vnode *vp2;
	struct stdata *stp;
	int error = 0;
	mon_t   *hadmonitor;	/* strclose */
	mon_state_t ms;

	if (!fnp) {
		return 0;
	}

	fifo_lock(fnp);

	/*
	 * Protect against multiple simultaneous calls to fifo_close,
	 * since we must release the fifo node lock around the call
	 * to strclose at the bottom of fifo_close to prevent double
	 * trip deadlock.  We have to wait for FIFOCLOSING to be false
	 * in order to depend on the value of vp->v_stream, since
	 * strclose will reset this and free the stream head.
	 */
	while (fnp->fn_flag & FIFOCLOSING) {
		sv_wait(&fifowaitclose, PZERO, &(fnp->fn_lockp->fn_lock), 0);
		fifo_lock(fnp);
	}

	/*
	 * clear stream events.
	 */
	if ((stp = vp->v_stream) != NULL)
		strclean(stp);
	/*
	 * If a file still has the pipe/FIFO open, return.
	 */
	if (lastclose == L_FALSE) {
		goto out;
	}

	/*
	 * Wake up any sleeping readers/writers.
	 */
	if (flag & FREAD) {
		if (--fnp->fn_numr == 0) {
			sv_broadcast(&fnp->fn_full);
			if (stp != NULL)
				pollwakeup(&stp->sd_pollist, POLLOUT);
		}
	}
	if (flag & FWRITE) {
		if (--fnp->fn_numw == 0) {
			sv_broadcast(&fnp->fn_empty);
			fnp->fn_flag |= FIFOWCLOSED;
			if (stp != NULL)
				pollwakeup(&stp->sd_pollist, 
						(POLLIN | POLLRDNORM));
		}
	}
	fnp->fn_open--;

	/*
	 * If not streaming, fifo open might have been interrupted
	 */
	if (stp == NULL) {
		goto out;
	}

	if (fnp->fn_open > 0) {
		goto out;
	}

	/*
	 * We need to be sure that strclose is non-recursive. So
	 * we mark FIFOCLOSING so that an open coming in after we've
	 * released fifo_lock (fn_open is already 0) doesn't assume
	 * vp->stream is valid (since strclose below will zap it)
	 */
	fnp->fn_flag |= FIFOCLOSING;

	/*
	 * If no more readers and writers:
	 * (1) tear down the stream
	 * (2) send stream hangup message to other side
	 * (3) force an unmount of other end if it was a mount point
	 */

	/*
	 * note that we can be closing a fifo that has never been openned.
	 * In that csee, both fn_numr and fn_numw can both be > 0. We thus
	 * remove the ASSERT. This can happen, for example, when we setup
	 * a FIFO but never do an ioctl(I_RECVFD).
	 *
	 * ASSERT(fnp->fn_numr == 0 && fnp->fn_numw == 0);
	 */

	if (fnp->fn_mate) {
		struct stdata *stp;

		ASSERT(fnp->fn_flag & ISPIPE);

		STRHEAD_LOCK(&vp->v_stream, stp);
		ASSERT(stp);
		putctl(stp->sd_wrq->q_next, M_HANGUP);
		qenable(stp->sd_wrq->q_next);
		STRHEAD_UNLOCK(stp);

		fnp2 = fnp->fn_mate;
		fnp->fn_mate = NULL;
		vp2 = FTOV(fnp2);

		/* free any waiting writers */
		sv_broadcast(&fnp2->fn_full);

		/* free any waiting readers */
		sv_broadcast(&fnp2->fn_empty);

		/* Blow away other fifonode's address of our vnode */
		fnp2->fn_mate = NULL;

		/*
		 * Neither side can have the lock on the other end's
		 * fifonode to avoid an A-A lock condition because the
		 * layering is violated by the nm_unmountall() procedure
		 * which calls vfile_close() with the file pointer for the
		 * mount point, which will then call fifo_close() and
		 * attempt to get the same lock on the fifonode.
		 */
		STRHEAD_LOCK(&vp2->v_stream, stp);
		ASSERT(stp);
		/*
		 * umount the other end's vnode if it was a mount point.
		 */
		if (stp->sd_flag & STRMOUNT) {
			STRHEAD_UNLOCK(stp);
			/*
			 * The unmount procedure is responsible for
			 * ensuring vnode 'vp2' and it's associated
			 * fifonode aren't deallocated by the vfile_close
			 * performed inside the nm_unmountall().
			 * Hence the hold/release operations aren't
			 * required to surround the unmountall call.
			 *
			 * XXX NOTE: The special case to avoid supporting
			 * general fifo node trip locking is to set the
			 * don't lock flag in the fifonode. As the unmount
			 * code will call us back with the common lock.
			 * Pipes share a common fifo lock for BOTH ends of
			 * the pipe to avoid race conditions on simultaneous
			 * closes.
			 */
			fifo_unlock(fnp);
#if SABLE
			cmn_err(CE_PANIC,"SABLE doesn't load namefs");
#else
			(void) nm_unmountall(vp2, crp);
#endif
		}
		else {
			STRHEAD_UNLOCK(stp);
			fifo_unlock(fnp);
		}
	}
	else
		fifo_unlock(fnp);

	/*
	 * Release the fifo node lock prior to calling a stream operation
	 * that might sleep because we will deadlock with the other process
	 * needing to obtain the fifo node lock to awaken the sleeping
	 * streams process.
	 */
	/*
	 * If we own a monitor, then give it up now.
	 * MUST be after releasing all locks, since vmonitor can call
	 * routines which will vsema or wake us up.
	 */
	if (hadmonitor = private.p_curkthread->k_monitor)
		rmonitor(hadmonitor, &ms);
	error = strclose(vp, flag, crp);
	/*
	 * get montior first. If we block, we do not what to be
	 * holding any flags.
	 */
	if (hadmonitor)
		amonitor(&ms);
	fifo_lock(fnp);

	if (fnp->fn_flag & FIFOSEND) {
		fnp->fn_flag &= ~FIFOSEND;
		if (sv_waitq(&fnp->fn_ioctl))
			sv_signal(&fnp->fn_ioctl);
	}
	fnp->fn_flag &= ~FIFOCLOSING;

	/* Wakeup any waiting closing processes */
	sv_broadcast(&fifowaitclose);
out:
	fifo_unlock(fnp);
	return (error);
}

/*
 * Read from a pipe or FIFO.
 * return 0 if....
 *    (1) user read request is 0 or no stream
 *    (2) broken pipe with no data
 *    (3) write-only FIFO with no data
 *    (4) no data and delay flags set.
 * While there is no data to read....
 *   -  if the NDELAY/NONBLOCK flag is set, return 0/EAGAIN.
 *   -  unlock the fifonode and sleep waiting for a writer.
 *   -  if a pipe and it has a mate, sleep waiting for its mate
 *      to write.
 */
/*ARGSUSED*/
int
fifo_read(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	cred_t *crp,
	flid_t *fl)
{
	register struct vnode *vp = BHV_TO_VNODE(bdp);
	register struct fifonode *fnp = BHVTOF(bdp);
	register int error = 0;
	timespec_t tv;
	register int s;

	if (!(ioflag & IO_ISLOCKED))
		fifo_rwlock(bdp, VRWLOCK_READ);

	ASSERT(fnp->fn_flag & FIFOLOCK);
	if (uiop->uio_resid == 0 || !vp->v_stream) {
		error = 0;
		goto out;
	}
	do {
		if (RD(vp->v_stream->sd_wrq)->q_first == NULL) {
			if (fnp->fn_flag & ISPIPE) {
				if (!fnp->fn_mate) {
					error = 0;
					goto out;
				}
			} else {
				if (fnp->fn_numw == 0) {
					error = 0;
					goto out;
				}
			}

			if (uiop->uio_fmode & FNDELAY) {
				error = 0;
				goto out;
			}
			if (uiop->uio_fmode & FNONBLOCK) {
				error = EAGAIN;
				goto out;
			}

			fnp->fn_flag |= FIFOREAD;
			fnp->fn_flag &= ~FIFOLOCK;

			/*
			 * XXXrs - UGLY HACK.  Not holding monitor so must
			 * use spinlock.
			 */
			LOCK_STR_RESOURCE(s);
			vp->v_stream->sd_pflag |= RSLEEP;
			UNLOCK_STR_RESOURCE(s);
			error = sv_wait_sig(&fnp->fn_empty, PPIPE,
					   &(fnp->fn_lockp->fn_lock), 0);
			/*
			 * XXXrs - UGLY HACK.  Clear RSLEEP here not in
			 * streams code.  See strrput() for details.
			 */
			LOCK_STR_RESOURCE(s);
			vp->v_stream->sd_pflag &= ~RSLEEP;
			UNLOCK_STR_RESOURCE(s);
			fifo_rwlock(bdp, VRWLOCK_WRITE);
			fnp->fn_flag &= ~FIFOREAD;
			if (error) {
				error = EINTR;
				goto out;
			}
		}
	} while ((error = strread(vp, uiop, crp)) == ESTRPIPE);
	if (error == 0) {
		if (fnp->fn_realvp) {
			struct vattr va;
			/*REFERENCED*/
			int unused;

			va.va_mask = AT_UPDATIME;
			VOP_SETATTR(fnp->fn_realvp, &va, 0, crp, unused);
		} else {
			nanotime_syscall(&tv);
			fnp->fn_atime = tv.tv_sec;
			if (fnp->fn_mate)
				fnp->fn_mate->fn_atime = tv.tv_sec;
		}
	}

	if (fnp->fn_mate)
		sv_signal(&(fnp->fn_mate->fn_full));
	else
		sv_signal(&fnp->fn_full);

 out:
	if (!(ioflag & IO_ISLOCKED))
		fifo_rwunlock(bdp, VRWLOCK_READ);
	
	return (error);
}

/*
 * send SIGPIPE and return EPIPE if ...
 *   (1) broken pipe
 *   (2) FIFO is not open for reading
 * return 0 if...
 *   (1) no stream
 *   (2) user request is 0 and STRSNDZERO is not set
 * While the stream is flow controlled....
 *   -  if the NDELAY/NONBLOCK flag is set, return 0/EAGAIN.
 *   -  unlock the fifonode and sleep waiting for a reader.
 *   -  if a pipe and it has a mate, sleep waiting for its mate
 *      to read.
 */
/*ARGSUSED*/
int
fifo_write(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	cred_t *crp,
	flid_t *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register struct fifonode *fnp = BHVTOF(bdp);
	register int error;
	register int write_size = uiop->uio_resid;
	timespec_t tv;
	register int s;

	if (!(ioflag & IO_ISLOCKED))
		fifo_rwlock(bdp, VRWLOCK_WRITE);

	ASSERT(fnp->fn_flag & FIFOLOCK);
	uiop->uio_offset = 0;
	if (!vp->v_stream) {
		error = 0;
		goto out;
	}
	/*
	 * Apparently, we should zap even zero-length writes with SIGPIPE.
	 */
	if (fnp->fn_numr == 0
	    || (!fnp->fn_mate && (fnp->fn_flag & ISPIPE))) {
		uiop->uio_sigpipe = 1;
		error = EPIPE;
		goto out;
	}
	if ((write_size == 0) && !(vp->v_stream->sd_flag & STRSNDZERO)) {
		error = 0;
		goto out;
	}

	while ((error = strwrite(vp, uiop, crp)) == ESTRPIPE) {
		if (uiop->uio_fmode & FNDELAY) {
			error = 0;
			goto out;
		}
		if (uiop->uio_fmode & FNONBLOCK) {
			if (uiop->uio_resid < write_size)
				error = 0;
			else
				error = EAGAIN;
			goto out;
		}
		if (fnp->fn_numr == 0
		    || (!fnp->fn_mate && (fnp->fn_flag & ISPIPE))) {
			uiop->uio_sigpipe = 1;
			error = EPIPE;
			goto out;
		}
		fnp->fn_flag |= FIFOWRITE;
		fnp->fn_flag &= ~FIFOLOCK;

		/*
		 * XXXrs - UGLY HACK.  Not holding monitor so must use
		 * spinlock.
		 */
		LOCK_STR_RESOURCE(s);
		vp->v_stream->sd_pflag |= WSLEEP;
		UNLOCK_STR_RESOURCE(s);

		error = sv_wait_sig(&fnp->fn_full, PPIPE,
				   &(fnp->fn_lockp->fn_lock), 0);

		/*
		 * XXXrs - UGLY HACK.  Clear WSLEEP here not in
		 * streams code.  See strrput/strwsrv for details.
		 */
		LOCK_STR_RESOURCE(s);
		vp->v_stream->sd_pflag &= ~WSLEEP;
		UNLOCK_STR_RESOURCE(s);
		fifo_rwlock(bdp, VRWLOCK_WRITE);
		fnp->fn_flag &= ~FIFOWRITE;
		if (error) {
			error = EINTR;
			goto out;
		}

		/* if the pipe is broken, do not continue */
		if (fnp->fn_numr == 0
		    || (!fnp->fn_mate && (fnp->fn_flag & ISPIPE))) {
			uiop->uio_sigpipe = 1;
			error = EPIPE;
			goto out;
		}
	}
	if (error == 0) {
		if (fnp->fn_realvp) {
			struct vattr va;
			/*REFERENCED*/
			int unused;

			va.va_mask = AT_UPDMTIME|AT_UPDCTIME;
			VOP_SETATTR(fnp->fn_realvp, &va, 0, crp, unused);
		} else {
			nanotime_syscall(&tv);
			fnp->fn_mtime = fnp->fn_ctime = tv.tv_sec;
			if (fnp->fn_mate) {
				fnp->fn_mate->fn_mtime = tv.tv_sec;
				fnp->fn_mate->fn_ctime = tv.tv_sec;
			}
		}
	}

	if (fnp->fn_mate)
		sv_signal(&(fnp->fn_mate->fn_empty));
	else
		sv_signal(&fnp->fn_empty);

	/* XXXbe MP-unsafe! */
	if (vp->v_stream->sd_wrq->q_next->q_count < PIPE_BUF)
		sv_signal(&fnp->fn_full);

 out:
	if (!(ioflag & IO_ISLOCKED))
		fifo_rwunlock(bdp, VRWLOCK_WRITE);

	return (error);
}

/*
 * Handle I_FLUSH and I_RECVFD request. All other requests are
 * directly sent to the stream head.
 */
/* ARGSUSED */
int
fifo_ioctl(
	bhv_desc_t *bdp,
	int cmd,
	void *arg,
	int mode,
	struct cred *cr,
	int *rvalp,
        struct vopbd *vbds)
{
	struct vnode *vp = BHV_TO_VNODE(bdp);
	register struct stdata *stp;
	register struct fifonode *fnp = BHVTOF(bdp);
	register int error = 0;

	switch (cmd) {

	default:
		error = strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp);
		break;

	case I_FLUSH:
		stp = vp->v_stream;
		if ((__psint_t)arg & ~FLUSHRW) {
			error = EINVAL;
			break;
		}
		/*
		 * If there are modules on the stream, pass
		 * the flush request to the stream head.
		 * XXXbe MP race with module poppers
		 */
		fifo_lock(fnp);
		if (stp->sd_wrq->q_next &&
		    stp->sd_wrq->q_next->q_qinfo != &strdata) {
			fifo_unlock(fnp);
			error = strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp);
			break;
		}
		/*
		 * flush the queues.
		 */
		if ((__psint_t)arg & FLUSHR) {
			fifo_flush(RD(stp->sd_wrq));
			sv_signal(&fnp->fn_full);
		}
		if (((__psint_t)arg & FLUSHW) && (stp->sd_wrq->q_next)) {
			fifo_flush(stp->sd_wrq->q_next);

			if (fnp->fn_mate) {
				sv_signal(&(fnp->fn_mate->fn_empty));
			} else {
				sv_signal(&fnp->fn_empty);
			}
		}
#ifdef _EAGER_RUNQUEUES
		/*
		 * run the queues
		 */
		if (qready())
			runqueues();
#endif
		fifo_unlock(fnp);
		break;

	/*
	 * Set the FIFOSEND flag to inform other processes that a file
	 * descriptor is pending at the stream head of this pipe.
	 * If the flag was already set, sleep until the other
	 * process has completed processing the file descriptor.
	 *
	 * The FIFOSEND flag is set by CONNLD when it is about to
	 * block waiting for the server to recieve the file
	 * descriptor.
	 */
	case I_S_RECVFD:
	case I_E_RECVFD:
	case I_RECVFD:
		/*
		 * The fifo lock must be released before doing any
		 * stream operation that will sleep otherwise the
		 * process to waken the blocked streams process cannot
		 * obtain the fifo lock. This avoids a deadlock case.
		 */
		error = strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp);
		if (error == 0) {
			fifo_lock(fnp);
			if (fnp->fn_flag & FIFOSEND) {
				fnp->fn_flag &= ~FIFOSEND;
				sv_signal(&fnp->fn_ioctl);
			}
			fifo_unlock(fnp);
		}
		break;
	}
	return (error);
}

/*
 * If shadowing a vnode (FIFOs), apply the VOP_GETATTR to the shadowed
 * vnode to obtain the node information. If not shadowing (pipes), obtain
 * the node information from the credentials structure.
 */
int
fifo_getattr(bhv_desc_t *bdp, struct vattr *vap, int flags, struct cred *crp)
{
	struct vnode *vp = BHV_TO_VNODE(bdp);
	register int error = 0;
	register struct fifonode *fnp = BHVTOF(bdp);
	struct queue *qp;
	struct qband *bandp;
	int tooklock = 0;

	/*
	 * This is a hack to handle the fact that fifo_frlock takes
	 * the fifo lock and then calls fs_frlock which calls reclock
	 * which calls convoff which calls VOP_GETATTR.  
	 */
 	if (!mutex_mine(&(fnp->fn_lockp->fn_lock))) {
 		tooklock = 1;
 		fifo_lock(fnp);
 	}

	if (fnp->fn_realvp) {
		/*
		 * for FIFOs or mounted pipes
		 */
		VOP_GETATTR(fnp->fn_realvp, vap, flags, crp, error);
		if (error)
			goto out;
	} else {
		/*
		 * for non-attached/ordinary pipes
		 */
		vap->va_mode = 0;
		vap->va_atime.tv_sec = fnp->fn_atime;
		vap->va_atime.tv_nsec = 0;
		vap->va_mtime.tv_sec = fnp->fn_mtime;
		vap->va_mtime.tv_nsec = 0;
		vap->va_ctime.tv_sec = fnp->fn_ctime;
		vap->va_ctime.tv_nsec = 0;
		vap->va_uid = crp->cr_uid;
		vap->va_gid = crp->cr_gid;
		vap->va_nlink = 0;
		vap->va_fsid = fifodev;
		vap->va_nodeid = fnp->fn_ino;
		vap->va_rdev = 0;
	}
	vap->va_type = VFIFO;
	vap->va_blksize = PIPE_BUF;
	/*
	 * Size is number of un-read bytes at the stream head and
	 * nblocks is the unread bytes expressed in blocks.
	 */
	if (vp->v_stream) {
		qp = RD(vp->v_stream->sd_wrq);
		if (qp->q_nband == 0)
			vap->va_size = qp->q_count;
		else {
			for (vap->va_size = qp->q_count, bandp = qp->q_bandp;
			     bandp; bandp = bandp->qb_next)
				vap->va_size += bandp->qb_count;
		}
		vap->va_nblocks = btod(vap->va_size);
	} else {
		vap->va_size = 0;
		vap->va_nblocks = 0;
	}
	vap->va_vcode = 0;
	vap->va_xflags = 0;
	vap->va_extsize = 0;
	vap->va_nextents = 0;
	vap->va_anextents = 0;
	vap->va_projid = 0;
	vap->va_gencount = 0;
out:
	if (tooklock)
		fifo_unlock(fnp);
	return (error);
}

/*
 * If shadowing a vnode, apply the VOP_SETATTR to it.
 * Otherwise, set the time and return 0.
 */
int
fifo_setattr(
	bhv_desc_t *bdp,
	struct vattr *vap,
	int flags,
	struct cred *crp)
{
	register struct fifonode *fnp = BHVTOF(bdp);
	register int error = 0;
	timespec_t tv;

	fifo_lock(fnp);
	if (fnp->fn_realvp) {
		VOP_SETATTR(fnp->fn_realvp, vap, flags, crp, error);
	}
	else {
		if (vap->va_mask & AT_ATIME)
			fnp->fn_atime = vap->va_atime.tv_sec;
		if (vap->va_mask & AT_MTIME) {
			fnp->fn_mtime = vap->va_mtime.tv_sec;
			nanotime_syscall(&tv);
			fnp->fn_ctime = tv.tv_sec;
		}
	}
	fifo_unlock(fnp);
	return (error);
}

/*
 * If shadowing a vnode, apply VOP_ACCESS to it.
 * Otherwise, return 0 (allow all access).
 */
int
fifo_access(bhv_desc_t *bdp, int mode, struct cred *crp)
{
	int error;

	if (BHVTOF(bdp)->fn_realvp) {
		VOP_ACCESS(BHVTOF(bdp)->fn_realvp, mode, crp, error);
		return (error);
	}
	return (0);
}

/*
 * If shadowing a vnode, apply the VOP_FSYNC to it.
 * Otherwise, return 0.
 */
int
fifo_fsync(bhv_desc_t *bdp, int flag, struct cred *crp, off_t start, off_t stop)
{
	int error;

	if (BHVTOF(bdp)->fn_realvp) {
		VOP_FSYNC(BHVTOF(bdp)->fn_realvp, flag, crp,
				start, stop, error);
		return (error);
	}
	return (0);
}

/*
 * Called when the upper level no longer holds references to the vnode.
 * First obtain the fifo list lock and the lock for this fifo node to avoid
 * a race with a fifofind operation which may perform a vn_hold on the
 * vnode. If we obtain both locks, then we sync the file system and free
 * the fifonode.
 */
int
fifo_inactive(bhv_desc_t *bdp, struct cred *crp)
{
	struct vnode *vp = BHV_TO_VNODE(bdp);
	register struct fifonode *fnp;

	fnp = BHVTOF(bdp);
	if (!fnp)
		return VN_INACTIVE_NOCACHE;

	ASSERT (vp->v_count == 0);
	ASSERT(BHV_OPS(bdp) == &fifo_vnodeops);

	mutex_lock(&fifoallocmon, PZERO);

	/*
	 * Check if we have a lock node. We may not in the error recovery
	 * cases called when creating a pipe if both ends of the pipe were
	 * not created and linked together
	 */
	if (fnp->fn_lockp) { 
		fifo_lock(fnp);
	}

	/*
	 * We assert that if the other half of a named pipe, the mate, still
	 * exists, then it must have a null vnode address to indicate the
	 * other end has have been made inactive.
	 */
	ASSERT(fnp->fn_mate == NULL);

	if (fnp->fn_flag & ISPIPE) {
		fifoclearid(fnp->fn_ino);
	}

	/*
	 * If we have a realvp remove the fifo node and release the real vnode
	 * reference count. In the other case we only have to remove the fifo
	 * node from fifo node list.
	 */
	if (fnp->fn_realvp) {
		fiforemove(fnp);
		VN_RELE(fnp->fn_realvp);
	}

	/*
	 * Remove the fifo behavior from the vnode and return
	 * the NOCACHE value below.
	 */
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);

	/*
	 * caller of fifo_freelocks MUST hold the fifo lock
	 * the freelocks procedure will do the vsema after
	 * decrementing the lock node's reference count.
	 *
	 * If the reference count of the fifo node lock is zero
	 * then
	 *   the fifo lock is unlocked and freed
	 *   the shared fifo locking node is deallocated
	 * else
	 *   the fifo lock is unlocked
	 */
	fifo_freelocks(fnp);
	kmem_zone_free(fifozone, fnp);

	mutex_unlock(&fifoallocmon);

	/* the inactive_teardown flag must have been set at vn_alloc time */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	return VN_INACTIVE_NOCACHE;
}

/*
 * If shadowing a vnode, apply the VOP_FID to it.
 * Otherwise, return EINVAL.
 */
int
fifo_fid(bhv_desc_t *bdp, struct fid **fidfnp)
{
	int error;

	if (BHVTOF(bdp)->fn_realvp) {
		VOP_FID(BHVTOF(bdp)->fn_realvp, fidfnp, error);
		return (error);
	}
	return (EINVAL);
}

/*
 * As above for VOP_FID but for VOP_FID2.
 */
int
fifo_fid2(bhv_desc_t *bdp, struct fid *fidfnp)
{
	int error;

	if (BHVTOF(bdp)->fn_realvp) {
		VOP_FID2(BHVTOF(bdp)->fn_realvp, fidfnp, error);
		return (error);
	}
	return (EINVAL);
}

/*
 * Return error since seeks are not allowed on pipes.
 */
/*ARGSUSED*/
int
fifo_seek(bhv_desc_t *bdp, off_t ooff, off_t *noffp)
{
	return (ESPIPE);
}

/*
 * fifo_frlock
 */
int
fifo_frlock(
	bhv_desc_t	*bdp,
	int		cmd,
	flock_t		*flockp,
	int		flag,
	off_t		offset,
	vrwlock_t	vrwlock,
	cred_t		*credp)
{
	int		dolock, error;

	dolock = (vrwlock == VRWLOCK_NONE);
	if (dolock) {
		fifo_rwlock(bdp, VRWLOCK_WRITE);
		vrwlock = VRWLOCK_WRITE;
	}

	error = fs_frlock(bdp, cmd, flockp, flag, offset, vrwlock, credp);
	if (dolock)
		fifo_rwunlock(bdp, VRWLOCK_WRITE);

	return error;
}

/*
 * If there is a realvp associated with vp, return it.
 */
int
fifo_realvp(bhv_desc_t *bdp, struct vnode **vpp)
{
	struct vnode *vp;
	register struct fifonode *fnp = BHVTOF(bdp);
	struct vnode *rvp;
	int error;

	if ((vp = fnp->fn_realvp) != NULL) {
		VOP_REALVP(vp, &rvp, error);
		if (error == 0)
			vp = rvp;
	}
	*vpp = vp;
	return (0);
}

/*
 * Poll for interesting events on a stream pipe
 */
/* ARGSUSED */
int
fifo_poll(bhv_desc_t *bdp,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int error;
	register struct fifonode *fnp = BHVTOF(bdp);

	if (!vp->v_stream)
		return (EINVAL);
	error = strpoll(vp->v_stream, events, anyyet, reventsp, phpp, genp);
	if ((fnp->fn_numr == 0) || (fnp->fn_flag & FIFOWCLOSED)) {
		*reventsp |= POLLHUP;
	}
	return(error);
}

/*
 * POSIX pathconf() support.
 */
/* ARGSUSED */
int
fifo_pathconf(bhv_desc_t *bdp, int cmd, long *valp, struct cred *cr)
{
	register int error;
	struct fifonode *fnp = BHVTOF(bdp);
	long val;

	/*
	 * Return the fifofs definition for PIPE_BUF and for
	 * named pipes we switch-out to file system specific
	 * routine where the named pipe resides.
	 */

	if ((cmd != _PC_PIPE_BUF) && fnp->fn_realvp) {
		VOP_PATHCONF(fnp->fn_realvp, cmd, &val, cr, error);	
	} else {
		switch (cmd) {
		case _PC_PIPE_BUF:
			val = PIPE_BUF;
			error = 0;
			break;

		default:
			error = EINVAL;
			break;
		}
	}

	if (error == 0)
		*valp = val;
	return error;
}


int
fifo_attr_get(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	struct vnode *vp = BHV_TO_VNODE(bdp);
	struct fifonode *fnp = BHVTOF(bdp);
	int error = ENOATTR;

	fifo_lock(fnp);

	if (fnp->fn_realvp) {
		/*
		 * for FIFOs or mounted pipes
		 */
		VOP_ATTR_GET(fnp->fn_realvp, name, value, valuelenp,
				flags, cred, error);
	}
	else {
		error = _MAC_FIFO_ATTR_GET(bdp, name, value, valuelenp,
				flags, cred);
	}
#ifdef	DEBUG
	if (error)
		cmn_err(CE_NOTE, "fifo_attr_get error=%d, %s", error,
		    (fnp->fn_realvp) ? "Realvp" : "NotReal");
#endif	/* DEBUG */

	fifo_unlock(fnp);
	return error;
}

int
fifo_attr_set(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int valuelen,
	int flags,
	struct cred *cred)
{
	struct fifonode *fnp = BHVTOF(bdp);
	int error = ENOATTR;

	fifo_lock(fnp);

	if (fnp->fn_realvp) {
		/*
		 * for FIFOs or mounted pipes
		 */
		VOP_ATTR_SET(fnp->fn_realvp, name, value, valuelen, flags, cred, error);
	}
	else {
#ifdef	DEBUG
		cmn_err(CE_NOTE, "fifo_attr_set %s(%d) Not Real",
		    __FILE__, __LINE__);
#endif	/* DEBUG */
		error = ENOSYS;
	}

	fifo_unlock(fnp);
	return error;
}

vnodeops_t fifo_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	fifo_open,
	fifo_close,
	fifo_read,
	fifo_write,
	fifo_ioctl,
	fs_setfl,
	fifo_getattr,
	fifo_setattr,
	fifo_access,
	(vop_lookup_t)fs_nosys,
	(vop_create_t)fs_nosys,
	(vop_remove_t)fs_nosys,
	(vop_link_t)fs_nosys,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	(vop_readdir_t)fs_nosys,
	(vop_symlink_t)fs_nosys,
	(vop_readlink_t)fs_nosys,
	fifo_fsync,
	fifo_inactive,
	fifo_fid,
	fifo_fid2,
	fifo_rwlock,
	fifo_rwunlock,
	fifo_seek,
	fs_cmp,
	fifo_frlock,	/* frlock */
	fifo_realvp,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_noval,
	(vop_map_t)fs_nodev,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	fifo_poll,
	(vop_dump_t)fs_nosys,
	fifo_pathconf,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	fifo_attr_get,
	fifo_attr_set,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	(vop_cover_t)fs_nosys,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	(vop_ptossvp_t)fs_noval,
	(vop_pflushinvalvp_t)fs_noval,
	(vop_pflushvp_t)fs_nosys,
	(vop_pinvalfree_t)fs_noval,
	(vop_sethole_t)fs_noval,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};
