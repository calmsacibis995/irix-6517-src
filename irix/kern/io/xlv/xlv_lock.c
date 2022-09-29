#ident "$Revision: 1.3 $"

/**************************************************************************
 *									  *
 *            Copyright (C) 1993-1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include <sys/types.h>
#include <sys/debug.h>
#include <sys/uuid.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bpqueue.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include "xlv_ioctx.h"
#include "xlv_mem.h"
#include "xlv_procs.h"
#include <sys/xlv_lock.h>

#ifdef XLV_LOCK_DEBUG
#include <sys/kmem.h>
#include <sys/ktrace.h>
ktrace_t	 *xlv_lock_dbg_trace_buf;
/* XLV LOCK DEBUG ktrace entry format:
 *  0: string identifying where called
 *  1: sv
 *  2: bp
 *  3: error_count
 *  4: io_count
 *  5: wait_count
 *  6: count of the error   queue
 *  7: head  of the error   queue
 *  8: tail  of the error   queue
 *  9: count of the reissue queue
 * 10: head  of the reissue queue
 * 11: tail  of the reissue queue
 */
#define LOCK_DEBUG(where, sv, bp, svl) \
	ktrace_enter(xlv_lock_dbg_trace_buf, where,			\
		     (void *) (long long) sv, bp,			\
		     (void *) (long long) svl->error_count,		\
		     (void *) (long long) svl->io_count,		\
		     (void *) (long long) svl->wait_count,		\
		     (void *) (long long) svl->error_queue.n_queued,	\
		     BUF_QUEUE_HEAD(svl->error_queue),			\
		     BUF_QUEUE_TAIL(svl->error_queue),			\
		     (void *) (long long) svl->reissue_queue.n_queued,	\
		     BUF_QUEUE_HEAD(svl->reissue_queue),		\
		     BUF_QUEUE_TAIL(svl->reissue_queue),		\
		     NULL, NULL, NULL, NULL)
#else
#define LOCK_DEBUG(where, sv, bp, svl)
#endif

extern bp_queue_t	  xlvd_work_queue;

/*
 * XLV locking routines
 * 
 * xlv_io_lock[sv] controls access to subvolume sv.
 * The types of accesses are:
 * 1) access to read subvolume data structures
 *	a) to open the subvolume, get information about it, or do ioctls;
 *	b) to do i/o
 * 2) access to carry out error recovery and perhaps consequently update
 *	the subvolume.
 * 3) access to change the subvolume structure
 *
 * There are correspondingly three lock routines:
 *	xlv_acquire_io_lock(), xlv_block_io(), and xlv_acquire_cfg_lock().
 *
 * Think of the xlv io lock as a multiple reader, single writer lock with
 * the following modifications:
 * 1) if you can't get the lock, you have a choice between getting blocked
 *	or getting queued up for later execution;
 * 2) "writers" get priority over readers.
 *
 * There are three counters with each lock:
 *	io_count: number of threads that have acquired the lock in "access
 *		mode" to look at data structures or do i/o;
 *	error_count: number of buffers that have an error and are either
 *		doing or waiting to do error recovery;
 *	wait_count: number of threads waiting to get in the lock, or are
 *		in doing subvolume configuration changes.
 *
 * There are two queues associated with each lock: one (reissue_queue)
 * holds those wanting to do io, but can't yet get the lock; the other
 * (error_queue) holds those wanting to do error recovery but can't begin
 * yet.  Those in the reissue queue get reissued (all together) through
 * xlvstrategy whenever possible; those in the error queue get put in the
 * xlvd_work_queue one at a time when error recovery is possible.
 *
 *
 * OPERATIONS ALLOWED ON XLV IO LOCKS
 *
 * int xlv_acquire_io_lock(int sv, buf_t *bp): acquire the lock for
 * ------------------------------------------
 * subvolume sv in "access mode": for doing io or looking at data structures.
 * This prevents someone from changing the subvolume's configuration until
 * the io is over.  One of three things can happen:
 * 	a) you get the lock (returns 1): proceed with your io;
 * 	b) you block until the lock is available (this only happens if
 * 	   if you pass in a NULL bp);
 * 	c) you can't get the lock (returns 0): you don't get blocked,
 * 	   but you can't do io now (only if you passed in a non-NULL bp).
 * 	   Not to worry, the buffer has been enqueued, and will be reissued
 *	   when the lock is available.
 * 
 * void xlv_release_io_lock(int sv): release the lock for subvolume sv.
 * --------------------------------
 * If you were the last one doing io:
 *	if this subvolume had an error, error recovery can begin.
 *	if any config changers were waiting, one can get in to do its thing.  
 * 
 * int xlv_block_io(int sv, buf_t *bp): block all further io's on this
 * -----------------------------------
 * subvolume.  No further io's will be allowed in; anyone currently doing
 * io will be allowed to complete.  If the call is successful (returns 1),
 * error recovery can begin.  Otherwise, the buffer bp is held aside, and
 * will be kicked off for error recovery when everyone else doing io is done.
 * 
 * void xlv_unblock_io(int sv): unblock io's on this subvolume.  First, any
 * ---------------------------
 * other buffers waiting for error recovery will be allowed to complete.
 * If there are none, one waiting for a config lock will be allowed in.  If
 * there is none, all io's that had been deferred until error recovery was
 * complete will be reissued; new io's will also be allowed.
 * 
 * void xlv_acquire_cfg_lock(int sv): acquire the lock for subvolume sv for
 * ---------------------------------
 * making configuration changes.  This can include subvolume geometry changes,
 * changes in the read-writeback/critical region and reassembly.  While the
 * lock is held in this mode, no io is allowed on this subvolume.
 * 
 * void xlv_release_cfg_lock(int sv): release the lock for subvolume sv when
 * ---------------------------------
 * done making configuration changes.  All pending io is reissued; new io is
 * also allowed.  There should be no one waiting for error recovery.
 */

