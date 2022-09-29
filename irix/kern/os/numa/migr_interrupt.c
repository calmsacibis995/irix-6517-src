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
 * Serve the SN0 hub migration interrupt
 */

#include <sys/types.h>
#include <sys/cpumask.h>
#include <sys/atomic_ops.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/systm.h>

#ifdef SN
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>
#include <sys/SN/router.h>
#include <sys/SN/intr.h>
#include "mem_profiler.h"
#endif /* SN */

#include "pfms.h"
#include "numa_hw.h"
#include "migr_control.h"
#include "migr_manager.h"
#include "migr_states.h"
#include "debug_levels.h"
#include "numa_idbg.h"
#include "numa_stats.h"
#include "migr_refcnt.h"


#ifdef SN0

#ifdef SN0_INTR_BROKEN
#error SN0_INTR_BROKEN not supported anymore !!!!!!!!!!!
#endif /* SN0_INTR_BROKEN */

void
migr_intr_prologue_handler(void)
{
        __uint64_t candidate;
        cnodeid_t node = cnodeid();

        candidate = MIGR_CANDIDATE_GET(node);

        MIGR_THRESHOLD_DIFF_DISABLE(node);
        MIGR_THRESHOLD_ABS_DISABLE(node);
        
        if (!MIGR_CANDIDATE_VALID(candidate)) {
                candidate = 0;
                NUMA_DSTAT_INCR(node, refcnt_invalid_candidate_intrnumber);
        }
        
        nodepda->migr_candidate = candidate;
        
        NUMA_STAT_INCR(node, refcnt_interrupt_number);
}

void
migr_intr_handler(void)
{
	cnodeid_t home_nodeid;
	__uint64_t migration_candidate;
        __uint64_t new_raw_candidate;
	pfn_t swpfn;
        pfn_t relpfn;
        pfn_t new_relpfn;
	cnodeid_t dest_nodeid;
        pfms_t pfms;
        pfmsv_t pfmsv;        

	home_nodeid = cnodeid();
        migration_candidate = nodepda->migr_candidate;

        while (migration_candidate != 0) {

                NUMA_DSTAT_INCR(home_nodeid, refcnt_intrhandler_numloops);
                /* 
                 * Extract hwpfn and convert it to swpfn 
                 * Since only bits 31..14 of the physical address is stored 
                 * in the migration candidate register, we need the nasid to
                 * make a pfn.
                 * Because of a hub bug, uh.... I mean a hub `feature'
                 * we get a 16KB page_frame_number, instead of the
                 * expected `hardware' 4KB page_frame_number.
                 */
                relpfn = MIGR_CANDIDATE_RELATIVE_SWPFN(migration_candidate);
                swpfn = mkpfn(COMPACT_TO_NASID_NODEID(home_nodeid), relpfn);
                ASSERT(swpfn && (swpfn <= pfdattopfn(PFD_HIGH(home_nodeid))));

                pfms = PFN_TO_PFMS(swpfn);
                pfmsv = PFMS_GETV(pfms);

                switch (MIGR_CANDIDATE_TYPE(migration_candidate)) {
                case MIGR_INTR_TYPE_ABS:
                        if (PFMSV_IS_MIGRREFCNT_ENABLED(pfmsv)) {

                                if (!(!PFMSV_IS_MIGR_ENABLED(pfmsv) || PFMSV_IS_FROZEN(pfmsv))) {

                                        NUMA_DSTAT_INCR(home_nodeid, refcnt_intrfail_absmigrnotfrz);
                                        migr_refcnt_update_counters(home_nodeid,
                                                                    swpfn,
                                                                    pfmsv,
                                                                    MD_PROT_MIGMD_IREL);
                                } else {

                                        NUMA_DSTAT_INCR(home_nodeid, refcnt_introk_absolute);
                                        migr_refcnt_update_counters(home_nodeid,
                                                                    swpfn,
                                                                    pfmsv,
                                                                    MD_PROT_MIGMD_IABS);
                                }
                        } else {
                                migr_refcnt_restart(swpfn, pfmsv, 0);
                        }

                        break;

                case MIGR_INTR_TYPE_REL:

                        if (!PFMS_IS_MIGR_ENABLED(PFN_TO_PFMS(swpfn))) {
                                NUMA_DSTAT_INCR(home_nodeid, refcnt_intrfail_relnotmigr);
                        } else {
                
                                /*
                                 * Extract destination node id and hwpfn
                                 */
                                dest_nodeid = MIGR_CANDIDATE_INITR(migration_candidate);

                            
                                ASSERT((dest_nodeid >= 0) &&
                                       (dest_nodeid < numnodes));

                                ASSERT(dest_nodeid != home_nodeid);

                                NUMA_DSTAT_INCR(home_nodeid, refcnt_introk_relative);
                                migr_manager(home_nodeid, dest_nodeid, swpfn);
                        }

                        break;

                default:
                        cmn_err(CE_PANIC, "migr_intr_handler: Invalid intr type");

                }

                migration_candidate = 0;
                new_raw_candidate = MIGR_CANDIDATE_GET(home_nodeid);
                if (MIGR_CANDIDATE_VALID(new_raw_candidate)) {
                        new_relpfn = MIGR_CANDIDATE_RELATIVE_SWPFN(new_raw_candidate);
                        if (new_relpfn != relpfn) {
                                migration_candidate = new_raw_candidate;
                        }
                }
        }

        MIGR_THRESHOLD_ABS_ENABLE(home_nodeid);
        MIGR_THRESHOLD_DIFF_ENABLE(home_nodeid);
}


