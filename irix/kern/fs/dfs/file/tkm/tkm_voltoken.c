/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_voltoken.c,v 65.4 1998/04/01 14:16:35 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/* Copyright (C) 1996, 1994 Transarc Corporation - All rights reserved. */

#include "tkm_internal.h"
#include "tkm_volume.h"
#include "tkm_file.h"
#include "tkm_tokenlist.h"
#include "tkm_recycle.h"
#include "tkm_asyncgrant.h"
#include "tkm_revokes.h"
#include "tkm_conflicts.h"
#include "tkm_ctable.h"
#include "tkm_misc.h"
#ifdef SGIMIPS
#include "tkm_file.h"
#include "tkm_filetoken.h"
#include "tkm_voltoken.h"
#endif

#ifdef KERNEL
#include <dcedfs/icl.h>
#include <dcedfs/tkm_trace.h>
extern struct icl_set *tkm_iclSet;
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_voltoken.c,v 65.4 1998/04/01 14:16:35 gwehrman Exp $")

#ifdef SGIMIPS
void tkm_GrantVolToken(tkm_internalToken_t *tokenP);
#endif

/**************************************************************************
 * tkm_GetVolToken(newTokenP, flags) : Grants a volume token
 *
 * Input Params: newToken = the token requested
 *	      flags = nothing, TKM_FLAGS_QUEUEREQUEST,
 *			    or TKM_FLAGS_EXPECTNOCONFL
 *
 * LOCKING: nothing is locked. Vol(newTokenP) is held (and will be released)
 * by the caller
 ****************************************************************************/
int tkm_GetVolToken(tkm_internalToken_t *newTokenP,
		    long flags)
{
    tkm_vol_t * volP = newTokenP->holder.volume;
    long otherHostVolMask, otherHostFileMask;
    int localVolCount;
    tkm_file_t *fileP, *nextFileP;
    long code;
    afs_hyper_t grantMask;

    lock_ObtainWrite(&(volP->lock));

    /* Yuck I really hate this but want to prevent 2 icl traces */
    AFS_hset64(grantMask,
	       volP->granted.mask->typeSum, volP->fileTokens.typeSum);
    icl_Trace4(tkm_iclSet, TKM_TRACE_GETVOLTOKEN, 
	       ICL_TYPE_HYPER, &(volP->id),
	       ICL_TYPE_LONG, newTokenP->types,
	       ICL_TYPE_LONG, flags,
	       ICL_TYPE_HYPER, &grantMask);

    /* add to the volume's beingGranted list */
    tkm_AddToDoubleList(newTokenP, &(volP->beingGranted));
    while (1) {
	code  = tkm_FixVolConflicts(newTokenP, flags,  &otherHostVolMask);
	tkm_dprintf(("getvt %x fix=%d lv = %d\n", newTokenP, code,volP->tokenGrants));
	localVolCount = volP->tokenGrants;
	if (code != TKM_SUCCESS) {
	    tkm_RemoveFromDoubleList(newTokenP, &(volP->beingGranted));
	    osi_Wakeup(&(volP->beingGranted));
	    lock_ReleaseWrite(&(volP->lock));
	    return(code);
	}
	if ((volP->fileTokens.typeSum &
	     TKM_TYPE_CONFLICTS(newTokenP->types)) == 0) {
	    tkm_RemoveFromDoubleList(newTokenP, &(volP->beingGranted));
	    tkm_dprintf(("getvt adding %x  lv = %d\n", newTokenP, volP->tokenGrants));
	    tkm_GrantVolToken(newTokenP);
	    osi_Wakeup(&(volP->beingGranted));
	    lock_ReleaseWrite(&(volP->lock));
	    return(TKM_SUCCESS);
	}
	lock_ReleaseWrite(&(volP->lock));
	lock_ObtainWrite(&tkm_fileListLock);
	lock_ObtainWrite(&(volP->fileLock));
	fileP = volP->files;
	if (fileP !=NULL)
	    tkm_FastHoldFile(fileP);
	while (fileP != NULL){
	    nextFileP = fileP->next;
	    if (nextFileP !=NULL)
		tkm_FastHoldFile(nextFileP);
	    lock_ReleaseWrite(&tkm_fileListLock);
	    lock_ReleaseWrite(&(volP->fileLock));
	    lock_ObtainWrite(&(fileP->lock));
	    code  = tkm_FixFileConflicts(newTokenP, fileP, flags,
					 &otherHostFileMask,
					 NULL);
	    if (code != TKM_SUCCESS){
		lock_ObtainWrite(&(volP->lock));
		tkm_RemoveFromDoubleList(newTokenP, &(volP->beingGranted));
		osi_Wakeup(&(volP->beingGranted));
		lock_ReleaseWrite(&(fileP->lock));
		lock_ReleaseWrite(&(volP->lock));
		tkm_ReleFile(fileP);
		if (nextFileP != NULL)
		    tkm_ReleFile(nextFileP);
		return(code);
	    }
	    lock_ObtainWrite(&tkm_fileListLock);
	    lock_ObtainWrite(&(volP->fileLock));
	    lock_ReleaseWrite(&(fileP->lock));
	    tkm_FastReleFile(fileP);
	    fileP = nextFileP;
	}
	lock_ReleaseWrite(&(volP->fileLock));
	lock_ReleaseWrite(&tkm_fileListLock);
	lock_ObtainWrite(&(volP->lock));
	if (localVolCount != volP->tokenGrants)
	    /*
	     * A conflicting whole volume token might have been granted. This 
	     * is possible since whole volume token grants (unlike file token
	     * grants) do not get blocked by conflicting whole volume tokens 
	     * in the process of being granted.
	     */
	    continue;
	tkm_RemoveFromDoubleList(newTokenP, &(volP->beingGranted));
	tkm_dprintf(("getvt adding %x  lv = %d\n", newTokenP, volP->tokenGrants));
	tkm_GrantVolToken(newTokenP);
	osi_Wakeup(&(volP->beingGranted));
	lock_ReleaseWrite(&(volP->lock));
	return(TKM_SUCCESS);
    }
}

