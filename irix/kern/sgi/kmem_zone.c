/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

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
#include <sys/rtmon.h>
#include <ksys/kern_heap.h>

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

/*
 * Zone allocator using Zone sets 
 *
 * NUMA systems need a locality aware zone allocator. Zone
 * allocators are used as a faster version of the regular memory
 * allocator for situations where specific size data structures
 * are needed. It is essential that in such situations, 
 * zone allocator has to try and allocate memory on the node
 * that's closest to the requestor.
 * Following implementation provides a 
 * mechanism to do locality based zone allocation.
 *
 * In this mechanism instead of creating a separate zone for each
 * of the request from user, we try to combine zone requirements
 * of nearby sizes into a single zone. (Yes deliberately causing
 * some memory fragmentation). A set of these zones (one on each
 * node is initialized) on behalf of the inovactor. Sharing of zones
 * between different type of zones avoids some amount of fragmentation.
 * When the caller invokes zone_alloc() they pass the zone handle. 
 * Allocator gets the size of zone needed based on this handle,
 * and allocates the zone on the right node (depending on which node
 * the zone allocator is being invoked).
 */

#define	ZONE_NAMELEN	16	/* Name will of format zone_node.size */
#define	MIN_ZONE_SIZE		sizeof(struct zone_freelist)

/*
 * Zone name array - Array used to create the generic zone names. 
 * Since this array is just used for naming purpose, it's okay to have
 * one copy (Mostly used in debugging mode).
 */
static char zone_names[ZONE_SIZES_PERNODE][ZONE_NAMELEN]; 

/*
 * Copy of the specific zone names created by the users. 
 * We keep this copy around so that it's easy
 * to figure out which sized zone is in use by which subsystem.
 */
#ifdef	KMEM_DEBUG
/*
 * No sharing of zones in this mode. 
 * So, MAX_ZONE_COUNT is same as ZONE_SIZES_PERNODE
 * KMEM_DEBUG can be used to force the zone based allocator to 
 * use "default" mode. Normally, (even in DEBUG mode) kernel tends
 * to combine multiple zones of nearby size to a single zone. 
 *
 * In case we are seeing some memory leak, it would be nice to 
 * force each zone to be dedicated to single purpose. KMEM_DEBUG
 * serves that purpose. 
 * One point to note:. If you turn on KMEM_DEBUG, you need to 
 * rebuild the ENTIRE kernel. This changes some data structures sizes
 * allocated in nodepda.
 */
#define	MAX_ZONE_COUNT	(ZONE_SIZES_PERNODE)

#else
/*
 * in non KMEM_DEBUG mode, each zone will be used by multiple
 * users. So, a multiplication factor of '4' takes care of this.
 * This is mostly debugging info, and is not critical.
 * Code properly handles the case where all the entries are used.
 */
#define	MAX_ZONE_COUNT	(ZONE_SIZES_PERNODE * 4)
#endif

struct zoneuser_names {
	char 	name[14];
	short	index;
}zoneuser_names[MAX_ZONE_COUNT];
int	zone_uindex;

static int kmem_zoneset_index_get(int);

static void kmem_validate_zone(zone_t *zone);

/*
 * Tuneable that specifies the percentage of a node's total memory
 * that can be accumulated before shaked is kicked in by sched.
 */
extern	int	zone_accum_limit;

#if defined(DEBUG) || defined(DEBUG_ALLOC)
/* Zone index for use by zone splitting */
static int zoneset_index = 0;
#endif

/*
 * Macros to be used in the new zone shake algorithm. These macros
 * keep track of the number of chunks that have been allocated from a given
 * page. The pfdats are chained so that shaked can scan through the pages
 * quickly. For more details refer to the new shake algorithm description
 * just before the routine zone_shake().
 */

static __inline void
ZONE_SET_PAGE_INFO(void *vaddr, int count)
{
	register pfd_t *pfd = kvatopfdat(vaddr);

	pfd->pf_pageno = count;
	pfd->pf_tag = vaddr;
}

#define	ZONE_INC_PAGE_COUNT(vaddr) ((kvatopfdat(vaddr))->pf_pageno++)
#define	ZONE_DEC_PAGE_COUNT(vaddr) ((kvatopfdat(vaddr))->pf_pageno--)

#define	ZONE_GET_PAGE_COUNT(pfd)	((pfd)->pf_pageno)
#define	ZONE_GET_PAGE_VADDR(x)		((caddr_t)((x)->pf_tag))

static __inline void
ZONE_LINK_PAGE(void *vaddr, pfd_t **zone_page_list)	
{
	pfd_t 	*pfd = (kvatopfdat(vaddr));
	pfd_t	*next = *zone_page_list;
	if (next)
		next->pf_prev = pfd;
	pfd->pf_next = next;
	pfd->pf_prev = NULL;
	*zone_page_list = pfd;
}
	
static __inline void
ZONE_UNLINK_PAGE(pfd_t *pfd, pfd_t **zone_page_list)	
{
	pfd_t 	*next, *prev;

	next = pfd->pf_next;
	prev = pfd->pf_prev;
	if (prev) {
		if (next)
			next->pf_prev = prev;
		prev->pf_next = next;
	} else {
		*zone_page_list = next;
		if (next)
			next->pf_prev = NULL;
	}
}

/*
 * Check if the given pointer belongs to the specified zone.
 * In order to check this, walk back to the pfdat that maps
 * the 'p' and extract the zone index to which that page belongs
 * to from the rawcnt field. 
 * Check if that value is the same as zone_index
 */
#define	VALID_ZONE_INDEX(z, p)	\
		if (zone_set_type(z)) 	\
			ASSERT(ZONE_INDEX_GET(p) == (z)->zone_index)

/*
 *	Zone allocators.
 */

gzone_t	kmem_zoneset;

gzone_t kmem_private_zones;

#define	ZONELOCK()	mutex_spinlock(&gz->zone_lock)
#define	ZONEUNLOCK(T)	mutex_spinunlock(&gz->zone_lock, T)

static void kmem_zoneset_init(void);

void
init_gzone(void)
{
	kmem_gzone_init(&kmem_zoneset, ZONE_SET);

	kmem_gzone_init(&kmem_private_zones, ZONE_GLOBAL_PRIVATE);
	kmem_zoneset_init();
	idbg_addfunc("zone_verify", kmem_validate_zone);
}

void
kmem_gzone_init(gzone_t *gz, int type)
{
	init_spinlock(&gz->zone_lock, "gzonelk", gz->zone_type);
	gz->zone_list = NULL;
	gz->zone_shakeptr = NULL;
	gz->zone_nzones = 0;
	gz->zone_type = type;
}

/*
 * Pass in the size of a structure.
 * This interface returns the actual number of bytes that will
 * be allocated for each structure.  The second arg is filled
 * with the number of units per page.
 */
static int
kmem_zone_size_roundoff(register int size, int *unitp)
{
	register int nunits, i;

	if (size > NBPP)
		return 0;

	if (size < MIN_ZONE_SIZE) {
		/* ensure minimum sizing */
		size = MIN_ZONE_SIZE;
	}

	/*
	 * round up to 8-byte alignment
	 * SCA -- changed (size_t *) to (uint64_t) to make N32 work
	 */
	size = (size + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) -1);

	nunits = NBPP / size;

	/*	Grow to the largest block that will still contain
	 *	nunits blocks in a page.
	 */
	for ( ; ; ) {
		i = NBPP / (size + sizeof(uint64_t)) ;
		if (i != nunits)
			break;
		size += sizeof(uint64_t);
	}

	*unitp = nunits;
	return size;
}

