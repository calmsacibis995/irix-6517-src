/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1986-1996 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.202 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/reg.h>

#if defined(SABLE) && defined(FAKE_PROM)
#define PROM	1
#include <sys/SN/kldir.h>
#undef PROM
#endif
#include <sys/pda.h>
#include <sys/kopt.h>
#include <sys/nodepda.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/cpumask.h>
#include <sys/cmn_err.h>
#include <sys/invent.h>
#include <sys/splock.h>		/* Defines splockmeter, for SABLE */
#include <sys/debug.h>
#include <sys/dump.h>
#include <sys/fpu.h>		/* set_fpc_csr, get_fpc_irr */
#include <sys/callo.h>		/* struct callout */
#include <sys/clksupport.h>	/* init_timebase */
#include <sys/idbg.h>		/* struct dbstbl */
#include <sys/nodemask.h>	/* CNODEMASK 	*/
#include <sys/SN/gda.h>
#include <sys/SN/intr.h>
#include <sys/SN/groupintr.h>
#include <sys/SN/gda.h>
#ifdef SABLE
#include <sys/kopt.h>		/* kopt_set */
#include <sys/arcs/spb.h>
#endif /* SABLE */
#if defined (SN0)
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/hubdev.h>
#include <sys/SN/SN0/sn0.h>
#endif /* SN0 */

#include <sys/SN/slotnum.h>
#include "sn_private.h"
#include <sys/SN/klconfig.h>

#include <sys/SN/nvram.h>
#include <sys/SN/war.h>

#include <sys/PCI/bridge.h>	/* for bridge_t */
#include <sys/PCI/ioc3.h>	/* for IOC3_ADDRSPACE_MASK */


#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
#include <sys/SN/kldir.h>
#include <sys/SN/launch.h>
#include <sys/kopt.h>
#include <ksys/partition.h>
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */

extern char arg_maxnodes[];

#if defined(SN0) && defined(SN0_HWDEBUG)
extern char arg_disable_bte[];
extern char arg_disable_bte_pbcopy[];
extern char arg_disable_bte_pbzero[];
extern char arg_disable_bte_poison_range[];
extern char arg_iio_bte_crb_count[];
extern char arg_iio_icmr_c_cnt[];
extern char arg_iio_icmr_precise[];
extern char arg_iio_ictp[];
extern char arg_iio_icto[];
extern char arg_la_trigger_nasid1[];
extern char arg_la_trigger_nasid2[];
extern char arg_la_trigger_val[];

extern int disable_bte;
extern int disable_bte_pbcopy;
extern int disable_bte_pbzero;
extern int disable_bte_poison_range;
extern int iio_bte_crb_count;
extern int iio_icmr_c_cnt;
extern int iio_icmr_precise;
extern int iio_ictp;
extern int iio_icto;
extern int la_trigger_nasid1;
extern int la_trigger_nasid2;
extern long la_trigger_val;

void 	hub_config_setup	(void);
void 	hub_config_init		(nasid_t);
void	hub_config_print	(void);
#endif

extern int hasmetarouter;

int		maxcpus;
cpumask_t	boot_cpumask;
hubreg_t	region_mask = 0;

int		r10k_r12k_mixed ;

extern xwidgetnum_t hub_widget_id(nasid_t);

#if defined (IP27)
short		cputype = CPU_IP27;
#elif defined (IP33)
short		cputype = CPU_IP33;
#else
#error <BOMB! define new cputype here >
#endif

static int	fine_mode = 0;

/* Global variables */
pdaindr_t	pdaindr[MAXCPUS];

static cnodemask_t	hub_init_mask;	/* Mask of cpu in a node doing init */
static volatile cnodemask_t hub_init_done_mask;
					/* Node mask where we wait for
					 * per hub initialization
					 */
static lock_t		hub_mask_lock;	/* Lock for hub_init_mask above. */

extern int valid_icache_reasons;	/* Reasons to flush the icache */
extern int valid_dcache_reasons;	/* Reasons to flush the dcache */
extern int numnodes;
extern u_char miniroot;
extern volatile int	need_utlbmiss_patch;
extern void iograph_early_init(void);

static void gen_region_mask(hubreg_t *, int);
static void gda_sym_init(void);
static void redo_nodes(int);

#ifdef SABLE
int fake_prom = 0;
void init_local_dir(void);
#endif /* SABLE */

nasid_t master_nasid = INVALID_NASID;

/*
 * Workarounds that are needed on _all_ hubs if there are any downrev hubs
 * in the system (e.g., crosscall interrupts).
 */
static warbits_t global_wars = (warbits_t)0;

/*
 * mlreset(int slave)
 * 	very early machine reset - at this point NO interrupts have been
 * 	enabled; nor is memory, tlb, p0, etc setup.
 *
 * 	slave is zero when mlreset is called for the master processor and
 *	is nonzero thereafter.
 */


