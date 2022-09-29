/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: fshs_hostops.c,v 65.5 1998/04/01 14:15:55 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1996, 1990 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/debug.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>
#include <dcedfs/queue.h>
#include <dcedfs/tkn4int.h>
#include <dcedfs/bomb.h>
#ifdef SGIMIPS
#include <dcedfs/tkset.h>
#endif
#include <fshs.h>
#include <fshs_trace.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/fshost/RCS/fshs_hostops.c,v 65.5 1998/04/01 14:15:55 gwehrman Exp $")

long fshs_HoldHost(), fshs_ReleHost(), fshs_LockHost(), fshs_UnlockHost();
long fshs_RevokeToken(), fshs_AsyncGrant();


/*     
 * Main instance of host operations 
 */
struct hostops fshs_ops = {
    fshs_HoldHost,
    fshs_ReleHost,
    fshs_LockHost,
    fshs_UnlockHost,
    fshs_RevokeToken,
    fshs_AsyncGrant,
};


/*
 * Bump the host's ref counter; it guarantees  the host entry won't disappear 
 * NB: At this point, the refCount is already bumped by the manager function.
 */
long fshs_HoldHost(hostp)
    register struct fshs_host *hostp;
{
    /* don't have to worry about stale flag here, since stale is only
     * set if refcount is 0, and caller shouldn't have a pointer to
     * this structure if it has a 0 refcount.
     */
    lock_ObtainWrite(&hostp->h.lock);
    hostp->h.refCount++;
    lock_ReleaseWrite(&hostp->h.lock);
#ifdef SGIMIPS
    return 0;
#endif
}


/* 
 * Decrement the host reference counter 
 */
long fshs_ReleHost(hostp)
    register struct fshs_host *hostp;
{
    fshs_PutHost(hostp);
#ifdef SGIMIPS
    return 0;
#endif
}


/* 
 * Locks the host structure; READL or WRITEL are the only permissable locking 
 * types. Timeout bounds the time this call should wait if not immediately 
 * satisfiable.
 */
long fshs_LockHost(hostp, type, timeout)
    register struct fshs_host *hostp;
    int type;
    long timeout;      /* Not implemented yet */
{
    if (type == HS_LOCK_READ) 
	lock_ObtainRead(&hostp->h.lock);
    else if (type == HS_LOCK_WRITE)
	lock_ObtainWrite(&hostp->h.lock);
#ifdef SGIMIPS
    return 0;
#endif
}


/* 	
 * Unlock the host structure: Unlock an already locked entry; type determines 
 * the  type of locking that was used (READL or WRITEL) 
 */
long fshs_UnlockHost(hostp, type)
    register struct fshs_host *hostp;
    int type;
{
    if (type == HS_LOCK_READ) 
	lock_ReleaseRead(&hostp->h.lock);
    else if (type == HS_LOCK_WRITE)
	lock_ReleaseWrite(&hostp->h.lock);
#ifdef SGIMIPS
    return 0;
#endif
}


/* 
 * This routine revokes token(s) from the host represented by hostp 
 */
