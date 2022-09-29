/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_queue.c,v 65.5 1998/03/23 16:28:00 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1994, 1992 Transarc Corporation - All rights reserved */
#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>

#include <tpq.h>
#include <tpq_private.h>

#if !defined(KERNEL)
#include <dce/pthread.h>
#endif /* !KERNEL */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_queue.c,v 65.5 1998/03/23 16:28:00 gwehrman Exp $")

/*
 * This module includes:
 *
 *		EnqueueEntry()		A private function to handle placing
 *					stuff on the queues.
 *
 *		tpq_QueueRequest()	This is the exported interface to
 *					queue operations on the queues.  It
 *					calls EnqueueEntry() to do the actual
 *					queuing.
 *
 *		tpq_DequeueRequest()	This is the external interface to cause a
 *					queued item to be removed from the queue and
 *					destroyed.
 *
 *		GCQueue()		Clean destroyed and dropped-dead entries
 *					from queues.
 *
 *		tpq_FindNext()		Return the next thing from the queue to
 *					to run plus useful time-out information.
 */

extern tpq_lock_data_t tpq_freeListLock;
extern struct tpq_queueEntry *tpq_qFreeList;
extern void tpq_WakeDispatcher _TAKES((struct tpq_threadPool *));

/*
 * Function that threads the qEntry on to the correct queue.  It wakes up the
 * dispatcher thread after queuing.
 * Called with no locks held.
 */
/* SHARED */
#ifdef SGIMIPS
void EnqueueEntry(
  IN struct tpq_threadPool *poolP,
  IN struct tpq_queueEntry *qEntryP)
#else
void EnqueueEntry(poolP, qEntryP)
  IN struct tpq_threadPool *poolP;
  IN struct tpq_queueEntry *qEntryP;
#endif
{
    int queueIsActive;
    struct tpq_queueEntry *qp;

    osi_assert(qEntryP && *qEntryP->op);

    /* make sure queue is active */
    tpqLock_ObtainRead(&poolP->queueLock);
    queueIsActive = poolP->active;
    tpqLock_ReleaseRead(&poolP->queueLock);

    /* check to see if we're supposed to destroy this request first */
    if (qEntryP->destroy || !queueIsActive)
    {
	tpqLock_ObtainWrite(&tpq_freeListLock);
	qEntryP->next = tpq_qFreeList;
	tpq_qFreeList = qEntryP;
	tpqLock_ReleaseWrite(&tpq_freeListLock);
	return;
    }

    icl_Trace4(tpq_iclSetp, TPQ_ICL_ENQ, ICL_TYPE_POINTER, poolP,
	       ICL_TYPE_POINTER, qEntryP, ICL_TYPE_LONG, qEntryP->priority,
	       ICL_TYPE_LONG, qEntryP->gracePeriod); 

    /* find the right queue and the right place in the queue and insert */
    /* sort with lowest time (earliest) at the head */
    tpqLock_ObtainWrite(&poolP->queueLock);
    qEntryP->mark = 0;		/* entry unmarked (as yet) by status function */
    if (poolP->queue[qEntryP->priority] == (struct tpq_queueEntry *)NULL)
    {
	poolP->queue[qEntryP->priority] = qEntryP;
	qEntryP->next = qEntryP->prev = qEntryP;
    }
    else
    {
	/*
	 * We are trying to maintain a circular list sorted with the
	 * earliest entries at the head (as pointed to by the pool
	 * structure.
	 */
	qp = poolP->queue[qEntryP->priority];
	do
	{
	    if (qEntryP->executionTime < qp->executionTime)
	    {
		if (qp == poolP->queue[qEntryP->priority])
		    poolP->queue[qEntryP->priority] = qEntryP;	/* head of list */
		break;
	    }
	    qp = qp->next;
	} while (qp != poolP->queue[qEntryP->priority]);

	/*
	 * When we get here, we always insert in front of qp.  If we found a place
	 * in the queue to insert in front of, it will be pointed to by qp, so
	 * inserting in front of it is intuitive.  If we didn't find a place, we
	 * need to insert at the end of the list; but qp will be pointing to the
	 * beginning of the list again, so inserting in front of it will be at the
	 * end.  The special case is if we have to insert at the beginning of the
	 * list.  This handling is done in the loop.
	 */
	qEntryP->next = qp;
	qEntryP->prev = qp->prev;
	qp->prev->next = qEntryP;
	qp->prev = qEntryP;
    }
    tpqLock_ReleaseWrite(&poolP->queueLock);
    tpq_WakeDispatcher(poolP);
}

/*
 * This is the exported interface for queuing new requests.  It returns an opaque
 * pointer to the queueEntry structure that is to be used as a handle should the
 * operation need to be aborted at a later date.
 */
