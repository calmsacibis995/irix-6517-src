/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1996 Silicon Graphics, Inc.                             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * promif.c-
 * 	This file specifies the interface between the kernel and the PROM's
 * 	configuration data structures.
 */

#ident "$Revision: 1.53 $"

#include <sys/types.h>
#include <sys/reg.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/invent.h>
#include <sys/syssgi.h>
#include <sys/debug.h>
#include <sys/SN/gda.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/module.h>
#if CELL
#include <sys/kopt.h>
#include <ksys/cell.h>
#include <ksys/partition.h>
#endif

#include "sn_private.h"

cnodeid_t nasid_to_compact_node[MAX_NASIDS];
nasid_t compact_to_nasid_node[MAX_COMPACT_NODES];
cnodeid_t cpuid_to_compact_node[MAXCPUS];

/* Forward declaration */
int do_cpumask(cnodeid_t, nasid_t, cpumask_t *, int *);

static __psunsigned_t master_bridge_base = (__psunsigned_t)NULL;
static nasid_t console_nasid;
static char console_wid;
static char console_pcislot;
#define	READYTIMEOUT	50000

/*
 * cpu_node_probe(cpumask_t *boot_cpumask, int *numnodes)
 *	"Probe" the PROM config structure for CPUs and nodes, initialize a CPU
 *	mask and the translation arrays, and then return maxcpus.
 */
