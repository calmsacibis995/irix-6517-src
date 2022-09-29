/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *
 */

#ident	"$Revision: 1.3 $"

#include <limits.h>
#include <sys/avl.h>
#include <ksys/cell/service.h>

struct credid_s {
	cell_t	 crid_cell;
	ushort_t crid_value;
};
#ifdef CRED_DEBUG
#define	CREDID_MAX	16	
#else
#define	CREDID_MAX	USHRT_MAX
#endif

typedef struct credid_lookup_entry {
	avlnode_t	cr_avlnode;		/* avlnode for lookup */
	credid_t	cr_id;			/* cred id */
	cred_t		*cr_cred;		/* associated cred */
						/* may be NULL for "dead" */
						/* entries requiring flushing */
} credid_lookup_entry_t;

#define	credid_lookup_insert(C, E)	avl_insert(C, (avlnode_t *)(E))
#define	credid_lookup_find(C, I) 	(credid_lookup_entry_t *) \
						avl_find(C, (__psunsigned_t)(I))
#define	credid_lookup_entry_remove(C, E)	avl_delete(C, (avlnode_t*)(E))

#define	CREDID_VALID(credp)	((credp)->cr_id >= 0)

#define	CREDID_CELL(credid)	((credid)>>16)
#define	CREDID_LOCAL(credid)	(CREDID_CELL(credid) == cellid())
#define	CREDID_VALUE(credid)	((credid)&0xFFFF)
/*
 * credids hash into a credid cache "line" which is designed to fit with
 * into a physical cache line
 */
#define	MAX_CCL_PER_CELL 8		/* # of cache lines per cell */
#define	MAX_CCL	MAX_CELLS*MAX_CCL_PER_CELL

#define	MAX_CCL_ENTRY	9		/* # should be chosen carefully */
typedef struct credid_cache_line {
	mrlock_t ccl_lock;		   /* multi-reader lock */
	int	 ccl_hint;		   /* location of last hit */
	credid_t ccl_id[MAX_CCL_ENTRY];	   /* array of packed cred ids */
	cred_t	 *ccl_cred[MAX_CCL_ENTRY]; /* corresponding cred pointers */
} credid_cache_line_t;

#define	CREDID_BUCKET(credid)	\
	(((CREDID_CELL(credid)%MAX_CELLS)*MAX_CCL_PER_CELL)| \
	 ((credid)&(MAX_CCL_PER_CELL-1)))

