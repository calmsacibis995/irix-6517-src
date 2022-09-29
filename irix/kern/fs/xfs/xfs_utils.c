#ident "$Revision: 1.16 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/dnlc.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/dmi.h>
#include <sys/dmi_kern.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/major.h>

#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_bmap.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_error.h"
#include "xfs_quota.h"
#include "xfs_rw.h"
#include "xfs_itable.h"
#include "xfs_utils.h"

/*
 * Test the sticky attribute of a directory.  We can unlink from a sticky
 * directory that's writable by us if: we are superuser, we own the file,
 * we own the directory, or the file is writable.
 */
int
xfs_stickytest(
	xfs_inode_t	*dp,
	xfs_inode_t	*ip,
	cred_t		*cr)
{
	extern int	xpg4_sticky_dir;

        if ((dp->i_d.di_mode & ISVTX) &&
	    cr->cr_uid != ip->i_d.di_uid &&
	    cr->cr_uid != dp->i_d.di_uid &&
	    !cap_able_cred(cr, CAP_DAC_WRITE)) {
		if (xpg4_sticky_dir) {
			return XFS_ERROR(EACCES);
		} else {
			return xfs_iaccess(ip, IWRITE, cr);
		}
        }
        return 0;
}

/*
 * Wrapper around xfs_dir_lookup. This routine will first look in
 * the dnlc.
 *
 * If DLF_IGET is set, then this routine will also return the inode.
 * Note that the inode will not be locked. Note, however, that the
 * vnode will have an additional reference in this case.
 */
