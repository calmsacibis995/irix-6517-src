/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_dcache.c,v 65.21 1999/08/24 20:05:41 gwehrman Exp $";
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
 * $Log: cm_dcache.c,v $
 * Revision 65.21  1999/08/24 20:05:41  gwehrman
 * Modified "dfs: stored chunk without t tokens modflags f pos x len y"
 * message to include the fileset identifier.  Fix for rfe 694510.
 *
 * Revision 65.20  1999/04/27 18:06:45  ethan
 * bring stop offsets into sync between buffer and page caches
 *
 * Revision 65.19  1998/10/26 22:31:06  bobo
 * Change the error message for ENOSPC to indicate that the problem could be
 * quota.  The Sun DFS server returns ENOSPC instead of EDQUOT.
 * Fix for 639680
 *
 * Revision 65.18  1998/10/08 21:32:45  gwehrman
 * In cm_GetDCache, check asyncStatus.  If it is set, then abort this call and
 * return NULL.  Fix for bug 488883.
 *
 * Revision 65.17  1998/08/24 16:39:31  lmc
 * Use the SC_STARTED_FLUSHING_ON and SC_STARTED_FLUSHING_OFF macros to
 * set the flag.  Moved any instances that could be done inside an
 * if to avoid unnecessary locking.  All areas that set this flag will
 * now check it first.
 *
 * Revision 65.16  1998/08/14  01:09:50  lmc
 * Add setting the SC_STARTED_FLUSHING flag to prevent a thread from
 * trying to flush twice which happens when it revokes tokens while
 * flushing.  It is set around VOP_FLUSHINVAL_PAGES, VOP_FLUSH_PAGES
 * and VOP_TOSS_PAGES.
 *
 * Revision 65.15  1998/08/11  15:22:57  bdr
 * Change cm_TryToSmush() to allow the sync case to only flush pages
 * and not toss them.  This is used by calls to cm flush which really
 * just want to invalidate the cache not just toss the data away.
 *
 * Revision 65.14  1998/08/05  19:52:51  lmc
 * Move getting the scp lock so that we get it after the VOP_TOSS_PAGES.
 * This removes potential deadlock from holding both the scp and r/w
 * lock at the same time.
 *
 * Revision 65.13  1998/07/29  19:40:26  gwehrman
 * Added pragmas around osi_assert() calls inside AFS_SMALLSTACK_ENV ifdefs to
 * turn off warning/error 1171.  This is required because the assert as used
 * is essentially a compile time assert.  If it is successful, it compiles into
 * a nop.  The compiler detects this condition, and while it is normally a
 * warning, the kernel flags turn it into an error.  If the assert fails,
 * it compiles into a panic.  AFS_SMALLSTACK_ENV is being turned on to fix
 * bug 621717.
 *
 * Revision 65.12  1998/07/24 17:53:15  lmc
 * Just call cm_rwlock and cm_rwunlock.  They now check for the same
 * thread owning the lock.
 *
 * Revision 65.11  1998/07/15  14:40:16  lmc
 * Add a call to cm_SyncDCache from the ReserveBlocks code.  If a file is being
 * written and needs more blocks, it may also hold all or most of the blocks
 * in the cache file as modified and thus needing to be written to the server
 * before they can be freed.  Because we have the R/W lock, the background
 * thread is unable to proceed so we must do the writing from within this thread.
 *
 * Revision 65.10  1998/06/09  17:43:53  lmc
 * Add file read/write locking around all calls to paging routines.
 * ie. VOP_FLUSHINVAL_PAGES, VOP_FLUSH_PAGES, VOP_TOSS_PAGES
 * Only one thread/file can be in the page flushing routines at a time.
 *
 * Revision 65.9  1998/04/22  15:57:48  bdr
 * Change stack var in cm_FetchDCache from a afs_hyper_t to a long.
 * We don't need the extra bits.
 *
 * Revision 65.8  1998/04/02  19:43:23  bdr
 * Changed cm_ComputeOffsetInfo function parameters back to long *.
 * I had made them afs_hyper_t's but they don't need to be 64 bit
 * unless we support chunk sizes > 32 bits, which would be silly.
 *
 * Revision 65.7  1998/03/23  16:24:27  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.6  1998/03/19 23:47:23  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.5  1998/03/05  19:59:33  bdr
 * Modified every function declaration to be the ANSI C standard.
 *
 * Revision 65.4  1998/03/03  15:17:36  lmc
 * Added a missing semicolon.  Added a bunch of prototypes.
 *
 * Revision 65.3  1998/03/02  22:27:07  bdr
 * Initial cache manager changes.
 *
 * Revision 65.2  1997/11/06  19:57:52  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:23  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.25.1  1996/10/02  17:07:35  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:45  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */


#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/afs4int.h>
#include <dcedfs/osi_cred.h>

#include <cm.h>				/* Cm-based standard headers */
#include <cm_bkg.h>
#include <cm_dcache.h>
#include <cm_vcache.h>
#include <cm_conn.h>
#include <cm_volume.h>
#include <cm_stats.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_dcache.c,v 65.21 1999/08/24 20:05:41 gwehrman Exp $")

static void GetDownDSlot _TAKES((int));
void cm_UpdateDCacheTokenID _TAKES((struct cm_scache *,
				    struct cm_dcache *,
				    afs_token_t *,
				    afs_token_t *));
static void cm_ClearDataMod _TAKES((struct cm_scache *, struct cm_dcache *));

/* 
 * Convenient release macro for use when cm_PutDCache would cause deadlock on 
 * cm_dcachelock lock 
 */
#define	lockedPutDCache(dcp) ((dcp)->refCount--)

struct lock_data cm_dcachelock;		/* lock: alloc new disk cache ents */
int cm_GDDInProgress = 0;		/* cm_GetDownD in progress */
struct lock_data cm_getdcachelock;	/* lock: alloc new disk cache ents */
short freeDCList = DC_NULLIDX;		/* free list for disk cache entries */
long freeDCCount = 0;			/* count of elts in freeDCList */
struct cm_dcache *freeDSList = 0;	/* free list for disk slots */
long freeDSCount = 0;			/* counter of elts in freeDSList */
struct cm_dcache  *Initial_freeDSList;	/* Ptr to orig malloced free list */
struct squeue DLRU;	  		/* LRU queue for dcache entries */
short cm_dvhashTable[DC_VHASHSIZE];	/* Data cache hash table */
short cm_dchashTable[DC_CHASHSIZE];	/* Data cache hash table */
struct cm_dcache **cm_indexTable;	/* pointers to dcache entries */
long *cm_indexTimes;			/* Dcache entry Access times */
long cm_StoreRetryTime = 1200;		/* seconds to retry storing data (20 minutes) */
short *cm_indexFlags;			/* only one: is there data there? */
long cm_indexCounter=0;			/* fake time for marking index ents */
long cm_dcount = 0;
long cm_discardedChunks = 0;		/* # of chunks tossed since last GC */

#define CM_DCACHE_FREE_RETRY_INTERVAL 1000 /* ms */
#define CM_DCACHE_FREE_MAX_RETRIES 300

#ifdef SGIMIPS
extern void cm_ResetSequentialHint(struct cm_scache *);
#else  /* SGIMIPS */
extern int cm_ResetSequentialHint();
#endif /* SGIMIPS */

/* 
 * This function is called to obtain a reference to data stored in the disk
 * cache.  Passed in are an unlocked scache entry and the byte position in the
 * file desired.
 *  
 * This function is responsible for locating a chunk of data containing the 
 * desired byte and returning a reference to the disk cache entry, with its 
 * ref count incremented.  It should be noted that this 
 * function can return to the caller without the data chunk "online".
 * The caller must check the dcp->states to see if the chunk is online.
 */
#ifdef SGIMIPS
struct cm_dcache *cm_GetDCache(
    register struct cm_scache *scp,   
    afs_hyper_t byteOffset)
#else  /* SGIMIPS */
struct cm_dcache *cm_GetDCache(scp, byteOffset)
    register struct cm_scache *scp;   
    long byteOffset;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    long index;
    afs_hyper_t chunk;
#else  /* SGIMIPS */
    long index, chunk;
#endif /* SGIMIPS */
    afsFid fid;
    register struct cm_dcache *dcp;
    register long i;
#ifdef SGIMIPS

    lock_ObtainRead(&scp->llock);
#endif /* SGIMIPS */
    /* 
     * Determine the chunk number and offset within the chunk corresponding to
     * the desired byte.  Dirs are described by chunk #; cookie list has
     * the required offset info for fetching the dir.
     */
    if (cm_vType(scp) == VDIR)
	chunk = byteOffset;
    else
	chunk = cm_chunk(byteOffset);
#ifdef SGIMIPS
    if (scp->asyncStatus) {
        lock_ReleaseRead(&scp->llock);
        return NULL;
    }
    fid = scp->fid;
    lock_ReleaseRead(&scp->llock);
#else /* SGIMIPS */
    fid = scp->fid;			/* structure assignment */
#endif /* SGIMIPS */
    i = DC_CHASH(&fid, chunk);	/* Hash on [fid, chunk] */
    lock_ObtainWrite(&cm_dcachelock);
  retry:
    for (index = cm_dchashTable[i]; index != DC_NULLIDX;) {
	dcp = cm_GetDSlot(index, (struct cm_dcache *)0);
	if (!FidCmp(&dcp->f.fid, &fid) && chunk == dcp->f.chunk) {
	    lock_ReleaseWrite(&cm_dcachelock);
	    CM_BUMP_COUNTER(dataCacheHits);
	    break;		/* leaving refCount high for caller */
	}
	index = dcp->f.hcNextp;
	lockedPutDCache(dcp);
    }
    if (index == DC_NULLIDX) {
	int counter = 0;
	/* 
	 * We didn't find the entry in the hash; we'll create one.  
	 */
	CM_BUMP_COUNTER(dataCacheMisses);
	icl_Trace2(cm_iclSetp, CM_TRACE_GETDCACHEFAIL, ICL_TYPE_POINTER, scp,
		   ICL_TYPE_LONG, chunk);
	if (freeDCList == DC_NULLIDX) {
	    /* add in cm_getdcachelock, in its rightful place */
	    lock_ReleaseWrite(&cm_dcachelock);
	    
	    /* 
	     * XXX Should we really be blocking small sized cm requests
	     * behind large sized cm requests which the cm_getdcachelock
	     * does? Also cm_GetNDCaches grabs this lock even while searching 
	     * the current cache for matching dcache files which involve disk 
	     * reads and block cm_GetDCache behind those disk reads is bogus.
	     */

	    lock_ObtainWrite(&cm_getdcachelock);
	    lock_ObtainWrite(&cm_dcachelock);
	    for (;;) {
	        cm_GetDownD(4, 0);    /* we still have write cm_dcachelock */
		if (freeDCList != DC_NULLIDX) 
		    break;
		/*
		 * If we cannot get space after 5 minutes we are in trouble
		 */
		if (++counter > CM_DCACHE_FREE_MAX_RETRIES)
		   panic("getdcache");
		lock_ReleaseWrite(&cm_dcachelock);
		osi_Wait(CM_DCACHE_FREE_RETRY_INTERVAL, 0, 0);
		lock_ObtainWrite(&cm_dcachelock);
	    } /* for (;;) */
	    /* done with waiting in line lock */
	    lock_ReleaseWrite(&cm_getdcachelock);
	    /* goto retry to verify that dropping cm_dcachelock
	     * in cm_GetDownD didn't allow someone else to create the entry.
	     */
	    goto retry;
	}
	/* data mod should have been cleared when put into freelist,
	 * this is just in case.
	 */
	cm_indexFlags[freeDCList] = DC_IFENTRYMOD | DC_IFEVERUSED;
	dcp = cm_GetDSlot(freeDCList, 0);
	freeDCList = dcp->f.hvNextp;
	freeDCCount--;

	/* 
	 * Fill in the newly-allocated dcache record. 
	 */
	dcp->f.fid = fid;
	AFS_hset64(dcp->f.versionNo,-1,-1); /* invalid value */
	AFS_hzero(dcp->f.tokenID);	/* invalid value */
	dcp->f.chunk = chunk;
	if (dcp->lruq.prev == &dcp->lruq) 
	    panic("cm_GetDCache: lruq");
	/* 
	 * Now add to the two hash chains 
	 */
	dcp->f.hcNextp = cm_dchashTable[i];	
	cm_dchashTable[i] = dcp->index;
	i = DC_VHASH(&fid);
	dcp->f.hvNextp = cm_dvhashTable[i];
	cm_dvhashTable[i] = dcp->index;
	dcp->f.states = 0;
#ifdef SGIMIPS64
	dcp->f.startDirty = 0x7ffffffffffffffe;
#else  /* SGIMIPS64 */
	dcp->f.startDirty = 0x7fffffff;
#endif /* SGIMIPS64 */
	dcp->f.endDirty = 0;
	/* reference count was 0, so OK to clear DC_DFWAITING */
	dcp->states = 0;
	cm_MaybeWakeupTruncateDaemon();
	lock_ReleaseWrite(&cm_dcachelock);
    }

    icl_Trace3(cm_iclSetp, CM_TRACE_GETDCACHEWIN, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, chunk, ICL_TYPE_POINTER, dcp);
    lock_ObtainWrite(&cm_dcachelock);
    cm_indexTimes[dcp->index] = cm_indexCounter++;
    lock_ReleaseWrite(&cm_dcachelock);
    return dcp;			/* check if we're done */
}


/* this next function is called by the Solaris port, when it needs
 * to pin N chunk in a row in the cache.
 *
 * This function is called with no locks held, and holds all of the
 * chunks between first and last-1, inclusive.
 *
 * It returns an error code, which, if 0, means that all the
 * chunks are held.
 *
 * A non-zero return code means that nothing was done.  Otherwise,
 * all chunks in the range have been allocated and held.
 *
 * Compile this code out for non-Solaris platfoms.
 */
