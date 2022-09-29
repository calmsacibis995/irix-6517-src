/***********************************************************************\
 *      File:           partition.c                                     *
 *                                                                      *
 *      Contains the logic for discovering, checking and managing the   *
 *      system's partition information.                                 *
 *                                                                      *
 \***********************************************************************/

#include <sys/types.h>
#include <ksys/elsc.h>
#include <hub.h>
#include <libkl.h>
#include <sys/SN/SN0/ip27log.h>
#include "ip27prom.h"
#include "discover.h"
#include "partition.h"
#include "libc.h"
#include "pod.h"

extern int check_console(pcfg_hub_t *hub);
extern void restore_restart_state(pcfg_t *p);
static int
in_src_partition(pcfg_t *p, int src, net_vec_t vec)
{
    pcfg_router_t       *rou_cf = &p->array[src].router;
    partid_t            partition = p->array[src].router.partition;
    int                 i;

    for (i = 0; i < vector_length(vec); i++) {
        int     port = (int) vector_get(vec, i);

        if (IS_META(rou_cf)) {
            if (rou_cf->partition && (rou_cf->partition != partition))
                return 0;
        }
        else {
            if (rou_cf->partition != partition)
                return 0;
        }

        rou_cf = &p->array[rou_cf->port[port].index].router;
    }

    return 1;
}

static int
power_of_2(__uint64_t n)
{
    int         i, one_seen = 0;

    for (i = 0; i < 8 * sizeof(__uint64_t); i++)
        if (n & ((__uint64_t) 0x1 << i)) {
            if (one_seen)
                return 0;
            else
                one_seen = 1;
        }

    return 1;
}

static partid_t
get_num_modules_part(pcfg_t *p, partid_t partition)
{
    char        module_bit_map[MAX_MODULES];
    int         i, n = 0;

    bzero(module_bit_map, MAX_MODULES);

    /* XXX: Doesn't take care of totally brainless modules */

    for (i = 0; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
                (p->array[i].hub.partition == partition) &&
                !module_bit_map[p->array[i].hub.module]) {
            n++;
            module_bit_map[p->array[i].hub.module] = 1;
        }

    return n;
}

static int
running_partition(pcfg_t *p, partid_t partition)
{
    int         i, found = 0;

    for (i = 0; i < p->count; i++) {
        if (p->array[i].any.type == PCFG_TYPE_HUB) {
            if (!IS_RESET_SPACE_HUB(&p->array[i].hub) &&
                        (p->array[i].hub.partition == partition)) {
                printf("*** HUB nasid %d has the same partitionid %d\n",
                        p->array[i].hub.nasid, partition);

                found = 1;
                break;
            }
        }
        else {
            if (IS_META(&p->array[i].router))
                continue;

            if (!IS_RESET_SPACE_RTR(&p->array[i].router) &&
                        (p->array[i].router.partition == partition)) {
                printf("*** RTR nic 0x%lx, module %d, slot %d  has the same partitionid %d\n",
                        p->array[i].router.nic, 
			p->array[i].router.module,
			p->array[i].router.slot,
			partition);

                found = 1;
                break;
            }
        }
    }

    return found;
}

/*
 * Function:		pttn_validate -> Checks pcfg to validate a partition
 * Args:		pcfg_t * -> pcfg ptr
 * Returns:		PARTITION_YES -> Valid partition found
 *			PARTITION_INVAL -> Invalid partition config
 */

