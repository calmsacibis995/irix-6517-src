/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_daemons.c,v 65.11 1999/04/27 18:06:45 ethan Exp $";
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
 * HISTORY
 * $Log: cm_daemons.c,v $
 * Revision 65.11  1999/04/27 18:06:45  ethan
 * bring stop offsets into sync between buffer and page caches
 *
 * Revision 65.10  1998/10/19 18:25:49  lmc
 * Only schedule the sync thread if we are running with disk
 * based cache.  PV 632996
 *
 * Revision 65.9  1998/10/16 19:55:42  lmc
 * Add a periodic sync for the CacheItems file to flush out buffers.
 *
 * Revision 65.8  1998/05/08  18:43:06  bdr
 * Scache entries were holding vnode's with a zero v_count
 * waiting for a FlushSCache to come along and clear them up.  This
 * is bad as various bits of IRIX (vn_get for example) assume that a
 * vnode with a zero v_count is on a freelist.  Change the code
 * to do an extra hold on the vnode when we alloc it and rework
 * how we inactivate vnode's so that FlushSCache now drops the count
 * to zero.  This fixes a panic where lpage release code called vn_get
 * with a vnode that was not on the freelist.  Also fixes a panic
 * in cm_inactive where we fail an assert because our vnode has been
 * reused.
 *
 * Revision 65.7  1998/03/23  16:23:47  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.6  1998/03/19 23:47:23  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.5  1998/03/09  19:37:34  lmc
 * Remove setting of process name because we are now sthreads instead
 * of uthreads - thus no process name.
 *
 * Revision 65.4  1998/03/05  17:44:54  bdr
 * Modified every function declaration to be the ANSI C standard.
 *
 * Revision 65.3  1998/03/02  22:27:04  bdr
 * Initial cache manager changes.
 *
 * Revision 65.2  1997/11/06  19:57:45  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:23  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.29.1  1996/10/02  17:07:30  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:42  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/sysincludes.h>		/* Standard vendor system headers */
#include <dcedfs/osi_cred.h>		/* Standard vendor system headers */
#include <dcedfs/tpq.h>			/* thread pool manager */
#include <dcedfs/rep_proc.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_dcache.h>
#include <cm_volume.h>
#include <cm_conn.h>
#include <cm_vcache.h>
#include <cm_server.h>

#ifdef AFS_SUNOS5_ENV
#include <dcedfs/auth_at.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_daemons.c,v 65.11 1999/04/27 18:06:45 ethan Exp $")

/* define thread pool parameters for low, med, and high priorities */
#define CM_DAEMON_MINTHREADS		2
#define CM_DAEMON_MEDMAXTHREADS		4
#define CM_DAEMON_HIGHMAXTHREADS	6

/*
 * We need an auxilliary thread pool for use by the parallel set code to
 * make sure we don't deadlock.  The problem is that a thread may schedule
 * a number of sub-operations then wait for them to complete.  If all the
 * threads are waiting, there aren't any left to run the sub-operations.
 */
/* define thread pool parameters for aux thread pool, used by tpq_ParSet */
#define CM_AUX_DAEMON_MINTHREADS	3
#define CM_AUX_DAEMON_MEDMAXTHREADS	5
#define CM_AUX_DAEMON_HIGHMAXTHREADS	8

/* define time (in seconds) a thread can sleep before being reaped */
#define CM_DAEMON_THREADENNUI		5*60

/* define how long before token actually expires that we want
 * to start renewing tokens.  Make it 10 minutes
 */
#define CM_DAEMON_TOKENRENEWLEAD	(10*60)

#define CYCLE_TIME 15 		/* Use 15 seconds as one interval */

/* forward declarations and prototypes */
static void ServerTokenMgt _TAKES((void *, void *));
static void RefreshTokens _TAKES((void *, void *));
static void UpdateTokensLifeTime _TAKES((void *));
static void GCOldConn _TAKES((void *, void *));
static void FlushActiveScaches _TAKES((void *, void *));
static void WriteThroughDSlots _TAKES((void *, void *));
static void RenewLazyReps _TAKES((void *, void *));
static void cm_RefreshKeepAlives _TAKES((void *, void *));
static void PingServers _TAKES((void *, void *));
static void PingServersNoAdjust _TAKES((void *, void *));
static void CheckDownServers _TAKES((void *, void *));
static void CheckVolumeNames _TAKES((void *, void *));
static int CheckOnVolRoot _TAKES((struct cm_volume *));
static void ReviveServerAddrs _TAKES((void *, void *));
static void ResetServerAuthn _TAKES((void *, void *));
#ifdef SGIMIPS
static void SyncCacheItems _TAKES((void *, void *));
#endif /* SGIMIPS */

/* define the handle through which we'll be able to access the daemon thread pool */
/* EXPORT */
/* struct osi_WaitHandle AFSWaitHandler; */	/* XXX Interrupt waiting on daemon */
void *cm_threadPoolHandle = NULL;
void *cm_auxThreadPoolHandle = NULL;

/*
 * Create the thread pool and queue up the various cm operations to run.
 */
