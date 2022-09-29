/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkc_cache.c,v 65.6 1998/04/01 14:16:18 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <tkc.h>
#ifdef SGIMIPS
#include <dcedfs/xvfs_export.h>
#include <tkc_private.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc_cache.c,v 65.6 1998/04/01 14:16:18 gwehrman Exp $")

/* in TKC_SPECIAL_TOKENS, don't bother with dataHolds, since non-zero dataHolds
 * requires a non-zero reference count.  Generally, we can recycle a vcache
 * entry if the file isn't open or locked.  If it is, our token prevents
 * conflicts with external sites accessing the file via the FX.
 */
#define TKC_SPECIAL_TOKENS(vcp) ((vcp)->readOpens || (vcp)->writeOpens || (vcp)->execOpens || (vcp)->locks || (vcp)->dataHolds)

struct tkc_qqueue tkc_vhash[TKC_VHASHSIZE];	/* vnode hash table */
struct squeue tkc_vclruq;			/* Vnode LRU queue */
struct lock_data tkc_lrulock;			/* lock for LRUQ updates */

/* 
 * tkc_InitCache -- initialize statics for the tkc_cache module. 
 */
#ifdef SGIMIPS
void tkc_InitCache(void)
#else
void tkc_InitCache()
#endif
{
    lock_Init(&tkc_lrulock);
}

/* Get a free tkc_vcache from the LRU queue (tkc_lrulock
 * already writelocked).  Returned dude won't be in any
 * hash tables or such.  Will have held ref count, empty
 * tokenList queue.
 */
#ifdef SGIMIPS
static struct tkc_vcache *GetFreeVcacheEnt(void)
#else
static struct tkc_vcache *GetFreeVcacheEnt()
#endif /* SGIMIPS */
{
    register struct tkc_vcache *vcp;    
    register struct squeue *qp;
    long code;

  retry:
    for (qp = tkc_vclruq.prev; qp != &tkc_vclruq; qp = QPrev(qp)) {
	vcp = (tkc_vcache_t *) qp;
	lock_ObtainRead(&vcp->lock);
	lock_ObtainWrite(&tkc_rclock);
	/* don't have to worry about TKC_GDELETED being set, since
	 * this guy is still in the lru queue, and no one in the LRU
	 * queue has GDELETED set (GDELETED is set only after removing
	 * vcache entry from all queues except its tokens are still in
	 * the token ID hash table).
	 */
	if (!vcp->refCount&& (!TKC_SPECIAL_TOKENS(vcp))) {
	    vcp->refCount++;
	    lock_ReleaseWrite(&tkc_rclock);
	    lock_ReleaseRead(&vcp->lock);
	    /* next call can fail, if we lose the race condition
	     * in reobtaining the necessary locks to actually do
	     * the remove.  However, we should be sure to update the
	     * condition test above to ensure that the only case that
	     * causes unexpected failure is the loss of the race condition,
	     * otherwise we could get into an infinite loop on a hard
	     * condition preventing tkc_RemoveCache from succeeding.
	     */
	    lock_ReleaseWrite(&tkc_lrulock);
	    code = tkc_RecycleVcache(vcp, 0);
	    if (code) tkc_Release(vcp);
	    lock_ObtainWrite(&tkc_lrulock);
	    if (code) goto retry;	/* unexpected loss */
	    break;
	} else {
	    tkc_stats.busyvcache++;
	    lock_ReleaseWrite(&tkc_rclock);
	    lock_ReleaseRead(&vcp->lock);
	}
    }
    /* Note that vcp may be non-zero even if we didn't find a useful entry */
    if (qp == &tkc_vclruq) {
	/* try making one from memory */
	vcp = (struct tkc_vcache *) osi_Alloc(sizeof(struct tkc_vcache));
	bzero((caddr_t)vcp, sizeof(*vcp));
	lock_Init(&vcp->lock);
	vcp->refCount = 1;	/* return it held */
	QInit(&vcp->tokenList);

	/* all entries must be in LRU queue; LRU lock already held */
	QAdd(&tkc_vclruq, &vcp->lruq);
	tkc_stats.extras++;
    }
    vcp->tokenAddCounter = vcp->tokenScanValue = 0;
    return (vcp);
}


/* called with no locks held, and returns with no locks held.
 * returns held vcp structure, or null.  Search by vnode hash table
 * instead of lruq since you can find stuff in intermediate states
 * of recycling in the LRU queue, but not in the hash tables.
 */
struct tkc_vcache *tkc_FindByFID(struct afsFid *fidp)
{
    register struct tkc_vcache *vcp;
    register struct squeue *qp;
    register struct tkc_qqueue *hqp;
    int i;

