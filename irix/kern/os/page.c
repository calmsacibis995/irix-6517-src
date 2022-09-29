/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.540 $"

#include <values.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/getpages.h>
#include <sys/immu.h>
#include <sys/map.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/sbd.h>
#include <os/as/region.h>	/* for syspreg */
#include <sys/sema.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/anon.h>
#include <sys/kopt.h>
#include <sys/numa.h>
#include <sys/sysmp.h>
#include <sys/page.h>
#include <sys/lpage.h>
#include <sys/nodepda.h>
#include "sys/atomic_ops.h"
#include <sys/runq.h>
#include <sys/numa_stats.h>
#include <sys/rt.h>
#if KSTACKPOOL || MH_R10000_SPECULATION_WAR
#include <sys/kmem.h>
#include <sys/errno.h>
#include <ksys/migr.h>
#include <os/as/pmap.h>
#endif
#include <sys/tile.h>
#ifdef	DEBUG
#include <ksys/rmap.h>
#endif
#include <sys/ktrace.h>
#include <ksys/vnode_pcache.h>
#include <ksys/vm_pool.h>

extern cnodemask_t kernel_allow_cnodemask;

extern int check_freemem_node(cnodeid_t);
/******************************************************************************/

/*
 * Verification of VM System Module Inclusion
 */

#if defined(NUMA_BASE)
#if !(defined(DISCONTIG_PHYSMEM))
#error <<BOMB! VM MODULE NUMA_BASE>>
#endif
#endif /* NUMA_BASE */

#if defined(PTE_64BIT)
#if !((defined(R4000) && defined(IP19)) || (defined(R10000) && !defined(IP28))\
                                        || (defined(BEAST)))
#error <<BOMB! VM MODULE PTE_64BIT>>
#endif
#endif /* PTE_64BIT */

#if defined(SN)
#if !(defined(NUMA_BASE))
#error <<BOMB! VM MODULE SN0>>
#endif
#endif /* SN0 */

#if defined(NUMA_REPLICATION)
#if !(defined(SN) && defined(PTE_64BIT))
#error <<BOMB! VM MODULE NUMA_REPLICATION>>
#endif
#endif /* NUMA_REPLICATION */

/*
 * DISCONTIG_PHYSMEM has no prerequisites.
 * Reverse Maps are always included
 */


/*******************************************************************************************/

/* Following defines can be used to aid in debugging.  DEBUG_K2_ADDRS will
 * cause allocation routines (i.e. kvpalloc) to return a K2 address even when
 * a "direct" (i.e. K0/K1 address) is available.  DEBUG_USE_BIGMEM will
 * cause the kernel to queue the pages so that large memory addresses are
 * used first.
 */
/*#define DEBUG_K2_ADDRS*/

#ifdef PGLISTDEBUG
int chkhash = -1;
int chkhash1 = -1;
int chkhash2 = -1;
int allocdrop;
int trcpalloc;
int trcvcache;
int s5go[255];
int un[255];
int al[255];
int rpcache[255];
int vpcache[255];
#endif

#ifdef PH_LONG
static void	flush_pheads(pfd_t *, int);
static void	flush_phead(phead_t *, pgszindx_t);
static void	flushcaches(void);
#endif

extern void	requeue(pfd_t *, int);

#if NUMA_BASE
static int	kpalloc_node(cnodeid_t, void *, uint, int);
#else
static int	kpalloc(void *, uint, int);
#define kpalloc_node(node, vaddr, size, flags)   kpalloc(vaddr, size, flags)
#endif

static pfd_t *	check_pfd_range(pfd_t *, pfd_t *, pgno_t, int);
static void	mem_range_init(cnodeid_t, pfn_t, pfn_t);
#ifdef DISCONTIG_PHYSMEM
static void	slot0_mem_init(cnodeid_t, pfn_t, pfn_t);
#endif

static void	kvpfree_k2(void *, uint, int);

sv_t	sv_sxbrk;	/* sync var for xbrk */
#ifdef MH_R10000_SPECULATION_WAR
sv_t	sv_sxbrk_low;	/* sv_sxbrk for low memory only */
sv_t	sv_sxbrk_high;	/* sv_sxbrk for high memory only */
#endif /* MH_R10000_SPECULATION_WAR */
lock_t	memory_lock;	/* spin lock for free mem insertions/deletions/flags */
/* raw counter lock for pfdat struct -- need multiple locks on Everest */
int	pheadshift;	/* shift amount for pheadmask */
char   *kvswap_map;	/* bitmap of swappable sysseg pages */
lock_t	sxbrk_lock;	/* Lock to synchronize sched and sxbrk */


extern cpumask_t isolate_cpumask;
pfd_t *pfd_direct;			/* highest directly-mappable pfd */
#if !DISCONTIG_PHYSMEM
pfd_t *pfdat;
#endif

#ifdef TILES_TO_LPAGES
/*
 * On IP32, we separate the 4K page freelist into three separate pools.
 */ 
tphead_t tphead[NTPHEADS];
#endif /* TILES_TO_LPAGES */

/*
 * Preemption check every HB_PREEMPT_MASK+1 hash bucket searches,
 * every CP_PREEMPT_MASK+1 vnode pagelock cpsema-s.
 */
#define HB_PREEMPT_MASK	0x3f
#define CP_PREEMPT_MASK	0x7f

extern timespec_t	boottime;

#if NOTDEFINED && NUMA_BASE && !SN0
int		pfn_nodeshft, pfd_nodeshft;
__psint_t	pfn_nodemask, pfd_nodemask;
#endif /* NUMA_BASE && !SN0 */

/*
 * VM System Relaxed Global Variables
 */

pfn_t relaxed_global_freemem;         /* global system free memory updated once a second */

/*
 * Statistics counter for gr_osview and others.  Emptymem is the number
 * free pages with no hash association.
 */
int   relaxed_global_emptymem;        /* global free memory with no hash association */

/*
 * Page and memory locking:
 *
 * A process wishing to ensure that a
 * page remains in the hash calls p{i}find with the ``acquire'' bit
 * set: the page reference count will be bumped for the process
 * within p{i}find on behalf of the routine.
 *
 * Another semaphore, upagelock, has been provided for sched
 * and rdublk to protect paging the uarea in and out.
 *
 * Yet another semaphore, swapsema, makes swap block {de}allocation
 * and page {un}hashing atomic.
 *
 *
 * pfdat pf_flags protect and manage individual pages over long transactions.
 *
 *	Pages that have been hashed (P_HASH) but have not been initialized
 *	from backing store will have P_DONE off.  P_WAIT is used to wait
 *	for P_DONE.  (Note that a P_DONE is not turned
 *	off just because a process is writing to (reading from) the page.
 *
 *	P_HASH pages are in the page cache, linked via pf_hchain.
 *
 *	P_QUEUE pages are on a free page queue.  The use count must be 0.
 *	A page in the hash (P_HASH) may also be on the free list, or it
 *	may be attached to a user process or in the midst of i/o or in the
 *	delwri queue.
 *
 *	P_SQUEUE - pages are on shared anon free list. The use count must
 *	be 0. These pages can be actually freed via vhand. They
 *	are hashed and pfind knows how to remove them from the list
 *
 * A spinlock is used to protect all lists and pfdat flags.
 *
 * Only the local routines in os/page.c manipulate this lock.  Processes
 * that wish to increment the use count or set flags all do so via
 * local routines.  (It is legitimate to set or test a flag on a page
 * which is not hashed and is owned by the process or which is otherwise
 * protected by other locks.)
 */


void
pageflagsinc(register pfd_t *pfd, int flag, int set)
{
	register int locktoken = pfdat_lock(pfd);
	ASSERT(pfd->pf_use > 0);
	ASSERT((pfd->pf_flags & (P_QUEUE|P_SQUEUE|P_RECYCLE|P_ANON)) == 0);
	ASSERT(pfd->pf_hchain == NULL || (pfd->pf_flags & P_HASH));
	if (set)
		pfd_nolock_setflags(pfd, flag);
	else
		pfd_nolock_clrflags(pfd, flag);
	pfdat_inc_usecnt(pfd);
	pfdat_unlock(pfd, locktoken);
}

#ifdef NOTYET
/*
 * This routine is used to determine if it is safe to read over
 * pages in the list that have already been initialized.  Doing so
 * could in some cases cause us to lose updates made to the pages.
 * To prevent the loss of any updates, we check to see that the pages
 * are not being referenced by anyone else and that they are not already
 * marked dirty.  We also clear the P_DONE flag in each of the pages
 * to ensure that anyone wishing to reference the pages will wait until
 * our caller is done re-initializing them.
 *
 * This routine returns 1 if it is OK for the caller to reinitialize the
 * pages and 0 otherwise.
 */
int
pagespatch(
	pfd_t	*head_pfd,
	int	count)
{
	int	lock_token;
	int	c;
	pfd_t	*pfd;

	ASSERT(head_pfd != NULL);
	lock_token = mem_lock();

	pfd = head_pfd;
	for (c = count; c > 0; c--) {
		ASSERT(pfd != NULL);
		/*
		 * If any of the pages are being referenced by other than
		 * the caller or are marked dirty then we can't go clearing
		 * the P_DONE bits so fail.
		 */
		if (((pfd->pf_flags & P_DONE) && (pfd->pf_use > 1)) ||
		    (pfd->pf_flags & P_DIRTY)) {
			goto fail;
		}
		pfd = pfd->pf_pchain;
		count--;
	}

	/*
	 * Clear the P_DONE flag in each of the pages.
	 */
	pfd = head_pfd;
	for (c = count; c > 0; c--) {
		ASSERT(pfd != NULL);
		pfd->pf_flags &= ~P_DONE;
		pfd = pfd->pf_pchain;
		count--;
	}

	mem_unlock(lock_token);
	return 1;

 fail:
	mem_unlock(lock_token);
	return 0;
}
#endif

/*
 * Mark page done and waken waiters.
 */
