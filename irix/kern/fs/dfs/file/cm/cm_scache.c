/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_scache.c,v 65.33 1999/04/27 18:06:45 ethan Exp $";
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
 * $Log: cm_scache.c,v $
 * Revision 65.33  1999/04/27 18:06:45  ethan
 * bring stop offsets into sync between buffer and page caches
 *
 * Revision 65.32  1999/04/23 16:19:14  gwehrman
 * Moved the code that clears the SC_MEMORY_MAPPED flag from the scp and
 * returns the open tokens so that all pages are flushed first.  Fix for
 * bug 667360.  Combined two if statements that determined whether or not
 * to flush pages into a single if statement.  Added an assert in the if
 * statment to verify that all pages are flushed.
 *
 * Revision 65.31  1998/09/23 14:33:37  bdr
 * Added code to FlushSCache to deal with the new SC_MEMORY_MAPPED flag
 * set in the scp.  We must give back any tokens that the server has
 * asked for just as we do in cm_close().  This change is part of the
 * mod that fixes PV 636089.
 *
 * Revision 65.30  1998/09/21  22:18:33  bdr
 * Fix call to cm_bkgQueue() in FlushSCache to pass in a credp.  This
 * fixes a panic that was caused by cm_bkgQueue() trying to do a
 * crhold on a null pointer.  This really really fixes the SC_RETURNREF
 * code path.
 *
 * Revision 65.29  1998/09/21  19:32:31  bdr
 * Fix if check in FlushSCache to do a logical and (&&) instead of a bitwise
 * and (&).  This should really fix SC_RETURNREF cases.  This also prevents
 * you from hitting the "scp not completely flushed" panic in FlushSCache
 * in a DEBUG kernel.
 *
 * Revision 65.28  1998/08/24  18:47:15  bdr
 * Fixed the SC_RETURNREF code in FlushSCache so that we will
 * actually queue the background request to give back the token.
 *
 * Revision 65.27  1998/08/24  16:39:31  lmc
 * Use the SC_STARTED_FLUSHING_ON and SC_STARTED_FLUSHING_OFF macros to
 * set the flag.  Moved any instances that could be done inside an
 * if to avoid unnecessary locking.  All areas that set this flag will
 * now check it first.
 *
 * Revision 65.26  1998/08/14  01:09:50  lmc
 * Add setting the SC_STARTED_FLUSHING flag to prevent a thread from
 * trying to flush twice which happens when it revokes tokens while
 * flushing.  It is set around VOP_FLUSHINVAL_PAGES, VOP_FLUSH_PAGES
 * and VOP_TOSS_PAGES.
 *
 * Revision 65.25  1998/08/04  14:22:32  lmc
 * Move where we release the scp to after we re-obtain the
 * global scache lock.  We depend on this scp not changing
 * between the time we give up and re-obtain the global lock
 * and the only way to guarantee that is to keep a ref count
 * on the scp.  It has to stay in the scp chain so that we can
 * use the prev pointer to continue looking at scps.
 *
 * Revision 65.24  1998/07/29  19:40:28  gwehrman
 * Added pragmas around osi_assert() calls inside AFS_SMALLSTACK_ENV ifdefs to
 * turn off warning/error 1171.  This is required because the assert as used
 * is essentially a compile time assert.  If it is successful, it compiles into
 * a nop.  The compiler detects this condition, and while it is normally a
 * warning, the kernel flags turn it into an error.  If the assert fails,
 * it compiles into a panic.  AFS_SMALLSTACK_ENV is being turned on to fix
 * bug 621717.
 *
 * Revision 65.23  1998/07/29 15:01:13  lmc
 * Delete the leftover line that prevents the queueing of
 * background threads from ever getting queued.  These threads
 * turn off the SC_RETURNREF flag.
 *
 * Revision 65.22  1998/07/24  20:16:52  bdr
 * Fix shutdown panic in cm_IdleSCache.  We were calling FlushSCache
 * which zero's out the vnode pointer in the scp.  After the flush
 * we were trying to set the vnode count to zero.  We don't need
 * to do that after we have zero'ed out the vnode.
 * This is a fix for PV 622039
 *
 * Revision 65.21  1998/07/24  17:36:24  lmc
 * Added code in cm_flushSCache to start the background thread for
 * returning tokens.  We can't do it in cm_inactive because its too
 * late.  SCPs weren't getting freed because the SC_RETURNREF
 * flag was never turned off.
 *
 * Revision 65.20  1998/07/08  14:19:30  bdr
 * Add the shuttingDown flag in a if that checks to see if things have gone
 * wrong in cm_FlushSCache.  Otherwise with a debug kernel we panic in a
 * shutdown when we really don't want to.
 *
 * Revision 65.19  1998/06/19  19:43:25  lmc
 * Release the cm_scachelock before doing the VOP_FLUSHINVAL_PAGES in
 * FlushSCache so that if we get stuck trying to get disk cache blocks
 * (cm_ReserveBlocks) other threads can run.  If a server was down and
 * the disk cache blocks were filled with modified entries for that server,
 * the token revocation code would get stuck waiting for the cm_scachelock.
 *
 * Revision 65.17  1998/06/09  17:43:53  lmc
 * Add file read/write locking around all calls to paging routines.
 * ie. VOP_FLUSHINVAL_PAGES, VOP_FLUSH_PAGES, VOP_TOSS_PAGES
 * Only one thread/file can be in the page flushing routines at a time.
 * Also added initialization and deletion of the read/write lock.  This
 * should have been done all along but wasn't.
 *
 * Revision 65.16  1998/06/04  20:14:09  n8631
 * FlushSCache can be recursively called when flushing pages from an scp. This
 * usually occurs by indirect means such as a pdflush invocation. Unlock the
 * cm_scachelock before entering into vop_flushinval_pages, re-obtain the lock
 * and then return EBUSY to the FlushSCache caller.
 *
 * Revision 65.15  1998/05/28  19:35:57  lmc
 * The second VOP_FLUSHINVAL_PAGES in FlushSCache was necessary for
 * invalidating pages that weren't memory mapped.  This puts it back
 * into FlushSCache.
 *
 * This also adds some debugging code.  Both of these panic's are
 * potentially valid cases.  Somehow a vnode has gotten through this
 * code on typhoon and still had pages attached to the vnode.  This
 * may trap it so we can fix it.
 *
 * Revision 65.14  1998/05/21  21:54:10  bdr
 * Modify cm_IdleSCaches to work for shutdown.  We were not flushing dirty
 * pages on our scp's when we did a scheduled shutdown.  Modified the code to
 * really try to flush out scp's that are on the SLRU.  This was broken
 * in the osf code as it would get stuck on the first busy VDIR and never
 * continue down the SLRU list.
 *
 * Pulled code from cm_FindSCache and cm_ResetAccessCache that was
 * checking to see if the VINACT flag was set.  We should no longer
 * need to do this with the reworking of the vnode counts and getting
 * rid of the two passes through cm_inactive.
 *
 * Pulled out duplicate page flushing from FlushSCache as well as
 * fixed an ASSERT to bu under the non shuttingDown flag.
 *
 * Revision 65.13  1998/05/20  19:02:19  lmc
 * We were dropping scp/vnode pointers while there were still reference counts on
 * the vnode.  This happened when we were in FlushSCache for an scp that was mmapped
 * and had dirty pages.  When VOP_FLUSHINVAL_PAGES was called, background requests
 * were queued and the vnode count was bumped.  When returning, we treated it as if
 * the vnode count was still 1, zero'd the scp->vp and took this scp out of the hash
 * chain.  Later, the background threads would come in and execute a request and
 * decrement the vnode count for a vnode that no longer belonged to that scp.
 * The fix is to check for dirty pages, flush them, and return EBUSY.  Also, after
 * VOP_FLUSHINVAL_PAGES is called recheck the assumptions in the vnode and scp.  There
 * may still be a window where we check everything, release the vnode lock....
 * someone else gets the lock and dirties a page....and then we do the CM_RELE and zero
 * the vnode pointer.
 *
 * Revision 65.12  1998/05/18  19:43:45  bdr
 * Pull cm_reclaim code out and make this function a no-op.  We never
 * call it anyway as it is not even in our cm_xops vector.  All of
 * the work that would usually get done in a VOP_RECLAIM will have
 * already been done by cm_inactive.  We never use vn_get either
 * so there is no point in keeping things around once the scp has
 * been put back on the freelist.  We always use vn_alloc when
 * filling in a new scache entry.
 *
 * Revision 65.11  1998/05/11  19:06:29  lmc
 * Put parenthesis around half of an if to ensure precedence is as
 * we expect.  Added the flushing of pages BEFORE we call vn_rele().
 * We were getting into cm_inactive with dirty pages on the vnode.
 * We shouldn't have dirty pages on a vnode with a count of 0.
 *
 * Revision 65.10  1998/05/08  18:43:15  bdr
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
 * Revision 65.9  1998/05/07  20:57:30  lmc
 * Moved a VN_UNLOCK so that if the shuttingDown flag is set
 * we won't try to unlock a vnode twice.
 *
 * Revision 65.8  1998/04/23  18:57:03  gwehrman
 * In FlushSCache(), moved the block of code to release the scache with
 * CM_RELE() in front of the code that removes the state information from
 * the scache.  The CM_RELE() after the state information has been
 * cleared resulted ultimately in an error in cm_GetTokensRange when
 * testing for a valid volume pointer.  This is the fix for bug 582557.
 *
 * Revision 65.7  1998/04/01 14:54:04  lmc
 * Added allocation of the vnode, as we point to the vnode instead
 * of including its structure inside the scp.
 * Added behavior initialization for the scp/vnode.
 *
 * Revision 65.6  1998/03/23  16:24:31  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.5  1998/03/19 23:47:24  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.4  1998/03/05  17:44:55  bdr
 * Modified every function declaration to be the ANSI C standard.
 *
 * Revision 65.3  1998/03/02  22:27:28  bdr
 * Initial cache manager changes.
 *
 * Revision 65.2  1997/11/06  19:57:53  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:25  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.24.1  1996/10/02  17:12:29  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:53  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */


#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/osi_cred.h>
#include <dcedfs/afs4int.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_volume.h>
#include <cm_conn.h>
#include <cm_stats.h>
#include <cm_aclent.h>
#include <cm_dnamehash.h>
#ifdef SGIMIPS
#include <cm_bkg.h>
#endif /* SGIMIPS */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_scache.c,v 65.33 1999/04/27 18:06:45 ethan Exp $")

