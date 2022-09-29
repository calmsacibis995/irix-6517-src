/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_rrequest.c,v 65.8 1999/08/24 20:05:41 gwehrman Exp $";
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


#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/sysincludes.h>		/* Standard vendor system headers */
#include <dcedfs/fshs_errs.h>		/* errors from fshost */
#include <dcedfs/vol_errs.h>		/* errors from fileset ops */
#include <dcedfs/tkm_errs.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/rep_errs.h>
#include <dcedfs/nubik.h>
#include <cm.h>
#include <cm_conn.h>
#include <cm_cell.h>
#include <cm_volume.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_rrequest.c,v 65.8 1999/08/24 20:05:41 gwehrman Exp $")

/*
 * Initialize an rrequest structure that will be used for
 * default/authenticated access.
 */
#ifdef SGIMIPS
void cm_InitReq(
    register struct cm_rrequest *rreqp,
    osi_cred_t *credp) 
#else  /* SGIMIPS */
void cm_InitReq(rreqp, credp)
    register struct cm_rrequest *rreqp;
    osi_cred_t *credp; 
#endif /* SGIMIPS */
{
    rreqp->uid = osi_GetRUID(credp);	/* get effective user id */
    rreqp->pag = osi_GetPagFromCred(credp);
    rreqp->serverp = (struct cm_server *) 0;
    /*
     * If there's no PAG involved here, simply allow the UID to serve
     * as the request ID.  osi_credp might be a real credential that
     * should really be authenticated with the self credential over
     * in cm_ConnByHost().
     */
    if (rreqp->pag == OSI_NOPAG)
	rreqp->pag = rreqp->uid; /* default when no pag is set */
    rreqp->connFlags = 0;
    rreqp->initd = 0;
}

/*
 * Initialize an unauthenticated cm_rrequest structure.
 */
#ifdef SGIMIPS
void cm_InitUnauthReq(register struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
void cm_InitUnauthReq(rreqp)
    register struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    rreqp->pag = rreqp->uid = CM_UNAUTH_USERNUM; /* -2 */
    rreqp->serverp = (struct cm_server *) 0;
    rreqp->connFlags = 0;
    rreqp->initd = 0;
}


/*
 * Initialize a cm_rrequest structure to be devoid of any
 * type of DCE RPC authentication.
 */
#ifdef SGIMIPS
void cm_InitNoauthReq(register struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
void cm_InitNoauthReq(rreqp)
    register struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    rreqp->pag = rreqp->uid = CM_UNAUTH_USERNUM; /* -2 but NOAUTH flag */
    rreqp->serverp = (struct cm_server *) 0;
    rreqp->connFlags = CM_RREQFLAG_NOAUTH;
    rreqp->initd = 0;
}



#ifdef SGIMIPS
void cm_FinalizeReq(register struct cm_rrequest *rreqp) 
#else  /* SGIMIPS */
void cm_FinalizeReq(rreqp)
    register struct cm_rrequest *rreqp; 
#endif /* SGIMIPS */
{
    if (rreqp->initd) 
	return;
#if 0
    rreqp->timeWentBusy = 0;
#endif /* 0 */
    rreqp->accessError = 0;
    rreqp->volumeError = 0;
    rreqp->networkError = 0;
    rreqp->readOnlyError = 0;
    rreqp->interruptedError = 0;
#ifdef SGIMIPS
    rreqp->tsrError = 0;
#endif /* SGIMIPS */
    rreqp->initd = 1;
}


#ifdef SGIMIPS
void cm_CopyError(
     register struct cm_rrequest *frreqp,
     register struct cm_rrequest *trreqp) 
#else  /* SGIMIPS */
void cm_CopyError(frreqp, trreqp)
     register struct cm_rrequest *frreqp, *trreqp; 
#endif /* SGIMIPS */
{
    if (!frreqp->initd) 
	return;
    cm_FinalizeReq(trreqp);
    if (frreqp->accessError) 
	trreqp->accessError = 1;
    if (frreqp->volumeError) 
	trreqp->volumeError = 1;
    if (frreqp->networkError) 
	trreqp->networkError = 1;
    if (frreqp->readOnlyError)
	trreqp->readOnlyError = 1;
    if (frreqp->interruptedError)
	trreqp->interruptedError = 1;
#ifdef SGIMIPS
    if (frreqp->tsrError)
	trreqp->tsrError = 1;
#endif /* SGIMIPS */
    return;
}


#if 0
#define CM_LOGERRORS	20
long cm_logerrors[CM_LOGERRORS];
long cm_logerrorcount;
void cm_LogError(aec)
register long aec; {
    register int i, j;
    if (aec == 0) return;
    for(i=0;i<CM_LOGERRORS;i++) {
	if ((j=cm_logerrors[i]) == 0) break;
	if (j == aec) return;
    }
    cm_logerrorcount++;
}
#endif /* 0 */
/* 
 * To covert the DFS error to local host error code
 */
#ifdef SGIMIPS
int
cm_CheckError(
    register long code,
    register struct cm_rrequest *rreqp) 
#else  /* SGIMIPS */
cm_CheckError(code, rreqp)
    register long code;
    register struct cm_rrequest *rreqp; 
#endif /* SGIMIPS */
{
    cm_FinalizeReq(rreqp);
    if (code != 0) {
	if (rreqp->networkError) 
	    code = ETIMEDOUT;
	else if (rreqp->accessError) 
	    code = EACCES;
	else if (rreqp->volumeError) 
	    code = ENODEV;
	else if (rreqp->readOnlyError)
	    code = EROFS;
	else if (rreqp->interruptedError)
	    code = EINTR;
    }
#ifdef SGIMIPS
    /*  Call it a pointer because we want 64 bits instead of 32 bits.  */
    icl_Trace1(cm_iclSetp, CM_TRACE_CHECKERROR, ICL_TYPE_64BITLONG, code);

    return (int)code;
#else  /* SGIMIPS */
    icl_Trace1(cm_iclSetp, CM_TRACE_CHECKERROR, ICL_TYPE_LONG, code);

    return code;
#endif /* SGIMIPS */

}

