#if defined(NUMA_REPLICATION)
/******************************************************************************
 * repl_policy_default.c
 *
 *	This file defines the routines implementing default page replication
 *	policy.
 *
 *	Default Replication policy is based on the concept of considering 
 *	vnode to be a Resource (Actual resource is although the pages it has
 *	hashed), and associating certain level of affinity to this Resource 
 *	This resource can be located at one or more place, and each 
 *	resource will be responsible for servicing requests from its 
 *	neighbours. 
 *	
 *	vnodes start off with no affinity. When a vnode gets mapped for
 *	execution for the first time, it gets associated with some node on
 *	the system, and thus creates a pole of attraction at that node.
 *	In addition this resource will also get a "radius" of coverage
 *	indicating how wide its influence is. When a vnode has only 
 *	one poll of attraction, radius is large enough to cover entire
 *	system. (radius * num_poles = numnodes)
 *
 *	"radius" is considered to be the distance (number of hops on SN0)
 *	between the node where the "pole" is situated, and farthest node 
 *	that's attracted to this pole.
 *
 *	Any process which uses this vnode as a resource (say for 
 *	execution), will get attracted to this resource, and hence this 
 *	node. For e.g. "cp" binary could create a pole for itself and
 *	any user process that decides to exec "cp" could be scheduled to
 *	run on that node. 
 *
 *	When there are too many users mapping the same vnode, we start 
 *	creating multiple vnode poles of attraction. Simultaneously,
 *	the "radius" of coverage of each of these poles are reduced
 *	keeping (radius*poles) a constant. This makes the nodes covered
 *	by replica at each pole to be mutually exclusive.
 *
 *	Whenever the number of poles of attraction need to be increased,
 *	we increase it by doubling the number of poles of attraction. 
 *	So number of poles of attraction for a vnode are always a proper
 *	power of two.
 *
 *	Similar to increasing the number of poles of attraction, we decrease
 *	number of poles when the number of users mapping a vnode 
 *	reduces. In this case, we reduce the number of nodes by half,
 *	and allow the radius of coverage to be doubled.
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
 *****************************************************************************/

#ident "$Revision: 1.33 $"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <sys/cmn_err.h>
#include <sys/idbgentry.h>
#include <sys/param.h>
#include <sys/nodepda.h>
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "mldset.h"
#include "pm.h"
#include "pmfactory_ns.h"
#include "raff.h"		/* Just because memsched needs it. */
#include "memsched.h"		/* node_distance() .. */
#include "repl_policy.h"
#include "repl_pageops.h"
#include "replattr.h"
#include "repl_control.h"
#include "numa_stats.h"

#ifdef	DEBUG
struct vnode *repl_track_vp;
#endif

/*
 * Default replication policy data structure.
 * This data structure is allocated and attached to the policy_pvt
 * field in replication policy data structure.
 *
 * radius of coverage defines the radius each copy of the replicated
 * text covers. 
 * nodemask is a bit mask of all the nodes that have a copy of the
 * page. 
 * This provides a quick way to search for the nodes this vnode 
 * has attraction for.
 */ 
typedef struct repl_policy_default_s {

	unsigned short	radius;		/* Current radius of coverage 	*/
	unsigned short	prev_rad;	/* Previous value		*/
	uint		bitlock;

	/* 
	 * Poles of attraction (List of nodes this resource has affinity to)
	 * we start with one node, and as the resource utilization
	 * increases, we create multiple points of attraction.
	 * users of this resource get attracted to nearest one.
	 *
	 * affinity_list is defined as a pmolist object, which holds
	 * Mask of all nodes that' in use by this policy.
	 * Currently it's only set or cleared. Later on, this would
	 * be returned via some interface to suggest the best nodes
	 * where one can look for a page (Or place a page)
	 */
	cnodemask_t	nodemask;

} rp_default_t;

#define	RPD_LOCKBIT	(uint)(0x80000000)

#define	CNODEID_BITMASK(node)	(1ULL << (node))


/* 
 * Zone used to allocate/free default replication policy structures.
 */
zone_t		*rp_default_zone;

/*
 * Some static declarations for functions used internally.
 */
static cnodeid_t 	rp_default_match(repl_policy_t *, cnodeid_t, cnodeid_t *);
static repl_policy_t	*rp_default_create(void *, void *);
static	void		rp_default_destroy(repl_policy_t *);
extern pfd_t*   	rp_default_getpage(bhv_desc_t *, struct pm *, pfd_t *, int);