void
mlreset(int slave)
{
	extern uint cachecolormask;
	extern int  pdcache_size;
	int i;

#if defined (LOGICAL_CELLS)
	extern partid_t partition_partid;

	partition_partid = INVALID_PARTID;
#endif
#ifdef SABLE
	/*
	 * Initialize the UART.
	 */
	du_earlyinit();

	{
		extern int nstrintr;
		nstrintr = 32;		/* saves about 1000 kmem_zalloc calls */
	}

#endif

	if (!slave) {
		master_nasid = get_nasid();

		
#ifndef SABLE
			/* Find and store the master bridge base. */
		set_master_bridge_base();

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
		if (get_console_nasid() == master_nasid) 
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */
			/* Set up the IOC3 */
			ioc3_mlreset((ioc3_cfg_t *)KL_CONFIG_CH_CONS_INFO(master_nasid)->config_base,
				     (ioc3_mem_t *)KL_CONFIG_CH_CONS_INFO(master_nasid)->memory_base);

#endif

		

		/*
		 * Initialize Master nvram base.
		 */
		nvram_baseinit();

		fine_mode = is_fine_dirmode();
#ifdef SABLE
		init_local_dir();

		/* Give the slaves a chance to get through the counter code. */

		for (i = 0; i < 100; i++)
			LOCAL_HUB_L(PI_CPU_NUM);
#endif

		/*
		 * Check if we should use only a subset of the nodes.
		 */
		if (is_specified(arg_maxnodes))
			redo_nodes(atoi(arg_maxnodes));

		/*
		 * Probe for all CPUs - this creates the cpumask and
		 * sets up the mapping tables.
		 */
		CPUMASK_CLRALL(boot_cpumask);
		maxcpus	 = cpu_node_probe(&boot_cpumask, &numnodes);
		ASSERT_ALWAYS(maxcpus > 0);

		maxnodes = numnodes;
		ASSERT_ALWAYS(numnodes > 0);
		boot_cnodemask = CNODEMASK_FROM_NUMNODES(numnodes);

		gen_region_mask(&region_mask, maxnodes);
		ASSERT(region_mask);

		/* We're the master processor */
		master_procid = getcpuid();
		master_nasid =
		    COMPACT_TO_NASID_NODEID(get_cpu_cnode(master_procid));
#if defined (MAPPED_KERNEL)						       
		/* Figure out which nodes will get copies of the kernel */
		setup_replication_mask(maxnodes);
#endif
		/*
		 * master_nasid we get back better be same as one from
		 * get_nasid()
		 */
		ASSERT_ALWAYS(master_nasid == get_nasid());

		/* Set all nodes' calias sizes to 8k */
		for (i = 0; i < maxnodes; i++) {
			nasid_t nasid;

			nasid = COMPACT_TO_NASID_NODEID(i);

			/*
			 * Always have node 0 in the region mask, otherwise CALIAS accesses
			 * get exceptions since the hub thinks it is a node 0 address.
			 */
#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
			if (kdebug) {
				extern nasid_t partition_master;
				hubreg_t master_region;

				master_region = (is_fine_dirmode() ? 
						 (partition_master >> NASID_TO_FINEREG_SHFT) :
						 (partition_master >> NASID_TO_COARSEREG_SHFT));
				REMOTE_HUB_S(nasid, PI_REGION_PRESENT,
					     (region_mask | 1 | (1ULL << master_region)));
			}
			else
#endif /* CELL_IRIX && LOGICAL_CELLS */
			REMOTE_HUB_S(nasid, PI_REGION_PRESENT, (region_mask | 1));



#if defined (SN0)
#if defined(DEBUG)
			if (REMOTE_HUB_L(nasid,
					PI_CALIAS_SIZE) > PI_CALIAS_SIZE_8K)
				dprintf("WARNING: HUB %d PI_CALIAS_SIZE > 8K\n",
					nasid);
#endif
			REMOTE_HUB_S(nasid, PI_CALIAS_SIZE, PI_CALIAS_SIZE_8K);

			/*
			 * Set up all hubs to havew a big window pointing at
			 * widget 0.
			 * Memory mode, widget 0, offset 0
			 */
			REMOTE_HUB_S(nasid, IIO_ITTE(SWIN0_BIGWIN),
				((HUB_PIO_MAP_TO_MEM << IIO_ITTE_IOSP_SHIFT) |
				(0 << IIO_ITTE_WIDGET_SHIFT)));
#endif /* SN0 */
		}

		/* Check if the system has a mix of r10k and r12k.
		 * Presence of r12k will determine how the FS bit
		 * is used in FPCSR while loading binaries. Used
		 * in os/machdep.c
	 	 */

		/* Assume mixed for now. If we find out that we
		 * need to do something different for mixed systems
		 * we use this. 
		 */

		r10k_r12k_mixed = 1 ; 

		/* Set up the hub initialization mask and init the lock */
		CNODEMASK_CLRALL(hub_init_mask);
		CNODEMASK_CLRALL(hub_init_done_mask);
		spinlock_init(&hub_mask_lock, "hub_mask_lock");

		/* Deal with situation where master processor # isn't 0. */
		if (master_procid != 0) {
			pdaindr[master_procid].pda = pdaindr[0].pda;
			pdaindr[0].pda = (pda_t *)NULL;
		}

		/* early initialization of iograph */
		iograph_early_init();

		/* Initialize memory */
		mem_init();

#if defined (SN0)
		/* Initialize Hub Pseudodriver Management */
		hubdev_init();
#endif
		/* NMI PROM symbolic disassembly support */
		gda_sym_init();

		/*
		 * if _check_dbg() did not turn kdebug on, turn it off here.
		 * XXX - All mlreset routines must do this.  Why do it here?
		 */
		if (kdebug == -1)
			kdebug = 0;

		/*
		 * Our IO system doesn't require cache writebacks.  Set some
		 * variables appropriately.
		 */
		cachewrback = 0;
		valid_icache_reasons &= ~(CACH_AVOID_VCES | CACH_IO_COHERENCY);
		valid_dcache_reasons &= ~(CACH_AVOID_VCES | CACH_IO_COHERENCY);

		/*
		 * cachecolormask is set from the bits in a vaddr/paddr
		 * which the processor looks at to determine if a VCE
		 * should be raised.  T5 has a 32kb 2-way set associative
		 * primary cache and our page size is 16kb, cachecolormask
		 * for us is zero. Also, T5 automatically handles VCE's in
		 * HW, so we don't have to worry about ever seeing one.
		 */
		cachecolormask = pdcache_size / (NBPP * 2) - 1;

		if ((int)cachecolormask < 0)
			cachecolormask = 0;

		/*
		 * make sure we are running with the right rev of chips
		 */
		verify_snchip_rev();

		if (warbits_override != -1)
			global_wars = warbits_override;

		/*
                 * Since we've wiped out memory at this point, we
                 * need to reset the ARCS vector table so that it
                 * points to appropriate functions in the kernel
                 * itself.  In this way, we can maintain the ARCS
                 * vector table conventions without having to actually
                 * keep redundant PROM code in memory.
                 */
		he_arcs_set_vectors();

#if SABLE
		/*
		 * Tell everyone we're a miniroot kernel.
		 */
		miniroot = 1;

		/* Don't try to do multi-user on sable! */
		{
			int rval;
			rval = kopt_set("initstate", "s");
			ASSERT(!rval);
		}

		splockmeter = 0;	/* Metering takes too long. */
#endif /* SABLE */

	} else { /* slave != 0 */
		/*
		 * This code is performed ONLY by slave processors.
		 */

		/* XXX - Enable interrupts on the slaves. */
	}

	/*
	 * ALL processors do this initialization
	 */

#if !SABLE	/* Was !SABLE_RTL */
	/* RTL simulator inits both the tlbs & the cache for us */

	flush_tlb(TLBFLUSH_NONPDA);	/* flush all BUT pda */

	flush_cache();			/* flush all caches */

#endif /* !SABLE */

}





/* XXX - Move the meat of this to intr.c ? */
/*
 * Set up the platform-dependent fields in the nodepda.
 */
