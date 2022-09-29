/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_vnodeops.c,v 65.55 1999/12/06 19:24:58 gwehrman Exp $";
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
#include <dcedfs/stds.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/osi_user.h>		/* User information */
#include <dcedfs/volume.h>		/* volume definitions include file */
#include <dcedfs/xvfs_export.h>
#include <dcedfs/dacl.h>
#include <dcedfs/direntry.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_ufs.h>
#include <dcedfs/osi_uio.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_dcache.h>
#include <cm_conn.h>
#include <cm_bkg.h>
#include <cm_vcache.h>
#include <cm_volume.h>
#include <cm_vnodeops.h>
#include <cm_aclent.h>
#include <cm_dnamehash.h>
#ifdef SGIMIPS
#include <sys/dirent.h>
#include <sys/pfdat.h>
#include <sys/sat.h>
#include <sys/mman.h>
#endif /* SGIMIPS */

#ifdef AFS_SUNOS5_ENV
#include <dcedfs/auth_at.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_vnodeops.c,v 65.55 1999/12/06 19:24:58 gwehrman Exp $")

#ifdef AFS_SUNOS5_ENV
static int (*at_atname_ptr)() = NULL;
#endif /* AFS_SUNOS5_ENV */

#ifdef AFS_HPUX_ENV
extern int vfs_pagein(), vfs_pageout();
#endif

#ifdef AFS_SUNOS54_ENV
extern void fs_dispose();
extern int fs_nosys();
#endif /* AFS_SUNOS54_ENV */

struct xvfs_vnodeops *xvfs_ops;		/* AFS xfs ops */

static int cm_write _TAKES((struct cm_scache *, struct uio *, int,
			    osi_cred_t *, int));
#ifdef SGIMIPS
static int cm_read _TAKES((struct cm_scache *, struct uio *, osi_cred_t *,
			    int, struct buf **, int));
#else  /* SGIMIPS */
static int cm_read _TAKES((struct cm_scache *, struct uio *, osi_cred_t *,
			    daddr_t, struct buf **, int));
#endif /* SGIMIPS */
static void cmattr_to_vattr _TAKES((struct cm_scache *, struct vattr *, int));
static int ENameOK _TAKES((char *));
static void HandleAtName _TAKES((char *, char *, struct cm_rrequest *,
				struct cm_scache *));
static long cm_DoPartialWrite _TAKES((struct cm_scache *, long,
				      struct cm_rrequest *, osi_cred_t *));
static long cm_MapDown _TAKES((struct cm_scache *, struct vnode *,
			       struct cm_scache **));

#ifdef SGIMIPS
static int cm_doio _TAKES((
    struct cm_scache *scp,
    struct uio *uiop,
    int ioflag,
    osi_cred_t *credp,
    enum uio_rw arw));

static cm_strat_write _TAKES((
    struct cm_scache *scp,
    struct cm_dcache *dcp,
    struct uio *uiop,
    int ioflag,
    osi_cred_t *credp));

int cm_noop _TAKES((void));
int cm_invalop _TAKES(());
void cm_void_invalop _TAKES((void));
void cm_badop _TAKES((void));
void vnl_cleanup(struct squeue *);
#endif /* SGIMIPS */

/*
When a file is first accessed we calculate whether the access is
sequential or random. Thereafter we use this result as a hint about
the nature of the access to the file. We look at the first two Unix
file system pages, then stop looking. This is the same algorithm
that Unix uses. This flag tells us to stop looking.
*/

#define CM_DONEFLAG	1

/*
We can look at a VM page at a time. This constant is the number
of bits to shift to convert a value to VM pages.
*/

#define CM_VMPAGESHIFT	12

/*
We use these values to specify either sequential or random access.
*/

#define CM_SEQUENTIAL	1
#define CM_RANDOM	0

/*
This helper routine resets all the sequential hints.
*/

#ifdef SGIMIPS
void
cm_ResetSequentialHint(struct cm_scache *scp)
#else  /* SGIMIPS */
cm_ResetSequentialHint(scp)
struct cm_scache *scp;
#endif /* SGIMIPS */
{
	/* first VM page to look at */

        scp->seq_hint.next_page=0;

	/* start out sequential */

        scp->seq_hint.seq_access_flag=CM_SEQUENTIAL;

	/*
	Calculate sequential vs random. Set this flag
	when done.
	*/

        scp->seq_hint.set_prefetch=0;

	/* the next chunk to prefetch */

        scp->seq_hint.next_chunk=1;
}

/*
Compute whether the access is random or sequential.
*/

#ifdef SGIMIPS 
void 
cm_SequentialHint(
     		struct cm_scache *scp,
		long filePos,
		long totalLength)
#else  /* SGIMIPS */
cm_SequentialHint(scp,filePos,totalLength)
struct cm_scache *scp;
long filePos;
long totalLength;
#endif /* SGIMIPS */
{
	/* start out sequential */

        if(scp->seq_hint.next_page!=filePos>>CM_VMPAGESHIFT)
        {
                /* random access */

                scp->seq_hint.seq_access_flag=CM_RANDOM;

		/*
		Done. Don't come into this routine again
		for this file.
		*/

                scp->seq_hint.set_prefetch=CM_DONEFLAG;

        }
        else
        {
                /* still sequential */

                scp->seq_hint.next_page += totalLength>>CM_VMPAGESHIFT;

               	/*
		We recognize that we have seen the first two (8 kb)
		Unix file system pages as soon as we have looked at the
		first 3 (4 kb) VM pages.

		If so, then we are done. Don't come into this
		routine again for this file.
		*/

                if(scp->seq_hint.next_page>2)
                        scp->seq_hint.set_prefetch=CM_DONEFLAG;
        }
}

/* 
 * Given a vnode ptr, open flags and credentials, open the file.  No access 
 * checks are done here, instead they're done by cm_create or cm_access, both 
 * called by the vn_open call.
 */
