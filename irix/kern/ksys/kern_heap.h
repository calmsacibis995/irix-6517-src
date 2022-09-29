/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * This is a kernel heap allocator specific private file. This is
 * used to define the heap allocator specific data structures 
 * Typically no other file (other thand idbg.c) need to include 
 * this file. If you are forced to include this file, you are
 * reaching for some variables that's out of your reach. 
 */
#ifndef __KSYS_KERN_HEAP_H__
#define __KSYS_KERN_HEAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.17 $"

#include <sys/param.h>
#include <sys/sema.h>
#include <sys/types.h>
#include <sys/immu.h>        
#include <sys/nodemask.h>
#include <sys/kmem.h>

/*
 * data structure for a dynamic memory arena
 */

#define LOG2GRAIN	3
#define GRAIN		(1 << LOG2GRAIN)

/*
 * Small block header.  Linkage when not on free list.
 */
struct blk {
	ushort_t prevblksz;
	ushort_t thisblksz;
	/* nodenum used only in NUMA, but this structure must
	 * be 8 bytes for kmem_alloc to return 8-byte aligned memory.
	 */
	__uint32_t nodenum;
};

/*
 * Small block free-list header must include arena linkage
 * info plus free list links.
 */
struct header {
	struct blk	blk;
	union {
		struct {
			struct header	*nextfree;
			struct header	*prevfree;
		} fl;
		char	clientspace[GRAIN];
	} hu;
};

struct zone_freelist {
        struct zone_freelist *zone_free_next;
        struct zone_freelist *zone_free_prev;
};


/*
 * This structure has been laid out such that the first 32 bytes (primary
 * cache line) contains most of the important data accessed in 
 * kmem_zone_quick_alloc() and kmem_zone_free().
 */
struct zone {
	void		*zone_free;
	void		*zone_free_dmap;	/* Direct mapped pages */
	int		zone_free_count;	/* Free elements in zone */
	cnodeid_t	zone_node;	/* Node to allocate memory from */
	short		zone_radius;	/* Radius of memory allocation */
	uchar_t		zone_index;	/* Index in node pda for this size */
	uchar_t		zone_type;	/* Numa/Private zone type indicator */
	short		zone_unitsize;	/* Size in bytes of zone unit */
	short		zone_units_pp;
	short		zone_usecnt;
	int		zone_total_pages; /* num pages allocated to this zone */
	int		zone_minsize;	/* min desired num pages allocated */
	int		zone_kvm;
	unsigned short	zone_pages_freed;
	unsigned short	zone_shake_skip;
	unsigned short	zone_shakes_skipped;
	char		*zone_name;
	struct zone	*zone_next;
	struct pfdat	*zone_page_list; /* List of pfdats for the pages 
					  * allocated for this zone 
					  */
	struct pfdat	*zone_dmap_page_list; /* Direct mapped pages */
	struct pfdat 	*zone_next_page;   /* Next page to be 
					    * scanned by shaked 
					    */ 
	struct pfdat 	*zone_dmap_next_page; /* For dmap pages */
};


/*
 * Arena is a pool from which an arbitrary size of memory is allocated. It has 
 * an alloc and free interface routines to allocate and free memory in terms 
 * of pages. Typically the interface routines are kvpalloc and kvpffree.
 * Private arenas can be created with special alloc and free routines to manage
 * a private list of pages.
 */
#define NARENAS	(BPCSHIFT - 1)

struct arena {
	struct header 	arena[NARENAS];
	lock_t		heaplock; /* Lock for this arena */
	short		initted;/* If initialized */
	cnodeid_t	node;	/* Node ID for this Arena, Used in NUMA mode */
	void *		(*arena_mem_alloc)(struct arena *, size_t, int, int);
	void		(*arena_mem_free)(void *, uint, int);
};

extern	void	init_amalloc(arena_t *, cnodeid_t, 
				void *(*)(arena_t *, size_t, int, int),
				void (*)(void *, uint, int));