/******************************************************************
 * tkm_FixVolConflicts(tokenP, flags, otherHostMaskP)
 *
 * Input Params:  tokenP = the token that we want to grant
 *		  flags = nothing, or TKM_FLAGS_EXPECTNOCONFL
 *				   or TKM_FLAGS_QUEUEREQUEST
 * Output Params: otherHostMaskP is a mask of token types not conflicting
 *     		  with token but granted to hosts other than token->host
 * LOCKING: file(tokenP) (if not NULL) and vol(tokenP) are locked
 *
 * Note: file(tokenP) is NULL for volume tokens
 *
 * GUARANTEES: If this routine returns SUCCESS it guarantees that until the
 * 	       vol lock is dropped again there will be no granted vol tokens
 * 	       conflicting with the token passed in as the first input
 * 	       parameter. If flags included TKM_FLAGS_QUEUEREQUEST a failed
 *	       return  guarantees that until the vol lock is dropped there
 * 	       will be in the granted list of the vol at least one conflicting
 * 	       token that has been sent a revocation so it's safe to queue
 *	       the token requested
 *****************************************************************/

int tkm_FixVolConflicts(tkm_internalToken_t *tokenP,
			long flags,
			long *otherHostMaskP)
{
    tkm_tokenConflictQ_t *tokensToRevoke;
    tkm_vol_t *volP;
    long code;

    if ((tokenP->flags & TKM_TOKEN_ISVOL))
	volP = tokenP->holder.volume;
    else
        volP = tokenP->holder.file->vol;


    while (((tokensToRevoke = tkm_ConflictingTokens(&(volP->granted), tokenP,
					     otherHostMaskP,
					     TKM_MAX_CONFLICTS)) != NULL) &&
	   (!(TKM_CONFLICTQ_ISEMPTY(tokensToRevoke))))
       {
	if (flags & TKM_FLAGS_EXPECTNOCONFL){
	    tkm_DeleteConflictQ(tokensToRevoke);
	    return(TKM_ERROR_TOKENCONFLICT);
	}
	/*
	 * This routine gets called from tkm_GetFileToken() and
	 * tkm_GetVolToken() so the token can be a file or a volume token
	 */
	TKM_UNLOCK_TOKEN_HOLDER(tokenP);
	if (!(tokenP->flags & TKM_TOKEN_ISVOL))  /* is a file token */
	    lock_ReleaseWrite(&(tokenP->holder.file->vol->lock));
	code = tkm_ParallelRevoke(tokensToRevoke,
				  (tokenP->flags & TKM_TOKEN_ISVOL ?
				   TKM_PARA_REVOKE_FLAG_FORVOLTOKEN : 0),
				  NULL);
	TKM_LOCK_TOKEN_HOLDER(tokenP);
	if (!(tokenP->flags & TKM_TOKEN_ISVOL)) /* is a file token */
	    lock_ObtainWrite(&(tokenP->holder.file->vol->lock));
	if (code != TKM_SUCCESS)  {
	    if (!(tokenP->flags & TKM_TOKEN_ISVOL)) { /* is a file token */
		if ((code == TKM_ERROR_TOKENCONFLICT) &&
		    (flags & TKM_FLAGS_QUEUEREQUEST)){
		    /*
		     * Since we had dropped the lock during the revocation call
		     * we can't be sure that the tokens that were refused in
		     * our revocation attempt haven't been returned by now so
		     * we have to check again
		     */
		    int i;
		    struct tkm_tokenConflictElem *conflict;

		    for (i = 0; i < tokensToRevoke->lastPos; i++) {
			conflict = &(tokensToRevoke->elements[i]);
			conflict->revoke &= conflict->token->types;
			if (conflict->revoke != 0) {
			    tkm_DeleteConflictQ(tokensToRevoke);
			    return(TKM_ERROR_TOKENCONFLICT);
			}
		    }
		    /* the refused tokens have been returned so try again */
		    tkm_DeleteConflictQ(tokensToRevoke);
		    continue;
		} else {
		    /* some tokens were refused and we are not queueing */
		    tkm_DeleteConflictQ(tokensToRevoke);
		    return(code);
		}
	    }	else { /* whole volume token no queueing on those */
		tkm_DeleteConflictQ(tokensToRevoke);
		return(code);
	    }
	}
	if (tokensToRevoke != NULL)
		tkm_DeleteConflictQ(tokensToRevoke);
    }
    return(TKM_SUCCESS);
}

