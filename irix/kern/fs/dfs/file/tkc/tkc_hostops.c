/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkc_hostops.c,v 65.4 1998/04/01 14:16:19 gwehrman Exp $";
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
 * $Log: tkc_hostops.c,v $
 * Revision 65.4  1998/04/01 14:16:19  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for tkc_private.h.  Added forward prototypes for
 * 	static functions.
 *
 * Revision 65.3  1998/03/23  16:27:14  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:30  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:02  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.537.1  1996/10/02  21:01:24  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:09  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <tkc.h>
#ifdef SGIMIPS
#include <tkc_private.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc_hostops.c,v 65.4 1998/04/01 14:16:19 gwehrman Exp $")

#ifdef SGIMIPS
static long tkchs_Hold(
    struct hs_host *hp);
static long tkchs_Rele(
    struct hs_host *hp);
static long tkchs_Lock(
    struct hs_host *hp,
    long type,
    long timeout);
static long tkchs_Unlock(
    struct hs_host *hp,
    int type);
static long tkchs_RevokeToken(
    struct hs_host *hp,
    int revokeLen,
    struct afsRevokeDesc *revokeList);
static long tkchs_AsyncGrant(
    struct hs_host *hp,
    char *tokdescP,
    unsigned long reqnum);
static long ProcessGrant(
  struct hs_tokenDescription *tokdesc,
  long areqid);
tkc_RevokeTokenByTID(
  struct afsRevokeDesc *revokep,
  afs_hyper_t *tokenType,
  int *foundTID);
#else
static long tkchs_Hold(), tkchs_Rele(), tkchs_Lock(), tkchs_Unlock();
static long tkchs_RevokeToken(), tkchs_AsyncGrant();
static long ProcessGrant();
#endif

struct hostops tkchs_hops = {
    tkchs_Hold,
    tkchs_Rele,
    tkchs_Lock,
    tkchs_Unlock,
    tkchs_RevokeToken,
    tkchs_AsyncGrant
};


/* this function is probably never called */
static long tkchs_Lock(hp, type, timeout)
    struct hs_host *hp;
    long type, timeout;
{	
    if (type == HS_LOCK_READ)
	lock_ObtainRead(&(hp->lock));
    else if (type == HS_LOCK_WRITE)
	lock_ObtainWrite(&(hp->lock));
    else 
	return -1;
    return 0;
}


/* this function is probably never called */
static long tkchs_Unlock(hp,type)
    struct hs_host *hp;
    int type;
{
    if (type == HS_LOCK_READ)
	lock_ReleaseRead(&(hp->lock));
    else if (type == HS_LOCK_WRITE)
	lock_ReleaseWrite(&(hp->lock));
    else 
	return -1;
    return 0;
}


/* the host object hold function, just bumps reference count under lock */
static long tkchs_Hold(hp)
    struct hs_host *hp;
{
    lock_ObtainWrite(&hp->lock);    
    hp->refCount++;
    lock_ReleaseWrite(&hp->lock);
    return 0;
}


/* release an object by dropping its reference count under a lock */
static long tkchs_Rele(hp)
    struct hs_host *hp;
{
    lock_ObtainWrite(&hp->lock);    
    if (hp->refCount > 0) 
	hp->refCount--;
    lock_ReleaseWrite(&hp->lock);
    return 0;
}


/* Revoke tokens in <revokeList>. Set status of each revoke in <revokeList>. */
static long tkchs_RevokeToken(hp, revokeLen, revokeList)
    struct hs_host *hp;
    int revokeLen;
    struct afsRevokeDesc *revokeList;
{
    struct afsRevokeDesc *startP = revokeList;
    register long bitmap, i, j, code;
    long rcode = 0;
    afs_hyper_t index;