void init_platform_nodepda(nodepda_t *npda, cnodeid_t node)
{
	hubinfo_t hubinfo;

	cnodeid_t i;
	ushort *numcpus_p;

	extern void router_map_init(nodepda_t *);
	extern void router_queue_init(nodepda_t *,cnodeid_t);
#if defined(DEBUG)
	extern lock_t		intr_dev_targ_map_lock;
	extern __uint64_t 	intr_dev_targ_map_size;

	/* Initialize the lock to access the device - target cpu mapping
	 * table. This table is explicitly for debugging purposes only and
	 * to aid the "intrmap" idbg command
	 */
	if (node == 0) {
		/* Make sure we do this only once .
		 * There is always a cnode 0 present.
		 */
		intr_dev_targ_map_size = 0;
		init_spinlock(&intr_dev_targ_map_lock,"dtmap_lock",0);
	}
#endif	/* DEBUG */
	/* Allocate per-node platform-dependent data */
	hubinfo = (hubinfo_t)kmem_alloc_node(sizeof(struct hubinfo_s), VM_NOSLEEP, node);
	ASSERT_ALWAYS(hubinfo);
	npda->pdinfo = (void *)hubinfo;
	hubinfo->h_nodepda = npda;
	hubinfo->h_cnodeid = node;
	hubinfo->h_nasid = COMPACT_TO_NASID_NODEID(node);

	init_spinlock(&hubinfo->h_crblock, "crb_lock", node);

	hubinfo->h_widgetid = hub_widget_id(hubinfo->h_nasid);
	npda->xbow_peer = INVALID_NASID;
	/* Initialize the linked list of
	 * router info pointers to the dependent routers
	 */
	npda->npda_rip_first = NULL;
	/* npda_rip_last always points to the place
	 * where the next element is to be inserted
	 * into the list 
	 */
	npda->npda_rip_last = &npda->npda_rip_first;
	npda->dependent_routers = 0;
	npda->module_id = INVALID_MODULE;

	/*
	 * Mark the tables as uninitialized - one for INT_PEND0, one for
	 * INT_PEND1
	 */
	npda->intr_dispatch0.vector_state = VECTOR_UNINITED;
	npda->intr_dispatch1.vector_state = VECTOR_UNINITED;

	npda->prof_count = 0;
	/* Make sure we have enough characters for METER_NAMSZ (12) */
	ASSERT(node < 10000);
	init_spinlock(&npda->intr_dispatch0.vector_lock, "ivecb0/", node);
	npda->intr_dispatch0.vector_spl = splhi;
	init_spinlock(&npda->intr_dispatch1.vector_lock, "ivecb1/", node);
	npda->intr_dispatch1.vector_spl = splerr;

	npda->vector_unit_busy = 0;

	init_spinlock(&npda->vector_lock, "veclck", node);
	init_sema(&npda->xbow_sema, 0, "xbowsem", node);
	init_spinlock(&npda->fprom_lock, "fprom", node);

	init_spinlock(&npda->node_utlbswitchlock, "utlbsw", node);
	npda->ni_error_print = 0;
	if (need_utlbmiss_patch) {
		npda->node_need_utlbmiss_patch = 1;
		npda->node_utlbmiss_patched = 1;
	}

	/*
	 * Clear out the nasid mask.
	 */
	for (i = 0; i < NASID_MASK_BYTES; i++)
		npda->nasid_mask[i] = 0;

	for (i = 0; i < numnodes; i++) {
		nasid_t nasid = COMPACT_TO_NASID_NODEID(i);

		/* Set my mask bit */
		npda->nasid_mask[nasid / 8] |= (1 << nasid % 8);
	}

	npda->node_first_cpu = get_cnode_cpu(node);

	if (npda->node_first_cpu != CPU_NONE) {
		/*
		 * Count number of cpus only if first CPU is valid.
		 */
		numcpus_p = &npda->node_num_cpus;
		*numcpus_p = 0;
		for (i = npda->node_first_cpu; i < MAXCPUS; i++) {
			if (CPUID_TO_COMPACT_NODEID(i) != node)
			    break;
			else
			    (*numcpus_p)++;
		}
	} else {
		npda->node_num_cpus = 0; 
	}

	/* Allocate memory for the dump stack on each node 
	 * This is useful during nmi handling since we
	 * may not be guaranteed shared memory at that time
	 * which precludes depending on a global dump stack
	 */
	npda->dump_stack = (__uint64_t *)kmem_zalloc_node(DUMP_STACK_SIZE,VM_NOSLEEP,
							  node);
	ASSERT_ALWAYS(npda->dump_stack);
	ASSERT(npda->dump_stack);
	/* Initialize the counter which prevents
	 * both the cpus on a node to proceed with nmi
	 * handling.
	 */
	npda->dump_count = 0;
	/* Setup the (module,slot) --> nic mapping for all the routers
	 * in the system. This is useful during error handling when
	 * there is no shared memory.
	 */
	router_map_init(npda);

	/* Allocate memory for the per-node router traversal queue */
	router_queue_init(npda,node);
#if defined (SN0)
	npda->sbe_info = kmem_zalloc_node_hint(sizeof (sbe_info_t), 0, node);
	ASSERT(npda->sbe_info);
#endif
}


/* XXX - Move the interrupt stuff to intr.c ? */
/*
 * Set up the platform-dependent fields in the processor pda.
 * Must be done _after_ init_platform_nodepda().
 * If we need a lock here, something else is wrong!
 */
void init_platform_pda(pda_t *ppda, cpuid_t cpu)
{
	hub_intmasks_t *intmasks;
	hubreg_t hub_chip_rev;
	cpuinfo_t cpuinfo;
	int i;

	/* Allocate per-cpu platform-dependent data */
	cpuinfo = (cpuinfo_t)kmem_alloc_node(sizeof(struct cpuinfo_s), VM_NOSLEEP, cputocnode(cpu));
	ASSERT_ALWAYS(cpuinfo);
	ppda->pdinfo = (void *)cpuinfo;
	cpuinfo->ci_cpupda = ppda;
	cpuinfo->ci_cpuid = cpu;

	intmasks = &ppda->p_intmasks;

	ASSERT_ALWAYS(&ppda->p_nodepda);

	/* Clear INT_PEND0 masks. */
	for (i = 0; i < N_INTPEND0_MASKS; i++)
		intmasks->intpend0_masks[i] = 0;

	/* Set up pointer to the vector block in the nodepda. */
	intmasks->dispatch0 = &ppda->p_nodepda->intr_dispatch0;

	/* Clear INT_PEND1 masks. */
	for (i = 0; i < N_INTPEND1_MASKS; i++)
		intmasks->intpend1_masks[i] = 0;

#if defined (SN0)	
	if (IP27CONFIG.mach_type == SN00_MACH_TYPE)
		ppda->p_sn00 = 1;
	else
		ppda->p_sn00 = 0;

	hub_chip_rev = get_hub_chiprev(ppda->p_nasid);

	switch (hub_chip_rev) {
	case HUB_REV_2_0:
		ppda->p_warbits = WAR_HUB2_0_WAR_BITS;
		break;
	case HUB_REV_2_1:
		ppda->p_warbits = WAR_HUB2_1_WAR_BITS;
		break;
	case HUB_REV_2_2:
		ppda->p_warbits = WAR_HUB2_2_WAR_BITS;
		break;
	case HUB_REV_2_3:
		ppda->p_warbits = WAR_HUB2_3_WAR_BITS;
		break;
	case HUB_REV_2_4:
		ppda->p_warbits = WAR_HUB2_4_WAR_BITS;
		break;
	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "Unknown hub revision %d", hub_chip_rev);
#endif
		ppda->p_warbits = 0;
		break;
	}
#elif defined (SN1)

	cmn_err(CE_WARN, "Fixme: init_platform_pda for sn1\n");
#endif
	ppda->p_warbits |= global_wars;

	/* Don't read the routers unless we're the master. */
	ppda->p_routertick = 0;

	/* Set up pointer to the vector block in the nodepda. */
	intmasks->dispatch1 = &ppda->p_nodepda->intr_dispatch1;
}

#if defined (SN0)
/*
 * For now, just protect the first page (exception handlers). We
 * may want to protect more stuff later.
 */
void
protect_hub_calias(nasid_t nasid)
{
	paddr_t pa = NODE_OFFSET(nasid) + 0; /* page 0 on node nasid */
	int i;

	for (i = 0; i < MAX_REGIONS; i++) {
		if (i == nasid_to_region(nasid))
			continue;
		/* Protect the exception handlers. */
		*(__psunsigned_t *)BDPRT_ENTRY(pa, i) = MD_PROT_NO;

		/* Protect the ARCS SPB. */
		*(__psunsigned_t *)BDPRT_ENTRY(pa + 4096, i) = MD_PROT_NO;
	}

}

