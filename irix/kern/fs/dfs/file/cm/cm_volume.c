/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_volume.c,v 65.5 1998/03/23 16:24:07 gwehrman Exp $";
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
#include <dcedfs/stds.h>
#include <dcedfs/osi_cred.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_volume.h>
#include <cm_cell.h>
#include <cm_vcache.h>
#include <cm_conn.h>
#include <cm_stats.h>
#include <cm_server.h>
#include <cm_site.h>
#include <dcedfs/afsvl_proc.h>		/* real VLDB inclusions */
#include <dcedfs/flserver.h>		/* extra VLDB inclusions */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_volume.c,v 65.5 1998/03/23 16:24:07 gwehrman Exp $")

/* 
 * Volume related variables 
 */
struct cm_volume *cm_volumes[VL_NVOLS];	/* Cached volumes' hash table */
struct lock_data cm_volumelock;		/* allocation lock for volumes */
struct cm_volume *cm_freeVolList;	/* free Volumes linked list */
struct cm_volume *Initialcm_freeVolList;/* Ptr to orig malloced list */
long cm_fvTable[VL_NFENTRIES];          /* Hash table for on-disk vol ents */
/*static*/ long cm_volCounter = 1;	/* for allocating volume indices */

static struct cm_fvolume staticFVolume;
static long FVIndex = -1;

/* Flag to force fetching of (non-existant) additional server addresses;
 * patchable value for multi-homed server testing.
 */
static int cm_forceGetMoreAddrs = 0;

static struct cm_volume *GetVolSlot _TAKES((void));
static struct cm_fvolume *FindVolCache _TAKES((afs_hyper_t *, afs_hyper_t *));
static struct cm_volume 
  *cm_GetVolumeByNameWithHint _TAKES((char *, afs_hyper_t *, long,
				      struct cm_rrequest *,
				      struct cm_volume *));
static void cm_InstallVolumeEntry _TAKES((struct cm_volume *,
					  struct vldbentry *,
					  struct cm_cell *,
					  int *));
#ifdef SGIMIPS
extern int dfsh_StrToHyper(char *, afs_hyper_t *, char **);
#endif /* SGIMIPS */

/* Check if an RPC is forced for volp, which is UNLOCKED. */
#ifdef SGIMIPS
int cm_NeedRPC(register struct cm_volume *volp)
#else  /* SGIMIPS */
int cm_NeedRPC(volp)
  register struct cm_volume *volp;
#endif /* SGIMIPS */
{
    int result = 0;
    /* We're allowed to return FALSE without a lock. */
    if (volp->states & VL_NEEDRPC) {
	/* Check again after locking */
	lock_ObtainWrite(&volp->lock);
	if (volp->states & VL_NEEDRPC) {
	    icl_Trace1(cm_iclSetp, CM_TRACE_NEEDRPC_YES,
		       ICL_TYPE_HYPER, &volp->volume);
	    result = 1;
	    volp->states &= ~VL_NEEDRPC;
	}
	lock_ReleaseWrite(&volp->lock);
    }
    return result;
}


/* hold a reference to a volume */
#ifdef SGIMIPS
void cm_HoldVolume(register struct cm_volume *volp)
#else  /* SGIMIPS */
void cm_HoldVolume(volp)
  register struct cm_volume *volp;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_volumelock);
    volp->refCount++;
    lock_ReleaseWrite(&cm_volumelock);
}


/* Set the VL_HISTORICAL flag. */
/* Call with volp->lock write-locked. */
#ifdef SGIMIPS
static void cm_TurnOnHistorical(struct cm_volume *volp)
#else  /* SGIMIPS */
static void cm_TurnOnHistorical(volp)
    struct cm_volume *volp;
#endif /* SGIMIPS */
{
    if ((volp->states & VL_HISTORICAL) == 0) {
	cm_printf("dfs: fileset %u,,%u (%s) no longer exists in cell %s\n",
		  AFS_HGETBOTH(volp->volume),
		  (volp->volnamep ? volp->volnamep : ""),
		  (volp->cellp && volp->cellp->cellNamep ?
		   volp->cellp->cellNamep : "*unk*"));
	volp->states |= VL_HISTORICAL;
    }
    volp->states &= ~VL_RECHECK;
}

/* Clear the VL_HISTORICAL flag. */
/* Call with volp->lock write-locked. */
#ifdef SGIMIPS
static void cm_TurnOffHistorical(struct cm_volume *volp)
#else  /* SGIMIPS */
static void cm_TurnOffHistorical(volp)
    struct cm_volume *volp;
#endif /* SGIMIPS */
{
    if (volp->states & VL_HISTORICAL) {
	cm_printf("dfs: fileset %u,,%u (%s) again exists in cell %s\n",
		  AFS_HGETBOTH(volp->volume),
		  (volp->volnamep ? volp->volnamep : ""),
		  (volp->cellp && volp->cellp->cellNamep ?
		   volp->cellp->cellNamep : "*unk*"));
	volp->states &= ~(VL_HISTORICAL);
    }
    volp->states &= ~(VL_RECHECK);
}

#if	KERNEL