int
xfs_dir_lookup_int(
	xfs_trans_t  		*tp,
	bhv_desc_t     		*dir_bdp,
	int		 	flags,
	char         		*name,
	struct pathname		*pnp,
	xfs_ino_t    		*inum,
	xfs_inode_t  		**ipp,
	struct ncfastdata	*fd,
	uint			*dir_unlocked)
{
	vnode_t		*vp;
	vnode_t		*dir_vp;	   
	xfs_inode_t	*dp;
	xfs_ino_t	curr_inum;
	int		name_len;
	int		error;
	int		do_iget;
	uint		dir_gen;
	uint		lock_mode;
	bhv_desc_t	*bdp;

	dir_vp = BHV_TO_VNODE(dir_bdp);
	vn_trace_entry(dir_vp, "xfs_dir_lookup_int",
		       (inst_t *)__return_address);
	do_iget = flags & DLF_IGET;
	ASSERT((flags & (DLF_IGET | DLF_NODNLC)) != (DLF_IGET | DLF_NODNLC));
	error = 0;
	ASSERT(!(flags & DLF_IGET) || (dir_unlocked != NULL));
	if (dir_unlocked != NULL) {
		*dir_unlocked = 0;
	}

	/*
	 * Handle degenerate pathname component.
	 */
	if (*name == '\0') {
		VN_HOLD(dir_vp);
		*inum = XFS_BHVTOI(dir_bdp)->i_ino;
		if (do_iget) {
			*ipp = XFS_BHVTOI(dir_bdp);
		}
		return 0;
        }
	if (flags & DLF_LOCK_SHARED) {
		lock_mode = XFS_ILOCK_SHARED;
	} else {
		lock_mode = XFS_ILOCK_EXCL;
	}
	dp = XFS_BHVTOI(dir_bdp);

        /*
         * Try the directory name lookup cache.  We can't wait on
	 * inactive/reclaim in the inode we're looking for because
	 * we're holding the directory lock.
         */
        if (!(flags & DLF_NODNLC)) {
retry_dnlc:
		dir_gen = dp->i_gen;
		bdp = dnlc_lookup_fast(dir_vp, name, pnp, fd, NOCRED,
			VN_GET_NOWAIT);
		if (!bdp && fd->vnowait) {
			if (dir_unlocked != NULL)
				*dir_unlocked = 1;
			xfs_iunlock(dp, lock_mode);
			bdp = dnlc_lookup_fast(dir_vp, name, pnp, fd,
				NOCRED, 0);
			xfs_ilock(dp, lock_mode);
		}
		if (bdp && dir_gen == dp->i_gen) {
			ITRACE(XFS_BHVTOI(bdp));
			*inum = XFS_BHVTOI(bdp)->i_ino;
			if (do_iget) {
				*ipp = XFS_BHVTOI(bdp);
				ASSERT((*ipp)->i_d.di_mode != 0);
			} else {
				/*
				 * This is only safe for directory
				 * inodes that have not been unlinked.
				 * Otherwise we could deadlock because
				 * the inactive call could start a
				 * transaction while we hold the directory
				 * lock here.
				 */
#ifdef DEBUG
				vp = BHV_TO_VNODE(bdp);
				ASSERT(vp->v_type == VDIR);
				ASSERT(XFS_BHVTOI(bdp)->i_d.di_nlink > 0);
#endif
				VN_RELE(BHV_TO_VNODE(bdp));
			}
			return 0;
		}
		/*
		 * If we get here with bdp set then we need to VN_RELE it
		 * because the directory generation number changed. It's not
		 * safe to do this until the directory is unlocked. If we
		 * are not going to get the actual inode then do this here
		 * and check the dnlc for the name again. If we are going to
		 * read the inode then let that code do the work for us.
		 */
		if (!do_iget && bdp) {
			if (dir_unlocked != NULL)
				*dir_unlocked = 1;
			xfs_iunlock(dp, lock_mode);
			VN_RELE(BHV_TO_VNODE(bdp));
			xfs_ilock(dp, lock_mode);
			goto retry_dnlc;
		}
        } else
		bdp = NULL;

	/*
	 * If all else fails, call the directory code.
	 */
	if (pnp != NULL)
		name_len = pnp->pn_complen;
	else
		name_len = strlen(name);

	error = XFS_DIR_LOOKUP(dp->i_mount, tp, dp, name, name_len, inum);
	if (!error && do_iget) {
		*dir_unlocked = 1;
		for (;;) {
			/*
			 * Unlock the directory and save the directory's
			 * generation count.  We do this because we can't
			 * hold the directory lock while doing the vn_get()
			 * in xfs_iget().  Doing so could cause us to hold
			 * a lock while waiting for the inode to finish
			 * being inactive while it's waiting for a log
			 * reservation in the inactive routine.
			 */
			dir_gen = dp->i_gen;
			xfs_iunlock(dp, lock_mode);

			if (bdp) {
				VN_RELE(BHV_TO_VNODE(bdp));
				bdp = NULL;
			}
				
			error = xfs_iget(dp->i_mount, NULL, *inum,
					 0, ipp, 0);

			xfs_ilock(dp, lock_mode);

			if (error) {
				*ipp = NULL;
				return error;
			}

			/*
			 * If something changed in the directory try again.
			 * If we get an error just return the error.  That
			 * includes the case where the file is removed
			 * (ENOENT).  If the inode number has changed, then
			 * drop the inode we have and get the latest one.
			 * If the inode number is the same, then we just
			 * go with it.  This includes the case where the
			 * inode is freed and recreated under the same
			 * name while we're doing the iget.  It is OK
			 * to use the inode we already have in that case,
			 * as there is only one copy of an inode in the
			 * cache at a time (regardless of generation count)
			 * and it just looks like the first lookup never
			 * occurred.
			 */
			if (dp->i_gen != dir_gen) {
				/*
				 * If the directory was removed while
				 * we left it unlocked then just get
				 * out of here.
				 */
				if (dp->i_d.di_nlink == 0) {
					vp = XFS_ITOV(*ipp);
					xfs_iunlock(dp, lock_mode);
					VN_RELE(vp);
					xfs_ilock(dp, lock_mode);
					return XFS_ERROR(ENOENT);
				}

				error = XFS_DIR_LOOKUP(dp->i_mount, tp, dp,
						       name, name_len,
						       &curr_inum);

				if (error || (curr_inum != *inum)) {
					vp = XFS_ITOV(*ipp);
					xfs_iunlock(dp, lock_mode);
					VN_RELE(vp);
					xfs_ilock(dp, lock_mode);
					if (error) {
						return error;
					} else {
						/*
						 * Drop the directory
						 * lock and do the iget
						 * again.  Save the ino
						 * that we found in *inum
						 * for when we do the
						 * check again.
						 */
						*inum = curr_inum;
						continue;
					}
				}
			}
			/*
			 * Everything is consistent, so just go with
			 * what we've got.
			 */
			break;
		}

		if ((*ipp)->i_d.di_mode == 0) {
			vp = XFS_ITOV(*ipp);
			/*
			 * The inode has been freed.  Something is
			 * wrong so just get out of here.
			 */
			*ipp = NULL;
			xfs_iunlock(dp, lock_mode);
			VN_RELE(vp);
			xfs_ilock(dp, lock_mode);
			error = XFS_ERROR(ENOENT);
		} else {
			bdp = XFS_ITOBHV(*ipp);
			ASSERT(!(flags & DLF_NODNLC));
			dnlc_enter_fast(dir_vp, fd, bdp, NOCRED);
			bdp = NULL;
		}
	}
	if (bdp) {
		/* The only time we should get here is if the dir_lookup
		 * failed.
		 */
		ASSERT(error);
		xfs_iunlock(dp, lock_mode);
		VN_RELE(BHV_TO_VNODE(bdp));
		xfs_ilock(dp, lock_mode);
		if (dir_unlocked != NULL) *dir_unlocked = 1;
	}
	return error;
}



