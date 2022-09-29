/*
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

/* Resource conservation module for page replication */

#ident  "$Revision: 1.11 $"

#include <sys/bsd_types.h>
#include <sys/types.h>
#include <sys/cpumask.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/idbgentry.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/pmo.h>
#include "pmo_base.h"
#include "pmo_ns.h"
#include "pmo_list.h"
#include "mld.h"
#include "mldset.h"
#include "raff.h"
#include "memsched.h"
#include "numa_traffic.h"
#include "repl_control.h"

/* lboot tunable parameters . XXX turn off */
int numa_repl_control_enabled = 0;
int numa_repl_traffic_highmark_percentage = 80;
int numa_repl_mem_lowmark = 32;

static void *repl_resource_check(cnodeid_t src_node, void* arg,
			       void *unused);

/* 
 * Initialize the replication control data in the nodepda
 * Called by: nodepda_numa_init()
 */
void 
repl_control_init(cnodeid_t node, caddr_t *nextbyte)
{
	/* Allocate the migration control data structure */
	NODEPDA_RCD(node) = (repl_control_data_t *) 
		low_mem_alloc(sizeof(repl_control_data_t), nextbyte,
			      "NODEPDA_RCD");
	ASSERT(NODEPDA_RCD(node));

	NODEPDA_RCD(node)->repl_control_enabled = 
		numa_repl_control_enabled;
	NODEPDA_RCD(node)->repl_traffic_highmark = 
		FULLY_UTILIZED * numa_repl_traffic_highmark_percentage / 
		PERCENTAGE_BASE;
	NODEPDA_RCD(node)->repl_mem_lowmark = 
		numa_repl_mem_lowmark;
}
	
/* 
 * Control the system resource utilization by page replication.
 * Select a node based on physical memory utilization on the desired node,
 * interconnect traffic on the route from the source node to the destination
 * node, and how flexible the user wants the assignment of the destination 
 * node. 
 * Called by: individual replication policies (some policies may not 
 *            respect the conservation of system resources.)
 * Return: the node where the page will be replicated if success
 *         -1 fail to find the node meeting the resource utilization measure
 */
cnodeid_t 
repl_control(cnodeid_t src_node, cnodeid_t dst_node, rqmode_t rqmode)
{
	cnodeid_t neighbor;

	if (NODEPDA_RCD(dst_node)->repl_control_enabled == 0) {
		/* 
		 * If replication control is not enabled, 
		 * just return with the destionation node
		 * desired by the replication policy
		 */
		return(dst_node); 
	}
	  
	if (NODEPDA_RCD(dst_node)->repl_control_enabled == 0) {
		return(dst_node);
	}

	if (repl_resource_check(dst_node, (void *)&src_node, 0) == 0) {
		return(dst_node);  
	}
	else if (rqmode == RQMODE_MANDATORY) {
		return(CNODEID_NONE);  
	}
	else {
		ASSERT(rqmode == RQMODE_ADVISORY);
		if (physmem_select_masked_neighbor_node(dst_node, 
						 node_distance(src_node, 
							       dst_node) - 1,
						 get_effective_nodemask(curthreadp),	
						 &neighbor, 
						 repl_resource_check,
						 &src_node, 0) > 0) {
			return(neighbor); 
		}
		else {
			return(CNODEID_NONE);
		}
	}
}

/* 
 * Check the resource availability in terms of free physcial memory 
 * on the destination node, and network bandwidth usage on the page 
 * movement path.
 * Return: 1 if the resource check is passed
 *         0 if it's failed
 */

/*ARGSUSED*/
static void *
repl_resource_check(cnodeid_t dst_node, void* arg, void* unused)
{
	cnodeid_t src_node = *(cnodeid_t*)arg;

	if (NODE_FREEMEM(dst_node) < 
	    NODEPDA_RCD(dst_node)->repl_mem_lowmark) {
		return(0);
	}
	else if (local_neighborhood_traffic_byroute(src_node, dst_node) >
		 NODEPDA_RCD(src_node)->repl_traffic_highmark) {
		return(0);
	}
	else if (remote_neighborhood_traffic_byroute(dst_node, src_node) >
		 NODEPDA_RCD(dst_node)->repl_traffic_highmark) {
		return(0);
	}
	else {
		return((void *)1L);
	}
}






