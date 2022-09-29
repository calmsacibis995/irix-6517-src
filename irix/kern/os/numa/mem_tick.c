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

/* 
 * Per node memory tick
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <sys/sat.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/idbgentry.h>
#include <sys/siginfo.h>
#include <sys/sysmacros.h>
#include <sys/pda.h>
#include <sys/timers.h>
#include <ksys/sthread.h>
#include <sys/schedctl.h>
#include <sys/nodepda.h>
#include <sys/par.h>
#include "pfms.h"
#include "migr_periodic_op.h"
#include "migr_bounce.h"
#include "debug_levels.h"
#include "numa_init.h"
#include "migr_control.h"
#include "migr_queue.h"
#include "migr_manager.h"
#include "numa_hw.h"
#include "numa_stats.h"
#include "migr_unpegging.h"
#include "mem_tick.h"
#include "os/proc/pproc_private.h"


/*
 * Local functions 
 */
#ifdef NOTYET
static void memoryd(void);
#endif

/* 
 * memoryd just takes care of the asynchronous migration queue requests
 * There's just one global instance of this daemon.
 */

int
memoryd_init(void)
{
#ifdef NOTYET
        
	/*
         * Initialize the semaphore to wake up memory daemon
         */
	initnsema(&memoryd_sema, 0, "memoryd");
        
        if (newproc()) {
                extern void vmp_set_weight(kthread_t *);
                bcopy("memoryd", curprocp->p_psargs, 8);
                bcopy("memoryd", curprocp->p_comm, 8);
                        
                kt_initialize_pri(curthreadp, PTIME_SHARE);
                vmp_set_weight(curthreadp);
                        
                memoryd();
        }
#endif
        return (0);
}

/*
 * An individual numa daemon runs on one node
 */
#ifdef NOTYET
static void
memoryd(void)
{
	for (;;) {

                cnodeid_t nodeid = cnodeid();
                NUMA_STAT_INCR(nodeid, memoryd_total_activations);

                /*
                 * Invoke on-demand queue migration daemon
                 * Triggers: capacity, coalescing are on demand triggers
                 *           traffic, time are periodic triggers
                 */
                if (nodepda->mcd->migr_system_kparms.migr_queue_control_enabled) {
                        migr_queue_migrate(NODEPDA_MCD(nodeid)->migr_queue);

                }

	}
}
#endif

