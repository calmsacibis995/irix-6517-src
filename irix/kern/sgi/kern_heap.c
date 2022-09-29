/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 4.166 $"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/signal.h>
#include <sys/strsubr.h>	/* for setqsched() */
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/var.h>
#include <sys/atomic_ops.h>
#include <sys/nodepda.h>
#include <sys/idbgentry.h>
#include <ksys/kern_heap.h>
#include <sys/rtmon.h>

#ifdef NUMA_BASE
#include <sys/numa.h>		/* physmem_select_neighbor_node() */
#endif	/* NUMA_BASE */
#include <ksys/vm_pool.h>

/*
 * make sure constants are the same - this permits
 * us to not have to include immu.h in kmem.h
 */
#if (KM_NOSLEEP != VM_NOSLEEP) || \
    (KM_PHYSCONTIG != VM_PHYSCONTIG) || \
    (KM_CACHEALIGN != VM_CACHEALIGN) || \
    (KM_NODESPECIFIC != VM_NODESPECIFIC)
#error "Constants wrong"
>>>>>>ERROR <<<<<<<<
#endif

#define HLOG(name, addr, len) \
    LOG_TSTAMP_EVENT(RTMON_ALLOC, TSTAMP_EV_ALLOC, \
		     (name), (uint64_t)(addr), (uint64_t)(len), \
		     (uint64_t)__return_address)

#define MINHEAD		((int)(sizeof(struct blk)))
#define FULLPG		(NBPP - MINHEAD)
#define ANCHOR		0x3f	
#define MINFREEBLKSZ	(sizeof(struct header))
#define LOG2BUCKETSZ	4
#define LOG2LBGRAIN	5
#define LBGRAIN		(1 << LOG2LBGRAIN)
#define NEXT(BLK)	(struct header *)((char *)(BLK)+(BLK)->blk.thisblksz)
#define PREV(BLK)	(struct header *)((char *)(BLK)-(BLK)->blk.prevblksz)

#define ROUNDUP(nb) \
	((((nb) + GRAIN - 1) >> LOG2GRAIN) << LOG2GRAIN)
#define ROUNDUP_SCLINE(nb) \
	(((nb) + scache_linemask) & ~scache_linemask)

#ifndef SCACHE_LINESIZE
#define SCACHE_LINESIZE (scache_linemask + 1)
#endif

static void addfreeq(arena_t *, struct header *);
static int carve(arena_t *, struct header *, int);
static int bytetobucket(arena_t *, int);

/*
 *	The following manipulate the free queue
 *
 *		DELFREEQ will remove x from the free queue
 */
#define DELFREEQ(x)	(x)->hu.fl.prevfree->hu.fl.nextfree = \
				(x)->hu.fl.nextfree;\
			(x)->hu.fl.nextfree->hu.fl.prevfree = \
				(x)->hu.fl.prevfree;

/*
 *	The following manipulate the busy flag
 */
#define BUSY	1
#define SETBUSY(x)	((__psint_t)(x) | BUSY)
#define CLRBUSY(x)	((__psint_t)(x) & ~BUSY)
#define TESTBUSY(x)	((__psint_t)(x) & BUSY)

/*
 * Comments specific to NUMA_BASE systems, and code under #ifdef NUMA_BASE
 *
 * Providing node affinity as part of kernel heap allocation.
 * On NUMA_BASE systems, it's necessary to provide affinity
 * to kernel heap allocations. This is done in two ways.
 * 	Default kmem_alloc() tries to allocate memory on the node where 
 * 	thread is running. If there is no memory available (neither
 *	in heap nor a new page can be allocated), it will decide to return
 *	memory from the nearest node. (Conecpt of nearest node is still TBD)
 *
 *	In addition there are special routines kmem_alloc_node()
 *	which take a node id, and try to allocate memory on the specific
 *	node. These special allocators will allocate memory only on the
 *	specific node. These routines MUST be called spefifying VM_NOSLEEP.
 *
 *	There are also kmem_alloc_node_hint routines that allow KM_SLEEP. These
 *	routines attempt to allocate on the designated node but will try
 *	alternate nodes if allocation on the requested node fails.
 *
 *
 * Implementation notes:
 *
 *	On NUMA_BASE systems each node manages its own heap, and the heap
 *	data structure is situated in Node PDA. CPU Pda has a pointer 
 *	to Node PDA. (Macro nodepda does the proper expansion to do this)
 *	So, when a thread tries to allocate heap memory on a specific 
 *	node, allocator starts looking in the heap attached to the 
 *	nodepda of the cpu where thread is running.
 *	"gheap" gets expanded to the appropriate node pda on NUMA_BASE systems.
 *
 * 	Non-NUMA_BASE systems are considered to be systems with just one Node.
 *	So, there is one Node PDA for the entire system, and the heap
 *	pointer is attached there. On these systems, 'nodepda' macro
 *	points to this global nodepda.
 *	Refer to sys/nodepda.h for details.
 *	
 *	Once you get the heap pointer, rest of the searches and 
 *	management is done in the way it used to be. 
 */


int	kheap_initialized;
#define	HEAPLOCK()	mutex_spinlock(&ap->heaplock)
#define	HEAPUNLOCK(T)	mutex_spinunlock(&ap->heaplock, T)

int kheapmapped;	/* # of multi-page k2seg mappings */

/* Declarations for internal static routines */
/*
 * Following set of declaratons define the two methods of allocating
 * heap memory. First one (arena_*) stuff uses a link list threading
 * through the free blocks to keep track of the freelist. So,
 * a request for a block, will require a search of the list of 
 * free blocks, and finding the one that satisfies the sizes.

 * Second set of interfaces (zone_ *) uses zone allocators as the
 * underlying mechanism to allocate/free the memory. In this,
 * based on the size of request, a zone is identified, and we
 * pull a block from that zone to be returned to the caller. 
 * On freeing the block is enqueued back in the right zone.
 *
 * Second method has the advantage of quick allocation/freeing.
 * It has a bit more overhead than the first one in terms of
 * allocation. Assumption is, since there are zones already, 
 * and pages allocated to them, we would be using just one pool
 * of memory instead of multiple pools, and that should reduce
 * the fragmentation.
 *
 * First scheme is primiarily in use by private arena creators
 * (VM and and scheduler subsystems seems to use this)
 */
static  void    *arena_malloc(arena_t *, size_t, int);
static  void    *arena_zalloc(arena_t *, size_t, int);
static  void    *arena_realloc(arena_t *, void *, size_t, int);
static  void    arena_free(arena_t *, void *);

/*
 * heapzone_* routine are the intermediary routines that convert
 * the kernel heap requests to appropriate zone requests so that
 * we use zone allocator as the basic memory management tool.
 */
static void    *heapzone_malloc(size_t, int, cnodeid_t);
static void    *heapzone_zalloc(size_t, int, cnodeid_t);
static void    *heapzone_realloc(void *, size_t, int, cnodeid_t, inst_t *);
static void    heapzone_free(void *);

#ifdef	PRHEAP

static void kern_prheap_nolock(__psint_t, int);
static void kern_prheap(__psint_t);
static void idbg_prheap(__psint_t x, __psint_t a2, int argc, char **argv);

/*
 * kreg_t's used to be k_machreg_t's but that isn't
 * right on 32bit kernels where k_machreg_t's are 64-bit
 * but kernel regs are 32-bit.
 */
#if _MIPS_SIM != _ABI64
typedef unsigned kreg_t;
#else
typedef k_machreg_t kreg_t;
#endif

/*
 * savepcs, and it's subordinate find_runtime_pdr, are apparently
 * COFF-specific and don't work correctly now that kernels are ELF
 * binaries.  We define ELF_KERNEL here to remove the code that
 * walks the stack providing a multi-level backtrace.  Instead we
 * save only the return_address of the direct caller of k*alloc()
 * and friends.
 */
#define ELF_KERNEL

#ifdef ELF_KERNEL

#define MAXPCS		1
#define LINKUP(arg1, arg2)	heap_linkup(arg1, arg2, \
					    (__psunsigned_t)__return_address + 1)
static void heap_linkup(char *, size_t, inst_t *);

#else

#define MAXPCS		16
#define LINKUP(arg1, arg2)	heap_linkup(arg1, arg2)
static void heap_linkup(char *, size_t);

#endif	/* ELF_KERNEL */

