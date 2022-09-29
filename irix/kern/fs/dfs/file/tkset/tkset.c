/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkset.c,v 65.5 1998/04/01 14:16:37 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */

/* Copyright (C) 1996, 1990 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>
#include <dcedfs/tkm_tokens.h>
#include <dcedfs/volume.h>
#include <dcedfs/vol_init.h>
#include <tkset.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkset/RCS/tkset.c,v 65.5 1998/04/01 14:16:37 gwehrman Exp $")

/* This module implements an abstraction representing a set of tokens that the
 * caller needs to obtain simultaneously.  The complication here is that the
 * obtainer of a token can block waiting to be granted a token.
 * In order to avoid deadlock in that case, we allow revocation of the
 * tokens we have while obtaining new tokens.
 *
 * Furthermore, there are some circumstances in which the caller needs to know
 * whether, as a part of obtaining a token, this package had to temporarily 
 * release a token (so as to be able to reobtain tokens in the standard 
 * locking order).  The error code TKSET_RELOCK indicates success (as does
 * the return code 0), but also that some tokens may have been dropped
 * during the token-obtaining phase, and any cached info based on those
 * tokens should be re-validated.
 *
 * The tkset_AddTokenSet call returns this information. This is typically used 
 * by something like SAFS_Unlink, where we lookup the name from the directory,
 * and then try to get a token for the file so named. If the file comes before 
 * the directory in the locking hierarchy, we have to reobtain the tokens in 
 * the other order, and if we release the directory token in the process, we 
 * have to recheck the results of the lookup, since its contents may have 
 * changed.  We have to do this rechecking if we end up releasing any of the 
 * tokens we previously held: thus the motivation for this return parameter.
 *
 * Finally a bit about synchronizing this package. The rule here is that things
 * don't get deleted until their reference count goes to 0, and of course, the
 * object must get fully deleted before the reference count can grow above 0 
 * again. So, we atomically delete the token set when deleting it, and we 
 * require holding the tkset_glock (global lock) when increasing the reference 
 * count on a token set object.
 *
 * Note also that in general, the only concurrent accesses to an individual 
 * token set come from token revocation calls; people won't
 * be calling the add token set function from multiple threads concurrently. If
 * we need this functionality, we should add another lock to this package at a
 * higher level to synchronize those users. Right now, ObtainSet won't 
 * necessarily work properly if it is executed concurrently by several threads.
 *
 * There is some anti-starvation code here to prevent two different threads
 * who are getting write tokens on a dir and then more tokens on some other
 * file, and retrying on TKM_RELOCK, from looping forever.
 *
 * There are thus several states:
 * Idle: allSets is empty.  Serial is false.
 *
 * Normal: allSets is non-zero and loopingSets is zero.  The guy who's
 * running has a retryCount of 0.  More than one tkset can be running
 * concurrently, and all have small retryCounts.
 *
 * Serial: allSets is non-zero, and loopingSets may be non-zero.
 * Serial is set.  In this mode, we only execute guys with high
 * retryCount values; others set easyWaiting and wait for normal mode to
 * resume.  We're also in serial mode when we have a bunch of guys
 * in normal mode, but we're waiting for them to finish because we have
 * someone with a high count waiting to start.
 *
 * All state transitions occur when we create or delete a tkset_set.
 */

/* list of all active sets; if in queueing mode, there's only one guy
 * executing, and he's in this list when he's running.
 */
struct tkset_set *tkset_allSets=0;

/* List of all sets delayed repeatedly because of TKM_RETRY return codes.
 * A set is put here when he's made too many retries, and sits here
 * until the system is idle and he gets to the top of the list.  When
 * he gets to the head of the queue, he's moved (by tkset_Create) into
 * the tkset_allSets list, and starts running.
 */
struct tkset_set *tkset_loopingSets = 0;

/* flag set if tksets are waiting that haven't been delayed
 * themselves, but which are being delayed to allow the
 * loopingSets to run.
 */
