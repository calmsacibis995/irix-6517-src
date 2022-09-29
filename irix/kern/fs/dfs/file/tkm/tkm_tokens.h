/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * Copyright (C) 1996, 1990 Transarc Corporation
 * All rights reserved.
 */

/*
 * tokens.h -- Token Manager public interface
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_tokens.h,v 65.1 1997/10/20 19:18:08 jdoak Exp $
 *
 */

#ifndef TRANSARC_TKM_TOKENS_H
#define TRANSARC_TKM_TOKENS_H

#include <dcedfs/common_data.h>

/* things needed to interface with the host module */
#include <dcedfs/queue.h>
#include <dcedfs/hs.h>
#define HS_SUCCESS (0L)

#include <dcedfs/tkm_race.h>
#include <dcedfs/tkm_errs.h>

#define TKM_SUCCESS			(0L)


/* the flags passable to the flavors of GetToken & ReturnToken */
#define TKM_FLAGS_NORMALREQUEST		(0x0L)
#define TKM_FLAGS_QUEUEREQUEST		(0x1L)
#define TKM_FLAGS_OPTIMISTICGRANT	(0x4L)
#define TKM_FLAGS_DOING_ASYNCGRANT	(0x8L)
#define TKM_FLAGS_NOEXPIRE		(0x20L)
#define TKM_FLAGS_EXPECTNOCONFL		(0x80L)
#define TKM_FLAGS_GETSAMEID		(0x100L)
#define TKM_FLAGS_DONTSLEEP		(0x200L)

/* constant subject to change & only here to be used by the macro */
/* use the macro to test for permanence!!!!!!!!! */
#define TKM_TOKEN_NOEXPTIME	(~0L)
#define tkm_IsTimeForever(theTime)      \
  ( (theTime) == TKM_TOKEN_NOEXPTIME )

/*********** fun things to do with token types *****************/

/* all the different types of tokens that may be requested */
typedef enum tkm_tokenType	{
  TKM_NO_TOKEN = -1,
  TKM_LOCK_READ, TKM_LOCK_WRITE,
  TKM_DATA_READ, TKM_DATA_WRITE,
  TKM_OPEN_READ, TKM_OPEN_WRITE, TKM_OPEN_SHARED, TKM_OPEN_EXCLUSIVE,
  TKM_OPEN_DELETE, TKM_OPEN_PRESERVE,
  TKM_STATUS_READ, TKM_STATUS_WRITE,
  TKM_NUKE,
  /* the token types for moving filesets */
  TKM_SPOT_HERE,
  TKM_SPOT_THERE,
  /* 
   * If more members are added to the enumeration, TKM_ALL_TOKENS must 
   * remain last in the enumeration to avoid breaking things like arrays 
   * indexed by	instances of this type. 
   */
  TKM_ALL_TOKENS
} tkm_tokenType_t;

/* useful constants */
#define TKM_BITS_PER_CHAR		(8)
#define TKM_BITS_PER_LONG		(sizeof(long) * TKM_BITS_PER_CHAR)

/* exported operations on sets of token types */
#define TKM_TOKENMASK_BASE		(1L)
#define TKM_TOKENTYPE_MASK(tknType) (TKM_TOKENMASK_BASE << ((int)(tknType)))

/*
 * free tokens are token types that can always be  granted 
 *(for example READ_DATA for files in readonly volumes)
 */

#define	TKM_FREE_RO_TOKENS	(	\
	TKM_TOKENTYPE_MASK(TKM_LOCK_READ)	\
	| TKM_TOKENTYPE_MASK(TKM_DATA_READ)	\
	| TKM_TOKENTYPE_MASK(TKM_OPEN_READ)	\
	| TKM_TOKENTYPE_MASK(TKM_OPEN_SHARED)	\
	| TKM_TOKENTYPE_MASK(TKM_OPEN_PRESERVE)	\
	| TKM_TOKENTYPE_MASK(TKM_STATUS_READ)	\
	| TKM_TOKENTYPE_MASK(TKM_SPOT_HERE)	\
    )

#define TKM_FREETOKENS(volP) ((volP->flags & TKM_VOLRO) ?  \
			    TKM_FREE_RO_TOKENS: 0)
#define TKM_MAXTOKENS(volP) ((volP->flags & TKM_VOLRO) ? \
			   TKM_FREE_RO_TOKENS: (~0L))

#define TKM_FREE_TOKENID(tknID)	(AFS_hcmp64(tknID,-1,-1) == 0)

#define TKM_MAKE_FREETOKEN(tokenP) AFS_hset64(tokenP->tokenID,-1,-1)

#define TKM_AFSFID_WHOLE_VOL_VNODE		(~(1L))
#define TKM_AFSFID_WHOLE_VOL_UNIQUE		0L

