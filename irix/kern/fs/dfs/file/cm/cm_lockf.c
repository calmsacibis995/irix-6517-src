/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_lockf.c,v 65.6 1998/03/23 16:24:21 gwehrman Exp $";
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
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */


#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/sysincludes.h>		/* Standard vendor system headers */
#ifdef AFS_SUNOS5_ENV
#include <sys/flock.h>			/* l_xxx, LOCKD_LAST_REF */
#endif
#include <cm.h>				/* Cm-based standard headers */
#include <cm_server.h>
#include <cm_async.h>
#include <dcedfs/com_locks.h>		/* basic lock stuff */
#include <dcedfs/tkm_errs.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_lockf.c,v 65.6 1998/03/23 16:24:21 gwehrman Exp $")

#ifdef SGIMIPS64
#define CM_MAXEND 0x7ffffffffffffffe
#else  /* SGIMIPS64 */
#define CM_MAXEND 0x7fffffff
#endif /* SGIMIPS64 */


/*
 * Convert a System V flock structure into the corresponding cm_lockf structure
 */
#ifdef SGIMIPS
static long SysToCM(
    struct flock *flp,
    struct cm_lockf *lckp)
#else  /* SGIMIPS */
static long SysToCM(flp, lckp)
    struct flock *flp;
    struct cm_lockf *lckp;
#endif /* SGIMIPS */
{
    if (flp->l_type == F_RDLCK)
	lckp->type = CM_READLOCK;
    else if (flp->l_type == F_WRLCK)
	lckp->type = CM_WRITELOCK;
    else if (flp->l_type == F_UNLCK)
	lckp->type = CM_UNLOCK;
    else /* invalid type */
         return EINVAL;

    lckp->startPos = flp->l_start;

    if (flp->l_len == CM_MAXEND)
        lckp->endPos = CM_MAXEND;
    else  {
        /* make sure lckp->endPos would not exceed CM_MAXEND */
        lckp->endPos = lckp->startPos + flp->l_len - 1;
	if (lckp->startPos > lckp->endPos)
	   return EINVAL;
    }
    lckp->pid = flp->l_pid;

#if defined(AFS_AIX31_ENV) || defined(AFS_SUNOS5_ENV) || defined(SGIMIPS)
    lckp->sysid = flp->l_sysid;
#else
    lckp->sysid = 0;
#endif
    return 0;
}


/*
 * Convert a cm_lockf structure into the corresponding Sys V flock structure
 */
#ifdef SGIMIPS
static void CMToSys(
    struct cm_lockf *lckp,
    struct flock *flp)
#else  /* SGIMIPS */
static void CMToSys(lckp, flp)
    struct cm_lockf *lckp;
    struct flock *flp;
#endif /* SGIMIPS */
{
    if (lckp->type == CM_READLOCK)
	flp->l_type = F_RDLCK;
    else if (lckp->type == CM_WRITELOCK)
	flp->l_type = F_WRLCK;
    else if (lckp->type == CM_UNLOCK)
	flp->l_type = F_UNLCK;
    flp->l_whence = 0;				/* set */
    flp->l_start = lckp->startPos;
    if (lckp->endPos == CM_MAXEND)
        flp->l_len = 0;
    else
        flp->l_len = lckp->endPos - lckp->startPos + 1;
#ifdef SGIMIPS
    flp->l_pid = (pid_t) lckp->pid;
#else  /* SGIMIPS */
    flp->l_pid = lckp->pid;
#endif /* SGIMIPS */

#if defined(AFS_AIX31_ENV) || defined(AFS_SUNOS5_ENV) || defined(SGIMIPS)
    flp->l_sysid = lckp->sysid;
#endif
}


/*
 * Try to release the lock lckp.  When done, we resweep the tokens required
 * for the remaining locks, and send back those requested that are no longer
 * required.
 *
 * This call is invoked from cm_lockctl when a lock is released.
 * The vnode must be write-locked to execute this call.
 */
#ifdef SGIMIPS
static int cm_ReleaseLockF(
    struct cm_scache *scp,
    struct cm_lockf *lckp,
    struct cm_rrequest *reqp)
#else  /* SGIMIPS */
static int cm_ReleaseLockF(scp, lckp, reqp)
    struct cm_scache *scp;
    struct cm_lockf *lckp;
    struct cm_rrequest *reqp;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS 
    register int code;
#else  /* SGIMIPS */
    register long code;
#endif /* SGIMIPS */
    struct flock tlock;
    struct squeue *jmarker;
#ifdef SGIMIPS
    afs_hyper_t end;
