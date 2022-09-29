/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_tokens.c,v 65.5 1998/04/01 14:16:34 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * Copyright (c) 1996, 1994, Transarc Corp.
 * All Rights Reserved.
 *
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * EXPORTED ROUTINES
 */

#include <tkm_tokens.h>
#include <tkm_internal.h>
#include <tkm_file.h>
#include <tkm_volume.h>
#include <tkm_recycle.h>
#include <tkm_filetoken.h>
#include <tkm_voltoken.h>
#include <tkm_ctable.h>
#include <dcedfs/vol_errs.h>
#ifdef KERNEL
#include <dcedfs/icl.h>
#include <dcedfs/tkm_trace.h>
extern struct icl_set *tkm_iclSet;
#endif /*KERNEL*/
#ifdef SGIMIPS
#include <tkm_asyncgrant.h>
#include <tkm_misc.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_tokens.c,v 65.5 1998/04/01 14:16:34 gwehrman Exp $")

#define FID_ISVOL(fid)	((fid->Vnode ==TKM_AFSFID_WHOLE_VOL_VNODE) \
                       &&(fid->Unique==TKM_AFSFID_WHOLE_VOL_UNIQUE))

afs_hyper_t tkm_localCellID = AFS_HINIT(0,1);

#ifdef SGIMIPS
static void tkm_ExternalToInternalToken(afs_token_t *externalP,
					tkm_internalToken_t *internalP);
static tkm_RefreshToken(afs_token_t	 *externalP,
			afsFid   	*fileDescP,
			struct hs_host *hostP);
static void tkm_VolRightsHeld(struct hs_host *hostP,
			      tkm_vol_t *volP,
			      long *rightsP);
static void tkm_FileRightsHeld(struct hs_host *hostP,
			       tkm_file_t *fileP,
			       afs_hyper_t *startPosP,
			       afs_hyper_t *endPosP,
			       long *rightsP);
#endif

static void tkm_ExternalToInternalToken(afs_token_t *externalP,
					tkm_internalToken_t *internalP)
{
    internalP->types = AFS_hgetlo(externalP->type);
    internalP->range.lowBnd = externalP->beginRange;
    internalP->range.upBnd = externalP->endRange;
}

/******************************************************************************
 * tkm_GetToken(fileDescP, flags, tokenP, hostp, reqNumber, lockDesctriptorP)
 * Parameters :
 * 	Input: 	afsfid *fileDescP  = the file's of volume's afsFid
 *		long flags -  can be nothing, QUEUE, NOCONFLICT, OPTIMISTIC
 *	   	struct hs_host *hostp = the host requesting the token
 *	   	u_long reqNumber = the host's request number
 *	Output:	token_t *tokenP =  the token granted
 *	   	afs_recordLock_t *lockDescriptorP -  describes the reason why a
 *						     lock token was not granted
 *
 * Possible Returns:
 *	SUCCESS
 *	TKM_ERROR_REQUESTQUEUED - not really a failure
 *	TKM_ERROR_TOKENCONFLICT
 *	TKM_ERROR_TOKENINVALID
 *	TKM_ERROR_INVALIDID
 *	TKM_ERROR_FILEINVALID
 *	TKM_ERROR_BADHOST
 *	EROFS
 *	TKM_ERROR_NOMEM
 *  
 * General : The exported interface to request a token.
 **************************************************************************/
