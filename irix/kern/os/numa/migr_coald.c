/*
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
/*
 * Try to move the pages that are not free. pmig_list  contains a list of 
 * pages that have been migrated. Returns -1 if pages cannot be moved.
 * pmig_list contains a list of pages that have been moved. 
 * Returns number of pages on the migration list.
 */

#include <values.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/lpage.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/sysmacros.h>
#include <ksys/rmap.h>
#include "stdarg.h"
#include <sys/errno.h>
#include <sys/anon.h>
#include <sys/vnode.h>
#include <sys/nodepda.h>
#include <ksys/migr.h>
#include <sys/numa.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/sysinfo.h>
#include <sys/idbgentry.h>
#include <sys/pfdat.h>
#include "pfms.h"
#include <sys/numa.h>
#include "migr_control.h"
#include "migr_manager.h"
#include "migr_states.h"
#include "numa_hw.h"
#include "migr_queue.h"
#include "numa_stats.h"
#include "migr_engine.h"
#include "debug_levels.h"
#include "migr_refcnt.h"
#include "sys/tile.h"

#ifdef TILES_TO_LPAGES
#define MIGR_PAGEALLOC(node,pfn)	tile_migr_pagealloc(node, pfn)
#else
#define MIGR_PAGEALLOC(node,pfn)	_pagealloc_size_node(node,\
						pfntocachekey(pfn, NBPP), 0, NBPP)
#endif /* TILES_TO_LPAGES */

#define DBG3(x)
int migration_err_stat[MIGRERR_MAX];

/*
 * Test if the pages that are not free in the chunk can be moved. Returns
 * FALSE even if one page in the chunk cannot be migrated.
 */

int
migr_coald_test_pagemoves(pfd_t *start_pfd, bitmap_t bm, pgszindx_t pindx)
{
	pgno_t	clicks;
	int	start_chunk = pfdat_to_bit(start_pfd);
	int	numbits = PGSZ_INDEX_TO_CLICKS(pindx);
	int	end_chunk;
	pfd_t	*pfd;
	int	i;
	int	err;

	/*
	 * Test if the pages can be migrated.
	 */
	DBG3(lpgtrace("colchunk:Checking without locks start_chunk %x \n",
		start_chunk));

	pfd = start_pfd;
	end_chunk = start_chunk+numbits;
	for (i = start_chunk; i < end_chunk;i += clicks) {
		clicks = PFDAT_TO_PAGE_CLICKS(pfd);
		if (!btst(bm, i)) {
			if (pfd->pf_flags & P_DUMP) {
				DBG3(lpgtrace(
                                "test_move_page:pfd %x fails kernel page \n",
                                                                pfd));
				INC_LPG_STAT(pfdattocnode(pfd), pindx, 
							CO_DAEMON_KERNEL_PAGE);
				return FALSE;
			}

			if (err = migr_check_migratable(pfdattopfn(pfd))) {
#ifdef	LPG_STAT
				migration_err_stat[err]++;
				DBG3(lpgtrace(
				"can_migrate_page:pfd %x fails err %d\n", 
								pfd, err));
#endif
				/*
				 * Try  getting rid of the buffer reference
				 * for a vnode page and check to see if
				 * it is migratable.
				 */
				if (((err == MIGRERR_REFS) ||
				     (err == MIGRERR_ZEROLINKS)) && 
					((pfd->pf_flags & (P_ANON|P_HASH)) 
								== P_HASH)) {

					err = lpage_release_buffer_reference(pfd, FALSE);
					if (!err) {
						pfd += clicks;
						INC_LPG_STAT(pfdattocnode(pfd), 
							pindx, 
						CO_DAEMON_PUSH_SUCC);
						continue;
					}

				}
				INC_LPG_STAT(pfdattocnode(pfd), pindx, 
						CO_DAEMON_TEST_MIG_FAIL);
				return FALSE;
			} else {
				DBG3(lpgtrace(
				"can_migrate_page:pfd %x success\n", pfd));
				INC_LPG_STAT(pfdattocnode(pfd), pindx, 
						CO_DAEMON_TEST_MIG_SUCC);
			}
		}
		pfd += clicks;
	}
	return TRUE;
}



