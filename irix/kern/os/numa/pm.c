/*
 * os/numa/pm.c
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
#include <os/as/as_private.h> /* XXX */
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kabi.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <os/as/pmap.h>
#include <sys/mmci.h>
#include <sys/vnode.h>
#include <sys/nodepda.h>
#include <sys/lpage.h>
#include <sys/anon.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "pmo_ns.h"
#include "debug_levels.h"
#include "pmo_list.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "pmfactory_ns.h"
#include "numa_init.h"

/*
 * The dynamic memory allocation zone for pm's
 */
static zone_t* pm_zone = 0;

/*
 * The pm initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
pm_init()
{
        ASSERT(pm_zone == 0);
        pm_zone = kmem_zone_init(sizeof(pm_t), "pm");
        ASSERT(pm_zone);

        return (0);
}


/**************************************************************************
 *                          Internal PM Management                        *
 **************************************************************************/


pm_t*
pm_create(	policy_set_t* policy_set,
		pmo_ns_t* pmo_ns,
		int* perrcode,
		void* pmo)
{
        pm_t* pm;
        pmfactory_t pmfactory;
        
        ASSERT(policy_set);
        ASSERT(pmo_ns);

        pm = kmem_zone_zalloc(pm_zone, KM_SLEEP);
        ASSERT(pm);

	pm->pmo = pmo;
        if (policy_set->page_size == PM_PAGESZ_DEFAULT) {
                pm->page_size = _PAGESZ;
        } else {
		if (!valid_page_size(policy_set->page_size)) {
			*perrcode = PMOERR_EINVAL;
                	goto error;
		}
                pm->page_size = policy_set->page_size;
        }

	pm->page_alloc_wait_timeout = policy_set->page_wait_timeout;
	if (policy_set->policy_flags & ~(POLICY_CACHE_COLOR_FIRST | POLICY_PAGE_ALLOC_WAIT)) {
		*perrcode = PMOERR_EINVAL;
		goto error;
	}

	pm->policy_flags = policy_set->policy_flags;
        
        /*
         * All base fields must be set by the
         * policy constructors
         */

        /*
         * Replication Policy Initialization
         * REPLICATION policy initialization must be done
         * first because the placement policy constructor
         * may use the policy service method.
         */
        pmfactory = pmfactory_ns_find(pmfactory_ns_getdefault(),
                                      policy_set->replication_policy_name);
        
        if (pmfactory == NULL) {
                *perrcode = PMOERR_INV_REPLPOL;
                goto error;
        }

        *perrcode = (*pmfactory)(pm, policy_set->replication_policy_args, pmo_ns);

        if (pmo_iserrorhandle(*perrcode)) {
                goto error;
        }

        /*
         * Placement Policy Initialization
         */
        pmfactory = pmfactory_ns_find(pmfactory_ns_getdefault(),
                                      policy_set->placement_policy_name);
        
        if (pmfactory == NULL) {
                *perrcode = PMOERR_INV_PLACPOL;
                goto error;
        }

        *perrcode = (*pmfactory)(pm, policy_set->placement_policy_args, pmo_ns);

        if (pmo_iserrorhandle(*perrcode)) {
                goto error;
        }

        /*
         * Fallback Policy Initialization
         */
        pmfactory = pmfactory_ns_find(pmfactory_ns_getdefault(),
                                      policy_set->fallback_policy_name);
        
        if (pmfactory == NULL) {
                *perrcode = PMOERR_INV_FBCKPOL;
                goto error;
        }

        *perrcode = (*pmfactory)(pm, policy_set->fallback_policy_args, pmo_ns);

        if (pmo_iserrorhandle(*perrcode)) {
                goto error;
        }



        /*
         * Migration Policy Initialization
         */
        pmfactory = pmfactory_ns_find(pmfactory_ns_getdefault(),
                                      policy_set->migration_policy_name);
        
        if (pmfactory == NULL) {
                *perrcode = PMOERR_INV_MIGRPOL;
                goto error;
        }

        *perrcode = (*pmfactory)(pm, policy_set->migration_policy_args, pmo_ns);

        if (pmo_iserrorhandle(*perrcode)) {
                goto error;
        }

        /*
         * Initialize mutex lock
         */

        mrlock_init(&pm->mrlock, MRLOCK_DEFAULT, "pmlck", (long)pm);

        /*
         * Initialize base ref counting
         */
        pmo_base_init(pm, __PMO_PM, (pmo_method_t)pm_destroy);
        
        *perrcode = 0;
        return (pm);

  error:
        /*
         * In case of error we have to destroy all created objects
         * -- pm itself
         * -- placement policy objects
         * -- fallback policy objetcs
         * -- replication policy objects
         */

        ASSERT(pmo_iserrorhandle(*perrcode));
#ifdef DEBUG
        cmn_err(CE_NOTE, "[pm_create]: ERROR: %d\n", *perrcode);
#endif
        (pm->plac_srvc)?(*pm->plac_srvc)(pm, PM_SRVC_DESTROY, NULL):0;
        (pm->fbck_srvc)?(*pm->fbck_srvc)(pm, PM_SRVC_DESTROY, NULL):0;
        (pm->repl_srvc)?(*pm->repl_srvc)(pm, PM_SRVC_DESTROY, NULL):0;
        (pm->migr_srvc)?(*pm->migr_srvc)(pm, PM_SRVC_DESTROY, NULL):0;        
        
        kmem_zone_free(pm_zone, pm);         

        return (NULL);
}

pmo_handle_t
pm_create_and_export(policy_set_t* policy_set,
                     pmo_ns_t* pmo_ns,
                     pm_t** pppm,
		     pmo_handle_t *required)
{
        pm_t* pm;
        pmo_handle_t pm_handle;
        int errcode;
	void *pmo = NULL;
        
        ASSERT(policy_set);
        ASSERT(pmo_ns);
        
	if (required && (required[1] != (pmo_handle_t)(-1))) {
		pmo = (void *)pmo_ns_find(pmo_ns, required[1], __PMO_ANY);
		if (pmo == NULL)
			return (PMOERR_ESRCH);
		if ((pmo_gettype(pmo) != __PMO_MLD)&&
		    (pmo_gettype(pmo) != __PMO_MLDSET)) {
			pmo_decref(pmo, pmo_ref_find);
			return (PMOERR_EINVAL);
		}
	}
        /*
         * Create pm
         */
        if ((pm = pm_create(policy_set, pmo_ns, &errcode, pmo)) == NULL) {
                ASSERT(pmo_iserrorhandle(errcode));
                if (pppm != NULL) {
                        *pppm = NULL;
                }
		if (pmo)
			pmo_decref(pmo, pmo_ref_find);
                return (errcode);
        }
	if (pmo)
		/*
		 * decrement find ref now that pm_create has takn a reference
		 */
		pmo_decref(pmo, pmo_ref_find);
        /*
         * Insert it into the pmo table
         * Ref count incremented in pmo_ns_insert
         */
	if (required) {
		pm_handle = pmo_ns_insert_handle(pmo_ns, __PMO_PM, pm, required[0]);
		ASSERT(pm_handle == required[0]);
	} else
        	pm_handle = pmo_ns_insert(pmo_ns, __PMO_PM, pm);

        if (pmo_iserrorhandle(pm_handle)) {
                pm_destroy(pm);
        }

        if (pppm != NULL) {
                *pppm = pm;
        }

        return (pm_handle);
}
        
