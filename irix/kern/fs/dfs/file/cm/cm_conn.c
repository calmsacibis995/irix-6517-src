/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_conn.c,v 65.12 1999/08/24 20:05:41 gwehrman Exp $";
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

#include <dcedfs/param.h>		        /* Should be always first */
#include <dcedfs/afs4int.h>
#include <dcedfs/scx_errs.h>
#include <dcedfs/vol_errs.h>
#include <dcedfs/tpq.h>                    /* thread pool package */
#include <dce/secsts.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_conn.h>
#include <cm_volume.h>
#include <cm_stats.h>
#include <cm_site.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_conn.c,v 65.12 1999/08/24 20:05:41 gwehrman Exp $")

struct lock_data cm_connlock;		/* allocation lock for new conns */
static int cm_connSleepCounter=0;		/* count of waiters for idle conn,
					 * protected by cm_connlock.
					 */
struct lock_data cm_tgtlock;		/* allocation lock for new tgt */
struct cm_tgt *cm_tgtList = 0;		/* root of tgt linked list */
struct cm_tgt *FreeTGTList = 0;		/* free list for tgt entries */
struct cm_conn *FreeConnList = 0;	/* free list for cm_conn entries */
struct krpc_pool cm_concurrentCallPool;	/* call queuing pool */

/* Track the communication with the auth helper. */
static unsigned32 cm_helperError;
static unsigned32 cm_helperDeadTime;
#define CM_HELPERDEAD_INTERVAL	90

/*
 * Use the same secondary object uuid for all FX servers
 */
static uuid_t  secObjectUUID; 

extern struct  afsNetData cm_tknServerAddr;
#ifdef SGIMIPS
extern unsigned32 cm_tknEpoch;
#else  /* SGIMIPS */
extern unsigned long cm_tknEpoch;
#endif /* SGIMIPS */
static void cm_StompRPCBinding _TAKES((struct cm_conn *));

#ifdef SGIMIPS
extern int rpc__ifaddr_from_destaddr(
    struct in_addr to,
    struct in_addr *ifaddr,
    int *max_tpdu);

extern void rpc_mgmt_set_call_timeout(
    rpc_binding_handle_t,
    unsigned32,
    unsigned32 *);



/*
 * Wrapper around AFS_SetContext which uses a server specific address
 * for the TKN address of the client rather than the global one created
 * during initialization.
 */

error_status_t cm_SetContext(
    struct cm_conn *connp,
    unsigned32 Flags,
    afsUUID *secObjectID)
{
    struct afsNetData *tmp_tknServerAddr;
    register error_status_t status = 0;

    tmp_tknServerAddr = (struct afsNetData *) osi_AllocBufferSpace();

    *tmp_tknServerAddr = cm_tknServerAddr;
    ((struct sockaddr_in *)(&tmp_tknServerAddr->sockAddr))->sin_addr =
                                                        connp->tknServerAddr;
    status = AFS_SetContext(connp->connp,
                          cm_tknEpoch,  /* CM birth time */
                          tmp_tknServerAddr,
                          Flags,
                          secObjectID,
                          OSI_MAX_FILE_PARM_CLIENT, 0);
    osi_FreeBufferSpace((struct osi_buffer *) tmp_tknServerAddr);
    return(status);
}
#endif /* SGIMIPS */

/* define macro that tells us if a call should be automatically blocked.
 * right now, those are primary file system interface calls.  Secondary
 * calls are reserved already, and fileset location calls can be made
 * recursively during a token revoke.
 * I actually think it is safe to block repserver calls, too, but
 * it doesn't really matter, since it can use the same call reservation
 * as the related call.
 */
#define CM_TOPLEVELCALL(srvx) ((srvx) == MAIN_SERVICE(SRT_FX))

/* init the max # of calls we can handle */
#ifdef SGIMIPS
unsigned32 cm_ConnInit(int cbThreads)
#else  /* SGIMIPS */
unsigned32 cm_ConnInit(cbThreads)
  int cbThreads;
#endif /* SGIMIPS */
{
    unsigned32 st;

    cm_helperError = 0;
    cm_helperDeadTime = 0;

    /* each callback thread counts as one server call (the incoming call)
     * and one outgoing call (the potential storeback it makes to write
     * back dirty status or data). Plus we want CM_CONN_MAXCLIENTCALLS
     * of extra client calls for us to manage ourselves with the
     * cm_concurrentCallPool.
     */
    krpc_PoolInit(&cm_concurrentCallPool, CM_CONN_MAXCLIENTCALLS);

    /* declare how many concurrent client and server calls we'll be making */
    krpc_AddConcurrentCalls(CM_CONN_MAXCLIENTCALLS+cbThreads, cbThreads);

    st = dfsuuid_Create(&secObjectUUID, 1);	/* create an odd UUID */
    if (st != error_status_ok) {
	cm_printf("dfs: can't create secondary svc objectID (code %d)\n", 
		  st);
	return st;
    }
    return st;
}


/*
 * Connect to the server associated with the object, fidp
 */
#ifdef SGIMIPS
struct cm_conn *cm_Conn(
    register afsFid *fidp,
    unsigned long service,
    register struct cm_rrequest *rreqp,
    struct cm_volume **volpp,
    long *srvixp)
#else  /* SGIMIPS */
struct cm_conn *cm_Conn(fidp, service, rreqp, volpp, srvixp)
    register afsFid *fidp;
    unsigned long service;
    register struct cm_rrequest *rreqp;
    struct cm_volume **volpp;
    long *srvixp;
#endif /* SGIMIPS */
{
    register struct cm_volume *volp;
    register struct cm_conn *connp;

    if (*volpp)
	volp = *volpp;
    else {
	if (!(volp = cm_GetVolume(fidp, rreqp))) {
	    cm_FinalizeReq(rreqp);
	    rreqp->volumeError = 1;
	    return (struct cm_conn *) 0;
	}
	/*
	 * Note that we are about to start an RPC on this volume.
	 * This is used by MergeStatus() so remember to always
	 * call cm_EndVolumeRPC() after all possible calls to
	 * MergeStatus() have completed.
	 */
	cm_StartVolumeRPC(volp);
	*volpp = volp;			/* return it held */
    }
    if (volp->cellp)
	volp->cellp->lastUsed = osi_Time(); /* XXX no lock ?? */
    connp = cm_ConnByMHosts(volp->serverHost, service,
			    &fidp->Cell, rreqp, volp, NULL, srvixp);
    return connp;
}


/*
 * Reconnect to the new-interface server after doing a token reset.
 * We got a NEEDS_RESET code, but we weren't in a mutex, so check
 * whether we still need to reset tokens after cutting to a single thread.
 */
#ifdef SGIMIPS
static unsigned32 cm_ConnAndReset(
#else  /* SGIMIPS */
static unsigned long cm_ConnAndReset(
#endif /* SGIMIPS */
  struct cm_conn *connp,
  struct cm_server *serverp,
  struct cm_rrequest *rreqp)
{
#ifdef SGIMIPS
    register unsigned32 code;
#else  /* SGIMIPS */
    register long code;
#endif /* SGIMIPS */
    register int mustSleep;
    uuid_t otherObj;
    unsigned32 st, oldstates, callStartTime, callEndTime;

    icl_Trace3(cm_iclSetp, CM_TRACE_CONNANDRESET,
		ICL_TYPE_POINTER, connp,
		ICL_TYPE_POINTER, serverp,
		ICL_TYPE_LONG, connp->service);
    mustSleep = 0;
    lock_ObtainWrite(&serverp->lock);
    serverp->states |= SR_NEWSTYLESERVER;
    oldstates = serverp->states;
    serverp->states &= ~SR_DOWN;	/* allow outgoing RPCs to happen */
    serverp->downTime = 0;
 
    /* clear maxFileSize info here so we are forced to reevaluate it. */
    serverp->maxFileParm = 0;
    serverp->maxFileSize = osi_maxFileSizeDefault;
    serverp->supports64bit = 0;

    /* New interface, but a new or DOWN host object. */

    /*
     * We're here because a non-resetting AFS_SetContext call got the
     * NEED_RESET response.  If the SR_DOINGSETCONTEXT flag is clear, we set
     * it, carry out TSR, and clear it.  If that flag is already set, we
     * don't wait here, but count on other CM mechanisms (chiefly those in
     * cm_HaveTokens) to prevent us from using tokens that we aren't sure that
     * we have.  Of course, if it's this thread that has set the flag, then
     * the communication has failed again, and we need to indicate failure.
     *
     * The simplest such mechanism is the SR_TSR flag, which is set to tag
     * all tokens from a server as undergoing re-examination.  If we are
     * the thread that sets SR_DOINGSETCONTEXT, we don't unlock the server
     * lock (making SR_DOINGSETCONTEXT visible to other threads) before
     * setting SR_TSR.
     */
    if (serverp->states & SR_DOINGSETCONTEXT)  {
	/*
	 * Context setting is already happening.
	 * The thread doing it sets and clears SR_TSR.
	 */
	if (serverp->setCtxProcID == osi_ThreadUnique()) {
	    /* We're called from cm_RecoverTokenState.
	     * This TSR instance is doomed; we'll need to try again after
	     * cm_RecoverTokenState has returned.  Meanwhile, get this
	     * recursive call to fail so that cm_RecoverTokenState can fail
	     *  (If we were to return 0, the host would be marked back up and
	     * cm_ConnByMHosts would try us again.)
	     */
	    code = TKN_ERR_NEW_NEEDS_RESET;
	} else {
	    /* Don't starve the thread doing the set-context */
	    mustSleep = 1;	/* Sleep after we unlock */
	    code = 0;
	}
    } else {
	/* I'm the one who will try doing SetContext on this server. */
	/* Set both of these for the benefit of other callers. */
	serverp->states |= (SR_DOINGSETCONTEXT | SR_TSR);
	serverp->setCtxProcID = osi_ThreadUnique();
	lock_ReleaseWrite(&serverp->lock);
	lock_ObtainRead(&connp->lock);
	if (connp->service == MAIN_SERVICE(SRT_FX))  {
	    otherObj = secObjectUUID;
	} else {
	    otherObj = serverp->serverUUID;
	}
	lock_ReleaseRead(&connp->lock);
	/* We can make the RPC first or last, but
	 * NO MATTER WHAT, we have to treat all current tokens from this server
	 * as suspect.  This is because the RPC may have arrived at the server
	 * to set the host-object flag to be UP (not DOWN), even if we don't
	 * see the reply.  Since we set SR_TSR in the server, above, tokens
	 * granted from this server will not be used.  By the time we turn
	 * SR_TSR off, all tokens granted from this server (and attached to any
	 * scache entry) will have been re-evaluated, either being dropped,
	 * returned, or replaced with fresh tokens.
         */
	icl_Trace2(cm_iclSetp, CM_TRACE_CONNANDRESET3,
		   ICL_TYPE_POINTER, connp,
		   ICL_TYPE_LONG, connp->service);
	callStartTime = osi_Time();
#ifdef SGIMIPS
	st = BOMB_EXEC("COMM_FAILURE",
		    (osi_RestorePreemption(0),
		     st = cm_SetContext(connp,
					 /* do the new-protocol reset */
					 AFS_FLAG_CONTEXT_DO_RESET
					 | AFS_FLAG_CONTEXT_NEW_IF
					 | AFS_FLAG_CONTEXT_NEW_ACL_IF
					 | AFS_FLAG_SEC_SERVICE,
					 &otherObj),
		     			 osi_PreemptionOff(),
		     			 st));
#else  /* SGIMIPS */
	st = BOMB_EXEC("COMM_FAILURE",
		    (osi_RestorePreemption(0),
		     st = AFS_SetContext(connp->connp,
					 cm_tknEpoch,	/* CM birth time */
					 (afsNetData *)&cm_tknServerAddr,
					 /* do the new-protocol reset */
					 AFS_FLAG_CONTEXT_DO_RESET
					 | AFS_FLAG_CONTEXT_NEW_IF
					 | AFS_FLAG_CONTEXT_NEW_ACL_IF
					 | AFS_FLAG_SEC_SERVICE,
					 &otherObj,
					 OSI_MAX_FILE_PARM_CLIENT, 0),
		     osi_PreemptionOff(),
		     st));
#endif /* SGIMIPS */
	code = osi_errDecode(st);
	icl_Trace2(cm_iclSetp, CM_TRACE_CONNANDRESET4,
		   ICL_TYPE_POINTER, connp,
		   ICL_TYPE_LONG, code);

	/* React to a change in binding addr; syncs conn and binding addrs */
	cm_ReactToBindAddrChange(connp, 1 /* doOverride */);

	if (cm_RPCError(code) && code != rpc_s_unsupported_protect_level) {
	    callEndTime = osi_Time();
	    if ((callEndTime - callStartTime) < 3) {
#ifdef SGIMIPS
		cm_PrintServerText("dfs: RPC error from server ",
			serverp, "\n");
#endif /* SGIMIPS */
		cm_printf("dfs: RPC error %d received in %d seconds from setcontext/reset\n",
			  code, callEndTime - callStartTime);
		icl_Trace1(cm_iclSetp, CM_TRACE_FAST_RPC_ERROR_RESET,
			   ICL_TYPE_LONG, (callEndTime - callStartTime));
	    }
	}
	if (code >= TKN_ERR_NEW_SET_BASE
	    && code < (TKN_ERR_NEW_SET_BASE + 1024)) {
	    /* Do synchronous token recovery */
	    /* cm_RecoverTokenState has to record all its changes in memory */
	    lock_ObtainWrite(&serverp->lock);
	    if (serverp->lastCall < callStartTime)
		serverp->lastCall = callStartTime;
	    /* Drop this connp, so that cm_RecoverTokenState will be sure to get a conn. */
	    lock_ObtainWrite(&connp->lock);
	    if (connp->currAuthn != rpc_c_protect_level_none
		&& (serverp->lastAuthenticatedCall < callStartTime))
		serverp->lastAuthenticatedCall = callStartTime;
	    connp->states &= ~(CN_FORCECONN | CN_SETCONTEXT | CN_EXPIRED);
	    if (connp->states & CN_INPROGRESS) {
		connp->states &= ~CN_INPROGRESS;
		osi_Wakeup(&connp->states);
	    }
	    lock_ReleaseWrite(&connp->lock);
	    lock_ReleaseWrite(&serverp->lock);
	    cm_PutConn(connp);	/* flag this via return value */
	    (void) cm_RecoverTokenState(serverp, -1,
#ifdef SGIMIPS
				      (unsigned32)(code-TKN_ERR_NEW_SET_BASE));
#else  /* SGIMIPS */
					(code-TKN_ERR_NEW_SET_BASE));
#endif /* SGIMIPS */
	    lock_ObtainWrite(&serverp->lock);
	    serverp->states &= ~SR_TSR;	/* Exit server-TSR mode */
	    lock_ReleaseWrite(&serverp->lock);
	    code = TKN_ERR_NEW_SET_BASE; /* flag that we need a new conn */
	} else if (code == 0 || code == TKN_ERR_NEW_IF) {
	    /* This is incorrect protocol from this call! */
#ifdef SGIMIPS
	    cm_PrintServerText("dfs: bad return from server ", serverp, "\n");
#endif /* SGIMIPS */
	    cm_printf("dfs: bad return from new set context: code %d\n", code);
	    code = EINVAL;
	} else
	    (void) cm_ReactToAuthCodes(connp, (struct cm_volume *)NULL,
				       rreqp, code);
	lock_ObtainWrite(&serverp->lock);
	/* Release the lock */
	serverp->states &= ~SR_DOINGSETCONTEXT;
	serverp->setCtxProcID = 0;
    }
    lock_ReleaseWrite(&serverp->lock);
    /* Print a message if we changed this state. */
    if (oldstates & SR_DOWN) {
	cm_PrintServerText("dfs: file server ", serverp, " back up!\n");
    }
    if (mustSleep)
	    CM_SLEEP(1);	/* always sleep at least 1 second */
    return code;
}



