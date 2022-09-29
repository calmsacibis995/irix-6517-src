/*
 * File: partition.c
 * Purpose: Deal with memory partitioning on SN0.
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

#ident "$Revision: 1.24 $"

#include "sys/debug.h"
#include "sys/param.h"
#include "sys/cmn_err.h"
#include "sys/clksupport.h"
#include "sys/pfdat.h"
#include "sys/sema.h"
#include "sys/systm.h"
#include "sys/pda.h"
#include "sys/avl.h"
#include "sys/kmem.h"
#include "sys/partition.h"
#include "sys/errno.h"
#include "sys/iobus.h"

#include "ksys/xthread.h"
#include "ksys/partition.h"
#include "sys/EVEREST/evintr.h"

#include "ksys/cell.h"

#include "ksys/partition.h"
#include "ksys/sthread.h"

#define	PART_DEBUG	0

#if PART_DEBUG
#   define	DPRINTF(x)	part_debug x
#else
#   define	DPRINTF(x)
#endif

#define	MAX_NASIDS	EV_MAX_CPUS

/*
 * Define: 	PART_HUBREV
 * Purpose: 	Minimum value of the hub revision register version field
 *		required for partitioning to work. If ANY HUB in the 
 *		current partition is less than this value, partitioning
 *		is effectively disabled.
 */

#define	PART_RM_SETB(p, b)	((p)->p_regions |= (1UL << (b)))
#define	PART_RM_CLRB(p, b)	((p)->p_regions &= ~(1UL << (b)))
#define	PART_RM_TSTB(p, b)	((p)->p_regions & (1UL << (b)))
#define	PART_RM_CLRALL(p)	((p)->p_regions = (0))

/*
 * Stucture:	pts_t (Partition thread)
 * Purpose:	One such structure for each interrupt thread running around. 
 */
typedef	struct pts_s {
    struct pts_s	*pts_next,
                        *pts_prev;
    xthread_t		*pts_xthread;	/* current thread */
    __uint32_t		pts_flags;	/* Flags - set ONLY by thread */
#   define		PTS_F_READY	0x01 /* ready - ON partition Q */
#   define		PTS_F_ACTIVE	0x02 /* active */
    struct partition_s  *pts_partition; /* current partition */
} pts_t;

static char *part_state[] = {
    "pInvalid", "pIdle", "pActivateReq", "pActivateWait", "pActivate", 
    "pReady", "pDeactivateReq", "pDeactivate"
};

/*
 * Typedef: partition_t
 * Purpose: Defines a bunch of stuff for each partition.
 */
typedef struct partition_s {
    char		p_name[8];	/* "part_##" */
    lock_t		p_lock;		/* Lock for this struct */
    int			p_lock_spl;	/* locked previous spl */
    partid_t		p_id;		/* Partition ID */
    __uint64_t		p_regions;	/* Regions */
    enum {
	pInvalid,
	pIdle,				/* Idle */
	pActivateReq,			/* activation request */
	pActivateWait,			/* activation pending */
	pActivate,			/* activation in progress */
	pReady,				/* partition ready */
	pDeactivateReq,			/* deactivation in progress */
	pDeactivate			/* deactivation in progress */
    }			p_state;
    __uint64_t		p_int_count;	/* interrupt count */
    __uint64_t		p_int_spurious;	/* spurious interrupt count */
    void		(*p_int)(void *); /* interrupt handler */
    __uint32_t		p_page_permit;	/* # pages permitted */
    __uint32_t		p_page_register; /* # registered remote pages */
    int			p_xthread_cnt;	/* xthread count */
    int			p_xthread_limit; /* # threads allowed to sleep */
    int			p_xthread_idle;	/* # threads idle at current time */
    int			p_xthread_create_failed; /* failed creates */
    int			p_xthread_create; /* # created threads */
    int			p_xthread_exit; /* # terminated threads */
    sema_t		p_isema;	/* sema for interrupt handler */
    sv_t		p_xsv;		/* exit sema */
    pts_t		p_xthreads;	/* xthread queue */
} partition_t;

#define	P_LOCK(_p)	(_p)->p_lock_spl = mutex_spinlock(&(_p)->p_lock)
#define	P_UNLOCK(_p)	mutex_spinunlock(&(_p)->p_lock, (_p)->p_lock_spl)
#define	P_LOCKED(_p)	spinlock_islocked(&(_p)->p_lock)

/*
 * Typedef:	xp_t
 * Purpose:	Describes the cross partition page structures. Each local
 *		page permitted to remote pages is described by this 
 *		structure - with the "regions" field indicating the 
 *		regions the pages is permitted to. 
 *		
 *		Each Remote partition page is registered locally with the
 *		xp flags indicating it is XP_REGISTER.
 *
 *		An AVL tree of all cross partition pages, both 
 *		registered remote and locally exported, is kept in 
 *		"partition_pages". 
 */
typedef	struct xp_s {
    avlnode_t		xp_avlnode; 		/* Tree linkage */
    paddr_t		xp_addr;		/* Physical address */
    size_t		xp_size; 		/* Size in bytes */
    pfd_t		*xp_pfd; 		/* pfdat for owned page */
    partid_t		xp_id;			/* owning partition */
    part_error_handler_t xp_error;		/* error handler this page */
    __uint64_t		xp_regions;		/* Regions that can access */
    __uint32_t		xp_hold;	        /* Hold count */
    __uint32_t		xp_flags;		/* Flags */
#   define		XP_CLEANING	1	/* Page being cleaned */
#   define		XP_ERROR	2	/* Error on page */
#   define		XP_POISON	4	/* Page poisoned */
#   define		XP_REGISTER	8       /* Registers remote page */
} xp_t;

/*
 * Variable:	partition_pages
 * Purpose:	Used by AVL routines to keep track of all pages in this 
 *		partition that "may" be cross partition pages. 
 * 		(pfdat indicates P_XP).
 * Locks:	Protected by partition_page_lock (P_PAGE_LOCK).
 */
static avltree_desc_t	partition_pages;
static	mutex_t		partition_page_lock;

#define	P_PAGE_LOCK()	mutex_lock(&partition_page_lock, 0)
#define	P_PAGE_UNLOCK()	mutex_unlock(&partition_page_lock)
#define	P_PAGE_LOCKED()	mutex_owned(&partition_page_lock)

/*
 * Typedef:	part_status_t
 * Purpose:	Defines the Q entires for a list of activate/deactivate 
 *	 	routines called when a partition is declared Up/Down.
 */
typedef struct part_status_s {
    struct part_status_s *ps_next;
    void		(*ps_activate)(partid_t);
    void		(*ps_deactivate)(partid_t);
} part_status_t;

/*
 * Variable:	partition_handlers
 * Purpose:	The actual Q of activation/deactivate/handlers called 
 *		IN THE ORDER THEY ARE REGISTERED when a partition
 *		is activated ot deativated. 
 * Locks:	Protected by partition_handlers_lock
 * 
 * Notes:	If a partition is activated (and running) BEFORE a 
 *		handler is registered, the handler IS called upon 
 *		registration.
 */
static	part_status_t		*partition_handlers = NULL;
static	mrlock_t		partition_handlers_lock;
#define	P_LOCK_HANDLERS()	mraccess(&partition_handlers_lock)
#define	P_UNLOCK_HANDLERS()	mrunlock(&partition_handlers_lock)
#define	P_UPDATE_HANDLERS()	mrupdate(&partition_handlers_lock)

