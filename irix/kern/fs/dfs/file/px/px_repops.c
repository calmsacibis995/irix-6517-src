/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: px_repops.c,v 65.5 1998/04/01 14:16:10 gwehrman Exp $";
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
 * $Log: px_repops.c,v $
 * Revision 65.5  1998/04/01 14:16:10  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed functions declarations to ansi style.  Changed arguments
 * 	of pxint_BulkKeepAlive() from unsigned long to unsigned32.  Added
 * 	type casting for arguments in calls to some_sort().  Added
 * 	missing return values.
 *
 * Revision 65.4  1998/03/23  16:24:46  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/12/16 17:05:40  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.2  1997/11/06  19:57:56  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:30  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.852.1  1996/10/02  18:13:06  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:39  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/fshs.h>
#include <dcedfs/aggr.h>
#include <dcedfs/volume.h>
#include <dcedfs/xcred.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/tkset.h>
#include <dcedfs/volreg.h>
#include <px.h>

RCSID ("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/px/RCS/px_repops.c,v 65.5 1998/04/01 14:16:10 gwehrman Exp $")

#ifdef KERNEL
/* 
 * Obviously not a quicker-sort, but it's some kind of sort function. 
 */
#define	some_sort bsort
static void bsort(char *base, int nel, int wid, int (*compar)(void *, void *))
{/* Bubble sort (sigh). */
    register int chg, ix, i2;
    register char *ptr, tc;

    for (chg = 1; chg != 0;) {
	chg = 0;
	for (ix = 0, ptr = base; ix < nel-1; ++ix, ptr += wid) {
	    if ((*compar)(ptr, ptr+wid) > 0) {
		chg = 1;
		for (i2 = 0; i2 < wid; ++i2) {	/* swap 'em */
		    tc = ptr[i2];
		    ptr[i2] = ptr[i2+wid];
		    ptr[i2+wid] = tc;
		}
	    }
	}
    }
}
#else /* KERNEL */
#define	some_sort qsort
#endif /* KERNEL */

#define	BASIC_RHYTHM	  120	/* how often we get called */
#define	BASIC_RHYTHM_HORIZON	(2*BASIC_RHYTHM + 20) /* how far ahead to look */

struct KeptAlive {
    afsFid fid;
    u_long reclaimTime;
    u_long flags;
    tkm_tokenID_t tokID;
    tkm_tokenID_t oldtokID;
    u_long tokExp;
};
#define	kafl_WantExecute	(0x1)
#define	kafl_HaveExecute	(0x2)
#define	kafl_Revoked	(0x4)
#define	kafl_NoID	(0x8)

static int pxka_inited = 0;
static struct lock_data pxka_lock;
static struct KeptAlive *pxKA;
static long maxKA, numKA;
static afs_hyper_t openPreserve, openExecute, openAll;
/*
 * Here's the statically-allocated host object for keep-alive operations.
 */
struct hs_host px_keepAliveHost;

#ifdef SGIMIPS
static int cmpFids(
afsFid *fid1p,
afsFid *fid2p)
#else
static int cmpFids(fid1p, fid2p)
afsFid *fid1p, *fid2p;
#endif
{
    register int res;

    /* Ignore the Cell part of the fid, since the PX's cells are always 0,,1
	and the remote ones are always real. */
    res = AFS_hcmp(fid1p->Volume, fid2p->Volume);
    if (res == 0) if (fid1p->Vnode < fid2p->Vnode) res = -1;
    else if (fid1p->Vnode > fid2p->Vnode) res = 1;
    else if (fid1p->Unique < fid2p->Unique) res = -1;
    else if (fid1p->Unique > fid2p->Unique) res = 1;
    else res = 0;
    return res;
}

