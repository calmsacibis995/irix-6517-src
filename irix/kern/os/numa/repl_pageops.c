
#ifdef NUMA_REPLICATION

/*****************************************************************************
 * repl_pageops.c
 *	List of operations done on a replicated page.
 *	Primarily this involves maintaining replicated pages on a chain
 *	off the page cache hash. This is a separate chain than the
 *	hash chain. 
 *	pfdats will get set with a new bit "P_REPLICA" if they form
 *	part of the replicated chain.
 *	Each replicated chain is rooted at the pfdat that's part of 
 *	the hash chain, and acts as the head of replicated chain.
 *
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
 ****************************************************************************/

#ident "$Revision: 1.10 $"


#include <sys/types.h>
#include <sys/immu.h>	
#include <sys/cmn_err.h>
#include <sys/pfdat.h>
#include <sys/vnode.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>		/* offtoc */
#include <sys/kmem.h>
#include <ksys/rmap.h>
#include <sys/systm.h>			/* splhi */
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>
#include <sys/nodepda.h>
#include <sys/page.h>
#include <sys/sysmp.h>			/* numainfo 	*/
#include <ksys/pcache.h>
#include <ksys/vnode_pcache.h>
#include "numa_stats.h"
#include "replattr.h"
#include "repl_pageops.h"
#include "repldebug.h"

/* 
 * P_NOTSETFLAGS are the set of flags that should never be 
 * set in a page that is being replicated. 
 */
#define	P_NOTSETFLAGS	(P_DIRTY|P_DQUEUE|P_ANON|P_SQUEUE|P_WAIT|P_BAD|P_DUMP)

/*
 * page_replica_insert:
 *
 * Try and insert page "new_pfd" on the hash chain where src_pfd
 * stays. 
 * 
 * While trying to insert, check if there is already a page that's on
 * the same node as the new_pfd. Existance indicates we were racing
 * with some other process trying to create a replica on the same node.
 *
 * So, instead of inserting new_pfd, grab an extra reference count
 * on the matching page, and return that page.
 *
 * If not, insert the new_pfd at the tail of replica list
 * and return. As part of insertion, update the relavent fields of
 * new_pfd to reflect the state of the page.
 * 
 * In either case, this returns the page that was successfully inserted.
 *
 * pinsert can be modified to do this work. But pinsert is left as it
 * is in order to catch other cases of duplicate insertion. We are
 * having a separate routine till testing is done.
 *
 */