void
pm_decusg(pm_t *pm)
{
        ASSERT(pm);

        /*
         * Inform the policies about a disconnecting user
         */
        (pm->plac_srvc)?(*pm->plac_srvc)(pm, PM_SRVC_DECUSG, NULL):0;

#if     EXTEND_IF_NEEDED
        /*
         * As long as these methods are NOOPs, there is no need to call them
         */
        (pm->fbck_srvc)?(*pm->fbck_srvc)(pm, PM_SRVC_DECUSG, NULL):0;
        (pm->repl_srvc)?(*pm->repl_srvc)(pm, PM_SRVC_DECUSG, NULL):0;
        (pm->migr_srvc)?(*pm->migr_srvc)(pm, PM_SRVC_DECUSG, NULL):0;
#endif
}

void
pm_destroy(pm_t* pm)
{
        ASSERT(pm);

        (pm->plac_srvc)?(*pm->plac_srvc)(pm, PM_SRVC_DESTROY, NULL):0;
        (pm->fbck_srvc)?(*pm->fbck_srvc)(pm, PM_SRVC_DESTROY, NULL):0;
        (pm->repl_srvc)?(*pm->repl_srvc)(pm, PM_SRVC_DESTROY, NULL):0;
        (pm->migr_srvc)?(*pm->migr_srvc)(pm, PM_SRVC_DESTROY, NULL):0;

        mrfree(&pm->mrlock);
        
        kmem_zone_free(pm_zone, pm);
}

/* ARGSUSED */
static pmo_handle_t
pm_attach(pm_t* pm, caddr_t arg_vaddr, size_t arg_vlen)
{
        preg_t* prp;
        reg_t*  rp;
        caddr_t vaddr;
        size_t vlen;
        size_t size;
        size_t offset;  
        caddr_t end;
        caddr_t pregend;
        attr_t* attr;
#ifdef DEBUG_PMATTACH             
        extern idbg_pregion(__psint_t p1);
#endif /* DEBUG_PMATTACH */
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
        
        ASSERT(pm);

        numa_message(DM_PM, 128, "[pm_attach]: vaddr, len",
                     (long long)arg_vaddr, arg_vlen);

#ifdef DEBUG_PMATTACH        
        printf("[pm_attach]: vaddr_arg: 0x%x, len_arg: 0x%x\n", arg_vaddr, arg_vlen);
#endif /* DEBUG_PMATTACH */        
        
        vaddr = (caddr_t)ctob(btoct(arg_vaddr));
        end   = (caddr_t)ctob(btoc((arg_vaddr + arg_vlen)));
        vlen = end - vaddr;

        numa_message(DM_PM, 128, "[pm_attach]: Rvaddr, Rlen",
                     (long long)vaddr, vlen);

#ifdef DEBUG_PMATTACH     
        printf("[pm_attach]: vaddr: 0x%x, len: 0x%x\n", vaddr, vlen);
#endif /* DEBUG_PMATTACH */              

        (*pm->plac_srvc)(pm, PM_SRVC_ATTACH, vaddr, vlen);
        
	mraccess(&pas->pas_aspacelock);
        while (vlen > 0) {
                prp = findfpreg(pas, ppas, vaddr, vaddr + vlen);

#ifdef DEBUG_PMATTACH                  
                printf("[pm_attach]: findfpreg 0x%x 0x%x --> 0x%x\n",
                       vaddr, vaddr + vlen, prp);
#endif /* DEBUG_PMATTACH */                     
                
                if (prp == NULL) {
#ifdef DEBUG_PMATTACH                                
                        idbg_pregion(-1);
#endif /* DEBUG_PMATTACH */                               
                        break;
                }

                /*
                 * A user may be setting pm's on sections
                 * only partially covered by the region returned
                 * by findfpreg. We have to skip the non-covered
                 * range.
                 */
                if ((__psunsigned_t)vaddr < (__psunsigned_t)prp->p_regva) {
                        offset =  prp->p_regva - vaddr;
                        vaddr += offset;
                        vlen -= offset;
                }
                
                rp = prp->p_reg;
                reglock(rp);

                pregend = prp->p_regva + ctob(prp->p_pglen);
                if (vaddr + vlen > pregend) {
                        size = pregend - vaddr;
                } else {
                        size = vlen;
                }

                end = vaddr + ctob(btoc(size));
                attr = findattr_range(prp, vaddr, end);
                while (1) {
                        ASSERT(attr);
                        ASSERT(attr->attr_pm);

			/*
		 	 * If the new pm's page size is different from
			 * the old pm's page size downgrade the entire
			 * range to the base page size. This makes the
			 * default path fast for the default page size
			 * in vfault and pfault.
			 */

			if ((PMAT_GET_PAGESIZE(attr) > NBPP) && 
			       (PMAT_GET_PAGESIZE(attr) != PM_GET_PAGESIZE(pm)))
					pmap_downgrade_range(pas, ppas, prp,
                                                             vaddr, 
                                                             attr->attr_end - vaddr);
#ifdef DEBUG_PMATTACH
                        {
                        char* plac_name;             
                        (*pm->plac_srvc)(pm, PM_SRVC_GETNAME, &plac_name);
                        printf(
                        "[pm_attach]: Setting New PM(plac: %s) for attr [0x%x, 0x%x]\n",
                        plac_name, attr->attr_start, attr->attr_end);
                        }
#endif /* DEBUG_PMATTACH */
                        
                        attr_pm_unset(attr);
                        attr_pm_set(attr, pm);
                        if (attr->attr_end >= end) {
                                break;
                        }
                        attr = attr->attr_next;
                }

                regrele(rp);
                vaddr += size;
                vlen -= size;
        }
	mrunlock(&pas->pas_aspacelock);

        return (0);
}