/* This comment describes the locking hierarchy used in the cache manager.
 * Please keep this information up-to-date!
 *
 * fetch/store flags in scache entries
 *
 * fetch/store flags in dcache entries
 *
 * scache llocks (lock in vnode order if lock more than one at once)
 *
 * dcache llocks
 *
 * nh_lock (dnamehash lock) (locks cm_scachelock, called with locked vnodes)
 *
 * cm_scachelock (locked before cm_dcachelock by FlushSCache and cm_GetDownD)
 *
 * cm_getdcachelock (locked before cm_dcachelock in
 *     cm_GetDCache, cm_GetNDCaches)
 *
 * cm_dcachelock
 *
 * volume lock
 *
 * cm_volumelock
 *
 * server lock
 *
 * cm_serverlock
 *
 * conn lock
 *
 * cm_connlock
 *
 * cm_tgtlock, cm_racinglock, cm_qtknlock, cm_memCacheLock, cm_initStateLock
 *
 * Note that the only locks that can be held over a primary interface
 * RPC are the fetching flags for scache and dcache entries (not
 * the storing flags!).  Holding any other lock over an RPC to the
 * primary interface will result in terrible global deadlocks.
 *
 * Also note that the fetch/store scache/dcache locks can only be
 * changed while holding the corresponding scache or dcache entry
 * locks in write mode.  This means that you can prevent a lock from
 * being acquired by holding the appropriate llock.  Also note that
 * this requires that the llocks to below the fetch/store flags in the
 * locking hierarchy, so that we can obtain the llocks before clearing
 * the flags.
 *
 * Note that no RPCs, to either the primary or secondary interface,
 * can be made while holding locks at or below the cm_volumelock
 * level, since locks below that level are required for the basic
 * RPC connection management machinery.
 *
 * Here's the definition of the locking semantics of the status and
 * data storing and fetching flags, as well as the storeCount field
 * in the scache entry:
 *
 * The status and data fetching flags are set whenever the status
 * (for a file) or data (for a dcache entry) is being fetched by the
 * server.
 *
 * The status storing flag is set whenever status info is being
 * sent back to the server.  This includes operations that truncate
 * or extend the file.  The scache storeCount field is incremented
 * everytime any data cache entry is stored back, even if it isn't
 * storing any status information back.  Typically, either the storeCount
 * field is non-zero (indicating a commutative store of some chunk,
 * protected by the dcache storing flag), or the scache store flag is
 * set, indicating that we're actually updating global file information,
 * like the file length.
 *
 * The data storing flag indicates that a particular chunk is being
 * stored back to the server.  Whenever this is happening, either
 * the vnode's storeCount is bumped or the vnode's status storing flag
 * is set.
 */
   

/*
 * Cache module related globals
 */
struct lock_data cm_scachelock;		/* lock: alloc new stat cache entries */
struct cm_scache *freeSCList = 0;	/* free list for stat cache entries */
struct cm_scache *Initial_freeSCList;	/* Ptr to orig malloced free list */
struct squeue SLRU;                     /* LRU queue for stat entries */
/*static*/ long cm_scount=0;		/* Runaway scache struct alloc cntr */
struct cm_scache *cm_shashTable[SC_HASHSIZE]; /* stat cache hash table */

static void GetDownS _TAKES((int));
static int FlushSCache _TAKES((struct cm_scache *, int));

/* holds bulkstat statistics */

struct sc_stats sc_stats;

/* 
 * This routine is responsible for allocating a new cache entry from the free 
 * list. It formats the cache entry and inserts it into the appropriate hash 
 * tables.  It must be called with cm_scachelock write-locked so as to prevent
 * several processes  from trying to create a new cache entry simultaneously.
 *    
 * param fidp is the file id of the file whose cache entry is being  created.
 */
#ifdef SGIMIPS
struct cm_scache *cm_NewSCache(
  register afsFid *fidp,
  struct cm_volume *avolp)
#else  /* SGIMIPS */
struct cm_scache *cm_NewSCache(fidp, avolp)
  register afsFid *fidp;
  struct cm_volume *avolp;
#endif /* SGIMIPS */
{

#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */

    struct cm_scache *scp;
    register long i;
#ifdef	AFS_AIX_ENV
    struct gnode *gnodepnt;
#endif

    /* 
     * Pull out a free cache entry 
     */
    if (!freeSCList) {
	GetDownS(5);
	/*
	 * The cm_scachelock may have been dropped to recycle scache
	 * entries, so recheck to see if another thread already created
	 * an entry.
	 */
	if (scp = cm_FindSCache(fidp)) {
	    return scp;
	}
    }
    if (!freeSCList) {
	/* 
	 * none free, making one is better than a panic 
	 */
	cm_scount++;			/* count in case we have a leak */
	scp = (struct cm_scache *) osi_Alloc(sizeof (struct cm_scache));
	CM_BUMP_COUNTER(statusCacheEntries);
    } else {
	scp = freeSCList;  		/* take from free list */
	freeSCList = (struct cm_scache *) (scp->lruq.next);
    }
    bzero((char *) scp, sizeof(struct cm_scache)); /* clear whole enchilada */
    scp->fid = *fidp;
#ifdef SGIMIPS
    scp->vp = vn_alloc(NULL, VNON, NODEV);
    s = VN_LOCK(SCTOV(scp));
    /* bump an extra count to keep our vnode held until a FlushScache happens */
    ASSERT(SCTOV(scp)->v_count > 0);
    CM_RAISERC(scp);
    scp->vp->v_flag &= ~VSHARE;
    VN_UNLOCK(SCTOV(scp), s);
#else
    CM_SETRC(scp, 1);
#endif
    scp->next = cm_shashTable[i = SC_HASH(fidp)];
    cm_shashTable[i] = scp;
    scp->ds.scanChunk = -1;	/* invalid */
    QInit(&scp->lockList);
    QInit(&scp->tokenList);
    QAdd(&SLRU, &scp->lruq);			/* put in lruq */
    lock_Init(&scp->llock);
#ifdef SGIMIPS
    mrinit(&scp->rwlock, "DFS RWLOCK");		/* Init our R/W lock */
#endif
#ifdef AFS_AIX31_VM
    lock_Init(&scp->pvmlock);
#endif
#ifdef AFS_SUNOS5_ENV
    rw_init(&scp->rwlock, "DFS vnode rwlock", RW_DEFAULT, DEFAULT_WT);
#endif
    scp->randomACL = (struct cm_aclent *) 0;
    /* 
     * Initialize vnode data
     */
#ifdef	AFS_AIX_ENV
    /* 
     * Don't forget to free the gnode space 
     */
    SCTOV(scp)->v_gnode = gnodepnt = (struct gnode*)osi_Alloc(sizeof(struct gnode));
    bzero((char *)gnodepnt, sizeof(struct gnode));
#endif
#ifndef SCACHE_PUBLIC_POOL
#ifdef SGIMIPS
    bhv_desc_init(&(scp->cm_bhv_desc), scp, scp->vp, cm_GetAFSOps())
    bhv_insert_initial(VN_BHV_HEAD(scp->vp), &(scp->cm_bhv_desc));
#else
    osi_SetVnodeOps(scp, cm_GetAFSOps());
#endif
#endif
    scp->volp = avolp;
    if (avolp) {
	cm_HoldVolume(avolp);
	/* check if this is the volume root; mark it as such */
	if (!FidCmp(&scp->fid, &avolp->rootFid)) {
	    scp->states |= SC_VOLROOT;
	    /* and copy out ".." information, if present */
	    if (avolp->dotdot.Vnode != 0) {
		if (!scp->rootDotDot)
		    scp->rootDotDot = (afsFid *)osi_Alloc(sizeof (afsFid));
		*scp->rootDotDot = avolp->dotdot;
	    }
	}
	/* and propagate out read-only flag */
	if (avolp->states & VL_RO)
	    scp->states |= SC_RO;
    }
    if (cm_globalVFS == (struct osi_vfs *) 0)
	panic("NewSCache: afs globalvfs");
#ifndef SCACHE_PUBLIC_POOL
    osi_vSetVfsp(SCTOV(scp), cm_globalVFS);
    SET_CONVERTED(SCTOV(scp));
#else
    scp->v.v_vnode = NULLVP;
#endif /* SCACHE_PUBLIC_POOL */
    cm_vSetType(scp, VREG);
#ifdef	AFS_HPUX_ENV
    scp->v.v_fstype = VDFS;
#endif	/* AFS_HPUX_ENV */
#ifdef	AFS_AIX_ENV
    scp->v.v_vfsnext = cm_globalVFS->vfs_vnodes;  
    scp->v.v_vfsprev = NULL;
    cm_globalVFS->vfs_vnodes = &scp->v;
    if (scp->v.v_vfsnext != NULL)
	scp->v.v_vfsnext->v_vfsprev = &scp->v;
    scp->v.v_next = gnodepnt->gn_vnode;		/* Single vnode per gnode */
    gnodepnt->gn_vnode = &scp->v;
#endif
    /* now do some periodic work that may be triggered by creating a
     * new scache entry.
     */
    if (cm_needServerFlush) {
	lock_ReleaseWrite(&cm_scachelock);
	cm_ServerTokenMgt();
	lock_ObtainWrite(&cm_scachelock);
    }
    return scp;
}

/* 
 * This function takes a file id and request structure, and is responsible
 * for fetching the status information associated with the file.
 * 
 * The return value is a UNIX error code, which may be 0 or ESTALE.  The
 * caller is responsible for retrying if ESTALE is returned.
 *
 * The cache entry is passed back with an increased vrefCount field. The entry 
 * must be discarded by calling cm_PutSCache when you are through using the 
 * pointer to the cache entry.
 *
 * Since the scp is not returned locked, the only guarantee of token state
 * associated with the scp is OPEN_PRESERVE (due to the bumped vrefCount) and
 * therefore the immutable fields in the status information can be relied upon
 * without obtaining additional tokens.
 *
 * You should not hold any locks when calling this function, except locks on 
 * other scache entries. If you lock more than one scache entry simultaneously,
 * you should lock them in this order:
 *
 *    1.  Lock all files first, then directories.
 * 
 *    2.  Within a particular type, lock entries in Fid.Vnode order.
 *  
 * This locking hierarchy is convenient because it allows locking of a parent 
 * dir cache entry, given a file (to check its access control list).  It also 
 * allows renames to be handled easily by locking directories in a constant 
 * order.
 */
