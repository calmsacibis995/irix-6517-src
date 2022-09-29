#ident "$Header: /proj/irix6.5.7m/isms/irix/kern/io/grio/RCS/grio_comm.c,v 1.29 1999/07/27 20:32:19 lhd Exp $"

#ifdef SIM
#define _KERNEL
#include <stdio.h>
#endif
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/major.h>
#include <sys/mkdev.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_inum.h>
#include <sys/time.h>
#include <sys/grio.h>
#include <sys/bpqueue.h>
#include <sys/kabi.h>
#include <sys/xlate.h>
#include <sys/kmem.h>
#include <sys/syssgi.h>
#include <sys/scsi.h>
#include <sys/schedctl.h>
#ifdef SIM
#undef _KERNEL
#else
#include <sys/pda.h>
#endif
#include <ksys/sthread.h>


/*
 * Global variables.
 */
extern int			grio_dbg_level;
				/* ggd daemon sleeps on sema waiting for work */
extern sv_t           		grio_q_sv;		
				/* protect global grio_info array 	      */
extern lock_t           	grio_global_lock;	
				/* used by bp queue daemon to start requests  */
extern bp_queue_t 		grio_subreq_bpqueue;		
extern grio_cmd_queue_t		*grio_q;
int				grio_numcmds = 0;  /* ggd access only */

extern int			grio_start_next_io(grio_disk_info_t *, 
					time_t, int);
extern grio_disk_info_t		*grio_get_disk_info( dev_t );
extern grio_stream_disk_info_t  *grio_get_stream_disk_info( buf_t *);


/*
 * Forward routine declarations.
 */
STATIC int 			grio_add_msg_to_q_and_wait( grio_cmd_queue_t *);
STATIC int 			grio_add_async_msg_to_q( grio_cmd_queue_t *);
STATIC int			grio_wait_for_msg_response( grio_cmd_queue_t *);
STATIC int 			grio_remove_msg_from_queue( grio_cmd_queue_t *);

/*
 * A note about the grio Q manipulation. grio_q points to the first
 * element, and new elements are added to the end of the queue. All
 * the elements are chained with forw and back pointers on a doubly
 * linked list. Different processes queue requests, and also might
 * want to take off their queued requests when they catch a signal
 * while waiting for response from ggd. ggd takes off elements from
 * the queue by either responding to them or by accepting async
 * requests. Also, ggd wants to copy out the data associated with
 * each q element to user space, and this operation might sleep, so
 * should not be done with locks held. The variable grio_numcmds
 * keeps track of the number of sub commands in the command that
 * ggd is servicing.
 */

/*
 * grio_issue_grio_req()
 *      This routine copies the user memory copy of the grio requests
 *      into kernel memory. Queues the requests to the daemon and then
 *      waits for a reply.
 *
 * RETURNS:
 *      0 on success
 *      EFAULT if could not access user space,
 *      -1 if the command terminated with a signal.
 */
int
grio_issue_grio_req( sysarg_t cnum, sysarg_t cptr )
{
	int               ret;
	grio_cmd_queue_t  *griocqp;
	grio_cmd_t	  *griocp;

	griocp = kmem_alloc(sizeof(grio_cmd_t) * (int)cnum, KM_SLEEP);
	griocqp = kmem_alloc(sizeof(grio_cmd_queue_t), KM_SLEEP);

	/*
	 * Copy the requests to kernel space.
	 */
	if (copyin((caddr_t)cptr, griocp, sizeof(grio_cmd_t)*(int)cnum)) {
		kmem_free(griocp, sizeof(grio_cmd_t) * (int)cnum);
		kmem_free(griocqp, sizeof(grio_cmd_queue_t));
		return( EFAULT );
	}

	/* Add a pointer to the grio_cmd_t array in the grio_cmd_queue_t
	 * structure.
	 */
	griocqp->griocmd = griocp;
	griocqp->num_cmds = (int)cnum;

	/*
	 * Wait for the daemon to answer the request.
	 */
	ret = grio_add_msg_to_q_and_wait( griocqp );

	/*
	 * Copy the results to user space.
	 */
	if (copyout(griocp, (void *)cptr, (int)cnum*sizeof(grio_cmd_t))) {
		ASSERT( 0 );
		kmem_free(griocp, sizeof(grio_cmd_t) * (int)cnum);
		kmem_free(griocqp, sizeof(grio_cmd_queue_t));
		return( EFAULT );
	}

	kmem_free(griocp, sizeof(grio_cmd_t) * (int)cnum);
	kmem_free(griocqp, sizeof(grio_cmd_queue_t));

	return( ret );
}


