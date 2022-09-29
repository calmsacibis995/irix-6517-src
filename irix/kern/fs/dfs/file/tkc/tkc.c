/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkc.c,v 65.4 1998/04/01 14:16:17 gwehrman Exp $";
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
 * $Log: tkc.c,v $
 * Revision 65.4  1998/04/01 14:16:17  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for tkc_private.h.  Defined tkc_Hold() as void.
 * 	Defined tkc_ReleaseTokens() as void.  Changed prototype for
 * 	DoHandleClose to ansi style.
 *
 * Revision 65.3  1998/03/23  16:27:18  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:31  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:02  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.534.1  1996/10/02  21:01:15  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:07  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <tkc.h>
#include <dcedfs/zlc.h>
#include <dcedfs/krpc_pool.h>
#include <dcedfs/tkc_trace.h>
#ifdef SGIMIPS
#include <tkc_private.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc.c,v 65.4 1998/04/01 14:16:17 gwehrman Exp $")

struct krpc_pool tkc_concurrentCallPool;

struct hs_host tkc_Host;		/* Tkc's host struct */
struct tkc_stats tkc_stats;		/* Some general tkc stats */

struct lock_data tkc_rclock;

struct tkm_race tkc_race;

struct icl_set *tkc_iclSetp = 0;

static int isInit = 0;

void
tkc_Init(void)
{
    if (!isInit) {
	extern struct hostops tkchs_hops;
	extern struct squeue tkc_vclruq;
	register struct tkc_vcache *vcp;
	register int i;
	int code;
	struct icl_log *logp;

	isInit = 1;
	gluevn_InitThreadFlags();
	tkm_Init();

	/* setup icl trace code */
	code = icl_CreateLog("cmfx", 0, &logp);
	if (code == 0) {
	    icl_CreateSet("tkc", logp, (struct icl_log *) 0, &tkc_iclSetp);
	    /* start this guy off deactivated, since it generates so much
	     * output.
	     */
#ifdef notdef
	    if (code == 0)
		icl_SetSetStat(tkc_iclSetp, ICL_OP_SS_DEACTIVATE);
#endif /* notdef */
	}

	/* build a resource pool so we don't make too many calls
	 * at once.
	 */
	krpc_PoolInit(&tkc_concurrentCallPool, TKC_MAXCONCURRENTCALLS);

	/* tell RPC how many we may be making */
	krpc_AddConcurrentCalls(TKC_MAXCONCURRENTCALLS, /* server */ 0);

	tkm_InitRace(&tkc_race);
	bzero((caddr_t)&tkc_stats, sizeof(struct tkc_stats));
	bzero((caddr_t)&tkc_Host, sizeof(struct hs_host));
	lock_Init(&tkc_Host.lock);
	tkc_Host.hstopsp = &tkchs_hops;
	for (i = 0; i < TKC_VHASHSIZE; i++) {	/* Init vnode hash table */
	    QInit(&tkc_vhash[i].chain);
	    lock_Init(&tkc_vhash[i].lock);
	}
	for (i = 0; i < TKC_THASHSIZE; i++) {	/* Init token hash table */
	    QInit(&tkc_thash[i].chain);
	    lock_Init(&tkc_thash[i].lock);
	}
	/*
	 * see xvfs_vnode.h for a better description of
	 * XVFS_MAX_VNODE_RECYCLE_SIZE.  If there is an intelligent way of
	 * implementing the above comment, it should be done by replacing
	 * the symbolic constant with a function call! - wam 4/24/91
	 */
	QInit(&tkc_vclruq);
	vcp = (struct tkc_vcache *)
	    osi_Alloc(XVFS_MAX_VNODE_RECYCLE_SIZE * sizeof(struct tkc_vcache));
	bzero((caddr_t)vcp,
	    XVFS_MAX_VNODE_RECYCLE_SIZE * sizeof (struct tkc_vcache));
	lock_Init(&vcp->lock);
	lock_Init(&tkc_rclock);
	for (i = 0; i < XVFS_MAX_VNODE_RECYCLE_SIZE; i++, vcp++) {
	    QInit(&vcp->tokenList);
	    QAdd(&tkc_vclruq, &vcp->lruq);
	}
	/* Init tkc_alloc.c structures */
	tkc_InitSpace();
	/* Init tkc_cache.c structures */
	tkc_InitCache();
    }
}


