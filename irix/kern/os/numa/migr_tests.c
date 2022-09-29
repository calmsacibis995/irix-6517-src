

/*
 * os/numa/migration_tests.c
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
#include <sys/immu.h>	
#include <sys/cmn_err.h>
#include <sys/pfdat.h>
#include <sys/pda.h>
#include <sys/kmem.h>
#include <ksys/rmap.h>
#include <sys/systm.h>	/* splhi */
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <ksys/vproc.h>
#include <sys/numa.h>
#include <sys/page.h>
#include <sys/lpage.h>
#include <ksys/migr.h>
#include "migr_engine.h"
#include "debug_levels.h"

#define ptoa(page_number) ((void*)((__psint_t)(page_number) << PNUMSHFT))

extern lock_t	memory_lock;

int
migrate_page_test(uint old_pfn)
{
        pfd_t* old_pfd;
        pfd_t* new_pfd;
        pfn_t  new_pfn;
        int    errcode;
        int    error_count;
        int    s;

        old_pfd = pfntopfdat(old_pfn);
        
        /*
         * We need to allocate a new page.
         * For testing functionality we don't need this
         * page to come from a different node.
         */

        new_pfd = pagealloc(pfntocachekey(old_pfn, NBPP), 0);
        if (new_pfd == NULL) {
                numa_message(DM_MIGR,
                             2,
                             "[migrate_page_test]: page_alloc failed", 0, 0);
                return (MIGRERR_NOMEM);
        }

        new_pfn = pfdattopfn(new_pfd);

        ASSERT(curuthread);
        s = RMAP_LOCK(old_pfd);
        error_count = rmap_scanmap(old_pfd,
                                   RMAP_VERIFYLOCKS,
                                   (void*)(__uint64_t)current_pid());
        RMAP_UNLOCK(old_pfd, s);
        if (error_count != 0) {
                printf("CASE1: error_count=%d, old_pfn = 0x%x\n", error_count, old_pfn);
                ASSERT(0);
        }
        

        if ((errcode = migr_migrate_frame(old_pfd, new_pfd)) != 0) {
                pagefree(new_pfd);

                ASSERT(curuthread);
                s = RMAP_LOCK(old_pfd);
                error_count = rmap_scanmap(old_pfd,
                                           RMAP_VERIFYLOCKS,
                                           (void*)(__uint64_t)current_pid());
                RMAP_UNLOCK(old_pfd, s);
                if (error_count != 0) {
                        printf("CASE2: error_count=%d, old_pfn = 0x%x\n", error_count, old_pfn);
                        printf("errcode=%d\n", errcode);
                        ASSERT(0);
                }

                return (errcode);
        }

        s = RMAP_LOCK(new_pfd);
        error_count = rmap_scanmap(new_pfd,
                                   RMAP_VERIFYLOCKS,
                                   (void*)(__uint64_t)current_pid());
        RMAP_UNLOCK(new_pfd, s);
        if (error_count != 0) {
                printf("CASE5: error_count=%d, new_pfn = 0x%x\n", error_count, new_pfn);
                ASSERT(0);
        }

        
        pagefree(old_pfd);
        
        return (0);
}

