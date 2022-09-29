/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_vnops.c 1.161     94/04/21 SMI"
 */

#ident	"$Revision: 1.129 $"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <ksys/vfile.h>
#include <sys/filio.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/dirent.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/mount.h>
#include <sys/dnlc.h>
#include <sys/kabi.h>
#include <ksys/vproc.h>

#include "cachefs_fs.h"
#include <sys/fs_subr.h>
#include <string.h>
#include <fs/specfs/spec_lsnode.h>
#include <sys/pfdat.h>
#include <sys/kthread.h>
#include <sys/uthread.h>

int  cachefs_open(bhv_desc_t *, vnode_t **, mode_t, cred_t *);
int  cachefs_close(bhv_desc_t *, int, lastclose_t, cred_t *);
int  cachefs_read(bhv_desc_t *, struct uio *, int, cred_t *, flid_t *);
int  cachefs_write(bhv_desc_t *, struct uio *, int, cred_t *, flid_t *);
int  cachefs_getattr(bhv_desc_t *, struct vattr *, int, cred_t *);
int  cachefs_setattr(bhv_desc_t *, struct vattr *, int, cred_t *);
int  cachefs_access(bhv_desc_t *, int, cred_t *);
int  cachefs_lookup(bhv_desc_t *, char *, vnode_t **,
		    struct pathname *, int, struct vnode *, cred_t *);
int  cachefs_create(bhv_desc_t *, char *, struct vattr *,
		    int, int, struct vnode **, cred_t *);
int  cachefs_remove(bhv_desc_t *, char *, cred_t *);
int  cachefs_link(bhv_desc_t *, struct vnode *, char *, cred_t *);
int  cachefs_rename(bhv_desc_t *, char *, struct vnode *, char *,
		    struct pathname *, cred_t *);
int  cachefs_mkdir(bhv_desc_t *, char *, register struct vattr *,
		   struct vnode **, cred_t *);
int  cachefs_rmdir(bhv_desc_t *, char *, struct vnode *, cred_t *);
int  cachefs_readdir(bhv_desc_t *, register struct uio *,
		     cred_t *, int *);
int  cachefs_symlink(bhv_desc_t *, char *, struct vattr *, char *, cred_t *);
int  cachefs_readlink(bhv_desc_t *, struct uio *, cred_t *);
int  cachefs_fsync(bhv_desc_t *, int, cred_t *, off_t, off_t);
int  cachefs_fid(bhv_desc_t *, struct fid **);
int  cachefs_fid2(bhv_desc_t *, struct fid *);
void cachefs_rwlock(bhv_desc_t *, vrwlock_t);
void cachefs_rwunlock(bhv_desc_t *, vrwlock_t);
int  cachefs_seek(bhv_desc_t *, off_t, off_t *);
int  cachefs_frlock(bhv_desc_t *, int, struct flock *,
		    int, off_t, vrwlock_t, cred_t *);
int  cachefs_realvp(bhv_desc_t *, struct vnode **);
int  cachefs_map(bhv_desc_t *, off_t, size_t, mprot_t, u_int, cred_t *,
		 vnode_t **);
#if 0
int  cachefs_addmap(bhv_desc_t *, off_t, void *, addr_t,
			   size_t, u_int, u_int, u_int, cred_t *);
int  cachefs_delmap(bhv_desc_t *, off_t, void *, addr_t,
			   size_t, u_int, u_int, u_int, cred_t *);
#endif

int  cachefs_dump(bhv_desc_t *, caddr_t, daddr_t, u_int);
int  cachefs_bmap(bhv_desc_t *, off_t, ssize_t, int, struct cred *,
		  struct bmapval *, int *);
void cachefs_strategy(bhv_desc_t *, struct buf *);
int  cachefs_reclaim(bhv_desc_t *, int);
int  cachefs_inactive(bhv_desc_t *vp, cred_t *cr);
int  cachefs_fcntl(bhv_desc_t *vp, int cmd, void *arg, int flags,
		   off_t offset, cred_t  *cr, rval_t  *rvp);
void cachefs_vnode_change(bhv_desc_t *, vchange_t, __psint_t);
int  cachefs_attr_get(bhv_desc_t *, char *, char *, int *, int, cred_t *);
int  cachefs_attr_set(bhv_desc_t *, char *, char *, int, int, cred_t *);
int  cachefs_attr_remove(bhv_desc_t *, char *, int, cred_t *);
int  cachefs_attr_list(bhv_desc_t *, char *, int, int,
		       struct attrlist_cursor_kern *, cred_t *);

vnodeops_t cachefs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	cachefs_open,
	cachefs_close,
	cachefs_read,
	cachefs_write,
	(vop_ioctl_t)fs_nosys,
	fs_setfl,
	cachefs_getattr,
	cachefs_setattr,
	cachefs_access,
	cachefs_lookup,
	cachefs_create,
	cachefs_remove,
	cachefs_link,
	cachefs_rename,
	cachefs_mkdir,
	cachefs_rmdir,
	cachefs_readdir,
	cachefs_symlink,
	cachefs_readlink,
	cachefs_fsync,
	cachefs_inactive,	/* VOP_INACTIVE */
	cachefs_fid,
	cachefs_fid2,
	cachefs_rwlock,
	cachefs_rwunlock,
	cachefs_seek,
	fs_cmp,
	cachefs_frlock,
	cachefs_realvp,
	cachefs_bmap,		/* VOP_BMAP */
	cachefs_strategy,	/* VOP_STRATEGY */
	cachefs_map,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	fs_poll,
	cachefs_dump,
	fs_pathconf,
	(vop_allocstore_t)fs_nosys,
	cachefs_fcntl,
	cachefs_reclaim,
	cachefs_attr_get,
	cachefs_attr_set,
	cachefs_attr_remove,
	cachefs_attr_list,
	fs_cover,
	(vop_link_removed_t)fs_noval,
	cachefs_vnode_change,
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

/* ARGSUSED */
int
cachefs_open(bhv_desc_t *bdp, vnode_t **vpp, mode_t flag, cred_t *cr)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_open: ENTER vpp %p flag %x \n", vpp, flag));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_open, current_pid(),
		vpp, flag);
	ASSERT(VALID_ADDR(vpp));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	if ( (flag & (FWRITE|FTRUNC)) && (((*vpp)->v_type == VDIR) ||
		((*vpp)->v_type == VLNK)) ) {
			error = EISDIR;
	}

	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_open: EXIT vpp %p error %d\n", vpp, error));
	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_open: EXIT vpp %p error %d\n", vpp, error));
	return (error);
}

/* ARGSUSED */
int
cachefs_close(bhv_desc_t *bdp, int flag, lastclose_t lastclose, 
	cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);
	int error = 0;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_close: ENTER vp %p\n", vp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_close, current_pid(),
		vp, flag);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;

	if (cp->c_flags & CN_NEEDINVAL) {
		error = cachefs_inval_object(cp, UNLOCKED);
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_close: invalidate cp 0x%p\n", cp));
	}
	if ((lastclose == L_TRUE) && (vp->v_type == VREG)) {
		VOP_FSYNC(vp, FSYNC_WAIT, cr, (off_t)0, (off_t)-1, error);
		if (error || cp->c_error) {
			if (!error) {
				error = cp->c_error;
			}
			(void)cachefs_inval_object(cp, UNLOCKED);
			cp->c_error = 0;
		}
	}

	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_close: EXIT vp %p \n", vp));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_close: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_close: EXIT vp %p error %d\n", vp, error));
	return (error);
}

/*ARGSUSED*/
int
cachefs_read_internal(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	cred_t *cr,
	flid_t *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct cnode *cp = BHVTOC(bdp);
	int n;
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	struct buf *bp;
	int nmaps;
	struct bmapval bmv[NCFSREADMAPS];
	int i;
	cred_t *old_cr;
	int ospl;
	vattr_t *attrp = NULL;

	CFS_DEBUG(CFSDEBUG_VOPS,
	    printf("cachefs_read: ENTER vp 0x%p, off 0x%llx, resid %d, c_size "
	    "%lld\n", vp, uiop->uio_offset, (int)uiop->uio_resid, cp->c_size));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_read, current_pid(),
		vp, uiop);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(cr));
	/*
	 * Do a quick check for easy cases (i.e. EOF, zero count)
	 */
	CACHEFS_STATS->cs_vnops++;
	if (vp->v_type != VREG) {
		CFS_DEBUG(CFSDEBUG_ERROR2,
			printf("cachefs_read: EXIT vp %p EISDIR\n", vp));
		return (EISDIR);
	}
	if (uiop->uio_offset < 0) {
		CFS_DEBUG(CFSDEBUG_ERROR2,
			printf("cachefs_read: EXIT vp %p EINVAL\n", vp));
		return (EINVAL);
	}
	error = CFSOP_CHECK_COBJECT(fscp, cp, (vattr_t *)NULL, cr, READLOCKED);
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_read: EXIT ESTALE\n"));
	switch (error) {
		case 0:
			break;
		default:
			CFS_DEBUG(CFSDEBUG_ERROR2,
				printf("cachefs_read: EXIT vp %p error %d\n",
					vp, error));
			return (error);
	}
	if (!C_CACHING(cp)) {
		attrp = CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
		attrp->va_mode = AT_MODE;
		(void)BACK_VOP_GETATTR(cp, attrp, 0, cr, BACKOP_BLOCK, &error);
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrp);
		if (error) {
			CFS_DEBUG(CFSDEBUG_ERROR2,
				printf("cachefs_read: EXIT vp %p error %d\n",
					vp, error));
			return (error);
		}
	}
	if ((uiop->uio_resid == 0) || (uiop->uio_offset >= cp->c_size))
		return (0);
	crhold(cr);
	ospl = mutex_spinlock( &cp->c_statelock );
	old_cr = cp->c_cred;
	cp->c_cred = cr;
	mutex_spinunlock( &cp->c_statelock, ospl );
	if ( old_cr ) {
		crfree( old_cr );
	}
	n = CFS_BMAPSIZE(cp);
	/*
	 * Sit in a loop and transfer (uiomove) the data.
	 * Do nothing if the user is reading past the end of the file.
	 * Note that n was initialized above to CFS_BMAPSIZE so that we
	 * make one pass through the loop when the other conditions have been
	 * met.
	 */
	while ( (error == 0) && (uiop->uio_resid > 0) && n &&
		(cp->c_size > uiop->uio_offset) ) {
			nmaps = cachefs_readahead + 1;
			error = cachefs_bmap(bdp, uiop->uio_offset, uiop->uio_resid,
				B_READ, cr, bmv, &nmaps);
			for (i = 0; (i < nmaps) && !error && (uiop->uio_resid > 0) && n &&
				bmv[i].pbsize; i++) {
#ifdef DEBUG
					/*
					 * Terminate readahead at the end of the file.
					 * This is debugging verification of cachefs_bmap.
					 */
					if ( BBTOOFF( bmv[i].offset ) >= cp->c_size ) {
						break;
					}
#endif /* DEBUG */
					bp = chunkread( vp, &bmv[i], nmaps - i, cr );
					ASSERT((uiop->uio_segflg != UIO_NOSPACE) ||
						pfind(vp, pnum(BBTOOFF(bmv[i].offset)), 0));
					CFS_DEBUG(CFSDEBUG_RDWR,
						printf("cachefs_read(line %d): read chunk "
							"uio_offset %lld, len %d, bp 0x%p, "
							"b_resid %d, b_error %d, pboff %d\n", __LINE__,
							uiop->uio_offset, n, bp, bp->b_resid, bp->b_error,
							bmv[i].pboff));
					if ( bp->b_flags & B_ERROR ) {
						error = bp->b_error;
					} else {
						/*
						 * we copy to the user either the amount requested or
						 * the valid data in the buffer, whichever is smaller
						 */
						n = MIN(cp->c_size - uiop->uio_offset,
							MIN(bmv[i].bsize - bp->b_resid - bmv[i].pboff,
							uiop->uio_resid));
						ASSERT( n >= 0 );
						if ( n > 0 ) {
							error = biomove( bp, bmv[i].pboff, n, UIO_READ,
								uiop );
						}
					}
					brelse( bp );
			}
	}

	ASSERT(!attrp);
	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_read: EXIT error %d resid %d \n", error,
			uiop->uio_resid));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_read: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_read: EXIT vp %p error %d\n", vp, error));
	return (error);
}

/*ARGSUSED*/
int
cachefs_read(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	cred_t *cr,
	flid_t *fl)
{
	int error;

	if (!(ioflag & IO_ISLOCKED))
		rw_enter(&BHVTOC(bdp)->c_rwlock, RW_READER);
	error = cachefs_read_internal(bdp, uiop, ioflag, cr, fl);
	if (!(ioflag & IO_ISLOCKED))
		rw_exit(&BHVTOC(bdp)->c_rwlock);

	return error;
}

/*ARGSUSED*/
int
cachefs_write_internal(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	cred_t *cr,
	flid_t *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int i;
	vattr_t *attrp = NULL;
	struct cnode *cp = BHVTOC(bdp);
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	ssize_t resid;
	struct buf *bp;
	int nmaps;
	int n;
	off_t limit;
	struct bmapval bmv[NCFSWRITEMAPS];
	cred_t *old_cr;
	int ospl;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_write: ENTER vp %p offset %llu count %d cflags %x\n",
			vp, uiop->uio_offset, uiop->uio_resid, cp->c_flags));
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(cr));

	ASSERT(RW_WRITE_HELD(&cp->c_rwlock));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_write, current_pid(),
		vp, uiop);
	CACHEFS_STATS->cs_vnops++;
	if (vp->v_type != VREG) {
		error = EISDIR;
		goto out;
	}
	if (error = cp->c_error) {
		goto out;
	}
	error = CFSOP_CHECK_COBJECT(fscp, cp, (vattr_t *)NULL, cr, WRITELOCKED);
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_write: EXIT ESTALE\n"));
	if (error) {
		CFS_DEBUG(CFSDEBUG_ERROR2,
			printf("cachefs_write: EXIT vp %p error %d\n", vp, error));
		return (error);
	}
	if (!C_CACHING(cp)) {
		attrp = CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
		attrp->va_mode = AT_MODE;
		(void)BACK_VOP_GETATTR(cp, attrp, 0, cr, BACKOP_BLOCK, &error);
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrp);
		if (error) {
			goto out;
		}
	}
	if (uiop->uio_resid == 0) {
		goto out;
	}

	crhold(cr);
	ospl = mutex_spinlock( &cp->c_statelock );
	old_cr = cp->c_cred;
	cp->c_cred = cr;
	mutex_spinunlock( &cp->c_statelock, ospl );
	if ( old_cr )
		crfree( old_cr );
	if (ioflag & IO_APPEND)
		uiop->uio_offset = cp->c_size;
	limit = uiop->uio_limit - uiop->uio_offset;
	if ( limit <= 0 ) {
		error = EFBIG;
		CFS_DEBUG(CFSDEBUG_ERROR2,
			printf("cachefs_write(line %d): uio_offset too big, uio_offset "
				"0x%llx, uio_limit 0x%llx, n 0x%llx\n", __LINE__,
				uiop->uio_offset, uiop->uio_limit, limit));
		goto out;
	} else if ( limit < (off_t)uiop->uio_resid ) {
		resid = (ssize_t)((off_t)uiop->uio_resid - limit);
		uiop->uio_resid = (ssize_t)limit;
	} else {
		resid = 0;
	}
	ospl = mutex_spinlock(&cp->c_statelock);
	cp->c_flags |= CN_WRITTEN;
	mutex_spinunlock(&cp->c_statelock, ospl);
	do {
		nmaps = NCFSWRITEMAPS;
		if ( error = cachefs_bmap(bdp, uiop->uio_offset, uiop->uio_resid,
			B_WRITE, cr, bmv, &nmaps) ) {
				break;
		}
		for (i = 0; (i < nmaps) && !error && (uiop->uio_resid > 0); i++) {
			/*
			 * n is how much to transfer
			 * it is either the block size or the resid of what the user will
			 * accept
			 */
			n = MIN( bmv[i].pbsize, uiop->uio_resid );
			/*
			 * Conditions for gettting a new buffer as opposed to reading
			 * it.  Any one of the following is sufficient.
			 *
			 *		1. uio_offset is >= the file size rounded up to the
			 *		   nearest cachefs block boundary.
			 *		2. uio_offset is a multiple of the block size and the
			 *		   number of bytes being tranferred in this pass is
			 *		   equal to the buffer length.
			 *		3. uio_offset is a multiple of the block size and
			 *		   uio_offset + n >= c_size
			 *
			 * If none of these conditions is met, read in the buffer.
			 */
			if ( (uiop->uio_offset >= ((cp->c_size + (off_t)CFS_BOFFSET(cp)) &
				CFS_BMASK(cp))) || ((bmv[i].pboff == 0) &&
				((n == bmv[i].length) ||
				((uiop->uio_offset + n) >= cp->c_size))) ) {
					bp = getchunk( vp, &bmv[i], cr );
					CNODE_TRACE(CNTRACE_WRITE, cp, (void *)getchunk,
						BBTOOFF(bmv[i].offset), bmv[i].pbsize);
			} else {
				bp = chunkread( vp, &bmv[i], nmaps - i, cr );
			}
			if ( bp->b_flags & B_ERROR ) {
				error = bp->b_error;
				brelse( bp );
				break;
			}
			CNODE_TRACE(CNTRACE_WRITE, cp, (void *)cachefs_write,
				uiop->uio_offset, n);
			CFS_DEBUG(CFSDEBUG_RDWR,
				printf("cachefs_write(line %d): write chunk uio_offset %lld, "
					"len %d, bp 0x%p, b_resid %d, b_error %d, pboff %d\n",
					__LINE__, uiop->uio_offset, n, bp, bp->b_resid, bp->b_error,
					bmv[i].pboff));
			/*
			 * Check for holes which need to be cleared.
			 * If the uio offset is greater than the old file size, we
			 * are creating a hole.
			 */
			if (cp->c_size < uiop->uio_offset) {
				if (CFS_SAME_BLOCK(cp, cp->c_size, uiop->uio_offset)) {
					CFS_DEBUG(CFSDEBUG_RDWR,
						printf("cachefs_write(line %d): fill hole from %lld "
							"to %lld, bp 0x%p\n", __LINE__, cp->c_size,
							uiop->uio_offset, bp));
					/*
					 * clear from the old EOF to the uio offset
					 */
					if (BP_ISMAPPED(bp)) {
						bzero(bp->b_un.b_addr + (cp->c_size & CFS_BOFFSET(cp)),
							uiop->uio_offset - cp->c_size + 1);
					} else {
						bp_mapin(bp);
						bzero(bp->b_un.b_addr + (cp->c_size & CFS_BOFFSET(cp)),
							uiop->uio_offset - cp->c_size + 1);
						bp_mapout(bp);
					}
				} else {
					int tmp_nmaps;
					struct bmapval tmp_bmv;
					struct buf *tmp_bp;

					/*
					 * read the buffer containing the old EOF, clear
					 * to the end of the it and write that buffer out
					 */
					/*
					 * map the buffer containing the old EOF
					 */
					if (error = cachefs_bmap(bdp, cp->c_size,
						CFS_BMAPSIZE(cp), B_WRITE, cr, &tmp_bmv, &tmp_nmaps)) {
							error = bp->b_error;
							brelse( bp );
							break;
					}
					tmp_bp = chunkread( vp, &tmp_bmv, 1, cr );
					CFS_DEBUG(CFSDEBUG_RDWR,
						printf("cachefs_write(line %d): fill hole from %lld "
							"to %lld, bp 0x%p tmp bp 0x%p\n", __LINE__,
							cp->c_size, uiop->uio_offset, bp, tmp_bp));
					if (BP_ISMAPPED(tmp_bp)) {
						bzero(tmp_bp->b_un.b_addr + tmp_bmv.pboff,
							tmp_bmv.pbsize);
					} else {
						bp_mapin(tmp_bp);
						bzero(tmp_bp->b_un.b_addr + tmp_bmv.pboff,
							tmp_bmv.pbsize);
						bp_mapout(tmp_bp);
					}
					if ( ioflag & (IO_SYNC | IO_DSYNC) ) {
						bwrite( tmp_bp );
					} else {
						bdwrite( tmp_bp );
					}
					/*
					 * clear from the start of the buffer containing the
					 * uio offset to the offset
					 */
					if (BP_ISMAPPED(bp)) {
						bzero(bp->b_un.b_addr,
							uiop->uio_offset & CFS_BOFFSET(cp));
					} else {
						bp_mapin(bp);
						bzero(bp->b_un.b_addr,
							uiop->uio_offset & CFS_BOFFSET(cp));
						bp_mapout(bp);
					}
				}
			}
			error = biomove( bp, bmv[i].pboff, n, UIO_WRITE, uiop );
			if ( error ) {
				if ( !(bp->b_flags & B_DONE) ) {
					bp->b_flags |= B_STALE|B_DONE|B_ERROR;
				}
				brelse( bp );
				break;
			}
			/*
			 * cp->c_attr->ca_size is the maximum number of
			 * bytes known to be in the file.
			 * Make sure it is at least as high as the
			 * last byte we just wrote into the buffer.
			 */
			ospl = mutex_spinlock( &cp->c_statelock );
			cp->c_written += n;
			if (cp->c_size < uiop->uio_offset) {
				CFS_DEBUG(CFSDEBUG_SIZE | CFSDEBUG_RDWR,
					printf("cachefs_write(line %d): cp 0x%p grows from %lld "
						"to %lld\n", __LINE__, cp, cp->c_size,
						uiop->uio_offset));
				cp->c_size = uiop->uio_offset;
				if (C_CACHING(cp)) {
					cp->c_attr->ca_size = cp->c_size;
				}
				CNODE_TRACE(CNTRACE_ATTR, cp, (void *)cachefs_write, 0, 0);
				CNODE_TRACE(CNTRACE_SIZE, cp, (void *)cachefs_write,
					cp->c_size, 0);
				cp->c_flags |= CN_UPDATED;
				CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_write,
					cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
				CFS_DEBUG(CFSDEBUG_ATTR,
					printf("cachefs_write(line %d): attr update, cp 0x%p\n",
						__LINE__, cp));
				ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
					FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
				mutex_spinunlock( &cp->c_statelock, ospl );
				if (!cp->c_frontvp) {
					error = cachefs_getfrontvp(cp);
					CFS_DEBUG(CFSDEBUG_ERROR, if (error)
						printf("cachefs_write(line %d): error %d "
							"getting front vnode\n", __LINE__, error));
					if (error) {
						(void)cachefs_nocache(cp);
						error = 0;
					}
				}
			} else {
				mutex_spinunlock( &cp->c_statelock, ospl );
			}
			if ( ioflag & (IO_SYNC | IO_DSYNC) ) {
				bwrite( bp );
			} else {
				bdwrite( bp );
			}
		}
	} while (error == 0 && uiop->uio_resid > 0);
	ASSERT(error || !uiop->uio_resid);
	uiop->uio_resid += resid;