void
mem_tick_init_node(cnodeid_t node)
{
        int base_numpfns_per_cpu;
        int cpu;
        int eff_cpus_on_node;
        int max_cpus_on_node;
        int acc_numpfns;
        cpuid_t lcpuid;
        pda_t* pda;
        pfn_t start_cpfn;
        uint node_totalmem;
	extern int numa_migr_default_mode;

        /*
         * Get max cpus on node
         */
        max_cpus_on_node = CNODE_NUM_CPUS(node);
                
        /*
         * Determine effective number of cpus available on this node
         */
        eff_cpus_on_node = 0;
        for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                if (cpu_enabled(lcpuid)) {
                        eff_cpus_on_node++;
                }
        }

        /*
         * Distribute pfns in this node across all available cpus
         */
        if (eff_cpus_on_node == 0) {
                /*
                 * No enabled cpus on this node;
                 * For now, we don't do any mem_ticks on these nodes
                 */
                cmn_err(CE_WARN, "[mem_tick_init]: No cpus on node %d\n",
                        node);
                return;
        }

        node_totalmem = pfn_getnumber(node);
        base_numpfns_per_cpu = node_totalmem / eff_cpus_on_node;

        acc_numpfns = 0;
        /* start_cpfn is a contiguous space pfn counter */
        start_cpfn = pfdattopfn(PFD_LOW(node));
        /* loop through all cpus in node except last */
        for (cpu = 0; cpu < (max_cpus_on_node - 1); cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        /* number of pfns in charge of this cpu */
                        pda->p_mem_tick_cpu_numpfns = base_numpfns_per_cpu;
                        /* initial pfn for this cpu - need convertion from cont to disct space  */
                        pda->p_mem_tick_cpu_startpfn = cpfn_to_pfn(node, start_cpfn);
                        /* pfns already assigned */
                        acc_numpfns += base_numpfns_per_cpu;
                        /* incr contiguous space cpfn (assumes contiguous physical space */
                        start_cpfn += base_numpfns_per_cpu;
                }
        }

        /* now we assign the leftover pfns to the last cpu */
        if (max_cpus_on_node > 0) {
                lcpuid = CNODE_TO_CPU_BASE(node) + (max_cpus_on_node - 1);
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        /* assign leftover pfns to last cpu in node */
                        pda->p_mem_tick_cpu_numpfns = node_totalmem - acc_numpfns;
                        /* initial pfn for last cpu */
                        pda->p_mem_tick_cpu_startpfn = cpfn_to_pfn(node, start_cpfn);
                }
        }

        /*
         * Distribute the work each cpu has to do across time.
         * We have mem_tick_fullscan_period ticks to scan all memory
         * assigned to a cpu.
         * We determine the number of pfns we have to scan per mem_tick.
         */
        if (NODEPDA_MCD(node)->migr_system_kparms.migr_bounce_control_interval == 0) {
                NODEPDA_MCD(node)->migr_system_kparms.migr_bounce_control_interval =  base_numpfns_per_cpu / 4;
        }
        if (NODEPDA_MCD(node)->migr_system_kparms.migr_unpegging_control_interval == 0) {
                NODEPDA_MCD(node)->migr_system_kparms.migr_unpegging_control_interval = base_numpfns_per_cpu / 8;
        }
                
        for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {

                        /*
                         * For the bounce control loop
                         */
                        pda->p_mem_tick_bounce_numpfns = pda->p_mem_tick_cpu_numpfns /
                                NODEPDA_MCD(node)->migr_system_kparms.migr_bounce_control_interval;

                        /*
                         * For the unpegging loop
                         */
                        pda->p_mem_tick_unpegging_numpfns = pda->p_mem_tick_cpu_numpfns /
                                NODEPDA_MCD(node)->migr_system_kparms.migr_unpegging_control_interval;
                }
        }

        /*
         * Initialize tick sequence counter & statistics
         */

        for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        pda->p_mem_tick_seq = 0;
                        pda->p_mem_tick_maxtime = 0;
                        pda->p_mem_tick_mintime = ~((__uint64_t)0);
                        pda->p_mem_tick_avgtime = 0;
                        pda->p_mem_tick_lasttime = 0;
                }
        }                

#ifdef DEBUG
        for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        pda->p_mem_tick_bounce_accpfns = pda->p_mem_tick_cpu_numpfns;
                        pda->p_mem_tick_unpegging_accpfns = pda->p_mem_tick_cpu_numpfns;
                }
        }
#endif /* DEBUG */

	if (numnodes >= 2 &&
	    (uchar_t)numa_migr_default_mode != MIGR_DEFMODE_DISABLED) {
		for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                	lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                	pda = pdaindr[lcpuid].pda;
                	if (cpu_enabled(lcpuid)) {
				ASSERT(pda->p_mem_tick_flags == 0);
				pda->p_mem_tick_flags = mem_tick_enabled;
			}
		}
	}
                
#ifdef MEMFIT_VERBOSE 
        {
                int slot;
                printf("Distribution of mem_tick work for node [%d] (%d active cpus):\n", node, eff_cpus_on_node);
                printf("Memory Configuration for node %d:\n", node);
                printf("LOWPFN: 0x%x, HIGHPFN: 0x%x\n",
                       pfdattopfn(PFD_LOW(node)),  pfdattopfn(PFD_HIGH(node)));
                for (slot = 0; slot < node_getnumslots(node); slot++) {
                        if (slot_getsize(node, slot) > 0) {
                                printf("S[%d]: 0x%x->0x%x, ",
                                       slot,
                                       slot_getbasepfn(node, slot),
                                       slot_getbasepfn(node, slot) + slot_getsize(node, slot) -1);
                        }
                }
                printf("\n");
                for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                        lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                        pda = pdaindr[lcpuid].pda;
                        if (cpu_enabled(lcpuid)) {
                                printf("    cpu[%d]: startpfn: 0x%x, pfns: %d, pfns per tick for bouncectrl: %d, for unpegging: %d\n",
                                       cpu,
                                       pda->p_mem_tick_cpu_startpfn,
                                       pda->p_mem_tick_cpu_numpfns,
                                       pda->p_mem_tick_bounce_numpfns,
                                       pda->p_mem_tick_unpegging_numpfns);
                        }

                }
         
                printf("    Bounce control period: %d [10ms ticks], Unpegging Period: %d [10ms ticks]\n",
                       NODEPDA_MCD(node)->migr_system_kparms.migr_bounce_control_interval,
                       NODEPDA_MCD(node)->migr_system_kparms.migr_unpegging_control_interval);

        }
