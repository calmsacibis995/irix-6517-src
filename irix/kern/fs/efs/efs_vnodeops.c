/*
 * EFS vnode operations.
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.213 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/dnlc.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <sys/iograph.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/mode.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/pfdat.h>		/* page flushing prototypes */
#include <sys/poll.h>
#include <sys/quota.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <sys/capability.h>
#include <sys/flock.h>
#include <sys/kfcntl.h>
#include <fs/specfs/spec_lsnode.h>
#include <string.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
#include "efs_inode.h"
#include "efs_dir.h"
#include "efs_sb.h"

#ifdef DATAPIPE
/* data pipe functions */
extern int fspe_get_ops(void *);
int        efs_fspe_dioinfo(struct vnode *, struct dioattr *);
#endif

static int efs_readi(struct inode *, struct uio *, int, u_short, struct cred *,
				struct flid *);
static int efs_writei(struct inode *, struct uio *, int, struct cred *,
				struct flid *);
static void efs_rwunlock(bhv_desc_t *, vrwlock_t);
static void efs_rwlock(bhv_desc_t *, vrwlock_t);
static int efs_fid(bhv_desc_t *, struct fid **);
static int efs_fid2(bhv_desc_t *, struct fid *);
static int efs_setattr(bhv_desc_t *, struct vattr *,int ,struct cred *);

#if _MIPS_SIM == _ABI64
int irix5_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5(void *, int, xlate_info_t *);
int irix5_n32_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5_n32(void *, int, xlate_info_t *);
#endif

/* 
 * EFS direct I/O can be other than page aligned as long as we report
 * the maximum transfer size as the maximum number of pages minus 1.
 * This takes care of the case where the I/O is not page aligned, but
 * it is of maxdmasz size.  We go with BBSIZE for the alignment, because
 * that is what it has always been.
 */
#define FDIRIOALIGN	BBSIZE

#define EFS_INVALIDOFF(off)	(((off) < 0) || ((off) > SEEKLIMIT32))

/*
 * No open action is required for regular files.  Devices are handled
 * through the specfs file system, pipes through fifofs.  Device and
 * fifo vnodes are "wrapped" by specfs and fifofs vnodes, respectively,
 * when a new vnode is first looked up or created.
 */

/* ARGSUSED */
static int
efs_close(
	bhv_desc_t *bdp,
	int flag,
	lastclose_t lastclose,
	struct cred *cr)
{
	return 0;
}

/* ARGSUSED */
static int
efs_read(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr,
	struct flid *fl)
{
	struct inode *ip;
	u_short type;
	int error;

	if (!(ioflag & IO_ISLOCKED))
		efs_rwlock(bdp, VRWLOCK_READ);

	ip = bhvtoi(bdp);
	ASSERT(ip->i_flags & IRWLOCK);
	type = ip->i_mode & IFMT;

	error =  efs_readi(ip, uiop, ioflag, type, cr, fl);

	if (!(ioflag & IO_ISLOCKED))
		efs_rwunlock(bdp, VRWLOCK_READ);

	return error;
}

/* ARGSUSED */
static int
efs_write(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr,
	struct flid *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct inode *ip;
	int error;

	if (!(ioflag & IO_ISLOCKED))
		efs_rwlock(bdp, VRWLOCK_WRITE);

	ip = bhvtoi(bdp);
	ASSERT(ip->i_flags & IRWLOCK);
	if (vp->v_type == VREG) {
		if (ioflag & IO_APPEND) {
			/*
			 * In append mode, start at end of file.
			 */
			uiop->uio_offset = ip->i_size;
		}
	}
	error = efs_writei(ip, uiop, ioflag, cr, fl);

	if (!(ioflag & IO_ISLOCKED))
		efs_rwunlock(bdp, VRWLOCK_WRITE);

	return error;
}

#define MAXDIOSPLIT	20

struct dio_s {
	bhv_desc_t *bdp;
	struct cred *cr;
	int ioflag;
};

static int
diostrat(buf_t *bp)
{
	struct dio_s *dp;
	bhv_desc_t *bdp;
	struct vnode *vp;
	scoff_t offset, pushstart;
	int count, i, j, n, totxfer;
	buf_t *bps[MAXDIOSPLIT];
	int dlen = 1, dbn;
	struct bmapval bmv;
	buf_t *nbp;
	caddr_t base;
	int error, resid, totresid;
	int trail = 0;
	struct inode *ip;
	scoff_t	o_size;

	ASSERT(!(bp->b_flags & B_DONE));

	dp = (struct dio_s*) bp->b_private;
	bdp = dp->bdp;
	vp = BHV_TO_VNODE(bdp);
	ip = bhvtoi(bdp);
	o_size = ip->i_size;
	offset = BBTOB(bp->b_blkno);
	totresid = count = bp->b_bcount;
	base = bp->b_un.b_addr;
	error = resid = totxfer = 0;

	while ( !error && count && !trail && dlen ) {
		for ( i = 0 ; (i < MAXDIOSPLIT) && count && !trail ; i++ ) {
			/* build an io */
			for ( dlen = 0 ; dlen < count ; ) {
				n = 1;
				error = efs_bmap(bdp, offset+dlen, count-dlen,
					bp->b_flags&B_READ, dp->cr, &bmv, &n);

				if (error || (bmv.pbsize == 0))
					break;

				/* prime the pump */
				if (dlen == 0) {
					dbn = bmv.bn + BTOBB(bmv.pboff);
					pushstart = BBTOB(bmv.offset) & ~NBPP;
				} else if (dbn + BTOBB(dlen) != bmv.bn + BTOBB(bmv.pboff))
					break;

				dlen += bmv.pbsize;

				/* see if the file grew from writes */
				if ((bp->b_flags & B_READ) == 0 &&
				    offset + dlen > ip->i_size ) {
					ASSERT((vp->v_flag & VISSWAP) == 0);
					ip->i_size = offset + dlen;
					ip->i_flags |= ITRUNC;
				}
			}

			/* end of file or an error */
			if ( (dlen == 0) || error )
				break;

			/*
			 * Flush out delwri data.
			 */
			if (!(dp->ioflag & IO_IGNCACHE)) {
				off_t end = BBTOB(bmv.offset + bmv.length);
				VOP_FLUSHINVAL_PAGES(vp, (off_t)pushstart, ctob(btoc(end)) - 1,
					FI_NONE);
			}

			/* check for partial reads at end of file */
			if ( dlen & BBMASK ) {
				ASSERT(bp->b_flags & B_READ);
				trail = dlen;
				dlen &= ~BBMASK;
				dlen += BBSIZE;
			}

			/* trim back xfer */
			if ( dlen > count )
				dlen = count;

			/* get the information from disk */
			bps[i] = nbp = getphysbuf(bp->b_edev);
			nbp->b_flags = bp->b_flags;
			nbp->b_error = 0;
			nbp->b_blkno = dbn;
			nbp->b_bcount = dlen;
			nbp->b_un.b_addr = base;

			VOP_STRATEGY(ip->i_mount->m_devvp,nbp);
			if (error = geterror(nbp)) {
				biowait(nbp);
				nbp->b_flags = 0;
				putphysbuf(nbp);
				break;
			}

			/* correct for partial reads */
			if( trail )
				dlen = trail;

			base += dlen;
			offset += dlen;
			count -= dlen;
		}

		/* recover the buffers */
		for ( j = 0 ; j < i ; j++ ) {

			nbp = bps[j];
			biowait(nbp);

			/* check for an error */
			if ( !error )
				error = geterror(nbp);

			if ( !error && !resid ) {
				resid = nbp->b_resid;

				/* prevent adding up partial xfers */
				if( trail && (j == (i-1)) ) {
					/* correct for partial reads */
					if( resid <= nbp->b_bcount - trail )
						totxfer += trail;
				}
				else
					totxfer += nbp->b_bcount - resid;
			}

			nbp->b_flags = 0;
			putphysbuf(nbp);
		}
	}

	/* if any of the io's fail, the whole thing fails */
	if ( error ) {
		totxfer = 0;
		if (((bp->b_flags & B_READ) == 0) && !(vp->v_flag & VISSWAP))
			efs_itrunc(ip, o_size, 0);
	}

	bp->b_resid = totresid - totxfer;

	/* see if the file grew from writes */
	if ( (bp->b_flags & B_READ) == 0 ) {
		timespec_t tv;

		if ((ip->i_mode & (ISUID|ISGID)) &&
		    !cap_able_cred(dp->cr, CAP_FSETID)) {
			ip->i_mode &= ~ISUID;
			if (ip->i_mode & (IEXEC >> 3))
				ip->i_mode &= ~ISGID;
		}
		nanotime_syscall(&tv);
		ip->i_flags |= IMOD;
		ip->i_mtime = ip->i_ctime = tv.tv_sec;
		ip->i_umtime = tv.tv_nsec;
	}

	bioerror(bp,error);
	biodone(bp);

	/* make the compiler happy */
	return 0;
}