/*************************************************************
 * tkm_GrantVolToken(tokenP)
 *
 * Input Params: tokenP = the token to be added to the granded
 *			  list of it's holder.
 *
 *  LOCKING: Vol(token) is locked
 *************************************************************/
#ifdef SGIMIPS
void
#endif
tkm_GrantVolToken(tkm_internalToken_t *tokenP)
{
    tkm_vol_t *volP = tokenP->holder.volume;

    osi_assert(lock_WriteLocked(&(volP->lock)));
    volP->tokenGrants++;
    tkm_HoldVol(volP);
    tokenP->expiration = tkm_FreshExpTime(tokenP);
    tkm_AddToTokenList(tokenP, &(volP->granted));
    tkm_AddToGcList(tokenP);
}

/***************************************************************
 * tkm_ReturnVolToken(tokenP)
 *
 * Input Params: tokenP = the token to be removed from the granted
 *			  list of its holder.
 *
 *
 * LOCKING: Vol(token) is locked
 *
 * GENERAL: will queue up work for the async_grant thread
 ***************************************************************/

#ifdef SGIMIPS
void
#endif
tkm_ReturnVolToken(tkm_internalToken_t *tokenP)
{
    osi_assert(lock_WriteLocked(&(tokenP->holder.volume->lock)));
    tkm_RemoveFromDoubleList(tokenP,&(tokenP->holder.volume->granted.list));
    tkm_RemoveFromGcList(tokenP);
    /* the token is not in any list anymore so it's safe */
    tkm_ReleVol(tokenP->holder.volume);
}
