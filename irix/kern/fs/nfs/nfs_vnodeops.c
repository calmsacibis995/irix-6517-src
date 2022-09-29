/* @(#)nfs_vnodeops.c	2.9 88/07/15 NFSSRC4.0 from 2.156 88/03/22 SMI
 *
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#ident	"$Revision: 1.322 $"

#include "types.h"
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/dirent.h>
#include <sys/dnlc.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/kfcntl.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/pfdat.h>
#include <sys/proc.h>
#include <sys/siginfo.h>	/* for CLD_EXITED exit code */
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/flock.h>
#include <fs/specfs/spec_lsnode.h>
#include <sys/capability.h>
#include <sys/mac_label.h>
#include <sys/acl.h>
#include <sys/runq.h>
#include <sys/ksignal.h>
#include <ksys/vproc.h>
#include <sys/sched.h>
#include "xdr.h"
#include "lockmgr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "nlm_debug.h"
#include "bds.h"
#ifdef REMOTE_IO
#include <sys/rio.h>
#endif
#include <sys/uuid.h>
#include <sys/fs/xfs_fsops.h>

#ifdef NSD
#include "nsd.h"
#endif

#define check_stale_fh(errno, bdp) \
	if ((errno) == ESTALE) { dnlc_purge_vp(BHV_TO_VNODE((bdp))); nfsattr_inval((bdp)); }

#ifdef DATAPIPE
/* data pipe function */
extern int fspe_get_ops(void *);
#endif

static int	nfs_rdwr(bhv_desc_t *, struct uio *, enum uio_rw, int,
			 struct cred *, flid_t *);
static int	rwvp(bhv_desc_t *, struct uio *, enum uio_rw,
			int, struct cred *, flid_t *);
static int	nfs_lookup(bhv_desc_t *, char *, struct vnode **,
			struct pathname *, int, struct vnode *, struct cred *);
static int	nfs_setattr(bhv_desc_t *, struct vattr *, int, struct cred *);
static int	flush_bio(bhv_desc_t *, int);
static int	sync_vp(bhv_desc_t *);
static void	riodone(struct rnode *);
static int	nfs_fsync_locked(bhv_desc_t *, int, struct cred *,
				 off_t, off_t);
static int	nfs_fsync(bhv_desc_t *, int, struct cred *, off_t, off_t);
static int	nfs_cleanlocks(bhv_desc_t *, pid_t, long, cred_t *);

#if _MIPS_SIM == _ABI64
int irix5_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5(void *, int, xlate_info_t *);
int irix5_n32_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5_n32(void *, int, xlate_info_t *);
#endif

#ifdef DEBUG_NOHANG
/*
 * These fake locking routines can be substituted for the real ones
 * when the real routines would fail because of debug_rlock().  This
 * can be used to test code paths where the lock must be acquired
 * twice, in which debug_rlock() would normally always cause the first
 * attempt to get the lock to fail.  See rlock_nohang().
 */
static int fake_nfs_rwlock_nohang(bhv_desc_t *bdp, vrwlock_t flags);
static int fake_rlock_nohang(rnode_t *);
#endif
extern int	async_bio;

extern int	nbuf;
extern volatile int	b_unstablecnt;

static int async_daemon_count;		/* number of existing biod's */

/*
 * These are the vnode ops routines which implement the vnode interface to
 * the networked file system.  These routines just take their parameters,
 * make them look networkish by putting the right info into interface structs,
 * and then calling the appropriate remote routine(s) to do the work.
 *
 * Note on directory name lookup caching:  we desire that all operations
 * on a given client machine come out the same with or without the cache.
 * To correctly do this, we serialize all operations on a given directory,
 * by using rlock and runlock around rfscalls to the server.  This way,
 * we cannot get into races with ourself that would cause invalid information
 * in the cache.  Other clients (or the server itself) can cause our
 * cached information to become invalid, the same as with data pages.
 * Also, if we do detect a stale fhandle, we purge the directory cache
 * relative to that vnode.  This way, the user won't get burned by the
 * cache repeatedly.
 */

/*ARGSUSED*/
static int
nfs_open(bhv_desc_t *bdp, vnode_t **vpp, mode_t flag, struct cred *cred)
{
	int error;
	struct rnode *rp = bhvtor(bdp);

	/*
	 * If we can't support BDS, return error if FDIRECT was set.
	 */
	if ((flag & FDIRECT) && !(rtomi(rp)->mi_flags & MIF_BDS))
		return EINVAL;

	/*
	 * validate cached data by getting the attributes from the server
	 */
	nfsattr_inval(bdp);
	error = nfs_getattr(bdp, 0, 0, cred);

	/*
	 * Get default credentials in case nfs_strategy is called
	 * from mapped file write.
	 */
	if (!error && (flag & FWRITE)) {
		if (rp->r_cred == NULL) {
			rp->r_cred = crgetcred();
		}
	}

	return (error);
}

/*ARGSUSED*/
static int
nfs_close(
	bhv_desc_t *bdp,
	int flag,
	lastclose_t lastclose,
	struct cred *crp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct rnode *rp;
	int error;

	ASSERT(vp->v_count > 0);

	if (lastclose == L_FALSE)
		return(0);

	rp = bhvtor(bdp);
	error = nfs_rwlock_nohang(bdp, VRWLOCK_WRITE);
	if (error) {
		return error;
	}
	
	if (rp->r_bds) {
		int bdserror = 0;
		
		nfs_rwlock(bdp, VRWLOCK_WRITE);
		if (rp->r_bds_flags & RBDS_DIRTY)
			bdserror = bds_sync(rp, crp);
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
		if (bdserror && rp->r_error == 0)
			rp->r_error = bdserror;
	}

	/*
	 * Flush all delayed and async buffers, including readaheads.
	 * NFS requires sync-on-close. Note that this could be last close
	 * but process(es) could still have the file mapped and still
	 * be writing to it via mmap. We shouldn't destroy these
	 * mmap guys - so we'll postpone last-close processing
	 * till inactive..
	 * If this file is on a private
	 * mount, defer flushing until vp is recycled.
	 */
	if (VN_MAPPED(vp)) {
		ASSERT(vp->v_count > 1);
		;
	} else {
		if (!(rtomi(rp)->mi_flags & MIF_PRIVATE)) {
			/*
			 * if RCLEANLOCKS is set, we need to do remote
			 * lock cleaning
			 */
			if (rp->r_flags & RCLEANLOCKS) {
				(void)nfs_cleanlocks(bdp, current_pid(),
					(long)0, crp);
			}
			error = flush_bio(bdp, 1);
			if (error) {
				nfs_rwunlock(bdp, VRWLOCK_WRITE);
				return error;
			}
			/*
			 * if the v_count is 1 we're the last reference so
			 * we an assert that everything is done
			 */
			ASSERT(rp->r_iocount == 0);
#ifdef never
			ASSERT(!VN_DIRTY(vp));
#endif /* never */
		}
		if (vp->v_type != VLNK && rp->r_unldrp != NULL || rp->r_error) {
			nfsattr_inval(bdp);
			VOP_INVALFREE_PAGES(vp, rp->r_localsize);
			/*
			 * Can't reset r_localsize here because VOP_INVALFREE_PAGES
			 * won't remove pages that have current references.
			 */
			dnlc_purge_vp(vp);
		}
	}
	nfs_rwunlock(bdp, VRWLOCK_WRITE);
	return (flag & FWRITE ? rp->r_error : 0);
}

static int
nfs_read(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *cred,
	struct flid *fl)
{
	int error;

	if (!(ioflag & IO_ISLOCKED)) {
		error = nfs_rwlock_nohang(bdp, VRWLOCK_READ);
		if (error) {
			return error;
		}
	}
    
	error = 0;
	if ((ioflag & IO_RSYNC) && (ioflag & (IO_SYNC | IO_DSYNC))) {
		error = sync_vp(bdp);
	}
	if (!error) {
		error = nfs_rdwr(bdp, uiop, UIO_READ, ioflag, cred, fl);
	}

	if (!(ioflag & IO_ISLOCKED))
		nfs_rwunlock(bdp, VRWLOCK_READ);

	return error;
}

static int
nfs_write(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *cred,
	struct flid *fl)
{
	int error;

	if (!(ioflag & IO_ISLOCKED)) {
		error = nfs_rwlock_nohang(bdp, VRWLOCK_WRITE);
		if (error) {
			return error;
		}
	}
    
	error = nfs_rdwr(bdp, uiop, UIO_WRITE, ioflag, cred, fl);

	if (!(ioflag & IO_ISLOCKED))
		nfs_rwunlock(bdp, VRWLOCK_WRITE);

	return error;
}

static int
nfs_rdwr(
	bhv_desc_t *bdp,
	struct uio *uiop,
	enum uio_rw rw,
	int ioflag,
	struct cred *cred,
	struct flid *fl)
{
	vnode_t *vp;
	struct rnode *rp;
	mntinfo_t *mi;
	int error = 0;

	vp = BHV_TO_VNODE(bdp);
	ASSERT(vp->v_count > 0);
	if (vp->v_type != VREG)
		return EISDIR;
	rp = bhvtor(bdp);
	mi = rtomi(rp);

	if (rw == UIO_WRITE || (rw == UIO_READ && rp->r_cred == NULL)) {
		/*
		 * Use rlock to keep another cpu from grabbing credentials
		 * which are about to be freed, in do_bio.
		 */
		error = rlock_nohang(rp);
		if (error) {
			return error;
		}
		if (rp->r_cred)
			crfree(rp->r_cred);
		rp->r_cred = cred;
		crhold(cred);
		runlock(rp);
	}
	if (ioflag & IO_APPEND) {
		error = nfs_getattr(bdp, 0, 0, cred);
		if (error)
			return error;
		uiop->uio_offset = rp->r_size;
	}


	/*
	 * To use bds, bds mount option must be set. Then, either
	 * FDIRECT needs to be set, or this needs to be "auto" bds.
	 * auto is used if:
	 * * FBULK is set, and access is larger than BDS_BULK_AUTO
	 * * or the bdsauto mount option is set, and we're larger than the
	 *   user specified minimum size
	 * * not mapped
	 * * old-style XFS DIRECT alignment restriction must be met
	 *   unless this is BDS 2.0 or better
	 */
	if (!(mi->mi_flags & MIF_BDS))
		goto nfs_rdwr_nobds;
	
	if ((ioflag & IO_DIRECT) ||			/* explicit BDS */
	    ((((ioflag & IO_BULK) && (BDS_BULK_AUTO <= uiop->uio_resid))
	      || (mi->mi_bdsauto && (mi->mi_bdsauto <= uiop->uio_resid)))
	     && !VN_MAPPED(vp)))			/* executables */
	{
		/*
		 * BDS 1.[01] does not allow unaligned accesses, so
		 * if the access is unaligned, do not allow an "auto"
		 * BDS access unless we've got BDS 2.0 or better
		 */
		if (!(ioflag & IO_DIRECT) &&
		    ((uiop->uio_resid & 4095) != 0 ||
		     (uiop->uio_offset & 4095) != 0) &&
		    (bds_vers(rp, 2) <= 0))
		{
			goto nfs_rdwr_nobds;
		}

		/*
		 * Don't do bdsauto if the request is too large for BDS.
		 */

		if (!(ioflag & IO_DIRECT) &&
		    (bds_vers(rp, 2) < 0 ||
		     uiop->uio_resid > mi->mi_bds_maxiosz))
		{
			goto nfs_rdwr_nobds;
		}

		/*
		 * Flush the page cache.
		 */
		if (rw == UIO_WRITE && VN_CACHED(vp)) {
			error = nfs_fsync_locked(bdp, FSYNC_INVAL, cred,
						 (off_t)0, (off_t) -1);
			if (error) {
				return error;
			}
		}

		/*
		 * No locking, V2 is single threaded on I/O.
		 */
		if (rw == UIO_READ)
			error = bds_read (rp, uiop, ioflag, cred);
		else {
			error = bds_write(rp, uiop, ioflag, cred);
			nfsattr_inval(bdp);
		}
		return (error);
	}
nfs_rdwr_nobds:
	/*
	 * if writes have gone through BDS, and BDS is dirty from writebehinds,
	 * but now we're using NFS, do a sync to ensure consistency.
	 */
	if (rw == UIO_WRITE && rp->r_bds && (rp->r_bds_flags & RBDS_DIRTY)) {
		if (error = bds_sync(rp, cred))
			return error;
	}

	return rwvp(bdp, uiop, rw, ioflag, cred, fl);
}

static int nfsread(bhv_desc_t *, caddr_t, u_int, int, int *, struct cred *, int);
static int nfswrite(bhv_desc_t *, caddr_t, u_int, int, struct cred *, int, buf_t *);

#define ROUNDUP(a, b)   ((((a) + (b) - 1) / (b)) * (b))

/* ARGSUSED */
static int
rwvp(bdp, uio, rw, ioflag, cred, fl)
	register bhv_desc_t *bdp;
	register struct uio *uio;
	enum uio_rw rw;
	int ioflag;
	struct cred *cred;
	struct flid *fl;
{
	vnode_t *vp;
	struct buf *bp;
	struct rnode *rp;
	daddr_t bn;
	int cnt, off;
	int size, len, error, diff, eof;
	struct bmapval bmv[2];
	int nmaps;
	off_t remainder;
	mntinfo_t *mi;
	ssize_t resid = uio->uio_resid;

	vp = BHV_TO_VNODE(bdp);
	if (uio->uio_resid == 0) {
		return (0);
	}
	if (uio->uio_offset < 0
	    || ((off_t)(uio->uio_offset + uio->uio_resid) < 0)
	    || ((uio->uio_offset + uio->uio_resid) > NFS_MAX_FILE_OFFSET)) {
		return (EINVAL);
	}

	/*
	 * Check to make sure that the process will not exceed
	 * its limit on file size.  It is okay to write up to
	 * the limit, but not beyond.  Thus, the write which
	 * reaches the limit will be short and the next write
	 * will return an error.
	 */
	remainder = 0;
	if (rw == UIO_WRITE && vp->v_type == VREG &&
		uio->uio_offset + uio->uio_resid > uio->uio_limit) {
		remainder = uio->uio_offset + uio->uio_resid - uio->uio_limit;
		uio->uio_resid = uio->uio_limit - uio->uio_offset;
		if (uio->uio_resid <= 0) {
			uio->uio_resid += remainder;
			return (EFBIG);
		}
	}