/*
 * Protect the page of low memory used to communicate with the NMI handler.
 */
void
protect_nmi_handler_data(nasid_t nasid, int slice)
{
	paddr_t pa = NODE_OFFSET(nasid) + NMI_OFFSET(nasid, slice);
	int i;

	for (i = 0; i < MAX_REGIONS; i++) {
		if (i == nasid_to_region(nasid))
			continue;
		*(__psunsigned_t *)BDPRT_ENTRY(pa, i) = MD_PROT_NO;
	}

}

#endif /* SN0 */
#if 0
/*
 * Protect areas of memory that we access uncached by marking them as
 * poisoned so the T5 can't read them speculatively and erroneously
 * mark them dirty in its cache only to write them back with old data
 * later.
 */
static void
protect_low_memory(nasid_t nasid)
{
	/* Protect low memory directory */
	poison_state_alter_range(KLDIR_ADDR(nasid), KLDIR_SIZE, 1);

	/* Protect klconfig area */
	poison_state_alter_range(KLCONFIG_ADDR(nasid), KLCONFIG_SIZE(nasid), 1);

	/* Protect the PI error spool area. */
	poison_state_alter_range(PI_ERROR_ADDR(nasid), PI_ERROR_SIZE(nasid), 1);

	/* Protect CPU A's cache error eframe area. */
	poison_state_alter_range(TO_NODE_UNCAC(nasid, CACHE_ERR_EFRAME),
				CACHE_ERR_AREA_SIZE, 1);

	/* Protect CPU B's area */
	poison_state_alter_range(TO_NODE_UNCAC(nasid, CACHE_ERR_EFRAME)
				^ UALIAS_FLIP_BIT,
				CACHE_ERR_AREA_SIZE, 1);
}
#endif

#if defined (SN0)
/*
 * per_hub_init
 *
 * 	This code is executed once for each Hub chip.
 */
void
per_hub_init(cnodeid_t cnode)
{
	__uint64_t	done;
	int		s;
	nasid_t		nasid;
	int		i;
	nodepda_t	*npdap;
	cpuid_t		cpu;
	hubcore_fifod_t	cs_cfdr;
	hubreg_t	ii_icmr;

	nasid = COMPACT_TO_NASID_NODEID(cnode);

	ASSERT(nasid != INVALID_NASID);
	ASSERT(NASID_TO_COMPACT_NODEID(nasid) == cnode);

	/* Grab the hub_mask lock. */
	s = mutex_spinlock(&hub_mask_lock);

	/* Test our bit. */
	if (!(done = CNODEMASK_TSTB(hub_init_mask, cnode))) {

		/* Turn our bit on in the mask. */
		CNODEMASK_SETB(hub_init_mask, cnode);
	}

#if defined(SN0_HWDEBUG)
	hub_config_setup();
#endif

	/* Release the hub_mask lock. */
	mutex_spinunlock(&hub_mask_lock, s);

	/*
	 * Do the actual initialization if it hasn't been done yet.
	 * We don't need to hold a lock for this work.
	 */
	if (!done) {
		long nigp;
		long nidp;

		npdap = NODEPDA(cnode);

		npdap->hub_chip_rev = get_hub_chiprev(nasid);

		for (i = 0; i < CPUS_PER_NODE; i++) {
			cpu = cnode_slice_to_cpuid(cnode, i);
			if (!cpu_enabled(cpu))
			    SET_CPU_LEDS(nasid, i, 0xf);
		}

		/*
		 * Set TAILTOWRAP to 0xffff
		 * Clear PORTTORESET
		 * Part of the spurious message workaround.
		 * Only needed if there is a metarouter.
		 */
		if (hasmetarouter) {
			nigp = REMOTE_HUB_L(nasid, NI_GLOBAL_PARMS);
			nigp |= 0xffffL << NGP_TAILTOWRAP_SHFT;
			REMOTE_HUB_S(nasid, NI_GLOBAL_PARMS, nigp);
			nidp = REMOTE_HUB_L(nasid, NI_DIAG_PARMS);
			nidp &= ~NDP_PORTTORESET;
			REMOTE_HUB_S(nasid, NI_DIAG_PARMS, nidp);
		}

		/*
		 * XXX - This will eventually be done by the prom
		 *  Enable aging.
		 *
		 * Bug #650455 workaround - disable aging (value was
		 *   0xfe) for hub starvation.
		 *
		 * Bug #761666. Several crashes at customer sites indicate
		 * that the fixed setting to 0x0 causes problem when we try
		 * to work around hub starvation. Make this a systune
		 * and set it to 0xfe (power on value is 0xff)
		 */
		{
			extern int hub_core_arb_ctrl;

			REMOTE_HUB_S(nasid, CORE_ARB_CTRL, hub_core_arb_ctrl);
		}


		/*	
		 * This code enables virtual channel adaption.
		 */

		for (i = NI_AGE_REG_MIN; i <= NI_AGE_REG_MAX; i+= sizeof(hubreg_t)) {
			hubreg_t age_reg;

			age_reg = REMOTE_HUB_L(nasid, i);

			/* Turn off virtual channel and congestion ctrl bits */
			age_reg &= ~(NAGE_VCH_MASK | NAGE_CC_MASK);

			/*
			 * Turn on Virtual channel
			 * and congestion control bits.
			 * Part of the spurious message workaround.
			 * Only needed if there is a metarouter.
			 */
			if (hasmetarouter && (i & 0xf) == 0) {
				/* Set CC and VCH */
				age_reg |= (1 << NAGE_CC_SHFT);
				age_reg |= (2 << NAGE_VCH_SHFT);
			}

#if OLD_VCH_ALLOCATION
			if ((i == NI_AGE_CPU0_MEMORY) ||
			    (i == NI_AGE_CPU1_MEMORY))
				age_reg |= (VCHANNEL_A << NAGE_VCH_SHFT);
			else
				age_reg |= (VCHANNEL_B << NAGE_VCH_SHFT);
#endif

			age_reg |= (VCHANNEL_A << NAGE_VCH_SHFT);

			REMOTE_HUB_S(nasid, i, age_reg);
		}


		/*
		 * Change the Hub PI PIQ depth to 0x1 from the reset value
		 * of 0x12. This helps to reduce the probability of POQ bug. 
		 * This fix is needed only on Rev 3 (hub 2.1) of hub. 
		 * This bug does not exist on other revs of hub. 
		 * We will do it in all Hubs of rev <= 5 (hub 2.3) for now.
		 * We could end up not doing it if needed.
		 */

		if (npdap->hub_chip_rev <= HUB_REV_2_3) {
			cs_cfdr.hc_fifod_value = REMOTE_HUB_L(nasid, CORE_FIFO_DEPTH);
			cs_cfdr.hc_fifod_fields.fifod_piq_req = 1;
			REMOTE_HUB_S(nasid, CORE_FIFO_DEPTH, cs_cfdr.hc_fifod_value);
			/* Reading back makes sure data is written */
			cs_cfdr.hc_fifod_value = REMOTE_HUB_L(nasid, CORE_FIFO_DEPTH);
		}
#ifdef SABLE
		/*
		 * This work is only necessary when we're not run by a real
		 * prom.
		 */
		init_local_dir();
#endif
		/*
		 * Protect low memory data structures from overzealous T5
		 * "speculative writes."
		 */
#if 0
		/* XXX - Needs a little more work to be safe */
		/* if you uncomment this, you need to uncomment
		 * the protect_low_memory function itself.
		 */
		protect_low_memory(nasid);
#endif

#if (SN0_HWDEBUG)
		hub_config_init(nasid);
#else

		/* 
		 * Set II to be in imprecise mode, c_cnt = 4, p_cnt = 0,
		 *   bte_crb_cnt = 2
		 * This avoids POQ/IVACK problems on hub 2.1, 2.3 systems.
		 * (WAR for bug 636960)
		 */
		if (npdap->hub_chip_rev <= HUB_REV_2_3) {
			const int iio_icmr_c_cnt = 4, iio_icmr_p_cnt = 0;
			const int iio_bte_crb_cnt = 2;

			REMOTE_HUB_S(nasid, IIO_BTE_CRB_CNT, iio_bte_crb_cnt);
			ii_icmr = REMOTE_HUB_L(nasid, IIO_ICMR);
			ii_icmr &= ~(IIO_ICMR_C_CNT_MASK | IIO_ICMR_P_CNT_MASK |
				     IIO_ICMR_PRECISE);
			ii_icmr |= (iio_icmr_c_cnt << IIO_ICMR_C_CNT_SHFT) |
				   (iio_icmr_p_cnt << IIO_ICMR_P_CNT_SHFT);
			REMOTE_HUB_S(nasid, IIO_ICMR, ii_icmr);
		}

		/*
		 * Hub rev 2.4 has fixed the missing IVACK problem. Since we
		 * do not have the HW ready for testing at this time we add
		 * systuneables which will be only effective for 2.4 Hubs with
		 * the default settings for pre 2.4 hubs.
		 * This allows us to dial the paramters up in the field to
		 * address performance issues
		 */
		if (npdap->hub_chip_rev == HUB_REV_2_4) {
			extern int hub_2_4_iio_icmr_c_cnt;
			extern int hub_2_4_iio_icmr_p_cnt;
			extern int hub_2_4_iio_bte_crb_cnt;

			REMOTE_HUB_S(nasid, IIO_BTE_CRB_CNT, hub_2_4_iio_bte_crb_cnt);
			ii_icmr = REMOTE_HUB_L(nasid, IIO_ICMR);
			ii_icmr &= ~(IIO_ICMR_C_CNT_MASK | IIO_ICMR_P_CNT_MASK |
				     IIO_ICMR_PRECISE);
			ii_icmr |= (hub_2_4_iio_icmr_c_cnt << IIO_ICMR_C_CNT_SHFT) |
				   (hub_2_4_iio_icmr_p_cnt << IIO_ICMR_P_CNT_SHFT);
			REMOTE_HUB_S(nasid, IIO_ICMR, ii_icmr);
		}


		/*
		 * Set CRB timeout to be 10ms.
		 */
#ifdef SN0XXL
		REMOTE_HUB_S(nasid, IIO_ICTP, 0xfffe );
#else
		REMOTE_HUB_S(nasid, IIO_ICTP, 0x1000 );
#endif
		REMOTE_HUB_S(nasid, IIO_ICTO, 0xff);

#endif /* SN0_HWDEBUG */


		/* Reserve all of the hardwired interrupt levels. */
		intr_reserve_hardwired(cnode);

		/* Initialize error interrupts for this hub. */
		hub_error_init(cnode);

		/* Set up correctable memory/directory ECC error interrupt. */
		install_eccintr(cnode);

		/* Protect our exception vectors from accidental corruption. */
		protect_hub_calias(nasid);

		/* Enable RT clock interrupts */
		hub_rtc_init(cnode);
		hub_migrintr_init(cnode); /* Enable migration interrupt */

#if defined (HUB_II_IFDR_WAR)
		hub_setup_kick(cnode);
		hub_init_ifdr_war(cnode);
#endif

		/*
		 * Fix the timeout to be a fairly large value.
		 * At boot time, it comes up as 0x64
		 * The prom sets it to 0x2000.
		 * For systems with a metarouter, we need it higher still.
		 */
		if (hasmetarouter) {
#ifdef SN0XXL
			REMOTE_HUB_S(nasid, PI_CRB_SFACTOR, 0xfffe);
			REMOTE_HUB_S(nasid, PI_MAX_CRB_TIMEOUT, 0xfe);
#else
			REMOTE_HUB_S(nasid, PI_CRB_SFACTOR, 0xd400);
			REMOTE_HUB_S(nasid, PI_MAX_CRB_TIMEOUT, 0xd0);
#endif
		}

		/* reduce size of moq to prevent overflow corruption */
		REMOTE_HUB_S(nasid, MD_MOQ_SIZE,
			((0x30 << MMS_RP_SIZE_SHFT) | 0x12));

		s = mutex_spinlock(&hub_mask_lock);
		CNODEMASK_SETB(hub_init_done_mask, cnode);
		mutex_spinunlock(&hub_mask_lock, s);

	} else {
		/*
		 * Wait for the other CPU to complete the initialization.
		 */
		while (CNODEMASK_TSTB(hub_init_done_mask, cnode) == 0)
			/* LOOP */
			;
	}
}