int
pttn_validate(pcfg_t *p)
{
    int         i;

    printf("Checking partitioning information .........\t\t");

    /*
     * Some partition info. available.
     * Need to check the Golden Rules of Partitioning
     * 1. Sanity check
     * 2. Partitionid musn't conflict with a existing partition
     */

    /*
     * 1. Power of 2 # of modules
     *          There're actually 2 parts to this.
     *          a) Check to see if both routers in a module have the
     *                  same partitionids
     */

    for (i = 0; i < p->count; i++) {
        partid_t        n_modules;

        if (p->array[i].any.type != PCFG_TYPE_ROUTER)
            continue;
        else if (IS_META(&p->array[i].router))
            continue;

        if ((p->array[i].router.port[6].index != PCFG_INDEX_INVALID) &&
                (p->array[i].router.partition !=
                p->array[p->array[i].router.port[6].index].router.partition)) {
		printf("\n") ;
            printf("RTR NIC 0x%lx does not have the same partitionid as "
                "RTR NIC 0x%lx in same module\n", p->array[i].router.nic,
                p->array[p->array[i].router.port[6].index].router.nic);
            return PARTITION_INVAL;
        }
    }

    /*
     * 2. Partitionid musn't conflict with a existing partition
     */

    for (i = 0; i < p->count; i++) {
        partid_t        partition;

        if (p->array[i].any.type == PCFG_TYPE_HUB) {
            if ((p->array[i].hub.flags & PCFG_HUB_HEADLESS) ||
                        !IS_RESET_SPACE_HUB(&p->array[i].hub))
                continue;

            partition = p->array[i].hub.partition;
        }
        else if (!IS_RESET_SPACE_RTR(&p->array[i].router) ||
                IS_META(&p->array[i].router))
            continue;
        else {
            partition = p->array[i].router.partition;
        }

        /*
         * There's a HUB or RTR with a valid partitionid. Is there a
         * running HUB or RTR with the same partitionid ?
         */

        if (partition > 0 && running_partition(p, partition)) {
		printf("\n") ;
            printf("Invalid partition %d. Running pttn has same pttn id.\n",
		partition);
            return PARTITION_INVAL;
        }
    }

    printf("DONE\n") ;
    return PARTITION_YES;
}

static int
get_conn_port(pcfg_t *p, int tgt, int src)
{
    int         port;

    for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
        int     conn_indx = p->array[src].router.port[port].index;

        if ((conn_indx != PCFG_INDEX_INVALID) &&
                (p->array[conn_indx].any.type == PCFG_TYPE_ROUTER) &&
                (p->array[conn_indx].router.nic == p->array[tgt].router.nic))
            return port;
    }

    return 0;
}

/*
 * Do we need reset tunnelling on the meta-rtr ? If a meta-rtr is connected
 * to our partition on more than one port, we need tunnelling
 */

static __inline int
tunnelling(pcfg_t *p, pcfg_router_t *router, partid_t pttn)
{
    __uint64_t	port_mask;

    if (!meta_in_partition(p, router, pttn, &port_mask))
        return 0;

    return !power_of_2(port_mask);
}

/*
 * Function: 		pttn_pcfg_fence -> Setup fences in pcfg
 * Args:		pcfg_t ptr
 * Logic:		Fences a port on a router if a different partition is
 *			connected to it. Doesn't setup fences on the meta rtr.
 *			If a mata rtr can't be pigeon-holed into a partition,
 *			its assigned a pttnid of 0. All rtrs connected to that
 *			meta rtr will have a non-zero pttnid and will fence
 *			the connection to the meta rtr
 */

static int
fence_router(pcfg_t *p, int index)
{
    int		port;

    for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
        int	conn_index;

        /*
         * NB: Meta rtrs are handled in pttn_setup_meta_routers
         */

        if (!IS_META(&p->array[index].router)) {
            conn_index = p->array[index].router.port[port].index;

            if (conn_index == PCFG_INDEX_INVALID)
                p->array[index].router.port[port].flags |=
			PCFG_PORT_FENCE_MASK;
            else if (p->array[conn_index].any.type == PCFG_TYPE_HUB) {
                if (p->array[conn_index].hub.partition !=
			p->array[index].router.partition) {
                    p->array[index].router.port[port].flags |=
			PCFG_PORT_FENCE_MASK;
                    p->array[conn_index].hub.port.flags |=
			PCFG_PORT_FENCE_MASK;
                }
                else {
                    p->array[index].router.port[port].flags &=
			~PCFG_PORT_FENCE_MASK;
                    p->array[conn_index].hub.port.flags &=
			~PCFG_PORT_FENCE_MASK;
                }
            }
            else {
                int	conn_port = get_conn_port(p, index, conn_index);

                if (IS_META(&p->array[conn_index].router)) {
                /*
                 * If a meta-rtr needs to do reset tunnelling i.e. it needs to
                 * propagate resets from the same partition thro' it, don't
                 * fence off the port connected to the meta-rtr. Else fence
                 * off the port connected to the meta-rtr, since the meta-rtr
                 * could go down and come back up and we don't want to be
                 * reset while its coming up
                 */
                    if (tunnelling(p, &p->array[conn_index].router, 
				p->array[index].router.partition))
                        p->array[index].router.port[port].flags &=
				~PCFG_PORT_FENCE_MASK;
                    else
                        p->array[index].router.port[port].flags |=
				PCFG_PORT_FENCE_MASK;
                }
                else if (p->array[conn_index].router.partition ==
			p->array[index].router.partition) {
                    p->array[index].router.port[port].flags &=
			~PCFG_PORT_FENCE_MASK;
                    p->array[conn_index].router.port[conn_port].flags &=
			~PCFG_PORT_FENCE_MASK;
                }
                else {
                    p->array[index].router.port[port].flags |=
			PCFG_PORT_FENCE_MASK;
                    p->array[conn_index].router.port[conn_port].flags |=
			PCFG_PORT_FENCE_MASK;
                }
            }
        }
    }

    return 0;
}

