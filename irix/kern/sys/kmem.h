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

#ifndef __SYS_KMEM_H__
#define __SYS_KMEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.52 $"

#include <sys/param.h>
/*
 * Definitions for KM_*, must match VM_*
 */
#define KM_SLEEP	0
#define KM_NOSLEEP	0x0001		/* must match VM_NOSLEEP */
#define KM_PHYSCONTIG	0x8000		/* physically contiguous */
#define KM_CACHEALIGN	0x0800		/* guarantee that memory is */
					/* cache aligned */
#define	KM_NODESPECIFIC	0x1000000	/* Allocate on specific node */

/*
 * Function prototypes.
 */

extern	void	*kmem_alloc(size_t, int);
extern	void	*kmem_zalloc(size_t, int);
extern	void	*kmem_realloc(void *, size_t, int);
extern	void	kmem_free(void *, size_t);
extern	ulong	kmem_avail(void);

extern void	*kmem_alloc_node(size_t, int, cnodeid_t);
extern void	*kmem_realloc_node(void *, size_t, int, cnodeid_t);
extern void	*kmem_zalloc_node(size_t, int, cnodeid_t);

extern void	*kmem_alloc_node_hint(size_t, int, cnodeid_t);
extern void	*kmem_zalloc_node_hint(size_t, int, cnodeid_t);



#if defined(_KERNEL) && !defined(_STANDALONE) && defined(BPCSHIFT)

typedef struct arena arena_t;
/*
 * Kmem interface routines which allocate memory from a private arena.
 */

/*
 * Routine to create/destroy a private areana
 */
extern arena_t *kmem_arena_create(void *(*)(arena_t *, size_t, int, int),
					void(*)(void *, uint, int));
extern  void    kmem_arena_destroy(arena_t *);

/*
 * Routines to allocate/free memory from private arena.
 */
extern	void	*kmem_arena_alloc(arena_t *, size_t, int);
extern	void	*kmem_arena_zalloc(arena_t *, size_t, int);
extern	void	*kmem_arena_realloc(arena_t *, void *, size_t, int);
extern	void	kmem_arena_free(arena_t *, void *, size_t);
#endif	/* _KERNEL && !defined(_STANDALONE) && defined(BPCSHIFT) */

/*
 * retrieve amount ot phys/virtual memory zones are taking
 * Interfac used by osview to retrieve heap memory info.
 */
extern void	kmem_zone_mem(int *, int *);

/*
 * Zone routines:
 *	kmem_zone_init(unit-size, zone-name) -- connect to global zone manager;
 *	kmem_zone_alloc(zone, zone-flags) -- alloc a zone unit from the zone;
 *	kmem_zone_zalloc(zone, flags) -- alloc a zeroed zone unit from the zone;
 *	kmem_zone_free(zone, data-pointer) -- free a zone unit into the zone.
 *	kmem_zone_minsize(zone, minunits) -- specify minimum #units in zone
 *	zone_shake(zone) -- standard shake routine
 */
typedef struct zone zone_t;

extern zone_t 	*kmem_zone_init(int, char *);
extern void	*kmem_zone_alloc(zone_t *, int);

extern void	*kmem_zone_zalloc(zone_t *, int);
extern void	kmem_zone_free(zone_t *, void *);
extern int 	kmem_zone_unitsize(zone_t *);

extern int	kmem_zone_minsize(zone_t *zone, int minunits);
extern int	zone_shake(zone_t*);

/*
 * Kmem contig alloc interface to allocate contiguous physical memory.
 */

extern	void *kmem_contig_alloc(size_t, size_t, int);
extern	void kmem_contig_free(void *, size_t);
extern	void *kmem_contig_alloc_node(cnodeid_t, size_t, size_t, int);

/*
 * Private zone interfaces: these zones are never shared;
 * the clients can pre-fill the zones and the zones
 * never get shaken (trimmed).
 *	kmem_zone_private(unit-size, zone-name) -- connect to a zone manager;
 *	kmem_zone_fill(zone, pointer-to-memory, size-of-memory) -- pre-fill;
 *	kmem_zone_reserve(zone, number-of-items-wanted) -- pre-fill.
 *	kmem_zone_reserve_node(cnodeid_t, zone, number-of-items-wanted) -- pre-fill.
 */
extern zone_t	*kmem_zone_private(int, char *);
extern int	kmem_zone_private_mode_noalloc(zone_t *);
extern int	kmem_zone_enable_shake(zone_t *zone);
extern void	kmem_zone_fill(zone_t *, void *, int);
extern int	kmem_zone_reserve(zone_t *, int);
extern int	kmem_zone_reserve_node(cnodeid_t, zone_t *, int);
extern int	kmem_zone_freemem(zone_t *);

/*
 * shake manager routines
 *
 * A resource manager that would like to have its resource
 * trimmed (shaken) when the general resource class is
 * depleted registers its shake routine with the shake manager
 * via shake_register.
 *
 * The resource class manager invokes the various registered
 * routines via shake_shake, passing the resource class id.
 */

extern void	shake_register(int, int (*)(int));
extern int	shake_shake(int);

/*
 * General resource classes defined.
 */
#define SHAKEMGR_MEMORY	1
#define SHAKEMGR_SWAP	2
#define SHAKEMGR_VM	3

#ifdef __cplusplus
}
#endif

#endif /* __SYS_KMEM_H__ */