#ifdef SGIMIPS
struct cm_volume *cm_GetVolumeByName(
    register char *volNamep,
    afs_hyper_t *cellIdp,
    long agood,
    struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
struct cm_volume *cm_GetVolumeByName(volNamep, cellIdp, agood, rreqp)
    struct cm_rrequest *rreqp;
    long agood;
    afs_hyper_t *cellIdp;
    register char *volNamep;
#endif /* SGIMIPS */
{
    return cm_GetVolumeByNameWithHint(volNamep, cellIdp, agood, rreqp, 
				      (struct cm_volume *) 0);
}

/* 
 * Find a volume by its id; note that areq may be null, in which case
 * we don't bother to set any request status information. 
 */
#ifdef SGIMIPS
struct cm_volume *cm_GetVolumeByFid(
    register afsFid *fidp,
    struct cm_rrequest *rreqp,
    int histOK,
    int quickSearch)
#else  /* SGIMIPS */
struct cm_volume *cm_GetVolumeByFid(fidp, rreqp, histOK, quickSearch)
    struct cm_rrequest *rreqp;
    register afsFid *fidp;
    int histOK;
    int quickSearch;
#endif /* SGIMIPS */
{
    register struct cm_volume *volp;
    register long i;
    char tbuffer[50];

    i = VL_HASH(AFS_hgetlo(fidp->Volume));
    lock_ObtainWrite(&cm_volumelock);
    for (volp = cm_volumes[i]; volp; volp = volp->next) {
	if (AFS_hsame(volp->volume, fidp->Volume) && 
	    AFS_hsame(volp->cellp->cellId, fidp->Cell)) {/* got one */
	    volp->refCount++; /* locking order, sigh ! */
	    if (quickSearch)
	        break;
	    lock_ReleaseWrite(&cm_volumelock);
	    lock_ObtainRead(&volp->lock);
	    if (volp->states & VL_RECHECK) { 
		lock_ReleaseRead(&volp->lock);
		lock_ObtainWrite(&cm_volumelock);
		break; /* get out of the for loop */
	    }
	    if (histOK == 0 && (volp->states & VL_HISTORICAL)) {
		lock_ReleaseRead(&volp->lock);
		cm_PutVolume(volp);
		volp = NULL;
		if (rreqp) {
		    cm_FinalizeReq(rreqp);
		    rreqp->volumeError = 1;
		}
	    } else {
		lock_ReleaseRead(&volp->lock);  
		/* got a good one */
	    }
	    return volp;
	}
    }
    lock_ReleaseWrite(&cm_volumelock);
    if (quickSearch)
        return volp;
    osi_cv2string(AFS_hgethi(fidp->Volume), tbuffer);
    strcat(tbuffer, ",,");
    osi_cv2string(AFS_hgetlo(fidp->Volume), &tbuffer[strlen(tbuffer)]);
    return cm_GetVolumeByNameWithHint(tbuffer, &fidp->Cell, 0, rreqp, volp);
}


#ifdef SGIMIPS
struct cm_volume *cm_GetVolume(
    register afsFid *fidp, 
    struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
struct cm_volume *cm_GetVolume(fidp, rreqp)
    struct cm_rrequest *rreqp;
    register afsFid *fidp; 
#endif /* SGIMIPS */
{
    return cm_GetVolumeByFid(fidp, rreqp, 0, 0);
}


/* 
 * Search/Setup a volume entry by it's name; if it doesn't exist it gets it 
 * from an appropriate VLDB server.
 * If hintvolp isn't null, it's a fid match that requires re-checking.
 */
#ifdef SGIMIPS
static struct cm_volume *cm_GetVolumeByNameWithHint(
    register char *volNamep,
    afs_hyper_t *cellIdp,
    long agood,
    struct cm_rrequest *rreqp,
    struct cm_volume *hintvolp)
#else  /* SGIMIPS */
static struct cm_volume *cm_GetVolumeByNameWithHint(volNamep, cellIdp, agood, 
						    rreqp, hintvolp)
    struct cm_rrequest *rreqp;
    long agood;
    afs_hyper_t *cellIdp;
    register char *volNamep;
    struct cm_volume *hintvolp;
#endif /* SGIMIPS */
{
    register long code, i, whichType;
    register struct cm_volume *volp;
    register struct vldbentry *tve;
    struct cm_cell *cellp;
    struct cm_fvolume *fvolp;
    char *bufferp;
    struct cm_conn *connp;
    afs_hyper_t tvolume;
    u_long startTime;
    struct cm_rrequest rreq;
    int gotNewLoc;
    struct cm_volume *histRkVolp = NULL;
    char hitHistorical = 0;

   /* Return a non-historical volp, or a historical one needing a recheck. */
    lock_ObtainWrite(&cm_volumelock);
    for (i = 0; i < VL_NVOLS; i++) {
	for (volp = cm_volumes[i]; volp; volp = volp->next) {
	    if (volp->volnamep && !strcmp(volNamep,volp->volnamep) && 
		AFS_hsame(volp->cellp->cellId, *cellIdp)) {
		volp->refCount++; /* locking order, sigh ! */
		lock_ReleaseWrite(&cm_volumelock);
		lock_ObtainRead(&volp->lock);
		/* We'll skip historical matches, but will refresh them if
		 * asked and there's no better match
		 */
		if (volp->states & VL_HISTORICAL) {
		    /* Check this one if necessary, too. */
		    if ((volp->states & VL_RECHECK) && histRkVolp == NULL) {
			histRkVolp = volp;
		    } else {
			cm_PutVolume(volp);
			hitHistorical = 1;
		    }
		    lock_ReleaseRead(&volp->lock);
		    lock_ObtainWrite(&cm_volumelock);
		    continue; /* keep trying */
		}
		if (volp->states & VL_RECHECK) {
		    /* got one but may have stale info */
		    lock_ReleaseRead(&volp->lock);
		    goto renew;
		}
		lock_ReleaseRead(&volp->lock);
		/* Got a good one; return it with high refCount. */
		if (hintvolp) cm_PutVolume(hintvolp);
		if (histRkVolp) cm_PutVolume(histRkVolp);
		return volp;
	    }
        }
    }
    lock_ReleaseWrite(&cm_volumelock);

    /* We're here if no non-historical vols were found. */
    if (histRkVolp) {
	/* Found at least one historical volp needing renewal. */
	volp = histRkVolp;
	histRkVolp = NULL;
    } else {
	if (hitHistorical) {
	    /* >=1 historical vol, but none needing renewal. */
	    if (rreqp) {
		cm_FinalizeReq(rreqp);
		rreqp->volumeError = 1;
	    }
	    if (hintvolp) cm_PutVolume(hintvolp);
	    return (struct cm_volume *) 0;
	}
    }

renew:
    if (histRkVolp) cm_PutVolume(histRkVolp);   /* done with this now */
    if (!(cellp = cm_GetCell(cellIdp))) {
	if (volp) cm_PutVolume(volp);
	if (hintvolp) cm_PutVolume(hintvolp);
	return (struct cm_volume *) 0;
    }

    icl_Trace1(cm_iclSetp, CM_TRACE_GETVOLBYNAME, ICL_TYPE_STRING,
	       volNamep);
    bufferp = osi_AllocBufferSpace();
    tve = (struct vldbentry *) (bufferp+1024);
    cm_InitUnauthReq(&rreq);	/* *must* be unauth for vldb */
    code = strlen(volNamep);
    if (code >= 8 && strcmp(volNamep+code-7, ".backup") == 0)
	whichType = BACKVOL;
    else if (code >= 10 && strcmp(volNamep+code-9, ".readonly")==0)
	whichType = ROVOL;
    else 
	whichType = RWVOL;
    do {
	if (connp = cm_ConnByMHosts(cellp->cellHosts, MAIN_SERVICE(SRT_FL),
				    &cellp->cellId, &rreq, NULL, cellp, NULL)){
	    icl_Trace0(cm_iclSetp, CM_TRACE_GETVOLSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    code = VL_GetEntryByName(connp->connp, (unsigned char *)volNamep,
				     tve);
	    osi_PreemptionOff();
	    icl_Trace1(cm_iclSetp, CM_TRACE_GETVOLEND, ICL_TYPE_LONG, code);
	} else 
	    code = -1;
    } while (cm_Analyze(connp, code, (afsFid *) 0, &rreq, NULL, -1, startTime));
    if (code) {
	if (rreqp) 
	    cm_CopyError(&rreq, rreqp);
	osi_FreeBufferSpace((struct osi_buffer *)bufferp);
	if (volp) {
	    if (code == VL_NOENT) { 
		lock_ObtainWrite(&volp->lock);
		cm_TurnOnHistorical(volp);
		lock_ReleaseWrite(&volp->lock);
	    } else {
		/* 
		 * We could not get new info from flservers. Turn on the 
		 * VL_RECHECK in case the fileset is moved later on.
		 */
		lock_ObtainWrite(&volp->lock);
		volp->states |= VL_RECHECK;
		lock_ReleaseWrite(&volp->lock);
	    }
	    cm_PutVolume(volp);
	}
	if (hintvolp) {    /* finalize a recheck-request from a FID match */
	    if (code == VL_NOENT) { 
		lock_ObtainWrite(&hintvolp->lock);
		cm_TurnOnHistorical(hintvolp);
		lock_ReleaseWrite(&hintvolp->lock);
	    } else {
		/*
		 * We could not get new info from flservers. Turn on the
		 * VL_RECHECK in case the fileset is moved later on.
		 */
		lock_ObtainWrite(&hintvolp->lock);
		hintvolp->states |= VL_RECHECK;
		lock_ReleaseWrite(&hintvolp->lock);
	    }
	    cm_PutVolume(hintvolp);
	}
	return (struct cm_volume *) 0;
    }
    if (volp) {
	/*
	 * If VL_HISTORICAL was set, then some external caller
	 * set VL_RECHECK after the fileset was marked as
	 * historical. Make sure VL_RECHECK gets turned off
	 * if new volume information was retrieved.
	 * If the fileset has been deleted, and then recreated
	 * with a new ID, this will be the only chance to turn off VL_RECHECK
	 * in this cm_volume structure that has the old ID.
	 */
	lock_ObtainWrite(&volp->lock);
	volp->states &= ~VL_RECHECK;
	lock_ReleaseWrite(&volp->lock);
    }

    /* won't need this reference any longer--will re-find it if necessary */
    if (hintvolp) cm_PutVolume(hintvolp);
    /* 
     * figure out which one we're really interested in (a set is returned) 
     */
    code = dfsh_StrToHyper(volNamep, &tvolume, NULL);
    if (code != 0) {
	whichType = VOLTIX_TO_VOLTYPE(whichType);
	for (i = 0; i < MAXVOLTYPES && !AFS_hiszero(tve->VolIDs[i]); ++i) {
	    if (tve->VolTypes[i] == whichType) break;
	}
	if (i < MAXVOLTYPES && !AFS_hiszero(tve->VolIDs[i])) {
	    tvolume = tve->VolIDs[i];
	} else {
	    AFS_hzero(tvolume);		/* entry not present */
	}
    }
    i = VL_HASH(AFS_hgetlo(tvolume));
    lock_ObtainWrite(&cm_volumelock);
    /* won't need this reference any longer, either */
    if (volp) volp->refCount--;
    for (volp = cm_volumes[i]; volp; volp = volp->next) {
        if (AFS_hsame(volp->volume, tvolume) &&
	    AFS_hsame(volp->cellp->cellId, cellp->cellId)) {
	    break;
	}
    }
    if (!volp) {
	volp = GetVolSlot();
	bzero((char *)volp, sizeof(struct cm_volume));
	volp->cellp = cellp;
	lock_Init(&volp->lock);
	volp->volume = tvolume;
	volp->next = cm_volumes[i];	/* thread into list */
	cm_volumes[i] = volp;
	volp->refCount++;
	if (fvolp = FindVolCache(&volp->cellp->cellId, &tvolume)) {
	    volp->vtix = FVIndex;
	    volp->mtpoint = fvolp->mtpoint;
	    volp->dotdot = fvolp->dotdot;
	    volp->rootFid = fvolp->rootFid;
	    volp->states &= ~VL_SAVEBITS;
	    volp->states |= fvolp->ustates; /* restore state bits from disk */
	} else 
	    volp->vtix = -1;
    } else
	volp->refCount++;
    lock_ReleaseWrite(&cm_volumelock);

    lock_ObtainWrite(&volp->lock);
    volp->accessTime = osi_Time();
    cm_InstallVolumeEntry(volp, tve, cellp, &gotNewLoc);
    osi_FreeBufferSpace((struct osi_buffer *)bufferp);
    if (agood) {	/* remember the name it will be referred to later */
	if (!volp->volnamep || strcmp(volp->volnamep, volNamep) != 0) {
	    icl_Trace2(cm_iclSetp, CM_TRACE_NEWVOLNAME,
		       ICL_TYPE_POINTER, volp,
		       ICL_TYPE_STRING, volNamep);
	    if (volp->volnamep)
		osi_Free(volp->volnamep, strlen(volp->volnamep)+1);
	    volp->volnamep = osi_Alloc(strlen(volNamep) + 1);
	    strcpy(volp->volnamep, volNamep);
	}
    }
    if (rreqp && (volp->states & VL_HISTORICAL)) {
	lock_ReleaseWrite(&volp->lock);
	cm_FinalizeReq(rreqp);
	rreqp->volumeError = 1;
	cm_PutVolume(volp);
	return NULL;
    }
    lock_ReleaseWrite(&volp->lock);
    return volp;
}

#endif	/* KERNEL */

/*
 * Note that there is an RPC starting on this volume
 * Also set the onlyOneOutStandingRPC boolean to 1 if
 * there is no other RPC currently running on this volume
 * or 0 otherwise (for a detailed description on the use
 * of this see the comments in cm_MergeStatus())
 */

#ifdef SGIMIPS
void cm_StartVolumeRPC(register struct cm_volume *volp) 
#else  /* SGIMIPS */
void cm_StartVolumeRPC(volp)
    register struct cm_volume *volp; 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&volp->lock);
    if (volp->outstandingRPC == 0)
    {
	volp->onlyOneOutstandingRPC = 1;
    } else {
	volp->onlyOneOutstandingRPC = 0;
    }
    volp->outstandingRPC++;
    lock_ReleaseWrite(&volp->lock);
}

/*
 * Decrement number of volume's outstanding RPCs 
 * Volume must be Held
 */

#ifdef SGIMIPS
void cm_EndVolumeRPC(register struct cm_volume *volp) 
#else  /* SGIMIPS */
void cm_EndVolumeRPC(volp)
    register struct cm_volume *volp; 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&volp->lock);
    osi_assert(volp->outstandingRPC > 0);
    volp->outstandingRPC--;
    lock_ReleaseWrite(&volp->lock);
    cm_PutVolume(volp);
}