static int
efs_diordwr(bhv_desc_t *bdp, struct uio *uiop, struct cred *cr, int ioflag,
	    uint64_t dirflg)
{
	struct inode *ip = bhvtoi(bdp);
	struct dio_s dp;
	buf_t *bp;
	int error;

	/* special case caused by trailing reads */
	if (dirflg & B_READ ){
		if (ip->i_size == uiop->uio_offset)
			return 0;
	}
	
	/* do alignment checks */
	if (((__psint_t)uiop->uio_iov->iov_base & (FDIRIOALIGN-1)) 
	    || (uiop->uio_offset & BBMASK) || (uiop->uio_resid & BBMASK))
		return EINVAL;

	/* do maxio check */
	if (uiop->uio_resid > ctob(v.v_maxdmasz - 1))
		return EINVAL;

	bp = getphysbuf(ip->i_dev);

	/* save the info for later... */
	dp.bdp = bdp;
	dp.cr = cr;
	dp.ioflag = ioflag;

	bp->b_private = &dp;

	error = biophysio(diostrat, bp, bp->b_edev, dirflg,
			  (daddr_t)BTOBB(uiop->uio_offset), uiop);

	bp->b_flags = 0;
	putphysbuf(bp);

	return error;
}


#define	COPYOUT(bp,off,len,uio)	biomove(bp,off,len,UIO_READ,uio)
#define	COPYIN(bp,off,len,uio)	biomove(bp,off,len,UIO_WRITE,uio)

#define NREADIMAPS 4
static int
efs_readi(struct inode *ip,
	struct uio *uio,
	int ioflag,
	u_short type,
	struct cred *cr,
	struct flid *fl)
{
	register off_t offset;
	int error, n, i;
	struct vnode *vp;
	struct bmapval bmv[NREADIMAPS];
	int nmaps;
	struct buf *bp;
	timespec_t tv;

	ASSERT(type == IFREG || type == IFDIR || ISLINK(type) || 
	       type == IFSOCK);
	vp = itov(ip);
	offset = uio->uio_offset;

	/* check for locks if some exist and mandatory locking is enabled */
	if ((vp->v_flag & (VENF_LOCKING|VFRLOCKS)) == 
	    (VENF_LOCKING|VFRLOCKS)) {
		error = fs_checklock(vp, FREAD, offset, uio->uio_resid,
				     uio->uio_fmode, cr, fl, VRWLOCK_READ);
		if (error)
			return error;
	}

	if (EFS_INVALIDOFF(offset))
		return EINVAL;
	if (uio->uio_resid <= 0)
		return 0;

	/*
	 * Do the following only for writeable file systems.
	 * This closes a POSIX conformance bug which says that a read on a
	 * file in a read-only file system should not update its access
	 * time. Also, logically, there's no point in updating the atime
	 * as it is never going to be written back to disk.
	 */
	if (!(itovfs(ip)->vfs_flag & VFS_RDONLY)) {
		nanotime_syscall(&tv);
		ip->i_flags |= IMOD;
		ip->i_atime = tv.tv_sec;
	}
	switch (type) {
	  case IFLNK:
	  case IFCHRLNK:
	  case IFBLKLNK:
		/* in-line sym link? */
		if (ip->i_numextents == 0) {
			ASSERT(ip->i_size <= EFS_MAX_INLINE);

			/* paranoia when asserts are gone... */
			n = MIN(ip->i_size, EFS_MAX_INLINE);
			if ((n -= uio->uio_offset) <= 0) {
				error = 0;
				break;
			}
			n = MIN(uio->uio_resid, n);

			error = uiomove((char *)ip->i_extents, n, UIO_READ,
								uio);
			break;
		}
		/* fall through */
	  case IFDIR:
		do {
			nmaps = 2;
			error = efs_bmap(itobhv(ip), uio->uio_offset, uio->uio_resid,
					 B_READ, cr, bmv, &nmaps);
			if (error || bmv[0].pbsize == 0)
				break;
			ASSERT(bmv[0].bn >= 0);

			if (nmaps > 1)
				bp = breada(bmv[0].pbdev,
					bmv[0].bn, bmv[0].length,
					bmv[1].bn, bmv[1].length);
			else
				bp = bread(bmv[0].pbdev,
					   bmv[0].bn, bmv[0].length);

			if (bp->b_flags & B_ERROR)
				error = bp->b_error;
			else if (bp->b_resid)
				n = 0;
			else {
				n = bmv[0].pbsize;
				error = COPYOUT(bp, bmv[0].pboff, n, uio);
			}
			brelse(bp);

		} while (!error && uio->uio_resid != 0 && n != 0);
		break;

	case IFREG:
	    	if (ioflag & IO_RSYNC) {
			/* First we sync the data */
		    if ((ioflag & IO_SYNC) || (ioflag & IO_DSYNC)) {
			VOP_FLUSH_PAGES(vp, (off_t)0, ip->i_size - 1, 0, FI_NONE, error );
			error = 0;
		    }
		    if ((ip->i_remember < ip->i_size) || (ioflag & IO_SYNC)) {
			ip->i_flags |= ISYN;
			ip->i_remember = ip->i_size;
			efs_iupdat(ip);
		    }
		}

		if (ioflag & IO_DIRECT) {
			error = efs_diordwr(itobhv(ip), uio, cr, ioflag,
					    B_READ);
			break;
		}

		do {
			nmaps = NREADIMAPS;
			error = efs_bmap(itobhv(ip), uio->uio_offset, uio->uio_resid,
					 B_READ, cr, bmv, &nmaps);

			if (error || (n = bmv[0].pbsize) == 0)
				break;

			/*
			 * Pass on the policy modules from our caller
			 * to the chunk cache.
			 */
			for (i = 0; i < nmaps; i++) {
				bmv[i].pmp = uio->uio_pmp;
			}

			bp = chunkread(vp, bmv, nmaps, cr);

			if (bp->b_flags & B_ERROR)
				error = bp->b_error;
			else if (bp->b_resid)
				n = 0;
			else
				error = COPYOUT(bp, bmv[0].pboff, n, uio);

			brelse(bp);

		} while (!error && uio->uio_resid != 0 && n != 0);
		break;

	case IFSOCK:
		error = ENODEV;
	}
	return error;
}

extern int efs_inline;
static int
efs_writei(struct inode *ip,
	struct uio *uio,
	int ioflag,
	struct cred *cr,
	struct flid *fl)
{
	int		type, error, n, count, resid;
	struct vnode *	vp;
	register off_t	offset;
	struct bmapval	bmv;
	int		nmaps;
	struct buf *	bp;
	int		dotime = 0;
	off_t		limit;

	type = ip->i_mode & IFMT;
	ASSERT(type == IFREG || type == IFDIR || ISLINK(type) || 
	       type == IFSOCK);
	vp = itov(ip);
	offset = uio->uio_offset;
	count = uio->uio_resid;

	/* check for locks if some exist and mandatory locking is enabled */
	if ((vp->v_flag & (VENF_LOCKING|VFRLOCKS)) == 
	    (VENF_LOCKING|VFRLOCKS)) {
	    error = fs_checklock(vp, FWRITE, offset, count, uio->uio_fmode, 
				 cr, fl, VRWLOCK_WRITE);
	    if (error)
		return error;
	}

	if ( EFS_INVALIDOFF(offset) || EFS_INVALIDOFF(offset + count) )
		return EINVAL;
	if (count <= 0)
		return 0;

