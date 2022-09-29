/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_tknimp.c,v 65.21 1999/12/06 19:24:58 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/tkn4int.h>
#include <dcedfs/tpq.h>                 /* thread pool package */
#include <dcedfs/hs.h>
#include <dce/rpc.h>
#include <dce/uuid.h>             
#include <cm.h>				/* Cm-based standard headers */
#include <cm_dcache.h>
#include <cm_bkg.h>
#include <cm_cell.h>
#include <cm_volume.h>
#include <cm_conn.h>
#include <cm_async.h>
#include <cm_stats.h>
#include <cm_aclent.h>
#include <cm_server.h>
#include <cm_dnamehash.h>
#include "cm_server.h"

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_tknimp.c,v 65.21 1999/12/06 19:24:58 gwehrman Exp $")

static int RevokeHereToken _TAKES((afsFid *, int, struct cm_server *, afs_hyper_t *));
static void cm_QueuedRecoverTokenState _TAKES((void *, void *));

#ifdef SGIMIPS
extern void rpc__binding_inq_sockaddr
(
    rpc_binding_handle_t    binding_h,
    sockaddr_p_t            *sa,
    unsigned32              *status
);
#endif /* SGIMIPS */


/*
 * some debugging aids
 */
static struct ltable {
    char *name;
    struct lock_data *addr;
} ltable []= {
    "cm_scachelock",	&cm_scachelock,
    "cm_dcachelock", 	&cm_dcachelock,
    "cm_serverlock", 	&cm_serverlock,
    "cm_qtknlock", 	&cm_qtknlock,
    "cm_bkgLock", 	&cm_bkgLock,
    "cm_celllock", 	&cm_celllock,
    "cm_connlock", 	&cm_connlock,
    "cm_volumelock", 	&cm_volumelock,
    "cm_ftxtlock", 	&cm_ftxtlock,
};

/*
 * Probe client Token interface call: If we make it to here
 * we're successful enough.
 */
#ifdef SGIMIPS
static unsigned32 STKN_Probe(handle_t h)
#else  /* SGIMIPS */
static unsigned32 STKN_Probe(h)
     handle_t h;
#endif /* SGIMIPS */
{
    int preempting;
    long code;

    preempting = osi_PreemptionOff();
    if (code = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
#ifdef SGIMIPS
	return (unsigned32)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }
    icl_Trace0(cm_iclSetp, CM_TRACE_TKNPROBE);
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);
    return error_status_ok;		/* this one's correct */
}

/* SetServerSize -- fills in info about the server's maximum file size and
 *     support for 64-bit offsets based on encoded parameter.
 *
 * LOCKS USED -- assumes the serverp is write locked. */

static void SetServerSize(struct cm_server *serverp, unsigned32 value)
{
    serverp->maxFileParm = value;       /* save this for SetParams */
    serverp->maxFileSize = osi_MaxFileParmToHyper(value);
    /*
     * If the remote host didn't set a value or if it is bogus (code may not
     * implemented on server yet), set maxFileSize to the default.  Also check
     * for anything but 2^31-2^0, which was the value originally used, or the
     * new flag bit.  This detects when the remote system needs the backward
     * compatibility features for 32bit servers.
     */
    if (!AFS_hiszero(serverp->maxFileSize) &&
	((value & AFS_CONN_PARAM_SUPPORTS_64BITS) ||
	 (AFS_hcmp(serverp->maxFileSize, osi_hMaxint32) != 0))) {
	serverp->supports64bit = 1;
	icl_Trace2(cm_iclSetp, CM_TRACE_SETSERVERSIZE,
		   ICL_TYPE_LONG, serverp,
		   ICL_TYPE_HYPER, &serverp->maxFileSize);
    } else {
	serverp->maxFileSize = osi_maxFileSizeDefault;
	icl_Trace2(cm_iclSetp, CM_TRACE_SETSERVERSIZE_OLD,
		   ICL_TYPE_LONG, serverp,
		   ICL_TYPE_HYPER, &serverp->maxFileSize);
    }
}

/*
 * Initialize all the token state associated with the server.
 * This is a server's "callback" call initiated by AFS_SetContext.
 */
static error_status_t STKN_InitTokenState(
  handle_t h,
  unsigned32 Flags,
  unsigned32 hostLifeGuarantee,
  unsigned32 hostRPCGuarantee,
  unsigned32 deadServerTimeout,
  unsigned32 serverRestartEpoch,
  unsigned32 serverSizesAttrs,
  unsigned32 spare2,
  unsigned32 spare3,
  unsigned32 *spare4,
  unsigned32 *spare5,
  unsigned32 *spare6)
{
    int preempting;
    afsUUID serverID;
    struct cm_server *serverp;
    long sameEpoch = 0;
    unsigned32 st = 0;
    struct cm_params *threadParamp;

    preempting = osi_PreemptionOff();
    if (st = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
	return st;
    }
    icl_Trace0(cm_iclSetp, CM_TRACE_TKNINITSTATE);
    /*
     * First check to see which FX server this request is coming from.
     * We check it by using server's UUID rather than its IP addr.
     */
    rpc_binding_inq_object(h, &serverID, &st);
    if (st == rpc_s_ok) {
        if (uuid_is_nil(&serverID, &st)) {
	    cm_printf("dfs: STKN_InitTokenState passed nil server uuid \n");
	    st = 0; /* should not return error to the server */
	    goto out;
	}
    } else {
	cm_printf("dfs: could not read uuid from rpc binding (code = %d)\n",
		  st);
	st = 0;
	goto out;
    }
    serverp = cm_FindServer(&serverID);
    if (!serverp) {
	struct sockaddr_in *sockPtr;
#ifdef SGIMIPS
	rpc__binding_inq_sockaddr(h, (sockaddr_p_t *)&sockPtr, &st);
#else  /* SGIMIPS */
	rpc__binding_inq_sockaddr(h, &sockPtr, &st);
#endif /* SGIMIPS */
	icl_Trace2(cm_iclSetp, CM_TRACE_BADTKNINITID,
		   ICL_TYPE_LONG, sockPtr->sin_addr.s_addr,
		   ICL_TYPE_UUID, &serverID);
	cm_printf("dfs: can't find server 'addr %x' based on its server ID\n",
		sockPtr->sin_addr.s_addr);
	st = 0;
	goto out;
    }

#ifdef SGIMIPS
    /*
     * If there are still timeout paramters less than CYCLE_TIME
     * (defined in cm_daemons.c), warn the user so he has a clue when
     * he gets other error messages.
     */
    if (hostLifeGuarantee < 15) {
	char msgBuf[20];
	sprintf(msgBuf, " host lifetime = %d\n", hostLifeGuarantee);
	cm_PrintServerText("dfs: server ", serverp, msgBuf);
    }
    if (hostRPCGuarantee < 15) {
	char msgBuf[20];
	sprintf(msgBuf, " RPC lifetime = %d\n", hostRPCGuarantee);
	cm_PrintServerText("dfs: server ", serverp, msgBuf);
    }
    if (deadServerTimeout < 15) {
	char msgBuf[20];
	sprintf(msgBuf, " poll interval = %d\n", deadServerTimeout);
	cm_PrintServerText("dfs: server ", serverp, msgBuf);
    }
#endif /* SGIMIPS */
    lock_ObtainWrite(&serverp->lock);
    /*
     * NOTE: we should check that  hostRPCGuarantee or deadServerTimeout
     *       should not be less than 15 secs, which is the "cycleTime" used in
     *       cm_daemons.c. If any of them is less than 15 seconds, we may
     *       want to use AFS_SetParams() to re-negotiate these paramaters
     *       IN THE FUTURE.
     */

    serverp->hostLifeTime = (hostLifeGuarantee > 30 ? hostLifeGuarantee - 30 :
			     hostLifeGuarantee) ;     /* Allow 30 more sec */
    serverp->origHostLifeTime = hostLifeGuarantee;
    serverp->deadServerTime = deadServerTimeout;
    SetServerSize(serverp, serverSizesAttrs);

    icl_Trace1(cm_iclSetp, CM_TRACE_TKNINITSERVER, ICL_TYPE_POINTER, serverp);

    serverp->states &= ~SR_DOWN;
    serverp->downTime = 0;
    serverp->failoverCount = 0;
    if ((serverRestartEpoch == serverp->serverEpoch)
	 || (serverp->serverEpoch == 0))
	/* Must be the network partion (not server crash) that separate us */
	/* Alternatively, this is a new server, but we've already done TSR on
	 * the secondary interface so that we now have tokens that can be
	 * re-attached.
	 */
	 sameEpoch = 1;
    serverp->serverEpoch = serverRestartEpoch;

    if (cm_HaveTokensFrom(serverp)) {/* slow path */
	/*
	 * We are here because the server might have been down before or the
	 * server has not heard from us for a long time. Our held token status
	 * may not be in sync with that of the server.
	 * We have to try to recover tokens and to continue keeping those
	 * files alive.
	 */
	/* But we don't need to do anything here if this was a new-style
	 * TKN_InitTokenState call and the server knows it too.
	 */
	if (!(serverp->states & SR_NEWSTYLESERVER)
	    || !(Flags & TKN_FLAG_NEW_IF)) {
	    serverp->states |= SR_TSR;      /* Enter TSR mode */
	    lock_ReleaseWrite(&serverp->lock);
	    threadParamp = (struct cm_params *)osi_Alloc(sizeof(struct cm_params));
#ifdef SGIMIPS
	    threadParamp->param1 = (unsigned long) sameEpoch;
	    threadParamp->param2 = (unsigned long) serverp;
	    threadParamp->param3 = (unsigned long) Flags;
#else  /* SGIMIPS */
	    threadParamp->param1 = (unsigned32) sameEpoch;
	    threadParamp->param2 = (unsigned32) serverp;
	    threadParamp->param3 = (unsigned32) Flags;
#endif /* SGIMIPS */

	    /* Perform the TSR asynchornously */
	    icl_Trace0(cm_iclSetp, CM_TRACE_STARTTSR);
	    (void) tpq_QueueRequest(cm_threadPoolHandle, cm_QueuedRecoverTokenState,
				    (void *)threadParamp, TPQ_HIGHPRI, 0, 0, 0);
	} else {
	    lock_ReleaseWrite(&serverp->lock);
	}
    } else {/* fast path */
        /*
	 * This is normally the case. That is, it may be cm's first time to
	 * contact this particular server. We do not have any tokens to be
	 * cleared.
	 */
	lock_ReleaseWrite(&serverp->lock);
    }

out:
    icl_Trace1(cm_iclSetp, CM_TRACE_TKNINITEND, ICL_TYPE_LONG, st);
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);
    return st;
}