void
pagedone(register pfd_t *pfd)
{
	sv_t	*pwtsv = pfdattopwtsv(pfd);
	int	do_broadcast = 0;
	register int locktoken = pfdat_lock(pfd);

	ASSERT((pfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);
	ASSERT(pfd->pf_hchain == NULL || (pfd->pf_flags & P_HASH));

	pfd_nolock_setflags(pfd, P_DONE);

	if (pfd->pf_flags & P_WAIT) {
		pfd_nolock_clrflags(pfd, P_WAIT);
		do_broadcast = 1;
	}
	pfdat_unlock(pfd, locktoken);
	if (do_broadcast)
		sv_broadcast_bounded(pwtsv);
}

/*
 * External routine for waiting for page completion.
 * Process must reacquire page after return.
 */
void
pagewait(register pfd_t *pfd)
{
	sv_t	*pwtsv = pfdattopwtsv(pfd);
	register int locktoken;

	ASSERT((pfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);
	ASSERT(pfd->pf_hchain == NULL || (pfd->pf_flags & P_HASH));

	while (!(pfd->pf_flags & P_DONE)) {
		locktoken = pfdat_lock(pfd);	/* check for sure */
		if (pfd->pf_flags & P_DONE) {
			pfdat_unlock(pfd, locktoken);
			break;
		}
		pfd_nolock_setflags(pfd, P_WAIT);
		pfdat_sv_wait(pfd, pwtsv, locktoken);
	}
}

/*
 * Mark a page bad. Caused if the backing storage goes away due to
 * truncation or ECC errors etc. 
 */
void
pagebad(pfd_t *pfd)
{
	if (pfd->pf_flags & P_ANON)
		anon_pagebad(pfd);
	else
		vnode_pagebad(pfd->pf_vp, pfd);
}

/*
 * Mark page done and waken waiters.
 */
void
ghostpagedone(register int key)
{
	sv_broadcast(&pfdwtsv[key & PWAITMASK]);
}

void
ghostpagewait(register struct region *rp, register int key)
{
	sv_mrlock_wait(&pfdwtsv[key & PWAITMASK], PZERO, &rp->r_lock);
}


#ifdef _VCE_AVOIDANCE
#define PAGEALLOC_FLAGS_VCE	(VM_STALE|VM_NOSLEEP|VM_VACOLOR)
#endif
#ifdef MH_R10000_SPECULATION_WAR
#define PAGEALLOC_FLAGS		(VM_STALE|VM_NOSLEEP|VM_UNSEQ|VM_DIRECT|VM_NO_DMA)
#else
#define PAGEALLOC_FLAGS		(VM_STALE|VM_NOSLEEP|VM_UNSEQ)
#endif

/*
 * System virtual and physical page allocation/deallocation
 *
 * These routines have been tuned to reduce tlb flushing.
 * Site-wide tlb flushing is required whenever system virtual/physical
 * address pair changes.  This can happen when virtual space is
 * deallocated and reallocated, or when the physical page associated
 * with a virtual address is deallocated and a new page allocated.
 *
 * To reduce the amount of flushing in the first case, virtual addresses
 * are freed into a "stale system addresses" map instead of the system
 * map.  If virtual system addresses run out, tlbsync() is called to flush
 * all tlbs and merge the stale system addresses into the system map.
 *
 * For the second case (deallocating/allocating physical pages to a
 * virtual address), we remember the page with which the virtual address
 * was associated by leaving the page frame number in the kernel page
 * table (corresponding to the virtual address) intact.  Further, we
 * link the kernel page table entries when deallocating the physical
 * page.  When a new page is requested for this virtual address, we can
 * find the previous page via the kernel page table entry and return it if
 * it is unused.  This linked list is occasionally cleared during
 * site-wide tlb flushes.  If the links are zero, then, we don't have to
 * return the same page, because we know that we have flushed all tlbs
 * since the page was associated with that virtual address.  If we haven't
 * flushed all tlbs and the page can't be returned, we must flush the tlbs
 * before allocating a new page.
 *
 * A chart of a kernel page table during allocations/deallocations.
 * (V means PG_VR|PG_G|PG_M|PG_NDREF|PG_SV)

		  kvalloc   kpalloc   kpfree   kvfree   tlb sync  
		  -------   -------   ------   ------   --------
	pg_pde	    0        pfn|V    ~valid     0        0 (queued entries)
	pg_ndx      -          0      queued     -        0 (all)
 */

/*
 * All of the kernel vm routines accept a flags argument:
 *
 *	VM_NOSLEEP   ->	If no memory is immediately available,
 *			return an error.  Only used by routines that
 *			allocate physical memory.
 *	VM_DIRECT    -> Return a ``direct-mapped'' (k0seg or k1seg)
 *			address.
 *	VM_UNCACHED  -> Return a k1seg address (with VM_DIRECT) or address
 *			with no-cache bit (P_N) set in page table entry.
 *	VM_UNSEQ     -> Do not request pages that are sequentially cache
 *			aligned.
 *			This is the default with kvpalloc() when size == 1.
 *	VM_VACOLOR   -> Return a K2seg address that is appropriately
 *			1st level cache colored (1 of 8)
 *	VM_UNCACH_ATTR -> Set of bits which indicate the caller requires 
 *			Special uncached attributes to be set while
 *			returning virtual address. This requires modifying
 *			the virtual address suitably in case of XKPHYS 
 *			uncached address, and setting appropriate bits in
 *			the TLB entry in case of kernel virtual address.
 */

/*
 * Allocate system virtual and physical pages.
 */
void *
kvpalloc(register uint size, register int flags, register int color)
{
#if NUMA_BASE
	register cnodeid_t	node;
	register caddr_t	vaddr;

	/*
	 * First do the reservation if needed. Bug 574170.
	 */
	if (!(flags & VM_NOAVAIL)) {
		if (reservemem(GLOBAL_POOL, size, size, size)) {
			if (flags & VM_NOSLEEP)
				return(NULL);
			kreservemem(GLOBAL_POOL, size, size, size);
		}
	}

	/*
	 * The caller isn't specifying a node, so we pick one here ourselves.
	 * Try the current node first.
	 */
    
       for (;;) {
		node = private.p_nodeid;

		/*
		 * Start getting a page on the node where we are,
		 * If that fails, do a round-robin search.
		 * Don't want to sleep on failure, so pass in VM_NOSLEEP
		 * so that we search next node. Resv has been done.
		 * We just cycle through all nodes at this time.
		 * This should get updated to do a smarter 
		 * topology based search.
		 */
		do  {
			if ((vaddr = kvpalloc_node(node, size, 
				   flags|VM_NOSLEEP|VM_NOAVAIL, color)) != NULL)
				return vaddr;
	    
			node = (node + 1) % numnodes;
		} while (node != private.p_nodeid); 
	
		/*
		 * Tried all the nodes.  Couldn't get any memory.  Caller
		 * said NOSLEEP.  Give up now. Unresv if we resvd before.
		 */

		if (flags & VM_NOSLEEP) {
			if (!(flags & VM_NOAVAIL))
				unreservemem(GLOBAL_POOL, size, size, size);
			return NULL;
		}
	
#ifdef MH_R10000_SPECULATION_WAR
		setsxbrk_class(flags);
#else /* MH_R10000_SPECULATION_WAR */
		setsxbrk();
#endif /* MH_R10000_SPECULATION_WAR */
	}
    
	/*UNREACHED*/
}


/* 
 * Allocate system virtual physical pages on a specific node.
 */

void *
kvpalloc_node_hint(
	register cnodeid_t node,
	register uint size,
	register int flags,
	register int color)
{
	register caddr_t	vaddr;

	if ((vaddr = kvpalloc_node(node, size, flags|VM_NOSLEEP, color)) != NULL)
		return vaddr;

	return(kvpalloc(size, flags, color));
}


/* 
 * Allocate system virtual physical pages on a specific node.
 */

void *
kvpalloc_node(
	register cnodeid_t node,
	register uint size,
	register int flags,
	register int color)
{
#else /* NUMA_BASE */
#define node	((cnodeid_t) 0)
#endif

	register caddr_t vaddr;
	uint		 flg;

#ifdef NUMA_BASE
	ASSERT_ALWAYS(flags&VM_NOSLEEP);
#endif /* NUMA_BASE */

	if (size == 1) {
		/*
		 *	There _might_ be a use some day for a flag which
		 *	forces a specific _physical_ color.  Note that
		 *	this is more stringent than the absence of the
		 *	VM_UNSEQ flag as that is advisory only.  VM_PACOLOR
		 *	would have to fail (return 0) in pagealloc if
		 *	the matching physical page color wasn't available.
		 *	Not worth putting in all that code in pagealloc
		 *	now for a flag that hasn't yet found a need.
		 *
		if (!(flags & VM_PACOLOR))
		 */
#ifdef _VCE_AVOIDANCE
		if (! vce_avoidance)
#endif
			flags |= VM_UNSEQ;
#ifndef DEBUG_K2_ADDRS
		if (!(flags & (VM_VACOLOR|VM_VM)))
			goto one;
#endif
	}

	/*
	 * Want direct-mapped (K0 or K1 seg) address?  Sure.
	 * This becomes the default for one-page allocations.
	 */
	if (flags & (VM_DIRECT|VM_PHYSCONTIG)) {
		register pgno_t pfn;
one:
		if (!(flags & VM_NOAVAIL))
			if (reservemem(GLOBAL_POOL, size, size, size)) {
				if (flags & VM_NOSLEEP)
					return(NULL);
				kreservemem(GLOBAL_POOL, size, size, size);
			}

		/* For single page allocation use pagealloc if all pages
		 * are directly addressable (in K0/K1 space) OR if the caller
		 * doesn't need VM_DIRECT.  We avoid contmemall since it will
		 * perform a relatively slow search of the pfdat AND it ends
		 * up flushing the caches.
		 */
#if  _MIPS_SIM == _ABI64
		if (size == 1)  {
#else
		if ((size == 1) && ((pfd_direct == PFD_HIGH(numnodes-1)) 
						|| !(flags & VM_DIRECT))) {
#endif
			register pfd_t *pfd;

			if ((color == 0) && !(flags & VM_VACOLOR))
				color = NOCOLOR;

			while ((pfd = (
#ifdef _VCE_AVOIDANCE
				   vce_avoidance
			               ? pagealloc_node(node, color,
 					    flags & PAGEALLOC_FLAGS_VCE)
				       :
#endif
				   pagealloc_node(node, 0,
 					    flags & PAGEALLOC_FLAGS))
					) == NULL) {
				if (flags & VM_NOSLEEP) {
					if (!(flags & VM_NOAVAIL))
						unreservemem(GLOBAL_POOL, size, size, size);
					return(NULL);
				}

#ifdef MH_R10000_SPECULATION_WAR
				setsxbrk_class(flags & PAGEALLOC_FLAGS);
#else /* MH_R10000_SPECULATION_WAR */
				setsxbrk();
#endif /* MH_R10000_SPECULATION_WAR */
			}

			pfn = pfdattopfn(pfd);

			/* Set the selective crash dump P_DUMP flag.
			 */

			flg = P_DUMP;
			if (flags & VM_BULKDATA)
			    flg |= P_BULKDATA;
			pfd_setflags(pfd, flg);

			if (flags & (VM_UNCACHED|VM_UNCACHED_PIO) ?
			    (small_K1_pfn(pfn)) :
			    (small_K0_pfn(pfn))) {
				vaddr = (flags & (VM_UNCACHED|VM_UNCACHED_PIO) ?
					small_pfntova_K1(pfn) :
					small_pfntova_K0(pfn));

				/* Modify the uncached address suitably to set 
				 * required uncached attribute bits. 
				 * Mostly used in SN0 
				 */
				if (flags & (VM_UNCACHED|VM_UNCACHED_PIO))
					XKPHYS_UNCACHADDR(vaddr, flags);
#ifdef _VCE_AVOIDANCE
				if (!(flags & (VM_UNCACHED|VM_UNCACHED_PIO)))
					COLOR_VALIDATION(pfd,
							 vcache_color(pfn),
							 0,
							 flags & VM_NOSLEEP);
#endif				    
#ifdef MH_R10000_SPECULATION_WAR
			} else if (pfd < pfd_lowmem) {
				ASSERT(flags & (VM_DIRECT|VM_NO_DMA));
				vaddr = (caddr_t)(EXTK0_BASE + ctob(pfn -
					(SMALLMEM_K0_R10000_PFNMAX + 1)));
#endif
			} else {
				/* Physical page allocated by pagealloc is
				 * not directly addressable in 32-bit K0/K1
				 * so we allocate a virtual address and setup
				 * a mapping for the page.
				 */
				register pgi_t		state;
				register pde_t		*pd;
#ifdef _VCE_AVOIDANCE
				if (vce_avoidance &&
				    ! (flags & VM_VACOLOR) &&
				    pfd_to_vcolor(pfd) != -1)
					vaddr = kvalloc(1, flags | VM_VACOLOR,
							pfd_to_vcolor(pfd));
				else
#endif							
					vaddr = kvalloc(1, flags, color);

				if (vaddr == 0) {
					if (!(flags & VM_NOAVAIL))
						unreservemem(GLOBAL_POOL, 1, 1, 1);
					pagefree(pfd);
					return(NULL);
				}
				state = PG_VR|PG_G|PG_M|PG_SV|
				  (flags&(VM_UNCACHED|VM_UNCACHED_PIO) ? PG_N :
							pte_cachebits());

				if(flags & (VM_UNCACHED|VM_UNCACHED_PIO))
					PTE_SETUNCACH_ATTR(state, flags);

				pd = kvtokptbl(vaddr);
#ifdef _VCE_AVOIDANCE
				if (! (flags & VM_UNCACHED))
					COLOR_VALIDATION(pfd,
							 colorof(vaddr),
							 0,flags);
#endif /* _VCE_AVOIDANCE */
				pg_setpgi(pd, mkpde(state, pfn));
#ifdef MH_R10000_SPECULATION_WAR
                                if (! pg_isnoncache(pd) &&
                                    IS_R10000() &&
                                    iskvirpte(pd) &&
                                    ! (flags & VM_NO_DMA))
                                        krpf_add_reference(pfd,
                                                           kvirptetovfn(pd));
#endif /* MH_R10000_SPECULATION_WAR */
			}
		} else {
			pfd_t *pfd;
			int i, flg;
			ASSERT(flags & (VM_DIRECT|VM_PHYSCONTIG));
			while ((pfd = lpage_alloc_contig_physmem(node, 
					ctob(size), 1, flags)) == NULL) {
				if (flags & VM_NOSLEEP) {
					if (!(flags & VM_NOAVAIL))
						unreservemem(GLOBAL_POOL, size, size, size);
					return(NULL);
				}

				setsxbrk();
			}

			pfn = pfdattopfn(pfd);

			flg = P_DUMP;
			if (flags & VM_BULKDATA)
			    flg |= P_BULKDATA;

			for ( i = 0; i < size; i++, pfd++)
				pfd_setflags(pfd, flg);

#ifdef MH_R10000_SPECULATION_WAR
                        if (!(flags & VM_UNCACHED) &&
			    pfn > SMALLMEM_K0_R10000_PFNMAX &&
			    IS_R10000()) {
                                int i;
                                pde_t *pd;

				if (pfd < pfd_lowmem) {
				    ASSERT(flags & (VM_DIRECT|VM_NO_DMA));
				    vaddr = (caddr_t)(EXTK0_BASE + ctob(pfn -
					(SMALLMEM_K0_R10000_PFNMAX + 1)));
				} else {
                                    vaddr = kvalloc(size,flags,color);
                                    if (vaddr == 0) {
                                            for (i = 0; i < size; i++)
                                                pagefree(pfntopfdat(pfn + i));
                                            if (! (flags & VM_NOAVAIL))
                                                unreservemem(GLOBAL_POOL,size,size,size);
                                        return(NULL);
                                    }
				
                                    for (pd = kvtokptbl(vaddr), i = 0;
                                        i < size; pd++, i++) {
                                        pg_setpgi(pd,
					    mkpde((PG_VR|PG_G|PG_M|PG_SV| \
							pte_cachebits()), \
							(pfn + i)));
                                        krpf_add_reference(pfntopfdat(pfn + i),
                                                   vatovfn(vaddr + ctob(i)));
                                    }
				}
			} else
#endif /* MH_R10000_SPECULATION_WAR */
			{
			ASSERT((flags & (VM_UNCACHED|VM_UNCACHED_PIO)) ?
				small_K1_pfn(pfn) : small_K0_pfn(pfn));
			vaddr = (flags & (VM_UNCACHED|VM_UNCACHED_PIO) ? 
				small_pfntova_K1(pfn) : small_pfntova_K0(pfn));

			}

			/* Modify the uncached address suitably to set 
			 * required uncached attribute bits. 
			 * Mostly used in SN0 
			 */
			if (flags & (VM_UNCACHED|VM_UNCACHED_PIO))
				XKPHYS_UNCACHADDR(vaddr, flags);
#ifdef _VCE_AVOIDANCE
			else {
				int	i;

				for (i = 0; i < size; i++)
					COLOR_VALIDATION(
						pfntopfdat((pfn + i)),
						colorof((vaddr + ctob(i))),
						0, flags);
			}
#endif /* _VCE_AVOIDANCE */
		}
		return(vaddr);
	}

	vaddr = kvalloc(size, flags, color);

	if (vaddr == 0)
		return(NULL);

	if (kpalloc_node(node, vaddr, size, flags) < 0) {
		sptfree(&sptbitmap, size, btoc(vaddr));
		return(NULL);
	}

	/* This following code is an optimization so that we can return
	 * a direct mapped address (i.e. K0/K1) on multiple page
	 * allocations when the allocated pages "happen" to be contiguous.
	 * This tends to occur at system startup time when allocating
	 * large kernel data structures.
	 * Note that we don't really need to require that ALL pages be
	 * directly accesssible, only that the allocated pages are.  But
	 * for now we just do the simple thing.
	 */
	if ((size > 1) && !(flags & VM_VM) && (pfd_direct == PFD_HIGH(numnodes -1))
#ifdef MH_R10000_SPECULATION_WAR
            && !IS_R10000()
#endif /* MH_R10000_SPECULATION_WAR */
            ) {
		pfn_t	firstpfn;
		pgno_t	sz;
		pde_t	*pd;

		pd = kvtokptbl(vaddr);
		firstpfn = pg_getpfn(pd);
		sz = size-1;
		pd++;

		/* Try to detect if the pages are contiguous.
		 */
		do {
			if (pg_getpfn(pd) != (firstpfn+size-sz))
				firstpfn = 0;
			pd++;
			sz--;
		} while (sz && firstpfn);

		if (firstpfn) {
			/*
			 * At this point we know the allocation is
			 * physically contiguous.  Free only the
			 * virtual (K2) address.
			 */
			kvfree(vaddr, size);
			vaddr = (flags&VM_UNCACHED ?
				 small_pfntova_K1(firstpfn) :
				 small_pfntova_K0(firstpfn));

			/* Modify the uncached address suitably to set 
			 * required uncached attribute bits. 
			 * Mostly used in SN0 
			 */
			XKPHYS_UNCACHADDR(vaddr, flags);
#ifdef _VCE_AVOIDANCE
			if (! (flags & VM_UNCACHED))
				COLOR_VALIDATION(pfntopfdat(firstpfn),
						 vcache_color(firstpfn),
						 0,
						 flags & VM_NOSLEEP);
#endif				    
		}
	}
	return(vaddr);
}

/*
 * Allocate kernel virtual address space
 */
void *
kvalloc(register uint size, register int flags, register int color)
{
	register pde_t		*pd;
	register caddr_t	vaddr;
	register pgi_t		state;
	struct sysbitmap *bm;

	ASSERT(size);

	bm = &sptbitmap;
	if ((vaddr = (caddr_t)ctob(sptalloc(bm, size, flags, color, 0))) == 0)
		return (0);

	/* 
	 * VM_UNCACHED is set on all non-EVEREST systems to indicate 
	 * a request for uncached mapping.
	 * VM_UNCACHED_PIO is set on EVEREST systems to indicate an
	 * unached mapping.
	 * We want to hand out uncached mapping only if VM_UNCACHED_PIO
	 * is set in the flag field on EVEREST systems.
	 */

	if (flags & (VM_UNCACHED|VM_UNCACHED_PIO)){
		state = PG_N;
		PTE_SETUNCACH_ATTR(state, flags);
	} else {
		state = pte_cachebits();
	}


#if R4000 || R10000
	state |= PG_G;
#endif
#ifdef KVDEBUG
	{
	int		i;
	uint		pg;
	__psint_t	rv;

	for (i = 0; i < size; i++) {
		rv = tlb_probe(0, vaddr + ctob(i), &pg);
		if (rv != -1 && ((rv & (TLBLO_V|TLBLO_D|TLBLO_G)) != TLBLO_G)) {
			cmn_err(CE_CONT,
				"kvalloc: vaddr 0x%x in tlb! tlblo 0x%x\n",
				vaddr + ctob(i), rv);
			debug(0);
		}
	}
	}
#endif

	pd = kvtokptbl(vaddr);
	while (size--){
		pg_setpgi(pd, state);
		pd++;
	}

	return(vaddr);
}

/*
 * Allocate physical pages for kernel virtual space
 * Kernel page tables are updated 
 * memlock must NOT be locked.
 */
static int
#if NUMA_BASE
kpalloc_node(
	register cnodeid_t node,
#else
kpalloc(
#endif
	register void *vaddr,
	register uint size,
	register int flags)
{
	register pde_t		*pd;
	register pfd_t		*pfd;
	register pgi_t		state;
	register int		i;
	register uint		flg;
	register int		cachekey = btoc(vaddr);

	ASSERT(IS_KSEG2(vaddr));

	if (!(flags & VM_NOAVAIL))
		if (reservemem(GLOBAL_POOL, size, size, size)) {
			if (flags & VM_NOSLEEP)
				return(-1);
			kreservemem(GLOBAL_POOL, size, size, size);
		}

	pd = kvtokptbl(vaddr);
	if ((flags & (VM_UNCACHED|VM_UNCACHED_PIO)) || pg_isnoncache(pd)){
		state = PG_VR | PG_G | PG_M | PG_SV | PG_N;
		/* If mapping requires special uncached attributes to
		 * be set, do it. 
		 * This is primarily used for SN0 
		 */
		PTE_SETUNCACH_ATTR(state, flags);
	}
	else{
		state = PG_VR | PG_G | PG_M | PG_SV |pte_cachebits();
	}

#ifdef MH_R10000_SPECULATION_WAR
        if (IS_R10000() &&
            (! (flags & (VM_NO_DMA | VM_STALE))))
                state &= ~PG_VR;
#endif /* MH_R10000_SPECULATION_WAR */

	for (i = 0 ; i < size ; i++, pd++, cachekey++) {
#ifdef PGLISTDEBUG
		if (!(flags | VM_UNSEQ) && trcpalloc) {
			cmn_err(CE_CONT, "v 0x%x size %d key 0x%x\n",
				vaddr, size, cachekey);
			if (allocdrop)
				debug(0);
		}
#endif
		while ((pfd = pagealloc_node(node, cachekey, flags)) == NULL) {
			if (flags & VM_NOSLEEP) {
				if (!(flags & VM_NOAVAIL))
					unreservemem(GLOBAL_POOL, size, size, size);
				for (pd = kvtokptbl(vaddr) ; i ; i--, pd++) {
					pfd = pdetopfdat(pd);
#ifdef MH_R10000_SPECULATION_WAR
                                        if (! pg_isnoncache(pd) &&
                                            IS_R10000() &&
                                            iskvirpte(pd)) {
                                                __psint_t vfn = kvirptetovfn(pd);

                                                krpf_remove_reference(pfd,vfn);
                                                pg_setpgi(pd, mkpde(PG_G, 0));
                                                unmaptlb(0,vfn);
                                        }
#endif /* MH_R10000_SPECULATION_WAR */
					pagefree(pfd);
					pg_clrvalid(pd);
				}
				return(-1);
			}
#ifdef MH_R10000_SPECULATION_WAR
			setsxbrk_class(flags);
#else /* MH_R10000_SPECULATION_WAR */
			setsxbrk();
#endif /* MH_R10000_SPECULATION_WAR */
		}

		/* Set the selective crash dump P_DUMP flag here since
		 * this is called by kvpalloc().
		 */
		flg = P_DUMP;
		if (flags & VM_BULKDATA)
		    flg |= P_BULKDATA;

		pfd_setflags(pfd, flg);
		COLOR_VALIDATION(pfd,colorof(((caddr_t)vaddr)+ctob(i)),
					     0,flags);
		pg_setpgi(pd, mkpde(state, pfdattopfn(pfd)));
#ifdef MH_R10000_SPECULATION_WAR
                if (! pg_isnoncache(pd) &&
                    IS_R10000() &&
                    iskvirpte(pd))
                        krpf_add_reference(pfd,
                                           kvirptetovfn(pd));
#endif /* MH_R10000_SPECULATION_WAR */
	}

	return(0);
}

/*
 * Allocate a set of contiguous page for a given size and alignment.
 * Both size and alignment is given in bytes. Size is rounded to the next
 * page boundary. Alignment should be a power of 2 pages and is also specified  
 * in bytes.
 */

void *
kmem_contig_alloc(size_t size, size_t alignment, int flags)
{
#if NUMA_BASE
	register cnodeid_t	node;
	register caddr_t	vaddr;

	/*
	 * The caller isn't specifying a node, so we pick one here ourselves.
	 * Try the current node first.
	 */
    
       for (;;) {
		if ((vaddr = kmem_contig_alloc_node(private.p_nodeid, 
				size, alignment, flags|VM_NOSLEEP)) != NULL)
			return vaddr;
	    
		for (node = 0; node < numnodes; node++) {

		    if ((vaddr = kmem_contig_alloc_node(node, size, 
					alignment, flags|VM_NOSLEEP)) != NULL)
			    return vaddr;

		}

		/*
		 * Tried all the nodes.  Couldn't get any memory.  Caller
		 * said NOSLEEP.  Give up now.
		 */

		if (flags & VM_NOSLEEP)
			return NULL;
	
		/*
		 * Wait for a page of this size or higher.
		 */

		lpage_size_wait(size, PZERO);
	}
    
	/*UNREACHED*/
}

void *
kmem_contig_alloc_node(
	cnodeid_t	node,
	size_t		size,
	size_t		alignment,
	int		flags)
{

#else /* NUMA_BASE */
#define node	((cnodeid_t) 0)
#endif /* NUMA_BASE */

        pde_t           *pd;
        caddr_t         vaddr;
        pgi_t           state;
        struct  sysbitmap *bm = &sptbitmap;
	int		i;
	pfd_t		*pfd;
	pgno_t		npgs;

        /*
         * Align the size to the nearest page boundary.
         */

        size= (size + NBPP - 1) & (~(NBPP -1 ));

	/*
	 * Make sure that alignment is a power of 2.
	 */

	alignment = btoc(alignment);

	if (alignment & (alignment - 1))
		return (0);
	
	npgs = btoct(size);

	if (reservemem(GLOBAL_POOL, npgs, npgs, npgs)) {
		if (flags & VM_NOSLEEP)
			return(NULL);
		kreservemem(GLOBAL_POOL, npgs, npgs, npgs);
	}
		


	while ((pfd = 
	lpage_alloc_contig_physmem(node, size, alignment, flags)) == 0) {

		if (flags & VM_NOSLEEP) {
			unreservemem(GLOBAL_POOL, npgs, npgs, npgs);
			return NULL;
		}

		lpage_size_wait(size, PZERO);
	}

	/*
	 * Check to see if we can return a k0 address.
	 */

	if (small_K0_pfn(pfdattopfn(pfd))) {
		int flg;
		vaddr = small_pfntova_K0(pfdattopfn(pfd));
		flg = P_DUMP;
		if (flags & VM_BULKDATA)
			flg |= P_BULKDATA;

		for ( i = 0; i < npgs; i++, pfd++)
			pfd_setflags(pfd, flg);
		return vaddr;
	}
	else {

		/*
		 * Try to allocate a virtual address for the given physical addresses.
		 */

		if ((vaddr = (caddr_t)ctob(sptalloc(bm, npgs, flags, 0, alignment))) 
										== 0) {
			lpage_free_contig_physmem(pfd, size);
			unreservemem(GLOBAL_POOL, npgs, npgs, npgs);
			return (NULL);
		}

		/*
		 * Setup the page table entries.
		 */

		state = PG_VR | PG_G | PG_M | PG_SV |pte_cachebits();



		pd = kvtokptbl(vaddr);

		for (i = npgs; i--; pfd++, pd++) {
			uint	flg;
			pg_setpgi(pd, mkpde(state, pfdattopfn(pfd)));

			/* Set the selective crash dump P_DUMP flag here since
			 * this is called by kvpalloc().
			 */

			flg = P_DUMP;
			if (flags & VM_BULKDATA)
				flg |= P_BULKDATA;

			pfd_setflags(pfd, flg);
			COLOR_VALIDATION(pfd,colorof(((caddr_t)vaddr)+ctob(i)),
							 0,flags);
		}

		return (vaddr);
	}
}

/*
 * Free physical and virtual memory allocated using kmem_contig_alloc.
 */

void
kmem_contig_free(void *vaddr, size_t size)
{
	pgno_t	npgs = btoc(size); /* Round to the next page boundary */
	pde_t	*pd;
	pfd_t	*pfd, *start_pfd;
	int	i;

	if (size <= 0)
		return;

	unreservemem(GLOBAL_POOL, npgs, npgs, npgs);

	if (IS_KSEG2(vaddr)) {
		pd = kvtokptbl(vaddr);
		start_pfd = pdetopfdat(pd);
#ifdef MH_R10000_SPECULATION_WAR
		ASSERT(start_pfd >= pfd_lowmem);
#endif
		for ( i = 0; i < npgs; i++, pd++) {
			pfd = pdetopfdat(pd);
			pfd_clrflags(pfd, P_DUMP|P_BULKDATA);
#if R4000 || R10000
			ASSERT(!pg_isvalid(pd) || (pg_getpgi(pd) & PG_G));
			pg_setpgi(pd, mkpde(PG_G, 0));
#else
			pg_clrpgi(pd);
#endif
		}
		sptfree(&sptbitmap, npgs, btoc(vaddr));
	} else if (IS_KSEG0(vaddr) || IS_KSEG1(vaddr)) {
		start_pfd = pfd = kvatopfdat(vaddr);
		while (npgs--) {
			/* Since this is a kernel page, we must have set the
			 * DUMP flag.
			 */
			ASSERT(pfd->pf_flags & P_DUMP);
			/* Turn off DUMP flag */
			pfd_clrflags(pfd, P_DUMP|P_BULKDATA);
			ASSERT(!(pfd->pf_flags & (P_HASH|P_ANON|P_SWAP)));
			pfd++;
		}
	} else {
		cmn_err_tag(109,CE_PANIC, "kmem_contig_free: illegal addr -- 0x%x", vaddr);
	}
	
	lpage_free_contig_physmem(start_pfd, size);
}

#ifndef	NUMA_BASE
#undef	node
#endif


/*
 * Deallocate kernel virtual address space and physical pages
 */
void
kvpffree(register void *vaddr, register uint size, register int flags)
{
	register int k;
	register pfd_t	*pfd;
#ifdef MH_R10000_SPECULATION_WAR
	register int is_extk0 = (IS_R10000() &&
				(kvatopfdat(vaddr) < pfd_lowmem));
#endif

	if (size <= 0)
		return;

	if (IS_KSEG2(vaddr)
#ifdef MH_R10000_SPECULATION_WAR
			&& (!is_extk0)
#endif
	) {
		kvpfree_k2(vaddr, size, flags);

	} else if (IS_KSEG0(vaddr) || IS_KSEG1(vaddr)
#ifdef MH_R10000_SPECULATION_WAR
			|| is_extk0
#endif
	) {
		k = size;
		pfd = kvatopfdat(vaddr);
		while (k--) {
			/* Since this is a kernel page, we must have set the
			 * DUMP flag.
			 */
			ASSERT_ALWAYS(pfd->pf_flags & P_DUMP);
			pfd_clrflags(pfd, P_DUMP|P_BULKDATA);

			ASSERT(!(pfd->pf_flags & (P_HASH|P_ANON|P_SWAP)));
			pagefree(pfd);
			pfd++;
		}
		if (!(flags & VM_NOAVAIL))
			unreservemem(GLOBAL_POOL, size, size, size);
	} else {
		cmn_err_tag(110,CE_PANIC, "kvpfree: illegal addr -- 0x%x", vaddr);
	}
}

void
kvpfree(register void *vaddr, register uint size)
{
	kvpffree(vaddr, size, 0);
}

/*
 * Free up K2 allocated memory.  Locks sysreg only if memory was previously
 * made swappable.  It is assumed that no one ever frees swappable K2
 * addresses from interrupt handlers.
 */

static void
kvpfree_k2(void *addr, register uint size, register int flags)
{
	register pde_t 		*pd = kvtokptbl(addr);
	register pfd_t		*pfd;
	register pgno_t		sz;
	register pgno_t		rpn;
	register pgno_t		valid = 0;
	int			swappable;

	ASSERT(IS_KSEG2(addr));

	/*
	 * If pages were swappable, get sysreg lock now so we don't
	 * race with vhand.
	 */

	rpn = vtorpn(&syspreg, (caddr_t)addr);

	if (bftstclr(kvswap_map, rpn, size) != size) {
		reglock(&sysreg);
		swappable = 1;
	} else
		swappable = 0;

	for (sz = size; sz; sz--, pd++) {

		/*
		 * Non-valid pages have been given away,
		 * e.g., by stackbld().
		 */

		if (pg_isvalid(pd)) {
			valid++;
			pfd = pdetopfdat(pd);

#ifdef MH_R10000_SPECULATION_WAR
                        if (IS_R10000()) {
                                __psint_t vfn = kvirptetovfn(pd);

                                krpf_remove_reference(pdetopfdat(pd),
                                                      vfn);
                                pg_setpgi(pd, mkpde(PG_G, 0));
                                unmaptlb(0,vfn);
                        }
#endif /* MH_R10000_SPECULATION_WAR */

			/*
			 * No need for memlock since sysreg is
			 * locked and these pages are private
			 */

			/* P_DUMP must have been set for this
			 * page since it's a kernel page
			 */
			ASSERT(pfd->pf_flags & P_DUMP);
			pfd_clrflags(pfd, P_DUMP|P_BULKDATA);

			pagefree(pfd);
		}
#if R4000 || R10000
		ASSERT(!pg_isvalid(pd) || (pg_getpgi(pd) & PG_G));
		pg_setpgi(pd, mkpde(PG_G, 0));
#else
		pg_clrpgi(pd);
#endif
	}

	/*
	 * If pages were swappable, tell the anon mgr that we aren't using
	 * them anymore.
	 */

	if (swappable) {
		anon_remove(sysreg.r_anon, rpntoapn(&syspreg, rpn), size);
		bfclr(kvswap_map, rpn, size);
		syspreg.p_nvalid -= valid;
		regrele(&sysreg);

		if (!(flags & VM_NOAVAIL))
			unreservemem(GLOBAL_POOL, size, 0, size);

	} else 	if (!(flags & VM_NOAVAIL))
		unreservemem(GLOBAL_POOL, size, size, size);

	sptfree(&sptbitmap, size, btoct((caddr_t)addr));
}


/*
 * Deallocate kernel virtual address
 *
 * Return to pool of stale addresses.
 */
void
kvfree(register void *vaddr, register uint size)
{
	register int i;
	register pde_t *pd;

	pd = kvtokptbl(vaddr);
	for (i = size; --i >= 0; pd++) {
#if R4000 || R10000
		ASSERT(!pg_isvalid(pd) || (pg_getpgi(pd) & PG_G));
#ifdef MH_R10000_SPECULATION_WAR
                if (pg_isvalid(pd) &&
                    IS_R10000()) {
                        __psint_t vfn = kvirptetovfn(pd);

                        krpf_remove_reference(pdetopfdat(pd),
                                              vfn);
                        pg_setpgi(pd, mkpde(PG_G, 0));
                        unmaptlb(0,vfn);
                } else
#endif /* MH_R10000_SPECULATION_WAR */
		pg_setpgi(pd, mkpde(PG_G, 0));
#else
		pg_clrpgi(pd);
#endif
	}

	/*
	 * Virtual memory is returned to the stale map.
	 */
	sptfree(&sptbitmap, size, btoct(vaddr));
}

#ifdef KV_DEBUG
unsigned long  *kv_checksums;
unsigned int	kv_swapped;
#endif
int	kv_swap_enabled = 0;	/* suspected problems */

/*
 * Make a portion of kernel K2SEG-mapped memory pageable.
 */
void
kvswappable(void *addr, pgno_t size)
{
	register pde_t	*pd;
	register pgno_t	apn;
	register pgno_t	rpn;
	register int	new_swap = 0;
	register caddr_t vaddr = addr;
	register attr_t *attr;
	register caddr_t attrchange = NULL;
	pde_t pdesave;

	ASSERT(((__psint_t)addr & NBPP-1) == 0);
	if ((__psint_t)addr < K2SEG ||
	    (__psint_t)addr + ctob(size) > K2SEG + ctob(syssegsz))
		return;

	if (!kv_swap_enabled)
		return;

#ifdef KV_DEBUG
	{
	if (!kv_checksums)
		kv_checksums = (unsigned long *)
			kmem_zalloc(sizeof(long) * syssegsz, 0);
	}
#endif
	pd = kvtokptbl(addr);
	rpn = vtorpn(&syspreg, vaddr);
	reglock(&sysreg);

	attr = findattr(&syspreg, vaddr);
	pdesave.pgi = 0;
	pg_setccuc(&pdesave, attr->attr_cc, attr->attr_uc);

	while (size) {

		/*
		 * If page is not already marked swappable, then set it
		 * up as an anonymous page in sysreg so vhand can page it.
		 */

		if (!btst(kvswap_map, rpn)) {
			apn = rpntoapn(&syspreg, rpn);
			anon_insert(sysreg.r_anon, pfntopfdat(pd->pte.pg_pfn),
				apn, P_DONE);
			bset(kvswap_map, rpn);
			new_swap++;
#ifdef KV_DEBUG
			{
			register char *c = vaddr;
			register int i = ctob(1);
			register unsigned long *check = kv_checksums + apn;

			*check = 0;
			while (i-- > 0)
				*check += *c++;
			}
#endif
		}

		if (pg_getcc(pd) != pg_getcc(&pdesave)) {
			if (attrchange == 0) {
				attrchange = vaddr;
			} else {
				attr = findattr_range(&syspreg,
						attrchange, vaddr);
				attr->attr_cc = pg_getcc(pd);
				attrchange = NULL;
			}
                        pg_setccuc(&pdesave, pg_getcc(pd), pg_getuc(pd));
		}

		size--;
		pd++;
		rpn++;
		vaddr += ctob(1);
	}

	if (attrchange) {
		attr = findattr_range(&syspreg, attrchange, vaddr);
		attr->attr_cc = pg_getcc(&pdesave);
	}

	/*
	 * The p_nvalid field in syspreg counts the number of valid,
	 * swappable pages (not just valid pages).  This is a hint to
	 * vhand that there are pages to swap in sysreg.
	 */

	syspreg.p_nvalid += new_swap;
	regrele(&sysreg);
	unreservemem(GLOBAL_POOL, 0, new_swap, 0);
}

#ifdef KV_DEBUG
int kv_wasfound;
int kv_unlocked;
unsigned int kv_pfind;
unsigned int kv_swapin;
#endif
/*
 * Ensure that all of a portion of K2SEG-mapped memory is
 * swapped in.
 */
int
kvswapin(void *addr, register uint size, int flags)
{
	register char	*vaddr;
	register pfd_t	*pfd;
	register pde_t	*pd;
	register pgno_t	sz;
	register attr_t	*attr;
	register pgno_t	rpn;
	pgno_t		apn;
	pgno_t		nreserved = 0;
	void		*id;
	pglst_t		pglist;
	sm_swaphandle_t onswap;

	if ((__psint_t)addr < K2SEG ||
	    (__psint_t)addr + ctob(size) > K2SEG + ctob(syssegsz))
		return -1;

	if (!kv_swap_enabled)
		return 0;

#ifdef KV_DEBUG
	kv_unlocked = 0;
#endif

startover:
	vaddr = (char *) addr;
	pd = kvtokptbl(vaddr);
	rpn = vtorpn(&syspreg, vaddr);

	if (flags & VM_NOSLEEP) {
		if (creglock(&sysreg) == 0)
			return -1;
	} else
		reglock(&sysreg);

	for (sz = size; sz; sz--, pd++, rpn++, vaddr += ctob(1)) {

		/* Pages that _are_ kswap won't be committed !kswap
		 * until after _all_ the pages have been brought in.
		 * This way, its simple to bail out or restart if
		 * we must abort or sleep waiting for memory.
		 */
		if (btst(kvswap_map, rpn) == 0)
			continue;

		/* Allocate rights to unswappable memory
		 * before attempting to swap in the pages.
		 */
		if (reservemem(GLOBAL_POOL, 0, 1, 0)) {
			if (flags & VM_NOSLEEP) {
				regrele(&sysreg);
				unreservemem(GLOBAL_POOL, 0, nreserved, 0);
				return -1;
			}
			kreservemem(GLOBAL_POOL, 0, 1, 0);
		}
		nreserved++;

		if (pg_isvalid(pd)) {
			syspreg.p_nvalid--;
			continue;
		}

		ASSERT(!(pg_getpfn(pd)));
		if (pg_getpfn(pd))
			cmn_err_tag(111,CE_PANIC, "kvswapin pg_pfn");

		apn = rpntoapn(&syspreg, rpn);

		if (pfd = anon_pfind(sysreg.r_anon, apn, &id, &onswap)) {
                        pg_setpfn(pd, pfdattopfn(pfd));
#ifdef MH_R10000_SPECULATION_WAR
                        if (! pg_isnoncache(pd) &&
                            IS_R10000())
                                krpf_add_reference(pfd,
                                                   vatovfn(vaddr));
#endif /* MH_R10000_SPECULATION_WAR */
		} else {
			ASSERT(onswap);
			if (!onswap) {
				cmn_err_tag(112,CE_PANIC, "kvswapin: not on swap");
			}

			if ((pfd = pagealloc(btoct(vaddr), flags)) == 0) {
				regrele(&sysreg);
				unreservemem(GLOBAL_POOL, 0, nreserved, 0);
				nreserved = 0;

				if (flags & VM_NOSLEEP)
					return -1;

#ifdef MH_R10000_SPECULATION_WAR
				setsxbrk_class(flags);
#else /* MH_R10000_SPECULATION_WAR */
				setsxbrk();
#endif /* MH_R10000_SPECULATION_WAR */
#ifdef KV_DEBUG
				kv_unlocked++;
#endif
				/*
				 * Must start from the begining because
				 * vhand could have started swapping out
				 * the pages behind us while the sysreg
				 * was unlocked.
				 */
				goto startover;
			}

			pg_setpfn(pd, pfdattopfn(pfd));
#ifdef MH_R10000_SPECULATION_WAR
                        if (! pg_isnoncache(pd) &&
                            IS_R10000())
                                krpf_add_reference(pfd,
                                                   vatovfn(vaddr));
#endif /* MH_R10000_SPECULATION_WAR */
			pglist.gp_pfd = pfd;

			if (sm_swap(&pglist, onswap, 1, B_READ, NULL)) {
				unreservemem(GLOBAL_POOL, 0, nreserved, 0);
				pagefree(pfd);
				pg_setpfn(pd, 0);
#ifdef MH_R10000_SPECULATION_WAR
                                if (! pg_isnoncache(pd) &&
                                    IS_R10000()) {
                                        unmaptlb(0,vatovfn(vaddr));
                                        krpf_remove_reference(pfd,
                                                   vatovfn(vaddr));
                                }
#endif /* MH_R10000_SPECULATION_WAR */
				regrele(&sysreg);
				cmn_err_tag(113,CE_WARN, "kvswapin failure");
				return -1;
			}

			/*
			 * We have to go through this drill in case
			 * we go setsxbrk trying to bring in a subsequeut
			 * page -- otherwise, vhand will assert botch
			 * when it encounters a valid anon-backed page
			 * that isn't hashed.  The page will get unhashed
			 * by anon_remove, below.
			 */
			pinsert(pfntopfdat(pd->pte.pg_pfn),
				anon_tag(sysreg.r_anon),
				apn, P_ANON|P_DUMP|P_DONE);
#ifdef KV_DEBUG
			kv_wasfound = 0;
			kv_swapin++;
#endif
		}

#if R4000 || R10000
	    	_bclean_caches((void *)ctob(btoct(vaddr)), NBPP,
			  pfdattopfn(pfd),
			  CACH_ICACHE|CACH_DCACHE|CACH_WBACK|
			  CACH_INVAL|CACH_ICACHE_COHERENCY|CACH_VADDR);

		if (pfd->pf_flags & P_CACHESTALE)
			pageflags(pfd, P_CACHESTALE, 0);
#endif
		attr = findattr(&syspreg, vaddr);
		if (attr->attr_prot & PROT_W)
			pg_setmod(pd);
		pg_setccuc(pd, attr->attr_cc, attr->attr_uc);
		pg_setvalid(pd);
		pg_setglob(pd);

#ifdef KV_DEBUG
		{
		register char *c = vaddr;
		register int i = ctob(1);
		register unsigned long check = 0;

		kv_swapped++;

		while (i-- > 0)
			check += *c++;

		if (check != *(kv_checksums + apn))
			cmn_err(CE_PANIC, "checksum error for pfd (%s) %x",
				pfd, kv_wasfound ? "pfound" : "swap-in");
		}
#endif
	}

	anon_remove(sysreg.r_anon, vtoapn(&syspreg, (char *) addr), size);
	bfclr(kvswap_map, vtorpn(&syspreg, (char *) addr), size);
	regrele(&sysreg);

	return 0;
}

extern sv_t runout;
extern unsigned int sxbrkcnt;
/*
 * setsxbrk - set the current thread to the SXBRK state. 
 */
void
setsxbrk(void)
{
#ifdef MH_R10000_SPECULATION_WAR
	setsxbrk_class(0);
#else /* MH_R10000_SPECULATION_WAR */
	int s;
	kthread_t *kt = curthreadp;
	uthread_t *ut;

	cvsema(&vhandsema);

	if (KT_ISUTHREAD(kt)) {
		ut = KT_TO_UT(kt);
		ut_flagset(ut, UT_SXBRK);
	}

	/*
	 * Gonna set timer, must be at splprof -- this should be
	 * pushed into sv_wait code?
	 */
	s = sxbrk_lockspl(splprof);
	++sxbrkcnt;

	if (KT_ISUTHREAD(kt)) {
		/*
		 * don't permit signalling - almost all callers
		 * simply loop and continue to call setsxbrk
		 */
		sv_wait(&sv_sxbrk, TIMER_SHIFT(AS_MEM_WAIT), &sxbrk_lock,s);
		ut_flagclr(ut, UT_SXBRK);
	} else
		sv_wait(&sv_sxbrk, TIMER_SHIFT(AS_MEM_WAIT), &sxbrk_lock, s);

#endif /* MH_R10000_SPECULATION_WAR */
}

#if EXTKSTKSIZE == 1
/*
 * Special setsxbrk for stack extention - this is called from the
 * the middle of context switching...
 */
void
stackext_setsxbrk(void)
{
	cvsema(&vhandsema);
}
#endif /* EXTKSTKSIZE */

#ifdef MH_R10000_SPECULATION_WAR 
unsigned int sxbrkcnt_any; /* sxbrkcnt for any memory */
unsigned int sxbrkcnt_high; /* sxbrkcnt for high memory */
unsigned int sxbrkcnt_low;  /* sxbrkcnt for low memory */

void
setsxbrk_class(int flags)
{
	int s;
	kthread_t *kt = curthreadp;
	uthread_t *ut;
	sv_t *sv_sxbrk_p = &sv_sxbrk;
 	unsigned int *sxbrkcnt_p = NULL;

	if (IS_R10000()) {
		if (flags & VM_UNCACHED) 
			flags = (flags & ~VM_NO_DMA) | VM_DMA_RW;
		else if (flags & VM_DIRECT) {
			if (flags & VM_DMA_RW)
				flags &= ~VM_DMA_RW;
			flags |= VM_NO_DMA;
		}
		if (!(flags & (VM_DMA_RW | VM_NO_DMA)))
			flags |= VM_DMA_RW;

		sxbrkcnt_p = &sxbrkcnt_any;
		if (flags & VM_DMA_RW) {
			sv_sxbrk_p = &sv_sxbrk_high;
			sxbrkcnt_p = &sxbrkcnt_high;
		} else if ((flags & VM_DIRECT) &&
			   ! (flags & VM_UNCACHED)) {
			sv_sxbrk_p = &sv_sxbrk_low;
			sxbrkcnt_p = &sxbrkcnt_low;
		}
	}

	cvsema(&vhandsema);

	if (KT_ISUTHREAD(kt)) {
		ut = KT_TO_UT(kt);
		ut_flagset(ut, UT_SXBRK);
	}

	/*
	 * Gonna set timer, must be at splprof -- this should be
	 * pushed into sv_wait code?
	 */
	s = sxbrk_lockspl(splprof);
	++sxbrkcnt;
	if (sxbrkcnt_p != NULL)
		++(*sxbrkcnt_p);

	if (KT_ISUTHREAD(kt)) {
		/*
		 * don't permit signalling - almost all callers
		 * simply loop and continue to call setsxbrk
		 */
		sv_wait(sv_sxbrk_p, TIMER_SHIFT(AS_MEM_WAIT), &sxbrk_lock,s);
		ut_flagclr(ut, UT_SXBRK);
	} else
		sv_wait(sv_sxbrk_p, TIMER_SHIFT(AS_MEM_WAIT), &sxbrk_lock, s);
}
#endif /* MH_R10000_SPECULATION_WAR */

#if defined(DEBUG) || defined(PGLISTDEBUG)

#include <sys/idbgentry.h>


#ifdef PH_LONG
char *li[PH_NLISTS] = {
	"CL_A",
	"IN_A",
	"ST_A",
	"CL_N",
	"IN_N",
	"ST_N"
};
#else
char *li[PH_NLISTS] = {
	"CL_A",
	"CL_N",
        "POIS",
};
#endif




/*
 * x == -2 print smaller summary
 * x == -1 print summary
 * x == 1 print all lists
 * x == 2 print all lists AND tag for each page
 */
void
pgfreeprint(int x)
{
	register phead_t	*pheadp;
	pfd_t *pfd;
	cnodeid_t node;
	int j, k, i, lcnt, cnt, tcnt;

#ifdef PGLISTDEBUG
	if (x == 3) {
		qprintf("rpc:\n");
		for (i = 0; i < 16; i++) {
			for(j = 0; j < 16; j++)
				qprintf("%d ", rpcache[i*16+j]);
			qprintf("\n");
		}
		qprintf("vpc:\n");
		for (i = 0; i < 16; i++) {
			for(j = 0; j < 16; j++)
				qprintf("%d ", vpcache[i*16+j]);
			qprintf("\n");
		}
		qprintf("s5go:\n");
		for (i = 0; i < 16; i++) {
			for(j = 0; j < 16; j++)
				qprintf("%d ", s5go[i*16+j]);
			qprintf("\n");
		}
		qprintf("al:\n");
		for (i = 0; i < 16; i++) {
			for(j = 0; j < 16; j++)
				qprintf("%d ", al[i*16+j]);
			qprintf("\n");
		}
		qprintf("un:\n");
		for (i = 0; i < 16; i++) {
			for(j = 0; j < 16; j++)
				qprintf("%d ", un[i*16+j]);
			qprintf("\n");
		}
		return;
	}
#endif

#ifdef TILES_TO_LPAGES
	qprintf("  %8s  %11s  %11s  %11s  %11s\n", "tphead", "phead", "end",
		"rotor", "count");
	for (i=0; i < NTPHEADS; i++) {
		qprintf("  0x%6x   0x%x   0x%x   0x%x     %d\n", &tphead[i],
			tphead[i].phead, tphead[i].pheadend,
			tphead[i].pheadrotor, tphead[i].count);
		tcnt = 0;
		for (pheadp = tphead[i].phead;
		     pheadp < tphead[i].pheadend; pheadp++)
			tcnt += pheadp->ph_count;
	}
#endif /* TILES_TO_LPAGES */

	tcnt = 0;
	for (node = 0; node < numnodes; node++) {
		pg_free_t	*pfl = GET_NODE_PFL(node);

		qprintf("node %d freemem %d future_freemem %d emptymem %d totalmem %d\n",
			node, NODE_FREEMEM(node), NODE_FUTURE_FREEMEM(node), 
			NODE_EMPTYMEM(node), NODE_TOTALMEM(node));
			
		for (k = 0; k < NUM_PAGE_SIZES; k++) {
			qprintf("node %d pgsz %x: phead 0x%x pheadend 0x%x\n",
				node, PGSZ_INDEX_TO_SIZE(k),
				pfl->phead[k], pfl->pheadend[k]);
			qprintf("\t\tpheadmask 0x%x pheadrotor 0x%x icleans %d\n", 
				PHEADMASK(k), PHEADROTOR(node,k), MINFO.iclean);
			qprintf("pfd low %x pfd high %x\n",pfl->pfd_low,
						pfl->pfd_high);
			if (x == -2) 
				continue;
				
	    		for (j = 0, pheadp = &pfl->phead[k][0]; 
				pheadp < pfl->pheadend[k]; j++, pheadp++) {

		    		qprintf("[0x%x] @0x%x cnt %d ",
				    pheadp - pfl->phead[k], pheadp, 
						pheadp->ph_count);
				tcnt += pheadp->ph_count;
				if (x == -1) {
				    if ((j % 2) == 1)
					    qprintf("\n");
				    continue;
				}
				for (i = 0, cnt = 0; i < PH_NLISTS; i++) {
				    for (lcnt = 0, 
					pfd = pheadp->ph_list[i].pf_next; 
					pfd != (pfd_t *)(&pheadp->ph_list[i]);
						    pfd = pfd->pf_next) {
					    lcnt++;
					    cnt++;
						if ( k == MIN_PGSZ_INDEX) 
					    if (pfd->pf_use != 0 || 
						!(pfd->pf_flags & P_QUEUE))
						    qprintf("\nPage 0x%x[%d] \
						has use > 0 or not P_QUEUE\n",
							  pfd, pfdattopfn(pfd));
					    if (x == 2)
						    qprintf("[0x%x]", 
								pfd->pf_tag);
				    }
				    qprintf("%s %d  ", li[i], lcnt);
				}
				qprintf("\n");
	    		}
		}
		qprintf("\n");
	}

        qprintf("Accurate global freemem %d Relaxed global freemem %d, onlists %d\n",
                GLOBAL_FREEMEM_GET(), GLOBAL_FREEMEM(), tcnt);
}
#endif

/*
 * Initialize memory related locks and global data structures
 *
 * returns:
 *	none
 */
void
meminit(void)
{
	register int		i;
	/* REFERENCED */
	pg_free_t		*pfl;
	pgno_t	availsmem, 	availrmem;

	/*
	 * We call tune_sanity to complete setup the tuning variables based
	 * on the value of maxmem.
	 */
	tune_sanity(&tune);

	/*
	 * Complete initialization of memory constants and semaphores.
	 */
        GLOBAL_FREEMEM_INIT();
	availrmem = GLOBAL_FREEMEM() - tune.t_minarmem - (GLOBAL_FREEMEM() >> 10);
	availsmem = GLOBAL_FREEMEM() - tune.t_minasmem - (GLOBAL_FREEMEM() >> 10);
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000()) {
		availrmem -= smallk0_freemem;
		availrmem -= min((availrmem >> 4),256); /* avoid deadlock */
	}
#endif /* MH_R10000_SPECULATION_WAR */
	init_global_pool(availsmem, availrmem, maxmem);
	spinlock_init(&memory_lock, "memlock");
	spinlock_init(&sxbrk_lock, "sxbrklock");

#ifdef	NUMA_BASE
	/*
	 * Initialize per-node nodepdaindr array
	 */
	for (i = 0; i < numnodes; i++) {
		ASSERT(NODEPDA_GLOBAL(i)->pernode_pdaindr);
		bcopy(Nodepdaindr, NODEPDA_GLOBAL(i)->pernode_pdaindr, 
					(sizeof(void *) * numnodes));
	}
#endif 	/* NUMA_BASE */

	/*
	 * Only after this point, NODEPDA(x) macro can be used to 
	 * access the individual nodepda.
	 */

	/*
	 * Initialize the per-node free page list lock.
	 */
	for (i = 0; i < numnodes; i++)
		init_spinlock(&NODE_PG_DATA(i).pg_freelst_lock,
					"freelst", i);

	/*
	 * Initialize page wait semaphores.
	 */
	for (i = 0; i <= PWAITMASK; i++)
		sv_init(&pfdwtsv[i], SV_DEFAULT, "pwt");

	/*
	 * Use the pg_freelst of node 0 which should be
	 * the current node. It will be copied to other nodes soon.
	 */

        pfl = (pg_free_t *)&(NODEPDA(0)->node_pg_data.pg_freelst[numnodes-1]);

#if _MIPS_SIM != _ABI64	/* 64 bit kernels don't need this */
	if (physmem > (0x10000 >> PGSHFTFCTR)) /* XXX should have define */
#if IP22
	{
		extern pfn_t low_maxclick;
		pfd_direct = pfntopfdat(low_maxclick);
	}
#else
		pfd_direct = pfntopfdat(0x10000 >> PGSHFTFCTR);
#endif  /* IP22 */
	else
		pfd_direct = pfl->pfd_high;
#endif /* _MIPS_SIM != _ABI64 */

	/*
	 * Initialize large page data structures.
	 */

	lpage_init();

	/*
	 * Copy the readonly page free list info. to all nodes from master node
	 */


#if !defined(NUMA_BASE)
	/* 
	 * Make sure that numnodes == 1, to gurantee the next for
	 * loop doesnot execute for NON-NUMA_BASE systems.
	 */
	ASSERT(numnodes == 1);
#endif	/* NUMA_BASE */
	for ( i = 1; i < numnodes; i++) {
		bcopy(NODEPDA(0)->node_pg_data.pg_freelst, 
				NODEPDA(i)->node_pg_data.pg_freelst,
				sizeof(pg_free_t)*numnodes);
	}

	/*
	 * Initialize locks.
	 */
	sv_init(&sv_sxbrk, SV_DEFAULT, "sxbrk");
#ifdef MH_R10000_SPECULATION_WAR
	sv_init(&sv_sxbrk_low, SV_DEFAULT, "sxbrklow");
	sv_init(&sv_sxbrk_high, SV_DEFAULT, "sxbrkhigh");
#endif /* MH_R10000_SPECULATION_WAR */
#if defined(DEBUG) || defined(PGLISTDEBUG)
	idbg_addfunc("pgfree", pgfreeprint);
#endif


}


/*
 * Initialize a node's memory.  The lower portion of a node's memory
 * contains some or all of the kernel text and some boot
 * time allocated static data structures, such as the phead structures.
 * Initialize these now and put all the pages from first
 * to last on the node's free list.
 *
 * Parameters:
 *		node  - the compact id of the node whose memory we are
 *			initializing
 *		first - physical pfn of first free page (immediately after
 *			pfdat array)
 *		last  - physical pfn of last free page
 */

/*ARGSUSED*/
void
node_meminit(cnodeid_t node, pfn_t first, pfn_t last)
{
	phead_t			*pheadp;
	int			i;
	pgszindx_t		j;
#if DISCONTIG_PHYSMEM
	int 			slot;
	int			last_slot;
	int			slot_psize;
	pfn_t			slot_firstpfn;
	int			node_pfdat_size;
#endif
	pfn_t			slot_lastpfn;
	pg_free_t		*pfl;
	nodepda_t		*node_pdap;

	node_pdap = NODEPDA_GLOBAL(node);

#if DISCONTIG_PHYSMEM
        /*
         * All low memory has to fit below the pfdat table in the first slot.
         */
	if (pfn_to_node_offset_pfn(first) > btoc(NODE_PFDAT_OFFSET))
		cmn_err_tag(114,CE_PANIC, "Static data on node %d doesn't fit below pfdat table (first=%d)\n", node, first);

	/*
	 * Need to have space for pfdat table
	 */
	last_slot = node_getlastslot(node);
	node_pfdat_size = btoc(SLOT_SIZE*(last_slot+1)) * sizeof(pfd_t);
	 if (pfn_to_node_offset_pfn(last) <
				btoc(NODE_PFDAT_OFFSET + node_pfdat_size))
		cmn_err_tag(115,CE_PANIC, "Insufficient space on node %d for pfdat array (first=%d last=%d)\n", node, first, last);

#else	/* !DISCONTIG_PHYSMEM */

        /*
         * All low memory has to fit in the first slot.
         */
	ASSERT(node == 0);
	slot_lastpfn = node_getmaxclick(0);
	if (first > slot_lastpfn)
		cmn_err_tag(116,CE_PANIC, "Not enough room in memory slot 0 on node %d for static static data (%d > %d).",
			node, first, slot_lastpfn);

#endif	/* !DISCONTIG_PHYSMEM */

	/*
	 * Initialize the pg_freelst of master node which should be
	 * the current node. It will be copied to other nodes.
	 * after the initializations are complete in meminit()
	 */

        pfl = (pg_free_t *)&(NODEPDA_GLOBAL(0)->node_pg_data.pg_freelst[(node)]);

	/*
	 * Setup a queue where all the pages with permanent errors can
	 * be put into. Initialize some statistics as to how many pages
	 * had errors, per node.
	 */

	makeempty(&node_pdap->error_discard_plist);
	node_pdap->error_discard_count = 0;
	node_pdap->error_cleaned_count = 0;
	node_pdap->error_page_count = 0;
	spinlock_init(&node_pdap->error_discard_lock, "error_discard_lock");


	/*
	 * Setup queue of free page headers for this node.
	 *
	 * The free pages are initially broken into buckets based on the
	 * largest of the physical caches (icache, dcache, second-level
	 * cache on mp systems).
	 *
	 * They are further subdivided into four lists based on their
	 * I-cached state (clean, or possibly having stale data in a
	 * processor's instruction cache) and their disk/TLB association
	 * (forgotten pages used first, or saved pages to be used last).
	 */

	for ( j = 0; j < NUM_PAGE_SIZES; j++) {

#ifdef	DEBUG
		if (j == MIN_PGSZ_INDEX) 
			ASSERT(PHEADMASK(j));
#endif
		for (i = node_pdap->node_pg_data.pheadmask[j]; i; i >>= 1)
			node_pdap->node_pg_data.pheadshift[j]++;

#ifdef TILES_TO_LPAGES
		if (j == MIN_PGSZ_INDEX)
			pfl->pheadend[j] = &pfl->phead[j][NTPHEADS *
				(node_pdap->node_pg_data.pheadmask[j]+1)];
		else
#endif /* TILES_TO_LPAGES */
		pfl->pheadend[j] = &pfl->phead[j]
				[node_pdap->node_pg_data.pheadmask[j]+1];
		for (pheadp = &pfl->phead[j][0]; 
				pheadp < pfl->pheadend[j]; pheadp++) {
			for (i = 0; i < PH_NLISTS; i++) {
				makeempty(&pheadp->ph_list[i]);
			}
			ASSERT(pheadp->ph_count == 0);
		}
	}

#ifdef TILES_TO_LPAGES
	/* setup phead info for our 3 separate 4K page pools */
	pheadp = &pfl->phead[MIN_PGSZ_INDEX][0];

	for (i = 0; i < NTPHEADS; i++) {
		tphead[i].phead = &pheadp[i *
			(node_pdap->node_pg_data.pheadmask[MIN_PGSZ_INDEX]+1)];
		tphead[i].pheadend = &pheadp[(i+1) *
			(node_pdap->node_pg_data.pheadmask[MIN_PGSZ_INDEX]+1)];
		tphead[i].pheadrotor = tphead[i].phead;
	}
#endif /* TILES_TO_LPAGES */

	if (node == 0)
		kpbase = ctob(first);

#if DISCONTIG_PHYSMEM

	/*
	 * Memory within a node may be physically discontiguous.  If so,
	 * there will be some number of slots where memory is installed.
	 * Go through and initialize each slot of memory.
	 */

        /*
         * The first slot (slot #0) needs special treatment
         * because it is used for low_mem allocation.
         */
	slot_firstpfn = slot_getbasepfn(node,0);
	slot_lastpfn = slot_firstpfn + slot_getsize(node,0) - 1;
	ASSERT(first >= slot_firstpfn);
        ASSERT(first <= slot_lastpfn);
	if (slot_lastpfn > last) {
#ifdef DEBUG
		cmn_err(CE_WARN, "Truncating length of mem slot 0 (%d to %d)\n",
			slot_lastpfn, last);
#endif
		slot_lastpfn = last;
	}
	slot0_mem_init(node, first, slot_lastpfn);

	/*
	 * Initialize remaining slots
	 */
	for (slot = 1; slot <= last_slot; slot++) {
		slot_psize = slot_getsize(node,slot);
		if (slot_psize) {
			slot_firstpfn = slot_getbasepfn(node,slot);
			slot_lastpfn = slot_firstpfn + slot_psize - 1;
			mem_range_init(node, slot_firstpfn, slot_lastpfn);
		}
	}
#else	/* !DISCONTIG_PHYSMEM */
	mem_range_init(node, first, last);
#endif	/* !DISCONTIG_PHYSMEM */
	if (pfd_direct < pfl->pfd_high)
		pfd_direct = pfl->pfd_high;
}

#if DISCONTIG_PHYSMEM
static void
slot0_mem_init(cnodeid_t node, pfn_t first, pfn_t last)
{
	int slot;
	int last_slot;
	int slot_psize;
	int node_pfdat_size;
	long bzero_size;
	pfn_t init_first;
	pfn_t init_last;
	pfd_t *basepfd;
	pfd_t *freepfd_first;
	pfd_t *freepfd_end;

	ASSERT(MAX_MEM_SLOTS*SLOT_SIZE == NODE_MAX_MEM_SIZE);
	/*
	 * The pfdats for a node are allocated from slot 0.
	 */

	/*
	 * For each slot, zero the pfdats it will use.
	 */
	last_slot = node_getlastslot(node);
	for (slot=0; slot<=last_slot; slot++) {
		slot_psize = slot_getsize(node,slot);
		if (slot_psize == 0)
			continue;

		basepfd = pfntopfdat(slot_getbasepfn(node,slot));
		ASSERT(((long)basepfd & (NBPC-1)) == 0);

		bzero_size = slot_psize*sizeof(pfd_t);
		ASSERT((bzero_size & (NBPC-1)) == 0);
		bzero(basepfd, bzero_size);
		lowmemdata_record("pfdats", node, bzero_size, (void*)basepfd);
	}

	/*
	 * Init mem up to the first pfdats
	 */
	init_last = kvatopfn(pfn_to_node_pfdat_base(first)) - 1;
#if 0
	if (init_last < last)
		init_last = last;
#endif
	ASSERT (first <= init_last);
	mem_range_init(node, first, init_last);

	/*
	 * Init the parts of the pfdat tables that are not being used.
	 */
	for (slot=0; slot<=last_slot; slot++) {
		slot_psize = slot_getsize(node,slot);

		basepfd = pfntopfdat(slot_getbasepfn(node,slot));
		ASSERT(((long)basepfd & (NBPC-1)) == 0);

		/* free the rest */
		freepfd_first = basepfd + slot_psize;
		ASSERT(((long)freepfd_first & (NBPC-1)) == 0);
		init_first = kvatopfn(freepfd_first);

		freepfd_end = basepfd + btoc(SLOT_SIZE);
		ASSERT(((long)freepfd_end & (NBPC-1)) == 0);
		init_last = kvatopfn(freepfd_end) - 1;
		ASSERT_ALWAYS(init_last <= last);

		/* if not a full slot, free up the memory */
		if (slot_psize < btoc(SLOT_SIZE))
			mem_range_init(node, init_first, init_last);
	}

	/*
	 * Now init the rest of the memory in slot0.
	 */
	init_first = init_last + 1;	/* pick up where we left off */

	node_pfdat_size = btoc(SLOT_SIZE*(last_slot+1)) * sizeof(pfd_t);
	ASSERT_ALWAYS(init_first ==
	       btoct(NODE_OFFSET(COMPACT_TO_NASID_NODEID(node)) +
		     NODE_PFDAT_OFFSET + node_pfdat_size));

	slot_psize = slot_getsize(node,0);
	init_last = slot_getbasepfn(node,0) + slot_psize - 1;
	if (init_last > init_first) {
		if (init_last < last) {
			ASSERT(last >= init_first);
			init_last = last;
		}

		mem_range_init(node, init_first, init_last);
	}
}
#endif	/* DISCONTIG_PHYSMEM */


/*
 * Initialize a range of memory.  A memory range is a block of physically
 * contiguous memory that is dealt with as a unit.  After allowing for
 * bad pages to be taken out of service, the rest are added to the node's
 * free list.
 */
static void
mem_range_init(cnodeid_t node, pfn_t first, pfn_t last)
{
	register pfd_t		*pfd;
	register phead_t	*pheadp;
	register plist_t	*plistp;
	register int		i;
	pgszindx_t		indx;
	pfn_t			badpages;
	pfn_t			freepages;
	int			debug_bigmem;
	pg_free_t		*pfl;
#ifdef TILES_TO_LPAGES
	tphead_t *tpheadp;
#endif /* TILES_TO_LPAGES */

	lowmemdata_record("mem_range_init", node, ctob(last-first+1),
			(void*)ctob(first));

	debug_bigmem = is_specified(arg_debug_bigmem);

	/*
	 * Allow machine dependent routine to take pages permanently
	 * out of service (i.e., for holes in memory).
	 */

	badpages = setupbadmem(pfntopfdat(first), first, last);

	/*
	 * Setup basic memory constants.
	 */

	freepages = (last - first + 1) - badpages;
#ifdef MH_R10000_SPECULATION_WAR
        if (first < pfn_lowmem) {
		extern pgno_t extk0_pages_added(void);

		smallk0_freemem = ((SMALLMEM_K0_R10000_PFNMAX + 1) - first) +
					extk0_pages_added();
        }
	ASSERT_ALWAYS(!IS_R10000() || smallk0_freemem);
#endif /* MH_R10000_SPECULATION_WAR */

	ADD_NODE_FREEMEM(node, freepages);
	ADD_NODE_FREE_PAGES(node,MIN_PGSZ_INDEX,freepages);
	ADD_NODE_TOTAL_PAGES(node,MIN_PGSZ_INDEX,freepages);
        ADD_NODE_EMPTYMEM(node, freepages);
	maxmem += freepages;

	pfd = pfntopfdat(first);

	/*
	 * Initialize the pg_freelst of node 0 which should be
	 * the current node.  It will be copied to other nodes
	 * after the initializations are complete in meminit().
	 */

        pfl = (pg_free_t *)&(NODEPDA_GLOBAL(0)->node_pg_data.pg_freelst[(node)]);

	SET_PAGE_SIZE_INDEX(pfd,MIN_PGSZ_INDEX);

	/*
	 * If this is the first time through for this node, set the low pfd
	 * and the free page rotor to point to page header of the first free
	 * page.
	 */

	if (pfl->pfd_low == NULL) {
		pfl->pfd_low = pfd;
		for ( indx = 0; indx < NUM_PAGE_SIZES; indx++ )
			SET_PHEADROTOR(node, indx, &(pfl->phead[indx]
				[pfdattopfn((pfd_t *)pfd) & PHEADMASK(indx)]));
	}
	ASSERT(pfl->pfd_low <= pfd);

#ifdef MH_R10000_SPECULATION_WAR
	/*
	 * On IP32, we add any lowmem pfdats to a separate set of pheads.
	 */
	if (first < pfn_lowmem) {
		tpheadp = &tphead[TPH_LOWMEM];
		tpheadp->pheadrotor =
			&tpheadp->phead[pfdattopfn(pfd) &
				       PHEADMASK(MIN_PGSZ_INDEX)];
		pheadp = tpheadp->pheadrotor;
		for (i = pfn_lowmem - first - 1; i >= 0; i--, pfd++) {
			if (PG_ISHOLE(pfd)) {
				if (++pheadp == tpheadp->pheadend)
					pheadp = tpheadp->phead;
				continue;
			}
			plistp = &pheadp->ph_list[CLEAN_NOASSOC];
			if (!debug_bigmem) {
				append(plistp, pfd);
			} else {
				prefix(plistp, pfd);
			}
			pfd->pf_flags = P_QUEUE;
			pheadp->ph_count++;
			tpheadp->count++;

			/*
			 * Set the free bit in the bitmap
			 */
			set_pfd_bit(pfdat_to_tainted_bm(pfd),pfd);
			set_pfd_bit(pfdat_to_pure_bm(pfd),pfd);
			if (++pheadp == tpheadp->pheadend)
				pheadp = tpheadp->phead;
		}
		first = pfn_lowmem;
	}
#endif /* MH_R10000_SPECULATION_WAR */

	/*
	 * NOTE: Can't use PFD_TO_PHEAD macros here because pheadss haven't
	 * been copied to all nodes yet.
	 */

#ifdef TILES_TO_LPAGES
	tpheadp = &tphead[TPH_UNFRAG];
	tpheadp->pheadrotor = &tpheadp->phead[pfdattopfn(pfd) &
					     PHEADMASK(MIN_PGSZ_INDEX)];
	pheadp = tpheadp->pheadrotor;
#else /* TILE_DATA */
	pheadp = &pfl->phead[MIN_PGSZ_INDEX][pfdattopfn(pfd) & 
					PHEADMASK(MIN_PGSZ_INDEX)];
#endif /* TILE_DATA */

	/*
	 * Add pages to free queues.
	 *
	 * At boot time, all pages are clean with respect to the
	 * instruction cache, and have no disk or TLB assocation.
	 */

	for (i = last - first; i >= 0; i--, pfd++) {
		if (PG_ISHOLE(pfd)) {
#ifdef TILES_TO_LPAGES
			if (++pheadp == tpheadp->pheadend)
				pheadp = tpheadp->phead;
#else
			if (++pheadp == pfl->pheadend[MIN_PGSZ_INDEX])
				pheadp = &pfl->phead[MIN_PGSZ_INDEX][0];
#endif /* TILES_TO_LPAGES */
			continue;
		}

		plistp = &pheadp->ph_list[CLEAN_NOASSOC];
		if (!debug_bigmem) {
			append(plistp, pfd);
		} else {
			prefix(plistp, pfd);
		}

#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			pfde_set_vcolor(pfd, -1);
		}
#endif /* _VCE_AVOIDANCE */

		pfd->pf_flags = P_QUEUE;
		pheadp->ph_count++;
#ifdef TILES_TO_LPAGES
		tpheadp->count++;
#endif /* TILES_TO_LPAGES */		

		/*
		 * Set the free bit in the bitmap
		 */
		set_pfd_bit(pfdat_to_tainted_bm(pfd),pfd);
		set_pfd_bit(pfdat_to_pure_bm(pfd),pfd);
#ifdef TILES_TO_LPAGES
		if (++pheadp == tpheadp->pheadend)
			pheadp = tpheadp->phead;
#else
		if (++pheadp == pfl->pheadend[MIN_PGSZ_INDEX])
			pheadp = &pfl->phead[MIN_PGSZ_INDEX][0];
#endif /* TILES_TO_LPAGES */
	}
	ASSERT(pfntopfdat(last) == pfd - 1);

	ASSERT(pfl->pfd_high <= pfd - 1);
	pfl->pfd_high = pfd - 1;

#ifdef TILES_TO_LPAGES
	(void) tile_init(pfdattopfn(pfl->pfd_low), pfdattopfn(pfl->pfd_high),
			 badpages);
#endif /* TILES_TO_LPAGES */
}


#if DISCONTIG_PHYSMEM
/*
 * Check a physical pfn for invalidity.  This code assumes that memory in
 * each node starts at offset 0 within the node.
 */

ckphyspnum(pfn_t pfn)
{
	cnodeid_t	node;
	nasid_t		nasid;
	int		slot, numslots;
	pfn_t		start;

	if ((nasid = pfn_nasid(pfn)) < 0 || nasid >= MAX_NASIDS ||
	    (node = NASID_TO_COMPACT_NODEID(nasid)) == INVALID_CNODEID)
		return 1;	/* illegal pfn */

	numslots = node_getnumslots(node);

	for (slot = 0; slot < numslots; slot++) {
		start = slot_getbasepfn(node, slot);

		if (pfn < start)
			break;	/* illegal pfn */

		if (pfn < start + slot_getsize(node, slot))
			return 0;	/* legal pfn */
	}

	return 1;		/* illegal pfn */
}
#endif /* DISCONTIG_PHYSMEM */

/*
 * Structure used for table driven list searches.
 */
struct searchtab {
	char	list;		/* list to test */
	char	flags;		/* state flags */
};
#define STAB_FLUSH	0x1	/* list holds stale pages */
#define STAB_INTRANSIT	0x2	/* list holds pages becoming clean */
#define STAB_POISONOUS  0x4     /* list holds poisonous pages */

/*
 * Selector function used to choose a neighbor to borrow memory from
 * This function tests the available memory on a node to see if
 * the available memory on the node exceeds a specified threshold 
 * AND pages of the requested size are available.
 *
 *	Input Params:
 *		arg1 - freemem_threshold
 *		arg2 - pagesize
 *
 *	 Return Value:
 *		1 - node freemem exceeds threshold AND has pages of the requested size
 *		0 - node does not have suffient memory.
 */

#if NUMA_BASE
static int
page_borrow_selector(cnodeid_t candidate, void* arg1, void* arg2)
{
        pfn_t 		freemem_threshold = (pfn_t)(long)arg1;
	size_t		size = (size_t)arg2;
	pgszindx_t	page_size_index;

        ASSERT(candidate >= 0  && candidate < numnodes);

        /*
         * We really don't need to lock the access to NODE_FREEMEM
         * because it is being use as a simple directive.
         */

        if (NODE_FREEMEM(candidate) >= freemem_threshold) {
		page_size_index = PAGE_SIZE_TO_INDEX(size);
		while (page_size_index < NUM_PAGE_SIZES) {
			if (GET_NUM_FREE_PAGES(candidate, page_size_index) > 0)
				return 1;
			page_size_index++;
		}
	}

        return (0);
}
#endif


/*
 *	Allocate a page of memory of a specified size. The caller does not 
 * 	specify a location with this interface, so we will choose the node
 *	to allocate the memory from ourselves.
 */

pfd_t *
pagealloc_size(
	register uint cachekey,
	register int flags,
	register size_t	size)
{
#if NUMA_BASE
	pfd_t	  *pfd;
        cnodeid_t neighbor;
	int	  freemem_threshold;

	/*
	 * Since the caller isn't specifying which node to allocate from,
	 * we'll try the current node first. 
	 */

	if ((pfd = pagealloc_size_node(private.p_nodeid, cachekey, flags, size)) != NULL) {
		return pfd;
        }

        /*
         * If no memory is available on the current node, 
         * we do a radial search up to the max possible radius. We go thru the loop
	 * twice; once specifying that memory should be allocated from nodes that
	 * have available memory above a threshold of 0x400 pages (arbitrary number).
	 * If no nodes have memory above this threshold, we try again with
	 * a threshold of 0.
         */

	for (freemem_threshold = 0x400;
	     freemem_threshold >= 0;
	     freemem_threshold -= 0x400) {
        	if (physmem_select_masked_neighbor_node(private.p_nodeid,
                                         physmem_max_radius,
					 get_effective_nodemask(curthreadp),
                                         &neighbor,
                                         (selector_t *)page_borrow_selector, 
					 (void*)(long)freemem_threshold, (void*)size) > 0) {
                	if ((pfd = pagealloc_size_node(neighbor, cachekey, flags, size)) != NULL) {
                        	NUMA_STAT_ADD(neighbor, pagealloc_lent_memory, btoc(size)); 
				return pfd;
                	}
        	}
        }


        /*
         * If the above search failed & we were not trying all nodes because
	 * the user has a restrictive nodemask, try again but this time
	 * try all nodes.  Skip this if the user has specified VM_NODEMASK.
         */
	if (!(flags&VM_NODEMASK) && CNODEMASK_NOTEQ(get_effective_nodemask(curthreadp), boot_cnodemask)) {
		for (freemem_threshold = 0x400;
		     freemem_threshold >= 0;
		     freemem_threshold -= 0x400) {
        		if (physmem_select_masked_neighbor_node(private.p_nodeid,
                                         	physmem_max_radius,
					 	boot_cnodemask,
                                         	&neighbor,
                                         	(selector_t *)page_borrow_selector, 
					 	(void*)(long)freemem_threshold, (void*)size) > 0) {
               		 	if ((pfd = pagealloc_size_node(neighbor, cachekey, flags, size)) != NULL) {
               		         	NUMA_STAT_ADD(neighbor, pagealloc_lent_memory, btoc(size)); 
					return pfd;
                		}
        		}
        	}
	}

        return NULL;
}


/*
 *	Allocate a page of a given size from one of the free page headers.
 *	If desired (cachekey) and possible, choose pages that will
 *	be contiguous cache-wise with pages virtually contiguous. 
 * 	If page of a particular size is not available the next higher size is
 *	tried. If one is found the page is split until we get a page at the
 * 	required size.
 */

pfd_t *
pagealloc_size_node(
	register cnodeid_t node,
	register uint cachekey,
	register int flags,
	register size_t size)
{
#else /* !NUMA_BASE */

#define node	((cnodeid_t) 0)

#endif

	pgszindx_t	req_page_size_index, /* Requested page size index */
			cur_page_size_index; /* Current page size index */
	int		cur_size; 	/* Current page size */
	uint		cur_cache_key; 	/* Current cach key */
	register pfd_t	*pfd;

	ASSERT((node >= 0) && (node < maxnodes));

#ifdef MH_R10000_SPECULATION_WAR
        if (IS_R10000()) {
		if (flags & VM_UNCACHED)
			flags = (flags & ~VM_NO_DMA) | VM_DMA_RW;
		else if (flags & VM_DIRECT) {
			if (flags & VM_DMA_RW)
				cmn_err_tag(117,CE_PANIC,"pagealloc_size_node: impossible flag set");
			flags |= VM_NO_DMA;
		}
		if (!(flags & (VM_DMA_RW | VM_NO_DMA)))
			flags |= VM_DMA_RW;
		if ((flags & VM_DIRECT) &&
		    (! (flags & VM_UNCACHED)) &&
		    size > NBPP) 
			cmn_err_tag(118,CE_PANIC,"pagealloc_size_node: large page size cannot be VM_DIRECT");
	}
#endif /* MH_R10000_SPECULATION_WAR */

	cur_page_size_index = req_page_size_index = PAGE_SIZE_TO_INDEX(size);
	cur_size = size;

	while (	cur_page_size_index < NUM_PAGE_SIZES) {

		pfd = _pagealloc_size_node(node, cachekey, flags, cur_size);

		if (!pfd) {

			/*
			 * Try a higher sized page.
			 */
			INC_LPG_STAT(node, cur_page_size_index, PAGE_SPLIT_ATTS);

			cur_cache_key >>= PAGE_SIZE_SHIFT;
			cur_page_size_index++;
			cur_size <<= PAGE_SIZE_SHIFT;

		} else {
			/* Got a page */
			break;
		}
	}

	/*
	 * Wake up the coalescing daemon if we are running out
	 * of large pages with the caveats that there is a need
	 * for large pages and there is enough freememory in the
	 * node.
	 */
	if (req_page_size_index > MIN_PGSZ_INDEX) {
		pgno_t lowat;
		pg_free_t *pfl = GET_NODE_PFL(node);

		lowat = GET_LPAGE_LOWAT(pfl, req_page_size_index); 
		
		if (lowat && ((lowat * btoct(size)) < NODE_FREEMEM(node)) && 
		(GET_NUM_FREE_PAGES(node, req_page_size_index) <= lowat)) {
			coalesce_daemon_wakeup(node, CO_DAEMON_IDLE_WAIT);
		}
	}

	/*
	 * If we did not get any pfd return failure.
	 */

	if  (!pfd)
		return (NULL);


	/*
	 * If we got a page of the required size just return
	 * the pfd.
	 */

	if ( cur_page_size_index == req_page_size_index)
		return pfd;

	/*
	 * Split the large page into smaller pages and return a page
	 * of the request page size.
	 */

	pfd = lpage_split(node, cur_page_size_index,
                        req_page_size_index, pfd);

	ASSERT(pfd != NULL);

	return pfd;
}

#if !NUMA_BASE
#undef node
#endif

#ifdef NUMA_BASE

/*
 * Allocate a page of memory of a specified size following
 * a round-robin allocation policy, across all nodes.
 */

pfd_t *
pagealloc_size_rr(
	register uint cachekey,
	register int flags,
	register size_t	size)
{
	pfd_t	   *pfd;
	cnodeid_t  node_counter;

        /*
         * Global Round-robin rotor
         */
        static uint pagealloc_rr_rotor = 0;

        /*
         * Local Round-robin rotor
         */
        cnodeid_t local_rotor = 0;
        
	/*
	 * Allocate a page from the node specified by the rotor.
	 */


        /*
         * POSSIBLE ENHANCEMENT: It may be a good idea to check how much
         * memory is available on the source node before allocating
         * memory, to avoid unbalancing memory usage in the system.
         * We'd have to pay the expense of of checking extra state.
         */

        /*
         * pagealloc_rr_rotor is a global shared variable variable that
         * just gets incremented. We sample its current value, which may
         * be different from the value we incremented to, and take the mod
         * to get the node we want to allocate memory from. Sometimes we
         * may skip nodes because the incr is not atomic, but that's ok.
         * We only need to make sure that the node we choose is valid.
         */
        
        /*
         * If no memory is available on the current node, 
         * we just increment the rotor and try again, until
         * success, or no more memory is available in the system,
         * in which case we do a plain pagealloc_size. This final
         * pagealloc_size will most likely return NULL, but it may
         * return a valid page if we skipped a node due to the
         * non atomic pagealloc_rr_rotor increment.
         */

        for (node_counter = 0; node_counter < numnodes; node_counter++) {
                pagealloc_rr_rotor++;
                local_rotor =  pagealloc_rr_rotor % numnodes;
                ASSERT(local_rotor >= 0 && local_rotor < numnodes);                
                /* Are kernel allocations allowed on this node? */
                if (!CNODEMASK_TSTB(kernel_allow_cnodemask, local_rotor))
                        continue;
                if ((pfd = pagealloc_size_node(local_rotor, cachekey, flags, size)) != NULL) {
                        if (0 != node_counter)
                        {
                                NUMA_STAT_ADD(local_rotor, pagealloc_lent_memory, btoc(size)); 
                        }
			return pfd;
                }
        }


        return pagealloc_size(cachekey, flags, size);
}

#endif /* NUMA_BASE */


/*
 *	Allocate a page from one of the free page headers.
 *
 *	If desired (cachekey) and possible, choose pages that will
 *	be contiguous cache-wise with pages virtually contiguous.
 *	and just release it.
 */

#ifdef TILES_TO_LPAGES
extern pfd_t		*tile_pageselect(phead_t *, int, int);
#endif /* TILES_TO_LPAGES */

int cachekey_rotor = 0;

pfd_t *
_pagealloc_size_node(
	register cnodeid_t node,
	register uint cachekey,
	register int flags,
	register size_t size)
{
	register pfd_t		*pfd;
	register phead_t	*pheadp;
	struct searchtab	*stab;
	register int		i;
	register int		locktoken;
	int			avglen;
	pg_free_t		*pfl;
	int			pheadmask;
	pgszindx_t		page_size_index;
	phead_t			*pheadrotor, *pheadend;
	phead_t			*pheadpp;
	void			*tag;
	int			anon_tag;
        int                     local_node_freemem;
#ifdef TILES_TO_LPAGES
	tphead_t		*tpheadp;
#endif /* TILES_TO_LPAGES */
	/*
	 *	Unless requested otherwise, need to allocate a clean
	 *	page with respect to the i- and d-caches.
	 *	i- and d-caches must be flushed before returning a
	 *	STALE page (if searching for a clean page).  Since
	 *	it is hoped that most pages will cache backing store
	 *	data (ASSOC), the STALE_NOASSOC queues will often be
	 *	very short.  To avoid excessive cache flushes as a
	 *	small number of pages are cycled through the STALE_NOASSOC
	 *	queues, traverse from CLEAN_NOASSOC to CLEAN_ASSOC.
	 */

#ifdef PH_LONG
	static struct searchtab cleantab[PH_NLISTS] = {
		{ CLEAN_NOASSOC,  0},
		{ INTRANS_NOASSOC,STAB_INTRANSIT},
		{ STALE_NOASSOC,  STAB_FLUSH},
		{ CLEAN_ASSOC,    0},
		{ INTRANS_ASSOC,  STAB_INTRANSIT},
		{ STALE_ASSOC,    STAB_FLUSH} };
	static struct searchtab staletab[PH_NLISTS] = {
		{ STALE_NOASSOC,  0},
		{ INTRANS_NOASSOC,0},
		{ CLEAN_NOASSOC,  0},
		{ STALE_ASSOC,    0},
		{ INTRANS_ASSOC,  0},
		{ CLEAN_ASSOC,    0} };
#else
	static struct searchtab cleantab[PH_NLISTS] = {
		{ CLEAN_NOASSOC,  0},
		{ CLEAN_ASSOC,    0},
                { POISONOUS,      STAB_POISONOUS} };
#endif
	ASSERT(valid_page_size(size));
#ifdef MH_R10000_SPECULATION_WAR
        if (IS_R10000()) {
		if (flags & VM_UNCACHED)
			flags = (flags & ~VM_NO_DMA) | VM_DMA_RW;
		else if (flags & VM_DIRECT) {
			if (flags & VM_DMA_RW)
				cmn_err_tag(119,CE_PANIC,"_pagealloc_size_node: impossible flag set");
			flags |= VM_NO_DMA;
		}
		if (!(flags & (VM_DMA_RW | VM_NO_DMA)))
			flags |= VM_DMA_RW;
	}
#endif /* MH_R10000_SPECULATION_WAR */

	page_size_index = PAGE_SIZE_TO_INDEX(size);
retry_alloc:

	locktoken = PAGE_FREELIST_LOCK(node);

        local_node_freemem = NODE_FREEMEM(node);
	if (local_node_freemem == 0) {
		PAGE_FREELIST_UNLOCK(node, locktoken);
		return (pfd_t *)NULL;
	}

        /*
         * If we go below our per node low memory
         * threshold, we force an update on GLOBAL_FREEMEM,
         * so that in `clock' we can immediately (within one tick)
         * determine whether we need to wake up vhand.
         */
        if (local_node_freemem == NODE_FREEMEM_LOW_THRESHOLD()) {
                GLOBAL_FREEMEM_UPDATE();
        }

	pfl = GET_NODE_PFL(node);

	if (GET_NUM_FREE_PAGES(node, page_size_index) == 0) {
		PAGE_FREELIST_UNLOCK(node, locktoken);
		return (pfd_t *) NULL;
	}

#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000() &&
	    (((flags & VM_DMA_RW) &&
	     (page_size_index == MIN_PGSZ_INDEX) &&
	     (GET_NUM_FREE_PAGES(node, page_size_index) == smallk0_freemem)) ||
	     ((flags & VM_DIRECT) && !smallk0_freemem))) {
		PAGE_FREELIST_UNLOCK(node, locktoken);
		return (pfd_t *) NULL;
	}
#endif /* MH_R10000_SPECULATION_WAR */

	ASSERT(check_freemem_node(node));

	MINFO.palloc += btoc(size);
	SUB_NODE_FREEMEM(node, btoc(size));
	DEC_NUM_FREE_PAGES(node, page_size_index);
	ASSERT(check_freemem());

#ifdef TILES_TO_LPAGES
        /*
	 * For tiles support, we use a slightly different algorithm
	 * to select which phead we want to allocate from.  If we
	 * are allocating a movable page we prefer to allocate it
	 * from the pool of free pages that are in available, unfragmented
	 * tiles.  If we are allocating a fixed page we prefer to place
	 * it in a tile that already has other fixed pages so we start
	 * with the fragmented tile page list.
	 */

	/*
	 * compute average length list
	 * compute AFTER decrementing freemem so can perform the <= check
	 * below and not have to check for 0 explicitly.
	 *
	 * compute all the per <node,pagesize> data structures too.
	 *
	 * if we're looking for a 4K page, then go to the tpheads for
	 * help on selecting the optimal page.
	 */
	if (page_size_index == MIN_PGSZ_INDEX) {
#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000() &&
		    !(flags & VM_DMA_RW) &&
		    tphead[TPH_LOWMEM].count > 0)
			i = TPH_LOWMEM;
		else
#endif /* MH_R10000_SPECULATION_WAR */
		i = (flags & VM_MVOK) ? TPH_UNFRAG : TPH_FRAG;

		tpheadp = &tphead[i];

		/*
		 * If no pages in our first choice, try another pool.
		 */
		if (tpheadp->count == 0) {
			/*
			 * If we have large pages available on the
			 * TILE_INDEX list, use one (this will
			 * restock the desired pool w/a few residual
			 * free pages to satsify future requests).
			 * 
			 * By returning null here, the caller can try
			 * the next higher page size.
			 */
			if (tpheadp != &tphead[TPH_LOWMEM] &&
			    GET_NUM_FREE_PAGES(node, TILE_INDEX)) {
				MINFO.palloc -= btoc(size);
				ADD_NODE_FREEMEM(node, btoc(size));
				INC_NUM_FREE_PAGES(node, page_size_index);
				ASSERT(check_freemem());
				PAGE_FREELIST_UNLOCK(node, locktoken);
				return ((pfd_t *)NULL);
			}
			tpheadp = &tphead[!i];
		}

		ASSERT(tpheadp->count);
		ASSERT((GET_NUM_FREE_PAGES(node, MIN_PGSZ_INDEX) + 1) ==
		       (tphead[TPH_UNFRAG].count + tphead[TPH_FRAG].count +
			tphead[TPH_LOWMEM].count) ||
		       (GET_NUM_FREE_PAGES(node, MIN_PGSZ_INDEX) == 0));

		avglen = (tpheadp->count-1) >> PHEADSHIFT(page_size_index);
		pheadmask = PHEADMASK(page_size_index);
		pheadrotor = tpheadp->pheadrotor;
		pheadend = tpheadp->pheadend;
		pheadpp = tpheadp->phead;
	} else {
		avglen = GET_NUM_FREE_PAGES(node, page_size_index) >> 
			PHEADSHIFT(page_size_index);
		pheadmask = PHEADMASK(page_size_index);
		pheadrotor = GET_PHEADROTOR(node,page_size_index);
		pheadend = pfl->pheadend[page_size_index];
		pheadpp = pfl->phead[page_size_index];
	}
#else
	avglen = GET_NUM_FREE_PAGES(node, page_size_index) >> 
		PHEADSHIFT(page_size_index);
	pheadmask = PHEADMASK(page_size_index);
	pheadrotor = GET_PHEADROTOR(node,page_size_index);
	pheadend = pfl->pheadend[page_size_index];
	pheadpp = pfl->phead[page_size_index];
#endif /* TILES_TO_LPAGES */
	if (flags & VM_UNSEQ) {
uncached:
		/*
		 * No need to worry about cache alignment -- rotor through
		 * the free page table entries.  Pick from a list which has
		 * at least an average # of pages left.
		 */

		pheadp = pheadrotor;
		while (pheadp->ph_count <= avglen) {
			if (++pheadp == pheadend)
				pheadp = &pheadpp[0];
			ASSERT(pheadp != pheadrotor);
		}

#ifdef TILES_TO_LPAGES
		if (page_size_index == MIN_PGSZ_INDEX) {
			if ((pheadp + 1) == tpheadp->pheadend)
				tpheadp->pheadrotor = &tpheadp->phead[0];
			else
				tpheadp->pheadrotor = pheadp + 1;
		} else {
			if ((pheadp + 1) == pheadend)
				SET_PHEADROTOR(node,page_size_index,
					       &pheadpp[0]);
			else
				SET_PHEADROTOR(node,page_size_index,
					       pheadp + 1);
		}
#else
		if ((pheadp + 1) == pheadend)
			SET_PHEADROTOR(node,page_size_index, &pheadpp[0]);
		else
			SET_PHEADROTOR(node,page_size_index,
				       pheadp + 1);
#endif /* TILES_TO_LPAGES */

#ifdef PGLISTDEBUG
		if (flags & VM_UNSEQ)
			un[pheadp-&pfl->phead[page_size_index][0]]++;
#endif
	} else {
		if (cachekey == NOCOLOR)
			cachekey = cachekey_rotor++ & 0xffff;
		/*
		 * See vcache2 for a description of this ugly hack!!!!!
		 */
		else if (cachekey == 0xbadc)
			cachekey = NOCOLOR;

		/*
		 * If using large page sizes, some of the bottom bits of
		 * the key are not significant - discard them. The reason for this
		 * is that the color was calculated using the logical page number
		 * of the object. Multiple logical page numbers all map to the
		 * same large page.
		 */
		cachekey = cachekey >> (2*page_size_index);
		pheadp = &pheadpp[cachekey & pheadmask];

		if (flags & VM_PACOLOR) {
			if (pheadp->ph_count) 
				goto got_it;
			MINFO.palloc -= btoc(size);
			ADD_NODE_FREEMEM(node, btoc(size));
			INC_NUM_FREE_PAGES(node, page_size_index);
			ASSERT(check_freemem());
			PAGE_FREELIST_UNLOCK(node, locktoken);
			return ((pfd_t *)NULL);
		}

		/*
		 * If this table entry is empty (or has
		 * <= 1/4 the avg free page list size) grab
		 * a page from another free page table entry.
		 * (n.b.: must be <= to catch the case where avglen == 0)
		 *
		 * First try folding the phead lists to find pages
		 * that match half- quarter- and eighth-size caches.
		 * If that fails, take whatever the rotor finds.
		 */
		if (pheadp->ph_count <= (avglen >> 2)) {
			register phead_t *op = pheadp;
			register int m;
			i = (pheadmask+1)/2;
			for (i = (pheadmask+1)/2; i > pheadmask/8; i >>= 1) {
				for (m = cachekey + i ; ; m+=i) {
					pheadp = &pheadpp [m & pheadmask];
					if (pheadp == op)
						break;
					if (pheadp->ph_count > (avglen >> 2))
						goto got_it;
				}
			}

			/* Give up and ... */
			goto uncached;
		}
	}

got_it:
	/*
	 * Allocate a page from one of the lists associated with the
	 * chosen free page table entry. (There's at least one!)
	 * Depending on whether a stale page is OK or not, a different
	 * sequence of lists is searched.  Priority is given to stale
	 * lists if possible, and toward saving pages with TLB/disk
	 * association since they are expensive to restore.
	 * If a clean page is needed but not available, it will be
	 * necessary to perform a synchronous flush of the instruction
	 * cache across all processors to restore all stale pages to
	 * a clean state.
	 */
#ifdef PH_LONG
	stab = (flags & VM_STALE) ? staletab : cleantab;
#else
        stab = cleantab;
#endif        

#ifdef PGLISTDEBUG
	if (chkhash != -1 && pheadp == &pfl->phead[page_size_index][chkhash]) {
		cmn_err(CE_CONT, "key 0x%x flgs 0x%x\n",
			cachekey, flags);
		if (allocdrop)
			debug(0);
	}
	al[pheadp-&pfl->phead[page_size_index][0]]++;
#endif
	for (i = PH_NLISTS; --i >= 0; stab++) {

		pfd = pheadp->ph_list[stab->list].pf_next;
		if (pfd->pf_prev == pfd)
			continue;

#ifdef TILES_TO_LPAGES
		/*
		 * If we're grabbing a page from the FRAG list, there
		 * may be a better candidate than the page we've
		 * selected (i.e. !VM_MVOK should come from tiles
		 * w/more locked pages and VM_MVOK should come from
		 * tiles w/fewer locked pages).
		 */
		if (tpheadp == &tphead[TPH_FRAG]) {
			pfd = tile_pageselect(pheadp, stab->list, flags);
		}
#endif /* TILES_TO_LPAGES */

#ifdef	DEBUG
		ASSERT(PFDAT_TO_PAGE_SIZE(pfd) == size);
#endif

		/*
		 * Unlink from the free queue,
		 * unhash if necessary, and set up pfd.
		 */

		nested_pfdat_lock(pfd);
		pagedequeue(pfd, pheadp);

		clr_pfd_bitfield(pfdat_to_tainted_bm(pfd),pfd,btoc(size));
		clr_pfd_bitfield(pfdat_to_pure_bm(pfd),pfd,btoc(size));

#ifdef TILES_TO_LPAGES
		if ((flags & VM_MVOK) == 0)
			TILE_PAGELOCK(pfd, btoc(size));
#endif /* TILES_TO_LPAGES */

		if (pfd->pf_flags & P_HASH) {
			/*
			 * Hold the pfdat lock and check if the page still
			 * has an association with the object. If not nothing
			 * needs to be done. Otherwise, set the P_RECYCLE flag
			 * before we unlock the page freelist lock. 
			 * We then need to hold the object so it is still
			 * around when we
			 * call into the object manager to remove the page out
			 * it's page cache.  Note that we have to save the
			 * tag in a local since the object manager may see
			 * the recycle flag and unhash the page before we
			 * get in (which would clear the pf_tag field).
			 */

			pfd->pf_flags |= P_RECYCLE;
			tag = pfd->pf_tag;
			if (pfd->pf_flags & P_ANON) {
				anon_hold((anon_hdl)tag);
				anon_tag = 1;
			} else {
				ASSERT(((vnode_t *)tag)->v_pcacheref >= 0);
				VNODE_PCACHE_INCREF((vnode_t *)tag);
				ASSERT(((vnode_t *)tag)->v_pcacheref > 0);
				anon_tag = 0;
			}
				
		}

		nested_pfdat_unlock(pfd);
		PAGE_FREELIST_UNLOCK(node, locktoken);

		if (pfd->pf_flags & P_RECYCLE) {
			if (anon_tag)
				anon_recycle((anon_hdl)tag, pfd);	
			else 
				vnode_page_recycle((vnode_t *)tag, pfd);

			ASSERT((pfd->pf_flags & 
				(P_ANON|P_HASH|P_QUEUE|P_SQUEUE)) == 0);
		}
		/*
		 * Check if this page was marked as having an error. If so,
		 * call the error handling code to clean this page for us,
		 * If the page is not cleanable, add it to a discarded list
		 * of pages.
		 */
		if (pfdat_iserror(pfd)) {
			if (page_error_clean(pfd) == 0) {
				page_discard_enqueue(pfd);
				goto retry_alloc;
			}
		}

#ifdef PH_LONG
		/* 
		 * If you are holding off device interrupts, or are
		 * calling from an interrupt handler (soft clock or
		 * net), and you ask for a clean page, then we simply
		 * flush the one page that we are getting.
		 *
		 * During system boot up, we are more lenient.
		 * This rule also only applies to SP systems.
		 */
#if IP32
#define isspldev(s) (((s >> SR_IMASKSHIFT) & 0xff) > (1 << 3))
#else
#define isspldev(s) ((s & SR_IMASK2) != SR_IMASK2)
#endif
#define is_interrupt	(private.p_kstackflag == PDA_CURINTSTK)
#define _B_ARGS CACH_DCACHE|CACH_ICACHE|CACH_WBACK|CACH_INVAL|CACH_FORCE|CACH_NOADDR
		if (stab->flags) {
			size_t tmp_size;
			pfn_t pfn;

			if (stab->flags & STAB_INTRANSIT) {
				pfn = pfdattopfn(pfd);
				for (tmp_size = 0; tmp_size < size; 
						tmp_size += NBPP, pfn++) {
					_bclean_caches(0, NBPP, pfn, _B_ARGS);
#ifdef _VCE_AVOIDANCE
					if (pfd->pf_flags & P_MULTI_COLOR) {
						_bclean_caches(0, NBPP, pfn,
							CACH_OTHER_COLORS|_B_ARGS);
					}
					if (vce_avoidance)
						pfd_set_vcolor(pfntopfdat(pfn),-1);
#endif /* _VCE_AVOIDANCE */
				}
			}
			else /* if (stab->flags & STAB_FLUSH) */
			{
				if ((isspldev(locktoken) || is_interrupt) &&
				    boottime.tv_sec) {
					
					pfn = pfdattopfn(pfd);
					for (tmp_size = 0; tmp_size < size; 
						tmp_size += NBPP, pfn++) {
						_bclean_caches(0, NBPP, pfn,
							_B_ARGS);
#ifdef _VCE_AVOIDANCE
						if (pfd->pf_flags & P_MULTI_COLOR) {
							_bclean_caches(0, NBPP, pfn,
								CACH_OTHER_COLORS|_B_ARGS);
						}
						if (vce_avoidance)
							pfd_set_vcolor(pfntopfdat(pfn),-1);
#endif /* _VCE_AVOIDANCE */
					}
				} else {
#ifdef	R4000PC
	/*
	 * Systems with very small caches should flush everythings.
	 * Ditto for systems that can flush their entire caches faster

	 * than they can flush specific lines (this includes R4600SC).
	 */
#ifdef	R4600
					extern int two_set_pcaches;
#endif

					if (private.p_scachesize == 0
#ifdef	R4600
					 || two_set_pcaches
#endif
					 )
						flushcaches();
					else
#endif	/* R4000PC */
					flush_phead(pheadp, page_size_index);
				}
			}
		}
#endif /* PH_LONG */
                
		if (stab->flags & STAB_POISONOUS) {
#if !defined(NUMA_BASE) 
			cmn_err_tag(120,CE_PANIC, "pagealloc_node - Poisnous page without migration");
#endif                        
			PHEAD_UNPOISON(node, pheadp, pfd);
                }
                
		ASSERT(!(pfd->pf_flags & P_HAS_POISONED_PAGES));

		pfd->pf_flags = P_CACHESTALE;
		pfd->pf_pageno = PGNULL;
		pfd->pf_vp = 0;
		pfd->pf_use = 1;
		pfd->pf_rawcnt = 0;

		ASSERT(GET_RMAP_PDEP1(pfd) == 0);
		ASSERT(pfd->pf_pdep2 == 0);
		ASSERT(pfd->pf_rmapp == 0);

#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000()) {
                        if (flags & VM_NO_DMA)
                                pfd->pf_flags |= P_NO_DMA;
                }
#endif /* MH_R10000_SPECULATION_WAR */

		/*
		 * Initialize the large page pfdats.
		 */

		if ( size > NBPP) {
			lpage_init_pfdat(pfd,size,PAGE_ALLOC);
		} else {
			CLR_PAGE_SIZE_INDEX(pfd);
		}

		return(pfd);
	}

        dprintf("pfl pheadmask pheadrotor pheadend pheadpp 0x%x 0x%x 0x%x 0x%x 0x%x\r\n\r\n",
               pfl, pheadmask, pheadrotor, pheadend, pheadpp);
        dprintf("node NODE_FREEMEM GET_NUM_FREE_PAGES %d %d %d\r\n\r\n",
               node, NODE_FREEMEM(node), GET_NUM_FREE_PAGES(node, page_size_index));

        conbuf_flush(CONBUF_UNLOCKED);

        debug("alloc panic");
        
	cmn_err_tag(121,CE_PANIC, "pagealloc: pheadp 0x%x", pheadp);
	/* NOTREACHED */
	return((pfd_t *)0);
}