/*
 * cm_ConnByHost() -- get a connection to the specified server
 * 
 * LOCKS USED: serverp->lock and cm_siteLock
 *
 * RETURN CODES: reference to connection, or NULL.
 */

#ifdef SGIMIPS
struct cm_conn*
cm_ConnByHost(
  struct cm_server *serverp,    /* server for connection */
  unsigned long service,        /* service type */
  afs_hyper_t *cellIdp,         /* cell ID for server */
  struct cm_rrequest *rreqp,    /* request record */
  long forceFlag)               /* attempt connection to down server (once) */
#else  /* SGIMIPS */
struct cm_conn*
cm_ConnByHost(serverp, service, cellIdp, rreqp, forceFlag)
  struct cm_server *serverp;    /* server for connection */
  unsigned long service;        /* service type */
  afs_hyper_t *cellIdp;         /* cell ID for server */
  struct cm_rrequest *rreqp;    /* request record */
  long forceFlag;               /* attempt connection to down server (once) */
#endif /* SGIMIPS */
{
    struct cm_conn *connp;
    struct sockaddr_in addr;
    int serverDown, fatalError;

    /* Essentially a wrapper for cm_ConnByAddr() which implements
     * server-address selection and retry logic for connection to a
     * single server.
     */

    do {
	/* Get "current" server address; normally is best-ranking up addr */
	lock_ObtainRead(&cm_siteLock);

	addr = serverp->sitep->addrCurp->addr;

	lock_ReleaseRead(&cm_siteLock);

	/* Attempt to get connection to server via chosen address */
	connp = cm_ConnByAddr(&addr,
			      serverp, service, cellIdp, rreqp, NULL,
			      forceFlag, &fatalError);

	icl_Trace4(cm_iclSetp, CM_TRACE_CONNBYHOST_RESULT,
		   ICL_TYPE_LONG, service,
		   ICL_TYPE_LONG, SR_ADDR_VAL(&addr),
		   ICL_TYPE_LONG, (connp ? SR_ADDR_VAL(&connp->addr) : 0),
		   ICL_TYPE_POINTER, connp);

	/* Determine retry status if connection could not be established */
	if (!connp) {
	    lock_ObtainRead(&serverp->lock);
	    serverDown = serverp->states & SR_DOWN;
	    lock_ReleaseRead(&serverp->lock);
	}
    } while (!connp && !serverDown && !fatalError);

    return connp;
}



/*
 * Connect to a specific server via a specific address.
 * The caller has NO lock on the server!
 */
#ifdef SGIMIPS
struct cm_conn*
cm_ConnByAddr(
    struct sockaddr_in *addrp,
    struct cm_server *serverp,
    unsigned long service,
    afs_hyper_t *cellIdp,
    struct cm_rrequest *rreqp,
    struct cm_volume *volp,	 /* optional */
    long forceFlag,
    int *fatalP)	/* whether to retry */
#else  /* SGIMIPS */
struct cm_conn*
cm_ConnByAddr(addrp, serverp, service, cellIdp, rreqp, volp, forceFlag, fatalP)
    struct sockaddr_in *addrp;
    struct cm_server *serverp;
    unsigned long service;
    afs_hyper_t *cellIdp;
    struct cm_rrequest *rreqp;
    struct cm_volume *volp;	 /* optional */
    long forceFlag;
    int *fatalP;	/* whether to retry */
