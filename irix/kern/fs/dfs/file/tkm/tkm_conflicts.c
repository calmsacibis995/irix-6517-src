/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_conflicts.c,v 65.4 1998/04/01 14:16:24 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 *      Copyright (C) 1996, 1992 Transarc Corporation
 *      All rights reserved.
 */
/*
 * This file includes tkm_Conflicting which returns a tkm_tokenConflictQ
 * structure that represents the list of tokens currently granted that
 * conflict with the token that we are trying to grant.  Additionally this
 * file contains all the routines necessary to manipulate tkm_tokenconflictQ
 * structures
 */

#include "tkm_internal.h"
#include "tkm_conflicts.h"
#include "tkm_recycle.h"
#include "tkm_ctable.h"
#ifdef SGIMIPS
#include "tkm_volume.h"
#include "tkm_file.h"
#endif

#ifdef KERNEL
#include <dcedfs/icl.h>
#include <dcedfs/tkm_trace.h>
extern struct icl_set *tkm_conflictQIclSet;
#endif /*KERNEL*/

#ifdef SGIMIPS
static int tkm_RevokeReqd(tkm_internalToken_p	issuedP,
			  tkm_internalToken_p	requestedP,
			  long *		revokeSetP,
			  long *		sliceSetP);
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_conflicts.c,v 65.4 1998/04/01 14:16:24 gwehrman Exp $")

#if TKM_DEBUG
/* it's really easy to get a leak of these conflict q structures */
/* so here is some code to help things out */

static osi_dlock_t tkm_cqtraceLock;

struct cqtrace {
    struct cqtrace *next;
    tkm_tokenConflictQ_t *q;
    caddr_t caller;
} *tkm_cqTrace = NULL;


#define ADDCQTRACE(q,c)	tkm_AddCQTrace(q,c)
#define DELCQTRACE(q)	tkm_DelCQTrace(q)

#ifdef SGIMIPS
static void tkm_AddCQTrace(tkm_tokenConflictQ_t *q, caddr_t caller)
#else
static tkm_AddCQTrace(tkm_tokenConflictQ_t *q, caddr_t caller)
#endif
{
    struct cqtrace *new;

    new = (struct cqtrace *) osi_Alloc (sizeof(struct cqtrace));
    new->q = q;
    new->caller = caller;
    lock_ObtainWrite(&tkm_cqtraceLock);
    new->next = tkm_cqTrace;
    tkm_cqTrace = new;
    lock_ReleaseWrite(&tkm_cqtraceLock);
}

#ifdef SGIMIPS
static void tkm_DelCQTrace(tkm_tokenConflictQ_t *q)
#else
static tkm_DelCQTrace(tkm_tokenConflictQ_t *q)
#endif
{
    struct cqtrace *old, *prev = NULL;

    lock_ObtainWrite(&tkm_cqtraceLock);
    old = tkm_cqTrace;
    while (old != NULL) {
	if (old->q == q) {
	    if (prev != NULL)
		prev->next = old->next;
	    else {
		osi_assert(tkm_cqTrace == old);
		tkm_cqTrace = old->next;
	    }
	    osi_Free(old, sizeof( struct cqtrace));
	    break;
	}
	prev = old;
	old = old->next;
    }
    lock_ReleaseWrite(&tkm_cqtraceLock);
}
#else /* TKM_DEBUG */
#define ADDCQTRACE(q,c)
#define DELCQTRACE(q)
#endif /*TKM_DEBUG*/

/*********************************************************************
 *
 * tkm_InitConflictQ(qPP, num) : initialize a tkm_tokenConflictq
 *
 * Input Params:		num = how many elements to allocate in the list
 * Output Params:	qPP = pointer to the tkm_tokenConflictq
 *			      to be initialized
 **********************************************************************/
void tkm_InitConflictQ(tkm_tokenConflictQ_t **qPP, int num)
{
    tkm_tokenConflictQ_t *newQ;
    tkm_tokenConflictElem_t *elems;

    newQ = (tkm_tokenConflictQ_t *)
	osi_Alloc(sizeof(tkm_tokenConflictQ_t));
    elems = (tkm_tokenConflictElem_t *)
	osi_Alloc (num * sizeof(tkm_tokenConflictElem_t));
    bzero((caddr_t) elems, num * sizeof(tkm_tokenConflictElem_t));
    lock_Init(&(newQ->numElementsLock));
    newQ->allocElements = num;
    newQ->lastPos = 0;
    newQ->elements = elems;
    newQ->sliceNdiceNeeded = 0;
    newQ->numElements = 0;
    *qPP = newQ;
    ADDCQTRACE(newQ, osi_caller());
}