	switch (type) {
	  case IFLNK:
	  case IFCHRLNK:
	  case IFBLKLNK:
		/*
		 * Create an in-line sym link iff there's room.
		 */
		ASSERT(offset == 0);
		ASSERT(ip->i_numextents == 0);
		if (efs_inline && count <= EFS_MAX_INLINE) {
			irealloc(ip, count);
			error = uiomove((char *)ip->i_extents, count,
					UIO_WRITE, uio);
			if (!error) {
				ip->i_size = count;
				dotime = 1;
			}
			break;
		}
	  case IFDIR:
		do {
			nmaps = 1;
			if (error = efs_bmap(itobhv(ip), uio->uio_offset,
					     uio->uio_resid, B_WRITE, cr,
					     &bmv, &nmaps))
				break;
			bp = ((n = bmv.pbsize) == bmv.bsize) ?
				getblk(bmv.pbdev, bmv.bn, bmv.length) :
				bread(bmv.pbdev, bmv.bn, bmv.length);

			if (error = COPYIN(bp, bmv.pboff, n, uio)) {
				brelse(bp);
				break;
			}

			if (uio->uio_offset > ip->i_size)
				ip->i_size = uio->uio_offset;
			dotime = 1;

			if ((ioflag & IO_SYNC) || (ioflag & IO_DSYNC))
				bwrite(bp);
			else
				bdwrite(bp);
		} while (uio->uio_resid != 0 && n != 0);
		break;

	  case IFREG:
		limit = MIN(uio->uio_limit, (off_t)EFS_MAX_FILE_OFFSET);
		n = (int)(limit - uio->uio_offset);
		if (n <= 0)
			return EFBIG;
		if (n < uio->uio_resid) {	/* only do partial write */
			resid = uio->uio_resid - n;
			uio->uio_resid = n;
		} else {
			resid = 0;
		}

		if (ioflag & IO_DIRECT) {
			error = efs_diordwr(itobhv(ip), uio, cr, ioflag,
					    B_WRITE);
			/* add back remainder of write */
			uio->uio_resid += resid;
			break;
		}

		do {
			nmaps = 1;
			if (error = efs_bmap(itobhv(ip), uio->uio_offset,
						uio->uio_resid, B_WRITE, cr,
						&bmv, &nmaps))
				break;

			/*
			 * We must bread the buffer if the write doesn't
			 * completely overwrite the buffer and the write
			 * either begins after the start of the buffer or
			 * ends before the current end of file.
			 */
			bmv.pmp = uio->uio_pmp;
			if ((n = bmv.pbsize) != bmv.bsize
			 && (bmv.pboff != 0 || uio->uio_offset != ip->i_size))
				bp = chunkread(vp, &bmv, 1, cr);
			else
				bp = getchunk(vp, &bmv, cr);

			if (bp->b_flags & B_ERROR) {
				error = bp->b_error;
				brelse(bp);
				break;
			}

			if (error = COPYIN(bp, bmv.pboff, n, uio)) {
				if (!(bp->b_flags & B_DONE))
					bp->b_flags |= B_STALE|B_DONE|B_ERROR;
				brelse(bp);
				break;
			}

			/*
			 * Update file size if COPYIN extended uio_offset.
			 */
			if (uio->uio_offset > ip->i_size) {
				ip->i_size = uio->uio_offset;
				ip->i_flags |= ITRUNC;
			}

			/*
			 * Mark inode modified and clear suid and sgid if
			 * not superuser.
			 */
			dotime = 1;
			if ((ip->i_mode & (ISUID|ISGID)) &&
			    !cap_able_cred(cr, CAP_FSETID)) {
				ip->i_mode &= ~ISUID;
				if (ip->i_mode & (IEXEC >> 3))
					ip->i_mode &= ~ISGID;
			}

			if ((ioflag & IO_SYNC) || (ioflag & IO_DSYNC))
				bwrite(bp);
			else
				bdwrite(bp);

		} while (uio->uio_resid != 0 && n != 0);

		uio->uio_resid += resid; /* add back remainder of write */
		break;

	case IFSOCK:
		error = ENODEV;
		break;
	}

	/*
	 * If we've already done a partial write, terminate
	 * the write but return no error.
	 */
	if (count != uio->uio_resid) {
		error = 0;
	}

	/*
	 * Set timestamps.  Don't put it off, we want the time to
	 * be reasonably accurate.
	 */
	if (dotime) {
		timespec_t tv;

		nanotime_syscall(&tv);
		ip->i_flags |= IMOD;
		ip->i_mtime = ip->i_ctime = tv.tv_sec;
		ip->i_umtime = tv.tv_nsec;
	}

	/*
	 * Update the inode only if inode changed.
	 * We set i_remember to i_size to ensure that the data
	 * written is actually permanent in the inode.
	 */
	if ((ioflag & (IO_SYNC | IO_DSYNC)) &&
	    (ip->i_flags & ITRUNC) &&
	    !(vp->v_flag & VISSWAP) &&
	    !error) {
		ip->i_flags |= ISYN;
		ip->i_remember = ip->i_size;
		error = efs_iupdat(ip);
	}

	return error;
}



/* ARGSUSED */
static int
efs_ioctl(
	bhv_desc_t *bdp,
	int cmd,
	void *arg,
	int flag,
	struct cred *cr,
	int *rvalp,
        struct vopbd *vbds)
{
	return ENOTTY;
}

/* ARGSUSED */
static int
efs_getattr(bdp, vap, flags, cr)
	bhv_desc_t *bdp;
	struct vattr *vap;
	int flags;
	struct cred *cr;
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct inode *ip;
	u_short type;

	ip = bhvtoi(bdp);

	vap->va_size = ip->i_size;
	if (vap->va_mask == AT_SIZE)
		return 0;
	vap->va_fsid = ip->i_dev;
	vap->va_nodeid = ip->i_number;
	vap->va_nlink = ip->i_nlink;
	vap->va_gencount = ip->i_gen;
	if (!(vap->va_mask & ~(AT_FSID|AT_NODEID|AT_NLINK|AT_GENCOUNT|AT_SIZE)))
		return 0;

	/*
	 * POSIX stat etc. require that any pending update flags
	 * be dealt with and cleared upon return from stat.
	 * Since we defer updating the inode on setting these flags
	 * we must pay now.  Rather than really go through the
	 * entire efs_iupdat, we simply get the times up to date.
	 * This emulates the setattr code below.
	 */
	if (ip->i_flags & (IACC|IUPD|ICHG)) {
		timespec_t tv;

		nanotime_syscall(&tv);
		ilock(ip);
		if (ip->i_flags & IACC)
			ip->i_atime = tv.tv_sec;
		if (ip->i_flags & IUPD) {
			ip->i_mtime = tv.tv_sec;
			ip->i_umtime = tv.tv_nsec;
		}
		if (ip->i_flags & ICHG)
			ip->i_ctime = tv.tv_sec;
		ip->i_flags &= ~(IACC|IUPD|ICHG);
		ip->i_updtimes = 0;
		ip->i_flags |= IMOD;
		iunlock(ip);
	} else if (ip->i_updtimes) {
		ilock(ip);
		ip->i_updtimes = 0;
		ip->i_flags |= IMOD;
		iunlock(ip);
	}

	/*
	 * Copy from in-core inode.
	 */
	vap->va_type = vp->v_type;
	vap->va_mode = ip->i_mode & MODEMASK;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_vcode = ip->i_vcode;
        if (vp->v_type == VCHR || vp->v_type == VBLK)
                vap->va_rdev = ip->i_rdev;
        else
                vap->va_rdev = 0;       /* not a b/c spec. */
	vap->va_atime.tv_sec = ip->i_atime;
	vap->va_atime.tv_nsec = 0;
	vap->va_mtime.tv_sec = ip->i_mtime;
	vap->va_mtime.tv_nsec = ip->i_umtime;
	vap->va_ctime.tv_sec = ip->i_ctime;
	vap->va_ctime.tv_nsec = 0;

	type = ip->i_mode & IFMT;
	switch (type) {
	  case IFBLK:
	  case IFCHR:
		vap->va_blksize = BLKDEV_IOSIZE;
		break;

	  case IFCHRLNK:
	  case IFBLKLNK:
		vap->va_rdev = HWGRAPH_STRING_DEV;
		vap->va_blksize = BLKDEV_IOSIZE;
		break;
	  default:
		vap->va_blksize = 1 << itoefs(ip)->fs_lbshift;
	}
	vap->va_nblocks = BTOBB(ip->i_size);
	vap->va_xflags = 0;
	vap->va_extsize = 0;
	vap->va_nextents = 0;
	vap->va_anextents = 0;
	vap->va_projid = 0;
	return 0;
}

