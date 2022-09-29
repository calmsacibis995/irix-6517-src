/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_dnamehash.c,v 65.7 1998/07/29 19:40:26 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/tkm_errs.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/dacl.h>
#include <dcedfs/direntry.h>		/* Need for bulkstat. */
#include <cm.h>				/* Cm-based standard headers */
#include <cm_dnamehash.h>
#include <cm_volume.h>
#include <cm_conn.h>
#include <cm_stats.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_dnamehash.c,v 65.7 1998/07/29 19:40:26 gwehrman Exp $")

static struct squeue nh_lru;			/* LRU queue for directory names */
struct squeue nh_cache[NH_HASHSIZE];	/* Dir name cache */
struct nh_stats nh_stats; 		/* name hash stats */
static struct nh_cache *find _TAKES((struct cm_scache *, char *, int, int32));
static struct lock_data nh_lock;		/* lock for global state */
static void purge _TAKES((struct nh_cache *));

/*
 * Initialize the directory name hash module
 */
static int nh_initialized=0;

#ifdef SGIMIPS
void nh_init(int hashsize)
#else  /* SGIMIPS */
void nh_init(hashsize)
    int hashsize;
#endif /* SGIMIPS */
{
    register struct nh_cache *nhp;
    int i;

    if (nh_initialized)
	return;
    nh_initialized = 1;
    QInit(&nh_lru);
    lock_Init(&nh_lock);
    nhp = (struct nh_cache *) osi_Alloc(hashsize * sizeof(struct nh_cache));
    bzero((char *) nhp, hashsize * sizeof(struct nh_cache));
    for (i = 0; i < hashsize; i++) {
	QAdd(&nh_lru, &nhp[i].lruq);
	/* 
	 * all entries are either in their own singleton queue (so RMHASH 
	 * works) or are in a normal hash queue.
	 */
	NH_NULLHASH(&nhp[i]);
    }
    /* 
     * Now initialize the cache hash 
     */
    for (i = 0; i < NH_HASHSIZE; i++)
	NH_NULLHASH(&nh_cache[i]);
}

/*
 * Lookup a name in the name hash, but don't go to the server (not even for
 * tokens).  The dir entry must not be locked.
 */
#ifdef SGIMIPS
int
nh_locallookup(
    struct cm_scache *dscp,
    char *namep,
    afsFid *fidp) 
#else  /* SGIMIPS */
nh_locallookup(dscp, namep, fidp)
    struct cm_scache *dscp;
    char *namep;
    afsFid *fidp; 
#endif /* SGIMIPS */
{
    long code;

    lock_ObtainRead (&dscp->llock);

    if (!cm_HaveTokens(dscp, TKN_READ)) {
	lock_ReleaseRead(&dscp->llock);
	return ESTALE;
    }
    code = nh_lookup(dscp, namep, fidp);

    lock_ReleaseRead(&dscp->llock);

    if (code == 0) {
	CM_BUMP_COUNTER(nameCacheHits);
	if (fidp->Vnode)
	    return 0;
	else
	    return ENOENT;
    } else
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
}

/*
 * Pipe push routine for bulk stat RPC.
 */

#ifdef SGIMIPS
void cm_PipePush(
    void *fstate,
    unsigned char *buf,
    unsigned long ecount)
#else  /* SGIMIPS */
void cm_PipePush(fstate,buf,ecount)
    void *fstate;
    unsigned char *buf;
    unsigned long ecount;
#endif /* SGIMIPS */
{
    struct pipeDState *FState = (struct pipeDState *)fstate;

    /* more to push? */
    if (ecount == 0)
	return;

    FState->scode = 0;
    FState->offset += ecount;
}

/*
 * RPC to hint the name cache and the status cache for a window of files
 * in directory.  Come here with no locks held. The file names are
 * transferred from the server in directory record wire format but we only
 * need the names for current use.  We bulk stat AFS_BULKMAX files or
 * osi_BUFFERSIZE of directory info, whichever comes first, or whatever the
 * server sends.
 */

/* clear to disable bulk stat */

int enable_bulkstat=1;