/* check to see if we have a non-lock token of the type specified
 * by the incoming parameter.  The caller should have already done
 * a tkc_Get or the equivalent, since otherwise a revoke could come
 * in and revoke the tokens we're checking for.
 *
 * Returns true if we have the desired tokens, otherwise false (0).
 *
 * This function is almost identical to tkc_HaveTokens, except that
 * tkc_HaveTokens requires the caller to already hold the lock.
 *
 * We do it inline for speed.
 */
tkc_CheckTokens(vcp, tokensNeeded)
  register struct tkc_vcache *vcp;
  long tokensNeeded;
{
    lock_ObtainRead(&vcp->lock);
    tokensNeeded &= ~(vcp->heldTokens);
    lock_ReleaseRead(&vcp->lock);
    /* tokensNeeded is now the set of tokens we want that we
     * don't have.
     */
    return (tokensNeeded? 0 : 1);
}


/* get a token whose type is described by tokenType and whose byte
 * range is specified by byteRangep.  Never tries queuing the request
 * since calls that enter here request tokens whose holders never
 * refuse to yield the token (i.e. data tokens and status tokens)
 * or for whom refusing to obtain a token is an error (open for read or
 * write tokens).  Increments the dataHold counter while holding the
 * lock to prevent racing revokes from revoking token before return.
 *
 * CAUTION --
 *     At present we do not support non-trivial byte ranges in TKC data tokens.
 *     The byteRangep argument must be null.
 */
struct tkc_vcache *tkc_Get(
  struct vnode *vp,
  u_long tokenType,
  tkc_byteRange_t *byteRangep)
{
    register struct tkc_vcache *vcp;
    int code;

    icl_Trace1(tkc_iclSetp, TKC_TRACE_GET, ICL_TYPE_LONG, tokenType);
    vcp = tkc_GetVcache(vp);
    lock_ObtainWrite(&vcp->lock);
    if (tkc_HaveTokens(vcp, tokenType, byteRangep)) {
	vcp->dataHolds++;
	lock_ReleaseWrite(&vcp->lock);
	icl_Trace1(tkc_iclSetp, TKC_TRACE_GET_HAVE, ICL_TYPE_POINTER, vcp);
	return vcp;				/* Success! */
    }
    code = tkc_GetToken(vcp, tokenType, byteRangep, /* wait */ 0, NULL);
    if (code) {
	lock_ReleaseWrite(&vcp->lock);
	tkc_Release(vcp);
	icl_Trace1(tkc_iclSetp, TKC_TRACE_GET_FAIL, ICL_TYPE_LONG, code);
	return (struct tkc_vcache *) 0;
    }
    vcp->dataHolds++;
    lock_ReleaseWrite(&vcp->lock);
    icl_Trace1(tkc_iclSetp, TKC_TRACE_GET_GOT, ICL_TYPE_POINTER, vcp);
    return (vcp);
}


/* release a vcache entry and a data hold */
void
tkc_Put(struct tkc_vcache *vcp)
{
    lock_ObtainWrite(&vcp->lock);
    if (vcp->dataHolds-- == 0) panic("tkc datahold");
    if (vcp->vstates & TKC_DHWAITING) {
	vcp->vstates &= ~TKC_DHWAITING;
	osi_Wakeup(vcp);
    }
    lock_ReleaseWrite(&vcp->lock);
    
    tkc_Release(vcp);
    icl_Trace2(tkc_iclSetp, TKC_TRACE_PUT,
	       ICL_TYPE_POINTER, vcp, ICL_TYPE_LONG, vcp->dataHolds);
}

/* Quickly release a vcache entry obtained via tkc_Get.
 *
 * For very specialized use: safe to use when holding all locks,
 * except for tkc_rclock.
 * However, you have to guarantee that you'll decrement the ref count
 * again before you're done with the entry, so that the finalization
 * code can eventually get run.
 * Typically used when traversing a hash table, e.g. tkc_flushvfsp.
 * When in doubt, don't use this function!
 */
