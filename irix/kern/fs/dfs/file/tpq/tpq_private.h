
/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: tpq_private.h,v $
 * Revision 65.2  1997/12/16 17:05:45  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:18:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.338.1  1996/10/02  18:49:21  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:49:28  damon]
 *
 * Revision 1.1.328.3  1994/07/13  22:30:12  devsrc
 * 	merged with bl-10
 * 	[1994/06/29  11:56:05  devsrc]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:03:33  mbs]
 * 
 * Revision 1.1.328.2  1994/06/09  14:23:16  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:34:58  annie]
 * 
 * Revision 1.1.328.1  1994/02/04  20:33:32  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:19:24  devsrc]
 * 
 * Revision 1.1.326.1  1993/12/07  17:36:18  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  17:39:17  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  15:56:31  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  17:40:37  cjd]
 * 
 * Revision 1.1.2.3  1992/11/24  19:48:47  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:33:29  bolinger]
 * 
 * Revision 1.1.2.2  1992/08/31  21:40:05  jaffe
 * 	Transarc delta: comer-ot3947-multithread-cm-daemon 1.25
 * 	[1992/08/30  13:21:14  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1992 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_TPQ_PRIVATE_H
#define TRANSARC_TPQ_PRIVATE_H

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>

#include <dcedfs/icl.h>
#include <tpq_trace.h>

extern struct icl_set *tpq_iclSetp;

/*
 * define kernel-level and user-level equivalents for locks
 */

#include <dcedfs/lock.h>

typedef osi_dlock_t tpq_lock_data_t;

#define tpqLock_Init		lock_Init
#define tpqLock_ObtainRead	lock_ObtainRead
#define tpqLock_ReleaseRead	lock_ReleaseRead
#define tpqLock_ObtainWrite	lock_ObtainWrite
#define tpqLock_ReleaseWrite	lock_ReleaseWrite

#define tpq_Wakeup(addr)	osi_Wakeup((opaque)(addr))
#define tpq_Sleep(addr, lockP)	osi_Sleep((opaque)(addr), (lockP))
#define tpq_SleepR(addr, lockP)	osi_SleepR((opaque)(addr), (lockP))
#define tpq_SleepS(addr, lockP)	osi_SleepS((opaque)(addr), (lockP))
#define tpq_SleepW(addr, lockP)	osi_SleepW((opaque)(addr), (lockP))


/* don't care that the interruptible sleeps aren't really interruptible */
#define tpq_SleepRI(addr, lockP)	osi_SleepR((opaque)(addr), (lockP))
#define tpq_SleepSI(addr, lockP)	osi_SleepS((opaque)(addr), (lockP))
#define tpq_SleepWI(addr, lockP)	osi_SleepW((opaque)(addr), (lockP))

#if defined(KERNEL) && ( defined(AFS_AIX_ENV) || defined (AFS_IRIX_ENV) )
/*
 * In the AIX kernel, we will have to hold the kernel lock.  After running
 * an op, we give it up and reobtain it to make sure we don't hog the processor.
 */

#define TPQ_RUNOP(qEntryP) \
    MACRO_BEGIN \
	(*(qEntryP)->op)((qEntryP)->arg, (void *)(qEntryP)); \
        osi_RestorePreemption(preempting); \
	preempting = osi_PreemptionOff(); \
    MACRO_END
#else
#define TPQ_RUNOP(qEntryP) (*(qEntryP)->op)((qEntryP)->arg, (void *)(qEntryP))
#endif /* KERNEL && AFS_AIX_ENV */

/* define structure of entries describing functions to be run: */
struct tpq_queueEntry
{
    int destroy;			/* destroy this when read */
    int mark;				/* used by status reporter */
    void (*op)(void *, void *);		/* function to be executed */
    void *arg;				/* argument to function */
    int priority;			/* priority under which to queue it */
    long gracePeriod;			/* period we're willing to wait for execution */
    long rescheduleInterval;		/* where to reschedule this entry */
    long dropDeadTime;			/* don't run function if this time passed */
    long executionTime;			/* when to run this function */
    struct tpq_queueEntry *next;	/* next in list */
    struct tpq_queueEntry *prev;	/* prev in list */
};

/* define structure to keep track of threads in pool */
struct tpq_threadPoolEntry
{
    tpq_lock_data_t threadLock;		/* lock to protect this structure */
    int sleeping;			/* is this thread sleeping? */
    int	die;				/* signal to thread to kill itself */
    struct tpq_queueEntry *privateQueue; /* private queue for this thread */
    long sleepTime;			/* time when helper went to sleep */
    struct tpq_threadPool *threadPool;	/* pointer to owning thread pool */
    struct tpq_threadPoolEntry *next;	/* next in list */
};

/* define controlling structure for thread pool itself */
struct tpq_threadPool
{
    int active;				/* is this pool active? */
    int running;			/* is the dispatcher running? */
    tpq_lock_data_t poolLock;		/* lock for this thread pool */
    tpq_lock_data_t queueLock;		/* lock for queues */
    int minThreads;			/* minimum thread pool size */
    int medMaxThreads;			/* max threads for medium priority */
    int highMaxThreads;			/* max threads for high priority */
    int nthreads;			/* current number of threads */
    struct tpq_threadPoolEntry *threads; /* threads in this pool */
    long threadEnnui;			/* the amount of time an extraneous thread can sleep */
    struct tpq_queueEntry *queue[TPQ_NPRIORITIES]; /* priority queues */
    struct osi_WaitHandle waitHandler;	/* Interrupt waiting on dispatch */
    char threadName[4];			/* name to assign thread for debugging */
};

#ifdef SGIMIPS
extern void EnqueueEntry _TAKES((struct tpq_threadPool *,
				 struct tpq_queueEntry *));
#endif

/* requeue an operation */
#define TPQ_REQUEUE(poolP, qEntryP)	\
    do{	\
        qEntryP->executionTime = osi_Time() + qEntryP->rescheduleInterval; \
        EnqueueEntry(poolP, qEntryP);	\
    } while(0)

#define TPQ_UNQUEUE(poolP, qEntryP) \
    do { \
        if (qEntryP->next == qEntryP) /* only one in list */ \
	    poolP->queue[qEntryP->priority] = (struct tpq_queueEntry *)NULL;  \
        else  \
        {  \
	    qEntryP->prev->next = qEntryP->next;  \
	    qEntryP->next->prev = qEntryP->prev;  \
	    if (poolP->queue[qEntryP->priority] == qEntryP) /* first in list? */  \
	        poolP->queue[qEntryP->priority] = qEntryP->next;  \
        }  \
    } while(0)

/*
 * Locking hierarchy:
 *	There are four locks used in this package.  They must be taken in
 *	the specified order:
 *
 *		poolP->queueLock
 *			This lock protects the request queues hanging off
 *			the request pool (poolP->queues).  It also protects
 *			the active flags in the pool structure, pool->active;
 *			and the remove flag of the queued requests,
 *			entryP->destroy.  It also protects the status mark,
 *			entryP->mark.
 *
 *		poolP->poolLock
 *			This lock protects the other members of the pool
 *			structure.  Specifically, these are the nthreads,
 *			threads members, and running.
 *
 *		threadP->threadLock
 *			This lock protects the elements of the threadPoolEntry
 *			structures.  Specifically, these members are the
 *			flag to signal that the thread should terminate itself,
 *			threadP->die; the flag that indicates the thread is
 *			sleeping, threadP->sleeping; and the thread's private
 *			queue, threadP->privateQueue.
 *
 *		tpq_freeListLock
 *			This is a global lock used to mediate access (from
 *			multiple thread pools to the request free list.
 */

#endif /* TRANSARC_TPQ_PRIVATE_H */
