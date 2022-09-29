/*
 *
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

#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/SN/agent.h>
#include <sys/SN/vector.h>
#include <sys/SN/router.h>
#include <sys/SN/klconfig.h>
#if defined (SN0)
#include <sys/SN/SN0/ip27log.h>
#endif
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <ksys/hwg.h>
#include <sys/nodepda.h>
#include <sys/iograph.h>
#ifdef SN0NET_DEBUG
#define db_cons_printf(x)	printf x
#define db_log_printf(x) 	ip27log_printf x
#else
#define db_cons_printf(x)	
#define db_log_printf(x)
#endif
/* Queue implementation done below is for a circular queue */
#define Q_INDEX_INCR(i)		(i = (i + 1) % MAX_RTR_BREADTH)
#define META_ROUTER_PORT	3	/* Normal router port
					 * which is used to connect 
					 * to a meta- router
					 */

typedef router_queue_t queue_t;

/* Types of queue elements */
#define Q_TYPE_NIC_RTR	1	/* queue element type during router probing */
#define Q_TYPE_HWG_RTR 	2	/* queue element type during router
				 * guardian assignment
				 */

#define Q_TYPES		(Q_TYPE_NIC_RTR | Q_TYPE_HWG_RTR)


/* Initiliaze the queue */
static void queue_init(queue_t *q,int type)
{
#ifdef Q_DEBUG
	db_cons_printf(("queue_init : q = 0x%x type = %d\n",q,type));
#endif
	bzero((char *)q,sizeof(*q));
	q->type = type;
}

/* Check if the queue is empty */
static int queue_empty(queue_t *q)
{
	return(q->head == q->tail);
}

/* Add an element on to the queue if there is room */
static void queue_push(queue_t *q, router_elt_t *elt)
{
#ifdef Q_DEBUG
	db_cons_printf(("queue_push : q = 0x%x elt = 0x%x\n"),q,elt);
#endif
	q->array[q->head] = *elt;
	Q_INDEX_INCR(q->head);
	
	ASSERT_ALWAYS(q->head != q->tail);
}

/* Remove an element from the queue */
static void queue_pop(queue_t *q, router_elt_t *elt)
{
	*elt = q->array[q->tail];
#ifdef Q_DEBUG
	db_cons_printf(("queue_pop : q = 0x%x elt = 0x%x\n"),q,elt);
#endif
	Q_INDEX_INCR(q->tail);
}

