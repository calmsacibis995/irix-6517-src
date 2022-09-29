/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_filetoken.c,v 65.4 1998/04/01 14:16:29 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 *      Copyright (C) 1996, 1992 Transarc Corporation
 *      All rights reserved.
 */

#include "tkm_internal.h"
#include "tkm_file.h"
#include "tkm_volume.h"
#include "tkm_conflicts.h"
#include "tkm_voltoken.h"
#include "tkm_asyncgrant.h"
#include "tkm_tokenlist.h"
#include "tkm_recycle.h"
#include "tkm_revokes.h"
#include "tkm_misc.h"
#include "tkm_ctable.h"
#include <dcedfs/hs_host.h>
#ifdef KERNEL
#include <dcedfs/icl.h>
#include <dcedfs/tkm_trace.h>
extern struct icl_set *tkm_grantIclSet;
#endif /*KERNEL*/
#ifdef SGIMIPS
#include "tkm_filetoken.h"
#include "tkm_tokens.h"
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_filetoken.c,v 65.4 1998/04/01 14:16:29 gwehrman Exp $")

static int tkm_CleanConflictQ(tkm_tokenConflictQ_t *q);

#ifdef SGIMIPS
static void tkm_GrantFileToken(tkm_internalToken_t *tokenP);
static tkm_MakeSliceNdice(tkm_internalToken_t	*newTokenP,
			  struct tkm_tokenConflictElem *conflict,
			  tkm_internalToken_t	**poolPP);
static int tkm_CleanConflictQ(tkm_tokenConflictQ_t *q);
#endif

/************************************************************
 * tkm_GrantFileToken(tokenP)
 *
 * Input Params: tokenP = the token to be added to the granted
 *			  list of it's holder.
 *
 * LOCKING: File(tokenP) and Vol(tokenP) are locked
 ************************************************************/
static void tkm_GrantFileToken(tkm_internalToken_t *tokenP)
{
    tkm_file_t *fileP = tokenP->holder.file;

    osi_assert(lock_WriteLocked(&(fileP->lock)));
    osi_assert(lock_WriteLocked(&(fileP->vol->lock)));
    fileP->tokenGrants++;
    fileP->vol->tokenGrants++;
    tkm_HoldFile(fileP);
    tkm_dprintf(("added %x to %x\n", tokenP, fileP));
    tokenP->expiration = tkm_FreshExpTime(tokenP);
    tkm_AddToTokenList(tokenP, &(fileP->granted));
    tkm_AddToGcList(tokenP);
    tkm_AddToTokenMask(&(fileP->vol->fileTokens), tokenP->types);
}

/**************************************************************************
 *
 * tkm_GetFileToken(newTokenP, flags, lockDescriptorP) : Grants a fid token
 *
 * Input Params:  flags = nothing, TKM_FLAGS_QUEUEDREQUEST,
 *		               or TKM_FLAGS_EXPECTNOCONFL,
 *			       or TKM_FLAGS_OPTIMISTICGRANT
 *			       or TKM_FLAGS_DOING_ASYNCGRANT
 * Output Params : lockDescriptoP = if applicable describes the lock that
 *				    blocked the grant of a lock token
 *
 * InOut Params : newTokenP = the token requested
 *
 * LOCKING: nothing is locked. The fid_t representing the file that newToken
 *	     will be granted for is held (and will be released) by the caller
 ****************************************************************************/
