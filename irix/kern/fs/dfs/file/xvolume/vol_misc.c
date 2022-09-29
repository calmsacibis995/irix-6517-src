/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: vol_misc.c,v 65.7 1998/04/01 14:16:47 gwehrman Exp $";
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
 * $Log: vol_misc.c,v $
 * Revision 65.7  1998/04/01 14:16:47  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for volreg.h and zlc.h.
 *
 * Revision 65.6  1998/03/24 17:20:28  lmc
 * Changed an osi_mutex_t to a mutex_t and changed osi_mutex_enter() to
 * mutex_lock and osi_mutex_exit to mutex_unlock.
 * Added a name parameter to osi_mutex_init() for debugging purposes.
 *
 * Revision 65.5  1998/03/23  16:46:59  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/02/24 15:34:34  lmc
 * Added lots of prototypes and made sure that return codes were
 * correctly defined as int or long.
 *
 * Revision 65.2  1997/11/06  20:01:48  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:21:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.739.1  1996/10/02  19:04:46  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  19:00:06  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/lock.h>
#include <dcedfs/volume.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/tpq.h>
#include <dcedfs/vol_errs.h>
#include <vol_init.h>
#include <vol_trace.h>
#ifdef SGIMIPS
#include <dcedfs/volreg.h>
#include <dcedfs/zlc.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvolume/RCS/vol_misc.c,v 65.7 1998/04/01 14:16:47 gwehrman Exp $")

/* Fwd decls */
#ifdef SGIMIPS
static vol_vp_elem_p_t vol_GetFreeVpElem(void);
static void vol_AllocFreeVpElem(void);
#else
static vol_vp_elem_p_t vol_GetFreeVpElem();
static void vol_AllocFreeVpElem();
#endif
static void vol_FreeVpElem(vol_vp_elem_p_t);

/*
 * Volume package related globals
 */

/* Import the current value from the TKM */
extern u_long tkm_expiration_default;

/* Allow this to be overridden, for quicker testing */
#ifndef MAX_TOKEN_LIFETIME
#define MAX_TOKEN_LIFETIME (tkm_expiration_default + ((25*60)+VOL_DESCTIMEOUT))
#endif

struct lock_data vol_vollock;		/* allocation lock for volumes */
struct volume *vol_FreeListp = 0;	/* Volume struct free list */
static long (*vol_fxActionProc)() = 0;	/* handle to enqueue exporter cleanup 
					 * actions */
static struct lock_data vol_actionlock;
static void* vol_tpqPool = 0;

/* Encourage movement towards the brave new world of mutexes and cv's etc !! */
static osi_mutex_t vol_freeVpLock;  /* lock after obtaining volp write lock */
static vol_vp_elem_p_t vol_freeVpList;
static u_int vol_freeVpCount = 0;

/*
 * Initialize the structures used by this module
 */
static vol_initialized = 0;
#ifdef SGIMIPS
/* EXPORT */ void vol_Init(void)
#else
/* EXPORT */ void vol_Init()
#endif
{
    extern struct lock_data vol_attlock;
    register struct vol_calldesc *cdp;
    register int i;

    if (vol_initialized)
	return;

    vol_initialized = 1;
    icl_Trace0(xops_iclSetp, XVOL_TRACE_VOLINITCALLED);

    vol_InitDescMod();
    vol_InitAttVfsMod();
    vol_fxActionProc = 0;

    lock_Init(&vol_vollock);
    lock_Init(&vol_actionlock);
#if defined(SGIMIPS) && defined(_KERNEL)
    osi_mutex_init(&vol_freeVpLock, "vol_free");
#else
    osi_mutex_init(&vol_freeVpLock);
#endif
    
    bzero((char *)vol_descs, sizeof(struct vol_callDesc *) * VOL_NODESC);
    cdp = (struct vol_calldesc *) osi_Alloc(VOL_DESCALLOC * sizeof(*cdp));
    vol_freeCDList = (struct vol_calldesc *) &(cdp[0]);
    for (i = 0; i < VOL_DESCALLOC - 1; i++ )
	cdp[i].lruq.next = (struct squeue *) (&cdp[i+1]);
    cdp[VOL_DESCALLOC-1].lruq.next = (struct squeue *) 0;
    vol_tpqPool = tpq_Init(0 /* minThreads*/,
			   2 /* medMaxThreads */,
			   2 /* highMaxThreads */,
			   12*60 /* threadEnnui */,
			   "xvl" /* threadNamePrefix */);
}

/*
 * Allow enqueued actions to be processed.
 */
#ifdef SGIMIPS
/* EXPORT */ void vol_SetActionProc(
    long (*proch)())
#else
/* EXPORT */ void vol_SetActionProc(proch)
    long (*proch)();
#endif
{
    if (vol_initialized == 0)
	vol_Init();
    lock_ObtainWrite(&vol_actionlock);
    vol_fxActionProc = proch;
    lock_ReleaseWrite(&vol_actionlock);
}

#ifdef SGIMIPS
static long vol_CallAction(
    register struct volume *volp,
    int actionh)
#else
static long vol_CallAction(volp, actionh)
    register struct volume *volp;
    int actionh;
#endif
{/* Enqueue an action on the volp, if possible. */
    long code;

    lock_ObtainRead(&vol_actionlock);
    if (vol_fxActionProc)
	code = (*vol_fxActionProc)(volp, actionh);
    else
	code = -1;
    lock_ReleaseRead(&vol_actionlock);
    return code;
}

/*
 * Link volume's ZLC list with DFS'
 */
#ifdef SGIMIPS
static int LinkZLCList(
  register struct volume *volp,
  int isExported,
  int isReplicated)
#else
static int LinkZLCList(volp, isExported, isReplicated)
  register struct volume *volp;
  int isExported;
  int isReplicated;