#ifdef SGIMIPS
int
cm_BulkFetchStatus(
    struct cm_scache *scp,
    struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
cm_BulkFetchStatus(scp, rreqp)
struct cm_scache *scp;
struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    long code=0;
    struct cm_conn *connp;
    error_status_t st;
    struct cm_volume *volp = NULL;
    long srvix;
    u_long startTime;
    BulkStat *bulk_stats;
    afsVolSync Sync;
    afs_hyper_t *minVVp;
    int i;
    long irevcnt;
    afsTimeval startDirChangeTime;
    struct cm_scache *dummy_scp;
    char *namep;
    afsFetchStatus *statp;
    afs_token_t *tokenp;
    afsFid *fidp;
    afsFetchStatus *dirstatp;
    struct cm_server *serverp=0;
    char *stat_buf=NULL;
    pipe_t dirStream;
    struct pipeDState pipeState;
    afs_hyper_t NextOffset;
    unsigned32 Size;
    afs_hyper_t Offset;
    struct dir_minwireentry *dp;
    struct lclHeap {
	afsFetchStatus OutDirStatus;
	afs_token_t OutToken;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lp = &lh;
#endif /* AFS_SMALLSTACK_ENV */

    icl_Trace1(cm_iclSetp, CM_TRACE_BULKSTAT, ICL_TYPE_POINTER, scp);

    /* sanity check - make sure it's a directory */

    lock_ObtainWrite(&scp->llock);
    if (cm_vType(scp) != VDIR) {
	lock_ReleaseWrite(&scp->llock);
	return -1;
    }

    /* Check tokens for directory lookup. */
    if (code = cm_GetTokens(scp, TKN_READ, rreqp, WRITE_LOCK)) {
	lock_ReleaseWrite(&scp->llock);
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }

    /* make sure offset is at least within length */
    if (scp->m.Length <= AFS_hgetlo(scp->bulkstatdiroffset)) {
	icl_Trace2(cm_iclSetp, CM_TRACE_BULKSTAT_OFFSET, 
		   ICL_TYPE_LONG, scp->m.Length,
		   ICL_TYPE_LONG, AFS_hgetlo(scp->bulkstatdiroffset));
	AFS_hzero(scp->bulkstatdiroffset);
	lock_ReleaseWrite(&scp->llock);
	return -1;
    }

    /* pick up current offset into directory */
    Offset = scp->bulkstatdiroffset;

    /* prep to record dir change by another client */
    irevcnt = scp->dirRevokes;

    /* prep to record dir change by another thread */
    startDirChangeTime = scp->m.ServerChangeTime;

    lock_ReleaseWrite(&scp->llock);

    /* 8 kbytes of array data */
    stat_buf = osi_AllocBufferSpace();

    /* bzero((caddr_t)stat_buf, osi_BUFFERSIZE); */
    bulk_stats = (BulkStat *)stat_buf;
    bulk_stats->BulkStat_len = 0;
    /* this is essentially a compile-time check, which will compile into
     * a noop or a panic, but which allows us to use bulk_stats as a pointer.
     */
#ifdef SGIMIPS
/* Turn off "expression has no effect" error. */
#pragma set woff 1171
#endif
    /* CONSTCOND */
    osi_assert(sizeof(BulkStat) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif

    /* and 8 kbytes of directory results */
    Size = osi_BUFFERSIZE;

    /* pipe setup */
    dirStream.alloc = cm_BufAlloc;	/* reuse same buffer */
    dirStream.state = (char *)&pipeState;
#ifdef SGIMIPS
    dirStream.push =(void (*)(rpc_ss_pipe_state_t,  idl_byte *, idl_ulong_int))
		    cm_PipePush;	 /* pipe push routine */
#else  /* SGIMIPS */
    dirStream.push = cm_PipePush;	 /* pipe push routine */
#endif /* SGIMIPS */
    ((struct pipeDState *)dirStream.state)->bsize = osi_BUFFERSIZE;
    /* 8 kb at a time: */
    ((struct pipeDState *)dirStream.state)->bufp = osi_AllocBufferSpace();

#ifdef AFS_SMALLSTACK_ENV
    /* Keep stack frame small by doing heap-allocation */
    lp = (struct lclHeap *)osi_AllocBufferSpace();
#ifdef SGIMIPS
#pragma set woff 1171
#endif
    /* CONSTCOND */
    osi_assert(sizeof(struct lclHeap) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif
#endif /* AFS_SMALLSTACK_ENV */

    cm_StartTokenGrantingCall();
    do {
	/* more pipe setup */
	((struct pipeDState *)dirStream.state)->scode = 0;
	((struct pipeDState *)dirStream.state)->offset = 0;

	connp = cm_Conn(&scp->fid, MAIN_SERVICE(SRT_FX), rreqp, &volp, &srvix);
	if (connp) {
	    if (volp && ((volp->states & VL_LAZYREP) != 0))
		minVVp = &volp->latestVVSeen;
	    else
		minVVp = &osi_hZero;

	    startTime = osi_Time();
	    serverp = connp->serverp;
	    lock_ObtainRead(&serverp->lock);
	    if (serverp->states & SR_RPC_BULKSTAT) {
		lock_ReleaseRead(&serverp->lock);

		/* server does not support RPC */
		icl_Trace0(cm_iclSetp, CM_TRACE_BULKSTAT_NORPC);
		cm_PutConn(connp);
		cm_EndTokenGrantingCall((afs_token_t *)0, (struct cm_server *)0,
					(struct cm_volume *)0, startTime);
		code = -1;
		goto norpc;
	    }
	    lock_ReleaseRead(&serverp->lock);

	    osi_RestorePreemption(0);

	    st = AFS_BulkFetchStatus(connp->connp, &scp->fid, &Offset, Size,
				     minVVp, AFS_FLAG_RETURNTOKEN, bulk_stats,
				     &NextOffset, &(lp->OutDirStatus),
				     &(lp->OutToken), &Sync, &dirStream);

	    if (st == 0 && AFS_hcmp(NextOffset, osi_maxFileClient) >= 0) {

		/* If the returned offset is beyond what we can handle just
		 * turn this into EOVERFLOW. */

		st = DFS_EOVERFLOW;
	    }
	    code = osi_errDecode(st);

	    /* No retry in these cases: */
	    if ((code == TKM_ERROR_TOKENCONFLICT) ||
		(code == TKM_ERROR_TRYAGAIN))
		code = TKM_ERROR_HARDCONFLICT;

	    osi_PreemptionOff();

	    cm_SetReqServer(rreqp, connp->serverp);

	    if (code == error_status_ok)
		code = ((struct pipeDState *)dirStream.state)->scode;
	} else
	    code = -1;

	/* check Sync */
	if (code == 0)
#ifdef SGIMIPS
	    code = (int)cm_CheckVolSync(&scp->fid, &Sync, volp, startTime, 
				   srvix, rreqp);
#else  /* SGIMIPS */
	    code = cm_CheckVolSync(&scp->fid, &Sync, volp, startTime, srvix,
				   rreqp);
#endif /* SGIMIPS */
    } while (cm_Analyze(connp, code, &scp->fid, rreqp, volp, srvix,
			startTime));

    /*
     * fatal error during RPC?
     */
    if (code) {
	/* trying to talk to old server? */
	if ((code == rpc_s_op_rng_error) && serverp) {
	    lock_ObtainWrite(&serverp->lock);
	    serverp->states |= SR_RPC_BULKSTAT;
	    lock_ReleaseWrite(&serverp->lock);

	    icl_Trace0(cm_iclSetp, CM_TRACE_BULKSTAT_NORPC);
	}

	AFS_hzero(scp->bulkstatdiroffset);
	cm_EndTokenGrantingCall((afs_token_t *)0, (struct cm_server *)0,
				(struct cm_volume *)0, startTime);
	goto norpc;
    }

    /* sanity check */

    if (bulk_stats->BulkStat_len<=0) {
	lock_ObtainWrite(&scp->llock);
	goto seterror;
    }

    /* pick up current status of this dir */
    dirstatp = &(lp->OutDirStatus);

    /* check for dir changes */
    lock_ObtainWrite(&scp->llock);
    cm_EndPartialTokenGrant(&(lp->OutToken), serverp, volp, startTime);
    cm_MergeStatus(scp, dirstatp, &(lp->OutToken), 0, rreqp);

    /* check for token changes */
    if ((AFS_hgetlo(lp->OutToken.type) & (TKN_OPEN_PRESERVE | TKN_STATUS_READ))
	!= (TKN_OPEN_PRESERVE | TKN_STATUS_READ)) {
	/*
	 Token got revoked, so don't hint the caches.
	 */
	icl_Trace0(cm_iclSetp, CM_TRACE_BULKSTAT_NOTOK);
	goto seterror;
    }

    if (irevcnt != scp->dirRevokes) {
	/* Some other client changed dir. */
	icl_Trace0(cm_iclSetp, CM_TRACE_BULKSTAT_OTHERCLIENT);
	goto seterror;
    }

    if (cm_tcmp(scp->m.ServerChangeTime, startDirChangeTime) > 0) {
	/* Some other thread changed directory. */
	icl_Trace0(cm_iclSetp, CM_TRACE_BULKSTAT_OTHERTHREAD);
	goto seterror;
    }

    /*
     * Make two passes over the returned info.
     * First pass under write lock to salt name cache.
     */

    /* get first dir entry in this window */
    dp = (struct dir_minwireentry*)((struct pipeDState*)dirStream.state)->bufp;
    for (i=0; i < bulk_stats->BulkStat_len; ++i) {
	/* get file name from wire formatted dir entry */
	namep = ((char *)dp) + SIZEOF_DIR_MINWIREENTRY;

	/* advance to next dir entry */
	dp = (struct dir_minwireentry *)((char *)dp + ntohs(dp->recordlen));

	/* skip directory and parent */
	if ((namep[0] == '.' && namep[1] == 0) ||
	    (namep[0] == '.' && namep[1] == '.' && namep[2] == 0)) {
	    continue;
	}

	/* skip on error */
	if (bulk_stats->BulkStat_val[i].error) {
	    icl_Trace1(cm_iclSetp, CM_TRACE_BULKSTAT_ERROR, 
		       ICL_TYPE_STRING, namep);
	    continue;
	}

	fidp = &bulk_stats->BulkStat_val[i].fid;
	fidp->Cell = scp->fid.Cell;
	fidp->Volume = scp->fid.Volume;

	/* install in name cache */
	nh_enter(scp, namep, fidp, NH_INSTALLEDBY_BULKSTAT);
    }
    lock_ReleaseWrite(&scp->llock);

    /*
     * Make second pass to salt stat cache.
     */

    /* reinit first dir entry in this window */
    dp = (struct dir_minwireentry*)((struct pipeDState*)dirStream.state)->bufp;

    for (i=0; i < bulk_stats->BulkStat_len; ++i) {
	namep = ((char *)dp) + SIZEOF_DIR_MINWIREENTRY;
	dp = (struct dir_minwireentry *)((char *)dp + ntohs(dp->recordlen));

	/* skip directory and parent */
	if ((namep[0] == '.' && namep[1] == 0) ||
	    (namep[0] == '.' && namep[1] == '.' && namep[2] == 0))
	    continue;

	/* skip on error */
	if (bulk_stats->BulkStat_val[i].error)
	    continue;

	fidp = &bulk_stats->BulkStat_val[i].fid;
	statp = &bulk_stats->BulkStat_val[i].stat;
	tokenp = &bulk_stats->BulkStat_val[i].token;

	/* come here to find scache entry */
	lock_ObtainWrite(&cm_scachelock);
	dummy_scp = cm_FindSCache(fidp);

	/* already in scache? */
	if (!dummy_scp) {
	    /* nope, so put it there */
	    dummy_scp = cm_NewSCache(fidp, scp->volp);
	    dummy_scp->installStates |=  BULKSTAT_INSTALLEDBY_BULKSTAT;
	    sc_stats.statusCacheBulkstatEntered++;
	}
	lock_ReleaseWrite(&cm_scachelock);

	/* come here to merge status and tokens */
	lock_ObtainWrite(&dummy_scp->llock);

	/* merge with other token changes */
	cm_EndPartialTokenGrant(tokenp, serverp, volp, startTime);
	cm_MergeStatus(dummy_scp, statp, tokenp, 0, rreqp);
	lock_ReleaseWrite(&dummy_scp->llock);

	cm_PutSCache(dummy_scp);
    }

    lock_ObtainWrite(&scp->llock);

    /* server sets this to 0 on EOF */
    scp->bulkstatdiroffset = NextOffset;
    lock_ReleaseWrite(&scp->llock);
    cm_TerminateTokenGrant();

norpc:
    /* free pipe buffer */
    osi_FreeBufferSpace((struct osi_buffer *)
			((struct pipeDState *)dirStream.state)->bufp);

    /* free stat buffer */
    osi_FreeBufferSpace((struct osi_buffer *)stat_buf);

#ifdef AFS_SMALLSTACK_ENV
    /* free stack-saving buffer */
    osi_FreeBufferSpace((struct osi_buffer *)lp);
#endif /* AFS_SMALLSTACK_ENV */

    if (volp)
	cm_EndVolumeRPC(volp);

    icl_Trace2(cm_iclSetp, CM_TRACE_BULKSTAT_END, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, code);

#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */

seterror:
    AFS_hzero(scp->bulkstatdiroffset);
    lock_ReleaseWrite(&scp->llock);

    code = -1;
    cm_EndTokenGrantingCall((afs_token_t *)0, (struct cm_server *)0,
			    (struct cm_volume *)0, startTime);
    goto norpc;
}


/*
 * Lookup a name in the cache, and if it ain't there, try to get it from
 * the server.  The dir entry must not be locked.
 *
 * The usual case is that the fid of the file looked up is returned in fidp
 * and the result scache pointer (*tscpp) is set to NULL.  In this case the
 * caller is expected to call cm_GetSCache to map the fid into an scache
 * entry.  If that call fails (e.g. due to ESTALE), the caller can decide
 * whether to retry.
 *
 * In some cases this function will come up with an scache pointer that is
 * known not to be stale, as a by-product of its own work.  In those cases
 * the result scache pointer (*tscpp) will be set to that pointer, and the
 * caller can just use it as is.
 */
#ifdef SGIMIPS
int 
nh_dolookup(
    struct cm_scache *dscp,
    char *namep,
    afsFid *fidp,
    struct cm_scache **tscpp,
    struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
nh_dolookup(dscp, namep, fidp, tscpp, rreqp)
    struct cm_scache *dscp;
    char *namep;
    afsFid *fidp;
    struct cm_scache **tscpp;
    struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    register long code;
    /* we're going to allocate these dynamically to save stack space.
     * Do allocation only on slow (RPC) path.
     */
    struct lclHeap {
	afsTaggedName NameBuf;
	afsFetchStatus OutFidStatus, OutDirStatus;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    afsVolSync tsync;
    register struct cm_conn *connp;
    struct cm_scache *tscp;
    struct cm_server *tserverp;
    afs_token_t ttoken;
    afsTimeval startDirChangeTime;
    struct cm_volume *volp = 0;
    int retryCount = 0;
    int blocked;
    int accessOK;
    u_long startTime;
    long irevcnt;
    long srvix;
    afs_hyper_t *vvp;
    error_status_t st;

    int bulkstatflag=1;

    *tscpp = NULL;
    icl_Trace2(cm_iclSetp, CM_TRACE_DOLOOKUP, ICL_TYPE_POINTER, dscp,
	       ICL_TYPE_STRING, namep);
redo:
    lock_ObtainRead(&dscp->llock);

    /* Get tokens for directory lookup */
    if (code = cm_GetTokens(dscp, TKN_READ, rreqp, READ_LOCK)) {
	lock_ReleaseRead(&dscp->llock);
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }

    /* Check access permissions for lookup */
    if (code = cm_GetAccessBits(dscp, dacl_perm_execute, &accessOK, &blocked,
				rreqp, READ_LOCK, &retryCount)) {
	lock_ReleaseRead(&dscp->llock);
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }
    if (blocked) {
	lock_ReleaseRead(&dscp->llock);
	goto redo;
    }
    if (!accessOK) {
	lock_ReleaseRead(&dscp->llock);
	return EACCES;
    }

    /* Check for .. in volume root */
    if ((dscp->states & SC_VOLROOT) &&
	namep[0] == '.' && namep[1] == '.' && namep[2] == 0) {
	lock_ReleaseRead(&dscp->llock);
	code = cm_GetMountPointDotDot(dscp, fidp, tscpp, rreqp);
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }

    /* Lookup in the name cache */
    code = nh_lookup(dscp, namep, fidp);

    irevcnt = dscp->dirRevokes;
    /* use startDirChangeTime for the error case, where server
     * may not have set the result parameter.
     */
    startDirChangeTime = dscp->m.ServerChangeTime;
    lock_ReleaseRead(&dscp->llock);
    if (code == 0) {
	CM_BUMP_COUNTER(nameCacheHits);
	if (fidp->Vnode)
	    	return 0;				/* found it */
	else
	    return ENOENT;			/* known not to exist! */
    }

    CM_BUMP_COUNTER(nameCacheMisses);

    /*
    Make a bulk stat RPC.
    */

    /*
    Verify that:

	1. directory is open
	2. did not already do a bulk stat in this lookup call
	3. bulk stat is enabled
    */

    if(bulkstatflag && enable_bulkstat)
    {
    	lock_ObtainRead(&dscp->llock);
    	if(dscp->opens>0)
    	{
	    lock_ReleaseRead(&dscp->llock);

    	    /*
	    Hint the name and status caches for files in this
	    directory. When returns, go back to start of lookup.
	    Either bulkstat or some other thread may have installed
	    the file in the name cache.
	    */

	    cm_BulkFetchStatus(dscp,rreqp);

	    bulkstatflag=0;

	    /* try again to find in cache */

	    goto redo;	/* recheck tokens, et cet */
    	}
    	else
    	    lock_ReleaseRead(&dscp->llock);
    }

#ifdef AFS_SMALLSTACK_ENV
    /* Keep stack frame small by doing heap-allocation */
    lp = (struct lclHeap *) osi_AllocBufferSpace();
    /* this is essentially a compile-time check, which will compile into
     * a noop or a panic.
     */
#ifdef SGIMIPS
#pragma set woff 1171
#endif
    /* CONSTCOND */
    osi_assert(sizeof(struct lclHeap) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif
#endif /* AFS_SMALLSTACK_ENV */

    /* 
     * Otherwise it is time to make an RPC 
     */
    /* unfortunately, we have no idea what fid we'll get back, so we don't
     * whether we have tokens for it yet or not.  For now, we'll just try
     * to get the token each time.
     */
    bzero((char *) &lp->NameBuf, sizeof(afsTaggedName));
    strcpy((char *) lp->NameBuf.tn_chars, namep);
    cm_StartTokenGrantingCall();
    do {
	tserverp = 0;
	if (connp = cm_Conn(&dscp->fid, MAIN_SERVICE(SRT_FX),
			    rreqp, &volp, &srvix)) {
	    if (volp && ((volp->states & VL_LAZYREP) != 0))
		vvp = &volp->latestVVSeen;
	    else
		vvp = &osi_hZero;
	    icl_Trace0(cm_iclSetp, CM_TRACE_DOLOOKUPSTART);
	    startTime = osi_Time();
	    st = BOMB_EXEC
		("COMM_FAILURE",
		 (osi_RestorePreemption(0),
		  st = AFS_Lookup(connp->connp, (afsFid *)&dscp->fid, 
				  &lp->NameBuf, vvp, AFS_FLAG_RETURNTOKEN, 
				  fidp, &lp->OutFidStatus, &lp->OutDirStatus, 
				  &ttoken, &tsync),
		  osi_PreemptionOff(),
		  st));
	    code = osi_errDecode(st);
	    tserverp = connp->serverp;
	    cm_SetReqServer(rreqp, tserverp);	/* for cm_MergeStatus */
	    icl_Trace1(cm_iclSetp, CM_TRACE_DOLOOKUPEND, ICL_TYPE_LONG, code);
	} else 
	    code = -1;
	if (code == 0) {
	    /* Loop back if we went backward in VV-time. */
	    code = cm_CheckVolSync((afsFid *)&dscp->fid, &tsync, volp, 
#ifdef SGIMIPS
				   startTime, (int)srvix, rreqp);
#else  /* SGIMIPS */
				   startTime, srvix, rreqp);
#endif /* SGIMIPS */
	}
    } while (cm_Analyze(connp, code, &dscp->fid, rreqp, volp, srvix, startTime));

    if (code) {
	lock_ObtainWrite(&dscp->llock);
	if (code == ENOENT) {
	    /* 
	     * We're told the thing doesn't exist 	
	     */
	    fidp->Vnode = 0;
	    /* UpdateStatus explains fundamental cache algorithm here.
	     * Note how this works: if dirRevokes increases, it tells us
	     * that a modification occurred to the directory *by some
	     * other client* while our call was running (with the vnode
	     * unlocked).  If startDirChangeTime changes, it tells us
	     * that some other thread on this client made an RPC that
	     * modified the directory (and potentially this name mapping).
	     * If this modification occurred before our lookup, the results
	     * of our lookup are fine.  Since AFS_Lookup doesn't change the
	     * inode change time, the startDirChangeTime should represent
	     * the changeTime at the server when our AFS_Lookup call executed.
	     * Even if our call changed the ctime, we'd just take the
	     * "invalidate" (nh_delete) branch below, which is always
	     * safe to do.
	     */
	    if (irevcnt == dscp->dirRevokes &&
		cm_tcmp(dscp->m.ServerChangeTime, startDirChangeTime) <= 0)
		nh_enter(dscp, namep, fidp, 0);
	    else
		nh_delete(dscp, namep);
	}
	lock_ReleaseWrite(&dscp->llock);
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	if (volp) 
	    cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
	osi_FreeBufferSpace((struct osi_buffer *) lp);
#endif /* AFS_SMALLSTACK_ENV */
	
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }

    /* Otherwise update the parent directory status */
    lock_ObtainWrite(&dscp->llock);
    /* and fill in rest of fid */
    fidp->Cell = dscp->fid.Cell;
    fidp->Volume = dscp->fid.Volume;
    cm_MergeStatus(dscp, &lp->OutDirStatus, (afs_token_t *) 0, 0, rreqp);
    if (irevcnt == dscp->dirRevokes &&
	cm_tcmp(dscp->m.ServerChangeTime, lp->OutDirStatus.changeTime) <= 0)
	nh_enter(dscp, namep, fidp, 0);
    else
	nh_delete(dscp, namep);
    lock_ReleaseWrite(&dscp->llock);

    /* 
     * Also, merge in the appropriate information for the file we just stat'd. 
     * We have token and status info, so this should be straightforward.  
     * However, we have to  deal with the possibility that others have already 
     * created the cache entry, as well as the possibility that we may have to 
     * do it ourselves (more likely).
     */
    lock_ObtainWrite(&cm_scachelock);	/* lock new cache entry lock */
    tscp = cm_FindSCache(fidp);
    if (!tscp)
	tscp = cm_NewSCache(fidp, dscp->volp);
    lock_ReleaseWrite(&cm_scachelock);	/* release new cache entry lock */

    /* 
     * Merge the information into the cache structure, if we got one 
     */
    lock_ObtainWrite(&tscp->llock);
    cm_EndTokenGrantingCall(&ttoken, tserverp, tscp->volp, startTime);
    tokenStat.Lookup++;
    cm_MergeStatus(tscp, &lp->OutFidStatus, &ttoken, 0, rreqp);
    lock_ReleaseWrite(&tscp->llock);
    /*
     * Check for race with deletion.
     * If we think we might have lost the race, let the caller figure out
     * for sure (by calling cm_GetSCache).
     */
    if (AFS_hgetlo(ttoken.type) & TKN_OPEN_PRESERVE)
	*tscpp = tscp;
    else
	*tscpp = NULL;

    if (*tscpp == NULL)
	cm_PutSCache(tscp);		/* release scp if not being returned */
    if (volp) 
	cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *) lp);
#endif /* AFS_SMALLSTACK_ENV */
    return 0;
}


/*
 * lookup the [dvp, namep] triplet in the hash table. Fidp
 * holds the name's Fid on success.
 */
#ifdef SGIMIPS
int
nh_lookup(
    register struct cm_scache *dvp,
    register char *namep,
    afsFid *fidp)
#else  /* SGIMIPS */
nh_lookup(dvp, namep, fidp)
    register struct cm_scache *dvp;
    register char *namep;
    afsFid *fidp;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    register int nlen = (int)strlen(namep);
#else  /* SGIMIPS */
    register int nlen = strlen(namep);
#endif /* SGIMIPS */
    register struct nh_cache *nhp;
    register int32 hash;

    if (nlen > NH_NAMELEN) {
	nh_stats.long_look++;
	return E2BIG;
    }
#ifdef SGIMIPS
    hash = (int32)NH_HASH(namep, nlen, dvp);
#else  /* SGIMIPS */
    hash = NH_HASH(namep, nlen, dvp);
#endif /* SGIMIPS */
    lock_ObtainWrite(&nh_lock);
    if (!(nhp = find(dvp, namep, nlen, hash))) {
	nh_stats.misses++;
	lock_ReleaseWrite(&nh_lock);
	return ENOENT;
    }
    nh_stats.hits++;

    /* installed by bulkstat? */

    if(nhp->states & NH_INSTALLEDBY_BULKSTAT)
    {
	nhp->states |= NH_SEEN;
	nh_stats.bulkstat_seen++;
    }

    QRemove(&nhp->lruq);
    QAdd(&nh_lru, &nhp->lruq);
    /*
     * If entry not at the head of the chain, we move it so it will be
     * found earlier if looked up again soon.
     */
    if (nhp != (struct nh_cache *) nh_cache[hash].next) {
	NH_RMHASH(nhp);
	/* reinsert at head */
	NH_INSHASH(nhp, (struct nh_cache *)&nh_cache[hash]);	
    }
    fidp->Cell = nhp->fid.Cell;
    fidp->Volume = nhp->fid.Volume;
    fidp->Vnode = nhp->fid.Vnode;
    fidp->Unique = nhp->fid.Unique;
    lock_ReleaseWrite(&nh_lock);
    return 0;
}


/*
 * Add a new entry to the directory cache
 */
#ifdef SGIMIPS
void nh_enter(
    register struct cm_scache *dvp,
    register char *namep,
    register afsFid *fidp,
    int flags)
#else  /* SGIMIPS */
void nh_enter(dvp, namep, fidp, flags)
    register struct cm_scache *dvp;
    register char *namep;
    register afsFid *fidp;
    int flags;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    register int nlen = (int)strlen(namep);
#else  /* SGIMIPS */
    register int nlen = strlen(namep);
#endif /* SGIMIPS */
    register struct nh_cache *nhp;
    register int32 hash;

    icl_Trace3(cm_iclSetp, CM_TRACE_NHENTER, ICL_TYPE_POINTER, dvp,
	       ICL_TYPE_STRING, namep, ICL_TYPE_LONG, fidp->Vnode);

    lock_ObtainWrite(&nh_lock);
    if (nlen > NH_NAMELEN) {
	nh_stats.long_enter++;
	lock_ReleaseWrite(&nh_lock);
	return;
    }
#ifdef SGIMIPS
    hash = (int32)NH_HASH(namep, nlen, dvp);
#else  /* SGIMIPS */
    hash = NH_HASH(namep, nlen, dvp);
#endif /* SGIMIPS */
    if (nhp = find(dvp, namep, nlen, hash)) {
	QRemove(&nhp->lruq);
    }
    else {
	nhp = NH_QTONH(QPrev(&nh_lru));
	QRemove(&nhp->lruq);
	if (nhp->dvp) {
	    if (nhp->fid.Vnode) nh_stats.normal--;
	    else nh_stats.missing--;

    	    /* installed by bulkstat and never hit? */

    	    if((nhp->states & NH_INSTALLEDBY_BULKSTAT) &&
            	(!(nhp->states & NH_SEEN)))
                    nh_stats.bulkstat_notseen++;
	}
	NH_RMHASH(nhp);
	if (nhp->dvp)
	    cm_PutSCache(nhp->dvp);	/* can block here on cm_scachelock */
	nhp->dvp = dvp;
	cm_HoldSCache(dvp);
	nhp->namelen = nlen;
	bcopy(namep, nhp->name, nlen);

    	/* record states flags */

    	nhp->states = flags;
    	if(flags & NH_INSTALLEDBY_BULKSTAT)
            nh_stats.bulkstat_entered++;

	NH_INSHASH(nhp, (struct nh_cache *)&nh_cache[hash]);
    }
    nhp->fid.Volume = fidp->Volume;
    nhp->fid.Cell = fidp->Cell;
    nhp->fid.Vnode = fidp->Vnode;

    /* count normal and ENOENT entries */
    if (fidp->Vnode == 0) nh_stats.missing++;
    else nh_stats.normal++;
    nhp->fid.Unique = fidp->Unique;
    QAdd(&nh_lru, &nhp->lruq);
    nh_stats.enter++;
    lock_ReleaseWrite(&nh_lock);
}