#else  /* SGIMIPS */
    int end;
#endif /* SGIMIPS */
    struct cm_tokenList ttokenl; /* temp for return; includes file ID descr */
    register struct cm_tokenList *ttp, *ntp;
    struct cm_server *tsp;

    CMToSys(lckp, &tlock);
    end = lckp->endPos;		/* last byte in range */
    /* tlock.l_pid = osi_GetPid();  ---this is already set */

    icl_Trace4(cm_iclSetp, CM_TRACE_RELEASELOCKF, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, lckp->type,
	       ICL_TYPE_LONG, lckp->startPos, ICL_TYPE_LONG, lckp->endPos);

    /*
     * First, remove lock from our internal queue
     */
    if (code = vnl_adjust(&tlock, &end, &scp->lockList, &scp->lockList,
			  &jmarker))
	return code;
    /*
     * Find anyone not marked and who's desired by the file server, and return
     * those lock tokens.
     */
    /* null server probably means R/O fileset */
    tsp = (struct cm_server *) 0;	/* last server we gave tokens */
    for (ttp = (struct cm_tokenList *) scp->tokenList.next;
	ttp != (struct cm_tokenList *) &scp->tokenList; ttp = ntp) {

	/* default next pointer, other branches below may recompute */
	ntp = (struct cm_tokenList *) ttp->q.next;

        if (ttp->flags & CM_TOKENLIST_DELETED) /* this one already deleted */
	    continue;

	if (ttp->flags & CM_TOKENLIST_RETURNL) {
	    /*
	     * This token may be no longer used, and has been revoked (and the
	     * revoke refused) by the file server.  Return it now.
	     */
	    ttokenl = *ttp;
	    AFS_hzero(ttokenl.token.type);
	    ttokenl.token.expirationTime = 0x7fffffff;
	    ttp->flags &= ~CM_TOKENLIST_RETURNL;
	    /* Now, hold token across the following,so we don't lose our place,
	     * which we can do in TryLockRevoke or further below.
	     */
	    cm_HoldToken(ttp);
	    if (AFS_hgetlo(ttp->token.type) & TKN_LOCK_READ) {
		if (cm_TryLockRevoke(scp, &ttokenl.token.tokenID,
				     TKN_LOCK_READ, 0, 0, 0, 0,
				     ttp->serverp))
		    ttp->flags |= CM_TOKENLIST_RETURNL;
		else
		    AFS_HOP32(ttokenl.token.type, |, TKN_LOCK_READ);
	    }
	    if (AFS_hgetlo(ttp->token.type) & TKN_LOCK_WRITE) {
		if (cm_TryLockRevoke(scp, &ttokenl.token.tokenID,
				     TKN_LOCK_WRITE, 0, 0, 0, 0,
				     ttp->serverp))
		    ttp->flags |= CM_TOKENLIST_RETURNL;
		else
		    AFS_HOP32(ttokenl.token.type, |, TKN_LOCK_WRITE);
	    }
	    /* tricky issue: even if we're not returning any tokens, we
	     * may have released a lock such that a conflicting token
	     * sitting at the server can be now granted (using slice and
	     * dice).  Since we're still using *some* part of this token,
	     * and the server has the only slice and dice code, we return
	     * empty rights for the token ID, which will cause the pending
	     * tokens to be retried.
	     */
	    /*
	     * Remove it from our list of usable tokens
	     */
	    cm_RemoveToken(scp, ttp, &ttokenl.token.type);
	    lock_ReleaseWrite(&scp->llock);
	    /* if we're flushing tokens from more than one server,
	     * flush tokens when we switch servers.
	     */
	    if (tsp && tsp != ttp->serverp)
		cm_FlushQueuedServerTokens(tsp);
	    tsp = ttp->serverp;		/* new server getting tokens */
	    cm_QueueAToken(tsp, &ttokenl, 1);	/* handles null tsp */
	    lock_ObtainWrite(&scp->llock);
	    ntp = (struct cm_tokenList *) ttp->q.next;
	    cm_ReleToken(ttp);			/* drop ref count now */
	}
    }	/* for loop over all tokens */
    if (tsp) {
       /*
	* If we've queued any tokens, we want to flush them, so that the caller
	* on the other side gets the tokens back immediately. This is necessary
	* for someone to get a lock on another machine as fast as possible
	* after the cache manager releases the tokens desired by the other
	* cache manager.  Drop lock to avoid deadlock when returning tokens.
	* Only do this for non-read-only tokens, of course.
	*/
	lock_ReleaseWrite(&scp->llock);
	cm_FlushQueuedServerTokens(tsp);
	lock_ObtainWrite(&scp->llock);
    }
    return 0;
}


