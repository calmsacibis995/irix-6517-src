/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_recycle.c,v 65.5 1998/04/01 14:16:32 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/* Copyright (C) 1996, 1994 Transarc Corporation - All rights reserved. */

/*
 * Routines to handle token allocation and recycling
 */

#include "tkm_internal.h"
#include "tkm_revokes.h"
#include "tkm_conflicts.h"
#include "tkm_recycle.h"
#include "tkm_misc.h"
#include "tkm_file.h"
#include "tkm_volume.h"
#ifdef SGIMIPS
#include "tkm_asyncgrant.h"
#include "tkm_filetoken.h"
#include "tkm_tokenlist.h"
#include "tkm_voltoken.h"
#endif

static osi_dlock_t   tkm_freeTokensLock; /* protects the following 5 */
static tkm_internalToken_t   *tkm_freeTokens; /* list of free tokens*/
static int	tkm_freeTokensGcFailed = 0;  /*
				  * set by the gc thread when it finds that
				  * all of the tokens in the tkm_tkm_gcList have
				  * been revoked and the revocations have
				  * been refused.
				  * Protectect by tkm_freeTokensLock
				  */
static int tkm_numOfFreeTokens;

#ifdef KERNEL
#include <dcedfs/icl.h>
#include <dcedfs/tkm_trace.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_recycle.c,v 65.5 1998/04/01 14:16:32 gwehrman Exp $")

static struct icl_set *tkm_recycleIclSet;

static int tkm_maxTokens = 10000; /* maximum tokens allowed */
static int tkm_minTokens = 100;
#define TKM_BULK_TOKENS 100
#else /* KERNEL */
static int tkm_maxTokens = 100; /* maximum tokens allowed */
static int tkm_minTokens = 5;
#define TKM_BULK_TOKENS 1
#endif /* KERNEL */



static osi_dlock_t		tkm_gcListLock; /* protects the next 2 */
static tkm_internalToken_p 	tkm_gcListHead = NULL;
static tkm_internalToken_p 	tkm_gcListTail = NULL;

static osi_dlock_t	 lastTokenIdLock;
static afs_hyper_t lastTokenId;

static osi_dlock_t tkm_tokenCounterLock;
static int tkm_tokenCounter; /* total number of allocated tokens in the system */


extern long tkm_expiration_default;

#ifdef SGIMIPS
static void tkm_ExpirationIntervalCheck(void);
static void tkm_CleanExpiredFileTokens(void);
static void tkm_CleanExpiredVolTokens(void);
static void tkm_PeriodicCleanUp(void);
static void tkm_TokenGcThread(void);
static void tkm_PutTokenInFreeList(tkm_internalToken_t *tokenP);
static void tkm_SetToNextTokenId(tkm_internalToken_t *tokenP);
static tkm_internalToken_t *tkm_GetFreeToken(void);
#endif

static void tkm_ExpirationIntervalCheck()
{
    int tokensNeeded;
    u_long curInterval;
    int availTokens;

    /*
     * Check to see how things look like and if necessary
     * wakeup the GC thread and adjust the expiration time
     */
    lock_ObtainWrite(&tkm_freeTokensLock);
    lock_ObtainWrite(&tkm_tokenCounterLock);
    availTokens = tkm_numOfFreeTokens /* how many are on the free list */
	+ (tkm_maxTokens - tkm_tokenCounter)/* how many we can allocate*/;
    curInterval = tkm_SetTokenExpirationInterval(-1);
    if (availTokens <= tkm_minTokens) {
	/*
	 * We have to revoke some tokens so we'll wakeup the GC thread
	 * and set the token expiration interval to the lowest possible
	 */
	tkm_SetTokenExpirationInterval(0);
	lock_ReleaseWrite(&tkm_freeTokensLock);
	lock_ReleaseWrite(&tkm_tokenCounterLock);
	icl_Trace1(tkm_recycleIclSet, TKM_TRACE_ADJUST_EXPTIME, ICL_TYPE_LONG,
		   tkm_SetTokenExpirationInterval(-1));
	osi_Wakeup(&tkm_gcListHead);
    } else {
	/* consider adjusting the expiration time */
	if (availTokens < (tkm_maxTokens/4)) {
	    curInterval -= (curInterval / 2); /* 50% decrease */
	    tkm_SetTokenExpirationInterval(curInterval);
	    icl_Trace1(tkm_recycleIclSet, TKM_TRACE_ADJUST_EXPTIME,
		       ICL_TYPE_LONG,  curInterval);
	} else if (curInterval < tkm_expiration_default) {
	    curInterval += (curInterval / 4); /* ~25% increase */
	    icl_Trace1(tkm_recycleIclSet, TKM_TRACE_ADJUST_EXPTIME,
		       ICL_TYPE_LONG,  curInterval);
	    tkm_SetTokenExpirationInterval(curInterval);
	}
	lock_ReleaseWrite(&tkm_freeTokensLock);
	lock_ReleaseWrite(&tkm_tokenCounterLock);
    }
}

