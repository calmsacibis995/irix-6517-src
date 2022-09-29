/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/***********************************************************************\
*	File:		traverse_nodes.c				*
*									*
*	Contains code that locates and numbers all nodes in this        *
*		partition.  It also sets up the node table in the       *
*               GDA.                                                    *
*									*
\***********************************************************************/

#ident  "$Revision: 1.80 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>
#include <ksys/elsc.h>
#include <sys/SN/gda.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/vector.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/launch.h>
#include <sys/SN/error.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/klpart.h>
#include "io6prom_private.h"
#include "traverse_nodes.h"

#define makecpuid(x, y)		(((x) << 1) | (y))

extern int Debug;
extern int pod_mode(void);
extern void slave_elsc_module_set(nasid_t nasid);
extern __psunsigned_t io6prom_elsc_init(void);

/* #define TRAVERSE_DEBUG	1 */

#ifdef TRAVERSE_DEBUG
#define TDPRT	printf
#else
#define TDPRT	if (Debug) printf
#endif

/*
 * The following need not be an array. It can be an array of 8 64bit ints
 * where we set bits to indicate the presence of a certain node.
 */
nasid_t node_table[MAX_NASIDS];
int	disabled_cpus = 0;
int     older_ip27prom = 0;
int	num_cpus = 0;

typedef struct vrtx_s {
	lboard_t	*rou_brd;	/* lboard_t rtr ptr */
	net_vec_t	vec;		/* vec from master node */
	int		src_port;	/* port first seen from master */
} vrtx_t;

typedef struct queue_s {
	char	head;
	char	tail;
	vrtx_t	array[MAX_ROUTERS];
} queue_t;

queue_t rou_q;

int router_vers_warning;

static void queue_init(queue_t *q)
{
    int         i;

    q->head = q->tail = 0;

    for (i = 0; i < MAX_ROUTERS; i++) {
        q->array[i].rou_brd = 0;
        q->array[i].vec = 0;
        q->array[i].src_port = 0;
    }
}

static int queue_empty(queue_t *q)
{
    return (q->head == q->tail);
}

static void queue_push(queue_t *q, vrtx_t *elem)
{

    TDPRT("***: Adding rtr brd 0x%lx: nic 0x%lx: on nasid %d\n", 
	   elem->vec, elem->rou_brd->brd_nic, NASID_GET(elem->rou_brd));

    q->array[q->head].rou_brd = elem->rou_brd;
    q->array[q->head].vec = elem->vec;
    q->array[q->head++].src_port = elem->src_port;

    q->head %= MAX_ROUTERS;

    if (q->head == q->tail)
        printf("WARNING: Router vertex queue limit %d exceeded!\n",
                MAX_ROUTERS);
}

static void queue_pop(queue_t *q, vrtx_t *elem)
{
    elem->rou_brd = q->array[q->tail].rou_brd;
    elem->vec = q->array[q->tail].vec;
    elem->src_port = q->array[q->tail++].src_port;

    q->tail %= MAX_ROUTERS;
}

void
number_node_board(lboard_t *brd, cnodeid_t *id, gda_t *gdap)

{
	int i;
	klinfo_t *kli;
	nasid_t nasid = INVALID_NASID;
	hubreg_t clock_control;
	int disabled_hub = 0;

	/*
	 * Mark this board as visited so we won't visit it again during
	 * node traversal.
	 */
	brd->brd_flags |= VISITED_BOARD;
	for (i = 0; i < brd->brd_numcompts; i++) {
		kli = (klinfo_t *)(NODE_OFFSET_TO_K1(NASID_GET(brd),
						     brd->brd_compts[i]));

		/*
		 * Set virtual IDs for CPUs and for Hubs but not for memory
		 */
		switch(kli->struct_type) {
		case KLSTRUCT_CPU:
			num_cpus++;
			if (KLCONFIG_INFO_ENABLED(kli) == 0)
			{
			    disabled_cpus++;
			/* hack to determine whether we have an ip27prom that
			   still assigns cpu-speed of -1 to disabled cpus */
		   	    if((signed short)(((klcpu_t *)kli)->cpu_speed) == -1)
			    older_ip27prom = 1;
			}
			break;
		case KLSTRUCT_HUB:
			/* cnodeid */
			kli->virtid = *id;
			/* nasid */
			nasid = kli->nasid;

			/* If we're the master, don't do this. */
			if (!(*id))
			    break;
			if(!(kli->flags & KLINFO_ENABLE)) {
			    disabled_hub = 1;
			    break;
			}

			/* Turn off the master global clock bits. */
			clock_control = REMOTE_HUB_L(nasid,
						     PI_RT_LOCAL_CTRL);
			TDPRT("REMOTE_HUB_L(%d, PI_RT_LOCAL_CTRL) == 0x%x\n",
			      nasid, clock_control); 
			clock_control &= ~(PRLC_GCLK_EN |
					   PRLC_USE_INT_MASK);
			REMOTE_HUB_S(nasid, PI_RT_LOCAL_CTRL,
				     clock_control);
			TDPRT("REMOTE_HUB_S(%d, PI_RT_LOCAL_CTRL, 0x%x\n)", 
			      nasid, clock_control); 
			clock_control = REMOTE_HUB_L(nasid, PI_RT_LOCAL_CTRL);
			TDPRT("REMOTE_HUB_L(%d, PI_RT_LOCAL_CTRL) == 0x%x\n",
			      nasid, clock_control);
			break;
		default:
			break;
		}
	}

	/* Check for valid nasid */
	if (nasid == INVALID_NASID) {
		printf("Encountered no KLSTRUCT_HUB in node board: 0x%x\n",
			brd);
	}
	if(!disabled_hub) {
		node_table[nasid] = nasid;
		/* Set up this hub's entry in the nasidtable */
		gdap->g_nasidtable[*id] = nasid;
		(*id)++;
	}
	/* move on to the next cnodeid */
}


