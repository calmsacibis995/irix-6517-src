/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: zlc_host.c,v 65.3 1998/03/23 16:28:19 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1995, 1992 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/zlc.h>
#include <dcedfs/zlc_trace.h>
#include <dcedfs/volreg.h>
#include <dcedfs/volume.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/zlc/RCS/zlc_host.c,v 65.3 1998/03/23 16:28:19 gwehrman Exp $")

/* SHARED */
extern void zlc_WakeupMgr(unsigned long serviceTime);
extern void zlc_GotoNextState(struct zlc_removeItem *trp);


/* declare prototypes for host functions */
static long zlc_HoldHost(struct hs_host *);
static long zlc_ReleHost(struct hs_host *);
static long zlc_LockHost(struct hs_host *, int, long);
static long zlc_UnlockHost(struct hs_host *, int);
static long zlc_RevokeToken(struct hs_host *, int, afsRevokeDesc *);
static long zlc_AsyncGrant(struct hs_host *,
			   struct hs_tokenDescription *,
			   long ourqnum);

/* define ZLC-module host structure */
struct hs_host zlc_host;

/* define host operations vector */
static struct hostops zlc_ops = 
{
    zlc_HoldHost,
    zlc_ReleHost,
    zlc_LockHost,
    zlc_UnlockHost,
    zlc_RevokeToken,
    zlc_AsyncGrant,
};


/*
 * zlc_InitHost() -- initialize zlc_host host structure.
 *
 * RETURN CODES: none
 */

/* SHARED */
void zlc_InitHost(void)
{
    bzero((char *)&zlc_host, sizeof(struct hs_host));
    lock_Init(&zlc_host.lock);
    zlc_host.hstopsp = &zlc_ops;
    return;
}


/*
 * zlc_HoldHost() -- increment host struct's reference counter
 * 
 * RETURN CODES: 0
 */

static long zlc_HoldHost(struct hs_host *hostp)
{
    lock_ObtainWrite(&hostp->lock);
    hostp->refCount++;
    lock_ReleaseWrite(&hostp->lock);
    return(0);
}


/*
 * zlc_ReleHost() -- decrement host struct's reference counter
 * 
 * RETURN CODES: 0
 */

static long zlc_ReleHost(struct hs_host *hostp)
{
    lock_ObtainWrite(&hostp->lock);
    hostp->refCount--;
    lock_ReleaseWrite(&hostp->lock);
    return(0);
}


/*
 * zlc_LockHost() -- get read (HS_LOCK_READ) or write (HS_LOCK_WRITE) lock
 *     on host struct
 * 
 * RETURN CODES: 0
 */

static long zlc_LockHost(struct hs_host *hostp,
			 int type,
			 long timeout /* unused */)
{
    if (type == HS_LOCK_READ) 
	lock_ObtainRead(&hostp->lock);
    else if (type == HS_LOCK_WRITE)
	lock_ObtainWrite(&hostp->lock);
    return(0);
}


/*
 * zlc_UnlockHost() -- release read (HS_LOCK_READ) or write (HS_LOCK_WRITE)
 *     lock held on host struct
 * 
 * RETURN CODES: 0
 */

static long zlc_UnlockHost(struct hs_host *hostp, int type)
{
    if (type == HS_LOCK_READ) 
	lock_ReleaseRead(&hostp->lock);
    else if (type == HS_LOCK_WRITE)
	lock_ReleaseWrite(&hostp->lock);
    return(0);
}


/*
 * zlc_RevokeToken() -- request that host relinquish specified tokens
 *
 *     Note: The ZLC module only requests OFD tokens for files.
 *           The module only cares that it obtains an OFD token
 *           at some point in time.  It is *not* necessary to retain
 *           the OFD across a vnode release.  The implication is that
 *           a revoke can always succeed.  Furthermore, it is not
 *           necessary to track the fact that a revoke occured.
 *           Essentially, an OFD grant is a signal.
 * 
 * RETURN CODES: always returns (0)
 */

static long zlc_RevokeToken(struct hs_host *hostp,
			    int revokeLen,
			    afsRevokeDesc *revokeListp)
{
    icl_Trace2(zlc_iclSetp, ZLC_TRACE_ENTER_REVOKETOKEN,
	       ICL_TYPE_LONG, revokeLen,
	       ICL_TYPE_FID, &revokeListp->fid);
    return(0);
}