long tkm_GetToken(afsFid 	*fileDescP,
		  long	flags,
		  afs_token_t	*tokenP,
		  struct hs_host *hostP,
		  u_long reqNumber,
		  afs_recordLock_t *lockDescriptorP)
{
    tkm_internalToken_t *internalTokenP;
    long code = TKM_SUCCESS;
    tkm_file_t *fileP = NULL;
    tkm_vol_t *volP = NULL;
    long reqtype;


    tkm_Init();

    tkm_dprintf(("Starting getToken fid %d.%d.%d.%d.%d.%d types %d flags %d\n",
		 AFS_HGETBOTH(fileDescP->Cell),
		 AFS_HGETBOTH(fileDescP->Volume),
		 fileDescP->Vnode, fileDescP->Unique,
		 AFS_hgetlo(tokenP->type), flags));

    icl_Trace4(tkm_iclSet, TKM_TRACE_GETTOKEN, ICL_TYPE_FID, (long) fileDescP,
	       ICL_TYPE_LONG, flags,
	       ICL_TYPE_HYPER, (long) &tokenP->type,
	       ICL_TYPE_POINTER, hostP);


    reqtype = AFS_hgetlo(tokenP->type);

    if (!(AFS_hfitsinu32(tokenP->type)) ||
	(reqtype < 0) || (reqtype >= TKM_TOKENTYPE_MASK(TKM_ALL_TOKENS))) {
	code = TKM_ERROR_ILLEGALTKNTYPE;
	goto done;
    }

    if (TKM_TYPES_WITH_RANGE(reqtype) != 0) {
	icl_Trace2(tkm_iclSet, TKM_TRACE_GETTOKEN_INRANGES,
		   ICL_TYPE_HYPER, &tokenP->beginRange,
		   ICL_TYPE_HYPER, &tokenP->endRange);
    }

    if (flags & TKM_FLAGS_GETSAMEID) {
	code = tkm_RefreshToken(tokenP, fileDescP, hostP);
	goto done;
    }

    /*
     * If the token request comes from PX in one of the dont sleep
     * cases, guard against dead lock in cases of severe token shortage
     * by using tkm_AllocTokenNoSleep().  Turn off the bit to keep flags
     * clean.
     */
    if (flags & TKM_FLAGS_DONTSLEEP) {
	int got_it = 0;

	flags &= ~(TKM_FLAGS_DONTSLEEP);
	internalTokenP = tkm_AllocTokenNoSleep(1, &got_it);
	if (got_it != 1) {
	    internalTokenP = NULL;
	}
    } else {
	internalTokenP = tkm_AllocToken(1);
    }

    tkm_dprintf(("got from free list %x\n", internalTokenP));
    if (internalTokenP == NULL) {
	code = TKM_ERROR_NOMEM;
	goto done;
    }
    /*
     * internalToken is bzeroed except for the id which is
     * a new unique id.
     */
    internalTokenP->host = hostP;
    HS_HOLD(hostP); /* the HS_RELE() is done when we recycle the token */
    tkm_ExternalToInternalToken(tokenP, internalTokenP);
    if (FID_ISVOL(fileDescP)){
	volP = tkm_FindVol(fileDescP, 1);
	if (volP != NULL){
	    internalTokenP->flags |= TKM_TOKEN_ISVOL;
	    internalTokenP->holder.volume = volP;
	} else {
	    HS_RELE(hostP); /*
			     * need to do it here since internalTokenP
			     * will get in the free list without going
			     * through the full recycle code
			     */
	    tkm_FreeToken(internalTokenP);
	    /*
	     * The error could have been a result of a memory exhaustion or
	     * a volreg_Lookup failure. In both cases return an error that
	     * could cause the CM to try a different site in the case of a 
	     * readonly volume
	     */
	    code =  VOLERR_PERS_DELETED;
	    goto done;
	}
	/*
	 * Check for byte range token on whole-vol token. Non byte range tokens
	 * will start at 0 and have a range that is at least bigger than
	 * 0x7fffffff (MAXLONG for 32 bit machines)
	 */
	if (!AFS_hiszero(tokenP->beginRange) ||
	    AFS_hcmp(tokenP->endRange, osi_hMaxint32) < 0) {

	    HS_RELE(hostP);
	    tkm_FreeToken(internalTokenP);
	    tkm_ReleVol(volP);
	    code = TKM_ERROR_TOKENINVALID;
	    goto done;
	}
    } else {
	fileP = tkm_FindFile(fileDescP, 1);
	if (fileP != NULL){
	    internalTokenP->holder.file = fileP;
	    volP = internalTokenP->holder.file->vol;
	} else {
	    HS_RELE(hostP);
	    tkm_FreeToken(internalTokenP);
	    /*
	     * same as above. Since it could be due to volreg_Lookup failing
	     * and it could be a file in an RO volume send an error that will
	     * make the CM try a different site.
	     */
	    code =  VOLERR_PERS_DELETED;
	    goto done;
	}

    }

    /*
     * Tag the host as possibly possessing a token.
     * This bit is set only once in the life of a host object.
     * Doesn't matter if there's a locking race while checking this bit.
     */
    if ((hostP->states & HS_HOST_HADTOKENS) == 0) {
	lock_ObtainWrite(&hostP->lock);
	hostP->states |= HS_HOST_HADTOKENS;
	lock_ReleaseWrite(&hostP->lock);
    }

    /*
     * freeTokens(fid) is the set of token types that can always be
     * granted (for example READ_DATA for files in readonly volumes)
     */

    if ((reqtype & TKM_FREETOKENS(volP)) == reqtype) {
	TKM_MAKE_FREETOKEN(tokenP);
	tokenP->expirationTime = TKM_TOKEN_NOEXPTIME;
	if (flags & TKM_FLAGS_OPTIMISTICGRANT) {
	    AFS_hset64(tokenP->type, 0,
		       (TKM_FREE_RO_TOKENS &
			~(TKM_TOKENTYPE_MASK(TKM_SPOT_HERE))));
	}
	HS_RELE(hostP);
	tkm_FreeToken(internalTokenP);
	code = TKM_SUCCESS;
	if (fileP != NULL)
	    tkm_ReleFile(fileP);
	else
	    tkm_ReleVol(volP);
	goto done;
    }
    /*
     * maxTokens(fid) is the set of token types that can possibly be
     * granted (for example WRITE_DATA is not in that set for files
     * in readonly volumes)
     */
    if ((reqtype & (TKM_MAXTOKENS(volP))) != reqtype) {
	HS_RELE(hostP);
	tkm_FreeToken(internalTokenP);
	code = EROFS;
	if (fileP != NULL)
	    tkm_ReleFile(fileP);
	else
	    tkm_ReleVol(volP);
	goto done;
    }
    if (fileP != NULL) {
	code = tkm_GetFileToken(internalTokenP, flags, lockDescriptorP);
	tkm_ReleFile(fileP);
    } else {
	code = tkm_GetVolToken(internalTokenP, flags);
	tkm_ReleVol(volP);
    }

    tkm_dprintf(("Did getToken for %x (%u,,%u) types %d code %d\n",
		 internalTokenP, AFS_HGETBOTH(internalTokenP->id),
		 internalTokenP->types, code));

    if ((code == TKM_SUCCESS) || (code == TKM_ERROR_REQUESTQUEUED)){
	internalTokenP->reqNum = reqNumber;
	if (flags & TKM_FLAGS_NOEXPIRE)
	    internalTokenP->flags |= TKM_TOKEN_FOREVER;
	internalTokenP->expiration = tkm_FreshExpTime(internalTokenP);
	tkm_InternalToExternalToken(internalTokenP, tokenP);
    } else {
	HS_RELE(hostP);
	tkm_FreeToken(internalTokenP);
    }

  done:
    icl_Trace3(tkm_iclSet, TKM_TRACE_GETTOKENEND, ICL_TYPE_LONG, code,
             ICL_TYPE_HYPER, (long) &tokenP->type,
             ICL_TYPE_HYPER, (long) &tokenP->tokenID);
    return(code);
}