#ifdef DEBUG_STUFF_XXXX



#ifdef DEBUG_MIGRINTR
int showcounts = 0;        
#endif
#ifdef DEBUG_MIGRINTR
{
        int i;
        int j;
        pfn_t hwpfn;
        extern void idbg_refcnt_node_state_print(cnodeid_t node);


                        
        printf("########### Migr not enabled #############\n");
        idbg_refcnt_node_state_print(home_nodeid);
        printf("swpfn 0x%x, pfmsv 0x%016llx\n", swpfn, pfmsv);

        for (i = 0, hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);
             i < NUM_OF_HW_PAGES_PER_SW_PAGE();
             i++,  hwpfn++) {
                
                /*
                 * For every counter in the set associated with hwpfn
                 */
                for (j = 0; j < numnodes; j++) {
                        uint count;
                        /*
                         * Get the current value
                         */
                        count = MIGR_COUNT_GET(MIGR_COUNT_REG_GET(hwpfn, j), j);

                        printf("swpfn 0x%08x hwpfn 0x%08x node %02d count %06d\n",
                               swpfn, hwpfn, j, count);
                               
                }

        }
                        
}

{
#ifdef DEBUG_MIGRINTR
        if (showcounts) {

                int i;
                int j;
                pfn_t hwpfn;
                int home_count;
                int maxdiff = 0;
                        
                for (i = 0, hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);
                     i < NUM_OF_HW_PAGES_PER_SW_PAGE();
                     i++,  hwpfn++) {

                        home_count = MIGR_COUNT_GET(MIGR_COUNT_REG_GET(hwpfn, home_nodeid),
                                                    home_nodeid);
                        /*
                         * For every counter in the set associated with hwpfn
                         */
                        for (j = 0; j < numnodes; j++) {
                                int count;
                                int localdiff;
                                /*
                                 * Get the current value
                                 */
                                count = MIGR_COUNT_GET(MIGR_COUNT_REG_GET(hwpfn, j), j);

                                localdiff = count - home_count;
                                if (localdiff > maxdiff) {
                                        maxdiff = localdiff;
                                }
                               
                        }

                }

                maxdiff = MIGR_DIFF_THRESHOLD_EFF_TO_REL(home_nodeid, maxdiff);
                printf("MIGRATION VERF: swpfn 0x%x, diff: %d, thres: %d\n",
                       swpfn, maxdiff, PFMSV_MGRTHRESHOLD_GET(pfmsv));
                        
        }
#endif
}
#endif


#endif /* DEBUG_STUFF_XXXX */


#endif /* SN0 */