#endif /* SGIMIPS */
{
    struct cm_conn *connp;
    int connCount;			/* count of matching conns */
    long code;
    char *principalName;
    unsigned32 mappedIdentity[2];
    uuid_t *objectp, *otherObjp;
    unsigned32 st, st2, oldstates, callStartTime, callEndTime;
    unsigned32 authn, authz, lbauthn, ubauthn;
    struct cm_security_bounds *secbp;
    short forceConn = 0, decreasedTimeout;
    int isExpired, forceUnauth, freshTries, retry;
#ifdef SGIMIPS
    int safety = 0;
#endif /* SGIMIPS */

    icl_Trace2(cm_iclSetp, CM_TRACE_CONNBYHOST, ICL_TYPE_LONG, service,
	       ICL_TYPE_POINTER, serverp);
    if (fatalP) {
	*fatalP = 0;
    }
    if (serverp->states & SR_DOWN) {
	if (forceFlag)
	    forceConn = 1;			/* be persistent! */
	else
	    return (struct cm_conn *) 0;	/* known down */
    }
    /*
     * The idea here is to figure out what kind of identity and authorization
     *   (a) that this user can claim,
     *   (b) that the CM is willing to use,
     *   (c) that the server will permit, and
     *   (d) that the fileset will permit.
     * Having arrived at these attributes, we look for an established
     * connection with those attributes, searching for an available such
     * connection but counting how many such connections are currently being
     * used.  If we find no available matching connection, we look at the
     * count of in-use connections, and if it exceeds the
     * CM_CONN_MAXCALLSPERUSER count, we wait for one of them to become
     * available (with suitable deadlock avoidance as well).  If there are
     * fewer than CM_CONN_MAXCALLSPERUSER matching connections in use, we
     * create a connection to our liking.
     *
     * Having obtained a suitable connection, we initialize the connection if
     * CN_FORCECONN is set, which will be the case if we just created it or
     * if some other process in the CM has marked it so (e.g. by calling
     * cm_MarkBadConns or cm_ResetUserConns).  Initialization consists of
     * making the RPC runtime calls that will establish its timeout and
     * authentication attributes.
     *
     * The user can claim her own identity only if she has a real PAG set
     * (meaning that the two mappedIdentity values are different).  As a
     * special case, unless CM_DISABLE_ROOT_AS_SELF is defined, we allow the
     * local root user, running with no PAG, to use the PAG belonging to the
     * local ..../self credential.  Even with a PAG, though, there may be no
     * TGT for the PAG value, or the TGT may be expired.
     *
     * Calls to the flserver or the repserver are always made with the
     * ..../self credential, as they are done on behalf of the entire CM and
     * not any specific one of its users.
     *
     * If the non-..../self caller has no available authentication, rather
     * than make unauthenticated calls to the file exporter, we make use of
     * the ..../self credential to authenticate, but we use authz_name
     * rather than authz_dce authorization.  This will not deliver a PAC to
     * the server, and the file server will therefore treat it as if it were
     * a call from an unauthenticated user, but it will allow authenticated
     * transfers to be used in the RPC calls.
     *
     * This procedure must ensure that it will not loop infinitely on odd
     * authentication situations.  We assume initially that the TGT is valid
     * and search for a connection on that basis.  TGT problems are captured
     * in the connection, though, in that the CN_EXPIRED state bit
     * indicates some problem with the TGT; upon detecting this, we alter our
     * expectations for the identity/authentication that we can claim and
     * restart the search for a matching connection.
     */
    forceUnauth = 0;
    freshTries = 0;
    /* Get initial connection parameters from the rreqp and the pag. */
    if (DFS_SRVTYPE(service) != SRT_FX) { /* Use DCE self if not FX. */
	mappedIdentity[0] = ~0;
	mappedIdentity[1] = 0;
	authz = rpc_c_authz_dce;
    } else if (rreqp->pag == rreqp->uid) { /* FX, but no PAG */
	/*
	 * Create an un-authenticated DFS connection by using the authz_name
	 * authorization service with the local ..../self identity.
	 */
	mappedIdentity[0] = ~0;
	mappedIdentity[1] = 0;
#ifndef CM_DISABLE_ROOT_AS_SELF
	/* DCE self if root, NAME self if not */
	authz = (rreqp->uid == 0) ? rpc_c_authz_dce : rpc_c_authz_name;
#else /* !CM_DISABLE_ROOT_AS_SELF */
	/* NAME self always */
	authz = rpc_c_authz_name;
#endif /* !CM_DISABLE_ROOT_AS_SELF */
    } else {	/* Binding to FX and we have a PAG */
	/* File server access runs as the user. */
#ifdef SGIMIPS
	mappedIdentity[0] = (unsigned32) rreqp->pag;
	mappedIdentity[1] = (unsigned32) rreqp->uid; /* effective user id */
#else  /* SGIMIPS */
	mappedIdentity[0] = rreqp->pag;
	mappedIdentity[1] = rreqp->uid;	/* effective user id */
#endif /* SGIMIPS */
	authz = rpc_c_authz_dce;
    }
    /* Make the initial presumption that the TGT is valid */
    isExpired = CM_TGTEXP_NOTEXPIRED;
  getfreshconn:
    /* We can return here with adjusted conn parameters */
    if (++freshTries > 50) {
	icl_Trace4(cm_iclSetp, CM_TRACE_CONN_GIVINGUP,
		   ICL_TYPE_LONG, forceUnauth,
		   ICL_TYPE_LONG, isExpired,
		   ICL_TYPE_LONG, authn,
		   ICL_TYPE_LONG, authz);
	goto accessProblem;
    }
    /*
     * React to expiration problems by checking for one or two cases in which
     * we will map to other forms of authentication.  If the compile-time
     * option is allowed, we map local root with a PAG value that corresponds
     * to no TGT (an accidental or meaningless PAG value) to DCE-authenticated
     * local root.  Otherwise, if there's any authentication problem and we're
     * not already using the ..../self entry, we do so.  If there's a problem
     * while we're using the ..../self credential, we don't do anything here,
     * but let the connection fall through to try being unauthenticated.
     */
    if (isExpired != CM_TGTEXP_NOTEXPIRED) {
	/*
	 * If a ticket expired to get us here, any subsequent error might be
	 * due to that fact, which might reasonably be interpreted as a
	 * transient error in our authentication.  We mark this condition in
	 * the cm_rrequest structure so that errors will be mapped to
	 * ETIMEDOUT, a more friendly error code than EACCES.
	 */
	if (isExpired == CM_TGTEXP_PAGEXPIRED) {
	    cm_FinalizeReq(rreqp);
	    rreqp->networkError = 1;
	}
#ifndef CM_DISABLE_ROOT_AS_SELF
	if (mappedIdentity[1] == 0 && mappedIdentity[0] != ~0
	    && authz == rpc_c_authz_dce
	    && isExpired == CM_TGTEXP_NOTGT) {
	    /* No TGT for DCE non-self local root?  Use DCE self. */
	    /* Expired TGT just stays expired (becoming authz_name, below). */
	    mappedIdentity[0] = ~0;
	    isExpired = CM_TGTEXP_NOTEXPIRED;
	} else
#endif /* !CM_DISABLE_ROOT_AS_SELF */
	    if (mappedIdentity[1] != 0 || mappedIdentity[0] != ~0) {
		/* Error on non-self or for non-root?  ->authz_name. */
		mappedIdentity[0] = ~0;
		mappedIdentity[1] = 0;
		authz = rpc_c_authz_name;
		isExpired = CM_TGTEXP_NOTEXPIRED;
	    }
    }
    if (cm_helperDeadTime) {
	lock_ObtainWrite(&cm_tgtlock);
	if ((cm_helperDeadTime + CM_HELPERDEAD_INTERVAL) < osi_Time())
	    cm_helperDeadTime = 0;
	lock_ReleaseWrite(&cm_tgtlock);
    }
    /* Check first for unbounded (non-)authentication */
    if (cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOAUTH) {
	/* Unauth binding, not checked. */
	/* If server complains, we'll clear CM_RREQFLAG_NOAUTH. */ 
	authn = rpc_c_protect_level_none;
    } else {
	/* We'll be bounding the authn choices */
	if (serverp->cellp->lclStates & CLL_LCLCELL) {
	    secbp = &cm_localSec;
	} else {
	    secbp = &cm_nonLocalSec;
	}
	/* Forced to be really unauthenticated? */
	if (cm_helperDeadTime || forceUnauth) {
	    /* Unauth binding, but bounds-check it */
	    authn = rpc_c_protect_level_none;
	} else if (isExpired != CM_TGTEXP_NOTEXPIRED) {
	    /* Well, no authn is available. */
	     authn = rpc_c_protect_level_none;
	} else if (DFS_SRVTYPE(service) == SRT_FX) {
	    /* Figure what kind of FX authn to start with. */
	     authn = secbp->initialProtectLevel;
	     if (authn == rpc_c_protect_level_default) {
		 authn = (serverp->cellp->lclStates & CLL_LCLCELL)
		   ? rpc_c_protect_level_pkt
		   : rpc_c_protect_level_pkt_integ;
	     }
	     /* Use any FLDB information as non-binding advice. */
	     if (volp) {
		 if (volp->minFldbAuthn > authn)
		     authn = volp->minFldbAuthn;
		 if (volp->maxFldbAuthn < authn)
		     authn = volp->maxFldbAuthn;
	     }
	} else {
	    /* flserver or repserver: different authn default. */
	    authn = rpc_c_protect_level_pkt_integ;
	}
	/* Coalesce the bounds before applying them */
	lbauthn = secbp->minProtectLevel;
	if (lbauthn == rpc_c_protect_level_default)
	    lbauthn = CM_MIN_SECURITY_LEVEL /* the lowest */;
	ubauthn = CM_MAX_SECURITY_LEVEL /* the highest */;
	if (serverp->minAuthn > lbauthn)
	    lbauthn = serverp->minAuthn;
	if (serverp->maxAuthn < ubauthn)
	    ubauthn = serverp->maxAuthn;
	if (volp) {
	    if (volp->minRespAuthn > lbauthn)
		lbauthn = volp->minRespAuthn;
	    if (volp->maxRespAuthn < ubauthn)
		ubauthn = volp->maxRespAuthn;
	}
	if (lbauthn > ubauthn) {
	    /* Constraints are mutually exclusive! */
	    icl_Trace4(cm_iclSetp, CM_TRACE_AUTHNFAIL1,
		       ICL_TYPE_POINTER, volp,
		       ICL_TYPE_LONG, serverp->cellp->lclStates,
		       ICL_TYPE_LONG, lbauthn,
		       ICL_TYPE_LONG, ubauthn);
	    goto accessProblem;
	}
	if (cm_helperDeadTime
	    || forceUnauth
	    || (isExpired != CM_TGTEXP_NOTEXPIRED)) {
	    if (lbauthn > rpc_c_protect_level_none) {
		/* Need to have more authentication than we can get! */
		icl_Trace4(cm_iclSetp, CM_TRACE_AUTHNFAIL2,
			   ICL_TYPE_POINTER, volp,
			   ICL_TYPE_LONG, serverp->cellp->lclStates,
			   ICL_TYPE_LONG, lbauthn,
			   ICL_TYPE_LONG, ubauthn);
		goto accessProblem;
	    }
	    authn = rpc_c_protect_level_none;
	} else /* some possible authentication */ {
	    /* Apply all the bounds. */
	    if (lbauthn > authn) {
		icl_Trace4(cm_iclSetp, CM_TRACE_AUTHNBOUND1,
			   ICL_TYPE_POINTER, volp,
			   ICL_TYPE_LONG, serverp->cellp->lclStates,
			   ICL_TYPE_LONG, authn,
			   ICL_TYPE_LONG, lbauthn);
		  authn = lbauthn;
	    } else if (ubauthn < authn) {
		icl_Trace4(cm_iclSetp, CM_TRACE_AUTHNBOUND2,
			   ICL_TYPE_POINTER, volp,
			   ICL_TYPE_LONG, serverp->cellp->lclStates,
			   ICL_TYPE_LONG, authn,
			   ICL_TYPE_LONG, ubauthn);
		  authn = ubauthn;
	    }
	}
    }
    for (;;) {
	lock_ObtainWrite(&serverp->lock);
	lock_ObtainWrite(&cm_connlock);
	for (;;) {
	    connCount = 0;
	    /* Find a match on addr,pag,authn,authz,svc */
	    for (connp = serverp->connsp; connp; connp = connp->next) {
		if (SR_ADDR_EQ(&connp->addr, addrp) &&
		    connp->authIdentity[0] == mappedIdentity[0] &&
		    connp->currAuthn == authn &&
		    connp->currAuthz == authz &&
		    connp->service == service) {
		    connCount++;
		    /*
		     * This check is under the wrong lock, but we check
		     * again under the correct lock before
		     * incrementing callCount.
		     */
		    if (connp->callCount == 0)
			break;  /* found one */
		    /* otherwise we found one, but it is busy.  Keep looking,
		     * but we may have to sleep and retry.
		     */
		}
	    }
	    /* now, if we have connp set, we have an unbusy connection which
	     * we can use.  Otherwise, there are no unbusy connections.  We
	     * either wait for a connection to free up, or create a new one,
	     * depending upon whether we're at the max # we're allowed to create.
	     *
	     * Well, if we need a new connp to do TSR, we allocate one.
	     */
	    if (connp || (connCount < CM_CONN_MAXCALLSPERUSER)) break;
	    /* faster to sleep for the call's end than to pay 1/3 second
	     * to authenticate yet another binding handle.
	     */
	    CM_BUMP_COUNTER(bindingHandleWaits);
	    lock_ReleaseWrite(&serverp->lock);
	    cm_connSleepCounter++;	/* > 0 if someone's sleeping */
	    osi_SleepW(serverp, &cm_connlock);
	    lock_ObtainWrite(&serverp->lock);
	    lock_ObtainWrite(&cm_connlock);
	    cm_connSleepCounter--;
	    /* Check if this thread is our only hope for doing TSR */
	    if ((serverp->states & SR_DOINGSETCONTEXT)
		&& serverp->setCtxProcID == osi_ThreadUnique())
#ifdef SGIMIPS
	    {
		safety = 1;
		break;
	    }
#else /* SGIMIPS */
		break;	/* if so, allocate another connection. */
#endif /* SGIMIPS */
	}
	if (!connp) {
	    CM_BUMP_COUNTER(bindingHandlesCreated);
	    if (!FreeConnList)
		connp = (struct cm_conn *) osi_Alloc(sizeof(struct cm_conn));
	    else {
		connp = FreeConnList;
		FreeConnList = connp->next;
	    }
	    icl_Trace2(cm_iclSetp, CM_TRACE_NEWCONN, ICL_TYPE_LONG, service,
		       ICL_TYPE_POINTER, connp);
	    bzero((caddr_t)connp, sizeof(*connp));
	    connp->addr = *addrp;
	    connp->authIdentity[0] = mappedIdentity[0];
	    connp->authIdentity[1] = mappedIdentity[1];
	    connp->service = service;
	    connp->serverp = serverp;
	    connp->states = CN_FORCECONN;
	    connp->currAuthn = authn;
	    connp->currAuthz = authz;
	    connp->expiryState = CM_TGTEXP_NOTEXPIRED;
	    connp->lifeTime = osi_Time() + CM_CONN_UNAUTHLIFETIME;
	    connp->next = serverp->connsp;
	    lock_Init(&connp->lock);
	    serverp->connsp = connp;
	}
	connp->refCount++;
	lock_ReleaseWrite(&cm_connlock);
	lock_ReleaseWrite(&serverp->lock);
#ifdef SGIMIPS
	if (CM_TOPLEVELCALL(service) && !safety)
#else /* SGIMIPS */
	if (CM_TOPLEVELCALL(service))
#endif /* SGIMIPS */
	    krpc_PoolAlloc(&cm_concurrentCallPool, 0, 1);
	else
	    krpc_PoolAlloc(&cm_concurrentCallPool, KRPC_POOL_NOFAIL, 1);
	lock_ObtainWrite(&connp->lock);
	/*
	 * Check callCount under the connp->lock, and retry if this connp is
	 * in use.  Increment callCount first since both paths will need to
	 * do that.  That is, even if we drop this one and get another, our
	 * cm_PutConn() call will decrement both counters.
	 */
	connp->callCount++;
	if (connp->callCount == 1)
	    break; /* if we're the only user */
	/* Free this one and get an unused one */
	icl_Trace2(cm_iclSetp, CM_TRACE_CONN_CLASH,
		   ICL_TYPE_POINTER, connp,
		   ICL_TYPE_LONG, connp->callCount);
	lock_ReleaseWrite(&connp->lock);
	cm_PutConn(connp);
    }
    /* Exit with connp->lock held for write and refCount, callCount bumped. */

    while (connp->states & CN_SETCONTEXT)  {
	/*
	 * Binding create is in progress.
	 */
	connp->states |= CN_INPROGRESS;
	osi_SleepW((opaque)(&connp->states), &connp->lock);
	lock_ObtainWrite(&connp->lock);
    }

    icl_Trace4(cm_iclSetp, CM_TRACE_CONN_WITHAUTH,
	       ICL_TYPE_POINTER, connp,
	       ICL_TYPE_LONG, connp->service,
	       ICL_TYPE_LONG, authn, ICL_TYPE_LONG, authz);

    /* Ensure that forceConn implies CN_FORCECONN */
    /* CN_FORCECONN says: conn might not yet be authn/authz'd */
    if (forceConn)
	connp->states |= CN_FORCECONN;
    if ((connp->states & CN_FORCECONN) == 0) {
	/* Any reason not simply to use this connection? */
	if (authn != rpc_c_protect_level_none
	    && (connp->states & CN_EXPIRED)) {
	    /* Whoops; this connection caches an authentication failure. */
	    if (connp->lifeTime < osi_Time()) {
		if (connp->connp)
		    cm_StompRPCBinding(connp);
		connp->states |= CN_FORCECONN;
		connp->states &= ~CN_EXPIRED;	/* for next time */
	    }
	    /* Ensure that we make progress on the next pass. */
	    icl_Trace3(cm_iclSetp, CM_TRACE_CONN_BADAUTHCACHE,
		       ICL_TYPE_LONG, isExpired,
		       ICL_TYPE_LONG, connp->expiryState,
		       ICL_TYPE_LONG, forceUnauth);
	    /* Pick up the cached result from trying to use this conn */
	    isExpired = connp->expiryState;
	    lock_ReleaseWrite(&connp->lock);
	    cm_PutConn(connp);
	    goto getfreshconn;
	}
    } else {
	connp->states |= CN_SETCONTEXT;
	if (connp->connp) {
	    cm_StompRPCBinding(connp);
	}
	code = krpc_BindingFromSockaddr("dfs", protSeq,
					(afsNetAddr *)addrp,
					&connp->connp);
	if (code) {
#ifdef SGIMIPS
	    osi_printIPaddr("dfs: can't create binding for address ",
		    SR_ADDR_VAL(addrp), "\n");
#endif /* SGIMIPS */
	    cm_printf("dfs: can't create binding from sockaddr: code %d\n",
		      code);
	    goto bad;
	}
#ifdef SGIMIPS
	/* Call down to routing code to establish a valid return IP
         * address for the server to send packets back to.
         */

        code = rpc__ifaddr_from_destaddr(addrp->sin_addr,
                                         &connp->tknServerAddr, NULL);
        if (code) {
            osi_printIPaddr("dfs: can't get local i/f IP address for dest addr ",
		    SR_ADDR_VAL(addrp), "\n");
            cm_printf("dfs: can't get local i/f IP address for dest addr code %d\n", code);
            goto bad;
        }
#endif /* SGIMIPS */

	/*
	 * Set call timeout for 10 minutes, as an emergency firewall.
	 */
	rpc_mgmt_set_call_timeout(connp->connp, 600 /* sec */, &st);
	if (st != error_status_ok) {
#ifdef SGIMIPS
	    cm_printf("dfs: can't set call timeout value: code %d, handle 0x%x\n",
		      st, connp->connp);
#else /* SGIMIPS */
	    cm_printf("dfs: can't set call timeout value: code %d\n", st);
#endif /* SGIMIPS */
	    /* goto bad; */
	}
	/* default + 2 should be about 2 minutes */
	rpc_mgmt_set_com_timeout(connp->connp,
				 rpc_c_binding_default_timeout+2, &st);
	if (st != error_status_ok) {
#ifdef SGIMIPS
	    cm_printf("dfs: can't set com timeout value: code %d, handle 0x%x\n",
		      st, connp->connp);
#else /* SGIMIPS */
	    cm_printf("dfs: can't set com timeout value: code %d\n", st);
#endif /* SGIMIPS */
	    /* goto bad; */
	}
	/*
	 * Create an authenticated binding
	 */
	if (authn != rpc_c_protect_level_none) {
	    /* allocate principalName off stack, since it is big */
	    principalName = (char *) osi_AllocBufferSpace();
	    if (DFS_SRVTYPE(connp->service) == SRT_FL) {
		/*
		 * The flserver principal already has dfs-server appended,
		 * so calling krpc_GetPrincName() does not work.
		 * The server principal names should be fixed to be consistent.
		 */
		strcpy(principalName, "/.../");
		strcat(principalName, serverp->cellp->cellNamep);
		strcat(principalName, "/");
		strcat(principalName, serverp->principal);
	    } else {
		krpc_GetPrincName(principalName, serverp->cellp->cellNamep,
				  &serverp->principal[0]);
	    }

	    lock_ReleaseWrite(&connp->lock);
	    callStartTime = osi_Time();
	    osi_RestorePreemption(0);
	    rpc_binding_set_auth_info(connp->connp,
				      (unsigned char *)principalName,
				      authn,
				      rpc_c_authn_dce_secret,
				      (rpc_auth_identity_handle_t)mappedIdentity,
				      authz,
				      &st);
	    osi_PreemptionOff();
	    osi_FreeBufferSpace((struct osi_buffer *) principalName);
	    icl_Trace4(cm_iclSetp, CM_TRACE_CONN_AUTHRESULT,
		       ICL_TYPE_POINTER, connp, ICL_TYPE_LONG, st,
		       ICL_TYPE_LONG, authn, ICL_TYPE_LONG, authz);
	    if (st != error_status_ok) {
		/* Check whether the problem is the helper or the auth. */
		if (st == rpc_s_helper_not_running
		    || st == rpc_s_helper_catatonic) {
		    if (st == rpc_s_helper_not_running)
#ifdef SGIMIPS
			cm_printf("dfs: auth helper not running; pag 0x%x, uid %d running unauthenticated.\n",
				mappedIdentity[0], mappedIdentity[1]);
#else /* SGIMIPS */
			cm_printf("dfs: auth helper not running; running unauthenticated.\n");
#endif /* SGIMIPS */
		    else
#ifdef SGIMIPS
			cm_printf("dfs: auth helper not responding; pag 0x%x, uid %d running unauthenticated.\n",
				mappedIdentity[0], mappedIdentity[1]);
#else /* SGIMIPS */
			cm_printf("dfs: auth helper not responding; running unauthenticated.\n");
#endif /* SGIMIPS */
		    lock_ObtainWrite(&cm_tgtlock);
		    if (cm_helperDeadTime < callStartTime)
			cm_helperDeadTime = callStartTime;
		    cm_helperError = st;
		    lock_ReleaseWrite(&cm_tgtlock);
		} else if (st == rpc_s_unsupported_protect_level
			   && authn > CM_MIN_SECURITY_LEVEL) {
		    /* Authentication level too high for this fileserver */
		    lock_ObtainWrite(&serverp->lock);
		    /* rpc_s_... is a condition that won't get better for this server, ever */
		    if (serverp->maxSuppAuthn > (authn-1)) {
			icl_Trace3(cm_iclSetp, CM_TRACE_CONN_DOWNMAXSUPP,
				   ICL_TYPE_LONG, serverp->maxSuppAuthn,
				   ICL_TYPE_LONG, authn-1,
				   ICL_TYPE_LONG, serverp->minAuthn);
			serverp->maxSuppAuthn = authn-1;
		    }
		    if (serverp->maxAuthn > (authn-1))
			serverp->maxAuthn = authn-1;
		    /* Push server lower-bound down in case it moved. */
		    if (serverp->maxAuthn < serverp->minAuthn)
			serverp->minAuthn = serverp->maxAuthn;
		    lock_ReleaseWrite(&serverp->lock);
		} else { /* Treat it as a problem with the current authentication. */
		    if (authz == rpc_c_authz_name) {
#ifdef SGIMIPS
			cm_printf("dfs: unauthenticated set auth binding failed pag 0x%x, uid %d\n",
				mappedIdentity[0], mappedIdentity[1]);
#else /* SGIMIPS */
			cm_printf("dfs: unauthenticated set auth binding failed\n");
#endif /* SGIMIPS */
		    }
		    if (st == rpc_s_auth_tkt_expired) {
			(void) cm_MarkExpired(mappedIdentity[0]);
			isExpired = CM_TGTEXP_PAGEXPIRED;
#ifdef SGIMIPS
			cm_printf("dfs: ticket for pag 0x%x, uid %d has expired; running unauthenticated\n",
				mappedIdentity[0], mappedIdentity[1]);
#else /* SGIMIPS */
			cm_printf("dfs: ticket has expired; running unauthenticated\n");
#endif /* SGIMIPS */
		    } else if (st == sec_login_s_no_current_context) {
			(void) cm_MarkExpired(mappedIdentity[0]);
			isExpired = CM_TGTEXP_NOTGT;
#ifdef SGIMIPS
			cm_printf("dfs: no ticket for pag 0x%x, uid %d; running unauthenticated.\n",
				  mappedIdentity[0], mappedIdentity[1]);
#else /* SGIMIPS */
#if 0
			cm_printf("dfs: no ticket for PAG %d; running unauthenticated.\n",
				  mappedIdentity[0]);
#endif /* 0 */
#endif /* SGIMIPS */
		    } else {
			/* we try for auth ONLY for a real PAG value */
			osi_assert(mappedIdentity[0] != mappedIdentity[1]);
			(void) cm_TGTLifeTime(mappedIdentity[0], &isExpired);
			if (isExpired == CM_TGTEXP_NOTEXPIRED)
			    isExpired = CM_TGTEXP_NOTGT;
#ifdef SGIMIPS
			cm_printf("dfs: set auth binding failed pag 0x%x, uid %d (code %d); running unauthenticated.\n",
				mappedIdentity[0], mappedIdentity[1], st);
#else /* SGIMIPS */
			cm_printf("dfs: set auth binding failed (code %d); running unauthenticated.\n", st);
#endif /* SGIMIPS */
		    }
		}
		/* This is a failed connection, but we leave it and get another. */
		lock_ObtainWrite(&connp->lock);
		if (isExpired != CM_TGTEXP_NOTEXPIRED) {
		    connp->expiryState = isExpired;
		    connp->states |= CN_EXPIRED;
		    /* keep this cache of the failure around for a bit */
		    connp->lifeTime = osi_Time() + 120;
		} else {
		    if (connp->connp)
			cm_StompRPCBinding(connp);
		    connp->states |= CN_FORCECONN;
		}
		connp->states &= ~CN_SETCONTEXT;
		if (connp->states & CN_INPROGRESS) {
		    connp->states &= ~CN_INPROGRESS;
		    osi_Wakeup(&connp->states);
		}
		lock_ReleaseWrite(&connp->lock);
		cm_PutConn(connp);
		goto getfreshconn;
	    }
	    /* Resume here if the binding-authentication attempt worked. */
	    lock_ObtainWrite(&connp->lock);
	    /* remember the TGT's expiration time for this connp */
	    connp->lifeTime = cm_TGTLifeTime(mappedIdentity[0], &isExpired);
	}  /* Create an authenticated binding */

	/*
	 * Set the object ID to the new binding depending on the service type
	 */
	switch (DFS_SRVTYPE(connp->service)) {

	  case SRT_FL:
	    rpc_binding_set_object(connp->connp, &serverp->objId, &st);
	    if (st != error_status_ok) {
#ifdef SGIMIPS
	       cm_PrintServerText("dfs: can't set uuid in conn to file location server ",
			 serverp, "\n");
#endif /* SGIMIPS */
	       cm_printf("dfs: can't set uuid in conn to file location server: code %d\n",
			 st);
	       goto bad;
	    }
	    break;

	  case SRT_FX:
	    /* if main service, bind to specific object, and
	     * pass in identity of secondary object explicitly.
	     * Otherwise, roles are reversed.
	     */
	    if (connp->service == MAIN_SERVICE(SRT_FX))  {
		objectp = &serverp->serverUUID;
		otherObjp = &secObjectUUID;
	    }
	    else {
		objectp = &secObjectUUID;
		otherObjp = &serverp->serverUUID;
	    }
	    rpc_binding_set_object(connp->connp, objectp, &st);
	    if (st != error_status_ok) {
#ifdef SGIMIPS
		cm_PrintServerText("dfs: can't set uuid in conn to file server ",
			  serverp, "\n");
#endif /* SGIMIPS */
		cm_printf("dfs: can't set uuid in conn to file server: code %d\n",
			  st);
		goto bad;
	    }
	    lock_ReleaseWrite(&connp->lock);
	    lock_ObtainRead(&serverp->lock);
	    /*
	     * If forceConn is set, the server was down at the beginning of
	     * this procedure.  If it is still down, use a smaller com timeout
	     * to avoid a long wait for reply that may never come.
	     * If the server is now UP, the lower timeout will be OK;
	     * if it is still down, diminishing the timeout will reduce our
	     * latency of checking.
	     */
	    decreasedTimeout = (forceConn && (serverp->states & SR_DOWN));
	    lock_ReleaseRead(&serverp->lock);
	    if (decreasedTimeout) {
		rpc_mgmt_set_com_timeout(connp->connp,
			 rpc_c_binding_default_timeout, &st); /* 30 Seconds */
	    }
	    icl_Trace2(cm_iclSetp, CM_TRACE_CONNIDSET,
		       ICL_TYPE_UUID, objectp,
		       ICL_TYPE_POINTER, connp);
	    icl_Trace2(cm_iclSetp, CM_TRACE_CONNIDSET2,
		       ICL_TYPE_UUID, otherObjp,
		       ICL_TYPE_POINTER, connp);
	    icl_Trace2(cm_iclSetp, CM_TRACE_STARTSETCONTEXT,
		       ICL_TYPE_POINTER, connp,
		       ICL_TYPE_LONG, connp->service);
	    callStartTime = osi_Time();
#ifdef SGIMIPS
	    st = BOMB_EXEC("COMM_FAILURE",
			   (osi_RestorePreemption(0),
			    st = cm_SetContext(connp,
				  /* need sec service, and try new I/F */
				  AFS_FLAG_CONTEXT_NEW_IF |
				  AFS_FLAG_CONTEXT_NEW_ACL_IF
				    | AFS_FLAG_SEC_SERVICE,
				  otherObjp),
			          osi_PreemptionOff(),
			          st));
#else  /* SGIMIPS */
	    st = BOMB_EXEC("COMM_FAILURE",
			   (osi_RestorePreemption(0),
			    st = AFS_SetContext(connp->connp,
				  cm_tknEpoch,	/* CM birth time */
				  (afsNetData *)&cm_tknServerAddr,
				  /* need sec service, and try new I/F */
				  AFS_FLAG_CONTEXT_NEW_IF |
				  AFS_FLAG_CONTEXT_NEW_ACL_IF
				    | AFS_FLAG_SEC_SERVICE,
				  otherObjp,
				  /* THIS IS REALLY PART OF
				   * ota-db7484-impl-RFC51.2 */
				  OSI_MAX_FILE_PARM_CLIENT, 0),
			    osi_PreemptionOff(),
			    st));
#endif /* SGIMIPS */
	    st = osi_errDecode(st);
	    icl_Trace1(cm_iclSetp, CM_TRACE_ENDSETCONTEXT,
		       ICL_TYPE_LONG, st);

	    /* React to a change in binding addr; syncs conn and bind addrs */
	    cm_ReactToBindAddrChange(connp, 1 /* doOverride */);

	    if (cm_RPCError(st) && st != rpc_s_unsupported_protect_level) {
		callEndTime = osi_Time();
		if ((callEndTime - callStartTime) < 3) {
#ifdef SGIMIPS
		    cm_PrintServerText("dfs: RPC error from server ",
			    serverp, "\n");
#endif /* SGIMIPS */
		    cm_printf("dfs: RPC error %d received in %d seconds from setcontext\n",
			      st, callEndTime - callStartTime);
		    icl_Trace1(cm_iclSetp, CM_TRACE_FAST_RPC_ERROR_SETCTX,
			       ICL_TYPE_LONG, (callEndTime - callStartTime));
		}
	    }
	    if (decreasedTimeout) {
		/*
		 *  Make sure the com timeout is reset to normal value.
		 *  This needs to happen before making the resetting
		 *   AFS_SetContext() call in cm_ConnAndReset().
		 */
		rpc_mgmt_set_com_timeout(connp->connp,
		    rpc_c_binding_default_timeout+2, &st2); /* ~ 2 Minutes */
		if (st2 != error_status_ok) {
#ifdef SGIMIPS
		    cm_printf("dfs: can't set com timeout value: code %d, handle 0x%x\n",
			      st2, connp->connp);
#else /* SGIMIPS */
		    cm_printf("dfs: can't set com timeout value: code %d\n", st2);
#endif /* SGIMIPS */
		    /* goto bad; */
		}
	    }
	    if (st == TKN_ERR_NEW_NEEDS_RESET) {
		/* Oops--new interface, but new object or we're DOWN. */
		/* Check again under a per-server mutex. */
		st = cm_ConnAndReset(connp, serverp, rreqp);
		if (st == TKN_ERR_NEW_SET_BASE) {
		    /* This thread carried out TSR after dropping this connp. */
		    /* Go back and try again. */
		    goto getfreshconn;
		}
#ifdef SGIMIPS
                if (st == TKN_ERR_NEW_NEEDS_RESET) {
                    /*
                     * We might be in big trouble here.  The only way out
                     * of cm_ConnAndReset with this flag set is if we have
                     * gone recursive and we are actually the guy in the
                     * process of doing a reset.  In other words we have
                     * been called by cm_RecoverTokenState.  This TSR
                     * instance is dommed; we need to try again after
                     * cm_RecoverTokenState has returned.  Don't rely on
                     * cm_DownServer (called below us) to do the right thing
                     * because there could be a ton of other SetContext calls
                     * going through and returning  TKN_ERR_NEW_NEEDS_RESET
                     * but setting lastCall in the server structure.
                     * Thus cm_DownServer will think we have talked to this
                     * server and therefore we should just retry.  We can't
                     * just retry.  We need to fail; unwind everything and
                     * try again.
                     */
                    cm_FinalizeReq(rreqp);
                    rreqp->tsrError = 1;
                    lock_ObtainWrite(&connp->lock);
                    goto bad;
                }
#endif /* SGIMIPS */
	    }
	    if (st == error_status_ok || st == TKN_ERR_NEW_IF) {
		lock_ObtainWrite(&serverp->lock);
		if (st == TKN_ERR_NEW_IF)
		    serverp->states |= SR_NEWSTYLESERVER;
		if (serverp->lastCall < callStartTime)
		    serverp->lastCall = callStartTime;
		if (serverp->lastAuthenticatedCall < callStartTime) {
		    lock_ObtainRead(&connp->lock);
		    if (connp->currAuthn != rpc_c_protect_level_none)
			serverp->lastAuthenticatedCall = callStartTime;
		    lock_ReleaseRead(&connp->lock);
		}
		/* turn off SR_DOWN in case server hadn't marked us DOWN */
		oldstates = serverp->states;
		serverp->states &= ~SR_DOWN;
		serverp->downTime = 0;
		serverp->failoverCount = 0;
		lock_ReleaseWrite(&serverp->lock);
		/* Print a message if it changed */
		if (oldstates & SR_DOWN) {
		    cm_PrintServerText("dfs: file server ", serverp, " back up!\n");
		}
		lock_ObtainWrite(&connp->lock);
		break;
	    }
	    /* See first if it's an authn code */
	    if (cm_ReactToAuthCodes(connp, volp, rreqp, st) == 1) {
		retry = 1;	/* OK, go for it again */
	    } else {
		/* Don't print the msg, if already down */
		if (!(serverp->states & SR_DOWN)) {
		    if (!cm_RPCError(st)) {
#ifdef SGIMIPS
			cm_PrintServerText("dfs: set context failed: server ",
			          serverp, "\n");
#endif /* SGIMIPS */
			cm_printf("dfs: set context failed: code %d\n", st);
		    } else {
#ifdef SGIMIPS
			cm_PrintServerText("dfs: comm failure/set context: server ",
				  serverp, "\n");
#endif /* SGIMIPS */
			cm_printf("dfs: comm failure/set context: code %d\n",st);
		    }
		}
		/* Do address fail-over and/or mark server down, if needed */
		(void)cm_ServerDown(serverp, connp, st);

		/* Let wrapper (cm_ConnBy[M]Host[s]) choose another addr */
		retry = 0;
	    }
	    if (retry) {
		/* Retry with same server-address */
		lock_ObtainWrite(&connp->lock);
		if (connp->connp)
		    cm_StompRPCBinding(connp);
		connp->states |= CN_FORCECONN;
		connp->states &= ~CN_SETCONTEXT;
		if (connp->states & CN_INPROGRESS) {
		    connp->states &= ~CN_INPROGRESS;
		    osi_Wakeup(&connp->states);
		}
		lock_ReleaseWrite(&connp->lock);
		cm_PutConn(connp);
		goto getfreshconn;
	    }
	    lock_ObtainWrite(&connp->lock);
#ifdef SGIMIPS
	    goto bad;
#else /* SGIMIPS */
	    goto bad1;
#endif /* SGIMIPS */

	  case SRT_REP:
	    /* got the binding to replication server */
	    break;

	  default:
#ifdef SGIMIPS
	    cm_printf("dfs: no such service type as %d, handle 0x%x\n",
		      DFS_SRVTYPE(connp->service), connp->connp);
#else /* SGIMIPS */
	    cm_printf("dfs: no such service type as %d\n",
		      DFS_SRVTYPE(connp->service));
#endif /* SGIMIPS */
	    goto bad;
	} /* switch */

	/* Got a new binding */
	connp->states &= ~(CN_FORCECONN | CN_SETCONTEXT | CN_EXPIRED);
	if (connp->states & CN_INPROGRESS) {
	    connp->states &= ~CN_INPROGRESS;
	    osi_Wakeup(&connp->states);
	}
	/* end if CN_FORCECONN case */
    }

    lock_ReleaseWrite(&connp->lock);
    return connp;

bad:
    if (fatalP)
	*fatalP = 1;
bad1:
    if (connp->connp)
        cm_StompRPCBinding(connp);
    /* Turn off CN_SETCONTEXT, CN_INPROGRESS, and CN_EXPIRED */
    connp->states &= CN_CALLCOUNTWAIT;	/* preserve only this flag (no ~) */
    connp->states |= CN_FORCECONN;
    osi_Wakeup(&connp->states);
    lock_ReleaseWrite(&connp->lock);
    cm_PutConn(connp);
    return NULL;

accessProblem:
    cm_FinalizeReq(rreqp);
    rreqp->accessError = 1;
    /*
     * Report a problem for callers for whom a null return is an
     * insufficient signal.
     */
    if (fatalP) {
	if (volp) {
	    /* record problem in volume's per-server badness info */
	    cm_SetVolumeServerBadness(volp, serverp, CM_ERR_PERS_CONNAUTH,
				CM_ERR_PERS_CONNAUTH - VOLERR_PERS_LOWEST);
	} else {
	    /* indicate that though server not down, should try another */
	    if (cm_ServerDown(serverp, connp, CM_ERR_PERS_CONNAUTH) == 0)
		*fatalP = 1;
	}
    }
    return (struct cm_conn *) 0;
}

