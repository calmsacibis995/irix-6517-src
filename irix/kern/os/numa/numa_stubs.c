#include <sys/bsd_types.h>
#include <sys/types.h>
#include <sys/cpumask.h>
#include <sys/nodepda.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/lpage.h>
#include <sys/numa.h>
#include <sys/systm.h>
#include "pfms.h"


/*
 * Stubs for numa_hw.c
 */
/*ARGSUSED*/
void
migr_threshold_adjust(cnodeid_t my_nodeid, int flag, int step)
{;}

/*ARGSUSED*/
void
migr_threshold_diff_set_lock(cnodeid_t node, int val)
{;}

/*ARGSUSED*/
void
migr_refcnt_disable(pfn_t swpfn, pfmsv_t pfmsv)
{;}

/*ARGSUSED*/
void
migr_refcnt_update_counters(cnodeid_t home_node,
                            pfn_t swpfn,
                            pfmsv_t pfmsv,
                            uint new_mode)
{;}

/*
 * Neighbor search stub routine for NON-NUMA
 * machines. Assume 1 cpu per node.
 */
/*ARGSUSED*/
void*
physmem_select_neighbor_node(cnodeid_t center,
                             int radius,
                             cnodeid_t* neighbor,
                             selector_t selector,
                             void* arg1,
                             void* arg2)
{
        void* retval;
        cnodeid_t candidate;
        
        for (candidate = 0; candidate < numcpus; candidate++) {
		if (retval = (*selector)(candidate, arg1, arg2)) {
                        *neighbor = candidate;
                        return (retval);
                }
		
	}

        return (NULL);
}