#define	MD_PREMIUM(a) \
    ((REMOTE_HUB_L(NASID_GET(a), MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM) \
        ? B_TRUE : B_FALSE)

partition_t	partition_master;
partition_t	partitions[MAX_PARTITIONS];
partid_t	partition_partid = INVALID_PARTID;
int		partition_inited[MAX_PARTITIONS] = { 0 };

/*
 * Variable: 	partition_nodes
 * Pupose:   	An array indexed by NASID which returns the partition ID of 
 *	     	the owning partition. By definition, all of the CPUs on each 
 *		node are in the same partition. 
 *		
 *		On SN0 - an entire module (8 cpus) is the granularity. 
 *		However, this level of software does NOT care, we 
 *		use the finest grain modularity - which is nodes in 
 *		fine grain mode. 
 */
static pn_t partition_nodes[MAX_NASIDS];

#define		NASID_TO_PARTID(n)	partition_nodes[n].pn_partid
#define		PARTID_VALID(n)		(((n) >= 0) && (n < MAX_PARTITIONS))

/*
 * Variable: 	partition_regions
 * Purpose:  	An array indexed by region which returns the partition ID of 
 *	    	the owning partition.
 */
static struct {
    __uint64_t	pr_int_count;		/* interrupt count */
    partition_t *pr_partition;		/* owning partition */
} partition_regions[MAX_REGIONS];

/* one initial value for partition_xp[] so it's not in BSS */
paddr_t partition_xp[EV_MAX_CELLS] = {0};

static 	__psunsigned_t	part_avl_start(avlnode_t *);
static 	__psunsigned_t	part_avl_end(avlnode_t *);
static	void		part_hold_page(xp_t *);
static	__uint32_t	part_release_page_nolock(xp_t *);
static	__uint32_t	part_release_page(xp_t *);
static	xp_t		*part_find_page(paddr_t);
static	xp_t		*part_new_page(pfd_t *, paddr_t, size_t, __uint32_t, 
				       part_error_handler_t);
        void		part_init_memory(void);
        void		part_set_memory_access(partition_t *, __uint32_t, 
					       pfd_t *, size_t);
static 	void		part_permit_xppage(partition_t *, xp_t *, 
					   pAccess_t);
        void		part_permit_page(partid_t, pfd_t *, pAccess_t);
        void		part_stop_permit_page(pfd_t *);
        void		part_clean_page(xp_t *);
        pfd_t		*part_page_allocate_node(cnodeid_t,partid_t,pAccess_t,
						 uint, int, size_t, 
						 part_error_handler_t);
        pfd_t		*part_page_allocate(partid_t, pAccess_t, uint, int, 
					    size_t, part_error_handler_t);
        void		part_page_free(pfd_t *, size_t);
static	void		part_interrupt_thread_exit(void *);
static  void		part_activate(void *);
        void		part_deactivate(partid_t);
static 	void		part_terminate(pts_t *pts);
static	void		part_probe_nasid(nasid_t);
static	void		part_discovery(void);
static	void		part_spurious_interrupt(void *);
static	void		part_interrupt_thread(void *);
static	void		part_interrupt_thread_setup(pts_t *);
static	void		part_interrupt(void *);
static	void		part_queue_xthread(pts_t *);
static	void		part_dequeue_xthread(pts_t *);
static  int 		part_start_partition_thread(partition_t *, 
						    int, boolean_t);
        boolean_t	part_start_thread(partid_t);
static	void		part_interrupt_reset(partition_t *);
        void		part_interrupt_set(partid_t, int, xt_func_t *);
        void		part_init(void);
        partid_t	part_from_region(__uint32_t);
        partid_t 	part_id(void);
static  partition_t	*part_get(partid_t);
        paddr_t 	part_find_xp(partid_t);
        void		part_set_xp(paddr_t);
        pn_t 		*part_get_nasid(const partid_t, nasid_t *);
        void		part_poison_page(pfd_t *);
        void		*part_register_page(paddr_t, size_t, 
					    part_error_handler_t);
        void		part_unregister_page(paddr_t);
        void		part_register_handlers(void (*)(partid_t), 
					       void (*)(partid_t));
        int		part_operation(int, int, void *, void *, rval_t *);
        void		part_page_dump(void (*)(char *, ...));
static	void		part_dump_partition(void (*)(char *, ...), 
					    partition_t *);
        void		part_dump(void (*)(char *, ...));


#if PART_DEBUG

static	void
part_debug(char *s, ...)
/*
 * Function: 	part_debug
 * Purpose:	To print debugging messages
 * Parameters:	s - template string (as to cmn_err)
 *		...  - any other args
 * Returns:	Nothing
 */
{
    va_list	ap;
    va_start(ap, s);
    icmn_err(CE_DEBUG, s, ap);
    va_end(ap);
}

#endif

static	__psunsigned_t
part_avl_start(avlnode_t *an)
/*
 * Function: 	part_avl_start
 * Purpose:	AVL support routine to compute the "start" of a region 
 *		from and avl node.
 * Parameters:	an - avl node
 * Returns:	Starting value of region.
 */
{
    return(((xp_t *)an)->xp_addr);
}

static	__psunsigned_t
part_avl_end(avlnode_t *an)
/*
 * Function: 	part_avl_end	
 * Purpose:	AVL support routine to compute "end" of a region from an
 *		avl node.
 * Parameters:	an - pointer to avl node in question
 * Returns:	End of region
 */
{
    return(((xp_t *)an)->xp_addr + ((xp_t *)an)->xp_size);
}

static	void
part_hold_page(xp_t *xp)
/*
 * Function: 	part_hold_page
 * Purpose:	To increment the hold cound on an xp page structure.
 * Parameters:	xp - pointer to xp page struct to hold.
 * Returns:	Nothing
 */
{
    ASSERT(P_PAGE_LOCKED());
    xp->xp_hold++;
}

static __uint32_t
part_release_page_nolock(xp_t *xp)
/*
 * Function: 	part_release_page_nolock
 * Purpose:	Release a reference to a xp page.
 * Parameters:	xp - pointer to xp page to be released.
 * Returns:	reference count AFTER release (0 --> page freed).
 */
{
    __uint32_t	rc;			/* reference count */

    ASSERT(xp->xp_hold);
    if (0 == (rc = --xp->xp_hold)) {
	(void)avl_delete(&partition_pages, &xp->xp_avlnode);
	kmem_free(xp, sizeof(*xp));
    }
    return(rc);
}

static	__uint32_t
part_release_page(xp_t *xp)
/*
 * Function: 	part_releae_page
 * Purpose:	To release a page reference, assuming the page lock is 
 *		not held.
 * Parameters:	xp - pointer to xp page structure.
 * Returns:	# references remain to page. 
 */
{
    __uint32_t	rc;

    P_PAGE_LOCK();
    rc = part_release_page_nolock(xp);
    P_PAGE_UNLOCK();
    return(rc);
}

static	xp_t	*
part_find_page(paddr_t pa)
/*
 * Function: 	part_find_page
 * Purpose:	Locate a xp page structure for a given physical address.
 * Parameters:	pa - physical address to look up.
 * Returns:	pointer to xp or NULL if not found.
 */
{
    xp_t	*xp;

    P_PAGE_LOCK();
    xp = (xp_t *)avl_findrange(&partition_pages, pa);
    if (xp) {
	part_hold_page(xp);
    }
    P_PAGE_UNLOCK();
    return(xp);
}

static	xp_t	*
part_new_page(pfd_t *pfd, paddr_t pa, size_t sz, __uint32_t flags, 
	      part_error_handler_t eh)
/*
 * Function: 	part_new_page
 * Purpose:	Create and add a new cross partition page structure.
 * Parameters:	pfd - pointer to pfd for page IFF page owned locally.
 *		      NULL otherwise.
 *		pa  - physical address of page
 *		sz  - size of page in bytes.
 *		flags-xp_flags values (XP_REGISTER if remote page 
 *		      registration).
 *		eh  - error handler for page.
 * Returns:	Pointer to xp structure for page.
 *		Null if partition no longer active.
 */
{
    xp_t	*xp= NULL;
    partition_t	*p = part_get(1);

    if (p) {
	P_LOCK(p);
	if (p->p_state == pReady) {
	    xp = kmem_alloc(sizeof(*xp), KM_SLEEP);
	    if (pfd) {
		xp->xp_addr = pfdattophys(pfd);
	    } else {
		xp->xp_addr = pa;
	    }
	    xp->xp_pfd   = pfd;
	    xp->xp_size  = sz;
	    xp->xp_flags = flags;
	    xp->xp_error = eh;
	    xp->xp_hold  = 1;
	    xp->xp_id    = p->p_id;
	    P_PAGE_LOCK();
	    (void)avl_insert(&partition_pages, &xp->xp_avlnode);
	    P_PAGE_UNLOCK();
	}
	P_UNLOCK(p);
    }
    return(xp);
}

void
part_init_memory(void)
/*
 * Function: 	part_init_node_memory
 * Purpose:	To set up the memory protection for all pages on a node. 
 *		Pages are permitted rw to this partition, none to all others.
 * Parameters:	
 * Returns:	Nothing.
 */
{
}

/*ARGSUSED*/
void
part_set_memory_access(partition_t *p, __uint32_t a, pfd_t *pfd, size_t size)
/*
 * Function: 	part_set_memoryaccess
 * Purpose:	To set access to memory in our partition, for another 
 *		partition.
 * Parameters:	part - partition to set access for. If NULL, then 
 *		       permissions are set for ALL regions.
 *		a - access to allow (MD_PROT_XX).
 *		pfd - pointer to pfdat for page to set permissions on.
 * Returns:	Nothing
 */
{
}

/*ARGSUSED*/
static void
part_permit_xppage(partition_t *p, xp_t *xp, pAccess_t a)
/*
 * Function: 	part_permit_xppage
 * Purpose:	DO actual permit work
 * Parameters:	p - pointer to partition to set permissions for.
 *		xp - pointer to cross partition page structure.
 *		a - access
 * Returns: nothing
 */
{
}    


/*ARGSUSED*/
void
part_permit_page(partid_t partid, pfd_t *pfd, pAccess_t a)
/*
 * Function: 	part_permit_page
 * Purpose:	To set the partition accesses to a page. 
 * Parameters:	partid	- partition id to set access for.
 *		pfd 	- pointer to PFD for page
 *		a	- access requested.
 * Returns:	Nothing.
 */
{
}

/*ARGSUSED*/
void
part_stop_permit_page(pfd_t *pfd)
/*
 * Function: 	part_stop_permit_page
 * Purpose:	Clean up permissions on a page that is being freed.
 * Parameters:	Pointer to pfdat.
 * Returns:	Nothing
 */
{
}

/*ARGSUSED*/
void
part_clean_page(xp_t *xp)
/*
 * Function: 	part_clean_page
 * Purpose:	Be sure no other partition has part/all of this page
 * Parameters:	xp - pointer to cross partition page descriptor.
 * Returns:	Nothing
 *
 * Notes: Cleaning a page is done as follows:
 *
 *		1) Mark page as RW to owning partition (us) ONLY. This
 *		   stops all accesses (demand and speculative) from 
 *		   other partitions. Write backs of exclusive lines
 *		   from another partition will cause the write back 
 *		   to be ignored, and the originating partition to 
 *		   get an error. The owning partition (we) do not
 *		   see this.
 *
 *		2) Read the entire page using the Coherent, Exclusive
 *		   on read attributes. This will cause interventions 
 *		   or invalidates to be sent based on the directory 
 *		   entires. All traffic within our partition is normal. 
 *		   Traffic that is sent outside our partition is OK - even
 * 		   though permissions do not allow accesses, intervention
 *		   and invalidation responses act normally.
 *
 *		   If the page is ever marked in error, then we stop
 *		   reading, because the error recovery code will have
 *	           poisoned it.
 *	
 *		3) At this point, our partition is the only partition that
 *		   has any of the page in question in our caches. 
 *
 * Errors: Errors are handled by partBusError. The XP_CLEANING flag
 *	   indicates the page is in the cleaning process. This is used
 *	   durning error recovery.
 */
{
}

/*ARGSUSED*/
pfd_t	*
part_page_allocate_node(cnodeid_t cn, partid_t partid, pAccess_t a, uint ck, 
			int flags, size_t sz, part_error_handler_t eh)
/*
 * Function: 	part_page_allocate_node
 * Purpose:	To allocate a page of memory for cross partition mapping.
 * Parameters:	cnodeid - compact nodeid indicating where to allocate memory
 *			  from.
 *		partid  - partition id that is allowed access to this page.
 *		ck	- cache key (same as pagealloc_size)
 *		flags	- (passed to pagealloc_size).
 *		size	- size of page requested (same as pagealloc_size).
 *		size 	- page size as passed to partPageAllocNode.
 *		eh	- error handler
 * Returns:	Nothing
 */
{
    pfd_t	*pfd;
    xp_t	*xp;

    if (pfd = pagealloc_size_node(cn, ck, flags, sz)) {
	xp = part_new_page(pfd, 0, sz, 0, eh);
	ASSERT(xp);
	part_permit_xppage(part_get(partid), xp, a);
    }
    return(pfd);
}

pfd_t	*
part_page_allocate(partid_t partid, pAccess_t a, uint ck, int flags, 
		   size_t sz, part_error_handler_t eh)
/*
 * Function: 	part_page_allocate
 * Purpose:	To allocate a page of memory for cross partition mapping.
 * Parameters:	partid  - partition id that is allowed access to this page.
 *		a	- access to allow partition specified.
 *		ck	- cache key (same as pagealloc_size)
 *		flags	- (passed to pagealloc_size).
 *		size	- size of page requested (same as pagealloc_size).
 *		size    - page size as passed to partPageAllocNode.
 *		eh 	- error handler pointer
 * Returns:	pointer to pfd if success.
 */
{
    pfd_t	*pfd;
    xp_t	*xp;

    /* XXX: All of these have race for de-activation */

    if (pfd = pagealloc_size(ck, flags, sz)) {
	xp = part_new_page(pfd, 0, sz, 0, eh);
	ASSERT(xp);
	part_permit_xppage(part_get(partid), xp, a);
    }	
    return(pfd);
}

/* ARGSUSED */
void
part_page_free(pfd_t *pfd, size_t sz)
/*
 * Function: 	part_page_free
 * Purpose:	To free up a page of memory previously allocated by 
 *		part_page_allocate_node
 * Parameters:	pfd - pfd for page being freed (returned by 
 *		      part_page_alloc_node)
 *		size- page size as passed to part_page_alloc_node.
 * Returns:	Nothing
 */
{
    xp_t	*xp;

    xp = part_find_page(pfdattophys(pfd));
    /* XXX: part_clean_page(); */
    part_release_page(xp);		/* for lookup above */
    part_release_page(xp);		/* actually cause free */
}

static	void
part_interrupt_thread_exit(void *arg)
/*
 * Function: 	part_interrupt_thread_exit
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    pts_t	*pts = (pts_t *)arg;
    partition_t	*p = pts->pts_partition;

    DPRINTF(("part_interrupt_thread_exit: p(%d) exitting\n", p->p_id));
    part_dequeue_xthread(pts);
    atomicAddInt(&pts->pts_partition->p_xthread_exit, 1);
    if (!(pts->pts_flags & PTS_F_ACTIVE)) {
	/* If we were idle, decrement idle count */
	(void)atomicAddInt(&p->p_xthread_idle, -1);
    }
    kmem_free(pts, sizeof(*pts));
    xthread_exit();
}    