/*
 * Release volume's reference counter
 */
#ifdef SGIMIPS
void cm_PutVolume(register struct cm_volume *volp) 
#else  /* SGIMIPS */
void cm_PutVolume(volp)
    register struct cm_volume *volp; 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_volumelock);
    osi_assert(volp->refCount > 0);
    volp->refCount--;
    lock_ReleaseWrite(&cm_volumelock);
}



/*
 * cm_SameHereServer() -- Compare whether the fileset has a HERE token or not.
 *     Note that a fileset could have several read-only filesets located in
 *     different fx servers.
 *
 * ASSUMPTIONS: caller holds volp->lock
 */
#ifdef SGIMIPS
int cm_SameHereServer(
     struct cm_server *serverp,
     struct cm_volume *volp)
#else  /* SGIMIPS */
int cm_SameHereServer(serverp, volp)
     struct cm_server *serverp;
     struct cm_volume *volp;
#endif /* SGIMIPS */
{
    register int i;
    register struct cm_server **srvpp;

    for (i = 0, srvpp = &volp->serverHost[0];
	  i < CM_VL_MAX_HOSTS && *srvpp != NULL;
	  ++i, ++srvpp) {
	if (serverp == *srvpp)
	    return 1;
    }
    return 0;
}


/* 
 * Call with the cm_volume structure write-locked.
 */
