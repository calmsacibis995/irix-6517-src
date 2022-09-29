/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_tokens.c,v 65.12 1998/08/24 18:49:39 bdr Exp $";
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
#include <dcedfs/afs4int.h>
#include <dcedfs/tkn4int.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/fshs_errs.h>		/* errors from fshost */
#include <dcedfs/vol_errs.h>
#include <dcedfs/tkm_errs.h>
#include <dcedfs/tkm_tokens.h>
#include "cm.h"				/* Cm-based standard headers */
#include "cm_conn.h"
#include "cm_dcache.h"
#include "cm_cell.h"
#include "cm_volume.h"
#include "cm_stats.h"
#include "cm_dnamehash.h"

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_tokens.c,v 65.12 1998/08/24 18:49:39 bdr Exp $")

static struct cm_tokenList *cm_FreeTokenList = 0;/* Free token linked list */
static long cm_FreeTokenCount = 0;	/* size of free list */
struct lock_data cm_tokenlock;		/* lock: updates to token list */
struct lock_data cm_qtknlock;		/* Sync lock for tokens queue */
struct lock_data cm_racinglock;		/* lock on racing structures */
static struct cm_racing cm_racingList;	/* racing list */

#define CM_RANGE_TOKENS \
    (TKN_LOCK_READ | TKN_LOCK_WRITE | TKN_DATA_READ|TKN_DATA_WRITE)

/*
 * 32/64-bit interoperability change:
 *
 * This macro clears the upper 32 bits of the start and end positions
 * in the record lock struct pointed to by blkrp, so that 64-bit range
 * checking works properly, if the server (svp) doesn't know about the
 * 64-bit extensions to the afs_recordLock_t type (added in DCE 1.1).
 * We can tell that the server is a 32-bit server if we failed to exchange max
 * file sizes via AFS_SetParams().  In this case svp->supports64bit will be
 * false and svp->maxFileSize will be set to 2^31-1.
 */
#define DFS_ADJUST_LOCK_EXT(blkrp, svp) \
    MACRO_BEGIN \
        if ((blkrp) && !(svp)->supports64bit) { \
	    AFS_hzerohi((blkrp)->l_start_pos); \
	    AFS_hzerohi((blkrp)->l_end_pos); \
	} \
    MACRO_END

/*
 * Maximum token expiration period CM will assign.  The way token
 * expirations will be good up to the last 2 months of when the
 * system time will wrap (way in the future).
 */
#define CM_MAX_TOKEN_EXPTIME 5184000	/* 60*60*24*60 or 60 days */

struct cm_tokenStat tokenStat = {0, 0, 0, 0, 0, 0, 0, 0, 0};

static void cm_QuickQueueAToken _TAKES((struct cm_server *,
					struct cm_tokenList *));
static int cm_ScanAsyncGrantingCall _TAKES((afs_token_t *,
					    struct cm_server *,
					    struct cm_volume *));
static int cm_HaveHereToken _TAKES((struct cm_volume *));
static void cm_FreeTokenEntry _TAKES((struct cm_tokenList *));

/*
 * Quickly allocate a free token list entry from a free list, or memory
 * if the free list is empty.
 */
#ifdef SGIMIPS
static struct cm_tokenList *cm_AllocTokenEntry(void) 
#else  /* SGIMIPS */
static struct cm_tokenList *cm_AllocTokenEntry() 
#endif /* SGIMIPS */
{
    register struct cm_tokenList *tlp;

    lock_ObtainWrite(&cm_tokenlock);
    if (tlp = cm_FreeTokenList) {
	cm_FreeTokenList = (struct cm_tokenList *) tlp->q.next;
	cm_FreeTokenCount--;
    } else {
	tlp = (struct cm_tokenList *) osi_Alloc(sizeof(struct cm_tokenList));
    }
    tokenStat.tokenAdded++;
    lock_ReleaseWrite(&cm_tokenlock);
    bzero((caddr_t)tlp, sizeof(*tlp));
    return tlp;
}


/* 
 * Free a token entry from a vnode list.  The vnode must be write-locked
 * to make this call.
 */
#ifdef SGIMIPS
void cm_DelToken(register struct cm_tokenList *atp) 
#else  /* SGIMIPS */
void cm_DelToken(atp)
    register struct cm_tokenList *atp; 
#endif /* SGIMIPS */
{
    if (atp->refCount == 0) {
	QRemove((struct squeue *) atp);
	cm_FreeTokenEntry(atp);
    } else 
	atp->flags |= CM_TOKENLIST_DELETED;
}


/* 
 * Release a token list element; vnode must be write-locked 
 */
#ifdef SGIMIPS
void cm_ReleToken(register struct cm_tokenList *atp)
#else  /* SGIMIPS */
void cm_ReleToken(atp)
    register struct cm_tokenList *atp; 
#endif /* SGIMIPS */
{
    if (atp->refCount == 0) {
	panic("rele token");
    }
    if (--atp->refCount == 0) {
	if (atp->flags & CM_TOKENLIST_DELETED) {
	    QRemove((struct squeue *) atp);	/* free the entry */
	    cm_FreeTokenEntry(atp);
	}
    }
}

/*
 * Free a tokenList entry, quickly.
 */
#ifdef SGIMIPS
static void cm_FreeTokenEntry(register struct cm_tokenList *tlp) 
#else  /* SGIMIPS */
static void cm_FreeTokenEntry(tlp)
    register struct cm_tokenList *tlp; 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_tokenlock);
    tlp->q.next = (struct squeue *) cm_FreeTokenList;
    cm_FreeTokenList = tlp;
    cm_FreeTokenCount++;
#ifdef CM_ENABLE_COUNTERS
    if (AFS_hgetlo(tlp->token.type) & (TKN_STATUS_READ | TKN_STATUS_WRITE)) {
	CM_BUMP_COUNTER(statusTokensReleased);
    }
    if (AFS_hgetlo(tlp->token.type) & (TKN_DATA_READ | TKN_DATA_WRITE)) {
	CM_BUMP_COUNTER(dataTokensReleased);
    }
#endif /* CM_ENABLE_COUNTERS */
    tokenStat.ReleaseTokens++; /* For NOW Temp debug !! */
    lock_ReleaseWrite(&cm_tokenlock);
}


/*
 * Free all tokens in a token list.  Disregards tokenList ref counts.
 */
#ifdef SGIMIPS
void cm_FreeAllTokens(struct squeue *tokenQueuep)
#else  /* SGIMIPS */
void cm_FreeAllTokens(tokenQueuep)
    struct squeue *tokenQueuep;
#endif /* SGIMIPS */
{
    register struct cm_tokenList *tlp, *ntp;

    for (tlp = (struct cm_tokenList *) tokenQueuep->next; 
	 tlp != (struct cm_tokenList *) tokenQueuep; tlp = ntp) {
	ntp = (struct cm_tokenList *) tlp->q.next;
	cm_FreeTokenEntry(tlp);
    }
}

/* 
 * Same as GetTokensRange (below), except that we want the whole file 
 * Call with lock held on scache.
 */
long cm_GetTokens(
  struct cm_scache *scp,
  long tokens,
  struct cm_rrequest *rreqp,
  int setLock)
{
    afs_token_t ttoken;
    afs_recordLock_t afsBlocker;

    AFS_hset64(ttoken.type, 0, tokens);
    AFS_hzero(ttoken.beginRange);
    ttoken.endRange = osi_hMaxint64;
    AFS_hzero(ttoken.tokenID);
    return cm_GetTokensRange(scp, &ttoken, rreqp, 0, &afsBlocker, setLock);
}

/*
 * Get one or more tokens, as per tokenp.  Assumes structure locked, and
 * returns the structure locked again.  However, structure may be unlocked
 * during processing if tokens weren't there.  If they were, we guarantee
 * not to release the lock.
 *
 * Note that this function also checks whether the fileset already has the 
 * HERE token or not. If not, it will get the HERE token first before acquiring
 * any file tokens for the user. The HERE token is stored in the volume.
 * However, if it notices that it has a HERE token from a wrong server (the 
 * fileset is mysteriously moved), it will then start the TSR-move procedure 
 * at run time to sweep all existing tokens and move them to the new server.
 * 
 * This function will make a last call to cm_HaveHereToken() before exit to 
 * guarantee the CM has acquired the file tokens from the same server from 
 * which it got the HERE token. 
 *
 * The request bits are defined by the CM_GETTOKEN_... defines in cm_tokens.h.
 */
#ifdef SGIMIPS
long cm_GetTokensRange(
   register struct cm_scache *scp,
   afs_token_t *tokenp,
   struct cm_rrequest *rreqp,
   int request,
   afs_recordLock_t *afsBlockerp,
   int setLock) 
#else  /* SGIMIPS */
long cm_GetTokensRange(scp, tokenp, rreqp, request, afsBlockerp, setLock)
   register struct cm_scache *scp;
   afs_token_t *tokenp;
   struct cm_rrequest *rreqp;
   int request;
   afs_recordLock_t *afsBlockerp;
   int setLock; 
#endif /* SGIMIPS */
{
   struct lclHeap {
       afsFetchStatus OutStatus;
       afsVolSync tsync;
       afs_token_t realToken;
       int grantRaceCode;		/* did we find a racing token grant? */
       int reallyOK;
   } *lhp = NULL;
   register long code = 0;
   register struct cm_conn *connp;
   struct cm_server *tserverp;
   int safety = 0;
   long srvix;
   u_long startTime;
   unsigned32 Flags = AFS_FLAG_RETURNTOKEN;
   struct cm_volume *volp, *heldVolp = 0;
   error_status_t st;
   int skipHaveTokens;

   icl_Trace2(cm_iclSetp, CM_TRACE_GETTOKENS, ICL_TYPE_POINTER, scp,
	      ICL_TYPE_LONG, AFS_hgetlo(tokenp->type));

   if (code = scp->asyncStatus) {
       return code;
   }
   if (!(request & CM_GETTOKEN_EXACT) &&	/* ! Matching exact token */
       (request & CM_GETTOKEN_ASYNC)) 		/* Get Async token */
       Flags |= AFS_FLAG_ASYNCGRANT;

   skipHaveTokens = (request & CM_GETTOKEN_FORCE);

   if (!(volp = scp->volp)) {
       if (rreqp) {
	   cm_FinalizeReq(rreqp);
	   rreqp->volumeError = 1;
       }
       return -1;
   }

   /* Keep stack frame small by doing heap-allocation */
   lhp = (struct lclHeap *)osi_AllocBufferSpace();
   for (;;) {
      /* 
       * Check to see if we already have the right tokens 
       */
      code = 0;				/* in case we looped */
      if (!skipHaveTokens) {
	  if (cm_HaveTokensRange(scp, tokenp,
				 ((request & CM_GETTOKEN_EXACT) ? 1 : 0),
				 &lhp->reallyOK))
	      break;			/* OK. Break the while loop */
	  if (lhp->reallyOK & CM_REALLYOK_LENINVAL) {
	      /* we have the token be the length is too large, just return
               * EOVERFLOW now without calling back to the server. */
	      code = osi_errDecode(DFS_EOVERFLOW);
	      break;
	  }
      }
      skipHaveTokens = 0;
      /*
       * Check to make sure we have a HERE token, if not we have to get it.
       */
      if (cm_HaveHereToken(volp)) {
	 lock_Release(&scp->llock, setLock);
	 if (safety++ > 10)
	     osi_Wait(1000, 0, 0);	/* for performance reasons */
	 cm_StartTokenGrantingCall();
	 /*
	  * Make sure we do the hold operation at least once before making
	  * the RPC.  Don't do it more often, to simplify cleanup.
	  */
	 if (!heldVolp) {
	     heldVolp = volp;  /* note that we're holding it, for release */
	     cm_StartVolumeRPC(volp);	/* need both to match EndVolumeRPC */
	     cm_HoldVolume(volp);
	 }
	 do {
	    tserverp = 0;
	    if (connp = cm_Conn(&scp->fid,  MAIN_SERVICE(SRT_FX),
				rreqp, &volp, &srvix)) {
		/*
		 * If somebody is doing TSR, don't jump in ahead of him.
		 */
		tserverp = connp->serverp;
		if (tserverp->states & SR_TSR) {
		    code = CM_ERR_TRANS_CLIENTTSR;
		    continue;
		}
		icl_Trace2(cm_iclSetp, CM_TRACE_GETTOKENSTART,
			   ICL_TYPE_POINTER, scp,
			   ICL_TYPE_LONG, AFS_hgetlo(tokenp->type));
		startTime = osi_Time();
		st = BOMB_EXEC
		    ("COMM_FAILURE",
		     (osi_RestorePreemption(0),
		      st = AFS_GetToken(connp->connp, &scp->fid, tokenp,
					&osi_hZero, Flags,
					&lhp->realToken, afsBlockerp,
					&lhp->OutStatus, &lhp->tsync),
		      osi_PreemptionOff(),
		      st));
		if ((AFS_hgetlo(tokenp->type)
		    & (TKN_LOCK_READ | TKN_LOCK_WRITE | 
		       TKN_OPEN_READ | TKN_OPEN_WRITE |
		       TKN_OPEN_SHARED | TKN_OPEN_EXCLUSIVE |
		       TKN_OPEN_DELETE))
		    && (st == TKM_ERROR_TOKENCONFLICT)) {
		    /* since TOKENCONFLICT may be fileset busy */
		    icl_Trace0(cm_iclSetp, CM_TRACE_TOKENERRORMAP);
		    st = TKM_ERROR_HARDCONFLICT;
		}
		code = osi_errDecode(st);
		tserverp = connp->serverp;
                DFS_ADJUST_LOCK_EXT(afsBlockerp, tserverp);
		cm_SetReqServer(rreqp, tserverp);
		icl_Trace1(cm_iclSetp, CM_TRACE_GETTOKENEND,
			   ICL_TYPE_LONG, code);
	    }
	    else
		code = -1;
	    if (code == 0) {
		/*
		 * Make sure VL_NEEDRPC is off, since an RPC was made 
		 * on this volume.  cm_NeedRPC will do that.
		 */
		(void)cm_NeedRPC(volp);
		/* Loop back if this call went backwards in VV-time */
		code = cm_CheckVolSync(&scp->fid, &lhp->tsync, volp,
#ifdef SGIMIPS
				       startTime, (int) srvix, rreqp);
#else  /* SGIMIPS */
				       startTime, srvix, rreqp);
#endif /* SGIMIPS */
	    }
	 } while (cm_Analyze(connp, code, &scp->fid, rreqp, volp, srvix,
			     startTime));
	 /*
	  * Start with a write lock, so that we can merge stuff in safely, and 
	  * w/o giving up vnode lock between cm_EndTokenGrantingCall and 
	  * cm_MergeStatus. Note, that we try to use just a read lock, if 
	  * setLock==READ_LOCK, sohat we never need more than a read lock to 
	  * check for tokens that are already here (increases concurrency).
	  */
	 lock_ObtainWrite(&scp->llock);
	 if (code) {
	     if (code == TKM_ERROR_REQUESTQUEUED) {
		 /* if we got a queued token, we may have actually raced
		  * with the async grant and/or the revoke of the async-granted
		  * token.  We check for both possibilities before queuing
		  * the token.
		  */
		 tokenp->tokenID = lhp->realToken.tokenID;
		 /* look for async-granted token arriving before we process
		  * gettoken call.  ScanAsyncGrantingCall returns 0 if
		  * succeeds, otherwise 1.
		  */
		 lhp->grantRaceCode = cm_ScanAsyncGrantingCall(&lhp->realToken,
							       tserverp,
							       scp->volp);

		 /* token could be both granted and (partially) revoked! */
		 cm_EndTokenGrantingCall(&lhp->realToken, tserverp,
					 scp->volp, startTime);

		 /* set appropriate flags on async grant tokens.  Last
		  * parm to AddToken is 1 if the token grant was queued
		  * at the server, otherwise 0.  Matches semantics
		  * of grantRaceCode.
		  */
		 cm_AddToken(scp, &lhp->realToken,
			     tserverp, lhp->grantRaceCode);
	     }
	     else
		 cm_EndTokenGrantingCall(&lhp->realToken, tserverp,
					 scp->volp, startTime);
	     if (setLock == READ_LOCK) {
		 lock_ConvertWToR(&scp->llock);
	     }
	     if (lhp->reallyOK & CM_REALLYOK_REPRECHECK) {
		 /* we really had the tokens, but this RPC failed.  Only
		  * set if a replicated file really is good enough, but we
		  * wanted to make an RPC to try to make things better
		  * anyway.
		  */
		 continue;
	     }
	     break; /* get out of the big while loop */
	 }
	 cm_EndTokenGrantingCall(&lhp->realToken,
				 tserverp, scp->volp, startTime);
	 tokenStat.GetToken++;
	 cm_MergeStatus(scp, &lhp->OutStatus, &lhp->realToken, 0, rreqp);
	 /* copy out the new fields */
	 tokenp->tokenID = lhp->realToken.tokenID;
	 tokenp->expirationTime = lhp->realToken.expirationTime;
	 if (setLock == READ_LOCK) {     /* convert back down */
	     lock_ConvertWToR(&scp->llock);
	 }

      } else {/* Don't HaveHereToken */
	 /* 
	  * If we get here, we DON'T have a HERE token for this fileset for
	  * different reasons.  Find the reason why.
	  */
	 if (setLock == READ_LOCK)
	    lock_ReleaseRead(&scp->llock);
	 else
	    lock_ReleaseWrite(&scp->llock);

	 lock_ObtainRead(&volp->lock);

	 /* Not because of someone is doing TSR-MOVE ... */
	 if ((volp->states & VL_RESTORETOKENS) == 0) {
	     if ((volp->hereServerp == 0) || 
		 cm_SameHereServer(volp->hereServerp, volp)) {
		 /* We either don't have one or it is expired */
		 lock_ReleaseRead(&volp->lock);
		 code = cm_GetHereToken(volp, rreqp, Flags, CM_GETTOKEN_DEF);
	     } 
	     else {
		 /* 
		  * Fileset is moved and the CM must have missed the HERE token
		  * revocation signal. Treat it as a CRASHMOVE case. 
		  */
		 /* It may be that we just don't have the HERE revocation yet,
		  * and that the target is being held for us.
		  * Thus, just don't call it TSR_KNOWCRASH.
		  */
		 lock_ReleaseRead(&volp->lock);
		 code = cm_RestoreMoveTokens(volp, volp->hereServerp, rreqp, 
					     TSR_CRASHMOVE);
	     }
	 } else {
	     /* 
	      * Some one is doing the TSR-move work now. Just wait here! 
	      */
	     while (volp->states & VL_RESTORETOKENS) {
		 osi_SleepR((opaque)(&volp->states), &volp->lock);
		 lock_ObtainRead(&volp->lock);
	     }
	     lock_ReleaseRead(&volp->lock);
	     code = 0;
	 }
	 
	 if (setLock == READ_LOCK)
	     lock_ObtainRead(&scp->llock);
	 else
	     lock_ObtainWrite(&scp->llock);
	 if (code)
	    break;
      } /* Dont' have token */

   } /* for (;;) */

   if (heldVolp)
       cm_EndVolumeRPC(heldVolp);

   osi_FreeBufferSpace((struct osi_buffer *)lhp);
   return code;
}

