/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_subr.c,v 65.15 1999/04/27 18:06:45 ethan Exp $";
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
#include <dcedfs/rep_proc.h>		/* Replication API */
#include <dcedfs/dacl.h>
#include <dcedfs/direntry.h>
#include <dcedfs/vol_errs.h>
#include <dcedfs/osi_device.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_volume.h>
#include <cm_conn.h>
#include <cm_vcache.h>
#include <cm_aclent.h>
#include <cm_dnamehash.h>
#ifdef AFS_OSF_ENV
#include <kern/mfs.h>
#endif /* AFS_OSF_ENV */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_subr.c,v 65.15 1999/04/27 18:06:45 ethan Exp $")

/*static*/ struct lock_data cm_ftxtlock;	/* flush text lock */


/* return an open token for a file being deleted, if possible */
#ifdef SGIMIPS
void cm_GetReturnOpenToken(
			   register struct cm_scache *adscp,
  			   char *anamep,
  			   afs_hyper_t *aIDp)
#else  /* SGIMIPS */
void cm_GetReturnOpenToken(adscp, anamep, aIDp)
  register struct cm_scache *adscp;
  char *anamep;
  afs_hyper_t *aIDp;
#endif /* SGIMIPS */
{
    afsFid tfid;
    register long code;
    struct cm_scache *tscp;

    /* init to zero in case we come out early */
    AFS_hzero(*aIDp);

    /* consult the name lookup cache first, to see if we can even
     * find this dude
     */
    code = nh_lookup(adscp, anamep, &tfid);
    if (code || tfid.Vnode == 0) return;	/* failed, or doesn't exist */

    /* otherwise, we have the fid here */
    lock_ObtainWrite(&cm_scachelock);
    tscp = cm_FindSCache(&tfid);
    lock_ReleaseWrite(&cm_scachelock);
    if (!tscp) return;

    /* now we have a vnode, and so we look to see if there is single
     * token ID with open tokens, and if so, we discard that token
     * and return it in *aIDp, which will cause it to be returned
     * to the file server.  Only do this if the file isn't open.
     * Now, there's an apparent race condition here,
     * since by the time we get the unlink to the server
     * someone might have opened the file again, but since
     * we atomically drop this token and check that we're not using
     * it *now*, we're fine; anyone who needs the file will first
     * end up getting a new token.
     *
     * Note that we don't need an open token because we check that the
     * file isn't open.  And the piggy-back return token only returns
     * open tokens.
     */
#ifdef AFS_SUNOS5_ENV
    if (CM_RC(tscp) == 1 && tscp->opens != 0) {
	lock_ObtainWrite(&cm_scachelock);
	cm_CheckOpens(tscp, 1);
	lock_ReleaseWrite(&cm_scachelock);
    }
#endif /* AFS_SUNOS5_ENV */
    lock_ObtainWrite(&tscp->llock);
    if (tscp->opens == 0)
	cm_ReturnSingleOpenToken(tscp, aIDp);
    lock_ReleaseWrite(&tscp->llock);

    /* finally, clean up */
    cm_PutSCache(tscp);
}


/* easy interface to check access to a particular file.
 * inRights are the desired rights (must have all)
 * returns an error code if something went wrong,
 * EACCES if we know that we don't have permission, and
 * 0 otherwise.
 */
#ifdef SGIMIPS
cm_AccessError(
  struct cm_scache *scp,
  long inRights,
  struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
cm_AccessError(scp, inRights, rreqp)
  struct cm_scache *scp;
  long inRights;
  struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    int blocked;
    int result;
    long code;
    int retryCount = 0;

    lock_ObtainRead(&scp->llock);
    for (;;) {
	code = cm_GetTokens(scp, TKN_STATUS_READ, rreqp, READ_LOCK);
	if (code) break;
	code = cm_GetAccessBits(scp, inRights, &result, &blocked,
				rreqp, READ_LOCK, &retryCount);
	if (code) break;
	if (!blocked) {
	    /* got an answer, code is 0 here */
	    if (!result) code = EACCES;
	    break;
	}
	/* otherwise we have no error, but blocked was set, we should
	 * retry after getting the status/read token again.
	 */
    }
    lock_ReleaseRead(&scp->llock);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}

/* 
 * Ensure that an ACL cache entry is loaded for a pag spec'd by rreqp.
 * Called with the scp locked in the mode specified by setLock,
 * and holding (at least) status/read tokens.
 *
 * Returns 0 if succeeds and bits are already there, in which case it
 * sets *resultp to 1.  If the bits aren't there, but we have the knowledge
 * that we're really lacking the rights, we return *resultp to 0.
 *
 * Otherwise, if it dropped the lock, it sets *blockedp to 1, and you
 * have to retry after re-obtaining status/read tokens.
 *
 * Otherwise, returns an error code.
 *
 * blockedp can be null, in which case the corresponding info
 * won't be returned.
 */
#ifdef SGIMIPS
int cm_GetAccessBits (
  register struct cm_scache *scp,
  long inRights,
  int *resultp,
  int *blockedp,
  struct cm_rrequest *rreqp,
  int setLock,
  int *retryCountP)
#else  /* SGIMIPS */
int cm_GetAccessBits (scp, inRights, resultp, blockedp,
		      rreqp, setLock, retryCountP)
  register struct cm_scache *scp;
  long inRights;
  int *resultp;
  int *blockedp;
  struct cm_rrequest *rreqp;
  int setLock;
  int *retryCountP;
#endif /* SGIMIPS */
{
    afsFetchStatus OutStatus;
    struct cm_volume *volp;
    struct cm_server *tserverp;
    afs_token_t TokenOut;
    afsVolSync tsync;
    afs_hyper_t *vvp;
    register struct cm_conn *connp;
    long srvix;
    u_long startTime;
    register long code;
    long rights = 0;
    error_status_t st;

    volp = NULL;
    *resultp = 0;
    if (blockedp)
	*blockedp = 0;	/* by default */

    /* check minimum access rights first; if they're OK, we're fine.
     * If anyAccess does't have enough rights, use caller's own rights.
     */
    if ((inRights & scp->anyAccess) == inRights) {
	*resultp = 1;
	return 0;
    }
    
    /* scan the acl cache, ignoring pag info from expired
     * tickets (this should really be handled asychronously
     * when the ticket expires).  Doesn't drop locks.
     */
    code = cm_GetACLCache(scp, rreqp, &rights);
    if (code == 0) {
	/* found the entry in the acl cache */
	if ((rights & inRights) == inRights)
	    *resultp = 1;
	else
	    *resultp = 0;
	return 0;	/* success */
    }
    /* 
     * If we are here, we don't have enough info to verify the access 
     * rights.  Fetch it over from the server. 
     */
    lock_Release(&scp->llock, setLock);
    if (blockedp) *blockedp = 1;	/* note that we've dropped lock */
    cm_StartTokenGrantingCall();
    do {
	tserverp = 0;
	if (connp = cm_Conn(&scp->fid,  MAIN_SERVICE(SRT_FX),
			    rreqp, &volp, &srvix)) {
	    if (volp && ((volp->states & VL_LAZYREP) != 0))
		vvp = &volp->latestVVSeen;
	    else
		vvp = &osi_hZero;
	    icl_Trace1(cm_iclSetp, CM_TRACE_GABFETCHSTART,
		       ICL_TYPE_POINTER, scp);
	    startTime = osi_Time();
	    st = BOMB_EXEC("COMM_FAILURE",
			   (osi_RestorePreemption(0),
			    st = AFS_FetchStatus(connp->connp, 
						 (afsFid *)&scp->fid, vvp, 0,
						 &OutStatus, &TokenOut, 
						 &tsync),
			    osi_PreemptionOff(),
			    st));
	    code = osi_errDecode(st);
	    tserverp = connp->serverp;
	    cm_SetReqServer(rreqp, tserverp);
	    icl_Trace1(cm_iclSetp, CM_TRACE_GABFETCHEND, 
		       ICL_TYPE_LONG, code);
	} else
	    code = -1;
	if (code == 0) {
	    /* Loop if we went backward in VV-time */
	    code =  cm_CheckVolSync(&scp->fid, &tsync, volp, startTime, 
#ifdef SGIMIPS
				    (int)srvix, rreqp);
#else  /* SGIMIPS */
				    srvix, rreqp);
#endif /* SGIMIPS */
	}
    } while (cm_Analyze(connp, code, &scp->fid, rreqp, volp, srvix,
			startTime));

    /* re-obtain locks now */
    lock_ObtainWrite(&scp->llock);
    if (code != 0) {
	cm_EndTokenGrantingCall((afs_token_t *)0, (struct cm_server *) 0,
				(struct cm_volume *)0, startTime);
    }
    else {
	cm_EndTokenGrantingCall(&TokenOut, tserverp, scp->volp, startTime);
	cm_MergeStatus(scp, &OutStatus, &TokenOut, 0, rreqp);
    }

    if (volp)
	cm_EndVolumeRPC(volp);
    /* 
     * At this point an RPC has been made to fetch the ACL rights.
     * Make sure that the rights were really loaded into the ACL
     * cache. If there is a time skew on the server and the ctime 
     * of the file on the server is older than the ctime in the cache 
     * cm_MergeStatus will not set the status until it makes sure
     * that there were no other concurrent RPCs on that volume for
     * the duration of this RPC. Even so, however, there is a
     * possibility that multiple callers will enter this routine 
     * simultaneously and thus there might not be only one 
     * outstanding RPC. To account for that, sleep for a semi-random 
     * interval to give the other RPCs the chance to complete. In general
     * this retry should rarely be required.  One case that can trigger it,
     * is to fts restore -overwrite old data into a fileset.
     */
    if (code == 0) {
	if (cm_GetACLCache(scp, rreqp, &rights) != 0) {
	    struct timeval tv;
	    if (++(*retryCountP) < 200) {
		osi_GetTime(&tv);
		tv.tv_usec = ((tv.tv_usec & 3)? (tv.tv_usec & 3): 1);
		lock_ReleaseWrite(&scp->llock);
		CM_SLEEP(tv.tv_usec + (*retryCountP & 0x7));
		lock_ObtainWrite(&scp->llock);
	    } else {
		cm_printf("dfs: could not merge permissions\n");
		code = EACCES;
		*resultp = 0;
	    }
	}
    }
    /*
     * After the cm_MergeStatus, scache now should have enough info
     * to perform the access checking, provided it still has the token.
     */
    if (setLock == READ_LOCK) {
	/* doesn't drop lock */
	lock_ConvertWToR(&scp->llock);
    }
    
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}