/*
 *	Allocate a page of memory of a specified size.
 *	Do not allocate the last page of any physically
 *	contiguous chunk.  In other words, the page after
 *	the allocated page needs to be valid memory.
 */
pfd_t *
pagealloc_notatend(
	register uint cachekey,
	register int flags,
	register size_t	size)
{
	pfd_t	*pfd;
	pfd_t	*pfdchain = NULL;
	pfd_t	*pfdtofree;
	pfn_t	pfn;
	pfn_t	nextpfn;

	while ((pfd = pagealloc_size(cachekey, flags, size)) != NULL) {
		pfn = pfdattopfn(pfd);
		nextpfn = pfn + btoct(size);
		if (page_validate_pfdat(pfntopfdat(nextpfn))) {
			break;
		}

		/*
		 * can't use this pfd, hang on to it
		 */
		pfd->pf_next = pfdchain;
		pfdchain = pfd;
	}

	/*
	 * Empty the pfdchain.
	 */
	while (pfdchain) {
		pfdtofree = pfdchain;
		pfdchain = pfdchain->pf_next;
		ASSERT(page_validate_pfdat(pfdtofree));
		pagefree_size(pfdtofree, size);
	}
	return pfd;
}



void
requeue(register pfd_t *pfd, int list)
{
	register phead_t *pheadp;
	register plist_t *plistp;
	register pfd_t	*prev, *next;

	next = pfd->pf_next;
	prev = pfd->pf_prev;
	next->pf_prev = prev;
	prev->pf_next = next;

	pheadp = PFD_TO_PHEAD(pfd, MIN_PGSZ_INDEX);
	plistp = &pheadp->ph_list[list];
	append(plistp, pfd);
}