	/*
	 * Update the attributes if necessry before we do anything that might
	 * use them.
	 */
	error = nfs_getattr(bdp,0,0,cred);
	if (error)
		return(error);

	rp = bhvtor(bdp);
	mi = rtomi(rp);
	size = rtoblksz(rp);
	ASSERT(size > 0);
	len = BTOBBT(size);

	/*
	 * Check for locks if some exist and mandatory locking is enabled.
	 */
	if (vp->v_flag & VFRLOCKS) {
		if ((rw == UIO_WRITE) && (uio->uio_segflg != UIO_SYSSPACE)) {
			flock_t ld;

			/*
			 * Do direct writes when there are file locks
			 * in place on this vnode except when the write
			 * is UIO_SYSSPACE.
			 *
			 * We need to do this to cover the case where
			 * two processes hold locks on portions of the
			 * file which fall into the same block.  For
			 * example, one client may lock the first
			 * 128 bytes of a file and another client
			 * the second 128 bytes.  If the first client
			 * writes to its locked region, we do not want
			 * it overwriting the second region.  Doing
			 * direct I/O will ensure that only as much
			 * as the user specified (now in uio_resid)
			 * will be written, and it will be written
			 * at the offset specified by the user (now
			 * in uio_offset).
			 *
			 * We do not do direct I/O is uio_segflg is
			 * UIO_SYSSPACE because there is the possiblity
			 * that the write is being done in the process
			 * of growing a memory-mapped file.
			 *
			 * See if this process has a file lock which spans the
			 * blocks which will be modified by this write.  If so,
			 * we can go through the buffer cache to do the write,
			 * otherwise, we must set IO_DIRECT.
			 */
			ld.l_start = (uio->uio_offset / size) * size;
			ld.l_len = (((uio->uio_offset + uio->uio_resid + size - 1) / size)
				* size) - ld.l_start;
			ld.l_pid = curuthread->ut_flid.fl_pid;
			ld.l_whence = 0;
			ld.l_type = F_WRLCK;
			ld.l_sysid = 0;
			if (!haslock(vp, &ld)) {
				ioflag |= IO_DIRECT;
				VOP_FLUSHINVAL_PAGES(vp,
					(uio->uio_offset / size) * size,
					MIN((((uio->uio_offset + uio->uio_resid +
					       size - 1) / size) * size),
					    rp->r_localsize) - 1, FI_REMAPF_LOCKED);
			}
		}
	}
	/*
	 * If this is a loopback mount of a local file system, we must
	 * avoid doing the write through the buffer cache, thus making
	 * the buffer dirty and in need of flushing.  To accomplish
	 * this, we set the IO_SYNC flag.  This relies on the code below
	 * which calls nfswrite directly for IO_SYNC rather than using
	 * bwrite.
	 */
	if (mi->mi_flags & MIF_LOCALLOOP) {
		ioflag |= IO_SYNC;
	}

	error = 0;
	eof = 0;
	do {
		if (ioflag & IO_DIRECT) {
			/*
			 * since we're not going into the page cache
			 * we don't need to worry about offsetting
			 * buffer
			printf("nfs DIRECT vp 0x%x sz %d\n", vp,uio->uio_resid);
			 */
			ASSERT(!(vp->v_flag & VISSWAP) || (poff(uio->uio_offset) == 0));
			off = 0;
			bn = -1;
			cnt = MIN((unsigned)size, uio->uio_resid);
		} else {
			off = uio->uio_offset % size;
			bn = BTOBBT(uio->uio_offset - off);
			cnt = MIN((unsigned)(size - off), uio->uio_resid);
		}

		bmv[0].bn = bn;
		bmv[0].offset = bn;
		bmv[0].length = len;
		bmv[0].eof = 0;	/* conservative */
		bmv[0].pboff = off;
		bmv[0].bsize = size;
		bmv[0].pbsize = cnt;
		bmv[0].pbdev = vp->v_rdev;
		bmv[0].pmp = uio->uio_pmp;

		/*
		 * UIO_NOSPACE means someone is faulting in a page from an
		 * mmap'ed file and therefore it has to be in the page cache.
		 */
		    
		if (ioflag & IO_DIRECT) {
			/*
			 * file lock file - normal read/write
			 * or swapin in/out
			 */

			/*
			 * We do not currently support direct I/O from user
			 * programs.  The only time IO_DIRECT will be set
			 * when uio_segflg is UIO_USERSPACE is when the I/O
			 * is a write and a file lock is held on the file.
			 */
			ASSERT((uio->uio_segflg != UIO_USERSPACE) ||
				(rw == UIO_WRITE));

			/* XXXbe optimize IO_DIRECT UIO_SYSSPACE case */
			if (uio->uio_segflg != UIO_USERSPACE) {
				bp = ngeteblk(0);
				bp->b_un.b_addr = uio->uio_iov->iov_base;
				if (!(ioflag & IO_PFUSE_SAFE))
					bp->b_flags |= B_PRIVATE_B_ADDR;
			} else {
				bp = ngeteblk(size);
			}
			if (rw == UIO_READ) {
				error = rlock_nohang(rp);
				if (error) {
					bp->b_flags |= B_ERROR;
					bp->b_error = error;
					brelse(bp);
					break;
				}
				error = nfsread(bdp, bp->b_un.b_addr + off,
						uio->uio_offset, cnt,
						(int *)&bp->b_resid, cred, 0);
				if (error) {
					runlock(rp);
					bp->b_flags |= B_ERROR;
					bp->b_error = error;
					brelse(bp);
					break;
				}
				rp->r_lastr = bn;
				diff = rp->r_size - uio->uio_offset;
				runlock(rp);
				if (diff <= 0) {
					brelse(bp);
					break;
				}
				if (diff < cnt) {
					cnt = diff;
					eof = 1;
				}
			}

		} else if (rw == UIO_READ) {

			scoff_t maxlocalsize;

			/*
			 * read/paging in a normal file - no file locks
			 * paging in a file locked file
			 */
			if ((mi->mi_flags & MIF_PRIVATE)
			    && uio->uio_offset >= rp->r_size) {
				break;
			}

			if (rp->r_lastr + len == bn) {
				nmaps = 2;
				bmv[1].bn = bmv[1].offset = bn + len;
				bmv[1].length = len;
				bmv[1].eof = 0;
				bmv[1].pboff = 0;
				bmv[1].bsize = size;
				bmv[1].pbsize = size;
				bmv[1].pbdev = bmv[0].pbdev;
				bmv[1].pmp = uio->uio_pmp;
				maxlocalsize = BBTOB(bn) + 2 * size;
			} else {
				nmaps = 1;
				maxlocalsize = BBTOB(bn) + size;
			}
			rp->r_lastr = bn;

			diff = rp->r_size - uio->uio_offset;
			if (diff <= 0) 
				break;
			if (diff < cnt) {
				cnt = diff;
				eof = 1;
				bmv[0].pbsize = cnt;
			}

			if (rp->r_localsize < maxlocalsize)
				rp->r_localsize = maxlocalsize;

			bp = chunkread(vp, bmv, nmaps, cred);

		} else /* (rw == UIO_WRITE) */ {

			/*
			 * writing a normal file - no file locks
			 */
			if (rp->r_error) {
				error = rp->r_error;
				break;
			}

			if (rp->r_localsize < BBTOB(bn) + size)
				rp->r_localsize = BBTOB(bn) + size;

			if ((uio->uio_offset >= ROUNDUP(rp->r_size, size)) ||
				((off == 0) && ((cnt == size) ||
				((uio->uio_offset + cnt) >= rp->r_size))) ) {
				/*
				 * Skip the unnecessary read and bzero() on
				 * the portion of the file being overwritten.
				 * Must still bzero() the unwritten portions
				 * of new buffers.
				 * The cnt==size case requires no bzero().
				 */
				bp = getchunk(vp, bmv, cred);

				if (!(bp->b_flags & B_ERROR) && cnt != size) {
					int end = off + cnt;

					(void) bp_mapin(bp);
					ASSERT(off >= 0 && off < size);
					ASSERT(cnt > 0 && cnt <= size);
					ASSERT(end <= size);
					if (off != 0)
						bzero(bp->b_un.b_addr, off);
					if (end < size)
						bzero(bp->b_un.b_addr + end,
						    size - end);
				}
			} else {
				bp = chunkread(vp, bmv, 1, cred);
			}
		}

		if (bp->b_flags & B_ERROR) {
			error = bp->b_error;
			if (!error)
				error = EIO;
			brelse(bp);
			break;
		}

#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			error = biomove(bp,bmv[0].pboff,cnt,rw,uio);
			if (!error)
				(void) bp_mapin(bp);
		} else
#endif
		{
		(void) bp_mapin(bp);
		error = uiomove(bp->b_un.b_addr + bmv[0].pboff, cnt, rw, uio);
		}

		if (rw == UIO_READ) {
			brelse(bp);
		} else if (!error) {
			/*
			 * r_size is the maximum number of bytes known
			 * to be in the file.
			 * Make sure it is at least as high as the last
			 * byte we just wrote into the buffer.
			 */
			if (rp->r_size < uio->uio_offset) {
				rp->r_size = uio->uio_offset;
			}
			if (ioflag & IO_DIRECT) {
				nfsattr_inval(bdp);
				error = flush_bio(bdp, 1);
				if (!error) {
					VOP_INVALFREE_PAGES(vp,
							    rp->r_localsize);
					error = rlock_nohang(rp);
					if (!error) {
						error = nfswrite(bdp,
						     bp->b_un.b_addr + off,
						      uio->uio_offset - cnt,
						      cnt, cred, 0, bp);
						runlock(rp);
					}
				}
				brelse(bp);
			} else if (ioflag & (IO_SYNC|IO_DSYNC)) {
				error = rlock_nohang(rp);
				if (!error) {
					error = nfswrite(bdp,
					     bp->b_un.b_addr + off,
					      uio->uio_offset - cnt,
					      cnt, cred, 0, bp);
					runlock(rp);
				}
				if (error) {
					bp->b_flags |= B_ERROR;
					bp->b_error = error;
				}
				bp->b_flags &= ~B_ASYNC; /* ??? */
				bp->b_flags |= B_DONE;
				brelse(bp);
			} else if (!async_daemon_count ||
				   (mi->mi_flags & MIF_NO_SERVER)) {
				bp->b_flags &= ~B_ASYNC;
				bwrite(bp);
			} else if ((off + cnt < size)
				   || (mi->mi_flags & MIF_PRIVATE)) {
				rsetflag(rp, RDIRTY);
				bdwrite(bp);
			} else {
				rsetflag(rp, RDIRTY);
				bp->b_flags |= B_AGE;
				bawrite(bp);
			}
			/*
			 * Errors after the uiomove should be
			 * reflected in the uio structure.
			 */
			if (error) {
				uioupdate(uio, -cnt);
			}
		} else {
			/*
			 * If we used getchunk() earlier, the buffer contains
			 * garbage and must be nuked.
			 */
			bp->b_flags &= ~(B_ASYNC|B_NFS_ASYNC|
					 B_NFS_UNSTABLE|B_NFS_RETRY);
			bp->b_flags |= B_DONE|B_ERROR|B_STALE;
			brelse(bp);
		}
	} while (!error && uio->uio_resid > 0 && !eof);

	uio->uio_resid += remainder;
	if (error && rw == UIO_WRITE && uio->uio_resid != resid)
		error = 0;

	return (error);
}

/*
 * Write to file.  Writes to remote server in largest size
 * chunks that the server can handle.  Write is synchronous.
 */
static int
nfswrite(bdp, base, offset, count, cred, async, bp)
	bhv_desc_t *bdp;     
	caddr_t base;
	u_int offset;
	int count;
	struct cred *cred;
	int async;
	buf_t *bp;
{
	int error;
	struct nfswriteargs wa;
	struct nfsattrstat ns;
	int tsize;
	rnode_t *rp = bhvtor(bdp);
#ifdef DEBUG
	int	status;
#endif /* DEBUG */

	do {
		tsize = MIN(rtomi(rp)->mi_stsize, count);
		wa.wa_data = base;
		wa.wa_fhandle = *bhvtofh(bdp);
		wa.wa_begoff = offset;
		wa.wa_totcount = tsize;
		wa.wa_count = tsize;
		wa.wa_offset = offset;
		wa.wa_putbuf_ok = !(bp->b_flags & B_PRIVATE_B_ADDR);
		error = rfscall(rtomi(rp), RFS_WRITE,
				xdr_writeargs, (caddr_t)&wa,
				xdr_attrstat, (caddr_t)&ns,
				cred, async);
		if (!error) {
			error = geterrno(ns.ns_status);
			check_stale_fh(error, bdp);
		}
		count -= tsize;
		base += tsize;
		offset += tsize;
	} while (!error && count);

	if (!error) {
#ifdef DEBUG
		status = nfs_attrcache(bdp, &ns.ns_attr, NOFLUSH, NULL);
		if (status)
		    printf ("nfswrite: bad nfs_attrcache\n");
#else /* DEBUG */
		(void)nfs_attrcache(bdp, &ns.ns_attr, NOFLUSH, NULL);
#endif /* DEBUG */
	}
	switch (error) {
	case EDQUOT:
		printf("NFS write error: quota exceeded on host %s\n",
		    rtomi(rp)->mi_hostname);
	case 0:
	case EINTR:
		break;

	case ENOSPC:
		/* XXXbe fix to print exported pathname too */
		printf("NFS write error: remote file system full on host %s\n",
		   rtomi(rp)->mi_hostname);
		break;

	case ETIMEDOUT:
		printf("NFS write error: soft mount from host %s timed out\n",
		    rtomi(rp)->mi_hostname);
		break;

	default:
		printf("NFS write error %d on host %s\n",
		    error, rtomi(rp)->mi_hostname);
		break;
	}
	return (error);
}

/*
 * Read from a file.
 * Reads data in largest chunks our interface can handle
 */