    icl_Trace1(tkc_iclSetp, TKC_TRACE_REVOKETOKEN, ICL_TYPE_LONG, revokeLen);
    rcode = 0;
    for (i = 0; i < revokeLen; i++, startP++) {
	AFS_hzero(startP->errorIDs);
	bitmap = 1;
	for (j = 0; j < 32; j++, (bitmap <<= 1)) {
	    if (!(AFS_hgetlo(startP->type) & bitmap))
		continue;	
	    AFS_hset64(index, 0, bitmap);
	    /* Try to revoke each of the token type
	     * associated with each token id.
	     */
	    if ((code = tkc_RevokeToken(startP, &index))) {
		/* failed to revoke the token, set corresponding error bit */
		AFS_HOP(startP->errorIDs, |, index);
		rcode = HS_ERR_PARTIAL;
	    }
	}
    }
    return rcode;
}


/* called here if an asynchronous grant is received due to a
 * queued token request finally completing.
 */
static long tkchs_AsyncGrant(hp, tokdescP, reqnum)
    struct hs_host *hp;
    char *tokdescP;
    unsigned long reqnum;
{
    return (ProcessGrant((struct hs_tokenDescription *)tokdescP, reqnum));
}


/*
 * Process an async grant of a token <tok> from the TKM. This async grant is
 * accepted only in response to a queued request earlier for the same token,
 * ie there exists an entry in tcache, marked queued. On obtaining the token,
 * the tcache entry is marked held instead of queued. It's updated to reflect
 * the token id and its expiration. If there is no pre-existing cache entry
 * we refuse ccepting the token. It's not possible in our scheme that we'll
 * get response from the tkm (for a queued request) before we have created
 * an entry for the queued request in the tcache.
 */
static long ProcessGrant(tokdesc, areqid)
  struct hs_tokenDescription *tokdesc;
  long areqid;
{
    register long i;
    struct tkc_qqueue *hqp;
    register struct squeue *tq;
    struct squeue *nq;
    struct tkc_vcache *nvc;
    int thisVC;		/* vcache contains granted token */
    register struct tkc_vcache *tvc;
    register struct tkc_tokenList *tlp;
    long rcode;

    rcode = HS_ERR_NOENTRY;	/* default error if we don't find reqID */
    /* note that there's no hash table for the request ID (which
     * is not the same as the token ID), so we linearly scan the entire
     * set of tokens looking for this guy.
     */
    for(i=0; i<TKC_VHASHSIZE; i++) {
	hqp = &tkc_vhash[i];
	lock_ObtainRead(&hqp->lock);
	tq = hqp->chain.next;
	if (tq != &hqp->chain){
	    tvc = TKC_QTOV(tq);
	    tkc_Hold(tvc);
	}
	lock_ReleaseRead(&hqp->lock);
	for(; tq != &hqp->chain; tq=nq) {
	    /* at this point in the loop, tvc is held already for us.
	     * We have to jump through all these hoops because we
	     * can't call tkc_Release with *any* locks held, which makes
	     * the end of this loop pretty hairy.
	     */
	    tvc = TKC_QTOV(tq);	/* already held */
	    lock_ObtainWrite(&tvc->lock);
	    /* now look through all of the entries in the vcache entry,
	     * checking if we can find this reqID field.
	     */
	    thisVC = 0;	/* not known if this vcache has our token */
	    for(tlp=(struct tkc_tokenList *) tvc->tokenList.next;
		tlp != (struct tkc_tokenList *) &tvc->tokenList;
		tlp = (struct tkc_tokenList *) tlp->vchain.next) {
		/* tokens we don't care about any more are marked
		 * as deleted, too.  We don't accept async grants
		 * for these guys (no one would return the granted
		 * tokens anyway).
		 */
		if (!(tlp->states & TKC_LDELETED)
		    && tlp->reqID == areqid) {
		    /* below locks token ID bucket lock, but that's OK, as
		     * we only have the vcache lock held at this point.
		     */

		    /* 
		     * we should not be getting any OPEN_DELETE here
		     * since they are now handled by the zlc manager.
		     */
		    osi_assert((AFS_hgetlo(tlp->token.type) & TKN_OPEN_DELETE)
			       == 0);
		    tkc_FinishToken(tvc, tlp, &tokdesc->token);
		    rcode = 0;
		    thisVC = 1;
		    break;
		}
	    }
	    lock_ReleaseWrite(&tvc->lock);

	    /* advance to the next guy in the hash table.  Use appropriate
	     * lock, and hold the next entry so that the invariant at the
	     * top of the loop is satisfied (tvc will be held after assignment
	     * of tvc from tq from nq).
	     */
	    lock_ObtainRead(&hqp->lock);
	    nq = QNext(tq);
	    if (nq != &hqp->chain) {
		nvc = TKC_QTOV(nq);
		tkc_Hold(nvc);
		/* nvc doesn't appear to be used again, but really appears
		 * again at the top of the loop, when we derive tvc from
		 * tq, which is really nq.
		 */
	    }
	    lock_ReleaseRead(&hqp->lock);

	    /* here's a tricky one: sometimes we have the last reference
	     * to a vcache entry.  Typically in this case, tkc_Release would
	     * return all tokens, but it turns out that you can't return
	     * a token to the tkm while it is being async-granted.
	     * Unfortunately, this is our only chance to return this
	     * token, so we've modified tkc_Release to tell us if
	     * the token would have been returned (had tkm been able
	     * to take it), and in this case, we don't accept the
	     * token grant.
	     */
	    if (tkc_Release(tvc) == 0 && thisVC)
		rcode = HS_ERR_JUSTKIDDING;
	}
	if (rcode != HS_ERR_NOENTRY) break;	/* done, can quit early */
    }
    return rcode;
}	

