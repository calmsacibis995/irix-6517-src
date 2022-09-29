/*
 * memsched.c
 *
 *	physical memory placement for NUMA machines
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
#include <ksys/as.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/pmo.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/kopt.h>
#include <sys/nodepda.h>
#include <sys/iograph.h>
#include <sys/nodemask.h>
#include "pmo_base.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "physmem_perm.h"
#include "physmem_comb.h"
#include "physmem_radial.h"
#include "physmem_debug.h"
#include "pmo_error.h"
#include "memsched.h"
#include "numa_init.h"
#include "pm.h"
#include "aspm.h"
#include "memfit.h"


/*
 * Maximum physmem radius
 */
int physmem_max_radius = 0;

int log2_nodes;
cnodeid_t cube_to_cnode[MAX_NODES];
cnodeid_t cnode_to_cube[MAX_NODES];
cnodemask_t __clustermask_rotor;

static uchar_t distance_metric[MAX_NODES][MAX_NODES];
extern char arg_maxnodes[];

int ceil_log2(int);
int physmem_place_cube(int, cnodeid_t, size_t*, size_t*, pmolist_t*);
int physmem_place_cube_p(int, cnodeid_t, size_t*, size_t*, pmolist_t*, int*);
int physmem_place_cube_with_affinity(int, cnodeid_t, 
				     size_t*, size_t*, pmolist_t*);
int physmem_place_cube_with_affinity_p(int, cnodeid_t, 
				     size_t*, size_t*, pmolist_t*, int* );
int physmem_place_physical(pmolist_t*, int, pmolist_t*,
                           int, rqmode_t, size_t*);
cnodeid_t physmem_selectnode(cnodeid_t, int, pmolist_t*, size_t*);
int physmem_place_cpucluster(int, pmolist_t*, cnodeid_t, size_t*);
int physmem_place_cluster(int, pmolist_t*, cnodeid_t, size_t*);
static cnodemask_t get_loadmask(int);

/*
 * mld-set memory placement routine. 
 * this routine finds topological regions of available
 * memory for a specified placement. If the topology is found
 * the mldset->mldlist array of cnodeid's is set to the found memory nodes.
 * return value is 0 for success and non-zero for failure.
 */