/* 
 *
 * Routines that deal with AFS mount points
 *
 */
/* 
 * Call with llock write locked; evaluate mountRoot field from a mt pt.
 * Temporarily drops write lock when getting root scache pointer.
 */
#ifdef SGIMIPS
int
cm_EvalMountPoint(
    register struct cm_scache *scp,
    struct cm_scache *dscp,	 
    register struct cm_rrequest *rreqp) 
#else  /* SGIMIPS */
cm_EvalMountPoint(scp, dscp, rreqp)
    register struct cm_scache *scp;
    struct cm_scache *dscp;	 
    register struct cm_rrequest *rreqp; 
#endif /* SGIMIPS */
{
    register long code;
    register struct cm_volume *volp;
    struct cm_scache *rootscp;
    afsFid tfid;
    struct cm_cell *cellp;
    char *cposp, type, doRetry;
    int retryLimit = 20;	/* just in case */
    struct cm_volume *histVolp = NULL;

    icl_Trace1(cm_iclSetp, CM_TRACE_EVALMOUNTPOINT, ICL_TYPE_POINTER, scp);
retry_evaluation:
    if (scp->mountRoot && (scp->states & SC_MVALID)) 
	return 0;				/* done while racing */
    if (code = cm_HandleLink(scp, rreqp))
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    /* 
     * now link name is in scp->linkDatap, get appropriate volume info 
     */
    type = scp->linkDatap[0];
    if (cposp = strchr(&scp->linkDatap[1], ':')) {
	/* 
	 * parse cellular mt point 
	 */
	*cposp = '\0';
	code = 0;
	cellp = cm_GetCellByName(&scp->linkDatap[1]);
	if (!cellp) 
	    cm_ConfigureCell(&scp->linkDatap[1], &cellp, rreqp);
	if (cellp) {
	    volp = cm_GetVolumeByName(cposp+1, &cellp->cellId, 1, rreqp);
	} else {
	    volp = NULL;
	    code = ENODEV;
	}
	*cposp = ':';			/* put it back */
    } else 
	volp = cm_GetVolumeByName(&scp->linkDatap[1], &scp->fid.Cell, 1,rreqp);

    if (!volp) 
	return ENODEV;

    tfid.Cell = volp->cellp->cellId;
    tfid.Volume = volp->volume;
    /* 
     * don't allocate mountRoot field till we're sure we have something to put
     */
    /* Check for cellular MP, or MP in R/O (not BK) fileset */
    if (cposp || (scp->volp
		  ? ((scp->volp->states & (VL_RO|VL_BACKUP)) == VL_RO)
		  : (scp->states & SC_RO))) {
	/* Do this ONLY for regular mount points of R/W filesets where R/O exists */
	if (type == '#' && !AFS_hiszero(volp->roVol) &&
	    (volp->states & (VL_RO|VL_BACKUP)) == 0) {
	    /* 
	     * there is a READONLY volume, and we should map to it 
	     */
	    tfid.Volume = volp->roVol;/* remember volume we really want */
	    cm_PutVolume(volp);		/* release the old volume */
	    volp = cm_GetVolume(&tfid, rreqp);	/* get the new one */
	    if (!volp) {
		return ENODEV; 		/* oops, can't do it */
	    }
	}
    }

    /*
     * If we now have a mount point to the same fileset, fail with ELOOP.
     * This handles the x.backup->x.backup case as well as the /:/.rw/.rw one.
     */
    if (AFS_hsame(tfid.Volume,scp->fid.Volume) &&
	AFS_hsame(tfid.Cell,scp->fid.Cell)) {
	cm_PutVolume(volp);
	return ELOOP;
    }

    /*
     * Get the root scache for this fileset 
     */
    lock_ReleaseWrite(&scp->llock);
    rootscp = cm_GetRootSCache(&tfid, rreqp);
    if (!rootscp) {
	doRetry = 0;
	lock_ObtainWrite(&scp->llock);
	/* Well, if this is now a freshly-``historical'' volume, let's check
	 * for a fresher one, unless we're looping.
	 */
	if ((volp != histVolp) && (volp->states & VL_HISTORICAL)
	    && (--retryLimit) > 0) {
	    histVolp = volp;	/* we'll do only pointer comparisons later */
	    lock_ObtainWrite(&volp->lock);
	    volp->states |= VL_RECHECK;
	    lock_ReleaseWrite(&volp->lock);
	    doRetry = 1;
	}
	cm_PutVolume(volp);
	if (doRetry)
	    goto retry_evaluation;
	return ENOENT;
    }
    /* now know volume's root file ID, too */
    volp->rootFid = rootscp->fid;
    lock_ObtainRead(&rootscp->llock);
    /* Check for staleness */
    code = cm_GetTokens(rootscp, TKN_OPEN_PRESERVE, rreqp, READ_LOCK);
    lock_ReleaseRead(&rootscp->llock);
    if (code) {              /* shouldn't happen! */
	cm_PutSCache(rootscp);
	lock_ObtainWrite(&scp->llock);
	cm_PutVolume(volp);
	return ENOENT;
    }
    lock_ObtainWrite(&scp->llock);

    if (scp->mountRoot == 0)
	scp->mountRoot = (afsFid *) osi_Alloc(sizeof(afsFid));
    scp->mountRoot->Cell = volp->cellp->cellId;
    scp->mountRoot->Volume = volp->volume;

    scp->states |= SC_MVALID;
    volp->mtpoint = scp->fid;		/* setup back pointer to mtpoint */
    volp->dotdot = dscp->fid;

    cm_PutVolume(volp);

    /* Update to an appropriate vnode number */
    scp->mountRoot->Vnode = rootscp->fid.Vnode;
    scp->mountRoot->Unique = rootscp->fid.Unique;
    cm_PutSCache(rootscp);
    return 0;
}


/*
 * Setup up the .. entry for the mount point's fileset.
 * The fsetscp llock should be write locked by the caller.
 */
#ifdef SGIMIPS
void cm_SetMountPointDotDot(
    struct cm_scache *dscp,	/* scp of directory representing .. */
    struct cm_scache *mpscp,	/* scp of the mountpoint */
    struct cm_scache *fsetscp)	/* scp of fileset pointed to by mpscp */
#else  /* SGIMIPS */
void cm_SetMountPointDotDot(dscp, mpscp, fsetscp)
    struct cm_scache *dscp;	/* scp of directory representing .. */
    struct cm_scache *mpscp;	/* scp of the mountpoint */
    struct cm_scache *fsetscp;	/* scp of fileset pointed to by mpscp */
#endif /* SGIMIPS */
{
    if (fsetscp->mountRoot == (afsFid *) 0) {
	fsetscp->mountRoot = (afsFid *) osi_Alloc(sizeof(afsFid));
    }
    *fsetscp->mountRoot = dscp->fid;	
    fsetscp->volp->mtpoint = mpscp->fid;
    fsetscp->volp->dotdot = dscp->fid;
}


/*
 * Lookup .. for the root of a fileset.
 *
 * If an scache entry known to be valid turns up, put a pointer to it in scpp.
 * Otherwise, if a fid is known, put it in fidp.  In this case the caller is
 * expected to call cm_GetSCache and may decide whether to retry if that
 * function returns ESTALE.
 *
 * LOCKS: called with no locks held.
 */
#ifdef SGIMIPS
int
cm_GetMountPointDotDot(
    struct cm_scache *dscp,	/* scp of root directory */
    afsFid *fidp,		/* result fid */
    struct cm_scache **scpp,	/* result vnode */
    struct cm_rrequest *rreqp)	/* info about errors from RPC's */
#else  /* SGIMIPS */
int
cm_GetMountPointDotDot(dscp, fidp, scpp, rreqp)
    struct cm_scache *dscp;	/* scp of root directory */
    afsFid *fidp;		/* result fid */
    struct cm_scache **scpp;	/* result vnode */
    struct cm_rrequest *rreqp;	/* info about errors from RPC's */
#endif /* SGIMIPS */
{
    struct cm_vdirent *tvdp;	/* vdir entry of .. */
    struct cm_scache *tscp;	/* scp of .. */
#ifndef SGIMIPS
    afsFid tfid;		/* fid of .. */
    int code = 0;
#endif /* SGIMIPS */

    *scpp = NULL;

    lock_ObtainRead(&dscp->llock);

    /*
     * Are we mounted via the global name space?
     */
    if (tvdp = dscp->parentVDIR) {
	if (tscp = tvdp->pvp) {
	    lock_ReleaseRead(&dscp->llock);
	    cm_HoldSCache(tscp);
	    *scpp = tscp;
	    return 0;
	} else
	    lock_ReleaseRead(&dscp->llock);
	    return ENOTTY;
    }

    /*
     * There had better be a rootDotDot structure
     */
    if (dscp->rootDotDot == NULL || AFS_hiszero(dscp->rootDotDot->Volume)) {
	icl_Trace0(cm_iclSetp, CM_TRACE_LOOKUPDOTDOTFAIL);
	lock_ReleaseRead(&dscp->llock);
	return ENOTTY;
    }

    *fidp = *dscp->rootDotDot;
    lock_ReleaseRead(&dscp->llock);
    return 0;
}


/* Function to wait for scache entry to stabilize.
 * asp is write locked before and after the call
 */
#ifdef SGIMIPS
void cm_StabilizeSCache(register struct cm_scache *asp)
#else  /* SGIMIPS */
void cm_StabilizeSCache(asp)
  register struct cm_scache *asp;