#ifdef DEBUG
int freelist_sanity_on;
int
freelist_sanity_nolock(cnodeid_t node,int cookie)
{
	pg_free_t 	*pfl;
	phead_t		*phead;
	plist_t		*plist;
	pfd_t		*pfirst;
	int		count;
	int		i, j, k;
	int		count_mismatch = 0;

	if (!freelist_sanity_on)
		return 0;

	pfl = GET_NODE_PFL(node);
	for (i = MIN_PGSZ_INDEX; i < MAX_PGSZ_INDEX; i++) {
		phead = pfl->phead[i];
		for (j = 0; j <= PHEADMASK(i); j++, phead++) {
			count = 0;
			for (k = 0; k < PH_NLISTS; k++) {
				plist = &phead->ph_list[k];
				pfirst = plist->pf_next;
		
				while (pfirst !=  (pfd_t *)plist) {
					if (PFD_TO_PHEAD(pfirst, i) != phead) {

						printf("Phead mismatch for pfd 0x%x actual %x computed %x cookie %d\n",
							pfirst, phead,
							PFD_TO_PHEAD(pfirst,i),
							cookie);
						debug("ring");
					}
					pfirst = pfirst->pf_next;
					count++;
				}
			}
			if (count != phead->ph_count) {
				printf("count mismatch: phead 0x%x count %d ph_count %d cookie %d\n", 
						phead, count, phead->ph_count,
						cookie);
				count_mismatch++;
			}
		}
	}
	return (count_mismatch == 0);
}