static int
nfsread(bdp, base, offset, count, residp, cred, async)
	bhv_desc_t *bdp;
	caddr_t base;
	u_int offset;
	int count;
	int *residp;
	struct cred *cred;
	int async;
{
	int error;
	struct nfsreadargs ra;
	struct nfsrdresult rr;
	register int tsize;
	rnode_t *rp = bhvtor(bdp);

	do {
		tsize = MIN(rtomi(rp)->mi_tsize, count);
		rr.rr_data = base;
		ra.ra_fhandle = *bhvtofh(bdp);
		ra.ra_offset = offset;
		ra.ra_totcount = tsize;
		ra.ra_count = tsize;
		error = rfscall(rtomi(rp), RFS_READ, xdr_readargs, (caddr_t)&ra,
				xdr_rdresult, (caddr_t)&rr, cred, async);
		if (!error) {
			error = geterrno(rr.rr_status);
			check_stale_fh(error, bdp);
		}
		if (!error) {
			if (rr.rr_count > (unsigned)tsize) {
				error = EIO;
				break;
			}
			count -= rr.rr_count;
			base += rr.rr_count;
			offset += rr.rr_count;
		}
	} while (!error && count && rr.rr_count == tsize);

	*residp = count;

	/* It used to do an nfs_attrcache(...sflush) if error was zero,
	 * but this was the cause of a bug in which clients would see data
	 * they hadn't read from the server yet, so it was all zeros.
	 * to fix that we now use the attributes returned with a read only
	 * to determine if we should invalidate the attribute cache if
	 * mod time has changed
	 */
	if (!error) {
		if (rr.rr_attr.na_mtime.tv_sec != rp->r_nfsattr.na_mtime.tv_sec
			|| rr.rr_attr.na_mtime.tv_usec
					!= rp->r_nfsattr.na_mtime.tv_usec) {
			nfsattr_inval(bdp);
		}
	}
	return (error);
}

/*ARGSUSED*/
static int
nfs_ioctl(
	bhv_desc_t *bdp,
	int com,
	void *arg,
	int flag,
	struct cred *cred,
	int *rvalp,
        struct vopbd *vbds)
{

	return (ENOTTY);
}

/* ARGSUSED */
int
nfs_setfl(bhv_desc_t *bdp, int oflags, int nflags, cred_t *cr)
{
	rnode_t *rp = bhvtor(bdp);
	
	/* If we can't support BDS, return error if FDIRECT was set. */

	if ((nflags & FDIRECT) && !(rtomi(rp)->mi_flags & MIF_BDS))
		return EINVAL;

	return 0;
}

int
nfs_attrcache(bhv_desc_t *bdp, struct nfsfattr *na, enum staleflush fflag,
	      int *rlock_held)
{
	struct rnode *rp;
	int modified, delta;
	struct mntinfo *mi;
	vnode_t *vp;

	ASSERT(!rlock_held || *rlock_held);
	/*
	 * We must update the attributes >before< a potential call to
	 * VOP_INVALFREE_PAGES, which may call nfswrite which will recur through
	 * this code.  As Sun wrote it, sometimes old attributes will
	 * replace new ones.
	 */
	rp = bhvtor(bdp);
	ASSERT(rnode_is_locked(rp));
	vp = BHV_TO_VNODE(bdp);
	mi = rtomi(rp);
	modified = na->na_mtime.tv_sec != rp->r_nfsattr.na_mtime.tv_sec
		|| na->na_mtime.tv_usec != rp->r_nfsattr.na_mtime.tv_usec;
	rp->r_nfsattr = *na;
	nattr_to_iattr(na, bdp);

	if (!(mi->mi_flags & MIF_NOAC)) {
		rp->r_nfsattrtime.tv_sec = time;

		/*
		 * Decide how long rp's attributes will be considered fresh.
		 */
		delta = (time - na->na_mtime.tv_sec) >> 4;
		if (vp->v_type == VDIR) {
			if (delta < mi->mi_acdirmin) {
				delta = mi->mi_acdirmin;
			} else if (delta > mi->mi_acdirmax) {
				delta = mi->mi_acdirmax;
			}
		} else {
			if (delta < mi->mi_acregmin) {
				delta = mi->mi_acregmin;
			} else if (delta > mi->mi_acregmax) {
				delta = mi->mi_acregmax;
			}
		}
		rp->r_nfsattrtime.tv_sec += delta;
	}

	if (modified) {
		/*
		 * The file has changed.
		 * If this was unexpected (fflag == SFLUSH),
		 * flush the delayed write blocks associated with this vnode
		 * from the buffer cache and mark the cached blocks on the
		 * free list as invalid. Also flush the page cache.
		 * If this is a text mark it invalid so that the next pagein
		 * from the file will fail.
		 * If the vnode is a directory, purge the directory name
		 * lookup cache.
		 */
		if (fflag == SFLUSH) {
			if (vp->v_type == VREG) {
				ASSERT(rlock_held);
				ASSERT(*rlock_held);
				runlock(rp);
				*rlock_held = 0;
				VOP_INVALFREE_PAGES(vp, rp->r_localsize);
				return 0;
			}
		}
		switch (vp->v_type) {
		  case VDIR:
			dnlc_purge_vp(vp);
			break;
		  case VLNK:
			if (rp->r_symval) {
				kmem_free(rp->r_symval, rp->r_symlen);
				rp->r_symval = 0;
			}
		}
	}
    return (0);
}

int
nfs_getattr(bdp, vap, flags, cred)
	bhv_desc_t *bdp;
	struct vattr *vap;
	int flags;
	struct cred *cred;
{
	int error = 0;
	struct rnode *rp;
	struct nfsattrstat ns;
	int rlock_held = 0;
#ifdef DEBUG
	int	status;
#endif /* DEBUG */

	vnode_t *vp;

	rp = bhvtor(bdp);

	if (flags & ATTR_LAZY) { 
	    /*
	     * From Vfault, always Use cached attributes.
	     */
	    error = rlock_nohang(rp);
	    if (error) {
		    return error;
	    }
	    if (vap) {
		    nattr_to_vattr(rp, &rp->r_nfsattr, vap);
			/* Return the client's view of file size */
		    vap->va_size = rp->r_size;
	    }
	    runlock(rp);
	} else {
	    /*
	     * Locking here prevents multiple threads from going over the wire.
	     */
	    error = nfs_rwlock_nohang(bdp, VRWLOCK_WRITE);
	    if (error) {
		    return error;
	    }
	    if (time < rp->r_nfsattrtime.tv_sec) {
		error = rlock_nohang(rp);
		if (error) {
			nfs_rwunlock(bdp, VRWLOCK_WRITE);
			return error;
		}
		if (vap) {
			nattr_to_vattr(rp, &rp->r_nfsattr, vap);
			/* Return the client's view of file size */
			vap->va_size = rp->r_size;
		}
		runlock(rp);
	    } else {
		/*
		 * This is too horrible for words - everyt 3 seconds
		 * when the attributes time out we'll wipe out all
		 * mappings! This includes read-only executables
		 * Looks like SVR4 doesn't bother either
		if (vp->v_type == VREG && VN_MAPPED(vp))
			remapf(vp, 0, 1);
		 */
		error = sync_vp(bdp);    /* sync blocks so mod time is right */
		if (error) {
			nfs_rwunlock(bdp, VRWLOCK_WRITE);
			return error;
		}
		error = rlock_nohang(rp);
		if (error) {
			nfs_rwunlock(bdp, VRWLOCK_WRITE);
			return error;
		}
		rlock_held = 1;
		error = rfscall(rtomi(rp), RFS_GETATTR, xdr_fhandle,
		    (caddr_t)bhvtofh(bdp), xdr_attrstat, (caddr_t)&ns, cred, 0);
		if (!error) {
			error = geterrno(ns.ns_status);
			if (!error) {
				vp = BHV_TO_VNODE(bdp);
				if ((vp->v_type != VNON)
				    && (n2v_type(&ns.ns_attr) != vp->v_type)) {
					error = ESTALE;
					check_stale_fh(error, bdp);
				} else {
#ifdef DEBUG
					status = nfs_attrcache(bdp,
							&ns.ns_attr, SFLUSH,
							&rlock_held);
					if (status)
						printf ("nfs_getattr: bad nfs_attrcache\n");
#else /* DEBUG */
					(void)nfs_attrcache(bdp, &ns.ns_attr, 								    SFLUSH, 
							    &rlock_held);
#endif /* DEBUG */
					if (vap) {
						nattr_to_vattr(rp, &ns.ns_attr, vap);
					}
				}
			} else {
				check_stale_fh(error, bdp);
			}
		}
		if (rlock_held) {
			runlock(rp);
		}
	    }
	    nfs_rwunlock(bdp, VRWLOCK_WRITE);
	}
	return (error);
}

static int
nfs_setattr(bdp, vap, flags, cred)
	register bhv_desc_t *bdp;
	register struct vattr *vap;
	int flags;
	struct cred *cred;
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct rnode *rp = bhvtor(bdp);
	uid_t fuid = rp->r_attr.va_uid;
	int error;
	struct nfssaargs args;
	struct nfsattrstat ns;
	long mask = vap->va_mask;
	int rlock_held = 0;
#ifdef DEBUG
	int	status;
#endif /* DEBUG */

	if (mask & AT_NOSET)
		return (EINVAL);
	
	if (flags & ATTR_LAZY)
		return 0;	/* XXX punt for now */

	if (mask & (AT_ATIME | AT_MTIME)) {
		if (cred->cr_uid != fuid && !_CAP_CRABLE(cred, CAP_FOWNER)) {
			if (flags & ATTR_UTIME) {
				return (EPERM);
			}
		}
	}

	if (error = _MAC_VACCESS(vp, cred, VWRITE))
		return (error);

	if ((((vap->va_mask & AT_UID) && vap->va_uid > 0xffff) ||
	     ((vap->va_mask & AT_GID) && vap->va_gid > 0xffff)) &&
	    (rtomi(rp)->mi_flags & MIF_SHORTUID))
	{
		/* the fs is mounted shortuid and the user or group id won't 
		 * fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}

	ASSERT(vp->v_count > 0);
	error = nfs_rwlock_nohang(bdp, VRWLOCK_WRITE);
	if (error) {
		return error;
	}
	if (mask & AT_SIZE) {
		if ((vap->va_size < rp->r_size) && vp->v_type == VREG) {
			/*
			 * toss everything that might possibly be there
			 */
			VOP_TOSS_PAGES(vp, vap->va_size, MAX(rp->r_size, rp->r_localsize) - 1,
				FI_REMAPF_LOCKED);
		}
	}
	error = sync_vp(bdp);
	if (error) {
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
		return error;
	}

	vattr_to_sattr(vap, &args.saa_sa);

	/*
	 * Allow SysV-compatible option to set access and
	 * modified times if root, owner, or write access.
	 *
	 * XXX - Until an NFS Protocol Revision, this may be
	 *       simulated by setting the client time in the
	 *       tv_sec field of the access and modified times
	 *       and setting the tv_usec field of the modified
	 *       time to an invalid value (1,000,000).  This
	 *       may be detected by servers modified to do the
	 *       right thing, but will not be disastrous on
	 *       unmodified servers.
	 */
	if ((mask & (AT_ATIME|AT_MTIME)) && !(flags & ATTR_UTIME)) {
		args.saa_sa.sa_mtime.tv_usec = 1000000;
	}
	args.saa_fh = *bhvtofh(bdp);
	error = rlock_nohang(rp);
	if (error) {
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
		return error;
	}
	rlock_held = 1;
	error = rfscall(rtomi(rp), RFS_SETATTR, xdr_saargs,
	    (caddr_t)&args, xdr_attrstat, (caddr_t)&ns, cred, 0);
	if (!error) {
		error = geterrno(ns.ns_status);
		if (!error) {
#ifdef DEBUG
			status = nfs_attrcache(bdp, &ns.ns_attr, SFLUSH,
					       &rlock_held);
			if (status)
			    printf ("nfs_setattr: bad nfs_attrcache\n");
#else /* DEBUG */
			(void)nfs_attrcache(bdp, &ns.ns_attr, SFLUSH,
					    &rlock_held);
#endif /* DEBUG */
		} else {
			check_stale_fh(error, bdp);
		}
	}
	if (rlock_held) {
		runlock(bhvtor(bdp));
	}

	nfs_rwunlock(bdp, VRWLOCK_WRITE);
	return (error);
}

/* ARGSUSED */
static int
nfs_access(bdp, mode, cred)
	bhv_desc_t *bdp;
	int mode;
	struct cred *cred;
{
	int error;
	struct vattr va;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	ASSERT(vp->v_count > 0);
	if ((mode & VWRITE) && !WRITEALLOWED(vp, cred))
		return (EROFS);

	error = nfs_getattr(bdp, &va, 0, cred);
	if (error)
		return (error);
	/*
	 * MAC: Call mac_access to check that the label is OKay. 
	 * DAC: If you're the super-user, you always get access.
	 */
	if (error = _MAC_VACCESS(vp, cred, mode))
		return (error);

	if (cred->cr_uid == 0)
		return (0);

	if ((error = _ACL_VACCESS(vp, mode, cred)) != -1)
		return error;
	/*
	 * Access check is based on only
	 * one of owner, group, public.
	 * If not owner, then check group.
	 * If not a member of the group,
	 * then check public access.
	 */
	if (cred->cr_uid != va.va_uid) {
		mode >>= 3;
		if (!groupmember(va.va_gid, cred))
			mode >>= 3;
	}
	if ((va.va_mode & mode) != mode)
		return (EACCES);
	return (0);
}

/* ARGSUSED */
static int
nfs_readlink(
	bhv_desc_t *bdp,
	struct uio *uiop,
	struct cred *cred)
{
	u_int symttl;
	struct rnode *rp;
	int error;
	struct nfsrdlnres rl;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	if (error = _MAC_VACCESS(vp, cred, VREAD))
		return (error);