#elif defined (SN1)

void
per_hub_init(cnodeid_t cnode)
{

	cmn_err(CE_NOTE, "fixme: per_hub_init");
}

#endif

/*
 * per_cpu_init
 *
 *	This stuff needs to be called on all cpus.  We call it from
 * allowboot on the master CPU and from cboot on the slaves.
 */
void
per_cpu_init(void)
{
	cpuid_t cpu =	getcpuid();
	cnodeid_t cnode = get_compact_nodeid();

	ASSERT(cnode != INVALID_CNODEID);
	ASSERT((cpu >= 0) && (cpu < maxcpus));

	/* Set up our CPU ID. */
	private.p_cpuid = cpu;

	/* Set up the interrupt tables for this CPU. */
	intr_init();

	/* Initialize this hub if it hasn't been done yet. */
	per_hub_init(cnode);

	/* Set up my cpuaction interrupt. */
	install_cpuintr(cpu);

	/* Set up my debug interrupt. */
	install_dbgintr(cpu);

	/* Set up my tlb interrupt. */
	install_tlbintr(cpu);

	/* Install our NMI handler if symmon hasn't installed one. */
	install_cpu_nmi_handler(cputoslice(cpu));

#if defined (SN0)
	/* Protect the locore page used by our NMI handler. */
	protect_nmi_handler_data(COMPACT_TO_NASID_NODEID(cnode),
				 cputoslice(cpu));
#endif
}


/*
 * Allow interrupts to reach cpus.
 */
void
allowintrs(void)
{
	/* XXX - Enable INTPEND0, INTPEND1, etc. */

	/* Do we need this?  We're enabling things as we go.
	 * They stay off in T5 until spl0().
	 */
}



cpuid_t
cnodetocpu(cnodeid_t cnode)
{
	cpuid_t		cpu;
	int		i;

	if ((cpu = CNODE_TO_CPU_BASE(cnode)) == CPU_NONE)
	    return CPU_NONE;

	for (i = 0; i < CNODE_NUM_CPUS(cnode); i++)
	    if (cpu_enabled(cpu + i))
		return cpu + i;

	return CPU_NONE;
}