void
part_activate(void *arg)
/*
 * Function: 	part_activate
 * Purpose:	To set up HUB chipd to allow current partition to access the 
 *		specified partition, and start an interrupt thread for the
 *		partition.
 * Parameters:	partid - partition ID that we wish to access.
 * Returns:	Nothing.
 * Notes:	This function simply updates the HUB chips to allow us to 
 *		make requests to the regions specified. It is up to that
 *		partition/region to actually permit the pages.
 */
{
    partition_t	*p = part_get((partid_t)(__psunsigned_t)arg);
    extern void evmk_send_ccintr(int);
    pn_t	*pn;			/* partition node pointer */
    part_status_t *ps;

    DPRINTF(("part_activate: partition(%d)\n", p->p_id));

    ASSERT(p);

    P_LOCK(p);
    if (p->p_state != pActivateWait) {
	/*
	 * Be sure we still want to activate, if not, then we assume 
	 * whatever changed the state will take care of correcting the
	 * thread dispatch address, se we simply return and go to sleep.
	 */
	P_UNLOCK(p);
	return;
    } 
    p->p_state = pActivate;		/* activate in progress */

    if (p->p_id != partition_partid) { /* don't interrupt self */
	pn = part_get_nasid(p->p_id, NULL); /* pick a nasid */
	ASSERT(pn);
	evmk_send_ccintr( pn->pn_nasid );
	/* If this fails, the error is ignored. */
    }

    P_UNLOCK(p);
    P_LOCK_HANDLERS();			/* Don't let handlers change now */
    for (ps = partition_handlers; ps; ps = ps->ps_next) {
	if (ps->ps_activate) {
	    ps->ps_activate(p->p_id);
	}
    }
    P_UNLOCK_HANDLERS();
    
    /*
     * If the partition is still in the activate-in-progress state, we 
     * finish up now. If not, we could only be deactivating - but in that
     * case the ipsema will return to the deactivation handler. 
     */
    P_LOCK(p);
    if (p->p_state == pActivate) {
	p->p_state = pReady;
    }
    P_UNLOCK(p);
    return;
}