int tkm_GetFileToken(tkm_internalToken_t *newTokenP,
		     long flags,
		     afs_recordLock_t *lockDescriptorP)
{
    tkm_file_t *fileP;
    tkm_vol_t *volP;
    long otherHostFileMask, otherHostVolMask;
    int localFileCount, localVolCount;
    long code;
    tkm_internalToken_t *tmp;

    fileP = newTokenP->holder.file;
    volP = fileP->vol;
    lock_ObtainWrite(&(fileP->lock));
    do {
	code =  tkm_FixFileConflicts(newTokenP, fileP, flags,
				 &otherHostFileMask, lockDescriptorP);
	localFileCount = fileP->tokenGrants;
	if (code != TKM_SUCCESS) {
	    if ((flags & TKM_FLAGS_QUEUEREQUEST) &&
		(code == TKM_ERROR_TOKENCONFLICT)){
		tkm_HoldFile(fileP);
		tkm_AddToDoubleList(newTokenP,&(fileP->queued));
		lock_ReleaseWrite(&(fileP->lock));
		return(TKM_ERROR_REQUESTQUEUED);
	    } else {
		lock_ReleaseWrite(&(fileP->lock));
		return(code);
	    }
	}
	lock_ObtainWrite(&(volP->lock));
	/*
	 * First we have to make sure that the token that we are trying to
	 * grant is not conflicting with any whole volume tokens in the
	 * process of being granted.  If it is we have to wait until the
	 * whole volume token is granted, or refused.
	 */
	tmp = volP->beingGranted;
	while(tmp != NULL) {
	    if ((newTokenP->host != tmp->host) &&
		(((newTokenP->types) & TKM_TYPE_CONFLICTS(tmp->types)) != 0)) {
		lock_ReleaseWrite(&(fileP->lock));
 		if (flags & (TKM_FLAGS_EXPECTNOCONFL)) {
 		    lock_ReleaseWrite(&(volP->lock));
 		    return(TKM_ERROR_TOKENCONFLICT);
 		} else {
 		    osi_SleepW(&(volP->beingGranted), &(volP->lock));
 		    /* get our locks back and start again */
 		    lock_ObtainWrite(&(fileP->lock));
 		    lock_ObtainWrite(&(volP->lock));
 		    tmp = volP->beingGranted;
		}
	    } else
		tmp=tmp->next;
	}
	code = tkm_FixVolConflicts(newTokenP, flags, &otherHostVolMask);
	localVolCount = volP->tokenGrants;
	if (localFileCount != fileP->tokenGrants) {
	    lock_ReleaseWrite(&(volP->lock));
	    continue;
	}
    } while (localFileCount != fileP-> tokenGrants);
    if (code != TKM_SUCCESS) {
	if ((flags & TKM_FLAGS_QUEUEREQUEST) &&
	    (code == TKM_ERROR_TOKENCONFLICT)){
	    tkm_HoldFile(fileP);
	    tkm_AddToDoubleList(newTokenP,&(volP->queuedFileTokens));
	    lock_ReleaseWrite(&(volP->lock));
	    lock_ReleaseWrite(&(fileP->lock));
	    return(TKM_ERROR_REQUESTQUEUED);
	} else {
	    lock_ReleaseWrite(&(volP->lock));
	    lock_ReleaseWrite(&(fileP->lock));
	    return(code);
	}
    }
    /*
     * fileP->host should be NULL if more than one hosts hold tokens for
     * this file. Since we are about to grant a token to newTokenP->host if
     * there are any types of tokens granted to other hosts then we should
     * set fileP->host to NULL, otherwise no other host holds any other
     * type of token so we can set fileP)->host to newTokenP->host. We
     * check to see if there are any types of tokens granted to other
     * hosts by looking at otherHostVolMask and otherHostFileMask. There
     * is one case however when otherHostVolMask and/or otherHostFileMask
     * are not accurate. This is the case where the FixFileConflicts
     * routine determined that there are no conflicts with the granted
     * tokens by simply looking at the granted  tokens mask, and not
     * walking down the token list. In this case the following will set
     * fileP->host to NULL even if the only other granted tokens are
     * granted to newTokenP->host but that's not too bad since having
     * fileP->host set has a significant performance gain only if we avoid
     * traversing the token list.
     */
    if ((otherHostFileMask | otherHostVolMask) == 0){
	fileP->host = newTokenP->host;
    } else {
	if (fileP->host != newTokenP->host){
	    fileP->host = NULL;
	}
    }
    if (flags & TKM_FLAGS_OPTIMISTICGRANT)
    {
	long moreRights;

	/*
	  add to token rights not conflicting
	  with (otherHostVolMask | otherHostFileMask)
	  excluding HERE and THERE rights
	  */
	moreRights = (~(TKM_TOKENTYPE_MASK(TKM_OPEN_DELETE) |
			TKM_TOKENTYPE_MASK(TKM_SPOT_HERE) |
			TKM_TOKENTYPE_MASK(TKM_SPOT_THERE) |
			TKM_TYPE_CONFLICTS(otherHostVolMask|otherHostFileMask)))
	    & (TKM_TOKENTYPE_MASK(TKM_ALL_TOKENS) - 1);
	
	/* 
	 * if the token is for a partial byterange don't add whole file
	 * types
	 */
	if (!AFS_hiszero(newTokenP->range.lowBnd) ||
	    AFS_hcmp(newTokenP->range.upBnd, osi_hMaxint32) < 0)
	    moreRights = TKM_TYPES_WITH_RANGE(moreRights);
	newTokenP->types |= moreRights;
    }
    tkm_GrantFileToken(newTokenP);
    if (flags & TKM_FLAGS_DOING_ASYNCGRANT) {
	struct hs_tokenDescription hsToken;

	tkm_InternalToExternalToken(newTokenP, &(hsToken.token));
	tkm_CopyTokenFid(newTokenP, &(hsToken.fileDesc));
	hsToken.hostp = newTokenP->host;
	/* do the RPC after disabling racing revokes */
	newTokenP->flags |= TKM_TOKEN_GRANTING;
	lock_ReleaseWrite(&(volP->lock));
	lock_ReleaseWrite(&(fileP->lock));

	icl_Trace3(tkm_grantIclSet, TKM_TRACE_STARTASYNC,
		   ICL_TYPE_POINTER, (long) newTokenP->host,
		   ICL_TYPE_HYPER, &(hsToken.token.tokenID),
		   ICL_TYPE_LONG, newTokenP->reqNum);

	code = HS_ASYNCGRANT(newTokenP->host, &hsToken, newTokenP->reqNum);

	icl_Trace1(tkm_grantIclSet, TKM_TRACE_ENDASYNC,
                       ICL_TYPE_LONG, code);

	lock_ObtainWrite(&(fileP->lock));
	lock_ObtainWrite(&(volP->lock));
	newTokenP->flags &= ~TKM_TOKEN_GRANTING;
	osi_Wakeup(&(fileP->granted));
    }

    lock_ReleaseWrite(&(volP->lock));
    lock_ReleaseWrite(&(fileP->lock));
    return (TKM_SUCCESS);
}