/******************************************************************************
 * tkm_ReturnToken(fileDescP, tokenIDP, rightsP, flags)
 *
 * Parameters :
 *	Input: 	fileDescP = the file's of volume's afsFid
 *		tokenIDP  = the id of the token returned
 *		rightsP  = the rights returned
 *		flags 	 - XXX ignored
 *
 * Possible Returns:
 *	TKM_ERROR_NOENTRY
 *	TKM_ERROR_TOKENINVALID
 *
 * General: Return a set of rights for a token
 *****************************************************************************/

long tkm_ReturnToken(afsFid   	 *fileDescP,
		     afs_hyper_t *tokenIDP,
		     afs_hyper_t *rightsP,
		     long 	flags)
{
    tkm_vol_t *volP = NULL;
    tkm_file_t *fileP = NULL;
    tkm_internalToken_t *thisTokenP;
    long retRights;

    tkm_dprintf(("tkm_ReturnToken id %u,,%u types %d\n",
		 AFS_HGETBOTH(*tokenIDP), AFS_hgetlo(*rightsP)));
    icl_Trace3(tkm_iclSet, TKM_TRACE_RETTOKEN, ICL_TYPE_FID, (long) fileDescP,
	       ICL_TYPE_HYPER, (long) tokenIDP,
	       ICL_TYPE_HYPER, (long) rightsP);

    if (TKM_FREE_TOKENID(*tokenIDP))
	return(TKM_SUCCESS);

    retRights = AFS_hgetlo(*rightsP);

    if (FID_ISVOL(fileDescP)){
	volP = tkm_FindVol(fileDescP, 0);
	if (volP == NULL)
	    return TKM_ERROR_NOENTRY;
	lock_ObtainWrite(&(volP->lock));
	thisTokenP = volP->granted.list;
    } else {
	fileP = tkm_FindFile(fileDescP, 0);
	if (fileP == NULL)
	    return TKM_ERROR_NOENTRY;
	volP = fileP->vol;
	lock_ObtainWrite(&(fileP->lock));
	thisTokenP = fileP->granted.list;
    }
    while (thisTokenP != NULL) {
	if (AFS_hsame(thisTokenP->id, *tokenIDP)){
	    if (fileP != NULL) {
		/*
		 * this is a file token and we need to lock the volume
		 * structure as well as the file structure to free a token
		 */
		osi_assert(fileP == thisTokenP->holder.file);
		lock_ObtainWrite(&(fileP->vol->lock));
		tkm_dprintf(("return %d for %x\n", retRights, thisTokenP));
		tkm_RemoveTypes(thisTokenP, retRights);
		/*
		 * If retRights is 0, then a cache manager may be returning
		 * a piece of a lock token.  Since there is no protocol
		 * for the cache manager to generate it's own slice and
		 * dice tokens it makes this call to trigger async grant
		 * attempts.
		 */
                if ((retRights == 0) && (thisTokenP->types) &&
                    !(thisTokenP->flags & TKM_TOKEN_GRANTING)) {
                    thisTokenP->refused = 0; /*
                                              * make sure that the revoke
                                              * RPCs are executed
                                              */
                    tkm_TriggerAsyncGrants(thisTokenP);
		}
		lock_ReleaseWrite(&(fileP->vol->lock));
	    } else {
		tkm_dprintf(("return %d for %x\n", retRights, thisTokenP));
		tkm_RemoveTypes(thisTokenP, retRights);
	    }
	    break;
	} else
	    thisTokenP = thisTokenP->next;
    }

    if (fileP != NULL){
	lock_ReleaseWrite(&(fileP->lock));
	tkm_ReleFile(fileP);
    } else {
	osi_assert(volP != NULL);
	lock_ReleaseWrite(&(volP->lock));
	tkm_ReleVol(volP);
    }
    /* return success even if the token returned was not found */
    return TKM_SUCCESS;
}


