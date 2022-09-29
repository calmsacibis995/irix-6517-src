/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_asyncgrant.c,v 65.4 1998/04/01 14:16:22 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 *      Copyright (C) 1996, 1992 Transarc Corporation
 *      All rights reserved.
 */

#include "tkm_internal.h"
#include "tkm_ctable.h"
#include "tkm_filetoken.h"
#include "tkm_tokenlist.h"
#include "tkm_file.h"
#include "tkm_volume.h"
#include "tkm_asyncgrant.h"
#ifdef SGIMIPS
#include "tkm_misc.h"
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_asyncgrant.c,v 65.4 1998/04/01 14:16:22 gwehrman Exp $")

/*
 * Code to handle asynchronously granted tokens
 */

/*
 * the parts of the token that is no longer granted that we might
 * care about
 */

struct asyncQElem {
    struct asyncQElem *next;
    unsigned long		flags;
    tkm_tokenHolder_t		holder;
    long	types;
    struct hs_host *host;
    int	status; /* for use by the alloc-free algorithms */
};

#define AQPOOLNUM 20

/* bits describing the allocation state of an element */
#define AQ_NOTFROMPOOL 1
#define AQ_UNUSED 2


static struct asyncQElem *tkm_asyncTryQ;
static struct asyncQElem tkm_AQPool[AQPOOLNUM];
static int tkm_AQPoolIndex = 0; /* just a hint */

/* Statistics for AQ elements not from the preallocated tkm_AQPool */
static int tkm_AQDynamicCurAllocCnt; /* Currently allocated count */
static int tkm_AQDynamicMaxAllocCnt; /* Max ever allocated count */
static int tkm_AQDynamicCumAllocCnt; /* Cumulatative allocated count */
static int tkm_AQDynamicCumFreeCnt;  /* Cumulative freed count */

/* protecting all the above static variables */
static osi_dlock_t tkm_asyncTryQLock;




/*************************************************************
 *
 * tkm_TryAsyncGrantsOnList (listP, host, revokedTypes, lockP)
 *
 * Input Params : listP = the list of candidates to be granted
 *		  host = the host that held the token who's revocation
 *			 triggered this async grant attempt
 *		  revokedTypes = the rights that were revoked from the token
 *				 who's revocation triggered this async grant
 *				 attempt
 *		  lockP = the lock protecting listP
 *
 * LOCKING: lockP must be held
 * 
 * GENERAL: This routine traverses a list of tokens and sees if any can
 *	    be granted as a result of a recent token revocation. The way
 *	    that it traverses the list is not MT-safe. I.e. This routine
 *	    depends on the fact that there is a single thread doing async
 *	    grants
 ************************************************************************/
static void tkm_TryAsyncGrantsOnList (tkm_internalToken_t **listPP,
				      long revokedTypes,
				      struct hs_host *host,
				      osi_dlock_t *lockP)

{
    tkm_internalToken_t *tryGrant, *nextToGrant;
    tkm_file_t *fileP;

    tryGrant = *listPP;
    while (tryGrant != NULL) {
	osi_assert(!(tryGrant->flags & TKM_TOKEN_ISVOL)); /* must be a file token */
	/*
	 * Store a pointer to the the next token in the list in a local
	 * variable. This is  not a problem although we might soon drop
	 * the lock protecting the list because there is only one thread
	 * consuming this list (the AsyncGrant thread).
	 */
	nextToGrant = tryGrant->next;

	if  ((tryGrant->host != host)  &&
	    ((revokedTypes & TKM_TYPE_CONFLICTS(tryGrant->types)) != 0)) {
	    tkm_RemoveFromDoubleList(tryGrant, listPP);
	    /* Refresh the exp time before throwing this into the maelstrom */
	    tryGrant->expiration = tkm_FreshExpTime(tryGrant);
	    lock_ReleaseWrite(lockP);
	    /*
	     * At this point the token still holds a reference to its file
	     * (so tryGrant->holder.file cannot have been recycled)
	     * and is not in any list so although we don't hold any locks
	     * it can't change from under us
	     */
	    fileP = tryGrant->holder.file;
	    tkm_GetFileToken(tryGrant, (TKM_FLAGS_QUEUEREQUEST |
					TKM_FLAGS_DOING_ASYNCGRANT), NULL);
	    /*
	     * now we have to release the reference that the queued token held
	     * since it will get a new one in tkm_GetFileToken() whether it's granted
	     * or requeued
	     */
	    tkm_ReleFile(fileP);
	    lock_ObtainWrite(lockP);
	}
	tryGrant = nextToGrant;
    }
}


/*
 * both of the following are protected by the tkm_asyncTryQLock
 */

#ifdef SGIMIPS
static struct  asyncQElem *tkm_getAQElement(void)
#else
static struct  asyncQElem *tkm_getAQElement()
#endif
{
    int start;
    struct  asyncQElem *new;

    osi_assert(lock_WriteLocked(&tkm_asyncTryQLock));

    start = tkm_AQPoolIndex;
    do {
	if (tkm_AQPool[tkm_AQPoolIndex].status & AQ_UNUSED) {
	    tkm_AQPool[tkm_AQPoolIndex].status &= ~AQ_UNUSED;
	    return (& tkm_AQPool[tkm_AQPoolIndex]);
	}
	if (++tkm_AQPoolIndex == AQPOOLNUM)
	    tkm_AQPoolIndex = 0;
    } while (tkm_AQPoolIndex != start);
    /*
     * if we got here we need to allocate an element
     */
    new = (struct asyncQElem *) osi_Alloc (sizeof(struct asyncQElem));
    new->status = AQ_NOTFROMPOOL;
    tkm_AQDynamicCurAllocCnt++;
    tkm_AQDynamicCumAllocCnt++;
    if (tkm_AQDynamicCurAllocCnt > tkm_AQDynamicMaxAllocCnt)
	tkm_AQDynamicMaxAllocCnt++;
    return(new);
}