/* ARGSUSED */
static zone_t *
kmem_zone_struct_init(int size, int nunits, char *zone_name, int type)
{
	zone_t 	*z;
	zone_t	*hostzone;
	int	index;

	/* alloc zero-filled zone struct */
	/*
	 * First we need to allocate a zone structure, and 
	 * initialize the zone structure for the proper size.
	 * Invoke kmem_heapzone_index to get the index of the
	 * zone that can support zone_t structure, allocate
	 * a zone from there, initialize it as needed and
	 * return.
	 */
	index = kmem_heapzone_index_get(sizeof(zone_t), CNODEID_NONE);
	hostzone = node_zone(index);
	ASSERT((index >= 0) && (index  <= ZONE_SIZES_PERNODE));
	ASSERT(hostzone);

	z = kmem_zone_zalloc(hostzone, KM_SLEEP);
	ASSERT(z);

	z->zone_unitsize = size;
	z->zone_units_pp = nunits;
	z->zone_name = zone_name;
	z->zone_usecnt = 1;
	z->zone_type   = type;
	z->zone_node   = CNODEID_NONE;
#ifdef zalloc_did_it
	z->zone_free_count = 0;
	z->zone_free = NULL;
	z->zone_total_pages = 0;
	z->zone_minsize = 0;
	z->zone_kvm = 0;
	z->zone_pages_freed = 0;
	z->zone_shake_skip = 0;
	z->zone_shakes_skipped = 0;
#endif
	return z;
}

/*
 * Allow caller to establish a minimum number of units that will
 * be associated (i.e. in use + free) with this zone at any time.
 * It is expected that this minimum number will be established 
 * once just after the zone is created.  Works only on private 
 * zones.  If the zone allows the zone manager to handle page
 * allocations make an attempt now to add pages as required.
 *
 * A negative value for minunits has no effect.
 *
 * In all cases, the old minsize value is returned.
 */
int
kmem_zone_minsize(zone_t *zone, int minunits)
{
	int old_minsize = zone->zone_minsize;
	int delta, pages_added;

	ASSERT_ALWAYS(ZONE_COOKIE_TYPE(zone) != ZONE_SET);
	ASSERT_ALWAYS(ZONE_TYPE(zone) == ZONE_GLOBAL_PRIVATE);

	if (minunits >= 0) {
		zone->zone_minsize =
			(minunits + zone->zone_units_pp-1) / zone->zone_units_pp;
	}

	delta = zone->zone_minsize - old_minsize;

	if ((delta > 0) &&
	    !(ZONE_MODE(zone) & ZONE_NOPAGEALLOC)) {

		pages_added = kmem_zone_reserve(zone, delta);

		/*
		 * This should rarely happen, since kmem_zone_minsize
		 * should be called during startup when there's lots of memory.
		 */
		if (pages_added < delta)
			cmn_err_tag(319,CE_WARN, "kmem_zone_minsize (%d) not satisfied for zone at %x\n",
				    minunits, zone);
	}

	return(old_minsize);
}


void
zoneuser_name_stash(char *name, int index)
{

	struct zoneuser_names *zu;
	int uindex;

	uindex = atomicAddInt(&zone_uindex, 1);

	/* If you hit this assertion, it's time to bump MAX_ZONE_COUNT */
	ASSERT(uindex < MAX_ZONE_COUNT);

	/*
	 * Take the safe route for non-debug kernels.
	 */
	if (uindex >= MAX_ZONE_COUNT)
		return;

	zu = &zoneuser_names[uindex];
	bcopy(name, zu->name, 13);
	zu->name[13] = 0;
	zu->index = index;
}


char *
zoneuser_name_lookup(int index, int *lastnamep)
{
	int	i = *lastnamep + 1;
	struct zoneuser_names *zu = &zoneuser_names[i];

	for (; i < zone_uindex; i++, zu++) {
		if (zu->name[0] && (zu->index == index)){
			*lastnamep = i;
			return zu->name;
		}
	}
	return 0;
	
}


/*
 * Initialize zoneset for different sizes.
 *
 * link together all the zones under the gzone passed in
 * This provides a convenient mechanism to scan through all the
 * zones in case we need (mostly for debugging purpose)
 * (gzone_t *, that gets passed in currently is the kmem_zones)
 * 
 */

static void kmem_zoneset_sizes_init(void);
static void kmem_heapzone_init(void);
void
kmem_zoneset_init()
{
	kmem_zoneset_sizes_init();
	kmem_heapzone_init();
}

/*
 * Build list of desired zone sizes.
 * Current algorithm tries to setup zones with increasing size,
 * and zone(n)size = zone(n-1)size + (zone(n-1)size/4)
 * We just initialize the zone sizes, and leave the initialization
 * of zone to later time.
 * NOTE: In order to service kmem_alloc requirements, zones of certain
 * sizes are pre-initialized.
 * 
 * Current list builds :
 *   If Zone sizes are bumped by (prev_size/4)
 * 	8     16    24    32    40    48    56    64    
 *	80    96    112   128   160   192   224   256   
 *	320   384   448   512   640   768   896   1024  
 *	1280  1536  1792  2048  2560  3072  3584  4096  
 *	5120  6144  7168  8192  10240 12288 14336 16384 
 *
 *   If Zone sizes are bumped by (prev_size/2)
 *	8     16    24    32    48    64    96    128   
 *	192   256   384   512   768   1024  1536  2048  
 *	3072  4096  6144  8192  12288 16384 
 */

#define ZONE_SIZE_INCR_FACTOR	(2)
#define	MIN_INCR_SIZE		8

void
kmem_zoneset_sizes_init()
{

	int	i;
	int	incr, size;
	int	n;

	size = MIN_ZONE_SIZE;
	for (i = 0; i < ZONE_SIZES_PERNODE; i++) {
		incr = size/ZONE_SIZE_INCR_FACTOR;
		if (incr < MIN_INCR_SIZE)
			incr = MIN_INCR_SIZE;
		while (incr & (incr - 1))
			incr--;
		size += incr;
		for (n = 0; n < numnodes; n++) {
			NODEPDA(n)->node_zones.node_zoneset_sizes[i] = size;
		}
		if (size >=NBPP)
			break;
	}
#if defined(DEBUG) || defined(DEBUG_ALLOC)
	/*
	 * Stash index of next free slot for later use
	 * by zone splitting. 
	 */
	zoneset_index = i+1;
#endif
}

/*
 * kmem_newzone_init
 *	zone_index corresponds to the index within the zone set that needs
 * 	to be initialized.
 *	'n' is the node on which this zone is allocated.
 *	This routine initializes all the fields in the zone structure to
 *	be reasonable.
 */
