/*
 *      Copyright (C) 1996, 1992 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_internal.h,v 65.3 1998/04/01 14:16:29 gwehrman Exp $ */

#ifndef TKM_INTERNAL_H
#define TKM_INTERNAL_H
#include <dcedfs/common_data.h>
#ifdef _KERNEL
#include <dce/ker/pthread.h>
#endif
#include <dcedfs/osi.h>
#include "tkm_tokens.h"
#include "tkm_range.h"

/*
 * The data structures used for tkm 
 */

/*
 * TKM's internal token representation.
 * All fields are protected by the holder's lock (or the freeTokenList's 
 * lock if the token is in the freeTokenList) except for GCnext and GCprev 
 * which are protected by gcList's lock
 */

typedef union tkm_holder {
    struct tkm_file	*file;
    struct tkm_volume  *volume;
} tkm_tokenHolder_t;

typedef struct tkm_internalToken {
    struct tkm_internalToken *	next;
    struct tkm_internalToken *	prev;
    afs_hyper_t		     	id;
    unsigned long		expiration;
    tkm_byterange_t	range;
    tkm_tokenHolder_t	holder;
    long	types; /* types granted */
    long	refused;  /* types revoked and refused */
    struct hs_host 	*host;
    unsigned int		flags; 
    unsigned long		refcount;
    unsigned long		reqNum;
    time_t			lastRefused;
    struct tkm_internalToken *	nextGC;
    struct tkm_internalToken *	prevGC;
} tkm_internalToken_t;

typedef tkm_internalToken_t * tkm_internalToken_p;

/* possible internal token flags */
#define TKM_TOKEN_ISVOL        1
#define TKM_TOKEN_GRANTING    2
#define TKM_TOKEN_DELETE      4
#define TKM_TOKEN_FOREVER     8

typedef struct tkm_tokenMask	{
    long		typeSum;
    unsigned long 	counter[TKM_ALL_TOKENS];
} tkm_tokenMask_t;

typedef struct tkm_tokenList {
    tkm_tokenMask_t *mask;
    tkm_internalToken_p   list;
} tkm_tokenList_t;

/* 
 * TKM's internal volume representation
 * All fields are protected by lock except for:
 *	files : protected by fileLock
 *	next, prev, refCount, gcNext protected by the global tkm_VolumelistLock
 */

typedef struct tkm_volume {
    struct tkm_volume	*next;
    struct tkm_volume	*prev;
    afs_hyper_t		cell; 
    afs_hyper_t		id;
    int 		flags; /* TKM_VOLRO | TKM_VOLRW */
    osi_dlock_t		lock;
    int 		tokenGrants;  /* since beginning of time */
    int 		refcount; /* sum of files + tokens + active requests */
    tkm_tokenList_t	granted; /* tokens granted (with mask)*/
    tkm_internalToken_p beingGranted; /* list of tokens in the process */
    tkm_internalToken_p	queuedFileTokens; /* 
					  * tokens for files belonging to
					  * this volume that were queued
					  * due to conflicts with the
					  * granted tokens of this volume
					  */
						
    tkm_tokenMask_t 	fileTokens; /* 
				    * Representing the sum of all types of
				    * granted tokens for files in 
				    * this volume. Given our locking hierarchy
				    * this is cheap to keep up to date 
				    */
    struct tkm_file	*files; /* list of files in this with tokens */
    osi_dlock_t		fileLock; /* lock protecting the file chain */
    struct tkm_volume	*gcNext; /* threading for recycling candidates */
}	tkm_vol_t;



#define TKM_VOLRW	0
#define TKM_VOLRO	1


/*
 * TKM's internal representation of a file  
 * All fields are protected by lock except:
 *    	next, prev protected by vol.fileLock
 *	refcount, hashNext, hashPrev gcNext by the global tkm_fileListLock
 */

typedef struct tkm_file {
    struct tkm_file	*hashNext; /* in the global file hash table */
    struct tkm_file	*hashPrev; /* in the global file hash table */
    afsFid		id;
    osi_dlock_t		lock;
    int 		tokenGrants;  /* since beginning of time */
    int 		refcount; /* sum of files + tokens + active requests */
    tkm_tokenList_t	granted; /* tokens granted (with mask) */
    tkm_internalToken_p	queued; /* tokens queued */
    struct hs_host	* host;  /* 
				  * NULL or only host that has been granted 
				  * tokens for this file
				  */
    tkm_vol_t		*vol; 
    struct tkm_file	*next; 	/* in the vol_t's file's list */
    struct tkm_file	*prev;	/* in the vol_t's files' list */
    struct tkm_file	*gcNext;   /* threading for recycling candidates */
} tkm_file_t;

/* 
 * Helpful macros to help with the difference between file 
 * and whole volume tokens
 */

#define TKM_HOLD_TOKEN_HOLDER(token) \
MACRO_BEGIN	\
    if ((token->flags & TKM_TOKEN_ISVOL)) \
       tkm_HoldVol(token->holder.volume);\
    else \
        tkm_HoldFile(token->holder.file);\
MACRO_END

#define TKM_RELE_TOKEN_HOLDER(token) \
MACRO_BEGIN	\
    if ((token->flags & TKM_TOKEN_ISVOL)) \
       tkm_ReleVol(token->holder.volume);\
    else \
        tkm_ReleFile(token->holder.file);\
MACRO_END

