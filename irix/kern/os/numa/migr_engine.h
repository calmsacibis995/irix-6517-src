/*
 * os/numa/migr_engine.h
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

#ifndef __PAGE_MIGRATION_ENGINE_H__
#define __PAGE_MIGRATION_ENGINE_H__

/*
 * Mechanism to be used for migration.
 * This value is available via the tuneable migr_mechanism_mode
 */

#define	MIGR_VEHICLE_IS_BTE		0
#define	MIGR_VEHICLE_IS_TLBSYNC		1

#define	MIGR_VEHICLE_BTE(x)	((x) == MIGR_VEHICLE_IS_BTE)
#define	MIGR_VEHICLE_TLBSYNC(x) ((x) == MIGR_VEHICLE_IS_TLBSYNC)

#define MIGR_TLB_SYNC()     tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM)


#if SN0_USE_BTE

#include <sys/SN/SN0/bte.h>

#define BTE_ACQUIRE()       bte_acquire(0)
#define BTE_RELEASE(x)	    bte_release(x)
#define	MIGR_VEHICLE_TYPE_GET()		\
		(NODEPDA_MCD(cnodeid())->migr_system_kparms.migr_vehicle)
#define BTE_POISON_COPY(c, t, v, w, x, y)			\
		if (MIGR_VEHICLE_BTE(t)) {			\
			int err;				\
			if (err = bte_poison_copy((c), (v), (w), (x), (y))) { \
				cmn_err(CE_PANIC, "[page_migrate]: Failed bte_copy 0x%x", err); \
			} 					\
		} else if (MIGR_VEHICLE_TLBSYNC(t)) {		\
			bte_pbcopy((v), (w), (x));	\
		} else {					\
			cmn_err(CE_PANIC, 			\
			    "[page_migrate]: Invalid Migration Vehicle %d\n", \
					numa_migr_vehicle);	\
		}

#else  

/* #define MIGR_POISONED_FLAG  0 */
#define BTE_ACQUIRE()       (void *)(__uint64_t)(1)
#define BTE_RELEASE(x)
#define MIGR_VEHICLE_TYPE_GET()		(MIGR_VEHICLE_IS_TLBSYNC)
#define BTE_POISON_COPY(c, t, v, w, x, y)			\
			bcopy((void *)(v), (void *)(w), (x))
#endif


extern int 
migr_migrate_page(pfn_t old_pfn, cnodeid_t dest_node, pfn_t* dest_pfnp);

extern int
migr_engine(pfn_t old_pfn, cnodeid_t dest_node, int caller);

extern int
migr_check_migratable(pfn_t pfn);

extern void
migr_migrate_page_list(struct migr_page_list_s *migr_page_list, int len, int caller);

extern int
migr_fastcheck_migratable(pfn_t pfn);

#endif /* __PAGE_MIGRATION_ENGINE_H__ */