	if (vp->v_type != VLNK)
		return (ENXIO);
	rp = bhvtor(bdp);
	symttl = rtomi(rp)->mi_symttl;
	error = rlock_nohang(rp);
	if (error) {
		return error;
	}
	if (rp->r_symval) {
		if (time < rp->r_symtime && rp->r_symtime <= time + symttl) {
			error = uiomove(rp->r_symval, (int)rp->r_symlen,
					UIO_READ, uiop);
			runlock(rp);
			return error;
		}
		kmem_free(rp->r_symval, rp->r_symlen);
		rp->r_symval = 0;
	}
	rl.rl_data = kmem_alloc(NFS_MAXPATHLEN, KM_SLEEP);
	error = rfscall(rtomi(rp), RFS_READLINK,
			xdr_fhandle, (caddr_t)bhvtofh(bdp),
		        xdr_rdlnres, (caddr_t)&rl,
			cred, 0);
	if (!error) {
		error = geterrno(rl.rl_status);
		if (!error) {
			error = uiomove(rl.rl_data, (int)rl.rl_count,
					UIO_READ, uiop);
			if (symttl == 0) {
				kmem_free(rl.rl_data, NFS_MAXPATHLEN);
			} else {
				if (rl.rl_count > 0) {
				    rp->r_symval =
					kmem_realloc(rl.rl_data, rl.rl_count,
						     KM_SLEEP);
				} else {
				    kmem_free(rl.rl_data, NFS_MAXPATHLEN);
				    rp->r_symval = NULL;
				}
				rp->r_symlen = rl.rl_count;
				rp->r_symtime = time + symttl;
			}
		} else {
			check_stale_fh(error, bdp);
		}
	}
	runlock(rp);
	if (error) {
		kmem_free(rl.rl_data, NFS_MAXPATHLEN);
	}
	return (error);
}

/*ARGSUSED*/
static int
nfs_fsync_locked(bdp, flag, cred, start, stop)
	bhv_desc_t *bdp;
	int flag;
	struct cred *cred;
	off_t start;
	off_t stop;
{
	register struct rnode *rp;
	int error = 0;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	rp = bhvtor(bdp);
	if (flag & FSYNC_INVAL) {
		VOP_FLUSHINVAL_PAGES(vp, 0, rp->r_localsize - 1, FI_REMAPF_LOCKED);
		ASSERT(!VN_DIRTY(vp) && vp->v_pgcnt == 0);
		rp->r_localsize = 0;
	} else {
		error = sync_vp(bdp);
	}
	if (!error && rp->r_bds)
		error = bds_sync(rp, cred);
	return (rp->r_error ? rp->r_error : error);
}

/*ARGSUSED*/
static int
nfs_fsync(bdp, flag, cred, start, stop)
	bhv_desc_t *bdp;
	int flag;
	struct cred *cred;
	off_t start;
	off_t stop;
{
	int error = nfs_rwlock_nohang(bdp, VRWLOCK_WRITE);
	if (error) {
		return error;
	}
	error = nfs_fsync_locked(bdp, flag, cred, start, stop);
	nfs_rwunlock(bdp, VRWLOCK_WRITE);
	return error;
}

static int
sync_vp(bdp)
	bhv_desc_t *bdp;
{
	/*
	 * flush if we think rnode is DIRTY (i.e. went through nfswrite)
	 * or if there are any pages on delwri list that have NOT yet
	 * gone through nfswrite (like via mmap/mapdetach)
	 */
	if ((bhvtor(bdp)->r_flags & RDIRTY) || VN_DIRTY(BHV_TO_VNODE(bdp))) {
		return flush_bio(bdp, 0);
	}

	return 0;
}

/*
 * Weirdness: if the file was removed while it was open it got renamed
 * (by nfs_remove) instead.  Here we remove the renamed file.
 * Note that this could be last use - an already closed file that was still
 * mmaped.
 */
/*ARGSUSED*/
int
nfs_inactive(bdp, cred)
	register bhv_desc_t *bdp;
	struct cred *cred;
{
	register struct rnode *rp;
	struct nfsdiropargs da;
	enum nfsstat status;
	int error;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	rp = bhvtor(bdp);
	rclrflag(rp, RCLEANLOCKS);
	if (vp->v_type != VLNK && rp->r_unldrp != NULL) {
		nfs_rwlock(bdp, VRWLOCK_WRITE);
		if (rtomi(rp)->mi_flags & MIF_PRIVATE) {
			/*
			 * rp->r_localsize is max mapping inserted
			 * in pagecache via chunkread/getchunk.
			 */
			VOP_TOSS_PAGES(vp, 0, rp->r_localsize - 1, FI_NONE);
			ASSERT(!VN_DIRTY(vp) && vp->v_pgcnt == 0);
			rp->r_localsize = 0;

			/*
			 * We'd like to cancel pending writes, since we
			 * are about to remove the file and want to avoid
			 * a race, but we'll settle for flushing.
			 */
		}
		flush_bio(bdp, 1);
		rclrflag(rp, RDIRTY);
		nfs_rwunlock(bdp, VRWLOCK_WRITE);

		/*
		 * Another process could have rp->unldvp locked right now,
		 * waiting in to acquire a reference to vp in vn_get().
		 * This will deadlock unless we mark the rnode as gone,
		 * and wake up the process waiting on the vnode while also
		 * setting a flag that indicates that it is (nearly) gone.
		 */
		vn_gone(vp);

		error = rlock_nohang(rp->r_unldrp);
		if (!error) {
			setdiropargs(&da, rp->r_unlname, rtobhv(rp->r_unldrp));
			error = rfscall(rtomi(rp->r_unldrp), RFS_REMOVE,
					xdr_diropargs, (caddr_t)&da,
					xdr_enum, (caddr_t)&status,
					rp->r_unlcred, 0);
			if (!error)
				error = geterrno(status);
			runlock(rp->r_unldrp);
		}
		if (error) {
			printf("NFS open unlink error %d file %s on host %s\n",
				error, rp->r_unlname, rtomi(rp)->mi_hostname);
		}
		VN_RELE(rtov(rp->r_unldrp));
		rp->r_unldrp = NULL;
		kmem_free(rp->r_unlname, NFS_MAXNAMLEN);
		rp->r_unlname = NULL;
		crfree(rp->r_unlcred);
		rp->r_unlcred = NULL;
	} else {
		nfs_rwlock(bdp, VRWLOCK_WRITE);
		flush_bio(bdp, 1);
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
	}
	if (rp->r_bds) {
		nfs_rwlock(bdp, VRWLOCK_WRITE);
		bds_close(rp, cred);
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
	}

#ifdef NSD
	/*
	 * For local servers (nsd) we call a close routine
	 * so they know the file is no longer in use.
	 */
	{
		mntinfo_t *mi;
		bool_t b;
		
		mi = rtomi(rp);
		if ((mi->mi_flags & MIF_SILENT) && (vp->v_type != VDIR)) {
			error = rlock_nohang(rp);
			if (error) {
				return error;
			}

			error = nsd_rfscall(mi, NSDPROC1_CLOSE,
			    xdr_nfs_nsdfh, (caddr_t)rtofh(rp), xdr_bool,
			    (caddr_t)&b, cred);

			runlock(rp);
			check_stale_fh(ESTALE, bdp);
		}
	}
#endif

	/*
	 * Forget any asynchronous errors which till now may have stopped
	 * further i/o.  Reset the readahead detector on last close.
	 */
	rp->r_error = 0;
	rp->r_lastr = 0;

	return VN_INACTIVE_CACHE;
}

static int
nfs_fid(bdp, fidpp)
	bhv_desc_t *bdp;
	struct fid **fidpp;
{
	/*
	 * the fid is just the file handle
	 * allocate a fid structure and copy the file handle into it
	 */
	*fidpp = (struct fid *)kmem_alloc(sizeof(fid_t), KM_SLEEP);
	(*fidpp)->fid_len = sizeof(fhandle_t);
	bcopy( bhvtofh(bdp), (*fidpp)->fid_data, sizeof(fhandle_t) );
	return 0;
}

static int
nfs_fid2(bdp, fidp)
	bhv_desc_t *bdp;
	struct fid *fidp;
{
	/*
	 * the fid is just the file handle
	 */
	fidp->fid_len = sizeof(fhandle_t);
	bcopy( bhvtofh(bdp), fidp->fid_data, sizeof(fhandle_t) );
	return 0;
}

/* ARGSUSED */
void
nfs_rwlock(bdp, write_flag)
	bhv_desc_t *bdp;
	vrwlock_t write_flag;
{
	struct rnode *rp = bhvtor(bdp);
	uint64_t id = get_thread_id();

	if (id == rp->r_rwlockid) {
		ASSERT(mutex_mine(&(rp->r_rwlock)));
		rp->r_locktrips++;
		return;
	}

	/*
	 * BDS note: BDS counts on all I/O being single threaded.  If you
	 * change this to a reader/writer lock, then do what nfs3 does and
	 * upgrade the lock to a writer lock if r_bds is set.
	 */
	mutex_lock(&rp->r_rwlock, PINOD);
	rp->r_rwlockid = id;
}

/* ARGSUSED */
int
nfs_rwlock_nohang(bdp, write_flag)
	bhv_desc_t *bdp;
	vrwlock_t write_flag;
{
	struct rnode *rp = bhvtor(bdp);
	uint64_t id = get_thread_id();
	timespec_t ts;
	int error = 0;

	if (id == rp->r_rwlockid) {
		ASSERT(mutex_mine(&(rp->r_rwlock)));
		rp->r_locktrips++;
		return 0;
	}

	/*
	 * BDS note: BDS counts on all I/O being single threaded.  If you
	 * change this to a reader/writer lock, then do what nfs3 does and
	 * upgrade the lock to a writer lock if r_bds is set.
	 */

	if (!nohang()) {
		mutex_lock(&rp->r_rwlock, PINOD);
		rp->r_rwlockid = id;
		return 0;
	} else {
		mntinfo_t *mi = rtomi(rp);
		int tryagain;

#ifdef DEBUG_NOHANG
		if (debug_rwlock()) {
			return ETIMEDOUT;
		}
#endif
		if (mi->mi_flags & MIF_PRINTED) {
			return ETIMEDOUT;
		}
		ts.tv_sec = mi->mi_timeo / 10;
		ts.tv_nsec = 100000000 * (mi->mi_timeo % 10);
		do {
			tryagain = 0;
			mutex_enter(&mi->mi_lock);
			if (!mutex_trylock(&rp->r_rwlock)) {
				if (sv_timedwait_sig(&mi->mi_nohang_wait, PZERO+1,
						     &mi->mi_lock, 0, 0, &ts, NULL)) {
					error = EINTR;
				} else if (mi->mi_flags & MIF_PRINTED) {
					error = ETIMEDOUT;
				} else {
					tryagain = 1;
				}
			} else {
				mutex_exit(&mi->mi_lock);
				rp->r_rwlockid = id;
			}
		} while(tryagain);
	}

	return(error);
}

int
nfs_rwlock_nowait(struct rnode *rp)
{
	if (mutex_trylock(&rp->r_rwlock)) {
		rp->r_rwlockid = get_thread_id();
		return 1;
	}
	return 0;
}

/* ARGSUSED */
void
nfs_rwunlock(bdp, write_flag)
	bhv_desc_t *bdp;
	vrwlock_t write_flag;
{
	struct rnode *rp = bhvtor(bdp);

	ASSERT(get_thread_id() == rp->r_rwlockid);

	if (rp->r_locktrips > 0) {
		--rp->r_locktrips;
		return;
	}

	rp->r_rwlockid = (uint64_t)-1L;
	mutex_unlock(&(rp->r_rwlock));
}

/* ARGSUSED */
static int
nfs_seek(bdp, ooff, noffp)
	bhv_desc_t *bdp;
	off_t ooff;
	off_t *noffp;
{
	return *noffp < 0 ? EINVAL : 0;
}

/*
 * Remote file system operations having to do with directory manipulation.
 */
/* ARGSUSED */
static int
nfs_lookup(bdp, nm, vpp, pnp, flags, rdir, cred)
	bhv_desc_t *bdp;
	char *nm;
	struct vnode **vpp;
	struct pathname *pnp;
	int flags;
	struct vnode *rdir;
	struct cred *cred;
{
	int error, isdotdot;
	struct ncfastdata fd;
	struct nfsdiropargs da;
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	struct mntinfo *mi = vn_bhvtomi(bdp);
	bhv_desc_t *nbdp;

	/*
	 * Before checking dnlc, validate caches
	 */
	if (error = nfs_getattr(bdp, 0, 0, cred)) {
		return (error);
	}
	/*
	 * NB: "." and ".." are the same in an fs root directory.
	 * XXXbe do this after dnlc_lookup_fast and use fd.flags & PN_ISDOT
	 */
	if (ISDOT(nm) || (isdotdot = ISDOTDOT(nm)) && (dvp->v_flag & VROOT)) {
		*vpp = vn_hold(dvp);
		return (0);
	}
	if (!isdotdot) {
		error = rlock_nohang(bhvtor(bdp));
		if (error) {
			return error;
		}
	}
	nbdp = dnlc_lookup_fast(dvp, nm, pnp, &fd, cred, 0);
	if (nbdp == NULL) {
		struct nfsdiropres dr;
		setdiropargs(&da, nm, bdp);

again_lookup:
		error = rfscall(mi, RFS_LOOKUP, xdr_diropargs,
		    (caddr_t)&da, xdr_diropres, (caddr_t)&dr, cred, 0);
		if (!error) {
			error = geterrno(dr.dr_status);
			check_stale_fh(error, bdp);
		}
		if (!error) {
			error = makenfsnode(dvp->v_vfsp, mi, &dr.dr_fhandle,
					    &dr.dr_attr, &nbdp);
			if (!error) {
				*vpp = BHV_TO_VNODE(nbdp);
				dnlc_enter_fast(dvp, &fd, nbdp, cred);
			}
			if (error == EAGAIN) {
#ifdef DEBUG
				printf ("nfs_lookup: try again\n");
#endif /* DEBUG */
				goto again_lookup;
			}
		}

		if (!isdotdot)
			runlock(bhvtor(bdp));
	} else {
		*vpp = BHV_TO_VNODE(nbdp);
		if (!isdotdot)
			runlock(bhvtor(bdp));

		/*
		 * Make sure we can search this directory (after the fact).
		 * It's done here because over the wire lookups verify 'x'
		 * permissions on the server.  VOP_ACCESS will one day go
		 * over the wire, so let's use it sparingly.
		 */
		VOP_ACCESS(dvp, VEXEC, cred, error);
		if (error) {
			VN_RELE(*vpp);
		}
  	}
	if (!error && ISVDEV((*vpp)->v_type)) {
		struct vnode *newvp;

		newvp = spec_vp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type, cred);

		VN_RELE(*vpp);
		*vpp = newvp;
#ifdef REMOTE_IO
		}
#endif
	}
	return (error);
}