/* stomp on an RPC binding.  This function is called with a connection
 * structure which is both held and has an active call (representing
 * the caller.  We wait (releasing the active call count, but not
 * the reference count (so the cm_conn structure stays around) until the
 * call count is 0, and then obliterate the rpc binding information.
 *
 * If we free the RPC binding while there are active calls, the
 * RPC package will segfault, since the binding is the only reference
 * keep various structures around.
 *
 * Note that if we don't drop the callCount before sleeping, two guys
 * could sleep, both waiting for the other to complete.   When we're
 * done, we return with the callCount and refCount both equal to
 * what they were at the start of the call.
 *
 * The connection structure is locked, and we return with it locked.
 */
#ifdef SGIMIPS
static void cm_StompRPCBinding(register struct cm_conn *connp)
#else  /* SGIMIPS */
static void cm_StompRPCBinding(connp)
    register struct cm_conn *connp;
#endif /* SGIMIPS */
{
    unsigned32 st;

    icl_Trace1(cm_iclSetp, CM_TRACE_STOMPBINDING, ICL_TYPE_POINTER, connp);

    /* wait there are no outstanding calls */
    for (;;) {
	if (connp->callCount == 1)
	    break;	/* only our reference */
	connp->callCount--;
	osi_assert(connp->callCount > 0);
	connp->states |= CN_CALLCOUNTWAIT;
	osi_SleepW(connp, &connp->lock);
	lock_ObtainWrite(&connp->lock);
	connp->callCount++;
    }
    /* if we get here, connection is locked, and we're ready to stomp
     * on the RPC binding.
     */
    rpc_binding_free(&connp->connp, &st);
    connp->connp = NULL;
}