/*
 * Remove the [dvp, namep] entry from the hash table.
 */
#ifdef SGIMIPS
void nh_delete(
    register struct cm_scache *dvp,
    register char *namep)
#else  /* SGIMIPS */
void nh_delete(dvp, namep)
    register struct cm_scache *dvp;
    register char *namep;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    register int nlen = (int)strlen(namep);
#else  /* SGIMIPS */
    register int nlen = strlen(namep);
#endif /* SGIMIPS */
    register struct nh_cache *nhp;
    register int32 hash;

    icl_Trace2(cm_iclSetp, CM_TRACE_NHDELETE, ICL_TYPE_POINTER, dvp,
	       ICL_TYPE_STRING, namep);
    if (nlen > NH_NAMELEN) {
	return;
    }
    lock_ObtainWrite(&nh_lock);
#ifdef SGIMIPS
    hash = (int32)NH_HASH(namep, nlen, dvp);
#else  /* SGIMIPS */
    hash = NH_HASH(namep, nlen, dvp);
#endif /* SGIMIPS */
    while (nhp = find(dvp, namep, nlen, hash)) {
	purge(nhp);
    }
    lock_ReleaseWrite(&nh_lock);
}


/*
 * Delete all entries associated with the dvp directory from the hash table.
 */
#ifdef SGIMIPS
void nh_delete_dvp(register struct cm_scache *dvp)
#else  /* SGIMIPS */
void nh_delete_dvp(dvp)
    register struct cm_scache *dvp;