out:
	ASSERT(!attrp);
	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_write: EXIT error %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_write: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_write: EXIT vp %p error %d\n", vp, error));
	return (error);
}

/*ARGSUSED*/
int
cachefs_write(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	cred_t *cr,
	flid_t *fl)
{
	int error;

	if (!(ioflag & IO_ISLOCKED))
		rw_enter(&BHVTOC(bdp)->c_rwlock, RW_WRITER);
	error = cachefs_write_internal(bdp, uiop, ioflag, cr, fl);
	if (!(ioflag & IO_ISLOCKED))
		rw_exit(&BHVTOC(bdp)->c_rwlock);

	return error;
}

/*ARGSUSED*/
int
cachefs_dump(bhv_desc_t *bdp, caddr_t foo1, daddr_t foo2, u_int foo3)
{
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_dump, current_pid(),
		BHV_TO_VNODE(bdp), foo1);
	ASSERT(VALID_ADDR(BHV_TO_VNODE(bdp)));
	CACHEFS_STATS->cs_vnops++;
	return (ENOSYS); /* should we panic if we get here? */
}

/*ARGSUSED*/
int
cachefs_getattr(bhv_desc_t *bdp, struct vattr *vap, int flags, cred_t *cr)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct cnode *cp = BHVTOC(bdp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;
	int ospl;
	int vmask = vap->va_mask;

	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_getattr: ENTER vp %p\n", vp));

	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(fscp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_getattr, current_pid(),
		vp, vap);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	if (!C_CACHING(cp)) {
		ASSERT(VALID_ADDR(cp->c_backvp));
		(void)BACK_VOP_GETATTR(cp, vap, flags, cr, BACKOP_BLOCK,
					&error);
	} else if (!(flags & ATTR_LAZY)) {
		error = CFSOP_CHECK_COBJECT(fscp, cp, (vattr_t *)NULL, cr,
						UNLOCKED);
		if (error == 0) {
			ospl = mutex_spinlock(&cp->c_statelock);
			CATTR_TO_VATTR(cp->c_attr, vap);
			vap->va_blksize = CFS_BMAPSIZE(cp);
			mutex_spinunlock(&cp->c_statelock, ospl);
		}
	} else {
		ospl = mutex_spinlock(&cp->c_statelock);
		CATTR_TO_VATTR(cp->c_attr, vap);
		vap->va_blksize = CFS_BMAPSIZE(cp);
		mutex_spinunlock(&cp->c_statelock, ospl);
	}
	vap->va_mask = vmask;
	vap->va_fsid = make_cachefs_fsid(fscp, vap->va_fsid);
	vap->va_rdev = FSC_TO_VFS(fscp)->vfs_dev;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_getattr: EXIT error = %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_getattr: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_getattr: EXIT vp %p error %d\n", vp, error));
	return (error);
}

int
cachefs_setattr(bhv_desc_t *bdp, register struct vattr *vap,
	int flags, cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	off_t vasize = vap->va_size;
	cnode_t *cp = BHVTOC(bdp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	register long int mask = vap->va_mask;
	int error = 0;
	off_t tossoff;
	int ospl;

	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_setattr: ENTER vp %p\n", vp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_setattr, current_pid(),
		vp, vap);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	/*
	 * Cannot set these attributes.
	 * Cannot set mandatory locking.
	 */
	if ((mask & AT_NOSET) || ((mask & AT_MODE) && MANDLOCK(vp, vap->va_mode))) {
		CFS_DEBUG(CFSDEBUG_ERROR2,
			printf("cachefs_setattr: EXIT vp %p EINVAL\n", vp));
		return (EINVAL);
	}
	/*
	 * Truncate file.  Must have write permission and not be a directory.
	 */
	if ((vp->v_type == VDIR) && (mask & AT_SIZE)) {
		CFS_DEBUG(CFSDEBUG_ERROR2,
			printf("cachefs_setattr: EXIT vp %p EISDIR\n", vp));
		return (EISDIR);
	}
	if (flags & ATTR_LAZY) {
		return(0);
	}

	/*
	 * Gotta deal with one special case here, where we're setting the
	 * size of the file. First, we zero out part of the page after the
	 * new size of the file. Then we toss (not write) all pages after
	 * page in which the new offset occurs. Note that the NULL passed
	 * in instead of a putapage() fn parameter is correct, since
	 * no dirty pages will be found (B_TRUNC | B_INVAL).
	 */
	rw_enter(&cp->c_rwlock, RW_WRITER);

	/* sync dirty pages */
	if (VN_DIRTY(vp)) {
		error = cachefs_flushvp(bdp, (off_t)0, (size_t)0, 0, 0);
		if (error) {
			/*
			 * We do not know what state the cached object is in
			 * now, so we will just invalidate it.
			 */
			cachefs_inval_object(cp, WRITELOCKED);
		}
	}

	/*
	 * If the file size is being changed then
	 * toss whole pages beyond the end of the file and zero
	 * the portion of the last page that is beyond the end of the file.
	 */
	if ((mask & AT_SIZE) && (vap->va_size < cp->c_size) &&
		(vp->v_type == VREG)) {
			/*
			 * if the file has shrunk, toss buffers
			 * flush out any delayed writes
			 * throw out everything from the new size through the
			 * largest possible readahead offset
			 */
			tossoff = (off_t)((cp->c_size + (off_t)BBMASK)
						& ~BBMASK) +
				(off_t)(cachefs_readahead * CFS_BMAPSIZE(cp));
			VOP_TOSS_PAGES(vp, vap->va_size, tossoff - 1, FI_NONE);
			/*
			 * If the file is being cached, truncate the front file.
			 */
			if (C_CACHING(cp)) {
				ASSERT(C_TO_FSCACHE(cp));
				ASSERT(cp->c_frontfid.fid_len &&
					(cp->c_frontfid.fid_len <= MAXFIDSZ));
				vasize = vap->va_size;
				vap->va_mask = AT_SIZE;
				vap->va_size +=
					DATA_START(C_TO_FSCACHE(cp)->fs_cache);
#ifdef DEBUG
				error = FRONT_VOP_SETATTR(cp, vap, flags, sys_cred);
				CFS_DEBUG(CFSDEBUG_ERROR, if (error)
					printf("cachefs_setattr: error setting front file size: "
						"%d\n", error));
				CNODE_TRACE(CNTRACE_FRONTSIZE, cp, (void *)cachefs_setattr,
					vap->va_size, (int)WRITELOCKED);
				error = 0;
#else /* DEBUG */
				(void)FRONT_VOP_SETATTR(cp, vap, flags, sys_cred);
#endif /* DEBUG */
				vap->va_mask = mask;
				vap->va_size = vasize;
			}
	}

	(void)BACK_VOP_SETATTR(cp, vap, flags, cr, BACKOP_BLOCK, &error);
	if (error) {
		goto out;
	}
	ospl = mutex_spinlock( &cp->c_statelock );
	if ( C_CACHING( cp ) &&
		(mask & (AT_MODE|AT_UID|AT_GID|AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME)) ) {
			ASSERT(VALID_ADDR(cp->c_fileheader));
			if (mask & AT_UID) {
				cp->c_attr->ca_uid = vap->va_uid;
			}
			if (mask & AT_GID) {
				cp->c_attr->ca_gid = vap->va_gid;
			}
			if (mask & AT_SIZE) {
				CFS_DEBUG(CFSDEBUG_SIZE,
					if (vap->va_size != cp->c_size)
						printf("cachefs_setattr(line %d): cp 0x%p size "
							"change from %lld to %lld\n", __LINE__, cp,
							cp->c_size, vap->va_size));
				cp->c_size = cp->c_attr->ca_size = vap->va_size;
				CNODE_TRACE(CNTRACE_SIZE, cp, (void *)cachefs_setattr,
					cp->c_size, 0);
			}
			if (mask & AT_ATIME) {
				cp->c_attr->ca_atime = vap->va_atime;
			}
			if (mask & AT_MTIME) {
				cp->c_attr->ca_mtime = vap->va_mtime;
			}
			if (mask & AT_CTIME) {
				cp->c_attr->ca_ctime = vap->va_ctime;
			}
			CNODE_TRACE(CNTRACE_ATTR, cp, (void *)cachefs_setattr, 0, 0);
			CFS_DEBUG(CFSDEBUG_ATTR,
				printf("cachefs_setattr(line %d): attr update, cp 0x%p\n",
					__LINE__, cp));
			cp->c_flags |= CN_UPDATED;
			CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_setattr,
				cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
			ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
				FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
			mutex_spinunlock( &cp->c_statelock, ospl );
			if (!cp->c_frontvp) {
				error = cachefs_getfrontvp(cp);
				CFS_DEBUG(CFSDEBUG_ERROR, if (error)
					printf("cachefs_setattr(line %d): error %d "
						"getting front vnode\n", __LINE__, error));
				if (error) {
					(void)cachefs_nocache(cp);
					error = 0;
				}
			}
	} else {
		mutex_spinunlock( &cp->c_statelock, ospl );
	}
	/*
	 * Attr's have changed, so we need to call the CFSOP_MODIFY_COBJECT
	 * function to notify the consistency interface of this.
	 */
	CFSOP_MODIFY_COBJECT(fscp, cp, cr);

out:
	rw_exit(&cp->c_rwlock);

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_setattr: EXIT vp %p error %d \n", vp, error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_setattr: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_setattr: EXIT vp %p error %d\n", vp, error));
	return (error);
}

/* ARGSUSED */
int
cachefs_access(bhv_desc_t *bdp, int mode, cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	enum backop_stat status;
	int error = 0;

	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_access: ENTER vp %p\n", vp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_access, current_pid(),
		vp, mode);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	/*
	 * Make sure the cnode attrs are valid first.
	 */
	error = CFSOP_CHECK_COBJECT(fscp, cp, (vattr_t *)NULL, cr, UNLOCKED);
	if (!error) {
		if ((fscp->fs_options & CFS_ACCESS_BACKFS) || !C_CACHING(cp)) {
			status = BACK_VOP_ACCESS(cp, mode, cr,
				C_CACHING(cp) ?
					BACKOP_NONBLOCK : BACKOP_BLOCK, &error);
			if (status == BACKOP_NETERR) {
				ASSERT(C_CACHING(cp));
				ASSERT(VALID_ADDR(cp->c_attr));
				error = 0;
				if ((mode & VWRITE) && !WRITEALLOWED(vp, cr)) {
					error = EROFS;
				} else if (cr->cr_uid == 0) {
					error = 0;
				} else {
					if (cr->cr_uid != cp->c_attr->ca_uid) {
						mode >>= 3;
						if (!groupmember(cp->c_attr->ca_gid, cr))
							mode >>= 3;
					}
					if ((cp->c_attr->ca_mode & mode) != mode) {
						error = EACCES;
					}
				}
			}
		} else {
			if ((mode & VWRITE) && !WRITEALLOWED(vp, cr)) {
				error = EROFS;
			} else if (cr->cr_uid == 0) {
				error = 0;
			} else {
				if (cr->cr_uid != cp->c_attr->ca_uid) {
					mode >>= 3;
					if (!groupmember(cp->c_attr->ca_gid, cr))
						mode >>= 3;
				}
				if ((cp->c_attr->ca_mode & mode) != mode) {
					error = EACCES;
				}
			}
		}
	}

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_access: EXIT error = %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_access: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_access: EXIT vp %p error %d\n", vp, error));
	return (error);
}

/*
 * CFS has a fastsymlink scheme. If the size of the link is < ALLOCMAP_SIZE,
 * then the link is placed in the allocation map.
 */
int
cachefs_readlink(bhv_desc_t *bdp, struct uio *uiop, cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);
	int error = 0;
	int ospl;
	int return_resid;
	int return_len;
	off_t return_offset;
	caddr_t return_base;
	int len;
	off_t offset;
	caddr_t base;
	uio_t uio;
	iovec_t iov;
	lockmode_t lockmode = UNLOCKED;

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_READLINK,
		printf("cachefs_readlink: ENTER vp %p\n", vp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_readlink, current_pid(),
		vp, uiop);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	if (vp->v_type != VLNK) {
		CFS_DEBUG(CFSDEBUG_ERROR2,
			printf("cachefs_readlink: EXIT vp %p ENXIO\n", vp));
		return (ENXIO);
	}

	cachefs_rwlock(bdp, VRWLOCK_READ);
	lockmode = READLOCKED;

again:
	/*
	 * Make sure the cnode attrs are valid first.
	 */
	error = CFSOP_CHECK_COBJECT(C_TO_FSCACHE(cp), cp, (vattr_t *)NULL, cr,
		lockmode);
	if ( !error ) {
		if ( C_CACHING( cp ) ) {
			ASSERT(cp->c_frontfid.fid_len &&
				(cp->c_frontfid.fid_len <= MAXFIDSZ));
			ASSERT(VALID_ADDR(cp->c_fileheader));
			ASSERT(valid_file_header(cp->c_fileheader, NULL));
			ospl = mutex_spinlock(&cp->c_statelock);
			/*
			 * If the file has been populated, transfer the data from the
			 * front file.
			 */
			if (cp->c_fileheader->fh_metadata.md_state & MD_POPULATED) {
				/*
				 * If there is no allocation map, the data is all contained
				 * within the allocation map.  Simply copy it out now.
				 * Otherwise, read it from the front file directly into the
				 * user's buffer.
				 */
				if (cp->c_fileheader->fh_metadata.md_state & MD_NOALLOCMAP) {
					mutex_spinunlock(&cp->c_statelock, ospl);
					CACHEFS_STATS->cs_shortrdlnk++;
					error = uiomove((caddr_t)cp->c_fileheader->fh_allocmap,
						cp->c_fileheader->fh_metadata.md_allocents,
						UIO_READ, uiop);
				} else {
					mutex_spinunlock(&cp->c_statelock, ospl);
					CACHEFS_STATS->cs_longrdlnk++;
					uiop->uio_offset += (off_t)DATA_START(C_TO_FSCACHE(cp));
					error = FRONT_READ_VP(cp, uiop, 0, sys_cred);
					uiop->uio_offset -= (off_t)DATA_START(C_TO_FSCACHE(cp));
					ASSERT(uiop->uio_offset <=
						cp->c_fileheader->fh_allocmap[0].am_size);
					if (error) {
						CACHEFS_STATS->cs_fronterror++;
						CFS_DEBUG(CFSDEBUG_ERROR,
							printf("cachefs_readlink(line %d): file header "
								"read error, cnode 0x%p: %d\n", __LINE__,
								cp, error));
						if (error != EINTR) {
							CFS_DEBUG(CFSDEBUG_NOCACHE,
								printf("cachefs_readlink: nocache cp 0x%p "
									"on error %d from FRONT_READ_VP\n",
									cp, error));
							cachefs_nocache(cp);
							error = 0;
							goto again;
						}
					}
				}
			} else {
				mutex_spinunlock(&cp->c_statelock, ospl);
				/*
				 * we need to get the link data from the back file system and
				 * cache it, so upgrade to a write lock
				 */
				if (lockmode == READLOCKED) {
					if (!rw_tryupgrade(&cp->c_rwlock)) {
						rw_exit(&cp->c_rwlock);
						rw_enter(&cp->c_rwlock, RW_WRITER);
						lockmode = WRITELOCKED;
						goto again;
					} else {
						lockmode = WRITELOCKED;
					}
				}
				/*
				 * The file has not been populated, so we will do that now.
				 * If the data will fit in the allocation map, put it there.
				 * Otherwise, store it after the file header.
				 */
				if (cp->c_size < sizeof(cp->c_fileheader->fh_allocmap)) {
					/*
					 * Read the link information from the back FS into the
					 * allocation map.
					 */
					UIO_SETUP(&uio, &iov,
						(caddr_t)cp->c_fileheader->fh_allocmap,
						cp->c_size, 0, UIO_SYSSPACE, FREAD,
						(off_t)RLIM_INFINITY);
					(void)BACK_VOP_READLINK(cp, &uio, cr, BACKOP_BLOCK, &error);
					if (!error) {
						/*
						 * The link was read into the allocation map.
						 * Indicate this in the metadata state and also
						 * indicate that the file is now populated.
						 * Mark the cnode as updated so the file header
						 * will be written.
						 */
						ospl = mutex_spinlock(&cp->c_statelock);
						cp->c_fileheader->fh_metadata.md_state |=
							(MD_NOALLOCMAP | MD_POPULATED);
						cp->c_fileheader->fh_metadata.md_allocents =
							cp->c_size - uio.uio_resid;
						CNODE_TRACE(CNTRACE_ALLOCENTS, cp,
							(void *)cachefs_readlink,
							(int)cp->c_fileheader->fh_metadata.md_allocents, 0);
						cp->c_flags |= CN_UPDATED;
						CNODE_TRACE(CNTRACE_UPDATE, cp,
							(void *)cachefs_readlink, cp->c_flags,
							cp->c_fileheader->fh_metadata.md_allocents);
						ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
							FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
						mutex_spinunlock(&cp->c_statelock, ospl);
						if (!cp->c_frontvp) {
							error = cachefs_getfrontvp(cp);
							CFS_DEBUG(CFSDEBUG_ERROR, if (error)
								printf("cachefs_readlink(line %d): error %d "
									"getting front vnode\n", __LINE__, error));
							if (error) {
								(void)cachefs_nocache(cp);
								error = 0;
							}
						}
						error = uiomove((caddr_t)cp->c_fileheader->fh_allocmap,
							cp->c_size, UIO_READ, uiop);
						if (error) {
							CACHEFS_STATS->cs_fronterror++;
							CFS_DEBUG(CFSDEBUG_ERROR,
								printf("cachefs_readlink(line %d): uiomove "
									"error %d, cnode 0x%p\n", __LINE__,
									error, cp));
						}
					} else if ((error != EPERM) && (error != EACCES)) {
						CACHEFS_STATS->cs_fronterror++;
						CFS_DEBUG(CFSDEBUG_ERROR,
							printf("cachefs_readlink(line %d): "
								"VOP_READLINK error %d, cnode 0x%p\n",
								__LINE__, error, cp));
					}
				} else {
					/*
					 * Read the link into the allocation map.
					 * Save the original uio values for resetting the uio
					 * structure.
					 */
					base = uiop->uio_iov->iov_base;
					len = uiop->uio_iov->iov_len;
					offset = uiop->uio_offset;
					/*
					 * The link data is larger than the allocation map, so
					 * we will have to read it into a kernel buffer and
					 * write it out past the fie header.
					 */
					(void)BACK_VOP_READLINK(cp, uiop, cr, BACKOP_BLOCK, &error);
					if (!error) {
						/*
						 * The link was successfully read.  Save the size
						 * and reset the uio structure for writing the data
						 * to the front file.
						 * Save the uio structure values and reset the
						 * uio structure for writing out the symlink data.
						 */
						return_base = uiop->uio_iov->iov_base;
						return_len = uiop->uio_iov->iov_len;
						return_offset = uiop->uio_offset;
						return_resid = uiop->uio_resid;
						uiop->uio_iov->iov_base = base;
						uiop->uio_iov->iov_len = len;
						uiop->uio_offset = offset +
							DATA_START(C_TO_FSCACHE(cp));
						uiop->uio_resid = len;
						error = FRONT_WRITE_VP(cp, uiop, 0, sys_cred);
						/*
						 * If we could not successfully write to the front
						 * file, nocache the object.  We have the data,
						 * so we can just copy it out to the user.
						 */
						if (error) {
							CACHEFS_STATS->cs_fronterror++;
							CFS_DEBUG(CFSDEBUG_ERROR,
								printf("cachefs_readlink(line %d): front file "
									"write error, cnode 0x%p: %d\n", __LINE__,
									cp, error));
							if (error != EINTR) {
								CFS_DEBUG(CFSDEBUG_NOCACHE,
									printf("cachefs_readlink: nocache cp 0x%p "
										"on error %d from FRONT_WRITE_VP\n",
										cp, error));
								cachefs_nocache(cp);
								error = 0;
							}
						} else if (uiop->uio_resid) {
							CACHEFS_STATS->cs_fronterror++;
							error = EIO;
							CFS_DEBUG(CFSDEBUG_NOCACHE,
								printf("cachefs_readlink: nocache cp 0x%p "
									"on short write, resid %d\n",
									cp, uiop->uio_resid));
							cachefs_nocache(cp);
							CFS_DEBUG(CFSDEBUG_ERROR,
								printf("cachefs_readlink(line %d): incomplete "
									"front file write, cnode 0x%p: resid "
									"%d\n", __LINE__, cp,
									(int)uiop->uio_resid));
						} else {
							/*
							 * Indicate that the file has been populated
							 * and that the file header needs to be
							 * written.
							 */
							ospl = mutex_spinlock(&cp->c_statelock);
							cp->c_fileheader->fh_metadata.md_state |=
								MD_POPULATED;
							cp->c_fileheader->fh_metadata.md_allocents = 1;
							CNODE_TRACE(CNTRACE_ALLOCENTS, cp,
								(void *)cachefs_readlink,
								(int)cp->c_fileheader->fh_metadata.md_allocents,
								0);
							cp->c_fileheader->fh_allocmap[0].am_start_off =
								(off_t)0;
							cp->c_fileheader->fh_allocmap[0].am_size =
								(off_t)len - uiop->uio_resid;
							CNODE_TRACE(CNTRACE_ALLOCMAP, cp,
								cachefs_readlink, 0,
								cp->c_fileheader->fh_allocmap[0].am_size);
							cp->c_flags |= CN_UPDATED;
							CNODE_TRACE(CNTRACE_UPDATE, cp,
								(void *)cachefs_readlink, cp->c_flags,
								cp->c_fileheader->fh_metadata.md_allocents);
							ASSERT(cachefs_zone_validate(
								(caddr_t)cp->c_fileheader,
								FILEHEADER_BLOCK_SIZE(
								C_TO_FSCACHE(cp)->fs_cache)));
							mutex_spinunlock(&cp->c_statelock, ospl);
							if (!cp->c_frontvp) {
								error = cachefs_getfrontvp(cp);
								CFS_DEBUG(CFSDEBUG_ERROR, if (error)
									printf("cachefs_readlink(line %d): "
										"error %d getting front vnode\n",
										__LINE__, error));
								if (error) {
									(void)cachefs_nocache(cp);
									error = 0;
								}
							}
						}
						uiop->uio_iov->iov_base = return_base;
						uiop->uio_iov->iov_len = return_len;
						uiop->uio_offset = return_offset;
						uiop->uio_resid = return_resid;
					}
				}
				ASSERT(valid_file_header(cp->c_fileheader, NULL));
			}
		} else {
			(void)BACK_VOP_READLINK(cp, uiop, cr, BACKOP_BLOCK, &error);
		}
	}

	ASSERT((lockmode == READLOCKED) || (lockmode == WRITELOCKED));
	cachefs_rwunlock(bdp,
		(lockmode == READLOCKED) ? VRWLOCK_READ : VRWLOCK_WRITE);

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_READLINK,
		printf("cachefs_readlink: EXIT error %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_readlink: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_readlink: EXIT vp %p error %d\n", vp, error));
	return (error);
}