#ifdef SGIMIPS
int cm_GetSCache(
    register afsFid *fidp,
    struct cm_scache **scpp,
    struct cm_rrequest *rreqp) 
#else  /* SGIMIPS */
int cm_GetSCache(fidp, scpp, rreqp)
    register afsFid *fidp;
    struct cm_scache **scpp;
    struct cm_rrequest *rreqp; 
#endif /* SGIMIPS */
{
    register long code;
    register struct cm_conn *connp;
    register struct cm_scache *scp;
    afsFetchStatus OutStatus;
    afs_token_t Token;
    u_long startTime, flags = 0;
    struct cm_server *tserverp;
    afsVolSync tsync;
    struct cm_volume *volp;
    long srvix;
    afs_hyper_t *vvp;
    error_status_t st;
    int reallyOK;

    *scpp = (struct cm_scache *) 0;
    volp = cm_GetVolume(fidp, rreqp);
    if (!volp)
       return ENODEV;
    lock_ObtainWrite(&cm_scachelock);
    scp = cm_FindSCache(fidp);
    if (!scp)
	/* 
	 * No cache entry, better grab one 
	 */
	scp = cm_NewSCache(fidp, volp);
    lock_ReleaseWrite(&cm_scachelock);

    icl_Trace3(cm_iclSetp, CM_TRACE_GETSCACHE, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, AFS_hgetlo(fidp->Volume),
	       ICL_TYPE_LONG, fidp->Vnode);

    lock_ObtainRead(&scp->llock);

    for (;;) {
	/* 
	 * Check if we're all done 
	 */
	if ((scp->states & SC_STATD) &&
	    cm_HaveTokens(scp, TKN_OPEN_PRESERVE)) {

	    lock_ReleaseRead(&scp->llock);
	    if (volp) 
		cm_PutVolume(volp);

	    *scpp = scp;
	    return 0;
	}

	lock_ReleaseRead(&scp->llock);
	lock_ObtainWrite(&scp->llock);
	if (scp->states & SC_FETCHING) {
	    /* someone else already fetching status for this file */
	    scp->states |= SC_WAITING;
	    osi_SleepW(&scp->states, &scp->llock);
	    lock_ObtainRead(&scp->llock);
	    continue;	/* from the top requires read lock*/
	}
	/* if we get here, we've committed to doing a stat RPC call,
	 * and we have a write lock on the scache entry.  We've set the
	 * status fetching flag, and so others will wait until we're done.
	 */
	scp->states |= SC_FETCHING;
	break;
    } /* for (;;) */

    /* 
     * Stat the file 
     */
    AFS_hzero(Token.beginRange);
    Token.endRange = osi_hMaxint64;
    AFS_hset64(Token.type, 0, TKN_STATUS_READ | TKN_OPEN_PRESERVE);
    if (cm_HaveTokensRange(scp, &Token, 0, &reallyOK))
	flags = 0;
    else if (reallyOK)
	/* Don't request tokens if we already have them. */
	flags = 0;
    else
	flags = AFS_FLAG_RETURNTOKEN;
    lock_ReleaseWrite(&scp->llock);
    cm_StartTokenGrantingCall();

    /* Normally cm_StartVolumeRPC() is called by cm_Conn, but since
     * in this case volp will not be NULL it won't so we call it here
     * to note the fact that we start an RPC for this volume.
     */
    if (volp) 
	cm_StartVolumeRPC(volp);
    do {
	tserverp = 0;
	if (connp = cm_Conn(fidp, MAIN_SERVICE(SRT_FX),
			    rreqp, &volp, &srvix)) {
	    if (volp && ((volp->states & VL_LAZYREP) != 0))
		vvp = &volp->latestVVSeen;
	    else
		vvp = &osi_hZero;
	    icl_Trace0(cm_iclSetp, CM_TRACE_SCACHESTART);
	    startTime = osi_Time();
	    st = BOMB_EXEC
		("COMM_FAILURE",
		 (osi_RestorePreemption(0),
		  st = AFS_FetchStatus(connp->connp, fidp, vvp, flags,
				       &OutStatus, &Token, &tsync),
		  osi_PreemptionOff(),
		  st));
	    code = osi_errDecode(st);
	    tserverp = connp->serverp;
	    cm_SetReqServer(rreqp, tserverp);
	    icl_Trace1(cm_iclSetp, CM_TRACE_SCACHEEND, ICL_TYPE_LONG, code);
	} else 
	    code = -1;
	if (code == 0) {
	    /* Loop back if this call went backwards in VV-time */
#ifdef SGIMIPS
	    code = cm_CheckVolSync(fidp, &tsync, volp, startTime, 
					(int)srvix, rreqp);
#else  /* SGIMIPS */
	    code = cm_CheckVolSync(fidp, &tsync, volp, startTime, srvix, rreqp);
#endif /* SGIMIPS */
	}
    } while (cm_Analyze(connp, code, fidp, rreqp, volp, srvix, startTime));
    lock_ObtainWrite(&scp->llock);

    /* wakeup others waiting for new status info */
    scp->states &= ~SC_FETCHING;
    if (scp->states & SC_WAITING) {
	scp->states &= ~SC_WAITING;
	osi_Wakeup(&scp->states);
    }

    if (code) {
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	/* now, if we're still within hardMaxTotalLatency, we're fine
	 * anyway.  So, if this is a readonly and we have stat info,
	 * and either we have no hard max latency, or it hasn't been
	 * exceeded, return success anyway.
	 */
	if ((reallyOK & CM_REALLYOK_REPRECHECK) && (scp->states & SC_STATD)) {
	    lock_ReleaseWrite(&scp->llock);
	    if (volp) 	
		cm_EndVolumeRPC(volp);		/* done with it */
	    *scpp = scp;
	    return 0;
	}
	else {
	    /* failed, and we can't use what we've got */
	    lock_ReleaseWrite(&scp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);
	    lock_ReleaseWrite(&cm_scachelock);
	    if (volp) 	
		cm_EndVolumeRPC(volp);		/* done with it */
#ifdef SGIMIPS
	    return (int)code;
#else  /* SGIMIPS */
	    return code;
#endif /* SGIMIPS */
	}
    }
    cm_EndTokenGrantingCall(&Token, tserverp, scp->volp, startTime);
    tokenStat.FetchStatus++;
    cm_MergeStatus(scp, &OutStatus, &Token, 0, rreqp);
    /* Check for race with deletion */
    code = cm_GetTokens(scp, TKN_OPEN_PRESERVE, rreqp, WRITE_LOCK);
    lock_ReleaseWrite(&scp->llock);
    if (volp) 	
	cm_EndVolumeRPC(volp);		/* done with it */
    if (code) {
	cm_PutSCache(scp);
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }
    *scpp = scp;
    return 0;
}


/* 
 * Find a scache entry.  Must be called with the cm_scachelock write locked.
 * (For CM_HOLD to work on an MP).
 */
#ifdef SGIMIPS
struct cm_scache *cm_FindSCache(register afsFid *fidp) 
#else  /* SGIMIPS */
struct cm_scache *cm_FindSCache(fidp)
    register afsFid *fidp; 
#endif /* SGIMIPS */
{
    register struct cm_scache *scp;

    for (scp = cm_shashTable[SC_HASH(fidp)]; scp; scp = scp->next) {
        if (!FidCmp(&scp->fid, fidp) && scp->asyncStatus == 0) {
	    break;
        }
    }

    if (scp) {

	/* installed by bulkstat? */

	if(scp->installStates & BULKSTAT_INSTALLEDBY_BULKSTAT)
	{
		scp->installStates |= BULKSTAT_SEEN;
        	sc_stats.statusCacheBulkstatSeen++;
	}

	CM_BUMP_COUNTER(statusCacheHits);
        CM_HOLD(scp);
        QRemove(&scp->lruq);
        QAdd(&SLRU, &scp->lruq);                /* Move to lruq head */
    }
    else {
	CM_BUMP_COUNTER(statusCacheMisses);
    }
    return scp;
}


#if CM_MAXFIDSZ<24

/* 
 * Find an scache entry for NFS (cm_vget).  Identical to cm_FindSCache (above),
 * but does a partial match, since some bits of the FID are not known.
 * There may be ambiguity (more than one match), in which case we fail.
 */
#ifdef SGIMIPS
struct cm_scache *cm_NFSFindSCache(register afsFid *fidp)
#else  /* SGIMIPS */
struct cm_scache *cm_NFSFindSCache(fidp)
    register afsFid *fidp; 
#endif /* SGIMIPS */
{
    register struct cm_scache *scp;
    struct cm_scache *found_scp = NULL;

    for (scp = cm_shashTable[SC_HASH(fidp)]; scp; scp = scp->next) {

#if CM_MAXFIDSZ>=20

	/* Ignore high 32 bits of Volume */
	if (scp->fid.Unique == fidp->Unique
	    && scp->fid.Vnode == fidp->Vnode
	    && AFS_hgetlo(scp->fid.Volume) == AFS_hgetlo(fidp->Volume)
	    && AFS_hsame(scp->fid.Cell, fidp->Cell)
	    && scp->asyncStatus == 0) {
	    if (found_scp)
		return NULL;
	    else found_scp = scp;
	}

#else

	/* Ignore high 48 bits of Volume and high 16 bits of Vnode */
	if (scp->fid.Unique == fidp->Unique
	    && (scp->fid.Vnode & 0xfffff) == (fidp->Vnode & 0xfffff)
	    && ((AFS_hgetlo(scp->fid.Volume) & 0xfffff) ==
		(AFS_hgetlo(fidp->Volume) & 0xfffff))
	    && AFS_hsame(scp->fid.Cell, fidp->Cell)
	    && scp->asyncStatus == 0) {
	    if (found_scp)
		return NULL;
	    else found_scp = scp;
        }

#endif /* CM_MAXFIDSZ>=20 */

    }

    scp = found_scp;
    if (scp != NULL) {
	CM_BUMP_COUNTER(statusCacheHits);
        CM_HOLD(scp);
        QRemove(&scp->lruq);
        QAdd(&SLRU, &scp->lruq);                /* Move to lruq head */
    }
    else {
	CM_BUMP_COUNTER(statusCacheMisses);
    }
    return scp;
}

#endif /* CM_MAXFIDSZ<24 */