int
freelist_sanity_cookie(cnodeid_t node, int cookie)
{
	int rv;
	int		locktoken;

	if(!freelist_sanity_on)
		return 0;
	locktoken = PAGE_FREELIST_LOCK(node);
	rv = freelist_sanity_nolock(node,cookie);
	PAGE_FREELIST_UNLOCK(node, locktoken);
	return rv;
}

int
freelist_sanity(cnodeid_t node)
{
	int rv;
	int		locktoken;

	if(!freelist_sanity_on)
		return 0;
	locktoken = PAGE_FREELIST_LOCK(node);
	rv = freelist_sanity_nolock(node,1);
	PAGE_FREELIST_UNLOCK(node, locktoken);
	return rv;
}
#endif	/* DEBUG */

/*
 *	Dequeue a page from a free page list, decrementing
 *	the count associated with the free page table entry.
 */
void
pagedequeue(register pfd_t *pfd, register phead_t *pheadp)
{
	register pfd_t	*next, *prev;
	int size = PFDAT_TO_PAGE_SIZE(pfd);
	/*REFERENCED*/
        cnodeid_t cnodeid;

        cnodeid = pfdattocnode(pfd);

	ASSERT(PAGE_FREELIST_ISLOCKED(cnodeid));
        
	ASSERT(pfd->pf_flags & P_QUEUE);
	if (pfd->pf_use != 0)
		cmn_err_tag(122,CE_PANIC, "pagedequeue: pf_use (0x%x)", pfd);

	if (pheadp->ph_count <= 0)
		cmn_err_tag(123,CE_PANIC, "pagedequeue: ph_count (0x%x)", pheadp);
	pheadp->ph_count--;

#ifdef TILES_TO_LPAGES
	if (PFDAT_TO_PGSZ_INDEX(pfd) == MIN_PGSZ_INDEX)
		TILE_TPHEAD_DEC_PAGES(pfd);
#endif /* TILES_TO_LPAGES */
#ifdef MH_R10000_SPECULATION_WAR
	if (pfd < pfd_lowmem) {
		smallk0_freemem--;
	}
#endif
	
#if	defined(NUMA_BASE)
	if (pfd->pf_flags & (P_POISONED|P_HAS_POISONED_PAGES))
		pheadp->ph_poison_count--;
#endif

	next = pfd->pf_next;
	prev = pfd->pf_prev;
	next->pf_prev = prev;
	prev->pf_next = next;
	pfd->pf_next = pfd->pf_prev = NULL;

	pfd->pf_flags &= ~P_QUEUE;

	if (size > NBPP) {
                SUB_NODE_EMPTYMEM(cnodeid, btoc(size));
        } else if ((pfd->pf_flags & P_HASH) == 0) {
                SUB_NODE_EMPTYMEM(cnodeid, 1);
        }
}

/*
 * Remove a page from the freelist if we can.  The page could have already
 * been taken off by pagealloc trying to recycle to the page.  If we hit
 * this case, we do nothing as pagealloc has claimed the page.  The caller
 * is responsible for detecting and handling this case.
 */

void
pageunfree(pfd_t *pfd)
{
	int		locktoken;
	/*REFERENCED*/
	cnodeid_t	node;

	node = pfdattocnode(pfd);

	locktoken = PAGE_FREELIST_LOCK(node);

	nested_pfdat_lock(pfd);
	ASSERT(pfd->pf_flags & (P_QUEUE|P_RECYCLE));
	if (pfd->pf_flags & P_QUEUE) {
		ASSERT(NODE_FREEMEM(node) > 0);
		SUB_NODE_FREEMEM(node, btoct(NBPP));
		DEC_NUM_FREE_PAGES(node, MIN_PGSZ_INDEX);
		pagedequeue(pfd, PFD_TO_PHEAD(pfd, MIN_PGSZ_INDEX));
		clr_pfd_bit(pfdat_to_pure_bm(pfd),pfd);
		clr_pfd_bit(pfdat_to_tainted_bm(pfd),pfd);
	}
	nested_pfdat_unlock(pfd);

	PAGE_FREELIST_UNLOCK(node, locktoken);	
}


/* 
 * contig_memalloc(npgs, alignment, flags)
 *
 * wrapper routine to contmemall which does reservation of memory.
 *
 * flags - see contmemall.
 *
 */
pgno_t
contig_memalloc(register int npgs, register int alignment, int flags)
{
	register pgno_t ret;


	if (reservemem(GLOBAL_POOL, npgs, npgs, npgs))
		return(NULL);

	if ((ret = contmemall(npgs, alignment, flags)) == NULL)
		unreservemem(GLOBAL_POOL, npgs, npgs, npgs);
	
	return ret;
}

/*
 * contmemall(npgs, alignment, flags)
 *
 * allocate physically contiguous pages
 * return pfn of first page
 *
 * flags are those passed to kvpalloc -- the only flags of
 * interest are: VM_STALE  - don't flush caches.
 *		 VM_DIRECT - allocate K0 pages
 */
pgno_t
contmemall(register int npgs, register int alignment, int flags)
{
#if NUMA_BASE
	cnodeid_t	node;
	pgno_t		pfn;

	/*
	 * Try local node first.
	 */

	if ((pfn = contmemall_node(private.p_nodeid, npgs, alignment, flags))
	     != NULL)
		return pfn;

	for (node = 0; node < numnodes; node++)
	    if ((pfn = contmemall_node(node, npgs, alignment, flags)) != NULL)
		return pfn;

	return NULL;
}


/*
 * contmemall_node(node, npgs, alignment, flags)
 *
 * allocate physically contiguous pages within the given node.
 * return pfn of first page
 *
 * flags are those passed to kvpalloc -- the only flags of
 * interest are: VM_STALE  - don't flush caches.
 *		 VM_DIRECT - allocate K0 pages
 */
pgno_t
contmemall_node(cnodeid_t node, register int npgs, register int alignment, 
		int flags)
{
#else
#define node	((cnodeid_t) 0)

#endif /* NUMA_BASE */

	register struct pfdat	*pfd, *pfd1, *pfd_limit;
	         int pgs;
	register int locktoken;
	/*REFERENCED*/
	int		freemem;
	pg_free_t	*pfl = GET_NODE_PFL(node);

	ASSERT(npgs > 0);

	/* The code below relies on aligned blocks being powers of 2 */
	ASSERT((alignment & (alignment-1)) == 0);

	/*
	 * Negative alignment is crazy, and zero alignment
	 * will cause an infinite loop.
	 */
	if (alignment <= 0)
		return(NULL);

#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000()) {
        	if (!(flags & (VM_DMA_RW|VM_NO_DMA|VM_DIRECT)))
                	flags |= VM_DMA_RW;
	}
#endif /* MH_R10000_SPECULATION_WAR */

	/* just lookin' */
	if (NODE_FREEMEM(node) < npgs)
		return(NULL);

#ifdef MH_R10000_SPECULATION_WAR
        if ((flags & VM_DMA_RW) &&
	    ((GLOBAL_FREEMEM() - smallk0_freemem) < npgs))
		return(NULL);
#endif /* MH_R10000_SPECULATION_WAR */

	pfd1 = pfl->pfd_low;
#ifdef MH_R10000_SPECULATION_WAR
        if ((flags & VM_DMA_RW) && pfd1 < pfd_lowmem)
		pfd1 = pfd_lowmem;
#endif /* MH_R10000_SPECULATION_WAR */

	if (flags & VM_DIRECT) {
#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000() && !(flags & VM_UNCACHED))
			pfd_limit = pfd_lowmem - 1;
		else
#endif /* MH_R10000_SPECULATION_WAR */
		pfd_limit = pfd_direct;
	} else
		pfd_limit = pfl->pfd_high;

	locktoken = PAGE_FREELIST_LOCK(node);

	while ((pfd1 = pfdat_probe(pfd1, &pgs)) != NULL) {
#ifndef MH_R10000_SPECULATION_WAR
		pfd_limit = pfd1 + pgs - 1;

		if ((flags & VM_DIRECT) && pfd_limit > pfd_direct)
			pfd_limit = pfd_direct;
#endif
		if ((pfd = check_pfd_range(pfd1, pfd_limit, npgs,
					   alignment)) != NULL)
			goto reap;

		pfd1 = pfd1 + pgs;
	}

	PAGE_FREELIST_UNLOCK(node, locktoken);
	return(NULL);

reap:
	/*
	 * Take pages *pfd .. *(pfd+npgs-1) off free list
	 */

	freemem = NODE_FREEMEM(node);
	for (pfd1 = pfd, pgs = npgs; pgs;) {

		/*
		 * Compute the page size as the pfd can be a large page.
		 */
		pgszindx_t page_size_index = PFDAT_TO_PGSZ_INDEX(pfd1);
		int num_base_pages = PGSZ_INDEX_TO_CLICKS(page_size_index);

		pagedequeue(pfd1, PFD_TO_PHEAD(pfd1, page_size_index));
		clr_pfd_bitfield(pfdat_to_tainted_bm(pfd),pfd1,num_base_pages);
		clr_pfd_bitfield(pfdat_to_pure_bm(pfd),pfd1,num_base_pages);
		DEC_NUM_FREE_PAGES(node, page_size_index);
		SUB_NODE_FREEMEM(node, num_base_pages);

		if (num_base_pages > btoc(NBPP)) {
			/*
			 * If we need only a part of the large page
			 * free up the rest of the pfdats into the free pool
			 * as base page size pfdats.
			 * Initialize them first and give them  temporary
			 * use count of 1 so that _pagefree_size does not
			 * complain.
			 */
			if (num_base_pages > pgs) {
				pfd_t *tmp_pfd = pfd1 + pgs;
				pfd_t *end_pfd = pfd1 + num_base_pages;

				while (tmp_pfd < end_pfd)  {
					tmp_pfd->pf_next = 
						tmp_pfd->pf_prev = NULL;
					tmp_pfd->pf_flags = P_CACHESTALE;
					tmp_pfd->pf_pageno = PGNULL;
					tmp_pfd->pf_vp = 0;
					tmp_pfd->pf_use = 1;		
					tmp_pfd->pf_rawcnt = 0;
					CLR_PAGE_SIZE_INDEX(tmp_pfd);
					INC_NUM_TOTAL_PAGES(node, MIN_PGSZ_INDEX);
					_pagefree_size(tmp_pfd, NBPP, 0);
					tmp_pfd++;
				}
				pfd1 += pgs;
				pgs  = 0;
			} else {
				pfd1 += num_base_pages;
				pgs -= num_base_pages;
			}
		} else {
			ASSERT(num_base_pages == 1);
			nested_pfdat_lock(pfd1);

			if (pfd1->pf_flags & P_HASH) {
				pfd1->pf_flags |= P_RECYCLE;

			/*
			 * Once we let go of the pfdat lock, the tag in the
			 * pfdat could change.  Since we have to call
			 * <pcache>_recycle with the same tag we passed to
			 * <pcache>_hold, we remember the tag in the pf_next
			 * field (which isn't being used at the moment since
			 * the page is already off the free list).
			 *
			 *
			 * In addition, use pf_prev (bit 0) to indicate if
			 * it's an anon page. 
			 * This is needed since anon_pfind turns off P_ANON
			 * bit, and we could be racing with anon_pfind.
			 */

				pfd1->pf_next = (pfd_t *)pfd1->pf_tag;

				if (pfd1->pf_flags & P_ANON) {
					pfd1->pf_prev = (pfd_t *)(__psunsigned_t)1;
					anon_hold((anon_hdl)pfd1->pf_tag);
				} else {
					pfd1->pf_prev = (pfd_t *)0;
					VNODE_PCACHE_INCREF(pfd1->pf_vp);
				}
			}

			nested_pfdat_unlock(pfd1);

			TILE_PAGELOCK(pfd1, 1);

#ifdef MH_R10000_SPECULATION_WAR
			if (IS_R10000() && flags & VM_NO_DMA)
				pfd1->pf_flags |= P_NO_DMA;
#endif /* MH_R10000_SPECULATION_WAR */
			pfd1++;
			pgs--;
		}
	}
	ASSERT(freemem == (NODE_FREEMEM(node) + npgs));


	PAGE_FREELIST_UNLOCK(node, locktoken);

	/*
	 * Now disassociate the pages from their memory object
	 * if there is one.
	 */

	for (pfd1 = pfd, pgs = npgs; pgs; pfd1++, pgs--) {

		/*
		 * Depending on if the page is anon or 
		 * vnode, we need to call the specific recycle 
		 * routine. 
		 * 
		 * We check for this by looking at bit 0 in the
		 * pf_prev field 
		 */
		if (pfd1->pf_flags & P_RECYCLE) {
			ASSERT(PFDAT_TO_PGSZ_INDEX(pfd1) == MIN_PGSZ_INDEX);
			ASSERT(pfd1->pf_next);
			if ((__psunsigned_t)(pfd1->pf_prev) & 1) {
				/* 
				 * We mark bit 0 as 1 if it's an anon page
				 * in the loop above. So if it's set, page
				 * was an anon page. Call anon_recycle.
				 * Make sure that the bit 0 is turned off.
				 */

				anon_recycle((anon_hdl) pfd1->pf_next, pfd1);
			} else {
				vnode_page_recycle((vnode_t *)pfd1->pf_next, pfd1);
			}
			pfd1->pf_prev = pfd1->pf_next = 0;
		}

		/*
		 * Page is now uncached.  It's now free, unassociated
		 * with anything and all ours.
		 */

		pfd1->pf_pageno = PGNULL;
		pfd1->pf_flags = P_DUMP | P_CACHESTALE;
		if (flags & VM_BULKDATA)
			pfd1->pf_flags |= P_BULKDATA;
		pfd1->pf_vp = 0;
		pfd1->pf_use = 1;
		pfd1->pf_rawcnt = 0;
		CLR_PAGE_SIZE_INDEX(pfd1);
	}

#ifdef PH_LONG
	if (!(flags & VM_STALE))
		flush_pheads(pfd, npgs);
#endif

	return pfdattopfn(pfd);
}

#if !NUMA_BASE
#undef node
#endif


/*
 * Check a range of pfdats for a contiguous block of free pages of size
 * npgs with the specified alignment.
 * We do two passes. In the first round we skip large pages. In the next round
 * we include large pages in search. This eliminates unnecessary fragmentation.
 */