/*
 * there are globally created zone lists one shareable and one private.
 * One can also create one's own zone lists by using kmem_gzone_init -
 * this will create a list that is never 'shaken'. Also, one can pick
 * the type of zone list - whether zonemaps will ever be shared or not
 */
typedef struct gzone_s {
	zone_t *zone_list;
	zone_t *zone_shakeptr;
	lock_t	zone_lock;
	int	zone_nzones;
	int	zone_type;
} gzone_t;

/* types */
#define ZONE_GLOBAL		1 /* global shared zone list 	*/
#define ZONE_GLOBAL_PRIVATE	2 /* global private zone list 	*/
#define	ZONE_SET		3 /* Zone set type zones	*/

#define ZONE_MODEMASK		(0xf0)
#define ZONE_TYPEMASK		(0x0f)
#define	ZONE_NOPAGEALLOC	(1 << 4)
#define	ZONE_ENABLE_SHAKE	(1 << 5)
#define	ZONE_MODE(z)		((z)->zone_type & ZONE_MODEMASK)
#define	ZONE_TYPE(z)		((z)->zone_type & ZONE_TYPEMASK)

#define	ZONE_MODESET(z, mode)	((z)->zone_type |= (mode & ZONE_MODEMASK))

#ifdef	NUMA_BASE
#define	zone_set_type(x)	(ZONE_TYPE(x) == ZONE_SET)
#else
#define	zone_set_type(x)	0
#endif	/* NUMA_BASE */

/*
 * Data structure allocated on each node to maintain and manage
 * the Zone data structures. 
 */
#if CELL_IRIX
#ifdef	KMEM_DEBUG
#define ZONE_SIZES_PERNODE              200
#else
#define ZONE_SIZES_PERNODE              60
#endif
#elif CELL_ARRAY
#ifdef	KMEM_DEBUG
#define ZONE_SIZES_PERNODE              180
#else
#define ZONE_SIZES_PERNODE              50
#endif
#else /* non-CELL */
#ifdef	KMEM_DEBUG
#define ZONE_SIZES_PERNODE              150
#else
#define ZONE_SIZES_PERNODE              40
#endif
#endif /* non-CELL */

#if	(NBPP == 4096)
#define	HEAP_ZONE_COUNT			(BPCSHIFT-2)
#elif	(NBPP == 16384)
#define	HEAP_ZONE_COUNT			(BPCSHIFT-4)
#else
<< HEAP_ZONE_COUNT for this page size is not defined >>
#endif

#define kmem_zone_free_count(z)	((z)->zone_free_count)

#ifdef	NUMA_BASE
/*
 * macro to get to the zoneset of required size.
 * this happens to be the index for the right size.
 */
#define node_zone_elem          (&(nodepda->node_zones))
#define NODE_ZONE_ELEM(n)       (&(NODEPDA(n)->node_zones))

#define node_zonesize(i)        (node_zone_elem->node_zoneset_sizes[i])
#define NODE_ZONESIZE(n, i)     (NODE_ZONE_ELEM(n)->node_zoneset_sizes[i])

/*
 * Pointer to a zone of specific Size within a node.
 */
#define node_zone(i)            (node_zone_elem->node_zoneset_ptr[i])
#define NODE_ZONE(n, i)         (NODE_ZONE_ELEM(n)->node_zoneset_ptr[(i)])

#else
/*
 * macro to get to the zoneset of required size.
 * i happens to be the index for the right size.
 */
#define node_zone_elem          (&(nodepda->node_zones))
#define NODE_ZONE_ELEM(n)       (node_zone_elem)

#define node_zonesize(i)        (node_zone_elem->node_zoneset_sizes[i])
#define NODE_ZONESIZE(n, i)     (node_zonesize(i))

/*
 * Pointer to a zone of specific Size within a node.
 */
