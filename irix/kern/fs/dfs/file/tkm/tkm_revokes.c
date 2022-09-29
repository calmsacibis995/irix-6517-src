/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_revokes.c,v 65.4 1998/04/01 14:16:32 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * Copyright (c) 1996, 1994, Transarc Corp.
 */

#include <dcedfs/tpq.h>
#include "tkm_internal.h"
#include "tkm_misc.h"
#include "tkm_asyncgrant.h"
#include "tkm_conflicts.h"
#include "tkm_recycle.h"
#include "tkm_revokes.h"
#ifdef SGIMIPS
#include "tkm_file.h"
#include "tkm_volume.h"
#endif

typedef struct tkm_hostRevokeElem {
    struct hs_host	*hostP;
    afsRevokeDesc	*revokeListP;
    unsigned int	numbRevokes;
    tkm_tokenConflictQ_t *conflicts;
    int			*conflictIndex;
    long 		revokeCode;
    afs_recordLock_t 	*savedLockP; /* describing what lock blocked us */
    osi_dlock_t		*assignmentLockP; /*
					   * used to make the assignment
					   * from the above to the common
					   * lock descriptor returned MT-safe
					   */
    int 		revokesAlloced;
} tkm_hostRevokeElem_t;

typedef struct tkm_hostRevokeList {
    tkm_hostRevokeElem_t	list[TKM_MAXPARALLELRPC];
    unsigned int	numElems;
    osi_dlock_t		listLock;
} tkm_hostRevokeList_t;

#ifdef KERNEL
#include <dcedfs/icl.h>
#include <dcedfs/tkm_trace.h>
static struct icl_set *tkm_revokeIclSet;
#endif /*KERNEL*/

#ifdef SGIMIPS
static long tkm_MostSevere(long	code1, long code2);
static void tkm_DoRevoke (void *argP,  void *ignoredTpqOpHandle);
static tkm_PreProcessConflictQ(tkm_tokenConflictQ_t *conflicts,
			       int flags);
static void tkm_ConflictToAfsRevokeDesc(tkm_tokenConflictElem_t  *conflict,
				        afsRevokeDesc	     *description);
static void tkm_AdjustAccepted(tkm_internalToken_t *internalP,
			       afs_token_t *offeredP,
			       long accepted);
static int tkm_FindRevokeHost(struct hs_host *hostP,
			      tkm_hostRevokeList_t *listP);
static int tkm_AddRevokeHost(struct hs_host *hostP,
			     tkm_hostRevokeList_t *listP,
			     int maxConflicts,
			     afs_recordLock_t *lockDescriptorP,
			     osi_dlock_t *lockP);
void tkm_ReInitHostList(tkm_hostRevokeList_t *listP);
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_revokes.c,v 65.4 1998/04/01 14:16:32 gwehrman Exp $")

void tkm_InitRevokes()
{
#ifdef KERNEL
    {
	struct icl_log *logp;
	int code;

	code = icl_CreateLog("cmfx", 0, &logp);
	if (code == 0)
	    code = icl_CreateSetWithFlags("tkm/revokes", logp, 0,
					  ICL_CRSET_FLAG_DEFAULT_OFF,
					  &tkm_revokeIclSet);
	osi_assert(code == 0);
    }
#endif /* KERNEL */
}

/*
 *  The following routine is used for picking the most severe tkm return
 * code out of the list of those returned by parallel revocations.  The
 * three possible return codes from the revocation routine are, ordered from
 * least to most severe:
 *	TKM_SUCCESS < TKM_ERROR_TRYAGAIN < TKM_ERROR_TOKENCONFLICT
 */