#ifdef SGIMIPS
int cm_ReactToAuthCodes(
    struct cm_conn *connp,
    struct cm_volume *volp,
    struct cm_rrequest *rreqp,
    long code)
#else  /* SGIMIPS */
int cm_ReactToAuthCodes(connp, volp, rreqp, code)
struct cm_conn *connp;
struct cm_volume *volp;
struct cm_rrequest *rreqp;
long code;
#endif /* SGIMIPS */
{
    struct cm_server *serverp = connp->serverp;
    int reTry, oldAuthn;

    switch (code) {
	case rpc_s_unsupported_protect_level:
	case FSHS_ERR_AUTHNLEVEL_S_TOO_HIGH:
	    /* Authentication level too high for this fileserver */
	    lock_ObtainWrite(&serverp->lock);
	    lock_ObtainRead(&connp->lock);
	    oldAuthn = serverp->maxAuthn;
	    if (connp->currAuthn > CM_MIN_SECURITY_LEVEL) {
		/* rpc_s_... is a condition that won't get better for this server, ever */
		if (code == rpc_s_unsupported_protect_level
		    && ((int)serverp->maxSuppAuthn) > (((int)connp->currAuthn)-1)) {
		    icl_Trace3(cm_iclSetp, CM_TRACE_ANA_DOWNMAXSUPP,
			       ICL_TYPE_LONG, serverp->maxSuppAuthn,
			       ICL_TYPE_LONG, connp->currAuthn-1,
			       ICL_TYPE_LONG, serverp->minAuthn);
		    serverp->maxSuppAuthn = connp->currAuthn-1;
		}
		if (((int)serverp->maxAuthn) > (((int)connp->currAuthn)-1))
		    serverp->maxAuthn = connp->currAuthn-1;
		reTry = 1;
		/* Push server lower-bound down in case it moved. */
		if (serverp->maxAuthn < serverp->minAuthn)
		    serverp->minAuthn = serverp->maxAuthn;
	    } else {
		rreqp->accessError = 1;
		reTry = 0;
	    }
	    icl_Trace3(cm_iclSetp, CM_TRACE_AUTHN_DINKING,
		       ICL_TYPE_POINTER, connp,
		       ICL_TYPE_LONG, connp->currAuthn,
		       ICL_TYPE_LONG, connp->currAuthz);
	    icl_Trace4(cm_iclSetp, CM_TRACE_SERVER_HIGH_AUTHN,
		       ICL_TYPE_POINTER, serverp,
		       ICL_TYPE_LONG, serverp->minAuthn,
		       ICL_TYPE_LONG, oldAuthn,
		       ICL_TYPE_LONG, serverp->maxAuthn);
	    lock_ReleaseRead(&connp->lock);
	    lock_ReleaseWrite(&serverp->lock);
	    break;

	case FSHS_ERR_AUTHNLEVEL_S_TOO_LOW:
	    /* Authentication level too low for this fileserver */
	    lock_ObtainWrite(&serverp->lock);
	    lock_ObtainRead(&connp->lock);
	    oldAuthn = serverp->minAuthn;
	    if (connp->currAuthn < CM_MAX_SECURITY_LEVEL) {
		serverp->minAuthn = connp->currAuthn+1;
		reTry = 1;
		if (serverp->maxAuthn < serverp->minAuthn) {
		    /* Want to push the UB up, in case it changed, but
		     * it can never go over maxSuppAuthn since we
		     * know that we'll always get unsupported_protect_level
		     * if we try that.
		     */
		    if (serverp->minAuthn > serverp->maxSuppAuthn) {
			/* No can do; can't push maxAuthn past maxSuppAuthn. */
			rreqp->accessError = 1;
			reTry = 0;
			icl_Trace4(cm_iclSetp, CM_TRACE_ANA_HARDUB,
				   ICL_TYPE_LONG, oldAuthn,
				   ICL_TYPE_LONG, serverp->minAuthn,
				   ICL_TYPE_LONG, serverp->maxAuthn,
				   ICL_TYPE_LONG, serverp->maxSuppAuthn);
		    } else {
			serverp->maxAuthn = serverp->minAuthn;
		    }
		}
	    } else {
		rreqp->accessError = 1;
		reTry = 0;
	    }
	    /* Clear this in case the server disagrees that NOAUTH is OK */
	    cm_ClearConnFlags(rreqp, CM_RREQFLAG_NOAUTH);
	    icl_Trace3(cm_iclSetp, CM_TRACE_AUTHN_DINKING,
		       ICL_TYPE_POINTER, connp,
		       ICL_TYPE_LONG, connp->currAuthn,
		       ICL_TYPE_LONG, connp->currAuthz);
	    icl_Trace4(cm_iclSetp, CM_TRACE_SERVER_LOW_AUTHN,
		       ICL_TYPE_POINTER, serverp,
		       ICL_TYPE_LONG, oldAuthn,
		       ICL_TYPE_LONG, serverp->maxAuthn,
		       ICL_TYPE_LONG, serverp->minAuthn);
	    lock_ReleaseRead(&connp->lock);
	    lock_ReleaseWrite(&serverp->lock);
	    break;

	case FSHS_ERR_AUTHNLEVEL_F_TOO_HIGH:
	    /* Authentication level too high for this fileset */
	    if (volp) {
		lock_ObtainWrite(&volp->lock);
		lock_ObtainRead(&connp->lock);
		oldAuthn = volp->maxRespAuthn;
		if (connp->currAuthn > CM_MIN_SECURITY_LEVEL) {
		    volp->maxRespAuthn = connp->currAuthn-1;
		    reTry = 1;
		    if (volp->maxRespAuthn < volp->minRespAuthn)
			volp->minRespAuthn = volp->maxRespAuthn;
		} else {
		    rreqp->accessError = 1;
		    reTry = 0;
		}
		icl_Trace3(cm_iclSetp, CM_TRACE_AUTHN_DINKING,
			   ICL_TYPE_POINTER, connp,
			   ICL_TYPE_LONG, connp->currAuthn,
			   ICL_TYPE_LONG, connp->currAuthz);
		icl_Trace4(cm_iclSetp, CM_TRACE_VOLUME_HIGH_AUTHN,
			   ICL_TYPE_POINTER, volp,
			   ICL_TYPE_LONG, volp->minRespAuthn,
			   ICL_TYPE_LONG, oldAuthn,
			   ICL_TYPE_LONG, volp->maxRespAuthn);
		lock_ReleaseRead(&connp->lock);
		lock_ReleaseWrite(&volp->lock);
	    } else
		reTry = 0;
	    break;

	case FSHS_ERR_AUTHNLEVEL_F_TOO_LOW:
	    /* Authentication level too low for this fileset */
	    if (volp) {
		lock_ObtainWrite(&volp->lock);
		lock_ObtainRead(&connp->lock);
		oldAuthn = volp->minRespAuthn;
		if (connp->currAuthn < CM_MAX_SECURITY_LEVEL) {
		    volp->minRespAuthn = connp->currAuthn+1;
		    reTry = 1;
		    if (volp->maxRespAuthn < volp->minRespAuthn)
			volp->maxRespAuthn = volp->minRespAuthn;
		} else {
		    rreqp->accessError = 1;
		    reTry = 0;
		}
		/* Clear this in case the server disagrees that NOAUTH is OK */
		cm_ClearConnFlags(rreqp, CM_RREQFLAG_NOAUTH);
		icl_Trace3(cm_iclSetp, CM_TRACE_AUTHN_DINKING,
			   ICL_TYPE_POINTER, connp,
			   ICL_TYPE_LONG, connp->currAuthn,
			   ICL_TYPE_LONG, connp->currAuthz);
		icl_Trace4(cm_iclSetp, CM_TRACE_VOLUME_LOW_AUTHN,
			   ICL_TYPE_POINTER, volp,
			   ICL_TYPE_LONG, oldAuthn,
			   ICL_TYPE_LONG, volp->maxRespAuthn,
			   ICL_TYPE_LONG, volp->minRespAuthn);
		lock_ReleaseRead(&connp->lock);
		lock_ReleaseWrite(&volp->lock);
	    } else
		reTry = 0;
	    break;

	default:
	    reTry = 2;
	    break;
    }
    return reTry;
}

