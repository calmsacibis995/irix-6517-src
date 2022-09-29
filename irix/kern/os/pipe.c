/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/pipe.c	1.5"*/
#ident	"$Revision: 3.32 $"

#include <sys/types.h>
#include <fifofs/fifonode.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/strsubr.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <pipefs/pipenode.h>
#include <sys/idbgactlog.h>

extern int svr3pipe;

/*
 * pipe(2) system call.
 * Create a pipe by connecting two streams together. Associate each end of
 * the pipe with a vnode, a file descriptor and one end of the stream.
 */
/*ARGSUSED*/
int
pipe(void *uap, rval_t *rvp)
{
	struct vnode *vp1, *vp2;
	struct vfile *fp1, *fp2;
	struct fifonode *fnp1, *fnp2;
	fifo_lock_t *plp;
	register int error = 0;
	int fd1, fd2;
	mon_t *mon;
	int s;

	if (svr3pipe && !(curuthread->ut_pproxy->prxy_flags & PRXY_SPIPE)) {
		error = oldpipe(uap, rvp);
		_SAT_PIPE(rvp->r_val1,rvp->r_val2,error);
		return error;
	}
	/*
	 * Allocate and initialize two vnodes (and associated fifo node)
	 * and create a shared locking node used to synchronize the closing
	 * of both ends of the pipe.
	 */
	if ((fnp1 = makepipe(FWRITE|FREAD)) == NULL) {
		_SAT_PIPE(0,0,ENOMEM);
		return ENOMEM;
	}
	vp1 = FTOV(fnp1);
	if ((fnp2 = makepipe(FWRITE|FREAD)) == NULL) {
		VN_RELE(vp1);
		_SAT_PIPE(0,0,ENOMEM);
		return ENOMEM;
	}
	vp2 = FTOV(fnp2);
	if ((plp = fifo_cr_lock((long)vp1->v_number)) == NULL) {
		VN_RELE(vp1);
		VN_RELE(vp2);
		_SAT_PIPE(0,0,ENOMEM);
		return ENOMEM;
	}

	/*
	 * Allocate and initialize two file table entries and two
	 * file pointers. Each file pointer is open for read and write.
	 */
	if (error = vfile_alloc(FWRITE|FREAD, &fp1, &fd1)) {
		VN_RELE(vp1);
		VN_RELE(vp2);
		fifo_del_lock(plp);
		_SAT_PIPE(0,0,error);
		return (error);
	}
	error = vfile_alloc(FWRITE|FREAD, &fp2, &fd2);
	if (error)
		goto out2;

	/*
	 * Create two stream heads and attach to each vnode.
	 * 
         * fn_open used to be incremented at the top of fifo_stropen().
         * That code was moved to fifo_open() to close a race condition,
         * so we have to do the increments here ourselves.
         */

	fnp1->fn_open++;
	if (error = fifo_stropen(&vp1, 0, fp1->vf_cred))
		goto out;

	fnp2->fn_open++;
	if (error = fifo_stropen(&vp2, 0, fp2->vf_cred)) {
		strclean(vp1->v_stream);
		strclose(vp1, 0, fp1->vf_cred);
		goto out;
	}


	/*
	 * Make both stream heads share a monitor.
	 */
	LOCK_STRHEAD_MONP(s);
        mon = str_mon_detach(vp2->v_stream);
	str_privmon_attach(vp2->v_stream, vp1->v_stream->sd_monp);
	UNLOCK_STRHEAD_MONP(s);
        str_mon_free(mon);


	/*
	 * Twist the stream head queues so that the write queue
	 * points to the other stream's read queue.
	 */
	vp1->v_stream->sd_wrq->q_next = RD(vp2->v_stream->sd_wrq);
	vp2->v_stream->sd_wrq->q_next = RD(vp1->v_stream->sd_wrq);
	/*
	 * Tell each pipe about its other half.
	 */
	fnp1->fn_mate = fnp2;
	fnp2->fn_mate = fnp1;
	fnp1->fn_ino = fnp2->fn_ino = fifogetid();
	/*
	 * Share the common pipe lock for close synchronization
	 * Set shared lock node's reference count to cover both ends
	 */
	plp->fn_refcnt = 2;
	fnp1->fn_lockp = fnp2->fn_lockp = plp;

	/*
	 * Return the file descriptors to the user. They now
	 * point to two different vnodes which have different
	 * stream heads.
	 */
	vfile_ready(fp1, vp1);
	vfile_ready(fp2, vp2);
	rvp->r_val1 = fd1;
	rvp->r_val2 = fd2;
	_SAT_PIPE(fd1,fd2,0);
	return (0);
out:
	vfile_alloc_undo(fd2, fp2);
out2:
	vfile_alloc_undo(fd1, fp1);
	VN_RELE(vp1);
	VN_RELE(vp2);
	fifo_del_lock(plp);
	_SAT_PIPE(0,0,error);
	return (error);
}