void
number_cpus_and_nodes(gda_t *gdap, int nodes)
{
    cnodeid_t cnode = 0;
    nasid_t curnasid, nasid;
    cpuid_t cpu = 0, dis_cpu = num_cpus - disabled_cpus;
    int i;
    klinfo_t *kli;
    lboard_t *brd;
    int	config_12p4i_nodes = 0;	

    for (nasid = gdap->g_nasidtable[cnode]; nasid < MAX_NASIDS; nasid++) {
	if (node_table[nasid] == nasid) {
	    gdap->g_nasidtable[cnode] = nasid;
	    cnode++;
	}
    }
    for (nasid = 0; nasid < gdap->g_nasidtable[0]; nasid++) {
	if (node_table[nasid] == nasid) {
	    gdap->g_nasidtable[cnode] = nasid;
	    cnode++;
	}
    }

    if (cnode != nodes) {
	printf("Error: Nodes numbered 0x%x. Nodes found 0x%x!\n",
	       nodes, cnode);
    }
    for (cnode = 0; cnode < nodes; cnode++) {
	curnasid = gdap->g_nasidtable[cnode];
	if (curnasid == INVALID_NASID)
	    break;

	if (CONFIG_12P4I_NODE(curnasid))
	    config_12p4i_nodes++;
	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(curnasid), KLTYPE_IP27);
	do {
	if (!brd) {
	    printf("number_cpus: Error - No IP27 on nasid %d!\n", curnasid);
	    continue;
	}
	for (i = 0; i < brd->brd_numcompts; i++) {
	    kli = (klinfo_t *)(NODE_OFFSET_TO_K1(NASID_GET(brd),
						 brd->brd_compts[i]));
	    switch(kli->struct_type) {
	    case KLSTRUCT_CPU:
		/* cnodeid:slice */
		if (KLCONFIG_INFO_ENABLED(kli))
		    kli->virtid = cpu++;
#if 0
		else
		    kli->virtid = -1;
#else
		/*
		 * if we need to, we could number the disabled cpus after
		 * the regular cpus.
		 */
		else
		{
		   if(older_ip27prom)
		    kli->virtid = -1;
		   else
		    kli->virtid = dis_cpu++;
		}
#endif
		break;
	    case KLSTRUCT_HUB:
		kli->virtid = cnode;
		break;

	    default:
		break;
	    }
	}
	brd = KLCF_NEXT(brd);
	if(!brd)
		break;
	brd = find_lboard((lboard_t *)brd, KLTYPE_IP27);
	}while(brd);
    }

    /* Check to make sure that all proms are in sync with each 
     * other with respect to the 12p 4i flag being set
     */
    if (config_12p4i_nodes != 0 &&
	config_12p4i_nodes != nodes)
	printf("WARNING: NOT ALL NODES HAVE 12P4I PROMS!!!\n");
    else if (config_12p4i_nodes == nodes)
	printf("Current proms correspond to 12P4I config\n");

    printf("%d CPUs on %d nodes found.\n", cpu, nodes);
}


void
check_and_mark_replicas(lboard_t *brd, klrou_t *router, int update)
{
	lboard_t *fnd_brd, *nbrd;
	klrou_t *nrou;
	int i;

	if (!update)
		brd->brd_flags |= VISITED_BOARD;

	TDPRT("mark_replicas: brd == 0x%x\n", brd);

	for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
		if (router->rou_port[i].port_nasid == INVALID_NASID)
			continue;

		fnd_brd = 
		    (lboard_t *)NODE_OFFSET_TO_K1(
			router->rou_port[i].port_nasid,
			router->rou_port[i].port_offset);

		if (fnd_brd->brd_type == KLTYPE_IP27) {
			TDPRT("mark_replicas: Look up router 0x%x, nasid %d\n",
				router, router->rou_port[i].port_nasid);
			if ((config_find_nic_router(router->rou_port[i].port_nasid,
					brd->brd_nic, &nbrd, &nrou)) != -1) {

			 	if (update) {
					if (nbrd->brd_flags & DUPLICATE_BOARD) {
						nbrd->brd_slot = brd->brd_slot;
						nbrd->brd_module =
							brd->brd_module;
					}
				} else {
					nbrd->brd_flags |= VISITED_BOARD;
				}
			}
		}
		else if ((brd->brd_type == KLTYPE_META_ROUTER) && (fnd_brd->brd_type 
				== KLTYPE_ROUTER)) {
			klrou_t	*rp;
			int	port;

			if (!(rp = (klrou_t *) find_first_component(fnd_brd,
				KLSTRUCT_ROU))) {
				printf("WARNING: Can't find RTR struct on "
				"nasid %d\n", fnd_brd->brd_nasid);
				continue;
			}
			
			for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
				lboard_t	*b;

				if (rp->rou_port[port].port_nasid ==
					INVALID_NASID)
					continue;

				if ((config_find_nic_router(
				rp->rou_port[port].port_nasid, 
				brd->brd_nic, &b, NULL)) != -1) {
			 		if (update) {
						if (b->brd_flags & 
							DUPLICATE_BOARD) {
							b->brd_slot = 
							brd->brd_slot;
							b->brd_module =
							brd->brd_module;
						}
					} 
					else
						b->brd_flags |= 
							VISITED_BOARD;
				}
			}
		}
	}

	/* Do a final check on the global master */

	if ((config_find_nic_router(get_nasid(), brd->brd_nic, &fnd_brd,
				NULL)) != -1) {
 		if (update) {
			if (fnd_brd->brd_flags & DUPLICATE_BOARD) {
				fnd_brd->brd_slot = brd->brd_slot;
				fnd_brd->brd_module = brd->brd_module;
			}
		}
		else
			fnd_brd->brd_flags |= VISITED_BOARD;
	}
}

#define SET_ROU_VECTOR(_rou, _vec)				 	\
        ((_rou)->rou_info.struct_version >= ROUTER_VECTOR_VERS) ? 	\
         (_rou)->rou_vector = _vec: 				 	\
	 ((_rou)->rou_info.virtid = (_vec >> 32) & 0xffffffff, 	 	\
	  (_rou)->rou_info.pad3   = (_vec >> 16) & 0xffff,	 	\
	  (_rou)->rou_info.pad4   = (_vec) & 0xffff)

#define GET_ROU_VECTOR(_rou)					 	\
        ((_rou)->rou_info.struct_version >= ROUTER_VECTOR_VERS) ? 	\
         (_rou)->rou_vector:	 			 	 	\
	 (((__uint64_t)(_rou)->rou_info.virtid << 32) | 		\
	  ((__uint64_t)(_rou)->rou_info.pad3 << 16)   | 		\
	  ((__uint64_t)(_rou)->rou_info.pad4))


static void
config_router(lboard_t *brd, net_vec_t vector,
	      klrou_t *router, int source_port)
{
	__uint64_t value;
	int rv;
	int slotbits;
	int port;
	int err;
	nasid_t source_nasid = get_nasid();

	TDPRT("config_router: vec 0x%lx: nic 0x%lx: src_port = %d\n", vector, 
		brd->brd_nic, source_port);

	err = vector_read_node(vector, source_nasid, 0, RR_STATUS_REV_ID, &value);
	if (err < 0) 
		goto error;
		
	if (((value & RSRI_CHIPID_MASK) >> RSRI_CHIPID_SHFT) != CHIPID_ROUTER) {
		printf("config_router: Error - not a router, value 0x%x!\n", value);
		return;
	}
	else {

		slotbits = ((value & RSRI_CHIPIN_MASK) >> RSRI_CHIPIN_SHFT);
		
		brd->brd_slot = router_slotbits_to_slot(slotbits);

		if (SLOTNUM_GETCLASS(brd->brd_slot) == SLOTNUM_INVALID_CLASS) {
			if (brd->brd_type == KLTYPE_META_ROUTER) 
			    brd->brd_slot = meta_router_slotbits_to_slot(slotbits);
			else
			    printf("config_router: Error - unknown router, board 0x%x slot %d slotbits %d\n",
				   brd, brd->brd_slot, slotbits);
		}
	}
	
	err = vector_read_node(vector, source_nasid, 0, RR_GLOBAL_PARMS, &value);
	if (err < 0)
	    goto error;

	value &= ~(RGPARM_CLOCK_MASK);

	value |= (source_port << RGPARM_CLOCK_SHFT);

	TDPRT("Setting RR_GLOBAL_PARMS to 0x%x\n", value); 

	rv = vector_write_node(vector, source_nasid, 0, RR_GLOBAL_PARMS, value);

	if (rv != NET_ERROR_NONE)
		printf("config_router: Error on vector write: %d, vec 0x%x\n",
		       rv, vector);

	TDPRT("Taking care of duplicates\n"); 
	
	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		err = vector_read_node(vector, source_nasid, 0, RR_ERROR_CLEAR(port),
				 &value);
		if (err < 0)
		    goto error;
	}

	/* Clearing klconfig error structure, so we don't see bogus errors */
	bzero((void *) NODE_OFFSET_TO_K1(brd->brd_nasid, 
		router->rou_info.errinfo), sizeof(klrouter_err_t));
	
	if (router->rou_info.struct_version < ROUTER_VECTOR_VERS) {
		if (!router_vers_warning) {
			router_vers_warning = 1;
			printf(" *** Please upgrade your IP27prom to the latest revision\n");
		}
	}
	
	SET_ROU_VECTOR(router, vector);

	check_and_mark_replicas(brd, router, 1);
	return;

