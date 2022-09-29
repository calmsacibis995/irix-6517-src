/*
 * os/numa/plac_policy_firsttouch.c
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

/*
 * Private PlacementFirstTouch definitions
 */
typedef struct plac_policy_firsttouch_data {
        ushort nthreads;
} plac_policy_firsttouch_data_t;

static int plac_policy_firsttouch_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
static char* plac_policy_firsttouch_name(void);
static void plac_policy_firsttouch_destroy(pm_t* pm);
static pfd_t* plac_policy_firsttouch_pagealloc(struct pm* pm,
                                               uint ckey,
                                               int flags,
                                               size_t* ppsz,
                                               caddr_t vaddr);
static void plac_policy_firsttouch_afflink(struct pm* pm,
                                           aff_caller_t aff_caller,
                                           kthread_t* ckt,
                                           kthread_t* pkt);
static int plac_policy_firsttouch_plac_srvc(struct pm* pm,
                                            pm_srvc_t srvc,
                                            void* args, ...);

/*
 * The dynamic memory allocation zone for plac_policy_firsttouch child data
 */
static zone_t* plac_policy_firsttouch_data_zone = 0;

/*
 * The PlacementFirstTouch initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
plac_policy_firsttouch_init()
{
        ASSERT(plac_policy_firsttouch_data_zone == 0);
        plac_policy_firsttouch_data_zone =
                kmem_zone_init(sizeof(plac_policy_firsttouch_data_t),
                               "plac_policy_firsttouch_data");
        ASSERT(plac_policy_firsttouch_data_zone);

        
        pmfactory_export(plac_policy_firsttouch_create, plac_policy_firsttouch_name());

        return (0);
}

/*
 * The constructor for PlacementFirstTouch takes no args.
 */

/*ARGSUSED*/
static int
plac_policy_firsttouch_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns)
{
        mld_t* mld;
	/*REFERENCED*/
        pmo_handle_t handle;

        ASSERT(plac_policy_firsttouch_data_zone);
        ASSERT(pm);
        ASSERT(pmo_ns);

	if ((mld = pm->pmo)) {
		if (pmo_gettype(mld) != __PMO_MLD)
			return (PMOERR_EINVAL);
        } else {
		handle = mld_create_and_export(0, MLD_DEFAULTSIZE, pmo_ns, &mld, -1);
	        if (pmo_iserrorhandle(handle)) {
	                return (PMOERR_E2BIG);
	        }
	        
	        physmem_mld_place_here(mld);
	}
        pmo_incref(mld, pm);
        pm->pmo = (void*)mld;
        
        /*
         * Placement Policy constructors have to set
         * the following methods:
         * -- pagealloc
         * -- afflink
         * -- plac_srvc
         * The placement policy private object data can be attached
         * to plac_data.
         */

        pm->pagealloc = plac_policy_firsttouch_pagealloc;
        pm->afflink = plac_policy_firsttouch_afflink;
        pm->plac_srvc = plac_policy_firsttouch_plac_srvc;

        /*
         * Allocate and initialize memory for the private PlacPol data
         */
        pm->plac_data = kmem_zone_alloc(plac_policy_firsttouch_data_zone, KM_SLEEP);
        ASSERT(pm->plac_data);
        ((plac_policy_firsttouch_data_t*)(pm->plac_data))->nthreads = 1;

        
        return (0);
}

/*ARGSUSED*/
static char*
plac_policy_firsttouch_name(void)
{
        return ("PlacementFirstTouch");
}

static void
plac_policy_firsttouch_destroy(pm_t* pm)
{
        ASSERT(pm);

        /*
         * Release the reference on the pmo object we're using.
         */
	if (pm->pmo) {
		pmo_decref(pm->pmo, pm);
	}

        /*
         * Free the plac_data memory
         */
	if (pm->plac_data) {
		kmem_zone_free(plac_policy_firsttouch_data_zone,
							pm->plac_data);
	}
}

