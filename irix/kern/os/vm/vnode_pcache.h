/*
 * os/vm/vnode_pcache.h
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

#ifndef __OS_VM_VNODE_PCACHE_H
#define __OS_VM_VNODE_PCACHE_H


#include <sys/types.h>
#include <ksys/pcache.h>

#ident	"$Revision: 1.13 $"


struct vnode;
struct pfdat;
struct vfs;
struct ktrace;

/*
 * vnode_pcache_t
 *	Data structure used to manage Vnode Page cache.
 *	One of this type would be part of each vnode data structure,
 *	and will be used to maintain/manage the vnode page cache
 *
 *	vnode_pcache_t has two parts.
 *	v_pagecache the data structure used by the page cache routines
 *	to maintain and manage page cache.
 *
 *	Rest of the fields used to provide a reference count mechanism
 *	on the vnode pagecache (and hence the vnode object). 
 *	A detailed description of the synchronization mechanism
 *	is at the top of vnode_pcache.c
 *
 *	Field used to provide the vnode reference mechanism:
 *
 *		v_pcacheref :   Reference count taken on vnode pagecache
 *				by threads trying to hold the vnode
 *				page cache and hence the vnode object
 *				from being stolen under them.
 *
 *	Field used to lock vnode pagecache
 *		v_pcacheflag:	lock bit and wait bit. 
 *
 *			VNODE_PCACHE_LOCKBIT: Bit lock for page cache.
 *			VNODE_PCACHE_WAITBIT: Indicates if a thread
 *						needs a wakeup.
 *
 *	It's not easy to merge the two fields into a single 'uint'.
 *	Problem is, the lock bin in v_pcacheflag is NOT protecting the
 *	update of the v_pcacheref field. Bit unlock routines donot use
 *	ll/sc to unlock the bit. They just use xor. So, if both these
 *	fields share the same int, we can have two threads one doing
 *	xor on the lock bit, and other doing a ll/sc on cacheref.
 *	This would lead to inconsistent update of fields.
 *	Two possible ways to collaps these:
 *		- Change the mutex_bitunlock to use ll/sc 
 *			- not preferred as it adds overhead for all others..
 *		- Use a different unlocking routine -- pain to maintain.
 *	So, we will burn extra four bytes. 
 */
typedef struct vnode_pcache_s {
	int		v_pcacheref;
	uint		v_pcacheflag;
	pcache_t	v_pagecache;
} vnode_pcache_t;

/*
 * Aliases to fields in vnode_pcache_t
 * 	Aliases include the field name (v_pcache) of this structure 
 * 	in the vnode structure 
 *	So, you can directly access these fields from vnode using the
 *	following aliases.
 */
#define	v_pcache	v_pc.v_pagecache
#define	v_pcacheflag	v_pc.v_pcacheflag
#define	v_pcacheref	v_pc.v_pcacheref


/*
 * Bit positions within the v_pcacheflag field where the lockbit and
 * the wait bit are placed.
 * These bit positions are used while doing the atomic operations
 * on the bit fields.
 * NOTE: It's necessary to do atomic operations while writing to any of the
 * bits within v_pcacheflag field 
 */

#define	VNODE_PCACHE_LOCKBIT	(1)
#define	VNODE_PCACHE_WAITBIT	(2)

#define	VNODE_PCACHE(vp)	(&(vp)->v_pcache)

#define	VNODE_PCACHE_LOCK(vp, s)	\
	(s) = mutex_bitlock(&(vp)->v_pcacheflag, VNODE_PCACHE_LOCKBIT)

#define	VNODE_PCACHE_UNLOCK(vp, s)	\
	mutex_bitunlock(&(vp)->v_pcacheflag, VNODE_PCACHE_LOCKBIT, (s))

#define	VNODE_PCACHE_ISLOCKED(vp)	\
	bitlock_islocked(&(vp)->v_pcacheflag, VNODE_PCACHE_LOCKBIT)

#define	VNODE_PCACHE_WAITING(vp)	\
			((vp)->v_pcacheflag & VNODE_PCACHE_WAITBIT)

/*
 * Bump the reference on the vnode, in order to prevent it from
 * going away. This counter is different than the one used by
 * vnode (v_count) since, it's much easier to do this in this way,
 * than trying to get a reference on vnode via vn_get() which is
 * pretty heavy, and could result in sleepwait. 
 * Since this is done in the pagealloc() path, using a separate
 * field provides a quick method of preventing vnode from going away.
 */
#define	VNODE_PCACHE_INCREF(vp)	(atomicAddInt((int *)&(vp)->v_pcacheref, 1))
#define	VNODE_PCACHE_DECREF(vp)	(atomicAddInt((int *)&(vp)->v_pcacheref, -1))
			

/*
 * Macros for tracing vnode activity.
 */