#ifdef AFS_SUNOS5_ENV
cm_GetNDCaches(scp, first, last)
  struct cm_scache *scp;
  long first, last;
{
    long i, j;		/* temp variables */
    long needed;	/* how many chunks we need */
    long code = 0;
    long index;
    afsFid fid;
    register struct cm_dcache *dcp;

    /* lock a global mutex indicating that we're trying to get one
     * or more dcache entries from the free pool.  This should provide
     * some fairness, and should give us a shot at getting a large
     * block of cache chunks, should we need them, even under a system
     * running under high load.
     */
    needed = last - first;
    fid = scp->fid;
    lock_ObtainWrite(&cm_getdcachelock);
    lock_ObtainWrite(&cm_dcachelock);

    /* cache size could conceivably change during execution */
    if (needed >= cm_cacheFiles) {
	code = EFBIG;	/* sorta */
	goto out;
    }

    for (j = 0; 
	 (freeDCCount < needed) && (j < CM_DCACHE_FREE_MAX_RETRIES);
	 j++) {
	cm_GetDownD(4, 0);	/* temp. releases cm_dcachelock */
	if (freeDCCount >= needed) 
	    break;
	lock_ReleaseWrite(&cm_dcachelock);
	lock_ReleaseWrite(&cm_getdcachelock);
	osi_Wait(CM_DCACHE_FREE_RETRY_INTERVAL, 0, 0);
	lock_ObtainWrite(&cm_getdcachelock);
	lock_ObtainWrite(&cm_dcachelock);
	/* cache size could conceivably change during execution */
	if (needed >= cm_cacheFiles) {
	    code = EFBIG;	/* sorta */
	    goto out;
	}
    }

    osi_assert(freeDCCount >= needed);

    /* we have our space, process it */
    for(i=first; i<last; i++) {
	dcp = cm_FindDCacheNL(scp, cm_chunktobase(i));
	if (!dcp) {
	    /* doesn't already exist, so we have to create.
	     * This code is essentially adapted from cm_GetDCache.
	     *
	     * Data mod should have been cleared when put into
	     * freelist, this is just in case.
	     */
	    cm_indexFlags[freeDCList] = DC_IFENTRYMOD | DC_IFEVERUSED;
	    if (freeDCList == DC_NULLIDX)
		panic("cm_GetNDCaches freeDCList empty");
	    dcp = cm_GetDSlot(freeDCList, 0);
	    freeDCList = dcp->f.hvNextp;
	    freeDCCount--;
	    
	    /* 
	     * Fill in the newly-allocated dcache record. 
	     */
	    dcp->f.fid = fid;
	    AFS_hset64(dcp->f.versionNo,-1,-1);	/* invalid value */
	    AFS_hzero(dcp->f.tokenID);	/* invalid value */
	    dcp->f.chunk = i;
	    if (dcp->lruq.prev == &dcp->lruq) 
		panic("cm_GetDCache: lruq");
	    /* 
	     * Now add to the two hash chains 
	     */
	    index = DC_CHASH(&fid, i);
	    dcp->f.hcNextp = cm_dchashTable[index];
	    cm_dchashTable[index] = dcp->index;
	    index = DC_VHASH(&fid);	/* and per-file hash */
	    dcp->f.hvNextp = cm_dvhashTable[index];
	    cm_dvhashTable[index] = dcp->index;
	    dcp->f.states = 0;
#ifdef SGIMIPS64
	    dcp->f.startDirty = 0x7ffffffffffffffe
#else  /* SGIMIPS64 */
	    dcp->f.startDirty = 0x7fffffff;
#endif /* SGIMIPS64 */
	    dcp->f.endDirty = 0;
	    /* reference count was 0, so OK to clear DC_DFWAITING */
	    dcp->states = 0;
	    /* cm_GetDSlot bumped ref count to 1 already */
	}
	/* now discard dcp, since our whole goal here is
	 * to leave all of these entries held.
	 */
    }

  out:
    cm_MaybeWakeupTruncateDaemon();
    lock_ReleaseWrite(&cm_dcachelock);
    lock_ReleaseWrite(&cm_getdcachelock);
    return code;
}
#endif /* AFS_SUNOS5_ENV */


/* Function to compute offset and length from scache, dcache and offset.
 * Must be called with at least readlock on scache entry, on dcache
 * entry and with a status/read token.
 *
 * scp		scache
 * dcp		dcache entry
 * pos		position in file
 * offsetp	returned offset within chunk
 * lenp		# of valid bytes
 */
#ifdef SGIMIPS
void cm_ComputeOffsetInfo(
  register struct cm_scache *scp,
  register struct cm_dcache *dcp,
  register afs_hyper_t pos,
  long *offsetp, long *lenp)
#else  /* SGIMIPS */
void cm_ComputeOffsetInfo(scp, dcp, pos, offsetp, lenp)
  register struct cm_scache *scp;
  register struct cm_dcache *dcp;
  register long pos;
  long *offsetp, *lenp;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    register afs_hyper_t base;
    register long csize;
#else  /* SGIMIPS */
    register long base, csize;
#endif /* SGIMIPS */
    base = cm_chunktobase(dcp->f.chunk);
    csize = cm_chunktosize(dcp->f.chunk);
    *offsetp = pos - base;
    if ((cm_vType(scp) != VDIR) && (base + csize > scp->m.Length))
	*lenp = scp->m.Length - pos;
    else
	*lenp = csize - *offsetp;
}

/* Function to fetch a chunk into the cache; waits for fetching
 * flag if necessary.
 *
 * scp		vnode being processed
 * dcp		chunk being processed
 * rreqp	request being processed
 *
 * returns 0 if succeeds, even if doesn't bring in more data
 * on this particular call.  Otherwise, returns failure code
 * that should abort caller's work.
 *
 * N.B. an important optimization is that we just create a
 * truncated entry if the file exists past EOF.  Note that
 * doing this check properly requires the caller to obtain
 * a status/read token before calling us.
 *
 * locks: write locks held on both scp and dcp, both before and after.
 * Fetching flag already set by caller (since online is set
 * and we are going to release locks during fetch, fetching flag
 * tells readers that online that part already fetched is valid).
 */
#ifdef SGIMIPS
static long cm_FetchDCache(
  register struct cm_scache *scp,
  register struct cm_dcache *dcp,
  register struct cm_rrequest *rreqp) 
#else  /* SGIMIPS */
static long cm_FetchDCache(scp, dcp, rreqp)
  register struct cm_scache *scp;
  register struct cm_dcache *dcp;
  register struct cm_rrequest *rreqp; 
#endif /* SGIMIPS */
{
    /*  
     * Note, watch for the stack overflow on other platforms 
     */
    struct lclHeap {
	afsFetchStatus OutStatus;
	afsVolSync tsync;
	afs_token_t DummyToken;
	pipe_t getDCache;
	struct pipeDState  pipeState;
	afs_hyper_t NetPosition;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
#ifdef SGIMIPS
    afs_hyper_t maxGoodLength, Position;
    long size;
#else  /* SGIMIPS */
    long maxGoodLength, Position, size;
#endif /* SGIMIPS */
    afs_hyper_t NextOffset;
    char *osiFilep;
    u_long startTime;
    register struct cm_conn *connp;
    register long code = 0;
    struct cm_volume *volp;
    long chunk;
    afs_hyper_t *vvp;
    long srvix;
#ifdef SGIMIPS
    unsigned32 st;
#else  /* SGIMIPS */
    error_status_t st;
#endif /* SGIMIPS */

    icl_Trace2(cm_iclSetp, CM_TRACE_FETCHDC, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_POINTER, dcp);
    chunk = dcp->f.chunk;

    /* official dir length may have nothing to do with # of bytes
     * returned from a dir enumeration.
     */
    if (cm_vType(scp) == VDIR)
	maxGoodLength = cm_chunktobase(chunk+1);
    else {
	maxGoodLength = scp->m.Length;	/* here we check for 0 length fetch */
	if (cm_chunktobase(chunk)>= maxGoodLength) {
	    /* No data in file to read at this position,
	     * but do know data version, so just create manually.
	     */
#ifdef AFS_IRIX_ENV_NOT_YET
            /*  Put in by Steve for a faster truncate, but never turned on yet.
*/
            cm_CFileQuickTrunc(&dcp->f.handle, 0);
#else  /* AFS_IRIX_ENV_NOT_YET */
#ifdef AFS_VFSCACHE
	    if (!(osiFilep = cm_CFileOpen(&dcp->f.handle)))
#else
		if (!(osiFilep = cm_CFileOpen(dcp->f.inode)))
#endif /*AFS_VFSCACHE */
		    panic("getdcache open0");
	    cm_CFileTruncate(osiFilep, 0);
	    cm_AdjustSize(dcp, 0);
	    cm_CFileClose(osiFilep);
#endif  /* AFS_IRIX_ENV_NOT_YET */
	    dcp->f.versionNo = scp->m.DataVersion;
	    cm_SetEntryMod(dcp);
	    icl_Trace0(cm_iclSetp, CM_TRACE_FAKE0);
	    return 0;
	}
    }

    Position = cm_chunktobase(chunk);
    size = cm_chunktosize(chunk);	/* expected max size */
    if (cm_vType(scp) == VDIR) {
	/* For dirs, we lookup the cookie offset based on the chunk # */
	code = cm_LookupCookie(scp, chunk, &Position);
	if (code || Position < 0) {
	    /* if dir changed while processing, and we've lost the
	     * cookie offset info, we're going to have to stop.
	     * Also, if know position is at past EOF, don't try
	     * to fetch more.
	     */
	    return CM_ERR_DIRSYNC;
	}
    }
    
    /* 
     * We fetch the whole file.
     */
    
#ifdef AFS_VFSCACHE
    if (!(osiFilep = cm_CFileOpen(&dcp->f.handle)))
#else
	if (!(osiFilep = cm_CFileOpen(dcp->f.inode)))
#endif /* AFS_VFSCACHE */
	    panic("getdcache open");
    
    dcp->f.states |= DC_DWRITING;
    dcp->validPos = Position;	/* last valid position in this chunk */
    if (dcp->states & DC_DFFETCHREQ) {
	dcp->states &= ~DC_DFFETCHREQ;
	osi_Wakeup(&dcp->states);
    }

#ifdef AFS_SMALLSTACK_ENV
    /* Keep stack frame small by doing heap-allocation */
    lhp = (struct lclHeap *)osi_AllocBufferSpace();
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
    volp = NULL;	
#ifdef SGIMIPS
    lhp->NetPosition = Position;
#else  /* SGIMIPS */
    AFS_hset64(lhp->NetPosition, 0, Position);
#endif /* SGIMIPS */
    
    /*
     * Set up the pipe
     */
    lhp->getDCache.alloc = cm_BufAlloc;
    lhp->getDCache.state = (char *)&lhp->pipeState;
    lhp->getDCache.push = cm_CacheFetchProc; /* used to check cm_vType == VDIR */
    ((struct pipeDState *)(lhp->getDCache.state))->dcp = dcp;
    ((struct pipeDState *)(lhp->getDCache.state))->fileP = osiFilep;
    ((struct pipeDState *)(lhp->getDCache.state))->bsize = osi_BUFFERSIZE; 
    ((struct pipeDState *)(lhp->getDCache.state))->bufp =
	osi_AllocBufferSpace(); 
    
    lock_ReleaseWrite(&scp->llock);
    lock_ReleaseWrite(&dcp->llock);

    do {
	/* (re-)init here to handle busy loop around case */
	((struct pipeDState *)(lhp->getDCache.state))->scode = 0;
	((struct pipeDState *)(lhp->getDCache.state))->offset = 0; 
	lock_ObtainWrite(&dcp->llock);
	dcp->validPos = Position;	/* last valid position in this chunk */
	lock_ReleaseWrite(&dcp->llock);

	if (connp = cm_Conn(&scp->fid, MAIN_SERVICE(SRT_FX),
			    rreqp, &volp, &srvix))  {
	    /* 
	     * Tell server the min acceptable VV 
	     */
	    if (volp && ((volp->states & VL_LAZYREP) != 0))
		vvp = &volp->latestVVSeen;
	    else
		vvp = &osi_hZero;
	    
	    
	    startTime = osi_Time();
	    if (cm_vType(scp) == VDIR)  {
		icl_Trace1(cm_iclSetp, CM_TRACE_READDIRSTART,
			   ICL_TYPE_LONG, Position);
		st = BOMB_EXEC("COMM_FAILURE",
			       (osi_RestorePreemption(0),
				st = AFS_Readdir(connp->connp,
					(afsFid *)&scp->fid, &lhp->NetPosition,
					size, vvp, 0, &NextOffset,
					&lhp->OutStatus, &lhp->DummyToken,
					&lhp->tsync, &lhp->getDCache),
				osi_PreemptionOff(),
				st));
		if (st == 0 && AFS_hcmp(NextOffset, osi_maxFileClient) >= 0) {

		    /* If the returned offset is beyond what we can handle just
                     * turn this into EOVERFLOW. */

		    st = DFS_EOVERFLOW;
		}
		code = osi_errDecode(st);
	        icl_Trace1(cm_iclSetp, CM_TRACE_READDIREND,
			   ICL_TYPE_LONG, code);
	    } else {
	        icl_Trace1(cm_iclSetp, CM_TRACE_FETCHDATASTART,
			   ICL_TYPE_LONG, Position);
		st = BOMB_EXEC("COMM_FAILURE",
			       (osi_RestorePreemption(0),
				st = AFS_FetchData(connp->connp,
					(afsFid *)&scp->fid, vvp,
					&lhp->NetPosition, size, 0,
					&lhp->OutStatus, &lhp->DummyToken,
					&lhp->tsync, &lhp->getDCache),
				osi_PreemptionOff(),
				st));
		code = osi_errDecode(st);
	        icl_Trace1(cm_iclSetp, CM_TRACE_FETCHDATAEND,
			   ICL_TYPE_LONG, code);
	    }
	    if (code == error_status_ok) {
		code = ((struct pipeDState *)(lhp->getDCache.state))->scode;
	    }
	} else {
	    code = -1;
	}
	if (code == 0) {
	    /* 
	     * Loop if this went backward in VV-time. 	
	     */
#ifdef SGIMIPS
	    code = cm_CheckVolSync(&scp->fid, &lhp->tsync, volp, startTime,
				   (int)srvix, rreqp);
#else  /* SGIMIPS */
	    code = cm_CheckVolSync(&scp->fid, &lhp->tsync, volp, startTime,
				   srvix, rreqp);
#endif /* SGIMIPS */
	}
    } while (cm_Analyze(connp, code, &scp->fid, rreqp, volp, srvix, startTime));
    
    lock_ObtainWrite(&scp->llock);
    lock_ObtainWrite(&dcp->llock);

    if (code == 0) {
	/* 
	 * ValidPos is updated by cm_CacheFetchProc, and can only be 
	 * modifed under an S or W lock, which we've locked out.
	 */
	size = dcp->validPos - Position; /* actual segment size */
	if (size < 0) 
	    size = 0;
	cm_CFileTruncate(osiFilep, size);
#ifdef CM_ENABLE_COUNTERS
	if (cm_vType(scp) != VDIR)  {
	    CM_BUMP_COUNTER_BY(dataCacheBytesReadFromWire, size);
	}
#endif /* CM_ENABLE_COUNTERS */
    }

    /*
     * Free the pipe buffer
     */
    osi_FreeBufferSpace((struct osi_buffer *)
			((struct pipeDState *)(lhp->getDCache.state))->bufp);
    
    if (scp->writers == 0) 
	dcp->f.states &= ~DC_DWRITING;
    /* 
     * Now, if code != 0, we have an error and should punt.
     * Also punt for replicas if all available VVs went backwards.
     */
    if (code || (dcp->states & DC_DFFETCHINVAL) || scp->asyncStatus) {
#ifdef AFS_SMALLSTACK_ENV
	osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	if (!code) {
	    /* make sure there's an error code of some sort */
	    code = scp->asyncStatus;
	    if (!code)
		code = ESTALE;	/* anything but 0 */
	}
	cm_CFileTruncate(osiFilep, 0);		/* discard old data */
	cm_CFileClose(osiFilep);
	cm_AdjustSize(dcp, 0);
	/* don't need to re-stabilize, since fetching flag is already
	 * on and we already stabilized stuff as needed.
	 */
	cm_InvalidateOneSegment(scp, dcp, 0);
	dcp->states &= ~DC_DFFETCHINVAL;	/* turn off */
	if (volp) 
	    cm_EndVolumeRPC(volp);
    	return code;
    }

    /* 
     * Otherwise we copy in the just-fetched info 
     */
    cm_CFileClose(osiFilep);
    /* re-adjust in case size dropped */
    cm_AdjustSize(dcp, size);
    if (cm_vType(scp) == VDIR)
	tokenStat.Readdir++;
    else
	tokenStat.FetchData++;	    

    cm_MergeStatus(scp, &lhp->OutStatus, 0, /* flags */ 0, rreqp);
    dcp->f.versionNo = lhp->OutStatus.dataVersion;
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    if (cm_vType(scp) == VDIR) {
#ifdef SGIMIPS
	if (NextOffset)
	    cm_EnterCookie(scp, chunk+1, NextOffset);
#else  /* SGIMIPS */
	if (AFS_hgetlo(NextOffset))
	    cm_EnterCookie(scp, chunk+1, AFS_hgetlo(NextOffset));
#endif /* SGIMIPS */
	else {
	    cm_EnterCookie(scp, chunk+1, -1);	/* means EOF */
	}
    }
    cm_SetEntryMod(dcp);
    lock_ObtainWrite(&cm_dcachelock);
    cm_indexFlags[dcp->index] |= DC_IFEVERUSED;
    lock_ReleaseWrite(&cm_dcachelock);
    if (volp) 
	    cm_EndVolumeRPC(volp);
    return 0;
}


/* function to bring a chunk online with token atokenp.  Returns
 * unlocked structure, so results may not be valid when locks
 * are reobtained.  The caller will typically recheck that the
 * chunk is in the desired state when done, and may call this function
 * again.
 *
 * Essentially, this function tries to make a chunk online, first, by
 * trying to just get an appropriate token, if we already have the
 * right version locally, or alternatively, by getting an appropriate
 * token and then fetching the data, if the data isn't already local.
 *
 * scp		-vnode for file.
 * dcp		-dcache entry for this chunk
 * atokenp	-token whose byte range and type describes desired op
 * areqp	-request structure
 *
 * call with no locks held; returns with no locks held
 */
#ifdef SGIMIPS
long cm_GetDOnLine(
  register struct cm_scache *scp,
  register struct cm_dcache *dcp,
  afs_token_t *atokenp,
  struct cm_rrequest *areqp)
#else  /* SGIMIPS */
long cm_GetDOnLine(scp, dcp, atokenp, areqp)
  register struct cm_scache *scp;
  register struct cm_dcache *dcp;
  afs_token_t *atokenp;
  struct cm_rrequest *areqp;
#endif /* SGIMIPS */
{
    register long code;
    afs_recordLock_t blocker;
    struct cm_tokenList *ttokenp;

    icl_Trace2(cm_iclSetp, CM_TRACE_GETDONLINE, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_POINTER, dcp);

    /* goal is to bring this chunk online for the appropriate type
     * of operation covered by token atoken.  This may only require
     * a new token, or may require new data, too.
     */
    lock_ObtainWrite(&scp->llock);

    /* now, if the chunk is already present, and has the right
     * data version, we just have to get the appropriate token
     * and try again.  If the data is online, that also means the version
     * is right (but we still might not have a sufficiently powerful 
     * token to do what we want).
     *
     * We need a token whether or not we're going to do the fetch.
     * If we do the fetch, we need to know what token we had over the
     * whole fetch, while if we don't fetch the file, we need to
     * know what candidate we have for trying to put the chunk online.
     */
    icl_Trace0(cm_iclSetp, CM_TRACE_GETDONLINEHARD);
    code = cm_GetTokensRange(scp, atokenp, areqp, 
			CM_GETTOKEN_EXACT, &blocker, WRITE_LOCK);
    if (code) {
	lock_ReleaseWrite(&scp->llock);
	return code;
    }

    /* if the data version is wrong, fetch a new chunk before trying
     * to put the chunk online.  We don't want to start a fetch while
     * someone else is fetching or storing the chunk already.
     * Also, if we end up fetching the chunk, we'll need to reserve space
     * for the whole thing, so we do that now, since otherwise we have to
     * worry about someone else playing with the fetching/storing
     * flags when we're blocked waiting for space.  If we can leave
     * our fetching flag on while waiting for space we could hold up
     * the background store daemon, which would prevent those waiting
     * for space from getting space from dirty chunks.
     */
    lock_ObtainWrite(&dcp->llock);
    for (;;) {
	if (!(dcp->f.states & DC_DONLINE) &&
	    ((dcp->states & DC_DFFETCHING) || (dcp->states & DC_DFSTORING))) {
	    dcp->states |= DC_DFWAITING;
	    lock_ReleaseWrite(&scp->llock);
	    osi_SleepW(&dcp->states, &dcp->llock);
	    lock_ObtainWrite(&scp->llock);
	    lock_ObtainWrite(&dcp->llock);
	    continue;
	}

	/* we can't fetch a chunk if there's a pending truncate here,
	 * since we might fetch data from the server that we've already
	 * discarded.  Perhaps we could optimize this better by doing
	 * the truncate locally somehow, but we'd have to make sure
	 * we understand the serialization between the fetch done by
	 * FetchDCache below, and an async storeback that truncates
	 * the file at the same time.
	 */
	if (scp->m.ModFlags & (CM_MODTRUNCPOS)) {
	    lock_ReleaseWrite(&scp->llock);
	    lock_ReleaseWrite(&dcp->llock);
	    code = cm_SyncSCache(scp);
	    if (code) return code;
	    lock_ObtainWrite(&scp->llock);
	    lock_ObtainWrite(&dcp->llock);
	    continue;
	}

	/* now make sure we have enough space in the cache to hold
	 * the chunk we may fetch below.  Keep this clause at the
	 * end of the check, so that once we get the space resv,
	 * we don't go around again.  This isn't very important, since
	 * at worst, we'd keep the cache space reserved until the dcache
	 * entry was recycled, but it is an efficiency issue.
	 */
	if (cm_ReserveBlocks(scp, dcp, cm_chunktosize(dcp->f.chunk)))
	    continue;
	break;
    }

    /* if we make it here, we have space reserved for fetching
     * the chunk (if necessary), and if the online flag is off
     * then we're not fetching or storing the file, either.
     * The file has no dirty length info.
     * Don't forget to clear the space reservation if we don't fetch
     * the chunk.
     */

    /* if we get here, the fetching/storing flags are off, and everything
     * is locked.  So, if we want, we can just set the fetching flag.
     */
    /* Turn on DC_DONLINE for read-only files, also. */
    if (!(dcp->f.states & DC_DONLINE)) {
	dcp->states |= DC_DFFETCHING;
	scp->fetchCount++;
	if  (((ttokenp = cm_FindMatchingToken(scp, atokenp))
	      && ((AFS_hgetlo(ttokenp->token.type) &
		   (TKN_STATUS_READ | TKN_DATA_READ))
		  == (TKN_STATUS_READ | TKN_DATA_READ)))) {
	    /* if we get here, we can manually set the ONLINE flag.
	     * there are really two cases: one where we have the data
	     * already (and the chunk can thus really go online immediately)
	     * and one where we're going to fetch the data (in which case
	     * the chunk can go online once fetching goes on, but has to
	     * get cleared if something goes wrong).  In this fetching
	     * subcase, we must ensure that the scache entry remains
	     * locked from the time we set online until the time we set
	     * the fetching flag, or else someone might see old data and
	     * believe it is current due to the presence of the online flag.
	     */
	    if (scp->m.Length <
		(cm_chunktobase(dcp->f.chunk)+dcp->f.chunkBytes)) {
		icl_Trace3(cm_iclSetp, CM_TRACE_GETDONL_LENGTHS,
			   ICL_TYPE_LONG, scp->m.Length,
			   ICL_TYPE_LONG, cm_chunktobase(dcp->f.chunk),
			   ICL_TYPE_LONG, dcp->f.chunkBytes);
	    }
	    dcp->f.tokenID = atokenp->tokenID;
	    dcp->f.states |= DC_DONLINE;
	    cm_SetEntryMod(dcp);
	    /* not online, so need to fetch the data if the version is wrong
	     * or if this is a dir.  Dir is special cased since dir listing
	     * representation can change during a fileset move between
	     * FS types, and we'll see a token revoke, but the DV won't
	     * change.
	     */
	    icl_Trace3(cm_iclSetp, CM_TRACE_GETDONLINEHARDER,
		       ICL_TYPE_HYPER, &scp->m.DataVersion,
		       ICL_TYPE_HYPER, &dcp->f.versionNo,
		       ICL_TYPE_LONG, cm_vType(scp));
	    if (!AFS_hsame(dcp->f.versionNo, scp->m.DataVersion) ||
		cm_vType(scp) == VDIR) {
		/* turns on fetching without releasing lock.  Returns
		 * non-zero code if anything goes wrong.
		 */
		code = cm_FetchDCache(scp, dcp, areqp);
		if (code) {
		    dcp->f.states &= ~(DC_DONLINE);
		    cm_SetEntryMod(dcp);
		}
	    }	/* if version wrong */
	}	/* if have read tokens */

	/* wakeup anyone who saw the fetching flag */
	dcp->states &= ~DC_DFFETCHING;
	scp->fetchCount--;
	if (scp->states & SC_WAITING) {
	    scp->states &= ~SC_WAITING;
	    osi_Wakeup(&scp->states);
	}
	if (dcp->states & DC_DFWAITING) {
	    dcp->states &= ~DC_DFWAITING;
	    osi_Wakeup(&dcp->states);
	}
    }		/* if not online */

    /* now, give up space reservation.  Note that haven't dropped lock
     * since we had the space reservation, except while fetching flag
     * was set.  Thus it is *our* space reservation we're yielding.
     */
    cm_AdjustSize(dcp, dcp->f.chunkBytes);

    lock_ReleaseWrite(&dcp->llock);
    lock_ReleaseWrite(&scp->llock);
    return code;
}


/* version of cm_FindDCache that assumes cm_dcachelock is already
 * write-locked.
 */
#ifdef SGIMIPS
struct cm_dcache *cm_FindDCacheNL(
    register struct cm_scache *scp,    
    afs_hyper_t byteOffset)
#else  /* SGIMIPS */
struct cm_dcache *cm_FindDCacheNL(scp, byteOffset)
    register struct cm_scache *scp;    
    long byteOffset; 
#endif /* SGIMIPS */
{
    register long i, index, chunk;
    register struct cm_dcache *dcp;

    chunk = cm_chunk(byteOffset);
    i = DC_CHASH(&scp->fid, chunk);		/* hash on [fid,chunk] */
    for (index = cm_dchashTable[i]; index != DC_NULLIDX;) {
	dcp = cm_GetDSlot(index, (struct cm_dcache *)0);
	if (!FidCmp(&dcp->f.fid, &scp->fid) && chunk == dcp->f.chunk) {
	    break;	   	/* leaving refCount high for caller */
	}
	index = dcp->f.hcNextp;
	lockedPutDCache(dcp);
    }
    if (index != DC_NULLIDX) {
	cm_indexTimes[dcp->index] = cm_indexCounter++;
	CM_BUMP_COUNTER(dataCacheHits);
	return dcp;
    } else {
	CM_BUMP_COUNTER(dataCacheMisses);
	return (struct cm_dcache *) 0;
    }
}


/*
 * Find the disk cache entry associated with the particular file's chunk
 *
 * Doesn't require any locks held.
 */
#ifdef SGIMIPS
struct cm_dcache *cm_FindDCache(
    register struct cm_scache *scp,    
    afs_hyper_t byteOffset) 
#else  /* SGIMIPS */
struct cm_dcache *cm_FindDCache(scp, byteOffset)
    register struct cm_scache *scp;
    long byteOffset; 
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    lock_ObtainWrite(&cm_dcachelock);
    dcp = cm_FindDCacheNL(scp, byteOffset);
    lock_ReleaseWrite(&cm_dcachelock);
    return dcp;
}