/* ARGSUSED */
int
cachefs_fsync(bhv_desc_t *bdp, int syncflag, cred_t *cr,
		off_t start, off_t stop)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct cnode *cp = BHVTOC(bdp);
	int ospl;
	int error = 0;

	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_fsync: ENTER vp %p \n", vp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_fsync, current_pid(),
		vp, syncflag);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(!issplhi(getsr()));
	CACHEFS_STATS->cs_vnops++;
	/*
	 * Wait for any population in progress to complete.
	 */
	if (cp->c_flags & CN_DESTROY) {
		syncflag |= FSYNC_INVAL;
	}
	if (syncflag & FSYNC_WAIT) {
		(void)await_population(cp);
		cachefs_write_header(cp, 0);
		rw_enter(&cp->c_rwlock, FSYNC_INVAL ? RW_WRITER : RW_READER);
	} else {
		cachefs_write_header(cp, 0);
		if (!cmrlock(&cp->c_rwlock, FSYNC_INVAL ? RW_WRITER : RW_READER)) {
			goto out;
		}
	}
	if ((syncflag & FSYNC_INVAL) || (cp->c_flags & CN_WRITTEN) ||
		(cp->c_nio && (syncflag & FSYNC_WAIT))) {
			ospl = mutex_spinlock(&cp->c_statelock);
			cp->c_flags &= ~CN_WRITTEN;
			mutex_spinunlock(&cp->c_statelock, ospl);
			error = cachefs_flushvp(bdp, (off_t)0, (size_t)0,
				(syncflag & FSYNC_INVAL) ? CFS_INVAL : 0,
				(syncflag & FSYNC_WAIT) ? 0 : B_ASYNC);
			if (!error && C_CACHING(cp) && (cp->c_flags & CN_WRITTEN)) {
				ASSERT(cp->c_frontfid.fid_len &&
					(cp->c_frontfid.fid_len <= MAXFIDSZ));
				error = FRONT_VOP_FSYNC(cp, syncflag, sys_cred,
							start, stop);
				CFS_DEBUG(CFSDEBUG_VALIDATE,
					if (cp->c_fileheader && !(cp->c_flags & CN_RECLAIM))
						(validate_fileheader(cp, __FILE__, __LINE__),
							ASSERT(valid_allocmap(cp))));
			}
	}
	rw_exit(&cp->c_rwlock);

out:

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_fsync: EXIT vp %p \n", vp));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_fsync: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_fsync: EXIT vp %p error %d\n", vp, error));
	return (error);
}

/*
 * This routine does the real work of inactivating a vnode.
 */
int
cachefs_inactive(bhv_desc_t *bdp, cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp;
	off_t tossoff;
	int ospl;

	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_inactive: ENTER vp %p \n",vp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_inactive, current_pid(),
		vp, cr);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;

	cp = BHVTOC(bdp);

	ASSERT(C_TO_FSCACHE(cp));
	ASSERT(C_TO_FSCACHE(cp)->fs_cache != NULL);

	/*
	 * Check the cnode state flags to see if this one is being reactivated.
	 * If so, do do anything more.
	 */
	ASSERT(!cp->c_fileheader ||
		find_fileheader_by_fid(&cp->c_frontfid, cp->c_fileheader));
	ospl = mutex_spinlock( &cp->c_statelock );
	CNODE_TRACE(CNTRACE_ACT, cp, (void *)cachefs_inactive, cp->c_flags, 0);
	if (cp->c_flags & CN_SYNC) {
		if (cp->c_flags & CN_NEEDINVAL) {
			mutex_spinunlock( &cp->c_statelock, ospl );
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_inactive: invalidate cp 0x%p\n", cp));
			cachefs_inval_object(cp, NOLOCK);
		} else {
			mutex_spinunlock( &cp->c_statelock, ospl );
		}
		CFS_DEBUG(CFSDEBUG_VOPS,
			printf("cachefs_inactive: EXIT vp %p sync or purge\n",
				vp));
		return VN_INACTIVE_CACHE;
	} else if (cp->c_flags & CN_NEEDINVAL) {
		mutex_spinunlock( &cp->c_statelock, ospl );
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_inactive: invalidate cp 0x%p\n", cp));
		cachefs_inval_object(cp, NOLOCK);
	} else {
		mutex_spinunlock( &cp->c_statelock, ospl );
	}

	/* if we need to get rid of this cnode */
	if (cp->c_flags & CN_DESTROY) {

		ASSERT(!cp->c_refcnt);

		(void)cachefs_flushvp(bdp, (off_t)0, (size_t)0, CFS_TRUNC, 0);
		if (VN_DIRTY(vp) && cp->c_error) {
			tossoff = (off_t)((cp->c_size + (off_t)BBMASK) & ~BBMASK) +
				(off_t)(cachefs_readahead * CFS_BMAPSIZE(cp));
			VOP_TOSS_PAGES(vp, 0, tossoff - 1, FI_NONE);
			CFS_DEBUG(CFSDEBUG_VOPS,
				if (VN_DIRTY(vp)) printf("inactive: vp %p still dirty\n", vp));
		}
		(void)cachefs_write_header(cp, 0);

		if (cp->c_cred != NULL) {
			crfree(cp->c_cred);
			cp->c_cred = NULL;
		}

		if (cp->c_frontdirvp) {
			VN_RELE(cp->c_frontdirvp);
			cp->c_frontdirvp = NULL;
		}
		if (cp->c_frontvp) {
			VN_RELE(cp->c_frontvp);
			cp->c_frontvp = NULL;
		}
		if (cp->c_backvp) {
			VN_RELE(cp->c_backvp);
			cp->c_backvp = NULL;
		}
		if (cp->c_dcp) {
			CN_RELE(cp->c_dcp);
			cp->c_dcp = NULL;
		}
		if (cp->c_fileheader) {
			/*
			 * Save a copy of the back file ID before releasing the
			 * file header.  fscache_sync will need it when removing
			 * this cnode from the hash.
			 */
			cp->c_backfid = (fid_t *)CACHEFS_ZONE_ALLOC(Cachefs_fid_zone,
				KM_SLEEP);
			bcopy((void *)&cp->c_fileheader->fh_metadata.md_backfid,
				(void *)cp->c_backfid, sizeof(fid_t));
			/*
			 * Release the file header now.  It will not be needed
			 * again.  CACHEFS_RELEASE_FILEHEADER is a macro and
			 * will set cp->c_fileheader to NULL.
			 */
			CACHEFS_RELEASE_FILEHEADER(&cp->c_frontfid, cp->c_fileheader);
		}
		/*
		 * since we've released the front vnode, the cnode had better not
		 * be marked as updated
		 * writing out the updated file header should have been taken
		 * care of in cachefs_write_header
		 */
		ASSERT(!(cp->c_flags & CN_UPDATED));

	} else {

		/*
		 * Only sync if a sync is not in progress for this file.
		 * It is important to use c_statelock here when checking
		 * for CN_SYNC as fscache_sync will check v_flag for VINACT
		 * and set CN_SYNC while holding c_statelock.
		 * Also, cachefs_fsync does not sync the file header.  We
		 * do not do that here.  Instead, we let that be done as
		 * a part of fscache_sync.
		 */
		ospl = mutex_spinlock( &cp->c_statelock );
		if (!(cp->c_flags & CN_SYNC)) {
			mutex_spinunlock( &cp->c_statelock, ospl );
			(void)cachefs_fsync(bdp, FSYNC_WAIT, cr,
					(off_t)0, (off_t)-1);
		} else {
			mutex_spinunlock( &cp->c_statelock, ospl );
		}
		if (VN_DIRTY(vp) && cp->c_error) {
			tossoff = (off_t)((cp->c_size + (off_t)BBMASK) & ~BBMASK) +
				(off_t)(cachefs_readahead * CFS_BMAPSIZE(cp));
			VOP_TOSS_PAGES(vp, 0, tossoff - 1, FI_NONE);
			CFS_DEBUG(CFSDEBUG_VOPS,
				if (VN_DIRTY(vp))
					printf("inactive: vp %p still dirty\n",
						vp));
		}

		ASSERT(!cp->c_frontvp || valid_allocmap(cp));

		if (cp->c_frontdirvp) {
			VN_RELE(cp->c_frontdirvp);
			cp->c_frontdirvp = NULL;
		}
		if (!(cp->c_flags & CN_UPDATED) && cp->c_frontvp) {
			VN_RELE(cp->c_frontvp);
			cp->c_frontvp = NULL;
		}
		if (cp->c_backvp) {
			VN_RELE(cp->c_backvp);
			cp->c_backvp = NULL;
		}

		CFS_DEBUG(CFSDEBUG_VALIDATE,
			validate_fileheader(cp, __FILE__, __LINE__));

		ASSERT(!C_CACHING(cp) ||
			!(cp->c_fileheader->fh_metadata.md_state & MD_INIT));
		ospl = mutex_spinlock( &cp->c_statelock );
		cp->c_error = 0;
		cp->c_flags &= ~CN_FRLOCK;
		cp->c_written = 0;
		mutex_spinunlock( &cp->c_statelock, ospl );
		ASSERT(cp->c_frontvp || !(cp->c_flags & CN_UPDATED) || !C_CACHING(cp));
	}


	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_inactive: EXIT vp %p\n", vp));
	return VN_INACTIVE_CACHE;
}

/*
 * File system operations having to do with directory manipulation.
 */

int
nocache_lookup(cnode_t *dcp, char *nm, bhv_desc_t **bdpp, int flags,
	struct vnode *rdir, cred_t *cr)
{
	int error = 0;
	fid_t *backfid = NULL;
	vnode_t *back_vp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cnode_t *cp = NULL;

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("nocache_lookup: ENTER dcp 0x%p nm %s\n", dcp, nm));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)nocache_lookup, current_pid(),
		dcp, nm);
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(bdpp));
	ASSERT(!rdir || VALID_ADDR(rdir));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_nocachelookup++;
	/*
	 * Go to the back FS to look up the file.  Wait for the lookup to
	 * succeed.
	 */
	(void)cachefs_lookup_back(dcp, nm, &back_vp, &backfid, flags, rdir, cr,
		C_ISFS_NONBLOCK(fscp) ? BACKOP_NONBLOCK : BACKOP_BLOCK, &error);
	if ( !error ) {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("nocache_lookup(line %d): nocache lookup nm %s\n",
				__LINE__, nm));
		error = makecachefsnode( dcp, backfid, fscp, (fileheader_t *)NULL,
			(vnode_t *)NULL, (vnode_t *)NULL, back_vp, (vattr_t *)NULL, cr,
			CN_NOCACHE, &cp );
		if ( !error ) {
			*bdpp = CTOBHV(cp);
		}
		VN_RELE(back_vp);
		back_vp = NULL;
	}
	if (backfid) {
		freefid(backfid);
	}
	ASSERT(!back_vp);
	ASSERT(error || *bdpp);
	ASSERT(!error || !*bdpp);
	ASSERT(!error || !cp);
	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("nocache_lookup: EXIT vp 0x%p error = %d\n",
		       BHV_TO_VNODE(*bdpp), error));
	return(error);
}

int search_cached_dir = 0;

int
cachemiss_lookup(cnode_t *dcp, char *nm, bhv_desc_t **bdpp, int flags,
	struct vnode *rdir, cred_t *cr, int *lockstate, vnode_t *back_vp,
	fid_t *backfid)
{
	int did_lookup = 0;
	fid_t frontfid;
	int cnflags = 0;
	cnode_t *cp = NULL;
	vnode_t *front_vp = NULL;
	fileheader_t *fhp = NULL;
	int error = 0;
	int fiderr = 0;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	vnode_t *frontdir_vp = NULL;

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("cachemiss_lookup: ENTER dcp 0x%p nm %s\n", dcp, nm));
	/*
	 * This is a cache miss or a lookup for a non-existent
	 * file.
	 */
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachemiss_lookup, current_pid(),
		dcp, nm);
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(bdpp));
	ASSERT(!rdir || VALID_ADDR(rdir));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(lockstate));
	ASSERT(C_CACHING(dcp));
	ASSERT(VALID_ADDR(dcp->c_fileheader));
	/*
	 * No front file exists.  Do the lookup in the back FS.
	 * First, see if we need to bother the back FS.
	 */
	if (!back_vp) {
		if ((C_ISFS_DISCONNECT(fscp) || search_cached_dir) &&
			(dcp->c_fileheader->fh_metadata.md_state & MD_POPULATED)) {
			/*
			 * We pass the lock mode to cachefs_search_dir because it
			 * may need to pass it on to cachefs_inval_object.  There is
			 * no danger, however, that dcp->c_rwlock will be released
			 * and reacquired in cachefs_inval_object because this is
			 * a directory and there will be no buffers to invalidate.
			 */
			error = cachefs_search_dir(dcp, nm,
				(*lockstate == RW_WRITER) ? WRITELOCKED : READLOCKED);
			switch (error) {
				case 0:
					break;
				case ENOENT:
					CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
						printf("cachemiss_lookup: EXIT vp 0x%p error = %d\n",
							*bdpp, error));
					return(ENOENT);
				default:
					error = 0;
			}
		}
		/*
		 * This is a cache miss, so we must wait for the lookup in the back
		 * FS to succeed.
		 */
		(void)cachefs_lookup_back(dcp, nm, &back_vp, &backfid, flags, rdir, cr,
			BACKOP_BLOCK, &error);
		if (error) {
			return(error);
		}
		did_lookup = 1;
	}
	ASSERT(!error);
	ASSERT(back_vp);
	ASSERT(backfid);
	/*
	 * Cache miss.
	 * The back FS lookup succeeded.  Create the front
	 * file.  If this fails, nocache the cnode.
	 */
	if (*lockstate == RW_READER) {
		if (!rw_tryupgrade(&dcp->c_rwlock)) {
			rw_exit(&dcp->c_rwlock);
			rw_enter(&dcp->c_rwlock, RW_WRITER);
			*lockstate = RW_WRITER;
			error = EAGAIN;
			goto out;
		} else {
			*lockstate = RW_WRITER;
		}
	}
	if (dcp->c_frontdirvp || !(error = cachefs_getfrontdirvp(dcp))) {
		error = cachefs_create_frontfile(fscp->fs_cache, dcp->c_frontdirvp,
			nm, &front_vp, backfid, &fhp, back_vp->v_type);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachemiss_lookup(line %d): nocache cp 0x%p on error %d "
				"from cachefs_getfrontdirvp\n", __LINE__, dcp, error));
		cachefs_nocache(dcp);
	}
	if (error) {
		cnflags = CN_NOCACHE;
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachemiss_lookup(line %d): nocache %s on error %d\n",
				__LINE__, nm, error));
	} else if (back_vp->v_type == VDIR) {
		/*
		 * The file is a directory, so create the front
		 * directory.
		 */
		error = cachefs_create_frontdir(fscp->fs_cache,
			fscp->fs_cacheidvp, backfid, &frontdir_vp);
		switch (error) {
			case EEXIST:
				error = cachefs_lookup_frontdir(fscp->fs_cacheidvp, backfid,
					&frontdir_vp);
				if (error) {
					cnflags = CN_NOCACHE;
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachemiss_lookup(line %d): nocache %s "
							"on error %d from cachefs_lookup_frontdir\n",
							__LINE__, nm, error));
					if (fhp) {
						VOP_FID2(front_vp, &frontfid, fiderr)
						if (!fiderr) {
							CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
						} else {
							CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
						}
					}
					if (front_vp) {
						VN_RELE(front_vp);
						front_vp = NULL;
					}
				}
			case 0:
				break;
			default:
				cnflags = CN_NOCACHE;
				CFS_DEBUG(CFSDEBUG_NOCACHE,
					printf("cachemiss_lookup(line %d): nocache %s "
						"on error %d from cachefs_create_frontdir\n",
						__LINE__, nm, error));
				if (fhp) {
					VOP_FID2(front_vp, &frontfid, fiderr);
					if (!fiderr) {
						CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
					} else {
						CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
					}
				}
				if (front_vp) {
					VN_RELE(front_vp);
					front_vp = NULL;
				}
		}
	}
	error = makecachefsnode( dcp, backfid, fscp, fhp, front_vp,
		frontdir_vp, back_vp, (vattr_t *)NULL, cr, cnflags, &cp );
	switch ( error ) {
		case 0:
			*bdpp = CTOBHV(cp);
			fhp = NULL;
			break;
		case EEXIST:
			if (cp) {
				ASSERT(fhp != cp->c_fileheader);
				ASSERT(!back_vp || (back_vp->v_type == CTOV(cp)->v_type));
				if (dcp->c_frontdirvp ||
					!(error = cachefs_getfrontdirvp(dcp))) {
						(void)cachefs_remove_frontfile( dcp->c_frontdirvp,
							nm);
						if (back_vp->v_type == VDIR) {
							(void)cachefs_remove_frontdir(fscp, backfid, 0);
						}
				} else {
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachemiss_lookup(line %d): nocache cp 0x%p "
							"on error %d from cachefs_getfrontdirvp\n",
							__LINE__, cp, error));
					cachefs_nocache(cp);
				}
				if (C_CACHING(cp)) {
					error = FRONT_VOP_LINK(dcp, cp, nm, sys_cred);
					if (error) {
						CFS_DEBUG(CFSDEBUG_NOCACHE,
							printf("cachemiss_lookup(line %d): nocache cp "
								"0x%p on error %d from FRONT_VOP_LINK\n",
								__LINE__, cp, error));
						cachefs_nocache(cp);
					}
				}
				error = 0;
				*bdpp = CTOBHV(cp);
#ifdef DEBUG
			} else {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachemiss_lookup: makecachefsnode error "
						"EEXIST\n"));
#endif
			}
		default:
			if (fhp) {
				VOP_FID2(front_vp, &frontfid, fiderr);
				if (!fiderr) {
					CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
				} else {
					CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
				}
			}
	}
	if (did_lookup) {
		VN_RELE(back_vp);
		back_vp = NULL;
		freefid(backfid);
		backfid = NULL;
	}
out:
	ASSERT(!fhp);
	ASSERT(error || *bdpp);
	ASSERT(!error || !*bdpp);
	ASSERT(!error || !cp);
	ASSERT(!*bdpp || !BHVTOC(*bdpp)->c_fileheader ||
		valid_file_header(BHVTOC(*bdpp)->c_fileheader, backfid));
	if (front_vp)
		VN_RELE(front_vp);
	if (frontdir_vp)
		VN_RELE(frontdir_vp);
	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("cachemiss_lookup: EXIT vp 0x%p error = %d\n", *bdpp, error));
	return(error);
}