/*
 * This method gets the list of PM's (handles) currently being
 * used to define the Memory Managenet Policies for a range
 * of the address space [vaddr, len].
 * The user is expected to pass a pointer to an array of
 * pmo_handles (for which memory has been allocated), and the
 * length of this array [handles, max_handles].
 * The method will insert in this array all the PM's that are used to
 * define policies for this range. If the number of PM handles to be
 * inserted is greater than the space provided in the array, this method
 * insets only the first `max_handles' handles it finds.
 * The return value is either an error code, or the number of
 * PM handles that are used to describe this range, even if this number
 * is greater than the slots available in the array.
 * The caller can use this return value to know how many handles have
 * been inserted in the array if it is less than `max_handles'.
 * Otherwise the number of handles in the array is exactly
 * `max_handles' entries.
 */
static pmo_handle_t
pm_getpmhandles(vasid_t *vasid,
		pmo_ns_t *ns,
		caddr_t vaddr_arg,
                size_t vlen_arg,
                pmo_handle_t* handles,
                int max_handles)
{
        preg_t* prp;
        reg_t*  rp;
        caddr_t vaddr;
        size_t vlen;
        size_t size;
        size_t offset;
        caddr_t end;
        caddr_t pregend;
        attr_t* attr;
        void* pm;
        pmo_handle_t pmo_handle;
        int pmo_list_index;
	pas_t *pas;
	ppas_t *ppas;
        pmo_handle_t* kbuffer;
        int errcode;
#ifdef DEBUG_PMGETALL
        extern idbg_pregion(__psint_t p1);
#endif        
	pas = VASID_TO_PAS(*vasid);
	ppas = (ppas_t *)vasid->vas_pasid;

        numa_message(DM_PM, 2, "[pm_getpmhandle]: vaddr, vlen:",
                     (long long)vaddr_arg, vlen_arg);

        if (max_handles < 1) {
                return (PMOERR_EINVAL);
        }

        if (max_handles > PM_LIMIT_MAX_HANDLES) {
                return (PMOERR_E2BIG);
        }

        kbuffer = kmem_zalloc(sizeof(pmo_handle_t) * max_handles, KM_SLEEP);
        ASSERT(kbuffer);
        
        vaddr = (caddr_t)ctob(btoct(vaddr_arg));
        end = (caddr_t)ctob(btoc((vaddr_arg + vlen_arg)));
        vlen = end - vaddr;
      
        numa_message(DM_PM, 2, "[pm_getpmhandle]: Rvaddr, Rvlen",
                     (long long)vaddr, vlen);

#ifdef DEBUG_PMGETALL
        idbg_pregion(-1);
#endif        
        
        pmo_list_index = 0;
	mraccess(&pas->pas_aspacelock);
        while (vlen > 0 && pmo_list_index < max_handles) {
                prp = findfpreg(pas, ppas, vaddr, vaddr + vlen);
                if (prp == NULL) {
                        break;
                }
#ifdef DEBUG_PM
                printf("PM_GETHANDLES: VADDR: 0x%x, REGION[0x%x, 0x%x]\n",
                       vaddr, prp->p_regva, prp->p_regva + ctob(prp->p_pglen));
#endif                       
                /*
                 * A user may be asking for pm's on sections
                 * only partially covered by the region returned
                 * by findfpreg. We have to skip the non-covered
                 * range.
                 */
                if ((__psunsigned_t)vaddr < (__psunsigned_t)prp->p_regva) {
#ifdef DEBUG_PM
                        printf("PM_GETHANDLES: Forwarding vaddr 0x%x to 0x%x\n",
                               vaddr, prp->p_regva);
#endif
                        offset =  prp->p_regva - vaddr;
                        vaddr += offset;
                        vlen -= offset;
                }

                rp = prp->p_reg;
                reglock(rp);

                pregend = prp->p_regva + ctob(prp->p_pglen);
                if (vaddr + vlen > pregend) {
                        size = pregend - vaddr;
                } else {
                        size = vlen;
                }

                end = vaddr + ctob(btoc(size));
#ifdef DEBUG_PM
                printf("PM_GETHANDLES: Range start 0x%x, end 0x%x\n",
                       vaddr, end);
#endif                
                attr = findattr_range_noclip(prp, vaddr, end);
                while (1) {
                        ASSERT(attr);
                        ASSERT(attr->attr_pm);
                        pm = (void*)attr_pm_get(attr);
#ifdef DEBUG_PM
                        printf("PM: 0x%x\n", pm);
#endif                        
                        pmo_handle = pmo_ns_pmo_lookup(ns, __PMO_PM, pm);
                        if (!pmo_iserrorhandle(pmo_handle)) {
                                if (pmo_list_index < max_handles) {
                                        kbuffer[pmo_list_index] = pmo_handle;
#ifdef DEBUG_PM
                                        printf("PMHANDLE: 0x%x\n", pmo_handle);
#endif
                                } else {
                                        break;
                                }
                                pmo_list_index++;
                        }
                        if (attr->attr_end >= end) {
                                break;
                        }
                        attr = attr->attr_next;
                }

                regrele(rp);
                vaddr += size;
                vlen -= size;
        }
	mrunlock(&pas->pas_aspacelock);

        if (copyout(kbuffer, handles, sizeof(pmo_handle_t) * max_handles)) {
                errcode = PMOERR_EFAULT;
        } else {
                errcode = pmo_list_index;
        }

        kmem_free(kbuffer, sizeof(pmo_handle_t) * max_handles);

        return (errcode);
}

#if _MIPS_SIM == _ABI64
static int pm_pginfo_to_irix5(void *, int, xlate_info_t *);
#endif