/*********************************************************************
 *
 * tkm_FreeConflictQ(q) : initialize a tkm_tokenConflictq
 *
 * Input Params:	q = pointer to the tkm_tokenConflictq to be freed
 *
 **********************************************************************/
static void tkm_FreeConflictQ(tkm_tokenConflictQ_t *q)
{
    osi_Free((caddr_t) q->elements, (q->allocElements *
			      sizeof(tkm_tokenConflictElem_t)));
    osi_Free((caddr_t) q, sizeof(tkm_tokenConflictQ_t));
}

/*********************************************************************
 *
 * tkm_DeleteConflictQ(q) : delete and free a tkm_tokenConflictq
 *
 * Input Params:	q = pointer to the tkm_tokenConflictq to be deleted
 *
 **********************************************************************/

void tkm_DeleteConflictQ(tkm_tokenConflictQ_t *q)
{
    int i;
    if (!TKM_CONFLICTQ_ISEMPTY(q)) {
	icl_Trace1(tkm_conflictQIclSet,   TKM_TRACE_DELETE_CONFLICTQ,
		   ICL_TYPE_POINTER, q);
	for (i=0; i < q->lastPos; i++)
	    if (q->elements[i].token != NULL)
		TKM_RELETOKEN(q->elements[i].token);
    }
    DELCQTRACE(q);
    tkm_FreeConflictQ(q);
}

/*********************************************************************
 *
 * tkm_DeleteConflictQUnlocked(q) : delete and free a tkm_tokenConflictq
 *
 * Input Params:	q = pointer to the tkm_tokenConflictq to be deleted
 *
 * Locks : this routine will attempt to obtain the locks of the tokens'
 *	holders which are the top of the hierarchy, so no other locks
 *	can be held when calling it.
 *
 **********************************************************************/

void tkm_DeleteConflictQUnlocked(tkm_tokenConflictQ_t *q)
{
    int i;
    tkm_file_t*file;
    tkm_vol_t *volume;

    for (i=0; i < q->lastPos; i++){
	if (q->elements[i].token != NULL){
	    if (q->elements[i].token->flags & TKM_TOKEN_ISVOL) {
		volume = q->elements[i].token->holder.volume;
		tkm_HoldVol(volume);
		lock_ObtainWrite(&volume->lock);
		TKM_RELETOKEN(q->elements[i].token);
		q->elements[i].token = NULL;
		lock_ReleaseWrite(&volume->lock);
		tkm_ReleVol(volume);
	    } else {
		file = q->elements[i].token->holder.file;
		tkm_HoldFile(file);
		lock_ObtainWrite(&file->lock);
		TKM_RELETOKEN(q->elements[i].token);
		q->elements[i].token = NULL;
		lock_ReleaseWrite(&file->lock);
		tkm_ReleFile(file);
	    }
	}
    }
    DELCQTRACE(q);
    tkm_FreeConflictQ(q);
}

/*********************************************************************
 *
 * tkm_AddToConflictQ(q, tokenP, revokeSet, sliceSet) : add an element to
 *						       a tkm_tokenConflictq
 *
 *
 * Input Params:	q = pointer to the tkm_tokenConflictq
 *			tokenP = the token to add
 *			revokeSet = the types in tokenP that were conflicting
 *			sliceSet = the subset of types in revokeSet that
 *				   can have slice & dice tokens offered in
 *				   exchange
 *
 **********************************************************************/
void tkm_AddToConflictQ(tkm_tokenConflictQ_t *q,
			tkm_internalToken_t *tokenP,
			long revokeSet,
			long sliceSet)
{
    int pos;

    pos = q->lastPos++;
    osi_assert(pos <= q->allocElements);
    q->elements[pos].token = tokenP;
    TKM_HOLDTOKEN(tokenP);
    q->elements[pos].revoke = revokeSet;
    if (sliceSet){
	q->elements[pos].grant = sliceSet;
        q->sliceNdiceNeeded++;
    }
    lock_ObtainWrite(&(q->numElementsLock));
    q->numElements++;
    tkm_dprintf(("added %x to %x (pos %d) (numelem %d) \n", tokenP, q, pos,
	   q->numElements));
    icl_Trace4(tkm_conflictQIclSet, TKM_TRACE_ADD_CONFLICT_1,
	       ICL_TYPE_POINTER, q,
	       ICL_TYPE_HYPER, &(tokenP->id),
	       ICL_TYPE_POINTER, tokenP->host,
	       ICL_TYPE_LONG, revokeSet);
    icl_Trace4(tkm_conflictQIclSet, TKM_TRACE_ADD_CONFLICT_2,
	      ICL_TYPE_HYPER, &(tokenP->id),
	      ICL_TYPE_LONG, sliceSet,
	      ICL_TYPE_LONG, tokenP->refused,
	      ICL_TYPE_LONG, tokenP->lastRefused);
    
    lock_ReleaseWrite(&(q->numElementsLock));
}