static void
kmem_newzone_init(gzone_t *gz, zone_t *z, int zone_index, cnodeid_t n)
{
	int	size;
	zone_t	**pzonep;
	zone_t	*nz;

	/*
	 * Initialize the zone structure for the given index. 
	 * z points to the zone structure to be initialized.
	 */
	size = NODE_ZONESIZE(n, zone_index);
	ASSERT((size > 0) && (size <= NBPP));
	ASSERT(NODE_ZONE(n, zone_index) == z);

	ASSERT(z);
	z->zone_free = NULL;
	z->zone_next = NULL;

	/*
	 * Connect new zones to the list. 
	 */
	/*
	 * Add the zone at the right place.
	 * We do a linear search to find the right place. Since this happens
	 * only at boot time (or zone creation time), it's no big deal
	 * NOTE: We never search this list. Looking up a zone is done via
	 * other tables. Sorting in ascending order of sizes is for debugging
	 * convenience.
	 */

	pzonep = &gz->zone_list;
	while (nz = *pzonep){
		if (nz->zone_unitsize > size) 
			break;
		pzonep = &nz->zone_next;
	}

	if (nz) {
		z->zone_next = nz;
	}
	*pzonep = z;

	gz->zone_nzones++;

	z->zone_free_count    = 0;
	z->zone_unitsize    = size;
	z->zone_kvm         = 0;
	z->zone_pages_freed = 0;
	z->zone_units_pp    = (NBPP/size);

	/* 
	 * Assign a name (which ends up allocating space
	 * for the variable), and print over it with proper
	 * size. This should give the right size.
	 */

	sprintf(&zone_names[zone_index][0], "size %d ", size);
	z->zone_name = &zone_names[zone_index][0];

	z->zone_index  = zone_index;
	z->zone_type   = ZONE_SET;
#ifdef	NUMA_SIMULATION1
	/* In NUMA_SIMULATION Mode, memory on node 0 
	 * seems to get used up pretty fast in 
	 * allocating initialization data structures 
	 * So, place some extra hacks for this.
	 */
	z->zone_node = ((n == 0) && numnodes > 2) ? (numnodes - 1) : n;
#else
	/*
	 * XXX 
	 * For SN0 mode, use zone_node same as node.
	 * When we need to have less zone_sets than numnodes,
	 * this value needs to be changed.
	 */
	z->zone_node = n;
#endif	/* NUMA_SIMULATION */

#ifdef	NUMA_BASE
	/*
	 * Fix the radius.
	 * This parameter decides how wide the search would be for
	 * allocating memory from this sized zone.
	 * Cost of a low value is increased use of memory for zones,
	 * and cost of a high value is increase use of time to
	 * search zones on all nodes within the radius.
	 * In addition, this needs to be a tuneable (?per zone size ?)
	 * At this time, we use a max value of 2 and min of physmem_max_radius
	 */
	z->zone_radius = MIN(1, physmem_max_radius);
#endif	/* NUMAB_BASE */
}


/*
 * Initialize the heap specific zones on all nodes. 
 * These zones are used to satisfy kmem_alloc/kern_alloc calls. 
 * Following code would end up creating zones of sizes 
 *	NBPP == 4K =>  08, 16, 32, 64, 128, 256, 512, 1024, 2048
 *	NBPP == 16K => 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192
 *
 */
static void
kmem_heapzone_init()
{
	int	i;
	int	zi;
	int	n;
#define	START_OFFS	(BPCSHIFT - HEAP_ZONE_COUNT)
	zone_t	*z;
	int	size;
	char	zone_name[16];

	for (i = 0; i < HEAP_ZONE_COUNT; i++) {
		size = (1 << (i + START_OFFS));
		zi = kmem_zoneset_index_get(size);
		for (n = 0; n < numnodes; n++) {
			z = &NODE_ZONE_ELEM(n)->node_zoneset_array[i];
			NODE_ZONE(n, zi) = z;
			kmem_newzone_init(&kmem_zoneset, z, zi, n);
			NODE_ZONE_ELEM(n)->zone_free_memory_limit = 
				(zone_accum_limit * NODE_TOTALMEM(n))/100;
		}
		sprintf(zone_name, "Heap %d", size);
		zoneuser_name_stash(zone_name, zi);
	}
}

/*
 * Given the size, find out the heap zone that can cater for the
 * requirement.
 * For heaps, we allocate every power of two. So, Calculate the
 * next power of 2 for the size, and find out the index for that.
 */
int
kmem_heapzone_index_get(int size, cnodeid_t node)
{
	int	i;

	cnt_t	*zone_sizes = node_zone_elem->node_zoneset_sizes;

	for (i=0; i < ZONE_SIZES_PERNODE; i++){
		if (zone_sizes[i] < size) 
			continue;
		if (node == CNODEID_NONE) {
			if (node_zone(i))
				return i;
			else continue;
		} else if (NODE_ZONE(node, i))
			return i;
	}

	cmn_err_tag(143,CE_PANIC, "Could not find heap zone for size %d\n", size);
	/*NOTREACHED*/
}

#ifdef	KMEM_DEBUG

/*
 * In KMEM_DEBUG mode, we don't want each zone of different size to get
 * its own zone structure. This is very useful to track down any sort
 * of memory corruption or memory leak problem.
 *
 * So, in this mode, everytime kmem_zoneset_index_get is called, 
 * we return a new index. 
 *
 * Heap allocator code makes sure that kmem_zoneset_index_get is
 * called only when a new zone needs to be created. 
 */

static int
kmem_zoneset_index_get(int size)
{
	static	int	zoneset_index = 0;
	int		n;
	int		nunits;

	/*
	 * zoneset_index points to the next zone we would like to use.
	 * So, in this mode, reinitialize the node_zoneset_sizes array
	 * to indicate the zone size it corresponds to .
	 */

	ASSERT(zoneset_index < ZONE_SIZES_PERNODE);

	size = kmem_zone_size_roundoff(size, &nunits);

	for (n = 0; n < numnodes; n++) {
		NODEPDA(n)->node_zones.node_zoneset_sizes[zoneset_index] = size;
	}
	return zoneset_index++;
}

#else	/* !KMEM_DEBUG */

/*
 * In non KMEM_DEBUG mode, kmem_zoneset_index_get() tries to look up
 * the list of zones already created, and if there is a zone of
 * nearby size, it returns that zone to be used. 
 * 
 * In addition, in non KMEM_DEBUG mode, the node_zoneset_sizes is 
 * initialized to "preferred" zone sizes. In addition, the 
 * sizes are also arranged in increasing order so that a sequential
 * look up gives the best fit..
 *
 * Heap allocator code makes sure that kmem_zoneset_index_get is
 * called only when a new zone needs to be created. 
 */
/*ARGSUSED*/
static int
kmem_zoneset_index_get(int size)
{
	int	i;
	cnt_t	*zone_sizes = node_zone_elem->node_zoneset_sizes;

#if defined(DEBUG) || defined(DEBUG_ALLOC)
	/*
	 * If the requested size (rounded up to one of the zone sizes)
	 * is equal to the debug systune kmem_private_zone then return
	 * a unique zone index for this request.  This aids in finding
	 * zone leaks by forcing all users of a particular zone size
	 * to have their own zones.  Note that rounding up size below
	 * will not hurt the standard follow on zone index search
	 * algorithm.
	 */

	if (kmem_make_zones_private_min <= size &&
	    kmem_make_zones_private_max >= size) {
		if (zoneset_index >= ZONE_SIZES_PERNODE)
			cmn_err(CE_PANIC,
				"Could not find zone index for size %d\n",
				 size);
		for (i = 0; i < numnodes; i++)
			NODEPDA(i)->node_zones.node_zoneset_sizes[zoneset_index] = size;
		return zoneset_index++;
	}
#endif

	for (i=0; i < ZONE_SIZES_PERNODE; i++){
		if (zone_sizes[i] < size) 
			continue;
		return i;
	}
	cmn_err_tag(144,CE_PANIC,"Could not find zone index for size %d\n", size);
	/*NOTREACHED*/

}

#endif	/* KMEM_DEBUG */
/*
 * Create and initialize zone data structure for 'zone_index'
 */
void
kmem_zoneset_create(gzone_t *gz, int zone_index)
{
	/*REFERENCED*/
	int	size;
	int	n;
	zone_t	*newzone;

	size = node_zonesize(zone_index);
	ASSERT(size > 0 && size <= NBPP);

	for ( n = 0; n < numnodes; n++) {
		/*
		 * Allocate memory for the zone structure, and 
		 * initialize the zones.
		 * We are invoking kmem_alloc with sleep. Potentially
		 * this could sleep, and kmem_zone_init could sleep
		 * for ever. (particularly on node zero. 
		 * This may have to be fixed. 
		 */
		ASSERT(NODE_ZONE(n, zone_index) == 0);
		newzone = kmem_zalloc_node(sizeof(zone_t), KM_NOSLEEP, n);
		ASSERT_ALWAYS(newzone);
		NODE_ZONE(n, zone_index) = newzone;
		kmem_newzone_init(gz, newzone, zone_index, n);
	}

}