static tkm_RefreshToken(afs_token_t	 *externalP,
			afsFid   	*fileDescP,
			struct hs_host *hostP)
{
    tkm_internalToken_t *thisTokenP;
    tkm_vol_t *volP = NULL;
    tkm_file_t *fileP = NULL;
    long reqtype;


    tkm_dprintf(("In refreshtoken\n"));

    if (TKM_FREE_TOKENID(externalP->tokenID))
	return(TKM_SUCCESS);

    if (FID_ISVOL(fileDescP)){
	volP = tkm_FindVol(fileDescP, 0);
	if (volP == NULL)
	    return TKM_ERROR_NOENTRY;
	lock_ObtainWrite(&(volP->lock));
	thisTokenP = volP->granted.list;
    } else {
	fileP = tkm_FindFile(fileDescP, 0);
	if (fileP == NULL)
	    return TKM_ERROR_NOENTRY;
	lock_ObtainWrite(&(fileP->lock));
	volP = fileP->vol;
	thisTokenP = fileP->granted.list;
    }
    reqtype = AFS_hgetlo(externalP->type);
    while (thisTokenP != NULL) {
	if (AFS_hsame(thisTokenP->id, externalP->tokenID)){
	    /* Ensure that this token covers the request. */
	    if ((reqtype & thisTokenP->types) == reqtype) {
		thisTokenP->expiration = tkm_FreshExpTime(thisTokenP);
		externalP->expirationTime = thisTokenP->expiration;
	    } else {
		thisTokenP = NULL;
	    }
	    break;
	} else
	    thisTokenP = thisTokenP->next;
    }
    if (fileP != NULL){
	lock_ReleaseWrite(&(fileP->lock));
	tkm_ReleFile(fileP);
    } else {
	osi_assert(volP != NULL);
	lock_ReleaseWrite(&(volP->lock));
	tkm_ReleVol(volP);
    }
    if (thisTokenP == NULL)
	return TKM_ERROR_INVALIDID;
    /* move to the tail of the GC list */
    tkm_RemoveFromGcList(thisTokenP);
    tkm_AddToGcList(thisTokenP);
    return(TKM_SUCCESS);
}
/*************************************/