#ifdef SGIMIPS
static void runTokens(void)
#else
static void runTokens()
#endif
{
    register int Mid;
    int kalo, kahi;
    register struct KeptAlive *kap, *kapd;
    u_long now, future, newExp, refTime, flagBuf, tkmFlagBuf;
    long code, origNumKA;
    afs_token_t Tok;
    afsFid fidBuf, fidBuf2;
    tkm_tokenID_t idBuf;
    int z, WLock, exact, AnyToDelete;
    enum {GetTok, ReturnTok} Action;
    /* Here's the idea behind this procedure.
      We don't want to make TKM calls with local locks held, so we loop:
      we find something to do in our local list, copy pertinent data, release locks,
      make the TKM call,then find where we were and record the changes. */

    icl_Trace0(px_iclSetp, PX_TRACE_RUNTOKENS);
    AnyToDelete = 0;
    lock_ObtainShared(&pxka_lock);
    WLock = 0;
    for (Mid = 0; Mid < numKA;) {
	now = osi_Time();	/* after possible delay */
	future = now + BASIC_RHYTHM_HORIZON;
	for (kap = &pxKA[Mid]; Mid < numKA; ++Mid, ++kap) {
	    if (kap->reclaimTime < now) {
		/* Expired request.  Return the token, but only if we still have it. */
		if (kap->tokExp != 0
		    && kap->tokExp >= now
		    && !AFS_hiszero(kap->tokID)) {
		    fidBuf = kap->fid;
		    idBuf = kap->tokID;
		    Action = ReturnTok;
		    break;
		}
	    } else {				/* if promise is still in effect */
		if (kap->tokExp != 0) {	/* if tokens weren't expired */
		    if (WLock == 0) {
			lock_UpgradeSToW(&pxka_lock);
			WLock = 1;
			now = osi_Time();
			future = now + BASIC_RHYTHM_HORIZON;
		    }
		    if (now >= kap->tokExp) {
			AFS_hzero(kap->tokID);	/* they are now. */
			AFS_hzero(kap->oldtokID);
			kap->tokExp = 0;
		    }
		}
		if (!AFS_hiszero(kap->oldtokID) && kap->tokExp != 0) {
		    fidBuf = kap->fid;
		    idBuf = kap->oldtokID;
		    Action = ReturnTok;
		    break;
		}
		if (kap->tokExp == 0	/* need to get this one? */
		    || (kap->flags & (kafl_WantExecute | kafl_HaveExecute)) == kafl_WantExecute
		    || (kap->tokExp < future)) {	/* need to renew this one? */
		    bzero((caddr_t)&Tok, sizeof(Tok));
		    if (kap->flags & kafl_WantExecute) {
			Tok.type = openExecute;
		    } else {
			Tok.type = openPreserve;
		    }
		    /* We have the kap->tokID in some cases. */
		    fidBuf = kap->fid;
		    tkmFlagBuf = TKM_FLAGS_JUMPQUEUEDTOKENS;
		    /* Maybe ask for the same ID also. */
		    if (kap->tokExp != 0
			&& !AFS_hiszero(kap->tokID)
			&& (kap->flags & kafl_NoID) == 0) {
			tkmFlagBuf |= TKM_FLAGS_GETSAMEID;
			Tok.tokenID = kap->tokID;
		    }
		    Action = GetTok;
		    break;
		}
	    }
	}
	if (Mid >= numKA) break;	/* no further external action to take */
	kap->flags &= ~kafl_Revoked;
	flagBuf = kap->flags;
	if (WLock) lock_ReleaseWrite(&pxka_lock);
	else lock_ReleaseShared(&pxka_lock);
	/* Do external calls without a local lock. */
	icl_Trace4(px_iclSetp, PX_TRACE_RUNTKN, ICL_TYPE_LONG, Action,
		   ICL_TYPE_LONG, Mid, ICL_TYPE_FID, &fidBuf,
		   ICL_TYPE_LONG, flagBuf);
	switch (Action) {
	    case GetTok:
		fidBuf2 = fidBuf;
		px_AdjustCell(&fidBuf2);
		code = tkm_GetToken(&fidBuf2, tkmFlagBuf, (afs_token_t *) &Tok,
				    &px_keepAliveHost, 0,
				    (afs_recordLock_t *)NULL);
		break;
	    case ReturnTok:
		fidBuf2 = fidBuf;
		px_AdjustCell(&fidBuf2);
		code = tkm_ReturnToken(&fidBuf2, &idBuf, &openAll, 0);
		Tok.expirationTime = now;
		break;
	}
	icl_Trace2(px_iclSetp, PX_TRACE_RUNTKNEND, ICL_TYPE_LONG, code,
		   ICL_TYPE_LONG, Tok.expirationTime - now);
	/* get a shared lock */
	WLock = 0;
	lock_ObtainShared(&pxka_lock);
	/* Now, under the lock, do binary-search to find where we were. */
	/* But be optimistic, first, and check if it didn't move. */
	if (Mid < numKA && cmpFids(&pxKA[Mid].fid, &fidBuf) == 0) {
	    exact = 1;
	} else {
	    kalo = 0;
	    kahi = numKA - 1;
	    exact = 0;
	    while (kalo <= kahi) {
		Mid = (kalo + kahi) / 2;
		z = cmpFids(&pxKA[Mid].fid, &fidBuf);
		if (z < 0) {
		    kalo = Mid+1;
		} else if (z > 0) {
		    kahi = Mid-1;
		} else {
		    exact = 1;
		    break;
		}
	    }
	    if (exact == 0) Mid = kalo;
	    if (Mid >= numKA) break;
	}
	if (exact != 0) {
	    icl_Trace1(px_iclSetp, PX_TRACE_RUNTKNAPPLY, ICL_TYPE_LONG, Mid);
	    kap = &pxKA[Mid];
	    /* Just try again if there was a revocation race. */
	    if ((kap->flags & kafl_Revoked) == 0) {
		++Mid;		/* else, by default, move on to next item */
		/* Record the result of the token manager call. */
		WLock = 1;
		lock_UpgradeSToW(&pxka_lock);
		switch (Action) {
		    case GetTok:
			if (code == TKM_SUCCESS) {
			    /* check what we asked for */
			    if (AFS_hgetlo(Tok.type) & AFS_hgetlo(openExecute))
				kap->flags |= kafl_HaveExecute;
			    if (!AFS_hsame(kap->tokID, Tok.tokenID)) {
				kap->oldtokID = kap->tokID;
				kap->tokID = Tok.tokenID;
			    }
			    kap->tokExp = Tok.expirationTime;
			    if (kap->tokExp == 0)
				kap->tokExp = 1;
			    kap->flags &= ~kafl_NoID;
			    if (!AFS_hiszero(kap->oldtokID)) --Mid;
			} else if (code == TKM_ERROR_INVALIDID) {
			    kap->flags |= kafl_NoID;
			    --Mid;	/* loop back to try for any token ID */
			} else if (code != TKM_ERROR_TRYAGAIN
				   && code != TKM_ERROR_NOMEM) {
			    /* Looks like some sort of horrible error. */
			    kap->reclaimTime = now-1; 	/* (delete the request) */
			    AnyToDelete = 1;
			    kap->tokExp = 0;
			    icl_Trace3(px_iclSetp, PX_TRACE_RUNTKN_GETERR,
				       ICL_TYPE_LONG, code,
				       ICL_TYPE_FID, &kap->fid,
				       ICL_TYPE_LONG, Mid);
			}
			break;
		    case ReturnTok:
			if (code != TKM_SUCCESS) {
			    icl_Trace1(px_iclSetp, PX_TRACE_RUNTKNRETFAIL,
				       ICL_TYPE_LONG, code);
			}
			/* retrying won't help!  Just give it up. */
			AFS_hzero(kap->oldtokID);
			if (AFS_hsame(idBuf, kap->tokID)) {
			    /* returning the last token for this request */
			    AFS_hzero(kap->tokID);
			}
			kap->flags &= ~kafl_NoID;
			/* See if this entry should be reclaimed. */
			if (kap->reclaimTime < now) {
			    AnyToDelete = 1;
			    kap->tokExp = 0;
			} else {
			    --Mid;	/* see if there's more to do on this one */
			}
			break;
		}
	    }
	}
    }
    icl_Trace1(px_iclSetp, PX_TRACE_RUNTKNDONE, ICL_TYPE_LONG, numKA);
    if (WLock) lock_ReleaseWrite(&pxka_lock);
    else lock_ReleaseShared(&pxka_lock);
    if (AnyToDelete != 0) {
	icl_Trace0(px_iclSetp, PX_TRACE_RUNTKNDEL);
	lock_ObtainWrite(&pxka_lock);
	now = osi_Time();
	origNumKA = numKA;
	for (Mid = 0, kapd = kap = pxKA; Mid < origNumKA; ++Mid, ++kap) {
	    if (kap->reclaimTime >= now) {
		if (kap != kapd)
		    *kapd = *kap;
		++kapd;
	    } else {
		--numKA;
	    }
	}
	lock_ReleaseWrite(&pxka_lock);
    }
}