static int
efs_setattr(bdp, vap, flags, cr)
	bhv_desc_t *bdp;
	struct vattr *vap;
	int flags;
	struct cred *cr;
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int mask;
	struct inode *ip;
	int error;
	timespec_t tv;
	int mandlock_before, mandlock_after;
	int file_owner;

	/*
	 * Cannot set certain attributes.
	 */
	mask = vap->va_mask;
	if (mask & AT_NOSET)
		return EINVAL;

	ip = bhvtoi(bdp);

	if (mask & AT_UPDTIMES) {
		ASSERT((mask & ~AT_UPDTIMES) == 0);
		nanotime_syscall(&tv);
		if (mask & AT_UPDATIME)
			ip->i_atime = tv.tv_sec;
		if (mask & AT_UPDCTIME)
			ip->i_ctime = tv.tv_sec;
		if (mask & AT_UPDMTIME) {
			ip->i_mtime = tv.tv_sec;
			ip->i_umtime = tv.tv_nsec;
		}
		ip->i_updtimes = 1;
		return 0;
	}

	ilock(ip);
	error = 0;

	/* determine whether mandatory locking mode changes */
	mandlock_before = MANDLOCK(vp, ip->i_mode);

	file_owner = (cr->cr_uid == ip->i_uid);

	if (mask & (AT_MODE|AT_UID|AT_GID)) {
		/*
		 * CAP_FOWNER overrides the following restrictions:
		 *
		 * The user ID of the calling process must be equal
		 * to the file owner ID, except in cases where the
		 * CAP_FSETID capability is applicable.
		 */
		if (!file_owner && !cap_able_cred(cr, CAP_FOWNER)) {
			error = EPERM;
			goto out;
		}
	}

	/*
	 * Change file access modes.  Must be owner or privileged.
	 */
	if (mask & AT_MODE) {
		mode_t m = 0;

		/*
		 * CAP_FSETID overrides the following restrictions:
		 *
		 * The effective user ID of the calling process shall match
		 * the file owner when setting the set-user-ID and
		 * set-group-ID bits on that file.
		 *
		 * The effective group ID or one of the supplementary group
		 * IDs of the calling process shall match the group owner of
		 * the file when setting the set-group-ID bit on that file
		 */
		if ((vap->va_mode & ISUID) && !file_owner)
			m |= ISUID;
		if ((vap->va_mode & ISGID) &&
		    !groupmember(ip->i_gid, cr))
			m |= ISGID;
		if ((vap->va_mode & ISVTX) && vp->v_type != VDIR)
			m |= ISVTX;
		if (m && !cap_able_cred(cr, CAP_FSETID))
			vap->va_mode &= ~m;

		ip->i_mode &= IFMT;
		ip->i_mode |= vap->va_mode & ~IFMT;
		ip->i_flags |= ICHG;
	}

	
	/*
	 * Change file ownership.  Must be the owner or privileged.
	 * If the system was configured with the "restricted_chown"
	 * option, the owner is not permitted to give away the file,
	 * and can change the group id only to a group of which he
	 * or she is a member.
	 */
	if (mask & (AT_UID|AT_GID)) {
		uid_t uid = (mask & AT_UID) ? vap->va_uid : ip->i_uid;
		gid_t gid = (mask & AT_GID) ? vap->va_gid : ip->i_gid;

		/* Prevent long uids from being silently truncated to 16bits */
		if (uid > 0xffff || gid > 0xffff)
		{
		    	error = EOVERFLOW;
			goto out;
		}

		/*
		 * CAP_CHOWN overrides the following restrictions:
		 *
		 * If _POSIX_CHOWN_RESTRICTED is defined, this capability
		 * shall override the restriction that a process cannot
		 * change the user ID of a file it owns and the restriction
		 * that the group ID supplied to the chown() function
		 * shall be equal to either the group ID or one of the
		 * supplementary group IDs of the calling process.
		 */
		if (restricted_chown &&
		    (ip->i_uid != uid || (ip->i_gid != gid &&
					  !groupmember(gid, cr))) &&
		    !cap_able_cred(cr, CAP_CHOWN)) {
			error = EPERM;
			goto out;
		}

		/*
		 * CAP_FSETID overrides the following restrictions:
		 *
		 * The set-user-ID and set-group-ID bits of a file will be
		 * cleared upon successful return from chown()
		 */
		if ((ip->i_mode & (ISUID|ISGID)) &&
		    !cap_able_cred(cr, CAP_FSETID)) {
			ip->i_mode &= ~(ISUID|ISGID);
		}

		if (ip->i_uid == uid) {
			/*
			 * XXX This won't work once we have group quotas
			 */
			ip->i_gid = gid;
		} else {
			long change = BTOBB(ip->i_size);

#ifdef _SHAREII
			if ((error = SHR_CHOWNDISK
					(
						itovfs(ip),
						ip->i_uid,
						vap->va_uid,
						(u_long)ip->i_blocks,
						DEV_BSIZE,
						cr
					)
			     )
			   )
				goto out;
#endif /* _SHAREII */
			/*
			 * We force the changes to the quota structure, hence we
			 * cannot fail because of want of space! Kludgy.
			 */
			(void) qt_chkdq(ip, -change, 1, NULL);
			(void) qt_chkiq(ip->i_mount, ip, (u_int)ip->i_uid, 1);
			qt_dqrele(ip->i_dquot);
			ip->i_uid = uid;
			ip->i_gid = gid;
			ip->i_dquot = qt_getinoquota(ip);
			(void) qt_chkdq(ip, change, 1, NULL);
			(void) qt_chkiq(ip->i_mount, (struct inode *)NULL,
					(u_int)ip->i_uid, 1);
		}
		ip->i_flags |= ICHG;
	}

	/*
	 * Truncate file.  Must have write permission and not be a directory.
	 */
	if (mask & AT_SIZE) {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		} else if (vp->v_type != VREG) {
			error = EINVAL;
			goto out;
		}
		if (vp->v_flag & VISSWAP) {
			error = EACCES;
			goto out;
		}
		if (!(mask & AT_SIZE_NOPERM)) {
			if (error = efs_iaccess(ip, IWRITE, cr))
				goto out;
		}
		/* must UPD|CHG even though efs_itrunc may not do anything */
		ip->i_flags |= IUPD|ICHG;
		if (error = efs_itrunc(ip, (scoff_t)vap->va_size, 0))
			goto out;
	}

	/*
	 * Change file access or modified times.
	 */
	if (mask & (AT_ATIME|AT_MTIME)) {
		/*
		 * We turn off I* bits to denote that our 'vap' time
		 * not the current time is the most up-to-date
		 * We turn on IMOD to be sure that sooner or later
		 * the inode will still get pushed. Future access, mod, changes
		 * will simply turn on the respective I* bit and overwrite our
		 * value
		 */
		if (!file_owner && !cap_able_cred(cr, CAP_FOWNER)) {
			if (error = (flags & ATTR_UTIME) ?
				    EPERM : efs_iaccess(ip, IWRITE, cr))
				goto out;
		}
		/*
		 * since utime() always updates both mtime and atime
		 * ctime will always be set, as it need to be so there
		 * no reason to set ICHG
		 */
		ip->i_flags |= IMOD;
		if (mask & AT_ATIME) {
			ip->i_atime = vap->va_atime.tv_sec;
			ip->i_flags &= ~IACC;
		}
		if (mask & AT_MTIME) {
			nanotime_syscall(&tv);
			ip->i_mtime = vap->va_mtime.tv_sec;
			ip->i_umtime = vap->va_mtime.tv_nsec;
			ip->i_ctime = tv.tv_sec;
			ip->i_flags &= ~(IUPD|ICHG);
		}
	}

out:
	if (!error && (flags & (ATTR_EXEC|ATTR_LAZY)) == 0 &&
			(ip->i_flags & (IACC|IUPD|ICHG|IMOD))) {
		/* XXXjwag ordering issue w.r.t delwri */
		/* XXXjwag - why do we really have to call iupdat here?? */
		IGETINFO.ig_attrchg++;
		error = efs_iupdat(ip);
	}

	/*
	 * If the (regular) file's mandatory locking mode changed, then
	 * notify the vnode.  We do this under the inode lock to prevent
	 * racing calls to vop_vnode_change.
	 */
	mandlock_after = MANDLOCK(vp, ip->i_mode);
	if (mandlock_before != mandlock_after) {
		VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_ENF_LOCKING, 
				 mandlock_after);
	}

	iunlock(ip);

	return error;
}

/*
 * This function knows that vnode mode bits are really inode mode bits.
 */
/* ARGSUSED */
static int
efs_access(bdp, mode, cr)
	bhv_desc_t *bdp;
	int mode;
	struct cred *cr;
{
	struct inode *ip;
	int error;

	ip = bhvtoi(bdp);
	ilock(ip);
	error = efs_iaccess(ip, mode, cr);
	iunlock(ip);
	return error;
}

/* ARGSUSED */
static int
efs_readlink(
	bhv_desc_t *bdp,
	struct uio *uiop,
	struct cred *cr)
{
	struct inode *ip;
	int error;
	u_short type;


	ip = bhvtoi(bdp);
	type = ip->i_mode & IFMT;

	if (!ISLINK(type))
		return EINVAL;

	ilock(ip);
	error = efs_readi(ip, uiop, 0, type, cr, NULL);
	iunlock(ip);
	return error;
}

/* ARGSUSED */
static int
efs_fsync(bdp, flag, cr, start, stop)
	bhv_desc_t *bdp;
	int flag;
	struct cred *cr;
	off_t start;
	off_t stop;
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct inode *ip;
	int error = 0;

	ip = bhvtoi(bdp);
	ilock(ip);
	if (flag & FSYNC_INVAL) {
		if (ip->i_flags & IINCORE && ip->i_numextents > 0) {
			struct extent *ex = &ip->i_extents[ip->i_numextents-1];
			VOP_FLUSHINVAL_PAGES(vp,0,BBTOB(ex->ex_offset+ex->ex_length) - 1,
				FI_REMAPF_LOCKED);
		}
	} else {
		VOP_FLUSH_PAGES(vp, (off_t)0, ip->i_size - 1,
			(flag & FSYNC_WAIT) ? 0 : B_ASYNC, FI_NONE, error);
		error = 0;
	}
	if (!(flag & FSYNC_DATA) ||
	    (((ip->i_mode & IFMT) == IFREG) &&
	     (ip->i_remember < ip->i_size))) {
		if (flag & FSYNC_WAIT)
			ip->i_flags |= ISYN;
		/*
		 * Since we just flushed all the data in the file, so ahead
		 * and bump i_remember all the way up to i_size.  This will
		 * ensure that all of our data blocks are permanent.
		 */
		ip->i_remember = ip->i_size;
		error = efs_iupdat(ip);
	}
	iunlock(ip);
	return error;	/* XXX should start all and sleep on v_sync */
}

/* ARGSUSED */
static int
efs_inactive(bdp, cr)
	bhv_desc_t *bdp;
	struct cred *cr;
{
	iinactive(bhvtoi(bdp));
	return VN_INACTIVE_CACHE;
}