static pfd_t *
check_pfd_range(pfd_t *pfd, pfd_t *pfd_limit, pgno_t npgs, int alignment)
{
	pfd_t	*pfd1;
	pfd_t	*start_pfd;
	int	skip_large_pages = 1;

	start_pfd = pfd;

next:
	/* Start on appropriate boundary */

	pfd += (alignment - (pfdattopfn(pfd) & (alignment-1))) & (alignment-1);

	for ( ; pfd <= pfd_limit; pfd += alignment) {
		if (pfd->pf_flags & P_QUEUE) {
			register int nmpgs;

			for (pfd1 = pfd+1, nmpgs = npgs; --nmpgs > 0; pfd1++) {
				if (pfd1 > pfd_limit) {
					return(NULL);
				}

				if (PFDAT_TO_PGSZ_INDEX(pfd1) && 
							skip_large_pages) {
					pfd = pfd1;
					goto next;
				}

				if (!(pfd1->pf_flags & P_QUEUE)) {
					if (PFDAT_TO_PGSZ_INDEX(pfd1) 
						!= SENTINEL_PGSZ_INDEX) {
						pfd = pfd1;
						goto next;
					}
				}

				/*
				 * Pages with errors are not allocatable. Skip
				 * to the next one.
				 */
				if (pfdat_iserror(pfd1)) {
					pfd = pfd1;
					goto next;
				}

			}

			ASSERT(nmpgs == 0);
			return pfd;	/* success! */
		}
	}

	if (skip_large_pages) {
		skip_large_pages = 0;
		pfd = start_pfd;
		goto next;
	}
		
	return NULL;
}


/*
 * Return a contiguous range of pfdats starting with the first existing
 * page that is greater than or equal to the specified starting pfdat.
 * The purpose of this routine is to allow the caller to linearly
 * scan pfdats without having to know about the details of discontiguous
 * memory and pfdat tables.  The returned pfd is always in the same
 * node as the start_pfd.  In other words, this routine does not cross
 * between nodes; that is the caller's responsibility.
 *
 * Parameters:
 *		start_pfd - the pfd where a search for the next contiguous
 *			    range of pfd's is to begin
 *
 *		npgs      - return value only: number of pages in returned
 * 			    range of pfd's
 *
 * Returns:
 *		pointer to the first pfd that is greater than or equal to
 * 		start_pfd or NULL if there are no more pfd's after start_pfd
 * 		in that node's memory.
 */

pfd_t *
pfdat_probe(pfd_t *start_pfd, int *npgs)
{
#if DISCONTIG_PHYSMEM
	cnodeid_t	node;
	int		slot;
	int		last_slot;
	pfd_t		*pfd, *pfd_limit;
	pfn_t		pfn;
	pg_free_t	*pfl;

	node = local_pfdattocnode(start_pfd);

	last_slot = node_getlastslot(node);

	/*
	 * Round up to first pfdat in node if necessary.
	 */

	pfl = GET_NODE_PFL(node);
	if (start_pfd < pfl->pfd_low)
		start_pfd = pfl->pfd_low;

	/*
	 * Search through all the memory slots until
	 * we find the slot containing start_pfd (if there is one).
	 */

	for (slot = 0; slot <= last_slot; slot++) {
		pfn = slot_getbasepfn(node,slot);
    
		pfd = local_pfntopfdat(pfn);
		pfd_limit = pfd + slot_getsize(node,slot) - 1;

		/*
		 * If start_pfd is below the start of this slot,
		 * then it means we have skipped over some non-existant
		 * memory.  Restart start_pfd to the base of this slot.
		 */
		if (start_pfd < pfd)
			start_pfd = pfd;

		/*
		 * If start_pfd falls below the end of this slot,
		 * then we've found the chunk of pfdats to return.
		 * Compute the number of pfds between here and the
		 * end of the slot.
		 */
		if (start_pfd < pfd_limit) {
			*npgs = pfd_limit - start_pfd + 1;
			return start_pfd;
		}
	}

	/*
	 * Start_pfd was greater than the last pfd for this node.  Return
	 * NULL to indicate the end.
	 */

	return NULL;

#else
	cnodeid_t	node;
	pg_free_t	*pfl;

	node = pfdattocnode(start_pfd);

	pfl = GET_NODE_PFL(node);
	/*
	 * Round up to first pfdat in node if necessary.
	 */

	if (start_pfd < pfl->pfd_low)
		start_pfd = pfl->pfd_low;

	/*
	 * If start_pfd is greater than the last pfd for this node, return
	 * NULL to indicate the end.
	 */

	if (start_pfd > pfl->pfd_high)
		return NULL;

	*npgs = pfl->pfd_high - start_pfd + 1;
	return start_pfd;

#endif /* DISCONTIG_PHYSMEM */
}

#ifdef NUMA_BASE 
/*
 * Given a pfn in a virtual intra-node contiguous physical (cpfn)
 * memory space, return the actual pfn on the real intra-node discontiguous
 * physical space.
 */
pfn_t
cpfn_to_pfn(cnodeid_t node, pfn_t cpfn)
{
        pfn_t cpfn_low;
        pfn_t cpfn_high;
        pfn_t pfn;
        int last_slot;
        int slot;
        int slot_psize;
        
        cpfn_low = slot_getbasepfn(node, 0);
	last_slot = node_getlastslot(node);
        for (slot = 0; slot <= last_slot; slot++) {
		slot_psize = slot_getsize(node, slot);
                cpfn_high = cpfn_low + slot_psize - 1;
                if (cpfn_low <= cpfn && cpfn <= cpfn_high) {
                        pfn = slot_getbasepfn(node, slot) + cpfn - cpfn_low;
                        return (pfn);
                }
                cpfn_low += slot_psize;
        }

        return 0;
}

/*
 * pfns in a node after PFD_LOW
 */
uint
pfn_getnumber(cnodeid_t node)
{
        uint number;
        int last_slot;
        int slot;
        
        number = slot_getbasepfn(node, 0) +
                slot_getsize(node, 0) -
                pfdattopfn(PFD_LOW(node));
        
        last_slot = node_getlastslot(node);
        for (slot = 1; slot <= last_slot; slot++) {
                number += slot_getsize(node, slot);
        }

        return (number);
}

#else
/*ARGSUSED*/
pfn_t
cpfn_to_pfn(cnodeid_t node, pfn_t cpfn)
{
        return (cpfn);
}

/*ARGSUSED*/
uint
pfn_getnumber(cnodeid_t node)
{
        return (NODE_TOTALMEM(node));
}


#endif        





long _enable_rawcnt_panic = 0;

/*
 * Put page on appropriate free page queue.
 * The page is of size (size) and the only flag says if the coalescing is to 
 * be tried on this page.
 * Returns 0 - if page not really freed
 *	   1 if page freed (use count went to zero)
 */
int
_pagefree_size(register pfd_t *pfd, size_t size, uint flags)
{
	register phead_t	*pheadp;
	register plist_t	*plistp;
	register int		pfflags;
	pg_free_t		*pfl;
	pgszindx_t		page_size_index;
	cnodeid_t		node = pfdattocnode(pfd);
        register int            list_type;
	extern			int	large_pages_not_needed;

	/*
	 * The following asserts that all the large pages that are freed
	 * have no association.
	 */

	ASSERT(!(pfd->pf_flags & P_HASH) || (size == NBPP));

	nested_pfdat_lock(pfd);

	if (pfd->pf_use == 0) {
		cmn_err_tag(124,CE_PANIC, "pagefree: page use %d (0x%x)",
			pfd->pf_use, pfd);
		/* NOTREACHED */
	}

	if (--pfd->pf_use > 0) {
		nested_pfdat_unlock(pfd);
		return(0);
	}

	if (pfd->pf_flags & P_ANON) {
		ASSERT(pfd->pf_swphdl == NULL);
	}

	pfl = GET_NODE_PFL(node);
	page_size_index = PAGE_SIZE_TO_INDEX(size);
	if (pfd < pfl->pfd_low || pfd > pfl->pfd_high)
		cmn_err_tag(125,CE_PANIC, "pagefree: invalid pfd %x", pfd);
	pfflags = pfd->pf_flags;
	pheadp = PFD_TO_PHEAD(pfd, page_size_index);
	pheadp->ph_count++;

#ifdef TILES_TO_LPAGES
	if (page_size_index == MIN_PGSZ_INDEX)
		TILE_TPHEAD_INC_PAGES(pfd);
#endif /* TILES_TO_LPAGES */
#ifdef MH_R10000_SPECULATION_WAR
	if (pfd < pfd_lowmem) {
		smallk0_freemem++;
		flags |= PAGE_FREE_NOCOALESCE;
	}
#endif /* MH_R10000_SPECULATION_WAR */

	if ( size > NBPP) pfflags &= ~P_HASH;

#ifdef PH_LONG
        list_type = (pfflags & P_HASH) ? STALE_ASSOC : STALE_NOASSOC;
#else        
#if defined(NUMA_BASE)
        if (pfflags & P_POISONED) {
                ASSERT(!(pfflags & P_HASH));
                list_type = POISONOUS;
		pheadp->ph_poison_count++;
        } else
#endif
	if (pfflags & P_HASH) {
                list_type = CLEAN_ASSOC;
        } else {
                list_type = CLEAN_NOASSOC;
        }
#endif        
	plistp = &pheadp->ph_list[list_type];
	append(plistp, pfd);

	/*
	 * If the page is not cached or poisoned than set the bit in
	 * light bitmap. Any page that is on the freelist will be be set in
	 * the heavy bitmap.
	 */

	if ( !(pfflags & (P_HASH | P_POISONED | P_ERROR)))
		set_pfd_bitfield(pfdat_to_pure_bm(pfd), pfd, btoc(size));
	set_pfd_bitfield(pfdat_to_tainted_bm(pfd), pfd, btoc(size));

	if (_enable_rawcnt_panic && (pfd->pf_rawcnt != 0)) {
		cmn_err_tag(126,CE_PANIC, "_pagefree_size: rawcnt != 0");
	}
	ASSERT(pfd->pf_rawcnt == 0);
	ASSERT(pfd->pf_hchain == NULL || (pfd->pf_flags & P_HASH));
	ASSERT((pfd->pf_flags & P_DQUEUE) == 0);

	pfflags &= ~(P_DIRTY | P_DUMP | P_BULKDATA);
	pfflags |= P_QUEUE;

	pfd->pf_flags = pfflags;

	ADD_NODE_FREEMEM(node, btoc(size));
	if ((pfd->pf_flags & P_HASH) == 0) {
                ADD_NODE_EMPTYMEM(node, btoc(size));
        }
	INC_NUM_FREE_PAGES(node, page_size_index); 
	if (size > NBPP)
		lpage_init_pfdat(pfd, size, PAGE_FREE);

	TILE_PAGEUNLOCK(pfd, btoc(size));

	ASSERT(GET_RMAP_PDEP1(pfd) == 0);
	ASSERT(pfd->pf_pdep2 == 0);
	ASSERT(pfd->pf_rmapp == 0);

	nested_pfdat_unlock(pfd);

	ASSERT(check_freemem_node(node));

	if ((!large_pages_not_needed) && !(flags & PAGE_FREE_NOCOALESCE)) 
				lpage_coalesce(node, pfd);

	return(1);
}

int
pagefree_size(register pfd_t *pfd, size_t size)
{
	register int	locktoken;
	register int    rv;

	if ((pfd->pf_flags & (P_HASH|P_ANON)) == P_HASH)
		KTRACE_ENTER(vnode_ktrace, VNODE_PAGEFREE, pfd->pf_tag, pfd, pfd->pf_use);

	locktoken = PAGE_FREELIST_LOCK(pfdattocnode(pfd));
	rv = _pagefree_size(pfd, size, 0);
	PAGE_FREELIST_UNLOCK(pfdattocnode(pfd), locktoken);

	return(rv);
}

/*
 * Free a page and try to re-allocate it for a new purpose.  If the
 * page is still in use by others, then we can't re-allocate it.
 * Otherwise unhash it so the caller can use it for a new purpose.
 * Return 1 if page was successfully reallocated and 0 if page still
 * in use.
 * Only called from vhand() at this time to reclaim a page for the page
 * swap handle cache. 
 */

int
pagerealloc(register pfd_t *pfd)
{
	register int	locktoken;
	void		*tag;

	ASSERT((pfd->pf_flags & (P_HASH|P_DONE|P_QUEUE|P_SQUEUE|P_RECYCLE)) ==
				(P_HASH|P_DONE));

	locktoken = pfdat_lock(pfd);
	if (pfd->pf_use > 1) {
		/*
		 * page is still in use by others.
		 * Drop the extra reference taken by the caller, and
		 * return failure.
		 */
		pfdat_unlock(pfd, locktoken);
		pagefree(pfd);
		return 0;
	}

	ASSERT(pfd->pf_use == 1);
	/*
	 * We are the sole owners of the page. 
	 * So, free it by making pf_use to be zero, get the
	 * page recycled, and return the page to caller.
	 */ 
	pfd->pf_flags |= P_RECYCLE;
	pfd->pf_use    = 0;
	tag  	       = pfd->pf_tag;

	if (pfd->pf_flags & P_ANON) {
		anon_hold((anon_hdl)tag);
		pfdat_unlock(pfd, locktoken);
		anon_recycle((anon_hdl)tag, pfd);
	} else {
		VNODE_PCACHE_INCREF((vnode_t *)tag);
		pfdat_unlock(pfd, locktoken);
		vnode_page_recycle((vnode_t *)tag, pfd);
	}

	/*
	 * Check if this page was marked as having an error. If so,
	 * call the error handling code to clean this page for us,
	 * If the page is not cleanable, add it to a discarded list
	 * of pages.
	 */
	if (pfdat_iserror(pfd)) {
		if (page_error_clean(pfd) == 0) {
			page_discard_enqueue(pfd);
			return 0;
		}
	}

	/*
	 * We are the sole users of the pfdat at this time.
	 * So it's okay to go and mess with the fields of pfdat
	 * without holding the lock.
	 */

	ASSERT(!(pfd->pf_flags & (P_HASH|P_ANON|P_QUEUE|P_SQUEUE)));

	pfd->pf_flags = P_CACHESTALE;
	pfd->pf_pageno = PGNULL;
	pfd->pf_vp = 0;
	pfd->pf_use = 1;
	pfd->pf_rawcnt = 0;

	return 1;

}

#ifdef PH_LONG
void
phead_join(register phead_t *pheadp)
{
	register plist_t *plistbp, *plistfp;

	ASSERT(pheadp->ph_flushcount >= 0);
	pheadp->ph_flushcount++;

	plistbp = &pheadp->ph_list[STALE_ASSOC];
	if (!isempty(plistbp)) {
		plistfp = &pheadp->ph_list[INTRANS_ASSOC];
		combine(plistfp, plistbp);
		makeempty(plistbp);
	}
	plistbp = &pheadp->ph_list[STALE_NOASSOC];
	if (!isempty(plistbp)) {
		plistfp = &pheadp->ph_list[INTRANS_NOASSOC];
		combine(plistfp, plistbp);
		makeempty(plistbp);
	}
}

#ifdef _VCE_AVOIDANCE
int
reset_color_lists(
	register phead_t *headp,
	register plist_t *newlp,
	register plist_t *pfdlp,
	register int locktoken)
{
	plist_t	tlist;
	int	tcount;
#ifdef MH_R10000_SPECULATION_WAR
        int     smallk0_tcount;
#endif /* MH_R10000_SPECULATION_WAR */
	pfd_t	*pfd;
#ifdef TILES_TO_LPAGES
	tphead_t *tpheadp = pfdattotphead(pfdlp->pf_next);
#endif /* TILES_TO_LPAGES */

	makeempty(&tlist);
	combine(&tlist,(pfdlp));
	makeempty((pfdlp));

	for (tcount = 0, pfd = tlist.pf_next;
	     pfd != ((pfd_t *) &tlist);
	     tcount++, pfd = pfd->pf_next) {
#ifdef MH_R10000_SPECULATION_WAR
                if (pfd < pfd_lowmem)
                        smallk0_tcount++;
#endif /* MH_R10000_SPECULATION_WAR */
		 ;
	}
	SUB_NODE_FREEMEM(private.p_nodeid, tcount);
	SUB_NODE_FREE_PAGES(private.p_nodeid, MIN_PGSZ_INDEX, tcount);
	headp->ph_count -= tcount;
#ifdef MH_R10000_SPECULATION_WAR
        smallk0_freemem -= smallk0_tcount;
#endif /* MH_R10000_SPECULATION_WAR */
#ifdef TILES_TO_LPAGES
	tpheadp->count -= tcount;
#endif /* TILES_TO_LPAGES */
		
	/* PAGE_FREELIST_UNLOCK(private.p_nodeid, locktoken); */

	for (pfd = pfdlp->pf_next; 
	     pfd != (pfd_t *) pfdlp;
	     pfd = pfd->pf_next) {
		if (vce_avoidance)
		    pfd_set_vcolor(pfd,-1);
		pfd->pf_flags &= ~P_MULTI_COLOR;
	}

	/* locktoken = PAGE_FREELIST_LOCK(private.p_nodeid); */
	combine((newlp),&tlist);
	headp->ph_count += tcount;
	ADD_NODE_FREEMEM(private.p_nodeid, tcount);
	ADD_NODE_FREE_PAGES(private.p_nodeid, MIN_PGSZ_INDEX, tcount);
#ifdef MH_R10000_SPECULATION_WAR
        smallk0_freemem += smallk0_tcount;
#endif /* MH_R10000_SPECULATION_WAR */
#ifdef TILES_TO_LPAGES
	tpheadp->count += tcount;
#endif /* TILES_TO_LPAGES */

	return locktoken;
}
#endif

int
phead_unjoin(register phead_t *pheadp, int locktoken)
{
	register plist_t *plistbp, *plistfp;

	/*
	 * If another process is pushing pfdats onto the
	 * intrans list, its corresponding cache_flush may
	 * not have happened yet.  Let last process clean up.
	 */
	ASSERT(pheadp->ph_flushcount > 0);
	if (--pheadp->ph_flushcount > 0)
		return locktoken;

	plistbp = &pheadp->ph_list[INTRANS_ASSOC];
	if (!isempty(plistbp)) {
		plistfp = &pheadp->ph_list[CLEAN_ASSOC];
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance)
			locktoken =
			reset_color_lists(pheadp, plistfp, plistbp, locktoken);
		else 
#endif
		{
			combine(plistfp, plistbp);
			makeempty(plistbp);
		}
	}

	plistbp = &pheadp->ph_list[INTRANS_NOASSOC];
	if (!isempty(plistbp)) {
		plistfp = &pheadp->ph_list[CLEAN_NOASSOC];
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance)
			locktoken =
			reset_color_lists(pheadp, plistfp, plistbp, locktoken);
		else 
#endif
		{
			combine(plistfp, plistbp);
			makeempty(plistbp);
		}
	}

	return locktoken;
}

/*
 * Flush the instruction caches of all of the processors and move the contents
 * of the "stale" page lists to the "clean" page lists.
 */
static void
flush_pheads(register pfd_t *pfd, register int npgs)
{
#ifdef	R4600
	extern int two_set_pcaches;
#endif	/* R4000PC */
	int	pheadmask = PHEADMASK(MIN_PGSZ_INDEX);

	ASSERT(npgs > 0);
	/*
	 * Systems with very small caches should flush everythings.
	 * Ditto for systems that can flush their entire caches faster
	 * than they can flush specific lines (this includes R4600SC).
	 */
	if (
#ifdef	R4000PC
	    private.p_scachesize == 0 ||
#ifdef	R4600
	    two_set_pcaches ||
#endif
#endif	/* R4000PC */
	    (npgs >= pheadmask / 2))
	{
		flushcaches();
		return;
	}

	while (--npgs >= 0) {
		flush_phead(PFD_TO_PHEAD(pfd, MIN_PGSZ_INDEX), MIN_PGSZ_INDEX);
		pfd++;
	}
}

static void
flush_phead(register phead_t *pheadp, pgszindx_t page_size_index)
{
	register int	locktoken;
	pg_free_t	*pfl = GET_NODE_PFL(0);

	locktoken = PAGE_FREELIST_LOCK(private.p_nodeid);
	phead_join(pheadp);
	PAGE_FREELIST_UNLOCK(private.p_nodeid, locktoken);

	/*
	 * Must pass a physical address -- otherwise _bclean_caches will
	 * only flush the specific cache lines represented by the pfn argument.
	 */
	_bclean_caches((void *)PHYS_TO_K0(ctob(pheadp-pfl->phead[page_size_index])),
		PGSZ_INDEX_TO_SIZE(page_size_index), 0, _B_ARGS);

	locktoken = PAGE_FREELIST_LOCK(private.p_nodeid);
	locktoken = phead_unjoin(pheadp, locktoken);
	PAGE_FREELIST_UNLOCK(private.p_nodeid, locktoken);
}

/*
 * Flush the instruction caches of all of the processors and move the contents
 * of the "stale" page lists to the "clean" page lists.  Because we don't want 
 * to hold mem_lock for too long (even if we periodically drop it and
 * reacquire it again), we only scan a max number of page colors, probably
 * just a subset of the whole.
 */
static void
flushcaches(void)
{
	register phead_t *pheadp;
	register int	locktoken;
	register int	mem_islocked = 0;
	register int	j;
	register pg_free_t	*pfl = GET_NODE_PFL(0);
	int	pheadmask = PHEADMASK(MIN_PGSZ_INDEX);

#define NCOMBINES	32

#if NUMA_BASE
<<bomb>>  This code will need work if we ever build a NUMA machine
	  without coherent DMA
#endif

	/*
	 * Combine lists in stages so that pages that get put
	 * on the STALE lists \\after// the cache flushes won't
	 * get promoted to CLEAN lists.
	 */
	j = pheadmask + 1;
#ifdef TILES_TO_LPAGES
	j *= NTPHEADS;
#endif /* TILES_TO_LPAGES */
	for (pheadp = &pfl->phead[MIN_PGSZ_INDEX][0]; --j >= 0; pheadp++) {
		if (!mem_islocked) {
			mem_islocked = NCOMBINES;
			locktoken =  PAGE_FREELIST_LOCK(private.p_nodeid);
		}

		phead_join(pheadp);

		/*
		 * If we've held mem_lock a while, let it go.
		 * It's perfectly safe to release mem_lock,
		 * 'cause any process in pagealloc that finds a
		 * page on the in-transit list will just flush
		 * that page's cache lines and return.
		 */
		if (--mem_islocked == 0)
			PAGE_FREELIST_UNLOCK(private.p_nodeid, locktoken);
	}

	if (mem_islocked)
		PAGE_FREELIST_UNLOCK(private.p_nodeid, locktoken);

	/*
	 * We get called on large mem systems from the code in kern_heap
	 * during boot, which is before bootime gets set.  At no other
	 * time should we be here while on the interrupt stack.
	 */
	ASSERT(private.p_kstackflag != PDA_CURINTSTK || !boottime.tv_sec);

	/* flush both i- and d- caches */
	flush_cache();

	locktoken =  PAGE_FREELIST_LOCK(private.p_nodeid);
	mem_islocked = NCOMBINES;

	j = pheadmask + 1;
#ifdef TILES_TO_LPAGES
	j *= NTPHEADS;
#endif /* TILES_TO_LPAGES */
	for (pheadp = &pfl->phead[MIN_PGSZ_INDEX][0]; --j >= 0; pheadp++) {
		if (!mem_islocked) {
			mem_islocked = NCOMBINES;
			locktoken =  PAGE_FREELIST_LOCK(private.p_nodeid);
		}

		locktoken = phead_unjoin(pheadp, locktoken);

		if (--mem_islocked == 0)
			PAGE_FREELIST_UNLOCK(private.p_nodeid, locktoken);
	}

	if (mem_islocked)
		PAGE_FREELIST_UNLOCK(private.p_nodeid, locktoken);

	MINFO.iclean++;
}
#endif /* PH_LONG */

/* page_mapin makes certain that the page is addressable either through K0SEG
 * or K2SEG.  If we need to perform a mapping, vaddr is used in order to
 * obtain a K2 address with the desired color.
 *
 * flags
 *	VM_VACOLOR	Page must be of specified color
 *	VM_UNCACHED	Page must be mapped uncached
 */
/* ARGSUSED */
caddr_t
page_mapin_pfn(register pfd_t *pfd, register int flags, register int color, uint pfn)
{
	register caddr_t	vaddr;
	register pgi_t		state;
#ifdef _VCE_AVOIDANCE
	int			current_color;
#endif
	
#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
		if (pfd == NULL)
			current_color = vcache_color(pfn);
		else
			current_color = pfn_to_vcolor(pfn);

		if (flags & VM_OTHER_VACOLOR) {
			flags |= VM_VACOLOR;
			current_color = -1;
		} else if (! (flags & VM_VACOLOR) &&
			 ! (flags & VM_UNCACHED) &&
			 current_color != -1) {
			flags |= VM_VACOLOR;
			color = current_color;
		} else if (flags & VM_VACOLOR) {
			if (color == -1)
				flags &= ~VM_VACOLOR;
			else if (color != current_color &&
				 current_color != -1) {
				cmn_err(CE_WARN,
	"page_mapin: color mismatch for page 0x%x (want %d, have %d)",
					pfn,
					color,
					current_color);
			}
		};
		if (current_color == -1) {
			if (pfd == NULL)
				current_color = vcache_color(pfn);
			else if (! (flags & (VM_UNCACHED|VM_VACOLOR|VM_OTHER_VACOLOR)) &&
				 pfd->pf_flags & P_HASH &&
				 ! (pfd->pf_flags & P_ANON) &&
				 pfd->pf_vp != NULL) {
				color = vcache_color(pfd->pf_pageno);
				flags |= VM_VACOLOR;
			}
		}
	}
#endif

	/* Return a small memory addrees (i.e. K0 address) when you can.
	 * We will also perform a K2 map if the caller specified VM_VACOLOR
	 * and the K0 address has the incorrect color.
	 */

#ifndef DEBUG_K2_ADDRS
	if (flags & (VM_UNCACHED|VM_UNCACHED_PIO)) {
		if (small_K1_pfn(pfn)) {
			vaddr = vfntova(small_pfntovfn_K1(pfn));

			XKPHYS_UNCACHADDR(vaddr, flags);

			return(vaddr);
		}
	} else {
		if (small_K0_pfn(pfn) &&
		   ((!(flags & VM_VACOLOR)) || (
#ifdef _VCE_AVOIDANCE
				(! vce_avoidance ||
				 vcache_color(pfn) == current_color ||
				 current_color == -1) &&
#endif /* _VCE_AVOIDANCE */
				vcache_color(pfn) == color))) {
			vaddr = vfntova(small_pfntovfn_K0(pfn));
#ifdef _VCE_AVOIDANCE
			if (vce_avoidance &&
			    pfd != NULL &&
			    ! (flags & VM_OTHER_VACOLOR))
				pfd_set_vcolor(pfd,vcache_color(pfn));
#endif /* _VCE_AVOIDANCE */
			return(vaddr);
		}
	}
