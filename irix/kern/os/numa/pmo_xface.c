/*
 * os/numa/pmo_xface.c
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
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/pmo.h>
#include <sys/mmci.h>
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
#include "pmo_process.h"
#include "migr_user.h"

int
pmoctl(int opcode, void* inargs, void* inoutargs, rval_t* rvp)
{
        switch (opcode) {

        case PMO_MLDCREATE:      /* Create an MLD */
                rvp->r_val1 = pmo_xface_mld_create(inargs, -1);
                return (pmo_checkerror(rvp->r_val1));

        case PMO_MLDCREATEX:      /* Create an MLD with a specific handle */
                rvp->r_val1 = pmo_xface_mld_create(inargs,
					(pmo_handle_t)(__uint64_t)inoutargs);
                return (pmo_checkerror(rvp->r_val1));

        case PMO_MLDDESTROY:     /* Destroy an MLD */
                return (pmo_checkerror(pmo_xface_mld_destroy((pmo_handle_t)(__uint64_t)inargs)));

                
        case PMO_MLDSETCREATE:   /* Create an MLDSET */
                rvp->r_val1 = pmo_xface_mldset_create(inargs, -1);
                return (pmo_checkerror(rvp->r_val1));

        case PMO_MLDSETCREATEX:   /* Create an MLDSET with a specific handle */
                rvp->r_val1 = pmo_xface_mldset_create(inargs,
					(pmo_handle_t)(__uint64_t)inoutargs);
                return (pmo_checkerror(rvp->r_val1));
        case PMO_MLDSETPLACE:   /* Place an MLDSET */
                rvp->r_val1 = pmo_xface_mldset_place(inargs);
                return (pmo_checkerror(rvp->r_val1));

        case PMO_MLDSETDESTROY:  /* Destroy an MLDSET */
                return (pmo_checkerror(pmo_xface_mldset_destroy((pmo_handle_t)(__uint64_t)inargs)));
                
        case PMO_MLDSETOP:       /* Invoke an MLDSET method */   
                return (ENOSYS);


                /*
                 * Policy Module (PM) syscalls
                 */
                
        case PMO_PMCREATE:       /* Create a policy module */
                rvp->r_val1 = pmo_xface_pm_create((pm_info_t*)inargs, NULL);
                return (pmo_checkerror(rvp->r_val1));

        case PMO_PMCREATEX:       /* Create a policy module with a specific handle */
                rvp->r_val1 = pmo_xface_pm_create((pm_info_t*)inargs,
						(pmo_handle_t *)inoutargs);
                return (pmo_checkerror(rvp->r_val1));

        case PMO_PMDESTROY:      /* Destroy a policy module */
                return (pmo_checkerror(pmo_xface_pm_destroy((pmo_handle_t)(__uint64_t)inargs)));

        case PMO_PMOP:           /* Invoke policy module method */
                return (ENOPKG);

        case PMO_PMATTACH:   /* Attach group to address space range */
                return (pmo_checkerror(pmo_xface_pm_attach((pmo_handle_t)(__uint64_t)inargs,
                                                           (vrange_t*)inoutargs)));
                
        case PMO_PMGETDEFAULT:  /* Get default pm for this address space */
		rvp->r_val1 = pmo_xface_aspm_getdefaultpm((mem_type_t)(__uint64_t)inargs);
                return (pmo_checkerror(rvp->r_val1));

        case PMO_PMSETDEFAULT:     /* Set the default pm for the current process' address space */
                return (pmo_checkerror(pmo_xface_aspm_setdefaultpm(
				(pmo_handle_t)(__uint64_t)inargs,
				(mem_type_t)(__uint64_t)inoutargs)));

        case PMO_PMGETALL:      /* Get all pms for a virtual range */
                rvp->r_val1 = pmo_xface_pm_getpmhandles((vrange_t*)inargs,
                                                        (pmo_handle_list_t*)inoutargs);
                return (pmo_checkerror(rvp->r_val1));

        case PMO_PMSTAT:        /* Get pm state */
                return (pmo_checkerror(pmo_xface_pm_stat((pmo_handle_t)(__uint64_t)inargs,
                                                         (pm_stat_t*)inoutargs)));

        case PMO_PMSETPAGESIZE: /* Set pm page size */
                return (pmo_checkerror(pmo_xface_pm_set_pagesize((pmo_handle_t)(__uint64_t)inargs,
                                                                 (size_t)(__uint64_t)inoutargs)));


        case PMO_PROCMLDLINK:    /* Link process to MLD */
                return (pmo_checkerror(
                        pmo_xface_process_mldlink((mldlink_info_t*)inargs)));

        case PMO_USERMIGRATE:    /* User migration request */
                return (pmo_checkerror(migr_xface_migrate((vrange_t*)inargs,
                                                          ((pmo_handle_t)(__uint64_t)inoutargs))));
                
	case PMO_PMGET_PAGE_INFO:
                rvp->r_val1 = pmo_xface_get_pginfo(
			(vrange_t *)inargs, (pm_pginfo_list_t *)inoutargs);
                return (pmo_checkerror(rvp->r_val1));

	case PMO_MLDGETNODE:
                rvp->r_val1 = 
		pmo_xface_mld_getnode((pmo_handle_t)(__uint64_t)inargs);
                return (pmo_checkerror(rvp->r_val1));
	case PMO_GETNODEMASK:
	   {
   	    /* USE THIS FOR MAX_COMPACTNODES <= 64 nodes */
		cnodemask_t tmp;
		tmp = pmo_xface_aspm_getcnodemask();
		rvp->r_val1 = CNODEMASK_WORD(tmp,0);
		return 0;
	   }
	case PMO_SETNODEMASK:
	   {
   	    /* USE THIS FOR MAX_COMPACTNODES <= 64 nodes */
		cnodemask_t rval, tmp;
		CNODEMASK_CLRALL(tmp);
		CNODEMASK_SET_WORD(tmp,0,(__uint64_t)inargs);
		rval = pmo_xface_aspm_setcnodemask(tmp);
		rvp->r_val1 = CNODEMASK_WORD(rval,0);
		return 0;
	   }