/* This function gets a status token and also waits for the appropriate
 * states bits to clear.
 * 
 * Returns with vnode locked iff error code is 0, otherwise returns
 * without any locks.  Lock is held in "alock" mode.
 *
 * Basic algorithm is obtain lock; check conditions, if fine,
 * return, otherwise release locks, try to fix the first problem that
 * came up, reobtain lock and recheck from the top.
 *
 * ascp: CM vnode to give tokens to
 * atokens: tokens required
 * aflags: CM_SLOCK_WRITE waits until there are no updates of any kind
 * 		happening to the file, including store backs
 * aflags: CM_SLOCK_WRDATAONLY waits until no store backs of whole file
 *		data, but allows pure data storebacks (of other chunks)
 * aflags: CM_SLOCK_READ doesn't wait
 * alock: lock mode for lock_Obtain
 * areqp: cm_rrequest structure for RPC calls
 */
#ifdef SGIMIPS
int
cm_GetSLock(
    register struct cm_scache *ascp,
    long atokens,
    long aflags,
    int alock,
    struct cm_rrequest *areqp)
#else  /* SGIMIPS */
cm_GetSLock(ascp, atokens, aflags, alock, areqp)
    register struct cm_scache *ascp;
    long atokens;
    long aflags;
    int alock;
    struct cm_rrequest *areqp;
#endif /* SGIMIPS */
{
    struct lclHeap {
	afs_token_t ttoken;
	afs_recordLock_t afsBlocker;
    } *lhp = NULL;
    register long code;
    int reallyOK;

    /* Keep stack frame small by doing heap-allocation */
    lhp = (struct lclHeap *)osi_AllocBufferSpace();
    for (;;) {
	/* obtain lock; if check works, we'll then be done */
	lock_Obtain(&ascp->llock, alock);

	if (code = ascp->asyncStatus) {
	    lock_Release(&ascp->llock, alock);
	    icl_Trace3(cm_iclSetp, CM_TRACE_GETSLOCK_ASYNC,
		       ICL_TYPE_POINTER, ascp,
		       ICL_TYPE_LONG, atokens,
		       ICL_TYPE_LONG, ascp->asyncStatus);
	    goto out;
	}
	/* now check that the required flags are 0; if not,
	 * sleep waiting for them to clear.
	 */
	if (((aflags != CM_SLOCK_READ) && (ascp->states & SC_STORING))
	    || ((aflags == CM_SLOCK_WRITE) && (ascp->storeCount > 0))) {
	    ascp->states |= SC_WAITING;
	    /* wait for clear */
	    osi_Sleep2(&ascp->states, &ascp->llock, alock);
	    continue;	/* try again */
	}
	AFS_hset64(lhp->ttoken.type, 0, atokens);
	AFS_hzero(lhp->ttoken.beginRange);
	lhp->ttoken.endRange = osi_hMaxint64;
	if (!cm_HaveTokensRange(ascp, &lhp->ttoken, 0, &reallyOK)) {
	    /* to avoid busy waiting, we have to wait until tokens
	     * get here.  cm_GetTokens is called with a lock held
	     * (in mode alock) and returns, in all cases, with
	     * the same lock held.
	     */
	    code = cm_GetTokensRange(ascp, &lhp->ttoken, areqp, 0,
				     &lhp->afsBlocker, alock);
	    /* need to recheck, so go back and retry */
	    lock_Release(&ascp->llock, alock);
	    
	    /* if something went wrong, we fail, returning sans lock */
	    if (code && !(reallyOK & CM_REALLYOK_REPRECHECK)) {
		icl_Trace3(cm_iclSetp, CM_TRACE_GETSLOCK_TOKRANGE,
			   ICL_TYPE_POINTER, ascp,
			   ICL_TYPE_LONG, atokens,
			   ICL_TYPE_LONG, code);
		goto out;
	    }
	    continue;
	}

	/* otherwise we're done, so just return, holding the lock */
	code = 0;
	break;
    }
out:
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}

/* Get open tokens for a file.  Note that we have to handle the open
 * tokens separately from the status tokens, since we want to handle
 * refusal to get the desired tokens differently.
 *
 * The atokens parameter is a long describing the *open* tokens (alone)
 * that we're trying to get.  This function adds in a status/read token
 * which we'll also need for open syscall processing.
 *
 * Called with an unlocked scache entry, returns with a write-locked
 * entry if successful, otherwise leaves entry unlocked.
 *
 * Returns ETXTBSY if this open call should fail with text file busy.
 */
#ifdef SGIMIPS
int
cm_GetOLock(
  register struct cm_scache *scp,
  long atokens,
  struct cm_rrequest *areqp)
#else  /* SGIMIPS */
cm_GetOLock(scp, atokens, areqp)
  register struct cm_scache *scp;
  long atokens;
  struct cm_rrequest *areqp;
#endif /* SGIMIPS */
{
    afs_token_t ttoken;
    long opentokens, othertokens;
    long code;
    int reallyOK;

    AFS_hset64(ttoken.type, 0, atokens | TKN_STATUS_READ);
    /* Split the total into open tokens and everything else: */
    /* Sometimes atokens can have tokens other than open ones. */
    opentokens = AFS_hgetlo(ttoken.type) &
	(TKN_OPEN_READ | TKN_OPEN_WRITE
	 | TKN_OPEN_SHARED | TKN_OPEN_EXCLUSIVE
	 | TKN_OPEN_PRESERVE | TKN_OPEN_DELETE);
    othertokens = AFS_hgetlo(ttoken.type) & ~opentokens;
    /* byte range is meaningless for these types of tokens */

    lock_ObtainWrite(&scp->llock);
    for (;;) {
	if (cm_HaveTokensRange(scp, &ttoken, 0, &reallyOK))
	    return 0;

	/* otherwise, get both types of tokens, and retry.  Start with
	 * the status read token.
	 */
	code = cm_GetTokens(scp, othertokens, areqp, WRITE_LOCK);
	if (code && (reallyOK & CM_REALLYOK_REPRECHECK))
	    continue;
	if (code) {
	    break;
	}
	code = cm_GetTokens(scp, opentokens, areqp, WRITE_LOCK);
	if (code && (reallyOK & CM_REALLYOK_REPRECHECK))
	    continue;
	if (code) {
	    /* if we make it here, either something serious went wrong,
	     * or we just have ETXTBSY to return.
	     */
	    if (code == TKM_ERROR_HARDCONFLICT)
		code = ETXTBSY;
	    break;
	}

	/* if we make here, nothing went wrong, and we should have the
	 * desired tokens.  However, when we got the open tokens, we may
	 * have lost the status/read tokens, so go back around and reverify.
	 */
    }
    /* got here only on error, so unlock things and
     * return the error code
     */
    lock_ReleaseWrite(&scp->llock);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}


/* Get a dcache entry in the state we require.  Involves getting
 * appropriate tokens, waiting for appropriate flags to clear,
 * triggering fetches of data, etc.
 *
 * scp		is locked scache entry
 * dcp		is unlocked dcache entry
 * aflags	indicates whether we're reading or writing data.  Results
 *		indicate whether tokens are update tokens, whether
 *		fetching and storing can be set, etc.  A bit mask:
 *		CM_DLOCK_READ	fetching flags clear
 *		CM_DLOCK_WRITE	fetching and storing flags clear
 *		CM_DLOCK_FETCHOK	storing clear, fetching OK
 *			(used for read-while-fetching)
 * alock	the type of lock to obtain on the dcache entry
 * areqp	the request block
 *
 * if returns 0, locks the dcache entry
 *
 * the scache entry is locked when this function is called, and the same
 * lock is held on return, no matter what the error code.  The lock is
 * required so that tokens don't disappear after return.  Note that
 * status fetching/storing flags may change while in here, though.
 * This shouldn't be a problem for those who call this function.
 *
 * N.B. DO NOT CALL THIS FUNCTION WITH AN SLOCK'ed VNODE: IT WILL
 * TREAT IT AS A WRITE_LOCKED VNODE (lock_Check's return code will
 * be interpreted incorrectly).
 */
#ifdef SGIMIPS
int 
cm_GetDLock(
  register struct cm_scache *scp,
  register struct cm_dcache *dcp,
  int aflags,
  int alock,
  struct cm_rrequest *areqp)
#else  /* SGIMIPS */
cm_GetDLock(scp, dcp, aflags, alock, areqp)
  register struct cm_scache *scp;
  register struct cm_dcache *dcp;
  int aflags;
  int alock;
  struct cm_rrequest *areqp;
#endif /* SGIMIPS */
{
    int scacheLock;
    register long code;
    afs_token_t token;
#ifdef SGIMIPS
    unsigned long once;

    once = 0;
#endif /* SGIMIPS */

    if (lock_Check(&scp->llock) < 0)
	scacheLock = WRITE_LOCK;
    else
	scacheLock = READ_LOCK;
#ifndef SGIMIPS
    AFS_hset64(token.beginRange, 0, cm_chunktobase(dcp->f.chunk));
    token.endRange = token.beginRange;
    AFS_hadd32(token.endRange, cm_chunktosize(dcp->f.chunk) - 1);
#endif /* SGIMIPS */
    for (;;) {
	/* if we have a status store going on, might be a potential
	 * truncate, so we should wait for it to complete.
	 */
	if (aflags == CM_DLOCK_WRITE) {
	    if (scp->states & SC_STORING) {
		scp->states |= SC_WAITING;
		osi_Sleep2(&scp->states, &scp->llock, scacheLock);
		lock_Obtain(&scp->llock, scacheLock);
		continue;
	    }
	}

	lock_Obtain(&dcp->llock, alock);
	if (aflags == CM_DLOCK_WRITE) {
	    /* if we're writing, we need to wait for fetching and
	     * storing to clear on the dcache entry.  Otherwise,
	     * we can live with either fetching or storing being
	     * set.
	     */
	    if (dcp->states & (DC_DFFETCHING|DC_DFSTORING)) {
		/* wait for flags to clear */
		dcp->states |= DC_DFWAITING;
		/* must release dcp->llock last to avoid missing wakeup */
		lock_Release(&scp->llock, scacheLock);
		osi_Sleep2(&dcp->states, &dcp->llock, alock);
		lock_Obtain(&scp->llock, scacheLock);
		continue;
	    }
	    AFS_hset64(token.type, 0, TKN_UPDATE);
	}
	else { /* read request */
	    AFS_hset64(token.type, 0, TKN_READ);
	    if (aflags == CM_DLOCK_READ) {	/* not CM_DLOCK_FETCHOK */
		/* wait for fetching to clear */
		if (dcp->states & DC_DFFETCHING) {
		    dcp->states |= DC_DFWAITING;
		    lock_Release(&scp->llock, scacheLock);
		    osi_Sleep2(&dcp->states, &dcp->llock, alock);
		    lock_Obtain(&scp->llock, scacheLock);
		    continue;
		}
	    }
	}

	/* next, make sure this cache entry is online, with the appropriate
	 * tokens.  If not,get appropriate token (TKN_READ or TKN_UPDATE)
	 * and proceed.
	 */
#ifdef SGIMIPS
	if (!once) {
	    token.beginRange =  cm_chunktobase(dcp->f.chunk);
    	    token.endRange=token.beginRange + cm_chunktosize(dcp->f.chunk) - 1;
	    once = 1;
	}
#endif /* SGIMIPS */
	if (cm_HaveTokensRange(scp, &token, 0, NULL)
	    && (dcp->f.states & DC_DONLINE)) return 0;

	/* otherwise, we're unhappy, and have to try to get the
	 * data online.
	 */
	lock_Release(&scp->llock, scacheLock);
	lock_Release(&dcp->llock, alock);
	code = cm_GetDOnLine(scp, dcp, &token, areqp);
	lock_Obtain(&scp->llock, scacheLock);
	if (code) {
	    icl_Trace4(cm_iclSetp, CM_TRACE_GETDLOCK_ONLINE,
		       ICL_TYPE_POINTER, scp,
		       ICL_TYPE_POINTER, dcp,
		       ICL_TYPE_HYPER, &token.type,
		       ICL_TYPE_LONG, code);
#ifdef SGIMIPS
	    return (int)code;
#else  /* SGIMIPS */
	    return code;
#endif /* SGIMIPS */
	}
    }
    /* NOTREACHED */
}

/*
 * Called with the same parameters as cm_GetDLock, but only verifies
 * the same conditions.  Also, alock parameter is gone, since we
 * already have the write-lock held.
 *
 * Called with scp and dcp at least read locked (differs from GetDLock).
 *
 * Returns 0 if things are fine (tokens here, chunk online, states happy),
 * otherwise returns non-zero.
 */
#ifdef SGIMIPS
int cm_TryDLock(
  register struct cm_scache *scp,
  register struct cm_dcache *dcp,
  int aflags)
#else  /* SGIMIPS */
int cm_TryDLock(scp, dcp, aflags)
  register struct cm_scache *scp;
  register struct cm_dcache *dcp;
  int aflags;
#endif /* SGIMIPS */
{
    afs_token_t token;

#ifdef SGIMIPS
    token.beginRange =  cm_chunktobase(dcp->f.chunk);
    token.endRange =  token.beginRange + cm_chunktosize(dcp->f.chunk) - 1;
#else  /* SGIMIPS */
    AFS_hset64(token.beginRange, 0, cm_chunktobase(dcp->f.chunk));
    token.endRange = token.beginRange;
    AFS_hadd32(token.endRange, cm_chunktosize(dcp->f.chunk) - 1);
#endif /* SGIMIPS */
    /* if we have a status store going on, might be a potential
     * truncate, so we should wait for it to complete.
     */
    if (scp->states & SC_STORING) {
	if (aflags == CM_DLOCK_WRITE) return 1;
    }
    
    if (aflags == CM_DLOCK_WRITE) {
	/* if we're writing, we need to wait for fetching and
	 * storing to clear on the dcache entry.  Otherwise,
	 * we can live with either fetching or storing being
	 * set.
	 */
	if (dcp->states & (DC_DFFETCHING|DC_DFSTORING))
	    return 1;
	AFS_hset64(token.type, 0, TKN_UPDATE);
    }
    else { /* read request */
	AFS_hset64(token.type, 0, TKN_READ);
	if (aflags == CM_DLOCK_READ) {	/* not CM_DLOCK_FETCHOK */
	    /* wait for fetching to clear */
	    if (dcp->states & DC_DFFETCHING)
		return 1;
	}
    }
    
    /* next, make sure this cache entry is online, with the appropriate
     * tokens.  If not,get appropriate token (TKN_READ or TKN_UPDATE)
     * and proceed.
     */
    if (cm_HaveTokensRange(scp, &token, 0, NULL)
	&& (dcp->f.states & DC_DONLINE)) return 0;
    
    /* otherwise, we're unhappy, and have to try to get the
     * data online.
     */
    return 1;
}

/* 
 * Add a token to the cm_scache entry's token list. Must be called under 
 * a scp llock.
 * If called with queued set, then this is an async grant token,
 * and we create a dummy entry with the right token ID and byte range
 * (so that it is properly sorted), but with 0 rights and expiration
 * time.
 */
#ifdef SGIMIPS
void cm_AddToken(
  register struct cm_scache *scp,
  afs_token_t *atp,
  struct cm_server *asp,
  int queued)