int
cachehit_lookup(cnode_t *dcp, char *nm, struct vnode *front_vp,
	bhv_desc_t **bdpp, int flags, struct vnode *rdir, cred_t *cr,
	int *lockstate, vnode_t *back_vp, fid_t *bfidp)
{
	fid_t frontfid;
	int cnflags = 0;
	int error = 0;
	int fiderr = 0;
	fid_t *backfid = NULL;
	cnode_t *cp = NULL;
	fileheader_t *fhp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	vnode_t *frontdir_vp = NULL;

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("cachehit_lookup: ENTER dcp 0x%p front vp 0x%p nm %s\n",
			dcp, front_vp, nm));
	ASSERT(front_vp);
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachehit_lookup, current_pid(),
		dcp, nm);
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(front_vp));
	ASSERT(VALID_ADDR(bdpp));
	ASSERT(!rdir || VALID_ADDR(rdir));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(lockstate));
	/*
	 * Cache hit.
	 * We have the front file, so read the file header.
	 * The expected case here is that the file exists on
	 * both the front and the back file systems.  This
	 * must, however, be verified.  The shortest route is
	 * to use the back file ID from the file header,
	 * passing that to cachefs_getbackvp.  This may not
	 * be required to go over the wire.
	 * A VFS_VGET call or a lookup in the back FS would
	 * be necessary anyway, since the back file vnode is
	 * required.
	 */
	error = cachefs_read_file_header(front_vp, &fhp, VNON, fscp->fs_cache);
	if (!error) {
		CACHEFS_STATS->cs_shortmiss++;
		error = 0;
		CACHEFS_STATS->cs_lookuphit++;
		backfid = &fhp->fh_metadata.md_backfid;
		/*
		 * Match the back file ID supplied by the caller to the one in
		 * the file header.  If they do not match, remove the front
		 * file and return ESTALE.  The lookup will be treated as
		 * a cache miss.
		 */
		if (bfidp && !FID_MATCH(backfid, bfidp)) {
			ASSERT(back_vp);
			(void)cachefs_remove_frontfile(dcp->c_frontdirvp, nm);
			if (back_vp->v_type == VDIR) {
				/*
				 * remove the front directory and all of its
				 * contents
				 */
				(void)cachefs_remove_frontdir(fscp, backfid, 1);
			}
			VOP_FID2(front_vp, &frontfid, fiderr);
			if (!fiderr) {
				CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
			} else {
				CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
			}
			return(ESTALE);
		}
		cnflags = CN_UPDATED;
		if (fhp->fh_metadata.md_attributes.ca_type == VDIR) {
			error = cachefs_lookup_frontdir(fscp->fs_cacheidvp,
				backfid, &frontdir_vp);
			if (error == ENOENT) {
				error = cachefs_create_frontdir(fscp->fs_cache,
					fscp->fs_cacheidvp, backfid, &frontdir_vp);
				if (error) {
					cnflags |= CN_NOCACHE;
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachehit_lookup(line %d): nocache %s "
						"on error %d from cachefs_create_frontdir\n",
						__LINE__, nm, error));
					if (fhp) {
						VOP_FID2(front_vp, &frontfid, fiderr);
						if (!fiderr) {
							CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
						} else {
							CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
						}
					}
					front_vp = NULL;
				}
			} else if (error) {
				cnflags |= CN_NOCACHE;
				CFS_DEBUG(CFSDEBUG_NOCACHE,
					printf("cachehit_lookup(line %d): nocache %s "
					"on error %d from cachefs_lookup_frontdir\n",
					__LINE__, nm, error));
				VOP_FID2(front_vp, &frontfid, fiderr);
				if (!fiderr) {
					CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
				} else {
					CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
				}
				front_vp = NULL;
			}
		}
		ASSERT(cachefs_zone_validate((caddr_t)fhp,
			FILEHEADER_BLOCK_SIZE(fscp->fs_cache)));
		error = makecachefsnode( dcp, backfid, fscp, fhp,
			front_vp, frontdir_vp, back_vp, (vattr_t *)NULL, cr,
			cnflags, &cp );
		switch ( error ) {
			case 0:
				*bdpp = CTOBHV(cp);
				/*
				 * do not free the file header
				 * it has been transferred to the cnode
				 */
				fhp = NULL;
				break;
			case ESTALE:
				ASSERT(!cp);
				if (dcp->c_frontdirvp ||
					!(error = cachefs_getfrontdirvp(dcp))) {
						(void)cachefs_remove_frontfile(dcp->c_frontdirvp, nm);
						(void)cachefs_remove_frontdir(fscp, backfid, 0);
				} else {
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachehit_lookup(line %d): nocache cp "
							"0x%p on error %d from "
							"cachefs_getfrontdirvp\n", __LINE__,
							dcp, error));
					cachefs_nocache(dcp);
				}
				if (front_vp) {
					/*
					 * Only free the file header if makecachefsnode
					 * returns an error.  Otherwise, the file header
					 * has been transferred to the cnode created.
					 */
					VOP_FID2(front_vp, &frontfid, fiderr);
					if (!fiderr) {
						CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
					} else {
						CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
					}
				}
				break;
			case EEXIST:
				if (cp) {
					error = 0;
					ASSERT(fhp != cp->c_fileheader);
					if (dcp->c_frontdirvp ||
						!(error = cachefs_getfrontdirvp(dcp))) {
							(void)cachefs_remove_frontfile(dcp->c_frontdirvp,
								nm);
							(void)cachefs_remove_frontdir(fscp, backfid, 0);
					} else {
						CFS_DEBUG(CFSDEBUG_NOCACHE,
							printf("cachehit_lookup(line %d): nocache "
								"cp 0x%p on error %d from "
								"cachefs_getfrontdirvp\n", __LINE__,
								cp, error));
						cachefs_nocache(cp);
					}
					if (C_CACHING(cp)) {
						error = FRONT_VOP_LINK(dcp, cp, nm, sys_cred);
						if (error) {
							cnflags |= CN_NOCACHE;
							CFS_DEBUG(CFSDEBUG_NOCACHE,
								printf("cachehit_lookup(line %d): "
									"nocache cp 0x%p on error %d from "
									"FRONT_VOP_LINK\n", __LINE__,
									cp, error));
							error = 0;
							if (fhp) {
								VOP_FID2(front_vp, &frontfid, fiderr);
								if (!fiderr) {
									CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
								} else {
									CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
								}
							}
							front_vp = NULL;
						}
					} else if (error) {
						cnflags |= CN_NOCACHE;
						CFS_DEBUG(CFSDEBUG_NOCACHE,
							printf("cachehit_lookup(line %d): nocache %s "
							"on error %d from cachefs_lookup_frontdir\n",
							__LINE__, nm, error));
						VOP_FID2(front_vp, &frontfid, fiderr);
						if (!fiderr) {
							CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
						} else {
							CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
						}
						front_vp = NULL;
					}
					*bdpp = CTOBHV(cp);
#ifdef DEBUG
				} else {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachehit_lookup: makecachefsnode "
							"error EEXIST\n"));
#endif
				}
				/*
				 * Fall through to the default case to free the
				 * file header.
				 */
			default:
				/*
				 * Only free the file header if makecachefsnode
				 * returns an error.  Otherwise, the file header
				 * has been transferred to the cnode created.
				 * We will only have a file header if we also
				 * have a front_vp.
				 */
				if (front_vp) {
					VOP_FID2(front_vp, &frontfid, fiderr);
					if (!fiderr) {
						CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
					} else {
						CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
					}
				}
		}
		backfid = NULL;
	} else {
		CACHEFS_STATS->cs_fronterror++;
		ASSERT(!fhp);
		/*
		 * We could not read the file header.
		 */
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachehit_lookup(line %d): "
				"error reading file header, file %s: %d\n",
				__LINE__, nm, error));
		if (*lockstate == RW_READER) {
			if (!rw_tryupgrade(&dcp->c_rwlock)) {
				rw_exit(&dcp->c_rwlock);
				rw_enter(&dcp->c_rwlock, RW_WRITER);
				*lockstate = RW_WRITER;
				error = EAGAIN;
				goto out;
			} else {
				*lockstate = RW_WRITER;
			}
		}
		if (dcp->c_frontdirvp || !(error = cachefs_getfrontdirvp(dcp))) {
			(void)cachefs_remove_frontfile(dcp->c_frontdirvp, nm);
			error = cachemiss_lookup(dcp, nm, bdpp, flags, rdir, cr, lockstate,
				back_vp, bfidp);
		} else {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachehit_lookup(line %d): nocache cp 0x%p on error %d "
					"from cachefs_getfrontdirvp\n", __LINE__, dcp, error));
			cachefs_nocache(dcp);
			error = nocache_lookup(dcp, nm, bdpp, flags, rdir, cr);
		}
	}
out:
	ASSERT(error || *bdpp);
	ASSERT(!error || !cp);
	ASSERT(!error || !*bdpp);
	ASSERT(!fhp);
	ASSERT(!*bdpp || !BHVTOC(*bdpp)->c_fileheader ||
		valid_file_header(BHVTOC(*bdpp)->c_fileheader, NULL));
	if (frontdir_vp)
		VN_RELE(frontdir_vp);
	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("cachehit_lookup: EXIT vp 0x%p error = %d\n", *bdpp, error));
	return(error);
}

/*
 * Assumes that c_rwlock is held as reader.
 */
/* ARGSUSED */
int
cachefs_lookup_locked(bhv_desc_t *dbdp, char *nm, bhv_desc_t **bdpp,
	struct pathname *pnp, int flags, struct vnode *rdir, cred_t *cr,
	int lockstate, int dnlc)
{
	enum backop_stat status;
	int error;
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	cnode_t *dcp = BHVTOC(dbdp);
	cnode_t *cp; 
	vnode_t *front_vp = NULL;
	vnode_t *vp;
	bhv_desc_t *bdp;
	vnode_t *back_vp = NULL;
	fid_t *backfid = NULL;

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("cachefs_lookup_locked: ENTER dvp %p nm %s\n", dvp, nm));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_lookup_locked,
		current_pid(), dvp, nm);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(bdpp));
	ASSERT(!pnp || VALID_ADDR(pnp));
	ASSERT(!rdir || VALID_ADDR(rdir));
	ASSERT(VALID_ADDR(cr));
	ASSERT((lockstate == RW_READER) || (lockstate == RW_WRITER));
	CACHEFS_STATS->cs_lookups++;
	*bdpp = NULL;
	/*
	 * If lookup is for ".", just return dvp.  Don't need
	 * to send it over the wire or look it up in the dnlc.
	 * Null component is a synonym for current directory.
	 *
	 * For lookups of "..", we save the parent vnode in the
	 * cnode.  The root cnode will always have a NULL parent
	 * vnode pointer.  This is acceptable because lookup of
	 * ".." at the root of a file system is handled in the
	 * file system independent lookup code.
	 *
	 * Also, cnodes created from cachefs_vget will have a
	 * NULL parent vnode pointer.  This will not cause any
	 * difficulty because we only use this pointer here and
	 * in order to get here to be looking up "..", we must
	 * have already looked up the current directory.  The
	 * parent vnode will have been filled in at that time.
	 */
	if (strcmp(nm, ".") == 0 || *nm == '\0') {
		VN_HOLD(dvp);
		CNODE_TRACE(CNTRACE_HOLD, dcp, (void *)cachefs_lookup_locked,
			dvp->v_count, 0);
		*bdpp = dbdp;
		/*
		 * Count lookups of "." as a cache hit.  The reasoning for this
		 * is that no lookup in the back file system will be generated.
		 */
		CACHEFS_STATS->cs_lookuphit++;
		ASSERT(!issplhi(getsr()));
		return( 0 );
	} else if (strcmp(nm, "..") == 0) {
		ASSERT(dcp->c_dcp);
		error = makecachefsnode(NULL, dcp->c_dcp->c_backfid, C_TO_FSCACHE(dcp),
			NULL, NULL, NULL, NULL, NULL, cr, 0, &cp);
		ASSERT(cp == dcp->c_dcp);
		if (!error) {
			*bdpp = CTOBHV(cp);
		}
		ASSERT(!error || (error == ESTALE));
		return(error);
	}

again:
	/*
	 * Make sure error is 0 here so that we do not return a bogus error
	 * value.  Not clearing error here can cause us to return an error
	 * AND a non-NULL *vpp.  That results in vnodes being held but not
	 * released.
	 */
	error = 0;
	/*
	 * The first place to look is the DNLC.  If that fails, the next place
	 * to try is the cnode hash table.  If that fails, do the lookup the
	 * long way by going to the front and back file systems.
	 */
	bdp = dnlc_lookup( dvp, nm, cr, 0 );
	if ( bdp ) {
		/*
		 * dnlc_enter is always done for cachefs vnodes (i.e., before
		 * calling spec_vp).  ASSERT that this is a cachefs vnode.
		 */
		vp = BHV_TO_VNODE(bdp);
		ASSERT( (vp)->v_vfsp->vfs_fstype == cachefsfstyp );
		cp = BHVTOC(bdp);
		/*
		 * If the cnode we found is CN_DESTROY the file may have been
		 * removed.  There is a race with fscache_sync for cnodes being
		 * purged.  fscache_sync may mark a cnode to be purged but race
		 * with dnlc_lookup above.  dnlc_lookup cannot be called while
		 * holding fs_cnodelock.  What may happen is that vn_purge
		 * (called by facache_sync) may fail.  This leaves the cnode out
		 * of the hash and marked CN_DESTROY.
		 */
		ASSERT(C_TO_FSCACHE(cp));
		if ( cp->c_flags & CN_DESTROY ) {
			VN_RELE(vp);
			CNODE_TRACE(CNTRACE_HOLD, cp, (void *)cachefs_lookup_locked,
				(vp)->v_count, 0);
			*bdpp = NULL;
			dnlc_remove(dvp, nm);
			error = ENOENT;
			CACHEFS_STATS->cs_race.cr_destroy++;
		} else {
			*bdpp = bdp;
			CNODE_TRACE(CNTRACE_ACT, cp, (void *)cachefs_lookup_locked,
				cp->c_flags, (vp)->v_count);
		}
	} else {
		error = ENOENT;
	}
	if (error == ENOENT) {
		/*
		 * Didn't get a dnlc hit. We have to search the directory.
		 * We should not need to cnode state lock for the directory to do this
		 * search.  This is because we will not be updating the directory.
		 */
		if ( C_CACHING( dcp ) ) {
			ASSERT(dcp->c_frontdirfid.fid_len &&
				(dcp->c_frontdirfid.fid_len <= MAXFIDSZ));
			front_vp = NULL;
			error = FRONT_VOP_LOOKUP( dcp, nm, &front_vp, NULL, flags,
				rdir, sys_cred);
			switch ( error ) {
				case ENOENT:	/* cache miss or non-existent file */
					ASSERT(!front_vp);
					CACHEFS_STATS->cs_lookupmiss++;
					error = cachemiss_lookup(dcp, nm, bdpp, flags, rdir, cr,
						&lockstate, NULL, NULL);
					ASSERT(!issplhi(getsr()));
					break;
				case 0:			/* cache hit but possibly stale */
					ASSERT(front_vp);
					/*
					 * We look the file up in the back file system here
					 * to catch the case where another client or a process
					 * on the server has renamed the file.
					 */
					status = cachefs_lookup_back(dcp, nm, &back_vp, &backfid,
						flags, rdir, cr, C_ISFS_DISCONNECT(C_TO_FSCACHE(dcp)) ?
						BACKOP_NONBLOCK : BACKOP_BLOCK, &error);
					if (status == BACKOP_NETERR) {
						error = 0;
					}
					if (!error) {
						error = cachehit_lookup(dcp, nm, front_vp, bdpp, flags,
							rdir, cr, &lockstate, back_vp, backfid);
						switch (error) {
							case ESTALE:
								CACHEFS_STATS->cs_lookupmiss++;
								error = cachemiss_lookup(dcp, nm, bdpp, flags,
									rdir, cr, &lockstate, back_vp, backfid);
								break;
							case 0:
							default:
								break;
						}
						if (backfid) {
							freefid(backfid);
							backfid = NULL;
						}
						if (back_vp) {
							VN_RELE(back_vp);
							back_vp = NULL;
						}
					} else {
						(void)cachefs_remove_frontfile(dcp->c_frontdirvp, nm);
					}
					VN_RELE(front_vp);
					ASSERT(!issplhi(getsr()));
					break;
				default:		/* some other error */
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_lookup_locked(line %d): front "
							"directory lookup error, file %s: %d\n",
							__LINE__, nm, error));
					ASSERT(!front_vp);
					CACHEFS_STATS->cs_lookupmiss++;
					error = cachemiss_lookup(dcp, nm, bdpp, flags, rdir, cr,
						&lockstate, NULL, NULL);
					ASSERT(!issplhi(getsr()));
					break;
			}
			/*
			 * If we got EAGAIN from cachemiss_lookup or cachehit_lookup,
			 * that means that c_rwlock was released and reacquired as
			 * a writer.  Since it was released, we need to start the
			 * lookup all over.
			 */
			if (error == EAGAIN) {
				goto again;
			}
			ASSERT(!back_vp && !backfid);
		} else {
			error = nocache_lookup(dcp, nm, bdpp, flags, rdir, cr);
		}
		if (dnlc && !error && C_CACHING(BHVTOC(*bdpp))) {
			dnlc_enter(dvp, nm, *bdpp, cr);
		}
	} else {
		/*
		 * Successful DNLC lookups come here.  These count as cache lookup
		 * hits.  This is because the front and back files are held as long
		 * as the cnode exists.
		 */
		CACHEFS_STATS->cs_lookuphit++;
		CACHEFS_STATS->cs_dnlchit++;
	}
	ASSERT(!issplhi(getsr()));
	ASSERT(error || *bdpp);
	ASSERT( (*bdpp == NULL) || ((BHV_TO_VNODE(*bdpp))->v_vfsp->vfs_fstype == cachefsfstyp) );
	ASSERT((*bdpp == NULL) || !BHVTOC(*bdpp)->c_backvp ||
		((BHV_TO_VNODE(*bdpp))->v_type == BHVTOC(*bdpp)->c_backvp->v_type));
#ifdef DEBUG
	if (!error) {
		CNODE_TRACE(CNTRACE_LOOKUP, BHVTOC(*bdpp), (void *)cachefs_lookup_locked,
			(BHV_TO_VNODE(*bdpp))->v_count, 0);
	}
#endif

#ifdef DEBUG
	if (!error && ((BHV_TO_VNODE(*bdpp))->v_vfsp->vfs_fstype == cachefsfstyp)) {
		cp = BHVTOC(*bdpp);
		CFS_DEBUG(CFSDEBUG_VALIDATE,
			validate_fileheader(cp, __FILE__, __LINE__));
		ASSERT((BHV_TO_VNODE(*bdpp))->v_count > 0);
		ASSERT(!cp->c_backvp || (cp->c_backvp->v_count > 0));
		ASSERT(!cp->c_frontvp || (cp->c_frontvp->v_count > 0));
		ASSERT(!cp->c_frontdirvp || (cp->c_frontdirvp->v_count > 0));
		if (*bdpp && C_CACHING(cp)) {
			ASSERT(cp->c_inhash);
			ASSERT(cp->c_frontfid.fid_len &&
				(cp->c_frontfid.fid_len <= MAXFIDSZ));
			if ((BHV_TO_VNODE(*bdpp))->v_type == VDIR) {
				ASSERT(cp->c_frontdirfid.fid_len &&
					(cp->c_frontdirfid.fid_len <= MAXFIDSZ));
			}
		}
	}
#endif
	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("cachefs_lookup_locked: EXIT nm %s bdp 0x%p error = %d\n", nm,
			*bdpp, error));
	ASSERT(!issplhi(getsr()));
	return( error );
}

/* ARGSUSED */
int
cachefs_lookup(bhv_desc_t *dbdp, char *nm, struct vnode **vpp,
	struct pathname *pnp, int flags, struct vnode *rdir, cred_t *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	cnode_t *dcp = BHVTOC(dbdp);
	bhv_desc_t *bdp;
	int error = 0;

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("cachefs_lookup: ENTER dvp %p nm %s\n", dvp, nm));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_lookup, current_pid(),
		dvp, nm);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(vpp));
	ASSERT(!pnp || VALID_ADDR(pnp));
	ASSERT(!rdir || VALID_ADDR(rdir));
	ASSERT(VALID_ADDR(cr));
	ASSERT(!issplhi(getsr()));
	CACHEFS_STATS->cs_vnops++;
	/* VOP_ACCESS will verify consistency for us */
	VOP_ACCESS(dvp, VEXEC, cr, error);
	if (error) {
		ASSERT(!issplhi(getsr()));
		return( error );
	}
	rw_enter( &dcp->c_rwlock, RW_READER );
	error = cachefs_lookup_locked( dbdp, nm, &bdp, pnp, flags, rdir, cr,
		RW_READER, 1 );
	rw_exit( &dcp->c_rwlock );
	if (!error) {
		*vpp = BHV_TO_VNODE(bdp);
		if C_ISVDEV((*vpp)->v_type) {
			struct vnode *newvp;
			struct vnode *oldvp;

			oldvp = *vpp;
			newvp = spec_vp(oldvp, oldvp->v_rdev,
					oldvp->v_type, cr);

			VN_RELE(oldvp);
			CNODE_TRACE(CNTRACE_HOLD, BHVTOC(bdp), (void *)cachefs_lookup,
				    oldvp->v_count, 0);
			if (newvp == NULL) {
				error = ENOSYS;
				*vpp = NULL;
			} else {
				*vpp = newvp;
			}
		}
	} else {
		*vpp = NULL;
	}
	ASSERT(!issplhi(getsr()));
	ASSERT(error || *vpp);
	ASSERT(!error || !*vpp);
	ASSERT(error != EAGAIN);
	ASSERT(error || VALID_ADDR(*vpp));

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_LOOKUP,
		printf("cachefs_lookup: EXIT error = %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_lookup: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_lookup: EXIT dvp %p nm %s error %d\n", dvp, nm, error));
	return (error);
}