void
part_deactivate(partid_t partid)
/*
 * Function: 	part_deactive
 * Purpose:	Request the deactivation of a specific partition.
 * Parameters:	partid - id of partition to deactive.
 * Returns:	Nothing.
 * Notes:	This routine is asynchronous, deactivation requests are 
 *		queued, and processed at some later time.
 */
{
    partition_t	*p = part_get(partid);
    pts_t	*pts;

    if (p) {
	P_LOCK(p);
	switch(p->p_state) {
	case pIdle:
	case pDeactivate:
	case pDeactivateReq:
	    break;
	case pActivateReq:
	case pActivateWait:
	case pActivate:
	case pReady:
	    p->p_state = pDeactivateReq;
	    for(pts = p->p_xthreads.pts_next; pts != &p->p_xthreads;  
		pts = pts->pts_next) {
		xthread_set_func(pts->pts_xthread, 
				 (xt_func_t *)part_terminate, pts);
	    }
	    for(pts = p->p_xthreads.pts_next; pts != &p->p_xthreads; 
		pts = pts->pts_next) {
		vsema(&p->p_isema);
	    }
	    break;
	default:
	    cmn_err(CE_PANIC, "part_deactivate: invalid state (%d)\n", 
		    p->p_state);
	    break;
	}
	P_UNLOCK(p);
    }
}

static void
part_terminate(pts_t *pts)
/*
 * Function: 	part_terminate
 * Purpose:	Remove a partitions region present mappings and terminate
 *		the interrupt thread.
 * Parameters:	partid - partition number being deactivated.
 * Returns:	nothing
 */
{
    partition_t	*p = pts->pts_partition;
    __uint32_t	r;			/* region index */
    nasid_t	n;			/* nasid # */
    avlnode_t	*a, *na;		/* avlnode/next avlnode */
    xp_t	*xp;			/* xp page pointer */
    part_status_t *ps;

    DPRINTF(("part_terminate: partid(%d)\n", p->p_id));

    ASSERT(p && (p->p_id != partition_partid));

    P_LOCK(p);

    if (p->p_state != pDeactivateReq) {
	P_UNLOCK(p);
	part_interrupt_thread_exit(pts);
    }
    p->p_state = pDeactivate;

    p->p_regions = 0;
    part_interrupt_reset(p);		/* zap interrupts */

    for (n = 0; n < MAX_NASIDS; n++) {
	if (p->p_id == partition_nodes[n].pn_partid) {
	    partition_nodes[n].pn_partid = INVALID_PARTID;
	    partition_nodes[n].pn_nasid  = INVALID_NASID;
	    partition_nodes[n].pn_cpuA 	 = 0;
	    partition_nodes[n].pn_cpuB 	 = 0;
	}
    }

    for (r = 0; r < MAX_REGIONS; r++) {
	if (p == partition_regions[r].pr_partition) {
	    partition_regions[r].pr_partition = &partition_master;
	    partition_regions[r].pr_int_count = 0;
	}
    }	
    P_UNLOCK(p);

    /* Notify whoever wants to know about this. */

    P_LOCK_HANDLERS();
    for (ps = partition_handlers; ps; ps = ps->ps_next) {
	if (ps->ps_deactivate) {
	    ps->ps_deactivate(p->p_id);
	}
    }
    P_UNLOCK_HANDLERS();

    /*
     * Freeze the page list, and call out for all pages remaining.
     */
    P_PAGE_LOCK();
    for (a = partition_pages.avl_firstino; NULL != a; a = na) {
	xp = (xp_t *)a;
	if (xp->xp_id == p->p_id) {
	    part_hold_page(xp);		/* be sure node stays */
	    P_PAGE_UNLOCK();
	    if (xp->xp_error) {
		/* Release page list - who knows what upper levels will do */
		xp->xp_error(NULL, xp->xp_addr, PART_ERR_DEACTIVATE);
	    }
	    P_PAGE_LOCK();
	    na = a->avl_nextino;
	    part_release_page_nolock(xp);
	} else {
	    na = a->avl_nextino;
	}
	    
    }
    P_PAGE_UNLOCK();

    /*
     * Wait until everything is gone that we need to be gone.
     */

    P_LOCK(p);
    while (p->p_page_permit || p->p_page_register) {
	sv_wait(&p->p_xsv, PZERO, &p->p_lock, 0);
	P_LOCK(p);
    }
    ASSERT(p->p_state == pDeactivate);
    p->p_state = pIdle;
    P_UNLOCK(p);
    part_interrupt_thread_exit(pts);
}

static	void
part_probe_nasid(nasid_t n)
/*
 * Function: 	part_probe_nasid
 * Purpose:	Process the results from a successful probe operation.
 * Parameters:	n - nasid to probe.
 * Returns:	nothing.
 */
{
    partition_t *p;
    pn_t	*pn;			/* partition node pointer */
    partid_t	pid;			/* partition id */
    __uint32_t	r;			/* region # */
		
    /* For this EVEREST MULTIKERNEL CELL we make
     * cell+1 == region == partition
     */

    pn = &partition_nodes[n];

    if (!partition_inited[EVMK_CPUID_TO_CELLID(n)]) {
	/* Failed to access KLDIR entry for this nasid */
	if (pn->pn_partid != INVALID_PARTID) {
	    part_deactivate(pn->pn_partid);
	    cmn_err(CE_WARN, 
		    "NASID 0x%x partition[%d] no longer responding\n", 
		    n, pn->pn_partid);
	}
	return;
    } else {
	pn->pn_nasid = n;
    }
    r = pid = EVMK_CPUID_TO_CELLID(n);

    /* Verify entry is valid, since node responded OK */

    if (pn->pn_partid == INVALID_PARTID) {
	pn->pn_partid = pid;
	pn->pn_nasid  = n;
	pn->pn_cpuA = 1;
	pn->pn_cpuB = 0;

	/* found for the first time */
	p = &partitions[pid];

	PART_RM_SETB(p, r);
	partition_regions[r].pr_partition = p;
	    /*
	     * Mark state transition if idle, and ready to activate, and 
	     * start a thread on it. 
	     */
	P_LOCK(p);
	if (p->p_state == pIdle) {
	    p->p_state = pActivateReq;
	}
	P_UNLOCK(p);
    } else if (pn->pn_partid != pid) {	/* changed? */
	/* Can't handle yet */
	cmn_err(CE_PANIC, 
		"NASID 0x%x old partition %d new partition %d?\n", 
		n, pn->pn_partid, pid);
    }
    DPRINTF(("part_probe_nasid: n(%d) cell(%d) part(%d)\n", 
	     n, r, pid));
}