/*
 * Mark access, modify and change times to be userTime. 0 (or current time if 
 * userTime is equal to zero.
 *
 * Requires write lock on llock.
 */
#ifdef SGIMIPS
int cm_MarkTime(
  register struct cm_scache *scp,
  long userTime,
  osi_cred_t *credp,
  long flag,			/* Which time to modify */
  int doLock)
#else  /* SGIMIPS */
int cm_MarkTime(scp, userTime, credp, flag, doLock)
  osi_cred_t *credp;
  register struct cm_scache *scp;
  long userTime;
  long flag;			/* Which time to modify */
  int doLock;
#endif /* SGIMIPS */
{
    register long code;
    struct cm_rrequest treq;
    
    if (userTime == 0)   /* user not providing the time */
        userTime = osi_Time();

    if (flag & ~CM_MODACCESS) {
	icl_Trace3(cm_iclSetp, CM_TRACE_MARKTIME, ICL_TYPE_POINTER, scp,
		   ICL_TYPE_LONG, userTime, ICL_TYPE_LONG, flag);

	/* if we're doing more than an access time update (for which we
	 * do not want to bother getting an update token), and if we
	 * have a credential, then get the appropriate token.
	 */
	if (doLock) {
	    cm_InitReq(&treq, credp);
	    if (code = cm_GetTokens(scp, TKN_UPDATESTAT, &treq, WRITE_LOCK))
		return cm_CheckError(code, &treq);
	}
    }
    if (flag & CM_MODACCESS) {
        scp->m.ModFlags |= CM_MODACCESS;
#ifdef SGIMIPS
	scp->m.AccessTime.sec = (unsigned32)userTime;
#else  /* SGIMIPS */
	scp->m.AccessTime.sec = userTime;
#endif /* SGIMIPS */
	scp->m.AccessTime.usec = 0;
    }
    if (flag & CM_MODMOD) {
        scp->m.ModFlags |= CM_MODMOD;
	/* utimes, which is the only call that needs to set the mtime
	 * to an exact value (and should thus disable the server code
	 * that forces the mtime to be a good version # and monotonically
	 * increase), doesn't use this path, so we always turn off
	 * MODMODEXACT here.
	 */
	scp->m.ModFlags &= ~CM_MODMODEXACT;	/* not from a utimes */
#ifdef SGIMIPS
	scp->m.ModTime.sec = (unsigned32)userTime;
#else  /* SGIMIPS */
	scp->m.ModTime.sec = userTime;
#endif /* SGIMIPS */
	scp->m.ModTime.usec = 0;
    }
    if (flag & CM_MODCHANGE) {
        scp->m.ModFlags |= CM_MODCHANGE;
#ifdef SGIMIPS
	scp->m.ChangeTime.sec = (unsigned32)userTime;
#else  /* SGIMIPS */
	scp->m.ChangeTime.sec = userTime;
#endif /* SGIMIPS */
	scp->m.ChangeTime.usec = 0;
    }
    return 0;
}


/*
 * This function is called to decrement the reference count on a cache entry. 
 */
#ifdef SGIMIPS
void cm_PutSCache(register struct cm_scache *scp) 
#else  /* SGIMIPS */
void cm_PutSCache(scp)
    register struct cm_scache *scp; 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_scachelock);
    CM_RELE(scp);
    lock_ReleaseWrite(&cm_scachelock);
}


/* called with at least a read-locked scp.
 * tells whether we need to store the status from this file or not.
 */
#ifdef SGIMIPS
int cm_NeedStatusStore(register struct cm_scache *scp)
#else  /* SGIMIPS */
int cm_NeedStatusStore(scp)
  register struct cm_scache *scp;
#endif /* SGIMIPS */
{
    /* cm_NeedStatusStore MUST NOT be more aggressive than what cm_ScanStatus 
     * can consume. Else, the loop in RevokeStatusToken cannot make sense.
     */
     if ((scp->m.ModFlags & (CM_MODTRUNCPOS | CM_MODLENGTH | CM_MODMOD
			      | CM_MODOWNER | CM_MODGROUP | CM_MODMODE)) ||
	(((scp->states & SC_RO) == 0) && (scp->m.ModFlags & CM_MODACCESS)
	 && scp->m.AccessTime.sec > scp->m.serverAccessTime + CM_ACCESSSKEW))
	return 1;
    else
	return 0;
}


/*
 * This function is called to increment the reference count on a cache entry. 
 */
#ifdef SGIMIPS
void cm_HoldSCache(register struct cm_scache *scp) 
#else  /* SGIMIPS */
void cm_HoldSCache(scp)
    register struct cm_scache *scp; 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_scachelock);
    CM_HOLD(scp);
    lock_ReleaseWrite(&cm_scachelock);
}


/* 
 * This function takes a fid and a cm_rrequest structure, and is responsible 
 * for storing the status information back to the server.  This routine 
 * must be called with the llock write-locked and uses the X file service.
 * This function drops the llock while storing the updates.
 *
 * The storeStatusp field describes a set of changes to be made
 * to the vnode only if the RPC succeeds.  The caller should merge
 * those changes in with those from cm_ScanStatus before calling
 * this function, or pass in a null, and we'll do the scan status.
 *
 */
#ifdef SGIMIPS
int
cm_StoreSCache(
    register struct cm_scache *scp,
    register afsStoreStatus *storeStatusp,
    int fsyncFlag,
    struct cm_rrequest *rreqp) 
#else  /* SGIMIPS */
cm_StoreSCache(scp, storeStatusp, fsyncFlag, rreqp)
    register struct cm_scache *scp;
    register afsStoreStatus *storeStatusp;
    int fsyncFlag;
    struct cm_rrequest *rreqp; 