struct heaplink {
	struct heaplink	*forw;
	struct heaplink	*back;
	kreg_t	callpc[MAXPCS];
	time_t		time;
	size_t		size;
	char name[8];
};


#define	PRHEAPLOCK()	mutex_spinlock(&prheaplock)
#define	PRHEAPUNLOCK(T)	mutex_spinunlock(&prheaplock, T)
static lock_t prheaplock;

static void heap_unlink(void *);

/******************************************************************/

#define	DBGHEAD		ROUNDUP_SCLINE(sizeof(struct heaplink))

extern	void	*Kmem_alloc(size_t, int);
extern	void	*Kmem_zalloc(size_t, int);
extern	void	*Kmem_realloc(void *, size_t, int);
extern	void	Kmem_free(void *, size_t);

extern  void 	*Kern_malloc(size_t);
extern  void 	*Kern_calloc(size_t, size_t);
extern  void 	*Kern_realloc(void *, size_t);
extern  void  	Kern_free(void *);

extern void	*Kern_calloc_node_hint(size_t, size_t, cnodeid_t);
extern void	*Kern_malloc_node_hint(size_t, cnodeid_t);

extern void	*Kmem_alloc_node(size_t, int, cnodeid_t);
extern void	*Kmem_realloc_node(void *, size_t, int, cnodeid_t);
extern void	*Kmem_zalloc_node(size_t, int, cnodeid_t);

extern void	*Kmem_alloc_node_hint(size_t, int, cnodeid_t);
extern void	*Kmem_realloc_node_hint(void *, size_t, int, cnodeid_t);
extern void	*Kmem_zalloc_node_hint(size_t, int, cnodeid_t);

void *
kmem_alloc(size_t nbytes, int flag)
{
	void	*ptr;

	if ( !(ptr = Kmem_alloc(nbytes + DBGHEAD, flag)) )
		return 0;
	LINKUP(ptr,nbytes);
	return (char *)ptr + DBGHEAD;
}

void *
kmem_zalloc(size_t nbytes, int flag)
{
	register void	*ptr;

	if (!(ptr = Kmem_zalloc(nbytes + DBGHEAD, flag)))
		return 0;
	LINKUP(ptr,nbytes);
	return (char *)ptr + DBGHEAD;
}

void *
kmem_realloc(void *vptr, size_t nbytes, int flag)
{
	if (vptr) {
		vptr = (char *)vptr - DBGHEAD;
		heap_unlink(vptr);
	}
	if (!(vptr = Kmem_realloc(vptr, nbytes + DBGHEAD, flag)))
		return 0;
	LINKUP(vptr,nbytes);
	return (char *)vptr + DBGHEAD;
}

void *
kern_malloc(size_t nbytes)
{
	register void	*ptr;

	ptr = Kern_malloc(nbytes + DBGHEAD);
	LINKUP(ptr,nbytes);
	return (char *)ptr + DBGHEAD;
}

void *
kern_calloc(size_t num,size_t size)
{
	register void	*ptr;

	ptr = Kern_calloc(1, (num * size) + DBGHEAD);
	LINKUP(ptr, num*size);
	return (char *)ptr + DBGHEAD;
}

void *
kern_realloc(void *vptr,size_t	nbytes)
{
	register void	*nptr;

	if (vptr) {
		vptr = (char *)vptr - DBGHEAD;
		heap_unlink(vptr);
	}
	if (!(nptr = Kern_realloc(vptr, nbytes + DBGHEAD)))
		return 0;

	LINKUP(nptr,nbytes);
	return (char *)nptr + DBGHEAD;
}

void
kmem_free(void *ptr, size_t size)
{
	void *rptr;

	rptr = (char *)ptr - DBGHEAD;
	heap_unlink(rptr);
	Kmem_free(rptr, size);
}

void
kern_free(void *vptr)
{
	void *rptr;

	rptr = (char *)vptr - DBGHEAD;
	heap_unlink(rptr);
	Kern_free(rptr);
}

void *
kern_calloc_node(register size_t num, register size_t size, cnodeid_t node)
{
	void	*ptr;
	ptr = Kern_calloc_node(1, (num*size)+DBGHEAD, node);
	if (!ptr)
		return ptr;
	LINKUP(ptr, num*size);
	return ((char *)ptr + DBGHEAD);
}

void *
kern_malloc_node(register size_t nbytes, cnodeid_t node)
{
	void	*ptr;
	ptr = Kern_malloc_node(nbytes+DBGHEAD, node);
	if (!ptr)
		return ptr;
	LINKUP(ptr, nbytes);
	return ((char *)ptr + DBGHEAD);
}

void *
kmem_alloc_node(register size_t size, register int flags, cnodeid_t node)
{
	void *ptr;
	ptr = Kmem_alloc_node(size+DBGHEAD, flags|NODE_SPECIFIC_FLAG, node);
	if (!ptr)
		return ptr;
	LINKUP(ptr, size);
	return ((char *)ptr + DBGHEAD);
}

void *
kmem_realloc_node(register void *ptr, register size_t nbytes, int flags, cnodeid_t node)
{
	void	*vptr;

	if (ptr) {
		ptr = (char *)ptr - DBGHEAD;
		heap_unlink(ptr);
	}

	vptr = Kmem_realloc_node(ptr, nbytes + DBGHEAD, 
				flags|NODE_SPECIFIC_FLAG, node);
	if (!vptr)
		return 0;
	LINKUP(vptr, nbytes);
	return ((char *)vptr + DBGHEAD);

}

void *
kmem_zalloc_node(register size_t size, register int flags, cnodeid_t node)
{
	void	*ptr;
	ptr = Kmem_zalloc_node(size+DBGHEAD, flags|NODE_SPECIFIC_FLAG, node);
	if (!ptr)
		return ptr;
	LINKUP(ptr, size);
	return ((char *)ptr + DBGHEAD);
}


void *
kmem_alloc_node_hint(register size_t size, register int flags, cnodeid_t node)
{
	void *ptr;
	ptr = Kmem_alloc_node_hint(size+DBGHEAD, flags|NODE_SPECIFIC_FLAG, node);
	if (!ptr)
		return ptr;
	LINKUP(ptr, size);
	return ((char *)ptr + DBGHEAD);
}

void *
kmem_zalloc_node_hint(register size_t size, register int flags, cnodeid_t node)
{
	void	*ptr;
	ptr = Kmem_zalloc_node_hint(size+DBGHEAD, flags|NODE_SPECIFIC_FLAG, node);
	if (!ptr)
		return ptr;
	LINKUP(ptr, size);
	return ((char *)ptr + DBGHEAD);
}

#define kmem_alloc	Kmem_alloc
#define kmem_zalloc	Kmem_zalloc
#define kmem_free	Kmem_free
#define kmem_realloc 	Kmem_realloc

#define kern_malloc     Kern_malloc
#define kern_calloc     Kern_calloc
#define kern_realloc    Kern_realloc
#define kern_free       Kern_free

/* NUMA flavors of the above routines */
#define kern_calloc_node 	Kern_calloc_node
#define kern_malloc_node 	Kern_malloc_node

#define	kmem_alloc_node		Kmem_alloc_node
#define	kmem_zalloc_node 	Kmem_zalloc_node
#define	kmem_realloc_node 	Kmem_realloc_node

#define	kmem_alloc_node_hint	Kmem_alloc_node_hint
#define	kmem_zalloc_node_hint 	Kmem_zalloc_node_hint

#endif	/* PRHEAP */


/****************************************************************/
/*								*/
/*	The actual kernel dynamic heap interface routines.	*/
/*								*/
/****************************************************************/

void *
kmem_alloc(register size_t size, register int flags)
{
	/* REFERENCED */
	void	*ptr;

	ptr = heapzone_malloc(size, flags, CNODEID_NONE);
#if defined(DEBUG) || defined(DEBUG_ALLOC)
	if (ptr && (size < (NBPP/2)) ) 
		kmem_poison(ptr, size, __return_address);
#endif
	HLOG(UTN('kmem','Aloc'), ptr, size);
	return ptr;
}

void *
kmem_realloc(register void *ptr, register size_t nbytes, int flags)
{
	void *ret;
	HLOG(UTN('kmem','RAl1'), ptr, nbytes);
	ret = heapzone_realloc(ptr, nbytes, flags, CNODEID_NONE, __return_address);
	HLOG(UTN('kmem','RAl2'), ret, nbytes);
	return ret;
}