#endif
{
    int code = 0;
    unsigned int iter;
    struct vnode *vp;
    struct afsFid afsfid;	/* AFS standard fid */

    icl_Trace3(xops_iclSetp, XVOL_TRACE_LINKZLC,
	       ICL_TYPE_POINTER, volp,
	       ICL_TYPE_LONG, isExported,
	       ICL_TYPE_LONG, isReplicated);
    iter = 0;
    while(1) {
	vp = (struct vnode *)0;
	code = VOL_GETZLC(volp, &iter, &vp);
	icl_Trace3(xops_iclSetp, XVOL_TRACE_GOTZLC,
		   ICL_TYPE_LONG, iter,
		   ICL_TYPE_POINTER, vp,
		   ICL_TYPE_LONG, code);
	/* break out of loop on error or null vnode ptr (no more left) */
	if (code || (vp == (struct vnode *)0)) 
	    break;

	VOPX_AFSFID(vp, &afsfid, /* wantv? */1, code);
	icl_Trace2(xops_iclSetp, XVOL_TRACE_GOTZLCFID,
		   ICL_TYPE_FID, &afsfid,
		   ICL_TYPE_LONG, code);
	if (code == 0) {
	    zlc_TryRemove(vp, &afsfid);
	}
	OSI_VN_RELE(vp);
    }
    icl_Trace1(xops_iclSetp, XVOL_TRACE_LINKZLC_DONE,
	       ICL_TYPE_LONG, code);
    return (code);
}

/*
  * Attach the specified volume
 */
#ifdef SGIMIPS
/* EXPORT */ int vol_Attach(
    register afs_hyper_t *volIdp,
    register struct vol_status *statusp,
    register struct aggr *aggrp,
    struct volumeops *volopsp)
#else
/* EXPORT */ int vol_Attach(volIdp, statusp, aggrp, volopsp)
    register afs_hyper_t *volIdp;
    register struct vol_status *statusp;
    register struct aggr *aggrp;
    struct volumeops *volopsp;
#endif
{
    register struct volume *volp;
    vol_vp_elem_p_t vpList;
    int code;

    icl_Trace2(xops_iclSetp, XVOL_TRACE_VOLATTACH,
		ICL_TYPE_HYPER, volIdp,
		ICL_TYPE_POINTER, aggrp);
    if (!vol_initialized)
	vol_Init();
    lock_ObtainWrite(&vol_vollock);
    if (!vol_FreeListp) {
	volp = (struct volume *) osi_Alloc(sizeof(struct volume));
    } else {
	volp = vol_FreeListp;
	vol_FreeListp = (struct volume *) (volp->v_lruq.next);
    }
    bzero((char *)volp, sizeof(struct volume));
    volp->v_stat_st = statusp->vol_st;
    volp->v_stat_st.tokenTimeout = 0;
    volp->v_stat_st.activeVnops = 0;
    /*
     * If this volume was involved in an interrupted move, we set the most
     * conservative timeout possible.  After the timeout expires, the
     * appropriate cleanup action will occur.
     */
    if (statusp->vol_st.states & (VOL_MOVE_SOURCE | VOL_MOVE_TARGET))
	volp->v_stat_st.volMoveTimeout = osi_Time() + MAX_TOKEN_LIFETIME;
    else
	volp->v_stat_st.volMoveTimeout = 0;
    /* Now hold it for our exclusive use until it's initialized, as if
      vol_open had been called */
    volp->v_states |= (VOL_BUSY|VOL_OPENDONE);
    volp->v_concurrency = VOL_CONCUR_NOOPS;
    volp->v_accError = VOLERR_TRANS_GETSTATUS;
    volp->v_accStatus = VOL_OP_NOACCESS;
    volp->v_procID = osi_GetPid();
    volp->v_volOps = volopsp;
    volp->v_vp = NULL;
    lock_Init(&volp->v_lock);
    VOL_HOLD (volp);
    AG_HOLD (aggrp);			/* So it won't disappear on us */
    aggrp->a_nVolumes++;
    volp->v_paggrp = aggrp;
    volp->v_volId = *volIdp;
    (void) VOL_SETDYSTAT(volp, &(statusp->vol_dy));
    code = volreg_Enter(&volp->v_volId, volp, volp->v_volName);
    lock_ReleaseWrite(&vol_vollock);
    if (code) {
	/* Do the equivalent of a VOLOP_CLOSE, though it's going away */
	lock_ObtainWrite(&volp->v_lock);
	volp->v_states |= VOL_DEADMEAT;
	/* Should not have to wakeup anyone or rele any deferred vnodes */
	(void) vol_close(volp, &vpList);
	osi_assert(vpList == NULL);

	VOL_FREEDYSTAT(volp);	/* UFS also clears some aggr state here */
	aggrp = volp->v_paggrp;
	lock_ReleaseWrite(&volp->v_lock);

	lock_ObtainWrite(&vol_vollock);
	aggrp->a_nVolumes--;
	AG_RELE(aggrp);
	lock_ReleaseWrite(&vol_vollock);
    } else {
	VOL_ATTACH(volp);
	icl_Trace2(xops_iclSetp, XVOL_TRACE_CHECK_ZLC,
		   ICL_TYPE_POINTER, volp,
		   ICL_TYPE_LONG, statusp->vol_st.states);
	if (((statusp->vol_st.states & VOL_DELONSALVAGE) == 0) &&
	    ((statusp->vol_st.states & VOL_TYPEFIELD) == VOL_TYPE_RW)) {
	    /*
	     * Add volume ZLC list to DFS ZLC list if volp is intact
	     * and it refers to a RW fileset.
	     */
	    code = LinkZLCList(volp,
			       !(statusp->vol_st.states & VOL_NOEXPORT),
			       statusp->vol_st.states & VOL_IS_REPLICATED);
	}
	/* Now VOLOP_CLOSE it to bring it into service */
	lock_ObtainWrite(&volp->v_lock);
	(void) vol_close(volp, NULL);
	lock_ReleaseWrite(&volp->v_lock);
    }
    /* This should invoke vol_VolInactive(), below, on error. */
    VOL_RELE (volp); 
    return (code);
}


/*
 * Detach the volume, volp
 */
