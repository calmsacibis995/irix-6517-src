/*
 * sys/mmci.h
 *
 *
 * Copyright 1996-1997 Silicon Graphics, Inc.
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

#ifndef __SYS_MMCI_H__
#define __SYS_MMCI_H__

#ifdef _KERNEL

#include <sys/kthread.h>

/*
 * PMO Object Base
 */
typedef void (*pmo_method_t)(void*, ...);

typedef struct pmo_base {
        pmo_method_t incref;     /* incr ref count */
        pmo_method_t decref;     /* decr ref count */
        pmo_method_t destroy;    /* destroy pmo */
        pmo_type_t   type;       /* pmo type */
        int          refcount;   /* reference count */
#ifdef  MMCI_REFCD
        int          refcdi;     /* ref counting debug */
#endif         
} pmo_base_t;

#ifdef MMCI_REFCD

/*
 * Reference count debugging version
 */
#define pmo_incref(pmo, rop)  ((*((pmo_base_t*)(pmo))->incref)(pmo, rop, __FILE__, __LINE__))
#define pmo_decref(pmo, rop)  ((*((pmo_base_t*)(pmo))->decref)(pmo, rop, __FILE__, __LINE__))

#else  /* !MMCI_REFCD */

/*
 * Plain version
 */
#define pmo_incref(pmo, rop)  ((*((pmo_base_t*)(pmo))->incref)(pmo))
#define pmo_decref(pmo, rop)  ((*((pmo_base_t*)(pmo))->decref)(pmo))

#endif /* !MMCI_REFCD */


#define pmo_destroy(pmo) ((*((pmo_base_t*)(pmo))->destroy)(pmo))
#define pmo_gettype(pmo) (((pmo_base_t*)(pmo))->type)
#define pmo_getref(pmo)  (((pmo_base_t*)(pmo))->refcount)

/*
 * Reference Object Pointers for non-object cases
 */
#define pmo_ref_find (void*)1LL
#define pmo_ref_keep (void*)2LL
#define pmo_ref_list (void*)3LL
#define pmo_ref_attr (void*)4LL

/*
 * Declarations for referenced external structs
 */

struct pmo_ns;           /* name space for pmo's */
struct pageattr;         /* page attributes */
struct pfdat;            /* physical page frame */
union rval;              /* syscall ret val */
struct aspm;             /* per process pm root */
struct vnode;            /* vnode */
struct pm;               /* policy module */


/*
 * Main mmci call
 */
extern int pmoctl(int opcode, void* inargs, void* inoutargs, union rval* rvp);

/*
 * MMCI initialization
 */
extern void pmo_init(void);


/*
 * Kernel aspm xface
 */
extern struct pm* aspm_getcurrentdefaultpm(struct aspm* aspm, mem_type_t);
extern struct pm* aspm_getdefaultpm_func(struct aspm* aspm, mem_type_t);
extern int aspm_isdefaultpm_func(struct aspm *, struct pm * , mem_type_t );
extern void aspm_print(struct aspm *);
#ifdef CKPT
extern void aspm_ckpt_swap(void);
#endif

/*
 * Kernel pm xface
 */

typedef int (*pmfactory_t)(struct pm*, void* args1, struct pmo_ns* pmo_ns);
extern void pmfactory_export(pmfactory_t pmfactory, char* name);
extern struct pfdat* pmat_pagealloc(struct pageattr* attr,
                                    uint ckey,
                                    int flags,
                                    size_t* ppsz,
                                    caddr_t vaddr);
extern struct pfdat* pm_pagealloc(struct pm* pm,
                                  uint ckey,
                                  int flags,
                                  size_t* ppsz,
                                  caddr_t vaddr);
extern size_t pmat_get_pagesize(struct pageattr* attr);
extern size_t pm_get_pagesize(struct pm* pm);
extern	int   pm_sync_policy(caddr_t, size_t);
extern int 	pm_page_alloc_wait_timeout(struct pm* pm);


/*
 * aspm xface
 */

/*
 * Attach affinity links on fork
 */
extern void kthread_afflink_new(kthread_t*, kthread_t*, struct pm*);

/*
 * Decrement old affinity link usage
 */
extern void kthread_afflink_decusg(kthread_t*, asid_t*);

/*
 * Detach old affinity links and attach new ones on exec
 */
extern void kthread_afflink_exec(kthread_t*, struct pm*);

/*
 * Detach afflink on kthread destruction
 */
extern void kthread_afflink_unlink(kthread_t*, asid_t*);

/*
 * Create new aspm when exec-ing
 */
struct aspm* aspm_exec_create(struct vnode* vp, int is_mustrun);

/*
 * Dup an aspm. Used at fork time.
 */
struct aspm* aspm_dup(struct aspm *);

/*
 * Create base aspm for init
 */
extern struct aspm *aspm_base_create(void);

/*
 * Aspm destroy.
 */
extern void aspm_destroy(struct aspm *aspm);

/*
 * Get and release the default pm to pass to functions needing a pm
 */
#define aspm_getdefaultpm(aspm, type)     aspm_getdefaultpm_func((aspm), (type))
#define aspm_releasepm(pm)          {ASSERT((pm)); pmo_decref((pm), (pm));}
#define aspm_isdefaultpm(aspm, pm, mem_type)    \
                        aspm_isdefaultpm_func(aspm, pm, mem_type)

/*
 * pm  macros
 */
#define PMAT_PAGEALLOC(attr, ckey, flags, ppsz, vaddr)  \
        (pmat_pagealloc((attr), (ckey), (flags), (ppsz), (vaddr)))
#define PM_PAGEALLOC(pm, ckey, flags, ppsz, vaddr)  \
        (pm_pagealloc((pm), (ckey), (flags), (ppsz), (vaddr)))

#ifdef  PTE_64BIT
#define PMAT_GET_PAGESIZE(attr)    (pmat_get_pagesize(attr))
#define PM_GET_PAGESIZE(pm)       (pm_get_pagesize(pm))
#define PM_PAGE_ALLOC_WAIT_TIMEOUT(pm)	(pm_page_alloc_wait_timeout(pm))
#else
#define PMAT_GET_PAGESIZE(attr)    NBPP
#define PM_GET_PAGESIZE(attr)      NBPP
#define PM_PAGE_ALLOC_WAIT_TIMEOUT(pm)	0
#endif

#define  PM_SYNC_POLICY(vaddr, len)      pm_sync_policy((vaddr), (len))

/* 
 * Default page size value to pass to PMAT_PAGEALLOC.
 * PM_PAGEALLOC will allocate page of page size specified in the pm and
 * return it in the page size parameter.
 */
#define PM_PAGESIZE               (0)


#ifdef DONT_USE_MMCI

#define PMAT_PAGEALLOC(attr, ckey, flags, ppsz, vaddr) (ASSERT(ppsz), *ppsz = _PAGESZ, \
                                                       pagealloc((ckey), (flags)))
#define PM_PAGEALLOC(pm, ckey, flags, ppsz, vaddr)     (ASSERT(ppsz), *ppsz = _PAGESZ, \
                                                       pagealloc((ckey), (flags)))
#define PMAT_GET_PAGESIZE(attr)		               NBPP
#define PM_GET_PAGESIZE(pm)		               NBPP
#define PM_PAGESIZE                                    (0)

#define  PM_SYNC_POLICY(vaddr, len)                    (EINVAL)

#endif /* DONT_USE_MMCI */

#endif /* _KERNEL */

#endif /* __SYS_MMCI_H__ */