/******************************************************************
 *
 * tkm_FixFileConflicts(tokenP, fileP, flags, otherHostMaskP, lockDescriptorP)
 *
 * Input Params: tokenP = the token that we want to grant
 *		 fileP = the file that must have no tokens conflicting
 *			 with tokenP
 *		 flags = nothing or TKM_FLAGS_EXPECTNOCONFL
 *				 or TKM_FLAGS_QUEUEDREQUEST
 * Output Params: : otherHostMaskP = the sum of  token types not conflicting
 *				      with token but granted to hosts other
 *				      than tokenP->host
 *		    lockDescriptoP = if applicable describes the lock that
 *				     blocked the grant of a lock token
 *
 * LOCKING: Called with file locked. Vol(token) MUST be unlocked. The file
 *          lock might be dropped and reobtained
 *
 * GENERAL: If this routine returns SUCCESS it guarantees that until the
 *	    file lock is dropped again there will be no granted tokens
 *	    conflicting with the token passed in as the first input parameter.
 *          If flags included TKM_FLAGS_QUEUEDREQUEST a FAILURE is return
 * 	    guarantees that until the file lock is dropped there will be in
 *	    the granted list of the file at least one conflicting token that
 *	    has been sent a revocation (so it's safe to queue tokenP)
 *
 ***************************************************************************/

