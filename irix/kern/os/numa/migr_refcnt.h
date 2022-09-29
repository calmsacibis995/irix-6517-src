/*****************************************************************************
 * Copyright 1997, Silicon Graphics, Inc.
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

#ifndef __NUMA_MIGR_REFCNT_H__
#define __NUMA_MIGR_REFCNT_H__

extern int migr_refcnt_init(cnodeid_t node);
extern void migr_refcnt_reset_counters(pfn_t swpfn);
extern void migr_refcnt_dealloc(cnodeid_t node);
extern void migr_refcnt_update_counters(cnodeid_t home_node,
                                        pfn_t swpfn,
                                        pfmsv_t pfmsv,
                                        uint new_mode);
extern void migr_refcnt_restart(pfn_t swpfn, pfmsv_t pfmsv, uint reset);

#ifndef NUMA_BASE
#define  MD_PROT_MIGMD_OFF 0
#endif

#endif /* __NUMA_MIGR_REFCNT_H__ */