#endif  /* MEMFIT_VERBOSE */

}
                
int
mem_tick_init(void)
{
        cnodeid_t node;

        for (node = 0; node < numnodes; node++) {
                init_bitlock(&NODEPDA(node)->mem_tick_lock, 0x1, "memtick", node);
                mem_tick_init_node(node);
        }

        return (0);
}

/*
 * Periodic NUMA memory management operations.
 * Invoked from clock for each cpu once per cycle
 * determined by p_mem_tick_base_period and
 * p_mem_tick_counter in the per cpu PDA.
 */
void
mem_tick(cpuid_t cpuid)
{
        pfn_t bounce_pfns;
        pfn_t unpegging_pfns;
        int scan_period;
        cnodeid_t nodeid;
        pda_t* pda;
        __int64_t entry_abs_time;
        __int64_t exit_abs_time;
        __int64_t rel_time;
        

        /*
         * We have to loop over one set of pfns per tick, defined by 
         * pda->p_mem_tick_tchunk_numpfns. A full cycle is completed
         * after every mem_tick_fullscan_period ticks.
         */

        pda = pdaindr[cpuid].pda;
        nodeid = get_cpu_cnode(cpuid);

        ASSERT(pda->p_mem_tick_flags);

        /*
         * Just return if not yet initialized
         */
        if (pda->p_mem_tick_cpu_numpfns <= 0) {
                return;
        }

        /*
         * Get timestamp for timing this operation
         */
        entry_abs_time =  absolute_rtc_current_time();

        
        /*
         * pfns to be processed during this tick for bounce control
         */
        scan_period = NODEPDA_MCD(nodeid)->migr_system_kparms.migr_bounce_control_interval;
        if ((pda->p_mem_tick_seq % scan_period) == 0) {
                bounce_pfns = pda->p_mem_tick_cpu_numpfns - (pda->p_mem_tick_bounce_numpfns * (scan_period - 1));
                pda->p_mem_tick_bounce_startpfn = pda->p_mem_tick_cpu_startpfn;
        } else {
                bounce_pfns = pda->p_mem_tick_bounce_numpfns;
        }

        /*
         * pfns to be processed  during this tick for unpegging
         */
        scan_period = NODEPDA_MCD(nodeid)->migr_system_kparms.migr_unpegging_control_interval;
        if ((pda->p_mem_tick_seq % scan_period) == 0) {
                unpegging_pfns = pda->p_mem_tick_cpu_numpfns - (pda->p_mem_tick_unpegging_numpfns * (scan_period - 1));
                pda->p_mem_tick_unpegging_startpfn = pda->p_mem_tick_cpu_startpfn;
        } else {
                unpegging_pfns = pda->p_mem_tick_unpegging_numpfns;
        }
        
#ifdef DEBUG
        /*
         * Verification for bounce control loop
         */
        scan_period = NODEPDA_MCD(nodeid)->migr_system_kparms.migr_bounce_control_interval;
        if ((pda->p_mem_tick_seq % scan_period) == 0) {
                ASSERT(pda->p_mem_tick_bounce_accpfns == pda->p_mem_tick_cpu_numpfns);
                pda->p_mem_tick_bounce_accpfns = 0;
        }
        pda->p_mem_tick_bounce_accpfns += bounce_pfns;

        /*
         * Verification for the unpegging loop
         */
        scan_period = NODEPDA_MCD(nodeid)->migr_system_kparms.migr_unpegging_control_interval;
        if ((pda->p_mem_tick_seq % scan_period) == 0) {
                ASSERT(pda->p_mem_tick_unpegging_accpfns == pda->p_mem_tick_cpu_numpfns);
                pda->p_mem_tick_unpegging_accpfns = 0;
        }
        pda->p_mem_tick_unpegging_accpfns += unpegging_pfns;
        

#endif

        pda->p_mem_tick_seq++;
        
        
        /*                                                              
	 * Invoke the bounce  control module at specified intervals 
         * Bounce control is enabled when freezing is enabled or
         * when migration dampening is enabled.
	 */
	if (NODEPDA_MCD(nodeid)->migr_as_kparms.migr_freeze_enabled ||
            NODEPDA_MCD(nodeid)->migr_as_kparms.migr_dampening_enabled) {
                pda->p_mem_tick_bounce_startpfn =
                        migr_periodic_bounce_control(nodeid, pda->p_mem_tick_bounce_startpfn, bounce_pfns);
	}

	/* 
	 * Invoke page reference counter refreshing module (Unpegging)  at specified 
	 * intervals
	 */
	if (NODEPDA_MCD(nodeid)->migr_system_kparms.migr_unpegging_control_enabled) {
               pda->p_mem_tick_unpegging_startpfn =
                       migr_periodic_unpegging_control(nodeid, pda->p_mem_tick_unpegging_startpfn, unpegging_pfns);
	}
        
#ifdef NOTYET   
	/*
	 * Invoke the periodic queue control module at specified intervals
	 */
	if (NODEPDA_MCD(nodeid)->migr_system_kparms.migr_queue_control_enabled) {

		if (--(NODEPDA_MCD(nodeid)->migr_memoryd_state.queue_period_count) <  mem_tick_fullscan_period) {

			migr_periodic_queue_control(NODEPDA_MCD(nodeid)->migr_queue);

                        if (NODEPDA_MCD(nodeid)->migr_memoryd_state.queue_period_count == 0) {
                                NODEPDA_MCD(nodeid)->migr_memoryd_state.queue_period_count =
                                        NODEPDA_MCD(nodeid)->migr_system_kparms.migr_queue_control_interval;
                        }
		}
	}

	/* 
	 * Invoke the traffic control module at specified intervals
	 */
	if (NODEPDA_MCD(nodeid)->migr_system_kparms.migr_traffic_control_enabled) {
             

		if (--(NODEPDA_MCD(nodeid)->migr_memoryd_state.traffic_period_count) == 0) {

                        /*
                         *
                         * XXXXXXXXXXXXXXXXXX
                         *
                         * Use NODEPDA_MCD(nodeid)->migr_system_kparms.migr_queue_traffic_trigger_threshold
                         */
                        
			if (migr_periodic_traffic_control(nodeid) ==  LIGHTLOADED &&
                            NODEPDA_MCD(nodeid)->migr_system_kparms.migr_queue_traffic_trigger_enabled) {
                                NUMA_STAT_INCR(nodeid, migr_queue_traffic_trigger_count);
				migr_queue_cluster_migr(nodeid); 
			}

                        NODEPDA_MCD(nodeid)->migr_memoryd_state.traffic_period_count =
                                NODEPDA_MCD(nodeid)->migr_system_kparms.migr_traffic_control_interval;
		}
            
	}
#endif    	        


        /*
         * Get exit timestamp and store max time observed in pda
         */
        
        exit_abs_time =  absolute_rtc_current_time();

        if (exit_abs_time > entry_abs_time) {
                uint n;
                rel_time = exit_abs_time  - entry_abs_time;
                pda->p_mem_tick_lasttime = rel_time;
                if (rel_time > pda->p_mem_tick_maxtime) {
                        pda->p_mem_tick_maxtime = rel_time;
                }
                if (rel_time < pda->p_mem_tick_mintime) {
                        pda->p_mem_tick_mintime = rel_time;
                }
                if ((n = pda->p_mem_tick_seq) > 0) {
                        pda->p_mem_tick_avgtime = (((n - 1) * pda->p_mem_tick_avgtime) + rel_time) / n;
                } else {
                        pda->p_mem_tick_avgtime = 0;
                }
        }

        /*
         * Check for a request for quiescing -- used to change the periods
         */
        if (pda->p_mem_tick_quiesce == 1) {
                pda->p_mem_tick_flags = 0;
                pda->p_mem_tick_quiesce++;
        }
}

