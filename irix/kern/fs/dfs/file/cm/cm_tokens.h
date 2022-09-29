/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_tokens.h,v $
 * Revision 65.2  1998/03/19 23:47:25  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.1  1997/10/20  19:17:27  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.93.1  1996/10/02  17:13:08  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:06:03  damon]
 *
 * $EndLog$
*/
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_tokens.h,v 65.2 1998/03/19 23:47:25 bdr Exp $ */

#ifndef _CM_INCL_TOKENS_H
#define _CM_INCL_TOKENS_H 1

/*
 * There are four different TSR (Token State Recovery) cases in which the CM 
 * performs the token recovery work. They are TSR-Crash, TSR-Partition, 
 * TSR-Move, and TSR-CrashMove. The recovery policy is different in each case.
 */
#define TSR_CRASH       0x1     /* TSR from a server crash. */
#define TSR_PARTITION   0x2     /* TSR from a network partition. */
#define TSR_MOVE        0x4     /* TSR from a moving a fileset. */
#define TSR_CRASHMOVE   0x8     /* TSR from a cascad failures */
#define TSR_NOWAIT	0x100	/* Don't wait for TSR work to finish */
#define TSR_KNOWCRASH	0x200	/* Know that we were detached */

/* 
 * One structure per cache manager, representing the list of revokes that came 
 * in while calls that return tokens were running. When the count of calls that
 * may return tokens hits 0, we can purge the whole racingList.Note that things
 * are somewhat complicated by the fact that when a create is outstanding, we 
 * have no idea what file ID will be returned, so a racing revoke may refer to
 * a file ID  we've never seen before.
 */

struct cm_racing {
    int count;				/* # of calls that may return tokens */
    struct cm_racingItem *racingList;	/* list of racing revokes */
};

/* 
 * Increment ref count on token list element; vnode must be write-locked. 
 */
#define cm_HoldToken(atp) \
  \
    (atp)->refCount++
  \


/* 
 * one structure, hanging off of cm_racing structure, for each token revoke
 * that wasn't found in any scache entry.  These are perused after making
 * calls that return new tokens to see if we lost a race against a revoke.
 */
#define CM_RACINGITEM_REVOKE		1	/* revoke raced */
#define CM_RACINGITEM_ASYNC		2	/* async grant raced */
struct cm_racingItem {
    struct cm_racingItem *next;		/* next dude in list */
    struct cm_server *serverp;		/* server from which token revoked */
    afs_token_t token;			/* token if async grant, otherwise
					 * token.type is rights revoked
					 * and token.tokenID is ID of revoked
					 * token.
					 */
    int request;			/* type of request queued */
};

/* 
 * Lock for token race-condition detection 
 */
extern struct lock_data cm_racinglock;

/* all open tokens that the CM might have */
#define CM_OPEN_TOKENS	(TKN_OPEN_READ | TKN_OPEN_WRITE | TKN_OPEN_SHARED | \
			 TKN_OPEN_EXCLUSIVE | TKN_OPEN_DELETE | \
			 TKN_OPEN_PRESERVE)
#define CM_ALL_TOKENS 	( \
	TKN_LOCK_READ | TKN_LOCK_WRITE | TKN_DATA_READ | TKN_DATA_WRITE \
	| TKN_OPEN_READ | TKN_OPEN_WRITE | TKN_OPEN_SHARED \
	| TKN_OPEN_EXCLUSIVE | TKN_OPEN_DELETE | TKN_OPEN_PRESERVE \
	| TKN_STATUS_READ | TKN_STATUS_WRITE)


/*
 * Flags for cm_GetTokensRange() and cm_GetHereToken().
 */
#define CM_GETTOKEN_DEF		0x0	/* Default */
#define CM_GETTOKEN_EXACT	0x1	/* Get exactly matched token */
#define CM_GETTOKEN_ASYNC	0x2	/* Get an async-grant token */
#define CM_GETTOKEN_FORCE	0x4	/* Bypass existing check & make RPC */
#define CM_GETTOKEN_SEC		0x8	/* Use the secondary service */

/* 
 * Prototypes 
 */
extern void cm_QueueRacingRevoke _TAKES ((struct afsRevokeDesc *,
					  struct cm_server *));
extern void cm_QueueRacingGrant _TAKES ((afs_token_t *,
					 struct cm_server *));
extern void cm_EndTokenGrantingCall _TAKES ((afs_token_t *,
					    struct cm_server *,
					    struct cm_volume *,
					    unsigned long));
extern void cm_StartTokenGrantingCall _TAKES ((void));
extern int cm_GetOLock _TAKES((struct cm_scache *,
			       long, struct cm_rrequest *));
extern int cm_GetDLock _TAKES ((struct cm_scache *,
				struct cm_dcache *,
				int, int,
				struct cm_rrequest *));
extern int cm_GetSLock _TAKES ((struct cm_scache *,
			 long, long, int,
			 struct cm_rrequest *));
extern void cm_AddToken _TAKES ((struct cm_scache *,
				 afs_token_t *,
				 struct cm_server *,
				 int));

extern struct cm_tokenList * cm_FindTokenByID _TAKES ((struct cm_scache *,
						       afs_hyper_t *));

extern struct cm_tokenList * cm_FindMatchingToken _TAKES ((struct cm_scache *,
							   afs_token_t *));

extern void cm_ReturnOpenTokens _TAKES((struct cm_scache *, afs_hyper_t *));

extern long cm_GetTokensRange _TAKES((struct cm_scache *, afs_token_t *,
				      struct cm_rrequest *, int,
				      afs_recordLock_t *, int));

extern void cm_ReleToken _TAKES((struct cm_tokenList *));
extern void cm_DelToken _TAKES((struct cm_tokenList *));
#ifdef SGIMIPS 
extern void cm_EndPartialTokenGrant _TAKES(( register afs_token_t *,
				     struct cm_server *,
				     struct cm_volume *,
				     unsigned long));

extern void cm_TerminateTokenGrant _TAKES((void));
#endif /* SGIMIPS */
#endif /* _CM_INCL_TOKENS_H */