/* 
 * This function is to decrement the reference count on a disk cache entry 
 */
#ifdef SGIMIPS
void cm_PutDCache(register struct cm_dcache *dcp)
#else  /* SGIMIPS */
void cm_PutDCache(dcp)
    register struct cm_dcache *dcp; 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_dcachelock);
    if (dcp->refCount <= 0) 
	panic("putdcache");
    --dcp->refCount;
    lock_ReleaseWrite(&cm_dcachelock);
}


/* 
 * This function is responsible for returning a particular dcache entry, named
 * by its slot index.If the entry is already present, it is returned, otherwise
 * the contents are read from the CacheInfo file. If the caller has supplied us
 * with a pointer to an in-memory cm_dcache structure, we place the info there.
 * Otherwise, we allocate a dcache entry from the free list and use it.
 *
 * We return the address of the in-memory copy of the file record. This entry's
 * refCount field has been incremented; use cm_PutDCache() to release it.
 *   
 * This function must be called with the cm_dcachelock lock write-locked.
 */
#ifdef SGIMIPS
struct cm_dcache *cm_GetDSlot(
    register long slot,
    register struct cm_dcache *tdcp)
#else  /* SGIMIPS */
struct cm_dcache *cm_GetDSlot(slot, tdcp)
    register long slot;
    register struct cm_dcache *tdcp; 
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    register long code;

    if (lock_Check(&cm_dcachelock) != -1) 
	panic("getdslot nolock");
    if (slot < 0 || slot >= cm_cacheFiles) 
	panic("getdslot slot");
    if (dcp = cm_indexTable[slot]) {
	QRemove(&dcp->lruq);			/* move to queue head */
	QAdd(&DLRU, &dcp->lruq);
	dcp->refCount++;
	return dcp;
    }
    if (cm_diskless) panic("memcache getdslot");

    /* 
     * Otherwise we should read it in from the cache file 
     * If we weren't passed an in-memory region to place the file info, we 
     * have to allocate one. 
     */
    if (tdcp == (struct cm_dcache *)0) {
	if (!freeDSList) 
	    GetDownDSlot(5);
	dcp = freeDSList;
	freeDSList = (struct cm_dcache *) dcp->lruq.next;
	freeDSCount--;
	dcp->states = 0;
	QAdd(&DLRU, &dcp->lruq);
	if (dcp->lruq.prev == &dcp->lruq) 
	    panic("getdslot: lruq");
    } else {
	dcp = tdcp;
	dcp->f.states = 0;
    }

    /* 
     * Seek to the slot'th entry and read it in.     
     */
#ifdef SGIMIPS 
    osi_Seek(cm_cacheFilep, ((long) (sizeof(struct cm_fcache) * slot + 
	     sizeof(struct cm_fheader))));
#else  /* SGIMIPS */
    osi_Seek(cm_cacheFilep, sizeof(struct cm_fcache) * slot + 
	     sizeof(struct cm_fheader));
#endif /* SGIMIPS */
    code = osi_Read(cm_cacheFilep, (char *)(&dcp->f), sizeof(struct cm_fcache));
    if (code != sizeof(struct cm_fcache)) {
	AFS_hzero(dcp->f.fid.Cell);
	AFS_hzero(dcp->f.fid.Volume);
	dcp->f.chunk = -1;
	AFS_hset64(dcp->f.versionNo,-1,-1);
	dcp->f.hcNextp = dcp->f.hvNextp = DC_NULLIDX;
	if (!tdcp) cm_indexFlags[slot] |= DC_IFENTRYMOD;
    }
    dcp->refCount = 1;
#ifdef SGIMIPS
    dcp->index = (short) slot;
#else  /* SGIMIPS */
    dcp->index = slot;
#endif /* SGIMIPS */

    /* 
     * If we didn't read into a temporary dcache region, update the slot 
     * pointer table.  
     */
    if (tdcp == (struct cm_dcache *)0)
	cm_indexTable[slot] = dcp;
    return dcp;
}


/* 
 * This function is called to write a particular dcache entry back to its home 
 * in the CacheInfo file. It has one parameter, the dcache entry.  The 
 * reference count is not  changed.  This function must be called with the 
 * cm_dcachelock lock at least read-locked.
 */
#ifdef SGIMIPS
int cm_WriteDCache(
    register struct cm_dcache *dcp, 
    int setModTime)
#else  /* SGIMIPS */
int cm_WriteDCache(dcp, setModTime)
    int setModTime;
    register struct cm_dcache *dcp; 
#endif /* SGIMIPS */
{
    register long code;

    if (setModTime) 
	dcp->f.modTime = osi_Time();

    /* now, if we're diskless, we don't actually have to write the stuff out */
    if (cm_diskless) return 0;

    /* 
     * Seek to the right dcache slot and write in-memory image out to disk.  
     */
#ifdef SGIMIPS
    osi_Seek(cm_cacheFilep, ((long)(sizeof(struct cm_fcache) * dcp->index + 
	     sizeof(struct cm_fheader))));
#else  /* SGIMIPS */
    osi_Seek(cm_cacheFilep, sizeof(struct cm_fcache) * dcp->index + 
	     sizeof(struct cm_fheader));
#endif /* SGIMIPS */
    code = osi_Write(cm_cacheFilep, (char *)(&dcp->f), sizeof(struct cm_fcache));
    if (code != sizeof(struct cm_fcache)) 
	return EIO;
    return 0;
}