#ifdef SGIMIPS
static int cmpFexs(
struct afsFidExp *fex1p,
struct afsFidExp *fex2p)
#else
static int cmpFexs(fex1p, fex2p)
struct afsFidExp *fex1p, *fex2p;
#endif
{
    register unsigned long *lp1, *lp2;
    register int count;

    lp1 = (unsigned long *) (&fex1p->fid);
    lp2 = (unsigned long *) (&fex2p->fid);
    for (count = (sizeof(afsFid) / sizeof(unsigned long)); count > 0; --count) {
	if (*lp1 < *lp2) return -1;
	else if (*lp1 > *lp2) return 1;
	++lp1; ++lp2;
    }
    return 0;
}

#ifdef SGIMIPS
static int cmpKAs(
struct KeptAlive *ka1p,
struct KeptAlive *ka2p)
#else
static int cmpKAs(ka1p, ka2p)
struct KeptAlive *ka1p, *ka2p;
#endif
{
    register unsigned long *lp1, *lp2;
    register int count;

    lp1 = (unsigned long *) ka1p;
    lp2 = (unsigned long *) ka2p;
    for (count = (sizeof(struct KeptAlive) / sizeof(unsigned long)); count > 0; --count) {
	if (*lp1 < *lp2) return -1;
	else if (*lp1 > *lp2) return 1;
	++lp1; ++lp2;
    }
    return 0;
}

