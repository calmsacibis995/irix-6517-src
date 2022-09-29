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
#include <sys/proc.h>

#include <sys/hwgraph.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "debug_levels.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "pfms.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "numa_init.h"
#include "migr_control.h"
#include "numa_hw.h"
#include "numa_tests.h"

int
numa_tests(int cmd, cnodeid_t apnode, void* arg)
{
        switch (cmd) {
        	/* Get the # of NUMA nodes in the system */
	case NUMNODES_GET:
        {
                if (copyout((void*)&numnodes, arg, sizeof(int))) {
                        return (EFAULT);
                }
                
		return (0);
        }

	case PFNHIGH_GET:
        {           
                pfn_t pfn;               
                               
                if (apnode == CNODEID_NONE) {
                        return (EINVAL);
                }

                pfn = pfdattopfn(PFD_HIGH(apnode));
                
                if (copyout((void*)&pfn, arg, sizeof(pfn_t))) {
                        return (EFAULT);
                }
                
		return(0);
        }

        case PFNLOW_GET:
        {           
                pfn_t pfn;               
                               
                if (apnode == CNODEID_NONE) {
                        return (EINVAL);
                }

                pfn = pfdattopfn(PFD_LOW(apnode));
                
                if (copyout((void*)&pfn, arg, sizeof(pfn_t))) {
                        return (EFAULT);
                }
                
		return(0);
        }
        
#ifdef SN0
        case REFCOUNTER_GET:
        {
                refcounter_info_t refcounter_info;
                int i;
                pfn_t hwpfn;
                cnodeid_t node;
                
                if (copyin(arg, (void*)&refcounter_info, sizeof(refcounter_info_t))) {
                        return (EFAULT);
                }

                hwpfn = SWPFN_TO_HWPFN_BASE(refcounter_info.pfn);
                
#ifdef DEBUG_REFCOUNTER_GET
                printf("REFCOUNTER_GET===> swpfn: 0x%x, hwpfn: 0x%x\n",
                       refcounter_info.pfn, hwpfn);
#endif
                
                for (i = 0; i < NUMATEST_HWPAGES; i++) {
                        for (node = 0; node < numnodes; node++) {
                                if (node >= NUMATEST_MAX_NODES) {
                                        break;
                                }
                                refcounter_info.set[i].counter[node] =
                                        MIGR_COUNT_GET(MIGR_COUNT_REG_GET(hwpfn, node), node);
#ifdef DEBUG_REFCOUNTER_GET                                
                                printf("REFCOUNTER_GET: hwpfn 0x%x, node 0x%x, value 0x%x\n",
                                       hwpfn, node, refcounter_info.set[i].counter[node]);
#endif
                                
                        }
                        hwpfn++;
                }

                if (copyout((void*)&refcounter_info, arg, sizeof(refcounter_info_t))) {
                        return (EFAULT);
                }

                return (0);
        }
#endif /* SN0 */
        
        case MLD_GETNODE:
        {
                mld_plac_info_t mld_plac_info;
                void* pmo;
                mld_t* mld;

                if (copyin(arg, (void*)&mld_plac_info, sizeof(mld_plac_info_t))) {
                        return (EFAULT);
                }

                pmo = pmo_ns_find(curpmo_ns(), mld_plac_info.handle, __PMO_ANY);

                if (pmo == NULL) {
                        return (ESRCH);
                }

                if (pmo_gettype(pmo) != __PMO_MLD) {
                        pmo_decref(pmo, pmo_ref_find);
                        return (EINVAL);
                }

                mld = (mld_t*)pmo;

                mld_plac_info.nodeid = mld_getnodeid(mld);
                mld_plac_info.radius = mld_getradius(mld);
                mld_plac_info.size   = mld_getsize(mld);

                if (copyout((void*)&mld_plac_info, arg, sizeof(mld_plac_info_t))) {
                        pmo_decref(pmo, pmo_ref_find);
                        return (EFAULT);
                }

                pmo_decref(pmo, pmo_ref_find);
                return (0);
        }
                
        default:
                return EINVAL;
        }


        /* NOTREACHED*/
	return(0);
}
        
