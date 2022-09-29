/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_pardo.c,v 65.5 1998/04/02 19:44:24 bdr Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * Copyright (C) 1994, 1992 Transarc Corporation
 * All rights reserved.
 */
/*
 *  tpq_pardo.c -- implementation of parallel-do facility based
 * on the tpq package
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>

#include <tpq.h>
#include <tpq_private.h>

#if !defined(KERNEL)
#include <dce/pthread.h>
#endif /* !KERNEL */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_pardo.c,v 65.5 1998/04/02 19:44:24 bdr Exp $")

#define TPQ_PARDO_THREAD_ENNUI		(10 /* secs */)

#if defined(KERNEL)
#define TPQ_PARDO_THREAD_CEILING	(5)
#else /* defined(KERNEL) */
#define TPQ_PARDO_THREAD_CEILING	(20)
#endif /* defined(KERNEL) */

struct tpq_pardo_args;	/* forward declaration */
typedef struct tpq_pardo_signal {
  tpq_lock_data_t		lock;
  unsigned int			numbOpsActive;
  struct tpq_pardo_args *	clientOpsAndArgsP;
} tpq_pardo_signal_t;

typedef struct tpq_pardo_args {
  void			(*clientOp)(void *, void *);
  void *		clientArgP;
  tpq_pardo_signal_t *	ourSignalP;
} tpq_pardo_args_t;

static void tpq_Pardo_DoOp(
     void *		argsP,
     void *		tpqOpHandle)
{
  tpq_pardo_args_t *		myArgsP = (tpq_pardo_args_t *)argsP;
  tpq_pardo_signal_t *		ourSignalP = myArgsP->ourSignalP;

  myArgsP->clientOp (myArgsP->clientArgP, tpqOpHandle);

  /* make sure the caller doesn't try to get rescheduled by tpq */
  tpq_SetRescheduleInterval(tpqOpHandle, 0);

  /* now, signal the controller that we are done */
  tpqLock_ObtainWrite(&(ourSignalP->lock));
  ourSignalP->numbOpsActive--;
  tpq_Wakeup(&(ourSignalP->numbOpsActive));
  tpqLock_ReleaseWrite(&(ourSignalP->lock));
}

