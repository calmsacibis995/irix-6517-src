/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: zlc_remove.c,v 65.3 1998/03/23 16:28:18 gwehrman Exp $";
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

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/volume.h>
#include <dcedfs/tkm_tokens.h>
#include <dcedfs/volreg.h>
#include <dcedfs/zlc.h>
#include <dcedfs/zlc_trace.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/zlc/RCS/zlc_remove.c,v 65.3 1998/03/23 16:28:18 gwehrman Exp $")

/* SHARED */
extern void zlc_GetDeleteToken(struct zlc_removeItem *trp, int itemIsQueued);
extern void zlc_WakeupMgr(unsigned long serviceTime);
extern void zlc_GotoNextState(struct zlc_removeItem *trp);




/*
 * zlc_TryRemove() -- requests ZLC module to hold specified vnode until
 *     conditions are such that an OFD token can be requested and subsequently
 *     granted, at which time the ZLC module performs the necessary token
 *     cache flushes and releases the vnode.
 *
 * LOCKS USED: may obtain a read lock on zlc_asyncGrantLock and/or a write
 *     lock on zlc_removeQLock; both released on exit.
 *
 * ASSUMPTIONS: *avp held prior to entry and released following exit.
 *
 * RETURN CODES: none
 */

/* EXPORT */
void zlc_TryRemove(struct vnode *avp, afsFid *afidp)
{
    register struct zlc_removeItem *trp;
    int wakeupMgr;
    unsigned long serviceTime;

    if (!zlc_initted) {
	(void)zlc_Init();
    }

    if (!avp) {
	/* no-op */
	return;
    } else {
	/* hold vnode; will be released at the appropriate time */
	OSI_VN_HOLD(avp);
    }

    icl_Trace2(zlc_iclSetp, ZLC_TRACE_ENTER_TRYREMOVE_2,
	       ICL_TYPE_POINTER, avp,
	       ICL_TYPE_FID, afidp);

    /* Allocate a remove queue item and initialize in start state */
    trp = (struct zlc_removeItem *)osi_Alloc(sizeof(*trp));

    trp->state    = ZLC_ST_START;
    trp->fid      = *afidp;
    trp->vp       = avp;
    trp->refCount = 0;

    /* Determine initial (non-start) state for queue item */
    zlc_GotoNextState(trp);

    icl_Trace2(zlc_iclSetp, ZLC_TRACE_TRYREMOVE_INITSTATE,
	       ICL_TYPE_FID, &trp->fid,
	       ICL_TYPE_LONG, trp->state);

    wakeupMgr = 0;

    if (trp->state == ZLC_ST_CLEANUP) {
	/* release vnode immediately and deallocate item */
	tkc_FlushVnode(avp);
	OSI_VN_RELE(avp);
	osi_Free((opaque)trp, sizeof(*trp));

    } else if (trp->state == ZLC_ST_OFDREQUEST) {
	/* request OFD immediately */
	lock_ObtainRead(&zlc_asyncGrantLock);

	zlc_GetDeleteToken(trp, 0 /* item NOT on remove queue */);

	if (trp->state == ZLC_ST_CLEANUP) {
	    /* deallocate item */
	    osi_Free((opaque)trp, sizeof(*trp));
	} else {
	    /* put new item on remove queue to await async grant */
	    lock_ObtainWrite(&zlc_removeQLock);

	    ZLC_RQ_ENQ(trp);
	    zlc_removeQSize++;

	    wakeupMgr   = zlc_WakeMgrToPruneQ();
	    serviceTime = 0;

	    lock_ReleaseWrite(&zlc_removeQLock);
	}

	lock_ReleaseRead(&zlc_asyncGrantLock);

    } else {
	/* item is in dally state; put on remove queue for later release */
	lock_ObtainWrite(&zlc_removeQLock);

	ZLC_RQ_ENQ(trp);
	zlc_removeQSize++;

	wakeupMgr   = 1;
	serviceTime = (zlc_WakeMgrToPruneQ() ? 0 : trp->timeTry);

	lock_ReleaseWrite(&zlc_removeQLock);
    }

    if (wakeupMgr) {
	/* wake mgr thread to schedule service or prune remove queue */
	zlc_WakeupMgr(serviceTime);
    }

    icl_Trace0(zlc_iclSetp, ZLC_TRACE_EXIT_TRYREMOVE);
    return;
}