/* Search the queue for a router with the given matching key.
 * During router probing router nic is used as the key.
 * During guardian assignment router vertex handle is the key.
*/
static router_elt_t *queue_find(queue_t *q,__uint64_t key)
{
	int		i;
	__uint64_t	elt_key;

	for (i = q->tail ; i != q->head ; Q_INDEX_INCR(i)) {
		
		if (q->type == Q_TYPE_NIC_RTR)
			elt_key = q->array[i].u.r_elt.nic;
		else if (q->type == Q_TYPE_HWG_RTR)
			elt_key = q->array[i].u.k_elt.vhdl;
		if (elt_key == key) {
			/* Found a router with matching nic or vhdl
			 * Return a pointer to the router element 
			 */
#ifdef Q_DEBUG
			db_cons_printf(("queue_find : on q = 0x%x "
				   " found key = 0x%x\n",
				   q,key);
#endif
			return(&q->array[i]);
		}
	}
	
#ifdef Q_DEBUG	
	db_cons_printf(("queue_find : on q = 0x%x "
		   " found key = 0x%x\n",
		   q,key);
#endif
	/* No router in the queue has a matching nic */
	return(NULL);
}

/* Return a pointer to the first element of the queue is non-empty */
static router_elt_t  *queue_first(queue_t *q)
{
	if (!queue_empty(q)) 
		return(&q->array[q->tail]);
	return(NULL);
}

/* Return a pointer to the last element of the queue if non-empty */
static router_elt_t *queue_last(queue_t *q) 
{
	if (!queue_empty(q)) 
		return(&q->array[q->head]);
	return(NULL);
}

/* Given a pointer to a router element present in the queue 
 * return the next element if one exists. If the given element
 * is the last element return NULL
 */
static router_elt_t *queue_next(queue_t *q,router_elt_t *elt)
{	
	int i = elt - &q->array[0];

	if (elt != &q->array[q->head])
		return(&q->array[Q_INDEX_INCR(i)]);
	return(NULL);
}

/* 
 * Clears the bypass enable bit in the router's
 * global parameters register.
 */
static int
turn_router_bypass_off(net_vec_t  path)
{
	__uint64_t	rr_global_parms;
	int		r;
	
	/* Read the RR_GLOBAL_PARMS */
	if ((r = vector_read(path,0, 
			      RR_GLOBAL_PARMS,
			      &rr_global_parms)) < 0) {
		ip27log_printf(IP27LOG_INFO,
		       "turn_router_bypass_off:  vector read RR_GLOBAL_PARMS failed at vec 0x%x %s\n",
			       path,net_errmsg(r));
		return(r);
	}
	
	/* Clear the bypass enable bit */
	rr_global_parms &= ~(RGPARM_BYPEN_MASK << RGPARM_BYPEN_SHFT);

	/* Write the modified RR_GLOBAL_PARMS */
	if ((r = vector_write(path,0, 
			      RR_GLOBAL_PARMS,
			      rr_global_parms)) < 0) {
		db_log_printf((IP27LOG_INFO,
		       "turn_router_bypass_off: vector write RR_GLOBAL_PARMS failed at vec 0x%x %s\n",
			       path,net_errmsg(r)));
		return(r);
	}
	
	return(r);
}


/*
 * Checks whether we have are looking at a HUB or a ROUTER
 *
 */

/* ARGSUSED */
static int 
get_chipid(int 		from_port,/* Valid if from router */
	   net_vec_t 	from_path,/* Path to source chip */
	   net_vec_t 	path,	  /* Path to new chip */
	   __uint64_t 	*status)
{
	int		r;


	/*
	 * Turn off bypassing while we're doing vector ops
	 * to work around a router bug.
	 */
	if ((r = turn_router_bypass_off(path)) < 0)
		return(r);
	/*
	 * Perform read of status rev id reg to see what's out there.
	 */

	r = vector_read(path, 0, NI_STATUS_REV_ID, status);

	db_log_printf((IP27LOG_INFO,
		       "get_chipid: path=0x%lx status=%y r=%d\n", 
		path, *status, r));

	if (r < 0) {

		ip27log_printf(IP27LOG_INFO,
			      "get_chipid: vector read NI_STATUS_REV_ID failed at vec 0x%x %s\n",
			       path, net_errmsg(r));

		/* retry read */

		if ((r = vector_read(path, 0, NI_STATUS_REV_ID, status)) < 0) {
			ip27log_printf(IP27LOG_INFO,
				       "get_chipid: vector re-read  NI_STATUS_REV_ID failed at vec 0x%x %s\n",
				       path, net_errmsg(r));
		} /* if status on retry */

	} /* if status on 1st read */

	return(r);
}
#if 0
static int
check_port_parms(net_vec_t path,int port,int default_status)
{
	__uint64_t	rr_port_parms;

	/* Read the router port parameters register for
	 * this port
	 */
	if (vector_read(path,0,RR_PORT_PARMS(port),&rr_port_parms) < 0) {
		/* 
		 * The vector read to the RR_PORT_PARMS register for this
		 * port shouldn't fail since we have just read
		 * the RR_PROT_CONF register on the same router.
		 * Something bad happened along the "path" during
		 * probing. Cannot pinpoint this case.
		 */

		ip27log_printf(IP27LOG_INFO,
		       "check_port_parms: vector_read failed for RR_PORT_PARMS at vec  0x%x port %d\n",
			       path,port);
		return(PORT_STATUS_UNDEF);
	} else {
		/* If the null time out value in the port parms
		 * is set to indicate that this link failed
		 * after coming out of reset and during
		 * prom discovery then there is no need
		 * to view this is a cause for failure
		 */
		if (GET_FIELD(rr_port_parms, RPPARM_NULLTO)
		    == RPPARM_NULLTO_LINK_BAD) {
			ip27log_printf(IP27LOG_INFO,
			       "check_port_parms: vec 0x%x port %d failed(disc)\n",
				       path,port);
			return(PORT_STATUS_DISCFAIL);
		} else {
			ip27log_printf(IP27LOG_INFO,
			       "check_port_parms: vec 0x%x port %d %s\n",
				       path,port,
				       default_status == PORT_STATUS_KERNFAIL ? 
				       "failed(kern)" : "failed(reset)");
			return(default_status);
		}
	}


}
#endif

/* Given the port status register of a router 
 * find out which links are up and which are down
 * and store this info in a more easily interpretable
 * array(port_status)
 */
static void get_port_status(__uint64_t 		status,
			    net_vec_t 		path,
			    char 		*port_status)
{
	__uint64_t      rr_protection;
	int             port;
	/*
	 * Fill up port_status on each of the ports
	 */

	if (vector_read(path, 0, RR_PROT_CONF, &rr_protection) < 0) {
		ip27log_printf(IP27LOG_INFO,
			       "get_port_status: vector_read of RR_PROT_CONF failed at vec 0x%x\n",
			       path);
		rr_protection = 0x0;
	}

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		port_status[port] = 0;	/* clear all the bits */
		if (status & RSRI_LINKWORKING(port)) {
			/* Link is working according to
			 * port router status rev id resgister
			 */
			db_log_printf((IP27LOG_INFO,
				      "get_port_status: link %d supposedly  working at %x\n",
				       port,path));
#if 0
			/* 
			 * If the RR_PORT_PARMS doesn't have NULLTO 
			 * set to a "link failure during prom discovery
			 * after coming out of reset" indicator then
			 * set the router link is working.
			 */
			port_status[port] |= check_port_parms(path,port,
							      PORT_STATUS_UP);
#else
			port_status[port] = PORT_STATUS_UP;
#endif
			if (~rr_protection & (1 << (port - 1)))
				port_status[port] |= PORT_STATUS_FENCE;
			else
				port_status[port] &= (~PORT_STATUS_FENCE);
		} else {
			db_log_printf((IP27LOG_INFO,
				   "get_port_status: link %d down at %x\n",
				       port,path));
			
			if (status & RSRI_LINKRESETFAIL(port)) {
				/* Link didnot come out of reset. */
				db_log_printf((IP27LOG_INFO,
					       "get_port_status: link %d resetfail at %x\n",
					       port,path));
			
				port_status[port] |= PORT_STATUS_RESETFAIL;
			} else {
#if 0
				/* Link came out of reset.
				 * If the RR_PORT_PARMS doesn't have NULLTO 
				 * set to a "link failure during prom discovery
				 * after coming out of reset" indicator then
				 * set the router link to have failed since this
				 * was supposed to be working at the time of
				 * prom discovery
				 */
				port_status[port] |= 
					check_port_parms(path,port,
							 PORT_STATUS_KERNFAIL);
#else
				port_status[port] |= PORT_STATUS_KERNFAIL;
#endif
			}
		}
	}
}

/* Probe the kind of object that can be got to from the master hub
 * at vector = from_path + port.
 */
static int probe_object(int 		port,
			net_vec_t 	from_path,
			queue_t 	*bfs_router_q)
{
	__uint64_t	status;
	net_vec_t       to_path;
	router_elt_t    new_rtr;
	int              r = 0;
	nic_t		nic = 0xdeadbeef;

	to_path = vector_concat(from_path, port);

	/*
	 * Call get_chipid to read the remote status register with retries
	 */

	if ((r = get_chipid(port, from_path,to_path, &status)) < 0) {

		/*
		 * get_chipid failed so we will just go on with probey
		 */
		
		ip27log_printf(IP27LOG_INFO,
			       "Probe encountered bad link at vec 0x%x ; continuing...\n",
			       to_path);
	} else {

		switch (GET_FIELD(status, NSRI_CHIPID)) {
		case CHIPID_HUB:
			/* Interested in routers only at present */
			db_log_printf((IP27LOG_INFO,
				       "probe_object: hub port = %d vect=%d\n",
				       port,from_path));
			break;
		case CHIPID_ROUTER:
			db_log_printf((IP27LOG_INFO,
				       "probe_object: router port = %d vect=%x\n",
				       port,from_path));

			/* Read the nic of the router . This serves
			 * the purpose of uniquely identifying this
			 * router when we want to know if we have 
			 * visited this already.
			 */
			if (router_nic_get(to_path,&nic) < 0) {
				ip27log_printf(IP27LOG_INFO,
					       "probe_object: read nic failed at vec 0x%x\n",
						to_path);
				
				break;
			}

			db_log_printf((IP27LOG_INFO,
				       "probe_object: nic = 0x%x\n",
				  nic));
			if ((r >= 0) && (nic != 0)) {


				/*
				 * Create a new router element 
				 */
				new_rtr.u.r_elt.nic 	= nic;
				new_rtr.u.r_elt.vec	= to_path;
				new_rtr.u.r_elt.status 	= status;
				/* Put it on the bfs queue of current
				 * routers.
				 */
				queue_push(bfs_router_q,&new_rtr);

			}
			break;
		default:
			db_log_printf((IP27LOG_INFO,
				       "probe_object: unknown chip ID,"
				       " path=0x%lx, "
				       "status=%y\n", to_path, status));
			r = 0;
			break;
		}
	}

	return(r);
}
/* Main Bread First Search Loop  for finding all the
 * routers in the system.
 */
static int probe_loop(queue_t 	*bfs_router_q,
		      queue_t	*visited_router_q)
{
	int     r, fence = 0;
	char	port_status[MAX_ROUTER_PORTS + 1];		

	/* Loop as long there are routers to be dealt with in the 
	 * bfs router queue
	 */
	while (!queue_empty(bfs_router_q)) {
		int  		port;
		router_elt_t   	next_router;
	
		/* Take the first router element in the queue*/
		queue_pop(bfs_router_q, &next_router);
		if (queue_find(visited_router_q,
			       next_router.u.r_elt.nic)) {
			/* We have already looked at all the links of
			 * this router.
			 */
			db_log_printf((IP27LOG_INFO,
				       "Seen router with nic 0x%x already\n",
				       next_router.u.r_elt.nic));
			continue;
		}

		/* Visiting the links of this router for the first
		 * time 
		 */

		/* Interpret the port status for this router */
		get_port_status(next_router.u.r_elt.status,
				next_router.u.r_elt.vec, 
				port_status);
		
		/* Continue probing on each of the working links of
		 * this router
		 */
		for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
			if (port_status[port] & PORT_STATUS_UP) {
				if (port_status[port] & PORT_STATUS_FENCE)
					fence = 1;
				else
					if ((r = probe_object(port,next_router.u.r_elt.vec, 
							      bfs_router_q)) < 0) {
						/* Router link went down during probing */
						ip27log_printf(IP27LOG_INFO,
							      "Link %d at vec 0x%x rtr nic 0x%x went down during probing\n",
							      port,next_router.u.r_elt.vec, 
							      next_router.u.r_elt.nic);
						port_status[port] &= ~PORT_STATUS_UP;
						port_status[port] |= PORT_STATUS_KERNFAIL;
					}
						
			}
			/* Store the interpreted port status in the
			 * router element of the visited router queue
			 */
			next_router.u.r_elt.port_status[port] = 
				port_status[port];
		}
		/* Remember that we have looked at all the links of
		 * this router 
		 */
		queue_push(visited_router_q,&next_router);
		
		db_log_printf((IP27LOG_INFO,
			       "Seeing router with nic 0x%x for the first time\n",
			       next_router.u.r_elt.nic));

	}
	return(fence ? fence : ((r < 0) ? r : 0));
}

