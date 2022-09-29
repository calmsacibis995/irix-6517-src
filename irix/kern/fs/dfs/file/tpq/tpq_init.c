/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_init.c,v 65.6 1998/04/02 19:44:22 bdr Exp $";
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

#if defined(AFS_HPUX_ENV) || defined(AFS_IRIX_ENV)
#ifdef	KERNEL
#include <dce/ker/pthread.h>	/* for osi_ThreadCreate */
#else	/* KERNEL */
#include <dce/pthread.h>		/* for osi_ThreadCreate */
#endif	/* KERNEL */
#endif	/* AFS_HPUX_ENV || AFS_IRIX_ENV */

#include <tpq.h>
#include <tpq_private.h>

#if !defined(KERNEL)
#include <dce/pthread.h>
#endif /* !KERNEL */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_init.c,v 65.6 1998/04/02 19:44:22 bdr Exp $")

extern tpq_lock_data_t tpq_freeListLock;

/* helper thread procedure */
extern void tpq_HelperThread _TAKES((struct tpq_threadPoolEntry *)); 
/* dispatch thread */
extern void tpq_DispatcherThread _TAKES((struct tpq_threadPool *));

/* define variables to keep and protect the global request queue free list */
/* SHARED */
struct tpq_queueEntry *tpq_qFreeList = (struct tpq_queueEntry *)0;
int tpq_freeListInitted = 0;
tpq_lock_data_t tpq_freeListLock;

/* define global ICL set pointer */
/* SHARED */
struct icl_set *tpq_iclSetp = (struct icl_set *)0;

/*
 * This module contains: 
 *	tpq_Init()	This is an exported function that creates the 
 *			requested number of threads (and the appropriate 
 *			structures), and returns a handle for the pool.
 *
 *	CreatePoolEntry()  This is a private function that creates a
 *			helper thread and the associated administrative
 *			baggage.
 *
 *	tpq_GrowThreadPool()  This is an internal function that is used
 *			to grow the thread pool based on the init parameters
 *			in the event a queued operation needs to run.
 */

static int CreatePoolEntry _TAKES((IN struct tpq_threadPool *threadPoolP,
				    OUT struct tpq_threadPoolEntry **entryPP));

/* macro to 'increment' thread name */
#define NEXTTHREADNAME(pp) \
    do{ \
	char *_p = &(pp)->threadName[3]; \
	if (*_p == '~') *_p = '!';	/* rollover */ \
	else *_p = *_p + 1; \
    } while(0)

/*
 * Initialize thread pool:
 */