static long tkm_MostSevere(long	code1, long code2)
{
  long		rtnVal = TKM_SUCCESS;

  /*
   *  to follow that we cover all the cases, consider the possible combinations
   * as two-digit, base 3 numbers
   */
  if ((code1 == TKM_ERROR_TOKENCONFLICT) || (code2 == TKM_ERROR_TOKENCONFLICT)) {
    /* covers cases:
     *	02
     *	12
     *	20
     *	21
     *	22
     */
    rtnVal = TKM_ERROR_TOKENCONFLICT;
  }
  else {
    if (code1 == TKM_SUCCESS) {
      /* covers cases:
       *	00
       *	01
       *	(02 already covered above)
       */
      rtnVal = code2;
    }
    else if (code2 == TKM_SUCCESS) {
      /* covers cases:
       *	(00 already covered above)
       *	10
       *	(20 already covered above)
       */
      rtnVal = code1;
    }
    else {
      /* they are both TKM_ERROR_TRYAGAIN */
      /* covers cases:
       *	11
       */
      rtnVal = code1;
    }
  }

  return rtnVal;
}

/*
 *  Actual Revocation routine
 *  This routine assumes no locks are held on entry
 */
static void tkm_DoRevoke (void *argP,  void *ignoredTpqOpHandle)
{
    /* The un-marshalled arguments */
    struct hs_host *myHostP;
    afsRevokeDesc  *myRevokesP, *firstRevokeP;
    unsigned int    myNumbRevokes;
    afs_recordLock_t *myLockP;
    tkm_tokenConflictQ_t *allConflicts;
    tkm_tokenConflictElem_t *conflict;
    int 		      *myConflicts;

    /* other helpful variables */
    long  rtnVal = TKM_SUCCESS;
    long  code;
    tkm_internalToken_t	*tokenP, *highP, *lowP;
    afsRevokeDesc *revoke;
    long refused, accepted, typesRevoked;
    int i,j;
    tkm_file_t *fileP;
    tkm_vol_t *volP;

    myHostP = ((tkm_hostRevokeElem_t *)argP)->hostP;
    myRevokesP = ((tkm_hostRevokeElem_t *)argP)->revokeListP;
    myNumbRevokes = ((tkm_hostRevokeElem_t *)argP)->numbRevokes;
    allConflicts = ((tkm_hostRevokeElem_t *)argP)->conflicts;
    myConflicts = ((tkm_hostRevokeElem_t *)argP)->conflictIndex;

    icl_Trace3(tkm_revokeIclSet, TKM_TRACE_REVOKE_START,
	       ICL_TYPE_POINTER, (long) allConflicts,
               ICL_TYPE_POINTER, (long) myHostP,
               ICL_TYPE_LONG, myNumbRevokes);

    code = HS_REVOKETOKEN(myHostP, myNumbRevokes, myRevokesP);

    icl_Trace3(tkm_revokeIclSet, TKM_TRACE_REVOKE_END,
	       ICL_TYPE_POINTER, (long) allConflicts,
               ICL_TYPE_POINTER, (long) myHostP,
               ICL_TYPE_LONG, code);

    firstRevokeP = myRevokesP;
    
    if ((code == HS_SUCCESS) || (code == HS_ERR_PARTIAL)){
	for (i=0; i < myNumbRevokes; i++) {
	    conflict =  &(allConflicts->elements[*(myConflicts++)]);
	    revoke = myRevokesP++;
	    tokenP = conflict->token;
	    osi_assert((tokenP != NULL) &&
		       AFS_hsame(tokenP->id, revoke->tokenID));
	    TKM_PREPARE_FOR_TOKEN_RECYCLE(tokenP, fileP, volP);
	    typesRevoked = AFS_hgetlo(revoke->type);
	    refused = AFS_hgetlo(revoke->errorIDs);
	    refused &= typesRevoked; /* we can only be refused if we asked */
	    typesRevoked &= ~refused;
	    if ((code != HS_SUCCESS) && (refused != 0)) {
		/*
		 * if some of the rights for this token were refused we
		 * need to keep track of that. 
		 * Don't keep track if this revocation was refused for
		 * the garbage collection thread since the CM may
		 * legitimately refuse revocations on behalf of
		 * garbage collection that it would not otherwise. 
		 * Hence we should cause real revocations from non
		 * garbage collection threads to be refused within the
		 * next TKM_REFUSED_VALID_TIME time.
		 */
		if (!(revoke->flags & AFS_REVOKE_DUE_TO_GC)) {
		    tokenP->refused |= refused;
		    tokenP->lastRefused = osi_Time();
		}
		/* copy out the lock descriptor info, if desired */
		/*
		 * Many hosts could be returning this information but it
		 * doesn't matter which one of them we return. We have to
		 * use the lock however to make the assignment MT-safe
		 */
		myLockP = ((tkm_hostRevokeElem_t *)argP)->savedLockP;
		if (myLockP != NULL) {
		    for (j = 0; j < myNumbRevokes; j++) {
			if (firstRevokeP[j].outFlags
			    & AFS_REVOKE_LOCKDATA_VALID) {
			    lock_ObtainWrite(((tkm_hostRevokeElem_t *)
					      argP)->assignmentLockP);
			    *myLockP = firstRevokeP[j].recordLock;
			    lock_ReleaseWrite(((tkm_hostRevokeElem_t *)
					      argP)->assignmentLockP);
			    break;
			}
		    }
		}
		/* lastRefused time is not strictly necessary in this trace */
		icl_Trace4(tkm_revokeIclSet, TKM_TRACE_REVOKE_REFUSE,
			  ICL_TYPE_HYPER, &(tokenP->id),
			  ICL_TYPE_HYPER, &(revoke->type),
			  ICL_TYPE_HYPER, &(revoke->errorIDs),
			  ICL_TYPE_LONG, tokenP->lastRefused);
		tkm_dprintf(("refused to revoke %d for %x\n", 
			     refused, tokenP));
		rtnVal = TKM_ERROR_TOKENCONFLICT;
	    } else { 
		/* 
		 * HS_SUCCESS or it wasn't this token that had types refused
		 */

		icl_Trace2(tkm_revokeIclSet, TKM_TRACE_REVOKE_SUCCESS,
			  ICL_TYPE_HYPER, &(tokenP->id),
			  ICL_TYPE_HYPER, &(revoke->type));
		/*
		 * we can remove the token from the conflict queue since
		 * all the rights that we requested were revoked
		 */
		(void) tkm_RemoveFromConflictQ(allConflicts, tokenP);
		/*
		 * don't use TKM_RELETOKEN here since it will do more work
		 * than it needs to
		 */
		tokenP->refcount--;
	    }

	    tkm_RemoveTypes(tokenP, typesRevoked);
	    TKM_UNLOCK_AFTER_TOKEN_RECYCLE(fileP, volP);
	    if ((revoke->flags & AFS_REVOKE_COL_A_VALID) != 0) {
		accepted = AFS_hgetlo(revoke->colAChoice);
		tkm_dprintf(("-- %x.A client accepted %d.%d (%d)\n", revoke,
			     AFS_HGETBOTH(revoke->colAChoice), accepted));
		tkm_AdjustAccepted(conflict->grantLow,
				   &(revoke->columnA),  accepted);
	    }
	    if ((revoke->flags & AFS_REVOKE_COL_B_VALID) != 0) {
		accepted = AFS_hgetlo(revoke->colBChoice);
		tkm_dprintf(("-- %x.B client accepted %d.%d (%d)\n", revoke,
			     AFS_HGETBOTH(revoke->colBChoice), accepted));
		tkm_AdjustAccepted(conflict->grantHigh,
				   &(revoke->columnB),  accepted);
	    }
	}
    } else { /* all other return codes except HS_SUCCESS and HS_ERR_PARTIAL */
	if (code == HS_ERR_TRYAGAIN) {
	    rtnVal = TKM_ERROR_TRYAGAIN;
	} else
	    rtnVal = TKM_ERROR_HARDCONFLICT; /* or panic? */
    }

    icl_Trace3(tkm_revokeIclSet, TKM_TRACE_REVOKE_COMPLETE,
	       ICL_TYPE_POINTER, (long) allConflicts,
               ICL_TYPE_POINTER, (long) myHostP,
               ICL_TYPE_LONG, rtnVal);

    ((tkm_hostRevokeElem_t *)argP)->revokeCode = rtnVal;
}