/*
 * Revoke token RPC call.
 */

#ifdef SGIMIPS
static unsigned32 STKN_TokenRevoke( handle_t h,
     				    register afsRevokes *descp)
#else  /* SGIMIPS */
static unsigned32 STKN_TokenRevoke(h, descp)
     handle_t h;
     register afsRevokes *descp;
#endif /* SGIMIPS */
{
    afsFid *fidp;
    long tlen;
    register afsRevokeDesc *tdescp;
    register struct cm_scache *scp;
    struct cm_server *serverp;
    afs_recordLock_t rlocker;
    afs_hyper_t typeID;
    int i, j;
    long tflags, columnflags;
    afs_token_t *colAp, *colBp;
    afsUUID serverID;
    unsigned long bitmap;
    int preempting;
    int code, dt = 0;
    int ht = 0;
    unsigned32 st = rpc_s_ok;

    preempting = osi_PreemptionOff();
    if (st = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
	return st;
    }
    icl_Trace0(cm_iclSetp, CM_TRACE_TKNREVSTART);
    /*
     * First check to see which FX server this request is coming from.
     * We check it by using server's UUID rather than its IP addr.
     */
    rpc_binding_inq_object(h, &serverID, &st);
    if (st == rpc_s_ok) {
        if (uuid_is_nil(&serverID, &st)) {
	    struct sockaddr_in *sockPtr;
#ifdef SGIMIPS
	    rpc__binding_inq_sockaddr(h, (sockaddr_p_t *)&sockPtr, &st);
#else  /* SGIMIPS */
	    rpc__binding_inq_sockaddr(h, &sockPtr, &st);
#endif /* SGIMIPS */
	    cm_printf("dfs: rpc binding from server 'addr %x' has a nil uuid\n",
		    sockPtr->sin_addr.s_addr);
	    st = 0;
	    goto out;
	}
    } else {
	cm_printf("dfs: could not read the uuid from rpc binding (code = %d)\n",
		  st);
	 st = 0;
	 goto out;
    }
    serverp = cm_FindServer(&serverID);
    if (!serverp) {
	struct sockaddr_in *sockPtr;
#ifdef SGIMIPS
	rpc__binding_inq_sockaddr(h, (sockaddr_p_t *)&sockPtr, &st);
#else  /* SGIMIPS */
	rpc__binding_inq_sockaddr(h, &sockPtr, &st);
#endif /* SGIMIPS */
	icl_Trace2(cm_iclSetp, CM_TRACE_BADTKNREVOKEID,
		   ICL_TYPE_LONG, sockPtr->sin_addr.s_addr,
		   ICL_TYPE_UUID, &serverID);
	cm_printf("dfs: can't find server 'addr %x' based on its serverID\n",
		sockPtr->sin_addr.s_addr);
	st = 0;
	goto out;
    }
#ifdef DFS_BOMB
    /*
     * Now, we try to simulate a special effect by implanting a "bomb point"
     * here. If the bomb point goes off (triggered by a tester in the user
     * space), then this functions stops here and returns a value assigned by
     * the tester.
     */
    st = BOMB_EXEC("COMM_FAILURE", 0);
    if (st == rpc_s_comm_failure) {
       goto out;
    }
    st = BOMB_EXEC("GIVEUP_TOKEN", 0);
    if (st == -1) {
       st = 0;
       goto out;
    }
#endif /* DFS_BOMB */

    /*
     * As a rule, any rpc calls to be made in this code path (i.e., we are
     * in a token revokcation from the server) must be made through the
     * 'Secondary Service' to avoid creating a resource-deadlock on the server
     * side.
     */
    tlen = descp->afsRevokes_len;
    tdescp = &descp->afsRevokes_val[0];

    icl_Trace2(cm_iclSetp, CM_TRACE_TKNREVBASE, ICL_TYPE_LONG, tlen,
	       ICL_TYPE_POINTER, serverp);
    for (i = 0; i < tlen; i++, tdescp++) {
	fidp = &tdescp->fid;
	cm_AdjustIncomingFid(serverp, fidp);	/* get correct cell ID */
	AFS_hzero(tdescp->errorIDs);
	tdescp->outFlags = 0;

        /*
         * 32/64-bit interoperability changes:
	 *
         * Check to see if the server we're talking to might not know about the
         * 64-bit extenstions to the afs_recordLock_t type.  If it doesn't,
         * we need to clear the upper 32 bits of the start and end positions in
         * the record lock struct, so that 64-bit range checking works properly
         * (there may be garbage in the upper 32 bits since the server doesn't
         * set it).
         */
        if (!serverp->supports64bit) {
            AFS_hzerohi(tdescp->recordLock.l_start_pos);
            AFS_hzerohi(tdescp->recordLock.l_end_pos);
        }

	/*
	 * Since, TKN_SPOT_HERE is a special token type and is a per fileset
	 * property. Let's treat this differently than other token types.
	 */
	if (AFS_hgetlo(tdescp->type) & TKN_SPOT_HERE) {
	    if (ht = RevokeHereToken(fidp, tdescp->flags, serverp,
				     &tdescp->tokenID)) {
	        /* could not revoke this one */
		AFS_HOP32(tdescp->errorIDs, |, TKN_SPOT_HERE);
	    }
	    continue;
	}
	/* queue for possible racing calls */
	cm_QueueRacingRevoke(tdescp, serverp);
	lock_ObtainWrite(&cm_scachelock);
	scp = cm_FindSCache(fidp);
	lock_ReleaseWrite(&cm_scachelock);
	if (!scp)			/* not found, ignore revocation */
	    continue;
	icl_Trace2(cm_iclSetp, CM_TRACE_TKNREVFILE,
		   ICL_TYPE_POINTER, scp,
		   ICL_TYPE_LONG, AFS_hgetlo(tdescp->type));

	/*
	 * For each token in the low type field, revoke it.
	 */
	bitmap = 1;
	columnflags = tdescp->flags;
	for (j = 0; j < 32; j++, (bitmap<<=1)) {
	    /*
	     * If the bit is off, we're not revoking this type of token, and
	     * we just continue around the loop.
	     */
	    if (!(AFS_hgetlo(tdescp->type) & bitmap))
		continue;
	    /*
	     * Otherwise, we have a token to revoke, so we call the code that
	     * does a token revoke for one type of token.
	     */
	    AFS_hset64(typeID, 0, bitmap);
	    if (columnflags & AFS_REVOKE_COL_A_VALID) {
		colAp = &tdescp->columnA;
		/* if we're not really offered this token, don't consider it
		 * offered.
		 */
		if (!(AFS_hgetlo(tdescp->columnA.type) & bitmap))
		    colAp = (afs_token_t *) 0;
	    } else
	        colAp = (afs_token_t *) 0;

	    if (columnflags & AFS_REVOKE_COL_B_VALID) {
		colBp = &tdescp->columnB;
		/* if we're not really offered this token, don't consider it
		 * offered.
		 */
		if (!(AFS_hgetlo(tdescp->columnB.type) & bitmap))
		    colBp = (afs_token_t *) 0;
	    } else
	        colBp = (afs_token_t *) 0;

#ifdef SGIMIPS
	    /* guarentee that RevokeDataToken() will succede */
	    cm_rwlock(SCTOV(scp), VRWLOCK_WRITE);
#endif /* SGIMIPS */
	    code = cm_RevokeOneToken(scp, &tdescp->tokenID, &typeID,
				     &rlocker, colAp, colBp, &tflags, 0);
#ifdef SGIMIPS
	    cm_rwunlock(SCTOV(scp), VRWLOCK_WRITE);
#endif /* SGIMIPS */
#ifdef AFS_SUNOS5_ENV
	    if ((bitmap & (TKN_DATA_READ | TKN_DATA_WRITE)) &&
		code == HS_ERR_TRYAGAIN)
		dt = HS_ERR_TRYAGAIN;
#endif
	    if (code) {
		/*
		 * if revoke fails, turn on the corresponding bit in the error
		 * structure
		 */
#ifdef SGIMIPS
		AFS_HOP32(tdescp->errorIDs, |, (afs_hyper_t)bitmap);
#else  /* SGIMIPS */
		AFS_HOP32(tdescp->errorIDs, |, bitmap);
#endif /* SGIMIPS */
		if (tflags & AFS_REVOKE_LOCKDATA_VALID) {
		    tdescp->recordLock = rlocker;
		    tdescp->outFlags = AFS_REVOKE_LOCKDATA_VALID;

                    /*
                     * 32/64-bit interoperability changes:
		     *
                     * Check to see if the server we're talking to might not
                     * know about the 64-bit extenstions to the
                     * afs_recordLock_t type.  If it doesn't, we need to clear
                     * the upper 32 bits of the start and end positions in the
                     * record lock struct, so that 64-bit range checking works
                     * properly (there may be garbage in the upper 32 bits
                     * since the server doesn't set it).  I'm not completely
                     * convinced this is needed here, but it can't do any harm,
                     * and I believe the Cray code is doing this check here
                     * also.
                     */

                    if (!serverp->supports64bit) {
			AFS_hzerohi(tdescp->recordLock.l_start_pos);
			AFS_hzerohi(tdescp->recordLock.l_end_pos);
		    }
		}
	    } else {
		/* Got the token, watch for colA and colB valid.
		 * If we accepted the column A or column B tokens, reflect
		 * this back to the caller, too.  Note that whenever we accept
		 * a column X token, we accept all offered rights.
		 */
		if (tflags & AFS_REVOKE_COL_A_VALID) {
		    tdescp->colAChoice = tdescp->columnA.type;
		    columnflags &= ~AFS_REVOKE_COL_A_VALID;
		}
		if (tflags & AFS_REVOKE_COL_B_VALID) {
		    tdescp->colBChoice = tdescp->columnB.type;
		    columnflags &= ~AFS_REVOKE_COL_B_VALID;
		}
	    }
	}
	cm_PutSCache(scp);
    }
out:
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);
    /*
     * If we were unable to revoke a HERE token, return HS_ERR_TRYAGAIN.
     * We are relying on the assumption that no other token types were being
     * revoked.  I don't know what I'd do if there were other token types and
     * some of them got successfully revoked.
     *
     * Likewise for a data token that couldn't be revoked because of Solaris
     * VM problems.
     */
    if (st == 0 && (ht == HS_ERR_TRYAGAIN || dt == HS_ERR_TRYAGAIN))
	st = HS_ERR_TRYAGAIN;
    return st;
}


