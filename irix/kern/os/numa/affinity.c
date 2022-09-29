/*
 * affinity.c
 *
 *	Affinity methods for memory_scheduler - process_scheduler interaction
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
 */


#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/pmo.h>
#include <sys/nodemask.h>
#include <sys/numa.h>
#include "pmo_base.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "mldset.h"
#include "memsched.h"

#define SCHED_AFF_RADIUS     2
#define min(a,b)	((a) <= (b) ? (a) : (b))

/*ARGSUSED*/
static void*
memaff_is_node_equal(cnodeid_t candidate, void* arg1, void* arg2)
{
        cnodeid_t base = (cnodeid_t)(long)arg1;
        
        ASSERT(base >= 0 && base < numnodes);
        ASSERT(candidate >= 0  && candidate < numnodes);

        if (base == candidate) {
                return ((void *) 1L);
        } else {
                return ((void *) 0);
        }
}


int
memaff_getaff(kthread_t *kt, cnodeid_t node)
{
        cnodeid_t affnode, neighbor;
        cnodemask_t maffbv, nodemask;
	

	ASSERT(issplhi(getsr()));

	affnode = kt->k_affnode;
	maffbv = kt->k_maffbv;
	nodemask = get_effective_nodemask(kt);

	/*
	 * No memory affinity info -> as good as 100% affinity
	 * for any node/cpu.
	 */
	if (affnode == CNODEID_NONE || affnode == node ||
				affnode < 0 || affnode > numnodes)
		return 100;

        /*
         * Next, we check the affinity vector. If there's
         * a match here, we return a 75% memory affinity.
         */

        if (CNODEMASK_TSTB(maffbv, node)) {
                return 75;
        } 

        /*
         * Next we check the neighbors to the mldlink
         */

        if (physmem_select_masked_neighbor_node(affnode,
			 min(SCHED_AFF_RADIUS,physmem_max_radius),
			 nodemask,
			 &neighbor,
			 memaff_is_node_equal, (void*)(long)affnode, 0) > 0) {
                return 50;
        }

        /*
         * We could also try the neighbors to each one of the nodes
         * in the affinity vector, but it'd be too expensive.
         */

        return 0;
}

/*
 * Returns non-zero if the node `candidate' can accept more load, and
 * the process procp can actually run there.
 * ---- To be updated by the scheduler dudes.
 */

/*ARGSUSED*/
static void*
memaff_availnode(cnodeid_t candidate, void *arg1, void* arg2)
{
	cpuid_t 	cpu;
	int		j;
	void		*retval;
	lbfunc_t	*sf = (lbfunc_t *) arg1;

        ASSERT(candidate >= 0 && candidate < numnodes);
	for (j = 0; j < CPUS_PER_NODE; j++) {
		cpu = cnode_slice_to_cpuid(candidate, j);
		if (cpu >= 0 && cpu < numcpus) {
			/* work only on cpus with valid ids */
			if (retval = (*sf)(cpu, arg2))
				return retval;
		}
	}
        return (NULL);
}

void*
memaff_getbestnode(kthread_t* kt, lbfunc_t sf, void *arg, int dolock)
{
        cnodemask_t 	affbv;
        cnodeid_t 	node, neighbor, affnode;
	void 		*retval;

	ASSERT(issplhi(getsr()));

	if (dolock && !kt_nested_trylock(kt)) {
		return NULL;
        }
        
	affnode = kt->k_mldlink ? mld_getnodeid(kt->k_mldlink) : CNODEID_NONE;
        affbv = kt->k_maffbv;
        
	if (dolock) {
		kt_nested_unlock(kt);
        }

        /*
         * First we check if the MLD link meets the filter
         * criteria.
         */
        
	if (affnode == CNODEID_NONE)
		return NULL;

        if (retval = memaff_availnode(affnode, (void *) sf, arg))
                return (retval);

        /*
         * Now we check the nodes in the aff vector
         */

        for (node = 0; node < numnodes; node++)
                if (CNODEMASK_TSTB(affbv, node))
                        if (retval = memaff_availnode(node, (void *) sf, arg))
                                return (retval);

        /*
         * And finally we check the neighbors to the MLD link node
         */

        if (retval = physmem_select_masked_neighbor_node(affnode,
			min(SCHED_AFF_RADIUS,physmem_max_radius), get_effective_nodemask(kt),
			&neighbor, memaff_availnode, (void *) sf, arg))
                return (retval);

        return (NULL);
}

/*
 * Given a node gets the next node in the hamiltonian circuit.
 */ 

cnodeid_t
memaff_nextnode(cnodeid_t cnode)
{
        int cube_index;
        int i = 0;
#ifdef DEBUG
        int iterations = 0;
#endif
        ASSERT(cnode < MAX_NODES);
        cube_index = bin_to_gray(cnode);
        i = cube_index;
        while (cube_to_cnode[i]  >= numnodes) {
                cube_index++;
                i = cube_index % log2_nodes;
		ASSERT(cube_to_cnode[i] >= numnodes);
#ifdef DEBUG
                iterations++;
                ASSERT(iterations < log2_nodes);
#endif
        }
        ASSERT(cube_to_cnode[i] < numnodes);
        return cube_to_cnode[i];

} 


#ifdef OLDOLDNOTNOT
/* ARGSUSED */
#error old code
void
memaff_alert(kthread_t* kt, cnodeid_t cnode)
{

	cnodeid_t affnode;

        ASSERT(issplhi(getsr()));

	if (!kt_nested_trylock(kt)) {
		return;
        }
        
	affnode = kt->k_mldlink ? mld_getnodeid(kt->k_mldlink) : CNODEID_NONE;

        if (affnode == CNODEID_NONE) {
		kt_nested_unlock(kt);
                return;
	}

        if (node_distance(affnode, cnode) > 4) {
                physmem_mld_place(kt->k_mldlink, cnode, CNODEMASK_ZERO());
        }
        
        kt_nested_unlock(kt);

}

#endif