#endif /* SGIMIPS */
{
    register long code;
    register struct cm_conn *connp;
    struct lclHeap {
	afsFetchStatus OutStatus;
	afsStoreStatus tstatus;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    struct cm_volume *volp = (struct cm_volume *)0;
    long serverFlags;
    long flags;
    long srvix;
    u_long startTime;
    error_status_t st;

    if (code = scp->asyncStatus) {
#ifdef SGIMIPS
       return (int)code;
#else  /* SGIMIPS */
       return code;
#endif /* SGIMIPS */
    }

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
    if (!storeStatusp) {
	/* stabilize the cache entry and mark it as being stored.
	 * don't StabilizeSCache if storeStatusp passed in, as scache
	 * already stabilized, and we don't want more
	 * dirty bits being added before we get SC_STORING set.
	 */
	cm_StabilizeSCache(scp);
	lhp->tstatus.mask = 0;	/* init */
	storeStatusp = &lhp->tstatus;
	cm_ScanStatus(scp, storeStatusp);	/* add local status */
    }

    if (storeStatusp->mask == 0 && !fsyncFlag) {
#ifdef AFS_SMALLSTACK_ENV
	osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	return 0;	/* nothing to do */
    }

    /* otherwise, commit to storing */
    scp->states |= SC_STORING;
    scp->states &= ~SC_STOREINVAL;

    if (fsyncFlag)
	serverFlags = AFS_FLAG_SKIPTOKEN | AFS_FLAG_SYNC;
    else
	serverFlags = AFS_FLAG_SKIPTOKEN;

    /* release the lock to make the RPC */
    lock_ReleaseWrite(&scp->llock);
    do {
	if (connp = cm_Conn(&scp->fid,  SEC_SERVICE(SRT_FX),
			    rreqp, &volp, &srvix)) {
	    /* Check whether TSR trashed this store. */
	    code = 0;
	    lock_ObtainWrite(&scp->llock);
	    if ((scp->states & SC_STOREINVAL) || scp->asyncStatus) {
		scp->states &= ~SC_STOREINVAL;
		if (!code) code = scp->asyncStatus;
		if (!code) code = ESTALE;	/* anything but 0 */
		icl_Trace3(cm_iclSetp, CM_TRACE_STORESCACHECONNINVALID,
			   ICL_TYPE_POINTER, scp,
			   ICL_TYPE_FID, &scp->fid,
			   ICL_TYPE_LONG, code);
	    }
	    lock_ReleaseWrite(&scp->llock);
	    if (!code) {
		icl_Trace0(cm_iclSetp, CM_TRACE_STORESTATUSSTART);
		startTime = osi_Time();
		st = BOMB_EXEC("COMM_FAILURE",
			       (osi_RestorePreemption(0),
				st = AFS_StoreStatus(connp->connp, &scp->fid, 
						     storeStatusp, &osi_hZero,
						     serverFlags, &lhp->OutStatus, 
						     &lhp->tsync),
				osi_PreemptionOff(),
				st));
		code = osi_errDecode(st);
		icl_Trace1(cm_iclSetp, CM_TRACE_STORESTATUSEND, ICL_TYPE_LONG, code);
	    }
	} else 
	    code = -1;
    } while (cm_Analyze(connp, code, &scp->fid, rreqp, volp, srvix, startTime));
    lock_ObtainWrite(&scp->llock);
    if (code == 0) {
        /* Success; do the changes locally */
	flags = CM_MERGE_STORING;	/* means we did a ScanStatus already */
	if (storeStatusp->mask & (AFS_SETMODE | AFS_SETOWNER | AFS_SETGROUP))
	    flags |= CM_MERGE_CHACL;
#ifdef SGIMIPS
	cm_MergeStatus(scp, &lhp->OutStatus, (afs_token_t *) 0, 
			(int)flags, rreqp);
	(void) cm_CheckVolSync(&scp->fid, &lhp->tsync, volp, startTime, 
			(int)srvix, rreqp);
#else  /* SGIMIPS */
	cm_MergeStatus(scp, &lhp->OutStatus, (afs_token_t *) 0, flags, rreqp);
	(void) cm_CheckVolSync(&scp->fid, &lhp->tsync, volp, startTime, srvix, rreqp);
#endif /* SGIMIPS */
    }
    /* nothing to do if fail, since we want to leave
     * already performed vnode changes around for bkg daemon to do
     */

    /* clear storing flag, and wakeup anyone waiting */
    scp->states &= ~SC_STORING;
    if ((scp->states & SC_STOREINVAL) || scp->asyncStatus) {
	scp->states &= ~SC_STOREINVAL;
	if (!code) code = scp->asyncStatus;
	if (!code) code = ESTALE;	/* anything but 0 */
	icl_Trace3(cm_iclSetp, CM_TRACE_STORESCACHEINVALID,
		   ICL_TYPE_POINTER, scp,
		   ICL_TYPE_FID, &scp->fid,
		   ICL_TYPE_LONG, code);
    }
    if (scp->states & SC_WAITING) {
	scp->states &= ~SC_WAITING;
	osi_Wakeup(&scp->states);
    }
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    if (volp) 
	cm_EndVolumeRPC(volp);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}


/*
 * This function stores back all of the modified status information for this 
 * file. It is called without any locks.
 */
#ifdef SGIMIPS
long cm_SyncSCache (register struct cm_scache *scp)
#else  /* SGIMIPS */
long cm_SyncSCache(scp)
  register struct cm_scache *scp;
#endif /* SGIMIPS */
{
    register long code = 0;
    struct cm_rrequest treq;

    icl_Trace1(cm_iclSetp, CM_TRACE_SYNCSCACHE, ICL_TYPE_POINTER, scp);
    lock_ObtainWrite(&scp->llock);
    if (scp->m.ModFlags & CM_MODMASK) {
	/* figure out what credential we need to use, based on opener.
	 */
	if (scp->writerCredp)
	    cm_InitReq(&treq, scp->writerCredp);
	else
	    cm_InitReq(&treq, osi_credp);

	/* finally, do the work */
	code = cm_StoreSCache(scp, (afsStoreStatus *) 0, 0, &treq);
	if ((code = cm_CheckError(code, &treq)) == 0)
	    goto out;
	if (CM_ERROR_TRANSIENT(code)) {
	    if (!(scp->states & SC_STOREFAILED)) {
		scp->storeFailTime = osi_Time();
#ifdef SGIMIPS
		scp->storeFailStatus = (u_char)code;
#else  /* SGIMIPS */
		scp->storeFailStatus = code;
#endif /* SGIMIPS */
		scp->states |= SC_STOREFAILED;
	    }
	    icl_Trace1(cm_iclSetp, CM_TRACE_SYNCSCACHEFAIL, ICL_TYPE_POINTER, 
		       scp);
	    goto out;
	}
	cm_MarkBadSCache(scp, code);
    }

out:
    lock_ReleaseWrite(&scp->llock);
    return code;
}

/*
 * Copy in random modified status information to the store status block.
 * Vnode llock must be write-locked.
 */
#ifdef SGIMIPS
void cm_ScanStatus(
    struct cm_scache *scp,
    afsStoreStatus *storeStatusp)
#else  /* SGIMIPS */
void cm_ScanStatus(scp, storeStatusp)
    struct cm_scache *scp;
    afsStoreStatus *storeStatusp; 
#endif /* SGIMIPS */
{
    /* cm_NeedStatusStore MUST NOT be more aggressive than what cm_ScanStatus can consume. */
    /* Else, the loop in RevokeStatusToken cannot make sense. */
    icl_Trace2(cm_iclSetp, CM_TRACE_SCANSTATUS, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, scp->m.ModFlags);
    if (scp->m.ModFlags & CM_MODTRUNCPOS) {
	storeStatusp->mask |= AFS_SETTRUNCLENGTH;
#ifdef SGIMIPS 
	storeStatusp->truncLength = scp->truncPos;
#else  /* SGIMIPS */
	AFS_hset64(storeStatusp->truncLength, 0, scp->truncPos);
#endif /* SGIMIPS */
    }
    if (scp->m.ModFlags & CM_MODLENGTH) {
	storeStatusp->mask |= AFS_SETLENGTH;
#ifdef SGIMIPS 
	storeStatusp->length = scp->m.Length;
#else  /* SGIMIPS */
	AFS_hset64(storeStatusp->length, 0, scp->m.Length);
#endif /* SGIMIPS */
    }
    if (scp->m.ModFlags & CM_MODMOD) {
	storeStatusp->mask |= AFS_SETMODTIME;
	if (scp->m.ModFlags & CM_MODMODEXACT)
	    storeStatusp->mask |= AFS_SETMODEXACT;
	storeStatusp->modTime = scp->m.ModTime;
    }
#if 0
    if (scp->m.ModFlags & CM_MODCHANGE) {
	/* 	
	 * don't do this one, it isn't handled by the server 
	 */
    }
#endif /* 0 */
    if (scp->m.ModFlags & CM_MODOWNER) {
	storeStatusp->mask |= AFS_SETOWNER;
#ifdef SGIMIPS
	storeStatusp->owner = (unsigned32)scp->m.Owner;
#else  /* SGIMIPS */
	storeStatusp->owner = scp->m.Owner;
#endif /* SGIMIPS */
    }
    if (scp->m.ModFlags & CM_MODGROUP) {
	storeStatusp->mask |= AFS_SETGROUP;
#ifdef SGIMIPS
	storeStatusp->group = (unsigned32)scp->m.Group;
#else  /* SGIMIPS */
	storeStatusp->group = scp->m.Group;
#endif /* SGIMIPS */
    }
    if (scp->m.ModFlags & CM_MODMODE) {
	storeStatusp->mask |= AFS_SETMODE;
	storeStatusp->mode = scp->m.Mode;
    }
    /* This is where we put code to skip setting access time too often.
     * If atime is within an hour, or we're making the call anyway,
     * then the access time at the server is good enough for us.
     * Explicit setting via utimes marks CM_MODACCESSEXACT, which
     * is covered by ~CM_MODACCESS.
     */
    if ((scp->m.ModFlags & CM_MODACCESS)
	&& ((scp->m.AccessTime.sec > scp->m.serverAccessTime + CM_ACCESSSKEW)
	    || (scp->m.ModFlags & ~CM_MODACCESS))) {
	storeStatusp->mask |= AFS_SETACCESSTIME;
	storeStatusp->accessTime = scp->m.AccessTime;
    }
}

/* 
 * Clear those bits that triggered the last scan status; used after a 
 * successful call to cm_ScanStatus to prevent storing the same info again.
 * The vnode llock must be write locked.
 */
#ifdef SGIMIPS
void cm_ClearScan(register struct cm_scache *scp) 
#else  /* SGIMIPS */
void cm_ClearScan(scp)
  register struct cm_scache *scp; 
#endif /* SGIMIPS */
{
    scp->m.ModFlags &= ~(CM_MODLENGTH | CM_MODMOD | CM_MODCHANGE | 
		 CM_MODACCESS | CM_MODOWNER | CM_MODGROUP | CM_MODMODE |
		 CM_MODMODEXACT | CM_MODTRUNCPOS | CM_MODACCESSEXACT);
}


/*
 * Reset access associated with all cached files for a
 * particular user.
 */
#ifdef SGIMIPS
void cm_ResetAccessCache(register long apag)
#else  /* SGIMIPS */
void cm_ResetAccessCache(apag)
     register long apag;
#endif /* SGIMIPS */
{
    register int i;
    register struct cm_scache *scp;

    lock_ObtainWrite(&cm_scachelock);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
	    CM_HOLD(scp);
	    lock_ReleaseWrite(&cm_scachelock);
	    lock_ObtainWrite(&scp->llock);
	    cm_InvalidateACLUser(scp, apag);
	    lock_ReleaseWrite(&scp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);
	}
    }
    lock_ReleaseWrite(&cm_scachelock);
}

/* 
 * This routine is responsible for moving at least one, (up to number) entries 
 * from the LRU queue to the free queue.  Number is just a hint, but this 
 * routine  must (if possible) move at least one entry, or its caller will 
 * panic.  This routine must be called with cm_scachelock write-locked.
 */
#ifdef SGIMIPS
static void GetDownS(int number)
#else  /* SGIMIPS */
static void GetDownS(number)
    int number; 