error:
	printf("config_router: Error on vector read: %d, vec 0x%x\n",
	       err, vector);
	return;
}

/*
 * Function:		outgoing_port -> Returns the target_rtr's port to which
 *			src_brd is connected
 * Args:		target_rtr -> target rtr's lboard ptr
 *			src_brd -> src brds (RTR or IP27)
 * Returns:		port #, if found
 *			< 0, otherwise
 */

static int
outgoing_port(lboard_t *target_rtr, lboard_t *src_brd)
{
	int		port;
	klrou_t		*roup;

	if (!(roup = (klrou_t *) find_first_component(target_rtr,
			KLSTRUCT_ROU))) {
		printf("WARNING: No ROU info for RTR nic 0x%lx  nasid "
			"%d\n", target_rtr->brd_nic, NASID_GET(target_rtr));
			return -1;
				
	}

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		lboard_t	*l;

		if (roup->rou_port[port].port_nasid == INVALID_NASID)
			continue;

		l = (lboard_t *) NODE_OFFSET_TO_K1(
			roup->rou_port[port].port_nasid, 
			roup->rou_port[port].port_offset);

		if (l->brd_nic == src_brd->brd_nic)
			return port;
	}

	return -1;
}

/*
 * Function:		bfs_find_object -> Given a IP27, calls number_node_board
 *			If it sees a hub connected to itself, calls itself
 *			Given a rtr, configures a rtr and checks all ports; If
 *			there is a IP27 calls itself again; else adds rtr to
 *			queue if rtr hasn't been seen before
 * Args:		brd -> object to be discovered
 *			gdap -> GDA ptr
 *			id -> max cnodeid to be filled up
 *			q -> rtr queue
 *			rou_vrtx -> If a rtr, the rtr queue vertex entry
 * Returns:		Nothing
 */

void
bfs_find_object(lboard_t *brd, 
		gda_t *gdap, 
		cnodeid_t *id, 
		queue_t *q, 
		vrtx_t *rou_vrtx)
{
	if (brd->brd_type == KLTYPE_IP27) {
		klhub_t		*hub_ptr;

		if (!(hub_ptr = (klhub_t *) find_first_component(brd, 
				KLSTRUCT_HUB))) {
			TDPRT("IP27 brd on nasid %d has no HUB!\n",
				brd->brd_nasid);
			return;
		}

		/* Number this node and mark it as visited. */
		number_node_board(brd, id, gdap);

		if (hub_ptr->hub_port.port_nasid != INVALID_NASID) {
			lboard_t	*l;

			l = (lboard_t *) NODE_OFFSET_TO_K1(
				hub_ptr->hub_port.port_nasid, 
				hub_ptr->hub_port.port_offset);

			if (!(l->brd_flags & VISITED_BOARD)) {
				if (l->brd_type == KLTYPE_IP27)
					bfs_find_object(l, gdap, id, q, 
						rou_vrtx);
				else if (KLCLASS(l->brd_type) != 
					KLCLASS_ROUTER)
					printf("Unknown brd type %d "
					"connected to HUB on nasid %d\n", 
					l->brd_type, brd->brd_nasid);
			}
		}
	}
	else if (KLCLASS(brd->brd_type) == KLCLASS_ROUTER) {
		klrou_t		*rou_ptr;
		int		port;

		if (!(rou_ptr = (klrou_t *) find_first_component(brd, 
				KLSTRUCT_ROU))) {
			printf("WARNING: RTR brd nasid %d has no RTR comp!\n",
				brd->brd_nasid);
			return;
		}

		config_router(brd, rou_vrtx->vec, rou_ptr, 
			rou_vrtx->src_port);

		for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
			lboard_t	*l = NULL ;

			if ((rou_ptr->rou_port[port].port_nasid == 
				INVALID_NASID) || 
				((rou_ptr->rou_port[port].port_flag & 
				SN0_PORT_FENCE_MASK) && 
				(l && (l->brd_type != KLTYPE_META_ROUTER))))
				continue;

			l = (lboard_t *) NODE_OFFSET_TO_K1(
				rou_ptr->rou_port[port].port_nasid, 
				rou_ptr->rou_port[port].port_offset);

			if (!(l->brd_flags & VISITED_BOARD)) {
				if (l->brd_type == KLTYPE_IP27)
					bfs_find_object(l, gdap, id, q, 
						NULL);
				else if (KLCLASS(l->brd_type) == 
						KLCLASS_ROUTER) {
					vrtx_t		tmp_vrtx;
	
					tmp_vrtx.rou_brd = l;
					tmp_vrtx.vec = vector_concat(
						rou_vrtx->vec, port);
					tmp_vrtx.src_port = outgoing_port(l, 
						brd);

					if (tmp_vrtx.src_port < 0) {
						printf("WARNING: Cannot find "
						"port on RTR nic 0x%lx on nasid"
						" %d to which brd nasid %d is"
						"connected.\n", l->brd_nic,
						l->brd_nasid, brd->brd_nasid);
						tmp_vrtx.src_port = 0;
					}

					queue_push(q, &tmp_vrtx);

					check_and_mark_replicas(l, rou_ptr,
						0);
				}
				else
					printf("Unknown brd type %d "
					"connected to RTR on nasid %d port "
					"%d\n", l->brd_type, brd->brd_nasid,
					port);
			}
		}
	}
}

/*
 * Function:		bfs_traverse_links -> traverse thro' the entire machine;
 *			Do appropriate tasks for HUBs & RTRs.
 * Args:		brd -> root brd (master IP27)
 *			gdap-> GDA ptr
 *			id -> will fill up max cnodeids assigned
 * Returns:		Nothing
 * Logic:		Sets up the master IP27; Adds root rtr to queue and does
 *			a queue traversal after that
 */