int
mem_tick_quiesce_node(cnodeid_t node)
{
        int cpu;
        int eff_cpus_on_node;
        int max_cpus_on_node;        
        cpuid_t lcpuid;
        pda_t* pda;
        int s;

        /*
         * Get max cpus on node
         */
        max_cpus_on_node = CNODE_NUM_CPUS(node);
                
        /*
         * Determine effective number of cpus available on this node
         */
        eff_cpus_on_node = 0;
        for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                if (cpu_enabled(lcpuid)) {
                        eff_cpus_on_node++;
                }
        }

        if (eff_cpus_on_node == 0) {
                /*
                 * No enabled cpus on this node;
                 * For now, we don't do any mem_ticks on these nodes
                 */
                return (0);
        }

        /* loop through all cpus in node verifying state */
        s = mutex_bitlock(&NODEPDA(node)->mem_tick_lock, 0x1);
        for (cpu = 0; cpu < max_cpus_on_node ; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        if (!pda->p_mem_tick_flags) {
                                mutex_bitunlock(&NODEPDA(node)->mem_tick_lock, 0x1, s);
                                return (1);
                        }
                        if (pda->p_mem_tick_quiesce > 0) {
                                mutex_bitunlock(&NODEPDA(node)->mem_tick_lock, 0x1, s);
                                return (1);
                        }
                }
        }

        /* state is ok -- proceed to set quiescing flags */
        for (cpu = 0; cpu < max_cpus_on_node ; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        pda->p_mem_tick_quiesce = 1;    
                }
        }        
        mutex_bitunlock(&NODEPDA(node)->mem_tick_lock, 0x1, s);
        
        
        /* loop waiting for all cpus in node to quiesce */
        for (cpu = 0; cpu < max_cpus_on_node ; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        while ((* (volatile ushort *)&pda->p_mem_tick_quiesce) <= 1) {
                                user_resched(RESCHED_Y);
                        }
                        
                }
        }        
        
        return (0);
}