static	void
part_discovery(void)
/*
 * Function: 	part_discovery
 * Purpose:	Go looking for new partitions.
 * Parameters:	None.
 * Returns:	Nothing
 *
 * Notes:	Poke at all NASIDs, looking in their KLDIR entry for 
 *		their partition number. If the access fails, leave
 *		the NASID as INVALID. If it succeeds, verify the entry
 *		is valid. If it is, use the partition number listed in 
 *		the targets KLDIR entry to assign it's partid. If it is
 * 		invalid, the partid is left as INVALID.
 */
{
    nasid_t	n;			/* nasid */
    partid_t	pid;
    partition_t	*p;

    /*
     * Check what we are connected to. If we are NOT connected to a 
     * router, then time outs will not really happen .... we need to 
     * do vector operations to determine the remote guys nasid.
     */

    DPRINTF(("part_discovery: EVEREST: Checking for remote partitions\n"));
    for (n = 0; n < MAX_NASIDS; n++)
	part_probe_nasid(n);

    /*
     * Now check which partitions need to conitnue activation, and 
     * start up activation threads on them.
     */

    for (pid = 0; pid < MAX_PARTITIONS; pid++) {
	p = &partitions[pid];
	P_LOCK(p);
	if (p->p_state == pActivateReq) {
	    p->p_state = pActivateWait;
	    P_UNLOCK(p);
	    part_interrupt_set(pid, 2, part_activate);
	    part_start_partition_thread(p, 2, B_TRUE);
	} else {
	    P_UNLOCK(p);
	}
    }
}

static	void
part_spurious_interrupt(void *arg)
/*
 * Function: 	part_spurious_interrupt
 * Purpose:	Interrupt handler for interrupts from partitions that
 *		do not have an interrupt jhandler set for them yet.
 * Parameters:	pid - partition id of sending interrupt
 *		region - region # sending interrupt
 * Returns:	nothing.
 */
{
    partition_t *p = part_get((partid_t)(__psunsigned_t)arg);

    if (p->p_state == pIdle) {
	/* No message if in process of activation */
	atomicAddLong(&p->p_int_spurious, 1);
	cmn_err(CE_WARN, "Spurious interrupt from partition: %d\n", p->p_id);
    }
}

static	void
part_interrupt_thread(void *vp)
/*
 * Function: 	part_interrupt_thread
 * Purpose:	Handle all interrupts from a given partition, calling the 
 *		required interrupt handlers set. 
 * Parameters:	vp - pointer to partition we are handling interrupts for. 
 * Returns:	Does not return.
 * Notes:	This routine is a wrapper for threaded partition interrupts. 
 *		It uses ipsema, which will cause the thread to resume
 *		execution at the start of the routine when another interrupt
 *		has arrived.
 */
{
    pts_t	*pts = (pts_t *)vp;
    partition_t *p = pts->pts_partition;

    
    pts->pts_flags |= PTS_F_ACTIVE;
    /* Process as many as we can before possibly dying. */

    atomicAddInt(&p->p_xthread_idle, -1);
    do {
	    p->p_int((void *)(__psunsigned_t)p->p_id);
    } while (cpsema(&p->p_isema));
    /* 
     * There is a race with the following check, but the values 
     * are updated atomically at a lower level, and we may end up with 
     * an extra thread or 2, or too few, but it will all work out
     * in the end.
     */
    pts->pts_flags &= ~PTS_F_ACTIVE;
    if (p->p_xthread_limit >= atomicAddInt(&p->p_xthread_idle, 1)) {
	ipsema(&p->p_isema);
    } else {
	part_interrupt_thread_exit(pts);
    }	
}

static	void
part_interrupt_thread_setup(pts_t *pts)
/*
 * Function: 	part_interrupt_thread_setup
 * Purpose:	Do one time interrupt thread setup, basically, make sure 
 *		that we gop to sleep right away, and not call the "real"
 *		interrupt handler until there is an interrupt.
 * Parameters:	p - pointer to partition structure.
 * Returns:	Does not return.
 * Notes:	The pts_pts_xthread value MAY not be assigned yet.
 */
{
#if PART_DEBUG
    partition_t	*p = pts->pts_partition;
#endif

    DPRINTF(("part_interrupt_thread_setup: p(%d) ready\n", p->p_id));
    pts->pts_xthread = (xthread_t *)curthreadp;
    xthread_set_func(pts->pts_xthread, part_interrupt_thread, pts);
    part_queue_xthread(pts);
    part_interrupt_thread(pts);
}

/*ARGSUSED*/
static	void
part_interrupt(void *arg)
/*
 * Function: 	part_interrupt
 * Purpose:	Handle a cross call interrupt.
 * Parameters:	arg - useless argument passed by interrupt code.
 * Returns:	Nothing
 */
{
    uint64_t	hr;
    int		r;			/* region # */
    __uint64_t	pm = 0;			/* partition mask */
    partition_t	*p;

    hr = atomicClearUint64(&evmk_cc_intr.cc_intr_pend[cpuid()][0], -1LL );

    for (r = 0; hr; r++, hr >>= 1) {
	if (hr & 1) {			/* this region pending? */
	    p = partition_regions[r].pr_partition;
	    atomicAddLong(&partition_regions[r].pr_int_count, 1);
	    if (0 == (pm & (1 << p->p_id))) {
		(void)atomicAddLong(&p->p_int_count, 1);
		pm |= 1 << p->p_id;
		vsema(&p->p_isema);
	    }
	}
    }
}

__uint64_t
part_interrupt_id(partid_t p)
/*
 * Function: 	part_interrupt_id
 * Purpose:	Determine the current interrupt id (count) for the specified
 *		partition.
 * Parameters:	p - partition id to locate count for.
 * Returns:	Current count (interrupt id).
 */
{
    return(partitions[p].p_int_count);
}

static	void
part_queue_xthread(pts_t *pts)
/*
 * Function: 	part_queue_xthread
 * Purpose:	Assign a xthread to a secific partition.
 * Parameters:	pts - pointer to allocated thread
 * Returns:	nothing
 */
{
    partition_t	*p = pts->pts_partition;

    P_LOCK(p);

    /*
     * If partition is now deactivating, simply exit.
     */
    if (p->p_state == pDeactivate) {
 	kmem_free(pts, sizeof(*pts));
	(void)atomicAddInt(&p->p_xthread_exit, 1);
	P_UNLOCK(p);
	xthread_exit();
    }

    pts->pts_prev = p->p_xthreads.pts_prev;
    pts->pts_next = &p->p_xthreads;
    pts->pts_prev->pts_next = pts;
    pts->pts_next->pts_prev = pts;

    pts->pts_flags = PTS_F_READY;

    p->p_xthread_cnt++;

    P_UNLOCK(p);
    atomicAddInt(&p->p_xthread_idle, 1); /* start off idle */
}

static	void
part_dequeue_xthread(pts_t *pts)
/*
 * Function: 	part_dequeue_xthread
 * Purpose:	To dequeue an xthread from a partition.
 * Parameters:	pts - pointer to partition thread struct to dequeue.
 * Returns:	nothing
 */
{
    partition_t	*p = pts->pts_partition;
    P_LOCK(p);
    pts->pts_next->pts_prev = pts->pts_prev;
    pts->pts_prev->pts_next = pts->pts_next;
    p->p_xthread_cnt--;
    if (0 == p->p_xthread_cnt) {	/* let waiter go ... */
	sv_broadcast(&p->p_xsv);
    }
    P_UNLOCK(p);
}