/*
 * Try to obtain the lock lckp, making sure that we have the right types of
 * tokenstok in the token cache to cover the lock. This call is invoked from
 * the cm_lockctvnode operation when a lock is requested.
 *
 * if aflags & 1, then we want to wait.
 * if aflags & 2, we just want aoutLockp set to the blocker, if any
 * if neither is set, then we just fail if we can't get the lock.
 */
#ifdef SGIMIPS
static long cm_SetLockF(
    struct cm_scache *scp,
    struct cm_lockf *lckp,
    struct cm_lockf *aoutLockp,
    struct cm_rrequest *reqp,
    int aflags)
#else  /* SGIMIPS */
static long cm_SetLockF(scp, lckp, aoutLockp, reqp, aflags)
    struct cm_scache *scp;
    struct cm_lockf *lckp;
    struct cm_lockf *aoutLockp;
    struct cm_rrequest *reqp;
    int aflags;
#endif /* SGIMIPS */
{
    register long code;
    struct flock tflock;
    struct record_lock *rlockp, *blockerp;
    struct squeue *markerp;
    afs_token_t ttoken;
    afs_recordLock_t afsBlocker;
#ifdef SGIMIPS
    long prsysid, prpid;
    int prtype;
    afs_hyper_t prend, prstart, end;
#else  /* SGIMIPS */
    long prsysid, prtype, prend, prstart, prpid, end;
#endif /* SGIMIPS */
    /*
     * First, make sure we have the right tokens
     */

    icl_Trace4(cm_iclSetp, CM_TRACE_SETLOCKF, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, lckp->type, ICL_TYPE_LONG, lckp->startPos,
	       ICL_TYPE_LONG, lckp->endPos);

    for (;;) {

        AFS_hset64(ttoken.type, 0, (lckp->type == CM_READLOCK ?
				    TKN_LOCK_READ : TKN_LOCK_WRITE));
#ifdef SGIMIPS
	ttoken.beginRange = lckp->startPos;
	ttoken.endRange = lckp->endPos;
#else  /* SGIMIPS */
	AFS_hset64(ttoken.beginRange, 0, lckp->startPos);
	AFS_hset64(ttoken.endRange, 0, lckp->endPos);
#endif /* SGIMIPS */
	bzero((caddr_t)&afsBlocker, sizeof(afs_recordLock_t));

	if (aflags & 1) /* F_SETLKW */ {
	    /* willing to wait for the token */
	    if (code = cm_GetAsyncToken(scp, &ttoken, reqp))
	        goto checkprob;

	} else {
	    if (code = cm_GetTokensRange(scp, &ttoken, reqp, 0, &afsBlocker,
					 WRITE_LOCK)) {
	        if (code == TKM_ERROR_HARDCONFLICT) {
		    icl_Trace0(cm_iclSetp, CM_TRACE_SETLOCKFNOTOKEN);
		    prtype = afsBlocker.l_type;
#ifdef SGIMIPS
		    prstart = afsBlocker.l_start_pos;
		    prend = afsBlocker.l_end_pos;
#else  /* SGIMIPS */
		    prstart = AFS_hgetlo(afsBlocker.l_start_pos);
		    prend = AFS_hgetlo(afsBlocker.l_end_pos);
#endif /* SGIMIPS */
		    prpid = afsBlocker.l_pid;
		    prsysid = afsBlocker.l_sysid;
		    code = EAGAIN;
		}
		goto checkprob;
	    }
	}
	/*
	 * Otherwise, we've successfully gotten the tokens, and should next
	 * check to see if we have anything preventing the lock from being
	 * granted locally (any clients on *this* machine conflicting).
	 */
	CMToSys(lckp, &tflock);		/* make an flock from the req */
	end = lckp->endPos;		/* last byte in range */
	code = vnl_blocked(&tflock, end, &scp->lockList, &blockerp,  &markerp);
	icl_Trace1(cm_iclSetp, CM_TRACE_SETLOCKFLOCAL, ICL_TYPE_LONG, code);
	if (code == 0) {
	    /*
	     * Got the lock, we should do a queue add, if we're trying to set a
	     * lock, of course.  If we're trying to test the lock, being here
	     * means that the lock isn't set, and we could get it if we wanted.
	     */
	    if (aflags & 2) {		/* just testing the lock */
		/*
		 * Copy incoming lock data into outgoing, then
		 * overwrite the type field, to comply with POSIX.
		 */
		*aoutLockp = *lckp;
		aoutLockp->type = CM_UNLOCK;
	    } else {			/* we really want the lock. */
		code = vnl_adjust(&tflock, &end, &scp->lockList,
				  &scp->lockList, &markerp);
		if (code == 0 && markerp) {
		    if (code = vnl_alloc(&tflock, end, &rlockp))
			return code;		/* can't allocate lock */
		    rlockp->un.blocking = 0;
		    QAdd(((struct squeue *)markerp),((struct squeue *)rlockp));
		}
	    }
	} else {
	    /*
	     * If we're here, then we failed to get the lock locally.  We're
	     * going to either wait for it or return failure, explaining what
	     * went wrong if anyone's curious (aflags & 2).  When we're done,
	     * we'll have dropped locks, so we'll have to retry the whole
	     * operation in case we lost the required lock tokens while waiting
	     * for the local locks.
	     */
	    if (aflags & 1) {	/* we want to wait ie., F_SETLKW */
		/*
		 * Note that next call drops write lock on scp->llock
		 */
		code = vnl_sleep (&tflock, end, blockerp, &scp->llock);
		if (code == 0) {
		    /*
		     * Try again now that the lock is available locally
		     */
		    continue;
		}
		/*
		 * Only time we want out here is due to EINTR, otherwise treat
		 * it as another "blocked due to lock" problem.
		 */
		if (code != EINTR)
		    code = EDEADLK;
	    } else {  /* F_SETLK or F_GETLK */
		/*
		 * Tell caller that we failed to get the lock
		 * Return blocking information also.
		 */
		prtype = blockerp->data.l_type;
		prstart = blockerp->data.l_start;
		prend = blockerp->data_end;
		prpid = blockerp->data.l_pid;
		prsysid = blockerp->data.l_sysid;
		code = EAGAIN;
	    }
	}

	/*
	 * Come here to decide what to do on a failure; we could either return
	 * a description of what went wrong, retry, or quietly give up.
	 */
checkprob:
	/*
	 * Code may come from either vnl_blocked or cm_GetTokensRange
	 */
	if (code == EAGAIN) {  /* cmd must be either F_GETLK or F_SETLK */
	    /*
	     * Just supposed to fail, set aoutLockp if aflags & 2
	     */
	    if (aflags & 2) {/* F_GETLK */
	      /*
	       * We failed to lock becase we couldn't get a covering token
	       * so report the token's range as the problem. This may be a
	       * little bigger than the real problem, but there *is* a real
	       * problem somewhere within it.  Hopefully this is good enough.
	       * We could have also made it here because the lock is
	       * held locally by another process. This is a GETLCK request
	       * so we change the return code to success.
	       */
	      aoutLockp->type = prtype;
	      aoutLockp->startPos = prstart;
	      aoutLockp->endPos = prend;
	      aoutLockp->pid = prpid;
	      aoutLockp->sysid = prsysid;
	      code = 0;
	    }

	} else { /* Errors other than EAGAIN */
	   /* EMPTY */
	   /*
	    * Mysteriously failed to get appropriate lock tokens, i.e.
	    * something went wrong aside from someone else having things locked
	    * Alternative, code may be 0, and things may just have worked.
	    * Perhaps not all that mysterious, i.e. EINTR comes here.
	    */
	}
	return code;

    }  /* for (;;) */
}