/* EXPORT */
#ifdef SGIMIPS
void cm_Daemon(void)
#else  /* SGIMIPS */
void cm_Daemon()
#endif /* SGIMIPS */
{

    for (;;) {

	cm_EnterDaemon();

	/* initialize the thread pool: */
	cm_threadPoolHandle = tpq_Init(CM_DAEMON_MINTHREADS,
				   CM_DAEMON_MEDMAXTHREADS,
				   CM_DAEMON_HIGHMAXTHREADS,
				   CM_DAEMON_THREADENNUI,
				   "cmD"	/* thread name prefix */);

	if (cm_threadPoolHandle == (void *)NULL)  {
	    uprintf("cm_Daemon: couldn't initialize the thread pool\n");
	    return;	/* couldn't create thread pool */
	}

	/* initialize the auxilliary thread pool: */
	cm_auxThreadPoolHandle = tpq_Init(CM_AUX_DAEMON_MINTHREADS,
				      CM_AUX_DAEMON_MEDMAXTHREADS,
				      CM_AUX_DAEMON_HIGHMAXTHREADS,
				      CM_DAEMON_THREADENNUI,
				      "cmA"	/* thread name prefix */);

	if (cm_auxThreadPoolHandle == (void *)NULL)  {
	    tpq_ShutdownPool(cm_threadPoolHandle);
	    uprintf(
		"cm_Daemon: couldn't initialize the auxilliary thread pool\n");
	    return;	/* couldn't create thread pool */
	}

	/* tpq_QueueRequest(handle, op, arg, priority, gracePeriod, 
	    resched, dropDead) */
	/* Perform keep-alive functions */
	(void) tpq_QueueRequest(cm_threadPoolHandle, PingServers, NULL,
			    TPQ_HIGHPRI, CYCLE_TIME, CYCLE_TIME, 0);
#ifdef SGIMIPS
	if (!cm_diskless) {
	   (void) tpq_QueueRequest(cm_threadPoolHandle, SyncCacheItems, NULL,
			    TPQ_HIGHPRI, CYCLE_TIME, CYCLE_TIME, 0);
	}
#endif /* SGIMIPS */

	/* Ping down servers */
	(void) tpq_QueueRequest(cm_threadPoolHandle, CheckDownServers, NULL,
			    TPQ_MEDPRI, CYCLE_TIME, CYCLE_TIME+5, 0);

	/* Ping up servers on addresses marked down */
	(void) tpq_QueueRequest(cm_threadPoolHandle,
				ReviveServerAddrs, NULL,
				TPQ_MEDPRI, 5 * 60, 15 * 60, 0);

	/* return queued tokens to the server every minute: */
	(void) tpq_QueueRequest(cm_threadPoolHandle, ServerTokenMgt, NULL,
			    TPQ_MEDPRI, 30, 60, 0);

        /* refresh tokens every 5 minutes */
        (void) tpq_QueueRequest(cm_threadPoolHandle, RefreshTokens, NULL,
			    TPQ_HIGHPRI, 60, CM_DAEMON_TOKENRENEWLEAD/3, 0);

	/* Check all mount points every hour */
	(void) tpq_QueueRequest(cm_threadPoolHandle, CheckVolumeNames, NULL,
			    TPQ_MEDPRI, 3600, 3600, 0);

	/* Reset cached file-server authentication bounds every hour */
	(void) tpq_QueueRequest(cm_threadPoolHandle,
				ResetServerAuthn, NULL,
				TPQ_MEDPRI, 10 * 60, 60 * 60, 0);

	/* Do delayed writes on data and status */
	(void) tpq_QueueRequest(cm_threadPoolHandle, FlushActiveScaches, NULL,
			    TPQ_MEDPRI, 15, 30, 0);

	/* Write through CacheInfo entries */
	(void) tpq_QueueRequest(cm_threadPoolHandle, WriteThroughDSlots, NULL,
			    TPQ_MEDPRI, 30, 60, 0);

	/* Keep alive any read-only files from lazy replicas */
	(void) tpq_QueueRequest(cm_threadPoolHandle, cm_RefreshKeepAlives, NULL,
			    TPQ_MEDPRI, 30*60, 60*60, 0);

	/* Periodic clock resync */
	if (cm_setTime)
	    (void) tpq_QueueRequest(cm_threadPoolHandle, PingServersNoAdjust, 
			    NULL, TPQ_LOWPRI, 600, 600, 0);

	/* Period GC old rpc connections */
	(void) tpq_QueueRequest(cm_threadPoolHandle, GCOldConn, NULL,
			    TPQ_LOWPRI, 600, 600, 0);

	/* Perform lazy replica processing */
	(void) tpq_QueueRequest(cm_threadPoolHandle, RenewLazyReps, NULL,
			    TPQ_LOWPRI, 60, 60, 0);

	/* wait here until we're told to shut down */
	cm_WaitForShutdown();

	tpq_ShutdownPool(cm_threadPoolHandle);
	tpq_ShutdownPool(cm_auxThreadPoolHandle);

	/* now tell shutdown code that we're done */
	cm_LeaveDaemon();
    } /* End for (;;) */
}

/* 
 * Ensure there are at least extraBlocks byte units of space left in cache 
 * This function should be called with no vnode locks held so that pending
 * stores can occur to free blocks.
 */
/* EXPORT */
#ifdef SGIMIPS
int cm_CheckSize(register long extraBlocks) 
#else  /* SGIMIPS */
int cm_CheckSize(extraBlocks)
    register long extraBlocks; 
#endif /* SGIMIPS */
{
    int call_counter = 0;	/* # calls to cm_GetDownD() */
    int wait_counter = 0;	/* # waits */
    long excess_blocks;		/* blocks over cachesize */
    long last_excess_blocks;	/* saved state */
    int rc = 0;

    lock_ObtainWrite(&cm_dcachelock);
    excess_blocks = last_excess_blocks = 
	cm_blocksUsed + extraBlocks - cm_cacheBlocks;

    while (excess_blocks > 0) {
	if ((excess_blocks >= last_excess_blocks) && 
	    ((++call_counter % 5) == 0)) {
	    /* 
	     * After a few tries, if blocks are not getting freed, then 
	     * delay to allow a sync to run.
	     */
	    if ((++wait_counter % 36) == 0) {
		rc = 1;
		break;		/* After 3 minutes give up! */
	    }
	    lock_ReleaseWrite(&cm_dcachelock);
	    osi_Wait(5000, 0, 0);
	    lock_ObtainWrite(&cm_dcachelock);
	}
	cm_GetDownD(5, CM_GETDOWND_NEEDSPACE);
	last_excess_blocks = excess_blocks;
	excess_blocks = cm_blocksUsed + extraBlocks - cm_cacheBlocks;
    }
    lock_ReleaseWrite(&cm_dcachelock);
    return rc;
}


/*
 * Refresh tokens' expiration time if they are about to expire. 
 */
/* ARGSUSED */
static void RefreshTokens(void *unused1, void *unused2)
{
    register struct cm_server *serverp;
    register int i;
    int opCount = 0;
    void *parsetp;

    /* create a tpq parallel set */
    parsetp = tpq_CreateParSet(cm_auxThreadPoolHandle);
    osi_assert(parsetp != NULL);

    lock_ObtainRead(&cm_serverlock);
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	     /* Go ahead, we don't GC servers. */
	     lock_ReleaseRead(&cm_serverlock);
	    /* 
	     * If we don't have any tokens from this server, don't bother 
	     * checking. The server may even be down.
	     */
	    lock_ObtainRead(&serverp->lock);

	    if (cm_HaveTokensFrom(serverp) && 
		!(serverp->states & (SR_DOWN | SR_TSR))) {
		lock_ReleaseRead(&serverp->lock);
		(void) tpq_AddParSet(parsetp, UpdateTokensLifeTime, 
				     (void *)serverp, TPQ_HIGHPRI, 30 /* sec */);
		opCount++;
	    } else
	        lock_ReleaseRead(&serverp->lock);
	    lock_ObtainRead(&cm_serverlock);
	}
    }
    lock_ReleaseRead(&cm_serverlock);

    icl_Trace2(cm_iclSetp,CM_TRACE_REFRESHTOKEN1, ICL_TYPE_LONG, osi_Time(),
	       ICL_TYPE_LONG, opCount);
    tpq_WaitParSet(parsetp);
    icl_Trace1(cm_iclSetp, CM_TRACE_REFRESHTOKEN2, ICL_TYPE_LONG, osi_Time());
    return;
}

