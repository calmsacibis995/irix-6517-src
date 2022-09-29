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

#ident "$Revision: 1.65 $"

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
#include "sys/capability.h"
#include "sys/kthread.h"
#include "sys/rt.h"
#if CELL
#include "sys/kopt.h"
#include "ksys/cell.h"
#endif

#include "sys/SN/addrs.h"
#include "sys/SN/agent.h"
#include "sys/SN/arch.h"
#include "sys/SN/klconfig.h"
#include "sys/SN/intr.h"
#include "sys/SN/klpartvars.h"
#include "sys/SN/klpart.h"
#include "sys/SN/gda.h"

#if defined (SN0)
#include "sys/SN/SN0/bte.h"
#endif

#include "ksys/partition.h"
#include "ksys/sthread.h"
#include  "ksys/cacheops.h"

#include "error_private.h"
#include "sn_private.h"

#ifdef DEBUG
#define PART_DEBUG      0
#define	DEBUG_VERBOSE	0
#endif

#if PART_DEBUG
#   define	DPRINTF(x)	part_debug x
#else
#   define	DPRINTF(x)
#endif

/*
 * Macro:	DPRINTFV (Debug printf verbose)
 * Purpose:	Verbose debug output. 
 */
#if DEBUG_VERBOSE
#    define	DPRINTFV(x)	DPRINTF(x)
#else 
#    define	DPRINTFV(x)
#endif

/*
 * Define:	PART_SCALE_TO
 * Purpose:	Defines the scale factor for timeouts. Normally set to 1, 
 *		but scaled up for debug kernels since they can take 
 *		and unreasonable amount of time to init. 
 */
#if defined(DEBUG)
#   define	PART_SCALE_TO	10
#else
#   define	PART_SCALE_TO	1
#endif

/*
 * Define: 	PART_HUBREV
 * Purpose: 	Minimum value of the hub revision register version field
 *		required for partitioning to work. If ANY HUB in the 
 *		current partition is less than this value, partitioning
 *		is effectively disabled.
 */
#define	PART_HUBREV	HUB_REV_2_1	/* Hub required */

#define	PART_RM_SETB(p, b)	((p)->p_regions |= (1UL << (b)))
#define	PART_RM_CLRB(p, b)	((p)->p_regions &= ~(1UL << (b)))
#define	PART_RM_TSTB(p, b)	((p)->p_regions & (1UL << (b)))
#define	PART_RM_CLRALL(p)	((p)->p_regions = (0))
#define LOCAL_REGION(b)		(region_mask & (1UL << (b)))

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
    "pActivateHB", "pReady", "pDeactivateReq", "pDeactivate"
};

static char *part_var[] = {"state", "   hb", "hb-fq", "hb-pm", "  xpc"};
static char *part_var_state[] = {"Invalid", "Running", "Stopped"};

/*
 * Typedef: partition_t
 * Purpose: Defines a bunch of stuff for each partition.
 */
typedef struct partition_s {
    char		p_name[8];	/* "part_##" */
    lock_t		p_lock;		/* Lock for this struct */
    int			p_lock_spl;	/* locked previous spl */
    int			p_handlers_cnt;	/* handlers called */
    partid_t		p_id;		/* Partition ID */
#ifdef CELL_IRIX
    domainid_t		p_domainid;	/* Domain ID */
    clusterid_t		p_clusterid;	/* Cluster ID */
#endif /* CELL_IRIX */
    __uint64_t		p_regions;	/* Regions */
    enum {
	pInvalid,
	pIdle,				/* Idle */
	pActivateReq,			/* activation request */
	pActivateWait,			/* activation pending */
	pActivate,			/* activation in progress */
	pActivateHB,			/* Waiting for remote HB to start */
	pReady,				/* partition ready */
	pDeactivateReq,			/* deactivation in progress */
	pDeactivate			/* deactivation in progress */
    }			p_state;
    part_var_t		*p_xp;		/* remote parameter area */
    __uint64_t		p_heartbeat;	/* Heartbeat value */
    __uint64_t		p_heartbeat_to;	/* Heartbeat activate timeout */
    __uint64_t		p_int_count;	/* interrupt count */
    __uint64_t		p_int_spurious;	/* spurious interrupt count */
    void		(*p_int)(void *, int); /* interrupt handler */
    __uint32_t		p_page_permit;	/* # pages permitted */
    __uint32_t		p_page_register; /* # registered remote pages */
    int			p_xthread_cnt;	/* P_LOCK: xthread count */
    int			p_xthread_cnt_peak; /* Max # threads */
    int			p_xthread_limit; /* # threads allowed to sleep */
    int			p_xthread_idle;	/* # threads idle at current time */
    int			p_xthread_started; /* # started, not yet idle */
    int			p_xthread_create_failed; /* failed creates */
    int			p_xthread_create; /* # created threads */
    int			p_xthread_exit; /* # terminated threads */
    int			p_xthread_requested; /* # xthreads requires failed */
    int			p_xthread_requested_bg;	/* # requested part_thread */
    sema_t		p_isema;	/* sema for interrupt handler */
    sv_t		p_xsv;		/* activate/exit sync variable */
    pts_t		p_xthreads;	/* xthread queue */
    xthread_t		*p_activate_thread; /* Activation callout thread */
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
 *		xp flags indicating it is XP_F_REGISTER.
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
    cpuid_t		xp_cpuid;		/* cpu cleaning */
    part_error_handler_t xp_error;		/* error handler this page */
    __uint64_t		xp_regions;		/* Regions that can access */
    __uint32_t		xp_hold;		/* Hold count */
    __uint32_t		xp_flags;		/* Flags */
#   define		XP_F_LOCK	0x01 	/* Lock */
#   define		XP_F_CLEANING	0x02	/* Page being cleaned */
#   define		XP_F_ERROR	0x04	/* Error on page */
#   define		XP_F_POISON	0x08	/* Page poisoned */
#   define		XP_F_REGISTER	0x10    /* Registers remote page */
} xp_t;

#define	XP_LOCK(_x)	bitlock_acquire_32bit(&(_x)->xp_flags, XP_F_LOCK)
#define	XP_UNLOCK(_x)	bitlock_release_32bit(&(_x)->xp_flags, XP_F_LOCK)

/*
 * Variable:	partition_pages
 * Purpose:	Used by AVL routines to keep track of all pages in this 
 *		partition that "may" be cross partition pages. 
 * 		(pfdat indicates P_XP).
 * Locks:	Protected by partition_page_lock (P_PAGE_LOCK).
 */
static  avltree_desc_t	partition_pages;
static	lock_t		partition_page_lock;
static  int		partition_page_spl;

#define	P_PAGE_LOCK()	partition_page_spl = \
                        mutex_spinlock_spl(&partition_page_lock, splerr)
#define	P_PAGE_UNLOCK()	mutex_spinunlock(&partition_page_lock, \
					 partition_page_spl)
#define	P_PAGE_LOCKED()	spinlock_islocked(&partition_page_lock)

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
static	int			partition_handlers_cnt = 0;
static	part_status_t		*partition_handlers = NULL;
static	mutex_t			partition_handlers_lock;
#define	P_LOCK_HANDLERS()	mutex_lock(&partition_handlers_lock, PZERO)
#define	P_UNLOCK_HANDLERS()	mutex_unlock(&partition_handlers_lock)

#define	MD_PREMIUM(a) \
    ((REMOTE_HUB_L(NASID_GET(a), MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM) \
        ? B_TRUE : B_FALSE)

partition_t	partition_idle;
partition_t	partitions[MAX_PARTITIONS];
partid_t	partition_partid = INVALID_PARTID;

extern hubreg_t	region_mask;

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
    partition_t	*pr_partition;		/* Partition pointer */
} partition_regions[MAX_REGIONS];


/*
 * Variable: 	partition_fine
 * Purpose:	Indicates if the system is running in fine (true) 
 *		or coarse (false) mode.
 *		(fine --> 1 node per region, coarse --> 8 nodes per region).
 */
static 	int 		partition_fine;

/*
 * Variable:	partition_heartbeat_ticks
 * Purpose:	Number of ticks between checking remote heartbeat. Default
 *		is 4 seconds.
 */
static	int		partition_heartbeat_ticks = HZ * 4 * PART_SCALE_TO;

/*
 * Variable:	partition_activate_to
 * Pupose:	Number of partition_heartbeat_ticks allowed to wait
 * 		for remote partition to start heartbeating us on activation.
 *		Default is 4 * 4 = 16 seconds.
 */
static	int		partition_activate_to = 4;

/*
 * Variable:	partition_vars
 * Purpose:	Address of local partitions exported cross partition 
 *		variables.
 */
static	part_var_t 	*partition_vars = NULL;
static	lock_t		partition_vars_lock;

/*
 * Variable:	partition_region_lock
 * Purpose:	Serialize updates to one or all region present masks in 
 *		partition.
 */
static	lock_t		partition_region_lock;

/*
 * Variable: 	partition_thread_sema
 * Purpose:	Semaphore partition thread sleeps on.
 */
static	sema_t		partition_thread_sema;

/*
 * Variable:	partition_discovery_in_progress
 * Purpose:	TRUE if discovery is currently going on.
 */
static	boolean_t	partition_discovery_in_progress = B_FALSE;


static 	__psunsigned_t	part_avl_start(avlnode_t *);
static 	__psunsigned_t	part_avl_end(avlnode_t *);
static	void		part_hold_page(xp_t *);
static	__uint32_t	part_release_page_nolock(xp_t *);
static	__uint32_t	part_release_page(xp_t *);
static	xp_t		*part_find_page(paddr_t);
static	xp_t		*part_new_page(pfd_t *, paddr_t, size_t, __uint32_t, 
				       part_error_handler_t);
static  void		part_init_hub(hubreg_t);
static  void		part_init_memory(hubreg_t);
static 	void		part_permit_xppage(xp_t *, __uint64_t, __uint64_t);
        void		part_permit_page(partid_t, pfd_t *, pAccess_t);
static	void		part_pfdat_permit(pfd_t *, __uint64_t, __uint64_t);
static	void		part_pfdat_depermit(pfd_t *, __uint64_t, __uint64_t);
static	void		part_pfdat_flags(pfd_t *, size_t, __uint64_t, 
					 __uint64_t);
        pfd_t		*part_page_allocate_node(cnodeid_t,partid_t,pAccess_t,
						 uint, int, size_t, 
						 part_error_handler_t);
        pfd_t		*part_page_allocate(partid_t, pAccess_t, uint, int, 
					    size_t, part_error_handler_t);
        void		part_page_free(pfd_t *, size_t);
static	void		part_interrupt_thread_exit(void *);
static	void		part_activate(void *, int);
        void            part_deactivate(partid_t);
static 	void		part_terminate(pts_t *);
static	int		part_copy_fault(void *, void *, int);
static	void		part_probe_nasid(nasid_t);
static	void		part_discovery(void);
static	void		part_spurious_interrupt(void *, int);
static	void		part_interrupt_thread(void *);
static	void		part_interrupt_thread_setup(pts_t *);
static	void		part_interrupt(void *);
        __uint64_t	part_interrupt_id(partid_t);
static	void		part_queue_xthread(pts_t *);
static	void		part_dequeue_xthread(pts_t *);
static  int 		part_start_partition_thread(partition_t *, 
						    int, boolean_t);
        boolean_t	part_start_thread(partid_t, int, boolean_t);
static	void		part_interrupt_reset(partition_t *);
        void		part_interrupt_set(partid_t, int, 
					   void (*)(void *, int));
static  void		part_init_partition(partition_t *, partid_t, 
                                            void (*)(void *, int));
        void		part_init(void);
        partid_t	part_from_addr(paddr_t);
        partid_t	part_from_region(__uint32_t);
	partid_t	part_get_promid(void);
        partid_t 	part_id(void);
static  partition_t	*part_get(partid_t);
        pn_t 		*part_get_nasid(const partid_t, nasid_t *);
static  void		part_poison_xppage(xp_t *);
        void		part_poison_page(pfd_t *);