/*
 * Eliminate token bits in the given token that are covered by other tokens.
 * scp is locked here, and cm_scachelock might also be locked.
 * atlp is held.
 */
#define STRIPPABLE_RANGE_TYPES ( TKN_LOCK_READ | TKN_LOCK_WRITE \
	| TKN_DATA_READ | TKN_DATA_WRITE )
#define STRIPPABLE_NONRANGE_TYPES ( CM_OPEN_TOKENS \
	| TKN_STATUS_READ | TKN_STATUS_WRITE )
#define STRIPPABLE_TYPES (STRIPPABLE_RANGE_TYPES | STRIPPABLE_NONRANGE_TYPES)
void cm_StripRedundantTokens(register struct cm_scache *scp,
			     struct cm_tokenList *atlp,
			     long tokenType,
			     int checkExp,
			     int needReturn)
{
#ifdef SGIMIPS
    register struct cm_tokenList *tlp;
#else  /* SGIMIPS */
    register struct cm_tokenList *tlp, *newlp;
    struct cm_tokenList *nlp;
    afsFid *fidp = &scp->fid;
#endif /* SGIMIPS */
    unsigned32 possibleTypes, temp, thisClearedTypes, formerTypes, earlyTypes;
    unsigned32 allClearedTypes, remainingTypes;
    struct cm_tokenList tokenl;	/* temp for return call */
    u_long expiryThreshold;	/* A viable token has to be good until this */

    if ((AFS_hgetlo(atlp->token.type) & STRIPPABLE_TYPES) == 0) {
	/* this procedure is constrained to be a no-op */
	return;
    }
    expiryThreshold = osi_Time() + (15*60);	/* never dup to an expiring token */
    /* Cut back to only what the CM ever cares about */
    formerTypes = AFS_hgetlo(atlp->token.type);
    allClearedTypes = formerTypes & ~CM_ALL_TOKENS;
    remainingTypes = formerTypes & CM_ALL_TOKENS;
    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *) &scp->tokenList;
	 tlp = (struct cm_tokenList *) tlp->q.next) {
	possibleTypes =
#ifdef SGIMIPS
	    AFS_hgetlo(tlp->token.type) & remainingTypes & (unsigned32)tokenType;
#else  /* SGIMIPS */
	    AFS_hgetlo(tlp->token.type) & remainingTypes & tokenType;
#endif /* SGIMIPS */
	if (tlp != atlp && possibleTypes != 0 &&
	    !(tlp->flags & (CM_TOKENLIST_DELETED | CM_TOKENLIST_ASYNC
				| CM_TOKENLIST_REVALIDATE))) {
	    /*
	     * There's some hope for redundancy.
	     * Is this token longer-lived than the given one?
	     * It's always OK to use a token with a longer lifetime.  Tokens with
	     * shorter lifetimes are prohibited if checkExp is true or if the
	     * shorter-lifetime token is due to expire soon.
	     */
	    if ((tlp->token.expirationTime <= atlp->token.expirationTime) &&
		(checkExp || (tlp->token.expirationTime <= expiryThreshold)))
		continue;
	    earlyTypes = remainingTypes;
	    /* Can eliminate the non-range tokens straightaway */
	    temp = possibleTypes & STRIPPABLE_NONRANGE_TYPES;
	    remainingTypes &= ~temp;
	    thisClearedTypes = temp;
	    /* Check ranges first for these types */
	    temp = possibleTypes & STRIPPABLE_RANGE_TYPES;
	    if (temp != 0 &&
		AFS_hcmp(tlp->token.beginRange, atlp->token.beginRange) <= 0 &&
		AFS_hcmp(tlp->token.endRange, atlp->token.endRange) >= 0) {
		remainingTypes &= ~temp;
		thisClearedTypes |= temp;
	    }
	    allClearedTypes |= thisClearedTypes;
	    if (thisClearedTypes & TKN_DATA_READ)
		cm_UpdateDCacheOnLineState(scp, &atlp->token, &tlp->token);
	    if (earlyTypes != remainingTypes)
		icl_Trace4(cm_iclSetp, CM_TRACE_REDUNDANT1,
			   ICL_TYPE_HYPER, &atlp->token.tokenID,
			   ICL_TYPE_LONG, earlyTypes,
			   ICL_TYPE_HYPER, &tlp->token.tokenID,
			   ICL_TYPE_LONG, thisClearedTypes);
	    if (remainingTypes == 0) {
		break;
	    }
	}
    }
    if (allClearedTypes & CM_ALL_TOKENS) {
	icl_Trace4(cm_iclSetp, CM_TRACE_REDUNDANT2,
		   ICL_TYPE_POINTER, atlp,
		   ICL_TYPE_HYPER, &atlp->token.tokenID,
		   ICL_TYPE_LONG, formerTypes,
		   ICL_TYPE_LONG, remainingTypes);
	tokenl = *atlp;	/* assign fid, id, type */
	AFS_hset64(tokenl.token.type, 0, allClearedTypes);
	tokenl.token.expirationTime = 0x7fffffff;
	/* Now clear things out. */
	AFS_HOP32(atlp->token.type, &~, allClearedTypes);
	if (AFS_hiszero(atlp->token.type)) {
	    cm_DelToken(atlp);
	}
	/*
	 * Return this token if asked to, either in this call, or due to an
	 * earlier failed revocation request.
	 */
	if (needReturn ||
	    ((atlp->flags & CM_TOKENLIST_RETURNL) &&
	      (allClearedTypes & (TKN_LOCK_READ | TKN_LOCK_WRITE))) ||
	    ((atlp->flags & CM_TOKENLIST_RETURNO) &&
	      (allClearedTypes & (TKN_OPEN_READ | TKN_OPEN_WRITE |
				TKN_OPEN_SHARED | TKN_OPEN_EXCLUSIVE)))) {
	    lock_ReleaseWrite(&scp->llock);
	    cm_QueueAToken(atlp->serverp, &tokenl, 1);
	    lock_ObtainWrite(&scp->llock);
	}
    }
}

/*
 * Called with cm_scachelock held, as well as scp's own lock.
 */