/*
 * get_compact_nodeid() returns the virtual node id number of the caller.
 */
cnodeid_t
get_compact_nodeid(void)
{
	nasid_t nasid;

	nasid = get_nasid();


	/*
	 * Map the physical node id to a virtual node id (virtual node ids
	 * are roughly contiguous).
 	 */
	return NASID_TO_COMPACT_NODEID(nasid);
}


/*
 * getcpuid() returns the compact CPU id number of the caller.  This
 * ends up being a combination of NASID and cpu "slice."
 * The funciton has been names this way for compatibility.
 */
cpuid_t
getcpuid(void)
{
	klcpu_t *klcpu;
#if defined (SABLE)
        if (fake_prom)
	    return ((get_nasid() << 1) | LOCAL_HUB_L(PI_CPU_NUM));
#endif

	klcpu = nasid_slice_to_cpuinfo(get_nasid(),LOCAL_HUB_L(PI_CPU_NUM));
	ASSERT_ALWAYS(klcpu);
	return klcpu->cpu_info.virtid;
}

/*
 * cpu_unenable(cpuid_t cpu)
 *
 *	Turn off a CPU.
 */
void
cpu_unenable(cpuid_t cpu)
{
	CPUMASK_CLRB(boot_cpumask, cpu);
	pdaindr[cpu].CpuId = CPU_NONE;
}

/*
 * cpu_enabled(cpuid_t cpu)
 *
 *	If the CPU's in the boot_mask, it's enabled.  Perhaps we'll
 *	want to do something more elaborate here later.
 */
int
cpu_enabled(cpuid_t cpu)
{
	if (cpu == CPU_NONE)
	    return 0;

	return (CPUMASK_TSTB(boot_cpumask, cpu) != 0);
}


cpuid_t
cnode_slice_to_cpuid(cnodeid_t cnode, int slice)
{
	cpuid_t cpu;
	int num_cpus;

	num_cpus = NODEPDA(cnode)->node_num_cpus;
	for (cpu = NODEPDA(cnode)->node_first_cpu;
	     (cpu >= 0) && num_cpus; cpu++, num_cpus--) 
		if (cpu_enabled(cpu) && (cputoslice(cpu) == slice))
		    return cpu;
	return CPU_NONE;
}

void
build_cpumap(cnodeid_t* map)
{
	cnodeid_t n;
	int j;
	for (n = 0; n < numcpus; n++)
		map[n] = (cnodeid_t) CNODEID_NONE;
	for (n = 0; n < numnodes; n++)
		for (j = 0; j < CPUS_PER_NODE; j++) {
			cpuid_t cpu = cnode_slice_to_cpuid(n, j);
			if (0 <= cpu && cpu < numcpus)
				map[cpu] = n;
		}
}
		



static void
gen_region_mask(hubreg_t *region_mask, int maxnodes)
{
	cnodeid_t cnode;

	(*region_mask) = 0;

	for (cnode = 0; cnode < maxnodes; cnode++) {
		(*region_mask) |= 1ULL << get_region(cnode);
	}
}

/*
 * is_cpuless(cnodeid_t cnode)
 *
 *	If there's at least one CPU on a node, return 0, else return 1.
 */
int
is_cpuless(cnodeid_t cnode)
{
	if (cnodetocpu(cnode) == CPU_NONE)
		return 1;
	else
		return 0;
}

int
cpuboard(void)
{

#if R10000
	ASSERT(cputype == CPU_IP27);
	return(INV_IP27BOARD);
#elif BEAST
	ASSERT(cputype == CPU_IP33);
	return(INV_IP33BOARD);
#else
#error <NEED cpu board here>
#endif

}


/*
 * Floating point coprocessor routines.
 */

void
fp_find(void)
{
	private.p_fputype_word = get_fpc_irr();
}

void
fp_init(void)
{
	set_fpc_csr(0);
}


/*
 * get_except_norm
 *
 *      return address of exception handler for all exceptions other
 *      then utlbmiss.
 */
inst_t *
get_except_norm(void)
{
	extern inst_t exception[];

	return exception;
}

static void
gda_sym_init(void)
{
	gda_t *gdap = GDA;
	extern int symmax;
	extern struct dbstbl dbstab[];
	extern char nametab[];

	if (kdebug) {
		gdap->g_symmax = symmax;
		gdap->g_dbstab = &dbstab[0];
		gdap->g_nametab = &nametab[0];
	} else {
		gdap->g_symmax = 0;
		gdap->g_dbstab = (struct dbstbl *)NULL;
		gdap->g_nametab = (char *)NULL;
	}
}

extern void
update_node_information(cnodeid_t cnodeid)
{
	nodepda_t *npda = NODEPDA(cnodeid);
	nodepda_router_info_t *npda_rip;
	
	/* Go through the list of router info 
	 * structures and copy some frequently
	 * accessed info from the info hanging
	 * off the corresponding router vertices
	 */
	npda_rip = npda->npda_rip_first;
	while(npda_rip) {
		if (npda_rip->router_infop) {
			npda_rip->router_portmask = 
				npda_rip->router_infop->ri_portmask;
			npda_rip->router_slot = 
				npda_rip->router_infop->ri_slotnum;
		} else {
			/* No router, no ports. */
			npda_rip->router_portmask = 0;
		}
		npda_rip = npda_rip->router_next;
	}
}

#if defined (SN0)
hubreg_t
get_region(cnodeid_t cnode)
{
	if (fine_mode)
		return COMPACT_TO_NASID_NODEID(cnode) >> NASID_TO_FINEREG_SHFT;
	else
		return COMPACT_TO_NASID_NODEID(cnode) >> NASID_TO_COARSEREG_SHFT;
}

hubreg_t
nasid_to_region(nasid_t nasid)
{
	if (fine_mode)
		return nasid >> NASID_TO_FINEREG_SHFT;
	else
		return nasid >> NASID_TO_COARSEREG_SHFT;
}

#endif


/*
 * This is an undocumented feature that allows the system to boot & use a subset
 * of the available cpus. To use this feature, at the PROM, enter:
 *		setenv maxnodes <n>
 *
 * Only cpus on the first <n> nodes will be used. Note that some of the 
 * router discovery will fail (harmless).
 */
static void
redo_nodes(int num)
{
	gda_t *gda = GDA;
	
	if (num > 0 && num < MAX_COMPACT_NODES)
		gda->g_nasidtable[num] = INVALID_NASID;
}


#ifdef SABLE

/*
 * init_kldir
 */


static void init_kldir(nasid_t nasid)
{
#if defined (SN0)
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
#endif
}



void
init_gda(gda_t *gdap)
{
	int i;

	if (get_nasid() != master_nasid)
		return;

	gdap->g_magic = GDA_MAGIC;
	gdap->g_version = GDA_VERSION;
	gdap->g_promop = PROMOP_INVALID;
	gdap->g_masterid = makespnum(master_nasid, LOCAL_HUB_L(PI_CPU_NUM));

	gdap->g_vds = 0;

	/* Clear the NASID table */
	for (i = 1; i < MAX_COMPACT_NODES; i++)
		gdap->g_nasidtable[i] = INVALID_CNODEID;

	if (fake_prom)
		for (i = 1; i < numnodes; i++)
			gdap->g_nasidtable[i] = i;

	/* We're the master node so we get to be node 0 */
	if (master_nasid)
		gdap->g_nasidtable[master_nasid] = 0;
	gdap->g_nasidtable[0] = master_nasid;
}