/*
 * Called by all write-on-close routines and also by cm_FlushActiveScaches
 * routine (when modChunks > 0).
 * If acredp is non-null, it is the identity of the user who is writing
 * to the file.  If unspecified, we use the identity of the last user
 * who wrote to the file.
 *
 * No locks held on call or return, but temporarily locks scache and
 * dcache entries.
 */
#ifdef SGIMIPS
int cm_SyncDCache(
   register struct cm_scache *scp,
   int fsyncFlag,
   osi_cred_t *acredp)
#else  /* SGIMIPS */
int cm_SyncDCache(
   register struct cm_scache *scp,
   int fsyncFlag,
   osi_cred_t *acredp)
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    register int code = 0;
#else  /* SGIMIPS */
    register long code = 0;
#endif /* SGIMIPS */
    struct cm_rrequest treq;
    osi_cred_t *tcredp;
#ifdef SGIMIPS
    int error = 0;
    struct vnode *vp = SCTOV(scp);
#endif /* SGIMIPS */

    icl_Trace1(cm_iclSetp, CM_TRACE_SYNCDCACHE, ICL_TYPE_POINTER, scp);

    if (acredp)
	tcredp = acredp;
    else if (!(tcredp = scp->writerCredp))
	tcredp = osi_credp;
#ifdef SGIMIPS
    /*  We can only have one thread per file in the flush/invalidate code at
	a time.  We must get the read/write lock here and release it when
	we're done.  */
    if (fsyncFlag & FSYNC_INVAL) {
        /* msync case */
        cm_rwlock(SCTOV(scp), VRWLOCK_WRITE);
	SC_STARTED_FLUSHING_ON(scp, error);
	if (!error) {
	   VOP_FLUSHINVAL_PAGES(vp, 0, SZTODFSBLKS(scp->m.Length) - 1, 
				FI_REMAPF_LOCKED);
	   SC_STARTED_FLUSHING_OFF(scp);
	}
        cm_rwunlock(SCTOV(scp), VRWLOCK_WRITE);
    } else if (fsyncFlag) {
        /* fsync case */
        cm_rwlock(SCTOV(scp), VRWLOCK_WRITE);
	SC_STARTED_FLUSHING_ON(scp, error);
	if (!error) {
	  VOP_FLUSH_PAGES(vp, 0, SZTODFSBLKS(scp->m.Length) - 1, 0, FI_NONE, error);
	  SC_STARTED_FLUSHING_OFF(scp);
	}
        cm_rwunlock(SCTOV(scp), VRWLOCK_WRITE);
    }
#endif /* SGIMIPS */

    /*
     * Update the cache from memory before writing the data back.
     * This doesn't guard against adding new pages, because it doesn't
     * mark the dcache or scache as clean.
     */
    code = cm_VMWriteWholeFile(scp, tcredp);
    if (code) return code;

    /*
     * Let's get the WRITE lock to avoid the race condition. And also, it is 
     * called by cm_FlushActiveScaches only when modChunks > 0.
     */
    lock_ObtainWrite(&scp->llock);

    if (code = scp->asyncStatus) {
       goto out;
    }
    /*
     * We have to assmue that we have the token at this point. (At least, 
     * we did have the token when modifying the file). (Note. we could do 
     * something like this: check the token and then abort the write if we 
     * lost token already).
     */
    if (scp->modChunks > 0
	|| (scp->m.ModFlags & (CM_MODTRUNCPOS | CM_MODLENGTH))) {
	/* 
	 * If we're here, we have a write locked vnode, and it has modified
	 * data.  Find out who wrote it and store it all back. 
	 */
	cm_InitReq(&treq, tcredp);
	lock_ReleaseWrite(&scp->llock);
	code = cm_StoreAllSegments(scp,
				   (fsyncFlag ? CM_STORED_FSYNC : 0),
				   &treq);
    } else if (fsyncFlag) {
	/*
	 * If doing fsync, tell the server to fsync even if we have
	 * no changes to send down.
	 */
	/* This can be STATIC since it's only ever an input parameter to an RPC. */
	static afsStoreStatus storeStatus;

	cm_InitReq(&treq, tcredp);
	storeStatus.mask = 0;
	code = cm_StoreSCache (scp, &storeStatus, 1, &treq);
	lock_ReleaseWrite(&scp->llock);
    } else
	goto out;

    if ((code = cm_CheckError(code, &treq)) == 0) {
	scp->states &= ~SC_STOREFAILED;
	return 0;
    }
    else {
	/* Tell the user if he is over quota or out of space */
#ifdef DFS_EDQUOT_MISSING
	if (code == ENOSPC)
#ifdef SGIMIPS
	    cm_printf("dfs: server disk quota or space exceeded fileset (%d,,%d)\n",
		      AFS_hgethi(scp->fid->Volume),
		      AFS_hgetlo(scp->fid->Volume)); 
#else /* SGIMIPS */
	    cm_printf("dfs: server disk quota or space exceeded\n");
#endif /* SGIMIPS */
#else
	if (code == EDQUOT)
	    cm_printf("dfs: server disk quota exceeded\n");
	else if (code == ENOSPC)
	    cm_printf("dfs: server disk quota or space exceeded\n");
#endif /* EDQUOT */

	lock_ObtainWrite(&scp->llock);
	/* 
	 * Even if store failed because of the network problem, we will 
	 * try to send it back again when the server comes up.
	 */
	if (CM_ERROR_TRANSIENT(code)) {
	    if (!(scp->states & SC_STOREFAILED)) {
		scp->storeFailTime = osi_Time();
#ifdef SGIMIPS
		scp->storeFailStatus = (unsigned char) code;
#else  /* SGIMIPS */
		scp->storeFailStatus = code;
#endif /* SGIMIPS */
		scp->states |= SC_STOREFAILED;
	    }
	    icl_Trace1(cm_iclSetp, CM_TRACE_SYNCDCACHEFAIL,
		       ICL_TYPE_POINTER, scp);
	    goto out;
	}
	cm_MarkBadSCache(scp, code);
    }

out:
    lock_ReleaseWrite(&scp->llock);
    return code;
}


/* 
 * Called with scp and request.  Find all modified chunks, and writes them
 * back.  It also updates the DataVersion # for on-line chunks, since
 * we're updating the dcache entry on disk anyway.
 *
 * This function should be called with an unlocked vnode.
 *
 * Note that this function doesn't prevent new chunks from being
 * dirtied while we're writing, but does write all chunks out
 * that were dirty at the start of the operation, since chunks
 * never change order in the dvhashTable chains.
 */
#ifdef SGIMIPS
int
cm_StoreAllSegments(
  register struct cm_scache *scp,
  int aflags,
  struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
cm_StoreAllSegments(scp, aflags, rreqp)
  register struct cm_scache *scp;
  int aflags;
  struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
#ifdef SGIMIPS
    register int code=0; 
    register long index;
#else  /* SGIMIPS */
    register long code=0, index;
#endif /* SGIMIPS */
    long hash, didAny=0;

    icl_Trace1(cm_iclSetp, CM_TRACE_STOREALLSEGS, ICL_TYPE_POINTER, scp);
    /* 
     * Store modified entries 
     */
    lock_ObtainWrite(&cm_dcachelock);
    hash = DC_VHASH(&scp->fid);
    for (index = cm_dvhashTable[hash]; index != DC_NULLIDX;) {
	dcp = cm_GetDSlot(index, 0);
	if ((cm_indexFlags[index] & DC_IFDATAMOD) &&
	    !FidCmp(&dcp->f.fid, &scp->fid)) {

	    lock_ReleaseWrite(&cm_dcachelock);
	    /* 
	     * same file, and modified, we'll store it back 
	     */
	    code = cm_StoreDCache(scp, dcp, aflags, rreqp);
	    didAny = 1;
	    lock_ObtainWrite(&cm_dcachelock);
	    if (code) {
		icl_Trace4(cm_iclSetp, CM_TRACE_STOREALL_DC,
			   ICL_TYPE_POINTER, scp,
			   ICL_TYPE_POINTER, dcp,
			   ICL_TYPE_LONG, aflags,
			   ICL_TYPE_LONG, code);
		lockedPutDCache(dcp);
		break;
	    }
	}
	index = dcp->f.hvNextp;
	lockedPutDCache(dcp);
    }
    lock_ReleaseWrite(&cm_dcachelock);

    /* 
     * Send a trivial truncation storestatus if did nothing else.
     */
    if (code == 0 && !didAny) {
	/* If the length is wrong, we still have to send some sort
	 * of request to update the length.  We'll use a storedata
	 * call, but probably can use storestatus, too.
	 */
	lock_ObtainWrite(&scp->llock);
	if (scp->m.ModFlags & (CM_MODLENGTH|CM_MODTRUNCPOS)) {
	    code = cm_StoreSCache(scp, (afsStoreStatus *) 0,
				  ((aflags & CM_STORED_FSYNC) ? 1 : 0), rreqp);
	    if (code) {
		icl_Trace3(cm_iclSetp, CM_TRACE_STOREALL_SC,
			   ICL_TYPE_POINTER, scp,
			   ICL_TYPE_LONG, aflags,
			   ICL_TYPE_LONG, code);
	    }
	}
	lock_ReleaseWrite(&scp->llock);
    }
    return code;
}


/* 
 * Called with scp write-locked; truncates the cache files appropriately.
 * Assumes that TKN_UPDATE tokens are held, too, and that cache entry has been
 * stabilized, including waiting for all chunk fetches to finish.
 * Can't drop lock, or all conditions setup by cm_TruncateFile could
 * go bad on us.
 */
#ifdef SGIMIPS
cm_TruncateAllSegments(
    register struct cm_scache *scp,
    afs_hyper_t length,
    struct cm_rrequest *rreqp) 
#else  /* SGIMIPS */
cm_TruncateAllSegments(scp, length, rreqp)
    register struct cm_scache *scp;
    long length;
    struct cm_rrequest *rreqp; 
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
#ifdef SGIMIPS
    register long code, index; 
    afs_hyper_t newSize;
#else  /* SGIMIPS */
    register long code, index, newSize;
#endif /* SGIMIPS */
    char *osiFilep;

    icl_Trace2(cm_iclSetp, CM_TRACE_TRUNCATEALLSEGS, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, length);
    /* update length and mtime, ctime */
    scp->m.ModTime.sec = osi_Time();
    scp->m.ModTime.usec = 0;
    scp->m.ChangeTime.sec = scp->m.ModTime.sec;
    scp->m.ChangeTime.usec = 0;
    scp->m.ModFlags |= CM_MODMOD | CM_MODCHANGE;
    scp->m.ModFlags &= ~CM_MODMODEXACT;	/* server time will do */

    /* want to proceed to invalidate data only if really shrinking the file */
    if (length >= scp->m.Length) {
	scp->m.Length = length;
	scp->m.ModFlags |= CM_MODLENGTH;
	return 0;	/* happy enough */
    }

    code = DC_VHASH(&scp->fid);
    lock_ObtainWrite(&cm_dcachelock);
    for (index = cm_dvhashTable[code]; index != DC_NULLIDX;) {
	dcp = cm_GetDSlot(index, 0);
	if (!FidCmp(&dcp->f.fid, &scp->fid)) {
	    /* 
	     * Same file, and modified, we will store it back.
	     */
	    lock_ReleaseWrite(&cm_dcachelock);
	    lock_ObtainWrite(&dcp->llock);
	    
#ifdef SGIMIPS
            /* Instead of asserting, just set the DC_DFWAITING and get woken up
             * once the  (DC_DFFETCHING|DC_DFSTORING) is done.
             */
            while (dcp->states & (DC_DFFETCHING|DC_DFSTORING)) {
                dcp->states |= DC_DFWAITING;
                lock_ReleaseWrite(&scp->llock);
                osi_SleepW(&dcp->states, &dcp->llock);
                lock_ObtainWrite(&scp->llock);
                lock_ObtainWrite(&dcp->llock);
                continue;
            }
#else  /* SGIMIPS */
	    /* cache should be stable */
	    osi_assert((dcp->states & (DC_DFFETCHING|DC_DFSTORING)) == 0);
#endif /* SGIMIPS */

	    if (cm_chunktobase(dcp->f.chunk) >= length) {
		cm_InvalidateOneSegment(scp, dcp, 0 /* !stabilize */);
	    }
	    newSize = length - cm_chunktobase(dcp->f.chunk);
	    if (newSize < 0) 
		newSize = 0;
	    if (newSize < dcp->f.chunkBytes) {
		if (newSize == 0)
		    cm_QuickDiscard(dcp);
		else {
#ifdef AFS_VFSCACHE
		    if (!(osiFilep = cm_CFileOpen(&dcp->f.handle)))
#else
			if (!(osiFilep = cm_CFileOpen(dcp->f.inode)))
#endif
			    panic("truncateall open");
#ifdef SGIMIPS
		    cm_CFileTruncate(osiFilep, (long)newSize);
#else  /* SGIMIPS */
		    cm_CFileTruncate(osiFilep, newSize);
#endif /* SGIMIPS */
		    cm_CFileClose(osiFilep);
		    cm_AdjustSize(dcp, newSize);
		}
	    }
	    lock_ReleaseWrite(&dcp->llock);
	    lock_ObtainWrite(&cm_dcachelock);
	}
	index = dcp->f.hvNextp;
	lockedPutDCache(dcp);
    }
    lock_ReleaseWrite(&cm_dcachelock);

    /* now we've waited for and truncated any chunks being
     * stored at the time this call started.  Next, update
     * things so that the data on the server actually gets
     * truncated, too.  Note that if we set truncPos before
     * finishing all pending stores, a later store may grow
     * the file at the server.  And remember, store-backs or
     * prefetches may be running asynchronously at any time.
     */
    scp->m.Length = length;
    scp->m.ModFlags |= CM_MODLENGTH;
    if (!(scp->m.ModFlags & CM_MODTRUNCPOS) || length < scp->truncPos) {
	scp->truncPos = length;
	scp->m.ModFlags |= CM_MODTRUNCPOS;
    }
    return 0;
}


/* called without any locks held.  Tries to discard the data
 * associated with scp.
 * Does so synchronously if sync is set, otherwise just queues the
 * data to get recycled promptly.
 */
#ifdef SGIMIPS
void cm_TryToSmush(
  register struct cm_scache *scp,
  int sync)
#else  /* SGIMIPS */
void cm_TryToSmush(scp, sync)
  register struct cm_scache *scp;
  int sync;
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    register int index, i;
#ifdef SGIMIPS
    int error=0;
#endif /* SGIMIPS */
    
    /* don't want to do anything here for a device. */
    if (scp->states & SC_SPECDEV) {
	return;
    }

#ifdef SGIMIPS
    /* Only one thread per file in the flush/invalidate page code. */
    cm_rwlock(SCTOV(scp), VRWLOCK_WRITE);
    SC_STARTED_FLUSHING_ON(scp, error);
    if (!error) {
      if (sync) {
	VOP_FLUSH_PAGES(SCTOV(scp), 0, SZTODFSBLKS(scp->m.Length) - 1, 0, 
		FI_NONE, error);
      } else {
        VOP_TOSS_PAGES(SCTOV(scp), 0, SZTODFSBLKS(scp->m.Length) - 1, FI_NONE);
      }
      SC_STARTED_FLUSHING_OFF(scp);
    }
    cm_rwunlock(SCTOV(scp), VRWLOCK_WRITE);
#endif /* SGIMIPS */
    lock_ObtainWrite(&scp->llock);


    /* reset sequential hints */

    cm_ResetSequentialHint(scp);

    /*
     * Discard any dirty pages that might come back to grow the file.
     * We don't have to explicitly block pageins because we are holding
     * the scache lock.
     */
    (void) cm_VMDiscardWholeFileNoLock(scp, sync);
    /*
     * Subtle point: we're not going to go through the trouble of
     * finding and updating the dcache entries here to turn off the
     * chunks' "have dirty pages" and "have pages" flags, but after the
     * cm_VMDiscardWholeFileNoLock call above, all of those pages are gone
     * from the whole file.  They'll *stay* gone until we drop the scache
     * lock below, when new pageins will finally be allowed.
     * *THUS*, we don't check DC_IFANYPAGES on the chunks below, since
     * the flag could be spuriously set, and we know that there really
     * aren't any pages hanging out.
     */