static long
RevokePreserveToken(
  struct cm_scache *scp,
  afs_token_t *tlp,
  long tokenType,
  int local)
{
    /*
     * If our own reference is the only one, go ahead and revoke.
     * If this vnode is the cell root (/.../mumble/fs), there will be an
     * extra reference held by the virtual dir node, so we allow for that.
     * Of course, no one will actually delete the cell root, but we will
     * get a revoke request when its aggregate is detached.
     */
#ifdef SGIMIPS
    if ((CM_RC(scp) == 2) ||
	((CM_RC(scp) == 3) && (scp->states & SC_CELLROOT))) {
	scp->states &= ~(SC_RETURNREF | SC_STARTED_RETURNREF);
#else  /* SGIMIPS */
    if ((CM_RC(scp) == 1) ||
	((CM_RC(scp) == 2) && (scp->states & SC_CELLROOT))) {
	scp->states &= ~SC_RETURNREF;
#endif /* SGIMIPS */
	return 0;
    }

    /*
     * If we get here, the token is busy, mark cache entry so that we return
     * the token when all references are dropped.
     */
    if (local == 0)
	scp->states |= SC_RETURNREF;
    return (1);
}


static long
RevokeOpenToken(
  struct cm_scache *scp,
  afs_token_t *tlp,
  long tokenType,
  int local)
{
#ifdef AFS_SUNOS5_ENV
    /* The cell root has a hold from the parent SC_VDIR-type pointer. */
    if (scp->opens != 0 &&
	CM_RC(scp) == ((scp->states & SC_CELLROOT) ? 2 : 1)) {
	/* Down to base level, everything locked.  Clear all opens. */
	/* All open tokens may then be revoked. */
	scp->opens = 0;
	scp->writers = 0;
	scp->shareds = 0;
	scp->readers = 0;
    }
#endif /* AFS_SUNOS5_ENV */
    if (tokenType & TKN_OPEN_READ) {
	/*
	 * Trying to revoke an open for read token is o.k. iff the
	 * file ain't open for reading
	 */
	if (scp->readers == 0)
	    return (0);
    } else if (tokenType & TKN_OPEN_SHARED) {
	if (scp->shareds == 0)
	    return (0);
    } else if (tokenType & TKN_OPEN_EXCLUSIVE) {
	if (scp->exclusives==0)
	    return (0);
    } else if (tokenType & TKN_OPEN_WRITE) {
	if (scp->writers == 0)
	    return (0);
    }

    /*
     * If we get here, the token is busy, mark cache entry so that we return
     * the token when all files are closed.
     */
    if (local == 0)
	scp->states |= SC_RETURNOPEN;
    return (1);
}


/*
 * tricky issues: even if losing read token, if cache is dirty,
 * make sure that we store back update.  Also, note that we want
 * to ensure that all dirty data is gone under a lock, so we wrap
 * this check in a while loop, since SyncSCache obtains and drops
 * the vnode's scache lock.
 */
static long
RevokeStatusToken(
  struct cm_scache *scp,
  afs_token_t *tlp,
  long tokenType,
  int local)
{
    register long code = 0;

    CM_BUMP_COUNTER(statusTokensRevoked);
    for (;;) {
	/* are we done? */
	if (!cm_NeedStatusStore(scp)) break;
	if (local) {
	    /*
	     * Sorry, there's state we would need to return, but we can't
	     * do so here.
	     */
	    return 1;
	}
	/* otherwise, make the update unlocked, and retry things */
	lock_ReleaseWrite(&scp->llock);
	code = cm_SyncSCache(scp);
	lock_ObtainWrite(&scp->llock);
	if (code) {
	    break;	/* failed */
	}
    }
    if (tokenType & TKN_STATUS_READ) {
	/* We must free all cached ACLs. Its a little
	 * destructive. We may need a new Token Type for guarding ACLs, sigh.
	 * In addition turn off the SC_STATD state so that the next fetch of
	 * of tokens and status will be used by cm_MergeStatus. This is
	 * important if the file's change time at the server moves backwards.
	 */
	cm_FreeAllACLEnts(scp);
    }

    /* return 0 since we've either succeeded or marked the vnode bad;
     * in either case, we want the guy getting the conflicting token
     * to proceed.
     */
    return 0;
}


/* Try to revoke a lock token.  Just call into lock package, passing
 * in same parameters.
 */
static long
RevokeLockToken(
  struct cm_scache *scp,
  afs_token_t *tlp,
  long tokenType,
  afs_recordLock_t *lockerp,
  afs_token_t *colAp,
  afs_token_t *colBp,
  long *flagsp,
  struct cm_server *serverp,
  int local)	/* immaterial here */
{
  if (cm_TryLockRevoke(scp, &tlp->tokenID, tokenType, lockerp, colAp, colBp,
		       flagsp, serverp))
	return EBUSY;		/* for now, we hold token iff locked */
    /*
     * If TryLockRevoke succeeds, we're willing to return this token.
     */
    return error_status_ok;
}


static long
RevokeDataToken(
  struct cm_scache *scp,
  afs_token_t *tlp,
  long tokenType,
  int local)
{
    long code = 0;
    struct cm_rrequest rreq;
#ifdef SGIMIPS		/* 64 bit file support */
    int64_t filePos, beginPos, endPos;
#else  /* SGIMIPS */
    long filePos, beginPos, endPos;
#endif /* SGIMIPS */
    int dropped;
#ifdef SGIMIPS
    struct vnode *vp = SCTOV(scp);
    int rc=0;
#endif /* SGIMIPS */
    struct cm_dcache *dcp;

    CM_BUMP_COUNTER(dataTokensRevoked);

    /*
     * Store any modified data back.
     */
    if (scp->writerCredp)
	cm_InitReq(&rreq, scp->writerCredp);
    else
	cm_InitReq(&rreq, osi_credp);

#ifdef SGIMIPS
    /* 
     * The next set of if's to range check the file assumes 32 bit files.
     * We need to change this so it will work for 64 bit files.
     * Therefore the ifdef.
     * We could use the scp->volp->serverHost[x]->supports64bit field
     * to govern this checking.  However that is a bunch of locking to look
     * at these structs, plus a bit of figuring to find out which server
     * we are currently using in the case of replication.  Instead lets
     * take the easy way out and just not do this check.  The old 110 code
     * does not do this nor does the 122 cray cm code.
     * -BDR
     */

    /* Shouldn't we special case the whole file range (e.g. beginRange==0 &&
     * endRange >= scp->m.Length) and flush the entire file? */

    AFS_hset64(beginPos, AFS_hgethi(tlp->beginRange), 
		AFS_hgetlo(tlp->beginRange));

    /*
     * Do not add one to the end position if the end position is the
     * maximum value.  Otherwise we end up with a negative value for
     * the end position, and then data doesn't get written back to the
     * server.  Fix for bug 673380.
     */
    AFS_hset64(endPos, AFS_hgethi(tlp->endRange), AFS_hgetlo(tlp->endRange));
    if (AFS_hcmp(tlp->endRange, osi_hMaxint64) >= 0)
	endPos = osi_hMaxint64;
    else
	endPos = endPos + 1;
#else  /* SGIMIPS */

    /* Shouldn't we special case the whole file range (e.g. beginRange==0 &&
     * endRange >= scp->m.Length) and flush the entire file? */

    if (AFS_hcmp(tlp->beginRange, osi_hMaxint32) >= 0)
	goto done;
    beginPos = AFS_hgetlo(tlp->beginRange);
    if (AFS_hcmp(tlp->endRange, osi_hMaxint32) >= 0)
	endPos = 0x7fffffff;
    else
	endPos = AFS_hgetlo(tlp->endRange)+1;
#endif /* SGIMIPS */

    /*
     * Loop storing all modified chunks from the range back
     */
#ifdef SGIMIPS
    if (!local) {
        lock_ReleaseWrite(&scp->llock);
        osi_RestorePreemption(0);
        /* Only one thread per file in the flush/invalidate page code. */
        if (scp->volp->states & VL_RO) {
	    /*
	     * Because we could be called from cm_CheckVolSync() with
	     * the rwlock already held in VRWLOCK_READ mode, we can't
	     * lock it in VRWLOCK_WRITE mode.  Since this is a read-
	     * only replica, there should not be any dirty pages.
	     * Thus, we should be able to get by with tossing pages
	     * while only holding the rwlock in VRWLOCK_READ mode.
	     */
	    cm_rwlock(vp, VRWLOCK_READ);
	    osi_assert(vp->v_dpages == NULL);
	    SC_STARTED_FLUSHING_ON(scp, rc);
	    if (rc) {
		cm_rwunlock(vp, VRWLOCK_READ);
		lock_ObtainWrite(&scp->llock);
		return HS_ERR_TRYAGAIN;
	    }
	    VOP_TOSS_PAGES(vp, 0 ,SZTODFSBLKS(scp->m.Length) - 1,
				 FI_REMAPF_LOCKED);
	    SC_STARTED_FLUSHING_OFF(scp);
	    cm_rwunlock(vp, VRWLOCK_READ);
	}
	else {
	    /* Don't wait on the lock since we may be called by way
	     * of cm_ConnAndReset().  Sleeping here causes deadlock
	     * because no one else can can get a conn to make an rpc
	     * to this server.  Fix for bug 768541. */
	    if (!cm_rwlock_nowait(vp)) {
		lock_ObtainWrite(&scp->llock);
		return HS_ERR_TRYAGAIN;
	    }
	    SC_STARTED_FLUSHING_ON(scp, rc);
	    if (rc) {
		cm_rwunlock(vp, VRWLOCK_WRITE);
		lock_ObtainWrite(&scp->llock);
		return HS_ERR_TRYAGAIN;
	    }
	    VOP_FLUSHINVAL_PAGES(vp, 0 ,SZTODFSBLKS(scp->m.Length) - 1,
				 FI_REMAPF_LOCKED);
	    SC_STARTED_FLUSHING_OFF(scp);
	    cm_rwunlock(vp, VRWLOCK_WRITE);
	}
        osi_PreemptionOff();
        lock_ObtainWrite(&scp->llock);
    }
#endif /* SGIMIPS */
    do {
	dropped = 0;	/* dropped scache lock */
	for (filePos = beginPos;
	     filePos < endPos;
	     filePos += cm_chunksize(filePos)) {

	    /* Don't bother scanning past the end of the file; you'll
	     * just get bored. */

	    if ((scp->states & SC_LENINVAL) || filePos >= scp->m.Length)
		break;

	    dcp = cm_FindDCache(scp, filePos);	/* find the chunk */
	    if (!dcp)
		continue;		/* no chunk at this position */

	    if (local) {
		/* Not necessarily a no-op! */
		/* Claim that the token can't be relinquished locally. */
		cm_PutDCache(dcp);
		return 1;
	    }
#ifdef AFS_SUNOS5_ENV
	    /*
	     * Solaris hack:
	     * If SC_PPLOCKED is set, pages are locked awaiting the completion
	     * of a uiomove() call in cm_nfsrdwr().  This uiomove() may trigger
	     * a pagefault, which may cause a fetch RPC.  Our token revocation
	     * RPC's would have to wait for the pages to become unlocked.  But
	     * if a secondary-service RPC such as token revocation has to wait
	     * for a primary-service RPC such as fetch, deadlocks can happen.
	     * We forestall this by telling the revoker to try again later.
	     */
	    if ((cm_indexFlags[dcp->index] & DC_IFDIRTYPAGES) &&
		(scp->states & SC_PPLOCKED)) {
		cm_PutDCache(dcp);
		return HS_ERR_TRYAGAIN;
	    }
#endif /* AFS_SUNOS5_ENV */

	    /* purge VM for this chunk; start blocking out getpage calls */
	    scp->activeRevokes++;

	    if (cm_indexFlags[dcp->index] & DC_IFANYPAGES) {
		dropped = 1;
		lock_ReleaseWrite(&scp->llock);

		/*
		 * We set the force flag.
		 * Thus, even if there is an error, the pages get thrown away.
		 */
		code = cm_VMDiscardChunk(scp, dcp, 1 /* force */);

		lock_ObtainWrite(&scp->llock);
		if (code) {
		    cm_PutActiveRevokes(scp);
		    cm_MarkBadSCache(scp, ESTALE);
		    goto done;
		}
	    }
	    /*
	     * At this point, dcp doesn't have any dirty pages, so can
	     * clear that flag.
	     */
	    lock_ObtainWrite(&cm_dcachelock);
	    cm_indexFlags[dcp->index] &= ~(DC_IFANYPAGES | DC_IFDIRTYPAGES);
	    lock_ReleaseWrite(&cm_dcachelock);
	    /*
	     * If not modified, skip this chunk
	     */
	    lock_ObtainRead(&cm_dcachelock);
	    if (!(cm_indexFlags[dcp->index] & DC_IFDATAMOD)) {
		lock_ReleaseRead(&cm_dcachelock);
		cm_PutDCache(dcp);
	    } else {
		lock_ReleaseRead(&cm_dcachelock);
		lock_ReleaseWrite(&scp->llock);
		dropped = 1;
		code = cm_StoreDCache(scp, dcp, 0, &rreq);
		lock_ObtainWrite(&scp->llock);
		cm_PutDCache(dcp);
		if (code) {
		    /* if we can't put the data back, warn the user
		     * by zapping the file, and then return the token
		     * anyway, to prevent blocking other users from
		     * getting work done.
		     */
		    cm_MarkBadSCache(scp, ESTALE);
		}
	    }
	    cm_PutActiveRevokes(scp);
	    if (code)
		goto done;
	}	/* for loop */
    } while (dropped);

 done:
    if (tokenType & TKN_DATA_READ) {
	/*
	 * We never complain about giving these guys up.
         * Note: cm_RevokeOneToken takes care of updating the
         * online flag for the chunks in the range of the token.
	 */
	if (cm_vType(scp) == VDIR) {
	    nh_delete_dvp(scp);
	    scp->dirRevokes++;
	    AFS_hzero(scp->bulkstatdiroffset);
	}
	cm_FlushText(scp);
    }

    /* we always return success, since we've either succeeded in getting
     * the data back, or we've marked the file as bad, and want the
     * user trying to get an incompatible token to proceed.
     */
    return 0;
}

/*
 * This function must be called with NO lock held.
 */
static int
RevokeHereToken(
  afsFid *fidp,
  int flags,
  struct cm_server *origserverp,
  afs_hyper_t *tokIdp)
{
    struct cm_rrequest rreq;
    struct cm_volume *volp;
    struct cm_cell *cellp;
    int gotNewLoc;
    afs_hyper_t *cellIdp;
    long code = 0;
    long relinquish = 0;

    cm_InitUnauthReq(&rreq);       /* *must* be unauth for vldb */
    volp = cm_GetVolumeByFid(fidp, &rreq, 0, 1 /* quickSearch */);
    icl_Trace1(cm_iclSetp, CM_TRACE_REVOKEHERE,
	       ICL_TYPE_LONG, (volp ? AFS_hgetlo(volp->volume): -1));
    if (!volp) {
	/*
	 * There is not much we can do.
	 * Yet, let the token be revoked.
	 */
	return 0;
    }
    /* Check if the server is revoking some token other than the one we're
     * trying to hold.
     */
    lock_ObtainRead(&volp->lock);
    if (!AFS_hsame(*tokIdp, volp->hereToken.tokenID)) {
	icl_Trace4(cm_iclSetp, CM_TRACE_REVOKEHEREID,
		   ICL_TYPE_HYPER, &volp->hereToken.tokenID,
		   ICL_TYPE_HYPER, &volp->volume,
		   ICL_TYPE_HYPER, tokIdp,
		   ICL_TYPE_POINTER, origserverp);
	if (AFS_hsame(*tokIdp, volp->lameHereToken.tokenID))
	    code = HS_ERR_TRYAGAIN;
	else
	    code = 0;			/* Don't care about this token ID */
	lock_ReleaseRead(&volp->lock);
	goto out;
	/* do not set ``relinquish,'' since we still want our HERE token */
    }
    lock_ReleaseRead(&volp->lock);

    cellIdp = &fidp->Cell;
    if ((cellp = cm_GetCell(cellIdp)) == NULL) {
	icl_Trace4(cm_iclSetp, CM_TRACE_REVOKEHERECELL,
		   ICL_TYPE_HYPER, cellIdp,
		   ICL_TYPE_FID, fidp,
		   ICL_TYPE_HYPER, tokIdp,
		   ICL_TYPE_POINTER, origserverp);
	relinquish = 1;        /* There is no such cell here. !! */
	goto out;
    }
    if (flags & AFS_REVOKE_DUE_TO_GC) {
	/* if no scache entry has a pointer to the volume, we know
	 * that we're not using the HERE token to keep an scache's
	 * tokens around.  Otherwise, we might be.
	 */
	code = (volp->refCount <= 1 ? 0 : -1);
	icl_Trace1(cm_iclSetp, CM_TRACE_REVOKEHEREGC,
		   ICL_TYPE_LONG, code);
	if (code == 0)
	    relinquish = 1;	/*  Let the token be revoked ! */
	goto out;
    }
    lock_ObtainRead(&volp->lock);
    if (cm_SameHereServer(origserverp, volp)) {
        lock_ReleaseRead(&volp->lock);
        /*
         * cm_volume still has the old info about the fileset
         * Find out where the fileset's new location is by the next call ...
         */
        if (code = cm_CheckVolumeInfo(fidp, &rreq, volp, cellp, &gotNewLoc)) {
	    /*
	     * Error in talking to flserver. Note that cm_volume must be marked
	     * VL_RECHECK. Relinquish the HERE token !
	     */
	    relinquish = 1;
	    goto out;
	}
	lock_ObtainRead(&volp->lock);
    }
    lock_ReleaseRead(&volp->lock);

    /* Doing the TSR-move work ! */
    if (code = cm_RestoreMoveTokens(volp, origserverp, &rreq, TSR_MOVE)) {
        relinquish = 1;
    }
    icl_Trace0(cm_iclSetp, CM_TRACE_REVOKEHERE3);

out:
    if (relinquish) {
	lock_ObtainWrite(&volp->lock);
	bzero((char *) &volp->hereToken, sizeof(afs_token_t));
	volp->hereServerp = origserverp;
	lock_ReleaseWrite(&volp->lock);
	icl_Trace1(cm_iclSetp, CM_TRACE_RELINQUISH, ICL_TYPE_LONG, code);
	code = 0;
    }
    cm_PutVolume(volp);
    if (!relinquish)
	icl_Trace1(cm_iclSetp, CM_TRACE_REVOKEHERE_END,
		   ICL_TYPE_LONG, code);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}


#ifdef SGIMIPS
int cm_RevokeOneToken( register struct cm_scache *scp,
		       afs_hyper_t *baseIDp,
		       afs_hyper_t *typep,
		       afs_recordLock_t *rlockerp,
		       afs_token_t *colAp,
		       afs_token_t *colBp,
		       long *flagsp,
		       int local)
#else  /* SGIMIPS */
int cm_RevokeOneToken(scp, baseIDp, typep, rlockerp, colAp, colBp, flagsp, local)
    register struct cm_scache *scp;
    afs_hyper_t *baseIDp;
    afs_hyper_t *typep;
    afs_recordLock_t *rlockerp;
    afs_token_t *colAp, *colBp;
    long *flagsp;
    int local;
#endif /* SGIMIPS */
{
    register struct cm_tokenList *tlp;
    long tokenType;
    struct cm_tokenList *foundlp = 0;
    afs_token_t tempToken;
    register long code = 0;
    int grabbedScacheLock = 0;

    tokenType = AFS_hgetlo(*typep);
    icl_Trace1(cm_iclSetp, CM_TRACE_REVOKEONETOKEN, ICL_TYPE_LONG, tokenType);

    lock_ObtainWrite(&scp->llock);

    /* Choose which tokens will check the reference count */
#ifdef AFS_SUNOS5_ENV
#define CM_SCACHELOCK_OPEN_TOKENS \
    (TKN_OPEN_PRESERVE | TKN_OPEN_READ | TKN_OPEN_SHARED \
		     | TKN_OPEN_EXCLUSIVE | TKN_OPEN_WRITE)
#else /* AFS_SUNOS5_ENV */
#define CM_SCACHELOCK_OPEN_TOKENS (TKN_OPEN_PRESERVE)
#endif /* AFS_SUNOS5_ENV */

    if (tokenType & CM_SCACHELOCK_OPEN_TOKENS) {
	grabbedScacheLock = 1;
	lock_ObtainWrite(&cm_scachelock);
#ifdef SGIMIPS
	if ((CM_RC(scp) > 2) && (cm_vType(scp) == VDIR)) {
#else  /* SGIMIPS */
	if ((CM_RC(scp) > 1) && (cm_vType(scp) == VDIR)) {
#endif /* SGIMIPS */
	    /* Try to release any namecache references for a directory */
	    lock_ReleaseWrite(&cm_scachelock);
	    nh_delete_dvp(scp);
	    lock_ObtainWrite(&cm_scachelock);
	}
    }
    *flagsp = 0;	/* initialize */
    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
	 tlp != (struct cm_tokenList *) &scp->tokenList;
	 tlp = (struct cm_tokenList *) tlp->q.next) {
	if (AFS_hsame(tlp->token.tokenID, *baseIDp) &&
	    (tokenType & AFS_hgetlo(tlp->token.type))) {
	    /*
	     * Found the token, prepare to do the revoke processing.
	     * We save the token for the call, so that we can unlock
	     * the vnode before calling the specific revoker, since
	     * some of them have to unlock the vnode.
	     */
	    tempToken = tlp->token;
	    /*
	     * Now drop the lock and do the processing; some locks are
	     * reobtained by various revokers.
	     */
	    foundlp = tlp;
	    cm_HoldToken(tlp);
	    /*
	     * See if we can throw this token away early (and other types too).
	     * * But don't bother if this is a remote revocation.  The token
	     * manager on the server has already enumerated the conflicting
	     * tokens and has queued revocation requests for them, so it's not
	     * worth our cycling through the token list another time, here, in
	     * order to add to the RPC traffic in returning the duplicate
	     * tokens that will be revoked anyway.
             */
	    if (local && ((tokenType & STRIPPABLE_TYPES) != 0)) {
		cm_StripRedundantTokens(scp, tlp, AFS_hgetlo(tlp->token.type),
					0, 1);
		/* Eliminate bits that we won't care about below */
		tokenType &= AFS_hgetlo(tlp->token.type);
	    }
	    /*
	     * We release locks over those tokens we must make RPCs to return.
	     * Conveniently, only for the others do we have to handle delayed
	     * token returns (lock and open).
	     */
	    if (tokenType & TKN_OPEN_PRESERVE) {
		code = RevokePreserveToken(scp, &tempToken, tokenType, local);
	    } else if (tokenType & (TKN_OPEN_READ| TKN_OPEN_WRITE |
			     TKN_OPEN_SHARED | TKN_OPEN_EXCLUSIVE)) {
		code = RevokeOpenToken(scp, &tempToken, tokenType, local);
	    } else if (tokenType & (TKN_LOCK_READ | TKN_LOCK_WRITE)) {
		code = RevokeLockToken(scp, &tempToken, tokenType, rlockerp,
				       colAp, colBp, flagsp, tlp->serverp, local);
	    } else if (tokenType & (TKN_STATUS_READ | TKN_STATUS_WRITE)) {
		code = RevokeStatusToken(scp, &tempToken, tokenType, local);
	    } else if (tokenType & (TKN_DATA_READ | TKN_DATA_WRITE)) {
		code = RevokeDataToken(scp, &tempToken, tokenType, local);
	    }
	    icl_Trace2(cm_iclSetp, CM_TRACE_TKNREVFILEEND,
		       ICL_TYPE_POINTER, scp, ICL_TYPE_LONG, code);
	    break;
	}
    }

    /*
     * If we found and revoked a token, it is now time to modify the
     * cm's token list to record that we no longer have the token.
     */
    if (foundlp) {
	tlp = foundlp;			/* move into register */
	if (code) {
	    /* ``local'' implies not a remote revoker */
	    if (local == 0) {
		/*
		 * Can't revoke, return when possible
		 */
		if (tokenType & (TKN_LOCK_READ | TKN_LOCK_WRITE))
		    tlp->flags |= CM_TOKENLIST_RETURNL;
		if (tokenType & (TKN_OPEN_READ | TKN_OPEN_WRITE |
				 TKN_OPEN_SHARED | TKN_OPEN_EXCLUSIVE)) {
		    tlp->flags |= CM_TOKENLIST_RETURNO;
		}
		/*
		 * Potential race: if we drop the lock while revoking open or
		 * lock tokens, the token may actually be revokable again by
		 * the time we reestablish the lock here. So, if anyone changes
		 * the code to drop the lock for those tokens above, be sure to
		 * make sure that the tokens are still in use before setting
		 * the RETURN[LO] flags above.
                 */
	    }
	} else {
	    /*
	     * Remove the revoked token's bits from the token object
	     */
	    AFS_HOP32(tlp->token.type, &~, tokenType);

	    /*
	     * If the token entry has been totally invalidated i.e. has type
	     * field of 0, then we're done with it and we unthread and free it.
	     */
	    if (AFS_hiszero(tlp->token.type))
		cm_DelToken(tlp);
	}
	cm_ReleToken(tlp);	/* finally drop ref count */
	if (code == 0 && local == 0) {
	    /*
	     * After revoking one read-data token,  check to see we can turn off
	     * the online dcache flag for each dcache within that token range.
	     * Do it only when revocation was successful or client was remote.
	     */
	    if (tokenType & TKN_DATA_READ)
		cm_UpdateDCacheOnLineState(scp, &tempToken, (afs_token_t *)0);
	}
    } else {
	/* we didn't find the token, so we didn't do any processing on it.
	 * That's fine, except for the racing revoke case.  We've never
	 * used the token that we've just revoked (since it hasn't lasted
	 * long enough to get into the cache).  However, our dnlc code
	 * assumes that a data/read revoke will bump dirRevokes if a revoke
	 * comes in, even if we never use the token.  This is important
	 * since the dnlc code doens't explicitly look for a token before
	 * adding an entry, but instead just checks this counter.
	 * Anyway, we need to bump the counter manually here since it isn't
	 * getting bumped above.
	 * We shouldn't have to call nh_delete_dvp explicitly, but we
	 * do out of paranoia in this case.
	 */
	if ((tokenType & TKN_DATA_READ) && cm_vType(scp) == VDIR) {
	    if (grabbedScacheLock) {
		lock_ReleaseWrite(&cm_scachelock);
		grabbedScacheLock = 0;
	    }
	    nh_delete_dvp(scp);
	    scp->dirRevokes++;
	    AFS_hzero(scp->bulkstatdiroffset);
	}
    }
    if (grabbedScacheLock)
	lock_ReleaseWrite(&cm_scachelock);
    lock_ReleaseWrite(&scp->llock);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}


#ifdef SGIMIPS
static error_status_t STKN_AsyncGrant( handle_t h,
				       afsFid *fidp,
  				       afs_token_t *tokenp,
  				       idl_long_int requestID)
#else  /* SGIMIPS */
static unsigned32 STKN_AsyncGrant(h, fidp, tokenp, requestID)
  handle_t h;
  afsFid *fidp;
  afs_token_t *tokenp;
  long requestID;
#endif /* SGIMIPS */
{
    int preempting;
    unsigned32 st;
    register struct cm_tokenList *tlp;
    register struct cm_scache *scp;
    register struct cm_server *serverp;
    afsUUID serverUUID;

    preempting = osi_PreemptionOff();
    if (st = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
	return st;
    }

    icl_Trace2(cm_iclSetp, CM_TRACE_TKNASYNC, ICL_TYPE_FID, fidp,
	       ICL_TYPE_LONG, AFS_hgetlo(tokenp->type));

    rpc_binding_inq_object(h, &serverUUID, &st);
    if (st != rpc_s_ok) goto done;
    if (uuid_is_nil(&serverUUID, &st)) {
	goto done;
    }

    st = 0;
    serverp = cm_FindServer(&serverUUID);
    if (!serverp) {
	struct sockaddr_in *sockPtr;
#ifdef SGIMIPS
	rpc__binding_inq_sockaddr(h, (sockaddr_p_t *)&sockPtr, &st);
#else  /* SGIMIPS */
	rpc__binding_inq_sockaddr(h, &sockPtr, &st);
#endif /* SGIMIPS */
	icl_Trace2(cm_iclSetp, CM_TRACE_BADTKNASYNCGRANTID,
		   ICL_TYPE_LONG, sockPtr->sin_addr.s_addr,
		   ICL_TYPE_UUID, &serverUUID);
	goto done;
    }

    cm_AdjustIncomingFid(serverp, fidp);	/* get correct cell ID */
    /* find the vnode, too */
    lock_ObtainWrite(&cm_scachelock);
    scp = cm_FindSCache(fidp);
    lock_ReleaseWrite(&cm_scachelock);
    if (!scp) {
	st = HS_ERR_JUSTKIDDING;
	goto done;
    }

    lock_ObtainWrite(&scp->llock);
    tlp = cm_FindTokenByID(scp, &tokenp->tokenID);
    if (tlp) {
        /* now the token has been granted.  We're going to move
	 * the token into the "really granted" mode.
	 */
	tlp->flags &= ~CM_TOKENLIST_ASYNC;
	if (tlp->flags & CM_TOKENLIST_ASYNCWAIT) {
	    tlp->flags &= ~CM_TOKENLIST_ASYNCWAIT;
	    osi_Wakeup((char *) &tlp->flags);
	}
        tlp->token = *tokenp;	/* take the granted token */
	tlp->token.expirationTime += osi_Time() - /* time since grant */ 300;
    } else {
	/* queue racing grant item, in case async grant is being
	 * processed before the AFS_GetToken call has completed.
	 */
	cm_QueueRacingGrant(tokenp, serverp);
    }
    lock_ReleaseWrite(&scp->llock);
    cm_PutSCache(scp);
    icl_Trace1(cm_iclSetp, CM_TRACE_TKNASYNCDONE, ICL_TYPE_POINTER, scp);

  done:
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);

    return st;
}