void
bfs_traverse_links(lboard_t *brd, 
		gda_t *gdap,
		cnodeid_t *id)
{
	klhub_t		*hubp;
	lboard_t	*l;
	
	/* Don't mark or traverse a board a second time. */
	if (brd->brd_flags & VISITED_BOARD) {
		TDPRT("Board already visited 0x%x\n", brd);
		return;
	}

	if (brd->brd_type != KLTYPE_IP27)
		return;

	queue_init(&rou_q);

	bfs_find_object(brd, gdap, id, &rou_q, NULL);

	if (!(hubp = (klhub_t *) find_first_component(brd, KLSTRUCT_HUB))) {
		printf("WARNING: No HUB seen on nasid %d\n", 
			brd->brd_nasid);
		return;
	}

	if (hubp->hub_port.port_nasid == INVALID_NASID)
		return;
	
	l = (lboard_t *) NODE_OFFSET_TO_K1(hubp->hub_port.port_nasid, 
			hubp->hub_port.port_offset);

	if (l->brd_type == KLTYPE_IP27)
		return;
	else if (KLCLASS(l->brd_type) == KLCLASS_ROUTER) {
		klrou_t	*roup;
		vrtx_t	tmp_vrtx;

		if (!(roup = (klrou_t *) find_first_component(l, 
			KLSTRUCT_ROU))) {
			printf("WARNING: No RTR comp on RTR nasid %d\n",
				brd->brd_nasid);
			return;
		}

		tmp_vrtx.rou_brd = l;
		tmp_vrtx.vec = 0;
		tmp_vrtx.src_port = outgoing_port(l, brd);

		if (tmp_vrtx.src_port < 0) {
			printf("WARNING: Cannot find port on RTR nic 0x%lx "
			"on nasid %d to which brd nasid %d is connected.\n", 
			l->brd_nic, l->brd_nasid, brd->brd_nasid); 
			tmp_vrtx.src_port = 0;
		}

		queue_push(&rou_q, &tmp_vrtx);

		check_and_mark_replicas(l, roup, 0);
	}
	else {
		printf("WARNING: Unknown brd type %d connected to nasid %d\n",
			l->brd_type, brd->brd_nasid);
		return;
	}

	while (!queue_empty(&rou_q)) {
		vrtx_t	tmp_vrtx;

		queue_pop(&rou_q, &tmp_vrtx);

		bfs_find_object(tmp_vrtx.rou_brd, gdap, id, &rou_q, 
				&tmp_vrtx);
	}
}

static int
hub_rtc_err(lboard_t *lboard)
{
	klcpu_t		*cpu;
	int		found;

	cpu = NULL;
	found = 0;
	while ((cpu = (klcpu_t *) find_component(lboard, 
		(klinfo_t *) cpu, KLSTRUCT_CPU)))
		if (!(cpu->cpu_info.flags & KLINFO_ENABLE) &&
			(cpu->cpu_info.diagval == KLDIAG_RTC_DIST))
			found = 1;

	return found;
}

#define	HUB_INT_CLOCK(brd)	\
		(REMOTE_HUB_L(brd->brd_nasid, PI_RT_LOCAL_CTRL) & \
		PRLC_USE_INT_MASK)

/*
 * Even if we find one HUB on this router who receives the clock properly
 * we don't have an error
 */

static int
rtr_rtc_err(lboard_t *router)
{
	int	port;
	klrou_t	*klrou;

	if (!(klrou = (klrou_t *) find_first_component(router, KLSTRUCT_ROU)))
		return 1;

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		lboard_t	*hub;

		if (klrou->rou_port[port].port_nasid == INVALID_NASID)
			continue;

		hub = (lboard_t *) NODE_OFFSET_TO_K1(
			klrou->rou_port[port].port_nasid, 
			klrou->rou_port[port].port_offset);

		if ((hub->brd_type == KLTYPE_IP27) && (!hub_rtc_err(hub) ||
				HUB_INT_CLOCK(hub)))
			return 0;
	}

	return 1;
}

static int
rtc_err_link(lboard_t *start,
             lboard_t *ip27,
             lboard_t **upstream,
             lboard_t **downstream)
{
	net_vec_t	vec;

	*upstream = start;
	*downstream = ip27;

	while (1) {
		__uint64_t	rr_global_parms;
		int		port;
		klrou_t		*router;

		if (!rtr_rtc_err(*upstream))
			return 0;

		vec = klcfg_discover_route(ip27, *upstream, 0);
		if (vec == NET_VEC_BAD)
			return 1;

		if (vector_read(vec, 0, RR_GLOBAL_PARMS, &rr_global_parms) < 0)
			return 1;

		port = (rr_global_parms & RGPARM_CLOCK_MASK) >> 
			RGPARM_CLOCK_SHFT;

		*downstream = *upstream;
		if (!(router = (klrou_t *) find_first_component(*upstream,
			KLSTRUCT_ROU)))
			return 1;
		*upstream = (lboard_t *) NODE_OFFSET_TO_K1(
			router->rou_port[port].port_nasid,
			router->rou_port[port].port_offset);
	}
}

void
check_rtc_tree(nasid_t master_nasid, gda_t *gdap, int maxnodes)
{
	int	cnode;

	for (cnode = 0; cnode < maxnodes; cnode++) {
		nasid_t		nasid;
		lboard_t	*lboard, *conn_brd;
		lboard_t	*link1, *link2;
		klhub_t		*hub;
		int		port;

		nasid = gdap->g_nasidtable[cnode];

		if (!(lboard = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid),
			KLTYPE_IP27)))
			continue;

		if (!hub_rtc_err(lboard))
			continue;

		printf("/hw/module/%d/slot/n%d: ERROR: RTC is not "
			"advancing\n", lboard->brd_module, 
			SLOTNUM_GETSLOT(lboard->brd_slot));

		if (nasid == master_nasid) {
			printf("\tCheck board /hw/module/%d/slot/n%d\n",
				lboard->brd_module, 
				SLOTNUM_GETSLOT(lboard->brd_slot));
			continue;
		}

		if (!(hub = (klhub_t *) find_first_component(lboard, 
				KLSTRUCT_HUB)))
			continue;

		if (hub->hub_port.port_nasid == INVALID_NASID) {
			printf("\tCheck board /hw/module/%d/slot/n%d\n",
				lboard->brd_module, 
				SLOTNUM_GETSLOT(lboard->brd_slot));
			continue;
		}

		conn_brd = (lboard_t *) NODE_OFFSET_TO_K1(
			hub->hub_port.port_nasid, hub->hub_port.port_offset);

		if (conn_brd->brd_type == KLTYPE_IP27) {
			printf("\tCheck board /hw/module/%d/slot/n%d\n",
				conn_brd->brd_module, 
				SLOTNUM_GETSLOT(conn_brd->brd_slot));
			continue;
		}

		if (KLCLASS(conn_brd->brd_type) != KLCLASS_ROUTER)
			continue;

		if (rtc_err_link(conn_brd, lboard, &link1, &link2))
			printf("\tUnable to isolate problem link\n");
		else
			printf("\tCheck CrayLink /hw/module/%d/slot/%c%d"
				" <--> /hw/module/%d/slot/%c%d\n",
				link1->brd_module,
				link1->brd_type == KLTYPE_IP27 ? 'n' : 'r',
				SLOTNUM_GETSLOT(link1->brd_slot),
				link2->brd_module,
				link2->brd_type == KLTYPE_IP27 ? 'n' : 'r',
				SLOTNUM_GETSLOT(link2->brd_slot));
	}
}