tkc_QuickRelease(vcp)
    register struct tkc_vcache *vcp;
{
    lock_ObtainWrite(&tkc_rclock);
    if (vcp->refCount-- == 0) panic("tkc refcount");
    lock_ReleaseWrite(&tkc_rclock);
    return 1;
}


/* release a vcache entry obtained via tkc_Get.  Can't hold any locks,
 * since tkc_RecycleVcache grabs piles of locks.
 *
 * Returns 0 iff the vcache entry was destroyed and all tokens
 * were returned.  Necessary due to oddity in AsyncGrant (see
 * ProcessGrant for details).
 */
tkc_Release(vcp)
    register struct tkc_vcache *vcp;
{
    /* just a hint, so we don't care if we're missing some locking */
    if (vcp->tokenAddCounter != vcp->tokenScanValue) {
	/* prune non-lock tokens; they'll be deleted by DoDelToken */
	tkc_PruneDuplicateTokens(vcp);
    }

    /* still a hint */
    if (vcp->vstates & TKC_DELTOKENS) {
	tkc_DoDelToken(vcp);	/* called with no locks held */
    }

    lock_ObtainWrite(&tkc_rclock);
    if (vcp->gstates & TKC_GDISCARD) {
	/* try to discard this element but if fail because
	 * reference count is too high, we can delay the
	 * real release until the final release call.
	 */
	lock_ReleaseWrite(&tkc_rclock);
	return tkc_RecycleVcache(vcp, 1);
    }
    else
	if (vcp->refCount-- == 0) panic("tkc refcount");
    lock_ReleaseWrite(&tkc_rclock);
    icl_Trace2(tkc_iclSetp, TKC_TRACE_RELEASE,
	       ICL_TYPE_POINTER, vcp, ICL_TYPE_LONG, vcp->refCount);
    return 1;
}


#ifdef SGIMIPS
void
#endif
tkc_Hold(vcp)
    register struct tkc_vcache *vcp;
{
    lock_ObtainWrite(&tkc_rclock);
    vcp->refCount++;
    lock_ReleaseWrite(&tkc_rclock);
}

/* get a set of tokens atomically.  Postcondition is that all tokens
 * are obtained.  Because we do not lock tokens in a particular order,
 * we can not refuse to return tokens while obtaining new tokens.
 * This means that we can't increment things like dataHolds, or the
 * various open counts, until we've successfully obtained all of the
 * tokens, since revoke procedure just blocks if it encounters a locked
 * vcache entry with non-zero dataHolds.
 *
 * CAUTION --
 *   At present we do not support non-trivial byte ranges in TKC data tokens.
 *   The byte ranges in all the requested tokens must be whole-file.
 */