/*
 * grio_issue_async_grio_req()
 *      Create a grio message and add it to the message queue.
 *
 * RETURNS:
 *      0 on success
 *      -1 on failure
 */
int
grio_issue_async_grio_req(char *cptr)
{
	int               ret = 0;
	grio_cmd_queue_t  *griocqp;
	grio_cmd_t	  *griocp;

	griocqp = kmem_alloc(sizeof(grio_cmd_queue_t), KM_SLEEP);
	griocp = kmem_alloc(sizeof(grio_cmd_t), KM_SLEEP);

	bcopy(cptr, (caddr_t)(griocp), sizeof(grio_cmd_t));
	griocqp->griocmd = griocp;
	griocqp->num_cmds = 1;
	griocqp->forw = griocqp->back = NULL;

	/*
	 * Add to the message q, do not wait.
	 */
	if (grio_add_async_msg_to_q( griocqp )) {
		/*
		 * Message could not be added to queue.
		 */
		kmem_free(griocp, sizeof(grio_cmd_t));
		kmem_free(griocqp, sizeof(grio_cmd_queue_t));
		ret = -1;
	}
	return( ret );
}



/*
 * grio_resp_grio_req()
 *      This routine is called by the grio daemon when it is completing
 *      a grio user request. The results from the daemon are copied to
 *      kernel memory. The request is removed from the queue and the client
 *      is notified via the semaphore.
 *
 *	Caller of this routine should ensure that none of the 
 *	sub-actions in the request being responded to were asynch.
 *
 * RETURNS:
 *       0 on success
 *	 1 when the request we are responding to has been taken off.
 *      -1 on failure
 * TODO:
 *	Device a better way to figure out that a requesting process
 *	has taken its request off the q.
 *	ggd should keep trying to respond to a request if it fails,
 *	else the requesting process will hang forever.
 */
int
grio_resp_grio_req(sysarg_t cptr)
{
	int                     s, numbytes;
	grio_cmd_queue_t        *griocqp;
	grio_cmd_t		*kcmd;

	/*
	 * Allocate a temporary kernel buffer to get results from
	 * ggd space. Allocate as much as we told ggd.
	 */
	numbytes = sizeof(grio_cmd_t) * grio_numcmds;
	kcmd = kmem_alloc(numbytes, KM_SLEEP);
	if (kcmd == 0)
		return(ENOMEM);

	/*
	 * Copy the results to kernel memory. Copy only grio_numcmds,
	 * since ggd should have responded back with only so many.
	 */
	if (copyin((caddr_t)cptr, kcmd, sizeof(grio_cmd_t)*grio_numcmds)) {
		kmem_free(kcmd, numbytes);
		return(EFAULT);
	}
	s = GRIO_GLOB_LOCK();

	/*
	 * Check that the request being responded to matches the
	 * one that issued the request. The request could have been
	 * removed from the queue if the requesting process was killed.
	 * We need to figure out a better way to affirm that we are
	 * responding back to the same request that we read from the q.
	 */
	if ((!grio_q) ||
	    (grio_q->num_cmds != grio_numcmds) ||
	    (grio_q->griocmd->gr_procid != kcmd->gr_procid)) {
		GRIO_GLOB_UNLOCK(s);
		kmem_free(kcmd, numbytes);
		return(ENOENT);
	}

	/*
	 * Copy from temp kernel buffer to grio q.
	 */
	bcopy(kcmd, grio_q->griocmd, numbytes);

	/*
	 * Remove the request from the queue.
	 */
	griocqp = grio_q;
	if (grio_q->back == grio_q ) {
		ASSERT( grio_q->forw == grio_q );
		grio_q = NULL;
	} else {
		grio_q = griocqp->back;
		grio_q->forw = griocqp->forw;
		griocqp->forw->back = grio_q;
		griocqp->forw = griocqp->back = NULL;
	}

	/*
	 * Assert is not always valid. There is a race condition
	 * where the semaphore count will become nonzero slightly
	 * before the queue pointer is set.
	 ASSERT((ggqp->sema.s_count == 0) || (ggqp->sema.s_queue));
	 */

	GRIO_GLOB_UNLOCK(s);
	vsema(&griocqp->sema);
	grio_numcmds = 0;
	kmem_free(kcmd, numbytes);
	return(0);
}