void *
kmem_zalloc(register size_t size, register int flags)
{
	void *ptr;
	ptr = heapzone_zalloc(size, flags, CNODEID_NONE);
	HLOG(UTN('kmem','ZAlo'), ptr, size);
	return ptr;
}

/*ARGSUSED1*/
void
kmem_free(register void *cp, register size_t size)
{
#if defined(DEBUG) || defined(DEBUG_ALLOC)
	kmem_poison(cp, size, __return_address);
#endif
	HLOG(UTN('kmem','Free'), cp, size);
	heapzone_free(cp);
}

/*
 * Pre-5.4 kernel heap routines.
 */
void *
kern_malloc(register size_t nbytes)
{
	void	*ptr;
	ptr = heapzone_malloc(nbytes, KM_SLEEP, CNODEID_NONE);
#if defined(DEBUG) || defined(DEBUG_ALLOC)
	if (ptr && (nbytes < (NBPP/2))) 
		kmem_poison(ptr, nbytes, __return_address);
#endif	

	HLOG(UTN('kern','Aloc'), ptr, nbytes);
	return ptr;
}

void *
kern_calloc(register size_t num, register size_t size)
{
	void *ptr;
	ptr = heapzone_zalloc(num*size, KM_SLEEP, CNODEID_NONE);
	HLOG(UTN('kern','CAlo'), ptr, num*size);
	return ptr;
}

void *
kern_realloc(register void *ptr, register size_t nbytes)
{
	void *ret;
	HLOG(UTN('kern','RAl1'), ptr, nbytes);
	ret = heapzone_realloc(ptr, nbytes, 0, CNODEID_NONE, __return_address);
	HLOG(UTN('kern','RAl2'), ret, nbytes);
	return ret;
}

void
kern_free(void *ptr)
{
#if defined(DEBUG) || defined(DEBUG_ALLOC)
	if (kmem_do_poison) {
		int	index;
		zone_t	*zone;
		index = ZONE_INDEX_GET(ptr);
		if (index < ZONE_SIZES_PERNODE){
			zone = node_zone(index);
			kmem_poison(ptr, kmem_zone_unitsize(zone), __return_address);
		}
	}
#endif
	HLOG(UTN('kern','Free'), ptr, 0);
	heapzone_free(ptr);
}

/*
 * The following interfaces take a private arena and try to allocate memory
 * from that arena. These routines pass in a private arena. Each arena
 * has its own page alloc method which is called by *_malloc() when a page
 * needs to be allocated. There is also a page free method which is called by
 * afree if a page needs to be freed (eg. after coalescing).
 */

/*
 * create and initialize an arena.
 */

arena_t *
kmem_arena_create(
		void *         (*arena_mem_alloc)(arena_t *, size_t, int, int), 
		void           (*arena_mem_free)(void *, uint, int))
{
	arena_t	*ap;
	ap = (arena_t *)kmem_zalloc(sizeof(arena_t), 0);
	ASSERT(ap);
	init_amalloc(ap, CNODEID_NONE, arena_mem_alloc, arena_mem_free);
	return ap;
}

void
kmem_arena_destroy(arena_t *ap)
{
	ASSERT(ap);
	kmem_free(ap, sizeof(arena_t));
}


/*
 * Allocate memory from a private arena.
 */

void *
kmem_arena_alloc(arena_t *ap, register size_t size, register int flags)
{
	void *ptr;
	ASSERT(ap->initted);
	ptr = arena_malloc(ap, size, flags);
	HLOG(UTN('arna','Aloc'), ptr, size);
	return ptr;
}

/*
 * Reallocate memory from a private arena.
 */

void *
kmem_arena_realloc(arena_t *ap, register void *ptr,
				 register size_t nbytes, int flags)
{
	void *ret;
	ASSERT(ap->initted);
	HLOG(UTN('arna','RAl1'), ptr, nbytes);
	ret = arena_realloc(ap, ptr, nbytes, flags);
	HLOG(UTN('arna','RAl2'), ret, nbytes);
	return ret;
}

/*
 * Allocate and zero memory from a private arena.
 */

void *
kmem_arena_zalloc(arena_t *ap, register size_t size, register int flags)
{
	void *ptr;
	ASSERT(ap->initted);
	ptr = arena_zalloc(ap, size, flags);
	HLOG(UTN('arna','ZAlo'), ptr, size);
	return ptr;
}

/*
 * Free memory into an arena.
 */

/*ARGSUSED1*/
void
kmem_arena_free(arena_t *ap, register void *cp, register size_t size)
{
	ASSERT(ap->initted);
	HLOG(UTN('arna','Free'), cp, size);
	arena_free(ap, cp);
}

/****************************************************************/
/*								*/
/*	NUMA_BASE Specific Kernel Heap Interface         	*/
/*								*/
/****************************************************************/
void *
kern_calloc_node(register size_t num, register size_t size, cnodeid_t node)
{
	void *ptr;
	ptr = heapzone_zalloc(num*size, KM_NOSLEEP|NODE_SPECIFIC_FLAG, node);
	HLOG(UTN('knod','CAlo'), ptr, num*size);
	return ptr;
}

void *
kern_malloc_node(register size_t nbytes, cnodeid_t node)
{
	void *ptr;
	ptr = heapzone_malloc(nbytes, KM_NOSLEEP|NODE_SPECIFIC_FLAG, node);
	HLOG(UTN('knod','MAlo'), ptr, nbytes);
	return ptr;
}


void *
kmem_alloc_node(register size_t size, register int flags, cnodeid_t node)
{
	void *ptr;
	ASSERT(flags&VM_NOSLEEP);
	ptr = heapzone_malloc(size, flags|NODE_SPECIFIC_FLAG, node);
	HLOG(UTN('knod','Aloc'), ptr, size);
	return ptr;
}

void *
kmem_realloc_node(register void *ptr, register size_t nbytes, int flags, cnodeid_t node)
{
	void *ret;
	ASSERT(flags&VM_NOSLEEP);
	HLOG(UTN('knod','RAl1'), ptr, nbytes);
	ret = heapzone_realloc(ptr, nbytes, flags|NODE_SPECIFIC_FLAG, node, __return_address);
	HLOG(UTN('knod','RAl2'), ret, nbytes);
	return ret;
}

void *
kmem_zalloc_node(register size_t size, register int flags, cnodeid_t node)
{
	void *ptr;
	ASSERT(flags&VM_NOSLEEP);
	ptr = heapzone_zalloc(size, flags|NODE_SPECIFIC_FLAG, node);
	HLOG(UTN('knod','ZAlo'), ptr, size);
	return ptr;
}




void *
kmem_alloc_node_hint(register size_t size, register int flags, cnodeid_t node)
{
	void *ptr;
	ptr = heapzone_malloc(size, flags|NODE_SPECIFIC_FLAG|VM_NOSLEEP, node);
	if (!ptr)
		ptr = heapzone_malloc(size, flags, node);
	HLOG(UTN('knod','Aloh'), ptr, size);
	return ptr;
}

void *
kmem_zalloc_node_hint(register size_t size, register int flags, cnodeid_t node)
{
	void *ptr;
	ptr = heapzone_zalloc(size, flags|NODE_SPECIFIC_FLAG|VM_NOSLEEP, node);
	if (!ptr)
		ptr = heapzone_zalloc(size, flags, node);
	HLOG(UTN('knod','ZAlh'), ptr, size);
	return ptr;
}


/*
 * Initialize the dynamic memory allocation system.
 */
void
init_kheap(void)
{
	extern mrlock_t	shake_lock;	/* semaphore for shake manager	*/
	/*REFERENCED*/
	cnodeid_t	nodes;
	if (kheap_initialized)
		return;

	/* init zone allocator */
	init_gzone();

	mrlock_init(&shake_lock, MRLOCK_DBLTRIPPABLE, "shake", -1);
	private.p_lticks = 2; /* fake out chkpreempt */
#if defined(DEBUG) && defined(PRHEAP)
	spinlock_init(&prheaplock, "prheaplck");
	idbg_addfunc("prheap", idbg_prheap);
#endif
	kheap_initialized++;	/* must be set AFTER init_mutex */
}

