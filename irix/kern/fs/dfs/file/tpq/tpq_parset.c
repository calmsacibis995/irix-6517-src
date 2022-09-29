/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_parset.c,v 65.5 1998/04/02 19:44:27 bdr Exp $";
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

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_parset.c,v 65.5 1998/04/02 19:44:27 bdr Exp $")

/*
 * define control structure
 */
typedef struct tpq_parset
{
    tpq_lock_data_t	lock;
    void			*threadPool;
    int				size;
    int				waiting;
} tpq_parset_t;

/* define argument structure */
typedef struct tpq_parset_arg
{
    tpq_parset_t	*setp;
    void		(*op)(void *);
    void		*arg;
} tpq_parset_arg_t;

/*
 * Create a parallel set -- that is, create a set where we can wait for the whole
 * set to complete.
 */
/* EXPORT */
void *tpq_CreateParSet(void *threadPool)
{
    tpq_parset_t *setp;

    /* create the control structure */
    setp = (tpq_parset_t *)osi_Alloc(sizeof(tpq_parset_t));
    osi_assert(setp != (tpq_parset_t *)NULL);

    /* initialize the lock and size */
    tpqLock_Init(&setp->lock);
    setp->size = 0;
    setp->waiting = 0;
    setp->threadPool = threadPool;

    return((void *)setp);
}

/*
 * define the function that will actually be queued to run
 */
#ifdef SGIMIPS
/* ARGSUSED1 */
#endif /* SGIMIPS */
static void ParSetWorker(void *arg, void *unused)
{
    tpq_parset_arg_t *ap = (tpq_parset_arg_t *)arg;
    tpq_parset_t *setp = ap->setp;

    /* run the operation */
    ap->op(ap->arg);

    tpqLock_ObtainWrite(&setp->lock);
    osi_assert(setp->size != 0);
    setp->size--;
    if (setp->size == 0)
	tpq_Wakeup(setp);
    tpqLock_ReleaseWrite(&setp->lock);

    /* free this arg structure */
    osi_Free(arg, sizeof(tpq_parset_arg_t));
}

/* EXPORT */
int tpq_AddParSet(void *setHandle, void (*op)(void *),
			  void *arg, int priority,
			  long gracePeriod)
{
    void		*ret;
    tpq_parset_arg_t	*newp;
    tpq_parset_t	*setp = (tpq_parset_t *)setHandle;

    tpqLock_ObtainWrite(&setp->lock);
    if (setp->waiting)
    {
	/* already waiting, can't queue anything else */
	tpqLock_ReleaseWrite(&setp->lock);
	return(0);	/* failure */
    }
    setp->size++;
    tpqLock_ReleaseWrite(&setp->lock);

    /* create a structure to hold this new op/arg set */
    newp = (tpq_parset_arg_t *)osi_Alloc(sizeof(tpq_parset_arg_t));
    osi_assert(newp != (tpq_parset_arg_t *)NULL);

    newp->setp = setp;
    newp->op = op;
    newp->arg = arg;

    /*
     * queue this guy to run with specified priority and granc period,
     * and with no reschedule interval and no rop dead time.
     */
    ret = tpq_QueueRequest(setp->threadPool, ParSetWorker,
			   (void *)newp, priority, gracePeriod, 0, 0);
    if (ret == NULL)
    {
	/* this queuing failed */
	tpqLock_ObtainWrite(&setp->lock);
	setp->size--;
	tpqLock_ReleaseWrite(&setp->lock);
	osi_Free(newp, sizeof(tpq_parset_arg_t));

	return(0);	/* failure */
    }

    return(1);		/* success */
}

/* EXPORT */
void tpq_WaitParSet(void *setHandle)
{
    tpq_parset_t *setp = (tpq_parset_t *)setHandle;

    tpqLock_ObtainWrite(&setp->lock);
    setp->waiting = 1;
    while(setp->size != 0)
    {
	tpq_SleepW(setp, &setp->lock);
	tpqLock_ObtainWrite(&setp->lock);
    }
    tpqLock_ReleaseWrite(&setp->lock);

    osi_Free(setHandle, sizeof(tpq_parset_t));
}
