#ident "$Header: /proj/irix6.5.7m/isms/irix/kern/io/RCS/bpqueue.c,v 1.13 1996/03/30 02:08:52 yohn Exp $"

#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/mkdev.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/bpqueue.h>


/*
 * These routines supply an interface to a generic kernel process.
 * The process reads from a queue of buffers and calls a user supplied 
 * routine to process each buffer. 
 *
 * Each instance of the daemon can use different queues, semaphores and 
 * processing routines.
 *
 *
 */

/*
 * bp_enqueue()
 * 	Put the bp on the queue pointed to by bpqueuep. The queuelock
 *	is used to protect the queue. The queuewakeup semaphore is used
 *	to inform the kernel process that there are items on the queue
 *	to process.
 *
 * RETURNS:
 *	none
 */
void
bp_enqueue(buf_t *bp, bp_queue_t *bpqp )
{
	buf_t 	*bpqueue;
	int	s;

	/*
 	 * Get exclusive access to the queue.
	 */
	s = mutex_bitlock(BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT);

	/*
	 * Get the buffer queue.
	 */
	bpqueue = *BPQ_QUEUE(bpqp);

	if (bpqueue) {
		/*
 		 * Put the new buffer at the end of the list.
	 	 */
		bpqueue->av_back->av_forw = bp;
		bp->av_back 	= bpqueue->av_back;
		bpqueue->av_back = bp;
		bp->av_forw =  bpqueue;
	} else {
		bpqueue = bp;
		bp->av_forw = bp->av_back = bp;
	}
		
	*BPQ_QUEUE(bpqp) = bpqueue;

	/*
	 * Wakeup the daemon.
	 * Waitbits get zero'd below as side-effect of the unlock call.
	 */
	if (bpqp->queuebits & BPQ_WAITBITS) {
		sv_broadcast(BPQ_QUEUEWAKEUP(bpqp));
	}

	/*
 	 * Release exclusive access.  Zero wait-bits, too.
	 */
	mutex_bitunlock(BPQ_QUEUELOCK(bpqp), 
		(BPQ_LOCKBIT|BPQ_WAITBITS) & (*(BPQ_QUEUELOCK(bpqp))), 
		s);

}

/*
 * bp_dequeue()
 * 	Remove the 1st bp from the queue pointed to by bpqueuep. 
 *	The queuelock, used to protect access to the queue, is held
 *	on entry and exit.
 *
 * RETURNS:
 *	none
 */
static buf_t *
bp_dequeue(buf_t **bpqueuep)
{
	buf_t 	*bp, *bpqueue;

	/*
	 * Get the buffer queue.
	 */
	bpqueue = *bpqueuep;

	if ((bp = bpqueue) != NULL) {
		if (bp->av_forw == bp) {
			ASSERT(bp->av_back == bp);
			bpqueue = NULL;
		} else {
			bpqueue = bp->av_forw;
			bpqueue->av_back = bp->av_back;
			bp->av_back->av_forw = bpqueue;
		}
		bp->av_forw = NULL;
		bp->av_back = NULL;
	}
	*bpqueuep = bpqueue;

	return( bp );
}

/*
 * bp_dequeue_wait()
 *      Remove the 1st bp from the queue pointed to by bpqueuep.
 *	If the queue is empty, wait until it is non-empty or until a signal.
 *      The queuelock is used to protect access to the queue.
 *
 * RETURNS:
 *     	valid bp on success
 *	NULL if a signal was sent
 */
buf_t *
bp_dequeue_wait(bp_queue_t *bpqp)
{
        buf_t   *bp;
	int	s;

	/*
 	 * Get exclusive access to the queue.
	 */
	s = mutex_bitlock(BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT);

	while ((bp = bp_dequeue(BPQ_QUEUE(bpqp))) == NULL) {
		/*
		 * Bump waiter-count.
		 */
		BPQ_QUEUEWAITCNT(bpqp)++;

		/* 
		 * Check if signal terminated the semaphore op.
		 */
		if (sv_bitlock_wait_sig(BPQ_QUEUEWAKEUP(bpqp), PZERO+1,
				BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT, s) == -1) {

			return( (buf_t *)NULL );
		}
		/*
		 * sv_bitlock_wait_sig() drops the bit lock, 
		 * need to re-acquire it before continuing.
		 */
		s = mutex_bitlock(BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT);
	}

	/*
 	 * Release exclusive access.
	 */
	mutex_bitunlock(BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT, s);

	return bp;
}



/*
 * bpqueuedaemon()
 * 	Kernel process which reads from the queue pointed at by
 *	bpqueuep. The queue is protected by bpqueuelock and the
 *	bpqueuewakeup semaphore is used to tell the process that
 *	there are items on the queue to process. The routine
 *	*process is called for each of the buffers on the queue.
 *	
 * RETURNS:
 *	none
 */
void
bpqueuedaemon(bp_queue_t *bpqp, void (*process)(buf_t *))
{
	buf_t	*bp;
	int	s;

loop:
	s = mutex_bitlock(BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT);
	while (bp = bp_dequeue(BPQ_QUEUE(bpqp))) {
		mutex_bitunlock(BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT, s);
		(*process)(bp);
		s = mutex_bitlock(BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT);
	}

	BPQ_QUEUEWAITCNT(bpqp)++;
	sv_bitlock_wait(BPQ_QUEUEWAKEUP(bpqp), PZERO,
		 BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT, s);

	goto loop;
}

/*
 * bpqueueinit()
 *	This is the initialization routine for the sync structures
 *	used by this instance of the bpqueue daemon.
 *
 * RETURNS:
 *	none
 */
void
bpqueue_init( bp_queue_t *bpqp )
{
	/* init bpqueue semas */
	sv_init(BPQ_QUEUEWAKEUP(bpqp), SV_DEFAULT, "bpqwake");

	*(BPQ_QUEUELOCK(bpqp)) = 0;

	init_bitlock(BPQ_QUEUELOCK(bpqp), BPQ_LOCKBIT,
		"bpq_lck", METER_NO_SEQ);
}