#else  /* SGIMIPS */
void cm_AddToken(scp, atp, asp, queued)
  register struct cm_scache *scp;
  afs_token_t *atp;
  struct cm_server *asp;
  int queued;
#endif /* SGIMIPS */
{
    register struct cm_tokenList *tlp, *newlp;
    struct cm_tokenList *beforelp, *nlp;
    afsFid *fidp = &scp->fid;

    /* 
     * Now merge the token in, if it is new.  Note that we sort the
     * token in so that tokens are in order by beginRange.  This is
     * required for the sys v locking code to work properly, since it
     * is required in order to determine whether a lock is covered by
     * the available tokens in an efficient manner.  Also note that
     * if a token is inserted twice, it will have the same beginRange
     * field.  Since the code below looks at all tokens with the same
     * beginRange field, it should find any token entry with the same
     * tokenID field.
     *
     * Finally, note that we move tokens obsoleted by the new token
     * to the server's return list, which will be returned 3 minutes
     * later by the background daemon.
     */
    beforelp = 0;
    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *) &scp->tokenList;
	 tlp = nlp) {
	nlp = (struct cm_tokenList *) tlp->q.next;

	/* don't return deleted tokens again, and don't return
	 * tokens still waiting to be granted.
	 */
	if (!(tlp->flags & CM_TOKENLIST_DELETED) && !queued) {
	    /* see if we can remove this entry, because it is
	     * already covered by the new token.  Async tokens
	     * never cover anything until they're granted,
	     * nor are they returned this way.
	     */
	    if ((AFS_hgetlo(tlp->token.type) & ~AFS_hgetlo(atp->type)) == 0
		&& tlp->refCount == 0	/* don't screw up list */
		&& !(tlp->flags & CM_TOKENLIST_ASYNC)
		&& AFS_hcmp(atp->beginRange, tlp->token.beginRange) <= 0
		&& AFS_hcmp(atp->endRange, tlp->token.endRange) >= 0) {
		QRemove(&tlp->q);
		if (AFS_hgetlo(tlp->token.type) & TKN_DATA_READ)
		    cm_UpdateDCacheOnLineState(scp, &tlp->token, atp);
		/* if the server is null, this is (probably) a freely-granted
		 * token (from a read-only server).
		 */
		cm_QuickQueueAToken(tlp->serverp, tlp);
		/* finally, continue, since this guy is no longer
		 * a candidate for insertion before, since he's in
		 * the server's return list.
		 */
		continue;
	    }
	}

	/* determine if this is where we should insert the new token */
	if (!beforelp && AFS_hcmp(atp->beginRange, tlp->token.beginRange) < 0)
	    beforelp = tlp;
    }
    /* if we didn't find a candidate, return insert at end, 'cause
     * all are earlier.
     */
    if (!beforelp)
	beforelp = (struct cm_tokenList *) &scp->tokenList;
    newlp = cm_AllocTokenEntry();	/* returns zeroed structure */
#ifdef CM_ENABLE_COUNTERS
    if (AFS_hgetlo(atp->type) & (TKN_STATUS_READ | TKN_STATUS_WRITE)) {
	CM_BUMP_COUNTER(statusTokensAcquired);
    }
    if (AFS_hgetlo(atp->type) & (TKN_DATA_READ | TKN_DATA_WRITE)) {
	CM_BUMP_COUNTER(dataTokensAcquired);
    }
#endif /* CM_ENABLE_COUNTERS */
    QAddT((struct squeue *) beforelp, &newlp->q);
    if (queued) {
	/* just set these fields; the rest should stay 0 since we
	 * don't have type or expiration bits until token is granted.
	 */
	newlp->token.tokenID = atp->tokenID;
	newlp->token.beginRange = atp->beginRange;
	newlp->token.endRange = atp->endRange;
	newlp->flags |= CM_TOKENLIST_ASYNC;
    }
    else newlp->token = *atp;
    newlp->tokenFid.Volume = fidp->Volume;
    newlp->tokenFid.Vnode = fidp->Vnode;
    newlp->tokenFid.Unique = fidp->Unique;
    if (tkm_IsGrantFree(atp->tokenID))
	newlp->serverp = (struct cm_server *) 0;
    else {
	newlp->serverp = asp;
	lock_ObtainWrite(&asp->lock);
	asp->states |= SR_HASTOKENS;
	lock_ReleaseWrite(&asp->lock);
    }
}

/* 
 * Remove a particular token ID from the list of token IDs associated with a
 * vnode.  The vnode must be already write-locked by the caller.
 */
#ifdef SGIMIPS
void cm_RemoveToken(
    register struct cm_scache *scp,
    struct cm_tokenList *atp,
    register afs_hyper_t *atokenType) 
#else  /* SGIMIPS */
void cm_RemoveToken(scp, atp, atokenType)
    register struct cm_scache *scp;
    struct cm_tokenList *atp;
    register afs_hyper_t *atokenType; 
#endif /* SGIMIPS */
{
    /* 
     * found the token ID of interest 
     */
    AFS_HOP(atp->token.type, &~, *atokenType); /* remove these types */
    /* 
     * Free up if we're all done with this entry 
     */
    if (AFS_hiszero(atp->token.type)) 
	cm_DelToken(atp);
}


/* 
 * Return all tokens of a particular type for a particular vnode.  This
 * function should be called without any locks held, on either the vnode
 * or the cm_qtknlock, since it may call AFS_ReleaseTokens, which may make
 * an RPC back to us for many different reasons.
 */
#ifdef SGIMIPS
void cm_ReturnOpenTokens(
     register struct cm_scache *scp,
     afs_hyper_t *tokenTypep)
#else  /* SGIMIPS */
void cm_ReturnOpenTokens(scp, tokenTypep)
     register struct cm_scache *scp;
     afs_hyper_t *tokenTypep;
#endif /* SGIMIPS */
{
    register struct cm_server *serverp;
    struct cm_tokenList tokenl;	/* temp for return call */
    register struct cm_tokenList *tlp, *nlp;
    int clearReturnRef;

    /* serverp tracks last server tokens queued for */
    serverp = (struct cm_server *) 0;

    /* Clear SC_RETURNREF only if we're asked to revoke O_P tokens. */
    clearReturnRef = (AFS_hgetlo(*tokenTypep) & TKN_OPEN_PRESERVE);

    /* 
     * Next call QueueAToken to put the request in the queue for the
     * server.  Do this for all tokens in the file's token list.
     */
    lock_ObtainWrite(&scp->llock);

    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *) &scp->tokenList; tlp = nlp) {

	/* compute this first, as a default */
	nlp = (struct cm_tokenList *) tlp->q.next;

	if (tlp->flags & CM_TOKENLIST_DELETED) /* this one already deleted */
	   continue;

	if (AFS_hgetlo(tlp->token.type) & AFS_hgetlo(*tokenTypep)) {
	    /* Must obtain cm_scachelock here if we are returning
	     * TKN_OPEN_PRESERVE.  The other token types don't need this,
	     * but it's harmless.
	     */
	    lock_ObtainWrite(&cm_scachelock);
	    /* 
	     * See if we're allowed to revoke the particular token bits 
	     */
	    AFS_hzero(tokenl.token.type);
	    if ((AFS_hgetlo(tlp->token.type) & TKN_OPEN_READ) &&
		scp->readers == 0)
		AFS_HOP32(tokenl.token.type, |, TKN_OPEN_READ);
	    if ((AFS_hgetlo(tlp->token.type) & TKN_OPEN_WRITE) &&
		scp->writers == 0)
		AFS_HOP32(tokenl.token.type, |, TKN_OPEN_WRITE);
	    if ((AFS_hgetlo(tlp->token.type) & TKN_OPEN_SHARED) &&
		scp->shareds == 0)
		AFS_HOP32(tokenl.token.type, |, TKN_OPEN_SHARED);
	    if ((AFS_hgetlo(tlp->token.type) & TKN_OPEN_EXCLUSIVE) &&
		scp->exclusives == 0)
		AFS_HOP32(tokenl.token.type, |, TKN_OPEN_EXCLUSIVE);
	    if ((AFS_hgetlo(tlp->token.type) & TKN_OPEN_PRESERVE) &&
		(AFS_hgetlo(*tokenTypep) & TKN_OPEN_PRESERVE)) {
		/* Found a TKN_OPEN_PRESERVE token */
		/* Maybe we'll be able to return it. */
		if ((scp->states & SC_RETURNREF) &&
#ifdef SGIMIPS
		    ((CM_RC(scp) == 2) ||
		     ((CM_RC(scp) == 3) && (scp->states & SC_CELLROOT)))) {
#else  /* SGIMIPS */
		    ((scp->v.v_count == 1) ||
		     ((scp->v.v_count == 2) && (scp->states & SC_CELLROOT)))) {
#endif /* SGIMIPS */
		    AFS_HOP32(tokenl.token.type, |, TKN_OPEN_PRESERVE);
		} else {
		    /* We can't return this!  Either SC_RETURNREF is already
		     * off, or the ref count isn't the base value.
		     * In either case, we can't clear SC_RETURNREF since
		     * we're not returning all O_P tokens. */
		    clearReturnRef = 0;
		}
	    }
	    if (AFS_hiszero(tokenl.token.type)) {
		/*
		 * Release this lock, but do nothing else since
		 * there are no types to really return
		 */
		lock_ReleaseWrite(&cm_scachelock);
	    } else {
		/* 
		 * This token shares at least some bits with the types we're 
		 * trying to revoke.  Prepare to call QueueAToken on this guy.
		 */
		tokenl.tokenFid = tlp->tokenFid;
		tokenl.token.tokenID = tlp->token.tokenID;
		tokenl.token.expirationTime = 0x7fffffff;
		/* 
		 * Queue the token 
		 */
		cm_HoldToken(tlp);
		cm_RemoveToken(scp, tlp, &tokenl.token.type);
		lock_ReleaseWrite(&cm_scachelock);
		lock_ReleaseWrite(&scp->llock);
		if (serverp && tlp->serverp != serverp)
		    cm_FlushQueuedServerTokens(serverp);
		serverp = tlp->serverp;
		cm_QueueAToken(serverp, &tokenl, 1); /* null is OK */

		lock_ObtainWrite(&scp->llock);
		/* recompute nlp since we dropped the vnode lock */
		nlp = (struct cm_tokenList *) tlp->q.next;
		cm_ReleToken(tlp);
	    }
	}
    }

    /*
     * If we were called because SC_RETURNREF was on, but there was no longer
     * a TKN_OPEN_PRESERVE token to return, we should still clear SC_RETURNREF,
     * so as to avoid getting called again.
     * Additionally, we're allowed to clear SC_RETURNREF *only* if all
     * TKN_OPEN_PRESERVE tokens have been returned, so we delay clearing
     * SC_RETURNREF until we've stepped through all the tokens for this vnode.
     */
    if (clearReturnRef)
#ifdef SGIMIPS
	scp->states &= ~(SC_RETURNREF | SC_STARTED_RETURNREF);
#else  /* SGIMIPS */
	scp->states &= ~SC_RETURNREF;
#endif /* SGIMIPS */

    lock_ReleaseWrite(&scp->llock);

    /* 
     * Finally, we should call the flush function, to make sure
     * this stuff is returned promptly.
     */
    if (serverp)
	cm_FlushQueuedServerTokens(serverp);
}

int cm_HaveTokens(struct cm_scache *scp, long types)
{
    afs_token_t token;

    AFS_hset64(token.type, 0, types);
    AFS_hzero(token.beginRange);
    token.endRange = osi_hMaxint64;
    return cm_HaveTokensRange(scp, &token, 0, NULL);
}

/*
 * Check to see if this scache has caller's desired token or not. 
 * If the "exactMatch" flag is on, search only for one particular token that 
 * would match the desired token type. If found, return TRUE , along with the
 * token ID to the caller. 
 * If the flag is not on, return TRUE, as long as the combination of all 
 * tokens of this scache would satisfy the request. 
 * This call assumes at least a read lock on the llock structure.
 */
int cm_HaveTokensRange(
  struct cm_scache *scp,
  afs_token_t *tokens,
  int exactMatch,
  int *reallyOKp)
{
    register struct cm_tokenList *tlp;
    register long stillNeeded, now;
    struct cm_server *serverp, *badServerp;
    register struct cm_volume *volp = scp->volp;
    int repCode;

    /* initialize the OUT parameter */
    if (reallyOKp)
	*reallyOKp = 0;

    /* Make sure we have a HERE token first */
    if (volp == 0) 
	return 0; /* fail */

    lock_ObtainRead(&volp->lock);
    if ((volp->states & VL_RESTORETOKENS) || 
	!cm_SameHereServer(volp->hereServerp, volp) ||
	(volp->hereToken.expirationTime <= osi_Time())) {
	lock_ReleaseRead(&volp->lock);
	return 0; /* fail */
    }
    lock_ReleaseRead(&volp->lock);

    stillNeeded = AFS_hgetlo(tokens->type); /* caller's requested token */
    now = osi_Time();			/* time we're running */
    /* Check whether replica correctness allows us to use the cached info. */
    repCode = 0;
    if (scp->states & SC_RO) {	/* a read-only scache entry */
	/* Catch cm checkfileset calls.  It's ok to check w/o lock. */
	if (volp->states & VL_NEEDRPC) {
	    return 0;
	}
	lock_ObtainRead(&volp->lock);
	/* on a replica? (volume installation checks for consistency) */
	if (volp->states & VL_LAZYREP) {
	    /* OK: this is replicated data.  Decide if it's usable. */
	    /* Case (a): all OK with this data. */
	    /*     - have looked recently and it's within maxTotal. */
	    /* Case (b): could use this data, but need to check first. */
	    /*      - have looked recently; beyond maxTotal, but still in hardMax. */
	    /*         = occasionally, call this data unusable. */
	    /* Case (c): data is clearly unusable. */
	    /*        - haven't ever looked, or not since hardMaxTotal. */
	    if (volp->timeWhenVVCurrent == 0
		|| ((volp->timeWhenVVCurrent+volp->hardMaxTotalLatency)
		    <= now)) {
		repCode = 2;	/* case (c): unusable */
		icl_Trace3(cm_iclSetp, CM_TRACE_HAVETOKHARDREPCHECK,
			   ICL_TYPE_LONG, AFS_hgetlo(volp->volume),
			   ICL_TYPE_LONG, volp->timeWhenVVCurrent,
			   ICL_TYPE_LONG, volp->hardMaxTotalLatency);
	    } else if ((volp->timeWhenVVCurrent+volp->maxTotalLatency) <= now) {
		/*
		 * Case (b).
		 * We're in the gray interval (after suggested target of
		 * maxTotalLatency, but before the time we need to produce
		 * hard failures of hardMaxTotalLatency).  We occasionally
		 * indicate the need for an update by returning a failure here,
		 * but we bound the frequency with which that happens by the
		 * repTryInterval.
		 */
		if ((volp->lastRepTry + volp->repTryInterval) < now) {
		    /* Time for next retry. */
		    repCode = 1;
		    icl_Trace4(cm_iclSetp, CM_TRACE_HAVETOKGRAYREPCHECK,
			       ICL_TYPE_LONG, AFS_hgetlo(volp->volume),
			       ICL_TYPE_LONG, volp->timeWhenVVCurrent,
			       ICL_TYPE_LONG, volp->lastRepTry,
			       ICL_TYPE_LONG, volp->repTryInterval);
		} /* else let this case go, as we've made a recent attempt on it. */
	    }	/* else case (a): repCode remains 0 */
	}
	if (repCode == 2) { /* MUST do an RPC */
	    lock_ReleaseRead(&volp->lock);
	    return 0;	/* indicate failure to caller */
	}
	lock_ReleaseRead(&volp->lock);
	/* For cases 0 and 1, there may be other reasons this won't work. */
    }
    serverp = (struct cm_server *) 0;	/* last server locked */
    badServerp = (struct cm_server *) 0;	/* last server known bad */
    /* look for appropriate tokens.  If exactMatch is false, we use
     * stillNeeded to track which tokens are still required.  If
     * we're doing an exactMatch search, we just use it as a flag
     * telling if we found all the tokens we're looking for or not.
     * Note that if it is zero to start with, we're not looking
     * for any tokens, so we can return success anyway.
     */
    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *) &scp->tokenList;
	 tlp = (struct cm_tokenList *) tlp->q.next) {

	/* if expired, deleted, or async and not granted, don't count it */
	if ((tlp->flags & (CM_TOKENLIST_DELETED|CM_TOKENLIST_ASYNC)) ||
	    tlp->token.expirationTime < now) {/* Time has been adjusted */
	    continue;		/* about to expire, skip it */
	}

	/* check for token from down server or server in TSR mode */
	if (tlp->serverp && (serverp != tlp->serverp)) {
	    /* check to see if we have wrong server locked */
	    if (serverp) lock_ReleaseRead(&serverp->lock);
	    serverp = tlp->serverp;
	    lock_ObtainRead(&serverp->lock);
	    if (serverp->states & (SR_DOWN|SR_TSR)) {
		badServerp = serverp;
		continue;
	    }
	    /* Also check whether we can be sure about this token's state.
	     * If we haven't gotten any RPCs through to the server for its
	     * hostLifetime, we can't be sure that this token is really ours.
	     * cm_HaveTokensFrom will be false if we have only free tokens
	     * from this server (i.e. all the filesets we've seen from it are
	     * read-only), so we don't care about the lastCall time.
	     */
	    if (cm_HaveTokensFrom(serverp) &&
		(serverp->lastCall + serverp->origHostLifeTime) < now) {
		badServerp = serverp;
		continue;
	    }
	} else if (tlp->serverp && tlp->serverp == badServerp) {
	    /* this server was already known bad, above */
	    continue;
	}

	/* 
	 * remove any whole-file tokens that apply; data tokens are
	 * checked later, after the range is verified.
	 */
	if (!exactMatch)
	    stillNeeded &= ~(AFS_hgetlo(tlp->token.type) & ~CM_RANGE_TOKENS);
        /*
         * Check to see if the range is reasonable. Token *tlp is only
         * useful for our purposes if it has a bigger range than the token for
         * which we're checking.
         */
	if (AFS_hcmp(tokens->beginRange, tlp->token.beginRange) < 0 ||
	    AFS_hcmp(tokens->endRange, tlp->token.endRange) > 0)
	    continue;			/* find someone else */

	if (exactMatch == 1) { 
	    if (AFS_hissubset(tokens->type, tlp->token.type)) {
		/* this exact token matches user's request */ 
		stillNeeded = 0;
		tokens->tokenID = tlp->token.tokenID;
		tokens->expirationTime = tlp->token.expirationTime;
		break;
	    }
	}
	else {
	    stillNeeded &= ~AFS_hgetlo(tlp->token.type);
	    if (stillNeeded == 0) 
		break;
	}
    }
    /* release last-used server */
    if (serverp)
	lock_ReleaseRead(&serverp->lock);
    /* when we get here, stillNeeded is non-zero iff there are tokens
     * we still need to complete this request.
     */
    if (stillNeeded)
	return 0;			/* still more required, fail */
    else {
	int reallyOK = 0;

	/* We have the desired tokens.  However, if we need TKN_STATUS_READ and
         * the SC_LENINVAL bit is on, pretend the token is absent. */
	if ((scp->states & SC_LENINVAL) &&
	    (AFS_hgetlo(tokens->type) & TKN_STATUS_READ))
	    reallyOK |= CM_REALLYOK_LENINVAL;

	/* or unless bounded by replication stuff.  This is a gray case, so we
         * need to do an RPC anyway */
	if (repCode != 0)
	    reallyOK |= CM_REALLYOK_REPRECHECK;

	if (reallyOKp)			/* return this if the caller cares */
	    *reallyOKp = reallyOK;

	if (reallyOK)
	    return 0;
	return 1;			/* else we just win */
    }
}