/*
 * Check to see if we're allowed to revoke a particular lock token.
 * return 0 if we're allowed to, otherwise non-zero, meaning that
 * the token is in use.  TKM may try to call us again with a "good"
 * offer, which it may use to poke a hole in one of our tokens.
 *
 * This function is called from the token revocation server to determine
 * if a lock token can be revoked.
 *
 * If colAp or colBp is non-null, we've been offered other tokens to
 * try to use instead of the token we're losing.  We return in *flagsp
 * AFS_REVOKE_COL_[AB]_VALID if we determine that we need either colAp's
 * or colBp's token in order to give up the token we're losing.
 *
 * Serverp tells us which server granted token being revoked.  We
 * interpret slice&dice offers as also from this server.
 */
#ifdef SGIMIPS
cm_TryLockRevoke(
  struct cm_scache *scp,
  afs_hyper_t *tokenID,
  long tokenType,
  afs_recordLock_t *rlockerp,
  afs_token_t *colAp,
  afs_token_t *colBp,
  long *flagsp,
  struct cm_server *serverp)
#else  /* SGIMIPS */
cm_TryLockRevoke(scp, tokenID, tokenType, rlockerp,
		 colAp, colBp, flagsp, serverp)
  struct cm_scache *scp;
  afs_hyper_t *tokenID;
  long tokenType;
  afs_recordLock_t *rlockerp;
  afs_token_t *colAp, *colBp;
  long *flagsp;
  struct cm_server *serverp;
