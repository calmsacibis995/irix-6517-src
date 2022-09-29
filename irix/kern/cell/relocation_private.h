/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: relocation_private.h,v 1.7 1997/08/19 19:15:31 dnoveck Exp $"

#define	OBJ_TRACE 1

#if DEBUG && OBJ_TRACE
#include <ksys/vproc.h>
extern struct ktrace	*obj_trace;
extern cell_t		obj_trace_id;

#define	OBJ_TRACE2(a,b) { \
	if (obj_trace_id == (b) || obj_trace_id == -1) { \
		KTRACE4(obj_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid()); \
	} \
}
#define	OBJ_TRACE4(a,b,c,d) { \
	if (obj_trace_id == (b) || obj_trace_id == -1) { \
		KTRACE6(obj_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d)); \
	} \
}
#define	OBJ_TRACE6(a,b,c,d,e,f) { \
	if (obj_trace_id == (b) || obj_trace_id == -1) { \
		 KTRACE8(obj_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f)); \
	} \
}
#define	OBJ_TRACE8(a,b,c,d,e,f,g,h) { \
	if (obj_trace_id == (b) || obj_trace_id == -1) { \
		KTRACE10(obj_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f), \
			(g), (void *)(__psint_t)(h)); \
	} \
}
#define	OBJ_TRACE10(a,b,c,d,e,f,g,h,i,j) { \
	if (obj_trace_id == (b) || obj_trace_id == -1) { \
		KTRACE12(obj_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f), \
			(g), (void *)(__psint_t)(h), \
			(i), (void *)(__psint_t)(j)); \
	} \
}
#else
#define	OBJ_TRACE2(a,b)
#define	OBJ_TRACE4(a,b,c,d)
#define	OBJ_TRACE6(a,b,c,d,e,f)
#define	OBJ_TRACE8(a,b,c,d,e,f,g,h)
#define	OBJ_TRACE10(a,b,c,d,e,f,g,h,i,j)
#endif

/*
 * Each cell has a table indicating whether object services
 * support object relocation.
 */
struct obj_celldata {
	obj_service_if_t	*obj_service_table[NUMSVCS];
	obj_relocation_if_t	*obj_callout_table[NUMSVCS];
};

/*
 * KORE steps:
 */
typedef enum {  OBJ_SOURCE_PRELUDE,
		OBJ_SOURCE_PREPARE,
		OBJ_TARGET_PREPARE,
		OBJ_SOURCE_RETARGET,
		OBJ_CLIENT_RETARGET,
		OBJ_SOURCE_BAG,
		OBJ_TARGET_UNBAG,
		OBJ_SOURCE_END,
		OBJ_SOURCE_ABORT,
		OBJ_TARGET_ABORT
} obj_kore_step_t;
	
typedef struct {
	short	depth;
	short	breadth;
} obj_tree_pos_t;

/*
 * The object manifest is the principal data structure used by 
 * the kernel object relocation engine to manage the relocation
 * of a list of objects, or list of object trees, from source to
 * target cell.
 */
struct obj_manifest {
	uint_t		nobj;		/* Object count */
	obj_kore_step_t step;		/* KORE state */
	obj_bag_t	sinfo;		/* Source object info */
	obj_bag_t	tinfo;		/* Target object info */
	cell_t		src;		/* Source cell */
	cell_t		tgt;		/* Target cell */
	obj_tree_pos_t	posn;		/* Current location in object tree */
	obj_rmode_t	rmode;		/* Relocation mode of current object */
};
#define	obj_depth	posn.depth
#define	obj_breadth	posn.breadth

/*
 * During object relocation, the server requests clients to retarget to the
 * new target cell. During this, we need to track how many client cells
 * need to be contacted and how many have confirmed their retargetting
 * so that we know when retargetting is complete. The following
 * structure is used for this. It contains a lock, count and sync
 * variable used in waiting for all the clients to report retargetting
 * complete.
 */
typedef struct {
	lock_t		rt_lock;
	int		rt_count;
	sv_t		rt_sync;
	void		*rt_data1;
	void		*rt_data2;
} obj_retarget_state_t;


/*
 * Bitmap operations for consolidating client retargeting.
 * Bitmaps must be allocated in word (64bit) chunks.
 */
typedef char * bitmap_t;
#define BITS_TO_LONGS(n)	((n-1)/(NBBY*sizeof(long)) + 1)
#define BITS_TO_BYTES(n)	(BITS_TO_LONGS(n) * sizeof(long))
#define bitmap_size(n)		BITS_TO_BYTES(n)
#define bitmap_alloc(n)		(char *) kmem_alloc(BITS_TO_BYTES(n), KM_SLEEP)
#define bitmap_free(bmp,n)	kmem_free((void *) bmp, BITS_TO_BYTES(n))
#define bitmap_zero(bmp,n)	bzero((void *) bmp, BITS_TO_BYTES(n))
#define bitmap_isnull(bmp,n)	(bfcount(bmp, 0, n) == 0)
extern bitlen_t bfcount(char *, bitnum_t, bitlen_t);

static __inline char *
bitmap_zalloc(bitnum_t n)
{
	char *bmp = bitmap_alloc(n);
	bitmap_zero(bmp, n);
	return bmp;
}

#define cellmap_alloc()		bitmap_alloc(MAX_CELLS)
#define cellmap_free(bmp)	bitmap_free(bmp,MAX_CELLS)
#define cellmap_zero(bmp)	bitmap_zero(bmp,MAX_CELLS)
#define cellmap_isnull(bmp)	bitmap_isnull(bmp,MAX_CELLS)
#define cellmap_zalloc()	bitmap_zalloc(MAX_CELLS)