/* EXPORT */
#ifdef SGIMIPS
/* ARGSUSED4 */
#endif /* SGIMIPS */
void * tpq_Init(minThreads, medMaxThreads, highMaxThreads, threadEnnui,
		       threadNamePrefix)
  IN int minThreads;		/* minimum thread pool size */
  IN int medMaxThreads;		/* thread pool size for medium priority */
  IN int highMaxThreads;	/* thread pool size for high priority */
  IN long threadEnnui;		/* the amount of time an extraneous thread is allowed to sleep */
  IN char *threadNamePrefix;	/* prefix for thread names (used by AIX-kernel only) */
{
    int code;				/* return value */
    int i;
#ifndef SGIMIPS
    struct tpq_threadPoolEntry *entryP;	/* helper thread entry structure */
#endif /* SGIMIPS */
    struct tpq_threadPool *poolP;	/* pool we are creating */
#ifdef SGIMIPS
    /* REFERENCED */
#endif /* SGIMIPS */
    char dispatchThreadName[4];		/* name of thread */
    struct icl_log *logp;		/* trace log pointer */

    /* set up free list if it's not already set up */
    if (!tpq_freeListInitted)
    {
	tpqLock_Init(&tpq_freeListLock);
	tpq_freeListInitted = 1;
    }

    /* initialize the tracing package */
    code = icl_CreateLog("cmfx", 0, &logp);
    if (code == 0) {
	code = icl_CreateSetWithFlags("tpq", logp, (struct icl_log *) 0, 
				      ICL_CRSET_FLAG_DEFAULT_OFF,
				      &tpq_iclSetp);
    }
    if (code)
	printf("tpq: warning: can't init icl for cmfx, code %d\n", code);

    /* allocate and initialize threadPool structure */
    poolP = (struct tpq_threadPool *)osi_Alloc(sizeof(struct tpq_threadPool));
    tpqLock_Init(&poolP->poolLock);
    tpqLock_Init(&poolP->queueLock);

    /* initialize pool parameters from arguments */
    poolP->minThreads = minThreads;
    poolP->medMaxThreads = medMaxThreads;
    poolP->highMaxThreads = highMaxThreads;
    poolP->nthreads = 0;
    poolP->threads = (struct tpq_threadPoolEntry *)NULL;
    poolP->threadEnnui = threadEnnui;
    poolP->active = 1;
    poolP->running = 0;
#if defined(SGIMIPS) && defined(_KERNEL)
    osi_InitWaitHandle(&poolP->waitHandler);
#endif

    icl_Trace4(tpq_iclSetp, TPQ_ICL_INIT, ICL_TYPE_LONG, minThreads,
	       ICL_TYPE_LONG, medMaxThreads,
	       ICL_TYPE_LONG, highMaxThreads,
	       ICL_TYPE_LONG, threadEnnui);
#if (defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV)) && defined(KERNEL)
    /* set thread name prefix */
    if (threadNamePrefix != NULL)
    {
	register char *p;
	int i;

	bcopy(threadNamePrefix, poolP->threadName, 3);
	/* make sure we have a 3 character prefix */
	for(p = poolP->threadName, i = 0; i < 3; i++, p++)
	{
	    if (*p == '\0')
		*p = '_';
	}
	/* reserve first printable ascii char (' ') for dispatcher */
	poolP->threadName[3] = '!';
    }
    else
	bcopy("tpq!", poolP->threadName, 4);
    bcopy(poolP->threadName, dispatchThreadName, 3);
    dispatchThreadName[3] = ' ';
#endif /* AFS_AIX_ENV/AFS_HPUX_ENV and KERNEL */

    /* initialize queues */
    for(i = 0; i < TPQ_NPRIORITIES; i++)
	poolP->queue[i] = (struct tpq_queueEntry *)NULL;

    /* allocate minThreads threads */
    code = 0;
    for(i = 0; i < minThreads; i++)
    {
	/* create helper threads and give them a name for AIX */

	/* When we don't ask for the pointer, it comes back unlocked. */
	code = CreatePoolEntry(poolP, (struct tpq_threadPoolEntry **)NULL);
	if (code)
	    break;
    }

    if (!code)
    {
	/* allocate a thread for the dispatcher */
	osi_ThreadCreate(tpq_DispatcherThread, poolP, 0, 0, dispatchThreadName, code);
	NEXTTHREADNAME(poolP);
    }

    if (code)
    {
	/* 
	 * either the dispatcher thread failed to start up or 
	 * we never tried to because a helper failed.
	 */
	tpq_ShutdownPool((void *)poolP);	/* shutdown thread pool */
	icl_Trace1(tpq_iclSetp, TPQ_ICL_INIT_RET, ICL_TYPE_POINTER, 0);
	return((void *)NULL);
    }
    icl_Trace1(tpq_iclSetp, TPQ_ICL_INIT_RET, ICL_TYPE_POINTER, poolP);
    return((void *)poolP);
}

/*
 * Function to adjust thread pool parameters:
 */
/* EXPORT */
void tpq_Adjust(poolHandle, flags, minThreads, medMaxThreads, highMaxThreads, 
			 threadEnnui)
  IN void *poolHandle;		/* thread pool */
  IN long flags;		/* which things you want to change */
  IN int minThreads;		/* minimum thread pool size */
  IN int medMaxThreads;		/* thread pool size for medium priority */
  IN int highMaxThreads;	/* thread pool size for high priority */
  IN long threadEnnui;		/* the amount of time an extraneous thread is allowed to sleep */
{
    struct tpq_threadPool *poolP = (struct tpq_threadPool *)poolHandle;

    icl_Trace2(tpq_iclSetp, TPQ_ICL_ADJUST, ICL_TYPE_POINTER, poolHandle,
	       ICL_TYPE_LONG, flags);
    icl_Trace4(tpq_iclSetp, TPQ_ICL_ADJUST2, ICL_TYPE_LONG, minThreads,
	       ICL_TYPE_LONG, medMaxThreads,
	       ICL_TYPE_LONG, highMaxThreads,
	       ICL_TYPE_LONG, threadEnnui);
    tpqLock_ObtainWrite(&poolP->poolLock);

    if (flags & TPQ_ADJUST_FLAG_MINTHREADS)
	poolP->minThreads = minThreads;

    if (flags & TPQ_ADJUST_FLAG_MEDMAXTHREADS)
	poolP->medMaxThreads = medMaxThreads;

    if (flags & TPQ_ADJUST_FLAG_HIGHMAXTHREADS)
	poolP->highMaxThreads = highMaxThreads;

    if (flags & TPQ_ADJUST_FLAG_THREADENNUI)
	poolP->threadEnnui = threadEnnui;

    tpqLock_ReleaseWrite(&poolP->poolLock);
}

