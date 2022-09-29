/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkc_locks.c,v 65.4 1998/04/01 14:16:20 gwehrman Exp $";
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
 * $Log: tkc_locks.c,v $
 * Revision 65.4  1998/04/01 14:16:20  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for tkc_private.h.
 *
 * Revision 65.3  1998/03/23  16:27:24  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:33  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:02  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.739.1  1996/10/02  21:01:28  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:11  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved  */

#include <tkc.h>
#ifdef SGIMIPS
#include <tkc_private.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc_locks.c,v 65.4 1998/04/01 14:16:20 gwehrman Exp $")


/* get a lock token for the requested lock range.  Check to see
 * if the current process already has tokens for the required
 * lock, and if not, get some new tokens.
 */
long tkc_GetLocks(
  struct vnode *avp,
  long atokenType,
  tkc_byteRange_t *abyteRangep,
  int ablock,
  afs_recordLock_t *rblockerp)
{
    register struct tkc_vcache *vcp;
    register long code;

    vcp = tkc_GetVcache(avp);
    /* get the new lock token, blocking synchronously if requested
     * otherwise just trying and failing if conflicting locks
     * exist.
     * Note that this code doesn't check to see if it has conflicting
     * tokens within this machine, even though it does keep track
     * of which process holds a lock token.  The real vnode op's
     * lockctl operation handles local locking; we use the pid field
     * in the tokenList structure just to decide which lock tokens
     * to return, when a token is returned.
     */
    lock_ObtainWrite(&vcp->lock);
    code = tkc_GetToken(vcp, atokenType, abyteRangep, (ablock? 1 : 0), 
			rblockerp);
    /* note that tkc_GetToken, via tkc_AddToken, actually sets
     * the locks field, so we don't have to do it here.
     */
    if (code == 0) vcp->vstates |= TKC_PARTLOCK;
    lock_ReleaseWrite(&vcp->lock);
    tkc_Release(vcp);
    return code;
}

/* notify that we have all the locks we need, so anyone who wants lock tokens
 * is welcome to try to get us to revoke ours.
 */
void
tkc_FinishGetLocks(vp)
     struct vnode *vp;
{
    register struct tkc_vcache *vcp;
    register long code;

    vcp = tkc_GetVcache(vp);
    lock_ObtainWrite(&vcp->lock);
    if (vcp->vstates & TKC_PARTLOCK) {
	vcp->vstates &= ~TKC_PARTLOCK;
	if (vcp->vstates & TKC_PLWAITING) {
	    vcp->vstates &= ~TKC_PLWAITING;
	    osi_Wakeup(&vcp->vstates);
	}
    }
    lock_ReleaseWrite(&vcp->lock);
    tkc_Release(vcp);
}

/* return all locks held by the calling process, over the particular
 * byte range.
 */