/*
 * Queue all tokens hanging off of a locked scache entry for return to the file
 * server's token manager.  Grabs qtknlock for a short period.  Generally used
 * when returning a cache entry to the free list. The vnode is write locked.
 *
 * No longer requires zero refcount on vnode, but doesn't guarantee that
 * all tokenList entries are gone from the list when done.  Instead, any
 * entries with non-zero refcounts are left in the list, but a *copy* is
 * queued for returning, and the entry left in the list is marked with
 * no rights remaining, and with the deleted flag (so that it will be
 * deleted when the refcount goes to 0).
 *
 * Of course, if called with a vnode with a refcount of 0, it *will* clean
 * things completely, since no one should have a hold on a tokenList entry
 * w/o holding the containing scache entry.
 */
#ifdef SGIMIPS
void cm_QuickQueueTokens(struct cm_scache *scp) 
#else  /* SGIMIPS */
void cm_QuickQueueTokens(scp)
    struct cm_scache *scp; 
#endif /* SGIMIPS */
{
    register struct cm_tokenList *tp;
    struct cm_tokenList *nextp;
    struct cm_tokenList *newp;
    register struct cm_server *tsp;
    long now;

    now = osi_Time();
    lock_ObtainWrite(&cm_qtknlock);
    tp = (struct cm_tokenList *) scp->tokenList.next;
    tsp = (struct cm_server *) 0;
    for (;;) { 
	/* 
	 * move obsolete tokens from scp to token return list, IF NOT expired
	 */
	if (&scp->tokenList == (struct squeue *)tp )  /* end of list */
	    break;
	nextp = (struct cm_tokenList*)(tp->q.next);	/* save */
	tsp = tp->serverp;	/* server we're dealing with */
	if (tp->refCount > 0) {
	    /* can't free this element, since someone holds a pointer
	     * to it.  Shouldn't happen if the vnode's ref count is 0.
	     * So, leave it in the queue, but remove all rights, and
	     * queue up a new entry with the rights for returning.
	     * If we have a freely granted token (tsp == 0), then we
	     * don't have to do the returning.
	     */
	    if (tsp) {
		newp = cm_AllocTokenEntry();
		newp->token = tp->token;
		newp->tokenFid = tp->tokenFid;
		newp->q.next = (struct squeue *) tsp->returnList;
		tsp->returnList = newp;
		if (++tsp->tokenCount >= SR_NTOKENS)
		    cm_needServerFlush = 1;
	    }
	    /* make this go away as soon as possible, and note that
	     * the tokens are already being returned.
	     */
	    AFS_hzero(tp->token.type);
	    tp->flags |= CM_TOKENLIST_DELETED;
	} else {
	    QRemove((struct squeue *) tp);
	    if (!tsp || (tp->token.expirationTime < now)
		|| AFS_hiszero(tp->token.type)) {
		/* token has expired, or otherwise worthless; just discard it */
		cm_FreeTokenEntry(tp);
	    }
	    else {
		/* queued the token for returning */
		tp->q.next = (struct squeue *) tsp->returnList;
		tsp->returnList = tp;
		if (++tsp->tokenCount >= SR_NTOKENS)
		    cm_needServerFlush = 1;
	    }
	}
	tp = nextp;
    }
    lock_ReleaseWrite(&cm_qtknlock);
}


/* utility to take a tokenList entry and add it to the
 * return queue properly.
 */
#ifdef SGIMIPS
static void cm_QuickQueueAToken(
  register struct cm_server *aserverp,
  register struct cm_tokenList *alistp)
#else  /* SGIMIPS */
static void cm_QuickQueueAToken(aserverp, alistp)
  register struct cm_server *aserverp;
  register struct cm_tokenList *alistp;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_qtknlock);
    if (!aserverp || AFS_hiszero(alistp->token.type)
	|| alistp->token.expirationTime < osi_Time()) {
	cm_FreeTokenEntry(alistp);
    }
    else {
	aserverp->tokenCount++;
	if (aserverp->tokenCount >= SR_NTOKENS)
	    cm_needServerFlush = 1;
	alistp->q.next = (struct squeue *) aserverp->returnList;
	aserverp->returnList = alistp;
    }
    lock_ReleaseWrite(&cm_qtknlock);
}

/*
 * Handle removing obsolete tokens for a server.  Called with no locks held.
 * Could check here for expired token, but we're returning it because
 * someone else wants it, and it is better to be safe and return an expired
 * token, than to make the other client wait until a daemon notices the
 * token has expired.
 */
#ifdef SGIMIPS
void cm_QueueAToken(
    register struct cm_server *serverp,
    struct cm_tokenList *tlp,
    int isRevoking)
#else  /* SGIMIPS */
void cm_QueueAToken(serverp, tlp, isRevoking)
    register struct cm_server *serverp;
    struct cm_tokenList *tlp;
    int isRevoking;
#endif /* SGIMIPS */
{
    struct cm_tokenList *ttp;
    afs_token_t *tokenp = &tlp->token;

    /* read-only fileset */
    if (!serverp) return;

    /*
     * Send expired tokens back only if there might be some process waiting for
     * it--only if it might have been revoked and we don't want the revoker to
     * have to wait around for a timer to expire somewhere.
     */
    if (!isRevoking && (tlp->token.expirationTime < osi_Time()))
	return;

    lock_ObtainWrite(&cm_qtknlock);
    /* 
     * Look for token structure containing the tokenID we're returning.
     */
    for (ttp = serverp->returnList; ttp;
	 ttp = (struct cm_tokenList *) ttp->q.next) {
	if (AFS_hsame(tokenp->tokenID, ttp->token.tokenID)) {
	    /* 
	     * Found the appropriate token structure here 
	     */
	    AFS_HOP(ttp->token.type, |, tokenp->type);
	    break;
	}
    }
    /* do *not* check to see if we're returning tokenType == 0, since
     * we sometimes return the null set of types for a token to force
     * the server's TKM to retry pending tokens with slice and dice
     * action.
     * Check out cm_lockf.c for an example.
     */
    if (!ttp) {
	/* 
	 * Add a new token entry to the structure 
	 */
	ttp = cm_AllocTokenEntry();	/* comes back bzeroed */
	ttp->tokenFid = tlp->tokenFid;
	ttp->token.tokenID = tokenp->tokenID;
	ttp->token.type = tokenp->type;
	ttp->q.next = (struct squeue *) serverp->returnList;
	serverp->returnList = ttp;
	serverp->tokenCount++;
	if (serverp->tokenCount >= SR_NTOKENS) 
	    cm_needServerFlush = 1;
    }
    lock_ReleaseWrite(&cm_qtknlock);
}


/* 
 * Function that records that a new call returning tokens has been made 
 */
#ifdef SGIMIPS
void cm_StartTokenGrantingCall(void) 
#else  /* SGIMIPS */
void cm_StartTokenGrantingCall() 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_racinglock);
    cm_racingList.count++;
    lock_ReleaseWrite(&cm_racinglock);
}

/*
Version of cm_EndTokenGrantingCall() that supports granting tokens
to many files via one RPC, e.g., bulk status. Must call 
cm_TerminateTokenGrant() after.
*/

#ifdef SGIMIPS
void cm_EndPartialTokenGrant(
  register afs_token_t *atokenp,
  struct cm_server *aserverp,	/* where the new token is from */
  struct cm_volume *volp,
  unsigned long startTime)	/* Time token request was started */
#else  /* SGIMIPS */
void cm_EndPartialTokenGrant(atokenp, aserverp, volp, startTime)
  register afs_token_t *atokenp;
  struct cm_server *aserverp;	/* where the new token is from */
  struct cm_volume *volp;
  unsigned long startTime;	/* Time token request was started */
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    register struct cm_racingItem *tip;
#else  /* SGIMIPS */
    register struct cm_racingItem *tip, *nip;
#endif /* SGIMIPS */
    unsigned32 lifeTime;
    struct cm_server *vserverp = (struct cm_server *) 0;

    /*
     * volp->serverHost[0]  has the up-to-date fileset location.
     * Maybe, we should 'hold' the volume lock until the end of this
     * function !
     */

    if(volp)
    {
    	lock_ObtainRead(&volp->lock);
    	vserverp=volp->serverHost[0];
    	lock_ReleaseRead(&volp->lock);
    }

    lock_ObtainWrite(&cm_racinglock);

    if (atokenp) {
	for (tip = cm_racingList.racingList; tip; tip = tip->next) {
	    if (AFS_hsame(tip->token.tokenID, atokenp->tokenID)
		&& tip->request == CM_RACINGITEM_REVOKE
		&& tip->serverp == aserverp) {
		icl_Trace0(cm_iclSetp, CM_TRACE_TOKENRACE);
		/* 
		 * Found the token, we remove the rights that were revoked
		 * by the revoke call.  There may be multiple revoke calls
		 * pertaining to this token, so we keep running through the
		 * whole list even after getting a match.
		 */
		AFS_HOP(atokenp->type, &~, tip->token.type);
	    }
	} 
	/* 
	 * Handle the read-only case, where HERE tokens could be granted from
	 * any of a variety of interchangeable servers. 
	 *
	 * NOTE: This will NOT really handle the case of backup filesets!! 
	 */
	if ((vserverp == aserverp) || tkm_IsGrantFree(atokenp->tokenID)) {
	    if (lifeTime = atokenp->expirationTime) {
		lifeTime -= (lifeTime >> 6); /* allow for small clock skew */
		lifeTime = MIN(lifeTime, CM_MAX_TOKEN_EXPTIME);
#ifdef SGIMIPS
		atokenp->expirationTime = (unsigned32) (lifeTime + startTime);
#else  /* SGIMIPS */
		atokenp->expirationTime = lifeTime + startTime;
#endif /* SGIMIPS */
	    }

	    /*
	     * 64-bit support: Recognize "whole file" tokens from old servers
	     * and convert them back into the local representation (0..2^63-1).
	     * To support old servers that don't handle full 64bit ranges we
	     * check for truncation by accepting anything at least as large as
	     * 2^31-1.  This will handle losing or garbaging the high half of
	     * 2^63-1.
             */
#ifdef SGIMIPS
	    if ((aserverp) && (!aserverp->supports64bit)) 
#else /* SGIMIPS */
	    if (!aserverp->supports64bit)
#endif /* SGIMIPS */
           {
		if (AFS_hcmp(atokenp->endRange, osi_hMaxint32) >= 0)
		    atokenp->endRange = osi_hMaxint64;
	    }
	    
	} 
	else {
	    /* 
	     * This token is actually from a new server which is different
	     * from where we have the HERE token. Discard the token.
	     */
	    bzero((char *) atokenp, sizeof(afs_token_t));
	}
    }

    lock_ReleaseWrite(&cm_racinglock);
}

/*
Second part of cm_EndPartialTokenGrant(). Cleans up token list
after tokens granted to many files in one RPC. Must call
cm_EndPartialTokenGrant() first.
*/

#ifdef SGIMIPS
void cm_TerminateTokenGrant(void)
#else  /* SGIMIPS */
void cm_TerminateTokenGrant()
#endif /* SGIMIPS */
{
    register struct cm_racingItem *tip, *nip;

    lock_ObtainWrite(&cm_racinglock);

    /* 
     * Now check to see if we're the last call that was outstanding.  If
     * so, we free all the queued revoked tokenID structures, since no
     * revoke received previous to now can be revoking tokens granted to
     * calls that haven't yet been made.
     */
    if (--cm_racingList.count == 0) {
	for (tip = cm_racingList.racingList; tip; tip = nip) {
	    nip = tip->next;
	    osi_Free((opaque)tip, sizeof(*tip));
	}
	cm_racingList.racingList = (struct cm_racingItem *) 0;
    }

    lock_ReleaseWrite(&cm_racinglock);
}




/* Declare end of token-granting call.  Check granted token
 * against tokens revoked during the call.
 *
 * aconnp shouldn't be null unless atokenp is null, too; when
 * these guys are null, it means that we weren't granted a token,
 * but we have to match a cm_StartTokenGrantingCall.
 */
#ifdef SGIMIPS
void cm_EndTokenGrantingCall(
  register afs_token_t *atokenp,
  struct cm_server *aserverp,	/* where the new token is from */
  struct cm_volume *volp,	/* fileset is in volp->serverHost[0] now */
  unsigned long startTime)	/* Time token request was started */
#else  /* SGIMIPS */
void cm_EndTokenGrantingCall(atokenp, aserverp, volp, startTime)
  register afs_token_t *atokenp;
  struct cm_server *aserverp;	/* where the new token is from */
  struct cm_volume *volp;	/* fileset is in volp->serverHost[0] now */
  unsigned long startTime;	/* Time token request was started */
#endif /* SGIMIPS */
{
	cm_EndPartialTokenGrant(atokenp,aserverp,volp,startTime);
	cm_TerminateTokenGrant();
}


/* look for a racing async grant token.  Returns 0 if found item,
 * otherwise returns 1.
 */
#ifdef SGIMIPS
static int cm_ScanAsyncGrantingCall(
  register afs_token_t *atokenp,
  struct cm_server *aserverp,	/* where the new token is from */
  struct cm_volume *volp)	/* fileset is in volp->serverHost[0] now */
#else  /* SGIMIPS */
static int cm_ScanAsyncGrantingCall(atokenp, aserverp, volp)
  register afs_token_t *atokenp;
  struct cm_server *aserverp;	/* where the new token is from */
  struct cm_volume *volp;	/* fileset is in volp->serverHost[0] now */
#endif /* SGIMIPS */
{
    register struct cm_racingItem *tip;
    int rcode;

    rcode = 1;	/* not found */
    lock_ObtainWrite(&cm_racinglock);
    if (atokenp) {
	for (tip = cm_racingList.racingList; tip; tip = tip->next) {
	    if (AFS_hsame(tip->token.tokenID, atokenp->tokenID)
		&& tip->serverp == aserverp
		&& tip->request == CM_RACINGITEM_ASYNC) {
		icl_Trace0(cm_iclSetp, CM_TRACE_TOKENASYNCRACE);
		/* Found the token, copy out to caller */
		*atokenp = tip->token;
		rcode = 0;
		break;
	    }
	} 
    }	
    lock_ReleaseWrite(&cm_racinglock);
    return rcode;
}


/*     
 * Called by the token revoke code when a revoke couldn't be delivered. If 
 * there are no outstanding token-granting calls, then we do nothing, otherwise
 * we queue the request to be processed at the end of the token-granting calls,
 * in case the token that was revoked was returned by one of these concurrently
 * executing calls.
 */
#ifdef SGIMIPS
void cm_QueueRacingRevoke(
  register afsRevokeDesc *atokenp,
  struct cm_server *aserverp)
