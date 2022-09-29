/*
 * Copyright 1996, Silicon Graphics, Inc.
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

#ident "$Revision: 1.10 $"

#ifdef SN0

#include <sys/bsd_types.h>
#include <sys/types.h>
#include <sys/cpumask.h>
#include <sys/nodepda.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include "pfms.h"
#include "numa_hw.h"
#include "numa_traffic.h"
#include "debug_levels.h"

/* 
 * Get the interconnect traffic in the neighborhood
 */
/*ARGSUSED*/
int
local_neighborhood_traffic_overall(cnodeid_t my_node_id)
{
#if 0
	int i;
	int load;
	int num_of_active_ports;

	ASSERT(my_node_id == cnodeid());

	/* XXX Read router traffic counters and cache them in nodepda */
	/* migr_traffic_update(my_node_id); */
	
	/* 
	 * Collect the traffic load info on all active ports:
	 * -- link receive utilization on port connecting local hub to local 
	 * -- link send utilization on other ports 
	 */

	num_of_active_ports = 0;

	load = 0;

	for (i = 0; i < MAX_ROUTER_PORTS; i++) {
		if (NODEPDA_LRS(my_node_id)->port_status[i]) {
			num_of_active_ports++;
			if (i == NODEPDA_LRS(my_node_id)->my_port_no) {
				/* 
				 * Get statistics on link receive 
				 *  utilization 
				 */
				load += ((uint)NODEPDA_LRS(my_node_id)->port_utilization[i].receive << MAGNIFICATION) / 
					NODEPDA_LRS(my_node_id)->port_utilization[i].free_running; 
			}
			else { 
				/* 
				 * Get statistics on link send utilization 
				 */
				load += (NODEPDA_LRS(my_node_id)->port_utilization[i].send << MAGNIFICATION) / 
					NODEPDA_LRS(my_node_id)->port_utilization[i].free_running;
			}
		}

	}

	/*
	 * It's perfectly fine for a machine without routers, e.g. 
	 * a 2-hub machine with direct link.
	 */
	if (num_of_active_ports > 0) {
		load /= num_of_active_ports;
	}

	mc_msg(DC_TRAFFIC, 15, "(node load) = (%d %d)\n", 
	       my_node_id, load);

	return(TRAFFIC_ABS_TO_REL(load));
#else
	return(TRAFFIC_ABS_TO_REL(0));
#endif
}

/* 
 * Get the traffic in the neighborhood of the local node, we are only 
 * concerned with those links used in page migration from source node to the 
 * destination node 
 * Called by: migr_manager_traffic_control()
 *            repl_control()
 */
/*ARGSUSED*/
int
local_neighborhood_traffic_byroute(cnodeid_t my_node_id,
				   cnodeid_t dest_node_id)
{
#if 0
	int my_port_no;
	int exit_port_no;
	int num_of_active_ports = 2;
	int free_running, utilization;
	int load = 0;

	my_port_no = nodepda->lrs->my_port_no;
	if (my_port_no == PORT_INVALID) {
		/* 
		 * No router is existent, no bandwidth information
		 * is available.
		 */
		return(TRAFFIC_ABS_TO_REL(load));
	}
	
	/* 
	 * Get the number of exit port to the destionation node on the 
	 * local router 
	 */
	exit_port_no = migr_exit_port_get(my_node_id, dest_node_id);
	ASSERT((exit_port_no >= 1) && (exit_port_no <= MAX_ROUTER_PORTS));

	/* 
	 * Collect traffic load on two ports:
	 * link receive utilization on entry port from my node to local router
	 * link send utililization on exit port from local router to dest node
	 */

	/* Get statistics on link receive utilization */
	free_running = nodepda->lrs->port_utilization[my_port_no].free_running;
	utilization = nodepda->lrs->port_utilization[my_port_no].receive;
	load += (utilization << MAGNIFICATION) / free_running /
		num_of_active_ports;
  
	/* Get statistics on link send utilization */
	free_running = nodepda->lrs->port_utilization[exit_port_no].
		free_running;
	utilization = nodepda->lrs->port_utilization[my_port_no].send;
	load += (utilization << MAGNIFICATION) / free_running /
		num_of_active_ports;

	mc_msg(DC_TRAFFIC, 10, "local side (src->dst load) = (%d->%d %d)\n", 
	       my_node_id, dest_node_id, load);

	return(TRAFFIC_ABS_TO_REL(load));
#else
	return(TRAFFIC_ABS_TO_REL(0));
#endif
}

/* 
 * Get the traffic in the neighborhood of the dest node, we are only 
 * concerned with those links used in page migration from my node to the 
 * destination node 
 * Called by: migr_manager_traffic_control()
 *            repl_control()
 */
/*ARGSUSED*/
int 
remote_neighborhood_traffic_byroute(cnodeid_t dest_node_id, 
				    cnodeid_t my_node_id)
{
#if 0
	port_no_t my_port_no;
	port_no_t entry_port_no;
	int num_of_active_ports = 2;
	int free_running, utilization;
	int load = 0;

	/* 
	 * Get the number of port which connects the remote hub to 
	 * its local router 
	 */
	my_port_no = NODEPDA_LRS(dest_node_id)->my_port_no;
	if (my_port_no == PORT_INVALID) {
		/* 
		 * No router is existent, no bandwidth information
		 * is available.
		 */
		return(TRAFFIC_ABS_TO_REL(load));
	}
	

	/* 
	 * Get the number of entry port to the destionation node 
	 * on the router 
	 */
	entry_port_no = migr_exit_port_get(dest_node_id, my_node_id);
	ASSERT((entry_port_no >= 0) && (entry_port_no <= MAX_ROUTER_PORTS));

	/* 
	 * Collect traffic load on two ports:
	 * link receive utilization on entry port from my node to local router
	 * link send utililization on exit port from local router to dest node
	 */
	load = 0;

	/* 
	 * Get statistics on link receive utilization 
	 */
	free_running = Nodepdaindr[dest_node_id]->lrs->
		port_utilization[entry_port_no].free_running;
	utilization = Nodepdaindr[dest_node_id]->lrs->
		port_utilization[entry_port_no].receive;
	load += (utilization << MAGNIFICATION) / free_running /
		num_of_active_ports;
  
	/* 
	 * Get statistics on link send utilization 
	 */
	free_running = Nodepdaindr[dest_node_id]->lrs->
		port_utilization[my_port_no].free_running;
	utilization = Nodepdaindr[dest_node_id]->lrs->
		port_utilization[entry_port_no].send;
	load += (utilization << MAGNIFICATION) / free_running /
		num_of_active_ports;

	mc_msg(DC_TRAFFIC, 10, "remote side (src->dst load) = (%d->%d %d)\n", 
	       my_node_id, dest_node_id, load);

	return(TRAFFIC_ABS_TO_REL(load));
#else
	return(TRAFFIC_ABS_TO_REL(0));
#endif
}

#endif /* SN0 */
