void
init_amalloc(arena_t *ap, cnodeid_t node, 
		void *         (*arena_mem_alloc)(arena_t *, size_t, int, int), 
		void           (*arena_mem_free)(void *, uint, int))
{
	register int i, sz;

	if (ap->initted)
		return;
	/* initialize free queues */
	for (i = 0, sz = ROUNDUP(1); i < NARENAS; i++) {
		ap->arena[i].hu.fl.nextfree = &(ap->arena[i]);
		ap->arena[i].hu.fl.prevfree = &(ap->arena[i]);
		ap->arena[i].blk.thisblksz = sz;
		sz <<= 1;
	}

	spinlock_init(&ap->heaplock, "heaplck");
	ap->initted = 1;
	ap->node    = node;
	ap->arena_mem_alloc = arena_mem_alloc;
	ap->arena_mem_free = arena_mem_free;
}

#ifdef MH_R10000_SPECULATION_WAR
#define HEAPALLOCFLAGS (VM_DIRECT|VM_PHYSCONTIG|KM_NOSLEEP|VM_VM|VM_UNCACHED|VM_NO_DMA)
#else
#define HEAPALLOCFLAGS (VM_DIRECT|VM_PHYSCONTIG|KM_NOSLEEP|VM_VM)
#endif /* MH_R10000_SPECULATION_WAR */

/*
 * heap memory allocation routine. 
 * It tries to allocate memory in the best possible place basd on the
 * flags/nodeid passed.
 * If the flags indicate that the allocation is node_specific, then
 * 	it just tries to do that irrespective of return value, or the 
 * 	possibility of sleeping.
 * 
 * If the flag indicates it's not node specific, but the node is not
 * 	invalid, it's a node preferred, allocation. Try to allocate 
 *	on desired node without sleeping. If successful, good. 
 *	Otherwise, go a generic allocation.
 * If nodeid is invalid, do a generic allocation
 * 
 * On non-numa systems, there are no complications. 
 */

/* ARGSUSED */
void *
heap_mem_alloc(cnodeid_t node, size_t size, int vmflags, int color)
{
#ifdef	NUMA_BASE
	if (NODE_SPECIFIC(vmflags)) {
		ASSERT(node != CNODEID_NONE);
		return kvpalloc_node(node, size, 
				vmflags & HEAPALLOCFLAGS, color);
	}
	/*
	 * Reques is not node specific. But try to do a node-preferred
	 * allocation. If not available, do generic kvpalloc 
	 */
	if (node != CNODEID_NONE) {
		void	*ptr;
		int	newflags;
		/*
		 * Turn off sleep, and turn on no_sleep.
		 */
		newflags = (vmflags & ~KM_SLEEP) | KM_NOSLEEP;
		ptr = kvpalloc_node(node, size,
				      newflags & HEAPALLOCFLAGS, color);
		if (ptr){
			/*
			 * Got a page on required node. return
			 */

			return ptr;
		}
	} 
	/*
	 * Just fall through to do regular kvpalloc which tries to get
	 * some page in the system.
	 */
#endif	/* NUMA_BASE */
	vmflags &= HEAPALLOCFLAGS;
	return (kvpalloc(size, vmflags, color));
}

/*
 * kmem_avail		return estimate of available memory
 *
 * This routine is provided only to support STREAMS bufcall() functionality.
 * Returns:	memory available in small & big pools, plus availrmem
 *		(since we'll attempt to allocate more pools if necessary).
 */
ulong
kmem_avail(void)
{
#ifndef notdef
	return ctob(GLOBAL_AVAILRMEM() - tune.t_minarmem);
#else
	register int i;
	register long size = 0;
	register struct ksa *ksa;

	for (i = 0; i < maxcpus; i++) {
		if ((pdaindr[i].CpuId == -1) ||
				!(ksa = pdaindr[i].pda->ksaptr))
			continue;
	     size += ksa->mi.hunused;
	}
	return ctob(availrmem - tune.t_minarmem) + size;
#endif
}

/*
 *	This version of malloc has been developed to reduce the amount
 *	of k2seg usage and to allow the heap to return unused memory to
 *	the general page pool.
 *
 *	k2seg usage is reduced by constraining all requests <= NBPP to
 *	fit within a page.  For these requests, the minimal header is
 *	two shorts which determine the byte offsets to the previous and
 *	next blocks.  The low bit set in the ``next'' offset
 *	(blk.thisblksz) indicates that the block is allocated.  A
 *	header is always constructed at the end of the page with its
 *	``next'' offset set busy.
 *
 *	N.B.:  Pages of heap are not linked together!!!
 *
 *	Pages are coalesced on free.  After coalescing, if the (free)
 *	chunk starts at a page boundary and points to the page footer,
 *	the entire page can be freed.
 *
 *	Requests for >= (slightly less than) NBPP get whole pages:
 *	the page-aligned user buffer is the cue to free the whole
 *	page(s) at kern_free:  The size info is cached in the 
 *	first page's pfdat pf_rawcnt field.
 */