/***********************************************************************
 * long tkm_ParallelRevoke (conflicts, flags, lockDescriptorP)
 *
 * Input Params: flags & ..DOINGGC = is this revocation for GC purposes?
 * Input Params: flags & ..FORVOLTOKEN = is this revocation to get a vol token?
 *
 * Inout Params: conflicts = a conflict representation of the tokens that
 *			     need to be revoked. All the tokens in this
 *			     list are held. On exit the list contains the
 *			     tokens that were refused (still held). The
 *			     tokens that were succesfully revoked are freed.
 *			     This list additonally contains all the slice
 *			     and dice tokens to be offered for each revoke
 * Output Params : lockDescriptoP = if applicable describes the lock that
 *				    blocked the grant of a lock token
 *
 * LOCKING: Must be called without any locks held since it will
 *	    probably issue RPCs.
 *
 * GENERAL: tokens that have the TKM_TOKEN_GRANTING flag on cannot be
 *	    revoked, so this routine has to sleep. The token->refused
 *	    mask is is only used for efficiency (to avoid sending
 *	    revocations to tokens that have already been refused)
 ********************************************************************/

long tkm_ParallelRevoke (tkm_tokenConflictQ_t *conflicts,
			 int flags,
			 afs_recordLock_t *lockDescriptorP)
{
    tkm_hostRevokeList_t *revokeHosts;
    tpq_pardo_clientop_t	*clientOpsP;
    tkm_hostRevokeElem_t *thisHost;
    tkm_tokenConflictElem_t *thisConflict;
    struct hs_host *hostP;
    long code = TKM_SUCCESS;
    int pos = 0, hnum, thisRevoke, i;
    tkm_internalToken_t *tokenP;
    struct afsRevokeDesc *description;

    icl_Trace3(tkm_revokeIclSet, TKM_TRACE_PARALLEL_REVOKE_BEGIN,
	       ICL_TYPE_POINTER, conflicts,
	       ICL_TYPE_LONG, conflicts->numElements,
	       ICL_TYPE_LONG, flags);

    if (TKM_CONFLICTQ_ISEMPTY(conflicts)) {
	icl_Trace1(tkm_revokeIclSet, TKM_TRACE_PARALLEL_REVOKE_EMPTY,
		   ICL_TYPE_POINTER, conflicts);
	goto finish;
    }

    code = tkm_PreProcessConflictQ(conflicts, flags);
    if (code != TKM_SUCCESS) {
	icl_Trace2(tkm_revokeIclSet, TKM_TRACE_PARALLEL_REVOKE_PREPROCESS,
		   ICL_TYPE_POINTER, conflicts,
		   ICL_TYPE_LONG, code);
	goto finish;
    }

    if (TKM_CONFLICTQ_ISEMPTY(conflicts)) {
	icl_Trace1(tkm_revokeIclSet, 
		   TKM_TRACE_PARALLEL_REVOKE_PREPROCESS_EMPTY,
		   ICL_TYPE_POINTER, conflicts);
	goto finish;
    }

    /*
     * At this point the conflict queue (conflicts) contains only tokens
     * that are not expired and can be revoked (i.e. they do not have
     * TKM_TOKEN_GRANTING set). TKM_TOKEN_GRANTING is set after a token
     * is added to a file's granted list and before dropping the file's
     * lock to perform the RPC
     */
    clientOpsP = (tpq_pardo_clientop_t *)
		osi_Alloc(TKM_MAXPARALLELRPC* sizeof(tpq_pardo_clientop_t));
    osi_assert(clientOpsP != NULL);
    bzero( (caddr_t) clientOpsP,
	  TKM_MAXPARALLELRPC* sizeof(tpq_pardo_clientop_t));

    revokeHosts = (tkm_hostRevokeList_t *) 
	osi_Alloc (sizeof(tkm_hostRevokeList_t));
    revokeHosts->numElems = 0;
    lock_Init(&revokeHosts->listLock);
    while (pos < conflicts->lastPos) {
	thisConflict =  &(conflicts->elements[pos]);
	tokenP = thisConflict->token;
	if (tokenP == NULL){
	    pos++;
	    continue;
	}
	osi_assert(tokenP->refcount > 0); /* it better be held */
	hostP = tokenP->host;
	if ((hnum = tkm_FindRevokeHost(hostP, revokeHosts)) < 0) {
	    if ((hnum =  tkm_AddRevokeHost(hostP,
					   revokeHosts,
					   conflicts->lastPos,
					   lockDescriptorP,
					   &revokeHosts->listLock)) >= 0) {
		clientOpsP[hnum].op = tkm_DoRevoke;
		clientOpsP[hnum].argP = (void *) &(revokeHosts->list[hnum]);
		revokeHosts->list[hnum].conflicts = conflicts;
	    } else {
		/*
		 * this is a new host and we already have a list of hosts
		 * as long as the maximum number of RPCs that we can issue
		 * so let's just do the RPCs for the hosts already in the list
		 */
		icl_Trace2(tkm_revokeIclSet, 
			   TKM_TRACE_PARALLEL_REVOKE_INVOKE_REVOKE_THREADS,
			   ICL_TYPE_POINTER, conflicts,
			   ICL_TYPE_LONG, revokeHosts->numElems);

		tpq_Pardo(tkm_threadPoolHandle, clientOpsP,
			  revokeHosts->numElems, TPQ_HIGHPRI, 0);
		/* return the most severe return code found */
		for (i = 0; i < revokeHosts->numElems ; i++) {
		    code = tkm_MostSevere(code,
					  revokeHosts->list[i].revokeCode);
		}
		/* We are done with this group of hosts */
		tkm_ReInitHostList(revokeHosts);
		if (code != TKM_SUCCESS) 
		    goto cleanup;	/* free the allocated space and return */
		bzero((caddr_t) clientOpsP,
		      TKM_MAXPARALLELRPC* sizeof(tpq_pardo_clientop_t));
		/*
		 * we haven't incremented pos so the continue will reprocess
		 * the host that didn't fit
		 */
		continue;
	    }
	}
	osi_assert(hnum < TKM_MAXPARALLELRPC);
	thisHost = &(revokeHosts->list[hnum]);
	thisRevoke = thisHost->numbRevokes++;
	tkm_dprintf(("working on host %d address %x revoke %d\n", hnum, thisHost,
	       thisRevoke));
	thisHost->conflictIndex[thisRevoke] = pos++;
 	description = &(thisHost->revokeListP[thisRevoke]);
	tkm_ConflictToAfsRevokeDesc(thisConflict, description);
	if (flags & TKM_PARA_REVOKE_FLAG_DOINGGC)
	    description->flags |= AFS_REVOKE_DUE_TO_GC;
    }
    /*
     * do the revocations grouped after the last call to
     * tpq_Pardo and before exiting the loop
     */
    if (revokeHosts->numElems > 0) {
	icl_Trace2(tkm_revokeIclSet, 
		   TKM_TRACE_PARALLEL_REVOKE_INVOKE_REVOKE_THREADS,
		   ICL_TYPE_POINTER, conflicts,
		   ICL_TYPE_LONG, revokeHosts->numElems);
	tpq_Pardo(tkm_threadPoolHandle, clientOpsP,
		  revokeHosts->numElems, TPQ_HIGHPRI, 0);
	/* return the most severe return code found */
	for (i = 0; i < revokeHosts->numElems ; i++) {
	    code = tkm_MostSevere(code,
				  revokeHosts->list[i].revokeCode);
	}
	/* we need to do that to free the dynamically allocated lists */
	tkm_ReInitHostList(revokeHosts);
    }
  cleanup:
    osi_Free(clientOpsP, TKM_MAXPARALLELRPC* sizeof(tpq_pardo_clientop_t));
    lock_Destroy(&revokeHosts->listLock);
    osi_Free(revokeHosts, sizeof(tkm_hostRevokeList_t));

    icl_Trace2(tkm_revokeIclSet, TKM_TRACE_PARALLEL_REVOKE_RESULT,
	      ICL_TYPE_POINTER, conflicts,
	      ICL_TYPE_LONG, code);

  finish:
    return(code);
}