/*
 * This function is called to examine tokens associated with open files. 
 * If they are about to expire, renew them. 
 */
static void UpdateTokensLifeTime(void *arg)
{ 
    struct cm_server *serverp = (struct cm_server *)arg;
    register struct cm_scache *scp;
    register struct cm_volume *volp;
    register struct cm_tokenList *tlp, *nlp;
    register int i;
    afs_recordLock_t blocker;
    struct cm_rrequest rreq;
    long refreshHorizon, code;
    int anyLeft, ix;
    u_long rc;
    long more;
    unsigned32 mask;
    afs_hyper_t tokType;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */

    icl_Trace0(cm_iclSetp, CM_TRACE_STARTUPDATETOKEN);
    cm_InitUnauthReq(&rreq);  /* using unauthen binding */
    cm_SetConnFlags(&rreq, CM_RREQFLAG_NOVOLWAIT);

    /* Renew the HERE token for each volume first */
    lock_ObtainWrite(&cm_volumelock);
    for (i = 0; i < VL_NVOLS; i++) {
        for (volp = cm_volumes[i]; volp; volp = volp->next) {
	    volp->refCount++;
	    lock_ReleaseWrite(&cm_volumelock);
	    refreshHorizon = osi_Time() + CM_DAEMON_TOKENRENEWLEAD;
	    lock_ObtainRead(&volp->lock);
	    if ((volp->hereServerp == serverp) &&
		!(volp->states & (VL_HISTORICAL | VL_RESTORETOKENS)) &&
		(volp->hereToken.expirationTime != 0) &&
		(volp->hereToken.expirationTime < refreshHorizon)) {
	         lock_ReleaseRead(&volp->lock);
	         code = cm_GetHereToken(volp, &rreq, 0, CM_GETTOKEN_FORCE);
		 if (code == ETIMEDOUT) {
		     cm_PutVolume(volp);
		     return; /* the server is down */
		 }
	    } else
		lock_ReleaseRead(&volp->lock);
	    lock_ObtainWrite(&cm_volumelock);
	    volp->refCount--;
	}
    }
    lock_ReleaseWrite(&cm_volumelock);

    lock_ObtainWrite(&cm_scachelock);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
#ifdef SGIMIPS
	    s = VN_LOCK(SCTOV(scp));
            if ( SCTOV(scp)->v_flag & VINACT ) {
                osi_assert(CM_RC(scp) == 0);
                VN_UNLOCK(SCTOV(scp), s);
                continue;
            }
            CM_RAISERC(scp);
            VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */
	    CM_HOLD(scp);
#endif /* SGIMIPS */
#ifdef AFS_SUNOS5_ENV
	    if (CM_RC(scp) == 1 && scp->opens != 0)
		cm_CheckOpens(scp, 1);
#endif /* AFS_SUNOS5_ENV */
	    rc = CM_RC(scp);
	    lock_ReleaseWrite(&cm_scachelock);
	    lock_ObtainWrite(&scp->llock);

	    /* renew tokens that'll expire in the next renew lead
	     * time seconds, in case we don't get around for a while.
	     */
	    refreshHorizon = osi_Time() + CM_DAEMON_TOKENRENEWLEAD;
	    for (tlp = (struct cm_tokenList *) scp->tokenList.next;
		 tlp != (struct cm_tokenList *)&scp->tokenList; tlp = nlp){

		/* Time has been adjusted */
		cm_HoldToken(tlp);	/* preserve over GetTokensRange &c */
		/* don't have to renew totally expired tokens, deleted
		 * tokens, or tokens that haven't been granted yet.
		 */
		if (tlp->serverp == serverp
		    && (tlp->token.expirationTime < refreshHorizon)
		    && (!(tlp->flags & (CM_TOKENLIST_DELETED
					| CM_TOKENLIST_ASYNC)))) {
		    /* Try to replace this token with a fresher one. */
		    /* The token is less than 10 minutes from expiration,
                     * though, so we don't bother to return obsoleted bits to
                     * the server.
                     */
		    cm_StripRedundantTokens(scp, tlp,
					    AFS_hgetlo(tlp->token.type),1,0);
		    /* See if our current CM state still needs this token. */
		    if (AFS_hgetlo(tlp->token.type)) {
			mask = 1;
			lock_ReleaseWrite(&scp->llock);
			for (ix = 0; ix < 32; ix++, mask = mask<<1) {
			    AFS_hset64(tokType, 0, mask);
			    if (AFS_hgetlo(tokType) &
				AFS_hgetlo(tlp->token.type)) {
				/* This does local-only work--no storebacks! */
				(void) cm_RevokeOneToken(scp, &tlp->token.tokenID,
							 &tokType, (afs_recordLock_t *) 0,
							 (afs_token_t *) 0, (afs_token_t *)0,
							 &more /* dummy */, 1);
			    }
			}
			lock_ObtainWrite(&scp->llock);
		    }
		    /* Check again whether this token is undeleted. */
		    if (!(tlp->flags & (CM_TOKENLIST_DELETED
					| CM_TOKENLIST_ASYNC))) {
			afs_token_t token;
			icl_Trace1(cm_iclSetp, CM_TRACE_UPDATETOKEN, 
				   ICL_TYPE_POINTER, scp);
#ifdef SGIMIPS
			if (scp->opens == 0 && rc <= 2) {
#else  /* SGIMIPS */
			if (scp->opens == 0 && rc <= 1) {
#endif /* SGIMIPS */
			    /* expired token that we don't want to renew,
			     * so invalidate caches.  Requires locked and held
			     * scp, held tlp.  Temporarily unlocks scp.
			     * May delete tlp if all tokens reclaim properly,
			     * but since tlp is held, it won't be unqueued.
			     */
			    (void) cm_SimulateTokenRevoke(scp, tlp, &anyLeft);
			} else
			    anyLeft = 1;
			if (anyLeft) {
			    token.type = tlp->token.type;
			    token.beginRange = tlp->token.beginRange;
			    token.endRange = tlp->token.endRange;
			    code = cm_GetTokensRange(scp, &token, &rreq,
				    CM_GETTOKEN_FORCE, &blocker, WRITE_LOCK);
			    if ((code != 0) && ((serverp->states & SR_DOWN) != 0)) {
				cm_ReleToken(tlp);
				lock_ReleaseWrite(&scp->llock);
				lock_ObtainWrite(&cm_scachelock);
				CM_RELE(scp);
				lock_ReleaseWrite(&cm_scachelock);
				return;   /* The server is down */
			    } /* network error */
			    /*
			     * If the token renewal was successful, then
			     * tlp can be discarded. cm_AddToken() did
			     * not do this since we have a reference on tlp.
			     */
			    if (code == 0) {
				cm_DelToken(tlp);
			    }
			}	/* file not open */
		    }
		}
		nlp = (struct cm_tokenList *) tlp->q.next;
		cm_ReleToken(tlp);
	    } /* for */
	    lock_ReleaseWrite(&scp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);
	} /* for */
    }
    lock_ReleaseWrite(&cm_scachelock);
}

