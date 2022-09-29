/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_async.c,v 65.4 1998/03/23 16:24:02 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright 1996, 1994 Transarc Corporation.  All rights reserved. */

#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/afs4int.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/tkm_errs.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_conn.h>
#include <cm_dcache.h>
#include <cm_cell.h>
#include <cm_async.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_async.c,v 65.4 1998/03/23 16:24:02 gwehrman Exp $")

/* 
 * This function guarantees that a token is still held when
 * it returns.
 * If it returns EINTR, it is because the process was aborted.
 * Called with a write-locked vnode, returns with the same.
 */
#ifdef SGIMIPS
long cm_GetAsyncToken(
     struct cm_scache *scp,	/* relevant vnode */
     afs_token_t *tokenp,		/* desired token (type, byte range) */
     struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
long cm_GetAsyncToken(scp, tokenp, rreqp)
     struct cm_scache *scp;	/* relevant vnode */
     afs_token_t *tokenp;		/* desired token (type, byte range) */
     struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    afs_recordLock_t afsBlocker;
    struct cm_tokenList *tlp;
    long code;

    icl_Trace4(cm_iclSetp, CM_TRACE_ASYNCSTART, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, AFS_hgetlo(tokenp->type),
	       ICL_TYPE_LONG, AFS_hgetlo(tokenp->beginRange),
	       ICL_TYPE_LONG, AFS_hgetlo(tokenp->endRange));

    for (;;) {
	code = cm_GetTokensRange(scp, tokenp, rreqp, CM_GETTOKEN_ASYNC, 
				 &afsBlocker, WRITE_LOCK);
	
	if (code != TKM_ERROR_REQUESTQUEUED) 
	    break;		/* either success or not in the queue */
	tlp = cm_FindTokenByID(scp, &tokenp->tokenID);
	/* if we can't find it now, it means that the token was queued,
	 * granted, revoked and GC'd all before we made it here.
	 */
	if (!tlp) break;
	
	cm_HoldToken(tlp);	/* so it doesn't go away if we block */
	
	/* 
	 * now, token may already be granted, or we may have to wait
	 * for the grant
	 */
	code = 0;	/* initialize it */
	/* wait for async to go off, or deleted to go on, representing
	 * either the granting of the token, or giving up on the
	 * async grant request.
	 */
	while ((tlp->flags & (CM_TOKENLIST_ASYNC | CM_TOKENLIST_DELETED))
	       == CM_TOKENLIST_ASYNC) {
	    tlp->flags |= CM_TOKENLIST_ASYNCWAIT;
	    code = osi_SleepWI((char *) &tlp->flags, &scp->llock);
	    lock_ObtainWrite(&scp->llock);
	    if (code) {
		code = EINTR;
		break;	/* got interrupted */
	    }
	}
	icl_Trace2(cm_iclSetp, CM_TRACE_ASYNCDONE,
		   ICL_TYPE_LONG, tlp->flags, ICL_TYPE_LONG, code);
	
	cm_ReleToken(tlp);
	/* go around again to handle race condition where revocation
	 * occurs before we relock things.  In the 2nd pass, typically
	 * cm_GetTokensRange will find the async-granted token and
	 * return success.
	 */
	if (code) break;	/* don't go around if ^C'd */
    }
    
    return code;
}