/**************************
 * PLACE HOLDERS
 */
static int XXrp_default_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
static char* XXrp_default_name(void);
static void XXrp_default_destroy(pm_t* pm);
static int XXrp_default_repl_srvc(struct pm* pm,
                                  pm_srvc_t srvc,
                                  void* args, ...);
extern int XXrp_default_init(void);



/*
 * Initialization routine called once during boot time.
 */
void
repl_policy_default_init()
{
	/*
	 * Initialize default replication policy module 
	 */
	rp_default_zone = kmem_zone_init(
					sizeof(rp_default_t),
					"repldefault");
	ASSERT(rp_default_zone);

	/* Add this to the policy module factory */
	pmfactory_ns_insert(pmfactory_ns_getdefault(), "ReplDefault", 
				(pmfactory_t)rp_default_create);


        /*
         * Added for now to initialize place holders
         */
        XXrp_default_init();

	return;

}

/*
 * Attach default replication policy to vnode passed.
 * Called while setting up replication data structure for a vnode.
 * (repl_policy_setup)
 */

#define	MIN_RADIUS		1

/*ARGSUSED*/
static repl_policy_t *
rp_default_create(void *arg1, void *arg2)
{
	repl_policy_t		*repl_policy = (repl_policy_t *)arg1;
	rp_default_t	*rp_default;


	rp_default = kmem_zone_zalloc(rp_default_zone, KM_SLEEP);

	init_bitlock(&rp_default->bitlock, RPD_LOCKBIT, "rpd_lock", 0);

	/*
	 * It's necessary to start off with a radius that works well,
	 * and avoids either too few or too many replicas. 
	 * 
	 * For now, we fix this to be the smaller of
	 *	MIN_RADIUS nodes, or (numnodes/3)
	 * MIN_RADIUS would be made to be a tuneable at some point of time.
	 * Also, make sure that radius will never go below MIN_RADIUS
	 *
	 * We still need to come up with an algorithm to decide this number.
	 *
	 */
	/*
	 * Start simple. Make radius to be 1. If we don't have enough memory,
	 * radius will get bumped.
	rp_default->radius      = MIN(MIN_RADIUS, MAX((numnodes/3), MIN_RADIUS));
	*/
	rp_default->radius      = MIN(1, numnodes);

	repl_policy->getpage    = rp_default_getpage;
	repl_policy->update     = 0;
	repl_policy->free	= rp_default_destroy;

	/*
	 * associate rp_defualt data structure with the private pointer
	 * in replicatio policy structure.
	 */
	policy_pvt_set(repl_policy, rp_default);

	return repl_policy; 
}

/*
 * repl_policy_default_destroy
 *	Free any data structure associated with default replication policy.
 */
void
rp_default_destroy(repl_policy_t *repl_policy)
{

	rp_default_t	*rp_default = repl_policy->policy_pvt;

	destroy_bitlock(&rp_default->bitlock);
	kmem_zone_free(rp_default_zone, rp_default);

	return;
}

/*ARGSUSED*/
static void *
rp_default_pole_place(cnodeid_t cnode, void *policy, void *unused)
{
	cnodeid_t	newnode;
	rp_default_t	*rp_default = (rp_default_t *)policy;
	cnodemask_t	mask = rp_default->nodemask;

	/*
	 * If radius is still zero, any node is fine.
	 */
	if (rp_default->radius == 0) 
		return (void *) 1L;

	/*
	 * Check if 'cnode' is a good node to place a new pole. 
	 * It checks following things.
	 * First it has to be atleast 2*rp_default->radius from any 
	 * other pole. (i.e. checked via bitmask 'nodemask')
	 * '2*radius' ensures that there is no overlap of coverage
	 * between the two nodes. 
	 * Second there should not be any resource constraints. 
	 */


	for (newnode = 0; CNODEMASK_IS_NONZERO(mask); CNODEMASK_SHIFTR_PTR(&mask), newnode++) {
		if (!(CNODEMASK_LSB_ISONE(mask)))
			continue;

		if (node_distance(newnode, cnode) < (2*rp_default->radius)){
			/*
			 * cnode is not in the distance spec to be a newpole.
			 * So reject this node.
			 */
			return (void *) 0;
		}
	}

	/*
	 * We could not find any node within the rp_default->nodemask
	 * which could have eliminated 'cnode' as a choice for new pole
	 * placement. So, return success for this node.
	 */

	/* 
	 * Before returning, we need to check if this node has sufficient
	 * resources to be a pole. 
	 * This is TBD
	 */
	/*
	 * For now just check if this node has sufficient memory.
	 */
	if (NODE_FREEMEM(cnode) < NODEPDA_RCD(cnode)->repl_mem_lowmark) {
		return (void *) 0;
	}
	return (void *) 1L;
}

