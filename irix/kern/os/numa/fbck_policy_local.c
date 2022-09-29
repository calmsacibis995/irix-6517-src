/*
 * os/numa/fbck_policy_local.c
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
#include <sys/proc.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <sys/sema.h>
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


static int fbck_policy_local_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
static char* fbck_policy_local_name(void);
static void fbck_policy_local_destroy(pm_t* pm);
static pfd_t* fbck_policy_local_fallback(struct pm* pm,
                                          uint ckey,
                                          int flags,
                                          mld_t* mld,
                                          size_t* ppsz);
static int fbck_policy_local_fbck_srvc(struct pm* pm,
					 pm_srvc_t srvc,
					 void* args, ...);


/*
 * The FallbackDefault initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
fbck_policy_local_init()
{
        pmfactory_export(fbck_policy_local_create, fbck_policy_local_name());

        return (0);
}

/*
 * The constructor for FallbackDeault takes no arguments
 */

/*ARGSUSED*/
static int
fbck_policy_local_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns)
{
        ASSERT(pm);
        ASSERT(pmo_ns);
        
        pm->fallback = fbck_policy_local_fallback;
        pm->fbck_srvc = fbck_policy_local_fbck_srvc;
        pm->fbck_data = 0;
        
        return (0);
}

/*ARGSUSED*/
static char*
fbck_policy_local_name(void)
{
        return ("FallbackLocal");
}

/*ARGSUSED*/
static void
fbck_policy_local_destroy(pm_t* pm)
{
        ASSERT(pm);
}


/*
 * We give preference to large pages on other nodes before 
 * allocating base pages from the same node.
 */
static pfd_t*
fbck_policy_local_fallback(struct pm* pm,
                             uint ckey,
                             int flags,
                             mld_t* mld,
                             size_t* ppsz)
{
        pfd_t* pfd;
        cnodeid_t node;
        int i;
	size_t	page_size, requested_page_size;

        ASSERT(pm);
        ASSERT(pm->pmo);
        ASSERT(mld);
        ASSERT(ppsz);

	requested_page_size = page_size = *ppsz;
        

	/*
	 * No memory was available on the requested node/requested pagesize.
	 * Try other nodes as follows:
	 *	- try to allocate the requested pagesize on neighboring
	 *	  nodes that are in the effective_nodemask.
	 *	- try to allocate a default pagesize on the requested
	 *	  node.
	 *	- try to allocate a default pagesize on neighboring
	 *	  nodes that are in the effective_nodemask.
	 *	- try to allocate the requested pagesize on neighboring
	 *	  nodes ignoring the effective_nodemask
	 *	- try to allocate a default pagesize on neighboring
	 *	  nodes ignoring the effective_nodemask
	 */

	/*
	 * Try the base page size on the local node first
	 */
	node = mld_getnodeid(mld);
	ASSERT(node >= 0);                
	pfd = pagealloc_size_node(node, ckey, flags, _PAGESZ);
	if (pfd != NULL) {
		*ppsz = _PAGESZ;
		return (pfd);
	}  

	/*
	 * If we could not find a page on the preferred mld,
	 * we try on the other mlds in the mldset.
	 */

	while (page_size >= _PAGESZ) {

		pm_mraccess(pm);
		if (pmo_gettype(pm->pmo) == __PMO_MLDSET) {
			mldset_t* mldset;
			pmolist_t* mldlist;
			mld_t* mld;
				
			mldset = (mldset_t*)pm->pmo;
			ASSERT(mldset);
			/*
			 * temporary reference to prevent the mldset
			 * from disappearing under our feet if some
			 * other thread happens to destroy it.
			 */
			pmo_incref(mldset, pmo_ref_keep);
			pm_mraccunlock(pm);

			mldset_acclock(mldset);
			
			mldlist = mldset_getmldlist(mldset);
			ASSERT(mldlist);

			for (i = 0; i < pmolist_getclen(mldlist); i++) {
				mld = (mld_t*)pmolist_getelem(mldlist, i);
				ASSERT(mld);
				node = mld_getnodeid(mld);
				ASSERT(node >= 0);                
				pfd = pagealloc_size_node(node, ckey, flags, *ppsz);
				if (pfd != NULL) {
					mldset_accunlock(mldset);
					pmo_decref(mldset, pmo_ref_keep);
					return (pfd);
				}
			}

			pmo_decref(mldset, pmo_ref_keep);
			mldset_accunlock(mldset);
		} else {
		     pm_mraccunlock(pm);
		}

		/*
		 * We give up, and borrow from some either the current
		 * node (first touch) or a topologically close node.
		 */

		pfd = pagealloc_size(ckey, flags|VM_NODEMASK, *ppsz);
		if (pfd != NULL)
			return(pfd);
		if (page_size > NBPP) {
                        if (pm->page_alloc_wait_timeout)
                                return ((pfd_t *)0);
			*ppsz = _PAGESZ;
			page_size = _PAGESZ;
		} else
			page_size = 0;
	}

	/*
	 * If we are running with an unrestricted nodemask, there is nothing
	 * more to try - no memory is available. Return NULL.
	 */
	if (CNODEMASK_EQ(get_effective_nodemask(curthreadp), boot_cnodemask))
		return ((pfd_t *)0);


	/*
	 * The thread is running with a restricted nodemask. Try again but this time
	 * try ALL nodes.
	 */
	page_size = requested_page_size;
	*ppsz = page_size;
	while (page_size >= _PAGESZ) {
		pfd = pagealloc_size(ckey, flags, *ppsz);
		if (pfd != NULL)
			return(pfd);
		if (page_size > NBPP) {
                        if (pm->page_alloc_wait_timeout)
                                return ((pfd_t *)0);
			*ppsz = _PAGESZ;
			page_size = _PAGESZ;
		} else
			page_size = 0;
	}

	
        return ((pfd_t *)0);
}
               
/*
 * This method provides generic services for this module:
 * -- destructor,
 * -- getname,
 * -- getaff (not valid for this placement module).
 */
/*ARGSUSED*/
static int
fbck_policy_local_fbck_srvc(struct pm* pm, pm_srvc_t srvc, void* args, ...)
{
        ASSERT(pm);
        
        switch (srvc) {
        case PM_SRVC_DESTROY:
                fbck_policy_local_destroy(pm);
                return (0);

        case PM_SRVC_GETNAME:
		*(char **)args = fbck_policy_local_name();
                return (0);

        case PM_SRVC_GETAFF:
                return (0);

	case PM_SRVC_GETARG:
		*(int *)args = 0;
		return (0);

        default:
                cmn_err(CE_PANIC, "[fbck_policy_local_srvc]: Invalid service type");
        }

        return (0);
}
