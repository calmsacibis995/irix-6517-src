/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_helper.c,v 65.4 1998/03/23 16:28:13 gwehrman Exp $";
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

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_helper.c,v 65.4 1998/03/23 16:28:13 gwehrman Exp $")

extern void tpq_HelperDie _TAKES((struct tpq_threadPool *,struct tpq_threadPoolEntry *));
extern struct tpq_queueEntry *tpq_FindNext _TAKES((struct tpq_threadPool *,
						   int, long, long *, int *));
extern tpq_lock_data_t tpq_freeListLock;
extern struct tpq_queueEntry *tpq_qFreeList;

/*
 * This file contains to code to run the helper threads.
 */
/* SHARED */
void tpq_HelperThread(pEntryP)
  IN struct tpq_threadPoolEntry *pEntryP;
{
    int i;
    int preempting;
    struct tpq_threadPool *poolP;
    struct tpq_queueEntry *qEntryP;

    /* We have to grab the kernel lock under the AIX kernel */
    preempting = osi_PreemptionOff();
    osi_MakeInitChild();	/* so at exit, someone wait(3)'s for us */

    poolP = pEntryP->threadPool;

    icl_Trace2(tpq_iclSetp, TPQ_ICL_HELPER, ICL_TYPE_POINTER, pEntryP, 
	       ICL_TYPE_POINTER, poolP);

    /* Eliminate race with the caller; let task assignment complete before
      we look for tasks. */
    icl_Trace2(tpq_iclSetp, TPQ_ICL_HELPER_INITLOCKWAIT,
		ICL_TYPE_POINTER, pEntryP, 
		ICL_TYPE_POINTER, poolP);
    tpqLock_ObtainWrite(&pEntryP->threadLock);
    /* At this point, if we're a product of GrowThreadPool, we know that
	the dispatcher has finished giving us our appointed task.
	We can release the per-thread lock and read our queues. */
    tpqLock_ReleaseWrite(&pEntryP->threadLock);
    icl_Trace2(tpq_iclSetp, TPQ_ICL_HELPER_INITLOCKDONE,
		ICL_TYPE_POINTER, pEntryP, 
		ICL_TYPE_POINTER, poolP);

    while(1)
    {
	tpqLock_ObtainWrite(&poolP->poolLock);
	tpqLock_ObtainWrite(&pEntryP->threadLock);
        pEntryP->sleeping = 0;
	/* see if this thread should commit suicide */
	if (pEntryP->die)
	{
	    icl_Trace1(tpq_iclSetp, TPQ_ICL_HELPER_DIE, ICL_TYPE_POINTER, pEntryP); 
	    tpqLock_ReleaseWrite(&pEntryP->threadLock);
	    tpq_HelperDie(poolP, pEntryP);
	    tpqLock_ReleaseWrite(&poolP->poolLock);
	    osi_RestorePreemption(preempting);	/* release kernel lock under AIX */
	    return;
	}

	/* check private queue to see if there's something to run */
	qEntryP = pEntryP->privateQueue;
	pEntryP->privateQueue = (struct tpq_queueEntry *)NULL;
	tpqLock_ReleaseWrite(&pEntryP->threadLock);
	tpqLock_ReleaseWrite(&poolP->poolLock);
	if (qEntryP != (struct tpq_queueEntry *)NULL)
	{
	    icl_Trace3(tpq_iclSetp, TPQ_ICL_HELPER_PVT, ICL_TYPE_POINTER, pEntryP,
		       ICL_TYPE_POINTER, qEntryP, ICL_TYPE_POINTER, poolP); 
	    TPQ_RUNOP(qEntryP);	/* run op */
	    icl_Trace3(tpq_iclSetp, TPQ_ICL_HELPER_PVT_DONE, ICL_TYPE_POINTER, pEntryP,
		       ICL_TYPE_POINTER, qEntryP, ICL_TYPE_LONG, qEntryP->rescheduleInterval); 
	    if (qEntryP->rescheduleInterval)
		TPQ_REQUEUE(poolP, qEntryP);
	    else
	    {
		/* return it to the free list */
		tpqLock_ObtainWrite(&tpq_freeListLock);
		qEntryP->next = tpq_qFreeList;
		tpq_qFreeList = qEntryP; 
		tpqLock_ReleaseWrite(&tpq_freeListLock);
	    }
	}

	/* 
	 * before going to sleep again, scan the queues to see if there's 
	 * something else to run.
	 */
	while(1)
	{
	    /* these are passed but not used */
	    long nextExpire;
	    int graceExpired;

	    icl_Trace1(tpq_iclSetp, TPQ_ICL_HELPER_CHECKQ, ICL_TYPE_POINTER, pEntryP);
	    tpqLock_ObtainWrite(&poolP->queueLock);
	    for(i = 0; i < TPQ_NPRIORITIES; i++)
	    {
		qEntryP = tpq_FindNext(poolP, i, osi_Time(), &nextExpire, &graceExpired);
		if (qEntryP != (struct tpq_queueEntry *)NULL)
		    break;
	    }
	    if (i != TPQ_NPRIORITIES)
	    {
		TPQ_UNQUEUE(poolP, qEntryP);
		tpqLock_ReleaseWrite(&poolP->queueLock);

		/* run op */
		icl_Trace2(tpq_iclSetp, TPQ_ICL_HELPER_QRUN, ICL_TYPE_POINTER, pEntryP,
			   ICL_TYPE_POINTER, qEntryP); 
		TPQ_RUNOP(qEntryP);
		icl_Trace3(tpq_iclSetp, TPQ_ICL_HELPER_QRUN_DONE, ICL_TYPE_POINTER, pEntryP,
			   ICL_TYPE_POINTER, qEntryP, ICL_TYPE_LONG, qEntryP->rescheduleInterval); 
		if (qEntryP->rescheduleInterval)
		    TPQ_REQUEUE(poolP, qEntryP);
		else
		{
		    /* return it to the free list */
		    tpqLock_ObtainWrite(&tpq_freeListLock);
		    qEntryP->next = tpq_qFreeList;
		    tpq_qFreeList = qEntryP; 
		    tpqLock_ReleaseWrite(&tpq_freeListLock);
		}
	    }
	    else
	    {
		tpqLock_ReleaseWrite(&poolP->queueLock);
		break;
	    }
	} /* while(1) */

	/* time to go to sleep */
	if (!pEntryP->die) {
	    icl_Trace2(tpq_iclSetp, TPQ_ICL_HELPER_SLEEP, ICL_TYPE_POINTER, pEntryP,
		       ICL_TYPE_POINTER, poolP);
	    tpqLock_ObtainWrite(&pEntryP->threadLock);
	    pEntryP->sleepTime = osi_Time();
	    pEntryP->sleeping = 1;
	    tpq_SleepW(pEntryP, &pEntryP->threadLock);
	}
    } /* main loop */
    /*NOTREACHED*/
}
