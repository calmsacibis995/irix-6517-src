/*
 * vm/anon.c
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

#ident	"$Revision: 1.97 $"

#include "sys/anon.h"
#include "anon_mgr.h"
#include "sys/kmem.h"
#include "sys/getpages.h"
#include "sys/swap.h"
#include "sys/pfdat.h"
#include "sys/vnode.h"
#include "sys/page.h"
#include "sys/nodepda.h"
#include "sys/lpage.h"
#include "sys/atomic_ops.h"
#include "sys/systm.h"
#include "sys/immu.h"
#include "sys/tuneable.h"
#include "sys/debug.h"
#include "ksys/migr.h"
#include "ksys/rmap.h"
#include "sys/numa.h"


/*
 * The Anonymous Manager.
 *
 * Its purpose is to cache and manage the anonymous pages in a region and
 * record the backing store location (i.e., swap space) associated with
 * them (and to provide a swap handle to locate them).  It also handles
 * copy-on-write sharing.
 *
 * When a region is created, a new anonymous struct is allocated which
 * is initially empty.  As anonymous pages are created (as a result of
 * modifying a page in MAP_PRIVATE mapping for example), they are inserted
 * into the anon struct.  The upper level region code can later check to 
 * see if an anonymous page is present for a given logical page number 
 * within a region.  If found, the anonymous manager returns a pointer
 * to the page's pfdat or a swap handle if it is on disk.
 *
 * When a process forks, non-shared regions are dup'ed and the pages
 * are shared with copy-on-write.  The anonymous manager keeps track of
 * this relationship by building a binary tree of anon structs.  The
 * parent's original anon struct becomes the root of the tree and 
 * both the parent and child are given a new, empty anon struct.  This
 * allows them to share pages that were entered into the anon struct
 * prior to the fork.  At the same time, each process has a unique
 * struct in which to enter their new anon pages.  Pages are ONLY
 * inserted into leaves.  All interior nodes of the tree represent
 * pages being shared copy-on-write.  No insertions can occur in 
 * these nodes since they would appear in all processes.
 *
 * A process's subtree grows by one level each time it forks.  
 * It logically shrinks by a level each time one of the processes execs 
 * or exits.  The actual shrink is postponned to avoid changing the
 * structure of the tree while other processes could be using handles
 * to it.
 *
 * A "handle" to a leaf node is returned when a new anonymous struct
 * is set up for a new region or when an existing one is dup'ed.
 * The handle must be given for all future operations involving the
 * anonymous memory associated with the region.  The handle is simply
 * the address of the leaf anon struct.
 *
 * A handle on a leaf node becomes shared after a fork as it moves down
 * one level in the tree.  The tree is not immediately collapsed when
 * one of the processes execs or exits since the other could still be
 * using the handle of the shared anon struct (collapsing the tree will
 * eliminate an anon struct).  Therefore the collapse is postponned until
 * the other process later forks, execs, or exits.  This is a good time
 * to do the collapse since it is guaranteed that it is the only process
 * using the affected anon structs, preventing any possible race conditions
 * where handles are being looked up and changed at the same time. 
 *
 * The anonymous manager keeps track of which pages are known to it
 * by storing the in-core pages in its own page cache structure.  When 
 * a page is swapped, the swap handle for that page is entered into the
 * anon node's swap handle cache.  A page is known to be an anonymous
 * page for this node if there is either an in-core page in the node, a
 * swap handle for the page, or both.  If neither exists, then the anonymous
 * page does not exist.
 *
 * We allow a race to occur between anon_setswap adding
 * a swap handle and anon_lookup which returns the current swap handle.
 * The caller (vfault) knows about this and re-tries the lookup
 * if we gave it a NULL handle when it thinks there should be one.  It's
 * not possible to get rid of this race in any event since the lookup could
 * have finished just before anon_setwap gets called.
 *
 * Since pages are inserted only into leaf nodes, new pages added
 * in leaves may "cover" pages in the parent.  This happens everytime
 * copy-on-write is broken for a process.  "Covering" a page in
 * a node makes any entries in any of the parent nodes invisible
 * to the process.  It is important to note that even though a page
 * in a node is covered by one child, a process in the other path
 * to the node could still be using it.  For performance reasons, we 
 * do not attempt to clean up fully covered pages when a new page 
 * is inserted in a leaf.  It is too much work to trace the tree to
 * see if all other paths to the page in question cover it.  Since
 * swap space is associated with an anonymous page in the node itself,
 * a side effect of this policy is that swap space is not freed
 * when a page becomes fully covered and is no longer in use by
 * anyone (the physical memory is of course freed by the upper layers).
 * This unused swap space is periodically reclaimed by vhand (when 
 * swap space is running low) by walking the anonymous tree to determine
 * which pages are fully covered.  Note that this is not a high runner
 * case as large anonymous trees with lots of covered pages only occur
 * when a process forks many times and none of the children exec.
 *
 * Locking Policies -
 *
 * Each anon tree is protected by a single lock.  Ideally, each anon
 * node could have its own lock.  Unfortunately, this is complex to
 * implement as different tree operations scan the tree in different
 * orders which creates many AB-BA locking problems.  Since the most
 * common case is a single anon node with no copy-on-write history,
 * implementing having one lock per tree is the same as having one
 * per node.  So it is currently felt that the complexity of one lock
 * node isn't worth it.
 *
 * Certain operations require the interaction of three different locks:
 * the anon tree lock, the free list lock, and a pfdat lock.  The
 * ordering of the locks in the hierarchy is as listed with the anon
 * tree lock (or more generally, the memory object lock) at the top
 * of the hierarchy, and the pfdat lock being at the lowest level.
 * Therefore, when the memory object is reclaiming a page off the free
 * list, the object is held during the free list removal.  Conversely,
 * the operation of recycling a page on the free list to a new use
 * requires that the free list lock be dropped before acquiring the
 * object lock.  To ensure that the object is not destroyed while 
 * waiting for the object lock, the free list code acquires a hold
 * on the anon object.  This is done while holding the pfdat lock for
 * a page already associated with the object, which ensures the object
 * will persist (since the object can't be destroyed while there are
 * pages associated with it and the pfdat lock is required to disassociate
 * a page from an object).  Also, the P_RECYCLE flag is set on the
 * page to prevent pfinds from acquiring it.  This means the code
 * doesn't have to worry about the pfdat changing state while it
 * is waiting for the object lock.
 *
 * The hold on anon object is implemented with a reference count.  Normally,
 * the reference count is 1, which reflects the node's membership in an
 * anon tree (i.e., the tree has a reference to the node).  When an outside
 * caller wants to ensure an anon object doesn't disappear, anon_hold is
 * called which bumps the count.  This prevents anon_release from freeing
 * the anon struct until the reference count goes to zero.
 *
 * Dynamic Memory Allocation -
 *
 * This module uses dynamic memory allocation for all of its data structures.
 * The pcache and scache sub-modules do dynamic allocation as well.  An 
 * important rule is that no memory allocations may be done while holding
 * the tree lock.  The first reason for this is that one cannot sleep while
 * holding a spin lock.  The second reason is that even if NOSLEEP is passed
 * to the allocation routines, they may call pagealloc to get a page which
 * may in turn try to recycle a page belonging to the tree for which we
 * are already holding the lock, and hence causing a deadlock.  Therefore,
 * all memory allocations must be done before the tree lock is acquired.
 *
 * An exception to this rule is the swap handle cache.  This module maintains
 * its own private reserve of pages (that have no hash assocation), and
 * so recycling pages is not needed.
 */


/*
 * Structure used to pass arguments to the swap_merge routines.
 */

struct swap_merge_cookie {
	anon_t	*into;
	void	*preempt_cookie;
};

typedef struct swap_merge_cookie swap_merge_cookie_t;

static anon_t *anon_alloc(void);
static anon_t *collapse(anon_t *, void *);
static anon_t *collapse_pair(anon_t *, void *);
static void ctop_page_merge(anon_t *, pfd_t *);
static void ctop_swap_merge(swap_merge_cookie_t *, pgno_t, sm_swaphandle_t);
static void ptoc_page_merge(anon_t *, pfd_t *);
static void ptoc_swap_merge(swap_merge_cookie_t *, pgno_t, sm_swaphandle_t);
static void anon_unlink(anon_t *, void *);
static void anon_release(anon_t *);
static void _anon_setsize(anon_t *, pgno_t, void *);
static void chk_coverage(anon_t *, pgno_t, sm_swaphandle_t);
static void prune(anon_t *);
static int  anon_isleaf(anon_t *);
static void anon_preempt(void *);
static void free_covered_page(anon_t *, pfd_t *);
static void add_noncovrd_page(anon_t *, pfd_t *);
static void _anon_pagefree_and_cache(pfd_t *);
void _psremove(pfd_t *);

#if TIME_LOCKS

void release_tree_lock_nocheck(anon_lock_t *, int);
int  get_lock_time(anon_lock_t *);
int  get_elapse_time(unsigned);
#endif

/*
 * A preemption cookie is used to encapsulate information about the lock
 * currently held so that it can be passed around as a unit.  This is
 * used to allow lock preemption (and hence allow interrupts to occur)
 * during long pcache_remove() operations.
 */

struct preemption_cookie {
	anon_lock_t	*pc_lock;	/* the lock that is held        */
	int		 pc_locktoken;	/* from the acquire of the lock */
};

typedef struct preemption_cookie preempt_cookie_t;


/*
 * Count of unreferenced, dirty, shared anonymous pages.
 * NOTE: count is maintained ONLY on DEBUG systems. The
 * variable is defined on non-DEBUG systems just in case
 * some 3rd party tools reads the variable thru kmem - it
 * is better to have an unused variable than not to have
 * one at all.
 */

int	psanonmem;		/* # pages on psanonlists    */


/*
 * This macros removes the given pfdat from the sanon free list.  For the
 * MP case, we need to check the flag again after getting the list lock
 * since it may have been removed from the list by the page out daemon
 * in the meantime.  If the page is no longer P_SQUEUE, then there's
 * nothing to do.
 */

#if MP

#define psremove(pfd)					\
    do {						\
	sanon_list_head_t *salist = &SANON_LIST_FOR_PFD(pfd);\
	nested_spinlock(&salist->sal_lock);		\
							\
	if (pfd->pf_flags & P_SQUEUE)			\
		_psremove(pfd);				\
							\
	nested_spinunlock(&salist->sal_lock);		\
    } while (0)

#else /* !MP */

#define psremove(pfd)	_psremove(pfd)

#endif /* !MP */

#ifdef DEBUG1
#define PRINTF(x) printf((x))
#else
#define PRINTF(x)
#endif


/*
 * init_anon
 *
 * One time initialization function.  Set up the dynamic allocation zone
 * and sanon free list head(s).
 */

static zone_t *anon_zone;
static zone_t *anon_lock_zone;

#ifdef  DEBUG
#define ANON_TRACE              1
#include "sys/kthread.h"
extern kthread_t *schedthread;
#endif

#ifdef	ANON_TRACE

#include "sys/ktrace.h"
#include <sys/idbgentry.h>              /* idbg_addfunc                 */
#include <ksys/vproc.h>

ktrace_t	*anon_ktrace;
void	idbg_anon_trace(__psunsigned_t);
void	idbg_anon_trace_time(__psunsigned_t);
void	idbg_anon_trace_pid(__psunsigned_t);

#define	ANON_NONE			0
#define	ANON_DUP			1
#define	ANON_FREE			2
#define	ANON_COLLAPSE			3
#define	ANON_COLLAPSE_PAIR		4
#define	ANON_FREE_COVERED_PAGE		5
#define	ANON_FREE_COVERED_PAGE_FREE	6
#define	ANON_CTOP_PAGE_MERGE		7
#define	ANON_CTOP_SWAP_MERGE		8
#define	ANON_UNLINK			9
#define	ANON_HOLD			10
#define	ANON_RELEASE			11
#define	ANON_INSERT			12
#define	ANON_MODIFY			13
#define	ANON_FLIP			14
#define	ANON_LOOKUP			15
#define	ANON_PFIND			16
#define	ANON_SETSIZE			17
#define	ANON_SETSWAP			18
#define	ANON_CLRSWAP			19
#define	ANON_REMOVE			20
#define	ANON_UNCACHE			21
#define	ANON_RECYCLE			22
#define	ANON_PAGEFREEANON		23
#define	ANON_PAGEFREESANON		24
#define	ANON_SWAPIN			25
#define	ANON_PAGEFREESANON1		26
#define	ANON_PGETSANON			27
#define	ANON_PSREMOVE			28
#define	ANON_MIGR			29
#define	ANON_PTOC_PAGE_MERGE		30
#define	ANON_PTOC_SWAP_MERGE		31
#define ANON_PAGEFREE_AND_CACHE		32
#define ANON_NEW			33

static char *anon_ops[] = {
	"none                   ",  
	"dup                    ",  
	"free                   ",  
	"collapse               ",
	"collapse_pair          ",  
	"free_covered_page      ",  
	"free_covered_free_page ",  
	"ctop page merge        ",
	"ctop swap merge        ",  
	"unlink                 ",  
	"hold                   ",  
	"release                ",
	"insert                 ",  
	"modify                 ",  
	"flip                   ",  
	"lookup                 ",
	"pfind                  ",  
	"setsize                ",  
	"setswap                ",  
	"clrswap                ",
	"remove                 ",  
	"uncache                ",  
	"recycle                ",  
	"pagefreeanon           ",
	"pagefreesanon          ",
	"swapin                 ",
	"pagefreesanon1         ",
	"pgetsanon              ",
	"psremove               ",
	"migr                   ",
	"ptoc page merge        ",
	"ptoc swap merge        ",  
	"anon pagefree and cache",
	"new                    ",
};


/*
 * Specify which ktrace events should be gathered.  Useful for keeping the
 * trace buffer from overflowing with uninteresting events.
 */

static char anon_ktrace_on[] = {
	0,	/* ANON_NONE			*/
	1,	/* ANON_DUP			*/
	1,	/* ANON_FREE			*/
	1,	/* ANON_COLLAPSE		*/
	1,	/* ANON_COLLAPSE_PAIR		*/
	0,	/* ANON_FREE_COVERED_PAGE	*/
	0,	/* ANON_FREE_COVERED_PAGE_FREE	*/
	0,	/* ANON_CTOP_PAGE_MERGE		*/
	0,	/* ANON_CTOP_SWAP_MERGE		*/
	1,	/* ANON_UNLINK			*/
	1,	/* ANON_HOLD			*/
	1,	/* ANON_RELEASE			*/
	1,	/* ANON_INSERT			*/
	0,	/* ANON_MODIFY			*/
	0,	/* ANON_FLIP			*/
	0,	/* ANON_LOOKUP			*/
	0,	/* ANON_PFIND			*/
	1,	/* ANON_SETSIZE			*/
	1,	/* ANON_SETSWAP			*/
	0,	/* ANON_CLRSWAP			*/
	0,	/* ANON_REMOVE			*/
	1,	/* ANON_UNCACHE			*/
	1,	/* ANON_RECYCLE			*/
	0,	/* ANON_PAGEFREEANON		*/
	0,	/* ANON_PAGEFREESANON		*/
	0,	/* ANON_SWAPIN			*/
	0,	/* ANON_PAGEFREESANON1		*/
	0,	/* ANON_PGETSANON		*/
	0,	/* ANON_PSREMOVE		*/
	1,	/* ANON_MIGR			*/
	0,	/* ANON_PTOC_PAGE_MERGE		*/
	0,	/* ANON_PTOC_SWAP_MERGE		*/
	0,	/* ANON_PAGEFREE_AND_CACHE	*/
	1,	/* ANON_NEW			*/
};