/* Store the mapping from router (nic) --> (module,slot) in the current
 * nodepda for all the routers in the system.
 * Each entry of the router map is of the following format:
 * 	+-----------------------------------------------+
 *	|MODULE(8bits)|SLOT(8bits) |	NIC (48bits)	|
 * 	+-----------------------------------------------+
 */
void
router_map_init(nodepda_t *npda)
{
	nasid_t 	nasid;
	cnodeid_t 	cnode;
	lboard_t 	*brd;
	__uint64_t	*router_map_ent;	
	
	npda->num_routers = 0;
	for (cnode = 0; cnode < maxnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);

		brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
				KLTYPE_ROUTER);

		if (!brd)
			continue;
		do {
			if (brd->brd_flags & DUPLICATE_BOARD)
				continue;
			/* If we are seeing this router board for
			 * the first time create a router mapping
			 * entry for this.
			 */
			router_map_ent = 
				&(npda->router_map[(npda->num_routers)++]);
			/* Set the module field in the router map entry */
			SET_FIELD(*router_map_ent,
				  ROUTER_MAP_MOD,
				  brd->brd_module);
			/* Set the slot field in the router map entry */
			SET_FIELD(*router_map_ent,
				  ROUTER_MAP_SLOT,
				  brd->brd_slot);
			/* Set the nic field in the router map entry */
			SET_FIELD(*router_map_ent,
				  ROUTER_MAP_NIC,
				  brd->brd_nic);
			/* Find the rest of the routers stored on this node. */
		} while (brd = find_lboard_class(KLCF_NEXT(brd), 
						 KLTYPE_ROUTER));

	}

}
/*
 * Given a router nic get the corresponding (module,slot) information
 * for the router from the mappping stored in the local nodepda.
 * This is useful during nmi handling when there might not be shared
 * memory.
 */
