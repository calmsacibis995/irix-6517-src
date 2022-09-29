/*
 * os/numa/pm_policy_stubs.c
 *
 *
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

extern int plac_policy_uma_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
extern int fbck_policy_uma_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
extern int migr_policy_uma_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
extern int repl_policy_uma_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);

int
pm_policy_stubs_init(void)
{

        /*
         * We only need to re-direct the non-default policies.
         * Automatically, the uma policies are installed as
         * default policies for uma machines.
         */
        
        /*
         * Placement Policies
         */
        pmfactory_export(plac_policy_uma_create, "PlacementFirstTouch");
        pmfactory_export(plac_policy_uma_create, "PlacementFixed");
        pmfactory_export(plac_policy_uma_create, "PlacementRoundRobin");
        pmfactory_export(plac_policy_uma_create, "PlacementThreadLocal");

        /*
         * Fallback Policies
         */
        pmfactory_export(fbck_policy_uma_create, "FallbackLargepage");

        /*
         * Migration Policies
         */
        pmfactory_export(migr_policy_uma_create, "MigrationControl");
        
        /*
         * Replication Policies
         */
        pmfactory_export(repl_policy_uma_create, "ReplicationOne");

        return (0);
}