/*ARGSUSED*/
zone_t *
kmem_zoneset_zoneinit(gzone_t *gz, int size, char *zone_name)
{
	int	zone_index;
	zone_t	*zone;
	int	s;
	int	n;
	/*
	 * Look through the zone_sizes to check which one fits the 
	 * size passed in by the caller, and return that.
	 */

	s = ZONELOCK();

	zone_index = kmem_zoneset_index_get(size);
	ASSERT(zone_index >= 0);

	if (node_zone(zone_index) == (zone_t *)0) {
		kmem_zoneset_create(&kmem_zoneset, zone_index);
	}
	for (n = 0; n < numnodes; n++) {
		zone = NODE_ZONE(n, zone_index);
		zone->zone_usecnt++;
	}

	ASSERT(node_zone(zone_index));
	zone = node_zone(zone_index);
	ASSERT(zone);
	zoneuser_name_stash(zone_name, zone_index);


#ifdef	DEBUG1
	printf("Returning zone 0x%x unitsize: %d for zone %s size %d\n", 
		zone, zone->zone_unitsize, 
		zone_name, size);
#endif	/* DEBUG */

	ZONEUNLOCK(s);

	return zone;
}


zone_t *
kmem_zone_init(register int size, char *zone_name)
{
	zone_t	*zone;

	zone = kmem_zoneset_zoneinit(&kmem_zoneset, size, zone_name);
	if (zone) 
		return (zone_t *)(ZONE_MAKE_COOKIE(zone));
	else 
		return (zone_t *)0;

}


zone_t *
kmem_zone_private(register int size, char *zone_name)
{
	return kmem_zone_ainit(&kmem_private_zones, size, zone_name);
}

/*
 * Set the private zone to not support any page allcoations.
 */
int
kmem_zone_private_mode_noalloc(zone_t *zone)
{
	if (!zone)
		return 0;

	ASSERT(ZONE_TYPE(zone) == ZONE_GLOBAL_PRIVATE);

	if (ZONE_TYPE(zone) != ZONE_GLOBAL_PRIVATE)
		return 0;
	
	ZONE_MODESET(zone, ZONE_NOPAGEALLOC);
	return 1;
}


/*
 * This is a kludgey call that tells the zone allocator that the
 * caller intends to be a "good citizen" and make it easy for the
 * zone allocator's zone_shake function to work.  Specifically:
 *	1) The zone allocator should track allocs/frees on this page
 *	   (in order to do the zone_shake properly)
 * 	2) The caller will never free blocks from other zones into 
 *	   this zone, and the caller will never free blocks from this
 * 	   zone into other zones.
 * 	3) The caller will add a full page at a time whenever it
 * 	   expands the space allocated to this zone.
 *
 * This should be called whenever we have a PRIVATE zone and wish
 * to use the standard "vanilla" shake routine, zone_shake.
 */
int
kmem_zone_enable_shake(zone_t *zone)
{
	if (!zone)
		return 0;

	ASSERT(ZONE_TYPE(zone) == ZONE_GLOBAL_PRIVATE);

	ZONE_MODESET(zone, ZONE_ENABLE_SHAKE);

	return 1;
}

zone_t *
kmem_zone_ainit(gzone_t *gz, register int size, char *zone_name)
{
	int nunits, type;
	register zone_t *z = NULL;
	register unsigned locktoken;

	if ((size = kmem_zone_size_roundoff(size, &nunits)) == 0)
		return (zone_t *)NULL;
	
	type = gz->zone_type;
	/*
	 * a little game - for PRIVATE types, we don't want to check the
	 * zone list at all - but this isn't a time critical function
	 * and we'll save code by looping twice..
	 */
	for (locktoken = ZONELOCK(); ; ) {
#ifndef DEBUG
		if (type == ZONE_GLOBAL) {
			register zone_t *zone;

			/*
			 * See if there is already another zone with the
			 * same unit size.  If so, we'll share it.
			 * If we're debugging, then we skip this so that
			 * all of the zones show up in idbg zone.
			 */
			for (zone = gz->zone_list; zone; zone = zone->zone_next)
			{
				if (zone->zone_unitsize == size) {
					zone->zone_usecnt++;
					ZONEUNLOCK(locktoken);
					if (z)
						kmem_free(z, sizeof(zone_t));
					return zone;
				}
			}
		}
#endif

		if (z) {
			z->zone_next = gz->zone_list;
			gz->zone_list = z;
			gz->zone_nzones++;
			ZONEUNLOCK(locktoken);
			return z;
		}

		ZONEUNLOCK(locktoken);
		z = kmem_zone_struct_init(size, nunits, zone_name, type);
		locktoken = ZONELOCK();
	}
	/*NOTREACHED*/
	return((zone_t *)NULL);
}

void *
kmem_zone_zalloc(register zone_t *zone, int flags)
{
	register void	*ptr;

	ptr = kmem_zone_alloc(zone, flags);
	if (ptr)
		bzero((char *)ptr, KMEM_ZONE_UNITSIZE(zone));
	return ptr;
}

/*
 * Allocate entity from named zone.
 */
/*ARGSUSED*/
static void *
kmem_zone_quick_alloc(zone_t *zone, int flags)
{
	void	**zone_freep;
	void	*ptr;
	int 	s;
	ASSERT(zone);

	/*
	 * Zone-alloc uses the low bit of the zone_free pointer as a lock
	 * bit (MP) -- the free routine, and the shake routine, use atomic
	 * assembler routines that ``honor'' the low bit but use ll/sc to
	 * manipulate the lists without having to grab locks (read: don't
	 * have to manipulate interrupt priority levels).
	 *
	 * mutex_psbitlock sets splhi and the lock bit on this pointer-sized
	 * word, returns the old ipl value indirectly, and the old value
	 * of the ``lock'' word directly.
	 */
#if	(_MIPS_SZPTR == 64)
	/*
	 * there is no scarcity of direct mapped space in 64 bit kernels.
	 * So, we always use it.
	 */
	ASSERT(zone->zone_free == 0);
	zone_freep = &zone->zone_free_dmap;
#else

	if (flags & (VM_DIRECT|VM_NO_DMA)){
		zone_freep = &zone->zone_free_dmap;
	} else {
		/*
		 * If callers don't care what type of zone, choose the
		 * one that has pages.
		 * Priority is for non-direct map pages.
		 */
		if (zone->zone_free || !zone->zone_free_dmap)
			zone_freep = &zone->zone_free;
		else	zone_freep = &zone->zone_free_dmap;
	}

#endif
	s = mutex_psbitlock((__psunsigned_t *)zone_freep, 1);
#if MP
	ptr = (void *)((__psunsigned_t )*zone_freep & ~1);
#else
	ptr = *zone_freep;
#endif
	
	if (ptr) {
		register struct zone_freelist *next_chunk;
#ifdef DEBUG
		long pp = *(long *)ptr;
		if (pp & 1)
			cmn_err(CE_PANIC,"(pp & 1) %x %x ",ptr, pp);
#endif

		if ((ZONE_TYPE(zone) != ZONE_GLOBAL_PRIVATE) ||
		    (ZONE_MODE(zone) & ZONE_ENABLE_SHAKE))
			ZONE_DEC_PAGE_COUNT(ptr);
		VALID_ZONE_MEM(ptr);
		VALID_ZONE_MEM(*(void **)ptr);
		zone->zone_free_count--;
		next_chunk = ((struct zone_freelist *)(ptr))->zone_free_next;
		if (next_chunk)
			next_chunk->zone_free_prev = NULL;
		*zone_freep = next_chunk;
		/*
		 * The store into zone_free above cleared lock bit --
		 * just reset interrupt priority level.
		 */


		splx(s);
		return ptr;
	}

	mutex_psbitunlock((__psunsigned_t *)zone_freep, 1, s);


	return 0;

}