#ifdef SGIMIPS
static void cm_InstallVolumeEntry(
    register struct cm_volume *volp,
    register struct vldbentry *vlentryp,
    struct cm_cell *cellp,
    int *gotNewLocP)
#else  /* SGIMIPS */
static void cm_InstallVolumeEntry(volp, vlentryp, cellp, gotNewLocP)
    register struct cm_volume *volp;
    register struct vldbentry *vlentryp;
    struct cm_cell *cellp;
    int *gotNewLocP;
#endif /* SGIMIPS */
{
    register int i, j, k, n;
    int rwix, isBogus, addrCount, getMoreAddrs;
    unsigned long mask, rwmask;
    int xl[3];	
    struct sockaddr_in taddr[ADDRSINSITE];
    struct cm_server *oldServers[CM_VL_MAX_HOSTS], *oldReps[CM_VL_MAX_HOSTS];
    struct cm_server *serverp;
    char *sfx;
    u_char minAuthn, maxAuthn;

    CM_BUMP_COUNTER(filesetCacheUpdates);
    icl_Trace1(cm_iclSetp, CM_TRACE_INSTALLVOLENTRY,
	       ICL_TYPE_LONG, AFS_hgetlo(volp->volume));

    /* Extract the replication-specific values from the vldb entry */

    volp->maxTotalLatency     = vlentryp->maxTotalLatency;
    volp->hardMaxTotalLatency = vlentryp->hardMaxTotalLatency;
    volp->reclaimDally        = vlentryp->reclaimDally;

    if (volp->maxTotalLatency != 0) {
	/* Figure how long to wait between probes in the gray area. */
	volp->repTryInterval =
	  (volp->hardMaxTotalLatency - volp->maxTotalLatency) / 8;
	/* Sanity-bound the interval between those rep probes. */
	if (((long)volp->repTryInterval) < 30)
	    volp->repTryInterval = 30;
	if (((long)volp->repTryInterval) > 300)
	    volp->repTryInterval = 300;
	volp->lastRepTry = (osi_Time() - volp->repTryInterval);
    }

    /* Track what the server tells us are auth values that are too high/low */

    if (cellp->lclStates & CLL_LCLCELL) {
	minAuthn = vlentryp->charSpares[VLAUTHN_MINLCLLEVEL];
	maxAuthn = vlentryp->charSpares[VLAUTHN_MAXLCLLEVEL];
    } else {
	minAuthn = vlentryp->charSpares[VLAUTHN_MINRMTLEVEL];
	maxAuthn = vlentryp->charSpares[VLAUTHN_MAXRMTLEVEL];
    }
    if (minAuthn == rpc_c_protect_level_default)
	minAuthn = CM_MIN_SECURITY_LEVEL;
    if (maxAuthn == rpc_c_protect_level_default)
	maxAuthn = CM_MAX_SECURITY_LEVEL;
    if (minAuthn > maxAuthn) {
	icl_Trace3(cm_iclSetp, CM_TRACE_INSTALLVOL_BADLIMITS,
		   ICL_TYPE_HYPER, &volp->volume,
		   ICL_TYPE_LONG, minAuthn,
		   ICL_TYPE_LONG, maxAuthn);
	minAuthn = CM_MIN_SECURITY_LEVEL;
	maxAuthn = CM_MAX_SECURITY_LEVEL;
    }
    volp->minFldbAuthn = minAuthn;
    volp->maxFldbAuthn = maxAuthn;
    volp->minRespAuthn = CM_MIN_SECURITY_LEVEL;
    volp->maxRespAuthn = CM_MAX_SECURITY_LEVEL;

    *gotNewLocP = 0;
    isBogus = 0;

    /* Figure out volp's volume type by looking it up in the vldb entry */

    for (i = 0; i < 3; ++i) {
	xl[i] = -1;
    }
    rwmask = 0;

    for (i = 0; i < MAXVOLTYPES && !AFS_hiszero(vlentryp->VolIDs[i]); ++i) {
	if (vlentryp->VolTypes[i] == VOLTIX_TO_VOLTYPE(RWVOL)) {
	    xl[RWVOL] = i;
	    rwmask = ((unsigned long)VLSF_ZEROIXHERE) >> i;
	} else if (vlentryp->VolTypes[i] == VOLTIX_TO_VOLTYPE(ROVOL)) {
	    xl[ROVOL] = i;
	} else if (vlentryp->VolTypes[i] == VOLTIX_TO_VOLTYPE(BACKVOL)) {
	    xl[BACKVOL] = i;
	}
    }

    if (xl[RWVOL] >= 0 &&
	VL_MATCH(volp->volume, vlentryp->VolIDs[xl[RWVOL]])) {
	/* RW volume type */
	if ((vlentryp->flags & VLF_RWEXISTS) == 0) {
	    /* RW not located during FLDB rebuild */
	    isBogus = 1;
	}
	/* set site location mask and volume states */
	mask = ((unsigned long)VLSF_ZEROIXHERE) >> xl[RWVOL];
	volp->states &= ~(VL_RO | VL_LAZYREP | VL_BACKUP);
	sfx = "";

    } else if (xl[ROVOL] >= 0 &&
	       VL_MATCH(volp->volume, vlentryp->VolIDs[xl[ROVOL]])) {
	/* RO volume type */
	if ((vlentryp->flags & VLF_ROEXISTS) == 0) {
	    /* RO not located during FLDB rebuild */
	    isBogus = 1;
	}
	/* set site location mask and volume states */
	mask = ((unsigned long)VLSF_ZEROIXHERE) >> xl[ROVOL];
	volp->states &= ~VL_BACKUP;
	volp->states |= VL_RO;

	/* validate replication parameters before calling this fileset
	 * really repserver-replicated.
	 */
	if (volp->maxTotalLatency != 0 &&
	    volp->hardMaxTotalLatency != 0 &&
	    volp->reclaimDally != 0) {
	    /* replication parameters look OK. */
	    volp->states |= VL_LAZYREP;
	} else {
	    /* something missing in the replication parameters. */
	    volp->states &= ~VL_LAZYREP;
	    cm_printf("dfs: fileset %u,,%u in cell %s has bad replication parameters (%d, %d, %d)\n",
		      AFS_HGETBOTH(volp->volume),
		      (cellp->cellNamep ? cellp->cellNamep : "*unk*"),
		      volp->maxTotalLatency,
		      volp->hardMaxTotalLatency,
		      volp->reclaimDally);
	    /* Pretend that we never even saw them. */
	    volp->maxTotalLatency = 0;
	    volp->hardMaxTotalLatency = 0;
	    volp->reclaimDally = 0;
	    volp->repTryInterval = 0;
	    volp->lastRepTry = 0;
	}
	sfx = ".readonly";

    } else if (xl[BACKVOL] >= 0 &&
	       VL_MATCH(volp->volume, vlentryp->VolIDs[xl[BACKVOL]])) {
	/* BACK volume type */
	if ((vlentryp->flags & VLF_BACKEXISTS) == 0) {
	    /* BAK not located during FLDB rebuild */
	    isBogus = 1;
	}
	/* set site location mask and volume states */
	mask = ((unsigned long)VLSF_ZEROIXHERE) >> xl[BACKVOL];
	volp->states &= ~VL_LAZYREP;
	volp->states |= (VL_RO|VL_BACKUP);
	sfx = ".backup";

    } else {
	/* Unrecognized volume type */
	isBogus = 1;
	mask = 0;
	sfx = NULL;
    }

    /* Set volume IDs for all volume's instance types */

    AFS_hzero(volp->rwVol);
    AFS_hzero(volp->roVol);
    AFS_hzero(volp->backVol);

    /* 
     * XXXX VLDB must come up with more volume # bits 
     */
    if ((vlentryp->flags & VLF_RWEXISTS) && xl[RWVOL] >= 0)  {
	volp->rwVol = vlentryp->VolIDs[xl[RWVOL]];
    }
    if ((vlentryp->flags & VLF_ROEXISTS) && xl[ROVOL] >= 0) {
	volp->roVol = vlentryp->VolIDs[xl[ROVOL]];
    }
    if ((vlentryp->flags & VLF_BACKEXISTS) && xl[BACKVOL] >= 0) {
	volp->backVol = vlentryp->VolIDs[xl[BACKVOL]];
    }

    /* If volume is release-replicated (both bits on), find the RW site. */

    rwix = -1;

    if ((vlentryp->flags & (VLF_LAZYREP | VLF_LAZYRELE)) ==
	                   (VLF_LAZYREP | VLF_LAZYRELE)) {
	for (i = 0; i < vlentryp->nServers; ++i) {
	    if (vlentryp->siteFlags[i] & rwmask) {
		rwix = i;
		break;
	    }
	}
    }

    /* Set volp's server references based on volp's volume type.
     *
     * If rwix >= 0, it's the index of the RW site for release-replication.
     * Since the site's repserver doesn't track the staging replica (RO),
     * the CM shouldn't try to use it.
     *
     * Note that mask == 0 for an invalid volume type, so that there will be
     * no servers for it.
     */
    bcopy((char *)volp->serverHost, (char *)oldServers, sizeof(oldServers));
    bcopy((char *)volp->repHosts, (char *)oldReps, sizeof(oldReps));

    bzero((char *)volp->serverHost, sizeof(volp->serverHost));
    bzero((char *)volp->repHosts, sizeof(volp->repHosts));

    for (i = 0, j = 0, k = 0; i < vlentryp->nServers; i++) {
	/* i - site*[] idx; j - serverHost idx; k - repHosts idx */

	/* determine if volume-instance type is at site */
	if ((vlentryp->siteFlags[i] & mask) == 0) {
	    /* volume-instance type not on site */
	    continue;
	}

	/* determine if this is an additional address for site */
	if ((vlentryp->siteFlags[i] & VLSF_SAMEASPREV)) {
	    /* site address employed in an earlier iteration */
	    continue;
	}

	/* determine number of site addresses in vldb entry */
	addrCount = 1;

	for (n = i + 1; n < vlentryp->nServers; n++) {
	    if (vlentryp->siteFlags[n] & VLSF_SAMEASPREV) {
		addrCount++;
	    } else {
		break;
	    }
	}

	/* convert address list to a kernel-usable form */
	for (n = 0; n < addrCount; n++) {
	    taddr[n] = *((struct sockaddr_in *) &vlentryp->siteAddr[i + n]);
	    osi_KernelSockaddr(&taddr[n]);
	    taddr[n].sin_port = 0;  /* port is of no use; use rpcd */
	}

	getMoreAddrs = 0;

	/* get FX server reference */
	if (j < CM_VL_MAX_HOSTS) {
	    volp->serverHost[j] = serverp =
		cm_GetServer(cellp,
			     (char *)&vlentryp->sitePrincipal[i].text[0],
			     SRT_FX,
			     &vlentryp->siteObjID[i],
			     taddr,
			     addrCount);

	    if (!(vlentryp->siteFlags[i] & VLSF_PARTIALADDRS)) {
		/* got all FLDB addrs; update server in case was extant */
		cm_SiteAddrReplace(serverp->sitep, taddr, addrCount);
	    } else {
		/* more FLDB addrs; leave server addrs until get all */
		getMoreAddrs = 1;
	    }
	    ++j;
	}

	/* get REP server reference if replica (but not staging replica) */
	if (k < CM_VL_MAX_HOSTS && (volp->states & VL_LAZYREP)) {
	    if (rwix < 0 || bcmp((char *)&vlentryp->siteAddr[i],
				 (char *)&vlentryp->siteAddr[rwix],
				 sizeof(afsNetAddr)) != 0) {
		volp->repHosts[k] = serverp =
		    cm_GetServer(cellp,
				 (char *)&vlentryp->sitePrincipal[i].text[0],
				 SRT_REP,
				 NULL,
				 taddr,
				 addrCount);

		if (!(vlentryp->siteFlags[i] & VLSF_PARTIALADDRS)) {
		    /* got all FLDB addrs; update server in case was extant */
		    cm_SiteAddrReplace(serverp->sitep, taddr, addrCount);
		} else {
		    /* more FLDB addrs; leave server addrs until get all */
		    getMoreAddrs = 1;
		}
		++k;
	    }
	}
#if defined(KERNEL)
	/* Update FX/REP address lists (in background) if there are more
	 * addresses than could be contained in the vldbentry structure.
	 * Note that in this case, server addr lists were left intact to
	 * avoid "forgetting" addresses we already knew (and might be using).
	 */
	if (getMoreAddrs || cm_forceGetMoreAddrs) {
	    cm_FxRepAddrFetch(cellp,
			      (char *)&vlentryp->sitePrincipal[i].text[0],
			      &vlentryp->siteAddr[i]);
	}
#endif
    }

    /* Sort server-sets so can detect changes from previous values */

    cm_SortServers(volp->serverHost, CM_VL_MAX_HOSTS);
    cm_SortServers(volp->repHosts, CM_VL_MAX_HOSTS);

    /* Get a canonical version of the volume name from the fldb entry */

    if (!volp->volnamep && sfx != NULL) {
#ifdef SGIMIPS
	i = (int)strlen((char *)vlentryp->name);
#else  /* SGIMIPS */
	i = strlen((char *)vlentryp->name);
#endif /* SGIMIPS */
	if (i > MAXFTNAMELEN) i = MAXFTNAMELEN;
	j = i;
#ifdef SGIMIPS
	i += (int)strlen(sfx);
#else  /* SGIMIPS */
	i += strlen(sfx);
#endif /* SGIMIPS */
	volp->volnamep = osi_Alloc(i + 1);
	strncpy(volp->volnamep, (char *)vlentryp->name, j);
	volp->volnamep[j] = '\0';
	strcat(volp->volnamep, sfx);
    }

    /* Remember a failed lookup */

    if (isBogus) {
	cm_TurnOnHistorical(volp);
    } else {
	cm_TurnOffHistorical(volp);
    }

    /* If locations of fileset remains "unchanged", we will keep the "bad"
     * status, so that the CM would not try the bad fileset later on.
     * Otherwise, fileset status information must be cleared.
     */

    if (!cm_SameServers(volp->serverHost, oldServers, CM_VL_MAX_HOSTS)) {
	/* FX server-set has changed; clear status info and bump gen count */
	for (i = 0; i < CM_VL_MAX_HOSTS; ++i) {
	    volp->timeBad[i] = 0;
	    volp->perSrvReason[i] = 0;
	}

	volp->hostGen++;

	/* Print message that fileset location has changed */
	if ((serverp = oldServers[0]) && !isBogus) {
	    cm_printf("dfs: fileset %u,,%u (%s) moved in cell %s\n",
		      AFS_HGETBOTH(volp->volume),
		      (volp->volnamep ? volp->volnamep : ""),
		      (cellp->cellNamep ? cellp->cellNamep : "*unk*"));

	    /* Note that this is the first time CM notices a fileset is moved.
	     * If this move is detected at the time of a cascaded-failure (ie.,
	     * TSR-CRASHMOVE), we'd better wait for getting appropriate WRITE 
	     * tokens before we send dirty data (of the fileset in question)
	     * to the target server.
	     */
	    lock_ObtainRead(&serverp->lock);
	    if (serverp->states & (SR_DOWN | SR_TSR)) {
	        volp->states |= VL_TSRCRASHMOVE;
	    }
	    lock_ReleaseRead(&serverp->lock);
	}

	/* THIS SHOULD BE THE TSR MOVE TRIGGER if this is a R/W fileset */
	if (!isBogus) {
	    *gotNewLocP = 1;
	}

    } else if (!cm_SameServers(volp->repHosts, oldReps, CM_VL_MAX_HOSTS)) {
	/* REP server-set has changed; bump generation count */
	volp->hostGen++;
    }
}