/*
 * rp_default_update_coverage
 *
 * Update the Coverage of the policy to include 'mld_node'
 * This can be done either by creating a new 'pole' which covers
 * the 'mld_node' or by increasing the radius of the current policy
 * to cover the node. 
 */

cnodeid_t
rp_default_update_coverage(
	repl_policy_t		*replp,
	cnodeid_t		mld_node)
{

	cnodeid_t	newnode;
	rp_default_t	*rp_default = policy_pvt_get(replp, rp_default_t);
	cnodemask_t	nodemask; 
	int		distance, newdistance;
	cnodeid_t 	node;
	cnodeid_t	node_to_use = CNODEID_NONE;
	int		s;

	REPL_POLICY_STAT.rp_affupdate++;
	s = mutex_bitlock(&rp_default->bitlock, RPD_LOCKBIT);
	/*
	 * First find the node that can cover mld_node and can become
	 * a pole.
	 * Ideally we would like to pick a node that has a coverage
	 * which doesnot overlap with any other poles already existing.
	 * To do this, algorithm would be
	 *	For all nodes within a distance of rp_default->radius, 
	 *		select a node which is atleast 2*rp_default->radius
	 *		from the set of nodes in rp_default->nodemask
	 *		which could already have a replica. 
	 *
	 */
	
	newnode = CNODEID_NONE;
	if (rp_default->radius &&
		physmem_select_masked_neighbor_node(mld_node, rp_default->radius, 
				        get_effective_nodemask(curthreadp),
					&newnode, 
					rp_default_pole_place, 
					(void *)rp_default, 
					0)){
		/*
		 * we selected a new node that could be a 
		 * pole for the replication policy. 
		 * Indicate this in the nodemask, and return.
		 */
		ASSERT(newnode != CNODEID_NONE);
		CNODEMASK_SETB(rp_default->nodemask,newnode);
		mutex_bitunlock(&rp_default->bitlock, RPD_LOCKBIT, s);
		return newnode;
	}

	if (CNODEMASK_IS_ZERO(rp_default->nodemask)){
		/*
		 * We could neither get a new node within the 
		 * specified radius for 'rp_default', nor is 
		 * there a node assigned to this policy at this
		 * time.
		 * Just decide to use 'mld_node' and return that
		 * as the node to be used. This was the node 
		 * where processor has locality (since this node
		 * was picked from its mld_link, and should be the
		 * best possible location.
		 */
		REPL_POLICY_STAT.rp_affnonode++;

		CNODEMASK_SETB(rp_default->nodemask, mld_node);
		mutex_bitunlock(&rp_default->bitlock, RPD_LOCKBIT, s);
		return mld_node;
	}

	/* 
	 * We were unable to get a node that can act as a new pole
	 * This should be primarily due to resource constraints.
	 * So, just bump up the radius of coverage such that 
	 * mld_node gets covered.
	 *
	 * In order to do this, we need to find the shortest
	 * distance between any existing pole, and 'mld_node'
	 * and update radius to that value.
	 * For this purpose, scan through the bit mask, and
	 * calculate the shortest distance. 
	 */
	nodemask = rp_default->nodemask;

	/*
	 * Assign distance to physmem_max_radius + 1
	 * This way, we can check for '<' instead of '<=' 
	 * in the 'if' statement inside the loop, and
	 * save the cost of doing the distance/node_to_use
	 * updates over and over.
	 * We just need to pick the node that's closest, and
	 * any node is fine in that case.
	 */
	distance = physmem_max_radius + 1;
	for (node = 0; CNODEMASK_IS_NONZERO(nodemask); 
					node++, CNODEMASK_SHIFTR_PTR(&nodemask)) {
		if (!(CNODEMASK_LSB_ISONE(nodemask)))
			continue;
		newdistance = node_distance(node, mld_node);

		if (newdistance < distance) {
			distance = newdistance;
			node_to_use = node;
		}
	}

	ASSERT(node_to_use != CNODEID_NONE);
	ASSERT(newdistance <= physmem_max_radius);
	ASSERT(CNODEMASK_TSTB(rp_default->nodemask, node_to_use ));

#ifdef	DEBUG
	rp_default->prev_rad = rp_default->radius;
#endif

	rp_default->radius = newdistance; 

	mutex_bitunlock(&rp_default->bitlock, RPD_LOCKBIT, s);

	if (REPL_POLICY_DEBUG_LEVEL(replp) > 0) {
		char msg[128];
		int i;
		sprintf(msg, "RPD_update: radius %d node_to_use %d nodemask ",
			newdistance, node_to_use);
		for (i=0; i<CNODEMASK_SIZE; i++)
		   sprintf(msg, "%s 0x%lx ", msg, 
			CNODEMASK_WORD(rp_default->nodemask,i));
		cmn_err(CE_NOTE|CE_CPUID, msg);
	}
			
	return node_to_use;
}