/* cm_GetServerSize -- tell the server our size and find out his.  Called from
 *     cm_RecoverTokenState if the SetContext / InitTokenState exchange didn't
 *     clue us in. */

void cm_GetServerSize(struct cm_server *serverp)
{
    struct cm_rrequest rreq;
    struct cm_conn *connp;
    afsConnParams afsParams;
    unsigned32 st;
    u_long startTime;
    long code;
    unsigned32 value;

    if (serverp->maxFileParm != 0)
	return;

    /*
     * Tell the server what our maximum size is.
     */
    value = 0;
    bzero((char *)&afsParams, sizeof(afsParams));
    AFS_CONN_PARAM_SET(&afsParams, AFS_CONN_PARAM_MAXFILE_CLIENT,
		       OSI_MAX_FILE_PARM_CLIENT);
    cm_InitUnauthReq(&rreq);
    do {
	if (connp = cm_ConnByHost(serverp,  SEC_SERVICE(SRT_FX),
				  &serverp->cellp->cellId, &rreq, 0)) {

	    icl_Trace2(cm_iclSetp, CM_TRACE_SETPARAMSTART_1,
		       ICL_TYPE_POINTER, serverp,
		       ICL_TYPE_LONG, OSI_MAX_FILE_PARM_CLIENT);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    st = AFS_SetParams(connp->connp, AFS_PARAM_SET_SIZE, &afsParams);
	    code = osi_errDecode(st);
	    if (AFS_CONN_PARAM_ISVALID(&afsParams,
				       AFS_CONN_PARAM_MAXFILE_SERVER))
		value = AFS_CONN_PARAM_GET(&afsParams,
					   AFS_CONN_PARAM_MAXFILE_SERVER);
	    icl_Trace2(cm_iclSetp, CM_TRACE_SETPARAMEND_1,
		       ICL_TYPE_LONG, value,
		       ICL_TYPE_LONG, code);
	    osi_PreemptionOff();

	} else
	    code = -1;
    } while (cm_Analyze(connp, code, 0, &rreq, NULL, -1, startTime));

    if (code) {
	/*
	 * An rpc failure here will leave the server ignorant of the client's
	 * max file size.  Currently, that means the server will assume the
	 * default max file size, which is currently 0x7fffffff.  Similarly,
	 * the client will not know the server's max file size, and will assume
	 * 0x7fffffff.
         */
	return;
    }
    /* If the error code is zero, we can learn what the server's max size is */

    if (value) {
	lock_ObtainWrite(&serverp->lock);
	SetServerSize(serverp, value);
	lock_ReleaseWrite(&serverp->lock);
    }
}