#endif /* SGIMIPS */
{
    for (;;) {
	if ((asp->states & SC_STORING) || asp->storeCount > 0) {
	    /* wait for storing to clear */
	    asp->states |= SC_WAITING;
	    icl_Trace1(cm_iclSetp, CM_TRACE_STABILIZESCACHEWAIT,
		       ICL_TYPE_POINTER, asp);
	    osi_SleepW(&asp->states, &asp->llock);
	    lock_ObtainWrite(&asp->llock);
	    continue;
	}

	/* success */
	return;
    }
}

/* function to wait for dcache entry to stabilize.
 * This function can not be called as part of token revocation,
 * since it waits for the fetching flag to clear.
 *
 * Both the scache and dcache entries are write-locked before and
 * after the call.
 * It may temporarily release the scache and dcache locks
 * to avoid deadlocks.
 *
 * See cm_dcache.h for description of type values.
 */
#ifdef SGIMIPS
void cm_StabilizeDCache(
  register struct cm_scache *asp,
  register struct cm_dcache *adp,
  int type)
#else  /* SGIMIPS */
void cm_StabilizeDCache(asp, adp, type)
  register struct cm_scache *asp;
  register struct cm_dcache *adp;
  int type;
#endif /* SGIMIPS */
{
    int mask;

    /* mask of dcache bits to wait for.  If we're invalidating a file
     * completely, or destroying a chunk completely, we don't want
     * to see the chunk in fetching or storing mode.
     */
    mask = ((type == CM_DSTAB_MAKEIDLE) ?
	    (DC_DFFETCHING | DC_DFSTORING)	/* make totally idle */
	    : DC_DFSTORING);			/* fetches are ok */

    for (;;) {
	if (asp->states & SC_STORING) {		/* truncates are never OK */
	    asp->states |= SC_WAITING;
	    /* need to release the dcache lock also, to obey hierarchy */
	    icl_Trace2(cm_iclSetp, CM_TRACE_STABILIZEDCACHEWAIT1,
		       ICL_TYPE_POINTER, asp, ICL_TYPE_POINTER, adp);
	    lock_ReleaseWrite(&adp->llock);
	    osi_SleepW(&asp->states, &asp->llock);
	    lock_ObtainWrite(&asp->llock);
	    lock_ObtainWrite(&adp->llock);
	    continue;
	}

	if (adp->states & mask) {
	    /* wait for storing (and maybe fetching) to clear */
	    adp->states |= DC_DFWAITING;
	    lock_ReleaseWrite(&asp->llock);
	    icl_Trace2(cm_iclSetp, CM_TRACE_STABILIZEDCACHEWAIT2,
		       ICL_TYPE_POINTER, asp, ICL_TYPE_POINTER, adp);
	    osi_SleepW(&adp->states, &adp->llock);
	    lock_ObtainWrite(&asp->llock);
	    lock_ObtainWrite(&adp->llock);
	    continue;
	}

	/* success */
	return;
    }
}

/* 
 * call under write-lock to read link into memory 
 */
#ifdef SGIMIPS
int
cm_HandleLink(
    register struct cm_scache *scp,
    struct cm_rrequest *rreqp) 
#else  /* SGIMIPS */
cm_HandleLink(scp, rreqp)
    register struct cm_scache *scp;
    struct cm_rrequest *rreqp; 
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    register char *tp;
    char *osiFilep;
    long len, alen, code;

    /* 
     * two different formats, one for links protected 644, have a "." at the 
     * end of the file name, which we turn into a null.  Others, protected 755,
     * we add a null to the end of
     */
    if (code = cm_GetTokens(scp, TKN_READ, rreqp, WRITE_LOCK))
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */

    if (!scp->linkDatap) {
	lock_ReleaseWrite(&scp->llock);
	if (!(dcp = cm_GetDCache(scp, 0)))
	    return EIO;
	lock_ObtainWrite(&scp->llock);
	code = cm_GetDLock(scp, dcp, CM_DLOCK_READ, WRITE_LOCK, rreqp);
	if (code) {
	    cm_PutDCache(dcp);
#ifdef SGIMIPS
	    return (int)code;
#else  /* SGIMIPS */
	    return code;
#endif /* SGIMIPS */
	}
	len = dcp->f.chunkBytes;
	/* 
	 * Otherwise we have the data loaded; go for it.  Note that symlink 
	 * contents can't disappear on us unless reference count is zero,
	 * so can unlock dcache entry now.
	 */
	lock_ReleaseWrite(&dcp->llock);
	if (len > 1024) {
	    cm_PutDCache(dcp);
	    return EFAULT;
	}
#ifdef AFS_VFSCACHE
	if (!(osiFilep = cm_CFileOpen(&dcp->f.handle)))
#else
	if (!(osiFilep = cm_CFileOpen(dcp->f.inode)))
#endif
	{
	    cm_PutDCache(dcp);
	    return EIO;
	}
	if (scp->states & SC_MOUNTPOINT)
	    alen = len;			/* mt point */
	else 
	    alen = len+1;		/* regular link */
	tp = osi_Alloc(alen);		/* make room for terminating null */
#ifdef SGIMIPS
	lock_ReleaseWrite(&scp->llock); /* we might end up in XFS */
#endif /* SGIMIPS */
	code = cm_CFileRW(osiFilep, UIO_READ, tp, 0, len, osi_credp);
#ifdef SGIMIPS
	lock_ObtainWrite(&scp->llock);
#endif /* SGIMIPS */
	tp[alen-1] = 0;
	cm_CFileClose(osiFilep);
	cm_PutDCache(dcp);
	if (code != len) {
	    osi_Free(tp, alen);
	    return EIO;
	}
	/* make sure we don't have any nulls that will confuse the
	 * code that free's strlen(foo)+1 bytes.  If we do, return
	 * the symlink, since someone may want to see what's there.
	 */
	for(code = 0;code < alen-1; code++) {
	    if (tp[code] == 0) tp[code] = '-';
	}
	scp->linkDatap = tp;
    }
    return 0;
}


/* 
 *
 * RPC-specific routines to send stuff over the net
 *
 */

/*
 * Routine to allocate a buffer to transfer pipe data
 * The buffer has already been set up by the user of the pipe.
 * This routine serves to pass the buffer to the RPC stub.
 */
#ifdef SGIMIPS
void cm_BufAlloc(
     rpc_ss_pipe_state_t  pstate,
     idl_ulong_int bsize,
     idl_byte **buf,
     idl_ulong_int  *bcount)
#else  /* SGIMIPS */
void cm_BufAlloc(pstate, bsize, buf, bcount)
     void           *pstate;
     unsigned long  bsize;
     unsigned char  **buf;
     unsigned long  *bcount;
#endif /* SGIMIPS */
{
    struct pipeDState *pipeState = (struct pipeDState *)pstate;

    *buf = (unsigned char *)pipeState->bufp;
    *bcount = pipeState->bsize;

}


/*
 * Routine called on store.
 * This is the implemantion of StoreData PIPE's "pull" routine. 
 */
#ifdef SGIMIPS
void cm_CacheStoreProc(
     rpc_ss_pipe_state_t sstate,   /* in: pipe's state ptr */
     idl_byte *buf,      /* in: buffer in which to place a chunk */
     idl_ulong_int bsize,     /* in: buffer size, # of bytes to be read */
     idl_ulong_int *bcount)   /* out: number of bytes actually read */
#else  /* SGIMIPS */
void cm_CacheStoreProc(sstate, buf, bsize, bcount)
     void          *sstate;   /* in: pipe's state ptr */
     unsigned char *buf;      /* in: buffer in which to place a chunk */
     unsigned long bsize;     /* in: buffer size, # of bytes to be read */
     unsigned long *bcount;   /* out: number of bytes actually read */
#endif /* SGIMIPS */
{
    struct pipeDState *storeState = (struct pipeDState *)sstate;
    int preempting;
    long tsize;		/* bytes to transfer */
		
    /* don't try more bytes than we've got dirty in the cache */
    tsize = bsize;
    if (tsize > storeState->nbytes) tsize = storeState->nbytes;

    if (!storeState->fileP || tsize <= 0) {
	/* storemini case, send back 0 bytes */
	*bcount = 0;
	return;
    }

    preempting = osi_PreemptionOff();
    *bcount = cm_CFileRW(storeState->fileP, UIO_READ, (char *)buf, 
			storeState->offset, tsize, osi_credp);
    storeState->offset += *bcount; /* Update the offset for this dcache */
    storeState->nbytes -= *bcount; /* and bytes left */
    osi_RestorePreemption(preempting);
    return;
}

#ifdef notdef
/* 
 * Routine called on fetch; also tells people waiting for data that more has 
 * arrived.  This is the implemantion of AFS_Readdir's "push" routine. 
 */