#ifdef	NUMA_BASE
/*
 * call back function which gets called as part of kmem_zone_alloc
 */
void *
kmem_zone_quick_alloc_node(cnodeid_t cnode, void *arg1, void *arg2)
{
	int	zone_index = (long)arg1;
	int	vmflags    = (long)arg2;
	void	*ptr;

	zone_t	*zone = NODE_ZONE(cnode, zone_index);
	ptr = kmem_zone_quick_alloc(zone, vmflags);
	return ptr;
}

#endif	/* NUMA_BASE */

#ifdef	KMME_ZONE_LOG
struct zone_alloc_log {
	void	*zone;
	void	*ptr;
} zlog[2048];
int	zl_inx;
#define	ZLOG(name, z, p)	\
	zlog[zl_inx].zone = z; zlog[zl_inx++].ptr = (void *)((__psunsigned_t)(p) | (o)); zl_inx %= 2048; \
	LOG_TSTAMP_EVENT(RTMON_ALLOC, TSTAMP_EV_ALLOC, \
			 (name), (uint64_t)(z), (uint64_t)(p) \
			 (uint64_t)__return_address);

int
idbg_zlog_lookup(__psint_t t)
{
	int	i;
	for (i = 0; i < 2048; i++) {
		if ((zlog[i].zone == (void *)t) ||
			(__uint64_t)zlog[i].ptr & ~1LL == t) {
		qprintf("zone 0x%x ptr 0x%x\n", 
			zlog[i].zone, zlog[i].ptr);
		}
	}
	return 0;
}
#else
#define	ZLOG(name, z, p)	\
	LOG_TSTAMP_EVENT(RTMON_ALLOC, TSTAMP_EV_ALLOC, \
			 (name), (uint64_t)(z), (uint64_t)(p), \
			 (uint64_t)__return_address);
#endif

int	debug_zone = 1;


void *
kmem_zone_alloc(register zone_t *zone, int flags)
{
	int	i, s;
	void	*ptr, *tmp;
	struct zone_freelist *current_chunk, *next_chunk, *old_chunk;
	size_t	size;
	void	**zone_freep;
	pfd_t 	**zone_page_list;

	ASSERT(zone);
	MINFO.halloc++;

	/*
	 * If its a cookie type of zone pointer get the real zone pointer.
	 */
	if (ZONE_COOKIE_TYPE(zone) == ZONE_SET) {
		ASSERT(ZONE_COOKIE_INDEX(zone) < ZONE_SIZES_PERNODE);
		/*
		 * Change the zone pointer only if caller has not
		 * specified node specific allocation. Otherwise, caller
		 * is expected to pass right zone.
		 */
		if (!NODE_SPECIFIC(flags))
			zone = node_zone(ZONE_COOKIE_INDEX(zone));
		else 
			zone = NODE_ZONE(ZONE_COOKIE_NODE(zone), 
				ZONE_COOKIE_INDEX(zone));
		ASSERT(zone->zone_unitsize == 
				node_zone(zone->zone_index)->zone_unitsize);
	}

	/*
	 * Try quick alloc : this checks if the zone structure already has
	 * pages, carves a zonelet from there and returns it.
	 */
	if (ptr = kmem_zone_quick_alloc(zone, flags)){
		VALID_ZONE_INDEX(zone, ptr);
		ZLOG(UTN('zone', 'Alo1'), zone, ptr);
		return ptr;
	}

	if ((ZONE_TYPE(zone) == ZONE_GLOBAL_PRIVATE) && 
		(ZONE_MODE(zone) & ZONE_NOPAGEALLOC)) {
		ZLOG(UTN('zone', 'Alo2'), zone, NULL);
		return NULL;
	}

#ifdef	NUMA_BASE
	/*
	 * On numa systems, if the request is not for memory on specific
	 * node, lookup the zones on neighboring nodes to get required
	 * memory. If zone_radius is zero, no need to search..
	 * 
	 */
	if (zone_set_type(zone) && !NODE_SPECIFIC(flags) && zone->zone_radius) {
		cnodeid_t	newnode;
		/*
		 * Search zones on neighboring nodes for memory.
		 */
		ptr = physmem_select_neighbor_node(zone->zone_node, 
						   zone->zone_radius,
						   &newnode, 
						   kmem_zone_quick_alloc_node,
						   (void *)(long)zone->zone_index,
						   (void *)(long)flags);
		if (ptr) {
			ZLOG(UTN('zone', 'Alo3'), zone, ptr);
			return ptr;
		}
	}
#endif	/* NUMA_BASE */

	ptr = heap_mem_alloc(zone->zone_node, 1, flags, 0);

	if (!(char *)ptr) {
		ZLOG(UTN('zone', 'Alo4'), zone, ptr);
		return NULL;
	}

	/*
	 * Since we could have gotten the page from a different node than
	 * what zone corresponds to, switch the zone pointers..
	 * Non-numa systems need nothing..
	 */
	if (zone_set_type(zone)){
		cnodeid_t	n;
		n = NASID_TO_COMPACT_NODEID(kvatonasid(ptr));
		if (zone->zone_node != n)
			zone = NODE_ZONE(n, zone->zone_index); 
	}

#ifdef MP
	/*
	 * Before going ahead, check if the zone pointed to by 'zone'
	 * can allocate a zonelet.
	 * We could get a zonelet for two reasons. First, we could be
	 * racing with a different cpu to allocate a page for the 
	 * zone we started with, and other cpu won the race..
	 * Second, the new zone calculated above, may be different than
	 * the ones we started off with.
	 */
	if (tmp = kmem_zone_quick_alloc(zone, flags)){
		kvpffree(ptr, 1, 0);
		ZLOG(UTN('zone', 'Alo5'), zone, tmp);
		return tmp;
	}
#endif	/* MP */

	size = zone->zone_unitsize;
	ASSERT(size >= 1);


	i = zone->zone_units_pp;
	current_chunk = (struct zone_freelist *)ptr;
	current_chunk->zone_free_prev = 0;
	next_chunk = current_chunk;
	while (--i > 0 ) {
		next_chunk = 
			(struct zone_freelist *)((char *)next_chunk + size);
		current_chunk->zone_free_next = next_chunk;
		next_chunk->zone_free_prev = current_chunk;
		current_chunk = next_chunk;
	}

	ZONE_INDEX_SET(ptr, zone->zone_index); 

	ZONE_SET_PAGE_INFO(ptr, zone->zone_units_pp - 1);

	if (IS_KSEGDM(ptr)
#ifdef MH_R10000_SPECULATION_WAR
				|| (kvatopfn(ptr) < pfn_lowmem)
#endif
	) {
		zone_freep = &zone->zone_free_dmap;
		zone_page_list = &zone->zone_dmap_page_list;
	} else {
		zone_freep = &zone->zone_free;
		zone_page_list = &zone->zone_page_list;
	}

	/*
	 * Free all but the first data item atomically.
	 */

	s = mutex_psbitlock((__psunsigned_t *)zone_freep, 1);

	ZONE_LINK_PAGE(ptr, zone_page_list);

	zone->zone_total_pages++;
	if (IS_KSEG2(ptr))
		zone->zone_kvm++;
	if (zone->zone_units_pp > 1) {
		tmp = ((struct zone_freelist *)ptr)->zone_free_next;
		((struct zone_freelist *)tmp)->zone_free_prev = 0;
		old_chunk = (struct zone_freelist *)
					((__psunsigned_t)*zone_freep & ~1);
		current_chunk->zone_free_next = 
				(struct zone_freelist *)old_chunk;
		if (old_chunk) 
			old_chunk->zone_free_prev = current_chunk;
		zone->zone_free_count += zone->zone_units_pp - 1;
		*zone_freep = tmp; 	/* Unlocks the lock */
		splx(s);
		ZLOG(UTN('zone', 'Alo6'), zone, ptr);
		return ptr;
	}
	mutex_psbitunlock((__psunsigned_t *)zone_freep, 1, s);

	ZLOG(UTN('zone', 'Alo7'), zone, ptr);
	return ptr;
}