/*
 * zlc_GetDeleteToken() -- attempt to get an OFD token for the specified file;
 *     if successful, the associated vnode is released and the token returned.
 * 
 * LOCKS USED: if itemIsQueued then obtains a write lock on zlc_removeQLock;
 *     released on exit.
 *
 * ASSUMPTIONS: zlc_asyncGrantLock is read-locked across call to prevent races
 *     with zlc_AsyncGrant(), and *trp is held by caller if itemIsQueued.
 *
 * SIDE EFFECTS: transitions *trp from ZLC_ST_OFDREQUEST state to one
 *     of ZLC_ST_QUEUED or ZLC_ST_CLEANUP state.
 *
 * CAUTIONS: because the zlc_removeQLock is not held across this call, *trp
 *     can be transitioned again before this call returns if itemIsQueued.
 *
 * RETURN CODES: none
 */

/* SHARED */
void zlc_GetDeleteToken(struct zlc_removeItem *trp, int itemIsQueued)
{
    long code;
    afs_token_t token;
    struct vnode *vp;
    int fromState, ofdGranted;

    icl_Trace1(zlc_iclSetp, ZLC_TRACE_ENTER_GETDELETETOKEN_2,
	       ICL_TYPE_FID, &trp->fid);

    /* Make OFD request for specified file; we can be sleazy and read fid w/o
     * locking zlc_removeQLock when itemIsQueued since value never changes.
     * Note that zlc_asyncGrantLock is read-locked by caller to prevent
     * possible races with zlc_AsyncGrant().
     */

    bzero((char *)&token, sizeof(token));
    AFS_hset64(token.type, 0, TKN_OPEN_DELETE);
    token.endRange = osi_hMaxint64;

    code = tkm_GetToken(&trp->fid,
			TKM_FLAGS_QUEUEREQUEST,
			&token,
			&zlc_host,
			(long)trp,
			(afs_recordLock_t *)NULL);

    /* If OFD granted then release the vnode (if necessary) and return token.
     * For a queued item, watch for a forced transition to the ZLC_ST_CLEANUP
     * state while remove queue lock was not held.
     */
    vp = (struct vnode *)0;
    ofdGranted = 0;

    if (itemIsQueued) lock_ObtainWrite(&zlc_removeQLock);

    osi_assert(trp->state == ZLC_ST_OFDREQUEST ||
	       trp->state == ZLC_ST_CLEANUP);

    fromState = trp->state;

    if (code == TKM_SUCCESS) {
	/* Got the OFD token */
	icl_Trace1(zlc_iclSetp, ZLC_TRACE_GETDELETETOKEN_OFD_GRANTED,
		   ICL_TYPE_FID, &trp->fid);

	ofdGranted = 1;

	if (trp->state == ZLC_ST_OFDREQUEST) {
	    vp = trp->vp;
	    trp->state = ZLC_ST_CLEANUP;

	    if (itemIsQueued) {
		zlc_removeQClean++;
	    }
	}
    }  else if (code == TKM_ERROR_REQUESTQUEUED) {
	/* OFD request has been queued */
	icl_Trace1(zlc_iclSetp, ZLC_TRACE_GETDELETETOKEN_OFD_QUEUED,
		   ICL_TYPE_FID, &trp->fid);

	if (trp->state == ZLC_ST_OFDREQUEST) {
	    trp->state = ZLC_ST_QUEUED;
	}
    } else {
	/* Unexpected error from tkm; probably fileset no longer attached */
	icl_Trace2(zlc_iclSetp, ZLC_TRACE_GETDELETETOKEN_TKMGETERR,
		   ICL_TYPE_FID, &trp->fid,
		   ICL_TYPE_LONG, code);

	if (trp->state == ZLC_ST_OFDREQUEST) {
	    vp = trp->vp;
	    trp->state = ZLC_ST_CLEANUP;

	    if (itemIsQueued) {
		zlc_removeQClean++;
	    }
	}
    }

    icl_Trace3(zlc_iclSetp, ZLC_TRACE_GETDELETETOKEN_STATETRANS,
	       ICL_TYPE_FID, &trp->fid,
	       ICL_TYPE_LONG, fromState,
	       ICL_TYPE_LONG, trp->state);

    osi_assert(zlc_removeQClean <= zlc_removeQSize);

    if (itemIsQueued) lock_ReleaseWrite(&zlc_removeQLock);

    if (vp) {
	/* release the vnode */
	tkc_FlushVnode(vp);
	OSI_VN_RELE(vp);
    }

    if (ofdGranted) {
	/* Return OFD token; once again read fid w/o lock when itemIsQueued.
	 * Note that returning token can result in an error if a revoke
	 * has occured (see zlc_host.c); this can be safely ignored.
	 */
	code = tkm_ReturnToken(&trp->fid,
			       (afs_hyper_t *)&token.tokenID,
			       (afs_hyper_t *)&token.type,
			       (long)0);
	if (code) {
	    icl_Trace2(zlc_iclSetp, ZLC_TRACE_GETDELETETOKEN_TKMRETURNERR,
		       ICL_TYPE_FID, &trp->fid,
		       ICL_TYPE_LONG, code);
	}
    }

    icl_Trace0(zlc_iclSetp, ZLC_TRACE_EXIT_GETDELETETOKEN);
    return;
}