/*ARGSUSED*/
static int
nfs_create(
	bhv_desc_t *bdp,
	char *nm,
	struct vattr *va,
	int flags,
	int mode,
	struct vnode **vpp,
	struct cred *cred)
{
	int error;
	struct nfscreatargs args;
	struct nfsdiropres dr;
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	rnode_t *drp = bhvtor(bdp);
	struct mntinfo *mi = vn_bhvtomi(bdp);
	bhv_desc_t *nbdp;

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}

	/*
	 * Creation requires write access to the directory
	 */
	if (error = _MAC_VACCESS(dvp, cred, VWRITE))
		return (error);

	error = nfs_lookup(bdp, nm, vpp, (struct pathname *) NULL,
			       0, 0, cred);
	if (error == 0) {
		if (flags & VEXCL)
			/*
			 * This is buggy: there is a race between the lookup and the
			 * create.  We should send the exclusive flag over the wire.
			 */
			error = EEXIST;
		else if ((*vpp)->v_type == VDIR && (mode & VWRITE))
			error = EISDIR;
		else {
			VOP_ACCESS(*vpp, mode, cred, error);
			if (!error) {
				if ((va->va_mask & AT_SIZE) &&
					(*vpp)->v_type == VREG) {
					va->va_mask = AT_SIZE;
					nbdp = vn_bhv_lookup_unlocked(
							VN_BHV_HEAD(*vpp),
							&nfs_vnodeops);
					ASSERT(nbdp != NULL);
					error = nfs_setattr(nbdp, va, 0, cred);
				}
			}
		}
		if (error) {
			VN_RELE(*vpp);
		}
		return (error);
	} else if (error == ENOENT) {
		/*
		 * XPG4 says create cannot allocate a file if the
		 * file size limit is set to 0.
		 */
		if (flags & VZFS) {
			return EFBIG;
		}
	}
	*vpp = NULL;
	setdiropargs(&args.ca_da, nm, bdp);

	/*
	 * Decide what the group-id of the created file should be.
	 * Set it in attribute list as advisory...then do a setattr
	 * if the server didn't get it right the first time.
	 */
	va->va_gid = setdirgid(bdp);

	/* set va_uid since vn_open doesn't set it */
	va->va_uid = cred->cr_uid;

	
	if ((va->va_uid > 0xffff || va->va_gid > 0xffff) &&
	    (rtomi(drp)->mi_flags & MIF_SHORTUID))
	{
		/* We're creating the file, the fs is mounted shortuid
		 * and the user or group id won't fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}

	/*
	 * This is a completely gross hack to make mknod
	 * work over the wire until we can wack the protocol
	 */
#define	IFCHR		0020000		/* character special */
#define	IFBLK		0060000		/* block special */
#define	IFSOCK		0140000		/* socket */
	if (va->va_type == VCHR) {
		va->va_mode |= IFCHR;
		va->va_mask |= AT_SIZE;
		va->va_size = (u_long)nfs_cmpdev((u_long)va->va_rdev);
	} else if (va->va_type == VBLK) {
		va->va_mode |= IFBLK;
		va->va_mask |= AT_SIZE;
		va->va_size = (u_long)nfs_cmpdev((u_long)va->va_rdev);
	} else if (va->va_type == VFIFO) {
		va->va_mode |= IFCHR;		/* xtra kludge for namedpipe */
		va->va_size = NFS_FIFO_DEV;	/* blech */
		va->va_mask |= AT_SIZE;
	} else if (va->va_type == VSOCK) {
		va->va_mode |= IFSOCK;
	}

	vattr_to_sattr(va, &args.ca_sa);
	if (va->va_type == VCHR || va->va_type==VBLK || va->va_type==VFIFO)
		va->va_mask &= ~AT_SIZE;
create_again:
	error = rlock_nohang(bhvtor(bdp));
	if (error) {
		return error;
	}
	error = rfscall(mi, RFS_CREATE,
			xdr_creatargs, (caddr_t)&args,
			xdr_diropres, (caddr_t)&dr,
			cred, 0);
	nfsattr_inval(bdp);	/* mod time changed */
	if (!error) {
		error = geterrno(dr.dr_status);
		check_stale_fh(error, bdp);
	}
	if (!error) {
		error = makenfsnode(dvp->v_vfsp, mi, &dr.dr_fhandle, &dr.dr_attr,
				    &nbdp);
		if (error && (error == EAGAIN)) {
#ifdef DEBUG
			printf ("nfs_create: try again\n");
#endif /* DEBUG */
			runlock(bhvtor(bdp));
			goto create_again;
		}
		if (!error) {
			register struct vnode *vp = BHV_TO_VNODE(nbdp);
			struct rnode *rp = bhvtor(nbdp);
			gid_t gid;

			*vpp = vp;
			dnlc_enter(dvp, nm, nbdp, cred);

			/*
			 * Make sure the gid was set correctly.
			 * If not, try to set it (but don't lose
			 * any sleep over it).
			 */
			gid = va->va_gid;
			nattr_to_vattr(rp, &rp->r_nfsattr, va);
			if (gid != va->va_gid &&
			    /* Don't bother if it would mean passing
			     * a long gid when we're not allowed to
			     */
				(gid <= 0xffff || !(rtomi(drp)->mi_flags & MIF_SHORTUID)))
			{
				struct vattr vattr;

				vattr.va_mask = AT_GID;
				vattr.va_gid = gid;
				(void) nfs_setattr(nbdp, &vattr, 0, cred);
				va->va_gid = gid;
			}
			/*
			 * If vnode is a device, create special node.
			 */
			if (ISVDEV((vp)->v_type)) {
				struct vnode *newvp;

				newvp = spec_vp(vp, vp->v_rdev,
						vp->v_type, cred);
				VN_RELE(vp);
				*vpp = newvp;
			}
		}
	}
	runlock(bhvtor(bdp));
	return (error);
}

/*
 * Weirdness: if the vnode to be removed is open
 * we rename it instead of removing it and nfs_inactive
 * will remove the new name.
 */
static int nfs_rename(bhv_desc_t *, char *, struct vnode *, char *,
		      struct pathname *, struct cred *);

static int
nfs_remove(dbdp, nm, cred)
	bhv_desc_t *dbdp;
	char *nm;
	struct cred *cred;
{
	int error;
	struct nfsdiropargs da;
	enum nfsstat status;
	struct vnode *vp;
	struct vnode *oldvp;
	struct vnode *realvp;
	struct rnode *rp;
	char *tmpname;
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	rnode_t *drp = bhvtor(dbdp);
	bhv_desc_t *bdp;

	/*
	 * For MAC: remove the object iff the caller has write access
	 * to the containing directory
	 */
	if (error = _MAC_VACCESS(dvp, cred, VWRITE))
		return (error);
	if (error = nfs_lookup(dbdp, nm, &vp, (struct pathname *) NULL,
			       0, (struct vnode *)0, cred)) {
		return (error);
	}
	/*
	 * Lookup may have returned a non-nfs vnode!
	 * get the real vnode.
	 */
	VOP_REALVP(vp, &realvp, error);
	if (!error) {
		oldvp = vp;
		vp = realvp;
	} else {
		oldvp = NULL;
	}

	dnlc_purge_vp(vp);

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &nfs_vnodeops);
	ASSERT(bdp != NULL);

	rp = bhvtor(bdp);
	if (vp->v_type != VLNK && vp->v_count > 1 && rp->r_unldrp == NULL) {

		tmpname = newname();
		error = nfs_rename(dbdp, nm, dvp, tmpname, NULL, cred);
		if (error) {
			kmem_free(tmpname, NFS_MAXNAMLEN);
		} else {
			VN_HOLD(dvp);
			rp->r_unldrp = bhvtor(dbdp);
			rp->r_unlname = tmpname;
			if (rp->r_unlcred != NULL) {
				crfree(rp->r_unlcred);
			}
			crhold(cred);
			rp->r_unlcred = cred;
		}
	} else {
		if (!(rtomi(rp)->mi_flags & MIF_PRIVATE)) {
			rclrflag(rp, RDIRTY);
		} else if (vp->v_type == VREG && rp->r_nfsattr.na_nlink == 1) {
			/*
			 * Toss up to highest page locally mapped.
			 */
			nfs_rwlock(bdp, VRWLOCK_WRITE);
			VOP_TOSS_PAGES(vp, 0, rp->r_localsize - 1, FI_NONE);
			nfs_rwunlock(bdp, VRWLOCK_WRITE);
			ASSERT(!VN_DIRTY(vp) && vp->v_pgcnt == 0);
			rp->r_localsize = 0;
			rclrflag(rp, RDIRTY);
		}

		setdiropargs(&da, nm, dbdp);

		error = rlock_nohang(drp);
		if ( error ) {
			if (oldvp)
				VN_RELE(oldvp);
			else
				VN_RELE(vp);
			return (error);
		}
		error = rfscall(rtomi(drp), RFS_REMOVE, xdr_diropargs,
		    (caddr_t)&da, xdr_enum, (caddr_t)&status, cred, 0);
		nfsattr_inval(dbdp);	/* mod time changed */
		nfsattr_inval(bdp);	/* link count changed */
		if (!error) {
			error = geterrno(status);
		}
		runlock(drp);
		check_stale_fh(error, dbdp);
	}

	if (oldvp) {
		VN_RELE(oldvp);
	} else {
		VN_RELE(vp);
	}
	return (error);
}

static int
nfs_link(dbdp, vp, tnm, cred)
	bhv_desc_t *dbdp;
	struct vnode *vp;
	char *tnm;
	struct cred *cred;
{
	int error;
	struct nfslinkargs args;
	enum nfsstat status;
        struct vnode *realvp;
	bhv_desc_t *bdp;
	rnode_t *rp;
	int lock_held = 0;

	/* May have been passed an snode.  Convert to rnode */
	VOP_REALVP(vp, &realvp, error);
        if (!error) {
                vp = realvp;
        }
	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &nfs_vnodeops);
	rp = bhvtor(bdp);
	ASSERT(bdp != NULL);
	args.la_from = *bhvtofh(bdp);
	setdiropargs(&args.la_to, tnm, dbdp);
	error = rlock_nohang(bhvtor(dbdp));
	if (!error) {
		lock_held = 1;
		error = rfscall(rtomi(rp), RFS_LINK, xdr_linkargs,
				(caddr_t)&args, xdr_enum,
				(caddr_t)&status, cred, 0);
	}
	nfsattr_inval(dbdp);	/* mod time changed */
	nfsattr_inval(bdp);	/* link count changed */
	/*
	 * If we're private, kick the vnode out of the dnlc so the
	 * check of the link count in nfs_remove will be accurate.
	 * It's not pretty, but it's fast and special cased to link/unlink
	 * sequences on privately mounted file systems.
	 */
	if (rtomi(rp)->mi_flags & MIF_PRIVATE) {
		dnlc_purge_vp(vp);
	}
	if (lock_held) {
		runlock(bhvtor(dbdp));
	}
	if (!error) {
		error = geterrno(status);
		check_stale_fh(error, bdp);
		check_stale_fh(error, dbdp);
	}
	return (error);
}

/* ARGSUSED */
static int
nfs_rename(bdp, onm, ndvp, nnm, npnp, cred)
	bhv_desc_t *bdp;
	char *onm;
	struct vnode *ndvp;
	char *nnm;
	struct pathname *npnp;
	struct cred *cred;
{
	int error;
	enum nfsstat status;
	struct nfsrnmargs args;
	struct vnode *realvp;
	vnode_t *odvp = BHV_TO_VNODE(bdp);
	rnode_t *odrp = bhvtor(bdp);
	vnode_t *ovp;
	vnode_t *nvp;
	char *tmpname;
	rnode_t *rp;
	bhv_desc_t *ndbdp;
	bhv_desc_t *nbdp;

	if (ISDOT(onm) || ISDOTDOT(onm) || ISDOT(nnm) || ISDOTDOT(nnm)) {
		return (EINVAL);
	}

	VOP_REALVP(ndvp, &realvp, error);
	if (!error) {
		ndvp = realvp;
	}

	ndbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(ndvp), &nfs_vnodeops);
	if (ndbdp == NULL) {
		return (EXDEV);
	}

	/*
	 * Lookup the target file.  If it exists, it needs to be
	 * checked to see whether it is a mount point and whether
	 * it is active (open).
	 */
	error = nfs_lookup(ndbdp, nnm, &nvp, NULL, 0, NULL, cred);
	if (!error) {
		/*
		 * If this file has been mounted on, then just
		 * return busy because renaming to it would remove
		 * the mounted file system from the name space.
		 */
		if (nvp->v_vfsmountedhere != NULL) {
			VN_RELE(nvp);
			return (EBUSY);
		}
		/*
		 * Purge the name cache of all references to this vnode
		 * so that we can check the reference count to infer
		 * whether it is active or not.
		 */
		dnlc_purge_vp(nvp);

		/*
		 * If the vnode is active, arrange to rename it to a
		 * temporary file so that it will continue to be
		 * accessible.  This implements the "unlink-open-file"
		 * semantics for the target of a rename operation.
		 * Before doing this though, make sure that the
		 * source and target files are not already the same.
		 */
		if (nvp->v_type != VLNK && nvp->v_count > 1) {

			/*
			 * Lookup the source name.
			 */
			error = nfs_lookup(bdp, onm, &ovp, NULL, 0, NULL, cred);

			/*
			 * The source name *should* already exist.
			 */
			if (error) {
				VN_RELE(nvp);
				return (error);
			}

			/*
			 * Compare the two vnodes.  If they are the same,
			 * just release all held vnodes and return success.
			 */
			if (ovp == nvp) {
				VN_RELE(ovp);
				VN_RELE(nvp);
				return (0);
			}

			/*
			 * Can't mix and match directories and non-
			 * directories in rename operations.  If
			 * they don't match, then return the error
			 * based on the type for source (ovp) file.
			 */
			if (ovp->v_type == VDIR) {
				if (nvp->v_type != VDIR)
					error = ENOTDIR;
			} else {
				if (nvp->v_type == VDIR)
					error = EISDIR;
			}

			if (error) {
				VN_RELE(ovp);
				VN_RELE(nvp);
				return (error);
			}

			VN_RELE(ovp);

			/*
			 * The target file exists, is not the same as
			 * the source file, and is active.  Rename it
			 * to avoid the server removing the file
			 * completely.
			 */
			tmpname = newname();
			error = nfs_rename(ndbdp, nnm, ndvp, tmpname, npnp, cred);
			if (error)
				kmem_free((caddr_t)tmpname, NFS_MAXNAMLEN);
			else {
				nbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(nvp),
							      &nfs_vnodeops);
				ASSERT(nbdp != NULL);
				rp = bhvtor(nbdp);
				mutex_enter(&rp->r_statelock);
				if (rp->r_unldrp == NULL) {
					VN_HOLD(ndvp);
					rp->r_unldrp = bhvtor(ndbdp);
					if (rp->r_unlcred != NULL)
						crfree(rp->r_unlcred);
					crhold(cred);
					rp->r_unlcred = cred;
					rp->r_unlname = tmpname;
				} else {
					kmem_free((caddr_t)rp->r_unlname,
						NFS_MAXNAMLEN);
					rp->r_unlname = tmpname;
				}
				mutex_exit(&rp->r_statelock);
			}
		}

		VN_RELE(nvp);
	}
	if (bhvtor(bdp)->r_nfsattr.na_fsid 
	    != bhvtor(ndbdp)->r_nfsattr.na_fsid) {
		return EXDEV;
	}

	if (ndvp == odvp) {
                error = rlock_nohang(bhvtor(bdp));
		if (error) {
			return error;
		}
        } else if (odvp->v_number < ndvp->v_number) {
                error = rlock_nohang(bhvtor(bdp));
		if (error) {
			return error;
		}
                error = rlock_nohang(bhvtor(ndbdp));
		if (error) {
			runlock(bhvtor(bdp));
			return error;
		}
        } else {
                error = rlock_nohang(bhvtor(ndbdp));
		if (error) {
			return error;
		}
                error = rlock_nohang(bhvtor(bdp));
		if (error) {
			runlock(bhvtor(ndbdp));
			return error;
		}
        }


	dnlc_remove(odvp, onm);
	dnlc_remove(ndvp, nnm);
	setdiropargs(&args.rna_from, onm, bdp);
	setdiropargs(&args.rna_to, nnm, ndbdp);
	error = rfscall(rtomi(odrp), RFS_RENAME,
			xdr_rnmargs, (caddr_t)&args,
			xdr_enum, (caddr_t)&status,
			cred, 0);
	nfsattr_inval(bdp);	/* mod time changed */
	nfsattr_inval(ndbdp);	/* mod time changed */
	runlock(bhvtor(bdp));
	if (ndvp != odvp) {
		runlock(bhvtor(ndbdp));
	}
	if (!error) {
		error = geterrno(status);
		check_stale_fh(error, bdp);
		check_stale_fh(error, ndbdp);
	}
	return (error);
}

static int
nfs_mkdir(bdp, nm, va, vpp, cred)
	bhv_desc_t *bdp;
	char *nm;
	register struct vattr *va;
	struct vnode **vpp;
	struct cred *cred;
{
	int error;
	struct nfscreatargs args;
	struct nfsdiropres dr;
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	struct mntinfo *mi = vn_bhvtomi(bdp);
	bhv_desc_t *nbdp;

	/*
	 * Creation requires write access to the directory
	 */
	if (error = _MAC_VACCESS(dvp, cred, VWRITE))
		return (error);

	*vpp = NULL;
	setdiropargs(&args.ca_da, nm, bdp);

	/*
	 * Decide what the group-id and set-gid bit of the created directory
	 * should be.  May have to do a setattr to get the gid right.
	 */
	va->va_gid = setdirgid(bdp);
	va->va_mode = setdirmode(bdp, va->va_mode);
	va->va_uid = cred->cr_uid;

	if ((va->va_uid > 0xffff || va->va_gid > 0xffff) &&
	    (mi->mi_flags & MIF_SHORTUID))
	{
		/* the fs is mounted shortuid and the user or group id won't 
		 * fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}
	vattr_to_sattr(va, &args.ca_sa);

	error = rlock_nohang(bhvtor(bdp));
	if (!error) {
		error = rfscall(mi, RFS_MKDIR,
				xdr_creatargs, (caddr_t)&args,
				xdr_diropres, (caddr_t)&dr,
				cred, 0);
		nfsattr_inval(bdp);	/* mod time changed */
		runlock(bhvtor(bdp));
	}
	if (!error) {
		error = geterrno(dr.dr_status);
		check_stale_fh(error, bdp);
	}
	if (!error) {
		gid_t gid;

		error = makenfsnode(dvp->v_vfsp, mi, &dr.dr_fhandle, &dr.dr_attr,
				    &nbdp);
		if (error == EAGAIN) {  /* bad times, retry w/o attrs */
		    error = makenfsnode(dvp->v_vfsp, mi, &dr.dr_fhandle, NULL,
					&nbdp);
		}
		if (!error) {
			*vpp = BHV_TO_VNODE(nbdp);
			/*
			 * Due to a pre-4.0 server bug the attributes that
			 * come back on mkdir are not correct. Use them only
			 * to set the vnode type in makenfsnode.
			 */
			nfsattr_inval(nbdp);

			dnlc_enter(dvp, nm, nbdp, cred);

			/*
			 * Make sure the gid was set correctly.
			 * If not, try to set it (but don't lose
			 * any sleep over it).
			 */
			gid = va->va_gid;
			nattr_to_vattr(bhvtor(nbdp), &dr.dr_attr, va);
			if (gid != va->va_gid) {
				va->va_mask = AT_GID;
				va->va_gid = gid;
				(void) nfs_setattr(nbdp, va, 0, cred);
			}
		}
	}
	return (error);
}

static int
nfs_rmdir(bdp, nm, cdir, cred)
	bhv_desc_t *bdp;
	char *nm;
	struct vnode *cdir;
	struct cred *cred;
{
	int error;
	struct vnode *vp;
	enum nfsstat status;
	struct nfsdiropargs da;
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	rnode_t *drp = bhvtor(bdp);

        /*
         * Attempt to prevent a rmdir(".") from succeeding.
         */
	VOP_LOOKUP(dvp, nm, &vp, (struct pathname *) 0, 0,	
			(struct vnode *) 0, cred, error);
        if (error) {
                return(error);
	}
	if (VN_CMP(vp, cdir)) {
		VN_RELE(vp);
		return (EINVAL);
	}
	VN_RELE(vp);

	setdiropargs(&da, nm, bdp);
	error = rlock_nohang(bhvtor(bdp));
	if (!error) {
		dnlc_purge_vp(dvp);
		error = rfscall(rtomi(drp), RFS_RMDIR, xdr_diropargs,
				(caddr_t)&da, xdr_enum,
				(caddr_t)&status, cred, 0);
		nfsattr_inval(bdp);	/* mod time changed */
		runlock(bhvtor(bdp));
	}
	if (!error) {
		error = geterrno(status);
		check_stale_fh(error, bdp);
	}
	return (error);
}

static int
nfs_symlink(bdp, lnm, tva, tnm, cred)
	bhv_desc_t *bdp;
	char *lnm;
	struct vattr *tva;
	char *tnm;
	struct cred *cred;
{
	int error;
	struct nfsslargs args;
	enum nfsstat status;
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	rnode_t *drp = bhvtor(bdp);

	/*
	 * Creation requires write access to the directory
	 */
	if (error = _MAC_VACCESS(dvp, cred, VWRITE))
		return (error);

	if ((cred->cr_uid > 0xffff || cred->cr_gid > 0xffff) && 
	    (rtomi(drp)->mi_flags & MIF_SHORTUID))
	{
		/* the fs is mounted shortuid and the user or group id won't 
		 * fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}

	setdiropargs(&args.sla_from, lnm, bdp);
	vattr_to_sattr(tva, &args.sla_sa);
	args.sla_tnm = tnm;
	error = rfscall(rtomi(drp), RFS_SYMLINK, xdr_slargs, (caddr_t)&args,
	    xdr_enum, (caddr_t)&status, cred, 0);
	nfsattr_inval(bdp);	/* mod time changed */
	if (!error) {
		error = geterrno(status);
		check_stale_fh(error, bdp);
	}
	return (error);
}

/*
 * Read directory entries.
 * There are some weird things to look out for here.  The uio_offset
 * field is either 0 or it is the offset returned from a previous
 * readdir.  It is an opaque value used by the server to find the
 * correct directory block to read.  The byte count must be at least
 * vtoblksz(vp) bytes.  The count field is the number of blocks to
 * read on the server.  This is advisory only, the server may return
 * only one block's worth of entries.  Entries may be compressed on
 * the server.
 */
static int
nfs_readdir(bdp, uiop, cred, eofp)
	bhv_desc_t *bdp;
	register struct uio *uiop;
	struct cred *cred;
	int *eofp;
{
	int error = 0;
	unsigned count;
	struct nfsrddirargs rda;
	struct nfsrddirres  rd;
	struct rnode *rp;

	rp = bhvtor(bdp);
	count = uiop->uio_resid;
	count = MIN(count, rtomi(rp)->mi_tsize);
	rda.rda_offset = uiop->uio_offset;
	rda.rda_fh = *bhvtofh(bdp);
	rd.rd_size = count;
	rd.rd_entries = kmem_alloc(count, KM_SLEEP);

	/* We need to format the dirent entries differently based on
	 * the abi of the caller, if the read is going to user space.
	 * This is because dirent structs are different between the
	 * 32 bit abi and the 64 bit abi.
	 * Pass sufficient information to xdr_getdirres to make
	 * this determination.
	 *
	 * Also determine the byte count for the request.  If we are using the
	 * 64-bit ABI, we will need to reduce the byte count sent to the
	 * server to 1/2 of what we are able to accept.  The reason for this
	 * is that when we receive the data, we will expand each off and ino in
	 * the dirent structure to 8 bytes.  The maximal expansion, then is
	 * 12 bytes for every 12 bytes (12 bytes being the minimum dirent
	 * size including the null-terminated entry name), since the
	 * minimum 64-bit ABI dirent size is 24 bytes given alignment
	 * constraints.
	 * If we are using the 32-bit ABI, just set the argument count to be
	 * the same as the results count.
	 */
	rd.rd_abi = GETDENTS_ABI(get_current_abi(), uiop);
	rd.rd_segflg = uiop->uio_segflg;

	if (ABI_IS_IRIX5_64(rd.rd_abi) || ABI_IS_IRIX5_N32(rd.rd_abi)) {
	        rda.rda_count = count / 2;
	} else {
	        rda.rda_count = count;
        }

	error = rfscall(rtomi(rp), RFS_READDIR, xdr_rddirargs, (caddr_t)&rda,
			xdr_getrddirres, (caddr_t)&rd, cred, 0);
	if (!error) {
		error = geterrno(rd.rd_status);
		check_stale_fh(error, bdp);
	}
	if (!error) {
		/*
		 * move dir entries to user land
		 */
		if (rd.rd_size) {
			error = uiomove((caddr_t)rd.rd_entries,
			    (int)rd.rd_size, UIO_READ, uiop);
			uiop->uio_offset = rd.rd_offset;
		}
		*eofp = rd.rd_eof;
	}
	kmem_free(rd.rd_entries, count);
	return (error);
}

/*
 * Convert from file system blocks to device blocks
 */
/* ARGSUSED */
static int
nfs_bmap(bdp, offset, count, flag, cred, bmv, nbmv)
	bhv_desc_t *bdp;	/* file's base pvnode */
	off_t offset;		/* request offset and count */
	ssize_t count;
	int flag;		/* read/write/bmv flag */
	struct cred *cred;
	struct bmapval *bmv;	/* first bmapval mapping */
	int *nbmv;		/* number of bmapvals */
{
	register struct bmapval *rabmv;
	register int bsize;		/* server's block size in bytes */
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register struct rnode *rp = bhvtor(bdp);
	register off_t size;
	register int n = *nbmv;

	ASSERT(n > 0);
	bsize = rtoblksz(rp);

	bmv->length = BTOBBT(bsize);
	bmv->bsize = bsize;
	bmv->bn = BTOBBT(offset / bsize * bsize);
	bmv->offset = bmv->bn;
	bmv->pmp = NULL;
	size = BBTOB(bmv->bn) + bsize;
	bmv->pboff = offset % bsize;
	bmv->pbsize = MIN(bsize - bmv->pboff, count);
	bmv->pbdev = vp->v_rdev;
	bmv->eof = 0;

	while (--n > 0) {
		rabmv = bmv + 1;

		rabmv->length = bmv->length;
		rabmv->bn = bmv->bn + bmv->length;
		rabmv->offset = rabmv->bn;
		rabmv->pmp = NULL;
		rabmv->pbsize = rabmv->bsize = bsize;
		rabmv->pboff = 0;
		rabmv->pbdev = vp->v_rdev;
		rabmv->eof = 0;
		size += bsize;

		bmv++;
	}

	if (rp->r_localsize < size)
		rp->r_localsize = size;

	return (0);
}

struct buf *async_bufhead;
#ifdef OS_METER
static int async_daemon_ready;		/* number of async biod's ready */
#endif

/*
 * buf.h compatibility
 */
#define	b_actf	av_forw

/*
 * Async daemon buffer queue limiting.
 */
#define	ASYNC_MAXBUFS(nbiods)	((5 * (nbiods)) / 4)

int	async_bufcount;		/* number of buffers on async_bufhead list */
int	async_maxbufs;		/* async_bufhead maximum list length */

lock_t	async_lock;		/* lock for async_* variable use */
sv_t	async_wait;		/* synchronization variable */


static void
nfs_strategy(bdp, bp)
	bhv_desc_t *bdp;
	struct buf *bp;
{
	struct rnode *rp = bhvtor(bdp);
	struct buf *bp1;
	cred_t *cred;
	int s;

	/*
	 * If there was an asynchronous write error on this rnode
	 * then we just return the old error code.  This continues
	 * until the rnode goes away (zero ref count).
	 */
	if (rp->r_error) {
		bp->b_error = rp->r_error;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}

	bp_mapin(bp);

	s = mutex_bitlock(&rp->r_flags, RLOCKED);
	rp->r_iocount++;
	mutex_bitunlock(&rp->r_flags, RLOCKED, s);

