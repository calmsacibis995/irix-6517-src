/*
 * os/numa/plac_policy_fixed.c
 *
 *
 * Copyright 1995, 1996 Silicon Graphics, Inc.
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
#include "pmo_base.h"
#include "pmo_error.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "memsched.h"
#include "numa_init.h"
#include "pm_policy_common.h"

static int plac_policy_fixed_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
static char* plac_policy_fixed_name(void);
static void plac_policy_fixed_destroy(pm_t* pm);
static pfd_t* plac_policy_fixed_pagealloc(struct pm* pm,
                                          uint ckey,
                                          int flags,
                                          size_t* ppsz,
                                          caddr_t vaddr);
static void plac_policy_fixed_afflink(struct pm* pm,
                                      aff_caller_t aff_caller,
                                      kthread_t* ckt,
                                      kthread_t* pkt);
static int plac_policy_fixed_plac_srvc(struct pm* pm,
                                       pm_srvc_t srvc,
                                       void* args, ...);

/*
 * The PlacementFixed initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
plac_policy_fixed_init()
{
        pmfactory_export(plac_policy_fixed_create, plac_policy_fixed_name());

        return (0);
}

/*
 * The constructor for PlacementFixed takes one argument:
 * -- mld handle to allocate memory from
 */

/*ARGSUSED*/
static int
plac_policy_fixed_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns)
{
        mld_t* mld;
        pmo_handle_t mld_handle = (pmo_handle_t)(__uint64_t)args;

        ASSERT(pm);
        ASSERT(pmo_ns);
        
	if (pm->pmo) {
		/*
		 * CPR restart case
		 */
		if (pmo_gettype(pm->pmo) != __PMO_MLD)
			return (PMOERR_EINVAL);

		mld = pm->pmo;
		pmo_incref(mld, pmo_ref_find);

	} else {
        	mld = (mld_t*)pmo_ns_find(pmo_ns, mld_handle, __PMO_MLD);
		if (mld == NULL)
                	return (PMOERR_ENOENT);
        }
        /*
         * Has the mld been placed?
         */
        if (!mld_isplaced(mld)) {
                pmo_decref(mld, pmo_ref_find);
                return (PMOERR_EINVAL);
        }
        
        pm->pmo = (void*)mld;
        

        /*
         * Incr ref for reference from pm, release ref acquired
         * in find. DOne this way to keep debugging trace sane.
         */
        pmo_incref(mld, pm);
        pmo_decref(mld, pmo_ref_find);

        /*
         * Placement Policy constructors have to set
         * the following methods:
         * -- pagealloc
         * -- afflink
         * -- plac_srvc
         * The placement policy private object data can be attached
         * to plac_data.
         */

        pm->pagealloc = plac_policy_fixed_pagealloc;
        pm->afflink = plac_policy_fixed_afflink;
        pm->plac_srvc = plac_policy_fixed_plac_srvc;

        return (0);
}

/*ARGSUSED*/
static char*
plac_policy_fixed_name(void)
{
        return ("PlacementFixed");
}

static void
plac_policy_fixed_destroy(pm_t* pm)
{
        ASSERT(pm);

        /*
         * Release the reference on the pmo object we're using.
         */
	if (pm->pmo) {
		pmo_decref(pm->pmo, pm);
	}
}

/*ARGSUSED*/
static pfd_t*
plac_policy_fixed_pagealloc(struct pm* pm,
                            uint ckey,
                            int flags,
                            size_t* ppsz,
                            caddr_t vaddr)
{
        cnodeid_t node;
        pfd_t* pfd;
        mld_t* mld;
        
        ASSERT(pm);
        ASSERT(pm->pmo);
        ASSERT(ppsz);

        ASSERT(pmo_gettype(pm->pmo) == __PMO_MLD);
        mld = (mld_t*)pm->pmo;
        node = mld_getnodeid(mld);
        ASSERT(node >= 0);

	if (pm->policy_flags & POLICY_CACHE_COLOR_FIRST)
                flags |= VM_PACOLOR;

	if (*ppsz == PM_PAGESIZE) {
		pfd = pagealloc_size_node(node, ckey, flags, pm->page_size);
		*ppsz = pm->page_size;
	} else {
		ASSERT(valid_page_size(*ppsz));
		pfd = pagealloc_size_node(node, ckey, flags, *ppsz);
	}

        if (pfd == NULL) {
                pfd = (*pm->fallback)(pm, ckey, flags, mld, ppsz);
		if ((flags & VM_PACOLOR) && (pfd == NULL)) {
                        flags &= ~VM_PACOLOR;
                        pfd = (*pm->fallback)(pm, ckey, flags, mld, ppsz);
                }
        }

        return (pfd);
}

/*
 * This method sets the affinity fields in the proc structure for the
 * child process `cp'. We can also dynamically change the placement policy
 * defined by this pm according to the calls to this method, which is called
 * every time a process forks (sprocs) or execs.
 */
/* ARGSUSED */
static void
plac_policy_fixed_afflink(struct pm* pm,
                          aff_caller_t aff_caller,
                          kthread_t* ckt,
                          kthread_t* pkt)
{
        ASSERT(pm);
        ASSERT(pm->pmo);
        ASSERT(ckt);
        
        switch (aff_caller) {
        case AFFLINK_NEWPROC_SHARED:     /* sproc with shared address space */
        case AFFLINK_NEWPROC_PRIVATE:    /* fork, or privated sproc */
        {
                /*
                 * Since it's a "fixed" policy, we'll just link
                 * all related procs/threads to the mld in the pm.
                 */
                mld_t* mld;
                ASSERT(pmo_gettype(pm->pmo) == __PMO_MLD);
                mld = (mld_t*)pm->pmo;
                aspm_proc_affset(ckt, mld, mld_to_bv(mld));
                break;
        }
                
        case AFFLINK_EXEC:               /* exec */
        {
                mld_t* mld;
                ASSERT(pmo_gettype(pm->pmo) == __PMO_MLD);
                mld = (mld_t*)pm->pmo;
                aspm_proc_affset(ckt, mld, mld_to_bv(mld));
                break;
        }
        default:
                cmn_err(CE_PANIC,"[plac_policy_fixed_afflink]: Invalid caller type");
        }
}

/*
 * This method provides generic services for this module:
 * -- destructor,
 * -- getname,
 * -- getaff (not valid for this placement module).
 */
/*ARGSUSED*/
static int
plac_policy_fixed_plac_srvc(struct pm* pm, pm_srvc_t srvc, void* args, ...)
{
        ASSERT(pm);
        
        switch (srvc) {
        case PM_SRVC_DESTROY:
                plac_policy_fixed_destroy(pm);
                return (0);

        case PM_SRVC_GETNAME:
		*(char **)args = plac_policy_fixed_name();
                return (0);

        case PM_SRVC_GETAFF:
                return (0);

	case PM_SRVC_SYNC_POLICY:
                return (plac_policy_sync(pm, (pm_setas_t *)args));

	case PM_SRVC_GETARG:
        {
                va_list ap; 
                pmo_ns_t* ns;

                va_start(ap, args);
                ns = va_arg(ap, pmo_ns_t*);
                va_end(ap);
		*((int *)args) = pmo_ns_pmo_lookup(ns, __PMO_MLD, pm->pmo);
		return (0);
        }

        case PM_SRVC_ATTACH:
                return (0);

        case PM_SRVC_DECUSG:
                return (0);

        default:
                cmn_err(CE_PANIC, "[plac_policy_fixed_srvc]: Invalid service type");
        }

        return (0);
}

                