cpuid_t
cpu_node_probe(cpumask_t *boot_cpumask, int *numnodes)
{
	int i, cpus;
	gda_t *gdap;
	nasid_t nasid;
	int highest = 0;
	cnodeid_t current_cnode;

	/*
	 * Initialize the arrays to invalid nodeid (-1)
	 */
	for (i = 0; i < MAX_COMPACT_NODES; i++) {
		compact_to_nasid_node[i] = INVALID_NASID;
	}

	for (i = 0; i < MAX_NASIDS; i++) {
		nasid_to_compact_node[i] = INVALID_CNODEID;
	}

	for (i = 0; i < MAXCPUS; i++) {
		cpuid_to_compact_node[i] = INVALID_CNODEID;
	}

#ifdef SABLE

#if SABLE_SYMMON
	/* SABLE_SYMMON is defined if we may be trying to run SABLE without
	 * a "real prom" but with symmon loaded.  In this case, symmon may
	 * have performed enough initialization such that the kernel may
	 * "think" there's a real prom and have fake_prom == 0.
	 * So if fake_prom is zero, we check the (SABLE_SYMMON_INITPC+4)
	 * location.
	 * If this is zero, then we have a "real prom" running under sable
	 * (i.e. symmon will zero this location if it finds that it is
	 * running with a real prom).
	 *
	 * NOTE: Works if symmon has been compiled with SABLE and the kernel
	 * has been compiled with both SABLE and SABLE_SYMMON.
	 */
	if (fake_prom || (*(int *)(SABLE_SYMMON_INITPC+4))) {
#else	/* !SABLE_SYMMON */
	if (fake_prom) {
#endif	/* !SABLE_SYMMON */

		/* max cpus == sable_mastercounter.  maxnodes == (maxcpus + 1)/2 */

		for (i = 0; i < (sable_mastercounter + 1)/2; i++) {
			nasid_to_compact_node[i] = i;
			compact_to_nasid_node[i] = i;
		}

		{
			short n = cputocnode(master_procid);
			short x;
#define SWAP(_a, _b)	x = (_a); (_a) = (_b); (_b) = x;
			SWAP(compact_to_nasid_node[n],
			     compact_to_nasid_node[0]);
			SWAP(nasid_to_compact_node[n],
			     nasid_to_compact_node[n]);
		}

		/* Set up the live CPU mask  - this is a fake one for sable. */
		for (i = 0; i < sable_mastercounter; i++) {
			CPUMASK_SETB(*boot_cpumask, i);
			cpuid_to_compact_node[i] = i/2;
		}

		*numnodes = (sable_mastercounter + CPUS_PER_NODE - 1) / 2;

		return sable_mastercounter;

	} /* fake_prom */
#endif
	/* Real code: */
	gdap = GDA;
	*numnodes = 0;
	cpus = 0;

#if CELL

	/* Who are we? */

	my_cellid = PARTID_TO_CELLID(part_get_promid());
	golden_cell = atoi(kopt_find("golden_cell"));
#endif

	for (i = 0; i < MAX_COMPACT_NODES; i++) {
		if ((nasid = gdap->g_nasidtable[i]) == INVALID_NASID) {
			break;
		} else {
			compact_to_nasid_node[i] = nasid;
			nasid_to_compact_node[nasid] = i;

			(*numnodes)++;

			/* Count the CPUs on this node and add mask bits */
			cpus += do_cpumask(i, nasid, boot_cpumask, &highest);
/*
			if (klconfig_bad_version(nasid))
 */

		}
	}

	current_cnode = 0;
	/*
	 * cpus are numbered in order of cnodes. If necessary, disabled
	 * cpus may be numbered following all the enabled cpus. Currently,
	 * disabled cpus are not numbered.
	 */
	for (i = 0; i <= highest; i++) {
		if (cpuid_to_compact_node[i] > current_cnode)
		    current_cnode = cpuid_to_compact_node[i];
		else if (cpuid_to_compact_node[i] != INVALID_CNODEID) {
                        /*
                         * PV : 547345
                         * Do not fail if the cpu happens to be disabled.
                         * The original ASSERTION would have failed if the
                         * last cnode does not have any cpu enabled.
                         */
                        ASSERT_ALWAYS(
                                (cpuid_to_compact_node[i] == current_cnode) ||
                                ((current_cnode + 1) == *numnodes) ||
                                (!(CPUMASK_TSTB(*boot_cpumask, i))));
		}
		
	}

	return highest + 1;
}

/*
 * int do_cpumask(cnodeid_t, nasid_t, cpumask_t *, int *)
 *
 *	Look around klconfig to determine how many CPUs we have and which
 * 	ones they are.  Return the number of CPUs on this node.
 *	It's too early to do anything useful with this information
 *	so we'll be back later.
 *
 *	If it should be convenient later, we can store away a pointer to
 *	the CPU structure.
 */

/*
 * HACK till we move to all 2.6 parts.
 * Check the revs of all CPUs, and if any of these are
 * 2.5, force the migration vehicle to be TLBsync (set numa_migr_vehicle to 1). 
 */

extern int numa_migr_vehicle;

int
do_cpumask(cnodeid_t cnode, nasid_t nasid, 
	   cpumask_t *boot_cpumask, int *highest)
{
	lboard_t *brd;
	klcpu_t *acpu;
	int cpus_found = 0;
	cpuid_t cpuid;
	
	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_IP27);

	ASSERT_ALWAYS(brd);

	do {

	acpu = (klcpu_t *)find_first_component(brd, KLSTRUCT_CPU);

	while (acpu) {
		cpuid = acpu->cpu_info.virtid;
		
		ASSERT_ALWAYS(cpuid < MAXCPUS);
		/* cnode is not valid for completely disabled brds */
		if (get_actual_nasid(brd) == brd->brd_nasid)
			cpuid_to_compact_node[cpuid] = cnode;
		
		if (cpuid > *highest)
			*highest = cpuid;

		/* Make sure the physid is in range */
		ASSERT(acpu->cpu_info.physid < CPUS_PER_NODE);

		/* Only let it join in if it's marked enabled */
		if (acpu->cpu_info.flags & KLINFO_ENABLE) {
			/* If this CPU's bit already set, something's wrong. */
			ASSERT_ALWAYS(!CPUMASK_TSTB(*boot_cpumask, cpuid));

			/* Set the bit. */
			CPUMASK_SETB(*boot_cpumask, cpuid);
			/* Count this CPU */
			cpus_found++;
		}
		acpu = (klcpu_t *)
		    find_component(brd, (klinfo_t *)acpu, KLSTRUCT_CPU);
 	} /* while */

        brd = KLCF_NEXT(brd) ;
        if (brd)
                brd = find_lboard(brd,KLTYPE_IP27);
        else
                break ;

        } while (brd) ;

	return cpus_found;
}