#define node_zone(i)            (node_zone_elem->node_zoneset_ptr[i])
#define NODE_ZONE(n, i)         (node_zone(i))

#endif	/* NUMA_BASE */

/*
 * Macros to keep track of the zone index in some place specific to
 * 'ptr'. At this time, we stash the zone index by reaching out to
 * the pfdat mapping the pointer, and using the pf_rawcnt field.
 * Using a value of ZONE_CONST_OFFST, provides a way to keep the
 * rawcnt number somewhat insane, and hence conspicuously visible
 */

#define	ZONE_CONST_OFFST	1024

#define	ZONE_INDEX_SET(p, zi) 	\
		(kvatopfdat(p)->pf_rawcnt = ((zi) + ZONE_CONST_OFFST))
#define	ZONE_INDEX_RESET(p) 	(kvatopfdat(p)->pf_rawcnt = 0)
#define	ZONE_INDEX_GET(p) 	(kvatopfdat(p)->pf_rawcnt - ZONE_CONST_OFFST)

/*
 * VALID_ZONE_MEM(ptr) macro checks if the pointer is a valid
 * pointer allocated from the kernel heap. At this time, all kernel
 * heap allocations  leave a mark in the pfdat that the specific
 * page was allocated for heap. When caller hands an address to
 * be freed, we can check if this address was handed out by
 * the heap allocator.
 * If you hit this assertion, it's a good chance that either 
 * the address got corrupted or a bad address is being passed
 * to the heap management routine.
 */
#define	VALID_ZONE_MEM(ptr)		\
	if (ptr)			\
ASSERT(kvatopfdat(ptr) >= PFD_LOW(NASID_TO_COMPACT_NODEID(kvatonasid(ptr))))

#ifdef	NUMA_BASE

#define	NODE_SPECIFIC_FLAG	(KM_NODESPECIFIC)
#else	/* Non NUMA_BASE systems */
#define	NODE_SPECIFIC_FLAG	(0)

#endif	/* NUMA_BASE */
#define	NODE_SPECIFIC(f)	((f) & NODE_SPECIFIC_FLAG)

/*
 * Interval between checks for zone bloat by sched. It is 10 minutes for now.
 */
#define	KMEM_ZONE_PERIOD	600	

typedef struct node_specific_zones {
	/*
	 * Array of zone sizes. This array will be atomically updated
	 * whenever a new zone is created. So, a specific index within
	 * this array on all nodes should have the same value. 
	 * This array is looked up while trying to allocate memory 
	 * via kern_heap mechanism.
	 */
	cnt_t		 node_zoneset_sizes[ZONE_SIZES_PERNODE];
	/*
	 * Array of pointers forming the zone set. Each pointer 
	 * points to a zone of specific size. 
	 */
	zone_t		*node_zoneset_ptr[ZONE_SIZES_PERNODE];
	/*
	 * Define the area needed for HEAP_ZONE_COUNT zones.
	 * This array will be used to initialize a few well known
	 * sizes of zones. Zone data structures for newer 
	 * zones will be allocated dynamically.
	 */
	zone_t		 node_zoneset_array[HEAP_ZONE_COUNT];
	/*
	 * Max limit of free memory that can be accumulated in the node's zones
 	 * before shaked is kicked.
	 */
	ulong		zone_free_memory_limit;
} node_specific_zones_t;

typedef struct node_specific_zones	zoneset_element_t;

#ifdef	NUMA_BASE
/*
 * The zone_t * returned by kmem_zone_init() to users is either a pointer
 * to the actual zone for private zones or a cookie masquerading as a pointer.
 * If its a cookie the LSB is set and the rest of the pointer contains
 * the type.
 */

#define	ZONE_COOKIE_BIT		(__psunsigned_t)1
#define	ZONE_COOKIE_INDEX_SHIFT	1