void
router_map_get(nic_t nic,moduleid_t *mod,slotid_t *slot)
{
	__uint64_t	*router_map = nodepda->router_map;
	int		i,num_routers = nodepda->num_routers;

	/* Store some bogus values into the module & slot 
	 * parameters.
	 */
	*mod = 0; *slot = 0xff;
	for(i = 0 ; i < num_routers ; i++) {
		if (GET_FIELD(router_map[i],ROUTER_MAP_NIC) == nic) {
			*mod = GET_FIELD(router_map[i],ROUTER_MAP_MOD);
			*slot = GET_FIELD(router_map[i],ROUTER_MAP_SLOT);
			return;
		}
	}
	/* We should never come here as there MUST be a mapping for
	 * for a particular router nic.It is the responsibility of
	 * the caller to pass a valid router nic to this function.
	 */
	ASSERT(0);
}
/* Print out the information about the results of
 * probing the routers and traversing all their links
 */
static int
probe_results_print(queue_t	*q,
		    int		level,
		    void (*pf)(int, char *, ...))
{
	int		port;
	router_elt_t	*elt;
	int		rv = PROBE_RESULT_GOOD;	/* sticky variable set to 
						 * BAD if a router link 
						 * died after coming out of
						 * reset
						 */
	char		failed_links[20];
	char		port_name[10],slot_name[10];
	moduleid_t	module;
	slotid_t	slot;
	int		link_failed = 0; /* Use this variable not to print
					  * info if no link failed after
					  * the kernel started running 
					  */
	int	       header_printed = 0;/* Print the header only once */

	/* If the queue is empty we don't have to print anything */
	if (queue_empty(q))
		return(PROBE_RESULT_GOOD);

	for(elt = queue_first(q) ; 
	    elt && elt != queue_last(q);
	    elt = queue_next(q,elt)) {
		strcpy(failed_links,"");
		for (port = 1 ; port <= MAX_ROUTER_PORTS ; port++) {
			char	status = elt->u.r_elt.port_status[port];

			if (status & PORT_STATUS_KERNFAIL) {
				/* Remember the fact that at least one link
				 * failed for this router.
				 */
				link_failed = 1;
				/* Update the list of failed links
				 * for this router.
				 */
				sprintf(port_name," %d",port);
				strcat(failed_links,port_name);
				rv = PROBE_RESULT_BAD;
			}
				
		}
		/* Print information only if at least one of the links 
		 * for the router failed.
		 */
		if (link_failed) {
			/* If this is the first router with bad links
			 * print the header. Remember the fact that the 
			 * header has been printed.
			 */
			if (!header_printed) {
				header_printed = 1;
				pf(level, "Failed Router Link Summary\n");
				pf(level, "Module\tSlot\tFailed Links\n");
			}
			link_failed = 0;
			router_map_get(elt->u.r_elt.nic,&module,&slot);
			get_slotname(slot,slot_name);
			/* Print the module , slot & failed links for this
			 * router.
			 */
			pf(level,
			   "%d\t%s\t[%s]\n",
			   module,slot_name,
			   failed_links);
		}
	}
	return(rv);
}
 