/* 
 * Here are the main work for some of the replication-related RPC calls. 
 */

/* 
 * Handle the bulk-keep-alive call.  This is the exported RPC call. 
 */
#ifdef SGIMIPS
pxint_BulkKeepAlive(
    handle_t h,
    afsBulkFEX *KAFEXp,	            /* Fids of files to be kept alive */
    unsigned32 numExecFids,	    /* num of Fids for open-for-execute */
    unsigned32 Flags,	    /* future expansion */
    unsigned32 spare1,
    unsigned32 spare2,
    unsigned32 *spare4)
#else
pxint_BulkKeepAlive(h, KAFEXp, numExecFids, Flags, spare1, spare2, spare4)
    handle_t h;
    afsBulkFEX *KAFEXp;	            /* Fids of files to be kept alive */
    unsigned long numExecFids;	    /* num of Fids for open-for-execute */
    unsigned long Flags;	    /* future expansion */
    unsigned long spare1;
    unsigned long spare2;
    unsigned long *spare4;
#endif
{
    register struct afsFidExp *fexp;
    register struct KeptAlive *kap, *kapd;
    register long fCtr, tCtr;
    unsigned long Now, tempTime, oldNumKA;
    int z;

    icl_Trace0(px_iclSetp, PX_TRACE_BKASTART);
    osi_assert(pxka_inited);
    if (numExecFids > KAFEXp->afsBulkFEX_len) return EINVAL;
    *spare4 = 0;

    /* Sort the exec and non-exec portions */
#ifdef SGIMIPS
    some_sort((char *) &KAFEXp->afsBulkFEX_val[0],
	       numExecFids,
	       sizeof(struct afsFidExp), (int (*)(void *, void *))cmpFexs);
    some_sort((char *) &KAFEXp->afsBulkFEX_val[numExecFids],
	       KAFEXp->afsBulkFEX_len - numExecFids,
	       sizeof(struct afsFidExp), (int (*)(void *, void *))cmpFexs);
#else
    some_sort((char *) &KAFEXp->afsBulkFEX_val[0],
	       numExecFids,
	       sizeof(struct afsFidExp), cmpFexs);
    some_sort((char *) &KAFEXp->afsBulkFEX_val[numExecFids],
	       KAFEXp->afsBulkFEX_len - numExecFids,
	       sizeof(struct afsFidExp), cmpFexs);
#endif
#ifdef notdef
    /* 
     * no longer need to do this since we don't need to worry about 
     * updating VOL_KNOWDALLY.  We are using the VOL_IS_REPLICATED bit 
     * to see if the fileset is repliacted.
     */
    /* If this is called from a real RPC, the caller may prompt an FLDB re-check. */
    if (h != 0) {
	register long code;
	struct volume *volp;
	register struct afsFidExp *fexp2;

	/* Called from a real RPC */
	fexp = &KAFEXp->afsBulkFEX_val[0];
	fexp2 = 0;
	fCtr = KAFEXp->afsBulkFEX_len;
	for (; fCtr > 0; --fCtr, ++fexp) {
	    /* Cycle through inputs, testing unique volume structures */
	    if (fexp2 == 0 || !AFS_hsame(fexp->fid.Volume, fexp2->fid.Volume)
		|| !AFS_hsame(fexp->fid.Cell, fexp2->fid.Cell)) {
		fexp2 = fexp;	/* we will have checked this one. */
		code = volreg_Lookup(&fexp->fid, &volp);
		if (code == 0) {
		    if (VOL_EXPORTED(volp)) {
			/* See if the dally value is reasonable. */
			lock_ObtainWrite(&volp->v_lock);
			if ((volp->v_reclaimDally == 0
			     || fexp->keepAliveTime == AFS_KA_TIME_RECHECK)
			    && (volp->v_states & VOL_KNOWDALLY)) {
			    /* No.  Ensure that the PX checks this on next reference. */
			    volp->v_states &= ~VOL_KNOWDALLY;
			}
			lock_ReleaseWrite(&volp->v_lock);
		    }
		    VOL_RELE(volp);
		}
	    }
	}
    }
#endif /* notdef */
    lock_ObtainWrite(&pxka_lock);
    if ((maxKA - numKA) < KAFEXp->afsBulkFEX_len) {	/* must grow storage */
	struct KeptAlive *newKA;
	long newMaxKA;

	newMaxKA = ((maxKA + KAFEXp->afsBulkFEX_len) * 3) / 2;
	icl_Trace2(px_iclSetp, PX_TRACE_BKAGROW, ICL_TYPE_LONG, maxKA,
		   ICL_TYPE_LONG, newMaxKA);
	newKA = (struct KeptAlive *) osi_Alloc(newMaxKA * sizeof(struct KeptAlive));
	if (pxKA != NULL) {
	    bcopy((caddr_t)pxKA, (caddr_t)newKA, numKA * sizeof(struct KeptAlive));
	    osi_Free((opaque) pxKA, maxKA * sizeof(struct KeptAlive));
	}
	pxKA = newKA;
	maxKA = newMaxKA;
    }
    Now = osi_Time();
    oldNumKA = numKA;
    kapd = &pxKA[numKA];	/* where we can stick new ones */
    fCtr = numExecFids;	/* first the exec-mode FIDs */
    tCtr = numKA;
    fexp = &KAFEXp->afsBulkFEX_val[0];
    kap = pxKA;
    while (fCtr > 0 && tCtr > 0) {
	z = cmpFids(&kap->fid, &fexp->fid);
	if (z < 0) {	/* existing table fid first */
	    ++kap; --tCtr;
	} else if (z > 0) {	/* input fid too low */
	    bzero((caddr_t)kapd, sizeof(struct KeptAlive));
	    kapd->fid = fexp->fid;
	    kapd->reclaimTime = Now + fexp->keepAliveTime;
	    kapd->flags |= kafl_WantExecute;
	    icl_Trace3(px_iclSetp, PX_TRACE_BKANEWFID,
		       ICL_TYPE_FID, &kapd->fid,
		       ICL_TYPE_LONG, 1,
		       ICL_TYPE_LONG, kapd->reclaimTime);
	    ++kapd; ++numKA;
	    ++fexp; --fCtr;
	} else {		/* the same fid */
	    tempTime = Now + fexp->keepAliveTime;
	    if (kap->reclaimTime < tempTime) {
		kap->reclaimTime = tempTime;
		icl_Trace3(px_iclSetp, PX_TRACE_BKAEXTENDFID,
			   ICL_TYPE_FID, &kapd->fid,
			   ICL_TYPE_LONG, 1,
			   ICL_TYPE_LONG, kapd->reclaimTime);
	    }
	    kap->flags |= kafl_WantExecute;
	    ++kap; --tCtr;
	    ++fexp; --fCtr;
	}
    }
    while (fCtr > 0) {
	bzero((caddr_t)kapd, sizeof(struct KeptAlive));
	kapd->fid = fexp->fid;
	kapd->reclaimTime = Now + fexp->keepAliveTime;
	kapd->flags |= kafl_WantExecute;
	icl_Trace3(px_iclSetp, PX_TRACE_BKANEWFID,
		   ICL_TYPE_FID, &kapd->fid,
		   ICL_TYPE_LONG, 3,
		   ICL_TYPE_LONG, kapd->reclaimTime);
	++kapd; ++numKA;
	++fexp; --fCtr;
    }
    fCtr = KAFEXp->afsBulkFEX_len - numExecFids;	/* next the read-mode FIDs */
    tCtr = numKA;
    icl_Trace2(px_iclSetp, PX_TRACE_BKAER, ICL_TYPE_LONG, numExecFids,
	      ICL_TYPE_LONG, fCtr);
    fexp = &KAFEXp->afsBulkFEX_val[numExecFids];
    kap = pxKA;
    while (fCtr > 0 && tCtr > 0) {
	z = cmpFids(&kap->fid, &fexp->fid);
	if (z < 0) {	/* existing table fid first */
	    ++kap; --tCtr;
	} else if (z > 0) {	/* input fid too low */
	    bzero((caddr_t)kapd, sizeof(struct KeptAlive));
	    kapd->fid = fexp->fid;
	    kapd->reclaimTime = Now + fexp->keepAliveTime;
	    icl_Trace3(px_iclSetp, PX_TRACE_BKANEWFID,
		       ICL_TYPE_FID, &kapd->fid,
		       ICL_TYPE_LONG, 0,
		       ICL_TYPE_LONG, kapd->reclaimTime);
	    ++kapd; ++numKA;
	    ++fexp; --fCtr;
	} else {		/* the same fid */
	    tempTime = Now + fexp->keepAliveTime;
	    if (kap->reclaimTime < tempTime) {
		kap->reclaimTime = tempTime;
		icl_Trace3(px_iclSetp, PX_TRACE_BKAEXTENDFID,
			   ICL_TYPE_FID, &kapd->fid,
			   ICL_TYPE_LONG, 0,
			   ICL_TYPE_LONG, kapd->reclaimTime);
	    }
	    ++kap; --tCtr;
	    ++fexp; --fCtr;
	}
    }
    while (fCtr > 0) {
	bzero((caddr_t)kapd, sizeof(struct KeptAlive));
	kapd->fid = fexp->fid;
	kapd->reclaimTime = Now + fexp->keepAliveTime;
	icl_Trace3(px_iclSetp, PX_TRACE_BKANEWFID,
		   ICL_TYPE_FID, &kapd->fid,
		   ICL_TYPE_LONG, 2,
		   ICL_TYPE_LONG, kapd->reclaimTime);
	++kapd; ++numKA;
	++fexp; --fCtr;
    }
    if (numKA != oldNumKA) {	/* need to re-sort? */
#ifdef SGIMIPS
	some_sort((char *) pxKA, numKA, sizeof(struct KeptAlive),
		  (int (*)(void *, void*))cmpKAs);
#else
	some_sort((char *) pxKA, numKA, sizeof(struct KeptAlive), cmpKAs);
#endif
    }
    lock_ReleaseWrite(&pxka_lock);
    runTokens();
    return 0;
}