#endif /* !DEBUG_K2_ADDRRS */

	/* Allocate new K2 address, setup pde, and return vaddr to caller */

	vaddr = kvalloc(1, flags & (VM_VACOLOR|VM_UNCACHED|VM_UNCACHED_PIO|VM_NOSLEEP), color);
	if (vaddr == 0)
		return(NULL);

	/* Fast path for regular cached mapping */
	if (!(flags & (VM_UNCACHED|VM_UNCACHED_PIO))) {
		state = (pgi_t)(PG_VR | PG_G | PG_M | PG_SV |pte_cachebits());
#ifdef MH_R10000_SPECULATION_WAR
                if (IS_R10000() &&
                    (flags & VM_DMA_RW))
                        state &= ~PG_VR;
#endif /* MH_R10000_SPECULATION_WAR */

	} else {
		if (flags & VM_UNCACHED)
			state = pte_noncached(PG_VR | PG_G | PG_M | PG_SV);

		else if (flags & VM_UNCACHED_PIO)
			state = pte_uncachephys(PG_VR | PG_G | PG_M | PG_SV);
	
		PTE_SETUNCACH_ATTR(state, flags);
	}

	pg_setpgi(kvtokptbl(vaddr), mkpde(state, pfn));
#ifdef MH_R10000_SPECULATION_WAR
        if (pfd != NULL &&
            ! pg_isnoncache(kvtokptbl(vaddr)) &&
            IS_R10000())
                krpf_add_reference(pfd,
                                   vatovfn(vaddr));
#endif /* MH_R10000_SPECULATION_WAR */

#ifdef _VCE_AVOIDANCE
	if (vce_avoidance &&
	    pfd != NULL &&
	    ! (flags & (VM_UNCACHED | VM_OTHER_VACOLOR)))
		pfd_set_vcolor(pfd,colorof(vaddr));
#endif
	return(vaddr);
}

caddr_t
page_mapin(register pfd_t *pfd, register int flags, register int color)
{
    return(page_mapin_pfn(pfd, flags, color, pfdattopfn(pfd)));
}

void
page_mapout(register caddr_t vaddr)
{
	if (IS_KSEG2(vaddr)) {
		kvfree(vaddr, 1);
	}
}


#ifdef SN0
/*
 * COW support.  Copies user page from source pfd to destination pfd.
 *
 * Use pages of appropriate color on both source and destination.
 */
