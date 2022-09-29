/*
 * os/numa/fbck_policy_uma.c
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


int fbck_policy_uma_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
static char* fbck_policy_uma_name(void);
static void fbck_policy_uma_destroy(pm_t* pm);
static pfd_t* fbck_policy_uma_fallback(struct pm* pm,
                                          uint ckey,
                                          int flags,
                                          mld_t* mld,
                                          size_t* ppsz);
static int fbck_policy_uma_fbck_srvc(struct pm* pm,
                                     pm_srvc_t srvc,
                                     void* args,
                                     ... );


/*
 * The FallbackUma initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
fbck_policy_uma_init()
{
        pmfactory_export(fbck_policy_uma_create, fbck_policy_uma_name());

        return (0);
}

/*
 * The constructor for FallbackUma takes no arguments
 */

/*ARGSUSED*/
int
fbck_policy_uma_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns)
{
        ASSERT(pm);
        ASSERT(pmo_ns);
        
        pm->fallback = fbck_policy_uma_fallback;
        pm->fbck_srvc = fbck_policy_uma_fbck_srvc;
        pm->fbck_data = 0;
        
        return (0);
}

/*ARGSUSED*/
static char*
fbck_policy_uma_name(void)
{
        return ("FallbackDefault");
}

/*ARGSUSED*/
static void
fbck_policy_uma_destroy(pm_t* pm)
{
        ASSERT(pm);
}


/*
 * We give preference to large pages on other nodes before 
 * allocating base pages from the same node.
 */
/*ARGSUSED*/
static pfd_t*
fbck_policy_uma_fallback(struct pm* pm,
                             uint ckey,
                             int flags,
                             mld_t* mld,
                             size_t* ppsz)
{
        pfd_t* pfd;
	size_t	page_size;

        ASSERT(pm);
        ASSERT(ppsz);

	page_size = *ppsz;
        
	while (page_size >= _PAGESZ) {

		pfd = pagealloc_size(ckey, flags, *ppsz);
		if (pfd != NULL)
			return(pfd);
		if (page_size > NBPP) {
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
fbck_policy_uma_fbck_srvc(struct pm* pm, pm_srvc_t srvc, void* args, ...)
{
        ASSERT(pm);
        
        switch (srvc) {
        case PM_SRVC_DESTROY:
                fbck_policy_uma_destroy(pm);
                return (0);

        case PM_SRVC_GETNAME:
		*(char **)args = fbck_policy_uma_name();
                return (0);

        case PM_SRVC_GETAFF:
                return (0);

	case PM_SRVC_GETARG:
		*(int *)args = 0;
		return (0);

        default:
                cmn_err_tag(106,CE_PANIC, "[fbck_policy_uma_srvc]: Invalid service type");
        }

        return (0);
}