int
cachefs_create(bhv_desc_t *dbdp, char *nm, struct vattr *vap,
	int exclusive, int mode, struct vnode **vpp, cred_t *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	fid_t frontfid;
	int cnflag = 0;
	fileheader_t *fhp = NULL;
	cnode_t *cp, *dcp = BHVTOC(dbdp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error = 0;
	int terr = 0;
	vnode_t *tvp = NULL;
	struct vnode *devvp = NULL;
	int vamask;
	vnode_t *front_vp = NULL;
	vnode_t *back_vp = NULL;
	fid_t *backfid = NULL;
	bhv_desc_t *tbdp;

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_create: ENTER dvp %p excl %d\n", dvp, exclusive));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_create, current_pid(),
		dvp, nm);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(vap));
	ASSERT(VALID_ADDR(vpp));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	if (vap->va_type == VDIR) {
		error = EISDIR;
		goto out;
	}

	rw_enter( &dcp->c_rwlock, RW_WRITER );
	error = cachefs_lookup_locked(dbdp, nm, &tbdp, NULL, 0, NULL, cr,
		RW_WRITER, 1);
	ASSERT(!issplhi(getsr()));
	switch (error) {
		case 0:
			rw_exit( &dcp->c_rwlock );
			tvp = BHV_TO_VNODE(tbdp);
			/*
			 * The file exists.  If this is an exclusive create, return
			 * EEXIST.  If the file is a directory and the caller has
			 * specified VWRITE in the create mode, return EISDIR.
			 * Let cachefs_access handle any further access checks.
			 */
			if (exclusive) {
				error = EEXIST;
			} else if ((tvp->v_type == VDIR) && (mode & VWRITE)) {
				error = EISDIR;
			} else if (mode) {
				VOP_ACCESS(tvp, mode, cr, error);
			}
			/*
			 * If the caller has requested that the size be set and this is
			 * a regular file and no error has occurred above, then set the
			 * size.  Use cachefs_setattr to correctly set the size.
			 * Temporarily set va_mask to AT_SIZE so that only the size is
			 * changed.  cachefs_setattr will hadle the necessary flushing
			 * and correct setting of c_size.
			 */
			if (error) {
				VN_RELE(tvp);
				CNODE_TRACE(CNTRACE_HOLD, BHVTOC(tbdp), (void *)cachefs_create,
					tvp->v_count, 0);
			} else if ((tvp->v_type == VREG) && (vap->va_mask & AT_SIZE)) {
				vamask = vap->va_mask;
				vap->va_mask = AT_SIZE;
				VOP_SETATTR(tvp, vap, 0, cr, error);
				vap->va_mask = vamask;
				if ( !error ) {
					*vpp = tvp;
				} else {
					VN_RELE(tvp);
					CNODE_TRACE(CNTRACE_HOLD, BHVTOC(tbdp), (void *)cachefs_create,
						tvp->v_count, 0);
				}
			} else {
				*vpp = tvp;
			}
			break;
		case ENOENT:
			/*
			 * Do a non-exclusive create for the back file.
			 */
			(void)BACK_VOP_CREATE(dcp, nm, vap, 0, mode,
				&devvp, cr, BACKOP_BLOCK, &error);
			if (error) {
				rw_exit( &dcp->c_rwlock );
				break;
			}
			VOP_REALVP(devvp, &back_vp, terr);
			if (terr == 0) {
				VN_HOLD(back_vp);
				VN_RELE(devvp);
			} else {
				back_vp = devvp;
			}
			(void)cachefs_update_directory(dcp, 0, cr);
			VOP_FID(back_vp, &backfid, error);
			if (error || (backfid == NULL)) {
				rw_exit( &dcp->c_rwlock );
				if (!error) {
					error = ESTALE;
				}
				break;
			}
create_front:
			/*
			 * The file does not exist.  Go to the front file system to
			 * create it.
			 */
			if ( C_CACHING( dcp ) ) {
				ASSERT(dcp->c_frontdirfid.fid_len &&
					(dcp->c_frontdirfid.fid_len <= MAXFIDSZ));
				if (dcp->c_frontdirvp ||
					!(error = cachefs_getfrontdirvp(dcp))) {
						error = cachefs_create_frontfile(fscp->fs_cache,
							dcp->c_frontdirvp, nm, &front_vp, backfid, &fhp,
							(vap->va_mask & AT_TYPE) ? vap->va_type : VREG);
				} else {
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachefs_create(line %d): nocache cp 0x%p on "
							"error %d from cachefs_getfrontdirvp\n", __LINE__,
							dcp, error));
					cachefs_nocache(dcp);
				}
				if (error) {
					ASSERT(!fhp);
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_create(line %d): front file create "
							"error, file %s: %d\n", __LINE__, nm, error));
					error = 0;
					cnflag = CN_NOCACHE;
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachefs_create(line %d): nocache %s "
						"on error %d\n", __LINE__, nm, error));
				}
			}
			/*
			 * We cannot pass vap to makecachefsnode since it is not
			 * complete.
			 */
makenode:
			error = makecachefsnode( dcp, backfid, fscp, fhp, front_vp,
				(vnode_t *)NULL, back_vp, NULL, cr, cnflag, &cp );
			ASSERT(!issplhi(getsr()));
			switch (error) {
				case 0:
					*vpp = CTOV(cp);
					fhp = NULL;
					break;
				case EEXIST:
					if (cp) {
						ASSERT(fhp != cp->c_fileheader);
						ASSERT(!back_vp ||
							(back_vp->v_type == CTOV(cp)->v_type));
						*vpp = CTOV(cp);
						error = 0;
					}
					cnflag = CN_NOCACHE;
				case ESTALE:
					if (!(cnflag & CN_NOCACHE)) {
						/*
						 * stale front file, remove it and create a new one
						 */
						if (front_vp)
							VN_RELE(front_vp);
						error = cachefs_remove_frontfile(dcp->c_frontdirvp, nm);
						if (!error) {
							goto create_front;
						}
						cnflag = CN_NOCACHE;
						goto makenode;
					}
				default:
					if (C_CACHING(dcp)) {
						if (dcp->c_frontdirvp ||
							!(error = cachefs_getfrontdirvp(dcp))) {
								if (cachefs_remove_frontfile(dcp->c_frontdirvp,
									nm)) {
										cachefs_inval_object( dcp,
											WRITELOCKED );
								}
						} else {
							CFS_DEBUG(CFSDEBUG_NOCACHE,
								printf("cachefs_create(line %d): nocache cp "
									"0x%p on error %d from "
									"cachefs_getfrontdirvp\n",
									__LINE__, dcp, error));
							cachefs_nocache(dcp);
						}
					}
					switch ( BACK_VOP_REMOVE( dcp, nm, cr, BACKOP_BLOCK,
						&error ) ) {
							case BACKOP_SUCCESS:
								break;
							case BACKOP_NETERR:
							default:
								cachefs_inval_object( dcp, WRITELOCKED );
					}
					if (fhp) {
						VOP_FID2(front_vp, &frontfid, terr);
						if (!terr) {
							CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
						} else {
							CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
						}
					}
					break;
			}
			rw_exit( &dcp->c_rwlock );
			ASSERT(error || *vpp);
			if (!error && C_CACHING(cp)) {
				dnlc_enter(dvp, nm, CTOBHV(cp), cr);
			}
			break;

		default:
			rw_exit( &dcp->c_rwlock );
	}
	ASSERT(!issplhi(getsr()));
out:
	ASSERT(!fhp);
	if (backfid) {
		freefid(backfid);
	}
	if ( back_vp ) {
		VN_RELE(back_vp);
	}
	if ( front_vp ) {
		VN_RELE( front_vp );
	}

	if (error == 0 && C_ISVDEV((*vpp)->v_type)) {
		struct vnode *newvp;

		newvp = spec_vp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type, cr);

		VN_RELE(*vpp);
		if (newvp == NULL) {
			error = ENOSYS;
		} else {
			*vpp = newvp;
		}
	}

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_create: EXIT dvp %p error %d\n", dvp, error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_create: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_create: EXIT dvp %p nm %s error %d\n", dvp,
			nm, error));

	return (error);
}

int
cachefs_remove(bhv_desc_t *dbdp, char *nm, cred_t *cr)
{
	/*REFERENCED*/
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	cnode_t *cp, *dcp = BHVTOC(dbdp);
	vnode_t *vp = NULL;
	int error = 0;
	bhv_desc_t *bdp;
	int ospl;
	nlink_t nlink;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_remove: ENTER dvp %p name %s \n", dvp, nm));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_remove, current_pid(),
		dvp, nm);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	rw_enter( &dcp->c_rwlock, RW_READER );
	error = cachefs_lookup_locked(dbdp, nm, &bdp, (struct pathname *)NULL, 0,
				(vnode_t *)NULL, cr, RW_READER, 1);
	rw_exit( &dcp->c_rwlock );
	ASSERT(!issplhi(getsr()));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_remove: EXIT ESTALE\n"));
	if (error) {
		ASSERT(!issplhi(getsr()));
		CFS_DEBUG(CFSDEBUG_ERROR2,
			printf("cachefs_remove: EXIT dvp %p error %d\n", dvp,
				error));
		return (error);
	}

	vp = BHV_TO_VNODE(bdp);

	cp = BHVTOC(bdp);

	ASSERT(!issplhi(getsr()));
	rw_enter( &dcp->c_rwlock, RW_WRITER );
	ASSERT(!issplhi(getsr()));

	if (C_CACHING(cp)) {
		ASSERT(cp->c_attr);
		nlink = cp->c_attr->ca_nlink;
	} else {
		/*
		 * We use nlink below to determine whether or not to
		 * sync the file header.  Since the file is not being
		 * cached, we don't bother syncing the file header.
		 */
		nlink = 1;
	}

	/* determine if the cnode is about to go inactive */
	dnlc_purge_vp(vp);

	/*
	 * Call VOP_REMOVE(BackFS). Remove the directory entry from the
	 * cached directory.
	 */
	(void)BACK_VOP_REMOVE(dcp, nm, cr, BACKOP_BLOCK, &error);
	if (error) {
		cachefs_inval_object( dcp, WRITELOCKED );
		ASSERT(!issplhi(getsr()));
		goto out;
	}

	if ( C_CACHING( dcp ) ) {
		ASSERT(dcp->c_frontdirfid.fid_len &&
			(dcp->c_frontdirfid.fid_len <= MAXFIDSZ));
		if (dcp->c_frontdirvp || !(error = cachefs_getfrontdirvp(dcp))) {
			(void)cachefs_remove_frontfile( dcp->c_frontdirvp, nm );
		} else {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_remove(line %d): nocache cp 0x%p on "
					"error %d from cachefs_getfrontdirvp\n", __LINE__,
					dcp, error));
			cachefs_nocache(dcp);
			error = 0;
		}
	}

	/*
	 * the file may have been a link, so invalidate the cnode to get the
	 * new attributes
	 */
	CFSOP_INVALIDATE_COBJECT(C_TO_FSCACHE(cp), cp, cr);

	/*
	 * sync here to make sure the correct file header data is written
	 * but only do this if the link count was greater than 1
	 */
	if (nlink > 1) {
		VOP_FSYNC(vp, FSYNC_WAIT, cr, (off_t)0, (off_t)-1, error);
		error = 0;
		cachefs_write_header(cp, 0);
	}
	ospl = mutex_spinlock( &cp->c_statelock );
	cp->c_flags |= CN_DESTROY;
	CNODE_TRACE(CNTRACE_DESTROY, cp, (void *)cachefs_remove, cp->c_flags, 0);
	mutex_spinunlock( &cp->c_statelock, ospl );

	/*
	 * The directory has been modified, so inform the consistency module
	 */
	(void)cachefs_update_directory(dcp, 0, cr);
	ASSERT(!issplhi(getsr()));

out:
	rw_exit( &dcp->c_rwlock );
	ASSERT(!issplhi(getsr()));
	VN_RELE(vp);
	CNODE_TRACE(CNTRACE_HOLD, BHVTOC(bdp), (void *)cachefs_remove,
		vp->v_count, 0);
	ASSERT(!issplhi(getsr()));

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_remove: EXIT dvp %p\n", dvp));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_remove: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_remove: EXIT dvp %p error %d\n", dvp, error));
	return (error);
}

int
cachefs_link(bhv_desc_t *tdbdp, struct vnode *svp, char *tnm, cred_t *cr)
{
	/*REFERENCED*/
	vnode_t *tdvp = BHV_TO_VNODE(tdbdp);
	cnode_t *cp, *tdcp = BHVTOC(tdbdp);
	fscache_t *fscp = C_TO_FSCACHE(tdcp);
	int error = 0;
	vnode_t *realvp;
	bhv_desc_t *sbdp;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_link: ENTER svp %p tdvp %p tnm %s \n",
			svp, tdvp, tnm));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_link, current_pid(),
		tdvp, svp);
	ASSERT(VALID_ADDR(tdvp));
	ASSERT(VALID_ADDR(svp));
	ASSERT(VALID_ADDR(tnm));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	VOP_REALVP(svp, &realvp, error);
	if (error == 0) {
		svp = realvp;
	}

	sbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(svp), &cachefs_vnodeops);
	if (sbdp == NULL) {
		return EXDEV;
	}
	cp = BHVTOC(sbdp);

	rw_enter(&tdcp->c_rwlock, RW_WRITER);
	error = CFSOP_CHECK_COBJECT(fscp, tdcp, (vattr_t *)NULL, cr, WRITELOCKED);
	if (error)
		goto out;

	(void)BACK_VOP_LINK(tdcp, cp, tnm, cr, BACKOP_BLOCK, &error);
	if (error) {
		goto out;
	}
	error = FRONT_VOP_LINK(tdcp, cp, tnm, sys_cred);
	if (error) {
		CACHEFS_STATS->cs_fronterror++;
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_link(line %d): error %d creating front link for "
				"file %s\n", __LINE__, error, tnm));
	}
	error = 0;

	CFSOP_MODIFY_COBJECT(fscp, cp, cr);
	(void)cachefs_update_directory(tdcp, 0, cr);
out:
	rw_exit(&tdcp->c_rwlock);

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_link: EXIT svp %p tdvp %p tnm %s \n",
			svp, tdvp, tnm));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_link: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_link: EXIT svp %p tdvp %p tnm %s error %d\n", svp,
			tdvp, tnm, error));

	return (error);
}

/*
 * Serialize all renames in CFS, to avoid deadlocks - We have to hold two
 * cnodes atomically.
 */
mutex_t cachefs_rename_lock;

int
cachefs_rename(bhv_desc_t *odbdp, char *onm, struct vnode *ndvp,
	char *nnm, struct pathname *pnp, cred_t *cr)
{
	/*REFERENCED*/
	vnode_t *odvp = BHV_TO_VNODE(odbdp);
	cnode_t *odcp = BHVTOC(odbdp);
	cnode_t *ndcp;
	int error = 0;
	vnode_t *nvp;
	cnode_t *ncp;
	vnode_t *ovp = NULL;
	int ospl;
	bhv_desc_t *ndbdp;
	bhv_desc_t *obdp;
	bhv_desc_t *nbdp;

	/*
	 * To avoid deadlock, we acquire this global rename lock before
	 * we try to get the locks for the source and target directories.
	 */
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_rename, current_pid(),
		odvp, onm);
	ASSERT(VALID_ADDR(odvp));
	ASSERT(VALID_ADDR(ndvp));
	ASSERT(VALID_ADDR(onm));
	ASSERT(VALID_ADDR(nnm));
	ASSERT(!pnp || VALID_ADDR(pnp));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;

	/*
	 * Find the cachefs behavior for the new directory.  If there
	 * isn't one, then it must be in another filesystem and we
	 * return an error.
	 */
	ndbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(ndvp), &cachefs_vnodeops);
	if (ndbdp == NULL) {
		return EXDEV;
	}
	ndcp = BHVTOC(ndbdp);

	mutex_enter(&cachefs_rename_lock);
	rw_enter(&odcp->c_rwlock, RW_WRITER);
	if (odcp != ndcp) {
		rw_enter(&ndcp->c_rwlock, RW_WRITER);
	}
	mutex_exit(&cachefs_rename_lock);

	/*
	 * No matter what, we need the vnode/cnode for the old file.
	 */
	obdp = NULL;
	error = cachefs_lookup_locked( odbdp, onm, &obdp, pnp, 0, (vnode_t *)NULL,
		cr, RW_WRITER, 0);
	if (error) {
		goto out;
	}
	ovp = BHV_TO_VNODE(obdp);
	if (obdp == ndbdp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * We have to sync the file being renamed so that we do not lose the
	 * data in the process.
	 */
	VOP_FSYNC(ovp, FSYNC_WAIT, cr, (off_t)0, (off_t)-1, error);
	if (error) {
		goto out;
	}
	/*
	 * Look up the new name in the new directory pointed to by ndvp.
	 */
	nbdp = NULL;
	error = cachefs_lookup_locked( ndbdp, nnm, &nbdp, pnp, 0, (vnode_t *)NULL,
		cr, RW_WRITER, 0);
	switch ( error ) {
		case 0:
			nvp = BHV_TO_VNODE(nbdp);
			/*
			 * The new name exists.  A rename will remove it.  We let the
			 * underlying file systems deal with that.  We must check
			 * for a mount point here, however.
			 *
			 * The order of operation is important here.  Once the file
			 * has been removed from the back file system, it cannot be
			 * recreated.  If the rename is successful on the back file
			 * system, no error should be returned to the user.
			 */
			if ( nvp->v_vfsmountedhere ) {
				error = EBUSY;
				goto out;
			}
			(void)BACK_VOP_RENAME( odcp, onm, ndcp, nnm,
				pnp, cr, BACKOP_BLOCK, &error);
			if ( error ) {
				goto out;
			}
			dnlc_purge_vp( nvp );
			dnlc_purge_vp( ovp );
			/*
			 * mark the new cnode to be destroyed
			 */
			ncp = BHVTOC(nbdp);
			ospl = mutex_spinlock( &ncp->c_statelock );
			ncp->c_flags |= CN_DESTROY;
			CNODE_TRACE(CNTRACE_DESTROY, ncp, (void *)cachefs_rename,
				ncp->c_flags, 0);
			mutex_spinunlock( &ncp->c_statelock, ospl );
			VN_RELE(nvp);
			CNODE_TRACE(CNTRACE_HOLD, ncp, (void *)cachefs_rename,
				nvp->v_count, 0);
			if ( C_CACHING( odcp ) && C_CACHING( ndcp ) ) {
				ASSERT(odcp->c_frontfid.fid_len &&
					(odcp->c_frontfid.fid_len <= MAXFIDSZ));
				ASSERT(ndcp->c_frontfid.fid_len &&
					(ndcp->c_frontfid.fid_len <= MAXFIDSZ));
				error = FRONT_VOP_RENAME( odcp, onm, ndcp, nnm, pnp, sys_cred);
				if ( error ) {
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_rename(line %d): front file rename "
							"error, file %s -> %s: %d\n", __LINE__,
							onm, nnm, error));
					error = 0;
					goto out;
				}
			}
			break;
		case ENOENT:
			/*
			 * No file with the name given in nnm exists in the directory
			 * ndvp.  Just go to the front and back file systems to do
			 * the rename.
			 *
			 * The order of operation is unimportant.  In the event of a
			 * crash, consistency will be checked and it will be noticed
			 * that there is a file on the back file system that is not
			 * on the front (and, possibly, vice-versa).
			 *
			 * What is important is that if the rename succeeds in the
			 * back file system, no error is returned to the user.  The
			 * front file system can always be made consistent at a later
			 * time.
			 */
			(void)BACK_VOP_RENAME( odcp, onm, ndcp, nnm,
				pnp, cr, BACKOP_BLOCK, &error);
			if ( error ) {
				goto out;
			}
			/*
			 * The back file rename has succeeded, purge the old vnode from
			 * the dnlc.
			 * If there is a failure during
			 * the next rename, the cache will be made consistent with the
			 * back file system.
			 */
			dnlc_purge_vp( ovp );
			if ( C_CACHING( odcp ) && C_CACHING( ndcp ) ) {
				error = FRONT_VOP_RENAME( odcp, onm, ndcp, nnm, pnp, sys_cred);
				if ( error ) {
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_rename(line %d): front file rename "
							"error, file %s -> %s: %d\n", __LINE__,
							onm, nnm, error));
					cachefs_inval_object( ndcp, WRITELOCKED );
					cachefs_inval_object( odcp, WRITELOCKED );
					error = 0;
				}
			}
			break;
		default:
			goto out;
	}

	/*
	 * Asynchronously update the directories.
	 */
	(void)cachefs_update_directory(odcp, 0, cr);
	if (odcp != ndcp) {
		(void)cachefs_update_directory(ndcp, 0, cr);
	}

out:
	if (ovp) {
		CNODE_TRACE(CNTRACE_HOLD, BHVTOC(obdp), (void *)cachefs_rename,
			ovp->v_count, 0);
		VN_RELE(ovp);
	}
	if (odcp != ndcp)
		rw_exit(&ndcp->c_rwlock);
	rw_exit(&odcp->c_rwlock);
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_rename: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_rename: EXIT onm %s nnm %s error %d\n", onm, nnm,
			error));
	return (error);
}