void cm_DirFetchProc(fstate, buf, ecount)
     void          *fstate;     /* in: the pipe stream state */
     unsigned char *buf;        /* in: pointer to the buffer containing data */
     unsigned long ecount;      /* in: number of bytes to be written */
{
    struct pipeDState *fetchState = (struct pipeDState *)fstate;
    char  dirBuf[DIR_BLKSIZE];
    char *osiFilep;
    struct cm_dcache *dcp;
    register long tlen, code, namelen;
    long remaining, baseAddr, bytesInBlk;
    struct dir_minnewentry *inEntry;
    long inHdrSize = SIZEOF_DIR_MINNEWENTRY;
    char temp[32]; /* print out the first 32 chars for debug */

#if defined(AFS_AIX31_ENV)
#define cm_DIR_eStruct dir_minnewentry
    struct dir_minnewentry *outEntry, *lastp;
    long outHdrSize = SIZEOF_DIR_MINNEWENTRY;
#else
#define cm_DIR_eStruct dir_minoldentry
    struct dir_minoldentry *outEntry, *lastp;
    long outHdrSize = SIZEOF_DIR_MINOLDENTRY;
#endif

    dcp = fetchState->dcp;
    lock_ObtainWrite(&dcp->llock);
    if (ecount == 0) {
      lock_ReleaseWrite(&dcp->llock);
      return;
    }
    fetchState->scode = 0;
    osiFilep = fetchState->fileP; 

    lastp = NULL;
    remaining = (long)ecount;   /* Total bytes left to be read */
    bytesInBlk = 0;		/* bytes written to the output buffer */
    baseAddr = dcp->validPos;   /* Last valid pos in this chunk */

    inEntry = (struct dir_minnewentry *)buf; /* Assuming in standard format */
    outEntry = (struct cm_DIR_eStruct *)dirBuf;
    while (remaining > 0) {
	/* 
	 * See if the next input dir entry will fit in the output buffer.
	 * If it will not fit, pad last record to DIR_BLKSIZE bytes, and write
	 * the output buffer out.  Note: In general, this is the last step.
	 */
	/* put input record into host byte order.  Note that we don't have
	 * to explicitly convert *lastp into host byte order, since lastp
	 * points to an entry that was *inEntry at some previous point.
	 */
	inEntry->offset = ntohl(inEntry->offset);
	inEntry->inode = ntohl(inEntry->inode);
	inEntry->recordlen = ntohs(inEntry->recordlen);
	inEntry->namelen = ntohs(inEntry->namelen);
	if (inEntry->recordlen + bytesInBlk > DIR_BLKSIZE) {
 	    if (!lastp) {
	      /* Never written to outEntry, inEntry length must be bogus 
	       */
	      code = EIO;
	      goto done;
	    }
	    /* adjust the last written dir entry's record_len */
	    lastp->recordlen += (DIR_BLKSIZE-bytesInBlk); 
   	     code = cm_CFileRW(osiFilep, UIO_WRITE, dirBuf, fetchState->offset,
        	                DIR_BLKSIZE, osi_credp);
	    /* 
	     * check that write worked, not disk full or other problems 
	     */
	    if (code != DIR_BLKSIZE) {
	       code = EIO;
	       goto done;
	     }
	    /* 	
	     * wakeup those waiting for more data 
	     */
	    baseAddr += DIR_BLKSIZE;
	    fetchState->offset += DIR_BLKSIZE;
	    dcp->validPos = baseAddr;
	    if (dcp->states & DC_DFWAITING) {
	        dcp->states &= ~DC_DFWAITING;
		osi_Wakeup(&dcp->states);
	    }
	    /* 
	     * now setup new buffer to look empty 
	     */
	    bytesInBlk = 0;
	    lastp = NULL;
	    outEntry = (struct cm_DIR_eStruct *)dirBuf;
	}  /* (if ....  > DIR_BLKSIZE) */
	/* 
	 * copy out this entry from input to output buffer, with adjustments 
	 */
#if defined(AFS_AIX31_ENV)
	outEntry->recordlen = inEntry->recordlen;
#else
	outEntry->recordlen = inEntry->recordlen + (outHdrSize - inHdrSize);
#endif
	/*
	 * sanity check the length now, in case recordlen was trashed by the 
	 * RPC; otherwise, we might really trash the client.
	 */
	if (outEntry->recordlen + bytesInBlk > DIR_BLKSIZE) {
	    printf("dfs: readdir too big !\n");
	    code = EIO;
	    goto done;
	}
	outEntry->namelen = inEntry->namelen;
	outEntry->inode   = inEntry->inode +
	    (AFS_hgetlo(dcp->f.fid.Volume) << 16);
	/* 
	 * copy the entry name from input buf to output buf 
	 */
	namelen = outEntry->recordlen - outHdrSize;
	bcopy((char *)inEntry+inHdrSize, (char*)outEntry+outHdrSize, namelen);
#if defined(AFS_AIX31_ENV)
	/* 	
	 * Get next record's offset. Assuming the input direntry is standard.
	 */
	outEntry->offset = inEntry->offset;
#endif
	bytesInBlk += outEntry->recordlen;
	lastp = outEntry;               /* remember this written record */
	/* to the next dir entry */
	outEntry = (struct cm_DIR_eStruct *) ((char *)outEntry + (namelen + outHdrSize));
	inEntry  = (struct dir_minnewentry *) ((char *)inEntry + (namelen + inHdrSize));
	remaining -= (namelen + inHdrSize);  /* total # of bytes left */
    }
    /* 
     * Once we're here, we've read the whole dir chunk. We next write out 
     * what's there in the last buffer, still in memory.  Before doing this, we
     * adjust the last record's offset to point to the next chunk, since we
     * probably didn't fill this chunk up, and our returned offsets are just 
     * offsets within this file. 
     */
    if (lastp) {
#if defined(AFS_AIX31_ENV)
	lastp->offset = cm_chunktobase(dcp->f.chunk+1);
#endif
	/* 
	 * move next as seen from size to the end of the block we're writing,
	 * so that next read, even by scanners using recordlen, will hit
	 * an EOF 
	 */
	lastp->recordlen += (DIR_BLKSIZE - bytesInBlk);
    }
    if (bytesInBlk > 0) {
        code = cm_CFileRW(osiFilep, UIO_WRITE, dirBuf, fetchState->offset,
			DIR_BLKSIZE, osi_credp);
	if (code != DIR_BLKSIZE) {
	    code = EIO;
	    goto done;
	}
	baseAddr += DIR_BLKSIZE;
	/* 
	 * don't bother waking user up any more, xfr is almost done
	 * and user will get wakeup when whole enchilada is finished. 
	 */
    }
    /* 
     * remember *server* offset of next record we'll read
     */

    fetchState->offset += DIR_BLKSIZE;
    dcp->validPos = baseAddr;
    code = 0;
done:
    fetchState->scode = code;
    lock_ReleaseWrite(&dcp->llock);
    return;
}
#endif /* notdef */

/* 
 * Routine called on fetch; also tells people waiting for data that more has 
 * arrived. At this point, the desired data is readily available in "buf", 
 * we just write them (from "buf") to the chunk. 
 * 
 * This is the implementation of FetchData's "push" rotuine
 */
#ifdef SGIMIPS
void cm_CacheFetchProc(
     rpc_ss_pipe_state_t fstate,    /* in: the pipe stream state */
     idl_byte *buf,       /* in: pointer to the buffer containing data */
     idl_ulong_int ecount)     /* in: number of bytes to be written */
#else  /* SGIMIPS */
void cm_CacheFetchProc(fstate, buf, ecount)
     void          *fstate;    /* in: the pipe stream state */
     unsigned char *buf;       /* in: pointer to the buffer containing data */
     unsigned long ecount;     /* in: number of bytes to be written */
#endif /* SGIMIPS */
{
    struct pipeDState *fetchState = (struct pipeDState *)fstate;
    char *osiFilep; 
    long baseAddr;
    struct cm_dcache *dcp;
    long code;
    int preempting;

    if (ecount == 0)			/* end of string */
	return;
    /*
     * If we have gotten an error on an earlier fetch, it's probably due
     * to something like the cache partition being full.  Don't try to
     * do more fetching.
     */
    if (fetchState->scode != 0)
	return;
    preempting = osi_PreemptionOff();
    osiFilep = fetchState->fileP;
    dcp = fetchState->dcp; 
    lock_ObtainWrite(&dcp->llock);
    baseAddr = dcp->validPos; /* Last valid pos in this chunk */
    code = cm_CFileRW(osiFilep, UIO_WRITE, (char *)buf, fetchState->offset,
		      ecount, osi_credp);
    if (code != ecount) {
        fetchState->scode = EIO;
	goto done;
    }
    baseAddr += (long)ecount;
    dcp->validPos = baseAddr; 
    icl_Trace2(cm_iclSetp, CM_TRACE_CACHEFETCHPROC, ICL_TYPE_POINTER, dcp,
	       ICL_TYPE_LONG, baseAddr);
    if (dcp->states & DC_DFWAITING) {
        dcp->states &= ~DC_DFWAITING;
	osi_Wakeup(&dcp->states);
    }
    fetchState->scode = 0;
    fetchState->offset += ecount;
  done:
    lock_ReleaseWrite(&dcp->llock);
    osi_RestorePreemption(preempting);
    return;
}


/* 
 *
 * Miscellaneous simple routines
 *
 */