static __inline void
fence_sn00_hub(pcfg_t *p, int index)
{
    int		conn_index = p->array[index].hub.port.index;

    if (p->array[conn_index].hub.partition != p->array[index].hub.partition)
        p->array[index].hub.port.flags |= PCFG_PORT_FENCE_MASK;
}

int
pttn_pcfg_fence(pcfg_t *p)
{
    int         i;
    partid_t	partid = p->array[0].hub.partition;

    for (i = 0; i < p->count; i++)
        if (p->array[i].any.type == PCFG_TYPE_ROUTER)
            fence_router(p, i);
        else if ((p->array[i].any.type == PCFG_TYPE_HUB) && SN00)
            fence_sn00_hub(p, i);

    return 0;
}

/*
 * Function:		pttn_erect_fences -> Based on the pcfg picture, sets
 *			up fences on the routers
 * Args:		pcfg ptr
 */

void
pttn_erect_fences(pcfg_t *p)
{
    int         i;

    db_printf("Erecting partition fences ................                        ");

    for (i = 0; i < p->count; i++) {
        int             port;
        __uint64_t      fence_mask = 0;
        net_vec_t       vec;

        if (p->array[i].any.type != PCFG_TYPE_ROUTER) {
            if (SN00 && (p->array[i].hub.port.flags & PCFG_PORT_FENCE_MASK))
                hub_reset_fence_set(p->array[i].hub.nasid);

            continue;
        }

        /*
         * Meta rtrs are setup later in pttn_setup_meta_routers
         */

        if (IS_META(&p->array[i].router) ||
		!IS_RESET_SPACE_RTR(&p->array[i].router))
            continue;

        for (port = 1; port <= MAX_ROUTER_PORTS; port++)
            if (p->array[i].router.port[port].flags & PCFG_PORT_FENCE_MASK)
                fence_mask |= (1 << (port - 1));

        if ((vec = discover_route(p, 0, i, 0)) == NET_VEC_BAD) {
            printf("\n*** erect_fences: Can't find route between local HUB to "
                "router NIC 0x%lx\n", p->array[i].router.nic);
            continue;
        }

        db_printf("erect_fences: router NIC 0x%lx vec 0x%lx fence_mask 0x%lx\n",                p->array[i].router.nic, vec, fence_mask);

        if (router_fence_ports(vec, fence_mask, SET, LOCALB | PORT, ALL_PORTS))
            printf("\n*** erect_fences: Can't fence ports mask 0x%lx on router "
                "vec 0x%lx\n", fence_mask, vec);

        if (router_fence_ports(vec, ~fence_mask, CLEAR, LOCALB | PORT,
		ALL_PORTS))
            printf("\n*** erect_fences: Can't clear ports mask 0x%lx on router "
                "vec 0x%lx\n", ~fence_mask, vec);
    }

    db_printf("DONE\n");
}