#ifdef	KERNEL
/* 
 * XXXX The following so called "overflown" mechanism where any over the 50 incore
 * volume entries are kept on the disk will probably go away and be replaced by a 
 * "traditional" LRU mechanism. So the folllowing routines will probably SOON BECOME
 * OBSOLETE... XXXX 
 */

/*
 * All of the vol cache routines must be called with the cm_volumelock
 * lock held in exclusive mode, since they use static variables.
 * In addition, we don't want two people adding the same volume
 * at the same time.
 */

#ifdef SGIMIPS
static struct cm_fvolume *GetVolCache(register long aslot) 
#else  /* SGIMIPS */
static struct cm_fvolume *GetVolCache(aslot)
    register long aslot; 
#endif /* SGIMIPS */
{
    register struct osi_file *osiFilep;
    register long code;

    if (cm_diskless) panic("memcache getvolcache");
    if (FVIndex != aslot) {
#ifdef AFS_VFSCACHE
	if (!(osiFilep = osi_Open(cm_cache_mp, &cm_volumeHandle)))
#else
	if (!(osiFilep = osi_Open(&cacheDev, cm_volumeInode)))
#endif /* AFS_VFSCACHE */
	    panic("open volumeinode");
#ifdef SGIMIPS
	osi_Seek(osiFilep, (long)(sizeof(struct cm_fvolume) * aslot));
#else  /* SGIMIPS */
	osi_Seek(osiFilep, sizeof(struct cm_fvolume) * aslot);
#endif /* SGIMIPS */
	code = osi_Read(osiFilep, (char *)(&staticFVolume), sizeof(struct cm_fvolume));
	if (code != sizeof(struct cm_fvolume)) 
	    panic("read volumeinfo");
	osi_Close(osiFilep);
	FVIndex = aslot;
    }
    return &staticFVolume;
}


