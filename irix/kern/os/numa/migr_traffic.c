/*****************************************************************************
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
#ifdef SN0

/*
 * NUMA migration traffic control module 
 */

#include <sys/types.h>
#include <sys/cpumask.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include "pfms.h"
#include "migr_control.h"
#include "numa_hw.h"
#include "migr_manager.h"
#include "migr_queue.h"
#include "numa_traffic.h"
#include "migr_traffic.h"
#include "numa_stats.h"

/*
 * Different steps used to increase/decrease the migration threshold 
 * in one node
 * XXX highly tunable, the basic rule is:
 * -- use bigger steps to increase migration threshold when traffic is high
 * -- use smaller steps to decrease migration threshold when traffic is low
 * -- use bigger steps to adjust migration threshold if overall traffic is
 *    gathered
 * -- use smaller steps to adjust migration threshold if traffic along a 
 *    a particular migration route is gathered
 * ALL STEP VALUES ARE DELTAS APPLIED TO THE RELATIVE [0.100] THRESHOLD VALUES
 */
#define MIGR_STEP_BIG      10
#define MIGR_STEP_MEDIUM    7
#define MIGR_STEP_SMALL     5
#define MIGR_STEP_TINY      2

/*
 * -- Run on each node
 * -- Adjust migration threshold according to neighborhood bandwidth usage
 * -- Migrate a number of pages if neighborhood bandwidth is low 
 * Called by: migr_periodic_op()
 * Parameters: current node id
 * Return: none
 * Side Effects: periodic traffic control is done
 */ 
/*ARGSUSED*/
migr_traffic_state_t 
migr_periodic_traffic_control(cnodeid_t source_nodeid)
{
#if 0        
	int load;

	ASSERT(source_nodeid == cnodeid());

	load = local_neighborhood_traffic_overall(source_nodeid);
        
        NUMA_STAT_SAMPLE(source_nodeid, traffic_local_router, load);

	if (load > NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_traffic_control_threshold) { 
		/* 
		 * The traffic is high out there, increase the threshold 
		 */
		migr_threshold_adjust(source_nodeid, INCREASE, MIGR_STEP_BIG);

		return(OVERLOADED);
	}
	else { 

		/* 
		 * The traffic is low right now, decrease the threshold and
		 * do several queued migrations
		 * we need decrease slowly to avoid flood of interrupts 
		 */
		migr_threshold_adjust(source_nodeid, DECREASE, MIGR_STEP_MEDIUM);

		return(LIGHTLOADED);
	}
#endif
      return (LIGHTLOADED); 
}

/* 
 * Check the SN0net traffic in the neighborhoods of source and destination 
 * nodes, plus the destination node memory availability.
 * If the SN0net is overloaded, increase the migration threshold a little 
 * and enter the migration state to a queue
 * Else, nothing is being done and the only the traffic condition is returned
 * Called by: migr_manager()
 * Parameters: page num to be migrated, destination node and current node
 * Return: OVERLOADED if traffic is high, LIGHTLOADED if traffic is low
 * Sync: per pfnd lock taken by the caller
 */
/*ARGSUSED*/
migr_traffic_state_t 
migr_manager_traffic_control(cnodeid_t source_nodeid, cnodeid_t dest_nodeid)
{
#if 0
	ASSERT(source_nodeid == cnodeid());

        if (TRAFFIC_ABS_TO_REL(local_neighborhood_traffic_byroute(source_nodeid, dest_nodeid)) > 
            NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_traffic_control_threshold) {
                migr_threshold_adjust(source_nodeid, INCREASE, MIGR_STEP_SMALL);
                return(OVERLOADED);
        }

        else if (TRAFFIC_ABS_TO_REL(remote_neighborhood_traffic_byroute(dest_nodeid, source_nodeid)) > 
                 NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_traffic_control_threshold) {
                migr_threshold_adjust(source_nodeid, INCREASE, MIGR_STEP_TINY);
                return(OVERLOADED);
        }
                
        /* 
         * Don't need the lock for accessing node_freemem since
         * we just peek at the value and don't need a very precise one
         */
        else if (NODE_FREEMEM_REL(dest_nodeid) <
                 NODEPDA_MCD(dest_nodeid)->migr_system_kparms.migr_memory_low_threshold ) {
                migr_threshold_adjust(source_nodeid, INCREASE, MIGR_STEP_TINY);
                return(OVERLOADED);
        }

        else { 	
                /* 
                 * pass all the traffic checkup, ready to migrate
                 */
                return(LIGHTLOADED);
        }

#endif
        return(LIGHTLOADED);
}


#endif /* SN0 */










































