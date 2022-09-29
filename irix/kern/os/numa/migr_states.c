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
 * Operations to transit from one state to another 
 */

#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/nodepda.h>
#include <sys/cpumask.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>

#include <sys/numa.h>
#include "pfms.h"
#include "pmo_base.h"
#include "pmo_error.h"
#include "pmo_ns.h"
#include "debug_levels.h"
#include "pmo_list.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "migr_control.h"
#include "migr_traffic.h"
#include "migr_bounce.h"
#include "migr_queue.h"
#include "migr_unpegging.h"
#include "migr_manager.h"
#include "numa_hw.h"
#include "migr_states.h"
#include "migr_refcnt.h"

int migr_mess = 0;

/* 
 * Set up migration control for a page
 */

void
migr_start(pfn_t swpfn, struct pm* pm)
{
        migr_as_kparms_t migr_as_kparms;
        ulong migr_all;
        pfmsv_t pfmsv;
	migr_as_kparms_t *nodepda_migr_parms;
        
#if CELL_IRIX
	/*
	 * ZZZ - temp til distributed
	 */
	if (1) {
		/* Doesn't support this yet */

		return;
	}
#endif

        ASSERT(pm);

        /*
         * Initialize the pfmsv to all 0's
         */
        PFMSV_CLR(pfmsv);
        

        /*
         * Get migration parameters from policy module
         */
        (void)(*pm->migr_srvc)(pm, PM_SRVC_GETMIGRPARMS, (void*)&migr_all);
        migr_as_kparms = (*(migr_as_kparms_t*)(&migr_all));

	nodepda_migr_parms = &(nodepda->mcd->migr_as_kparms);

        switch(nodepda_migr_parms->migr_base_mode) {
        case MIGRATION_MODE_DIS:
                /*
                 * Migration forced OFF
                 */
                PFMSV_MIGR_ENABLED_CLR(pfmsv);
                break;
        case MIGRATION_MODE_EN:
                /*
                 * Migration forced ON
                 */
                PFMSV_MIGR_ENABLED_SET(pfmsv);
                PFMSV_MGRTHRESHOLD_PUT(pfmsv, 
				nodepda_migr_parms->migr_base_threshold);
                break;
        case MIGRATION_MODE_OFF:
        case MIGRATION_MODE_ON:
                /*
                 * Migration is up to the policy module
                 */
                switch (migr_as_kparms.migr_base_mode) {
                case MIGRATION_MODE_OFF:
                case MIGRATION_MODE_DIS:
                        /*
                         * Migration OFF
                         */
                        PFMSV_MIGR_ENABLED_CLR(pfmsv);
                        break;
                case MIGRATION_MODE_ON:
                case MIGRATION_MODE_EN:
                        /*
                         * Migration ON
                         */
                        PFMSV_MIGR_ENABLED_SET(pfmsv);
                        PFMSV_MGRTHRESHOLD_PUT(pfmsv, migr_as_kparms.migr_base_threshold);
                        break;
                default:
                        panic("[migr_start]: Invalid migr mode specified by PM");
                }
                break;
        default:
                panic("[migr_start]: Invalid default migr mode");
        }

        switch(nodepda_migr_parms->migr_refcnt_mode) {
        case REFCNT_MODE_DIS:
                /*
                 * Refcounting forced OFF
                 */
                PFMSV_MIGRREFCNT_ENABLED_CLR(pfmsv);
                break;
        case REFCNT_MODE_EN:
                /*
                 * Refcounting forced ON
                 */
                PFMSV_MIGRREFCNT_ENABLED_SET(pfmsv);
                break;
        case REFCNT_MODE_OFF:
        case REFCNT_MODE_ON:
                /*
                 * Refcounting is up to the policy module
                 */
                switch (migr_as_kparms.migr_refcnt_mode) {
                case REFCNT_MODE_OFF:
                case REFCNT_MODE_DIS:
                        /*
                         * Refcounting OFF
                         */
                        PFMSV_MIGRREFCNT_ENABLED_CLR(pfmsv);
                        break;
                case REFCNT_MODE_ON:
                case REFCNT_MODE_EN:
                        /*
                         * Refcounting ON
                         */
                        PFMSV_MIGRREFCNT_ENABLED_SET(pfmsv);
                        break;
                default:
                        panic("[migr_start]: Invalid refcnt mode specified by PM");
                }
                break;
        default:
                panic("[migr_start]: Invalid default refcnt mode");
        }

        
        /*
         * Set the freezing parameters
         */
        if (nodepda_migr_parms->migr_freeze_enabled != MIGRATION_MODE_DIS) {
                if (migr_as_kparms.migr_freeze_enabled) {
                        PFMSV_FREEZE_ENABLED_SET(pfmsv);
                        PFMSV_FRZTHRESHOLD_PUT(pfmsv,
                                               PFMSV_MGRCNT_REL_TO_ABS(migr_as_kparms.migr_freeze_threshold));
                }
        }

        /*
         * Set the melting parameters
         */
        if (nodepda_migr_parms->migr_melt_enabled != MIGRATION_MODE_DIS) { 
                if (migr_as_kparms.migr_melt_enabled) {
                        PFMSV_MELT_ENABLED_SET(pfmsv);
                        PFMSV_MLTTHRESHOLD_PUT(pfmsv,
                                               PFMSV_MGRCNT_REL_TO_ABS(migr_as_kparms.migr_melt_threshold));
                }
        }

        /*
         * Set the dampening parameters
         */
        if (nodepda_migr_parms->migr_dampening_enabled != MIGRATION_MODE_DIS) { 
                if (migr_as_kparms.migr_dampening_enabled) {
                        PFMSV_MIGRDAMPEN_ENABLED_SET(pfmsv);
                        PFMSV_MIGRDAMPENFACTOR_PUT(pfmsv,
                                  PFMSV_MIGRDAMPENFACTOR_REL_TO_ABS(
					migr_as_kparms.migr_dampening_factor));
                }
        }
        

        /*
         * Set the enqonfail bit
         */
        if (migr_as_kparms.migr_enqonfail_enabled) {
                PFMSV_ENQONFAIL_SET(pfmsv);
        }

        /*
         * Initialize in the mapped state
         */
        PFMSV_STATE_MAPPED_SET(pfmsv);

        /*
         * Set pfms
         */
        PFMS_PUTV(PFN_TO_PFMS(swpfn), pfmsv);

        /*
         * Initialize counters according to migration mode:
         * a) migration enabled: relative mode
         * b) refcnt enabled: absolute mode
         * c) disable
         */

	if (nodepda_migr_parms->migr_refcnt_mode != REFCNT_MODE_DIS)
        	migr_refcnt_restart(swpfn, pfmsv, 1);/* reset sw counters */
        
        
}


