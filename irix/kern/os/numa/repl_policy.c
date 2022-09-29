#if defined(NUMA_REPLICATION)
/******************************************************************************
 * repl_policy.c
 *
 *	This file implements page replication policy mechanism. 
 *	
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
 ******************************************************************************/

#ident "$Revision: 1.10 $"

#include <limits.h>			/* LONG_MAX */
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/nodemask.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <sys/cmn_err.h>
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "mldset.h"
#include "pm.h"
#include "pmfactory_ns.h"
#include "repl_policy.h"
#include "replattr.h"
#include "aspm.h"

void
repl_policy_init()
{
	/*
	 * Anything needed to initialize the replication policy 
	 * data structures.
	 */

	repl_policy_default_init();
}

/*
 * repl_policy_setup:
 *	Setup replication policy for vnode.
 *
 *	This normally gets invoked when a vnode gets mapped in for execution.
 *	Purpose of this routine is:
 *		- Check if vnode can have replicated pages.
 *		- What should be the policy associated with this vnode.
 *
 */

replattr_t *
repl_policy_setup(char *policy_name)
{
	/* 
	 * Setup the replication policy for Vnode.
	 */
	pmfactory_t	repl_policy_create;
	replattr_t	*replattr;
	repl_policy_t	*rp;

	/* 
	 * Check if the replication policy requested has been registered.
	 */

	repl_policy_create = pmfactory_ns_find(pmfactory_ns_getdefault(), 
								policy_name);

	if (repl_policy_create == NULL) {
		cmn_err(CE_NOTE,"Unable to find Replication policy %s\n",
					policy_name);
		return 0;
	}


	/*
	 * Create replication attribute structure.
	 */
	replattr = replattr_creat();

	if (!replattr){
		/* 
		 * If we can't allocate memory for attributes, then there
		 * is no memory for replication. So just return.
		 */
		return 0;
	}

	/* 
	 * Create replication policy structure.
	 */ 
	rp = REPLPOLICY_GET(replattr);

	/*
	 * replication_policy_create is the "policy specific initialization
	 * routine responsible for creating policy private data structures.
	 * It's also responsible for updating the function pointers in
	 * repl_policy structure.
	 */
	if ( !((repl_policy_create)((void *)rp, (void *)0, curpmo_ns())) ) {
		replattr_free(replattr);
		return 0;
	}


	/*
	 * Now we have all the data needed to setup the replicatio policy.
	 * If no other process has raced with us to setup the policy,
	 * and no other process has raced to open the file in write mode,
	 * do it now.
	 */
	REPL_POLICY_STAT.rp_setup++;

	return replattr;
}

/*
 * repl_policy_teardown:
 *	Teardown any replication policy associated with the vnode. 
 *
 *	This gets called when a vnode is being reclaimed, and
 *	the replication layer was interposed on this vnode. 
 *	
 *	Free all associated data structures, and turn off the
 *	replicable bit.  (Inicidentally, clear non-replicable
 *	bit too).
 */
void
repl_policy_teardown(replattr_t *replattr)
{
	repl_policy_t	*rp;

	rp = REPLPOLICY_GET(replattr);

	ASSERT(rp);
	(rp->free)(rp);

	replattr_free(replattr);
	REPL_POLICY_STAT.rp_teardn++;
}


/*
 * Enable replication policy for a vnode.
 *	Called when vnode is being marked as inactive, and the vnode 
 *	was marked as non-replicable..
 */
void
repl_policy_enable(replattr_t *replattr)
{

	ASSERT(replattr);
	ASSERT(replattr->replstate == REPLSTATE_BLOCK);
	REPLSTATE_MKRDY(replattr);
}

/*
 * repl_policy_disbale:
 *	Do needed to disable replication.
 *
 *	If successful, mark vnode as non replicable, and return success.
 *	In order to mark vnode as non replicable, it's necessary to tear down
 *	all replicas associated with the vnode.  
 *
 *	If unable to shoot down replicas, indicate the error.
 */
int
repl_policy_disable(bhv_desc_t *bdp)
{

	replattr_t	*replattr = BHV_TO_REPL(bdp);
	vnode_t		*vp = BHV_TO_VNODE(bdp);

	/*
	 * Move to transient state. if some other process already did
	 * what we were supposed to do, just return.
	 */
	if (replattr_state_trans(replattr) != REPLOP_SUCCESS) {
		/*
		 * Some other process raced with us, and has shot down
		 * ALL replicated pages. So, return success.
		 */
		ASSERT(replattr->replstate == REPLSTATE_BLOCK);
		ASSERT(replattr->replpgcnt == 0);
		return REPLOP_SUCCESS;
	}

	 

	if (page_replica_shoot(vp, replattr, 0, btoc(LONG_MAX)) == REPLOP_FAILED) {
		/*
		 * Error in shooting down replicated pages.
		 * So, there is no reason to move the file to blocked
		 * state.
		 * move it back to ready state to allow more replicas
		 */
		replattr_state_mkrdy(replattr);
		return REPLOP_FAILED;
	}

	/*
	 * Successful in shooting down replicated pages.
	 * Block all further replicas.
	 */
	ASSERT(replattr->replpgcnt == 0);
	replattr_state_block(replattr);

	return REPLOP_SUCCESS;
}
#endif /* defined(NUMA_REPLICATION) */