/*ARGSUSED*/
void
page_copy(pfd_t *spfd, pfd_t *dpfd, int scolor, int dcolor)
{
	ASSERT((spfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);
	ASSERT((dpfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);

#if SN0_USE_BTE
	/*
	 * For now, just wait for the transfer to finish.
	 * We may want to do it asynchronously in the future.
	 * What about cache coloring?
	 */
	bte_pbcopy(pfdattophys(spfd), pfdattophys(dpfd), NBPP);
#else
	/* We have no issues with page coloring or VCEs since we use 16K pages.
	 */
	bcopy((const void*)PHYS_TO_K0(pfdattophys(spfd)), 
	      (void*)PHYS_TO_K0(pfdattophys(dpfd)), NBPP);
#endif /* SN0_USE_BTE */
}


/*
 * page_zero is used by vfault to zero a physical page or portion of page.
 * Use pages of appropriate color on destination.
 */
/*ARGSUSED*/
void
page_zero(pfd_t *dpfd, int dcolor, uint pgoffset, uint cnt)
{
#if SN0_USE_BTE
	caddr_t dest;
	uint cnt1;
	extern	int	origin_use_hardware_bzero;
#endif

	ASSERT((dpfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);
	ASSERT( (pgoffset+cnt) <= NBPP);

#if SN0_USE_BTE

	if (!origin_use_hardware_bzero) {
		dest = vfntova(small_pfntovfn_K0(pfdattopfn(dpfd))) + pgoffset;
		bzero(dest, cnt);
		return;
	}

	/*
	 * For now, just wait for the transfer to finish.
	 * We may want to do it asynchronously in the future.
	 * What about cache coloring?
	 *
	 * Clear up to next cache line first. We do it inline
	 * since calling bcopy for such a small size (less than
	 * a cache line) is too expensive.
	 */
	dest = 0;
	if (pgoffset & BTE_LEN_ALIGN) {
		dest = vfntova(small_pfntovfn_K0(pfdattopfn(dpfd))) + pgoffset;
		while ((__psint_t)dest & BTE_LEN_ALIGN) {
			*dest++ = 0;
			pgoffset++;
			cnt--;
		}
	}

	cnt1 = cnt & ~BTE_LEN_ALIGN;	/* now copy the aligned part */
	if (cnt1 >= BTE_LEN_MINSIZE) {	/* only use bte if big enough */
		bte_pbzero(pfdattophys(dpfd) + pgoffset, cnt1, 0);
		cnt -= cnt1;
	}
	else
		cnt1 = 0;

	if (cnt) {			/* now finish up the last chunk */
		if (dest == 0)
			dest = vfntova(small_pfntovfn_K0(pfdattopfn(dpfd))) + pgoffset;
		bzero(dest + cnt1, cnt);
	}
#else
	bzero( (void *)PHYS_TO_K0(pfdattophys(dpfd)+pgoffset), cnt );
#endif /* SN0_USE_BTE */
}

/*
 * page_zero_nofault is used by vfault to zero a physical page or portion of 
 * page and attempt to recover from double bit errors.
 * Use pages of appropriate color on destination.
 */
/*ARGSUSED*/
void
page_zero_nofault(pfd_t *dpfd, int dcolor, uint pgoffset, uint cnt)
{
#if SN0_USE_BTE
	caddr_t dest;
	uint cnt1;
	extern	int	origin_use_hardware_bzero;
#endif
	int nofault_save;

	ASSERT((dpfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);
	ASSERT( (pgoffset+cnt) <= NBPP);

#if SN0_USE_BTE

	if (!origin_use_hardware_bzero) {
		dest = vfntova(small_pfntovfn_K0(pfdattopfn(dpfd))) + pgoffset;
		if (curthreadp) {
			nofault_save = curthreadp->k_nofault;
			curthreadp->k_nofault = NF_BZERO;
		}
		bzero(dest, cnt);
		if (curthreadp) {
			curthreadp->k_nofault = nofault_save;
		}
		return;
	}

	/*
	 * For now, just wait for the transfer to finish.
	 * We may want to do it asynchronously in the future.
	 * What about cache coloring?
	 *
	 * Clear up to next cache line first. We do it inline
	 * since calling bcopy for such a small size (less than
	 * a cache line) is too expensive.
	 */
	dest = 0;
	if (pgoffset & BTE_LEN_ALIGN) {
		dest = vfntova(small_pfntovfn_K0(pfdattopfn(dpfd))) + pgoffset;
		while ((__psint_t)dest & BTE_LEN_ALIGN) {
			*dest++ = 0;
			pgoffset++;
			cnt--;
		}
	}

	cnt1 = cnt & ~BTE_LEN_ALIGN;	/* now copy the aligned part */
	if (cnt1 >= BTE_LEN_MINSIZE) {	/* only use bte if big enough */
		bte_pbzero(pfdattophys(dpfd) + pgoffset, cnt1, 1);
		cnt -= cnt1;
	}
	else
		cnt1 = 0;

	if (cnt) {			/* now finish up the last chunk */
		if (dest == 0)
			dest = vfntova(small_pfntovfn_K0(pfdattopfn(dpfd))) + pgoffset;
		if (curthreadp) {
			nofault_save = curthreadp->k_nofault;
			curthreadp->k_nofault = NF_BZERO;
		}
		bzero(dest + cnt1, cnt);
		if (curthreadp) {
			curthreadp->k_nofault = nofault_save;
		}
	}
#else
	if (curthreadp) {
		nofault_save = curthreadp->k_nofault;
		curthreadp->k_nofault = NF_BZERO;
	}
	bzero( (void *)PHYS_TO_K0(pfdattophys(dpfd)+pgoffset), cnt );
	if (curthreadp) {
		curthreadp->k_nofault = nofault_save;
	}
#endif /* SN0_USE_BTE */
}
#else /* !SN0 */
/*
 * The following pagelists are intended to be reserved by the kernel to bcopy
 * or bzero pages in the various fault handlers (pfault, tfault, etc).
 * We need to map pages for one of two reasons:
 *	(1) To avoid VCEs
 *	(2) To access pages above 256 MB in large memory configs
 * We do not expect to context switch while in routines which use these pages
 * nor do we expect to enter them recursively.
 */
#define MAX_VPGLIST 2	 /* number of p_pcopy_pagelists in pda */
#define SVPGLIST 0
#define DVPGLIST 1

static caddr_t
pfnattach(
	register uint color,
	register int pfn,
	register int plistnum)
{
	register char *kvaddr;

	if (!private.p_pcopy_pagelist[0]) {
		int i;

		for (i=0; i<MAX_VPGLIST; i++) {
			private.p_pcopy_pagelist[i] =
				 kvalloc(cachecolormask+1, VM_VACOLOR, 0);
			ASSERT(private.p_pcopy_pagelist[i] != NULL);
			private.p_pcopy_inuse[i] = NOCOLOR;
		 }
	}
#ifdef _VCE_AVOIDANCE
	/*
	 * We need to choose a color at which to map this page.
	 * We'll use the caller's suggested color if we can,
	 * but we'll prefer to use the color already established
	 * for this page, if one exists.
	 *
	 * If the page does not yet have a color then
	 *     If the caller passed us a color
	 *         use the color pssed in by the caller
	 *     else if the page is a mapped file
	 *         use the color that matches the color
	 *         of the file offset
	 *     else
	 *         use the physical color
	 * else
	 *     use the curent color of the page
	 */
	if (vce_avoidance) {
		pfd_t *pfd;
		int	current_color;

		pfd = pfntopfdat(pfn);
		current_color = pfd_to_vcolor(pfd);
		if (current_color == -1) {
			if (color != NOCOLOR)
				current_color = color;
			else if (pfd->pf_flags & P_HASH &&
				 ! (pfd->pf_flags & P_ANON) &&
				 pfd->pf_vp != NULL) {
				current_color = vcache_color(pfd->pf_pageno);
			} else
				current_color = vcache_color(pfn);
			pfd_set_vcolor(pfd,current_color);
		}
		color = current_color;
	} else
#endif
	if (color == NOCOLOR)
		color = 0;	/* Use arbitrary color */

	ASSERT(color <= cachecolormask);
	ASSERT(plistnum < MAX_VPGLIST);
	ASSERT(private.p_pcopy_inuse[plistnum] == NOCOLOR);

	private.p_pcopy_inuse[plistnum] = color;

	kvaddr =  private.p_pcopy_pagelist[plistnum] + NBPP * color;

	pg_setpgi(kvtokptbl(kvaddr),
		  mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn));

	return kvaddr;
}

static void
pfnfree(register int plistnum)
{
	register char *kvaddr;
	register int color;

	ASSERT(plistnum < MAX_VPGLIST);
	ASSERT(private.p_pcopy_inuse[plistnum] != NOCOLOR);

	color = private.p_pcopy_inuse[plistnum];
	kvaddr =  private.p_pcopy_pagelist[plistnum] + NBPP * color;
	unmaptlb(
#ifdef TILES_TO_LPAGES
		((curuthread == NULL) ? 0 : curuthread->ut_as.utas_tlbpid),
#else
		curuthread->ut_as.utas_tlbpid,
#endif /* TILES_TO_LPAGES */
						btoct(kvaddr));
	pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0)); /* safety net */

	private.p_pcopy_inuse[plistnum] = NOCOLOR;
}
/*
 * COW support.  Copies user page from source pfd to destination pfd.
 *
 * Use pages of appropriate color on both source and destination.
 */
/*ARGSUSED*/
void
page_copy(pfd_t *spfd, pfd_t *dpfd, int scolor, int dcolor)
{
	caddr_t src, dest;

#ifndef MH_R10000_SPECULATION_WAR
	ASSERT((spfd->pf_flags & (P_QUEUE | P_SQUEUE)) == 0);
#else
	/* page_dup() may copy P_SQUEUE pages */
	ASSERT((spfd->pf_flags & (P_QUEUE)) == 0);
#endif /* MH_R10000_SPECULATION_WAR */
	ASSERT((dpfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);

#ifndef DEBUG_K2_ADDRS
	if ((pfd_direct == PFD_HIGH(numnodes -1)) &&
#ifdef _VCE_AVOIDANCE
	    (! vce_avoidance) &&
#endif
#ifdef MH_R10000_SPECULATION_WAR
            (spfd <= pfdat + SMALLMEM_K0_PFNMAX) &&
            (dpfd <= pfdat + SMALLMEM_K0_PFNMAX) &&
#endif /* MH_R10000_SPECULATION_WAR */
	    ((cachecolormask == 0)||((scolor == NOCOLOR) && (dcolor == NOCOLOR)))) {
		/* Small memory config (< 256 MB) and no need for
		 * cache coloring means we can use K0 addresses.
		 */
		src = vfntova(small_pfntovfn_K0(pfdattopfn(spfd)));
		dest = vfntova(small_pfntovfn_K0(pfdattopfn(dpfd)));
		bcopy( src, dest, NBPP);
	} else
#endif /* !DEBUG_K2_ADDRS */
	{
		/* protect use of private.p_pcopy_pagelist and
		 * private.p_pcopy_inuse (using p_pcopy_lock allows
		 * kpreempt to occur while still protecting the
		 * private.p_pcopy fields).
		 */
		/*REFERENCED*/
		void *old_val = rt_pin_thread();
		mutex_lock(&pdaindr[cpuid()].pda->p_pcopy_lock, PZERO);

		src = pfnattach(scolor, pfdattopfn(spfd), SVPGLIST);
		dest = pfnattach(dcolor, pfdattopfn(dpfd), DVPGLIST);
		bcopy( src, dest, NBPP);
		pfnfree(SVPGLIST);
		pfnfree(DVPGLIST);

		mutex_unlock(&pdaindr[cpuid()].pda->p_pcopy_lock);
		rt_unpin_thread(old_val);
	}
}


/*
 * page_zero is used by vfault to zero a physical page or portion of page.
 * Use pages of appropriate color on destination.
 */
void
page_zero(pfd_t *dpfd, int dcolor, uint pgoffset, uint cnt)
{
	caddr_t dest;

	ASSERT((dpfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);
	ASSERT( (pgoffset+cnt) <= NBPP);

#ifndef DEBUG_K2_ADDRS
	if ((pfd_direct == PFD_HIGH(numnodes-1)) &&
#ifdef _VCE_AVOIDANCE
	    (! vce_avoidance) &&
#endif
#ifdef MH_R10000_SPECULATION_WAR
            (dpfd <= pfdat + SMALLMEM_K0_PFNMAX) &&
#endif /* MH_R10000_SPECULATION_WAR */
	    ((cachecolormask == 0)||(dcolor == NOCOLOR))) {
		/* Small memory config (< 256 MB) and no need for
		 * cache coloring means we can use K0 addresses.
		 */
		dest = vfntova(small_pfntovfn_K0(pfdattopfn(dpfd)));
		bzero( dest+pgoffset, cnt);
	} else
#endif /* !DEBUG_K2_ADDRS */
	{
		/* protect use of private.p_pcopy_pagelist and
		 * private.p_pcopy_inuse (using p_pcopy_lock allows
		 * kpreempt to occur while still protecting the
		 * private.p_pcopy fields).
		 */
		/*REFERENCED*/
		void *old_val = rt_pin_thread();
		mutex_lock(&pdaindr[cpuid()].pda->p_pcopy_lock, PZERO);

		dest = pfnattach(dcolor, pfdattopfn(dpfd), DVPGLIST);
		bzero( dest+pgoffset, cnt);
		pfnfree(DVPGLIST);

		mutex_unlock(&pdaindr[cpuid()].pda->p_pcopy_lock);
		rt_unpin_thread(old_val);
	}
}
#endif /* !SN0 */


/* 
 * The following routine is called from the vfntopfn macro when the
 * vfn is an invalid value.  For now we panic, though the macro believes
 * we return a value.
 */
int
bad_vfn(pgno_t x)
{
	cmn_err(CE_PANIC, "Invalid value for vfn: 0x%x\n", x);
	/* NOTREACHED */
}

/*
 * Obtain pdat from pte locking the pfn to make sure
 * migration is not changing it while we make the translation,
 * and freezing the pfdat, to make sure the page will not migrate
 * while we have a temporary reference in the code.
 */
 
pfd_t*
pdetopfdat_hold(pde_t* pd)
{
        pfd_t* pfd;
	int	s;
        
	/* Peek in order to avoid locking */
	if (!pg_isvalid(pd))
		return NULL;

        pg_pfnacquire(pd);
	if (!pg_isvalid(pd)) {
        	pg_pfnrelease(pd);
		return NULL;
	}

        pfd = pdetopfdat(pd);
        ASSERT (pfd != NULL);
	s = pfdat_lock(pfd);

	ASSERT(pfd->pf_use > 0 );
	ASSERT((pfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);

	pfdat_inc_usecnt(pfd);

	pfdat_unlock(pfd, s);
        pg_pfnrelease(pd);
        return (pfd);
}

/*
 * Handling of poisonous pages
 */

#ifdef NUMA_BASE
void
page_poison(pfd_t *pfd)
{
	/* 
	 * Use BTE to poison the page.
	 */
	ASSERT(pfd->pf_use);
	/*
	 * Set the flags before poisoning. Allows handling
	 * of speculative errors. 
	 */
	pfd_setflags(pfd, P_POISONED);
	BTE_PAGE_POISON(((paddr_t)pfdattopfn(pfd) << PNUMSHFT), NBPP);
}

/*
 * Unpoison pages
 */
void
page_unpoison(pfd_t* pfd)
{
        /*
         * shootdown tlbs
         */
        tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
        
        /*
         * Write to the directory entries doing the unpoisoning
         */
        POISON_STATE_CLEAR(pfdattopfn(pfd));
	pfd_nolock_clrflags(pfd, P_POISONED);
}
              
/*
 * Unpoison phead. To make sure that we don't hold the freelist lock
 * for too long, the poison pages are moved to a local list.
 * We decrement the count to keep all freelist data in sync.
 * We unlock the lock before clearing the poison errors.
 * Clearing the poison errors is a fairly expensive ~ 100 usec.
 * We grab the lock again and merge the pages into the clean list.
 */
void
phead_unpoison(cnodeid_t node, phead_t* pheadp, pfd_t *pfd)
{
        pfd_t* poish;
        pfd_t* cleanh;
        int ospl;
	int numpages, i, s;
	pfd_t	*tmp_pfd;
	plist_t	tmp_list;
	int	num_poisoned_pages;
	pgszindx_t 	page_size_index;

        poish  = (pfd_t*)&pheadp->ph_list[POISONOUS];
        cleanh = (pfd_t*)&pheadp->ph_list[CLEAN_NOASSOC];
	makeempty(&tmp_list);

	ospl =  PAGE_FREELIST_LOCK(node);

	combine(&tmp_list, poish);
	makeempty(poish);

	
	num_poisoned_pages = pheadp->ph_poison_count;
	pheadp->ph_poison_count = 0;
	page_size_index = PFDAT_TO_PGSZ_INDEX(pfd);
	pheadp->ph_count -= num_poisoned_pages;

	SUB_NODE_FREEMEM(node, num_poisoned_pages * 
			PGSZ_INDEX_TO_CLICKS(page_size_index));
	SUB_NODE_FREE_PAGES(node, page_size_index, num_poisoned_pages);
        PAGE_FREELIST_UNLOCK(node, ospl);

#ifdef	DEBUG
    {
	int	tmp_num_poisoned_pages = 0;
	for (tmp_pfd = tmp_list.pf_next;
	     tmp_pfd != ((pfd_t *)&tmp_list); 
	     tmp_pfd = tmp_pfd->pf_next)
		tmp_num_poisoned_pages++;
	if(tmp_num_poisoned_pages != num_poisoned_pages) {
		cmn_err(CE_PANIC,
"tmp_num_poisoned_pages %d num_poisoned_pages %d pheadp 0x%x pfd 0x%x\n", 
		tmp_num_poisoned_pages, num_poisoned_pages, pheadp, pfd);
	}
    }
#endif
	
        /*
         * Shootdown tlbs
         */
        tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
        

	numpages  = PFDAT_TO_PAGE_CLICKS(pfd);

	for (i = 0; i < numpages; i++, pfd++) {
		if (pfd->pf_flags & P_POISONED)
			POISON_STATE_CLEAR(pfdattopfn(pfd));
		pfd_nolock_clrflags(pfd, P_POISONED|P_HAS_POISONED_PAGES);
	}

        for (pfd = tmp_list.pf_next; 
			pfd != (pfd_t *)&tmp_list; pfd = pfd->pf_next) {
                /*
                 * Write to directory to unpoison this page 
                 */
		numpages  = PFDAT_TO_PAGE_CLICKS(pfd);

		tmp_pfd = pfd;
		for (i = 0; i < numpages; i++, tmp_pfd++) {
			if (tmp_pfd->pf_flags & P_POISONED) 
				POISON_STATE_CLEAR(pfdattopfn(tmp_pfd));
			s = pfdat_lock(tmp_pfd);
			pfd_nolock_clrflags(tmp_pfd, 
				P_POISONED|P_HAS_POISONED_PAGES);
			pfdat_unlock(tmp_pfd, s);
		
		}
        }

	ospl =  PAGE_FREELIST_LOCK(node);
	pheadp->ph_count += num_poisoned_pages;
	ADD_NODE_FREEMEM(node, num_poisoned_pages *
                        PGSZ_INDEX_TO_CLICKS(page_size_index));
	ADD_NODE_FREE_PAGES(node, page_size_index, num_poisoned_pages);
        combine(cleanh, &tmp_list);
        PAGE_FREELIST_UNLOCK(node, ospl);
}


/*
 * Check if the VM subsystem has this page marked as poisoned.
 */
int 
page_ispoison(paddr_t paddr)
{
	pfd_t *pfd = pfntopfdat(pnum(paddr));
	/*
	 * A pfdat which does not point to a valid page cannot have been
	 * poisoned by the vm subsystem.
	 */
	if (!page_validate_pfdat(pfd))
	    return 0;
	
	return pfdat_ispoison(pfd);
}
               
#endif /* NUMA_BASE */

#ifdef PGLISTDEBUG
void
trcpglist(char *loc, uint cachekey, uint off, uints uniq, void *vaddr)
{
	uint key = cachekey & pheadmask;
	/*
	 * Should probably be changed to vcache2 but I'm not
	 * sure if this debug code matters anymore. (So I didnt...)
	 */
	ASSERT(cachekey == vcache(uniq, off));
	s5go[key]++;
	if (trcvcache || chkhash == key || chkhash1 == key || chkhash2 == key)
		cmn_err(CE_CONT, "!%s k 0x%x (0x%x + %d) p %d v 0x%x\n",
			loc, key,
			off, uniq, current_pid(), vaddr);
}
#endif

/*
 * Calculation of global free memory
 */

pfn_t
global_freemem_init(void)
{
        cnodeid_t node;
        pfn_t total_memory = 0;
        
        for ( node = 0; node  < numnodes; node++) {
                total_memory += NODE_FREEMEM(node);
        }

        MEMFIT_INIT();
        
        return (total_memory);
}        

pfn_t
global_freemem_calculate(int update)
{
        cnodeid_t node;
        pfn_t total_memory = 0;
        pfn_t max_freemem = 0;
        pfn_t tmp_freemem;
        
        for ( node = 0; node  < numnodes; node++) {
                tmp_freemem =  NODE_FREEMEM(node);
                total_memory += tmp_freemem;
                if (update && tmp_freemem > max_freemem) {
                        max_freemem = tmp_freemem;
                }
        }

	if (!update) {

		/* Don't need update */

		return(total_memory);
	}

        MEMFIT_MASTER_UPDATE(max_freemem);
        
        return (total_memory);
}

/*
 * Calculation of global empty mem
 */

pfn_t
global_emptymem_calculate(void)
{
        cnodeid_t node;
        pfn_t total_emptymem = 0;
        
        for ( node = 0; node  < numnodes; node++) {
                total_emptymem += NODE_EMPTYMEM(node);
        }

        return (total_emptymem);
}


#if KSTACKPOOL

static int	Kstack_pagecnt;
static int	Kstack_basemax;
static int	Kstack_max;
static int	Kstack_failed;
static int	Kstack_timeout;
static pfd_t	*Kstack_pool;
lock_t 		Kstack_pool_lock;
static int	kstackpool_shake(int);

extern int sync_dcaches_excl(void *, unsigned int, unsigned int, unsigned int);

void
kstack_pool_init()
{
	pfd_t 	*pfd = NULL;
	pfd_t 	*npfd;
	int 	i;

	Kstack_max = Kstack_basemax = MIN_KSTACK_PAGES * numcpus;
	for (i = 0; i < Kstack_max; i++) {
		npfd = pagealloc(0,VM_UNSEQ);
		if (npfd == NULL)
			break;
		npfd->pf_next = pfd;
		pfd = npfd;
		/*
		 * NOTE: Rather than invoke _bclean_caches, we call a lower
		 * level function which simply brings the page into our
		 * cache with the EXCLUSIVE attribute using the correct
		 * virtual alias ... this forces any potential VCEs to
		 * occur now and avoids problems in the future.
		 */
		sync_dcaches_excl((void *)(KSTACKPAGE),
			NBPP, pfdattopfn(pfd),
			CACH_NOADDR|CACH_AVOID_VCES);
	}
	Kstack_pagecnt = i;
	Kstack_pool = pfd;
	spinlock_init(&Kstack_pool_lock, "kstkpoolck");

	/* let vhand know that there are pages here */
	shake_register(SHAKEMGR_MEMORY, kstackpool_shake);
}

static void
kstack_timeout()
{
	int 	s;

	s = mutex_spinlock(&Kstack_pool_lock);
	if (Kstack_pagecnt == 0) {
		/*
		 * Don't do anything, other than make
		 * sure that Kstack_timeout is set
		 * (vhand may have turned it off).
		 */
		Kstack_timeout = 1;
	} else if (Kstack_failed) {
		/* 
		 * In the last interval there were still not
		 * enough pages in the pool, even though 
		 * some have come back in. Don't free any
		 * pages yet, but drop the max down so that
		 * kstack_free() might start freeing up pages
		 * a little sooner.
		 */
		ASSERT(Kstack_max >= Kstack_basemax);
		Kstack_max -= ((Kstack_max - Kstack_basemax) / 6);
		Kstack_failed = 0;
	} else {

		int	i;

		/*
		 * Pages have come back into the
		 * pool and there have been no allocation
		 * failures in the last interval. Slowly
		 * bring Kstack_max back down (the steady
		 * state in the system might imply that it
		 * should be larger than Kstack_basemax).
		 */

		ASSERT(Kstack_max >= Kstack_basemax);
		Kstack_max -= ((Kstack_max - Kstack_basemax) / 3);

		if (Kstack_pagecnt > Kstack_max) {

			pfd_t	*pfd;
			for (i = Kstack_pagecnt - Kstack_max; i > 0; i--) {
				pfd = Kstack_pool;
				Kstack_pool = pfd->pf_next;
				pfd->pf_next = NULL;
				pagefree(pfd);
				Kstack_pagecnt--;
			}
		} 
		if ((Kstack_max - Kstack_basemax) <= MIN_KSTACK_PAGES) {
			/*
			 * Don't bother adjusting Kstack_max any
			 * more- this is close enough.
			 */
			Kstack_timeout = 0;
		} else
			Kstack_timeout = 1;
	}
	mutex_spinunlock(&Kstack_pool_lock, s);
	if (Kstack_timeout)
		(void) timeout(kstack_timeout, 0, KSTACK_TIMEOUT);
}

pfd_t *
kstack_alloc()
{
	pfd_t	*pfd;
	int	s;
	int     queued_timeout = 0;

	
	s = mutex_spinlock(&Kstack_pool_lock);
	pfd = Kstack_pool;
	if (pfd) {
		if ((pfd->pf_flags & P_QUEUE) ||
		    (pfd->pf_use == 0))
			cmn_err_tag(127,CE_PANIC,
			"kstack_alloc: free page 0x%x in kstack pool\n", pfd);
		Kstack_pool = pfd->pf_next;
		pfd->pf_next = NULL;
		Kstack_pagecnt--;
		/*
		 * These pages are guaranteed to be
		 * VCE-safe- nothing more to do.
		 */
	} else {

		/*
		 * Allocate one and flush out the VCEs.
		 * If a timeout has not been set up, then
		 * do so, in case there gets to be too many
		 * pages in the Kstack pool. If they really 
		 * are not needed after a while, then the routine
		 * kstack_timeout will free them up.
		 */
		mutex_spinunlock(&Kstack_pool_lock, s);
		pfd = pagealloc(0,VM_UNSEQ);
		if (pfd) {
			/*
			 * NOTE: Rather than invoke _bclean_caches, we call
			 * a lower level function which simply brings the
			 * page into our cache with the EXCLUSIVE attribute
			 * using the correct virtual alias ... this forces
			 * any potential VCEs to occur now and avoids
			 * problems in the future.
			 */
			sync_dcaches_excl((void *)(KSTACKPAGE),
				NBPP, pfdattopfn(pfd),
				CACH_NOADDR|CACH_AVOID_VCES);
		}

		s = mutex_spinlock(&Kstack_pool_lock);
		
		/* let the pool grow as needed */
                Kstack_max++;
                Kstack_failed++;
                if (Kstack_timeout == 0) {
                        Kstack_timeout = 1;
                        queued_timeout++;
                        mutex_spinunlock(&Kstack_pool_lock, s);
                        (void) timeout(kstack_timeout, 0, KSTACK_TIMEOUT);
                }
        }
        if (queued_timeout == 0)
                mutex_spinunlock(&Kstack_pool_lock, s);

	return (pfd);
}

void
kstack_free(pfd_t *pfd)
{
	int	s;
		
	if ((pfd->pf_flags & P_QUEUE) ||
	    (pfd->pf_use == 0))
		cmn_err_tag(128,CE_PANIC,"kstack_free: freeing free page 0x%x\n", pfd);

	if (Kstack_pagecnt >= Kstack_max)
		pagefree(pfd);
	else {
		s = mutex_spinlock(&Kstack_pool_lock);
		pfd->pf_next = Kstack_pool;
		Kstack_pool = pfd;
		Kstack_pagecnt++;
		mutex_spinunlock(&Kstack_pool_lock, s);
	}
}

/* ARGSUSED */
static int
kstackpool_shake(int level)
{
	int 	s;
	int	i = 0;
	pfd_t	*pfd;


	if (Kstack_pagecnt == 0)
		return (0);

	s = mutex_spinlock(&Kstack_pool_lock);

	pfd = Kstack_pool;
	while (pfd) {
		Kstack_pool = pfd->pf_next;
		Kstack_pagecnt--;
		mutex_spinunlock(&Kstack_pool_lock, s);

		pagefree(pfd); 

		s = mutex_spinlock(&Kstack_pool_lock);
		i++;
		pfd = Kstack_pool;
	}
	Kstack_pool = NULL;
	Kstack_timeout = 0;
	Kstack_max = Kstack_basemax;

	mutex_spinunlock(&Kstack_pool_lock, s);
	return(i);
}

#endif /* KSTACKPOOL */


/*
 * Support to discard pages that either have transient or permanent errors.
 * All error pages will be marked with P_ERROR. Additionally, if it is an
 * uncorrectable error, the page will be marked with P_HWBAD.
 *
 * When the error handling code calls page_discard, we take care of those
 * pages that are either on the free queue (with or without any association)
 * or pages that are anonymous. Vnode pages are currently not handled, since 
 * this could lead to saving corrupted files. In future, however, we could
 * swap a good page for the bad one, if the bad page is not dirty.
 * 
 * Processes that access uncorrectable error pages will take an exception. 
 * This is taken care of by the error handling code, either by turning off 
 * access to the bad page or using any other feature provided by the 
 * hardware.
 *
 * If the page is already on the free list, it is marked error and left on 
 * the list. A subsequent allocation of the page detects that the page was
 * marked as error and calls into the error recovery code to reclaim the page.
 * If the page cannot be reclaimed, it is put in a discard freelist.
 *
 * If the page is in use when we are marking it as error, when the page is
 * freed it is disassociated from its hash and either reclaimed or discarded.
 *
 * The other related changes are for vhand not to steal pages that are marked
 * and for pfault not to bcopy cow pages that are marked as error.
 *
 * Arguments permanent indicate that the page should be discarded permanently.
 * Otherwise, the page will be discarded only as long as someone has a 
 * a reference to the page
 *
 * Argument uncorrectable indicates that the page should be marked 
 * inaccessible. Otherwise, accesses to the page will be allowed.
 *
 * NOTES: pages that are marked uncorrectable but temporary will cause the
 * page to be inaccessible as long as someone has a reference to the page.
 * When the last reference to the page is gone, the page is put back on
 * the free list.
 *
 * page_discard(addr, 0, 0) will have no effects on the page and its users.
 */

int
page_discard(paddr_t paddr, int permanent, int uncorrectable)
{
	uint discard_flags;
	pfd_t *pfd;
	int token;
	pfn_t pfn;
	extern void error_mark_page(paddr_t);

#if defined (DEBUG)
	uint	flags;
#endif /* DEBUG */
	discard_flags = P_ERROR;

	if (permanent)
	    discard_flags |= P_HWBAD;

	if (uncorrectable)
	    discard_flags &= ~P_HWSBE;
	else
	    discard_flags |=  P_HWSBE;

	/* correctable, transient: do nothing */
	if (!permanent && !uncorrectable) 
	    return 1;

	pfn = pnum(paddr);
	pfd   =	pfntopfdat(pnum(paddr));

	if (pfdattopfn(pfd) != pfn)
	    return 0;

	/*
	 * make sure this pfdat points to a valid page, not to a page that
	 * does not have a pfdat or a page where the pfdat itself belongs to.
	 */

	if (!page_validate_pfdat(pfd))
	    return 0;

	token = pfdat_lock(pfd);

	if (pfdat_iserror(pfd)) {
		pfdat_unlock(pfd, token);
		return 1;
	}
	
	if (((pfd->pf_flags & P_QUEUE) == 0) &&
	    ((pfd->pf_flags & (P_ANON | P_HASH)) != (P_ANON | P_HASH))) {
		pfdat_unlock(pfd, token);
#if defined (DEBUG)
		cmn_err(CE_WARN, "!page_discard 0x%x: flags 0x%x not handled",
			paddr, pfd->pf_flags);
#endif
		return 0;
	}

	if (reservemem(GLOBAL_POOL, 1, 1, 1)) {
		pfdat_unlock(pfd, token);
		cmn_err(CE_WARN, "Cannot discard page, memory fully reserved");
		return 0;
	}

	pfd->pf_flags |= discard_flags;
	/*
	 * Call into error handler to do stuff necessary to ensure any 
	 * future accesses to page will result in some exception.
	 */
	if (uncorrectable)
	    error_mark_page(paddr);

#if defined (DEBUG)
	flags = pfd->pf_flags;
#endif /* DEBUG */

	pfdat_unlock(pfd, token);

#if defined (DEBUG)
	cmn_err(CE_NOTE | CE_CPUID, "page discard, pfd 0x%x flags 0x%x [%s %s]",
		pfd, flags,
		flags & P_HWBAD ? "P_HWBAD ": "",
		flags & P_HWSBE ? "P_HWSBE" : "");
#endif	

	/*
	 * Rough count for statistics only. No locks.
	 */
	NODEPDA(pfdattocnode(pfd))->error_page_count++;

	return 1;
}

int 
page_isdiscarded(paddr_t paddr)
{
	pfd_t *pfd = pfntopfdat(pnum(paddr));
	
	/*
	 * A pfdat which does not point to a valid page cannot have been
	 * discarded.
	 */
	if (!page_validate_pfdat(pfd))
	    return 0;
	
	return pfdat_iserror(pfd);
}


void
page_discard_enqueue(pfd_t *pfd)
{
#ifdef SN
	extern int sbe_mem_verbose;
#endif
	int locktoken;
	plist_t	*plistp;

	/*REFERENCED*/
	cnodeid_t cnode = pfdattocnode(pfd);

	plistp = &NODEPDA(cnode)->error_discard_plist;

	locktoken = mutex_spinlock(&NODEPDA(cnode)->error_discard_lock);

	NODEPDA(cnode)->error_discard_count++;
	append(plistp, pfd);
	mutex_spinunlock(&NODEPDA(cnode)->error_discard_lock, locktoken);

#if defined (DEBUG)
	cmn_err(CE_NOTE,"Pfd 0x%x unusable, discarded", pfd);
#else
#ifdef SN
	if( sbe_mem_verbose & SBE_MEM_VERBOSE_REPORT_DISCARD )
	    cmn_err(CE_NOTE,"Discarding bad memory page, pfd=0x%x\n", pfd);
#endif /* SN */
#endif /* DEBUG */
}


int
page_error_clean(pfd_t *pfd)
{
	extern int error_reclaim_page(paddr_t, int);
	int rc;

#if defined (DEBUG)
	cmn_err(CE_NOTE | CE_CPUID, "page clean on pfd 0x%x flags 0x%x",
		pfd, pfd->pf_flags);
#endif /* DEBUG */

	rc = error_reclaim_page(ctob(pfdattopfn(pfd)), 
			(pfdat_ishwbad(pfd) || pfdat_ishwsbe(pfd)));
	if (rc) {
		/*
		 * Rough count for statistics only. No locks.
		 */
		NODEPDA(pfdattocnode(pfd))->error_cleaned_count++;
		pfd->pf_flags &= ~(P_ERROR | P_HWBAD | P_HWSBE);
	}

	return rc;
}

/*ARGSUSED */	
int
page_validate_pfdat(pfd_t *pfd)
{
	pfn_t pfn;
#ifdef DISCONTIG_PHYSMEM
	cnodeid_t node;
	int slot;
	int slot_psize;
	int slot_offset;

	node = local_pfdattocnode(pfd);
	if (node == CNODEID_NONE)
		return 0;

	pfn = local_pfdattopfn(pfd);
	slot = pfn_to_slot_num(pfn);
	
	slot_psize = slot_getsize(node,slot);
	slot_offset = pfn_to_slot_offset(pfn);

	if (slot_offset < slot_psize)
	    if (local_pfntopfdat(pfn) == pfd)
		if (!PG_ISHOLE(pfd))
		    return 1;
	
	return 0;
#else
	pfn = pfdattopfn(pfd);
	if (pfn < btoc(kpbase) || pfn > maxclick)
		return 0;

	if (PG_ISHOLE(pfd))
		return 0;

	return 1;	/* success, pfdat is valid */
#endif
}

#ifdef MH_R10000_SPECULATION_WAR

/*
 * Attempt to relocate any users of spfd to a newly allocated page
 * (reference to be stored in *dpfd_p), of a class defined by flags.
 * If (flags & VM_DMA_RW), then the new page will be in I/O-addressable
 * memory.  Otherwise, it will be in any available memory.  
 *
 * If the page need not be moved, *dpfd_p will be NULL, and this routine
 * will return 0.  If the page must be moved, and can be moved, *dpfd_p
 * will be non-NULL, spfd will have been freed, and this routine will 
 * return 0.  Otherwise, *dpfd_p will be NULL and this routine will 
 * return a non-zero error code.
 *
 * Note: this function could be extended to support local memory.
 */
/*ARGSUSED*/
int
page_migrate_lowmem_page(pfd_t *spfd, pfd_t **dpfd_p, int flags)
{
	pfd_t *dpfd;
	int	error;

	cmn_err(CE_NOTE,"Migrating lowmem page 0x%x\n", spfd);
	if (dpfd_p != NULL)
		*dpfd_p = NULL;
	
	if (!(flags & (VM_DMA_RW | VM_NO_DMA)))
		flags |= VM_DMA_RW;
	if ((!(flags & VM_DMA_RW)) || spfd >= pfd_lowmem)
		return(0);

#ifdef _VCE_AVOIDANCE
	/* called only as part of the O2/R10k WAR */
	ASSERT(!vce_avoidance);
#endif /* _VCE_AVOIDANCE */

	/* allocate a replacement page */
again:
	dpfd = pagealloc(pfdattopfn(spfd), flags);
	if (dpfd == NULL) {
		if (flags & VM_NOSLEEP)
			return(ENOMEM);
#ifdef MH_R10000_SPECULATION_WAR
		setsxbrk_class(flags);
#else /* MH_R10000_SPECULATION_WAR */
		setsxbrk();
#endif /* MH_R10000_SPECULATION_WAR */
		goto again;
	}

	error = migr_migrate_frame(spfd, dpfd);
	if (error) {
		pagefree(dpfd);
		if (showconfig)
			cmn_err(CE_NOTE,
		"!page_migrate_lowmem_page: migr_migrate_frame 0x%x to 0x%x failed %d\n",
				pfdattopfn(spfd), pfdattopfn(dpfd), error);
		return(EBUSY);
	}

	if (dpfd_p != NULL)
		*dpfd_p = dpfd;
	pagefree(spfd);
	if (showconfig)
		cmn_err(CE_NOTE,"!Migrated lowmem page 0x%x to page 0x%x with %s tag 0x%x pageno 0x%x flags 0x%x\n", 
			pfdat_to_eccpfn(spfd),
			pfdat_to_eccpfn(dpfd),
			((dpfd->pf_flags & P_ANON) ? "anon" : "vnode"),
			dpfd->pf_tag, dpfd->pf_pageno, dpfd->pf_flags);
	return(0);
}

/*
 * Kernel page reference list
 *
 * For certain purposes, notably turning off hardware-valid in PTEs on
 * Moosehead R10000 systems for pages to which a DMA read is in progress,
 * it is necessary to know all kernel virtual pages which reference a given
 * physical page.  When the page is not hashed, and there is at most one
 * reference, the reference is stored in the pf_tag field.  Otherwise, both
 * when there are more references and when the page is in the page hash table,
 * the references are stored in a separate hash table.  The manner of storage
 * of kernel references is opaque to all callers of the kernel reference
 * list access routines; callers simply walk the list.
 *
 * At the present time, these routines execute correctly only on uniprocessor
 * systems.  This is not presently a problem, since multiprocessor systems have
 * no need to record kernel references.
 */

typedef __psint_t krpf_ref_t;

#define KRPF_NO_REF 0
#define KRPF_END_REF 1

static krpf_ref_t *krpf_pfdat;
static krpf_ref_t *krpf_pfdat_base;
static krpf_ref_t *krpf_kptbl;

#ifdef pfdat_to_eccpfn
#define krpf_pfn_to_ref(pfn) (krpf_pfdat + pfn_to_eccpfn(pfn))
#define krpf_pfdat_to_ref(pfd) (krpf_pfdat + pfdat_to_eccpfn(pfd))
#else 
#define krpf_pfn_to_ref(pfn) (krpf_pfdat + pfn)
#define krpf_pfdat_to_ref(pfd) (krpf_pfdat + pfdattopfn(pfd))
#endif
#define krpf_vpn_to_ref(vpn) (krpf_kptbl + (vpn - btoct(K2BASE)))

/*REFERENCED*/
static lock_t *krpflock = NULL;
static int krpf_enabled = 0;

#ifdef _KRPF_TRACING
#define KRPF_TRACE_MAX 2000

#define KRPF_TRACE_ADD 1
#define KRPF_TRACE_REMOVE 2
#define KRPF_TRACE_NONE 3

typedef int krpf_trace_op_t;

typedef struct krpf_trace_s {
	krpf_trace_op_t op;
	pfd_t *pfd;
	krpf_ref_t vpn;
} krpf_trace_t;
	
krpf_trace_t krpf_trace_buf[KRPF_TRACE_MAX];

krpf_trace_t *krpf_trace_next = krpf_trace_buf;
krpf_trace_t *krpf_trace_last = krpf_trace_buf + KRPF_TRACE_MAX - 1;

krpf_ref_t krpf_trace_check_ref = 0;

int krpf_debug(krpf_trace_t *);

void
krpf_check(__psint_t vfn)
{
#if 0
	if (krpf_trace_check_ref == vfn)
		krpf_debug(NULL);
#endif
}
#if 0
extern int get_r4k_counter(krpf_trace_t *);
#endif
int
krpf_debug(krpf_trace_t *a)
{

	return(get_r4k_counter(a));
}

#define KRPF_TRACE(x,y,z) \
	( krpf_trace_next->op = (x), \
	  krpf_trace_next->pfd = (y), \
	  krpf_trace_next->vpn = (z), \
	  (((z) == krpf_trace_check_ref) ? krpf_debug(krpf_trace_next) : 0), \
	  krpf_trace_next = ((krpf_trace_next == krpf_trace_last) \
			     ? krpf_trace_buf \
			     : (krpf_trace_next + 1)))

void 
krpf_no_reference(__psint_t vfn)
{
	krpf_ref_t *vpn_ref;
	int	s;
	krpf_trace_t *trace;
	
	if (! krpf_enabled)
		return;

	vpn_ref = krpf_vpn_to_ref(vfn);

	s = mutex_spinlock_spl(&krpflock,splhi);
	if (*vpn_ref == KRPF_NO_REF)
		goto done;
	trace = krpf_trace_next;
	KRPF_TRACE(KRPF_TRACE_NONE,NULL,vfn);
	krpf_debug(trace);
done:
	mutex_spinunlock(&krpflock,s);
}
#else /* _KRPF_TRACING */
#define KRPF_TRACE(x,y,z)
#endif /* _KRPF_TRACING */

void
krpf_early_init(caddr_t *nextbyte)
{
	pfn_t	curclick;
	int	tsize;

	if (! IS_R10000())
		return;

	tsize = syssegsz * sizeof(krpf_ref_t);
	krpf_kptbl = (krpf_ref_t *)low_mem_alloc(tsize, nextbyte, "krpf_kptbl");
	bzero((caddr_t)krpf_kptbl, tsize);

	curclick = btoct(K0_TO_PHYS(*nextbyte));
	tsize = (maxclick - curclick + 1) * sizeof(krpf_ref_t);
	krpf_pfdat_base = (krpf_ref_t *)low_mem_alloc(tsize, nextbyte,
						"krpf_pfdat_base");
	bzero((caddr_t)krpf_pfdat_base, tsize);
	krpf_pfdat = krpf_pfdat_base - curclick;
}

void
krpf_init(void)
{
	if (! IS_R10000())
		return;

	krpflock = spinlock_alloc(0,"krpf");
	krpf_enabled = 1;
}

void
krpf_add_reference(pfd_t *pfd,	/* pfdat for page referenced 	*/
		   __psint_t ref) /* kernel VPN to add	*/
{
	krpf_ref_t *vpn_ref;
	krpf_ref_t *pfd_ref;
	int	s;
	
	if (! krpf_enabled)
		return;

	vpn_ref = krpf_vpn_to_ref(ref);
	pfd_ref = krpf_pfdat_to_ref(pfd);

	s = mutex_spinlock_spl(&krpflock,splhi);
	KRPF_TRACE(KRPF_TRACE_ADD,pfd,ref);
	if (*vpn_ref != KRPF_NO_REF) {
		pfn_t 	i;
		pgno_t 	j;
		pgno_t	cvpn;

		cvpn = ref;
		while (1) {
			for (i = pnum(kpbase); i < maxclick; i++) {
				if (krpf_pfdat[i] == cvpn)
					goto found_pfdat;
			}
			for (j = 0; j < syssegsz; j++) {
				if (krpf_kptbl[j] == cvpn) 
					goto found_kptbl;
			}
			goto no_pfdat;
found_kptbl:
			cvpn = j;
		}

found_pfdat:
		cmn_err_tag(129,CE_PANIC,
			"krpf_add_reference: dup reference for VPN 0x%x (PFN = 0x%x)",
			ref,i);
		goto done;
no_pfdat:
		cmn_err(CE_WARN,
			"krpf_add_reference: lost dup reference for VPN 0x%x",ref);
		*vpn_ref = KRPF_NO_REF;
	}
	if (*pfd_ref == KRPF_NO_REF)
		*vpn_ref = KRPF_END_REF;
	else
		*vpn_ref = *pfd_ref;
	*pfd_ref = ref;
done:
	mutex_spinunlock(&krpflock,s);
}


void
krpf_remove_reference(pfd_t *pfd,  /* pfdat for page referenced */
		      __psint_t ref) /* kernel VPN to remove	*/
{
	krpf_ref_t *vpn_ref;
	krpf_ref_t *pfd_ref;
	krpf_ref_t lvpn;
	krpf_ref_t *lvpn_ref;
	int	s;
	
	if (! krpf_enabled)
		return;

	vpn_ref = krpf_vpn_to_ref(ref);
	pfd_ref = krpf_pfdat_to_ref(pfd);

	s = mutex_spinlock_spl(&krpflock,splhi);
	KRPF_TRACE(KRPF_TRACE_REMOVE,pfd,ref);
	lvpn = *pfd_ref;
	if (*vpn_ref == KRPF_NO_REF)
		goto done;
	if (lvpn == ref)
		*pfd_ref = (*vpn_ref == KRPF_END_REF ? KRPF_NO_REF : *vpn_ref);
	else if (lvpn == KRPF_NO_REF)
		goto done;
	else {
		while (1) {
			lvpn_ref = krpf_vpn_to_ref(lvpn);

			if (*lvpn_ref == ref) 
				break;
			lvpn = *lvpn_ref;
			if (lvpn == KRPF_END_REF ||
			    lvpn == KRPF_NO_REF)
				goto done;
		}
		*lvpn_ref = *vpn_ref;
	}
	*vpn_ref = KRPF_NO_REF;
done:
	mutex_spinunlock(&krpflock,s);
}


void
krpf_visit_references(pfd_t *pfd,
		      void (*fp)(void *,pfd_t *,__psint_t),
		       void *fparg)
{
	krpf_ref_t *vpn_ref;
	krpf_ref_t *pfd_ref;
	krpf_ref_t vpn;
	krpf_ref_t nvpn;
	int s;

	if (! krpf_enabled)
		return;
		
	pfd_ref = krpf_pfdat_to_ref(pfd);

	s = mutex_spinlock_spl(&krpflock,splhi);
	
	for (vpn = *pfd_ref;
	     vpn != KRPF_NO_REF &&  vpn != KRPF_END_REF;
	     vpn = nvpn) {
		vpn_ref = krpf_vpn_to_ref(vpn);
		nvpn = *vpn_ref;
		fp(fparg,pfd,vpn);
	}
	mutex_spinunlock(&krpflock,s);
	return;
}
#endif /* MH_R10000_SPECULATION_WAR */

#if IP32 && defined(DEBUG)
void
page_eccverify(pfd_t *pfd)
{
    caddr_t va;
    long *vaend;
    volatile long l, *lp;

    va = page_mapin(pfd, VM_UNCACHED, 0);
    if (va) {
	vaend = (long *)(va + ctob(1));
	for (lp = (long *)va; lp < vaend; lp++)
	    l = *lp;
	page_mapout(va);
    }
}
#endif /* IP32 && DEBUG */
#ifdef TILES_TO_LPAGES
/*
 * tphead_move
 *
 * Transfers 16 pfdats from one tphead pool to another.
 */
static int
tphead_move(pfd_t *pfd, tphead_t *srctphead, tphead_t *dsttphead)
{
	phead_t *srcpheadp, *dstpheadp;
	plist_t *plistp;
	pfd_t *next, *prev;
	int i, pidx, nmoved;

	ASSERT(issplhi(getsr()));

	nmoved = 0;
	for (i = 0; i < TILE_PAGES; i++, pfd++) {
		if ((pfd->pf_flags & P_QUEUE) == 0)
			continue;

		ASSERT((pfd->pf_flags & P_ECCSTALE) == 0);

		pidx = pfdattopfn(pfd) & PHEADMASK(MIN_PGSZ_INDEX);
		srcpheadp = &srctphead->phead[pidx];
		dstpheadp = &dsttphead->phead[pidx];

		/* pagedequeue from src phead... */
		srcpheadp->ph_count--;
		next = pfd->pf_next;
		prev = pfd->pf_prev;
		next->pf_prev = prev;
		prev->pf_next = next;

		/* ...then append to dst phead */
		dstpheadp->ph_count++;
		plistp = &dstpheadp->ph_list[pfd->pf_flags & P_HASH ?
					    STALE_ASSOC : STALE_NOASSOC];
		append(plistp, pfd);
		nmoved++;
	}

	return nmoved;
}


/*
 * This tile has transitioned from unfragmented to fragmented
 * because one of the pages has been fixed.  We need to find
 * all the free pages in this tile and move them from the
 * unfragmented tile page pool to the fragmented pool.
 */
void
tilepages_to_frag(pfd_t *pfd)
{
	tphead_t *srctphead, *dsttphead;
	int nmoved;

#ifdef MH_R10000_SPECULATION_WAR
	if (pfd < pfd_lowmem)
		return;
#endif /* MH_R10000_SPECULATION_WAR */

	srctphead = &tphead[TPH_UNFRAG];
	dsttphead = &tphead[TPH_FRAG];

	nmoved = tphead_move(pfd, srctphead, dsttphead);

	srctphead->count -= nmoved;
	dsttphead->count += nmoved;
}


/*
 * Inverse of tilepages_to_frag() above, transition from
 * fragmented to unfragmented tile page pool.
 */
void
tilepages_to_unfrag(pfd_t *pfd)
{
	tphead_t *srctphead, *dsttphead;
	int nmoved;

#ifdef MH_R10000_SPECULATION_WAR
	if (pfd < pfd_lowmem)
		return;
#endif /* MH_R10000_SPECULATION_WAR */

	srctphead = &tphead[TPH_FRAG];
	dsttphead = &tphead[TPH_UNFRAG];

	nmoved = tphead_move(pfd, srctphead, dsttphead);

	srctphead->count -= nmoved;
	dsttphead->count += nmoved;
}
#endif /* TILES_TO_LPAGES */