static void *
arena_malloc(
	arena_t *ap,
	register size_t	nbytes,
	register int	vmflags)
{
	register struct header	*blk;
	register struct header	*newblk;
	register unsigned	nb;	/* size of entire block we need */
	register unsigned	i;
	register unsigned	ao;	/* arena offset */
	register int		locktoken;
	register int		rem = 0;

	/* WARNING: Be careful if you change the following comparision with
	 * syssegsz since the straightforward syssegsz*NBPP results in a
	 * truncated 32-bit value - problem if syssegsz >= 4GB.
	 */
	if (nbytes == 0 || pnum(nbytes) > syssegsz) {
		cmn_err_tag(146,CE_WARN|CE_CPUID, "Kernel arena allocator was passed an invalid size -- %d bytes", nbytes);
		return 0;
	}

	MINFO.halloc++;

	/*
	 * Get gross number of bytes we need.
	 */

	if (vmflags & VM_CACHEALIGN) {
		if (SCACHE_LINESIZE > GRAIN)
			nb = ROUNDUP_SCLINE(nbytes) + MINHEAD;
		else
			nb = ROUNDUP(nbytes) + MINHEAD;
		i = ROUNDUP_SCLINE(nb);
	} else
		i = nb = ROUNDUP(nbytes) + MINHEAD;

	if (i > NBPP - MINFREEBLKSZ - MINHEAD) {
		/*
		 * If the actual number of bytes needed would take more
		 * than a page, or all of a page, with no room to squeeze
		 * out another free block and a page stub, or if the
		 * number of bytes requested rounded to grainsize without
		 * a header would fit just within a page, allocate a whole
		 * page and return the address of the page itself.
		 *
		 * VM_CACHEALIGN is always satisfied since we're returning
		 * a page-aligned address.
		 */
		register void *ptr;

		/* Won't be using a header... */
		nb -= MINHEAD;

		i = btoc(nb);

		ptr = ap->arena_mem_alloc(ap, i, vmflags, 0);


		if (ptr == NULL)
			return(NULL);

		kvatopfdat(ptr)->pf_rawcnt = i;

		if (IS_KSEG2(ptr))
			atomicAddInt(&kheapmapped, i);

		(void)atomicAddInt(&MINFO.heapmem, ctob(i));
		return(ptr);
	}

	/*
	 * Look for a free block large enough to accomodate request.
	 * Will drop out of interal for-loop when either a large enough
	 * block has been found, or when it has looped back to the
	 * arena header (arena header's size is set to NBPP).
	 */
	if (vmflags & VM_CACHEALIGN) {
		for (i = 0, ao = bytetobucket(ap, nb); ao < NARENAS; ao++) {
			register struct header *Arena_head = &(ap->arena[ao]);

			locktoken = HEAPLOCK();
			for (blk = Arena_head->hu.fl.nextfree;
			     blk != Arena_head;
			     blk = blk->hu.fl.nextfree, i++)
			{
				rem = ROUNDUP_SCLINE((__psunsigned_t) blk + MINHEAD) -
				      ((__psunsigned_t) blk + MINHEAD);
				while (rem > 0 && rem < MINFREEBLKSZ)
					rem += SCACHE_LINESIZE;
				if (blk->blk.thisblksz >= rem + nb &&
				    (!(vmflags & VM_DIRECT) || IS_KSEGDM(blk)))
					break;
			}

			if (blk != Arena_head)
				break;

			HEAPUNLOCK(locktoken);
			blk = (struct header *)0;
		}
	} else {
		for (i = 0, ao = bytetobucket(ap, nb); ao < NARENAS; ao++) {
			register struct header *Arena_head = &(ap->arena[ao]);

			locktoken = HEAPLOCK();
			for (blk = Arena_head->hu.fl.nextfree;
			     blk != Arena_head;
			     blk = blk->hu.fl.nextfree, i++)
			{
				if ((blk->blk.thisblksz >= nb) &&
				    (!(vmflags & VM_DIRECT) || IS_KSEGDM(blk)))
					break;
			}

			if (blk != Arena_head)
				break;

			HEAPUNLOCK(locktoken);
			blk = (struct header *)0;
		}
	}

	/* confusing ASSERT -- if VM_DIRECT, make sure is direct */
	ASSERT((blk == NULL) || !(vmflags & VM_DIRECT) || IS_KSEGDM(blk));

	/*	if we didn't find a block, get more memory
	 */
	if (!blk) {
		/*
		 * Allocate another page of memory.
		 */

		blk = ap->arena_mem_alloc(ap, 1, vmflags, 0);


		if (blk == (struct header *)NULL)
			return(NULL);

		if (IS_KSEG2(blk))
			atomicAddInt(&kheapmapped, 1);
		/*
		 * One block is carved out of the page with a struct blk
		 *	at the beginning and the end.
		 *	Pages are not linked in any way.
		 */

		/*
		 * Set rem for cache alignment.
		 */
		if ((vmflags & VM_CACHEALIGN) && SCACHE_LINESIZE > MINHEAD)
			rem = ROUNDUP_SCLINE(MINFREEBLKSZ) - MINHEAD;

		/*
		 * Size includes header.  It is the offset to next/prev block.
		 */
		newblk = (struct header *)((char *)blk + FULLPG);
		blk->blk.thisblksz = newblk->blk.prevblksz = FULLPG;
		blk->blk.prevblksz = 0x0;
		newblk->blk.thisblksz = ANCHOR;	/* low bit implies not free */

		(void)atomicAddInt(&MINFO.heapmem, NBPP);
		(void)atomicAddInt(&MINFO.hunused, NBPP - MINHEAD * 2);
		(void)atomicAddInt(&MINFO.hovhd, MINHEAD * 2);
		locktoken = HEAPLOCK();
	} else {
		DELFREEQ(blk);
	}

	/*
	 * Blk now points to an adequate block.
	 */
	ASSERT(blk->blk.thisblksz && !TESTBUSY(blk->blk.thisblksz));
	if (rem) {
		unsigned sz;

		/*
		 * The following is mostly from carve()
		 */
		ASSERT(SETBUSY(blk->blk.thisblksz));
		ASSERT(blk->blk.thisblksz > rem);
		sz = CLRBUSY(blk->blk.thisblksz - rem);
		ASSERT(sz >= MINFREEBLKSZ);
		blk->blk.thisblksz = CLRBUSY(rem);
		newblk = (struct header *) ((char *) blk + rem);
		newblk->blk.prevblksz = rem;
		newblk->blk.thisblksz = sz;
		addfreeq(ap, blk);
		blk = newblk;
		newblk = NEXT(newblk);
		newblk->blk.prevblksz = sz;
		(void)atomicAddInt(&MINFO.hovhd, MINHEAD);
		(void)atomicAddInt(&MINFO.hunused, -MINHEAD);
	}
	blk->blk.thisblksz = SETBUSY(blk->blk.thisblksz);
	if (carve(ap, blk, nb)) {
		(void)atomicAddInt(&MINFO.hovhd, MINHEAD);
		(void)atomicAddInt(&MINFO.hunused, -MINHEAD);
	}

	/*
	 * Set nb to requested bytes rounded to grainsize.
	 */
	HEAPUNLOCK(locktoken);
	(void)atomicAddInt(&MINFO.hunused,
			   -(CLRBUSY(blk->blk.thisblksz) - MINHEAD));

#ifdef	NUMA_BASE
	blk->blk.nodenum = ap->node;
#endif	/* NUMA_BASE */

	return (void *)((char *)blk + MINHEAD);
}


static void *
arena_zalloc(arena_t *ap, register size_t nb, int flags)
{
	register void *ptr = arena_malloc(ap, nb, flags);
	if (ptr)
		(void) bzero(ptr, nb);
	return(ptr);
}

/*	free(ptr) - free block that user thinks starts at ptr 
*/
static void
arena_free(arena_t *ap, void *vptr)
{
	register struct header *blk;	/* real start of block*/
	register struct header *next;
	register struct header *prev;
	register int		nb;
	register char		*ptr = vptr;
	register int		locktoken;

	MINFO.hfree++;

	if (!ptr) {
		cmn_err_tag(147,CE_PANIC|CE_CPUID, "Kernel arena_free was passed a null pointer");
		/*NOTREACHED*/
	}

	/*
	 * Only allocations of multiple pages are page-aligned.
	 */
	if (poff(ptr) == 0) {
		register struct pfdat *pfd = kvatopfdat(ptr);

		nb = pfd->pf_rawcnt;
		if (!nb)
			cmn_err_tag(148,CE_PANIC, "kern_free: null size");
		if (pfd->pf_use != 1)
			cmn_err_tag(149,CE_PANIC, "kern_free: free ptr @ 0x%x pfdat @ 0x%x use count wrong (%d)", ptr, pfd, pfd->pf_use);

		pfd->pf_rawcnt = 0;
		ap->arena_mem_free(vptr, nb, 0);

		if (IS_KSEG2(vptr))
			atomicAddInt(&kheapmapped, -nb);

		nb = ctob(nb);
		(void)atomicAddInt(&MINFO.heapmem, -nb);
		return;
	}

	blk = (struct header *)(ptr - MINHEAD);
#ifdef	NUMA_BASE
	ASSERT(ap->node == blk->blk.nodenum);
#endif	/* NUMA_BASE */

	if (!TESTBUSY(blk->blk.thisblksz))
		cmn_err_tag(150,CE_PANIC,
			"kern_free: free ptr @ 0x%x block @ 0x%x already free",
			ptr, blk);

	nb = CLRBUSY(blk->blk.thisblksz);

	atomicAddInt(&MINFO.hunused, nb - MINHEAD);
	locktoken = HEAPLOCK();

	blk->blk.thisblksz = nb;
	next = NEXT(blk);
#ifdef	PRHEAP
	if (next && next->blk.prevblksz == blk->blk.thisblksz)
		;
	else
		kern_prheap(10);
#endif
	/*
	 * See if we can compact with preceeding chunk(s).
	 * Only do so if chunk isn't at top of page.
	 */
	if (poff(blk) != 0 &&		/* top of page */
	    !(TESTBUSY((prev = PREV(blk))->blk.thisblksz))) {
		DELFREEQ(prev);
		prev->blk.thisblksz += blk->blk.thisblksz;
		ASSERT(next);
		next->blk.prevblksz = prev->blk.thisblksz;
		blk = prev;
		atomicAddInt(&MINFO.hunused, MINHEAD);
		atomicAddInt(&MINFO.hovhd, -MINHEAD);
	}
#define nextnext prev
	/*
	 * See if we can compact with following chunk.
	 */
	if (!TESTBUSY(next->blk.thisblksz)) {
		nextnext = NEXT(next);
		DELFREEQ(next);
		blk->blk.thisblksz += next->blk.thisblksz;
		next = nextnext;
		next->blk.prevblksz = blk->blk.thisblksz;
		atomicAddInt(&MINFO.hunused, MINHEAD);
		atomicAddInt(&MINFO.hovhd, -MINHEAD);
	}
#undef nextnext

	/*
	 * If blk now consumes a whole page, give it back to the system.
	 */
	if (poff(blk) == 0 && blk->blk.thisblksz == FULLPG) {
		HEAPUNLOCK(locktoken);
		atomicAddInt(&MINFO.heapmem, -NBPP);
		atomicAddInt(&MINFO.hunused, -(NBPP - MINHEAD * 2));
		atomicAddInt(&MINFO.hovhd, -(MINHEAD * 2));
		ASSERT(TESTBUSY(next->blk.thisblksz));
		ap->arena_mem_free((void *)blk, 1, 0);
		if (IS_KSEG2(blk))
			atomicAddInt(&kheapmapped, -1);
	} else {
		addfreeq(ap, blk);
		HEAPUNLOCK(locktoken);
	}
}

/*	realloc(vptr, size, vmflags)
 *
 *	Give the caller a block of size "size", with
 *	the contents pointed to by ptr.  Free ptr.
 */