long tkc_GetTokens(struct tkc_sets *setp, long size)
{
    register struct tkc_sets *sp;
    register struct tkc_vcache *vcp;
    struct tkc_vcache *blockerp;
    register long code, i;
    int j;

    icl_Trace2(tkc_iclSetp, TKC_TRACE_GETTOKENS,
	       ICL_TYPE_POINTER, setp, ICL_TYPE_LONG, size);
    /* Our goal here is to obtain all of the tokens without ever
     * blocking while holding a locked vcache entry or even having
     * a vcache entry with a high dataHolds count (since that alone
     * could block a revoke request).
     * Thus, we first hold all of the vcache entries.  We then lock
     * everything, and check to see if we have the desired tokens.
     * If we do, we bump all the dataHold counters, release all the
     * locks, and return success.  Otherwise, we release all locks
     * except the dude missing some tokens, get his tokens and then
     * try again.
     */
    for (sp=setp, i = 0; i < size; i++, sp++) {
	vcp = tkc_GetVcache(sp->vp);
	if (!vcp) {
	    tkc_ReleaseTokens(setp, i);
	    tkc_stats.gettokensF++;
	    code = EINVAL;
	    return code;
	}
	sp->vcp = vcp;
    }	/* hold vcache for loop */
    while (1) {
	/* remember guy who prevents us from setting locks; later we'll
	 * block on just him if we can't set all locks.  If block on
	 * many, we could cause deadlock, unless we were careful about
	 * our locking order.
	 */
	blockerp = (struct tkc_vcache *) 0;

	/* try to set all locks, remembering who prevents us, if anyone */
	for (sp=setp, i = 0; i < size; i++, sp++) {
	    vcp = sp->vcp;
	    code = lock_ObtainWriteNoBlock(&vcp->lock);
	    if (code == 0) {
		blockerp = vcp;
		break;
	    }
	}	/* get tokens for loop */

	/* if can't set all, wait for blocking guy alone, and then
	 * retry.  Dudes in [0 - i-1] are already locked.
	 */
	if (blockerp) {
	    /* undo, wait for a better time and retry */
	    for(sp=setp, j=0; j<i; j++, sp++) {
		vcp = sp->vcp;
		lock_ReleaseWrite(&vcp->lock);
	    }
	    /* now wait for the guy blocking our progress */
	    lock_ObtainWrite(&blockerp->lock);
	    lock_ReleaseWrite(&blockerp->lock);
	    continue;	/* try again */
	}

	/* now we have all items safely locked.  Check we have
	 * appropriate tokens, and if not, retry things again,
	 * otherwise claim victory.
	 */
	blockerp = (struct tkc_vcache *) 0;
	for(sp=setp, i=0; i<size; i++, sp++) {
	    vcp = sp->vcp;
	    if (!tkc_HaveTokens(vcp, sp->types, sp->byteRangep)) {
		blockerp = vcp;
		break;
	    }
	}
	if (blockerp) {
	    /* missing a token, must release locks and obtain the token.
	     * leave sp alone from break above.
	     */
	    for(i=0; i<size; i++) {
		vcp = setp[i].vcp;
		if (vcp != blockerp)
		    lock_ReleaseWrite(&vcp->lock);
	    }
	    /* now we've returned all locks but blockerp's; get the token */
	    code = tkc_GetToken(blockerp, sp->types, sp->byteRangep, 0, NULL);
	    lock_ReleaseWrite(&blockerp->lock);
	    continue;
	}
	/* if we make it here, we've essentially won.  We have all
	 * tkc_vcache entries locked, and all have the desired tokens.
	 * just bump dataHolds, so that we keep these tokens after
	 * unlocking things, and then unlock things.
	 */
	for(sp=setp, i=0; i<size; i++, sp++) {
	    vcp = sp->vcp;
	    vcp->dataHolds++;
	    lock_ReleaseWrite(&vcp->lock);
	}
	icl_Trace1(tkc_iclSetp, TKC_TRACE_GETTOKENS_END,
		   ICL_TYPE_LONG, code);
	return 0;
    }		/* whole while loop */
    /* not reached */
}


/* release the reference count on the vcache entries in
 * this set.
 */
#ifdef SGIMIPS
void
#endif
tkc_ReleaseTokens(setsp, size)
    struct tkc_sets *setsp;
    register long size;
{
    long i;

    icl_Trace1(tkc_iclSetp, TKC_TRACE_RELEASETOKENS, ICL_TYPE_LONG, size);
    for (i = 0; i <  size; i++, setsp++) {
	if (setsp->vcp) {
	    tkc_Release(setsp->vcp);
	    setsp->vcp = (struct tkc_vcache *)0;	
	}
    }
}


/* drop the reference count and data holds on the vcache
 * entries in this set.
 */
void
tkc_PutTokens(struct tkc_sets *setsp, long size)
{
    long i;

    icl_Trace1(tkc_iclSetp, TKC_TRACE_PUTTOKENS, ICL_TYPE_LONG, size);
    for (i = 0; i <  size; i++, setsp++) {
	if (setsp->vcp) {
	    tkc_Put(setsp->vcp);
	    setsp->vcp = (struct tkc_vcache *)0;	
	}
    }
}


#ifdef SGIMIPS
static void DoHandleClose(struct tkc_vcache *vcp, int arw);
#else
static
void DoHandleClose ();
#endif

/* mark that we've opened the specified file, in write mode if arw is true,
 * otherwise in read mode.  Handle the getting of the appropriate tokens, too.
 * return a held token cache entry.  Called with nothing locked.  Returns
 * held vcache entry, or null if failed.
 *
 * The arw parm is 0 for open/read, 1 for open/write, and 2 for open/exec. 
 * Bit 4 set means that we also need status/write and data/write because we're
 * going to do a truncate.  This is used in AIX where the same vnode op handles
 * both opening and truncation.
 */