#ifdef SGIMIPS
int cm_CheckVolSync(
     register afsFid *fidp,
     register afsVolSync *syncp,
     struct cm_volume *involp,
     u_long startTime,
     int srvix,
     register struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
int cm_CheckVolSync(fidp, syncp, involp, startTime, srvix, rreqp)
     register afsFid *fidp;
     register afsVolSync *syncp;
     struct cm_volume *involp;
     u_long startTime;
     int srvix;
     register struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{/* Return a code usable as a DCE err code. */
    register int i, j;
    register struct cm_volume *volp;
    register struct cm_scache *scp;
    u_long oneTime, myStartTime;
    int needPutVol, needSweep, retVal, needVVUpdate, flushingMe;
    afs_hyper_t reallyNewVV, latestVVSeen;
    struct cm_rrequest treq;
#ifdef SGIMIPS64
    unsigned int cookie, nextcookie, numFIVs, repflags;
    unsigned int dum1, dum2;
    u_long Now, dum3;
#else  /* SGIMIPS64 */
    u_long cookie, nextcookie, numFIVs, repflags;
    u_long Now, dum1, dum2, dum3;
#endif /* SGIMIPS64 */
    struct fidsInVol *FIVbufp;
    struct fidInVol *fivp;
    struct cm_conn *connp;
    long code;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */

    if (!fidp || !syncp) 
	return 0;			/* don't try again */
    if (!AFS_hsame(fidp->Volume, syncp->VolID))
	return 0;			/* not talking about the same vol ID */

    /* 
     * see if the volume sync info has changed 
     */
    if (involp != NULL) {
	volp = involp;
	needPutVol = 0;
    } else {
	if (!(volp = cm_GetVolume(fidp, (struct cm_rrequest *) 0)))
	    return 0;
	needPutVol = 1;
    }
    lock_ObtainWrite(&volp->lock);
    Now = osi_Time();
    if (Now > volp->lastRepTry)		/* Don't go backwards in time */
	volp->lastRepTry = Now;
    if (!(volp->states & VL_RO)) {
	/* applies only to readonly vols */
	lock_ReleaseWrite(&volp->lock);
	if (needPutVol) 
	    cm_PutVolume(volp);
	return 0;
    }
    if (AFS_hiszero(syncp->VV) && (volp->hardMaxTotalLatency != 0)) {
	/* RO must have a valid VV */
	lock_ReleaseWrite(&volp->lock);
	if (needPutVol) 
	    cm_PutVolume(volp);
	return(VOLERR_PERS_AGEOLD);
    }
    /* 
     * Allow a 1% clock rate slew between server and us. 
     */
    if (AFS_hsame(syncp->VV, volp->latestVVSeen)) {
	code = 0;
	if ((volp->states & VL_LAZYREP) != 0) {	/* lazy replication? */
	    /* 
	     * Update our memory of when this version was last current 
	     */
	    oneTime = startTime - ((syncp->VVAge * 129)>>7) - 1;
	    if (oneTime > volp->timeWhenVVCurrent ||
		volp->onlyOneOutstandingRPC) {
		icl_Trace3(cm_iclSetp, CM_TRACE_CVSUPDATE2,
			   ICL_TYPE_HYPER, &volp->volume,
			   ICL_TYPE_LONG, volp->timeWhenVVCurrent,
			   ICL_TYPE_LONG, oneTime);
		volp->timeWhenVVCurrent = oneTime;
	    }
	    /* Check whether the data from this server is too old to use. */
	    /* (the VL_LAZYREP check also ensures that hardMax.. is nonzero) */
	    if ((volp->timeWhenVVCurrent+volp->hardMaxTotalLatency) <= Now) {
		code = VOLERR_PERS_AGEOLD;
	    }
	}
	lock_ReleaseWrite(&volp->lock);
	if (needPutVol) 
	    cm_PutVolume(volp);			/* just put it back */
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }
    needSweep = 1;
    flushingMe = 0;
    retVal = 0;
    needVVUpdate = 0;
    if ((volp->states & VL_LAZYREP) == 0) {	/* unknown replication? */
	volp->latestVVSeen = syncp->VV;
	lock_ReleaseWrite(&volp->lock);
	if (needPutVol) 
	    cm_PutVolume(volp);	/* done with it now */
	needPutVol = 0;
    } else {			/* no--use lazy/new-release replication. */
	/* 
	 * Check for this server being before us in time. 
	 */
	if (!AFS_hiszero(volp->latestVVSeen) &&
	    AFS_hcmp(syncp->VV, volp->latestVVSeen) < 0) {
	    /* 
	     * We have a real VV locally, and this volsync is earlier.  Can't 
	     * use it. Mark this server as bad for this volume, if we can, too.
	     */
	    icl_Trace3(cm_iclSetp, CM_TRACE_CVSBACK2,
		       ICL_TYPE_HYPER, &volp->volume,
		       ICL_TYPE_HYPER, &volp->latestVVSeen,
		       ICL_TYPE_HYPER, &syncp->VV);
	    lock_ReleaseWrite(&volp->lock);
	    if (needPutVol) 
		cm_PutVolume(volp);		/* done with it now */
	    return VOLERR_TRANS_VVOLD;	/* need to skip this site */
	}
	/* Here, the VV is later than what we have. */
	reallyNewVV = syncp->VV;
	needVVUpdate = 1;		/* delay updating the latestVVSeen */
	latestVVSeen = volp->latestVVSeen;	/* save before dropping lock */
	lock_ReleaseWrite(&volp->lock);
	cm_InitUnauthReq(&treq);	/* might as well use unauth */
	dum3 = 0;
	FIVbufp = (struct fidsInVol *) osi_AllocBufferSpace();
	do {
	    /*
	     * Don't do the RPC if we have nothing cached so far; we would
	     * simply make the repserver get and return a list of all files.
	     */
	    if (AFS_hiszero(latestVVSeen))
		connp = NULL;	/* let it fall into the failure case below */
	    else
		connp = cm_ConnByMHosts(volp->repHosts, MAIN_SERVICE(SRT_REP),
					&fidp->Cell, &treq, volp, NULL, NULL);
	    if (connp) {
		for (cookie = repflags = 0, code = 0;
		     !code && ((repflags & 1) == 0);
		     cookie = nextcookie) {
		    bzero((char *)FIVbufp, sizeof(*FIVbufp));
		    myStartTime = osi_Time();
		    osi_RestorePreemption(0);
		    code = REP_GetVolChangedFiles(connp->connp, &fidp->Cell, 
					  &fidp->Volume, &latestVVSeen,
					  &syncp->VV, cookie, 0, 0, 0,
					  &reallyNewVV, &nextcookie, FIVbufp, 
					  &numFIVs, &repflags, &dum1, &dum2);
		    osi_PreemptionOff();
		    code = osi_errDecode(code);
		    if (code == 0) {
			numFIVs = FIVbufp->fidsInVol_len;
			dum3 += numFIVs;

			if (numFIVs != 0) {
			    fivp = &FIVbufp->fidsInVol_val[0];
			    for (j = 0; j < numFIVs; ++j, ++fivp) {
				/* check if we're asked to flush the caller's data */
				if (fivp->Vnode == fidp->Vnode) {
				    flushingMe = 1;
				}
			    }
			    lock_ObtainWrite(&cm_scachelock);
			    for (i = 0; i < SC_HASHSIZE; i++) {
			    for (scp = cm_shashTable[i]; scp; scp = scp->next){
				if (AFS_hsame(scp->fid.Volume, fidp->Volume) &&
				    AFS_hsame(scp->fid.Cell, fidp->Cell)) {
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
				    fivp = &FIVbufp->fidsInVol_val[0];
				    for (j = 0; j < numFIVs; ++j, ++fivp) {
					if (fivp->Vnode == scp->fid.Vnode) {
					    icl_Trace4(cm_iclSetp, CM_TRACE_CVSSET5,
						       ICL_TYPE_LONG, 1,
						       ICL_TYPE_FID, &scp->fid,
						       ICL_TYPE_LONG, cm_vType(scp),
						       ICL_TYPE_POINTER, scp);
					    lock_ObtainWrite(&scp->llock);
					    cm_ForceReturnToken(scp, TKN_READ, 0);
					    lock_ReleaseWrite(&scp->llock);
					    break;
					}
				    }
				    lock_ObtainWrite(&cm_scachelock);
				    CM_RELE(scp);
				}
			    } /* for scp */
			    } /* for i */
			    lock_ReleaseWrite(&cm_scachelock);
		        } /* if (numFIVs != 0) */
		    }
		} /* for cookie */
		if (code == 0) {
		    needSweep = 0;	/* successfully swept it */
		    icl_Trace4(cm_iclSetp, CM_TRACE_CVSSET2,
			       ICL_TYPE_LONG, dum3,
			       ICL_TYPE_HYPER, &volp->volume,
			       ICL_TYPE_HYPER, &syncp->VV,
			       ICL_TYPE_HYPER, &reallyNewVV);
		}
	    } else
		code = -1;
	} while (cm_Analyze(connp, code, (afsFid *) 0, &treq, NULL, -1, myStartTime));
	osi_FreeBufferSpace((struct osi_buffer *)FIVbufp);
	if (code == 0) {
	    /* see if we've advanced even beyond the volsync number */
	    code = AFS_hcmp(syncp->VV, reallyNewVV);
	    if (code < 0) {
		/* reallyNewVV is bigger */
		retVal = CM_REP_ADVANCED_AGAIN;
	    } else if (code > 0) {
		/* even this one is too old! */
		retVal = VOLERR_TRANS_VVOLD;
	    }
	}
    }

    /* 
     * If needSweep, we're in non-lazy mode, or some repser call failed.
     * Search for all vnodes from this volume, and invalidate them (i.e.
     * clear stat'd flag)
     */
    if (needSweep != 0) {
	dum3 = 0;
	/* Assume that we're flushing the caller's data, too */
	flushingMe = 1;
	lock_ObtainWrite(&cm_scachelock);
	for (i = 0; i < SC_HASHSIZE; i++) {
	    for (scp = cm_shashTable[i]; scp; scp = scp->next) {
		if (AFS_hsame(scp->fid.Volume, fidp->Volume) &&
		    AFS_hsame(scp->fid.Cell, fidp->Cell)) {
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
		    icl_Trace4(cm_iclSetp, CM_TRACE_CVSSET5,
			       ICL_TYPE_LONG, 2,
			       ICL_TYPE_FID, &scp->fid,
			       ICL_TYPE_LONG, cm_vType(scp),
			       ICL_TYPE_POINTER, scp);
		    lock_ObtainWrite(&scp->llock);
		    cm_ForceReturnToken(scp, TKN_READ, 0);
		    lock_ReleaseWrite(&scp->llock);
		    lock_ObtainWrite(&cm_scachelock);
		    CM_RELE(scp);
		    ++dum3;
		}
	    }
	}
	lock_ReleaseWrite(&cm_scachelock);
	icl_Trace2(cm_iclSetp, CM_TRACE_CVSFLUSH,
		   ICL_TYPE_LONG, dum3,
		   ICL_TYPE_HYPER, &volp->volume);
    }
    if (needVVUpdate != 0) {
	lock_ObtainWrite(&volp->lock);
	/* 
	 * Update our memory of when this version was last current.
	 * Update both timeWhenVVCurrent and latestVVSeen, if we
	 * now have the latest information (having reacquired the lock).
	 */
	if (AFS_hcmp(syncp->VV, volp->latestVVSeen) > 0) {
	    icl_Trace4(cm_iclSetp, CM_TRACE_CVSSET3,
		       ICL_TYPE_HYPER, &volp->volume,
		       ICL_TYPE_HYPER, &volp->latestVVSeen,
		       ICL_TYPE_HYPER, &syncp->VV,
		       ICL_TYPE_HYPER, &reallyNewVV);
	    volp->latestVVSeen = syncp->VV;
	}
	oneTime = startTime - ((syncp->VVAge * 129)>>7) - 1;
	if (oneTime > volp->timeWhenVVCurrent) {
	    icl_Trace3(cm_iclSetp, CM_TRACE_CVSUPDATE3,
		       ICL_TYPE_HYPER, &volp->volume,
		       ICL_TYPE_LONG, volp->timeWhenVVCurrent,
		       ICL_TYPE_LONG, oneTime);
	    volp->timeWhenVVCurrent = oneTime;
	}
	lock_ReleaseWrite(&volp->lock);
    }
    if (needPutVol != 0) {
	cm_PutVolume(volp);			/* done with it now */
    }
    if (retVal == 0 && flushingMe) {
	/* Force caller to try again, since retrieved data was flushed */
	icl_Trace1(cm_iclSetp, CM_TRACE_CVSSET6,
		   ICL_TYPE_FID, fidp);
	retVal = CM_REP_ADVANCED_AGAIN;
    }
    return retVal;
}


/* 
 * Make adjustment for the new size in the disk cache entry.
 * Must be called with either both the dcp and cm_dcachelock
 * write-locked, or with a 0 reference count dcache entry
 * while holding the cm_dcachelock (during garbage collection).
 */
#ifdef SGIMIPS
void cm_AdjustSizeNL(
  register struct cm_dcache *dcp,
  register long newSize)
#else  /* SGIMIPS */
void cm_AdjustSizeNL(dcp, newSize)
  register struct cm_dcache *dcp;
  register long newSize;
#endif /* SGIMIPS */
{
    unsigned long newBlocks;

    icl_Trace4(cm_iclSetp, CM_TRACE_ADJSIZE_ENTRY,
	       ICL_TYPE_POINTER, dcp,
	       ICL_TYPE_LONG, dcp->f.chunkBytes,
	       ICL_TYPE_LONG, dcp->f.startDirty,
	       ICL_TYPE_LONG, dcp->f.endDirty);
    if (dcp->refCount <= 0) panic("adjustsizenl refcount");
    dcp->f.chunkBytes = newSize;
    /* No: forbid this call from increasing endDirty; do that elsewhere. */
    if (dcp->f.endDirty > newSize)	/* PREVENT GROWTH HERE */
	dcp->f.endDirty = newSize;	/* NO MORE: can grow or shrink */
    newBlocks = (((newSize-1)|(cm_cacheUnit-1)) + 1) >> 10;
    cm_blocksUsed += (newBlocks - dcp->f.cacheBlocks);
    /* if we're growing the file, wakeup the daemon */
    if (newBlocks > dcp->f.cacheBlocks)
	cm_MaybeWakeupTruncateDaemon();
#ifdef SGIMIPS
    dcp->f.cacheBlocks = (unsigned short)newBlocks;
#else  /* SGIMIPS */
    dcp->f.cacheBlocks = newBlocks;
#endif /* SGIMIPS */
    cm_indexFlags[dcp->index] |= DC_IFENTRYMOD;
    icl_Trace4(cm_iclSetp, CM_TRACE_ADJSIZE_EXIT,
	       ICL_TYPE_LONG, newSize,
	       ICL_TYPE_LONG, dcp->f.chunkBytes,
	       ICL_TYPE_LONG, dcp->f.startDirty,
	       ICL_TYPE_LONG, dcp->f.endDirty);
}

/* Same as cm_AdjustSizeNL, except the dcache llock is held, but
 * the cm_dcachelock is not held.  Obtains proper locks and
 * calls cm_AdjustSizeNL.
 * Returns with dcache llock still held, but releases cm_dcachelock.
 */
#ifdef SGIMIPS
void cm_AdjustSize(
  register struct cm_dcache *dcp,
  register long newSize)
#else  /* SGIMIPS */
void cm_AdjustSize(dcp, newSize)
  register struct cm_dcache *dcp;
  register long newSize;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_dcachelock);
    cm_AdjustSizeNL(dcp, newSize);
    lock_ReleaseWrite(&cm_dcachelock);
}