/*ARGSUSED*/
int
physmem_mldset_place(mldset_t* mldset)
{
	int i, j, cube_dim, ret_val;
	cnodeid_t start_node;
	size_t mem_ask[MAX_NODES], mem_avail[MAX_NODES];
	cnodemask_t cnodemask, loadmask, effective_cnodemask;
	int live_nodes;
	int live_cpus;

        topology_type_t topology_type;
        rqmode_t rqmode;
        pmolist_t* base_mldlist;
        int base_mldlist_len;

        pmolist_t* mldlist;
        int mldlist_len;
        pmolist_t* rafflist;
        int rafflist_len;
        int rep_factor;
        int base_efflen;
        int errcode;
        
	/* REFERENCED */
	auto pgcnt_t size, rss;

	ASSERT (mldset != NULL);

	base_mldlist = mldset_getmldlist(mldset);
        base_mldlist_len = mldset_getclen(mldset);
	cnodemask = effective_cnodemask = get_effective_nodemask(curthreadp);
		
	loadmask = get_loadmask(base_mldlist_len);
	for (i=0; i<CNODEMASK_SIZE; i++) { 
           physmem_debug(1,"[mldset_place]: cnodemask[%d] = ", i);
           physmem_debug(1,"0x0%llx\n", CNODEMASK_WORD(cnodemask,i));
	}
	for (i=0; i<CNODEMASK_SIZE; i++) {
           physmem_debug(1,"[mldset_place]: loadmask[%d] = ", i);
           physmem_debug(1,"0x0%llx\n", CNODEMASK_WORD(loadmask,i));
	}

        if( CNODEMASK_TSTM(cnodemask,loadmask) &&
		!is_batch(curthreadp) && !is_bcritical(curthreadp)) {
                CNODEMASK_ANDM(cnodemask,loadmask);
        }

	for (i = 0, live_nodes = 0, live_cpus = 0; i < numnodes; i++) {
		if( CNODEMASK_TSTB(cnodemask,i) ) {
                        live_nodes++;
			live_cpus += CNODE_NUM_CPUS(i);
                }
        }
        
	ASSERT (live_nodes);
	ASSERT (live_cpus);

	if( base_mldlist_len > live_nodes &&
	     CNODEMASK_NOTEQ(cnodemask,effective_cnodemask) ){
		cnodemask = effective_cnodemask;
		for (i = 0, live_nodes = 0, live_cpus = 0; i < numnodes; i++) {
			if( CNODEMASK_TSTB(cnodemask,i) ) {
				live_nodes++;
				live_cpus += CNODE_NUM_CPUS(i);
			}
		}
	}  

        ret_val = 0;
        errcode = 0;

        /*
         * First of all, we need to make sure the requested
         * size for each MLD in the MLDSET is valid. If it
         * is the DEFAULTSIZE, then we need to set it to something
         * reasonable. For now I'll just use the following
         * approximation:
         * (p_size / 2) / number_of_mlds
         * Roughly, divided by two to eliminate code and
         * data not included in this MLDSET; divided by
         * the number of MLDs assuming uniform distribution
         * of data.
         */


        if (base_mldlist_len < 1) {
                errcode = PMOERR_EINVAL;
                goto error1;
        }
        
        for (i = 0; i < base_mldlist_len; i++) {
                mld_t* mld = (mld_t*)pmolist_getelem(base_mldlist, i);
                if (mld_getsize(mld) == MLD_DEFAULTSIZE) {
			as_getassize(curuthread->ut_asid, &size, &rss);
                        size = (ctob(size) / 2) / base_mldlist_len;
                        mld_setsize(mld, size);
                        physmem_debug(1,"SETSIZE MLD[0x%x]:", mld);
                        physmem_debug(1," %d\n", size);
                }
        }

        /*
         * Now, we determine the effective mldlist to be used by
         * placement methods.
         * We may need to change the effective list and/or list length
         * for requests exceeding the available nodes (live_nodes) or
         * some specific topologies.
         */
        
        topology_type = mldset->topology_type;
        rqmode = mldset->rqmode;
        rafflist = mldset->rafflist;
        rafflist_len = (rafflist == NULL) ? 0 : pmolist_getclen(rafflist);

        /*
         * Determine effective mldlist parameter to be used by
         * placement methods
         */
	mldlist = base_mldlist;

        switch(topology_type) {
        case TOPOLOGY_FREE:
        {
                rep_factor = (base_mldlist_len + live_nodes - 1) / live_nodes;
                mldlist_len = (base_mldlist_len + rep_factor - 1) / rep_factor;
                base_efflen = base_mldlist_len;
                break;
        }
        case TOPOLOGY_CUBE:
        case TOPOLOGY_CUBE_FIXED:
        {
                cube_dim = ceil_log2(base_mldlist_len);
                if (base_mldlist_len != (1 << cube_dim)) {
                        errcode = PMOERR_EINVAL;
                        goto error1;
                }
                if (base_mldlist_len > live_nodes) {
                        rep_factor = 1 << (cube_dim + 1 - ceil_log2(live_nodes + 1));
                } else {
                        rep_factor = 1;
                }
                mldlist_len = (base_mldlist_len + rep_factor - 1) / rep_factor;
                base_efflen = base_mldlist_len;
                break;
        }
        case TOPOLOGY_PHYSNODES:
        {
                /*
                 * For physical requests, we just do exactly what the user
                 * is asking.
                 */
                mldlist_len = base_mldlist_len;
                rep_factor = 1;
                base_efflen = base_mldlist_len;
                break;
        }
        case TOPOLOGY_CPUCLUSTER:
        {
                rep_factor = (base_mldlist_len + live_cpus - 1) / live_cpus;
                mldlist_len = (base_mldlist_len + rep_factor - 1) / rep_factor;
                base_efflen = base_mldlist_len;
                break;
        }
        default:
                errcode = PMOERR_EINVAL;
                goto error1;
        }

#ifdef DEBUG_MEMSCHED
        {
                extern int idbg_mldprint(mld_t* mld);
                extern int idbg_mldsetprint(mldset_t* mldset);
                
                printf("ORIGINAL MLDSET:\n");
                printf("Top: %d, len: %d\n", topology_type, mldset_getclen(mldset));

                printf("*****EFFECTIVE MLDLIST (len: %d, rep_factor: %d)\n",
                       mldlist_len, rep_factor);

        }
        
	physmem_debug(mldlist_len>1, "[mldset_place]: mldlist_len = %d .\n",
                      mldlist_len);
	physmem_debug(mldlist_len>1, "[mldset_place]: topology = %x .\n", 
                      mldset->topology_type);	
	if (mldlist_len>1){
		physmem_debug(1,"[mldset_place]: live_nodes = %d\n", live_nodes );
		for (i=0; i<CNODEMASK_SIZE; i++) 
		   physmem_debug(1,"[mldset_place]: cnodemask[%d] = 0x0%llx\n", 
				(i, CNODEMASK_WORD(cnodemask,i)) );
		
		for (i=0;i<numnodes;i++){
			physmem_debug(1,"[mldset_place]: [%d] ", i);
			physmem_debug(1,"freemem = %d, ",NODE_FREEMEM(i));
			physmem_debug(1,"future = %d\n",NODE_FUTURE_FREEMEM(i));
	    }
	}

#endif

	switch (topology_type) {
	case TOPOLOGY_CUBE:
	case TOPOLOGY_CUBE_FIXED:
	case TOPOLOGY_PHYSNODES:
	{
		/*
		 * local copy of memory requests into mem_ask[].
		 */

	        for (i = 0; i < mldlist_len; i++) {
       	         int index;
       	         mem_ask[i] = 0;
       	         for (j = 0; j < rep_factor; j++) {
                        index = i + j * mldlist_len;
                        if (index < base_mldlist_len) {
                                mem_ask[i] += (size_t)mld_getsize((mld_t*)pmolist_getelem(base_mldlist, index));
                        }
                 }
        	}
		break;
	}
	default:
		break;
	}

	/*
	 * local copy of available memory in mem_avail[].
	 */

	for (i = 0; i < numnodes; i++) {
		mem_avail[i] = CNODEMASK_TSTB(cnodemask,i) ? NODE_MIN_FUTURE_FREEMEM(i) : 0;
	}

	/*
	 * non power of 2 machines have zero memory in 
	 * remainder nodes
	 */

	for (i = numnodes; i < (1<<log2_nodes) ; i++) {
                mem_avail[i] = 0;
        }

	switch(topology_type) {
	case TOPOLOGY_CUBE:

		/*
		 * This case is almost like the one below except
		 * we have to iterate over all possible cube orientations.
		 */

		cube_dim = ceil_log2(mldlist_len);

		/*
		 * mldlist_len must be a power of 2
		 */

		if (mldlist_len != (1<<cube_dim) ) {
                        errcode = PMOERR_EINVAL;
                        goto error2;
                }

		if (rafflist) {
			int x, y, k;
			int nfac = facs0thru8[cube_dim];
			int ncube = 1 << cube_dim;
			unsigned char* List = perms0thru8[cube_dim];
			int Cube[MAX_NODES];
			/*
			 * there *is* memory affinity so the search
			 * begins at the attractive node
			 */
			start_node = raff_getnodeid((raff_t *)pmolist_getelem(rafflist, 0));
			physmem_debug(1,
                                      "[physmem_place_cube_with_affinity_p]: cube_dim = %d .\n",
				      cube_dim);
			for (i = 0; i < nfac; i++) {
				/*
				 * permute the bits of Cube
				 */
				for (j = 0; j < ncube; j++) {
					x = 0;
					y = j;
					for (k = 0; k < cube_dim; k++) {
					    x |= ((y>>k)&1)<<List[7*i+k];
                                        }
					Cube[j] = x;
				}

				/*
				 * circularly rotate Cube's coordinates
				 */
				for (j = 0; j < ncube; j++) {
					ret_val = 
                                                physmem_place_cube_with_affinity_p(
                                                        cube_dim, start_node, mem_ask, 
                                                        mem_avail, mldlist, Cube);
					if (0 == ret_val) {
                                                break;
                                        }
					x = Cube[0];
					for (k = 0; k < ncube-1; k++) {
                                                Cube[k] = Cube[k+1];
                                        }
					Cube[ncube-1] = x;
				}
				if (0 == ret_val) {
                                        break;
                                }
			}
			
		} else {
			int x,y,k;
			int nfac = facs0thru8[cube_dim];
			int ncube = 1 << cube_dim;
			unsigned char *List = perms0thru8[cube_dim];
			int Cube[MAX_NODES];

			/*
			 * there is no memory affinity so pick starting node
			 * based on current node mask and allocation history
			 */
			start_node = memfit_selectnode(cnodemask);

			physmem_debug(1,
                                      "[physmem_place_cube_p]: cube_dim = %d .\n",
                                      cube_dim);

			for (i = 0; i < nfac; i++){
				/*
				 * permute the bits of Cube
				 */
				for (j = 0; j < ncube; j++){
					x = 0;
					y = j;
					for (k = 0; k < cube_dim; k++) {
					    x |= ((y>>k)&1)<<List[7*i+k];
                                        }
					Cube[j] = x;
				}

				/*
				 * circularly rotate Cube's coordinates
				 */
				for (j = 0; j < ncube; j++){
					ret_val = 
                                                physmem_place_cube_p(
                                                        cube_dim, start_node, mem_ask, 
                                                        mem_avail, mldlist, Cube);
					if (0 == ret_val) {
                                                break;
                                        }
					x = Cube[0];
					for (k = 0; k < ncube-1; k++) {
                                                Cube[k] = Cube[k+1];
                                        }
					Cube[ncube-1] = x;
				}
				if (0 == ret_val) {
                                        break;
                                }
			}
		}
		break;

	case TOPOLOGY_CUBE_FIXED:

		cube_dim = ceil_log2(mldlist_len);

		/*
		 * mldlist_len must be a power of 2
		 */

		if (mldlist_len != (1<<cube_dim)) {
                        errcode = PMOERR_EINVAL;
                        goto error2;
                }
		
		if (rafflist) {
			/*
			 * there *is* memory affinity so the search
			 * begins at the attractive node
			 */
		    	start_node = raff_getnodeid((raff_t *)pmolist_getelem(rafflist, 0));
			ret_val = physmem_place_cube_with_affinity(cube_dim,
                                                                   start_node, mem_ask, 
                                                                   mem_avail, mldlist);
		} else {

			/*
			 * there is no memory affinity so pick starting node
			 * based on current node mask and allocation history
			 */	

			start_node = memfit_selectnode(cnodemask);

			ret_val = physmem_place_cube(cube_dim, start_node,
                                                     mem_ask, mem_avail, mldlist);
		}
		break;

	case TOPOLOGY_PHYSNODES:
		/*
		 * all we have to do here is to make sure the memory
		 * is available on the nodes specified in the raff list
		 */
		if (mldlist_len > MAX_NODES) {
                        errcode = PMOERR_EINVAL;
                        goto error2;
                }

                if (rafflist_len < mldlist_len) {
                        errcode = PMOERR_EINVAL;
                        goto error2;
                }
                        
		ret_val = physmem_place_physical(mldlist,
                                                 mldlist_len,
                                                 rafflist,
                                                 rafflist_len,
                                                 rqmode,
                                                 mem_ask);
		break;

        case TOPOLOGY_CPUCLUSTER:         

		if (rafflist){
			/*
			 * there *is* memory affinity so the search
			 * begins at the attractive node
			 */ 
		    	start_node = raff_getnodeid(
					(raff_t *)pmolist_getelem(rafflist, 0));
		} else {

			/*
			 * There is no memory affinity so pick starting node
			 * based on current node mask and allocation history.
			 * Assume nodes have all CPUs enabled and select
			 * starting node in the same way as done
			 * for regular clusters.
			 */	
			start_node = physmem_selectnode(
				memfit_selectnode(cnodemask),
				(mldlist_len + CPUS_PER_NODE-1) / CPUS_PER_NODE,
				mldlist,
				mem_avail);

		}

		ret_val = physmem_place_cpucluster(mldlist_len,
				mldlist, start_node, mem_avail);
		break;

	case TOPOLOGY_FREE:
	default:

		if (rafflist){
			/*
			 * there *is* memory affinity so the search
			 * begins at the attractive node
			 */ 
		    	start_node = raff_getnodeid(
					(raff_t *)pmolist_getelem(rafflist, 0));
		} else {
			/*
			 * there is no memory affinity so pick starting node
			 * based on current node mask and allocation history
			 */	
			start_node = physmem_selectnode(
				memfit_selectnode(cnodemask),
				mldlist_len,
				mldlist,
				mem_avail);

		}

		ret_val = physmem_place_cluster(mldlist_len, 
				mldlist, start_node, mem_avail);

		break;
	}

	/*
	 * If we did not find a placement of the specified topology
	 * and the request is advisory, then get a cluster instead. 
	 */
	if ( ret_val != 0 && rqmode == RQMODE_ADVISORY) {
                ASSERT(start_node != -1);
                ret_val = physmem_place_cluster(mldlist_len, mldlist, start_node, mem_avail);
        }

        if (ret_val != 0) {
                errcode = ret_val;
                goto error2;
        }

	/*
	 * replicate if needed
	 */

        for (i = 0; i < mldlist_len; i++) {
                int index;
                mld_t* mld = (mld_t*)pmolist_getelem(mldlist, i);
                cnodeid_t node = mld_getnodeid(mld);
                for (j = 1; j < rep_factor; j++) {
                        index = i + j * mldlist_len;
                        if (index < base_efflen) {
                                physmem_mld_place_onnode(
                                        (mld_t*)pmolist_getelem(mldlist, index),
                                        node);
                        }
                }
        }

#ifdef DEBUG
	physmem_debug(1,"MLDSET_PLACE: ",0);
        for (i = 0; i < base_mldlist_len; i++) {
                mld_t* mld = (mld_t*)pmolist_getelem(base_mldlist, i);
                if (!mld_isplaced(mld)) {
                        cmn_err_tag(104,CE_PANIC, "UNPLACED MLD, nodeid=%d",
                                mld_getnodeid(mld));      
                }
		physmem_debug(1,"%3d ",mld_getnodeid(mld));
        }
	physmem_debug(1,"\n",0);
#endif

        ASSERT (errcode == 0);
        ASSERT (ret_val == 0);
        return (0);
                
  error2:
  error1:
        ASSERT(errcode != 0);
	return (errcode);
}

