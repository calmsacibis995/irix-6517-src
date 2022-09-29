/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: krpc_pool.h,v $
 * Revision 65.1  1997/10/20 19:20:12  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.44.1  1996/10/02  17:53:19  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:33  damon]
 *
 * Revision 1.1.39.2  1994/06/09  14:12:14  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:42  annie]
 * 
 * Revision 1.1.39.1  1994/02/04  20:21:44  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:14:29  devsrc]
 * 
 * Revision 1.1.37.2  1994/01/20  18:43:23  annie
 * 	added copyright header
 * 	[1994/01/20  18:39:39  annie]
 * 
 * Revision 1.1.37.1  1993/12/07  17:27:22  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:42:29  jaffe]
 * 
 * $EndLog$
 */
/*
 * Copyright (C) 1994, 1992 Transarc Corporation.
 * All rights reserved.
 */

#ifndef __KRPC_POOL_H_
#define __KRPC_POOL_H_ 1

#include <dcedfs/queue.h>
#include <dcedfs/stds.h>
#include <dcedfs/lock.h>

/* This package implements a resource queue for the cache manager.
 * Processes allocate elements and free them, and there's a max
 * # of allocated entries.  Processes that try to exceed the pool's
 * entry limit block and get queued for fairness reasons.
 */

/* KRPC pool flags */
#define KRPC_POOL_NOWAIT	1	/* don't wait, but can fail */
#define KRPC_POOL_NOFAIL	2	/* don't wait, and don't fail, even
					 * if we have to ask the RPC to allow
					 * more calls.
					 */

/* structure represents a pool of resources */
struct krpc_pool {
    struct squeue q;		/* queue of guys waiting */
    long allocated;		/* number already allocated */
    long max;			/* max number allowed to be outstanding */
    long origMax;		/* original max # desired */
};

/* represents an individual waiting for some resource to become
 * available.
 */
struct krpc_poolelt {
    struct squeue q;		/* queue of resource waiters */
    long count;			/* number we're waiting for */
};

#define KRPC_POOL_NALLOC	32	/* # of poolelts to allocate at once */

extern int krpc_PoolInit(struct krpc_pool *poolp, long count);
extern int krpc_PoolAlloc(struct krpc_pool *poolp, long flags,
				  long count);
extern int krpc_PoolFree (struct krpc_pool *poolp, long count);
extern int krpc_AddConcurrentCalls (int clientCalls, int serverCalls);
extern int krpc_SetConcurrentProc(int (*proc)());

#endif /* __KRPC_POOL_H_ 1 */