/*
 * Allocates a new inode from disk and return a pointer to the
 * incore copy. This routine will internally commit the current
 * transaction and allocate a new one if the Space Manager needed
 * to do an allocation to replenish the inode free-list.
 *
 * This routine is designed to be called from xfs_create and
 * xfs_create_dir.
 *
 */
int
xfs_dir_ialloc(
	xfs_trans_t	**tpp,		/* input: current transaction;
					   output: may be a new transaction. */
	xfs_inode_t	*dp,		/* directory within whose allocate
					   the inode. */
	mode_t		mode,
	nlink_t		nlink,
	dev_t		rdev,
	cred_t		*credp,
	prid_t		prid,		/* project id */
	int		okalloc,	/* ok to allocate new space */
	xfs_inode_t	**ipp,		/* pointer to inode; it will be
					   locked. */
	int		*committed)

{
	xfs_trans_t	*tp;
	xfs_trans_t	*ntp;
	xfs_inode_t	*ip;
	buf_t		*ialloc_context = NULL;
	boolean_t	call_again = B_FALSE;
	int		code;
	uint		log_res;
	uint		log_count;
	void		*dqinfo;
	uint		tflags;

	tp = *tpp;
	ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);

	/*
	 * xfs_ialloc will return a pointer to an incore inode if
	 * the Space Manager has an available inode on the free
	 * list. Otherwise, it will do an allocation and replenish
	 * the freelist.  Since we can only do one allocation per
	 * transaction without deadlocks, we will need to commit the
	 * current transaction and start a new one.  We will then
	 * need to call xfs_ialloc again to get the inode.
	 *
	 * If xfs_ialloc did an allocation to replenish the freelist,
	 * it returns the bp containing the head of the freelist as 
	 * ialloc_context. We will hold a lock on it across the
	 * transaction commit so that no other process can steal 
	 * the inode(s) that we've just allocated.
	 */
	code = xfs_ialloc(tp, dp, mode, nlink, rdev, credp, prid, okalloc,
			  &ialloc_context, &call_again, &ip);

	/*
	 * Return an error if we were unable to allocate a new inode.
	 * This should only happen if we run out of space on disk or
	 * encounter a disk error.
	 */
	if (code) {
		*ipp = NULL;
		return code;
	}
	if (!call_again && (ip == NULL)) {
		*ipp = NULL;
		return XFS_ERROR(ENOSPC);
	}

	/*
	 * If call_again is set, then we were unable to get an
	 * inode in one operation.  We need to commit the current
	 * transaction and call xfs_ialloc() again.  It is guaranteed
	 * to succeed the second time.
	 */
	if (call_again) {

		/*
		 * Normally, xfs_trans_commit releases all the locks.
	         * We call bhold to hang on to the ialloc_context across
		 * the commit.  Holding this buffer prevents any other
		 * processes from doing any allocations in this 
		 * allocation group.
		 */
		xfs_trans_bhold(tp, ialloc_context);
		/*
		 * Save the log reservation so we can use
		 * them in the next transaction.
		 */
		log_res = xfs_trans_get_log_res(tp);
		log_count = xfs_trans_get_log_count(tp);
		
		/*
		 * We want the quota changes to be associated with the next
		 * transaction, NOT this one. So, detach the dqinfo from this
		 * and attach it to the next transaction.
		 */
		dqinfo = NULL;
		tflags = 0;
		if (tp->t_dqinfo) {
			dqinfo = (void *)tp->t_dqinfo;
			tp->t_dqinfo = NULL;
			tflags = tp->t_flags & XFS_TRANS_DQ_DIRTY;
			tp->t_flags &= ~(XFS_TRANS_DQ_DIRTY);
		}

		ntp = xfs_trans_dup(tp);
		code = xfs_trans_commit(tp, 0, NULL);
		tp = ntp;
		if (committed != NULL) {
			*committed = 1;
		}
		/*
		 * If we get an error during the commit processing,
		 * release the buffer that is still held and return
		 * to the caller.
		 */
		if (code) {
			brelse(ialloc_context);
			if (dqinfo) {
				tp->t_dqinfo = dqinfo;
				xfs_trans_free_dqinfo(tp);
			}
			*tpp = ntp;
			*ipp = NULL;
			return code;
		}
		code = xfs_trans_reserve(tp, 0, log_res, 0,
					 XFS_TRANS_PERM_LOG_RES, log_count);
		/*
		 * Re-attach the quota info that we detached from prev trx.
		 */
		if (dqinfo) {
			tp->t_dqinfo = dqinfo;
			tp->t_flags |= tflags;
		}
		
		if (code) {
			brelse(ialloc_context);
			*tpp = ntp;
			*ipp = NULL;
			return code;
		}
		xfs_trans_bjoin (tp, ialloc_context);

		/*
		 * Call ialloc again. Since we've locked out all
		 * other allocations in this allocation group,
		 * this call should always succeed.
		 */
		code = xfs_ialloc(tp, dp, mode, nlink, rdev, credp, prid,
				  okalloc, &ialloc_context, &call_again, &ip);

		/*
		 * If we get an error at this point, return to the caller
		 * so that the current transaction can be aborted.
		 */
		if (code) {
			*tpp = tp;
			*ipp = NULL;
			return code;
		}
		ASSERT ((!call_again) && (ip != NULL));

	} else {
		if (committed != NULL) {
			*committed = 0;
		}
	}

	*ipp = ip;
	*tpp = tp;

	return 0;
}