	s = mp_mutex_spinlock(&async_lock);
	if (bp->b_flags & B_ASYNC && async_daemon_count &&
	    (bp->b_flags & B_BDFLUSH || async_bufcount < async_maxbufs)) {
		if (async_bufhead) {
			bp1 = async_bufhead;
			while (bp1->b_actf) {
				bp1 = bp1->b_actf;
			}
			bp1->b_actf = bp;
		} else {
			async_bufhead = bp;
		}
		bp->b_actf = NULL;
		/*
		 * Stuff the behavior description pointer in the
		 * b_private field so that we can retrieve it int
		 * the daemon code.  Credential, too.
		 */
		bp->b_private = bdp;
		cred = get_current_cred();
		crhold(cred);
		bp->b_fsprivate = (void *)cred;
		async_bufcount++;
		sv_signal(&async_wait);
		mp_mutex_spinunlock(&async_lock, s);
	} else {
		/*
		 * do_bio calls riodone to finish i/o.
		 */
		mp_mutex_spinunlock(&async_lock, s);
		(void)do_bio(bp, rp, get_current_cred(), 0);
	}
	return;
}


/*
 * Drop the rnode's i/o count and if it goes to zero, wakeup anyone
 * interested in flushing i/o.  Unlock the rnode in any case.
 */
static void
riodone(rp)
	struct rnode *rp;
{
	int s;

	ASSERT(rnode_is_locked(rp));
	ASSERT(rp->r_iocount != 0);
	s = mutex_bitlock(&rp->r_flags, RLOCKED);
	if (--rp->r_iocount == 0) {
		sv_broadcast(&rp->r_iowait);
	}
	mutex_bitunlock(&rp->r_flags, RLOCKED, s);
	runlock(rp);
}

/*
 * Flush all async and delayed i/o for vp.  Loop over the async daemon
 * buffer queue waiting for async writes and (conditionally) readaheads
 * to complete, and unlinking the buffers.  (It would be more efficient
 * to mark the readahead buffers invalid.)  Then push out delayed writes
 * with VOP_FLUSH_PAGES and clear the rnode dirty bit.
 */
static int
flush_bio(bdp, readaheads)
	bhv_desc_t *bdp;
	int readaheads;
{
	vnode_t *vp;
	struct rnode *rp;
	struct buf *bp, **bpp;
	cred_t *cred;
	int s;
	/* REFERENCED */
	int error;

	rp = bhvtor(bdp);
	vp = BHV_TO_VNODE(bdp);
	ASSERT(rnode_is_rwlocked(rp) && (get_thread_id() == rp->r_rwlockid));

	/*
	 * Necessary to flush r_localsize, even if it is past r_size,
	 * just to keep assertions about !VN_DIRTY quiet.
	 */
	VOP_FLUSH_PAGES(vp, (off_t)0, rp->r_localsize - 1, B_ASYNC, FI_NONE, error);

start:
	bpp = &async_bufhead;
	s = mp_mutex_spinlock(&async_lock);
	while ((bp = *bpp) != 0) {
		ASSERT(bp->b_flags & B_BUSY);
		ASSERT(bp->b_flags & B_ASYNC);
		if (bp->b_vp == vp
		    && (readaheads || !(bp->b_flags & B_READ))) {
			*bpp = bp->av_forw;
			async_bufcount--;
			mp_mutex_spinunlock(&async_lock, s);
			cred = (cred_t *)bp->b_fsprivate;
			ASSERT(cred);
			bp->b_fsprivate = NULL;
			/*
			 * The only time do_bio will return an error is when
			 * rlock_nohang returns an error.
			 */
			error = do_bio(bp, rp, cred, 0);
			if (error) {
				crfree(cred);
				return error;
			}
			crfree(cred);
			goto start;
		}
		bpp = &bp->av_forw;
	}
	mp_mutex_spinunlock(&async_lock, s);

	s = mutex_bitlock(&rp->r_flags, RLOCKED);
	if (rp->r_iocount != 0) {
		sv_bitlock_wait(&rp->r_iowait, PRIBIO,
				&rp->r_flags, RLOCKED, s);
		ASSERT(rp->r_iocount == 0);
	} else
		mutex_bitunlock(&rp->r_flags, RLOCKED, s);

	rclrflag(rp, RDIRTY);
	return 0;
}

/*
 * Sun uses async_daemon_count to keep track of how many biods are
 * sleeping waiting for work.  If this count is non-zero, nfs_strategy
 * will enqueue async i/o for the biods to finish.  If biods aren't
 * scheduled in a timely fashion, the queue may fill with every buffer
 * in the system before async_daemon_count drops to zero.  Therefore
 * we redefine async_daemon_count to be the absolute number of biods,
 * sleeping or running, and use a formulaic queue limit heuristic.
 */
async_daemon()
{
	struct buf *bp;
	struct rnode *rp;
	int s;
	bhv_desc_t *bdp;
	int error;
	cred_t *cred;
	rval_t rvp;

	/*
	 * There's no real harm in allowing unprivileged users
	 * to invoke this, but then again, they shouldn't have to.
	 */
	if (mac_enabled && !_CAP_ABLE(CAP_MOUNT_MGT))
		return EPERM;

	/* First, release resources */
	if (error = vrelvm())
		return error;

	/*
	 * Set scheduler policy and priority
	 */
	VPROC_SCHED_SETSCHEDULER(curvprocp, 21, SCHED_TS, &rvp.r_val1, &error);
	if (error) {
	     printf("Unable to set biod scheduler priority/policy: error=%d\n",                error);
	}

	s = mp_mutex_spinlock(&async_lock);
	async_daemon_count++;
	async_maxbufs = ASYNC_MAXBUFS(async_daemon_count);

	for (;;) {
		while (async_bufhead == NULL) {
			if (mp_sv_wait_sig(&async_wait, PZERO+1,
					   &async_lock, s))
				goto bail_out;

			s = mp_mutex_spinlock(&async_lock);
			METER(async_daemon_ready++);
		}
		METER(async_daemon_ready--);
		bp = async_bufhead;
		async_bufhead = bp->b_actf;
		--async_bufcount;
		ASSERT(async_bufcount >= 0);
		mp_mutex_spinunlock(&async_lock, s);
		bdp = (bhv_desc_t *)bp->b_private;
		bp->b_private = NULL;
		cred = (cred_t *)bp->b_fsprivate;
		ASSERT(cred);
		bp->b_fsprivate = NULL;
		rp = bhvtor(bdp);
		ASSERT(rp->r_cred);
		(void)do_bio(bp, rp, cred, 1);
		crfree(cred);
		s = mp_mutex_spinlock(&async_lock);
	}

bail_out:
	s = mp_mutex_spinlock(&async_lock);
	if (async_daemon_count == 0) {
		/*
		 * We already were processing requests below
		 * and we were signaled again.  So this time,
		 * just give up and abort all the requests.
		 */
		while ((bp = async_bufhead) != NULL) {
			async_bufhead = bp->b_actf;
			mp_mutex_spinunlock(&async_lock, s);
			/*
			 * Since we are always ASYNC iodone
			 * will free the buf.
			 */
			bp->b_flags |= B_ERROR;
			bp->b_private = NULL;
			cred = (cred_t *)bp->b_fsprivate;
			bp->b_fsprivate = NULL;
			ASSERT(cred);
			crfree(cred);
			biodone(bp);
			s = mp_mutex_spinlock(&async_lock);
		}
	} else {
		async_daemon_count--;
		METER(async_daemon_ready--);
		/*
		 * If we were the last async daemon,
		 * process all the queued requests.
		 */
		if (async_daemon_count == 0) {
		    while ((bp = async_bufhead) != NULL) {
			async_bufhead = bp->b_actf;
			mp_mutex_spinunlock(&async_lock, s);
			/*
			 * Since we are ASYNC do_bio
			 * will free the bp.
			 */
			bdp = (bhv_desc_t *)bp->b_private;
			cred = (cred_t *)bp->b_fsprivate;
			bp->b_fsprivate = NULL;
			ASSERT(cred);
			rp = bhvtor(bdp);
			(void)do_bio(bp, rp, cred, 1);
			crfree(cred);
			s = mp_mutex_spinlock(&async_lock);
		    }
		}
	}
	mp_mutex_spinunlock(&async_lock, s);
	exit(CLD_EXITED, 0);
	return 0;
}

int parallel_writes = 1;

int
do_bio(bp, rp, cr, async)
	struct buf *bp;
	struct rnode *rp;
	cred_t *cr;
	int async;
{
	struct cred *cred;
	int resid;
	int error;
	int s;

	if (async && parallel_writes) {
		mraccess(&rp->r_otw_attr_lock);
		mutex_enter(&rp->r_statelock);
	} else if (error = rlock_nohang(rp)) {
		bp->b_error = error;
		bp->b_flags |= B_ERROR;

		s = mutex_bitlock(&rp->r_flags, RLOCKED);
		if (--rp->r_iocount == 0) {
		    sv_broadcast(&rp->r_iowait);
		}
		mutex_bitunlock(&rp->r_flags, RLOCKED, s);

		biodone(bp);
		return(error);
	}
	/*
	 * We must hold r_cred before passing it to nfsread or nfswrite,
	 * because they may sleep and another write may free the credentials
	 * that we passed to them, and change r_cred.
	 */
	if (rp->r_cred != NULL) {
		cred = rp->r_cred;
		crhold(cred);
	} else {
		rp->r_cred = cr;
		crhold(cr);
		cred = cr;
		crhold(cred);
	}
	if (async) {
		mutex_exit(&rp->r_statelock);
	}
	if (bp->b_flags & B_READ) {
	read_again:
		error = nfsread(rtobhv(rp), bp->b_un.b_addr, BBTOB(bp->b_blkno),
			    bp->b_bcount, &resid, cred, async);
		if (!error && resid) {
			/*
			 * Didn't get it all because we hit EOF, clear bp's
			 * memory beyond the EOF so that holes in files read
			 * as zeroes.
			 */
			bzero(bp->b_un.b_addr + bp->b_bcount - resid,
			      (u_int)resid);
		} else if (error == EACCES && cred != cr) {
			crfree(rp->r_cred);
			rp->r_cred = cr;
			crhold(rp->r_cred);
			crfree(cred);
			cred = cr;
			crhold(cred);
			goto read_again;
		}
		bp->b_error = error;
	} else {
		u_int offset, diff;
		/*
		 * If the write fails and it was asynchronous, all future
		 * writes will get the error.
		 */
	write_again:
		if (rp->r_error) {
			bp->b_error = rp->r_error;
		} else if ((offset = BBTOB(bp->b_blkno)) < rp->r_size) {

			diff = rp->r_size - offset;	/* avoid sample skew */
			resid = MIN(bp->b_bcount, diff);
			ASSERT(resid >= 0);
			error = nfswrite(rtobhv(rp), bp->b_un.b_addr, offset,
					       resid, cred, async, bp);
			if (error == EACCES && cred != cr) {
				crfree(rp->r_cred);
				rp->r_cred = cr;
				crhold(rp->r_cred);
				crfree(cred);
				cred = cr;
				crhold(cred);
				goto write_again;
			}
			bp->b_error = error;
			if (bp->b_error && error != EINTR &&
			   (bp->b_flags & B_ASYNC))
				rp->r_error = (short) error;
		} else
			bp->b_error = 0;
	}

	if (bp->b_error) {
		bp->b_flags |= B_ERROR;
	}
	crfree(cred);
	biodone(bp);
	riodone(rp);
	return(0);
}

/* ARGSUSED */
static int
nfs_map(
	bhv_desc_t *bdp,
	off_t off,
	size_t len,
	mprot_t prot,
	u_int flags,
	struct cred *cr,
	vnode_t **nvp)
{
	if ((off + (off_t)len) > (off_t)NFS_MAX_FILE_OFFSET)
		return EINVAL;
	return 0;
}

static int
nfs_cleanlocks(bhv_desc_t *bdp, pid_t pid, long sysid, cred_t *cr)
{
	u_int oldsig;
	uthread_t *ut = curuthread;
	k_sigset_t oldmask;
	k_sigset_t newmask;
	int s;
	struct cred *cred;
	struct flock ld;
	int error;

	NLM_DEBUG(NLMDEBUG_CLEAN,
		printf("nfs_frlock: cleaning locks for process %d\n", pid));
	cred = crdup(cr);
	ld.l_pid = pid;
	ld.l_sysid = sysid;
	ld.l_type = F_UNLCK;
	ld.l_whence = 0;
	ld.l_start = 0;
	ld.l_len = 0;
	/*
	 * We cannot allow signals here.
	 * Mask off everything but those
	 * which cannot be masked and
	 * restore the mask after the call.
	 */
	sigfillset(&newmask);
	sigdiffset(&newmask, &cantmask);
	s = ut_lock(ut);
	oldmask = *ut->ut_sighold;
	*ut->ut_sighold = newmask;
	oldsig = ut->ut_cursig;
	ut->ut_cursig = 0;
	ut_unlock(ut, s);

	error = nfs_lockctl(bdp, &ld, F_SETLK, cred);

	s = ut_lock(ut);
	*ut->ut_sighold = oldmask;
	ut->ut_cursig = oldsig;
	ut_unlock(ut, s);
	crfree(cred);
	if (error) {
		rsetflag(bhvtor(bdp), RCLEANLOCKS);
	}
	return(error);
}

static int
nfs_frlock(
	bhv_desc_t *bdp,
	int cmd,
	struct flock *lfp,
	int flag,
	off_t offset,
	vrwlock_t vrwlock,
	cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	rnode_t *rp = bhvtor(bdp);
	int error = 0;
	/*REFERENCED*/
	int err = 0;
	int dolock;
	flock_t fl_save;

	if (cmd == F_CLNLK || cmd == F_CHKLK || cmd == F_CHKLKW) {
		if (cmd == F_CLNLK) {
		        /*
			 * Clean remote first, then clean local by falling
			 * through.
			 */
			if (!(rtomi(rp)->mi_flags & MIF_PRIVATE) &&
				(haslocks(vp, lfp->l_pid, lfp->l_sysid) ||
				(rp->r_flags & RCLEANLOCKS))) {
					error = nfs_cleanlocks(bdp, lfp->l_pid,
						lfp->l_sysid, cr);
			}
			/* fall through to fs_frlock */
		}

		dolock = (vrwlock == VRWLOCK_NONE);
		if (dolock) {
			nfs_rwlock(bdp, VRWLOCK_WRITE);
			vrwlock = VRWLOCK_WRITE;
		}

		error = fs_frlock(bdp, cmd, lfp, flag, offset, vrwlock, cr);
		if (dolock)
			nfs_rwunlock(bdp, VRWLOCK_WRITE);

		return(error);
	}

	if (cmd != F_GETLK && cmd != F_SETLK && cmd != F_SETLKW &&
	    cmd != F_SETBSDLK && cmd != F_SETBSDLKW)
		return EINVAL;

	/*
	 * If file is being mapped, disallow mandatory lock.
	 */
	if (rtomi(rp)->mi_flags & MIF_PRIVATE) {
		ASSERT(vrwlock == VRWLOCK_NONE);
		nfs_rwlock(bdp, VRWLOCK_WRITE);
		/*
		 * Check that we're not trying to lock beyond the 32 bit
		 * limit before handing this off to fs_frlock().
		 */
		error = convoff(vp, lfp, lfp->l_whence, offset, SEEKLIMIT32, cr);
		if (!error) {
			error = fs_frlock(bdp, cmd, lfp, flag, offset,
					  VRWLOCK_WRITE, cr);
		}
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
	} else {
		error = convoff(vp, lfp, 0, offset, SEEKLIMIT32, cr);
		if (!error) {
			ASSERT(lfp->l_sysid == 0L);
			/*
			 * issue the remote lock request
			 * if there is no error, record the lock in the vnode
			 * if that fails, issue an unlock request
			 */
			if (lfp->l_type == F_UNLCK) {
					error = nfs_reclock(vp, lfp, cmd, flag, cr);
					if (!error) {
						error = nfs_lockctl(bdp, lfp, cmd, cr);
					}
			} else {
				fl_save = *lfp;
				error = nfs_lockctl(bdp, lfp, cmd, cr);
				if (!error && (cmd != F_GETLK)) {
					/*
					 * Record the lock in the vnode.  nfs_reclock
					 * calls reclock which will only return
					 * an error when the lock could not be
					 * recorded.  Thus, if there is a lock
					 * record for this lock, there must have
					 * been a previous identical request.
					 */
					error = nfs_reclock(vp, &fl_save, cmd, flag, cr);
					if (error) {
						fl_save.l_type = F_UNLCK;
						err = nfs_lockctl(bdp, &fl_save, cmd, cr);
						NLM_DEBUG(NLMDEBUG_ERROR,
								printf("nfs_frlock: unable to release "
								"lock after error, error %d\n",
								err));
					}
				}
			}
			if (error) {
				rsetflag(rp, RCLEANLOCKS);
			}
		}
	}
	return error;
}

/*
 * Record-locking requests are passed to the local Lock-Manager daemon.
 */
int
nfs_lockctl(bdp, lfp, cmd, cred)
	bhv_desc_t *bdp;
	struct flock *lfp;
	int cmd;
	struct cred *cred;
{
	int islock;
	int error = 0;
	lockhandle_t lh;
	vnode_t *vp;
	rnode_t *rp = bhvtor(bdp);

