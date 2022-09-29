/*
 *      Copyright (C) 1992 1993 1994 Transarc Corporation
 *      All rights reserved.
 */
/*
 * Routines to return and manipulate lists of conflicting tokens
 */

#ifndef TRANSARC_TKM_CONFLICTS_H
#define TRANSARC_TKM_CONFLICTS_H
#include "tkm_internal.h"

/*
 * struct tkm_tokenConflictQ :  These queues store lists of tokens that 
 *                              were found to be conflicting with the token 
 *                              that we are trying to grant.
 *                              These lists are meant to be local variables 
 *                              so there is no locking in the operations 
 *                              manipulating them. 
 */

typedef struct tkm_tokenConflictElem {
    long revoke;
    long grant;
    tkm_internalToken_t *token; /* if NULL the element has been removed */
    tkm_internalToken_t *grantLow; /* These last two are slice & dice tokens*/
    tkm_internalToken_t *grantHigh; /* which are filled by another module */
} tkm_tokenConflictElem_t;

typedef struct tkm_tokenConflictQ {
    int allocElements;
    int sliceNdiceNeeded;
    int lastPos; 
    osi_dlock_t	numElementsLock;
    long numElements; /* if 0 the queue is empty */
    tkm_tokenConflictElem_t *elements;
} tkm_tokenConflictQ_t;


/* 
 * the following is not MT safe because but it doesn't need to be 
 * since it is not used in tkm_DoRevoke which is the only case that
 * a conflictQ structure is shared by multiple threads
 */

#define TKM_CONFLICTQ_ISEMPTY(q) ((q)->numElements == 0)

extern void tkm_InitConflictQ _TAKES((tkm_tokenConflictQ_t **qPP, 
				      int num ));

extern void tkm_DeleteConflictQ _TAKES(( tkm_tokenConflictQ_t *q));

#ifdef SGIMIPS
extern void tkm_DeleteConflictQUnlocked(tkm_tokenConflictQ_t *q);
#endif

extern void tkm_AddToConflictQ _TAKES((tkm_tokenConflictQ_t *q,
				       tkm_internalToken_t * tokenP,
				       long revokeSet,
				       long sliceSet));

extern int tkm_RemoveFromConflictQ _TAKES((tkm_tokenConflictQ_t *q,
					   tkm_internalToken_t * tokenP ));

extern struct tkm_tokenConflictQ 
    *tkm_ConflictingTokens _TAKES((tkm_tokenList_t *issuedP,
				   tkm_internalToken_t *requestedP,
				   long *otherHostMaskP,
				   int maxnum));
#ifdef AFS_DEBUG
extern void tkm_TokenConflictQToString _TAKES((tkm_tokenConflictQ_t *q,
					       char *s));
#endif

#endif /*TRANSARC_TKM_CONFLICTS_H*/