/*
 * Funtion:             pttn_wallin -> wall off routers that are
 *                      in reset space. Called if partition is invalid
 * Args:                pcfg_t *
 * Returns:             0
 * Logic:               All router ports that are connected to either HUBs
 *                      or RTRs in non-reset space are walled off.
 * N.B.:                Should be called only if walling is necessary. All
 *			RESET_SPACE elements are given a pttnid of 0 and the
 *			reset non-zero
 */

int
pttn_wallin(pcfg_t *p)
{
    int         i;

    for (i = 0; i < p->count; i++)
        if (p->array[i].any.type == PCFG_TYPE_HUB) {
            p->array[i].hub.partition = (IS_RESET_SPACE_HUB(&p->array[i].hub) ?
                                        0 : (partid_t) -1);
            if (SN00)
                p->array[i].hub.port.index = -1;
        }
        else {
            int port;

            if (IS_RESET_SPACE_RTR(&p->array[i].router)) {

                p->array[i].router.partition = 0;

                for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
                    int index = p->array[i].router.port[port].index;

                    if (index != PCFG_INDEX_INVALID) {
                        if (((p->array[index].any.type == PCFG_TYPE_HUB) &&
                                !IS_RESET_SPACE_HUB(&p->array[index].hub)) ||
                                ((p->array[index].any.type == PCFG_TYPE_ROUTER)
                                && !IS_RESET_SPACE_RTR(
                                &p->array[index].router))) {
                            p->array[i].router.port[port].index =
                                        PCFG_INDEX_INVALID;
                            p->array[i].router.port[port].flags |=
                                        PCFG_PORT_FENCE_MASK;
                        }
                        else
                            p->array[i].router.port[port].flags &=
                                        ~PCFG_PORT_FENCE_MASK;
                    }
                }
            }
            else
                p->array[i].router.partition = (partid_t) -1;
        }

    return 0;
}

/*
 * Function:		pttn_pcfg_nasid_update -> Updates nasid picture in
 *			promcfg for all local masters
 * Args:		to_p -> pcfg to update (usually local)
 *			from_p -> gmaster pcfg
 * N.B.:		Also updates pttnid for headless nodes.
 */

int
pttn_pcfg_nasid_update(pcfg_t *to_p, pcfg_t *from_p)
{
    int         i, rc = 0;

    if (to_p == from_p)
        return 0;

    for (i = 0; i < to_p->count; i++) {
        int     index;

        if (to_p->array[i].any.type == PCFG_TYPE_HUB) {
            if (!to_p->array[i].hub.nic || ((index = discover_find_hub_nic(
		from_p, to_p->array[i].hub.nic)) == PCFG_INDEX_INVALID)) {
                __uint64_t      ni_status;
                net_vec_t       vec = discover_route(to_p, 0, i, 0);
		elsc_t		remote_elsc;

                /* Headless node! */

                if (vec == NET_VEC_BAD) {
                    printf("*** discover_nasid_update: Can't update nasid for "
                        "headless node!\n");
                    rc = -1;
                    continue;
                }

                if (!vector_read(vec, 0, NI_STATUS_REV_ID, &ni_status))
                    to_p->array[i].hub.nasid = (nasid_t) GET_FIELD(ni_status,
                                NSRI_NODEID);
            }
            else
                to_p->array[i].hub.nasid = from_p->array[index].hub.nasid;
        }
    }

    return rc;
}

/*
 * Function:		pttn_setup_meta_routers -> If meta routers are connected
 *			to 2 or more partitions, wall off the cross-partition
 *			connections so that klconfig is setup right
 *                      Punch a hole thro' meta rtrs for partition intra-reset
 * Args:		pcfg ptr
 *			partition -> partition master pttnid
 */