/*
 * cm_ConnByMHosts() -- connect to any server in the specified set
 * 
 * LOCKS USED: cellp->lock (SRT_FL), volp->lock (SRT_FX/REP), one or more
 *     of the servers' locks, and cm_siteLock.
 *
 * RETURN CODES: reference to connection, or NULL
 */
#ifdef SGIMIPS
struct cm_conn*
cm_ConnByMHosts(
  struct cm_server *serverp[],           /* server set */
  unsigned long service,                 /* service type */
  afs_hyper_t *cellIdp,                  /* cell ID for servers */
  struct cm_rrequest *rreqp,             /* request record */
  register struct cm_volume *volp,       /* volume for SRT_FX/SRT_REP */
  register struct cm_cell *cellp,        /* cell for SRT_FL */
  long *srvixp)				 /* connected-server index; optional */
#else  /* SGIMIPS */
struct cm_conn*
cm_ConnByMHosts(serverp, service, cellIdp, rreqp, volp, cellp, srvixp)
  struct cm_server *serverp[];           /* server set */
  unsigned long service;                 /* service type */
  afs_hyper_t *cellIdp;                  /* cell ID for servers */
  struct cm_rrequest *rreqp;             /* request record */
  register struct cm_volume *volp;       /* volume for SRT_FX/SRT_REP */
  register struct cm_cell *cellp;        /* cell for SRT_FL */
  long *srvixp;				 /* connected-server index; optional */