/*ARGSUSED*/
void
physmem_mld_place(mld_t* mld, cnodeid_t center, cnodemask_t skipbv)
{
        int i;
        int j;
        cnodeid_t chosen;
        cnodeid_t candidate;
        cnodemask_t cnodemask;
        memfit_t* memfit = &global_memfit;
        int seq;
        
        ASSERT(mld);
        
        if (mld_isplaced(mld)) {
                ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
        }
        
        /*
         * Choose some node with a reasonable amount of free
         * memory, by scanning radially from the currently
         * running node.
         * If that fails, we just place on the center node
         * In addition, we never place on a recently assigned node
         */

        chosen = CNODEID_NONE;

	cnodemask = get_effective_nodemask(curthreadp);
	
	/*
         * Search in radial order twice.
	 * The first time we use the current mem_unassigned,
	 * the second time we use a fresh mem_unassigned
	 */

        for (seq = 0; seq < 2; seq++) {

                /*
                 * Consider the center first
                 */
                candidate = center;
                if (NODE_FREEMEM_REL(candidate) > 5
                    && !SKIPN(skipbv, candidate)
                    && CNODEMASK_TSTB(memfit->mem_unassigned, candidate)
		    && CNODEMASK_TSTB(cnodemask, candidate)) {
                        CNODEMASK_CLRB(memfit->mem_unassigned, candidate);
                        chosen = candidate;
                        goto out;
                }

                /*
                 * And now the rest of the spiral
                 */
		for ( i = 1 ; i <= physmem_max_radius ; i++ ) for ( j=0 ; j < numnodes ; j++ ) {
			if ( distance_metric[center][j] != i || !CNODEMASK_TSTB(cnodemask,j) )
				continue;
			candidate = j;

                        if (NODE_FREEMEM_REL(candidate) > 5
                            && !SKIPN(skipbv, candidate)
                            && CNODEMASK_TSTB(memfit->mem_unassigned, candidate)) {
                                CNODEMASK_CLRB(memfit->mem_unassigned, candidate);
                                chosen = candidate;
                                goto out;
                        }
                }

                /*
                 * We're done with the first loop, and we haven't
                 * found a reasonable node. Re-initialize  mem_unassigned and try again.
                 */
                CNODEMASK_SETALL(memfit->mem_unassigned);

        }

         /*
          * The search failed. We just place on the center node, even if it is
          * the skip node.
          */
        chosen = center;
        CNODEMASK_CLRB(memfit->mem_unassigned, chosen);

  out:
        ASSERT(chosen >= 0 && chosen < numnodes);
        mld_setnodeid(mld, chosen);
        mld_setplaced(mld);
        SUB_NODE_FUTURE_FREEMEM_ATOMIC(chosen, mld_getsize(mld));
}


int
physmem_mld_place_here(mld_t* mld)
{
        ASSERT(mld);
        if (mld_isplaced(mld)) {
                ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
        }        
        mld_setnodeid(mld, cnodeid());
        mld_setplaced(mld);
        SUB_NODE_FUTURE_FREEMEM_ATOMIC(cnodeid(), mld_getsize(mld));
        return (0);
}

void
physmem_mld_place_onnode(mld_t* mld, cnodeid_t node)
{
        ASSERT(mld);
        if (mld_isplaced(mld)) {
                ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
        }
        mld_setnodeid(mld, node);
        mld_setplaced(mld);
        SUB_NODE_FUTURE_FREEMEM_ATOMIC(node, mld_getsize(mld));
}

void
physmem_mld_weakmoveto(mld_t* mld, cnodeid_t node)
{
        ASSERT(mld);
        ASSERT(mld_isplaced(mld));

        if (mld_isweak(mld)) {
                physmem_mld_place_onnode(mld, node);
        }
}


/*
 * Find any availible imbed_dim hypercube of memories
 * with mem_ask[] memory on each node. 
 * Start searching from the point start_node.
 */

int
physmem_place_cube(int imbed_dim, 
		   cnodeid_t start_node, size_t *mem_ask, 
		   size_t*  mem_avail, pmolist_t* mldlist)
{
  	cnodeid_t center;
	int imbed_list_dim, comp_dim, imbed_size, comp_size;
	int i, j, k, l, x, y, z, foundmem;
	uchar_t *radial_list;
	uchar_t *imbed_list, *comp_imbed_list;	   
	center = cnode_to_cube[start_node];
	imbed_size = 1 << imbed_dim;
	comp_dim = log2_nodes - imbed_dim;
	comp_size = 1 << comp_dim;
	imbed_list_dim = binom[imbed_dim];
	radial_list = radii[log2_nodes-imbed_dim];
	imbed_list = imbed[imbed_dim];
	comp_imbed_list = imbed_list+imbed_dim;
	physmem_debug(1,"[physmem_place_cube]: cube_dim = %d .\n", imbed_dim);

	/* Here is an example:
	 *
	 * Consider the case of looking for all 2-cubes in a 5-cube.
	 *
	 * Each node z in a 5-cube can be represented by its
	 * 5 binary digits: z = (z4,z3,z2,z1,z0).
	 *
	 * The number of 2 cube imbeddings is 
	 * all possible pairs of dimensions chosen out of 0,1,2,3,4.
	 *
	 * They are:
	 * (0,1)(0,2)(0,3)(0,4)(1,2)(1,3)(1,4)(2,3)(2,4)(3,4)
	 * so there are 10 possible imbeddings.
	 *
	 * For each of the 10 imbeddings
	 * there are 2^3 = 8 values for the complementary dimensions
	 * x = (x2,x1,x0). We must also search over the 4 memories
	 * y = (y1,y0) of the 2 cube.
	 *
	 * If the imbedding is: (1,3) then the 
	 * complementary dimensions must be (0,2,4) so that
	 * z = (x2,y1,x1,y0,x0).
	 * The loop over the 8 values is done in a radial order
	 * 0,1,2,4,3,5,6,7 (increasing Hamming distance).
	 *
	 * The outer (j loop) loop below is over the coordinates 
	 * in the complementary space ordered by Hamming distance.
	 *
	 * The inner (i loop) loop below is over the different imbeddings.
	 *
	 * The first (k loop) accumulates the coordinates of the 
	 * complementary dimensions into x.
	 *
	 * The second k loop looks at the memories defined by y.
	 * z = x | y ;

	 */

	/*
	 * Radial loop in complementary space.
	 * A radial cutoff to restrict search can easily be added
	 * if needed by using a reduced comp_size.
	 */
	for (j=0 ; j<comp_size ; j++) {
		/* 
		 * Loop over all possible imbeddings.
		 */
		for (i=0 ; i<imbed_list_dim ; i++) { 
			/*
			 * start building up the node number
			 * coordinate in x starting with 
			 * the complement bits.
			 */
			for (k=0, x=0 ; k<comp_dim ; k++) 
				x |= ((radial_list[j]>>k)&1) << 
					comp_imbed_list[log2_nodes*i+k];
			/* 
			 * if foundmem gets zeroed, its time to try
			 * another imbedding.
			 */
			foundmem = 1;
			/*
			 * Loop over the coordinates in the
			 * imbedded cube and find out if the
			 * appropriate node has enough memory.
			 */
			for (k=0 ; k<imbed_size ; k++) {
				z = x;
				y = bin_to_gray(k);
				for (l=0 ; l<imbed_dim ; l++) 
					z |= ((y>>l)&1) << 
						imbed_list[log2_nodes*i+l];
				/*
				 * do we have the memory and is this a valid node
				 */
				if ((mem_avail[cube_to_cnode[z^center]] < mem_ask[k]) || 
				    (numnodes <= cube_to_cnode[z^center])) {
					foundmem = 0;
					break;
				}
			}
			/*
			 * If we get here and foundmem is still set
			 * we have found our imbedded cube.
			 */
			if (foundmem) 
				goto foundit;
		}
	}

	/*
 notfound:
         */
	return (PMOERR_NOMEM);

 foundit:
	physmem_debug(1,"Found it on nodes: [ %s","");
	/*
	 * loop over the coordinates of the found cube and
	 * set the nodeid's for each mld and
	 * update the appropriate mem_avail entry.
	 */

	for (k=0 ; k<imbed_size ; k++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldlist, k); 
		z = x;
		y = bin_to_gray(k);
		for (l=0 ; l<imbed_dim ; l++) 
			z |= ((y>>l)&1) << imbed_list[log2_nodes*i+l];
		physmem_debug(1,"%d ",z^center);
		z = cube_to_cnode[z^center];
                if (mld_isplaced(mld)) {
                        ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
                }
 		mld_setnodeid(mld, z);
                mld_setplaced(mld);
                SUB_NODE_FUTURE_FREEMEM_ATOMIC(z, mld_getsize(mld));
	}
	physmem_debug(1,"]\n%s","");
	return(0);

}

/*
 * Find a imbed_dim hypercube of memories
 * with affinity to the point start_node and
 * with mem_ask[] memory on each node. 
 */