/* Called from the host-check-daemon process periodically.  Return seconds until next call. */
#ifdef SGIMIPS
u_long pxint_PeriodicKA(void)
#else
u_long pxint_PeriodicKA()
#endif
{
    icl_Trace0(px_iclSetp, PX_TRACE_PKA);
    osi_assert(pxka_inited);
    runTokens();
    return (u_long) BASIC_RHYTHM;
}

/*
 * This is called from the fshs_RevokeToken procedure, and is expected to
 * revoke all the tokens and return 0, or revoke some of them and return
 * HS_ERR_PARTIAL.
 */
#ifdef SGIMIPS
static long pxrep_RevokeSet(
struct fshs_host *hp,
int revokeLen,
register afsRevokeDesc *revokeListp)	/* really an array */
#else
static long pxrep_RevokeSet(hp, revokeLen, revokeListp)
struct fshs_host *hp;
int revokeLen;
register afsRevokeDesc *revokeListp;	/* really an array */
#endif
{
    register int ix, kahi, kalo, kamid;
    int found, anyKept, z;
    unsigned long Now;

    icl_Trace1(px_iclSetp, PX_TRACE_REVOKESET, ICL_TYPE_LONG, revokeLen);
    osi_assert(pxka_inited);
    anyKept = 0;
    for (ix = 0; ix < revokeLen; ++ix) {
	lock_ObtainWrite(&pxka_lock);
	Now = osi_Time();
	kahi = numKA - 1;	/* binary search on the local list */
	kalo = 0;
	found = 0;
	while (kalo <= kahi) {
	    kamid = (kalo + kahi) / 2;
	    z = cmpFids(&revokeListp[ix].fid, &pxKA[kamid].fid);
	    if (z < 0) {
		kahi = kamid - 1;
	    } else if (z > 0) {
		kalo = kamid + 1;
	    } else {
		found = 1;
		break;
	    }
	}
	AFS_hzero(revokeListp[ix].errorIDs);
	if (found) {
	    icl_Trace1(px_iclSetp, PX_TRACE_REVOKESETFOUND, ICL_TYPE_LONG, ix);
	    pxKA[kamid].flags |= kafl_Revoked;
	    if (pxKA[kamid].reclaimTime >= Now) {
		/* Can't give it up: we promised to hold it. */
		if (AFS_hsame(pxKA[kamid].tokID, revokeListp[ix].tokenID)) {
		    if ((pxKA[kamid].flags & kafl_HaveExecute) != 0) {
			revokeListp[ix].errorIDs = openExecute;
		    } else {
			revokeListp[ix].errorIDs = openPreserve;
		    }
		    ++anyKept;
		    pxKA[kamid].flags &= ~kafl_Revoked;
		    icl_Trace4(px_iclSetp, PX_TRACE_REVOKESETHOLDING_2,
			       ICL_TYPE_HYPER, &pxKA[kamid].tokID,
			       ICL_TYPE_FID, &pxKA[kamid].fid,
			       ICL_TYPE_LONG, pxKA[kamid].reclaimTime,
			       ICL_TYPE_LONG, pxKA[kamid].flags);
		} else if (AFS_hsame(pxKA[kamid].oldtokID, revokeListp[ix].tokenID)) {
		    AFS_hzero(pxKA[kamid].oldtokID);
		    icl_Trace3(px_iclSetp, PX_TRACE_REVOKESETRELINQUISH_2,
			       ICL_TYPE_HYPER, &pxKA[kamid].oldtokID,
			       ICL_TYPE_FID, &pxKA[kamid].fid,
			       ICL_TYPE_LONG, pxKA[kamid].flags);
		}
		/* else it's a token ID we don't care about to protect. */
	    }
	}
	lock_ReleaseWrite(&pxka_lock);
    }
    return (anyKept ? HS_ERR_PARTIAL : 0);
}