#ifdef SGIMIPS
int
cm_open(
    struct vnode **vpp,
    long flags,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_open(vpp, flags, credp)
    struct vnode **vpp;
    long flags;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code, tokens, writing = 0, execing;
    struct cm_rrequest rreq;
#ifdef SGIMIPS
    register struct cm_scache *tscp;
#else  /* SGIMIPS */
    register struct cm_scache *tscp = VTOSC(*vpp);
#endif /* SGIMIPS */
#ifdef EXPRESS_OPS
    struct volume *volp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */

#ifdef SGIMIPS
    VTOSC(*vpp, tscp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_OPEN, ICL_TYPE_POINTER, tscp,
	       ICL_TYPE_LONG, flags);

#ifdef AFS_SUNOS5_ENV
    /* if a SPECDEV scache vnode makes it this far, it's not
     * a real device.
     */
    if (tscp->states & SC_SPECDEV)
	return ENODEV;
#endif /* AFS_SUNOS5_ENV */

#ifdef EXPRESS_OPS
    if (cm_ExpressPath(tscp, (struct cm_scache *) 0, &volp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_open(&vp1, flags, credp);
	cm_ExpressRele(volp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    /* don't let someone open a file for writing and exec.  But
     * if writing is off, we let FEXEC override FREAD.
     */
    execing = 0;
#ifdef AFS_AIX31_ENV
    if (flags & FEXEC)
	execing = 1;
#endif /* AFS_AIX31_ENV */
#ifdef AFS_SUNOS5_ENV
    if (osi_DoingExecCall())
	execing = 1;
#endif  /* AFS_SUNOS5_ENV */

    if (execing) {
	if (flags & (FWRITE|FTRUNC))
	    return EINVAL;
	tokens = TKN_OPEN_SHARED;
    } else {
	tokens = TKN_OPEN_READ;	/* for reading or writing */
    }

    if (flags & FTRUNC)	/* for cm_MarkTime, below */
	tokens |= TKN_STATUS_WRITE;
    if (flags & (FWRITE | FTRUNC)) {
	writing = 1;
	tokens |= TKN_OPEN_WRITE;
	if (tscp->states & SC_RO) {
	    return EROFS;
	}
    }
    if (tscp->states & SC_VDIR) {
	if (writing)  {
	    return EISDIR;
	}
	else {
	    return 0;
	}
    }
    if (cm_vType(tscp) == VDIR && writing)
	return EISDIR;

    cm_InitReq(&rreq, credp);

    if (code = cm_GetOLock(tscp, tokens, &rreq)) {
	/* file ID is bad by the time we get here, probably ESTALE
	 * gives a better idea of what went wrong.
	 */
	if (code == ENOENT)
	    code = ESTALE;
	return cm_CheckError(code, &rreq);
    }

    /* success, so entry is write-locked */

#ifndef AFS_SUNOS5_ENV
    if ((execing && tscp->writers > 0)
	|| (writing && tscp->shareds > 0)) {
	lock_ReleaseWrite(&tscp->llock);
	return cm_CheckError(ETXTBSY, &rreq);
    }
#endif /* AFS_SUNOS5_ENV */

    /*
     * open the file; still have the tokens from cm_GetToken call.  Must open
     * before releasing llock, or open revoke may occur
     */
    if (writing)
	tscp->writers++;
    else if (execing)
	tscp->shareds++;
    else
	tscp->readers++;
    tscp->opens++;

    /*
     * if we're writing, stash the opener's cred so that asynchronous
     * store backs are done on behalf of the opener
     */
    if (writing) {
	crhold(credp);		/* remember (possibly new) writer cred */
	if (tscp->writerCredp) crfree(tscp->writerCredp);
	tscp->writerCredp = credp;
    }

    /*
     * once we're here, we can't lose open tokens, since tkm server won't
     * give them up if the file is still open
     */
    if (code == 0 && (flags & FTRUNC)) {
	/*
	 * set date on file if open in O_TRUNC mode: this fixes touch
	 */
	/* set mtime and ctime */
	cm_MarkTime(tscp, 0, credp, CM_MODMOD | CM_MODCHANGE, 1);
    }

    /* initialize for default sequential access */

    cm_ResetSequentialHint(tscp);

    lock_ReleaseWrite(&tscp->llock);
    return cm_CheckError(code, &rreq);
}


/*
 * Close a file.
 *
 * cm_close follows the SunOS 3 specs (3 parameters).  SunOS 5 close is
 * implemented by a wrapper in the xvnode module (ns5_close).
 */
#ifdef SGIMIPS
int
cm_close(
    struct vnode *vp,
    long flags,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_close(vp, flags, credp)
    struct vnode *vp;
    long flags;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code = 0;
    afs_hyper_t type;
#ifdef EXPRESS_OPS
    struct volume *volp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    register struct cm_scache *scp;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_CLOSE, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, flags);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &volp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_close(vp1, flags, credp);
	cm_ExpressRele(volp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */
    if (scp->states & SC_VDIR) {
	return 0;
    }

    /*
     * make sure all dirty chunks are written back by the end of the
     * close call, so that we don't lose any error indications if something
     * goes wrong storing the data back to the server (e.g. disk full).
     * NOTE:
     * However, if we know the server is down, discard all dirty data and
     * make the scache invisible to new openers.
     */
    if (flags & (FWRITE | FTRUNC)) {
	/* cheap heuristic to limit our use of bkg store: allocate
	 * a call here; will hold us if we're over quota.
	 */
	krpc_PoolAlloc(&cm_concurrentCallPool, 0, 1);
	code = cm_SyncDCache(scp, 0, credp);
	krpc_PoolFree(&cm_concurrentCallPool, 1);
    }
    else code = 0;
    if (code) {
	if (!CM_ERROR_NOT_CLOSE_LIMITED(code)) {
	    lock_ObtainWrite(&scp->llock);
	    /* Catch this on the last writer, not the last opener; readers
	     * won't care about the error code.
	     */
	    if (code == ESTALE || scp->writers <= 1) {
		cm_MarkBadSCache(scp, code);
	    }
	    lock_ReleaseWrite(&scp->llock);
	}
    }

    lock_ObtainWrite(&scp->llock);
    /* don't drop open count until we have sent the data back, otherwise
     * someone could unlink the file while we're still using it, and
     * we'd get an ESTALE error code.
     */
    /* On Solaris, a vn_close() call from exec() will likely be for
     * close-on-exec files, not paired with  an earlier vn_open() call.
     * Therefore, we can't decrease the shareds count.
     */
    scp->opens--;
    if (flags & (FWRITE|FTRUNC))
	scp->writers--;
#ifdef AFS_AIX31_ENV
    else if (flags & FEXEC)
	scp->shareds--;
#endif /* AFS_AIX31_ENV */
    else
	scp->readers--;

#if defined (AFS_SUNOS5_ENV) || defined (SGIMIPS)
    /* Solaris has bugs where open counts can get mismatched.  Bullet-proof
     * this code for now against too many VOP_CLOSEs.  Also, note that
     * the glue is ignoring certain VOP_OPENs due to bugs in this area
     * in the exec code.
     */
    if (scp->opens < 0) scp->opens = 0;
    if (scp->writers < 0) scp->writers = 0;
    if (scp->shareds < 0) scp->shareds = 0;
    if (scp->readers < 0) scp->readers = 0;
#endif /* AFS_SUNOS5_ENV */

#ifdef SGIMIPS
    /* Bug: if a locked DFS file is not unlocked by a process before exit,
     * next lock request (with wait option) will hang forever. This is
     * because the * scache->lockList will not be knocked off and hence the
     * vnl_blocked() will think that it is still held by the old process.
     *
     * Fix: do a lockList cleanup before close.
     */
    if ( (void *) QNext(&scp->lockList) != (void *) &(scp->lockList) )
            vnl_cleanup(&scp->lockList);
#endif /* SGIMIPS */

    /* reset sequential hints */

    cm_ResetSequentialHint(scp);

    if (scp->opens == 0 && (scp->states & SC_RETURNOPEN)) {
	/*
	 * File is not active, put back all open tokens
	 */
	scp->states &= ~SC_RETURNOPEN;
	AFS_hset64(type, 0, (TKN_OPEN_READ | TKN_OPEN_WRITE |
			     TKN_OPEN_EXCLUSIVE | TKN_OPEN_SHARED));
	lock_ReleaseWrite(&scp->llock);
	cm_ReturnOpenTokens(scp, &type);
    } else
	lock_ReleaseWrite(&scp->llock);

#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}


/*
 * Get the max file size supported by the DFS server serving this scp.
 */
afs_hyper_t GetMaxSize(struct cm_scache *scp)
{
    struct cm_volume *volp;
    int i;

    if (volp = scp->volp)  {
        for(i = 0; i < CM_VL_MAX_HOSTS; i++)
            if (volp->serverHost[i])
                return(volp->serverHost[i]->maxFileSize);
    }
    /*
     * If we get here, just return the default max file size.
     */
    return(osi_maxFileSizeDefault);
}

/*
 * cm_rdwr, cm_nlrw, cm_write, cm_read, cm_strategy, cm_ustrategy --
 *	entry points for reading and writing
 *
 * We recognize two types of kernels, depending on the style in which VOP_RDWR
 * is supposed to be implemented:
 * - old-fashioned:  VOP_RDWR accesses disk buffers;
 * - VM-based:  VOP_RDWR maps the file to kernel memory and accesses the
 *		memory.
 *
 * In old-fashioned kernels, VOP_RDWR should be implemented by cm_rdwr, which
 * calls cm_write or cm_read as appropriate.
 *
 * In VM-based kernels, VOP_RDWR should be implemented by some other function,
 * for example cm_nfsrdwr in Solaris.  The kernel's VM manager is expected to
 * make calls back to the file system to read the file into VM when mapping it
 * (such as when handling page faults), and to write the file from VM when
 * necessary (such as when doing msync).  These calls should be implemented
 * by cm_nlrw or some function that calls cm_nlrw.  In Solaris, the calls from
 * the VM manager are implemented by cm_getpage and cm_putpage, each of which
 * calls cm_strategy, which calls cm_ustrategy, which calls cm_nlrw.
 *
 * In addition, there is a call to cm_read from cm_bread.  In old-fashioned
 * kernels, there may be a "strategy" function that is used to implement VM
 * (as in HP/UX) or a limited form of VM for management of text (as in
 * Berkeley 4.3).  This should be implemented by cm_strategy, which will call
 * cm_ustrategy, which will call cm_rdwr.
 *
 * cm_rdwr and cm_nlrw have specs that are different in significant ways.
 * Here are some crucial differences:
 * - cm_nlrw is called with the scache entry's llock held, and any dcp's that
 *   may be used are also assumed to exist, to be locked, and to be online.
 *   cm_rdwr is called with no locks held and does its own locking and its own
 *   calls to create and lock dcache entries and get them online.
 * - cm_rdwr, when writing, must check the file length limit, to comply with
 *   POSIX.  cm_nlrw does not (this is done at a higher level).
 * - cm_rdwr, when writing, must check the OSI_IOAPPEND flag and adjust the
 *   starting offset accordingly, to comply with POSIX.  cm_nlrw does not
 *   (this is done at a higher level).
 * - cm_rdwr must set the file's atime or its mtime and ctime, to comply with
 *   POSIX.  cm_nlrw does not (this is done at a higher level).
 * - We try to remember the credential of the last writer in the writerCredp
 *   field of the scache entry.  In old-fashioned kernels this is done in
 *   cm_write; in VM-based kernels it is done at a higher level.
 * - The write system call is expected to fail if there has been a failed store
 *   to the server that we have not successfully retried.  In old-fashioned
 *   kernels this check is in cm_write; in VM-based kernels it is done at a
 *   higher level.
 * - cm_write can fail due to some problem with writing to the cache.  If it
 *   is called from cm_rdwr, the dcache entry should be invalidated.  If it
 *   is called from cm_nlrw, the dcache entry should be left alone.  VM still
 *   contains valid data and until this is discarded or successfully written,
 *   the dcache entry must remain shipshape.
 *
 * For historical reasons, some of these differences are implemented by
 * #ifdef's (on AFS_SUNOS5_ENV), while others are implemented by runtime tests
 * of the nolock parameter in cm_write and cm_read.
 */

/*
 * cm_rdwr -- reading and writing for old-fashioned (non-VM-based) kernels
 */
#ifdef SGIMIPS 
int
cm_rdwr(
    struct vnode *vp,
    struct uio *uiop,
    enum uio_rw arw,
    int ioflag,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_rdwr(vp, uiop, arw, ioflag, credp)
    struct vnode *vp;
    struct uio *uiop;
    enum uio_rw arw;
    int ioflag;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code;
#ifdef EXPRESS_OPS
    struct volume *volp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    register struct cm_scache *scp;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &volp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_rdwr(vp1, uiop, arw, ioflag, credp);
	cm_ExpressRele(volp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    if (scp->states & SC_VDIR) {
	return EISDIR;
    }
    if (arw == UIO_WRITE)
	code = cm_write(scp, uiop, ioflag, credp, 0);
    else
#ifdef SGIMIPS
	code = cm_doio(scp, uiop, ioflag, credp, UIO_READ);
    return (int)code;
#else  /* SGIMIPS */
	code = cm_read(scp, uiop, credp, 0, 0, 0);
    return code;
#endif /* SGIMIPS */
}

#ifdef SGIMIPS
/* Under IRIX, we need to handle the calls to rd/wr differently.  All
 * reads and writes go through the buffer cache which invokes the
 * actual cm_read and cm_write via the strategy routine.
 *
 * read(2)                              write(2)
 *  VOP_READ()                           VOP_WRITE()
 *   cm_rdwr()                            cm_rdwr()
 *    cm_doio()                            cm_write()
 *     chunkread()                          bwrite()
 *      .                                    .
 *      .                                    .
 *      .                                    .
 *       cm_strategy()                        cm_strategy()
 *        osi_MapStrategy()                    osi_MapStrategy()
 *         cm_ustrategy()                       cm_ustrategy()
 *          cm_read()                            cm_write_strat()
 *
 * For memory mapped files, all io is through the strategy routine.
 */
static int cm_doio (
    struct cm_scache *scp,
    struct uio *uiop,
    int ioflag,
    osi_cred_t *credp,
    enum uio_rw arw)
{
    int off, cnt, error, eof;
    afs_hyper_t diff, olen;
    struct vnode *vp;
    struct bmapval bmv[2];
    struct buf *bp;
    struct cm_rrequest rreq;

    vp = SCTOV (scp);
    error = 0;
    eof = 0;

    icl_Trace2(cm_iclSetp, CM_TRACE_READ, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, 2);

    if (!(ioflag & IO_ISLOCKED))
        cm_rwlock(SCTOV(scp), VRWLOCK_READ);

    cm_InitReq(&rreq, credp);

    /* We have been invoked via VOP_READ() */
    lock_ObtainRead(&scp->llock);
    olen = scp->m.Length;

    do {
	error = (int)cm_GetTokens(scp, TKN_STATUS_READ, &rreq, READ_LOCK);
	if (error)
	    break;

	off = (int)(uiop->uio_offset % DFS_BLKSIZE);
	cnt = (int) MIN((unsigned)(DFS_BLKSIZE - off), uiop->uio_resid);
	diff = olen - uiop->uio_offset;
	if (diff <= 0) {
	    break;
	}
	if (diff < cnt) {
	    cnt = diff;
	    eof = 1;
	}
	  
	bmv[0].bn = bmv[0].offset = OFFTOBBT(uiop->uio_offset - off);
	bmv[0].length = BTOBBT(DFS_BLKSIZE);
	bmv[0].eof = 0;
	bmv[0].pboff = off;
	bmv[0].bsize = DFS_BLKSIZE;
	bmv[0].pbsize = cnt;
	bmv[0].pmp = uiop->uio_pmp;
	/*
	 * Set BMAP_DELAY to cause the buffer cache to treat DFS buffers
	 * as DELALLOC buffers.  This helps insure that there will be
	 * buffers available for other filesystems.
	 */
	bmv[0].eof = BMAP_DELAY;

	/* when lock is dropped, status including length, can change! */
	lock_ReleaseRead(&scp->llock);

	bp = chunkread (vp, bmv, 1, credp);

	lock_ObtainRead(&scp->llock);

	if (bp->b_flags & B_ERROR) {
	    error = geterror(bp);
	    buftrace("DFS DIOERROR", bp);
	    bp->b_flags &= ~(B_READ|B_DELWRI);
	    bp->b_flags |= B_STALE | B_DONE;
	    brelse(bp);
	    break;
	}

	/* check to see if read was truncated when llock was dropped */
	if (olen > scp->m.Length) {
	   diff = scp->m.Length - uiop->uio_offset;
	   if (diff <= 0) {
	       brelse(bp);
	       eof = 1;
	       goto tosspages;
	   }
	   if (diff < cnt) {
	       cnt = diff;
	       eof = 1;
	   }
	}

	/* move data from buffer to uio */
#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
	    error = biomove (bp, bmv[0].pboff, cnt, arw, uiop);
	}
	else 
#endif /* _VCE_AVOIDANCE */
	{
	    (void) bp_mapin(bp);
	    error = uiomove(bp->b_un.b_addr + bmv[0].pboff, cnt, arw, uiop);
	}
	brelse(bp);

tosspages:
	if (olen > scp->m.Length) {
	    /*
	     * Must toss truncated data when the file length changed
	     * to prevent data corruption.  cm_MergeStatus() may have
	     * truncated the file length, and it cannot do the toss.
	     * The buffer must be released before the toss can be done.
	     */
	    diff = scp->m.Length;
	    lock_ReleaseRead(&scp->llock);
	    SC_STARTED_FLUSHING_ON(scp, error)
	    if (!error) {
		VOP_TOSS_PAGES(SCTOV(scp), diff, SZTODFSBLKS(olen) - 1,
			FI_REMAPF_LOCKED);
		SC_STARTED_FLUSHING_OFF(scp);
	    }
	    lock_ObtainRead(&scp->llock);
	}
    } while (!error && uiop->uio_resid > 0 && !eof);

    if (!(ioflag & IO_ISLOCKED))
        cm_rwunlock(SCTOV(scp), VRWLOCK_READ);
    lock_ReleaseRead(&scp->llock);

    return (error);
}

/* The new IRIX cm_write */
/*
 * cm_write -- writing from user space or VM to the cache
 */
static int cm_write(
    struct cm_scache *scp,
    struct uio *uiop,
    int ioflag,
    osi_cred_t *credp,
    int noLock)
{
    long totalLength,  startDate, max;
    off_t filePos;
    long offset, len, csize, error=0, code=0;
    register struct cm_dcache *dcp;
    ssize_t adjusted = 0;
    int  wroteLastByte;
    long lastByteOffset;
    struct uio tuio;
    struct cm_rrequest rreq;
    
    struct buf *bp = NULL;
    struct bmapval bmv[2];
    struct vnode *vp = SCTOV(scp);
    int size = DFS_BLKSIZE;
    int off, cnt;
    afs_hyper_t biggestSrvFile;
    int convert=0;

    icl_Trace2(cm_iclSetp, CM_TRACE_WRITE, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, ioflag);

    if (!(ioflag & IO_ISLOCKED))
        cm_rwlock(vp, VRWLOCK_WRITE);

    if (scp->states & SC_RO) {
	code = EROFS;
	goto error0;
    }

    cm_InitReq(&rreq, credp);

    /*
     * totalLength bounds the amount we'll grow this file
     */
    startDate = osi_Time();
    code = cm_GetSLock(scp, TKN_UPDATESTAT, CM_SLOCK_WRDATAONLY,
			   WRITE_LOCK, &rreq);
    if (code) {
	convert = 1;
	error = code;
	goto error0;
    }


    /* check for failed store; this check done elsewhere for VM systems */
    if (scp->states & SC_STOREFAILED) {
	code = scp->storeFailStatus;
	lock_ReleaseWrite(&scp->llock);
	goto error0;
    }

    if (ioflag & OSI_FAPPEND) {
	/* append mode, start it at the right spot */
	uiop->osi_uio_offset = scp->m.Length;
    }

    filePos = uiop->osi_uio_offset;
    if (code = scp->asyncStatus) {
	lock_ReleaseWrite(&scp->llock);
	goto error0;
    }

    if (filePos + uiop->osi_uio_resid > osi_getufilelimit ()) {
        if (filePos >= osi_getufilelimit ()) {
            lock_ReleaseWrite (&scp->llock);
	    code = EFBIG;
	    goto error0;
        } else {
            adjusted = uiop->osi_uio_resid;
            uiop->osi_uio_resid = (long) (osi_getufilelimit () - filePos);
            adjusted -= uiop->osi_uio_resid;
        }
    }

    /*
     * 32/64-bit interoperability changes:
     *
     * Check for exceeding file server size limit by using the maxFileSize
     * parameter in struct cm_server.  This parameter is obtained from the
     * server via the AFS_SetParams call.
     */
    biggestSrvFile = GetMaxSize(scp);
    if (filePos + uiop->osi_uio_resid > biggestSrvFile) {
        if (filePos >= biggestSrvFile) {
            lock_ReleaseWrite (&scp->llock);
	    code = EFBIG;
	    goto error0;
        } else {
            adjusted += uiop->osi_uio_resid;
            uiop->osi_uio_resid = biggestSrvFile - filePos;
            adjusted -= uiop->osi_uio_resid;
        }
    }

    /* If we're not on Solaris (not VM-integrated) we'll set the writer cred
     * here so that writes due to NFS set the credential.  If we are
     * VM-integrated, we don't want the pageout daemon to stuff its
     * credentials in `writerCredp'.
     */
    crhold(credp);		/* remember (possibly new) writer cred */
    if (scp->writerCredp) crfree(scp->writerCredp);
    scp->writerCredp = credp;

    totalLength = uiop->osi_uio_resid;
    /*
     * Avoid counting lock-waiting time in file date (for ranlib)
     * Don't bother getting token, since we've just gotten it.
     * N.B. cm_MarkTime sets scp->writerCredp for us.
     */
    cm_MarkTime(scp, startDate, credp, CM_MODMOD|CM_MODCHANGE, 1);
    lock_ReleaseWrite(&scp->llock);

    while (totalLength > 0) {
	/*
	 * read the cached info.  Note that since this call may make an
	 * RPC to get the data, we may release the llock on the cache
	 * item while we're in there.
	 */
	wroteLastByte = 0;
	/*
	 * Call cm_GetDCache until the data is "online" and we
	 * have a write token and the scp->llock write locked.
	 * There is a race between the time GetDCache returns
	 * and the time we obtain the lock where the token could be
	 * lost and the data marked offline and/or the write token is lost
	 */
	dcp = cm_GetDCache(scp, filePos);
        if (!dcp) {
            lock_ObtainRead(&scp->llock);
            if (code = scp->asyncStatus) {
                lock_ReleaseRead(&scp->llock);
                goto error0;
            }
            else {
                panic("read no dcp");
            }
        }
	offset = cm_chunkoffset(filePos);
	csize = offset + totalLength;

	if (csize > cm_chunksize(filePos))
	    csize = cm_chunksize(filePos);

	off = (int)(filePos % size);
	cnt = (int)MIN((unsigned) (size - off), totalLength);

	bmv[0].bn = bmv[0].offset = OFFTOBBT(filePos - off);
	bmv[0].length = (int)BTOBBT(size);
	bmv[0].pboff = off;
	bmv[0].bsize = size;
	bmv[0].pbsize = cnt;
	bmv[0].pmp = uiop->uio_pmp;
	/*
	 * Set BMAP_DELAY to cause the buffer cache to treat DFS buffers
	 * as DELALLOC buffers.  This helps insure that there will be
	 * buffers available for other filesystems.
	 */
	bmv[0].eof = BMAP_DELAY;

	if ((cnt != size) && (off != 0 || filePos != scp->m.Length)) {
	    bp = chunkread (vp, bmv, 1, credp);
	} else {
	    /* 
	     * Skip unnecessary read and bzero() on the portion of the
	     * file being overwritten.  We must still bzero() unwritten 
	     * portions of new buffers. 
	     * The cnt==size case requires no bzero().
	     */
	    bp = getchunk (vp, bmv, credp);

	    if (!(bp->b_flags & B_ERROR) && cnt != size) {
		int end = off + cnt;
		(void) bp_mapin(bp);
		ASSERT(off >= 0 && off < size);
                ASSERT(cnt > 0 && cnt <= size);
                ASSERT(end <= size);
		if (off != 0)
			bzero(bp->b_un.b_addr, off);
		if (end < size)
			bzero(bp->b_un.b_addr + end, size - end);
	    }
	}

        if (bp->b_flags & B_ERROR) {
		code = geterror(bp);
        	buftrace("DFS CMW IOERRELSE", bp);
        	bp->b_flags &= ~(B_READ|B_DELWRI);
        	bp->b_flags |= B_STALE;
                brelse(bp);
                bp = NULL; /* Done with bp clear */
                break;
        }

	/*
	 * Get dcache entry on line with enough space reserved from
	 * cache to hold the write.  If locks were dropped during
	 * cache reservation, then try again until the entry can be
	 * put online with reserved space without releasing locks
	 */
	lock_ObtainWrite(&scp->llock);
	for (;;) {
	    error = cm_GetDLock(scp, dcp, CM_DLOCK_WRITE, WRITE_LOCK, &rreq);
	    if (error) {
		lock_ObtainWrite(&dcp->llock);
		cm_AdjustSize(dcp, 0);
		lock_ReleaseWrite(&dcp->llock);
		cm_PutDCache(dcp);
		lock_ReleaseWrite(&scp->llock);
		convert = 1;
		goto out;
	    }
	    if (!cm_ReserveBlocks(scp, dcp, csize))
		break;		/* all conditions met, exit loop */
	    lock_ReleaseWrite(&dcp->llock);	/* must try again */
	}	/* end for (;;) */
	cm_ComputeOffsetInfo(scp, dcp, filePos, &offset, &len);
	/* dcache is write locked, online flag won't go away
	 * until dcache llock is released.
	 */

	/*
	 * Now have the data chunk locked and the proper
	 * tokens to complete the write.
	 */

	cm_SetDataMod(scp, dcp, credp);
	dcp->f.states |= DC_DWRITING;
	cm_SetEntryMod(dcp);
	len = cnt;				/* write this amount by default */
	max = cm_chunktosize(dcp->f.chunk);	/* max size of this chunk */
	/*
	 * if we'd go past the end of this chunk
	 */
	if (max <= len + offset) {
	    /*
	     * It won't all fit in this chunk, so write as much	as will fit
	     */
	    len = max - offset;
	    wroteLastByte = 1;
	    lastByteOffset = cm_chunktobase(dcp->f.chunk);
	    icl_Trace2(cm_iclSetp, CM_TRACE_WRITEOVL, ICL_TYPE_LONG, max,
		       ICL_TYPE_LONG, len);
	}

	/* now, update the file size for this chunk.  We must do it
	 * before dirtying the chunk, so that any asynchronous store-back
	 * will store back a length that includes any dirty chunks
	 * stored back.  Otherwise, the stored-back chunk will be truncated
	 * by the server.
	 */
	if (filePos+len > scp->m.Length) {
	    scp->m.Length = filePos+len;
	    scp->m.ModFlags |= CM_MODLENGTH;
	}

	lock_ReleaseWrite(&scp->llock);
#ifdef _VCE_AVOIDANCE
        if (vce_avoidance) {
            error = biomove (bp, bmv[0].pboff, cnt, UIO_WRITE, uiop);
        } else
#endif /* _VCE_AVOIDANCE */
        {
            (void) bp_mapin(bp);
            error = uiomove(bp->b_un.b_addr + bmv[0].pboff, cnt, UIO_WRITE, uiop);
        }

	if (error) {
	    icl_Trace4(cm_iclSetp, CM_TRACE_CM_WRITE_LCLNOGO,
		       ICL_TYPE_POINTER, scp,
		       ICL_TYPE_POINTER, dcp,
		       ICL_TYPE_LONG, code,
		       ICL_TYPE_LONG, tuio.osi_uio_resid);
	    /* need to stabilize, and also need scache lock */
	    lock_ReleaseWrite(&dcp->llock);
	    lock_ObtainWrite(&scp->llock);
	    lock_ObtainWrite(&dcp->llock);
	    cm_InvalidateOneSegment(scp, dcp, 1);
	    lock_ReleaseWrite(&scp->llock);
	    lock_ReleaseWrite(&dcp->llock);	/* release write lock */
	    cm_PutDCache(dcp);
	    break;
	}

        bp->b_fsprivate = (void *)dcp;

	code = bwrite(bp);
	bp = NULL; /* Done with buffer, clear bp */

	totalLength = uiop->uio_resid;
	filePos = uiop->uio_offset;
	lock_ReleaseWrite(&dcp->llock);

	cm_PutDCache(dcp);
	/* determine if we should store the data asynchronously */
	if (wroteLastByte) {
	    code = cm_DoPartialWrite(scp, lastByteOffset, &rreq, credp);
	    if (code) {
		icl_Trace3(cm_iclSetp, CM_TRACE_CM_WRITE_STOREIT,
			       ICL_TYPE_POINTER, scp,
			       ICL_TYPE_LONG, lastByteOffset,
			       ICL_TYPE_LONG, code);
		error = code;
		break;
	    }
	}
    }	/* the big while loop */
  out:
    if (bp) {
        /* bp still set - must have had a failure and abandoned write,
         * discard buffer.  Set B_STALE and B_ERROR regardless of
         * whether or not B_DONE is set.
         */
        buftrace("DFS CMOUT IOERRELSE", bp);
        bp->b_flags &= ~(B_READ|B_DELWRI);
        bp->b_flags |= B_STALE|B_ERROR|B_DONE;
        brelse(bp);
    }

    if (!error && adjusted)
	uiop->osi_uio_resid = adjusted;

error0:
    if (!(ioflag & IO_ISLOCKED))
        cm_rwunlock(vp, VRWLOCK_WRITE);

    if (convert)
    	return cm_CheckError(error, &rreq);
    else
	return ((int)code);
}

/* 
 * called on writes 
 */
static int cm_strat_write(
    struct cm_scache *scp,
    struct cm_dcache *dcp,
    struct uio *uiop,
    int ioflag,
    osi_cred_t *credp) 
{
    long totalLength, startDate = osi_Time(), max;
    off_t filePos;
    long offset, len, error=0, code=0;
    int wroteLastByte;
    long adjusted = 0;
    long lastByteOffset;
    struct uio tuio;
    struct iovec *iovecp; 
    char *osifilep;
    struct cm_rrequest rreq;
    int	called_from_write = (dcp != NULL);
    afs_hyper_t biggestSrvFile;


    if (scp->writerCredp) credp = scp->writerCredp;

    cm_InitReq(&rreq, credp);

    icl_Trace2(cm_iclSetp, CM_TRACE_WRITE, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, ioflag);
    if (scp->states & SC_RO) {
	return cm_CheckError(EROFS, &rreq);
    }

    /*
     * totalLength bounds the amount we'll grow this file
     */
    if (ioflag & OSI_FAPPEND) {
        /*
         * append mode, start it at the right spot
         */
        uiop->osi_uio_offset = scp->m.Length;
    }
    filePos = uiop->osi_uio_offset;
    if (code = scp->asyncStatus) {
	return (int)code;
    }

    /* don't bother checking on Solaris; we do this in VM version */
    if (filePos + uiop->osi_uio_resid > osi_getufilelimit ()) {
        if (filePos >= osi_getufilelimit ()) {
            return (EFBIG);
        } else {
            adjusted = uiop->osi_uio_resid;
            uiop->osi_uio_resid = (long) (osi_getufilelimit () - filePos);
            adjusted -= uiop->osi_uio_resid;
        }
    }

    /*
     * 32/64-bit interoperability changes:
     *
     * Check for exceeding file server size limit by using the maxFileSize
     * parameter in struct cm_server.  This parameter is obtained from the
     * server via the AFS_SetParams call.
     */
    biggestSrvFile = GetMaxSize(scp);
    if (filePos + uiop->osi_uio_resid > biggestSrvFile) {
        if (filePos >= biggestSrvFile) {
            return(EFBIG);
        } else {
            adjusted += uiop->osi_uio_resid;
            uiop->osi_uio_resid = biggestSrvFile - filePos;
            adjusted -= uiop->osi_uio_resid;
        }
    }


    totalLength = uiop->osi_uio_resid;
    if (!called_from_write) {
	code = cm_GetSLock(scp, TKN_UPDATESTAT, CM_SLOCK_WRDATAONLY,
                       	WRITE_LOCK, &rreq);
        if (code) {
        	return cm_CheckError(code, &rreq);  /* failed, punt */
	}
        /* avoid counting lock-waiting time in file date (for ranlib)
         * Don't bother getting token, since we've just gotten it.
         * N.B. cm_MarkTime sets scp->writerCredp for us.
         */
        cm_MarkTime(scp, startDate, credp, CM_MODMOD|CM_MODCHANGE, 0);
	lock_ReleaseWrite(&scp->llock);
    }
    iovecp = (struct iovec *) osi_AllocBufferSpace();

    while (totalLength > 0) {
	/* 
	 * read the cached info.  Note that since this call may make an
	 * RPC to get the data, we may release the llock on the cache
	 * item while we're in there.
	 */
	wroteLastByte = 0;
	if (!called_from_write) {
	    if (!(dcp = cm_FindDCache(scp, filePos))) {
		/*  
		 * We might have been called from a msync.  Thus
		 * there might be data in pages from writing to a memory
		 * mapped file.  This data might be for parts of the file
		 * that are not currently held in our online dcache.
		 */

		dcp = cm_GetDCache(scp, filePos);
                if (!dcp) {
                    lock_ObtainRead(&scp->llock);
                    if (code = scp->asyncStatus) {
                        lock_ReleaseRead(&scp->llock);
                        return code;
                    }
                    else {
                        panic("cm_strat_write read no dcp");
                    }
                }
		offset = filePos - cm_chunktobase(dcp->f.chunk);
		len = dcp->f.chunkBytes - offset;

		/*
         	 * Get dcache entry on line with enough space reserved from
          	 * cache to hold the write.  If locks were dropped during
         	 * cache reservation, then try again until the entry can be
         	 * put online with reserved space without releasing locks
         	 */
		lock_ObtainWrite(&scp->llock);
        	while (1) {
            		error = cm_GetDLock(scp, dcp, CM_DLOCK_WRITE, 
					WRITE_LOCK, &rreq);
            		if (error) {
                		lock_ObtainWrite(&dcp->llock);
                		cm_AdjustSize(dcp, 0);
                		lock_ReleaseWrite(&dcp->llock);
                		cm_PutDCache(dcp);
                		lock_ReleaseWrite(&scp->llock);
                		goto stratout;
            		}
            		if (cm_ReserveBlocks(scp, dcp, len)) {
                		lock_ReleaseWrite(&dcp->llock);
                		continue;               /* must try again */
            		}
            		break;      /* all conditions met, exit loop */
        	}       /* end while(1) */
	    }
	    else {
		lock_ObtainWrite(&scp->llock);
		lock_ObtainWrite(&dcp->llock);
	    }
	}

	if (!dcp) {
	    error = EIO;
	    break;
	}
	offset = (long) (filePos - cm_chunktobase(dcp->f.chunk));

	/*
	 * Now have the data chunk locked and the proper 
	 * tokens to complete the write.
	 */
	if (!called_from_write) {
	    cm_SetDataMod(scp, dcp, credp);
	    dcp->f.states |= DC_DWRITING;
	    cm_SetEntryMod(dcp);
	}
	len = totalLength;		/* write this amount by default */
	max = cm_chunktosize(dcp->f.chunk);	/* max size of this chunk */
	/*
	 * if we'd go past the end of this chunk 
	 */					
	if (max	<= len + offset) {  
	    /* 
	     * It won't all fit in this chunk, so write as much	as will fit 
	     */
	    len = max - offset;
	    wroteLastByte = 1;
	    lastByteOffset = cm_chunktobase(dcp->f.chunk);
	    icl_Trace2(cm_iclSetp, CM_TRACE_WRITEOVL, ICL_TYPE_LONG, max,
		       ICL_TYPE_LONG, len);
	}

	/* now, update the file size for this chunk.  We must do it
	 * before dirtying the chunk, so that any asynchronous store-back
	 * will store back a length that includes any dirty chunks
	 * stored back.  Otherwise, the stored-back chunk will be truncated
	 * by the server.
	 */
	if (!called_from_write) {
	    if (filePos+len > scp->m.Length) {
		scp->m.Length = filePos+len;
		scp->m.ModFlags |= CM_MODLENGTH;
	    }
	    lock_ReleaseWrite(&scp->llock);
	}
	cm_SetChunkDirtyRange(dcp, offset, offset+len);

#ifdef AFS_VFSCACHE
        if (!(osifilep = cm_CFileOpen(&dcp->f.handle)))
#else
        if (!(osifilep = cm_CFileOpen(dcp->f.inode)))
#endif /* AFS_VFSCACHE */
	    panic("afswrite open");

	/* 
	 * mung uio structure to be right for this transfer 
	 */
	
	osi_uio_copy(uiop, &tuio, iovecp);
	osi_uio_trim(&tuio, len);
	tuio.osi_uio_offset = offset;
	code = cm_CFileRDWR(osifilep, UIO_WRITE, &tuio, osi_credp);
	if (code || (tuio.osi_uio_resid > 0)) {
	    error = code;
	    if (error == 0) error = EIO;	/* didn't write all! */
	     /* need to stabilize, and also need scache lock */
	    lock_ReleaseWrite(&dcp->llock);
	    lock_ObtainWrite(&scp->llock);
	    lock_ObtainWrite(&dcp->llock);
	    cm_InvalidateOneSegment(scp, dcp, 0);
	    cm_CFileTruncate(osifilep, 0);	/* fake truncate the segment */
	    cm_AdjustSize(dcp, 0);
	    lock_ReleaseWrite(&scp->llock);
	    if (!called_from_write) {
		cm_PutDCache(dcp);
	    }
	    cm_CFileClose(osifilep);
	    if (!called_from_write) {
		lock_ReleaseWrite(&dcp->llock); /* release write lock */
	    }
	    break;
	}
	/*
	 * Otherwise we've written some, fixup length, etc and 
	 * continue with next seg 
	 */
	osi_uio_skip(uiop, len);/*advance uiop over data written*/
	if (offset + len > dcp->f.chunkBytes)	/* compute new file size */
	    cm_AdjustSize(dcp, offset+len);
	totalLength -= len;
	filePos += len;
	cm_CFileClose(osifilep);
	if (!called_from_write) {
	    lock_ReleaseWrite(&dcp->llock);
	    cm_PutDCache(dcp);
	}
	/* determine if we should store the data asynchronously */
	if (!called_from_write && wroteLastByte) {
	        /* can't do it ourselves, since vp is locked, but can
	         * ask daemon to do it.  So, try that, at least, to get
	         * some asynchrony on storebacks.
	         */
		ASSERT(SCTOV(scp)->v_count > 0);
	        (void)cm_bkgQueue(BK_STORE_OP, scp, 0, credp, lastByteOffset,
			          0, 0, 0, 0);
	}
    }	/* the big while loop */
  stratout:
    osi_FreeBufferSpace((struct osi_buffer *)iovecp);
    if (!error && adjusted)
	uiop->osi_uio_resid = adjusted;	
    return cm_CheckError(error, &rreq);
}
#endif /* SGIMIPS */


/*
 * cm_nlrw -- reading and writing for VM-based kernels
 *
 * Like cm_rdwr, but assumes that all appropriate locks have been set.
 * Only does the work it can without releasing the locks it has.
 */

/*
Set this flag to calculate whether access to files is sequential
or random. Clear to disable the calculation.
*/

long enable_prefetch=1;

/*
How many chunks to prefetch. Set to 0 to disable prefetch.
Tune by benchmarking.
*/

long hot_prefetch=2;

#ifndef SGIMIPS 
cm_nlrw(vp, uiop, rwflag, ioflag, credp)
    struct vnode *vp;
    struct uio *uiop;
    enum uio_rw rwflag;
    int ioflag;
    osi_cred_t *credp;
{
    register long code;
#ifdef EXPRESS_OPS
    struct volume *volp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    struct cm_scache *scp;

        long filePos = uiop->osi_uio_offset;
        long totalLength = uiop->osi_uio_resid;

    scp = VTOSC(vp);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &volp, &vp1,
                      (struct vnode **) 0)) {
	code = cmglue_nlrw(vp1, uiop, rwflag, ioflag, credp);
	cm_ExpressRele(volp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

/* for SUNOS5 see cm_sun5vm.c */

#ifndef AFS_SUNOS5_ENV

    /* figure out whether we have seq or random access */

    if(enable_prefetch && !scp->seq_hint.set_prefetch)
	cm_SequentialHint(scp,filePos,totalLength);

#endif

    if (rwflag == UIO_WRITE)
	code = cm_write(scp, uiop, ioflag, credp, 1);
    else
	code = cm_read(scp, uiop, credp, 0, 0, 1);
    return code;
}


/*
 * cm_write -- writing from user space or VM to the cache
 */
static int cm_write(scp, uiop, ioflag, credp, noLock)
    struct cm_scache *scp;
    struct uio *uiop;
    int ioflag;
    osi_cred_t *credp;
    int noLock;
{
    long totalLength, filePos, startDate, max;
    long offset, len, csize, error=0, code=0;
    register struct cm_dcache *dcp;
    int adjusted = 0, wroteLastByte;
    long lastByteOffset;
    struct uio tuio;
    struct iovec *iovecp;
    char *osifilep;
    struct cm_rrequest rreq;

    icl_Trace2(cm_iclSetp, CM_TRACE_WRITE, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, ioflag);

    if (scp->states & SC_RO) 
	return EROFS;

#ifdef AFS_SUNOS5_ENV
    /* if a SPECDEV scache vnode makes it this far, it's not
     * a real device.
     */
    if (scp->states & SC_SPECDEV)
	return ENODEV;
#endif /* AFS_SUNOS5_ENV */

    cm_InitReq(&rreq, credp);

    /*
     * totalLength bounds the amount we'll grow this file
     */
    if (!noLock) {
	startDate = osi_Time();
	code = cm_GetSLock(scp, TKN_UPDATESTAT, CM_SLOCK_WRDATAONLY,
			   WRITE_LOCK, &rreq);
	if (code) {
	    return cm_CheckError(code, &rreq);	/* failed, punt */
	}
    }

#ifndef AFS_SUNOS5_ENV
    /* check for failed store; this check done elsewhere for VM systems */
    if (scp->states & SC_STOREFAILED) {
	code = scp->storeFailStatus;
	if (!noLock) lock_ReleaseWrite(&scp->llock);
	return code;
    }

    if (ioflag & OSI_FAPPEND) {
	/* append mode, start it at the right spot */
	uiop->osi_uio_offset = scp->m.Length;
    }
#endif /* !AFS_SUNOS5_ENV */

    filePos = uiop->osi_uio_offset;
    if (code = scp->asyncStatus) {
	if (!noLock) lock_ReleaseWrite(&scp->llock);
	return code;
    }

    /*
     * 32/64-bit interoperability changes:
     *
     * Check for exceeding file server size limit by using the maxFileSize
     * parameter in struct cm_server.  This parameter is obtained from the
     * server via the AFS_SetParams call.
     */
    {	afs_hyper_t biggestSrvFile, biggestFile;
	afs_hyper_t firstByte, lastByte;
	afs_hyper_t xfer;		/* bytes legal to transfer */
	afs_hyper_t left;		/* bytes untranserable */

	/* First get local limit.  This limit is also checked at a higher level
         * in VM systems. */
#ifdef AFS_SUNOS5_ENV
	AFS_hset64(biggestFile, 0, uiop->uio_limit);
#else  
	AFS_hset64(biggestFile, 0, osi_getufilelimit());
#endif 

	/* The get remote limit and max with local limit. */
	biggestSrvFile = GetMaxSize(scp);
	if (AFS_hcmp(biggestFile, biggestSrvFile) > 0)
	    biggestFile = biggestSrvFile;

	AFS_hset64(firstByte, 0, uiop->osi_uio_offset);
	lastByte = firstByte;
	AFS_hadd32(lastByte, uiop->osi_uio_resid);

	if (AFS_hcmp(lastByte, biggestFile) > 0) {
	    if (AFS_hcmp(firstByte, biggestFile) >= 0) {
		if (!noLock)
		    lock_ReleaseWrite(&scp->llock);
		return (EFBIG);
	    }
	    xfer = biggestFile;
	    AFS_hsub(xfer, firstByte);
	    left = lastByte;
	    AFS_hsub(left, biggestFile);

	    /* XXX -- 32bit only code. */
	    uiop->osi_uio_resid = AFS_hgetlo(xfer);
	    adjusted = AFS_hgetlo(left);
	}
    }

#ifndef AFS_SUNOS5_ENV
    /* If we're not on Solaris (not VM-integrated) we'll set the writer cred
     * here so that writes due to NFS set the credential.  If we are
     * VM-integrated, we don't want the pageout daemon to stuff its
     * credentials in `writerCredp'.
     */
    crhold(credp);		/* remember (possibly new) writer cred */
    if (scp->writerCredp) crfree(scp->writerCredp);
    scp->writerCredp = credp;
#endif /* not AFS_SUNOS5_ENV */

    totalLength = uiop->osi_uio_resid;
    /*
     * Avoid counting lock-waiting time in file date (for ranlib)
     * Don't bother getting token, since we've just gotten it.
     * N.B. cm_MarkTime sets scp->writerCredp for us.
     */
    if (!noLock) {
        cm_MarkTime(scp, startDate, credp, CM_MODMOD|CM_MODCHANGE, 1);
	lock_ReleaseWrite(&scp->llock);
    }
    iovecp = (struct iovec *) osi_AllocBufferSpace();

    while (totalLength > 0) {
	/*
	 * read the cached info.  Note that since this call may make an
	 * RPC to get the data, we may release the llock on the cache
	 * item while we're in there.
	 */
	wroteLastByte = 0;
	if (noLock) {
	    dcp = cm_FindDCache(scp, filePos);
	    /*
	     * The universal assumption is that if there are VM pages, they
	     * must be backed by a valid dcache entry.  So we do not create
	     * a new dcache entry, nor do we check whether the existing one
	     * is online.
	     */
	    if (!dcp) {
		/* This can't really happen */
		icl_Trace2(cm_iclSetp, CM_TRACE_CM_WRITE_NODCACHE,
			   ICL_TYPE_POINTER, scp,
			   ICL_TYPE_LONG, filePos);
		error = EIO;
		break;
	    }
	    offset = filePos - cm_chunktobase(dcp->f.chunk);
	} else {	/* nolock */
	    /*
	     * Call cm_GetDCache until the data is "online" and we
	     * have a write token and the scp->llock write locked.
	     * There is a race between the time GetDCache returns
	     * and the time we obtain the lock where the token could be
	     * lost and the data marked offline and/or the write token is lost
	     */
	    dcp = cm_GetDCache(scp, filePos);
	    if (!dcp) panic("read no dcp");
	    offset = cm_chunkoffset(filePos);
	    csize = offset + totalLength;
	    if (csize > cm_chunksize(filePos))
		csize = cm_chunksize(filePos);
	    /*
	     * Get dcache entry on line with enough space reserved from
	     * cache to hold the write.  If locks were dropped during
	     * cache reservation, then try again until the entry can be
	     * put online with reserved space without releasing locks
	     */
	    lock_ObtainWrite(&scp->llock);
	    for (;;) {
		error = cm_GetDLock(scp, dcp, CM_DLOCK_WRITE, 
		    WRITE_LOCK, &rreq);
		if (error) {
		    lock_ObtainWrite(&dcp->llock);
		    cm_AdjustSize(dcp, 0);
		    lock_ReleaseWrite(&dcp->llock);
		    cm_PutDCache(dcp);
		    lock_ReleaseWrite(&scp->llock);
		    goto out;
	        }
		if (!cm_ReserveBlocks(scp, dcp, csize))
		    break;		/* all conditions met, exit loop */
		lock_ReleaseWrite(&dcp->llock);	/* must try again */
	    }	/* end for (;;) */
	    cm_ComputeOffsetInfo(scp, dcp, filePos, &offset, &len);
	    /* dcache is write locked, online flag won't go away
	     * until dcache llock is released.
	     */
	}	/* nolock */


	/*
	 * Now have the data chunk locked and the proper
	 * tokens to complete the write.
	 */
	cm_SetDataMod(scp, dcp, credp);
	dcp->f.states |= DC_DWRITING;
	cm_SetEntryMod(dcp);
	len = totalLength;		/* write this amount by default */
	max = cm_chunktosize(dcp->f.chunk);	/* max size of this chunk */
	/*
	 * if we'd go past the end of this chunk
	 */
	if (max <= len + offset) {
	    /*
	     * It won't all fit in this chunk, so write as much	as will fit
	     */
	    len = max - offset;
	    wroteLastByte = 1;
	    lastByteOffset = cm_chunktobase(dcp->f.chunk);
	    icl_Trace2(cm_iclSetp, CM_TRACE_WRITEOVL, ICL_TYPE_LONG, max,
		       ICL_TYPE_LONG, len);
	}

	/* now, update the file size for this chunk.  We must do it
	 * before dirtying the chunk, so that any asynchronous store-back
	 * will store back a length that includes any dirty chunks
	 * stored back.  Otherwise, the stored-back chunk will be truncated
	 * by the server.
	 */
	if (filePos+len > scp->m.Length) {
	    scp->m.Length = filePos+len;
	    scp->m.ModFlags |= CM_MODLENGTH;
	}
	cm_SetChunkDirtyRange(dcp, offset, offset+len);
	if (!noLock)
	    lock_ReleaseWrite(&scp->llock);

#ifdef AFS_VFSCACHE
	if (!(osifilep = cm_CFileOpen(&dcp->f.handle)))
#else
	if (!(osifilep = cm_CFileOpen(dcp->f.inode)))
#endif /* AFS_VFSCACHE */
	    panic("afswrite open");

	/*
	 * mung uio structure to be right for this transfer
	 */
	osi_uio_copy(uiop, &tuio, iovecp);
	osi_uio_trim(&tuio, len);
	tuio.osi_uio_offset = offset;
	/* We now allow truncate to set the chunkBytes low
	 * while leaving data allocated to the chunk.  So, if
	 * we're writing a sparse file, we have to make sure that
	 * the hole we're skipping is zeroed.  The easiest way to
	 * handle this rare occurrence is to truncate the chunk
	 * to the size required by its current amount of data
	 * before doing this write, iff we're writing sparsely.
	 */
	if (offset > dcp->f.chunkBytes) {
	    /* we're writing sparsely*/
	    cm_CFileTruncate(osifilep, dcp->f.chunkBytes);
	}
	code = cm_CFileRDWR(osifilep, UIO_WRITE, &tuio, osi_credp);
	if (code || (tuio.osi_uio_resid > 0)) {
	    icl_Trace4(cm_iclSetp, CM_TRACE_CM_WRITE_LCLNOGO,
		       ICL_TYPE_POINTER, scp,
		       ICL_TYPE_POINTER, dcp,
		       ICL_TYPE_LONG, code,
		       ICL_TYPE_LONG, tuio.osi_uio_resid);
	    error = code;
	    if (error == 0) error = EIO;	/* didn't write all! */

	    /*
	     * If called from cm_rdwr, we have just lost the user's data, and
	     * have no choice but to invalidate the dcache entry.
	     *
	     * If called from cm_nlrw, the user's data are still sitting there
	     * in VM, and we have to be ready for repeated attempts to write
	     * them to the cache.  So we do not invalidate the dcache entry.
	     * The universal assumption is that if there are VM pages, they
	     * must be backed by a valid dcache entry.
	     */
	    if (!noLock) {
		/* need to stabilize, and also need scache lock */
		lock_ReleaseWrite(&dcp->llock);
		lock_ObtainWrite(&scp->llock);
		lock_ObtainWrite(&dcp->llock);
		cm_InvalidateOneSegment(scp, dcp, 1);
		lock_ReleaseWrite(&scp->llock);
		cm_CFileTruncate(osifilep, 0);	/* fake truncate the segment */
		cm_AdjustSize(dcp, 0);
	    }
	    cm_CFileClose(osifilep);
	    if (!noLock)
		lock_ReleaseWrite(&dcp->llock);	/* release write lock */
	    cm_PutDCache(dcp);
	    break;
	}
	/*
	 * Otherwise we've written some, fixup length, etc and
	 * continue with next seg
	 */
	osi_uio_skip(uiop, len);	/* advance uiop over data written */
	if (offset + len > dcp->f.chunkBytes)	/* compute new file size */
	    cm_AdjustSize(dcp, offset+len);
	totalLength -= len;
	filePos += len;
	cm_CFileClose(osifilep);
	if (!noLock) {
	    lock_ReleaseWrite(&dcp->llock);
	}
	cm_PutDCache(dcp);
	/* determine if we should store the data asynchronously */
	if (wroteLastByte) {
	    if (!noLock) {
		code = cm_DoPartialWrite(scp, lastByteOffset, &rreq, credp);
		if (code) {
		    icl_Trace3(cm_iclSetp, CM_TRACE_CM_WRITE_STOREIT,
			       ICL_TYPE_POINTER, scp,
			       ICL_TYPE_LONG, lastByteOffset,
			       ICL_TYPE_LONG, code);
		    error = code;
		    break;
		}
	    }
	    else {
		/* can't do it ourselves, since vp is locked, but can
		 * ask daemon to do it.  So, try that, at least, to get
		 * some asynchrony on storebacks.
		 */
		(void)cm_bkgQueue(BK_STORE_OP, scp, 0, credp, lastByteOffset,
				  0, 0, 0, 0);
	    }
	}
    }	/* the big while loop */
  out:
    osi_FreeBufferSpace((struct osi_buffer *)iovecp);
    if (!error && adjusted)
	uiop->osi_uio_resid = adjusted;
    return cm_CheckError(error, &rreq);
}
#endif  /* !SGIMIPS */

/* do partial write if we're low on unmodified chunks.
 * No locks should be held when calling this function.
 */
#ifdef SGIMIPS
static long cm_DoPartialWrite(
  register struct cm_scache *avc,
  long offset,
  struct cm_rrequest *areq,
  osi_cred_t *acredp)
#else  /* SGIMIPS */
static long cm_DoPartialWrite(avc, offset, areq, acredp)
  register struct cm_scache *avc;
  long offset;
  osi_cred_t *acredp;
  struct cm_rrequest *areq;
#endif /* SGIMIPS */
{
    register long code;
    register struct cm_dcache *dcp;
    int needSync;	/* need to do this write synchronously */

    icl_Trace0(cm_iclSetp, CM_TRACE_DOPARTIALWRITE);

    /* by default, we try to store data from a background process */
    needSync = 0;

    /* if too many dirty chunks, we want to store this data back synchronously
     * to prevent lots of writers from filling the cache with dirty chunks.
     */
    if (cm_cacheDirty >= cm_maxCacheDirty) needSync = 1;

    /* otherwise, store back modified chunks; need to do them all, since
     * an individual storedcache does bizarre things if we leave other
     * modified chunks in the cache (appears to truncate the file if we
     * piggyback a truncate).
     *
     * It would be tempting to add a vm flush here, but that'd generate even
     * more dirty chunks now, which is not what we really want.
     */
    code = 0;	/* initialize */
    if (needSync) {
	code = cm_StoreAllSegments(avc, CM_STORED_ASYNC, areq);
    }
    else {
	/* now, this call might fail, but in that case, we'll leave the
	 * data in the cache, marked as still dirty, and we'll try the store
	 * back again at close time.  If it fails then, we'll return the error
	 * on the close call.
	 */
	if (!cm_bkgQueue(BK_STORE_OP, avc, 0, acredp, offset, 0, 0, 0, 0)) {
	    /* all daemons busy, do it by hand */
	    dcp = cm_FindDCache(avc, offset);
	    /* if we find the chunk, we're fine.  If not, that's fine,
	     * too, since then we don't have anything to write.
	     */
	    if (dcp) {
		code = cm_StoreDCache(avc, dcp, CM_STORED_ASYNC, areq);
		icl_Trace3(cm_iclSetp, CM_TRACE_CM_WRITE_STORED,
			       ICL_TYPE_POINTER, avc,
			       ICL_TYPE_POINTER, dcp,
			       ICL_TYPE_LONG, code);
		cm_PutDCache(dcp);
	    }
	}
    }
    return code;
}

/*
 * cm_read -- reading into user-space or VM from the cache
 *
 * Non-zero abpp indicates that we are called from cm_bread.  If it is
 * convenient to do so, we point abpp to a data-containing buffer; otherwise,
 * we just return data via uio.
 * Currently, abpp-pointing is not implemented; if so, try to do VOP_BREAD in
 * place of VOP_RDWR, making sure that bsize is the same, etc.
 */

#ifdef SGIMIPS
static cm_read(
    struct cm_scache *scp,
    struct uio *uiop,
    osi_cred_t *credp,
    int ioflag,
    struct buf **abpp,
    int noLock)
#else  /* SGIMIPS */
static cm_read(scp, uiop, credp, albn, abpp, noLock)
    struct cm_scache *scp;
    struct uio *uiop;
    osi_cred_t *credp;
    daddr_t albn;
    struct buf **abpp;
    int noLock;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    afs_hyper_t totalLength, filePos;
    long needPrefetch=0;
    register struct cm_dcache *dcp=0;
    afs_hyper_t len, origLen, offset;
    long temp, code=0;
#else  /* SGIMIPS */
    long totalLength, filePos, needPrefetch=0;
    register struct cm_dcache *dcp=0;
    long offset, len, origLen, temp, code=0;
#endif /* SGIMIPS */
    struct uio tuio;
    struct iovec *iovecp;
    char *osifilep;
    struct cm_rrequest rreq;
    int need_chunk = 1;
    struct cm_dcache *fetchCkDcp;

    icl_Trace2(cm_iclSetp, CM_TRACE_READ, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, noLock);
#ifdef SGIMIPS
    if (!(ioflag & IO_ISLOCKED)) 
	cm_rwlock(SCTOV(scp), VRWLOCK_READ);

#endif /* SGIMIPS */

#ifdef AFS_SUNOS5_ENV
    /* if a SPECDEV scache vnode makes it this far, it's not
     * a real device.
     */
    if (scp->states & SC_SPECDEV)
	return ENODEV;
#endif /* AFS_SUNOS5_ENV */

    cm_InitReq(&rreq, credp);
    iovecp = (struct iovec *) osi_AllocBufferSpace();
    totalLength = uiop->osi_uio_resid;
    filePos = uiop->osi_uio_offset;

    while (totalLength > 0) {
	if (!noLock) {
	    lock_ObtainRead(&scp->llock);
            if (code = cm_GetTokens(scp, TKN_STATUS_READ, &rreq, READ_LOCK)) {
                lock_ReleaseRead(&scp->llock);
                break;
            }
	}
	len = scp->m.Length;
	origLen = scp->m.Length;
	code = scp->asyncStatus;

	if (!noLock)
	    lock_ReleaseRead(&scp->llock);

	/* if we have an async error, we should bail out now */
	if (code) break;

	if (filePos >= len)
	    break;				/* all done */

	if (noLock) {
	    if (need_chunk) {
		if (dcp)
		    cm_PutDCache(dcp);
	        dcp = cm_FindDCache(scp, filePos);
		need_chunk--;
	    }
	    if (!dcp)
		break;
	    offset = filePos - cm_chunktobase(dcp->f.chunk);
	    /* if still fetching, validPos has right size */
	    if (dcp->states & DC_DFFETCHING)
		len = dcp->validPos - offset;
	    else
		len = dcp->f.chunkBytes - offset;
	} else {
	    if (need_chunk) {
		if (dcp)
		    cm_PutDCache(dcp);
#ifdef SGIMIPS
                if (!(dcp = cm_GetDCache(scp, filePos))) {
                    lock_ObtainRead(&scp->llock);
                    if (code = scp->asyncStatus) {
                        lock_ReleaseRead(&scp->llock);
                        break;
                    }
                    else {
                        panic("cm_read read no dcp");
                    }
                }
#else /* SGIMIPS */
		dcp = cm_GetDCache(scp, filePos);
#endif /* SGIMIPS */
		need_chunk--;
	    }
	    lock_ObtainRead(&scp->llock);
	    lock_ObtainWrite(&dcp->llock);
#ifdef SGIMIPS
	    {
	    long tmp_offset, tmp_len;

	    if (filePos >= scp->m.Length) {
		/* bail if the file was truncated and left us hanging */
		 break;
	    }
	    cm_ComputeOffsetInfo(scp, dcp, filePos, &tmp_offset, &tmp_len);
	    offset = tmp_offset;
	    len = tmp_len;
	    }
#else  /* SGIMIPS */
	    cm_ComputeOffsetInfo(scp, dcp, filePos, &offset,(long *)&len);
#endif /* SGIMIPS */
	    /* now, first try to start transfer, if we'll need the data.
	     * If data is already coming or is here, we don't need to do
	     * this, obviously.
	     */
	    if ((dcp->f.states & DC_DONLINE) &&
		!(dcp->states & DC_DFFETCHING)) {
		dcp->f.versionNo = scp->m.DataVersion; /* update dv */
		cm_SetEntryMod(dcp);
	    } else if (!(dcp->states & DC_DFFETCHING)) {
		/*
		 * have cache entry, it is not coming in now, and we'll need
		 * new data
		 */
		temp = dcp->states;
		dcp->states |= DC_DFFETCHREQ;
		if (cm_bkgQueue(BK_FETCH_OP, scp, 0, credp,
				filePos, (long)dcp, CM_DLOCK_FETCHOK, 0, 0)) {
		    /* request worked, wait for FETCHREQ to go away */
		    while (dcp->states & DC_DFFETCHREQ) {
			icl_Trace1(cm_iclSetp, CM_TRACE_READFAWAIT,
				   ICL_TYPE_POINTER, dcp);
			/*
			 * Don't need waiting flag on this one
			 */
			lock_ReleaseRead(&scp->llock);
			osi_SleepW((opaque)(&dcp->states), &dcp->llock);
			lock_ObtainRead(&scp->llock);
			lock_ObtainWrite(&dcp->llock);
		    }
		}
		else {
		    /*
		     * failed to queue bkg request, turn off fetch
		     * request flag if it was off when we started
		     */
		    if (!(temp & DC_DFFETCHREQ))
			dcp->states &= ~DC_DFFETCHREQ;
		}
	    }
	    /*
	     * Now data may have started flowing in (if DC_DFFETCHING is on).
	     * If data is now streaming in, then wait for some interesting
	     * stuff.  Wait for DC_DFETCHING flag to vanish, or data to appear
	     */
	    for (;;) {
		/* if fetching, sleep if don't have >= 1 byte to read */
		if ((dcp->states & DC_DFFETCHING) && dcp->validPos <= filePos) {
		    icl_Trace1(cm_iclSetp, CM_TRACE_READDWAIT,
			       ICL_TYPE_LONG, dcp->validPos);
		    dcp->states |= DC_DFWAITING;
		    lock_ReleaseRead(&scp->llock);
		    osi_SleepW((opaque)(&dcp->states), &dcp->llock);
		    lock_ObtainRead(&scp->llock);
		    lock_ObtainWrite(&dcp->llock);
		}

		/* if not online, try to get it online */
		if (!(dcp->f.states & DC_DONLINE)) {
		    /* get the data online the hard way */
		    lock_ReleaseWrite(&dcp->llock);
		    icl_Trace0(cm_iclSetp, CM_TRACE_READMANUALLYONLINE);
		    code = cm_GetDLock(scp, dcp, CM_DLOCK_FETCHOK,
				       WRITE_LOCK, &rreq);
		    if (code) {
			lock_ReleaseRead(&scp->llock);
			cm_PutDCache(dcp);
			goto out;
		    }
		}

		/* now we may be done, if online and fetching is off
		 * or we've got at least one byte.
		 */
		if ((dcp->f.states & DC_DONLINE) &&
		    (!(dcp->states & DC_DFFETCHING) || dcp->validPos > filePos))
		    break;
	    }

	    /* now we have the ONLINE flag set and perhaps we're
	     * still fetching the chunk or perhaps the data's already
	     * here.  We compute the proper range of the chunk to use.
	     */
	    offset = filePos - cm_chunktobase(dcp->f.chunk);
	    if (dcp->states & DC_DFFETCHING) {
		/* Still fetching, some new data is here: compute length */
		len = dcp->validPos - filePos;
	    } else { /* data is here */
		len = dcp->f.chunkBytes - offset;
	    }
	}	/* locking case to find chunk */

	/* now do the real transfer from this chunk */
	if (len	> totalLength)
	    len = totalLength; 		/* will read len bytes */
	if (len	<= 0) {
	    /*
	     * Read past the end of a chunk, may not be at next chunk yet, and
	     * yet also not at eof, so may have to supply fake zeros
	     */
	    len	= cm_chunktosize(dcp->f.chunk) - offset;
	    if (len > totalLength)
		len = totalLength;	/* and still within xfr request */
	    code = scp->m.Length - offset;	/* and still within file */
	    if (len > code)
		len = code;
	    if (len > CM_ZEROS)
		len = sizeof(cm_zeros);	/* and in 0 buffer */
	    osi_uio_copy(uiop, &tuio, iovecp);
	    osi_uio_trim(&tuio, len);
            if (code = osi_uiomove(cm_zeros, len, UIO_READ, &tuio)) {
		if (!noLock) {
		    lock_ReleaseWrite(&dcp->llock);
		    lock_ReleaseRead(&scp->llock);
		}
		cm_PutDCache(dcp);
		dcp = (struct cm_dcache *) 0;
		break;
	    }
	} else {
	    /*
	     * Get the data from the file
	     */
#ifdef AFS_VFSCACHE
	    if (!(osifilep = cm_CFileOpen(&dcp->f.handle)))
#else
	    if (!(osifilep = cm_CFileOpen(dcp->f.inode)))
#endif /* AFS_VFSCACHE */
		panic("cm_read open");
	    /* mung uio struct to be ok for xfr */
	    osi_uio_copy(uiop, &tuio, iovecp);
	    osi_uio_trim(&tuio, len);
	    tuio.osi_uio_offset = offset;

	    code = cm_CFileRDWR(osifilep, UIO_READ, &tuio, osi_credp);
	    cm_CFileClose(osifilep);
	    if (code) {
		if (!noLock) {
		    lock_ReleaseWrite(&dcp->llock);
		    lock_ReleaseRead(&scp->llock);
		}
		cm_PutDCache(dcp);
		dcp = (struct cm_dcache *) 0;
		break;
	    }
	}

	/*
	 * Now we've read some, so fixup length, see if we are
	 * going to the next chunk and continue with next seg
	 */
	len = len - tuio.osi_uio_resid;	/* compute amount really transferred */
	osi_uio_skip(uiop, len);	/* update input uio structure */
	totalLength -= len;
	/*
	 * See if we will be crossing into the next chunk and if so
	 * free the current dcp.
	 */
	if (cm_chunk(filePos) != cm_chunk(filePos + len))
	    need_chunk++;
	filePos += len;

	/*
	 * Decide if we should prefetch next chunk.
	 *
	 * Required conditions:
	 *
	 *	1. noLock=1
	 *	2. enable_prefetch=1 and hot_prefetch > 0
	 *	3. scp->seq_hint.seq_access_flag=1
	 *	4. scp->seq_hint.next_chunk<=cm_chunk(origLen-1)
	 *	5. either of:
	 *
	 *		a. dcp->states & DC_DFNEXTSTARTED = 0
	 *		b. both of:
	 *			i. hot_prefetch>1
	 *			ii. scp->seq_hint.next_chunk-
	 *				cm_chunk(uiop->osi_uio_offset +
	 *				uiop->osi_uio_resid-1)<=hot_prefetch
	 */
	if ((!(dcp->states & DC_DFNEXTSTARTED)
	    || (hot_prefetch > 1
		&& (scp->seq_hint.next_chunk -
		    cm_chunk(uiop->osi_uio_offset + uiop->osi_uio_resid-1)) <=
		hot_prefetch))
	    && noLock
	    && enable_prefetch
	    && hot_prefetch > 0
	    && scp->seq_hint.seq_access_flag
	    && scp->seq_hint.next_chunk <= cm_chunk(origLen-1))
		needPrefetch=1;
	else
		needPrefetch=0;

	if (!noLock) {
	    lock_ReleaseRead(&scp->llock);
	    lock_ReleaseWrite(&dcp->llock);

	    /* update the access time */
	    lock_ObtainWrite(&scp->llock);
	    cm_MarkTime(scp, 0, credp, CM_MODACCESS, 1);
	    lock_ReleaseWrite(&scp->llock);
	}

	if (len <= 0) {
	    break;			/* surprise eof */
	}
    } /* end of while */
    /*
     * Try to queue prefetch, if needed
     */
    /* if we don't have a chunk, or we don't need to prefetch, or we're running
     * in nolock mode, then don't even think about prefetching.  Nolock is
     * important since the daemon will make all sorts of RPCs.
     */

    /*
     * We now do prefetch for seq access only.
     *
     * The value of hot_prefetch determines the number of chunks prefetched.
     */
    if (needPrefetch && dcp) {
	offset = cm_chunktobase(scp->seq_hint.next_chunk);
	scp->seq_hint.next_chunk++;
	icl_Trace1(cm_iclSetp, CM_TRACE_READPREFETCH, ICL_TYPE_LONG, offset);
	fetchCkDcp = cm_FindDCache(scp, offset);
	if ( !fetchCkDcp && offset < origLen) {
	    if (cm_bkgQueue(BK_FETCH_OP, scp, 0, credp, 
			    offset, 0, CM_DLOCK_FETCHOK, 0, 0)) {
		/* we've tried to prefetch this guy */
		dcp->states |= DC_DFNEXTSTARTED;
	    }
	}
	if (fetchCkDcp)
		cm_PutDCache(fetchCkDcp);
    }

    /*
     * It is ok to put the dcp without the scp->llock, since the refCount
     * is syncronized by the cm_dcachelock. This put puts the last chunk
     * that was being used.
     */
    if (dcp)
	cm_PutDCache(dcp);
out:
    osi_FreeBufferSpace((struct osi_buffer *)iovecp);
#ifdef SGIMIPS
    if (!(ioflag & IO_ISLOCKED))
	cm_rwunlock(SCTOV(scp), VRWLOCK_READ);
#endif /* SGIMIPS */
    return cm_CheckError(code, &rreq);
}


/* ARGSUSED */
#ifdef SGIMIPS
int
cm_ioctl(
    struct vnode *vp,
    int	cmd,
    void *arg)
#else  /* SGIMIPS */
int
cm_ioctl(vp, cmd, arg)
    struct vnode *vp;
    int	cmd, arg;
#endif /* SGIMIPS */
{
    return ENOTTY;
}


/*
 * Getattr call
 */
#ifdef SGIMIPS
int
cm_getattr(
    struct vnode *vp,
    struct xvfs_attr *xattrsp,
    int flag,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_getattr(vp, xattrsp, flag, credp)
    struct vnode *vp;
    struct xvfs_attr *xattrsp;
    int flag;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code;
    struct cm_rrequest rreq;
#ifdef EXPRESS_OPS
    struct volume *volp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    register struct cm_scache *scp;
    struct vattr *attrsp = &xattrsp->vattr;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_GETATTR, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, flag);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &volp,
                        &vp1, (struct vnode **) 0)) {
	code = cmglue_getattr(vp1, attrsp, flag, credp);
	/* we change the dev # because certain pwd's check to see if
	 * we've hit the root yet by checking against the root's dev #.
	 */
	attrsp->va_rdev ^= 0x7040;	/* change major and minor dev # */
	attrsp->va_fsid ^= 0x7040;	/* change major and minor dev # */
	cm_ExpressRele(volp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */
    cm_InitReq(&rreq, credp);
    if (scp->states & SC_VDIR) {
	code = cm_GetAttrVDir(scp, attrsp, &rreq);
#ifdef SGIMIPS
	return (int)code;
#else  /* SGIMIPS */
	return code;
#endif /* SGIMIPS */
    }

    lock_ObtainRead(&scp->llock);
    code = cm_GetTokens(scp, TKN_STATUS_READ, &rreq, READ_LOCK);
    if (!code)
	cmattr_to_vattr(scp, attrsp, flag);
    lock_ReleaseRead(&scp->llock);

    return cm_CheckError(code, &rreq);
}

#ifdef SGIMIPS
/* 
 *  Get lock.  
 *	Set us as the owner if we lock it exclusively.
 *	If we already have it write locked, bump counter.
 *	If we already have it read locked, promote the lock.
 *	If we want it read and already have it write, bump counter.
 *
 *	Returns 1 if successful, 0 otherwise.
 */
int
cm_rwlocker(struct vnode *vp, vrwlock_t locktype, int flag)
{
	int locked;

	struct cm_scache *scp;
        VTOSC(vp, scp);
        if (vp->v_type == VDIR)
                return 1;
        if (locktype == VRWLOCK_WRITE) {
                locked = mrtryupdate(&scp->rwlock);
		if (locked) {
			scp->rwl_owner = osi_ThreadUnique();
		} else if (scp->rwl_owner == osi_ThreadUnique()) {
			scp->rwl_count++;
		} else {
			if (flag) {
			     return 0;
			} else {
			    mrupdate(&scp->rwlock);
			    scp->rwl_owner = osi_ThreadUnique();
			}
		}
        } else {
                ASSERT((locktype == VRWLOCK_READ) ||
                        (locktype == VRWLOCK_WRITE_DIRECT));
		if (scp->rwl_owner == osi_ThreadUnique()) {
			scp->rl_count++;
		} else {
        	        mraccess(&scp->rwlock);
		}
        }
        return 1;
}

void
cm_rwlock(struct vnode *vp, vrwlock_t locktype)
{
    (void)cm_rwlocker(vp, locktype, 0);
}


int
cm_rwlock_nowait(struct vnode *vp)
{
    return (cm_rwlocker(vp, VRWLOCK_WRITE, 1));
}


void
cm_rwunlock(struct vnode *vp, vrwlock_t locktype)
{
        struct cm_scache *scp;
        VTOSC(vp, scp);
        if (vp->v_type == VDIR)
                return;
        if (locktype == VRWLOCK_WRITE) {
		if (scp->rwl_count) {
			scp->rwl_count--;
		} else {
			scp->rwl_owner = 0;
                	mrunlock(&scp->rwlock);
		}
        } else {
		if (scp->rl_count) {
			scp->rl_count--;
		} else {
                	mrunlock(&scp->rwlock);
		}
        }
        return;
}

#endif /* SGIMIPS */

/*
 * Setattr call
 */
#ifdef SGIMIPS
int
cm_setattr(
    struct vnode *vp,
    struct xvfs_attr *xattrsp,
    int flag,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_setattr(vp, xattrsp, flag, credp)
    struct vnode *vp;
    struct xvfs_attr *xattrsp;
    int flag;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    register long code = 0;
#ifdef EXPRESS_OPS
    struct volume *volp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    register struct cm_scache *scp;
    afsStoreStatus outStatus;
    int utime_write_ck = 0;
    int haveWrite, haveControl, blocked;
    int retryCount = 0;
    struct vattr *attrsp = &xattrsp->vattr;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_SETATTR, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, flag);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &volp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_setattr(vp1, attrsp, flag, credp);
	cm_ExpressRele(volp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */
    if (scp->states & SC_VDIR)
	return EINVAL;
    if (scp->states & SC_RO)
	return EROFS;
    cm_InitReq(&rreq, credp);

    /*
     * On Solaris, we hold the file's rwlock (the same that is held by the
     * VOP_RWLOCK op).  This prevents a truncate from racing in and changing
     * the file's length while cm_nfsrdwr has dropped the file's llock.  This
     * is important, because at that point cm_nfsrdwr is doing things that
     * will trigger calls to cm_getpage, and by the time cm_getpage is called
     * (and realizes that it is beyond EOF), it is too late to make a clean
     * recovery.
     */
#ifndef SGIMIPS
    cm_sun_rwlock(scp);
#endif /* SGIMIPS */

    if (osi_setting_size(attrsp)) {

	/* 32/64-bit interoperability changes:
	 *
	 * Check for exceeding file server size limit by using the maxFileSize
	 * parameter in struct cm_server.  This parameter is obtained from the
	 * server via the AFS_SetParams call. */

	/* XXX -- this is bogus for now but a placeholder for when va_size
         *     is bigger than 32 bits long. */

#ifdef SGIMIPS
	afs_hyper_t biggestSrvFile = GetMaxSize(scp);

	if ( attrsp->va_size > biggestSrvFile ) {
#else  /* SGIMIPS */
	afs_hyper_t biggestSrvFile = GetMaxSize(scp);

	if (AFS_hcmp64(biggestSrvFile, 0, attrsp->va_size) < 0) {
#endif  /* SGIMIPS */
	    code = EFBIG;
	} else {
#ifdef SGIMIPS
	    cm_rwlock(vp, VRWLOCK_WRITE);
	    code = cm_TruncateFile(scp, attrsp->va_size, credp, &rreq, 0);
	    cm_rwunlock(vp, VRWLOCK_WRITE);
#else  /* SGIMIPS */
	    code = cm_TruncateFile(scp, attrsp->va_size, credp, &rreq);
#endif /* SGIMIPS */
	}
    } else { 	/* We need control access for chmod */
	/*
	 * Handle POSIX utime EACCES and EPERM rules
	 */
	if (flag & XVN_SET_TIMES_NOW)
	    utime_write_ck = 1;		/* write should suffice */
	else {
	    if (osi_setting_mtime(attrsp)) {
		/* A time has been provided.  Sync data modifications
		 * back to the server now so the provided time will
		 * not be overwritten by async data stores.  This
		 * is important for commands like cp -p.
		 */
		if (code = cm_SyncDCache(scp, 0, (osi_cred_t *)0)) {
#ifdef SGIMIPS
		    return (int)code;
#else  /* SGIMIPS */
		    cm_sun_rwunlock(scp);

		    return code;
#endif /* SGIMIPS */
		}
	    }
	}
	for (;;) {
	    code = cm_GetSLock(scp, 
		TKN_UPDATESTAT, CM_SLOCK_WRITE, WRITE_LOCK, &rreq);
	    if (code) {
		/* ensure that we always have write lock when exiting
		 * this loop.
		 */
		lock_ObtainWrite(&scp->llock);
		break;
	    }

	    /* figure out if we have control access */
	    code = cm_GetAccessBits(scp, dacl_perm_control, &haveControl,
				    &blocked, &rreq, WRITE_LOCK, &retryCount);
	    if (code) break;
	    /* dropped lock to get access info, retry stabilizing vnode */
	    if (blocked) {
		lock_ReleaseWrite(&scp->llock);
		continue;
	    }

	    /* figure out if we have write access, if we don't have control,
	     * otherwise we don't care.
	     */
	    if (!haveControl) {
		code = cm_GetAccessBits(scp, dacl_perm_write, &haveWrite,
					&blocked, &rreq, WRITE_LOCK, &retryCount);
		if (code) break;
		/* dropped lock to get access info, retry stabilizing vnode */
		if (blocked) {
		    lock_ReleaseWrite(&scp->llock);
		    continue;
		}
	    }

	    /* all done, have stable vnode with acl info */
	    break;
	}	/* for (;;) */

	/* Check permissions, which are set only if code == 0 */
	if (code == 0 && !haveControl) {
	    if (utime_write_ck) {	/* if write would suffice */
		if (!haveWrite)
		    code = EACCES;
		/* otherwise we're fine, we have w but no c rights,
		 * and an operation that requires only w rights.
		 */
	    }
	    else /* need to be owner, write won't help */
		code = EPERM;
	}
	if (code == 0) {
	    outStatus.mask = 0;
	    /* the order of ScanStatus and then TranslateStatus is
	     * important, so that the changes we're doing now get
	     * added *after* the current state.
	     */
	    if (scp->m.ModFlags)
		cm_ScanStatus(scp, &outStatus);
	    cm_TranslateStatus(attrsp, &outStatus,
			       (flag & XVN_SET_TIMES_NOW));	/* overwrites */
	    code = cm_StoreSCache(scp, &outStatus, 0, &rreq);
	}
	lock_ReleaseWrite(&scp->llock);
    }
#ifndef SGIMIPS
    cm_sun_rwunlock(scp);
#endif /* SGIMIPS */
    return cm_CheckError(code, &rreq);
}


/*
 * Access call
 */
#ifdef SGIMIPS
int
cm_access(
    struct vnode *vp,
    int mode,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_access(vp, mode, credp)
    struct vnode *vp;
    int mode;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code;
#ifdef SGIMIPS
    long ttype;
#else  /* SGIMIPS */
    long tmode, ttype;
#endif /* SGIMIPS */
    long rights1, rights2;
    struct cm_rrequest rreq;
#ifdef EXPRESS_OPS
    struct volume *volp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    register struct cm_scache *scp;

#ifdef AFS_SUNOS5_ENV
    /*
     * Solaris NFS/DFS gateway hack:
     *	 VOP_ACCESS is called by nfssys(...NFS_CNVT...), called by the lockd
     *	 daemon, while opening a file in preparation for locking on behalf of
     *	 NFS.  The "right" way for us to handle this is to intercept the RPC
     *	 and find credentials for the server (the lockd daemon) to use.
     *	 Instead, as a quick hack, we test whether we are called from nfssys()
     *	 and, if so, always allow access.
     */
    if (osi_DoingNfssysCall())
	return 0;
#endif

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_ACCESS, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, mode);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &volp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_access(vp1, mode, credp);
	cm_ExpressRele(volp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    if (scp->states & SC_VDIR) {
	return ((mode & VWRITE) ? EROFS : 0);	/* pretend most everything works */
    }
    cm_InitReq(&rreq, credp);

    lock_ObtainRead(&scp->llock);
    if ((mode & VWRITE) && (scp->states & SC_RO)) {
        /* 
	 * write access in a read-only file system?
	 */
	lock_ReleaseRead(&scp->llock);
	return EROFS;
    }

    code = 0;
    ttype = (int) cm_vType(scp);

    lock_ReleaseRead(&scp->llock);

    rights1 = 0; rights2 = 0;

    if (ttype == (int) VDIR) {
	if (mode & VEXEC)
	    rights1 |= dacl_perm_execute;
	if (mode & VREAD)
	    rights1 |= dacl_perm_read;
	if (mode & VWRITE) {
	    /* don't report write access to a dir unless we have
	     * W rights and at least one of I or D.
	     */
	    rights2 = rights1;
	    rights1 |= (dacl_perm_write | dacl_perm_insert);
	    rights2 |= (dacl_perm_write | dacl_perm_delete);
	}
    } else {
	if (mode & VREAD)
	    rights1 |= dacl_perm_read;
	if (mode & VEXEC)
	    rights1 |= dacl_perm_execute;
	if (mode & VWRITE)
	    rights1 |= dacl_perm_write;
    }
    code = cm_AccessError(scp, rights1, &rreq);
    if (code == EACCES && rights2)
	code = cm_AccessError(scp, rights2, &rreq);

    /* now, code is set if access is fine.  Map to this call's semantics,
     * which is that we return 0 or EACCES.
     */
#ifndef SGIMIPS
  done:
#endif /* !SGIMIPS */
    return cm_CheckError(code, &rreq);	/* failure code */
}


/*
 * Lookup call
 */
#ifdef SGIMIPS
int
cm_lookup(
    struct vnode *dvp,
    char *namep,
    struct vnode **vpp,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_lookup(dvp, namep, vpp, credp)
    struct vnode *dvp, **vpp;
    char *namep;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    char *tnamep;
    struct cm_scache *tscp=0;
    long pass=0, code;
    afsFid tfid, oldfid;
    struct cm_volume *volp;
#ifndef SGIMIPS
    struct cm_vdirent *tvdp;
#endif /* !SGIMIPS */
    struct cm_scache *uscp;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1, *vpx;
#endif /* EXPRESS_OPS */
    struct cm_scache *dscp;

#ifdef SGIMIPS
    VTOSC(dvp, dscp);
#else  /* SGIMIPS */
    dscp = VTOSC(dvp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_LOOKUP, ICL_TYPE_POINTER, dscp,
	       ICL_TYPE_STRING, namep);
    /*
     * lookup the name namep in the appropriate dir, and return a cache entry
     * on the resulting fid
     */
    cm_InitReq(&rreq, credp);

#ifdef EXPRESS_OPS
    if (cm_ExpressPath(dscp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	if (namep[0] == '.' && namep[1] == '.' && namep[2] == 0 &&
	    (vp1->v_flag & VROOT)) {
	    code = cm_GetMountPointDotDot(dscp, &tfid, &tscp, &rreq);
	    if (code == 0 && !tscp)
		code = cm_GetSCache(&tfid, &tscp, &rreq);
	    if (code == 0) {
#ifdef SCACHE_PUBLIC_POOL
		*vpp = cm_get_vnode(tscp);
		if (*vpp == NULLVP) {
		    cm_PutSCache(tscp);
		    code = ENOMEM;
		}
#else
		*vpp = SCTOV(tscp);
#endif /* SCACHE_PUBLIC_POOL */
	    }
	} else {
	    /* not ".." for a name */
	    code = cmglue_lookup(vp1, namep, &vpx, credp);
	    if (code == 0) {
#ifdef SGIMIPS
		struct cm_scache *scpx;
#endif  /* SGIMIPS */
		xvfs_convert(vpx);	/* convert vnode */
		code = cm_MapDown(dscp, vpx, &uscp);
		*vpp = SCTOV(uscp);
#ifdef SGIMIPS
		VTOSC(vpx, scpx);
		cm_PutSCache(scpx);
#else  /* SGIMIPS */
		cm_PutSCache(VTOSC(vpx));
#endif /* SGIMIPS */
	    }
	}
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return cm_CheckError(code, &rreq);
    } /* Express path */
#endif /* EXPRESS_OPS */

    if (dscp->states & SC_VDIR) {
	code = cm_LookupVDir(dscp, namep, &tscp, &rreq);
	if (code) {
		*vpp = (struct vnode *)0;
		return cm_CheckError(code, &rreq);
	}
#ifdef SCACHE_PUBLIC_POOL
	*vpp = cm_get_vnode(tscp);
	if (*vpp == NULLVP){
		cm_PutSCache(tscp);
	  	return(ENOMEM);
	}
#else
	*vpp = SCTOV(tscp);
#endif /* SCACHE_PUBLIC_POOL */
	return 0;
    }

    /* From this point downward, instead of returning from the mainline
     * code, we must goto done, so that we clean up the buffer allocated
     * in tnamep.
     */
#ifdef SGIMIPS
    if ((code =(long)strlen(namep))>= 4 && strcmp(namep+code-4, "@sys") == 0) {
#else  /* SGIMIPS */
    if ((code = strlen(namep)) >= 4 && strcmp(namep+code-4, "@sys") == 0) {
#endif /* SGIMIPS */
	tnamep = (char *) osi_AllocBufferSpace();
	HandleAtName(namep, tnamep, &rreq, dscp);
    } else
#ifdef SGIMIPS
      if ((code = strlen(namep)) >= 5 && strcmp(namep+code-5, "@host") == 0) {
#else  /* SGIMIPS */
      if ((code = strlen(namep)) >= 5 && strcmp(namep+code-5, "@host") == 0) {
#endif /* SGIMIPS */
	tnamep = (char *) osi_AllocBufferSpace();
	HandleAtName(namep, tnamep, &rreq, dscp);
    } else
	tnamep = namep;
    /*
     * Come back to here if we want to retry a failed GetSCache
     */
redo:
    *vpp = (struct vnode *) 0;  /* Since some callers don't init it */

    /*
     * Use the cache first (but make a lookup RPC if necessary).
     */
    if (code = nh_dolookup(dscp, tnamep, &tfid, &tscp, &rreq))
	goto done;
    if (!tscp) code = cm_GetSCache(&tfid, &tscp, &rreq);

    icl_Trace4(cm_iclSetp, CM_TRACE_LOOKUPFID,
	       ICL_TYPE_LONG, AFS_hgetlo(tfid.Cell),
	       ICL_TYPE_LONG, AFS_hgetlo(tfid.Volume),
	       ICL_TYPE_LONG, tfid.Vnode,
	       ICL_TYPE_LONG, tfid.Unique);
    if (tscp) {
        /*
	 * now get the status info
	 */
	lock_ObtainRead(&tscp->llock);
	if (tscp->states & SC_MOUNTPOINT) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_LOOKUPMTPT);
	    /*
	     * a mount point, possibly unevaluated
	     */
	    lock_ReleaseRead(&tscp->llock);
	    lock_ObtainWrite(&tscp->llock);
	    /*
	     * Don't have to recheck SC_MOUNTPOINT after dropping the lock
	     * because it won't get turned off
	     */
	    if (tscp->mountRoot == (afsFid *)0 || !(tscp->states & SC_MVALID)){
		code = cm_GetTokens(tscp, TKN_READ, &rreq, WRITE_LOCK);
		if (code == 0) {
		    code = cm_EvalMountPoint(tscp, dscp, &rreq);
		}
	    }
	    lock_ReleaseWrite(&tscp->llock);
	    /*
	     * next, we want to continue using the target of the mt point
	     * tscp->mountRoot contains a valid fid for this fileset root.
	     */
	    if (tscp->mountRoot && (tscp->states & SC_MVALID)) {
	        struct cm_scache *tmpscp;
		/*
		 * now lookup target, to set .. pointer
		 */
		icl_Trace4(cm_iclSetp, CM_TRACE_LOOKUPMTPTFID,
			   ICL_TYPE_LONG, AFS_hgetlo(tscp->fid.Cell),
			   ICL_TYPE_LONG, AFS_hgetlo(tscp->fid.Volume),
			   ICL_TYPE_LONG, tscp->fid.Vnode,
			   ICL_TYPE_LONG, tscp->fid.Unique);
		tmpscp = tscp;			/* remember for later */
		(void) cm_GetSCache(tmpscp->mountRoot, &tscp, &rreq);
		if (!tscp) {
		    cm_PutSCache(tmpscp);	/* we're done with it */
		    code = ENOENT;
		    goto done;
		}
		/*
		 * Always reevaluate the .. ptr for the mount point to
		 * point back to the appropriate place.
		 */
		lock_ObtainWrite(&tscp->llock);
		cm_SetMountPointDotDot(dscp, tmpscp, tscp);
		lock_ReleaseWrite(&tscp->llock);
		cm_PutSCache(tmpscp);		/* we're done with it */
	    } else {
		icl_Trace0(cm_iclSetp, CM_TRACE_LOOKUPMTPTFAIL);
		cm_PutSCache(tscp);
		if (code == 0)	/* catch the odd failure */
		    code = ENOENT;
		goto done;
	    }
	} else {
	    /*
	     * not a mount point
	     */
	    lock_ReleaseRead(&tscp->llock);
	}
	code = 0;
	if (tscp->states & SC_SPECDEV) {
            if (!(tscp = cm_MakeDev(tscp, tscp->devno)))
		code = ENOENT;	/* failed mysteriously */
	}
#ifdef SCACHE_PUBLIC_POOL
	*vpp = cm_get_vnode(tscp);
	if (*vpp == NULLVP) {
		cm_PutSCache(tscp);
		code = ENOMEM;
		goto done;
	}
#else
        *vpp = SCTOV(tscp);
#endif /* SCACHE_PUBLIC_POOL */
	icl_Trace1(cm_iclSetp, CM_TRACE_LOOKUPVP, ICL_TYPE_POINTER, tscp);
    } else {
	/*
	 * if we get here, we found something in a directory that couldn't be
	 * located (a Multics "connection failure"). If the volume is read-only
	 * we try flushing this entry from the cache and trying again. This
	 * will, as a side effect of re-stating the dir, possibly call
	 * cm_CheckVolSync.  We retry only once.
	 *
	 * If cm_GetSCache returned ESTALE, we likewise try again.  We retry
	 * as long as the fid keeps changing.  If the fid doesn't change,
	 * it could be just because the file is being renamed back and forth,
	 * but it also could be because there's something wrong with the
	 * directory on the server.  In either case we are not making progress
	 * and should quit.  As a further precaution, we stop retrying after
	 * a suspiciously large number of iterations.
	 */
	icl_Trace0(cm_iclSetp, CM_TRACE_LOOKUPCF);
	if ((code == ESTALE) && ((pass == 0) || FidCmp (&tfid, &oldfid)) && (pass < 10)) {
	    pass++;
	    FidCopy((&oldfid), (&tfid));
	    goto redo;
	}
	if (pass == 0) {
	    if (volp = cm_GetVolume(&dscp->fid, &rreq)) {
		if (volp->states & VL_RO) {
		    pass++;			/* try this *once* */
		    lock_ObtainWrite(&dscp->llock);
		    cm_ForceReturnToken(dscp, TKN_READ, 0);
		    lock_ReleaseWrite(&dscp->llock);
		    cm_PutVolume(volp);
		    FidCopy((&oldfid), (&tfid));
		    goto redo;
		}
		cm_PutVolume(volp);
	    }
	}
	code = ENOENT;
    }
done:
    /*
     * put the network buffer back, if need be
     */
    if (tnamep != namep)
	osi_FreeBufferSpace((struct osi_buffer *)tnamep);

    return cm_CheckError(code, &rreq);
}


/*
 * Hack for disabling tracing on core file creation.
 */
int cm_autoCore = 0;		/* Easy-to-patch variable for debugging */

static int CreateHack(name, rock, setp)
    char *name;
    char *rock;
    struct icl_set *setp;
{
    icl_SetSetStat(setp, ICL_OP_SS_DEACTIVATE);
    return 0;			/* Don't stop calling us */
}


/*
 * Create call
 */
#ifdef SGIMIPS
cm_create(
    struct vnode *dvp,
    char *namep,
    struct vattr *attrsp,
    int aexcl,
    int mode,
    struct vnode **vpp,
    osi_cred_t *credp)
#else  /* SGIMIPS */
cm_create(dvp, namep, attrsp, aexcl, mode, vpp, credp)
    struct vnode *dvp;
    char *namep;
    struct vattr *attrsp;
    enum vcexcl aexcl;
    int mode;
    struct vnode **vpp;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    long code, srvix;
    afs_hyper_t len;
    vnode_t *vp = 0;
#else  /* SGIMIPS */
    long code, len, srvix;
#endif /* SGIMIPS */
    struct cm_rrequest rreq;
    register struct cm_conn *connp;
    struct cm_server *tserverp;
    afsFid newFid, oldFid;
    int pass = 0;
    struct lclHeap {
	afsStoreStatus InStatus;
	afsFetchStatus OutFidStatus, OutDirStatus;
	afsVolSync tsync;
	afs_token_t Token;
	afsTaggedName NameBuf;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    struct cm_scache *tscp;
    struct cm_volume *volp = (struct cm_volume *) 0;
    error_status_t st;
    u_long startTime;
    struct cm_scache *uscp;
    long irevcnt;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1, *vpx;
#endif /* EXPRESS_OPS */
    struct cm_scache *dscp;

    /*
     * Bugs in DFS may result in core files being produced after obscure
     * process failures.  To get useful traces in such cases it may be
     * necessary to disable tracing while the core file is being produced.
     */
    if (cm_autoCore && !strcmp("core", namep))
	icl_EnumerateSets(CreateHack, (char *) 0);

#ifdef SGIMIPS
    tscp = (struct cm_scache *) 0;

    VTOSC(dvp, dscp);
    if (vp = *vpp) {
	/* 
	 * We were passed in a vnode to use, that already has a hold on it.
	 */
	if (IS_CONVERTED(vp)) {
	    VTOSC(vp, tscp);
	} else {
	    /* 
	     * We did not find a DFS converted vnode.  Drop through
	     * to the old case where we are not given a vnode.
	     */
	    VN_RELE(vp);
	    *vpp = NULL;
	}
    }
#else  /* SGIMIPS */
    dscp = VTOSC(dvp);
#endif /* SGIMIPS */
    icl_Trace3(cm_iclSetp, CM_TRACE_CREATE, ICL_TYPE_POINTER, dscp,
	       ICL_TYPE_STRING, namep, ICL_TYPE_LONG, mode);
    if (!ENameOK(namep) || strlen(namep) >= sizeof(lhp->NameBuf.tn_chars)) {
	return EINVAL;
    }

#ifdef EXPRESS_OPS
    if (cm_ExpressPath(dscp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_create(vp1, namep, attrsp, aexcl, mode, &vpx, credp);
	if (code == 0) {
	    /* convert UFS vnode to AFS CM vnode */
            code = cm_MapDown(dscp, vpx, &uscp);
            *vpp = SCTOV(uscp);
	    OSI_VN_RELE(vpx);
	}
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    if (dscp->states & SC_RO) {
	return EROFS;
    }
    cm_InitReq(&rreq, credp);

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

    bzero((caddr_t)&lhp->NameBuf, sizeof(lhp->NameBuf));
    strcpy((caddr_t)&lhp->NameBuf.tn_chars[0], namep);

    /*
     * Come back to here if we want to retry after a failed GetSCache
     */
redo:
#ifdef SGIMIPS
    if (tscp) {
	/* we were passed in a vnode with a hold on it */
 	code = 0;
    } else {
        code = nh_dolookup(dscp, namep, &newFid, &tscp, &rreq);
    }
#else  /* SGIMIPS */
    code = nh_dolookup(dscp, namep, &newFid, &tscp, &rreq);
#endif /* SGIMIPS */

    if (code == 0) {
	/*
	 * File exists, so try to use it
	 */
#ifdef SGIMIPS
	if (aexcl == VEXCL) 
#else  /* SGIMIPS */
	if (aexcl != NONEXCL) 
#endif /* SGIMIPS */
        {
	    code = EEXIST;	/* file exists in excl mode open */
	    if (tscp) cm_PutSCache(tscp);
	    goto done;
	}
	/*
	 * found the file, so use it
	 */
	if (!tscp) {
	    newFid.Cell = dscp->fid.Cell;
	    newFid.Volume = dscp->fid.Volume;
	    code = cm_GetSCache(&newFid, &tscp, &rreq);
	}
	if (code == 0) {
	    /*
	     * if the thing exists, we need the right access to open it, and we
	     * must check that here, since no other checks are made by the open
	     * system call
	     */
	    len = attrsp->va_size;	/* only truncate */
	    if ((mode & VREAD)
		&& (code = cm_AccessError(tscp, dacl_perm_read, &rreq))) {
		cm_PutSCache(tscp);
		goto done;
	    }
	    if ((mode & VWRITE) || osi_setting_size(attrsp)) {
		/*
		 * need write mode for these guys
		 */
		if (code = cm_AccessError(tscp, dacl_perm_write, &rreq)) {
		    cm_PutSCache(tscp);
		    goto done;
		}
	    }
	    lock_ObtainRead(&tscp->llock);
	    /* we only look at immutable properties below, so we don't
	     * need token again, since we had status info when we
	     * did cm_GetSCache above.
	     */
	    if (osi_setting_size(attrsp) && !cm_IsDevVP(tscp)) {
                if (cm_vType(tscp) != VREG) {
		    lock_ReleaseRead(&tscp->llock);
		    cm_PutSCache(tscp);
		    code = EISDIR;
		    goto done;
		}
		if (tscp->states & SC_RO) {
		    lock_ReleaseRead(&tscp->llock);
		    cm_PutSCache(tscp);
		    code = EROFS;
		    goto done;
		}
		/*
		 * Do a truncate
		 *
		 * We have to get an open-for-write token at this point.  The
		 * caller is about to call VOP_OPEN which will do the same
		 * thing, but at that point, if there is a failure to get the
		 * token, it's too late to undo the damage (the file has
		 * already been truncated).
		 *
		 * Then we drop the lock in order to call cm_TruncateFile.
		 * At that point we could lose the token.  That's too bad.
		 * We were going to drop the lock anyway before returning from
		 * cm_create, so we are just hosed.  This race is inherent in
		 * the way that vn_open does things--not our fault.
		 */
		lock_ReleaseRead(&tscp->llock);
		code = cm_GetOLock(tscp, TKN_OPEN_WRITE | TKN_STATUS_WRITE,
				   &rreq);
		if (code) {
		    cm_PutSCache(tscp);
		    goto done;
		}
		lock_ReleaseWrite(&tscp->llock);

		/* Finally ready to truncate! */

		/*
		 * See comment in cm_setattr for the rationale for using the
		 * rwlock at this point.
		 */
#ifdef SGIMIPS

		cm_rwlock(SCTOV(tscp), VRWLOCK_WRITE);
		code = cm_TruncateFile(tscp, len, credp, &rreq, 1);
		cm_rwunlock(SCTOV(tscp), VRWLOCK_WRITE);

#else  /* SGIMIPS */

		cm_sun_rwlock(tscp);
		code = cm_TruncateFile(SCTOV(tscp), len, credp, &rreq);
		cm_sun_rwunlock(tscp);

#endif /* SGIMIPS */

		if (code) {
		    cm_PutSCache(tscp);
		    goto done;
		}
		lock_ObtainRead(&tscp->llock);
	    }
#ifdef SCACHE_PUBLIC_POOL
	    *vpp = cm_get_vnode(tscp);
	    if (*vpp == NULLVP) {
		    lock_ReleaseRead(&tscp->llock);
		    cm_PutSCache(tscp);
		    code = ENOMEM;
		    goto done;
	    }
#else
	    *vpp = SCTOV(tscp);
#endif /* SCACHE_PUBLIC_POOL */
	    lock_ReleaseRead(&tscp->llock);

	} else {	/* cm_GetSCache call failed */
	    /*
	     * If we get a stale scache entry, retry.  But if we have already
	     * tried this fid before, don't retry; probably the directory on
	     * the server is damaged.  As a precaution, don't retry more than
	     * ten times.
	     */
	    if ((code == ESTALE) && ((pass == 0) || FidCmp(&oldFid, &newFid)) && (pass < 10)) {
		pass++;
		FidCopy((&oldFid), (&newFid));
		goto redo;
	    }
	    code = ENOENT;
	}
	/*
	 * make sure refcount bumped only if code == 0
	 */
	goto done;

    } /* If file exists already */

    /*
     * if we create the file, we don't do any access checks, since that's what
     * O_CREAT is supposed to do
     */
    lhp->InStatus.mask = AFS_SETMODTIME | AFS_SETACCESSTIME | AFS_SETMODE;
    if (attrsp->va_type == VCHR) {
	osi_EncodeDeviceNumber(&lhp->InStatus, attrsp->va_rdev);
	lhp->InStatus.mask |= AFS_SETDEVNUM;
	lhp->InStatus.deviceType = CharDev;
    } else if (attrsp->va_type == VBLK) {
	osi_EncodeDeviceNumber(&lhp->InStatus, attrsp->va_rdev);
	lhp->InStatus.mask |= AFS_SETDEVNUM;
	lhp->InStatus.deviceType = BlockDev;
    } else if (attrsp->va_type == VFIFO) {
	osi_EncodeDeviceNumber(&lhp->InStatus, 0);
	lhp->InStatus.mask |= AFS_SETDEVNUM;
	lhp->InStatus.deviceType = FIFO;
    }
#ifdef SGIMIPS
    osi_afsGetTime(&lhp->InStatus.modTime);
#else /* SGIMIPS */
    osi_GetTime((struct timeval *) &lhp->InStatus.modTime);
#endif /* SGIMIPS */
#ifdef SGIMIPS
    /*  Find out what the mode bits were before the umask was applied,
     * this is a workaround until the umask application is moved
     * below the vnode layer.
     */

    switch  (curuthread->ut_syscallno) {
    case SAT_SYSCALL_CREAT:    /* create */
        attrsp->va_mode = (attrsp->va_mode & ~MODEMASK) |
                       (unsigned int)(curuthread->ut_scallargs[1] & MODEMASK);
        break;
    case SAT_SYSCALL_MKNOD:    /* mknod */
        attrsp->va_mode = (unsigned int) 
			   (curuthread->ut_scallargs[1] & MODEMASK);
        break;
    }
#endif /* SGIMIPS */
    lhp->InStatus.accessTime = lhp->InStatus.modTime;
    lhp->InStatus.mode = attrsp->va_mode & 0xffff;
    lhp->InStatus.cmask = osi_GetCMASK();

#ifdef AFS_SUNOS5_ENV
    /* set group properly for Solaris NFS */
    if (attrsp->va_mask & AT_GID) {
	lhp->InStatus.mask |= AFS_SETGROUP;
	lhp->InStatus.group = attrsp->va_gid;
    }
#endif /* AFS_SUNOS5_ENV */

    lock_ObtainRead(&dscp->llock);
    code = cm_GetTokens(dscp, TKN_READ, &rreq, READ_LOCK);
    irevcnt = dscp->dirRevokes;
    lock_ReleaseRead(&dscp->llock);
    if (code) goto done;
    cm_StartTokenGrantingCall();
    volp = NULL;
    do {
	tserverp = 0;
	if (connp = cm_Conn(&dscp->fid,  MAIN_SERVICE(SRT_FX),
			    &rreq, &volp, &srvix)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_CREATESTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    st = AFS_CreateFile(connp->connp, &dscp->fid, &lhp->NameBuf,
				&lhp->InStatus, &osi_hZero,
				AFS_FLAG_RETURNTOKEN, &newFid,
				&lhp->OutFidStatus, &lhp->OutDirStatus,
				&lhp->Token, &lhp->tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(st);
	    tserverp = connp->serverp;
	    cm_SetReqServer(&rreq, tserverp);
	    icl_Trace1(cm_iclSetp, CM_TRACE_CREATEEND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, &dscp->fid, &rreq, volp, srvix, startTime));
    /* adjust incoming fid */
    newFid.Cell = dscp->fid.Cell;
    if (code) {
#ifdef SGIMIPS
	if (code == EEXIST && aexcl != VEXCL)
#else  /* SGIMIPS */
	if (code == EEXIST)
#endif /* SGIMIPS */
        {
	    /* First flush the DN cache */
	    cm_FlushExists(dscp, namep);
#ifndef SGIMIPS
	    if (aexcl == NONEXCL) {
#endif /* SGIMIPS */
	    /*
	     * if we get an EEXIST in nonexcl mode, just do a lookup
	     */
		code = cm_lookup(dvp, namep, vpp, credp);
#ifndef SGIMIPS
	    }
#endif /* SGIMIPS */
	}
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	goto done;
    }
    /*
     * Update the parent dir's status
     */
    if (code = cm_UpdateStatus(dscp, &lhp->OutDirStatus, namep, namep, &newFid,
			       &rreq, irevcnt)) {
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	goto done;
    }
    /*
     * Now process the newly created file.
     * NOTE: Merge the status even though the scache could have been created
     *       by others. The Merge func should take the most recent status.
     */
    lock_ObtainWrite(&cm_scachelock);
    tscp = cm_FindSCache(&newFid);
    icl_Trace1(cm_iclSetp, CM_TRACE_CREATEFIND, ICL_TYPE_POINTER, tscp);
    if (!tscp) {
	if (! (tscp = cm_NewSCache(&newFid, dscp->volp))) {
	    lock_ReleaseWrite(&cm_scachelock);
	    cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				    (struct cm_volume *) 0, startTime);
	    code = ENOENT;
	    goto done;
	}
    }
    lock_ReleaseWrite(&cm_scachelock); /* Locking hierachy */
    lock_ObtainWrite(&tscp->llock);
    cm_EndTokenGrantingCall(&lhp->Token, tserverp, tscp->volp, startTime);
    tokenStat.CreateFile++;
    cm_MergeStatus(tscp, &lhp->OutFidStatus, &lhp->Token, 0, &rreq);
    /* Check for race with deletion (!?) */
    if (code = cm_GetTokens(tscp, TKN_OPEN_PRESERVE, &rreq, WRITE_LOCK)) {
	lock_ReleaseWrite(&tscp->llock);
	cm_PutSCache(tscp);
	tscp = (struct cm_scache *) 0;
	goto done;
    }
#ifdef SCACHE_PUBLIC_POOL
    *vpp = cm_get_vnode(tscp);
    if (*vpp == NULLVP) {
	    lock_ReleaseWrite(&tscp->llock);
	    cm_PutSCache(tscp);
	    code = ENOMEM;
	    goto done;
    }
#else
    *vpp = SCTOV(tscp);
#endif /* SCACHE_PUBLIC_POOL */
    /* Stash the cred so that asynchronous store backs can be done on
     * the creator's behalf.  This is also done in cm_open(), but not
     * all systems call VOP_OPEN before writing a core file.
     */
    crhold(credp);
    if (tscp->writerCredp) crfree(tscp->writerCredp);
    tscp->writerCredp = credp;


    /* reset sequential hints */

    cm_ResetSequentialHint(tscp);

    lock_ReleaseWrite(&tscp->llock);

done:
    /* 	Decrement the volume counter of concurrent RPCs. It appears
     *	that this could have been done earlier, but given that this
     * 	counter can be used to override the Changetime test in
     *	cm_MergeStatus() better safe than sorry
     */
    if (volp)
	cm_EndVolumeRPC(volp);

    if (code == 0) {
	/*
	 * Used to call cmattr_to_vattr here, but none of our currently
	 * supported platforms require it.
	 */

	/*
	 * Before proceeding, handle device vnode mapping
	 */

#ifdef SGIMIPS
	if (tscp && (tscp->states & SC_SPECDEV)) {
#else  /* SGIMIPS */
	if (tscp->states & SC_SPECDEV) {
#endif /* SGIMIPS */
            if (tscp = cm_MakeDev(tscp, tscp->devno))
                *vpp = SCTOV(tscp);
            else
                code = ENOENT;
	}
    }
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    return cm_CheckError(code, &rreq);
}


/*
 * Remove file call
 */
#ifdef SGIMIPS
int
cm_remove(
    struct vnode *dvp,
    char *namep,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_remove(dvp, namep, credp)
    struct vnode *dvp;
    char *namep;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    register struct cm_conn *connp;
    register struct cm_scache *scp;
    long code, srvix;
    struct lclHeap {
	afsFetchStatus OutDelFileStatus, OutDirStatus;
	afsFidTaggedName RemoveFileName;
	afsFid OutDelFileFid;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    struct cm_volume *volp = NULL;
    long irevcnt;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    error_status_t st;
    u_long startTime;
    register struct cm_scache *dscp;
    int call_smush = 0;
    afs_hyper_t openID;

#ifdef SGIMIPS
    VTOSC(dvp, dscp);
#else  /* SGIMIPS */
    dscp = VTOSC(dvp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_REMOVE, ICL_TYPE_POINTER, dscp,
	       ICL_TYPE_STRING, namep);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(dscp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_remove(vp1, namep, credp);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    cm_InitReq(&rreq, credp);
    if (dscp->states & SC_RO) {
	return EROFS;
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

    bzero((caddr_t)&lhp->RemoveFileName, sizeof(lhp->RemoveFileName));
    strcpy((caddr_t)&lhp->RemoveFileName.name.tn_chars[0], namep);
    lock_ObtainRead(&dscp->llock);
    code = cm_GetTokens(dscp, TKN_READ, &rreq, READ_LOCK);
    irevcnt = dscp->dirRevokes;
    lock_ReleaseRead(&dscp->llock);
    if (code) goto done;
    cm_GetReturnOpenToken(dscp, (char *)lhp->RemoveFileName.name.tn_chars,
			  &openID);
    /*
     * cm_UpdateStatus (called below) will delete the name hash entry, but
     * why wait till then?  Doing it now as well makes it harder (but not
     * impossible) for other processes on the same client to obtain stale
     * file handles.
     */
    nh_delete(dscp, namep);
    do {
	if (connp = cm_Conn(&dscp->fid,  MAIN_SERVICE(SRT_FX),
			    &rreq, &volp, &srvix)){
	    icl_Trace0(cm_iclSetp, CM_TRACE_REMOVESTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
            st = AFS_RemoveFile(connp->connp, (afsFid *) &dscp->fid,
				&lhp->RemoveFileName, &openID, &osi_hZero, 0,
				&lhp->OutDirStatus, &lhp->OutDelFileStatus,
				&lhp->OutDelFileFid, &lhp->tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(st);
	    icl_Trace1(cm_iclSetp, CM_TRACE_REMOVEEND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, &dscp->fid, &rreq, volp, srvix, startTime));
    if (code)
	goto done;

    /* update incoming file ID */
    lhp->OutDelFileFid.Cell = dscp->fid.Cell;

    /*
     * Update the parent dir's status
     */
    code = cm_UpdateStatus(dscp, &lhp->OutDirStatus, namep, (char *)0,
			   &lhp->OutDelFileFid, &rreq, irevcnt);
    if (code)
	goto done;

    /*
     * Now, get vnode for unlinked dude, and see if we should force it from
     * cache. dscp is now the deleted files vnode. Note that we call FindScache
     * instead of GetSCache since if the file's really gone, we won't be able
     * to fetch the status info anyway.
     */
    lock_ObtainWrite(&cm_scachelock);
    scp = cm_FindSCache(&lhp->OutDelFileFid);
    if (!scp) {
	scp = cm_NewSCache(&lhp->OutDelFileFid, dscp->volp);
    }
    lock_ReleaseWrite(&cm_scachelock);
    if (scp) {
	lock_ObtainWrite(&scp->llock);
	cm_MergeStatus(scp, &lhp->OutDelFileStatus, (afs_token_t *)0, 0, &rreq);
	/* sufficient to check output status link count, since if link count
	 * was ever 0, we know it'll never get back up.
	 */
	scp->m.ModFlags &= ~CM_MODCHANGE;
	/*
	 * note that the token will be broken on the deleted file if there
	 * are still >0 links left to it, so we'll get the stat right
	 */
#ifdef SGIMIPS
	if (lhp->OutDelFileStatus.linkCount == 0 && !cm_Active(scp)) {
#else  /* SGIMIPS */
	if (&lhp->OutDelFileStatus.linkCount == 0 && !cm_Active(scp)) {
#endif /* SGIMIPS */
	    if (cm_trySmush)
		call_smush = 1;
	}

	lock_ReleaseWrite(&scp->llock);
	if (call_smush)
	    cm_TryToSmush(scp, 0);
	cm_PutSCache(scp);
    }
done:
    if (volp)
	cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    return cm_CheckError(code, &rreq);
}


/*
 * Link call
 */
#ifdef SGIMIPS
cm_link(
    struct vnode *vp,
    struct vnode *dvp,
    char *namep,
    osi_cred_t *credp)
#else  /* SGIMIPS */
cm_link(vp, dvp, namep, credp)
    struct vnode *vp;
    struct vnode *dvp;
    char *namep;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    register struct cm_conn *connp;
    long code, srvix;
    struct lclHeap {
	afsTaggedName NameBuf;
	afsFetchStatus OutFidStatus, OutDirStatus;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    afsFid tfid;
    struct cm_volume *volp = NULL;
    error_status_t st;
    u_long startTime;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1, *vp2;
#endif /* EXPRESS_OPS */
    long irevcnt;
    register struct cm_scache *scp, *dscp;

#ifdef SGIMIPS
    VTOSC(vp, scp);
    VTOSC(dvp, dscp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
    dscp = VTOSC(dvp);
#endif /* SGIMIPS */
    icl_Trace3(cm_iclSetp, CM_TRACE_LINK, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_POINTER, dscp, ICL_TYPE_STRING, namep);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, dscp, &tvolp, &vp1, &vp2)) {
	code = cmglue_link(vp1, vp2, namep, credp);
	cm_ExpressRele(tvolp, vp1, vp2);
	return code;
    }
#endif /* EXPRESS_OPS */

    /*
     * create a hard link; new entry is namep in dir dscp
     */
    cm_InitReq(&rreq, credp);
    if (!AFS_hsame(scp->fid.Cell, dscp->fid.Cell) ||
	!AFS_hsame(scp->fid.Volume, dscp->fid.Volume)) {
	return EXDEV;
    }
    if (dscp->states & SC_RO) {
	return EROFS;
    }
    /*
     * Before creating a link, check to see if it is already there, to
     * keep the load on the server down.  This will also prevent looping
     * if there's competition for the creation.
     */
    code = nh_locallookup(dscp, namep, &tfid);
    if (code == 0) {
	return EEXIST;
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

    bzero((caddr_t)&lhp->NameBuf, sizeof(lhp->NameBuf));
    strcpy((caddr_t)&lhp->NameBuf.tn_chars[0], namep);
    lock_ObtainRead(&dscp->llock);
    code = cm_GetTokens(dscp, TKN_READ, &rreq, READ_LOCK);
    irevcnt = dscp->dirRevokes;
    lock_ReleaseRead(&dscp->llock);
    if (code) goto done;
    do {
	if (connp = cm_Conn(&dscp->fid,  MAIN_SERVICE(SRT_FX),
			    &rreq, &volp, &srvix)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_LINKSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    st = AFS_HardLink(connp->connp, &dscp->fid, &lhp->NameBuf,
			      &scp->fid, &osi_hZero, 0,
			      &lhp->OutFidStatus, &lhp->OutDirStatus,
			      &lhp->tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(st);
	    icl_Trace1(cm_iclSetp, CM_TRACE_LINKEND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, &dscp->fid, &rreq, volp, srvix, startTime));
    if (code) {
	if (code == EEXIST)
	    cm_FlushExists(dscp, namep);
	goto done;
    }
    /*
     * Update the parent dir's status
     */
    if (code = cm_UpdateStatus(dscp, &lhp->OutDirStatus, namep, namep,
			       &scp->fid, &rreq, irevcnt))
	goto done;

    /*
     * Get appropriate tokens to update the linked object's status
     */
    lock_ObtainWrite(&scp->llock);
    scp->m.ModFlags &= ~CM_MODCHANGE;
    cm_MergeStatus(scp, &lhp->OutFidStatus, (afs_token_t *) 0, 0, &rreq);
    lock_ReleaseWrite(&scp->llock);
done:
    if (volp)
	cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    return cm_CheckError(code, &rreq);
}


/*
 * Rename call
 */
#ifdef SGIMIPS
int
cm_rename(
    struct vnode *ovp,
    char *OldNamep,
    struct vnode *nvp,
    char *NewNamep,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_rename(ovp, OldNamep, nvp, NewNamep, credp)
    struct vnode *ovp, *nvp;
    char *OldNamep, *NewNamep;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    register struct cm_conn *connp;
    long code, oneDir, srvix;
    struct cm_scache *tsc;
    struct lclHeap {
	afsFidTaggedName OldName, NewName;
	afsFetchStatus OutOldFileStatus, OutNewFileStatus;
	afsFetchStatus OutOldDirStatus, OutNewDirStatus;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    afsFid OutOldFileFid, OutNewFileFid;
    struct cm_volume *volp = NULL;
    error_status_t st;
    u_long startTime;
    long irevcnto, irevcntn;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1, *vp2;
#endif /* EXPRESS_OPS */
    struct cm_scache *odscp, *ndscp;
    int call_smush = 0;
    afs_hyper_t openID;

#ifdef SGIMIPS
    VTOSC(ovp, odscp);
    VTOSC(nvp, ndscp);
#else  /* SGIMIPS */
    odscp = VTOSC(ovp);
    ndscp = VTOSC(nvp);
#endif /* SGIMIPS */
    icl_Trace4(cm_iclSetp, CM_TRACE_RENAME, ICL_TYPE_POINTER, odscp,
	       ICL_TYPE_STRING, OldNamep, ICL_TYPE_POINTER, ndscp,
	       ICL_TYPE_STRING, NewNamep);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(odscp, ndscp, &tvolp, &vp1, &vp2)) {
	code = cmglue_rename(vp1, OldNamep, vp2, NewNamep, credp);
	cm_ExpressRele(tvolp, vp1, vp2);
	return code;
    }
#endif /* EXPRESS_OPS */

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

    cm_InitReq(&rreq, credp);
    if (odscp->states & SC_RO) {
	code = EROFS;
	goto done;
    }
    /*
     * check for cross-device links right away.
     */
    if (!AFS_hsame(odscp->fid.Cell, ndscp->fid.Cell) ||
	 !AFS_hsame(odscp->fid.Volume, ndscp->fid.Volume)) {
	code = EXDEV;
	goto done;
    }
    bzero((caddr_t)&lhp->OldName, sizeof(lhp->OldName));
    strcpy((caddr_t)lhp->OldName.name.tn_chars, OldNamep);
    bzero((caddr_t)&lhp->NewName, sizeof(lhp->NewName));
    strcpy((caddr_t)lhp->NewName.name.tn_chars, NewNamep);
    lock_ObtainRead(&odscp->llock);
    code = cm_GetTokens(odscp, TKN_READ, &rreq, READ_LOCK);
    irevcnto = odscp->dirRevokes;
    lock_ReleaseRead(&odscp->llock);
    if (code) goto done;
    /*
     * cm_UpdateStatus (called below) will delete the name hash entry, but
     * why wait till then?  Doing it now as well makes it harder (but not
     * impossible) for other processes on the same client to obtain stale
     * file handles.
     */
    nh_delete(odscp, OldNamep);
    lock_ObtainRead(&ndscp->llock);
    code = cm_GetTokens(ndscp, TKN_READ, &rreq, READ_LOCK);
    irevcntn = ndscp->dirRevokes;
    lock_ReleaseRead(&ndscp->llock);
    if (code) goto done;
    cm_GetReturnOpenToken(ndscp, (char *)lhp->NewName.name.tn_chars, &openID);
    do {
	if (connp = cm_Conn(&odscp->fid,  MAIN_SERVICE(SRT_FX),
			    &rreq,&volp,&srvix)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_RENAMESTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
            st = AFS_Rename(connp->connp, &odscp->fid,
                            &lhp->OldName, &ndscp->fid, &lhp->NewName,
                            &openID, &osi_hZero, 0,
			    &lhp->OutOldDirStatus, &lhp->OutNewDirStatus,
			    &OutOldFileFid, &lhp->OutOldFileStatus,
                            &OutNewFileFid, &lhp->OutNewFileStatus,
			    &lhp->tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(st);
	    icl_Trace1(cm_iclSetp, CM_TRACE_RENAMEEND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, &odscp->fid, &rreq, volp, srvix, startTime));
    if (code)
	goto done;

    /* otherwise, adjust all incoming fids */
    OutOldFileFid.Cell = odscp->fid.Cell;
    OutNewFileFid.Cell = odscp->fid.Cell;

    /*
     * update dir status, including link counts and vnode times.  Also, as long
     * as we have the tokens, update the directory contents properly.
     */
    oneDir = (ndscp == odscp);
    if (oneDir) {
	/*
	 * Update the parent dir's status
	 */
	if (code = cm_UpdateStatus(ndscp, &lhp->OutNewDirStatus,
				   (char *)lhp->OldName.name.tn_chars,
				   NewNamep, &OutOldFileFid, &rreq, irevcntn))
	    goto done;
    } else {
	/*
	 * Update the New parent dir's status
	 */
	if (code = cm_UpdateStatus(ndscp, &lhp->OutNewDirStatus,
				   (char *)lhp->OldName.name.tn_chars,
				   NewNamep, &OutOldFileFid, &rreq, irevcntn))
	    goto done;
	/*
	 * Update the Old parent dir's status
	 */
	if (code = cm_UpdateStatus(odscp, &lhp->OutOldDirStatus,
				   (char *)lhp->OldName.name.tn_chars,
				   (char *)0, &OutOldFileFid, &rreq, irevcnto))
	    goto done;
    }

    /* also, update moving vnode, since we may have seen intermediate
     * vnode results while rename is executing.
     * Now, we've dealt with the status and contents of the two dirs.  We
     * next update the moving file's information (iff it is a dir moving
     * between two dirs, since rename changes moving dirs' ".." pointers).
     */
    lock_ObtainWrite(&cm_scachelock);
    tsc = cm_FindSCache(&OutOldFileFid);
    if (!tsc)
	tsc = cm_NewSCache(&OutOldFileFid, odscp->volp);
    lock_ReleaseWrite(&cm_scachelock);
    if (tsc) {
	lock_ObtainWrite(&tsc->llock);
	cm_MergeStatus(tsc, &lhp->OutOldFileStatus, (afs_token_t *) 0, 0, &rreq);
	if (cm_vType(tsc) == VDIR) {
	    nh_delete(tsc, "..");
	    cm_InvalidateAllSegments(tsc, CM_INVAL_SEGS_MAKESTABLE);
	}
	lock_ReleaseWrite(&tsc->llock);
	cm_PutSCache(tsc);
    }

    /*
     * Next, we update the status of the deleted file, if any
     */
    if (OutNewFileFid.Vnode) {
	lock_ObtainWrite(&cm_scachelock);
	tsc = cm_FindSCache(&OutNewFileFid);
	if (!tsc) {
	    tsc = cm_NewSCache(&OutNewFileFid, odscp->volp);
	}
	lock_ReleaseWrite(&cm_scachelock);
	if (tsc) {
	    lock_ObtainWrite(&tsc->llock);
	    cm_MergeStatus(tsc, &lhp->OutNewFileStatus, NULL, 0, &rreq);
	    /*
	     * now, if the updated link count is 0, we get rid of it from
	     * the cache, too
	     */
	    if (&lhp->OutNewFileStatus.linkCount == 0 && !cm_Active(tsc)) {
		/*
		 * if this was last guy (probably) discard from cache. We
		 * have to be careful not get rid of the stat information,
		 * since otherwise  operations will start failing even if
		 * the file was still open (or otherwise active), and the
		 * server no longer has the info.  If the file still has
		 * valid links, we'll get a break-callback msg from the
		 * server, so it doesn't matter that we don't discard the
		 * status info try to discard from cache to save space.
		 */
		if (cm_trySmush)
		    call_smush = 1;
	    }
	    lock_ReleaseWrite(&tsc->llock);
	    if (call_smush)
		cm_TryToSmush(tsc, 0);
	    cm_PutSCache(tsc);
	}
    }
    code = 0;
done:
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    if (volp)
	cm_EndVolumeRPC(volp);
    return cm_CheckError(code, &rreq);
}


/*
 * Create Dir call
 */
#ifdef SGIMIPS
int
cm_mkdir(
    struct vnode *dvp,
    char *namep,
    struct vattr *attrsp,
    struct vnode **vpp,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_mkdir(dvp, namep, attrsp, vpp, credp)
    struct vnode *dvp;
    char *namep;
    struct vattr *attrsp;
    struct vnode **vpp;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    long code, srvix;
    register struct cm_conn *connp;
    struct cm_server *tserverp;
    afsFid newFid;
    struct cm_scache *tscp;
    struct lclHeap {
	afsTaggedName NameBuf;
	afsStoreStatus InStatus;
	afsFetchStatus OutFidStatus, OutDirStatus;
	afs_token_t Token;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    struct cm_volume *volp = NULL;
    error_status_t st;
    u_long startTime;
    long irevcnt;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1, *vpx;
#endif /* EXPRESS_OPS */
    struct cm_scache *dscp, *scp;

#ifdef SGIMIPS
    VTOSC(dvp, dscp);
#else  /* SGIMIPS */
    dscp = VTOSC(dvp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_MKDIR, ICL_TYPE_POINTER, dscp,
	       ICL_TYPE_STRING, namep);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(dscp, (struct cm_scache *) 0, &tvolp, &vp1,
			(struct vnode **) 0)) {
	code = cmglue_mkdir(vp1, namep, attrsp, &vpx, credp);
	if (code == 0) {
#ifdef SGIMIPS
	    struct cm_scache *scpx;
#endif /* SGIMIPS */
	    code = cm_MapDown(dscp, vpx, &scp);
	    *vpp = vpx;
#ifdef SGIMIPS
	    VTOSC(vpx, scpx);
	    cm_PutSCache(scpx);
#else  /* SGIMIPS */
	    cm_PutSCache(VTOSC(vpx));
#endif /* SGIMIPS */
	}
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    cm_InitReq(&rreq, credp);
    if (!ENameOK(namep) || strlen(namep) >= sizeof(lhp->NameBuf.tn_chars)) {
	return EINVAL;
    }
    if (dscp->states & SC_RO) {
	return EROFS;
    }
    /*
     * Before creating a directory, check to see if it is already there, to
     * keep the load on the server down.  This will also prevent looping
     * if there's competition for the creation.
     */
    code = nh_locallookup(dscp, namep, &newFid);
    if (code == 0) {
	return EEXIST;
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

    bzero((caddr_t)&lhp->NameBuf, sizeof(lhp->NameBuf));
    strcpy((caddr_t)&lhp->NameBuf.tn_chars[0], namep);
    lock_ObtainRead(&dscp->llock);
    code = cm_GetTokens(dscp, TKN_READ, &rreq, READ_LOCK);
    irevcnt = dscp->dirRevokes;
    lock_ReleaseRead(&dscp->llock);
    if (code) goto done;
    lhp->InStatus.mask = AFS_SETMODTIME | AFS_SETMODE | AFS_SETACCESSTIME;
#ifdef SGIMIPS
    osi_afsGetTime(&lhp->InStatus.modTime);
#else  /* SGIMIPS */
    osi_GetTime((struct timeval *) &lhp->InStatus.modTime);
#endif /* SGIMIPS */
    lhp->InStatus.accessTime = lhp->InStatus.modTime;
    lhp->InStatus.cmask = osi_GetCMASK();
    lhp->InStatus.mode = attrsp->va_mode & 0xffff; /*only care of protection bits */
    cm_StartTokenGrantingCall();
    do {
	tserverp = 0;
	if (connp = cm_Conn(&dscp->fid,  MAIN_SERVICE(SRT_FX),
			    &rreq,&volp,&srvix)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_MKDIRSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    st = AFS_MakeDir(connp->connp, &dscp->fid, &lhp->NameBuf,
                             &lhp->InStatus, &osi_hZero, AFS_FLAG_RETURNTOKEN,
			     &newFid, &lhp->OutFidStatus, &lhp->OutDirStatus,
			     &lhp->Token, &lhp->tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(st);
	    tserverp = connp->serverp;
	    cm_SetReqServer(&rreq, tserverp);
	    icl_Trace1(cm_iclSetp, CM_TRACE_MKDIREND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, &dscp->fid, &rreq, volp, srvix, startTime));
    if (code) {
	if (code == EEXIST)
	    cm_FlushExists(dscp, namep);
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	goto done;
    }
    /* adjust incoming fid */
    newFid.Cell = dscp->fid.Cell;
    /*
     * Update the parent dir's status
     */
    code = cm_UpdateStatus(dscp, &lhp->OutDirStatus, namep,
			   namep, &newFid, &rreq, irevcnt);
    if (code) {
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	goto done;
    }
    /*
     * Now create the scache entry locally with the information
     * provided from the mkdir RPC.
     */
    lock_ObtainWrite(&cm_scachelock);
    tscp = cm_FindSCache(&newFid);
    if (!tscp) {
	if (! (tscp = cm_NewSCache(&newFid, dscp->volp))) {
	    lock_ReleaseWrite(&cm_scachelock);
	    cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				    (struct cm_volume *) 0, startTime);
	    code = ENOENT;
	    goto done;
	}
    }
    lock_ReleaseWrite(&cm_scachelock); /* Locking hierachy */
    lock_ObtainWrite(&tscp->llock);
    cm_EndTokenGrantingCall(&lhp->Token, tserverp, tscp->volp, startTime);
    cm_MergeStatus(tscp, &lhp->OutFidStatus, &lhp->Token, 0, &rreq);
    /* Check for race with deletion (!?) */
    if (code = cm_GetTokens(tscp, TKN_OPEN_PRESERVE, &rreq, WRITE_LOCK)) {
	lock_ReleaseWrite(&tscp->llock);
	cm_PutSCache(tscp);
	tscp = (struct cm_scache *) 0;
	goto done;
    }
#ifdef SCACHE_PUBLIC_POOL
    *vpp = cm_get_vnode(tscp);
    if (*vpp == NULLVP) {
	cm_PutSCache(tscp);
	code = ENOMEM;
    }
#else
    *vpp = SCTOV(tscp);
#endif /* SCACHE_PUBLIC_POOL */
    lock_ReleaseWrite(&tscp->llock);

done:
    if (volp)
	cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    return cm_CheckError(code, &rreq);
}


/*
 * Remove Dir Call
 */
#ifdef SGIMIPS
int
cm_rmdir(
    struct vnode *dvp,
    char *namep,
    struct vnode *junkcp,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_rmdir(dvp, namep, junkcp, credp)
    struct vnode *dvp;
    char *namep;
    struct vnode *junkcp;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    long code, srvix;
    register struct cm_conn *connp;
    long irevcnt;
    struct lclHeap {
	afsFetchStatus OutDirStatus, OutFileStatus;
	afsFidTaggedName RemoveDirName;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    afsFid RemoveDirFid;
    struct cm_volume *volp = NULL;
    error_status_t st;
    u_long startTime;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    struct cm_scache *dscp;
    struct cm_scache *rmsp;

#ifdef SGIMIPS
    VTOSC(dvp, dscp);
#else  /* SGIMIPS */
    dscp = VTOSC(dvp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_RMDIR, ICL_TYPE_POINTER, dscp,
	       ICL_TYPE_STRING, namep);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(dscp, (struct cm_scache *)0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_rmdir(vp1, namep, junkcp, credp);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    cm_InitReq(&rreq, credp);
    if (dscp->states & SC_RO) {
	return EROFS;
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

    bzero((caddr_t)&lhp->RemoveDirName, sizeof(lhp->RemoveDirName));
    strcpy((caddr_t)&lhp->RemoveDirName.name.tn_chars[0], namep);
    lock_ObtainRead(&dscp->llock);
    code = cm_GetTokens(dscp, TKN_READ, &rreq, READ_LOCK);
    irevcnt = dscp->dirRevokes;
    lock_ReleaseRead(&dscp->llock);
    if (code) goto done;
    /*
     * cm_UpdateStatus (called below) will delete the name hash entry, but
     * why wait till then?  Doing it now as well makes it harder (but not
     * impossible) for other processes on the same client to obtain stale
     * file handles.
     */
    nh_delete(dscp, namep);
    do {
	if (connp = cm_Conn(&dscp->fid,  MAIN_SERVICE(SRT_FX),
			    &rreq, &volp, &srvix)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_RMDIRSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    st = AFS_RemoveDir(connp->connp, &dscp->fid,
			       &lhp->RemoveDirName, &osi_hZero, &osi_hZero,
			       0, &lhp->OutDirStatus, &RemoveDirFid,
			       &lhp->OutFileStatus, &lhp->tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(st);
	    icl_Trace1(cm_iclSetp, CM_TRACE_RMDIREND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, &dscp->fid, &rreq, volp, srvix, startTime));
    if (code)
	goto done;
    /*
     * Update the parent dir's status
     */
    code = cm_UpdateStatus(dscp, &lhp->OutDirStatus, namep, (char *)0, (afsFid *)0,
			   &rreq, irevcnt);
    /*
     * We must call cm_InvalidateAllSegments on the delete'd dir in case
     * someone's cd'd into there, for POSIX compliance.  Also must
     * invalidate "." and ".." entries from it, since dir itself has changed.
     *
     * We call cm_FindSCache, not cm_GetSCache.  If there's no scache entry
     * for the deleted directory, there are no segments and no name hash
     * entries, so we don't have to do anything.  We don't have to contact
     * the server again.  It's unlikely that there will be no scache entry,
     * because most kernels call VOP_LOOKUP before calling VOP_RMDIR.
     */
    RemoveDirFid.Cell = dscp->fid.Cell;
    lock_ObtainWrite(&cm_scachelock);
    rmsp = cm_FindSCache(&RemoveDirFid);
    lock_ReleaseWrite(&cm_scachelock);
    if (rmsp) {
	/* if we found it, get rid of info mentioned above.
	 */
	lock_ObtainWrite(&rmsp->llock);
	cm_InvalidateAllSegments(rmsp, CM_INVAL_SEGS_MAKESTABLE);
	nh_delete(rmsp, ".");
	nh_delete(rmsp, "..");
	lock_ReleaseWrite(&rmsp->llock);
	cm_PutSCache(rmsp);
    }
done:
    if (volp)
	cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    return cm_CheckError(code, &rreq);
}

/*
 * Readdir: Not the same as cm_read because we don't know how big the thing
 * will be until the chunk has arrived. Thus, we don't do read-while-fetching.
 *
 * Call to read a directory. Our cookie (offset) value that we return is the
 * offset within our chunk (of our directory in Sun V4 format).  However, there
 * are two vnode  formats, one without these extra fields.
 */

/* As of DCE 1.0.2, there is a third directory format, the wire format, that
    includes a per-name codesetTag. */

#ifdef SGIMIPS
int
cm_readdir(
  struct vnode *vp,
  struct uio *uiop,
  osi_cred_t *credp,
  int *eofp,
  int isPX)			/* ignored; must be 0 */
#else  /* SGIMIPS */
int
cm_readdir(vp, uiop, credp, eofp, isPX)
  struct vnode *vp;
  struct uio *uiop;
  osi_cred_t *credp;
  int *eofp;
  int isPX;			/* ignored; must be 0 */
#endif /* SGIMIPS */
{
    long totalLength;		/* bytes to read */
#ifdef SGIMIPS
    afs_hyper_t filePos;	/* server cookie offset for request */
#else  /* SGIMIPS */
    long filePos;		/* server cookie offset for request */
#endif /* SGIMIPS */
    register struct cm_dcache *dcp=0;	/* the data cache entry */
    long len;			/* bytes left in chunk */
    long code;			/* error code */
#ifdef SGIMIPS
    afs_hyper_t cookie;		/* server cookie of chunk */
#else  /* SGIMIPS */
    long cookie;		/* server cookie of chunk */
#endif /* SGIMIPS */
    long dirChunk;		/* chunk # we're handling */
    char *osifilep;		/* open file for reading chunk */
    struct cm_rrequest rreq;	/* request structure */
    int orecSize;		/* size of output record */
#ifdef SGIMIPS
    dirent_t    *newirixdir;
    irix5_dirent_t *oldirixdir;
    char        *curdirp;
    char        *lastdirp;
    int is32;
#else  /* SGIMIPS */
    struct dir_minentry *toutEntryp;	/* entry we're writing */
    struct dir_minentry *lastEntryp;	/* last entry we wrote (for patching) */
#endif /* SGIMIPS */
    struct dir_minwireentry *tinEntryp;	/* entry we're reading */
    char *tinDatap;		/* buffer to use for readdir */
    char *toutDatap;		/* buffer to use for output of readdir */
#ifdef EXPRESS_OPS
    struct volume *tvolp;	/* express path volume */
    struct vnode *vp1;		/* express path vnode */
#endif /* EXPRESS_OPS */
    struct cm_scache *scp;	/* scache entry */
    long inBytes;		/* bytes in input dir block buffer */
    long outBytes;		/* bytes written to new dir block buffer */
    long chunkOffset;		/* local position we're at in the chunk */
#ifdef SGIMIPS
    afs_hyper_t chunkCookie;	/* cookie value we're currently scanning */
#else  /* SGIMIPS */
    long chunkCookie;		/* cookie value we're currently scanning */
#endif /* SGIMIPS */
    long oblockSize;		/* size of the blocks we're outputing */

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    *eofp = 0;
    icl_Trace2(cm_iclSetp, CM_TRACE_READDIR, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, uiop->osi_uio_resid);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_readdir(vp1, uiop, credp, eofp);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    cm_InitReq(&rreq, credp);
    if (scp->states & SC_VDIR) {
#ifdef SGIMIPS
	return (int)cm_ReadVDir(scp, uiop, eofp, &rreq);
#else  /* SGIMIPS */
	return cm_ReadVDir(scp, uiop, eofp, &rreq);
#endif /* SGIMIPS */
    }

    /* handle silly allocation now */
    tinDatap = (char *) osi_AllocBufferSpace();	/* space for reading dir */
    toutDatap = tinDatap + DIR_BLKSIZE;		/* space for writing dir */

    /* Compute request parameters.  Compromise: some systems
     * require you to read exactly what's been requested, while
     * others want data blocked in DIR_BLKSIZE pieces.  We handle
     * both by reading what you've asked for if you request fewer
     * than DIR_BLKSIZE bytes, otherwise we just round the length
     * down to the next lowest multiple of DIR_BLKSIZE.
     *
     * This algorithm expects to divide the output stream into
     * fixed-sized blocks, and so to handle smaller requests, we
     * tell the code that our fixed block size is the # of bytes
     * we've been asked for (oblockSize).
     *
     * In either case, we pad the record out to the required size.
     */
    if (uiop->osi_uio_resid < DIR_BLKSIZE)
	oblockSize = totalLength = uiop->osi_uio_resid;
    else {
	totalLength = uiop->osi_uio_resid & ~(DIR_BLKSIZE-1);
	oblockSize = DIR_BLKSIZE;
    }

    filePos = uiop->osi_uio_offset;

    /* compute basic output state variables */
    outBytes = 0;	/* bytes currently in output buffer */
#ifdef SGIMIPS 
    lastdirp = 0;
    curdirp = (char *) toutDatap;
#else  /* SGIMIPS */
    lastEntryp = 0;	/* none yet */
    toutEntryp = (struct dir_minentry *) toutDatap;
#endif /* SGIMIPS */

    /* first, find which chunk likely has our data, by scanning
     * the cookie list for the biggest guy <= filePos.  Note
     * that the info we require may not be in that chunk, however;
     * it may be further down the line.
     */
    lock_ObtainWrite(&scp->llock);
    cm_FindClosestChunk(scp, filePos, &dirChunk);
    while (totalLength > 0) {	/* while still more to return */
	icl_Trace2(cm_iclSetp, CM_TRACE_READDIRLOOP,
		  ICL_TYPE_LONG, filePos, ICL_TYPE_LONG, dirChunk);
	/* check to see if the next chunk doesn't exist, if so,
	 * we're at dir EOF.
	 */
	if (cm_LookupCookie(scp, dirChunk, &cookie) == 0) {
	    if (cookie == -1) {
		/* means we hit dir's EOF */
		*eofp = 1;
		code = 0;
		break;
	    }
	    chunkCookie = cookie;	/* where we'll start scanning */
	    icl_Trace1(cm_iclSetp, CM_TRACE_READDIRCFOUND, ICL_TYPE_LONG, cookie);
	}
	else {
	    /* don't know where we are (and we'll need to know).  Start
	     * scanning from last known place.
	     */
	    cm_FindClosestChunk(scp, filePos, &dirChunk);
	    icl_Trace1(cm_iclSetp, CM_TRACE_READDIRCLOST, ICL_TYPE_LONG, dirChunk);
	    continue;
	}
	lock_ReleaseWrite(&scp->llock);

	/* otherwise, chunk should be good, so prepare to get it */
	dcp = cm_GetDCache(scp, dirChunk);
	if (!dcp) {
	    code = EIO;
	    goto out;
	}
	lock_ObtainWrite(&scp->llock);

	/* try to fill the chunk we need with data */
	if (cm_LookupCookie(scp, dirChunk+1, &cookie) != 0) {
	    /* if we don't already know cookie value of next chunk,
	     * we need to force fetch to ensure cookie is computed
	     * by fetch code.
	     */
	    lock_ObtainWrite(&dcp->llock);
	    cm_InvalidateOneSegment(scp, dcp, 1);
	    lock_ReleaseWrite(&dcp->llock);
	}
	code = cm_GetDLock(scp, dcp, CM_DLOCK_READ, WRITE_LOCK, &rreq);
	if (code) {
	    cm_PutDCache(dcp);
	    if (code == CM_ERR_DIRSYNC) {
		/* dir changed, and we're lost.  We may have to refetch
		 * most of the dir, just to rediscover which chunk
		 * to put the new dir listing into.
		 */
		cm_FindClosestChunk(scp, filePos, &dirChunk);
		icl_Trace2(cm_iclSetp, CM_TRACE_READDIRSYNC,
			   ICL_TYPE_LONG, filePos, ICL_TYPE_LONG, dirChunk);
		continue;	/* start again */
	    }
	    /* otherwise, we have a real error */
	    lock_ReleaseWrite(&scp->llock);
	    goto out;
	}

	/* Now we have the dcache locked, too.
	 * Compute the # of bytes to process in this chunk,
	 * offset within chunk and # of bytes in chunk input buffer.
	 */
	len = dcp->f.chunkBytes;
	chunkOffset = 0;	/* where, relative to chunk, to read */
	if (scp->ds.scanChunk >= 0 && scp->ds.scanChunk == dirChunk
	    && filePos >= scp->ds.scanCookie) {
	    len -= scp->ds.scanOffset; /* # of bytes to get to scanCookie */
	    chunkOffset = scp->ds.scanOffset;	/* where we are in chunk */
	    chunkCookie = scp->ds.scanCookie;	/* and in cookie-space */
	}
	inBytes = 0;		/* bytes left in input chunk buffer */
	if (len == 0) {
	    lock_ReleaseWrite(&dcp->llock);
	    cm_PutDCache(dcp);
	    goto done;	/* for safety */
	}
#ifdef SGIMIPS
	is32 = ABI_IS_IRIX5(GETDENTS_ABI(get_current_abi(), uiop));
#endif /* SGIMIPS */

#ifdef AFS_VFSCACHE
	osifilep = cm_CFileOpen(&dcp->f.handle);
#else
	osifilep = cm_CFileOpen(dcp->f.inode);
#endif /* AFS_VFSCACHE */
	if (!osifilep) panic("readdir open");
	/* while there's still stuff to look at in this chunk */
	while(len > 0) {
	    if (inBytes == 0) {
		icl_Trace2(cm_iclSetp, CM_TRACE_READDIRREAD,
			   ICL_TYPE_LONG, dirChunk, ICL_TYPE_LONG, chunkOffset);
		/* read new chunk of dir */
		inBytes = cm_CFileRW(osifilep, UIO_READ, tinDatap, chunkOffset,
				  DIR_BLKSIZE, osi_credp);
		if (inBytes < 0) {
		    lock_ReleaseWrite(&scp->llock);
		    lock_ReleaseWrite(&dcp->llock);
		    cm_PutDCache(dcp);
		    cm_CFileClose(osifilep);
		    code = EIO;
		    goto out;
		}

		/* remember offset we've just reached in scan hint */
		scp->ds.scanChunk = dirChunk;
		scp->ds.scanOffset = chunkOffset;
		scp->ds.scanCookie = chunkCookie;

		/* update input scanning parameters */
		chunkOffset += DIR_BLKSIZE;
		tinEntryp = (struct dir_minwireentry *) tinDatap;

		/* if still 0, we hit chunk EOF */
		if (inBytes == 0) break;
	    }
	    /* put wire entry into host byte order */
	    tinEntryp->inode = ntohl(tinEntryp->inode);
	    tinEntryp->recordlen = ntohs(tinEntryp->recordlen);
	    tinEntryp->namelen = ntohs(tinEntryp->namelen);
	    tinEntryp->offset = ntohl(tinEntryp->offset);
	    /* tinEntryp->tag = ntohl(tinEntryp->tag); uncomment when used */
	    /* tinEntryp->dir_highOffset = ntohl(tinEntryp->dir_highOffset); uncomment when used */

	    /* now look at entry, and see if it is one we should copy out.
	     * NB dir_offset is the offset of the next dir record, not
	     * this one.
	     */
	    if (tinEntryp->inode != 0 && filePos < tinEntryp->offset) {
		/* see if this entry will fit into output buffer */
#ifdef SGIMIPS
		if (is32) {
                        orecSize = IRIX5_DIRENTSIZE(tinEntryp->namelen +1);
                } else {
                        orecSize = DIRENTSIZE(tinEntryp->namelen +1);
		}
#else  /* SGIMIPS */
		orecSize = NDIRSIZE(tinEntryp);
#endif /* SGIMIPS */
		if (orecSize + outBytes > oblockSize) {
#ifdef SGIMIPS
                    if (lastdirp)  {
			if (is32) {
                                oldirixdir=(irix5_dirent_t *)lastdirp;
                                oldirixdir->d_reclen += (oblockSize-outBytes);
                        }
                        else {
                                newirixdir=(dirent_t *)lastdirp;
                                newirixdir->d_reclen += (oblockSize-outBytes);
                        }
                    }
#else  /* SGIMIPS */
		    if (lastEntryp)
			lastEntryp->recordlen += (oblockSize-outBytes);
#endif /* SGIMIPS */
		    /* uio move this stuff out */
		    code = osi_uiomove(toutDatap, oblockSize, UIO_READ, uiop);
		    if (code) {
			lock_ReleaseWrite(&scp->llock);
			lock_ReleaseWrite(&dcp->llock);
			cm_PutDCache(dcp);
			cm_CFileClose(osifilep);
			goto out;
		    }
		    uiop->osi_uio_offset = chunkCookie;	/* for next call */
#ifdef SGIMIPS
                    curdirp = (char *)toutDatap;
#else  /* SGIMIPS */
		    toutEntryp = (struct dir_minentry *) toutDatap;
#endif /* SGIMIPS */
		    outBytes = 0;
		    totalLength -= oblockSize;
		    if (totalLength <= 0) {
			cm_CFileClose(osifilep);
			lock_ReleaseWrite(&dcp->llock);
			cm_PutDCache(dcp);
			code = 0;	/* success, and we're done for now */
			goto done;
		    }
		}
#ifdef SGIMIPS
                if (is32) {
                        oldirixdir = (irix5_dirent_t *)curdirp;
                        oldirixdir->d_off = tinEntryp->offset;
                        oldirixdir->d_ino = tinEntryp->inode +
                                 (AFS_hgetlo(scp->fid.Volume) << 16);
                        strcpy((char *)oldirixdir->d_name,
                                ((char *)tinEntryp) + SIZEOF_DIR_MINWIREENTRY);
                        oldirixdir->d_reclen = orecSize;
                } else {
                        newirixdir = (dirent_t *)curdirp;
                        newirixdir->d_off = tinEntryp->offset;
                        newirixdir->d_ino = tinEntryp->inode +
                                 (AFS_hgetlo(scp->fid.Volume) << 16);
                        strcpy((char *)newirixdir->d_name,
                                ((char *)tinEntryp) + SIZEOF_DIR_MINWIREENTRY);
                        newirixdir->d_reclen = orecSize;
                }
                /* now adjust output pointers and counters */
                outBytes += orecSize;
                lastdirp = curdirp;
                curdirp = curdirp + orecSize;
#else  /* SGIMIPS */
#if defined(AFS_AIX31_ENV) || defined (AFS_SUNOS5_ENV)
		/* if there's an offset field.  NB: offset is always
		 * server cookie value, even on client side.
		 */
		toutEntryp->offset = tinEntryp->offset;
#endif
		/* we fudge the inode # to add in some volume bits,
		 * to reduce the chances of an inode collision.
		 */
		toutEntryp->inode = tinEntryp->inode
		    + (AFS_hgetlo(scp->fid.Volume) << 16);
		strcpy(((char *)toutEntryp) + SIZEOF_DIR_MINENTRY,
		       ((char *)tinEntryp) + SIZEOF_DIR_MINWIREENTRY);
		toutEntryp->recordlen = orecSize;
#ifndef AFS_SUNOS5_ENV
		/* SVR4 uses strlen to compute this */
		toutEntryp->namelen = tinEntryp->namelen;
#endif /* AFS_SUNOS5_ENV */

		/* now adjust output pointers and counters */
		lastEntryp = toutEntryp;
		outBytes += orecSize;
		toutEntryp = (struct dir_minentry *)
		    ((char *)toutEntryp + orecSize);
#endif /* SGIMIPS */
		/* moved out a record, now adjust filePos so we don't
		 * reread this record (if a SYNC_ERROR occurs, for
		 * instance).  Filepos = cookie of next record to read.
		 */
		filePos = tinEntryp->offset;
	    }	/* if some data in this record */

	    /* watch for bogus input record */
	    code = tinEntryp->recordlen;	/* input record length */
	    if (code == 0) {
		code = EIO;	/* return EIO when data stream is bad */
		lock_ReleaseWrite(&scp->llock);
		lock_ReleaseWrite(&dcp->llock);
		cm_PutDCache(dcp);
		cm_CFileClose(osifilep);
		goto out;
	    }

	    /* process next input record */
	    chunkCookie = tinEntryp->offset;
	    inBytes -= code;		/* bytes remaining in record */
	    len -= code;		/* bytes remaining in chunk */
	    tinEntryp = (struct dir_minwireentry *) (((char *)tinEntryp) + code);
	}	/* while len > 0 */
	dirChunk++;	/* switch to next chunk */

	/* we're done with this chunk prepare to get a new one */
	cm_CFileClose(osifilep);
	lock_ReleaseWrite(&dcp->llock);
	cm_PutDCache(dcp);
    }
    /* we might be here due to input EOF or output buffer full.  If input
     * EOF, may have useful stuff in the output buffer; copy it out.
     */
  done:
    if (totalLength >= oblockSize && outBytes > 0) {
#ifdef SGIMIPS
        if (lastdirp) {
                if (is32) {
                        oldirixdir=(irix5_dirent_t *)lastdirp;
                        oldirixdir->d_reclen += (oblockSize-outBytes);
                } else {
                        newirixdir=(dirent_t *)lastdirp;
                        newirixdir->d_reclen += (oblockSize-outBytes);
                }
        }
#else  /* SGIMIPS */
	if (lastEntryp) {
	    lastEntryp->recordlen += (oblockSize - outBytes);
	}
#endif /* SGIMIPS */
	code = osi_uiomove(toutDatap, oblockSize, UIO_READ, uiop);
	if (code) {
	    lock_ReleaseWrite(&scp->llock);
	    goto out;
	}
	uiop->osi_uio_offset = filePos;	/* for next call */
    }

    /* update the access time per POSIX */
    cm_MarkTime(scp, 0, credp, CM_MODACCESS, 0 /* Do not get tokens */);

    /* when we get here, we've copied out all the data the user wants */
    lock_ReleaseWrite(&scp->llock);
    code = 0;	/* success */

out:
    icl_Trace2(cm_iclSetp, CM_TRACE_READDIRRET,
	       ICL_TYPE_LONG, uiop->osi_uio_offset,
	       ICL_TYPE_LONG, code);
    osi_FreeBufferSpace((struct osi_buffer *) tinDatap);
    return cm_CheckError(code, &rreq);
}

/* helper for cm_symlink and cm_PCreateMount */
int32
cm_SymlinkOrMount(
		  struct cm_scache *dscp,
		  char *namep,
		  unsigned32 nameTag,
		  unsigned32 nameLen,
		  char *targetp,
		  unsigned32 targetTag,
		  unsigned32 targetLen,
		  int isMount,
		  struct cm_rrequest *rreqp)
{
    long code, alen, srvix;
    register struct cm_conn *connp;
    afsFid newFid;
    struct cm_server *tserverp;
    struct lclHeap {
	afsTaggedName NameBuf;
	afsTaggedPath PathBuf;
	afsStoreStatus InStatus;
	afsFetchStatus OutFidStatus;
	afsFetchStatus OutDirStatus;
	afs_token_t Token;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    struct cm_volume *volp = NULL;
    struct cm_scache *scp;
    error_status_t st;
    u_long startTime;
    long irevcnt;

    /* Need a parent directory that's not a virtual directory. */
    if (cm_vType(dscp) != VDIR) {
	return ENOTDIR;
    }
    if (dscp->states & (SC_VDIR | SC_RO))
	return EROFS;
    if (nameLen >= sizeof(lhp->NameBuf.tn_chars)
	|| targetLen >= sizeof(lhp->PathBuf.tp_chars))
	return E2BIG;

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
    bzero((char *)lhp, sizeof(struct lclHeap));

    lhp->NameBuf.tn_tag = nameTag;
    lhp->NameBuf.tn_length = nameLen;
    strncpy((char *)&lhp->NameBuf.tn_chars[0], namep, nameLen);

    /*
     * Before creating a symlink, check to see if it is already there, to
     * keep the load on the server down.  This will also prevent looping
     * if there's competition for the creation.
     */
    code = nh_locallookup(dscp, namep, &newFid);
    if (code == 0) {
	code = EEXIST;
	goto done;
    }
    lhp->InStatus.mask = AFS_SETMODTIME | AFS_SETMODE | AFS_SETACCESSTIME;
#ifdef SGIMIPS
    osi_afsGetTime(&lhp->InStatus.modTime);
#else /* SGIMIPS */
    osi_GetTime((struct timeval *)&lhp->InStatus.modTime);
#endif /* SGIMIPS */
    lhp->InStatus.accessTime = lhp->InStatus.modTime;
    lhp->InStatus.cmask = osi_GetCMASK();
    lhp->InStatus.mode = (isMount ? 0644 : 0777);
    lhp->PathBuf.tp_tag = targetTag;
    lhp->PathBuf.tp_length = targetLen;
    strncpy((char *)&lhp->PathBuf.tp_chars[0], targetp, targetLen);
#ifdef SGIMIPS
    alen = (long)strlen(targetp);		/* include the null */
#else  /* SGIMIPS */
    alen = strlen(targetp);		/* include the null */
#endif /* SGIMIPS */
    /* Mount points null out the final ``.'', but not normal symlinks. */
    if (!isMount)
	++alen;
    lock_ObtainRead(&dscp->llock);
    code = cm_GetTokens(dscp, TKN_READ, rreqp, READ_LOCK);
    irevcnt = dscp->dirRevokes;
    lock_ReleaseRead(&dscp->llock);
    if (code) goto done;
    cm_StartTokenGrantingCall();
    do {
	tserverp = 0;
	if (connp = cm_Conn(&dscp->fid,  MAIN_SERVICE(SRT_FX),
			    rreqp, &volp, &srvix)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_SYMLINKSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    st = AFS_Symlink(connp->connp, (afsFid *) &dscp->fid,
			     &lhp->NameBuf, &lhp->PathBuf, &lhp->InStatus,
			     &osi_hZero, AFS_FLAG_RETURNTOKEN,
			     &newFid, &lhp->OutFidStatus, &lhp->OutDirStatus,
			     &lhp->Token, &lhp->tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(st);
	    tserverp = connp->serverp;
	    cm_SetReqServer(rreqp, tserverp);
	    icl_Trace1(cm_iclSetp, CM_TRACE_SYMLINKEND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, &dscp->fid, rreqp, volp, srvix, startTime));
    if (code) {
	if (code == EEXIST)
	    cm_FlushExists(dscp, namep);
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	goto done;
    }

    /* adjust incoming fid */
    newFid.Cell = dscp->fid.Cell;

    /*
     * Update the parent dir's status
     */
    if (code = cm_UpdateStatus(dscp, &lhp->OutDirStatus, namep, namep, &newFid,
			       rreqp, irevcnt)) {
	cm_EndTokenGrantingCall((afs_token_t *) 0, (struct cm_server *) 0,
				(struct cm_volume *) 0, startTime);
	goto done;
    }
    /*
     * Now create the link's entry.  Note that no one can get a pointer to the
     * new cache entry until we release the scachelock lock.
     */
    lock_ObtainWrite(&cm_scachelock);
    if (!(scp = cm_FindSCache(&newFid)))
	scp = cm_NewSCache(&newFid, dscp->volp);
    lock_ReleaseWrite(&cm_scachelock);
    lock_ObtainWrite(&scp->llock);
    cm_EndTokenGrantingCall(&lhp->Token, tserverp, scp->volp, startTime);
    tokenStat.SymLink++;
    cm_MergeStatus(scp, &lhp->OutFidStatus, &lhp->Token, 0, rreqp);
    cm_vSetType(scp, VLNK);
    if (!scp->linkDatap) {
	scp->linkDatap = (char *) osi_Alloc(alen);
	strncpy(scp->linkDatap, targetp, alen-1);
	scp->linkDatap[alen-1] = 0;
    }
    lock_ReleaseWrite(&scp->llock);
    cm_PutSCache(scp);

done:
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    if (volp)
	cm_EndVolumeRPC(volp);
    icl_Trace1(cm_iclSetp, CM_TRACE_CRMOUNT_END, ICL_TYPE_LONG, code);
#ifdef SGIMIPS
    return (int32)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}


/*
 * Symbolic link call
 */
#ifdef SGIMIPS
int
cm_symlink(
    struct vnode *dvp,
    char *namep,
    struct vattr *attrsp,
    char *targetNamep,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_symlink(dvp, namep, attrsp, targetNamep, credp)
    struct vnode *dvp;
    char *namep;
    struct vattr *attrsp;
    char *targetNamep;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    long code;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    struct cm_scache *dscp;

#ifdef SGIMIPS
    VTOSC(dvp, dscp);
#else  /* SGIMIPS */
    dscp = VTOSC(dvp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_SYMLINK, ICL_TYPE_POINTER, dscp,
	       ICL_TYPE_STRING, namep);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(dscp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_symlink(vp1, namep, attrsp, targetNamep, credp);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    cm_InitReq(&rreq, credp);

#ifdef SGIMIPS
    code = cm_SymlinkOrMount(dscp, namep, 0, (unsigned32)strlen(namep), 
			     targetNamep, 0,
			     (unsigned32) strlen(targetNamep), 0, &rreq);
#else  /* SGIMIPS */
    code = cm_SymlinkOrMount(dscp, namep, 0, strlen(namep), targetNamep, 0,
			     strlen(targetNamep), 0, &rreq);
#endif /* SGIMIPS */
    return cm_CheckError(code, &rreq);
}


/*
 * Readlink call
 */
#ifdef SGIMIPS
int
cm_readlink(
    struct vnode *vp,
    struct uio *uiop,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_readlink(vp, uiop, credp)
    struct vnode *vp;
    struct uio *uiop;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code;
    struct cm_rrequest rreq;
    register char *linkContentsp;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    struct cm_scache *scp;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    icl_Trace1(cm_iclSetp, CM_TRACE_READLINK, ICL_TYPE_POINTER, scp);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_readlink(vp1, uiop, credp);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    cm_InitReq(&rreq, credp);
    lock_ObtainWrite(&scp->llock);
    if (cm_vType(scp) != VLNK) {
	code = EINVAL;
	lock_ReleaseWrite(&scp->llock);
	goto done;
    }
    if (scp->states & SC_VDIR) {
	code = 0;
    } else {
	code = cm_HandleLink(scp, &rreq);	/* obtains read tokens */
    }
    if (!code) {
	/*
	 * finally uiomove it to user-land
	 */
	if (linkContentsp = scp->linkDatap)
            code = osi_uiomove(linkContentsp,strlen(linkContentsp),UIO_READ,uiop);
	else
	    code = EIO;
    }
    lock_ReleaseWrite(&scp->llock);
done:
    return cm_CheckError(code, &rreq);
}


/*
 * Fsync call
 */
#ifdef SGIMIPS
cm_fsync(
    struct vnode *vp,
    int flag,
    osi_cred_t *credp,
    off_t start,
    off_t stop)
#else  /* SGIMIPS */
cm_fsync(vp, credp)
    struct vnode *vp;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code = 0;
    struct cm_rrequest rreq;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    struct cm_scache *scp;

#ifdef SGIMIPS
/* 
 * XXX
 * XXX  We need to do something with the start and stop values
 * XXX  that are now given to us in kudzu
 * XXX
 */
#endif /* SGIMIPS */

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    icl_Trace1(cm_iclSetp, CM_TRACE_FSYNC, ICL_TYPE_POINTER, scp);
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_fsync(vp1, credp);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */
    cm_InitReq(&rreq, credp);
    lock_ObtainRead(&scp->llock);
#ifdef SGIMIPS
    if (scp->writers > 0 || (flag & FSYNC_INVAL)) {
        /*
         * Do the cm_SyncDCache if we have somebody writing, or
         * we are a msync call with data cached on our vp
         */
        /*
         * Put the file back
         */
        lock_ReleaseRead(&scp->llock);
        code = cm_SyncDCache(scp, (flag ? flag: 1), credp);
#else  /* SGIMIPS */
    if (scp->writers > 0) {
        /*
	 * Put the file back
	 */
	lock_ReleaseRead(&scp->llock);
	code = cm_SyncDCache(scp, 1, credp);
#endif /* SGIMIPS*/
    } else
	lock_ReleaseRead(&scp->llock);
    return cm_CheckError(code, &rreq);
}


/*
 * Inactive call
 */
#ifdef SGIMIPS
int
cm_inactive(
    struct vnode *vp,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int
cm_inactive(vp, credp)
    struct vnode *vp;
    osi_cred_t *credp;
#endif /* SGIMIPS */
{
    struct cm_scache *scp;
    afsFid lfid;
    afsFid *fidp;

#ifdef SGIMIPS
    int s;
    afs_hyper_t length;
    int shared = 0;

    VTOSC(vp, scp);

    icl_Trace1(cm_iclSetp, CM_TRACE_INACTIVE, ICL_TYPE_POINTER, scp);
    s = VN_LOCK(vp);

    if ( vp->v_count != 0 || !(vp->v_flag & VINACT) )
        panic("cm_inactive: vp->v_count != 0 || !vp->v_flag & VINACT");

    if (scp->m.LinkCount) {
        length=scp->m.Length;
    } else {
        length=0;
    }
    VN_UNLOCK(vp, s);
    if (cm_vType(scp) == VREG) {
	if (vp->v_dpages) panic("cm_inactive: vp->v_dpages is set");
    }

    vn_bhv_remove(VN_BHV_HEAD(vp),&scp->cm_bhv_desc);

    osi_assert(vp->v_count == 0 );

    return VN_INACTIVE_CACHE;

#endif /* SGIMIPS */


#ifndef SGIMIPS 
    scp = VTOSC(vp);

    /* Return open tokens if requested.  Can't wait for this, so
     * we put it in a bkg request.
     */

#ifdef AFS_SUNOS5_ENV
#define RETURN_ON_INACTIVE (SC_RETURNREF | SC_RETURNOPEN)
#else
#define RETURN_ON_INACTIVE SC_RETURNREF
#endif

    if (scp->states & RETURN_ON_INACTIVE) {
	lfid = scp->fid;		/* Make local copy before sleeping */
#ifdef AFS_SUNOS5_ENV
	osi_PreemptionOff();
#endif
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
	(void) cm_bkgQueue(BK_RET_TOK_OP, 0, BK_FLAG_ALLOC, credp,
			(long)fidp, 0, 0, 0, 0);
#ifdef AFS_SUNOS5_ENV
	osi_RestorePreemption(0);
#endif
    }

#ifndef AFS_SUNOS5_ENV
    icl_Trace1(cm_iclSetp, CM_TRACE_INACTIVE, ICL_TYPE_POINTER, scp);
#else
    /*
     * VN_RELE increments the ref. count before calling VOP_INACTIVE
     * to prevent concurrent calls.  It is up to us to decrement it
     * again before returning.
     */
    (void) osi_PreemptionOff();
    mutex_enter(&vp->v_lock);
    osi_assert(vp->v_count != 0);
    vp->v_count -= 1;
    mutex_exit(&vp->v_lock);
    osi_RestorePreemption(0);
#endif /* AFS_SUNOS5_ENV */

    /* we don't have to check the express path on this call, since
     * this is a vnode management call, and UFS doesn't really care
     * when our vnodes' reference counts go to zero.  And, of course, it
     * would be a disaster to call inactive with a vnode whose reference count
     * is non-zero.
     */

    /*
     * Make cm_inactive a noop.  Otherwise, locking the desired vnode
     * would easily create a deadlock.
     */

    return 0;			/* LRU should do everything for us */
#endif /* !SGIMIPS */
}


/*
 * Bmap call
 */
#ifdef SGIMIPS
int
cm_bmap(
    struct vnode *vp,
    long abn,
    struct vnode **vpp,
    long *anbnp)
#else  /* SGIMIPS */
int
cm_bmap(vp, abn, vpp, anbnp)
    struct vnode *vp;
    long abn, *anbnp;
    struct vnode **vpp;
#endif /* SGIMIPS */
{
    struct cm_scache *scp;

#ifdef SGIMIPS
    VTOSC(vp,  scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */

    icl_Trace1(cm_iclSetp, CM_TRACE_BMAP, ICL_TYPE_POINTER, scp);
    if (vpp)
        *vpp = vp;
    if (anbnp)
	*anbnp = abn * (8192 / DEV_BSIZE);	/* in 512 byte units */
    return 0;
}


#ifndef AFS_SUNOS5_ENV
/* SUNOS5 version is in SUNOS5 subdir */

/*
 * Strategy call
 */
#ifdef SGIMIPS
int
cm_strategy (struct buf *bufp)
#else  /* SGIMIPS */
int
cm_strategy (bufp)
    struct buf *bufp;
#endif /* SGIMIPS */
{
    register long code;

    icl_Trace1(cm_iclSetp, CM_TRACE_STRATEGY, ICL_TYPE_POINTER, bufp);
    code = osi_MapStrategy(cm_ustrategy, bufp);
    return code;
}

#if defined(SGIMIPS) && defined(DEBUG)
#define BERR_LOGSZ	32
typedef struct berr_s {
    struct berr_s	*berr_next;
    buf_t		*berr_baddr;
    buf_t		berr_buf;
} berr_t;
berr_t	berr_log[BERR_LOGSZ];
berr_t	*berr_trace=NULL;
#endif /* defined(SGIMIPS) && defined(DEBUG) */

/*
 * Basic strategy routine, assuming stuff mapped in.
 *
 * This function doesn't do any express path checking, since by the time
 * our pagein/getpage routine(s) get called, if we're running on the express
 * path, the pagein/getpage routine will call the local file system's strategy
 * function, and we won't even make it to here.
 */
#ifdef SGIMIPS
int
cm_ustrategy(register struct buf *bufp)
#else  /* SGIMIPS */
cm_ustrategy(bufp)
    register struct buf *bufp;
#endif /* SGIMIPS */
{
    register long code=0;
    struct uio tuio;
#ifdef SGIMIPS
    register struct cm_scache *scp;
#else  /* SGIMIPS */
    register struct cm_scache *scp = VTOSC(bufp->b_vp);
#endif /* SGIMIPS */
    register long len = bufp->b_bcount;
    struct iovec tiovec[1];
#ifdef SGIMIPS
    afs_hyper_t offset, diff;
#endif /* SGIMIPS */

#ifdef SGIMIPS
    VTOSC(bufp->b_vp, scp);
#endif /* SGIMIPS */
    icl_Trace2(cm_iclSetp, CM_TRACE_USTRAT, ICL_TYPE_POINTER, bufp->b_vp,
	       ICL_TYPE_LONG, bufp->b_flags);
    if ((bufp->b_flags & B_READ) == B_READ) {
	/*
	 * read b_bcount bytes into kernel address b_un.b_addr starting
	 * at byte dbtob(b_blkno).  Bzero anything we can't read,
	 * and finally call iodone(bufp).  File is in bufp->b_vp.  Credentials
	 * are from u area??
	 */
	osi_InitUIO(&tuio);
	tuio.osi_uio_iov = tiovec;
	tuio.osi_uio_iovcnt = 1;
#ifdef SGIMIPS
	tuio.osi_uio_offset = BBTOOFF(bufp->b_blkno);
#else  /* SGIMIPS */
	tuio.osi_uio_offset = dbtob(bufp->b_blkno);
#endif /* SGIMIPS */
	tuio.osi_uio_seg = OSI_UIOSYS;
	tuio.osi_uio_resid = bufp->b_bcount;
	tiovec[0].iov_base = bufp->b_un.b_addr;
	tiovec[0].iov_len = bufp->b_bcount;
	tuio.osi_uio_limit = osi_getufilelimit ();
	/*
	 * are user's credentials valid here?  probably, but this sure seems
	 * like the wrong thing to do.
	 */
#ifdef  AFS_AIX31_ENV

        code = cm_nlrw(bufp->b_vp, &tuio, UIO_READ, 0,
                       osi_getucred());
#else
#ifdef SGIMIPS
        code = cm_read(scp, &tuio, osi_getucred(), IO_ISLOCKED, 0, 0);
#else  /* SGIMIPS */
        code = cm_rdwr(bufp->b_vp, &tuio, UIO_READ, 0,
                       osi_getucred());
#endif /* SGIMIPS */
#endif  /* AFS_AIX31_ENV */
	if (code == 0) {
	    if (tuio.osi_uio_resid > 0)
		bzero(bufp->b_un.b_addr+bufp->b_bcount-tuio.osi_uio_resid,
		      tuio.osi_uio_resid);
	}
    } else {
	osi_InitUIO(&tuio);
	tuio.osi_uio_iov = tiovec;
	tuio.osi_uio_iovcnt = 1;
#ifdef SGIMIPS
	tuio.osi_uio_offset = BBTOOFF(bufp->b_blkno);
#else  /* SGIMIPS */
	tuio.osi_uio_offset = dbtob(bufp->b_blkno);
#endif /* SGIMIPS */
	tuio.osi_uio_seg = OSI_UIOSYS;
	/*
	 * Adjust length so as not to write beyond EOF.
	 * On HP/UX, this is done by the caller.
	 */
#ifdef AFS_AIX31_VM
	len = MIN(len, scp->m.Length - dbtob(bufp->b_blkno));
#endif
#ifdef SGIMIPS
        if ((offset = BBTOOFF(bufp->b_blkno)) < scp->m.Length) {
             diff = scp->m.Length - offset;     /* avoid sample skew */
             len = MIN(bufp->b_bcount, diff);
        }
        else
	{
	    /*
	     * Should never have a buffer beyond the eof.  I don't
	     * know how this happened, but get out.  Continuing with
	     * the operation can cause a bcopy with a negative
	     * length in cm_read().  Bug 633675.  If this is a write
	     * continuing will extend the length of the file, but
	     * that shouldn't happen here.
	     */
	    code = EFAULT;
	    debug("");
	    goto berr;
	}
#endif /* SGIMIPS */
	tuio.osi_uio_resid = len;
	tiovec[0].iov_base = bufp->b_un.b_addr;
	tiovec[0].iov_len = len;
	tuio.osi_uio_limit = osi_getufilelimit ();
	/*
	 * are user's credentials valid here?  probably, but this sure seems
	 * like the wrong thing to do.
	 */
#ifdef  AFS_AIX31_VM
        code = cm_nlrw(bufp->b_vp, &tuio, UIO_WRITE, 0,
                       osi_getucred());
#else
#ifdef SGIMIPS
        code = cm_strat_write (scp, (struct cm_dcache *)bufp->b_fsprivate,
                                &tuio, 0, osi_getucred());
        bufp->b_fsprivate = NULL;
#else  /* SGIMIPS */
        code = cm_rdwr(bufp->b_vp, &tuio, UIO_WRITE, 0,
                       osi_getucred());
#endif /* SGIMIPS */
#endif  /* AFS_AIX31_VM */
    }

berr:
    if (code || (bufp->b_flags & B_ERROR)) {
	bufp->b_error = code;
	bufp->b_flags |= B_ERROR;
#ifdef SGIMIPS
#ifdef DEBUG
	if (!berr_trace) {
	    int i;
	    berr_trace = &berr_log[BERR_LOGSZ-1];
	    for (i = 0; i < BERR_LOGSZ; i++) {
		berr_trace->berr_next = &berr_log[i];
		berr_trace = berr_trace->berr_next;
	    }
	}
	berr_trace = berr_trace->berr_next;
	berr_trace->berr_baddr = bufp;
	bcopy(bufp,&berr_trace->berr_buf,sizeof(buf_t));
#endif /* DEBUG */
	buftrace("DFS IOERRELSE", bufp);
        bufp->b_flags &= ~(B_READ|B_DELWRI);
        bufp->b_flags |= B_STALE;
	biodone(bufp);
	return code;
#endif

    }

#if !defined(AFS_AIX31_VM) && !defined(AFS_HPUX_ENV) && !defined(SGIMIPS)
    iodone(bufp);
    return 0;
#elif defined (SGIMIPS)
    biodone(bufp);
    return code;
#endif
}
#endif /* not AFS_SUNOS5_ENV */

/*
 * a freelist of one
 */
struct buf *cm_bread_freebp = 0;

/*
 *  Only rfs_read calls this, and it only looks at bp->b_un.b_addr.
 *  Thus we can use fake bufs (ie not from the real buffer pool).
 */
#ifdef SGIMIPS
int
cm_bread(
    struct vnode *vp,
    daddr_t lbn,
    struct buf **bufpp)
#else  /* SGIMIPS */
int
cm_bread(vp, lbn, bufpp)
    struct vnode *vp;
    daddr_t lbn;
    struct buf **bufpp;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    afs_hyper_t offset;
    int fsbsize, error;
#else  /* SGIMIPS */
    int offset, fsbsize, error;
#endif /* SGIMIPS */
    struct buf *bp;
    struct iovec iov;
    struct uio uio;
#ifdef SGIMIPS
    struct cm_scache *scp;
#endif /* SGIMIPS */

#ifdef AFS_OSF_ENV
    fsbsize = vp->v_mount->m_stat.f_bsize;
#else
    fsbsize = vp->v_vfsp->vfs_bsize;
#endif /* AFS_OSF_ENV */
    offset = lbn * fsbsize;
    if (cm_bread_freebp) {
	bp = cm_bread_freebp;
	cm_bread_freebp = 0;
    } else {
	bp = (struct buf *) osi_Alloc(sizeof(*bp));
	bp->b_un.b_addr = (caddr_t) osi_Alloc(fsbsize);
    }

    iov.iov_base = bp->b_un.b_addr;
    iov.iov_len = fsbsize;
    osi_InitUIO(&uio);
    uio.osi_uio_iov = &iov;
    uio.osi_uio_iovcnt = 1;
    uio.osi_uio_seg = OSI_UIOSYS;
    uio.osi_uio_offset = offset;
    uio.osi_uio_resid = fsbsize;
#ifdef AFS_OSF_ENV
    uio.uio_rw = UIO_READ;
#endif /* AFS_OSF_ENV */
    *bufpp = 0;
#ifdef SGIMIPS
    VTOSC(vp, scp);
    if (error = cm_read(scp, &uio, osi_getucred(), lbn, bufpp, 0)) {
#else  /* SGIMIPS */
    if (error = cm_read(VTOSC(vp),&uio,osi_getucred(),lbn, bufpp, 0)) {
#endif /* SGIMIPS */
	cm_bread_freebp = bp;
	return error;
    }
    if (*bufpp) {
	cm_bread_freebp = bp;
    } else {
	*(struct buf **)&bp->b_vp = bp;		/* mark as fake */
	*bufpp = bp;
    }
    return 0;
}


/*
 * Brelse call
 */
#ifdef SGIMIPS
int
cm_brelse(
    struct vnode *vp,
    struct buf *bufp)
#else  /* SGIMIPS */
int
cm_brelse(vp, bufp)
    struct vnode *vp;
    struct buf *bufp;
#endif /* SGIMIPS */
{
#ifdef AFS_SUNOS5_ENV
    panic("cm_brelse");
#else /* AFS_SUNOS5_ENV */
    if ((struct buf *)bufp->b_vp != bufp) {	/* not fake */
#if defined(AFS_AIX_ENV) || defined(AFS_OSF_ENV) || defined(SGIMIPS)
	bufp->b_flags |= B_AGE;
	bufp->b_resid = 0;
	brelse(bufp);
#else
        ufs_brelse(bufp->b_vp, bufp);
#endif /* AFS_AIX_ENV || AFS_OSF_ENV || SGIMIPS */
    } else if (cm_bread_freebp) {
#ifdef AFS_OSF_ENV
        osi_Free(bufp->b_un.b_addr, vp->v_mount->m_stat.f_bsize);
#else
        osi_Free(bufp->b_un.b_addr, vp->v_vfsp->vfs_bsize);
#endif /* AFS_OSF_ENV */
	osi_Free(bufp, sizeof(*bufp));
    } else {
	cm_bread_freebp = bufp;
    }
#endif /* AFS_SUNOS5_ENV */
    return 0;
}


/*
 * XXX the cm_lockctl function is defined in cm_lockf.c XXX
 */


/*
 * Copy out AFS 4 fid representation to caller
 *
 * No express path code here, we really want to have NFS requests, for
 * instance,still come to the cache manager, even if the CM will process the
 * request local right now. Eventually, for example, the fileset could move,
 * and if we had connected someone directly to UFS by returning a UFS file ID,
 * that client wouldn't track the moving fileset.
 */
#ifdef SGIMIPS
int
cm_afsfid(
    struct vnode *vp,
    afsFid *afsfidp,
    int flag)
#else  /* SGIMIPS */
int
cm_afsfid(vp, afsfidp, flag)
    struct vnode *vp;
    afsFid *afsfidp;
    int flag;
#endif /* SGIMIPS */
{
    struct cm_scache *scp;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    *afsfidp = scp->fid;
    /*
     * Make our volumes look different from exported ufs, otherwise we
     * conflict with the UFS disk backing our stuff.
     */
    AFS_hset64(afsfidp->Volume, AFS_hgethi(afsfidp->Volume) | 0x80000000,
	       AFS_hgetlo(afsfidp->Volume));
    return 0;
}


/*
 * Call xvfs_volume.c to get a dummy volume for us to use
 *
 * Again, no express path here, since we don't want to get the CM completely
 * out of the picture, we just want to execute calls quickly.
 */
/* ARGSUSED */
#ifdef SGIMIPS
int
cm_getvolume(
    struct vnode *realvp,
    struct volume **volpp)
#else  /* SGIMIPS */
int
cm_getvolume(realvp, volpp)
    struct vnode *realvp;
    struct volume **volpp;
#endif /* SGIMIPS */
{
    /*
     * Let's hope that cm would not call tkc anymore !
     */
    /* Return as if the volume is simply not exported; disable the glue. */
    *volpp = NULL;
    return 0;
}


/*
 * Nothing fancy here; just compare if vnodes are identical ones
 */
#ifdef SGIMIPS
int
cm_cmp(
    struct vnode *vp1,
    struct vnode *vp2)
#else  /* SGIMIPS */
int
cm_cmp(vp1, vp2)
    struct vnode *vp1, *vp2;
#endif /* SGIMIPS */
{
    return(vp1 == vp2);
}

#ifdef AFS_AIX_ENV
int
cm_hold(struct vnode *vp)
{
    vp->v_count++;
    return 0;
}

int
cm_rele(struct vnode *vp)
{
    osi_assert(vp->v_count != 0);
    --vp->v_count;
    if (vp->v_count == 0)
	return cm_inactive(vp, osi_getucred());
    else
	return 0;
}
#endif /* AFS_AIX_ENV */

int
cm_lock(struct vnode *vp)
{
    struct cm_scache *scp;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    lock_ObtainWrite(&scp->llock);
    return 0;
}

int
cm_unlock(struct vnode *vp)
{
    struct cm_scache *scp;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    lock_ReleaseWrite(&scp->llock);
    return 0;
}


/*
 * These next two aren't supposed to obtain locks first; assume locks held
 */
#ifdef SGIMIPS
int
cm_getlength(
    struct vnode *vp,
    afs_hyper_t *lenp,
    osi_cred_t *cred)
#else  /* SGIMIPS */
int
cm_getlength(vp, lenp, cred)
    struct vnode *vp;
    long *lenp;
    osi_cred_t *cred;
#endif /* SGIMIPS */
{
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    long code;
    struct cm_scache *scp;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_getlength(vp1, lenp, cred);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    *lenp = scp->m.Length;
    return 0;
}


#if !defined(AFS_SUNOS5_ENV) && !defined(SGIMIPS)
/* SUNOS5 version is in SUNOS5 subdir */
int
cm_map(vp, addr, length, offset, flags)
    struct vnode *vp;
    caddr_t addr;
    u_int   length, offset, flags;
{
    return ENOSYS;
}
#endif /* !AFS_SUNOS5_ENV && !SGIMIPS */

#ifdef SGIMIPS
int
cm_map(
    struct vnode *vp,
    u_int flags)
{
    struct cm_scache *scp;

    VTOSC(vp, scp);
   
    lock_ObtainWrite(&scp->llock);

    /* 
     * BDR
     * We are memory mapping a file here.
     * Bump the opens count so we don't give up the open XXX token
     * until this vnode goes inactive.  We can't mess with the readers
     * and writers count because we don't get notified when the file is
     * unmapped.  If we did up the readers and writers count here we
     * could lock out two procs that have memory mapped shared the same file.
     * Instead we will just let the system take care of sorting out who gets
     * to update the mapped file.  We still have the same age old problem
     * of not quite getting the proper tokens for memory mapped files
     * when it comes time to flush out our pages to disk.  *sigh*
     */
    scp->opens++;

    scp->states |= SC_MEMORY_MAPPED;

    lock_ReleaseWrite(&scp->llock);

    return(0);
}

    
int
cm_unmap(
    struct vnode *vp,
    int flag)
#else  /* SGIMIPS */
int
cm_unmap(vp, flag)
    struct vnode *vp;
    int flag;
#endif /* SGIMIPS */
{
    return ENOSYS;
}

#ifdef SGIMIPS
int     
cm_addmap (
	bhv_desc_t *bdp, 
	vaddmap_t map_type, 
	struct __vhandl_s *vt,
	pgno_t *pgno_type, 
	off_t offset, 
	size_t len, 
	mprot_t prot,
	struct cred *credp)
{
    return 0;
}

int 
cm_delmap (
	bhv_desc_t *bdp,
	struct __vhandl_s *vt,
	size_t len,
	struct cred *credp)
{
    return 0;
}
#endif /* SGIMIPS */


#ifdef SGIMIPS
int
cm_noop(void)
#else  /* SGIMIPS */
int
cm_noop()
#endif /* SGIMIPS */
{
    return 0;
}

#ifdef SGIMIPS
int
cm_invalop()
#else  /* SGIMIPS */
int
cm_invalop()
#endif /* SGIMIPS */
{
    icl_Trace0(cm_iclSetp, CM_TRACE_NOOP);
    return EINVAL;
}

#ifdef SGIMIPS
void cm_void_invalop(void)
#else  /* SGIMIPS */
void cm_void_invalop()
#endif /* SGIMIPS */
{
    icl_Trace0(cm_iclSetp, CM_TRACE_NOOP);
}

#ifdef SGIMIPS
void cm_badop(void)
#else  /* SGIMIPS */
void cm_badop()
#endif /* SGIMIPS */
{
    panic("cm_badop");
}


/*
 * Copy network attributes (i.e. afsFetchStatus) astat block into scache info;
 * must be called under a write lock.
 *
 * What information is sent to the server when a call is made?  All
 * information that might have been modified locally is sent back to the server
 * at any store (the high-level lock prevents races between stores and threads
 * performing new local modifications to the vnode)
 *
 * The time in the tokenp structure is an absolute time, converted by the
 * caller (since only the caller knows when the call really started running).
 */
#ifdef SGIMIPS
void cm_MergeStatus(
  struct cm_scache *scp,
  afsFetchStatus *fetchStatusp,
  afs_token_t *tokenp,
  int aflags,
  struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
void cm_MergeStatus(scp, fetchStatusp, tokenp, aflags, rreqp)
  struct cm_scache *scp;
  afsFetchStatus *fetchStatusp; 
  afs_token_t *tokenp;
  int aflags;
  struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    long mf;
    struct cm_server *serverp;

    icl_Trace3(cm_iclSetp, CM_TRACE_MERGESTAT, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_POINTER, tokenp, ICL_TYPE_LONG, aflags);

    /* add the token, if provided */
    if (tokenp) {
	serverp = cm_GetReqServer(rreqp);
	osi_assert(serverp);
	if (!AFS_hiszero(tokenp->tokenID) && !AFS_hiszero(tokenp->type))
	    cm_AddToken(scp, tokenp, serverp, 0);
    }

    /* if we were storing status in this call, we should clear the ModFlags
     * bits, since we've stored them already.  Must do the clear scan
     * before executing code below that watches for CM_MOD* flags.
     * CM_MERGE_STORING implies that we've already sent all dirty status
     * info to the server, and have proper info in the response.
     */
    if (aflags & CM_MERGE_STORING) {
	cm_ClearScan(scp);
    }

    /* File mode or ownership was changed, so invalidate the cached
     * ACL information.  Do this before merging in updated ACL cache
     * info, since the updated info is correct.
     */
    if (aflags & CM_MERGE_CHACL) {
	cm_FreeAllACLEnts(scp);
    }

    /*
     * if writing the file, these values were stored by the call's start.
     */
    mf = scp->m.ModFlags;
    /* if we've never filled in m.ChangeTime or if m.ChangeTime is earlier than
     * our response, fill it in from the net.  Otherwise, we've already got
     * a more recent stat response, and shouldn't overwrite it with old junk.
     * Note that if fileType is invalid, it means that the status info
     * wasn't returned to us at all.
     *
     * Unfortunately the above will cause some of our callers (GetAccessBits()
     * in particular) to get into an infinite loop if the ChangeTime
     * does get set on the server to be less than the ChangeTime that we have
     * cached. This can happen due to a restore -overwrite, or due to a time
     * reset in the server. To protect ourselves from this case we keep a
     * state variable in cm_volume (onlyOneOutstandingRPC) that tells us if
     * any other RPCs overlapped with the one that brought us here.
     * This state variable is only set to 1 (no concurrent RPCs) if at the time
     * of the call to cm_conn() we changed the outstandingRPC counter in the
     * cm_volume structure from 0 to 1 (i.e. there were no outstanding RPCs for
     * this volume at the time) and gets reset to 0 by cm_conn() if the
     * outstanding RPC counter was bumped from anything other than 0. So if
     * some other RPC was outstanding when we would have set this state to
     * 0 and it cannot be reset to 1 until we complete, and if some other RPC
     * started while we were outstanding it would have set the state to 0 and
     * it won't be reset to 1 until we both complete (and another RPC starts)
     *
     * The above guarantees that if we find onlyOneOutstandingRPC set to 1
     * changeTime discrepancies are not due to concurrent RPCs so we can
     * safely ignore them.
     */
    if (fetchStatusp->fileType != Invalid &&
	(!(scp->states & SC_STATD) || scp->volp->onlyOneOutstandingRPC ||
	 cm_tcmp(scp->m.ServerChangeTime, fetchStatusp->changeTime) <= 0)) {
	icl_Trace1(cm_iclSetp, CM_TRACE_MERGESTATOK, ICL_TYPE_LONG, mf);
	if (!(mf & CM_MODLENGTH)) {
	    afs_hyper_t oldlen;

	    /* merge in the length.  If the length is too long to represent we
             * need vnodeops to return EOVERFLOW.  We set a new bits in the
             * scp->states and check this in a few strategic places. */

#ifdef SGIMIPS
	    oldlen = scp->m.Length;
#else  /* SGIMIPS */
	    AFS_hset64(oldlen, 0, scp->m.Length);
#endif /* SGIMIPS */
	    if (AFS_hcmp(fetchStatusp->length, osi_maxFileClient) > 0) {
		scp->states |= SC_LENINVAL;
		scp->m.Length = 0;	/* safer than 2^31-1 */
		icl_Trace1(cm_iclSetp, CM_TRACE_MERGESTATLONGLEN,
			   ICL_TYPE_HYPER, &fetchStatusp->length);
	    } else {
		scp->states &= ~SC_LENINVAL;
#ifdef SGIMIPS
		/*
		 * If the file length is trucated, we really need to
		 * toss pages.  We cannot do that here without creating
		 * a deadlock between the scp->llock and a buffer
		 * semaphore when there is a cm_read() in progress.
		 */
		scp->m.Length = fetchStatusp->length;
#else  /* SGIMIPS */
		scp->m.Length = AFS_hgetlo(fetchStatusp->length);
#endif /* SGIMIPS */
	    }

	    if (AFS_hcmp(oldlen, fetchStatusp->length) > 0)
		icl_Trace3(cm_iclSetp, CM_TRACE_MERGESTATLENGTH,
			   ICL_TYPE_POINTER, scp,
			   ICL_TYPE_HYPER, &oldlen,
			   ICL_TYPE_HYPER, &fetchStatusp->length);
	}
	if (!(mf & CM_MODMOD))
	    scp->m.ModTime = fetchStatusp->modTime;
	if (!(mf & CM_MODOWNER))
	    scp->m.Owner = fetchStatusp->owner;
	if (!(mf & CM_MODMODE))
	    scp->m.Mode = fetchStatusp->mode;
	if (!(mf & CM_MODCHANGE))
	    scp->m.ChangeTime = fetchStatusp->changeTime;
	if (!(mf & CM_MODACCESS))
	    scp->m.AccessTime = fetchStatusp->accessTime;
	scp->m.serverAccessTime = fetchStatusp->accessTime.sec;
	scp->m.ServerChangeTime = fetchStatusp->changeTime;
	scp->m.BlocksUsed = fetchStatusp->blocksUsed;
	if (!(mf & CM_MODGROUP))
	    scp->m.Group = fetchStatusp->group;

	if (fetchStatusp->fileType == File) {
	    cm_vSetType(scp, VREG);
	    scp->m.Mode |= S_IFREG;
	} else if (fetchStatusp->fileType == Directory) {
	    cm_vSetType(scp, VDIR);
	    scp->m.Mode |= S_IFDIR;
	} else if (fetchStatusp->fileType == SymbolicLink) {
	    cm_vSetType(scp, VLNK);
	    scp->m.Mode |= S_IFLNK;
	}
	else if (fetchStatusp->fileType == MountPoint) {
	    cm_vSetType(scp, VLNK);	/* need *some* vnode type */
	    scp->m.Mode |= S_IFLNK;
	    scp->states |= SC_MOUNTPOINT;
	}
	/* XXXX add 30-32 to rpc incl file */
	else if (fetchStatusp->fileType == CharDev) {
	    cm_vSetType(scp, VCHR);
	    scp->m.Mode |= S_IFCHR;
	    scp->states |= SC_SPECDEV;
	    if (!osi_DecodeDeviceNumber(fetchStatusp, &scp->devno)) {
		scp->states |= SC_BADSPECDEV;
	    }
	}
	else if (fetchStatusp->fileType == BlockDev) {
	    cm_vSetType(scp, VBLK);
	    scp->m.Mode |= S_IFBLK;
	    scp->states |= SC_SPECDEV;
	    if (!osi_DecodeDeviceNumber(fetchStatusp, &scp->devno)) {
		scp->states |= SC_BADSPECDEV;
	    }
	}
	else if (fetchStatusp->fileType == FIFO) {
	    cm_vSetType(scp, VFIFO);
	    scp->m.Mode |= S_IFIFO;
	    scp->states |= SC_SPECDEV;
	    if (!osi_DecodeDeviceNumber(fetchStatusp, &scp->devno)) {
		scp->states |= SC_BADSPECDEV;
	    }
	}

	scp->m.DataVersion = fetchStatusp->dataVersion;
	scp->m.LinkCount = fetchStatusp->linkCount;
	scp->anyAccess = fetchStatusp->anonymousAccess;

	/*
	 * only call AddACLCache if ctime increasing, since otherwise
	 * we could merge in old info from an earlier, delayed FetchData,
	 * which has been cancelled by an already processed StoreStatus
	 * that changed the mode bits, for example.
	 */
	/*
	 * If we don't have TKN_STATUS_READ token at this point, then we really
	 * can not trust that ACL info from fetchStatus is still valid.  Why?
	 *
	 * cm_MergeStatus is called after some RPC.  Suppose that a token
	 * revocation occurs concurrently with the RPC, in such a way that
	 * the RPC finishes on the server before the server starts the
	 * revocation, but the revocation gets to the client first, i.e. the
	 * revocation is processed on the client before the return from the
	 * RPC.  The info that we're adding in is therefore old (predates the
	 * revocation), and shouldn't be used.
	 *
	 * Now, if the token was revoked as part of a server request
	 * creating file with ctime N, then the ctime of this request
	 * must be properly less than (<) N.  Furthermore, if we have a
	 * status/read token now, then our scp's ctime must be current, i.e.
	 * >= N.  So, if we have tokens and the ctime of the file is <=
	 * that of the request, we have information that wasn't invalidated
	 * by a revoke, and we can merge it in.
	 *
	 * If we made an update to the ACL cache ourselves, the proper
	 * invalidation is handled by the FreeAllACLEnts call near the
	 * start of this function.
         */
        if (cm_HaveTokens(scp, TKN_STATUS_READ)) {
	    icl_Trace2(cm_iclSetp, CM_TRACE_MERGESTATACL,
		       ICL_TYPE_LONG, rreqp->pag,
		       ICL_TYPE_LONG, fetchStatusp->callerAccess);
	    cm_AddACLCache(scp, rreqp, fetchStatusp->callerAccess);
	}	/* better access info */

	scp->states |= SC_STATD;

    }	/* newer change time on response */
}

/*
 * Update the status from the server (in FetchStatusp). If delnamep is set
 * then it's removed from the directory name hash; similarly if enternamep is
 * set it's added to the dir name hash. If setTokens is set then UPDATE tokens
 * are obtained first.
 *
 * Note that the llock shouldn't be set by the caller
 */
#ifdef SGIMIPS
int cm_UpdateStatus(
    struct cm_scache *scp,
    afsFetchStatus *FetchStatusp,
    char *delnamep,
    char *enternamep,
    afsFid *fidp,
    struct cm_rrequest *rreqp,
    long adirval)
#else  /* SGIMIPS */
int cm_UpdateStatus(scp, FetchStatusp, delnamep, enternamep, fidp, rreqp, adirval)
    struct cm_scache *scp;
    afsFetchStatus *FetchStatusp;
    char *delnamep, *enternamep;
    afsFid *fidp;
    struct cm_rrequest *rreqp;
    long adirval;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&scp->llock);
    if (fidp) {
	fidp->Cell = scp->fid.Cell;
	fidp->Volume = scp->fid.Volume;
    }
    /* check to see if info returned by this call is still good.  We
     * started with the data read token, and any revoke will bump
     * dirRevokes.  So, dirRevokes staying the same means that no revokes
     * happened since the call executed at the server.  Furthermore,
     * having the changeTime increase means that this information postdates
     * any returned by any concurrent calls issued by *this* cache manager.
     * Thus, we can overwrite existing info.
     *
     * If either of these conditions is false, then consider these four
     * cases: server revoke happened before call started, server
     * revoke occurred after server call, no revoke and our info
     * is latest, and no revoke and later info is available.
     * We're going to delete all info about both delnamep and enternamep.
     * In case 1, we had correct info, but don't use it.  Any info
     * remaining in the cache is still correct.
     * In case 2, our info is bad, since we raced with revoke.  Can't
     * merge our info in, since it is bad.  Already existing info was cleared
     * by revoke proc.
     * In case 3, we have the right info, but don't know it; the existing
     * info about these names is now incorrect.   No other names should
     * have been affected.
     * In case 4, we should discard our info.
     */
    icl_Trace1(cm_iclSetp, CM_TRACE_UPDATESTAT, ICL_TYPE_LONG,
	       adirval==scp->dirRevokes);
    if (adirval == scp->dirRevokes
	&& cm_tcmp(scp->m.ServerChangeTime, FetchStatusp->changeTime) <= 0) {
	if (delnamep)
	    nh_delete(scp, delnamep);
	if (enternamep)
	    nh_enter(scp, enternamep, (fidp ? fidp : &scp->fid), 0);
    }
    else {
	/* otherwise, our info may be older, or newer, than the best;
	 * we can't tell.  If newer, current info is bad, but if older
	 * our info is bad.  Just invalidate names that were affected
	 * by this call.
	 */
	if (delnamep)
	    nh_delete(scp, delnamep);
	if (enternamep)
	    nh_delete(scp, enternamep);
    }
    cm_MergeStatus(scp, FetchStatusp, (afs_token_t *) 0, 0, rreqp);
    /* invalidate directory listing: */
    cm_InvalidateAllSegments(scp, CM_INVAL_SEGS_MAKESTABLE);
    lock_ReleaseWrite(&scp->llock);
    return 0;
}

/*
 * The idea is that an RPC got an EEXIST code trying to create something that
 * the nh_dolookup routine already said wasn't there.  Thus, this most likely
 * reflects a race between multiple threads on the same client that are
 * basically all creating the same object.  Now, our thread might have been
 * the one that returned first, in which case we know that the nh_dolookup
 * cache was wrong when we checked it, and it might well still be wrong.
 * Alternatively, our thread might be returning after the thread that got
 * there first (and created the file) returned and already merged in the
 * correct entry.  Because the file server doesn't return any FetchStatus
 * information if it's returning a failure from the underlying filesystem
 * operation (VOPX_MKDIR, VOPX_CREATE, VOPX_LINK, VOPX_SYMLINK), we can't
 * know for sure how to synchronize this result with the successful result.
 * (We could do so if we got back the parent-directory FetchStatus block in
 * the EEXIST case, but we don't have that information, so we don't know.)
 * What we know, though, is that if the directory name cache still says that
 * this object doesn't exist, then it's wrong.  If that's the case, we flush
 * that entry from the name cache, and discard any directory listing at the same
 * time.
 *
 * The real problem is that the token-revocation mechanism protects only
 * inter-client accesses, and that because the directory name cache is
 * believed to be authoritative, we have to update that cache aggressively
 * when multiple clients are updating the same directory simultaneously.
 * Unfortunately, the strategy of updating the cache only when an RPC returns
 * successfully ignores the fact that the state of the server may have
 * changed before that RPC return, and that other threads on the same client
 * (sharing a CM) will meanwhile be treating the DNC as authoritative.  One
 * anomalous sequence of calls would be multiple threads doing the same
 * mkdir() call, and once EEXIST is returned from that call in a single
 * thread, proceeding to create a file in the new directory.  If the
 * AFS_MakeDir call that has actually created the directory hasn't returned
 * to the client yet, then the DNC will still be wrong, and the file-creation
 * attempt will fail with ENOENT based on the DNC contents.
 */
#ifdef SGIMIPS
void cm_FlushExists(
    struct cm_scache *scp,
    char *namep)
#else  /* SGIMIPS */
void cm_FlushExists(scp, namep)
    struct cm_scache *scp;
    char *namep;
#endif /* SGIMIPS */
{
    int code;
    afsFid tfid;

    icl_Trace2(cm_iclSetp, CM_TRACE_FLUSHEXISTS,
	       ICL_TYPE_STRING, (namep?namep:"<none>"),
	       ICL_TYPE_POINTER, scp);
    lock_ObtainWrite(&scp->llock);
    code = nh_lookup(scp, namep, &tfid);
    icl_Trace2(cm_iclSetp, CM_TRACE_FLUSHEXISTS_1,
	       ICL_TYPE_LONG, code,
	       ICL_TYPE_LONG, tfid.Vnode);
    if (code == 0 && tfid.Vnode == 0) {
    /* There's an ENOENT cached here!  Fix it. */
	icl_Trace2(cm_iclSetp, CM_TRACE_FLUSHEXISTS_2,
		   ICL_TYPE_STRING, namep,
		   ICL_TYPE_POINTER, scp);
	nh_delete(scp, namep);
	/* invalidate directory listing: */
	cm_InvalidateAllSegments(scp, CM_INVAL_SEGS_MAKESTABLE);
    }
    lock_ReleaseWrite(&scp->llock);
}


/*
 * Copy out attributes from cache entry
 */
#ifdef SGIMIPS
static void cmattr_to_vattr(
    struct cm_scache *scp,
    struct vattr *attrsp,
    int flag)				/* indicates extended attributes */
#else  /* SGIMIPS */
static void cmattr_to_vattr(scp, attrsp, flag)
    struct cm_scache *scp;
    struct vattr *attrsp;
    int flag;				/* indicates extended attributes */
#endif /* SGIMIPS */
{
    struct cm_volume *volp;
    struct cm_vdirent *tvdp;

    attrsp->va_type = cm_vType(scp);
    attrsp->va_mode = scp->m.Mode;
    volp = scp->volp;
    if (scp->m.Mode & (VSUID|VSGID)) {
	/*
	 * setuid or setgid, make sure we can run them from this volume.
	 */
	if (!(volp->states & VL_SGIDOK))
	    attrsp->va_mode &= ~(VSUID|VSGID);
    }
    attrsp->va_uid = scp->m.Owner;
    attrsp->va_gid = scp->m.Group;
#ifdef SGIMIPS
    attrsp->va_fsid = SCTOV(scp)->v_vfsp->vfs_dev;
#else  /* SGIMIPS */
#ifdef AFS_SUNOS5_ENV
    attrsp->va_fsid = scp->v.v_vfsp->vfs_dev;
#else
    /* otherwise, we should set the fsid to a unique number so that
     * generic code knows that this is a different file system.
     *
     * The right thing to do here is to allocate a unique device ID
     * from the kernel, but I don't know how to do that in general.
     * Probably need to add an osi function here.
     */
    attrsp->va_fsid = 1;
#endif /* AFS_SUNOS5_ENV */
#endif /* SGIMIPS */
    if (scp->states & SC_VOLROOT) {
	/*
	 * The mount point's vnode.
	 */
	if (tvdp = scp->parentVDIR) {
	    /* match our virtual mount point's inode # */
	    attrsp->va_nodeid = tvdp->inode;
	}
        else {
	    attrsp->va_nodeid = volp->mtpoint.Vnode +
		(AFS_hgetlo(volp->mtpoint.Volume)<<16);
	}
    }
    else attrsp->va_nodeid =
	scp->fid.Vnode + (AFS_hgetlo(scp->fid.Volume) << 16);
    attrsp->va_nlink = scp->m.LinkCount;
    attrsp->va_size = scp->m.Length;
#ifdef SGIMIPS
    attrsp->va_blksize = DFS_BLKSIZE;
#else  /* SGIMIPS */
    attrsp->va_blocksize = 8192;
#endif /* SGIMIPS */
    osi_SubFromUTime(attrsp->va_atime, scp->m.AccessTime);
    osi_SubFromUTime(attrsp->va_ctime, scp->m.ChangeTime);
    osi_SubFromUTime(attrsp->va_mtime, scp->m.ModTime);
    if (scp->states & SC_SPECDEV) {
	attrsp->va_rdev = scp->devno;
    }
    else
	attrsp->va_rdev = 1;
#ifdef AFS_OSF_ENV
    attrsp->va_bytes = (scp->m.BlocksUsed) << 10;
#else
#ifdef SGIMIPS
    attrsp->va_nblocks = scp->m.BlocksUsed;
#else  /* SGIMIPS */
    attrsp->va_blocks = scp->m.BlocksUsed;
#endif /* SGIMIPS */
#endif /* AFS_OSF_ENV */
}


/*
 *
 *  "@sys"-related routines.
 *
 */

static int ENameOK(char *namep)
{
    int tlen;

    tlen = strlen(namep);
    if (tlen >= 4 && strcmp(namep+tlen-4, "@sys") == 0)
	return 0;
    /* used to check for 0x80 bit on, but that's a potentially
     * internationalized name, so those are OK now.
     */
    return 1;
}


/* ARGSUSED */
#ifdef SGIMIPS
static char * getsysname(
    struct cm_rrequest *rreqp,
    struct cm_scache *scp,
    char *str,
    int strsize)
#else  /* SGIMIPS */
static char * getsysname(rreqp, scp, str, strsize)
    struct cm_rrequest *rreqp;
    struct cm_scache *scp;
    char *str;
    int strsize;
#endif /* SGIMIPS */
{

#ifdef AFS_SUNOS5_ENV
    /* Get @sys from DFS/NFS gateway if it's relevant */
    if (osi_nfs_gateway_loaded == 0) 
      goto not_gateway;
    if (!osi_ProcIsNFS())
	goto not_gateway;
    if (rreqp->pag == -2)
	goto not_gateway;
    if (at_atname_ptr == NULL) {
      at_atname_ptr = (int (*)()) modlookup ("dfsgw", "at_atname");
      if (at_atname_ptr == NULL)
	goto not_gateway;
    }
    if ((*at_atname_ptr)(ATSYS, rreqp->pag, str, strsize) == 0)
      goto not_gateway;
    return str;
 not_gateway:
#endif /* AFS_SUNOS5_ENV */

#ifdef SGIMIPS
    extern char cm_sgi_sysname[];
    return (cm_sysname == 0? cm_sgi_sysname : cm_sysname);
#else  /* SGIMIPS */

    /* vanilla AFS user */
    return (cm_sysname == 0 ? SYS_NAME : cm_sysname);
#endif /* SGIMIPS */
}


/* ARGSUSED */
#ifdef SGIMIPS
static char *getmachname(
    struct cm_rrequest *rreqp,
    struct cm_scache *scp,
    char *str,
    int strsize)
#else  /* SGIMIPS */
static char *getmachname(rreqp, scp, str, strsize)
    struct cm_rrequest *rreqp;
    struct cm_scache *scp;
    char *str;
    int strsize;
#endif /* SGIMIPS */
{

#ifdef AFS_SUNOS5_ENV
    /* Get @host from DFS/NFS gateway if it's relevant */
    if (osi_nfs_gateway_loaded == 0) 
	goto not_gateway;
    if (!osi_ProcIsNFS())
	goto not_gateway;
    if (rreqp->pag == -2)
	goto not_gateway;
    if (at_atname_ptr == NULL) {
      at_atname_ptr = (int (*)()) modlookup ("dfsgw", "at_atname");
      if (at_atname_ptr == NULL)
	goto not_gateway;
    }
    if ((*at_atname_ptr)(ATHOST, rreqp->pag, str, strsize) == 0)
	goto not_gateway;
    return str;

not_gateway:

#endif /* AFS_SUNOS5_ENV */

    /* vanilla AFS user */
    return (osi_GetMachineName (str, OSI_MAXHOSTNAMELEN) == 0 ? str : NULL);
}


#ifdef SGIMIPS
static void HandleAtName(
    char *namep,
    char *resultp,
    struct cm_rrequest *rreqp,
    struct cm_scache *scp)
#else  /* SGIMIPS */
static void HandleAtName(namep, resultp, rreqp, scp)
    char *namep;
    char *resultp;
    struct cm_rrequest *rreqp;
    struct cm_scache *scp;
#endif /* SGIMIPS */
{
    int tlen;
    char hoststr[OSI_MAXHOSTNAMELEN];
    char sysstr[CM_MAXSYSNAME];
    char *strp;
    
    tlen = strlen(namep);
    if (tlen >= 4 && strcmp(namep+tlen-4, "@sys") == 0) {
	strp = getsysname(rreqp, scp, sysstr, CM_MAXSYSNAME);
	if (strp != NULL) {
	    strncpy(resultp, namep, tlen-4);
	    strcpy(resultp+tlen-4, strp);
	    return;
	}
    } else if (tlen >= 5 && strcmp(namep+tlen-5, "@host") == 0) {
      strp = getmachname(rreqp, scp, hoststr, OSI_MAXHOSTNAMELEN);
      if (strp != NULL) {
	/* got the name */
	strncpy(resultp, namep, tlen-5);
	strcpy(resultp+tlen-5, strp);
	return;
      }
    }
    /* otherwise, just return the basic name */
    strcpy(resultp, namep);
}



/*
 * Get an ACL from the server.
 */
#ifdef SGIMIPS
int cm_getacl(
    struct vnode *vp,
    struct dfs_acl *aclp,
    int aclType,
    osi_cred_t *credp)
#else  /* SGIMIPS */
int cm_getacl(vp, aclp, aclType, credp)
struct vnode *vp;
struct dfs_acl *aclp;
int aclType;
osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code;
    long srvix;
    struct afsFetchStatus outStatus;
    struct cm_rrequest rreq;
    struct afsVolSync tsync;
    register struct cm_conn *connp;
    struct cm_volume *volp;
#ifdef EXPRESS_OPS
    struct volume *tvolp;
    struct vnode *vp1;
#endif /* EXPRESS_OPS */
    struct cm_scache *scp;
    u_long startTime;

#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    icl_Trace1(cm_iclSetp, CM_TRACE_GETACL, ICL_TYPE_POINTER, scp);
    if (scp->states & SC_VDIR)
	return EINVAL;

#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_getacl(vp1, aclp, aclType, credp);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */

    cm_InitReq(&rreq, credp);
    volp = (struct cm_volume *) 0;
    do {
	if (connp = cm_Conn(&scp->fid,  MAIN_SERVICE(SRT_FX),
			    &rreq, &volp, &srvix)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_GETACLSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    code = AFS_FetchACL(connp->connp, &scp->fid, aclType,
			    &osi_hZero, 0, (afsACL*)aclp, &outStatus, &tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(code);
	    icl_Trace1(cm_iclSetp, CM_TRACE_GETACLEND, ICL_TYPE_LONG, code);
	}
	else code = -1;
    } while (cm_Analyze(connp, code, &scp->fid, &rreq, volp, srvix, startTime));
    if (code == 0) {
	lock_ObtainWrite(&scp->llock);
	cm_MergeStatus(scp, &outStatus, (afs_token_t *) 0, 0, &rreq);
	lock_ReleaseWrite(&scp->llock);
    }
    if (volp)
        cm_EndVolumeRPC(volp);

    return cm_CheckError(code, &rreq);
}


/*
 * Store an ACL, either using user provided ACLs or copying them from file ccp
 */
#ifdef SGIMIPS
int cm_setacl(
  struct vnode * vp,
  struct dfs_acl *aclp,
  struct vnode * cvp,    /* copy ACL from this vnode */
  int dw,
  int sw,
  osi_cred_t *credp)
#else  /* SGIMIPS */
int cm_setacl(vp, aclp, cvp, dw, sw, credp)
  struct vnode * vp;
  struct dfs_acl *aclp;
  struct vnode * cvp;    /* copy ACL from this vnode */
  int dw, sw;
  osi_cred_t *credp;
#endif /* SGIMIPS */
{
    register long code;
    long srvix;
    afsFetchStatus outStatus;
    struct cm_rrequest rreq;
    struct afsVolSync tsync;
    struct dfs_acl *taclp = 0;	/* temporary if needed for RPC */
    register struct cm_conn *connp;
    struct cm_volume *volp;
#ifdef EXPRESS_OPS
    struct vnode *vp1;
    struct volume *tvolp;
#endif /* EXPRESS_OPS */
    struct afsFid aclFid;
    long aflags = 0;
    struct cm_scache *scp = 0;
    struct cm_scache *ccp = 0;
    u_long startTime;

#ifdef SGIMIPS
    if (vp) VTOSC(vp, scp);
    if (cvp) VTOSC(cvp, ccp);
#else  /* SGIMIPS */
    if (vp) scp = VTOSC(vp);
    if (cvp) ccp = VTOSC(cvp);
#endif /* SGIMIPS */
    icl_Trace1(cm_iclSetp, CM_TRACE_SETACL, ICL_TYPE_POINTER, scp);
    if (scp->states & SC_RO)
	return EROFS;
    if (scp->states & SC_VDIR)
	return EINVAL;
    cm_InitReq(&rreq, credp);

#ifdef EXPRESS_OPS
    if (cm_ExpressPath(scp, (struct cm_scache *) 0, &tvolp, &vp1,
		       (struct vnode **) 0)) {
	code = cmglue_setacl(vp1, aclp, cvp, dw, sw, credp);
	cm_ExpressRele(tvolp, vp1, (struct vnode *) 0);
	return code;
    }
#endif /* EXPRESS_OPS */
    if (aclp == NULL) {
	if (ccp) {
	    aflags |= AFS_ACLFLAG_COPY;	/* tell server what we're doing */
	    aclFid = ccp->fid;
	    /* What if ACL source and destination are on different filesets? */
	    if (!AFS_hsame(scp->fid.Volume, aclFid.Volume)
		|| !AFS_hsame(scp->fid.Cell, aclFid.Cell)) {
		return EXDEV;
	    }
	} else {
	    return EINVAL;
	}
    }
    volp = (struct cm_volume *) 0;

    if (code = cm_AccessError(scp, dacl_perm_control, &rreq)) {
	return cm_CheckError(code, &rreq);
    }
    code = cm_GetSLock(scp, TKN_UPDATESTAT, CM_SLOCK_WRDATAONLY,
		       WRITE_LOCK, &rreq);
    if (code) {
	/* if fails, returns sans lock */
	goto done;
    }
    scp->states |= SC_STORING;
    scp->states &= ~SC_STOREINVAL;
    scp->m.ChangeTime.sec = osi_Time();
    scp->m.ChangeTime.usec = 0;
    scp->m.ModFlags |= CM_MODCHANGE;
    lock_ReleaseWrite(&scp->llock);

    /* can't pass null pointer for ACL over RPC, so make a dummy
     * empty ACL if we weren't given one to start with.
     */
    if (!aclp) {
	taclp = (struct dfs_acl *) osi_AllocBufferSpace();
	taclp->dfs_acl_len = 0;
	aclp = taclp;
    }
    do {
	if (connp = cm_Conn(&scp->fid,  MAIN_SERVICE(SRT_FX),
			    &rreq, &volp, &srvix)) {
	    /* Check for TSR having trashed this scp */
	    code = 0;
	    lock_ObtainWrite(&scp->llock);
	    if ((scp->states & SC_STOREINVAL) || scp->asyncStatus) {
		scp->states &= ~SC_STOREINVAL;
		if (!code) code = scp->asyncStatus;
		if (!code) code = ESTALE;	/* anything but 0 */
		icl_Trace3(cm_iclSetp, CM_TRACE_STOREACLCONNINVALID,
			   ICL_TYPE_POINTER, scp,
			   ICL_TYPE_FID, &scp->fid,
			   ICL_TYPE_LONG, code);
	    }
	    lock_ReleaseWrite(&scp->llock);
	    if (!code) {
		icl_Trace0(cm_iclSetp, CM_TRACE_SETACLSTART);
		startTime = osi_Time();
		osi_RestorePreemption(0);
		code = AFS_StoreACL(connp->connp, &scp->fid, (afsACL*)aclp,
				    (aflags << 16)|((dw & 0xff) << 8)|(sw & 0xff),
				    &aclFid, &osi_hZero, 0,
				    &outStatus, &tsync);
		osi_PreemptionOff();
		code = osi_errDecode(code);
		icl_Trace1(cm_iclSetp, CM_TRACE_SETACLEND, ICL_TYPE_LONG, code);
	    }
	}
	else
	    code = -1;
    } while (cm_Analyze(connp, code, &scp->fid, &rreq, volp, srvix, startTime));

    /* free temp used for acl, if present */
    if (taclp)
	osi_FreeBufferSpace((struct osi_buffer *)taclp);

    lock_ObtainWrite(&scp->llock);
    if (code == 0) {
        cm_MergeStatus(scp, &outStatus, (afs_token_t *) 0, CM_MERGE_CHACL, &rreq);
    }
    scp->states &= ~SC_STORING;
    if ((scp->states & SC_STOREINVAL) || scp->asyncStatus) {
	scp->states &= ~SC_STOREINVAL;
	if (!code) code = scp->asyncStatus;
	if (!code) code = ESTALE;	/* anything but 0 */
	icl_Trace3(cm_iclSetp, CM_TRACE_STOREACLINVALID,
		   ICL_TYPE_POINTER, scp,
		   ICL_TYPE_FID, &scp->fid,
		   ICL_TYPE_LONG, code);
    }
    if (scp->states & SC_WAITING) {
	scp->states &= ~SC_WAITING;
	osi_Wakeup(&scp->states);
    }
    lock_ReleaseWrite(&scp->llock);
    if (volp)
        cm_EndVolumeRPC(volp);

  done:
    return cm_CheckError(code, &rreq);
}


/*
 * function to map a cm vnode and a derived local vnode into a CM
 * equivalent vnode (cm_scache) that corresponds to the local vnode.  Used by
 * cm_lookup, cm_create, etc, to get us to something we can return
 * to our caller.
 */
#ifdef SGIMIPS
static long cm_MapDown(
     register struct cm_scache *acmvp,
     register struct vnode *alocalvp,
     struct cm_scache **anewvpp)
#else  /* SGIMIPS */
static long cm_MapDown(acmvp, alocalvp, anewvpp)
     register struct cm_scache *acmvp;
     register struct vnode *alocalvp;
     struct cm_scache **anewvpp;
#endif /* SGIMIPS */
{
    struct afsFid tfid;		/* derived file ID */
    register struct cm_scache *tscp;
    register long code;

#ifndef SGIMIPS
    /*  osi_CantCVTVP() is noop in SGIMIPS */

    if (osi_CantCVTVP(alocalvp)) {
	/* just return it, it is as good as we can do */
        *anewvpp = VTOSC(alocalvp);
	cm_HoldSCache(*anewvpp);
	return 0;
    }
    code = VOPX_AFSFID(alocalvp, &tfid, /* don't need volume filled in */ 0);
    if (code)
        return code;
#else  /* SGIMIPS */
    /* cm_afsfid will always return 0 - so we can remove assignment and
     * the check below...
     */
    VOPX_AFSFID(alocalvp, &tfid, /* don't need volume filled in */ 0, code);
#endif /* SGIMIPS */
    tfid.Cell = acmvp->fid.Cell;
    tfid.Volume = acmvp->fid.Volume;
#if 0
    if (alocalvp->v_flag & VROOT) {
        /*
	 * tfid should have an appropriate (vnodeId, vnodeUnique)
	 */
    }
#endif /* 0 */
    /* now, tfid is the file ID we need; get a cm scache for it */
    lock_ObtainShared(&cm_scachelock);
    tscp = cm_FindSCache(&tfid);
    if (!tscp) {
	lock_UpgradeSToW(&cm_scachelock);
	tscp = cm_NewSCache(&tfid, acmvp->volp);
	lock_ReleaseWrite(&cm_scachelock);
	/* propagate type information down */
	lock_ObtainWrite(&tscp->llock);
        cm_vSetType(tscp, osi_vType(alocalvp));
	lock_ReleaseWrite(&tscp->llock);
    }
    else {
	lock_ReleaseShared(&cm_scachelock);
    }
    if (!tscp)
        return ENOENT;
    *anewvpp = tscp;
    return 0;
}

#ifndef AFS_SUNOS5_ENV
/*
  SUNOS5 version is in SUNOS5 subdir because the
  function's signature is different
*/
#ifdef SGIMIPS
int
cm_fid(struct vnode *vp, struct fid **fidpp)
#else
int
cm_fid(struct vnode *vp, struct fid *fidp)
#endif
{
#ifdef SGIMIPS
    struct cm_scache *scp;
    struct fid *fidp;
#else  /* SGIMIPS */
    struct cm_scache *scp = VTOSC(vp);
#endif /* SGIMIPS */
    struct cm_cell *cellp;
    long code;
#if	CM_MAXFIDSZ<24
    struct SmallFid Sfid;
#endif	/* CM_MAXFIDSZ<24 */

#ifdef SGIMIPS
    VTOSC(vp, scp);

    *fidpp = fidp = (struct fid *)kmem_alloc(
	    (sizeof(struct fid) - MAXFIDSZ + SIZEOF_SMALLFID), KM_SLEEP);
#endif /* SGIMIPS */
    /*
     * Each platform has a different maximum size for a fid,
     * usually smaller than a DFS Fid.
     */
#if	CM_MAXFIDSZ>=24

    fidp->fid_len = min(sizeof(afsFid), CM_MAXFIDSZ);
    bcopy((caddr_t)&scp->fid, (caddr_t)fidp->fid_data, fidp->fid_len);

#else	/* CM_MAXFIDSZ<24 */
#if	CM_MAXFIDSZ>=20

    Sfid.Cell = scp->fid.Cell;
    Sfid.Volume= AFS_hgetlo(scp->fid.Volume);
    Sfid.Vnode = scp->fid.Vnode;
    Sfid.Unique= scp->fid.Unique;

#else	/* CM_MAXFIDSZ<20 */

    if (cellp = cm_GetCell(&scp->fid.Cell))
        Sfid.CellVolAndVnodeHigh = (cellp->cellIndex & 0x000000ff) << 24;
    else
        Sfid.CellVolAndVnodeHigh = 0xff000000;

    Sfid.CellVolAndVnodeHigh |=
	(AFS_hgetlo(scp->fid.Volume) & 0x000fffff) << 4;
    Sfid.CellVolAndVnodeHigh |= (scp->fid.Vnode & 0x000f0000) >> 16;
    Sfid.Vnodelow = scp->fid.Vnode & 0x0000ffff;
    Sfid.Unique = scp->fid.Unique;

#endif	/* CM_MAXFIDSZ>=20 */

    fidp->fid_len = min(sizeof(Sfid), CM_MAXFIDSZ);
    bcopy((caddr_t)&Sfid, (caddr_t)fidp->fid_data, fidp->fid_len);
#endif	/* CM_MAXFIDSZ>=24 */
    return 0;
}
#endif


/*
 * AFS cm vnode-operations array
 */
static struct xvfs_xops cm_xops = {
    cm_open,
    cm_close,
    cm_rdwr,
    cm_ioctl,
#ifdef SGIMIPS
    (int (*)(struct vnode *, int, osi_cred_t *))cm_noop, /* cm_select */
#else  /* SGIMIPS */
    cm_noop, /* cm_select */
#endif /* SGIMIPS */
    cm_getattr,
    cm_setattr,
    cm_access,
    cm_lookup,
    cm_create,
    cm_remove,
    cm_link,
    cm_rename,
    cm_mkdir,
    cm_rmdir,
    cm_readdir,
    cm_symlink,
    cm_readlink,
    cm_fsync,
    cm_inactive,
    cm_bmap,
    cm_strategy,
    cm_ustrategy,
    cm_bread,
    cm_brelse,
    cm_lockctl,
    cm_fid,				/* VOPX_FID (old style) */
#ifdef AFS_AIX_ENV
    cm_hold,				/* write: do a vn hold */
    cm_rele,				/* write: do a vn_rele */
#else
    (int (*)(struct vnode *))cm_invalop,		/* cm_hold */
    (int (*)(struct vnode *))cm_invalop,		/* cm_rele */
#endif /* AFS_AIX_ENV */
    cm_setacl,
    cm_getacl,
    cm_afsfid,				/* new fid */
    cm_getvolume,
    cm_getlength,
    cm_map,
    cm_unmap,
#ifdef AFS_OSF_ENV
    cm_reclaim,				/* vnode reclaim */
#else
    (int (*)(struct vnode *))cm_invalop,
#endif
#ifdef AFS_SUNOS5_ENV
    cm_vmread,
    cm_vmwrite,
    cm_realvp,
    cm_rwlock,
    cm_rwunlock,
    cm_seek,
    cm_space,
    cm_getpage,
    cm_putpage,
    cm_addmap,
    cm_delmap,
    cm_pageio,
#else /* not AFS_SUNOS5_ENV */
#ifdef SGIMIPS
    (int (*)(struct vnode *, struct uio *, int, osi_cred_t *)) cm_invalop,
    (int (*)(struct vnode *, struct uio *, int, osi_cred_t *)) cm_invalop,
    (int (*)(struct vnode *, struct vnode **))cm_invalop,
    cm_rwlock,
    cm_rwunlock,
#else  /* SGIMIPS */
    cm_invalop,
    cm_invalop,
    cm_invalop,
    cm_void_invalop,			/* cm_rwlock */
    cm_void_invalop,			/* cm_rwunlock */
#endif /* SGIMIPS */
    cm_invalop,				/* cm_seek */
    cm_invalop,				/* cm_space */
    cm_invalop,				/* cm_getpage */
    cm_invalop,				/* cm_putpage */
#ifdef SGIMIPS
    cm_addmap,
    cm_delmap,
#else  /* SGIMIPS */
    cm_invalop,				/* cm_addmap */
    cm_invalop,				/* cm_delmap */
#endif /* SGIMIPS */
    cm_invalop,				/* cm_pageio */
#endif /* not AFS_SUNOS5_ENV */
#ifdef AFS_HPUX_ENV
    vfs_pagein,
    vfs_pageout,
#else
    cm_invalop,
    cm_invalop,
#endif /* AFS_HPUX_ENV */
#ifdef AFS_SUNOS5_ENV
    cm_setfl,
#else
#ifdef SGIMIPS
    (int (*)(struct vnode *, int, int, osi_cred_t *)) cm_invalop, /* cm_setfl */
#else  /* SGIMIPS */
    cm_invalop, *			/* cm_setfl */
#endif /* SGIMIPS */
#endif /* AFS_SUNOS5_ENV */
#ifdef AFS_SUNOS54_ENV
    fs_dispose,				/* cm_dispose */
    fs_nosys,				/* cm_setsecattr */
    cm_getsecattr,			/* cm_getsecattr */
#else
    cm_void_invalop,			/* cm_dispose */
    cm_invalop,				/* cm_setsecattr */
    cm_invalop				/* cm_getsecattr */
#endif /* AFS_SUNOS54_ENV */
};
struct xvfs_xops *cm_ops = &cm_xops;	/* Cache Manager generic ops */

#ifdef SGIMIPS

/*
 * The way things are configured IRIX wants a afs_vnodeops type when
 * we add "afs" as a filesystem type.  Dummy up things here so that
 * the kernel is fat and happy at link time.  We don't actually use these
 * for anything.  More kernel bloat.
 */

static int
cm_vnodeops_panic()
{
 	panic("CM VNODEOPS currently not supported\n");
	return 0;
}

struct vnodeops afs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
        (vop_open_t)cm_vnodeops_panic,
        (vop_close_t)cm_vnodeops_panic,
        (vop_read_t)cm_vnodeops_panic,
        (vop_write_t)cm_vnodeops_panic,
        (vop_ioctl_t)cm_vnodeops_panic,
        (vop_setfl_t)cm_vnodeops_panic,
        (vop_getattr_t)cm_vnodeops_panic,
        (vop_setattr_t)cm_vnodeops_panic,
        (vop_access_t)cm_vnodeops_panic,
        (vop_lookup_t)cm_vnodeops_panic,
        (vop_create_t)cm_vnodeops_panic,
        (vop_remove_t)cm_vnodeops_panic,
        (vop_link_t)cm_vnodeops_panic,
        (vop_rename_t)cm_vnodeops_panic,
        (vop_mkdir_t)cm_vnodeops_panic,
        (vop_rmdir_t)cm_vnodeops_panic,
        (vop_readdir_t)cm_vnodeops_panic,
        (vop_symlink_t)cm_vnodeops_panic,
        (vop_readlink_t)cm_vnodeops_panic,
        (vop_fsync_t)cm_vnodeops_panic,
        (vop_inactive_t)cm_vnodeops_panic,
        (vop_fid_t)cm_vnodeops_panic,
        (vop_fid2_t)cm_vnodeops_panic,
        (vop_rwlock_t)cm_vnodeops_panic,
        (vop_rwunlock_t)cm_vnodeops_panic,
        (vop_seek_t)cm_vnodeops_panic,
        (vop_cmp_t)cm_vnodeops_panic,
        (vop_frlock_t)cm_vnodeops_panic,
        (vop_realvp_t)cm_vnodeops_panic,
        (vop_bmap_t)cm_vnodeops_panic,
        (vop_strategy_t)cm_vnodeops_panic,
        (vop_map_t)cm_vnodeops_panic,
        (vop_addmap_t)cm_vnodeops_panic,
        (vop_delmap_t)cm_vnodeops_panic,
        (vop_poll_t)cm_vnodeops_panic,
        (vop_dump_t)cm_vnodeops_panic,
        (vop_pathconf_t)cm_vnodeops_panic,
        (vop_allocstore_t)cm_vnodeops_panic,
        (vop_fcntl_t)cm_vnodeops_panic,
        (vop_reclaim_t)cm_vnodeops_panic,
        (vop_attr_get_t)cm_vnodeops_panic,
        (vop_attr_set_t)cm_vnodeops_panic,
        (vop_attr_remove_t)cm_vnodeops_panic,
        (vop_attr_list_t)cm_vnodeops_panic,
        (vop_cover_t)cm_vnodeops_panic,
        (vop_link_removed_t)cm_vnodeops_panic,
        (vop_vnode_change_t)cm_vnodeops_panic,
        (vop_ptossvp_t)cm_vnodeops_panic,
        (vop_pflushinvalvp_t)cm_vnodeops_panic,
        (vop_pflushvp_t)cm_vnodeops_panic,
        (vop_pinvalfree_t)cm_vnodeops_panic,
        (vop_sethole_t)cm_vnodeops_panic,
        (vop_commit_t)cm_vnodeops_panic,
        (vop_readbuf_t)cm_vnodeops_panic,
        (vop_strgetmsg_t)cm_vnodeops_panic,
        (vop_strputmsg_t)cm_vnodeops_panic,
};
#endif /* SGIMIPS */