static void *
arena_realloc(
	arena_t *ap,
	register void *ptr,	/* block to change */
	size_t size,		/* new size */
	int vmflags)
{
	register struct header *blk;	/* block n which ptr is contained */
	register struct header *next;	/* block after blk */
	register void	*newptr;	/* pointer to user's new block */
	register unsigned nb;		/* size of block, allocaters' view */
	register unsigned sz;
	register int	locktoken;

	if (ptr == NULL)
		return arena_malloc(ap, size, vmflags);

	ASSERT(size > 0);

	/*
	 * (multi-) page allocations.
	 */
	if (poff(ptr) == 0) {

		register struct pfdat *pfd = kvatopfdat(ptr);

		if ((sz = pfd->pf_rawcnt) == 0 || pfd->pf_use != 1)
			cmn_err_tag(151,CE_PANIC, "kmem_realloc bad pointer");

		if (btoc(size) == sz) {
			/*
			 * Don't bother to shrink -- not worth the code
			 * for something that will rarely happen.
			 */
			MINFO.halloc++;
			return ptr;
		}

		sz = ctob(sz);	/* set-up for bite_the_bullet */

		/*
		 * Go the hard way.
		 */
		goto bite_the_bullet;
	}

	blk = (struct header *)((char *)ptr - MINHEAD);

	if (vmflags & VM_CACHEALIGN) {
		sz = CLRBUSY(blk->blk.thisblksz);
		goto bite_the_bullet;
	}

	locktoken = HEAPLOCK();
	next = (struct header *)NEXT(blk);

	if (!TESTBUSY(next))
		cmn_err_tag(152,CE_PANIC,
		"kern_realloc: free ptr @ 0x%x block @ 0x%x already free",
		(char *)ptr, blk);

	next = (struct header *)CLRBUSY(next);
	ASSERT(next && next->blk.prevblksz == CLRBUSY(blk->blk.thisblksz));

	/* make blk as big as possible */
	if (!TESTBUSY(next->blk.thisblksz)) {
		/*
		 * At most one following block will be free since
		 * we coalesce blocks in both directions on free.
		 */
		sz = next->blk.thisblksz;
		DELFREEQ(next);
		blk->blk.thisblksz += sz;
		next = NEXT(next);
		next->blk.prevblksz = CLRBUSY(blk->blk.thisblksz);
		HEAPUNLOCK(locktoken);
		atomicAddInt(&MINFO.hunused, -(sz - MINHEAD));
		atomicAddInt(&MINFO.hovhd, -MINHEAD);
	} else
		HEAPUNLOCK(locktoken);

	/* get size we really need */
	nb = ROUNDUP(size) + MINHEAD;

	/*
	 * See if we have enough.
	 * blk.thisblksz has BUSY bit set, but it won't
	 * affect thisblksz >= nb test.  It will still
	 * be set when nb is set to ``sz - blk->blk.thisblksz''
	 * so nb will be correct.
	 */
	if ((sz = blk->blk.thisblksz) >= nb) {
		/*
		 * Carve out the size we need.
		 * Return value is # of bytes returned to the heap.
		 */
		locktoken = HEAPLOCK();
		if (nb = carve(ap, blk, nb)) {
			HEAPUNLOCK(locktoken);
			atomicAddInt(&MINFO.hunused, nb - MINHEAD);
			atomicAddInt(&MINFO.hovhd, MINHEAD);
		} else {
			HEAPUNLOCK(locktoken);
		}

		MINFO.halloc++;

		return ptr;
	}

	sz = CLRBUSY(sz) - MINHEAD;

bite_the_bullet:	/* ... and call alloc */

	newptr = arena_malloc(ap, size, vmflags);

	if (newptr) {
		bcopy((char *)ptr, newptr, size > sz ? sz : size);
		arena_free(ap, ptr);
	}

	return newptr;
}

/*
 * Find bucket for block sized ``nb''.
 */
static int
bytetobucket(arena_t *ap, int nb)
{
	register struct header *ahead;
	register int	bn;

	for (bn = 0, ahead = ap->arena; nb > ahead->blk.thisblksz; ahead++, bn++)
		continue;

	return bn;
}

static void
addfreeq(arena_t *ap, register struct header *blk)
{
	register int	ao;
	struct header *Head;

	ASSERT(blk->blk.thisblksz >= MINFREEBLKSZ);
	ao = bytetobucket(ap, blk->blk.thisblksz);

	Head = &ap->arena[ao];
	blk->hu.fl.prevfree = Head;
	blk->hu.fl.nextfree = Head->hu.fl.nextfree;
	Head->hu.fl.nextfree->hu.fl.prevfree = blk;
	Head->hu.fl.nextfree = blk;
	ASSERT(blk->hu.fl.nextfree != blk);
	ASSERT(blk->hu.fl.prevfree != blk);
}

/*
 * Carve out the right size block from blk.
 * Free the remainder.
 */
static int
carve(arena_t *ap,
	register struct header *blk,
	register int		nb)
{
	register int		sz;
	register struct header *newblk;

	nb = MAX(MINFREEBLKSZ, nb);
	ASSERT(SETBUSY(blk->blk.thisblksz));
	ASSERT(blk->blk.thisblksz > nb);

	if ((sz = CLRBUSY(blk->blk.thisblksz) - nb) >= MINFREEBLKSZ) {
		blk->blk.thisblksz = SETBUSY(nb);
		newblk = (struct header *)((char *)blk + nb);
		newblk->blk.prevblksz = nb;
		ASSERT(!TESTBUSY(sz) && sz >= MINFREEBLKSZ);
		newblk->blk.thisblksz = sz;
		addfreeq(ap, newblk);
		newblk = NEXT(newblk);
		newblk->blk.prevblksz = sz;

		return sz;
	}

	return 0;
}

/*
 * Following zone_* routines are versions of kern_* routines but
 * use zone as the underlying data structure for allocation/freeing.
 *	
 * 	As part of allocation, we pick the zone based on the size,
 * 	and try to allocate a zone from there. There is no extra header
 *	or anything allocated to keep track of the zone from which the
 *	chunk of memory is allocated. Instead, we take advantage of an
 *	assumption in the zone implementation. Each zone allocates a
 *	page, and keeps the entire page for itself. So, the zone allocator
 *	has been changed to keep the zone index to which the page belongs
 *	to in the pfdat (pf_rawcount) field. This is all that's needed
 *	during freeing.
 *
 *	As part of freeing, we get the zone to be freed into  from the
 *	pf_rawcnt field in the pfdat structure of the page being freed.
 *	This gives the index of the zone, and nodepda provides 
 *	mapping between index and zone structure.
 *
 *	realloc works in a similar fashion. use the pf_rawcnt field to
 *	get the zone pointer. check if the zone size can support the
 *	new size needed, and return the same one if it can. Otherwise,
 *	get a new chunk and free this one.
 */
