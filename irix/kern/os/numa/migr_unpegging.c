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

#ifdef SN0

/*
 * NUMA periodic page reference counter unpegging control
 */

#include <sys/bsd_types.h>
#include <sys/cpumask.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include "pfms.h"
#include "migr_control.h"
#include "numa_hw.h"
#include "migr_states.h"
#include "debug_levels.h"
#include "numa_stats.h"
#include "migr_refcnt.h"

/*
 * Counter saturation control
 * -- go through all the protection entries on the node and refresh 
 *    reference counters if necessary 
 */

pfn_t
migr_periodic_unpegging_control(cnodeid_t nodeid, pfn_t startpfn, pfn_t numpfns)
{
	pfd_t *pfd;
	int npgs;
	pfn_t swpfn;
        pfn_t hwpfn;
        int i;
        uint pegging_threshold;
        int count;
        pfms_t pfms;
        pfmsv_t pfmsv;
        pfn_t pfncounter;
        int s;
	
        
        pegging_threshold =
                MIGR_COUNTER_REL_TO_EFF(nodeid,
                                        NODEPDA_MCD(nodeid)->migr_system_kparms.migr_unpegging_control_threshold);
        
	pfd = pfntopfdat(startpfn);
        pfncounter = numpfns; 

        NUMA_STAT_INCR(nodeid, migr_unpegger_calls);
        NUMA_STAT_SAMPLE(nodeid, migr_unpegger_lastnpages, 0);

	while ((pfd = pfdat_probe(pfd, &npgs)) != NULL) {
		ASSERT(pfd != NULL);

		swpfn = pfdattopfn(pfd);

                ASSERT(PFN_TO_CNODEID(swpfn) == nodeid);

                
		while (npgs--) {
                        pfms = PFN_TO_PFMS(swpfn);
                        PFMS_TESTBIT_SET(pfms);
                        PFMS_LOCK(pfms, s);
                        pfmsv = PFMS_GETV(pfms);
                        if (PFMSV_STATE_IS_MAPPED(pfmsv) && PFMSV_IS_MIGR_ENABLED(pfmsv) && !PFMSV_IS_FROZEN(pfmsv)) {
                                hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);
                                for (i = 0; i < NUM_OF_HW_PAGES_PER_SW_PAGE(); i++) {
                                        /*
                                         * We just check tha count for the home node -
                                         * The theory here is that the home ref counter
                                         * is usually going to be the highest. Otherwise
                                         * the page should ahve been migrated.
                                         */
                                        count = MIGR_COUNT_GET(MIGR_COUNT_REG_GET(hwpfn, nodeid), nodeid);
                                        if (count > pegging_threshold) {
                                                NUMA_STAT_INCR(nodeid, migr_unpegger_npages);
                                                NUMA_STAT_INCR(nodeid, migr_unpegger_lastnpages);
                                                migr_refcnt_update_counters(nodeid,
                                                                            swpfn,
                                                                            pfmsv,
                                                                            MD_PROT_MIGMD_IREL);
                                                break;
                                        }
                                        hwpfn++;
                                }
			}
                        PFMS_UNLOCK(pfms, s);

			swpfn++;

                        
                        if (--pfncounter == 0) {
#ifdef DEBUG_MEMTICK
                                printf("unpegging_control: N[%d], F[%d] T[%d]\n",
                                       numpfns, startpfn, swpfn - 1);
#endif                                
                                       
                                return (swpfn);
                        }

		}

		pfd = pfntopfdat(swpfn);
	}

#ifdef DEBUG
        printf("unpegging_control: Exit 2\n");
        printf("node %d, swpfn 0x%x, pfncounter %d, npgs %d, startpfn 0x%x, numpfns %d\n",
               nodeid,
               swpfn,
               pfncounter,
               npgs,
               startpfn,
               numpfns);
#endif

        return (swpfn);
}
							 
#endif /* SN0 */								   