void
kmem_zone_free(register zone_t *zone, void *ptr)
{
	void	**zone_freep;
	int	s;

	ASSERT(zone);
	VALID_ZONE_MEM(ptr);
	if (ZONE_COOKIE_TYPE(zone) == ZONE_SET) {
		/*
		 * True only on non-numa systems using zone set mode.
		 */
		/*REFERENCED*/
		cnodeid_t	n;
		/* In NUMA mode, get the pointer to the right zone. */
		ASSERT(ZONE_COOKIE_INDEX(zone) < ZONE_SIZES_PERNODE);
		n = NASID_TO_COMPACT_NODEID(kvatonasid(ptr));
		zone = NODE_ZONE(n, ZONE_COOKIE_INDEX(zone)); 
		/*
		 * Check if the page offset of the zone is a multiple
		 * of the size. This should be true if this page belongs
		 * to this zone.
		 */
		ASSERT((poff(ptr) % zone->zone_unitsize) == 0);
		VALID_ZONE_INDEX(zone, ptr);
	}


	MINFO.hfree++;
	ASSERT(zone && ptr);
	VALID_ZONE_INDEX(zone, ptr);

#if defined(DEBUG) || defined(DEBUG_ALLOC)
	kmem_poison(ptr, KMEM_ZONE_UNITSIZE(zone), __return_address);
#endif
	zone_freep = IS_KSEGDM(ptr)
#ifdef MH_R10000_SPECULATION_WAR
					|| (kvatopfn(ptr) < pfn_lowmem)
#endif
			 ? &zone->zone_free_dmap : &zone->zone_free;

	s = mutex_psbitlock((__psunsigned_t *)zone_freep, 1);

	if ((ZONE_TYPE(zone) != ZONE_GLOBAL_PRIVATE) ||
	    (ZONE_MODE(zone) & ZONE_ENABLE_SHAKE))
		ZONE_INC_PAGE_COUNT(ptr);
	((struct zone_freelist *)(ptr))->zone_free_next = 
		(struct zone_freelist *)((__psunsigned_t)*zone_freep & ~1);
	((struct zone_freelist *)(ptr))->zone_free_prev = NULL;
	if (((struct zone_freelist *)(ptr))->zone_free_next)
		((struct zone_freelist *)(ptr))->zone_free_next->zone_free_prev
									= ptr;
	zone->zone_free_count++;
	*zone_freep = ptr;	/* UNLOCKS the lock */
	splx(s);
	ZLOG(UTN('zone', 'Free'), zone, ptr);
}


/*
 * Forcefeed memory into the named zone.
 */
void
kmem_zone_fill(register zone_t *zone, void *ptr, int fillsize)
{
	int	i, s, size, freesize;
	void	**zone_freep;
	struct	zone_freelist *current_chunk, *next_chunk, *old_chunk;
	pfd_t	**zone_page_list; 


	/* zone_shake fails if zone units straddle page boundaries */
	ASSERT_ALWAYS(pnum(ptr) == pnum((__psunsigned_t)ptr+fillsize-1));

	ASSERT(ZONE_COOKIE_TYPE(zone) != ZONE_SET);
	size = zone->zone_unitsize;
	freesize = fillsize / size;
	i = freesize;

	current_chunk = (struct zone_freelist *)ptr;
	current_chunk->zone_free_prev = 0;
	next_chunk = current_chunk;

	while (--i > 0 ) {
		next_chunk = 
			(struct zone_freelist *)((char *)next_chunk + size);
		current_chunk->zone_free_next = next_chunk;
		next_chunk->zone_free_prev = current_chunk;
		current_chunk = next_chunk;
	}

	if (IS_KSEGDM(ptr)
#ifdef MH_R10000_SPECULATION_WAR
				|| (kvatopfn(ptr) < pfn_lowmem)
#endif
	) {
		zone_freep = &zone->zone_free_dmap;
		zone_page_list = &zone->zone_dmap_page_list;
	} else {
		zone_freep = &zone->zone_free;
		zone_page_list = &zone->zone_page_list;
	}

	ZONE_SET_PAGE_INFO(ptr, zone->zone_units_pp);
	s = mutex_psbitlock((__psunsigned_t *)zone_freep, 1);
	ZONE_LINK_PAGE(ptr, zone_page_list);
	old_chunk = (struct zone_freelist *)((__psunsigned_t)*zone_freep & ~1);
	current_chunk->zone_free_next = (struct zone_freelist *)old_chunk;
	if (old_chunk)
		old_chunk->zone_free_prev = current_chunk;
	zone->zone_free_count += freesize;
	zone->zone_total_pages += btoc(fillsize);	/* in pages */
	if (IS_KSEG2(ptr))
		zone->zone_kvm += btoc(fillsize);	/* in pages */
	*zone_freep = ptr;      /* Unlocks the lock */
	splx(s);
}

/* ARGSUSED */
int
kmem_zone_reserve_node(cnodeid_t cnode, register zone_t *zone, register int size)
{
	void	*ptr;

	ASSERT(ZONE_COOKIE_TYPE(zone) != ZONE_SET);
	while (size > zone->zone_free_count * zone->zone_unitsize) {
		if (cnode == CNODEID_NONE)
			ptr = kvpalloc(1, VM_NOSLEEP, 0);
		else
			ptr = kvpalloc_node(cnode, 1, VM_NOSLEEP, 0);

		if (ptr == NULL)
			break;

		kmem_zone_fill(zone, ptr, NBPP);
	}

	return zone->zone_free_count;
}

int
kmem_zone_reserve(register zone_t *zone, register int size)
{
	return(kmem_zone_reserve_node(CNODEID_NONE, zone, size));
}

int
kmem_zone_unitsize(zone_t *zone)
{
	return (KMEM_ZONE_UNITSIZE(zone));

}


#ifdef DEBUG
int zone_shakes;
int zone_shake_pages;
int zone_shake_nothing;
int zone_shakes_skipped;
int zone_shakes_ignore;
#endif


/*
 * Shake from the given list.
 * The shake algorithm has been changed from the O(num_free_entries**2) to 
 * O(num_chunks_per_page). Considering that num_free_entries is 
 * thousands and num_chunks_per_page is in tens the performance gain is 
 * tremendous.
 * The new shake algorithm keeps the freelist as a doubly linked list.
 * Also whenever a page is allocated for a given zone the pfdat corresponding
 * to that page is linked to a list in the zone. The pfdat list is kept in
 * zone_dmap_page_list or zone_page_list depending on the whether address 
 * mapping the page is a direct map address or not. The pfdat contains two
 * values. A count (in pf_pageno) of the number of chunks of that page that
 * are in the zone freelist and the virtual address at which the page is
 * mapped. This data is updated during kmem_zone_alloc() and kmem_zone_free().
 * The linked list of the pfdat is updated only when we allocate a new  page
 * in kmem_zone_alloc() so it does not happen very often. The only thing
 * that happens in kmem_zone_quick_alloc() and kmem_zone_free() (the performance
 * critical paths) is the update of the count.
 * The zone algorithm is quite simple. It goes through the pfdat list twice
 * once for the zone_page_list and another for zone_dmap_page_list. It holds
 * zone freelist lock as the count can change if we don't hold the lock.
 * The algorithm compares the count against zone_units_pp. If the count matches
 * zone_units_pp it means all the chunks in the page are on the freelist. 
 * As we know the virtual address at which the page is mapped (from the pfdat)
 * and the size of the zone we can unlink all the chunks that belong to that
 * page from the freelist immediately. Note that we don't have to do a linked
 * list search to find the chunks in the freelist as the chunks are in a doubly
 * linked list. We unlock the zone freelist lock  after each page so that
 * we don't hold the lock too long.
 */