#ifdef SGIMIPS
static struct cm_fvolume *FindVolCache(
    register afs_hyper_t *cellIdp, 
    register afs_hyper_t *volIdp)
#else  /* SGIMIPS */
static struct cm_fvolume *FindVolCache(cellIdp, volIdp)
    register afs_hyper_t *volIdp, *cellIdp; 
#endif /* SGIMIPS */
{
    register long i;
    register struct cm_fvolume *fvolp;

    i = VL_FHASH(*cellIdp, *volIdp);
    for (i = cm_fvTable[i]; i != 0; i = fvolp->next) {
	fvolp  = GetVolCache(i);
	if (AFS_hsame(fvolp->cellId, *cellIdp) && AFS_hsame(fvolp->volume, *volIdp)) 
	    return fvolp;
    }
    return (struct cm_fvolume *) 0;
}


#ifdef SGIMIPS
static void cm_WriteVolCache(void) 
#else  /* SGIMIPS */
static void cm_WriteVolCache() 
#endif /* SGIMIPS */
{
    register struct osi_file *osiFilep;
    register long code;

    if (FVIndex < 0 || FVIndex >= cm_volCounter) 
	panic("volumeinfo alloc");
    if (cm_diskless) panic("memcache writevolcache");
#ifdef AFS_VFSCACHE
    if (!(osiFilep = osi_Open(cm_cache_mp, &cm_volumeHandle)))
#else
    if (!(osiFilep = osi_Open(&cacheDev, cm_volumeInode)))
#endif /* AFS_VFSCACHE */
	panic("open volumeinode");
#ifdef SGIMIPS
    osi_Seek(osiFilep, (long)(sizeof(struct cm_fvolume) * FVIndex));
#else  /* SGIMIPS */
    osi_Seek(osiFilep, sizeof(struct cm_fvolume) * FVIndex);
#endif /* SGIMIPS */
    code = osi_Write(osiFilep, (char *)(&staticFVolume), sizeof(struct cm_fvolume));
    if (code != sizeof(struct cm_fvolume)) 
	panic("write volumeinfo");
    osi_Close(osiFilep);    
}


