/*
 * ksys/migr.h
 *
 *
 * Copyright 1996, Silicon Graphics, Inc.
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

#ifndef __SYS_MIGR_H__
#define __SYS_MIGR_H__

typedef struct  migr_page_list_s {
	pfd_t   *old_pfd;
	pfd_t   *new_pfd;
	short   migr_err;
	cnodeid_t dest_node;
} migr_page_list_t;

/*
 * Migration failure codes
 */

#define MIGRERR_RAWCNT       1
#define MIGRERR_NOMEM        2
#define MIGRERR_ONELOCK      3
#define MIGRERR_REFS         4
#define MIGRERR_PBAD         5
#define MIGRERR_NOPFDAT      6
#define MIGRERR_BTE          7
#define MIGRERR_INSERTLOCK   8
#define MIGRERR_ZEROLINKS    9
#define MIGRERR_UNDONE       10
#define MIGRERR_LPAGE        11
#define MIGRERR_RMAP         12
#define MIGRERR_MOMUTATION   13
#define MIGRERR_NOMIGR       14
#define MIGRERR_PFMSSTATE    15
#define MIGRERR_PTEXT        16

#define MIGRERR_MAX          17

#ifdef SN0_USE_BTE
#define MIGR_POISONED_FLAG  P_POISONED
#else
#define MIGR_POISONED_FLAG  0
#endif

extern int
migr_check_migratable(pfn_t pfn);

extern int
migr_coald_test_pagemoves(pfd_t *start_pfd, bitmap_t bm, pgszindx_t pindx);

extern int
migr_coald_move_pages(cnodeid_t node,
                      pfd_t* start_pfd,
                      bitmap_t bm,
                      pgszindx_t pindx,
                      pfd_t** pmig_list);

extern int
migr_migrate_frame(pfd_t* old_pfd, pfd_t* new_pfd);


#endif /* __SYS_MIGR_H__ */