/* EXPORT */
void *tpq_QueueRequest(queueHandle, op, arg, priority, gracePeriod,
			      rescheduleInterval, dropDeadTime)
  IN  void *queueHandle;
  IN  void (*op)_TAKES((void *, void *));
  IN  void *arg;
  IN  int priority;
  IN  long gracePeriod;
  IN  long rescheduleInterval;
  IN  long dropDeadTime;
{
    int qActive;
    struct tpq_threadPool *poolP = (struct tpq_threadPool *)queueHandle;
    struct tpq_queueEntry *qEntryP;

    icl_Trace4(tpq_iclSetp, TPQ_ICL_QREQ, ICL_TYPE_POINTER, poolP,
	       ICL_TYPE_POINTER, op, ICL_TYPE_LONG, arg,
	       ICL_TYPE_LONG, priority); 
    icl_Trace3(tpq_iclSetp, TPQ_ICL_QREQ2, ICL_TYPE_LONG, gracePeriod,
	       ICL_TYPE_LONG, rescheduleInterval, ICL_TYPE_LONG, dropDeadTime);

    /* check to make sure pool is active (i.e., not being shutdown */
    tpqLock_ObtainRead(&poolP->queueLock);
    qActive = poolP->active;
    tpqLock_ReleaseRead(&poolP->queueLock);

    if (!qActive)
	return((void *)NULL);		/* queue operation failed */

    /* allocate a new entry */
    tpqLock_ObtainWrite(&tpq_freeListLock);
    if (tpq_qFreeList)
    {
	qEntryP = tpq_qFreeList;
	tpq_qFreeList = qEntryP->next;
	tpqLock_ReleaseWrite(&tpq_freeListLock);
    }
    else
    {
	tpqLock_ReleaseWrite(&tpq_freeListLock);
	/* may want to allocate more than one? */
	qEntryP = (struct tpq_queueEntry *)osi_Alloc(sizeof(struct tpq_queueEntry));
    }

    qEntryP->destroy = 0;
    qEntryP->op = op;
    qEntryP->arg = arg;
    qEntryP->priority = priority;
    qEntryP->gracePeriod = gracePeriod;
    qEntryP->rescheduleInterval = rescheduleInterval;
    qEntryP->dropDeadTime = dropDeadTime;

    /*
     * If this thing is periodic, let's wait for the period to end so that
     * the requests on initialization don't overwhelm us.  If this thing is
     * one time only (rescheduleTime == 0), do it now.
     */
    qEntryP->executionTime = osi_Time() + rescheduleInterval;
    EnqueueEntry(poolP, qEntryP);

    return((void *)qEntryP);
}

/*
 * This is the external interface to destroying a queued operation.  Since this
 * operation may be in the process of being run, we will just set the destroy
 * flag in the structure.  When this flag is spotted, either by trying to
 * requeue the operation or by reading requests from the queue, it will be
 * freed.
 */
/* EXPORT */
void tpq_DequeueRequest(queueHandle, requestHandle)
  IN void *queueHandle;
  IN void *requestHandle;
{
    struct tpq_threadPool *poolP;
    struct tpq_queueEntry *qEntryP;

    icl_Trace2(tpq_iclSetp, TPQ_ICL_DEREQ, ICL_TYPE_POINTER, queueHandle,
	       ICL_TYPE_POINTER, requestHandle);

    poolP = (struct tpq_threadPool *)queueHandle;
    qEntryP = (struct tpq_queueEntry *)requestHandle;

    tpqLock_ObtainWrite(&poolP->queueLock);
    qEntryP->destroy = 1;
    tpqLock_ReleaseWrite(&poolP->queueLock);
}

/*
 * clean destroyed and "dropped dead" entries from queue.
 * The queue lock is held by caller.
 */
#ifdef SGIMIPS
static void GCQueue(
  IN struct tpq_threadPool *poolP,
     int priority,
  IN long now)
#else
static void GCQueue(poolP, priority, now)
  IN struct tpq_threadPool *poolP;
  IN long now;
#endif
{
    int changeBeginning;			/* change beginning of list? */
    struct tpq_queueEntry *qp;
    struct tpq_queueEntry *nextp;

    icl_Trace3(tpq_iclSetp, TPQ_ICL_GCQ, ICL_TYPE_POINTER, poolP,
	       ICL_TYPE_LONG, priority, ICL_TYPE_LONG, now); 
 GCQueueLoop:
    if (!(qp = poolP->queue[priority]))
	return;
    do{
	nextp = qp->next;
	if (qp->destroy || (qp->dropDeadTime && (qp->dropDeadTime <= now)))
	{
	    changeBeginning = (qp == poolP->queue[priority]);
	    TPQ_UNQUEUE(poolP, qp);
	    tpqLock_ObtainWrite(&tpq_freeListLock);
	    qp->next = tpq_qFreeList;
	    tpq_qFreeList = qp; 
	    tpqLock_ReleaseWrite(&tpq_freeListLock);

	    /* if we've changed the beginning of the list, start over */
	    if (changeBeginning)
		goto GCQueueLoop;
	}
	qp = nextp;
    } while(qp != poolP->queue[priority]);
}

