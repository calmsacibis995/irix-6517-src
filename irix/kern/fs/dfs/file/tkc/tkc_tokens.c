/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkc_tokens.c,v 65.5 1998/04/01 14:16:21 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: tkc_tokens.c,v $
 * Revision 65.5  1998/04/01 14:16:21  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for tkm_private.h.  Changed function definitions to
 * 	ansi style.
 *
 * Revision 65.4  1998/03/23  16:27:19  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/12/16 17:05:43  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.2  1997/11/06  19:58:32  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:03  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.542.1  1996/10/02  21:01:34  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:12  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <tkc.h>
#ifdef SGIMIPS
#include <tkc_private.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc_tokens.c,v 65.5 1998/04/01 14:16:21 gwehrman Exp $")

struct tkc_qqueue tkc_thash[TKC_THASHSIZE];


/* Function to add tokenList entry *alp to the tokenID hash
 * table; called after the token ID becomes valid.  Note that
 * unlike in the cache manager case, here, we're guaranteed
 * that this function will be called by tkc_FinishToken before
 * we see any revokes for this token ID.
 */
#ifdef SGIMIPS
void tkc_HashIn(
  register struct tkc_tokenList *alp)
#else
tkc_HashIn(alp)
  register struct tkc_tokenList *alp;
#endif
{
    register struct tkc_qqueue *hqp;

    hqp = &tkc_thash[TKC_THASH((&alp->token.tokenID))];
    lock_ObtainWrite(&hqp->lock);	
    QAdd(&hqp->chain, &alp->thash);
    lock_ReleaseWrite(&hqp->lock);
}

/* Add a token to the list.  Called with vcache entry write-locked.
 */
#ifdef SGIMIPS
struct tkc_tokenList *tkc_AddToken(
  register struct tkc_vcache *vcp,
  afs_token_t *tokenp)
#else
struct tkc_tokenList *tkc_AddToken(vcp, tokenp)
  register struct tkc_vcache *vcp;
  afs_token_t *tokenp;
#endif
{
    register struct tkc_tokenList *tlp;

    tlp = (struct tkc_tokenList *) tkc_AllocSpace();
    bzero((caddr_t)tlp, sizeof(*tlp));
    tlp->token = *tokenp;
    QAdd(&vcp->tokenList, &tlp->vchain);
    vcp->tokenAddCounter++;	/* so we know that there are more tokens
				 * in the list.
				 */
    /* set tokens as held if we already have them, not before */
    tlp->backp = vcp;
    tlp->procid = (unsigned long) osi_ThreadUnique();	/* For file locking */
    tkc_stats.tentries++;
    tlp->states = TKC_LQUEUED;
    /* record if this is a requested lock token.  Do it based on
     * *requested* type, in case we support optimistic grants someday.
     */
    if (AFS_hgetlo(tokenp->type) & (TKN_LOCK_READ|TKN_LOCK_WRITE)) {
	vcp->locks = 1;	/* mark that we've got active lock tokens */
	tlp->states |= TKC_LLOCK;
    }
    return tlp;
}

/* Look through the list of tokens hanging off of a vcache, and
 * remove tokens that are covered by other tokens.
 * Don't do this for lock tokens.  
 *
 * This function ignores byte ranges.  We are relying on the assumption that
 * no one asks the TKC for a non-trivial byte range, except for lock tokens.
 *
 * Set the tokenScanValue to tokenAddCounter when done; this means
 * that as of the "time" when this vcache had an add counter
 * of a particular value, we know that there were no duplicate tokens.
 * This saves us the trouble of calling this function unless new tokens
 * were added.
 *
 * No locks should be held when calling this function.  The vcache
 * entry should be held.
 */
#ifdef SGIMIPS
tkc_PruneDuplicateTokens(
  register struct tkc_vcache *vcp)
#else
tkc_PruneDuplicateTokens(vcp)
  register struct tkc_vcache *vcp;