static void cm_QueuedRecoverTokenState(void *arg, void *unused2)
{
     struct cm_params *paramsp = (struct cm_params *)arg;
     long sameEpoch;
     struct cm_server *serverp;
     struct cm_rrequest rreq;
     struct cm_conn *connp;
     afsConnParams afsParams;
     unsigned32 st, Flags, oldstates;
     u_long startTime;
     long code;

     sameEpoch = (long) paramsp->param1;
     serverp = (struct cm_server *)paramsp->param2;
#ifdef SGIMIPS
     Flags = (unsigned32)paramsp->param3;
#else  /* SGIMIPS */
     Flags = paramsp->param3;
#endif /* SGIMIPS */
     osi_Free(paramsp, sizeof(struct cm_params));
     /*
      * As a rule, any rpc calls to be made in this code path (i.e., we are
      * in a callback from the server) must be made through the 'Secondary
      * Service' to avoid creating a resource-deadlock on the server side.
      */

     cm_RecoverTokenState(serverp, sameEpoch, Flags);
     /*
      * Tell the server that we have completed the TSR work.
      *
      * NOTE: This call is required only if the server decides, in the future,
      * to perform a callback to client based on the info kept in the "stable
      * storage". The call is used to signal the completion of the TSR work.
      */
     /* Also tell the server what our maximum size is. */
     bzero((char *)&afsParams, sizeof(afsParams));
     AFS_CONN_PARAM_SET(&afsParams, AFS_CONN_PARAM_MAXFILE_CLIENT,
			OSI_MAX_FILE_PARM_CLIENT);
     cm_InitUnauthReq(&rreq);
     do {
	 if (connp = cm_ConnByHost(serverp,  SEC_SERVICE(SRT_FX),
				   &serverp->cellp->cellId, &rreq, 0)) {

	     icl_Trace0(cm_iclSetp, CM_TRACE_SETPARAMSTART);
	     startTime = osi_Time();
	     osi_RestorePreemption(0);
	     st = AFS_SetParams(connp->connp, AFS_PARAM_TSR_COMPLETE,
				&afsParams);
	     osi_PreemptionOff();
	     code = osi_errDecode(st);
	     icl_Trace1(cm_iclSetp, CM_TRACE_SETPARAMEND,
			ICL_TYPE_LONG, code);
	 } else
	     code = -1;
     } while (cm_Analyze(connp, code, 0, &rreq, NULL, -1, startTime));

     if (code) {
	 cm_printf("dfs: TSR AFS_SetParams failed: code %d.\n",
		   code);
	 /*
	  * The rpc fail doesn't matter in the current implementation. However,
	  * when the fx server decides to implement the "stable storage"
	  * algorithm, we may have to do some clean-up appropriately.
	  */
     }
     lock_ObtainWrite(&serverp->lock);
     /*
      * Check for server's maximum file size, passed back from AFS_SetParams().
      */
     if (AFS_CONN_PARAM_ISVALID(&afsParams, AFS_CONN_PARAM_MAXFILE_SERVER)) {
	 SetServerSize(serverp,
		       AFS_CONN_PARAM_GET(&afsParams,
					  AFS_CONN_PARAM_MAXFILE_SERVER));
     }
     oldstates = serverp->states;
     serverp->states &= ~SR_TSR;     /* Exit TSR mode */
     lock_ReleaseWrite(&serverp->lock);
     if (oldstates & SR_TSR) {
	 cm_PrintServerText("dfs: fx server ",  serverp, " in service\n");
	 icl_Trace0(cm_iclSetp, CM_TRACE_ENDTSR);
     }
}