retry:
    i =	DC_VHASH(&scp->fid);		/* hash chain of all dce's for fid */
    lock_ObtainWrite(&cm_dcachelock);
    for (index = cm_dvhashTable[i]; index != DC_NULLIDX; index=i) {
	dcp = cm_GetDSlot(index, (struct cm_dcache *)0);
	i = dcp->f.hvNextp;		/* next pointer this hash table */
	if (!FidCmp(&dcp->f.fid, &scp->fid)) {
	    /* don't check DC_IFANYPAGES here; see comment above */
	    if (sync) {
		if ((cm_indexFlags[index] & (DC_IFFREE | DC_IFFLUSHING
					     | DC_IFDATAMOD)) == 0
		    && dcp->refCount == 1) {
		    /* FlushDCache drops dcache lock, so we have to
		     * go to retry label above, since we've lost our place.
		     */
		    icl_Trace2(cm_iclSetp, CM_TRACE_SMUSH, ICL_TYPE_POINTER, scp,
			       ICL_TYPE_POINTER, dcp);
		    cm_FlushDCache(dcp);
		    lockedPutDCache(dcp);
		    lock_ReleaseWrite(&cm_dcachelock);
		    goto retry;
		}
	    }
	    else
		cm_indexTimes[index] = 0;
	}
	lockedPutDCache(dcp);
    }
    lock_ReleaseWrite(&cm_dcachelock);
    lock_ReleaseWrite(&scp->llock);
}


/* 
 * This routine is responsible for moving at least one entry
 * from the LRU queue to the free queue.  Number is unused, but
 * this routine must move at least one entry if possible.
 *
 * This routine must be called with cm_dcachelock write-locked. It periodically
 * obtains vnode locks on arbitrary files, so it shouldn't be called(nor should
 * cm_GetDCache be called) with any vnode locks held.  Also, this function, in
 * order to obey the CM's locking hierarchy, may drop the cm_dcachelock when it
 * runs, although it will be reobtained on return.
 */
#define	MAXATONCE   16			/* max we can obtain at once */
#ifdef SGIMIPS
void cm_GetDownD(
  int number,
  int flags)
#else  /* SGIMIPS */
void cm_GetDownD(number, flags)
  int number;
  int flags;
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    register long i, j, skip;
    int	needSpace;			/* true if need space, not slots */
    int onlyDiscards;			/* if true, only GC discarded elts,
					 * i.e. those with time of 0
					 */
    int onlyGC;		     
    long vtime;
    long code;
    register struct cm_scache *scp;
#ifdef SGIMIPS
    long victims[MAXATONCE];
#else  /* SGIMIPS */
    short victims[MAXATONCE];
#endif /* SGIMIPS */
    char skipVictims[MAXATONCE];	/* entry at index i non zero if we 
					 * cannot recycle victim at index i
					 * in the victims array */
    short numSkipVictims;		/* number of victims that cannot be
					 * recycled */
    long victimTimes[MAXATONCE]; 	/* youngest (largest LRU time) first */
    long maxVictim;		 	/* youngest (largest LRU time)victim */
    short victimPtr;			/* next free item in victim arrays */

    needSpace = flags & CM_GETDOWND_NEEDSPACE; /* XXX Not used */
    onlyDiscards = flags & CM_GETDOWND_ONLYDISCARDS;
    onlyGC = flags & CM_GETDOWND_ONLYGC;

    if (lock_Check(&cm_dcachelock) != -1) 
	panic("getdownd nolock");
    icl_Trace1(cm_iclSetp, CM_TRACE_GETDOWNDSTART, ICL_TYPE_LONG, needSpace);

    /*
     * If somebody else got in ahead of us, wait until they either make
     * progress or quit, and then return.  Our caller will decide whether
     * to try again.
     */
    if (cm_GDDInProgress) {
	osi_SleepW(&cm_GDDInProgress, &cm_dcachelock);
	lock_ObtainWrite(&cm_dcachelock);
	return;
    }
    cm_GDDInProgress = 1;

    for (;;) {
#ifdef SGIMIPS
	victimPtr = numSkipVictims = 0;
	maxVictim = 0;
#else  /* SGIMIPS */
	victimPtr = maxVictim = numSkipVictims = 0;
#endif /* SGIMIPS */
	/* 
	 * Select victims from access time array, perhaps LRU queue would be
	 * better, but this runs infrequently.
	 */
	for (i = 0; i < cm_cacheFiles; i++) {
	    /* 
	     * Check to see if data not written, or if already in free list 
	     */
	    if (cm_indexFlags[i] &
		(DC_IFDATAMOD | DC_IFFREE | DC_IFFLUSHING)) 
		continue;
	    dcp = cm_indexTable[i];
	    vtime = cm_indexTimes[i];
	    if (dcp) {
		/* 
		 * Active guy, but may be necessary to use 
		 */
		if (dcp->refCount != 0) 
		    continue;	  	/* can not use this one */
	    }

	    /* if we're only GC'ing discarded chunks (those with vtime 0),
	     * then skip this guy if he doesn't fit the conditions.
	     */
	    if (onlyDiscards && vtime != 0) continue;

	    if (victimPtr < MAXATONCE) {
		/* 
		 * If there's at least one free victim slot left 
		 */
		victims[victimPtr] = i;
		victimTimes[victimPtr] = vtime;
		skipVictims[victimPtr] = 0;
		if (vtime > maxVictim) 
		    maxVictim = vtime;
		victimPtr++;
	    } else if (vtime < maxVictim) {
		/* 
		 * We're older than youngest victim, so we replace at least 
		 * one victim  find youngest (largest LRU) victim 
		 *
		 * No need to change value for this index in the skipVictims
		 * array.
		 */
		for (j = 0; j < victimPtr; j++) 
		    if (victimTimes[j] == maxVictim) 
			break;
		if (j == victimPtr) 
		    panic("getdownd local");
		victims[j] = i;
		victimTimes[j] = vtime;
		maxVictim = 0;
		for (j = 0;j < victimPtr; j++)	/* recompute maxVictim */
		    if (maxVictim < victimTimes[j])
			maxVictim = victimTimes[j];
	    }
	}	

	/*
	 * try to invalidate any VM associated with victims.  We're going to
	 * drop the big lock, so make sure we hold references
	 * to dcache entries we expect to use again.
	 */
	for(i=0; i<victimPtr; i++) {  
	    /* bump ref count, we'll surrender dcachelock */
	    dcp = cm_GetDSlot(victims[i], 0);
	}
	for(i=0; i<victimPtr; i++) {
	    j = victims[i];
	    dcp = cm_indexTable[j];
	    if (!dcp) panic("getdownd indextable");
	    if (cm_indexFlags[j] & DC_IFANYPAGES) {
		lock_ReleaseWrite(&cm_dcachelock);
		lock_ObtainWrite(&cm_scachelock);
		scp = cm_FindSCache(&dcp->f.fid);
		if (scp) {

		    lock_ReleaseWrite(&cm_scachelock);

		    if (onlyGC) {
			/*
			 * Do not try to obtain the scache lock in blocking
			 * mode else we run the risk of a deadlock (see
			 * defect 6666).
			 */
			if (lock_ObtainWriteNoBlock(&scp->llock) == 0) {
			    skipVictims[i] = 1;
			    numSkipVictims++;
			    cm_PutSCache(scp);
			    lock_ObtainWrite(&cm_dcachelock);
			    continue;
			}
		    } else 
			lock_ObtainWrite(&scp->llock);

#ifdef AFS_SUNOS5_ENV
		    /* Solaris hack: don't wait for locked pages. */
		    if (onlyGC && (scp->states & SC_PPLOCKED)) {
			skipVictims[i] = 1;
			numSkipVictims++;
			lock_ReleaseWrite(&scp->llock);
			cm_PutSCache(scp);
			lock_ObtainWrite(&cm_dcachelock);
			continue;
		    }
#endif /* AFS_SUNOS5_ENV */

		    /* block getting new pages */
		    scp->activeRevokes++;
		    lock_ReleaseWrite(&scp->llock);
		    /*
		     * We pass 0 for the "force" argument.
		     * Thus, if there is an error, we don't throw away the
		     * pages, and so we don't re-use the dcache entry.
		     *
		     * This could in principle cause the cache to become
		     * clogged with unwriteable data.  We should detect
		     * and handle this case.
		     */
		    code = cm_VMDiscardChunk(scp, dcp, 0 /* force */);
		    lock_ObtainWrite(&scp->llock);
		    lock_ObtainWrite(&cm_dcachelock);
		    /*
		     * We actually removed all pages, clean and dirty
		     *
		     * Here is where we give up if an I/O error was detected.
		     * If we had set the force argument, we wouldn't have to
		     * test for a non-zero error code.
		     */
		    if (code == 0)
			cm_indexFlags[j] &= ~(DC_IFDIRTYPAGES|DC_IFANYPAGES);
		    lock_ReleaseWrite(&cm_dcachelock);
		    cm_PutActiveRevokes(scp);
		    lock_ReleaseWrite(&scp->llock);
		    cm_PutSCache(scp);
		    lock_ObtainWrite(&cm_dcachelock);
		} else {
		    /*
		     * No vnode, so DC_IFANYPAGES is spurious (we don't
		     * sweep dcaches on vnode recycling, so we can have
		     * ANYPAGES set even when all pages are gone).  Just
		     * clear the flag.
		     *
		     * Hold scache lock to prevent vnode from being
		     * created while we're clearing DC_IFANYPAGES.
		     */
		    lock_ObtainWrite(&cm_dcachelock);
		    cm_indexFlags[j] &= ~(DC_IFDIRTYPAGES|DC_IFANYPAGES);
		    lock_ReleaseWrite(&cm_scachelock);
		}
	    }
	}
	
	/*
	 * Now really reclaim the victims
	 */
	for (i = 0; i < victimPtr; i++) {
	    j = victims[i];
	    dcp = cm_indexTable[j];
	    skip = 0;
	    if (number <= 0 || dcp->refCount != 1 ||
		(cm_indexFlags[j] &
		 (DC_IFDATAMOD | DC_IFFREE | DC_IFANYPAGES | DC_IFFLUSHING)) ||
		skipVictims[i] != 0) 
		skip = 1;
	    if (!skip){
		/* Flush the dude and reclaim.  Reclaim temporarily
		 * drops cm_dcachelock, so don't drop hold on dcache
		 * entry until after all done with the dcp entry.
		 */
		cm_FlushDCache(dcp);	/* flush it from the data cache */
		number--;			/* one fewer chunks */
	    }
	    dcp->refCount--;		/* put it back */
	    /* Really need this wakeup here */
	    osi_Wakeup(&cm_GDDInProgress);   
	}

	/* now see if we've done what we came for */
	if (number <= 0 && (freeDCList != DC_NULLIDX))
	    break;

	/* Still can not find any victim, let's quit, since we're not
	 * getting anywhere.
	 */
	if ((victimPtr == 0) || 
	    (onlyGC && (numSkipVictims >= victimPtr)))
	    break;
    }

    cm_GDDInProgress = 0;
    osi_Wakeup(&cm_GDDInProgress);
    return;
}


#define CM_MAXDISCARDEDCHUNKS	16	/* # of chunks */
#define CM_DCACHECOUNTFREEPCT	95	/* max pct of chunks in use */
#define CM_DCACHESPACEFREEPCT	90	/* max pct of space in use */
#define CM_DCACHEEXTRAPCT	5	/* extra to get when freeing */

#define TRUNC_DAEMON_TRIGGER_RECYCLE_DCACHE \
    ((cm_blocksUsed >= \
      ((CM_DCACHESPACEFREEPCT - CM_DCACHEEXTRAPCT) * cm_cacheBlocks) / 100) \
     || \
     (freeDCCount <= \
      (((100 - CM_DCACHECOUNTFREEPCT + CM_DCACHEEXTRAPCT) * cm_cacheFiles) / \
       100)))

/* called with no locks; this is the first function called
 * by the cache truncate daemon, started by the DFS init process.
 */
void cm_CacheTruncateDaemon(void)
{
    int counter;

    osi_PreemptionOff();
    cm_EnterDaemon();
    lock_ObtainWrite(&cm_dcachelock);
    for (;;) {
	/* if we're running with a disk cache and don't have a
	 * really dinky cache, then try to free some cache space.
	 */
	if (!cm_diskless &&
	    (cm_cacheBlocks<<10) > 10 * cm_chunksize(0)) {

	    /* if we get woken up, we should try to clean something
	     * out, since we might have been woken up because of
	     * lots of calls to QuickDiscard.
	     */

	    /* 
	     * Previously the process only discarded dcaches call to
	     * cm_GetDownD was being made only after a 20 calls to
	     * cm_GetDownD to process discarded and non-discarded
	     * dcaches had been made - i.e. previously the call to
	     * cm_GetDownD to process only discarded entries only at
	     * the end was useless. Instead the first call to
	     * cm_GetDownD is to process discarded entries only with
	     * the hope that if we make enough progress we do not have
	     * to recycle dcaches that are potentially could still be
	     * useful in the future. 
	     * 
	     * If the discard only call makes progress, the for
	     * loop will break in the 1st iteration itself without
	     * calling cm_GetDownD again at all.
	     */


	    /* 
	     * We do not try to tell cm_GetDownD not to block while
	     * obtaining locks on discarded dcaches, since getting locks 
	     * on discarded dcaches should be uncontested
	     */

	    cm_GetDownD(16, CM_GETDOWND_ONLYDISCARDS);

	    for(counter = 0; 
		(TRUNC_DAEMON_TRIGGER_RECYCLE_DCACHE) && (counter < 20); 
		counter++) {
		/* Tell cm_GetDownD to try not to block */
		cm_GetDownD(16, CM_GETDOWND_ONLYGC);
		if ((TRUNC_DAEMON_TRIGGER_RECYCLE_DCACHE) == 0)
		    break;
		/* Wait for 62 ms approx before retrying. */
		osi_Wait((CM_DCACHE_FREE_RETRY_INTERVAL >> 4), 0, 0);
	    }	/* while loop */
	}	/* if we should be doing anything at all */

	/* start counting again */
	cm_discardedChunks = 0;

	/* and go back to sleep, synchronizing with shutdown code */
	cm_LeaveDaemon();
	osi_SleepW((char *) cm_CacheTruncateDaemon, &cm_dcachelock);
	cm_EnterDaemon();	/* synchronize with shutdown code */
	lock_ObtainWrite(&cm_dcachelock);
    }
}


/* Check to see if we need to wakeup the truncate daemon.
 * Officially, this function should be called with the cm_dcachelock
 * held, but we don't always bother, since the penalty is low if
 * a race condition occurs.
 */
void cm_MaybeWakeupTruncateDaemon(void)
{
    if (!cm_diskless &&
	(cm_discardedChunks > CM_MAXDISCARDEDCHUNKS
	 || cm_blocksUsed > (CM_DCACHESPACEFREEPCT*cm_cacheBlocks)/100
	 || freeDCCount < ((100-CM_DCACHECOUNTFREEPCT)*cm_cacheFiles)/100))
	osi_Wakeup((char *) cm_CacheTruncateDaemon);
}