/*
 * Unix file system operations having to do with directory manipulation.
 */
/* ARGSUSED */
static int
efs_lookup(bdp, nm, vpp, pnp, flags, rdir, cr)
	bhv_desc_t *bdp;
	char *nm;
	struct vnode **vpp;
	struct pathname *pnp;
	int flags;
	struct vnode *rdir;
	struct cred *cr;
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	struct inode *dp, *ip;
	struct entry ent;
	int error;
	struct vnode *vp, *newvp;

	if (dvp->v_type != VDIR)
		return ENOTDIR;
	dp = bhvtoi(bdp);
	ilock(dp);
	error = efs_dirlookup(dp, nm, pnp, DLF_IGET|DLF_MUSTHAVE, &ent, cr);
	iunlock(dp);
	if (error)
		return error;
	ip = ent.e_ip;
	vp = itov(ip);
#ifdef _IRIX_LATER
	if ((ip->i_mode & ISVTX) && !(ip->i_mode & (IEXEC | IFDIR))
	    && efs_stickyhack) {
		VN_FLAGSET(vp, VISSWAP);
	}
#endif
	if (ip != dp)
		iunlock(ip);
	/*
	 * If vnode is a device return special vnode instead.
	 */
	if (ISVDEV(vp->v_type)) {
		newvp = spec_vp(vp, vp->v_rdev, vp->v_type, cr);

		VN_RELE(vp);
		if (newvp == NULL)
			return ENOSYS;
		vp = newvp;
	}

	*vpp = vp;
	return 0;
}

static int
efs_create(
	bhv_desc_t *bdp,
	char *name,
	struct vattr *vap,
	int flags,
	int mode,
	struct vnode **vpp,
	struct cred *cr)
{
	struct inode *dp, *ip;
	int error;
#ifdef CELL
	int truncated = 0;
#endif
	struct entry ent;
	struct vnode *vp, *newvp;

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}

	dp = bhvtoi(bdp);
	ip = NULL;
	ilock(dp);
	if (error = efs_dirlookup(dp, name, NULL, DLF_IGET, &ent, cr))
		goto bad;

	/*
	 * If no entry was found, allocate an inode and enter it in dp.
	 * If an entry already exists and this is a non-exclusive create,
	 * check permissions and allow access for non-directory inodes.
	 * Read-only create of an existing directory is also allowed.
	 * Fail an exclusive create of anything which already exists.
	 */
	ip = ent.e_ip;
	if (ip == NULL) {
		if (error = efs_iaccess(dp, IWRITE, cr))
			goto bad;
		/*
		 * XPG4 says create cannot allocate a file if the
		 * file size limit is set to 0.
		 */
		if (flags & VZFS) {
			error = EFBIG;
			goto bad;
		}
		if (error = efs_ialloc(dp, MAKEIMODE(vap->va_type,vap->va_mode),
				       1, (vap->va_mask & AT_RDEV) ?
					   vap->va_rdev : NODEV, &ip, cr)) {
			goto bad;
		}

		if (error = efs_direnter(dp, ip, &ent, cr)) {
			ip->i_nlink = 0;
			ip->i_flags |= ICHG;
			goto bad;
		}
		vp = itov(ip);
	} else {
		vp = itov(ip);
		if (flags & VEXCL)
			error = EEXIST;
		else if (vp->v_type == VDIR && (mode & IWRITE))
			error = EISDIR;
		else if (mode)
			error = efs_iaccess(ip, mode, cr);
		if (!error && vp->v_type == VREG && (vap->va_mask & AT_SIZE)) {
			/*
			 * Truncate regular file, if requested by caller.
			 * POSIX requires the time stamps be updated
			 * regardless of whether file actually changes.
			 */
			ip->i_flags |= IUPD|ICHG;
			error = efs_itrunc(ip, (scoff_t)vap->va_size, 0);
#ifdef CELL
			truncated = 1;
#endif
		}
		if (error)
			goto bad;
	}
	iunlock(dp);
	if (ip != dp)
		iunlock(ip);

#ifdef CELL
	if (truncated)
		VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_TRUNCATED, 0);
#endif
	/*
	 * If vnode is a device, return special vnode instead.
	 */
	if (ISVDEV(vp->v_type)) {
		newvp = spec_vp(vp, vp->v_rdev, vp->v_type, cr);

		VN_RELE(vp);
		if (newvp == NULL)
			return ENOSYS;
		vp = newvp;
	}

	*vpp = vp;
	return 0;

bad:
	iunlock(dp);
	if (ip) {
		if (ip == dp)
			irele(ip);
		else
			iput(ip);
	}

	return error;
}

/*
 * Bump ip's link count and update the disk inode.  Let the link count
 * overflow MAXLINK so our caller can unconditionally recover by calling
 * efs_droplink.
 */
static int
efs_bumplink(struct inode *ip)
{
	ASSERT(ip->i_lockid == get_thread_id());
	ASSERT(ip->i_nlink >= 0);

	if (ip->i_nlink++ >= MAXLINK)
		return EMLINK;
	ip->i_flags |= ICHG;
	return efs_iupdat(ip);
}

/*
 * Drop ip's link count and flag it for later update.  Too many links
 * can be fixed by fsck; too few and a directory may be left pointing
 * at an unallocated inode.
 */
static void
efs_droplink(struct inode *ip)
{
	ASSERT(ip->i_lockid == get_thread_id());
	ASSERT(ip->i_nlink > 0);

	ip->i_nlink--;
	ip->i_flags |= ICHG;
}

/* ARGSUSED */
static int
efs_remove(
	bhv_desc_t *bdp,
	char *nm,
	struct cred *cr)
{
	struct inode *dp, *ip;
	int error;
	struct entry ent;
	int link_removed = 0;

	dp = bhvtoi(bdp);
	ilock(dp);
	error = efs_dirlookup(dp, nm, NULL, DLF_IGET|DLF_MUSTHAVE|DLF_REMOVE,
			      &ent, cr);
	if (error) {
		iunlock(dp);
		return error;
	}

	ip = ent.e_ip;
	if (itov(ip)->v_vfsmountedhere)
		error = EBUSY;
	else if ((ip->i_mode & IFMT) == IFDIR)
		error = EPERM;
	else {
		error = efs_dirremove(dp, &ent, cr);
		if (!error) {
			efs_droplink(ip);
			link_removed = 1;
			error = efs_iupdat(ip);
		}
	}
	if (ip != dp)
		iunlock(ip);
	iunlock(dp);

	if (link_removed) {
		/*
		 * Let interposed file systems know about removed links.
		 */
		VOP_LINK_REMOVED(itov(ip), itov(dp), (ip)->i_nlink==0);
	}
	irele(ip);

	return error;
}

/*
 * Link a file or a directory.  Only the superuser is allowed to make a
 * link to a directory.  Take pains to increment the source inode's link
 * count and update it before entering it in the target directory.
 */
static int
efs_link(
	bhv_desc_t *tbdp,
	struct vnode *svp,
	char *tnm,
	struct cred *cr)
{
	struct vnode *realvp;
	struct inode *tdp, *sip;
	struct entry ent;
	int error;
	bhv_desc_t *src_bdp;
	vn_bhv_head_t *src_bhp;

	VOP_REALVP(svp, &realvp, error);
	if (error == 0)
		svp = realvp;
	if (svp->v_type == VDIR)
		return EPERM;
	/*
	 * For now, manually find the EFS behavior descriptor for
	 * the source vnode.  If it doesn't exist then something
	 * is wrong and we should just return an error.
	 * Eventually we need to figure out how link is going to
	 * work in the face of stacked vnodes.
	 */
	src_bhp = VN_BHV_HEAD(svp);
	src_bdp = vn_bhv_lookup_unlocked(src_bhp, &efs_vnodeops);
	if (src_bdp == NULL) {
		return EXDEV;
	}
	sip = bhvtoi(src_bdp);
	ilock(sip);
	error = efs_bumplink(sip);
	iunlock(sip);
	if (!error) {
		tdp = bhvtoi(tbdp);
		ilock(tdp);
		error = efs_dirlookup(tdp, tnm, NULL, DLF_ENTER|DLF_EXCL,
				      &ent, cr);
		if (!error)
			error = efs_direnter(tdp, sip, &ent, cr);
		iunlock(tdp);
	}
	if (error) {
		ilock(sip);
		efs_droplink(sip);
		iunlock(sip);
	}
	return error;
}

/*
 * Rename the file named by snm in source directory sdvp to tnm in tdvp.
 * We can't do two-phase commit without extra state in the inode, but we
 * can guarantee that tnm exists throughout the operation.  Unlock the
 * source inodes to avoid deadlock (this means the source entry can be
 * unlinked while we're working).  Keep the target directory locked from
 * lookup through enter (rewrite).
 *
 * Sketch:
 *
 *	1.  Bump the source inode's link count right away to keep it
 *	    from being unlinked while it is unlocked.
 *
 *	2.  Link the source inode into the target directory.  If the
 * 	    target exists, rewrite its entry in-place (efs_direnter uses
 *	    the offset discovered by efs_dirlookup; the target directory
 *	    must remain locked across lookup and enter).  If the source
 *	    is a directory and it moved to a different parent, rewrite
 *	    its ".." entry to point at the target directory.
 *
 *	3.  Unlink the source directory entry, if it's still around.
 *	    When renaming one hard link over another link to the same
 *	    inode, only steps 1 and 3 are executed.
 */
