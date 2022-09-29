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
 * NUMA migration bounce control module 
 */

#include <sys/bsd_types.h>
#include <sys/types.h>
#include <sys/cpumask.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/atomic_ops.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include "pfms.h"
#include "migr_control.h"
#include "numa_hw.h"
#include "migr_bounce.h"
#include "migr_states.h"
#include "debug_levels.h"
#include "numa_stats.h"
#include "migr_refcnt.h"



/* ----------------------------------------------------------------------

   Periodic operations
   
   ---------------------------------------------------------------------- */

/*
 * Periodic bounce control operation:
 * -- run on each node 
 * -- scan all page frame NUMA descriptors on the node, do bounce control
 * Called by: migr_periodic_op()
 * Sync: none
 */

pfn_t
migr_periodic_bounce_control(cnodeid_t nodeid, pfn_t startpfn, pfn_t numpfns)
{
	pfd_t *pfd;
	int npgs;
	pfn_t swpfn;
        uint mgrcnt;
        uint dampencnt;
        pfms_t pfms;
        pfmsv_t pfmsv;
        pfn_t pfncounter;
        int s;
	
        NUMA_STAT_INCR(nodeid, migr_bouncecontrol_calls);
        NUMA_STAT_SAMPLE(nodeid, migr_bouncecontrol_last_melt_pages, 0);

        pfd = pfntopfdat(startpfn);
        pfncounter = numpfns;
        
	while ((pfd = pfdat_probe(pfd, &npgs)) != NULL) {

		ASSERT(pfd != NULL);

		swpfn = pfdattopfn(pfd);

                ASSERT(PFN_TO_CNODEID(swpfn) == nodeid);


		while (npgs--) {
                        pfms = PFN_TO_PFMS(swpfn);
                        PFMS_LOCK(pfms, s);
                        pfmsv = PFMS_GETV(pfms);
                        mgrcnt = PFMSV_MGRCNT_GET(pfmsv);
                        if (PFMSV_STATE_IS_MAPPED(pfmsv) && PFMSV_IS_MIGR_ENABLED(pfmsv)) {
                                if (PFMSV_IS_FROZEN(pfmsv)) {
                                        /*
                                         * We're melting the page
                                         */
                                        if (PFMSV_IS_MELT_ENABLED(pfmsv) &&
                                            mgrcnt > PFMSV_MLTTHRESHOLD_GET(pfmsv)) {
                                               PFMS_FROZEN_CLR(pfms);
                                               PFMS_MGRCNT_PUT(pfms, 0);
                                               PFMSV_FROZEN_CLR(pfmsv);
                                               migr_refcnt_update_counters(nodeid, swpfn, pfmsv, MD_PROT_MIGMD_IREL);
                                               NUMA_STAT_INCR(nodeid, migr_bouncecontrol_melt_pages);
                                               NUMA_STAT_INCR(nodeid, migr_bouncecontrol_last_melt_pages);
                                        } else if (mgrcnt <  PFMSV_MGRCNT_MAX) {
                                                PFMS_MGRCNT_PUT(pfms, (mgrcnt+1));
                                        }
                                } else {
                                        /*
                                         * We're aging the bounce count
                                         */
                                        if (mgrcnt > 0) {
                                                PFMS_MGRCNT_PUT(pfms, (mgrcnt-1));
                                        }
                                        /*
                                         * And we're also decaying the dampening count
                                         */
                                        dampencnt = PFMSV_MIGRDAMPENCNT_GET(pfmsv);
                                        if (dampencnt > 0) {
                                                PFMS_MIGRDAMPENCNT_PUT(pfms, (dampencnt -1));
                                        }
                                }
                        }
                        PFMS_UNLOCK(pfms, s);

			swpfn++;

                        if (--pfncounter == 0) {
#ifdef DEBUG_MEMTICK
                                printf("bounce_control: N[%d], F[%d] T[%d]\n",
                                       numpfns, startpfn, swpfn - 1);
#endif                                
                                       
                                return (swpfn);
                        }
		}

		pfd = pfntopfdat(swpfn);
	}
#ifdef DEBUG
        printf("bounce_control: Exit 2\n");
#endif        

        return (swpfn);
}


/* ----------------------------------------------------------------------

   Invoked by migration manager
   
   ---------------------------------------------------------------------- */

/* 
 * Freeze the page if it's bouncing, incr migr count
 */

migr_page_state_t
migr_manager_bounce_control(pfms_t pfms, pfn_t swpfn, cnodeid_t source_nodeid)
{
        pfmsv_t pfmsv;
        uint mgrcnt;
        uint mgrtrh;
        uint dampencnt;
        uint dampenftr;

        /*
         * Counting may not be accurate because we don't
         * increment the count atomically.
         * This is a reasonable approach since we're
         * just trying to loosely identify bouncing
         * pages for the sake of performance and prevent
         * saturation of the links due to excessive migration.
         */

        pfmsv = PFMS_GETV(pfms);
        mgrcnt = PFMSV_MGRCNT_GET(pfmsv);
        mgrtrh = PFMSV_FRZTHRESHOLD_GET(pfmsv);
        dampencnt = PFMSV_MIGRDAMPENCNT_GET(pfmsv);
        dampenftr = PFMSV_MIGRDAMPENFACTOR_GET(pfmsv);

        if (PFMSV_IS_MIGRDAMPEN_ENABLED(pfmsv)) {
                /*
                 * Check if we're still within the dampening period
                 */
                if (dampencnt < dampenftr) {
                        PFMS_MIGRDAMPENCNT_PUT(pfms, (dampencnt + 1));
                        migr_restart(swpfn, pfms);
                        NUMA_STAT_INCR(source_nodeid, migr_bouncecontrol_dampened_pages);
                        return (DAMPENED);
                } else {
                        PFMS_MIGRDAMPENCNT_PUT(pfms, 0);
                }
        }

        if (PFMSV_IS_FREEZE_ENABLED(pfmsv)) {
                if (mgrcnt >= mgrtrh) {
                        /* 
                         * Freeze the page 
                         */
                        PFMS_FROZEN_SET(pfms);
                        PFMS_MGRCNT_PUT(pfms, 0);
                        migr_restart(swpfn, pfms);
                        NUMA_STAT_INCR(source_nodeid, migr_bouncecontrol_frozen_pages);
                        return (FROZEN);
                } else  {
                        /*
                         * Just incr the migr count
                         */
                        if (mgrcnt <  PFMSV_MGRCNT_MAX) {
                                PFMS_MGRCNT_PUT(pfms, (mgrcnt+1));
                        }
                }
        }

        return (MIGRATABLE);
        
}






