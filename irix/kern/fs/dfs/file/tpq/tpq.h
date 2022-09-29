/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: tpq.h,v $
 * Revision 65.1  1997/10/20 19:18:10  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.326.1  1996/10/02  18:48:48  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:49:20  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1994, 1992 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_TPQ_H
#define TRANSARC_TPQ_H

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

/* define priorities: */
#define TPQ_HIGHPRI	0
#define TPQ_MEDPRI	1
#define TPQ_LOWPRI	2
#define TPQ_NPRIORITIES 3

/*
 * Prototypes for the exported functions:
 */

/* initialize thread pool and return handle */
extern void *tpq_Init(
    int minThreads,
    int medMaxThreads,
    int highMaxThreads,
    long threadEnnui,
    char *threadNamePrefix
);

#define TPQ_ADJUST_FLAG_MINTHREADS	0x1
#define TPQ_ADJUST_FLAG_MEDMAXTHREADS	0x2
#define TPQ_ADJUST_FLAG_HIGHMAXTHREADS	0x4
#define TPQ_ADJUST_FLAG_THREADENNUI	0x8
#define TPQ_ADJUST_FLAG_ALL		(TPQ_ADJUST_FLAG_MINTHREADS |   \
					 TPQ_ADJUST_FLAG_MEDMAXTHREADS | \
					 TPQ_ADJUST_FLAG_HIGHMAXTHREADS | \
					 TPQ_ADJUST_FLAG_THREADENNUI)

extern void tpq_Adjust(
    void *queueHandle,
    long flags,
    int minThreads,
    int medMaxThreads,
    int highMaxThreads,
    long threadEnnui
);

/* queue a request to the thread pool and return a handle to the request */
extern void *tpq_QueueRequest(
    void *queueHandle,
    void (*op)(void *, void *),
    void *arg,
    int priority,
    long gracePeriod,
    long rescheduleInterval,
    long dropDeadTime
);

/* dequeue a previously queued request */
extern void tpq_DequeueRequest(void *queueHandle, void *requestHandle);

/* shut down thread pool */
extern void tpq_ShutdownPool(void *queueHandle);

/*
 * define structure to hold status information for return to
 * client process.
 */
struct tpq_stats
{
    int nthreads;		/* current number of threads */
    int totalQueued[TPQ_NPRIORITIES];
    	/* total number of queued ops per queue */
    int sluggishQueued[TPQ_NPRIORITIES];
    	/* total number that haven't run since last call */
};

/* get thread pool stats */
extern void tpq_Stat(void *queueHandle, struct tpq_stats *statsP);

/*
 * The access functions for the members of
 * the queue structure passed to functions executed through the 
 * thread pool.  For example, if a scheduled operation wants to
 * changes its reschedule interval, it can query and reset that
 * interval using tpq_[GS]etRescheduleInterval().
 */

extern void tpq_SetArgument(
    /* IN */ void *opHandle,
    /* IN */ void *arg
);
extern int tpq_GetPriority(/* IN */ void *opHandle);
extern void tpq_SetPriority(
    /* IN */ void *opHandle,
    /* IN */ int priority
);
extern long tpq_GetGracePeriod(/* IN */ void *opHandle);
extern void tpq_SetGracePeriod(
    /* IN */ void *opHandle,
    /* IN */ long gracePeriod
);
extern long tpq_GetRescheduleInterval(/* IN */ void *opHandle);
extern void tpq_SetRescheduleInterval(
    /* IN */ void *opHandle,
    /* IN */ long rescheduleInterval
);
extern long tpq_GetDropDeadTime(/* IN */ void *opHandle);
extern void tpq_SetDropDeadTime(
    /* IN */ void *opHandle,
    /* IN */ long dropDeadTime
);

/*
 *  The interfaces for the pardo package based on tpq
 */

/* this has to be exported because the client must pass a list to the pardo routine */
typedef struct tpq_pardo_clientop {
  void				(*op)(void *, void *);
  void *			argP;
} tpq_pardo_clientop_t;

/* this is exported because it is what the client op will be passed in a forall */
typedef struct tpq_forall_arg {
  unsigned int		opIndex;
  void *		clientArgP;
} tpq_forall_arg_t;

extern long tpq_Pardo(
    void *			threadPoolHandleP,
    tpq_pardo_clientop_t *	opArrayP,
    unsigned int		numbOps,
    int				priority,
    long			gracePeriod
);
extern long tpq_ForAll(
    void *			threadPoolHandleP,
    void			(*opP)(void *, void *),
    void *			argP,
    unsigned int		numbInstances,
    int				priority,
    long			gracePeriod
);

/* Exported interface for the parallel set functions: */
extern void *tpq_CreateParSet(void *threadPool);
extern int tpq_AddParSet(
    void *setHandle,
    void (*op)(void *),
    void *arg,
    int priority,
    long gracePeriod
);
extern void tpq_WaitParSet(void *setHandle);

#endif /* TRANSARC_TPQ_H */