int tkc_HandleOpen(vp, arw, vcpp)
  register struct vnode *vp;
  int arw;
  struct tkc_vcache **vcpp;
{
    register struct tkc_vcache *vcp;
    long tokenType;
    long code;
    int arw_orig;
    
    /* determine which open token we require */
    arw_orig = arw;
    arw &= ~4;
    if (arw == 0)
	tokenType = TKN_OPEN_READ;
    else if (arw == 1)
	tokenType = TKN_OPEN_WRITE;
    else if (arw == 2)
	tokenType = TKN_OPEN_SHARED;
    vcp = tkc_GetVcache(vp);

    /* get token, if we don't already have it */
    lock_ObtainWrite(&vcp->lock);
    code = 0;
    if (!tkc_HaveTokens(vcp, tokenType, NULL)) {
	/* try getting a new token */
       code = tkc_GetToken(vcp, tokenType, NULL, /* block */ 0, NULL);
    }
    if (code == 0) {
	if (arw == 0) vcp->readOpens++;
	else if (arw == 1) vcp->writeOpens++;
	else if (arw == 2) vcp->execOpens++;

	/*
	 * Get status and data tokens if requested.  This won't normally fail,
	 * but if it does, we have to undo the open.
	 */
	if (arw_orig & 4) {
	    tokenType = TKN_STATUS_WRITE|TKN_DATA_WRITE;
	    if (!tkc_HaveTokens(vcp, tokenType, NULL)) {
		code = tkc_GetToken(vcp, tokenType, NULL, /* block */ 0, NULL);
	    }
	    if (code != 0) {
		DoHandleClose(vcp, arw);
	    }
	}
    }
    lock_ReleaseWrite(&vcp->lock);
    if (code)
	tkc_Release(vcp);
    else
	*vcpp = vcp;
    return code;
}


/* mark that we've just closed the specified file in the mode specified by arw
 * (possible values of arw are as with tkc_HandleClose).  Called with no
 * locks held.  Returns a held and unlocked vcache entry.
 */
struct tkc_vcache *tkc_HandleClose(vp, arw)
    register struct vnode *vp;
    int arw; 
{
    register struct tkc_vcache *vcp;

    vcp = tkc_FindVcache(vp);
    /*
     * should be here, since we shouldn't be GC'ing vcache entries representing
     * open files.  Of course, better safe than sorry....
     */
    if (vcp) {
	lock_ObtainWrite(&vcp->lock);
	DoHandleClose (vcp, arw);
	lock_ReleaseWrite(&vcp->lock);
    }
    return vcp;
}


/* Do the work of tkc_HandleClose (see above).
 * The parameter is a locked vcache entry.
 */
static
void DoHandleClose(vcp, arw)
    register struct tkc_vcache *vcp;
    int arw;
{
    long returnType;

    returnType = 0;
    /* adjust the count down */
    if (arw == 0) --vcp->readOpens;
    else if (arw == 1) --vcp->writeOpens;
    else if (arw == 2) --vcp->execOpens;

#ifdef AFS_SUNOS5_ENV
    /* Solaris 2.2 has bugs wherein we can get mismatched VOP_OPENs
     * and VOP_CLOSEs.  Bullet-proof this code against mismatch.
     */
    if (vcp->readOpens < 0) vcp->readOpens = 0;
    if (vcp->writeOpens < 0) vcp->writeOpens = 0;
    if (vcp->execOpens < 0) vcp->execOpens = 0;
#endif /* AFS_SUNOS5_ENV */

    /* now return the tokens as long as we have a 0 open count on the
     * type of open we're checking.
     */
    while (1) {
	/* figure out what type of token we want to return (based
	 * on the type of close) and that we're allowed to return
	 * (based on the types of open still done).  If we
	 * have nothing to do, then we're done.
	 */
	returnType = 0;
	if (arw == 0) {
	    if (vcp->readOpens == 0) returnType = TKN_OPEN_READ;
	}
	else if (arw == 1) {
	    if (vcp->writeOpens == 0) returnType = TKN_OPEN_WRITE;
	}
	else if (arw == 2) {
	    if (vcp->execOpens == 0) returnType = TKN_OPEN_SHARED;
	}

	/* now, if returnType is 0, someone opened the file on us.
	 * we've returned all the tokens we can, so we should stop.
	 */
	if (returnType == 0) break;

	/* otherwise, it is still safe to return tokens of type
	 * returnType, so look for one and return it.  If we
	 * don't find any, we're done.  If we do find some,
	 * we'll return one and then iterate through this loop again.
	 *
	 * ReturnSpecificTokens returns true if it blocks, false
	 * if it doesn't find any tokens matching the type
	 * specified.  If there are none, we're done.
	 */
	if (tkc_ReturnSpecificTokens(vcp, returnType) == 0) break;
	/* otherwise, check to see if the file is still closed
	 * before returning more tokens.
	 */
    }
}