int tkm_FixFileConflicts(tkm_internalToken_t *tokenP,
			 tkm_file_t *fileP,
			 long flags,
			 long *otherHostMaskP,
			 afs_recordLock_t *lockDescriptorP)
{
    struct tkm_tokenConflictQ *tokensToRevoke;
    tkm_internalToken_t *sliceNdice = NULL, *sliceNdicePool = NULL;
    int sliceNdiceAlloced = 0, needed = 0;
    tkm_internalToken_t *more, *tmp;
    long code;

    *otherHostMaskP = 0;
    if (tokenP->host == fileP->host)
	return TKM_SUCCESS;
    while (((tokensToRevoke = tkm_ConflictingTokens(&(fileP->granted),
					     tokenP,
					     otherHostMaskP,
					     TKM_MAX_CONFLICTS)) != NULL) &&
	   (!(TKM_CONFLICTQ_ISEMPTY(tokensToRevoke))))
       {
	if (flags & (TKM_FLAGS_EXPECTNOCONFL)){
	    tkm_DeleteConflictQ(tokensToRevoke);
	    return(TKM_ERROR_TOKENCONFLICT);
	}
	if ((needed = (2*(tokensToRevoke->sliceNdiceNeeded)
		       -sliceNdiceAlloced)) > 0) {
	    int got = 0;

	    more = tkm_AllocTokenNoSleep(needed, &got);
	    /* add the new tokens to the sliceNdicePool */
	    if (sliceNdicePool == NULL) {
		sliceNdicePool = more;
	    } else {
		while (more != NULL) {
		    tmp = more;
		    more = more->next;
		    tmp->next = sliceNdicePool;
		    sliceNdicePool = tmp;
		}
	    }
	    if (got != needed) {
		/*
		 * We couldn't get all the tokens we needed without
		 * recycling so we have to drop our locks and try the
		 * other allocator
		 */
		lock_ReleaseWrite(&(fileP->lock));
		if ((more = tkm_AllocToken(needed - got)) == NULL) {
		    while(sliceNdicePool != NULL) {
			tmp = sliceNdicePool;
			sliceNdicePool = sliceNdicePool->next;
			tkm_FreeToken(tmp);
		    }
		    lock_ObtainWrite(&(fileP->lock));
		    tkm_DeleteConflictQ(tokensToRevoke);
		    return ENOMEM;
		} else {
		    /* add the new tokens to the sliceNdicePool */
		    if (sliceNdicePool == NULL) {
			sliceNdicePool = more;
		    } else {
			while (more != NULL) {
			    tmp = more;
			    more = more->next;
			    tmp->next = sliceNdicePool;
			    sliceNdicePool = tmp;
			}
			/*we dropped our locks we have to start all over*/
			lock_ObtainWrite(&(fileP->lock));
			tkm_DeleteConflictQ(tokensToRevoke);
			continue;
		    }
		}
		lock_ObtainWrite(&(fileP->lock));
	    }
	}
	if (tokensToRevoke->sliceNdiceNeeded > 0) {
	    struct tkm_tokenConflictElem *conflict;
	    tkm_byterange_t	slicedRange;
	    int i;

	    for (i = 0; i < tokensToRevoke->lastPos; i++) {
		conflict = &(tokensToRevoke->elements[i]);
		if (conflict->grant != 0) {
		    (void) tkm_MakeSliceNdice(tokenP, conflict,
					      &sliceNdicePool);
		    if (conflict->grantLow != NULL){
			tkm_dprintf(("adding to s&l %x\n", conflict->grantLow));
			tkm_AddToDoubleList(conflict->grantLow, &(sliceNdice));
		    }
		    if (conflict->grantHigh != NULL){
			tkm_dprintf(("adding to s&l %x \n",conflict->grantHigh));
			tkm_AddToDoubleList(conflict->grantHigh, &(sliceNdice));
		    }
		}
	    }
	    /* free all the unused tokens that we allocated */
	    while(sliceNdicePool != NULL) {
		tmp = sliceNdicePool;
		sliceNdicePool = sliceNdicePool->next;
		tkm_FreeToken(tmp);
	    }

	    /*
	     * grant all the sliceNdice tokens that we created.
	     * They all have the GRANTING flag turned on (set in
	     * tkm_MakeSliceNdice()).
	     * We grant them all together to avoid getting and
	     * dropping the volume lock, as well as minimize the
	     * time that we have tokens with the GRANTING flag turned
	     * on in the file's granted list
	     * The GRANTING flag will be unset once the RPC that does
	     * the revoke and the offer returns in tkm_DoRevoke().
	     */
	    lock_ObtainWrite(&(fileP->vol->lock));
	    while (sliceNdice != NULL) {
		tmp = sliceNdice;
		sliceNdice = sliceNdice->next;
		tkm_dprintf(("granting internally %x \n", tmp));
		tkm_GrantFileToken(tmp);
	    }
	    lock_ReleaseWrite(&(fileP->vol->lock));
	} else {
	    while(sliceNdicePool != NULL) {
		tmp = sliceNdicePool;
		sliceNdicePool = sliceNdicePool->next;
		tkm_FreeToken(tmp);
	    }
	}
	lock_ReleaseWrite(&(fileP->lock));
	code = tkm_ParallelRevoke(tokensToRevoke,
				  (tokenP->flags & TKM_TOKEN_ISVOL ?
				   TKM_PARA_REVOKE_FLAG_FORVOLTOKEN : 0),
				  lockDescriptorP);
	/*
	 * at this point tokensToRevoke has the tokens that were
	 * refused
	 */
	lock_ObtainWrite(&(fileP->lock));
	/*
	 * this will wake up all the waiters of slice and dice tokens
	 * but if tokensToRevoke is not empty and we do a DeleteConflictQ
	 * we'll need to try and wake them up again
	 */
	osi_Wakeup(&(fileP->granted));
	if (code != TKM_SUCCESS){
	    if ((code == TKM_ERROR_TOKENCONFLICT) &&
		(flags & TKM_FLAGS_QUEUEREQUEST)){
		/*
		 * Since we had dropped the lock during the revocation call
		 * we can't be sure that the tokens that were refused in our
		 * revocation attempt haven't been returned by now so we have
		 * to check again
		 */
		int i;
		struct tkm_tokenConflictElem *conflict;

		for (i = 0; i < tokensToRevoke->lastPos; i++) {
		    conflict = &(tokensToRevoke->elements[i]);
		    if (conflict->token != NULL) {
			conflict->revoke &= conflict->token->types;
			if (conflict->revoke != 0) {
			    lock_ObtainWrite(&(fileP->vol->lock));
			    if (tkm_CleanConflictQ(tokensToRevoke))
				osi_Wakeup(&(fileP->granted));
			    lock_ReleaseWrite(&(fileP->vol->lock));
			    return(TKM_ERROR_TOKENCONFLICT);
			}
		    }
		}
		/* the refused tokens have been returned so let's try again */
		lock_ObtainWrite(&(fileP->vol->lock));
		if (tkm_CleanConflictQ(tokensToRevoke))
		    osi_Wakeup(&(fileP->granted));
		lock_ReleaseWrite(&(fileP->vol->lock));
		continue;
	    } else {
		/*
		 * some tokens were refused and we are not queueing
		 * or we got some other kind of error
		 */
		lock_ObtainWrite(&(fileP->vol->lock));
		if (tkm_CleanConflictQ(tokensToRevoke))
		    osi_Wakeup(&(fileP->granted));
		lock_ReleaseWrite(&(fileP->vol->lock));
		return(code);
	    }
	}
	if (tokensToRevoke != NULL){
	    osi_assert(TKM_CONFLICTQ_ISEMPTY(tokensToRevoke));
	    tkm_DeleteConflictQ(tokensToRevoke);
	}
    }
    /* we exited the while so there are no more conflicting tokens*/
    return (TKM_SUCCESS);
}