#endif /* SGIMIPS */
{
    register struct cm_server *tserverp;
    register long i;
    long code, delayTime;
#ifdef SGIMIPS
    long now;
    unsigned long hostGen, addrGen;
#else  /* SGIMIPS */
    unsigned long now, hostGen, addrGen;
#endif /* SGIMIPS */

#ifdef SGIMIPS
    long eligibleServer;              /* current server of choice */
#else  /* SGIMIPS */
    int eligibleServer;              /* current server of choice */
#endif /* SGIMIPS */
    unsigned long eligibleRtt;       /* RTT of eligibleServer */
    unsigned long eligibleRank;      /* address-rank for eligibleServer */
    struct sockaddr_in eligibleAddr; /* address for eligibleServer */

#ifdef SGIMIPS 
    long earliestIx;		 /* server w/earliest transient vol err time */
    long earliestTime;	 	/* vol err time for earliestIx */
#else  /* SGIMIPS */
    int earliestIx;		 /* server w/earliest transient vol err time */
    unsigned long earliestTime;	 /* vol err time for earliestIx */
#endif /* SGIMIPS */

    unsigned long softestErrType; /* 0 OK, 1 transient, 2 persistent, 3 init */
    unsigned long softestErrCode; /* vol err classified as softestErrType */

    struct cm_conn *connp;
    int maxServers, fatalProblem;


    /* Essentially a wrapper for cm_ConnByAddr() which implements
     * server/server-address selection and retry logic for connection
     * to a set of servers.
     */

    icl_Trace1(cm_iclSetp, CM_TRACE_CONNBYMHOSTS,
	       ICL_TYPE_LONG, service);

    if (DFS_SRVTYPE(service) == SRT_FX || DFS_SRVTYPE(service) == SRT_REP) {
	/* Volume information must be supplied for FX/REP servers */
	osi_assert(volp != NULL);
	cellp = NULL;

	maxServers = CM_VL_MAX_HOSTS;
    } else {
	/* Cell information must be supplied for FL servers */
	osi_assert(cellp != NULL);
	volp = NULL;

	maxServers = MAXVLHOSTSPERCELL;
    }

    if (srvixp) {
	/* Initialize connected-server index to garbage */
	*srvixp = -1;
    }

    /* Check if last server contacted from set is still known to be eligible.
     * If so, try to get a connection to it without further ado.
     * The last server contacted is still eligible if:
     *    1) the associated hostGen has not changed (same server-set), and
     *    2) the global cm_siteAddrGen has not changed (same addr info), and
     *    3) no time-out for RTT/server-state re-evaluation, and
     *    4) the server is not down, and
     *    5) the volume is OK (SRT_FX/REP).
     */

    now = osi_Time();
    eligibleServer = -1;

    if (volp) {
	/* SRT_FX or SRT_REP */
	lock_ObtainWrite(&volp->lock);

	i = volp->lastIdx;

	if (tserverp = serverp[i]) {
	    lock_ObtainRead(&tserverp->lock);
	    lock_ObtainRead(&cm_siteLock);

	    if (volp->lastIdxTimeout >  now &&
		volp->lastIdxHostGen == volp->hostGen &&
		volp->lastIdxAddrGen == cm_siteAddrGen) {
		/* lastIdx valid; check if server is up and vol is OK */

		if (!(tserverp->states & SR_DOWN)) {
		    /* server OK; check vol */
#ifdef SGIMIPS
		    /* 
		     * Check to see if timeBad is set for this server in the
		     * volume struct.  However allow the TSR thread through
		     * if timeBad was set due to a  transient error 
		     * ( CM_ERR_TRANS_CLIENTTSR) caused by other people
		     * trying to get tokens for this same vol/server.
		     * We can't stop people from attempting to get tokens
	 	     * but we should not hold up the TSR thread because of
		     * it either.
		     */
		    if ((volp->timeBad[i] <= (now - CM_RREQ_SLEEP)) ||
			((tserverp->states & SR_DOINGSETCONTEXT) &&
			(tserverp->setCtxProcID == osi_ThreadUnique()) &&
			(volp->perSrvReason[i] == 
			(CM_ERR_TRANS_CLIENTTSR - VOLERR_PERS_LOWEST))))
#else  /* SGIMIPS */
		    if (volp->timeBad[i] <= (now - CM_RREQ_SLEEP)) 
#endif /* SGIMIPS */
		    {
			/* OK to try this volume */
			volp->timeBad[i] = 0;
		    }
		    if ((volp->timeBad[i] == 0) ||
			(cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOVOLWAIT)) {
			/* volume OK or no wait; prepare to attempt conn */
			eligibleServer = i;
			eligibleAddr   = tserverp->sitep->addrCurp->addr;
		    }
		}
	    }
	    lock_ReleaseRead(&cm_siteLock);
	    lock_ReleaseRead(&tserverp->lock);
	}
	lock_ReleaseWrite(&volp->lock);

    } else {
	/* SRT_FL */
	lock_ObtainRead(&cellp->lock);

	i = cellp->lastIdx;

	if (tserverp = serverp[i]) {
	    lock_ObtainRead(&tserverp->lock);
	    lock_ObtainRead(&cm_siteLock);

	    if (cellp->lastIdxTimeout >  now &&
		cellp->lastIdxHostGen == cellp->hostGen &&
		cellp->lastIdxAddrGen == cm_siteAddrGen) {
		/* lastIdx valid; check if server is up */

		if (!(tserverp->states & SR_DOWN)) {
		    /* server OK; prepare to attempt conn */
		    eligibleServer = i;
		    eligibleAddr   = tserverp->sitep->addrCurp->addr;
		}
	    }
	    lock_ReleaseRead(&cm_siteLock);
	    lock_ReleaseRead(&tserverp->lock);
	}
	lock_ReleaseRead(&cellp->lock);
    }

    if (eligibleServer >= 0) {
	/* last server contacted from set is eligible; attempt connection */
	icl_Trace1(cm_iclSetp, CM_TRACE_CONNBYMHOSTS_LASTHIT,
		   ICL_TYPE_LONG, service);

	if (connp = cm_ConnByAddr(&eligibleAddr,
				  tserverp, service, cellIdp, rreqp, volp, 0,
				  &fatalProblem)) {
	    icl_Trace4(cm_iclSetp, CM_TRACE_CONNBYMHOSTS_SUCCESS,
		       ICL_TYPE_LONG, service,
		       ICL_TYPE_LONG, SR_ADDR_VAL(&eligibleAddr),
		       ICL_TYPE_LONG, SR_ADDR_VAL(&connp->addr),
		       ICL_TYPE_POINTER, connp);
	    if (srvixp) {
		*srvixp = eligibleServer;
	    }
	    return connp;
	} else {
	    icl_Trace1(cm_iclSetp, CM_TRACE_CONNBYHOSTFAILED,
		       ICL_TYPE_LONG, fatalProblem);

	    /* if cm_ConnByAddr() fails fatally, give up; else continue */
#ifdef SGIMIPS
	    if (fatalProblem)
#else  /* SGIMIPS */
	    if (fatalProblem || rreqp->tsrError)
#endif /* SGIMIPS */
	    {
		/* cm_ConnByAddr() already set a rreqp error */
		return (struct cm_conn *) 0;
	    } else {
		/* always sleep at least 1 sec before another conn attempt */
		CM_SLEEP(1);
	    }
	}
    }


    /* Search server-set, trying servers in address-rank order.
     * Note that it is only necessary to examine the rank of the
     * current address of each server; i.e. the best-ranking up address.
     */

    for (;;) {
	/* keep looping if all vols are busy/early (transient errors) */

	now = osi_Time();

	/* no server selected; no transient/persistent vol errors seen yet */
	eligibleServer = earliestIx = -1;

	softestErrType = 3;
	softestErrCode = 0;

	/* save server/address-generation counts to detect updates to
	 * server-set or server-addresses/ranks
	 */
	if (volp) {
	    /* SRT_FX or SRT_REP */
	    lock_ObtainRead(&volp->lock);
	    lock_ObtainRead(&cm_siteLock);

	    hostGen = volp->hostGen;
	    addrGen = cm_siteAddrGen;

	    lock_ReleaseRead(&cm_siteLock);
	    lock_ReleaseRead(&volp->lock);
	} else {
	    /* SRT_FL */
	    lock_ObtainRead(&cellp->lock);
	    lock_ObtainRead(&cm_siteLock);

	    hostGen = cellp->hostGen;
	    addrGen = cm_siteAddrGen;

	    lock_ReleaseRead(&cm_siteLock);
	    lock_ReleaseRead(&cellp->lock);
	}

	/* iterate through server-set and choose best */

	for (i = 0; i < maxServers; i++) {
	    /* set next server to look at */
	    if (volp) {
		/* SRT_FX or SRT_REP */
		lock_ObtainRead(&volp->lock);
		tserverp = serverp[i];
		lock_ReleaseRead(&volp->lock);
	    } else {
		/* SRT_FL */
		lock_ObtainRead(&cellp->lock);
		tserverp = serverp[i];
		lock_ReleaseRead(&cellp->lock);
	    }

	    if (!tserverp) {
		/* no more servers to look at */
		break;
	    }

	    /* check that server is not currently marked down */
	    lock_ObtainRead(&tserverp->lock);
	    if (tserverp->states & SR_DOWN)  {
		lock_ReleaseRead(&tserverp->lock);
		icl_Trace1(cm_iclSetp, CM_TRACE_FOUNDSERVERDOWN,
			   ICL_TYPE_LONG, i);
		continue;  /* try another server */
	    }
	    lock_ReleaseRead(&tserverp->lock);

	    /* if SRT_FX/SRT_REP, check for volume error */
	    if (volp) {
		lock_ObtainWrite(&volp->lock);

		/* check that server-set has not been updated */
		if (hostGen != volp->hostGen) {
		    /* server-set updated, start all over */
		    icl_Trace2(cm_iclSetp, CM_TRACE_GENCNTCHANGED,
			       ICL_TYPE_LONG, hostGen,
			       ICL_TYPE_LONG, volp->hostGen);
		    icl_Trace3(cm_iclSetp, CM_TRACE_OLDNEWSERVER,
			       ICL_TYPE_LONG, i,
			       ICL_TYPE_POINTER, tserverp,
			       ICL_TYPE_POINTER, serverp[i]);
		    lock_ReleaseWrite(&volp->lock);
		    CM_SLEEP(1);
		    goto startOver;
		}
		osi_assert(tserverp == serverp[i]);

		/* check if can reset a volume error */
#ifdef SGIMIPS
	        lock_ObtainRead(&tserverp->lock);
		/* 
		 * Check to see if timeBad is set for this server in the
		 * volume struct.  However allow the TSR thread through
		 * if timeBad was set due to a  transient error 
		 * ( CM_ERR_TRANS_CLIENTTSR ) caused by other people
		 * trying to get tokens for this same vol/server.
		 * We can't stop people from attempting to get tokens
	 	 * but we should not hold up the TSR thread because of
		 * it either.
		 */
		if ((volp->timeBad[i] <= (now - CM_RREQ_SLEEP)) ||
		    ((tserverp->states & SR_DOINGSETCONTEXT) &&
		    (tserverp->setCtxProcID == osi_ThreadUnique()) &&
		    (volp->perSrvReason[i] == 
		    (CM_ERR_TRANS_CLIENTTSR - VOLERR_PERS_LOWEST)))) {
		    volp->timeBad[i] = 0;  /* OK to try this volume */
		}
		lock_ReleaseRead(&tserverp->lock);
#else  /* SGIMIPS */
		if (volp->timeBad[i] <= (now - CM_RREQ_SLEEP)) {
		    volp->timeBad[i] = 0;  /* OK to try this volume */
		}
#endif /* SGIMIPS */

                /* classify volume error, if any; track "softest" error
		 * seen and "earliest" transient error seen.
		 */
		if (volp->timeBad[i] == 0) {
		    /* volume OK; indicate that found vol to try */
		    softestErrType = 0;
		    softestErrCode = 0;

		    lock_ReleaseWrite(&volp->lock);
		} else {
		    /* volume not OK; classify error and try another server */
		    code = volp->perSrvReason[i] + VOLERR_PERS_LOWEST;

		    if ((code >= VOLERR_TRANS_LOWEST) &&
			(code <= CM_ERR_TRANS_HIGHEST)) {
			/* volume error is transient */
			if (softestErrType > 1) {
			    softestErrType = 1;
			    softestErrCode = code;
			}

			if ((earliestIx < 0) ||
			    (volp->timeBad[i] < earliestTime)) {
			    /* track transient error with earliest time */
			    earliestIx   = i;
			    earliestTime = volp->timeBad[i];
			}
		    } else {
			/* volume error is persistent */
			if (softestErrType == 3) {
			    softestErrType = 2;
			    softestErrCode = code;
			}
		    }
		    icl_Trace4(cm_iclSetp, CM_TRACE_TIMEBAD,
			       ICL_TYPE_LONG, i,
			       ICL_TYPE_LONG, volp->timeBad[i],
			       ICL_TYPE_LONG, earliestIx,
			       ICL_TYPE_LONG, softestErrType);

		    lock_ReleaseWrite(&volp->lock);
		    continue;	/* try another server */
		}
	    } /* if (volp) */


	    /* We know of nothing wrong with this server/vol.  Compare it to
	     * the best-so-far; within an equivalence class of server-address
             * ranks, use the server RTT as a tie-breaker.
	     */
	    lock_ObtainRead(&tserverp->lock);
	    lock_ObtainRead(&cm_siteLock);

	    if ((eligibleServer < 0) ||                              /* (1) */

		(tserverp->sitep->addrCurp->rank < eligibleRank) ||  /* (2) */

		(tserverp->sitep->addrCurp->rank == eligibleRank &&  /* (3) */
		 tserverp->avgRtt < eligibleRtt)) {
		/*
		 * (1) first server selected, or
		 * (2) current server (addr) has rank lower than previous, or
		 * (3) current server (addr) has rank equal to previous but
                 *     a lower RTT.
		 */
		eligibleServer = i;
		eligibleRtt    = tserverp->avgRtt;
		eligibleRank   = tserverp->sitep->addrCurp->rank;
	    }

	    lock_ReleaseRead(&cm_siteLock);
	    lock_ReleaseRead(&tserverp->lock);
	} /* for (i = 0; i < maxServers; i++) */


	/* Have either made a server/address choice, or have found none to
	 * be eligible at this point in time.
	 * For NOVOLWAIT, if no vol is eligible now then try one with earliest
	 * transient-error time, if any.
	 */

	i = eligibleServer;

	if (i < 0 && (cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOVOLWAIT)) {
	    i = earliestIx;
	}

	if (i >= 0) {
	    /* Have made a server/address choice */

	    /* Set server/address; check that server-set and server-addresses
	     * have not changed
	     */
	    if (volp) {
		/* SRT_FX or SRT_REP */
		lock_ObtainRead(&volp->lock);

		if (tserverp = serverp[i]) {
		    lock_ObtainRead(&cm_siteLock);

		    eligibleAddr = tserverp->sitep->addrCurp->addr;
		    code = (hostGen == volp->hostGen &&
			    addrGen == cm_siteAddrGen);

		    lock_ReleaseRead(&cm_siteLock);
		}
		lock_ReleaseRead(&volp->lock);

	    } else {
		/* SRT_FL */
		lock_ObtainRead(&cellp->lock);

		if (tserverp = serverp[i]) {
		    lock_ObtainRead(&cm_siteLock);

		    eligibleAddr = tserverp->sitep->addrCurp->addr;
		    code = (hostGen == cellp->hostGen &&
			    addrGen == cm_siteAddrGen);

		    lock_ReleaseRead(&cm_siteLock);
		}
		lock_ReleaseRead(&cellp->lock);
	    }

	    if (!tserverp || !code) {
		/* server-set or server-addresses updated, must start over */
		icl_Trace0(cm_iclSetp, CM_TRACE_GENCHANGED_ADDRORHOST);
		CM_SLEEP(1);
	    } else {
		/* everything looks good; attempt to get connection */
		if (connp =
		    cm_ConnByAddr(&eligibleAddr,
				  tserverp, service, cellIdp, rreqp, volp, 0,
				  &fatalProblem)) {
		    icl_Trace4(cm_iclSetp, CM_TRACE_CONNBYMHOSTS_SUCCESS,
			       ICL_TYPE_LONG, service,
			       ICL_TYPE_LONG, SR_ADDR_VAL(&eligibleAddr),
			       ICL_TYPE_LONG, SR_ADDR_VAL(&connp->addr),
			       ICL_TYPE_POINTER, connp);

		    /* cache server/address selection information */

		    lock_ObtainRead(&tserverp->lock);
		    delayTime = (tserverp->hostLifeTime << 2) + 30;
		    lock_ReleaseRead(&tserverp->lock);

		    if (volp) {
			/* SRT_FX or SRT_REP */
			lock_ObtainWrite(&volp->lock);

			volp->lastIdx        = eligibleServer;
			volp->lastIdxHostGen = hostGen;
			volp->lastIdxAddrGen = addrGen;
			volp->lastIdxTimeout = now + delayTime;

			lock_ReleaseWrite(&volp->lock);
		    } else {
			/* SRT_FL */
			lock_ObtainWrite(&cellp->lock);

			cellp->lastIdx        = eligibleServer;
			cellp->lastIdxHostGen = hostGen;
			cellp->lastIdxAddrGen = addrGen;
			cellp->lastIdxTimeout = now + delayTime;

			lock_ReleaseWrite(&cellp->lock);
		    }

		    if (srvixp) {
			*srvixp = eligibleServer;
		    }
		    return connp;
		} else {
		    icl_Trace1(cm_iclSetp, CM_TRACE_CONNBYHOSTFAILED,
			       ICL_TYPE_LONG, fatalProblem);

		    /* If cm_ConnByAddr() tagged a fatalProblem, there's
		     * already an rreqp error picked; return failure now.
		     * If NOVOLWAIT is in force, return failure now as well.
		     * Otherwise, start selection process over again.
		     */
		    if (fatalProblem ||
#ifdef SGIMIPS
			(cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOVOLWAIT) ||
			rreqp->tsrError )
#else  /* SGIMIPS */
			(cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOVOLWAIT))
#endif /* SGIMIPS */
		    {
			return (struct cm_conn *) 0;
		    } else {
			CM_SLEEP(1);
		    }
		}
	    }

	} else {
	    /* No server was selected as ready for a try (yet) */

	    if (volp) {
		/* SRT_FX or SRT_REP */
		if (softestErrType == 2) {
		    /* best vol error was persistent (==> earliestIx < 0) */
		    cm_FinalizeReq(rreqp);

		    /* check which error it was that we saved */
		    if (softestErrCode == CM_ERR_PERS_CONNAUTH) {
			rreqp->accessError = 1;
		    } else {
			rreqp->volumeError = 1;
		    }
		}
	    }

	    /* Decide whether to return or to try again. */
	    if (volp == NULL || earliestIx < 0 ||
		(cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOVOLWAIT)) {
		/* SRT_FL or (SRT_FX/REP and no vol with a transient error or
		 * NOVOLWAIT)
		 */
		icl_Trace0(cm_iclSetp, CM_TRACE_FILESETSALLBAD);
		return (struct cm_conn *) 0;
	    }

#if 0
	    /* Check if timed out yet; if not initd, timeWentBusy is 0 */
	    if (rreqp->initd && rreqp->timeWentBusy != 0 &&
		(rreqp->timeWentBusy + CM_RREQ_TIMEOUT) < now) {
		icl_Trace1(cm_iclSetp, CM_TRACE_FILESETWENTBUSY,
			   ICL_TYPE_POINTER, volp);
		return (struct cm_conn *) 0;
	    }
#endif /* 0 */

	    /* Sleep till specified time past end of transient vol error */
	    icl_Trace2(cm_iclSetp, CM_TRACE_CBMREASON,
		       ICL_TYPE_LONG, volp->perSrvReason[earliestIx],
		       ICL_TYPE_POINTER, volp);

	    if ((delayTime = (earliestTime + CM_RREQ_SLEEP) - now) <= 0) {
		delayTime = 1;
	    }

#ifdef SGIMIPS
	    if (CM_SLEEP_INTR((long)delayTime))
#else  /* SGIMIPS */
	    if (CM_SLEEP_INTR(delayTime))
#endif /* SGIMIPS */
	    {
		cm_FinalizeReq(rreqp);
		rreqp->interruptedError = 1;
		icl_Trace0(cm_iclSetp, CM_TRACE_CONN_INTERRUPTED);
		return (struct cm_conn *) 0;
	    }
	}

	/* Start server/address selection process all over again */
startOver:;
    } /* for (;;) */
    /* NOTREACHED */
}