long tkc_PutLocks(
  struct vnode *vp,
  tkc_byteRange_t *byteRangep)
{
    register struct tkc_vcache *vcp;
    register struct tkc_tokenList *tlp;
    register struct squeue *qp, *tqp;
    tkc_byteRange_t byteRanges;
    register long code = 0;
    long tcode;
    int doReturn;
    afs_token_t ttoken;
    afs_hyper_t lmin, lmax, rtMin, rtMax;
    int noChange;
    int newLocks;

    vcp = tkc_GetVcache(vp);
    lock_ObtainWrite(&vcp->lock);
  retry:
    newLocks = 0;
    for (qp = vcp->tokenList.next; qp != &vcp->tokenList; qp = tqp) {
	tqp = QNext(qp);
	tlp = (tkc_tokenList_t *) qp;
	/* we compute new version of vcp->locks incrementally, because
	 * any time we make a change to the lock state, we go back to
	 * the top (the label retry) and start again.  We exit only
	 * if we make a pass through w/o making any changes.
	 */
	if ((tlp->states & (TKC_LHELD|TKC_LLOCK|TKC_LDELETED))
	    == (TKC_LLOCK|TKC_LHELD)) newLocks = 1;
	if ((tlp->procid == (unsigned long) osi_ThreadUnique())
	    && (tlp->states & TKC_LLOCK)
	    && !(tlp->states & TKC_LDELETED)) {

	    AFS_hzero(lmin);
	    AFS_hset64(lmax,-1,-1);
	    rtMin = lmin;
	    rtMax = lmax;
	    noChange = 0;
	    /*
	     * Obtain the overlap of the byte ranges of the two token elements.
	     * lmin and lmax will be the range to the left of the hole being
	     * poked in the token.  rtMin and rtMax will be the part of the 
	     * token range left to the right of the hole poked in the token.
	     */
	    if (AFS_hcmp(byteRangep->beginRange, tlp->token.beginRange) <= 0) {
		/* there is, at best, stuff left at the right */
		if (AFS_hcmp(byteRangep->endRange,
			     tlp->token.beginRange) >= 0) {
		    if (AFS_hcmp(byteRangep->endRange,
				 tlp->token.endRange) < 0) {
			rtMax = tlp->token.endRange;
			rtMin = byteRangep->endRange;
			AFS_hincr(rtMin);
		    }
		    /* else we've eliminated the entire token, so we don't
		     * set rt* to anything.
		     */
		}
		else {
		    /* else the returned token is completely to the left of
		     * this token, and so we leave the entire token as
		     * the rt token.
		     */
		    noChange = 1;
		}
	    } else if ((AFS_hcmp(byteRangep->beginRange,
				 tlp->token.beginRange) > 0) && 
		       (AFS_hcmp(byteRangep->beginRange,
				 tlp->token.endRange) <= 0)) {
		/* 
		 * Left of unlock is properly internal to the token being
		 * considered, so that we know that there is at least some
		 * portion of the remainder to the left of the token being
		 * returned.  There may be a second piece to the right, 
		 * depending upon the endpoint of the returned token.
		 */
		lmin = tlp->token.beginRange;
		lmax = byteRangep->beginRange;
		AFS_hdecr(lmax);
		if (AFS_hcmp(byteRangep->endRange, tlp->token.endRange) < 0) {
		    rtMax = tlp->token.endRange;
		    rtMin = byteRangep->endRange;
		    AFS_hincr(rtMin);
		}
	    }
	    else {
		/* token being returned is completely to the right of the
		 * token being considered: don't change the token.
		 */
		noChange = 1;
	    }
	    if (!noChange) {
		/* we're going to have to drop the lock when exchanging
		 * tokens.  Now, the right way to do this would be
		 * to have reference counts on the tokenList elements,
		 * but I don't have the time to make a major change
		 * like that now.  So, we'll copy out all the required
		 * info, do our basic work, and then go back and restart
		 * from the top of the loop.
		 */
		ttoken = *((afs_token_t *) &tlp->token);
		doReturn = tlp->states & TKC_LHELD;
		tkc_DelToken(tlp);	/* tlp released when count->0 */
		code = 0;

		/* Intersection is not an identity, so we have to get
		 * little tokens spanning what we need to keep.
		 */
		if (AFS_hcmp64(lmax,-1,-1) != 0) {
		    byteRanges.beginRange = lmin;
		    byteRanges.endRange = lmax;
		    code = tkc_GetToken(vcp, AFS_hgetlo(ttoken.type),
					&byteRanges, 0, NULL);
		}
		if (code == 0 && AFS_hcmp64(rtMax,-1,-1) != 0) {
		    byteRanges.beginRange = rtMin;
		    byteRanges.endRange = rtMax;
		    code = tkc_GetToken(vcp, AFS_hgetlo(ttoken.type),
					&byteRanges, 0, NULL);
		}
		/* Intersection is an identity, or the token on
		 * subranges have been obtained anyway, it's ok
		 * to discard the old tokens.
		 * Return this token even if code != 0, since we've
		 * already deleted the token.
		 */
		if (doReturn && !tkm_IsGrantFree(ttoken.tokenID)) {
		    lock_ReleaseWrite(&vcp->lock);
		    /* return containing token.  Don't stomp on earlier
		     * error code if this fails.
		     */
		    tcode = TKM_RETTOKEN(&vcp->fid, &ttoken,
					 TKM_FLAGS_PENDINGRESPONSE);
		    if (code == 0) code = tcode;
		    lock_ObtainWrite(&vcp->lock);
		}
		if (code) break;

		/* now, we must try from the top, since we lost our position
		 * in the list when we broke out of this list.
		 */
		goto retry;
	    }	/* if !noChange */
	}  /* if applies to this process and this is an undeleted lock */
    }	/* for loop over all tokens */
    if (code == 0) {
	/* if code is 0, then newLocks is set to 1 iff there are any remaining
	 * lock tokens for any process ID.  Copy this into the locks field.
	 * Note that if code != 0, then locks shouldn't have changed, and we
	 * don't have to worry about updating it.
	 */
	vcp->locks = newLocks;
    }
    lock_ReleaseWrite(&vcp->lock);
    tkc_Release(vcp);
    return (code);
}
