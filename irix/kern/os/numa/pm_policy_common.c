/*
 * os/numa/pm_policy_common.c
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
#include "os/as/as_private.h" /* XXX */
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/pmo.h>
#include <sys/mmci.h>
#include <sys/lpage.h>
#include <os/as/pmap.h>
#include <ksys/migr.h>
#include "sys/pmo.h"
#include "pmo_base.h"
#include "pmo_ns.h"
#include "pm.h"
#include "pm_policy_common.h"
#include "sys/nodepda.h"
#include "pfms.h"
#include "migr_control.h"
#include "migr_engine.h"
#include "numa_hw.h"
#include "migr_states.h"
#include "migr_refcnt.h"



/*
 * Contains routines which are common to several policies, will be called
 * from individual policy modules.
 */
/*
 * The following routine gets called from various placement policy modules
 * to replace pages that don't correspond to the new policy for that address
 * range.
 */

/* ARGSUSED */
int
plac_policy_sync(pm_t *pm, pm_setas_t *as_data)
{
	preg_t	*prp;
	caddr_t	vaddr;
	attr_t	*attr;
	size_t	len;
	pgno_t	sz;
	caddr_t	aligned_addr, end_addr;
	size_t	page_size, allocated_page_size;
	pgmaskindx_t	page_mask_index;
	pde_t	*pt, *tmp_pt, template_pte;
	pgno_t	npgs;
	uint	ckey;
	pfd_t	*pfd, *lpfd;
	int	is_even_page = 0;
	int	is_large_page;
	int	mod_bit_differs = 0;
	int	pte_not_valid;
	pgno_t	num_page_clicks;
        /*REFERENCED*/
	pfms_t	pfms;
        /*REFERENCED*/
	int	pfms_spl;
        /*REFERENCED*/
	pfn_t	pfn;
	vasid_t vasid;
	pas_t *pas;
	long	mismatched_bits; /* Bits that differ in a pte */

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);

	prp = as_data->prp;
	vaddr = as_data->vaddr;
	len = as_data->len;
	attr = as_data->attr;

	page_size = PM_GET_PAGESIZE(pm);

	aligned_addr = LPAGE_ALIGN_ADDR(vaddr, 
				NUM_TRANSLATIONS_PER_TLB_ENTRY * page_size);

	if (aligned_addr < vaddr)
		aligned_addr += NUM_TRANSLATIONS_PER_TLB_ENTRY * page_size;

	end_addr = LPAGE_ALIGN_ADDR(vaddr + len, 
				NUM_TRANSLATIONS_PER_TLB_ENTRY * page_size);

	len = end_addr - aligned_addr;

			
	/*
	 * If large pages are not needed or the length is less than the
	 * page size skip.
	 */

	if ((page_size == NBPP) || (len < page_size)) return 0;

	page_mask_index = PAGE_SIZE_TO_MASK_INDEX(page_size);

	num_page_clicks = btoct(page_size);

	len = btoct(len);

	is_even_page = 1; /* Start with even page */

	while (len > 0) {
		sz = len;
		pt = pmap_ptes(pas, prp->p_pmap, aligned_addr, &sz, 0);
		len -= sz;
		for (;sz > 0; sz -= num_page_clicks,
				aligned_addr += page_size,
				pt += num_page_clicks,
				(is_even_page = is_even_page ? 0 : 1)) {
			/*
			 * If the page is already of the desired page size
			 * skip it.
			 */
			if (pg_get_page_mask_index(pt) == page_mask_index) {
				continue;
			}

			pfd = pdetopfdat(pt);
			is_large_page = 1;
			pte_not_valid = 0;

			/*
			 * The template pte is used to ensure that all the ptes
			 * in the large page have the same attributes. This
			 * include mod bit, cc bits, vr, dirty etc., Only if
			 * they are the same do we allow the page to be
			 * upgraded. So the template_pte is copied with the
			 * first pte. In the loop we override the pfn field
			 * with the current pte. If only the mod bit differs
                         * we allow the pte to be upgraded but we also
                         * clear the mod bit on all the large page ptes.
                         * This will cause an extra fault but worth not losing
                         * the large page.
                         */
			pg_setpgi(&template_pte, pg_getpgi(pt));
			mod_bit_differs = 0;
			for (npgs = num_page_clicks, tmp_pt = pt; npgs; 
						npgs--, tmp_pt++) {
				if (!pg_isvalid(tmp_pt)) {
					pte_not_valid++;
					break;
				}
	
				pg_setpfn(&template_pte, pg_getpfn(tmp_pt));

				mismatched_bits = pg_getpgi(&template_pte) ^
                                                        pg_getpgi(tmp_pt);
				if (mismatched_bits) {
                                        if (mismatched_bits == (PG_M|PG_D)) {
                                                mod_bit_differs++;
                                        } else {
                                                pte_not_valid++;
                                                break;
                                        }
                                }

				if (pt == tmp_pt) continue;
				if ((pfd + 1) == pdetopfdat(tmp_pt))
					pfd++;
				else is_large_page = 0;
			}

			if (pte_not_valid) {
				/*
				 * Downgrade the even part of the TLB entry.
				 */
				if (!is_even_page) {
					pmap_downgrade_lpage_pte(pas, 
						aligned_addr - page_size, 
						pt - num_page_clicks);
				}
				continue;
			}

			pfd = pdetopfdat(pt);

			if (pfd != pfdat_to_large_pfdat(pfd, num_page_clicks))
				is_large_page = 0;

			if (is_large_page) {
				for (npgs = num_page_clicks, tmp_pt = pt; npgs; 
							npgs--, tmp_pt++) {
					pg_set_page_mask_index(tmp_pt, 
							page_mask_index);
					pg_setsftval(tmp_pt);
				}
				continue;
			}

			ckey = vcache2(prp, attr, vtoapn(prp, vaddr));
			allocated_page_size = page_size;

			lpfd = PM_PAGEALLOC(pm,
                                            ckey,
                                            0,
                                            &allocated_page_size,
                                            aligned_addr);
			if (allocated_page_size == _PAGESZ) {
				pagefree(lpfd);
				if (!is_even_page) {
					pmap_downgrade_lpage_pte(pas, 
						aligned_addr - page_size, 
						pt - num_page_clicks);
				}
				return EAGAIN;
			}

			if (lpfd == NULL) {
				if (!is_even_page) {
					pmap_downgrade_lpage_pte(pas, 
						aligned_addr - page_size, 
						pt - num_page_clicks);
				}
				return EAGAIN;
			}
			
			for (npgs = num_page_clicks, tmp_pt = pt;
                             npgs; 
                             npgs--, tmp_pt++, lpfd++) {
                                cnodeid_t node;
                                pfmsv_t pfmsv;
                                
				pfd = pdetopfdat_hold(tmp_pt);
				if (!pfd) {
					/*
					 * Free the rest of the pages in
				 	 * the large page.	
					 */
					while (npgs--)
						pagefree(lpfd++);
					if (!is_even_page) {
						pmap_downgrade_lpage_pte(pas, 
							aligned_addr - 
							page_size, 
							pt - num_page_clicks);
					}
					return EAGAIN;
				}
					
				ASSERT(pfd->pf_flags & P_HASH);

				/*
				 * Try to get rid of any buffer cache 
				 * references
				 * to this page.
				 */
				if (!(pfd->pf_flags & P_ANON)) {
					(void)lpage_release_buffer_reference(pfd, TRUE);
				}

				pfdat_release(pfd);

				pfn = pfdattopfn(pfd);
                                node = PFN_TO_CNODEID(pfn);
				pfms = PFN_TO_PFMS(pfn);
                                pfmsv = PFMS_GETV(pfms);
				PFMS_LOCK(pfms, pfms_spl);
				PFMS_STATE_MIGRED_SET(pfms);
				PFMS_UNLOCK(pfms, pfms_spl);

                                migr_refcnt_update_counters(node,
                                                            pfn,
                                                            pfmsv,
                                                            MD_PROT_MIGMD_OFF);   

				if (migr_migrate_frame(pfd, lpfd) != 0) {

					/*
					 * Restart the old pfn.
					 */
					pfms = PFN_TO_PFMS(pfdattopfn(pfd));
					migr_restart(pfn, pfms);
					/*
					 * Free the rest of the pages in
				 	 * the large page.	
					 */
					while (npgs--)
						pagefree(lpfd++);
					if (!is_even_page) {
						pmap_downgrade_lpage_pte(pas, 
							aligned_addr - 
							page_size, 
							pt - num_page_clicks);
					}
					return EAGAIN;
				}

				/*
			 	 * Restart the new pfn.
				 */
				pfn = pfdattopfn(lpfd);
				pfms = PFN_TO_PFMS(pfn);
				migr_restart_clear(pfn, pfms);
				/*
				 * Set the ptes to the new page size.
				 * Migration cleared the valid bits and so
				 * the processes are forced to take a vfault().
				 * They will wait on the region lock.
				 */

			        /*
                                 * If the ptes differ only in the mod bit
                                 * clear only the mod bit for all the ptes.
                                 */
				pg_set_page_mask_index(tmp_pt, page_mask_index);
				pg_setsftval(tmp_pt);
				if (mod_bit_differs)
                                        pg_clrmod(tmp_pt);
				pagefree(pfd);

			}
		}
	}
	return 0;
}