static long pxrep_OK() {return 0;}

/* 
 * Locks the host structure; READL or WRITEL are the only permissable locking 
 * types. Timeout bounds the time this call should wait if not immediately 
 * satisfiable.
 */
#ifdef SGIMIPS
static long pxrep_LockHost(
    struct hs_host *hostp,
    int type,
    long timeout)      /* Not implemented yet */
#else
static long pxrep_LockHost(hostp, type, timeout)
    struct hs_host *hostp;
    int type;
    long timeout;      /* Not implemented yet */
#endif
{
    if (type == HS_LOCK_READ) 
	lock_ObtainRead(&hostp->lock);
    else if (type == HS_LOCK_WRITE)
	lock_ObtainWrite(&hostp->lock);
#ifdef SGIMIPS
    return 0;
#endif
}


/* 	
 * Unlock the host structure: Unlock an already locked entry; type determines 
 * the  type of locking that was used (READL or WRITEL) 
 */
#ifdef SGIMIPS
static long pxrep_UnlockHost(
    struct hs_host *hostp,
    int type)
#else
static long pxrep_UnlockHost(hostp, type)
    struct hs_host *hostp;
    int type;
#endif
{
    if (type == HS_LOCK_READ) 
	lock_ReleaseRead(&hostp->lock);
    else if (type == HS_LOCK_WRITE)
	lock_ReleaseWrite(&hostp->lock);
#ifdef SGIMIPS
    return 0;
#endif
}