/* EXPORT */ int vol_Detach(struct volume *volp, int anyForce)
{
    register struct aggr *aggrp;
#ifdef SGIMIPS
    int code;		/* Returns an int - not a long */
#else
    long code;
#endif

    icl_Trace3(xops_iclSetp, XVOL_TRACE_VOLDETACH,
		ICL_TYPE_POINTER, volp,
		ICL_TYPE_HYPER, &volp->v_volId,
		ICL_TYPE_LONG, (u_long) volp->v_count);
    if (!vol_initialized)
	vol_Init();

    /* Clear any queued cleanup request */
    /* This may leak the 8-byte argument block to the TPQ proc. */
    if (volp->v_tpqRequest) {
	icl_Trace2(xops_iclSetp, XVOL_TRACE_VOLDETACH_TPQ,
		   ICL_TYPE_POINTER, volp,
		   ICL_TYPE_POINTER, volp->v_tpqRequest);
	tpq_DequeueRequest(vol_tpqPool, volp->v_tpqRequest);
	volp->v_tpqRequest = NULL;
    }

    if (code = VOL_DETACH (volp, anyForce)) { /* (obtains/releases v_lock) */
	return code;
    }

    if (code = volreg_Delete(&volp->v_volId, volp->v_volName)) {
	return code;
    }

    lock_ObtainWrite(&volp->v_lock);
    VOL_FREEDYSTAT(volp);	/* UFS also clears some aggr state here */
    aggrp = volp->v_paggrp;
    lock_ReleaseWrite(&volp->v_lock);

    lock_ObtainWrite(&vol_vollock);
    aggrp->a_nVolumes--;
    AG_RELE(aggrp);
    lock_ReleaseWrite(&vol_vollock);
    return 0;
}

/*
 * Volume's reference count is being decremented from 1.
 * Called with volp->v_lock write-locked.
 */
#ifdef SGIMIPS
/* EXPORT */ int vol_VolInactive(register struct volume *volp)
#else
/* EXPORT */ int vol_VolInactive(volp)
    register struct volume *volp;
#endif
{
    volp->v_count--;
    lock_ReleaseWrite(&volp->v_lock);

    /* Put the struct volume on the free list */
    lock_ObtainWrite(&vol_vollock);
    volp->v_lruq.next = (struct squeue *) vol_FreeListp;
    vol_FreeListp = volp;
    lock_ReleaseWrite(&vol_vollock);

    return 0;
}

#ifdef SGIMIPS
/* EXPORT */ void vol_SwapIdentities(
    register struct volume *vol1p,
    register struct volume *vol2p)
#else
/* EXPORT */ void vol_SwapIdentities(vol1p, vol2p)
    register struct volume *vol1p;
    register struct volume *vol2p;
#endif
{/* Actually, we swap everything but the identities! */
 /* MUST be called with both volumes' v_lock WRITE-LOCKED. */
    long code;
    struct vol_stat_st	statbuf1, statbuf2;
    /* variables used in the structure swap: */
    struct aggr	*v_paggrp;
    opaque		v_fsDatap;

    icl_Trace4(xops_iclSetp, XVOL_TRACE_SWAPIDENTS,
	       ICL_TYPE_POINTER, vol1p,
	       ICL_TYPE_HYPER, &vol1p->v_volId,
	       ICL_TYPE_POINTER, vol2p,
	       ICL_TYPE_HYPER, &vol2p->v_volId);
    osi_assert(lock_WriteLocked(&vol1p->v_lock));
    osi_assert(lock_WriteLocked(&vol2p->v_lock));
    osi_assert(vol1p->v_volOps == vol2p->v_volOps);
    /* statbuf2 will overwrite vol2p->v_stat_st */
    /* Copy everything from vol1p except the volid. */
    statbuf2 = vol1p->v_stat_st;
    statbuf2.volId = vol2p->v_volId;
    strcpy(statbuf2.volName, vol2p->v_volName);
    /* statbuf1 will overwrite vol1p->v_stat_st */
    /* Copy everything from vol2p except the volid. */
    statbuf1 = vol2p->v_stat_st;
    statbuf1.volId = vol1p->v_volId;
    strcpy(statbuf1.volName, vol1p->v_volName);
    /* Preserve the VOL_BITS_NOSWAP flags in their original places */
    statbuf2.states = (vol1p->v_states & ~VOL_BITS_NOSWAP)
		      | (vol2p->v_states & VOL_BITS_NOSWAP);
    statbuf1.states = (vol2p->v_states & ~VOL_BITS_NOSWAP)
		      | (vol1p->v_states & VOL_BITS_NOSWAP);
    /* Now copy the status blocks into place. */
    vol2p->v_stat_st = statbuf2;
    vol1p->v_stat_st = statbuf1;
#define SWAPFLD(FLD)     FLD = vol1p->FLD; \
		vol1p->FLD = vol2p->FLD; \
		vol2p->FLD = FLD;
    SWAPFLD(v_paggrp);
    SWAPFLD(v_fsDatap);
}


/*
 * Swap volids for the two given volumes.
 */
/* EXPORT */ int vol_SwapIDs(struct volume *vol1p, struct volume *vol2p)
{
    int code;

    /* Lock both volumes down, in some defined order. */
    if (AFS_hcmp(vol1p->v_volId, vol2p->v_volId) < 0) {
	lock_ObtainWrite(&vol1p->v_lock);
	lock_ObtainWrite(&vol2p->v_lock);
    } else {
	lock_ObtainWrite(&vol2p->v_lock);
	lock_ObtainWrite(&vol1p->v_lock);
    }
    /*
     * Change the ``struct volume'' static status structures themselves.
     */
    vol_SwapIdentities(vol1p, vol2p);

    /*
     * Change the ID in the FS-specific part
     */
    if (code = VOL_SWAPIDS(vol1p, vol2p, osi_getucred()))
	vol_SwapIdentities(vol1p, vol2p); /* Undo the swap */

    lock_ReleaseWrite(&vol1p->v_lock);
    lock_ReleaseWrite(&vol2p->v_lock);
    return code;
}

/* MUST LIST HERE any fileset states bit that would cause a conflict with a vnode op */
#define ALL_VOL_STATES (VOL_BUSY | VOL_DEADMEAT | VOL_CLONEINPROG \
			| VOL_ZAPME | VOL_DELONSALVAGE \
			| VOL_OUTOFSERVICE | VOL_OFFLINE \
			| VOL_MOVE_TARGET | VOL_MOVE_SOURCE)

/* Expire the moveTimeout field in volume states. */
/* Called to make any pending move-cleanup transition. */
#ifdef SGIMIPS
static void vol_doMoveTimeout(
  IN register struct volume *volp)