long fshs_RevokeToken(hostp, revokeLen, revokeListp)
    register struct fshs_host *hostp;
    register revokeLen;
    afsRevokeDesc *revokeListp;	/* really an array */
{
    register long i, tknLeft, numRevoke, code = 0;
    long rcode = 0;	/* this proc's final return code */
    afsRevokeDesc *revdp = revokeListp, *tmprevdp;
    afsRevokes *trevp;
    error_status_t st;
    unsigned long now;		/* current time */
    int lifeExceeded = 0;	/* hostLifeGuarantee exceeded? */
    int RPCExceeded = 0;	/* hostRPCGuarantee exceeded? */
    afs_hyper_t volId;
    int hereCount = 0;		/* count of HERE tokens in volid not returned */

    icl_Trace3(fshs_iclSetp, FSHS_REVOKETOKENX,
		ICL_TYPE_POINTER, hostp,
		ICL_TYPE_LONG, hostp->flags,
		ICL_TYPE_LONG, revokeLen);

    now = osi_Time();
    /*
     * TSR: If the host is already marked down, don't try to make
     * the RPC.  Instead, return success and token is considered revoked.
     */
    lock_ObtainWrite(&hostp->h.lock);
    if (FSHS_ISHOSTDOWN(hostp)) {
	lock_ReleaseWrite(&hostp->h.lock);
	icl_Trace1(fshs_iclSetp, FSHS_REVOKETOKENDOWN,
		ICL_TYPE_POINTER, hostp);
	return 0; /* success  */
    }
    /*
     * Since the host is not already marked DOWN, check other expirations:
     * If RPCExceeded, mark host down and return success. NOTE: This is 
     * the only place (code path) that we will mark the host DOWN.
     */
    lifeExceeded = (now > hostp->lastCall + hostp->hostLifeGuarantee);
    RPCExceeded =  (now > hostp->lastCall + hostp->hostRPCGuarantee);
    if (RPCExceeded) {
	hostp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
	lock_ReleaseWrite(&hostp->h.lock);
	icl_Trace2(fshs_iclSetp, FSHS_REVOKETOKENRPCEXCEEDED,
		   ICL_TYPE_LONG, hostp->hostRPCGuarantee,
		   ICL_TYPE_POINTER, hostp);
	return 0;
    }
    lock_ReleaseWrite(&hostp->h.lock);
    /* 
     * Iterate over all tokens, offering it first to the token set manager
     * and then to the token manager. Note, Each RPC call can not revoke more 
     * than AFS_BULKMAX - 1 tokens
     */
    trevp = (afsRevokes *)osi_AllocBufferSpace();
    tknLeft = revokeLen;

    AFS_hzero(volId);
    while (tknLeft > 0)  {
	numRevoke = (tknLeft >= AFS_BULKMAX ? AFS_BULKMAX-1 : tknLeft);
	tmprevdp = &trevp->afsRevokes_val[0];
	bcopy((char *) revdp, (char *) tmprevdp,
	      numRevoke * sizeof (afsRevokeDesc));

        for (i = 0; i < numRevoke; i++, tmprevdp++) {
	    if (code = tkset_TokenIsRevoked(revdp+i)) {
	      /* 
	       * Failed to wait for token from token collector, give up.
	       * This failure represents an internal programming error,
	       * or something like that, and really shouldn't occur.
	       */
	       rcode = HS_ERR_NOENTRY;
	       goto bad;
	    }

	    /* convert token granting structure to relative times to avoid
	     * clock skew problems.
	     *
	     * Avoid passing token byte ranges to a client that can't deal with
	     * them.  So map Maxint64 to Maxint32 and invalidate tokens whose
	     * range is entirely beyond Maxint32. */

	    if (tmprevdp->flags & AFS_REVOKE_COL_A_VALID) {
		tmprevdp->columnA.expirationTime -= osi_Time();
		if (!hostp->supports64bit) {
		    if (AFS_hcmp(tmprevdp->columnA.endRange,
				 osi_hMaxint32) > 0)
			tmprevdp->columnA.endRange = osi_hMaxint32;
		    if (AFS_hcmp(tmprevdp->columnA.beginRange,
				 tmprevdp->columnA.endRange) > 0) {
			tmprevdp->flags &= ~AFS_REVOKE_COL_A_VALID;
			osi_Memset(&tmprevdp->columnA, 0, sizeof(afs_token_t));
			AFS_hzero(tmprevdp->colAChoice);
		    }
		}
	    }
	    if (tmprevdp->flags & AFS_REVOKE_COL_B_VALID) {
		tmprevdp->columnB.expirationTime -= osi_Time();
		if (!hostp->supports64bit) {
		    if (AFS_hcmp(tmprevdp->columnB.endRange,
				 osi_hMaxint32) > 0)
			tmprevdp->columnB.endRange = osi_hMaxint32;
		    if (AFS_hcmp(tmprevdp->columnB.beginRange,
				 tmprevdp->columnB.endRange) > 0) {
			tmprevdp->flags &= ~AFS_REVOKE_COL_B_VALID;
			osi_Memset(&tmprevdp->columnB, 0, sizeof(afs_token_t));
			AFS_hzero(tmprevdp->colBChoice);
		    }
		}
	    }
	}
	/* 
	 * Try getting tokens back from AFS client by calling  revoke proc
	 */
	trevp->afsRevokes_len = numRevoke;

	fshs_StartCall(hostp);
	osi_RestorePreemption(0);
	st = TKN_TokenRevoke(hostp->cbBinding, trevp);
	osi_PreemptionOff();
	code = (long) st;
	lock_ObtainWrite(&hostp->h.lock);
	fshs_EndCall(hostp);
	BOMB_IF("FSHS_TSRPART") {
	    /* simulate partition better here at the server side, by
	     * making it looked like we failed to revoke a token, and
	     * so just took the token away from the client (which we
	     * do if lifeExceeded is true).
	     */
	    code = rpc_s_comm_failure;
	    lifeExceeded = 1;
	}
	icl_Trace1(fshs_iclSetp, FSHS_TOKENREVOKE, ICL_TYPE_LONG, st);
	/* count the number of successive rpc&sec errors. */
	if ((code >= rpc_s_mod && code < rpc_s_mod+4096)
	    || (code >= sec_s_authz_unsupp && code < sec_s_authz_unsupp+4096))
	    hostp->rvkErrorCount++;
	else
	    hostp->rvkErrorCount = 0;
	if (code != 0 && code != HS_ERR_PARTIAL) {
	    /* TSR: The RPC has failed.  If the host lifetime has
	     * been exceeded, mark the host as down and return success.
	     * Ditto if the client runtime can't take our calls and we don't
	     * yet know how to retry.
	     * Ditto again if there have been 5 or more successive RPC/SEC
	     * errors from our revocation attempts and the last successful
	     * revocation was more than 3 times the RPC lifetime.
	     * (** this should be revisited to deal with revocation refusals,
	     *  here and/or below after looking at errorIDs.)
	     * Otherwise, return HS_ERR_TRYAGAIN.
	     */
	    if (lifeExceeded || (code == rpc_s_wrong_boot_time)
		|| (hostp->rvkErrorCount > 5
		    && (now - hostp->lastRevoke) > 3*(hostp->hostRPCGuarantee))) {
		hostp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
		icl_Trace4(fshs_iclSetp, FSHS_REVOKETOKENLIFEEXCEEDED_2,
			   ICL_TYPE_POINTER, hostp,
			   ICL_TYPE_LONG, hostp->hostLifeGuarantee,
			   ICL_TYPE_LONG, hostp->rvkErrorCount,
			   ICL_TYPE_LONG, hostp->lastRevoke);
		rcode = 0;	/* success */
	    }
	    else
		rcode = HS_ERR_TRYAGAIN;
	    lock_ReleaseWrite(&hostp->h.lock);
	    goto bad;
	}
	/* RPC result is just fine. */
	/* Extend the host life time, but don't inadvertently put it back
	 * in time
	 */
	if (hostp->lastCall < now)
	    hostp->lastCall = now;
	if (hostp->lastRevoke < now)
	    hostp->lastRevoke = now;
	lock_ReleaseWrite(&hostp->h.lock);

	/* 
	 * Finally, if we didn't get all of the tokens back, we return
	 * HS_ERR_PARTIAL instead of 0, or else TKM will loop forever.
	 */
	for (i = 0; i < numRevoke; i++) {
	   if (rcode == 0 && !AFS_hiszero(trevp->afsRevokes_val[i].errorIDs))
	     rcode = HS_ERR_PARTIAL;
#ifdef SGIMIPS
/* Turn off "pointless comparison of unsigned integer with zero" error. */
#pragma set woff 1183
#endif
	   /* Count the HERE tokens for the fileset that didn't come back */
	   if (AFS_hcmp64(trevp->afsRevokes_val[i].errorIDs,
			  0, TKN_SPOT_HERE) == 0) {
	       if (AFS_hiszero(volId))
		   volId = trevp->afsRevokes_val[i].fid.Volume;
	       if (AFS_hsame(volId, trevp->afsRevokes_val[i].fid.Volume))
		   ++hereCount;
	   }
#ifdef SGIMIPS
#pragma reset woff 1183
#endif
	   /* copy fields returned as out parameters to our caller */
	   revdp[i].errorIDs = trevp->afsRevokes_val[i].errorIDs;
	   revdp[i].colAChoice = trevp->afsRevokes_val[i].colAChoice;
	   revdp[i].colBChoice = trevp->afsRevokes_val[i].colBChoice;
	   if (trevp->afsRevokes_val[i].outFlags != 0) {
	       revdp[i].outFlags = trevp->afsRevokes_val[i].outFlags;
	       revdp[i].recordLock = trevp->afsRevokes_val[i].recordLock;
	       /*
                * 32/64-bit interoperability changes:
		*
                * Check to see if the client we're talking to may not know
                * about the 64-bit extenstions to the afs_recordLock_t type.
                * If it doesn't, we need to clear the upper 32 bits of the
                * start and end positions in the record lock struct, so that
                * 64-bit range checking works properly (there may be garbage in
                * the upper 32 bits since the client doesn't set it).
                */
               if (!hostp->supports64bit) {
		   AFS_hzerohi(revdp[i].recordLock.l_start_pos);
		   AFS_hzerohi(revdp[i].recordLock.l_end_pos);
               }
	   }
	}
	tknLeft -= numRevoke;
	revdp += numRevoke;
    } /* while */
    /* Now, if we know that 50 or more HERE tokens for the same fileset are
     * held by the same client, something is seriously wrong.  Mark it DOWN
     * so that those rogue tokens can be reclaimed.
     */
    if (hereCount >= 50) {
	hostp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
	icl_Trace3(fshs_iclSetp, FSHS_REVOKETOKENTOOMANYHERE,
		   ICL_TYPE_POINTER, hostp,
		   ICL_TYPE_LONG, hereCount,
		   ICL_TYPE_HYPER, &volId);
	osi_printIPaddr("fx: client ", hostp->clientAddr.sin_addr.s_addr,
			" holds on to 50 HERE tokens: marking DOWN.\n");
    }
bad:
    osi_FreeBufferSpace((struct osi_buffer *)trevp);
    return rcode;
}

