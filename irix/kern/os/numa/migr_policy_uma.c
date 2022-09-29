/*
 * os/numa/migr_policy_uma.c
 *
 *
 * Copyright 1995,1996 Silicon Graphics, Inc.
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
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/nodepda.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "pfms.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "numa_init.h"
#include "migr_parms.h"
#include "migr_control.h"
#include "migr_refcnt.h"


int migr_policy_uma_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
static char* migr_policy_uma_name(void);
static void migr_policy_uma_destroy(pm_t* pm);
static int migr_policy_uma_migr_srvc(struct pm* pm,
                                     pm_srvc_t srvc,
                                     void* args, ...);


/*
 * The MigrationDefault initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
migr_policy_uma_init()
{
        pmfactory_export(migr_policy_uma_create, migr_policy_uma_name());

        return (0);
}

/*
 * The constructor for MigrationDefault takes no arguments
 */

/*ARGSUSED*/
int
migr_policy_uma_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns)
{
        migr_as_kparms_t migr_as_kparms;
        
        ASSERT(pm);
        ASSERT(pmo_ns);
        
        migr_as_kparms.migr_base_mode = MIGRATION_MODE_DIS;
        migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_DIS;
        migr_as_kparms.migr_freeze_enabled = SETOFF;
        migr_as_kparms.migr_freeze_threshold = 0;
        migr_as_kparms.migr_melt_enabled = SETOFF;
        migr_as_kparms.migr_melt_threshold = 0;
        migr_as_kparms.migr_enqonfail_enabled = SETOFF;
        migr_as_kparms.migr_dampening_enabled = SETOFF;
        migr_as_kparms.migr_dampening_factor = 0;
        
        pm->migr_srvc = migr_policy_uma_migr_srvc;
        pm->migr_data = (void*)(*(__uint64_t*)(&migr_as_kparms));
        
        return (0);
}

/*ARGSUSED*/
static char*
migr_policy_uma_name(void)
{
        return ("MigrationDefault");
}

/*ARGSUSED*/
static void
migr_policy_uma_destroy(pm_t* pm)
{
        ASSERT(pm);
}


/*
 * This method provides generic services for this module:
 * -- destructor,
 * -- getname,
 * -- getaff (not valid for this placement module).
 */
/*ARGSUSED*/
static int
migr_policy_uma_migr_srvc(struct pm* pm, pm_srvc_t srvc, void* args, ...)
{
        ASSERT(pm);
        
        switch (srvc) {
        case PM_SRVC_DESTROY:
                migr_policy_uma_destroy(pm);
                return (0);

        case PM_SRVC_GETNAME:
		*(char **)args = migr_policy_uma_name();
		return (0);

        case PM_SRVC_GETAFF:
                return (0);

        case PM_SRVC_GETMIGRPARMS:
                /*
                 * For this policy, migr_data contains
                 * a migr_policy_parms_t structure.
                 */
                *(__uint64_t*)args = (__uint64_t)pm->migr_data;
                return (0);                

	case PM_SRVC_GETARG:
		*(int *)args = 0;
		return (0);

        default:
                cmn_err(CE_PANIC,
                        "[migr_policy_uma_migr_srvc]: Invalid service type");
        }

        return (0);
}