#else
static void vol_doMoveTimeout(volp)
  IN register struct volume *volp;
#endif
{
    register unsigned long sts;
    struct vol_status newStat;

    osi_assert(lock_WriteLocked(&volp->v_lock));
    sts = volp->v_states & ALL_VOL_STATES;
    osi_assert((sts & VOL_BUSY) == 0);
    /* Check for timeout. */
    if (volp->v_stat_st.volMoveTimeout != 0
	 && volp->v_stat_st.volMoveTimeout <= osi_Time()
	 && (sts & VOL_DEADMEAT) == 0) {
	/* moveTimeout just expired. */
	icl_Trace4(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT,
		   ICL_TYPE_POINTER, volp,
		   ICL_TYPE_HYPER, &volp->v_volId,
		   ICL_TYPE_LONG, volp->v_states,
		   ICL_TYPE_LONG, volp->v_stat_st.volMoveTimeout);
	volp->v_stat_st.volMoveTimeout = 0;
	newStat.vol_st.states = volp->v_states;
	newStat.vol_st.volMoveTimeout = 0;
	/* We know of only a few cases here. */
	/* For the rest, just complain to the console. */
	switch (sts) {
	    case (VOL_MOVE_TARGET | VOL_DELONSALVAGE | VOL_ZAPME):
	    case (VOL_MOVE_TARGET | VOL_DELONSALVAGE):
	    case (VOL_MOVE_TARGET | VOL_ZAPME):
		icl_Trace0(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_0);
		/* Target timed out before it was finished. */
		/* Turn MOVE_TARGET OFF since we're not automatically zapping
		 * the fileset yet
		 */
		newStat.vol_st.states &= ~VOL_MOVE_TARGET;
		VOL_SETSTATUS(volp, VOL_STAT_STATES | VOL_STAT_VOLMOVETIMEOUT,
			      &newStat);
		vol_CallAction(volp, 0);
		break;

	    case VOL_MOVE_TARGET:
		/* Timed out while changing the FLDB entry and getting token. */
		newStat.vol_st.states |= VOL_OUTOFSERVICE;
		newStat.vol_st.volMoveTimeout =
		  osi_Time() + MAX_TOKEN_LIFETIME;
		icl_Trace1(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_1,
			   ICL_TYPE_LONG, newStat.vol_st.volMoveTimeout);
		VOL_SETSTATUS(volp, VOL_STAT_STATES | VOL_STAT_VOLMOVETIMEOUT,
			      &newStat);
		vol_CallAction(volp, 3);
		break;

	    case VOL_MOVE_SOURCE:
		/* Timed out while changing the FLDB entry. */
		newStat.vol_st.states |= VOL_OUTOFSERVICE;
		newStat.vol_st.volMoveTimeout =
		  osi_Time() + MAX_TOKEN_LIFETIME;
		icl_Trace1(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_2,
			   ICL_TYPE_LONG, newStat.vol_st.volMoveTimeout);
		VOL_SETSTATUS(volp, VOL_STAT_STATES | VOL_STAT_VOLMOVETIMEOUT,
			      &newStat);
		vol_CallAction(volp, 2);
		break;

	    case (VOL_MOVE_SOURCE | VOL_OFFLINE):
		/* Timed out while getting the THERE token. */
		newStat.vol_st.states |= VOL_OUTOFSERVICE;
		newStat.vol_st.volMoveTimeout =
		  osi_Time() + MAX_TOKEN_LIFETIME;
		icl_Trace1(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_3,
			   ICL_TYPE_LONG, newStat.vol_st.volMoveTimeout);
		VOL_SETSTATUS(volp, VOL_STAT_STATES | VOL_STAT_VOLMOVETIMEOUT,
			      &newStat);
		vol_CallAction(volp, 1);
		break;

	    case (VOL_MOVE_TARGET | VOL_OUTOFSERVICE):
		/* TARGET timed out earlier */
		newStat.vol_st.states &= ~(VOL_MOVE_TARGET | VOL_OUTOFSERVICE);
		icl_Trace0(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_4);
		VOL_SETSTATUS(volp, VOL_STAT_STATES | VOL_STAT_VOLMOVETIMEOUT,
			      &newStat);
		break;

	    case (VOL_MOVE_SOURCE | VOL_OUTOFSERVICE):
		/* SOURCE timed out earlier */
		newStat.vol_st.states &= ~(VOL_MOVE_SOURCE | VOL_OUTOFSERVICE);
		icl_Trace0(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_5);
		VOL_SETSTATUS(volp, VOL_STAT_STATES | VOL_STAT_VOLMOVETIMEOUT,
			      &newStat);
		break;

	    case (VOL_MOVE_SOURCE | VOL_OFFLINE | VOL_OUTOFSERVICE):
		/* Timed out getting THERE, then waited 4.5 hours. */
		/* XXX Ideally, we'd just delete this now, but we'll set */
		/* VOL_ZAPME and clear VOL_MOVE_SOURCE for now */
		newStat.vol_st.states |= VOL_ZAPME;
		newStat.vol_st.states &= ~VOL_MOVE_SOURCE;
		icl_Trace0(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_6);
		VOL_SETSTATUS(volp, VOL_STAT_STATES | VOL_STAT_VOLMOVETIMEOUT,
			      &newStat);
		break;

	    case (VOL_MOVE_SOURCE | VOL_DELONSALVAGE | VOL_ZAPME):
	    case (VOL_MOVE_SOURCE | VOL_DELONSALVAGE):
	    case (VOL_MOVE_SOURCE | VOL_ZAPME):
		icl_Trace0(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_7);
		/* Source timed out while being deleted. */
		/* Turn MOVE_SOURCE OFF since we're not automatically zapping
		 * the fileset yet
		 */
		newStat.vol_st.states &= ~VOL_MOVE_SOURCE;
		VOL_SETSTATUS(volp, VOL_STAT_STATES | VOL_STAT_VOLMOVETIMEOUT,
			      &newStat);
		vol_CallAction(volp, 0);
		break;

	    default:
		printf("fx: fileset %u,,%u has unrecognized timeout state: 0x%x\n",
		       AFS_HGETBOTH(volp->v_volId),
		       volp->v_states);
		icl_Trace3(xops_iclSetp, XVOL_TRACE_MOVETIMEOUT_8,
			   ICL_TYPE_POINTER, volp,
			   ICL_TYPE_HYPER, &volp->v_volId,
			   ICL_TYPE_LONG, volp->v_states);
		break;
	}
    }
}

