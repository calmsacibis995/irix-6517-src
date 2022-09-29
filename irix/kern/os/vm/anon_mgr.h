#ifndef __VM_ANON_MGR_H__
#define __VM_ANON_MGR_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * vm/anon_mgr.h
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
#ident	"$Revision: 1.21 $"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/immu.h>
#include "pcache.h"
#include "scache.h"

/*
 * Internal definitions for the Anonymous Manager.  Not for use outside
 * of the manager and kernel debuggers.
 */

/*
 * Anonymous memory is represented by a tree of anonymous structures that
 * indicates which pages within the region are presently known to the anonymous
 * manager.  For efficiency, anonymous structures are shared when a proces
 * forks.  The parent and the child each receives a new anonymous structure
 * to keep track of pages they modify after the fork.  Each of these structures
 * point to the parent's orignal anon struct which contains the pages
 * that are shared copy-on-write.  A binary tree of anon structs is
 * then formed as the processes continue to fork.  
 *
 * In-core pages belonging to a particular anon node are cached by that node
 * in its a_pcache structure.  When these pages are swapped, their swap
 * handles are saved in the a_scache structure.  In this manner, each anon
 * node has its own page cache for its pages and manages them itself.
 *
 * Each anon tree shares a lock structure to coordinate updates to the
 * tree and the tree's data structures (such as the page and swap handle
 * caches).  The lock structure has a reference count in it which shows
 * how many anon nodes there are in the tree and lets us know when to
 * deallocate the lock.
 *
 * For the moment, we'll just use a simple spin lock for the tree.  The
 * multi-reader lock structure is rather large and I'm concerned about
 * the space overhead especially since most processes will have single
 * node anon trees and hence a multi-reader lock provides no advantages
 * despite all the space it requires.
 */

struct anon_lock {
    union anon_lock_u {
	struct {
		uint_t	l_treelock:1,	/* the tree spin lock itself	     */
			l_refcnt:31;	/* # of anon nodes using this lock   */
	} l_st;
	uint_t		l_all;		/* the whole lock word		     */
    } l_un;

#if TIME_LOCKS
	unsigned	lock_time;
#endif
};

#define ANON_LOCK_BIT	0x80000000	/* mask corresponding to l_treelock  */

typedef struct anon_lock	anon_lock_t;
typedef	struct anon		anon_t;

struct anon {
	anon_t		*a_parent;	/* parent node			     */
	anon_t		*a_left;	/* left  sub-child (parent of fork)  */
	anon_t		*a_right;	/* right sub-child (child of fork)   */
	anon_lock_t	*a_lock;	/* protects anon tree and its page   */
					/* caches and page state transitions,*/
					/* the same lock is shared by all    */
					/* nodes in the tree                 */
	pcache_t	a_pcache;	/* in-core pages for this anon node  */
	scache_t	a_scache;	/* swap handles for this nodes pages */
	int		a_refcnt;	/* reference count on this anon node */
					/* updated with ll/sc, so it must be */
					/* an int			     */
	uint_t		a_depth;	/* depth tree has grown since last   */
					/* prune (used as a hint)	     */
	uint_t		a_limit;	/* max lpn that is valid in parent   */
					/* or above			     */
};


/*
 * Number of levels the anon tree can grow (as indicated by a_depth) between
 * pruning operations.  Pruning operations are done at fork time via
 * anon_dup().
 */

#define PRUNE_THRESHOLD		100	


#define MAXPAGES   (btoc(HIUSRATTACH_64))/* max value for a_limit          */
#define MAXLPN	   (MAXPAGES-1)		/* max logical page number -      */
					/* lpn's run from 0 to MAXPAGES-1 */
						
/*
 * Macros for returning a pointer to the sanon list head for
 * a specified page or cnode.
 */
#define SANON_LIST_FOR_CNODE(cnode)	(NODEPDA(cnode)->sanon_list_head)
#define SANON_LIST_FOR_PFD(pfd)		SANON_LIST_FOR_CNODE(pfdattocnode(pfd))


/*
 * Some macros to hide how the locks are acquired.  As described above,
 * we're just using plain mutex spin locks, not MR locks, at the moment.
 */

#define init_tree_lock(lock) \
		init_bitlock(&(lock)->l_un.l_all, ANON_LOCK_BIT, "anon lock", -1)

/*
 * These macros get and release extra references on the lock.  They must
 * be called with the lock already held for update.  (That's why they
 * don't need to use atomic ops.)
 */

#define tree_lock_use_inc(lock)	((lock)->l_un.l_st.l_refcnt++)

#define tree_lock_use_dec(lock)	((lock)->l_un.l_st.l_refcnt--)

#define tree_lock_refcnt(lock)	((lock)->l_un.l_st.l_refcnt)


/*
 * These macros acquire and release the anon tree lock.  Both of the
 * "acquire" routines return a "lock token" which is the old spl.  This
 * must be passed as the second argument to the "release" routine.
 */

#define acquire_tree_access_lock(lock) acquire_tree_update_lock(lock)

#if TIME_LOCKS

int  acquire_tree_update_lock(anon_lock_t *);
void release_tree_lock(anon_lock_t *, int);

#else

#define acquire_tree_update_lock(lock) \
		mp_mutex_bitlock(&(lock)->l_un.l_all, ANON_LOCK_BIT)

#define release_tree_lock(lock, s)     \
		mp_mutex_bitunlock(&(lock)->l_un.l_all, ANON_LOCK_BIT, (s))

#endif

#ifdef __cplusplus
}
#endif

#endif /* __VM_ANON_MGR_H__ */