static void tkm_CleanExpiredFileTokens()
{
   tkm_file_t *fileP, *prevP = NULL;
   tkm_internalToken_t *tokenP, *nextP;
   time_t myTime;

   myTime = osi_Time();
   for (fileP = tkm_GetFirstFile();
	fileP != NULL; fileP = tkm_GetNextFile(prevP)) {
       if (prevP != NULL)
	   tkm_ReleFile(prevP);
       lock_ObtainWrite(&(fileP->lock));
       for (tokenP = fileP->granted.list;
	    tokenP != NULL; tokenP = nextP) {
	   nextP = tokenP->next;
	   if (TKM_TOKEN_EXPIRED(tokenP)) {
	       lock_ObtainWrite(&(fileP->vol->lock));
	       TKM_RECYCLE_TOKEN(tokenP);
	       lock_ReleaseWrite(&(fileP->vol->lock));
	   } else if ((tokenP-> refused != 0) &&
	       (tokenP->lastRefused + TKM_REFUSED_VALID_TIME < myTime)) {
		   /*
		    * Maybe the client crashed before returning the token
		    * that we want. If there is anyone waiting try again
		    */
		   tokenP->refused = 0;
		   tkm_TriggerAsyncGrants(tokenP);
	       }
       }
       lock_ReleaseWrite(&fileP->lock);
       prevP = fileP;
   }
   /* release the last file from the above loop*/
   if (prevP != NULL)
       tkm_ReleFile(prevP);
}

static void tkm_CleanExpiredVolTokens()
{
   tkm_vol_t *volP, *prevP = NULL;
   tkm_internalToken_t *tokenP, *nextP;
   time_t myTime;

   myTime = osi_Time();
   for (volP = tkm_GetFirstVol();
	volP != NULL; volP = tkm_GetNextVol(prevP)) {
       if (prevP != NULL)
	   tkm_ReleVol(prevP);
       lock_ObtainWrite(&(volP->lock));
       for (tokenP = volP->granted.list;
	    tokenP != NULL; tokenP = nextP) {
	   nextP = tokenP->next;
	   if (TKM_TOKEN_EXPIRED(tokenP)) {
	       TKM_RECYCLE_TOKEN(tokenP);
	   } else if ((tokenP-> refused != 0) &&
	       (tokenP->lastRefused + TKM_REFUSED_VALID_TIME < myTime)) {
		   /*
		    * Maybe the client crashed before returning the token
		    * that we want. If there is anyone waiting try again
		    */
		   tokenP->refused = 0;
		   tkm_TriggerAsyncGrants(tokenP);
	       }

       }
       lock_ReleaseWrite(&volP->lock);
       prevP = volP;
   }
   /* release the last volume from the above loop*/
   if (prevP != NULL)
       tkm_ReleVol(prevP);
}

static void tkm_PeriodicCleanUp()
{
    int myInterval = (2*60);

    osi_PreemptionOff();
    while (1) {
	tkm_dprintf(("Started periodic_cleanup at %d\n", osi_Time()));
	icl_Trace1(tkm_recycleIclSet, TKM_TRACE_START_CLEANUP,
		   ICL_TYPE_LONG, osi_Time());
	tkm_CleanExpiredFileTokens();
	tkm_CleanExpiredVolTokens();
	tkm_ExpirationIntervalCheck();
	icl_Trace1(tkm_recycleIclSet, TKM_TRACE_END_CLEANUP,
		   ICL_TYPE_LONG, osi_Time());
	tkm_dprintf(("Finished periodic_cleanup at %d\n", osi_Time()));
	osi_Pause(myInterval);
    } /* while (1) */
}
/**************************************************************************
 *
 * tkm_TokenGcThread(): This is the single thread that does GC. When woken up
 *			it will keep running until there is at least
 *			tkm_minTokens tokens in the freeTokenList (or the
 *			difference could be allocated). It has to do
 *			some funny sequences of locking and unlocking once
 *			it finds a revocation candidate.
 ***************************************************************************/