#define ANON_TRACE_ENTER(v0, v1, v2, v3)                \
	do {						\
	    if (anon_ktrace_on[v1])			\
                ktrace_enter(anon_ktrace,               \
                        (void *)(__psunsigned_t)v0,     \
                        (void *)(__psunsigned_t)v1,     \
                        (void *)(__psunsigned_t)v2,     \
                        (void *)(__psunsigned_t)v3,     \
                        (void *)(__psunsigned_t)lbolt,  \
                        (void *)__return_address,       \
                        (void *)(__psunsigned_t)current_pid(), \
			(void *)0, \
                        (void *)0 , (void *)0 , (void *) 0, (void *) 0, \
                        (void *)0 , (void *)0 , (void *) 0, (void *) 0);\
	} while (0)

#else
#define ANON_TRACE_ENTER(v0, v1, v2, v3)
#endif	/* ANON_TRACE */

void
init_anon(void)
{
	cnodeid_t cnode;
	sanon_list_head_t *salist;

	anon_zone = kmem_zone_init(sizeof(anon_t), "Anon structs");
	ASSERT(anon_zone);
	anon_lock_zone = kmem_zone_init(sizeof(anon_lock_t), "Anon locks");
	ASSERT(anon_lock_zone);

	/* init shared anon list */
	for (cnode=0; cnode<numnodes; cnode++) {
		salist = &(SANON_LIST_FOR_CNODE(cnode));
		makeempty(&salist->sal_listhead);
		init_spinlock(&salist->sal_lock, "sanon", cnode);
	}

	scache_init();
#ifdef	ANON_TRACE
	anon_ktrace = ktrace_alloc(4096,1);
	idbg_addfunc("traceanon", idbg_anon_trace);
	idbg_addfunc("traceanontime", idbg_anon_trace_time);
	idbg_addfunc("traceanonpid", idbg_anon_trace_pid);
#endif	/* ANON_TRACE */
}


/*
 * anon_new
 *
 * Return a handle to a new initialized anon struct.
 */

anon_t *
anon_new(void)
{
	anon_t *ap;

	ap = anon_alloc();
	ap->a_lock = (anon_lock_t *) kmem_zone_zalloc(anon_lock_zone,
							 KM_SLEEP);
	init_tree_lock(ap->a_lock);
	tree_lock_use_inc(ap->a_lock);		/* use count now 1 */

	ANON_TRACE_ENTER(ap, ANON_NEW, 0, 0);

	return ap;
}


/*
 * anon_alloc
 * 
 * Allocate and do basic anon node initialization.  All anon tree linkage
 * fields are initially NULL.  The caller is responsible for setting up the
 * the locks appropriately.
 */

static anon_t *
anon_alloc(void)
{
	anon_t *ap;

	ap = (anon_t *) kmem_zone_zalloc(anon_zone, KM_SLEEP);
	ap->a_refcnt = 1;
	ap->a_depth  = 1;
	ap->a_limit  = MAXPAGES;
	pcache_init(&ap->a_pcache);
	return ap;
}


/*
 * anon_dup
 *
 * Set up anon structs so that parent and child will share the existing
 * anonymous pages with copy-on-write while having all new anonymous pages be
 * unique to the process.  This is implemented by forming a binary
 * tree of anon structs (or adding a level to the existing tree if
 * the parent has forked before).  The parent's existing anon struct
 * becomes the root of a sub-tree and a new anon struct is allocated
 * for both the parent and child.  These new structs become children of
 * the old one.
 *
 * Returns the new anon struct for the child.  The parent's anon
 * handle is changed to the new struct.
 *
 */

anon_t *
anon_dup(anon_t **pap)	/* parent's anon pointer */
{
	anon_t	*cap;	/* child's anon pointer */
	anon_t	*opap;	/* parent's old anon pointer */
	anon_t	*npap;	/* parent's new anon pointer */
	void	*pcache_token;
	int	 locktoken;
	preempt_cookie_t	preempt_cookie;

	opap = *pap;
	ASSERT(opap->a_left == NULL && opap->a_right == NULL);

	/*
	 * Allocate the new leaves of the tree.  This needs to be done
	 * before acquiring the tree lock since anon_alloc may sleep while
	 * waiting for memory.  Likewise, see if the scache pool needs more
	 * memory since it may have to allocate some during the collapse
	 * operation.
	 */

	cap  = anon_alloc();
	npap = anon_alloc();

	ANON_TRACE_ENTER(*pap, ANON_DUP, cap, npap);

	scache_pool_reserve();

	locktoken = acquire_tree_update_lock(opap->a_lock);

	preempt_cookie.pc_lock      = opap->a_lock;
	preempt_cookie.pc_locktoken = locktoken;

	/*
	 * Before we put the new nodes into the tree, see we can clean
	 * up this branch a bit.
	 */

	opap = collapse(opap, (void *) &preempt_cookie);

	/*
	 * Now link the new leaves into the tree.
	 */

	cap->a_lock  = opap->a_lock;	/* copy lock ptrs */
	tree_lock_use_inc(opap->a_lock);

	npap->a_lock = opap->a_lock;
	tree_lock_use_inc(opap->a_lock);

	cap->a_depth  = opap->a_depth + 1;    /* tree is one level deeper now */
	npap->a_depth = cap->a_depth;

	/*
	 * Link nodes together to form a binary tree with the original
	 * node at the root.
	 */

	npap->a_parent	= opap;
	cap->a_parent	= opap;
	opap->a_left	= npap;
	opap->a_right	= cap;

	release_tree_lock(opap->a_lock, locktoken);
	*pap = npap;

	/*
	 * See if collapsing the tree caused the pcache at the old parent node
	 * to overflow the old hash table it was using.  If so, grow it.
	 */

	if ((pcache_token = pcache_getspace(&opap->a_pcache, 0)) != NULL) {
		locktoken = acquire_tree_update_lock(opap->a_lock);
		pcache_token = pcache_resize(&opap->a_pcache, pcache_token);
		release_tree_lock(opap->a_lock, locktoken);
		pcache_resize_epilogue(pcache_token);
	}

	/*
	 * Is tree getting too deep?  If so, check for cases where we
	 * can prune out some intermediate nodes.
	 */

	if (npap->a_depth > PRUNE_THRESHOLD)
		prune(npap);

	return cap;
}


/*
 * anon_free
 *
 * Disassociate region from its anonymous memory.  The given leaf anon
 * struct is removed from the tree.  The list of pages managed by the freed 
 * leaf node is also freed along with any associated swap space.  This operation
 * is repeated for the parent if it has no other children (and on to its parent)
 * since once the leaf is gone, nothing in this branch of the tree can be 
 * referenced again.
 *
 */

void
anon_free(anon_t *ap)
{
	anon_t		*next_ap;	/* parent's anon struct */
	anon_lock_t	*tree_lock;
	int		 locktoken;
	preempt_cookie_t preempt_cookie;

	ASSERT(ap->a_left == NULL && ap->a_right == NULL);

	tree_lock = ap->a_lock;
	locktoken = acquire_tree_update_lock(tree_lock);

	ANON_TRACE_ENTER(ap, ANON_FREE, 0, 0);
	preempt_cookie.pc_lock      = tree_lock;
	preempt_cookie.pc_locktoken = locktoken;

	/*
	 * We hold an extra reference on the tree lock while in this
	 * routine.  That way the lock won't disappear if we end up
	 * freeing the whole tree in the loop that follows, which in
	 * turn makes it simpler to handle the unlocking at the end of
	 * this function.
	 */

	tree_lock_use_inc(tree_lock);

	/*
	 * Walk the tree towards to the root, freeing all anon's in the
	 * path that have only a single child.
	 */

	while (ap) {
		next_ap = ap->a_parent;

		anon_unlink(ap, (void *) &preempt_cookie);
		ap = next_ap;

		/*
		 * Stop when we hit a node that has something on its other
		 * subtree.
		 */

		if (ap && (ap->a_left || ap->a_right)) {
			break;
		}
	}

	/*
	 * Give up our extra reference on the tree lock.  If we were the
	 * last reference, then the whole tree was freed above, so free the
	 * lock now.  If there are still references, then there are some
	 * nodes left somewhere, so just release the lock.
	 */

	tree_lock_use_dec(tree_lock);

	if (tree_lock_refcnt(tree_lock) == 0) {
		release_tree_lock(tree_lock, locktoken);
		destroy_bitlock(&tree_lock->l_un.l_all);
		kmem_zone_free(anon_lock_zone, (void *) tree_lock);

	} else
		release_tree_lock(tree_lock, locktoken);
}


/*
 * collapse
 *
 * Check to see if we need to collapse the tree.  This is done whenever
 * the given leaf node's parent has only one child (the given node itself).
 * Try to collapse as many levels out of the tree as possible.  We stop
 * as soon as we find a node with two children.  The caller must already
 * have acquired the tree update lock.  Returns the new leaf node.
 */

static anon_t * 
collapse(anon_t *ap, void *preempt_cookie)
{
	anon_t	*pap;

	ASSERT(ap->a_left == NULL && ap->a_right == NULL);  /* must be a leaf */

	ANON_TRACE_ENTER(ap, ANON_COLLAPSE, 0, 0);
	while (pap = ap->a_parent) {

		/*
		 * Can't collapse further if both children are present 
		 */

		if (pap->a_left && pap->a_right)
			break;

		/*
		 * Collapse ap and its parent, get the surviving node
		 */

		ap = collapse_pair(ap, preempt_cookie);
	}

	return ap;
}

/*
 * collapse_pair
 *
 * Given a child anon struct, collapse it with its parent node (the other
 * child has already been freed).  Return a pointer to the surviving node. 
 */

static anon_t *
collapse_pair(anon_t *cap, void *preempt_cookie)
{
	anon_t	*pap;		/* parent anon struct */
	anon_t	*from, *into;
	swap_merge_cookie_t cookie;
	void (*pfunc)(anon_t *, pfd_t *);
	void (*sfunc)(swap_merge_cookie_t *, pgno_t, sm_swaphandle_t);

	pap = cap->a_parent;

	/*
	 * Delete anything in the parent's cache that is beyond
	 * the child's limit.
	 */

	ANON_TRACE_ENTER(cap, ANON_COLLAPSE_PAIR, pap, 0);

	_anon_setsize(pap, cap->a_limit, preempt_cookie);

	if (pcache_pagecount(&cap->a_pcache) > 
				pcache_pagecount(&pap->a_pcache)) {
		from = pap;
		into = cap;
		pfunc = ptoc_page_merge;
		sfunc = ptoc_swap_merge;
	} else {
		from = cap;
		into = pap;
		pfunc = ctop_page_merge;
		sfunc = ctop_swap_merge;
	}

	/*
	 * Now merge any pages cached by the child into the parent.  Covered
	 * pages are discarded at this point.  Swap handles for the non-
	 * covered pages are moved at this time as well.
	 *
	 * With this algorithm, the node with more pages is the surviving node.
	 */

	if (scache_pool_pages_needed())
		anon_preempt(preempt_cookie);

	pcache_remove(&from->a_pcache, 0, MAXPAGES,
		     (void (*)(void *, struct pfdat *)) pfunc, into,
		     anon_preempt, preempt_cookie);

	/*
	 * Now do the same for any swap handles that didn't have the
	 * corresponding pages in memory.
	 */

	if (scache_pool_pages_needed())
		anon_preempt(preempt_cookie);

	cookie.into = into;
	cookie.preempt_cookie = preempt_cookie;

	scache_scan(&from->a_scache, REMOVE, 0, MAXPAGES,
		(void (*)(void *,pgno_t,sm_swaphandle_t)) sfunc, &cookie);

	if (scache_pool_pages_needed())
		anon_preempt(preempt_cookie);

	/*
	 * Everything of interest has been moved into the "into" at this
	 * point.  It's now safe to get rid of the "from" node.
	 */

	anon_unlink(from, preempt_cookie);

	if (into == cap) into->a_depth--;
	return into;
}



/*
 * Free the  page from the parent anon while collapsing the child with the
 * parent. If the child's page covers the parent's page free the parent's page.
 */