#ifdef SGIMIPS
static void cm_ShowServerRecoveryReason(
    struct cm_server *aserverp,
    struct cm_volume *avolp,
    int subCode)
#else  /* SGIMIPS */
static void cm_ShowServerRecoveryReason(aserverp, avolp, subCode)
struct cm_server *aserverp;
struct cm_volume *avolp;
int subCode;
#endif /* SGIMIPS */
{/* provide hint that some async processes have finished */
    /* ``dfs: fileset (4294967295,,4294967295) is no longer busy with code 4294967295 on server '' */
    char msgbuf[100];

    if (subCode == (CM_ERR_TRANS_SERVERTSR - VOLERR_PERS_LOWEST))
	cm_PrintServerText("dfs: server ", aserverp,
			   " has finished with token recovery from clients.\n");
    else if (subCode == (CM_ERR_TRANS_CLIENTTSR - VOLERR_PERS_LOWEST))
	cm_PrintServerText("dfs: finished recovering tokens on server ",
			   aserverp, ".\n");
    else {
	strcpy(msgbuf, "dfs: fileset (");
	osi_cv2string(AFS_hgethi(avolp->volume),
		      &msgbuf[sizeof "dfs: fileset (" - 1]);
	strcat(msgbuf,",,");
	osi_cv2string(AFS_hgetlo(avolp->volume), msgbuf + strlen(msgbuf));
	strcat(msgbuf,") ");
	if (subCode == (VOLERR_TRANS_VVOLD - VOLERR_PERS_LOWEST))
	    strcat(msgbuf, "is now recent enough");
	else if (subCode == (CM_ERR_TRANS_MOVE_TSR - VOLERR_PERS_LOWEST))
	    strcat(msgbuf, "has reestablished tokens after a move");
	else if (subCode == (VOLERR_PERS_CLONECLEAN - VOLERR_PERS_LOWEST))
	    strcat(msgbuf, "has completed clone cleanup");
	else if (subCode == (VOLERR_PERS_OUTOFSERVICE - VOLERR_PERS_LOWEST))
	    strcat(msgbuf, "has completed move cleanup");
	else if (subCode == (VOLERR_PERS_DAMAGED - VOLERR_PERS_LOWEST))
	    strcat(msgbuf, "is no longer inconsistent");
	else if (subCode == (VOLERR_PERS_DELETED - VOLERR_PERS_LOWEST))
	    strcat(msgbuf, "is now exported");
	else if (subCode == (CM_ERR_PERS_CONNAUTH - VOLERR_PERS_LOWEST))
	    strcat(msgbuf, "has reestablished authentication");
	else {
	    if ((subCode >= (VOLERR_TRANS_LOWEST - VOLERR_PERS_LOWEST)) &&
		(subCode <= (CM_ERR_TRANS_HIGHEST - VOLERR_PERS_LOWEST))) {
		strcat(msgbuf, "is no longer busy with code ");
	    } else {
		strcat(msgbuf, "no longer shows error code ");
	    }
	    osi_cv2string(subCode+VOLERR_PERS_LOWEST,
			  msgbuf + strlen(msgbuf));
	}
	strcat(msgbuf, " on server ");
	cm_PrintServerText(msgbuf, aserverp, ".\n");
    }
}