/* called with dcp write-locked, and cm_dcachelock unlocked.
 * sets the # of bytes in the chunk to 0 without freeing the blocks,
 * but also queues the guy for async truncation.
 */
#ifdef SGIMIPS
void cm_QuickDiscard(register struct cm_dcache *dcp)
#else  /* SGIMIPS */
void cm_QuickDiscard(dcp)
  register struct cm_dcache *dcp;
#endif /* SGIMIPS */
{
    register int ix;

    ix = dcp->index;
    icl_Trace4(cm_iclSetp, CM_TRACE_QUICKDISCARD,
	       ICL_TYPE_POINTER, dcp,
	       ICL_TYPE_LONG, dcp->f.chunkBytes,
	       ICL_TYPE_LONG, dcp->f.startDirty,
	       ICL_TYPE_LONG, dcp->f.endDirty);
    dcp->f.chunkBytes = 0;
    dcp->f.endDirty = 0;
    lock_ObtainWrite(&cm_dcachelock);
    cm_indexTimes[ix] = 0;		/* move to head of LRU queue */
    cm_indexFlags[ix] |= DC_IFENTRYMOD;	/* entry's f structure has changed */
    cm_discardedChunks++;
    lock_ReleaseWrite(&cm_dcachelock);
    cm_MaybeWakeupTruncateDaemon();
}

/* Convert a Unix update (vattr) into a status store request.  Returns
 * the mask it set, which, if 0, means that it found nothing to do.
 */
#ifdef SGIMIPS
void cm_TranslateStatus(
  register struct vattr *av,
  register afsStoreStatus *asp,
  int s_t_n)				/* set_times_now */
#else  /* SGIMIPS */
void cm_TranslateStatus(av, asp, s_t_n)
  register struct vattr *av;
  register afsStoreStatus *asp;
  int s_t_n;				/* set_times_now */
#endif /* SGIMIPS */
{
    register long mask;

    mask = asp->mask;	/* preserve what's there already */
    if (osi_setting_mode(av)) {
	mask |= AFS_SETMODE;
	asp->mode = av->va_mode & 0xffff;
    }
    if (osi_setting_gid(av)) {
	mask |= AFS_SETGROUP;
	asp->group = av->va_gid;
    }
    if (osi_setting_uid(av)) {
	mask |= AFS_SETOWNER;
	asp->owner = av->va_uid;
    }
    if (s_t_n) {			/* System-V style utime call */
	mask |= (AFS_SETMODTIME | AFS_SETMODEXACT | AFS_SETACCESSTIME);
#ifdef SGIMIPS
	/* modTime is defined as a long, and expects a 32 bit
		seconds and 32 bit usec */
	osi_afsGetTime ((struct afsTimeval *) &asp->modTime);
#else /* SGIMIPS */
	osi_GetTime ((struct timeval *) &asp->modTime);
#endif /* SGIMIPS */
	asp->accessTime = asp->modTime;
    } else {
	if (osi_setting_mtime(av)) {
	    mask |= (AFS_SETMODTIME | AFS_SETMODEXACT);
	    osi_UTimeFromSub(asp->modTime, av->va_mtime);
	}
	if (osi_setting_atime(av)) {
	    mask |= AFS_SETACCESSTIME;
	    osi_UTimeFromSub(asp->accessTime, av->va_atime);
	}
    }

    /* change time will be set on any update; now just copy in the
     * updated mask.
     */
#ifdef SGIMIPS
    asp->mask = (unsigned32)mask;
#else  /* SGIMIPS */
    asp->mask = mask;
#endif /* SGIMIPS */
}


#ifdef SGIMIPS
cm_Active(register struct cm_scache *scp) 
#else  /* SGIMIPS */
cm_Active(scp)
     register struct cm_scache *scp; 
#endif /* SGIMIPS */
{
#ifdef AFS_OSF_ENV
     struct vnode *vp;
#endif  /* AFS_OSF_ENV */

#if defined(AFS_AIX_ENV) || defined(AFS_SUNOS5_ENV) || defined(SGIMIPS)
    if (scp->opens > 0)
	return 1;
#else
#ifdef AFS_OSF_ENV
     lock_ObtainWrite(&cm_scachelock);
     vp = SCTOV(scp); /* only sure it points to scp when holding scachelock */
     if (scp->opens > 0 ||
	 (vp && (vp->v_vm_info != VM_INFO_NULL) &&
	  (vp->v_vm_info->pager != MEMORY_OBJECT_NULL) &&
#ifndef AFS_OSF11_ENV
	  !inode_uncache_try(vp)
#else
	  !vnode_uncache_try(vp)
#endif /* AFS_OSF11_ENV */
	  )) {
	 lock_ReleaseWrite(&cm_scachelock);
	 return 1;
     }
    lock_ReleaseWrite(&cm_scachelock);
#else /* end of AFS_OSF_ENV */
    if (scp->opens > 0 || ((SCTOV(scp))->v_flag & VTEXT)) 
	return 1;
#endif  /* AFS_OSF_ENV */
#endif	/* AFS_AIX_ENV or AFS_SUNOS5_ENV or SGIMIPS */
    return 0;
}


/*
 * This call is supposed to flush all caches that might be invalidated by 
 * either a local write operation or a write operation done on another client. 
 * This call may be called repeatedly on the same version of a file, even while
 * a file is being written, so it shouldn't do anything that would discard 
 * newly written data before it is written to the file system. 
 */
#ifdef SGIMIPS
void cm_FlushText(register struct cm_scache *scp) 
#else  /* SGIMIPS */
void cm_FlushText(scp)
    register struct cm_scache *scp; 