#ifdef SN0XXL
	/*
	 * The GET/SET NODEMASK functions above don't
	 * support more than 64 nodes.  Add new functions
	 * that support more than 64 nodes.
	 *
    	 * These should be considered temporary and should
    	 * eventually be replaced.
	 */
	case PMO_GETNODEMASK_BYTESIZE:
	   {
		rvp->r_val1 = sizeof(cnodemask_t);
		return 0;
	   }
	case PMO_GETNODEMASK_UINT64:
	   {
   	    /* 
	     * USE THIS FOR MAX_COMPACTNODES > 64 nodes 
	     * pmoctl(PMO_GETNODEMASK_UINT64, (__uint64_t *)mask, (size_t)size)
	     */
		size_t msize;
		cnodemask_t rval;
		msize = (size_t)inoutargs;
		if (msize < sizeof(cnodemask_t)) {
		   return(ENOMEM);
		}
		rval = pmo_xface_aspm_getcnodemask();
		if (copyout((caddr_t)&rval, (caddr_t)inargs, sizeof(rval))) {
		   return(EFAULT);
		}
		rvp->r_val1 = 0;
		return 0;
	   }
	case PMO_SETNODEMASK_UINT64:
	   {
   	    /* 
	     * USE THIS FOR MAX_COMPACTNODES > 64 nodes 
	     * pmoctl(PMO_SETNODEMASK_UINT64, (__uint64_t *)mask, (size_t)size)
	     */
		size_t msize;
		cnodemask_t rval, tmp;
		CNODEMASK_CLRALL(tmp);
		msize = (size_t)inoutargs;
		if (msize < sizeof(cnodemask_t)) {
		   return(ENOMEM);
		}
		if (copyin((void *)inargs, (void *)&tmp, sizeof(tmp))) {
		   return(EFAULT);
		}

		rval = pmo_xface_aspm_setcnodemask(tmp);

		rvp->r_val1 = 0;
		return 0;
	   }
#endif /* SN0XXL */

        default:
                return (pmo_checkerror(PMOERR_INV_OPCODE));
        }
 
}