/* Drive the move-timeout stuff to stability. */
/* Called as a worker from TPQ and from ops that turn off VOL_BUSY. */
#ifdef SGIMIPS
static void vol_advanceMove(
  struct volume *volp)
#else
static void vol_advanceMove(volp)
  struct volume *volp;
#endif
{
    while (volp->v_stat_st.volMoveTimeout != 0
	 && volp->v_stat_st.volMoveTimeout <= osi_Time()
	 && (volp->v_states & VOL_DEADMEAT) == 0) {
	icl_Trace3(xops_iclSetp, XVOL_TRACE_ADVANCE_MOVE,
			   ICL_TYPE_POINTER, volp,
			   ICL_TYPE_HYPER, &volp->v_volId,
			   ICL_TYPE_LONG, volp->v_stat_st.volMoveTimeout);
	vol_doMoveTimeout(volp);
    }
}

/* Called asynchronously from TPQ to drive the move timeout. */
/* If the vol is not busy, we can work.  If it's busy, comparable processing
 * in vol_SetMoveTimeoutTrigger will be called when VOL_BUSY is cleared.
 */
#ifdef SGIMIPS
static void vol_DriveMove(
  void *opaque_volidp,
  void *opaque_tpqEntry)
#else
static void vol_DriveMove(opaque_volidp, opaque_tpqEntry)
  void *opaque_volidp;
  void *opaque_tpqEntry;
#endif
{
    register afs_hyper_t *idp = (afs_hyper_t *)opaque_volidp;
    struct afsFid dummyFid;
    long reschedTime = 0;	/* by default, don't reschedule us */
    struct volume *volp;
    int code;

    bzero((char *)&dummyFid, sizeof(dummyFid));
    dummyFid.Volume = *idp;
    code = volreg_Lookup(&dummyFid, &volp);
    if (code) {
	/* Couldn't find the fileset--maybe a race with vol_Detach.  Ignore
	 * the problem.
	 */
	icl_Trace2(xops_iclSetp, XVOL_TRACE_DRIVE_MOVE_0,
		   ICL_TYPE_HYPER, idp,
		   ICL_TYPE_LONG, code);
	/* Don't let this TPQ repeat! */
	tpq_SetRescheduleInterval(opaque_tpqEntry, 0);
	osi_Free((void *)idp, sizeof(afs_hyper_t));
	return;
    }
    icl_Trace2(xops_iclSetp, XVOL_TRACE_DRIVE_MOVE,
			   ICL_TYPE_POINTER, volp,
			   ICL_TYPE_HYPER, idp);
    lock_ObtainWrite(&volp->v_lock);
    if (volp->v_states & VOL_BUSY) {
	/* Do nothing: vol_SetMoveTimeoutTrigger will be called when
	 * VOL_BUSY is turned off.
	 */
	icl_Trace2(xops_iclSetp, XVOL_TRACE_DRIVE_MOVE_1,
		   ICL_TYPE_POINTER, volp,
		   ICL_TYPE_LONG, volp->v_states);
    } else {
	/* Advance the cleanup state as necessary. */
	vol_advanceMove(volp);
	/* See if there's more to be done. */
	if (volp->v_stat_st.volMoveTimeout != 0
	    && (volp->v_states & VOL_DEADMEAT) == 0) {
	    /* Reschedule this procedure for the coming timeout. */
	    reschedTime = volp->v_stat_st.volMoveTimeout - osi_Time();
	    if (reschedTime <= 0)
		reschedTime = 1;	/* something > 0 */
	    icl_Trace2(xops_iclSetp, XVOL_TRACE_DRIVE_MOVE_2,
		       ICL_TYPE_POINTER, volp,
		       ICL_TYPE_LONG, reschedTime);
	} else {
	    icl_Trace3(xops_iclSetp, XVOL_TRACE_DRIVE_MOVE_3,
		      ICL_TYPE_POINTER, volp,
		      ICL_TYPE_LONG, volp->v_states,
		      ICL_TYPE_LONG, volp->v_stat_st.volMoveTimeout);
	}
    }
    /* Reschedule as necessary */
    tpq_SetRescheduleInterval(opaque_tpqEntry, reschedTime);
    if (reschedTime == 0) {
	icl_Trace3(xops_iclSetp, XVOL_TRACE_DRIVE_MOVE_4,
		   ICL_TYPE_POINTER, volp,
		   ICL_TYPE_POINTER, volp->v_tpqRequest,
		   ICL_TYPE_POINTER, idp);
	volp->v_tpqRequest = 0;
	osi_Free((void *)idp, sizeof(afs_hyper_t));
    }
    lock_ReleaseWrite(&volp->v_lock);
}

/* This is called whenever VOL_BUSY is cleared and there's fileset-move
 * involvement, indicated via volMoveTimeout and/or VOL_MOVE_SOURCE or
 * VOL_MOVE_TARGET.  The idea is that if there's move-cleanup processing to
 * be done, we do as much as we can now, and then if there's more to do, we
 * make sure that there's a TPQ task that will carry it out.  This procedure
 * will both initiate move-cleanup scheduling on a fileset without a TPQ
 * request as well as continue the cleanup processing if the TPQ task
 * happened to fire while VOL_BUSY was set in the fileset.
 */
#ifdef SGIMIPS
/* EXPORT */ void vol_SetMoveTimeoutTrigger(
  IN register struct volume *volp)
#else
/* EXPORT */ void vol_SetMoveTimeoutTrigger(volp)
  IN register struct volume *volp;