/*ARGSUSED*/
static void *
heapzone_malloc(size_t nbytes, int vmflags, cnodeid_t node)
{
	size_t		nb;	/* size of entire block we need */
	int		index;
	caddr_t		ptr;
	zone_t		*zone;

	/* WARNING: Be careful if you change the following comparision with
	 * syssegsz since the straightforward syssegsz*NBPP results in a
	 * truncated 32-bit value - problem if syssegsz >= 4GB.
	 */
	if (nbytes == 0 || pnum(nbytes) > syssegsz){
		cmn_err_tag(153,CE_WARN, "kernel malloc: invalid size -- %d", nbytes);
		return 0;
	}



	/*
	 * Double word align nb
	 */
	nb = ROUNDUP(nbytes);

	if (vmflags & VM_CACHEALIGN) {
		nb = ROUNDUP_SCLINE(nb);
	}

	if ((nb > (NBPP/2))
#ifdef MH_R10000_SPECULATION_WAR
		/* XXX VM_UNCACHED also requires a whole page?? */
		|| (vmflags & VM_UNCACHED) ||
		(IS_R10000() && (vmflags & (VM_CACHEALIGN | VM_PHYSCONTIG)))
#endif /* MH_R10000_SPECULATION_WAR */
		) {
		int	npgs;
		/*
		 * If the actual number of bytes needed would take more
		 * than a page, or all of a page, with no room to squeeze
		 * out another free block and a page stub, or if the
		 * number of bytes requested rounded to grainsize without
		 * a header would fit just within a page, allocate a whole
		 * page and return the address of the page itself.
		 *
		 * VM_CACHEALIGN is always satisfied since we're returning
		 * a page-aligned address.
		 */
		npgs = btoc(nbytes);

		ptr = heap_mem_alloc(node, npgs, vmflags, 0);

		if (ptr == NULL)
			return(NULL);
		MINFO.halloc++;

		ZONE_INDEX_SET(ptr, (npgs + ZONE_SIZES_PERNODE));

		if (IS_KSEG2(ptr))
			atomicAddInt(&kheapmapped, npgs);

		/*
		 * Use atomic operations to update heapmem stuff.
		 */
		(void)atomicAddInt(&MINFO.heapmem, ctob(npgs));

		return(ptr);
	}


	/*
	 * Find out the zone that matches the required size, and
	 * try to allocate from that zone.
	 */
	index = kmem_heapzone_index_get(nb, node);

	ASSERT((index >= 0) && (index  <= ZONE_SIZES_PERNODE));

	if (node == CNODEID_NONE)
		zone = node_zone(index);
	else	zone = NODE_ZONE(node, index);

        ASSERT(zone);
	ASSERT(kmem_zone_unitsize(zone) >= nb);

	if (NODE_SPECIFIC(vmflags))
		ASSERT(node != CNODEID_NONE);

	ptr = kmem_zone_alloc(ZONE_MAKE_COOKIE(zone), vmflags);

	if (!ptr)
		return ptr;

#if 0
	printf("Addr 0x%x zone 0x%x(%d) used for size %d index %d flags %x\n", 
		ptr, zone, kmem_zone_unitsize(zone), nb, index, vmflags);
#endif

	if (vmflags & VM_CACHEALIGN) {
		/*
		 * Cache aligned allocation assumes that a request for 
		 * cache aligned size will always be satisfied via a 
		 * zone of either the same size, or the size is cache 
		 * aligned. So, we don't have to do anything special to
		 * make sure that it's aligned. 
		 * Assertion is sufficient
		 */
		ASSERT(((__psunsigned_t)ptr & scache_linemask) == 0);
	}

	if (NODE_SPECIFIC(vmflags))
		ASSERT(NASID_TO_COMPACT_NODEID(kvatonasid(ptr)) == node);

	return ptr;

}

static void *
heapzone_zalloc(size_t nb, int flags, cnodeid_t node)
{
	register void *ptr = heapzone_malloc(nb, flags, node);
	if (ptr)
		(void) bzero(ptr, nb);
	return(ptr);
}

/*	free(ptr) - free block that user thinks starts at ptr 
 * 	Get the zone from the pfdat mapping the virtual address, 
 *	and free the block into the zone.
 */
static void
heapzone_free(void *vptr)
{
	caddr_t		ptr;
	zone_t		*zone;
	int		index;

	ptr = (caddr_t)vptr;

	if (!ptr){
		cmn_err_tag(154,CE_PANIC|CE_CPUID, 
		    "kernel heap allocator free routine got a NULL Pointer");
		/*NOTREACHED*/
	}

	/*
	 * All we need to do is to find out which zone the 
	 * pointer belongs to, and invoke kmem_zone_free 
	 * It's easy to find out the zone this pointer was
	 * allocated from. Just go to the pfdat that maps
	 * this pointer, and look in the pf_rawcnt field.
	 * If the field is < ZONE_SIZES_PERNODE, it indicates
	 * the zone index. 
	 * If not, it indicates that this was a direct allocation, and 
	 * invoke the free function associated with the arena pointer.
	 */
	index = ZONE_INDEX_GET(ptr);

	if (index > ZONE_SIZES_PERNODE) {
		int	np;
		/*
		 * Case of direct page allocation mode. Take appropriate
		 * steps.
		 */
		VALID_ZONE_MEM(ptr);
		np = index - ZONE_SIZES_PERNODE;
		if (!np) {
			cmn_err_tag(155,CE_PANIC, "kern_free null size");
			/*NOTREACHED*/
		}

		ZONE_INDEX_RESET(vptr);
		kvpffree(vptr, np, 0);
		MINFO.hfree++;

		if (IS_KSEG2(ptr))
			atomicAddInt(&kheapmapped, -np);

		atomicAddInt(&MINFO.heapmem, -ctob(np));

	} else {
		zone = node_zone(index);
		kmem_zone_free(ZONE_MAKE_COOKIE(zone), vptr);
	}
}

/*	realloc(vptr, size, vmflags)
 *
 *	Give the caller a block of size "size", with
 *	the contents pointed to by ptr.  Free ptr.
 */
/*ARGSUSED*/
static void *
heapzone_realloc(void *ptr, size_t size, int vmflags, cnodeid_t node, inst_t *ra)
{
	caddr_t		*newptr;	/* pointer to user's new block */
	size_t 		sz;
	zone_t		*zone;
	int		index;

	if (ptr == NULL)
		return heapzone_malloc(size, vmflags, node);

	ASSERT(size > 0);

	/*
	 * realloc algorithm is similar to free algorithm.
	 * Use the pf_rawcnt field in the pfdat to figure out
	 * the zone index. 
	 * If the index indicates the page was directly allocated,
	 * bite the bullet if the size is greater than the numbe rof
	 * pages allocated, and reallocate required pages.
	 *
	 * If allocated via some zone, find out the zone index,
	 * check if the zone size can cater for the required size,
	 * and handle it accordingly 
	 */
	index = ZONE_INDEX_GET(ptr);

	if (index > ZONE_SIZES_PERNODE) {

		sz = index - ZONE_SIZES_PERNODE;

		ASSERT(sz);

		if (btoc(size) <= sz) {
			/*
			 * Don't bother to shrink -- not worth the code
			 * for something that will rarely happen.
			 */
			MINFO.halloc++;
			return ptr;
		}

		sz = ctob(sz);	/* set-up for bite_the_bullet */

		/*
		 * Go the hard way.
		 */

	} else {
		zone = node_zone(index);
		ASSERT(zone);
		/*
		 * May be if the size is drastically different, 
		 * we could try to return a smaller sized zone ?
		 */
		if ((sz = kmem_zone_unitsize(zone)) >= size) {
			return ptr;
		}
	}

	ASSERT(size > sz);
	newptr = heapzone_malloc(size, vmflags, node);

	if (newptr) {
		bcopy((char *)ptr, newptr, sz);
#if defined(DEBUG) || defined(DEBUG_ALLOC)
		kmem_poison(ptr, sz, ra);
#endif
		heapzone_free(ptr);
	}

	return newptr;
}

#ifdef	PRHEAP

static struct heaplink heaphead = {&heaphead, &heaphead, 0, 0};
extern time_t time;

#include "sys/pcb.h"
#include <sym.h>
#include <string.h>

static int	adjusted;

#ifdef ELF_KERNEL

#define savepcs(x,y,z)		((x)[0] = (kreg_t)return_address)

#else

/*
 * Return 1 if sp seems like it could be a valid stack pointer.
 */