/* 
 * Routine to get a slot from the volume list--must be called under 
 * cm_volumelock lock (write-locked). 
 */
#ifdef SGIMIPS
static struct cm_volume *GetVolSlot(void) 
#else  /* SGIMIPS */
static struct cm_volume *GetVolSlot() 
#endif /* SGIMIPS */
{
    register struct cm_volume *volp, **tvolp;
    register long i, bestTime;
    struct cm_volume *bestVp, **bestLp;
    struct cm_tokenList tokenl;

    if (!cm_freeVolList) {		/* get free slot */
	if (cm_diskless) {
	    /* very simple algorithm for mem cache entries: just allocate a new
	     * one, since we don't have anything better to swap onto.
	     */
	    volp = (struct cm_volume *) osi_Alloc(sizeof(struct cm_volume));
	    bzero((char *)volp, sizeof(*volp));
	}
	else {
	    /* find a not-recently used volume entry, swap it out and reuse
	     * its memory.  Will allocate new entry only if this fails.
	     */
	    bestTime = 0x7fffffff;
	    bestVp = 0;
	    bestLp = 0;
	    for (i = 0; i < VL_NVOLS; i++) {
		tvolp = &cm_volumes[i];
		for (volp = *tvolp; volp; tvolp = &volp->next, volp = *tvolp) {
		    if (volp->refCount == 0) {	/* is this one available? */
			if (volp->accessTime < bestTime) {	
			    bestTime = volp->accessTime;
			    bestLp = tvolp;
			    bestVp = volp;
			}
		    }
		}
	    }
	    if (!bestVp) { 
		volp = (struct cm_volume *)osi_Alloc(sizeof(struct cm_volume));
		bzero((char *)volp, sizeof(*volp));
		return volp;
	    }
	    /* 
	     * otherwise, recycle the entry, swapping out its permanent data,
	     * return the HERE token to the server.
	     */
	    volp = bestVp;
	    tokenl.token = volp->hereToken;
	    cm_QueueAToken(volp->serverHost[0], &tokenl, 0);
	    bzero((char *)&volp->hereToken, sizeof(afs_token_t));
	    *bestLp = volp->next;
	    if (volp->volnamep) 
		osi_Free(volp->volnamep, strlen(volp->volnamep)+1);
	    volp->volnamep = (char *) 0;
	    icl_Trace1(cm_iclSetp, CM_TRACE_SWAPOUTVOL, ICL_TYPE_POINTER,volp);
	    if (volp->vtix < 0) {	
		/* 
		 * now write out cm_volume structure to file 
		 */	  
		volp->vtix = cm_volCounter++;
		/* 
		 * now put on hash chain 
		 */
		i = VL_FHASH(volp->cellp->cellId, volp->volume);
		staticFVolume.next = cm_fvTable[i];
		cm_fvTable[i]=volp->vtix;
	    } else {
		/* 
		 * haul the guy in from disk so we don't overwrite hash table next chain
		 */
		(void) GetVolCache(volp->vtix);
	    }
	    FVIndex = volp->vtix;
	    staticFVolume.volume = volp->volume;
	    staticFVolume.cellId = volp->cellp->cellId;
	    staticFVolume.mtpoint = volp->mtpoint;
	    staticFVolume.dotdot = volp->dotdot;
	    staticFVolume.rootFid = volp->rootFid;
	    staticFVolume.ustates = (volp->states & VL_SAVEBITS);
	    cm_WriteVolCache();
	}
    } else {
	volp = cm_freeVolList;
	cm_freeVolList = volp->next;
    }
    return volp;
}