/*
 * This function returns the next entry to be processed from the queue.
 * If there is none that is currently ready to run, NULL is returned and
 * the amount of time until the next expire is returned.
 *
 * Called with the pool structure queue lock held.
 *
 * If return value is NULL:
 *	There is nothing in the queue to process at this point.  nextExpiredP
 *	is set to contain the time until the next wake-up is required.
 * If return value is non-NULL:
 *	If the grace period of the request has expired, *graceExpired is set to 1.
 *	If the grace period has not expired, *graceExpired is 0 and *nextExpiredP
 *	is set to the shortest grace period of all runnable things in the queue.
 *	This is done in case we don't have any available threads.  The returned
 *	interval in *nextExpiredP is always non-zero (since waiting for 0 seconds
 *	in the event there are no threads available is not very useful).  If there
 *	is nothing on the queue to run, an arbitrary value is put there.
 *
 * Note that the returned pointer refers to a structure still on the queue.  This
 * structure must be removed by the caller before it can be run.
 */
/* SHARED */
struct tpq_queueEntry *tpq_FindNext(poolP, priority, now, nextExpireP, graceExpiredP)
  IN struct tpq_threadPool *poolP;	/* thread pool structure */
  IN int priority;				/* which queue to use */
  IN long now;				/* current time */
  OUT long *nextExpireP;		/* ptr to when next will be ready to run */
  OUT int *graceExpiredP;		/* grace period has expired */
{
    register struct tpq_queueEntry *qp;
    int foundExpired;			/* found an entry with an expired grace period */
    long interval;
    long remainder;
    struct tpq_queueEntry *queueP;

    icl_Trace3(tpq_iclSetp, TPQ_ICL_FINDQ, ICL_TYPE_POINTER, poolP,
	       ICL_TYPE_LONG, priority, ICL_TYPE_LONG, now); 

    *graceExpiredP = 0;
    GCQueue(poolP, priority, now);	/* clean junk out of queue */
    queueP = poolP->queue[priority];
    if (queueP == (struct tpq_queueEntry *)NULL)
    {
	*nextExpireP = 10*60;		/* nothing here, wait arbitrary 10 minutues */
	icl_Trace4(tpq_iclSetp, TPQ_ICL_FINDQ_RET, ICL_TYPE_POINTER, poolP,
		   ICL_TYPE_LONG, *nextExpireP, ICL_TYPE_LONG, *graceExpiredP,
		   ICL_TYPE_POINTER, queueP); 
	
	return((struct tpq_queueEntry *)NULL);
    }

    /* is there at least one runnable item on this queue? */
    if (queueP->executionTime > now)
    {
	/*
	 * Nothing to run yet.  Record time until this guy is runnable
	 * in *nextExpireP.
	 */
	*nextExpireP = queueP->executionTime - now;
	icl_Trace4(tpq_iclSetp, TPQ_ICL_FINDQ_RET, ICL_TYPE_POINTER, poolP,
		   ICL_TYPE_LONG, *nextExpireP, ICL_TYPE_LONG, *graceExpiredP,
		   ICL_TYPE_POINTER, 0); 
	
	return((struct tpq_queueEntry *)NULL);
    }

    /*
     * Since the first thing on the queue is runnable, we have something ready to
     * run.  We want to check first, however, if there is a runnable item whose
     * grace period has expired.  If so, we'll take the first one of these.  We
     * have an additional problem that there might not be a thread available to
     * run this item so we have to return a reasonable interval to wait.  The value
     * we return is the smallest of the remaining unexpired grace periods and
     * the time until the next runnable item is ready to run.  If there is nothing
     * but expired grace periods on the queue, an arbitrary 10 minute wait is
     * returned.
     */
    foundExpired = 0;
    interval = 10*60;	/* arbitrary 10 minute wait */
    qp = poolP->queue[priority];
    do{
	remainder = qp->executionTime - now;
	if (remainder > 0)
	{
	    /* this guy is not yet ready to run, but we may need his interval */
	    if (remainder < interval)
		interval = remainder;	/* save it only if it's shortest non-zero */
	    break;	/* done (no more runnables on the queue) */
	}

	/* try to locate entries whose grace periods have expired and return first */
	remainder = qp->executionTime + qp->gracePeriod - now;
	if (remainder <= 0)
	{
	    /* found an expired grace period -- return the first one */
	    if (!foundExpired)
	    {
		foundExpired = 1;
		queueP = qp;
	    }
	}
	else
	{
	    /* grace period has not yet expired.  Look at interval until it does */
	    /* find shortest remaining (non-zero) grace period */
	    if (remainder < interval)
		interval = remainder;
	}
	qp = qp->next;
    } while(qp != poolP->queue[priority]);
    *graceExpiredP = foundExpired;
    *nextExpireP = interval;

    icl_Trace4(tpq_iclSetp, TPQ_ICL_FINDQ_RET, ICL_TYPE_POINTER, poolP,
	       ICL_TYPE_LONG, *nextExpireP, ICL_TYPE_LONG, *graceExpiredP,
	       ICL_TYPE_POINTER, queueP); 
    
    return(queueP);
}
