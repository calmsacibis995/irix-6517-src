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

#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/cpumask.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>

#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/pmo.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>

#include <sys/hwgraph.h>
#include "pfms.h"
#include "migr_control.h"
#include "numa_hw.h"
#include "numa_stats.h"
#include "migr_engine.h"
#include "mem_tick.h"

#define IS_ON_OR_OFF(b) (((b)==0) || ((b)==1))

#define IS_MIGR_MECH_IMMEDIATE(m)  ((m)==MIGR_MECH_IMMEDIATE)

#define IS_PERCENTAGE(x) ((x) <= 100)

#define EINVAL_MESSAGE(x) /*cmn_err(CE_WARN, "[migr_parms_check]: %3d", (x))*/

int
migr_parms_check(migr_parms_t* migr_parms, cnodeid_t node)
{
        /*
         * System parameters
         */

        ASSERT(migr_parms);
        
        /* memoryd must always be disabled */
        if (migr_parms->migr_system_kparms.memoryd_enabled != 0) {
                EINVAL_MESSAGE(1);
                return EINVAL;
        }

        /* traffic control must be disabled */
        if (migr_parms->migr_system_kparms.migr_traffic_control_enabled != 0) {
                EINVAL_MESSAGE(2);
                return EINVAL;
        }

        /* unpegging control can be on or off */
        if (!IS_ON_OR_OFF(migr_parms->migr_system_kparms.migr_unpegging_control_enabled)) {
                EINVAL_MESSAGE(3);
                return EINVAL;
        }

        /* all mechs must be IMMEDIATE */
        if (!IS_MIGR_MECH_IMMEDIATE(migr_parms->migr_system_kparms.migr_auto_migr_mech)) {
                EINVAL_MESSAGE(4);
                return EINVAL;
        }
        if (!IS_MIGR_MECH_IMMEDIATE(migr_parms->migr_system_kparms.migr_user_migr_mech)) {
                EINVAL_MESSAGE(4);
                return EINVAL;
        }        

        if (!IS_MIGR_MECH_IMMEDIATE(migr_parms->migr_system_kparms.migr_coaldmigr_mech)) {
                EINVAL_MESSAGE(5);
                return EINVAL;
        }

        /* restrict max reference threshold depending on type of directory */
        if (NODE_DIRTYPE_GET(cnodeid()) == DIRTYPE_PREMIUM) {
                if (migr_parms->migr_system_kparms.migr_threshold_reference > MIGR_COUNTER_PREMIUM_MAX) {
                        EINVAL_MESSAGE(6);
                        return EINVAL;
                }
        } else {
                if (migr_parms->migr_system_kparms.migr_threshold_reference > MIGR_COUNTER_STANDARD_MAX) {
                        EINVAL_MESSAGE(7);
                        return EINVAL;
                }
        }

        /* restrict refcnt update threshold depending on type of directory */
        if (NODE_DIRTYPE_GET(cnodeid()) == DIRTYPE_PREMIUM) {
                if (migr_parms->migr_system_kparms.migr_effthreshold_refcnt_overflow > MIGR_COUNTER_PREMIUM_MAX) {
                        EINVAL_MESSAGE(8);
                        return EINVAL;
                }
        } else {
                if (migr_parms->migr_system_kparms.migr_effthreshold_refcnt_overflow > MIGR_COUNTER_STANDARD_MAX) {
                        EINVAL_MESSAGE(9);
                        return EINVAL;
                }
        }

        /* we don't care about traffic control paramaters for now because traffic control is off */

        /* Bounce control interval: period in terms of memticks (at least 1) */

        if (migr_parms->migr_system_kparms.migr_bounce_control_interval < 1) {
                EINVAL_MESSAGE(10);
                return EINVAL;
        }

        /* Unpegging control interval: period in terms of memticks (at least 1) */
        if (migr_parms->migr_system_kparms.migr_unpegging_control_interval < 1) {
                EINVAL_MESSAGE(11);
                return EINVAL;
        }

        /* Unpegging threshold - percentage */
        if (!IS_PERCENTAGE(migr_parms->migr_system_kparms.migr_unpegging_control_threshold)) {
                EINVAL_MESSAGE(12);
                return EINVAL;
        }

        /* Queue control must be disabled and we don't care about queue parameters */
        if (migr_parms->migr_system_kparms.migr_queue_control_enabled) {
                EINVAL_MESSAGE(13);
                return EINVAL;
        }

        /* Memory pressure is ON/OFF bit */
        if (!IS_ON_OR_OFF(migr_parms->migr_system_kparms.migr_memory_low_enabled)) {
                EINVAL_MESSAGE(14);
                return EINVAL;
        }

        /* Memory pressure low threshold is a percentage */
        if (!IS_PERCENTAGE(migr_parms->migr_system_kparms.migr_memory_low_threshold)) {
                EINVAL_MESSAGE(15);
                return EINVAL;
        }

        /* Migration vehicle can be BTE/TLBSYNC for some machs, TLBSYNC only for other machs */
        if ((migr_parms->migr_system_kparms.migr_vehicle != MIGR_VEHICLE_IS_BTE) &&
            (migr_parms->migr_system_kparms.migr_vehicle != MIGR_VEHICLE_IS_TLBSYNC)) {
                EINVAL_MESSAGE(16);
                return EINVAL;
        }
        if ((migr_parms->migr_system_kparms.migr_vehicle == MIGR_VEHICLE_IS_BTE) &&
            (!NODEPDA_MCD(node)->bte_poison_ok)) {
                EINVAL_MESSAGE(17);
                return (ENXIO);
        }

        /* Migration base mode */
        switch (migr_parms->migr_as_kparms.migr_base_mode) {
        case MIGRATION_MODE_DIS:
        case MIGRATION_MODE_EN:
        case MIGRATION_MODE_ON:
        case MIGRATION_MODE_OFF:
                break;
        default:
                EINVAL_MESSAGE(18);
                return EINVAL;
        }

        /* freeze enabled is ON/OFF */
        if (!IS_ON_OR_OFF(migr_parms->migr_as_kparms.migr_freeze_enabled)) {
                EINVAL_MESSAGE(19);
                return EINVAL;
        }

        /* melt enabled is ON/OFF */
        if (!IS_ON_OR_OFF(migr_parms->migr_as_kparms.migr_melt_enabled)) {
                EINVAL_MESSAGE(20);
                return EINVAL;
        }

        /* Reference Counting base mode */
        switch (migr_parms->migr_as_kparms.migr_refcnt_mode) {
        case REFCNT_MODE_DIS:
        case REFCNT_MODE_EN:
        case REFCNT_MODE_ON:
        case REFCNT_MODE_OFF:
                break;
        default:
                EINVAL_MESSAGE(21);
                return EINVAL;
        }

        /* if no refcounter space has been allocated, we cannot have refcnt enabled */
        if (NODEPDA(node)->migr_refcnt_counterbase == NULL) {
                if (migr_parms->migr_as_kparms.migr_refcnt_mode != REFCNT_MODE_DIS) {
                        EINVAL_MESSAGE(22);
                        return (ENOENT);
                }
        }
        
        /* enqueue on fail must be off */
        if (migr_parms->migr_as_kparms.migr_enqonfail_enabled) {
                EINVAL_MESSAGE(23);
                return EINVAL;
        }

        /* dampening is ON/OFF */
        if (!IS_ON_OR_OFF(migr_parms->migr_as_kparms.migr_dampening_enabled)) {
                EINVAL_MESSAGE(24);
                return EINVAL;
        }
                
        /* base threshold is pecentage */
        if (!IS_PERCENTAGE(migr_parms->migr_as_kparms.migr_base_threshold)) {
                EINVAL_MESSAGE(25);
                return EINVAL;
        }

        /* minimum distance to do migration can be any anumber between 0..255 */

        /* min_maxradius is only used at system init */

        /* freeze threshold is percentage */
        if (!IS_PERCENTAGE(migr_parms->migr_as_kparms.migr_freeze_threshold)) {
                EINVAL_MESSAGE(26);
                return EINVAL;
        }        

        /* melt threshold is pecentage */
        if (!IS_PERCENTAGE(migr_parms->migr_as_kparms.migr_melt_threshold)) {
                EINVAL_MESSAGE(27);
                return EINVAL;
        }             

        /* dampening factor is percentage */
        if (!IS_PERCENTAGE(migr_parms->migr_as_kparms.migr_dampening_factor)) {
                EINVAL_MESSAGE(28);
                return EINVAL;
        }             
        
        EINVAL_MESSAGE(0);
        return 0;
}
        
                