/*
 * net_link_up
 *
 *   Return values:
 *	0		Link is up
 *	non-zero	Link is down (failed after reset or still in reset)
 */

static int
net_link_up(void)
{

    return (int) GET_FIELD(LOCAL_HUB_L(NI_STATUS_REV_ID), NSRI_LINKUP);
}

/* Allocate memory for the router traversal queue
 */
void
router_queue_init(nodepda_t *npda,cnodeid_t cnode)
{
	npda->visited_router_q = 
		(router_queue_t *)kmem_zalloc_node(sizeof (router_queue_t),VM_NOSLEEP, 
						   cnode);
	ASSERT_ALWAYS(npda->visited_router_q);
	npda->bfs_router_q = 
		(router_queue_t *)kmem_zalloc_node(sizeof (router_queue_t),VM_NOSLEEP, 
						   cnode);
	ASSERT_ALWAYS(npda->bfs_router_q);
}
/* Start the probing process (for routers) from a master hub
 */
int 
probe_routers(void)
{
	int			r = PROBE_RESULT_GOOD;
	queue_t			*bfs_router_q,*visited_router_q;

	db_log_printf((IP27LOG_INFO,"Inside probe\n"));
	/*
	 * Probe network interface to make sure it's up.
	 * Then start probe with null path from current hub.
	 */

	if (! net_link_up()) {
		ip27log_printf(IP27LOG_INFO,"*** Local network link down\n");
		return(0);
	}

	bfs_router_q = nodepda->bfs_router_q;
	visited_router_q = nodepda->visited_router_q;
	queue_init(bfs_router_q,Q_TYPE_NIC_RTR);
	queue_init(visited_router_q,Q_TYPE_NIC_RTR);

	db_log_printf((IP27LOG_INFO,"probing local router\n"));
	if ((r = probe_object(0, 0, bfs_router_q)) < 0) {
		r = PROBE_RESULT_BAD;
		ip27log_printf(IP27LOG_INFO,"Local router probe failed\n");
		return(r);
	}
	db_log_printf((IP27LOG_INFO,"probe other router\n"));
	if ((r = probe_loop(bfs_router_q,visited_router_q)) < 0) {
		r = PROBE_RESULT_BAD;
		db_log_printf((IP27LOG_INFO,"Remote router probe failed\n"));
		return(r);
	}
		
	/* Put the router probing results into the promlog */
	(void)probe_results_print(visited_router_q,IP27LOG_INFO,
				  ip27log_printf);
	/* Try printing the router probe results to the console */
	r = probe_results_print(visited_router_q,CE_CONT,cmn_err);
	return(r);
}
/*
 * Get the link number which takes us from 
 * the source router to the destination 
 * router
 */
