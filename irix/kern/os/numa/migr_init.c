/*****************************************************************************
 * Copyright 1997, Silicon Graphics, Inc.
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
#include <sys/types.h>
#include <sys/cpumask.h>
#include <sys/atomic_ops.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/systm.h>
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

/* 
 * Last thing needed for migration interrupt: enabling
 * XXX We have to fix 0xFFFFF for Hub rev 1.0 & 2.0
 */
int
migr_init(void)
{
        cnodeid_t node;

	for (node = 0; node < numnodes; node++) {

#if CELL_IRIX
		/*
		 * ZZZ - temp til distributed
		 */
		if (NODEPDA(node)->node_num_cpus == 0) {

			/* golden cell will appear to have headless nodes */

			continue;
		}
#endif

		/* 
		 * Both migration difference and absolute threshold 
		 * registers are dormant at this time.
		 */
		ASSERT(MIGR_THRESHOLD_ABS_IS_ENABLED(node) == 0);
		ASSERT(MIGR_THRESHOLD_DIFF_IS_ENABLED(node) == 0);

                migr_effthreshold_diff_set(node,
                                           MIGR_COUNTER_REL_TO_EFF(node,
                                                                   NODEPDA_MCD(node)->migr_as_kparms.migr_base_threshold));

                /*
                 * Check if we need to allocate memory for the
                 * software extended mem ref counters.
                 */

                if (NODEPDA_MCD(node)->migr_as_kparms.migr_refcnt_mode != REFCNT_MODE_DIS) {
                        int errcode;
                        if ((errcode = migr_refcnt_init(node)) != 0) {
                                cnodeid_t tnode;
                                cmn_err(CE_WARN,
                                        "Node[%d]: Extended Memory Reference Counter Initialization Failed",
                                        node);
                                cmn_err(CE_WARN,
                                        "Node[%d]: Errorcode: %d - All counters will be disabled",
                                        node, errcode);
                                for (tnode = 0; tnode < numnodes; tnode++) {
                                        migr_refcnt_dealloc(tnode);
                                        NODEPDA_MCD(tnode)->migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_DIS;
                                }
                        } else {
#if defined (DEBUG)
                                cmn_err(CE_NOTE,
                                        "Allocated Ext Mem Ref Counters for node %d", node);
#endif /* DEBUG */
                        }
                }

                /*
		 * Init migr intr prologue handler
		 * migr candidate
                 */
		NODEPDA(node)->migr_candidate = 0;

        }
                        

        return (0);
}

#endif /* SN0 */