#else  /* SGIMIPS */
void cm_QueueRacingRevoke(atokenp, aserverp)
  register afsRevokeDesc *atokenp;
  struct cm_server *aserverp;
#endif /* SGIMIPS */
{
    register struct cm_racingItem *tip;

    lock_ObtainWrite(&cm_racinglock);
    if (cm_racingList.count > 0) {
	tip = (struct cm_racingItem *) osi_Alloc(sizeof(*tip));
	tip->next = cm_racingList.racingList;
	cm_racingList.racingList = tip;
	tip->token.type = atokenp->type;
	tip->token.tokenID = atokenp->tokenID;
	tip->serverp = aserverp;
	tip->request = CM_RACINGITEM_REVOKE;
    }
    lock_ReleaseWrite(&cm_racinglock);
}


/* called by async grant code when async grant couldn't be delivered.
 * otherwise, identical to cm_QueueRacingRevoke.  ExpirationTime is
 * passed in relative format; caller will add in current time
 * after cm_EndTokenGrantingCall is called.
 */
#ifdef SGIMIPS
void cm_QueueRacingGrant(
  register afs_token_t *atokenp,
  struct cm_server *aserverp)
#else  /* SGIMIPS */
void cm_QueueRacingGrant(atokenp, aserverp)
  register afs_token_t *atokenp;
  struct cm_server *aserverp;
#endif /* SGIMIPS */
{
    register struct cm_racingItem *tip;

    lock_ObtainWrite(&cm_racinglock);
    if (cm_racingList.count > 0) {
	tip = (struct cm_racingItem *) osi_Alloc(sizeof(*tip));
	tip->next = cm_racingList.racingList;
	cm_racingList.racingList = tip;
	tip->token = *atokenp;
	tip->serverp = aserverp;
	tip->request = CM_RACINGITEM_ASYNC;
    }
    lock_ReleaseWrite(&cm_racinglock);
}


/* 
 * Flush all the queued tokens being returned to a particular
 * server, no matter how many there are queued (except if
 * there are none at all queued).  Called with no locks held.
 */
#ifdef SGIMIPS
void cm_FlushQueuedServerTokens(register struct cm_server *serverp)
#else  /* SGIMIPS */
void cm_FlushQueuedServerTokens(serverp)
    register struct cm_server *serverp; 
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    register long code;
    register afsReturns *tknlistP;    /* not to allocate this from stack */
    afsReturnDesc *trevp;
    register struct cm_tokenList *ttp;
    struct cm_tokenList *xtp;
    register struct cm_conn *connp;
    error_status_t st;
    u_long startTime;
    int i;
    int tcount;

    icl_Trace1(cm_iclSetp, CM_TRACE_FQST, ICL_TYPE_POINTER, serverp);

    lock_ObtainRead(&serverp->lock);
    if (serverp->states & (SR_TSR|SR_DOWN)) {
       /* Server can not perform work.  Check
	* now to avoid losing tokens from spurious tries.
	*/
       lock_ReleaseRead(&serverp->lock);
       return;
    }
    lock_ReleaseRead(&serverp->lock);
    
    for (;;) {
	if (serverp->tokenCount == 0)	/* check for trivial case */
	    return;

	/* this is essentially a compile-time check, which will compile
	 * into a noop or a panic.
	 */
	/* CONSTCOND */
#ifdef SGIMIPS
/* Turn off "expression has no effect" error. */
#pragma set woff 1171
#endif
	osi_assert(SR_NTOKENS * sizeof(afsReturnDesc) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif

	lock_ObtainWrite(&cm_qtknlock);
	
	tknlistP = (afsReturns *)osi_AllocBufferSpace();
	tcount = serverp->tokenCount;
	if (tcount > SR_NTOKENS) 
	    tcount = SR_NTOKENS;
	tknlistP->afsReturns_len = tcount;
	trevp = tknlistP->afsReturns_val;
	for (i=0, ttp=serverp->returnList; ttp && i < tcount; i++, trevp++) {
	    trevp->tokenID = ttp->token.tokenID;
	    trevp->type = ttp->token.type;
	    trevp->fid.Cell = serverp->cellp->cellId;
	    trevp->fid.Volume = ttp->tokenFid.Volume;
#ifdef SGIMIPS
	    trevp->fid.Vnode = (unsigned32)ttp->tokenFid.Vnode;
	    trevp->fid.Unique = (unsigned32)ttp->tokenFid.Unique;
#else  /* SGIMIPS */
	    trevp->fid.Vnode = ttp->tokenFid.Vnode;
	    trevp->fid.Unique = ttp->tokenFid.Unique;
#endif /* SGIMIPS */
	    trevp->flags = 0;	/* initialize */
	    xtp = ttp;
	    ttp = (struct cm_tokenList *) ttp->q.next;
	    cm_FreeTokenEntry(xtp);
	}
	serverp->returnList = ttp;	/* splice out returned tokens */
	serverp->tokenCount -= tcount;	/* make sure it matches */
	lock_ReleaseWrite(&cm_qtknlock);
	
	cm_InitNoauthReq(&rreq);
	cm_SetConnFlags(&rreq, CM_RREQFLAG_NOVOLWAIT);
	do {
	    if (connp = cm_ConnByHost(serverp,  MAIN_SERVICE(SRT_FX),
				      &serverp->cellp->cellId, &rreq, 0)) {
		icl_Trace1(cm_iclSetp, CM_TRACE_RELTOKENSTART,
			   ICL_TYPE_LONG, tcount);
		startTime = osi_Time();
		st = BOMB_EXEC("COMM_FAILURE",
			       (osi_RestorePreemption(0),
				st = AFS_ReleaseTokens(connp->connp, tknlistP, 0),
				osi_PreemptionOff(),
				st));
		code = osi_errDecode(st);
		icl_Trace1(cm_iclSetp, CM_TRACE_RELTOKENEND,
			   ICL_TYPE_LONG, code);
	    }
	    else 
		code = -1;
	} while (cm_Analyze(connp, code, 0, &rreq, NULL, -1, startTime));
	/* 
	 * code doesn't matter, server may have done the work 
	 */
	osi_FreeBufferSpace((struct osi_buffer *)tknlistP);

	if (code) return;	/* it ain't gonna get better */
    }
}


/* Look for one open token, and return the token ID if we find exactly
 * one token with any of the "open" bits set.  Otherwise, do nothing.
 * This function deletes the open bits from the token if it returns
 * a token ID.
 * This function must be called with a write-locked vnode.
 */
#ifdef SGIMIPS
void cm_ReturnSingleOpenToken(
  register struct cm_scache *ascp,
  afs_hyper_t *aIDp)
#else  /* SGIMIPS */
void cm_ReturnSingleOpenToken(ascp, aIDp)
  register struct cm_scache *ascp;
  afs_hyper_t *aIDp;
#endif /* SGIMIPS */
{
    register struct cm_tokenList *tlp, *openlp;

    openlp = (struct cm_tokenList *) 0;	/* none found yet */
    for (tlp = (struct cm_tokenList *) ascp->tokenList.next;
	 tlp != (struct cm_tokenList *) &ascp->tokenList;
	 tlp = (struct cm_tokenList *) tlp->q.next) {
	if (!(tlp->flags & (CM_TOKENLIST_DELETED|CM_TOKENLIST_ASYNC))
	    && (AFS_hgetlo(tlp->token.type) & CM_OPEN_TOKENS)) {
	    if (openlp) return;	/* now we'd have two */
	    openlp = tlp;
	}
    }
    if (openlp) {
	/* found exactly one open token */
	*aIDp = openlp->token.tokenID;
	AFS_HOP32(openlp->token.type, &~, CM_OPEN_TOKENS);
	if (AFS_hiszero(openlp->token.type)) {
	    cm_DelToken(openlp);
	}
    }
}

/*
 * Return the token corresponding to tokenID if it exists in
 * the file's list of tokens. This call assumes at least a read lock
 * on the llock structure.
 *
 * This function should return async tokens, too.
 * It probably doesn't matter what it does with deleted tokens.
 */
#ifdef SGIMIPS
struct cm_tokenList *cm_FindTokenByID(
    struct cm_scache *scp,
    afs_hyper_t *tokenID)
#else  /* SGIMIPS */
struct cm_tokenList *cm_FindTokenByID(scp, tokenID)
    struct cm_scache *scp;
    afs_hyper_t *tokenID;
#endif /* SGIMIPS */
{
    register struct cm_tokenList *tlp;

    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *) &scp->tokenList;
	 tlp = (struct cm_tokenList *) tlp->q.next) {

	if (AFS_hsame(tlp->token.tokenID, *tokenID))
	    return tlp;
    }
    return ((struct cm_tokenList *) 0);    /* not found */
}


/*
 * Returns the token matching the token passed if it exists in
 * the file's list of tokens. We need this call for tokens of
 * readonly files which all have the same ID. This call assumes 
 * at least a read lock on the llock structure.
 */
#ifdef SGIMIPS
struct cm_tokenList *cm_FindMatchingToken(
    struct cm_scache *scp,
    afs_token_t *atoken)
#else  /* SGIMIPS */
struct cm_tokenList *cm_FindMatchingToken(scp, atoken)
    struct cm_scache *scp;
    afs_token_t *atoken;
#endif /* SGIMIPS */
{

    register struct cm_tokenList *tlp;


    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *) &scp->tokenList;
	 tlp = (struct cm_tokenList *) tlp->q.next) {

	/*
	 * make sure that the token we return is at least as 
	 * 'powerful' as the one passed in 
	 */

	if ((tlp->token.expirationTime >= atoken->expirationTime) &&
	    AFS_hissubset(atoken->type, tlp->token.type) &&
	    (AFS_hcmp(tlp->token.beginRange, atoken->beginRange) <= 0) &&
	    (AFS_hcmp(tlp->token.endRange, atoken->endRange) >= 0) &&
	    !(tlp->flags & (CM_TOKENLIST_DELETED|CM_TOKENLIST_ASYNC))) {
	    return tlp;
	}
    }
    return ((struct cm_tokenList *) 0);    /* not found */
}

/*
 * To make sure that volume has HERE token.
 */
#ifdef SGIMIPS
int cm_HaveHereToken(register struct cm_volume *volp)
#else  /* SGIMIPS */
int cm_HaveHereToken(volp)
   register struct cm_volume *volp;
#endif /* SGIMIPS */
{
   int haveToken = 0;
   
   /* 
    * Note there is no need to check server's status because the HERE token 
    * is a basic token and also the cm_HaveTokens will do the final check. 
    */
   lock_ObtainRead(&volp->lock);
   if (((volp->states & VL_RESTORETOKENS) == 0)  &&
       (cm_SameHereServer(volp->hereServerp, volp)) &&
       (volp->hereToken.expirationTime > osi_Time()))
       haveToken = 1;

   lock_ReleaseRead(&volp->lock);
   return haveToken;
}     

/*
 * To get the HERE token for the volume.
 * Called with NO lock held. Caller must Hold/Rele RefCount for volume.
 * The internalFlags are defined by the CM_GETTOKEN_... defines in cm_tokens.h.
 */
#ifdef SGIMIPS
cm_GetHereToken(
    struct cm_volume *volp,
    struct cm_rrequest *rreqp,
    long flags,
    long internalFlags)
#else  /* SGIMIPS */
cm_GetHereToken(volp, rreqp, flags, internalFlags)
    struct cm_volume *volp;
    struct cm_rrequest *rreqp;
    long flags;
    long internalFlags;