/*
 * Release conn's reference count
 */
#ifdef SGIMIPS
void cm_PutConn(register struct cm_conn *connp)
#else  /* SGIMIPS */
void cm_PutConn(connp)
    register struct cm_conn *connp;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&connp->lock);
    osi_assert(connp->callCount > 0);
    if (--connp->callCount == 0 && (connp->states & CN_CALLCOUNTWAIT)) {
	connp->states &= ~CN_CALLCOUNTWAIT;
	osi_Wakeup(connp);
    }
    lock_ReleaseWrite(&connp->lock);
    krpc_PoolFree(&cm_concurrentCallPool, 1);
    lock_ObtainWrite(&cm_connlock);
    osi_assert(connp->refCount > 0);
    connp->refCount--;
    /* if someone's sleeping, waiting for fewer active connections,
     * then wakeup anyone who's waiting for this server's conns.
     */
    if (cm_connSleepCounter > 0)
	osi_Wakeup(connp->serverp);
    lock_ReleaseWrite(&cm_connlock);
}

/*
 * Mark a particular pag's rpc connections in a given server obsolete.
 * If pag is -6 (CM_BADCONN_ALLCONNS), then all of connections within that
 * server are marked obsolete.
 */
#ifdef SGIMIPS
void cm_MarkBadConns(
    struct cm_server *serverp,
    unsigned32 pag)
#else  /* SGIMIPS */
void cm_MarkBadConns(serverp, pag)
    struct cm_server *serverp;
    unsigned32 pag;
