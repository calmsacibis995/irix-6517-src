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
 * NUMA page migration manager
 */


#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/cpumask.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/pda.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/lpage.h>
#include <ksys/migr.h>
#include "pfms.h"
#include "migr_control.h"
#include "migr_traffic.h"
#include "migr_bounce.h"
#include "migr_queue.h"
#include "migr_manager.h"
#include "migr_engine.h"
#include "migr_states.h"
#include "debug_levels.h"
#include "numa_stats.h"
#include "numa_hw.h"
#include "migr_refcnt.h"

extern int node_distance(cnodeid_t n0, cnodeid_t n1);


/* 
 * Migration manager module to decide if a page gets migrated
 */

void 
migr_manager(cnodeid_t source_nodeid,
             cnodeid_t dest_nodeid,
             pfn_t     swpfn) 

{
        pfms_t pfms;
        pfmsv_t pfmsv;
        int pfms_spl;
        pfn_t dest_pfn;
        int ret;

        /*
         * We change the state of the page into mapped 
         */

        pfms = PFN_TO_PFMS(swpfn);
        PFMS_LOCK(pfms, pfms_spl);
        pfmsv = PFMS_GETV(pfms);

        if (!PFMSV_STATE_IS_MAPPED(pfmsv)) {
                NUMA_STAT_INCR(source_nodeid, migr_interrupt_failstate);
                PFMS_UNLOCK(pfms, pfms_spl);
                goto out;
        }

        if (!PFMSV_IS_MIGR_ENABLED(pfmsv)) {
                NUMA_STAT_INCR(source_nodeid, migr_interrupt_failenabled);
                PFMS_UNLOCK(pfms, pfms_spl);
                goto out;
        }

        if (PFMSV_IS_FROZEN(pfmsv)) {
                NUMA_STAT_INCR(source_nodeid, migr_interrupt_failfrozen);
                PFMS_UNLOCK(pfms, pfms_spl);
                goto out;
        }       
        
        PFMS_STATE_MIGRED_SET(pfms);
        PFMS_UNLOCK(pfms, pfms_spl);

        migr_refcnt_update_counters(source_nodeid,
                                    swpfn,
                                    pfmsv,
                                    MD_PROT_MIGMD_PREL);        
        /*
         * Locking strategy.
         * We use the current value of the pfms (pfmsv)
         * to take most of the migration control decisions.
         * We are purposely disregarding state changes while
         * we use the temporary local value. Later, before
         * doing the actual migration, or enqueuing the page
         * to be migrated later, we synchronize our state,
         * and abort if we realize the page has been freed.
         */

        /*
         * Memory pressure control
         */
        if (NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_memory_low_enabled ) {
                if (NODE_FREEMEM_REL(dest_nodeid) <
                    NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_memory_low_threshold) {

                        NUMA_STAT_INCR(source_nodeid, migr_auto_number_fail);
                        NUMA_STAT_INCR(source_nodeid, migr_pressure_frozen_pages);
                        /*
                         * We need to freeze the page
                         */
                        PFMS_FROZEN_SET(pfms);
                        PFMS_MGRCNT_PUT(pfms, 0);
                        migr_restart(swpfn, pfms);
                        NUMA_STAT_INCR(source_nodeid, migr_bouncecontrol_frozen_pages);
                        goto out;
                }
        }                        

	/*
	 * Distance Control
	 */
	if (node_distance(source_nodeid, dest_nodeid) < NODEPDA_MCD(source_nodeid)->migr_as_kparms.migr_min_distance) {
                NUMA_STAT_INCR(source_nodeid, migr_auto_number_fail);
                NUMA_STAT_INCR(source_nodeid, migr_distance_frozen_pages);
                /*
                 * We need to freeze the page
                 */
                PFMS_FROZEN_SET(pfms);
                PFMS_MGRCNT_PUT(pfms, 0);
                migr_restart(swpfn, pfms);
                NUMA_STAT_INCR(source_nodeid, migr_bouncecontrol_frozen_pages);
                goto out;
                
	}

        /*
         * Cost Control
         * Avod migrating locked pages, or pages with too many references
         */
        if (migr_fastcheck_migratable(swpfn) != 0) {
                NUMA_STAT_INCR(source_nodeid, migr_auto_number_fail);
                NUMA_STAT_INCR(source_nodeid, migr_cost_frozen_pages);
                /*
                 * We need to freeze the page
                 */
                PFMS_FROZEN_SET(pfms);
                PFMS_MGRCNT_PUT(pfms, 0);
                migr_restart(swpfn, pfms);
                NUMA_STAT_INCR(source_nodeid, migr_bouncecontrol_frozen_pages);
                goto out;
                
	}
                  
         

#ifdef NOTYET        
        /*
         * Traffic control if enabled
         * This is a system parameter (we check the nodepda)
         */
	if (NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_traffic_control_enabled ) {
                if (migr_manager_traffic_control(source_nodeid, dest_nodeid) == OVERLOADED) {
 
                        /*
                         * Insert in queue if migr_enqonfail enabled
                         */

                        if (NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_queue_control_enabled &&
                            PFMSV_IS_ENQONFAIL(pfmsv)) {

                                migr_queue_insert(NODEPDA_MCD(source_nodeid)->migr_queue,
                                                  pfms,
                                                  swpfn,
                                                  dest_nodeid);
                                /*
                                 * migr_queue_insert has moved the pfms state into QUEUED
                                 */
                                goto out;                                
                                
                        }

                        NUMA_STAT_INCR(source_nodeid, migr_auto_number_fail);
                        /*
                         * We need to move the state back into MAPPED
                         */
                        goto fail;                        

                }
        }
#endif        

        /*
         * Bounce control if enabled
         * This is an address space parameter (we check the pfms for this page)
         * We increment the migration count in migr_manager_bounce_control.
         * This count is not really goin to represent the exact number of
         * times we've migrated this page, but the number of times we
         * intended to migrate it.
         */

        if (PFMSV_IS_FREEZE_ENABLED(pfmsv) || PFMSV_IS_MIGRDAMPEN_ENABLED(pfmsv)) {
                migr_page_state_t action =  migr_manager_bounce_control(pfms, swpfn, source_nodeid);
                if (action != MIGRATABLE) {
                        /* 
                         * The page is detected bouncing and is frozen =>
                         * we abort the migration.
                         * If frozen, the bounce controller has taken care
                         * of the state transitions.
                         * Or the page is still within the dampening period,
                         * in which case we just count this migr request, and
                         * abort the actual migration operation. When the page
                         * being dampened, the bounce controller takes care
                         * of the state transition.
                         * 
                         */
                        NUMA_STAT_INCR(source_nodeid, migr_auto_number_fail);
                        goto out;
                }
        }

        /*
         * Choose migration mechanism
         * If the migr mechanism currently active is
         * a) enqueuing, then we just enqueue and return,
         * b) immediate, the we call the migration engine.
         *    If the immediate migration fails, and enqueuing
         *    on failure is enabled, we enqueue.
         */

        if (NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_queue_control_enabled &&
                NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_auto_migr_mech == MIGR_MECH_ENQUEUE) {

                migr_queue_insert(NODEPDA_MCD(source_nodeid)->migr_queue,
                                  pfms,
                                  swpfn, 
                                  dest_nodeid);
                /*
                 * migr_queue_insert has moved the pfms state into QUEUED
                 */
                goto out;          
        }


	if (((ret = migr_migrate_page(swpfn, dest_nodeid, &dest_pfn)) != 0)) { 
		
		/*
		 * Not successful -
                 * The pfms state has not been updated. We need to
                 * take care of it.
		 */

		if (NODEPDA_MCD(source_nodeid)->migr_system_kparms.migr_queue_control_enabled &&
                    PFMSV_IS_ENQONFAIL(pfmsv) &&
		    ((ret != MIGRERR_ZEROLINKS) || (ret != MIGRERR_NOMEM))) {

			/* 
			 * queue the page - the method takes care of the pfms state
			 */

			migr_queue_insert(NODEPDA_MCD(source_nodeid)->migr_queue, pfms, swpfn, dest_nodeid);
                        goto out;

		}

                /*
                 * Enqueueing is not enabled -
                 * Just give up.
                 */
                ASSERT(dest_pfn == swpfn);
                ASSERT(pfms == PFN_TO_PFMS(swpfn));
                NUMA_STAT_INCR(source_nodeid, migr_auto_number_fail);

                /*
                 * We need to move the state back into MAPPED
                 */
                goto fail;

	}

        /*
         * On success 
         */
        ASSERT(dest_pfn != swpfn);
        NUMA_STAT_INCR(source_nodeid, migr_auto_number_out);
        NUMA_STAT_INCR(dest_nodeid, migr_auto_number_in);
        migr_restart_clear(dest_pfn, PFN_TO_PFMS(dest_pfn));
        goto out;

        
  fail:
        migr_restart(swpfn, pfms);

  out:
        return;
}