#endif /* SGIMIPS */
{
    register long code;
    register struct cm_conn *connp;
    struct cm_server *tserverp;
    u_long startTime;
    struct lclHeap {
	afsFetchStatus OutStatus;
	afsVolSync tsync;
	afs_token_t realToken, ttoken;
	afs_recordLock_t afsBlocker;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    int safety = 0, doingTSR = 0;
    long srvix;
    unsigned32 service;
    unsigned32 Flags;
    error_status_t st;
    int skipHaveTokens;

#ifdef SGIMIPS
    skipHaveTokens = (int)(internalFlags & CM_GETTOKEN_FORCE);
#else  /* SGIMIPS */
    skipHaveTokens = (internalFlags & CM_GETTOKEN_FORCE);
#endif /* SGIMIPS */
    Flags = AFS_FLAG_RETURNTOKEN | AFS_FLAG_NOOPTIMISM | AFS_FLAG_NOREVOKE;

    icl_Trace2(cm_iclSetp, CM_TRACE_GETHERETOKENS,
	       ICL_TYPE_LONG, AFS_hgetlo(volp->volume), ICL_TYPE_LONG, flags);

    if ((internalFlags & CM_GETTOKEN_SEC) ||
	(flags & (AFS_FLAG_SERVER_REESTABLISH | AFS_FLAG_MOVE_REESTABLISH |
	AFS_FLAG_TOKENID))) {
	service = SEC_SERVICE(SRT_FX);
	doingTSR = 1;
    }
    else 
	service = MAIN_SERVICE(SRT_FX);
	  
    Flags |= flags;

#ifdef AFS_SMALLSTACK_ENV
    /* Keep stack frame small by doing heap-allocation */
    lhp = (struct lclHeap *)osi_AllocBufferSpace();
    /* this is essentially a compile-time check, which will compile into
     * a noop or a panic.
     */
#ifdef SGIMIPS
#pragma set woff 1171
#endif
    /* CONSTCOND */
    osi_assert(sizeof(struct lclHeap) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif
#endif /* AFS_SMALLSTACK_ENV */

    AFS_hset64(lhp->ttoken.type, 0, TKN_SPOT_HERE);
    AFS_hzero(lhp->ttoken.beginRange);
    lhp->ttoken.endRange = osi_hMaxint64;
    AFS_hzero(lhp->ttoken.tokenID);

    lock_ObtainRead(&volp->lock);
    if (flags & AFS_FLAG_TOKENID) {/* same token id */
	lhp->ttoken.tokenID = volp->hereToken.tokenID;
    }
    lock_ReleaseRead(&volp->lock);

    for (;;) {
	/* 
	 * First check to see if we already have the right tokens 
	 */
	code = 0;
	lock_ObtainRead(&volp->lock);   /* In case we have one */
	if (!skipHaveTokens && cm_SameHereServer(volp->hereServerp, volp) &&
	    (volp->hereToken.expirationTime > osi_Time())) {
	    lock_ReleaseRead(&volp->lock);
	    break;		/* OK. Break the while loop */
	}
	skipHaveTokens = 0;
	lock_ReleaseRead(&volp->lock);
	
	if (safety++ > 10)
	    osi_Wait(1000, 0, 0);	/* for performance reasons */
	cm_StartTokenGrantingCall();
	do {
	    tserverp = 0;
	    /* we don't need to do the vol op counting work here, since
	     * we're not making a call whose status will be merged into
	     * a vnode.
	     */
	    if (connp = cm_Conn(&volp->rootFid, service, rreqp, &volp, &srvix)){
		icl_Trace2(cm_iclSetp, CM_TRACE_GETTOKENSTART,
			   ICL_TYPE_POINTER, NULL, 
			   ICL_TYPE_LONG, AFS_hgetlo(lhp->ttoken.type));
		startTime = osi_Time();
		st = BOMB_EXEC
		    ("COMM_FAILURE",
		     (osi_RestorePreemption(0),
		      st = AFS_GetToken(connp->connp, &volp->rootFid, 
					&lhp->ttoken, &osi_hZero, Flags, 
					&lhp->realToken, &lhp->afsBlocker, 
					&lhp->OutStatus, &lhp->tsync),
		      osi_PreemptionOff(),
		      st));
		code = osi_errDecode(st);
		tserverp = connp->serverp;
                DFS_ADJUST_LOCK_EXT(&lhp->afsBlocker, tserverp);
		icl_Trace1(cm_iclSetp, CM_TRACE_GETTOKENEND,
			   ICL_TYPE_LONG, code);
	    }
	    else
		code = -1;
	    /*
	     * Should NOT retry upon receiving a token conflict error in TSR.
	     */
	    if (doingTSR && (code == TKM_ERROR_TOKENCONFLICT)) {
		code = TKM_ERROR_HARDCONFLICT;
	    }
	} while (cm_Analyze(connp, code, &volp->rootFid, rreqp, volp, srvix,
			    startTime));
	/* watch for races */
	cm_EndTokenGrantingCall(&lhp->realToken, tserverp, volp, startTime);	
	if (code) {
	    /* recognize TOKENINVALID also since we were using that for a while */
	    if (code == TKM_ERROR_INVALIDID
		|| code == TKM_ERROR_TOKENINVALID) {
		/* Couldn't get the same token; get any token */
		Flags &= ~AFS_FLAG_TOKENID;
		continue;	/* try again */
	    } else if (code == FSHS_ERR_MOVE_FINISHED) {
		/* 
		 * the fileset has already been moved. Do not use the 
		 * special flag, AFS_FLAG_MOVE_REESTABLISH, any more.
		 */
		Flags &= ~AFS_FLAG_MOVE_REESTABLISH;
		continue;	/* try again */
	    } else if (code == FSHS_ERR_SERVER_UP) {
		Flags &= ~AFS_FLAG_SERVER_REESTABLISH;
		continue;	/* try again */
	    } else 
		break;		/* break the for(;;) loop */
	}
	lock_ObtainWrite(&volp->lock);
	if ((volp->serverHost[0] == tserverp) || (volp->states & VL_RO)) {
	    /* add the HERE token to the volume */
	    volp->hereToken = lhp->realToken;
	    volp->hereServerp = tserverp;
	    /* Tell the rest of the CM that we have tokens from this server. */
	    if (tserverp && !tkm_IsGrantFree(lhp->realToken.tokenID)) {
		lock_ObtainWrite(&tserverp->lock);
		tserverp->states |= SR_HASTOKENS;
		lock_ReleaseWrite(&tserverp->lock);
	    }
	}
	lock_ReleaseWrite(&volp->lock);
    } /* for (;;) */

#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    return cm_CheckError(code, rreqp);
}

/* Structures for summarizing TSR error reports. */
struct tsrOne {
    afs_hyper_t vol;
    u_long vnode;
    u_long count;
};
/* The several kinds of TSR problems to be summarized */
#define tsrSum_Crash 0
#define tsrSum_Partition 1
#define tsrSum_Version 2
#define tsrSum_Ctime 3
#define tsrSum_MAX 3
#define tsrSum_SIZE (tsrSum_MAX+1)
static char *tsrDescrs[tsrSum_SIZE] = {
    "cannot recover file locks after a crash",
    "server reclaimed token during a partition",
    "wrong data version",
    "wrong creation-time"
};
struct tsrSummary {
    struct tsrOne summs[tsrSum_SIZE];
    long allVnodes;
};

#ifdef SGIMIPS
static void cm_printTSRSummary(register struct tsrSummary *tsrp)
#else  /* SGIMIPS */
static void cm_printTSRSummary(tsrp)
  register struct tsrSummary *tsrp;
#endif /* SGIMIPS */
{
    register int ix;

    if (tsrp->allVnodes == 0)
	return;
    cm_printf("dfs: Token recovery failed for %d file(s):\n", tsrp->allVnodes);
    for (ix = 0; ix < tsrSum_SIZE; ++ix) {
	if (tsrp->summs[ix].count) {
	    if (tsrp->summs[ix].count == 1) {
		cm_printf("dfs: %s, fileset %u,,%u index %d\n",
			  tsrDescrs[ix],
			  AFS_HGETBOTH(tsrp->summs[ix].vol),
			  tsrp->summs[ix].vnode);
	    } else {
		cm_printf("dfs: %s for %d files, including fileset %u,,%u index %d\n",
			  tsrDescrs[ix], tsrp->summs[ix].count,
			  AFS_HGETBOTH(tsrp->summs[ix].vol),
			  tsrp->summs[ix].vnode);
	    }
	}
    }
}

/* Count one more TSR failure of the given kind. */
#ifdef SGIMIPS
static void cm_tagTSR(
  register struct tsrSummary *tsrp,
  register int ix,	/* which guy in the summary */
  register struct cm_scache *scp)
#else  /* SGIMIPS */
static void cm_tagTSR(tsrp, ix, scp)
  register struct tsrSummary *tsrp;
  register int ix;	/* which guy in the summary */
  register struct cm_scache *scp;
#endif /* SGIMIPS */
{
    tsrp->allVnodes++;
    tsrp->summs[ix].count++;
    tsrp->summs[ix].vol = scp->fid.Volume;
    tsrp->summs[ix].vnode = scp->fid.Vnode;
}

/* forward declaration */
static int cm_RecoverSCacheToken _TAKES((struct cm_scache *, struct cm_server *,
					 long *, int, int *, struct tsrSummary *));
/*
 * Perform the Token State Recovery after the server is reconnected. 
 * Must be called with NO server locks held. 
 * NB. We do not recycle cm_server structure. 
 */
int cm_RecoverTokenState(
  struct cm_server *serverp,
  long sameEpoch,
  unsigned32 inFlags)
{
    register struct cm_scache *scp;
    register struct cm_volume *volp;
    struct cm_rrequest rreq;
    long Flags = 0, code = 0, rcode = 0;
    register int i, renew = 0;
    int tsrMode = 0;
    register struct cm_tokenList *tlp;
    struct tsrSummary tsrsum;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */

    cm_GetServerSize(serverp);		/* if we don't alreaady know */

    /*
     * If it is the same server epoch, we can recover the token with the
     * same token id, since the server may still remember those tokens. 
     * Otherwise, the server must have crashed before and set the flag to
     * SERVER_REESTABLISH. 
     */
    /* If the same-token-ID could be OK, go for it. */
    if (!(inFlags & TKN_FLAG_DISALLOW_SAME)) {
	Flags |= AFS_FLAG_TOKENID;
	tsrMode = TSR_PARTITION;
    }
    /* If the server is in post-crash recovery, use that in flags */
    if ((inFlags & TKN_FLAG_CRASHED)) {
	Flags |= AFS_FLAG_SERVER_REESTABLISH;
    }
    /* Call it a crash-recovery for purposes of doing lock recovery */
    if (!sameEpoch || (inFlags & TKN_FLAG_CRASHED))
	tsrMode = TSR_CRASH;
    /* Else it's just a partition recovery. */
    if (tsrMode == 0)
	tsrMode = TSR_PARTITION;
    icl_Trace2(cm_iclSetp, CM_TRACE_RECOVERTOKENSTATE, ICL_TYPE_LONG, Flags,
	       	       ICL_TYPE_LONG, tsrMode);
    /* 
     * Renew the HERE token for each volume if it had one before.
     */
    bzero((char *) &tsrsum, sizeof(tsrsum));
    cm_InitUnauthReq(&rreq);       /* use a un-authen binding */
    lock_ObtainWrite(&cm_volumelock);
    for (i = 0; i < VL_NVOLS; i++) {
	for (volp = cm_volumes[i]; volp; volp = volp->next) {
	    volp->refCount++;
	    lock_ReleaseWrite(&cm_volumelock);

	    lock_ObtainRead(&volp->lock);
	    if (!(volp->states & (VL_HISTORICAL | VL_RESTORETOKENS)) && 
		(volp->hereServerp ==  serverp)) {/* in the same server */
	      
		lock_ReleaseRead(&volp->lock);
		if (cm_HaveHereToken(volp)) {
		    /* 
		     * Invalidate token so that we are forced to get it later.
		     */
		    lock_ObtainWrite(&volp->lock);
		    volp->hereToken.expirationTime = 0;
		    lock_ReleaseWrite(&volp->lock);
		    code = cm_GetHereToken(volp, &rreq, Flags, CM_GETTOKEN_SEC);
		    if (code) {
			icl_Trace1(cm_iclSetp, CM_TRACE_RECOVERHERETOKEN,
				   ICL_TYPE_LONG, code);
			if (code == ETIMEDOUT) {
			    /* server is down, couldn't continue it.*/
			    /* Don't give up on this server; this vol might
			     * have moved to some other server, and the
			     * ETIMEDOUT might be from that.  Proceed with
			     * reclaiming the rest of the tokens on this server.
			     */
			    if (!(volp->states & VL_TSRCRASHMOVE)) {
				cm_PutVolume(volp);
#ifdef SGIMIPS
				return (int)code;
#else  /* SGIMIPS */
				return code;
#endif /* SGIMIPS */
			    }
			} 
		    }
		    lock_ObtainRead(&volp->lock);

		    if (code != ETIMEDOUT
			&& !cm_SameHereServer(serverp, volp)) {
			lock_ReleaseRead(&volp->lock);
			/* 
			 * Fileset is moved while network is partitioned. Since
			 * don't know whether the move is complete or not, call
			 * the function to handle this Crash-Move case.
			 */
			/* We know that there was a comm failure here of some sort. */
			code = cm_RestoreMoveTokens(volp, serverp, &rreq,
						    (tsrMode & TSR_CRASH)
						    ? (TSR_CRASHMOVE | TSR_NOWAIT | TSR_KNOWCRASH)
						    : (TSR_CRASHMOVE | TSR_NOWAIT));
		    } else {
			lock_ReleaseRead(&volp->lock);
		    }

		    if (!rcode) rcode = code;
		} /* Never have HERE token */

	    } else /* NOT the same server */
		lock_ReleaseRead(&volp->lock);

	    lock_ObtainWrite(&cm_volumelock);
	    volp->refCount--;

	} /* for */
    } /* for */
    lock_ReleaseWrite(&cm_volumelock);

    lock_ObtainWrite(&cm_scachelock);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
#ifdef SGIMIPS
            s = VN_LOCK(SCTOV(scp));
            if ( SCTOV(scp)->v_flag & VINACT ) {
                osi_assert(CM_RC(scp) == 0);
                VN_UNLOCK(SCTOV(scp), s);
                continue;
            }
            CM_RAISERC(scp);
            VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */
	    CM_HOLD(scp);
#endif /* SGIMIPS */
#ifdef AFS_SUNOS5_ENV
/* Don't do this: you can't do data stores until TSR is finished! */
#if 0
	    if (scp->v.v_count == 1 && scp->opens != 0)
		cm_CheckOpens(scp, 1);
#endif /* 0 */
#endif /* AFS_SUNOS5_ENV */
	    lock_ReleaseWrite(&cm_scachelock);
	    lock_ObtainWrite(&scp->llock);
	    /* Process only those scp's that are on volumes on this server. */
	    if (scp->volp) {
		lock_ObtainRead(&scp->volp->lock);

		if (cm_SameHereServer(serverp, scp->volp)) {
		    lock_ReleaseRead(&scp->volp->lock);

		    if (cm_HaveHereToken(scp->volp)) {
			/*
			 * Try to recover tokens for files that were open
			 * before or files that have pending dirty data.
			 */
			renew = 0;
			if (scp->opens > 0     ||
			    scp->modChunks > 0 || 
			    cm_NeedStatusStore(scp))
			    renew = 1;
			code = cm_RecoverSCacheToken(scp, serverp, &Flags,
						     renew, &tsrMode, &tsrsum);
			switch(code) {
			  case 0:
			    /* A OK ! Resume the business as usual. */
			    break;
			  case ETIMEDOUT:
			    /* a very rare case */
			    break;
			  case ESTALE:
			  default:
			    cm_MarkBadSCache(scp, ESTALE);
			} /* switch */
		    } else {
			unsigned32 now = osi_Time();
			/* Don't have the HERE token for this fileset */
			/* Flush this scp only if we have any tokens from this server. */
			for(tlp = (struct cm_tokenList *) scp->tokenList.next;
			    tlp != (struct cm_tokenList *) &scp->tokenList;
			    tlp = (struct cm_tokenList *) tlp->q.next) {
			    if (!(tlp->flags & CM_TOKENLIST_DELETED)
				&& (tlp->serverp == serverp)
				&& (tlp->token.expirationTime >= now)) {
				/*
				 * Perhaps we could instead be calling
				 * RecoverSCacheToken with renew==0.
				 */
				cm_MarkBadSCache(scp, ESTALE);
				break;
			    }
			}
		    }
		    if (!rcode) rcode = code;

		} else {
		    lock_ReleaseRead(&scp->volp->lock);
		}
	    }
	    lock_ReleaseWrite(&scp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);
	} /* for */
    }
    lock_ReleaseWrite(&cm_scachelock);
    cm_printTSRSummary(&tsrsum);
    
#ifdef SGIMIPS
    return (int)rcode;
#else  /* SGIMIPS */
    return rcode;
#endif /* SGIMIPS */
}

/*
 * To restablish the token state following a move of a given fileset to a new
 * location (server).  Must be called with NO server locks held. 
 */
#ifdef SGIMIPS
int cm_RestoreMoveTokens(
    struct cm_volume *volp,
    struct cm_server *origserverp,
    struct cm_rrequest *rreqp,    
    int tsrMode)
#else  /* SGIMIPS */
int cm_RestoreMoveTokens(volp, origserverp, rreqp, tsrMode)
    struct cm_volume *volp;
    struct cm_server *origserverp;
    struct cm_rrequest *rreqp;    
    int tsrMode;
#endif /* SGIMIPS */
{
    register struct cm_scache *scp;
    register int i, renew = 0;
    long Flags = 0, error, code = 0;
    int anyError = 0;
    struct tsrSummary tsrsum;
    struct cm_server *newserverp;
    struct cm_tokenList *tlp;
    unsigned32 now;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */

    CM_BUMP_COUNTER(filesetCacheInvalidations);
    icl_Trace0(cm_iclSetp, CM_TRACE_MOVETOKEN);

    Flags = AFS_FLAG_MOVE_REESTABLISH;

    lock_ObtainWrite(&volp->lock);
    /* Check this one more time in case of a race condition !*/
    if (volp->states & VL_RESTORETOKENS) {
	/* someone else is doing TSR-move right now */

	icl_Trace1(cm_iclSetp, CM_TRACE_ISDOINGTSRMOVE,
		   ICL_TYPE_LONG, tsrMode);
	if ((tsrMode & TSR_NOWAIT) == 0) {
	    while (volp->states & VL_RESTORETOKENS) {
		osi_SleepW((opaque)(&volp->states), &volp->lock);
		lock_ObtainWrite(&volp->lock);
	    }
	}
	/* Unconditionally return.  The TSR_NOWAIT case will not really get
	 * an error code. */
	lock_ReleaseWrite(&volp->lock);
	return cm_CheckError(code, rreqp);
    }
    /* 
     * Update the volp->hereServerp. That is where we will get HERE token.
     * Note that volp->serverHost must have the new server location.
     */
    volp->lameHereToken = volp->hereToken;
    volp->hereServerp = volp->serverHost[0];
    newserverp = volp->hereServerp;
    volp->hereToken.expirationTime = 0;	/* in case we fail */
    volp->states |= VL_RESTORETOKENS;
    lock_ReleaseWrite(&volp->lock);
    bzero((char *) &tsrsum, sizeof(tsrsum));

    /*
     * While VL_RESTORETOKENS is set, no tokens on this volume will be usable by
     * ordinary threads.  And if this TSR attempt fails, the volume will be left
     * with no HERE token, forcing some later user to retry this procedure (and
     * again ensuring that no tokens are used).  After TSR is completed, all
     * tokens from this volume will have been either forgotten, returned, or
     * re-obtained.
     */

    if (code = cm_GetHereToken(volp, rreqp, Flags, CM_GETTOKEN_DEF)) {
	/* Fail to get the HERE token */
	lock_ObtainWrite(&volp->lock);
	volp->hereServerp = origserverp;	/* force to do TSR later */
	bzero((char *) &volp->hereToken, sizeof(afs_token_t));
	cm_printf("dfs: failed to get a HERE token for fileset %u,,%u in TSR mode %x; code = %d\n",
		  AFS_HGETBOTH(volp->volume), tsrMode, code);
	goto out;
    }
    lock_ObtainWrite(&cm_scachelock);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
	    if (!AFS_hsame(volp->volume,  scp->fid.Volume))
		continue;
#ifdef SGIMIPS
            s = VN_LOCK(SCTOV(scp));
            if ( SCTOV(scp)->v_flag & VINACT ) {
                osi_assert(CM_RC(scp) == 0);
                VN_UNLOCK(SCTOV(scp), s);
                continue;
            }
            CM_RAISERC(scp);
            VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */
	    CM_HOLD(scp);
#endif /* SGIMIPS */
#ifdef AFS_SUNOS5_ENV
/* Don't do this: you can't do data stores until TSR is finished! */
#if 0
	    if (scp->v.v_count == 1 && scp->opens != 0)
		cm_CheckOpens(scp, 1);
#endif /* 0 */
#endif /* AFS_SUNOS5_ENV */
	    lock_ReleaseWrite(&cm_scachelock);
	    lock_ObtainWrite(&scp->llock);
	    if ((scp->states & SC_RO) == 0) {
		/*
		* In TSR-MOVE there should be NO pending dirty data left unless
		* it is in the TSR-CRASHMOVE case. In a case of TSR-CRASHMOVE,
		* we have to get WRITE tokens before we send them back to the
		* new server.
		*/
		/* check for normal TSR-Move mode */
		if (anyError == 0 && (tsrMode & TSR_MOVE)) {
		    /* Turn the assertions into cm_printf's */
		    if (scp->modChunks != 0) {
			cm_printf("dfs: %d dirty chunks, fid %u of fileset %u,,%u, TSR mode %x\n",
				  scp->modChunks, scp->fid.Vnode,
				  AFS_HGETBOTH(volp->volume), tsrMode);
		    }
		    if ((scp->m.ModFlags & ~CM_MODACCESS) != 0) {
			cm_printf("dfs: modflags 0x%x, fid %u of fileset %u,,%u, TSR mode %x\n",
				  scp->m.ModFlags, scp->fid.Vnode,
				  AFS_HGETBOTH(volp->volume), tsrMode);
		    }
		    /* Will remove these two asserts eventually */
		    /* osi_assert(scp->modChunks == 0); */
		    /* osi_assert((scp->m.ModFlags & ~CM_MODACCESS) == 0); */
		}
	       renew = 0;
	       if ((scp->opens > 0) || 
		   ((scp->modChunks || (scp->m.ModFlags & ~CM_MODACCESS))))
		   renew = 1;	/* actively renew tokens for active files */
	       error = cm_RecoverSCacheToken(scp, origserverp, &Flags,
					     renew, &tsrMode, &tsrsum);
	       switch(error) {
	       case 0:
		  /* A OK ! Resume the business as usual. */
		  break;
	       case ETIMEDOUT:
		  /* 
		   * Cascade failure ! Except from a normal server crash, 
		   * mark all files BAD.
		   */
	       case ESTALE:
	       default:
		  cm_MarkBadSCache(scp, ESTALE);
		  anyError = 1;
	       } /* switch */
	    }
	    lock_ReleaseWrite(&scp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);		
	} /* for */
    }
    /*
     * Now verify that the results are sane--we hold nothing from the old
     * server.
     */
    icl_Trace2(cm_iclSetp, CM_TRACE_RESTOREMOVE_PREP,
	       ICL_TYPE_POINTER, origserverp,
	       ICL_TYPE_POINTER, newserverp);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
	    if (!AFS_hsame(volp->volume,  scp->fid.Volume))
		continue;