static pmo_handle_t
pm_get_pginfo(vasid_t vasid, 
	caddr_t vaddr_arg,
	size_t vlen_arg,
	pm_pginfo_t *pm_pginfo_buf,
	int max_pginfo_elems, int ckpt)
{
        preg_t* prp;
        reg_t*  rp;
        caddr_t vaddr;
        caddr_t end;
        size_t vlen;
        size_t size;
        size_t offset, page_size;
        caddr_t pregend;
        dev_t node_devt;
        int pm_pginfo_list_index;
        pm_pginfo_t* kbuffer;
        int errcode;
	pgno_t npgs, pfn;
	pde_t *pt;
#ifdef DEBUG_PMNODES
        extern idbg_pregion(__psint_t p1);
#endif        
	void 		*pm;
	pmo_handle_t	pmo_handle;
	pmo_ns_t	*pm_ns;
	attr_t		*attr;

	/*
	 * Just define it here as no other ABI functions have been declared 
	 * outside.
	 */
	pas_t *pas;
	ppas_t *ppas;

	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
                
        numa_message(DM_PM, 2, "[pm_getpmhandle]: vaddr, vlen:",
                     (long long)vaddr_arg, vlen_arg);

        if (max_pginfo_elems < 1) {
                return (PMOERR_EINVAL);
        }

        if (max_pginfo_elems > PM_LIMIT_MAX_PGINFO_ELEMS) {
                return (PMOERR_E2BIG);
        }

        kbuffer = kmem_zalloc(sizeof(pm_pginfo_t) * max_pginfo_elems, KM_SLEEP);
        ASSERT(kbuffer);
        
        vaddr = (caddr_t)ctob(btoct(vaddr_arg));
        end   = (caddr_t)ctob(btoc((vaddr_arg + vlen_arg)));
        vlen = end - vaddr;
      
        numa_message(DM_PM, 2, "[pm_vrangetonodes]: Rvaddr, Rvlen",
                     (long long)vaddr, vlen);

#ifdef DEBUG_PMNODES
        idbg_pregion(-1);
#endif        
        
        pm_pginfo_list_index = 0;
	mraccess(&pas->pas_aspacelock);
        while (vlen > 0 && pm_pginfo_list_index < max_pginfo_elems) {
                prp = findfpreg(pas, ppas, vaddr, vaddr + vlen);

                if (prp == NULL) {
                        break;
                }
#ifdef DEBUG_PMNODES
                printf("pm_getnodes: VADDR: 0x%x, REGION[0x%x, 0x%x]\n",
                       vaddr, prp->p_regva, prp->p_regva + ctob(prp->p_pglen));
#endif                       
                /*
                 * A user may be asking for pm's on sections
                 * only partially covered by the region returned
                 * by findfpreg. We have to skip the non-covered
                 * range.
                 */
                if ((__psunsigned_t)vaddr < (__psunsigned_t)prp->p_regva) {
#ifdef DEBUG_PMNODES
                        printf("pm_getnodes: Forwarding vaddr 0x%x to 0x%x\n",
                               vaddr, prp->p_regva);
#endif
                        offset =  prp->p_regva - vaddr;
                        vaddr += offset;
                        vlen -= offset;
                }

                rp = prp->p_reg;
                reglock(rp);

		attr = findattr(prp, vaddr); 

                pregend = prp->p_regva + ctob(prp->p_pglen);
                if (vaddr + vlen > pregend) {
                        size = pregend - vaddr;
                } else {
                        size = vlen;
                }

		npgs = btoct(size);

		pt = pmap_ptes(pas, prp->p_pmap, vaddr, &npgs, VM_NOSLEEP);

		if (npgs == 0) {
			regrele(rp);
			vaddr += size;
			vlen -= size;
			continue;
		}
#ifdef DEBUG_PMNODES
                printf("PM_GETHANDLES: Range start 0x%x, end 0x%x\n",
                       vaddr, vaddr + ctob(btoc(size)));
#endif                
                vlen -= ctob(npgs);

		for(; npgs > 0; pt++, npgs--, vaddr += NBPP) {

			if(pg_isvalid(pt)) {
				pg_pfnacquire(pt);
				pfn = pg_getpfn(pt);
				page_size = PAGE_MASK_INDEX_TO_SIZE(
						pg_get_page_mask_index(pt));
				pg_pfnrelease(pt);
			} else if (ckpt && rp->r_anon) {
				pfd_t *pfd;
				void *id;
				sm_swaphandle_t onswap;
				/* look at possible shared anon pages */
				pfd = anon_pfind(rp->r_anon, 
					vtoapn(prp, vaddr), &id, &onswap);
				if(pfd) {
					pfdat_hold(pfd);
					pfn = pfdattopfn(pfd);
					pfdat_release(pfd);
					page_size = NBPP;
				} else 
					continue;	
			} else  /* page is not valid */
				continue;
			
			if(ckpt) /* ckpt just uses the cnodeid */
				node_devt = PFN_TO_CNODEID(pfn);
			else 
				node_devt = 
					cnodeid_to_vertex(PFN_TO_CNODEID(pfn));
			ASSERT(attr);
			ASSERT(attr->attr_pm);
			pm = (void*)attr_pm_get(attr);
			/*
			 * Use foreign name space
			 */
			pm_ns = pas->pas_aspm->pmo_ns;
			pmo_handle = pmo_ns_pmo_lookup(pm_ns, __PMO_PM, pm);
			if (pm_pginfo_list_index < max_pginfo_elems) {
				kbuffer[pm_pginfo_list_index].vaddr = vaddr; 
				kbuffer[pm_pginfo_list_index].node_dev 
								= node_devt;
				kbuffer[pm_pginfo_list_index].page_size 
								= page_size;
				kbuffer[pm_pginfo_list_index].pm_handle 
								= pmo_handle;
				pm_pginfo_list_index++;
#ifdef	DEBUG_PMNODES
				printf(
		"NODE DEVT: vaddr 0x%x node %d devt 0x%x psz %x\n", 
		vaddr, PFN_TO_CNODEID(pfn), node_devt, page_size);
#endif
			} else {
				break;
			}

			if (attr->attr_end <= vaddr)
				attr = attr->attr_next;
                }

                regrele(rp);
        }
	mrunlock(&pas->pas_aspacelock);

	if (pm_pginfo_list_index && 
		(XLATE_COPYOUT(kbuffer, pm_pginfo_buf, 
			sizeof(pm_pginfo_t) * pm_pginfo_list_index, 
			pm_pginfo_to_irix5, 
			get_current_abi(), 
			pm_pginfo_list_index))) {

                errcode = PMOERR_EFAULT;
        } else {
                errcode = pm_pginfo_list_index;
        }

        kmem_free(kbuffer, sizeof(pm_pginfo_t) * max_pginfo_elems);

        return (errcode);
}