/* 
 * Find all the information regarding the volume associated with fidp from 
 * the FLDB server for the particular cell. 
 */
#ifdef SGIMIPS
int cm_CheckVolumeInfo (
     afsFid *fidp,
     register struct cm_rrequest *rreqp,
     struct cm_volume *involp,	
     struct cm_cell *cellp,
     int *gotNewLocP)
#else  /* SGIMIPS */
int cm_CheckVolumeInfo (fidp, rreqp, involp, cellp, gotNewLocP)
     afsFid *fidp;
     register struct cm_rrequest *rreqp;
     struct cm_volume *involp;	
     struct cm_cell *cellp;
     int *gotNewLocP;
#endif /* SGIMIPS */
{
    struct cm_conn *connp;
    struct cm_rrequest treq;
    register long code = 0;
    struct vldbentry *tvep;
    u_long startTime;
    register struct cm_volume *volp;

    *gotNewLocP = 0;
    code = 0;
    cm_FinalizeReq(rreqp);
	/*
	 * Imitate other callers of osi_AllocBufferSpace and don't
	 * check return value.  (osi_Alloc panics if it can't get
	 * requested memory.)
	 */
    tvep = (struct vldbentry *)osi_AllocBufferSpace();

    cm_InitUnauthReq(&treq);       /* *must* be unauth for vldb */
    do {
	if (connp = cm_ConnByMHosts(cellp->cellHosts,  MAIN_SERVICE(SRT_FL),
				    &cellp->cellId, &treq, NULL, cellp, NULL)){
	    icl_Trace0(cm_iclSetp, CM_TRACE_GETVOLBYIDSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    code = VL_GetEntryByID(connp->connp, &fidp->Volume, -1, tvep);
	    osi_PreemptionOff();
	    icl_Trace1(cm_iclSetp, CM_TRACE_GETVOLBYIDEND, ICL_TYPE_LONG,
		       code);
	}
	else 
	    code = -1;
    } while (cm_Analyze(connp, code, (afsFid *) 0, &treq, 
			(struct cm_volume *)NULL, -1, startTime));
    if (code) {
        if (involp) {
	    if (code == VL_NOENT) {
		lock_ObtainWrite(&involp->lock);
		cm_TurnOnHistorical(involp);
		lock_ReleaseWrite(&involp->lock);
	    } else {
		/*
		* We could not get new info from flservers. Turn on the
		* VL_RECHECK in case the fileset is moved later on.
		*/
		lock_ObtainWrite(&involp->lock);
		involp->states |= VL_RECHECK;
		lock_ReleaseWrite(&involp->lock);
	    }
	}
	cm_CopyError(&treq, rreqp);
	goto done;
    }
    if (involp) {
	volp = involp;
    } else {
	volp = cm_GetVolumeByFid(fidp, &treq, 1, 0);
	if (!volp) {	 		/* can't get volume */
	    cm_CopyError(&treq, rreqp);
	    goto done;
	}
    }
    /* 
     * Have info, copy into serverHost array 
     */
    lock_ObtainWrite(&volp->lock);
    cm_InstallVolumeEntry(volp, tvep, cellp, gotNewLocP);
    if (rreqp && (volp->states & VL_HISTORICAL)) {
	cm_FinalizeReq(rreqp);
	rreqp->volumeError = 1;
    }
    lock_ReleaseWrite(&volp->lock);
    if (!involp) 
	cm_PutVolume(volp);		/* got it here: put it here */

    code = 0;				/* Return zero unconditionally */
done:
    osi_FreeBufferSpace((struct osi_buffer *)tvep);
#ifdef SGIMIPS
    return ((int)code);
#else  /* SGIMIPS */
    return (code);
#endif /* SGIMIPS */
}

#endif	/* KERNEL */