/* 
 * This routine must be called with the cm_dcachelock lock held (in write mode)
 * This function removes a dcache entry from the cache, reclaiming its space.
 *
 * This function doesn't require a lock on the dcache entry since the
 * reference count already indicates that no one has a reference, much
 * less a *locked* reference, to the dcache entry.
 *
 * We can even temporarily unlock this lock as long as we first remove
 * the entry from the requisite hash tables.  The entry won't be recycled
 * since it has a non-zero reference count.  It also won't be found
 * by anyone else since it isn't in any hash table.  We do this so that
 * when we truncate the cache file, we don't hold any locks.  That way,
 * if an OSF/1 vnode recycle occurs when opening the file, we won't
 * deadlock trying to obtain the cm_scachelock.
 */
#ifdef SGIMIPS
void cm_FlushDCache(register struct cm_dcache *dcp)
#else  /* SGIMIPS */
void cm_FlushDCache(dcp)
    register struct cm_dcache *dcp; 
#endif /* SGIMIPS */
{
    register struct cm_dcache *tdcp;
    register long i;
    register short us;
    char *osiFilep;
    int ix;
    struct cm_dcache tmpdcp;

    /* should be called with held dcache entry */
    if (dcp->refCount <= 0) panic("flushdcache refcount");

    /* 
     * We know this guy's in the LRUQ.  We'll move dude into DCQ below 
     */
    icl_Trace1(cm_iclSetp, CM_TRACE_FLUSHDC, ICL_TYPE_POINTER, dcp);

    /* we should be protected against this by caller */
    ix = dcp->index;	/* convenient to remember */

    /* IFFLUSHING keeps others out when we release the dcache lock below */
    if (cm_indexFlags[ix] & (DC_IFFREE | DC_IFFLUSHING))
	panic("FlushDCache already flushing");
    cm_indexFlags[ix] |= DC_IFFLUSHING;

    /* 
     * If this guy is in the hash table, pull him out 
     */
    if (dcp->f.fid.Vnode != 0) {
	/* 
	 * Remove entry from first hash chains 
	 */
	i = DC_CHASH(&dcp->f.fid, dcp->f.chunk);
	us = cm_dchashTable[i];
	if (us == ix) {	
	    /*
	     * It's the first dude in the list 
	     */
	    cm_dchashTable[i] = dcp->f.hcNextp;
	} else {
	    /*	
	     * Must be somewhere on the chain 
	     */
	    while (us != DC_NULLIDX) {
		/*
		 * Supply a temporary cm_dcache structure to use in looking up
		 * the next slot in case it's not already in memory- we're 
		 * here because there's a shortage of them! 
		 */
		tdcp = cm_GetDSlot(us, &tmpdcp);
		if (tdcp->f.hcNextp == ix) {
		    /* 
		     * Found item pointing at the one to delete 
		     */
		    tdcp->f.hcNextp = dcp->f.hcNextp;
		    /*
		     * If we are using a temporary cm_dcache structure, write
		     * it out.  But if we are using the real thing, mark it
		     * as dirty, saving the writing-out for later.
		     */
		    if (tdcp == cm_indexTable[us])
			cm_indexFlags[us] |= DC_IFENTRYMOD;
		    else
			cm_WriteDCache(tdcp, 1);
		    lockedPutDCache(tdcp);	/* fix refCount*/
		    break;
		}
		us = tdcp->f.hcNextp;
		lockedPutDCache(tdcp);
	    }
	    if (us == DC_NULLIDX) 
		panic("dcache hc");
	}

	/* 
	 * Remove entry from *other* hash chain 
	 */
	i = DC_VHASH(&dcp->f.fid);
	us = cm_dvhashTable[i];
	if (us == ix) {
	    /*
	     * It's the first dude in the list 
	     */
	    cm_dvhashTable[i] = dcp->f.hvNextp;
	} else {
	    /*	
	     * Must be somewhere on the chain 
	     */
	    while (us != DC_NULLIDX) {
		/* 
		 * Same as above: don't ask the slot lookup to grab an 
		 * in-memory dcache structure - we can't spare one. 
		 */
		tdcp = cm_GetDSlot(us, &tmpdcp);
		if (tdcp->f.hvNextp == ix) {
		    /* 
		     * Found item pointing at the one to delete 
		     */
		    tdcp->f.hvNextp = dcp->f.hvNextp;
		    /*
		     * If we are using a temporary cm_dcache structure, write
		     * it out.  But if we are using the real thing, mark it
		     * as dirty, saving the writing-out for later.
		     */
		    if (tdcp == cm_indexTable[us])
			cm_indexFlags[us] |= DC_IFENTRYMOD;
		    else
			cm_WriteDCache(tdcp, 1);
		    lockedPutDCache(tdcp);
		    break;
		}
		us = tdcp->f.hvNextp;
		lockedPutDCache(tdcp);
	    }
	    if (us == DC_NULLIDX) 
		panic("dcache hv");
	}
	/* 
	 * Format the entry to look like it has no associated file
	 */
	dcp->f.fid.Vnode = 0;
    }
    
    /*	
     * Free its space.  We release the cm_dcachelock for two reasons:
     *
     * 1.  Help performance, since most of the real time spent in
     * dcache flushing is spent truncate the cache files.
     *
     * 2.  Avoid public pool deadlock which would occur if a reclaim
     * of a cm vnode occurred when opening this file while holding
     * the cm_dcachelock.
     */
    lock_ReleaseWrite(&cm_dcachelock);
#ifdef AFS_IRIX_ENV_NOT_YET
    cm_CFileQuickTrunc(&dcp->f.handle, 0);
#else  /* AFS_IRIX_ENV_NOT_YET */
#ifdef AFS_VFSCACHE
    if (!(osiFilep = cm_CFileOpen(&dcp->f.handle)))
#else
    if (!(osiFilep = cm_CFileOpen(dcp->f.inode)))
#endif
	panic("flushdcache truncate");
    cm_CFileTruncate(osiFilep, 0);
    cm_CFileClose(osiFilep);
#endif /* AFS_IRIX_ENV_NOT_YET */
    lock_ObtainWrite(&cm_dcachelock);
    /* fix up size one last time */
    cm_AdjustSizeNL(dcp, 0);

    /* 
     * Finally put the entry in the free list if it is not there already
     */
    if (!(cm_indexFlags[ix] & DC_IFFREE)) {
	dcp->f.hvNextp = freeDCList;
	freeDCList = ix;
	freeDCCount++;
	cm_indexFlags[ix] |= (DC_IFFREE | DC_IFENTRYMOD);
    }
    cm_indexFlags[ix] &= ~DC_IFFLUSHING;
}

#define CM_NEWDSLOTCOUNT	16	/* # of dslots to add when out */

/*
 * Get up to number dcache entries from the free list
 */
#ifdef SGIMIPS
static void GetDownDSlot(int number) 
#else  /* SGIMIPS */
static void GetDownDSlot(number)
    int number; 
#endif /* SGIMIPS */
{
    register struct squeue *tqp, *nqp;
    register struct cm_dcache *dcp;
    register long ix;

    /* this can't be called when diskless, cause it reshuffles the inode slots
     * around.  Anyway, it shouldn't be called, since we should always have
     * enough dcache slots when running w/o a disk.  Actually, since we get
     * the inode slot from f.inode these days, it is probably ok to call it,
     * unlike in the AFS 3 version, but we still should see if we ever
     * need to.
     */
    if (cm_diskless) panic("memcache GetDownDSlot");

    if (lock_Check(&cm_dcachelock) != -1) 
	panic("getdowndslot nolock");
    /* 
     * Decrement number first for all dudes in free list 
     */
    number -= freeDSCount;
    if (number <= 0) {
	/* 
	 * enough already free 
	 */
	return;
    }
    for (tqp = DLRU.prev; tqp != &DLRU && number > 0; tqp = nqp) {
	dcp = (struct cm_dcache *)tqp;	/* q is first elt in dcache entry */
	nqp = QPrev(tqp);
	if (dcp->refCount == 0) {
	    if ((ix=dcp->index) == DC_NULLIDX) 
		panic("getdowndslot");
	    
	    /* 
	     * Pull the entry out of the lruq and put it on the free list 
	     */
	    icl_Trace1(cm_iclSetp, CM_TRACE_GETDOWNDSLOTRECYCLE,
		       ICL_TYPE_POINTER, dcp);
	    QRemove(&dcp->lruq);
	    CM_BUMP_COUNTER(dataCacheRecycles);

	    /* 
	     * Write-through if modified 
	     */
	    if (cm_indexFlags[ix] & DC_IFENTRYMOD) {
		cm_WriteDCache(dcp, 1);
		/* DC_IFENTRYMOD cleared below */
	    }

	    /* 
	     * Finally put the entry in the free list 
	     */
	    cm_indexTable[ix] = (struct cm_dcache *) 0;
	    cm_indexTimes[ix] = cm_indexCounter++;
	    cm_indexFlags[ix] &= ~(DC_IFEVERUSED | DC_IFENTRYMOD);
	    dcp->index = DC_NULLIDX;
	    dcp->lruq.next = (struct squeue *) freeDSList;
	    freeDSList = dcp;
	    freeDSCount++;
	    if (--number <= 0) break;
	}
    }

    /* now, if we get here and freeDSList is still empty, we're
     * in trouble.  Allocate more and record that we've done this.
     * The CM may require as many as one dcp entry for each thread
     * executing DFS code, up to the number of files in the cache.
     * So, we may have to allocate entries beyond the initial allocation.
     * Note that this same problem doesn't occur at the dcache (as
     * opposed to the dslot) level, since when we're allocating a new
     * dcache entry (representing a different chunk), we're at a place
     * in the locking hierarchy where it is safe to sleep waiting
     * for existing users of these entries to finish with them.  That's
     * not true for dslot entries; if we wait here for a dcp's refcount
     * to go to 0, we could easily deadlock, since cm_GetDSlot is called
     * at very low levels in the system.
     */
    if (!freeDSList) {
	if (cm_diskless) panic("dfs: diskless !freeDSList");
	dcp = (struct cm_dcache *) osi_Alloc(CM_NEWDSLOTCOUNT
					     * sizeof(struct cm_dcache));
	bzero((char *) dcp, CM_NEWDSLOTCOUNT * sizeof(struct cm_dcache));
	for (ix = 0; ix < CM_NEWDSLOTCOUNT-1; ix++)
	    dcp[ix].lruq.next = (struct squeue *) (&dcp[ix+1]);
	dcp[CM_NEWDSLOTCOUNT-1].lruq.next = (struct squeue *) 0;
	freeDSList = dcp;
	freeDSCount += CM_NEWDSLOTCOUNT;
	cm_dcount += CM_NEWDSLOTCOUNT;
	CM_BUMP_COUNTER_BY(dataCacheEntries, CM_NEWDSLOTCOUNT);
    }
}

/* 
 * what to do if we attempt a store of what looks like a out of date chunk 
 */
#define CM_IGNORE 0
#define CM_PRINT 1
#define CM_PANIC 2

#define CHECK_DATA_TOKENS_ONLY 1
int cm_CheckTokensOnStore = CM_PRINT;

/* 
 * This function is called without any locks, and returns without
 * holding any locks.
 *
 * There are two bits of interest in the aflags argument.
 * CM_STORED_ASYNC means we don't worry about
 * storing any but the truncation flags to the server.
 * CM_STORED_FSYNC means we are doing an fsync, so we expect the
 * data to be fsync'ed at the server.
 */