/*
 * zlc_AsyncGrant() -- make an asynchronous OFD token grant; called by the
 *     ZLC's host-module AsyncGrant routine.
 *
 * LOCKS USED: obtains a write lock on zlc_asyncGrantLock and zlc_removeQLock;
 *     both are released on exit.
 *
 * SIDE EFFECTS: transitions relevant queue entry from ZLC_ST_QUEUED state
 *     to next logical state; if entry is found in or transitioned to the
 *     ZLC_ST_CLEANUP state, it is deallocated if not held.
 *
 * RETURN CODES: always returns (HS_ERR_JUSTKIDDING)
 */

static long zlc_AsyncGrant(struct hs_host *hostp,
			   struct hs_tokenDescription *tokenp,
			   long ourqnum)
{
    register struct zlc_removeItem *trp;
    struct vnode *vp;
    int wakeupMgr, found, moreDally;
    unsigned long serviceTime;

    icl_Trace3(zlc_iclSetp, ZLC_TRACE_ENTER_ASYNCGRANT,
	       ICL_TYPE_POINTER, hostp,
	       ICL_TYPE_POINTER, tokenp,
	       ICL_TYPE_LONG, ourqnum);

    /* scan queue searching for entry that made the OFD request */

    found     = 0;
    moreDally = 0;
    vp        = (struct vnode *)0;

    lock_ObtainWrite(&zlc_asyncGrantLock);
    lock_ObtainWrite(&zlc_removeQLock);
    lock_ReleaseWrite(&zlc_asyncGrantLock);

    for (trp = zlc_removeQ.head; trp; trp = trp->next) {
	if ((long)trp == ourqnum) {
	    /* found request that resulted in async grant; note that we
	     * must watch for a forced transition to the ZLC_ST_CLEANUP
	     * state while waiting for OFD token.
	     */
	    icl_Trace2(zlc_iclSetp, ZLC_TRACE_ASYNCGRANT_ENTRY_FOUND,
		       ICL_TYPE_FID, &trp->fid,
		       ICL_TYPE_LONG, trp->state);
	    found = 1;

	    osi_assert(trp->state == ZLC_ST_QUEUED ||
		       trp->state == ZLC_ST_CLEANUP);

	    if (trp->state == ZLC_ST_QUEUED) {
		/* determine next state from ZLC_ST_QUEUED */
		zlc_GotoNextState(trp);

		if (trp->state == ZLC_ST_CLEANUP) {
		    /* flag vnode release */
		    vp = trp->vp;
		    zlc_removeQClean++;
		} else {
		    /* flag more dally time */
		    moreDally = 1;
		}

		icl_Trace3(zlc_iclSetp, ZLC_TRACE_ASYNCGRANT_STATETRANS,
			   ICL_TYPE_FID, &trp->fid,
			   ICL_TYPE_LONG, ZLC_ST_QUEUED,
			   ICL_TYPE_LONG, trp->state);
	    }

	    /* cleanup entry if possible */
	    if ((trp->state == ZLC_ST_CLEANUP) && (!ZLC_RQ_ISHELD(trp))) {
		/* remove item from queue */
		ZLC_RQ_DEQ(trp);
		zlc_removeQSize--;
		zlc_removeQClean--;

		/* deallocate storage */
		osi_Free((opaque)trp, sizeof(*trp));
	    }
	    osi_assert(zlc_removeQClean <= zlc_removeQSize);

	    break;
	}
    }

    if (!found) {
	/* request not located; was forced to ZLC_ST_CLEANUP and deallocated */
	icl_Trace0(zlc_iclSetp, ZLC_TRACE_ASYNCGRANT_NOTFOUND);
    }

    wakeupMgr = 0;

    if (zlc_WakeMgrToPruneQ()) {
	wakeupMgr   = 1;
	serviceTime = 0;
    } else if (moreDally) {
	wakeupMgr   = 1;
	serviceTime = trp->timeTry;
    }

    lock_ReleaseWrite(&zlc_removeQLock);

    if (wakeupMgr) {
	/* wake mgr thread to schedule service or prune remove queue */
	zlc_WakeupMgr(serviceTime);
    }

    if (vp) {
	/* release vnode */
	tkc_FlushVnode(vp);
	OSI_VN_RELE(vp);
    }

    /* Work is done, so don't need to hold OFD token.  Note that tkm can't be
     * called recursively, so refuse token rather than call tkm_ReturnToken().
     */
    icl_Trace0(zlc_iclSetp, ZLC_TRACE_EXIT_ASYNCGRANT);
    return(HS_ERR_JUSTKIDDING);
}