/*
 * Decrement the link count on an inode & log the change.
 * If this causes the link count to go to zero, initiate the
 * logging activity required to truncate a file.
 */
int				/* error */
xfs_droplink(
	xfs_trans_t *tp,
	xfs_inode_t *ip)
{
	int	error;

	xfs_ichgtime(ip, XFS_ICHGTIME_CHG);

        ASSERT (ip->i_d.di_nlink > 0);
        ip->i_d.di_nlink--;
        xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	error = 0;
        if (ip->i_d.di_nlink == 0) {
                /*
                 * We're dropping the last link to this file.
		 * Move the on-disk inode to the AGI unlinked list.
		 * From xfs_inactive() we will pull the inode from
		 * the list and free it.
                 */
		error = xfs_iunlink(tp, ip);
        }
	return error;
}

/*
 * This gets called when the inode's version needs to be changed from 1 to 2.
 * Currently this happens when the nlink field overflows the old 16-bit value
 * or when chproj is called to change the project for the first time.
 * As a side effect the superblock version will also get rev'd 
 * to contain the NLINK bit.
 */
void
xfs_bump_ino_vers2(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip)
{
	xfs_mount_t	*mp;
	int		s;

	ASSERT(ismrlocked (&ip->i_lock, MR_UPDATE));
	ASSERT(ip->i_d.di_version == XFS_DINODE_VERSION_1);

	ip->i_d.di_version = XFS_DINODE_VERSION_2;
	ip->i_d.di_onlink = 0;
	bzero(&(ip->i_d.di_pad[0]), sizeof(ip->i_d.di_pad));
	mp = tp->t_mountp;
	if (!XFS_SB_VERSION_HASNLINK(&mp->m_sb)) {
		s = XFS_SB_LOCK(mp);
		if (!XFS_SB_VERSION_HASNLINK(&mp->m_sb)) {
			XFS_SB_VERSION_ADDNLINK(&mp->m_sb);
			XFS_SB_UNLOCK(mp, s);
			xfs_mod_sb(tp, XFS_SB_VERSIONNUM);
		} else {
			XFS_SB_UNLOCK(mp, s);
		}
	}
	/* Caller must log the inode */
}

/*
 * Increment the link count on an inode & log the change.
 */
int
xfs_bumplink(
	xfs_trans_t *tp,
	xfs_inode_t *ip)
{
	if (ip->i_d.di_nlink >= XFS_MAXLINK)
		return XFS_ERROR(EMLINK);
	xfs_ichgtime(ip, XFS_ICHGTIME_CHG);

	ASSERT(ip->i_d.di_nlink > 0);
        ip->i_d.di_nlink++;
	if ((ip->i_d.di_version == XFS_DINODE_VERSION_1) &&
	    (ip->i_d.di_nlink > XFS_MAXLINK_1)) {
		/*
		 * The inode has increased its number of links beyond
		 * what can fit in an old format inode.  It now needs
		 * to be converted to a version 2 inode with a 32 bit
		 * link count.  If this is the first inode in the file
		 * system to do this, then we need to bump the superblock
		 * version number as well.
		 */
		xfs_bump_ino_vers2(tp, ip);
	}

        xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	return 0;
}

/*
 * Try to truncate the given file to 0 length.  Currently called
 * only out of xfs_remove when it has to truncate a file to free
 * up space for the remove to proceed.
 */