int
mem_tick_reactivate_node(cnodeid_t node)
{
        int cpu;
        int eff_cpus_on_node;
        int max_cpus_on_node;        
        cpuid_t lcpuid;
        pda_t* pda;
        int s;

        /*
         * Get max cpus on node
         */
        max_cpus_on_node = CNODE_NUM_CPUS(node);
                
        /*
         * Determine effective number of cpus available on this node
         */
        eff_cpus_on_node = 0;
        for (cpu = 0; cpu < max_cpus_on_node; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                if (cpu_enabled(lcpuid)) {
                        eff_cpus_on_node++;
                }
        }

        if (eff_cpus_on_node == 0) {
                /*
                 * No enabled cpus on this node;
                 * For now, we don't do any mem_ticks on these nodes
                 */
                return (0);
        }

        /* loop through all cpus in node verifying state */
        s = mutex_bitlock(&NODEPDA(node)->mem_tick_lock, 0x1);
        for (cpu = 0; cpu < max_cpus_on_node ; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        if (!pda->p_mem_tick_flags) {
                                mutex_bitunlock(&NODEPDA(node)->mem_tick_lock, 0x1, s);
                                return (1);
                        }
                        if (pda->p_mem_tick_quiesce <= 1) {
                                mutex_bitunlock(&NODEPDA(node)->mem_tick_lock, 0x1, s);
                                return (1);
                        }
                }
        }
        
        /* state is ok -- proceed to reactivate node */
        for (cpu = 0; cpu < max_cpus_on_node ; cpu++) {
                lcpuid = CNODE_TO_CPU_BASE(node) + cpu;
                pda = pdaindr[lcpuid].pda;
                if (cpu_enabled(lcpuid)) {
                        pda->p_mem_tick_quiesce = 0;
                        pda->p_mem_tick_flags = mem_tick_enabled; 
                }
        }        
        mutex_bitunlock(&NODEPDA(node)->mem_tick_lock, 0x1, s);

        return (0);
}
