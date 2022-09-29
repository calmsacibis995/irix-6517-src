/*****************************************************************************
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
 ****************************************************************************/

/* exported interface for migration bounce control module */

#ifndef __MIGRATION_BOUNCE_H__
#define __MIGRATION_BOUNCE_H__

typedef enum {MIGRATABLE, DAMPENED, FROZEN} migr_page_state_t;

extern pfn_t migr_periodic_bounce_control(cnodeid_t nodeid,
                                          pfn_t startpfn,
                                          pfn_t numpfns);
extern migr_page_state_t
migr_manager_bounce_control(pfms_t pfms, pfn_t swpfn, cnodeid_t source_nodeid);


#endif /* __MIGRATION_BOUNCE_H__ */