/* compute all tokens held, must be called with a readlocked
 * vcache entry, at least.
 */
long tkc_OrAllTokens(alp)
register struct squeue *alp; {
    register struct tkc_tokenList *tlp;
    long sum=0;

    for(tlp = (struct tkc_tokenList *) QNext(alp);
	(struct squeue *) tlp != alp;
	tlp = (struct tkc_tokenList *) QNext(&tlp->vchain)) {
	/* if held and not deleted, token is available */
	if ((tlp->states & (TKC_LDELETED | TKC_LHELD)) == TKC_LHELD)
	    sum |= AFS_hgetlo(tlp->token.type);
    }
    return sum;
}


/* called with no locks, and returns 0 if we got the token back,
 * otherwise we return an error code.
 *
 * Very tricky algorithm:
 * First, note that the guy getting the token merges in the token
 * under the vcp lock.  We call tkm_EndRacingCall and then FinishToken
 * under this lock.
 *
 * Now, if we only call tkc_RevokeTokenByTID, we could end up running
 * before the token is merged as described above, and we'd lose a revoke.
 * Thus, afterwards, we lock the vcp and register the token race,
 * which will ensure that if we run before the returned token is merged,
 * then we'll remove the just-revoked rights from the returned set before
 * merging.
 *
 * Now, it is also possible that both of these cases missed: it could
 * be that we do the original search before the token is hashed in, and
 * do the racing revoke processing after the token is merged in.  In this
 * case, no one has removed the revoked token's rights.  Thus, we try
 * one more time to remove the rights by TID.  If the racing code failed to
 * block the token being granted, it must be because the token grantee
 * has already made it past the token hashin/merge point.  Thus, at this
 * point, the tkc_RevokeTokenByTID should work properly.
 *
 * Why not just start at the racing revoke code, and skip the
 * first RevokeTokenByTID?  History and speed.
 * The call to tkc_FindByFID is *slow*, since historically, this package
 * didn't expect to have to search by file ID.  Function tkc_RevokeTokenByTID,
 * on the other hand, is fast, and typically does its job.  And if
 * we *found* the token by ID, then we know that we're done, since this
 * means the grantee is already past the merge/hashin point.
 *
 * Thus, this function typically works by only calling the first
 * RevokeTokenByTID, and is thus typically fast.
 */
tkc_RevokeToken(revokep, tokenType)
    struct afsRevokeDesc *revokep;
    afs_hyper_t *tokenType;
{
    int foundIt;
    register long code;
    register struct tkc_vcache *vcp;

    code = tkc_RevokeTokenByTID(revokep, tokenType, &foundIt);
    if (foundIt) return code;

    /* otherwise, we have to do the slow path: racing revoke */
    vcp = tkc_FindByFID(&revokep->fid);
    if (vcp) {
	lock_ObtainWrite(&vcp->lock);
	tkm_RegisterTokenRace(&tkc_race, &revokep->fid, tokenType);
	lock_ReleaseWrite(&vcp->lock);

	/* need to try again to remove the token */
	code = tkc_RevokeTokenByTID(revokep, tokenType, &foundIt);
	tkc_Release(vcp);
	return code;
    }
    else return 0;
}