int
migr_coald_move_pages(cnodeid_t node,
                      pfd_t* start_pfd,
                      bitmap_t bm,
                      pgszindx_t pindx,
                      pfd_t** pmig_list)
{
	pgno_t	clicks;
	int	start_chunk = pfdat_to_bit(start_pfd);
	int	numbits = PGSZ_INDEX_TO_CLICKS(pindx);
	int	end_chunk = start_chunk + numbits;
	int	i;
	pfn_t	pfn;
	pfd_t	*pfd;
	int	num_mig_pages = 0;
	int	num_pages_to_migrate = 0;
	migr_page_list_t* migr_list;
        migr_page_list_t* mpl;
	size_t migr_list_size;
	/* REFERENCED */
        pfms_t pfms;
	/* REFERENCED */
        int pfms_spl;

	/*
	 * Now try migrating the pages.
	 */

	migr_list_size = PGSZ_INDEX_TO_CLICKS(pindx) * sizeof(migr_page_list_t);
	migr_list = kmem_alloc(migr_list_size,  KM_SLEEP);
	pfd = start_pfd;

	num_pages_to_migrate = 0;

	mpl = migr_list;
	for (i = start_chunk; i < end_chunk; i += clicks) {
		clicks = PFDAT_TO_PAGE_CLICKS(pfd);

		/*
		 * If page is migratable then note it down.
		 * Also note down the pages that are free.
		 * We have to wait until we have scanned the
		 * list as we can find a page that cannot
		 * be migrated in which case we have to undo
		 * free pages.
		 */

		if (pfd->pf_use && !btst(bm, i)) {
			pfn = pfdattopfn(pfd);
 
			/*
			 * Use _pagealloc_size_node which does not
			 * split a large page.
			 */

			mpl->new_pfd = MIGR_PAGEALLOC(node, pfn);

                        pfms = PFN_TO_PFMS(pfn);
                        PFMS_LOCK(pfms, pfms_spl);

                        if (!PFMS_STATE_IS_MAPPED(pfms) || mpl->new_pfd == NULL) {
                                PFMS_UNLOCK(pfms, pfms_spl);
#ifdef	LPG_STAT
				migration_err_stat[MIGRERR_NOMEM]++;
#endif
                                if (mpl->new_pfd != NULL) {
                                        pagefree(mpl->new_pfd);
                                }
                                
				mpl = migr_list;
				for (i = 0; i < num_pages_to_migrate; i++, mpl++) {
					/* REFERENCED */
                                        
                                        /*
                                         * We're restarting all pages we stopped
                                         * migration for in previous iterations
                                         * of this loop.
                                         */
                                        pfn_t old_pfn = pfdattopfn(mpl->old_pfd);
                                        migr_restart(old_pfn, PFN_TO_PFMS(old_pfn));
					pagefree(mpl->new_pfd);
                                }
				kmem_free(migr_list, migr_list_size);
				return -1;
			}

                        PFMS_STATE_MIGRED_SET(pfms);
                        PFMS_UNLOCK(pfms, pfms_spl);

                        migr_refcnt_update_counters(node,
                                                    pfn,
                                                    PFMS_GETV(pfms),
                                                    MD_PROT_MIGMD_OFF);       


			mpl->old_pfd = pfd;
			mpl->dest_node = node;
			mpl->migr_err = 0;
			num_pages_to_migrate++;
			mpl++;

			DBG3(lpgtrace("co1chunk:migrate pfd %x \n", pfd));
		}
		pfd += clicks;
	}

	migr_migrate_page_list(migr_list,
                               num_pages_to_migrate, 
                               CALLER_COALDMIGR);

        /*
         * Restart migration state
         */
        for (i = 0, mpl = migr_list; i < num_pages_to_migrate; i++, mpl++) {
                if (mpl->migr_err) {
                        /*
                         * We have to restart the old pfn
                         */
                        pfn = pfdattopfn(mpl->old_pfd);
                        pfms = PFN_TO_PFMS(pfn);
                        migr_restart(pfn, pfms);
                } else {
                        /*
                         * We have to restart the new pfn
                         */
                        pfn = pfdattopfn(mpl->new_pfd);
                        pfms = PFN_TO_PFMS(pfn);
                        migr_restart_clear(pfn, pfms);
                        NUMA_STAT_INCR(cnodeid(), migr_coalescingd_number_out);
                        NUMA_STAT_INCR(mpl->dest_node, migr_coalescingd_number_in);
                }
        }        
        
	mpl = migr_list;
	for (i = 0; i < num_pages_to_migrate; i++, mpl++) {
		if (mpl->migr_err) {
#ifdef  LPG_STAT
			migration_err_stat[mpl->migr_err]++;
#endif
			DBG3(lpgtrace(
                                "co1chunk:migrate pfd %x failed err %x \n", 
                                mpl->new_pfd, mpl->migr_err));
#ifdef	LPG_STAT
			INC_LPG_STAT(node, pindx, CO_DAEMON_MIG_FAIL);
#endif
			kmem_free(migr_list, migr_list_size);
			return -1;
		}

		/*
		 * We can modify the use count as this pfdat
		 * is not in anyone's list. Set it zero 
		 * as migr_migrate_page_list() set it to 1.
		 */

		pfd = mpl->old_pfd;
		pfd->pf_use = 0;
		pfd->pf_next = *pmig_list;
		*pmig_list = pfd;
		num_mig_pages++;

		INC_LPG_STAT(node, pindx, CO_DAEMON_MIG_SUCC);
	}

	kmem_free(migr_list, migr_list_size);
	return num_mig_pages;
}