/***********************************************
 * tkm_ReturnFileToken(tokenP)
 *
 * Input Params: tokenP = the token to be removed from the granted
 *			  list of its holder.
 *
 * LOCKING: File(tokenP) and Vol(tokenP) are locked
 *
 * GENERAL: will queue up work for the async_grant thread
 *****************************************************/
void tkm_ReturnFileToken(tkm_internalToken_t *tokenP)
{
    osi_assert(lock_WriteLocked(&(tokenP->holder.file->lock)));
    tkm_RemoveFromGcList(tokenP);
    tkm_dprintf(("RETURN removed %x from %x\n", tokenP, tokenP->holder.file));
    tkm_RemoveFromDoubleList(tokenP,&(tokenP->holder.file->granted.list));
    /* the token is not in any list anymore so it's safe */
    tkm_dprintf(("RETURN rele %x \n", tokenP->holder.file));
    tkm_ReleFile(tokenP->holder.file);
}

static tkm_MakeSliceNdice(tkm_internalToken_t	*newTokenP,
			  struct tkm_tokenConflictElem *conflict,
			  tkm_internalToken_t	**poolPP)
{
    tkm_byterangePair_t	slicedRange;

    tkm_ByterangeComplement(&(conflict->token->range),
			    &(newTokenP->range),
			    &slicedRange);
    if (slicedRange.lo != (tkm_byterange_p)NULL) {
	conflict->grantLow = *poolPP;
	osi_assert(conflict->grantLow != NULL);
	/* advance the list */
	*poolPP = (*poolPP)->next;
	/* unthread  our token from the list */
	conflict->grantLow->next = conflict->grantLow->prev = NULL;
	conflict->grantLow->expiration =
	    tkm_FreshExpTime(conflict->token);
	conflict->grantLow->types = conflict->grant;
	TKM_BYTERANGE_COPY(&(conflict->grantLow->range),  slicedRange.lo);
	osi_Free(slicedRange.lo, sizeof(tkm_byterange_t));
	conflict->grantLow->host = conflict->token->host;
	HS_HOLD(conflict->grantLow->host);
	conflict->grantLow->holder.file = conflict->token->holder.file;
	conflict->grantLow->flags |= TKM_TOKEN_GRANTING;
    } else
	conflict->grantLow = NULL;
    if (slicedRange.hi != (tkm_byterange_p)NULL) {
	conflict->grantHigh = *poolPP;
	osi_assert(conflict->grantHigh != NULL);
	/* advance the list */
	*poolPP = (*poolPP)->next;
	/* unthread  our token from the list */
	conflict->grantHigh->next = conflict->grantHigh->prev = NULL;
	conflict->grantHigh->expiration =
	    tkm_FreshExpTime(conflict->token);
	conflict->grantHigh->types = conflict->grant;
	TKM_BYTERANGE_COPY(&(conflict->grantHigh->range),  slicedRange.hi);
	osi_Free(slicedRange.hi, sizeof(tkm_byterange_t));
	conflict->grantHigh->host = conflict->token->host;
	HS_HOLD(conflict->grantHigh->host);
	conflict->grantHigh->holder.file = conflict->token->holder.file;
	conflict->grantHigh->flags |= TKM_TOKEN_GRANTING;
    } else
	conflict->grantHigh = NULL;

#ifdef SGIMIPS
    return 0;
#endif
}