void
synchronize_clocks(nasid_t nasid, gda_t *gdap, int maxnodes)
{
	hubreg_t clock_control;
	hubreg_t clock_value, master_time;
	cnodeid_t cnode;
	nasid_t curnasid;
	int i;

	/* Stop the clocks. */
	clock_control = REMOTE_HUB_L(nasid, PI_RT_LOCAL_CTRL);
	clock_control &= ~(PRLC_GCLK_EN | PRLC_USE_INT);
	REMOTE_HUB_S(nasid, PI_RT_LOCAL_CTRL, clock_control);

	master_time = REMOTE_HUB_L(nasid, PI_RT_COUNT);

	/* Synchronize the clocks. */
	for (cnode = 0; cnode < maxnodes; cnode++) {
		curnasid = gdap->g_nasidtable[cnode];
		if (curnasid == nasid)
			continue;
		REMOTE_HUB_S(curnasid, PI_RT_COUNT, master_time);
	}

	/* Start 'em back up.  This time in sync. */  
	clock_control |= (PRLC_GCLK_EN | PRLC_USE_INT_MASK);
	REMOTE_HUB_S(nasid, PI_RT_LOCAL_CTRL, clock_control);

	/* Wait 5 microseconds. */
	us_delay(5);

	for (cnode = 0; cnode < maxnodes; cnode++) {
		curnasid = gdap->g_nasidtable[cnode];
		
		if (REMOTE_HUB_L(curnasid, PI_RT_COUNT) <= master_time) {
			klcpu_t *acpu;
			cpuid_t cpu;
			char reason[128];
                        lboard_t *brd;

			sprintf(reason, "RTC is not advancing on NASID %d, "
				"due to CrayLink problems.\n", curnasid);

			/* Disable the processors temporarily in h/w */

			ed_cpu_mem(curnasid, "DisableA", NULL, reason, 1, 0);
			ed_cpu_mem(curnasid, "DisableB", NULL, reason, 1, 0);

			/* Disable the processors in software */

			if (brd = find_lboard((lboard_t *) 
				KL_CONFIG_INFO(curnasid), KLTYPE_IP27)) {
				acpu = NULL;
				while (acpu = (klcpu_t *) find_component(brd, 
					(klinfo_t *) acpu, KLSTRUCT_CPU)) {
					acpu->cpu_info.flags &= ~KLINFO_ENABLE;
					acpu->cpu_info.diagval = KLDIAG_RTC_DIST;
				}
			}
		}
	}

	check_rtc_tree(nasid, gdap, maxnodes);
}

int
update_modnum_node(lboard_t *brd, int modid)
{

	while (brd) {
		if ((brd->brd_type != KLTYPE_IP27) &&
		    (KLCLASS(brd->brd_type) != KLCLASS_ROUTER))
		    brd->brd_module = modid ;
		brd = KLCF_NEXT(brd);
	}

	return 1 ;
}

int
update_modnum_io(gda_t *gdap, partid_t partition)
{
        cnodeid_t cnode;
        nasid_t nasid;
        lboard_t *brd;

        for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
                nasid = gdap->g_nasidtable[cnode];
                if (nasid == INVALID_NASID)
                        continue;
                brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
                                  KLTYPE_IP27);
                if (!brd) {
                        printf("number_modules: Error - No IP27 on nasid %d!\n",                                nasid);
                        continue;
                }
                if (brd->brd_module >= 0) {
			update_modnum_node(brd, brd->brd_module) ;
		}
	}

	return 1 ;
}


void
unmark_all_boards(gda_t *gdap, int numnodes)
{
	lboard_t *brd;
	nasid_t nasid;
	int cnode;

	for (cnode = 0; cnode < numnodes; cnode ++) {
		nasid = gdap->g_nasidtable[cnode];
		if (nasid == INVALID_NASID)
			break;
		brd = (lboard_t *)KL_CONFIG_INFO(nasid);

		while (brd) {
			brd->brd_flags &= ~VISITED_BOARD;
			brd = KLCF_NEXT(brd);
		}
	}
}

/*
 * Function:		launch_set_module -> launches a CPU in slave brd
 *			to set moduleid in the ELSC
 * Args:		slave_brd -> slave brd ptr
 *			module -> moduleid to set
 * Returns:		>= 0 if all right
 *			< 0 if something's wrong
 */

int launch_set_module(lboard_t *slave_brd, moduleid_t module)
{
	klcpu_t		*cpu_ptr = (klcpu_t *) NULL;
	int		i, launched = 0;

	if (!slave_brd)
		return -1;

	if (!(slave_brd->brd_flags & ENABLE_BOARD))
		return -1;

	for (i = 0; i < CPUS_PER_NODE; i++) {
		cpu_ptr = (klcpu_t *) find_component(slave_brd, (klinfo_t *) 
			cpu_ptr, KLSTRUCT_CPU);

		if (!cpu_ptr)
			return -1;

		if (!KLCONFIG_INFO_ENABLED((klinfo_t *) cpu_ptr))
			continue;

		LAUNCH_SLAVE(slave_brd->brd_nasid, cpu_ptr->cpu_info.physid, 
			(launch_proc_t) PHYS_TO_K0(kvtophys(
			(void *)slave_elsc_module_set)), get_nasid(), (void *) 
			TO_NODE(slave_brd->brd_nasid, IP27PROM_STACK_A + 1024),
			(void *) (__psunsigned_t) module);

			launched = 1;

		break;
	}

	if (launched)
		return 0;
	else
		return -1;
}

/*
 * Function:		re_number_modules -> re-number modules afresh
 *			beginning with 1 in the current module
 * Args:		gda ptr
 * Returns:		Nothing. Resets system.
 */