int
router_link_get(vertex_hdl_t	src_rtr,
		vertex_hdl_t	dst_rtr)
{
	int		port;
	vertex_hdl_t	port_vhdl;
	char		port_name[10];

	for(port = 1 ; port <= MAX_ROUTER_PORTS; port++) {
		sprintf(port_name,"%d",port);
		if (hwgraph_traverse(src_rtr,port_name,&port_vhdl)
		    != GRAPH_SUCCESS)
			continue;
		if (port_vhdl == dst_rtr)
			return(port);

	}
	return(0);
}

/* Breadth first search routine to search for a possible guardian
 * node for a router corresponding to a given vertex handle.
 * This routine also returns the vector route from the guardian to 
 * router if a guardian is found.
 */
static vertex_hdl_t
router_guardian_search(net_vec_t 		*vector,
			     vertex_hdl_t	routerv)
{
	vertex_hdl_t	retval;
	vertex_hdl_t	port_vhdl;
	int		i,rev_port;
	char		port[10];
	cnodeid_t	cnodeid;
	router_elt_t	next_router,*visited_router = NULL;
	queue_t		*router_vertex_qp;
	queue_t		*visited_router_vertex_qp;
	nodepda_t	*npda;
	vertex_hdl_t	candidate_guard;
	nodepda_t	*candidate_npda;
	net_vec_t	candidate_vector;

	/* Order in which the ports of a router are traversed .
	 * Note that we try to find guardians among the directly
	 * connected nodes to the router first and then head off
	 * for the other routers connected to this router.
	 */
	int 		portnum[] = {  4 , 5 , 6 , 1 , 2 , 3};
	
	
	/* Initialize the current queue of router elements */
	router_vertex_qp = kmem_alloc(sizeof(*router_vertex_qp), 0);
	queue_init(router_vertex_qp,Q_TYPE_HWG_RTR);
	/* Initialize the visited queue of router elements */
	visited_router_vertex_qp = kmem_alloc(sizeof(*visited_router_vertex_qp), 0);
	queue_init(visited_router_vertex_qp,Q_TYPE_HWG_RTR);

	/* Put the router for which the guardian is to be
	 * found on the current queue
	 */
	next_router.u.k_elt.vhdl 	= routerv;
	next_router.u.k_elt.guard 	= GRAPH_VERTEX_NONE;
	next_router.u.k_elt.vec		= 0;
	queue_push(router_vertex_qp,&next_router);

	while (!queue_empty(router_vertex_qp)) {
		candidate_npda 		= NULL;
		/* Look at the first router in the bfs router queue */
		queue_pop(router_vertex_qp,&next_router);
		/* Check if we have already looked at the links of this 
		 * router
		 */
		visited_router = queue_find(visited_router_vertex_qp,
					    next_router.u.k_elt.vhdl);
		if (visited_router) {
			/* We have already looked at the links of this router
			 */
			db_cons_printf(("Seen this router %v already\n",
				  next_router.u.k_elt.vhdl));
			continue;
		}

		/* We are seeing this router for the first time.
		 * Look at each of its links.
		 */
		for(i = 0 ; i < MAX_ROUTER_PORTS ; i++) {
			sprintf(port,"%d",portnum[i]);
			if (hwgraph_traverse(next_router.u.k_elt.vhdl,
					     port,&port_vhdl)
			    != GRAPH_SUCCESS) 
				continue;
			/* If the router is connected to a node
			 * which has a cpu enabled then choose this
			 * node as the guardian.
			 */
			if ((cnodeid = nodevertex_to_cnodeid(port_vhdl))
			    != CNODEID_NONE) {
				/* Check if this node has any enabled
				 * cpus
				 */
				if (cnodetocpu(cnodeid) != CPU_NONE) {
					npda = NODEPDA(cnodeid);
					ASSERT(npda);
					if (candidate_npda) {
						/* We have already encountered
						 * another node which is a possible
						 * candidate for being a guardian
						 * for this router.
						 * Among the 2 possible candidates
						 * choose the node with the min.
						 * number of dependent routers.
						 * Ties are broken on fcfs basis.
						 */
						if (npda->dependent_routers <
						    candidate_npda->dependent_routers) {
							*vector = next_router.u.k_elt.vec;
							next_router.u.k_elt.guard =
								port_vhdl;
							retval = port_vhdl;
							goto done;
						} else {
							*vector = candidate_vector;
							retval = candidate_guard;
							goto done;
						}
					} else {
						candidate_npda = npda;
						candidate_guard = port_vhdl;
						candidate_vector = next_router.u.k_elt.vec;
					}

				} else
					db_cons_printf(("router_guardian_search: "
						   "node %v is headless\n",
						   port_vhdl));
			} else {
				router_elt_t	new_router;
				/* Try to go to the router connected to this
				 * router and see if that one can come up with
				 *  any possible guardian candidate.
				 */
				db_cons_printf(("router_guardian_search : "
					  "Taking an extra hop from %v to "
					  "%v at port %x\n",
					  next_router.u.k_elt.vhdl,
					  port_vhdl,portnum[i]));
				  
				new_router.u.k_elt.vhdl = port_vhdl;
				new_router.u.k_elt.guard = GRAPH_VERTEX_NONE;
				/* Get the port number from the new router 
				 * to the current router 
				 */
				if (rev_port = router_link_get(port_vhdl,next_router.u.k_elt.vhdl)) {
					/* Form the reverse vector path */
					new_router.u.k_elt.vec = 
						vector_concat(rev_port,next_router.u.k_elt.vec);
					db_cons_printf(("new vec = 0x%x  old vec = 0x%x\n",
							new_router.u.k_elt.vec,
							next_router.u.k_elt.vec));
					  
					queue_push(router_vertex_qp,&new_router);

				} else {
					/* This is weird . There is a link from current to the
					 * new router . But there is no link from the new to
					 * the current router
					 */
					db_cons_printf(("No router link from %v to %v\n",
							port_vhdl,next_router.u.k_elt.vhdl));
					
				}
					
			}
		}
		/* Check if we got hold of a unique candidate
		 * guardian node. Then make that the guardian node
		 */
		if (candidate_npda) {
			*vector = candidate_vector;
			retval = candidate_guard;
			goto done;
		}
		/* Remember that we have looked at all the links of
		 * this router.
		 */
		queue_push(visited_router_vertex_qp,&next_router);
		db_cons_printf(("Seeing this router %v for the first time\n",
			  next_router.u.k_elt.vhdl));
		
	}
	retval = GRAPH_VERTEX_NONE;

done:
	kmem_free(router_vertex_qp, sizeof(*router_vertex_qp));
	kmem_free(visited_router_vertex_qp, sizeof(*visited_router_vertex_qp));
	return retval;

}
/*
 * Store the initial relevant information about the
 * router on the nodepda corresponding to the
 * guardian node.
 * This is useful during router info initialization 
 * which hangs off the vertex correspoing to this router.
 */