static int
part_start_partition_thread(partition_t *p, int count, boolean_t wait)
/*
 * Function: 	part_start_partition_thread
 * Purpose:	Start a thread on a partition given the partition structure.
 * Parameters:	p - pointer to partition to start thread on.
 *		count - number of threads to start
 *		wait - if TRUE, wait for memory, else, give up if allocation 
 *		    fails.
 * Returns:	# of threads started.
 */
{
    pts_t	*pts;
    void	*stack;
    int		c, flags, new;
    extern int	default_intr_pri;

    for (new = 0, c = 0; c < count; c++) {
	if (cvsema(&p->p_isema)) {	/* someone waiting */
	    continue;
	}

	/*
	 * Allocate pts struck and stack page without waiting. 
	 */
	if (!(pts = kmem_alloc(sizeof(*pts), wait ? KM_SLEEP : VM_NOSLEEP))) {
	    atomicAddInt(&p->p_xthread_create_failed, 1);
	    break;
	}

	stack = kmem_zalloc(2*KTHREAD_DEF_STACKSZ, 
			    VM_DIRECT | (wait ? KM_SLEEP : VM_NOSLEEP));
	if (!stack) {
	    atomicAddInt(&p->p_xthread_create_failed, 1);
	    kmem_free(pts, sizeof(*pts));
	    break;
	}

	pts->pts_partition = p;
	flags = KT_STACK_MALLOC;
	(void)xthread_create(p->p_name, stack, 2*KTHREAD_DEF_STACKSZ, 
			     flags, default_intr_pri, KT_PS | KT_BIND, 
			     (xt_func_t *)part_interrupt_thread_setup, pts);
	new++;
    }
    atomicAddInt(&p->p_xthread_create, new);
    if (count > c) {
	/*
	 * If more requested that started, be sure a returning thread 
	 * gets launched 
	 */
	vsema(&p->p_isema);
    }
    return(c);
}

boolean_t
part_start_thread(partid_t partid)
/*
 * Function: 	part_start_thread
 * Purpose:	Start another interrupt thread for the specified partition.
 * Parameters:	partid - partition id.
 * Returns:	
 */
{
    partition_t	*p = part_get(partid);
    return(part_start_partition_thread(p, 1, B_FALSE) ? B_TRUE : B_FALSE);
}

static	void
part_interrupt_reset(partition_t *p)
/*
 * Function: 	part_interrupt_rest
 * Purpose:	Reset interrupt handler for a partition.
 * Parameters:	p - pointer to partition to reset interrupts for.
 * Returns:	Nothing.
 */
{
    pts_t	*pts;
    int		i;

    ASSERT(P_LOCKED(p));
    /* Kill the interrupt thread assoctiated with this partition */
    for (pts = p->p_xthreads.pts_next; pts != &p->p_xthreads; 
	 pts = pts->pts_next) {
	xthread_set_func(pts->pts_xthread, part_interrupt_thread_exit, pts);
    }
    /* Be sure to wake everyone up */
    for (i = 0; i < p->p_xthread_cnt; i++) {
	vsema(&p->p_isema);
    }
    p->p_int     = part_spurious_interrupt;
    sv_broadcast(&p->p_xsv);
}

void
part_interrupt_set(partid_t partid, int threads, xt_func_t *rtn) 
/*
 * Function: 	part_interrupt_set
 * Purpose:	Set the inter-partition interrupt handler for the specified
 *		partition.
 * Parameters:	partid - partition id to set handler for.
 *		threads - 0 indicates not a threaded interrupt, >0 
 *			indicates the default # threads "ready" for
 *			action. Ingored if rtn is NULL.
 *		rtn - !NULL - function to call when interrupt occurs.
 *		      NULL - restore default handler.
 * Returns:	nothing
 */
{
    partition_t *p = &partitions[partid];

    ASSERT(p);

    P_LOCK(p);
    if (NULL == rtn) {
	part_interrupt_reset(p);
    } else {
	p->p_int           = rtn;
	p->p_xthread_limit = threads;
    }
    P_UNLOCK(p);
}

static	void
part_init_partition(partition_t *p, partid_t pid, void (f)(void *))
/*
 * Function: 	part_init_partition
 * Purpose:	Initialize a partition structure. 
 * Parameters:	p - pointer to partition structure. 
 *		pid - partition ID. 
 *		f - default interrupt routine.
 * Returns:	Nothing.
 */
{
    sprintf(p->p_name, "part_%d", (int)pid);
    p->p_id 		= pid;
    p->p_state 		= pIdle;
    p->p_int_count   	= 0;
    p->p_int_spurious	= 0;
    p->p_int	    	= f;
    p->p_xthread_cnt 	= 0;
    p->p_xthread_idle   = 0;
    p->p_xthread_limit 	= 1;
    p->p_xthread_create = 0;
    p->p_xthread_create_failed = 0;
    p->p_xthread_exit   = 0;
    p->p_xthreads.pts_next 
	= p->p_xthreads.pts_prev
	= &p->p_xthreads;

    PART_RM_CLRALL(p);
    init_spinlock(&p->p_lock, "p_lock", (long)p);
    init_sema(&p->p_isema, 0, "p_isema", (long)p);
    init_sv(&p->p_xsv, PZERO, "p_xsv", (long)p);
}

void
part_init(void)
/*
 * Function: 	part_init
 * Purpose:	Initialize the partition managment functions.
 * Parameters:	none
 * Returns:	nothing
 */
{
    extern int intr_connect_level(cpuid_t, int, ilvl_t, intr_func_t, void *);

    __uint32_t  r;			/* region # */
    partid_t    p;			/* partition # */
    nasid_t	n;			/* nasid # */
    static avlops_t avlOps = {
	part_avl_start, 
	part_avl_end
    };

    DPRINTF(("Partition initialization ...\n"));

    /* Node map initialization */

    for (n = 0; n < MAX_NASIDS; n++) {
	partition_nodes[n].pn_partid  = INVALID_PARTID;
	partition_nodes[n].pn_nasid   = INVALID_NASID;
	partition_nodes[n].pn_cpuA    = 0;
	partition_nodes[n].pn_cpuB    = 0;
    }

    /* Region map initialization */
 
    for (r = 0; r < MAX_REGIONS; r++) {
        partition_regions[r].pr_partition = &partition_master;
        partition_regions[r].pr_int_count = 0;
    }

    /* Partition initialization */

    for (p = 0; p < MAX_PARTITIONS; p++) {
	part_init_partition(&partitions[p], p, part_activate);
    }

    /*
     * Initialize our "master" special. Any region that does NOT 
     * belong to a real partition belongs to the "master" partition. 
     * The "master" partition has 1 ithread associated with it, which does
     * the actual probing, and then launches a new thread for new 
     * partitions. The interrupt routine is set to the probe routine.
     */
    part_init_partition(&partition_master, INVALID_PARTID, 
			(void (*)(void *))part_discovery);

    mutex_init(&partition_page_lock, MUTEX_DEFAULT, "p_page_lock");
    mrinit(&partition_handlers_lock, "partition_handlers_lock");

    /* grab the interrupt vectors for cc interrupts */

    evintr_connect(0, EVINTR_LEVEL_CCINTR, SPLMAX,
		       EVINTR_DEST_CPUACTION,
		       (EvIntrFunc)part_interrupt, 0);
    /* figure out our partition id */

    partition_partid = EVMK_CPUID_TO_CELLID(cpuid());

    avl_init_tree(&partition_pages, &avlOps);

    /* mark our parition as initialized and "probe-able" */

    partition_inited[partition_partid] = 1;
    evmk_replicate( &partition_inited[partition_partid],  sizeof(int));

    /* Our general purpose thread */

    part_start_partition_thread(&partition_master, 1, B_TRUE);
}

partid_t
part_from_region(__uint32_t region)
/*
 * Function: 	part_from_region
 * Purpose:	To locate a partition based on a region #.
 * Parameters:	region - locate the partition that contains 
 *			 the specified region.
 * Returns:	Pointer to partition.
 */
{
    ASSERT(region < MAX_REGIONS);
    return(partition_regions[region].pr_partition->p_id);
}