/*
 * Scan a 100 pages before unlocking the lock. Of course if we find one that
 * we can free unlock the lock before freeing it.
 */
#define	MAX_NUM_PAGES_SCANNED	100

int
zone_shake(register zone_t *zone)
{
	struct zone_freelist *cur_chunk, *next_chunk, *prev_chunk;
	int 	s, ret = 0;
	void	**zone_freep, *freelist_head;
	int	unit_size, num_pages_scanned, num_chunks;
	pfd_t 	*current_page, **zone_page_list, **next_pagep;
	caddr_t	page_addr;

#ifdef	DEBUG
	zone_shakes++;
#endif

	ASSERT((ZONE_TYPE(zone) != ZONE_GLOBAL_PRIVATE) ||
		(ZONE_MODE(zone) & ZONE_ENABLE_SHAKE));

	if (zone->zone_shakes_skipped < zone->zone_shake_skip) {
		zone->zone_shakes_skipped++;
#ifdef DEBUG
		zone_shakes_skipped++;
#endif
		return 0;
	}

	if (zone->zone_free_count < zone->zone_units_pp) {
		/*
 		 * Not enough units to form a page. 
		 */
		return 0;
	}

#if	(_MIPS_SZPTR == 64)
	/*
	 * In 64 bit kernels, only direct map pointer has zones.
	 */
	zone_freep = &zone->zone_free_dmap;
	zone_page_list = &zone->zone_dmap_page_list;
	next_pagep = &zone->zone_dmap_next_page;
#else
	zone_freep = zone->zone_free ? &zone->zone_free : &zone->zone_free_dmap;
	if (zone->zone_free) {
		zone_freep = &zone->zone_free;
		zone_page_list =  &zone->zone_page_list;
		next_pagep = &zone->zone_next_page;
	} else {
		zone_freep = &zone->zone_free_dmap;
		zone_page_list =  &zone->zone_dmap_page_list;
		next_pagep = &zone->zone_dmap_next_page;
	}
#endif
	unit_size = zone->zone_unitsize;


	/*
	 * Following code would be executed possibly twice for each zone
	 * Once for the zone_freelist and second for zone_freelist_dmap
	 * Restart below for the other free list.
	 */
again:

	/*
	 * Remove the free list from the zone so that we don't hold
	 * the lock during garbage collection.
 	 */
	s = mutex_psbitlock((__psunsigned_t *)zone_freep, 1);
	
	if (*next_pagep == NULL)
		*next_pagep = *zone_page_list;
	current_page = *next_pagep;

	/*
	 * For every chunk in the freelist see if the chunk's page can be
	 * freed. We scan 100 pages before unlocking the lock if don't find
 	 * a page that cannot be freed. If not we unlock the lock for every
	 * page we free. This should keep the lock time low.
	 */


	num_pages_scanned = 0;
	while (current_page && (zone->zone_total_pages > zone->zone_minsize)) {
		*next_pagep = current_page->pf_next;
		num_pages_scanned++;
		if (ZONE_GET_PAGE_COUNT(current_page) == zone->zone_units_pp) {
			page_addr = ZONE_GET_PAGE_VADDR(current_page);
			VALID_ZONE_INDEX(zone, page_addr);
			cur_chunk = (struct zone_freelist *)page_addr;
			num_chunks = zone->zone_units_pp;

			freelist_head = (void *)
				((__psunsigned_t )*zone_freep & ~1);
			while (num_chunks--) {
				next_chunk = cur_chunk->zone_free_next;
				prev_chunk = cur_chunk->zone_free_prev;

				if (prev_chunk) {
					prev_chunk->zone_free_next = next_chunk;
					if (next_chunk)
						next_chunk->zone_free_prev 
								= prev_chunk;
				} else {
					ASSERT(freelist_head == cur_chunk);
					freelist_head = 
					(void *)((__psunsigned_t)next_chunk);
					if (next_chunk)
						next_chunk->zone_free_prev 
							= NULL;
				}

				cur_chunk = (struct zone_freelist *)
					((caddr_t)cur_chunk + unit_size);
				zone->zone_free_count--;
			}

			/*
			 * Zero the index saved while allocating,
			 * and free page.
			 */
			ZONE_INDEX_RESET(page_addr);
			ZONE_UNLINK_PAGE(current_page, zone_page_list);
			ASSERT(pbase(freelist_head) != (__psunsigned_t)page_addr);
			*zone_freep = freelist_head; /* Unlocks the lock */
			splx(s);

			kvpffree((void *)page_addr, 1, 0);

			if (IS_KSEG2(page_addr))
				atomicAddInt(&zone->zone_kvm, -1);
			zone->zone_pages_freed++;
			atomicAddInt(&zone->zone_total_pages, -1);
			ret++;
			s = mutex_psbitlock((__psunsigned_t *)zone_freep, 1);
			num_pages_scanned = 0;
		} else if (num_pages_scanned == MAX_NUM_PAGES_SCANNED) {
			mutex_psbitunlock((__psunsigned_t *)zone_freep, 1, s);
			delay(10);
			s = mutex_psbitlock((__psunsigned_t *)zone_freep, 1);
		}
		current_page = *next_pagep;
	}

	mutex_psbitunlock((__psunsigned_t *)zone_freep, 1, s);

	/*
	 * In first pass we would have shaken the list in zone->zone_free.
	 * Now shake the zones hanging from zone->zone_free_dmap.
	 */
	if (zone_page_list == &zone->zone_page_list) {
		zone_page_list = &zone->zone_dmap_page_list;
		zone_freep = &zone->zone_free_dmap;
		next_pagep = &zone->zone_dmap_next_page;
		goto again;
	}

	if (ret == 0) {
		zone->zone_shake_skip++;
#ifdef DEBUG
		zone_shake_nothing++;
#endif
	} else {
		zone->zone_shake_skip = 0;
#ifdef DEBUG
		zone_shake_pages += ret;
#endif
	}
	zone->zone_shakes_skipped = 0;

	return ret;
}

/* ARGSUSED2 */
static int
kmem_zone_ashake(gzone_t *gz, int level)
{
	register zone_t *zone;
	int iterations;
	int ret = 0;

	if (gz->zone_list == NULL)
		return 0;
	iterations = gz->zone_nzones / 8;

	ASSERT(level == SHAKEMGR_MEMORY);
	do {
		/*
		 * Since shaked invokes this, and only one
		 * shaked can be running at any time, 
		 * no locking is needed.
		 */
		if ((zone = gz->zone_shakeptr) == NULL)
			zone = gz->zone_list;
		gz->zone_shakeptr = zone->zone_next;
		if (zone->zone_free_count >= zone->zone_units_pp)
			ret += zone_shake(zone);
#ifdef	DEBUG
		else
			zone_shakes_ignore++;
#endif	/* DEBUG */
	} while (--iterations > 0);

	return ret;
}

int
kmem_zone_shake(int level)
{
	return kmem_zone_ashake(&kmem_zoneset, level);
}

/*
 * addminfo's little helper -- dump zone stats
 */
static void
kmem_zone_amem(gzone_t *gz, int *physmem, int *kvm)
{
	register int locktoken;
	register zone_t *zone;
	int v = 0;
	int p = 0;

	locktoken = ZONELOCK();
	for (zone = gz->zone_list; zone; zone = zone->zone_next) {
		v += zone->zone_kvm;
		p += zone->zone_total_pages;
	}
	ZONEUNLOCK(locktoken);
	*physmem += p;
	*kvm += v;
}