/*
 * Create a new thread pool thread with the specified name.  This new entry
 * is threaded onto the pool's thread queue and the number of threads is
 * incremented.  A pointer to the new structure is passed in the out parameter.
 * The tpe->threadLock is left locked if we pass the pointer back.
 */
static int CreatePoolEntry(threadPoolP, entryPP)
  IN struct tpq_threadPool *threadPoolP;
  OUT struct tpq_threadPoolEntry **entryPP;
{
    int code;
    struct tpq_threadPoolEntry *entryP;	/* helper thread entry structure */

    icl_Trace1(tpq_iclSetp, TPQ_ICL_CREATEPOOL, ICL_TYPE_POINTER, threadPoolP);
    
    /* allocate and initialize structure */
    entryP = (struct tpq_threadPoolEntry *)osi_Alloc(sizeof(struct tpq_threadPoolEntry));
    tpqLock_Init(&entryP->threadLock);
    tpqLock_ObtainWrite(&entryP->threadLock);
    entryP->sleeping = 1;
    entryP->die = 0;
    entryP->privateQueue = (struct tpq_queueEntry *)NULL;
    entryP->sleepTime = osi_Time();
    entryP->threadPool = threadPoolP;

    osi_ThreadCreate(tpq_HelperThread, entryP, 0, 0, threadPoolP->threadName, code);
    if (code)
    {
	tpqLock_ReleaseWrite(&entryP->threadLock);
	osi_Free(entryP, sizeof(struct tpq_threadPoolEntry));
	return(code);
    }
    NEXTTHREADNAME(threadPoolP);
    icl_Trace2(tpq_iclSetp, TPQ_ICL_CREATEPOOL_CREATED, ICL_TYPE_POINTER, threadPoolP,
	       ICL_TYPE_POINTER, entryP);
    
    /* thread onto threadPool structure for this pool */
    tpqLock_ObtainWrite(&threadPoolP->poolLock);
    entryP->next = threadPoolP->threads;
    threadPoolP->threads = entryP;
    threadPoolP->nthreads++;
    tpqLock_ReleaseWrite(&threadPoolP->poolLock);

    /* return entryP in out parameter if desired */
    if (entryPP != (struct tpq_threadPoolEntry **) NULL) {
	*entryPP = entryP;
    } else {
	/* else if caller doesn't want pointer, unlock thread. */
	tpqLock_ReleaseWrite(&entryP->threadLock);
    }
    return(0);
}

/*
 * Grow the thread pool based on the priority and on the parameters
 * governing the thread pool growth that are now kept in the threadPool
 * structure.
 *
 * If a new thread can be created, it is and the structure for the entry is
 * returned in the out parameter.  If the NewProc fails, its error code is returned.  If
 * a new thread can't be created because it would violate the thread parameters,
 * a -1 is returned.  Otherwise, a 0 is returned.
 * If a thread is successfully returned via entryPP, its thread lock is
 * left write-locked.
 */
/* SHARED */
int tpq_GrowThreadPool(poolP, priority, entryPP)
  IN struct tpq_threadPool *poolP;
  IN int priority;
  OUT struct tpq_threadPoolEntry **entryPP;
{
    int code;
    int okToCreate = 0;
#ifndef SGIMIPS 
    char name[4];
#endif /* SGIMIPS */

    icl_Trace2(tpq_iclSetp, TPQ_ICL_GROWPOOL, ICL_TYPE_POINTER, poolP,
	       ICL_TYPE_LONG, priority);
    /*
     * Decide whether or not to create a new thread based on the number
     * of existing threads and the numbers specified in the MaxThread parameters.
     */
    tpqLock_ObtainRead(&poolP->poolLock);
    if (((priority == TPQ_HIGHPRI) && (poolP->nthreads < poolP->highMaxThreads)) ||
	((priority == TPQ_MEDPRI) && (poolP->nthreads < poolP->medMaxThreads)) ||
	((priority == TPQ_LOWPRI) && (poolP->nthreads < poolP->minThreads)))
	okToCreate++;
    tpqLock_ReleaseRead(&poolP->poolLock);
    
    if (okToCreate)
    {
	icl_Trace1(tpq_iclSetp, TPQ_ICL_GROWPOOL_CREATE, ICL_TYPE_POINTER, poolP);
	code = CreatePoolEntry(poolP, entryPP);
	if (code)
	    return(code);
	return(0);	/* success */
    }
    return(-1);		/* did not create a new proc */
}