int
physmem_place_cube_with_affinity(int imbed_dim,
				 cnodeid_t start_node, size_t *mem_ask,
				 size_t*  mem_avail, pmolist_t* mldlist)
{
  	cnodeid_t center;
	int imbed_list_dim, comp_dim, imbed_size, comp_size;
	int i, j, k, l, x, y, z, foundmem;
	uchar_t *radial_list;
	uchar_t *imbed_list, *comp_imbed_list;	   
	center = cnode_to_cube[start_node];
	imbed_size = 1 << imbed_dim;
	comp_dim = log2_nodes - imbed_dim;
	comp_size = 1 << comp_dim;
	imbed_list_dim = binom[imbed_dim];
	radial_list = radii[log2_nodes-imbed_dim];
	imbed_list = imbed[imbed_dim];
	comp_imbed_list = imbed_list+imbed_dim;
	physmem_debug(1,
		      "[physmem_place_cube_with_affinity]: cube_dim = %d .\n",
		      imbed_dim);
	/* Here is an example:
	 *
	 * Consider the case of looking for all 2-cubes in a 5-cube.
	 *
	 * Each node z in a 5-cube can be represented by its
	 * 5 binary digits: z = (z4,z3,z2,z1,z0).
	 *
	 * The number of 2 cube imbeddings is 
	 * all possible pairs of dimensions chosen out of 0,1,2,3,4.
	 *
	 * They are:
	 * (0,1)(0,2)(0,3)(0,4)(1,2)(1,3)(1,4)(2,3)(2,4)(3,4)
	 * so there are 10 possible imbeddings.
	 *
	 * For each of the 10 imbeddings
	 * there are 2^3 = 8 values for the complementary dimensions
	 * x = (x2,x1,x0). We must also search over the 4 memories
	 * y = (y1,y0) of the 2 cube.
	 *
	 * If the imbedding is: (1,3) then the 
	 * complementary dimensions must be (0,2,4) so that
	 * z = (x2,y1,x1,y0,x0).
	 * The loop over the 8 values is done in a radial order
	 * 0,1,2,4,3,5,6,7 (increasing Hamming distance).
	 *
	 * The outer (i loop) loop below is over the different imbeddings.
	 *
	 * The inner (j loop) loop below is over the coordinates 
	 * in the complementary space ordered by Hamming distance.
	 *
	 * The first (k loop) accumulates the coordinates of the 
	 * complementary dimensions into x.
	 *
	 * The second k loop looks at the memories defined by y.
	 * z = x | y ;

	 */

	/* 
	 * Loop over all possible imbeddings.
	 */
	for (i=0 ; i<imbed_list_dim ; i++) {
		/*
		 * Radial loop in complementary space.
		 * A radial cutoff to restrict search can easily be added
		 * if needed by using a reduced comp_size.
		 */
		for (j=0 ; j<comp_size ; j++) {
			/*
			 * start building up the node number
			 * coordinate in x starting with 
			 * the complement bits.
			 */ 
			for (k=0 ,x=0 ; k<comp_dim ; k++) 
				x |= ((radial_list[j]>>k)&1) <<
					comp_imbed_list[log2_nodes*i+k];
			/* 
			 * if foundmem gets zeroed, its time to try
			 * another imbedding.
			 */
			foundmem = 1;
			/*
			 * Loop over the coordinates in the
			 * imbedded cube and find out if the
			 * appropriate node has enough memory.
			 */
			for (k=0 ; k<imbed_size ; k++) {
				z = x;
				y = bin_to_gray(k);
				for (l=0 ; l<imbed_dim ; l++)
					z |= ((y>>l)&1) <<
						imbed_list[log2_nodes*i+l];
				/*
				 * do we have the memory and is this a valid node
				 */
				if ((mem_avail[cube_to_cnode[z^center]] < mem_ask[k]) ||
				    (numnodes <= cube_to_cnode[z^center])) {
					foundmem = 0;
					break;
				}
			}
			/*
			 * If we get here and foundmem is still set
			 * we have found our imbedded cube.
			 */
			if (foundmem)
				goto foundit;
		}
	}

	/*
 notfound:
	*/
	return (PMOERR_NOMEM);

 foundit:
	physmem_debug(1,"Found it on nodes : [ %s","");
	/*
	 * loop over the coordinates of the found cube and
	 * set the nodeid's for each mld and
	 * update the appropriate mem_avail entry.
	 */
	for (k=0 ; k<imbed_size ; k++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldlist, k);
		z = x;
		y = bin_to_gray(k);
		for (l=0 ; l<imbed_dim ; l++) 
			z |= ((y>>l)&1) << imbed_list[log2_nodes*i+l];
		physmem_debug(1,"%d ",z^center);
		z = cube_to_cnode[z^center];
                if (mld_isplaced(mld)) {
                        ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
                }
 		mld_setnodeid(mld, z);
                mld_setplaced(mld);
                SUB_NODE_FUTURE_FREEMEM_ATOMIC(z, mld_getsize(mld));
	}
	physmem_debug(1,"]\n%s","");
	return (0);
}

/*
 * Look for a specific physical memory configuration.
 *
 * If the memory is available the mldlist's cnodeid's are set to
 * the nodes requested by the cnodeid's from the rafflist.
 *
 */

/*ARGSUSED*/
int
physmem_place_physical(pmolist_t* mldlist,
                       int mldlist_len,
                       pmolist_t* rafflist,
                       int rafflist_len,
                       rqmode_t rqmode,
                       size_t* mem_ask)
{
	int i;
        int node;

        ASSERT(rafflist_len >= mldlist_len); rafflist_len=rafflist_len;
        ASSERT(mldlist);
        ASSERT(rafflist);

        /*
         * For physical placement requests, the rqmodes take
         * a slightly different meaning:
         * MANDATORY: Fail if not enough memory on these nodes
         * ADVISORY: Place on these nodes anyways.
         */
        if (rqmode == RQMODE_MANDATORY) {
                for ( i = 0; i < mldlist_len; i++) {
                        node = raff_getnodeid((raff_t *)
					pmolist_getelem(rafflist,i));
			/* if user ask mandatory, just check if there is 
			 * enough physical memory.
			 */
                        if (NODE_MIN_FUTURE_FREEMEM(node) < mem_ask[i]) {
                                return (PMOERR_NOMEM);
                        }
                }
	}
	
	for ( i = 0; i < mldlist_len; i++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldlist, i);
		node = raff_getnodeid((raff_t *)pmolist_getelem(rafflist,i));
                if (mld_isplaced(mld)) {
                        ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
                }
 		mld_setnodeid(mld, node);
                mld_setplaced(mld);
                SUB_NODE_FUTURE_FREEMEM_ATOMIC(node, mld_getsize(mld));
	}
	return (0);
}

/*
 * Select a good starting node for subsequent cluster placement.
 */
cnodeid_t
physmem_selectnode(cnodeid_t origin, int length, 
	pmolist_t* mldlist, size_t* mem_avail)
{
	int		mintdist, avg_mld_size;
	int		tdist, dist, len, i;
	int		spiral_in, maxtdist, avail_nodes;
	cnodeid_t	minstart, start, node;
	mld_t		*cmld;
	char		metric[MAX_NODES];


	/*
	 * The trivial case
	 */
	if (length <= 1)
		return origin;

	/*
	 * In order to minimize the search effort (and assuming that in
	 * many real life cases all specified mlds have the same size
	 * anyway) just calculate the average size requested by all mlds.
	 * This value will be used to determine if a given node has enough
	 * capacity available to host any of these mlds.
	 *
	 * Note that because of this simplification, the subsequent
	 * actual placement of the cluster could be different. Adding
	 * the full knowledge here would drastically inflate the length
	 * of the scan loops.
	 */
	avg_mld_size = 0;
	for (i = 0; i < length; i++) {
		cmld = (mld_t*)pmolist_getelem(mldlist, i);
		avg_mld_size += mld_getsize(cmld);
	}
	avg_mld_size = avg_mld_size / length;
	if (!avg_mld_size)
		avg_mld_size = 1;

	/*
	 * Determine the scan direction:
	 * - for smaller (< numnodes / 2) MLDSETs, do an outbound
	 *   radial search looking for nodes that make up the cluster
	 * - for larger (> numnodes / 2) MLDSETs, do an inbound
	 *   radial search looking for nodes that will not make up the cluster.
	 *
	 * Without this split, scan loops on either side of the spectrum
	 * would get rather long without enhancing actual placement.
	 */
	spiral_in = length * 2 > numnodes;

	/*
	 * Now scan all nodes to find the starting node that would lead
	 * to the most compact cluster allocation. Start the scan
	 * at the specified origin node.
	 */
	mintdist = physmem_max_radius * numnodes + 1;
	minstart = origin;
	for (i = 0; i < numnodes; i++) {

		start = (origin + i) % numnodes;

		/*
		 * Don't bother to go any further if this start node
		 * isn't eligible to participate in the cluster.
		 */
		if (mem_avail[start] < avg_mld_size)
			continue;

		/*
		 * Get distances for this start node and determine
		 * maximum total distance and loaded node count.
		 */
		maxtdist = 0;
		avail_nodes = 0;
		for (node = 0; node < numnodes; node++) {
			metric[node] = distance_metric[start][node];
			if (mem_avail[node] >= avg_mld_size) {

				maxtdist += metric[node];
				avail_nodes++;

			} else {

				/*
				 * Since this node does not have sufficient
				 * memory available, inflate the metric
				 * to make it inelligible for selection.
				 */
				metric[node] = physmem_max_radius + 1;
			}
		}

		/*
		 * Determine compactness of cluster placed at this start node
		 */
		if (spiral_in) {

			/*
			 * Spiral inwards looking for nodes that will
			 * not participate in cluster.
			 */
			tdist = maxtdist;
			len = avail_nodes - length;
			for (dist=physmem_max_radius; dist>0 && len>0; dist--)
			 for (node = numnodes-1; node >= 0 && len > 0; node--) {
				if (metric[node] != dist)
					continue;
				tdist -= dist;
				len--;
			}

		} else {

 			/*
 			 * Spiral outwards looking for nodes that will
			 * participate in the cluster.
 			 */
			tdist = 0;
			len = length - 1;
 			for (dist=1; dist<physmem_max_radius && len>0; dist++)
 			 for (node = 0; node < numnodes && len > 0; node++) {
 				if (metric[node] != dist)
 					continue;
 				tdist += dist;
 				len--;
 			}
		}

		/*
		 * If we haven't found enough nodes with enough memory,
		 * there is no need to continue; just return the 
		 * original origin.
		 */
		if (len)
			break;

		/*
		 * Check if current start node would provide a more
		 * compact cluster.
		 */
		if (tdist < mintdist) {
			mintdist = tdist;
			minstart = start;
		}
	}

	return minstart;
}

/*
 * Find a compact cluster of memory to place mldset with as many
 * mlds per node as the node has active cpus.
 * Start searching from the start node.
 * If space is not found, the remaining mld's are placed on either
 * used nodes or the start node.
 */