/* 
 * grio_read_num_cmds()
 *	This routine is called by the grio daemon to find out the
 *	number of sub-actions in the next "action list" item
 * RETURNS:
 *      0 on success
 *      2 if no requests are present
 *      -1 if there is a request present but an error occured during 
 *	the copyout.
 */
int
grio_read_num_cmds(sysarg_t  ncmds, sysarg_t cptr)
{
	int			num_cmds = 0, s;
	grio_cmd_t              griocmd;
	timespec_t		ts, rts;

	/*
	 * Copy the grio blk to kernel memory.
	 * This is only done to get the grio_wait_time field.
	 */
	if (copyin((caddr_t)cptr, &griocmd, sizeof(grio_cmd_t))) {
		dbg1printf(("copyin failed \n"));
		return(-1);
	}

	s = GRIO_GLOB_LOCK();

	if(NULL == grio_q) {
		/*
		 * Empty queue.
		 */

		/*
		 * The grio_wait_time is the time in seconds
		 * until the ggd daemon has to wakeup.
		 */
		if ( griocmd.gr_time_to_wait > 0 ) {

			/*
			 * Wait until the timeout expires or
			 * something is placed on the queue.
			 */
			ts.tv_sec = griocmd.gr_time_to_wait;
			ts.tv_nsec = 0;
			sv_timedwait_sig(&grio_q_sv, 0, &grio_global_lock,
				s, 1, &ts, &rts);
	
			s = GRIO_GLOB_LOCK();
			if ( grio_q == NULL ) {
			/*
			 * If there is still nothing on the queue, return.
			 */
				GRIO_GLOB_UNLOCK( s );
				return( 2 );
			}
		} else {
			GRIO_GLOB_UNLOCK( s );
			return( 2 );
		}
	}

	num_cmds = grio_q->num_cmds;

	GRIO_GLOB_UNLOCK( s );

	/*
	 * Keep track of what we are telling ggd.
	 */
	grio_numcmds = num_cmds;

	if (copyout(&num_cmds, (caddr_t) ncmds, sizeof(int))) {
		grio_numcmds = 0;
		return -1;
	}

	return 0;
}



/*
 * grio_read_grio_req()
 *      This routine is called by the grio daemon to read the next
 *      grio request from the queue.
 *
 * RETURNS:
 *      0 on success
 *	1 if an async request failed copyout and is lost forever.
 *	2 if the # subcmds in 1st q element changed from what we had 
 *	  told ggd previously (process took off request from q).
 *      3 if a sync request failed copyout. 
 */