int
cachefs_mkdir(bhv_desc_t *dbdp, char *nm, register struct vattr *va,
	struct vnode **vpp, cred_t *cr)
{
	fid_t frontfid;
	int cnflags = 0;
	fileheader_t *fhp = NULL;
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	cnode_t *cp = NULL;
	cnode_t *dcp = BHVTOC(dbdp);
	struct vnode *front_vp = NULL;
	struct vnode *frontdir_vp = NULL;
	struct vnode *back_vp = NULL;
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	fid_t *backfid = NULL;

	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_mkdir: ENTER dvp %p\n", dvp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_mkdir, current_pid(),
		dvp, nm);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(va));
	ASSERT(VALID_ADDR(vpp));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	rw_enter(&dcp->c_rwlock, RW_WRITER);

	(void)BACK_VOP_MKDIR(dcp, nm, va, &back_vp, cr, BACKOP_BLOCK, &error);
	if (error) {
		goto out;
	}
	VOP_FID(back_vp, &backfid, error);
	if (error) {
		CACHEFS_STATS->cs_fronterror++;
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_mkdir(line %d): error getting back file ID, "
				"file %s: %d\n", __LINE__, nm, error));
		switch ( BACK_VOP_RMDIR(dcp, nm, curuthread->ut_cdir, cr, BACKOP_BLOCK, &error) ) {
			case BACKOP_SUCCESS:
				break;
			case BACKOP_NETERR:
			default:
				cachefs_inval_object( dcp, WRITELOCKED );
		}
		goto out;
	}
create_front:
	if ( C_CACHING( dcp ) ) {
		ASSERT(dcp->c_frontfid.fid_len &&
			(dcp->c_frontfid.fid_len <= MAXFIDSZ));
		if (dcp->c_frontdirvp || !(error = cachefs_getfrontdirvp(dcp))) {
			error = cachefs_create_frontfile(fscp->fs_cache, dcp->c_frontdirvp,
				nm, &front_vp, backfid, &fhp,
				(va->va_mask & AT_TYPE) ? va->va_type : VDIR);
		} else {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_mkdir(line %d): nocache cp 0x%p on "
					"error %d from cachefs_getfrontdirvp\n", __LINE__,
					dcp, error));
			cachefs_nocache(dcp);
		}
		if (error) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_mkdir(line %d): front file create error, "
					"file %s: %d\n", __LINE__, nm, error));
			cachefs_inval_object(dcp, WRITELOCKED);
			cnflags |= CN_NOCACHE;
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_mkdir(line %d): nocache %s "
				"on error %d\n", __LINE__, nm, error));
		} else {
			error = cachefs_create_frontdir(fscp->fs_cache, fscp->fs_cacheidvp,
				backfid, &frontdir_vp);
			switch (error) {
				case EEXIST:
					error = cachefs_lookup_frontdir(fscp->fs_cacheidvp,
						backfid, &frontdir_vp);
					if (error) {
						cnflags = CN_NOCACHE;
						CFS_DEBUG(CFSDEBUG_NOCACHE,
							printf("cachefs_mkdir(line %d): nocache %s "
							"on error %d from cachefs_lookup_frontdir\n",
							__LINE__, nm, error));
						if (fhp) {
							VOP_FID2(front_vp, &frontfid, error);
							if (!error) {
								CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
							} else {
								CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
							}
						}
						if (front_vp) {
							VN_RELE(front_vp);
							front_vp = NULL;
						}
					}
					break;
				case 0:
					break;
				default:
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_mkdir(line %d): front directory create "
							"error, file %s: %d\n", __LINE__, nm,
							error));
					if (dcp->c_frontdirvp ||
						!(error = cachefs_getfrontdirvp(dcp))) {
							(void)cachefs_remove_frontfile(dcp->c_frontdirvp,
								nm);
					} else {
						CFS_DEBUG(CFSDEBUG_NOCACHE,
							printf("cachefs_mkdir(line %d): nocache cp 0x%p on "
								"error %d from cachefs_getfrontdirvp\n",
								__LINE__, dcp, error));
						cachefs_nocache(dcp);
					}
					VN_RELE(front_vp);
					cnflags = CN_NOCACHE;
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachefs_mkdir(line %d): nocache %s "
						"on error %d from cachefs_create_frontdir\n",
						__LINE__, nm, error));
					if (fhp) {
						VOP_FID2(front_vp, &frontfid, error);
						if (!error) {
							CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
						} else {
							CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
						}
					}
					front_vp = NULL;
			}
		}
	}

	/*
	 * Create the cnode for the directory.  If this fails, we need to
	 * undo the mkdirs done above.  If either of these fail, we will need
	 * to mark the directory as invalid.  One problem is when the rmdir
	 * on the back FS fails.  In that case, another mkdir will fail because
	 * the directory already exists on the back FS.  This may happen when
	 * the back FS is NFS and is soft-mounted.
	 *
	 * We cannot pass va to makecachefsnode since it is not complete.
	 */
makenode:
	error = makecachefsnode( dcp, backfid, fscp, fhp, front_vp, frontdir_vp,
		back_vp, NULL, cr, cnflags, &cp );
	switch (error) {
		case 0:
			fhp = NULL;
			break;
		case EEXIST:
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_mkdir: makecachefsnode error EEXIST\n", error));
			if (cp) {
				ASSERT(fhp != cp->c_fileheader);
				VN_RELE(CTOV(cp));
				CNODE_TRACE(CNTRACE_HOLD, cp, (void *)cachefs_mkdir,
					CTOV(cp)->v_count, 0);
			}
			cnflags = CN_NOCACHE;
		case ESTALE:
			if (!(cnflags & CN_NOCACHE)) {
				/*
				 * stale front file, remove it and create a new one
				 */
				if (front_vp)
					VN_RELE(front_vp);
				if (frontdir_vp)
					VN_RELE(frontdir_vp);
				error = cachefs_remove_frontfile(dcp->c_frontdirvp, nm);
				if (!error) {
					goto create_front;
				}
				cnflags = CN_NOCACHE;
				goto makenode;
			}
		default:
			if (C_CACHING(dcp)) {
				if (dcp->c_frontdirvp ||
					!(error = cachefs_getfrontdirvp(dcp))) {
						if (cachefs_remove_frontfile(dcp->c_frontdirvp, nm)) {
							cachefs_inval_object( dcp, WRITELOCKED );
						}
						(void)cachefs_remove_frontdir(fscp, backfid, 0);
				} else {
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachefs_mkdir(line %d): nocache cp 0x%p on "
							"error %d from cachefs_getfrontdirvp\n",
							__LINE__, dcp, error));
					cachefs_nocache(dcp);
				}
			}
			switch ( BACK_VOP_RMDIR( dcp, nm, curuthread->ut_cdir, cr, BACKOP_BLOCK,
				&error ) ) {
					case BACKOP_SUCCESS:
						break;
					case BACKOP_NETERR:
					default:
						cachefs_inval_object( dcp, WRITELOCKED );
			}
			if (fhp) {
				VOP_FID2(front_vp, &frontfid, error);
				if (!error) {
					CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
				} else {
					CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
				}
			}
			goto out;
	}
	ASSERT(CTOV(cp)->v_type == VDIR);
	*vpp = CTOV(cp);
	dnlc_enter(dvp, nm, CTOBHV(cp), cr);
	(void)cachefs_update_directory(dcp, 0, cr);

out:

	ASSERT(!fhp);
	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_mkdir: EXIT error = %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_mkdir: EXIT ESTALE\n"));
	if (front_vp)
		VN_RELE(front_vp);
	if (frontdir_vp)
		VN_RELE(frontdir_vp);
	if (back_vp)
		VN_RELE(back_vp);
	if (backfid) {
		freefid(backfid);
	}

	rw_exit(&dcp->c_rwlock);

	return (error);
}

int
cachefs_rmdir(bhv_desc_t *dbdp, char *nm, struct vnode *cdir, cred_t *cr)
{
	/*REFERENCED*/
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	cnode_t *cp = NULL;
	cnode_t *dcp = BHVTOC(dbdp);
	vnode_t *vp = NULL;
	bhv_desc_t *bdp;
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int ospl;
	int cmp;
	nlink_t nlink;

	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_rmdir: ENTER vp %p\n", dvp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_rmdir, current_pid(),
		dvp, nm);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(cdir));
	ASSERT(VALID_ADDR(cr));
	ASSERT(!issplhi(getsr()));
	CACHEFS_STATS->cs_vnops++;
	rw_enter(&dcp->c_rwlock, RW_WRITER);
	error = cachefs_lookup_locked(dbdp, nm, &bdp, (struct pathname *)NULL, 0,
		(vnode_t *)NULL, cr, RW_WRITER, 0);
	if (error) {
		goto out1;
	}
	vp = BHV_TO_VNODE(bdp);
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out1;
	}
	cmp = VN_CMP(vp, cdir);
	if (cmp) {
		error = EINVAL;
		goto out1;
	}
	cp = BHVTOC(bdp);

	if (C_CACHING(cp)) {
		ASSERT(VALID_ADDR(cp->c_attr));
		nlink = cp->c_attr->ca_nlink;
	} else {
		nlink = 1;
	}

	(void)BACK_VOP_RMDIR(dcp, nm, cdir, cr, BACKOP_BLOCK, &error);
	if (error)
		goto out1;
	dnlc_purge_vp(vp);
	if ( C_CACHING( dcp ) ) {
		if (dcp->c_frontdirvp || !(error = cachefs_getfrontdirvp(dcp))) {
			error = cachefs_remove_frontfile(dcp->c_frontdirvp, nm);
			if (error) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_rmdir(line %d): front file remove "
						"error, file %s: %d\n", __LINE__, nm, error));
				error = 0;
			} else {
				error = cachefs_remove_frontdir(fscp, cp->c_backfid, 0);
				if (error) {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_rmdir(line %d): front directory remove "
							"error, file %s: %d\n", __LINE__, nm, error));
					error = 0;
				}
			}
		} else {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_rmdir(line %d): nocache cp 0x%p on "
					"error %d from cachefs_getfrontdirvp\n",
					__LINE__, dcp, error));
			cachefs_nocache(dcp);
		}
	}
	CFSOP_INVALIDATE_COBJECT(fscp, cp, cr);
	if (nlink > 1) {
		cachefs_write_header(cp, 0);
	}
	ospl = mutex_spinlock( &cp->c_statelock );
	cp->c_flags |= CN_DESTROY;
	CNODE_TRACE(CNTRACE_DESTROY, cp, (void *)cachefs_rmdir, cp->c_flags, 0);
	mutex_spinunlock( &cp->c_statelock, ospl );

	(void)cachefs_update_directory(dcp, 0, cr);
	ASSERT(!issplhi(getsr()));

out1:
	rw_exit(&dcp->c_rwlock);
	ASSERT(!issplhi(getsr()));
	if (vp) {
		VN_RELE(vp);
		CNODE_TRACE(CNTRACE_HOLD, cp, (void *)cachefs_rmdir,
			vp->v_count, 0);
	}
	ASSERT(!issplhi(getsr()));
	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_rmdir: EXIT error = %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_rmdir: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_rmdir: EXIT nm %s error %d\n", nm, error));

	return (error);
}

int
cachefs_symlink(bhv_desc_t *dbdp, char *lnm, struct vattr *tva,
	char *tnm, cred_t *cr)
{
	vattr_t *vap;
	fid_t frontfid;
	vnode_t *backvp = NULL;
	fid_t *fidp;
	int remerr = 0;
	/*REFERENCED*/
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	cnode_t *dcp = BHVTOC(dbdp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error = 0;
	int fiderr = 0;
	fileheader_t *fileheader = NULL;
	int len;
	uio_t uio;
	iovec_t iov;
	vnode_t *frontvp = NULL;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_symlink: ENTER dvp %p lnm %s tnm %s\n",
		    dvp, lnm, tnm));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_symlink, current_pid(),
		dvp, lnm);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(lnm));
	ASSERT(VALID_ADDR(tva));
	ASSERT(VALID_ADDR(tnm));
	ASSERT(VALID_ADDR(cr));
	ASSERT(!issplhi(getsr()));
	CACHEFS_STATS->cs_vnops++;
	rw_enter(&dcp->c_rwlock, RW_WRITER);
	ASSERT(!issplhi(getsr()));

	error = CFSOP_CHECK_COBJECT(fscp, dcp, (vattr_t *)NULL, cr, WRITELOCKED);
	ASSERT(!issplhi(getsr()));
	if (error)
		goto out;
	(void)BACK_VOP_SYMLINK(dcp, lnm, tva, tnm, cr, BACKOP_BLOCK, &error);
	ASSERT(!issplhi(getsr()));
	if (error)
		goto out;
	if ( C_CACHING( dcp ) ) {
		ASSERT(dcp->c_frontfid.fid_len &&
			(dcp->c_frontfid.fid_len <= MAXFIDSZ));
		/*
		 * Create the front file for the symlink.
		 * If this fails, just ignore the error and return immediately.
		 */
		ASSERT(cachefs_name_is_legal(lnm));
		if (dcp->c_frontdirvp || !(error = cachefs_getfrontdirvp(dcp))) {
			error = cachefs_create_frontfile(fscp->fs_cache, dcp->c_frontdirvp,
				lnm, &frontvp, NULL, &fileheader, VLNK);
		} else {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_symlink(line %d): nocache cp 0x%p on "
					"error %d from cachefs_getfrontdirvp\n",
					__LINE__, dcp, error));
			cachefs_nocache(dcp);
		}
		if (error) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_symlink(line %d): front file create "
					"error, file %s: %d\n", __LINE__, lnm, error));
			error = 0;
		} else if (!(fileheader->fh_metadata.md_state & MD_INIT)) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_symlink(line %d): front file %s exists\n",
					__LINE__, lnm, error));
			VOP_FID2(frontvp, &frontfid, fiderr);
			if (!fiderr) {
				CACHEFS_RELEASE_FILEHEADER(&frontfid, fileheader);
			} else {
				CACHEFS_RELEASE_FILEHEADER(NULL, fileheader);
			}
			VN_RELE(frontvp);
		} else {
			/*
			 * The front file was successfully created.  Now, the file header
			 * must be filled in and the symlink data written to the file.
			 */
			fileheader->fh_metadata.md_state &= ~MD_INIT;
			len = strlen(tnm);
			if (len <= sizeof(fileheader->fh_allocmap)) {
				/*
				 * The symlink data fits in the file header, so put it there.
				 * Indicate that this file has no allocation map and that it
				 * is fully populated.
				 */
				bcopy(tnm, fileheader->fh_allocmap, len);
				fileheader->fh_metadata.md_state |=
					(MD_MDVALID | MD_NOALLOCMAP | MD_POPULATED);
				fileheader->fh_metadata.md_allocents = len;
			} else {
				UIO_SETUP(&uio, &iov, tnm, len,
					(off_t)DATA_START(fscp), UIO_SYSSPACE,
					FWRITE, RLIM_INFINITY);
				WRITE_VP(frontvp, &uio, 0, sys_cred, &curuthread->ut_flid, error);
				if (error) {
					CACHEFS_STATS->cs_fronterror++;
					ASSERT(uio.uio_sigpipe == 0);
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_symlink(line %d): front file write "
							"error, file %s: %d\n", __LINE__, lnm, error));
					remerr = FRONT_VOP_REMOVE(dcp, lnm, sys_cred);
					if (remerr) {
						CACHEFS_STATS->cs_fronterror++;
						CFS_DEBUG(CFSDEBUG_ERROR,
							printf("cachefs_symlink(line %d): front file "
								"remove error, file %s: %d\n", __LINE__, lnm,
								remerr));
					}
					VOP_FID2(frontvp, &frontfid, fiderr);
					if (!fiderr) {
						CACHEFS_RELEASE_FILEHEADER(&frontfid, fileheader);
					} else {
						CACHEFS_RELEASE_FILEHEADER(NULL, fileheader);
					}
					VN_RELE(frontvp);
					ASSERT(!issplhi(getsr()));
					goto out1;
				} else if (uio.uio_resid) {
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_symlink(line %d): incomplete front "
							"file write, file %s: resid %d\n", __LINE__, lnm,
							(int)uio.uio_resid));
					remerr = FRONT_VOP_REMOVE(dcp, lnm, sys_cred);
					if (remerr) {
						CACHEFS_STATS->cs_fronterror++;
						CFS_DEBUG(CFSDEBUG_ERROR,
							printf("cachefs_symlink(line %d): front file "
								"remove error, file %s: %d\n", __LINE__, lnm,
								remerr));
					}
					error = EIO;
					VOP_FID2(frontvp, &frontfid, fiderr);
					if (!fiderr) {
						CACHEFS_RELEASE_FILEHEADER(&frontfid, fileheader);
					} else {
						CACHEFS_RELEASE_FILEHEADER(NULL, fileheader);
					}
					VN_RELE(frontvp);
					ASSERT(!issplhi(getsr()));
					goto out1;
				} else {
					/*
					 * Indicate that the file has been populated.  Also,
					 * indicate how much has been populated.
					 */
					fileheader->fh_metadata.md_state |=
						(MD_MDVALID | MD_POPULATED);
					fileheader->fh_metadata.md_allocents = 1;
					fileheader->fh_allocmap[0].am_start_off = (off_t)0;
					fileheader->fh_allocmap[0].am_size = (off_t)len;
					ASSERT(len > 0);
				}
			}
			/*
			 * Look up the back file so we can get its fid and attributes.
			 * If this fails, just remove the front file.
			 */
			(void)cachefs_lookup_back(dcp, lnm, &backvp, &fidp, 0, NULL,
				cr, C_ISFS_NONBLOCK(fscp) ? BACKOP_NONBLOCK : BACKOP_BLOCK,
				&error);
			if (error) {
				VOP_FID2(frontvp, &frontfid, fiderr);
				if (!fiderr) {
					CACHEFS_RELEASE_FILEHEADER(&frontfid, fileheader);
				} else {
					CACHEFS_RELEASE_FILEHEADER(NULL, fileheader);
				}
				VN_RELE(frontvp);
				error = FRONT_VOP_REMOVE(dcp, lnm, sys_cred);
				if (error) {
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_symlink(line %d): front file "
							"remove error, file %s: %d\n", __LINE__, lnm,
							error));
				}
				error = 0;
				ASSERT(!issplhi(getsr()));
				goto out1;
			}
			/*
			 * copy the fid data into the file header
			 */
			bcopy(fidp, &fileheader->fh_metadata.md_backfid,
				sizeof(fid_t) - MAXFIDSZ + fidp->fid_len);
			freefid(fidp);
			/*
			 * Fill in the attributes from the back file.  Place them
			 * directly into the file header.
			 */
			vap = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
			vap->va_mask = AT_ALL;
			NET_VOP_GETATTR_NOSTAT(backvp, vap, 0, cr, BACKOP_BLOCK, error);
			VATTR_TO_CATTR(dcp->c_vnode, vap, 
				       &fileheader->fh_metadata.md_attributes,
				       AT_SIZE);
			CACHEFS_ZONE_FREE(Cachefs_attr_zone, vap);
			VN_RELE(backvp);
			backvp = NULL;
			if (error) {
				VOP_FID2(frontvp, &frontfid, fiderr)
				if (!fiderr) {
					CACHEFS_RELEASE_FILEHEADER(&frontfid, fileheader);
				} else {
					CACHEFS_RELEASE_FILEHEADER(NULL, fileheader);
				}
				VN_RELE(frontvp);
				error = FRONT_VOP_REMOVE(dcp, lnm, sys_cred);
				if (error) {
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_symlink(line %d): front file "
							"remove error, file %s: %d\n", __LINE__, lnm,
							error));
				}
				error = 0;
				ASSERT(!issplhi(getsr()));
				goto out1;
			}
			ASSERT(fileheader->fh_metadata.md_attributes.ca_type == VLNK);
			/*
			 * Write the file header.
			 */
			error = cachefs_write_file_header((cnode_t *)NULL, frontvp,
				fileheader, FILEHEADER_BLOCK_SIZE(fscp->fs_cache),
				DIO_ALIGNMENT(fscp->fs_cache));
			if (error) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_symlink(line %d): file header write "
						"error, file %s: %d\n", __LINE__, lnm, error));
				remerr = FRONT_VOP_REMOVE(dcp, lnm, sys_cred);
				if (remerr) {
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_symlink(line %d): front file "
							"remove error, file %s: %d\n", __LINE__,
							lnm, remerr));
				}
			}
			VOP_FID2(frontvp, &frontfid, fiderr);
			if (!fiderr) {
				CACHEFS_RELEASE_FILEHEADER(&frontfid, fileheader);
			} else {
				CACHEFS_RELEASE_FILEHEADER(NULL, fileheader);
			}
			VN_RELE(frontvp);
		}
	}
	ASSERT(!issplhi(getsr()));
out1:
	(void)cachefs_update_directory(dcp, 0, cr);
	ASSERT(!issplhi(getsr()));

out:
	rw_exit(&dcp->c_rwlock);
	ASSERT(backvp == NULL);
	ASSERT(!issplhi(getsr()));
	ASSERT(!fileheader);

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_symlink: EXIT error = %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_symlink: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_symlink: EXIT lnm %s error %d\n", lnm, error));
	return (error);
}