int
physmem_place_cpucluster(int length, pmolist_t* mldlist,
                      cnodeid_t start_node,
                      size_t*  mem_avail)
{
	int		cdist;	/* current distance to start node */
	cnodeid_t	cnode;	/* current node */
	mld_t		*cmld;	/* current mld */
	size_t		cmem;	/* current memory required */
	int		clen;	/* current length */

	size_t		avmem;	/* available memory on current node */
	int		avcpu;	/* available CPUs on current node */
	int		*index;	/* mld lookup table */
	cnodemask_t	mask;	/* used node mask */
	/* REFERENCED */
	cnodemask_t	oldmask;/* prev. node mask */

	int		i,j,k;	/* scratch */

	ASSERT(mldlist);
	CNODEMASK_CLRALL(mask);
	physmem_debug(1,"[physmem_place_cpucluster]: %d mld's; ",length);
	physmem_debug(1,"start_node=%d\n",start_node);

	/*
	 * Initialize mld lookup table
	 */
	clen = length;
	index = (int*)kmem_alloc(sizeof(int) * length, KM_SLEEP);
	ASSERT(index);
	for (i = 0; i < clen; i++)			index[i] = i;

	/*
	 * Radial node scan
	 */
	for (cdist = 0; cdist <= physmem_max_radius && clen > 0; cdist++)
	 for (cnode = 0; cnode < numnodes && clen > 0; cnode++) {

		if (distance_metric[start_node][cnode] != cdist)   continue;

		avmem = mem_avail[cnode];
		avcpu = CNODE_NUM_CPUS(cnode);

		physmem_debug(1,"[physmem_place_cpucluster]: cnode=%d; ",cnode);
		physmem_debug(1,"avmem=0x%x; ",avmem);
		physmem_debug(1,"avcpu=%d\n",avcpu);

		/*
		 * Scan mlds in lookup table
		 */
		i = 0;
		while (i < clen && avcpu > 0) {

			cmld = (mld_t*)pmolist_getelem(mldlist, index[i]);
			cmem = mld_getsize(cmld);
	
			if (cmem <= avmem) {

				/*
				 * Place this mld on this node
				 */
				if (mld_isplaced(cmld)) {
					ADD_NODE_FUTURE_FREEMEM_ATOMIC(
						mld_getnodeid(cmld), cmem);
				}
				mld_setnodeid(cmld, cnode);
				mld_setplaced(cmld);

				physmem_debug(1,
					"[physmem_place_cpucluster]: ", 0);
				physmem_debug(1,"mld 0x%x ",cmld);
				physmem_debug(1,"on node %d\n",cnode);

				CNODEMASK_SETB(mask, cnode);
				SUB_NODE_FUTURE_FREEMEM_ATOMIC(cnode, cmem);
				avcpu--;
				avmem -= cmem;

				/*
				 * Compress lookup table
				 */
				clen--;
				k = index[i];
				for (j = i; j < clen; j++)
					index[j] = index[j+1];
				index[clen] = k;
			} else {

				/*
				 * Advance to next mld in lookup table
				 */
				i++;
			}
		}
	}

	/*
	 * Place any remaining mlds
	 */
	if (clen > 0) {
		int	placed = length - clen;

		physmem_debug(1,
			"[physmem_place_cpucluster]: remaining clen=%d\n",clen);
		for (i = 0; i < clen; i++) {

			cmld = (mld_t*)pmolist_getelem(mldlist,index[i]);
			cmem = mld_getsize(cmld);
			cnode = placed ? mld_getnodeid(
					    (mld_t*)pmolist_getelem(
						mldlist,
						index[clen + (i % placed)]))
					: start_node;

			if (mld_isplaced(cmld)) {
				ADD_NODE_FUTURE_FREEMEM_ATOMIC(cnode, cmem);
			}
			mld_setnodeid(cmld, cnode);
			mld_setplaced(cmld);
			SUB_NODE_FUTURE_FREEMEM_ATOMIC(cnode, cmem);
		}
	}

	kmem_free(index, sizeof(int) * length);
	for (i=0; i<CNODEMASK_SIZE; i++) {
		physmem_debug(1,"[physmem_place_cpucluster]: mask[%d] = ", i);
		physmem_debug(1,"0x0%llx\n", CNODEMASK_WORD(mask,i));
	}
	CNODEMASK_ATOMSET_MASK(oldmask, __clustermask_rotor, mask);
	return 0;
}


/*
 * Find a compact cluster of memory
 * to place mldset.
 * Start searching from the point start_node.
 * If space is not found the remaining mld's are
 * placed on start_node.
 */
int
physmem_place_cluster(int length, pmolist_t* mldlist,
                      cnodeid_t start_node,
                      size_t*  mem_avail)
{
	int i, j, k, l, index[MAX_NODES];
	cnodeid_t trial_node;
	/* REFERENCED */
	cnodemask_t oldmask;
	cnodemask_t mask;
	size_t mem;
	int orig_length = length;

        ASSERT(mldlist);
        
        CNODEMASK_CLRALL(mask);
	physmem_debug(1,"[physmem_place_cluster]: %d mld's\n", length);
        
        
	for (i = 0; i < length; i++)
                index[i] = i;
	
        
	for (j = 0 ; j <= physmem_max_radius && length > 0; j++) for (l=0;l<numnodes && length > 0 ;l++){
			
		if( distance_metric[start_node][l] != j)
			continue;
		trial_node = l;
		mem = mem_avail[trial_node];
		for (i = 0; i < length; i++) {
                        mld_t* mld = (mld_t*)pmolist_getelem(mldlist, index[i]);
                        size_t mem_ask = mld_getsize(mld);
			physmem_debug(1, "[physmem_place_cluster]: size %x \n", mem_ask);
			physmem_debug(1, "[physmem_place_cluster]: node %d \n", l);
			physmem_debug(1, "[physmem_place_cluster]: avail %x \n", mem);
			if ( mem_ask <= mem ) {
				/* found it */
				int indexi = index[i];

                                if (mld_isplaced(mld)) {
                                        ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
                                }
 				mld_setnodeid(mld, trial_node);
                                mld_setplaced(mld);
				CNODEMASK_SETB(mask, trial_node);
                                SUB_NODE_FUTURE_FREEMEM_ATOMIC(trial_node,  mem_ask); 
				mem_avail[trial_node] -= mem_ask;
				/* eliminate the hole in the list */
				length--;
				for (k = i; k < length; k++) {
					index[k] = index[k+1];
				}
				index[length] = indexi;
				break;
			}
		}
	}
	
	if ( length > 0 ) {
		int placed = orig_length - length;
		/* if all nodes aren't yet placed, cycle leftovers onto already placed nodes or start node */
		for (i = 0; i < length; i++) {
                        mld_t* mld = (mld_t*)pmolist_getelem(mldlist, index[i]);
			
			if (mld_isplaced(mld)) {
                                ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
                        }
			
			if(!placed) 
				trial_node = start_node;
			else  
				trial_node = mld_getnodeid((mld_t*)pmolist_getelem(mldlist,
										    index[length + (i%placed)]));
			mld_setnodeid(mld, trial_node);
                        mld_setplaced(mld);
                        SUB_NODE_FUTURE_FREEMEM_ATOMIC(trial_node, mld_getsize(mld));
                }
	}

#ifdef DEBUG
	length = orig_length;
        for (i = 0; i < length; i++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldlist, i);
                if (!mld_isplaced(mld)) {
                        int j;
                        for (j = 0; j < length; j++) {
                                printf("Index[%d] = %d\n", j, index[j]);
                        }
                        printf("Length: %d\n", length);
                        printf("start_node: %d\n", start_node);
                        cmn_err_tag(105,CE_PANIC, "UNPLACED MLD[%d], nodeid=%d",
				    i, mld_getnodeid(mld));      
                }
        }
#endif
	physmem_debug(1, "[physmem_place_cluster]: 0x%x\n", mask);
        CNODEMASK_ATOMSET_MASK(oldmask, __clustermask_rotor, mask);
	return 0;

}

/*
 * Find any availible imbed_dim hypercube of memories
 * with mem_ask[] memory on each node. 
 * Start searching from the point start_node.
 */