void
xlv_io_lock_init(int sv)
{
    register xlv_lock_t	 *svl = &xlv_io_lock[sv];

    init_spinlock(&svl->lock, "XLV", sv);
    svl->error_count = 0;
    svl->io_count = 0;
    svl->wait_count = 0;
    init_sema(&svl->wait_sem, 0, "XLV", sv);
    BUF_QUEUE_INIT(svl->reissue_queue);
    BUF_QUEUE_INIT(svl->error_queue);
#ifdef XLV_LOCK_DEBUG
    if (sv == 0) xlv_lock_dbg_trace_buf = ktrace_alloc(512, KM_SLEEP);
#endif
}


int
xlv_acquire_io_lock(int sv, buf_t *bp)
{
    register xlv_lock_t	 *svl = &xlv_io_lock[sv];
    int			  s;
    register buf_t	 *nbp;

    s = mutex_spinlock(&svl->lock);
    LOCK_DEBUG("ACQ IO ", sv, bp, svl);
    
    /* check if io lock is available */
    if (svl->error_count + svl->wait_count == 0) {
	svl->io_count++;
	mutex_spinunlock(&svl->lock, s);
	return 1;
    }
    
    /*
     * can't get lock:
     * either wait or defer depending on whether bp is NULL or not
     */
    if (bp) {
	/* defer: put bp in reissue queue */
	BUF_ENQUEUE(svl->reissue_queue, bp);
	mutex_spinunlock(&svl->lock, s);
	return 0;
    }

    /* have to wait until no one is doing io */
    svl->wait_count++;
    mutex_spinunlock(&svl->lock, s);
    psema(&svl->wait_sem, PRIBIO);

    /* okay, I'm in, and there should be no ios or errors! */
    s = mutex_spinlock(&svl->lock);
    svl->wait_count--;
    LOCK_DEBUG("GOT IO ", sv, bp, svl);
    ASSERT(svl->error_count == 0 && svl->io_count == 0 &&
	   BUF_QUEUE_ISEMPTY(svl->error_queue));
    svl->io_count = 1;
    /* if wait count is 0, reissue any pending ios */
    if (svl->wait_count == 0) {
	BUF_QUEUE_EMPTY(svl->reissue_queue, bp);
    }
    mutex_spinunlock(&svl->lock, s);

    for (; bp; bp = nbp) {
	nbp = BUF_QUEUE_NEXT(bp);
	bp->b_flags &= ~B_XLV_HBUF;
	xlvstrategy(bp);
    }

    return 1;
}