int
cachefs_readdir(bhv_desc_t *bdp, register struct uio *uiop, cred_t *cr,
	int *eofp)
{
	int dirbuf_len = 0;
	off_t nextoff = 0;
	char *dirstart = NULL;
	off_t diroff = (off_t)0;
	int uiolen;
	off_t user_offset;
	int invalidate_once = 0;
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *dcp = BHVTOC(bdp);
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	u_char abi;
	uio_t inuio;
	iovec_t iniov;
	char *dirbuf = NULL;
	char *bp = NULL;
	dirent_t *dep;
	u_short reclen;
	int count;
	int dircount;
#ifdef DEBUG
	dirent_t *lastdep = NULL;
#endif

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_READDIR,
		printf("cachefs_readdir: ENTER vp %p offset 0x%lld resid %d\n", vp,
			uiop->uio_offset, uiop->uio_resid));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_readdir, current_pid(),
		vp, uiop);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(eofp));
	CACHEFS_STATS->cs_vnops++;
	CACHEFS_STATS->cs_readdirs++;
	if (uiop->uio_iov->iov_len == 0) {
		goto out;
	}
	/*
	 * Determine the ABI.  For UIO_USERSPACE, get the ABI from
	 * the proc structure.  For UIO_SYSSPACE, the ABI is always
	 * ABI_IRIX5_64.
	 */
	abi = GETDENTS_ABI(get_current_abi(), uiop);
again:
	ASSERT(RW_READ_HELD(&dcp->c_rwlock) || RW_WRITE_HELD(&dcp->c_rwlock));
	*eofp = 0;
	error = CFSOP_CHECK_COBJECT(fscp, dcp, (vattr_t *)NULL, cr, READLOCKED);
	if (error) {
		goto out;
	}
	/*
	 * If the directory is not valid, it must be made valid before doing
	 * the readdir.  This is to avoid the case where the directory becomes
	 * valid while a process is doing a sequence of readdirs.  Reads of the
	 * directory will always go to the front FS.
	 */
	if ( !C_CACHING( dcp ) ) {
		(void)BACK_VOP_READDIR(dcp, uiop, cr, eofp, BACKOP_BLOCK, &error);
	} else {
		ASSERT(dcp->c_frontfid.fid_len &&
			(dcp->c_frontfid.fid_len <= MAXFIDSZ));
		ASSERT(VALID_ADDR(dcp->c_fileheader));
		if ( !(dcp->c_fileheader->fh_metadata.md_state & MD_POPULATED) ) {
			CACHEFS_STATS->cs_readdirmiss++;
			rw_exit(&dcp->c_rwlock);
			rw_enter(&dcp->c_rwlock, RW_WRITER);
			error = cachefs_update_directory( dcp, IO_SYNC, cr );
			rw_downgrade(&dcp->c_rwlock);
			/*
			 * We must go back and verify that we are still caching the
			 * directory.  An error may have caused us to nocache the
			 * directory while populating it.  Make sure we recognize
			 * EINTR, though.  We get into an infinite loop otherwise.
			 */
			switch (error) {
				case 0:
					break;
				case EINTR:
					goto out;
				default:
					/*
					 * If we are still caching the directory, an error
					 * must have been returned by the backFS.  Return
					 * that error to the user.
					 */
					if (C_CACHING( dcp )) {
						goto out;
					}
					goto again;
			}
		} else {
			CACHEFS_STATS->cs_readdirhit++;
		}
		if (!error) {
			ASSERT(dcp->c_fileheader->fh_metadata.md_state & MD_POPULATED);
			ASSERT(C_CACHING(dcp));
			ASSERT(dcp->c_fileheader->fh_allocmap[0].am_size > (off_t)0);
			/*
			 * One problem which must be handled is that the
			 * directory may have been updated since the last
			 * time the user called readdir.  To handle this,
			 * d_off contains an index as opposed to an offset.
			 * We must search for the proper entry.  The directory
			 * data is guaranteed to be self-consistent as long as
			 * we hold c_rwlock.
			 *
			 * Allocate the buffer to hold the directory entries.
			 * Make the buffer at least as big as the user's and
			 * rounded up to the nearest CFS_BMAPSIZE multiple.
			 * We don't just read in the entire file as it may be
			 * too large.
			 * The initial file offset is DATA_START().  This is
			 * Where the directory entries begin.
			 */
			dirbuf_len = ROUNDUP(uiop->uio_resid, CFS_BMAPSIZE(dcp));
			dirbuf = CACHEFS_KMEM_ALLOC(dirbuf_len, KM_SLEEP);
			diroff = (off_t)DATA_START(fscp);
			/*
			 * Set up the uio structure for reading the directory
			 * and read in the first chunk.
			 */
			UIO_SETUP(&inuio, &iniov, dirbuf, dirbuf_len, diroff,
				UIO_SYSSPACE, 0, RLIM_INFINITY);
			error = FRONT_READ_VP(dcp, &inuio, 0, sys_cred);
			dircount = MIN(dirbuf_len - inuio.uio_resid,
				dcp->c_fileheader->fh_allocmap[0].am_size);
			if (error || (dircount < (int)DIRENTSIZE(1))) {
				CFS_DEBUG(CFSDEBUG_NOCACHE,
					printf("cachefs_readdir(line %d): nocache cp 0x%p on "
						"error %d from FRONT_READ_VP\n",
						__LINE__, dcp, error));
				cachefs_nocache(dcp);
				goto again;
			}
			ASSERT(cachefs_kmem_validate(dirbuf, dirbuf_len));
			/*
			 * Determine how much was actually read.  uio_resid tells
			 * how much of the buffer has not been filled.  Subtract
			 * this from the total size of the buffer to find out how
			 * many bytes were read.
			 * Set dircount to be no larger than what we think has been
			 * allocated.  Under some circumstances, the front file
			 * system may have rounded to a block boundary.
			 */
			ASSERT(dcp->c_fileheader->fh_allocmap[0].am_size > (off_t)0);
			CFS_DEBUG(CFSDEBUG_READDIR,
				printf("cachefs_readdir: dircount %d dirbuf 0x%p\n", dircount,
					dirbuf));
			ASSERT(valid_dirents((dirent_t *)dirbuf, dircount));
			dep = (dirent_t *)dirbuf;
			/*
			 * Locate the starting entry based on the user offset.
			 * The user offset is an index of the entry to read, not
			 * a file offset.
			 */
			for (user_offset = uiop->uio_offset; user_offset > 0;
				user_offset--) {
					/*
					 * If the amount remaining in the directory buffer
					 * is smaller than the minimum directory size or
					 * the current directory entry is incomplete,
					 * read some more of the directory.  diroff
					 * corresponds to the file offset for the
					 * directory entry pointed to by dep (i.e., the
					 * current directory entry).  We begin reading
					 * at diroff and reset dep to dirbuf.
					 */
					if ((dircount < (int)(DIRENTSIZE(1))) ||
						(dircount < (int)dep->d_reclen)) {
							UIO_SETUP(&inuio, &iniov, dirbuf,
								dirbuf_len, diroff, UIO_SYSSPACE,
								0, RLIM_INFINITY);
							error = FRONT_READ_VP(dcp, &inuio, 0, sys_cred);
							ASSERT(cachefs_kmem_validate(dirbuf,
								dirbuf_len));
							dircount = MIN(dirbuf_len - inuio.uio_resid,
								dcp->c_fileheader->fh_allocmap[0].am_size -
								diroff + (off_t)DATA_START(fscp));
							ASSERT(valid_dirents((dirent_t *)dirbuf, dircount));
							if (error) {
								CACHEFS_STATS->cs_fronterror++;
								CFS_DEBUG(CFSDEBUG_ERROR,
									printf("cachefs_readdir: directory "
										"read error: %d\n", error));
								goto out;
							} else if (!dircount) {
								*eofp = 1;
								CFS_DEBUG(CFSDEBUG_READDIR,
									printf("cachefs_readdir: EOF detected at "
										"line %d\n", __LINE__));
								error = 0;
								goto out;
							}
							dep = (dirent_t *)dirbuf;
					}
					ASSERT(dircount >= (int)DIRENTSIZE(1));
					ASSERT(dircount >= (int)dep->d_reclen);
					/*
					 * Move to the next directory entry.  Adjust
					 * diroff and dircount by the record length of
					 * the current entry first.
					 */
					diroff += (off_t)dep->d_reclen;
					dircount -= (int)dep->d_reclen;
					dep = (dirent_t *)((u_long)dep + (u_long)dep->d_reclen);
			}
			/*
			 * At this point, diroff contains the file offset of the
			 * current directory entry.  dep points to the current
			 * directory entry in dirbuf.  dircount tells how many
			 * bytes remain in dirbuf.
			 * If the user wants more data than is in the directory buffer,
			 * we reset the buffer by rereading the data.  We read in only
			 * as much as the user has requested.
			 */
			CFS_DEBUG(CFSDEBUG_READDIR,
				printf("cachefs_readdir(line %d): dircount %d\n", __LINE__,
					dircount));
			ASSERT(diroff <= dcp->c_fileheader->fh_allocmap[0].am_size +
				(off_t)DATA_START(fscp));
			if (diroff == (dcp->c_fileheader->fh_allocmap[0].am_size +
				(off_t)DATA_START(fscp))) {
					*eofp = 1;
					CFS_DEBUG(CFSDEBUG_READDIR,
						printf("cachefs_readdir: EOF detected at line %d\n",
							__LINE__));
					error = 0;
					goto out;
			}
			if (dircount < uiop->uio_iov->iov_len) {
				UIO_SETUP(&inuio, &iniov, dirbuf, dirbuf_len,
					diroff, UIO_SYSSPACE, 0, RLIM_INFINITY);
				error = FRONT_READ_VP(dcp, &inuio, 0, sys_cred);
				ASSERT(cachefs_kmem_validate(dirbuf, dirbuf_len));
				dircount = MIN(uiop->uio_iov->iov_len - inuio.uio_resid,
					dcp->c_fileheader->fh_allocmap[0].am_size - diroff +
					(off_t)DATA_START(fscp));
				ASSERT(valid_dirents((dirent_t *)dirbuf, dircount));
				if (error) {
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_readdir: directory "
							"read error: %d\n", error));
					goto out;
				} else if (!dircount) {
					*eofp = 1;
					CFS_DEBUG(CFSDEBUG_READDIR,
						printf("cachefs_readdir: EOF detected at line %d\n",
							__LINE__));
					error = 0;
					goto out;
				}
				dep = (dirent_t *)dirbuf;
			}
			bp = dirstart = (char *)dep;
			/*
			 * dircount is the count of the number of bytes remaining in
			 * the directory buffer.  count will be the upper bound for
			 * the number of bytes to be transferred to the user.  This
			 * upper bound must not be greater than either dircount or the
			 * number of bytes the application is willing to accept.
			 * Thus, it is the smaller of dircount and uiop->uio_iov->iov_len.
			 */
			count = MIN(dircount, uiop->uio_iov->iov_len);
			ASSERT(dircount >= (int)DIRENTSIZE(1));
			ASSERT(dircount >= (int)dep->d_reclen);
			CFS_DEBUG(CFSDEBUG_READDIR,
				printf("cachefs_readdir(line %d): count %d\n", __LINE__,
					count));
			/*
			 * Format the data according to the ABI.
			 */
			switch (abi) {
				case ABI_IRIX5_64:
				case ABI_IRIX5_N32:
					CFS_DEBUG(CFSDEBUG_READDIR,
						printf("cachefs_readdir(line %d): 64-bit abi\n",
							__LINE__));
					/*
					 * The data is already in the correct format, simply
					 * locate the end of the data.  The end will be either
					 * the end of the buffer or the start of an incomplete
					 * record at the end of the buffer.
					 */
					while (count && (count >= (int)DIRENTSIZE(1)) &&
						(count >= dep->d_reclen)) {
							/*
							 * We need to save the offset of the next entry
							 * so that it can be returned to the caller.
							 */
							nextoff = dep->d_off;
							/*
							 * Protect against invalid entries.  If an
							 * invalid one is detected, invalidate the
							 * object and retry the operation.
							 */
							if ((dep->d_reclen < DIRENTSIZE(1)) ||
								((dep->d_reclen - DIRENTBASESIZE) >
								MAXPATHLEN)) {
									CFS_DEBUG(CFSDEBUG_ERROR,
										printf("cachefs_readdir: bad directory "
											"file entry: dep 0x%p count %d "
											"dirbuf 0x%p iov len %d "
											"uio_resid %d uio_offset %lld "
											"dep 0x%p lastdep 0x%p bp "
											"0x%p\n", dep, count, dirbuf,
											(int)uiop->uio_iov->iov_len,
											(int)inuio.uio_resid,
											uiop->uio_offset, dep,
											lastdep, bp));
									if (!invalidate_once) {
										(void)cachefs_inval_object(dcp,
											READLOCKED);
										invalidate_once = 1;
									} else {
										CFS_DEBUG(CFSDEBUG_NOCACHE,
											printf("cachefs_readdir(line %d): "
												"nocache cp 0x%p on invalid "
												"dirent after invalidation\n",
												__LINE__, dcp, error));
										cachefs_nocache(dcp);
									}
									goto again;
							}
							count -= (int)dep->d_reclen;
							ASSERT(((caddr_t)bp - dirstart) <=
								uiop->uio_iov->iov_len);
#ifdef DEBUG
							lastdep = dep;
#endif
							dep = (dirent_t *)((u_long)dep +
								(u_long)dep->d_reclen);
					}
					bp = (char *)dep;
					break;
				case ABI_IRIX5:
					CFS_DEBUG(CFSDEBUG_READDIR,
						printf("cachefs_readdir(line %d): 32-bit abi\n",
							__LINE__));
					/*
					 * Data is stored in the front file in its 64-bit
					 * format.  It must be translated before being
					 * sent to the user.
					 * Format data so long as the user buffer is large
					 * enough.
					 */
					while ((count >= (int)(DIRENTSIZE(1))) &&
						(count >= (int)dep->d_reclen)) {
							/*
							 * We will be formatting in place, so we need
							 * to save the record length.  We also need to
							 * save the offset of the next entry so that it
							 * can be returned to the caller.
							 */
							nextoff = dep->d_off;
							reclen = dep->d_reclen;
							error = cachefs_irix5_fmtdirent((void **)&bp,
								dep, count);
							if (error) {
								CFS_DEBUG(CFSDEBUG_ERROR,
									printf("cachefs_readdir: bad directory "
										"file entry: dep 0x%p count %d "
										"dirbuf 0x%p iov len %d uio_resid "
										"%d uio_offset %lld dep 0x%p "
										"lastdep 0x%p bp 0x%p\n", dep,
										count, dirbuf,
										(int)uiop->uio_iov->iov_len,
										(int)inuio.uio_resid,
										uiop->uio_offset, dep,
										lastdep, bp));
								if (!invalidate_once) {
									(void)cachefs_inval_object(dcp, READLOCKED);
									invalidate_once = 1;
								} else {
									CFS_DEBUG(CFSDEBUG_NOCACHE,
										printf("cachefs_readdir(line %d): "
											"nocache cp 0x%p on invalid "
											"dirent after invalidation\n",
											__LINE__, dcp, error));
									cachefs_nocache(dcp);
								}
								goto again;
							}
							count -= reclen;
							ASSERT(((caddr_t)bp - dirstart) <=
								uiop->uio_iov->iov_len);
#ifdef DEBUG
							lastdep = dep;
#endif
							dep = (dirent_t *)((u_long)dep + (u_long)reclen);
					}
					break;
				default:
					cmn_err(CE_WARN, "CacheFS readdir: unknown ABI 0x%x\n",
						abi);
					error = EINVAL;
			}
			ASSERT(cachefs_kmem_validate(dirbuf, dirbuf_len));
			/*
			 * If no error occurred, transfer the data to the user.
			 */
			if (!error) {
				CFS_DEBUG(CFSDEBUG_READDIR,
					printf("cachefs_readdir(line %d): dirstart 0x%p, "
						"count %d\n", __LINE__, dirstart, count));
				/*
				 * At this point, bp points to the first byte in
				 * dirbuf past the end of the data to be sent to
				 * the user.  dirstart points to the start of this
				 * data.
				 */
				uiolen = (int)(bp - dirstart);
				CFS_DEBUG(CFSDEBUG_ERROR,
					if ((count < 0) ||
						(uiolen > (uiop->uio_iov->iov_len - count)) ||
						(uiolen > uiop->uio_iov->iov_len))
							printf("cachefs_readdir: count %d, "
								"iov len %d, uiolen %d, inuio len %d\n",
								count, (int)uiop->uio_iov->iov_len, uiolen,
								inuio.uio_resid));
				ASSERT(count >= 0);
				ASSERT(uiolen <= (uiop->uio_iov->iov_len - count));
				ASSERT(uiolen <= uiop->uio_iov->iov_len);
				/*
				 * Now, the count value tells us how much of the
				 * original buffer is left.  The dirent pointer
				 * dep points to either an incomplete record at the
				 * end of the buffer or to the first byte just
				 * beyond the end of the buffer.
				 */
				error = uiomove(dirstart, uiolen, UIO_READ, uiop);
				if (!error) {
					/*
					 * At this point, the user's offset is incorrect
					 * with respect to the data in the front file.
					 * The offset of the next directory entry was
					 * retrieved in the loop above.
					 */
					uiop->uio_offset = nextoff;
				}
			}
		}
	}
out:
	if (dirbuf) {
		CACHEFS_KMEM_FREE(dirbuf, dirbuf_len);
	}

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_READDIR,
		printf("cachefs_readdir: EXIT error = %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_readdir: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_readdir: EXIT vp %p error %d\n", vp, error));
	return (error);
}


int
cachefs_fid(bhv_desc_t *bdp, struct fid **fidpp)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);
	int ospl;
	int error;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_fid, current_pid(),
		vp, fidpp);
	ASSERT(cp);
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(fidpp));
	CACHEFS_STATS->cs_vnops++;
	/*
	 * The file ID to be returned will be the file ID of the front file.
	 * If there is no front file, no file ID can be constructed.
	 */
	if (C_CACHING(cp)) {
		ASSERT(cp->c_frontfid.fid_len && (cp->c_frontfid.fid_len <= MAXFIDSZ));
		ASSERT(VALID_ADDR(cp->c_fileheader));
		error = FRONT_VOP_FID(cp, fidpp);
		if (!error && (*fidpp != NULL)) {
			ospl = mutex_spinlock(&cp->c_statelock);
			/*
			 * Since we are returning the fid of the front file to the caller,
			 * we need to make sure that the front file does not get removed.
			 * Do this by setting MD_KEEP in the file header.
			 */
			cp->c_fileheader->fh_metadata.md_state |= MD_KEEP;
			cp->c_flags |= CN_UPDATED;
			CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_fid, cp->c_flags,
				cp->c_fileheader->fh_metadata.md_allocents);
			ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
				FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
			mutex_spinunlock(&cp->c_statelock, ospl);
			if (!cp->c_frontvp) {
				error = cachefs_getfrontvp(cp);
				CFS_DEBUG(CFSDEBUG_ERROR, if (error)
					printf("cachefs_fid(line %d): error %d getting "
						"front vnode\n", __LINE__, error));
				if (error) {
					(void)cachefs_nocache(cp);
					error = 0;
				}
			}
		}
	} else {
		error = EINVAL;
	}
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_fid: EXIT vp %p error %d\n", vp, error));
	return(error);
}


int
cachefs_fid2(bhv_desc_t *bdp, struct fid *fidp)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);
	int ospl;
	int error;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_fid2, current_pid(),
		vp, fidp);
	ASSERT(cp);
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(fidp));
	CACHEFS_STATS->cs_vnops++;
	/*
	 * The file ID to be returned will be the file ID of the front file.
	 * If there is no front file, no file ID can be constructed.
	 */
	if (C_CACHING(cp)) {
		ASSERT(cp->c_frontfid.fid_len && (cp->c_frontfid.fid_len <= MAXFIDSZ));
		ASSERT(VALID_ADDR(cp->c_fileheader));
		error = FRONT_VOP_FID2(cp, fidp);
		if (!error) {
			ospl = mutex_spinlock(&cp->c_statelock);
			/*
			 * Since we are returning the fid of the front file to the caller,
			 * we need to make sure that the front file does not get removed.
			 * Do this by setting MD_KEEP in the file header.
			 */
			cp->c_fileheader->fh_metadata.md_state |= MD_KEEP;
			cp->c_flags |= CN_UPDATED;
			CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_fid2, cp->c_flags,
				cp->c_fileheader->fh_metadata.md_allocents);
			ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
				FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
			mutex_spinunlock(&cp->c_statelock, ospl);
			if (!cp->c_frontvp) {
				error = cachefs_getfrontvp(cp);
				CFS_DEBUG(CFSDEBUG_ERROR, if (error)
					printf("cachefs_fid2(line %d): error %d getting "
						"front vnode\n", __LINE__, error));
				if (error) {
					(void)cachefs_nocache(cp);
					error = 0;
				}
			}
		}
	} else {
		error = EINVAL;
	}
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_fid2: EXIT vp %p error %d\n", vp, error));
	return(error);
}


void
cachefs_rwlock(bhv_desc_t *bdp, vrwlock_t write_lock)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_rwlock, current_pid(),
		vp, write_lock);
	ASSERT(VALID_ADDR(vp));
	CACHEFS_STATS->cs_vnops++;
	if (write_lock == VRWLOCK_READ)
		rw_enter(&cp->c_rwlock, RW_READER);
	else
		rw_enter(&cp->c_rwlock, RW_WRITER);
}

/* ARGSUSED */
void
cachefs_rwunlock(bhv_desc_t *bdp, vrwlock_t write_lock)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_rwunlock, current_pid(),
		vp, write_lock);
	ASSERT(VALID_ADDR(vp));
	CACHEFS_STATS->cs_vnops++;
	rw_exit(&cp->c_rwlock);
}

/* ARGSUSED */
int
cachefs_seek(bhv_desc_t *bdp, off_t ooff, off_t *noffp)
{
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_seek, current_pid(),
		BHV_TO_VNODE(bdp), ooff);
	ASSERT(VALID_ADDR(BHV_TO_VNODE(bdp)));
	CACHEFS_STATS->cs_vnops++;
	return (0);
}