#define	ZONE_COOKIE_INDEX_MASK	(__psunsigned_t)0xff
#define	ZONE_COOKIE_NODE_SHIFT	9
#define	ZONE_COOKIE_NODE_MASK	(__psunsigned_t)0xffff

#define	ZONE_COOKIE_SIZE_SHIFT	25
#define	ZONE_COOKIE_SIZE_MASK	(__psunsigned_t)0xffff

#define	ZONE_MAKE_COOKIE(zone) \
	((zone_t *)(\
	(((__psunsigned_t)(zone->zone_index) & \
	(ZONE_COOKIE_INDEX_MASK)) << ZONE_COOKIE_INDEX_SHIFT) |\
	(((__psunsigned_t)(zone->zone_node) & \
	(ZONE_COOKIE_NODE_MASK)) << ZONE_COOKIE_NODE_SHIFT) | \
	(((__psunsigned_t)(zone->zone_unitsize) & \
	(ZONE_COOKIE_SIZE_MASK)) << ZONE_COOKIE_SIZE_SHIFT) | \
	ZONE_COOKIE_BIT))

#define	ZONE_COOKIE_TYPE(zone_cookie)	\
		((((__psunsigned_t)(zone_cookie) & ZONE_COOKIE_BIT) \
			== ZONE_COOKIE_BIT) ? ZONE_SET : ZONE_GLOBAL_PRIVATE)

#define	ZONE_COOKIE_INDEX(zone_cookie)	\
		(((__psunsigned_t)(zone_cookie) >> ZONE_COOKIE_INDEX_SHIFT) \
			& ZONE_COOKIE_INDEX_MASK)

#define	ZONE_COOKIE_NODE(zone_cookie)	\
		(((__psunsigned_t)(zone_cookie) >> ZONE_COOKIE_NODE_SHIFT) \
			& ZONE_COOKIE_NODE_MASK)

#define	ZONE_COOKIE_SIZE(zone_cookie)	\
		(((__psunsigned_t)(zone_cookie) >> ZONE_COOKIE_SIZE_SHIFT) \
			& ZONE_COOKIE_SIZE_MASK)

#define KMEM_ZONE_UNITSIZE(zone) \
	((ZONE_COOKIE_TYPE(zone) == ZONE_SET) ? \
		ZONE_COOKIE_SIZE(zone): (zone)->zone_unitsize)
#else
#define	ZONE_MAKE_COOKIE(zone)		(zone)
#define	ZONE_COOKIE_TYPE(zone_cookie)	(((zone_t *)zone_cookie)->zone_type)
#define	ZONE_COOKIE_INDEX(zone_cookie)	(((zone_t *)zone_cookie)->zone_index)
#define	ZONE_COOKIE_NODE(zone_cookie)	(((zone_t *)zone_cookie)->zone_node)
#define	ZONE_COOKIE_SIZE(zone_cookie)	(((zone_t *)zone_cookie)->zone_unitsize)
#define KMEM_ZONE_UNITSIZE(zone)  	((zone)->zone_unitsize)

#endif /* NUMA_BASE */

/*
 * initialize a list of zones
 */
extern void	kmem_gzone_init(gzone_t *, int);
extern zone_t	*kmem_zone_ainit(gzone_t *, int, char *);

/* system wide global heap */
extern	void init_gzone(void);
extern int kmem_heapzone_index_get(int, cnodeid_t node);
extern  void    *heap_mem_alloc(cnodeid_t, size_t, int, int);
extern int	kmem_zone_minsize(zone_t *zone, int minunits);
extern int	kmem_zone_unitsize(zone_t *);
extern void 	kmem_zone_curb_zone_bloat(void);

extern int kmem_do_poison;
extern int kmem_make_zones_private_min;
extern int kmem_make_zones_private_max;
extern void kmem_poison(void *, size_t, inst_t *);

#ifdef __cplusplus
}
#endif

#endif /* __KSYS_KERN_HEAP_H__ */