/*
 * slurp_inventory(void)
 *	Copy the PROM's configuration databases into the kernel's magic
 *	graph.
 */
void
slurp_inventory(void)
{


}

cnodeid_t
nasid_to_compact_nodeid(nasid_t nasid)
{
	ASSERT(nasid >= 0 && nasid < MAX_NASIDS);
	return nasid_to_compact_node[nasid];
}

nasid_t
compact_to_nasid_nodeid(cnodeid_t cnode)
{
#ifdef SABLE
	ASSERT_ALWAYS(compact_to_nasid_node[cnode] >= 0);
#else
	ASSERT(cnode >= 0 && cnode <= MAX_COMPACT_NODES);
	ASSERT(compact_to_nasid_node[cnode] >= 0);
#endif
	return compact_to_nasid_node[cnode];
}

nasid_t
get_lowest_nasid(void)
{
	cnodeid_t cnode;
	nasid_t lowest;

	lowest = compact_to_nasid_node[0];

	for (cnode = 1; cnode < maxnodes; cnode++)
		if (compact_to_nasid_node[cnode] < lowest)
			lowest = compact_to_nasid_node[cnode];

	return lowest;
}

int
cnode_exists(cnodeid_t cnode)
{
	if ((cnode < 0) || (cnode >= MAX_COMPACT_NODES))
		return 0;
	else 
		return (compact_to_nasid_node[cnode] != INVALID_NASID);
}


int
getsysid(char *hostident)
{
	int error;
	module_info_t m_info;
	
	bzero(hostident, MAXSYSIDSIZE);

	/* for the old interfaces return the serial number of the lowest
	numbered module */
	if (error = get_kmod_info(0,&m_info))
		return error;
#if defined (SN0)
	if (private.p_sn00) {
		
		*(__uint32_t*)hostident = m_info.serial_num;
		
	}
#endif
	else
		strncpy(hostident,m_info.serial_str,MAX_SERIAL_SIZE);

	return 0;
}


void
set_master_bridge_base(void)
{

	console_nasid = KL_CONFIG_CH_CONS_INFO(master_nasid)->nasid;
	console_wid = WIDGETID_GET(KL_CONFIG_CH_CONS_INFO(master_nasid)->memory_base);
	console_pcislot = KL_CONFIG_CH_CONS_INFO(master_nasid)->npci;

	master_bridge_base = (__psunsigned_t)NODE_SWIN_BASE(console_nasid,
							    console_wid);
}

nasid_t
get_console_nasid(void)
{
	return console_nasid;
}

char
get_console_pcislot(void)
{
	return console_pcislot;
}


int
check_nasid_equiv(nasid_t nasida, nasid_t nasidb)
{
	if ((nasida == nasidb) ||
	    (nasida == NODEPDA(NASID_TO_COMPACT_NODEID(nasidb))->xbow_peer))
		return 1;
	else
		return 0;
}


int
is_master_nasid_widget(nasid_t test_nasid, xwidgetnum_t test_wid)
{

	/*
	 * If the widget numbers are different, we're not the master.
	 */
	if (test_wid != (xwidgetnum_t)console_wid)
		return 0;

	/*
	 * If the NASIDs are the same or equivalent, we're the master.
	 */
	if (check_nasid_equiv(test_nasid, console_nasid)) {
		return 1;
	} else {
		return 0;
	}
}

__psunsigned_t
get_master_bridge_base(void)
{
	return master_bridge_base;
}