int
xfs_truncate_file(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip)
{
	xfs_trans_t	*tp;
	int		error;

#ifdef QUOTADEBUG
	/* 
	 * This is called to truncate the quotainodes too.
	 */
	if (XFS_IS_UQUOTA_ON(mp)) {
		if (ip->i_ino != mp->m_sb.sb_uquotino)
			ASSERT(ip->i_udquot);
	}
	if (XFS_IS_PQUOTA_ON(mp)) {
		if (ip->i_ino != mp->m_sb.sb_pquotino)
			ASSERT(ip->i_pdquot);
	}
#endif
	/*
	 * Make the call to xfs_itruncate_start before starting the
	 * transaction, because we cannot make the call while we're
	 * in a transaction.
	 */
	xfs_ilock(ip, XFS_IOLOCK_EXCL);
	xfs_itruncate_start(ip, XFS_ITRUNC_DEFINITE, (xfs_fsize_t)0);

	tp = xfs_trans_alloc(mp, XFS_TRANS_TRUNCATE_FILE);
	if (error = xfs_trans_reserve(tp, 0, XFS_ITRUNCATE_LOG_RES(mp), 0,
				      XFS_TRANS_PERM_LOG_RES,
				      XFS_ITRUNCATE_LOG_COUNT)) {
		xfs_trans_cancel(tp, 0);
		xfs_iunlock(ip, XFS_IOLOCK_EXCL);
		return error;
	}

	/*
	 * Follow the normal truncate locking protocol.  Since we
	 * hold the inode in the transaction, we know that it's number
	 * of references will stay constant.
	 */
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	/*
	 * Signal a sync xaction.  The only case where that isn't
	 * the case is if we're truncating an already unlinked file
	 * on a wsync fs.  In that case, we know the blocks can't
	 * reappear in the file because the links to file are
	 * permanently toast.  Currently, we're always going to
	 * want a sync transaction because this code is being
	 * called from places where nlink is guaranteed to be 1
	 * but I'm leaving the tests in to protect against future
	 * changes -- rcc.
	 */
	error = xfs_itruncate_finish(&tp, ip, (xfs_fsize_t)0,
				     XFS_DATA_FORK,
				     ((ip->i_d.di_nlink != 0 ||
				       !(mp->m_flags & XFS_MOUNT_WSYNC))
				      ? 1 : 0));
	if (error) {
		xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES |
				 XFS_TRANS_ABORT);
	} else {
		xfs_ichgtime(ip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
		error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES,
					 NULL);
	}
	xfs_iunlock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);

	return error;
}

#if (defined(DEBUG) || defined(INDUCE_IO_ERROR))
/*
 * for error injection
 */
int
xfs_get_fsinfo(int fd, char **fsname, int64_t *fsid)
{
	xfs_mount_t *mp;
	int error;

	if (error = xfs_fd_to_mp(fd, 0, &mp, 1))
		return XFS_ERROR(error);

	*fsname = mp->m_fsname;
	bcopy(mp->m_fixedfsid, fsid, sizeof(fsid_t));

	return 0;
}
#endif /* DEBUG || INDUCE_IO_ERROR */

int
xfs_mk_sharedro(int fd)
{
	int error;
	xfs_mount_t *mp;

	if (error = xfs_fd_to_mp(fd, 1, &mp, 1))
		return XFS_ERROR(error);

	if (emajor(mp->m_dev) != XLV_MAJOR) {
		return XFS_ERROR(EINVAL);
	}

	mp->m_mk_sharedro = 1;

	cmn_err(CE_NOTE,
		"Filesystem \"%s\" will be marked shared read-only on unmount",
		mp->m_fsname);

	return 0;
}

int
xfs_clear_sharedro(int fd)
{
	int error;
	xfs_mount_t *mp;

	if (error = xfs_fd_to_mp(fd, 1, &mp, 1))
		return XFS_ERROR(error);

	if (emajor(mp->m_dev) != XLV_MAJOR) {
		return XFS_ERROR(EINVAL);
	}

	mp->m_mk_sharedro = 0;

	cmn_err(CE_NOTE,
	"Filesystem \"%s\" will not be marked shared read-only on unmount",
		mp->m_fsname);

	return 0;
}

#ifdef DEBUG
int
xfs_isshutdown(bhv_desc_t *bhv)
{
	xfs_inode_t	*ip = XFS_BHVTOI(bhv);
	xfs_mount_t	*mp = ip->i_mount;

	return XFS_FORCED_SHUTDOWN(mp) != 0;
}
#endif	/* DEBUG */