	vp = BHV_TO_VNODE(bdp);
	lh.lh_vp = vp;
	lh.lh_servername = rtomi(rp)->mi_hostname;
	lh.lh_timeo = rtomi(rp)->mi_timeo;
	lh.lh_id.n_bytes = (char *)bhvtofh(bdp);
	lh.lh_id.n_len = sizeof (fhandle_t);
	islock = (lfp->l_type != F_UNLCK) && (cmd != F_GETLK);
	/* 
	 * unlocks sync and invalidate the attributes before releasing
	 * the lock
	 * locks do this after acquisition
	 */
	if (!islock) {
		VOP_FSYNC(vp, FSYNC_INVAL, cred, (off_t)0, (off_t)-1, error);
		/*
		 * On each unlock request, invalidate the attributes to
		 * ensure a call to the server on the next attribute check.
		 * If the file is mapped, also check the attributes
		 * here.
		 */
		nfsattr_inval(bdp);
		if (VN_MAPPED(vp)) {
			(void)nfs_getattr(bdp, 0, 0, cred);
		}
	}
	error = klm_lockctl(&lh, lfp, cmd, cred);
	switch (error) {
		case 0:
			if (islock) {
				VOP_FSYNC(vp, FSYNC_INVAL, cred, (off_t)0,
					(off_t)-1, error);
				/*
				 * On each successful lock request, invalidate
				 * the attributes to ensure a call to the
				 * server on the next check.  If the file is
				 * mapped, also check the attributes here.
				 * Do not do invalidation for unlocks.
				 */
				nfsattr_inval(bdp);
				if (VN_MAPPED(vp)) {
					(void)nfs_getattr(bdp, 0, 0, cred);
				}
			}
			break;
		case ENOPKG:
			error = ENOLCK;
			break;
		default:
			break;
	}
	return (error);
}

/* ARGSUSED1 */
static int
nfs_reclaim(bdp, flag)
	bhv_desc_t *bdp;
	int flag;
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct rnode *rp;

	ASSERT(!VN_MAPPED(vp));
	rp = bhvtor(bdp);
	switch (vp->v_type) {
	  case VREG:
		VOP_FLUSHINVAL_PAGES(vp, 0, rp->r_localsize - 1, FI_NONE);
		ASSERT(!VN_DIRTY(vp) && vp->v_pgcnt == 0);
		rp->r_localsize = 0;
		break;
	  case VLNK:
		if (rp->r_symval) {
			kmem_free(rp->r_symval, rp->r_symlen);
			rp->r_symval = NULL;
		}
		break;
	}
	dnlc_purge_vp(vp);
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
	rdestroy(rp, rtomi(rp));
	return 0;
}


#define BDS_GET_FCNTL(x) \
	bds_get_fcntl(rp, mi, cmd, arg, &x, sizeof(x), offset, cr);

static int
bds_get_fcntl(
	rnode_t		*rp,
	mntinfo_t	*mi,
	int		cmd,
	void		*arg,
	void		*cmdstruct,
	int		cmdstructsz,
	off_t		offset,
	cred_t		*cr)
{
	int error;
	
	if ((mi->mi_flags & MIF_BDS) == 0 || bds_vers(rp, 2) < 2)
		return EINVAL;
	
	error = bds_fcntl(rp, cmd, cmdstruct, offset, cr);
	
	if (error)
		return error;
	if (copyout(cmdstruct, arg, cmdstructsz))
		return EFAULT;

	return 0;
}

#define BDS_SET_FCNTL(x) \
	bds_set_fcntl(rp, mi, cmd, arg, &x, sizeof(x), offset, cr);

static int
bds_set_fcntl(
	rnode_t		*rp,
	mntinfo_t	*mi,
	int		cmd,
	void		*arg,
	void		*cmdstruct,
	int		cmdstructsz,
	off_t		offset,
	cred_t		*cr)
{
	int error;
	
	if ((mi->mi_flags & MIF_BDS) == 0 || bds_vers(rp, 2) < 2)
		return EINVAL;

	if (copyin(arg, cmdstruct, cmdstructsz))
		return EFAULT;
	
	error = bds_fcntl(rp, cmd, cmdstruct, offset, cr);
	return error;
}

/* ARGSUSED */
int
nfs_fcntl(
	bhv_desc_t *bdp,
	int	cmd,
	void	*arg,
	int	flags,
	off_t	offset,
	cred_t	*cr,
	rval_t 	*rvp)
{
	int     		error = 0;
	struct flock            bf;
	struct irix5_flock      i5_bf;
	char abi = get_current_abi();
	vnode_t *vp = BHV_TO_VNODE(bdp);
	rnode_t	*rp = bhvtor(bdp);
	mntinfo_t *mi = vn_bhvtomi(bdp);

	switch (cmd) {
#ifdef DATAPIPE
	case F_GETOPS:
		fspe_get_ops(arg);
		break;
#endif
	case F_ALLOCSP:
	case F_FREESP:
		
	case F_RESVSP:
	case F_UNRESVSP:
		
	case F_ALLOCSP64:
	case F_FREESP64:
		
	case F_RESVSP64:
	case F_UNRESVSP64:
		if ((flags & FWRITE) == 0) {
			error = EBADF;
		} else if (vp->v_type != VREG) {
			error = EINVAL;
#if _MIPS_SIM == _ABI64
		} else if (ABI_IS_IRIX5_64(abi)) {
			if (copyin((caddr_t)arg, &bf, sizeof bf)) {
				error = EFAULT;
				break;
			}
#endif
		} else if (cmd == F_ALLOCSP64 || cmd == F_FREESP64   ||
			   cmd == F_RESVSP64  || cmd == F_UNRESVSP64 ||
			   ABI_IS_IRIX5_N32(abi)) {
			/*
			 * The n32 flock structure is the same size as the
		 	 * o32 flock64 structure. So the copyin_xlate
			 * with irix5_n32_to_flock works here.
			 */
			if (COPYIN_XLATE((caddr_t)arg, &bf, sizeof bf,
				irix5_n32_to_flock,
				abi, 1)) {
				error = EFAULT;
				break;
			}
		} else {
			if (copyin((caddr_t)arg, &i5_bf, sizeof i5_bf)) {
				error = EFAULT;
				break;
			}
			/*
			 * Now expand to 64 bit sizes.
			 */
			bf.l_type = i5_bf.l_type;
			bf.l_whence = i5_bf.l_whence;
			bf.l_start = i5_bf.l_start;
			bf.l_len = i5_bf.l_len;
		}
		if ((mi->mi_flags & MIF_BDS) && bds_vers(rp, 2) >= 2) {
			error = bds_fcntl(bhvtor(bdp), cmd, &bf, offset, cr);
			break;
		}
		if (cmd == F_RESVSP || cmd == F_UNRESVSP ||
		    cmd == F_RESVSP64 || cmd == F_UNRESVSP64)
		{
			error = EINVAL;
			break;
		}
		if ((error = convoff(vp, &bf, 0, offset, SEEKLIMIT32, cr)) == 0) {
			struct vattr vattr;
			vattr.va_size = bf.l_start;
			vattr.va_mask = AT_SIZE;
			error = nfs_setattr(bdp, &vattr, 0, cr);
		}
		break;
	case F_BDS_FSOPERATIONS + XFS_GROWFS_DATA: {
		xfs_growfs_data_t grow;

		error = BDS_SET_FCNTL(grow);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GROWFS_LOG: {
		xfs_growfs_log_t grow;

		error = BDS_SET_FCNTL(grow);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GROWFS_RT: {
		xfs_growfs_rt_t grow;

		error = BDS_SET_FCNTL(grow);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_COUNTS: {
		xfs_fsop_counts_t counts;

		error = BDS_GET_FCNTL(counts);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_SET_RESBLKS: {
		xfs_fsops_getblks_t getblks;

		if ((mi->mi_flags & MIF_BDS) == 0 || bds_vers(rp, 2) < 2) {
			error = EINVAL;
			break;
		}
		if (copyin(arg, &getblks, sizeof(getblks))) {
			error = EFAULT;
			break;
		}
		error = bds_fcntl(rp, cmd, &getblks, offset, cr);

		if (error == 0 && copyout(&getblks, arg, sizeof(getblks)))
			error = EFAULT;
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GET_RESBLKS: {
		xfs_fsops_getblks_t getblks;

		error = BDS_GET_FCNTL(getblks);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY_V1: {
		xfs_fsop_geom_v1_t geom;

		error = BDS_GET_FCNTL(geom);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY_V2: {
		xfs_fsop_geom_v2_t geom;

		error = BDS_GET_FCNTL(geom);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY: {
		xfs_fsop_geom_t geom;

		error = BDS_GET_FCNTL(geom);
		break;
	}
	case F_FSGETXATTR:
	case F_FSGETXATTRA: {
		struct fsxattr fa;

		error = BDS_GET_FCNTL(fa);
		break;
	}
	case F_FSSETXATTR: {
		struct fsxattr fa;

		error = BDS_SET_FCNTL(fa);
		break;
	}
	case F_DIOINFO: {
		struct dioattr	da;
		int vers;
		
		if ((mi->mi_flags & MIF_BDS) == 0 || (vers = bds_vers(rp, 2)) == -1) {
			error = EINVAL;
			break;
		}
		if (vers == 0)
			da.d_miniosz = 4096; /* BDS 1.0 */
		else
			da.d_miniosz = 1;
		da.d_mem     = _PAGESZ;
		da.d_maxiosz = mi->mi_bds_maxiosz;

		if (copyout(&da, arg, sizeof(da))) {
			error = EFAULT;
		}
		break;
	}
	case F_GETBDSATTR: {
		bdsattr_t ba;

		error = BDS_GET_FCNTL(ba);
		break;
	}
	case F_SETBDSATTR: {
		bdsattr_t ba;

		error = BDS_SET_FCNTL(ba);
		break;
	}
	default:
		error = EINVAL;
	}

	return error;
}

vnodeops_t nfs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	nfs_open,
	nfs_close,
	nfs_read,
	nfs_write,
	nfs_ioctl,
	nfs_setfl,
	nfs_getattr,
	nfs_setattr,
	nfs_access,
	nfs_lookup,
	nfs_create,
	nfs_remove,
	nfs_link,
	nfs_rename,
	nfs_mkdir,
	nfs_rmdir,
	nfs_readdir,
	nfs_symlink,
	nfs_readlink,
	nfs_fsync,
	nfs_inactive,
	nfs_fid,
	nfs_fid2,
	nfs_rwlock,
	nfs_rwunlock,
	nfs_seek,
	fs_cmp,
	nfs_frlock,
	(vop_realvp_t)fs_nosys,
	nfs_bmap,
	nfs_strategy,
	nfs_map,
	(vop_addmap_t)fs_noerr,
	(vop_delmap_t)fs_noerr,
	fs_poll,
	(vop_dump_t)fs_nosys,
	fs_pathconf,
	(vop_allocstore_t)fs_nosys,
	nfs_fcntl,	/* fcntl */
	nfs_reclaim,
	(vop_attr_get_t)fs_nosys,
	(vop_attr_set_t)fs_nosys,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	fs_cover,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	fs_tosspages,
	fs_flushinval_pages,
	fs_flush_pages,
	fs_invalfree_pages,
	fs_pages_sethole,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};

#ifdef DEBUG_NOHANG
int fake_rlock_nohang(rnode_t *rp)
{
    rlock(rp);
    return 0;
}

static int fake_nfs_rwlock_nohang(bhv_desc_t *bdp, vrwlock_t flags)
{
    nfs_rwlock(bdp, flags);
    return 0;
}
#endif