void re_number_modules(gda_t *gdap)
{
        lboard_t        *mod_brds[NODESLOTS_PER_MODULE];
	int		i, module;
	char		buf[8];
	nasid_t		m_nasid = get_nasid();
	char		brds_to_visit[MAX_NASIDS];

	/*
	 * Do the printf statements early to make sure they make it to the
	 * console before the reset
	 */
        printf("****************************************************************\n");
        printf("*          Inconsistency detected in module numbering.         *\n");
	printf("*                Automatically renumbering modules.            *\n");
	if (!SN00)
        	printf("*    You can assign globally unique module id(s) in the MSC.   *\n");
	else
        	printf("* You can assign moduleids using the module command at the POD *\n");
	printf("*                       Resetting system....                   *\n");
        printf("****************************************************************\n");
	for (i = 0; i < MAX_COMPACT_NODES; i++)
		if (gdap->g_nasidtable[i] != INVALID_NASID)
			brds_to_visit[gdap->g_nasidtable[i]] = 1;
		else
			brds_to_visit[gdap->g_nasidtable[i]] = 0;

	module = 1;

	sprintf(buf, "%d", module);

	/* First  assign current module */
	io6prom_elsc_init();

	if (module_brds(m_nasid, mod_brds, NODESLOTS_PER_MODULE) < 0)
		printf("ERROR: Trying to re-number modules...\n");
	else {
		for (i = 0; i < NODESLOTS_PER_MODULE; i++)
			if (mod_brds[i]) {
				if (SN00) {
					if (ip27log_setenv(
					mod_brds[i]->brd_nasid, 
					IP27LOG_MODULE_KEY, buf, 0) < 0) {
						printf("*** Cannot re-number "
						"on nasid %d. ip27log is "
						"either corrupt or "
						"uninitialized.\n",
						mod_brds[i]->brd_nasid);
						printf("*** Try doing a "
						"initlog or a initalllogs "
						"from POD.\n");
						pod_mode();
					}
				}
				else if (mod_brds[i]->brd_nasid == 
						get_nasid()) {
					if (elsc_module_set(get_elsc(),
							    module) < 0)
						printf("ERROR: Setting moduleid!. You MUST set it through the MSC.\n");
				}

				brds_to_visit[mod_brds[i]->brd_nasid] 
					= 0;
			}
	}

	module++;

	/* Assign all other modules */
	for (i = 0; i < MAX_COMPACT_NODES; i++) {
		int		j;
		lboard_t	*slave_brd;

		if ((gdap->g_nasidtable[i] == INVALID_NASID) ||
			(!brds_to_visit[gdap->g_nasidtable[i]]))
			continue;

		sprintf(buf, "%d", module);

		if (module_brds(gdap->g_nasidtable[i], mod_brds, 
					NODESLOTS_PER_MODULE) < 0)
			printf("ERROR: Trying to re-number modules...\n");
		else {
			int	elsc_set = 0;

			for (j = 0; j < NODESLOTS_PER_MODULE; j++)
				if (mod_brds[j]) {
					if (SN00) {
						if (ip27log_setenv(
						mod_brds[j]->brd_nasid,
						IP27LOG_MODULE_KEY, buf, 0) 
						< 0) {
						printf("*** Cannot re-number "
						"on nasid %d. ip27log is "
						"either corrupt or "
						"uninitialized.\n",
						mod_brds[j]->brd_nasid);
						printf("*** Try doing a "
						"initlog or a initalllogs "
						"from POD.\n");
						pod_mode();
						}
					}
					else if (!elsc_set)
						if (launch_set_module(
							mod_brds[j], module) 
							>= 0)
							elsc_set = 1;

					brds_to_visit[mod_brds[j]->brd_nasid] 
						= 0;
				}

			if (!SN00 && !elsc_set)
				printf("ERROR: Setting moduleid!. You MUST set it through the MSC.\n");
		}

		module++;
	}

	cpu_hardreset();
}

/*
 * Function:		highest_module
 * Args:		gdaptr
 * Returns:		highest module id
 */

moduleid_t highest_module(gda_t *gdap)
{
        int             i;
        moduleid_t      high = 0;

        for (i = 0; i < MAX_COMPACT_NODES; i++) {
                nasid_t         nasid;
                lboard_t        *brd;

                nasid = gdap->g_nasidtable[i];
                if (nasid == INVALID_NASID)
                        continue;

                brd = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid),
                        KLTYPE_IP27);
                if (!brd) {
                        printf("highest_module: Error - No IP27 on nasid "
                                "%d!\n", nasid);
                        continue;
                }

                if (brd->brd_module > high)
                        high = brd->brd_module;
        }

        return high;
}

/*
 * Function:		check_modules
 * Args:		gda_t	* -> gda
 * Returns:		0 if successful
 *			-1 if failed
 *			-2 if old version of IP27prom
 * Checks:		All modules have consistent module id
 *			All modules (as far as we can see) have unique modids
 *			If any module(s) have 0 as modids, assign new modids(s)
 */

int check_modules(gda_t *gdap)
{
        lboard_t        *mod_brds[NODESLOTS_PER_MODULE];
        char            brds_to_visit[MAX_NASIDS];
        char            modids_used[MAX_MODULE_ID + 1];
        int             i, panictype, new_module = 0;
        moduleid_t      high;

        high = highest_module(gdap) + 1;

        bzero(brds_to_visit, MAX_NASIDS);
        bzero(modids_used, MAX_MODULE_ID + 1);

        /* all brds need to be visited */
        for (i = 0; i < MAX_COMPACT_NODES; i++)
                if (gdap->g_nasidtable[i] != INVALID_NASID)
                        brds_to_visit[gdap->g_nasidtable[i]] = 1;

        for (i = 0; i < MAX_COMPACT_NODES; i++) {
                nasid_t         nasid;
                moduleid_t      module;
                int             j;

                nasid = gdap->g_nasidtable[i];
                if ((nasid == INVALID_NASID) || !brds_to_visit[nasid])
                        continue;

                if (module_brds(nasid, mod_brds, NODESLOTS_PER_MODULE) < 0) {
                        TDPRT("check_modules: Cannot get module boards for "
                                "nasid %d\n", nasid);
                        continue;
                }

                module = -1;

                for (j = 0; j < NODESLOTS_PER_MODULE; j++)
                        if (mod_brds[j]) {
                                module = mod_brds[j]->brd_module;
                                break;
                        }

                if (module < 0) {
                        TDPRT("check_modules: mod_brds didn't find any "
                                "modules for nasid %d\n", nasid);
                        continue;
                }

                /* Check if all module brds have same module id */
                for (j = 0; j < NODESLOTS_PER_MODULE; j++) {
                        if (mod_brds[j]) {
                                if (module != mod_brds[j]->brd_module) {
                                        TDPRT("check_modules: modids not all"
                                        " same for nasid %d slot n%d\n",
                                        mod_brds[j]->brd_nasid,
                                        mod_brds[j]->brd_slot);
					panictype = 1;
                                        goto panic;
                                }
                        }
                }

                /* Check if module id has been used previously */
                if (module && modids_used[module]) {
			int	opt;

			printf("*** PANIC: Different modules have the same moduleid.\n");
			printf("*** PANIC: Renumbering modules may destroy old config. information.\n");
			printf("*** PANIC: Do you wish to renumber? [n] ");

			opt = cpu_errgetc();
			printf("%c\n", opt);

			if ((opt == 'y') || (opt == 'Y'))
				re_number_modules(gdap);

			panictype = 3;
			goto panic;
                }

                /* module id is zero! assign it */
                if (!module) {
                        char    buf[8];

			if (high > MAX_MODULE_ID) {
				if (SN00)
					re_number_modules(gdap);

				panictype = 2;
				goto panic;
			}

                        new_module = 1;

                        for (j = 0; j < NODESLOTS_PER_MODULE; j++)
                                if (mod_brds[j])
                                        break;

                        TDPRT("check_modules: module id not assigned for "
                                " brds on nasid %d in slot n%d\n",
                                mod_brds[j]->brd_nasid, mod_brds[j]->brd_slot);
                        TDPRT("check_modules: Assigning new module id %d\n",
                                high);
                        /* XXX: Must detect if fence and refuse to go fwd */
                        TDPRT("check_modules: WARNING: This will NOT work "
                                "on a partitioned machine\n");

                        sprintf(buf, "%d", high);

                        for (j = 0; j < NODESLOTS_PER_MODULE; j++)
                                if (mod_brds[j]) {
                                        mod_brds[j]->brd_module = high;

					if (ip27log_setenv(
						mod_brds[j]->brd_nasid,
						IP27LOG_MODULE_KEY, buf, 0) 
						< 0) {
						printf("*** Cannot assign "
						"moduleid on nasid %d. ip27log"
						" is either corrupt or "
						"uninitialized.\n", 
						mod_brds[j]->brd_nasid);
						printf("*** Try doing a "
						"initlog or a initalllogs "
						"from POD.\n");
						pod_mode();
					}
                                }

                        module = high;
                	high++;
                }

                modids_used[module] = 1;

                for (j = 0; j < NODESLOTS_PER_MODULE; j++)
                        if (mod_brds[j])
                                brds_to_visit[mod_brds[j]->brd_nasid] = 0;
        }

        if (new_module) {
                printf("***************************************************************\n");
                printf("*  Some module(s) did not have module id(s). New module id(s) *\n");
                printf("*               assigned by default. Resetting system.        *\n");
		if (SN00)
        		printf("* You can assign moduleids using the module command at the POD*\n");
		else
			printf("*   You can assign globally unique module id(s) in the MSC.   *\n");
                printf("***************************************************************\n");
                cpu_hardreset();
        }
        else
                return 0;

panic:

        printf("****************************************************************\n");
	if (panictype == 1)
        	printf("*    PANIC: Boards in same module show different moduleids.    *\n");
	else if (panictype == 2)
		printf("*      PANIC: Some modules detected without a moduleid.        *\n");
	else if (panictype == 3) {
		printf("*              PANIC: Duplicate moduleid(s) detected           *\n");
		printf("*           PANIC: Please assign moduleid(s) at the MSC.       *\n");
		printf("  PANIC: You can do this by the 'mod' command at the MSC or -  *\n");
		printf("       PANIC: - using the \"module\" command at the IP27 POD.    *\n");
	}

	printf("*      PANIC: Failed to automatically assign moduleid(s)       *\n");
	if (SN00)
        	printf("* You can assign moduleids using the module command at the POD*\n");
	else
        	printf("*    Please assign globally unique module id(s) at the MSC.   *\n");
        printf("****************************************************************\n");
        pod_mode();
        return -1; /* Never reached */
}