static void
pm_getstat(pmo_ns_t *ns,
	   struct pm* pm,
           policy_set_t* policy_set,
           size_t* page_size,
           pmo_handle_t* pmo_handle,
	   vasid_t vasid)
{
	char	*policy_name;
	pas_t	*pas;
	aspm_t	*aspm;
	int	s;

       	if (pm->plac_srvc)
		(*pm->plac_srvc)(pm, PM_SRVC_GETNAME, &policy_name);
	else policy_name = NULL;

	policy_set->placement_policy_name = policy_name;

       	if (pm->fbck_srvc)
		(*pm->fbck_srvc)(pm, PM_SRVC_GETNAME, &policy_name);
	else policy_name = NULL;

        policy_set->fallback_policy_name = policy_name;

	if (pm->repl_srvc)
		(*pm->repl_srvc)(pm, PM_SRVC_GETNAME, &policy_name);
	else policy_name = NULL;

        policy_set->replication_policy_name = policy_name;

	if (pm->migr_srvc)
		(*pm->migr_srvc)(pm, PM_SRVC_GETNAME, &policy_name);
	else policy_name = NULL;

        policy_set->migration_policy_name = policy_name;

        policy_set->paging_policy_name = NULL;

	policy_set->policy_flags = pm->policy_flags;
	policy_set->page_wait_timeout = pm->page_alloc_wait_timeout;
        *page_size = pm->page_size;

        *pmo_handle = pmo_ns_pmo_lookup(ns, __PMO_ANY, pm);

	/*
	 * Set policy flags if this policy module is also used
	 * as a default policy module.
	 */
	pas = VASID_TO_PAS(vasid);
	aspm = pas->pas_aspm;
	s = aspm_lock(aspm);

	if (*pmo_handle == aspm->pm_default_hdl[MEM_STACK])
		policy_set->policy_flags |= POLICY_DEFAULT_MEM_STACK;
	if (*pmo_handle == aspm->pm_default_hdl[MEM_TEXT])
		policy_set->policy_flags |= POLICY_DEFAULT_MEM_TEXT;
	if (*pmo_handle == aspm->pm_default_hdl[MEM_DATA])
		policy_set->policy_flags |= POLICY_DEFAULT_MEM_DATA;
 
	aspm_unlock(aspm, s);
	
#ifdef DEBUG_PM
        printf("PM_GETSTAT: pmo_handle 0x%x\n", *pmo_handle);
#endif        
        if (pmo_iserrorhandle(*pmo_handle)) {
                *pmo_handle = (pmo_handle_t)(-1);
        }
}


/**************************************************************************
 *                     Policy Module Kernel Xface                         *
 **************************************************************************/

/*
 * For this object, the kernel constructor for pm's is the same as
 * the internal pm_create_and_export method.
 * Aliased via a macro.
 */

/* See pm_create_and_export above */

pfd_t*
pmat_pagealloc(attr_t* attr, uint ckey, int flags, size_t* ppsz, caddr_t vaddr)
{
        ASSERT(attr);
        ASSERT(ppsz);

        return (*((pm_t*)(attr->attr_pm))->pagealloc)((attr->attr_pm),
                                                      ckey,
                                                      flags,
                                                      ppsz,
                                                      vaddr);        

}

pfd_t*
pm_pagealloc(pm_t* pm, uint ckey, int flags, size_t* ppsz, caddr_t vaddr)
{
        ASSERT (pm);
        ASSERT(ppsz);
        
        return (*((pm_t*)pm)->pagealloc)(pm, ckey, flags, ppsz, vaddr);
}

size_t
pmat_get_pagesize(attr_t *attr)
{
#ifdef	DEBUG
	extern	int	debug_page_size;
#endif
        ASSERT (attr);

        return (((pm_t*)(attr)->attr_pm)->page_size);
}

size_t
pm_get_pagesize(pm_t *pm)
{
#ifdef	DEBUG 
	extern	int	debug_page_size;
#endif
        ASSERT (pm);
        return (((pm_t*)(pm))->page_size);
}
        
int
pm_page_alloc_wait_timeout(pm_t *pm)
{
#ifdef	DEBUG 
	extern	int	debug_page_size;
#endif
        ASSERT (pm);
        return (((pm_t*)(pm))->page_alloc_wait_timeout);
}


int
pm_sync_policy(caddr_t uvaddr, size_t len)
{
	caddr_t		evaddr = uvaddr + len;
	caddr_t		vaddr;
	pm_setas_t	pm_asdata;
	size_t		vlen, size;
	preg_t		*prp;
	reg_t		*rp;
        caddr_t		end;
        caddr_t		pregend;
        attr_t		*attr;
	pm_t		*pm;
	int		error;
	vnode_t		*vp;
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;

	if (len == 0)
		return (0);


	for (vaddr = uvaddr; vaddr < evaddr; vaddr += NBPP) {
		if (fubyte(vaddr) < 0)
			return EINVAL;
	}

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
	mraccess(&pas->pas_aspacelock);

	vaddr = uvaddr;
	vlen = len;

	while (vlen > 0) {

		prp = findfpreg(pas, ppas, vaddr, vaddr + vlen);
		if (prp == NULL) {
			mrunlock(&pas->pas_aspacelock);
			return EINVAL;
		}

		rp = prp->p_reg;
		if (rp->r_flags & RG_TEXT) {
			vp = rp->r_vnode;
			ASSERT(vp);
			VOP_RWLOCK(vp, VRWLOCK_WRITE);
		} else vp = NULL;

		reglock(rp);

		pregend = prp->p_regva + ctob(prp->p_pglen);

                if (vaddr + vlen > pregend) {
                        size = pregend - vaddr;
                } else {
                        size = vlen;
                }

                end = vaddr + ctob(btoc(size));
                attr = findattr_range(prp, vaddr, end);
		while (attr->attr_end <= end) {
			pm = (void*)attr_pm_get(attr);

			pm_asdata.prp = prp;
			pm_asdata.vaddr = attr->attr_start;
			pm_asdata.len = attr->attr_end - attr->attr_start;
			pm_asdata.attr = attr;

			if (pm->plac_srvc) {
				error = (long)(*pm->plac_srvc)(pm,
                                                               PM_SRVC_SYNC_POLICY, 
                                                               &pm_asdata);
					
				if (error) {
					regrele(rp);
					mrunlock(&pas->pas_aspacelock);
					return error;
				}
			}

			if (attr->attr_end == end)
				break;
			attr = attr->attr_next;
			ASSERT(attr);
		}

		regrele(rp);
		if (vp)
			VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
		vaddr += size;
		vlen -=size;
	}
	mrunlock(&pas->pas_aspacelock);
	return 0;
}

/**************************************************************************
 *                              ABI conversion                            *
 **************************************************************************/

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int
irix5_to_pm_info(
        enum xlate_mode mode,
        void *to,
        int count,
        xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(irix5_pm_info, pm_info);
        
        bcopy(source->placement_policy_name,
              target->placement_policy_name,
              PM_NAME_SIZE + 1);
        target->placement_policy_args =
                (void*)(__psint_t)(int)source->placement_policy_args;
        
        bcopy(source->fallback_policy_name,
              target->fallback_policy_name,
              PM_NAME_SIZE + 1);
        target->fallback_policy_args =
                (void*)(__psint_t)(int)source->fallback_policy_args;
        
        bcopy(source->replication_policy_name,
              target->replication_policy_name,
              PM_NAME_SIZE + 1);
        target->replication_policy_args =
                (void*)(__psint_t)(int)source->replication_policy_args;
        
        bcopy(source->migration_policy_name,
              target->migration_policy_name,
              PM_NAME_SIZE + 1);
        target->migration_policy_args =
                (void*)(__psint_t)(int)source->migration_policy_args;
        
        bcopy(source->paging_policy_name,
              target->paging_policy_name,
              PM_NAME_SIZE + 1);        
        target->paging_policy_args =
                (void*)(__psint_t)(int)source->paging_policy_args;

        target->page_size = source->page_size;
        target->policy_flags = source->policy_flags;
        target->page_wait_timeout = source->page_wait_timeout;
        
        return 0;
}

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

