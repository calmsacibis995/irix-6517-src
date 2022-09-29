/*****************************************************************************
 * Copyright 1995-1999, Silicon Graphics, Inc.
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

#include <sys/bsd_types.h>
#include <sys/types.h>
#include <sys/cpumask.h>
#include <sys/nodepda.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/numa.h>
#include <sys/systm.h>
#include "pfms.h"
#include "migr_control.h"
#include "numa_hw.h"
#include "migr_states.h"
#include "debug_levels.h"
#include "numa_stats.h"
#include <sys/SN/hwcntrs.h>
#include "migr_refcnt.h"





/*
 * XXX Hub migration difference threshold is fixed for Hub 1.0 & 2.0
 * XXX In Hub 2.1 and above, the actual migration difference threshold
 *     is 0xFFFFF subtracted by the value in the migration difference 
 *     threshold register.
 */
void
migr_effthreshold_diff_set(cnodeid_t node, uint eff_threshold)
{
#if HUB_MIGR_WAR
        if (CNODE_NUM_CPUS(node) && WAR_MD_MIGR_ENABLED(node)) {
			
                /*
                 * Hub of rev 1.0 & 2.0 has a dormant migration 
                 * difference register.  We fix a value for the 
                 * migration threshold there and shall never touch it 
                 * again.
                 */
                MD_MIG_DIFF_THRESH_SET(COMPACT_TO_NASID_NODEID(node), 
                                       MIGR_DIFF_THRESHOLD_WAR);

        }
        else 
#endif /* HUB_MIGR_WAR */
        {
                /*
                 * Set the threshold already set in NODEPDA_MCD.
                 * In Hub 2.1 and above, the actual migration difference threshold
                 * is 0xFFFFF subtracted by the value in the migration difference 
                 * threshold register.
                 */

                MD_MIG_DIFF_THRESH_SET(COMPACT_TO_NASID_NODEID(node), 
                                       MD_MIG_DIFF_THRES_VALUE_MASK - eff_threshold);

        }
}        

/* -------------------------------------------------------------------------

   Operations on reference counters

   ------------------------------------------------------------------------- */

#ifdef NOTNOT
extern void
migr_refcnt_get_offsets(cnodeid_t home_node,
                        pfn_t swpfn,
                        pfmsv_t pfmsv,
                        uint* home_ofs,
                        uint* remote_ofs,
                        uint* new_mode,
                        refcnt_t** cbase);
void 
migr_pageref_reset(pfn_t swpfn, pfmsv_t pfmsv)
{
	int i, j;
	pfn_t hwpfn;
        uint mode;
        uchar_t migr_threshold;
        uint remote_ofs;
        uint home_ofs;
        refcnt_t* cbase;
        
	cnodeid_t home_node = PFN_TO_CNODEID(swpfn);

        migr_threshold = PFMSV_MGRTHRESHOLD_GET(pfmsv);

        cbase = NULL;

        
        migr_refcnt_get_offsets(home_node,
                                swpfn,
                                pfmsv,
                                &home_ofs,
                                &remote_ofs,
                                &mode,
                                &cbase
                                );

        if (cbase) {
                /*
                 * Reset the software counters
                 */
                
                for (i = 0, hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);
                     i < NUM_OF_HW_PAGES_PER_SW_PAGE();
                     i++,  hwpfn++, cbase+=numnodes) {
                
                        /*
                         * For every counter in the set associated with hwpfn
                         */
                        for (j = 0; j < numnodes; j++) {
                                /*
                                 * Zero out extended counter
                                 */
                                cbase[j] = 0;
                        }
                }
        }
        

        hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);

        for (i = 0; i < NUM_OF_HW_PAGES_PER_SW_PAGE(); i++) {

                for(j = 0; j < numnodes; j++) {
#ifdef HUB_MIGR_WAR
                        if (CNODE_NUM_CPUS(home_node) && WAR_MD_MIGR_ENABLED(home_node)) {
                                if (j == home_node) {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           MIGR_DIFF_THRESHOLD_REL_TO_EFF(j, migr_threshold),
                                                           MD_PROT_MIGMD_OFF);

                                
                                } else {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           0,
                                                           mode);
                                }
                        } else
#endif /* HUB_MIGR_WAR */
                        {
                                if (j == home_node) {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           home_ofs,
                                                           MD_PROT_MIGMD_OFF);
                                } else {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           remote_ofs,
                                                           mode);
                                }
                        }
                        
                }
                
                hwpfn++;
                
        }
}

#endif

void
migr_pageref_disable(pfn_t swpfn)
{
	int i;
        int j;
	pfn_t hwpfn;

        hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);

	for (i = 0; i < NUM_OF_HW_PAGES_PER_SW_PAGE(); i++) {
                for (j = 0; j < numnodes; j++) {
                        MIGR_COUNT_REG_SET(hwpfn,
                                           j, 
                                           0,
                                           MD_PROT_MIGMD_OFF);
                }
                hwpfn++;
        }
}



#endif /* SN0 */

