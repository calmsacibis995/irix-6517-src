/*
 * os/numa/plac_policy_default.c
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
#include <sys/nodemask.h>
#include <sys/mmci.h>
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
#include "memfit.h"
/*
 * Private PlacementDefault definitions
 */
typedef struct plac_policy_default_data {
        ushort nthreads;
} plac_policy_default_data_t;

static int plac_policy_default_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns);
static void plac_policy_default_destroy(pm_t* pm);
static void plac_policy_default_afflink(struct pm* pm,
                                        aff_caller_t aff_caller,
                                        kthread_t* ckt,
                                        kthread_t* pkt);
static char* plac_policy_default_name(void);
static pfd_t* plac_policy_default_pagealloc(struct pm* pm,
                                            uint ckey,
                                            int flags,
                                            size_t* ppsz,
                                            caddr_t vaddr);
static int plac_policy_default_plac_srvc(struct pm* pm,
                                         pm_srvc_t srvc,
                                         void* args, ...);

/*
 * The dynamic memory allocation zone for plac_policy_default child data
 */
static zone_t* plac_policy_default_data_zone = 0;

/*
 * The PlacementDefault initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
plac_policy_default_init()
{
        ASSERT(plac_policy_default_data_zone == 0);
        plac_policy_default_data_zone =
                kmem_zone_init(sizeof(plac_policy_default_data_t),
                               "plac_policy_default_data");
        ASSERT(plac_policy_default_data_zone);

        pmfactory_export(plac_policy_default_create, plac_policy_default_name());

        return (0);
}

/*
 * The constructor for PlacementDefault takes one argument:
 * -- number of threads that will be using this addr space
 */

/*ARGSUSED*/
static int
plac_policy_default_create(pm_t* pm, void* args, pmo_ns_t* pmo_ns)
{
        int nthreads;
        /*REFERENCED*/
        raff_t raff;
        void* pmo;
	/*REFERENCED*/
        pmo_handle_t handle;
        mld_t* mld;

        ASSERT(plac_policy_default_data_zone);
        ASSERT(pm);
        ASSERT(pmo_ns);
        
        nthreads = (int)(__uint64_t)args;
        if (nthreads <= 0) {
                return (PMOERR_EINVAL);
        }
        
        /*
         * Placement Policy constructors have to set
         * the following methods:
         * -- pagealloc
         * -- afflink
         * -- plac_srvc
         * The placement policy private object data can be attached
         * to plac_data.
         */

        pm->pagealloc = plac_policy_default_pagealloc;
        pm->afflink = plac_policy_default_afflink;
        pm->plac_srvc = plac_policy_default_plac_srvc;

        /*
         * For the nthreads < THREADS_PER_MLD we just create and
         * place one single mld.
         *
         * For multihreaded cases (nthreads > THREADS_PER_MLD) we create an mldset,
         * and place it.
         *
         * Normally we don't really know the number of threads an
         * application will end up using. Therefore, we have to add
         * some dynamicity to this default policy so that it can adjust
         * to apps that start growing the number of threads.
         * We can do this by relying on the afflink method calls
         * invoked everytime a process forks or sprocs.
         */
	if (pm->pmo)
		pmo = pm->pmo;

        else if (nthreads <= THREADS_PER_MLD ) {
                cnodeid_t bestnode;
                handle = mld_create_and_export(0, MLD_DEFAULTSIZE, pmo_ns, &mld, -1);
                if (pmo_iserrorhandle(handle)) {
                        return (PMOERR_E2BIG);
                }

                ASSERT(pm->repl_srvc);

                bestnode = memfit_selectnode(curthreadp->k_nodemask);
		if (bestnode == CNODEID_NONE)
			bestnode = (cnodeid_t)( random() % numnodes );
                physmem_mld_place_onnode(mld, bestnode);
                mld_setweak(mld);

                pmo = (void*)mld;
        } else {
                pmolist_t* mldlist;
                mldset_t* mldset;
                int nmlds;
                int i;
                /*REFERENCED*/
                pmolist_t rafflist;
                
                /*
                 * For the current sn0 architecture, we create one
                 * mld for every two threads.
                 */
                nmlds = nthreads / THREADS_PER_MLD;
                
                /*
                 * Create mlds and insert them into a pmolist
                 */
                mldlist = pmolist_create(nmlds);
                ASSERT(mldlist);
                
                for (i = 0; i < nmlds; i++) {
                        handle = mld_create_and_export(1, MLD_DEFAULTSIZE, pmo_ns, &mld, -1);
                        if (pmo_iserrorhandle(handle)) {
                                pmolist_destroy(mldlist, pmo_ref_list);
                                return (PMOERR_E2BIG);
                        }
                        pmolist_insert(mldlist, mld, pmo_ref_list);
                }

                /*
                 * Get affinity info from the replication policy module
                 */
                ASSERT(pm->repl_srvc);
                /*
                 * REPL is not interfacing right for now....
                 *  (*pm->repl_srvc)(pm, PM_SRVC_GETAFF, &raff);
                 *  pmolist_setup(&rafflist, &raff);
                 */ 

                handle = pmo_kernel_mldset_create_export_and_place(TOPOLOGY_FREE,
                                                                   mldlist,
                                                                   NULL,
                                                                   RQMODE_ADVISORY,
                                                                   pmo_ns,
                                                                   &mldset);
                pmo = (void*)mldset;
        }

        /*
         * Allocate and initialize memory for the private PlacPol data
         */
        pm->plac_data = kmem_zone_alloc(plac_policy_default_data_zone, KM_SLEEP);
        ASSERT(pm->plac_data);
        ((plac_policy_default_data_t*)(pm->plac_data))->nthreads = nthreads;

        /*
         * Acquire a reference on the mld/mldset we're going to link
         * to this pm.
         */
        pmo_incref(pmo, pm);
        pm->pmo = pmo;

        return (0);
}