int
pttn_setup_meta_routers(pcfg_t *p, partid_t partition)
{
    int         i;
    __uint64_t	port_mask;

    for (i = 0; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_ROUTER) &&
                IS_META(&p->array[i].router) &&
                meta_in_partition(p, &p->array[i].router, partition, 
		&port_mask)) {
            net_vec_t	vec;
            int		port;

            p->array[i].router.partition = partition;

            vec = discover_route(p, 0, i, 0);

            if (router_fence_ports(vec, ALL_PORTS, SET, LOCALB, ALL_PORTS))
                printf("*** meta_rtr_setup: Unable to setup META-RTR NIC "
			"0x%lx for intra-pttn reset\n",p->array[i].router.nic);

            if (router_fence_ports(vec, ~port_mask, SET, PORT, port_mask))
                printf("*** meta_rtr_setup: Unable to setup META-RTR NIC "
			"0x%lx for intra-pttn reset\n",p->array[i].router.nic);

            if (router_fence_ports(vec, port_mask, CLEAR, PORT, port_mask))
                printf("*** meta_rtr_setup: Unable to setup META-RTR NIC "
			"0x%lx for intra-pttn reset\n",p->array[i].router.nic);

            for (port = 1; port <= MAX_ROUTER_PORTS; port++)
                if (port_mask & (1 << (port - 1))) {
                    int	conn_port, conn_index;

                    conn_index = p->array[i].router.port[port].index;
                    conn_port = get_conn_port(p, conn_index, i);

                    p->array[conn_index].router.port[conn_port].flags &=
			~PCFG_PORT_FENCE_MASK;
                    p->array[i].router.port[port].flags &=
			~PCFG_PORT_FENCE_MASK;
                }
                else
                    p->array[i].router.port[port].flags |=
			PCFG_PORT_FENCE_MASK;
        }

    return 0;
}

static partid_t get_router_partition(pcfg_t *p, int indx)
{
    int         port;

    for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
        int             conn_index;

        conn_index = p->array[indx].router.port[port].index;

        if ((conn_index != PCFG_INDEX_INVALID) &&
                (p->array[conn_index].any.type == PCFG_TYPE_HUB) &&
                !(p->array[conn_index].hub.flags & PCFG_HUB_HEADLESS))
                    return p->array[conn_index].hub.partition;
     }

     return -1;
}

/*
 * Function:		pttn_router_partid -> Setup pttnid for routers. This is
 *			based on the topology knowledge. If there is not even a
 *			single working HUB in a module, this would fail
 *			Meta rtrs are assigned a pttnid of 0, since they
 *			belong to everyone and noone!
 */

int pttn_router_partid(pcfg_t *p)
{
    int         i;

    for (i = 0; i < p->count; i++) {
        partid_t        partition;

        if (p->array[i].any.type != PCFG_TYPE_ROUTER)
            continue;

        if (IS_META(&p->array[i].router)) {
            p->array[i].router.partition = 0;
            continue;
        }

        if ((partition = get_router_partition(p, i)) != -1) {
            p->array[i].router.partition = partition;
            continue;
        }

        if ((p->array[i].router.port[6].index != PCFG_INDEX_INVALID) &&
                ((partition = get_router_partition(
                p, p->array[i].router.port[6].index)) != -1))
            p->array[i].router.partition = partition;
        else
            printf("*** WARNING: discover_router_partition: no partition id for"                "router nic 0x%lx\n", p->array[i].router.nic);
    }

    return 0;
}

/*
 * Function:		pttn_partprom -> If anyone advertized a partprom
 * Args:		pcfg ptr
 * Returns:		1 -> if anyone did
 *			0 -> if noone did
 */