static	int		part_error_handler(eframe_t *, paddr_t, 
					   enum part_error_type);
        void		*part_register_page(paddr_t, size_t, 
					    part_error_handler_t);
        void		part_unregister_page(paddr_t);
static	void		part_call_activate_handlers(partition_t *);
static	void		part_activate_thread_setup(partition_t *);
static	void		part_call_deactivate_handlers(partition_t *);
        void		part_register_handlers(void (*)(partid_t), 
					       void (*)(partid_t));
static	void		part_call_activate_handlers(partition_t *);
static	void		part_call_deactivate_handlers(partition_t *);
static	void		part_activate_thread_setup(partition_t *);
        int		part_operation(int, int, void *, void *, rval_t *);
        void		part_page_dump(void (*)(char *, ...));
static	void		part_dump_partition(void (*)(char *, ...), 
					    partition_t *);
        void		part_dump(void (*)(char *, ...));
        int 		part_export_page(paddr_t, size_t, partid_t);
        int		part_unexport_page(paddr_t, size_t, partid_t, int);
        int		part_get_var(partid_t, int, __psunsigned_t *);
        void		part_set_var(int, __psunsigned_t);
static	void		part_heartbeat(void);
        void		part_heart_beater(void);
static	void		part_thread(void);


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



partid_t
nasid_to_partid(nasid_t nasid)
/*
 * Function:    nasid_to_partid
 * Purpose:     Convert a nasid to the partition that contains the node
 * Parameters:  nasid
 * Returns:     partid
 */
{
	return(NASID_TO_PARTID(nasid));
}



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
    partition_t	*p;

    ASSERT(xp->xp_hold);
    if (0 == (rc = --xp->xp_hold)) {
	p = part_get(xp->xp_id);
	(void)avl_delete(&partition_pages, &xp->xp_avlnode);
	if (xp->xp_pfd) {
	    part_pfdat_flags(xp->xp_pfd, xp->xp_size, 0UL, P_XP);
	    (void)pagefree_size(xp->xp_pfd, xp->xp_size);
	}
	if (0 == atomicAddInt((xp->xp_flags & XP_F_REGISTER) 
			      ? &p->p_page_register : &p->p_page_permit, 
			      -1)) {
	    P_LOCK(p);
	    sv_broadcast(&p->p_xsv);
	    P_UNLOCK(p);
	}
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

static	xp_t   *
part_new_page(pfd_t *pfd, paddr_t pa, size_t sz, __uint32_t flags, 
	      part_error_handler_t eh)
/*
 * Function: 	part_new_page
 * Purpose:	Create and add a new cross partition page structure.
 * Parameters:	pfd - pointer to pfd for page IFF page owned locally.
 *		      NULL otherwise.
 *		pa  - physical address of page
 *		sz  - size of page in bytes.
 *		flags-xp_flags values (XP_F_REGISTER if remote page 
 *		      registration).
 *		eh  - error handler for page.
 * Returns:	Pointer to xp structure for page.
 *		Null if partition no longer active.
 */
{
    xp_t	*xp= NULL;
    partition_t	*p;

    if (pfd) {
	p = part_get(part_id());
    } else {
	p = part_get(part_from_addr(pa));
    }

    if (p) {
	xp = kmem_alloc(sizeof(*xp), KM_SLEEP); /* before P_LOCK - spinlock */
	P_PAGE_LOCK();
	P_LOCK(p);
	if (p->p_state == pReady) {
	    if (pfd) {
		xp->xp_addr = pfdattophys(pfd);
	    } else {
		xp->xp_addr = pa;
	    }
	    xp->xp_pfd     = pfd;
	    xp->xp_size    = sz;
	    xp->xp_flags   = flags;
	    xp->xp_error   = eh;
	    xp->xp_hold    = 1;
	    xp->xp_id      = p->p_id;
	    xp->xp_regions = p->p_regions;
	    (void)avl_insert(&partition_pages, &xp->xp_avlnode);
	    if (flags & XP_F_REGISTER) {
		(void)atomicAddInt(&p->p_page_register, 1);
	    } else {
		(void)atomicAddInt(&p->p_page_permit, 1);
	    }
	} else {
	    kmem_free(xp, sizeof(*xp));
	    xp = NULL;
	}
	P_UNLOCK(p);
	P_PAGE_UNLOCK();
    }
    return(xp);
}

static void
part_init_hub(hubreg_t r)
/*
 * Function: 	part_init_hub
 * Purpose:	Init Hub registers inside partition.
 * Parameters:	r - regions present in partition (region mask).
 * Returns:	Nothing
 */
{
    cnodeid_t	cn;
    nasid_t	nasid;
    hubreg_t	pi_regn = r;

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
    extern hubreg_t partition_region_mask;
    if (kdebug) {
	cmn_err(CE_NOTE, "part_init_hub: for logical cells, "
		         "changing pi protection mask from %x to %x",
		pi_regn, pi_regn | partition_region_mask);
	pi_regn |= partition_region_mask;
    }
#endif /* CELL_IRIX && LOGICAL_CELLS */

    DPRINTF(("part_init_hub: setting partition hub protections: 0x%x\n", r));
    for (cn = 0; cn < maxnodes; cn++) {
	nasid = COMPACT_TO_NASID_NODEID(cn);
	REMOTE_HUB_S(nasid, PI_CPU_PROTECT, pi_regn);
	REMOTE_HUB_S(nasid, PI_IO_PROTECT, r);
	REMOTE_HUB_S(nasid, MD_IO_PROTECT, r);
	REMOTE_HUB_S(nasid, MD_HSPEC_PROTECT, r);
    }
}

static void
part_init_memory(hubreg_t r)
/*
 * Function: 	part_init_node_memory
 * Purpose:	To set up the memory protection for all pages on a node. 
 *		Pages are permitted rw to this partition, none to all others.
 * Parameters:	r - regions present in partition (region mask).
 * Returns:	Nothing.
 */
{
    cnodeid_t	cn;			/* cnodeid */
    nasid_t	nasid;
    __uint64_t	la, ra;			/* local/remote access bits */
    paddr_t	pa, paEnd;		/* physical address/end */
    __uint32_t	ri, mr;			/* region # /max region # */
    __int32_t	s;
    pfn_t	ssz;
    hubreg_t	mmc;			/* MD config register */

    DPRINTF(("part_init_memory:  regmask 0x%x\n", r));
    for (cn = 0; cn < maxnodes; cn++) {
	nasid = COMPACT_TO_NASID_NODEID(cn);
	if(REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM) {
	    la = (MD_PROT_RW<<MD_PPROT_SHFT)|(MD_PROT_RW<<MD_PPROT_IO_SHFT);
	    ra = (MD_PROT_NO<<MD_PPROT_SHFT)|(MD_PROT_NO<<MD_PPROT_IO_SHFT);
	    mr = MAX_PREMIUM_REGIONS;

	    /* Enable I/O protection for premium nodes */

	    mmc = REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG);
	    mmc |= MMC_IO_PROT;
	    REMOTE_HUB_S(nasid, MD_MEMORY_CONFIG, mmc);
	} else {
	    la = MD_PROT_RW << MD_SPROT_SHFT;
	    ra = MD_PROT_NO << MD_SPROT_SHFT;
	    mr = MAX_NONPREMIUM_REGIONS;
	}

	/* set protections for each page on a node */

	for (s = node_getnumslots(cn) - 1; s >= 0; s--) {
	    if (ssz = slot_getsize(cn, s)) {
		pa    = slot_getbasepfn(cn, s) * (paddr_t)NBPP;
		paEnd = pa + (paddr_t)ssz * (paddr_t)NBPP;
		/* Found some memory to protect */
		DPRINTF(("part_init_memory: node (%d) nasid(%d) slot(%d) "
			 "0x%x --> 0x%x\n", cn, nasid, s, pa, paEnd));
		while (pa < paEnd) {
		    for (ri = 0; ri < mr; ri++) {
			if (r & (1UL << ri)) {
			    *(__psunsigned_t *)BDPRT_ENTRY(pa, ri) = la;
			} else {
			    *(__psunsigned_t *)BDPRT_ENTRY(pa, ri) = ra;
			}
		    }
		    pa += MD_PAGE_SIZE;
		}
	    }
	}
    }
#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
    if (kdebug) {
	extern nasid_t partition_master;
	extern hubreg_t partition_region_mask;
	paddr_t prot_ov_end;

	if (partition_master == master_nasid) {
	    pa = TO_NODE(master_nasid, KERNEL_START_OFFSET);
	    prot_ov_end = TO_NODE(master_nasid, UNIX_DEBUG_LOADADDR);
	    while (pa < prot_ov_end) {
		for (ri = 0; ri < mr; ri++) {
		    if (partition_region_mask & (1UL << ri)) 
			*(__psunsigned_t *)BDPRT_ENTRY(pa, ri) = la;	
		}
		pa += MD_PAGE_SIZE;
	    }
	}
    }
#endif /* CELL_IRIX && LOGICAL_CELLS */
}

static void
part_permit_xppage(xp_t *xp, __uint64_t ar, __uint64_t rr)
/*
 * Function: 	part_permit_xppage
 * Purpose:	Do actual permit work
 * Parameters:	xp - pointer to cross partition page structure.
 *		ar - regions being added tp permit list
 *		rr - regions being removed from permit list.
 * Returns: nothing
 */
{
    __uint64_t	or, nr;			/* original/new region mask */
    pfd_t	*pfd, *pfd_end;
    int		il;

    XP_LOCK(xp);

    or = xp->xp_regions;
    rr &= or;				/* remove only require regions */
    nr = (or | ar) & ~rr;

    xp->xp_regions = nr;		/* Set new regions */

    pfd = xp->xp_pfd;
    pfd_end = pfd + xp->xp_size / NBPP;

    if (rr) {				/* If removing permissions */
	il = splerr();
	xp->xp_cpuid = cpuid();
	xp->xp_flags |= XP_F_CLEANING;	/* Mark as cleaning */
	
	do {
	    part_pfdat_depermit(pfd, or, nr);
	} while (++pfd < pfd_end);
	xp->xp_cpuid  = cpuid();
	xp->xp_flags &= ~XP_F_CLEANING;	/* Clear cleaning flag */
	splx(il);
    } else {
	do {
	    part_pfdat_permit(pfd, or, nr);
	} while (++pfd < pfd_end);
    }

    XP_UNLOCK(xp);
}    

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
    xp_t	*xp;
    __uint64_t	rr, ar;			/* remove/add regions */
    partition_t	*p = part_get(partid);

    if (p) {
	xp = part_find_page(pfdattophys(pfd));
	ASSERT(xp);

	switch(a) {
	case pAccessNone:
	    rr = p->p_regions;
	    ar = 0UL;
	    break;
	case pAccessRW:
	    ar = p->p_regions;
	    rr = 0UL;
	    break;
	default:
	    cmn_err(CE_PANIC, "part_permit_page: invalid access(%d)\n", a);
	}
	part_permit_xppage(xp, ar, rr);
	part_release_page(xp);
    }
}

static	void
part_pfdat_permit(pfd_t *pfd, __uint64_t or, __uint64_t nr)
/*
 * Function:	part_pfdat_permit
 * Purpose:	Set page permissions for a particular pfdat
 * Parameters:	pfd - pfdat to set permissions for
 *		or  - old region mask
 *		nr  - new region mask
 * Returns:	Nothing
 * Notes:	Does not pull bnack cache line, only sets permissions 
 *		using back door entries.
 */
{
    __uint64_t	a, na;
    __uint32_t	cr;			/* access/noaccess/current region */
    paddr_t	pa, pe; 		/* start/end address */

    or ^= nr;				/* regions that are changing */

    pa = pfdattophys(pfd);
    pe = pa + NBPP;

    if (MD_PREMIUM(pa)) {
	a = (MD_PROT_RW << MD_PPROT_SHFT) | (MD_PROT_RW << MD_PPROT_IO_SHFT);
	na = (MD_PROT_NO << MD_PPROT_SHFT) | (MD_PROT_NO << MD_PPROT_IO_SHFT);
    } else {
	a = MD_PROT_RW << MD_SPROT_SHFT;
	na = MD_PROT_NO << MD_SPROT_SHFT;
    }

    while (pa < pe) {
       for (cr = 0; cr < MAX_REGIONS; cr++) {
	   if (or & (1UL << cr)) {	/* Affected region ? */
	       if (nr & (1UL << cr)) {
		   *(__psunsigned_t *)BDPRT_ENTRY(pa, cr) = a;
	       } else {
		   *(__psunsigned_t *)BDPRT_ENTRY(pa, cr) = na;
	       }
	   }
	}
        pa += MD_PAGE_SIZE;
    }
    return;
}