int valid_serial_num(klmod_serial_num_t *serial_ptr)
{
	char		temp[MAX_SERIAL_NUM_SIZE];

	decode_str_serial(serial_ptr->snum.snum_str, temp);

	return (temp[0] == 'K');
}

int touch_module_nic(nasid_t nasid)
{
	klmod_serial_num_t	*serial_comp, *serial_in_brd;
	lboard_t		*brds_in_module[NODESLOTS_PER_MODULE];
	int			i, found;
	lboard_t		*l;

	if (SN00)
		return 0;

	if (!(l = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid),
			KLTYPE_MIDPLANE))) {
		TDPRT("Can't find MIDPLANE lboard on nasid %d\n", nasid);
		return -1;
	}

	serial_comp = NULL;

	if (!(serial_comp = GET_SNUM_COMP(l))) {
		TDPRT("WARNING: Cannot find serial # comp. on midplane brd "
			"on nasid %d\n", nasid);
		return -1;
	}

	if (valid_serial_num(serial_comp))
		return 0;

	/* Invalid serial #. Need to update */
	if (module_brds(nasid, brds_in_module, NODESLOTS_PER_MODULE) 
			< 0) {
		TDPRT("WARNING: module_brds failed for /hw/module/%d/slot/n%d\n"
			, l->brd_module, SLOTNUM_GETSLOT(l->brd_slot));
		return -1;
	}

	/*
	 * Search the rest of the brds in the module to see if anyone has a
	 * valid serial #
	 */

	found = 0;

	for (i = 0; i < NODESLOTS_PER_MODULE; i++)
		if (brds_in_module[i]) {
			lboard_t		*midplane;

			if (get_nasid() == brds_in_module[i]->brd_nasid)
				continue;

			if (!(midplane = find_lboard((lboard_t *) 
				KL_CONFIG_INFO(brds_in_module[i]->brd_nasid), 
				KLTYPE_MIDPLANE)))
				continue;

			serial_in_brd = NULL;

			if (!(serial_in_brd = GET_SNUM_COMP(midplane))) {
				TDPRT("WARNING: Cannot find component of type "
				"serial # on /hw/module/%d/slot/n%d\n", 
				brds_in_module[i]->brd_module, 
				SLOTNUM_GETSLOT(brds_in_module[i]->brd_slot));
				continue;
			}

			if (valid_serial_num(serial_in_brd)) {
				found = 1;
				break;
			}
		}

	if (!found) {
		TDPRT("WARNING: Board with nasid %d does NOT have a serial"
			"number. May fail to boot.\n", l->brd_nasid);

		return -1;
	}
	else {
		bcopy(&(serial_in_brd->snum), &(serial_comp->snum),
			sizeof(serial_comp->snum));

		return 0;
	}
}

int update_serial_number(gda_t *gdap)
{
	int		i;

	for (i = 0; i < MAX_COMPACT_NODES; i++)
                if (gdap->g_nasidtable[i] != INVALID_NASID)
			touch_module_nic(gdap->g_nasidtable[i]);
			

	return 0;
}

static int
majority(int *votes, int n)
{
	int	i, aye = 0, no = 0;

	for (i = 0; i < n; i++)
		if (votes[i] != -1) {
			if (votes[i])
				aye++;
			else
				no++;
		}

	return (aye > no);
}

#define	ID_DOMAIN	1
#define	ID_CLUSTER	2
#define	ID_CELL		3

static int
voteid(nasid_t nasid, int type)
{
	int		i;
	char		*key;
	lboard_t	*mod_brds[NODESLOTS_PER_MODULE];
	int		ids[NODESLOTS_PER_MODULE];

	if (module_brds(nasid, mod_brds, NODESLOTS_PER_MODULE) < 0) {
		printf("Error trying to find all boards in module with nasid"
			"%d\n", nasid);
		return 0;
	}

	switch (type) {
	case ID_DOMAIN:
		key = IP27LOG_DOMAIN;
		break;
	case ID_CLUSTER:
		key = IP27LOG_CLUSTER;
		break;
	case ID_CELL:
	default:
		key = IP27LOG_CELL;
		break;
	}

	for (i = 0; i < NODESLOTS_PER_MODULE; i++)
		if (mod_brds[i]) {
			char	buf[8];

			ip27log_getenv(mod_brds[i]->brd_nasid, key, 
				buf, "0", 0);
			ids[i] = atoi(buf);
		}

	for (i = 0; i < NODESLOTS_PER_MODULE; i++)
		if (mod_brds[i]) {
			int	j, votes[NODESLOTS_PER_MODULE];

			for (j = 0; j < NODESLOTS_PER_MODULE; j++)
				if (mod_brds[j]) {
					if (ids[i] == ids[j])
						votes[j] = 1;
					else
						votes[j] = 0;
				}
				else
					votes[j] = -1;

			if (majority(votes, NODESLOTS_PER_MODULE))
				return ids[i];
		}

	return 0;
}