extern afs_hyper_t tkm_localCellID;


/* interface calls exported to the protocol exporters */

/* meanings of return codes (except as otherwise noted with the call descr) */
/*
 *
 *	TKM_SUCCESS:		Call completed as requested.
 *	TKM_ERROR_BADHOST:	Illegal host specified.
 *	TKM_ERROR_FILEINVALID:	Incorrect file specification.
 *	TKM_ERROR_ILLEGALTKNTYPE: Token type given is not a legal token type.
 *	TKM_ERROR_INVALIDTKNID:	Invalid token ID specified.
 *	TKM_ERROR_LOCKED:	TKM entry is locked by another entity.
 *	TKM_ERROR_NOENTRY:	TKM does not know about specified file.
 *	TKM_ERROR_TIMEDOUT:	Attempt to fulfill request took longer
 *				than time limit specified.
 *	TKM_ERROR_TOKENCONFLICT: Token requested was in conflict with another
 *				 (non-revocable) token.
 *	TKM_ERROR_TOKENINVALID:	Invalid or inconsistent token description given.
 *	TKM_ERROR_REQUESTQUEUED: Token request was queued for later consideration.
 *	TKM_ERROR_TOKENLOST:	Token was deleted from token database during locking.
 *	TKM_ERROR_NOTOKENENTRY:	TKM does not know about specified token.
 *	TKM_ERROR_NOTINMAXTOKEN: Token permission set not within MaxToken for 
 *				 named volume.
 *
 */


/* the actual interface calls */
/*
 * initialize token manager data structures - must be called at least once, but
 * cannot be called in parallel with ANY other token manager operations
 */
extern long tkm_Init(void);

extern long tkm_GetToken(
    afsFid 	*fileDescP,
    long	flags,
    afs_token_t	*tokenp,
    struct hs_host *hostp,
    u_long reqNumber,
    afs_recordLock_t *lockDescriptorP
);
/* potential return codes (other than TKM_SUCCESS):	*/
/*	TKM_ERROR_BADHOST				*/
/*	TKM_ERROR_FILEINVALID				*/
/*	TKM_ERROR_REQUESTQUEUED				*/
/*	TKM_ERROR_TOKENCONFLICT				*/
/*	TKM_ERROR_TOKENINVALID				*/
/********************************************************/

/* return a token to the token manager */
extern long tkm_ReturnToken(
    afsFid	*fileDescP,
    afs_hyper_t *tokenIDP,
    afs_hyper_t *rightsP,
    long flags
);
/* potential return codes (other than TKM_SUCCESS):	*/
/*	TKM_ERROR_BADHOST				*/
/*	TKM_ERROR_FILEINVALID				*/
/*	TKM_ERROR_NOENTRY				*/
/*	TKM_ERROR_TOKENINVALID				*/
/********************************************************/

/*
 *  This routine returns the set of rights held by the named host over the
 * specified byte range on the specified fid.
 *
 *  NB:  THIS SHOULD ONLY BE USED AS AN ADVISORY FUNCTION.  THE RIGHTS SET
 * THAT IT RETURNS IS NOT GUARANTEED TO BE VALID, EVEN BY THE TIME THE CALLER
 * STARTS TO INTERPRET THE RETURNED SET.
 */
extern long tkm_GetRightsHeld(
    struct hs_host *hostP,
    afsFid *fidP,
    afs_hyper_t *startPosP,
    afs_hyper_t *endPosP,
    afs_hyper_t *rightsP
);

/*
 * the token manager syscall sub-opcodes
 */
#define TKMOP_SET_EXP			0
#define TKMOP_SET_MAX_TOKENS		1
#define TKMOP_SET_MIN_TOKENS		2
#define TKMOP_SET_MAX_FILES		3
#define TKMOP_SET_MAX_VOLS		4

/* 
 * OBSOLETE : the following is only in this file for other modules
 * to compile. None of them is used by TKM
 */

typedef afs_hyper_t tkm_tokenID_t;
typedef afs_hyper_t tkm_tokenSet_t; 

/* flags for GetToken and ReturnToken */
#define TKM_FLAGS_PENDINGRESPONSE	(0x2L)
#define TKM_FLAGS_FORCEVOLQUIESCE	(0x10L)
#define TKM_FLAGS_JUMPQUEUEDTOKENS	(0x40L)
#define TKM_FLAGS_NONEWEPOCH		(0x200L)

#define tkm_IsGrantFree(id) TKM_FREE_TOKENID(id)	

#define TKM_TOKENSET_CLEAR(setP) AFS_hzero(*(setP))
#define TKM_TOKENSET_ADD_TKNTYPE(setP, type) \
    AFS_HOP32(*(setP), |, TKM_TOKENTYPE_MASK(type))

#endif /* TRANSARC_TKM_TOKENS_H */
