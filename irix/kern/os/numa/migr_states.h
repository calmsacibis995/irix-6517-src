/*****************************************************************************
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
 ****************************************************************************/

#ifndef __MIGR_STATES_H__
#define __MIGR_STATES_H__

#ifdef	NUMA_BASE

extern void
migr_start(pfn_t swpfn, struct pm* pm);

extern void
migr_restart(pfn_t swpfn, pfms_t pfms);

extern void
migr_restart_clear(pfn_t swpfn, pfms_t pfms);

extern void 
migr_stop(pfn_t swpfn);

extern int
migr_xfer_nested_lock(pfn_t old_pfn);

extern void
migr_xfer_and_nested_unlock(pfn_t old_pfn, pfn_t new_pfn);

#else /* ! NUMA_BASE */

#define	migr_restart(pfn, pfms)
#define migr_restart_clear(swpfn, pfms)

#endif /* ! NUMA_BASE */

#endif /* __MIGR_STATES_H__ */