static void tkm_TokenGcThread()
{
    tkm_internalToken_t *token, *lastHeldToken;
    tkm_tokenConflictQ_t *tokensToGC;
    int availTokens, tokensNeeded;
    tkm_file_t *fileP;
    tkm_vol_t *volP;
    afs_hyper_t tokenId;
    long tkm_lastGCWarning = 0;


    osi_PreemptionOff();
    while(1) {
	icl_Trace1(tkm_recycleIclSet, TKM_TRACE_START_GC, ICL_TYPE_LONG,
		   osi_Time());
	lock_ObtainWrite(&tkm_gcListLock);
	lock_ObtainWrite(&tkm_freeTokensLock);
	lock_ObtainWrite(&tkm_tokenCounterLock);
	availTokens = tkm_numOfFreeTokens /* how many are on the free list */
	    + (tkm_maxTokens - tkm_tokenCounter)/* how many we can allocate*/;
	if (availTokens >= tkm_minTokens){
	    lock_ReleaseWrite(&tkm_tokenCounterLock);
	    lock_ReleaseWrite(&tkm_freeTokensLock);
	    osi_SleepW(&tkm_gcListHead, &tkm_gcListLock);
	    continue;
	} else {
	    tokensNeeded = MAX((tkm_minTokens - availTokens),
			       TKM_BULK_TOKENS);
	    lock_ReleaseWrite(&tkm_tokenCounterLock);
	}
	token = tkm_gcListHead;
	tkm_dprintf(("found candidate %x id %u,,%u\n", token,
		     AFS_HGETBOTH(token->id)));
	icl_Trace1(tkm_recycleIclSet, TKM_TRACE_GCEXAM,
		   ICL_TYPE_HYPER, &(token->id));
	while (token != NULL){
	    if (token->refused == 0)
		/* we haven't tried to revoke this one yet */
		break;
	    else
		token = token->nextGC;
	}
	if (token == NULL) {
	    long curTime = osi_Time();
	    /*
	     * all of the tokens in the tkm_gcList have been already
	     * revoked and the revocations have been refused
	     */
	    icl_Trace1(tkm_recycleIclSet, TKM_TRACE_GCFAILED,
		       ICL_TYPE_LONG, tkm_numOfFreeTokens);
	    if ((tkm_lastGCWarning + TKM_TIME_BETWEEN_WARNINGS) < curTime){
		printf("TKM: out of tokens and none to GC\n");
		tkm_lastGCWarning = curTime;
	    }
	    tkm_freeTokensGcFailed = 1;
	    lock_ReleaseWrite(&tkm_freeTokensLock);
	    osi_Wakeup(&tkm_freeTokens);
	    osi_SleepW(&tkm_gcListHead, &tkm_gcListLock);
	    continue;
	} else {
	    lock_ReleaseWrite(&tkm_freeTokensLock);
	}
	icl_Trace2(tkm_recycleIclSet, TKM_TRACE_WILL_GC,
		   ICL_TYPE_HYPER, &(token->id),
		   ICL_TYPE_LONG, token->expiration);
	lastHeldToken = NULL;
	tkm_InitConflictQ(&tokensToGC, tokensNeeded);
	while (tokensToGC->lastPos < tokensNeeded){

	    tokenId = token->id;
	    /*
	     * we need to get the holder's lock before adding a token
	     * to the conflictQ, so we need to drop the gcListLock and
	     * reobtain it but once we drop the gcListLock the token
	     * might get recycled so we need to be very careful to not
	     * dereference it again until we are sure that it is still
	     * the same token
	     */
	    if (token->flags & TKM_TOKEN_ISVOL) {
		volP = token->holder.volume;
		tkm_HoldVol(volP);
		fileP = NULL;
	    } else {
		fileP = token->holder.file;
		tkm_HoldFile(fileP);
		volP = NULL;
	    }
	    lock_ReleaseWrite(&tkm_gcListLock);
	    if (fileP != NULL)
		lock_ObtainWrite(&(fileP->lock));
	    else
		lock_ObtainWrite(&(volP->lock));
	    lock_ObtainWrite(&tkm_gcListLock);
	    if (AFS_hsame(token->id,tokenId)){
		/*
		 * We can do the above test safely because tokens never
		 * get freed just recycled. Additionally when a token is
		 * put in the free list its id is zeroed and then when
		 * it is used again it gets assigned a brand new id (token
		 * ids are not recycled until the next reboot) so if the
		 * id is still the same the token has not been recycled
		 * while we had dropped the tkm_gcListLock
		 */
		tkm_AddToConflictQ(tokensToGC, token, token->types, 0);
		lastHeldToken = token;
	    }
	    if (fileP != NULL) {
		lock_ReleaseWrite(&(fileP->lock));
		tkm_ReleFile(fileP);
	    } else {
		lock_ReleaseWrite(&(volP->lock));
		tkm_ReleVol(volP);
	    }
	    /*
	     * Follow the nextGC pointer of the last token that we know
	     * hasn't been recycled, and follow it until we find one that
	     * we can revoke
	     */

	    if (lastHeldToken != NULL){
		token = lastHeldToken->nextGC;
	        while ((token != NULL) && (token->refused != 0))
		    token = token->nextGC;
	    } else {
		/*if we don't have any such token go to the top of the gcList*/
		token = tkm_gcListHead;
	        while ((token != NULL) && (token->refused != 0))
		    token = token->nextGC;
	    }
	    if (token == NULL) {
		/*
		 * we can't GC as many tokens as we wanted to
		 * but we'll revoke all the ones that have been gathered
		 * until now
		 */
		break;
	    }
	} /*
	   * end of while we haven't put as many tokens as we wanted to in the
	   * list of tokens to do parallel revokes on
	   */
	lock_ReleaseWrite(&tkm_gcListLock);
	tkm_ParallelRevoke(tokensToGC, TKM_PARA_REVOKE_FLAG_DOINGGC, NULL);
	/*
	 * If there are any tokens left in the conflict queue we have to
	 * release them, but since they all have different holders we can't
	 * just use tkm_DeleteConflictQ()
	 */
	tkm_DeleteConflictQUnlocked(tokensToGC);
	/*
	 * give other threads a chance to run
	 */
	osi_Pause(1);
    } /* while forever */
}