/*
 * Check all mount points.  This dude will be scheduled to run every hour
 * at medium priority.  It will have no sub-operations.
 * The ``interactive'' flag says that this is a pioctl-style call.
 */
/* EXPORT */
#ifdef SGIMIPS
void cm_CheckVolumeNames(int interactive)
#else  /* SGIMIPS */
void cm_CheckVolumeNames(interactive)
int interactive;
#endif /* SGIMIPS */
{
    register struct cm_volume *volp;
    register struct cm_scache *scp;
    register struct cm_server *serverp;
    register long i;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */

    icl_Trace0(cm_iclSetp, CM_TRACE_CHECKVOLNAMES);
    lock_ObtainWrite(&cm_volumelock);
    for (i = 0; i < VL_NVOLS; i++) {
	for (volp = cm_volumes[i]; volp; volp = volp->next) {
	    volp->refCount++;
	    lock_ReleaseWrite(&cm_volumelock);
	    lock_ObtainWrite(&volp->lock);
	    /*
	     * The VL_RECHECK flag will cause cm_GetVolume to refresh its
	     * contents; the VL_NEEDRPC flag will cause some cm_GetSCache to
	     * attempt an RPC to this volume next time, in order to check
	     * volume-version advances.
	     */
	    volp->states |= VL_RECHECK;
	    if (volp->volnamep) {
		osi_Free(volp->volnamep, strlen(volp->volnamep)+1);
		volp->volnamep = (char *) 0;
	    }
	    /* Cause an RPC for backup or interactive use. */
	    /* Essentially, let periodic R/Os go without forced updates. */
	    if ((volp->states & VL_RO) != 0 &&
		    (interactive || ((volp->states & VL_BACKUP) != 0))) {
		volp->states |= VL_NEEDRPC;
	    }
	    lock_ReleaseWrite(&volp->lock);
	    lock_ObtainWrite(&cm_volumelock);
	    volp->refCount--;
	}
    }
    lock_ReleaseWrite(&cm_volumelock);

    /* 
     * Next ensure all mt points are re-evaluated 
     */
    lock_ObtainWrite(&cm_scachelock);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
	    /* 
	     * Here we have scachelock held, and a reference to scp, which 
	     * we'll have to hold.  ocp, if non-null, has an extra reference 
	     */
#ifdef SGIMIPS
	    s = VN_LOCK(SCTOV(scp));
            if ( SCTOV(scp)->v_flag & VINACT ) {
                osi_assert(CM_RC(scp) == 0);
                VN_UNLOCK(SCTOV(scp), s);
                continue;
            }
            CM_RAISERC(scp);
            VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */
	    CM_HOLD(scp);
#endif /* SGIMIPS */
	    lock_ReleaseWrite(&cm_scachelock);
	    lock_ObtainWrite(&scp->llock);
	    scp->states &= ~SC_MVALID;		/* re-eval basic mt pts */
	    /* 
	     * Now drop this one and get to the next 
	     */
	    lock_ReleaseWrite(&scp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);
	}
    }
    lock_ReleaseWrite(&cm_scachelock);

    /*
     * For all servers, clear SR_GOTAUXADDRS flag
     */
    lock_ObtainRead(&cm_serverlock);

    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    if (!(serverp->states & SR_GOTAUXADDRS)) {
		continue;
	    }
	    /* Force additional addresses to be fetched from the FLDB in
	     * the case where incomplete server information is returned in
	     * a vldbentry structure.
	     */

	    lock_ReleaseRead(&cm_serverlock);
	    lock_ObtainWrite(&serverp->lock);

	    serverp->states &= ~(SR_GOTAUXADDRS);

	    lock_ReleaseWrite(&serverp->lock);
	    lock_ObtainRead(&cm_serverlock);
	}
    }
    lock_ReleaseRead(&cm_serverlock);
}

/* 
 * This function is called with a new process with no lock held. 
 */
static void AuxFlushActiveScaches(void *arg)
{
    struct cm_scache *scp = (struct cm_scache *)arg;
    struct cm_volume *volp;

    icl_Trace0(cm_iclSetp, CM_TRACE_FLUSHACTIVESCACHES);

    if (scp->states & SC_RO) {
        /* In general, we should not be here in the first place */
        /* osi_assert(serverp); */
        goto out;
    }
    
    /* 
     * Really cannot send dirty back to the server even in a TSR-CRASHMOVE mode
     */
    if (volp = scp->volp) {/* get volp first */
	lock_ObtainRead(&volp->lock);
	if (volp->states & VL_TSRCRASHMOVE) {
	    lock_ReleaseRead(&volp->lock);
	    goto out;
	}
	lock_ReleaseRead(&volp->lock);
    } else { /* should NOT be here, really */
	goto out;
    }

    if (scp->modChunks > 0 || (scp->m.ModFlags & CM_MODTRUNCPOS)) {
	(void) cm_SyncDCache(scp, 0, (osi_cred_t *) 0);
	cm_ConsiderGivingUp(scp);
    }
    if (cm_NeedStatusStore(scp)) {
	(void) cm_SyncSCache(scp);
    }

out:
    /* release this entry (held by tpq_ParSet caller) */
    cm_PutSCache(scp);
}

/*
 * Keep flocked files alive on the server and do periodic delayed writes
 * on modified data (i.e. scp->modChunks) and status (i.e. SC_MODMASK)
 *
 * Each scache entry will be separately scheduled with a 15 second grace 
 * period at high priority.
 */