#endif /* SGIMIPS */
{
#ifdef	AFS_FTXT
    register struct text *xp;
#endif /*AFS_FTXT */
#ifdef AFS_OSF_ENV
    struct vnode *vp;
#endif /* AFS_OSF_ENV */

    lock_ObtainWrite(&cm_ftxtlock);
#ifdef	AFS_FTXT
    if (SCTOV(scp)->v_flag & VTEXT) {
#ifdef AFS_HPUX_ENV
	register struct text *txp;
#endif

	xp = (struct text *) 0;
#ifdef AFS_HPUX_ENV
	/* 
	 * HP's vnode doesn't support the v_text field!
	 * We'll do it the long way (looking in the text table), since it's
	 * the only way...
	 */
	for (txp = text; txp < textNTEXT; txp++)
	    if (scp == (struct cm_scache *) txp->x_vptr) {
		xp = txp;
		break;
	    }
#else
	if (SCTOV(scp)->v_text) 
	    xp = SCTOV(scp)->v_text;
#endif	/* AFS_HPUX_ENV */

	/* 
	 * now, if text object is locked, give up 
	 */
	if (xp && (xp->x_flag & XLOCK)) {
	    lock_ReleaseWrite(&cm_ftxtlock);
	    return;
	}
	if (SCTOV(scp)->v_flag & VTEXT)	/* still has a text object? */
	    xrele(scp);		/* try now, brown cow */
    }

    /* 
     * next do the stuff that need not check for deadlock problems
     */
#ifndef AFS_OSF_ENV
    mpurge(scp);
#endif /* AFS_OSF_ENV */

#else	/* !AFS_FTXT */

#ifdef AFS_OSF_ENV
    lock_ObtainRead(&cm_scachelock);
    vp = SCTOV(scp);	/* need scachelock to ensure vp still scp's */
    if (vp && (vp->v_vm_info != VM_INFO_NULL) &&
	(vp->v_vm_info->pager != MEMORY_OBJECT_NULL)) {
#ifndef AFS_OSF11_ENV
	inode_uncache(vp);
#else
	vnode_uncache(vp);
#endif /* AFS_OSF11_ENV */
    }
    lock_ReleaseRead(&cm_scachelock);
#endif /* AFS_OSF_ENV */

#ifdef AFS_HPUX_ENV
    /* Question: should we ONLY do this if VTEXT flag is set (better for
     * performance) ... or should we ALWAYS do it (maybe it'll help for
     * mapped files in general)?  It turns out that we CAN'T look at
     * VTEXT: it has been cleared by HPUX.
     */
    xrele(scp);
#endif

#endif	/* AFS_FTXT */
    lock_ReleaseWrite(&cm_ftxtlock);
}


/* find cookie corresponding to a particular chunk we're looking
 * to fetch.  Basic idea is that the set of cookie offsets for
 * the start of each chunk depends upon the dir's DV, so when
 * the data version changes, we have to free this list.
 */
#ifdef SGIMIPS
cm_LookupCookie(
    struct cm_scache *scp,
    long achunk,
    afs_hyper_t *avalp) 
#else  /* SGIMIPS */
cm_LookupCookie(scp, achunk, avalp)
    struct cm_scache *scp;
    long achunk;
    long *avalp; 
#endif /* SGIMIPS */
{
    register struct cm_cookieList *tcp;

    if (achunk == 0) {
	/* 
	 * chunk 0 always starts at offset 0 
	 */
	*avalp = 0;
	return 0;
    }

    /* check for new version */
    if (AFS_hcmp(scp->m.DataVersion, scp->cookieDV)) {
	/* wrong version, fail (could clear it now, but
	 * that would require a write lock)
	 */
	return -1;
    }

    /* otherwise look it up */
    for (tcp = scp->cookieList; tcp; tcp = tcp->next) {
	if (tcp->chunk == achunk) {
	    *avalp = tcp->cookie;
	    return 0;
	}
    }
    return -1;
}


/* find the chunk closest to, but below, the server cookie offset
 * for which we're searching.  Return the corresponding chunk #.
 */
#ifdef SGIMIPS
void cm_FindClosestChunk(
  struct cm_scache *scp,
  afs_hyper_t apos,
  long *achunkp)
#else  /* SGIMIPS */
void cm_FindClosestChunk(scp, apos, achunkp)
  struct cm_scache *scp;
  long apos;
  long *achunkp;
#endif /* SGIMIPS */
{
    register struct cm_cookieList *tcp;
    long bestChunk;		/* best chunk we've found so far */
#ifdef SGIMIPS 
    afs_hyper_t bestCookie;	/* cookie offset of that chunk */
#else  /* SGIMIPS */
    long bestCookie;		/* cookie offset of that chunk */
#endif /* SGIMIPS */

    /* check for wrong version in cookie list */
    if (AFS_hcmp(scp->m.DataVersion, scp->cookieDV)) {
	*achunkp = 0;	/* only one that we know */
	return;
    }

    /* search for chunk with highest cookie offset less than or equal
     * to apos, the cookie offset we're looking for.
     */
    bestChunk = 0;
    bestCookie = 0;
    for (tcp = scp->cookieList; tcp; tcp = tcp->next) {
	if (tcp->cookie <= apos && tcp->cookie > bestCookie) {
	    bestChunk = tcp->chunk;
	    bestCookie = tcp->cookie;
	}
    }
    *achunkp = bestChunk;
}


/*
 * remember the offset of a chunk in server cookie-space for later
 * fetches or other searches.
 */
#ifdef SGIMIPS
void cm_EnterCookie(
    struct cm_scache *scp,
    long achunk,
    afs_hyper_t aval)
#else  /* SGIMIPS */
void cm_EnterCookie(scp, achunk, aval)
    struct cm_scache *scp;
    long achunk;
    long aval;
#endif /* SGIMIPS */
{
    register struct cm_cookieList *tcp;

    /* check for wrong version, and clear things */
    if (AFS_hcmp(scp->cookieDV, scp->m.DataVersion)) {
	cm_FreeAllCookies(scp);
    }

    /* 
     * chunk 0 always starts at offset 0 
     */
    if (achunk == 0)
	return;
    /* 
     * otherwise check to see if it is there 
     */
    for (tcp = scp->cookieList; tcp; tcp = tcp->next) {
	if (tcp->chunk == achunk) {
	    tcp->cookie = aval;
	    return;
	}
    }
    tcp = (struct cm_cookieList *) osi_Alloc(sizeof(struct cm_cookieList));
    tcp->cookie = aval;
    tcp->chunk = achunk;
    tcp->next = scp->cookieList;
    scp->cookieList = tcp;
}


/*
 * Free all tokens in a token list.
 */
#ifdef SGIMIPS
void cm_FreeAllCookies(struct cm_scache *scp)
#else  /* SGIMIPS */
void cm_FreeAllCookies(scp)
  struct cm_scache *scp;
#endif /* SGIMIPS */
{
    register struct cm_cookieList *acp; 
    register struct cm_cookieList *ncp;

    icl_Trace1(cm_iclSetp, CM_TRACE_FREEALLCOOKIES, ICL_TYPE_POINTER, scp);
    scp->ds.scanChunk = -1;	/* reset dir scan hint */
    for (acp = scp->cookieList; acp; acp = ncp) {
	ncp = acp->next;
	osi_Free((opaque)acp, sizeof(*acp));
    }
    scp->cookieList = (struct cm_cookieList *) 0;
    scp->cookieDV = scp->m.DataVersion;
}


/*
 * Atomically get and hold enough space in cache for chunk have cbytes
 * worth of data.  This should be called the scp and dcp llocks write
 * locked. If locks had to be dropped to get space from the cache
 * then a 1 is returned.  Otherwise a 0 is returned.
 *
 * This function should not be called from any code in the callback
 * (token revocation) path, since this function may wait for the
 * FETCHING flag to clear.
 */
#ifdef SGIMIPS
cm_ReserveBlocks(
  struct cm_scache *scp,
  struct cm_dcache *dcp,
  long cbytes)
#else  /* SGIMIPS */
cm_ReserveBlocks(scp, dcp, cbytes)
  struct cm_scache *scp;
  struct cm_dcache *dcp;
  long cbytes;
#endif /* SGIMIPS */
{
    int counter = 0;
    int droppedLocks = 0;
    int wait;
    long ix;
    long newblocks;
    char *osiFilep;

    ix = dcp->index;
    for (;;) {
        wait = 0;
	if (cbytes <= (long)((dcp->f.cacheBlocks << 10)))
	    break; 	/* done, get out of loop */

        newblocks = ((((cbytes-1)|(cm_cacheUnit-1)) + 1) >> 10) - 
	    dcp->f.cacheBlocks; 

	lock_ObtainWrite(&cm_dcachelock);
        if (cm_cacheBlocks >= (cm_blocksUsed + newblocks)) {
	    cm_blocksUsed += newblocks;
	    dcp->f.cacheBlocks += newblocks;
	    cm_indexFlags[dcp->index] |= DC_IFENTRYMOD;
	    lock_ReleaseWrite(&cm_dcachelock);
	    if (newblocks > 0)
		cm_MaybeWakeupTruncateDaemon();
	    break;	/* done, get out of loop */
	} else {
	    if (counter > 5) {
		/* if we have an "emergency", we might be in the
		 * situation where all of the cache's space is pinned
		 * by chunks being grown by cm_ReserveBlocks.  In this
		 * case, to get the space, we have to truncate this file
		 * and take it offline.  Note that this could happen anyway
		 * in this function, since we're dropping both the scp and
		 * dcp locks.
		 *
		 * DON'T DO THIS IF THE CHUNK IS DIRTY.  It will get cleaned
		 * by the background cleaner, and then discarded.
		 *
		 * Don't use DC_INVALID, since that loses chunk's identity.
		 *
		 * Also, make sure that the fetching and storing flags
		 * are clear before doing this, otherwise we could damage
		 * an in-transit chunk (storing is probably less important,
		 * since we wouldn't be storing a clean chunk, and we don't
		 * damage dirty chunks, but we'll be extra paranoid and
		 * watch for both).
		 */
		lock_ReleaseWrite(&cm_dcachelock);
		cm_StabilizeDCache(scp, dcp, CM_DSTAB_MAKEIDLE);
		lock_ObtainWrite(&cm_dcachelock);

		if (!(cm_indexFlags[ix] & (DC_IFDATAMOD | DC_IFDIRTYPAGES))) {
#ifdef AFS_IRIX_ENV_NOT_YET
                    cm_CFileQuickTrunc(&dcp->f.handle, 0);
#else  /* AFS_IRIX_ENV_NOT_YET */
#ifdef AFS_VFSCACHE
		    osiFilep = cm_CFileOpen(&dcp->f.handle);
#else
		    osiFilep = cm_CFileOpen(dcp->f.inode);
#endif
		    if (!osiFilep) panic("reserveblocks open");
		    cm_CFileTruncate(osiFilep, 0);
		    cm_CFileClose(osiFilep);
#endif /* AFS_IRIX_ENV */
		    cm_AdjustSizeNL(dcp, 0L);
		    dcp->f.states &= ~DC_DONLINE;
		    AFS_hzero(dcp->f.tokenID);
		    AFS_hset64(dcp->f.versionNo,-1,-1);
		    cm_indexFlags[ix] |= DC_IFENTRYMOD;
		    icl_Trace2(cm_iclSetp, CM_TRACE_RESERVEBLOCKSEMERGENCY,
			       ICL_TYPE_POINTER, scp,
			       ICL_TYPE_POINTER, dcp);
		}
	    }
	    lock_ReleaseWrite(&dcp->llock);
	    lock_ReleaseWrite(&scp->llock);
	    droppedLocks = 1;
#ifdef SGIMIPS
	    /*  Allow writing back to the server for this file in case this
		is the file that has all the cache blocks allocated and waiting
		to be written.  Otherwise, the background thread gets stuck
		trying to get the R/W lock for this scp and can't write anything
		to the server. Only make this call if we own the R/W lock.
		Otherwise there is potential for deadlock waiting for the
		R/W lock while another thread is waiting for a dcache
		entry to come online.  */
	    if (scp->rwl_owner == osi_ThreadUnique()) {
		lock_ReleaseWrite(&cm_dcachelock);
	    	cm_SyncDCache(scp, 0, (osi_cred_t *)0);
		lock_ObtainWrite(&cm_dcachelock);
	    }
#endif /* SGIMIPS */
	    cm_GetDownD(4, CM_GETDOWND_NEEDSPACE);
            if (cm_cacheBlocks < (cm_blocksUsed + newblocks)) {
		wait = 1;
		if ((++counter % 300) == 0)
		    printf("dfs:can't free cacheblocks\n");
	    }
	}
	lock_ReleaseWrite(&cm_dcachelock);

	if (wait)
	    osi_Wait(1000, 0, 0);

	/* Reaquire locks, and try again */
	lock_ObtainWrite(&scp->llock);
	lock_ObtainWrite(&dcp->llock);
    } /* for (;;) */
    return(droppedLocks);
}

