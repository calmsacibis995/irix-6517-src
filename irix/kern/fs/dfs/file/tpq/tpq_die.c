/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_die.c,v 65.4 1998/03/23 16:28:01 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1992 Transarc Corporation - All rights reserved */
#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>

#include <tpq.h>
#include <tpq_private.h>

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */
    
RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_die.c,v 65.4 1998/03/23 16:28:01 gwehrman Exp $")

/*
 * This module contains: 
 *	tpq_ShutdownPool()	This is an exported function that shuts down the
 *				helper threads than frees up the pool structure.
 *
 *	tpq_HelperDie()		This is an internal function that is called by
 *				a helper thread when it wants to die.
 *
 *	tpq_GCThreads()		This is an internal function that reclaims threads
 *				after they have been sleeping for longer than the 
 *				specified parameter (in the event where more than
 *				the minimum number of threads are running).
 */
extern tpq_lock_data_t tpq_freeListLock;
extern struct tpq_queueEntry *tpq_qFreeList;
extern void tpq_WakeDispatcher _TAKES((struct tpq_threadPool *));
/*
 * This function:
 *	1. Signals all helper threads in the pool to die by setting the 'die'
 *	flag in each entry structure.
 *	2. Wakes up each helper thread.
 *	3. Waits for them all to run.  When the last helper thread dequeues itlsef
 *	from the pool structure, it wakes up the pool structure.
 *	4. Free up pool structure memory.
 */
/* EXPORT */
void tpq_ShutdownPool(poolHandle)
  IN void *poolHandle;
{
    int i;
    int wasActive;
    struct tpq_threadPoolEntry *entryP;
    struct tpq_threadPool *poolP;

    icl_Trace1(tpq_iclSetp, TPQ_ICL_SHUTDOWN, ICL_TYPE_POINTER, poolHandle);
    poolP = (struct tpq_threadPool *)poolHandle;

    tpqLock_ObtainWrite(&poolP->queueLock);
    wasActive = poolP->active;
    poolP->active = 0;		/* disallow queuing of more requests */
    tpqLock_ReleaseWrite(&poolP->queueLock);

    if (!wasActive)
	return;		/* let only one of these guys through */

    /* go ahead and tell the other threads to die and wake them up */
    tpq_WakeDispatcher(poolP);	/* let dispatcher thread go away */
    tpqLock_ObtainWrite(&poolP->poolLock);
    for(entryP = poolP->threads; entryP; entryP = entryP->next)
    {
	tpqLock_ObtainWrite(&entryP->threadLock);
	entryP->die = 1;
	tpq_Wakeup(entryP);
	tpqLock_ReleaseWrite(&entryP->threadLock);
	poolP->nthreads--;
    }

    /* sleep if there are any threads out there */
    while(poolP->threads != (struct tpq_threadPoolEntry *)NULL)
    {
	tpq_SleepW(&poolP->threads, &poolP->poolLock);
	tpqLock_ObtainWrite(&poolP->poolLock);
    }

    /* shut down dispatcher thread */
    while(poolP->running)
    {
	tpq_SleepW(poolP, &poolP->poolLock);
	tpqLock_ObtainWrite(&poolP->poolLock);
    }

    tpqLock_ReleaseWrite(&poolP->poolLock);

    /* 
     * Now, all the threads should have been killed off so there should be
     * operations in transit.  We can now free tham all up.
     */
    tpqLock_ObtainWrite(&poolP->queueLock);
    tpqLock_ObtainWrite(&tpq_freeListLock);
    for(i = 0; i < TPQ_NPRIORITIES; i++)
    {
	if (poolP->queue[i])
	{
	    /* graft whole queue onto free list */
	    /* point last entry to beginning of existing free list */
	    poolP->queue[i]->prev->next = tpq_qFreeList;
	    tpq_qFreeList = poolP->queue[i];
	    poolP->queue[i] = (struct tpq_queueEntry *)NULL;
	}
    }
    tpqLock_ReleaseWrite(&tpq_freeListLock);
    tpqLock_ReleaseWrite(&poolP->queueLock);

    /* okay to free up the structure at this point */
    osi_Free(poolP, sizeof(struct tpq_threadPool));
    icl_Trace1(tpq_iclSetp, TPQ_ICL_SHUTDOWN_END, ICL_TYPE_POINTER, poolHandle);
}

/*
 * This function is called when a helper thread is putting itself to death.  It
 * unthreads itself from the pool thread queue and frees its storage.  It also 
 * decrements the thread count in the pool structure.  Finally, it wakes up the
 * killing thread if it is the last helper.
 *
 * Must be called with poolP->poolLock held.
 */
/* SHARED */
void tpq_HelperDie(poolP, entryP)
  IN struct tpq_threadPool *poolP;
  IN struct tpq_threadPoolEntry *entryP;
{
    struct tpq_threadPoolEntry *removeP;
    struct tpq_threadPoolEntry *removePrevP;

    icl_Trace2(tpq_iclSetp, TPQ_ICL_HELPERDIE, ICL_TYPE_POINTER, poolP,
	       ICL_TYPE_POINTER, entryP);

    for(removePrevP = (struct tpq_threadPoolEntry *)NULL, removeP = poolP->threads;
	removeP;
	removePrevP = removeP, removeP = removeP->next)
    {
	if (removeP == entryP)
	{
	    if (removePrevP == NULL)
		poolP->threads = removeP->next;		/* beginning of the list */
	    else
		removePrevP->next = removeP->next;	/* middle, preceeded by prev */

	    /* wakeup killing thread if this is the last one */
	    if (poolP->threads == (struct tpq_threadPoolEntry *)NULL)
		tpq_Wakeup(&poolP->threads);

	    osi_Free(removeP, sizeof(struct tpq_threadPoolEntry));
	    break;
	}
    }
}

/*
 * This function attempts to GC a thread that's been waiting for longer
 * than poolP->threadEnnui seconds to process a request.  This is an 
 * attempt to avoid the destroy-create-destroy etc scenario by building
 * in some kind of hysteresis.
 */
/* SHARED */
void tpq_GCThreads(poolP)
  IN struct tpq_threadPool *poolP;
{
    int nthreads;
    int foundOneToKill;
    long now;
    struct tpq_threadPoolEntry *entryP;

    icl_Trace1(tpq_iclSetp, TPQ_ICL_GC, ICL_TYPE_POINTER, poolP);
    now = osi_Time();
    tpqLock_ObtainRead(&poolP->poolLock);
    entryP = poolP->threads;
    for(nthreads = poolP->nthreads; nthreads > poolP->minThreads; nthreads--)
    {
	/* continue to check the thread list to find one to kill */
	/* entryP is not reset so we take up where we left off */
	for(foundOneToKill = 0 ; !foundOneToKill && entryP; entryP = entryP->next)
	{
	    tpqLock_ObtainWrite(&entryP->threadLock);
	    if (entryP->sleeping &&
		(entryP->privateQueue == (struct tpq_queueEntry *)NULL) &&
		(entryP->sleepTime + poolP->threadEnnui < now))
	    {
		icl_Trace2(tpq_iclSetp, TPQ_ICL_GC_DIE, ICL_TYPE_POINTER, poolP,
			   ICL_TYPE_POINTER, entryP);
		entryP->die = 1;
		tpq_Wakeup(entryP);
		poolP->nthreads--;
		foundOneToKill = 1;
	    }
	    tpqLock_ReleaseWrite(&entryP->threadLock);
	}
	/* quit if there aren't any more */
	if (entryP == (struct tpq_threadPoolEntry *)NULL)
	    break;
    }
    tpqLock_ReleaseRead(&poolP->poolLock);
}