/*
 * replica_selector:
 *	Returns 1 if 'target_node' is a pole for the passed in policy.
 *	Returns a zero otherwise.
 */

/*ARGSUSED*/
static void *
replica_selector(cnodeid_t target_node, void *rp, void *unused)
{
	ASSERT(target_node >= 0 && target_node < numnodes);

	if (CNODEMASK_TSTB(((rp_default_t *)rp)->nodemask, target_node)) {
		return (void *) 1L;
	} else {
		return (void *) 0;
	}
}

/*
 * repl_policy_default_match:
 * 	Find the node that's closest to "cnodeid" and hence has the 
 *	maximum attraction to the node. 
 *	
 *	This routine looks through the bitmask of nodes where the replica 
 *	for the node could exist, and tries to get the node that's
 *	closest to cnodeid.
 *	
 *	Returned node is passed in 'newnode'.
 *
 *	Routine returns non-zero for success, 0 for failure
 */

static cnodeid_t
rp_default_match(
	repl_policy_t		*repl_policy,
	cnodeid_t		cnodeid,
	cnodeid_t		*newnode)
{
	rp_default_t	*rp_default;
	int		s;

	rp_default = policy_pvt_get(repl_policy, rp_default_t);
	ASSERT(rp_default);


	REPL_POLICY_STAT.rp_affmatch++;
	/*
	 * Find the node that may have a page that covers the 'cnodeid' 
	 * If one such page does not exist, mark 'cnodeid' as a 
	 * potential node where we could create replicas.
	 */

	/*
	 * First, if we donot have any affinity, return no match.
	 */
	if (CNODEMASK_IS_ZERO(rp_default->nodemask))
		return 0;

	s = mutex_bitlock(&rp_default->bitlock, RPD_LOCKBIT);

	/*
	 * Start searching from 'cnodeid' and
	 * find the nearest neighbor within policy's radius 
	 *
	 * physmem_select_masked_neighbor_node does this job for us.
	 *	It invokes replica_selector function on all nodes,
	 *	that are within 'radius' from 'cnodeid'. replica_selector
	 *	returns a '1' if any of these nodes is a pole.
	 *	physmem_select_masked_neighbor_node return when it gets a '1'
	 *	from the selector function. It also stuffs the selected
	 *	node in 'newnode'
	 */
	
	if (physmem_select_masked_neighbor_node(cnodeid, rp_default->radius,
				 curthreadp->k_nodemask, newnode, replica_selector, 
				(void *)rp_default, 0)) {
		/*
		 * We found a node that covers 'cnodeid'.
		 * Make sure of that fact.
		 */
		ASSERT(*newnode >= 0 && *newnode < numnodes);
		ASSERT(CNODEMASK_TSTB(rp_default->nodemask, *newnode));
		ASSERT(node_distance(cnodeid, *newnode) <= rp_default->radius);

		mutex_bitunlock(&rp_default->bitlock, RPD_LOCKBIT, s);

		if (REPL_POLICY_DEBUG_LEVEL(repl_policy) > 0 ) {
			cmn_err(CE_NOTE|CE_CPUID,
				"RPD_match returning new node %d\n", *newnode);
		}
		return 1;
	} 

	/*
	 * We could not find a replica that's within the coverage area
	 * defined by radius. 
	 * Leave it to the caller to decide what's the right action.
	 */ 

	mutex_bitunlock(&rp_default->bitlock, RPD_LOCKBIT, s);
	return 0;
}