#endif /* SGIMIPS */
{
    register struct cm_conn *connp;

    lock_ObtainWrite(&serverp->lock);
    icl_Trace1(cm_iclSetp, CM_TRACE_MARKCONNBAD, ICL_TYPE_LONG, pag);
    lock_ObtainWrite(&cm_connlock);
    for (connp = serverp->connsp; connp; connp = connp->next) {
	connp->refCount++;
	lock_ReleaseWrite(&cm_connlock);
	lock_ObtainWrite(&connp->lock);
	if ((connp->authIdentity[0] == pag) || (pag == CM_BADCONN_ALLCONNS)) {
	    connp->states |= CN_FORCECONN;
	}
	lock_ReleaseWrite(&connp->lock);
	lock_ObtainWrite(&cm_connlock);
	osi_assert(connp->refCount > 0);
	connp->refCount--;
    }
    lock_ReleaseWrite(&cm_connlock);
    lock_ReleaseWrite(&serverp->lock);
}

/*
 * Ensure all connections make use of new tokens.  Discard incorrectly-cached
 * access info.
 * It is called without any lock held.
 */
#ifdef SGIMIPS
void cm_ResetUserConns(unsigned32 pag)
#else  /* SGIMIPS */
void cm_ResetUserConns(pag)
    unsigned32 pag;
#endif /* SGIMIPS */
{
    register struct cm_server *serverp;
    register int i;

    icl_Trace1(cm_iclSetp, CM_TRACE_RESETUSERCONNS, ICL_TYPE_LONG, pag);
    lock_ObtainRead(&cm_serverlock);
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    /* we don't GC servers, so no ref count here */
	    lock_ReleaseRead(&cm_serverlock);
	    cm_MarkBadConns(serverp, pag);
	    lock_ObtainRead(&cm_serverlock);
	}
    }
    lock_ReleaseRead(&cm_serverlock);
    cm_ResetAccessCache(pag);
}


/*
 * GC old rpc connections for a given server (with a read serverlock held).
 * This function is called from cm_daemons
 */
#ifdef SGIMIPS
void cm_DeleteConns(struct cm_server *serverp)
#else  /* SGIMIPS */
void cm_DeleteConns(serverp)
    struct cm_server *serverp;
#endif /* SGIMIPS */
{
    register struct cm_conn **cpp, *connp;
    time_t now;

    now = osi_Time();

    lock_ObtainWrite(&serverp->lock);
    lock_ObtainWrite(&cm_connlock);
    for (cpp = &serverp->connsp; connp = *cpp;) {
	/*
	 * This is the strategy we use to GC old bindings:
	 * 1) the binding must not be referenced by anyone,
	 * 2) If it is an authenticated binding (ie., 'lifeTime' is
	 * non-zero), we then check whether the TGT expires or not.
	 */
	if (connp->refCount == 0 && ((connp->lifeTime + 60) < now)) {
	    /* Let's make this invisible first, so no one will access it */
	    connp->authIdentity[0] = CM_UNUSED_USERNUM;
	    connp->refCount++;
	    lock_ReleaseWrite(&cm_connlock);
	    icl_Trace1(cm_iclSetp, CM_TRACE_GCCONNS, ICL_TYPE_POINTER,
		       connp->callCount);
	    /* increase count of binding users */
	    lock_ObtainWrite(&connp->lock);
#ifdef SGIMIPS
	    connp->callCount++;
	    if (connp->connp) {
		/*
		 * Avoid sleeping in cm_StompRPCBinding.
		 */
		if (connp->callCount != 1) {
		    lock_ReleaseWrite(&connp->lock);
		    lock_ObtainWrite(&cm_connlock);
		    cpp = &connp->next;
		    continue;
		}
		cm_StompRPCBinding(connp);
	    }
#else  /* SGIMIPS */
	    connp->callCount++;
	    if (connp->connp)
	        cm_StompRPCBinding(connp);
#endif /* SGIMIPS */
	    connp->callCount--;
	    osi_assert(connp->callCount >= 0);
	    lock_ReleaseWrite(&connp->lock);
	    lock_ObtainWrite(&cm_connlock);
#ifdef SGIMIPS
            /*
             * Make sure the world hasn't changed in case we went to
             * sleep in cm_StompRPCBinding.
             */
            if (*cpp != connp) {
                /*
                 * Someone else modified the list whilst we didn't have
                 * the lock.  Go back to the beginning and start over.
                 */
                cpp = &serverp->connsp;
                continue;
            }
#endif /* SGIMIPS */
	    connp->refCount--;
	    osi_assert(connp->refCount == 0);
	    *cpp = connp->next;
	    connp->next = FreeConnList;
	    FreeConnList = connp;
        } else
	    cpp = &connp->next;
    }
    lock_ReleaseWrite(&cm_connlock);
    lock_ReleaseWrite(&serverp->lock);
}
/*
 * To get TGT's expiration time assocaited with the pag.
 * cm_tgtlock should be one of the lowest level locks.
 */
#ifdef SGIMIPS
time_t cm_TGTLifeTime(
    unsigned32 pag,
    int *expiredP)
#else  /* SGIMIPS */
long cm_TGTLifeTime(pag, expiredP)
    unsigned32 pag;
    int *expiredP;
#endif /* SGIMIPS */
{
    time_t time = 0;
    int expired = CM_TGTEXP_NOTGT;

    struct cm_tgt *tgtp;
    lock_ObtainRead(&cm_tgtlock);
    for (tgtp = cm_tgtList; tgtp; tgtp = tgtp->next) {
	if (tgtp->pag == pag) {
	    if (tgtp->tgtTime <= osi_Time()) {
		time = 0;	/* return zero if tgt expires */
		tgtp->tgtFlags |= CM_TGT_EXPIRED;
		expired = CM_TGTEXP_PAGEXPIRED;
	    } else {
		time = tgtp->tgtTime;
		expired = (tgtp->tgtFlags & CM_TGT_EXPIRED)
		  ? CM_TGTEXP_PAGEXPIRED : CM_TGTEXP_NOTEXPIRED;
	    }
	    break;
	}
    }
    lock_ReleaseRead(&cm_tgtlock);
    icl_Trace3(cm_iclSetp, CM_TRACE_GETTGTTIME2,
	       ICL_TYPE_LONG, pag,
	       ICL_TYPE_LONG, time,
	       ICL_TYPE_LONG, expired);
    *expiredP = expired;
    return time;	/* time = 0, if cannot find its tgt */
}

/* Mark this PAG as having expired authentication. */
#ifdef SGIMIPS
void cm_MarkExpired(unsigned32 pag)
#else  /* SGIMIPS */
int cm_MarkExpired(pag)
#endif /* SGIMIPS */
{
    struct cm_tgt *tgtp;
#ifndef SGIMIPS
    /* not used for anything.  Get rid of it to silence the compiler */
    int result = 0;
#endif /* SGIMIPS */

    icl_Trace1(cm_iclSetp, CM_TRACE_MARKEXPIRED, ICL_TYPE_LONG, pag);
    lock_ObtainWrite(&cm_tgtlock);
    for (tgtp = cm_tgtList; tgtp; tgtp = tgtp->next) {
        if (tgtp->pag == pag) {
	    tgtp->tgtFlags |= CM_TGT_EXPIRED;
#ifndef SGIMIPS 
	    /* silence the compiler */
	    result = 1;
#endif /* SGIMIPS */
	    break;
	}
    }
    lock_ReleaseWrite(&cm_tgtlock);
}



/*
 * cm_ReactToBindAddrChange() -- check to see that the address listed in
 *     the conn structure matches that in the corresponding binding.
 *
 *     If the addresses do *not* match then:
 *       - reset the conn address to that of the binding, and
 *       - if the doOverride flag is set, override the current best-ranking
 *         address for the server in favor of the binding address.
 *
 *     This routine provides a mechanism for responding to a conflicting
 *     transport-layer routing decision, which is observed as a change in
 *     a RPC binding address.  In this case, we defer to the wisdom of the
 *     transport-layer.
 * 
 * LOCKS USED: locks connp->lock; released on exit.
 *
 * RETURN CODES: none
 */

void
cm_ReactToBindAddrChange(struct cm_conn *connp, int doOverride)
{
    unsigned char *strBind, *strBindAddr, strConnAddr[32];
    unsigned32 st;
    struct sockaddr_in bindAddr;
    int bindAddrWasCur;
    char preMsgBuf[40], postMsgBuf[80];

    /* Get actual binding-address */

    /* Note: if performance becomes a problem, we can go "under the covers" to
     *       get the address via rpc__binding_inq_sockaddr()
     */

    strBind = strBindAddr = NULL;

    lock_ObtainWrite(&connp->lock);

    rpc_binding_to_string_binding(connp->connp, &strBind, &st);

    if (st == rpc_s_ok) {
	rpc_string_binding_parse(strBind,       /* string binding */
				 NULL,          /* obj uuid */
				 NULL,          /* proto seq */
				 &strBindAddr,  /* network address */
				 NULL,          /* endpoint */
				 NULL,          /* options */
				 &st);
    }

#ifdef SGIMIPS
    if (st != rpc_s_ok || 
		osi_inet_aton((char *)strBindAddr, &bindAddr.sin_addr)) {
#else  /* SGIMIPS */
    if (st != rpc_s_ok || osi_inet_aton(strBindAddr, &bindAddr.sin_addr)) {
#endif /* SGIMIPS */
	/* can't get/parse addr; should never occur; log and mark conn bad */
	icl_Trace3(cm_iclSetp, CM_TRACE_REACTBINDCHANGE_ADDRERR,
		   ICL_TYPE_POINTER, connp,
		   ICL_TYPE_LONG, doOverride,
		   ICL_TYPE_LONG, st);

	connp->states |= CN_FORCECONN;
	lock_ReleaseWrite(&connp->lock);

	if (strBind) {
	    rpc_string_free(&strBind, &st);

	    if (strBindAddr) {
		rpc_string_free(&strBindAddr, &st);
	    }
	}
	return;
    }

    icl_Trace4(cm_iclSetp, CM_TRACE_REACTBINDCHANGE_ENTER,
	       ICL_TYPE_POINTER, connp,
	       ICL_TYPE_LONG, doOverride,
	       ICL_TYPE_LONG, SR_ADDR_VAL(&connp->addr),
	       ICL_TYPE_LONG, SR_ADDR_VAL(&bindAddr));

    /* Compare binding-address to original conn-address */

    if (!SR_ADDR_EQ(&connp->addr, &bindAddr)) {
	/* addrs are different; react as specified by doOverride */
	if (!doOverride) {
	    /* just reset conn address (keeping other fields) */
	    connp->addr.sin_addr.s_addr = SR_ADDR_VAL(&bindAddr);

	} else {
	    /* remember current conn addr */
#ifdef SGIMIPS
	    osi_inet_ntoa_buf(connp->addr.sin_addr, (char *)strConnAddr);
#else  /* SGIMIPS */
	    osi_inet_ntoa_buf(connp->addr.sin_addr, strConnAddr);
#endif /* SGIMIPS */

	    /* reset conn address (keeping other fields) */
	    connp->addr.sin_addr.s_addr = SR_ADDR_VAL(&bindAddr);

	    /* override preferred server address */
	    cm_SiteAddrRankOverride(connp->serverp->sitep,
				    &connp->addr, &bindAddrWasCur);

	    /* report override */
	    if (!bindAddrWasCur) {
		strcpy(preMsgBuf, "dfs: ");

		switch (connp->serverp->sType) {
		  case SRT_FL:
		    strcat(preMsgBuf, "fileset location server ");
		    break;

		  case SRT_REP:
		    strcat(preMsgBuf, "replication server ");
		    break;
	    
		  case SRT_FX:
		    strcat(preMsgBuf, "fx server ");
		    break;

		  default:
		    break;
		}

		sprintf(postMsgBuf,
			" - overriding preferred address %s in favor of %s\n",
			strConnAddr, strBindAddr);

		cm_PrintServerText(preMsgBuf, connp->serverp, postMsgBuf);
	    }
	}
    }

    lock_ReleaseWrite(&connp->lock);

    rpc_string_free(&strBindAddr, &st);
    rpc_string_free(&strBind, &st);
}