void
kmem_zone_mem(int *physmem, int *kvm)
{
	*kvm = 0;
	*physmem = 0;

	kmem_zone_amem(&kmem_zoneset, physmem, kvm);

	kmem_zone_amem(&kmem_private_zones, physmem, kvm);
}

/*
 * Periodically check to see if we are accumulating too much memory in
 * zones. If we are "shake" up the zones. We need to 
 * do this periodically because normally shaked gets woken up only by vhand
 * and if the system has lots of memory (eg., 11 G) shaked never gets to
 * run even though we have tons of memory accumulated in zones.
 */
void
kmem_zone_curb_zone_bloat()
{
	zone_t *zone;
	int	index;
	size_t	freemem_in_zones; /* in pages for this node */
	cnodeid_t	n;

	for (n = 0; n < numnodes; n++) {
		freemem_in_zones = 0;
		for (index = 0; index < ZONE_SIZES_PERNODE; index++) {
			zone =  NODE_ZONE(n, index);
			if (!zone) continue;
			freemem_in_zones += 
				zone->zone_unitsize * zone->zone_free_count;
		}
		freemem_in_zones = btoct(freemem_in_zones);
		if (freemem_in_zones > 
			NODE_ZONE_ELEM(n)->zone_free_memory_limit) {
			for (index = 0; index < ZONE_SIZES_PERNODE; index++) {
				zone =  NODE_ZONE(n, index);
				if (!zone) continue;
				zone_shake(zone);
			}
		}
	}
}

/*
 * Shake manager.
 */

typedef struct shakefunc {
	int	(*sf_func)(int);
} sf_t;

typedef struct shakelist {
	size_t	shake_size;
	int	shake_mgr;
	int	shake_ing;
	struct shakelist *shake_next;
	sf_t	shake_func[1];
} sl_t;

static struct shakelist *shakelist;	/* list of shake managers/routines */
mrlock_t	shake_lock;		/* semaphore for shake manager	*/

#define	newsize(sl) (sizeof(sl_t) + \
		(sl)->shake_size * sizeof(shakelist->shake_func))

/*
 * Register a shake routine.
 */
void
shake_register(int shake_mgr, int (*shake_func)(int))
{
	register sl_t	*sl, **slp;

	mrlock(&shake_lock, MR_UPDATE, PSWP);

	for (slp = &shakelist; sl = *slp; slp = &(sl)->shake_next) {
		if (sl->shake_mgr == shake_mgr) {
			*slp = sl->shake_next;
			sl = kmem_realloc(sl, newsize(sl), KM_SLEEP);
			sl->shake_func[sl->shake_size++].sf_func = shake_func;
			sl->shake_next = shakelist;
			shakelist = sl;
			mrunlock(&shake_lock);
			return;
		}
	}

	sl = kmem_alloc(sizeof(sl_t), KM_SLEEP);
	sl->shake_size = 1;
	sl->shake_ing = 0;
	sl->shake_next = shakelist;
	shakelist = sl;
	sl->shake_mgr = shake_mgr;
	sl->shake_func[0].sf_func = shake_func;

	mrunlock(&shake_lock);
}

#ifdef	KMEM_ZONE_DEBUG
#include <sys/kthread.h>
__uint64_t	shake_log[1024];
static int	sh_indx;
#define	SH_LOG(kt, t)	shake_log[sh_indx++ % 1024] = ((__uint64_t)kt | t)
#else
#define	SH_LOG(kt, t)
#endif

/*
 * Call shake routines of a given type.
 */
int
shake_shake(int shake_mgr)
{
	sl_t	*sl;
	int	ret = 0;
	int	count;

	if (cmrlock(&shake_lock, MR_ACCESS) == 0)
		return -1;

	SH_LOG(curthreadp, 1);
	for (sl = shakelist; sl; sl = sl->shake_next) {
		if (sl->shake_mgr == shake_mgr) {
			if (sl->shake_ing)
				break;
			count = sl->shake_size;
			while (--count >= 0)
			    ret += (*sl->shake_func[count].sf_func)(shake_mgr);
			break;
		}
	}
	SH_LOG(curthreadp, 0);
	mrunlock(&shake_lock);

	return ret;
}


/*
 * IDBG function that verifies the freelist is correct.
 */
static void
kmem_validate_zone(zone_t *zone)
{
	struct zone_freelist *freelist, *current_chunk, *previous_chunk;
	int	num_entries = 0;

	freelist = zone->zone_free_dmap;

	current_chunk = freelist;
	previous_chunk = NULL;
	while (current_chunk) {
		if (current_chunk->zone_free_prev != previous_chunk)
			qprintf("Previous chunk does not match current_chunk 0x%x previous_chunk 0x%x \n",current_chunk, previous_chunk);
		previous_chunk = current_chunk;
		current_chunk = current_chunk->zone_free_next;
		num_entries++;
	}
	if (num_entries != zone->zone_free_count)
		qprintf("Expected number of free entries %d actual %d\n",
				zone->zone_free_count, num_entries);
}

#ifdef	DEBUG

#include <sys/clksupport.h>

void
kmem_test(int test_zone_size, int num_kmem_tries)
{
	void *ptr;
	zone_t	*test_zone;
	int	i;
	/* REFERENCED */
	long long start_time;
	/* REFERENCED */
	long long  elapsed_time;	
	

	printf("Testing kmem alloc size %d tries %d\n", test_zone_size,
				num_kmem_tries);
	test_zone = kmem_zone_init(test_zone_size, "Test zone");

	/*
	 * Do the initial allocation.
	 */
	ptr = kmem_zone_alloc(test_zone, KM_NOSLEEP);
	kmem_zone_free(test_zone, ptr);

#if	(defined(IP19) || defined(IP25) || defined(IP27))
	start_time = GET_LOCAL_RTC;
#endif
	for (i = 0; i < num_kmem_tries; i++) {
		ptr = kmem_zone_alloc(test_zone, KM_NOSLEEP);
		kmem_zone_free(test_zone, ptr);
	}
#if	(defined(IP19) || defined(IP25) || defined(IP27))
	elapsed_time = GET_LOCAL_RTC - start_time;
	printf("Total elapsed time 0x%llx\n", elapsed_time*NSEC_PER_CYCLE);
#endif
}


void
kmem_test_shake(int test_zone_size, int node)
{
	int index = kmem_heapzone_index_get(test_zone_size, CNODEID_NONE);
	zone_t	*zone;
	/* REFERENCED */
	int	ret;
	/* REFERENCED */
	long	long start_time;
	/* REFERENCED */
	long long  elapsed_time;	

        ASSERT((index >= 0) && (index  <= ZONE_SIZES_PERNODE));

        if (node == CNODEID_NONE)
                zone = node_zone(index);
        else    zone = NODE_ZONE(node, index);
	printf("Shaking zone 0x%x size %d node %d\n", zone, test_zone_size, node);
	printf("Zone free size %d\n", zone->zone_free_count);
	kmem_validate_zone(zone);
#if	(defined(IP19) || defined(IP25) || defined(IP27))
	start_time = GET_LOCAL_RTC;
#endif
	ret = zone_shake(zone);
#if	(defined(IP19) || defined(IP25) || defined(IP27))
	elapsed_time = GET_LOCAL_RTC - start_time;
	printf("Zone shake shook %d pages time %0xx\n",ret,elapsed_time *NSEC_PER_CYCLE);
#endif
	printf("Zone free size %d\n", zone->zone_free_count);
}

#endif

int
kmem_zone_freemem(zone_t *zp)
{
	return zp->zone_free_count;
}