static void tkm_freeAQElement(struct asyncQElem *old)
{
    osi_assert(lock_WriteLocked(&tkm_asyncTryQLock));
    if (old->status & AQ_NOTFROMPOOL) {
	osi_Free(old, sizeof(struct asyncQElem));
	tkm_AQDynamicCurAllocCnt--;
	tkm_AQDynamicCumFreeCnt++;
    } else {
	osi_assert(!(old->status & AQ_UNUSED));
	old->status |= AQ_UNUSED;
    }
}

/**********************************************************************
 * tkm_TriggerAsyncGrants(tokenP)
 *
 * Input Params : tokenP = the token just revoked
 *
 * LOCKING: tokenP's holder must be locked
 *
 * General:
 * This routine simply copies a part of the token just revoked to
 * an element of the tkm_asyncTryQ and wakes up the thread that will
 * read this element and decide whether there are any tokens waiting
 * to be granted that were blocked on the token just revoked (tokenP)
 *
 **********************************************************************/

void tkm_TriggerAsyncGrants(tkm_internalToken_t *tokenP)
{
    struct asyncQElem *new;
    tkm_internalToken_p queuedTokens;

    /*
     * Only wakeup the async grant thread if there are queued tokens
     * waiting on this token.  This avoids a lot of unnecessary wakeups!
     * The following assert() verifies that we can check for queued tokens.
     */
    osi_assert(TKM_TOKEN_HOLDER_LOCKED(tokenP));
    if (tokenP->flags & TKM_TOKEN_ISVOL) {
	queuedTokens = tokenP->holder.volume->queuedFileTokens;
    } else {
	queuedTokens = tokenP->holder.file->queued;
    }
    if (queuedTokens == (tkm_internalToken_p)0)
	return; /* no work to do */

    /*
     * the rele for the following hold will be done by the
     * AsyncGrantThread() when this entry is processed.
     */
    TKM_HOLD_TOKEN_HOLDER(tokenP);
    lock_ObtainWrite(&tkm_asyncTryQLock);
    new = tkm_getAQElement();
    new->flags = tokenP->flags;
    new->holder = tokenP->holder;
    new->types = tokenP->types;
    new->host = tokenP->host;
    new->next = tkm_asyncTryQ;
    tkm_asyncTryQ = new;
    osi_Wakeup(&tkm_asyncTryQ);
    lock_ReleaseWrite(&tkm_asyncTryQLock);
}



/***********************************************************************
 * tkm_AsyncGrantThread() : This is the thread that takes care of async grants
 *			    it is woken up by whoever puts a fid or a vol in
 * 		            the TryasyncGrantTryQ queue. Once woken up it will
 *		            continue running until the queue is empty
 ************************************************************************/
#ifdef SGIMIPS
static void tkm_AsyncGrantThread(void)
#else
static void tkm_AsyncGrantThread()
#endif
{
    osi_PreemptionOff();
    while(1){
	lock_ObtainWrite(&tkm_asyncTryQLock);
	while (tkm_asyncTryQ != NULL) {
	    tkm_file_t *fileP = NULL;
	    tkm_vol_t  *volP = NULL;
	    long 	types;
	    struct hs_host *host;
	    struct asyncQElem *old;

	    if ((tkm_asyncTryQ->flags & TKM_TOKEN_ISVOL))
		volP = tkm_asyncTryQ->holder.volume;
	    else
		fileP = tkm_asyncTryQ->holder.file;
	    types = tkm_asyncTryQ->types;
	    host = tkm_asyncTryQ->host;

	    old = tkm_asyncTryQ;
	    tkm_asyncTryQ = tkm_asyncTryQ->next;

	    tkm_freeAQElement(old);
	    lock_ReleaseWrite(&tkm_asyncTryQLock);
	    if (fileP != NULL) {
		lock_ObtainWrite(&(fileP->lock));
		tkm_TryAsyncGrantsOnList(&(fileP->queued), types, host,
					 (&(fileP->lock)));
		lock_ReleaseWrite(&(fileP->lock));
		tkm_ReleFile(fileP);
	    } else {
		lock_ObtainWrite(&(volP->lock));
		tkm_TryAsyncGrantsOnList(&(volP->queuedFileTokens), types,
					 host, (&(volP->lock)));
		lock_ReleaseWrite(&(volP->lock));
		tkm_ReleVol(volP);
	    }
	    lock_ObtainWrite(&tkm_asyncTryQLock);
	}
	osi_SleepW(&tkm_asyncTryQ, &tkm_asyncTryQLock);
    }
}

/*************************************************************************
 *
 * tkm_InitAsyncGrant() : Initialize the global lock and start the async
 *			  grant thread
 *
 *************************************************************************/
#ifdef SGIMIPS
void tkm_InitAsyncGrant(void)
#else
void tkm_InitAsyncGrant()
#endif
{
    long code;
    int i;

    lock_Init(&tkm_asyncTryQLock);

    /* 
     * Rely on static declaration of tkm_AQPool for the structures to be 
     * bzeroed. Set the element available for use indication.
     */
    for (i = 0; i < AQPOOLNUM; i++) 
	tkm_AQPool[i].status |= AQ_UNUSED;

    osi_ThreadCreate(tkm_AsyncGrantThread, (void *)0, 0, 0,
		     "TKM_Async_Grant_Thread", code);
    osi_assert(code==0);
}