#ifdef SGIMIPS
cm_IsDevVP(register struct cm_scache *scp) 
#else  /* SGIMIPS */
cm_IsDevVP(scp)
 register struct cm_scache *scp; 
#endif /* SGIMIPS */
{
    if (cm_vType(scp) == VCHR || cm_vType(scp) == VBLK || 
	cm_vType(scp) == VFIFO)
	return 1;
    else
	return 0;
}

/* VARARGS1 */
#ifndef SGIMIPS 
void cm_printf(a, b, c, d, e, f, g)
  char *a, *b, *c, *d, *e, *f, *g;
{
    printf(a, b, c, d, e, f, g);
    uprintf(a, b, c, d, e, f, g);
}
#endif  /* !SGIMIPS */

#ifdef SGIMIPS
struct cm_scache *cm_MakeDev(
  register struct cm_scache *scp,
  long adevno) 
#else  /* SGIMIPS */
struct cm_scache *cm_MakeDev(scp, adevno)
register struct cm_scache *scp;
long adevno; 
#endif /* SGIMIPS */
{

#ifdef AFS_SUNOS5_ENV
    register struct vnode *tvp = NULL;
    struct vnode *avp = SCTOV(scp);

    /*
     * If SC_BADSPECDEV is set, it means that this is really a device 
     * but the device is not representable on this architecture because
     * of incompatibilities in number of bits for major/minor encoding 
     */
    if (((scp->volp->states & VL_DEVOK) || cm_vType(scp) == VFIFO) &&
	!(scp->states & SC_BADSPECDEV)) {
	osi_RestorePreemption(0);
	tvp = (struct vnode *) specvp(avp, adevno, avp->v_type, osi_credp);
	osi_PreemptionOff();
	if (tvp) {
	    cm_PutSCache(scp);	/* release original scp ! */
	    return (struct cm_scache *) tvp;	/*just cast */
	} else {
	    return scp;
	}
    }
#endif /* AFS_SUNOS5_ENV */

#ifdef AFS_AIX32_ENV
    if (scp->states & SC_BADSPECDEV) {
	/* 
	 * This is a device but it isn't supported on this architecture
	 * because of incompatibility with device numbers.
	 */
	(SCTOV(scp))->v_rdev = DEVCNT + 1;
    } else {
	(SCTOV(scp))->v_rdev = 
	    ((scp->volp->states & VL_DEVOK)? adevno: (DEVCNT + 1));
    }
#endif /* AFS_AIX32_ENV */

/*
 * warning: for public pool systems
 * if this routine allocates a new vnode, it's got to
 * allocate a new scache, set up backlinks between scache and vnode
 * etc, etc...
 */
    return scp;
}

/* Do the work to truncate a file.  Much of work is verifying
 * that the file's in the desired state beforehand.
 *
 * Called with no locks held, returns with no locks held.
 * Temporarily locks vnode, dcp entries.
 *
 * Nowadays, kernels do not expect the file system to check for write
 * access when truncating.  This is done above the VFS interface.  An
 * exception is HP/UX.  Code to perform the access check is now conditionally
 * compiled for HP/UX only.
 */
#ifdef SGIMIPS
int cm_TruncateFile(
    struct cm_scache	*scp,
    afs_hyper_t		length,
    osi_cred_t		*credp,
    struct cm_rrequest  *rreqp,
    int			flag)		/* 1 from create, 0 from setattr */
#else  /* SGIMIPS */
int cm_TruncateFile(scp, length, credp, rreqp)
    struct cm_scache	*scp;
    long		length;
    osi_cred_t		*credp;
    struct cm_rrequest  *rreqp;
#endif /* SGIMIPS */
{
    long code = 0;
#ifdef SGIMIPS
    vnode_t     *vp;
    int error=0;
#endif /* SGIMIPS */
#ifdef AFS_HPUX_ENV
    int retryCount = 0;
    int accessOK;	/* access check variable */
    int blocked;	/* do we have to retry due to lock loss */
#endif /* AFS_HPUX_ENV */
    /* 
     * Get rid of vm pages and zero remainder of last page, so that mapped 
     * people are happy.  Block pageins until we are done clobbering all the
     * dcache entries.
     *
     * It's important not to allow anyone else, such as cm_putpage, to see
     * pages beyond EOF.  This is why we destroy the pages before lowering the
     * EOF, and why we hold the activeRevokes counter high in between.
     */
#ifdef SGIMIPS
     /*  ???????What happens when the file is growing?  */
     vp=SCTOV(scp);
       if (flag) {
          SC_STARTED_FLUSHING_ON(scp, error);
          if (!error) {
            VOP_FLUSHINVAL_PAGES(vp, length, SZTODFSBLKS(scp->m.Length) - 1,
			     FI_REMAPF_LOCKED);
          SC_STARTED_FLUSHING_OFF(scp);
	  }
       } else {
          if (length < scp->m.Length) {
             SC_STARTED_FLUSHING_ON(scp, error);
             if (!error) {
               VOP_TOSS_PAGES(vp, length, SZTODFSBLKS(scp->m.Length) - 1,
			  FI_REMAPF_LOCKED);
               SC_STARTED_FLUSHING_OFF(scp);
	     }
          }
       }
#endif /* SGIMIPS */
    cm_VMPreTruncate(scp, length);
    lock_ObtainWrite(&scp->llock);
    scp->activeRevokes++;
    lock_ReleaseWrite(&scp->llock);
    cm_VMTruncate(scp, length);

    /* check for a marked-bad vnode and bail out */
    lock_ObtainRead(&scp->llock);
    code = scp->asyncStatus;
    lock_ReleaseRead(&scp->llock);
#ifdef SGIMIPS
    if (code) return (int)code;	/* from async status */
#else  /* SGIMIPS */
    if (code) return code;	/* from async status */
#endif /* SGIMIPS */

    /* 
     * Loop until all needed conditions are met to truncate the dcache
     */
    for (;;) {
	code = cm_GetSLock(scp, TKN_UPDATE, CM_SLOCK_WRITE, WRITE_LOCK, rreqp);
	if (code) {
	    lock_ObtainWrite(&scp->llock);
	    goto out;
	}
	/* vnode write-locked now (cm_GetSLock returned 0) */

#ifdef AFS_HPUX_ENV

	/* We need write access for the file size change  */
	code = cm_GetAccessBits(scp, dacl_perm_write, &accessOK, &blocked,
			     rreqp, WRITE_LOCK, &retryCount);
	if (code)
	    goto out;

	/* if we lost the lock, vnode may no longer be stable; retry */
	if (blocked) {
	    lock_ReleaseWrite(&scp->llock);
	    continue;
	}
	/* if we don't have the right access, fail */
	if (!accessOK) {
	    code = EACCES;
	    goto out;
	}

#endif /* AFS_HPUX_ENV */

	/* 
	 * Wait for all fetch and store activity to finish before
	 * truncating the dcache entries.
	 */
	if ((scp->states & SC_STORING) || (scp->storeCount > 0) ||
	    (scp->fetchCount > 0)) {
	    scp->states |= SC_WAITING;
	    osi_SleepW(&scp->states, &scp->llock);
	    continue; /* must start over */
	}

	/* if we make it here, all conditions are go, so update
	 * cred and go for it.
	 */
	if (credp) { 
	   crhold(credp);          /* in case credp == scp->writerCredp */
	   if (scp->writerCredp)
	       crfree(scp->writerCredp);
	   scp->writerCredp = credp;
        }
	break;	/* all conditions met */
    }	/* end for (;;) */

    code = cm_TruncateAllSegments(scp, length, rreqp);
out:
    cm_PutActiveRevokes(scp);
    lock_ReleaseWrite(&scp->llock);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}

/* Called with write-locked scp.  Decrements activeRevokes counter,
 * and if necessary, wakes anyone waiting for this counter to hit
 * zero.
 */
#ifdef SGIMIPS
void cm_PutActiveRevokes(register struct cm_scache *scp)
#else  /* SGIMIPS */
void cm_PutActiveRevokes(scp)
  register struct cm_scache *scp;
#endif /* SGIMIPS */
{
    if (--scp->activeRevokes == 0 &&
	(scp->states & SC_REVOKEWAIT)) {
	/* someone's waitiing */
	scp->states &= ~SC_REVOKEWAIT;
	osi_Wakeup((char *) &scp->states);
    }
    return;
}