#ifdef SGIMIPS
static unsigned32 STKN_GetCellName(
    handle_t h,
    afs_hyper_t *cellp,
    NameString_t cellNamepp)
#else  /* SGIMIPS */
static unsigned32 STKN_GetCellName(h, cellp, cellNamepp)
    handle_t h;
    afs_hyper_t *cellp;
    NameString_t cellNamepp;
#endif /* SGIMIPS */
{
    int preempting;
    unsigned32 st = error_status_ok;

    preempting = osi_PreemptionOff();
    if (st = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
	return st;
    }
    cellNamepp[0] = 0;
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);
    return st;
}

/*
 * These are debugging only RPC calls and might be removed without any warning
 */
#ifdef SGIMIPS
static error_status_t STKN_GetCE(
    handle_t h,
    unsigned32 index,
    afsDBCacheEntry *resultp)
#else  /* SGIMIPS */
static unsigned32 STKN_GetCE(h, index, resultp)
    handle_t h;
    u_long index;
    afsDBCacheEntry *resultp;
#endif /* SGIMIPS */
{
    register int i;
    register struct cm_scache *scp;
    int preempting;
    unsigned32 st;

    preempting = osi_PreemptionOff();
    if (st = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
#ifdef SGIMIPS
	return (error_status_t)st;
#else  /* SGIMIPS */
	return st;
#endif /* SGIMIPS */
    }
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
	    if (index == 0)
		goto done;
	    index--;
	}
    }
done:
    if (scp == (struct cm_scache *) 0) {
	st = 1;				/* past EOF */
	goto out;
    }
    /*
     * otherwise copyout result
     */
#ifdef SGIMIPS
    /* 
     * XXX  This is bad.  For a 64 bit machine there is no reason that the
     * XXX  scp addr will only be 32 bits!.  Yet this is a wire format 
     * XXX  structure so we can't change it.  Just quiet the compiler for now.
     * XXX  This should only be a debugging RPC!
     */
    resultp->addr = (unsigned32) ((unsigned long)scp & 0xffffffff);
#else  /* SGIMIPS */
    resultp->addr = (long) scp;
#endif /* SGIMIPS */
    resultp->fid.Cell = scp->fid.Cell;
    resultp->fid.Volume = scp->fid.Volume;
    resultp->fid.Vnode = scp->fid.Vnode;
    resultp->fid.Unique = scp->fid.Unique;
    resultp->lock = *(afsDBLockDesc *) &scp->llock;	/* XXX */
#ifdef SGIMIPS
    resultp->length = scp->m.Length;
#else  /*SGIMIPS */
    AFS_hset64(resultp->length, 0, scp->m.Length);
#endif /*SGIMIPS */
    resultp->dataVersion = scp->m.DataVersion;
    /*
     * Do something for tokens
     */
    resultp->refCount = CM_RC(scp);
    resultp->opens = scp->opens;
    resultp->writers = scp->writers;
#ifdef SGIMIPS
    resultp->states = (unsigned8)scp->states;
#else  /* SGIMIPS */
    resultp->states = scp->states;
#endif /* SGIMIPS */
out:
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);
#ifdef SGIMIPS
    return (error_status_t)st;
#else  /* SGIMIPS */
    return st;