/*************************************************************************
 *
 * tkm_InitRecycle() : Initialize locks and global variables and start
 *		       the GC thread;
 *
 *************************************************************************/
#ifdef SGIMIPS
void tkm_InitRecycle(void)
#else
void tkm_InitRecycle()
#endif
{
    long code;

    lock_Init(&lastTokenIdLock);
    lock_ObtainWrite(&lastTokenIdLock);
    AFS_hset64(lastTokenId, osi_Time(), 0);
    lock_ReleaseWrite(&lastTokenIdLock);
    lock_Init(&tkm_tokenCounterLock);
    lock_ObtainWrite(&tkm_tokenCounterLock);
    tkm_tokenCounter = 0;
    lock_ReleaseWrite(&tkm_tokenCounterLock);
    lock_Init(&tkm_freeTokensLock);
    lock_Init(&tkm_gcListLock);
#ifdef KERNEL
    {
	struct icl_log *logp;
	code = icl_CreateLog("cmfx", 0, &logp);
	if (code == 0)
	    code = icl_CreateSetWithFlags("tkm/recycle", logp, 0,
					  ICL_CRSET_FLAG_DEFAULT_OFF,
					  &tkm_recycleIclSet);
	osi_assert(code == 0);
    }
#endif /* KERNEL */

#ifdef SGIMIPS
    osi_ThreadCreate(tkm_TokenGcThread, (void *)0, 0, 0,
#else
    code = osi_ThreadCreate(tkm_TokenGcThread, (void *)0, 0, 0,
#endif
		     "TKM_GC_Thread", code);
    osi_assert(code == 0);
#ifdef SGIMIPS
    osi_ThreadCreate(tkm_PeriodicCleanUp, (void *)0, 0, 0,
#else
    code = osi_ThreadCreate(tkm_PeriodicCleanUp, (void *)0, 0, 0,
#endif
		     "TKM_Periodic_Cleanup", code);
    osi_assert(code == 0);
}

int tkm_SetMaxTokens(int n)
{
    int old;
    tkm_Init();

    lock_ObtainWrite(&tkm_freeTokensLock);
    old = tkm_maxTokens;
    if (n > 0)
	tkm_maxTokens = n;
    lock_ReleaseWrite(&tkm_freeTokensLock);
    return (old);
}

int tkm_SetMinTokens(int n)
{
    int old;
    tkm_Init();
    lock_ObtainWrite(&tkm_freeTokensLock);
    old = tkm_minTokens;
    if (n > 0)
	tkm_minTokens = n;
    lock_ReleaseWrite(&tkm_freeTokensLock);
    return(old);
}

/*
 * tkm_freeTokensLock must be held
 */

static void tkm_PutTokenInFreeList(tkm_internalToken_t *tokenP)
{
    osi_assert(lock_WriteLocked(&tkm_freeTokensLock));
    AFS_hzero(tokenP->id);
    tokenP->next = tkm_freeTokens;
    tkm_freeTokens = tokenP;
    tkm_freeTokensGcFailed = 0;
    tkm_numOfFreeTokens++;
}


/***************************************************************************
 *
 * tkm_RemoveTypes(tokenP,types): remove types from tokenP->types
 *
 * LOCKING:  called with tokenP's holder locked. Calls HS_RELE() which
 * 	     means that the hs_host->lock must be lower than the tkm
 *	     locks
 *
 ***************************************************************************/
void tkm_RemoveTypes(tkm_internalToken_t *tokenP,
		     long types)
{
    osi_assert(TKM_TOKEN_HOLDER_LOCKED(tokenP));
    /*
     * only remove from the mask the types that are revoked AND are still
     * part of the token
     */
    if ((tokenP->types & types) != 0) {
	TKM_REMOVE_FROM_MASK(tokenP, (tokenP->types & types));
	/*
	 * since we are removing some types we might be able to
	 * grant some tokens. However we can't try the asyncgrant
	 * if the token returned is an asynchronously granted token
	 * (slice and dice or queued) because then we would recurse.
	 * The above is not a problem since there can be no tokens
	 * queued on an asynchronously granted token because any
	 * requests conflicting with asynchronously granted tokens
	 * block so all we need to do is wakeup those waiting.
	 */
	if (!(tokenP->flags & TKM_TOKEN_GRANTING)){
	    tkm_TriggerAsyncGrants(tokenP);
	} else {
	    osi_assert( !(tokenP->flags & TKM_TOKEN_ISVOL));
	    tokenP->flags &= ~TKM_TOKEN_GRANTING;
	    osi_Wakeup(&(tokenP->holder.file->granted));
	    tkm_dprintf(("RemoveTypes : doing wakeup on %x\n",
			 &(tokenP->holder.file->granted)));
	}
    }
    tokenP->types &= ~types;
    tokenP->refused &= ~types; /*
				* since they are no longer there they are
				* no longer refused
				*/
    if (tokenP->types == 0){
	if (tokenP->refcount == 0) {
	    if (tokenP->host != NULL)
		HS_RELE(tokenP->host);
	    if (tokenP->flags & TKM_TOKEN_ISVOL)
		tkm_ReturnVolToken(tokenP);
	    else
		tkm_ReturnFileToken(tokenP);

	    lock_ObtainWrite(&tkm_freeTokensLock);
	    tkm_PutTokenInFreeList(tokenP);
	    lock_ReleaseWrite(&tkm_freeTokensLock);
	    osi_Wakeup(&tkm_freeTokens);
	}
    }
}

static void tkm_SetToNextTokenId(tkm_internalToken_t *tokenP)
{
    lock_ObtainWrite(&lastTokenIdLock);
    AFS_hincr(lastTokenId);
    tokenP->id = lastTokenId;
    lock_ReleaseWrite(&lastTokenIdLock);
}



/*
 * tkm_freeTokensLock must be held
 */

static tkm_internalToken_t *tkm_GetFreeToken()
{
    tkm_internalToken_t *tmp = NULL;

    osi_assert(lock_WriteLocked(&tkm_freeTokensLock));
    if (tkm_freeTokens != NULL) {
	tmp = tkm_freeTokens;
	tkm_freeTokens = tkm_freeTokens -> next;
	bzero((caddr_t) tmp,sizeof(tkm_internalToken_t));
	tkm_SetToNextTokenId(tmp);
	tkm_numOfFreeTokens--;
    }
    return tmp;
}


/********************************************************************
 *
 * tkm_AllocTokenNoSleep(n, got): returns a single linked list of n
 *			free token. The elements of the list are
 *			linked via the next field of the tokens.
 *
 * Input Params : n = how many tokens to return
 * Output Params: got = how many tokens are returned
 *
 * LOCKING : will get and drop the tkm_freeTokensLock
 *******************************************************************/

tkm_internalToken_t *tkm_AllocTokenNoSleep(int n,
					   int *got)
{
    tkm_internalToken_t *tmp;
    tkm_internalToken_t *ret = NULL;

    *got = 0;
    lock_ObtainWrite(&tkm_freeTokensLock);
    while (*got < n) {
	if ((tmp = tkm_GetFreeToken()) != NULL){
	    (*got)++;
	    tmp->next = ret;
	    ret = tmp;
	} else {
	    lock_ObtainWrite(&tkm_tokenCounterLock);
	    if (tkm_tokenCounter <= tkm_maxTokens){
		tkm_internalToken_t *tmp;
		int i;

		tmp = (tkm_internalToken_t *)
		    osi_Alloc(sizeof(tkm_internalToken_t) * TKM_BULK_TOKENS);
		osi_assert(tmp != NULL);
		for (i = 0; i < TKM_BULK_TOKENS; i++) {
		    tkm_PutTokenInFreeList(tmp++);
		}
		tkm_tokenCounter += TKM_BULK_TOKENS;
		lock_ReleaseWrite(&tkm_tokenCounterLock);
	    } else {
		lock_ReleaseWrite(&tkm_tokenCounterLock);
		break;
	    }
	}
    }
    lock_ReleaseWrite(&tkm_freeTokensLock);
    return (ret);
}

/********************************************************************
 *
 *  tkm_AllocToken(n): returns a single linked list of n free token
 *		       The elements of the list are linked via the
 *		       next field of the tokens.
 *
 * LOCKING: Since this routine might sleep no other locks can be held
 *	    when it's called.
 *********************************************************************/
tkm_internalToken_t *tkm_AllocToken(int n)
{
    tkm_internalToken_t *tmp, *follow;
    tkm_internalToken_t *ret = NULL;
    int got = 0, more;

    while (got < n) {
	more = 0;
	tmp = tkm_AllocTokenNoSleep(n - got, &more);
	if (tmp != NULL){
	    if (ret == NULL)
		ret = tmp;
	    else {
		follow = ret;
		while (follow->next)
		    follow = follow->next;
		follow->next = tmp;
	    }
	    got += more;
	} else {
	    lock_ObtainWrite(&tkm_freeTokensLock);
	    /*
	     * tkm_freeTokensGcFailed gets set by the GC thread and unset
	     * whenever a token gets added to the free list
	     */
	    if (tkm_freeTokensGcFailed) {
		/*
		 * if we got here we can't get all the tokens that have been
		 * requested and the GC thread has given up. Before returning
		 * NULL we need to put back in the free list all the tokens
		 * that we have gotten so far
		 */
		while (ret != NULL) {
		    tmp = ret;
		    ret = ret->next;
		    tkm_PutTokenInFreeList(tmp);
		}
		lock_ReleaseWrite(&tkm_freeTokensLock);
		return NULL;
	    }
	    osi_Wakeup(&tkm_gcListHead);
	    osi_SleepW(&tkm_freeTokens, &tkm_freeTokensLock);
	}
    }
    return(ret);
}

/*
 * Put that was allocated and not used back in the free list
 */

void tkm_FreeToken(tkm_internalToken_t *tokenP)
{
    lock_ObtainWrite(&tkm_freeTokensLock);
    tkm_PutTokenInFreeList(tokenP);
    lock_ReleaseWrite(&tkm_freeTokensLock);
}

/* Add a token to the tail of the list */

void tkm_AddToGcList(tkm_internalToken_t *tokenP)
{
    lock_ObtainWrite(&tkm_gcListLock);
    tokenP->nextGC = NULL;
    tokenP->prevGC = tkm_gcListTail;
    if (tkm_gcListTail != NULL)
	tkm_gcListTail->nextGC = tokenP;
    else {
	/* this is the first token */
	osi_assert(tkm_gcListHead == NULL);
	tkm_gcListHead = tokenP;
    }
    tkm_gcListTail = tokenP;
    lock_ReleaseWrite(&tkm_gcListLock);
}

void tkm_RemoveFromGcList(tkm_internalToken_t *tokenP)
{
    lock_ObtainWrite(&tkm_gcListLock);

    if (tokenP->nextGC != NULL)
	tokenP->nextGC->prevGC = tokenP->prevGC;
    else {
	osi_assert(tkm_gcListTail == tokenP);
	tkm_gcListTail = tokenP->prevGC;
    }

    if (tokenP->prevGC != NULL)
	tokenP->prevGC->nextGC = tokenP->nextGC;
    else {
	osi_assert(tokenP == tkm_gcListHead);
	tkm_gcListHead = tokenP->nextGC;
    }

    lock_ReleaseWrite(&tkm_gcListLock);
}
