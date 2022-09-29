/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_bkg.h,v $
 * Revision 65.1  1997/10/20 19:17:22  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.75.1  1996/10/02  17:07:17  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:39  damon]
 *
 * $EndLog$
 */
/*
*/
/* Copyright (C) 1989, 1994 Transarc Corporation - All rights reserved */

#ifndef _CM_BKGH_
#define _CM_BKGH_

#include <dcedfs/osi_cred.h>

/*
 * Background module related macros
 */
#define	BK_NBRS		10	/* max number of queued daemon requests */
#define	BK_PARMS	4	/* Max parameters per request call */

/* 
 * Background operation opcodes.
 */
#define	BK_FETCH_OP	1	/* parm1 is chunk to get */
#define BK_STORE_OP	2	/* no parameters: store whole thing */
#define	BK_PATH_OP	3	/* parm1 is path, parm2 is chunk to fetch */
#define BK_RET_TOK_OP	4	/* no parameters; return TKN_OPEN_PRESERVE */

/* flags for bkgQueue */
#define BK_FLAG_NOREL	1	/* caller will do release later */
#define BK_FLAG_WAIT	2	/* wait for a bkg request structure */
#define BK_FLAG_ALLOC	4	/* alloc new bkg request if necessary */
#define BK_FLAG_NOWAITD	8	/* do not wait for a bkg daemon */

/* 
 * Background's state 
 */
#define	BK_STARTED	1	/* request picked up by a daemon */
#define BK_CODEVALID	2	/* return code is valid */
#define BK_CODEWAIT	4	/* waiting for return code to become valid */
#define BK_URGENT	8	/* start this request before others */

/* 
 * Protocol is: refCount is incremented by user to take block out of free pool.
 * BK_STARTED is set when daemon finds request. This prevents other daemons from
 * picking up the same request. When request is done, refCount is zeroed.
 * BK_CODEVALID is set when the procedure returns, and all the states
 * are cleared when the ref count hits zero.
 * BK_CODEWAIT is set if someone's waiting for the BK_CODEVALID flag to
 * come on.
 *
 * The only fields accessed concurrently are refCount and states.
 * All the others are owned serially by, first, the thread queueing
 * the request, second, the daemon, and lastly, the thread again.
 * Thus, you will typically not see locks obtained when the other
 * fields are changed.
 *
 * All fields are locked by the global cm_bkgLock.
 */

struct cm_bkgrequest {
    struct cm_scache *vnodep;   /* vnode to use, with vrefcount bumped */
    osi_cred_t *credp;		/* credentials to use for operation */
    long parm[BK_PARMS];	/* random parameters */
    long code;			/* return code */
    short refCount;		/* use counter for this structure */
    char opcode;		/* what to do (store, fetch, etc) */
    char states;		/* free, etc */
};

struct cm_bkgrequest_list {
    struct cm_bkgrequest r;
    struct cm_bkgrequest_list *next;
};

/* 
 * Exported by cm_bkg.c 
 */
extern struct lock_data cm_bkgLock;
extern short cm_bkgDaemons, cm_bkgWaiters, cm_bkgUrgent;
extern struct cm_bkgrequest *cm_bkgQueue _TAKES((int, struct cm_scache *,
					 long, osi_cred_t *,
					 long, long, long,
					 long, long));
extern void cm_bkgRelease _TAKES((struct cm_bkgrequest *));
extern void cm_bkgDaemon _TAKES((void));
extern long cm_bkgWait _TAKES((struct cm_bkgrequest *));

#endif /* _CM_BKGH_ */