#ifdef SGIMIPS
int cm_StoreDCache(
    register struct cm_scache *scp,
    register struct cm_dcache *dcp, 
    int aflags,
    struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
int cm_StoreDCache(scp, dcp, aflags, rreqp)
    register struct cm_scache *scp;
    register struct cm_dcache *dcp; 
    struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    register struct cm_conn *connp;
    struct lclHeap {
	afsStoreStatus InStatus;
	afsFetchStatus OutStatus;
	afsVolSync tsync;
	pipe_t storeDCache;
	struct pipeDState pipeState;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
#ifdef SGIMIPS
    long code, srvix;
    afs_hyper_t base;
#else  /* SGIMIPS */
    long code, base, srvix;
#endif /* SGIMIPS */
    int serverFlags;
    char storeStatus, justStatusStore = 0;
    u_long startTime;
    error_status_t st;
    long xfrLength;
    long relBase;	/* base of transfer relative to chunk start */
    afs_hyper_t NetPos;
    struct cm_volume *volp = 0;

#ifdef AFS_SMALLSTACK_ENV
    /* Keep stack frame small by doing heap-allocation */
    lhp = (struct lclHeap *)osi_AllocBufferSpace();
#ifdef SGIMIPS
#pragma set woff 1171
#endif
    /* CONSTCOND */
    osi_assert(sizeof(struct lclHeap) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif
#endif /* AFS_SMALLSTACK_ENV */
    lhp->storeDCache.pull = cm_CacheStoreProc;
    lhp->storeDCache.alloc = cm_BufAlloc;
    lhp->storeDCache.state = (char *)&lhp->pipeState;
    ((struct pipeDState *)(lhp->storeDCache.state))->bsize = osi_BUFFERSIZE;
    ((struct pipeDState *)(lhp->storeDCache.state))->bufp =
			osi_AllocBufferSpace();

    icl_Trace2(cm_iclSetp, CM_TRACE_STOREFILE, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_POINTER, dcp);
    /* some tricky issues here: we can store multiple concurrent chunks
     * unless we have some modifications to the per-file status info
     * also being piggybacked.  So, we try to set the scache storing
     * flag first, but only if we're going to store some status.  Otherwise,
     * we don't set this flag, but instead only set the dcache storing
     * flag, which of course won't block out stores of other chunks.
     */
    lock_ObtainWrite(&scp->llock);
    lock_ObtainWrite(&dcp->llock);
    for (;;) {
	/* begin by setting appropriate storing flags.  We set the data
	 * chunk storing flag, and also set the status storing flag
	 * if we're doing a truncate, or if we're called with async off
	 * and there are modified status flags to store back, too.
	 * We must do this inside the while loop, since more flags can
	 * get turned on until we finally get all our STORING flags set.
	 */
	if ((scp->m.ModFlags & CM_MODTRUNCPOS) ||
	    (!(aflags & CM_STORED_ASYNC) && scp->m.ModFlags))
	    storeStatus = 1;
	else
	    storeStatus = 0;

	/* we wait for status storing to clear even if we're not
	 * going to store status ourselves, since otherwise we
	 * could do a store data that would race against an
	 * already executing truncation, in which case the truncate
	 * could kill our data.  Also, we don't start status update,
	 * including possible truncate, while others are storing.
	 * Basic rule is N stores can run concurrently, but only
	 * one can run if it is storing status, too.
	 */
	if ((scp->states & SC_STORING)
	    || (storeStatus && scp->storeCount > 0)) {
	    scp->states |= SC_WAITING;
	    lock_ReleaseWrite(&dcp->llock);
	    osi_SleepW(&scp->states, &scp->llock);
	    lock_ObtainWrite(&scp->llock);
	    lock_ObtainWrite(&dcp->llock);
	    continue;
	}
	/* otherwise, status storing is clear or irrelevant, see if we
	 * can wait for data storing.
	 */
	if (dcp->states & DC_DFSTORING) {
	    dcp->states |= DC_DFWAITING;
	    lock_ReleaseWrite(&scp->llock);
	    osi_SleepW(&dcp->states, &dcp->llock);
	    lock_ObtainWrite(&scp->llock);
	    lock_ObtainWrite(&dcp->llock);
	    continue;
	}

	/* Before starting the store, make sure that this store
	 * operation still has something to do.
	 */
	lock_ObtainRead(&cm_dcachelock);
	if (!(cm_indexFlags[dcp->index] & DC_IFDATAMOD)) {
	  if (storeStatus == 0) {
	    /* surprise!  by the time we locked everything, there was
	     * nothing left to do.
	     */
	    lock_ReleaseRead(&cm_dcachelock);
	    code = 0;
	    goto done;
	  }  else {
	    /* there is no data to be stored but we need to call StoreData
	     * to store the new status so just let the code below know
	     * that it has to set the transfer length to 0
	     */
	    justStatusStore = 1;
	  }
	}


	lock_ReleaseRead(&cm_dcachelock);

	/* here, all is clear; set the required storing flags.  After
	 * this point, we can release locks and still no one will try
	 * to make conflicting concurrent stores.
	 */
	dcp->states |= DC_DFSTORING;
	dcp->states &= ~DC_DFSTOREINVAL;
	if (storeStatus) {
	    scp->states |= SC_STORING;
	    scp->states &= ~SC_STOREINVAL;
	} else scp->storeCount++;
	break;
    }

    relBase = 0;
    /* now adjust relBase and xfrLength for dirty range */
    if (!justStatusStore && (dcp->f.startDirty < dcp->f.endDirty)) {
	/* non-trivial dirty range */
	relBase = dcp->f.startDirty;
	xfrLength = dcp->f.endDirty - relBase;
    }
    else
	xfrLength = 0;	/* no dirty bytes to write */
    base = cm_chunktobase(dcp->f.chunk) + relBase;
    lock_ReleaseWrite(&dcp->llock);
    lhp->InStatus.mask = 0;
    if (storeStatus) cm_ScanStatus(scp, &lhp->InStatus);
    lock_ReleaseWrite(&scp->llock);
    if (aflags & CM_STORED_FSYNC)
      serverFlags = AFS_FLAG_SKIPTOKEN | AFS_FLAG_SYNC;
    else
      serverFlags = AFS_FLAG_SKIPTOKEN;
    do {
	/* init this here, in case busy error causes us to loop around */
#ifdef SGIMIPS
	NetPos = base;
	((struct pipeDState *)(lhp->storeDCache.state))->offset = 
				(unsigned32) relBase;
	((struct pipeDState *)(lhp->storeDCache.state))->nbytes = 
				(unsigned32) xfrLength;
#else  /* SGIMIPS */
	AFS_hset64(NetPos, 0, base);
	((struct pipeDState *)(lhp->storeDCache.state))->offset = relBase;
	((struct pipeDState *)(lhp->storeDCache.state))->nbytes = xfrLength;
#endif /* SGIMIPS */

	if (connp = cm_Conn(&scp->fid, SEC_SERVICE(SRT_FX),
			    rreqp, &volp, &srvix)) {
	    /* 
	     * The writes done before a store back will clear setuid-ness in 
	     * cache file. 
	     */
	    if (!(((struct pipeDState *)(lhp->storeDCache.state))->fileP = 
		  
#ifdef AFS_VFSCACHE
                  cm_CFileOpen(&dcp->f.handle)
#else
		  cm_CFileOpen(dcp->f.inode)
#endif
		  )) panic("storeDCache: osi_Open");

	    /* Ensure that cm_Conn didn't trash this store. */
	    code = 0;
	    lock_ObtainWrite(&scp->llock);
	    lock_ObtainWrite(&dcp->llock);
	    if ((dcp->states & DC_DFSTOREINVAL) || scp->asyncStatus) {
		dcp->states &= ~DC_DFSTOREINVAL;
		if (!code) code = scp->asyncStatus;
		if (!code) code = ESTALE;	/* anything but 0 */
		icl_Trace4(cm_iclSetp, CM_TRACE_STOREDATACONNINVALID,
			   ICL_TYPE_FID, &scp->fid,
			   ICL_TYPE_HYPER, &NetPos,
			   ICL_TYPE_LONG, xfrLength,
			   ICL_TYPE_LONG, code);
	    }
	    if (storeStatus) {
		if (scp->states & SC_STOREINVAL) {
		    scp->states &= ~SC_STOREINVAL;
		    if (!code) code = scp->asyncStatus;
		    if (!code) code = ESTALE;	/* anything but 0 */
		    icl_Trace3(cm_iclSetp, CM_TRACE_STORESTATUSCONNINVALID,
			       ICL_TYPE_POINTER, scp,
			       ICL_TYPE_FID, &scp->fid,
			       ICL_TYPE_LONG, code);
		}
	    }
	    if ((cm_CheckTokensOnStore != CM_IGNORE) && !code) {

		/* 
		 * We are about to store the chunk. Make sure that we
		 * have all the necessary tokens.
		 */

		afs_token_t token;

		AFS_hzero(token.type);
#if CHECK_DATA_TOKENS_ONLY
		if (xfrLength > 0) {
		    AFS_hset64(token.type, 0, TKN_DATA_WRITE);
		}

#else /* CHECK_DATA_TOKENS_ONLY */
		if (storeStatus && (scp->m.ModFlags != CM_MODACCESS)) {
		  if (xfrLength == 0) {
		      AFS_hset64(token.type, 0, TKN_STATUS_WRITE);
		      AFS_hzero(token.beginRange);
		      token.endRange = osi_hMaxint64;
		  } else {
		      AFS_hset64(token.type, 0,
				 (TKN_DATA_WRITE | TKN_STATUS_WRITE));
		  }
		} else {
		    if (xfrLength > 0)
			AFS_hset64(token.type, 0, TKN_DATA_WRITE);
		}
#endif /* CHECK_DATA_TOKENS_ONLY */ 

		if (!AFS_hiszero(token.type)) {
#ifdef SGIMIPS
		    token.beginRange = base;
		    token.endRange = token.beginRange + xfrLength - 1;
#else  /* SGIMIPS */
		    AFS_hset64(token.beginRange, 0, base);
		    token.endRange = token.beginRange;
		    AFS_hadd32(token.endRange, xfrLength - 1);
#endif /* SGIMIPS */
		    if (!cm_HaveTokensRange(scp, &token, 0, NULL)) {
			long typesmissing = AFS_hgetlo(token.type);

			if (AFS_hgetlo(token.type) & TKN_STATUS_WRITE) {
			    /* check to see if we at least have data tokens */
			    if (xfrLength > 0) {
				AFS_hset64(token.type, 0, TKN_DATA_WRITE);
				if (cm_HaveTokensRange(scp, &token, 0, NULL)) {
				    typesmissing = TKN_STATUS_WRITE;
				}
			    } else
				typesmissing = TKN_STATUS_WRITE;
			}
			icl_Trace4(cm_iclSetp, CM_TRACE_STOREDATANOTOKENS,
				   ICL_TYPE_FID, &scp->fid,
				   ICL_TYPE_HYPER, &NetPos,
				   ICL_TYPE_LONG, xfrLength,
				   ICL_TYPE_LONG, scp);
			osi_assert(cm_CheckTokensOnStore != CM_PANIC);
			if (cm_CheckTokensOnStore == CM_PRINT)
			    cm_printf("dfs: stored chunk without %x tokens modflags %x  pos %d len %d\n", 
				      typesmissing, scp->m.ModFlags, 
				      AFS_hgetlo(NetPos), xfrLength); 
		    }
		    if ((!(dcp->f.states & DC_DONLINE)) && (xfrLength > 0)){
			icl_Trace4(cm_iclSetp, CM_TRACE_STOREDATANOTINLINE,
				   ICL_TYPE_FID, &scp->fid,
				   ICL_TYPE_HYPER, &NetPos,
				   ICL_TYPE_LONG, xfrLength,
				   ICL_TYPE_LONG, dcp);
			osi_assert(cm_CheckTokensOnStore != CM_PANIC);
			if (cm_CheckTokensOnStore == CM_PRINT)
			    cm_printf("dfs: stored offline chunk (off %d len %d\n",
				      AFS_hgetlo(NetPos), xfrLength); 
		    }
		}
	    }
	    lock_ReleaseWrite(&dcp->llock);
	    lock_ReleaseWrite(&scp->llock);
	    startTime = osi_Time();
	    if (!code) {
		icl_Trace0(cm_iclSetp, CM_TRACE_STOREFILESTART);
		st = BOMB_EXEC("COMM_FAILURE",
			       (osi_RestorePreemption(0),
				st = AFS_StoreData(connp->connp,
					(afsFid *)&scp->fid, &lhp->InStatus,
					&NetPos, xfrLength, &osi_hZero,
					serverFlags, &lhp->storeDCache,
					&lhp->OutStatus, &lhp->tsync),
				osi_PreemptionOff(),
				st));
		code = osi_errDecode(st);
		icl_Trace1(cm_iclSetp, CM_TRACE_STOREFILEEND, ICL_TYPE_LONG, code);
	    }
	    cm_CFileClose(((struct pipeDState *)(lhp->storeDCache.state))->fileP);
	} else 
	    code = -1;
    } while (cm_Analyze(connp, code, &scp->fid, rreqp, volp, srvix, startTime));
    /* 
     * Now copy stuff back out 
     */
    lock_ObtainWrite(&scp->llock);
    lock_ObtainWrite(&dcp->llock);
    if (code == 0) {
	CM_BUMP_COUNTER_BY(dataCacheBytesWrittenToWire, xfrLength);
	cm_MergeStatus(scp, &lhp->OutStatus, (afs_token_t *) 0,
		       (storeStatus? CM_MERGE_STORING : 0), rreqp);
	cm_ClearDataMod(scp, dcp);
	if (scp->writers == 0)
	    dcp->f.states &= ~DC_DWRITING;
    }
    cm_SetEntryMod(dcp);
    dcp->states &= ~DC_DFSTORING;
    if ((dcp->states & DC_DFSTOREINVAL) || scp->asyncStatus) {
	dcp->states &= ~DC_DFSTOREINVAL;
	if (!code) code = scp->asyncStatus;
	if (!code) code = ESTALE;	/* anything but 0 */
	icl_Trace4(cm_iclSetp, CM_TRACE_STOREDATAINVALID,
		   ICL_TYPE_FID, &scp->fid,
		   ICL_TYPE_HYPER, &NetPos,
		   ICL_TYPE_LONG, xfrLength,
		   ICL_TYPE_LONG, code);
    }
    if (dcp->states & DC_DFWAITING) {
	dcp->states &= ~DC_DFWAITING;
	osi_Wakeup(&dcp->states);
    }
    if (storeStatus) {
	scp->states &= ~SC_STORING;
	if (scp->states & SC_STOREINVAL) {
	    scp->states &= ~SC_STOREINVAL;
	    if (!code) code = scp->asyncStatus;
	    if (!code) code = ESTALE;	/* anything but 0 */
	    icl_Trace3(cm_iclSetp, CM_TRACE_STORESTATUSINVALID,
		       ICL_TYPE_POINTER, scp,
		       ICL_TYPE_FID, &scp->fid,
		       ICL_TYPE_LONG, code);
	}
    }
    else scp->storeCount--;
    if (scp->states & SC_WAITING) {
	scp->states &= ~SC_WAITING;
	osi_Wakeup(&scp->states);
    }

  done:
    osi_FreeBufferSpace((struct osi_buffer *)
	((struct pipeDState *)(lhp->storeDCache.state))->bufp);
    lock_ReleaseWrite(&dcp->llock);
    lock_ReleaseWrite(&scp->llock);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    if (volp)
	cm_EndVolumeRPC(volp);
    return cm_CheckError(code, rreqp);
}

/*
 * Called with unlocked scp vnode; determines whether we've tried hard
 * enough already to store the file back, and if so, gives up on it by
 * calling cm_InvalidateAllSegments on it, which *discards* all modified
 * chunks.
 */
#ifdef SGIMIPS
void cm_ConsiderGivingUp(register struct cm_scache *scp)
#else  /* SGIMIPS */
void cm_ConsiderGivingUp(scp)
register struct cm_scache *scp;
#endif /* SGIMIPS */
{
    int discard_data = 0;
    /*
     * Check to see if retained dirty data for a file needs to be discarded.
     * The data is discarded if the error code is time-limited and the data
     * are older than CM_STOREFAIL_HOLD_TIME, or if
     * there are a lot of modified chunks in the cache, or if there are 
     * not any free dcache entries in the freeDCList, or if there are not any
     * free scache entries in the freeSCList.  In addition the 
     * file must be inactive (no opens).
     */
    if (scp->states & SC_STOREFAILED) {
	lock_ObtainWrite(&scp->llock);
	if ((scp->states & SC_STOREFAILED) && !cm_Active(scp)) {
	    lock_ObtainRead(&cm_dcachelock);
	    if ((CM_ERROR_TIME_LIMITED(scp->storeFailStatus) &&
		 (osi_Time() - scp->storeFailTime) > cm_StoreRetryTime) ||
		(cm_cacheDirty >= cm_maxCacheDirty) || 
		(freeDCList == DC_NULLIDX) ||
		(!freeSCList)) {
		    discard_data = 1;
	    }
	    lock_ReleaseRead(&cm_dcachelock);
	    if (discard_data) {
		cm_MarkBadSCache(scp, ESTALE);
	    }
	}
	lock_ReleaseWrite(&scp->llock);
    }
}

/* 
 * Called with scp write-locked; invalidates all chunks for a file after an
 * error.
 *
 * Note that this function releases the cm_dcachelock while running, which
 * means that it guarantees to invalidate all entries that existed
 * in the cache when it is first called, but isn't guaranteed to invalidate
 * any entries created after this call is made.  This should suffice for
 * handling chunks damaged by RPC failures.
 *
 * If stable is anything but CM_INVAL_SEGS_MAKESTABLE, then the data is
 * already stabilized, and we should just proceed to invalidate it.
 *
 * A value of CM_INVAL_SEGS_MAKESTABLE for stable is *not* suitable for
 * callback code, since it uses MAKEIDLE in InvalidateOneSegment.  Other
 * values are OK for callback code.  This limitation could probably be easily
 * fixed, since the code knows how to invalidate fetching segments, anyway.
 * The parameter ``stable'' is one of:
 *	CM_INVAL_SEGS_MAKESTABLE (we must stabilize scache first)
 *	CM_INVAL_SEGS_MARKINGBAD (catch in-progress scache-stores also)
 *	CM_INVAL_SEGS_DATAONLY (don't notice in-progress scache stores)
 */
#ifdef SGIMIPS
void cm_InvalidateAllSegments(
  register struct cm_scache *scp,
  int stable)
#else  /* SGIMIPS */
void cm_InvalidateAllSegments(scp, stable)
  register struct cm_scache *scp;
  int stable;
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    register long i, index;

    /* reset sequential hints */

    cm_ResetSequentialHint(scp);

    icl_Trace1(cm_iclSetp, CM_TRACE_INVALIDATEALL, ICL_TYPE_POINTER, scp);
    i = DC_VHASH(&scp->fid);
    if (stable == CM_INVAL_SEGS_MAKESTABLE) cm_StabilizeSCache(scp);
    else if ((stable == CM_INVAL_SEGS_MARKINGBAD) &&
	     (scp->states & SC_STORING)) {
	/* Oops--something's still going on here.  Tag the error. */
	scp->states |= SC_STOREINVAL;
	icl_Trace4(cm_iclSetp, CM_TRACE_INVALIDATEALLINVALID,
		   ICL_TYPE_FID, &scp->fid,
		   ICL_TYPE_LONG, scp->states,
		   ICL_TYPE_LONG, scp->storeCount,
		   ICL_TYPE_LONG, scp->asyncStatus);
    }
    lock_ObtainWrite(&cm_dcachelock);
    for (index = cm_dvhashTable[i]; index != DC_NULLIDX;) {
	dcp = cm_GetDSlot(index, 0);
	if (!FidCmp(&dcp->f.fid, &scp->fid)) {
	    lock_ReleaseWrite(&cm_dcachelock);
	    lock_ObtainWrite(&dcp->llock);	/* obtain in legal order */
	    cm_InvalidateOneSegment(scp, dcp,
				    (stable == CM_INVAL_SEGS_MAKESTABLE));
	    lock_ReleaseWrite(&dcp->llock);
	    lock_ObtainWrite(&cm_dcachelock);
	}
	index = dcp->f.hvNextp;
	lockedPutDCache(dcp);
    }
    lock_ReleaseWrite(&cm_dcachelock);
}

/* called with write-locked scp, and write-locked dcp.  Ensures
 * that the chunk is offline and will be fetched next time
 * the data is required.  The cm_dcachelock must be unlocked!
 */
#ifdef SGIMIPS
void cm_InvalidateOneSegment(
  register struct cm_scache *scp,
  register struct cm_dcache *dcp,
  int stabilize)
#else  /* SGIMIPS */
void cm_InvalidateOneSegment(scp, dcp, stabilize)
  register struct cm_scache *scp;
  register struct cm_dcache *dcp;
  int stabilize;
#endif /* SGIMIPS */
{
    long ix;

    icl_Trace2(cm_iclSetp, CM_TRACE_INVALIDATEONESEG, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_POINTER, dcp);
    if (stabilize) cm_StabilizeDCache(scp, dcp, CM_DSTAB_MAKEIDLE);
    ix = dcp->index;
    /* if stabilize is clear, we're called when chunks may be still be
     * in FETCHING mode.  We can't wait for them, since we might be called
     * from callback code, but we can turn off
     * the ONLINE flag (so people won't use them during the fetch,
     * and we can set FETCHINVAL, so that the chunk will not be
     * put on line after the fetch.
     */
    /* We might well also be called when chunks are still in STORING mode,
     * similarly.  Set STOREINVAL if that occurs, so that the caller can unwind
     * correctly.
     */
    dcp->f.states &= ~DC_DONLINE;
    if (dcp->states & (DC_DFFETCHING | DC_DFSTORING)) {
	icl_Trace4(cm_iclSetp, CM_TRACE_INVALIDATEONEINVALID,
		   ICL_TYPE_POINTER, scp,
		   ICL_TYPE_FID, &scp->fid,
		   ICL_TYPE_LONG, dcp->states,
		   ICL_TYPE_LONG, scp->asyncStatus);
	if (dcp->states & DC_DFFETCHING)
	    dcp->states |= DC_DFFETCHINVAL;
	if (dcp->states & DC_DFSTORING)
	    dcp->states |= DC_DFSTOREINVAL;
    }
    /* mark the chunk as clean */
    cm_ClearDataMod(scp, dcp);
    AFS_hzero(dcp->f.tokenID);
    AFS_hset64(dcp->f.versionNo,-1,-1);
    lock_ObtainWrite(&cm_dcachelock);
    cm_indexFlags[ix] |= DC_IFENTRYMOD;
    lock_ReleaseWrite(&cm_dcachelock);
}

/* turn on a dcache entry's DC_IFENTRYMOD flag.
 * dcp		pointer to dcache entry
 * locks cm_dcachelock
 */
#ifdef SGIMIPS
void cm_SetEntryMod(register struct cm_dcache *adp)
#else  /* SGIMIPS */
void cm_SetEntryMod(adp)
  register struct cm_dcache *adp;
#endif /* SGIMIPS */
{
    register int ix;

    lock_ObtainWrite(&cm_dcachelock);
    ix = adp->index;
    if (!cm_indexTable[ix]) panic("setentrymod");
    cm_indexFlags[ix] |= DC_IFENTRYMOD;
    lock_ReleaseWrite(&cm_dcachelock);
}

/* mark a range of a chunk as dirty, passed in dcache entry,
 * first byte modified, and last byte modified+1.
 * dcache entry must be write-locked.
 *
 * The entry mod flag must be set by the caller before the
 * dcp entry's reference count goes to 0.
 */
#ifdef SGIMIPS
void cm_SetChunkDirtyRange(
  register struct cm_dcache *dcp,
  long startPos, 
  long endPos)
#else  /* SGIMIPS */
void cm_SetChunkDirtyRange(dcp, startPos, endPos)
  register struct cm_dcache *dcp;
  long startPos, endPos;
#endif /* SGIMIPS */
{
    icl_Trace3(cm_iclSetp, CM_TRACE_SETCHUNKRANGE_1,
	       ICL_TYPE_POINTER, dcp,
	       ICL_TYPE_LONG, dcp->f.startDirty,
	       ICL_TYPE_LONG, dcp->f.endDirty);
    if (startPos < dcp->f.startDirty) dcp->f.startDirty = startPos;
    if (endPos > dcp->f.endDirty) dcp->f.endDirty = endPos;
    icl_Trace4(cm_iclSetp, CM_TRACE_SETCHUNKRANGE_2,
	       ICL_TYPE_LONG, startPos,
	       ICL_TYPE_POINTER, endPos,
	       ICL_TYPE_LONG, dcp->f.startDirty,
	       ICL_TYPE_LONG, dcp->f.endDirty);
}

/* turn on a dcache entry's DC_IFDATAMOD flag,
 * update the vnode's modChunks counter, and update
 * cm_cacheDirty.
 *
 * scp and dcp describe chunk being processed
 * function obtains cm_dcachelock
 * function must be called with asp and adp write-locked
 */
#ifdef SGIMIPS
void cm_SetDataMod(
  register struct cm_scache *asp,
  register struct cm_dcache *adp,
  osi_cred_t *acredp)
#else  /* SGIMIPS */
void cm_SetDataMod(asp, adp, acredp)
  register struct cm_scache *asp;
  register struct cm_dcache *adp;
  osi_cred_t *acredp;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_dcachelock);

    /* check to see if we're done */
    if (!(cm_indexFlags[adp->index] & DC_IFDATAMOD)) {
	cm_indexFlags[adp->index] |= DC_IFDATAMOD;
	asp->modChunks++;
	cm_cacheDirty++;
    }
    lock_ReleaseWrite(&cm_dcachelock);
}

