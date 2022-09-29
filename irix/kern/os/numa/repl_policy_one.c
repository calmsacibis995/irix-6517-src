/*
 * os/numa/repl_policy_one.c
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
#include <sys/pmo.h>
#include <sys/numa.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "numa_init.h"


static int repl_policy_one_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
static char* repl_policy_one_name(void);
static void repl_policy_one_destroy(pm_t* pm);
static int repl_policy_one_repl_srvc(struct pm* pm,
                                     pm_srvc_t srvc,
                                     void* args, ...);


/*
 * The ReplicationOne initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
repl_policy_one_init()
{
        pmfactory_export(repl_policy_one_create, repl_policy_one_name());

        return (0);
}

/*
 * The constructor for ReplicationOne takes no arguments
 */

/*ARGSUSED3*/
static int
repl_policy_one_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns)
{
        ASSERT(pm);
        ASSERT(pmo_ns);
        
        pm->repl_srvc = repl_policy_one_repl_srvc;
        pm->repl_data = NULL;
        
        return (0);
}

static char *
repl_policy_one_name(void)
{
        return ("ReplicationOne");
}

/*ARGSUSED1*/
static void
repl_policy_one_destroy(pm_t* pm)
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
repl_policy_one_repl_srvc(struct pm* pm, pm_srvc_t srvc, void* args, ...)
{
        ASSERT(pm);
        
        switch (srvc) {
        case PM_SRVC_DESTROY:
                repl_policy_one_destroy(pm);
                return (0);

        case PM_SRVC_GETNAME:
		*(char **)args = repl_policy_one_name();
                return (0);

        case PM_SRVC_GETAFF:
                raff_setup((raff_t*)args, 0, CNODEMASK_ZERO(), 1, RAFFATTR_ATTRACTION);
                return (0);

	case PM_SRVC_GETARG:
		*(int *)args = 0;
		return (0);

        default:
                cmn_err(CE_PANIC,
                        "[repl_policy_one_repl_srvc]: Invalid service type");
        }

        return (0);
}
                                    