void
propagate_lowmem(gda_t *gdap, __int64_t spb_offset)
{
	int i;
	kldir_ent_t *ke;
	nasid_t nasid;

	/*
         * The GDA is already available on this node (the master)
         * but a pointer to it needs to be propagated to all other nodes.
         * Therefore, start at node 1 (0 is the master) and copy away.
         */
	for (i = 1; i < numnodes; i++) {
		nasid = gdap->g_nasidtable[i];
		if (nasid == INVALID_NASID) {
			printf("propagate_gda: Error - bad nasid (%d) for cnode %d\n",
				nasid, i);
			continue;
		}

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
		 * Also note: We're copying into uncached space to avoid calias
		 * problems when copying to node 0.
		 */
		bcopy(SPB, (void *)NODE_OFFSET_TO_K1(nasid, spb_offset),
			sizeof(*SPB));
#if defined (SN0)
		/* Set its CALIAS_SIZE */
		REMOTE_HUB_S(nasid, PI_CALIAS_SIZE, PI_CALIAS_SIZE_8K);
#endif
	}
}


/*
 * Initialize the portions of the low memory directory that need to be
 * set up in the IO6 prom.
 */
void init_local_dir()
{
	kldir_ent_t *ke;
	nasid_t nasid = get_nasid();

	ke = KLD_LAUNCH(nasid);

	if (ke->magic != KLDIR_MAGIC) {
		fake_prom = 1;
		init_kldir(nasid);

		ke = KLD_GDA(nasid);

		ke->magic  = KLDIR_MAGIC;
		/* There's only one GDA per kernel.  Set up an absolute ptr. */
		ke->pointer = PHYS_TO_K0(NODE_OFFSET(master_nasid) + IO6_GDA_OFFSET);
		ke->size   = IO6_GDA_SIZE;
		ke->count  = IO6_GDA_COUNT;
		ke->stride = IO6_GDA_STRIDE;

		init_gda(GDA);
	}

	return;
}

#endif /* SABLE */

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
void
copy_whole_kernel(nasid_t src, nasid_t dest)
{
	void *src_addr;
	void *dest_addr;	
	int  size;

	src_addr = (void *)PHYS_TO_K0(TO_NODE(src, KERNEL_START_OFFSET));
	dest_addr = (void *)PHYS_TO_K0(TO_NODE(dest, KERNEL_START_OFFSET));
	size = KERNEL_END_OFFSET - KERNEL_START_OFFSET;

	bcopy((caddr_t)src_addr, (caddr_t)dest_addr, size);
}

void
copy_cell_gda(int ndx)
{
	gda_t *gda = GDA;
	kldir_ent_t *ke;	
	nasid_t master = gda->g_nasidtable[ndx];

	ke = KLD_GDA(master);
	ke->pointer = PHYS_TO_K0(NODE_OFFSET(master) + IO6_GDA_OFFSET);
	*(gda_t *)ke->pointer = *gda;
}


void
repair_gda(int ndx, int nodes, partid_t part)
{
	gda_t *gda = GDA;
	gda_t *new_gda;
	kldir_ent_t *ke;	
	nasid_t master = gda->g_nasidtable[ndx];
	int j;

	ke = KLD_GDA(master);
	ke->pointer = PHYS_TO_K0(NODE_OFFSET(master) + IO6_GDA_OFFSET);
	
	new_gda = (gda_t *)GDA_ADDR(master);

	for (j = 0; j < nodes; j++) 
		new_gda->g_nasidtable[j] = new_gda->g_nasidtable[ndx + j];

	for (; j < MAX_NASIDS; j++)
	    new_gda->g_nasidtable[j] = INVALID_NASID;

	for (j = 1; j < nodes; j++) {
		ke = KLD_GDA(new_gda->g_nasidtable[j]);
		ke->pointer = (__psunsigned_t)new_gda;
	}
	if (new_gda->g_version < PART_GDA_VERSION)
	    new_gda->g_version = PART_GDA_VERSION;

	new_gda->g_partid = part;
	
}

void
copy_args(nasid_t master, int argc, char *argv[], char *environ[])
{
	__psunsigned_t *args = (__psunsigned_t *)PHYS_TO_K0(TO_NODE(master, IO6_PROM_OFFSET));
	char *strings = (char *)PHYS_TO_K0(TO_NODE(master, 
						   IO6_PROM_OFFSET + 0x20000));

	__psunsigned_t *arg_array = (__uint64_t *)args + 3;

	*(args++) = (__psunsigned_t)argc;
	
	/* argv */
	*(args++) = (__psunsigned_t)(arg_array);
	for (; *argv; argv++) {	
		strcpy(strings, *argv);
		*(arg_array++) = (__psunsigned_t)strings;
		strings += strlen(*argv) + 1;
	}
	*(arg_array++) = NULL;
	
	/* environ */
	*(args) = (__psunsigned_t)(arg_array);
	for (; *environ; environ++) {
		strcpy(strings, *environ);
		*(arg_array++) = (__psunsigned_t)strings;
		strings += strlen(*environ) + 1;
	}
	*(arg_array) = NULL;
}

extern void slave_cell_start(void);

void
launch_cell(nasid_t master)
{
	__psunsigned_t start_addr;
	__uint64_t sp;
	int slice;
	klcpu_t *acpu;

	start_addr = (__psunsigned_t)slave_cell_start & MAPPED_KERN_PAGEMASK;
	start_addr |= NODE_CAC_BASE(master);

	copy_whole_kernel(get_nasid(), master);
	sp = PHYS_TO_K0(TO_NODE(master, TO_PHYS((__uint64_t)getsp())));

	for (slice = 0; slice < CPUS_PER_NODE; slice++) {
		acpu = nasid_slice_to_cpuinfo(master, slice);
		if (acpu && KLCONFIG_INFO_ENABLED(&acpu->cpu_info))
			break;
	}
	

	cmn_err(CE_NOTE, "starting subcell: master %d, slice %d at 0x%x\n", master, slice, start_addr);
	if (slice == CPUS_PER_NODE)
	    return;

	LAUNCH_SLAVE(master, slice,
		     (launch_proc_t)start_addr,
		     0,
		     (void *)sp,
		     0); /* Don't need to set the gp */
}


#define nodes_in_cell(x)	\
     ((num_nodes / num_cells) + \
      ((num_nodes - (num_nodes / num_cells) * num_cells) <= (x) ? 0 : 1))

nasid_t partition_master = 0;
hubreg_t partition_region_mask = 0;
nasid_t  partition_nasidtable[MAX_COMPACT_NODES] = {
	0, 0
};