#endif
{
    register struct tkc_tokenList *tlp;
    long held;	/* tokens already seen */
    int didAny;	/* did any token returns */
    afs_token_t ttoken;	/* temp for returning token */

    /* look at all tokens, starting with most recently added tokens,
     * and looking back in time, and if we have a token that is covered
     * by the preceding tokens, copy out enough info to return it,
     * and then return it w/o holding any locks.
     * Don't do this processing for lock tokens.  We would have to check
     * for held locks, which is more trouble than it is worth.
     */
    lock_ObtainWrite(&vcp->lock);
    do {
	held = 0;	/* none seen yet */
	didAny = 0;	/* none done yet */
	for(tlp = (struct tkc_tokenList *) QNext(&vcp->tokenList);
	    tlp !=(struct tkc_tokenList *) &vcp->tokenList;
	    tlp = (struct tkc_tokenList *) QNext(&tlp->vchain)) {

	    /* if this token already deleted, or if it is a lock
	     * token, or it hasn't been granted yet, then skip it.
	     */
	    if (tlp->states & (TKC_LDELETED|TKC_LLOCK)
		|| !(tlp->states & TKC_LHELD)) continue;

	    /* check to see if we can return this token */
	    if ((~held & AFS_hgetlo(tlp->token.type)) == 0) {
		/* no bits in token are missing from "held" bits */
		/* save token before unlock */
		ttoken = *((afs_token_t *) &tlp->token);
		tkc_DelToken(tlp);	/* not done until vcache released */
		lock_ReleaseWrite(&vcp->lock);
		TKM_RETTOKEN(&vcp->fid, &ttoken, TKM_FLAGS_PENDINGRESPONSE);
		lock_ObtainWrite(&vcp->lock);
		didAny = 1;
		/* can't proceed, since tlp may have been destroyed
		 * while vcache was unlocked.  So, retry from the top
		 */
		break;
	    }

	    /* otherwise, we're keeping this token, so add it to our
	     * set of known token types.
	     */
	    held |= AFS_hgetlo(tlp->token.type);
	}
    } while (didAny);
    /* we made it through without blocking to return any tokens, so
     * it is safe to set the tokenScanValue field.
     */
    vcp->tokenScanValue = vcp->tokenAddCounter;
    lock_ReleaseWrite(&vcp->lock);
    return 0;
}

/*
 * Finish granting of a token when it comes in; vcp must be write locked
 * when this function is called.
 */
#ifdef SGIMIPS
tkc_FinishToken(
  struct tkc_vcache *vcp,
  struct tkc_tokenList *lp,
  afs_token_t *tokenp)
#else
tkc_FinishToken(vcp, lp, tokenp)
  struct tkc_vcache *vcp;
  struct tkc_tokenList *lp;
  afs_token_t *tokenp;
#endif
{
    if (lp->states & TKC_LHELD) panic("tkc twice cooked token");
    *((afs_token_t *) &lp->token) = *tokenp;
    lp->states |= TKC_LHELD;
    vcp->heldTokens |= AFS_hgetlo(lp->token.type);
    tkc_HashIn(lp);
    if (lp->states & TKC_LQUEUED)
	osi_Wakeup((char *) vcp);
    lp->states &= ~TKC_LQUEUED;
    return 0;
}

/* Called when we decide to refuse the async grant of a token.
 * This function is called when we decide that we don't want a
 * token that was async granted after all.
 */
#ifdef SGIMIP
tkc_RefuseToken(
  struct tkc_vcache *vcp,
  struct tkc_tokenList *lp)
#else
tkc_RefuseToken(vcp, lp)
  struct tkc_vcache *vcp;
  struct tkc_tokenList *lp;
#endif
{
    if (lp->states & TKC_LHELD) panic("tkc twice cooked token");
    if (lp->states & TKC_LQUEUED)
	osi_Wakeup((char *) vcp);
    lp->states &= ~TKC_LQUEUED;
    return 0;
}

/* Delete given tokenList objects associated with vcache and
 * which have LDELETED flag set.  Called with one reference by the
 * caller (only one allowable) and with no locks held.
 *
 * Because we're called with one reference, the count won't drop
 * further, and thus no one *else* can try to remove any tokenList
 * entries while we hold the rclock.  Thus, the tokenList entry
 * won't go bad even when we don't have any locks held.
 *
 * If we delete any entries, we also recompute avc->locks, which
 * we must do in a single pass while holding the vcp lock.
 */
#ifdef SGIMIPS
tkc_DoDelToken(
  struct tkc_vcache *avc)
#else
tkc_DoDelToken(avc)
  struct tkc_vcache *avc;
#endif
{    
    register struct tkc_tokenList *tlp;
    struct tkc_qqueue *hqp;
    struct squeue *tsqp, *nsqp;
    int didAny;
    long code;
    int sawLock;

    lock_ObtainWrite(&avc->lock);
    /* clear flag first, so that later adds reset it */
    avc->vstates &= ~TKC_DELTOKENS;
    /* now remove all entries with the TKC_LDELETED flag set */
    code = 0;	/* return code */
    sawLock = 0;	/* didn't see any lock tokens yet */
    for(tsqp = QNext(&avc->tokenList); tsqp != &avc->tokenList;
	tsqp = nsqp) {
	tlp = (struct tkc_tokenList *) tsqp;
	nsqp = QNext(tsqp);
	if (tlp->states & TKC_LDELETED) {
	    /* delete this one, after setting locks in correct order */
	    hqp = &tkc_thash[TKC_THASH((&tlp->token.tokenID))];
	    lock_ObtainWrite(&hqp->lock);
	    lock_ObtainWrite(&tkc_rclock);
	    /* now, we've relocked things, but refcount could be
	     * high.  If it is, we're done now, and guy who finally
	     * drops count again will finish up.
	     */
	    if (avc->refCount > 1) {
		/* reset so that deleted tokenList entries get cleaned
		 * up in next pass through tkc_Release.
		 */
		avc->vstates |= TKC_DELTOKENS;
		lock_ReleaseWrite(&tkc_rclock);
		lock_ReleaseWrite(&hqp->lock);
		code = 1;	/* return indication that we're not done */
		break;
	    }
	    if (tlp->states & TKC_LHELD)
		QRemove(&tlp->thash);
	    if (tlp->states & TKC_LLOCK)
		sawLock = 1;
	    QRemove(&tlp->vchain);
	    lock_ReleaseWrite(&tkc_rclock);
	    lock_ReleaseWrite(&hqp->lock);
	    tkc_FreeSpace((char *) tlp);
	    tkc_stats.tentries--;
	}
    }
    if (sawLock) {
	/* if we removed any locks, we should recompute the "have locks"
	 * field.  Otherwise, it shouldn't have changed.
	 */
	sawLock = 0;
	for(tsqp = QNext(&avc->tokenList); tsqp != &avc->tokenList;
	    tsqp = nsqp) {
	    tlp = (struct tkc_tokenList *) tsqp;
	    nsqp = QNext(tsqp);
	    if (tlp->states & TKC_LLOCK) {
		sawLock = 1;
		break;
	    }
	}
	avc->locks = sawLock;
    }
    lock_ReleaseWrite(&avc->lock);
    return code;
}