/* Handle a vnode deletion, either directly or via the ZLC module */
long tkc_HandleDelete(vp, volp)
    struct vnode *vp; 
    struct volume *volp;  /* currently unused */
{
    register struct tkc_vcache *vcp;

    vcp = tkc_GetVcache(vp);

    lock_ObtainWrite(&vcp->lock);

    if (tkc_HaveTokens(vcp, TKN_OPEN_DELETE, NULL)) {
	/* we've already got an open/exclusive token, so we just
	 * have to make sure that this entry gets destroyed when the
	 * refCount goes to 0.
	 */
	lock_ObtainWrite(&tkc_rclock);
	vcp->gstates |= TKC_GDISCARD;
	lock_ReleaseWrite(&tkc_rclock);
	
	lock_ReleaseWrite(&vcp->lock);
    } else {
	/* do not have open/exclusive token; let ZLC module handle delete */
	lock_ReleaseWrite(&vcp->lock);

	zlc_TryRemove(vp, &vcp->fid);
    }

    tkc_Release(vcp);
    return 0;
}

/* Get a token, possibly blocking or queueing request processing for
 * later.  Return the new token *locked* so as to allow the caller to
 * increment the appropriate hold(s) before releasing the lock.
 * MUST BE CALLED WITH tkc_vcache WRITE-LOCKED!
 * blocked 1 bit means block (wait for async grant locally)
 * blocked 2 bit means async (queue the operation and return)
 */