/*
 * Grant tokens asynchronously.
 */
long fshs_AsyncGrant(hostp, tkndp, reqnum)
    register struct fshs_host *hostp;
    struct hs_tokenDescription *tkndp;
    long reqnum;
{
    register long code;
    unsigned32 now;

    icl_Trace1(fshs_iclSetp, FSHS_ASYNCGRANT, ICL_TYPE_POINTER, hostp);
    if (reqnum == 0) {
        /*
	 * this means we've got an async grant for the cache manager.
	 * reqnum of 0 means that tkset didn't generate the request.
	 */
	fshs_StartCall(hostp);
	osi_RestorePreemption(0);
	/* convert expiration time from absolute to relative.  We don't
	 * ask for tokens that don't expire, so it's safe to just
	 * subtract the time now.
	 */
	tkndp->token.expirationTime -= osi_Time();
	code = TKN_AsyncGrant(hostp->cbBinding, &tkndp->fileDesc,
			      &tkndp->token, reqnum);
	osi_PreemptionOff();
	/* Need to obtain this lock for fshs_EndCall */
	lock_ObtainWrite(&hostp->h.lock);
	fshs_EndCall(hostp);
	/* 
	 * IF failed due to a network problem, mark the host DOWN. So that the
	 * client will renew this token during the TSR procedure.
	 * Do it only for unexpected errors, though.
	 * Not in old mode--just mark it as NEEDINIT, to get to TSR some
	 * other way, while still allowing revocations to work.
	 */
	if (code && code != HS_ERR_JUSTKIDDING) {
	    if (hostp->flags & FSHS_HOST_NEW_IF) {
		hostp->h.states |= (HS_HOST_NEEDINIT | HS_HOST_HOSTDOWN);
	    } else {
		hostp->h.states |= HS_HOST_NEEDINIT;
	    }
	} else {
	    /* return was OK */
	    now = osi_Time(); 
	    if (hostp->lastRevoke < now)
		hostp->lastRevoke = now;
	}
	lock_ReleaseWrite(&hostp->h.lock);
	return 0;
    }

    if (!(code = tkset_HereIsToken(&tkndp->fileDesc, &tkndp->token, reqnum)))
	return 0;	/* ate the token */

    return 0;
}

int tokenint_InitTokenState(hostp, hdl)
    register struct fshs_host *hostp;
    handle_t hdl;
{
    int errorCode;
#ifdef SGIMIPS
    unsigned32 dummy = 0;
#else
    unsigned long dummy = 0;
#endif /* SGIMIPS */
    unsigned long Flags;

    icl_Trace1(fshs_iclSetp, FSHS_INITTOKENSTATE, ICL_TYPE_POINTER, hostp);

    /* Obtain the flags useful for the client's TSR */
    lock_ObtainWrite(&hostp->h.lock);
    Flags = fshs_GetTSRCode(hostp);
    lock_ReleaseWrite(&hostp->h.lock);

    osi_RestorePreemption(0);
    errorCode = TKN_InitTokenState(hdl,
				   Flags,
				   hostp->hostLifeGuarantee,
				   hostp->hostRPCGuarantee,
				   hostp->deadServerTimeout,
				   fshs_tsrParams.serverRestart,
				   OSI_MAX_FILE_PARM_SERVER,
				   /* spare ins*/0,0,
				   /* outs */&dummy, &dummy, &dummy);
    osi_PreemptionOff();
    return errorCode;
}
