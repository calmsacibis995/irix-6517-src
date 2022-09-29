
#ifdef NUMA_REPLICATION

/*****************************************************************************
 * replinit.c
 *
 *	Initialize replication related data structures.
 *	Also has routines that do state transitions of the 
 *	replication attribute.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

#ident "$Revision: 1.2 $"

/*
 * replinit.c
 * This file has the routines used to create/destroy replication attribute
 * structures. 
 *
 * Replication attribute structures are attached to vnode objects. 
 *
 * Replication attributes are created when we need them. Typically this 
 * happens when a process maps vnode for execution. 
 * These * structures are removed (lazily) when system decides to 
 * recirculate the vnode for other purposes.
 */


#include <sys/types.h>
#include <sys/immu.h>	
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/systm.h>			/* splhi */
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>
#include <sys/sysmp.h>			/* numainfo 	*/
#include "replattr.h"
#include "repldebug.h"


zone_t	*replattr_zone;

sema_t pgrepl_sema[REPL_SEMAS];

repl_stats_t	repl_stat;
static void replattr_init(void);


/*
 * Do all needed to initialize the replication subsystem.
 * This is called as part of early initialization, and is 
 * responsible for initializing all other replication
 * dependent subsystems.
 */
void
repl_init()
{

	replattr_init();	/* Replication attributes 	*/
	repl_policy_init();	/* Replication policy 		*/
	repl_vninit();
}

/*
 * replattr_init_replzone:
 * 	various initialization needed for supporting page replication
 *	in kernel.
 *	This gets called ONCE during system initialization time
 */
static void
replattr_init(void)
{
	int	i;

	/* Initialize the zone used to allocate replication 
	 * attribute structures.
	 */
	replattr_zone = kmem_zone_init(sizeof(replattr_t), "VrRepl");

	/*
	 * Initialize an array of semaphores which are used to 
	 * wait on while trying to shootdown pages.
	 * This may become unnecessary if we decide to not
	 * waiting for ever to shoot down pages. 
	 */ 
	for (i = 0; i < REPL_SEMAS; i++)
		initnsema(&pgrepl_sema[i], 0, "replsema");

#ifdef	DEBUG
	idbg_replfuncs();
#endif	/* DEBUG */
}


/*
 * replattr_create:
 *	Create  and initialize a new page replication attribute structure.
 *
 * 	Allocate a replication attribute structure and attach it to 
 *	the desired object. 
 *	As part of creation initialize lock and synchronization structures.
 *
 * 	replattr_creat may get called twice for the same vnode, since
 *	we are trying to avoid holding vnode lock across files,
 *	(Note: kmem_zone_alloc can sleep!!).
 *	Hence, make sure that you want to attach the attribute structure,
 */
replattr_t *
replattr_creat()
{
	replattr_t    	*replattr;

	/* Allocate needed memory */
	replattr = (replattr_t *)kmem_zone_zalloc(replattr_zone, KM_NOSLEEP);

	if (replattr) {
		mrinit(&replattr->replwait, "replwait");
		REPLSTATE_MKRDY(replattr);
		REPL_VN_STATS.replobjs++;
	}

	return replattr;
}


/*
 * replattr_free:
 *	Free the data structures associated with replication.
 *	This routine is called when kernel wishes to reclaim the 
 * 	vnode data structure. It's convenient to free the replication
 *	data structure at that time, rather than when all mappings
 *	are gone. 
 *
 * 	replattr_destroy is called only if repllist_leave says there
 *	are no further references to this data structure.
 *	
 *	What can happen is, while system decides to reclaim vnode, 
 *	we could still be munging through some replication pages
 *	that may be with vnode.
 */
void
replattr_free(replattr_t *replattr)
{
	ASSERT(replattr);

	ASSERT(!replattr->replpgcnt);

	mrfree(&replattr->replwait);

	kmem_zone_free(replattr_zone, replattr);
	REPL_VN_STATS.replobjs--;

}


/* 
 * Replication State Transition routines.
 */

/*
 * Check if the replication attribute is in ready state to create
 * replicas. If so, hold the attribute in that state, till released.
 * If the state is ready, then grab the replwait lock in access mode
 * This provides the synchronization between processes creating replicas
 * and processes shooting down replicas.
 */
int
replattr_state_hold_rdy(replattr_t *replattr)
{

	/* 
	 * If not in ready state, don't even bother acquiring lock.
	 */
	if (replattr->replstate != REPLSTATE_RDY)
		return REPLOP_FAILED;

	if (cmrlock(&replattr->replwait, MR_ACCESS)){
		ASSERT(replattr->replstate == REPLSTATE_RDY);
		return REPLOP_SUCCESS;
	}

	/* 
	 * Some process is shooting down the replicas.
	 * return indicating it's not possible to create replicas.
	 */
	ASSERT(replattr->replstate != REPLSTATE_RDY);
	return REPLOP_FAILED;

}

/*
 * release the access count held on the rw lock.
 */
void
replattr_state_release_rdy(replattr_t *replattr)
{
	ASSERT(replattr->replstate == REPLSTATE_RDY);
	mrunlock(&replattr->replwait);
}

/*
 * Move replication attribute to "Trans" state.
 *	First try to hold the attribute in update mode.
 * 	Next, check if some other process raced with us in moving the
 *	attribute from ready->transient->block state, returrn error.
 *	If not, move the attribute to "trans" mode, and return success.
 */
int
replattr_state_trans(replattr_t *replattr)
{

	mrlock(&replattr->replwait, MR_UPDATE, PZERO);

	if (replattr->replstate == REPLSTATE_BLOCK) {
		/*
		 * Attribute is already in blocked state, and nothing
		 * else to do. Return .
		 */
		ASSERT(replattr->replpgcnt == 0);
		mrunlock(&replattr->replwait);
		return REPLOP_FAILED;
	}

	REPLSTATE_MKTRANS(replattr);

	return REPLOP_SUCCESS;
}

/*
 * Move from either "Trans" or "Blocked" state to ready state.
 *	If shoot down failed, then we need to move from trans -> ready state.
 *	As part of reenabling replication we need block -> ready transition. 
 */
void
replattr_state_mkrdy(replattr_t *replattr)
{
	/*
	 * Mark it back as ready for replication.
	 */
	ASSERT(ismrlocked(&replattr->replwait, MR_UPDATE));

	REPLSTATE_MKRDY(replattr);
	mrunlock(&replattr->replwait);
}

/*
 * Move the replication attribute to blocked state.
 * Happens when some process was successful in shooting down all replicas
 * associated with this vnode.
 */
void
replattr_state_block(replattr_t *replattr)
{
	/*
	 * Mark state as blocked. No further replication till 
	 * use count becomes zero.
	 */
	ASSERT(ismrlocked(&replattr->replwait, MR_UPDATE));
	ASSERT(replattr->replpgcnt == 0);

	REPLSTATE_MKBLOCK(replattr);

	/* 
	 * Release the lock here, in order to allow any other process that'
	 * trying to grab the lock in update mode to be woken up.
	 */
	mrunlock(&replattr->replwait);
}

/*
 * Set the replication  state ready on the attribute.
 *
 * Perhaps we can do away with locking() since this should happen
 * only when the vnode is being moved from being non-replicable to
 * be replicable.
 */
void
replattr_state_setrdy(replattr_t *replattr)
{
	mrlock(&replattr->replwait, MR_ACCESS, PZERO);
	replattr->replstate = REPLSTATE_RDY;
	mrunlock(&replattr->replwait);
	return;
}

#endif /* NUMA_REPLICATION */