static tkm_PreProcessConflictQ(tkm_tokenConflictQ_t *conflicts,
			       int flags)
{
    tkm_file_t *fileP = NULL;
    tkm_vol_t *volP = NULL;
    tkm_internalToken_t *tokenP;
    long code = TKM_SUCCESS;
    int i, pos;

    for (i=0; i < conflicts->lastPos; i++){
	tokenP = conflicts->elements[i].token;
	TKM_PREPARE_FOR_TOKEN_RECYCLE(tokenP, fileP, volP);
	/*
	 * to check the token's type and expiration we need the
	 * token holder's lock but TKM_PREPARE_FOR_TOKEN_RECYCLE()
	 * has already gotten that for us
	 */
	if (TKM_TOKEN_EXPIRED(tokenP) || (tokenP->types == 0)) {
	    tkm_internalToken_t *tmp;
	    int deletedGrants = 0;

	    pos = tkm_RemoveFromConflictQ(conflicts, tokenP);
	    TKM_RELE_AND_RECYCLE_TOKEN(tokenP);
	    /*
	     * now we also have to get rid of the slice & dice tokens
	     * since they can't be revoked
	     */
	    if (pos >= 0) {
		tmp = conflicts->elements[pos].grantLow;
		if (tmp != NULL) {
		    tmp->flags &= ~TKM_TOKEN_GRANTING;
		    TKM_RECYCLE_TOKEN(tmp);
		    deletedGrants = 1;
		}
		tmp = conflicts->elements[pos].grantHigh;
		if (tmp != NULL) {
		    tmp->flags &= ~TKM_TOKEN_GRANTING;
		    TKM_RECYCLE_TOKEN(tmp);
		    deletedGrants = 1;
		}
		if (deletedGrants)
		    osi_Wakeup(&(fileP->granted));
	    }
	    TKM_UNLOCK_AFTER_TOKEN_RECYCLE(fileP, volP);
	    continue;
	}

	/*
	 * See if we can avoid doing the RPC by checking the refused mask.
	 * Note that before using this optimization we need to make sure
	 * that we are trying to revoke the entire byte range of the token
	 * (since the refused mask cannot represent ranges) and that we've
	 * heard from the client holding the token lately (i.e. we've tried
	 * a revocation in the past TKM_REFUSED_VALID_TIME seconds) since
	 * there is no interface from the hshost package to TKM to inform us
	 * when a client holding tokens is marked to be unreachable
	 */

	if (((tokenP->refused & conflicts->elements[i].revoke) != 0) &&
	    (conflicts->elements[i].grantLow == NULL &&
	     conflicts->elements[i].grantHigh == NULL) &&
	    !(flags & TKM_PARA_REVOKE_FLAG_FORVOLTOKEN) &&
	    ((osi_Time() - tokenP->lastRefused) < TKM_REFUSED_VALID_TIME)) {

	    if (flags & TKM_PARA_REVOKE_FLAG_DOINGGC) {
		(void) tkm_RemoveFromConflictQ(conflicts, tokenP);
		TKM_RELETOKEN(tokenP);
		TKM_UNLOCK_AFTER_TOKEN_RECYCLE(fileP, volP);
		continue; /* this is the GC case */
	    } else {
		afs_hyper_t rights;

		code = TKM_ERROR_TOKENCONFLICT;

		/* Yuck I really hate this but want to prevent 2 icl traces */
		AFS_hset64(rights,
			   tokenP->refused, conflicts->elements[i].revoke);
		icl_Trace4(tkm_revokeIclSet, 
			   TKM_TRACE_REVOKE_PREPROCESS_UNEXPIRED_REFUSAL,
			   ICL_TYPE_POINTER, conflicts,
			   ICL_TYPE_HYPER, &(tokenP->id),
			   ICL_TYPE_HYPER, &rights,
			   ICL_TYPE_LONG, tokenP->lastRefused);
		TKM_UNLOCK_AFTER_TOKEN_RECYCLE(fileP, volP);
		break;
	    }
	    /* NOTREACHED */
	}
	if (tokenP->flags & TKM_TOKEN_GRANTING){
	    /*
	     * TKM_TOKEN_GRANTING is set when a token is being granted 
	     * asynchronously.
	     * Tokens with this flag set cannot be revoked because the
	     * racing revokes mechanism on the cache manager won't work.
	     * The only tokens that are asynchronously granted are slice and
	     * dice tokens and queued tokens. Both are file tokens
	     */
	    osi_assert(fileP != NULL);

	    if (flags & TKM_PARA_REVOKE_FLAG_DOINGGC){
		(void) tkm_RemoveFromConflictQ(conflicts, tokenP);
		TKM_RELETOKEN(tokenP);
		TKM_UNLOCK_AFTER_TOKEN_RECYCLE(fileP, volP);
		continue; /* don't bother with this one */
	    }
	    lock_ReleaseWrite(&(volP->lock));
	    while (tokenP->flags & TKM_TOKEN_GRANTING){
		tkm_dprintf(("sleeping for %x on %x\n", tokenP, 
			     &(fileP->granted)));
		osi_SleepW(&(fileP->granted), &(fileP->lock));
		lock_ObtainWrite(&(fileP->lock));
	    }
	    lock_ReleaseWrite(&(fileP->lock));
	    /*
	     * We got the locks (released above) with
	     * TKM_PREPARE_FOR_TOKEN_RECYCLE.  That held the file and vol.
	     * Now do the reset of the bookkeeping normally handled
	     * in TKM_UNLOCK_AFTER_TOKEN_RECYCLE
	     */
	    tkm_ReleFile(fileP);
	    tkm_ReleVol(volP);
	} else {
	    TKM_UNLOCK_AFTER_TOKEN_RECYCLE(fileP, volP);
	}
    }
    return(code);
}