pfd_t *
page_replica_insert(pfd_t *src_pfd, pfd_t *pfd_new)
{
	register struct pfdat	*pfd_hash; 
	register nasid_t	nnode = pfdattonasid(pfd_new);
	int			s;
	vnode_t			*vp = src_pfd->pf_vp;
	pgno_t			pgno = src_pfd->pf_pageno;
	void			*pcache_token;

	/*
	 * These assignments are safe since page is currently anonymous.
	 */
	ASSERT(!(pfd_new->pf_flags & (P_HASH|P_SWAP|P_ANON|P_QUEUE|P_SQUEUE)));

	pcache_token = pcache_getspace(&vp->v_pcache, 1);

	if (pcache_token) {
		int locktoken;
		/*
		 * If we were able to alloate the space, grow the
		 * pagecache also. If it's already grown to proper size,
		 * pcache_resize will free the allocated space.
                 */
                VNODE_PCACHE_LOCK(vp, locktoken);
                pcache_token = pcache_resize(&vp->v_pcache, pcache_token);
                VNODE_PCACHE_UNLOCK(vp, locktoken);
                pcache_resize_epilogue(pcache_token);
        }

	/* 
	 * first scan through the page cache checking if a page exists
	 * already on the same node as "pfd_new".
	 */
	VNODE_PCACHE_LOCK(vp, s);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_REPLINSERT, vp, src_pfd, pfd_new);

	pfd_hash = pcache_find(&vp->v_pcache, pgno);

	while (pfd_hash) {
		ASSERT((pfd_hash->pf_flags & P_BAD) == 0);
		ASSERT(pfd_hash->pf_vp == vp); 

		if (pfdattonasid(pfd_hash) == nnode) {
			ASSERT(pfd_hash->pf_flags & P_HASH);
			ASSERT(!(pfd_hash->pf_flags & P_SQUEUE));
			ASSERT(pfd_hash->pf_pageno == pgno);

			pfd_hash = vnode_page_attach(vp, pfd_hash);
			/*
			 * pfd_hash points to a proper pfdat if 
			 * vnode_page_attach was successful in attaching
			 * the page. Otherwise (if we are racing with 
			 * a thread trying to reallocate the page), 
			 * we would fail, and we have a null pointer.
			 */
			if (pfd_hash) {
				KTRACE_ENTER(vnode_ktrace, 
					VNODE_PCACHE_REPLFOUND, vp, 
					src_pfd, pfd_hash);

				VNODE_PCACHE_UNLOCK(vp, s);
				pagefree(pfd_new);
				return pfd_hash;
			} else {
				/*
				 * Null pfd_hash indicates the page which 
				 * was in the page table got reallocated
				 * for a different purpose.
				 * So, we break, and decide to insert the pfd
				 * that got passed to us. 
				 */
				break;
			}

		} else {
			pfd_hash = pcache_next(&vp->v_pcache,  pfd_hash);
		}
	}

	/*
	 *
	 * No page on the desired node found. Insert the page
	 * that got passed.
	 *
	 * Since the page is still anonymous, it's ok to do these..
	 * without holding the pfdat lock.
	 */
	pfd_new->pf_vp = vp;
	pfd_new->pf_pageno = src_pfd->pf_pageno;

	pfd_new->pf_flags &= ~P_DONE;
	pfd_new->pf_flags |= (P_HASH|P_CACHESTALE|P_REPLICA);

	pcache_insert(&vp->v_pcache,  pfd_new);

	ASSERT(vp->v_pgcnt >= 0);
	vp->v_pgcnt++;
	replattr_lookup(vp)->replpgcnt++;
	NUMA_STAT_ADD(pfdattocnode(pfd_new), repl_pagecount, 1);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_REPLINSERTED, vp, src_pfd, pfd_new);

	VNODE_PCACHE_UNLOCK(vp, s);

	return pfd_new;
}

/*
 * page_replica_remove
 *	Do what ever needed to unassociate pfd from being replicated.
 *	It's already out of hash chain.
 *	Just decrement the page count.
 *	
 *	We could layer this better after page cache code is checked in.
 */
void
page_replica_remove(pfd_t *pfd)
{
	ASSERT(pfd_replicated(pfd));

	(replattr_lookup(pfd->pf_vp))->replpgcnt--;
	pfd->pf_flags &= ~P_REPLICA;
	NUMA_STAT_ADD(pfdattocnode(pfd), repl_pagecount, -1);
}


/*
 * Find page on a specific node, by looking on hash chain
 *	tag	-> page's tag
 *	pgno	-> logical page of object
 * returns:
 *	0	-> can't find it
 *	pfd	-> ptr to pfdat entry
 *
 * This routine uses _plookup to get the first page on the hash list
 * that matches the "tag" and "pgno". It then uses vnode_pnext to 
 * get next page that has the same attributes as the first one.
 */
pfd_t *
pfind_node(
	struct vnode	*vp,
	pgno_t		pgno,
	cnodeid_t	page_node,
	int		acquire)
{
	pfd_t		*pfd;
	nasid_t		nnode;
	int		locktoken;

	nnode = COMPACT_TO_NASID_NODEID(page_node);

	VNODE_PCACHE_LOCK(vp, locktoken);

	KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_NODEPFIND, vp, pgno, nnode);

	pfd = pcache_find(VNODE_PCACHE(vp), pgno);

	while (pfd) {

		/* 
  		 * Get the first non-bad page on the right node. 
		 */
		if (((pfd->pf_flags & P_BAD) == 0) &&
		    (pfdattonasid(pfd) == nnode)) {
			ASSERT(pfd->pf_flags & P_HASH);
			ASSERT(!(pfd->pf_flags & P_SQUEUE));

			if (acquire & VM_ATTACH) {
				pfd = vnode_page_attach(vp, pfd);
			}
			break;
		}
		pfd = pcache_next(VNODE_PCACHE(vp), pfd);
	}

	if(pfd)
		KTRACE_ENTER(vnode_ktrace, VNODE_PCACHE_NODEPFOUND, vp, pfd, nnode);

	VNODE_PCACHE_UNLOCK(vp, locktoken);

	return(pfd);
}


#endif /* NUMA_REPLICATION */