partid_t 
part_id(void)
/*
 * Function: 	part_id
 * Purpose:	Return partittion ID of calling partition.
 * Parameters:	nothing
 * Returns:	partid.
 */
{
    return(partition_partid);
}

static partition_t	*	
part_get(partid_t p)
/*
 * Function: 	part_get
 * Purpose:	Return a pointer to the partition structure for a given 
 *		partition ID.
 * Parameters:	p - partition id.
 * Returns:	NULL - Invalid partition
 *		!NULL - pointer to parititon structure.
 */
{
    ASSERT(p < MAX_PARTITIONS);

    if ((p < MAX_PARTITIONS) && (partitions[p].p_id != INVALID_PARTID)){
	return(&partitions[p]);
    } else {
	return(NULL);
    }
}

paddr_t 
part_find_xp(partid_t partid)
/*
 * Function: 	part_find_xp
 * Purpose:  	Locate the cross partition parameter for the specified 
 *		nasid.
 * Parameters:	partid - partition id of parition to get parameters for.
 * Returns:	0 - XP variable is invalid, not set, or access failed.
 *		!0 - Value of cross partition parameter.
 */
{
    nasid_t		nasid;		/* partition nasid is on */
    /*
     * Locate a node in that partition - we need to address a KLDIR entry
     * somewhere.
     */
    for (nasid = 0; nasid < MAX_NASIDS; nasid++) {
	if (NASID_TO_PARTID(nasid) == partid) { /* looks like a match */
	    return(partition_xp[partid]);
	}
    }
    return(0);
}
    
void
part_set_xp(paddr_t xp)
/*
 * Function: 	part_set_xp
 * Purpose:	Set the cross partition parameter for calling partition.
 * Parameters:	xp - value to set the cross partition parameter to.
 * Returns:	nothing
 */
{
    if (partition_xp[part_id()])
    	cmn_err(CE_PANIC,
		"part_set_xp: already invoked for this cell %d\n",
		evmk_cellid);

    partition_xp[part_id()] = xp;

    /* Make change visible to all partitions (cells) */
    evmk_replicate( &partition_xp[part_id()], sizeof(paddr_t));
}

pn_t *
part_get_nasid(const partid_t partid, nasid_t *nasid)
/*
 * Function:	part_get_nasid
 * Purpose:	Find a node in a partition.
 * Parameters:	partid	- partition ID of partition to locate a node in.
 *		nasid   - starting nasid, updated to nasid selected on
 *			  return (May be NULL). If non-null, usually
 *			  '0' is passed on the first call. If return
 *			  value is NULL - this value is undefined.
 * Returns: 	Pointer to pn_t or NULL if failed.
 */
{
    nasid_t	n;
    pn_t	*pn;

    for (n = (NULL == nasid) ? 0 : *nasid; n < MAX_NASIDS; n++) {
	pn = &partition_nodes[n];
	if ((partid == pn->pn_partid) && (pn->pn_cpuA || pn->pn_cpuB)) {
	    if (NULL != nasid) {
		*nasid = n;
	    }
	    return(pn);
	}
    }
    return(NULL);
}

/*ARGSUSED*/
void
part_poison_page(pfd_t *pfd)
/*
 * Function:	part_poison_page
 * Purpose: 	Posion the specified page. 
 * Parameters:	pfd - pointer to pfd for page.
 * Returns:	Nothing.
 */
{
}

void	*
part_register_page(paddr_t a, size_t s, part_error_handler_t eh)
/*
 * Function: 	part_register_page
 * Purpose:	Register a remote partitions page in use locally.
 * Parameters:	a - base address of page.
 *		s - size of page in bytes.
 *		eh - error handler routine.
 * Returns:	handle (pointer to xp struct), NULL if failed.
 */
{
    xp_t	*xp;

    xp = part_new_page(NULL, a, s, XP_REGISTER, eh);
    return(xp);
}

void
part_unregister_page(paddr_t a)
/*
 * Function:	part_unregister_page
 * Purpose:	Un-register a page previously registered using 
 *		partRegisterPage.
 * Parameters:	a - physical address of base of page (MUST BE IDENTICAL 
 *		    to address passed to partRegisterPage).
 * Returns:	Nothing.
 */
{
    xp_t	*xp;

    xp = part_find_page(a);
    ASSERT(xp);
    part_release_page(xp);		/* once for find page above */
    part_release_page(xp);		/* once to cause delete */
}

void
part_register_handlers(void (*ar)(partid_t), void (*dr)(partid_t))
/*
 * Function: 	part_register_handler
 * Purpose:	Register a partition "activate" and "deactivate"
 *		handler.
 * Parameters:	ar - pointer to routine to call when a partition
 *		     is "activated".
 *		dr - pointer to routine to call when a partition is
 *		     deactivated.
 * Returns:	Nothing
 * Notes: 	Routines are called in the order in which they are 
 *		registered. 
 *
 * 		Any "active" partitions when an activate routine is
 *		registered will cause a call out  to the newly 
 *		registered routine.
 */
{
    part_status_t	*ps, *nps;
    partid_t		pid;
    partition_t		*p;

    nps = kmem_alloc(sizeof(*nps), 0);

    nps->ps_activate   = ar;
    nps->ps_deactivate = dr;
    nps->ps_next       = NULL;

    P_UPDATE_HANDLERS();

    if (!partition_handlers) {
	partition_handlers = nps;
    } else {
	for (ps = partition_handlers; ps->ps_next; ps = ps->ps_next)
	    ;
	ps->ps_next = nps;
    }

    /* Call new activate routine for every partition that is active */

    for (pid = 0; pid < MAX_PARTITIONS; pid++) {
	p = part_get(pid);
	if (p && (p->p_state == pReady)) {
	    if (nps->ps_activate) {
		nps->ps_activate(pid);
	    }
	}
    }

    P_UNLOCK_HANDLERS();
}

/*ARGSUSED*/
int
part_operation(int op, int a1, void *a2, void *a3, rval_t *rvp)
/*
 * Function: 	part_operation
 * Purpose:	Performs tasks available at user level via SYSSGI.
 * Parameters:	op - SYSSGI_PARTOP to perform.
 *		a1,a2,a3 - arguments
 *		rvp - pointer to return value.
 * Returns:	0 for success, errno for failure.
 */
{
    int	errno = 0;

    switch(op) {
    case SYSSGI_PARTOP_GETPARTID:
	if (INVALID_PARTID == (rvp->r_val1 = part_id())) {
	    errno = ENOTSUP;		/* Partitioning not turned on */
	} 
	break;
    case SYSSGI_PARTOP_PARTMAP: {
	part_info_t	*upi = (part_info_t *)a2;
	part_info_t	pi;
	int		count = 0;
	partid_t	pid;
	partition_t 	*p;

	for (pid = 0; pid < MAX_PARTITIONS; pid++) {
	    p = &partitions[pid];
	    if (p->p_state == pReady) {
		pi.pi_partid 	= pid; 
		pi.pi_flags 	= PI_F_ACTIVE;
		if (0 < a1--) {
		    if (copyout(&pi, upi, sizeof(pi))) {
			return(EFAULT);
		    }
		    upi++;
		}
		count++;
	    }
	}
	rvp->r_val1 = count;
	break;
    }
    case SYSSGI_PARTOP_NODEMAP: {
	pn_t 	*upn = (pn_t *)a2;
	pn_t	*pn;
	int	count = 0;
	nasid_t	n;

	for (n = 0; n < MAX_NASIDS; n++) {
	    pn = &partition_nodes[n];
	    if (pn->pn_partid != INVALID_PARTID) {
		if (0 < a1--) {
		    if (copyout(pn, upn, sizeof(*pn))) {
			return(EFAULT);
		    }
		    upn++;
		}
		count++;
	    }
	}
	rvp->r_val1 = count;
	break;
    }
    case SYSSGI_PARTOP_DEACTIVATE:
	/* XXX: CHECK PERMISSIONS */
	part_deactivate((partid_t)a1);
	break;
    case SYSSGI_PARTOP_ACTIVATE:
	/* XXX: CHECK PERMISSIONS */
	vsema(&partition_master.p_isema);
	break;
    default:
	errno = EINVAL;
	break;
    }
    return(errno);
}