#ifdef SGIMIPS
static void tkm_ConflictToAfsRevokeDesc(tkm_tokenConflictElem_t  *conflict,
				        afsRevokeDesc	     *description)
#else
static tkm_ConflictToAfsRevokeDesc(tkm_tokenConflictElem_t  *conflict,
				   afsRevokeDesc	     *description)
#endif
{
    tkm_dprintf(("preparing to revoke %x offer %x %x\n", conflict->token,
	   conflict->grantLow, conflict->grantHigh));
    AFS_hzero(description->errorIDs);
    description->flags = 0;
    description->tokenID = conflict->token->id;
    tkm_CopyTokenFid(conflict->token, &(description->fid));
    AFS_hset64(description->type, 0, conflict->revoke);
    AFS_hzero(description->colAChoice);
    if (conflict->grantLow != NULL){
	description->flags |= AFS_REVOKE_COL_A_VALID;
	tkm_InternalToExternalToken(conflict->grantLow,
				    &(description->columnA));
    }
    AFS_hzero(description->colBChoice);
    if (conflict->grantHigh != NULL) {
	description->flags |= AFS_REVOKE_COL_B_VALID;
	tkm_InternalToExternalToken(conflict->grantHigh,
				    &(description->columnB));
    }
}

#ifdef SGIMIPS
static void tkm_AdjustAccepted(tkm_internalToken_t *internalP,
			  afs_token_t *offeredP,
			  long accepted)
