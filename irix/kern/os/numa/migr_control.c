/*****************************************************************************
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
 ****************************************************************************/

/*
 * Migration Control Initialization
 */

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
#include <sys/lpage.h>
#include <sys/mmci.h>
#include <ksys/migr.h>
#include "pmo_base.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pfms.h"
#include "numa_hw.h"
#include "migr_control.h"
#include "migr_queue.h"
#include "migr_engine.h"
#include "debug_levels.h"
#include "memsched.h"
#include "migr_refcnt.h"


#if defined(SN0)
/* Defined in klgraph.c */
extern char klhwg_premium_std_dimm_mix_check(void);

#include <sys/SN/SN0/sn0.h>
#endif

/*
 * Initialize the migration control data in the nodepda
 * Called by: nodepda_numa_init()
 */
void 
migr_control_init(cnodeid_t node, caddr_t* nextbyte)
{
        int bte_poison_ok;

	/*
         * Initialize the migration control data structure
         */
	NODEPDA_MCD(node) = (migr_control_data_t *)low_mem_alloc(sizeof(migr_control_data_t), nextbyte, "XXX");
	ASSERT(NODEPDA_MCD(node));

#ifdef SN0
	/*
	 * Get the type of DIMM used for directory 
	 * XXX We need a hub private data cached in nodepda
	 */
	/* Get the directory dimm flavor of the whole system.
	 * If there is a mixture of premium & standard dimms 
	 * then only standard dimms will be used if possible.
	 */
	NODEPDA(node)->membank_flavor =  klhwg_premium_std_dimm_mix_check();
	NODEPDA_MCD(node)->hub_dir_type = NODE_DIRTYPE_GET(node);
#endif /* SN0 */

        /*
         * Initialize system default for AS migr parameters
         */
        switch (numa_migr_default_mode) {
        case MIGR_DEFMODE_DISABLED:
                NODEPDA_MCD(node)->migr_as_kparms.migr_base_mode = MIGRATION_MODE_DIS;
                break;
        case MIGR_DEFMODE_ENABLED:
                NODEPDA_MCD(node)->migr_as_kparms.migr_base_mode = MIGRATION_MODE_EN;
                break;
        case MIGR_DEFMODE_NORMON:
                NODEPDA_MCD(node)->migr_as_kparms.migr_base_mode = MIGRATION_MODE_ON;
                break;
        case MIGR_DEFMODE_NORMOFF:
                 NODEPDA_MCD(node)->migr_as_kparms.migr_base_mode = MIGRATION_MODE_OFF;
                 break;
        case MIGR_DEFMODE_LIMITED:
                /*
                 * Enable migration only for machines with maxradius greater or equal than min_maxradius
                 */
                if (physmem_maxradius() >= numa_migr_min_maxradius) {
                        NODEPDA_MCD(node)->migr_as_kparms.migr_base_mode = MIGRATION_MODE_ON;
                } else {
                        NODEPDA_MCD(node)->migr_as_kparms.migr_base_mode = MIGRATION_MODE_OFF;
                }
                break;
        default:
                cmn_err(CE_WARN, "[migr_control_init] Invalid Default Migration Mode, Disabling migration...");
                NODEPDA_MCD(node)->migr_as_kparms.migr_base_mode = MIGRATION_MODE_DIS;
        }

        switch (numa_refcnt_default_mode) {
        case REFCNT_DEFMODE_DISABLED:
                NODEPDA_MCD(node)->migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_DIS;
                break;
        case REFCNT_DEFMODE_ENABLED:
                NODEPDA_MCD(node)->migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_EN;
                break;
        case REFCNT_DEFMODE_NORMON:
                NODEPDA_MCD(node)->migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_ON;
                break;
        case REFCNT_DEFMODE_NORMOFF:
                 NODEPDA_MCD(node)->migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_OFF;
                 break;
        default:
                cmn_err(CE_WARN, "[migr_control_init] Invalid Default Refcnt Mode, Disabling refcnt...");
                NODEPDA_MCD(node)->migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_DIS;
        }

        switch (numa_migr_threshold_reference) {
        case MIGR_THRESHREF_STANDARD:
                NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference = MIGR_COUNTER_STANDARD_MAX;
                break;
        case MIGR_THRESHREF_PREMIUM:
                if (NODE_DIRTYPE_GET(node) != DIRTYPE_PREMIUM) {
                        cmn_err(CE_WARN, "Node [%d]: Invalid Threshold Reference - Not all DIMMS are premium", node);
                        cmn_err(CE_WARN, "Node [%d]: Setting Threshold Reference to STANDARD (%d)", node, MIGR_COUNTER_STANDARD_MAX);
                        NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference = MIGR_COUNTER_STANDARD_MAX;
                } else {
                        NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference = MIGR_COUNTER_PREMIUM_MAX;
                }
                break;
        default:
                cmn_err(CE_WARN, "Node [%d]: Invalid Threshold Reference - Invalid code {%d}", node, numa_migr_threshold_reference);
                cmn_err(CE_WARN, "Node [%d]: Setting Threshold Reference to STANDARD (%d)", node, MIGR_COUNTER_STANDARD_MAX); 
                NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference = MIGR_COUNTER_STANDARD_MAX;
        }
                        

        if (numa_migr_default_threshold < 0 || numa_migr_default_threshold > 100) {
                cmn_err(CE_WARN, "Node [%d]: Invalid Default Migration Threshold (%d)", node, numa_migr_default_threshold);
                NODEPDA_MCD(node)->migr_as_kparms.migr_base_threshold = NUMA_MIGR_BASE_RELTHRESHOLD_DEF;
                cmn_err(CE_WARN, "Node [%d]: Migration Threshold set to (%d%%) over reference (%d), eff (%d)",
                        node,
                        NODEPDA_MCD(node)->migr_as_kparms.migr_base_threshold,
                        NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference,
                        MIGR_COUNTER_REL_TO_EFF(node, NODEPDA_MCD(node)->migr_as_kparms.migr_base_threshold));
        } else if (MIGR_COUNTER_REL_TO_EFF(node, numa_migr_default_threshold) < NUMA_MIGR_BASE_EFFTHRESHOLD_MIN) {
                cmn_err(CE_WARN, "Node [%d]: Invalid Default Migration Threshold of (%d%%) over reference (%d), eff (%d)",
                        node,
                        numa_migr_default_threshold,
                        NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference,
                        MIGR_COUNTER_REL_TO_EFF(node, numa_migr_default_threshold));
                NODEPDA_MCD(node)->migr_as_kparms.migr_base_threshold =  MIGR_COUNTER_EFF_TO_REL(node, NUMA_MIGR_BASE_EFFTHRESHOLD_MIN);
                cmn_err(CE_WARN, "Node [%d]: Resetting Migration Threshold to (%d%%) over reference (%d), eff (%d)",
                        node,
                        NODEPDA_MCD(node)->migr_as_kparms.migr_base_threshold,
                        NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference,
                        MIGR_COUNTER_REL_TO_EFF(node, NODEPDA_MCD(node)->migr_as_kparms.migr_base_threshold));
        } else {
                NODEPDA_MCD(node)->migr_as_kparms.migr_base_threshold =  numa_migr_default_threshold;
        }

        if (numa_refcnt_overflow_threshold < 0 || numa_refcnt_overflow_threshold > 100) {
                cmn_err(CE_WARN, "Node [%d]: Invalid Refcnt Overflow Threshold (%d)",
                        node,
                        numa_refcnt_overflow_threshold);
                NODEPDA_MCD(node)->migr_system_kparms.migr_effthreshold_refcnt_overflow = NUMA_REFCNT_BASE_EFFTHRESHOLD_DEF;
                cmn_err(CE_WARN, "Node [%d]: Setting Refcnt Overflow Threshold to (%d%%), over ref (%d), eff (%d)",
                        node,
                        MIGR_COUNTER_EFF_TO_REL(node, NODEPDA_MCD(node)->migr_system_kparms.migr_effthreshold_refcnt_overflow),
                        NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference,
                        NODEPDA_MCD(node)->migr_system_kparms.migr_effthreshold_refcnt_overflow);
        } else if (MIGR_COUNTER_REL_TO_EFF(node, numa_refcnt_overflow_threshold) < NUMA_REFCNT_BASE_EFFTHRESHOLD_MIN) {
                cmn_err(CE_WARN, "Node [%d]: Invalid Refcnt Overflow Threshold of (%d%%) over ref (%d), eff (%d)",
                        node,
                        numa_refcnt_overflow_threshold,
                        NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference,
                        MIGR_COUNTER_REL_TO_EFF(node, numa_refcnt_overflow_threshold));
                NODEPDA_MCD(node)->migr_system_kparms.migr_effthreshold_refcnt_overflow = NUMA_REFCNT_BASE_EFFTHRESHOLD_MIN;
                cmn_err(CE_WARN, "Node [%d]: Setting Refcnt Overflow Threshold to (%d%%), over ref (%d), eff (%d)",
                        node,
                        MIGR_COUNTER_EFF_TO_REL(node, NODEPDA_MCD(node)->migr_system_kparms.migr_effthreshold_refcnt_overflow),
                        NODEPDA_MCD(node)->migr_system_kparms.migr_threshold_reference,
                        NODEPDA_MCD(node)->migr_system_kparms.migr_effthreshold_refcnt_overflow);
        } else {
                NODEPDA_MCD(node)->migr_system_kparms.migr_effthreshold_refcnt_overflow =
                        MIGR_COUNTER_REL_TO_EFF(node, numa_refcnt_overflow_threshold);
        }
        

        NODEPDA_MCD(node)->migr_as_kparms.migr_min_distance = (uchar_t)numa_migr_min_distance;
        NODEPDA_MCD(node)->migr_as_kparms.migr_min_maxradius = (uchar_t)numa_migr_min_maxradius;


        NODEPDA_MCD(node)->migr_as_kparms.migr_freeze_enabled = (uchar_t)numa_migr_freeze_enabled;
        NODEPDA_MCD(node)->migr_as_kparms.migr_freeze_threshold = (uchar_t)numa_migr_freeze_threshold;
        NODEPDA_MCD(node)->migr_as_kparms.migr_melt_enabled = (uchar_t)numa_migr_melt_enabled;
        NODEPDA_MCD(node)->migr_as_kparms.migr_melt_threshold = (uchar_t)numa_migr_melt_threshold;
        NODEPDA_MCD(node)->migr_as_kparms.migr_enqonfail_enabled = (uchar_t)numa_migr_enqonfail_enabled;
        NODEPDA_MCD(node)->migr_as_kparms.migr_dampening_enabled = (uchar_t)numa_migr_dampening_enabled;
        NODEPDA_MCD(node)->migr_as_kparms.migr_dampening_factor = (uchar_t)numa_migr_dampening_factor;


        if (numa_migr_auto_migr_mech == MIGR_MECH_ENQUEUE ||
            numa_migr_user_migr_mech == MIGR_MECH_ENQUEUE ||
            numa_migr_coaldmigr_mech == MIGR_MECH_ENQUEUE ||
            numa_migr_enqonfail_enabled) {
                if (!numa_migr_queue_control_enabled) {
                        cmn_err(CE_WARN,
                                "INCONSISTENT NUMA PARAMETERS: numa_migr_queue_control_enabled must be on");
                        cmn_err(CE_WARN,
                                "when any migration client has its migr mechanism set to MIGR_MECH_ENQUEUE\n\n");
                }
        }

                
        /*
         * Inititalize system default for migr system parameters
         */
        NODEPDA_MCD(node)->migr_system_kparms.memoryd_enabled = memoryd_enabled;
        NODEPDA_MCD(node)->migr_system_kparms.migr_traffic_control_enabled = numa_migr_traffic_control_enabled;
        NODEPDA_MCD(node)->migr_system_kparms.migr_unpegging_control_enabled = numa_migr_unpegging_control_enabled;
        NODEPDA_MCD(node)->migr_system_kparms.migr_auto_migr_mech = numa_migr_auto_migr_mech;
        NODEPDA_MCD(node)->migr_system_kparms.migr_user_migr_mech = numa_migr_user_migr_mech;
        NODEPDA_MCD(node)->migr_system_kparms.migr_coaldmigr_mech = numa_migr_coaldmigr_mech;

        NODEPDA_MCD(node)->migr_system_kparms.migr_traffic_control_interval = numa_migr_traffic_control_interval;
        NODEPDA_MCD(node)->migr_system_kparms.migr_traffic_control_threshold = numa_migr_traffic_control_threshold;
        NODEPDA_MCD(node)->migr_system_kparms.migr_bounce_control_interval = numa_migr_bounce_control_interval;
        NODEPDA_MCD(node)->migr_system_kparms.migr_unpegging_control_interval = numa_migr_unpegging_control_interval;
        NODEPDA_MCD(node)->migr_system_kparms.migr_unpegging_control_threshold = numa_migr_unpegging_control_threshold;
        
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_control_enabled = numa_migr_queue_control_enabled;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_traffic_trigger_enabled = numa_migr_queue_traffic_trigger_enabled;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_capacity_trigger_enabled = numa_migr_queue_capacity_trigger_enabled;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_time_trigger_enabled = numa_migr_queue_time_trigger_enabled;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_maxlen = numa_migr_queue_maxlen;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_control_interval = numa_migr_queue_control_interval;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_control_mlimit = numa_migr_queue_control_mlimit;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_traffic_trigger_threshold = numa_migr_queue_traffic_trigger_threshold;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_capacity_trigger_threshold = numa_migr_queue_capacity_trigger_threshold;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_time_trigger_interval = numa_migr_queue_time_trigger_interval;
        NODEPDA_MCD(node)->migr_system_kparms.migr_queue_control_max_page_age = numa_migr_queue_control_max_page_age;
        NODEPDA_MCD(node)->migr_system_kparms.migr_memory_low_enabled = numa_migr_memory_low_enabled;
        NODEPDA_MCD(node)->migr_system_kparms.migr_memory_low_threshold = numa_migr_memory_low_threshold;
        
        /*
         * Initialize memoryd state (daemon period counters)
         */
	NODEPDA_MCD(node)->migr_memoryd_state.traffic_period_count = numa_migr_traffic_control_interval;
	NODEPDA_MCD(node)->migr_memoryd_state.queue_period_count = numa_migr_queue_control_interval;


	
        /*
         * Allocation of memory for the Migration Queue
         */
	NODEPDA_MCD(node)->migr_queue = migr_queue_alloc(numa_migr_queue_maxlen, node, nextbyte);

        /*
         * Allocation of memory for the PFMS array.
         */
        
	NODEPDA_MCD(node)->pfms = nodepda_pfms_alloc(node, nextbyte);


	NODEPDA_MCD(node)->local_migr_page_list =
                low_mem_alloc(NODEPDA_MCD(node)->migr_system_kparms.migr_queue_maxlen * sizeof(migr_page_list_t), nextbyte, "YYY");

	/* 
	 * A spinlock for accessing NUMA related hub hardware registers:
	 * 1. migration threshold register 
	 * 2. reference counters 
	 */        
	spinlock_init(&(NODEPDA_MCD(node)->hub_lock), "migr_hub_lock");

#if defined(SN0_USE_BTE) && defined(SN0_USE_POISON_BITS)
        bte_poison_ok = hub_bte_poison_ok();
#else /* !SN0_USE_BTE || !SN0_USE_POISON_BITS */
        bte_poison_ok = 0;
#endif /* !SN0_USE_BTE || !SN0_USE_POISON_BITS */
        
        NODEPDA_MCD(node)->bte_poison_ok = bte_poison_ok;        

        /*
         * If we can't use the poison bits, we force
         * numa_migr_vehicle to be the TLBsync.
         * Otherwise, we follow the mtune/numa setting
         */
        if (!bte_poison_ok) {
                numa_migr_vehicle = MIGR_VEHICLE_IS_TLBSYNC;
        }

        NODEPDA_MCD(node)->migr_system_kparms.migr_vehicle  = numa_migr_vehicle;

}