#ifdef SGIMIPS
            s = VN_LOCK(SCTOV(scp));
            if ( SCTOV(scp)->v_flag & VINACT ) {
                osi_assert(CM_RC(scp) == 0);
                VN_UNLOCK(SCTOV(scp), s);
                continue;
            }
            CM_RAISERC(scp);
            VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */
	    CM_HOLD(scp);
#endif /* SGIMIPS */
	    lock_ReleaseWrite(&cm_scachelock);
	    lock_ObtainWrite(&scp->llock);
	    if ((scp->states & SC_RO) == 0) {
		now = osi_Time();
		for (tlp = (struct cm_tokenList *) scp->tokenList.next;
		     tlp != (struct cm_tokenList *) &scp->tokenList;
		     tlp = (struct cm_tokenList *) tlp->q.next) {
		    /*
		     * Expect:
		     * - if not _DELETED or expired:
		     * --- no CM_TOKENLIST_REVALIDATE
		     * --- not from origserverp
		     * --- most likely from newserverp
		     * --- could have _ASYNCWAIT
		     */
		    if (!(tlp->flags & CM_TOKENLIST_DELETED)
			&& (tlp->token.expirationTime >= now)) {
			if (tlp->flags & CM_TOKENLIST_REVALIDATE) {
			    icl_Trace4(cm_iclSetp, CM_TRACE_RESTOREMOVE_FAIL_1,
				       ICL_TYPE_FID, &scp->fid,
				       ICL_TYPE_LONG, tlp->flags,
				       ICL_TYPE_POINTER, tlp->serverp,
				       ICL_TYPE_LONG, AFS_hgetlo(tlp->token.type));
			}
			if (tlp->serverp == origserverp) {
			    icl_Trace4(cm_iclSetp, CM_TRACE_RESTOREMOVE_FAIL_2,
				       ICL_TYPE_FID, &scp->fid,
				       ICL_TYPE_LONG, tlp->flags,
				       ICL_TYPE_POINTER, tlp->serverp,
				       ICL_TYPE_LONG, AFS_hgetlo(tlp->token.type));
			} else if (tlp->serverp != newserverp) {
			    icl_Trace4(cm_iclSetp, CM_TRACE_RESTOREMOVE_FAIL_3,
				       ICL_TYPE_FID, &scp->fid,
				       ICL_TYPE_LONG, tlp->flags,
				       ICL_TYPE_POINTER, tlp->serverp,
				       ICL_TYPE_LONG, AFS_hgetlo(tlp->token.type));
			}
		    }
		}
	    }
	    lock_ReleaseWrite(&scp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);		
	} /* for */
    }
    lock_ReleaseWrite(&cm_scachelock);
    /*
     * We finally finish the TSR-Move work 
     */
    lock_ObtainWrite(&volp->lock);
    volp->states &= ~VL_TSRCRASHMOVE;
out:
    bzero((char *)&volp->lameHereToken, sizeof(afs_token_t));
    volp->states &= ~VL_RESTORETOKENS;
    osi_Wakeup(&volp->states);
    lock_ReleaseWrite(&volp->lock);
    cm_printTSRSummary(&tsrsum);
    return cm_CheckError(code, rreqp);
}
/*
 * To recover the same tokens or comparable tokens for this particular file.
 * However, if the renew is 0, we do not issue an rpc call to get tokens for 
 * this file, since this file may not be active any more.
 *
 * Renew is typically set when a file is open, or has dirty data/status,
 * at the time we're doing the recovery, the idea being that unused
 * tokens can just be dropped rather than renewed at the new server.
 *
 * The function is called when the server comes back or when a fileset is 
 * moved to another server. 
 * The function must be called with a WRITE LOCK held.  
 */
#ifdef SGIMIPS
static int cm_RecoverSCacheToken(
  struct cm_scache *scp,
  struct cm_server *startServerp,
  long *flagp,
  int  renew, /* boolean */
  int *tsrModeP,
  struct tsrSummary *tsrP)
#else  /* SGIMIPS */
static int cm_RecoverSCacheToken(scp, startServerp, flagp, renew, tsrModeP, tsrP)
  struct cm_scache *scp;
  struct cm_server *startServerp;
  long *flagp;
  int  renew; /* boolean */
  int *tsrModeP;
  struct tsrSummary *tsrP;
#endif /* SGIMIPS */
{
    struct lclHeap {
	afsFetchStatus OutStatus;
	struct cm_rrequest rreq;
	afs_token_t Token;
	afsVolSync tsync;
	afs_recordLock_t afsBlocker;
	afs_hyper_t versionNumber, oldTokenID, tType;
	struct cm_tokenList tokenl;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    struct cm_volume *volp = 0;
    struct cm_conn *connp;
    struct cm_server *serverp = 0;
    struct cm_tokenList *tlp, *nlp;
    long retry, srvix, code = 0, more = 0;
    u_long startTime, now;
    long rcode;
    unsigned32 service, st, thisFlag, mask;
    int safety, needPreserve;

    icl_Trace1(cm_iclSetp, CM_TRACE_RECOVERTOKENS, ICL_TYPE_LONG, renew);

    /*
     * Check whether this SCP has any tokens that are granted from the
     * server in question.  Do this early, before the ``renew'' check, so that
     * we don't gratuitously clear our caches in the renew==0 case.
     */
    now = osi_Time();
    for(tlp = (struct cm_tokenList *) scp->tokenList.next;
	tlp != (struct cm_tokenList *) &scp->tokenList;
	tlp = (struct cm_tokenList *) tlp->q.next) {
	if (!(tlp->flags & CM_TOKENLIST_DELETED)
	    && (tlp->serverp == startServerp)
	    && (tlp->token.expirationTime >= now)) {
	    break;
	}
    }
    if (tlp == (struct cm_tokenList *) &scp->tokenList) {
	/* Didn't break the loop above: all tokens either deleted or on
	   different servers. */
	return 0;
    }

#ifdef AFS_SMALLSTACK_ENV
    /* Keep stack frame small by doing heap-allocation */
    lhp = (struct lclHeap *)osi_AllocBufferSpace();
#ifdef SGIMIPS
#pragma set woff 1171
#endif
    /* CONSTCOND */
    osi_assert(sizeof(struct lclHeap) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif
#endif /* AFS_SMALLSTACK_ENV */

    cm_InitUnauthReq(&lhp->rreq);	/* use a un-authen binding */
    cm_SetConnFlags(&lhp->rreq, CM_RREQFLAG_NOVOLWAIT);
    /*
     * This is the TSR interface, where we want to re-establish exactly those
     * tokens that we believe we have already, without an optimistic grant and
     * without causing revocations
     */
    *flagp |= AFS_FLAG_RETURNTOKEN | AFS_FLAG_NOOPTIMISM | AFS_FLAG_NOREVOKE;
    lhp->versionNumber = scp->m.DataVersion;
    service = SEC_SERVICE(SRT_FX);
    lock_ObtainRead(&cm_scachelock);
    /* Was held exactly once by recoverer; if count==1, was junk. */
#ifdef SGIMIPS
    needPreserve = (CM_RC(scp) > 2);
#else  /* SGIMIPS */
    needPreserve = (scp->v.v_count > 1);
#endif /* SGIMIPS */
    lock_ReleaseRead(&cm_scachelock);

    /* 
     * At this point, assuming the scp may not have token from that serverp
     */

    /* if we're not renewing the tokens for this file, then invalidate
     * all of the appropriate caches by using ForceReturnToken.  If we
     * *are* renewing tokens, then we'll try to get the appropriate
     * tokens, and if we fail, we'll mark the scache as bad (which will
     * prevent our use of any of these caches, anyway).  If we succeed
     * at getting the tokens back, then our caches are still valid.
     *
     * Since we don't mark dirs as bad if we fail to get the tokens back,
     * we have to invalidate their contents (and listings) manually here.
     */
    if (!renew) {
	/* if we have a crash, we don't want to send the tokens
	 * back to the server (since it has none), otherwise
	 * we do, since in move or partition case, the original
	 * server is still keeping track of the tokens.
	 */
	cm_ForceReturnToken(scp, TKN_UPDATE,
			    ((*tsrModeP & TSR_CRASH)? 1 : 0));
    }
    /* Remove all cached name entries in this dir */
    if (cm_vType(scp) == VDIR) {
	/*
	 * We're claiming scp is stable, even though it isn't.  Since
	 * this is a dir, all that could happen is that we might
	 * be fetching the listing, but cm_InvalidateAllSegments
	 * can handle that race condition (via the FETCHINVAL flag).
	 * We can't allow InvalidateAllSegments to call StabilizeSCache,
	 * since we're running at callback level and can't wait for
	 * fetches.
	 */
	cm_InvalidateAllSegments(scp, CM_INVAL_SEGS_DATAONLY);
	nh_delete_dvp(scp);
	scp->dirRevokes++;
	AFS_hzero(scp->bulkstatdiroffset);
    }

    /* set revalidate flag on all tokens that need to be reestablished.
     * we leave them in the main list, since people may continue
     * to use the tokens while they're there, since while tokens
     * are being moved, they're still valid.  We don't bother to clear
     * the REVALIDATE flags, since we'll just be setting them the next
     * time we go through this code.
     * Basically, the only tokens w/o REVALIDATE set will be ones
     * added by this code.
     */
    for(tlp = (struct cm_tokenList *) scp->tokenList.next;
	tlp != (struct cm_tokenList *) &scp->tokenList;
	tlp = (struct cm_tokenList *) tlp->q.next) {
	tlp->flags |= CM_TOKENLIST_REVALIDATE;
    }

    /* Let's restore tokens for this file, one by one.  Even if something
     * goes wrong, we'll keep moving/validating the rest of the tokens.
     */
    rcode = 0;	/* code to return: first non-zero code from GetToken */
    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *)&scp->tokenList; tlp = nlp) {

	/* default value, will be recomputed by some code below, too */
	nlp = (struct cm_tokenList *) tlp->q.next;

	/* async grant tokens are simply cancelled; we wake up the
	 * guy and tell him that the token is gone; he'll retry
	 * when this happens, getting the token from the right
	 * server.
	 */
	if (tlp->flags & CM_TOKENLIST_ASYNCWAIT) {
	    tlp->flags &= ~CM_TOKENLIST_ASYNCWAIT;
	    tlp->flags |= CM_TOKENLIST_DELETED;
	    osi_Wakeup((char *) &tlp->flags);
	    continue;
	}

	/*
	 * if it is deleted, or not from the server for which we're
	 * doing the cleanup, or this token is new since we started the
	 * validation (and was thus added by this code), then skip this token.
	 */
	if (((tlp->flags & (CM_TOKENLIST_DELETED | CM_TOKENLIST_REVALIDATE))
	     != CM_TOKENLIST_REVALIDATE)
	    || (tlp->serverp != startServerp)) {
	    continue;
	}

	/* now hold the token since we'll be dropping the vnode lock */
	cm_HoldToken(tlp);

	/* Check for redundancy. */
	cm_StripRedundantTokens(scp, tlp, AFS_hgetlo(tlp->token.type),
				(tlp->token.expirationTime < (osi_Time()+600)),
				(*tsrModeP & TSR_CRASH) ? 0 : 1);
	/* See if our current CM state still needs this token. */
	if (!AFS_hiszero(tlp->token.type)) {
	    mask = 1;
	    lock_ReleaseWrite(&scp->llock);
	    for (srvix = 0; srvix < 32; srvix++, mask = mask<<1) {
		AFS_hset64(lhp->tType, 0, mask);
		if (AFS_hgetlo(lhp->tType) & AFS_hgetlo(tlp->token.type)) {
		    /* This does local-only work--no storebacks! */
		    (void) cm_RevokeOneToken(scp, &tlp->token.tokenID,
					     &lhp->tType, (afs_recordLock_t *) 0,
					     (afs_token_t *) 0, (afs_token_t *)0,
					     &more /* dummy */, 1);
		}
	    }
	    lock_ObtainWrite(&scp->llock);
	    /* Get a fresh nlp since we unlocked. */
	    nlp = (struct cm_tokenList *) tlp->q.next;
	}
	if (tlp->flags & CM_TOKENLIST_DELETED) {
	    /* Stripper or revoker was able to kill it off. */
	    cm_ReleToken(tlp);
	    continue;
	}

	/* We must free this token from this scache to
	 * avoid confusion in the future.  It won't get freed because
	 * we have a hold on it (above).  In all branches, we allocate
	 * a new token.
	 */
	tlp->flags |= CM_TOKENLIST_DELETED;
	code = 0;
	safety = 0;			/* safety for each token */
	more = renew;
	if (needPreserve && (AFS_hgetlo(tlp->token.type) & TKN_OPEN_PRESERVE))
	    more = 1;		/* Recover this one even if file not open */
	/*
	 * this is really an "if" statement, using "break" as a
	 * way of getting to the end quickly.
	 */
	while (more) {			/* really "if (renew) ..." */
	    lhp->oldTokenID = tlp->token.tokenID;
	    /* Initialize the flags we'll be using for this token */
#ifdef SGIMIPS
	    thisFlag = *(unsigned32 *)flagp;
#else  /* SGIMIPS */
	    thisFlag = *flagp;
#endif /* SGIMIPS */

	    do { /* retry loop, should flags to AFS_GetToken have to change */
		retry = 0;
		/*
		 * Abandon ship on this token if we can't guarantee that a
		 * LOCK token will have been preserved.
		 * It's a bit brutal, perhaps, but safe.
		 */
		if (!(thisFlag & (AFS_FLAG_MOVE_REESTABLISH | AFS_FLAG_TOKENID))
		    && (&scp->lockList != QNext(&scp->lockList))
		    && (AFS_hgetlo(tlp->token.type) &
			(TKN_LOCK_READ|TKN_LOCK_WRITE))) {
		    /*
		     * We're not able to get tokens by ID and we're not doing
		     * move recovery, yet we have a lock held.  Abandon this
		     * one, since it might be one of the lock tokens.
		     * Someday, this could check on the token ranges, so that
		     * it wouldn't have to abandon tokens if this lock token
		     * weren't being used to cover a lock that had to be held.
		     */
		    icl_Trace3(cm_iclSetp, CM_TRACE_TOKENTSRNOLOCKS,
				ICL_TYPE_FID, &scp->fid,
				ICL_TYPE_LONG, thisFlag,
				ICL_TYPE_LONG, AFS_hgetlo(tlp->token.type));
		    code = ESTALE;
		    if (!rcode) {
			/* If TOKENID was originally set, assume the
			 * competitor (token lost to a competitor due to
			 * partition) case, rather than that tokens were
			 * discarded in a server crash.
			 */
			cm_tagTSR(tsrP,
				  ((*flagp & AFS_FLAG_TOKENID) ?
				   tsrSum_Partition : tsrSum_Crash),
				  scp);
		    }
		    break;	/* break the retry loop */
		}

		lock_ReleaseWrite(&scp->llock);
		do {/* do an rpc call */
		    if (safety++ > 10)
			osi_Wait(1000, 0, 0);	/* for performance reasons */
		    retry = 0;
		    serverp = 0;
		    bzero((caddr_t)&lhp->Token, sizeof(lhp->Token));
		    cm_StartTokenGrantingCall();
		    /*
		     * In order to avoid a possible starvation problem, 
		     * get tokens via the secondary service.
		     */
		    if (connp = cm_Conn(&scp->fid,  service, 
					&lhp->rreq, &volp, &srvix)) {
			icl_Trace2(cm_iclSetp, CM_TRACE_STARTGETTOKENTSR,
				   ICL_TYPE_LONG, AFS_hgetlo(tlp->token.type),
				   ICL_TYPE_HYPER, &lhp->versionNumber);
			startTime = osi_Time();
			st = BOMB_EXEC("COMM_FAILURE",
					(osi_RestorePreemption(0),
					 st = AFS_GetToken(connp->connp,
						&scp->fid, &tlp->token,
						&osi_hZero, thisFlag,
						&lhp->Token, &lhp->afsBlocker,
						&lhp->OutStatus, &lhp->tsync),
					 osi_PreemptionOff(),
					 st));
			code = osi_errDecode(st);
			serverp = connp->serverp;
			DFS_ADJUST_LOCK_EXT(&lhp->afsBlocker, serverp);
			cm_SetReqServer(&lhp->rreq, serverp);
			icl_Trace3(cm_iclSetp, CM_TRACE_ENDGETTOKENTSR,
				   ICL_TYPE_LONG, code, 
				   ICL_TYPE_LONG, AFS_hgetlo(lhp->Token.type),
				   ICL_TYPE_HYPER, &lhp->OutStatus.dataVersion);
		    } else
			code = -1;
		    /*
		     * Should not retry upon receiving a token conflict error.
		     * Otherwise, the TSR work might never finish.
		     */
		    if (code == TKM_ERROR_TOKENCONFLICT)
			code = TKM_ERROR_HARDCONFLICT;
		} while (cm_Analyze(connp, code, &scp->fid, &lhp->rreq, volp,
				    srvix, startTime));
		/*
		 * If we cannot get the same or comparable token back, then we
		 * have a problem. Let the caller decide what to do. 
		 */
		lock_ObtainWrite(&scp->llock);    
		cm_EndTokenGrantingCall(&lhp->Token, serverp, scp->volp, startTime);
		/* In several cases, set up for a retry. */
		switch (code) {
		case 0:
		    /* First, check whether cm_EndTokenGrantingCall left us
		     * with the token that we really wanted.
		     */
		    if (!AFS_hsame(lhp->Token.type, tlp->token.type)) {
			/*
			 * Whoops--some tokens were revoked.
			 * Handle this as if there had been a conflict:
			 * don't retry, or TSR might never finish.
			 */
			icl_Trace2(cm_iclSetp, CM_TRACE_TSRRACINGREVOKE,
				   ICL_TYPE_FID, &scp->fid,
				   ICL_TYPE_HYPER, &lhp->Token.tokenID);
			code = ESTALE;
			tlp->flags |= CM_TOKENLIST_DELETED;
			break;
		    }
		    /* A ok !*/
		    break;
		case TKM_ERROR_INVALIDID:
		case TKM_ERROR_TOKENINVALID:
		    if (!(thisFlag & AFS_FLAG_TOKENID))
			break;
		    /* Try again without insisting on the same-ID token. */
		    retry = 1;
		    thisFlag &= ~AFS_FLAG_TOKENID;
		    icl_Trace2(cm_iclSetp, CM_TRACE_TRYWITHOUTID,
				ICL_TYPE_FID, &scp->fid,
				ICL_TYPE_LONG, thisFlag);
		    break;
		case FSHS_ERR_SERVER_UP:
		    /*
		     * The fx server's "recovery period" must have expired.
		     * Renew token without re-establish flag.
		     */
		    if (!(thisFlag & AFS_FLAG_SERVER_REESTABLISH))
			break;
		    retry = 1;
		    thisFlag &= ~AFS_FLAG_SERVER_REESTABLISH;
		    /* Want to affect this whole crash-recovery process, too. */
		    *flagp &= ~AFS_FLAG_SERVER_REESTABLISH;
		    icl_Trace2(cm_iclSetp, CM_TRACE_SERVERPOSTCRASH,
				ICL_TYPE_FID, &scp->fid,
				ICL_TYPE_LONG, thisFlag);
		    break;
		case FSHS_ERR_MOVE_FINISHED:
		    /* 
		     * The target server is no longer in a TSR-MOVE mode. 
		     */
		    if (!(thisFlag & AFS_FLAG_MOVE_REESTABLISH))
			break;
		    retry = 1;
		    thisFlag &= ~AFS_FLAG_MOVE_REESTABLISH;
		    /* Want to affect this whole TSR-move process, too. */
		    *flagp &= ~AFS_FLAG_MOVE_REESTABLISH;
		    icl_Trace3(cm_iclSetp, CM_TRACE_TSRVOLMOVEDONE,
				   ICL_TYPE_FID, &scp->fid,
				   ICL_TYPE_LONG, thisFlag,
				   ICL_TYPE_LONG, *tsrModeP);
		    if (!(*tsrModeP & TSR_KNOWCRASH)) {
			*tsrModeP |= TSR_KNOWCRASH;
			cm_printf("dfs: TSR failed, fileset %u,,%u, fid %u: \
move ended prematurely!\n",
				  AFS_HGETBOTH(scp->fid.Volume),
				  scp->fid.Vnode);
		    }
		    /* FALL THROUGH */
		default:
		    tlp->flags |= CM_TOKENLIST_DELETED;
		} /* switch */

	    } while (retry);

	    if (code)
		break;	/* break the while(more) loop */

	    /* If the data version has been changed, someone else must
	     * have changed this while the CM was separated from the
	     * server.
	     *
	     * Note, by the way, that we use OutStatus.* without checking
	     * that OutStatus.fileType != Invalid.  This is important:
	     * we don't return definitive status from AFS_GetToken unless
	     * a TKN_STATUS_READ token is returned, but that may not be
	     * the case when renewing a token, obviously.  However, we
	     * can compare the mtime and ctime, since if someone issued
	     * a conflicting operation while we were disconnected, then
	     * either they've already sent the update back to the server
	     * before the token was obtained, in which case the c/mtime
	     * will be seen to have changed, or they haven't, in which case
	     * the token obtain call will fail.
	     *
	     * Note also that even when fileType == Invalid, we still
	     * fill in the other fields of the file status at the
	     * server for this RPC.  This, too, is important, since
	     * we use those fields below.
	     *
	     * The data version and ctime checks are valid for crash 
	     * cleanup, but not for normal fileset moves.  In the
	     * normal fileset-move case, this CM might not have been the CM
	     * that last updated the ctime or the DV, but its tokens (e.g.
	     * data token on some sub-piece of the file) might be perfectly
	     * valid.  Check for a normal move via the TSR_KNOWCRASH flag and
	     * by seeing if the token in hand was granted using the
	     * AFS_FLAG_MOVE_REESTABLISH option bit.
	     * Similarly, check for the partition-only case; if we got exactly
	     * the same token back in post-partition recovery, the ctime/DV
	     * checks are irrelevant.
	     */
	    if (
		/* FALSE if this is a normal move: */
		(!(thisFlag & AFS_FLAG_MOVE_REESTABLISH)
		 || !(*tsrModeP & (TSR_MOVE | TSR_CRASHMOVE))
		 || (*tsrModeP & TSR_KNOWCRASH))
		&&
		/* FALSE if this is normal reconnection: */
		(!(thisFlag & AFS_FLAG_TOKENID)
		 || !AFS_hsame(lhp->oldTokenID, lhp->Token.tokenID)
		 || (*tsrModeP & (TSR_CRASH | TSR_KNOWCRASH)))
		) {

		if (!AFS_hsame(lhp->versionNumber, lhp->OutStatus.dataVersion)) {
		    icl_Trace3(cm_iclSetp, CM_TRACE_TOKENTSRDV,
				ICL_TYPE_FID, &scp->fid,
				ICL_TYPE_HYPER, &lhp->versionNumber,
				ICL_TYPE_HYPER, &lhp->OutStatus.dataVersion);
		    code = ESTALE;
		    if (!rcode)
			cm_tagTSR(tsrP, tsrSum_Version, scp);
		    break;     /* break the while(more) loop */
		}
		if ((scp->modChunks > 0 || scp->m.ModFlags & CM_MODMASK) &&
		    cm_tcmp(scp->m.ServerChangeTime, lhp->OutStatus.changeTime) != 0) {
		    icl_Trace3(cm_iclSetp, CM_TRACE_TOKENTSRBADSCP,
				ICL_TYPE_FID, &scp->fid,
				ICL_TYPE_LONG, scp->modChunks,
				ICL_TYPE_LONG, scp->m.ModFlags);
		    icl_Trace4(cm_iclSetp, CM_TRACE_TOKENTSRCTIME,
				ICL_TYPE_LONG, scp->m.ServerChangeTime.sec,
				ICL_TYPE_LONG, scp->m.ServerChangeTime.usec,
				ICL_TYPE_LONG, lhp->OutStatus.changeTime.sec,
				ICL_TYPE_LONG, lhp->OutStatus.changeTime.usec);
		    code = ESTALE;
		    if (!rcode)
			cm_tagTSR(tsrP, tsrSum_Ctime, scp);
		    break;     /* break the while(more) loop */
		}
	    }

	    cm_MergeStatus(scp, &lhp->OutStatus, &lhp->Token, 0, &lhp->rreq);
	    if (AFS_hgetlo(lhp->Token.type) & TKN_OPEN_PRESERVE)
		needPreserve = 0;
	    if (!AFS_hsame(tlp->token.tokenID, lhp->Token.tokenID)) {
		/* 
		 * If not the same tokenID, and DataVersion is still correct, 
		 * let's update the on line dcache with new token IDs
		 */
		if (AFS_hgetlo(tlp->token.type) & TKN_DATA_READ)
		    cm_UpdateDCacheOnLineState(scp, &tlp->token, &lhp->Token);
	    }
	    more = 0;
	} /* while (more) */

	/* now, we've done our best to re-establish this token.  If
	 * we failed, remember the first failing code in rcode to
	 * return to our caller.
	 */
	if (!rcode) rcode = code;

	/* recompute nlp since the tokenList may have changed while the
	 * vnode lock was released.
	 */
	nlp = (struct cm_tokenList *) tlp->q.next;

	/* 
	 * Send token to the source server if we are doing a fileset move 
	 * or free this if we are doing a token recovery from a server to 
	 * avoid adding extra load to the server.
	 */
	if (*tsrModeP & (TSR_MOVE | TSR_CRASHMOVE)) {
	    lhp->tokenl.tokenFid = tlp->tokenFid;
	    lhp->tokenl.token.tokenID = tlp->token.tokenID;
	    lhp->tokenl.token.expirationTime = 0x7fffffff;
	    lhp->tokenl.token.type = tlp->token.type;
	    cm_QueueAToken(startServerp, &lhp->tokenl, 1);
	}
	cm_ReleToken(tlp);
#ifdef SGIMIPS
        /*
         * We might be called by cm_ConnAndReset and things may have
         * gone badly.  If tsrError is set there is no point in going
         * on, everything will fail.  We simply want to get out of here
         * so that another cm_ConnAndReset can happen.
         */
        if (lhp->rreq.tsrError)
            break;
#endif /* SGIMIPS */
    } /* for */
    if (volp)
	cm_EndVolumeRPC(volp);
    rcode = cm_CheckError(rcode, &lhp->rreq);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
#ifdef SGIMIPS
    return (int)rcode;
#else  /* SGIMIPS */
    return rcode;
#endif /* SGIMIPS */
}