/************************************************************************
 *
 * tkm_RemoveFromConflictQ(q, tokenP) : removes tokenP from Q
 *
 * Params:	q = pointer to the tkm_tokenConflictq
 *		tokenP = the token to remove
 *
 * Note: The caller is responsible for calling tkm_RELETOKEN(token)
 *	 Also this is the only routine that MUST use a lock to protect
 *	 any parts of the conflict q header that it alters (currently
 *	 just numElements) since it can be called for the same q (but
 *	 different tokens) at the same time from parallel revoke threads
 ************************************************************************/
int tkm_RemoveFromConflictQ(tkm_tokenConflictQ_t *q,
			     tkm_internalToken_p token)
{
    int i, retIx = -1;

    for (i=0; i < q->lastPos; i++){
	if (q->elements[i].token == token){
	    q->elements[i].token = NULL;
	    tkm_dprintf(("removed %x from %x (pos %d) \n", token, q, i));
	    lock_ObtainWrite(&(q->numElementsLock));
	    q->numElements--;
	    osi_assert(q->numElements >= 0);
	    lock_ReleaseWrite(&(q->numElementsLock));
	    retIx = i;
	    break;
	}
    }
    icl_Trace3(tkm_conflictQIclSet, TKM_TRACE_REMOVE_CONFLICT,
	       ICL_TYPE_HYPER, &(token->id),
	       ICL_TYPE_POINTER, q, 
	       ICL_TYPE_LONG, retIx);
    return(retIx);
}

/***********************************************************************
 *
 * tkm_Conflicting(issuedP, requestedP, otherHostMaskP, maxnum)
 *
 * Input Params:issuedP is a token list representation (tkm_tokenlist_t) of
 *	       the tokens already granted
 *	       requestedP is the token that we want to grant
 *	       maxnum is the maximum number of tokens to return.
 * Output Params: otherHostMaskP is a mask of token types not conflicting
 *	        with token but granted to hosts other than token->host
 * 	        This mask might include token types even if the only
 * 	        tokens granted for this type are granted to token->host
 *	        The above will happen if this routine can determine that
 *	        there are no conflicts without traversing the token list
 *	        (i.e. by simply looking at the token list's mask)
 *
 * LOCKING: called with holder locked.
 *
 * ALGORITHM DESCRIPTION:  First tries using the token mask. If the token
 *			   mask indicates that a there might be a conflict
 *			   the routine determines conflicts by traversing
 *			   the linked list of tokens in issuedP
 ***********************************************************************/

tkm_tokenConflictQ_t *tkm_ConflictingTokens(tkm_tokenList_t *issuedP,
					    tkm_internalToken_p requestedP,
					    long *otherHostMaskP,
					    int maxnum)
{

    long revokeSet;
    long sliceSet;
    tkm_internalToken_p	granted;
    tkm_tokenConflictQ_t *conflictingTokens;
    int confTokens = 0;

    *otherHostMaskP = 0;

    if ((TKM_TYPE_CONFLICTS(issuedP->mask->typeSum) & requestedP->types) == 0){
	*otherHostMaskP = issuedP->mask->typeSum;
	return(NULL);
    }
    tkm_InitConflictQ(&conflictingTokens, maxnum);
    for (granted = issuedP->list; granted != NULL; granted = granted->next){
	if (tkm_RevokeReqd(granted, requestedP, &revokeSet, &sliceSet)){
	    tkm_AddToConflictQ(conflictingTokens, granted,
			       revokeSet, sliceSet);
	    if (++confTokens == maxnum)
		break;
	} else {
	    if (granted->host != requestedP->host){
		*otherHostMaskP |= granted->types;
	    }
	}
    }
    if (TKM_CONFLICTQ_ISEMPTY(conflictingTokens)){
	tkm_DeleteConflictQ(conflictingTokens);
	return(NULL);
    } else
	return(conflictingTokens);
}