#endif
{
    afs_hyper_t *idp;
    long reschedTime;

    osi_assert(lock_WriteLocked(&volp->v_lock));
    osi_assert((volp->v_states & VOL_BUSY) == 0);
    vol_advanceMove(volp);
    /* Do nothing further here if there's already a pending TPQ */
    /* But start up a new TPQ if there's more to do. */
    if (!volp->v_tpqRequest
	&& volp->v_stat_st.volMoveTimeout != 0
	&& (volp->v_states & VOL_DEADMEAT) == 0) {
	idp = (afs_hyper_t *) osi_Alloc(sizeof(afs_hyper_t));
	*idp = volp->v_volId;
	/* Ensure that we run this at least a bit into the future */
	reschedTime = volp->v_stat_st.volMoveTimeout - osi_Time();
	if (reschedTime <= 0) reschedTime = 1;
	volp->v_tpqRequest = tpq_QueueRequest(vol_tpqPool /* queue hdl */,
					      vol_DriveMove /* op */,
					      (void *)idp /* arg */,
					      TPQ_MEDPRI /* pri */,
					      5*60 /* grace period */,
					      reschedTime /* resched */,
					      0 /* dropDead */);
	icl_Trace4(xops_iclSetp, XVOL_TRACE_SET_MOVE_TRIGGER,
		   ICL_TYPE_POINTER, volp,
		   ICL_TYPE_HYPER, &volp->v_volId,
		   ICL_TYPE_LONG, volp->v_states,
		   ICL_TYPE_LONG, volp->v_stat_st.volMoveTimeout);
	icl_Trace3(xops_iclSetp, XVOL_TRACE_SET_MOVE_TRIGGER_0,
		   ICL_TYPE_POINTER, idp,
		   ICL_TYPE_LONG, reschedTime,
		   ICL_TYPE_POINTER, volp->v_tpqRequest);
    }
}

/* Special case for AIX VMM: only check for busy flag, otherwise
 * succeed.  And never block.
 * waitFlags specifies which flags we're waiting for.  Typically
 * are VOL_BUSY or VOL_OPENDONE.
 */
#ifdef SGIMIPS
/* EXPORT */ int vol_fsStartBusyOp(
  IN struct volume *volp,
  IN int waitFlags,
  IN opaque *dmpp)
#else
/* EXPORT */ int vol_fsStartBusyOp(volp, waitFlags, dmpp)
  IN struct volume *volp;
  IN int waitFlags;
  IN opaque *dmpp;
#endif
{
    int code;

    lock_ObtainWrite(&volp->v_lock);
    if (!(volp->v_states & waitFlags)) {
	volp->v_activeVnops++;
	code = 0;
    }
    else {
	code = volp->v_accError;
	osi_assert(code);
    }
    lock_ReleaseWrite(&volp->v_lock);
    return code;
}

/* start a vnode operation for a particular volume.
 * optype tells what type of operation we're doing (VNOP_TYPE_READWRITE,
 * VNOP_TYPE_READONLY, or VNOP_TYPE_NOOP).
 * bit VOL_XCODE_NOWAIT --> return error immediately
 * bit VOL_XCODE_SOURCEOK --> move source OK
 *     (if set, this flag is OK, otherwise error)
 * bit VOL_XCODE_TARGETOK --> move target OK
 * bit VOL_XCODE_RECURSIVE --> don't block to give ftserver priority,
 *     since we're called recursively, and could deadlock.
 */
#ifdef SGIMIPS
/* EXPORT */ int vol_fsStartVnodeOp(
    IN  register struct volume *volp,
    IN  int optype,
    IN  int xcode,
    IN  opaque *dmpp)
#else
/* EXPORT */ int vol_fsStartVnodeOp(volp, optype, xcode, dmpp)
    IN  register struct volume *volp;
    IN  int optype;
    IN  int xcode;
    IN  opaque *dmpp;