/* Record a transition (good->bad, bad->bad, bad->good) in the CM's per-volume,
   per-server information. */
/* Return 0 if we found the server in the volume's server set but this error
   was already set, 1 if we found the server in the volume's server set and
   this error was different from what was set already (so that this call
   changed its state), or 2 if the given server wasn't in the volume's server
   set. */
#ifdef SGIMIPS
int cm_SetVolumeServerBadness(
    register struct cm_volume *volp,
    register struct cm_server *aserverp,
    long code, 
    long subCode)
#else  /* SGIMIPS */
int cm_SetVolumeServerBadness(volp, aserverp, code, subCode)
    register struct cm_volume *volp;
    register struct cm_server *aserverp;
    long code; 
    long subCode;
#endif /* SGIMIPS */
{
    register int ix;
    register struct cm_server *tserverp;
    register int val;

    lock_ObtainWrite(&volp->lock);
    for (ix = 0; ix < CM_VL_MAX_HOSTS; ++ix) {
	if ((tserverp = volp->serverHost[ix]) == (struct cm_server *) 0) 
	    break;
	if (tserverp == aserverp) {
	    val = 0;	/* Found the server.  Set val=1 if we change its info. */
	    if (code == 0) {
		if (volp->perSrvReason[ix] != subCode) {
		    val = 1;
		    /* provide hint that some async processes have finished */
		    cm_ShowServerRecoveryReason(aserverp, volp,
						volp->perSrvReason[ix]);
		}
		volp->timeBad[ix] = 0;
	    } else {
		if (volp->perSrvReason[ix] != subCode)
		    val = 1;
		volp->timeBad[ix] = osi_Time();
	    }
#ifdef SGIMIPS
	    volp->perSrvReason[ix] = (unsigned char)subCode;
#else  /* SGIMIPS */
	    volp->perSrvReason[ix] = subCode;
#endif /* SGIMIPS */
	    lock_ReleaseWrite(&volp->lock);
	    return val;
	}
    }
    lock_ReleaseWrite(&volp->lock);
    /* didn't find it in the volume's set of servers */
    return 2;
}

/* 
 * The connection should be held (by refCount) (cm_Conn locks it).  It will
 * be released when this routine is done. 
 */
#ifdef SGIMIPS
int cm_Analyze(
    register struct cm_conn *connp,
    long code,
    afsFid *fidp,
    register struct cm_rrequest *rreqp,
    register struct cm_volume *volp,	  /* optional (NULL) */
    long srvix,				/* optional (-1) */
    u_long startTime)
#else  /* SGIMIPS */
int cm_Analyze(connp, code, fidp, rreqp, volp, srvix, startTime)
    register struct cm_conn *connp;
    long code;
    register struct cm_rrequest *rreqp;
    afsFid *fidp;
    register struct cm_volume *volp;	  /* optional (NULL) */
    long srvix;				/* optional (-1) */
    u_long startTime;