/* same function as above, only clear the data mod
 * flag for this chunk and adjust pointers.
 */
#ifdef SGIMIPS
static void cm_ClearDataMod(
  register struct cm_scache *asp,
  register struct cm_dcache *adp)
#else  /* SGIMIPS */
static void cm_ClearDataMod(asp, adp)
  register struct cm_scache *asp;
  register struct cm_dcache *adp;
#endif /* SGIMIPS */
{
    register long ix;

    lock_ObtainWrite(&cm_dcachelock);

    /* check to see if we're done */
    ix = adp->index;
    if (cm_indexFlags[ix] & DC_IFDATAMOD) {
	cm_indexFlags[ix] &= ~DC_IFDATAMOD;
	asp->modChunks--;
	cm_cacheDirty--;
    }

#ifdef SGIMIPS64
    adp->f.startDirty = 0x7ffffffffffffffe;
#else  /* SGIMIPS64 */
    adp->f.startDirty = 0x7fffffff;
#endif /* SGIMIPS64 */
    adp->f.endDirty = 0;
    cm_indexFlags[ix] |= DC_IFENTRYMOD;

    lock_ReleaseWrite(&cm_dcachelock);
}


/* 
 * Clears DC_DONLINE state for all cache files for the scp.  
 * This is called when an vnode is recycled.
 * There should not be any chunks with modyfied data at this point.
 * Also any VMM operations should be performed prior to calling this interface.
 * The cm_scachelock should be locked and the scp reference count 
 * should be zero. Since there are no opens on the file there should
 * be no activity on any of the dcache entries for the file.
 */
#ifdef SGIMIPS
void cm_ClearOnlineState(struct cm_scache *scp)
#else  /* SGIMIPS */
void cm_ClearOnlineState(scp)
    struct cm_scache *scp;
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    register long index;

    lock_ObtainWrite(&cm_dcachelock);
    for (index = cm_dvhashTable[DC_VHASH(&scp->fid)]; index != DC_NULLIDX;) {
	dcp = cm_GetDSlot(index, 0);
	if (!FidCmp(&dcp->f.fid, &scp->fid)) {
	    /* if the chunk is online, we know *scp has been stat'd,
	     * and that we had a token until we started the flush.
	     * Thus, we know that the data version from the scache
	     * entry describes the data version for the chunk.  By
	     * setting it here, we don't have to do the update every time
	     * we store a chunk back to the server (the online flag will
	     * handle reuses until we lose the online flag).
	     */
	    icl_Trace2(cm_iclSetp, CM_TRACE_CLEAROLS, ICL_TYPE_POINTER, scp,
		       ICL_TYPE_POINTER, dcp);
	    if ((dcp->f.states & DC_DONLINE) || !AFS_hiszero(dcp->f.tokenID)) {
		if (dcp->f.states & DC_DONLINE) {
		    dcp->f.versionNo = scp->m.DataVersion;
		    dcp->f.states &= ~DC_DONLINE;
	        }
		AFS_hzero(dcp->f.tokenID);
		cm_indexFlags[index] |= DC_IFENTRYMOD;
	    }
	}
	index = dcp->f.hvNextp;
	lockedPutDCache(dcp);
    }
    lock_ReleaseWrite(&cm_dcachelock);
}


/*
 * Update dcache entry(s) online state and and tokenID information for a file.
 *    otknp is the token that is being removed from the files token list.
 *    If ntknp is non null then it is a token that may be used inplace
 *    of the token being removed(otknp).
 *
 * The caller must have at least a read lock on the scp->llock.  
 * This function temporarily write locks the dcache entries and of course, the
 * cm_dcachelock itself.
 */
void
cm_UpdateDCacheOnLineState(
  struct cm_scache *scp,
  afs_token_t *otknp,
  afs_token_t *ntknp)
{
    struct cm_dcache *dcp;
#ifdef SGIMIPS
    long index;
    afs_hyper_t filePos, beginPos, endPos;
#else  /* SGIMIPS */
    long filePos, index;
    long beginPos, endPos;
#endif /* SGIMIPS */
    int byPos;

    byPos = 0;				/* walk dvhashTable by default */

    if (otknp) {
#ifdef SGIMIPS
	if (AFS_hcmp(otknp->beginRange, osi_hMaxint64) >= 0)
	    return;			/* we're totally off the hook  */

	beginPos = otknp->beginRange;
	endPos = otknp->endRange;
	if ((endPos - beginPos) / cm_chunksize(beginPos) < 10)
	    byPos = 1;			/* token encompasses few chunks */
    }
#else  /* SGIMIPS */
	if (AFS_hcmp(otknp->beginRange, osi_hMaxint32) >= 0)
	    return;			/* we're totally off the hook  */

	beginPos = AFS_hgetlo(otknp->beginRange);
	endPos = ((AFS_hcmp(otknp->endRange, osi_hMaxint32) >= 0)
		  ? 0x7fffffff : AFS_hgetlo(otknp->endRange));
	if ((endPos - beginPos) / cm_chunksize(beginPos) < 10)
	    byPos = 1;			/* token encompasses few chunks */
    }
#endif /* SGIMIPS */

    if (!byPos) {
	lock_ObtainWrite(&cm_dcachelock);
	for (index = cm_dvhashTable[DC_VHASH(&scp->fid)]; 
	    index != DC_NULLIDX;) {
	    dcp = cm_GetDSlot(index, 0);
	    /* Don't follow a null otknp! */
	    if (!FidCmp(&dcp->f.fid, &scp->fid) && 
		((otknp == (afs_token_t *)0) ||
#ifdef SGIMIPS
		 (( otknp->beginRange <= cm_chunktobase(dcp->f.chunk+1) - 1) &&
		  ( otknp->endRange >= cm_chunktobase(dcp->f.chunk))))) {
#else  /* SGIMIPS */
		 ((AFS_hcmp64(otknp->beginRange, 0,
			     cm_chunktobase(dcp->f.chunk+1) - 1) <= 0) &&
		  (AFS_hcmp64(otknp->endRange, 0,
			      cm_chunktobase(dcp->f.chunk)) >= 0)))) {
#endif /* SGIMIPS */
		lock_ReleaseWrite(&cm_dcachelock);
		lock_ObtainWrite(&dcp->llock);
		cm_UpdateDCacheTokenID(scp, dcp, otknp, ntknp);
		lock_ReleaseWrite(&dcp->llock);
		lock_ObtainWrite(&cm_dcachelock);
	    }
	    index = dcp->f.hvNextp;
	    lockedPutDCache(dcp);
	} /* for */
	lock_ReleaseWrite(&cm_dcachelock);
    } else {
	for (filePos = beginPos;
	     filePos <= endPos;
	     filePos += cm_chunksize(filePos)) {
	    if (dcp = cm_FindDCache(scp, filePos)) {
		lock_ObtainWrite(&dcp->llock);
		cm_UpdateDCacheTokenID(scp, dcp, otknp, ntknp);
		lock_ReleaseWrite(&dcp->llock);
		cm_PutDCache(dcp);
	    }
	} /* for */
    }
}

/* 
 * Update TokenID field in a dcache entry.  If a new token is provided
 * to replace the oldToken then the replacement will be done for 
 * entries containing the old token ID.  If the entry does not contain
 * the old token ID then the tokenID field will be zeroed.  If a new
 * tokenID is not provided than an attempt will be made to supply a new
 * tokenID if one is held which will allow the dcache entry to stay online.
 * If a suitable ID is not found, then the tokenID is zeroed.
 * The dcp->llock should be write locked and the scp->llock should be at
 * least read locked by the caller
 */
#ifdef SGIMIPS
void cm_UpdateDCacheTokenID(
    struct cm_scache *scp,
    struct cm_dcache *dcp,
    afs_token_t *oldTokenp,
    afs_token_t *newTokenp)
#else  /* SGIMIPS */
void cm_UpdateDCacheTokenID(scp, dcp, oldTokenp, newTokenp)
    struct cm_scache *scp;
    struct cm_dcache *dcp;
    afs_token_t *oldTokenp;
    afs_token_t *newTokenp;
#endif /* SGIMIPS */
{
    afs_token_t Token;
    int reallyOK;
#ifdef SGIMIPS
    afs_hyper_t beginrange;
#endif /* SGIMIPS */

    icl_Trace2(cm_iclSetp, CM_TRACE_UPDATETID, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_POINTER, dcp);
    if ((cm_vType(scp) != VDIR)) {
	if (newTokenp) {
	    if (oldTokenp &&
		(AFS_hcmp(dcp->f.tokenID, oldTokenp->tokenID) == 0))
		dcp->f.tokenID = newTokenp->tokenID;
	} else {
	    AFS_hset64(Token.type, 0, TKN_DATA_READ);
#ifdef SGIMIPS
	    beginrange = cm_chunktobase(dcp->f.chunk);
	    Token.beginRange = beginrange;
	    Token.endRange = beginrange + cm_chunktosize(dcp->f.chunk) - 1;
#else  /* SGIMIPS */
	    AFS_hset64(Token.beginRange, 0, cm_chunktobase(dcp->f.chunk));
	    Token.endRange = Token.beginRange;
	    AFS_hadd32(Token.endRange, cm_chunktosize(dcp->f.chunk) - 1);
#endif /* SGIMIPS */
	    if (cm_HaveTokensRange(scp, &Token, 1/*exact match*/, &reallyOK))
	        dcp->f.tokenID = Token.tokenID;
	    else if (reallyOK)
	        dcp->f.tokenID = Token.tokenID;
	    else
	        AFS_hzero(dcp->f.tokenID);
	}
    } else {
	AFS_hzero(dcp->f.tokenID);
    }

    cm_SetEntryMod(dcp);
    if (AFS_hiszero(dcp->f.tokenID))
	dcp->f.states &= ~DC_DONLINE;

    return;
}


/* takes parameter as blocks desired, returns safely rounded
 * up version of cache blocks to use.  Units are 1024 byte cache
 * blocks.
 * Used by init code and cache size changing pioctl.
 */
#ifdef SGIMIPS
long cm_GetSafeCacheSize(long newSize)
#else  /* SGIMIPS */
long cm_GetSafeCacheSize(newSize)
  long newSize;
#endif /* SGIMIPS */
{
    register long minSize;

    /*
     * Make sure that there is at least two free chunk blocks worth of
     * space left in the cache.
     */
    minSize = ((cm_firstcsize >> 10) << 1);
    if (newSize < minSize)
	newSize = minSize;
    return newSize;
}