/*ARGSUSED*/
int
cachefs_map(bhv_desc_t *bdp,
	off_t off,
	size_t len,
	mprot_t prot,
	u_int flags,
	cred_t *cr,
	vnode_t **nvp)
{
	/* REFERENCED */
	vnode_t	*vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);
	fscache_t *fscp = C_TO_FSCACHE(cp);

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_map: ENTER vp %p off %lld len %d flags %d\n",
			vp, off, len, flags));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_map, current_pid(),
		vp, off);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_vnops++;
	if ((prot & PROT_WRITE) && !(flags & MAP_PRIVATE) &&
		(C_ISFS_WRITE_AROUND(fscp)) ) {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_map(line %d): nocache cp 0x%p for mmap\n",
					__LINE__, cp));
			cachefs_nocache(cp);
	}
	return (0);
}

/* ARGSUSED */
int
cachefs_frlock(bhv_desc_t *bdp, int cmd, struct flock *bfp, int flag,
	off_t offset, vrwlock_t vrwlock, cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct cnode *cp = BHVTOC(bdp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int err;
	flock_t fl;
	int ospl, dolock;
	int error;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_frlock, current_pid(),
		vp, cmd);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(bfp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(!C_CACHING(cp) || VALID_ADDR(cp->c_attr));
	CACHEFS_STATS->cs_vnops++;
	
	if (cmd == F_CLNLK || cmd == F_CHKLK || cmd == F_CHKLKW) {
		if (cmd == F_CLNLK) {
		        /*
			 * Clean remote first, then clean local by falling
			 * through.
			 */
			if (!C_ISFS_PRIVATE(fscp) && 
			    (cp->c_flags & CN_FRLOCK)) {
				struct cred *cred;

				CFSOP_EXPIRE_COBJECT(C_TO_FSCACHE(cp), cp, 
						     sys_cred);
				cred = crdup(cr);
				fl.l_pid = bfp->l_pid;
				fl.l_sysid = bfp->l_sysid;
				fl.l_type = F_UNLCK;
				fl.l_whence = 0;
				fl.l_start = 0;
				fl.l_len = 0;      /* unlock entire file */
				(void)BACK_VOP_FRLOCK(cp, F_SETLK, &fl, 0, 
						      (off_t)0, cred,
						      BACKOP_BLOCK, vrwlock,
						      &error);
				crfree(cred);
			}
			/* fall through to fs_frlock */
		}

		dolock = (vrwlock == VRWLOCK_NONE);
		if (dolock) {
			VOP_RWLOCK(vp, VRWLOCK_WRITE);
			vrwlock = VRWLOCK_WRITE;
		}

		error = fs_frlock(bdp, cmd, bfp, flag, offset, vrwlock, cr);
		if (dolock)
			VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
		return(error);
	}

	if ((cmd != F_GETLK) && (cmd != F_SETLK) && (cmd != F_SETLKW) &&
		cmd != F_SETBSDLK && cmd != F_SETBSDLKW) {
			CFS_DEBUG(CFSDEBUG_ERROR2,
				printf("cachefs_frlock: EXIT vp %p cmd %d EINVAL\n", vp, cmd));
			return (EINVAL);
	}

	ASSERT(vrwlock == VRWLOCK_NONE);

	if (C_ISFS_PRIVATE(C_TO_FSCACHE(cp))) {
			VOP_RWLOCK(vp, VRWLOCK_WRITE);
			error = fs_frlock(bdp, cmd, bfp, flag, offset, 
					  VRWLOCK_WRITE, cr);
			VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
	} else {
		/*
		 * It is very important to not hold c_rwlock here.  If it is
		 * held across the call to BACK_VOP_FRLOCK below, a deadlock
		 * can result.
		 * Invalidate and nocache the file since we are doing file locking.
		 */
		error = cachefs_inval_object(cp, UNLOCKED);
		ospl = mutex_spinlock( &cp->c_statelock );
		cp->c_flags |= (CN_FRLOCK | CN_NOCACHE);
		mutex_spinunlock( &cp->c_statelock, ospl );
		if (bfp->l_whence == 2) {
			bfp->l_start += cp->c_size;
			bfp->l_whence = 0;
		}
		fl = *bfp;
		(void)BACK_VOP_FRLOCK(cp, cmd, bfp, flag, offset, cr, 
				      BACKOP_BLOCK, vrwlock, &error);
		if (!error && (cmd != F_GETLK)) {
			if (fl.l_type != F_UNLCK) {
				/*
				 * Expire for every lock request but not for unlocks.
				 * This will force a validation of the attributes.
				 * If the file is memory mapped, we have to check the object
				 * here to guarantee that the next access will get the
				 * correct data.  This is because the next access may be
				 * through the mapping.
				 */
				CFSOP_EXPIRE_COBJECT(C_TO_FSCACHE(cp), cp, sys_cred);
				if (VN_MAPPED(vp)) {
					error = CFSOP_CHECK_COBJECT(C_TO_FSCACHE(cp), cp,
						(vattr_t *)NULL, sys_cred, UNLOCKED);
					if (error) {
						/*
						 * error in checking the object, so release the lock
						 * and return the error
						 */
						fl.l_type = F_UNLCK;
						(void)BACK_VOP_FRLOCK(cp, cmd, &fl, 
								      flag, offset, cr,
								      BACKOP_BLOCK, 
								      vrwlock, &err);
					}
				}
			}
			if (!error) {
				/*
				 * Always record the file lock in the cachefs vnode so that
				 * the upper layer can tell that file locks are or are not
				 * held.  We need to keep the setting of VFRLOCKS in the
				 * cachefs vnode consistent with that for the back file.
				 * We have to do this here because reclock will never
				 * be called for the cachefs vnode and there will never
				 * be a list of file lock for the cachefs vnode.  All of
				 * this information is maintained by the back file system.
				 */
				VOP_RWLOCK(vp, VRWLOCK_WRITE);
				if (cp->c_backvp->v_flag & VFRLOCKS) {
					if (!(vp->v_flag & VFRLOCKS)) {
						VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 1);
					}
				} else if (vp->v_flag & VFRLOCKS) {
					VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 0);
				}
				VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
			}
		}
	}


	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_frlock: EXIT ESTALE\n"));
	CFS_DEBUG(CFSDEBUG_ERROR2, if (error && (error != ENOENT))
		printf("cachefs_frlock: EXIT vp %p error %d\n", vp, error));
	return (error);
}


/*ARGSUSED*/
int
cachefs_realvp(bhv_desc_t *bdp, struct vnode **vpp)
{
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_realvp, current_pid(),
		BHV_TO_VNODE(bdp), vpp);
	ASSERT(VALID_ADDR(BHV_TO_VNODE(bdp)));
	ASSERT(VALID_ADDR(vpp));
	CACHEFS_STATS->cs_vnops++;
	return (EINVAL);
}

/* ARGSUSED */
int
cachefs_bmap(bhv_desc_t *bdp, off_t offset, ssize_t size, int flags,
	struct cred *crp, struct bmapval *bvp, int *nmaps)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);
	u_long bsize = CFS_BMAPSIZE(cp);
	int n;

	CFS_DEBUG(CFSDEBUG_BMAP,
		printf("cachefs_bmap: ENTER vp 0x%p, offset %lld, size %d, bmap "
			"0x%p, nmaps %d\n", vp, offset, (int)size, bvp, *nmaps));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_bmap, current_pid(),
		vp, offset);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(crp));
	ASSERT(VALID_ADDR(bvp));
	ASSERT(VALID_ADDR(nmaps));
	CACHEFS_STATS->cs_vnops++;
	bvp->pboff = offset & CFS_BOFFSET(cp);
	ASSERT(bvp->pboff < bsize);
	/*
	 * cp->c_size - offset is what is left in the file
	 * bsize - bvp->pboff is what will fit in a buffer
	 * For reads, the smallest of these is pbsize.
	 * For writes, ignore the current file size.
	 * We intentionally ignore the size requested by the caller.  This
	 * way we always create mappings for bsize or whatever is
	 * left in the file if this is a read mapping.
	 */
	if ( flags & B_READ ) {
		bvp->pbsize = (int)MIN( (off_t)(bsize - bvp->pboff),
			MAX( cp->c_size - offset, (off_t)0 ) );
		ASSERT( bvp->pbsize >= 0 );
		/*
		 * For reads, bsize is the sum of pboff and pbsize rounded up to the
		 * nearest basic block.
		 */
		bvp->bsize = (bvp->pboff + bvp->pbsize + BBMASK) & ~BBMASK;
	} else { /* B_WRITE */
		bvp->pbsize = bsize - bvp->pboff;
		bvp->bsize = bsize;
	}
	CFS_DEBUG(CFSDEBUG_BMAP,
		printf("pbsize for map 0 (0x%x) set to %d\n", &bvp[0], bvp[0].pbsize));
	bvp->pbdev = vp->v_rdev;
	/*
	 * length is always a function of bsize
	 */
	bvp->length = BTOBB( bvp->bsize );
	bvp->offset = BTOBBT( offset & CFS_BMASK(cp) );
	/*
	 * Set BMAP_DELAY to cause the buffer cache to always make sure
	 * that there will be some buffers lying about for cachefs_strategy
	 * to use (acutally, the buffers will be needed by the front and
	 * back file systems).
	 */
	bvp->eof = BMAP_DELAY;
	/*
	 * The block number must be -1 for BMAP_DELAY to work properly.
	 */
	bvp->bn = -1;
	/*
	 * Cachefs doesn't do NUMA yet.
	 */
	bvp->pmp = NULL;
	/*
	 * If offset + bvp->pbsize is greater than or equal to the current
	 * file size, then the first mapping contains the EOF.  Set the mapping
	 * count to 1 and indicate that this mapping contains the EOF.
	 */
	if (offset + (off_t)bvp->pbsize >= cp->c_size) {
		bvp->eof |= BMAP_EOF;
		*nmaps = 1;
	} else {
		ASSERT( bvp->pbsize > 0 );
		/*
		 * Construct as many readahead mappings as possible.
		 * Stop when as many mappings as requested have been constructed
		 * (*nmaps) or a mapping is constructed which contains the EOF.
		 * We assume here that all mappings past the first for writes
		 * are readahead.
		 */
		for ( n = 1; n < *nmaps; n++ ) {
			bvp[n].offset = bvp[n-1].offset + bvp[n-1].length;
			bvp[n].pboff = 0;
			bvp[n].pbdev = vp->v_rdev;
			/*
			 * Set BMAP_DELAY to cause the buffer cache to always make sure
			 * that there will be some buffers lying about for cachefs_strategy
			 * to use (acutally, the buffers will be needed by the front and
			 * back file systems).
			 */
			bvp[n].eof = BMAP_DELAY;
			bvp[n].bn = -1;
			/*
			 * Cachefs doesn't do NUMA yet.
			 */
			bvp[n].pmp = NULL;
			/*
			 * pbsize is the minimum of bsize and whatever is left
			 * in the file relative to the starting file offset for the
			 * block
			 */
			if ( (cp->c_size -
				(off_t)BBTOOFF( bvp[n].offset )) <= (off_t)bsize ) {
					bvp[n].pbsize = (int)(cp->c_size -
						(off_t)BBTOOFF( bvp[n].offset ));
					bvp[n].eof |= BMAP_EOF;
					*nmaps = n + 1;
					bvp[n].bsize = (bvp[n].pbsize + BBMASK) & ~BBMASK;
					bvp[n].length = BTOBBT( bvp[n].bsize );
					CFS_DEBUG(CFSDEBUG_BMAP,
						printf("pbsize for map %d (0x%x) set to %d\n", n,
							&bvp[n], bvp[n].pbsize));
					ASSERT( bvp[n].pbsize > 0 );
					break;
			} else {
				bvp[n].pbsize = bsize;
				CFS_DEBUG(CFSDEBUG_BMAP,
					printf("pbsize for map %d (0x%x) set to %d\n", n, &bvp[n],
						bvp[n].pbsize));
				bvp[n].bsize = (bvp[n].pbsize + BBMASK) & ~BBMASK;
				bvp[n].length = BTOBBT( bvp[n].bsize );
			}
		}
	}
	CFS_DEBUG(CFSDEBUG_BMAP, printf("cachefs_bmap: EXIT nmaps %d\n", *nmaps));
	return( 0 );
}

void
cachefs_strategy(bhv_desc_t *bdp, struct buf *bp)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC( bdp );
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;
#ifdef DEBUG
	off_t off = BBTOOFF( bp->b_offset );
#endif /* DEBUG */
	struct cachefs_req *rp;
	int ospl;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_strategy: ENTER vp 0x%p, bp 0x%p %s\n", vp,
			bp, (bp->b_flags & B_READ) ? "READ" : "WRITE"));
	ASSERT(cp->c_cred != NULL);
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_strategy, current_pid(),
		vp, bp);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(bp));
	ASSERT(!issplhi(getsr()));
	CACHEFS_STATS->cs_vnops++;
	if (bp->b_flags & B_ASYNC) {
		if ( bp->b_flags != B_ASYNC ) {
			ASSERT(cp->c_size > off);
			    /*LINTED alignment okay*/
			rp = (struct cachefs_req *)CACHEFS_ZONE_ZALLOC(Cachefs_req_zone,
			    KM_SLEEP);
			rp->cfs_req_u.cu_io.cp_cp = cp;
			rp->cfs_req_u.cu_io.cp_buf = bp;
			rp->cfs_req_u.cu_io.cp_flags = bp->b_flags & ~B_ASYNC;
			rp->cfs_cr = cp->c_cred;
			crhold(rp->cfs_cr);
			ospl = mutex_spinlock(&cp->c_iolock);
			if ( bp->b_flags & B_READ ) {
				rp->cfs_cmd = CFS_ASYNCREAD;
				cp->c_ioflags |= CIO_ASYNCREAD;
			} else { /* B_WRITE */
				rp->cfs_cmd = CFS_ASYNCWRITE;
				cp->c_ioflags |= CIO_ASYNCWRITE;
			}
			cp->c_nio++;
			mutex_spinunlock(&cp->c_iolock, ospl);
			error = cachefs_addqueue(rp, &fscp->fs_cache->c_workq);
			/*
			 * if we get an error, we fall back to synchronous I/O
			 * we assume that an error means that the request
			 * could not be accepted and was not enqueued
			 * the exception to this is bdflush
			 */
			if ( error ) {
				crfree(rp->cfs_cr);
				CACHEFS_ZONE_FREE(Cachefs_req_zone, rp);
				if ( !(bp->b_flags & B_BDFLUSH) ) {
					bp->b_flags &= ~B_ASYNC;
					if ( bp->b_flags & B_READ ) {
						error = cachefsio_read(cp,
								bp, cp->c_cred);
					} else { /* B_WRITE */
						error = cachefsio_write(cp,
								bp, cp->c_cred);
					}
#ifdef DEBUG
				} else {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_strategy: async bdflush buf 0x%p "
							"ignored due to error %d\n", bp, error));
#endif /* DEBUG */
				}
			}
#ifdef DEBUG
		} else {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_strategy: async buf 0x%p ignored\n", bp));
#endif /* DEBUG */
		}
	} else if ( bp->b_flags & B_READ ) {
		error = cachefsio_read(cp, bp, cp->c_cred);
	} else { /* B_WRITE */
		error = cachefsio_write(cp, bp, cp->c_cred);
	}

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_strategy: EXIT error %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefs_strategy: EXIT ESTALE\n"));
	return;
}

krwlock_t reclaim_lock;

/* ARGSUSED */
int
cachefs_reclaim(bhv_desc_t *bdp, int flag)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC( bdp );
#ifdef DEBUG
	int error = 0;
#endif /* DEBUG */
	int ospl;
	fscache_t *fscp;

	CFS_DEBUG(CFSDEBUG_VOPS,
		printf("cachefs_reclaim: ENTER vp 0x%p, flag 0x%x\n", vp, flag));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_reclaim, current_pid(),
		vp, flag);
	ASSERT(VALID_ADDR(vp));
	CACHEFS_STATS->cs_vnops++;
	/*
	 * always purge from the dnlc
	 */
	dnlc_purge_vp( vp );
	if ( cp ) {
		ASSERT(cp->c_frontvp || !(cp->c_flags & CN_UPDATED));
		ASSERT(!(cp->c_flags & CN_WRITTEN));
		CACHEFS_STATS->cs_reclaims++;
		fscp = C_TO_FSCACHE(cp);
		ASSERT(fscp);
		/*
		 * always flush, but don't always sync the file header
		 */
#ifdef DEBUG
		error = cachefs_fsync(bdp, FSYNC_INVAL | FSYNC_WAIT, sys_cred,
					(off_t)0, (off_t)-1);
		CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_ERROR,
			if ( error ) printf("cachefs_reclaim: flush error %d\n", error));
#else /* DEBUG */
		(void)cachefs_fsync(bdp, FSYNC_INVAL | FSYNC_WAIT, sys_cred,
					(off_t)0, (off_t)-1);
#endif /* DEBUG */
		mutex_enter(&fscp->fs_cnodelock);
		ospl = mutex_spinlock(&cp->c_statelock);
		cp->c_vnode = NULL;
		/*
		 * We do not reclaim cnodes for directories with non-zero
		 * reference counts.  But we tell the caller that the vnode
		 * has been reclaimed.  We can find the cnode later if we
		 * want it.
		 * If c_backvp is non-NULL, we cannot reclaim here.  Mark
		 * it CN_DESTROY so fscache_sync will get rid of it.
		 * If the cnode has been updated (i.e., the file header needs
		 * to be written), we cannot reclaim.
		 */
		if (cp->c_refcnt || cp->c_backvp || cp->c_frontvp ||
			(cp->c_flags & CN_UPDATED)) {
				ASSERT(cp->c_inhash);
				vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
				if (!cp->c_refcnt) {
					cp->c_flags |= CN_DESTROY;
					cachefs_remhash(cp);
					cp->c_hash = fscp->fs_reclaim;
					fscp->fs_reclaim = cp;
				}
				mutex_spinunlock(&cp->c_statelock, ospl);
				mutex_exit(&fscp->fs_cnodelock);
				return(0);
		} else {
			cp->c_flags |= CN_RECLAIM;
		}
		mutex_spinunlock(&cp->c_statelock, ospl);
		cachefs_remhash(cp);
		cp->c_flags |= CN_DESTROY;
		ASSERT(!cp->c_frontvp && !cp->c_frontdirvp);
		/*
		 * Use fs_cnodelock to make sure that we do not free the cnode data
		 * while makecachefsnode or fscache_sync are using it.
		 */
		vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
		cachefs_cnode_free( cp );
		mutex_exit(&fscp->fs_cnodelock);
	}
	CFS_DEBUG(CFSDEBUG_VOPS, printf("cachefs_reclaim: EXIT\n", error));
	return( 0 );
}

/* ARGSUSED */
int
cachefs_fcntl(bhv_desc_t *bdp, int cmd, void *arg, int flags, off_t offset,
	cred_t *cr, rval_t *rvp)
{
	cnode_t *cp = BHVTOC(bdp);
	int error = 0;

	(void)BACK_VOP_FCNTL(cp, cmd, arg, flags, offset, cr, rvp,
		BACKOP_BLOCK, &error);
	/*
	 * since there are a number of fcntls which result in attribute
	 * changes, we need to expire the object to make sure we go
	 * to the back file system to get the updated attributes (if any)
	 */
	CFSOP_EXPIRE_COBJECT(C_TO_FSCACHE(cp), cp, sys_cred);
	return(error);
}

void
cachefs_vnode_change(
        bhv_desc_t	*bdp, 
        vchange_t 	cmd, 
        __psint_t 	val)
{
	/* 
	 * In addition to applying to the backing vnode, also apply
	 * to the cachefs vnode since it's the one visible to the vnode
	 * layer above.  
	 */
	(void) BACK_VOP_VNODE_CHANGE(BHVTOC(bdp), cmd, val, BACKOP_BLOCK);
	fs_vnode_change(bdp, cmd, val);
}

int
cachefs_attr_get(bhv_desc_t *bdp, char *name, char *value, int *valuelenp,
		 int flags, cred_t *cr)
{
	cnode_t *cp = BHVTOC(bdp);
	int error = 0;

	(void) BACK_VOP_ATTR_GET(cp, name, value, valuelenp, flags, cr,
				 BACKOP_BLOCK, &error);
	return(error);
}

int
cachefs_attr_set(bhv_desc_t *bdp, char *name, char *value, int valuelen,
		 int flags, cred_t *cr)
{
	cnode_t *cp = BHVTOC(bdp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;

	(void) BACK_VOP_ATTR_SET(cp, name, value, valuelen, flags, cr,
				 BACKOP_BLOCK, &error);
	CFSOP_MODIFY_COBJECT(fscp, cp, cr);
	return(error);
}

int
cachefs_attr_remove(bhv_desc_t *bdp, char *name, int flags, cred_t *cr)
{
	cnode_t *cp = BHVTOC(bdp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;

	(void) BACK_VOP_ATTR_REMOVE(cp, name, flags, cr, BACKOP_BLOCK, &error);
	CFSOP_MODIFY_COBJECT(fscp, cp, cr);
	return(error);
}

int
cachefs_attr_list(bhv_desc_t *bdp, char *buffer, int bufsize, int flags,
		  struct attrlist_cursor_kern *cursor, cred_t *cr)
{
	cnode_t *cp = BHVTOC(bdp);
	int error = 0;

	(void) BACK_VOP_ATTR_LIST(cp, buffer, bufsize, flags, cursor, cr,
				  BACKOP_BLOCK, &error);
	return(error);
}