#endif /* SGIMIPS */
}

#ifdef SGIMIPS
static error_status_t STKN_GetLock (
    handle_t  h,
    unsigned32 index,
    afsDBLock *resultp)
#else  /* SGIMIPS */
static unsigned32 STKN_GetLock (h, index, resultp)
    handle_t  h;
    u_long index;
    afsDBLock *resultp;
#endif /* SGIMIPS */
{
    struct ltable *tl;
    int nentries;
    int preempting;
    unsigned long st = error_status_ok;

    preempting = osi_PreemptionOff();
    if (st = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
#ifdef SGIMIPS
	return (error_status_t)st;
#else  /* SGIMIPS */
	return st;
#endif /* SGIMIPS */
    }
    nentries = sizeof(ltable)/sizeof(struct ltable);
    /* index is unsigned */
    if (index >= nentries) {
        st = 1;			/* past EOF */
	goto out;
    }
    tl = &ltable[index];
    strcpy((char *)resultp->name, tl->name);
#if 0
/*
 * XXX Need to fill in these values in some other way on SunOS
 */
    resultp->lock.waitStates = tl->addr->wait_states;
    resultp->lock.exclLocked = tl->addr->excl_locked;
    resultp->lock.readersReading = tl->addr->readers_reading;
    resultp->lock.numWaiting = tl->addr->num_waiting;
#endif /* 0 */
out:
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);
#ifdef SGIMIPS
    return (error_status_t)st;
#else  /* SGIMIPS */
    return st;
#endif /* SGIMIPS */
}

/*
 * This API is called by the fx server to advise the CM to change
 * some specific parameters. For example, the fx server could advise the CM
 * to perform the "keep-alive" call to the server less frequently.
 */
#ifdef SGIMIPS
static error_status_t STKN_SetParams (
    handle_t  h,
    unsigned32 Flags,
    afsConnParams *paramsPtr)
#else  /* SGIMIPS */
static unsigned32 STKN_SetParams (h, Flags, paramsPtr)
    handle_t  h;
    unsigned long Flags;
    afsConnParams *paramsPtr;
#endif /* SGIMIPS */
{
    int preempting;
#ifdef SGIMIPS
    unsigned32 st = error_status_ok;
#else  /* SGIMIPS */
    unsigned long st = error_status_ok;
#endif /* SGIMIPS */
    afsUUID serverID;
    struct cm_server *serverp;

    preempting = osi_PreemptionOff();
    if (st = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
	return st;
    }
    icl_Trace2(cm_iclSetp, CM_TRACE_TKNSETPARAMS_MORE,
	       ICL_TYPE_LONG, Flags,
	       ICL_TYPE_LONG, paramsPtr->Mask);

    /*
     * First check to see which FX server this request is coming from.
     * We check it by using server's UUID rather than its IP addr.
     */
    rpc_binding_inq_object(h, &serverID, &st);
    if (st == rpc_s_ok) {
        if (uuid_is_nil(&serverID, &st)) {
	    cm_printf("dfs: STKN_SetParams passed nil server uuid \n");
	    goto out;
	}
    } else {
	cm_printf("dfs: could not read uuid from rpc binding (code = %d)\n",
		  st);
	goto out;
    }
    serverp = cm_FindServer(&serverID);
    if (!serverp) {
	struct sockaddr_in *sockPtr;
#ifdef SGIMIPS
	rpc__binding_inq_sockaddr(h, (sockaddr_p_t *)&sockPtr, &st);
#else  /* SGIMIPS */
	rpc__binding_inq_sockaddr(h, &sockPtr, &st);
#endif /* SGIMIPS */
	icl_Trace2(cm_iclSetp, CM_TRACE_BADTKNINITID,
		   ICL_TYPE_LONG, sockPtr->sin_addr.s_addr,
		   ICL_TYPE_UUID, &serverID);
	cm_printf("dfs: can't find server 'addr %x' based on its server ID\n",
		sockPtr->sin_addr.s_addr);
	st = ENOENT;
	goto out;
    }

    /* Added by the 64/32-bit compatibility effort. */

    if (AFS_CONN_PARAM_ISVALID(paramsPtr, AFS_CONN_PARAM_MAXFILE_SERVER)) {
	lock_ObtainWrite(&serverp->lock);
	SetServerSize(serverp,
		      AFS_CONN_PARAM_GET(paramsPtr,
					 AFS_CONN_PARAM_MAXFILE_SERVER));
	lock_ReleaseWrite(&serverp->lock);
    }

    switch (Flags) {
    case AFS_PARAM_RESET_CONN:
    case AFS_PARAM_SET_SIZE:
      /* Allow this SET_SIZE value as another way to alter the parameters. */

      /* don't allow changing dead server timeout */

      /*
       * Allow changes to host and RPC lifetimes but RPC >= host
       * maxima must also be checked
       */
      if (AFS_CONN_PARAM_ISVALID(paramsPtr, AFS_CONN_PARAM_HOSTLIFE)) {
	  st = ENOSYS;	/* Sub-function not implemented */
      }

      if (AFS_CONN_PARAM_ISVALID(paramsPtr, AFS_CONN_PARAM_HOSTRPC))  {
	  st = ENOSYS;	/* Sub-function not implemented */
      }

      break;

    case AFS_PARAM_TSR_COMPLETE:
      st = ENOSYS;	/* Sub-function not implemented */
      break;

    default:
      cm_printf("dfs: Unreconized flag (%d) from TKN_SetParams\n", Flags);

    } /* switch */

    /* Change with new protocol: always return current values, not simply
     * the ones that changed.
     */
    /* Having dealt with the input parameters, make the return structure
     * represent what the connection parameters are currently set to.  In
     * addition, make its Mask field describe exactly what field values are
     * valid.
     */
    bzero((char *)paramsPtr, sizeof(*paramsPtr));
    lock_ObtainRead(&serverp->lock);
    AFS_CONN_PARAM_SET(paramsPtr, AFS_CONN_PARAM_HOSTLIFE,
		       serverp->origHostLifeTime);
    AFS_CONN_PARAM_SET(paramsPtr, AFS_CONN_PARAM_DEADSERVER,
		       serverp->deadServerTime);
#ifdef SGIMIPS
    AFS_CONN_PARAM_SET(paramsPtr, AFS_CONN_PARAM_EPOCH,
		       (unsigned32) serverp->serverEpoch);
#else  /* SGIMIPS */
    AFS_CONN_PARAM_SET(paramsPtr, AFS_CONN_PARAM_EPOCH,
		       serverp->serverEpoch);
#endif /* SGIMIPS */
    AFS_CONN_PARAM_SET(paramsPtr, AFS_CONN_PARAM_MAXFILE_CLIENT,
		       OSI_MAX_FILE_PARM_CLIENT);
    if (serverp->maxFileParm)
	AFS_CONN_PARAM_SET(paramsPtr, AFS_CONN_PARAM_MAXFILE_SERVER,
			   serverp->maxFileParm);
    lock_ReleaseRead(&serverp->lock);
out:
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);
#ifdef SGIMIPS
    return (error_status_t)st;
#else  /* SGIMIPS */
    return st;
#endif /* SGIMIPS */
}


#ifdef SGIMIPS
static unsigned32 STKN_GetServerInterfaces (
    handle_t  h,
    dfs_interfaceList *serverInterfacesP)
#else  /* SGIMIPS */
static unsigned32 STKN_GetServerInterfaces (h, serverInterfacesP)
    handle_t  h;
    dfs_interfaceList *serverInterfacesP;
#endif /* SGIMIPS */
{
    int preempting;
    register struct dfs_interfaceDescription *idp;
    rpc_if_id_t if_id;
#ifdef SGIMIPS
    unsigned32 st = error_status_ok;
#else  /* SGIMIPS */
    unsigned long st = error_status_ok;
#endif /* SGIMIPS */

    serverInterfacesP->dfs_interfaceList_len = 0;
    preempting = osi_PreemptionOff();
    if (st = cm_EnterTKN()) {
	osi_RestorePreemption(preempting);
	return st;
    }
    icl_Trace0(cm_iclSetp, CM_TRACE_TKNGETSI);
    /*
     * only one interface defined so far. New interfaces or extensions will
     * add to this list (not replacing the original).
     */
    rpc_if_inq_id (TKN4Int_v4_0_s_ifspec, &if_id, &st);
    if (st != error_status_ok)
        goto out;
    serverInterfacesP->dfs_interfaceList_len = 1;
    idp = &serverInterfacesP->dfs_interfaceList_val[0]; /* fill this in */

    bzero((caddr_t)idp, sizeof(struct dfs_interfaceDescription));
    idp->interface_uuid = if_id.uuid;
    idp->vers_major = if_id.vers_major;
    idp->vers_minor = if_id.vers_minor;
    idp->vers_provider = 1;	/* provider_version(1) from tkn4int.idl file */
    strncpy((char *)&idp->spareText[0], "Transarc TKN CM revocation service",
	    MAXSPARETEXT);
out:
    icl_Trace1(cm_iclSetp, CM_TRACE_GETSIEND, ICL_TYPE_LONG, st);
    cm_LeaveTKN();
    osi_RestorePreemption(preempting);
    return st;
}

globaldef TKN4Int_v4_0_epv_t TKN4Int_v4_0_manager_epv = {
    STKN_Probe,
    STKN_InitTokenState,
    STKN_TokenRevoke,
    STKN_GetCellName,
    STKN_GetLock,
    STKN_GetCE,
    STKN_GetServerInterfaces,
    STKN_SetParams,
    STKN_AsyncGrant
};
