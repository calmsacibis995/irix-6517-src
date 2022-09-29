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
*	File:		directory.c					*
*									*
*	Contains code that manipulates the low memory directory		*
*		and its ilk.						*
*									*
\***********************************************************************/

#ident  "$Revision: 1.27 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h> 
#include <libsc.h>
#include <libsk.h>
#include <sys/SN/arch.h>
#include <sys/SN/gda.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/SN0/ip27log.h>
#include <arcs/spb.h>
#include "io6prom_private.h"
#include "traverse_nodes.h"

#ifdef SABLE
#include <sys/SN/promcfg.h>
#include "ip27prom.h"

extern ulong  get_BSR(void) ;

extern void more_ip27prom_init(void);

int fake_prom = 0;
#endif

int num_nodes = 0;
extern nasid_t master_nasid;
extern int klcfg_consist_check(nasid_t nasid);

/* Forward declarations */
void propagate_lowmem(gda_t *gdap, __int64_t spb_addr);
void init_gda(gda_t *);

#ifdef SABLE

/*
 * init_kldir
 * We only need this code in the IO6 prom for simulating what's already
 * done by the IP27 prom.
 */

static void init_kldir(nasid_t nasid)
{
    kldir_ent_t	*ke;

    ke = KLD_LAUNCH(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_LAUNCH_OFFSET;
    ke->size   = IP27_LAUNCH_SIZE;
    ke->count  = IP27_LAUNCH_COUNT;
    ke->stride = IP27_LAUNCH_STRIDE;

    ke = KLD_NMI(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_NMI_OFFSET;
    ke->size   = IP27_NMI_SIZE;
    ke->count  = IP27_NMI_COUNT;
    ke->stride = IP27_NMI_STRIDE;

    ke = KLD_KLCONFIG(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_KLCONFIG_OFFSET;
    ke->size   = IP27_KLCONFIG_SIZE;
    ke->count  = IP27_KLCONFIG_COUNT;
    ke->stride = IP27_KLCONFIG_STRIDE;

    ke = KLD_PI_ERROR(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_PI_ERROR_OFFSET;
    ke->size   = IP27_PI_ERROR_SIZE;
    ke->count  = IP27_PI_ERROR_COUNT;
    ke->stride = IP27_PI_ERROR_STRIDE;

    ke = KLD_SYMMON_STK(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_SYMMON_STK_OFFSET;
    ke->size   = IP27_SYMMON_STK_SIZE;
    ke->count  = IP27_SYMMON_STK_COUNT;
    ke->stride = IP27_SYMMON_STK_STRIDE;

    ke = KLD_FREEMEM(nasid);

    ke->magic  = KLDIR_MAGIC;
    ke->offset = IP27_FREEMEM_OFFSET;
    ke->size   = IP27_FREEMEM_SIZE;
    ke->count  = IP27_FREEMEM_COUNT;
    ke->stride = IP27_FREEMEM_STRIDE;

}

#endif

/*
 * Initialize the portions of the low memory directory that need to be
 * set up in the IO6 prom.
 */
void init_local_dir()
{
    kldir_ent_t *ke;
    nasid_t nasid = get_nasid();
    gda_t *gdap;
    lboard_t *ip27;
    partid_t local_partition;
    int cnode;
    nasid_t brd_nasid;

#ifdef SABLE
    ke = KLD_LAUNCH(nasid);

    if (ke->magic != KLDIR_MAGIC) {
	fake_prom = 1;
	init_kldir(nasid);
    }

    /*
     * XXX fixme - We still need to do directory init on other nodes.
     * it's easy once you know which nodes to initialize.
     */

#endif

    ke = KLD_GDA(nasid);

    ke->magic  = KLDIR_MAGIC;
    /* There's only one GDA per kernel.  Set up an absolute pointer here. */
    ke->pointer = PHYS_TO_K0(NODE_OFFSET(nasid) + IO6_GDA_OFFSET);
    ke->size   = IO6_GDA_SIZE;
    ke->count  = IO6_GDA_COUNT;
    ke->stride = IO6_GDA_STRIDE;

#ifdef SABLE
    if (fake_prom)
        more_ip27prom_init();
#endif

    gdap = (gda_t *)GDA_ADDR(master_nasid);

    init_gda(gdap);

    /*
     * XXX - This call won't work until we have a KLconfig structure
     * complete with router links.  That should be true here when
     * the IP27 integration is complete.
     * The node_probe works now. The bringup is removed. Need to check
     * if there is any dependency of this for partitions.
     */

    if (!(ip27 = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid), 
			KLTYPE_IP27))) {
        char	partid_buf[8];

        if (ip27log_getenv(nasid, IP27LOG_PARTITION, partid_buf, 0, 0) >= 0)
            local_partition = atoi(partid_buf);
        else
            local_partition = 1;
    }
    else
        local_partition = ip27->brd_partition;

    num_nodes = node_probe((lboard_t *)KL_CONFIG_INFO(master_nasid), gdap,
			local_partition);

    if (num_nodes == 0)
	printf("init_local_dir: Error - No nodes found.\n");

    propagate_lowmem(gdap, SPBADDR);
    for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
        brd_nasid = gdap->g_nasidtable[cnode];
	if(brd_nasid != INVALID_NASID)
	    klcfg_consist_check(brd_nasid);
    }
    return;
}

partid_t
get_part_from_nasid(nasid_t n)
{
	lboard_t *lb ;
	lb = (lboard_t *)KL_CONFIG_INFO(n) ;
	lb = find_lboard(lb, KLTYPE_IP27) ;
	if (lb)
		return lb->brd_partition ;
	return INVALID_PARTID ;
}

void
init_gda(gda_t *gdap)
{
	int i;

	gdap->g_version = GDA_VERSION;
	gdap->g_promop = PROMOP_INVALID;
	gdap->g_masterid = makespnum(get_nasid(), *LOCAL_HUB(PI_CPU_NUM));

/* XXX fixme - need to read the dipswitches or OR together the values
 * on all boards' dipswitches.
 */
	gdap->g_vds = 0;

	/* Clear the NASID table */
	for (i = 1; i < MAX_COMPACT_NODES+1; i++)
		gdap->g_nasidtable[i] = INVALID_CNODEID;

	/* We're the master node so we get to be node 0 */
	gdap->g_nasidtable[0] = get_nasid();
	gdap->g_magic = GDA_MAGIC;
	gdap->g_partid = get_part_from_nasid(get_nasid()) ;
	/*
	 * We need to wait until KLconfig is complete before calling
	 * node probe.  It should be called here when the move to the IP27
	 * prom is complete.
	 */
}


void
propagate_lowmem(gda_t *gdap, __int64_t spb_offset)
{
	int i;
	kldir_ent_t *ke;
	nasid_t nasid;

	if (*LOCAL_HUB(PI_CALIAS_SIZE) != PI_CALIAS_SIZE_8K)
	{
		printf("propagate_lowmem: Error - CALIAS set to %d!\n",
			*LOCAL_HUB(PI_CALIAS_SIZE));
	}

	/*
	 * The GDA is already available on this node (the master)
	 * but a pointer to it needs to be propagated to all other nodes.
	 * Therefore, start at node 1 (0 is the master) and copy away.
	 */
	for (i = 1; i < num_nodes; i++) {
		nasid = gdap->g_nasidtable[i];

		if (nasid == INVALID_NASID) {
			printf("propagate_lowmem: Error - bad nasid (%d) for cnode %d\n",
				nasid, i);
			continue;
		}

#if DEBUG	/* happens on headless nodes */
		if (*REMOTE_HUB(nasid, PI_CALIAS_SIZE) && 
		    *REMOTE_HUB(nasid, PI_CALIAS_SIZE) != PI_CALIAS_SIZE_4K)
			printf("ERROR: Node %d CALIAS size set to %d!\n",
				nasid, *REMOTE_HUB(nasid, PI_CALIAS_SIZE));
#endif

		ke = KLD_GDA(nasid);

		ke->magic  = KLDIR_MAGIC;
		ke->pointer = (__psunsigned_t) gdap;
		ke->size   = IO6_GDA_SIZE;
		ke->count  = IO6_GDA_COUNT;
		ke->stride = IO6_GDA_STRIDE;

		/* Copy the SPB to all nodes.
		 * Note: This assumes that while slaves spin in their
		 * slave loops, they don't have this stuff in their
		 * caches.  The IP27 prom flushes the cache when a slave
		 * enters the loop so this should be okay.
		 */
		bcopy(SPB, (void *)NODE_OFFSET_TO_K1(nasid, spb_offset),
		      sizeof(*SPB));

		/* Set its CALIAS_SIZE */
		*REMOTE_HUB(nasid, PI_CALIAS_SIZE) = PI_CALIAS_SIZE_8K;
	}
}


#ifdef SABLE
void init_other_local_dir()
{
    kldir_ent_t *ke;
    int i;
    pcfg_ent_t 	*pcfgp;
    nasid_t 	nasid;

    for (i = 0; i <PCFG(get_nasid())->count; i++) {
	    pcfgp = PCFG_PTR(get_nasid(), i);
	    if (pcfgp->hub.type == PCFG_TYPE_HUB)
		nasid = pcfgp->hub.nasid;
	    else continue;
	    if (nasid == get_nasid()) continue;

	    ke = KLD_LAUNCH(nasid);

	    if (ke->magic != KLDIR_MAGIC) {
		    init_kldir(nasid);
	    }
    }

    ke = KLD_GDA(nasid);

    ke->magic  = KLDIR_MAGIC;
    /*
     * There's only one GDA per kernel.  Set up an absolute pointer here.
     * The GDA lives in the master node so use its nasid.
     */
    ke->pointer = PHYS_TO_K0(NODE_OFFSET(master_nasid) + IO6_GDA_OFFSET);
    ke->size   = IO6_GDA_SIZE;
    ke->count  = IO6_GDA_COUNT;
    ke->stride = IO6_GDA_STRIDE;

    return;
}

#endif