#define	VNODE_PCACHE_INIT		1
#define	VNODE_PCACHE_REINIT		2
#define	VNODE_PCACHE_FREE		3
#define	VNODE_PCACHE_RECLAIM		4
#define	VNODE_PCACHE_PAGEOP		5
#define	VNODE_PCACHE_PAGEBAD		6
#define	VNODE_PCACHE_PAGEATTACH		7
#define	VNODE_PCACHE_PFIND		8
#define	VNODE_PCACHE_PINS_TRY		9
#define	VNODE_PCACHE_PINSERT		10
#define	VNODE_PCACHE_PREMOVE		11
#define	VNODE_PCACHE_PTOSS		12
#define	VNODE_PCACHE_FLUSHINVAL		13
#define	VNODE_PCACHE_INVALFREE		14
#define	VNODE_PCACHE_PAGESRELEASE	15
#define	VNODE_PCACHE_PAGERECYCLE	16
#define	VNODE_PCACHE_REPLINSERT		17
#define	VNODE_PCACHE_REPLFOUND		18
#define	VNODE_PCACHE_REPLINSERTED	19
#define	VNODE_PCACHE_NODEPFIND		20
#define	VNODE_PCACHE_NODEPFOUND		21
#define	VNODE_PCACHE_REPLSHOOT_START	22
#define	VNODE_PCACHE_REPLSHOOT_PAGE	23
#define	VNODE_PCACHE_REPLSHOOT_END	24
#define	VNODE_PAGEFREE			25
#define	VNODE_PAGEFREE_ANON		26
#define	VNODE_PCACHE_MIGR		27
#define	VNODE_MIGR_EPILOGUE		28
#define	VNODE_PG_ISMIGRATING		29
#define	VNODE_PCACHE_REPLACE		30

#ifdef	DEBUG
#define	VNODE_PCACHE_TRACE		1
#endif

#ifdef  VNODE_PCACHE_TRACE
#define KTRACE_ENTER(kt, v0, v1, v2, v3)                \
		ktrace_enter(kt,                        \
			(void *)(__psunsigned_t)v0,     \
			(void *)v1,                     \
			(void *)(__psunsigned_t)v2,     \
			(void *)(__psunsigned_t)v3,     \
			(void *)(__psunsigned_t)lbolt,	\
			(void *)__return_address, 	\
			(void *)0 , (void *) 0, \
			(void *)0 , (void *)0 , (void *) 0, (void *) 0, \
			(void *)0 , (void *)0 , (void *) 0, (void *) 0)
extern struct ktrace *vnode_ktrace;
#else
#define KTRACE_ENTER(kt, v0, v1, v2, v3)
#endif


/*
 * External function declaration.
 */

extern void vnode_pcache_init(struct vnode *);
extern void vnode_pcache_reinit(struct vnode *);
extern void vnode_pcache_free(struct vnode *);
extern void vnode_pcache_reclaim(struct vnode *);

extern struct pfdat *vnode_pfind(struct vnode *, pgno_t, int);
extern struct pfdat *vnode_pfind_nolock(struct vnode *, pgno_t, int);

extern void vnode_hold(struct vnode *);
extern int vnode_pagemigr(struct vnode *, struct pfdat *, struct pfdat *);

extern struct pfdat *vnode_pnext(struct vnode *, struct pfdat *);
extern struct pfdat *vnode_plookup(struct vnode *, pgno_t);

extern struct pfdat *vnode_pinsert_try(struct vnode *, struct pfdat *, pgno_t);
extern struct pfdat *vnode_page_attach(struct vnode *, struct pfdat *);

extern void vnode_pagebad(struct vnode *, struct pfdat *);
extern void vnode_pinsert(struct vnode *, struct pfdat *, pgno_t, unsigned);
extern void vnode_pinsert_nolock(struct vnode *, struct pfdat  *, pgno_t, unsigned, int);

extern void vnode_page_recycle(struct vnode *, struct pfdat *);


extern void vnode_premove(struct vnode *, struct pfdat  *);
extern void vnode_premove_nolock(struct vnode *, struct pfdat  *);

extern void vnode_tosspages(struct vnode *, off_t, off_t);
extern void vnode_flushinval_pages(struct vnode *, off_t, off_t);
extern int  vnode_flush_pages(struct vnode *, off_t, off_t, uint64_t);
extern void vnode_invalfree_pages(struct vnode *, off_t);
extern void vfs_flushinval_pages(struct vfs *);
extern void vnode_pagesrelease(struct vnode *, struct pfdat *, int, uint64_t);
extern void vnode_pages_sethole(struct vnode *, struct pfdat *, int );

extern struct pfdat *vnode_page_replace(struct pfdat *, struct pfdat *);


#endif /* __OS_VM_VNODE_PCACHE_H */
