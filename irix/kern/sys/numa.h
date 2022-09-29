/*
 * sys/numa.h
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

#ifndef __SYS_NUMA_H__
#define __SYS_NUMA_H__


#include <sys/pmo.h>
#include <sys/mmci.h>
#include <sys/pfdat.h>
#include <sys/kthread.h>

/******************************************************************/
/*                     EXPORTED NUMA_BASE XFACES                  */
/******************************************************************/

typedef void *(selector_t)(cnodeid_t, void*, void*);
extern void* physmem_select_neighbor_node(cnodeid_t center,
                                          int radius,
                                          cnodeid_t* neighbor,
                                          selector_t selector,
                                          void* arg1,
                                          void* arg2);
extern void* physmem_select_masked_neighbor_node(cnodeid_t center,
                                          int radius,
					  cnodemask_t cnodemask,
                                          cnodeid_t* neighbor,
                                          selector_t selector,
                                          void* arg1,
                                          void* arg2);
extern int physmem_maxradius(void);	
extern cnodemask_t physmem_find_node_cluster(cnodemask_t in_mask, int n);
extern cnodemask_t physmem_find_cpu_cluster(cpumask_t cpumask, int ncpus);

/***********************************************************
 *                   MLD PUBLIC KERNEL XFACES              *
 ***********************************************************/

struct mld;
typedef struct mld* mldref_t;

cnodeid_t mld_getnodeid(mldref_t);
int mld_isplaced(mldref_t);

/***********************************************************
 *                 MLDSET PUBLIC KERNEL XFACES             *
 ***********************************************************/

struct mldset;
typedef struct mldset* mldsetref_t;

/***********************************************************
 *                 PMO NS PUBLIC KERNEL XFACES             *
 ***********************************************************/
void* curpmo_ns_find(pmo_handle_t, pmo_type_t);

/*
 * Migration interfaces.
 */
#define CALLER_AUTOMIGR  0
#define CALLER_USERMIGR  1
#define CALLER_COALDMIGR 2
#define MIGR_PFDAT_XFER(x, y)      migr_pfdat_xfer((x), (y))

extern int migrate_page_test(uint old_pfn);
extern int migr_pfdat_xfer(pfd_t* old_pfd, pfd_t* new_pfd);
extern int migr_page_migrate(pfd_t* old_pfd, pfd_t* new_pfd);
extern void migr_mark_unmigratable(pde_t *pt) ;

typedef uint pfmsv_t;
typedef pfmsv_t* pfms_t;

extern pfmsv_t pflip_pfms_get(pfd_t* old_pfd);
extern void pflip_pfms_set(pfd_t* new_pfd, pfmsv_t new_pfmsv);

#ifdef NUMA_BASE
#define PFLIP_PFMS_GET(pfd)          pflip_pfms_get((pfd))
#define PFLIP_PFMS_SET(pfd, pfmsv)   pflip_pfms_set((pfd), (pfmsv))
#else
#define PFLIP_PFMS_GET(pfd)          (0)
#define PFLIP_PFMS_SET(pfd, pfmsv)
#endif


#ifdef NUMA_BASE

/*
 * Numa module initialization
 */
extern void numa_init(void);
extern int memoryd_init(void);

/*
 * Physical Memory Scheduler
 */
typedef void *(lbfunc_t)(cpuid_t, void *);

extern int physmem_max_radius;

#define MAX_DIST 255
extern int node_distance(cnodeid_t n0, cnodeid_t n1);

extern void* memaff_getbestnode(kthread_t*, lbfunc_t, void *, int);
extern int memaff_getaff(kthread_t*, cnodeid_t);
extern cnodeid_t memaff_nextnode(cnodeid_t cnode);
extern void memaff_alert(kthread_t*, cnodeid_t cnode);

/*
 * Process relocation when it is mustrun-ed
 */
extern void pmo_process_mustrun_relocate(struct uthread_s* uthreadp, cnodeid_t dest_node);

/*
 * Kernel Migration & Replication Xface
 */


struct migr_page_list_s;
struct local_router_state_s;
struct migr_control_data_s;
struct repl_control_data_s;

extern void
migr_migrate_page_list(struct migr_page_list_s* migr_page_list,
                       int len,
                       int caller);
extern int migrate_page_test(uint old_pfn);
extern int migr_check_migratable(pfn_t pfn);
extern int migr_pfdat_xfer(pfd_t* old_pfd, pfd_t* new_pfd);
extern int migr_page_migrate(pfd_t* old_pfd, pfd_t* new_pfd);
extern int migr_engine(pfn_t old_pfn, cnodeid_t dest_node, int caller);
extern void migr_pagemove(pfd_t* old_pfd, pfd_t* new_pfd, void* cookie, int migr_vehicle);
extern int migr_check_migr_poisoned(paddr_t);
extern void migr_page_unmap_tlb(paddr_t, caddr_t);

#ifdef SN0
extern void migr_intr_handler(void);
extern void migr_intr_prologue_handler(void);
#endif

extern void migr_start(pfn_t swpfn, struct pm* pm);
extern void migr_stop(pfn_t swpfn);
extern int numa_tune(int cmd, char* nodepath, void* arg);
extern int numa_stats_get(char* nodepath, void* arg);
extern int numa_tests(int cmd, cnodeid_t apnode, void* arg);
extern void nodepda_numa_init(cnodeid_t node, caddr_t *nextbyte);
extern void idbg_pfms(pfn_t swpfn);
extern int idbg_mldprint(struct mld* mld);
/*
 * Process relocation
 */
#define PMO_PROCESS_MUSTRUN_RELOCATE(ut, n)  pmo_process_mustrun_relocate((ut), (n))



#else /* !NUMA_BASE */



extern void uma_init(void);

#define migr_start(swpfn, pm)
#define migr_stop(swpfn)
#define numa_tune( cmd, nodepath, arg)
#define numa_stats_get(nodepath, arg)
#define numa_tests( cmd, apnode, arg)
#define nodepda_numa_init(node, nextbyte)


#define PMO_PROCESS_MUSTRUN_RELOCATE(ut, n)

#endif /* !NUMA_BASE */



#endif /* __SYS_NUMA_H__ */