static int
is_validsp(kreg_t *sp)
{
	if ((private.p_kstackflag == PDA_CURKERSTK) &&
#if EXTKSTKSIZE == 1
		/* kernel stack is extended by a page */
		(sp < (kreg_t *)(KSTACKPAGE-NBPP) ||
#else
		(sp < (kreg_t *)KSTACKPAGE ||
#endif
		 sp > (kreg_t *)KERNELSTACK))
			return 0;

	if ((private.p_kstackflag == PDA_CURINTSTK) &&
		(sp < (kreg_t *)private.p_intstack ||
		 sp > (kreg_t *)private.p_intlastframe))
			return 0;

	if (private.p_kstackflag == PDA_CURIDLSTK ||
				private.p_kstackflag == PDA_CURUSRSTK) {
		printf("Funny stack to call kern_malloc() on.\n");
		return 0;
	}

	return 1;
}

/*
 * A small cache of pdrs is kept so that frequently seen
 * routines will not take long to find.
 */
#define PDR_CACHE_SIZE  64
static RPDR* m_pdr_cache[PDR_CACHE_SIZE];

/*
 * Locate and return the run time procedure for this "pc"
 */
static RPDR*
find_runtime_pdr(kreg_t pc)
{
	RPDR *p, *end;
	u_int base, limit, hash, midpoint;
	typedef unsigned long p_adr_t;

	/* see if pc is in cache */
	hash = (pc >> 2) & (PDR_CACHE_SIZE-1);
	end = &_procedure_table[PSIZE];
	p = m_pdr_cache[hash];

	if (p && ((p_adr_t)pc >= p->adr))
		if (p+1 < end) {
			if ((p_adr_t)pc < (p+1)->adr)
				return p;
		} else
			return p;


	/* do a binary search through the pdr table */
	base = 0;
	limit = PSIZE-1;
	while (base <= limit) {
		midpoint = (base + limit)>>1;
		p = &_procedure_table[midpoint];
		if ((p_adr_t)pc < p->adr)
			limit = midpoint - 1;
		else {
			if (p+1 < end) {
				if ((p_adr_t)pc < (p+1)->adr) {
					m_pdr_cache[hash] = p;
					return p;
				}
			}
			base = midpoint + 1;
		}
	}
	return 0;
}

/* Back trace the stack starting with current "pc" numpcs times. Return 
 *  backtraced "pcs".						   
 */
static void
savepcs(kreg_t *pcs, int numpcs, int skip)
{
	kreg_t		pc;
	kreg_t *	sp;
	int slot;

	extern int start();

	{
	label_t jb;
	setjmp(jb);
	pc =  jb[JB_PC];
	sp = (kreg_t *)jb[JB_SP];
	}

	for (slot = 0; slot < numpcs-1;) {
		RPDR *		p;

		if (!(p = find_runtime_pdr(pc)))
			break;

		/* back up to previous frame */
		sp = (kreg_t *) ((unsigned char *)sp + p->frameoffset);

		if (!is_validsp(sp))
			break;

		/* fetch return pc in this frame */
		pc = sp[p->regoffset >> 2];

		if (pc <= (kreg_t)start)
			break;

		if ( ! skip )
			pcs[slot++] = pc;
		else
			skip -= 1;
	}
	pcs[slot] = 0;

	return;
}
#endif	/* ELF_KERNEL */

/* Time stamp initial blocks which did not get stamped */
static void
adjust_heaptime(void)
{
	register int	locktoken;
	struct heaplink *linkp;

	locktoken = PRHEAPLOCK();
	for (linkp = heaphead.forw; linkp != &heaphead; linkp = linkp->forw) 
		if (linkp->time <= 60)
			linkp->time = time;
	PRHEAPUNLOCK(locktoken);

	adjusted = 1;
}

/*
 * Link all calls to the kernel heap so they can be traced.
 * Add callpc, time stamps and allocated size.
 */
static void
#ifdef ELF_KERNEL
heap_linkup(char *ptr, size_t size, inst_t *return_address)
#else
heap_linkup(char *ptr, size_t size)
#endif /* ELF_KERNEL */
{
	struct heaplink *hlp = (struct heaplink *)ptr;
	register int	locktoken;

	locktoken = PRHEAPLOCK();
	heaphead.forw->back = hlp;
	hlp->forw = heaphead.forw;
	heaphead.forw = hlp;
	hlp->back = &heaphead;
	PRHEAPUNLOCK(locktoken);

  	savepcs(hlp->callpc, MAXPCS, 2);

	if (!adjusted && time > 60)
		adjust_heaptime();

	hlp->time = time;
	hlp->size = size;
	strncpy(hlp->name, get_current_name(), sizeof(hlp->name));
}

static void
heap_unlink(void *ptr)
{
	register struct heaplink *hlp = (struct heaplink *)ptr;
	register int	locktoken;

	locktoken = PRHEAPLOCK();
	hlp->forw->back = hlp->back;
	hlp->back->forw = hlp->forw;
	PRHEAPUNLOCK(locktoken);
}

#define PRHEAP_SIZE	1	/* cnt means size of allocation */
#define PRHEAP_N	2	/* cnt means number of records */
#define PRHEAP_PC	3	/* cnt means calling pc */
#define PRHEAP_GSIZE	4	/* cnt means >= size of allocation */

/*
 * Print kernel heap info. 
 * if arg < 0, then it is taken as an address of the caller - only
 * records with that callpc are printed.
 * If arg > 0 then print that many records out in reverse time order (latest
 * first).
 * If arg == 0 - print all records.
 */
static void
kern_prheap(__psint_t argpc)
{
	register int	locktoken;
	int op = PRHEAP_N;

	locktoken = PRHEAPLOCK();
	if (argpc < 0)
		op = PRHEAP_PC;
	kern_prheap_nolock(argpc, op);
	PRHEAPUNLOCK(locktoken);
}

/* ARGSUSED */
static void
idbg_prheap(__psint_t x, __psint_t a2, int argc, char **argv)
{
	int op = PRHEAP_N;

	if (argc && *argv[0] == '?') {
		qprintf("Print prheap log\n");
		qprintf("Usage:prheap cnt [op]\n");
		qprintf("\tcnt - # interpreted w.r.t op\n");
		qprintf("\top\t1=size of alloc == cnt\n"
			"\t\t2=# of records (default)\n"
			"\t\t3=calling pc == cnt\n"
			"\t\t4=size of alloc >= to cnt\n");
		return;
	}

	switch (argc) {
	case 2:
		op = (int)atoi(argv[1]);
		break;
	}
	kern_prheap_nolock(x, op);
}

static void
kern_prheap_nolock(__psint_t argpc, int op)
{
	struct heaplink *linkp;
	kreg_t callpc;
	/*REFERENCED*/
	time_t tm;
	int i,count=0,flg=0;
	__psunsigned_t offset;
	char *sym;
	int size;
	int total = 0;
	
	/*
	 * Without #ifdef PRHEAP allocated blocks are not linked up
	 * anywhere.  With PRHEAP, extra bytes are allocated,
	 * comprised of two link pointers, the caller's address and the
	 * time.  The blocks get doubly linked to ``heaphead''.
	 *
	 * Subtracting MINHEAD from the doubly-linked list gets us to the
	 * actual heap block.  The BUSY bit set in blk->blk.thisblksz
	 * marks the block as allocated.
	 */
	for (linkp = heaphead.forw; linkp != &heaphead; linkp = linkp->forw) {

	  	count++;
		if (op == PRHEAP_N && argpc > 0 && count > argpc)
			break;
	  	size = linkp->size;
		for (i = 0; i != MAXPCS && linkp->callpc[i]; i++) {
			callpc = linkp->callpc[i];
			if (op == PRHEAP_PC && callpc != argpc)
				break;
			if (op == PRHEAP_SIZE && size != argpc)
				break;
			if (op == PRHEAP_GSIZE && size < argpc)
				break;
			sym = fetch_kname((void *)callpc, 
					  (void *)&offset);
			tm = time - linkp->time;
			if (*sym == 'N') break;
			if (!flg++) {
#ifdef ELF_KERNEL
				qprintf(
				"%s(...)+0x%x\t[0x%x] Sec:%d "
				"addr %lx size %d who <%s>\n",
				sym, offset, callpc, tm, linkp, size, linkp->name);
#else
				qprintf(
				"%s(...)+0x%x\t\t[0x%x], %d "
				"Seconds, Size=%d\n",
				sym, offset, callpc, tm, size);
#endif
				total += size;
			}
			else
				qprintf("%s(...)+0x%x\t\t[0x%x]\n",
				sym, offset, (int)callpc);
		}
#if MAXPCS > 1	/* only bother with separator if printing multiple pc's */
		qprintf("=====\n");
#endif
		flg = 0;
	}
	qprintf("Total: %d\n", total);
}
#endif	/* PRHEAP */

/*
 * Poison the indicated buffer with the specified return address.  We set
 * the bottom bit to 1 in order to cause a bus error if the memory is used
 * as a pointer.  If poisoning has been disabled return immediately.  If
 * the first word of the buffer looks like a poision value, skip return
 * without poisoning.  This allows layered allocation subsystems to poison
 * free()'ed memory at a higher layer without having kmem_zone_free()
 * overwrite the more valuable higher layer return address.
 */
void
kmem_poison(void *buf, size_t size, inst_t *ra)
{
	size_t i;
	__psunsigned_t poison, *bp;
	extern int _ftext[], _etext[];

	if (kmem_do_poison == 0)
		return;

	bp = buf;
	if (*bp >= (__psunsigned_t)_ftext && *bp < (__psunsigned_t)_etext
	    && (*bp & 3) == 1)
		return;

	size /= sizeof(*bp);
	poison = (__psunsigned_t)ra | 1;
	for (i = 0; i < size; i++)
		*bp++ = poison;
}