int
grio_read_grio_req( sysarg_t cptr )
{
	int                     s, numbytes, asyncreq = 0, ret = 0;
	grio_cmd_queue_t        *griocqp;
	grio_cmd_t		*kcmd;

	s = GRIO_GLOB_LOCK();

	/* 
	 * Number checks.
	 */
	if (grio_numcmds != grio_q->num_cmds) {
		GRIO_GLOB_UNLOCK(s);
		return(2);
	}

	/*
	 * Now get some kernel memory to temporarily copyout the
	 * data from the first element, so that we do not have to 
	 * hold the grio lock while copying out to user memory
	 * and possibly going to sleep.
	 */
	numbytes = sizeof(grio_cmd_t) * grio_numcmds;
	kcmd = kmem_alloc(numbytes, KM_NOSLEEP);
	if (kcmd == 0) {
		GRIO_GLOB_UNLOCK(s);
		return(3);
	}

	bcopy(grio_q->griocmd, kcmd, numbytes); 


	if ((GRIO_ASYNC_CMD((grio_q->griocmd))) &&
		(grio_q->num_cmds == 1)) {

		/*
	 	 * This is an asynchronous request, no one is waiting for it
	 	 * to complete so remove the request from the queue.
	 	 */
			asyncreq = 1;
			griocqp = grio_q;
			if (grio_q->back == grio_q ) {
				ASSERT( griocqp == grio_q );
				ASSERT( grio_q->forw == grio_q);
				grio_q = NULL;
			} else {
				grio_q = griocqp->back;
				grio_q->forw = griocqp->forw;
				griocqp->forw->back = grio_q;
				griocqp->forw = griocqp->back = NULL;
			}
	
			GRIO_GLOB_UNLOCK( s );
			kmem_free(griocqp->griocmd, 
					sizeof(grio_cmd_t) * griocqp->num_cmds);
			kmem_free(griocqp, sizeof(grio_cmd_queue_t));
			grio_numcmds = 0;
	} else {
		/*
	 	 * Leave the synchronous request on the queue until
	 	 * the daemon has a response.
	 	 */
		GRIO_GLOB_UNLOCK(s);
	}

	/*
	 * Now copy request to user space. Note that if we fail for
	 * async requests, the request is lost. Should we tag the
	 * request at the end again?
	 * GGD should have allocated enough space (obtainable through
	 * grio_read_num_cmds wrapper); otherwise, kernel will happily
	 * over-write GGD data structures!
	 */
	if (copyout(kcmd, (caddr_t)cptr, numbytes)) {
		if (asyncreq)
			ret = 1;
		else
			ret = 3;
	}

	/*
	 * Free up the temporary kernel buffer.
	 */
	kmem_free(kcmd, numbytes);

	return(ret);
}


/*
 * grio_add_msg_to_q_and_wait()
 *      This routine takes the given grio msg pointer, places it on the
 *      end of the global msg list, and then waits for a response from
 *      the grio daemon.
 *
 * RETURNS:
 *      0 on success
 *      -1 on failure
 */
STATIC int
grio_add_msg_to_q_and_wait( grio_cmd_queue_t *griocqp)
{
	int     s, ret;

	/*
	 * Initialize the semaphore.
	 */
	initnsema(&griocqp->sema, 0, "grio_q");

	/*
	 * Place the message on the queue.
	 */
	s = GRIO_GLOB_LOCK();
	if (!grio_q) {
		grio_q = griocqp;
		griocqp->forw = griocqp;
		griocqp->back = griocqp;
	} else {
		griocqp->forw = grio_q->forw;
		grio_q->forw->back = griocqp;
		griocqp->back = grio_q;
		grio_q->forw = griocqp;
	}
	GRIO_GLOB_UNLOCK( s );

	sv_signal(&grio_q_sv);

	if (ret = grio_wait_for_msg_response( griocqp ) ) {
	/*
	 * The wait terminated due to a signal.
	 * Check it the message is still on the queue and
	 * remove it if necessary.
	 */
		grio_remove_msg_from_queue( griocqp );
	}

	ASSERT(!grio_remove_msg_from_queue( griocqp ));

	/*
	 * Message has already been removed from the queue.
	 * Just free the semaphore here.
	 */
	freesema( &griocqp->sema );
	return( ret );
}

/*
 * grio_remove_msg_from_queue()
 *      This routine checks if the given message is on
 *      the grio message queue and removes it.
 *
 *      The GRIO_GLOB_LOCK() must be held.
 *
 * RETURNS:
 *      none
 */
STATIC int
grio_remove_msg_from_queue( grio_cmd_queue_t *griocqp )
{
	int                     s, ret = 0;
	grio_cmd_queue_t        *gqp;

	s = GRIO_GLOB_LOCK();
	if (grio_q) {
		/*
	 	 * Check if the msg is in the queue
	 	 */
		gqp = grio_q->forw;
		while ((gqp != griocqp) && (gqp != grio_q))
			gqp  = gqp->forw;
		if (gqp == griocqp) {
			/*
	 	 	 * Remove the request from the queue.
		 	 */
			if (grio_q->back == grio_q ) {
				ASSERT( grio_q->forw == grio_q );
				ASSERT( grio_q == griocqp );
				ASSERT( gqp == griocqp );
				grio_q = NULL;
			} else {
				if (gqp == grio_q ) {
					grio_q = gqp->back;
				}
				gqp->forw->back = gqp->back;
				gqp->back->forw = gqp->forw;
	
				gqp->forw = gqp->back = NULL;
				ASSERT(gqp == griocqp);
			}
			ret = 1;
		}
	}
	GRIO_GLOB_UNLOCK( s );
	return ( ret );
}