/*ARGSUSED*/
static char*
plac_policy_default_name(void)
{
        return ("PlacementDefault");
}

static void
plac_policy_default_destroy(pm_t* pm)
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
		kmem_zone_free(plac_policy_default_data_zone, pm->plac_data);
	}
}

/*ARGSUSED*/
static pfd_t*
plac_policy_default_pagealloc(struct pm* pm,
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
plac_policy_default_afflink(struct pm* pm,
                            aff_caller_t aff_caller,
                            kthread_t* ckt,
                            kthread_t* pkt)
{
        
        ASSERT(pm);
        ASSERT(pm->pmo);
        ASSERT(ckt);
        
        switch (aff_caller) {
        case AFFLINK_NEWPROC_SHARED:     /* sproc with shared address space */
        case AFFLINK_NEWPROC_PRIVATE:    /* fork, or private sproc */         
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

                ASSERT(pkt); pkt=pkt;

                pm_mrupdate(pm);
                
                nthreads = ++(((plac_policy_default_data_t*)(pm->plac_data))->nthreads);

                if (nthreads == 0) {
                        ((plac_policy_default_data_t*)(pm->plac_data))->nthreads = numnodes;
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
                                        physmem_mld_place(new_mld,
                                                          mld_getnodeid(old_mld),  /* center around old mld */
                                                          CNODEMASK_CVTB(mld_getnodeid(old_mld))); /* skip old mld's node */ 
                                
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

                                        physmem_mld_place(new_mld,
                                                          mld_getnodeid(old_mld),
                                                          mldset_getmldbv(mldset));

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
                mldset_t* mldset;
                pmolist_t* mldlist;
                
                /*
                 * We need to select a preferred mld from the newly created
                 * mldset.
                 */

                pm_mraccess(pm);
                
                if (pmo_gettype(pm->pmo) == __PMO_MLD) {
                        mld = (mld_t*)pm->pmo;
                } else {
                        mldset = (mldset_t*)pm->pmo;
                        ASSERT(mldset);
                        mldset_acclock(mldset);
                        mldlist = mldset_getmldlist(mldset);
                        ASSERT(mldlist);
                        mld = (mld_t*)pmolist_getelem(mldlist, 0);
                        mldset_accunlock(mldset);
                }
                ASSERT(mld);
                aspm_proc_affset(ckt, mld, mld_to_bv(mld));

                pm_mraccunlock(pm);
                break;
        }
        default:
                cmn_err(CE_PANIC,"[plac_policy_default_afflink]: Invalid caller type");
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
plac_policy_default_plac_srvc(struct pm* pm, pm_srvc_t srvc, void* args, ...)
{
        ASSERT(pm);

        switch (srvc) {
        case PM_SRVC_DESTROY:
                plac_policy_default_destroy(pm);
                return (0);

        case PM_SRVC_GETNAME:
		*(char **)args = plac_policy_default_name();
                return (0);

        case PM_SRVC_GETAFF:
                return (0);

        case PM_SRVC_SYNC_POLICY:
		return (plac_policy_sync(pm, (pm_setas_t *)args));

	case PM_SRVC_GETARG:
		*(int *)args = ((plac_policy_default_data_t*)(pm->plac_data))->nthreads;
		return (0);

        case PM_SRVC_ATTACH:
                return (0);

	case PM_SRVC_DECUSG:
		((plac_policy_default_data_t*)(pm->plac_data))->nthreads--;
		return (0);


        default:
                cmn_err(CE_PANIC, "[plac_policy_default_srvc]: Invalid service type");
        }

        return (0);
}