static void
guardian_nodepda_update(vertex_hdl_t 	guardian,
		     net_vec_t		vector,
		     lboard_t		*router_brd,
		     vertex_hdl_t	router_hndl)
{
	cnodeid_t		cnodeid;
	nodepda_t		*npda;	
	nodepda_router_info_t	*npda_rip,**npda_ripp;

	cnodeid = nodevertex_to_cnodeid(guardian);
	npda = NODEPDA(cnodeid);
	/* Get the pointer to place in the list of
	 * dependent routers where a new dependent
	 * router can be added on the guardian 
	 * nodepda.
	 */
	npda_ripp = npda->npda_rip_last;
	/* Allocate memory for the router information
	 * to be stored in the nodepda for this dependent
	 * router
	 */
	npda_rip = *npda_ripp = (nodepda_router_info_t *)
		kmem_zalloc_node(sizeof(nodepda_router_info_t),
				 VM_DIRECT|KM_NOSLEEP , cnodeid);
	ASSERT_ALWAYS(npda_rip);

	npda_rip->router_next = NULL;
	/* Update the pointer to the place where the
	 * next dependent router info (if any) can be
	 * added
	 */
	npda->npda_rip_last  = &npda_rip->router_next;
	/* Accumulate the count of dependent routers
	 * for this guardian node
	 */
	npda->dependent_routers++;
	
	/* Initialize the initial router specific info in the
	 * nodepda.
	 * In particular store
	 *	router's vertex handle
	 *	vector router from the guardian node to the router
	 *	router board slot
	 * 	router module id
	 *	router board type 
	 */
	npda_rip->router_vhdl 	= router_hndl;	
	npda_rip->router_vector = vector;
	ASSERT(router_brd);
	npda_rip->router_slot 	= router_brd->brd_slot;
	npda_rip->router_module = router_brd->brd_module;
	npda_rip->router_type 	= router_brd->brd_type;
	
}
#ifdef SN0NET_DEBUG
typedef struct rtr_guard_s {
	vertex_hdl_t	rtr;
	vertex_hdl_t	guard;
	net_vec_t	vec;
} rtr_guard_t;