#endif /* SGIMIPS */
{
    register struct squeue *tq;
    struct squeue *uq;
    register struct cm_scache *scp;

    if (lock_Check(&cm_scachelock) != -1) 
	panic("getdownS lock");
    for (tq = SLRU.prev; tq != &SLRU; tq = uq) {
	scp = SC_QTOS(tq);
#ifdef SGIMIPS
	if (CM_RC(scp) == 1) {		/* if open, ref count is fine */
#else  /* SGIMIPS */
	if (CM_RC(scp) == 0) {		/* if open, ref count is fine */
#endif /* SGIMIPS */
	    if (!FlushSCache(scp, 0))
		break;
	}
	/* do this after the FlushSCache, since it releases the
	 * scachelock, and uq might be bad.  However, tq isn't bad,
	 * since FlushSCache holds the vnode when the scachelock is
	 * released.  Note that if FlushSCache actually frees the vnode,
	 * we break out above.
	 */
	uq = QPrev(tq);
    }
}


/* 
 * This routine must be called with the cm_scachelock lock held for writing,
 * which prevents people from changing the vrefCount field. 
 */
#ifdef SGIMIPS
static int FlushSCache(
    register struct cm_scache *scp,
    int shuttingDown)
#else  /* SGIMIPS */
static int FlushSCache(scp, shuttingDown)
    register struct cm_scache *scp;
    int shuttingDown;
#endif /* SGIMIPS */
{
    register long i;
    register struct cm_scache **uscp, *wscp;
#ifdef SGIMIPS
    int error=0;
    afsFid lfid;
    afsFid *fidp;
#endif /* SGIMIPS */
#ifdef SGIMIPS
    int s;
    s = VN_LOCK(SCTOV(scp));
    if (CM_RC(scp) != 1) {
	if (!shuttingDown) {
            VN_UNLOCK(SCTOV(scp), s);
	    return EBUSY;
	}
    }
#else  /* SGIMIPS */
    if (CM_RC(scp) != 0) {
	if (!shuttingDown) {
	    return EBUSY;
	}
    }
#endif /* SGIMIPS */

#ifdef SGIMIPS
    VN_UNLOCK(SCTOV(scp), s);
#endif /* SGIMIPS */

    /*
     * Flush VM for this file.  On Solaris, this also checks whether the
     * open count is high.
     */
    if (cm_VMFlushSCache(scp) != 0)
	return EBUSY;

    /* Other generic checks: don't recycle a vnode that has dirty
     * data, since even if unused, we need to keep token around
     * until data sent back to server.  Same applies to modified
     * status fields.  Use cm_NeedStatusStore since it knows what
     * atime values we'd really bother to send back to the server.
     *
     * Note that open or lock tokens require an open file descriptor,
     * and so we don't have to worry about a 0 reference count file
     * requiring those tokens in order to be processed properly.
     *
     * Don't recycle a vnode if the bkg daemon hasn't yet returned
     * its TKN_OPEN_PRESERVE token.
     */
    if ((scp->modChunks > 0) || cm_NeedStatusStore(scp)) return EBUSY;
#ifdef SGIMIPS
    /* This code comes from cm_inactive.  We must do it here because
	cm_inactive is too late.  A new state is added so that we only
	queue the background request once. 
     */
    if ((scp->states & SC_RETURNREF) && (!shuttingDown)) {
        if (!(scp->states & SC_STARTED_RETURNREF)) {
            lock_ObtainWrite(&scp->llock);
	    scp->states |= SC_STARTED_RETURNREF;
            lock_ReleaseWrite(&scp->llock);
            lfid = scp->fid;                /* Make local copy before sleeping */
            /* Now make a copy that the bkg daemon can use */
            fidp = (afsFid *) osi_Alloc (sizeof(lfid));
            *fidp = lfid;
            /*
             * Since we may be called with cm_scachelock set, it's important
             * that we do not try to obtain that lock, or obtain any higher
             * lock, or wait for any process that might be waiting for that
             * lock.  The parameters to cm_bkgQueue are carefully chosen to
             * ensure this:
             *  - Flag BK_FLAG_ALLOC is set so that we do not have to wait for
             *    a bkg daemon to make a bkg request structure available;
             *  - scp's fid is passed, rather than scp itself, so that we do
             *    not call cm_HoldSCache, which would try to grab the lock.
             */
            (void) cm_bkgQueue(BK_RET_TOK_OP, 0, BK_FLAG_ALLOC, osi_getucred(),
                              (long)fidp, 0, 0, 0, 0);
        }
        return EBUSY;
    }
#else /* SGIMIPS */
    if (scp->states & SC_RETURNREF) return EBUSY;
#endif /* SGIMIPS */

    icl_Trace1(cm_iclSetp, CM_TRACE_FLUSHSCACHE, ICL_TYPE_POINTER, scp);
    CM_BUMP_COUNTER(statusCacheFlushes);

    /* installed by bulkstat and never referrenced? */

    if((scp->installStates & BULKSTAT_INSTALLEDBY_BULKSTAT) &&
	(!(scp->installStates & BULKSTAT_SEEN)))
            sc_stats.statusCacheBulkstatNotseen++;

#ifdef SGIMIPS
    /* 
     * If there are any pages left to flush, release the scache lock and do
     * it now. Then, return EBUSY in case the scache linkage has changed.
     */
    if (SCTOV(scp)->v_pgcnt || VN_DIRTY(SCTOV(scp))) {
	CM_HOLD(scp);
	/*  The cm_scachelock must be released because we could end up
		waiting for disk cache blocks.  Other threads need to
		be able to run to free up disk cache blocks, particularly
		in the case where a server is unavailable and we need
		to go through token revocation.   PV 614101 */
	lock_ReleaseWrite(&cm_scachelock);
        /* Only one thread per file in the flush/invalidate page code. */
        cm_rwlock(SCTOV(scp), VRWLOCK_WRITE);
	SC_STARTED_FLUSHING_ON(scp, error);
	if (!error) {
          VOP_FLUSHINVAL_PAGES(SCTOV(scp), 0, SZTODFSBLKS(scp->m.Length) - 1,
                                 FI_REMAPF_LOCKED);
	  SC_STARTED_FLUSHING_OFF(scp);
	}
	cm_rwunlock(SCTOV(scp), VRWLOCK_WRITE);
        lock_ObtainWrite(&cm_scachelock);
	ASSERT((SCTOV(scp)->v_pgcnt == 0) && !VN_DIRTY(SCTOV(scp)));
	CM_RELE(scp);
	return EBUSY;
    }

    /*
     * The file may have been memory mapped, but since there is no call to 
     * indicated when the file is unmapped, the SC_MEMORY_MAPPED may still 
     * be set.  We wouldn't be here if someone still has the file mapped,
     * since to get here our vnode refcount dropped to one.  We've finished
     * flushing out all of our pages, so now it is safe to clear the
     * SC_MEMORY_MAPPED flag and return our open tokens.
     */
    if ((scp->states & SC_MEMORY_MAPPED) && (!shuttingDown)) {
        lock_ObtainWrite(&scp->llock);
	scp->opens = 0;
	scp->states &= ~SC_MEMORY_MAPPED;
	if (scp->states & SC_RETURNOPEN) {
	    afs_hyper_t type;

	    scp->states &= ~SC_RETURNOPEN;
	    AFS_hset64(type, 0, (TKN_OPEN_READ | TKN_OPEN_WRITE |
                             TKN_OPEN_EXCLUSIVE | TKN_OPEN_SHARED));
            lock_ReleaseWrite(&scp->llock);
	    CM_HOLD(scp);
	    lock_ReleaseWrite(&cm_scachelock);
            cm_ReturnOpenTokens(scp, &type);
            lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);
	    return EBUSY;
	} else {
            lock_ReleaseWrite(&scp->llock);
	}
    }

    s = VN_LOCK(SCTOV(scp));
    if (!shuttingDown && ((CM_RC(scp) != 1) || scp->modChunks > 0 || 
	    cm_NeedStatusStore(scp) || (scp->states & SC_RETURNREF))) {
        VN_UNLOCK(SCTOV(scp), s);
#ifdef DEBUG
	panic("scp not completely flushed");
#else /* DEBUG */
        return EBUSY;
#endif /* DEBUG */
    }
    scp->vp->v_flag |= VSHARE;
    VN_UNLOCK(SCTOV(scp), s);
    if (!shuttingDown) {
    	ASSERT(SCTOV(scp)->v_count==1);
    }
    /* LMC    
     * We maybe have a window here.  What if someone is about to dirty
     * our pages....
     */
    CM_RELE(scp);
#ifdef DEBUG
    s = VN_LOCK(SCTOV(scp));
    if (SCTOV(scp)->v_pgcnt || SCTOV(scp)->v_dpages || SCTOV(scp)->v_buf) {
	panic("Check to see if this vnode has been reused already");
    }
    VN_UNLOCK(SCTOV(scp), s);
#endif
    scp->vp = NULL;
#endif

    /* 
     * pull the entry out of the lruq and put it on the free list 
     */
    /* Free the R/W lock in the scp */
    mrfree(&scp->rwlock);
    QRemove(&scp->lruq);
    /* 
     * Remove entry from the hash chain 
     */
    i = SC_HASH(&scp->fid);
    uscp = &cm_shashTable[i];
    for (wscp = *uscp; wscp; uscp = &wscp->next, wscp = *uscp) {
	if (scp == wscp) {
	    *uscp = scp->next;
	    break;
	}
    }
    if (!wscp) 
	panic("flushscache");	/* not in correct hash bucket */
    if (scp->volp) {
	cm_PutVolume(scp->volp);
	scp->volp = (struct cm_volume *) 0;
    }
    if (scp->rootDotDot) 
	osi_Free((opaque)(scp->rootDotDot), sizeof(afsFid));
#ifdef	AFS_AIX_ENV
    /* 
     * Free the alloced gnode that was accompanying the scache's vnode 
     */
    aix_gnode_rele(SCTOV(scp));
#endif
    cm_QuickQueueTokens(scp);		/* queue tokens 4 respective servers */
    cm_FreeAllTokens(&scp->tokenList);	/* free any left, just in case */
    cm_ClearOnlineState(scp);
    cm_FreeAllACLEnts(scp);
    cm_FreeAllCookies(scp);
    if (scp->states & SC_VDIR) cm_FreeAllVDirs(scp);
    if (scp->linkDatap) 
	osi_Free(scp->linkDatap, strlen(scp->linkDatap)+1);

    /* 
     * Put the entry in the free list and free the callback 
     */
    scp->lruq.next = (struct squeue *) freeSCList;
    freeSCList = scp;
    if (scp->writerCredp) {
	crfree(scp->writerCredp);
	scp->writerCredp = (osi_cred_t *) 0;
    }
#ifdef AFS_SUNOS5_ENV
    rw_destroy(&scp->rwlock);
    if (scp->NFSCredp) {
	crfree(scp->NFSCredp);
	scp->NFSCredp = (osi_cred_t *) 0;
    }
#endif
    return 0;
}

/* 
 * Return the scache (held) for the fileset root.
 */