int tkset_easyWaiting = 0;

/* flag to tell us if we're in serial mode */
int tkset_serial = 0;

struct lock_data tkset_glock;
struct tkm_race tkset_race;	/* global race structure */

static int tkset_Zap _TAKES((struct tkset_set *));
static int tkset_nlrele _TAKES((struct tkset_set *));
static int ReleaseOneToken _TAKES((struct tkset_set *, struct tkset_token *));

/*
 * DON'T have special routines for FAST mallocing but rather use COMMON stuff 
 */
#define	MIN_ALLOCSIZE	(sizeof(struct tkset_token)) /* Size of max struct */

static struct tksSpace {
    struct tksSpace *next;
} *freeSpaceList = 0;
static struct lock_data tsalloc_lock;
static tkset_spaceAlloced = 0;


#ifdef SGIMIPS
static char * tkset_AllocSpace(void) {
#else
static char * tkset_AllocSpace() {
#endif
    register struct tksSpace *tcp;

    lock_ObtainWrite(&tsalloc_lock);
    if (!freeSpaceList) {
	tkset_spaceAlloced++;
	tcp = (struct tksSpace *) osi_Alloc(MIN_ALLOCSIZE);
    }
    else {
	tcp = freeSpaceList;
	freeSpaceList = tcp->next;
    }
    lock_ReleaseWrite(&tsalloc_lock);
    return (char *) tcp;
}

#ifdef SGIMIPS
static void tkset_FreeSpace(char *datap)
#else
static tkset_FreeSpace(datap)
    register char *datap;    
#endif
{
    lock_ObtainWrite(&tsalloc_lock);
    ((struct tksSpace *)datap)->next = freeSpaceList;
    freeSpaceList = ((struct tksSpace *)datap);
    lock_ReleaseWrite(&tsalloc_lock);
}


/*initialize package */
int tkset_Init() {
    static int initDone = 0;

    if (initDone) 
	return 0;		/* Already initialized, pretend success */
    initDone = 1;
    lock_Init(&tsalloc_lock);	/* init the space allocation lock */
    lock_Init(&tkset_glock);	/* init the basic global lock */
    tkm_Init();			/* init the token manager itself */
    tkm_InitRace(&tkset_race);
    return 0;
}


/* create a new token set.  Called with one parameter, a count of
 * how many times the caller has retried due to getting TKM_RELOCK
 * return codes from tkset_AddTokenSet.
 *
 * The idea is that we can hold up tkset_Create calls if we seem to
 * be thrashing tokens in the file server.  Passing the retryCount
 * to tkset_Create allows the policy to be completely hidden within
 * the tkset package.
 *
 * The policy that we're going to implment is if the retryCount is
 * greater than some compile-time max, we'll block this call until
 * there are no other calls running, and then run it, blocking all
 * other calls.
 */

#define tkset_HIGHRETRY(rc)	((rc)>TKSET_MAXRETRIES)

#ifdef SGIMIPS
struct tkset_set *tkset_Create(
  long retryCount)
#else
struct tkset_set *tkset_Create(retryCount)
  long retryCount;
#endif
{
    register struct tkset_set *setp;
    struct tkset_set *tsetp, **lsetpp;

    setp = (struct tkset_set *) tkset_AllocSpace();
    setp->flags = 0;
    setp->revokeCount = 0;
    setp->tokens = (struct tkset_token *)0;
    setp->volp = (struct volume *) 0;
    setp->refCount = 1;	
    lock_ObtainWrite(&tkset_glock);
    if (tkset_HIGHRETRY(retryCount)) {
	/* add ourselves to the end of the list */
	for(lsetpp = &tkset_loopingSets; tsetp = *lsetpp; lsetpp = &tsetp->next)
	    ;
	setp->next = (struct tkset_set *) 0;
	*lsetpp = setp;
	tkset_serial = 1;
    }
    /* invariant at start: high-retry requests are in the queue, others
     * are just floating around.  When we're done with this loop, the
     * request will be in tkset_allSets.
     */
    while (1) {
	/* check to see if we can start now, based on whether we're
	 * an easy or a hard tkset to start.
	 */
	if (tkset_HIGHRETRY(retryCount)) {
	    /* check to see if this is a hard one that can start now,
	     * because he's at the head of the queue and no one else
	     * is running.
	     */
	    if (setp == tkset_loopingSets && !tkset_allSets) {
		tkset_loopingSets = setp->next;	/* splice out of this list */
		break;	/* and start it */
	    }
	    /* otherwise, we block and try again */
	    osi_SleepW((char *)&tkset_loopingSets, &tkset_glock);
	}
	else {
	    /* check to see if this is an easy one that can start now */
	    if (!tkset_serial) break;	/* start it */
	    /* otherwise block until we're out of serial mode */
	    tkset_easyWaiting = 1;
	    osi_SleepW((char *)&tkset_easyWaiting, &tkset_glock);
	}
	lock_ObtainWrite(&tkset_glock);
    }

    /* splice into the allSets list */
    setp->next = tkset_allSets;
    tkset_allSets = setp;
    lock_ReleaseWrite(&tkset_glock);
    return setp;
}


/* Internal procedure.
 * Iterate through all the tokens in a set, ensuring that they've all been
 * obtained properly and completely.
 *
 * Must be called with the tkset_glock write-locked.
 *
 * N.B.  at most one process can be running this procedure on a given
 * token set at any given time.
 */
#ifdef SGIMIPS
static ObtainSet(
     struct tkset_set *asetp,
     afs_recordLock_t *blockerp,
     opaque *dmptrp)
#else
static ObtainSet(asetp, blockerp, dmptrp)
     struct tkset_set *asetp; 
     afs_recordLock_t *blockerp;
     opaque *dmptrp;
#endif
{
    register struct tkset_token *tp, *fp;
    struct hs_host *thostp = (struct hs_host *) 0;
    register long code;
    long flags;
    int done;
    struct tkm_raceLocal localRaceState;
    long revokeCount;	/* for detecting revokes during a grant sweep */
    /* for detecting loss of any token we had when we started */

    done = 0;
    /* keep trying until we verify that we have all desired tokens
     * without any revokes interrupting us.
     */
    while (!done) {
	done = 1;	/* start out optimistically */
	/* revoke count will change if any element in the set is revoked
	 * while we're obtaining other tokens.  If that happens, we'll
	 * have to verify that we still have all the desired tokens
	 * before we return success.
	 */
	revokeCount = asetp->revokeCount;
	for (tp = asetp->tokens; tp; tp = tp->next) {
	    /* Try to grant the token described by tp.
	     * We check by ensuring that the flags have been set to non-zero 
	     * and if it has been, our work for this token has already been 
	     * done. It was done by the async grant procedure when the token 
	     * was granted in the first place.
	     */
	    if (tp->flags & TKSET_HAVE_TOKEN) continue;

	    if (asetp->flags & TKSET_SET_DELETED) {/* Set deleted - we abort */
		if (asetp->volp) {
		    /* close this down too, since we're passing back an err code
			that is not TKSET_RELOCK */
		    vol_EndVnodeOp(asetp->volp, dmptrp);
		    VOL_RELE(asetp->volp);
		    asetp->volp = (struct volume *) 0;
		}
		return 1;
	    }

	    /* ask for the the token.  tkset never queues tokens, since
	     * we never wait for grants.  Either we get an error, or
	     * we get the appropriate token when the conflicting tokens
	     * are revoked.
	     */
	    flags = 0;
	    if (tp->flags & TKSET_OPTIMISTIC)
		flags |= TKM_FLAGS_OPTIMISTICGRANT;
	    if (tp->flags & TKSET_JUMPQUEUE)
		flags |= TKM_FLAGS_JUMPQUEUEDTOKENS;
	    if (tp->flags & TKSET_NO_NEWEPOCH)
		flags |= TKM_FLAGS_NONEWEPOCH;
	    if (tp->flags & TKSET_SAME_TOKENID)
		flags |= TKM_FLAGS_GETSAMEID;
	    if (tp->flags & TKSET_NO_REVOKE)
		flags |= TKM_FLAGS_EXPECTNOCONFL;
	    if (tp->flags & TKSET_VOLQUIESCE)
		flags |= TKM_FLAGS_FORCEVOLQUIESCE;

	    /* now, we're about to start playing around with tokens, so
	     * if caller has set a volume to release, release it now.
	     */
	    if (asetp->volp)
		vol_EndVnodeOp(asetp->volp, dmptrp);
	    /* next call drops lock and plays around with NOCLAMP flag */
	    ReleaseOneToken(asetp, tp);
	    tp->token.type = tp->desiredRights;
	    tkm_StartRacingCall(&tkset_race, &tp->fid, &localRaceState);
	    asetp->flags |= TKSET_SET_NOCLAMP;	/* allow revokes */
	    /* let revokes proceed while we're calling the TKM */
	    if (asetp->flags & TKSET_SET_WAITING) {
		asetp->flags &= ~TKSET_SET_WAITING;
		osi_Wakeup(asetp);
	    }
	    lock_ReleaseWrite(&tkset_glock);
	    code = tkm_GetToken(&(tp->fid), flags,
				(afs_token_t *) &tp->token, tp->hostp, (long) tp,
				blockerp);
	    if (asetp->volp) {
		if (code == 0)
		    code = vol_StartVnodeOp(asetp->volp, asetp->volSyncType, 1, dmptrp);
		if (code) {
		    VOL_RELE(asetp->volp);
		    asetp->volp = (struct volume *) 0;
		}
	    }
	    lock_ObtainWrite(&tkset_glock);
	    tkm_EndRacingCall(&tkset_race, &tp->fid, &localRaceState,
			      (afs_token_t *) &tp->token);
	    asetp->flags &= ~TKSET_SET_NOCLAMP;	/* block revokes again */
	    if (code == 0) {
		tp->flags |= TKSET_ID_GRANTED;
		if (AFS_hissubset(tp->desiredRights, tp->token.type)) {
		    /* we have all of the rights we need in this token */
		    tp->flags |= TKSET_HAVE_TOKEN;
		    continue;
		}
		/* otherwise, try again from the top */
		done = 0;	/* already done by revoke? */
		break;
	    }
	    else {
		/* oops, something went wrong, just give up */
		return code;
	    }
	}	/* for each token in the token set */

	/* if a revoke came in and damaged one of the tokens we already
	 * had, we have to verify that we still have all of the tokens
	 * in the set.
	 */
	if (revokeCount != asetp->revokeCount)
	    done = 0;
    }		/* while trying to get all tokens */

    return 0;
}


#ifdef SGIMIPS
int tkset_AddTokenSet(
    register struct tkset_set *asetp,
    struct afsFid *afidp,
    afs_token_t *atokenp,
    afs_recordLock_t *blockerp,
    struct hs_host *ahostp,
    int aflags,
    opaque *dmptrp)
#else
int tkset_AddTokenSet(asetp, afidp, atokenp, blockerp, ahostp, aflags, dmptrp)
    register struct tkset_set *asetp;
    struct afsFid *afidp;
    afs_token_t *atokenp;
    afs_recordLock_t *blockerp;
    struct hs_host *ahostp;
    int aflags;
    opaque *dmptrp;
#endif
{
    register struct tkset_token *tep, **lepp, *nep;
    struct volume *oldvolp;
    long code;
    long baseRevokeCount;

    if (!asetp) 
	return 0;		  	/* pretend success for trivial set */
    lock_ObtainWrite(&tkset_glock);

    /*	
     * Allocate and insert token element in list.
     */
    nep = (struct tkset_token *) tkset_AllocSpace();
    nep->next = asetp->tokens;
    asetp->tokens = nep;
    nep->hostp = ahostp;
    HS_HOLD(ahostp);
    nep->flags = 0;
    if (aflags & TKSET_ATS_WANTOPTIMISM)
	nep->flags |= TKSET_OPTIMISTIC;
    if (aflags & TKSET_ATS_WANTJUMPQUEUE)
	nep->flags |= TKSET_JUMPQUEUE;
    if (aflags & TKSET_ATS_NONEWEPOCH)
	nep->flags |= TKSET_NO_NEWEPOCH;
    if (aflags & TKSET_ATS_SAMETOKENID)
	nep->flags |= TKSET_SAME_TOKENID;
    if (aflags & TKSET_ATS_NOREVOKE)
	nep->flags |= TKSET_NO_REVOKE;
    if (aflags & TKSET_ATS_DONT_KEEP)
	nep->flags |= TKSET_DONT_KEEP;
    if (aflags & TKSET_ATS_FORCEVOLQUIESCE)
	nep->flags |= TKSET_VOLQUIESCE;

    FidCopy((&nep->fid), afidp);
    *((afs_token_t *) &nep->token) = *atokenp;
    /* save originally desired rights */
    nep->desiredRights = nep->token.type;
    baseRevokeCount = asetp->revokeCount;
    oldvolp = asetp->volp;

    /* 
     * Obtain token, possibly reobtaining others while waiting.
     */
    code = ObtainSet(asetp, blockerp, dmptrp);
    /* next is useful since we assume volume is released on non-tkset
     * relock errors.  Don't want confusion from returning this error
     * from strange paths.
     */
    osi_assert(code != TKSET_RELOCK);
    /* Similar assertion about whether volp is released on error. */
    /* Clients assume that they don't need vol_EndVnodeOp if this fails. */
    if (code && oldvolp)
	osi_assert(asetp->volp == 0);

    /* now, if we get here, we have all of the tokens we want.  However,
     * we may have temporarily given up some tokens that we had when we
     * started this procedure, and if so, we let our caller know of
     * that possibiliity.  Some callers care, and others don't.  In either
     * case, all of the desired tokens have now been obtained.
     */
    if (code == 0 && baseRevokeCount != asetp->revokeCount)
	code = TKSET_RELOCK;
    /* tell caller that we dropped the activeVnops count,
     * and released the volume.
     */
    lock_ReleaseWrite(&tkset_glock);
    return code;
}


/* 
 * This function is called to return a token set to tkm. Certain tokens may be 
 * marked as "do not return"; these will be kept around, and revokes wil make 
 * it back to the appropriate AFS 4 cache manager.
 *
 * This function decrements the ref count on the set.
 */
#ifdef SGIMIPS
int tkset_Delete(
    register struct tkset_set *asetp)
#else
int tkset_Delete(asetp)
    register struct tkset_set *asetp; 
#endif
{
    if (!asetp) 
	return 0;		  	/* pretend success for trivial set */
    lock_ObtainWrite(&tkset_glock);
    if (asetp->refCount == 1) 		/* last one - zap it! */
	tkset_Zap(asetp);
    else {
	asetp->flags |= TKSET_SET_DELETED;

	/* 
	 * Wakeup anyone waiting for it to be deleted 
	 */
	if (asetp->flags & TKSET_SET_WAITING) {
	    asetp->flags &= ~TKSET_SET_WAITING;
	    osi_Wakeup(asetp);
	}
	tkset_nlrele(asetp);	/* finally, we decrement the ref count */
    }
    lock_ReleaseWrite(&tkset_glock);
    return 0;
}


/* 
 * This function is called after verifying that all references to a token set 
 * have been dropped, to free the resource associated with a token set, 
 * including tokens.
 *
 * We must be sure to destroy the object w/o letting go of the tkset_glock, so 
 * that the reference count doesn't get a chance to go above 0 before we get 
 * this object out of the hash tables.
 */
#ifdef SGIMIPS
static int tkset_Zap(
    struct tkset_set *asetp)
#else
static int tkset_Zap(asetp)
    struct tkset_set *asetp; 
#endif
{
    register struct tkset_token *ttp, *ntp;
    register struct tkset_set *tsp, **lsp;
    register long code = 0, rcode = 0;

    /* 
     * First, must unthread from local chain, to prevent people from finding this
     * token set again.  Otherwise, refcount could go up in the middle of a
     * call to tkm_ReturnToken.
     */
    for (lsp = &tkset_allSets; tsp = *lsp; lsp = &tsp->next) {
	if (tsp == asetp) {
	    *lsp = tsp->next;
	    break;
	}
    }

    /* wakeup waiters if the system is idle */
    if (tkset_allSets == (struct tkset_set *) 0) {
	/* system is now idle.  Either take someone from the priority
	 * queue, or if there's no one there, wakeup the normal first-timers.
	 */
	if (tkset_loopingSets) {
	    osi_Wakeup((char *)&tkset_loopingSets);
	}
	else  {
	    tkset_serial = 0;	/* this is how we leave serial mode */
	    if (tkset_easyWaiting) {
		tkset_easyWaiting = 0;
		osi_Wakeup((char *)&tkset_easyWaiting);
	    }
	}
    }

    /* 
     * now release lock so that tkm_ReturnToken doesn't cause deadlocks. No one
     * can find this tkset any more, anyway, so we're the only ones access it.
     */
    lock_ReleaseWrite(&tkset_glock);

    for (ttp = asetp->tokens; ttp; ttp = ntp) {
	ntp = ttp->next;	/* Since we'll free it before advancing */
	/* 
	 * Release the element if it isn't still being used by the caller.
	 */
	if (!(ttp->flags & TKSET_DONT_RELEASE)
	    && (ttp->flags & TKSET_ID_GRANTED)) {
	    /* Don't call TKM if it was a free grant on R/O data. */
	    if (tkm_IsGrantFree(ttp->token.tokenID)) {
		code = 0;
	    } else {
		code = tkm_ReturnToken(&ttp->fid, &ttp->token.tokenID,
				       &ttp->token.type,
				       TKM_FLAGS_PENDINGRESPONSE);
	    }
	    if (code && !rcode) 
		rcode = code;	/* keep track of 1st non-zero return code */
	}
	if (ttp->hostp) 
	    HS_RELE(ttp->hostp);
	tkset_FreeSpace((char *)ttp);	/* finally, free the structure */
    }

    tkset_FreeSpace((char *)asetp);
    lock_ObtainWrite(&tkset_glock);
    return rcode;
}


/* 
 * It's called to prevent the tokens associated with a particular file ID from 
 * being returned when the token set is deleted. This is called when a server 
 * is returni tokens for a particular file ID back to the client via an RPC.
 */
#ifdef SGIMIPS
int tkset_KeepToken(
    struct tkset_set *asetp,
    struct afsFid *afidp,
    afs_token_t *atokenp)
#else
int tkset_KeepToken(asetp, afidp, atokenp)
    struct tkset_set *asetp;
    afs_token_t *atokenp;
    struct afsFid *afidp; 
#endif
{
    register struct tkset_token *ttp;

    if (!asetp) 
	return 0;		/* pretend success for trivial set */

    lock_ObtainWrite(&tkset_glock);
    for (ttp = asetp->tokens; ttp; ttp = ttp->next) {
	if (!FidCmp(&ttp->fid, afidp) && !(ttp->flags & TKSET_DONT_KEEP)) {
	    ttp->flags |= TKSET_DONT_RELEASE;
	    if (atokenp)
		*atokenp = *((afs_token_t *) &ttp->token);
	    lock_ReleaseWrite(&tkset_glock);
	    return 0;
	}
    }
    lock_ReleaseWrite(&tkset_glock);
    return ENOENT;
}


/* 
 * It's called by the host module when an async grant is received. It tries to 
 * deliver the token to the tkm, if anyone is waiting for it, and returns 0 if
 * it has been delivered, and 1 if it hasn't.
 *
 * IMPORTANT: it turns out we don't need to do async grant processing
 * in this package, so we always return 1.
 */
#ifdef SGIMIPS
int tkset_HereIsToken(
    struct afsFid *afidp,
    afs_token_t *atokenp,
    long areq)
#else
int tkset_HereIsToken(afidp, atokenp, areq)
    struct afsFid *afidp;
    long areq;
    afs_token_t *atokenp; 
#endif
{
    return 1;		/* We never found the appropriate token */
}


/* 
 * It's called by the host module when a token is revoked. Sleeps until it's 
 * safe to try to revoke the token, i.e. until tkm is no longer trying to 
 * accumulate the specified token in a token set.
 *
 * The guarantee made by this module is that after this has been called, the 
 * token set containing the token, if any, has been released.
 */
#ifdef SGIMIPS
int tkset_TokenIsRevoked(
    struct afsRevokeDesc *arp)
#else
int tkset_TokenIsRevoked(arp)
    struct afsRevokeDesc *arp; 
#endif
{
    register struct tkset_set *tsetp, *nsetp;
    register struct tkset_token *ttp;
    int found;

    lock_ObtainWrite(&tkset_glock);
    found = 0;
  retry:
    for (tsetp = tkset_allSets; tsetp; tsetp = nsetp) {
	/* if the token set has been deleted, we're not interested in
	 * any of its tokens; they've already been sent to the client.
	 * Otherwise, we check all obtained tokens.
	 */
	if (!(tsetp->flags & TKSET_SET_DELETED)) {
	    for (ttp = tsetp->tokens; ttp; ttp = ttp->next) {
		if (ttp->flags & TKSET_ID_GRANTED) {/* have token already */
		    if (AFS_hsame(arp->tokenID, ttp->token.tokenID)) {
			/* note we found it to avoid race code */
			found = 1;
			if (tsetp->flags & TKSET_SET_NOCLAMP) {
			    /* bump revoke count so ObtainSet knows that
			     * tokens have disappeared.
			     */
			    tsetp->revokeCount++;
			    /* remove revoked tokens */
			    AFS_HOP(ttp->token.type, &~, arp->type);
			    /*
			     * Now, if we don't have all of the tokens that the
			     * caller desired, turn off TKSET_HAVE_TOKEN so
			     * that ObtainSet knows that this token isn't
			     * everything that it wanted it to be.
                             */
			    if (!AFS_hissubset(ttp->desiredRights,
					       ttp->token.type)) {
				ttp->flags &= ~TKSET_HAVE_TOKEN;
			    }
			    break;	/* at most one here */
			}
			else {
			    /* if we get here, the token set is clamped,
			     * i.e. someone is using the tokens right now
			     * and we can't revoke the tokens.  Wait for
			     * them to be done, or to unclamp the set.
			     */
			    tsetp->flags |= TKSET_SET_WAITING;
			    tsetp->refCount++;
			    osi_SleepW(tsetp, &tkset_glock);
			    lock_ObtainWrite(&tkset_glock);
			    tkset_nlrele(tsetp);
			    goto retry;
			}
		    } /* if this is the token being revoked */
		} /* if we have a token */
	    } /* for loop over all tokens */
	} /* if the set is worth looking at */
	nsetp = tsetp->next;
    }
    if (!found) {
	tkm_RegisterTokenRace(&tkset_race, &arp->fid, &arp->type);
    }
    lock_ReleaseWrite(&tkset_glock);
    return 0;
}


#ifdef SGIMIPS
static int tkset_nlrele(
    register struct tkset_set *asetp)
#else
static int tkset_nlrele(asetp)
    register struct tkset_set *asetp; 
#endif
{
    if ((!--asetp->refCount) && (asetp->flags & TKSET_SET_DELETED))
	tkset_Zap(asetp);
#ifdef SGIMIPS
    return 0;
#endif
}


/* Return a token, temporarily releasing the token clamp if necessary.
 * Releases the global lock so that async grants don't deadlock.
 *
 * Returns all tokens ever associated with this ID, so that we
 * don't leak tokens when we have token race conditions leading
 * to final results we're not sure of.
 */
#ifdef SGIMIPS
static int ReleaseOneToken(
  register struct tkset_set *asetp,
  register struct tkset_token *atokenp)
#else
static int ReleaseOneToken(asetp, atokenp)
  register struct tkset_set *asetp;
  register struct tkset_token *atokenp; 
#endif
{
    register long code;
    afs_hyper_t ttype;

    if (atokenp->flags & TKSET_ID_GRANTED) {
	atokenp->flags &= ~TKSET_ID_GRANTED;
	/* Don't call TKM if it was a free grant on R/O data. */
	if (tkm_IsGrantFree(atokenp->token.tokenID)) {
	    code = 0;
	} else {
	    asetp->flags |= TKSET_SET_NOCLAMP;
	    if (asetp->flags & TKSET_SET_WAITING) {
		asetp->flags &= ~TKSET_SET_WAITING;
		osi_Wakeup(asetp);
	    }
	    /* could have been optimistic granted */
	    AFS_hset64(ttype, ~0, ~0);
	    lock_ReleaseWrite(&tkset_glock);
	    code = tkm_ReturnToken(&atokenp->fid,
				   &atokenp->token.tokenID, 
				   (tkm_tokenSet_t *) &ttype,
				   TKM_FLAGS_PENDINGRESPONSE);
	    lock_ObtainWrite(&tkset_glock);
	    asetp->flags &= ~TKSET_SET_NOCLAMP;
	}
    }
    return 0;
}

#ifdef notdef
/*
 * PLEASE DO NOT REMOVE THIS CODE!  WE ENABLE IT PERIODICALLY HERE AT
 * TRANSARC FOR DEBUGGING PURPOSES.
 */

struct tkmdeb_record {
    struct tkmdeb_record *next;
    afs_hyper_t tid;
    afs_hyper_t rights;
    long flags;
    long field;
} *tkmdeb_list = 0;


#ifdef SGIMIPS
tkmdeb_Grant(
  afs_hyper_t *atid,
  afs_hyper_t *arights,
  long aflags,
  long afield)
#else
tkmdeb_Grant(atid, arights, aflags, afield)
  afs_hyper_t *atid;
  afs_hyper_t *arights;
  long aflags;
  long afield;
#endif
{
    register struct tkmdeb_record *tp;

    /* if no token allocated, just return */
    if (AFS_hiszero(tp->tid))
	return 0;
    tp = (struct tkmdeb_record *)osi_Alloc(sizeof(*tp));
    tp->tid = *atid;
    tp->rights = *arights;
    tp->flags = aflags;
    tp->field = afield;
    tp->next = tkmdeb_list;
    tkmdeb_list = tp;
    return 0;
}

#ifdef SGIMIPS
tkmdeb_Return(
  afs_hyper_t *atid,
  afs_hyper_t *arights)
#else
tkmdeb_Return(atid, arights)
  afs_hyper_t *atid, *arights;
#endif
{
    register struct tkmdeb_record *tp, **lpp;

    lpp = &tkmdeb_list;
    for(tp = *lpp; tp; lpp = &tp->next, tp = *lpp) {
	if (AFS_hsame(tp->tid, *atid)) {
	    AFS_HOP(tp->tid, &~ atid);
	    tp->flags |= 0x10000;
	    if (AFS_hiszero(tp->tid)) {
		*lpp = tp->next;
		osi_Free(tp, sizeof(*tp));
	    }
	    break;
	}
    }
}
#endif /* notdef */