#endif
{ /* vol_StartVnodeOp */
#ifdef SGIMIPS
    register int code;				/* Return code */
#else
    register long code;				/* Return code */
#endif
    register unsigned long sts;			/* volume status temp */
    register int serverOp;			/* called by ftserver */

    if (!volp) return 0;
    for (;;) {
	/* First, optionally check for a busy volume. */
	lock_ObtainWrite(&volp->v_lock);
	/* see if there's already a conflicting volume op running,
	 * and either wait for it, return an error, or go for it
	 * if we're compatible with the volume operation.
	 */
	/* ALL_VOL_xxx *must* list all states bits that could create conflict */
	sts = volp->v_states & ALL_VOL_STATES;
	code = 0;
	serverOp = 0;
	if (sts != 0) {
	    /* need to do time-consuming case analysis */
	    if (sts & VOL_BUSY) {
		code = volp->v_accError;
		osi_assert(code);
		/* if we're called by the ftserver, don't block, since many
		    * volume ops call vnode ops.
		    */
		if (volp->v_procID && volp->v_procID == osi_GetPid()) {
		    serverOp = 1;
		    code = 0;	/* just let the vnode op (for this server) occur */
		} else {
		    /*
		     * But don't allow any concurrent vnops if the fileset has
		     * any of the other access-control bits on--any other
		     * than VOL_BUSY.
		     */
		    if (VOL_VNOP_COMPAT(volp->v_concurrency, optype)
			&& sts == VOL_BUSY) {
			code = 0;	/* compatible: let it proceed */
		    }
		    /* OK, not compatible.  Wait if transient (i.e. will get better)
			and allowed. */
		    else if (!(xcode & VOL_XCODE_NOWAIT)
			     && VOL_ERROR_IS_TRANSIENT(code)) {
			/* Incompatible error.  Wait only if it's transient. */
			volp->v_states |= VOL_LOOKUPWAITING;
			icl_Trace2(xops_iclSetp, XVOL_TRACE_STARTVNWAITING,
				   ICL_TYPE_POINTER, volp,
				   ICL_TYPE_LONG, (u_long) code);
			osi_SleepW(&volp->v_states, &volp->v_lock);
			continue;/* the big loop */
		    } /* else we're not waiting, or it's an incompatible
			  persistent error, and we simply return it. */
		}
	    } else {
		/* Translate the separate error bits into an error code. */
		if (sts & VOL_DEADMEAT) {
		    code = VOLERR_PERS_DELETED;
		} else if (sts & VOL_CLONEINPROG) {
		    code = VOLERR_PERS_CLONECLEAN;
		} else if (sts & (VOL_DELONSALVAGE|VOL_ZAPME)) {
		    code = VOLERR_PERS_DAMAGED;
		}
		/*
		 * Only VOL_MOVE_SOURCE, VOL_MOVE_TARGET, VOL_OFFLINE,
		 * and VOL_OUTOFSERVICE could still be set here.
		 */
		else if ((sts & (VOL_MOVE_SOURCE | VOL_MOVE_TARGET))
			 == (VOL_MOVE_SOURCE | VOL_MOVE_TARGET)) {
		    code = VOLERR_PERS_DAMAGED;
		} else if (sts & VOL_MOVE_TARGET) {
		    /* target but not source */
		    if (!(xcode & VOL_XCODE_TARGETOK)) {
			if (sts & (VOL_OUTOFSERVICE | VOL_OFFLINE))
			    code = VOLERR_PERS_OUTOFSERVICE;
			else
			    code = VOLERR_TRANS_MOVE;
		    }
		} else if ((sts & VOL_MOVE_SOURCE) && (sts & VOL_OUTOFSERVICE)) {
		    code = VOLERR_PERS_OUTOFSERVICE;
		} else if (sts == (VOL_MOVE_SOURCE | VOL_OFFLINE)) {
		    if (!(xcode & VOL_XCODE_SOURCEOK))
			code = VOLERR_PERS_DELETED;
		} else if (sts == VOL_MOVE_SOURCE) {
		    if (!(xcode & VOL_XCODE_SOURCEOK))
			code = VOLERR_PERS_OUTOFSERVICE;
		} else if (sts & (VOL_OUTOFSERVICE|VOL_OFFLINE)) {
		    code = VOLERR_PERS_DAMAGED;
		} else
		    printf("fx: Weird states value for %u,,%u: 0x%x\n",
			   AFS_HGETBOTH(volp->v_volId), volp->v_states);
	    }
	}

	/* if we succeed, bump the count, but also check for fairness
	 * considerations.
	 */
	if (code == 0) {
	    /* now, give volume ops priority, so that frequent vnode ops
	     * can't completely lock out a vol op.
	     * Don't stop recursive vnode ops, since that would deadlock.
	     */
	    if ((volp->v_states & VOL_GRABWAITING)
		&& !serverOp && !(xcode & VOL_XCODE_RECURSIVE)) {
		if (xcode & VOL_XCODE_NOWAIT) {
		    /* Exporter case: return a soft failure. */
		    code = VOLERR_TRANS_PENDING;
		} else {
		    /* Local-glue case: wait here. */
		    volp->v_states |= VOL_LOOKUPWAITING;
		    osi_SleepW(&volp->v_states, &volp->v_lock);
		    continue;
		}
	    }

	    if (code == 0) {
		/* if we succeed, bump the count of active vnode ops */
		volp->v_activeVnops++;
	    }
	}
	lock_ReleaseWrite(&volp->v_lock);
	/* If NOWAIT is clear, this is the glue calling, and it needs an 8-bit error code. */
	if (code != 0 && !(xcode & VOL_XCODE_NOWAIT)) {
	    if (code == VOLERR_PERS_DELETED)
		code = ENODEV;
	    else
		code = EIO;
	}
	return code;
    }	/* for loop */
}	/* procedure */


#ifdef SGIMIPS
/* EXPORT */ int vol_fsEndVnodeOp(
  register struct volume *volp,
  opaque *dmpp)
#else
/* EXPORT */ int vol_fsEndVnodeOp(volp, dmpp)
  register struct volume *volp;
  opaque *dmpp;
#endif
{
    if (!volp) return 0;
    lock_ObtainWrite(&volp->v_lock);
    osi_assert(volp->v_activeVnops > 0);
    if (--volp->v_activeVnops <= 0) {
	vol_RCZero(volp);
    }
    lock_ReleaseWrite(&volp->v_lock);
    return 0;
}
 
#ifdef SGIMIPS
/* EXPORT */ void vol_fsStartInactiveVnodeOp(
  IN struct volume *volp,
  IN struct vnode *vp,
  IN opaque *dmpp)
#else
/* EXPORT */ void vol_fsStartInactiveVnodeOp(volp, vp, dmpp)
  IN struct volume *volp;
  IN struct vnode *vp;
  IN opaque *dmpp;
#endif
{
    int code = 0;
    int queue;
    struct vattr vattr;
    
    lock_ObtainWrite(&volp->v_lock);
    while (1) {
	queue = 0;
	if (volp->v_states & VOL_BUSY) {
	    
	    /* Allow inactive originating in a volop to go on.
	     * It seems I do not need to be worry about
	     * VOL_XCODE_RECURSIVE as does vol_StartVnodeOp. 
	     */
	    if (volp->v_procID && volp->v_procID == osi_GetPid()) 
		break;
	    
	    icl_Trace3(xops_iclSetp, XVOL_TRACE_STARTINACTVNODEOP,
		       ICL_TYPE_POINTER, volp,
		       ICL_TYPE_POINTER, vp,
		       ICL_TYPE_LONG, (u_long) volp->v_concurrency);
	    if (volp->v_concurrency == VOL_CONCUR_NOOPS) {
		/* 
		 * Heavy weight fileset op in progress. Can't examine link
		 * count. Defer inactive.
		 */
		queue = 1;
	    } else if (volp->v_concurrency == VOL_CONCUR_READONLY) {
		volp->v_activeVnops++; /* Ensure no new volume op start */
		lock_ReleaseWrite(&volp->v_lock);
		
		/* Examine link count */
		VOPX_GETATTR(vp, ((struct xvfs_attr *)&vattr), 0, 
				osi_getucred(), code);
		lock_ObtainWrite(&volp->v_lock);
		volp->v_activeVnops--;
		if (code) {
		    /* Defer inactive in case situation improves */
		    queue = 1;
		} else if (volp->v_states & VOL_BUSY) {
		    /* volume op still in progress */
		    if (vattr.va_nlink == 0) {
			queue = 1;
		    }
		}
	    } else 
		/* Pass through on VOL_CONCUR_ALLOPS */
		osi_assert(volp->v_concurrency == VOL_CONCUR_ALLOPS);
	    
	    /* queue request */
	    if (queue) {
		vol_vp_elem_p_t vpElemP;
		
		vpElemP = vol_GetFreeVpElem();
		if (vpElemP == NULL) {
		    /* No free vp elems. Need to allocate */
		    lock_ReleaseWrite(&volp->v_lock);
		    (void) vol_AllocFreeVpElem();
		    lock_ObtainWrite(&volp->v_lock);
		    /* Need to rexamine the volume */
		    continue;           
		}
		
		icl_Trace2(xops_iclSetp, XVOL_TRACE_INACTIVE_HOLDING,
			   ICL_TYPE_POINTER, vp,
			   ICL_TYPE_POINTER, vpElemP);
		OSI_VN_HOLD(vp);
		vpElemP->vp = vp;
		vpElemP->next = volp->v_vp;
		volp->v_vp = vpElemP;
	    }
	}
	break;
    }
    volp->v_activeVnops++;
    lock_ReleaseWrite(&volp->v_lock);
    
    return;
}