int
physmem_place_cube_p(int imbed_dim, 
		   cnodeid_t start_node, size_t *mem_ask, 
		   size_t*  mem_avail, pmolist_t* mldlist, int *p)
{
  	cnodeid_t center;
	int imbed_list_dim, comp_dim, imbed_size, comp_size;
	int i, j, k, l, x, y, z, foundmem;
	uchar_t *radial_list;
	uchar_t *imbed_list, *comp_imbed_list;	   
	center = cnode_to_cube[start_node];
	imbed_size = 1 << imbed_dim;
	comp_dim = log2_nodes - imbed_dim;
	comp_size = 1 << comp_dim;
	imbed_list_dim = binom[imbed_dim];
	radial_list = radii[log2_nodes-imbed_dim];
	imbed_list = imbed[imbed_dim];
	comp_imbed_list = imbed_list+imbed_dim;

	/* Here is an example:
	 *
	 * Consider the case of looking for all 2-cubes in a 5-cube.
	 *
	 * Each node z in a 5-cube can be represented by its
	 * 5 binary digits: z = (z4,z3,z2,z1,z0).
	 *
	 * The number of 2 cube imbeddings is 
	 * all possible pairs of dimensions chosen out of 0,1,2,3,4.
	 *
	 * They are:
	 * (0,1)(0,2)(0,3)(0,4)(1,2)(1,3)(1,4)(2,3)(2,4)(3,4)
	 * so there are 10 possible imbeddings.
	 *
	 * For each of the 10 imbeddings
	 * there are 2^3 = 8 values for the complementary dimensions
	 * x = (x2,x1,x0). We must also search over the 4 memories
	 * y = (y1,y0) of the 2 cube.
	 *
	 * If the imbedding is: (1,3) then the 
	 * complementary dimensions must be (0,2,4) so that
	 * z = (x2,y1,x1,y0,x0).
	 * The loop over the 8 values is done in a radial order
	 * 0,1,2,4,3,5,6,7 (increasing Hamming distance).
	 *
	 * The outer (j loop) loop below is over the coordinates 
	 * in the complementary space ordered by Hamming distance.
	 *
	 * The inner (i loop) loop below is over the different imbeddings.
	 *
	 * The first (k loop) accumulates the coordinates of the 
	 * complementary dimensions into x.
	 *
	 * The second k loop looks at the memories defined by y.
	 * z = x | y ;

	 */

	/*
	 * Radial loop in complementary space.
	 * A radial cutoff to restrict search can easily be added
	 * if needed by using a reduced comp_size.
	 */
	for (j=0 ; j<comp_size ; j++) {
		/* 
		 * Loop over all possible imbeddings.
		 */
		for (i=0 ; i<imbed_list_dim ; i++) { 
			/*
			 * start building up the node number
			 * coordinate in x starting with 
			 * the complement bits.
			 */
			for (k=0, x=0 ; k<comp_dim ; k++) 
				x |= ((radial_list[j]>>k)&1) << 
					comp_imbed_list[log2_nodes*i+k];
			/* 
			 * if foundmem gets zeroed, its time to try
			 * another imbedding.
			 */
			foundmem = 1;
			/*
			 * Loop over the coordinates in the
			 * imbedded cube and find out if the
			 * appropriate node has enough memory.
			 */
			for (k=0 ; k<imbed_size ; k++) {
				z = x;
				y = bin_to_gray(k);
				for (l=0 ; l<imbed_dim ; l++) 
					z |= ((y>>l)&1) <<
						imbed_list[log2_nodes*i+l];
				/*
				 * do we have the memory and is this a valid node
				 */
				if ((mem_avail[cube_to_cnode[z^center]] < mem_ask[k]) ||
				    (numnodes <= cube_to_cnode[z^center])) {
					foundmem = 0;
					break;
				}
			}
			/*
			 * If we get here and foundmem is still set
			 * we have found our imbedded cube.
			 */
			if (foundmem) 
				goto foundit;
		}
	}

	/*
 notfound:
         */
	physmem_debug(1,".%s","");
	return (PMOERR_NOMEM);

 foundit:
	physmem_debug(1,"Found it on nodes: [ %s","");
	/*
	 * loop over the coordinates of the found cube and
	 * set the nodeid's for each mld and
	 * update the appropriate mem_avail entry.
	 */

	for (k=0 ; k<imbed_size ; k++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldlist, p[k]);
		z = x;
		y = bin_to_gray(k);
		for (l=0 ; l<imbed_dim ; l++) 
			z |= ((y>>l)&1) << imbed_list[log2_nodes*i+l];
		physmem_debug(1,"%d",z^center);
		physmem_debug(1,"(%d) ",p[k]);
		z = cube_to_cnode[z^center];
                if (mld_isplaced(mld)) {
                        ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
                }
 		mld_setnodeid(mld, z);
                mld_setplaced(mld);
                SUB_NODE_FUTURE_FREEMEM_ATOMIC(z, mld_getsize(mld));
	}
	physmem_debug(1,"]\n%s","");
	return (0);

}

/*
 * Find a imbed_dim hypercube of memories
 * with affinity to the point start_node and
 * with mem_ask[] memory on each node. 
 */

int
physmem_place_cube_with_affinity_p(int imbed_dim,
				 cnodeid_t start_node, size_t *mem_ask,
				 size_t*  mem_avail, pmolist_t* mldlist, int *p)

{
  	cnodeid_t center;
	int imbed_list_dim, comp_dim, imbed_size, comp_size;
	int i, j, k, l, x, y, z, foundmem;
	uchar_t *radial_list;
	uchar_t *imbed_list, *comp_imbed_list;	   
	center = cnode_to_cube[start_node];
	imbed_size = 1 << imbed_dim;
	comp_dim = log2_nodes - imbed_dim;
	comp_size = 1 << comp_dim;
	imbed_list_dim = binom[imbed_dim];
	radial_list = radii[log2_nodes-imbed_dim];
	imbed_list = imbed[imbed_dim];
	comp_imbed_list = imbed_list+imbed_dim;

	/* 
	 * Here is an example:
	 *
	 * Consider the case of looking for all 2-cubes in a 5-cube.
	 *
	 * Each node z in a 5-cube can be represented by its
	 * 5 binary digits: z = (z4,z3,z2,z1,z0).
	 *
	 * The number of 2 cube imbeddings is 
	 * all possible pairs of dimensions chosen out of 0,1,2,3,4.
	 *
	 * They are:
	 * (0,1)(0,2)(0,3)(0,4)(1,2)(1,3)(1,4)(2,3)(2,4)(3,4)
	 * so there are 10 possible imbeddings.
	 *
	 * For each of the 10 imbeddings
	 * there are 2^3 = 8 values for the complementary dimensions
	 * x = (x2,x1,x0). We must also search over the 4 memories
	 * y = (y1,y0) of the 2 cube.
	 *
	 * If the imbedding is: (1,3) then the 
	 * complementary dimensions must be (0,2,4) so that
	 * z = (x2,y1,x1,y0,x0).
	 * The loop over the 8 values is done in a radial order
	 * 0,1,2,4,3,5,6,7 (increasing Hamming distance).
	 *
	 * The outer (i loop) loop below is over the different imbeddings.
	 *
	 * The inner (j loop) loop below is over the coordinates 
	 * in the complementary space ordered by Hamming distance.
	 *
	 * The first (k loop) accumulates the coordinates of the 
	 * complementary dimensions into x.
	 *
	 * The second k loop looks at the memories defined by y.
	 * z = x | y ;
	 */

	/* 
	 * Loop over all possible imbeddings.
	 */
	for (i=0 ; i<imbed_list_dim ; i++) {
		/*
		 * Radial loop in complementary space.
		 * A radial cutoff to restrict search can easily be added
		 * if needed by using a reduced comp_size.
		 */
		for (j=0 ; j<comp_size ; j++) {
			/*
			 * start building up the node number
			 * coordinate in x starting with 
			 * the complement bits.
			 */ 
			for (k=0 ,x=0 ; k<comp_dim ; k++) 
				x |= ((radial_list[j]>>k)&1) <<
					comp_imbed_list[log2_nodes*i+k];
			/* 
			 * if foundmem gets zeroed, its time to try
			 * another imbedding.
			 */
			foundmem = 1;
			/*
			 * Loop over the coordinates in the
			 * imbedded cube and find out if the
			 * appropriate node has enough memory.
			 */
			for (k=0 ; k<imbed_size ; k++) {
				z = x;
				y = bin_to_gray(k);
				for (l=0 ; l<imbed_dim ; l++)
					z |= ((y>>l)&1) <<
						imbed_list[log2_nodes*i+l];
				/*
				 * do we have the memory and is this a valid node
				 */
				if ((mem_avail[cube_to_cnode[z^center]] < mem_ask[k]) ||
				    (numnodes <= cube_to_cnode[z^center])) {
					foundmem = 0;
					break;
				}
			}
			/*
			 * If we get here and foundmem is still set
			 * we have found our imbedded cube.
			 */
			if (foundmem)
				goto foundit;
		}
	}

	/*
 notfound:
	*/
	physmem_debug(1,".%s","");
	return (PMOERR_NOMEM);

 foundit:
	physmem_debug(1,"Found it on nodes : [ %s","");
	/*
	 * loop over the coordinates of the found cube and
	 * set the nodeid's for each mld and
	 * update the appropriate mem_avail entry.
	 */
	for (k=0 ; k<imbed_size ; k++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldlist, p[k]);
		z = x;
		y = bin_to_gray(k);
		for (l=0 ; l<imbed_dim ; l++) 
			z |= ((y>>l)&1) << imbed_list[log2_nodes*i+l];
		physmem_debug(1,"%d",z^center);
		physmem_debug(1,"(%d) ",p[k]);
		z = cube_to_cnode[z^center];
                if (mld_isplaced(mld)) {
                        ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld_getnodeid(mld), mld_getsize(mld));
                }
		mld_setnodeid(mld, z);
                mld_setplaced(mld);
                SUB_NODE_FUTURE_FREEMEM_ATOMIC(z, mld_getsize(mld));
	}
	physmem_debug(1,"]\n%s","");
	return (0);
}

/*
 * static uchar_t distance_metric[MAX_NODES][MAX_NODES];
 */
/*	(see note at bottom)
 * This numnodes by numnodes distance metric
 * whose integer values in tell you how many
 * edges need to be crossed to get from node
 * i to node j. If distance_metric[i][j] == k > 0
 * then k links/wires and k-1 routers need to be
 * traversed to get from i to j. 
 * distance_metric[i][j] = distance_metric[j][i]
 * and distance_metric[i][i] == 0.
 * 
 * Examples:
 * 	2 nodes + null router
 *	[0 1]
 *	[1 0]
 *
 *	4 nodes + star router
 *	[0 2 2 2]
 *	[2 0 2 2]
 *	[2 2 0 2]
 *	[2 2 2 0]
 *
 *	8 nodes + 4 routers in a square
 *	[0 2 3 3 3 3 4 4]
 *	[2 0 3 3 3 3 4 4]
 *	[3 3 0 2 4 4 3 3]
 *	[3 3 2 0 4 4 3 3]
 *	[3 3 4 4 0 2 3 3]
 *	[3 3 4 4 2 0 3 3]
 *	[4 4 3 3 3 3 0 2]
 *	[4 4 3 3 3 3 2 0]
 *
 *	8 nodes + 4 routers in a square + xpress links
 *	[0 2 3 3 3 3 3 3]
 *	[2 0 3 3 3 3 3 3]
 *	[3 3 0 2 3 3 3 3]
 *	[3 3 2 0 3 3 3 3]
 *	[3 3 3 3 0 2 3 3]
 *	[3 3 3 3 2 0 3 3]
 *	[3 3 3 3 3 3 0 2]
 *	[3 3 3 3 3 3 2 0]
 *
 *	(this is the note)
 * Internally, this array will get "renormalized" to the number of
 * router hops except for the case of a null router. 
 */