/*
 * Default Replication policy getpage.
 * 
 * If process has a hint about the node where its locality domain is,
 * (UT_TO_KT(private.p_curuthread->ut_proc)->k_mldlink) get that first.
 * Otherwise, we go with the
 * locality associated with the vnode.
 * 
 * From the replication policy associated with the vnode, find out the
 * nearest node that covers the node preferred by process.
 * This would give the "Best match" node based on the topology
 * associated with the vnode. 
 * 
 * Now, scan the page hash chain to see if there is a page that
 * exists on the "best match" node. 
 *
 * If a page is found, return that page.
 *
 * Otherwise, evaluate the system state to see if it's possible
 * to create a new page. If so, create a new page, copy the data
 * and return the new page.
 * 
 * If system state doesnot allow a new page to be created, we have
 * already done our best. Just return the old page that was passed
 * in.
 */

/*ARGSUSED*/
pfd_t *
rp_default_getpage(
	bhv_desc_t	*bdp, 
	struct pm	*pm, 
	pfd_t		*srcpfd,
	int		flags)
{

	cnodeid_t	mld_node;	/* Node where page is needed */
	cnodeid_t       dst_node;       /* Node where page is allocated */
	cnodeid_t	targ_node;	/* Target Node 			*/
	pfd_t		*newpfd;	/* Pointer to new pfdat allocated */
	int		vcolor;		/* virtual color of the page */
	repl_policy_t	*replp;	/* vnode replication policy pointer */
	replattr_t	*replattr;	/* replication attribute */
        mld_t           *mld;           /* memory affinity link */

	REPL_POLICY_STAT.rp_getpage++;

	/* 
	 * Step 1: Using process's pm, find the most suitable node id.
	 * 	Plan here is, this method would return an "ideal" node, and 
	 * 	a bit map of preferred nodes. 
	 * 	We will first try to scan the existing replicas to see if
	 * 	there is a match for ideal node.
	 *	Only first part is (i.e. scanning for most suitable one is
	 *	implemented.
	 */

        mld = UT_TO_KT(private.p_curuthread)->k_mldlink;
	if (mld){
		ASSERT(mld_refcount(mld) > 0);
		mld_node = mld_getnodeid(mld);
	}else{
		/*
		 * there seem to be cases where mldpool is zero.
		 * This happens if a pagealloc() has not been done
		 * on behalf of the process yet (Process still running
		 * in its  libc?. 
		 * In such case, we try to see if we can satisfy the
		 * request by checking if a page exists on a local node.
		 */
		mld_node = cnodeid();
	}

	ASSERT(mld_node < MAX_COMPACT_NODES);

	/* Get replication policy associated with vnode */
	replattr = BHV_TO_REPL(bdp);
	replp = REPLPOLICY_GET(replattr);

	ASSERT(replp);

	/* 
	 * Invoke the replication matching policy to find out 
	 * a suitable node where the page for the vnode may be found.
	 */
	dst_node = CNODEID_NONE;
	if (rp_default_match(replp, mld_node, &dst_node) == 0) {
		/*
		 * didnot find a node that has the replica covering 
		 * 'mld_node' 
		 * We either need to create a new node, and replicate there,
		 * or increase the radius to cover this node. 
		 */
		dst_node = rp_default_update_coverage(replp, mld_node);
	}

	/*
	 * Find the page that matches the requirementes of the caller.
	 * First preference goes to "srcpfd"
	 */
	if (pfdattocnode(srcpfd) == dst_node){
		NUMA_STAT_ADD(dst_node, repl_page_reuse, 1);
		return srcpfd;
	}

	newpfd = pfind_node(srcpfd->pf_vp, srcpfd->pf_pageno, 
							dst_node, flags);

	if (newpfd) {
		/* OK, we have a newer page that's matching our ideal
		 * requirements.
		 * Return that page.
		 */
		ASSERT(newpfd != srcpfd);
		NUMA_STAT_ADD(dst_node, repl_page_reuse, 1);
		return newpfd;
	}

	/* 
	 * Step 2:
	 * Time to create a new replica 
	 *
	 * First check if system resource state allow further replications
	 * to be done at this time.
	 * This would require to check if the global system memory state
	 * allows for replication as well as the local node where we
	 * need to create the copy allows this possibility.
	 * 
	 * TBD
	 *
	 * If all fails, return the old page. 
	 */
#if DEBUG
	/*
	 * Force replication to go on in DEBUG mode.
	 */
	targ_node = dst_node;
#else
	/* 
	 * Select a node which meets the resource utilitzation 
	 * criteria -- enough physical memory left, enough 
	 * bandwidth between the source node and destination node.
	 */
	targ_node = repl_control(pfdattocnode(srcpfd), 
				dst_node, RQMODE_ADVISORY);

#endif

	if (targ_node == CNODEID_NONE) {
		NUMA_STAT_ADD(dst_node, repl_control_refuse, 1);
		return(srcpfd);
	} else {
		dst_node = targ_node;
	}

	/*
	 * As part of page allocation, we will try not to sleep and
	 * extend the faulting duration of the process. If the
	 * page is not readily available, it's better to return
	 * the one we have.
	 */
	vcolor = pfncolorof(pfdattopfn(srcpfd));
	newpfd = pagealloc_node(dst_node, vcolor, VM_MVOK|VM_NOSLEEP);

	if (newpfd) {
		pfd_t	*pfd2;
		pfd2 = page_replica_insert(srcpfd, newpfd);

		ASSERT(pfd2);
		ASSERT(pfd2 != srcpfd);

		if (pfd2 != newpfd) {
			/* 
			 * There is already a page on the page cache, which
			 * was inserted by some other process. Return
			 * that to the user process.
			 *
			 * Process which inserted this page would have taken
			 * responsibility of copying the data.
			 *
			 * Remember: If a page already exists, then newpfd
			 * would have been freed in the call chain.
			 * (page_replica_insert to be precise)
			 */
			NUMA_STAT_ADD(dst_node, repl_page_reuse, 1);
			return pfd2;
		}
		page_copy(srcpfd, newpfd, NOCOLOR, NOCOLOR);
		pagedone(newpfd);
		NUMA_STAT_ADD(dst_node, repl_page_destination, 1);
		return newpfd;
	} else {
		NUMA_STAT_ADD(dst_node, repl_page_notavail, 1);
	}

	return srcpfd;
}