/* ARGSUSED */
static int
efs_rename(
	bhv_desc_t *sbdp,		/* old (source) parent vnode */
	char *snm,			/* old (source) entry name */
	struct vnode *tdvp,		/* new (target) parent vnode */
	char *tnm,			/* new (target) entry name */
	struct pathname *tpnp,		/* new (target) pathname or null */
	struct cred *cr)
{
	int error, dflag;		/* error and efs_dirisempty result */
	int directory;			/* simple flags, see below */
	struct inode *sdp, *tdp;	/* source and target directories */
	struct inode *sip, *tip;	/* source and target inodes */
	struct entry sent, tent;	/* source and target entries */
	efs_ino_t newparent;		/* inumber of new parent directory */
	int tip_dropped = 0;		/* tip link dropped? */
	int tdp_dropped = 0;		/* tdp link dropped? */
	int sip_dropped = 0;		/* sip link dropped? */
	int sdp_dropped = 0;		/* sdp link dropped? */
	bhv_desc_t *tdbdp;

	/*
	 * Lookup the source inode (again -- it's a shame we can't keep
	 * a handle on what rename has already looked up).  Increment its
	 * link count and update it on disk right now, to prevent someone
	 * else from removing it behind our back.
	 */
	sdp = bhvtoi(sbdp);
	ilock(sdp);
	error = efs_dirlookup(sdp, snm, NULL, DLF_IGET|DLF_MUSTHAVE|DLF_REMOVE,
			      &sent, cr);
	iunlock(sdp);
	if (error)
		return error;
	sip = sent.e_ip;
	if (sip == sdp) {
		irele(sip);
		return EINVAL;
	}
	directory = ((sip->i_mode & IFMT) == IFDIR);
	error = efs_bumplink(sip);
	iunlock(sip);

	/*
	 * 1.  Lock target directory, check for an efs_bumplink error, and
	 * then lookup the target name, in case an inode is already linked
	 * under it in tdp.  Tell efs_dirlookup to check for permission to
	 * unlink as well as permission to enter.
	 *
	 * Find the EFS behavior descriptor for the target directory
	 * vnode since it was not handed to us.
	 */
	tdbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(tdvp), &efs_vnodeops);
	if (tdbdp == NULL) {
		return EXDEV;
	}
	tdp = bhvtoi(tdbdp);
	tip = NULL;
	ilock(tdp);
	if (error)
		goto bad;
	if (error = efs_dirlookup(tdp, tnm, NULL, DLF_IGET|DLF_ENTER|DLF_REMOVE,
				  &tent, cr))
		goto bad;
	tip = tent.e_ip;
	if (tip == tdp) {
		error = EINVAL;
		goto bad;
	}
	ASSERT(!(sent.e_flags & PN_ISDOT) && !(tent.e_flags & PN_ISDOT));

	/*
	 * Source and target are identical.
	 */
	if (sip == tip) {
		ASSERT(sip != sdp);
		error = 0;	/* no-op */
		goto bad;
	}

	/*
	 * Directory rename requires special error checks.  We do not
	 * rely on the system call layer to check these cases, because
	 * there may be novel system call layers like the NFS server,
	 * which should not all have to do the same checks.
	 */
	newparent = 0;
	if (directory) {
		/*
		 * Renaming ".." is illegal.
		 */
		if ((sent.e_flags & PN_ISDOTDOT)
		    || (tent.e_flags & PN_ISDOTDOT)) {
			error = EINVAL;
			goto bad;
		}

		/*
		 * Check whether this rename would orphan the tree rooted at
		 * sip by moving it under itself.  Note that efs_notancestor
		 * unlocks tdp, so we must lookup tip again afterwards.  All
		 * calls to efs_notancestor go single-file through a monitor,
		 * to ensure that "mv /a/b /c/d/b2" won't lose the race with
		 * "mv /c/d /a/b/d2", resulting in "mv /a/b /a/b/d2/b2" and
		 * disconnection of the tree at /a/b.
		 */
		if (sdp != tdp) {
			newparent = tdp->i_number;
			if (tip) {
				iput(tip);
				tip = NULL;
			}
			if (error = efs_notancestor(sip, tdp, cr))
				goto bad;
			if (error = efs_dirlookup(tdp, tnm, NULL,
						  DLF_IGET|DLF_ENTER|DLF_REMOVE,
						  &tent, cr))
				goto bad;
			tip = tent.e_ip;
		}
	}

	if (tip == NULL) {
		/*
		 * If no target exists and the rename crosses directories,
		 * adjust the target directory link count to include the new
		 * ".." reference being added.
		 */
		if (newparent && (error = efs_bumplink(tdp)))
			goto bad;
		if (error = efs_direnter(tdp, sip, &tent, cr)) {
			if (newparent) {
				efs_droplink(tdp);
				tdp_dropped = 1;
			}
			goto bad;
		}
	} else {
		/*
		 * If target exists and it's a directory, check that both
		 * target and source are directories and that target can be
		 * destroyed, or that neither is a directory.
		 */
		if ((tip->i_mode & IFMT) == IFDIR) {
			if ((error = efs_dirisempty(tip, &dflag, cr))
			    || tip->i_nlink > 2) {
				if (error == ENOTEMPTY)
					error = EEXIST;	/* XXX */
				goto bad;
			}
			if (!directory) {
				error = EISDIR;
				goto bad;
			}
			if (itov(tip)->v_vfsmountedhere) {
				error = EBUSY;
				goto bad;
			}
		} else {
			if (directory) {
				error = ENOTDIR;
				goto bad;
			}
		}

		/*
		 * Purge all name cache references to the old target.
		 */
		dnlc_purge_vp(itov(tip));

		/*
		 * 2.  Link the source inode under the target name.  This
		 * is atomic, but if the source inode is a directory, and
		 * if the rename isn't local to a directory, the source's
		 * ".." entry will be inconsistent till the efs_dirinit().
		 * Now that the target entry has been rewritten, drop the
		 * old target's link count.
		 */
		if (error = efs_dirrewrite(tdp, sip, &tent, cr))
			goto bad;
		efs_droplink(tip);
		tip_dropped = 1;

		if (directory && (dflag & DIR_HASDOT)) {
			/*
			 * If the source is a directory and the target
			 * existed already, drop the target's link count
			 * again to deallocate it.
			 */
			efs_droplink(tip);
		}
	}

	iunlock(tdp);
	if (tip) {
		iunlock(tip);
		/* tell interposed file systems about removed links */
		if (tip_dropped)
			VOP_LINK_REMOVED(itov(tip), tdvp, (tip)->i_nlink==0);
		irele(tip);
	}

	/*
	 * 3.  Finally, remove the source.  Since sdp and sip have
	 * been unlocked, someone else may have already unlinked sip,
	 * so we ignore ENOENT.  If we're moving an inode over top of
	 * one of its hard links, remember to drop the link count we
	 * added in step 1.  Also remember to drop the source dir's
	 * link count if renaming a directory to a new parent.
	 */
	ilock(sdp);
	ilock(sip);

	error = efs_dirlookup(sdp, snm, NULL, DLF_REMOVE, &sent, cr);
	if (error == ENOENT)
		error = 0;
	else if (!error) {
		if (sent.e_inum != sip->i_number) {
			if (directory)
				panic("rename: lost directory");
		} else {
			if (newparent)
				error = efs_dirinit(sip, newparent, cr);
			if (!error && (newparent || 
				       (directory && tip != NULL))) {
				efs_droplink(sdp);
				sdp_dropped = 1;
			}
			if (!error) {
				error = efs_dirremove(sdp, &sent, cr);
				if (!error) {
					efs_droplink(sip);
					sip_dropped = 1;
				}
			}
		}
	}
	iunlock(sip);
	iunlock(sdp);

	/* tell interposed file systems about removed links */
	if (sdp_dropped)
		VOP_LINK_REMOVED(itov(sdp), itov(sip), (sdp)->i_nlink==0);
	if (sip_dropped)
		VOP_LINK_REMOVED(itov(sip), itov(sdp), (sip)->i_nlink==0);
	irele(sip);
	return error;

bad:
	/*
	 * Release old target inode if any and unlock target directory.
	 * Restore source's link count and iput it.
	 */
	if (tip) {
		if (tip == tdp)
			irele(tip);
		else
			iput(tip);
	}
	iunlock(tdp);
	if (tdp_dropped)
		VOP_LINK_REMOVED(itov(tdp), itov(sip), (tdp)->i_nlink==0);
	ilock(sip);
	efs_droplink(sip);
	iunlock(sip);
	VOP_LINK_REMOVED(itov(sip), itov(sdp), (sip)->i_nlink==0);
	irele(sip);
	return error;
}