/* called with write-locked vcache entry, mark an entry to be deleted */
#ifdef SGIMIPS
tkc_DelToken(
  register struct tkc_tokenList *tlp)
#else
tkc_DelToken(tlp)
  register struct tkc_tokenList *tlp;
#endif
{    
    tlp->states |= TKC_LDELETED;
    tlp->backp->vstates |= TKC_DELTOKENS;
    return 0;
}


/* this function takes the specified types of tokens and returns them.
 * The return vcache lock is held on entry and exit.
 *
 * The function returns true if it blocked (returned any tokens) and
 * false otherwise, and returns as soon as any tokens are returned,
 * even if more are available to be returned, since it may not be
 * correct to return them any more.
 */
#ifdef SGIMIPS
tkc_ReturnSpecificTokens(
  register struct tkc_vcache *vcp,
  register u_long tokenType)
#else
tkc_ReturnSpecificTokens(vcp, tokenType)
  register struct tkc_vcache *vcp;
  register u_long tokenType;
#endif
{
    int doReturn;
    afs_token_t ttoken;
    register struct tkc_tokenList *tlp;
    register struct squeue *qp, *nqp;
    register long code;

    for (qp = vcp->tokenList.next; qp != &vcp->tokenList; qp = nqp) {
	nqp = QNext(qp);
	tlp = (tkc_tokenList_t *)qp;
	/* have to release lock over return token, since return token is
	 * going to try to grant pending async grants, and this may
	 * involve trying to revoke already-granted tokens from other
	 * hosts, including this one.
	 */

	/* don't process tokens that aren't yet granted, or that have
	 * already been returned.
	 */
	if ((tlp->states & (TKC_LHELD|TKC_LDELETED)) != TKC_LHELD) continue;
	/* if we don't have this type of token, skip this guy */
	if (!(AFS_hgetlo(tlp->token.type) & tokenType)) continue;

	/* otherwise, save the token we'll be returning */
	ttoken = tlp->token;
	/* set the type we're returning */
	AFS_hset64(ttoken.type, 0, tokenType);

	/* cross this guy out from our held list */
	AFS_HOP32(tlp->token.type, &~, tokenType);
	if (AFS_hiszero(tlp->token.type))
	    tkc_DelToken(tlp);
	vcp->heldTokens = tkc_OrAllTokens(&vcp->tokenList);
	if (!tkm_IsGrantFree(ttoken.tokenID)) {
	    lock_ReleaseWrite(&vcp->lock);
	    if (code = TKM_RETTOKEN(&vcp->fid, (&ttoken), TKM_FLAGS_PENDINGRESPONSE)) {
		tkc_stats.rettokenF++;
		printf("TKM_RETTOKEN failed!! = %d\n", code);
	    }
	    lock_ObtainWrite(&vcp->lock);
	    return 1;
	}
    }

    /* no tokens left to be returned */
    return 0;
}


/* Return true if we have the appropriate tokens over the appropriate
 * byte range.  Note that vcp->heldTokens has only granted tokens set
 * in it; the bits appear when an async grant is handled.  On the other
 * hand, tlp->token.type has the bits as soon as the (lock) request is
 * made, but these bits are not valid until TKC_LHELD goes on.
 *
 * This function assumes that its caller has at least read-locked
 * the vcache entry.  It is used for open, data and status tokens,
 * but not for any byte-range tokens. (which for the glue, are only
 * lock tokens, since the glue never asks for byte range data tokens).
 */
int tkc_HaveTokens(
  struct tkc_vcache *vcp,
  u_long tokensNeeded,
  tkc_byteRange_t *byteRangep)
{
    tokensNeeded &= ~(vcp->heldTokens);
    return (tokensNeeded ? 0 : 1);
}