#define TKM_LOCK_TOKEN_HOLDER(token) \
MACRO_BEGIN	\
    if ((token->flags & TKM_TOKEN_ISVOL)) \
       lock_ObtainWrite(&(token->holder.volume->lock));\
    else \
       lock_ObtainWrite(&(token->holder.file->lock));\
MACRO_END

#define TKM_UNLOCK_TOKEN_HOLDER(token) \
MACRO_BEGIN	\
    if ((token->flags & TKM_TOKEN_ISVOL)) \
       lock_ReleaseWrite(&(token->holder.volume->lock));\
    else \
       lock_ReleaseWrite(&(token->holder.file->lock));\
MACRO_END

#define TKM_TOKEN_HOLDER_LOCKED(token) \
    (((token)->flags & TKM_TOKEN_ISVOL) ? \
     lock_WriteLocked(&((token)->holder.volume->lock)): \
     lock_WriteLocked(&((token)->holder.file->lock)))

#define TKM_REMOVE_FROM_MASK(token, types) \
MACRO_BEGIN	\
    if ((token->flags & TKM_TOKEN_ISVOL)) \
        tkm_RemoveFromTokenMask((token)->holder.volume->granted.mask, \
				(types));\
    else {\
        tkm_RemoveFromTokenMask((token)->holder.file->granted.mask, (types));\
	osi_assert(lock_WriteLocked(&(tokenP->holder.file->vol->lock))); \
        tkm_RemoveFromTokenMask(&((token)->holder.file->vol->fileTokens), \
				(types));\
    }\
MACRO_END


/* 
 * Before recycling a token we need to lock it's holder and in
 * the case of file tokens it's holder's vol as well. We also
 * need to store the holder (and holder's vol for file tokens)
 * so that we can unlock them after the recycling (since we cannot
 * look at the token anymore
 */

#define TKM_PREPARE_FOR_TOKEN_RECYCLE(tokenP, fileP, volP) \
MACRO_BEGIN	\
    if ((tokenP->flags & TKM_TOKEN_ISVOL)) {\
	volP = tokenP->holder.volume ;\
	fileP = NULL;\
    } else {\
	fileP = tokenP->holder.file;\
	tkm_HoldFile(fileP);\
	lock_ObtainWrite(&(fileP->lock));\
	volP = fileP->vol;\
    }\
    tkm_HoldVol(volP);\
    lock_ObtainWrite(&(volP->lock));\
MACRO_END

#define TKM_UNLOCK_AFTER_TOKEN_RECYCLE(fileP, volP) \
MACRO_BEGIN	\
    lock_ReleaseWrite(&(volP->lock));\
    if (fileP != NULL) {\
          lock_ReleaseWrite(&(fileP->lock));\
	  tkm_ReleFile(fileP);\
    }\
    tkm_ReleVol(volP);\
MACRO_END

/****************************************************************************
 *			 
 *			    LOCKING HIERARCHY
 *			 
 *************************************************************************** 
 *			  file_t.lock (get first)
 *			        |
 *			   vol_t.lock
 *				|
 *			 tkm_gcListLock
 *				|
 *		       tkm_freeTokensLock              
 *				|
 *			tkm_maxTokensLock
 *				|
 *		       tkm_tokenCounterLock       
 *				|
 *		       tkm_asyncGrantTryQLock 
 *				|
 *			tkm_fileListLock
 *				|
 *			tkm_volumeListLock    
 *				|
 *			 vol_t.fileLock (get last)
 *				.
 *				.
 *				.
 *			 hs_host->lock : We must also consider in our 
 *					 hierarchy  this lock from the hs_host
 *					 package since we do call HS_RELE, 
 *					 which gets that lock, while we are
 *					 holding a number of our locks in 
 *					 tkm_RemoveTypes()
 *****************************************************************************/


/* Routines exported by tkm_tokens.c for use only within TKM */

extern void tkm_InternalToExternalToken _TAKES((tkm_internalToken_t *internalP,
						afs_token_t * externalP));

#ifdef SGIMIPS
extern void tkm_CopyTokenFid _TAKES((tkm_internalToken_t *tokenP,
				     afsFid *fidP));
#endif

#define TKM_TIME_BETWEEN_WARNINGS (5*60) /* 5 minutes */
#define TKM_REFUSED_VALID_TIME (2*60) /* 2 minutes */

#if TKM_DPRINT
#if KERNEL
extern char tkm_debugBuffer[];
extern osi_dlock_t tkm_debugBufferLock;
extern int tkm_kernel_print;
#define tkm_dprintf(s)      if (tkm_kernel_print) tkm_kdprintf s
#else /*KERNEL*/
extern long tkm_debug;
#define tkm_dprintf(s) if (tkm_debug) printf s
#endif /* KERNEL */

#else /*TKM_DPRINT */
#define tkm_dprintf(s)
#endif /*TKM_DPRINT */

/* disable icl traces for the user level code (since we have tkm_dprintf) */
#if !KERNEL
#undef icl_Trace0
#undef icl_Trace1
#undef icl_Trace2
#undef icl_Trace3
#undef icl_Trace4
#define icl_Trace0(a,b) /* */
#define icl_Trace1(a,b,c1,d1) /* */
#define icl_Trace2(a,b,c1,d1,c2,d2) /* */
#define icl_Trace3(a,b,c1,d1,c2,d2,c3,d3) /* */
#define icl_Trace4(a,b,c1,d1,c2,d2,c3,d3,c4,d4) /* */
#endif /* !KERNEL */


#endif /*TKM_INTERNAL_H*/