/* EXPORT */
long tpq_Pardo (
     void *			threadPoolHandleP,
     tpq_pardo_clientop_t *	opArrayP,
     unsigned int		numbOps,
     int			priority,
     long			gracePeriod)
{
  long				rtnVal = 0;
  void *			localPoolHandleP = threadPoolHandleP;
  void **			requestHandlesP;
  int				numbOpsQueued = 0;
  int				i;
  int				numberThreads;
  tpq_pardo_args_t *		ourClientOpsAndArgsP;
  tpq_pardo_signal_t *		ourSignalP;
  static char			routineName[] = "tpq_Pardo";

  if (numbOps > 1) {
    /* if there really is more than one to do */
    if (localPoolHandleP == (void *)NULL) {
      numberThreads = ((numbOps > TPQ_PARDO_THREAD_CEILING) ?
		       TPQ_PARDO_THREAD_CEILING : numbOps);

      localPoolHandleP = tpq_Init(numberThreads	/* minThreads */,
				  numberThreads	/* medMaxThreads */,
				  numberThreads	/* highMaxThreads */,
				  TPQ_PARDO_THREAD_ENNUI /* threadEnnui */,
				  "tpd" /* threadNamePrefix */);
    }

    if (localPoolHandleP != (void *)NULL) {
       /*
        * Make sure the client ops & args aren't on the local thread
        * stack.  It would probably work in user-space, but fail
        * miserably in the kernel. These will not be freed while the
        * client threads are trying to access them because this thread
        * will wait for the client threads to signal that they are done
        * before doing anything like freeing memory. 
        */
	
      requestHandlesP = (void **)osi_Alloc(numbOps * sizeof(void *));
      osi_assert(requestHandlesP != (void **)NULL);

      ourSignalP = (tpq_pardo_signal_t *)
          osi_Alloc(sizeof(tpq_pardo_signal_t));
      osi_assert(ourSignalP != (tpq_pardo_signal_t *)NULL);

      ourClientOpsAndArgsP = (tpq_pardo_args_t *)
          osi_Alloc(numbOps * sizeof(tpq_pardo_args_t));
      osi_assert(ourClientOpsAndArgsP != (tpq_pardo_args_t *)NULL);

      tpqLock_Init(&(ourSignalP->lock));
      tpqLock_ObtainWrite(&(ourSignalP->lock));	/* grab the lock */
      ourSignalP->numbOpsActive = 0;
      ourSignalP->clientOpsAndArgsP = ourClientOpsAndArgsP;

      for (i = 0; (i < numbOps) && (rtnVal == 0); i++) {
	ourClientOpsAndArgsP[i].clientOp = opArrayP[i].op;
	ourClientOpsAndArgsP[i].clientArgP = opArrayP[i].argP;
	ourClientOpsAndArgsP[i].ourSignalP = ourSignalP;

	/* queue them all up to the thread pool */

	/*
	 * Tell the thread pool to do the operation right away (no grace period)
	 * if the pool was created solely to handle these operations.
	 */
	requestHandlesP[i] = tpq_QueueRequest(localPoolHandleP,
					      tpq_Pardo_DoOp,
					      &(ourClientOpsAndArgsP[i]),
					      priority,
					      gracePeriod,
					      0	/* don't reschedule these ops */,
					      0	/* drop dead time */);
	if (requestHandlesP[i] != (void *)NULL) {
	  ourSignalP->numbOpsActive++;
	  numbOpsQueued++;
	}
	else {
	  osi_printf("%s: error queueing request to thread pool", routineName);
	  rtnVal = -1;	/* flag an error and break out of the queueing loop */
	}
      }
      tpqLock_ReleaseWrite(&(ourSignalP->lock));	/* drop the lock */

      if (rtnVal == 0) {
	/* now, wait for all the tasks to complete */
	tpqLock_ObtainWrite(&(ourSignalP->lock));
	while (ourSignalP->numbOpsActive) {
	  tpq_SleepW(&(ourSignalP->numbOpsActive), &(ourSignalP->lock));
	  tpqLock_ObtainWrite(&(ourSignalP->lock));
	}

	tpqLock_ReleaseWrite(&(ourSignalP->lock));
	/* at this point, all the client ops are done */
      }
      else {
	/*
	 *   We had some problems queueing the ops; dequeue the ones for which we
	 * were successful.
	 */
	for (i = 0; i < numbOpsQueued; i++) {
	  tpq_DequeueRequest(localPoolHandleP, requestHandlesP[i]);
	}
      }

      /* shut down the pool if it was setup only for this job */
      if (threadPoolHandleP == (void *)NULL) {
	tpq_ShutdownPool(localPoolHandleP);
      }

      osi_Free(requestHandlesP, numbOps * sizeof(void *));
      osi_Free(ourClientOpsAndArgsP, numbOps * sizeof(tpq_pardo_args_t));
      osi_Free(ourSignalP, sizeof(tpq_pardo_signal_t));
    }
    else {
      /* thread pool init error */
      osi_printf("%s: error initializing thread pool", routineName);
      rtnVal = -1;		/* we can only indicate that something went wrong */
    }
  }
  else {
    /* there's only one; go ahead and do it in this thread */
    opArrayP[0].op (opArrayP[0].argP, (void *)NULL);
  }

  return rtnVal;
}

/* EXPORT */
long tpq_ForAll(
     void *		threadPoolHandleP,
     void		(*opP)(void *, void *),
     void *		argP,
     unsigned int	numbInstances,
     int		priority,
     long		gracePeriod)
{
  long				rtnVal = 0;
#ifndef SGIMIPS
  tpq_forall_arg_t		argToPass;
#endif /* SGIMIPS */
  tpq_pardo_clientop_t *	clientOpArrayP;
  int				argsAllocd = 0;
  int				i;
  static char			routineName[] = "tpq_ForAll";

  clientOpArrayP = (tpq_pardo_clientop_t *)
      osi_Alloc(numbInstances * sizeof(tpq_pardo_clientop_t));
  osi_assert(clientOpArrayP != (tpq_pardo_clientop_t *)NULL);

  for (i = 0; (i < numbInstances) && (rtnVal == 0); i++) {
    clientOpArrayP[i].argP = (void *)osi_Alloc(sizeof(tpq_forall_arg_t));
    if (clientOpArrayP[i].argP != (void *)NULL) {
      clientOpArrayP[i].op = opP;
      ((tpq_forall_arg_t *)(clientOpArrayP[i].argP))->opIndex = i;
      ((tpq_forall_arg_t *)(clientOpArrayP[i].argP))->clientArgP = argP;
      argsAllocd++;
    }
    else {
      osi_printf("%s: error allocating forall arg structure\n", routineName);
      rtnVal = -1;
    }
  }

  if (rtnVal == 0) {
    rtnVal = tpq_Pardo(threadPoolHandleP, clientOpArrayP, numbInstances,
		       priority, gracePeriod);
  }

  /* no matter how we got here, we are done with the arg structures */
  for (i = 0; i < argsAllocd; i++) {
    /* clean up after ourselves */
    osi_Free(clientOpArrayP[i].argP, sizeof(tpq_forall_arg_t));
  }

  osi_Free(clientOpArrayP, numbInstances * sizeof(tpq_pardo_clientop_t));

  return rtnVal;
}