/* ARGSUSED */
static void FlushActiveScaches(void *unused1, void *unused2)
{
    register struct cm_scache *scp;
    register int i;
    int opCount;
    void *parsetp;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */


    /* create a tpq parallel set */
    parsetp = tpq_CreateParSet(cm_auxThreadPoolHandle);
    osi_assert(parsetp != NULL);

    lock_ObtainWrite(&cm_scachelock);
    opCount = 0;
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
	    if (((scp->states & SC_RO) == 0) && 
		scp->modChunks > 0 || cm_NeedStatusStore(scp)) {
	         /* Only used as a hint only -- no read lock first */
#ifdef SGIMIPS 
		 s = VN_LOCK(SCTOV(scp));
                 if ( SCTOV(scp)->v_flag & VINACT ) {
                     osi_assert(CM_RC(scp) == 0);
                     VN_UNLOCK(SCTOV(scp), s);
                     continue;
                 }
                 CM_RAISERC(scp);
                 VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */
	         CM_HOLD(scp);	/* will be released later */
#endif /* SGIMIPS */
		 (void) tpq_AddParSet(parsetp, AuxFlushActiveScaches,
				     (void *)scp, TPQ_HIGHPRI, 15 /*seconds*/);
		 opCount++;
	    }
	}
    }
    lock_ReleaseWrite(&cm_scachelock);

    icl_Trace1(cm_iclSetp, CM_TRACE_FLUSHACTIVESUBCALLS, ICL_TYPE_LONG,
	       opCount);
    tpq_WaitParSet(parsetp);
}


/* 
 * This function is to ensure that the cache info file is up-to-date. 
 */
/* ARGSUSED */
static void WriteThroughDSlots(void *unused1, void *unused2) 
{
    register struct cm_dcache *dcp;
    register long i;
    int wroteEntry;
    struct cm_fheader hdr;
    
    icl_Trace0(cm_iclSetp, CM_TRACE_WTHRUDSLOTS);
    wroteEntry = 0;
    lock_ObtainWrite(&cm_dcachelock);
    for (i = 0; i < cm_cacheFiles; i++) {
	dcp = cm_indexTable[i];
	if (cm_indexFlags[i] & DC_IFENTRYMOD) {
	    cm_indexFlags[i] &= ~DC_IFENTRYMOD;
	    cm_WriteDCache(dcp, 1);
	    wroteEntry = 1;
	}
    }
    if (!wroteEntry && !cm_diskless) {
	/* make sure we write something periodically, to keep mtime
	 * on cache file up-to-date.  During crash recovery, mtime
	 * indicates time system went down, and is used by cache
	 * scan code to determine if a chunk was written too near the
	 * time of the crash to be trusted.
	 */
	hdr.magic = DC_FHMAGIC;
	hdr.firstCSize = cm_firstcsize;
	hdr.otherCSize = cm_othercsize;
	osi_Seek(cm_cacheFilep, 0);
	osi_Write(cm_cacheFilep, (char *) &hdr, sizeof(hdr));
    }
    lock_ReleaseWrite(&cm_dcachelock);
}

static void FlushQueuedServerTokens(void *arg)
{
    struct cm_server *serverp = (struct cm_server *)arg;

    /* 
     * The following function will check whether the server is in TSR mode; 
     * If true, could not perform the flush work for now.
     */    
    cm_FlushQueuedServerTokens(serverp);
}

/* 
 * Periodic management daemon, called every minute with a 30 second grace period.
 * Each cm_FlushQueuedServerTokens will be queued separately at high priority
 * with a 30 second grace period.
 */
/* EXPORT */
#ifdef SGIMIPS
void cm_ServerTokenMgt(void)
#else  /* SGIMIPS */
void cm_ServerTokenMgt()
#endif /* SGIMIPS */
{
    register struct cm_server *serverp;
    int i;
    int opCount;
    void *parsetp;

    /* create a tpq parallel set */
    parsetp = tpq_CreateParSet(cm_auxThreadPoolHandle);
    osi_assert(parsetp != NULL);

    lock_ObtainRead(&cm_serverlock);
    cm_needServerFlush = 0;
    opCount = 0;
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    /* queue up individual ops */
	    (void) tpq_AddParSet(parsetp, FlushQueuedServerTokens,
				 (void *)serverp, TPQ_HIGHPRI, 30 /*seconds*/);
	    opCount++;
	}
    }
    lock_ReleaseRead(&cm_serverlock);

    /* OK, we can wait for these dudes now */
    icl_Trace1(cm_iclSetp, CM_TRACE_STKNMGMT, ICL_TYPE_LONG, opCount);
    tpq_WaitParSet(parsetp);
    return;
}

/*
 * function scheduled for each volume that needs updating 
 */
static void AuxRenewLazyReps(void *arg)
{
    struct cm_volume *volp = (struct cm_volume *)arg;

    icl_Trace0(cm_iclSetp, CM_TRACE_RENEWLAZYREPS);
    (void) CheckOnVolRoot(volp);
    cm_PutVolume(volp);
}

/* 
 * Consider doing lazy-replica processing.  Keep all lazy-rep volumes working. 
 */
/* ARGSUSED */
static void RenewLazyReps(void *unused1, void *unused2)
{
    register struct cm_volume *volp;
    register u_long vvage, grace, totIv, i;
    u_long now;
    int opCount = 0;
    void *parsetp;

    /* create a tpq parallel set */
    parsetp = tpq_CreateParSet(cm_auxThreadPoolHandle);
    osi_assert(parsetp != NULL);

    now = osi_Time();
    icl_Trace0(cm_iclSetp, CM_TRACE_MAJORRENEW);
    lock_ObtainWrite(&cm_volumelock);
    for (i = 0; i < VL_NVOLS; ++i) {
	for (volp = cm_volumes[i]; volp; volp = volp->next) {
	    if (((volp->states & (VL_LAZYREP|VL_HISTORICAL|VL_RECHECK))
		 == VL_LAZYREP) && volp->timeWhenVVCurrent != 0) {
	        volp->refCount++;
		lock_ReleaseWrite(&cm_volumelock);
		lock_ObtainWrite(&volp->lock);
		vvage = now - volp->timeWhenVVCurrent;
		/* 
		 * Calculate how often to probe 
		 */
		if (volp->maxTotalLatency > 40*60) {
		    /* 
		     * Ten tries in last 1/4 of interval 
		     */
		    grace = volp->maxTotalLatency / 40;
		    totIv = grace * 10;
		} else {
		    grace = 60;			/* seconds per minute */
		    totIv = grace * 10;
		    if (totIv >= volp->maxTotalLatency) 
			totIv = volp->maxTotalLatency;
		}
		/* 
		 * Probe if in last quarter of period or beyond. 
		 */
		if (vvage >= (volp->maxTotalLatency - totIv)) {
		    /* 
		     * But not too often 
		     */
		    if ((volp->lastProbeTime + grace) < now) {
			volp->refCount++;
			icl_Trace4(cm_iclSetp, CM_TRACE_RENEWWORK,
				   ICL_TYPE_LONG, AFS_hgetlo(volp->volume),
				   ICL_TYPE_LONG, vvage,
				   ICL_TYPE_LONG, volp->maxTotalLatency,
				   ICL_TYPE_LONG, totIv);
			/* update this now so we can avoid recalling next time
			 * schedule individual vol with 30 sec grace periods.
			 */
			volp->lastProbeTime = osi_Time();

			(void) tpq_AddParSet(parsetp, AuxRenewLazyReps,
				 (void *)volp, TPQ_MEDPRI, 30 /*seconds*/);
			opCount++;
		    }
		}
		lock_ReleaseWrite(&volp->lock);
		lock_ObtainWrite(&cm_volumelock);
		volp->refCount--;
	    } /* if */
	} /* for */
    } /* for */
    lock_ReleaseWrite(&cm_volumelock);

    /* OK, we can wait for these dudes now */
    icl_Trace1(cm_iclSetp, CM_TRACE_RENEWSUBS, ICL_TYPE_LONG, opCount);
    tpq_WaitParSet(parsetp);
}