/* called with write-locked scp, and types to return.  Invalidates
 * caches covered by specified token, and returns guaranteeing
 * that no such tokens are held any longer.
 *
 * Only works for data and status tokens for now, since those
 * always come back.
 *
 * If discardToken is set, we don't return the token to the server.
 */
static int cm_forceTypes[] = {TKN_STATUS_READ, TKN_STATUS_WRITE,
			TKN_DATA_READ, TKN_DATA_WRITE, 0};
#ifdef SGIMIPS
cm_ForceReturnToken(
  struct cm_scache *scp,
  long atype,
  int discardToken)
#else  /* SGIMIPS */
cm_ForceReturnToken(scp, atype, discardToken)
  struct cm_scache *scp;
  long atype;
  int discardToken;
#endif /* SGIMIPS */
{
    struct cm_tokenList *tlp;
    struct cm_tokenList *nlp;
    long cmask;
    int i;
    long code;
    struct cm_tokenList tokenl;
    long dummyFlags;
    int counter = 0;

  retry:
    if (++counter > 1000) {
	cm_printf("dfs: ForceReturn looping\n");
	return -1;
    }
    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *) &scp->tokenList; tlp = nlp) {

	/* compute next pointer early */
	nlp = (struct cm_tokenList *) tlp->q.next;

	/* if this is a deleted, or ungranted token, don't process it */
	if (tlp->flags & (CM_TOKENLIST_DELETED|CM_TOKENLIST_ASYNC)) {
	    continue;
	}

	for(i=0;;i++) {
	    cmask = cm_forceTypes[i];
	    if (cmask == 0) break;	/* no more types */
	    if (!(cmask & atype & AFS_hgetlo(tlp->token.type)))
		continue;

	    /* otherwise, we have a relevant token, do one revoke, and
	     * then queue the token for return.
	     */
	    tokenl.tokenFid = tlp->tokenFid;
	    tokenl.token.tokenID = tlp->token.tokenID;
	    tokenl.token.expirationTime = 0x7fffffff;
	    AFS_hset64(tokenl.token.type, 0, cmask);

	    /* don't remove the token from the list until we're
	     * back from RevokeOneToken, since it doesn't do anything
	     * if the token is already gone.  Hold the token
	     * so we can get rid of it later.
	     */
	    cm_HoldToken(tlp);
	    lock_ReleaseWrite(&scp->llock);
	    code = cm_RevokeOneToken(scp, &tokenl.token.tokenID,
				     &tokenl.token.type, (afs_recordLock_t *) 0,
				     (afs_token_t *) 0, (afs_token_t *) 0,
				     &dummyFlags, 0);
	    /* we got the token from the cache manager, and cleared
	     * the appropriate caches.  Now tell the server that
	     * we're not using the token any more.  Even if something
	     * goes wrong with revoke, we return the token anyway, since
	     * the caches are cleared, even if unsuccessfully (e.g.
	     * a write-back may have failed).
	     */
	    if (!discardToken) cm_QueueAToken(tlp->serverp, &tokenl, 0);
	    lock_ObtainWrite(&scp->llock);
	    /* now remove it, just in case.  Should already be gone,
	     * of course, since RevokeOneToken should have revoked it.
	     */
	    AFS_HOP32(tlp->token.type, &~, cmask);
	    /* don't look at tlp after this next line */
	    if (AFS_hiszero(tlp->token.type))
		cm_DelToken(tlp);
	    cm_ReleToken(tlp);		/* may delete it now */
	    goto retry;
	}
	/* if we get here, we haven't released the lock, so
	 * nlp and tlp are still good. */
    }
    /* if we make it here, we've got all of the tokens back
     * of the type(s) requested.  We're done.
     */
    return 0;
}

/* Pretend that token was revoked, handling token shutdown procedure
 * for relevant tokens (e.g. invalidate VM for data tokens).
 * Used when we realize that we lost a token (e.g. token expiration)
 * but weren't called in our revoke procedure.
 *
 * Called with locked vnode, temporarily unlocks it
 */
#ifdef SGIMIPS
int
cm_SimulateTokenRevoke(
  register struct cm_scache *scp,
  struct cm_tokenList *tlp,
  int *anyLeftp)
#else  /* SGIMIPS */
cm_SimulateTokenRevoke(scp, tlp, anyLeftp)
  register struct cm_scache *scp;
  struct cm_tokenList *tlp;
  int *anyLeftp;
#endif /* SGIMIPS */
{
    long mask;
    int i;
    int anyLeft;
    long code, rcode;
    long dummyFlags;
    afs_hyper_t baseType;
    register struct cm_server *tserverp;

    icl_Trace2(cm_iclSetp, CM_TRACE_SIMTOKENREVOKE,
	       ICL_TYPE_POINTER, (long) scp,
	       ICL_TYPE_LONG, AFS_hgetlo(tlp->token.type));
    mask = 1;
    anyLeft = 0;
    rcode = 0;
    lock_ReleaseWrite(&scp->llock);
    for(i=0; i<32; i++, mask = mask<<1) {
	AFS_hset64(baseType, 0, mask);
	if (AFS_hgetlo(baseType) & AFS_hgetlo(tlp->token.type)) {
	    code = cm_RevokeOneToken(scp, &tlp->token.tokenID,
			&baseType, (afs_recordLock_t *) 0,
			(afs_token_t *) 0, (afs_token_t *)0, &dummyFlags, 0);
	    /* If we fail to revoke TKN_OPEN_PRESERVE, it's not an error. */
	    if (code && mask == TKN_OPEN_PRESERVE) {
		code = 0;
		anyLeft = 1;
	    }
	    /* return first error, if any */
	    if (!rcode) rcode = code;
	}
    }
    lock_ObtainWrite(&scp->llock);
    if (rcode) {
	/* something went wrong, so check to see
	 * if the server is up or down.  If it is
	 * up, we treat it as a failed TSR would
	 * be handled: mark the scache as bad.
	 * Otherwise, we let the normal TSR
	 * process handle things when the server
	 * comes back up.  If we always marked it as
	 * bad, we could mess up what would be a normal
	 * TSR by failing to renew a token.
	 */
	anyLeft = 0;
	if (tserverp = tlp->serverp) {
	    lock_ObtainRead(&tserverp->lock);
	    code = tserverp->states & SR_DOWN;
	    lock_ReleaseRead(&tserverp->lock);
	}
	else code = 0;	/* assume server up */
	/* if server up, mark scache bad now,
	 * rather than waiting for TSR (since
	 * it will not happen).
	 */
	if (code == 0)
	    cm_MarkBadSCache(scp, ESTALE);
    }
    else {
	/* all tokens are back; this is our opportunity to actually
	 * reclaim this tlp entry, if we're done with it.  No
	 * need to check rights, since this is an expired entry, and
	 * all bits are irrelevant.  Furthermore, we've already made
	 * an attempt, above, to call RevokeOneToken on each token
	 * here.
	 */
	if (!anyLeft)
	    cm_DelToken(tlp);
    }
    *anyLeftp = anyLeft;
#ifdef SGIMIPS
    return (int)rcode;
#else  /* SGIMIPS */
    return rcode;
#endif /* SGIMIPS */
}