static void tkm_VolRightsHeld(struct hs_host *hostP,
			      tkm_vol_t *volP,
			      long *rightsP)
{
    tkm_internalToken_t *thisTokenP;

    lock_ObtainWrite(&(volP->lock));
    thisTokenP = volP->granted.list;
    while (thisTokenP != NULL){
	if (thisTokenP->host == hostP){
	    *rightsP |= thisTokenP->types;
	}
	thisTokenP = thisTokenP->next;
    }
    lock_ReleaseWrite(&(volP->lock));
    tkm_dprintf(("checking rights for %x got %d\n", volP, *rightsP));
}

static void tkm_FileRightsHeld(struct hs_host *hostP,
			       tkm_file_t *fileP,
			       afs_hyper_t *startPosP,
			       afs_hyper_t *endPosP,
			       long *rightsP)
{
    tkm_internalToken_t *thisTokenP;
    tkm_byterange_t inRange;

    lock_ObtainWrite(&(fileP->lock));
    if (fileP->host == hostP){
	long allHeld = fileP->granted.mask->typeSum;
	/*
	 * hostP is the only host holding rights on this file so we
	 * can optimize but only if we know that there are no byteranged
	 * tokens in the list.
	 * XXX If we knew what rights the caller was interested in we could
	 * use this optimization more often which would be important since
	 * this routine is mostly called to check for STATUS* rights
	 */
	if (TKM_TYPES_WITHOUT_RANGE(allHeld) == allHeld) {
	    *rightsP = allHeld;
	    lock_ReleaseWrite(&(fileP->lock));
	    return;
	}
    } else {
	if (fileP->host != NULL) {
	    /* some other host is the only host holding rights on this file */
	    lock_ReleaseWrite(&(fileP->lock));
	    return;
	}
    }
    TKM_BYTERANGE_SET_RANGE(&inRange, startPosP, endPosP);
    thisTokenP = fileP->granted.list;
    while (thisTokenP != NULL){
	if ((thisTokenP->host == hostP) &&
	    (TKM_BYTERANGE_IS_SUBRANGE(&inRange, &(thisTokenP->range)))) {
	    *rightsP |= thisTokenP->types;
	}
	thisTokenP = thisTokenP->next;
    }
    lock_ReleaseWrite(&(fileP->lock));
    tkm_dprintf(("checking rights for %x got %d\n", fileP, *rightsP));
}