/*
 * Scan the neighboring nodes within the specified radius
 * and apply the selector function. If the selector function
 * returns non-zero, set the `neighbor' out-parameter to the
 * candidate being tested, and return immediately.
 */

void *
physmem_select_neighbor_node(cnodeid_t center,
                             int radius,
                             cnodeid_t* neighbor,
                             selector_t selector,
                             void* arg1,
                             void* arg2)
{
	int i,j;
        cnodeid_t candidate;
	void	*retval;
	cnodeid_t dummy;
	
	if (!neighbor)
		neighbor = &dummy;

        ASSERT(center >= 0 && center < numnodes);
        ASSERT(radius >= 0 && radius <= physmem_max_radius);
        ASSERT(neighbor);
        ASSERT(selector);
                
        /*
         * Search in the hardware in radial order.
	 */
	for ( i = 1 ; i <= radius ; i++ ) for ( j=0 ; j < numnodes ; j++ ) {
		if ( distance_metric[center][j] != i )
			continue;
		candidate = j;
		if (retval = (*selector)(candidate, arg1, arg2)) {
                        *neighbor = candidate;
                        return (retval);
                }
	}

        return (NULL);
}

void *
physmem_select_masked_neighbor_node(cnodeid_t center,
                             int radius,
			     cnodemask_t cnodemask,
                             cnodeid_t* neighbor,
                             selector_t selector,
                             void* arg1,
                             void* arg2)
{
	int i,j;
        cnodeid_t candidate;
	void	*retval;
	cnodeid_t dummy;

	
	if (!neighbor)
		neighbor = &dummy;

        ASSERT(center >= 0 && center < numnodes);
        ASSERT(radius >= 0 && radius <= physmem_max_radius);
        ASSERT(neighbor);
        ASSERT(selector);
                
        /*
         * Search in the cube we are imbedded in radial order.
	 */
	for ( i = 1 ; i <= radius ; i++ ) for ( j=0 ; j < numnodes ; j++ ) {
		if ( distance_metric[center][j] != i || !CNODEMASK_TSTB(cnodemask,j) )
			continue;
		candidate = j;
		if (retval = (*selector)(candidate, arg1, arg2)) {
                        *neighbor = candidate;
                        return (retval);
                }
	}

        return (NULL);
}

#ifdef SN0
/*
 * get_shotest_paths finds the shortest paths from the
 * source node to all other nodes and fills a row of
 * the distance metric: distance_metric[node][].
 * This is a depth-first search which terminates at
 * vertices which are nodes.
 */

static void
get_shortest_paths_from_node(cnodeid_t node)
{
	int i,j, chase_list_length;
	cnodeid_t trail_node;
	graph_edge_place_t place;
	/* REFERENCED */
	graph_error_t rv;
	vertex_hdl_t target_vertex, *chase_list;
	uchar_t dist[2*MAX_NODES];
	char buffer[LABEL_LENGTH_MAX];


	/*
	 * Go out the "link" edge from source node.
	 */

	rv = hwgraph_edge_get( cnodeid_to_vertex(node), 
				      EDGE_LBL_INTERCONNECT, &target_vertex);
	(void)hwgraph_vertex_unref(target_vertex);

	if (is_specified(arg_maxnodes) && rv != GRAPH_SUCCESS) {
		/*
		 * hwgraph may not be correct if running with truncated node list.
		 */
		for (i=0;i<numnodes;i++) distance_metric[node][i] = 12;
		return;
	}
	ASSERT( rv == GRAPH_SUCCESS ); /* better have a link!! */

	chase_list_length = 1;
	chase_list = kmem_alloc(2*MAX_NODES*sizeof(vertex_hdl_t), KM_NOSLEEP);
	chase_list[0] = target_vertex;
	dist[0] = 1;

	/* create list of all nodes and routers */
	for (i=0;i<chase_list_length;i++){
		if ( CNODEID_NONE == nodevertex_to_cnodeid(chase_list[i])){
			/*
			 * If we aren't a node expand out all edges.
			 */

			place = EDGE_PLACE_WANT_REAL_EDGES;
			while( GRAPH_HIT_LIMIT != hwgraph_edge_get_next(chase_list[i],
									buffer,
									&target_vertex,
									&place) ){
				int found = 0;
				
				(void)hwgraph_vertex_unref(target_vertex);
			
				/*
				 * Add it to the list
				 * if its not already there.
				 */

				for (j=0;j<chase_list_length;j++){
					if ( target_vertex == chase_list[j] ){
						found = 1;
						break;
					}
				}
				if ( found == 0 ){
					chase_list[chase_list_length] = target_vertex;
					dist[chase_list_length] = dist[i] + 1;
					chase_list_length++;
						
				}
			}
		}
	}
#ifdef DEBUG
	if ( node == 0 )
		cmn_err(CE_DEBUG, "[topology] There are %d routers.\n", 
		       max(0,chase_list_length - numnodes) );
#endif
	/* 
	 * Set to all distances to MAX_DIST for sanity check 
	 */
	for (i=0;i<numnodes;i++) distance_metric[node][i] = MAX_DIST;

	for (i=0;i<chase_list_length;i++){
		trail_node = nodevertex_to_cnodeid(chase_list[i]);
		if (  CNODEID_NONE != trail_node ){
			ASSERT(trail_node >= 0 &&  trail_node < numnodes );
			distance_metric[node][trail_node] = dist[i];
		}
	}

	kmem_free(chase_list, 2*MAX_NODES*sizeof(vertex_hdl_t));

	distance_metric[node][node] = 0;
	/*
	 * Sanity check: all nodes *should* have been visited. Note that
	 * if using a subset of nodes, this check may fail. Just set the
	 * value to a large legal value.
	 */
	for (i=0;i<numnodes;i++) 
		if (is_specified(arg_maxnodes))
			if( distance_metric[node][i] == MAX_DIST )
				distance_metric[node][i] = 12;
		else
			ASSERT(distance_metric[node][i] != MAX_DIST);
	
}
#endif /*SN0*/

/* 
 * topology_init() is only called at boot time from NUMA_INIT()
 * which is called from main. topology_init() figures out the
 * appropriate hypercube to imbed the system in.
 */

int
topology_init(void)
{
	static int topology_init_done = 0;
#ifdef SN0
#define MAX_SN0_MODULES 64
	int i, j, k, module, slot, found, index, cslot, cmodule;
	uint_t slot_list[NODESLOTS_PER_MODULE], module_list[MAX_SN0_MODULES];
	int slots_found = 0, modules_found = 0;
	int log2_slots, log2_modules;
	uchar_t min_dist,max_dist;
#endif
	if (topology_init_done ||  numnodes < 2 )
		return 0;

#ifdef SN0

	for (i=0;i<numnodes;i++){

		/* 
		 * keep track of modules & slots in sorted lists 
		 */

		slot =  SLOTNUM_GETSLOT(NODEPDA(i)->slotdesc);
 		found = 0;
		for (j=0;j<slots_found;j++){
			if ( slot == slot_list[j]){
				found = 1;
				break;
			}
		}
		if ( !found ){
			for (j=0;j<slots_found;j++) 
				if ( slot < slot_list[j]) break;
			slots_found++;
			for (k = slots_found-1 ; k > j ; k--) slot_list[k] = slot_list[k-1];
			slot_list[j] = slot;
		}

		module = NODEPDA(i)->module_id ;
		found = 0;
		for (j=0;j<modules_found;j++){
			if ( module == module_list[j]){
				found = 1;
				break;
			}
		}
		if ( !found ){
			for (j=0;j<modules_found;j++) 
				if ( module < module_list[j]) break;
			modules_found++;
			for (k = modules_found-1 ; k > j ; k--) module_list[k] = module_list[k-1];
			module_list[j] = module;
		}

		get_shortest_paths_from_node( i );   
		
	}

#ifdef DEBUG
	cmn_err(CE_DEBUG, 
		"[topology] Found %d modules and %d types of node slots.\n",
		modules_found, slots_found);
	cmn_err(CE_DEBUG, "[topology] cnode distance metric:\n");

	for (i=0;i<numnodes;i++){
		char	 buf[130], str[8];

		strcpy(buf, "");
		for (j=0;j<numnodes;j++){
			sprintf(str, " %d", distance_metric[i][j]);
			if (strlen(buf)+strlen(str) > sizeof(buf)-5) {
				strcat(buf, " ...");
				break;
			}
			strcat(buf, str);
		}
		cmn_err(CE_DEBUG, "[topology] [%s ]\n", buf);
	}
#endif /* DEBUG */

	min_dist = MAX_DIST;
	max_dist = 0;

	for (i=0;i<numnodes-1;i++){
		for (j=i+1;j<numnodes;j++){
			min_dist = min(distance_metric[i][j] , min_dist );
			max_dist = max(distance_metric[i][j] , max_dist );
		}
	}

	/* convert from # of graph edges to router hops */

	if ( min_dist > 1 ){
		for (i=0;i<numnodes-1;i++){
			for (j=i+1;j<numnodes;j++){
				if ( distance_metric[i][j] ){
					distance_metric[i][j]--;
					distance_metric[j][i]--;
				}
			}
		}
		min_dist--;
		max_dist--;
#ifdef DEBUG
		cmn_err(CE_DEBUG,
			"[topology] Maximal number of router traversals is %d.\n",
			max_dist);
		cmn_err(CE_DEBUG,
			"[topology] Minimal number of router traversals is %d.\n",
			min_dist);
#endif
	}
	physmem_max_radius = max_dist;

	/*
	 * We imbed into a boolean cube of dimension:
	 * ceil_log2(modules_found) + ceil_log2(slots_found)
	 * this *should* be the same as log2_nodes. If this
	 * is a weird system we will correct the number of modules.
	 */

	log2_slots = ceil_log2(slots_found);
	log2_modules = ceil_log2(modules_found);

	if ( log2_nodes != ( log2_slots + log2_modules ) ){
#ifdef DEBUG
		cmn_err(CE_DEBUG, "[topology] log2_nodes = %d ",log2_nodes);
		cmn_err(CE_DEBUG, "[topology] log2_slots = %d , log2_modules = %d",
		       log2_slots,log2_modules);
#endif /*DEBUG*/
		
		log2_modules = log2_nodes - log2_slots;
#ifdef DEBUG
		cmn_err(CE_DEBUG, "[topology] resetting log2_modules to %d",log2_modules);
#endif /*DEBUG*/
	}



	for (cslot=0;cslot < (1<<log2_slots) ; cslot++){
		for (cmodule=0;cmodule < (1<<log2_modules) ; cmodule++ ){
			index = (cmodule << log2_slots) | cslot ;
			cube_to_cnode[bin_to_gray(index)] = index;
			cnode_to_cube[index] = bin_to_gray(index);
		}
	}

	for (i=0;i<numnodes;i++){

		module = NODEPDA(i)->module_id;
		slot = SLOTNUM_GETSLOT(NODEPDA(i)->slotdesc);

		for (cslot = 0; cslot < slots_found; cslot++)
			if ( slot == slot_list[cslot] ) break;
		ASSERT( cslot < slots_found );
		for (cmodule = 0; cmodule < modules_found; cmodule++)
			if ( module == module_list[cmodule] ) break;
		ASSERT( cmodule < modules_found );

		index = bin_to_gray( (cmodule << log2_slots) | cslot );

		cube_to_cnode[index] = (cnodeid_t)i;
		cnode_to_cube[i] = index;
#ifdef MEMDEBUG
		cmn_err(CE_DEBUG, "[topology] cube_to_cnode[%2d] = %2d\n", index, i );
		cmn_err(CE_DEBUG, "[topology] cnode_to_cube[%2d] = %2d\n", i,  index );
#endif /*DEBUG*/
	}

	   
#endif /* SN0 */

	return 0;
}

