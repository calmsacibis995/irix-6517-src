/*
 * os/numa/pm.h
 *
 *
 * Copyright 1995-1996, Silicon Graphics, Inc.
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

#ifndef __NUMA_PM_H__
#define __NUMA_PM_H__

#include <ksys/as.h>
#include <sys/numa.h>

struct pmo_ns;
/*
 * pm method
 */
typedef void*  (*pfmethod_t)(struct pm* this, void* args1, void* args2);

/*
 * Policy Module
 * The service methods include the destructor and the identifier
 * for each policy.
 */

typedef enum {
        PM_SRVC_DESTROY,      /* destructor for every policy */
        PM_SRVC_GETNAME,      /* name for every policy */
        PM_SRVC_GETAFF,       /* affinity info returned by replication policy */
	PM_SRVC_SYNC_POLICY,  /* Check if physical pages conform to the policy
			       * and replace them if they are not.
			       */
        PM_SRVC_GETMIGRPARMS, /* get parameters for migration */
        PM_SRVC_RELOCATE,     /* relocate the PMO's associated with a pm */
	PM_SRVC_GETARG,       /* get argument for creating module */
        PM_SRVC_ATTACH,       /* actions on pm_attach */
        PM_SRVC_DECUSG        /* decrement pm usage*/
} pm_srvc_t;

/*
 * Caller type for afflink
 */

typedef enum {
        AFFLINK_NEWPROC_SHARED,
        AFFLINK_NEWPROC_PRIVATE,
        AFFLINK_EXEC
} aff_caller_t;

typedef struct pm {
        /*
         * Pmo base
         */
        pmo_base_t base;                             /* ref counting */

        /*
         * Defined by Placement Policy
         */

        /* alloc page */
        pfd_t* (*pagealloc)(struct pm*, uint, int, size_t*, caddr_t vaddr);
        /* aff link */
        void (*afflink)(struct pm*, aff_caller_t, kthread_t*, kthread_t*);
        /* placement policy srvcs */
        int (*plac_srvc)(struct pm*, pm_srvc_t, void*, ...);
        /* private placpol data */
        void* plac_data;                             
        
        /*
         * Defined by Fallback Policy
         */
        pfd_t* (*fallback)(struct pm*, uint, int, struct mld*, size_t*); 
        int (*fbck_srvc)(struct pm*, pm_srvc_t, void*, ...);
        void* fbck_data;   
        
        /*
         * Defined by Replication Policy
         */
        int (*repl_srvc)(struct pm*, pm_srvc_t, void*, ...);
        void* repl_data;                   
        
        /*
         * Defined by Migration Policy
         */
        int (*migr_srvc)(struct pm*, pm_srvc_t, void*, ...);
        void* migr_data;     
        
        /*
         * Defined by Paging Policy
         */

        /* Nothing for paging yet */
        
        /*
         * Page Size
         */
        size_t page_size;

        /*
         * Policy modifiers
         */
        short policy_flags;

	/*
 	 * Page alloc wait timeout value.
	 */
	short page_alloc_wait_timeout;
        /*
         * Mlds used by this pm.
         * Normally this pmo is an mld
         * or an mldset. More complex objects
         * can be constructed and referenced by
         * this pmo pointer.
         */
        void* pmo;

        /*
         * Synchr for policy ops
         */
        mrlock_t mrlock; 
        
} pm_t;

#define pm_mrupdate(pm) mrupdate(&(pm)->mrlock)
#define pm_mrunlock(pm) mrunlock(&(pm)->mrlock)
#define pm_mraccess(pm) mraccess(&(pm)->mrlock)
#define pm_mraccunlock(pm) mraccunlock(&(pm)->mrlock)

struct	pregion;
struct pageattr;

typedef struct pm_setas_s {
	struct  pregion  *prp;
	caddr_t vaddr;
	size_t  len;
	struct pageattr	*attr;
} pm_setas_t;

/*
 * Limits for state arg lists
 */
#define PM_LIMIT_MAX_HANDLES       1024
#define PM_LIMIT_MAX_PGINFO_ELEMS  1024

/*
 * Management methods
 */

extern pm_t* pm_create(policy_set_t* policy_set,
                       struct pmo_ns* pmo_ns,
                       int* perrcode,
		       void *pmo);
extern pmo_handle_t pm_create_and_export(policy_set_t* policy_set,
                                         struct pmo_ns* pmo_ns,
                                         pm_t** pppm,
					 pmo_handle_t *);
extern void pm_decusg(pm_t* pm);
extern void pm_destroy(pm_t* pm);

#define pmo_kernel_pm_create(x, y, z) \
        pm_create_and_export((x), (y), (z), NULL)

/*
 * User xface
 */
extern pmo_handle_t pmo_xface_pm_create(pm_info_t* pm_info_arg, pmo_handle_t *);
extern pmo_xface_pm_destroy(pmo_handle_t pm_handle) ;
extern pmo_handle_t pmo_xface_pm_attach(pmo_handle_t handle, vrange_t* vrange_arg);
extern pmo_handle_t pmo_xface_pm_getpmhandles(vrange_t* vrange_arg,
                                              pmo_handle_list_t* pmo_handle_list_arg);
extern pmo_handle_t pmo_xface_pm_stat(pmo_handle_t handle_arg, pm_stat_t* pm_stat_arg);
extern pmo_handle_t pmo_xface_pm_set_pagesize(pmo_handle_t handle, size_t page_size);
extern pmo_xface_get_pginfo(vrange_t *vrange_arg, 
					pm_pginfo_list_t *pm_pginfo_list_arg);
/*
 * Interfaces to support checkpointing
 */
extern pmo_handle_t pmo_ckpt_pm_getpmhandles(asid_t, vrange_t* vrange_arg,
                                        pmo_handle_list_t* pmo_handle_list_arg,
					int *);
#ifdef CKPT
extern int pm_ckpt_get_pginfo(vasid_t vasid, vrange_t vrange_arg,
					pm_pginfo_list_t pginfo_list);
extern pmo_handle_t pmo_ckpt_pmo_nexthandle(int, asid_t, pmo_handle_t, pmo_type_t, int *);
extern pmo_handle_t pmo_ckpt_pm_info(asid_t, pmo_handle_t handle_arg,
					pm_info_t *, pmo_handle_t *);
#endif

#endif /* __NUMA_PM_H__ */