void
migr_restart(pfn_t swpfn, pfms_t pfms)
{
        pfmsv_t pfmsv;
        int pfms_spl;
        
        PFMS_LOCK(pfms, pfms_spl);
        
        pfmsv = PFMS_GETV(pfms);
        if (PFMSV_STATE_IS_MIGRED(pfmsv)) {
                PFMS_STATE_MAPPED_SET(pfms);
        }
        PFMS_UNLOCK(pfms, pfms_spl);

        migr_refcnt_restart(swpfn, pfmsv, 0); 
        
}
                
void
migr_restart_clear(pfn_t swpfn, pfms_t pfms)
{
        pfmsv_t pfmsv;
        int pfms_spl;
        
        PFMS_LOCK(pfms, pfms_spl);
        
        pfmsv = PFMS_GETV(pfms);
        if (PFMSV_STATE_IS_MIGRED(pfmsv)) {
                PFMS_STATE_MAPPED_SET(pfms);
        }
        PFMS_UNLOCK(pfms, pfms_spl);

        migr_refcnt_restart(swpfn, pfmsv, 1); 
        
}        


/* 
 * Stop migration control for a page
 */
void 
migr_stop(pfn_t swpfn)
{
        pfms_t pfms;
        int s;
        

#if CELL_IRIX
	/*
	 * ZZZ - temp til distributed
	 */
	if (1) {
		return;
	}
#endif

        pfms = PFN_TO_PFMS(swpfn);
        migr_refcnt_update_counters(PFN_TO_CNODEID(swpfn),
                                    swpfn,
                                    PFMS_GETV(pfms),
                                    MD_PROT_MIGMD_OFF);
        PFMS_LOCK(pfms, s);
        PFMS_FLAGS_CLR(pfms); /* Moves page to STATE_UNMAPPED */
        PFMS_UNLOCK(pfms, s);

}