int
numa_tune(int cmd, char* nodepath, void* arg)
{
	int cnode;
        vnode_t* vp;
        cnodeid_t apnode;

        if (nodepath == NULL) {
                apnode = CNODEID_NONE;
        } else {
                if (lookupname(nodepath,
                               UIO_USERSPACE,
                               FOLLOW,
                               NULLVPP,
                               &vp,
			       NULL)) {
                        return (ENOENT);
                }

                apnode = master_node_get(vp->v_rdev);
                VN_RELE(vp);
        }
        
        ASSERT((apnode == CNODEID_NONE) || (apnode >= 0 && apnode < numnodes));
                
	switch (cmd) {
        case MIGR_PARMS_GET:
        {
                migr_parms_t migr_parms;
                
                if (apnode == CNODEID_NONE) {
                        apnode = cnodeid();
                }

                if (arg == NULL) {
                        return (EFAULT);
                }

                migr_parms.migr_as_kparms = NODEPDA_MCD(apnode)->migr_as_kparms;
                migr_parms.migr_system_kparms = NODEPDA_MCD(apnode)->migr_system_kparms;
                
                if (copyout((void*)&migr_parms, arg, sizeof(migr_parms_t))) {
                        return (EFAULT);
                }

                return (0);
        }

        case MIGR_PARMS_SET:
        {
                migr_parms_t migr_parms;
                int errcode;

		if (!_CAP_ABLE(CAP_MEMORY_MGT)) {
			return (EPERM);
                }
                
                if (arg == NULL) {
                        return (EFAULT);
                }
                
                if (copyin(arg, (void*)&migr_parms, sizeof(migr_parms_t))) {
                        return (EFAULT);
                }

                /*
                 * Check parameters
                 */
                if (apnode == CNODEID_NONE) {               
                        for (cnode = 0; cnode < numnodes; cnode++) {
                                if ((errcode = migr_parms_check(&migr_parms, cnode)) != 0) {
                                        return (errcode);
                                }
                        }
                } else {
                        if ((errcode = migr_parms_check(&migr_parms, apnode)) != 0) {
                                return (errcode);
                        }
                }

                /*
                 * Quiesce memticks
                 */
                if (apnode == CNODEID_NONE) {               
                        for (cnode = 0; cnode < numnodes; cnode++) {
                                if (mem_tick_quiesce_node(cnode) != 0) {
                                         int i;
                                         for (i = 0; i < cnode; i++) {
                                                 (void)mem_tick_reactivate_node(i);
                                         }
                                         return EBUSY;
                                }
                        }
                } else {
                        if (mem_tick_quiesce_node(apnode) != 0) {
                                return EBUSY;
                        } 
                }

                /*
                 * Set parameters
                 */
                if (apnode == CNODEID_NONE) {
                        for (cnode = 0; cnode < numnodes; cnode++) {
                                NODEPDA_MCD(cnode)->migr_as_kparms = migr_parms.migr_as_kparms;
                                NODEPDA_MCD(cnode)->migr_system_kparms = migr_parms.migr_system_kparms;
                        }
                } else {
                        NODEPDA_MCD(apnode)->migr_as_kparms = migr_parms.migr_as_kparms;
                        NODEPDA_MCD(apnode)->migr_system_kparms = migr_parms.migr_system_kparms;
                }

                /*
                 * Sync hardware registers with new system parameters
                 */
                if (apnode == CNODEID_NONE) {
                        for (cnode = 0; cnode < numnodes; cnode++) {
                                /* absolute refcnt threshold */
                                MIGR_THRESHOLD_ABS_SET(
                                        cnode,
                                        NODEPDA_MCD(cnode)->migr_system_kparms.migr_effthreshold_refcnt_overflow
                                        );

                        }
                } else {
                        /* absolute refcnt threshold */
                        MIGR_THRESHOLD_ABS_SET(
                                apnode,
                                NODEPDA_MCD(apnode)->migr_system_kparms.migr_effthreshold_refcnt_overflow
                                );
                }
                

                /*
                 * Reinitialize memtick
                 */
                if (apnode == CNODEID_NONE) {               
                        for (cnode = 0; cnode < numnodes; cnode++) {
                                mem_tick_init_node(cnode);
                        }
                } else {
                        mem_tick_init_node(apnode);
                }                             

                /*
                 * Reactivate memticks
                 */
                if (apnode == CNODEID_NONE) {               
                        for (cnode = 0; cnode < numnodes; cnode++) {
                                errcode = mem_tick_reactivate_node(cnode);
                                ASSERT(errcode == 0);
                        }
                } else {
                        errcode = mem_tick_reactivate_node(apnode);
                        ASSERT(errcode == 0);
                }                

                return (0);
        }

        
	default: 
		return EINVAL;
	}

        /* NOTREACHED*/
	return(0);
}

		
		