/*ARGSUSED*/
static int
irix5_to_pmo_handle_list(
        enum xlate_mode mode,
        void *to,
        int count,
        xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(irix5_pmo_handle_list, pmo_handle_list);
        target->handles = (pmo_handle_t*)(__psint_t)source->handles;
        target->length = source->length;
        return 0;
}

/*ARGSUSED*/
static int
irix5_to_pm_pginfo_list(
        enum xlate_mode mode,
        void *to,
        int count,
        xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(irix5_pm_pginfo_list, pm_pginfo_list);
        target->pm_pginfo = (pm_pginfo_t *)(__psint_t)(int)source->pm_pginfo;
        target->length = source->length;
        return 0;
}

/* ARGSUSED */
static int
pm_stat_to_irix5(void *from,
		 int count,
		 xlate_info_t *info)
{
        XLATE_COPYOUT_PROLOGUE(pm_stat, irix5_pm_stat);

        bcopy(source->placement_policy_name,
              target->placement_policy_name,
              PM_NAME_SIZE + 1);

        bcopy(source->fallback_policy_name,
              target->fallback_policy_name,
              PM_NAME_SIZE + 1);

        bcopy(source->replication_policy_name,
              target->replication_policy_name,
              PM_NAME_SIZE + 1);
        
        bcopy(source->migration_policy_name,
              target->migration_policy_name,
              PM_NAME_SIZE + 1);
        
        bcopy(source->paging_policy_name,
              target->paging_policy_name,
              PM_NAME_SIZE + 1);        

        target->page_size = source->page_size;
        target->policy_flags = source->policy_flags;
        target->pmo_handle = source->pmo_handle;

        return 0;
}

/* ARGSUSED */
static int
pm_pginfo_to_irix5(void *from, int count, xlate_info_t *info)
{
	int	i;

	XLATE_COPYOUT_VARYING_PROLOGUE(pm_pginfo_s, irix5_pm_pginfo_s, 
				sizeof(irix5_pm_pginfo_t) * count);

	bzero(target, sizeof(irix5_pm_pginfo_t) * count);

	for (i = 0; i < count; i++) {
		target->page_size = source->page_size;
		target->node_dev = source->node_dev;
		target->vaddr = (__psint_t)source->vaddr;
		target->pm_handle = source->pm_handle;
		target++;
		source++;
	}
	return 0;
}
#endif

/**************************************************************************
 *                     Policy Module User Xface                           *
 **************************************************************************/

pmo_handle_t
pmo_xface_pm_create(pm_info_t* pm_info_arg, pmo_handle_t *hdlspec)
{
        pm_info_t pm_info;
        int ec;
        pmo_handle_t pm_handle;
        policy_set_t policy_set;
	pmo_handle_t pmhandles[2]	/* 1 handle for pm, 1 for pm->pmo */;
	pmo_handle_t *required = NULL;

        ec = COPYIN_XLATE((void*)pm_info_arg,
                          (void*)&pm_info,
                          sizeof(pm_info_t),
                          irix5_to_pm_info,
                          get_current_abi(), 1);
        if (ec) {
                return (PMOERR_EFAULT);
        }
	if (hdlspec) {
		ec = copyin((void *)hdlspec, pmhandles, sizeof(pmhandles));
		if (ec)
			return (PMOERR_EFAULT);
		required = pmhandles;
	}
        numa_message(DM_PM, 2, "[pmo_xface_pm_create]: pm names", 0, 0);
        numa_message(DM_PM, 2, pm_info.placement_policy_name, 0, 0);
        numa_message(DM_PM, 2, pm_info.fallback_policy_name, 0, 0);
        numa_message(DM_PM, 2, pm_info.replication_policy_name, 0, 0);
        numa_message(DM_PM, 2, pm_info.migration_policy_name, 0, 0);
        numa_message(DM_PM, 2, pm_info.paging_policy_name, 0, 0);
        numa_message(DM_PM, 2, "[pmo_xface_pm_create: page_size:", pm_info.page_size, 0);
        numa_message(DM_PM, 2, "[pmo_xface_pm_create: policy_flags:", pm_info.policy_flags, 0);
        numa_message(DM_PM, 2, "[pmo_xface_pm_create: page_wait_timeout:", pm_info.page_wait_timeout, 0);
        
        policy_set.placement_policy_name = pm_info.placement_policy_name;
        policy_set.fallback_policy_name = pm_info.fallback_policy_name;
        policy_set.replication_policy_name = pm_info.replication_policy_name;
        policy_set.migration_policy_name = pm_info.migration_policy_name;
        policy_set.paging_policy_name = pm_info.paging_policy_name;
        policy_set.page_size = pm_info.page_size;
        policy_set.policy_flags = pm_info.policy_flags;
        policy_set.page_wait_timeout = pm_info.page_wait_timeout;

        numa_message(DM_PM, 2, "[pmo_xface_pm_create]: pm args", 0, 0);
        numa_message(DM_PM, 2, "plac_args: ",
                     (long long)pm_info.placement_policy_args, 0);
        numa_message(DM_PM, 2, "fbck_args: ",
                     (long long) pm_info.fallback_policy_args, 0);
        numa_message(DM_PM, 2, "repl_args: ",
                     (long long) pm_info.replication_policy_args, 0);
        numa_message(DM_PM, 2, "migr_args: ",
                     (long long) pm_info.migration_policy_args, 0);
        numa_message(DM_PM, 2, "pagn_args: ",
                     (long long) pm_info.paging_policy_args, 0);        

        policy_set.placement_policy_args = pm_info.placement_policy_args;
        policy_set.fallback_policy_args = pm_info.fallback_policy_args;
        policy_set.replication_policy_args = pm_info.replication_policy_args;
        policy_set.migration_policy_args = pm_info.migration_policy_args;
        policy_set.paging_policy_args = pm_info.paging_policy_args;

        pm_handle = pm_create_and_export(&policy_set,
                                         curpmo_ns(),
                                         NULL,
					 required);
        return (pm_handle);
}