/*
 *   Get the root scache for this fileset 
 */
#ifdef SGIMIPS
static int CheckOnVolRoot(struct cm_volume *volp)
#else  /* SGIMIPS */
static int CheckOnVolRoot(volp)
    struct cm_volume *volp;
#endif /* SGIMIPS */
{
    afsFid fid;
    struct cm_rrequest rreq;
    struct cm_scache *rootscp;

    icl_Trace0(cm_iclSetp, CM_TRACE_CHECKONVOLROOT);
    fid.Cell = volp->cellp->cellId;
    fid.Volume = volp->roVol;	/* always want the R/O version */

    cm_InitUnauthReq(&rreq);
    cm_SetConnFlags(&rreq, CM_RREQFLAG_NOVOLWAIT);
    rootscp = cm_GetRootSCache(&fid, &rreq);
    if (!rootscp)
	return ENOENT;
    cm_PutSCache(rootscp);
    return 0;
}


/* 
 * Every hour, make sure to keep alive any read-only files that are from lazily
 * replicated volumes. 
 */
/* ARGSUSED */
static void cm_RefreshKeepAlives(void *unused1, void *unused2)
{
    register struct cm_scache *scp;
    register int i;
    afsFids *fidsp;
#ifdef SGIMIPS
    register unsigned32  curFid;
    unsigned32 spare4, spare5;
#else  /* SGIMIPS */
    register unsigned long curFid;
    unsigned long spare4, spare5;
#endif /* SGIMIPS */
    int LowPriCount, LowPriToo, TakeMe, matchCount, forceConn;
    register int ix;
    unsigned long Now, NextRun, FakeNext;
    struct cm_volume *volp;
    struct cm_server *repp;
    long code;
    struct cm_conn *repconn;
    struct cm_rrequest reprq;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */


    icl_Trace0(cm_iclSetp, CM_TRACE_REFRESHKEEPALIVES);
    /*
     * This code makes multiple iterations over the scache, looking for 
     * keep-alives that must be sent and piggybacking keep-alives that may be 
     * sent in the same packet.
     *
     * Each iteration works as follows:
     * (a) initial pass (LowPriToo == 0) looking for an initial to-be-expired 
     *     entry  and at the same time all to-be-expired entries  whose KA is 
     *     to be sent to the same server, up to the size limit on the packet 
     *     to be sent; <<all iterations terminate here if nothing needs to be 
     *     sent>>
     * (b) if any room left in the packet, a second pass (LowPriToo != 0) 
     *     looking for any additional not-yet-to-be-expired entries whose KA 
     *     can be sent to that server;
     * (c) send the packet to the selected rep server;
     * (d) make a pass through the scache updating it according to the results
     *     of the call.
     *
     * In addition, we count the possible not-yet-to-be-expired entries in pass
     * (a) so that pass (b) doesn't always piggyback the first few such ones.  
     * Rather, we compute a dithered starting point for pass(b) in LowPriCount.
     * The code for passes (a) and (b) is, unfortunately, intertwined.  
     * Unwinding it would probably make it a bit clearer but a good deal 
     * larger. I could be wrong here. This all works fine if the rep-server RPC
     * succeeded. If it failed, though, this algorithm lies about the stored 
     * result so that it can terminate.
     */
    fidsp = (afsFids *) osi_AllocBufferSpace();
    for (;;) {
	repp = NULL;
	curFid = LowPriCount = LowPriToo = 0;
	Now = osi_Time();
	NextRun = Now + 3600+1200;	/* allow for some timing slop here */
	for (;;) {   /* choosing a server and adding to the current packet */
	    lock_ObtainWrite(&cm_scachelock);
	    for (i = 0; i < SC_HASHSIZE; ++i) {
		for (scp = cm_shashTable[i]; scp; scp = scp->next) {
		    if (scp->states & SC_RO) {
#ifdef SGIMIPS
                        s = VN_LOCK(SCTOV(scp));
                        if ( SCTOV(scp)->v_flag & VINACT ) {
                                osi_assert(CM_RC(scp) == 0);
                                VN_UNLOCK(SCTOV(scp), s);
                                continue;
                        }
                        CM_RAISERC(scp);
                        VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */

			CM_HOLD(scp);
#endif /* SGIMIPS */
#ifdef AFS_SUNOS5_ENV
			/*
			 * Use ref count also, since files can be mapped after
			 * they're closed.  But don't bother on directories,
			 * since they can be held in the dname hash table.
			 */
			if (CM_RC(scp) == 1)
			    cm_CheckOpens(scp, 1);
			if (scp->opens <= 0 &&
			    (cm_vType(scp) == VDIR || CM_RC(scp) <= 1))
			    continue;
#else /* AFS_SUNOS5_ENV */
			if (scp->opens <= 0)
			    continue;
#endif /* AFS_SUNOS5_ENV */

			lock_ReleaseWrite(&cm_scachelock);
			volp = cm_GetVolume(&scp->fid,(struct cm_rrequest *)0);
			/*
			 * Check to make sure that we really have found this volume.
			 */
			if (volp == NULL) {
			    lock_ObtainWrite(&cm_scachelock);
			    CM_RELE(scp);
			    continue;
			}
			lock_ObtainRead(&volp->lock);
			if ((volp->states & VL_LAZYREP) != 0
			    && volp->repHosts[0] != NULL) {
			    /* OK, some kind of replication, and a repserver
			     * we can talk to
			     */
			    if (repp == NULL) {	/* and we're in first pass */
				if (scp->kaExpiration < NextRun) {
				    /* pick a rep server that's not down now */
				    for (ix = 0; ix < CM_VL_MAX_HOSTS && 
					 volp->repHosts[ix] != NULL; ++ix) {
					if ((volp->repHosts[ix]->states & 
					     SR_DOWN) == 0) {
					    repp = volp->repHosts[ix];
					    break;
					}
				    }
				    if (repp == NULL) 
					repp = volp->repHosts[0];
				    icl_Trace4(cm_iclSetp, CM_TRACE_ADD_REFR,
					       ICL_TYPE_LONG, 9,
					       ICL_TYPE_LONG, curFid,
					       ICL_TYPE_POINTER, repp,
					       ICL_TYPE_FID, &scp->fid);
				    fidsp->afsFids_val[curFid] = scp->fid;
				    ++curFid;
				} else 
				    ++LowPriCount;
			    } else {	/* could be either pass */
				for (ix = 0; ix < CM_VL_MAX_HOSTS && 
				     volp->repHosts[ix] != NULL; ++ix) {
				    if (repp == volp->repHosts[ix]) {
					TakeMe = 0;
					if (scp->kaExpiration < NextRun) {
					    if (LowPriToo == 0) 
						TakeMe = 1;
					} else {
					    if (LowPriToo) {
						if (LowPriCount > 0) 
						    --LowPriCount;
						else 
						    TakeMe = 1;
					    } else 
						++LowPriCount;
					}
					if (TakeMe) {
					    icl_Trace4(cm_iclSetp, CM_TRACE_ADD_REFR,
						       ICL_TYPE_LONG, LowPriToo,
						       ICL_TYPE_LONG, curFid,
						       ICL_TYPE_POINTER, repp,
						       ICL_TYPE_FID, &scp->fid);
					    fidsp->afsFids_val[curFid] = scp->fid;
					    ++curFid;
					}
					break;
				    }
				}
			    }
			}
			lock_ReleaseRead(&volp->lock);
			cm_PutVolume(volp);
			lock_ObtainWrite(&cm_scachelock);
			CM_RELE(scp);
			if (curFid == AFS_BULKMAX) break; /* packet's full */
		    }
		}
		if (curFid == AFS_BULKMAX) 
		    break;			/* packet's full */
	    }
	    lock_ReleaseWrite(&cm_scachelock);
	    /* 
	     * If nothing to add, or no room, or already did low-pri, or no
	     * low-pri to add, break & send the packet; else make a second pass
	     * trying to fill out the packet.
	     */
	    if (curFid == 0 || curFid == AFS_BULKMAX
		|| LowPriToo != 0 || LowPriCount == 0) 
		break;
	    LowPriToo = 1;
	    /* 
	     * We have LowPriCount possible guys to take along.
	     * Diminish our dither by the number of slots we could still fill.
	     * In the second pass, LowPriCount is the number of eligible
	     * entries to skip before starting to piggyback along.
	     */
	    LowPriCount -= (AFS_BULKMAX - curFid);
	    if (LowPriCount > 0) 
		LowPriCount = ((int) (Now & 0xffff)) % LowPriCount;
	}
	if (curFid == 0 || repp == NULL) 
	    break;	/* ALL DONE */
	LowPriCount = LowPriToo = 0;
	fidsp->afsFids_len = curFid;	/* Send the rep-server packet */
	cm_InitUnauthReq(&reprq);	/* using unauthen binding */
	forceConn = 1;
	do {
	    repconn = cm_ConnByHost(repp, MAIN_SERVICE(SRT_REP),
				    &fidsp->afsFids_val[0].Cell, &reprq,
				    forceConn);
	    forceConn = 0;
	    icl_Trace3(cm_iclSetp, CM_TRACE_CALLING_REFRESH,
		       ICL_TYPE_POINTER, repconn,
		       ICL_TYPE_LONG, curFid,
		       ICL_TYPE_FID, &fidsp->afsFids_val[0]);
	    if (repconn) {
		Now = osi_Time();
		osi_RestorePreemption(0);
		code = (int32) REP_KeepFilesAlive(repconn->connp, fidsp, curFid,
						  0, /*could be REP_FLAG_EXECUTING */
						  0, 0, 0, &spare4, &spare5);
		osi_PreemptionOff();
		code = osi_errDecode(code);
		icl_Trace2(cm_iclSetp, CM_TRACE_REFRESHCALLED,
			   ICL_TYPE_LONG, curFid, ICL_TYPE_LONG, code);
	    } else 
		code = -1;
	} while (cm_Analyze(repconn, code, &fidsp->afsFids_val[0], &reprq,
			    NULL, -1, Now));

	if (code) 
	    /* to get past the rest of the passes */
	    FakeNext = Now + 3600+1200+300;

	if (cm_RPCError(code))
	    (void) cm_ServerDown(repp, NULL, code);
	else if (repp->states & SR_DOWN) {
	    cm_PrintServerText("dfs: replication server ", repp, " back up\n");
	    lock_ObtainWrite(&repp->lock);
	    repp->states &= ~SR_DOWN;
	    lock_ReleaseWrite(&repp->lock);
	}
	/* 
	 * Apply the results to the scache. 
	 */
	matchCount = curFid;
	lock_ObtainWrite(&cm_scachelock);
	for (i = 0; i < SC_HASHSIZE; ++i) {
	    for (scp = cm_shashTable[i]; scp; scp = scp->next) {
		if (scp->states & SC_RO) {
#ifdef SGIMIPS
                        s = VN_LOCK(SCTOV(scp));
                        if ( SCTOV(scp)->v_flag & VINACT ) {
                                osi_assert(CM_RC(scp) == 0);
                                VN_UNLOCK(SCTOV(scp), s);
                                continue;
                        }
                        CM_RAISERC(scp);
                        VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */
		    CM_HOLD(scp);
#endif /* SGIMIPS */
#ifdef AFS_SUNOS5_ENV
			/*
			 * Use ref count also, since files can be mapped after
			 * they're closed.  But don't bother on directories,
			 * since they can be held in the dname hash table.
			 */
			if (CM_RC(scp) == 1)
			    cm_CheckOpens(scp, 1);
			if (scp->opens <= 0 &&
			    (cm_vType(scp) == VDIR || CM_RC(scp) <= 1))
			    continue;
#else /* AFS_SUNOS5_ENV */
			if (scp->opens <= 0)
			    continue;
#endif /* AFS_SUNOS5_ENV */

		    lock_ReleaseWrite(&cm_scachelock);
		    volp = cm_GetVolume(&scp->fid, (struct cm_rrequest *) 0);
		    /*
		     * Check to make sure that we really have found this volume.
		     */
		    if (volp == NULL) {
			lock_ObtainWrite(&cm_scachelock);
			CM_RELE(scp);
			continue;
		    }
		    if ((volp->states & VL_LAZYREP) != 0
			&& volp->repHosts[0] != NULL) {
			lock_ObtainRead(&volp->lock);
			for (ix = 0; 
			    ix < CM_VL_MAX_HOSTS && volp->repHosts[ix]; ++ix) {
			    if (repp == volp->repHosts[ix]) {
				for (ix = 0; ix < curFid; ++ix) {
				    if (bcmp((caddr_t)&scp->fid,
					     (caddr_t)&fidsp->afsFids_val[ix], 
					     sizeof(afsFid)) == 0) {
					icl_Trace3(cm_iclSetp, CM_TRACE_ADD_EXPR,
						   ICL_TYPE_POINTER, scp,
						   ICL_TYPE_FID, &scp->fid,
						   ICL_TYPE_LONG, scp->kaExpiration);
					scp->kaExpiration = code ?
					    FakeNext :
					    (Now + volp->reclaimDally);
					--matchCount;
					break;
				    }
				}
				break;
			    }
			}
			lock_ReleaseRead(&volp->lock);
		    }
		    cm_PutVolume(volp);
		    lock_ObtainWrite(&cm_scachelock);
		    CM_RELE(scp);
		    if (matchCount <= 0) 
			break;
		}
	    }
	    if (matchCount <= 0) 
		break;
	}
	lock_ReleaseWrite(&cm_scachelock);
    }
    osi_FreeBufferSpace((struct osi_buffer *) fidsp);
    return;
}