#ifdef DEBUG
idbg_replprint(void *data)
{
	int i; 
	repl_policy_t		*rp = data;
	rp_default_t	*rp_default;
	
	rp_default = policy_pvt_get(rp, rp_default_t);

	if (!rp_default) {
		return 0;
	}

	qprintf("policy@ %x radius %d(prev %d) nodemask ",
			rp_default, 
			rp_default->radius, rp_default->prev_rad);
	for (i=0; i<CNODEMASK_SIZE; i++) 
	     qprintf("%x ", CNODEMASK_WORD(rp_default->nodemask,i));
	
	qprintf("\n\n");

	return 0;
}
#endif	/* DEBUG */

/*
 * Place holders for "ReplicationDefault"
 */
 

/*
 * The ReplicationDefault initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
XXrp_default_init()
{
        pmfactory_export(XXrp_default_create, XXrp_default_name());

        return (0);
}

/*
 * The constructor for ReplicationDefault takes no arguments
 */

/*ARGSUSED*/
static int
XXrp_default_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns)
{
        ASSERT(pm);
        ASSERT(pmo_ns);
        
        /*
         * If args (vnode) is NULL, setup
         * aff to return current node.
         */
        pm->repl_srvc = XXrp_default_repl_srvc;
        pm->repl_data = NULL;
        
        return (0);
}

/*ARGSUSED*/
static char*
XXrp_default_name(void)
{
        return ("ReplicationDefault");
}

/*ARGSUSED*/
static void
XXrp_default_destroy(pm_t* pm)
{
        ASSERT(pm);
}


/*
 * This method provides generic services for this module:
 * -- destructor,
 * -- getname,
 * -- getaff (not valid for this placement module).
 */
/*ARGSUSED*/
static int
XXrp_default_repl_srvc(struct pm* pm, pm_srvc_t srvc, void* args, ...)
{
        ASSERT(pm);
        
        switch (srvc) {
        case PM_SRVC_DESTROY:
                XXrp_default_destroy(pm);
                return (0);

        case PM_SRVC_GETNAME:
		*(char **)args = XXrp_default_name();
                return (0);

        case PM_SRVC_GETAFF:
                raff_setup((raff_t*)args, 0, CNODEMASK_ZERO(), 1, RAFFATTR_ATTRACTION);
                return (0);

	case PM_SRVC_GETARG:
		*(int *)args = 0;
		return (0);

        default:
                cmn_err(CE_PANIC,
                        "[XXrp_default_repl_srvc]: Invalid service type");
        }

        return (0);
}
                                    





#endif /* defined(NUMA_REPLICATION) */