#endif /* SGIMIPS */
{
    int reTry, reasonToPrint, setVolCode, gotNewLoc;
    struct cm_server *serverp;
    char msgbuf[200];
    long subCode;
    long vlCode;
    unsigned long oldstates;
    unsigned long Now;

    if (fidp) {
	icl_Trace4(cm_iclSetp, CM_TRACE_ANALYZEFID, ICL_TYPE_POINTER, connp,
		   ICL_TYPE_LONG, code, ICL_TYPE_LONG, rreqp->pag,
		   ICL_TYPE_FID, fidp);
    } else {
	icl_Trace3(cm_iclSetp, CM_TRACE_ANALYZE, ICL_TYPE_POINTER, connp,
		   ICL_TYPE_LONG, code, ICL_TYPE_LONG, rreqp->pag);
    }

    if (!connp) {		/* Could not get a connection */
	cm_FinalizeReq(rreqp);
#ifdef SGIMIPS
	if (rreqp->tsrError) return 0;
#endif /* SGIMIPS */
	if (!fidp || (rreqp->volumeError == 0 &&
		      rreqp->interruptedError == 0 &&
		      rreqp->accessError == 0))
	    rreqp->networkError = 1;
	return 0;		/* Fail if no connection. */
    }
    serverp = connp->serverp;		/* Server associated with this conn */

#if defined(KERNEL)
    /* React to a change in binding addr; syncs conn and binding addrs */
    cm_ReactToBindAddrChange(connp, 1 /* doOverride */);
#endif

    /* Record the last time a call to a replica was attempted. */
    if (volp != 0) {
	Now = osi_Time();
	lock_ObtainWrite(&volp->lock);
	if ((volp->states & VL_LAZYREP) && Now > volp->lastRepTry)
	    volp->lastRepTry = Now;
	lock_ReleaseWrite(&volp->lock);
    }
    if (code == 0) {
	/*
	 * All went fine.  Clear any pending badness info.  At the same time,
	 * be sensitive to the possibility that a volume's set of hosts might
	 * have changed.
	 */
	/* Do not obtain volp->lock, for normal-case execution speed.  If we
	 * occasionally mark a stray host as now-OK, we may make an extra RPC
	 * call, but that should be immaterial.
	 */
	if (volp != 0 && srvix >= 0) {
	    if (serverp == volp->serverHost[srvix] &&
		volp->perSrvReason[srvix] == 0) {
		volp->timeBad[srvix] = 0;
	    } else {
		(void) cm_SetVolumeServerBadness(volp, serverp, 0L, 0L);
	    }
	    rreqp->volumeError = 0;
	}
#if 0
	rreqp->timeWentBusy = 0;
#endif /* 0 */
	/* 
	 * Record the most recent starting time of a successful RPC.
	 * Get the lock only if we could possibly update something.
	 */
	if (serverp->lastCall < startTime || serverp->failoverCount > 0) {
	    lock_ObtainWrite(&serverp->lock);
	    serverp->failoverCount = 0;
	    if (serverp->lastCall < startTime)
		serverp->lastCall = startTime;
	    if (serverp->lastAuthenticatedCall < startTime) {
		lock_ObtainRead(&connp->lock);
		if (connp->currAuthn == rpc_c_protect_level_none)
		    serverp->lastAuthenticatedCall = startTime;
		lock_ReleaseRead(&connp->lock);
	    }
	    lock_ReleaseWrite(&serverp->lock);
	}
	cm_PutConn(connp);
	return 0;
    }

    /*
     * Check to see if it is a fileset error. Distinguish between persistent
     * and transient error conditions.
     */
    if (code >= VOLERR_PERS_LOWEST && code <= CM_ERR_TRANS_HIGHEST) {
	/* Set 'code' to the major class, but retain any fine distinctions in 'subCode'. */
	subCode = code - VOLERR_PERS_LOWEST;
	if (code < VOLERR_TRANS_LOWEST) {
	    /* persistent error code */
	    code = VOLERR_PERS_LOWEST;
	} else {
	    /* transient error code */
	    code = VOLERR_TRANS_LOWEST;
	}
	icl_Trace1(cm_iclSetp, CM_TRACE_ANALYZESUB, ICL_TYPE_LONG, subCode);
    }
    cm_FinalizeReq(rreqp);

    switch (code) {
      case rpc_s_op_rng_error:
	reTry = 0;
	break;

      case FSHS_ERR_NULLUUID:
	/* 
	 * The rpc binding has a nil object uuid
	 * Try to restart a new cotext and a connection as well.
	 */
	lock_ObtainWrite(&connp->lock);
	oldstates = connp->states;
	connp->states |= CN_FORCECONN;
	lock_ReleaseWrite(&connp->lock);
	if (!(oldstates & CN_FORCECONN))
	    cm_PrintServerText("dfs: rpc binding to server ",
			       serverp, " has a nil object uuid!\n");
	reTry = 1;
	break;

      case FSHS_ERR_FATALCONN:
	/* 
	 * The server has internal problems!!
	 * Restart a new context and a connection as well.
	 */
	lock_ObtainWrite(&connp->lock);
	oldstates = connp->states;
	connp->states |= CN_FORCECONN;
	lock_ReleaseWrite(&connp->lock);
	if (!(oldstates & CN_FORCECONN))
	    cm_PrintServerText("dfs: An internal problem from the fx server ",
			       serverp, "!\n");
	reTry = 1;
	break;

      case FSHS_ERR_STALEHOST:
	/* 
	 * The server no longer remembers this client! 
	 * Restart a new context and a connection as well.
	 */
	lock_ObtainWrite(&connp->lock);
	connp->states |= CN_FORCECONN;
	lock_ReleaseWrite(&connp->lock);
	icl_Trace0(cm_iclSetp, CM_TRACE_ANALYZESTALE);
	reTry = 1;	  
	break;

      case FSHS_ERR_SERVER_UP:
	/* 
	 * The server is no longer in token-recovery mode or it is not in
	 * token-move-reestablish mode.
	 */
	cm_PrintServerText("dfs: the fx server ", serverp, 
			   " is no longer awaiting token recovery from clients.\n");
	reTry = 0;
	break;

      case FSHS_ERR_MOVE_FINISHED:	/* TSR might see this */
      case TKM_ERROR_INVALIDID:	/* another one that TSR might see */
/* recognize TOKENINVALID also since we were using that for a while */
      case TKM_ERROR_TOKENINVALID:	/* another one that TSR might see */
	reTry = 0;
	break;

      case FSHS_ERR_SERVER_REESTABLISH:
	/*
	 * The fx server is still in the TSR mode serving token-renew 
	 * requests and would not accept any other new request in that period. 
	 * Treat as a transient volume error.
	 */
	subCode = CM_ERR_TRANS_SERVERTSR - VOLERR_PERS_LOWEST;
	goto transient_volume_error;
	
      case rpc_s_wrong_boot_time:
	/*
	 * The server must have been crashed before. Re-create a rpc binding.
	 */
	lock_ObtainWrite(&connp->lock);
	oldstates = connp->states;
	connp->states |= CN_FORCECONN;
	lock_ReleaseWrite(&connp->lock);
	if (!(oldstates & CN_FORCECONN))
	    cm_PrintServerText("dfs: the fx server ", serverp,
			       " has been rebooted\n");
	reTry = 1;
	break;

      case rpc_s_auth_tkt_expired:
	/*
	 * The client's ticket has expired.  Since this can race with a user's
	 * reauthenticating herself, we retry this and allow cm_ConnByHost to
	 * handle an inability to re-authenticate.
	 */
	lock_ObtainWrite(&connp->lock);
	oldstates = connp->states;
	connp->states |= CN_FORCECONN;
	lock_ReleaseWrite(&connp->lock);
	if (!(oldstates & CN_FORCECONN)) {
	    strcpy(msgbuf, " for user ");
#ifdef SGIMIPS
	    if (rreqp->uid == (unsigned long)-2) {
		strcat(msgbuf,  "unauthenticated");	/* CM_UNAUTH_USERNUM */
	    }
	    else {
		osi_cv2string(rreqp->uid, &msgbuf[sizeof " for user " - 1]);
	    }
	    strcat(msgbuf, ", pag ");
	    osi_cv2hstring(rreqp->pag, &msgbuf[strlen(msgbuf)]);
#else /* SGIMIPS */
	    osi_cv2string(rreqp->uid, &msgbuf[sizeof " for user " - 1]);
#endif /* SGIMIPS */
	    strcat(msgbuf,  " has expired!\n");
	    cm_PrintServerText("dfs: ticket to server ", serverp, msgbuf);
	}
	reTry = 1;
	break;

      case EACCES:
	rreqp->accessError = 1;
	reTry = 0;	/* should be no better on another server, either */
	break;

      case TKM_ERROR_NOTOKENENTRY:      /* client returned unknown token id */
      case TKM_ERROR_REQUESTQUEUED:
      case TKM_ERROR_HARDCONFLICT:
      case TKM_ERROR_NOENTRY:
        /*
         * Just return and not to print out msg.
         */
      case REP_ERR_SHORTVVLOG:
      case REP_ERR_NOSTORAGE:
      case REP_ERR_NOTREPLICA:
	/* this one (from a repserver) recovers itself. */
	reTry = 0;
	break;

      case TKM_ERROR_NOTINMAXTOKEN:
      case EROFS:
	/*
	 * Must be an illegal op on a read-only file system
	 */
	rreqp->readOnlyError = 1;
	reTry = 0;	/* and no better on another server, either */
	break;

      case TKM_ERROR_TOKENCONFLICT:
	/* can't get token due to fts refusal to yield it.  Retry
	 * again later.
	 */
      case TKM_ERROR_TRYAGAIN:
	/*
	 * Cannot get token (Data or Status) due to the network failure.
	 * Sleep for 20 secs and try it again.  
	 */
	/* Let the guys who want to fail instead of wait, do so */
	if (cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOVOLWAIT) {
	    /* ensure that this is set if we can't ask for a retry */
	    rreqp->networkError = 1;
	    reTry = 0;
	} else if (CM_SLEEP_INTR(CM_RREQ_SLEEP)) {
	    rreqp->interruptedError = 1;
	    icl_Trace0(cm_iclSetp, CM_TRACE_TKMSLEEP_INTERRUPTED);
	    reTry = 0;
	} else {
	    reTry = 1;
	}
	break;

      case rpc_s_comm_failure:		/* Network problem */
	/*
	 * If we are here, we do not know whether the network) is 
	 * down or the rpc binding (addr + endpoint) is invalid, but we
	 * don't really care.  We redo all the connections in either case.
	 */
	(void) cm_ServerDown(serverp, connp, code);
	/* mark-as-bad all of them to that server */
	cm_MarkBadConns(serverp, CM_BADCONN_ALLCONNS);
	reTry = 1;
	break;

      case rpc_s_call_timeout:
	lock_ObtainWrite(&connp->lock);
	connp->states |= CN_FORCECONN;
	lock_ReleaseWrite(&connp->lock);
	(void) cm_ServerDown(serverp, connp, code);
	reTry = 1;
	break;

      case rpc_s_helper_not_running:
	/*
	 * We retry this, allowing cm_ConnByHost to handle an inability to
	 * re-authenticate.
	 */
	lock_ObtainWrite(&connp->lock);
	connp->states |= CN_FORCECONN;
	lock_ReleaseWrite(&connp->lock);
	reTry = 1;
	break;

    case rpc_s_unsupported_protect_level:
    case FSHS_ERR_AUTHNLEVEL_S_TOO_HIGH:
    case FSHS_ERR_AUTHNLEVEL_S_TOO_LOW:
    case FSHS_ERR_AUTHNLEVEL_F_TOO_HIGH:
    case FSHS_ERR_AUTHNLEVEL_F_TOO_LOW:
	reTry = (cm_ReactToAuthCodes(connp, volp, rreqp, code) == 1);
	break;

      case FSHS_ERR_MOVE_REESTABLISH:
	/*
	 * The fileset must be just moved to that server and the server is
	 * still busy in serving other CMs in reestablishing tokens.
	 * The fileset location has already been updated. Let's treat it as a 
	 * trans error.
	 */
	  /* Nonetheless, we have to choose a value for subCode! */
	  subCode = CM_ERR_TRANS_MOVE_TSR - VOLERR_PERS_LOWEST;
	  /* FALLTHROUGH */

      case VOLERR_TRANS_LOWEST:	/* transient fileset error condition */
	/*
	 * This case covers conditions such as fileset busy for a short 
	 * interval including offline due to an undergoing cloning process.
	 * In this case, set reTry = 1, enter the error in the cm_volume
	 * struct if possible, sleep if necessary.
	 */
transient_volume_error:
	  reTry = 1;
	  if (volp == 0) {
#if 0
	    if (rreqp->timeWentBusy != 0 &&
		((rreqp->timeWentBusy + CM_RREQ_TIMEOUT) < osi_Time())) {
		rreqp->networkError = 1;
		reTry = 0;	/* finally time out the request, if no vol to use */
	    } else
#endif /* 0 */
	    {
#if 0
		if (rreqp->timeWentBusy == 0) 
		    rreqp->timeWentBusy = osi_Time();
#endif /* 0 */
		if (fidp)
		    cm_printf("dfs: Waiting for busy fileset (%u,,%u) (code %u)\n",
			      AFS_HGETBOTH(fidp->Volume),
			      subCode+VOLERR_PERS_LOWEST);
		else
#ifdef SGIMIPS
		if (subCode == (CM_ERR_TRANS_SERVERTSR - VOLERR_PERS_LOWEST)) {
		    cm_PrintServerText("dfs: server ", serverp,
					" is awaiting token recovery from clients.\n");
		} else if (subCode == (CM_ERR_TRANS_CLIENTTSR - VOLERR_PERS_LOWEST)) {
		    cm_PrintServerText("dfs: client is recovering tokens on server", serverp,
					".\n");
		} else {
#define BUSY_FILESET_PREAMBLE "dfs: Waiting for busy fileset (code "
#define BUSY_FILESET_POSTAMBLE ") on server "
		    strcpy(msgbuf, BUSY_FILESET_PREAMBLE);
		    osi_cv2string(subCode+VOLERR_PERS_LOWEST,
			    &msgbuf[sizeof BUSY_FILESET_PREAMBLE - 1]);
		    strcat(msgbuf, BUSY_FILESET_POSTAMBLE);
		    cm_PrintServerText(msgbuf, serverp, "\n");
		}
#else /* SGIMIPS */
		    cm_printf("dfs: Waiting for busy fileset (code %u)\n",
			      subCode+VOLERR_PERS_LOWEST);
#endif /* SGIMIPS */
		icl_Trace1(cm_iclSetp, CM_TRACE_ANALYZEBUSY, ICL_TYPE_LONG,
			   (fidp? AFS_hgetlo(fidp->Volume) : 0));
		if (CM_SLEEP_INTR(CM_RREQ_SLEEP)) {
		    rreqp->interruptedError = 1;
		    icl_Trace0(cm_iclSetp, CM_TRACE_BUSYSLEEP_INTERRUPTED);
		    reTry = 0;
		}
	    }
	  } else {
#if 0
	    if (rreqp->timeWentBusy == 0) 
		rreqp->timeWentBusy = osi_Time();
#endif /* 0 */
	    /* Print a message only if the fileset is transiting to a new state. */
	    setVolCode = cm_SetVolumeServerBadness(volp, serverp, code, subCode);
	    if (setVolCode == 1) {
		/*
		 * print error msg based on subCode; try to convert subCode to 
		 * the actual error message from the catalog
		 */
		if (subCode == (CM_ERR_TRANS_SERVERTSR - VOLERR_PERS_LOWEST)) {
		    cm_PrintServerText("dfs: server ", serverp,
					" is awaiting token recovery from clients.\n");
		} else if (subCode == (CM_ERR_TRANS_CLIENTTSR - VOLERR_PERS_LOWEST)) {
		    cm_PrintServerText("dfs: client is recovering tokens on server", serverp,
					".\n");
		} else {
		    strcpy(msgbuf, "dfs: fileset (");
		    osi_cv2string(fidp ? AFS_hgethi(fidp->Volume) : 0, 
				  &msgbuf[sizeof "dfs: fileset (" - 1]);
		    strcat(msgbuf,",,");
		    osi_cv2string(fidp ? AFS_hgetlo(fidp->Volume) : 0,
				  msgbuf + strlen(msgbuf));
		    strcat(msgbuf,") ");
		    if (subCode == (VOLERR_TRANS_VVOLD - VOLERR_PERS_LOWEST)) {
			strcat(msgbuf, "has version older than our latest");
		    } else if (subCode ==
			       (CM_ERR_TRANS_MOVE_TSR - VOLERR_PERS_LOWEST)) {
			strcat(msgbuf, "is reestablishing tokens after a move");
		    } else {
			strcat(msgbuf, "is busy with code ");
			osi_cv2string(subCode+VOLERR_PERS_LOWEST,
				      msgbuf + strlen(msgbuf));
		    }
		    strcat(msgbuf, " on server ");
		    cm_PrintServerText(msgbuf, serverp, ".\n");
		}
	    }
	    icl_Trace2(cm_iclSetp, CM_TRACE_ANALYZEFASTBUSY,
		       ICL_TYPE_LONG, AFS_hgetlo(volp->volume),
		       ICL_TYPE_LONG, subCode+VOLERR_PERS_LOWEST);
	    /* Let the guys who want to fail instead of wait, do so */
	    if (cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOVOLWAIT) {
		/* ensure that this is set if we can't ask for a retry */
		rreqp->networkError = 1;
		reTry = 0;
	    }
	}
	break;

      case CM_ERR_PERS_CONNAUTH:
	/*
	 * We get connection failures talking to this server about this volume.
	 * Get cm_ConnByMHosts to try another server if there are others.
	 */
	subCode = CM_ERR_PERS_CONNAUTH - VOLERR_PERS_LOWEST;
	/* FALLTHROUGH */
	
      case VOLERR_PERS_LOWEST:	/* persistent fileset error condition */
	/*
	 * We do several things here.
	 * If there's a volp, we check if there is new FLDB information for this
	 * volume, unless we can tell that we were operating with obsolete
	 * information because our server has already been removed from the
	 * cm_volume array.  If there is new information, the timeBad[] and
	 * perSrvReason[] arrays will be cleared, in cm_InstallVolumeEntry().
	 * We can always retry if we can record any pertinent information in
	 * the cm_volume, since cm_ConnByMHosts can handle it; otherwise, we
	 * can't retry.
	 * We don't want to record the persistent-bad fileset error info in the
	 * cm_volume if we're about to check the FLDB, since other threads are
	 * trying to use that same information to evaluate whether a fileset is
	 * available.  Instead, delay storing the fileset-error condition until
	 * the FLDB check is completed.
	 * Keep the error-message printing to a minimum: print only when the
	 * fileset is transiting to a new state.
	 */
	reasonToPrint = 1;
	gotNewLoc = 0;
	if (fidp && volp) {
	    lock_ObtainRead(&volp->lock);
	    /* Check if our serverp is out of the cm_volume set already */
	    vlCode = cm_SameHereServer(serverp, volp);
	    lock_ReleaseRead(&volp->lock);
	    if (vlCode) {
		/* Out server is still in the set */
		struct cm_cell *cellp;
		if (cellp = cm_GetCell(&fidp->Cell)) { /* still have the cell */
		    vlCode = cm_CheckVolumeInfo(fidp, rreqp, volp, cellp, &gotNewLoc);
		    if (vlCode == 0) {
			/* found a new home */
			icl_Trace3(cm_iclSetp, CM_TRACE_ANALYZEMOVED,
				   ICL_TYPE_LONG, AFS_hgetlo(fidp->Volume),
				   ICL_TYPE_LONG, vlCode,
				   ICL_TYPE_LONG, gotNewLoc);
			if (gotNewLoc) {
			    /* already printed location change */
			    reasonToPrint = 0;
			} else {
			    CM_SLEEP(3);	/* throttle back */
			}
		    }
		    /* See if this is an active volume but this server isn't in
		     * the active set any longer.  If so, stay quiet.
		     */
		    if (reasonToPrint) {
			lock_ObtainRead(&volp->lock);
			if (!(volp->states & VL_HISTORICAL)
			    && (cm_SameHereServer(serverp, volp) == 0)) {
			    reasonToPrint = 0;
			}
			lock_ReleaseRead(&volp->lock);
		    }
		}
	    }
	}/* fidp && volp */
	/* See if there's still an error to be stored & a place to store it. */
	if (!volp) {
	    reTry = 0;	/* Nowhere to record the persistent problem */
	    rreqp->volumeError = 1;   /* ensure that this is set if we can't ask for a retry */
	} else {
	    reTry = 1;	/* We have a place to record loop-control info */
	    /* Don't record badness if we have a new location */
	    if (gotNewLoc == 0) {
		setVolCode = cm_SetVolumeServerBadness(volp, serverp, code, subCode);
		if (setVolCode != 1)
		    reasonToPrint = 0;
		/* In particular, don't print if nothing changed, and DON'T
		 * print if the server to which this call was made no longer
		 * stores this volume.
		 */
		/* Let the guys who want to fail instead of wait, do so */
		if (cm_GetConnFlags(rreqp) & CM_RREQFLAG_NOVOLWAIT) {
		    /* ensure that this is set if we can't ask for a retry */
		    rreqp->networkError = 1;
		    reTry = 0;
		}
	    }
	}
	/* Always print if we cannot retry this call. */
	if (reasonToPrint || reTry == 0) {
	    strcpy(msgbuf, "dfs: fileset (");
	    osi_cv2string(fidp ? AFS_hgethi(fidp->Volume) : 0,
			  msgbuf + strlen(msgbuf));
	    strcat(msgbuf, ",,");
	    osi_cv2string(fidp ? AFS_hgetlo(fidp->Volume) : 0,
			  msgbuf + strlen(msgbuf));
	    strcat(msgbuf, ") ");
	    if (subCode == (VOLERR_PERS_CLONECLEAN - VOLERR_PERS_LOWEST)) {
		strcat(msgbuf, "awaiting clone cleanup");
	    } else if (subCode == (VOLERR_PERS_OUTOFSERVICE - VOLERR_PERS_LOWEST)) {
		strcat(msgbuf, "awaiting move cleanup");
	    } else if (subCode == (VOLERR_PERS_DAMAGED - VOLERR_PERS_LOWEST)) {
		strcat(msgbuf, "inconsistent");
	    } else if (subCode == (CM_ERR_PERS_CONNAUTH-VOLERR_PERS_LOWEST)) {
		strcat(msgbuf, "authentication error");
	    } else if (subCode == (VOLERR_PERS_DELETED - VOLERR_PERS_LOWEST)) {
		strcat(msgbuf, "is not exported");
	    } else {
		strcat(msgbuf, "error, code ");
		osi_cv2string(subCode+VOLERR_PERS_LOWEST, 
			      msgbuf + strlen(msgbuf));
		strcat(msgbuf, ",");
	    }
	    strcat(msgbuf, " on server ");
	    cm_PrintServerText(msgbuf, serverp, ".\n");
	}
	break;

      case VL_NOENT:
	rreqp->volumeError = 1;
	reTry = 0;
	break;

      case UNOQUORUM:
	/* Not really down, but should try other servers. */
	(void) cm_ServerDown(serverp, connp, code);
	CM_SLEEP(1);	/* better safe than sorry */
	reTry = 1;
	break;
	
      case CM_REP_ADVANCED_AGAIN:
	  /* In reacting to an increased fileset version, it increased again. */
	reTry = 1;
	break;

      default:

	if (cm_RPCError(code)) {
	    /* Other RPC errors ? */
	    (void) cm_ServerDown(serverp, connp, code);
	    reTry = 1;
	} 
	else if ((code & 0xf0000000) == 0x20000000) {
	    /* Other DFS errors ? */
	    (void) cm_ServerDown(serverp, connp, code);
	    reTry = 0;
	}
#if defined(SGIMIPS) && defined(KERNEL)
	else if (code > MAXKERNELERRORS) {
#else  /* SGIMIPS && KERNEL */
	else if (code > 256) {
#endif /* SGIMIPS && KERNEL */
	    /* Other DCE errors ? */
	    (void) cm_ServerDown(serverp, connp, code);
	    reTry = 1;
	}
	else	/* Other generic OS errors */
	    reTry = 0;	/* assume that other servers would be no better. */
	/*
	 * No message here for EDQUOT/ENOSPC;
	 * that message is printed during cm_SyncDCache.
	 */

      } /* Switch */
    
    cm_PutConn(connp);		
    return reTry;
}