/* ARGSUSED */
static int
efs_mkdir(
	bhv_desc_t *bdp,
	char *dirname,
	struct vattr *vap,
	struct vnode **vpp,
	struct cred *cr)
{
	struct inode *dp, *cdp;
	struct entry ent;
	int error;

	dp = bhvtoi(bdp);
	ilock(dp);
	/*
	 * Since dp is not locked between the lookup and this mkdir,
	 * it could have been removed.
	 */
	if (dp->i_nlink <= 0) {
		error = ENOENT;
		goto out2;
	}
	if (error = efs_iaccess(dp, IEXEC | IWRITE, cr)) {
		goto out2;
	}
	if (error = efs_bumplink(dp))
		goto out;
	if (error =
	    efs_dirlookup(dp, dirname, NULL, DLF_ENTER|DLF_EXCL, &ent, cr))
		goto out;
	error = efs_ialloc(dp, IFDIR | (vap->va_mode & ~IFMT), 2, 0, &cdp, cr);
	if (!error) {
		error = efs_dirinit(cdp, dp->i_number, cr);
		if (!error) {
			error = efs_direnter(dp, cdp, &ent, cr);
		}
		if (!error) {
			*vpp = itov(cdp);
			iunlock(cdp);
		} else {
			cdp->i_nlink = 0;
			cdp->i_flags |= ICHG;
			iput(cdp);
		}
	}
 out:
	if (error)
		efs_droplink(dp);
 out2:
	iunlock(dp);
	return error;
}

/* ARGSUSED */
static int
efs_rmdir(
	bhv_desc_t *bdp,
	char *nm,
	struct vnode *cdir,
	struct cred *cr)
{
	struct inode *dp, *cdp;
	struct entry ent;
	int error, dflag;
	int link_removed = 0;

	dp = bhvtoi(bdp);

	ilock(dp);
	error = efs_dirlookup(dp, nm, NULL, DLF_IGET|DLF_MUSTHAVE|DLF_REMOVE,
			      &ent, cr);
	if (error) {
		iunlock(dp);
		return error;
	}

	cdp = ent.e_ip;
	if (cdp == dp || itov(cdp) == cdir) {
		error = EINVAL;
	} else if ((cdp->i_mode & IFMT) != IFDIR) {
		error = ENOTDIR;
	} else if (itov(cdp)->v_vfsmountedhere) {
		error = EBUSY;
	} else if (cdp->i_nlink > 2) {
		error = EEXIST;		/* XXX ENOTEMPTY */
	} else if (error = efs_dirisempty(cdp, &dflag, cr)) {
		if (error == ENOTEMPTY)
			error = EEXIST;	/* XXX */
	} else {
		error = efs_dirremove(dp, &ent, cr);
		if (!error) {
			link_removed = 1;
			if (dflag & DIR_HASDOTDOT) {
				efs_droplink(dp);
				(void) efs_iupdat(dp);
			}
			if (dflag & DIR_HASDOT)
				cdp->i_nlink -= 2;
			else
				cdp->i_nlink--;
			cdp->i_flags |= ICHG;
			error = efs_iupdat(cdp);
		}
	}
	if (cdp != dp)
		iunlock(cdp);
	iunlock(dp);

	if (link_removed) {
		/*
		 * Let interposed file systems know about removed links.
		 */
		VOP_LINK_REMOVED(itov(dp), itov(cdp), (dp)->i_nlink==0);
		VOP_LINK_REMOVED(itov(cdp), itov(dp), (cdp)->i_nlink==0);
	}
	irele(cdp);

	return error;
}

/*
 * efs_readdir is in efs_dir.c
 */

/* ARGSUSED */
static int
efs_symlink(
	bhv_desc_t *bdp,		/* ptr to parent dir vnode */
	char *linkname,			/* name of symbolic link */
	struct vattr *vap,		/* attributes */
	char *target,			/* target path */
	struct cred *cr)		/* user credentials */
{
	struct inode *dp, *ip;
	struct entry ent;
	int error = 0, pathlen;
	struct uio uio;
	struct iovec iov;
	struct pathname cpn, ccpn;
	int newfile = 1;

	/* 
	 * Check component lengths of the target path name.
	 */
	pathlen = strlen(target);
	if (pathlen >= MAXPATHLEN)	/* total string too long */
		return ENAMETOOLONG;
	if (pathlen >= MAXNAMELEN) {	/* is any component too long? */
		pn_alloc(&cpn);
		pn_alloc(&ccpn);
		bcopy(target, cpn.pn_path, pathlen);
		cpn.pn_pathlen = pathlen;
		while (cpn.pn_pathlen > 0 && !error) {
			if (error = pn_getcomponent(&cpn, ccpn.pn_path, 0)) {
				pn_free(&cpn);
				pn_free(&ccpn);
				if (error == ENAMETOOLONG)
					return error;
			} else if (cpn.pn_pathlen) {	/* advance past slash */
				cpn.pn_path++;
				cpn.pn_pathlen--;
			}
		}
		pn_free(&cpn);
		pn_free(&ccpn);
	}

	dp = bhvtoi(bdp);
	ilock(dp);
	error = efs_dirlookup(dp, linkname, NULL, DLF_ENTER|DLF_EXCL, &ent, cr);
	if (!error) {
		error = efs_ialloc(dp, IFLNK | (vap->va_mode&~IFMT), 1, 0,
				   &ip, cr);
	}

	if (!error) {
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = 0;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_resid = iov.iov_len = pathlen;
		uio.uio_pmp = NULL;
		uio.uio_pio = 0;
		uio.uio_readiolog = 0;
		uio.uio_writeiolog = 0;
		uio.uio_pbuf = 0;
		iov.iov_base = target;
		error = efs_writei(ip, &uio, 0, cr, NULL);
		if (!error & newfile) {
			error = efs_direnter(dp, ip, &ent, cr);
		}
		if (error) {
			ip->i_nlink = 0;
			ip->i_flags |= ICHG;
		}
		iput(ip);
	}
	iunlock(dp);
	return error;
}

static int
efs_fid(
	bhv_desc_t *bdp,
	struct fid **fidpp)
{
	struct efid *efid;

	efid = kmem_alloc(sizeof *efid, KM_SLEEP);
	efid->efid_len = sizeof *efid - sizeof efid->efid_len;
	efid->efid_pad = 0;
	efid->efid_ino = bhvtoi(bdp)->i_number;
	efid->efid_gen = bhvtoi(bdp)->i_gen;
	*fidpp = (struct fid *)efid;
	return 0;
}

static int
efs_fid2(
	bhv_desc_t *bdp,
	struct fid *fidp)
{
	struct efid *efid = (struct efid *)fidp;

	ASSERT(sizeof(fid_t) >= sizeof(struct efid));
	efid->efid_len = sizeof *efid - sizeof efid->efid_len;
	efid->efid_pad = 0;
	efid->efid_ino = bhvtoi(bdp)->i_number;
	efid->efid_gen = bhvtoi(bdp)->i_gen;
	return 0;
}

/* ARGSUSED */
static void
efs_rwlock(bhv_desc_t *bdp, vrwlock_t write_lock)
{
	struct inode *ip;
	
	ip = bhvtoi(bdp);
	ilock(ip);
	ip->i_flags |= IRWLOCK;
}

/* ARGSUSED */
static void
efs_rwunlock(bhv_desc_t *bdp, vrwlock_t write_lock)
{
	struct inode *ip;

	ip = bhvtoi(bdp);
	ip->i_flags &= ~IRWLOCK;
	iunlock(ip);
}

/* ARGSUSED */
static int
efs_seek(bdp, ooff, noffp)
	bhv_desc_t *bdp;
	off_t ooff;
	off_t *noffp;
{
	return *noffp < 0 ? EINVAL : 0;
}

static int
efs_frlock(
	bhv_desc_t *bdp,
	int cmd,
	struct flock *lfp,
	int flag,
	off_t offset,
	vrwlock_t vrwlock,
	cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int dolock, error = 0;

	dolock = (vrwlock == VRWLOCK_NONE);
	if (dolock) {
		efs_rwlock(bdp, VRWLOCK_WRITE);
		vrwlock = VRWLOCK_WRITE;
	} 
	if (cmd != F_CLNLK)
		error = convoff(vp, lfp, lfp->l_whence, offset, SEEKLIMIT32,cr);
	if (!error)
		error = fs_frlock(bdp, cmd, lfp, flag, offset, vrwlock, cr);
	if (dolock)
		efs_rwunlock(bdp, VRWLOCK_WRITE);
	return error;
}

/*
 * efs_bmap is defined in efs_bmap.c, oddly enough.
 */

static void
efs_strategy(bhv_desc_t *bdp, struct buf *bp)
{
	struct inode	*ip;
	scoff_t		isize;

	ip = bhvtoi(bdp);
	if (bp->b_flags & B_READ) {
		ASSERT(mutex_mine(&ip->i_lock));
		ASSERT(ip->i_lockid == get_thread_id());
		isize = ip->i_size;
		if (isize <= BBTOB(bp->b_offset)) {
			IGETINFO.ig_readcancel++;
			iodone(bp);
			return;
		}
	}

	VOP_STRATEGY(ip->i_mount->m_devvp, bp);
}