/*ARGSUSED*/
static pfd_t*
plac_policy_firsttouch_pagealloc(struct pm* pm,
                                 uint ckey,
                                 int flags,
                                 size_t* ppsz,
                                 caddr_t vaddr)
{
        cnodeid_t node;
        pfd_t* pfd;
        mld_t* mld;
        
        ASSERT(pm);
        ASSERT(pm->plac_data);
        ASSERT(pm->pmo);
        ASSERT(ppsz);

        /*
         * Allocate memory from the mld we have set for this
         * process to be the "affinity link".
         */

        mld = UT_TO_KT(private.p_curuthread)->k_mldlink;
        
        ASSERT(mld);
        ASSERT(mld_isplaced(mld));
        
        node = mld_getnodeid(mld);

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
static void
plac_policy_firsttouch_afflink(struct pm* pm,
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
                int nthreads;
		/*REFERENCED*/
                pmo_handle_t handle;
                mld_t* old_mld;
                mld_t* new_mld;
                pmolist_t* mldlist;
                mldset_t* mldset;
                
                /*
                 * Case 1: We have an mld.
                 *         Case 1.a: This is only the second sproc
                 *                   and the node where we are has two
                 *                   cpus. We just set the aff
                 *                   link to point to this mld.
                 *         Case 1.b: This is the third process,
                 *                   we need to allocate another mld, and create
                 *                   an mldset to contain both the old and the new mlds.
                 *                   We set the aff link to the new mld.
                 *         
                 * Case 2: We have an mldset
                 *         Case 2.a: This is an even process
                 *                   Just use the last created mld
                 *                   to set the aff link.
                 *         Case 2.b: This is an odd process
                 *                   We need a new mld; we get this
                 *                   new mld by growing our mldset.
                 *                   We set the link to the new mld.
                 */

                /*
                 * XXXXXXXXXXXXXXXXXXX
                 * We still need to check for cpu availability.
                 */
                ASSERT(pkt); pkt=pkt;

                pm_mrupdate(pm);
                
                nthreads = ++(((plac_policy_firsttouch_data_t*)(pm->plac_data))->nthreads);

                if (nthreads == 0) {
                        ((plac_policy_firsttouch_data_t*)(pm->plac_data))->nthreads = numnodes;
                        nthreads = numnodes;
                }
                
                if (pmo_gettype(pm->pmo) == __PMO_MLD) {

                        /*
                         * We have an mld
                         */
                        
                        old_mld = (mld_t*)pm->pmo;
                        mld_setunweak(old_mld);

                        /*
                         * If we just have one node, avoid creating any more MLDs
                         */
                        
                        if (nthreads <= THREADS_PER_MLD || numnodes <= 1) {
                                aspm_proc_affset(ckt, old_mld, mld_to_bv(old_mld));
                        } else {
                                /*
                                 * We can't link any more threads to the
                                 * only mld we currently have, we need
                                 * to create a new mld.
                                 */
                                handle = mld_create_and_export(0,
                                                               MLD_DEFAULTSIZE,
                                                               curpmo_ns(),
                                                               &new_mld,
							       -1);
                                if (pmo_iserrorhandle(handle)) {
                                        aspm_proc_affset(ckt, old_mld, mld_to_bv(old_mld));
                                } else {

                                
                                        physmem_mld_place_here(new_mld);

                                
                                        /*
                                         * Create a pmolist as a container for the
                                         * new mlds. We make the size of the list be
                                         * 8, so that we can accomodate a max of 8 mlds
                                         * without having to re-allocate the pmolist.
                                         */
                                        mldlist = pmolist_create(8);
                                        ASSERT(mldlist);

                                        /*
                                         * Insert old mld into list
                                         */
                                        pmolist_insert(mldlist, old_mld, pmo_ref_list);
                                        /*
                                         * Insert new mld into list
                                         */
                                        pmolist_insert(mldlist, new_mld, pmo_ref_list);

                                        /*
                                         * create mldset
                                         */
                                        handle = mldset_create_and_export(mldlist,
                                                                          curpmo_ns(),
                                                                          &mldset,
                                                                          -1);
                                        if (pmo_iserrorhandle(handle)) {
                                                aspm_proc_affset(ckt, new_mld, mld_to_bv(new_mld));
                                        } else {

                                                /*  HACK WARNING!!!! */
                                                CNODEMASK_SETB(mldset->mldbv, mld_getnodeid(old_mld));
                                                CNODEMASK_SETB(mldset->mldbv, mld_getnodeid(new_mld));

                                                pmo_decref(old_mld, pm);
                                                pmo_incref(mldset, pm);
                                                pm->pmo = (void*)mldset;

                                                aspm_proc_affset(ckt, new_mld, mldset_getmldbv(mldset));
                                        }
                                }
                        }

                } else {
                        /*
                         * We  have an mldset
                         */
                        ASSERT(pmo_gettype(pm->pmo) == __PMO_MLDSET);
                        mldset = (mldset_t*)pm->pmo;
                        ASSERT(mldset);

                        mldset_acclock(mldset);
                        mldlist = mldset_getmldlist(mldset);
                        ASSERT(mldlist);
                        old_mld = (mld_t*)pmolist_getlastelem(mldlist);
                        ASSERT(old_mld);

       
                        /*
                         * First check if we've run out of physical nodes
                         */
                        if (numnodes <= mldset_getclen(mldset)) {

                                pmo_incref(mldset, pmo_ref_keep);
                                
                                new_mld =  (mld_t*)pmolist_getelem(mldlist, (nthreads - 1) % mldset_getclen(mldset));
                                ASSERT_ALWAYS(new_mld);

                                mldset_accunlock(mldset);

                                aspm_proc_affset(ckt, new_mld, mldset_getmldbv(mldset));

                                pmo_decref(mldset, pmo_ref_keep);
                                
                        } else if (((nthreads - 1) % THREADS_PER_MLD) != 0) {
                                /*
                                 * We still have space in the last created
                                 * mld.
                                 */
                                mldset_accunlock(mldset);
                                aspm_proc_affset(ckt, old_mld, mldset_getmldbv(mldset));
                        } else {
                                /*
                                 * We can't link any more threads to the
                                 * mlds we currently have, we need
                                 * to create a new mld.
                                 */
                                mldset_accunlock(mldset);
                                handle = mld_create_and_export(0,
                                                               MLD_DEFAULTSIZE,
                                                               curpmo_ns(),
                                                               &new_mld,
							       -1);
                                if (pmo_iserrorhandle(handle)) {
                        		mldset_acclock(mldset);
                                        pmo_incref(mldset, pmo_ref_keep);
                                
                                        new_mld =  (mld_t*)pmolist_getelem(mldlist, (nthreads - 1) % mldset_getclen(mldset));
                                        ASSERT_ALWAYS(new_mld);

                                        mldset_accunlock(mldset);

                                        aspm_proc_affset(ckt, new_mld, mldset_getmldbv(mldset));

                                         pmo_decref(mldset, pmo_ref_keep);
                                } else {
                                
                                        physmem_mld_place_here(new_mld);

                                        mldset_grow(mldset, new_mld, 0);

                                        aspm_proc_affset(ckt, new_mld, mldset_getmldbv(mldset));
                                }
                        }
                        
                }

                pm_mrunlock(pm);
                
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
                cmn_err(CE_PANIC,"[plac_policy_firsttouch_afflink]: Invalid caller type");
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
plac_policy_firsttouch_plac_srvc(struct pm* pm, pm_srvc_t srvc, void* args, ...)
{
        ASSERT(pm);
        
        switch (srvc) {
        case PM_SRVC_DESTROY:
                plac_policy_firsttouch_destroy(pm);
                return (0);

        case PM_SRVC_GETNAME:
		*(char **)args = plac_policy_firsttouch_name(); 
		return (0);

        case PM_SRVC_GETAFF:
                return (0);

	case PM_SRVC_SYNC_POLICY:
                return (plac_policy_sync(pm, (pm_setas_t *)args));

	case PM_SRVC_GETARG:
		*(int *)args = 0;
		return (0);

        case PM_SRVC_ATTACH:
                return (0);

        case PM_SRVC_DECUSG:
                return (0);

        default:
                cmn_err(CE_PANIC, "[plac_policy_firsttouch_srvc]: Invalid service type");
        }

        return (0);
}

                