tkc_RevokeTokenByTID(revokep, tokenType, foundTID)
  struct afsRevokeDesc *revokep;
  afs_hyper_t *tokenType;
  int *foundTID;
{
    register struct tkc_tokenList *tlp;
    register struct tkc_vcache *vcp;
    register struct squeue *qp;
    register struct tkc_qqueue *hqp = &tkc_thash[TKC_THASH((&revokep->tokenID))];
    register long code = 0;
    long ttype;

    ttype = AFS_hgetlo(*tokenType);
    lock_ObtainRead(&hqp->lock);
    for (qp = hqp->chain.next; qp != &hqp->chain; qp = QNext(qp)) {
	if (AFS_hsame(revokep->tokenID, (tlp = TKC_QTOT(qp))->token.tokenID)) {
	    vcp = tlp->backp;
	    lock_ObtainWrite(&tkc_rclock);
	    /* if in the process of deleting this guy, we can't
	     * bump the reference count; instead we treat this vcache
	     * as already deleted.
	     */
	    if (vcp->gstates & TKC_GDELETING) {
		lock_ReleaseWrite(&tkc_rclock);
		continue;
	    }
	    vcp->refCount++;
	    icl_Trace2(tkc_iclSetp, TKC_TRACE_REVOKESTART,
		       ICL_TYPE_POINTER, (long) vcp,
		       ICL_TYPE_LONG, AFS_hgetlo(*tokenType));
	    lock_ReleaseWrite(&tkc_rclock);
	    lock_ReleaseRead(&hqp->lock);
	    /* Call the associated revoke procedure with rock arguments.
	     * At this point, vcache is held and write-locked.
	     * CAUTION --
	     *   Some of these procedures can drop and re-obtain the lock.
	     */
	    lock_ObtainWrite(&vcp->lock);
	    if (ttype & AFS_hgetlo(tlp->token.type)) {
		if (ttype & (TKN_DATA_READ | TKN_DATA_WRITE))
		    code = tkc_RevokeDataToken(vcp, revokep, ttype);
		else if (ttype & (TKN_LOCK_READ | TKN_LOCK_WRITE))
		    code = tkc_RevokeLockToken(vcp, revokep, ttype, tlp);
		else if (ttype & (TKN_STATUS_READ | TKN_STATUS_WRITE))
		    code = tkc_RevokeStatusToken(vcp, revokep, ttype);
		else if (ttype & (TKN_OPEN_READ | TKN_OPEN_WRITE
				  | TKN_OPEN_SHARED | TKN_OPEN_EXCLUSIVE))
		    code = tkc_RevokeOpenToken(vcp, revokep, ttype);
	    }
	    else {
		/* if we get here, we don't think we have this type of token
		 * with this token ID, so we just return it.
		 */
		code = 0;
	    }
	    icl_Trace1(tkc_iclSetp, TKC_TRACE_REVOKEEND, ICL_TYPE_LONG, code);
	    if (!code) {
		/* Remove revoked token bits */
		AFS_HOP(tlp->token.type, &~, *tokenType);
		vcp->heldTokens = tkc_OrAllTokens(&vcp->tokenList);
		if (AFS_hiszero(tlp->token.type))
		    tkc_DelToken(tlp);
	    }
	    lock_ReleaseWrite(&vcp->lock);

	    /* drop ref count again */
	    tkc_Release(vcp);
	    /* ok to return, since at most one instance of a token ID
	     * in table.  Also note: since we've dropped locks w/o ref
	     * count, qp may be invalid after tkc_Release call above.
	     */
	    *foundTID = 1;
	    return (code);
	}	/* if token ID matches */
    }		/* iterate over all tokens in set */

    /* if we make it here, we haven't found the token ID anywhere.
     * This could be a race however, so record that we missed finding
     * it so our caller can ensure that racing grants are handled
     * properly.
     */
    lock_ReleaseRead(&hqp->lock);	
    *foundTID = 0;
    return 0;
}