    for(i=0; i<TKC_VHASHSIZE; i++) {
	hqp = &tkc_vhash[i];
	lock_ObtainRead(&hqp->lock);
	for (qp = hqp->chain.next; qp != &hqp->chain; qp = QNext(qp)) {
	    /* don't have to check TKC_DELETING here since that flag is
	     * only for references originating from the token ID hash table.
	     */
	    vcp = TKC_QTOV(qp);
	    if (FidCmp(&vcp->fid, fidp) == 0) {
		tkc_stats.hhits++;
		lock_ObtainWrite(&tkc_rclock);
		vcp->refCount++;
		lock_ReleaseWrite(&tkc_rclock);
		lock_ReleaseRead(&hqp->lock);	/* cleanup queue lock */
		/* don't bother moving around in LRU queue, this is
		 * done very rarely.
		 */
		return vcp;
	    }
	}
	lock_ReleaseRead(&hqp->lock);
    }
    tkc_stats.hmisses++;
    return (struct tkc_vcache *) 0;
}


/*
 * Return vnode's cached entry, if any.
 */
struct tkc_vcache *tkc_FindVcache(struct vnode *vp)
{
    register struct tkc_vcache *vcp;
    register struct squeue *qp;
    register struct tkc_qqueue *hqp = &tkc_vhash[TKC_VHASH(vp)];

    lock_ObtainRead(&hqp->lock);
    for (qp = hqp->chain.next; qp != &hqp->chain; qp = QNext(qp)) {
	/* don't have to check TKC_DELETING here since that flag is
	 * only for references originating from the token ID hash table.
	 */
	if ((vcp = TKC_QTOV(qp))->vp == vp) {
	    tkc_stats.hhits++;
	    lock_ObtainWrite(&tkc_rclock);
	    vcp->refCount++;
	    lock_ReleaseWrite(&tkc_rclock);
	    lock_ReleaseRead(&hqp->lock);	/* cleanup queue lock */
	    /*
	     * Move the vcache entry to the front of the LRU queue
	     */
	    lock_ObtainWrite(&tkc_lrulock);
	    QRemove(&vcp->lruq);
	    QAdd(&tkc_vclruq, &vcp->lruq);
	    lock_ReleaseWrite(&tkc_lrulock);
	    return vcp;
	}
    }
    tkc_stats.hmisses++;
    lock_ReleaseRead(&hqp->lock);
    return (struct tkc_vcache *) 0;
}


/*
 * Return the cached entry associated with the vnode; create one if necessary.
 * Returns the entry held.  Called with no locks held.
 */
#ifdef SGIMIPS
struct tkc_vcache *tkc_GetVcache(
    struct vnode *vp)
#else
struct tkc_vcache *tkc_GetVcache(vp)
    struct vnode *vp;
#endif
{
    register struct tkc_qqueue *hqp;
    register struct tkc_vcache *vcp;
    int ccode;

    vcp = tkc_FindVcache(vp);
    if (!vcp) {	
	tkc_stats.hentries++;
	hqp = &tkc_vhash[TKC_VHASH(vp)];
	lock_ObtainWrite(&tkc_lrulock);
	vcp = GetFreeVcacheEnt();	/* returned held */
	lock_ObtainWrite(&vcp->lock);	/* lock whole element */

	/*
	 * Initialize/Set vcache's fields (rest are zeroed already)
	 */
	lock_ObtainWrite(&tkc_rclock);	/* fix ref count */
	osi_assert(vcp->refCount > 0);
	vcp->gstates = 0;	/* turns off deleting */
	vcp->vstates = 0;	/* turns off deleting */
	lock_ReleaseWrite(&tkc_rclock);
	lock_ObtainWrite(&hqp->lock);	/* start hash in */
	QAdd(&hqp->chain, &vcp->vhash);
	lock_ReleaseWrite(&hqp->lock);	/* hashed in */
	vcp->vp = vp;
	OSI_VN_HOLD(vp);		/* vcp is holding it */
	OSI_VN_HOLD(vp);		/* I am holding it for a while */
	xvfs_ConvertDev(&vp);
#ifdef SGIMIPS
	VOPX_AFSFID(vp, &vcp->fid, 1, ccode);
#else
	ccode = VOPX_AFSFID(vp, &vcp->fid, 1);
#endif
	osi_assert (!ccode);
	OSI_VN_RELE(vp);		/* I'm done with it now. */
	QInit(&vcp->tokenList);

	lock_ReleaseWrite(&vcp->lock);	/* unlock element */
	lock_ReleaseWrite(&tkc_lrulock);/* done with LRU Q */
    }
    return (vcp);
}