/* ARGSUSED */
static int
efs_map(
	bhv_desc_t *bdp,
	off_t off,
	size_t len,
	mprot_t prot,
	u_int flags,
	struct cred *cr,
	vnode_t **nvp)
{
	if ((off + (off_t)len) > (off_t)EFS_MAX_FILE_OFFSET)
		return EINVAL;
	return 0;
}

/* ARGSUSED */
static int
efs_reclaim(
	bhv_desc_t *bdp,
	int flag)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct inode *ip;

	ASSERT(!VN_MAPPED(vp));
	ip = bhvtoi(bdp);

	if (ip->i_flags & IINCORE && ip->i_numextents > 0) {
		struct extent *ex = &ip->i_extents[ip->i_numextents - 1];
		VOP_FLUSHINVAL_PAGES(vp, 0, BBTOB(ex->ex_offset+ex->ex_length) - 1,
			FI_NONE);
	}
	dnlc_purge_vp(vp);
	ASSERT((ip->i_flags & (IMOD|IACC|IUPD|ICHG)) == 0);
	ireclaim(ip);
	return 0;
}


/* ARGSUSED */
int
efs_setfl(
	bhv_desc_t *bdp,
	int oflags,
	int nflags,
	cred_t *cr)
{
	return 0;
}

#ifdef DATAPIPE
/* ARGSUSED */
int
efs_fspe_dioinfo(
	struct vnode   *vp,
	struct dioattr *da)
{
	/* This is a copy from fcntl - F_DIOINFO cmd */
#ifdef R10000_SPECULATION_WAR	
	da->d_mem = _PAGESZ;
#else
	da->d_mem = FDIRIOALIGN;
#endif
	da->d_miniosz = BBSIZE;
	da->d_maxiosz = ctob(v.v_maxdmasz - 1);
	return 0;
}
#endif

/* ARGSUSED */
int
efs_fcntl(
	bhv_desc_t *bdp,
	int cmd,
	void *arg,
	int flags,
	off_t offset,
	cred_t *cr,
	rval_t *rvp)
{
	int 	error = 0;
	struct flock		bf;
	struct irix5_flock	i5_bf;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	char abi = get_current_abi();

	switch (cmd) {
#ifdef DATAPIPE
	case F_GETOPS:
		fspe_get_ops(arg);
		break;
#endif
	case F_DIOINFO: {
		struct dioattr da;

		/* only works on files opened for direct I/O */
		if (!(flags & FDIRECT)) {
			error = EINVAL;
			break;
		}

#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000())
			da.d_mem = _PAGESZ;
		else
			da.d_mem = FDIRIOALIGN;
#elif R10000_SPECULATION_WAR	/* makes tlb invalidate during dma more
	effective, by decreasing the likelihood of a valid reference in the
	same page as dma user address space; leaving the tlb invalid avoids
	the speculative reference. We return the more stringent
	"requirements" on the fcntl(), but do *NOT* enforced them
	in the read/write code, to be sure we don't break apps... */
		da.d_mem = _PAGESZ;
#else
		da.d_mem = FDIRIOALIGN;
#endif
		da.d_miniosz = BBSIZE;
		da.d_maxiosz = ctob(v.v_maxdmasz - 1);

		if (copyout(&da, arg, sizeof da))
			error = EFAULT;

		break;
	}
	case F_ALLOCSP:
	case F_FREESP:
	case F_ALLOCSP64:
	case F_FREESP64:
		if ((flags & FWRITE) == 0) {
			error = EBADF;
		} else if (vp->v_type != VREG) {
			error = EINVAL;
		} else if (vp->v_flag & VISSWAP) {
			error = EACCES;
#if _MIPS_SIM == _ABI64
		} else if (ABI_IS_IRIX5_64(abi)) {
			if (copyin((caddr_t)arg, &bf, sizeof bf)) {
				error = EFAULT;
				break;
			}
#endif
		} else if (cmd == F_ALLOCSP64 || cmd == F_FREESP64 ||
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
		if ((error = convoff(vp, &bf, 0, offset, SEEKLIMIT32, cr)) == 0) {
			struct vattr vattr;

			vattr.va_size = bf.l_start;
			vattr.va_mask = AT_SIZE;
			error = efs_setattr(bdp, &vattr, 0, cr);
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

/* 
 * EFS doesn't fully support attributes.  We allow getting/setting one
 * particular attribute, though: _DEVNAME_ATTR is an attribute
 * for special device files stored on EFS file systems.  The value of this
 * attribute is a hwgraph device path, and it's stored on disk the same
 * way a symbolic link is stored.  The on-disk EFS type is changed to CHRLNK 
 * or BLKLNK respectively.  This is seen by upper layers as VCHR or VBLK.
 */
/*ARGSUSED*/
int								/* error */
efs_attr_get(bhv_desc_t *bdp, char *name, char *value, int *valuelenp, 
	     int flags, struct cred *cred)
{
	struct inode *ip;
	u_short type;
	struct uio uio;
	struct iovec iov;
	int error;

	/* Is it a MAC label */
	if (strcmp(name, SGI_MAC_FILE) == 0)
		return _MAC_EFS_ATTR_GET(bdp, name, value, valuelenp, flags, cred);
	/* Make sure we're getting the only permissible attribute */
	if (strcmp(name, _DEVNAME_ATTR))
		return(ENOSYS);

	ip = bhvtoi(bdp);

	/* 
	 * Make sure we're only trying to get this attribute on
	 * an appropriate hwgraph special device file.
	 */
	type = ip->i_mode & IFMT;
	if ((type != IFCHRLNK) && (type != IFBLKLNK))
		return(ENOSYS);

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = iov.iov_len = *valuelenp;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	iov.iov_base = value;

	ilock(ip);
	error = efs_readi(ip, &uio, 0, type, cred, NULL);
	iunlock(ip);

	return(error);
}


/*ARGSUSED */
int
efs_attr_set(bhv_desc_t *bdp, char *name, char *value, int valuelen, int flags, 
		struct cred *cred)
{
	struct inode *ip;
	u_short type;
	int error;

	/* Is it a MAC Label */
	if (strcmp(name, SGI_MAC_FILE) == 0)
		return (_MAC_EFS_ATTR_SET(bdp, name, value, valuelen,
				flags, cred));

	/* Make sure we're setting the only permissible attribute */
	if (strcmp(name, _DEVNAME_ATTR))
		return(ENOSYS);

	/* 
	 * Make sure we've got permission to make make special files,
	 * since by changing this attribute we're essentially creating
	 * a new special file.
	 */
	if (!cap_able_cred(cred, CAP_MKNOD))
		return(EPERM);

	ip = bhvtoi(bdp);
	ilock(ip);

	/* 
	 * Only allow attribute to be written on hwgraph special device files. 
	 */
	type = ip->i_mode & IFMT;
	if ((type == IFCHR) && 
	     IS_HWGRAPH_STRING_DEV(ip->i_rdev)) {
		error = efs_ichange_type(ip, IFCHRLNK);
	} else if ((type == IFBLK) && 
		    IS_HWGRAPH_STRING_DEV(ip->i_rdev)) {
		error = efs_ichange_type(ip, IFBLKLNK);
	} else if ((type == IFCHRLNK) || (type == IFBLKLNK))
		error = 0;
	else
		error = ENOSYS;

	if (!error) {
		struct uio uio;
		struct iovec iov;

		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = 0;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_resid = iov.iov_len = valuelen;
		uio.uio_pmp = NULL;
		uio.uio_pio = 0;
		uio.uio_readiolog = 0;
		uio.uio_writeiolog = 0;
		uio.uio_pbuf = 0;
		iov.iov_base = value;

		error = efs_writei(ip, &uio, 0, cred, NULL);
		if (error) {
			/* On failure, restore old file type */
			efs_ichange_type(ip, type);
		}
	}
	iunlock(ip);

	return(error);
}

vnodeops_t efs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	(vop_open_t)fs_noerr,
	efs_close,
	efs_read,
	efs_write,
	efs_ioctl,
	efs_setfl,
	efs_getattr,
	efs_setattr,
	efs_access,
	efs_lookup,
	efs_create,
	efs_remove,
	efs_link,
	efs_rename,
	efs_mkdir,
	efs_rmdir,
	efs_readdir,
	efs_symlink,
	efs_readlink,
	efs_fsync,
	efs_inactive,
	efs_fid,
	efs_fid2,
	efs_rwlock,
	efs_rwunlock,
	efs_seek,
	fs_cmp,
	efs_frlock,
	(vop_realvp_t)fs_nosys,
	efs_bmap,
	efs_strategy,
	efs_map,
	(vop_addmap_t)fs_noerr,
	(vop_delmap_t)fs_noerr,
	fs_poll,
	(vop_dump_t)fs_nosys,
	fs_pathconf,
	(vop_allocstore_t)fs_nosys,
	efs_fcntl,
	efs_reclaim,
	efs_attr_get,
	efs_attr_set,
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