pmo_handle_t
pmo_xface_pm_destroy(pmo_handle_t pm_handle)
{
        pm_t* pm;

        /*
         * A user level destroy only removes this pm
         * from the name space. pmo_ns_extract releases
         * the ref corresponding to the ref from the ns.
         */
        pm = (pm_t*)pmo_ns_extract(curpmo_ns(), pm_handle, __PMO_PM);
        if (pm == NULL) {
                return (PMOERR_INV_PM_HANDLE);
        }
        
        return (0);
}
        
pmo_handle_t
pmo_xface_pm_attach(pmo_handle_t handle, vrange_t* vrange_arg)
{
        vrange_t vrange;
        pm_t* pm;

        if (COPYIN_XLATE((void*)vrange_arg,
                         (void*)&vrange,
                         sizeof(vrange_t),
                         irix5_to_vrange,
                         get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        pm = (pm_t*)pmo_ns_find(curpmo_ns(),
                                handle,
                                __PMO_PM);
        if (pm == NULL) {
                return (PMOERR_INV_PM_HANDLE);
        }
#ifdef DEBUG_PMATTACH
        printf("[pmo_xface_pm_attach]: pm<%d>, vaddr 0x%x, len 0x%x\n",
               handle, vrange.base_addr, vrange.length);
#endif /* DEBUG_PMATTACH */
        handle = pm_attach(pm, vrange.base_addr, vrange.length);
        pmo_decref(pm, pmo_ref_find);

        return (handle);
}        

static pmo_handle_t
_pmo_xface_pm_getpmhandles(vasid_t *vasid, pmo_ns_t *ns, vrange_t* vrange_arg,
				pmo_handle_list_t* pmo_handle_list_arg)
{
        vrange_t vrange;
        pmo_handle_list_t pmo_handle_list;

        if (COPYIN_XLATE((void*)vrange_arg,
                         (void*)&vrange,
                         sizeof(vrange_t),
                         irix5_to_vrange,
                         get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        if (COPYIN_XLATE((void*)pmo_handle_list_arg,
                         (void*)&pmo_handle_list,
                         sizeof(pmo_handle_list_t),
                         irix5_to_pmo_handle_list,
                         get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        return (pm_getpmhandles(vasid,
				ns,
				vrange.base_addr,
                                vrange.length,
                                pmo_handle_list.handles,
                                pmo_handle_list.length));
}

pmo_handle_t
pmo_xface_pm_getpmhandles(vrange_t* vrange_arg,
				pmo_handle_list_t* pmo_handle_list_arg)
{
	vasid_t vasid;

	as_lookup_current(&vasid);
	return _pmo_xface_pm_getpmhandles(&vasid,
					curpmo_ns(),
					vrange_arg,
					pmo_handle_list_arg);
}

pmo_handle_t
pmo_xface_get_pginfo(vrange_t* vrange_arg, 
			pm_pginfo_list_t* pginfo_arg)
{
        vrange_t vrange;
        pm_pginfo_list_t pm_pginfo_list;
	vasid_t vasid;

        if (COPYIN_XLATE((void*)vrange_arg,
                         (void*)&vrange,
                         sizeof(vrange_t),
                         irix5_to_vrange,
                         get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        if (COPYIN_XLATE((void*)pginfo_arg,
                         (void*)&pm_pginfo_list,
                         sizeof(pm_pginfo_list_t),
                         irix5_to_pm_pginfo_list,
                         get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

	as_lookup_current(&vasid);
        return (pm_get_pginfo(vasid, 
				vrange.base_addr,
                                vrange.length,
                                pm_pginfo_list.pm_pginfo,
                                pm_pginfo_list.length, 0));
}


pmo_handle_t
pmo_xface_pm_stat(pmo_handle_t handle_arg, pm_stat_t* pm_stat_arg)
{
        pm_t* pm;
        policy_set_t policy_set;
        size_t page_size;
        pmo_handle_t pmo_handle;
	pm_stat_t pm_stat;
	vasid_t	vasid;

        pm = (pm_t*)pmo_ns_find(curpmo_ns(),
                                handle_arg,
                                __PMO_PM);
        if (pm == NULL) {
                return (PMOERR_INV_PM_HANDLE);
        }

	as_lookup_current(&vasid);
        pm_getstat(curpmo_ns(), pm, &policy_set, &page_size, &pmo_handle,vasid);
        pmo_decref(pm, pmo_ref_find);

        if (policy_set.placement_policy_name) {
                bcopy(policy_set.placement_policy_name,
                      pm_stat.placement_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_stat.placement_policy_name[0] = '\0';
        }

        if (policy_set.fallback_policy_name) {
                bcopy(policy_set.fallback_policy_name,
                      pm_stat.fallback_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_stat.fallback_policy_name[0] = '\0';
        }

        if (policy_set.replication_policy_name) {
                bcopy(policy_set.replication_policy_name,
                      pm_stat.replication_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_stat.replication_policy_name[0] = '\0';
        }

        if (policy_set.migration_policy_name) {
                bcopy(policy_set.migration_policy_name,
                      pm_stat.migration_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_stat.migration_policy_name[0] = '\0';
        }

        if (policy_set.paging_policy_name) {
                bcopy(policy_set.paging_policy_name,
                      pm_stat.paging_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_stat.paging_policy_name[0] = '\0';
        }

        pm_stat.policy_flags = policy_set.policy_flags;
        pm_stat.page_size = page_size;
        pm_stat.pmo_handle = pmo_handle;

        if (XLATE_COPYOUT(&pm_stat,
                          pm_stat_arg,
                          sizeof(pm_stat_t),
                          pm_stat_to_irix5,
                          get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        return (0);
}

pmo_handle_t
pmo_xface_pm_set_pagesize(pmo_handle_t handle, size_t page_size)
{
        pm_t* pm;

        /*
         * Check validity of args here...
         */

	if (!valid_page_size(page_size)) {
		return (PMOERR_EINVAL);
	}

        pm = (pm_t*)pmo_ns_find(curpmo_ns(),
                                handle,
                                __PMO_PM);
        if (pm == NULL) {
                return (PMOERR_INV_PM_HANDLE);
        }

        atomicFieldAssignLong((long*)&pm->page_size, ~0LL, page_size);

        pmo_decref(pm, pmo_ref_find);

        return (0);
}              

#ifdef CKPT
/**************************************************************************
 *                          Checkpoint Interfaces                         *
 **************************************************************************/

pmo_handle_t
pmo_ckpt_pmo_nexthandle(int cur, asid_t asid, pmo_handle_t handle, pmo_type_t type, int *rval)
{
	vasid_t vasid;

	if (cur == 0) {
		/*
		 * make sure addr space doesn't go away
		 */
		if (as_lookup(asid, &vasid))
			return pmo_checkerror(PMOERR_EINVAL);
	}
	switch (type) {
	case __PMO_PM:
	case __PMO_MLD:
	case __PMO_MLDSET:
		*rval = pmo_ns_find_handle((cur)? curpmo_ns() : getpmo_ns(asid),
					   handle, type);
		if (cur == 0)
			relpmo_ns(asid);
		break;
	default:
		*rval = PMOERR_EINVAL;
		break;
	}
	if (cur == 0)
		as_rele(vasid);

	return pmo_checkerror(*rval);
}

pmo_handle_t
pmo_ckpt_pm_getpmhandles(asid_t asid, vrange_t* vrange_arg,
				pmo_handle_list_t* pmo_handle_list_arg,
				int *rval)
{
	vasid_t vasid;

	if (as_lookup(asid, &vasid))
		return pmo_checkerror(PMOERR_EINVAL);

	*rval = _pmo_xface_pm_getpmhandles(&vasid,
					getpmo_ns(asid),
					vrange_arg,
					pmo_handle_list_arg);
	relpmo_ns(asid);
	as_rele(vasid);
	return pmo_checkerror(*rval);
}

pmo_handle_t
pmo_ckpt_pm_info(	asid_t asid,
			pmo_handle_t handle_arg,
			pm_info_t* pm_info_arg,
			pmo_handle_t *plac_handle)
{
	pmo_ns_t *ns;
        pm_t* pm;
        policy_set_t policy_set;
        size_t page_size;
        pmo_handle_t pmo_handle;
	pm_info_t pm_info;
	int arg;
	__uint64_t migr_arg;
	int err = 0;
	vasid_t	vasid;

	ns = getpmo_ns(asid);
	if (ns == NULL)
		return (pmo_checkerror(PMOERR_ENOENT));

        pm = (pm_t*)pmo_ns_find(ns, handle_arg, __PMO_PM);
        if (pm == NULL) {
		relpmo_ns(asid);
                return (pmo_checkerror(PMOERR_INV_PM_HANDLE));
        }
	if (as_lookup(asid, &vasid))
		return(pmo_checkerror(PMOERR_EINVAL));
        pm_getstat(ns, pm, &policy_set, &page_size, &pmo_handle, vasid);
	as_rele(vasid);

	/*
	 * Get name and arg for each policy
	 */
        if (policy_set.placement_policy_name) {
                bcopy(policy_set.placement_policy_name,
                      pm_info.placement_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_info.placement_policy_name[0] = '\0';
        }
	if (pm->plac_srvc) {
        	(*pm->plac_srvc)(pm, PM_SRVC_GETARG, &arg, ns);
		if (err = pmo_checkerror(arg))
			goto done;

		pm_info.placement_policy_args = (void *)(__int64_t)arg;
	} else
		pm_info.placement_policy_args = NULL;

        if (policy_set.fallback_policy_name) {
                bcopy(policy_set.fallback_policy_name,
                      pm_info.fallback_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_info.fallback_policy_name[0] = '\0';
        }
	if (pm->fbck_srvc) {
        	(*pm->fbck_srvc)(pm, PM_SRVC_GETARG, &arg, ns);
		if (err = pmo_checkerror(arg))
			goto done;

		pm_info.fallback_policy_args = (void *)(__int64_t)arg;
	} else
		pm_info.fallback_policy_args = NULL;

        if (policy_set.replication_policy_name) {
                bcopy(policy_set.replication_policy_name,
                      pm_info.replication_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_info.replication_policy_name[0] = '\0';
        }
	if (pm->repl_srvc) {
        	(*pm->repl_srvc)(pm, PM_SRVC_GETARG, &arg, ns);
		if (err = pmo_checkerror(arg))
			goto done;

		pm_info.replication_policy_args = (void *)(__int64_t)arg;
	} else
		pm_info.replication_policy_args = NULL;

        if (policy_set.migration_policy_name) {
                bcopy(policy_set.migration_policy_name,
                      pm_info.migration_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_info.migration_policy_name[0] = '\0';
        }
	if (pm->migr_srvc) {
        	(*pm->migr_srvc)(pm, PM_SRVC_GETMIGRPARMS, &migr_arg, ns);
		if (err = pmo_checkerror(arg))
			goto done;

		pm_info.migration_policy_args = (void *)migr_arg;
	} else
		pm_info.migration_policy_args = NULL;

        if (policy_set.paging_policy_name) {
                bcopy(policy_set.paging_policy_name,
                      pm_info.paging_policy_name,
                      PM_NAME_SIZE + 1);
        } else {
                pm_info.paging_policy_name[0] = '\0';
        }
	pm_info.paging_policy_args = NULL;

        pm_info.policy_flags = policy_set.policy_flags;
        pm_info.page_wait_timeout = policy_set.page_wait_timeout;
        pm_info.page_size = page_size;
	/*
	 * Get handle for pm->pmo
	 */
	if (pm->pmo) {
		pmo_handle = pmo_ns_pmo_lookup(	ns,
							pmo_gettype(pm->pmo),
							pm->pmo);
		if (pmo_handle == PMOERR_ESRCH) {
			/*
			 * Somehow got taken out of namespace.  Put
	 		 * it back in so can have a value to restore to.
			 */
			ASSERT(0);
			pmo_handle = pmo_ns_insert(	ns,
							pmo_gettype(pm->pmo),
							pm->pmo);
		}
		err = pmo_checkerror(pmo_handle);

		ASSERT(err == 0);
	} else
		pmo_handle = (pmo_handle_t)(-1);
done:
        pmo_decref(pm, pmo_ref_find);
	relpmo_ns(asid);
	/*
	 * Checkpoint only supports abi's that match kernel abi,
	 * and this enforced by ckpt entry before getting here, so
	 * do a copyout rather than XLATE_COPYOUT
	 */
        if (copyout(&pm_info, pm_info_arg, sizeof(pm_info_t)))
		return (pmo_checkerror(PMOERR_EFAULT));

        if (copyout(&pmo_handle, plac_handle, sizeof(pmo_handle_t)))
		return (pmo_checkerror(PMOERR_EFAULT));

	return (err);
}

int pm_ckpt_get_pginfo(vasid_t vasid, vrange_t vrange, 
				pm_pginfo_list_t pginfo_list)
{
	return (pm_get_pginfo(vasid, vrange.base_addr, vrange.length, 
			pginfo_list.pm_pginfo, pginfo_list.length, 1));

}

#endif