/*REFERENCED*/
static void
part_pfdat_depermit(pfd_t *pfd, __uint64_t or, __uint64_t nr)
/*
 * Function: 	part_pfdat_depermit
 * Purpose:	To safely clean a region by pulling all cache lines 
 *		associated with that region locally. 
 * Parameters:	pfd - pointer to pfdat descirbing page, MUST BE NBPP
 *		    pfdat, if a large page, this routine must be called
 *		    multiple times.
 *		or  - old region mask indicating pfd permissions.
 *		nr  - new region mask, indicating pfd permissions
 *		    to be put in place.
 * Returns:	Nothing
 *
 * Notes: 
 * Cleaning a page is done as follows:
 *
 * 1) Rex each cache line in page, hopefully pulling cache line out of 
 *    each cpu that has it (shared or ex), ignore errors. 
 * 2) Depermit page from remote partitions that should no longer have access.
 * 3) Rex each cache line, if owned or shared remotely, poison 
 *    lines that fail to get pulled. If a remote partition CPU has
 *    aquired a shared/ex copy before permissions were revoked, this will
 *    cause a remote write back, which will fail, and poison the cache 
 *    line locally. 
 * 4) For each line in page, be sure is is not shared or owned by the 
 *    partition we are removing it from.
 *
 * MUST BE CALLED AT SPLERR.
 */
{
    extern 	char *dir_state_str[];
    paddr_t	p, pa, pe;		/* current/start/end address */
    hubreg_t	elo, n_elo;		/* cur/new directory lo */
    int32_t	word;			/* dummy word */
    int		state, n_state;		/* cur/new cache line state */
    __uint64_t	owner, n_owner;		/* cur/new cache line owner */
    boolean_t	busy;			/* busy state */
    void	*pin;
    int		us;
    __uint64_t	r, rr;			/* changed/removed regions */

    r = or ^ nr;			/* regions affected */
    rr= or & r;				/* removed regions */

    if (rr && curthreadp) {
	/* Don't migrate if we are removing permissions */
	pin = rt_pin_thread();
    }

    pa = pfdattophys(pfd) | K0BASE_EXL; /* read exclusive */
    pe = pa + NBPP;

    /* Attempt to pull back entire contents of page */

    /* Step (1) */

     __cache_hwb_inval_pfn(0, NBPP, pfdattopfn(pfd));
    for (p = pa; p < pe; p += CACHE_SLINE_SIZE) {
	(void)badaddr_val((volatile void *)p, 4, &word);
    }

    /* Step (2) */

    part_pfdat_permit(pfd, or, nr);

    /* Step (3) */

     __cache_hwb_inval_pfn(0, NBPP, pfdattopfn(pfd));
    for (p = pa; p < pe; p += CACHE_SLINE_SIZE) {
	(void)badaddr_val((volatile void *)p, 4, &word);
    }

    /* Step (4) */

    for (p = pa; p < pe; p+= CACHE_SLINE_SIZE) {
	for (busy = B_TRUE; busy; ) {	/* Start out busy */
	    get_dir_ent(p, &state, &owner, &elo);
	    switch(state) {
	    case MD_DIR_SHARED:
		/* Assert that nothing outside our partition has the line */
		ASSERT(!(rr & owner));
		busy = B_FALSE;
		break;
	    case MD_DIR_EXCLUSIVE:
		/* Assert that only nodes in our partition owns the line */
		ASSERT(nr & (1UL << (owner >> 2)));
		busy = B_FALSE;
		break;
	    case MD_DIR_POISONED:
		DPRINTF(("part_clean_pfdat: pa=0x%x state=(poison)\n", p));
		busy = B_FALSE;
		break;
	    case MD_DIR_BUSY_SHARED:
	    case MD_DIR_BUSY_EXCL:
	    case MD_DIR_WAIT:
		DPRINTF(("part_clean_pfdat: pa=0x%x state=(%s) "
			 "owner=0x%x\n", p, dir_state_str[state], owner));
		/*
		 * Give the directory lots of time to get replies. If after 
		 * 100us, the state appears not to have changed, then assume
		 * the worst.
		 */
		us = 0;
		do {
		    us_delay(1);
		    get_dir_ent(p, &n_state, &n_owner, &n_elo);
		} while ((++us<100) && (n_state==state) && (n_owner==owner));
		if ((n_state != state) || (n_owner != owner)) {
		    break;		/* switch(state) */
		}
		/*
		 * It appears we are stuck - time to go figure out how to 
		 * clean up the directory.
		 * 
		 * BUSY_SHARED: owner points to the excl owner.
		 * BUSY_EXCL:   owner points to the excl requestor, don't know
		 *		which region is holding things up.
		 * WAIT:   	owner points to the excl own requestor.
		 *
		 * In the case of BUSY_EXCL, we assume that a node outside
		 * our partition is holding things up since if it is internal, 
		 * things are already toasted beyond repair. 
		 */
		set_dir_state(p, MD_DIR_POISONED); /* Mark line poison */
		break;
	    case MD_DIR_UNOWNED:	/* Happy as a clam */
		busy = B_FALSE;
		break;
	    default:
		cmn_err(CE_PANIC, "Invalid partition directory state: "
			"pa=0x%x state=%d\n", p, state);
	    }
	}
    }

    if (rr && curthreadp) {
	rt_unpin_thread(pin);
    }

    return;
}

static	void
part_pfdat_flags(pfd_t *pfd, size_t s, __uint64_t set, __uint64_t clr)
/*
 * Function: 	part_pfdat_flags
 * Purpose:	Set/clear flags for all pfdats in a page.
 * Parameters:	pfd - pointer to base pfd for a page
 *		s   - size of page in bytes
 *		set - bit mask of bits to set
 *		clr - bit mask of bits to clear
 * Return:	Nothing.
 */
{
    pfd_t	*p, *pe;

    pe = pfd + s / NBPP;

    if (set) {
	for (p  = pfd; p < pe; p++) {
	    pfd_setflags(p, set);
	}
    }
    if (clr) {
	for (p  = pfd; p < pe; p++) {
	    pfd_clrflags(p, clr);
	}
    }	
}
	


/* ARGSUSED */
pfd_t	*
part_page_allocate_node(cnodeid_t cn, partid_t partid, pAccess_t a, uint ck, 
			int flags, size_t sz, part_error_handler_t eh)
/*
 * Function: 	part_page_allocate_node
 * Purpose:	To allocate a page of memory for cross partition mapping.
 * Parameters:	cnodeid - compact nodeid indicating where to allocate memory
 *			  from.
 *		partid  - partition id that is allowed access to this page.
 *		ck	- cache key (same as large_pagealloc)
 *		flags	- (passed to large_pagealloc).
 *		size	- size of page requested (same as large_pagealloc).
 *		size 	- page size as passed to partPageAllocNode.
 *		eh	- error handler
 * Returns:	Nothing
 */
{
    pfd_t	*pfd;
    xp_t	*xp;

    if (pfd = pagealloc_size_node(cn, ck, flags, sz)) {
	xp = part_new_page(pfd, 0, sz, 0, eh);
	part_pfdat_flags(pfd, sz, P_XP, 0UL);
	ASSERT(xp);
	part_permit_xppage(xp, part_get(partid)->p_regions, 0);
    }
    return(pfd);
}

/*ARGSUSED*/
pfd_t	*
part_page_allocate(partid_t partid, pAccess_t a, uint ck, int flags, 
		   size_t sz, part_error_handler_t eh)
