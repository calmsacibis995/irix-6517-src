/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_dispatch.c,v 65.5 1998/04/02 19:44:18 bdr Exp $";
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

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_dispatch.c,v 65.5 1998/04/02 19:44:18 bdr Exp $")

/*
 * This file contains:
 *
 *		tpq_DispatcherThread()		This implements the dispatcher thread.  It
 *						is called only at init time, ans loops.
 *
 *		tpq_WakeDispatcher()		This wakes up the dispatcher from an osi_Wait().
 */

extern void tpq_GCThreads _TAKES((struct tpq_threadPool *));
extern struct tpq_queueEntry *tpq_FindNext _TAKES((struct tpq_threadPool *,
						  int, long, long *, int *));
extern int tpq_GrowThreadPool _TAKES((struct tpq_threadPool *, int, struct tpq_threadPoolEntry **));

/*
 * This function implements the dispatch function for the thread pool.
 */
/* SHARED */
void tpq_DispatcherThread(poolP)
  IN struct tpq_threadPool *poolP;
{
    int preempting;
    int priority;
    int graceExpired;		/* grace period has expired for this entry */
    long now;
    long nextExpire;		/* when next expiration will take place for queue */
    long interval;		/* amount of time to sleep */
    struct tpq_queueEntry *qEntryP;
#ifndef SGIMIPS
    struct tpq_queueEntry *qp;
#endif /* SGIMIPS */
    struct tpq_threadPoolEntry *threadP;	/* thread we're going to run on */
    preempting = osi_PreemptionOff();

    icl_Trace1(tpq_iclSetp, TPQ_ICL_DISPATCH, ICL_TYPE_POINTER, poolP);

    tpqLock_ObtainWrite(&poolP->poolLock);
    poolP->running = 1;
    tpqLock_ReleaseWrite(&poolP->poolLock);

    while(1)
    {
	int qActive;

	now = osi_Time();

	tpqLock_ObtainRead(&poolP->queueLock);
	qActive = poolP->active;
	tpqLock_ReleaseRead(&poolP->queueLock);
	if (!qActive)
	    break;		/* we're being shut down */

	/* search the queues and set qEntryP to the next one to run */
	interval = 10*60;	/* arbitrary wait */
	qEntryP = (struct tpq_queueEntry *)NULL;
	tpqLock_ObtainWrite(&poolP->queueLock);
	for(priority = 0; priority < TPQ_NPRIORITIES; priority++)
	{
	    qEntryP = tpq_FindNext(poolP, priority, now, &nextExpire, &graceExpired);
	    if (nextExpire < interval)
		interval = nextExpire;		/* find shortest non-zero interval */
	    if (qEntryP != (struct tpq_queueEntry *)NULL)
		break;
	}

	/* we either found something to run or we ran out of queues */
	if (qEntryP != (struct tpq_queueEntry *)NULL)
	{
	    icl_Trace4(tpq_iclSetp, TPQ_ICL_DISPATCH_READY, ICL_TYPE_POINTER, poolP,
		       ICL_TYPE_POINTER, qEntryP, ICL_TYPE_LONG, priority,
		       ICL_TYPE_LONG, graceExpired);

	    /* we have something to run, find a thread, and leave it
	     * locked so that it can't get killed after we find it
	     * and before we give it work.
	     */
	    tpqLock_ObtainRead(&poolP->poolLock);
	    for(threadP = poolP->threads; threadP; threadP = threadP->next)
	    {
		tpqLock_ObtainWrite(&threadP->threadLock);
		if ((threadP->sleeping) && !(threadP->die) &&
		    (threadP->privateQueue == (struct tpq_queueEntry *)NULL))
		    break;
		tpqLock_ReleaseWrite(&threadP->threadLock);
	    }
	    tpqLock_ReleaseRead(&poolP->poolLock);
		
	    if (!threadP)
	    {
		/* no thread available, see if we need to make one */
		if (graceExpired)
		{
		    /* the grace period has expired, try to make a new thread */
		    if (tpq_GrowThreadPool(poolP, priority, &threadP) != 0)
		    {
			/* can't make one */
			threadP = (struct tpq_threadPoolEntry *)NULL;	
		    }
		    /* It's returned locked. */
		}
		else	/* grace period not expired */
		{
		    /*
		     * this one's grace period hasn't expired but it's 
		     * possible that something else in a lower queue has
		     * an expired grace period that would force us to try
		     * to clear out the stuff ahead of it.  So, try to 
		     * create a new thread for the lower priority.
		     */
		    priority++;		/* next priority down */
		    for(; priority < TPQ_NPRIORITIES; priority++)
		    {
			qEntryP = tpq_FindNext(poolP, priority, now,
					       &nextExpire, &graceExpired);
			if (nextExpire < interval)
			    interval = nextExpire;	/* find shortest interval */
			if (graceExpired)
			{
			    /* found one that's expired -- try to make a thread */
			    if (tpq_GrowThreadPool(poolP, priority, &threadP) != 0)
			    {
				/* couldn't make one */
				threadP = (struct tpq_threadPoolEntry *)NULL;
			    }
			    /* It's returned locked. */
			    /* either we made one or there's no point going lower */
			    break;
			}
		    }
		}
	    } /* if (!threadP) */

	    /* If we have a thread now, go ahead and schedule it to run.
	     * Thread is write-locked if non-null.
	     */
	    if (threadP)
	    {
		icl_Trace3(tpq_iclSetp, TPQ_ICL_DISPATCH_THREAD, ICL_TYPE_POINTER, poolP,
			   ICL_TYPE_POINTER, qEntryP, ICL_TYPE_POINTER, threadP);

		TPQ_UNQUEUE(poolP, qEntryP);
		threadP->privateQueue = qEntryP;
		tpqLock_ReleaseWrite(&threadP->threadLock);
		tpq_Wakeup(threadP);
		/* process the next thing in the queue immediately (no wait) */
		interval = 0;
	    }
	} /* if (qEntryP != (struct tpq_queueEntry *)NULL) */
	else	/* nothing to run */
	{   
	    /* nothing to process at this point */
	    /* check for threads that need to be killed off */
	    tpq_GCThreads(poolP);
	}
	/* we need to wait for interval seconds */
	tpqLock_ReleaseWrite(&poolP->queueLock);
	icl_Trace2(tpq_iclSetp, TPQ_ICL_DISPATCH_INTERVAL, ICL_TYPE_POINTER, poolP,
		   ICL_TYPE_LONG, interval);

	if (interval)
	    osi_Wait(interval*1000, &poolP->waitHandler, 0);
	icl_Trace1(tpq_iclSetp, TPQ_ICL_DISPATCH_WAKEUP, ICL_TYPE_POINTER, poolP);
    } /* while(poolP->running) */

    tpqLock_ObtainWrite(&poolP->poolLock);
    poolP->running = 0;
    tpqLock_ReleaseWrite(&poolP->poolLock);

    icl_Trace1(tpq_iclSetp, TPQ_ICL_DISPATCH_DEAD, ICL_TYPE_POINTER, poolP);
    tpq_Wakeup(poolP);		/* signal killing thread */
    osi_RestorePreemption(preempting);	/* release kernel lock */
}

/* SHARED */
void tpq_WakeDispatcher(poolP)
  IN struct tpq_threadPool *poolP;
{
    icl_Trace1(tpq_iclSetp, TPQ_ICL_WAKEUP, ICL_TYPE_POINTER, poolP);
    osi_CancelWait(&poolP->waitHandler);
}