/* Delete vnode's cache entry.  Called with no locks set
 * and with vcp->refCount held by the guy removing it.
 *
 * If decr is set, the ref count is decremented, and the caller
 * shouldn't use the pointer to vcp again (the vcache entry could
 * get re-used at any point thereafter).  Otherwise, the vcache entry
 * is left held (if the call fails) or is left held (if it succeeds),
 * and the caller should use tkc_Release to let go of the entry when done.
 *
 * We do assignments into vcp->[gv]states since no one else is using this
 * entry once we past our safety checks.
 */
#ifdef SGIMIPS
int tkc_RecycleVcache(
register struct tkc_vcache *vcp,
int decr)
#else
int tkc_RecycleVcache(vcp, decr)
register struct tkc_vcache *vcp;
int decr;
#endif
{
    int origRefCount;
    struct tkc_qqueue *hqp, *tqp;
    struct tkc_tokenList *tlp, *nlp;

    /* first lock all relevant structures */
    lock_ObtainWrite(&tkc_lrulock);
    lock_ObtainWrite(&vcp->lock);
    hqp = &tkc_vhash[TKC_VHASH(vcp->vp)];
    lock_ObtainWrite(&hqp->lock);
    lock_ObtainWrite(&tkc_rclock);
    if (vcp->refCount > 1 || TKC_SPECIAL_TOKENS(vcp)) {
	/* abort the deletion, this guy's referenced now */
	if (decr) vcp->refCount--;
	lock_ReleaseWrite(&tkc_rclock);
	lock_ReleaseWrite(&hqp->lock);
	lock_ReleaseWrite(&vcp->lock);
	lock_ReleaseWrite(&tkc_lrulock);
	return 1;
    }
    /* commit to deleting here, mark things so that the cache entry
     * can't be found by starting with the token ID hash table, and
     * then element from vnode hash table, LRU queue.
     */
    QRemove(&vcp->lruq);		/* Remove from LRU queue */
    if (vcp->vp) {
	OSI_VN_RELE(vcp->vp);
	vcp->vp = (struct vnode *) 0;
	QRemove(&vcp->vhash);		/* Remove from vnode's hash chain */
    }
    vcp->gstates = TKC_GDELETING;
    vcp->vstates = 0;
    vcp->heldTokens = 0;
    if (--vcp->refCount != 0) panic("recyclevcache refcount");
    vcp->readOpens = vcp->writeOpens = vcp->execOpens =
	vcp->dataHolds = vcp->locks = 0;
    lock_ReleaseWrite(&vcp->lock);
    lock_ReleaseWrite(&hqp->lock);
    lock_ReleaseWrite(&tkc_rclock);
    lock_ReleaseWrite(&tkc_lrulock);

    /* remove from token ID hash table.  It was too hard to remove this
     * guy above because we'd first have had to lock all of the tokenList
     * hash table buckets before checking refCount.  At this point,
     * the tokenList entries are still in their hash table, but no one
     * can bump the refCount on vcp since TKC_DELETING is set.  The vcache
     * element is out of all other tables and queues.  We'll now return
     * all the tokens and free up these tokenList elements.  Note that
     * returning the tokens requires holding absolutely no locks on these
     * elements.
     * We don't have to hold the vcache lock, since its refcount is 0,
     * and thus we're the only dude looking at it.
     */
    for(tlp = (struct tkc_tokenList *) QNext(&vcp->tokenList);
	tlp !=(struct tkc_tokenList *) &vcp->tokenList;
	tlp = nlp) {
	nlp = (struct tkc_tokenList *) QNext(&tlp->vchain);
	tqp = &tkc_thash[TKC_THASH(&tlp->token.tokenID)];
	lock_ObtainWrite(&tqp->lock);
	/* pull this entry out of its queue, if it is on the queue */
	if (tlp->states & TKC_LHELD)
	    QRemove(&tlp->thash);
	lock_ReleaseWrite(&tqp->lock);
	/* locks are all released, so return this token if it
	 * ever was granted and we still have some token types
	 * remaining.
	 */
	if (((tlp->states & (TKC_LHELD|TKC_LDELETED)) == TKC_LHELD)
	    && !AFS_hiszero(tlp->token.type)
	    && !tkm_IsGrantFree(tlp->token.tokenID)) {
	    TKM_RETTOKEN(&vcp->fid, (&tlp->token), TKM_FLAGS_PENDINGRESPONSE);
	}
	tkc_FreeSpace((char *) tlp);
    }
    QInit(&vcp->tokenList);

    /* set the reference count properly, before putting back in lru queue,
     * where others could find it.
     */
    vcp->gstates = 0;	/* turn off GDELETING */
    vcp->refCount = (decr? 0 : 1);

    /* finally, put this guy back into the lru queue */
    lock_ObtainWrite(&tkc_lrulock);
    QAdd(&tkc_vclruq, &vcp->lruq);
    lock_ReleaseWrite(&tkc_lrulock);
    return 0;
}

