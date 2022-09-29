/*
 * os/numa/migration_user.c
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
#include <ksys/as.h>
#include <os/as/as_private.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <os/as/pmap.h>
#include <sys/nodepda.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <sys/lpage.h>
#include <ksys/migr.h>
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
#include "migr_user.h"
#include "numa_stats.h"
#include "pfms.h"
#include "migr_control.h"
#include "migr_queue.h"
#include "migr_states.h"
#include "migr_engine.h"
#include "numa_hw.h"
#include "migr_refcnt.h"


/*
 * The migration interface.
 * 1) Users can migrate ranges of vpages to a destination specifiable
 *    via an mld handle or an mldset.
 *    The migration request will be processed by the Migration Policy
 *    Module, who will decide whether to migrate the page right now,
 *    or put the page in a migration queue for later migration.
 *    [For later]:
 *    On Demand: The page is invalidated, and the destination mld
 *    nodeid is placed in the pte for this page. When it's
 *    referenced again, the page is migrated to the node specified
 *    int the pte (Not yet).
 * 2) Users can freeze ranges of pages.
 * 3) users can unfreeze ranges of pages
 */

static int
migr_user_page_migrate(pfn_t old_pfn, cnodeid_t destination_node);
static int
migr_user_range_migrate(caddr_t vaddr, size_t len, void* pmo);


#if _MIPS_SIM == _ABI64

/*ARGSUSED*/
static int
irix5_to_vrange(
    enum xlate_mode mode,
    void *to,
    int count,
    xlate_info_t *info)
{
    COPYIN_XLATE_PROLOGUE(irix5_vrange, vrange);
    target->base_addr = (caddr_t)(__psint_t)(int)source->base_addr;
    target->length = source->length;
    return 0;
}

#endif

/*
 * The handle can either be an MLD or an MLDSET
 */