void
part_page_dump(void (*printf)(char *, ...))
/*
 * Function: 	part_page_dump
 * Purpose:	Print out current list of pages in the page tree.
 * Parameters:	printf - pointer to print function to use
 * Returns:	nothing
 */
{
    avlnode_t	*a;
    xp_t	*xp;

    printf("\nCross Partition Pages"
	   "\n\tAddress\t\tLength\tOwner\tHold\tError\t\t\tFlags\n");
    for (a = partition_pages.avl_firstino; NULL != a; a = a->avl_nextino) {
	xp = (xp_t *)a;
	printf("0x%x\t%d\t%d\t%d\t0x%x\t0x%x <", 
	       xp->xp_addr, xp->xp_size, xp->xp_id, xp->xp_hold, 
	       xp->xp_error, xp->xp_flags);
	if (xp->xp_flags) {
	    if (xp->xp_flags & XP_CLEANING) 	printf("cleaning ");
	    if (xp->xp_flags & XP_ERROR) 	printf("error ");
	    if (xp->xp_flags & XP_POISON) 	printf("poison ");
	    if (xp->xp_flags & XP_REGISTER) 	printf("registered-remote ");
	}
	printf(">\n");
    }
}

static	void
part_dump_partition(void (*printf)(char *, ...), partition_t *p)
/*
 * Function: 	part_dump_partition
 * Purpose:	Dumps info about the specifiec partition.
 * Parameters:	printf - pointer to print function to use
 *		pid    - pid of partition to display info on.
 * Returns:	nothing
 */
{
    int		r, fc;			/* region/format count */
    int		tri;			/* total region interrupts */
    nasid_t	n;			/* node */
    pn_t	*pn;			/* partition node */
    pts_t	*pts;

    printf("Partition (%d): isema(0x%x) pages permit(%d) "
	   "reg(%d) state(%s)\n", 
	   p->p_id, &p->p_isema, p->p_page_permit, p->p_page_register, 
	   part_state[p->p_state]);

    printf("\tinterrupts:");
    for (tri = 0, r = 0; r < MAX_REGIONS; r++) {
	if (partition_regions[r].pr_partition == p) {
	    printf(" r[%d]=%d", r, partition_regions[r].pr_int_count);
	    tri += partition_regions[r].pr_int_count;
	}
    }
    printf(" tot (%d), spur(%d)\n", p->p_int_count, p->p_int_spurious);
    printf("\tthreads: total(%d) idle(%d) idle limit(%d) active(%d) "
	   "create(%d) exit(%d) failed create(%d)\n", 
	   p->p_xthread_cnt, p->p_xthread_idle, p->p_xthread_limit,
	   p->p_xthread_cnt - p->p_xthread_idle, p->p_xthread_create,
	   p->p_xthread_exit, p->p_xthread_create_failed);

    printf("\txthreads:\n\t\t");
    fc = 0;
    for (pts=p->p_xthreads.pts_next; pts!=&p->p_xthreads; pts=pts->pts_next) {
	printf("0x%x%c%s", pts->pts_xthread, 
	       (pts->pts_flags & PTS_F_ACTIVE) ? '*' : ' ', 
	       !(++fc % 4) ? "\n\t\t" : " ");
    }
    printf("\n");

    printf("\tRegions(0x%x):", p->p_regions);
    for (r = 0; r < MAX_REGIONS; r++) {
	if (PART_RM_TSTB(p, r)) {
	    printf(" %d", r);
	}
    }

    printf("\n\tNodes:\n\t");
    for (fc = 0, n = 0; n < MAX_NASIDS; n++) {
	pn = &partition_nodes[n];
	if (pn->pn_partid == p->p_id) {
	    if (fc == 10) {
		printf("\n\t");
		fc = 0;
	    }
	    printf(" %d<%c%c>", n, 
		   pn->pn_cpuA ? 'A' : '_', pn->pn_cpuB ? 'B' : '_');
	    fc++;
	}
    }
    printf("\n");
}

void
part_dump(void (*printf)(char *, ...))
/*
 * Function: 	part_dump
 * Purpose:	Display information on current partition maps
 * Parameters:	printf - pointer to print routine to use.
 * Returns:	Nothing.
 * Notes:	This routine is called from symmon to dump interesting
 *		information.
 */
{
    partid_t	pid;
    pn_t	*pn;
    int		r, i, fc;
    nasid_t	n;
    part_status_t *ps;

    if (INVALID_PARTID == partition_partid) {
	printf("Partitions not enabled\n");
	return;
    }

    printf("Local Partition: %d\n", partition_partid);
    printf("\n\thandlers:\n");
    fc = 1;
    for (ps = partition_handlers; ps; ps = ps->ps_next) {
	printf("\t\t[%d] 0x%x/0x%x\n", 
	       fc++, ps->ps_activate, ps->ps_deactivate);
    }
    
    for (pid = 0; pid < MAX_PARTITIONS; pid++) {
	if (pIdle != partitions[pid].p_state) {
	    part_dump_partition(printf, &partitions[pid]);
	}
    }

    printf("\nPartition Node Map:\n\t");
    for (n = i = 0; n < MAX_NASIDS; n++) {
	pn = &partition_nodes[n];
	if (pn->pn_partid != INVALID_PARTID) {
	    printf("n(%d,p=%d,m=%d) ", pn->pn_nasid, pn->pn_partid, 
		   pn->pn_module);
	    if ((i % 4) == 3) {
		printf("\n\t");
	    }
	    i++;
	} 
    }

    printf("\n\nPartition Region Map:\n\t");
    for (r = i = 0; r < MAX_REGIONS; r++) {
	if (partition_regions[r].pr_partition->p_id != INVALID_PARTID) {
	    printf("r(%d,p=%d,int=%d) ", r, 
		   partition_regions[r].pr_partition->p_id, 
		   partition_regions[r].pr_int_count);
	    if ((i % 4) == 3) {
		printf("\n\t");
	    }
	    i++;
	}
    }

    part_page_dump(printf);
}




int
part_export_page(paddr_t paddr, size_t size, partid_t partid)
/*
 * Function:    part_export_page
 * Purpose:     Export page to another cell by dropping the firewall
 * Parameters:  paddr - physical addr
 *		size - number of bytes (multiple of NBPP)
 *              part  - partition to export to
 * Returns:     0 = page exported, 1 = page already exported
 */
{
	int		firsttime=1;
	__uint64_t	partbit = ((__uint64_t)1)<<partid;
	pfd_t		*pfd;
	
	ASSERT(size == NBPP);
	pfd = pfntopfdat(pnum(paddr));
	if (firsttime) {
		firsttime = 0;
		if (pfd->pf_exported_to&partbit)
			return 1;
	}
	ASSERT( !(pfd->pf_exported_to&partbit));
	pfd->pf_exported_to |= partbit;

	return 0;
}




int
part_unexport_page(paddr_t paddr, size_t size, partid_t partid)
/*
 * Function:    part_unexport_page
 * Purpose:     Unexport pages to another part by raising the firewalls
 * Parameters:  paddr - physical addr
 *		size - number of bytes (multiple of NBPP)
 *              part  - partition to export to
 * Returns:     0 = page no longer exported to ANY part, 1 = page still
 *              exported to other parts
 */
{
	__uint64_t	partbit = ((__uint64_t)1)<<partid;
	pfd_t		*pfd;
	
	ASSERT(size == NBPP);
	pfd = pfntopfdat(pnum(paddr));
	ASSERT(pfd->pf_exported_to&partbit);
	pfd->pf_exported_to &= ~partbit;

	return (pfd->pf_exported_to != 0);
}