/*
 * Function: 	part_page_allocate
 * Purpose:	To allocate a page of memory for cross partition mapping.
 * Parameters:	partid  - partition id that is allowed access to this page.
 *		a	- access to allow partition specified.
 *		ck	- cache key (same as large_pagealloc)
 *		flags	- (passed to large_pagealloc).
 *		size	- size of page requested (same as large_pagealloc).
 *		size    - page size as passed to partPageAllocNode.
 *		eh 	- error handler pointer
 * Returns:	pointer to pfd if success.
 */
{
    pfd_t	*pfd;
    xp_t	*xp;

    if (pfd = pagealloc_size(ck, flags, sz)) {
	if (xp = part_new_page(pfd, 0, sz, 0, eh)) {
	    ASSERT(xp);
	    part_pfdat_flags(pfd, sz, P_XP, 0UL);
	    part_permit_xppage(xp, part_get(partid)->p_regions, 0);
	} else {
	    (void)pagefree_size(pfd, sz);
	    pfd = NULL;
	}
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
    paddr_t	pa, pe;			/* physical address start/end */
    int		state;			/* directory state */
    __uint64_t	owner;			/* NOTUSED: directory bitvec/owner */
    hubreg_t	elo;			/* NOTUSED: directory LO entry */
    partition_t	*p = part_get(part_id());
    pfd_t	*pf, *pfe;

    xp = part_find_page(pfdattophys(pfd));
    ASSERT(xp);

    part_permit_xppage(xp, p->p_regions, ~p->p_regions);

    pf  = xp->xp_pfd;
    pfe = xp->xp_pfd + xp->xp_size / NBPP;

    while (pf < pfe) {
	if (!pfdat_ispoison(xp->xp_pfd) && !pfdat_iserror(xp->xp_pfd)) {
	    /* 
	     * The cleaning process may have left things POISON, so clean 
	     * that up now. If the pfdat is marked as poison, then we know
	     * the entire allocated page was poisoned, so we leave it and
	     * let the VM system handle it.
	     */
	    pa = pfdattophys(pf);
	    pe = pa + NBPP;

	    while (pa < pe) {
		get_dir_ent(pa, &state, &owner, &elo);
		if (state == MD_DIR_POISONED) {
		    set_dir_state(pa, MD_DIR_UNOWNED);
		}
		pa += CACHE_SLINE_SIZE;
	    }
	}
	pf++;
    }
	
    part_release_page(xp);		/* for lookup above */
    part_release_page(xp);		/* actually cause free */
}

static	void
part_interrupt_thread_exit(void *arg)
/*
 * Function: 	part_interrupt_thread_exit
 * Purpose:	End the life of an interrupt thread.
 * Parameters:	arg - PTS pointer
 * Returns:	Does not return.
 * Notes: 	If we are the last thread to go away, we wake up the 
 *		exit semaphore .
 */
{
    pts_t	*pts = (pts_t *)arg;
    partition_t	*p = pts->pts_partition;

    DPRINTFV(("part_interrupt_thread_exit: p(%d) xt(0x%x) exitting\n", 
	      p->p_id, pts->pts_xthread));
    part_dequeue_xthread(pts);
    (void)atomicAddInt(&pts->pts_partition->p_xthread_exit, 1);
    if (!(pts->pts_flags & PTS_F_ACTIVE)) {
	/* If we were idle, decrement idle count */
	(void)atomicAddInt(&p->p_xthread_idle, -1);
    }
    kmem_free(pts, sizeof(*pts));
    xthread_exit();
}    

/*ARGSUSED*/
static void
part_activate(void *arg, int idle)
/*
 * Function: 	part_activate
 * Purpose:	To set up HUB chip to allow current partition to access the 
 *		specified partition, and start an interrupt thread for the
 *		partition.
 * Parameters:	arg - partition # to activate.
 *		idle - # idle threads this partition.
 * Returns:	Nothing.
 * Notes:	This function simply updates the HUB chips to allow us to 
 *		make requests to the regions specified. It is up to that
 *		partition/region to actually permit the pages.
 */
{
    partition_t	*p = part_get((partid_t)(__psunsigned_t)arg);
    hubreg_t	r;
    cnodeid_t	cn;
    nasid_t	nasid;
    pn_t	*pn;			/* partition node pointer */
    kldir_ent_t	kld;
    part_var_t	*xpp, xp;		/* Remote pointer/copy */
    partid_t	spid;
    int		il;			/* SPL level */

    DPRINTF(("part_activate: partition(%d) state(%s)\n", 
	     p->p_id, part_state[p->p_state]));

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
    p->p_state        = pActivateHB;	/* activate in progress */
    p->p_heartbeat    = 0;		/* clear heartbeat */
    p->p_heartbeat_to = 0;		/* clear heartbeat TO */

    /* Try to locate Cross partition Parameters */

    for (nasid = 0; nasid < MAX_NASIDS; nasid++) {
	if (NASID_TO_PARTID(nasid) == p->p_id) { /* looks like a match */
	    if (part_copy_fault((kldir_ent_t *)KLD_KERN_XP(nasid), 
				&kld, sizeof(kld))) {
		P_UNLOCK(p);
		part_deactivate(p->p_id);
		return;
	    }

	    /* Verify it is valid */

	    if ((kld.magic != KLDIR_MAGIC) || 0 == kld.pointer) {
		continue;
	    }
	    xpp = (part_var_t *)kld.pointer;
	    /*
	     * Now verify that the address we have is at least likely 
	     * to exist in the partition that owns nasid specified.
	     */
	    if (p->p_id != (spid = part_from_addr(kld.pointer))) {
		/* Only print message if partition is assigned */
		if (PARTID_VALID(spid)) {
		    cmn_err(CE_WARN, 
			    "XP for partition %d is in partition %d\n",
			    p->p_id, spid);
		} else {
		    DPRINTF(("XP for partition %d: 0x%x: unknown partition\n",
			     p->p_id, kld.pointer));
		}
		P_UNLOCK(p);
		part_deactivate(p->p_id);
		return;
	    }
	    if (part_copy_fault(xpp, &xp, sizeof(xp))) {
		DPRINTF(("part_activate: partition(%d): unable to access"
			 " XP variables\n", p->p_id));
		P_UNLOCK(p);
		part_deactivate(p->p_id);
		return;
	    }
	    if (!PV_VERSION_COMP(PV_VERSION, xp.pv_version)) {
		cmn_err(CE_NOTE,"partition: Remote partition %d noticed\n", 
			p->p_id);
		cmn_err(CE_NOTE, "partition: Local Partition version %d.%d "
			"does not support remote version %d.%d\n", 
			PV_VERSION_MAJOR(PV_VERSION), 
			PV_VERSION_MINOR(PV_VERSION), 
			PV_VERSION_MAJOR(xp.pv_version), 
			PV_VERSION_MINOR(xp.pv_version));
		P_UNLOCK(p);
		part_deactivate(p->p_id);
		return;
	    }
		
	    p->p_xp = (part_var_t *)xpp;
	    break;
	}
    }

    if (!p->p_xp) {
	P_UNLOCK(p);
	part_deactivate(p->p_id);
	return;
    }

    il = mutex_spinlock(&partition_region_lock);
    for (cn = 0; cn < maxnodes; cn++) {
	nasid = COMPACT_TO_NASID_NODEID(cn);
	r = REMOTE_HUB_L(nasid, PI_REGION_PRESENT);
	r |= p->p_regions;
	REMOTE_HUB_S(nasid, PI_REGION_PRESENT, r);
    }
    mutex_spinunlock(&partition_region_lock, il);

    if (p->p_id != partition_partid) { /* don't interrupt self */
	pn = part_get_nasid(p->p_id, NULL); /* pick a nasid */
	ASSERT(pn);
	/* If this fails, the error is ignored. */
	REMOTE_HUB_S(pn->pn_nasid, 
		     pn->pn_cpuA ? PI_CC_PEND_SET_A : PI_CC_PEND_SET_B,
		     0);
    }

    /* Mark new partition as being "known" */

    il = mutex_spinlock(&partition_vars_lock);
    partition_vars->pv_vars[PART_VAR_HB_PM] |= (1UL << p->p_id);
    mutex_spinunlock(&partition_vars_lock, il);

    P_UNLOCK(p);
    part_call_activate_handlers(p);
    /*
     * If the partition is still in the activate-in-progress state, we 
     * finish up now. If not, we could only be deactivating - but in that
     * case the ipsema will return to the deactivation handler. 
     */
    P_LOCK(p);

    if(p->p_state == pActivateHB) {
	sv_wait(&p->p_xsv, PZERO, &p->p_lock, p->p_lock_spl);
	P_LOCK(p);
    }

    if (p->p_state == pActivate) {
	p->p_state = pReady;
    }
    P_UNLOCK(p);
    return;
 }

void
part_deactivate(partid_t partid)
/*
 * Function:    part_deactive
 * Purpose:     Request the deactivation of a specific partition.
 * Parameters:  partid - id of partition to deactive.
 * Returns:     Nothing.
 * Notes:       This routine is asynchronous, deactivation requests are 
 *              queued, and processed at some later time.
 */
{
    partition_t *p = part_get(partid);
    pts_t       *pts;

    ASSERT(partid != partition_partid);

    if (p) {
	P_LOCK(p);
	switch(p->p_state) {
	case pIdle:
	case pDeactivate:
	case pDeactivateReq:
	    break;
	case pActivateHB:
	    p->p_state = pDeactivateReq;
	    sv_broadcast(&p->p_xsv);
	    /*FALLTHROUGH*/
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
	    vsema(&p->p_isema);	
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
 *		the interrupt threads. 
 * Parameters:	partid - partition number being deactivated.
 * Returns:	nothing
 */
{
    partition_t	*p = pts->pts_partition;
    cnodeid_t	cn;			/* compact node id */
    __uint32_t	r;			/* region index */
    nasid_t	n;			/* nasid # */
    hubreg_t	rm;			/* temp region mask value */
    avlnode_t	*a, *na;		/* avlnode/next avlnode */
    xp_t	*xp;			/* xp page pointer */
    __uint64_t	pm;			/* partition mask */
    int		il;			/* SPL level */

    DPRINTF(("part_terminate: partid(%d) state(%s)\n", 
	     p->p_id, part_state[p->p_state]));

    ASSERT(p && (p->p_id != partition_partid));

    P_LOCK(p);

    if (p->p_state != pDeactivateReq) {
	P_UNLOCK(p);
	part_interrupt_thread_exit(pts);
    }
    p->p_state = pDeactivate;

    /* Mark new partition as being "unknown" */

    il = mutex_spinlock(&partition_vars_lock);
    (void)part_get_var(partition_partid, PART_VAR_HB_PM, &pm);
    pm &= ~(1UL << p->p_id);
    part_set_var(PART_VAR_HB_PM, pm);
    mutex_spinunlock(&partition_vars_lock, il);

    il = mutex_spinlock(&partition_region_lock);
    for (cn = 0; cn < maxnodes; cn++) {
	n = COMPACT_TO_NASID_NODEID(cn);
	rm = REMOTE_HUB_L(n, PI_REGION_PRESENT);
	rm &= ~p->p_regions;
	rm |= 1;			/* MUST HAVE NASID 0 due to HUB BUG */
	REMOTE_HUB_S(n, PI_REGION_PRESENT, rm);
    }
    mutex_spinunlock(&partition_region_lock, il);

    p->p_regions = 0;

    for (n = 0; n < MAX_NASIDS; n++) {
	if (p->p_id == partition_nodes[n].pn_partid) {
	    partition_nodes[n].pn_partid = INVALID_PARTID;
	    partition_nodes[n].pn_cpuA 	 = 0;
	    partition_nodes[n].pn_cpuB 	 = 0;
	    partition_nodes[n].pn_state	 = KLP_STATE_INVALID;
	    
	}
    }

    for (r = 0; r < MAX_REGIONS; r++) {
	if (p == partition_regions[r].pr_partition) {
	    partition_regions[r].pr_partition = &partition_idle;
	    partition_regions[r].pr_int_count = 0;
	}
    }	
    P_UNLOCK(p);

    /* Notify whoever wants to know about this. */

    part_call_deactivate_handlers(p);
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
	sv_wait(&p->p_xsv, PZERO, &p->p_lock, p->p_lock_spl);
	P_LOCK(p);
    }
    ASSERT(p->p_state == pDeactivate);
    p->p_state = pIdle;
    P_UNLOCK(p);
    part_interrupt_thread_exit(pts);
}

static	int
part_copy_fault(void *src, void *dst, int len)
/*
 * Function: 	part_copy_fault
 * Purpose:	Attempt a "safe" copy from one address to another.
 * Parameters:	src - source to copy from (accesses to this address 
 *		      are allowed to fail).
 *		dst - destination of copy (accesses to this address
 *		      are NOT allow to fail).
 *		len - number of bytes to copy.
 * Returns:	0 - success
 *		!0- failed
 */
{
    extern	int	_badaddr_val(void *, int, void *);
    ASSERT((((__psunsigned_t)src|(__psunsigned_t)dst|(__psunsigned_t)len) & 3)
	   == 0);
    while (len -= 4) {
	if (_badaddr_val(src, 4, dst)) {
	    return(-1);
	}
	src = (void *)((__psunsigned_t)src + 4);
	dst = (void *)((__psunsigned_t)dst + 4);
    }
    return(0);
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
    kldir_ent_t	kld;			/* copy of kldir entry */
    partition_t	*p;			/* partition pointer */
    pn_t	*pn;			/* partition node pointer */
    partid_t	pid;			/* partition id */
    klp_t	klp;			/* KL partition entry */
    int		accFailed;		/* Failed on copy */
    __uint32_t	r;			/* region # */
    __uint64_t	rm;			/* region mask */
    int		il;			/* interrupt level */
		  
    pn = &partition_nodes[n];
    /*
     * Figure out region we are about to touch and be sure the hub 
     *  will let us
     */ 
    r = n >> (partition_fine ? 
	      NASID_TO_FINEREG_SHFT : NASID_TO_COARSEREG_SHFT);

    il = mutex_spinlock(&partition_region_lock);
    rm = LOCAL_HUB_L(PI_REGION_PRESENT);
    if (!(rm & (1UL << r))) {
	LOCAL_HUB_S(PI_REGION_PRESENT, rm | (1UL << r));
    }

    accFailed = part_copy_fault((void *)KLD_KERN_PARTID(n),&kld, sizeof(kld));

    if (!(rm & (1UL << r))) {		/* restore region mask */
	LOCAL_HUB_S(PI_REGION_PRESENT, rm);
    }
    mutex_spinunlock(&partition_region_lock, il);

    if (accFailed) {
	/* Failed to access KLDIR entry for this nasid */
	if (pn->pn_partid != INVALID_PARTID) {
	    part_deactivate(pn->pn_partid);
	    cmn_err(CE_WARN, 
		    "NASID 0x%x partition[%d] no longer responding\n", 
		    n, pn->pn_partid);
	}
	pn->pn_nasid = INVALID_NASID;
	return;
    } else {
	pn->pn_nasid = n;
    }

    /* Verify entry is valid, since node responded OK */

    if (kld.pointer & 7) {
	DPRINTF(("part_probe_nasid: nasid(0x%x) KLP not aligned (0x%x)\n", 
		 n, kld.pointer));
	pn->pn_state = KLP_STATE_INVALID;
	return;
    } else if (part_copy_fault((void *)PHYS_TO_K1(kld.pointer), &klp, 
			       sizeof(klp))) {
	DPRINTF(("part_probe_nasid: nasid(0x%x) KLP not accessible\n", n));
	pn->pn_state = KLP_STATE_INVALID;
	return;
    } else if (kld.magic != KLDIR_MAGIC) {
	DPRINTF(("part_probe_nasid: nasid(0x%x) not ready to partition\n",n));
	pn->pn_state = KLP_STATE_INVALID;
	return;
    } else if (!KLP_VERSION_COMP(klp.klp_version, KLP_VERSION)) {
	DPRINTF(("part_probe_nasid: nasid(0x%x) incompatable "
		 "KLP version (%d.%d)\n", n,
		 KLP_VERSION_MAJOR(klp.klp_version),
		 KLP_VERSION_MINOR(klp.klp_version)));
	pn->pn_state = KLP_STATE_INVALID;
	return;
    } else if (!KLP_STATE_VALID(klp.klp_state)) {
	DPRINTF(("part_probe_nasid: nasid(0x%x) invalid state (%d)\n", 
		 klp.klp_state));
	pn->pn_state = KLP_STATE_INVALID;
	return;
    } else if (!PARTID_VALID((partid_t)klp.klp_partid)) {
	DPRINTF(("part_probe_nasid: nasid(0x%x) invalid partid(%d)\n", 
		 klp.klp_partid));
	pid = INVALID_PARTID;
    } else {
	pid = klp.klp_partid;
    }

    /* Look for interesting state change */
    if ((pn->pn_partid != INVALID_PARTID) && (pn->pn_partid != pid)) {
	/*
	 * This should never happen, but if it does, be sure old 
	 * partition and new partition are first "deactivated", and 
	 * then allow normal "mode" of operation to re-activate the new
	 * partition later.
	 */
	DPRINTF(("part_probe_nasid: nasid(0x%x) cpu<%c%c> "
		 " *** Reassigned Partid *** o(%d) n(%d)\n",
		 n, klp.klp_cpuA ? 'A' : ' ', klp.klp_cpuB ? 'B' : ' ', 
		 pn->pn_partid, pid));
	part_deactivate(pid);
    } 
    if (pn->pn_state != klp.klp_state) {
	p = &partitions[pid];
	pn->pn_partid    = pid;
	pn->pn_nasid     = n;
	pn->pn_state     = klp.klp_state;
	pn->pn_module    = klp.klp_module;
	pn->pn_cpuA      = klp.klp_cpuA;
	pn->pn_cpuB      = klp.klp_cpuB;
	pn->pn_domainid  = klp.klp_domainid;
	pn->pn_clusterid = klp.klp_cluster;
	pn->pn_cellid    = klp.klp_cellid;
	pn->pn_ncells    = klp.klp_ncells;
#ifdef CELL
	/*
	 * XXX This should not be done here
	 */
	p->p_clusterid = klp.klp_cluster;
	p->p_domainid = klp.klp_domainid;
#endif
	DPRINTF(("part_probe_nasid: nasid(0x%x) cpu<%c%c> "
		 "partid(%d) rgn(%d) module(%d) added\n", n, 
		 klp.klp_cpuA ? 'A' : ' ', 
		 klp.klp_cpuB ? 'B' : ' ', 
		 pn->pn_partid, r, pn->pn_module));

	if (pn->pn_state == KLP_STATE_KERNEL) {
	    PART_RM_SETB(p, r);
	    partition_regions[r].pr_partition = &partitions[pid];
	    /* Mark state transition if idle, and ready to activate. */
	    P_LOCK(p);
	    if (p->p_state == pIdle) {
		p->p_state = pActivateReq;
	    }
	    P_UNLOCK(p);
	}
    }
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
    __uint64_t	vr;			/* vector read value */
    partid_t	pid;
    partition_t	*p;

    partition_discovery_in_progress = B_TRUE;

    /* 
     * To avoid multiple scans which can take a long time, be sure
     * no more are pending.
     */
    while (cpsema(&partition_idle.p_isema))
	;
    
    /*
     * Check what we are connected to. If we are NOT connected to a 
     * router, then time outs will not really happen .... we need to 
     * do vector operations to determine the remote guys nasid.
     */

    if (vector_read(0, 0, NI_STATUS_REV_ID, &vr)) { /* failed - no probe */
	DPRINTF(("part_discovery: Lone Node\n"));
	return;
    } else if ((vr & NSRI_CHIPID_MASK) >> NSRI_CHIPID_SHFT == 0) { /* HUB */
	DPRINTF(("part_discovery: No router\n"));
	/* No probe, simply look at NODE address */
	n = (vr & NSRI_NODEID_MASK) >> NSRI_NODEID_SHFT;
	part_probe_nasid(n);
	part_probe_nasid(cputonasid(cpuid()));
    } else {
	DPRINTF(("part_discovery: Router: Checking for remote partitions\n"));
	for (n = 0; n < MAX_NASIDS; n++) {
	    part_probe_nasid(n);
	}
    } 

    partition_discovery_in_progress = B_FALSE;

    /*
     * Now check which partitions need to conitnue activation, and 
     * start up activation threads on them.
     */

    for (pid = 0; pid < MAX_PARTITIONS; pid++) {
	p = &partitions[pid];
	P_LOCK(p);
	switch(p->p_state) {
	case pActivateReq:
	    p->p_state = pActivateWait;
	    /*FALLTHROUGH*/
	case pDeactivateReq:
	    /*
	     * If deactivation request has appeared already, be sure to
	     * launch the threads that are required for cleanup.
	     */
	    P_UNLOCK(p);
	    part_interrupt_set(pid, 2, part_activate);
	    part_start_partition_thread(p, 2, B_TRUE);
	    break;
	default:
	    P_UNLOCK(p);
	}
    }
}

/*ARGSUSED*/
static	void
part_spurious_interrupt(void *arg, int idle)
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
 * Parameters:	vp - pts pointer to "our" thread.
 * Returns:	Does not return.
 * Notes:	This routine is a wrapper for threaded partition interrupts. 
 *		It uses ipsema, which will cause the thread to resume
 *		execution at the start of the routine when another interrupt
 *		has arrived.
 *		
 *		This routine will call the interrupt handler at least once
 *		on startup. This allows a single remaining thread to 
 *		start a thread and go to sleep.
 */
{
    pts_t	*pts = (pts_t *)vp;
    partition_t *p = pts->pts_partition;
    int		idle;			/* # idle  threads */

    pts->pts_flags |= PTS_F_ACTIVE;

    /* Process as many as we can before possibly dying. */

    while (1) {
        idle = p->p_xthread_started + atomicAddInt(&p->p_xthread_idle, -1);
	if (idle < 0) {
	    /* Could be < 0 for a short period. */
	    idle = 0;
	}
	p->p_int((void *)(__psunsigned_t)p->p_id, idle);
	/*
	 * If we are NOT the last thread, xthread_requested or interrupt 
	 * semaphore can cause use to run. If we are the last thread, 
	 * only a vsema on the interrupt semaphore can cause us to run.
	 */

	idle = atomicAddInt(&p->p_xthread_idle, 1);

	ASSERT(idle > 0);
	if (idle == 1) {
	    /* We are only thread idle - can only handle interrupts */
	    pts->pts_flags &= ~PTS_F_ACTIVE;
	    ipsema(&p->p_isema);
	} 
	if (p->p_xthread_requested) {
	    compare_and_dec_int_gt(&p->p_xthread_requested, 0);
	    continue;
	}
	pts->pts_flags &= ~PTS_F_ACTIVE; /* no longer counted as active */
	if (idle > p->p_xthread_limit) {
	    part_interrupt_thread_exit(pts);
	} else {
	    ipsema(&p->p_isema);
	}
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
 * Notes:	The pts_xthread value IS not be assigned yet.
 */
{
    DPRINTFV(("part_interrupt_thread_setup: p(%d) ready\n", 
	      pts->pts_partition->p_id));
    pts->pts_xthread = (xthread_t *)curthreadp;
    xthread_set_func(pts->pts_xthread,(xt_func_t *)part_interrupt_thread,pts);
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
    partition_t	*p;
    hubreg_t	hr;
    int		s = cputoslice(cpuid()); /* slice */
    int		r;			/* region # */
    __uint64_t	pm = 0;			/* partition mask */

    hr = LOCAL_HUB_L(PI_CC_PEND_CLR_A + s * PI_INT_SET_OFFSET);
    LOCAL_HUB_S(PI_CC_PEND_CLR_A + s * PI_INT_SET_OFFSET, hr);
    LOCAL_HUB_CLR_INTR(N_INTPEND_BITS + CC_PEND_A + s);

    for (r = 0; hr; r++, hr >>= 1) {
	if (hr & 1) {			/* this region pending? */
	    p = partition_regions[r].pr_partition;
	    (void)atomicAddLong(&partition_regions[r].pr_int_count, 1);
	    if (0 == (pm & (1UL << p->p_id))) {
		(void)atomicAddLong(&p->p_int_count, 1);
		pm |= 1UL << p->p_id;
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
 * Purpose:	Assign a xthread to a specific partition.
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

    if (++p->p_xthread_cnt > p->p_xthread_cnt_peak) {
	p->p_xthread_cnt_peak = p->p_xthread_cnt;
    }
    
    atomicAddInt(&p->p_xthread_started, -1);
    atomicAddInt(&p->p_xthread_idle, 1); /* start off idle */
    if (p->p_state == pDeactivateReq) {
	/* 
	 * Must be sure this thread does deactivate if partition never 
	 * activated yet. Go ahead and queue it normally, it will dequeue
	 * itself later.
	 */
	P_UNLOCK(p);
	part_terminate(pts);
    } else {
	P_UNLOCK(p);
    }
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
 *		wait - if TRUE, force the create of a new thread, and wait
 *		      for any allocations to finish.
 * Returns:	# of threads started.
 */
{
    pts_t	*pts;
    void	*stack;
    int		c, new, flags, bg;
    extern int	partition_pri;

    for (new = 0, c = 0; c < count; c++) {
	if (!wait && cvsema(&p->p_isema)) { /* someone waiting */
	    continue;
	}

	/*
	 * Allocate pts struck and stack page without waiting. 
	 */
	if (!(pts = kmem_zalloc(sizeof(*pts), wait ? KM_SLEEP:VM_NOSLEEP))) {
	    break;
	}

	stack = kmem_alloc(2*KTHREAD_DEF_STACKSZ, 
			   VM_DIRECT | (wait ? KM_SLEEP : VM_NOSLEEP));
	if (!stack) {
	    kmem_free(pts, sizeof(*pts));
	    break;
	}

	pts->pts_partition = p;
	flags = KT_STACK_MALLOC;
	(void)xthread_create(p->p_name, stack, 2*KTHREAD_DEF_STACKSZ, 
			     flags, partition_pri, KT_PS | KT_BIND, 
			     (xt_func_t *)part_interrupt_thread_setup, pts);
	new++;
	(void)atomicAddInt(&p->p_xthread_started, 1);
    }
    atomicAddInt(&p->p_xthread_create, new);
    if (count > c) { 
	/*
	 * If more requested that started, be sure a returning thread gets
	 * launched. Only do a vsema if it appears we are the first to
	 * hit this condition.
	 */
	bg = count - c;
	atomicAddInt(&p->p_xthread_create_failed, bg);
	if (bg >= atomicAddInt(&p->p_xthread_requested, bg)) {
	    vsema(&p->p_isema);
	}
	if (bg == atomicAddInt(&p->p_xthread_requested_bg, bg)) {
	    vsema(&partition_thread_sema);
	}
    }
    return(c);
}

int
part_start_thread(partid_t partid, int c, boolean_t w)
/*
 * Function: 	part_start_thread
 * Purpose:	Start another interrupt thread for the specified partition.
 * Parameters:	partid - partition id.
 *	        c      - # threads to start
 *		wait   - if true, wait for memory, otherwise, 
 * Returns:	# threads started.
 */
{
    return(part_start_partition_thread(part_get(partid), c, w));
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
	xthread_set_func(pts->pts_xthread,
				(xt_func_t *)part_interrupt_thread_exit, pts);
    }
    /* Be sure to wake everyone up */
    for (i = 0; i < p->p_xthread_cnt; i++) {
	vsema(&p->p_isema);
    }
    sv_broadcast(&p->p_xsv);
    p->p_int = part_spurious_interrupt;
}

void
part_interrupt_set(partid_t partid, int threads, void (*rtn)(void *, int)) 
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
    partition_t *p = part_get(partid);

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
part_init_partition(partition_t *p, partid_t pid, void (*f)(void *, int))
/*
 * Function: 	part_init_partition
 * Purpose:	Initialize a partition structure. 
 * Parameters:	p - pointer to partition structure. 
 *		pid - partition ID. 
 *		f - default interrupt routine.
 * Returns:	Nothing.
 */
{
    if (PARTID_VALID(pid)) {
	sprintf(p->p_name, "part_%d", (int)pid);
    } else {
	sprintf(p->p_name, "part");
    }
    p->p_id 		= pid;
    p->p_state 		= pIdle;
    p->p_xp          	= 0;
    p->p_heartbeat	= 0;
    p->p_heartbeat_to	= 0;
    p->p_handlers_cnt	= 0;
    p->p_int_count   	= 0;
    p->p_int_spurious	= 0;
    p->p_int	    	= f;
    p->p_xthread_cnt 	= 0;
    p->p_xthread_cnt_peak = 0;
    p->p_xthread_idle   = 0;
    p->p_xthread_limit 	= 1;
    p->p_xthread_started= 0;
    p->p_xthread_create = 0;
    p->p_xthread_create_failed= 0;
    p->p_xthread_requested    = 0;
    p->p_xthread_requested_bg = 0;
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
    hubreg_t	nsri;
    __uint32_t  r;			/* region # */
    partid_t    p;			/* partition # */
    nasid_t	n;			/* nasid # */
    cnodeid_t	cn;			/* compact node # */
    cpuid_t	c; 			/* cpuid cpunter */
    lboard_t	*lb;			/* klconfig board pointer */
    klp_t	*klp;			/* pointer to kldir overlay struct */
    kldir_ent_t	*kld;			/* kldir entry pointer */
    part_var_t	*pv;
    size_t	sz;			/* Allocation rounded size */
    static avlops_t avlOps = {
	part_avl_start, 
	part_avl_end
    };
    extern int partition_pri;

    DPRINTF(("Partition initialization ...\n"));

    /* Node map initialization */

    bzero(partition_nodes, sizeof(partition_nodes));

    for (n = 0; n < MAX_NASIDS; n++) {
	partition_nodes[n].pn_partid  = INVALID_PARTID;
	partition_nodes[n].pn_nasid   = INVALID_NASID;
	partition_nodes[n].pn_cpuA    = 0;
	partition_nodes[n].pn_cpuB    = 0;
    }

    /* Region map initialization */
 
    for (r = 0; r < MAX_REGIONS; r++) {
        partition_regions[r].pr_partition = &partition_idle;
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
    part_init_partition(&partition_idle, INVALID_PARTID, 
			(void (*)(void *, int))part_discovery);
    vsema(&partition_idle.p_isema);	/* be sure it runs now */

    spinlock_init(&partition_page_lock, "p_page_lock"); 
    spinlock_init(&partition_vars_lock, "p_vars_lock");
    spinlock_init(&partition_region_lock, "p_region_lock");
    mutex_init(&partition_handlers_lock, MUTEX_DEFAULT, "p_handlers_lock");

    partition_partid = INVALID_PARTID;

    /* 
     * Set protections for hub registers and I/O interrupts, do this 
     * regardless of if partitions are enabled below in case we are 
     * running in a partition system anyway.
     */

    part_init_hub(region_mask);

    /* Verify all nodes can handle partitioning ... */

    for (cn = 0; cn < maxnodes; cn++) {
	if (((REMOTE_HUB_L(COMPACT_TO_NASID_NODEID(cn), NI_STATUS_REV_ID)
	      & NSRI_REV_MASK) >> NSRI_REV_SHFT) < PART_HUBREV) {
	    DPRINTF(("part_init: DR_hubs"));
	    return;
	}
    }

    sz = (sizeof(*pv) + BTE_LEN_MINSIZE - 1) & ~((size_t)BTE_LEN_ALIGN);
    pv = kmem_alloc(sz, VM_DIRECT|VM_CACHEALIGN);
    ASSERT(pv);
    if (bte_poison_range((paddr_t)pv, sz)) {
	/* Don't free memory - don't know if part poison or what */
	cmn_err(CE_WARN, "part_init: unable to poison XP variables");
	cmn_err(CE_WARN, "part_init: system will not partition");
	return;
    }

    /* grab the interrupt vectors for cc interrupts */

    for (c = 0; c < maxcpus; c++) {
	if (cpu_enabled(c)) {
	    if (intr_connect_level(c, CC_PEND_A + cputoslice(c),
		       INTPEND0_MAXMASK, part_interrupt, 0, NULL)) {
		cmn_err(CE_PANIC, "part_init: can't setup interrupts\n");
	    }
	}
    }

    /* figure out our partition id */

    partition_partid = part_get_promid();
    if (!PARTID_VALID(partition_partid)) {
	DPRINTF(("part_init: partitioning disable: %d\n", partition_partid));
	partition_partid = INVALID_PARTID;
	return;
    }

    /* Figure ganularity of regions */

    nsri = LOCAL_HUB_L(NI_STATUS_REV_ID);
    partition_fine = (nsri & NSRI_REGIONSIZE_MASK) >> NSRI_REGIONSIZE_SHFT;
    DPRINTF(("System running in %s grain mode\n", 
	     partition_fine ? "fine" : "coarse"));

    /* Setup and start partition thread */

    init_sema(&partition_thread_sema, 0, "partition_sema", 0);

    sthread_create("partition", NULL, 0, 0, partition_pri, KT_PS, 
		   (st_func_t *)part_thread, 0, 0, 0, 0);

    /* Setup cross partition parameter area */

    pv = (part_var_t *)K0_TO_K1((__psunsigned_t)pv);	/* go uncached */

    bzero(pv, sizeof(*pv));		/* clear vars too */

    pv->pv_version  = PV_VERSION;
    pv->pv_size     = sizeof(*pv);
    pv->pv_count    = PART_VAR_LIMIT;

    pv->pv_vars[PART_VAR_HB]    = 1;
    pv->pv_vars[PART_VAR_HB_FQ] = HZ;
    pv->pv_vars[PART_VAR_HB_PM] = 1UL << partition_partid;
    pv->pv_vars[PART_VAR_STATE] = PV_STATE_RUNNING;

    partition_vars  = pv;

    /* Update symmon values */

    debugger_stopped= PV_STATE_STOPPED;
    debugger_update = &pv->pv_vars[PART_VAR_STATE];

    /* Find all local CPUs - and set up kldir entries */

    for (cn = 0; cn < maxnodes; cn++) {
	n = COMPACT_TO_NASID_NODEID(cn);
	lb = find_lboard((lboard_t *)KL_CONFIG_INFO(n), KLTYPE_IP27);
	ASSERT(lb);
	KLD_KERN_XP(n)->pointer = (__psunsigned_t)pv;
	KLD_KERN_XP(n)->magic   = KLDIR_MAGIC;
	kld = KLD_KERN_PARTID(n);

	if (kld->magic == KLDIR_MAGIC) {
	    klp = (klp_t *)kld->pointer;
	} else {
	    klp = (klp_t *)kld->rsvd;
	    /*
	     * Fill in KLDIR entry if not filled in by prom. 
	     */
	    klp->klp_version  = KLP_VERSION;
	    klp->klp_partid   = partition_partid;
	    klp->klp_cellid   = partition_partid;
	    klp->klp_module   = lb->brd_module;
	    klp->klp_state    = KLP_STATE_LAUNCHED;
	    klp->klp_cluster  = 0xffff;
	    klp->klp_domainid = partition_partid;
	    klp->klp_ncells   = 0;

	    if (cpu_enabled(cnode_slice_to_cpuid(cn, 0))) {
		klp->klp_cpuA = 1;
	    } 
	    if (cpu_enabled(cnode_slice_to_cpuid(cn, 1))) {
		klp->klp_cpuB = 1;
	    }
	    kld->pointer = (__psunsigned_t)klp;
	    kld->offset  = 0;
	    kld->size    = 0;
	    kld->stride  = 0;
	    kld->magic   = KLDIR_MAGIC;
	}

#ifdef	CELL_IRIX
	if (auto_cell_numbering) {
		klp->klp_cluster = base_part * max_subcells + 
						part_subcell_offset;
		klp->klp_domainid = klp->klp_cluster;	
	}
#endif
#ifdef	CELL
	if (is_specified(arg_clusterid))
		klp->klp_cluster = (uchar_t)atoi(arg_clusterid);
#endif
#ifdef	CELL_IRIX
	if (is_specified(arg_domainid))
		klp->klp_domainid = (uchar_t)atoi(arg_domainid);
#endif
	klp->klp_state = KLP_STATE_KERNEL; /* Mark in kernel */
    }
    avl_init_tree(&partition_pages, &avlOps);

    /* Start heartbeats */

    timeout_pri(part_heartbeat, 0, partition_heartbeat_ticks, partition_pri+1);

    /* Register error handler */

    error_reg_partition_handler(part_error_handler);

    /* Set memory protections for EVERY page in our partition */
    part_init_memory(region_mask);

    /* Start a thread for the master partition. */
    (void)part_start_partition_thread(&partition_idle, 1, B_TRUE);
}

partid_t
part_from_addr(paddr_t pa)
/*
 * Function: 	part_from_addr
 * Purpose:	Determine the owning partition of a memory address.
 * Parameters:	pa - physical address to check.
 * Returns:	partid_t of partition owning address. INVALID_PARTID 
 *		returned if no one owns address.
 */
{
    return(partition_nodes[NASID_GET(pa)].pn_partid);
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
part_get_promid(void)
/*
 * Function: 	part_get_promid
 * Purpose:	Read determine the "proms" concept of the local 
 *		partition number.
 * Parameters:	None.
 * Returns:	partid.
 */
{
    gda_t *gda = GDA;
    lboard_t	*lb;			/* klconfig board pointer */
    nasid_t nasid;

    if (gda->g_version < PART_GDA_VERSION) {
	nasid = gda->g_nasidtable[0];
	lb = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_IP27);
	ASSERT(lb);
	gda->g_partid = lb->brd_partition;
	cmn_err(CE_NOTE, "GDA Version(%d) expected Version(%d): "
		"Partition ID set to %d (check for downrev proms)?\n", 
		gda->g_version, PART_GDA_VERSION, gda->g_partid);
    }
    
    return((partid_t)gda->g_partid);
}

#ifdef CELL_IRIX
domainid_t
part_get_domainid(partid_t p)
{
	return partitions[p].p_domainid;
}

clusterid_t
part_get_clusterid(partid_t p)
{
	return partitions[p].p_clusterid;
}
#endif /* CELL_IRIX */

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

    if (PARTID_VALID(p)){
	return(&partitions[p]);
    } else {
	return(NULL);
    }
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

static void
part_poison_xppage(xp_t *xp)
/*
 * Function: 	part_poison_page	
 * Purpose:	Poison a page owned by calling partition.
 * Parameters:	xp - cross partition page to poison.
 * Returns:	Nothing.
 * Notes: 	Writes directory entires indicating page is poison, 
 */
{
    pfd_t	*p, *pe;		/* current/ending pfdat */
    paddr_t	pa;

    XP_LOCK(xp);
    if (!(xp->xp_flags & XP_F_POISON)) {
	xp->xp_flags |= XP_F_POISON;

	/* Need to mark each pfdat associated with the page as poison */

	p  = xp->xp_pfd;
	pe = p + (xp->xp_size / NBPP);
	while (p < pe) {
	    pfd_setflags(p, P_POISONED);
	    p++;
	}
    
	pa = pfdattophys(xp->xp_pfd);
	(void)bte_poison_range(pa, xp->xp_size);
    }
    XP_UNLOCK(xp);
}

void
part_poison_page(pfd_t *pfd)
/*
 * Function:	part_poison_page
 * Purpose: 	Posion the specified page. 
 * Parameters:	pfd - pointer to pfd for page.
 * Returns:	Nothing.
 */
{
    xp_t	*xp;

    xp = part_find_page(pfdattophys(pfd));
    part_poison_xppage(xp);
    part_release_page(xp);
}

error_result_t
part_error_handler(eframe_t *ep, paddr_t paddr, enum part_error_type cause)
/*
 * Function: 	part_error_handler
 * Purpose:	Called ONLY if a bus error occured on a XP page.
 * Parameters:	a - address of access that failed.
 * Returns:	Nothing
 *
 * Notes: This routine can be called as a result of "cleaning" a cross 
 *	  partition page, OR, an error at some random time. If the 
 *	  XP_F_CLEANING bit is on, then the page is being cleaned.
 */
{
    xp_t		*xp;
    part_error_result_t	er;
    int			il;

    /*
     * Do not print this if discovery is happening since all the printouts
     * caused problems with booting.
     */
    if (!partition_discovery_in_progress) {
	DPRINTF(("part_error_handler: eframe=0x%x paddr=0x%x cause=%d\n", 
		 ep, paddr, cause));
    }

    xp = part_find_page(paddr);
    if (!xp) {				/* Do normal thing */
	return(ERROR_USER);
    }

    /*
     * Check if page is undergoing cleaning - if so, just return. 
     */
    if ((xp->xp_flags & XP_F_CLEANING) && (xp->xp_cpuid == cpuid())) {
	part_release_page(xp);
	return(ERROR_USER);
    } else if (xp->xp_flags & XP_F_REGISTER) {
	if (PART_ERR_WB == cause) {
	    /* 
	     * If remote write back - ignore, remote will handle it, and
	     * we are probably on the interrupt stack and can not deal
	     * with everything anyway.
	     */
	    part_release_page(xp);
	    return(ERROR_NONE);
	}
    }

    il = splerr();
    if (!(xp->xp_flags & XP_F_REGISTER)) {
	int		us;
	int		dir_state, n_state;
	__uint64_t	dir_owner, n_owner;
	hubreg_t	dir_lo, n_elo;
	boolean_t	done = B_FALSE;

	while (!done) {
	    get_dir_ent(paddr, &dir_state, &dir_owner, &dir_lo);
	    DPRINTFV(("part_error_handler: a=0x%x state=0x%d "
		      "own=0x%x lo=0x%x\n", 
		      paddr, dir_state, dir_owner, dir_lo));
	    switch(dir_state) {
	    case MD_DIR_SHARED:
	    case MD_DIR_EXCLUSIVE:
	    case MD_DIR_POISONED:
	    case MD_DIR_UNOWNED:
		/* So why did we take an error? */
		done = B_TRUE;
		break;
	    case MD_DIR_BUSY_SHARED:
	    case MD_DIR_BUSY_EXCL:
	    case MD_DIR_WAIT:
		/*
		 * Give the directory lots of time to get replies. If after 
		 * 100us, the state appears not to have changed, then assume
		 * the worst.
		 */
		us = 0;
		do {
		    us_delay(1);
		    get_dir_ent(paddr, &n_state, &n_owner, &n_elo);
		} while ((++us<100)&&(n_state==dir_state)&&(n_owner==dir_owner));
		if ((n_state != dir_state) || (n_owner != dir_owner)) {
		    continue;		/* while */
		}
		set_dir_state(paddr, MD_DIR_POISONED); /* Mark line poison */
		done = B_TRUE;
		break;
	    default:
		cmn_err(CE_PANIC, "Invalid partition directory state: "
			"pa=0x%x state=%d\n", paddr, dir_state);
	    }
	}
    }

    /* Need lock now */

    XP_LOCK(xp);
    if (!(xp->xp_flags & XP_F_ERROR)) {
	xp->xp_flags |= XP_F_ERROR;	/* Error processing in progress */
	if (xp->xp_error) {
	    XP_UNLOCK(xp);
	    splx(il);			/* Don't do this at SPLERR */
	    er = xp->xp_error(ep, paddr, cause);
	    il = splerr();
	    XP_LOCK(xp);
	} else {
	    er = PART_ERROR_RESULT_USER;
	}
	xp->xp_flags &= ~XP_F_ERROR;	/* Error processing done */
    } else {
	/*
	 * If error bit is already set, someone elase is handling it, 
	 * we ignore it, and return. Re-running the cycle could cause a 
	 * problem again, but eventually, we should get here without the 
	 * ERROR flag already set indicating someone else is processing
	 * errors on this page - and deal with whatever we are running into.
	 */
	er = PART_ERROR_RESULT_NONE;
    }
    XP_UNLOCK(xp);
    splx(il);

    part_release_page(xp);
    switch(er) {
    case PART_ERROR_RESULT_NONE:
	return(ERROR_NONE);
    case PART_ERROR_RESULT_HANDLED:
	return(ERROR_HANDLED);
    case PART_ERROR_RESULT_USER:
	return(ERROR_USER);
    case PART_ERROR_RESULT_FATAL:
    default:
	ASSERT(er == PART_ERROR_RESULT_FATAL);
	return(ERROR_FATAL);
    }
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

    xp = part_new_page(NULL, a, s, XP_F_REGISTER, eh);
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

static	void
part_call_activate_handlers(partition_t *p)
/*
 * Function: 	part_call_activate_handlers
 * Purpose:	Call all uncalled activation handlers for a partition
 * Parameters:	p - pointer to partition to call activation handlers for.
 * Returns:	Does not return, continues as normal partition thread.
 */
{
    int			c, e;
    part_status_t	*ps;

    P_LOCK_HANDLERS();
    P_LOCK(p);
    if (p->p_activate_thread) {		/* Do nothing if in progess */
	P_UNLOCK(p);
	P_UNLOCK_HANDLERS();
	return;
    } else {
	p->p_activate_thread = (xthread_t *)curthreadp;
    }
    if ((p->p_state == pReady) || (p->p_state == pActivate) 
	|| (p->p_state == pActivateHB)) {
	while (p->p_handlers_cnt < partition_handlers_cnt) {
	    e = partition_handlers_cnt;
	    for (c = 0, ps = partition_handlers; c < p->p_handlers_cnt; 
		 ps = ps->ps_next, c++)
		;
	    while (c < e) {
		c++;
		if (ps->ps_activate) {

		    /* RACE CONDITION exists right here!!!!! 
		     * There is a chance that we can call the deactivate
		     * handler for the partition right before the
		     * activate.  This could cause some problems if
		     * someone ever needed the deactivate to only come
		     * after an activate. */
		    p->p_handlers_cnt = c;
		    P_UNLOCK(p);
		    P_UNLOCK_HANDLERS();

		    ps->ps_activate(p->p_id);

		    P_LOCK_HANDLERS();
		    P_LOCK(p);

		    /* bail out early if someone has taken us
		     * to another state */
		    if(!(p->p_state == pReady) && 
		       !(p->p_state == pActivate) && 
		       !(p->p_state == pActivateHB)) {
			p->p_activate_thread = NULL;
			P_UNLOCK(p);
			P_UNLOCK_HANDLERS();
			return;
		    }

		} else {
		    p->p_handlers_cnt = c;
		}
		ps = ps->ps_next;
	    }
	}
    }
    p->p_activate_thread = NULL;
    P_UNLOCK(p);
    P_UNLOCK_HANDLERS();
}

static	void
part_activate_thread_setup(partition_t *p)
/*
 * Function: 	part_activate_thread_setup
 * Purpose:	Stub routine for callout thread.
 * Parameters:	p - partition paramter passed to call_activate_handlers.
 * Returns:	Does not return.
 */
{
    part_call_activate_handlers(p);
    xthread_exit();
}

static	void
part_call_deactivate_handlers(partition_t *p)
/*
 * Function: 	part_call_deactivate_handlers
 * Purpose:	Call all uncalled deactivate handlers for a partition
 * Parameters:	p - pointer to partition to call activation handlers for.
 * Returns:	Does not return, continues as normal partition thread.
 */
{
    int			c;
    part_status_t	*ps;

    P_LOCK_HANDLERS();

    P_LOCK(p);
    c = p->p_handlers_cnt;
    p->p_handlers_cnt = 0;
    P_UNLOCK(p);
    
    for (ps = partition_handlers; c > 0; c--) {
	if (ps->ps_deactivate) {
	    ps->ps_deactivate(p->p_id);
	}
	ps = ps->ps_next;
    }
    P_UNLOCK_HANDLERS();
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
    int			c;
    extern int	partition_pri;

    nps = kmem_alloc(sizeof(*nps), 0);

    nps->ps_activate   = ar;
    nps->ps_deactivate = dr;
    nps->ps_next       = NULL;

    P_LOCK_HANDLERS();

    if (!partition_handlers) {
	partition_handlers = nps;
    } else {
	for (ps = partition_handlers; ps->ps_next; ps = ps->ps_next)
	    ;
	ps->ps_next = nps;
    }
    c = partition_handlers_cnt++;	/* Say it is there now */
    P_UNLOCK_HANDLERS();

    /* 
     * For each of the partitions that are not idle, call out. This
     * must be done with a new thread to avoid deadlocks between
     * partitions.
     */

    for (pid = 0; pid < MAX_PARTITIONS; pid++) {
	p = part_get(pid);
	P_LOCK(p);
	if (p && ((p->p_state == pReady) || (p->p_state == pActivate) ||
		  (p->p_state == pActivateHB))) {
	    if (p->p_handlers_cnt == c) {
		P_UNLOCK(p);
		/*
		 * Only if the previous count was equal do we start another
		 * callout. If it was !=, then callouts already started.
		 */
		(void)xthread_create(p->p_name, NULL, 2*KTHREAD_DEF_STACKSZ, 
				     KT_STACK_MALLOC, partition_pri, 
				     KT_PS | KT_BIND, 
				    (xt_func_t *)part_activate_thread_setup,p);
		continue;		/* skip unlock */
	    }
	}
	P_UNLOCK(p);
    }
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
	    if (pn->pn_nasid != INVALID_NASID) {
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
	if (!_CAP_ABLE(CAP_DEVICE_MGT)) {
	    errno = EPERM;
	} else if (a1 == part_id()) {
	    errno = EINVAL;
	} else {
	    part_deactivate((partid_t)a1);
	}
	break;
    case SYSSGI_PARTOP_ACTIVATE:
	if (!_CAP_ABLE(CAP_DEVICE_MGT)) {
	    errno = EPERM;
	} else {
	    vsema(&partition_idle.p_isema); /* Force discovery */
	}
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
	    if (xp->xp_flags & XP_F_LOCK) 	printf("LOCKED ");
	    if (xp->xp_flags & XP_F_CLEANING) 	printf("cleaning(cpu %d) ",
						       xp->xp_cpuid);
	    if (xp->xp_flags & XP_F_ERROR) 	printf("error ");
	    if (xp->xp_flags & XP_F_POISON) 	printf("poison ");
	    if (xp->xp_flags & XP_F_REGISTER) 	printf("registered-remote ");
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
    int		r, fc, i;		/* region/format/loop count */
    nasid_t	n;			/* node */
    pn_t	*pn;			/* partition node */
    pts_t	*pts;
    __psunsigned_t state;		/* remote state */
    char	*cstate;		/* character form - remote state */

    if (part_get_var(p->p_id, PART_VAR_STATE, &state)) {
	cstate = "inaccessible";
    } else if (state > PV_STATE_STOPPED) {
	cstate = "** Unknown **";
    } else {
	cstate = part_var_state[state];
    }
    printf("\nPartition (%d): state(%s) XP-State(%s) handlers(%d)", 
	   p->p_id, part_state[p->p_state], cstate, p->p_handlers_cnt);
    if (p->p_activate_thread) {
	printf(" activate_thread(0x%x)\n", p->p_activate_thread);
    } else {
	printf("\n");\
    }
	
    printf("\tisema(0x%x) pages permit(%d) reg(%d)\n", 
	   &p->p_isema, p->p_page_permit, p->p_page_register);
    printf("\tXP variables: 0x%x\n", p->p_xp);
    for (i = 0; i <= PART_VAR_LIMIT; i++) {
	if (part_get_var(p->p_id, i, &state)) {
	    printf("\t\t[%d] = %s = **** (*)\n", i, part_var[i]);
	} else {
	    printf("\t\t[%d] = %s = 0x%x (%d)\n", i, part_var[i], 
		   state, state);
	}
    }
 
    printf("\tinterrupts:");
    for (r = 0; r < MAX_REGIONS; r++) {
	if (partition_regions[r].pr_partition == p) {
	    printf(" r[%d]=%d", r, partition_regions[r].pr_int_count);
	}
    }
    printf(" tot (%d), spur(%d)\n", p->p_int_count, p->p_int_spurious);
    printf("\tthreads: total(%d) peak(%d) idle(%d) start(%d) req(%d) "
	   "req_bg(%d)\n",
	   p->p_xthread_cnt, p->p_xthread_cnt_peak, p->p_xthread_idle, 
	   p->p_xthread_started, p->p_xthread_requested, 
	   p->p_xthread_requested_bg);
    printf("\t         idle limit(%d) active(%d) "
	   "create(%d) exit(%d) failed create(%d)\n", 
	   p->p_xthread_limit, p->p_xthread_cnt - p->p_xthread_idle, 
	   p->p_xthread_create, p->p_xthread_exit, p->p_xthread_create_failed);

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

    printf("\n\tNodes:\t");
    for (fc = 8, n = 0; n < MAX_NASIDS; n++) {
	pn = &partition_nodes[n];
	if (pn->pn_partid == p->p_id) {
	    if (!fc--) {
		printf("\n\t      \t");
		fc = 10;
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
    printf("\n\thandlers(%d):\n", partition_handlers_cnt);
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
 * Purpose:     Export page to another partition by dropping the firewall
 * Parameters:  paddr - physical addr
 *		size - number of bytes (multiple of NBPP)
 *              part  - partition to export to
 * Returns:     0 = page exported, 1 = page already exported
 */
{
    int			r;		/* region */
    paddr_t		pa, paEnd;	
    int			mr, firsttime=1;
    nasid_t		nasid;
    __uint64_t		la;
    __psunsigned_t 	*protp, prot; 
    partition_t 	*p;
	
    p = part_get(partid);

    nasid = NASID_GET(paddr);
    if(REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM) {
	mr = MAX_PREMIUM_REGIONS;
	la = (MD_PROT_RW<<MD_PPROT_SHFT)|(MD_PROT_RW<<MD_PPROT_IO_SHFT);
    } else {
	mr = MAX_NONPREMIUM_REGIONS;
	la = MD_PROT_RW << MD_SPROT_SHFT;
    }

    for (r = 0; r < mr; r++) {
	if (PART_RM_TSTB(p, r)) {
	    for (pa=paddr, paEnd=pa+size; pa<paEnd; pa+=MD_PAGE_SIZE) {
		protp = (__psunsigned_t*) BDPRT_ENTRY(pa, r);
		if (firsttime) {
		    firsttime = 0;
		    prot = (((*protp) & MD_PPROT_MASK) >> MD_PPROT_SHFT);
		    if (prot == MD_PROT_RW) {
			return(1);
		    }
		}
#ifdef DEBUG
		prot = (((*protp) & MD_PPROT_MASK) >> MD_PPROT_SHFT);
		ASSERT(prot == MD_PROT_NO);
#endif
		*protp = la;
	    }
	}
    }
    return 0;
}

int
part_unexport_page(paddr_t paddr, size_t size, partid_t partid, int clean_cache)
/*
 * Function:    part_unexport_page
 * Purpose:     Unexport pages to another partition by raising the firewalls
 * Parameters:  paddr - physical addr
 *		size - number of bytes (multiple of NBPP)
 *              part  - partition to export to
 * Returns:     0 = page no longer exported to ANY partition,
 *		1 = page still exported to other partitions
 */
{
    int			r;		/* region */
    __psunsigned_t 	*protp, prot; 
    paddr_t		pa, paEnd;	
    nasid_t		nasid;
    partition_t 	*p;
    __uint64_t		ra;
    int			mr, still_exported=0;
    /*REFERENCED*/
    char		ch;
	
    p = part_get(partid);

    /*
     * Pull back any cache lines held by other partition
     * ZZZ - needs to use a nofault mechanism
     */
    if (clean_cache) {
        for (pa=paddr|K0BASE_EXL, paEnd=pa+size; pa<paEnd; pa+=CACHE_SLINE_SIZE)
	    ch = *((volatile char*) pa);
    }

    nasid = NASID_GET(paddr);
    if(REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM) {
	mr = MAX_PREMIUM_REGIONS;
	ra = (MD_PROT_NO<<MD_PPROT_SHFT)|(MD_PROT_NO<<MD_PPROT_IO_SHFT);
    } else {
	mr = MAX_NONPREMIUM_REGIONS;
	ra = MD_PROT_NO << MD_SPROT_SHFT;
    }

    for (r = 0; r < mr; r++) {
	if (PART_RM_TSTB(p, r)) {
	    for (pa=paddr, paEnd=pa+size; pa<paEnd; pa+=MD_PAGE_SIZE) {
		protp = (__psunsigned_t*) BDPRT_ENTRY(pa, r);
#ifdef DEBUG
		prot = (((*protp) & MD_PPROT_MASK) >> MD_PPROT_SHFT);
		ASSERT(prot == MD_PROT_RW);
#endif
		*protp = ra;
	    }
	} else if ( !still_exported && ! LOCAL_REGION(r)) {
	    protp = (__psunsigned_t*) BDPRT_ENTRY(paddr, r);
	    prot = (((*protp) & MD_PPROT_MASK) >> MD_PPROT_SHFT);
	    still_exported = (prot == MD_PROT_RW);
	}
    }

    /*
     * Pull back cache lines again. May have been pulled over again by
     * speculation.
     * ZZZ - needs to use a nofault mechanism
     */
    if (clean_cache) {
        for (pa=paddr|K0BASE_EXL, paEnd=pa+size; pa<paEnd; pa+=CACHE_SLINE_SIZE)
	    ch = *((volatile char*) pa);
    }

    return (still_exported);
}

int
part_get_var(partid_t pid, int vid, __psunsigned_t *v)
/*
 * Function: 	part_get_var
 * Purpose:	Retrieve a remote partitions cross partition parameter.
 * Parameters:	pid - remote partition id to retrieve variable from.
 *		vid - variable id to retrieve
 *		v   - address of value to fill in with variables value.
 * Returns:    	0 - success
 *		!0 - failed.
 */
{
    extern	int	_badaddr_val(void *, int, void *);
    partition_t	*p = part_get(pid);
    paddr_t	pa;

    if (p && p->p_xp) {
	pa = (paddr_t)&(((part_var_t *)(p->p_xp))->pv_vars[vid]);
	pa = PHYS_TO_K1((pa) & TO_PHYS_MASK);
	return(_badaddr_val((void *)pa, sizeof(*v), (void *)v));
    }
    return(-1);
}

void
part_set_var(int vid, __psunsigned_t v)
/*
 * Function: 	part_set_var
 * Purpose:	Set a cross partition variable.
 * Parameters:	vid - variable ID to set
 *		v   - value to set variable to.
 * Returns:     Nothing.
 * Notes:	Cross partition variables are stored uncached in a poisoned
 *		region, and can only be accesses uncached.
 */
{
    partition_vars->pv_vars[vid] = v;
}

static	void
part_heartbeat(void)
/*
 * Function: 	part_heartbeat
 * Purpose:	CHeck that partitions we know about are hearbeating.
 * Parameters:	None.
 * Returns:	Nothing
 */
{
    __uint64_t	hb;			/* heartbeat value */
    __uint64_t	state;			/* current state */
    __uint64_t	pm;			/* partition mask value*/
    partid_t	pid;			/* partition id */
    partition_t	*p;			/* parition pointer */

    for (pid = 0; pid < MAX_PARTITIONS; pid++) {
	p = &partitions[pid];

	P_LOCK(p);
	switch(p->p_state) {
	case pActivateHB:
	    if (p->p_heartbeat_to++ > partition_activate_to) {
		P_UNLOCK(p);
		cmn_err(CE_WARN, "Partition %d not responding: Timeout(AHB)\n",
			pid);
		part_deactivate(pid);
		break;
	    }
	    DPRINTF(("part_heartbeat: Partition %d: waiting for activate HB\n",
		     pid));
	    /*FALLTHROUGH*/
	case pActivate:
	case pReady:
	    if (part_get_var(p->p_id, PART_VAR_HB, (void *)&hb)) {
		cmn_err(CE_WARN, 
			"Partition %d not responding: Access(HB)\n", 
			pid);
		P_UNLOCK(p);
		part_deactivate(pid);
		break;		/* switch */
	    } 
	    if (part_get_var(p->p_id, PART_VAR_HB_PM, (void *)&pm)) {
		cmn_err(CE_WARN, 
			"Partition %d not responding: Access(PM)\n", 
			pid);
		P_UNLOCK(p);
		part_deactivate(pid);
		break;		/* switch */
	    } 
	    if (p->p_heartbeat == hb) { /* Hmmmm - trouble */
		if (part_get_var(p->p_id,PART_VAR_STATE,(void *)&state)){
		    cmn_err(CE_WARN, 
			    "Partition %d not responding: Access(STATE)\n", 
			    pid);
		    P_UNLOCK(p);
		    part_deactivate(pid);
		    break;		/* switch */
		} 
		if (state == PV_STATE_STOPPED) { /* ignore */
		    P_UNLOCK(p);
		    break;		/* switch */
		}
		if (p->p_state != pActivateHB) {
		    cmn_err(CE_WARN, 
			    "Partition %d not responding: Timeout(HB)\n", 
			    pid);
		    P_UNLOCK(p);
		    part_deactivate(pid);
		    break;		/* switch */
		}
	    } 

	    if (!(pm & (1UL << partition_partid))) {
		if (p->p_state != pActivateHB) {
		    cmn_err(CE_WARN, 
			    "Partition %d not responding: Removed(PM)\n", 
			    pid);
		    P_UNLOCK(p);
		    part_deactivate(pid);
		    break;		/* switch */
		}
	    }
	    p->p_heartbeat = hb;
	    if (p->p_state == pActivateHB) {
		/*
		 * If we were waiting for this partition to start 
		 * heartbeating, things are now running so continue
		 * activation.
		 */
		p->p_state = pActivate;
		sv_broadcast(&p->p_xsv);
	    }
	    P_UNLOCK(p);
	    break;
	default:
	    P_UNLOCK(p);
	    break;
	}
    }

    /* Clear debugger flags now - not stopped. */

    partition_vars->pv_vars[PART_VAR_STATE] = PV_STATE_RUNNING;

    timeout(part_heartbeat, 0, partition_heartbeat_ticks);
}

void
part_heart_beater(void)
/*
 * Function:   	part_heart_beater
 * Purpose:	Perform local heart beat. 
 * Parameters:	None
 * Returns:	Nothing
 */

{
    if (partition_vars) {
	partition_vars->pv_vars[PART_VAR_HB]++;
    }
}

static void
part_thread(void)
/*
 * Function: 	part_thread
 * Purpose:	Start threads when interrupt handlers can not.
 * Parameters:	none.
 * Returns:	Never returns.
 */
{
    int		s, r;			/* # started/# requested */
    partid_t	pid;			/* partition ID */
    partition_t	*p;			/* partition pointer */

    while (1) {
	do {
	    s = 0;
	    for (pid = 0; pid < MAX_PARTITIONS; pid++) {
		p = &partitions[pid];
		if (p->p_xthread_requested_bg > 0) {
		    r = atomicAddInt(&p->p_xthread_requested, -1);
		    if (r < 0) {
			/*
			 * This means a thread completed work, and is
			 * about to go to sleep.
			 */
			(void)atomicAddInt(&p->p_xthread_requested, 1);
			p->p_xthread_requested_bg = 0; /* No background */
			continue;	/* for */
		    }
		    DPRINTFV(("part_thread: starting on partition %d\n", 
			      p->p_id));
		    s += part_start_partition_thread(p, 1, B_TRUE);
		    (void)atomicAddInt(&p->p_xthread_requested_bg, -1);
		}
	    }
	} while (s > 0);
	psema(&partition_thread_sema, PZERO);
    }
}