/*
 * grio_add_async_msg_to_q
 *
 *
 * RETURNS:
 *      0 if request was added to queue
 *      1 if queue was full.
 */
STATIC int
grio_add_async_msg_to_q( grio_cmd_queue_t *gqp)
{
	int                     s, count = 0;
	grio_cmd_queue_t        *ptr;

	s = GRIO_GLOB_LOCK();

	/*
	 * Place the message on the queue.
	 */
	if (!grio_q) {
		grio_q = gqp;
		gqp->forw = gqp;
		gqp->back = gqp;
	} else {
		ptr = grio_q->forw;
		/*
		 * Count the requests currently in the queue.
		 */
		while ( ptr != grio_q ) {
			count++;
			ptr = ptr->forw;
		}

		/*
		 * If the count is too large,
		 * ggd may not be running.
		 */
		if (count > MAX_GRIO_QUEUE_COUNT) {
			GRIO_GLOB_UNLOCK( s );
			printf("Grio queue overflow - async message lost.\n");
			return( 1 );
		}

		gqp->forw = grio_q->forw;
		grio_q->forw->back = gqp;
		gqp->back = grio_q;
		grio_q->forw = gqp;
	}
	GRIO_GLOB_UNLOCK( s );
	sv_signal(&grio_q_sv);

	return( 0 );
}



/*
 * grio_bp_queue
 *	Routine to call disk drive strategy routine.
 *	This is called by the bpqueue daemon at base level.
 *
 * RETURNS:
 *	none
 */
void
grio_start_subreq(buf_t *bp)
{
	struct bdevsw *my_bdevsw;
#ifdef GRIO_DEBUG
	grio_disk_info_t	*griodp;
#endif

	ASSERT( BUF_IS_GRIO_ISSUED(bp) );

#ifdef LATER
#ifdef GRIO_DEBUG
	griodp = grio_get_disk_info( bp->b_edev );
	if ( (lbolt - griodp->time_start ) > 3 ) {
		printf("GRIO: %d ticks to get thru queue (dks%dd%d)\n",
			lbolt - griodp->time_start,
			SCSI_EXT_CTLR(DKSC_CTLR(griodp->diskdev)),
			DKSC_UNIT( griodp->diskdev ) );

	}
#endif
#endif
	my_bdevsw = get_bdevsw(bp->b_edev);
	ASSERT(my_bdevsw != NULL);
	bdstrat( my_bdevsw, bp  );
}



/*
 * grio_wait_for_msg_response()
 *      This routine waits on the message semaphore until the
 *      grio daemon completes the request.
 *
 * RETURNS:
 *      0 on success
 *      EINTR if the process was killed by a signal.
 */
STATIC int
grio_wait_for_msg_response( grio_cmd_queue_t *griomsgq )
{
	if ( psema(&griomsgq->sema, (PZERO + 1)) ) {
		return( EINTR );
	}
	return( 0 );
}


static void
bpsubq(void)
{
	bpqueue_init(&grio_subreq_bpqueue);
	bpqueuedaemon( &grio_subreq_bpqueue, grio_start_subreq);
}

/*
 * grio_start_bpqueue()
 *	Initialize the grio bp queue.
 *	The bp queue is used to initiate i/o requests from 
 *	base level rather than at interrupt time.
 *
 * RETURNS:
 *	none
 */
void
grio_start_bpqueue(void)
{
	extern int bpsqueue_pri;

	sthread_create("bpsqueue", 0, 0, 0, bpsqueue_pri, KT_PS,
			(st_func_t *)bpsubq, 0, 0, 0, 0); 
	return;
}