int
pttn_partprom(pcfg_t *p)
{
    int		i, found = 0;

    for (i = 0; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		!(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
		(p->array[i].hub.flags & PCFG_HUB_PARTPROM)) {
            found = 1;
            break;
        }

    return found;
}

/*
 * Function:	pttn_vote_partitionid -> vote for partitionid in case
 *		we were unbale to read from the MSC.
 * Args:	pcfg ptr
 *		my_indx -> one HUB in the module
 */

partid_t
pttn_vote_partitionid(pcfg_t *p, int my_indx)
{
    int         module_indexes[NODESLOTS_PER_MODULE];
    int         compact_indx[NODESLOTS_PER_MODULE];
    int         votes[NODESLOTS_PER_MODULE];
    int         i, j, n_nodes;

    for (i = 0; i < NODESLOTS_PER_MODULE; i++)
        module_indexes[i] = -1;

    discover_module_indexes(p, my_indx, module_indexes);

    n_nodes = 0;

    /* Ignore headless nodes. They aren't voting. */
    for (i = 0; i < NODESLOTS_PER_MODULE; i++)
        if ((module_indexes[i] != -1) &&
                !(p->array[module_indexes[i]].hub.flags & PCFG_HUB_HEADLESS))
            compact_indx[n_nodes++] = module_indexes[i];

    for (i = 0; i < n_nodes; i++) {
        for (j = 0; j < n_nodes; j++)
            if (p->array[compact_indx[j]].hub.partition ==
                                p->array[compact_indx[i]].hub.partition)
                votes[j] = 1;
            else
                votes[j] = 0;

        if (majority(n_nodes, votes))
            return p->array[compact_indx[i]].hub.partition;
    }

    return -1;
}

/*
 * Function:		pttn_hub_partid -> Update pttnid for all HUBs
 */

void
pttn_hub_partid(pcfg_t *p, int headless_only)
{
    int		i;

    for (i = 1; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		IS_RESET_SPACE_HUB(&p->array[i].hub)) {
            net_vec_t	vec;

            if ((headless_only && !(p->array[i].hub.flags & PCFG_HUB_HEADLESS))
		|| (!headless_only && (p->array[i].hub.flags & 
		PCFG_HUB_HEADLESS)))
                continue;

            if ((vec = discover_route(p, 0, i, 0)) != NET_VEC_BAD) {
                __uint64_t	sr0;

                if (vector_read(vec, 0, NI_SCRATCH_REG0, &sr0) < 0)
                    continue;

                p->array[i].hub.partition = (char) GET_FIELD(sr0, 
			ADVERT_PARTITION);
            }
        }
}

/*
 * Function:		pttn_update_module_partid -> Update own module's partid
 *			in pcfg
 */

void
pttn_update_module_partid(pcfg_t *p)
{
    int         module_indexes[NODESLOTS_PER_MODULE];
    int         compact_indx[NODESLOTS_PER_MODULE];
    int		i;

    for (i = 0; i < NODESLOTS_PER_MODULE; i++)
        module_indexes[i] = -1;

    discover_module_indexes(p, 0, module_indexes);

    for (i = 0; i < NODESLOTS_PER_MODULE; i++)
        if (module_indexes[i] != -1) {
            char	buf[8];

            ip27log_getenv(p->array[module_indexes[i]].hub.nasid, 
			IP27LOG_PARTITION, buf, "0", 0);
            p->array[module_indexes[i]].hub.partition = atoi(buf);
        }
}

void
pttn_clean_partids(pcfg_t *p)
{
    int		i;

    for (i = 0; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		IS_RESET_SPACE_HUB(&p->array[i].hub))
            p->array[i].hub.partition = 0;
}

void check_other_consoles(pcfg_t *p)
{
	int i, rc;
	pcfg_hub_t *hub;

	for(i=0; i<p->count; i++)
	{
		if (p->array[i].any.type == PCFG_TYPE_HUB &&
                !(p->array[i].hub.flags & PCFG_HUB_HEADLESS) &&
                !(p->array[i].hub.flags & PCFG_HUB_MEMLESS) &&
		(p->array[0].hub.partition > 0) &&
		(p->array[i].hub.partition > 0) &&
		(p->array[i].hub.partition == p->array[0].hub.partition) &&
		((p->array[i].hub.flags & PCFG_HUB_BASEIO) && !SN00) &&
		(p->array[i].hub.nasid != get_nasid()))
		{
			hub = &p->array[i].hub;
			rc = check_console(hub);
			if(rc>0)
			/* there is another node which is candidate 
			   for global master and it was global master
			    last time this partition-id was used */
			{
				printf("*** New console on module %d\n",
                        	hub->module);
            			printf("*** You can change the console by setting the "
                        		"ConsolePath variable\n");
            			printf("*** Setting ConsolePath variable and resetting.\n");
            			ip27log_setenv(hub->nasid, "ForceConsole", "1", 0);

            			rtc_sleep(2 * 1000000);
				restore_restart_state(p) ;
            			reset_system();
					
			}
		}
	}
}