/*
 * Lock the pfms and
 * verify the state is still apt for migration
 */
int
migr_xfer_nested_lock(pfn_t old_pfn)
{
        pfms_t pfms;

        pfms = PFN_TO_PFMS(old_pfn);

        NESTED_PFMS_LOCK(pfms);
        
        if (!PFMS_STATE_IS_MIGRED(pfms)) {
                /*
                 * The page is not migratable anymore
                 */
                NESTED_PFMS_UNLOCK(pfms);
                return (1);
        }

        return (0);
}
        
                  
/*
 * Transfer pfms state when migrating a page
 */
void
migr_xfer_and_nested_unlock(pfn_t old_pfn, pfn_t new_pfn)
{
        pfms_t old_pfms;
        pfms_t new_pfms;
        pfmsv_t pfmsv;

        old_pfms = PFN_TO_PFMS(old_pfn);
        new_pfms = PFN_TO_PFMS(new_pfn);

        ASSERT(PFMS_IS_LOCKED(old_pfms));

        pfmsv = PFMS_GETV(old_pfms);
        
        /*
         * Move old page to STATE_UNMAPPED
         */
        PFMS_FLAGS_CLR(old_pfms);

        NESTED_PFMS_UNLOCK(old_pfms);

#ifdef NOTNOT

        
        migr_refcnt_update_counters(PFN_TO_CNODEID(old_pfn),
                                    old_pfn,
                                    pfmsv, /* value before clearing */
                                    MD_PROT_MIGMD_PREL);
#endif        
        /*
         * pfmsv has the lock set. we
         * have to clr it.
         */
        PFMSV_LOCK_CLR(pfmsv);

        PFMS_PUTV(new_pfms, pfmsv);
}



/*
 * Lock the pfms for pflipping
 */
pfmsv_t
pflip_pfms_get(pfd_t* old_pfd)
{
        pfms_t old_pfms;
        pfmsv_t old_pfmsv;
        pfn_t old_pfn;

        old_pfn = pfdattopfn(old_pfd);
        old_pfms = PFN_TO_PFMS(old_pfn);
        old_pfmsv = PFMS_GETV(old_pfms);
        migr_stop(old_pfn);
        
        return (old_pfmsv);
}
        
                  
/*
 * Set new pfmsv 
 */
void
pflip_pfms_set(pfd_t* new_pfd, pfmsv_t new_pfmsv)
{
        pfn_t new_pfn;
        pfms_t new_pfms;
        
        new_pfn = pfdattopfn(new_pfd);
        new_pfms = PFN_TO_PFMS(new_pfn);
        PFMS_PUTV(new_pfms, new_pfmsv);
        migr_refcnt_restart(new_pfn, new_pfmsv, 1); 
}

#ifdef USELESS
/* mark page as unmigratable */
void
migr_mark_unmigratable(pde_t *pt) 
{
	pfms_t pfms;
        pfmsv_t pfmsv;
        pfn_t pfn;
	int s;

        pfn = pg_getpfn(pt);
	pfms = PFN_TO_PFMS(pfn);
        pfmsv = PFMS_GETV(pfms);
	PFMS_LOCK(pfms, s);
	PFMS_MIGR_ENABLED_CLR(pfms);
	PFMS_UNLOCK(pfms, s);

        migr_refcnt_update_counters(PFN_TO_CNODEID(pfn),
                                    pfn,
                                    pfmsv, /* value before clearing */
                                    MD_PROT_MIGMD_OFF);      
}

#endif