void
redo_cells(int argc, char **argv, char **environ)
{
	int num_cells = 1, max_sub_cells = 4, num_nodes = 0;
	int i, ndx;
	gda_t *gda = GDA;

	partid_t part = part_get_promid();
	
	if (( KL_CONFIG_CH_CONS_INFO(get_nasid())->nasid) != get_nasid())
	    return;

	if (is_specified(arg_subcells))
	    num_cells = atoi(arg_subcells);
	
	if (is_specified(arg_max_subcells))
	    max_sub_cells = atoi(arg_max_subcells);

	if (max_sub_cells < num_cells) {
		cmn_err(CE_NOTE, "Incorrect logical cell configuration: no logical cells configured");
		return;
	}
	partition_master = gda->g_nasidtable[0];
	partition_region_mask = 0;	    
	for (i = 0; i < MAX_COMPACT_NODES; i++) {
		hubreg_t rgn;

		if (gda->g_nasidtable[i] == INVALID_NASID) 
		    break;
		
		rgn = is_fine_dirmode() ?
		    ((gda->g_nasidtable[i]) >> NASID_TO_FINEREG_SHFT) :
			((gda->g_nasidtable[i]) >> NASID_TO_COARSEREG_SHFT);

		partition_nasidtable[i] = gda->g_nasidtable[i];
		partition_region_mask |= 1ULL << rgn;
		num_nodes++;
	}

	for (; i < MAX_COMPACT_NODES; i++)
	    partition_nasidtable[i] = INVALID_NASID;

	
	for (i = 0, ndx = 0;  i < (num_cells - 1); i++) {
		if (nodes_in_cell(i) == 0)
		    continue;
		ndx += nodes_in_cell(i);
		copy_cell_gda(ndx);
	}

	for (i = 0, ndx = 0; i < (num_cells - 1); i++) {
		if (nodes_in_cell(i) == 0)
		    continue;
		ndx += nodes_in_cell(i);
		copy_args(gda->g_nasidtable[ndx], argc, argv, environ);
	}

	for (i = 0, ndx = 0; i < (num_cells - 1); i++) {
		if (nodes_in_cell(i) == 0)
		    continue;
		ndx += nodes_in_cell(i);
		repair_gda(ndx, nodes_in_cell(i + 1), 
			   part * max_sub_cells + (i + 1));
		launch_cell(gda->g_nasidtable[ndx]);

	}
	repair_gda(0, nodes_in_cell(0),  (part * max_sub_cells));
}


#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */


#if defined(SN0) && defined(SN0_HWDEBUG)

/*
 * setup the values for hub configuration registers
 *
 * per_hub_init from per_hub_init() protected by hub_mask_lock
 */
void 
hub_config_setup(void)
{
	static int done = 0;

	if (done)
		return;

	if (strlen(arg_iio_icmr_precise)) 
		iio_icmr_precise = atoi(arg_iio_icmr_precise);

	if (strlen(arg_iio_icmr_c_cnt))
		iio_icmr_c_cnt = atoi(arg_iio_icmr_c_cnt);

	if (strlen(arg_iio_bte_crb_count))
		iio_bte_crb_count = atoi(arg_iio_bte_crb_count);

	if (strlen(arg_iio_ictp)) 
		iio_ictp = atoi(arg_iio_ictp);

	if (strlen(arg_iio_icto))
		iio_icto = atoi(arg_iio_icto);

	if (strlen(arg_disable_bte))
		disable_bte = atoi(arg_disable_bte);
	if (strlen(arg_disable_bte_pbcopy))
		disable_bte_pbcopy = atoi(arg_disable_bte_pbcopy);
	if (strlen(arg_disable_bte_pbzero))
		disable_bte_pbzero = atoi(arg_disable_bte_pbzero);
	if (strlen(arg_disable_bte_poison_range))
		disable_bte_poison_range = atoi(arg_disable_bte_poison_range);

	/*
	 * Logic Analyzer trigger points in PCI error interrupt
	 */
	if (strlen(arg_la_trigger_nasid1))
		la_trigger_nasid1 = atoi(arg_la_trigger_nasid1);
	if (strlen(arg_la_trigger_nasid2))
		la_trigger_nasid2 = atoi(arg_la_trigger_nasid2);
	if (strlen(arg_la_trigger_val))
		la_trigger_val = atoi(arg_la_trigger_val);

	done = 1;
}


void
hub_config_init(nasid_t nasid)
{
	hubreg_t ii_icmr;


	if (iio_icmr_precise != -1) {
		ii_icmr = REMOTE_HUB_L(nasid, IIO_ICMR);
		if (iio_icmr_precise) {
			ii_icmr |= IIO_ICMR_PRECISE;
		} else {
			ii_icmr &= ~IIO_ICMR_PRECISE;
		}	
		REMOTE_HUB_S(nasid, IIO_ICMR, ii_icmr);
	}

	if (iio_icmr_c_cnt != -1) {
		ii_icmr = REMOTE_HUB_L(nasid, IIO_ICMR);
		ii_icmr &= ~IIO_ICMR_C_CNT_MASK;
		ii_icmr |= (iio_icmr_c_cnt << IIO_ICMR_C_CNT_SHFT);
		REMOTE_HUB_S(nasid, IIO_ICMR, ii_icmr);
	}

	
	if (iio_bte_crb_count != -1)	
		REMOTE_HUB_S(nasid, IIO_BTE_CRB_CNT, iio_bte_crb_count);

	if (iio_ictp != -1) 
		REMOTE_HUB_S(nasid, IIO_ICTP, iio_ictp);

	if (iio_icto != -1)
		REMOTE_HUB_S(nasid, IIO_ICTO, iio_icto);
}

	

#define PRINTCNF(name,arg,val) { 			   	     	\
		if (strlen(arg))				     	\
			printf("PROM setting overwrites systune, ");	\
		printf("%s ",name);					\
		if (val == -1)						\
			printf("is NOT set\n");				\
		else							\
			printf("= 0x%x\n",val);				\
		}

	
void
hub_config_print(void)
{
	PRINTCNF("iio_icmr_precise" ,arg_iio_icmr_precise ,iio_icmr_precise);
	PRINTCNF("iio_icmr_c_cnt"   ,arg_iio_icmr_c_cnt   ,iio_icmr_c_cnt);
	PRINTCNF("iio_bte_crb_count",arg_iio_bte_crb_count,iio_bte_crb_count);
	PRINTCNF("iio_ictp"         ,arg_iio_ictp         ,iio_ictp);
	PRINTCNF("iio_icto"         ,arg_iio_icto         ,iio_icto);

	
	PRINTCNF("disable_bte",arg_disable_bte,disable_bte);
	PRINTCNF("disable_bte_pbcopy",arg_disable_bte_pbcopy,	
					disable_bte_pbcopy);
	PRINTCNF("disable_bte_pbzero",arg_disable_bte_pbzero,	
					disable_bte_pbzero);
	PRINTCNF("disable_bte_poison_range",arg_disable_bte_poison_range,	
					disable_bte_poison_range);

	if (la_trigger_nasid1 == -1 && la_trigger_nasid2 == -1) {
		printf("no LA trigger points set for PCI error intr\n");
	} else {
		if (la_trigger_nasid1 != -1) 
			printf(
			"LA trigger point 1 for PCI error intr NASID = %d\n",
			la_trigger_nasid1);
		if (la_trigger_nasid2 != -1) 
			printf(
			"LA trigger point 2 for PCI error intr NASID = %d\n",
			la_trigger_nasid2);

		printf("LA trigger value = 0x%x + 0xcafe\n",la_trigger_val);

	}
}	
#endif