#endif /* SGIMIPS */
{
    register struct nh_cache *nhp, *nnhp;
    register struct nh_cache *tqep;
    register int i;

    icl_Trace1(cm_iclSetp, CM_TRACE_NHDELETEDVP, ICL_TYPE_POINTER, dvp);
    lock_ObtainWrite(&nh_lock);
    for (i = 0; i < NH_HASHSIZE; i++) {
	tqep = (struct nh_cache *) &nh_cache[i];
	for (nhp = tqep->next; nhp != tqep; nhp = nnhp) {
	    nnhp = nhp->next;			/* before purge trashes it */
	    if (nhp->dvp == dvp) {
		purge(nhp);
	    }
	}
    }
    lock_ReleaseWrite(&nh_lock);
}


/*
 * Garbage collect the given entry.
 *
 * Must be called with nh_lock write-locked.
 */
#ifdef SGIMIPS
static void purge(register struct nh_cache *nhp)
#else  /* SGIMIPS */
static void purge(nhp)
    register struct nh_cache *nhp;
#endif /* SGIMIPS */
{
    QRemove(&nhp->lruq);
    NH_RMHASH(nhp);
    NH_NULLHASH(nhp);
    cm_PutSCache(nhp->dvp);
    if (nhp->fid.Vnode) 
	nh_stats.normal--;
    else 
	nh_stats.missing--;

    /* installed by bulkstat and never hit? */

    if((nhp->states & NH_INSTALLEDBY_BULKSTAT) &&
        (!(nhp->states & NH_SEEN)))
            nh_stats.bulkstat_notseen++;

    nhp->dvp = (struct cm_scache *)0;
    QAddT(&nh_lru, &nhp->lruq);		/* At tail; to  be grabbed first */
}


/*
 * Search for [dvp, namep] in the hash table.
 *
 * Must be called with nh_lock write-locked.
 */
#ifdef SGIMIPS
static struct nh_cache *find(
    register struct cm_scache *dvp,
    register char *namep,
    int length,
    int32 hash)
#else  /* SGIMIPS */
static struct nh_cache *find(dvp, namep, length, hash)
    register struct cm_scache *dvp;
    register char *namep;
    int length;
    int32 hash;
#endif /* SGIMIPS */
{
    register struct nh_cache *nhp;
    register struct nh_cache *tqe;       /* really only the queue header */

    tqe = (struct nh_cache *) &nh_cache[hash];
    for (nhp = tqe->next; nhp != tqe ; nhp = nhp->next) {
	if (nhp->dvp == dvp &&
	    nhp->namelen == length && *nhp->name == *namep &&
	    bcmp(nhp->name, namep, length) == 0) {
	    return nhp;
	}
    }
    return (struct nh_cache *)0;
}
