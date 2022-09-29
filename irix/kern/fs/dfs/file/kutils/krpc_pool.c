/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: krpc_pool.c,v 65.5 1998/04/01 14:16:02 gwehrman Exp $";
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
 * HISTORY
 * $Log: krpc_pool.c,v $
 * Revision 65.5  1998/04/01 14:16:02  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Provided forward prototype for krpc_Free_PoolElt().  Changed
 * 	krpc_GetPoolElt() declaration to a valid prototype.
 *
 * Revision 65.4  1998/03/23 16:36:11  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/12/16 17:05:36  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.2  1997/11/06  20:00:30  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.43.1  1996/10/02  17:53:16  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:32  damon]
 *
 * Revision 1.1.38.2  1994/06/09  14:12:13  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:40  annie]
 * 
 * Revision 1.1.38.1  1994/02/04  20:21:38  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:14:29  devsrc]
 * 
 * Revision 1.1.36.2  1994/01/20  18:43:22  annie
 * 	added copyright header
 * 	[1994/01/20  18:39:38  annie]
 * 
 * Revision 1.1.36.1  1993/12/07  17:27:20  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:42:15  jaffe]
 * 
 * $EndLog$
 */

/*
 * Copyright (C) 1994, 1992 Transarc Corporation.
 * All rights reserved.
 */

#include <dcedfs/param.h>		        /* Should be always first */
#include <dcedfs/stds.h>
#include <dcedfs/lock.h>
#include <dcedfs/krpc_pool.h>

RCSID("$Header:")

struct lock_data krpc_poolLock;
struct krpc_poolelt *krpc_freePoolListp = 0;

#ifdef SGIMIPS
krpc_FreePoolElt(struct krpc_poolelt *ap);
#endif

/* get a block */
#ifdef SGIMIPS
struct krpc_poolelt *krpc_GetPoolElt(void)
#else
struct krpc_poolelt *krpc_GetPoolElt()
#endif
{
    register struct krpc_poolelt *sep, *newFree;
    int i;

    if (!krpc_freePoolListp) {
	/* dump a bunch of new entries into the free list */
	sep = (struct krpc_poolelt *)
	    osi_Alloc(KRPC_POOL_NALLOC * sizeof(struct krpc_poolelt));
	for(i=0;i<KRPC_POOL_NALLOC;i++) {
	    krpc_FreePoolElt(sep++);
	}
    }
    sep = krpc_freePoolListp;
    krpc_freePoolListp = (struct krpc_poolelt *) sep->q.next;
    return sep;
}

/* free a block */
krpc_FreePoolElt(struct krpc_poolelt *ap)
{
    ap->q.next = (struct squeue *) krpc_freePoolListp;
    krpc_freePoolListp = ap;
    return 0;
}


/* initialize a pool structure */
krpc_PoolInit(poolp, count)
  register struct krpc_pool *poolp;
  long count;
{
    lock_ObtainWrite(&krpc_poolLock);
    poolp->origMax = poolp->max = count;
    poolp->allocated = 0;
    QInit(&poolp->q);
    lock_ReleaseWrite(&krpc_poolLock);
    return 0;
}

/* reserve count items from a pool. */
krpc_PoolAlloc(poolp, flags, count)
  register struct krpc_pool *poolp;
  long flags;
  long count;
{
    struct krpc_poolelt *sep;
    long extras;

    lock_ObtainWrite(&krpc_poolLock);
    if (flags & KRPC_POOL_NOFAIL) {
	/* never fail, just do the arithmetic, but also tell krpc
	 * if we're starting more calls, if necessary.
	 */
	poolp->allocated += count;
	if ((extras=poolp->allocated - poolp->max) > 0) {
	    /* need to ask KRPC for more packet buffers to avoid
	     * deadlock with this many calls outstanding.
	     */
	    lock_ReleaseWrite(&krpc_poolLock);
	    krpc_AddConcurrentCalls(extras, 0);
	    lock_ObtainWrite(&krpc_poolLock);
	    poolp->max += extras;
	}
	lock_ReleaseWrite(&krpc_poolLock);
	return 0;
    }
    /* if we get here, we will wait or fail instead of allocating
     * more packet buffers.
     */
    while (1) {
	if (poolp->allocated + count <= poolp->max) {
	    /* we're done */
	    poolp->allocated += count;
	    lock_ReleaseWrite(&krpc_poolLock);
	    return 0;
	}

	/* if we're not supposed to wait, return failure */
	if (flags & KRPC_POOL_NOWAIT) {
	    lock_ReleaseWrite(&krpc_poolLock);
	    return -1;
	}

	/* otherwise, we have to sleep */
	sep = krpc_GetPoolElt();
	sep->count = count;
	QAddT(&poolp->q, &sep->q);
	osi_SleepW((char *) sep, &krpc_poolLock);
	lock_ObtainWrite(&krpc_poolLock);
	QRemove(&sep->q);
	krpc_FreePoolElt(sep);
    }
    /* not reached */
}

krpc_PoolFree(poolp, count)
  register struct krpc_pool *poolp;
  long count;
{
    register struct krpc_poolelt *sep;

    lock_ObtainWrite(&krpc_poolLock);
    poolp->allocated -= count;
    osi_assert (poolp->allocated >= 0);
    sep = (struct krpc_poolelt *) QNext(&poolp->q);
    if (sep != (struct krpc_poolelt *) &poolp->q) {
	/* if queue is not empty */
	if (sep->count + poolp->allocated <= poolp->max) {
	    /* should be able to wakeup this dude; don't look
	     * further: the whole point of this package is that
	     * we're trying to be fair.
	     */
	    osi_Wakeup((char *) sep);
	}
    }
    lock_ReleaseWrite(&krpc_poolLock);
    return 0;
}

/* Called by a package when it needs to declare that it will be
 * using up to scalls server calls and up to ccalls client calls.
 * Note that a server thread typically requires one of each, for
 * those servers that actually make outgoing calls, too.
 */
long krpc_maxServerCalls = 0;
long krpc_maxClientCalls = 0;
int (*krpc_concurrentProc)() = 0;
krpc_AddConcurrentCalls(ccalls, scalls)
  int ccalls;
  int scalls;
{
    long status;

    krpc_maxServerCalls += scalls;
    krpc_maxClientCalls += ccalls;
    if (krpc_concurrentProc)
	(*krpc_concurrentProc)(krpc_maxClientCalls, krpc_maxServerCalls,
				 &status);
    return 0;
}

/* called by the file exporter when it starts, since glue doesn't have
 * pointer to RPC code in some AIX configurations.  The function passed
 * in is rpc_mgmt_set_max_concurrency.
 */
krpc_SetConcurrentProc(aprocp)
  int (*aprocp)();
{
    long status;

    krpc_concurrentProc = aprocp;
    /* now set it, since we have the procedure */
    (*krpc_concurrentProc)(krpc_maxClientCalls, krpc_maxServerCalls, &status);
    return 0;
}