#ifdef SGIMIPS
struct cm_scache *cm_GetRootSCache(
     afsFid *fidp,
     struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
struct cm_scache *cm_GetRootSCache(fidp, rreqp)
     afsFid *fidp;
     struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    struct lclHeap {
#else  /* SGIMIPS */
    register struct lclHeap {
#endif /* SGIMIPS */
	afsFetchStatus OutFidStatus;
	afs_token_t Token;
	afsFid outFid;
	afs_hyper_t savedCellID;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    struct cm_scache *rootscp = NULL;
    struct cm_server *tserverp;
    struct cm_volume *volp;
    u_long tokenFlag;
    register struct cm_conn *connp;
    long srvix;
    u_long startTime;
    afs_hyper_t *vvp;
    error_status_t st;
    long code;

    volp = cm_GetVolume(fidp, rreqp);
    if (!volp)
       return (struct cm_scache *) 0;

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

    /* 
     * save the cell id for later use after the rpc call. 
     */
    lhp->savedCellID = fidp->Cell;
    if (volp->states & VL_RO)
	tokenFlag = AFS_FLAG_SKIPTOKEN;
    else {
	tokenFlag = AFS_FLAG_RETURNTOKEN;
	AFS_hzero(lhp->Token.beginRange);
	lhp->Token.endRange = osi_hMaxint64;
	AFS_hset64(lhp->Token.type, 0, TKN_STATUS_READ | TKN_OPEN_PRESERVE);
    }
    cm_StartTokenGrantingCall();
    /* Normally cm_StartVolumeRPC() is called by cm_Conn, but since
     * in this case volp will not be NULL it won't so we call it here
     * to note the fact that we start an RPC for this volume.
     */
    if (volp) 
	cm_StartVolumeRPC(volp);

    do {
	tserverp = 0;
	if (connp = cm_Conn(fidp, MAIN_SERVICE(SRT_FX), rreqp, &volp, &srvix)){
	    if (volp && ((volp->states & VL_LAZYREP) != 0))
		vvp = &volp->latestVVSeen;
	    else
		vvp = &osi_hZero;
	    icl_Trace0(cm_iclSetp, CM_TRACE_LOOKUPROOTSTART);
	    startTime = osi_Time();
	    st = BOMB_EXEC("COMM_FAILURE",
			   (osi_RestorePreemption(0),
			    st = AFS_LookupRoot(connp->connp, fidp, vvp,
				tokenFlag, &lhp->outFid, &lhp->OutFidStatus,
				&lhp->Token, &lhp->tsync),
			    osi_PreemptionOff(),
			    st));
	    code = osi_errDecode(st);
	    tserverp = connp->serverp;
	    cm_SetReqServer(rreqp, tserverp);
	    icl_Trace1(cm_iclSetp, CM_TRACE_LOOKUPROOTEND, ICL_TYPE_LONG,code);
	} else 
	    code = -1;
	if (code == 0) {
	    /* Loop if we went backward in VV-time */
#ifdef SGIMIPS
	    code = cm_CheckVolSync(fidp, &lhp->tsync, volp, startTime, 
				   (int)srvix, rreqp);
#else  /* SGIMIPS */
	    code = cm_CheckVolSync(fidp, &lhp->tsync, volp, startTime, srvix,
				   rreqp);
#endif /* SGIMIPS */
	}
    } while (cm_Analyze(connp, code, fidp, rreqp, volp, srvix, startTime));
    if (code != 0) {
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
	osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	return NULL;
    }
    /*
     * Now process the new info for the fileset root. Note, OutFid contains
     * an appropriate vnode info, ie, (vnodeId, vnodeUnique) provided by FX.
     */
    lock_ObtainWrite(&cm_scachelock);

    /* Adjust the cell ID, in case it has been changed by FX servers */

    lhp->outFid.Cell = lhp->savedCellID;
    /*
     * Use NULL for the old-server values, since we do not know that
     * we have the HERE token for this fileset.
	 */
    rootscp = cm_FindSCache(&lhp->outFid);
    if (rootscp) { /* race condition */
       /* 
	* well, someone else already got the root scache for this fileset.
	* Just merge the new status. 
	*/
       lock_ReleaseWrite(&cm_scachelock);
       lock_ObtainWrite(&rootscp->llock);
       cm_EndTokenGrantingCall(&lhp->Token, tserverp, rootscp->volp, startTime);
       cm_MergeStatus(rootscp, &lhp->OutFidStatus, &lhp->Token, 0, rreqp);
       lock_ReleaseWrite(&rootscp->llock);
    } else  {
       rootscp = cm_NewSCache(&lhp->outFid, volp);
       lock_ReleaseWrite(&cm_scachelock);
       lock_ObtainWrite(&rootscp->llock);
       rootscp->states |= SC_VOLROOT;
       cm_EndTokenGrantingCall(&lhp->Token, tserverp, rootscp->volp, startTime);
       cm_MergeStatus(rootscp, &lhp->OutFidStatus, &lhp->Token, 0, rreqp);
       lock_ReleaseWrite(&rootscp->llock);
    }
    cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    return rootscp;
}

/*
 * This is called to mark this scache as "BAD" after the CM decides that it
 * could not reuse this scache. So, subsequent operations to this file will
 * be denied.
 *
 * Must be called with scp write-locked.
 */
#ifdef SGIMIPS
void cm_MarkBadSCache(
    struct cm_scache *scp,
    long asyncCode)
#else  /* SGIMIPS */
void cm_MarkBadSCache(scp, asyncCode)
    struct cm_scache *scp;
    long asyncCode;
#endif /* SGIMIPS */
{
    /* watch for recursive calls to this function, in case something
     * goes wrong with cm_ForceReturnToken (not too unlikely).
     */
    if (scp->states & SC_MARKINGBAD) 
        return;

    icl_Trace2(cm_iclSetp, CM_TRACE_MARKBADSCACHE,
	       ICL_TYPE_POINTER, (long) scp,
	       ICL_TYPE_LONG, asyncCode);

    scp->states |= SC_MARKINGBAD;

    /* now we want to stabilize the scache entry against all
     * stores (we can't do fetches since this is called from
     * callback procedures, but cm_InvalidateAllSegments now knows
     * how to handle a chunk being fetched, so that it will be
     * invalidated after the fetch is over, before FETCHING is cleared.
     *
     * This next call can block and release the scp lock.
     */
    /* cm_StabilizeSCache(scp); */
    /* Nope, we don't want to stabilize this entry now.  The problem is that
     * we might be being called because TSR failed for a file, underneath
     * cm_Conn(), and cm_Conn() might be being called by cm_StoreDCache() or
     * any of the other procedures that will turn off SC_STORING only when
     * they exit.
     * 
     * Instead, cm_InvalidateAllSegments will have to tolerate non-stabilized
     * scache entries and set SC_STOREINVAL and/or DC_DFSTOREINVAL; when a
     * caller goes to turn off SC_STORING or DC_DFSTORING, they'll notice
     * those xxxSTOREINVAL bits and propagate their failure.
     */

    /* if it is a file, mark vnode as bad, so that all references to
     * this vnode (from already open files, esp.) will get asyncCode
     * as an error.
     */
    if (cm_vType(scp) != VDIR) {
	lock_ObtainWrite(&cm_scachelock);
	if (!scp->asyncStatus)
#ifdef SGIMIPS
	    scp->asyncStatus = (u_char)asyncCode;
#else  /* SGIMIPS */
	    scp->asyncStatus = asyncCode;
#endif /* SGIMIPS */
	lock_ReleaseWrite(&cm_scachelock);
    }

    /* Turn off all dirty status bits.  We've already returned any
     * tokens we have that cover dirty data, and invalidated caches.
     * Now, turn off all status stuff.
     */
    cm_ClearScan(scp);
    scp->m.ModFlags &= ~CM_MODTRUNCPOS;

    /* discard all chunks, dirty or otherwise */
    cm_InvalidateAllSegments(scp,
		/* don't stabilize! */ CM_INVAL_SEGS_MARKINGBAD);

    scp->states &= ~SC_STOREFAILED;	/* no longer trying */

    /* discard caches supported by all data and status tokens; this
     * call releases the scp lock, so do it after above so we don't
     * allow vnode to be de-stabilized again.
     */
#ifdef AFS_SUNOS5_ENV
    /*
     * On Solaris, cm_ForceReturnToken may cause calls to cm_putpage, which
     * will wait for SC_PPLOCKED to clear.  The process that sets and clears
     * that flag may, however, be waiting for cm_Conn, which may be waiting
     * for TSR to complete, which may be waiting for us.  To avoid this
     * deadlock, let that process call cm_ForceReturnToken.
     */
    if (!(scp->states & SC_PPLOCKED))
	cm_ForceReturnToken(scp, TKN_UPDATE, 0);
#else
    cm_ForceReturnToken(scp, TKN_UPDATE, 0);
#endif

    icl_Trace2(cm_iclSetp, CM_TRACE_MARKBADSCACHE_2,
	       ICL_TYPE_POINTER, (long) scp,
	       ICL_TYPE_LONG, asyncCode);

    /* finally, mark that we're out of this function */
    scp->states &= ~SC_MARKINGBAD;
}

#if defined(SCACHE_PUBLIC_POOL) && defined(AFS_OSF_ENV)
cm_reclaim(vp)
register struct vnode *vp;
{
	struct cm_scache *scp;
	struct cm_scache **tscp;
	long code;

	scp = VTOSC(vp);
	/* assumes that vp and scp point at each other */
	lock_ObtainWrite(&cm_scachelock);
	tscp = (struct cm_scache **)vp->v_data;
	*tscp = (struct cm_scache *)NULL;
	scp->v.v_vnode = NULLVP;
	if (scp->v.v_count-- == 0) panic("cm_reclaim v_count");
	lock_ReleaseWrite(&cm_scachelock);
	return(0);
}

/* Function to get a corresponding public pool vnode from the system,
 * given the appropriate scache entry.  On private pool systems, there
 * is only one structure, and this function turns into a macro that
 * just casts the pointer to the appropriate type.
 *
 * Decrements the refCount on scp, and increment it on returned
 * vp.  Note that we also adjust cm refcount based on vp->v_data
 * so that if we add a new vnode reference, we bump the ref count,
 * and if we remove a reference, we decrement the refcount.
 *
 * If we fail, we don't decrement the refcount on scp, and we return
 * a null pointer from this function.
 *
 * Invariant: if scache points to a vnode, the vnode points back
 * to the same scache entry (if the scachelock is held).  The invariant
 * in the other direction also applies: if vnode points to scache entry,
 * the scache entry points back to the same vnode (under scachelock).
 */
struct vnode *
cm_get_vnode(struct cm_scache *scp)
{
    struct vnode *vp;
    struct cm_scache **cptr;

    /* fast path: see if back pointer can be validated */
    lock_ObtainWrite(&cm_scachelock);
    if (vp = scp->v.v_vnode) {
	VN_LOCK(vp);
	if (vget_nowait(vp) == 0) {
	    VN_UNLOCK(vp);
	    cptr = (struct cm_scache **)&vp->v_data;
	    /* back and forward pointer only change under scachelock */
	    osi_assert(*cptr == scp);
	    /* decrement as per cm_get_vnode's description */
	    if (scp->v.v_count-- == 0)
		panic("cm_get_vnode v_count");
	    lock_ReleaseWrite(&cm_scachelock);
	    return(vp);
	}
	VN_UNLOCK(vp);
    }
    lock_ReleaseWrite(&cm_scachelock);

    /* otherwise, we must get a new vnode and point it at our scache entry */
    for (;;) {
	/* used to just lock cm_scachelock, and VREF the pointer found in
	 * the scache entry.  But we could be in the middle of a reclaim,
	 * and VREF would bump the ref count of a doomed vnode.  We must
	 * use vget_nowait (normal vget could wait for VXLOCK (and thus
	 * for a reclaim to finish), which would violated locking hierarchy
	 * established by cm_reclaim (lock vxlock first, then cm_scachelock))
	 */

	/* wait for reclaim to finish (probably why we're here in 1st place)
	 * otherwise first vget_nowait below will fail, too.
	 */
	if (vp = scp->v.v_vnode) {
	    if (vget(vp) == 0)
		vrele(vp);
	}

	/* start by getting a vnode to install */
	if (getnewvnode(VT_AFS,(struct osi_vnodeops *)cm_GetAFSOps(), &vp)) {
	    return(NULLVP);
	}

	lock_ObtainWrite(&cm_scachelock);
	if (scp->v.v_vnode) { /* did someone already allocate it ? */
	    vp->v_type = VBAD;	/* throw away newly allocated vnode */
	    vfree(vp);
	    vp = scp->v.v_vnode;	/* use existing vnode */
	    VN_LOCK(vp);
	    if (vget_nowait(vp) == 0) {
		VN_UNLOCK(vp);
		/* only keep 1 ref for osf/1 vnode */
		if (scp->v.v_count-- == 0) panic("cm_get_vnode v_count2");
		lock_ReleaseWrite(&cm_scachelock);
		return(vp);
	    }
	    VN_UNLOCK(vp);
	    /* otherwise we lost the race; the vnode is probably vxlocked
	     * and we have to wait for it before we try again.  Probably
	     * most reasonable (in terms of OSF/1 dependencies reflected
	     * here) is to just do a vget w/o the cm_scachelock held.
	     */
	    lock_ReleaseWrite(&cm_scachelock);
	    icl_Trace1(cm_iclSetp, CM_TRACE_RECLAIMLOST, ICL_TYPE_POINTER, scp);
	    continue;
	}	/* if there's a back ptr from scache to vnode */

	/* otherwise, we found no back pointer, so we install our
	 * new vnode in here, and set its back pointer.
	 */
	osi_vSetType(vp, scp->v.v_type);
	scp->v.v_vnode = vp;
	cptr = (struct cm_scache **)vp->v_data;
	*cptr = scp;
	lock_ReleaseWrite(&cm_scachelock);
	osi_vSetVfsp(vp, cm_globalVFS);
	SET_CONVERTED(vp);
	return vp;
    } /* for (;;) */
    /* NOTREACHED */
}

#else
cm_reclaim(scp)
register struct cm_scache *scp;
{
	return(0);
}
#endif /* SCACHE_PUBLIC_POOL && AFS_OSF_ENV */


/*
 * Attempt to idle the scache pool by flushing all entries which do
 * not have a reference count.  If any entries have references or
 * are unable to be flushed, then EBUSY is returned.  The idle is
 * performed in a multi pass loop.  The first passes check for and flushe
 * all unreferenced entries which are not virtual dirs or mountpoints for
 * cell roots.  The last pass then frees the virtual dirs and cell
 * mountpoints if the earlier loops were able to free the other scache entries.
 * This is done to avoid removing an vdir or cell root entry which is
 * a parent of a referenced entry. An entry can be considered to have 
 * no references if the following conditions exist:
 *  1) it has a refcount of 0
 *  2) it is a virtual directory or the root of a cell fileset
 *     and has a refcount of 1.  Note that normal fileset mountpoints
 *     cannot have a refcount and be considered free.
 *  3) it is the root (VROOT) virtual directory for a cell and has
 *     a refcount of 2 or less.
 *
 * This function  should not be called with any scp locks or 
 * the cm_scachelock lock held.
 */
#ifdef AFS_SUNOS5_ENV
#define	AFS_ROOT_REFCOUNT		1
#else /* AFS_SUNOS5_ENV */
#define	AFS_ROOT_REFCOUNT		2
#endif /* AFS_SUNOS5_ENV */

#ifdef SGIMIPS
int cm_IdleSCaches(void)
#else  /* SGIMIPS */
int cm_IdleSCaches()
#endif /* SGIMIPS */
{
    register struct squeue *tq;
    struct squeue *uq;
    register struct cm_scache *scp;
    int rc = 0;
    int final_flush = 0;
    /* infinite loop protection: */
#ifdef SGIMIPS
    long max_retry = 20000+20*(cm_cacheScaches+cm_scount);
#else  /* SGIMIPS */
    int max_retry = 20000+20*(cm_cacheScaches+cm_scount);
#endif /* SGIMIPS */
    int retry;
#ifdef AFS_SUNOS5_ENV
    int okcount;
#endif /* AFS_SUNOS5_ENV */
    extern struct cm_scache *cm_rootVnode;
#ifdef SGIMIPS
    int s;
    vnode_t *vp;
#endif /* SGIMIPS */

    lock_ObtainWrite(&cm_scachelock);
#ifdef SGIMIPS
    for (;;) {
	retry = 0;
	/* 
	 * Break things up into two loops.  One for VDIRS and on for 
         * everything else.  If we kept this as one loop we could get hung
	 * up on a VDIR that is busy and not free'ed up by calls to
 	 * nh_delete_dvp.  Because we break out of the walking of the SLRU
	 * each time we call nh_delete_dvp (because of dropping the 
	 * cm_scachelock) we never get to the other scp's which we might
 	 * be able to clean up.
	 */
	for (tq = SLRU.prev; tq != &SLRU; tq = uq) {
	    scp = SC_QTOS(tq);
	    uq = QPrev(tq);

            vp = SCTOV(scp);
            s = VN_LOCK(vp);
	    if (CM_RC(scp) && (cm_vType(scp) == VDIR)) {
		/* 
		 * Try to clean up references in the name cache.
		 * But must make sure the scp is not one of the
		 * special cases where recounts can exists when
		 * the scp is really free.
	 	 */
		if (!(((CM_RC(scp) == 2) && 
		    (scp->states & (SC_VDIR | SC_VOLROOT)) && 
		    (scp->parentVDIR)) ||
		    ((CM_RC(scp) == (AFS_ROOT_REFCOUNT +1)) && 
		    ((SCTOV(scp))->v_flag & VROOT)))) {
                    VN_UNLOCK(vp, s);
		    lock_ReleaseWrite(&cm_scachelock);
		    nh_delete_dvp(scp);
		    lock_ObtainWrite(&cm_scachelock);
		    if (--max_retry >= 0)
			retry = 1;
		    else
			rc = EBUSY;
		    break;	/* dropped lock, start over */
		}
	    }
            VN_UNLOCK(vp, s);
	}

	for (tq = SLRU.prev; tq != &SLRU; tq = uq) {
	    scp = SC_QTOS(tq);

            vp = SCTOV(scp);
            s = VN_LOCK(vp);

	    if (CM_RC(scp) == 1) {
                VN_UNLOCK(vp, s);
		if (FlushSCache(scp, 0)) {
		    rc = EBUSY;
		    break;
		}
		/* Retry, since more ref counts might have gone to zero */
		if (--max_retry >= 0)
		    retry = 1;
		else
		    rc = EBUSY;
	    } else if ((CM_RC(scp) == 2) && 
		(scp->states & (SC_VDIR | SC_VOLROOT)) &&
		(scp->parentVDIR)) {
		if (final_flush) {
		    VN_UNLOCK(vp, s);
		    (void) FlushSCache(scp, 1);
		}
		else
		    VN_UNLOCK(vp, s);
	    } else if (((SCTOV(scp))->v_flag & VROOT) && 
		(CM_RC(scp) == AFS_ROOT_REFCOUNT + 1)) {
		if (final_flush) {
		    VN_UNLOCK(vp, s);
		    (void) FlushSCache(scp, 1);
		    cm_rootVnode = 0;
		}
		else
		    VN_UNLOCK(vp, s);
	    } else {
		VN_UNLOCK(vp, s);
		rc = EBUSY;
		break;
	    }
	    uq = QPrev(tq);
	}	/* end for */

	if (final_flush)
	    break; 	/* done */
	else if (retry == 0)
	    final_flush = 1;
    }	/* end for (;;) */
#else  /* SGIMIPS */
    for (;;) {
	retry = 0;
	for (tq = SLRU.prev; tq != &SLRU; tq = uq) {
	    scp = SC_QTOS(tq);
	    uq = QPrev(tq);

	    if (CM_RC(scp) && (cm_vType(scp) == VDIR)) {
		/* 
		 * Try to clean up references in the name cache.
		 * But must make sure the scp is not one of the
		 * special cases where recounts can exists when
		 * the scp is really free.
	 	 */
		if (!(((CM_RC(scp) == 1) && 
		    (scp->states & (SC_VDIR | SC_VOLROOT)) && 
		    (scp->parentVDIR)) ||
		    ((CM_RC(scp) == AFS_ROOT_REFCOUNT) && 
		    ((SCTOV(scp))->v_flag & VROOT)))) {
		    lock_ReleaseWrite(&cm_scachelock);
		    nh_delete_dvp(scp);
		    lock_ObtainWrite(&cm_scachelock);
		    if (--max_retry >= 0)
			retry = 1;
		    else
			rc = EBUSY;
		    break;	/* dropped lock, start over */
		}
	    }

#ifdef AFS_SUNOS5_ENV
	    if (!final_flush) {
		okcount = -1;
		if (CM_RC(scp) == 0) {
		    okcount = 0;
		} else if ((CM_RC(scp) == 1) && 
			   (scp->states & (SC_VDIR | SC_VOLROOT)) &&
			   (scp->parentVDIR)) {
		    okcount = 1;
		} else if (((SCTOV(scp))->v_flag & VROOT) && 
			   (CM_RC(scp) == AFS_ROOT_REFCOUNT)) {
		    okcount = AFS_ROOT_REFCOUNT;
		}
		/* The reference to scp->opens is just for use as a hint, so
		 * holding the wrong lock should be OK.
		 */
		if (okcount >= 0 && scp->opens != 0) {
		    CM_RAISERC(scp);
		    cm_CheckOpens(scp, okcount+1);
		    CM_LOWERRC(scp);
		}
	    }
#endif /* AFS_SUNOS5_ENV */
	    if (CM_RC(scp) == 0) {
		if (FlushSCache(scp, 0)) {
		    rc = EBUSY;
		    break;
		}
		/* Retry, since more ref counts might have gone to zero */
		if (--max_retry >= 0)
		    retry = 1;
		else
		    rc = EBUSY;
	    } else if ((CM_RC(scp) == 1) && 
		(scp->states & (SC_VDIR | SC_VOLROOT)) &&
		(scp->parentVDIR)) {
		if (final_flush) {
		    (void) FlushSCache(scp, 1);
		}
	    } else if (((SCTOV(scp))->v_flag & VROOT) && 
		(CM_RC(scp) == AFS_ROOT_REFCOUNT)) {
		if (final_flush) {
		    (void) FlushSCache(scp, 1);
		    cm_rootVnode = 0;
		    CM_SETRC(scp, 0);
		}
	    } else {
		rc = EBUSY;
		break;
	    }
	}	/* end for */

	if (rc || final_flush)
	    break; 	/* done */
	else if (retry == 0)
	    final_flush = 1;
    }	/* end for (;;) */
#endif /* SGIMIPS */
    lock_ReleaseWrite(&cm_scachelock);
    return rc;
}