/*
 * Operations on the keep-alive host
 */
struct hostops pxrep_ops = {
    pxrep_OK,		/* hs_hold */
    pxrep_OK,		/* hs_rele */
    pxrep_LockHost,		/* hs_lock */
    pxrep_UnlockHost,	/* hs_unlock */
    pxrep_RevokeSet,		/* hs_revoketoken */
    pxrep_OK /* no async grants requested */,	/* hs_asyncgrant */
};

#ifdef SGIMIPS
void px_InitKeepAliveHost(void)
#else
void px_InitKeepAliveHost()
#endif
{
    osi_assert(pxka_inited == 0);

    lock_Init(&pxka_lock);

    pxKA = NULL;
    maxKA = numKA = 0;
    TKM_TOKENSET_CLEAR(&openPreserve);
    TKM_TOKENSET_ADD_TKNTYPE(&openPreserve, TKM_OPEN_PRESERVE);
    /* prevent writes to files being executed */
    TKM_TOKENSET_CLEAR(&openExecute);
    TKM_TOKENSET_ADD_TKNTYPE(&openExecute, TKM_OPEN_SHARED);
    /* all kinds of rights */
    TKM_TOKENSET_CLEAR(&openAll);
    AFS_hset64(openAll, ~(0L), ~(0L));

    /* Initialize the px_keepAliveHost proper. */
    bzero((char *)&px_keepAliveHost, sizeof(px_keepAliveHost));
    px_keepAliveHost.refCount = 1;	/* never changes; never deallocated. */
    px_keepAliveHost.hstopsp = &pxrep_ops;
    lock_Init(&px_keepAliveHost.lock);

    pxka_inited = 1;
}