/* ARGSUSED */
static void
free_covered_page(anon_t *pap, struct pfdat *covered_pfd)
{

	nested_pfdat_lock(covered_pfd);

	ASSERT((covered_pfd->pf_flags & (P_HASH|P_ANON)) == (P_HASH|P_ANON));

	ANON_TRACE_ENTER(pap, ANON_FREE_COVERED_PAGE, pap, covered_pfd);

	covered_pfd->pf_flags &= ~(P_HASH|P_ANON|P_SWAP);
	covered_pfd->pf_tag    = NULL;


	/*
	 * A covered page can't be in use by a process, but could
	 * still be pattach'ed by the networking code.
	 */

	if (covered_pfd->pf_use == 0) {

		ANON_TRACE_ENTER(pap, ANON_FREE_COVERED_PAGE_FREE, pap, covered_pfd);

		/*
		 * The page is free.  If it's an sanon page, then
		 * there's no longer any need to swap it.  So take it
		 * off the sanon free list and put it on the regular
		 * free list.  If it's not an sanon page, then it's
		 * already on the free list.
		 */

		ADD_NODE_EMPTYMEM((pfdattocnode(covered_pfd)), 1);

		if (covered_pfd->pf_flags & P_SQUEUE) {
			psremove(covered_pfd);		
			pfdat_inc_usecnt(covered_pfd);
			nested_pfdat_unlock(covered_pfd);
			pagefree(covered_pfd);
			return;

		} else
			ASSERT(covered_pfd->pf_flags & 
					(P_QUEUE|P_RECYCLE));

	} else {

		/*
		 * There are two cases where someone will still have
		 * a reference count on a covered page.  The first
		 * is a pattach'ed page still in use by the networking
		 * code, and the second is when someone is doing a
		 * pagewait on this page.  The latter can happen when
		 * multiple sproc threads vfault on a swapped out 
		 * copy-on-write page.  One will do the swapin while
		 * the others wait in pagewait.  Before the waiters
		 * run again, the one that swapped the page in may
		 * pfault on it and break copy on write.  Now if that
		 * process forks and causes the anon node containing
		 * the old copy-on-write page and the other processes
		 * that were in pagewait haven't run yet, then we'll
		 * still see a reference count on the page.
		 *
		 * We don't need to do anything here except unhash it.  
		 * In the pattach case, the networking code will
		 * eventually free it up by calling prelease().  In 
		 * the pagewait case, the waiters will eventually call
		 * pagefreeanon() to release the page.  So eventually
		 * the reference count will go to zero and the page
		 * will go on the free list.
		 *
		 * It must be true that no process has this page
		 * mapped in its address space (since it's a covered
		 * page).  So we use the reverse maps to check for
		 * that.
		 */

		ASSERT((covered_pfd->pf_flags &
			(P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);

		ASSERT((covered_pfd->pf_flags & P_CW) ||
		((GET_RMAP_PDEP1(covered_pfd) == NULL)
			&& (covered_pfd->pf_pdep2 == NULL)));
	}

	nested_pfdat_unlock(covered_pfd);

}

/*
 * ctop_page_merge
 *
 * This is called to merge pages from a child anon node to its parent when
 * the two nodes are being collapsed.  Covered pages are not merged.  They
 * are unhashed and freed.  We will find covered pages in three situations.
 * First, a page may still be hashed to the parent node if it was swapped
 * at some point in the past, but not referenced in the meantime.  In this
 * case it will be sitting on the free list.  Similarly, a page may have
 * gone sanon at one point and may now be sitting on the sanon free list.  Finally,
 * a page may have been pattached and is still in use by the networking code.
 */

static void
ctop_page_merge(anon_t *pap, pfd_t *pfd)
{

	/*
	 * If the child node already has its own copy of this page,
	 * then we have a covered page.  So we need to get rid of
	 * the parent page by unhashing it.  If there's a swap handle for the
	 * parent page, then toss it as well.
	 */
	ANON_TRACE_ENTER(pap, ANON_CTOP_PAGE_MERGE, pfd, pfd->pf_pageno);

	pcache_remove(&(pap->a_pcache), pfd->pf_pageno, 1, 
		(void (*)(void *, struct pfdat *))free_covered_page, pap, 
				NULL, NULL);

	scache_clear(&(pap->a_scache), pfd->pf_pageno);

	add_noncovrd_page(pap, pfd);
}

/*
 * add_noncovrd_page
 *
 * This is called when a page has to be put into the "to" anon's
 * cache from the page's current cache, because the "to" anon 
 * does not have a copy of this lpn.
 */

static void
add_noncovrd_page(anon_t *to, pfd_t *pfd)
{
	anon_t	*from = (anon_t *)pfd->pf_tag;

	nested_pfdat_lock(pfd);

	ASSERT((pfd->pf_flags & (P_HASH|P_ANON)) == (P_HASH|P_ANON));

	/*
	 * The page is covered, so we need to move it to the
	 * "to" node.  If the page has a swap handle, then transfer
	 * it to the "to" as well.
	 */

	scache_scan(&from->a_scache, REMOVE, pfd->pf_pageno, 1,
		(void (*)(void *, pgno_t, sm_swaphandle_t)) scache_add,
		&to->a_scache);

	/*
	 * If the page is being recycled, just finish unhashing it and
	 * ignore it.
	 */

	if (pfd->pf_flags & P_RECYCLE) {
		ASSERT(pfd->pf_use == 0);
		ASSERT((pfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0);

		pfd->pf_flags &= ~(P_HASH|P_ANON|P_DONE|P_SWAP);
		pfd->pf_tag    = NULL;

	} else {
		pfd->pf_tag = (void *) to;
		pcache_insert(&to->a_pcache, pfd);
	}

	nested_pfdat_unlock(pfd);
}


/*
 * ctop_swap_merge
 *
 * This is called to merge any remaining swap handles in the child node
 * into the parent node during a collapse operation.  Any swap handle in
 * the child for which there isn't either a page or a swap handle in the
 * parent is transferred to the parent.
 */


static void
ctop_swap_merge(swap_merge_cookie_t *cookie, pgno_t lpn,
		sm_swaphandle_t swap_handle)
{
	anon_t *pap;

	pap = cookie->into;

	ANON_TRACE_ENTER(pap, ANON_CTOP_SWAP_MERGE, swap_handle, lpn);

	pcache_remove(&(pap->a_pcache), lpn, 1, 
		(void (*)(void *, struct pfdat *))free_covered_page, pap, 
				NULL, NULL);

	scache_clear(&(pap->a_scache), lpn);

	/*
	 * The parent doesn't have this page yet.  So transfer the
	 * swap handle to it.
	 */

	scache_add(&pap->a_scache, lpn, swap_handle);
	anon_preempt(cookie->preempt_cookie);
}

/*
 * ptoc_page_merge
 *
 * This is called to merge pages from a parent anon node to its child when
 * the two nodes are being collapsed.  Covered pages are not merged.  They
 * are unhashed and freed.  We will find covered pages in three situations.
 * First, a page may still be hashed to the parent node if it was swapped
 * at some point in the past, but not referenced in the meantime.  In this
 * case it will be sitting on the free list.  Similarly, a page may have
 * gone sanon at one point and may now be sitting on the sanon free list.  Finally,
 * a page may have been pattached and is still in use by the networking code.
 */

static void
ptoc_page_merge(anon_t *cap, pfd_t *pfd)
{

	ANON_TRACE_ENTER(cap, ANON_PTOC_PAGE_MERGE, pfd, pfd->pf_pageno);

	/*
	 * If the child node already has its own copy of this page,
	 * then we have a covered page.  So we need to get rid of
	 * this page by unhashing it.  If there's a swap handle for the
	 * page, then toss it as well.
	 */
	if (pcache_find(&cap->a_pcache, pfd->pf_pageno) ||
	    scache_lookup(&cap->a_scache, pfd->pf_pageno)) {
		scache_clear(&((anon_t *)(pfd->pf_tag))->a_scache, 
				pfd->pf_pageno);
		free_covered_page(cap->a_parent, pfd);
	} else {
		add_noncovrd_page(cap, pfd);
	}

	return;
}


/*
 * ptoc_swap_merge
 *
 * This is called to merge any remaining swap handles in the parent node
 * into the child node during a collapse operation.  Any swap handle in
 * the parent for which there isn't either a page or a swap handle in the
 * child is transferred to the child.
 */

static void
ptoc_swap_merge(swap_merge_cookie_t *cookie, pgno_t lpn,
		sm_swaphandle_t swap_handle)
{
	anon_t *cap;

	cap = cookie->into;

	ANON_TRACE_ENTER(cap, ANON_CTOP_SWAP_MERGE, swap_handle, lpn);

	/*
	 * If the child node already has its own copy of this page,
	 * then we have a covered page.  So we need to get rid of
	 * the swap handle by calling sm_free.  The handle has already
	 * been removed from the parent's cache at this point.
	 */

	if (pcache_find(&cap->a_pcache, lpn) ||
	    scache_lookup(&cap->a_scache, lpn)) {
		sm_free(&swap_handle, 1);

	} else {
		/*
		 * The child doesn't have this page yet.  So transfer the
		 * swap handle to it.
	 	 */

		scache_add(&cap->a_scache, lpn, swap_handle);
	}

	anon_preempt(cookie->preempt_cookie);
}


/*
 * prune
 *
 * Processes that continually fork new children before their older children
 * exit or exec generate an anon tree that grows without bounds.  This
 * happens because the collapse code can only merge nodes that are on
 * a private branch of a tree (that starts from a leaf).  (Merging shared
 * nodes cannot be done since each process uses its own region lock while
 * faulting and doing other VM activities.)  If the process forking
 * happens to be a long running server, then performance continually
 * degrades as anon_lookup operations have to search more and more levels
 * (and memory is continually consumed by the growing tree as well).
 * Therefore, anon_dup detects deep trees and calls here to try and
 * prune out some old nodes.
 *
 * An easy case to handle is a node that has no pages hashed to it,
 * has no swap handles, and only one child.  These coniditions guarantee
 * that no one will miss this node if we get rid of it.  So search the given
 * branch of the tree for such nodes and delete them.
 *
 * No other process can be allowed to search the tree while we are
 * deleting nodes.
 *
 * As we go through the tree, we reset all the depth hints in our branch
 * to one, since we don't want to prune this branch again for awhile.
 */

static void
prune(anon_t *ap)
{
	anon_t	*last_ap;
	int	 locktoken;

	locktoken = acquire_tree_update_lock(ap->a_lock);

	ap->a_depth = 1;
	last_ap = ap;		/* skip the leaf we were given, caller */
	ap = ap->a_parent;	/* still wants it		       */

	while (ap) {
		ap->a_depth = 1;

		/*
		 * If node still has both children, has pages hashed
		 * to it, or has swap handles, then it's not a candidate
		 * for pruning.
		 */

		if ((ap->a_left && ap->a_right) || 
		     pcache_pagecount(&ap->a_pcache) != 0 || 
		     scache_has_swaphandles(&ap->a_scache)) {
			last_ap = ap;
			ap = ap->a_parent;
			continue;
		}

		/*
		 * It's safe to nuke this node.
		 */

		anon_unlink(ap, (void *) NULL);
		ap = last_ap->a_parent;
	}

	release_tree_lock(last_ap->a_lock, locktoken);
}


/*
 * anon_unlink
 *
 * Unlink and free the given node from the tree.  If unlinking a non-leaf node, 
 * it must have only one child.  The tree must be locked for update by 
 * the caller.  The node being unlinked may still have pages in its cache.
 * This can happen in several cases: when there are free pages still around
 * for this node (which may either on the free list or the sanon free list) or
 * when another subsystem acquires a reference to one of our pfdats (such
 * as the migration code or future page out daemon).  We remove everything
 * we can from the free lists and if there are still referenced pages left
 * then we postpone the free'ing of this anon struct until the references
 * all go away.
 */

static void
anon_unlink(anon_t *ap, void *preempt_cookie)
{
	anon_t	*cap;
	preempt_cookie_t	*cookie;

	ANON_TRACE_ENTER(ap, ANON_UNLINK, ap->a_parent, 0);

	ASSERT(preempt_cookie || pcache_pagecount(&ap->a_pcache) == 0);

	/*
	 * Unhash all pages belonging to this anon node.  Since the
	 * anon node is being destroyed, we know these pages aren't in
	 * any process address space anymore.  They could be on the free
	 * list or the sanon free list.  There might also be some pages in
	 * the P_RECYCLE state.  Pages that were on the free list stay
	 * there.  Pages on the sanon are freed.
	 */

	pcache_remove(&ap->a_pcache, 0, MAXLPN,
		     (void (*)(void *, struct pfdat *)) psanonfree, NULL,
		     anon_preempt, preempt_cookie);
	ASSERT(pcache_pagecount(&ap->a_pcache) == 0);

	/*
	 * Since no process can access this node anymore, no one will ever
	 * try to swap in one of the node's pages.  Therefore, we can discard
	 * all swap handle info at this point.  Since this node is no longer
	 * visible anymore, we can drop the lock while in scache_release().
	 */

	cookie = (preempt_cookie_t *)preempt_cookie;

	if (cookie)
		release_tree_lock(cookie->pc_lock, cookie->pc_locktoken);

	pcache_release(&ap->a_pcache);
	scache_release(&ap->a_scache);

	if (cookie)
		cookie->pc_locktoken = acquire_tree_update_lock(cookie->pc_lock);

	/*
	 * Now, take this node out of the anon tree.  We do this last
	 * so that two children going through anon_free/anon_dup at the
	 * same time don't race when they both hit a common parent
	 * node.
	 */

	if (cap = ap->a_left) {
		ASSERT(ap->a_right == NULL);
		cap->a_parent = ap->a_parent;

	} else if (cap = ap->a_right) {
		ASSERT(ap->a_left == NULL);
		cap->a_parent = ap->a_parent;
	} else
		cap = NULL;

	if (ap->a_parent)
		if (ap->a_parent->a_left == ap)
			ap->a_parent->a_left = cap;

		else {
			ASSERT(ap->a_parent->a_right == ap);
			ap->a_parent->a_right = cap;
		}

	/*
	 * Make sure an unlinked node can be freed later. sched unlinks
	 * nodes but makes sure they hang around till victim process exit.
	 */

	ap->a_parent = NULL;

	/*
	 * Now that all the node's pages are gone, drop the anon tree's
	 * reference to this node.
	 */

	anon_release(ap);
}


/*
 * anon_hold
 *
 * Hold a reference to given anon node.  The caller must be holding the
 * pfdat lock for a page hashed to this anon node.  Bump the anon node
 * reference count to reflect the hold. Sched calls this too.
 */

void
anon_hold(anon_t *ap)
{
	ASSERT((pcache_pagecount(&ap->a_pcache) > 0) || 
					(curthreadp == schedthread));
	ASSERT(ap->a_refcnt >= 1);

	ANON_TRACE_ENTER(ap, ANON_HOLD, 0, 0);
	atomicAddInt(&ap->a_refcnt, 1);
}


/*
 * anon_release
 *
 * Decrement the reference count on this anon node and if it's zero,
 * free up all remaining resources associated with anonymous node.  At this
 * point, the caller ensures that all physical pages have been disassociated
 * and the swap handle cache has already been released.  Normally, the
 * anon node's reference will be one when this routine is called.  Occassionally
 * the destruction of an anon node may be racing with someone trying to
 * recycle one of its pages off the free list or trying to migrate one of its
 * pages.  Those operations grab an additional reference to the anon node
 * to prevent it from disappearing while they are waiting for the anon tree
 * lock.  The page they are using may be unhashed while they wait for the
 * lock, but the anon node will persist until they're done.
 *
 * To simplify the code, the locking convention dictates that the tree lock
 * reference count will never go to zero while in this routine.  The caller
 * guarantees there is at least one extra reference left so that the routine
 * that originally acquired the lock can either release it or free it when
 * we pop our way back out.
 */

static void
anon_release(anon_t *ap)
{

	ASSERT(ap->a_refcnt >= 1);

	/*
	 * The reference count on an anon node only goes to zero once (it
	 * goes to zero and stays there).  So we can compare the reference
	 * count to zero without worrying about races.  Just return if the
	 * node still has references to it.
	 */

	ANON_TRACE_ENTER(ap, ANON_RELEASE, ap->a_refcnt, 0);

	if (atomicAddInt(&ap->a_refcnt, -1) >= 1)
		return;

	/*
	 * The last reference to this node is now gone.  All the pages this
	 * node had should be gone now.
	 */

	ASSERT(pcache_pagecount(&ap->a_pcache) == 0);

	/*
	 * Give up this node's reference on the tree lock.  The reference
	 * count never goes to zero here since either (a) a previous routine
	 * in the call stack has taken an extra reference on the lock, or
	 * (b) a previous routine ensures we aren't releasing the last node
	 * in the tree.  So we ASSERT the ref count is still non-zero.
	 */

	tree_lock_use_dec(ap->a_lock);

	ASSERT(tree_lock_refcnt(ap->a_lock) > 0);

#if DEBUG
	/*
	 * Set all the pointer fields to a bogus value to try and catch
	 * someone who might still have a reference to this anon struct.
	 */

#if _MIPS_SIM == _ABI64
	ap->a_parent  = (anon_t *)      0xdeaddeaddeadcfcfL;
	ap->a_left    = (anon_t *)      0xdeaddeaddeadcfcfL;
	ap->a_right   = (anon_t *)      0xdeaddeaddeadcfcfL;
	ap->a_lock    = (anon_lock_t *) 0xdeaddeaddeadcfcfL;
#else
	ap->a_parent  = (anon_t *)      0xdeadcfcf;
	ap->a_left    = (anon_t *)      0xdeadcfcf;
	ap->a_right   = (anon_t *)      0xdeadcfcf;
	ap->a_lock    = (anon_lock_t *) 0xdeadcfcf;
#endif

#endif

	kmem_zone_free(anon_zone, (void *) ap);
	return;
}


/*
 * anon_insert
 *
 * Add the given page to list of pages managed by the given anon structure.
 * Newly inserted pages may cover pages that appear lower in the tree.  The
 * swap space for these will eventually be freed by the "shake the tree"
 * operation, during a collapse operation, or when the tree is freed.
 */

void
anon_insert(anon_t *ap, pfd_t *pfd, pgno_t lpn, int pdoneflag)
{
	int	 locktoken;
	void	*pcache_token;

	ASSERT(ap->a_right == NULL && ap->a_left == NULL); /* must be a leaf */
	ASSERT(pfd->pf_use == 1);
	ASSERT((pfd->pf_flags &
		(P_HASH|P_SWAP|P_QUEUE|P_SQUEUE|P_RECYCLE|P_CW)) == 0);
	ASSERT(pfd->pf_tag == NULL);

	ANON_TRACE_ENTER(ap, ANON_INSERT, lpn, pfd);

	pcache_token = pcache_getspace(&ap->a_pcache, 1);

	if (pcache_token) {
		locktoken = acquire_tree_update_lock(ap->a_lock);
		pcache_token = pcache_resize(&ap->a_pcache, pcache_token);
		release_tree_lock(ap->a_lock, locktoken);
		pcache_resize_epilogue(pcache_token);
	}

	locktoken = acquire_tree_update_lock(ap->a_lock);

	ASSERT(pcache_find(&ap->a_pcache, lpn) == NULL);   /* no dups allowed */

	nested_pfdat_lock(pfd);
	pfd->pf_tag    = ap;
	pfd->pf_pageno = lpn;
	pfd->pf_flags |= P_ANON | P_HASH | pdoneflag;
	nested_pfdat_unlock(pfd);

	pcache_insert(&ap->a_pcache, pfd);
	release_tree_lock(ap->a_lock, locktoken);

	return;
}


/*
 * anon_modify
 *
 * The given page is being modified, decide whether to steal or copy the
 * page.  For the steal case throw away any swap information for the page
 * and rehash it to the top level if necessary.
 */

anon_modify_ret_t
anon_modify(anon_t *ap, pfd_t *pfd, int sanon_region)
{
	pgno_t	lpn;
	int	locktoken;
	int	top_level, refgtone;
	void	*pcache_token;

	ASSERT(ap->a_right == NULL && ap->a_left == NULL); /* must be a leaf */
	ASSERT(((pfd->pf_flags &
			(P_HASH|P_DONE|P_QUEUE|P_SQUEUE|P_RECYCLE)) ==
			(P_HASH|P_DONE)) ||
		/* truncated vnode page, bug 536543 */
		((pfd->pf_flags & (P_ANON|P_BAD)) == P_BAD));

	lpn = pfd->pf_pageno;
	ANON_TRACE_ENTER(ap, ANON_MODIFY, lpn, pfd);
	top_level = pfd->pf_tag == ap;

	/*
	 * If we're going to re-hash the page, see if the leaf anon cache
	 * needs more space.  We can do this check without locking since
	 * it doesn't matter if we needless grow the page cache once in
	 * a while.  Even if we don't re-hash the page here, the odds are
	 * the caller will do a copy-on-write and then anon_insert the new
	 * page.  So we'll use the space then.
	 */

	if (!top_level) {
		pcache_token = pcache_getspace(&ap->a_pcache, 1);

		if (pcache_token) {
			locktoken = acquire_tree_update_lock(ap->a_lock);
			pcache_token = pcache_resize(&ap->a_pcache,
						     pcache_token);
			release_tree_lock(ap->a_lock, locktoken);
			pcache_resize_epilogue(pcache_token);
		}
	}

	locktoken = acquire_tree_update_lock(ap->a_lock);

	/*
	 * It is assumed that the caller has held the pfdat, so decide
	 * if there is only one user of the pfdat or multiple users.
	 */

	if (pfdat_held_musers(pfd))
		refgtone = 1;		/* greater than one user */
	else
		refgtone = 0;		/* only one user	 */


	/*
	 * Decide whether page should be copied or stolen.  We must hold
	 * the tree lock for update while we do this since we don't want
	 * another process to pfind a page once we make the decision to
	 * steal it.
	 *
	 * We must copy the page under the following conditions:
	 *
	 *	- page not anonymous yet.  This happens when modifying a
	 *	  vnode page from a MAP_PRIVATE mapping.
	 *
	 *	- page is not top level and other processes are still
	 *	  sharing the page.  This is the basic copy-on-write case
	 *	  for data/bss/stack in forked processes.
	 *
	 *	- page is not top level and this is an sanon region and
	 *	  the page hasn't been swapped yet.  With sanon regions,
	 *	  there may be more processes that logically have references
	 *	  to the page but don't currently have pmap entries for it.
	 *	  So the ref count on the pfdat won't tell us this.  If the
	 *	  page hasn't been swapped yet, then we always copy these
	 *	  pages so that these other processes with logical references
	 *	  will still have the original copy around to reference.
	 *	  Once the page is swapped, we can steal it since the other
	 *	  processes can swap the old copy back in whenever they need
	 *	  it.
	 *
	 *	- page has been pattach'ed (P_CW) and the ref count is
	 *	  greater than one implying the networking code is still
	 *	  using the page.  In this manner, the P_CW flag forces
	 *	  a copy even if the page is already top level.
	 *
	 *	- page is already on swap and the reference count is one
	 *	  and extra memory available and the page isn't top level.
	 *	  We copy these pages even though we could steal them.  This
	 *	  saves us having to do disk I/O when the other process
	 *	  logically sharing the page references it.  This tends to
	 *	  happen with shells where they get a dirty page swapped
	 *	  out that only the child process will ever touch again.
	 *	  Without this heuristic, each child would have to swap the
	 *	  page in, wasting disk I/O.  So we basically cache a copy
	 *	  of the page if we have enough memory.
	 *
	 * In all other cases, we steal the page since this process has the
	 * only reference.
	 */

	if ((pfd->pf_flags & P_ANON) == 0 ||
	    (!top_level && (refgtone ||
			   (sanon_region && !(pfd->pf_flags & P_SWAP)))) ||
	    ((pfd->pf_flags & P_CW) && refgtone) ||
	    ((pfd->pf_flags & P_SWAP) && (pfd->pf_rawcnt == 0) &&
		!refgtone &&
		GLOBAL_FREEMEM() > tune.t_gpgshi &&
		!top_level)) {

		/*
		 * Caller will copy the page and anon_insert it.
		 */

		release_tree_lock(ap->a_lock, locktoken);
		return COPY_PAGE;
	}

	/*
	 * Steal the page since no one else is using it.  Non-copy-on-write
	 * pages (i.e., shared memory) also come through here.
	 */

	scache_clear(&ap->a_scache, lpn);

	nested_pfdat_lock(pfd);

	if (pfd->pf_tag != ap) {

		/*
		 * If there's a swap handle in the pfd, then it belongs
		 * to node the pfd is currently hashed to.  So save it
		 * in that node since we're about to move the page to
		 * the leaf node.
		 */

		if (pfd->pf_swphdl) {
			ASSERT(pfd->pf_flags & P_SWAP);
			ASSERT(scache_lookup(&((anon_t *)pfd->pf_tag)->a_scache,
						 lpn) == NULL);

			scache_add(&((anon_t *)pfd->pf_tag)->a_scache, lpn,
					pfd->pf_swphdl);
			pfd->pf_swphdl = NULL;
		}

		pcache_remove_pfdat(&((anon_t *)pfd->pf_tag)->a_pcache, pfd);
		ASSERT(pcache_find(&ap->a_pcache, lpn) == NULL); /* no dups */
		pcache_insert(&ap->a_pcache, pfd);
		pfd->pf_tag = ap;

	/*
	 * Otherwise the page is already in a leaf anon node and it's about
	 * to be modified.  Therefore, any swap handle saved in the pfdat
	 * is no longer valid.  So toss it.  Note that any swap handle
	 * for this page in the scache was tossed above.
	 */

	} else if (pfd->pf_swphdl) {
		ASSERT(pfd->pf_flags & P_SWAP);
		sm_free(&pfd->pf_swphdl, 1);
		pfd->pf_swphdl = NULL;
	}

	pfd->pf_flags &= ~(P_SWAP|P_CW);
	nested_pfdat_unlock(pfd);
	release_tree_lock(ap->a_lock, locktoken);

	/*
	 * We may have done an scache_add() above which might have taken
	 * a page from the scache pool.  Replenish it now if needed.
	 */

	scache_pool_reserve();

	return PAGE_STOLEN;
}


/*
 * anon_swapin
 *
 * The specified page has been/will be swapped in.  Insert the page into the
 * page cache for this anon tag.
 */

anon_swapin_ret_t
anon_swapin(anon_t *ap, pfd_t *pfd, pgno_t lpn)
{
	int	 locktoken;
	void	*pcache_token;

	ASSERT((pfd->pf_flags &
		(P_HASH|P_ANON|P_SWAP|P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);
	ASSERT(pfd->pf_tag == NULL);

	ANON_TRACE_ENTER(ap, ANON_SWAPIN, lpn, pfd);
	pcache_token = pcache_getspace(&ap->a_pcache, 1);

	if (pcache_token) {
		locktoken = acquire_tree_update_lock(ap->a_lock);
		pcache_token = pcache_resize(&ap->a_pcache, pcache_token);
		release_tree_lock(ap->a_lock, locktoken);
		pcache_resize_epilogue(pcache_token);
	}

	locktoken = acquire_tree_update_lock(ap->a_lock);

	if (pcache_find(&ap->a_pcache, lpn) != NULL) {
		release_tree_lock(ap->a_lock, locktoken);
		return DUPLICATE;
	}

	nested_pfdat_lock(pfd);
	pfd->pf_tag    = ap;
	pfd->pf_pageno = lpn;
	pfd->pf_flags |= P_ANON | P_HASH | P_SWAP;
	nested_pfdat_unlock(pfd);
	pcache_insert(&ap->a_pcache, pfd);
	release_tree_lock(ap->a_lock, locktoken);

	return SUCCESS;
}

/*
 * anon_pagemigr
 *
 * Check whether the page represented by old_pfd is migratable and
 * if so, copy its state to new_pfd, remove it from the pcache,
 * and insert new_pfd.
 */

int
anon_pagemigr(anon_t *ap, pfd_t *old_pfd, pfd_t *new_pfd)
{
	int locktoken;
        int errcode;
        anon_lock_t *tree_lock;
        
        ASSERT(ap && old_pfd && new_pfd);
        
        tree_lock = ap->a_lock;
	locktoken = acquire_tree_update_lock(tree_lock);

	/*
	 * We hold an extra reference on the tree lock while in this
	 * routine.  That way the lock won't disappear if we end up
	 * freeing the last node in the tree when we call anon_release.
	 * This makes it simpler to handle the unlocking at the end of
	 * this function.
	 */
	tree_lock_use_inc(tree_lock);
        
	nested_pfdat_lock(old_pfd);

	ANON_TRACE_ENTER(ap, ANON_MIGR, old_pfd, new_pfd);
        /*
         * Has the memory object remained constant?
         */
        if (pfd_getmo(old_pfd) != ap) {
                PRINTF(("[anon_pagemigr]: memory object mutation\n"));
                errcode = MIGRERR_MOMUTATION;
                goto migr_fail0;
        }

        /*
         * At this point we've obtained the anon_tree lock
         * and the old_pfdat lock in the right order, with
         * the pfdat still being owned by the same memory
         * object we detected in the migration engine (this
         * function's caller).
         */

        /*
         * Check whether the page is in a migratable state
         */
        if ((errcode = MIGR_PFDAT_XFER(old_pfd, new_pfd)) != 0) {
                PRINTF(("[anon_pagemigr]: failed pfdat_xfer\n"));
                goto migr_fail0;
        }


        /*
         * The old page is *not* freed; it's just removed from the cache
         * The pf_pageno field in this pfdat must be intact.
         */
	pcache_remove_pfdat(&ap->a_pcache, old_pfd);
        /*
         * We're completely done with the old pfdat.
         * Release its pfdat lock
         */
        nested_pfdat_unlock(old_pfd);

        /*
         * Now we insert the new pfdat into the pcache
         */
       	pcache_insert(&ap->a_pcache, new_pfd);

	/*
	 * Copy over the swap handle if there is one in the pfd.
	 */

	if (old_pfd->pf_swphdl) {
		ASSERT(new_pfd->pf_flags & P_SWAP);
		ASSERT(scache_lookup(&((anon_t *)new_pfd->pf_tag)->a_scache,
					new_pfd->pf_pageno) == NULL);

		new_pfd->pf_swphdl = old_pfd->pf_swphdl;
		old_pfd->pf_swphdl = NULL;
	}
        errcode = 0;
        goto migr_success;

        /*
         * Exit levels.
         */

  migr_fail0:
        nested_pfdat_unlock(old_pfd);

  migr_success:

        /*
	 * Drop the extra reference the caller acquired on the anon node. 
	 * This may cause the node ap points at to be freed, so we can't
	 * reference ap anymore after this call.
	 */

	anon_release(ap);

	/*
	 * Give up our extra reference on the tree lock.  If we were the
	 * last reference, then anon_release freed the last node in the tree,
	 * so free the lock now.  If there are still references, then there
	 * are some nodes left somewhere, so just release the lock.
	 */

	tree_lock_use_dec(tree_lock);

	if (tree_lock_refcnt(tree_lock) == 0) {
		release_tree_lock(tree_lock, locktoken);
		destroy_bitlock(&tree_lock->l_un.l_all);
		kmem_zone_free(anon_lock_zone, (void *) tree_lock);

	} else {
		release_tree_lock(tree_lock, locktoken);
        }
        
        
        return (errcode);
}


/*
 * anon_flip
 *
 * Exchange old_pfd for new_pfd in the page cache.  Any swap space associated
 * with the old page is freed.  The flip can only be made if the page old
 * page belongs to a leaf anon node.  The caller is responsible for ensuring
 * this.
 */

void
anon_flip(pfd_t *old_pfd, pfd_t *new_pfd)
{
	anon_t	*ap;
	pgno_t	lpn;
	int	locktoken;
        /*REFERENCED*/
        pfmsv_t pfmsv;

        ASSERT(pfdat_held_oneuser(old_pfd));
	ASSERT((old_pfd->pf_flags &
		(P_HASH|P_ANON|P_DONE|P_QUEUE|P_SQUEUE|P_RECYCLE)) ==
		(P_HASH|P_ANON|P_DONE));

        ASSERT(new_pfd->pf_use == 1);
	ASSERT((new_pfd->pf_flags &
		(P_HASH|P_ANON|P_DONE|P_SWAP|P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);

        /*
         * When pflipping, we also need to transfer the pfms state
         * (That is the migration state)
         * In addition, we need to transfer the holding state.
         * The old_pfd must have been set to a pfdat_hold
         * state when pflip started. This state has to
         * reset for the old_pfd, and set for the new_pfd.
         */

        
	/*
	 * Assumption: the anon node that old_pfd is referring to can't go
	 * away while we're in here.  This is because the caller is just
	 * flipping one page for another and is logically still holding
	 * a reference to anon.  (Basically, the region points to this anon
	 * and the region lock is held right now.)
	 */

	ap = old_pfd->pf_tag;
	lpn = old_pfd->pf_pageno;

	ANON_TRACE_ENTER(ap, ANON_FLIP, old_pfd, new_pfd);

	ASSERT(ap->a_left == NULL && ap->a_right == NULL);

	locktoken = acquire_tree_update_lock(ap->a_lock);

	/*
	 * Out with the old ...
	 */

	if ((old_pfd->pf_flags & P_SWAP) && old_pfd->pf_swphdl) {
		sm_free(&old_pfd->pf_swphdl, 1);
		old_pfd->pf_swphdl = NULL;
	}

	nested_pfdat_lock(old_pfd);
	old_pfd->pf_tag    = NULL;
	old_pfd->pf_flags &= ~(P_SWAP | P_ANON | P_HASH | P_DONE);
	old_pfd->pf_flags |= P_DUMP;
        pfmsv = PFLIP_PFMS_GET(old_pfd);
	nested_pfdat_unlock(old_pfd);
	scache_clear(&ap->a_scache, lpn);
	pcache_remove_pfdat(&ap->a_pcache, old_pfd);
        pfdat_release(old_pfd);
        
	/*
	 * In with the new ...
	 */

	nested_pfdat_lock(new_pfd);
	new_pfd->pf_tag    = ap;
	new_pfd->pf_pageno = lpn;
	new_pfd->pf_flags |= P_ANON | P_HASH | P_DONE;
        PFLIP_PFMS_SET(new_pfd, pfmsv);
	nested_pfdat_unlock(new_pfd);

        pfdat_hold(new_pfd);
	pcache_insert(&ap->a_pcache, new_pfd);
	release_tree_lock(ap->a_lock, locktoken);
        
}


/*
 * anon_lookup
 *
 * Traverse the list of anon structs starting at the given leaf node
 * and moving towards the root until the specified lpn is found.  If
 * found, return the address of the associated anon struct as the cache key
 * along with any swap information for the page.  Return zero if not found.
 */

void *
anon_lookup(anon_t *ap, pgno_t lpn, sm_swaphandle_t *sp)
{
	anon_t	*start_ap;
	pfd_t	*pfd;
	int	 locktoken;

	/*
	 * Traverse the path from the leaf to the parent looking for the lpn.
	 */

	start_ap  = ap;
	locktoken = acquire_tree_access_lock(start_ap->a_lock);

	while (ap) {
		pfd = pcache_find(&ap->a_pcache, lpn);
		*sp = scache_lookup(&ap->a_scache, lpn);

		if (pfd) {
			nested_pfdat_lock(pfd);

			/*
			 * If the page isn't on the free list, it may have
			 * a swap handle in it.  So pick it up if it's
			 * there.  P_SQUEUE pages are waiting to be swapped,
			 * and so never have swap handles in them.
			 */

			if ((pfd->pf_flags & (P_QUEUE|P_SQUEUE)) == 0 && 
			     pfd->pf_swphdl != NULL) {
				ASSERT(pfd->pf_flags & P_SWAP);
				ASSERT(*sp == NULL);

				*sp = pfd->pf_swphdl;
			}

			nested_pfdat_unlock(pfd);
		}

		/*
		 * If we found a page that is currently being recycled,
		 * then there had better be a swap handle for the page
		 * (since there has to be a way to get the page back again
		 * off disk).  Since this interface doesn't return a pfd,
		 * there's no need to unhash P_RECYCLE pages here.
		 */

		if (pfd && pfd->pf_flags & P_RECYCLE)
			ASSERT(*sp);

		/*
		 * If we found either a pfd or a swap handle, then the anon
		 * page exists (either in memory, on swap, or both).  So our
		 * search is complete.  Return cache key and swap info for
		 * the page.
		 */

		if (pfd || *sp) {
			ANON_TRACE_ENTER(start_ap, ANON_LOOKUP, lpn, ap);
			release_tree_lock(ap->a_lock, locktoken);
			return (void *) ap;
		}

		/*
		 * Stop search early if page info in remaining nodes is stale.
		 */

		if (lpn >= ap->a_limit)
			break;

		ap = ap->a_parent;
	}

	/*
	 * Page not found in any parent node.
	 */

	release_tree_lock(start_ap->a_lock, locktoken);

	*sp = NULL;
	return NULL;
}


/*
 * anon_pfind
 *
 * Traverse the list of anon structs starting at the given leaf node
 * and moving towards the root until the specified lpn is found.  If
 * found, return the address of the associated anon struct as the cache key
 * along with any swap information for the page.  Return zero if not found.
 */

pfd_t *
anon_pfind(anon_t *ap, pgno_t lpn, void **id, sm_swaphandle_t *sp)
{
	anon_t	*start_ap;
	pfd_t	*pfd;
	int	locktoken;

	/*
	 * Traverse the path from the leaf to the parent looking for the lpn.
	 */

	start_ap  = ap;
	locktoken = acquire_tree_access_lock(start_ap->a_lock);


	while (ap) {
		pfd = pcache_find(&ap->a_pcache, lpn);
		*sp = scache_lookup(&ap->a_scache, lpn);

		/*
		 * If we found either a pfd or a swap handle, then the anon
		 * page exists (either in memory, on swap, or both).  So our
		 * search is complete.  Return cache key and swap info for
		 * the page.
		 */

		if (pfd || *sp) {

			if (pfd) {
				ASSERT((pfd->pf_flags & (P_ANON|P_HASH)) ==
						        (P_ANON|P_HASH));


				/*
				 * If the page is on the sanon free list, take
				 * it off since someone wants to use it.
				 */

				if (pfd->pf_flags & P_SQUEUE)
					psremove(pfd);

				/*
				 * If the page is on a free list, try to take
				 * it off.  We may be racing with pagealloc
				 * trying to recycle a page.  If pagealloc
				 * gets there first, then we lose.
				 */

				nested_pfdat_lock(pfd);

				if (pfd->pf_flags & P_QUEUE) {
					nested_pfdat_unlock(pfd);
					pageunfree(pfd);
					nested_pfdat_lock(pfd);
				}

				/*
				 * At this point, either pageunfree got the
				 * page off the free list and granted it to
				 * us, or the page is in the recycle state.
				 * Since we're holding the tree lock, the page
				 * can't proceed from the recycle state until
				 * we're done.
				 */

				if (pfd->pf_flags & P_RECYCLE) {

					/*
					 * If we hit a P_RECYCLE page, it means
					 * pagealloc has already acquired this 
					 * page off the free list and wants to
					 * recycle it to a new use.  When this
					 * race occurs, we let pagealloc win
					 * (it simplifies the locking).  So if
					 * we find this case, then unhash the
					 * page now so the caller can create
					 * and insert a new one.  This case
					 * only happens when we get the anon
					 * tree lock before pagealloc does and
					 * shouldn't happen very often.
					 */
	
					ASSERT(*sp && (pfd->pf_flags & P_SWAP));
					pfd->pf_tag    = NULL;
					pfd->pf_flags &=
						~(P_HASH|P_ANON|P_DONE|P_SWAP);
					nested_pfdat_unlock(pfd);
					pcache_remove_pfdat(&ap->a_pcache, pfd);
					pfd = NULL;

				} else {

					/*
					 * The page is ours.
					 */

					ASSERT((pfd->pf_flags & 
						(P_HASH|P_ANON|P_QUEUE|
						 P_SQUEUE|P_RECYCLE)) ==
						(P_HASH|P_ANON));

					pfdat_inc_usecnt(pfd);

					if (pfd->pf_swphdl) {
						ASSERT(pfd->pf_flags & P_SWAP);
						ASSERT(*sp == NULL);

						*sp = pfd->pf_swphdl;
					}

					nested_pfdat_unlock(pfd);
				}
			}

			release_tree_lock(ap->a_lock, locktoken);
			ANON_TRACE_ENTER(start_ap, ANON_PFIND, lpn, ap);

			*id = ap;
			return pfd;
		}

		/*
		 * Stop search early if page info in remaining nodes is stale.
		 */

		if (lpn >= ap->a_limit)
			break;

		ap = ap->a_parent;
	}

	/*
	 * Page not found in any parent node.
	 */

	release_tree_lock(start_ap->a_lock, locktoken);

	*sp = NULL;
	*id = NULL;
	return NULL;
}


/*
 * anon_setsize
 *
 * Set the maximum region size in the leaf anon struct so that stale
 * pages in the parent's node can no longer be referenced.  This is
 * used anytime the region is shrunk.
 */

void
anon_setsize(anon_t *ap, pgno_t newsize)
{
	int locktoken;
	preempt_cookie_t	preempt_cookie;

	locktoken = acquire_tree_update_lock(ap->a_lock);

	preempt_cookie.pc_lock      = ap->a_lock;
	preempt_cookie.pc_locktoken = locktoken;

	_anon_setsize(ap, newsize, (void *) &preempt_cookie);
	release_tree_lock(ap->a_lock, locktoken);
}


/*
 * _anon_setsize
 *
 * Internals of anon_setsize without the locking so it can be called by
 * code that already has the lock.
 */

static void
_anon_setsize(anon_t *ap, pgno_t newsize, void *preempt_cookie)
{

	ANON_TRACE_ENTER(ap, ANON_SETSIZE, newsize, 0);
	/*
	 * Go through the node's page and swap handle cache and get rid of
	 * any pages over the limit.  We're keeping pages [0..newsize-1]
	 * and discarding pages [newsize..MAXPAGES], so the count of pages
	 * we're discarding is MAXPAGES-newsize+1.
	 *
	 * Stale entries in the parent's node and beyond are eventually
	 * cleaned up by the "shake tree" operation or when all leaves go away.
	 * It's too hard to do this now since some of those pages could still
	 * be in use by other children and it's too time consuming to figure
	 * this out now.
	 */

	pcache_remove(&ap->a_pcache, newsize, MAXPAGES - newsize + 1,
		     (void (*)(void *, struct pfdat *)) psanonfree, NULL,
		     anon_preempt, preempt_cookie);
	scache_scan(&ap->a_scache, REMOVE, newsize, MAXPAGES - newsize + 1,
		      NULL, NULL);

	/*
	 * Can never raise the limit as this would potentially allow
	 * stale pages (from a previous shrink) in other nodes to be referenced.
	 */

	if (newsize < ap->a_limit)
		ap->a_limit = newsize;
}


/*
 * anon_getsizes
 *
 * Find the total number of anon pages and the highest apn in the given
 * range of anon pages.  Note that since the count is made by looking in
 * the anon's page cache, this routine must be called *before* deleting
 * things from the object's cache using pagefree.
 *
 * This code could be optimized a bit in the case where there's just a single
 * anon node with no parent (i.e., the process has never forked).  In this
 * case, one could make a single scan of the pcache and scache looking for 
 * the highest referenced page in the range.  This would require some new
 * pcache and scache routines but would cut down on the locking and function
 * call overhead for anon_lookup().  Currently this code is only used in
 * the rare path of AUTORESERV and AUTOGROW regions, and then only in non-
 * performance critical paths (like exit).  So we won't worry about this
 * for now.
 */

void
anon_getsizes(anon_t *ap, pgno_t start_apn, pgno_t len, pgno_t *anonpgs,
	      pgno_t *maxapn)
{
	pgno_t	apn, max, pgs, last_apn;
	sm_swaphandle_t	swap;

	ASSERT(ap->a_left == NULL && ap->a_right == NULL);    /* leaves only */

	max = -1;
	pgs = 0;
	last_apn = start_apn + len - 1;

	for (apn = start_apn; apn <= last_apn; apn++)
		if (anon_lookup(ap, apn, &swap) != NULL) {
			max = apn;
			pgs++;
		}

	if (anonpgs)
		*anonpgs = pgs;

	if (maxapn)
		*maxapn = max;
}


/*
 * anon_setswap_and_pagefree
 *
 * The given page has been swapped.  So store the swap handle and free
 * the page.  The swap handle is temporarily stored in the pfdat itself
 * until the last reference on the page goes away.  That way the page
 * can be immediately recycled into the scache pool if saving the
 * swap handle in the scache depleted it.
 *
 * There's a slight race with other processes doing anon_lookups on
 * shared anon nodes while we're inserting the new swap handle.  In
 * this case, they'll either get a NULL swap handle if they look
 * before we make the change or will get the new handle if they look
 * after.  In the first case, they'll do the lookup again if they can't
 * find the page in the cache otherwise it's guaranteed they'll get the
 * right handle since a swap handle in a shared anon can only be set
 * once (the page is read-only and will never be swapped out more than
 * once).
 */

void
anon_setswap_and_pagefree(pfd_t *pfd, sm_swaphandle_t swphdl, int do_pagedone)
{
	anon_t	*ap;
	pgno_t	lpn;
	int	locktoken;

	ap = (anon_t *) pfd->pf_tag;
	lpn = pfd->pf_pageno;

	ANON_TRACE_ENTER(ap, ANON_SETSWAP, lpn, swphdl);
	ASSERT((pfd->pf_flags & (P_HASH|P_ANON)) == (P_HASH|P_ANON));
	ASSERT(swphdl);

	locktoken = acquire_tree_update_lock(ap->a_lock);

	/*
	 * If we're swapping it out, it had better be in the cache...
	 */

	ASSERT(pcache_find(&ap->a_pcache, lpn) == pfd);

	/*
	 * An interior node of the tree only gets a swap handle
	 * assigned ONCE, otherwise vfault could swap in the wrong
	 * page!  A leaf node may get a new swap handle if the
	 * region is being shared wihtout copy-on-write (shmem).
	 * This happens when vhand swaps the page for one process
	 * while another still has write permission to the page in its
	 * pmap.  Vhand will swap it a second time for the other
	 * process.  So just free the old one if we have one.
	 */

	ASSERT((ap->a_left == NULL && ap->a_right == NULL) ||
	       (pfd->pf_swphdl == NULL &&
		scache_lookup(&ap->a_scache, lpn) == NULL));

	/*
	 * If we already had a swap handle saved in the pfd, toss it.  The
	 * new one supercedes it.  This happens for writable shared anon
	 * mappings because vhand swaps the page for each process.  Also,
	 * toss any swap handles that have already been stored in the
	 * scache.
	 */

	if (pfd->pf_swphdl) {
		ASSERT(pfd->pf_flags & P_SWAP);
		ASSERT(scache_lookup(&ap->a_scache, lpn) == NULL);
		sm_free(&pfd->pf_swphdl, 1);

	} else
		scache_clear(&ap->a_scache, lpn);

	/*
	 * Now save the new swap handle we were given.
	 */

	nested_pfdat_lock(pfd);
	pfd->pf_swphdl = swphdl;
	pfd->pf_flags |= P_SWAP;
	nested_pfdat_unlock(pfd);

	/*
	 * We shut off P_DONE when vhand swaps sanon pages to 
	 * prevent races with other processes reclaiming and 
	 * dirtying the page.  Now that swapping on the page
	 * is complete, call pagedone() to wake up anyone
	 * waiting for this page.  This must be done before
	 * freeing the page.
	 */

	if (do_pagedone) 
		pagedone(pfd);

	/*
	 * Allow for a preemption point between the "setswap" and "pagefree"
	 * portions of this routine.
	 */

	release_tree_lock(ap->a_lock, locktoken);
	delay_for_intr();
	locktoken = acquire_tree_update_lock(ap->a_lock);

	/*
	 * And drop vhand's reference on this page.
	 */

	_anon_pagefree_and_cache(pfd);

	release_tree_lock(ap->a_lock, locktoken);
}


/*
 * anon_pagefree_and_cache
 *
 * Decrement the reference count on an anonymous page.  If it goes to
 * zero, the page is retained in the page cache in case it's referenced
 * again the future.  This is different than what pagefreeanon() does
 * since it tosses the page when the last reference goes away.
 */

void
anon_pagefree_and_cache(pfd_t *pfd)
{
	/* REFERENCED */
	anon_t	*ap;
	int	locktoken;

	ap = pfd->pf_tag;

	locktoken = acquire_tree_update_lock(ap->a_lock);
	_anon_pagefree_and_cache(pfd);
	release_tree_lock(ap->a_lock, locktoken);
}


/*
 * _anon_pagefree_and_cache
 *
 * Internal guts of anon_pagefree_and_cache.  Assumes the caller is
 * handling all the locking.
 *
 * This routine decrements the reference count on the page.  If it
 * goes to zero, we try to keep the page in the page cache for future
 * use.  The only time we don't keep the page is when we need to recycle it
 * into the scache pool.  Note that if the last reference to the page
 * is going away, then the page must have previously been swapped.
 */

static void
_anon_pagefree_and_cache(pfd_t *pfd)
{
	anon_t	*ap;
	pgno_t	lpn;

	ASSERT((pfd->pf_flags & (P_HASH|P_ANON)) == (P_HASH|P_ANON));

	ap = pfd->pf_tag;

	ANON_TRACE_ENTER(ap, ANON_PAGEFREE_AND_CACHE, pfd, pfd->pf_pageno);

	nested_pfdat_lock(pfd);

	/*
	 * Are we giving up the last reference on the page?
	 */

	if (pfd->pf_use == 1) {
		nested_pfdat_unlock(pfd);

		/*
		 * Transfer any swap handle from the temporary location in
		 * the pfd to the permanent location in the scache.  Since
		 * the last use is going away, then the page must have
		 * been swapped.
		 */

		ASSERT(pfd->pf_flags & P_SWAP);

		lpn = pfd->pf_pageno;

		if (pfd->pf_swphdl) {

			/*
			 * Any exsiting swap handle in the scache was
			 * cleared in anon_setswap_and_pagefree().  Make
			 * sure of this before we add the new one.
			 */
		
			ASSERT(scache_lookup(&ap->a_scache, lpn) == NULL);

			scache_add(&ap->a_scache, lpn, pfd->pf_swphdl);
			pfd->pf_swphdl = NULL;

		} else
			ASSERT(scache_lookup(&ap->a_scache, lpn) != NULL);


		/*
		 * If we need more pages in the scache pool, then recycle
		 * this page right now.  Otherwise, just pagefree() it.
		 */

		if (scache_pool_pages_needed()) {
			pcache_remove_pfdat(&ap->a_pcache, pfd);

			pfd->pf_flags  = P_CACHESTALE;
			pfd->pf_tag    = NULL;
			pfd->pf_pageno = PGNULL;

			scache_pool_add(pfd);

		} else {
			pagefree(pfd);
		}

	} else {

		/*
		 * There are still more references to this page.  So all
		 * we have to do is to decrement the count.
		 */

		if (pfd->pf_flags & P_SWAP) {
			ASSERT(scache_lookup(&ap->a_scache,
				    	     pfd->pf_pageno) != NULL ||
			       pfd->pf_swphdl != NULL);
		}

		pfd->pf_use--;
		nested_pfdat_unlock(pfd);
	}
}

/*
 * anon_clrswap
 *
 * Remove a swap handle - used by unswap when deleteing swap
 * Replace it with the swaphandle passed in.
 */

void
anon_clrswap(void *key, pgno_t lpn, sm_swaphandle_t sh)
{
	anon_t *ap = key;
	pfd_t  *pfd;
	int	locktoken;

	ANON_TRACE_ENTER(ap, ANON_CLRSWAP, lpn, sh);
	locktoken = acquire_tree_update_lock(ap->a_lock);
	pfd = pcache_find(&ap->a_pcache, lpn);

	ASSERT(scache_lookup(&ap->a_scache, lpn) || (pfd && pfd->pf_swphdl));

	/*
	 * Check if we have a swap handle in the pfdat.  If so, then
	 * there's none for this page in the scache.
	 */

	if (pfd && pfd->pf_swphdl) {
		ASSERT((pfd->pf_flags & 
		       (P_HASH|P_ANON|P_SWAP|P_QUEUE|P_SQUEUE|P_RECYCLE)) ==
		       (P_HASH|P_ANON|P_SWAP));
		ASSERT(scache_lookup(&ap->a_scache, lpn) == NULL);

		/*
		 * Toss the old swap handle.
		 */

		sm_free(&pfd->pf_swphdl, 1);

		/*
		 * Save the new swap handle.  Note that 'sh' may be NULL.
		 */

		pfd->pf_swphdl = sh;

	} else {

		/*
		 * If there wasn't a swap handle in the pfdat, then there
		 * might be one in the scache.
		 */

		scache_clear(&ap->a_scache, lpn);
	
		if (sh)
			scache_add(&ap->a_scache, lpn, sh);
	}

	/*
	 * If there's no new swap handle, then any page in memory is no
	 * longer P_SWAP.
	 */

	if (pfd && sh == NULL)
		pfd_clrflags(pfd, P_SWAP);

	release_tree_lock(ap->a_lock, locktoken);
	return;
}

/*
 * anon_remove
 *
 * Remove the set of contiguous logical pages from the given anon structure.
 * Swap space is also freed.  Note that this routine does *not* search
 * the tree looking for a parent node that might have a reference to the
 * given lpn.  It assumes that all 'npgs' pages are recorded in the given
 * anon struct *only*.
 */

void
anon_remove(anon_t *ap, pgno_t lpn, pgno_t npgs)
{
	int	locktoken;

	locktoken = acquire_tree_update_lock(ap->a_lock);
	ANON_TRACE_ENTER(ap, ANON_REMOVE, lpn, npgs);
	pcache_remove(&ap->a_pcache, lpn, npgs,
		     (void (*)(void *, struct pfdat *)) psanonfree, NULL,
		     NULL, NULL);
	scache_scan(&ap->a_scache, REMOVE, lpn, npgs, NULL, NULL);
	release_tree_lock(ap->a_lock, locktoken);
}


/*
 * anon_uncache
 *
 * Remove the given page from the page cache.  Any swap handles are
 * left intact.  This call is used  when I/O fails on a
 * swap-in request or when a process is modifying a pattach'ed page
 * (so that the old copy of the page that the networking code is still
 * using becomes disassociated with the anonymous memory object).
 * The caller must ensure that the anon node can't be unlinked and that
 * the given page can't be diassociated from its anon node by someone else
 * while we're in here.
 */

void
anon_uncache(pfd_t *pfd)
{
	anon_t	*ap;
	int	 locktoken;

	ap = (anon_t *) pfd->pf_tag;

	ASSERT(ap->a_refcnt >= 1);
	ASSERT(pfd->pf_use > 0);
	ASSERT((pfd->pf_flags &
		(P_HASH|P_ANON|P_DONE|P_QUEUE|P_SQUEUE|P_RECYCLE)) ==
		(P_HASH|P_ANON|P_DONE));

	locktoken = acquire_tree_update_lock(ap->a_lock);
	nested_pfdat_lock(pfd);
	ANON_TRACE_ENTER(ap, ANON_UNCACHE, pfd, pfd->pf_flags);

	pfd->pf_flags &= ~(P_ANON|P_HASH|P_SWAP);
	pfd->pf_tag    = NULL;
	pcache_remove_pfdat(&ap->a_pcache, pfd);

	nested_pfdat_unlock(pfd);
	release_tree_lock(ap->a_lock, locktoken);
	return;
}


/*
 * anon_recycle
 *
 * Remove the given page from the page cache so that pagealloc can recycle
 * it to a new use.  Any swap handles are left intact.  It is assumed that
 * the caller has done an anon_hold on ap.  We release this extra reference
 * on the anon node before we return.  The pfdat lock must *not* be
 * held when this routine is called.  Note that we don't look at the tag
 * field of the pfdat to get the anon node.  That's because the page could
 * have been unhashed by someone else by the time we get in here.  So we
 * use the ap that the caller did the anon_hold on.
 */

void
anon_recycle(anon_t *ap, pfd_t *pfd)
{
	anon_lock_t	*tree_lock;
	int		 locktoken;

	ASSERT(ap);
	ASSERT(ap->a_refcnt >= 1);
	ASSERT(pfd->pf_use == 0);
	ASSERT((pfd->pf_flags & (P_RECYCLE|P_QUEUE|P_SQUEUE)) == P_RECYCLE);

	tree_lock = ap->a_lock;
	locktoken = acquire_tree_update_lock(tree_lock);

	ANON_TRACE_ENTER(ap, ANON_RECYCLE, pfd, pfd->pf_flags);
	/*
	 * We hold an extra reference on the tree lock while in this
	 * routine.  That way the lock won't disappear if we end up
	 * freeing the last node in the tree when we call anon_release.
	 * This makes it simpler to handle the unlocking at the end of
	 * this function.
	 */

	tree_lock_use_inc(tree_lock);

	/*
	 * A page in the P_RECYCLE state may have already been removed from
	 * the cache.  So only try to remove it if it's still in the cache.
	 */

	nested_pfdat_lock(pfd);

	if (pfd->pf_flags & P_ANON) {
		ASSERT(pfd->pf_tag == ap);
		ASSERT(scache_lookup(&ap->a_scache, pfd->pf_pageno) != NULL);
		ASSERT((pfd->pf_flags & (P_HASH|P_DONE|P_ANON|P_SWAP)) ==
				        (P_HASH|P_DONE|P_ANON|P_SWAP));

		pfd->pf_flags &= ~(P_HASH|P_DONE|P_ANON|P_SWAP);
		pfd->pf_tag    = NULL;
		pcache_remove_pfdat(&ap->a_pcache, pfd);

	} else {
		ASSERT((pfd->pf_flags & (P_HASH|P_SWAP)) == 0);
		ASSERT(pfd->pf_tag == NULL);
	}

	pfd->pf_flags &= ~P_RECYCLE;

	nested_pfdat_unlock(pfd);

	/*
	 * Drop the extra reference the caller acquired on the anon node. 
	 * This may cause the node ap points at to be freed, so we can't
	 * reference ap anymore after this call.
	 */

	anon_release(ap);

	/*
	 * Give up our extra reference on the tree lock.  If we were the
	 * last reference, then anon_release freed the last node in the tree,
	 * so free the lock now.  If there are still references, then there
	 * are some nodes left somewhere, so just release the lock.
	 */

	tree_lock_use_dec(tree_lock);

	if (tree_lock_refcnt(tree_lock) == 0) {
		release_tree_lock(tree_lock, locktoken);
		destroy_bitlock(&tree_lock->l_un.l_all);
		kmem_zone_free(anon_lock_zone, (void *) tree_lock);

	} else
		release_tree_lock(tree_lock, locktoken);

	return;
}

/* 
 * Called when the page gets an ecc error.
 * We have to hold the anon so that it does not
 * go away when the pfdat lock is unlocked.
 */
void
anon_pagebad(pfd_t *pfd)
{
	int s = pfdat_lock(pfd);
	anon_t 	*ap;

	if (pfd->pf_flags & P_ANON) {
		ap = (anon_t *)pfd->pf_tag;
		anon_hold(ap);
		pfdat_unlock(pfd, s);
		anon_uncache(pfd);
		anon_release(ap);
		pfd_setflags(pfd, P_BAD);
	} else
		pfdat_unlock(pfd, s);
}

/*
 * The given page is no longer going to be associated with anonymous
 * memory.  If it's P_SQUEUE, take it off the sanon free list and free it.
 * Otherwise, the page will typically be on the free list (or in the
 * process of being recycled) although there are few rare cases where
 * the page will still be in use.  This can happen if a pattach has
 * been done to the page or if sched is tossing this region to break
 * a memory deadlock while the process is swapping the page back in,
 * for example.  In these cases, there will still references to the
 * page, but this is OK since they will eventually call pagefree to
 * free up the page.
 */

/*ARGSUSED*/
void
psanonfree(void *unused_arg, pfd_t *pfd)
{

	nested_pfdat_lock(pfd);

	/*
	 * Check if there's a swap handle in the pfd.  If there is, toss
	 * it since the logical anonymous page is being discarded.
	 */

	if ((pfd->pf_flags & (P_SWAP|P_QUEUE|P_SQUEUE)) == P_SWAP &&
	     pfd->pf_swphdl) {
		sm_free(&pfd->pf_swphdl, 1);
		pfd->pf_swphdl = NULL;
	}

	/*
	 * Disassociate page from anonymous memory object.
	 */

	pfd->pf_flags &= ~(P_HASH|P_ANON|P_SWAP);
	pfd->pf_tag    = NULL;

	/*
	 * Check the P_SQUEUE state first without getting any locks.
	 * Since the given page isn't in use anymore, we know the state
	 * can't switch back and forth between P_SQUEUE and other states.
	 * Also, the sanon_lock must be held by a previous routine in our call
	 * stack if we may be dealing with sanon pages.  So vhand can't
	 * get in and start swapping these pages either.  So we can check
	 * without holding a lock.
	 */

	if (pfd->pf_flags & P_SQUEUE) {

		/*
		 * Now take it off the sanon free list.  It should still be
		 * there because of the sanon_lock.  Put the page on the
		 * free list.
		 */

		psremove(pfd);
		pfdat_inc_usecnt(pfd);
		nested_pfdat_unlock(pfd);
		pagefree(pfd);

	} else {
		nested_pfdat_unlock(pfd);
	}
}


/*
 * pagefreeanon
 *
 * Free a page because (all or part) of a region is being destroyed.
 * The page passed in may or may not be an anonymous page.  This routine
 * gets called to free pages belonging to any region that has anonymous
 * backings, and in the case of MAP_PRIVATE regions, some pages will be
 * vnode pages and some will be anonymous pages.  For vnode pages, we
 * call through to pagefree since vnode pages hang around in the cache
 * when they're free (at least until the file is removed).  However, 
 * anonymous pages don't persist after the region is released, therefore
 * we don't want them to go on the free list when the last reference is
 * gone.  Instead we nuke them since no one will ever reference them
 * again.
 * If npgs > 0, it means pfd is the first page of a large page npgs
 * indicates the size of the large page in clicks.
 */


void
pagefreeanon_size(register pfd_t *pfd, pgno_t npgs)
{
	anon_t		*ap;
	anon_lock_t	*tree_lock;
	int		 locktoken, tree_locktoken;
	pfd_t		*next_pfd, *lpfd_list, *lpfd;
	int		anon_locked = 0;
	int		num_pages_on_list = 0, i;
	sm_swaphandle_t	swphdl;


	/*
	 * Fast path. If the pfdat has more than 1 use just
	 * decrement the use count and return.
	 */

	lpfd_list = NULL;
	lpfd = pfd;
	for (i = 0; i < npgs; i++, pfd++) {
		locktoken = pfdat_lock(pfd);

		ASSERT(!(pfd->pf_flags & (P_QUEUE|P_SQUEUE|P_RECYCLE)));
		ASSERT(pfd->pf_use >= 1);
		if (pfd->pf_use > 1) {
			pfd->pf_use--;
			pfdat_unlock(pfd, locktoken);
			continue;
		}


		/*
		 * If the page is not anon, then just do the pagefree.
		 */

		if ((pfd->pf_flags & P_ANON) == 0) {
			pfdat_unlock(pfd, locktoken);
			pagefree(pfd);
			continue;
		}

		/*
		 * Hold the anon node to make sure it doesn't go away while
		 * we're waiting for the tree lock.  Normally this won't 
		 * happen since most callers won't release the anonymous node
		 * until they're done freeing all all the pages.  But this can
		 * also be called after a pagewait was done on the pfdat and
		 * another sproc thread could be tearing down or collapsing the
		 * anon tree right now.
		 */

		if (!anon_locked) {
			ap = (anon_t *)(pfd->pf_tag);
			anon_hold(ap);
			pfdat_unlock(pfd, locktoken);

			tree_lock = ap->a_lock;
			tree_locktoken = acquire_tree_update_lock(tree_lock);

		/*
		 * We hold an extra reference on the tree lock while in this
		 * routine.  That way the lock won't disappear if we end up
		 * freeing the last node in the tree when we call anon_release.
		 * This makes it simpler to handle the unlocking at the end of
		 * this function.
		 */

			tree_lock_use_inc(tree_lock);
			anon_locked = 1;
			nested_pfdat_lock(pfd);
		}

		ANON_TRACE_ENTER(ap, ANON_PAGEFREEANON, pfd, pfd->pf_flags);


	/*
	 * If the page is still anon then the state of the page should not have 
	 * changed while we waited for the locks since we haven't given up our 
	 * reference to the page yet. It is possible that the page has been
	 * removed from the page cache due to a COW of a P_CW page. In that
	 * case the P_ANON flag will be clear.
	 */


		if ((pfd->pf_flags & P_ANON) && (pfd->pf_use == 1)) {

			ASSERT((pfd->pf_flags &
				(P_HASH|P_ANON|P_QUEUE|P_SQUEUE|P_RECYCLE)) ==
				(P_HASH|P_ANON));
			ASSERT(pfd->pf_tag == ap);
			ASSERT(pfd->pf_use >= 1);

			/*
			 * There may be a swap handle saved in the pfd.  If
			 * one's there, then there shouldn't be one in the
			 * scache yet.
			 */

			swphdl = pfd->pf_swphdl;

			if (swphdl) {
				ASSERT(pfd->pf_flags & P_SWAP);
				ASSERT(scache_lookup(&ap->a_scache,
						     pfd->pf_pageno) == NULL);
			}

			pfd->pf_flags &= ~(P_ANON|P_HASH|P_DONE|P_SWAP|P_CW);
			pfd->pf_tag    = NULL;
			nested_pfdat_unlock(pfd);
			pcache_remove_pfdat(&ap->a_pcache, pfd);

			/*
			 * Since the last reference to the page is going
			 * away, save any swap handle from the pfd into
			 * the scache for permanent storage.
			 */

			if(swphdl) {
				scache_add(&ap->a_scache, pfd->pf_pageno, 
					   swphdl);
				pfd->pf_swphdl = NULL;
			}

			/*
			 * If we need pages in the scache pool, recycle
			 * this page right now.
			 */

			if (scache_pool_pages_needed()) {
				pfd->pf_flags  = P_CACHESTALE;
				pfd->pf_pageno = PGNULL;
				scache_pool_add(pfd);

			} else if (npgs > 1) {
				pfd->pf_next = lpfd_list;
				lpfd_list = pfd;
				num_pages_on_list++;

			} else pagefree(pfd);

		} else {
			nested_pfdat_unlock(pfd);

			pagefree(pfd);
		}

	}

	if (anon_locked) {
		/*
		 * Drop the extra reference we acquired on the anon node. 
		 * This may cause the node ap points at to be freed, so we can't
		 * reference ap anymore after this call.
		 */

		anon_release(ap);

	/*
	 * Give up our extra reference on the tree lock.  If we were the
	 * last reference, then anon_release freed the last node in the tree,
	 * so free the lock now.  If there are still references, then there
	 * are some nodes left somewhere, so just release the lock.
	 */

		tree_lock_use_dec(tree_lock);

		if (tree_lock_refcnt(tree_lock) == 0) {
			release_tree_lock(tree_lock, tree_locktoken);
			destroy_bitlock(&tree_lock->l_un.l_all);
			kmem_zone_free(anon_lock_zone, (void *) tree_lock);

		} else
			release_tree_lock(tree_lock, tree_locktoken);
	}

	/*
	 * Try to free the large page. If all pages have been removed from
	 * the page cache then lpfd points to the beginning of the 
	 * large page.
	 */ 
	if (lpfd_list) {
		if (num_pages_on_list == npgs) {
			pagefree_size(lpfd, ctob(npgs));
		} else {
			for (pfd = lpfd_list; pfd; pfd = next_pfd) {
				next_pfd = pfd->pf_next;
				pagefree(pfd);
			}
		}
	}
		
	return;
}


/*
 * Shared Anonymous Pages - Swap List Handling Routines
 *
 * Pages that belong to shared anonymous regions that can exist
 * after any process is referencing them (such as shd mem) must be
 * remembered somewhere (not on the free list) so that vhand can page them
 * and so that they don't get recycled before being written to swap
 * This list is headed by a per-node list using the pf_prev/next pointers.
 * Pages on this list are marked SQUEUE
 */


/*
 * Give a list of P_SQUEUE pages to vhand for cleaning.  We return a singly 
 * linked list of up to npgs pages.  These pages are removed
 * from the psanon free list and are logically allocated to vhand by incrementing
 * the use count on the pages.  This way vhand can use all the usual
 * algorithms and routines for swapping out pages without having to have
 * special case code to handle pages whose use count is 0.  It also eliminates
 * another special case that would result if someone else tried to get a
 * reference to the page while vhand was swapping it.
 *
 * XXX when you have an anon tree where all the leaves are attached to
 * regions marked RG_HASSANON, then we could get in the position where
 * the code will try to swap pages associated with non-leaf nodes more
 * than once.  These situations come up when debugging processes that
 * fork.  To avoid this problem, we don't swap pages from non-leaf nodes
 * that are on the sanon free list at all.  Yes, this is a kludge.  Maybe we'll 
 * figure out a better way to handle sanon pages some day.
 */

pfd_t *
pgetsanon(register int npgs)
{
	register int 	locktoken;
	register pfd_t 	*pfd, *last, *first;
	sanon_list_head_t *salist;
	static cnodeid_t cnode = 0;
	cnodeid_t	cnode_start;

	first = NULL;
	cnode_start = cnode;

	/*
	 * Pull the first npgs pages off the front of sanon free lists.  These are
	 * the oldest ones.
	 */

again:
	salist = &SANON_LIST_FOR_CNODE(cnode);
	while (npgs > 0 && salist->sal_listhead.pf_next != (pfd_t *)(&salist->sal_listhead)) {

		locktoken = mp_mutex_spinlock(&salist->sal_lock);
		pfd = salist->sal_listhead.pf_next;

		/*
		 * Look for a suitable page.
		 */

		while (pfd != (pfd_t *)(&salist->sal_listhead) && 
		      (!anon_isleaf(pfd->pf_tag)
#ifdef NUMA_BASE
		       || pfdat_ispoison(pfd)
#endif
		       )) {

			pfd = pfd->pf_next;
		}

		/*
		 * Quit if end of list.
		 */

		if (pfd == (pfd_t *)(&salist->sal_listhead)) {
			mp_mutex_spinunlock(&salist->sal_lock, locktoken);
			break;
		}

		ASSERT((pfd->pf_flags & (P_QUEUE|P_SQUEUE|P_HASH|P_SWAP|P_ANON)) == \
			(P_SQUEUE|P_HASH|P_ANON));
		ASSERT(pfd->pf_use == 0);

		ANON_TRACE_ENTER(pfd->pf_tag, ANON_PGETSANON, pfd, pfd->pf_flags);

		nested_pfdat_lock(pfd);

		pfd->pf_flags &= ~P_DONE;

		_psremove(pfd);

		pfdat_inc_usecnt(pfd);
		nested_pfdat_unlock(pfd);
		mp_mutex_spinunlock(&salist->sal_lock, locktoken);

		if (first == NULL)
			first = pfd;
		else
			last->pf_next = pfd;

		last = pfd;
		npgs--;

		/*
		 * We need a brief delay here to give other processors
		 * a chance to get the sanon_list_lock.  The problem is
		 * that spin locks are not fair in that there's no
		 * guarantee that a processor spinning for a contended
		 * lock will ever get it.  Since the lock is already in
		 * our cache, we can reacquire it when we loop back to
		 * the top here before another processor can get the
		 * cache line.  This has been observed on both Challenge
		 * and Origin.  Since the sanon_list_lock is used in
		 * a nested fashion in some places, we want to make sure
		 * that other processors don't get starved too long,
		 * which would increase their interrupt latencies too
		 * much.  Adding a 1 us delay here solves the problem
		 * without adding code complexity.  This isn't a
		 * performance critical path, so an extra microsecond
		 * per iteration won't hurt anything.
		 */

		us_delay(1);
	}

	if (first != NULL)
		last->pf_next = NULL;

	/*
	 * If we didnt get the number of pages we asked for & we havent searched
	 * all the lists yet, search the next one.
	 */
	if (npgs) {
		cnode = (cnode+1) % numnodes;
		if (cnode != cnode_start)
			goto again;
	}
		
	/*
	 * Next time we are called, start at a different node.
	 */
	cnode = (cnode_start+1) % numnodes;
	return first;
}




/*
 * Remove from shared anon list
 */

void
_psremove(register pfd_t *pfd)
{
	register pfd_t	*next, *prev;

	ASSERT((pfd->pf_flags & (P_QUEUE|P_SQUEUE|P_RECYCLE|P_CW)) == P_SQUEUE);

	ANON_TRACE_ENTER(pfd->pf_tag, ANON_PSREMOVE, pfd, pfd->pf_flags);
#ifdef DEBUG
	ASSERT(psanonmem > 0 && pfd->pf_use == 0);
	psanonmem--;
#endif

	next = pfd->pf_next;
	prev = pfd->pf_prev;

	next->pf_prev = prev;
	prev->pf_next = next;

	pfd->pf_next = NULL;
	pfd->pf_prev = NULL;
	pfd->pf_flags &= ~P_SQUEUE;
}


/*
 * pagefreesanon - free a page a shared anon page, making arrangements
 * to swap it if necessary.  This ensures that the pages of a shared
 * anon region will exist until the region is freed, even if no one
 * is currently referencing them.
 */

void
pagefreesanon(register pfd_t *pfd, int dirty)
{
	register int	locktoken;
	anon_t		*ap;

	ASSERT(!(pfd->pf_flags & (P_QUEUE|P_SQUEUE|P_RECYCLE)));
	ASSERT(pfd->pf_use >= 1);

	/*
	 * We can get vnode pages here in the case of a MAP_PRIVATE file
	 * mapping where the region went sanon at some point but this
	 * particular page hasn't gone anonymous yet.  There's nothing
	 * special to do for vnode pages, so just free them and return.
	 */

	if ((pfd->pf_flags & P_ANON) == 0) {
		pagefree(pfd);
		return;
	}

	/*
	 * We cheat on the locking here a bit.  We know the caller still
	 * has a reference to this anon node, so we can skip the the 
	 * anon_holding protocol.
	 */

	ap = (anon_t *)(pfd->pf_tag);
	locktoken = acquire_tree_access_lock(ap->a_lock);


	nested_pfdat_lock(pfd);

	/*
	 * If the page is dirty and is in a leaf, then any swap copies are
	 * stale.  Shut off P_SWAP so we will know later (when the last use
	 * goes away) whether the page needs sanon free list swapping or not.  
	 * We don't do this for non-leaf pages since they're read-only and
	 * hence don't need to be swapped more than once (and in fact can't
	 * be swapped more than once for things to work correctly).
	 */
	ANON_TRACE_ENTER(ap, ANON_PAGEFREESANON, pfd, pfd->pf_flags);

        if (dirty && ap->a_left == NULL && ap->a_right == NULL &&
	   (pfd->pf_flags & P_SWAP)) {
		scache_clear(&ap->a_scache, pfd->pf_pageno);

		if (pfd->pf_swphdl) {
			sm_free(&pfd->pf_swphdl, 1);
			pfd->pf_swphdl = NULL;
		}

                pfd->pf_flags &= ~P_SWAP;
	}

	if ((pfd->pf_flags & P_SWAP) == 0)
		pfd->pf_flags |= P_SANON;

	ANON_TRACE_ENTER(ap, ANON_PAGEFREESANON1, pfd->pf_flags, pfd->pf_use);

	/*
	 * When the last use of the page is going away, it goes on the
	 * sanon free list if it hasn't already been swapped.  This makes sure
	 * that a copy will be around if someone references the page later
	 * on after the page is re-cycled (since the anon region still
	 * exists).
	 */

	if (pfd->pf_use == 1 && (pfd->pf_flags & P_SWAP) == 0) {
		register pfd_t		*prev;
		sanon_list_head_t	*salist;

		pfd->pf_use = 0;
		pfd->pf_flags |= P_SQUEUE;
		pfd->pf_flags &= ~P_SANON;

		salist = &SANON_LIST_FOR_PFD(pfd);
		nested_spinlock(&salist->sal_lock);
#ifdef DEBUG
		psanonmem++;
#endif
		pfd->pf_next = (pfd_t *)&salist->sal_listhead;
		prev = pfd->pf_prev = salist->sal_listhead.pf_prev;
		salist->sal_listhead.pf_prev = prev->pf_next = pfd;
		nested_spinunlock(&salist->sal_lock);
		nested_pfdat_unlock(pfd);
		release_tree_lock(ap->a_lock, locktoken);
	} else if (pfd->pf_use > 1) { 
		/*
		 * Don't unlock the pfdat lock before
		 * decrementing reference count, to
		 * prevent race with migr_page_epilogue()
		 * where it may be giving up its pfdat
		 * reference at the same time and trying
		 * to check P_SANON while we're setting it.
		 */
		pfd->pf_use--;
		nested_pfdat_unlock(pfd);
		release_tree_lock(ap->a_lock, locktoken);
	} else {
		nested_pfdat_unlock(pfd);
		_anon_pagefree_and_cache(pfd);
		release_tree_lock(ap->a_lock, locktoken);
	}
}


/*
 * anon_isdegenerate - return true if passed anon handle is the leaf
 * of a degenerate tree.  This is a tree where there is only one leaf
 * which means there are no other processes sharing any of the pages.
 */

int
anon_isdegenerate(anon_t *ap)
{
	/* REFERENCED */
	anon_t	*start_ap;
	int	 locktoken;

	ASSERT(ap->a_left == NULL && ap->a_right == NULL);

	start_ap = ap;
	locktoken = acquire_tree_access_lock(ap->a_lock);

	while (ap = ap->a_parent) {
		if (ap->a_left && ap->a_right) {
			release_tree_lock(ap->a_lock, locktoken);
			return 0;
		}
	}

	release_tree_lock(start_ap->a_lock, locktoken);
	return 1;
}


/*
 * anon_isleaf - return true if the passed anon handle is the leaf
 * of a tree.
 */

static int
anon_isleaf(anon_t *ap)
{
	return (ap->a_left == NULL && ap->a_right == NULL);
}


/*
 * Momentarily release the tree lock to allow any pending interrupts to occur.
 */

static void
anon_preempt(void *cookie)
{
	preempt_cookie_t	*preempt_cookie;

	preempt_cookie = (preempt_cookie_t *)cookie;

	release_tree_lock(preempt_cookie->pc_lock, preempt_cookie->pc_locktoken);

	delay_for_intr();
	scache_pool_reserve();

	preempt_cookie->pc_locktoken =
		acquire_tree_update_lock(preempt_cookie->pc_lock);
}

	
/*
 *	Given a pointer to a leaf anon node, garbage collect all the
 *	unnecessary swap space we can find in the whole tree.
 */

void
anon_shake_tree(anon_t *ap)
{
	anon_t *root;
	int	locktoken;

	if (ap->a_parent == NULL)
		return;

	locktoken = acquire_tree_update_lock(ap->a_lock);

	/*
	 * Shakes start at the root since that's the easiest way
	 * to walk the tree.  So walk to the root.
	 */

	while (ap->a_parent != NULL)
		ap = ap->a_parent;

	root = ap;

	/*
	 * Visit each non-leaf node in the tree and check every allocated
	 * swap page for coverage.  Trees are always walked along the leftmost
	 * branch first, then we handle the right branches as we head back
	 * towards the root.  This way we don't have to keep any state
	 * information about which nodes we've already done.  When we
	 * eventually unwind back to the root after having walked its
	 * right branch, then we're done.
	 */

	for (;;) {

		/*
		 * Shake this node if it is a non-leaf node.  We can never
		 * remove swap space from a leaf node since it's in use
		 * by definition.
		 */

		
		if (ap->a_left || ap->a_right)
			scache_scan(&ap->a_scache, SCAN, 0, MAXPAGES,
				(void (*)(void *, pgno_t, sm_swaphandle_t))
				chk_coverage, ap);

		/*
		 * Descend another level down the tree following the left
		 * branch first if there is one.
		 */

		if (ap->a_left)
			ap = ap->a_left;

		else if (ap->a_right)
			ap = ap->a_right;

		else {

			/*
			 * We've hit a leaf.
			 *
			 * If we got here by tranversing a right branch,
			 * then we've already done the left branch (cause
			 * we always go left first), so we keep popping
			 * back to the parent node until we find a right
			 * branch we haven't taken yet (or run out of tree).
			 */

backup:
			while (ap == ap->a_parent->a_right) {
				ap = ap->a_parent;

				if (ap == root) {
					release_tree_lock(ap->a_lock, locktoken);
					return;	   /* tree walk complete */
				}
			}

			/*
			 * At this point, the walk of the subtree pointed
			 * to by ap has completed.  Pop back to its parent
			 * and see if it has a right branch to follow.  If
			 * it has no right branch, then keep popping back
			 * til we find one or run out of tree.
			 */

			ap = ap->a_parent;

			if (ap->a_right == NULL) {
				if (ap == root) {
					release_tree_lock(ap->a_lock, locktoken);
					return;     /* tree walk complete */
				}

				goto backup;
			}

			/*
			 * The left hand branch of ap's subtree has been
			 * walked.  Go to the right now.
			 */

			ap = ap->a_right;
		}
	}

	/*NOTREACHED*/
	return;
}


/*
 *	Check the subtree starting at root to see if lpn is completely
 *	covered by nodes above it.  If so, free any swap space associated
 *	with it for all covered instances.
 */

/*ARGSUSED*/
static void
chk_coverage(anon_t *root, pgno_t lpn, sm_swaphandle_t arg_ignored)
{
	anon_t	*ap;

	/*
	 * Walk the tree starting at the root.  We always take the left
	 * branch first (if there is one).
	 */

	if (root->a_left)
		ap = root->a_left;

	else 
		ap = root->a_right;

	for (;;) {

		/*
		 * If lpn is present in this node or if the lpn is beyond the
		 * nodes limit, then it covers all nodes below us.  Therefore
		 * there's no need to walk this subtree any further.
		 */

		if (lpn >= ap->a_limit ||
		    pcache_find(&ap->a_pcache, lpn) ||
		    scache_lookup(&ap->a_scache, lpn)) {

			/*
			 * Since we aren't going to search this sub-tree
			 * further, backtrace and try the other branches.
			 *
			 * If we got here via a right branch, then we know
			 * we've done the left branch already.  So keep
			 * popping back til we find a right branch we haven't
			 * taken yet or we run out of tree.
			 */

backup:
			while (ap == ap->a_parent->a_right) {
				ap = ap->a_parent;

				/*
				 * We know the page is covered by the sub-tree
				 * above us, so if there's swap space in this
				 * node, free it.
				 */

				scache_clear(&ap->a_scache, lpn);
				pcache_remove(&ap->a_pcache, lpn, 1,
					(void (*)(void *, struct pfdat *))
						free_covered_page,
					ap, NULL, NULL);

				if (ap == root)
					return;		/* lpn is covered */
			}

			/*
			 * At this point, the walk of the subtree pointed
			 * to by ap has completed.  Pop back to its parent
			 * and see if it has a right branch to follow.  If
			 * it has no right branch, then keep popping back
			 * til we find one or run out of tree.
			 */

			ap = ap->a_parent;

			if (ap->a_right == NULL) {
				/*
				 * We know the page is covered by the sub-tree
				 * above us, so if there's swap space in this
				 * node, free it.
				 */

				scache_clear(&ap->a_scache, lpn);
				pcache_remove(&ap->a_pcache, lpn, 1,
					(void (*)(void *, struct pfdat *))
						free_covered_page,
					ap, NULL, NULL);

				if (ap == root)
					return;		/* lpn is covered */

				goto backup;
			}

			/*
			 * We've done the left branch, now do the right.
			 */

			ap = ap->a_right;
			continue;
		}

		/*
		 * Not covered by this node, try the next one by taking the
		 * left branch first.  If we're at a leaf, then it doesn't
		 * contain lpn and therefore lpn is not covered.  This means
		 * we can't free the associated space in root yet.  If we're
		 * at a leaf, then it doesn't contain lpn and therefore lpn
		 * is not covered.  This means we can't free the associated
		 * space in root yet.
		 */

		if (ap->a_left)
			ap = ap->a_left;

		else if (ap->a_right)
			ap = ap->a_right;

		else
			return;		/* lpn not covered */
	}

	/*NOTREACHED*/
	return;
}

#if TIME_LOCKS

/*
 * Instrumented versions of lock acquire and release functions that
 * time how long the lock was held.  The debugger is entered if the
 * the elapsed time exceeds the specified threshold.
 */


#include "sys/cmn_err.h"
#include "sys/timers.h"

int anon_lock_threshold = 1000;
int anon_maxtime;

int
acquire_tree_update_lock(anon_lock_t *lock)
{
	int locktoken;

	locktoken = mp_mutex_bitlock(&(lock)->l_un.l_all, ANON_LOCK_BIT);
	lock->lock_time = get_timestamp();
	return locktoken;
}

void
release_tree_lock(anon_lock_t *lock, int locktoken)
{
	unsigned rel_time, lock_time;
	int usecs, elapsed;

	rel_time = get_timestamp();
	lock_time = lock->lock_time;
	mp_mutex_bitunlock(&(lock)->l_un.l_all, ANON_LOCK_BIT, locktoken);

	elapsed = rel_time - lock_time;

	if (elapsed < 0)
		elapsed += TIMER_MAX;

	usecs = (int)(((long long)elapsed * timer_unit) / 1000);

	if (usecs > anon_lock_threshold) {
		cmn_err(CE_CONT, "\nrelease_tree_lock: lock held %d usecs (lock time %x, rel time %x)\n",
			usecs, lock_time, rel_time);
		debug("anon");
	}

	if (usecs > anon_maxtime)
		anon_maxtime = usecs;
}


int
get_lock_time(anon_lock_t *lock)
{
	unsigned cur_time, lock_time;
	int usecs, elapsed;

	cur_time = get_timestamp();
	lock_time = lock->lock_time;

	elapsed = cur_time - lock_time;

	if (elapsed < 0)
		elapsed += TIMER_MAX;

	usecs = (int)(((long long)elapsed * timer_unit) / 1000);

	return usecs;
}


int
get_elapse_time(unsigned lock_time)
{
	unsigned cur_time;
	int usecs, elapsed;

	cur_time = get_timestamp();

	elapsed = cur_time - lock_time;

	if (elapsed < 0)
		elapsed += TIMER_MAX;

	usecs = (int)(((long long)elapsed * timer_unit) / 1000);

	return usecs;
}

void
release_tree_lock_nocheck(anon_lock_t *lock, int locktoken)
{
	mp_mutex_bitunlock(&(lock)->l_un.l_all, ANON_LOCK_BIT, locktoken);
}


#endif

#ifdef  ANON_TRACE

void
idbg_anon_trace(__psunsigned_t val)
{
        struct anon *ap = (struct anon *)val;
        ktrace_entry_t  *kt;
        ktrace_snap_t   ktsnap;

        if (anon_ktrace == NULL)
                return;

        qprintf("Anon pagecache operations for anon 0x%x\n", ap);
        kt = ktrace_first(anon_ktrace, &ktsnap);
        while (kt != NULL) {
                if ((val == -1) || ((anon_t *)(kt->val[0]) == ap) ||
		    ((__psunsigned_t)kt->val[1] == ANON_DUP &&
			(kt->val[2] == ap || kt->val[3] == ap))) {
                        qprintf("ap: 0x%x", kt->val[0]);
                        qprintf("  %s @%d pid %d RA:0x%x 0x%x 0x%x \n",
                                        anon_ops[(long)kt->val[1]],
                                        kt->val[4],
					kt->val[6],
                                        kt->val[5],
                                        kt->val[2], kt->val[3]);
                }
                kt = ktrace_next(anon_ktrace, &ktsnap);
        }
}


void
idbg_anon_trace_time(__psunsigned_t val)
{
        ktrace_entry_t  *kt;
        ktrace_snap_t   ktsnap;

        if (anon_ktrace == NULL)
                return;

	if (val == -1) {
		qprintf("usage: traceanontime <start time>\n");
		return;
	}

        qprintf("Anon pagecache operations from time %d\n", val);
        kt = ktrace_first(anon_ktrace, &ktsnap);
        while (kt != NULL) {
                if ((__psunsigned_t)kt->val[4] >= val) {
                        qprintf("ap: 0x%x", kt->val[0]);
                        qprintf("  %s @%d pid %d RA:0x%x 0x%x 0x%x \n",
                                        anon_ops[(long)kt->val[1]],
                                        kt->val[4],
					kt->val[6],
                                        kt->val[5],
                                        kt->val[2], kt->val[3]);
                }
                kt = ktrace_next(anon_ktrace, &ktsnap);
        }
}


void
idbg_anon_trace_pid(__psunsigned_t val)
{
        ktrace_entry_t  *kt;
        ktrace_snap_t   ktsnap;

        if (anon_ktrace == NULL)
                return;

	if (val == -1) {
		qprintf("usage: traceanonpid <pid>\n");
		return;
	}

        qprintf("Anon pagecache operations for pid %d\n", val);
        kt = ktrace_first(anon_ktrace, &ktsnap);
        while (kt != NULL) {
                if ((__psunsigned_t)kt->val[6] == val) {
                        qprintf("ap: 0x%x", kt->val[0]);
                        qprintf("  %s @%d pid %d RA:0x%x 0x%x 0x%x \n",
                                        anon_ops[(long)kt->val[1]],
                                        kt->val[4],
					kt->val[6],
                                        kt->val[5],
                                        kt->val[2], kt->val[3]);
                }
                kt = ktrace_next(anon_ktrace, &ktsnap);
        }
}
#endif

