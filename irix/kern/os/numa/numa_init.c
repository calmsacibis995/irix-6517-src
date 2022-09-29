/*
 * os/numa/numa_init.c
 *
 *
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
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/sysmacros.h>
#include <sys/pmo.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mmci.h>
#include "pmo_base.h"
#include "pfms.h"
#include "numa_init.h"
#include "numa_hw.h"
#include "repl_control.h"
#include "migr_control.h"
#include "numa_stats.h"

/* Defined in startup.c */
extern void numa_sim_init(void);
extern int migr_init(void);

typedef struct numa_init_table {
        int   (*init_function)(void);
        char* name;
} numa_init_table_t;

numa_init_table_t numa_init_table[] = {
        mld_init,                         "Mld",
        raff_init,                        "Raff",
        mldset_init,                      "MldSet",
        pmolist_init,                     "PmoList",
        topology_init,                    "Topology",
	migr_init,                        "Migr",
        plac_policy_default_init,         "Placement Policy: Default",
        plac_policy_fixed_init,           "Placement Policy: Fixed",
        plac_policy_firsttouch_init,      "Placement Policy: FirstTouch",
	plac_policy_thread_init,	  "Placement Policy: ThreadLocal",
        plac_policy_roundrobin_init,      "Placement Policy: RoundRobin",
        plac_policy_cachecolor_init,      "Placement Policy: CacheColor",
        fbck_policy_default_init,         "Fallback Policy: Default",
        fbck_policy_local_init,        	  "Fallback Policy: Local",
        migr_policy_default_init,         "Migration Policy: Default",
        migr_policy_control_init,         "Migration Policy: Control",
        migr_policy_refcnt_init,          "Migration Policy: Refcnt",
        repl_policy_one_init,             "Replication Policy: One",
        mem_tick_init,                    "MemTickInit",
        0,                                NULL
};
        
/*
 * This method takes care of initializing the complete
 * numa management module. All init routines in the numa
 * module that need to be called at boot time should be called
 * from here.
 */

void
numa_init()
{
        numa_init_table_t* pt = numa_init_table;
        int errcode;

#ifdef MMCI_REFCD
        pmo_base_refcd_trace_init();
#endif         
        while (pt->init_function) {
#ifdef DEBUG            
                printf("INITIALIZING {%s}\n", pt->name);
#endif                
                if ((errcode =  (*pt->init_function)()) != 0) {
                        cmn_err(CE_PANIC,
                                "[numa_init]: Module <%s> initialization failed (%d)",
                                pt->name, errcode);
                }
                pt++;
        }
}


/*
 * Initialize the NUMA portion of the nodepda
 */
void 
nodepda_numa_init(cnodeid_t node, caddr_t* nextbyte)
{
        numa_stats_init(node, nextbyte);
	migr_control_init(node, nextbyte);
	repl_control_init(node, nextbyte);
}