/******************************************************************************
 * tkm_GetRightsHeld(hostP, fidP, startPos, endPos, rightsP)
 * Parameters :
 *	Input: 	fidP =  the file's of volume's afsFid
 *		hostP = the host that we are queried about
 *		startPos -> endPos = the byte range that we are queried about
 *	Output:
 *		rightsP = the rights held
 *
 * Possible Returns
 *	TKM_ERROR_NOENTRY
 *	TKM_SUCCESS
 * General: This routine returns the rights held by a host for a given file
 * 	    or volume and in the file's case for a given byte range
 *****************************************************************************/

long tkm_GetRightsHeld(struct hs_host *hostP,
		       afsFid *fidP,
		       afs_hyper_t *startPosP,
		       afs_hyper_t *endPosP,
		       afs_hyper_t *rightsP)
{
    tkm_vol_t *volP;
    tkm_file_t *fileP;
#ifdef SGIMIPS
    /*  This value gets "|"d so it needs initial setting. */
    long held = 0;
#else
    long held;
#endif

    tkm_dprintf(("Checking rights of %x on %d.%d.%d.%d.%d.%d\n", hostP,
		 AFS_HGETBOTH(fidP->Cell), AFS_HGETBOTH(fidP->Volume),
		 fidP->Vnode, fidP->Unique));
    icl_Trace4(tkm_iclSet, TKM_TRACE_GETRIGHTS, ICL_TYPE_POINTER, hostP,
	       ICL_TYPE_FID, (long) fidP,
	       ICL_TYPE_HYPER, (long) startPosP,
	       ICL_TYPE_HYPER, (long) endPosP);

    if (FID_ISVOL(fidP)){
	volP = tkm_FindVol(fidP, 0);
	if (volP == NULL)
	    return TKM_ERROR_NOENTRY;
	tkm_VolRightsHeld( hostP, volP, &held);
	tkm_ReleVol(volP);
    } else {
	fileP = tkm_FindFile(fidP, 0);
	if (fileP == NULL)
	    return TKM_ERROR_NOENTRY;
	tkm_FileRightsHeld( hostP, fileP, startPosP, endPosP, &held);
	tkm_ReleFile(fileP);
    }
    AFS_hset64(*rightsP, 0, held);
    return(TKM_SUCCESS);
}



void tkm_InternalToExternalToken(tkm_internalToken_t *internalP,
				 afs_token_t * externalP)
{
    externalP->tokenID = internalP->id;
    externalP->expirationTime = internalP-> expiration;
    AFS_hset64(externalP->type, 0, internalP->types);
    externalP->beginRange = internalP->range.lowBnd;
    externalP->endRange = internalP->range.upBnd;
}

void tkm_CopyTokenFid(tkm_internalToken_t *tokenP,
		      afsFid *fidP)
{
    if (!(tokenP->flags & TKM_TOKEN_ISVOL)){
	FidCopy(fidP, (&(tokenP->holder.file->id)));
    }
    else {
	fidP->Cell = tokenP->holder.volume->cell;
	fidP->Volume = tokenP->holder.volume->id;
	fidP->Vnode = TKM_AFSFID_WHOLE_VOL_VNODE;
	fidP->Unique = TKM_AFSFID_WHOLE_VOL_UNIQUE;
    }
}

int tkm_ResetTokensFromHost(struct hs_host *hostp)
{
    /*
     * XXX this call is assumed to have been used for debugging purposes
     * only so I won't bother implementing it. I need it defined for dfs to
     * modload.
     */
    return(0);
}