long tkc_GetToken(
  struct tkc_vcache *vcp,
  u_long tokenType,
  tkc_byteRange_t *byteRangep,
  long block,
  afs_recordLock_t *rblockerp)
{
    register long code = 0;
    afs_token_t token;
    register struct tkc_tokenList *tlp;
    static int tcounter = 1;
    int async;
    int flags;
    struct tkm_raceLocal localRaceState;

    async = block & 2;	/* bit 2 means async */
    block &= 1;	/* just keep the 1 bit for blocking flag */
    if (byteRangep) {
	token.beginRange = byteRangep->beginRange;
	token.endRange = byteRangep->endRange;
    } else {
	AFS_hzero(token.beginRange);
	token.endRange = osi_hMaxint64;
    }
    while (1) {
	tcounter++;		/* don't reuse request IDs */
	/* init this in the loop, since tkm_EndRacingCall may
	 * change token.type.
	 */
	AFS_hset64(token.type, 0, tokenType);
	tlp = tkc_AddToken(vcp, &token); /* do early to catch quick revokes */
	/* above sets TKC_LLOCK if this is a requested lock token */
	tlp->reqID = tcounter;
	if (block) tlp->states |= TKC_LBLOCK;
	tkm_StartRacingCall(&tkc_race, &vcp->fid, &localRaceState);
	lock_ReleaseWrite(&vcp->lock);

	/* wait for a spare call to become available.  Too many potential
	 * callers exist (every user thread on a server) to preallocate
	 * these dudes.
	 */
	krpc_PoolAlloc(&tkc_concurrentCallPool, 0, 1);

	/* 
	 * rlockerp should be NULL, IF tokenType is not Lock Token 
	 */
	icl_Trace2(tkc_iclSetp, TKC_TRACE_GETTOKENSTART,
		   ICL_TYPE_POINTER, (long) vcp,
		   ICL_TYPE_LONG, AFS_hgetlo(token.type));
	flags = TKM_FLAGS_NOEXPIRE;	/* our tokens never expire */
	/* if not a lock token, do optimistic grant */
	if (!(tlp->states & TKC_LLOCK))
	    flags |= TKM_FLAGS_OPTIMISTICGRANT;
	/* if we want to wait, use queuerequest flag */
	if (block || async) flags |= TKM_FLAGS_QUEUEREQUEST;
	code = tkm_GetToken(&vcp->fid, flags, (afs_token_t *) &token,
			    &tkc_Host, tcounter,rblockerp);
	icl_Trace1(tkc_iclSetp, TKC_TRACE_GETTOKENEND, ICL_TYPE_LONG, code);

	/* return call to pool */
	krpc_PoolFree(&tkc_concurrentCallPool, 1);

#if !defined(KERNEL)
	pthread_yield();
#endif /* !defined(KERNEL) */

	lock_ObtainWrite(&vcp->lock);
	tkm_EndRacingCall(&tkc_race, &vcp->fid,
			  (tkm_raceLocal_t *) &localRaceState,
			  (afs_token_t *) &token);
	if (code == TKM_ERROR_REQUESTQUEUED) {/* token req was queued ok */
	    if (!async) {
		/* synchronously wait for async grant */
		code = 0;
		while (tlp->states & TKC_LQUEUED) {
		    code = osi_SleepWI((opaque)vcp, &vcp->lock);
		    if (code) {
			/* signalled, return EINTR.  Async grant will
			 * come in later, but this guy will be deleted
			 * by that time, and so token will be returned
			 * immediately.
			 */
			code = EINTR;
			lock_ObtainWrite(&vcp->lock);
			tkc_DelToken(tlp);
			break;
		    }
		    lock_ObtainWrite(&vcp->lock);
		}
		/* fall through and check that we still have token */
	    } /* async? */
	    else {
		/* If async call don't wait.  Async processing will handle
		 * finalization.  Just return TKM_ERROR_REQUESTQUEUED.
		 * Caller has to deal with possibility that tokens
		 * have vanished by the time the async grant is called.
		 */
		break;
	    };
	} else if (code == 0) {/* got the token */
	    /* code was 0, process grant now */
	    tkc_FinishToken(vcp, tlp, &token);
	}
	else if (code == TKM_ERROR_TOKENCONFLICT) {
	    tkc_DelToken(tlp);
	    if (tokenType & (TKN_LOCK_READ | TKN_LOCK_WRITE ))
		code = EWOULDBLOCK;
	    else if (tokenType & (TKN_OPEN_READ | TKN_OPEN_WRITE
				  | TKN_OPEN_SHARED | TKN_OPEN_EXCLUSIVE)) {
		code = ETXTBSY;
	    }
	    else {
		/* we were refused a data or status token, just
		 * wait until it gets better (treat it as a busy volume,
		 * since the cause is a conflicting fts command).
		 */
		lock_ReleaseWrite(&vcp->lock);
		osi_Wait(2000, 0, 0);	/* wait a few seconds */
		lock_ObtainWrite(&vcp->lock);
		continue;		/* and try again */
	    }
	}
	else if (code == TKM_ERROR_TRYAGAIN) {
	    tkc_DelToken(tlp);
	    lock_ReleaseWrite(&vcp->lock);
	    osi_Wait(1000, 0, 0);
	    lock_ObtainWrite(&vcp->lock);
	    continue;	/* Try again */
	}
	else {
	    /* unknown or expected error code */
	    if (code == TKM_ERROR_NOTINMAXTOKEN)
		code = EROFS;/* asked for more than is possible */
	    tkc_DelToken(tlp);
	}
	if (code != 0 || (AFS_hgetlo(token.type) & tokenType) == tokenType)
	    break;
	/* otherwise, we have a successful call, but by the time
	 * we relocked the vcache entry, some of the tokens were
	 * revoked.  So, we try again after returning the remaining
	 * tokens associated with this ID.
	 */
	tkc_DelToken(tlp);
	vcp->heldTokens = tkc_OrAllTokens(&vcp->tokenList);
	if (!tkm_IsGrantFree(token.tokenID)) {
	    lock_ReleaseWrite(&vcp->lock);
	    TKM_RETTOKEN(&vcp->fid, &token, TKM_FLAGS_PENDINGRESPONSE);
	    lock_ObtainWrite(&vcp->lock);
	}
	/* and go around and try again */
    }	/* fundamental while loop */
    return code;
}