int
migr_xface_migrate(vrange_t* vrange_arg, pmo_handle_t handle)
{
        vrange_t vrange;
        void* pmo;
        int errcode;
        caddr_t vaddr;
        caddr_t end;
        size_t len;

#ifdef DEBUG_USERMIGR        
        printf("Xface_migrate\n");
#endif /* DEBUG_USERMIGR */        
        
        if (COPYIN_XLATE((void*)vrange_arg,
                         (void*)&vrange,
                         sizeof(vrange_t),
                         irix5_to_vrange,
                         get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        pmo = pmo_ns_find(curpmo_ns(), handle, __PMO_ANY);

        if (pmo == NULL) {
                 return (PMOERR_INV_PM_HANDLE);
        }

        if (pmo_gettype(pmo) != __PMO_MLD && pmo_gettype(pmo) != __PMO_MLDSET) {
                pmo_decref(pmo, pmo_ref_find);
                return (PMOERR_INV_PM_HANDLE);
        }

#ifdef DEBUG_USERMIGR                
        printf("calling migr_user....\n");
#endif /* DEBUG_USERMIGR */

        vaddr = (caddr_t)ctob(btoct(vrange.base_addr));
        end = (caddr_t)ctob(btoc((vrange.base_addr + vrange.length)));
        len = end - vaddr;
        
        errcode = migr_user_range_migrate(vaddr, len, pmo);
        
        pmo_decref(pmo, pmo_ref_find);

        return (errcode);
}


/*ARGSUSED*/
int
migr_xface_freeze(vrange_t* vrange_arg)
{
        return (0);
}

/*ARGSUSED*/
int
migr_xface_unfreeze(vrange_t* vrange_arg)
{
        return (0);
}

/******************************************************************
 *                User initiated migration methods                *
 ******************************************************************/


#if _MIPS_SIM == _ABI64
static int
is_kuseg_abi(int abi, caddr_t addr)
{
	if (ABI_IS_64BIT(abi))
		return IS_KUSEG64(addr);
	else
		return IS_KUSEG32(addr);
}
#else
#define	is_kuseg_abi(abi, addr)		(IS_KUSEG(addr))
#endif

static int
migr_user_range_migrate(caddr_t vaddr, size_t len, void* pmo)
{
        preg_t* prp;
        reg_t*  rp;
        int abi;
        __psunsigned_t kusize;
        int errcode;
        pgno_t total_npages;
        pgno_t region_npages;
        pgno_t extent_npages;
        pgno_t region_allpages;
        caddr_t region_end_vaddr;
	/*REFERENCED*/
        int migr_code;
        cnodeid_t destination_node;
        pde_t* pd;
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;

        numa_message(DM_MIGR,
                     2,
                     "[migr_user_range_migrate]: vaddr, len", (long long)vaddr, len);

#ifdef DEBUG_USERMIGR        
        printf("[migr_user_range_migrate]: vaddr, len 0x%x, 0x%x\n", vaddr, len);
#endif /* DEBUG_USERMIGR */        

	abi = get_current_abi();
	kusize = ABI_IS_64BIT(abi) ? KUSIZE_64 : KUSIZE_32;
	if (!(IS_KUSEG(vaddr)) || !(is_kuseg_abi(abi, vaddr + MIN(len, kusize) - 1))) {
		return (PMOERR_EINVAL);
        }

        switch (pmo_gettype(pmo)) {
        case __PMO_MLD:
        {
                /*
                 * If the destination pmo is an MLD, then we
                 * know what node to migrate to.
                 * NOTES:
                 * -- User  migration requests also go through the
                 *    migration controller.
                 * -- If the migration controller decides not
                 *    to migrate the page, for now we'll just
                 *    add it to the integrator queue.
                 */

                destination_node = mld_getnodeid((mld_t*)pmo);
                break;
        }
        case __PMO_MLDSET:
        {
                /*
                 * We are given the option here to migrate to any
                 * node in the mldset.
                 * We can choose based on current available memory
                 * and current traffic.
                 */
                mld_t* mld = (mld_t*)pmolist_getelem(mldset_getmldlist((mldset_t*)pmo), 0);
                destination_node = mld_getnodeid(mld);
                break;
        }
                
        default:
                cmn_err(CE_PANIC, "[migr_xface_migrate]: invalid dest");
        }

        numa_message(DM_MIGR,
                     2,
                     "[migr_user_range_migrate]: dest_node, dest_node",
                     destination_node, destination_node);

#ifdef DEBUG_USERMIGR       
        printf("Destination node: 0x%x\n", destination_node);
#endif /* DEBUG_USERMIGR */
        
	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
        mraccess(&pas->pas_aspacelock);

        total_npages = btoc(len);

#ifdef DEBUG_USERMIGR       
        printf("Total Pages: 0x%x\n", total_npages);
#endif /* DEBUG_USERMIGR */
        
        while (total_npages) {
		/*
		 * Go through the address range a region at a time
		 */
		if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
#ifdef DEBUG_USERMIGR
                        extern idbg_pregion(__psint_t p1);
                        printf("[migr_user_range_migrate]: PMOERR_NOMEM, vaddr: 0x%x\n", vaddr);
                        idbg_pregion(-1);
#endif /* DEBUG_USERMIGR */                        
			errcode = PMOERR_NOMEM;
			goto out;
		}

		rp = prp->p_reg;

                region_npages  = MIN(total_npages, pregpgs(prp, vaddr));
                region_end_vaddr = vaddr + ctob(region_npages);
                region_allpages = region_npages;

                numa_message(DM_MIGR,
                             2,
                             "[migr_user_range_migrate]: reg, npages",
                             (long long)rp, region_allpages);

                reglock(rp);

                while (region_npages) {
                        pd = pmap_probe(prp->p_pmap,
                                        &vaddr,
                                        &region_npages,
                                        &extent_npages);

                        vaddr += ctob(extent_npages);
                        region_npages -= extent_npages;

                        numa_message(DM_MIGR,
                                     2,
                                     "[migr_user_range_migrate]: ext_vaddr, npages",
                                     (long long)vaddr, extent_npages);
                        
                        while (extent_npages > 0) {
                                pfn_t pfn = pg_getpfn(pd);
                                if ((pg_isvalid(pd)) && (destination_node != PFN_TO_CNODEID(pfn))) {
#ifdef DEBUG_USERMIGR                                               
                                        printf("Migrating pfn 0x%x to node 0x%x\n",
                                               pfn, destination_node);
#endif /* DEBUG_USERMIGR */                                        
                                        migr_code = migr_user_page_migrate(pfn, destination_node);
                                        numa_message(DM_MIGR,
                                                     2,
                                                     "[migr_user_range_migrate]:p r",
                                                     pfn, migr_code);
                                } else {
#ifdef DEBUG_USERMIGR                                               
                                        printf("REjected pfn 0x%x to node 0x%x\n",
                                                pfn, destination_node);
#endif /* DEBUG_USERMIGR */                                        
                                }
                                
                                extent_npages --; 
                                pd++;
                        }
                }

                regrele(rp);
                vaddr = region_end_vaddr;
                total_npages -= region_allpages;
        }

        errcode = 0;

out:
        mrunlock(&pas->pas_aspacelock);
        return (errcode);
}
        
static int
migr_user_page_migrate(pfn_t old_pfn, cnodeid_t destination_node)
{
        cnodeid_t source_node;
        int pfms_spl;
        pfn_t dest_pfn;
        pfms_t pfms;
        pfmsv_t pfmsv;
        int ret;
        rqmode_t rqmode = RQMODE_ADVISORY;

        source_node = PFN_TO_CNODEID(old_pfn);

        pfms = PFN_TO_PFMS(old_pfn);
        PFMS_LOCK(pfms, pfms_spl);
        pfmsv = PFMS_GETV(pfms);

        if (!PFMSV_STATE_IS_MAPPED(pfmsv)) {
                PFMS_UNLOCK(pfms, pfms_spl);
                NUMA_STAT_INCR(source_node, migr_interrupt_failstate);
                NUMA_STAT_INCR(source_node, migr_user_number_fail); 
                return (PMOERR_EBUSY);
        }


        PFMS_STATE_MIGRED_SET(pfms);
        PFMS_UNLOCK(pfms, pfms_spl);

        migr_refcnt_update_counters(source_node,
                                    old_pfn,
                                    pfmsv,
                                    MD_PROT_MIGMD_OFF);       

        
        /*
         * We migrate the page using the currently selected mechanism:
         * Either enqueued or immediate migration.
         */
       
        
        if (NODEPDA_MCD(source_node)->migr_system_kparms.migr_queue_control_enabled &&
            NODEPDA_MCD(source_node)->migr_system_kparms.migr_user_migr_mech == MIGR_MECH_ENQUEUE) {
               
                migr_queue_insert(NODEPDA_MCD(source_node)->migr_queue,
                                  pfms,
                                  old_pfn, 
                                  destination_node);
 
                NUMA_STAT_INCR(source_node, migr_user_number_queued);

                return (0);
        }

	if (((ret = migr_migrate_page(old_pfn, destination_node, &dest_pfn)) != 0)) { 
		
		/*
		 * Not successful -
                 * The pfms state has not been updated. We need to
                 * take care of it.
		 */

		if (NODEPDA_MCD(source_node)->migr_system_kparms.migr_queue_control_enabled &&
                    rqmode == RQMODE_ADVISORY &&
		    (ret != MIGRERR_ZEROLINKS)) {

			/* 
			 * queue the page - the method takes care of the pfms state
			 */

			migr_queue_insert(NODEPDA_MCD(source_node)->migr_queue,
                                          pfms,
                                          old_pfn,
                                          destination_node);

                        return (0);

		} else {
                        ASSERT(dest_pfn == old_pfn);
                        ASSERT(pfms == PFN_TO_PFMS(old_pfn));
                        migr_restart(old_pfn, pfms);

                        NUMA_STAT_INCR(source_node, migr_user_number_fail);

                        return (PMOERR_EBUSY);
		}

	}
        

        /*
         * On success
         */
        ASSERT(dest_pfn != old_pfn);
        migr_restart_clear(dest_pfn, PFN_TO_PFMS(dest_pfn));
        NUMA_STAT_INCR(source_node, migr_user_number_out);
        NUMA_STAT_INCR(destination_node, migr_user_number_in);

        return (0);


 
}