/*
 * tkc_flushvfsp -- flush tkc for a file system
 *
 * Called when a file system is about to be unmounted
 * with no locks held.
 */

#ifdef SGIMIPS
int tkc_flushvfsp (
    struct osi_vfs *vfsp)		/* virtual file system */
#else
int tkc_flushvfsp (vfsp)
    struct osi_vfs *vfsp;		/* virtual file system */
#endif
{
    int i;
    struct tkc_qqueue *hqp;
    struct tkc_vcache *vcp;
    struct tkc_vcache *tcp;
    struct vnode *vp;
    long code;
    int doit;
    struct squeue *qp, *nextqp;

    for (i = 0; i < TKC_VHASHSIZE; i++) {
	hqp = &tkc_vhash [i];
	lock_ObtainWrite (&hqp->lock);	/* lock hash bucket */
	for (qp = hqp->chain.next; qp != &hqp->chain; qp = nextqp) {
	    vcp = TKC_QTOV (qp);
	    tkc_Hold(vcp);
	    /* and compute and hold the "next" guy, if any, before
	     * deleting this entry.
	     */
	    nextqp = QNext (qp);
	    if (nextqp != &hqp->chain) {
		/* bump next guy's ref count, if he exists */
		tcp = TKC_QTOV(nextqp);
		tkc_Hold(tcp);
	    }
	    else tcp = (struct tkc_vcache *) 0;
	    lock_ReleaseWrite(&hqp->lock);
	    lock_ObtainWrite(&vcp->lock);	/* lock node */
	    vp = vcp->vp;
	    if (OSI_VP_TO_VFSP (vp) == vfsp) {
		lock_ObtainRead(&tkc_rclock);
		if (vcp->refCount > 1 || TKC_SPECIAL_TOKENS (vcp)) {
		    /* can't unmount now, probably should sleep
		     * if the problem is just the refCount.
		     */
		    lock_ReleaseWrite(&vcp->lock);
		    lock_ReleaseRead(&tkc_rclock);
		    tkc_Release(vcp);
		    if (tcp) tkc_Release(tcp);
		    return -1;
		}
		lock_ReleaseRead(&tkc_rclock);

		/* now release locks so we can call RecycleVcache
		 * safely.  Vcache must be held going in, but refCount
		 * will be cleared whether the call succeeds or loses.
		 */
		lock_ReleaseWrite(&vcp->lock);
		code = tkc_RecycleVcache(vcp, 1);
		if (code) {
		    if (tcp)
			tkc_Release(tcp);
		    return -2;
		}
	    }		/* in the right vfs */
	    else {	/* not in the right vfs */
		lock_ReleaseWrite(&vcp->lock);
		tkc_Release(vcp);
	    }	/* vfs check */
	    /* re-obtain the vnode hash table queue lock*/
	    lock_ObtainWrite(&hqp->lock);
	    /* here we should just have the vnode hash bucket lock.  At
	     * the top of the loop, we want to have the next guy in qp
	     * but not held.  Since hqp's lock is held, we don't have
	     * to worry about nextqp becoming invalid as we loop around.
	     *
	     * We call  QuickRelease here so we can do the hold again
	     * at the top of the loop.  Wheen we hold vcp at the top,
	     * we'll be doing a hold of what is now tcp.  Essentially,
	     * we're just calling QuickRelease so we can get to the
	     * top of the loop again with the right preconditions,
	     * specifically, that qp isn't held.  The cleanup processing
	     * will really happen when we release vcp in the next
	     * iteration.
	     */
	    if (tcp) tkc_QuickRelease(tcp);
	}	/* for loop over items in hash bucket */
	lock_ReleaseWrite (&hqp->lock);
    }	/* for loop over all hash buckets */
    return 0;
}

/* tkc_FlushEntry -- flush tokens from a cache entry if it is about to be
 * released.
 */

#ifdef SGIMIPS
int tkc_FlushEntry (
    register struct tkc_vcache *vcp)
#else
int tkc_FlushEntry (vcp)
    register struct tkc_vcache *vcp;
#endif
{
    lock_ObtainWrite(&tkc_rclock);
    vcp->gstates |= TKC_GDISCARD;
    lock_ReleaseWrite(&tkc_rclock);
    return 0;
}

void tkc_FlushVnode(struct vnode *avp)
{
    register struct tkc_vcache *tvc;

    tvc = tkc_FindVcache(avp);
    if (tvc) {
	/* if found, set the flag and release the entry.  If
	 * not found, we're happy, since we're not holding the vnode
	 * if there isn't even an entry.
	 */
	tkc_FlushEntry(tvc);
	tkc_Release(tvc);
    }
}