/*
 * Copy the distance matrix into an
 * array packed numnodes x numnodes.
 */
void
copy_distmatrix(uchar_t* dmat)
{
	int n;
	for (n = 0; n < numnodes; n++)
		bcopy(&distance_metric[n][0], &dmat[n*numnodes],
		    numnodes*sizeof (uchar_t));
}

/* 
 * memsched_init() is only called at boot time.
 * the number of memory nodes is examined for the purpose of
 * setting pointers into appropriate tables.
 */

int
memsched_init(void)
{
	int i;
	static int memsched_init_done = 0;

	if (memsched_init_done)
		return 0;

	log2_nodes = ceil_log2( numnodes );
	/* Temp value till it gets set later properly */
        physmem_max_radius = log2_nodes;

	if( physmem_max_radius )
		physmem_max_radius--;

#if DEBUG
	cmn_err(CE_NOTE, 
		"[memsched_init]: There are %d <=  2^%d memory nodes.",
		numnodes,log2_nodes);
#endif /* DEBUG */

	/* 
	 *  Initialize appropriate table pointers.
	 */
	imbed = imbeds[log2_nodes];
	binom = binoms[log2_nodes];

	/*
	 * Initialize future freemem to node freemem and
	 * cubeid<->cnodeid mappings.
	 */

	for (i=0;i<numnodes;i++){
		SET_NODE_FUTURE_FREEMEM_ATOMIC(i,NODE_FREEMEM(i));
		/* for now... till we get it from sw */
		cube_to_cnode[bin_to_gray(i)] = (cnodeid_t)i;
		cnode_to_cube[i] = (cnodeid_t)bin_to_gray(i);
	}
	for (i = numnodes ; i < (1<<log2_nodes) ; i++ ){
		cube_to_cnode[bin_to_gray(i)] = (cnodeid_t)i;
		cnode_to_cube[i] = (cnodeid_t)bin_to_gray(i);
	}
	/* startup distance metric */
	for (i=0;i<numnodes;i++){
		int j;
		for (j=0;j<numnodes;j++){
			int sum = 0, val = cnode_to_cube[i] ^ cnode_to_cube[j];
			while(val){
				sum += val & 1;
				val >>= 1;
			}
			distance_metric[i][j] = sum;
		}
	}

	memsched_init_done = 1;
	return 0;
}

/* 
 * The smallest integer k, such that n <= 2^k .
 */

int 
ceil_log2(int n)
{
	int count = 0;
	n--;
	while(n>0) {
		count++; 
		n = n>>1; 
	}
	return count;
}

/*
 * Find the distance(in terms of hops) between two nodes
 */


int
node_distance(cnodeid_t n0, cnodeid_t n1)
{
	return( (int)distance_metric[n0][n1] );
}

/*
 * Return the maxradius for the system
 */

int
physmem_maxradius(void)
{
	return physmem_max_radius;
}
 
static cnodemask_t
get_loadmask(int len)
{
	cnodemask_t	mask;
	cnodemask_t	nmask;
	cnodeid_t	node;
	int		nlive;

	nlive = 0;
	mask = __clustermask_rotor;
	CNODEMASK_CLRALL(nmask);
	for (node = 0; node < numnodes; node++) {
		if (CNODEMASK_TSTB(mask, node))
			continue;

		CNODEMASK_SETB(nmask, node);
		nlive++;
	}
	if (nlive < len)
		CNODEMASK_CLRALL(__clustermask_rotor);

	return nmask;
}

void
set_loadmask(cnodeid_t node)
{
	CNODEMASK_CLRB(__clustermask_rotor, node);
}

/*
 * The base for finding a cluster of CPUs or a cluster of nodes.
 * if ncpu >0 then find cpu cluster, otherwise find node cluster.
 */
static cnodemask_t physmem_find_cluster(cpumask_t cpumask, 
				cnodemask_t nodemask, int ncpus, int nnodes)
{
	int i, node = 0, radius, start_node = 0;
	int cpu,  node_count, tnode, cpu_count;
	int neighbor_count, max_neighbor_count = 0;
	cnodemask_t out_mask = CNODEMASK_ZERO();

	/* 
	 *   find the node with the largest number
	 *   of neighbors with radius <=2 to use as the seed node.
	 */
	for (node = 0; node < numnodes; node++){
		if ( CNODEMASK_TSTB( nodemask, node) ){
			neighbor_count = 0;
			for ( i = 0 ; i<numnodes; i++ ){
				if( distance_metric[node][i] <=2 && 
						CNODEMASK_TSTB( nodemask, i ))
					neighbor_count++;
			}
			if( neighbor_count >= max_neighbor_count ){
				start_node = node;
				max_neighbor_count = neighbor_count;
			}
		}
	}
	
       	tnode = memfit_selectnode(nodemask); 
	if (tnode != CNODEID_NONE) {
		for (i = 0 ; i < numnodes; i++) 
			if (distance_metric[tnode][i] <=2 && 
				CNODEMASK_TSTB (nodemask, i))
				neighbor_count++;

		if (neighbor_count >= max_neighbor_count)
			start_node = tnode;
	}
	
	ASSERT( start_node >= 0 && start_node < numnodes );
	
	cpu_count = 0;
	node_count = 0;	
	CNODEMASK_SETB(out_mask, start_node);
	physmem_debug(1,"[find node]: start_node %d \n", start_node);
	physmem_debug(1,"[find node]: tnode %d \n", tnode);
	for (radius = 0 ; radius <= physmem_max_radius ; radius++) {
		for (node=0; node<numnodes ; node++){
			if( distance_metric[start_node][node] != radius 
				|| !CNODEMASK_TSTB( nodemask, node) )
				continue;
			CNODEMASK_SETB(out_mask, node);
			if(ncpus > 0) { /* find CPU clusters */
				cpu = CNODE_TO_CPU_BASE(node);
				for(i=cpu; i<cpu+CPUS_PER_NODE; i++) {
					if(!CPUMASK_TSTB(cpumask, i)) {
						if(++cpu_count == ncpus) 
							return out_mask;
					}
				}
			} else { /* find node clusters */
				if( ++node_count == nnodes) 
				return out_mask;
				
			}
		}
	}
	return out_mask;
}

/*
 * Find a compact cluster of cpus
 * Find n cpus in a cluster from cpus NOT used by cpumask
 * Return a cnodemask_t of the used nodes.
 */

cnodemask_t
physmem_find_cpu_cluster(cpumask_t cpumask, int ncpus)
{
	int i, cpu, node;
	cnodemask_t nodemask = CNODEMASK_ZERO();
        
	ASSERT( ncpus > 0 && ncpus <= maxcpus);

	/* find usable nodes from cpumask */
	for (node = 0; node < numnodes; node++){
		cpu = CNODE_TO_CPU_BASE(node);
		for(i =cpu; i<cpu+CPUS_PER_NODE; i++) {
			if(!CPUMASK_TSTB(cpumask, i)) {
				/* count this node */
				CNODEMASK_SETB(nodemask, node);
				break;
			}
		}
	}
	ASSERT(CNODEMASK_IS_NONZERO(nodemask));
	return physmem_find_cluster(cpumask, nodemask, ncpus, -1);
}


/*
 * Find a compact cluster of nodes.
 * Find n nodes in a cluster from the nodes defined by nodemask.
 * Return a cnodemask_t of the found nodes.
 */

cnodemask_t
physmem_find_node_cluster(cnodemask_t nodemask , int nnodes)
{
	cpumask_t cpumask;

	ASSERT( nnodes > 0 && nnodes <= numnodes );
	ASSERT(CNODEMASK_IS_NONZERO(nodemask));

	CPUMASK_CLRALL(cpumask);
	return physmem_find_cluster(cpumask, nodemask, -1, nnodes);
}
