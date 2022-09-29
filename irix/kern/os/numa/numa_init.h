/*
 * Copyright 1995, 1996,  Silicon Graphics, Inc.
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

#ifndef __NUMA_INIT_H__
#define __NUMA_INIT_H__

extern int mld_init(void);
extern int raff_init(void);
extern int mldset_init(void);
extern int pmolist_init(void);
extern int memsched_init(void);
extern int mem_tick_init(void);
extern int topology_init(void);
extern int memoryd_init(void);
extern int plac_policy_default_init(void);
extern int plac_policy_fixed_init(void);
extern int plac_policy_firsttouch_init(void);
extern int plac_policy_thread_init(void);
extern int plac_policy_ccolor_init(void);
extern int plac_policy_roundrobin_init(void);
extern int plac_policy_cachecolor_init(void);
extern int fbck_policy_default_init(void);
extern int fbck_policy_local_init(void);
extern int migr_policy_default_init(void);
extern int migr_policy_control_init(void);
extern int migr_policy_refcnt_init(void);
extern int repl_policy_one_init(void);
extern int mem_profiler_init(void);

#endif /* __NUMA_INIT_H__ */