static rtr_guard_t	guard_assignment[MAX_RTR_BREADTH];
static int		num_routers = 0;
#endif
/*
 * Try to find a suitable guardian node for this router
 */
static void
router_guardian_set(vertex_hdl_t	hwgraph_root,
		    lboard_t		*brd)
{
	char 		path_buffer[50];
	vertex_hdl_t 	router_hndl;
	int 		rc;
	vertex_hdl_t	guardian = GRAPH_VERTEX_NONE;
	net_vec_t	vector = 0;

	/* No need to process duplicate boards. */
	if (brd->brd_flags & DUPLICATE_BOARD) {
		board_to_path(brd,path_buffer);
		db_cons_printf(("router_guardian_set: Duplicate router 0x%x "
			  " for %s\n",
			  brd, path_buffer));
		return;
	}

	/* Generate a hardware graph path for this board. */
	board_to_path(brd, path_buffer);

	rc = hwgraph_traverse(hwgraph_root, path_buffer, &router_hndl);

	if (rc != GRAPH_SUCCESS) {
		cmn_err(CE_WARN,
			"Can't find router: %s", path_buffer);
		return;
	}

	/* We don't know what to do with multiple router components */
	if (brd->brd_numcompts != 1) {
		cmn_err(CE_PANIC,
			"connect_one_router: %d cmpts on router\n",
			brd->brd_numcompts);
		return;
	}

	db_cons_printf(("router_guardian_set: Setting guardian for %v\n",
		  router_hndl));
	guardian = router_guardian_search(&vector,router_hndl);
	if (guardian != GRAPH_VERTEX_NONE) {
		
		guardian_nodepda_update(guardian,vector,brd,router_hndl);
		db_cons_printf(("Found guardian %v for %v at %x\n",
			  guardian,router_hndl,vector));
#ifdef SN0NET_DEBUG
		ASSERT(num_routers < MAX_RTR_BREADTH);
		guard_assignment[num_routers].rtr 	= router_hndl;
		guard_assignment[num_routers].guard	= guardian;
		guard_assignment[num_routers].vec	= vector;
		num_routers++;
#endif
	}
}

/* Find guardians for all the routers in the system */
void
router_guardians_set(vertex_hdl_t hwgraph_root) 
{
	nasid_t 	nasid;
	cnodeid_t 	cnode;
	lboard_t 	*brd;

	for (cnode = 0; cnode < maxnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);

		db_cons_printf(("guardians_set: Connecting routers on cnode "
			  "%d\n",cnode));

		brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
				KLTYPE_ROUTER);

		if (!brd)
			continue;

		do {
			router_guardian_set(hwgraph_root, brd);
			/* Find the rest of the routers stored on this node. */
		} while (brd = find_lboard_class(KLCF_NEXT(brd), KLTYPE_ROUTER));
	}
#ifdef SN0NET_DEBUG
	{
		int	i;
		db_cons_printf(("\n\t\tROUTER GUARDIAN ASSIGNMENT\n"));
		db_cons_printf(("\nROUTER\t\t\t\tGUARD\t\t\t\tVECTOR\n"));
		for(i = 0 ; i < num_routers; i++) {
			db_cons_printf(("%v",guard_assignment[i].rtr));
			db_cons_printf((" %v",guard_assignment[i].guard));
			db_cons_printf(("\t\t0x%x\n",guard_assignment[i].vec));
		}
		db_cons_printf(("\n\n"));
	}
#endif
}