#else
static tkm_AdjustAccepted(tkm_internalToken_t *internalP,
			  afs_token_t *offeredP,
			  long accepted)
#endif
{
    tkm_file_t *fileP = NULL;
    tkm_vol_t	*volP = NULL;

    osi_assert((internalP != NULL) &&
	       (internalP->flags & TKM_TOKEN_GRANTING) &&
	       (!(internalP->flags & TKM_TOKEN_ISVOL)) &&
	       (AFS_hsame(offeredP->tokenID, internalP->id)));

    if (accepted == 0) {
	/* nothing was accepted */
	TKM_PREPARE_FOR_TOKEN_RECYCLE(internalP, fileP, volP);
	TKM_RECYCLE_TOKEN(internalP);
	TKM_UNLOCK_AFTER_TOKEN_RECYCLE(fileP, volP);
    } else {
	fileP = internalP->holder.file;
	lock_ObtainWrite(&(fileP->lock));
	internalP->types = accepted;
	internalP->flags &= ~TKM_TOKEN_GRANTING;
	tkm_dprintf(("adjustAccepted : doing wakeup on %x\n",
		     &(fileP->granted)));
	osi_Wakeup(&(fileP->granted));
	lock_ReleaseWrite(&(fileP->lock));
    }
}


static int tkm_FindRevokeHost(struct hs_host *hostP,
			      tkm_hostRevokeList_t *listP)
{
    int i;

    for (i=0; i < listP->numElems; i++) {
	if (listP->list[i].hostP == hostP)
	    return (i);
    }
    return (-1);
}