/*
 * tkm_cleanConflictQ(tkm_tokenConflictQ_t *q)
 *
 * Called to clean up conflict queues if some error occured after
 * slice and dice tokens have been allocated. Returns 0 if it
 * didn't find any slice and dice tokens that someone could have
 * been waiting on and 0 otherwise
 */
static int tkm_CleanConflictQ(tkm_tokenConflictQ_t *q)
{
    int i, foundBlockers = 0;

    /*
     * before deleting the list of tokens that did not get revoked
     * make sure that the slice and dice tokens that were not offered
     * are also deleted from the file's granted list
     */

    for (i = 0; i < q->lastPos; i++) {
	if (q->elements[i].token != NULL){
	    if ((q->elements[i].grantLow != NULL) &&
		(q->elements[i].grantLow->flags & TKM_TOKEN_GRANTING)) {
		TKM_RECYCLE_TOKEN(q->elements[i].grantLow);
		foundBlockers = 1;
	    }
	    if ((q->elements[i].grantHigh != NULL) &&
		(q->elements[i].grantHigh->flags & TKM_TOKEN_GRANTING)) {
		TKM_RECYCLE_TOKEN(q->elements[i].grantHigh);
		foundBlockers = 1;
	    }
	}
    }
    tkm_DeleteConflictQ(q);
    return(foundBlockers);
}