#endif /* SGIMIPS */
{
    register struct record_lock *tlp;
    register struct cm_tokenList *ttp;
    struct cm_tokenList *returnTokenp;
    afs_hyper_t ttype;
    long lockType;
    long newFlags, temp;
    int thereIsHope;
    afs_hyper_t leftEdge, rightEdge;
    int usedA, usedB;

    icl_Trace4(cm_iclSetp, CM_TRACE_REVOKELOCKF, ICL_TYPE_POINTER, scp,
               ICL_TYPE_LONG, tokenType,
               ICL_TYPE_POINTER, colAp, ICL_TYPE_POINTER, colBp);
    /*
     * Look for the token whose ID was passed in for this file.
     */
    returnTokenp = (struct cm_tokenList *) 0;
    for (ttp = (struct cm_tokenList *) scp->tokenList.next;
	ttp != (struct cm_tokenList *) &scp->tokenList;
	ttp = (struct cm_tokenList *) ttp->q.next) {
	if (AFS_hsame(ttp->token.tokenID, *tokenID) &&
	    (AFS_hgetlo(ttp->token.type) & tokenType)) {
	    returnTokenp = ttp;
	    break;
	}
    }
    /* if we didn't find the token with the rights being revoked, we can
     * return the token w/o worrying.  We don't really have to do this as
     * an explicit check, but it is a little safer this way, since if the
     * CM is in an inconsistent state with fewer tokens than required to cover
     * all locks, code below won't return any tokens.
     */
    if (!returnTokenp) {
        icl_Trace1(cm_iclSetp, CM_TRACE_REVOKENOLOCKF, ICL_TYPE_POINTER, scp);
        return 0;
    }

    /* now, look at each lock, and for each lock, look down the *sorted* list
     * of tokens (sorted by left edge), making sure the lock is completely
     * covered by the held tokens, minus the token being revoked (or at least
     * missing the types being revoked) and possibly using in addition any
     * colA or colB tokens offered up in return by the token manager.
     */
    newFlags = 0;	/* which of colA and colB we end up using */
    for (tlp = (struct record_lock *) QNext(&scp->lockList);
	 tlp != (struct record_lock *) &scp->lockList;
	 tlp = (struct record_lock *) QNext(&tlp->links)) {

	/* keep track of what's left of the token tlp; as we advance over
	 * tokens, we'll move leftEdge over to the right.  If it ever gets to
	 * be > than rightEdge, then we've covered this lock, and can go on
	 * to the next.  In this loop, since we're trying to revoke tokenID,
	 * we don't use that token; instead we use whichever of colAp and colBp
	 * we've been offered, if we need to.  This makes the checking a little
	 * more difficult.
	 */
#ifdef SGIMIPS
	leftEdge =  tlp->data.l_start;
#else  /* SGIMIPS */
	AFS_hset64(leftEdge, 0,tlp->data.l_start);
#endif /* SGIMIPS */
	/*
	 * in the internal representation of the lock package, data.l_len is
	 * really the *end* position, not the length!
	 */
#ifdef SGIMIPS 
	rightEdge = tlp->data.l_len;
#else  /* SGIMIPS */
	AFS_hset64(rightEdge, 0,tlp->data.l_len);
#endif /* SGIMIPS */
	icl_Trace4(cm_iclSetp, CM_TRACE_TRYLOCKREV_2,
		   ICL_TYPE_POINTER, tlp,
		   ICL_TYPE_LONG, tlp->data.l_type,
		   ICL_TYPE_LONG, tlp->data.l_start,
		   ICL_TYPE_LONG, tlp->data.l_len);
	/* determine token type useful for covering this lock */
	if (tlp->data.l_type == F_RDLCK)
	    lockType = TKN_LOCK_READ;
	else
	    lockType = TKN_LOCK_WRITE;
	/* first token to look at */
	ttp = (struct cm_tokenList *) scp->tokenList.next;
	thereIsHope = 1;	/* make sure we go around once */
	usedA= 0; usedB= 0;	/* reset temp colA and colB usage results*/
	while (thereIsHope) {
	    /* see if we're done */
	    if (AFS_hcmp(leftEdge, rightEdge) > 0)
	        break;

	    /* first try to use the token we're looking at.  We do this before
	     * looking at the column A and B choices to minimize the chances
	     * that we'll use them, since using col[AB] causes token
	     * fragmentation. */

	    thereIsHope = 0;
	    if (ttp != ((struct cm_tokenList *) &scp->tokenList) &&
		(AFS_hcmp(ttp->token.beginRange, leftEdge) <= 0)) {
		temp = AFS_hgetlo(ttp->token.type);
		/* if this is the token being revoked, we shouldn't use
		 * the rights being revoked, since if we accept the revocation,
		 * we won't have these rights any more.
		 */
		if (AFS_hsame(ttp->token.tokenID, *tokenID))
		    temp &= ~tokenType;
		if (temp & lockType) {
		    /* this token may move leftEdge up */
		    if (AFS_hcmp(ttp->token.endRange, leftEdge) >= 0) {
			leftEdge = ttp->token.endRange;
			if (AFS_hcmp(leftEdge, osi_hMaxint64) < 0)
			    AFS_hincr(leftEdge);
			thereIsHope = 1;
			if (AFS_hcmp(leftEdge, rightEdge) > 0)
			    break;
		    }
		}
	    }
	    if (!usedA && colAp) {
		if (AFS_hcmp(colAp->beginRange, leftEdge) <= 0 &&
		    AFS_hcmp(colAp->endRange, leftEdge) >= 0 &&
		    (AFS_hgetlo(colAp->type) & lockType)) {
		    leftEdge = colAp->endRange;
		    if (AFS_hcmp(leftEdge, osi_hMaxint64) < 0)
			AFS_hincr(leftEdge);
		    usedA = 1;
		    newFlags |= AFS_REVOKE_COL_A_VALID;
		    thereIsHope = 1;
		    continue;	/* don't advance token pointer */
		}
	    }
	    if (!usedB && colBp) {
		if (AFS_hcmp(colBp->beginRange, leftEdge) <= 0 &&
		    AFS_hcmp(colBp->endRange, leftEdge) >= 0 &&
		    (AFS_hgetlo(colBp->type) & lockType)) {
		    leftEdge = colBp->endRange;
		    if (AFS_hcmp(leftEdge, osi_hMaxint64) < 0)
			AFS_hincr(leftEdge);
		    usedB = 1;
		    newFlags |= AFS_REVOKE_COL_B_VALID;
		    thereIsHope = 1;
		    continue;	/* don't advance token pointer */
		}
	    }
	    /*
	     * leftEdge comparison done again at top of loop. Now try advance.
	     *  Don't bother looking at all tokens more than once.
	     */
	    if (ttp != (struct cm_tokenList *) &scp->tokenList) {
		ttp = (struct cm_tokenList *) ttp->q.next;
		/* maybe the next token will do better */
		thereIsHope = 1;
	    }
	} /* while */

	/* if leftEdge <= rightEdge, there remain bytes in a lock that can't
	 * be covered without the token being revoked, even when using colA and
	 * colB tokens.  We must fail to return the token in this case.
	 * Also clear the colA and colB usage information in newflags.
	 * This step is not abslutely neccessary because the caller ignores
	 * alternative token usage states in the flagsp in the revocation
	 * failure case.  However it is a good precaution to go ahead and
	 * clear these states anyway.  It also helps to document the
	 * behavior.
	 */
#ifdef SGIMIPS
	if (AFS_hcmp(leftEdge, CM_MAXEND) < 0 &&
#else  /* SGIMIPS */
	if (AFS_hcmp64(leftEdge, 0, CM_MAXEND) < 0 &&
#endif /* SGIMIPS */
	    AFS_hcmp(leftEdge, rightEdge) <= 0) {
	    newFlags &= ~(AFS_REVOKE_COL_A_VALID | AFS_REVOKE_COL_B_VALID);
	    /*
	     * Don't return the token, but do return blocker's info, if
	     * it is asked by the caller.
	     */
	    if (rlockerp) {
	        rlockerp->l_whence = 0;
	        rlockerp->l_type = tlp->data.l_type;
#ifdef SGIMIPS 
		rlockerp->l_start_pos = tlp->data.l_start;
		rlockerp->l_end_pos = tlp->data.l_len;
#else  /* SGIMIPS */
	        AFS_hset64(rlockerp->l_start_pos, 0, tlp->data.l_start);
	        AFS_hset64(rlockerp->l_end_pos, 0, tlp->data.l_len);
#endif /* SGIMIPS */
	        rlockerp->l_pid = tlp->data.l_pid;
#if defined(AFS_AIX31_ENV) || defined(AFS_SUNOS5_ENV) || defined(SGIMIPS)
#ifdef SGIMIPS
		rlockerp->l_sysid = (unsigned int) tlp->data.l_sysid;
#else  /* SGIMIPS */
		rlockerp->l_sysid = tlp->data.l_sysid;
#endif /* SGIMIPS */
#endif
		newFlags |= AFS_REVOKE_LOCKDATA_VALID;
		if (flagsp)
		   *flagsp = newFlags;
	    }
	    icl_Trace4(cm_iclSetp, CM_TRACE_TRYLOCKREV_3,
		       ICL_TYPE_LONG, tlp->data.l_type,
		       ICL_TYPE_LONG, tlp->data.l_start,
		       ICL_TYPE_LONG, tlp->data.l_len,
		       ICL_TYPE_LONG, tlp->data.l_pid);
	    return 1;
        }
    } /* for */

    /*
     * We didn't find any locks we can't handle, so we can return this token.
     */
    AFS_hset64(ttype, 0, tokenType);
    if (flagsp)
        *flagsp = newFlags;

    icl_Trace3(cm_iclSetp, CM_TRACE_TRYLOCKREV_4,
	       ICL_TYPE_LONG, tokenType,
	       ICL_TYPE_HYPER, &returnTokenp->token.tokenID,
	       ICL_TYPE_LONG, newFlags);
    cm_RemoveToken(scp, returnTokenp, &ttype); /* remove the token */

    if (newFlags & AFS_REVOKE_COL_A_VALID) {
	colAp->expirationTime += osi_Time();
	cm_AddToken(scp, colAp, serverp, 0);
    }
    if (newFlags & AFS_REVOKE_COL_B_VALID) {
	colBp->expirationTime += osi_Time();
	cm_AddToken(scp, colBp, serverp, 0);
    }
    return 0;
}

#ifdef AFS_SUNOS5_ENV
/*
 * SunOS 5: determine whether there are any remote locks left on a file.
 *	    lockd uses this to decide whether to close the open file.
 */
static int no_remote_locks(scp)
  struct cm_scache *scp;
{
    register struct record_lock *tlp;

    for (tlp = (struct record_lock *) QNext(&scp->lockList);
	 tlp != (struct record_lock *) &scp->lockList;
	 tlp = (struct record_lock *) QNext(&tlp->links)) {
	if (tlp->data.l_sysid != 0)
	    return 0;
    }
    return 1;
}
#endif /* AFS_SUNOS5_ENV */

/*
 * Lockctl vnode operation.  Called by fcntl and lockf system calls.
 */
#ifdef AFS_SUNOS5_ENV
cm_lockctl(vp, acmd, flockp, flag, offset, credp)
  struct vnode *vp;
  int acmd;
  struct flock *flockp;
  int flag;
  offset_t offset;
  osi_cred_t *credp;
#else /* !AFS_SUNOS5_ENV */
#ifdef SGIMIPS
cm_lockctl(
  struct vnode *vp,
  struct flock *flockp,
  int acmd,
  osi_cred_t *credp,
  int id,
  long offset)
#else  /* SGIMIPS */
cm_lockctl(vp, flockp, acmd, credp, id, offset)
  struct vnode *vp;
  struct flock *flockp;
  int acmd;
  osi_cred_t *credp;
  int id;
  long offset;
#endif /* SGIMIPS */
#endif /* AFS_SUNOS5_ENV */
{
    struct cm_rrequest rreq;
    register long code;
    struct cm_lockf tinLock, toutLock;
    struct cm_scache *scp;

#ifdef AFS_SUNOS5_ENV
    int lockd = 0;			/* called from lockd daemon */
#endif

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    /*
     * Convert the l_start / l_len fields to absolute byte ranges, taking into
     * account the file's current length as well as the l_whence field.
     */
    cm_InitReq(&rreq, credp);

    /*
     * Use GetSLock if we have to look at the file length for
     * l_whence type 2.
     */
    if (flockp->l_whence == 2) {
	code = cm_GetSLock(scp, TKN_STATUS_READ, CM_SLOCK_READ,
			   WRITE_LOCK, &rreq);
	if (code) return cm_CheckError(code, &rreq);
    } else
	lock_ObtainWrite(&scp->llock);

    /* convert to absolute offset */
    if (flockp->l_whence == 1)
	flockp->l_start += offset;
    else if (flockp->l_whence == 2)
	flockp->l_start += scp->m.Length;
    flockp->l_whence = 0;

    /*
     * The entire file (ie. beyond eof) will be locked, if flockp->l_len == 0.
     */
    if (flockp->l_len < 0 || flockp->l_start < 0) {
        code = EINVAL;
        goto out;
    }
    if (flockp->l_len == 0)
        flockp->l_len = CM_MAXEND;

#ifdef AFS_SUNOS5_ENV
    switch (acmd) {
	case F_RGETLK: acmd = F_GETLK; lockd = true; break;
	case F_RSETLK: acmd = F_SETLK; lockd = true; break;
	case F_RSETLKW: acmd = F_SETLKW; lockd = true; break;
    }
#endif

#ifdef AFS_SUNOS5_ENV
    if (lockd)
	vnl_ridset(flockp);
    else
#endif /* AFS_SUNOS5_ENV */
	vnl_idset(flockp);

    code = SysToCM(flockp, &tinLock);
    if (code)
        goto out;

    if (acmd == F_GETLK) {
	/*
	 * return if lock is set. We don't have a call that tells us if a lock
	 * is set so we act a little conservatively, returning "no lock set" if
	 * we can't tell. Passing a 2 for the last arg tells cm_SetLockF
	 * to pass back the blocking lock info.
	 */
	code = cm_SetLockF(scp, &tinLock, &toutLock, &rreq, 2);
	CMToSys(&toutLock, flockp);		/* convert structure back */
#ifdef AFS_SUNOS5_ENV
#ifdef AFS_SUNOS55_ENV
	/*
	 * Solaris 5.5 as extended the kernel interface for file locking
	 * There is now support for checking if a lock is remote and a
	 * callback, presumably to notify lockd.  I need to determine how
	 * to use these functions.  dlc 9/28/95
	 */
	if (lockd && no_remote_locks (scp)) {
	    cm_printf("dfs: lockd hack - write some code and try again\n");
	}
#else	/* AFS_SUNOS55_ENV */
	/* Pre Solaris 5.5 */
	if (lockd && no_remote_locks (scp))
	    flockp->l_xxx = LOCKD_LAST_REF;
#endif	/* AFS_SUNOS55_ENV */
#endif

    } else if (acmd == F_SETLK || acmd == F_SETLKW) {
       /*
	* This branch represents three separate types of operation.  F_SETLK
	* means set a lock and fail if you can't obtain it.  F_SETLKW means set
	* the lock, and wait for it if not available.  But in either case, if
	* the type field of the lock is F_UNLCK, we drop the lock instead,
	* which represents another code.
	*
	* XXX Temporary hack -- to implement F_SETLKW for lockd on SunOS 5,
	*     we aren't allowed to block.  If the lock can't be granted
	*     immediately, we are supposed to return EINTR.  Later, when it
	*     is granted, there are elaborate protocols for notifying lockd.
	*     Until we have implemented that, we just return ENOLCK.
	*/
	if (tinLock.type == CM_UNLOCK) {/* drop a lock */
	    code = cm_ReleaseLockF(scp, &tinLock, &rreq);
#ifdef AFS_SUNOS5_ENV
#ifdef AFS_SUNOS55_ENV
	/*
	 * Solaris 5.5 as extended the kernel interface for file locking
	 * There is now support for checking if a lock is remote and a
	 * callback, presumably to notify lockd.  I need to determine how
	 * to use these functions.  dlc 9/28/95
	 */
	if (lockd && no_remote_locks (scp)) {
	    cm_printf("dfs: lockd hack - write some code and try again\n");
	}
#else	/* AFS_SUNOS55_ENV */
	if (lockd && no_remote_locks (scp))
	    flockp->l_xxx = LOCKD_LAST_REF;
#endif	/* AFS_SUNOS55_ENV */
#endif
	} else {			/* obtain a lock, possibly waiting */
#ifndef AFS_SUNOS5_ENV
	    code = cm_SetLockF(scp, &tinLock, (struct cm_lockf *) 0, &rreq,
			       (acmd == F_SETLKW ? 1 : 0));
#else
	    code = cm_SetLockF(scp, &tinLock, (struct cm_lockf *) 0, &rreq,
			       (acmd == F_SETLKW && !lockd ? 1 : 0));
	    if (lockd && acmd == F_SETLKW && code == EAGAIN) {
		code = ENOLCK;
		cm_printf("dfs: blocking lock request from lockd not granted\n");
	    }
#endif /* !AFS_SUNOS5_ENV */
	}
    } else
	code = EINVAL;		/* probably should panic here */

out:
    lock_ReleaseWrite(&scp->llock);
    return cm_CheckError(code, &rreq);
}