void
update_part_kldir(gda_t *gdap)
{
	int		i;
	nasid_t		nasid;

	for (i = 0; i < MAX_COMPACT_NODES; i++)
                if ((nasid = gdap->g_nasidtable[i]) != INVALID_NASID) {
			elsc_t		e;
			lboard_t	*l;
			kldir_ent_t	*kld;
			klp_t		*klp;
			int		domain, cluster, cell;

			if (!(l = find_lboard((lboard_t *) 
					KL_CONFIG_INFO(nasid), KLTYPE_IP27)))
				continue;

			kld = KLD_KERN_PARTID(nasid);
			/* XXX: Pointer might not point to the end of kldir */
			kld->pointer = (__psunsigned_t) kld->rsvd;
			klp = (klp_t *) kld->pointer;

			kld->magic	= KLDIR_MAGIC ;
			klp->klp_version = KLP_VERSION;
			/* Do not change master's state, till it gets
			   to the very end. This is because we may
			   have to reset the machine if SCSI DMA fails
			   or something else, and anyone who has read
			   this state meanwhile thinks we are in the PROM. */
			if (nasid != get_nasid())
				klp->klp_state = KLP_STATE_PROM;
			klp->klp_cpuA = REMOTE_HUB_L(nasid, PI_CPU_ENABLE_A);
			klp->klp_cpuB = REMOTE_HUB_L(nasid, PI_CPU_ENABLE_B);
			/* XXX: Need to change this for SN1 */
			klp->klp_cpuC = klp->klp_cpuD = 0;
			klp->klp_module = l->brd_module;
			klp->klp_partid = l->brd_partition;

			if (SN00) {
				char	buf[8];

				ip27log_getenv(nasid, IP27LOG_DOMAIN, buf, "0",
					0);
				klp->klp_domainid = atoi(buf);
				ip27log_getenv(nasid, IP27LOG_CLUSTER, buf, "0",
					0);
				klp->klp_cluster = atoi(buf);
				ip27log_getenv(nasid, IP27LOG_CELL, buf, "0",
					0);
				klp->klp_cellid = atoi(buf);
			}
			else {
				elsc_init(&e, nasid);

				klp->klp_domainid = elsc_domain_get(&e);
				klp->klp_cluster = elsc_cluster_get(&e);
				klp->klp_cellid = elsc_cell_get(&e);

				if (klp->klp_domainid == 0xff)
					klp->klp_domainid = voteid(nasid, 
						ID_DOMAIN);
				if (klp->klp_cluster == 0xff)
					klp->klp_cluster = voteid(nasid, 
						ID_CLUSTER);
				if (klp->klp_cellid == 0xff)
					klp->klp_cellid = voteid(nasid, 
						ID_CELL);
			}

#define	ID_HACK		1
#if ID_HACK
			/*
			 * XXX: Hack for now: domainid, cellid and
			 * clusterid are the same as partid
			 */
#if 0

			klp->klp_domainid = klp->klp_partid;
			klp->klp_cluster = klp->klp_partid;
#endif
			klp->klp_cellid = klp->klp_partid;

			if (!SN00) {
#if 0
				elsc_domain_set(&e, klp->klp_domainid);
				elsc_cluster_set(&e, klp->klp_cluster);
#endif
				elsc_cell_set(&e, klp->klp_cellid);
			}
#endif
		}
}

int
node_probe(lboard_t *start, gda_t *gdap, partid_t partition)
{
	cnodeid_t current_cnodeid = 0;
	lboard_t *brd;
	nasid_t master_nasid = get_nasid();
	int i;

	for (i = 0; i < MAX_NASIDS; i++)
	    node_table[i] = INVALID_NASID;

	/* Find our IP27 board structure to start with. */
	brd = find_lboard(start, KLTYPE_IP27);

	/* If we couldn't find a node board, we fail. */
	if (!brd) {
		printf("node_probe: Error - No master node (nasid 0x%x)\n",
			master_nasid);
		return 0;
	}

	bfs_traverse_links(brd, gdap, &current_cnodeid);
	
	brd = KLCF_NEXT(brd);
	while((brd) && (brd = find_lboard(brd, KLTYPE_IP27))) {
		number_node_board(brd, &current_cnodeid, gdap);
		brd = KLCF_NEXT(brd);
	}
	
		

	/* Make sure the master board is numbered zero. */
	if (gdap->g_nasidtable[0] != master_nasid)
		printf("node_probe: Error - Master hub NASID == 0x%x "
			"not 0x%x\n", gdap->g_nasidtable[0],
			master_nasid);

	number_cpus_and_nodes(gdap, current_cnodeid);

	/* We're the master node so we get to be clock master! */
	synchronize_clocks(master_nasid, gdap, current_cnodeid);


	/* 12p 4io config has only one module always */
	/* Number the modules. */
	if (!CONFIG_12P4I)
		check_modules(gdap);
	
	/* update the module numbers to IO boards also */

	update_modnum_io(gdap, partition);

	if (!CONFIG_12P4I)
	/* Update module serial numbers */
		update_serial_number(gdap);

	/* Update kldir part entry */
	update_part_kldir(gdap);

	/* Unmark all fo the nodes so that the prom can be run again. */
	unmark_all_boards(gdap, current_cnodeid);

	return current_cnodeid;	/* number of nodes in the partition */
}


int klcfg_consist_check(nasid_t nasid)
{
    lboard_t *brd;
    klrou_t *rou;
    int port_flag;
    int disabled_brd_count = 0;
    int i;
    brd = (lboard_t *)KL_CONFIG_INFO(nasid);
    while( brd)
    {
	if(((signed char)(brd->brd_module) >= 90 ) 
	 && (brd->brd_type != KLTYPE_META_ROUTER)) {
            printf("***Warning: board %x in nasid %d has invalid module id %d\n", brd->brd_nic, brd->brd_nasid, brd->brd_module);
	}
        if((signed char)(brd->brd_module) <= 0)
        {
            printf("***Warning: board %x in nasid %d has invalid module id %d\n", brd->brd_nic, brd->brd_nasid, brd->brd_module);
	    brd->brd_module = 0;
        }
        if((signed char)(brd->brd_partition) < 0)
        {
           printf("***Warning: board %x in nasid %d has invalid partition id\n",
		 brd->brd_nic, brd->brd_nasid);
        }
        if(KLCF_CLASS(brd) == KLCLASS_ROUTER)
        {
            rou = (klrou_t *) find_first_component(brd,KLSTRUCT_ROU);
	    if (! (rou->rou_flags & PCFG_ROUTER_META)) 
	    {
               if(rou == NULL) {
                   printf("***Warning: Router board %x has no router chip\n", 
				brd->brd_nic);
               } else {
                   port_flag = 0;
                   for(i = 1; i< MAX_ROUTER_PORTS + 1; i++) {
                       if(rou->rou_port[i].port_nasid != INVALID_NASID)
                           port_flag = 1;
                   }
                   if(port_flag == 0) {
                        printf("***Warning: router %x is not connected on any port\n",brd->brd_nic);
                   }
            	}
	    }
        }
	brd = KLCF_NEXT(brd);
    }
    return 0;
}