/* 
 * Called to ensure that client conns are cleaned up when the xuser is zapped.
 * We iterate through all conns, those whose xcreds for the same pag are 
 * missing.
 */
/* ARGSUSED */
static void GCOldConn(void *unused1, void *unused2)
{
    register struct cm_server *serverp;
    register struct cm_tgt **tpp, *tgtp = NULL;
    register int i;
    long now;

    /* GC those expirated cm_tgts first */
    now = osi_Time();
    /*
     * NOTE: both FreeTGTList and cm_tgt are under the same 'cm_tgtlock'
     */
    lock_ObtainWrite(&cm_tgtlock);
    for (tpp = &cm_tgtList; tgtp = *tpp;) {
         if (tgtp->tgtTime < now) {
	     *tpp = tgtp->next;
	     tgtp->next = FreeTGTList; /* GC this one */
	     icl_Trace4(cm_iclSetp, CM_TRACE_GC_TGT,
			ICL_TYPE_POINTER, tgtp,
			ICL_TYPE_LONG, tgtp->pag,
			ICL_TYPE_LONG, tgtp->tgtTime,
			ICL_TYPE_LONG, tgtp->tgtFlags);
	     FreeTGTList = tgtp;
	 } else
	     tpp = &tgtp->next;
    }
    lock_ReleaseWrite(&cm_tgtlock);

    lock_ObtainRead(&cm_serverlock);
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	     /* Go ahead, we don't GC servers. */
	     lock_ReleaseRead(&cm_serverlock);
	     cm_DeleteConns(serverp);
	     lock_ObtainRead(&cm_serverlock);
	}
    }
    lock_ReleaseRead(&cm_serverlock);

    /* 
     * Garbage collect expired creds installed in the NFS/DFS gateway
     * Don't assert for p to be non-nil. If its nil, its an internal error and 
     * we will SEGV anyway 
     */
    if (osi_nfs_gateway_loaded)
	(*osi_nfs_gateway_gc_p)();
}