/***********************************************************************
 * tkm_RevokeReqd(issuedP, requestedP, revokeSetP, sliceSetP)
 *
 * Input Params : issuedP = the already granted token
 *		  requesestedP = the new token requested
 * Output Params: revokeSetP = the conflicting rigths
 *		  sliceSetP = the rights in issueddP for which slice
 *			      and dice tokens can be offered for
 * LOCKING: the holder of issuedP must be locked. If requestedP has a
 * 	    holder this holder must be locked as well
 *
 * GENERAL:
 *
 * Routine determines whether or not any types that are a part of issued
 * must be revoked in order to issue requested.
 *
 * Returns 0 if tokens do not conflict, and 1 if they do conflict.
 * Only sets revokeSetP and sliceSetP if there's a conflict.
 * revokeSetP points to the set of rights over which there's a conflict,
 * and sliceSet is the subset of those rights which are byte range rights.
 ***********************************************************************/

static int tkm_RevokeReqd(tkm_internalToken_p	issuedP,
			  tkm_internalToken_p	requestedP,
			  long *		revokeSetP,
			  long *		sliceSetP)
{

    int requestedTypes;	/* bits we're really using from requestedP */
    int conflicts;	/* tokens conflicting with requestedTypes */
    int overlap;		/* true iff two ranges overlap */
    int totalConflicts;	/* tokens which conflict between these two */

    if (revokeSetP != NULL)
	*revokeSetP = 0;
    if (sliceSetP != NULL)
	*sliceSetP = 0;

    /* if same host, tokens never conflict */
    if (issuedP->host == requestedP->host)
	return 0;

    /* we don't care about byte range tokens unless both of the tokens
     * have some byte range tokens and the ranges overlap.
     */
    if ((TKM_TYPES_WITH_RANGE(issuedP->types) == 0) ||
	(TKM_TYPES_WITH_RANGE(requestedP->types) == 0) ||
	(TKM_DISJOINT_BYTERANGES(&(issuedP->range), &(requestedP->range))))
	overlap = 0;
    else
	overlap = 1;


    /* now lookup all of the tokens that conflict with the requested
     * token.  First, we compute the requested token we're interested
     * in, since if we don't overlap, we don't care about potential
     * data or lock token overlaps.  Note that if we just pretend that
     * the requested token has no byte range tokens, we can ignore
     * any byte range tokens held by the issued token, since none of the
     * byte range tokens conflict with any non-byte range tokens.
     */

    if (overlap == 0)
	requestedTypes = TKM_TYPES_WITHOUT_RANGE(requestedP->types);
    else
	requestedTypes = requestedP->types;

    conflicts = TKM_TYPE_CONFLICTS(requestedTypes);

    /* now, conflicts contains those tokens that conflict with
     * requestedTypes.  Note that all of its undefined tokens are 0.
     * We have a conflict if any of the issued tokens match one of
     * these conflicting tokens.
   */
    if (totalConflicts = (issuedP->types & conflicts)) {
	if (revokeSetP) {
	    *revokeSetP= totalConflicts;
	}
	if (!(issuedP->flags & TKM_TOKEN_ISVOL) &&
	    !(requestedP->flags & TKM_TOKEN_ISVOL) &&
	    (sliceSetP != NULL)) {
	    /* 
	     * if the byte ranges don't overlap, conflicts, and thus
	     * totalConflicts, will have no byte range tokens, and thus this
	     * will just zero out sliceSetP.
	     */
	    *sliceSetP = TKM_TYPES_WITH_RANGE(totalConflicts);
	}
	return 1;
    }
    else
	return 0;
}

#ifdef TKM_DEBUG

int tkm_ExpRevokeReqd(tkm_internalToken_p	issuedP,
		      tkm_internalToken_p	requestedP,
		      long *		revokeSetP,
		      long *		sliceSetP)

{
    return (tkm_RevokeReqd(issuedP, requestedP, revokeSetP, sliceSetP));
}

void tkm_PrintTokenConflictQ(tkm_tokenConflictQ_t *q)
{
    int i;

    for (i=0; i < q->lastPos; i++){
	if (q->elements[i].token != NULL){
	    tkm_dprintf(("For token  %d  revoke %d grant %d\n",
			 AFS_hgetlo(q->elements[i].token->id),
			 q->elements[i].revoke,
			 q->elements[i].grant));
	}
    }
    tkm_dprintf(("conflictQ has %d alloc %d slice&dice needed\n",
	   q->allocElements, q->sliceNdiceNeeded));
}
#endif /*TKM_DEBUG*/