void
xlv_release_io_lock(int sv)
{
    register xlv_lock_t	 *svl = &xlv_io_lock[sv];
    int			  s;
    buf_t		 *e_bp = NULL;

    s = mutex_spinlock(&svl->lock);
    LOCK_DEBUG("REL IO ", sv, NULL, svl);

    ASSERT(svl->io_count);

    svl->io_count--;

    /*
     * If no one is doing io:
     *   if ios are blocked for error recovery, recovery can start;
     *   if not, but there are waiters, let one in.
     */
    if (svl->io_count == 0) {
	if (svl->error_count + svl->wait_count != 0) {
	    if (svl->error_count != 0) {
		BUF_DEQUEUE(svl->error_queue, e_bp);
	    }
	    else {
		ASSERT(svl->wait_count != 0);
		vsema(&svl->wait_sem);
	    }
	}
    }

    mutex_spinunlock(&svl->lock, s);

    if (e_bp) {
	/* kick off error recovery */
	bp_enqueue(e_bp, &xlvd_work_queue);
    }
}


int
xlv_block_io(int sv, buf_t *bp)
{
    register xlv_lock_t	 *svl = &xlv_io_lock[sv];
    int			  s;

    s = mutex_spinlock(&svl->lock);
    LOCK_DEBUG("BLK IO ", sv, bp, svl);

    if (svl->io_count == 0) {
	ASSERT(svl->error_count);
	mutex_spinunlock(&svl->lock, s);
	return 1;
    }

    svl->io_count--;
    svl->error_count++;

    if (svl->io_count == 0) {
	/* no one else in there doing io: start error recovery */
	mutex_spinunlock(&svl->lock, s);
	return 1;
    }

    /* someone's still doing io: defer (put bp in error_queue) */
    BUF_ENQUEUE(svl->error_queue, bp);

    mutex_spinunlock(&svl->lock, s);
    return 0;
}


void
xlv_unblock_io(int sv)
{
    register xlv_lock_t	 *svl = &xlv_io_lock[sv];
    register buf_t	 *bp, *nbp;
    int			  s;

    s = mutex_spinlock(&svl->lock);
    LOCK_DEBUG("UNBLKIO", sv, NULL, svl);

    svl->error_count--;

    if (svl->error_count) {
	/* more buffers with error */
	BUF_DEQUEUE(svl->error_queue, bp);
	mutex_spinunlock(&svl->lock, s);
	bp_enqueue(bp, &xlvd_work_queue);
	return;
    }

    /* all error recovery done: check if there are any waiters */
    if (svl->wait_count) {
	mutex_spinunlock(&svl->lock, s);
	vsema(&svl->wait_sem);
	return;
    }

    /* no waiters either: reissue all pending io */
    BUF_QUEUE_EMPTY(svl->reissue_queue, bp);
    mutex_spinunlock(&svl->lock, s);

    for (; bp; bp = nbp) {
	nbp = BUF_QUEUE_NEXT(bp);
	xlvstrategy(bp);
    }

    return;
}


void
xlv_acquire_cfg_lock(int sv)
{
    register xlv_lock_t	 *svl = &xlv_io_lock[sv];
    int			  s;

    s = mutex_spinlock(&svl->lock);
    LOCK_DEBUG("ACQ CFG", sv, NULL, svl);

    if (svl->io_count + svl->error_count + svl->wait_count == 0) {
	svl->wait_count = 1;
	mutex_spinunlock(&svl->lock, s);
	return;
    }

    svl->wait_count++;
    mutex_spinunlock(&svl->lock, s);
    psema(&svl->wait_sem, PRIBIO);
    LOCK_DEBUG("GOT CFG", sv, NULL, svl);

    return;
}


void
xlv_release_cfg_lock(int sv)
{
    register xlv_lock_t	 *svl = &xlv_io_lock[sv];
    int			  s;
    buf_t		 *bp = NULL, *nbp;

    s = mutex_spinlock(&svl->lock);
    LOCK_DEBUG("REL CFG", sv, NULL, svl);

    ASSERT(svl->wait_count != 0 && svl->io_count == 0 && svl->error_count == 0
	   && BUF_QUEUE_ISEMPTY(svl->error_queue));
    svl->wait_count--;
    if (svl->wait_count == 0) {
	BUF_QUEUE_EMPTY(svl->reissue_queue, bp);
    }
    else {
	vsema(&svl->wait_sem);
    }
    mutex_spinunlock(&svl->lock, s);

    for (; bp; bp = nbp) {
	/* reissue bp */
	nbp = BUF_QUEUE_NEXT(bp);
	xlvstrategy(bp);
    }

    return;
}