/*
 * zlc_CleanVolume() -- releases all the vnodes for a particular volume that
 *     are held by the ZLC module.
 *
 * LOCKS USED: obtains a write lock on zlc_removeQLock; released on exit.
 *
 * SIDE EFFECTS: transitions volume's queue entries to the ZLC_ST_CLEANUP
 *     state, and then deallocates them if not held.
 *
 * RETURN CODES: none
 */

/* EXPORT */
void zlc_CleanVolume(afs_hyper_t *volIDp)
{
    register struct zlc_removeItem *trp, *nrp;
    struct vnode *vp;
    int wakeupMgr;

    if (!zlc_initted) {
	(void)zlc_Init();
    }

    icl_Trace1(zlc_iclSetp, ZLC_TRACE_ENTER_CLEANVOLUME,
	       ICL_TYPE_HYPER, volIDp);

    /* scan queue searching for items related to the specified volume */

    lock_ObtainWrite(&zlc_removeQLock);

    for (trp = zlc_removeQ.head; trp; trp = nrp) {
	nrp = trp->next;

	/* check for volume ID match */
	if (AFS_hsame((*volIDp), (trp->fid.Volume))) {
	    /* relevant volume; release vnode if necessary */
	    if (trp->state != ZLC_ST_CLEANUP) {
		icl_Trace3(zlc_iclSetp, ZLC_TRACE_CLEANVOLUME_STATETRANS,
			   ICL_TYPE_FID, &trp->fid,
			   ICL_TYPE_LONG, trp->state,
			   ICL_TYPE_LONG, ZLC_ST_CLEANUP);

		vp = trp->vp;
		trp->state = ZLC_ST_CLEANUP;
		zlc_removeQClean++;

		if (nrp) ZLC_RQ_HOLD(nrp);
		ZLC_RQ_HOLD(trp);
		lock_ReleaseWrite(&zlc_removeQLock);

		tkc_FlushVnode(vp);
		OSI_VN_RELE(vp);

		lock_ObtainWrite(&zlc_removeQLock);
		ZLC_RQ_RELE(trp);
		if (nrp) ZLC_RQ_RELE(nrp);

		osi_assert(trp->state == ZLC_ST_CLEANUP);
	    }

	    /* cleanup entry if possible */
	    if (!ZLC_RQ_ISHELD(trp)) {
		/* remove item from queue */
		ZLC_RQ_DEQ(trp);
		zlc_removeQSize--;
		zlc_removeQClean--;

		/* deallocate storage */
		osi_Free((opaque)trp, sizeof(*trp));
	    }
	}
    }

    osi_assert(zlc_removeQClean <= zlc_removeQSize);

    wakeupMgr = zlc_WakeMgrToPruneQ();

    lock_ReleaseWrite(&zlc_removeQLock);

    if (wakeupMgr) {
	/* wake mgr thread to prune remove queue */
	zlc_WakeupMgr(0);
    }

    icl_Trace0(zlc_iclSetp, ZLC_TRACE_EXIT_CLEANVOLUME);
    return;
}