/*
 * Call the function that will resync the clock with servers.  This 
 * function is also used to perform the keep-alive function for the
 * client on each server.  This function will adjust the reschedule
 * interval.
 */
static void PingServers(void *arg, void *opHandle)
{
    unsigned int newInterval;

    icl_Trace0(cm_iclSetp, CM_TRACE_PINGSERVERS);
    newInterval = cm_PingServers(NULL, CYCLE_TIME);
    if (newInterval == 0)	/* a 0 would mean "don't reschedule" */
	newInterval = 5;	/* don't want to get out of hand */
    if (newInterval == 0x7fffffff)
	newInterval = 60;	/* don't really want to wait that long */
    /* reset the reschedule interval */
    tpq_SetRescheduleInterval(opHandle, newInterval);
}

#ifdef SGIMIPS
/*
 * Call the function that will sync buffers for the CacheItems file to disk.
 */
static void SyncCacheItems(void *arg, void *opHandle)
{
    struct vnode *vp;
    int code;

    vp = cm_cacheFilep->vnode;
    lock_ObtainWrite(&cm_dcachelock);
    VOP_RWLOCK(vp, VRWLOCK_WRITE);
    VOP_FLUSH_PAGES(vp, 0, ((cm_cacheFiles * sizeof(struct cm_fcache)) + 
	            sizeof(struct cm_fheader)) - 1, 0, FI_NONE, code);
    VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
    lock_ReleaseWrite(&cm_dcachelock);
}
#endif /* SGIMIPS */

/*
 * This function is the same as the previous except that it does not
 * adjust its reschedule interval.
 */
#define PINGLONGTIME 0x7fffffff
/* ARGSUSED */
static void PingServersNoAdjust(void *unused1, void *unused2)
{
    icl_Trace0(cm_iclSetp, CM_TRACE_PINGSERVERSNA);
    (void) cm_PingServers(NULL, PINGLONGTIME);
}
#undef PINGLONGTIME

/*
 * Call the function that pings down servers.  This function
 * will adjust its reschedule interval based on what is returned
 * by cm_CheckDownServers().
 */
static void CheckDownServers(void *arg, void *opHandle)
{
    unsigned int newInterval;

    icl_Trace0(cm_iclSetp, CM_TRACE_CHECKDOWN);
#ifdef SGIMIPS
    newInterval = (unsigned int) cm_CheckDownServers(NULL, CYCLE_TIME);
#else  /* SGIMIPS */
    newInterval = cm_CheckDownServers(NULL, CYCLE_TIME);
#endif /* SGIMIPS */
    if (newInterval == 0)	/* a 0 would mean "don't reschedule" */
	newInterval = 5;	/* don't want to get out of hand */
    if (newInterval == 0x7fffffff)
	newInterval = 60;	/* don't really want to wait that long */

    /* reset the reschedule interval */
    tpq_SetRescheduleInterval(opHandle, newInterval);
}


/*
 * ReviveServerAddrs() -- ping all up servers on addresses marked down
 */
static void ReviveServerAddrs(void *arg, void *opHandle)
{
    /* Wrapper function calls the real thing */
    cm_ReviveAddrsForServers();
}

/*
 * ResetServerAuthn() -- reset cached authn bounds for all file-servers
 */
static void ResetServerAuthn(void *arg, void *opHandle)
{
    /* Wrapper function calls the real thing */
    cm_ResetAuthnForServers();
}

/*
 * stubs needed to get the arguments to match */
static void CheckVolumeNames(void *arg, void *opHandle)
{
    cm_CheckVolumeNames(0);
}

/* ARGSUSED */
static void ServerTokenMgt(void *unused1, void *unused2)
{
    cm_ServerTokenMgt();
}