static int tkm_AddRevokeHost(struct hs_host *hostP,
			     tkm_hostRevokeList_t *listP,
			     int maxConflicts,
			     afs_recordLock_t *lockDescriptorP,
			     osi_dlock_t *lockP)
{
    int i  = listP->numElems;
    tkm_hostRevokeElem_t *thisHost;

    if ( i == TKM_MAXPARALLELRPC)
	return (-1);
    listP->numElems++;
    thisHost =  &(listP->list[i]);
    tkm_dprintf(("Alloced host %d address %x\n", i, thisHost));
    thisHost->hostP = hostP;
    thisHost->revokeListP = (struct afsRevokeDesc *)
	osi_Alloc(maxConflicts * sizeof(struct afsRevokeDesc));
    bzero((caddr_t)thisHost->revokeListP,
	  maxConflicts * sizeof(struct afsRevokeDesc));
    thisHost->conflictIndex =
	(int *) osi_Alloc(maxConflicts * sizeof(int));
    bzero((caddr_t)thisHost->conflictIndex, maxConflicts * sizeof(int));
    thisHost->revokesAlloced = maxConflicts;
    thisHost->numbRevokes = 0;
    if (lockDescriptorP != NULL){
	thisHost->savedLockP = lockDescriptorP;
	thisHost->assignmentLockP = lockP;
    }
    else  {
	thisHost->savedLockP = NULL;
	thisHost->assignmentLockP = NULL;
    }
    return(i);
}

#ifdef SGIMIPS
void
#endif
tkm_ReInitHostList(tkm_hostRevokeList_t *listP)
{
    int i;

    for (i = 0; i < listP->numElems; i++) {
	listP->list[i].numbRevokes = 0;
	osi_Free((caddr_t) listP->list[i].revokeListP,
		 sizeof(struct afsRevokeDesc) * listP->list[i].revokesAlloced);
	osi_Free((caddr_t) listP->list[i].conflictIndex,
		 sizeof(int) * listP->list[i].revokesAlloced);
    }
    listP->numElems = 0;
}