/* Allow inactives pending for end of fileset operations to go through. */
/* SHARED */
#ifdef SGIMIPS
void vol_ProcessDeferredReles(
  vol_vp_elem_p_t vpList)
#else
void vol_ProcessDeferredReles(vpList)
  vol_vp_elem_p_t vpList;
#endif
{
    vol_vp_elem_p_t vpElemP;
    
    while (vpElemP = vpList) {
	icl_Trace2(xops_iclSetp, XVOL_TRACE_INACTIVE_RELEASING,
		   ICL_TYPE_POINTER, vpElemP->vp,
		   ICL_TYPE_POINTER, vpElemP);
	vpList = vpList->next;
	OSI_VN_RELE(vpElemP->vp);  /* these are non blocking */
	(void) vol_FreeVpElem(vpElemP);
    }
    return;
}

#ifdef SGIMIPS
static vol_vp_elem_p_t vol_GetFreeVpElem(void)
#else
static vol_vp_elem_p_t vol_GetFreeVpElem()
#endif
{
    vol_vp_elem_p_t elem = NULL;
    
    osi_mutex_enter(&vol_freeVpLock);
    if (vol_freeVpCount != 0) {
	elem = vol_freeVpList;
	vol_freeVpList = vol_freeVpList->next;
	vol_freeVpCount--;
    }
    osi_mutex_exit(&vol_freeVpLock);
    return elem;
}

#ifdef SGIMIPS
static void vol_AllocFreeVpElem(void)
#else
static void vol_AllocFreeVpElem()
#endif
{
    vol_vp_elem_p_t list;
    int i, n;
    
    n = VOL_NUM_ALLOC_FREE_VP;
    list = osi_Alloc(n * sizeof(*list));
    for (i = 0; i < n - 1; i++) {
	list[i].vp = NULL;
	list[i].next = &list[i + 1];
    }
    list[n - 1].vp = NULL;
    osi_mutex_enter(&vol_freeVpLock);
    list[n - 1].next = vol_freeVpList;
    vol_freeVpList = &list[0];
    vol_freeVpCount += n;
    osi_mutex_exit(&vol_freeVpLock);
}

#ifdef SGIMIPS
static void vol_FreeVpElem(
  vol_vp_elem_p_t vpElemP)
#else
static void vol_FreeVpElem(vpElemP)
  vol_vp_elem_p_t vpElemP;
#endif
{
    osi_mutex_enter(&vol_freeVpLock);
    vpElemP->vp = NULL;
    vpElemP->next = vol_freeVpList;
    vol_freeVpList = vpElemP;
    vol_freeVpCount++;
    osi_mutex_exit(&vol_freeVpLock);
}

/* Some filler routines for ops vectors */

int vol_fsHold(struct volume *volp)
{
    lock_ObtainWrite(&volp->v_lock);
    volp->v_count++;
    lock_ReleaseWrite(&volp->v_lock);
    icl_Trace2(xops_iclSetp, XVOL_TRACE_VOLHOLD,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, volp->v_count);
    return 0;
}

int vol_fsRele(struct volume *volp)
{
    lock_ObtainWrite(&volp->v_lock);
    icl_Trace2(xops_iclSetp, XVOL_TRACE_VOLRELE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, volp->v_count);
    osi_assert(volp->v_count > 0);
    if (volp->v_count == 1) {
	vol_VolInactive(volp);
    } else {
	volp->v_count--;
	lock_ReleaseWrite(&volp->v_lock);
    }
    return 0;
}

int vol_fsLock(struct volume *volp, int type)
{
    int code = 0;

    icl_Trace2(xops_iclSetp, XVOL_TRACE_ENTER_VOLLOCK,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, type);
    if (type == READ_LOCK)
	lock_ObtainRead(&volp->v_lock);
    else if (type == WRITE_LOCK)
	lock_ObtainWrite(&volp->v_lock);
    else if (type == SHARED_LOCK)
	lock_ObtainShared(&volp->v_lock);
    else
	code = EINVAL;
    icl_Trace1(xops_iclSetp, XVOL_TRACE_LEAVE_VOLLOCK,
	       ICL_TYPE_LONG, code);
    return code;
}


int vol_fsUnlock(struct volume *volp, int type)
{
    int code = 0;

    icl_Trace2(xops_iclSetp, XVOL_TRACE_ENTER_VOLUNLOCK,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, type);
    if (type == READ_LOCK)
	lock_ReleaseRead(&volp->v_lock);
    else if (type == WRITE_LOCK)
	lock_ReleaseWrite(&volp->v_lock);
    else if (type == SHARED_LOCK)
	lock_ReleaseShared(&volp->v_lock);
    else
	code = EINVAL;
    icl_Trace1(xops_iclSetp, XVOL_TRACE_LEAVE_VOLUNLOCK,
	       ICL_TYPE_LONG, code);
    return code;
}

int vol_fsDMWait(struct volume *volp, opaque *blobpp)
{
    icl_Trace2(xops_iclSetp, XVOL_TRACE_DMWAIT,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, blobpp);
    return 0;
}

int vol_fsDMFree(struct volume *volp, opaque *blobpp)
{
    icl_Trace2(xops_iclSetp, XVOL_TRACE_DMFREE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, blobpp);
    return 0;
}
