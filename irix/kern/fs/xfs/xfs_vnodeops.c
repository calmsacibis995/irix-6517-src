#ident "$Revision: 1.408 $"


#ifdef SIM
#define _KERNEL 1
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timers.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/fs/spec_lsnode.h>
#include <sys/systm.h>
#include <sys/dnlc.h>
#include <sys/sysmacros.h>
#include <sys/prctl.h>
#include <sys/cred.h>
#include <sys/uuid.h>
#include <sys/unistd.h>
#include <sys/grio.h>
#include <sys/pfdat.h>
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <sys/dmi.h>
#include <sys/dmi_kern.h>
#include <sys/pda.h>
#include <sys/debug.h>
#include <sys/uthread.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#ifdef SIM
#undef _KERNEL
#endif
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/flock.h>
#include <sys/fs_subr.h>
#include <sys/fcntl.h>
#include <sys/ktrace.h>
#ifdef SIM
#include <bstring.h>
#else
#include <sys/conf.h>
#endif
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/sema.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <ksys/vfile.h>
#include <sys/mode.h>
#include <sys/var.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/flock.h>
#include <sys/kfcntl.h>
#include <string.h>
#include <sys/dirent.h>
#include <sys/attributes.h>
#include <sys/major.h>
#include <sys/ddi.h>
#include <ksys/fdt.h>
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_itable.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"
#include "xfs_rw.h"
#include "xfs_error.h"
#include "xfs_bit.h"
#include "xfs_quota.h"
#include "xfs_utils.h"
#include "xfs_trans_space.h"
#include "xfs_dir_leaf.h"
#include "xfs_dmapi.h"
#ifdef SIM
#include "sim.h"
#endif

/*
 * Here so that we do not need to include vproc.h -> vpgrp.h ->
 * space.h -> vpag.h.
 */
extern prid_t dfltprid;

#ifdef DATAPIPE
/* data pipe functions */
extern	int fspe_get_ops(void *);
int         xfs_fspe_dioinfo(struct vnode *, struct dioattr *);
#endif

#if _MIPS_SIM == _ABI64
int irix5_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5(void *, int, xlate_info_t *);
int irix5_n32_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5_n32(void *, int, xlate_info_t *);
#endif

extern void xfs_lock_inodes (xfs_inode_t **, int, int, uint);
/*
 * The maximum pathlen is 1024 bytes. Since the minimum file system
 * blocksize is 512 bytes, we can get a max of 2 extents back from
 * bmapi.
 */
#define SYMLINK_MAPS 2


#ifndef SIM
STATIC int
xfs_open(
	bhv_desc_t	*bdp,
	vnode_t		**vpp,
	mode_t		flag,
	cred_t		*credp);

STATIC int
xfs_close(
	bhv_desc_t	*bdp,
	int		flag,
	lastclose_t	lastclose,
	cred_t		*credp);

STATIC int
xfs_getattr(
	bhv_desc_t	*bdp,
	vattr_t		*vap,
	int		flags,
	cred_t		*credp);

STATIC int
xfs_setattr(
	bhv_desc_t	*bdp,
	vattr_t		*vap,
	int		flags,
	cred_t		*credp);

STATIC int
xfs_access(
	bhv_desc_t	*bdp,
	int		mode,
	cred_t		*credp);

STATIC int
xfs_fsync(
	bhv_desc_t	*bdp,
	int		flag,
	cred_t		*credp,
	off_t		start,
	off_t		stop);

STATIC int
xfs_lookup(
	bhv_desc_t	*dir_bdp,
	char		*name,
	vnode_t		**vpp,
	pathname_t	*pnp,
	int		flags,
	vnode_t		*rdir, 
	cred_t		*credp);

STATIC int
xfs_create(
	bhv_desc_t	*dir_bdp,
	char		*name,
	vattr_t		*vap,
	int		flags,
	int		mode,
	vnode_t		**vpp,
	cred_t		*credp);

STATIC int
xfs_remove(
	bhv_desc_t	*dir_bdp,
	char		*name,
	cred_t		*credp);

STATIC int
xfs_link(
	bhv_desc_t	*target_dir_bdp,
	vnode_t		*src_vp,
	char		*target_name,
	cred_t		*credp);

STATIC int
xfs_readlink(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	cred_t		*credp);

STATIC int
xfs_mkdir(
	bhv_desc_t	*dir_bdp,
	char		*dir_name,
	vattr_t		*vap,
	vnode_t		**vpp,
	cred_t		*credp);

STATIC int
xfs_rmdir(
	bhv_desc_t	*dir_bdp,
	char		*name,
	vnode_t		*current_dir_vp,
	cred_t		*credp);

STATIC int
xfs_readdir(
	bhv_desc_t	*dir_bdp,
	uio_t		*uiop,
	cred_t		*credp,
	int		*eofp);

STATIC int
xfs_symlink(
	bhv_desc_t	*dir_bdp,
	char		*link_name,
	vattr_t		*vap,
	char		*target_path,
	cred_t		*credp);

STATIC int
xfs_fid(
	bhv_desc_t	*bdp,
	fid_t		**fidpp);

STATIC int
xfs_fid2(
	bhv_desc_t	*bdp,
	fid_t		*fidp);

STATIC int
xfs_seek(
	bhv_desc_t	*bdp,
	off_t		old_offset,
	off_t		*new_offsetp);

STATIC int
xfs_frlock(
	bhv_desc_t	*bdp,
	int		cmd,
	flock_t		*flockp,
	int		flag,
	off_t		offset,
	vrwlock_t	vrwlock,
	cred_t		*credp);

STATIC int
xfs_allocstore(
	bhv_desc_t	*bdp,
	off_t		offset,
	size_t		len,
	cred_t		*credp);

STATIC int
xfs_pathconf(
	bhv_desc_t	*bdp,
	int		cmd,
	long		*valp,
	struct cred 	*credp);

STATIC int
xfs_fcntl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flags,
	off_t		offset,
	cred_t		*credp,
	rval_t		*rvalp);

STATIC int
xfs_change_file_space(
	bhv_desc_t	*bdp,
	int		cmd,
	flock_t		*bf,
	off_t		offset,
	cred_t		*credp,
	int		attr_flags);

STATIC int
xfs_ioctl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flag,
	cred_t		*credp,
	int		*rvalp,
        struct vopbd    *vbds);

#if 0
STATIC void
xfs_itruncate_cleanup(
	xfs_trans_t	**tpp,
	xfs_inode_t	*ip,
	int		commit_flags,
	int		fork);
#endif

#endif	/* !SIM */

STATIC int
xfs_inactive(
	bhv_desc_t	*bdp,
	cred_t		*credp);

#ifndef SIM
STATIC int
xfs_inactive_free_eofblocks(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip);
#endif	/* !SIM */

STATIC int
xfs_reclaim(
	bhv_desc_t	*bdp,
	int		flag);

#ifdef XFS_RW_TRACE
STATIC void
xfs_ctrunc_trace(
	int		tag,
	xfs_inode_t	*ip);
#else
#define	xfs_ctrunc_trace(tag, ip)
#endif /* DEBUG */

#ifndef SIM
/*
 * For xfs, we check that the file isn't too big to be opened by this kernel.
 * No other open action is required for regular files.  Devices are handled
 * through the specfs file system, pipes through fifofs.  Device and
 * fifo vnodes are "wrapped" by specfs and fifofs vnodes, respectively,
 * when a new vnode is first looked up or created.
 */
/*ARGSUSED*/
STATIC int
xfs_open(
	bhv_desc_t 	*bdp,
	vnode_t		**vpp,
	mode_t		flag,
	cred_t		*credp)
{
	int		mode;
	int		rval = 0;
	vnode_t		*vp;
	xfs_inode_t	*ip;

	vp = BHV_TO_VNODE(bdp);
	ip = XFS_BHVTOI(bdp);

	if (XFS_FORCED_SHUTDOWN(ip->i_mount))
		return XFS_ERROR(EIO);

	/*
	 * If it's a directory with any blocks, read-ahead block 0
	 * as we're almost certain to have the next operation be a read there.
	 */
	if (vp->v_type == VDIR && ip->i_d.di_nextents > 0) {
		mode = xfs_ilock_map_shared(ip);
		(void)xfs_da_reada_buf(NULL, ip, 0, XFS_DATA_FORK);
		xfs_iunlock(ip, mode);
	}
#if !XFS_BIG_FILES
	else if (vp->v_type == VREG) {
		xfs_ilock(ip, XFS_ILOCK_SHARED);
		if (ip->i_d.di_size > XFS_MAX_FILE_OFFSET)
			rval = XFS_ERROR(EFBIG);
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
	}
#endif
	return rval;
}

/*
 * xfs_close
 *
 */
/*ARGSUSED*/
STATIC int
xfs_close(
	bhv_desc_t	*bdp,
	int		flag,
	lastclose_t	lastclose,
	cred_t		*credp)
{
	/* REFERENCED */
	vnode_t 	*vp;

	vp = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_close", (inst_t *)__return_address);
	return 0;
}


/*
 * xfs_getattr
 */
/*ARGSUSED*/
STATIC int
xfs_getattr(
	bhv_desc_t	*bdp,
	vattr_t		*vap,
	int		flags,
	cred_t		*credp)
{
	xfs_inode_t	*ip;
	xfs_mount_t	*mp;
	vnode_t 	*vp;

	vp  = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_getattr", (inst_t *)__return_address);
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	if (!(flags & ATTR_LAZY))
		xfs_ilock(ip, XFS_ILOCK_SHARED);

	vap->va_size = ip->i_d.di_size;
        if (vap->va_mask == AT_SIZE) {
		if (!(flags & ATTR_LAZY))
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
                return 0;
	}
        vap->va_fsid = ip->i_dev;
#if XFS_BIG_FILESYSTEMS
	vap->va_nodeid = ip->i_ino + mp->m_inoadd;
#else
	vap->va_nodeid = ip->i_ino;
#endif
        vap->va_nlink = ip->i_d.di_nlink;

	/*
	 * Quick exit for non-stat callers
	 */
	if ((vap->va_mask & ~(AT_SIZE|AT_FSID|AT_NODEID|AT_NLINK)) == 0) {
		if (!(flags & ATTR_LAZY))
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
                return 0;
	}

	/*
         * Copy from in-core inode.
         */
        vap->va_type = vp->v_type;
        vap->va_mode = ip->i_d.di_mode & MODEMASK;
        vap->va_uid = ip->i_d.di_uid;
        vap->va_gid = ip->i_d.di_gid;
	vap->va_projid = ip->i_d.di_projid;

	/*
	 * Check vnode type block/char vs. everything else.
	 * Do it with bitmask because that's faster than looking
	 * for multiple values individually.
	 */
	if (((1 << vp->v_type) & ((1<<VBLK) | (1<<VCHR))) == 0) {
		vap->va_rdev = 0;

		if (!(ip->i_d.di_flags & XFS_DIFLAG_REALTIME)) {

                        vap->va_blksize = mp->m_swidth ?
                                /*
                                 * If the underlying volume is a stripe, then
                                 * return the stripe width in bytes as the
                                 * recommended I/O size.
                                 */
                                (mp->m_swidth << mp->m_sb.sb_blocklog) :
                                /*
                                 * Return the largest of the preferred buffer
                                 * sizes since doing small I/Os into larger
                                 * buffers causes buffers to be decommissioned.
                                 * The value returned is in bytes.
                                 */
                                (1 << (int)MAX(ip->i_readio_log,
                                               ip->i_writeio_log));

		} else {

                        /*
                         * If the file blocks are being allocated from a
                         * realtime partition, then return the inode's
                         * realtime extent size or the realtime volume's
                         * extent size.
                         */
                        vap->va_blksize = ip->i_d.di_extsize ?
                                (ip->i_d.di_extsize << mp->m_sb.sb_blocklog) :
                                (mp->m_sb.sb_rextsize << mp->m_sb.sb_blocklog);
		}
	} else {
                vap->va_rdev = ip->i_df.if_u2.if_rdev;
                vap->va_blksize = BLKDEV_IOSIZE;
	}

        vap->va_atime.tv_sec = ip->i_d.di_atime.t_sec;
        vap->va_atime.tv_nsec = ip->i_d.di_atime.t_nsec;
        vap->va_mtime.tv_sec = ip->i_d.di_mtime.t_sec;
        vap->va_mtime.tv_nsec = ip->i_d.di_mtime.t_nsec;
        vap->va_ctime.tv_sec = ip->i_d.di_ctime.t_sec;
        vap->va_ctime.tv_nsec = ip->i_d.di_ctime.t_nsec;
	vap->va_nblocks =
		XFS_FSB_TO_BB(mp, ip->i_d.di_nblocks + ip->i_delayed_blks);

	/*
	 * Exit for stat callers.  See if any of the rest of the fields
	 * to be filled in are needed.
	 */
	if ((vap->va_mask &
	     (AT_XFLAGS|AT_EXTSIZE|AT_NEXTENTS|AT_ANEXTENTS|
	      AT_GENCOUNT|AT_VCODE)) == 0) {
		if (!(flags & ATTR_LAZY))
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
                return 0;
	}
	/*
	 * convert di_flags to xflags
	 */
	vap->va_xflags = 
		((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) ?
			XFS_XFLAG_REALTIME : 0) |
		((ip->i_d.di_flags & XFS_DIFLAG_PREALLOC) ?
			XFS_XFLAG_PREALLOC : 0);
	vap->va_extsize = ip->i_d.di_extsize << mp->m_sb.sb_blocklog;
	vap->va_nextents =
		(ip->i_df.if_flags & XFS_IFEXTENTS) ?
			ip->i_df.if_bytes / sizeof(xfs_bmbt_rec_t) :
			ip->i_d.di_nextents;
	if (ip->i_afp != NULL)
		vap->va_anextents =
			(ip->i_afp->if_flags & XFS_IFEXTENTS) ?
				ip->i_afp->if_bytes / sizeof(xfs_bmbt_rec_t) :
				 ip->i_d.di_anextents;
	else
		vap->va_anextents = 0;
	vap->va_gencount = ip->i_d.di_gen;
	vap->va_vcode = 0L;

	if (!(flags & ATTR_LAZY))
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
	return 0;
}


/*
 * xfs_setattr
 */
STATIC int
xfs_setattr(
	bhv_desc_t	*bdp,
	vattr_t		*vap,
	int		flags,
	cred_t		*credp)
{
        xfs_inode_t     *ip;
	xfs_trans_t	*tp;
	xfs_mount_t	*mp;
	int		mask;
	int		code;
	uint		lock_flags;
	uint		commit_flags;
	uid_t		uid, iuid;
	gid_t		gid, igid;
	int		timeflags = 0;
	vnode_t 	*vp;
	xfs_prid_t	projid, iprojid;
	int		privileged;
	int 		mandlock_before, mandlock_after;
	uint		qflags;
	struct xfs_dquot *udqp, *pdqp, *olddquot1, *olddquot2;
	int 		file_owner;

	vp = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_setattr", (inst_t *)__return_address);
	/*
	 * Cannot set certain attributes.
         */
        mask = vap->va_mask;
        if (mask & AT_NOSET) {
                return XFS_ERROR(EINVAL);
	}

        ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

        /*
         * Timestamps do not need to be logged and hence do not
         * need to be done within a transaction.
         */
        if (mask & AT_UPDTIMES) {
                ASSERT((mask & ~AT_UPDTIMES) == 0);
		timeflags = ((mask & AT_UPDATIME) ? XFS_ICHGTIME_ACC : 0) |
			    ((mask & AT_UPDCTIME) ? XFS_ICHGTIME_CHG : 0) |
			    ((mask & AT_UPDMTIME) ? XFS_ICHGTIME_MOD : 0);
		xfs_ichgtime(ip, timeflags);
		return 0;
        }

	olddquot1 = olddquot2 = NULL;
	udqp = pdqp = NULL;
	
	/*
	 * If disk quotas is on, we make sure that the dquots do exist on disk,
	 * before we start any other transactions. Trying to do this later
	 * is messy. We don't care  to take a readlock to look at the ids
	 * in inode here, because we can't hold it across the trans_reserve.
	 * If the IDs do change before we take the ilock, we're covered
	 * because the i_*dquot fields will get updated anyway.
	 */
	if (XFS_IS_QUOTA_ON(mp) && (mask & (AT_UID|AT_PROJID))) {
		qflags = 0;
		if (mask & AT_UID) {
			uid = vap->va_uid;
			qflags |= XFS_QMOPT_UQUOTA;
		} else {
			uid = ip->i_d.di_uid;
		}
		if (mask & AT_PROJID) {
			projid = (xfs_prid_t)vap->va_projid;
			qflags |= XFS_QMOPT_PQUOTA;
		}  else {
			projid = ip->i_d.di_projid;
		}
		/*
		 * We take a reference when we initialize udqp and pdqp,
		 * so it is important that we never blindly double trip on
		 * the same variable. See xfs_create() for an example.
		 */
		ASSERT(udqp == NULL);
		ASSERT(pdqp == NULL);
		if (code = xfs_qm_vop_dqalloc(mp, ip, uid, projid, qflags,
						    &udqp, &pdqp)) 
			return (code);
	}

	/*
	 * For the other attributes, we acquire the inode lock and
	 * first do an error checking pass.
	 */
	tp = NULL;
	lock_flags = XFS_ILOCK_EXCL;
	if (!(mask & AT_SIZE)) {
		tp = xfs_trans_alloc(mp, XFS_TRANS_SETATTR_NOT_SIZE);
		commit_flags = 0;
		if (code = xfs_trans_reserve(tp, 0,
					     XFS_ICHANGE_LOG_RES(mp), 0,
					     0, 0)) {
			lock_flags = 0;
			goto error_return;
		}
	} else {
#ifndef SIM
		if (DM_EVENT_ENABLED (vp->v_vfsp, ip, DM_EVENT_TRUNCATE) &&
		    !(flags & ATTR_DMI)) {
			code = xfs_dm_send_data_event (DM_EVENT_TRUNCATE, bdp,
				vap->va_size, 0, AT_DELAY_FLAG(flags), NULL);
			if (code) {
				lock_flags = 0;
				goto error_return;
			}
		}
#endif
		lock_flags |= XFS_IOLOCK_EXCL;
	}

        xfs_ilock(ip, lock_flags);

	if (_MAC_XFS_IACCESS(ip, MACWRITE, credp)) {
		code = XFS_ERROR(EACCES);
		goto error_return;
	}

	/* boolean: are we the file owner? */
	file_owner = (credp->cr_uid == ip->i_d.di_uid);

	/*
	 * Change various properties of a file.
	 * Only the owner or users with CAP_FOWNER
	 * capability may do these things.
	 */
        if (mask & (AT_MODE|AT_XFLAGS|AT_EXTSIZE|AT_UID|AT_GID|AT_PROJID)) {
		/*
		 * CAP_FOWNER overrides the following restrictions:
		 *
		 * The user ID of the calling process must be equal
		 * to the file owner ID, except in cases where the
		 * CAP_FSETID capability is applicable.
		 */
                if (!file_owner && !cap_able_cred(credp, CAP_FOWNER)) {
                        code = XFS_ERROR(EPERM);
                        goto error_return;
                }

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
		if (mask & AT_MODE) {
			mode_t m = 0;

			if ((vap->va_mode & ISUID) && !file_owner)
				m |= ISUID;
			if ((vap->va_mode & ISGID) &&
			    !groupmember(ip->i_d.di_gid, credp))
				m |= ISGID;
			if ((vap->va_mode & ISVTX) && vp->v_type != VDIR)
				m |= ISVTX;
			if (m && !cap_able_cred(credp, CAP_FSETID))
				vap->va_mode &= ~m;
		}
        }

        /*
         * Change file ownership.  Must be the owner or privileged.
         * If the system was configured with the "restricted_chown"
         * option, the owner is not permitted to give away the file,
         * and can change the group id only to a group of which he
         * or she is a member.
         */
        if (mask & (AT_UID|AT_GID|AT_PROJID)) {
                /*
		 * These IDs could have changed since we last looked at them.
		 * But, we're assured that if the ownership did change
		 * while we didn't have the inode locked, inode's dquot(s) 
		 * would have changed also.
		 */
		iuid = ip->i_d.di_uid;
		iprojid = ip->i_d.di_projid;
		igid = ip->i_d.di_gid; 
                gid = (mask & AT_GID) ? vap->va_gid : igid;
		uid = (mask & AT_UID) ? vap->va_uid : iuid;
		projid = (mask & AT_PROJID) ? (xfs_prid_t)vap->va_projid : 
			 iprojid;

		/*
		 * CAP_CHOWN overrides the following restrictions:
		 *
		 * If _POSIX_CHOWN_RESTRICTED is defined, this capability
		 * shall override the restriction that a process cannot
		 * change the user ID of a file it owns and the restriction
		 * that the group ID supplied to the chown() function
		 * shall be equal to either the group ID or one of the
		 * supplementary group IDs of the calling process.
		 *
		 * XXX: How does restricted_chown affect projid?
		 */
		if (restricted_chown &&
		    (iuid != uid || (igid != gid &&
				     !groupmember(gid, credp))) &&
		    !cap_able_cred(credp, CAP_CHOWN)) {
			code = XFS_ERROR(EPERM);
			goto error_return;
		}
		/*
		 * Do a quota reservation only if uid or projid is actually
		 * going to change.
		 */
		if ((XFS_IS_UQUOTA_ON(mp) && iuid != uid) ||
		    (XFS_IS_PQUOTA_ON(mp) && iprojid != projid)) {
			ASSERT(tp);
			/*
			 * XXX:casey - This may result in unnecessary auditing.
			 */
			privileged = cap_able_cred(credp, CAP_FOWNER);
			if (code = xfs_qm_vop_chown_reserve(tp, ip, udqp, pdqp,
							  privileged ?
							  XFS_QMOPT_FORCE_RES :
							  0))
				/* out of quota */
				goto error_return;
		}
        }

        /*
         * Truncate file.  Must have write permission and not be a directory.
         */
        if (mask & AT_SIZE) {
                if (vp->v_type == VDIR) {
                        code = XFS_ERROR(EISDIR);
                        goto error_return;
		} else if (vp->v_type != VREG) {
			code = XFS_ERROR(EINVAL);
			goto error_return;
		}
		if (vp->v_flag & VISSWAP) {
			code = XFS_ERROR(EACCES);
			goto error_return;
		}
		if (!(mask & AT_SIZE_NOPERM)) {
			if (code = xfs_iaccess(ip, IWRITE, credp))
				goto error_return;
		}
		/* 
		 * Make sure that the dquots are attached to the inode.
		 */
		if (XFS_IS_QUOTA_ON(mp) && XFS_NOT_DQATTACHED(mp, ip)) {
			if (code = xfs_qm_dqattach(ip, XFS_QMOPT_ILOCKED)) 
				goto error_return;
		}
        }

        /*
         * Change file access or modified times.
         */
        if (mask & (AT_ATIME|AT_MTIME)) {
		if (!file_owner) {
			if ((flags & ATTR_UTIME) &&
			    !cap_able_cred(credp, CAP_FOWNER)) {
				code = XFS_ERROR(EPERM);
				goto error_return;
			}
			if ((code = xfs_iaccess(ip, IWRITE, credp)) &&
			    !cap_able_cred(credp, CAP_FOWNER)) {
				goto error_return;
			}
		}
        }

	/*
	 * Change extent size or realtime flag.
	 */
	if (mask & (AT_EXTSIZE|AT_XFLAGS)) {
		/*
		 * Can't change extent size if any extents are allocated.
		 */
		if (ip->i_d.di_nextents && (mask & AT_EXTSIZE) &&
		    ((ip->i_d.di_extsize << mp->m_sb.sb_blocklog) != 
		     vap->va_extsize) ) {
			code = XFS_ERROR(EINVAL);	/* EFBIG? */
			goto error_return;
		}

		/*
		 * Can't set extent size unless the file is marked, or
		 * about to be marked as a realtime file.	
		 *
		 * This check will be removed when fixed size extents
		 * with buffered data writes is implemented.
		 *
		 */
		if ((mask & AT_EXTSIZE) 			&&
		    ((ip->i_d.di_extsize << mp->m_sb.sb_blocklog) != 
		     vap->va_extsize) &&
		    (!((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) ||
		       ((mask & AT_XFLAGS) &&
			(vap->va_xflags & XFS_XFLAG_REALTIME))))) {
			code = XFS_ERROR(EINVAL);
			goto error_return;
		}

		/*
		 * Can't change realtime flag if any extents are allocated.
		 */
		if (ip->i_d.di_nextents && (mask & AT_XFLAGS) &&
		    (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) !=
		    (vap->va_xflags & XFS_XFLAG_REALTIME)) {
			code = XFS_ERROR(EINVAL);	/* EFBIG? */
			goto error_return;
		}
		/*
		 * Extent size must be a multiple of the appropriate block
		 * size, if set at all.
		 */
		if ((mask & AT_EXTSIZE) && vap->va_extsize != 0) {
			xfs_extlen_t	size;

			if ((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) ||
			    ((mask & AT_XFLAGS) && 
			    (vap->va_xflags & XFS_XFLAG_REALTIME))) {
				size = mp->m_sb.sb_rextsize <<
				       mp->m_sb.sb_blocklog;
			} else {
				size = mp->m_sb.sb_blocksize;
			}
			if (vap->va_extsize % size) {
				code = XFS_ERROR(EINVAL);
				goto error_return;
			}
		}
		/*
		 * If realtime flag is set then must have realtime data.
		 */
		if ((mask & AT_XFLAGS) &&
		    (vap->va_xflags & XFS_XFLAG_REALTIME)) {
			if ((mp->m_sb.sb_rblocks == 0) ||
			    (mp->m_sb.sb_rextsize == 0) ||
			    (ip->i_d.di_extsize % mp->m_sb.sb_rextsize)) {
				code = XFS_ERROR(EINVAL);
				goto error_return;
			}
		}
	}

	/*
	 * Now we can make the changes.  Before we join the inode
	 * to the transaction, if AT_SIZE is set then take care of
	 * the part of the truncation that must be done without the
	 * inode lock.  This needs to be done before joining the inode
	 * to the transaction, because the inode cannot be unlocked
	 * once it is a part of the transaction.
	 */
	if (mask & AT_SIZE) {
		if (vap->va_size > ip->i_d.di_size) {
			code = xfs_igrow_start(ip, vap->va_size, credp);
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
		} else if (vap->va_size < ip->i_d.di_size) {
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			xfs_itruncate_start(ip, XFS_ITRUNC_DEFINITE,
					    (xfs_fsize_t)vap->va_size);
			code = 0;
		} else {
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			code = 0;
		}
		if (code) {
			ASSERT(tp == NULL);
			lock_flags &= ~XFS_ILOCK_EXCL;
			ASSERT(lock_flags == XFS_IOLOCK_EXCL);
			goto error_return;
		}
		tp = xfs_trans_alloc(mp, XFS_TRANS_SETATTR_SIZE);
		if (code = xfs_trans_reserve(tp, 0,
					     XFS_ITRUNCATE_LOG_RES(mp), 0,
					     XFS_TRANS_PERM_LOG_RES,
					     XFS_ITRUNCATE_LOG_COUNT)) {
			xfs_trans_cancel(tp, 0);
			xfs_iunlock(ip, XFS_IOLOCK_EXCL);
			return code;
		}
		commit_flags = XFS_TRANS_RELEASE_LOG_RES;
		xfs_ilock(ip, XFS_ILOCK_EXCL);
	}

        xfs_trans_ijoin(tp, ip, lock_flags);
	xfs_trans_ihold(tp, ip);

	/* determine whether mandatory locking mode changes */
	mandlock_before = MANDLOCK(vp, ip->i_d.di_mode);

	/*
         * Truncate file.  Must have write permission and not be a directory.
         */
        if (mask & AT_SIZE) {
		if (vap->va_size > ip->i_d.di_size) {
			xfs_igrow_finish(tp, ip, vap->va_size,
			    !(flags & ATTR_DMI));
		} else if (vap->va_size < ip->i_d.di_size) {
			/*
			 * signal a sync transaction unless
			 * we're truncating an already unlinked
			 * file on a wsync filesystem
			 */
			code = xfs_itruncate_finish(&tp, ip,
					    (xfs_fsize_t)vap->va_size,
					    XFS_DATA_FORK,
					    ((ip->i_d.di_nlink != 0 ||
					      !(mp->m_flags & XFS_MOUNT_WSYNC))
					     ? 1 : 0));
			if (code) {
				goto abort_return;
			}
		}
		/*
		 * Have to do this even if the file's size doesn't change.
		 */
		timeflags |= XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG;
        }

        /*
         * Change file access modes.
         */
        if (mask & AT_MODE) {
                ip->i_d.di_mode &= IFMT;
                ip->i_d.di_mode |= vap->va_mode & ~IFMT;

		xfs_trans_log_inode (tp, ip, XFS_ILOG_CORE);
		timeflags |= XFS_ICHGTIME_CHG;
        }

	/*
	 * Change file ownership.  Must be the owner or privileged.
         * If the system was configured with the "restricted_chown"
         * option, the owner is not permitted to give away the file,
         * and can change the group id only to a group of which he
         * or she is a member.
         */
        if (mask & (AT_UID|AT_GID|AT_PROJID)) {
		/*
		 * CAP_FSETID overrides the following restrictions:
		 *
		 * The set-user-ID and set-group-ID bits of a file will be
		 * cleared upon successful return from chown()
		 */
                if ((ip->i_d.di_mode & (ISUID|ISGID)) && 
		    !cap_able_cred(credp, CAP_FSETID)) {
                        ip->i_d.di_mode &= ~(ISUID|ISGID);
                }
                
		/*
		 * Change the ownerships and register quota modifications
		 * in the transaction.
		 */
		if (iuid != uid) {
			if (XFS_IS_UQUOTA_ON(mp)) {
				ASSERT(mask & AT_UID);
				ASSERT(udqp);
				ASSERT(xfs_qm_dqid(udqp) == (xfs_dqid_t)uid);
				olddquot1 = xfs_qm_vop_chown(tp, ip, 
							     &ip->i_udquot, 
							     udqp);
				/*
				 * We'l dqrele olddquot at the end.
				 */
			}
			ip->i_d.di_uid = uid;
		}
		if (iprojid != projid) {
			if (XFS_IS_PQUOTA_ON(mp)) {
				ASSERT(mask & AT_PROJID);
				ASSERT(pdqp);
				ASSERT(xfs_qm_dqid(pdqp) == (xfs_dqid_t)projid);
				olddquot2 = xfs_qm_vop_chown(tp, ip, 
							     &ip->i_pdquot, 
							     pdqp);
				
			}
			ip->i_d.di_projid = projid;
			/*
			 * We may have to rev the inode as well as
			 * the superblock version number since projids didn't
			 * exist before DINODE_VERSION_2 and SB_VERSION_NLINK.
			 */
			if (ip->i_d.di_version == XFS_DINODE_VERSION_1)
				xfs_bump_ino_vers2(tp, ip);
		}
		if (igid != gid)
			ip->i_d.di_gid = gid;

		xfs_trans_log_inode (tp, ip, XFS_ILOG_CORE);
		timeflags |= XFS_ICHGTIME_CHG;
        }


	/*
         * Change file access or modified times.
         */
        if (mask & (AT_ATIME|AT_MTIME)) {
                if (mask & AT_ATIME) {
                        ip->i_d.di_atime.t_sec = vap->va_atime.tv_sec;
                        ip->i_d.di_atime.t_nsec = vap->va_atime.tv_nsec;
			ip->i_update_core = 1;
			timeflags &= ~XFS_ICHGTIME_ACC;
		}
                if (mask & AT_MTIME) {
			ip->i_d.di_mtime.t_sec = vap->va_mtime.tv_sec;
			ip->i_d.di_mtime.t_nsec = vap->va_mtime.tv_nsec;
			timeflags &= ~XFS_ICHGTIME_MOD;
			timeflags |= XFS_ICHGTIME_CHG;
                }
        }

	/*
	 * Change XFS-added attributes.
	 */
	if (mask & (AT_EXTSIZE|AT_XFLAGS)) {
		if (mask & AT_EXTSIZE) {
			/*
 			 * Converting bytes to fs blocks.
			 */
			ip->i_d.di_extsize = vap->va_extsize >>
				mp->m_sb.sb_blocklog;
		}
		if (mask & AT_XFLAGS) {
			ip->i_d.di_flags = 0;
			if (vap->va_xflags & XFS_XFLAG_REALTIME) {
				ip->i_d.di_flags |= XFS_DIFLAG_REALTIME;
			}
			/* can't set PREALLOC this way, just ignore it */
		}
		xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
		timeflags |= XFS_ICHGTIME_CHG;
	}

	/*
         * Change file inode change time only if AT_CTIME set 
	 * AND we have been called by a DMI function.
         */

	if ( (flags & ATTR_DMI) && (mask & AT_CTIME) ) {
                ip->i_d.di_ctime.t_sec = vap->va_ctime.tv_sec;
                ip->i_d.di_ctime.t_nsec = vap->va_ctime.tv_nsec;
		ip->i_update_core = 1;
		timeflags &= ~XFS_ICHGTIME_CHG;
	}

	/*
	 * Send out timestamp changes that need to be set to the
	 * current time.  Not done when called by a DMI function.
	 */
	if (timeflags && !(flags & ATTR_DMI))
		xfs_ichgtime(ip, timeflags);

	XFSSTATS.xs_ig_attrchg++;

	/*
	 * If this is a synchronous mount, make sure that the
	 * transaction goes to disk before returning to the user.
	 * This is slightly sub-optimal in that truncates require
	 * two sync transactions instead of one for wsync filesytems.
	 * One for the truncate and one for the timestamps since we
	 * don't want to change the timestamps unless we're sure the
	 * truncate worked.  Truncates are less than 1% of the laddis
	 * mix so this probably isn't worth the trouble to optimize.
	 */
	if (mp->m_flags & XFS_MOUNT_WSYNC)
		xfs_trans_set_sync(tp);

	code = xfs_trans_commit(tp, commit_flags, NULL);

	/*
	 * If the (regular) file's mandatory locking mode changed, then
	 * notify the vnode.  We do this under the inode lock to prevent
	 * racing calls to vop_vnode_change.
	 */
	mandlock_after = MANDLOCK(vp, ip->i_d.di_mode);
	if (mandlock_before != mandlock_after) {
		VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_ENF_LOCKING, 
				 mandlock_after);
	}

	xfs_iunlock(ip, lock_flags);
	
	/* 
	 * release any dquot(s) inode had kept before chown
	 */
	if (olddquot1) 
		xfs_qm_dqrele(olddquot1); 
	if (olddquot2) 
		xfs_qm_dqrele(olddquot2); 
	if (udqp)
		xfs_qm_dqrele(udqp);
	if (pdqp)
		xfs_qm_dqrele(pdqp);

	if (code) {
		return code;
	}

#ifndef SIM
	if (DM_EVENT_ENABLED(vp->v_vfsp, ip, DM_EVENT_ATTRIBUTE) &&
	    !(flags & ATTR_DMI)) {
		(void) dm_send_namesp_event (DM_EVENT_ATTRIBUTE, bdp, DM_RIGHT_NULL,
				NULL, DM_RIGHT_NULL, NULL, NULL,
			0, 0, AT_DELAY_FLAG(flags));
	}
#endif
	return 0;

 abort_return:
	commit_flags |= XFS_TRANS_ABORT;
	/* FALLTHROUGH */
 error_return:
	if (udqp)
		xfs_qm_dqrele(udqp);
	if (pdqp)
		xfs_qm_dqrele(pdqp);
	if (tp) {
		xfs_trans_cancel(tp, commit_flags);
	}
	if (lock_flags != 0) {
		xfs_iunlock(ip, lock_flags);
	}
	return code;
} /* xfs_setattr */


/*
 * xfs_access
 * Null conversion from vnode mode bits to inode mode bits, as in efs.
 */
/*ARGSUSED*/
STATIC int
xfs_access(
	bhv_desc_t	*bdp,
	int		mode,
	cred_t		*credp)
{
	xfs_inode_t	*ip;
	int		error;

	vn_trace_entry(BHV_TO_VNODE(bdp), "xfs_access",
		       (inst_t *)__return_address);
	ip = XFS_BHVTOI(bdp);
	xfs_ilock(ip, XFS_ILOCK_SHARED);
	error = xfs_iaccess(ip, mode, credp);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	return error;
}


/*
 * xfs_readlink
 *
 */
/*ARGSUSED*/
STATIC int
xfs_readlink(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	cred_t		*credp)
{
        xfs_inode_t     *ip;
	int		count;
	off_t		offset;
	int		pathlen;
	vnode_t 	*vp;
        int             error = 0;
	xfs_mount_t	*mp;
	xfs_fsblock_t	firstblock;
	int             nmaps;
	xfs_bmbt_irec_t mval[SYMLINK_MAPS];
	daddr_t         d;
	int             byte_cnt;
	int		n;
	buf_t		*bp;

	vp = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_readlink", (inst_t *)__return_address);
	if (vp->v_type != VLNK)
                return XFS_ERROR(EINVAL);

	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	xfs_ilock(ip, XFS_ILOCK_SHARED);

	ASSERT((ip->i_d.di_mode & IFMT) == IFLNK);

	offset = uiop->uio_offset;
        count = uiop->uio_resid;

	if (offset < 0) {
                error = XFS_ERROR(EINVAL);
		goto error_return;
	}
        if (count <= 0) {
                error = 0;
		goto error_return;
	}

	if (!(uiop->uio_fmode & FINVIS)) {
		xfs_ichgtime(ip, XFS_ICHGTIME_ACC);
	}

	/*
	 * See if the symlink is stored inline.
	 */
	pathlen = (int)ip->i_d.di_size;

	if (ip->i_df.if_flags & XFS_IFINLINE) {
		error = uiomove(ip->i_df.if_u1.if_data, pathlen, UIO_READ, uiop);
	}
	else {
		/*
		 * Symlink not inline.  Call bmap to get it in.
		 */
                nmaps = SYMLINK_MAPS;
		firstblock = NULLFSBLOCK;

                error = xfs_bmapi(NULL, ip, 0, XFS_B_TO_FSB(mp, pathlen),
				  0, &firstblock, 0, mval, &nmaps, NULL);

		if (error) {
			goto error_return;
		}

                for (n = 0; n < nmaps; n++) {
                        d = XFS_FSB_TO_DADDR(mp, mval[n].br_startblock);
                        byte_cnt = XFS_FSB_TO_B(mp, mval[n].br_blockcount);
                        bp = read_buf_targ(mp->m_ddev_targp, d,
				      BTOBB(byte_cnt), 0);
			error = geterror(bp);
			if (error) {
				brelse(bp);
				goto error_return;
			}
                        if (pathlen < byte_cnt)
                                byte_cnt = pathlen;
                        pathlen -= byte_cnt;

                        error = uiomove(bp->b_un.b_addr, byte_cnt,
					 UIO_READ, uiop);
			brelse (bp);
                }

	}


error_return:

	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	return error;
}

/*
 * xfs_fsync
 *
 * This is called to sync the inode and its data out to disk.
 * We need to hold the I/O lock while flushing the data, and
 * the inode lock while flushing the inode.  The inode lock CANNOT
 * be held while flushing the data, so acquire after we're done
 * with that.
 */
/*ARGSUSED*/
STATIC int
xfs_fsync(
	bhv_desc_t	*bdp,
	int		flag,
	cred_t		*credp,
	off_t		start,
	off_t		stop)
{
	xfs_inode_t	*ip;
	int		error;
	/* REFERENCED */
	int		error2;
					/* REFERENCED */
	int		syncall;
	vnode_t 	*vp;
	xfs_trans_t	*tp;

	vp = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_fsync", (inst_t *)__return_address);
	ip = XFS_BHVTOI(bdp);

	ASSERT(start >= 0 && stop >= -1);

	if (XFS_FORCED_SHUTDOWN(ip->i_mount))
		return XFS_ERROR(EIO);

	xfs_ilock(ip, XFS_IOLOCK_EXCL);

	syncall = error = error2 = 0;

	if (stop == -1)  {
		ASSERT(start >= 0);
		if (start == 0)
			syncall = 1;
		stop = xfs_file_last_byte(ip);
	}

	/*
	 * If we're invalidating, always flush since we want to
	 * tear things down.  Otherwise, don't flush anything if
	 * we're not dirty.
	 */
	if (flag & FSYNC_INVAL) {
		if (ip->i_df.if_flags & XFS_IFEXTENTS &&
		    ip->i_df.if_bytes > 0) {
			VOP_FLUSHINVAL_PAGES(vp, start, stop, FI_REMAPF_LOCKED);
		}
		ASSERT(syncall == 0 ||
		       (vp->v_pgcnt == 0 && vp->v_buf == 0));
	} else if (VN_DIRTY(vp)) {
		/*
		 * In the non-invalidating case, calls to fsync() do not
		 * flush all the dirty mmap'd pages.  That requires a
		 * call to msync().
		 */
		VOP_FLUSH_PAGES(vp, start, stop, (flag & FSYNC_WAIT) ? 0 : B_ASYNC,
				FI_NONE, error2);
	}

	if (error2) {
		xfs_iunlock(ip, XFS_IOLOCK_EXCL);
		return XFS_ERROR(error2);
	}

	/*
	 * Make sure that we flushed everything in a full sync.
	 * We used to assert that i_delayed_blks was 0 here,
	 * but we can't do that since xfs_allocstore() could
	 * come in and add more even though we have the I/O
	 * lock here.  All it needs to do so is the inode lock,
	 * and we don't want to force it to acquire the I/O
	 * lock unnecessarily.
	 */
	ASSERT(!(flag & (FSYNC_INVAL | FSYNC_WAIT)) ||
	       syncall == 0 ||
	       (!VN_DIRTY(vp) && (ip->i_queued_bufs == 0)));

	/*
	 * We always need to make sure that the required inode state
	 * is safe on disk.  The vnode might be clean but because
	 * of committed transactions that haven't hit the disk yet.
	 * Likewise, there could be unflushed non-transactional
	 * changes to the inode core that have to go to disk.
	 *
	 * The following code depends on one assumption:  that
	 * any transaction that changes an inode logs the core
	 * because it has to change some field in the inode core
	 * (typically nextents or nblocks).  That assumption
	 * implies that any transactions against an inode will
	 * catch any non-transactional updates.  If inode-altering
	 * transactions exist that violate this assumption, the
	 * code breaks.  Right now, it figures that if the involved
	 * update_* field is clear and the inode is unpinned, the
	 * inode is clean.  Either it's been flushed or it's been
	 * committed and the commit has hit the disk unpinning the inode.
	 * (Note that xfs_inode_item_format() called at commit clears
	 * the update_* fields.)
	 */
	if (!(flag & FSYNC_DATA)) {
		xfs_ilock(ip, XFS_ILOCK_SHARED);

		if (ip->i_update_core == 0)  {
			/*
			 * Timestamps/size haven't changed since last inode
			 * flush or inode transaction commit.  That means
			 * either nothing got written or a transaction
			 * committed which caught the updates.  If the
			 * latter happened and the transaction hasn't
			 * hit the disk yet, the inode will be still
			 * be pinned.  If it is, force the log.
			 */
			if (xfs_ipincount(ip) == 0)  {
				xfs_iunlock(ip, XFS_IOLOCK_EXCL |
						XFS_ILOCK_SHARED);
			} else  {
				xfs_iunlock(ip, XFS_IOLOCK_EXCL |
						XFS_ILOCK_SHARED);
				xfs_log_force(ip->i_mount, (xfs_lsn_t)0,
					      XFS_LOG_FORCE |
					      ((flag & FSYNC_WAIT)
					       ? XFS_LOG_SYNC : 0));
			}
			error = 0;
		} else  {
			/*
			 * Kick off a transaction to log the inode
			 * core to get the updates.  Make it
			 * sync if FSYNC_WAIT is passed in (which
			 * is done by everybody but specfs).  The
			 * sync transaction will also force the log.
			 */
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
			tp = xfs_trans_alloc(ip->i_mount, XFS_TRANS_FSYNC_TS);
			if (error = xfs_trans_reserve(tp, 0,
					XFS_FSYNC_TS_LOG_RES(ip->i_mount),
					0, 0, 0))  {
				xfs_trans_cancel(tp, 0);
				xfs_iunlock(ip, XFS_IOLOCK_EXCL);
				return error;
			}
			xfs_ilock(ip, XFS_ILOCK_EXCL);

			/*
			 * Note - it's possible that we might have pushed
			 * ourselves out of the way during trans_reserve
			 * which would flush the inode.  But there's no
			 * guarantee that the inode buffer has actually
			 * gone out yet (it's delwri).  Plus the buffer
			 * could be pinned anyway if it's part of an
			 * inode in another recent transaction.  So we
			 * play it safe and fire off the transaction anyway.
			 */
			xfs_trans_ijoin(tp, ip, XFS_IOLOCK_EXCL|XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);
			xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
			if (flag & FSYNC_WAIT)
				xfs_trans_set_sync(tp);
			error = xfs_trans_commit(tp, 0, NULL);

			xfs_iunlock(ip, XFS_IOLOCK_EXCL|XFS_ILOCK_EXCL);
		}
	} else {
		/*
		 * We don't care about the timestamps here.  We
		 * only care about the size field growing on us
		 * and forcing any space allocation transactions.
		 * We have to flush changes to the size fields
		 * otherwise we could write out data that
		 * becomes inaccessible after a crash.
		 */
		xfs_ilock(ip, XFS_ILOCK_SHARED);

		if (ip->i_update_size == 0)  {
			/*
			 * Force the log if the inode is pinned.
			 * That ensures that all transactions committed
			 * against the inode hit the disk.  This may do
			 * too much work but it's safe.
			 */
			if (xfs_ipincount(ip) == 0)  {
				xfs_iunlock(ip, XFS_IOLOCK_EXCL |
						XFS_ILOCK_SHARED);
			} else  {
				xfs_iunlock(ip, XFS_IOLOCK_EXCL |
						XFS_ILOCK_SHARED);
				xfs_log_force(ip->i_mount, (xfs_lsn_t)0,
					      XFS_LOG_FORCE |
					      ((flag & FSYNC_WAIT)
					       ? XFS_LOG_SYNC : 0));
			}
			error = 0;
		} else  {
			/*
			 * Kick off a sync transaction to log the inode
			 * core.  The transaction has to be sync since
			 * we need these updates to guarantee that the
			 * data written will be seen.  The sync
			 * transaction will also force the log.
			 */
			xfs_iunlock(ip, XFS_ILOCK_SHARED);

			tp = xfs_trans_alloc(ip->i_mount, XFS_TRANS_FSYNC_TS);
			if (error = xfs_trans_reserve(tp, 0,
					XFS_FSYNC_TS_LOG_RES(ip->i_mount),
					0, 0, 0))  {
				xfs_trans_cancel(tp, 0);
				xfs_iunlock(ip, XFS_IOLOCK_EXCL);
				return error;
			}
			xfs_ilock(ip, XFS_ILOCK_EXCL);

			/*
			 * Note - it's possible that we might have pushed
			 * ourselves out of the way during trans_reserve
			 * which would flush the inode.  But there's no
			 * guarantee that the inode buffer has actually
			 * gone out yet (it's delwri).  Plus the buffer
			 * could be pinned anyway if it's part of an
			 * inode in another recent transaction.  So we
			 * play it safe and fire off the transaction anyway.
			 */
			xfs_trans_ijoin(tp, ip, XFS_IOLOCK_EXCL|XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);
			xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
			if (flag & FSYNC_WAIT)
				xfs_trans_set_sync(tp);
			error = xfs_trans_commit(tp, 0, NULL);

			xfs_iunlock(ip, XFS_IOLOCK_EXCL|XFS_ILOCK_EXCL);
		}
	}
	return error;
}


#if 0
/*
 * This is a utility routine for xfs_inactive.  It is called when a
 * transaction attempting to free up the disk space for a file encounters
 * an error.  It cancels the old transaction and starts up a new one
 * to be used to free up the inode.  It also sets the inode size and extent
 * counts to 0 and frees up any memory being used to store inline data,
 * extents, or btree roots.
 */
STATIC void
xfs_itruncate_cleanup(
	xfs_trans_t	**tpp,
	xfs_inode_t	*ip,
	int		commit_flags,
	int		fork)
{
	xfs_mount_t	*mp;
	/* REFERENCED */
	int		error;

	mp = ip->i_mount;
	if (*tpp) {
		xfs_trans_cancel(*tpp, commit_flags | XFS_TRANS_ABORT);
	}
	xfs_iunlock(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
	*tpp = xfs_trans_alloc(mp, XFS_TRANS_INACTIVE);
	error = xfs_trans_reserve(*tpp, 0, XFS_IFREE_LOG_RES(mp), 0, 0,
				  XFS_DEFAULT_LOG_COUNT);
	if (error) {
		return;
	}
	
	xfs_ilock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	xfs_trans_ijoin(*tpp, ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
	xfs_trans_ihold(*tpp, ip);

	xfs_idestroy_fork(ip, fork);

	if (fork == XFS_DATA_FORK) {
		ip->i_d.di_nblocks = 0;
		ip->i_d.di_nextents = 0;
		ip->i_d.di_size = 0;
	} else {
		ip->i_d.di_anextents = 0;
	}
	xfs_trans_log_inode(*tpp, ip, XFS_ILOG_CORE);
}
#endif

/*
 * This is called by xfs_inactive to free any blocks beyond eof,
 * when the link count isn't zero.
 */
STATIC int
xfs_inactive_free_eofblocks(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip)
{
	xfs_trans_t	*tp;
	int		error;
	xfs_fileoff_t	end_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_filblks_t	map_len;
	int		nimaps;
	xfs_bmbt_irec_t	imap;
	xfs_fsblock_t	first_block;

	/*
	 * Figure out if there are any blocks beyond the end
	 * of the file.  If not, then there is nothing to do.
	 */
	end_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)ip->i_d.di_size));
	last_fsb = XFS_B_TO_FSB(mp, (xfs_ufsize_t)XFS_MAX_FILE_OFFSET);
	map_len = last_fsb - end_fsb;
	if (map_len <= 0)
		return (0);

	nimaps = 1;
	first_block = NULLFSBLOCK;
	xfs_ilock(ip, XFS_ILOCK_SHARED);
	error = xfs_bmapi(NULL, ip, end_fsb, map_len, 0,
			  &first_block, 0, &imap, &nimaps,
			  NULL);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	if (!error && (nimaps != 0) &&
	    (imap.br_startblock != HOLESTARTBLOCK)) {
		/* 
		 * Attach the dquots to the inode up front.
		 */
		if (XFS_IS_QUOTA_ON(mp) &&
		    ip->i_ino != mp->m_sb.sb_uquotino && 
		    ip->i_ino != mp->m_sb.sb_pquotino) {
			if (XFS_NOT_DQATTACHED(mp, ip)) {
				if (error = xfs_qm_dqattach(ip, 0))
					return (error);
			}
		}
		
		/*
		 * There are blocks after the end of file.
		 * Free them up now by truncating the file to
		 * its current size.
		 */
		tp = xfs_trans_alloc(mp, XFS_TRANS_INACTIVE);
		
		/*
		 * Do the xfs_itruncate_start() call before
		 * reserving any log space because
		 * itruncate_start will call into the buffer
		 * cache and we can't
		 * do that within a transaction.
		 */
		xfs_ilock(ip, XFS_IOLOCK_EXCL);
		xfs_itruncate_start(ip, XFS_ITRUNC_DEFINITE,
				    ip->i_d.di_size);
				
		error = xfs_trans_reserve(tp, 0,
					  XFS_ITRUNCATE_LOG_RES(mp),
					  0, XFS_TRANS_PERM_LOG_RES,
					  XFS_ITRUNCATE_LOG_COUNT);
		if (error) {
			ASSERT(XFS_FORCED_SHUTDOWN(mp));
			xfs_trans_cancel(tp, 0);
			xfs_iunlock(ip, XFS_IOLOCK_EXCL);
			return (error);
		}
				
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		xfs_trans_ijoin(tp, ip,
				XFS_IOLOCK_EXCL |
				XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);

		error = xfs_itruncate_finish(&tp, ip,
					     ip->i_d.di_size,
					     XFS_DATA_FORK,
					     0);
		/*
		 * If we get an error at this point we
		 * simply don't bother truncating the file.
		 */
		if (error) {
			xfs_trans_cancel(tp,
					 (XFS_TRANS_RELEASE_LOG_RES |
					  XFS_TRANS_ABORT));
		} else {
			error = xfs_trans_commit(tp,
						XFS_TRANS_RELEASE_LOG_RES,
						NULL);
		}
		xfs_iunlock(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
	}
	return (error);
}

/*
 * Free a symlink that has blocks associated with it.
 */
STATIC int
xfs_inactive_symlink_rmt(
	xfs_inode_t	*ip,
	xfs_trans_t	**tpp)
{
	buf_t		*bp;
	int		committed;
	int		done;
	int		error;
	xfs_fsblock_t	first_block;
	xfs_bmap_free_t	free_list;
	int		i;
	xfs_mount_t	*mp;
	xfs_bmbt_irec_t	mval[SYMLINK_MAPS];
	int		nmaps;
	xfs_trans_t	*ntp;
	int		size;
	xfs_trans_t	*tp;

	tp = *tpp;
	mp = ip->i_mount;
	ASSERT(ip->i_d.di_size > XFS_IFORK_DSIZE(ip));
	/*
	 * We're freeing a symlink that has some
	 * blocks allocated to it.  Free the
	 * blocks here.  We know that we've got
	 * either 1 or 2 extents and that we can
	 * free them all in one bunmapi call.
	 */
	ASSERT(ip->i_d.di_nextents > 0 && ip->i_d.di_nextents <= 2);
	if (error = xfs_trans_reserve(tp, 0, XFS_ITRUNCATE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_ITRUNCATE_LOG_COUNT)) {
#pragma mips_frequency_hint NEVER
		ASSERT(XFS_FORCED_SHUTDOWN(mp));
		xfs_trans_cancel(tp, 0);
		*tpp = NULL;
		return error;
	}
	/*
	 * Lock the inode, fix the size, and join it to the transaction.
	 * Hold it so in the normal path, we still have it locked for
	 * the second transaction.  In the error paths we need it 
	 * held so the cancel won't rele it, see below.
	 */
	xfs_ilock(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
	size = (int)ip->i_d.di_size;
	ip->i_d.di_size = 0;
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	/*
	 * Find the block(s) so we can inval and unmap them.
	 */
	done = 0;
	XFS_BMAP_INIT(&free_list, &first_block);
	nmaps = sizeof(mval) / sizeof(mval[0]);
	if (error = xfs_bmapi(tp, ip, 0, XFS_B_TO_FSB(mp, size),
			XFS_BMAPI_METADATA, &first_block, 0, mval, &nmaps,
			&free_list))
		goto error0;
	/*
	 * Invalidate the block(s).
	 */
	for (i = 0; i < nmaps; i++) {
		bp = xfs_trans_get_buf(tp, mp->m_ddev_targp,
			XFS_FSB_TO_DADDR(mp, mval[i].br_startblock),
			XFS_FSB_TO_BB(mp, mval[i].br_blockcount), 0);
		xfs_trans_binval(tp, bp);
	}
	/*
	 * Unmap the dead block(s) to the free_list.
	 */
	if (error = xfs_bunmapi(tp, ip, 0, size, XFS_BMAPI_METADATA, nmaps,
			&first_block, &free_list, &done))
		goto error1;
	ASSERT(done);
	/*
	 * Commit the first transaction.  This logs the EFI and the inode.
	 */
	if (error = xfs_bmap_finish(&tp, &free_list, first_block, &committed))
		goto error1;
	/*
	 * The transaction must have been committed, since there were
	 * actually extents freed by xfs_bunmapi.  See xfs_bmap_finish.
	 * The new tp has the extent freeing and EFDs.
	 */
	ASSERT(committed);
	/*
	 * The first xact was committed, so add the inode to the new one.
	 * Mark it dirty so it will be logged and moved forward in the log as
	 * part of every commit.
	 */
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	/*
	 * Get a new, empty transaction to return to our caller.
	 */
	ntp = xfs_trans_dup(tp);
	/*
	 * Commit the transaction containing extent freeing and EFD's.
	 * If we get an error on the commit here or on the reserve below,
	 * we need to unlock the inode since the new transaction doesn't
	 * have the inode attached.
	 */
	error = xfs_trans_commit(tp, 0, NULL);
	tp = ntp;
	if (error) {
		ASSERT(XFS_FORCED_SHUTDOWN(mp));
		goto error0;
	}
	/*
	 * Remove the memory for extent descriptions (just bookkeeping).
	 */
	if (ip->i_df.if_bytes)
		xfs_idata_realloc(ip, -ip->i_df.if_bytes, XFS_DATA_FORK);
	ASSERT(ip->i_df.if_bytes == 0);
	/*
	 * Put an itruncate log reservation in the new transaction
	 * for our caller.
	 */
	if (error = xfs_trans_reserve(tp, 0, XFS_ITRUNCATE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_ITRUNCATE_LOG_COUNT)) {
		ASSERT(XFS_FORCED_SHUTDOWN(mp));
		goto error0;
	}
	/*
	 * Return with the inode locked but not joined to the transaction.
	 */
	*tpp = tp;
	return 0;

 error1:
	xfs_bmap_cancel(&free_list);
 error0:
#pragma mips_frequency_hint NEVER
	/*
	 * Have to come here with the inode locked and either
	 * (held and in the transaction) or (not in the transaction).
	 * If the inode isn't held then cancel would iput it, but
	 * that's wrong since this is inactive and the vnode ref
	 * count is 0 already.
	 * Cancel won't do anything to the inode if held, but it still
	 * needs to be locked until the cancel is done, if it was
	 * joined to the transaction.
	 */
	xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_ABORT);
	xfs_iunlock(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
	*tpp = NULL;
	return error;

}

STATIC int
xfs_inactive_symlink_local(
	xfs_inode_t	*ip,
	xfs_trans_t 	**tpp)
{
	int		error;
	ASSERT(ip->i_d.di_size <= XFS_IFORK_DSIZE(ip));
	/*
	 * We're freeing a symlink which fit into
	 * the inode.  Just free the memory used
	 * to hold the old symlink.
	 */
	error = xfs_trans_reserve(*tpp, 0,
				  XFS_ITRUNCATE_LOG_RES(ip->i_mount),
				  0, XFS_TRANS_PERM_LOG_RES,
				  XFS_ITRUNCATE_LOG_COUNT);
	
	if (error) {
		xfs_trans_cancel(*tpp, 0);
		*tpp = NULL;
		return (error);
	}
	xfs_ilock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	
	/*
	 * Zero length symlinks _can_ exist.
	 */
	if (ip->i_df.if_bytes > 0) {
		xfs_idata_realloc(ip, 
				  -(ip->i_df.if_bytes),
				  XFS_DATA_FORK);
		ASSERT(ip->i_df.if_bytes == 0);
	}
	return (0);
}

/*
 *
 */
STATIC int
xfs_inactive_attrs(
	xfs_inode_t 	*ip,
	xfs_trans_t 	**tpp,
	int		*commitflags)
{
	xfs_trans_t 	*tp;
	int		error;
	xfs_mount_t	*mp;

	ASSERT(ismrlocked(&ip->i_iolock, MR_UPDATE));
	tp = *tpp;
	mp = ip->i_mount;
	ASSERT(ip->i_d.di_forkoff != 0);
	xfs_trans_commit(tp, *commitflags, NULL);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	*commitflags = 0;

	error = xfs_attr_inactive(ip);	
	if (error) {
		*tpp = NULL;
		xfs_iunlock(ip, XFS_IOLOCK_EXCL);
		return (error); /* goto out*/
	}
	
	tp = xfs_trans_alloc(mp, XFS_TRANS_INACTIVE);
	error = xfs_trans_reserve(tp, 0,
				  XFS_IFREE_LOG_RES(mp),
				  0, 0,
				  XFS_DEFAULT_LOG_COUNT);
	if (error) {
		ASSERT(XFS_FORCED_SHUTDOWN(mp));
		xfs_trans_cancel(tp, 0);
		*tpp = NULL;
		xfs_iunlock(ip, XFS_IOLOCK_EXCL);
		return (error);
	}
	
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	xfs_idestroy_fork(ip, XFS_ATTR_FORK);
	
	ASSERT(ip->i_d.di_anextents == 0);

	*tpp = tp;
	return (0);
}


#endif	/* !SIM */

/*
 * xfs_inactive
 *
 * This is called when the vnode reference count for the vnode
 * goes to zero.  If the file has been unlinked, then it must
 * now be truncated.  Also, we clear all of the read-ahead state
 * kept for the inode here since the file is now closed.
 */
/*ARGSUSED*/
STATIC int
xfs_inactive(
	bhv_desc_t	*bdp,
	cred_t		*credp)
{
	xfs_inode_t	*ip;
			/* REFERENCED */
	vnode_t 	*vp;
#ifndef SIM
	xfs_trans_t	*tp;
	xfs_mount_t	*mp;
	int		error;
	int		commit_flags;
	int		truncate;
#endif

	vp = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_inactive", (inst_t *)__return_address);
	ip = XFS_BHVTOI(bdp);

	/*
	 * If the inode is already free, then there can be nothing
	 * to clean up here.
	 */
	if (ip->i_d.di_mode == 0) {
		ASSERT(ip->i_df.if_real_bytes == 0);
		ASSERT(ip->i_df.if_broot_bytes == 0);
		return VN_INACTIVE_CACHE;
	}

#ifndef SIM
	/*
	 * Only do a truncate if it's a regular file with
	 * some actual space in it.  It's OK to look at the
	 * inode's fields without the lock because we're the
	 * only one with a reference to the inode.
	 */
	truncate = ((ip->i_d.di_nlink == 0) &&
	    ((ip->i_d.di_size != 0) || (ip->i_d.di_nextents > 0)) &&
	    ((ip->i_d.di_mode & IFMT) == IFREG));

	mp = ip->i_mount;

	if (ip->i_d.di_nlink == 0 &&
	    DM_EVENT_ENABLED(vp->v_vfsp, ip, DM_EVENT_DESTROY)) {
		(void) dm_send_destroy_event(bdp, DM_RIGHT_NULL);
	}
	/*
	 * We don't mark the TEARDOWN flag, so
	 * xfs_inactive always returns VN_INACTIVE_CACHE.
	 */
	ASSERT(! (vp->v_flag & VINACTIVE_TEARDOWN));

	error = 0;
	if (ip->i_d.di_nlink != 0) {
		if ((((ip->i_d.di_mode & IFMT) == IFREG) &&
		     ((ip->i_d.di_size > 0) || (vp->v_pgcnt > 0)) &&
		     (ip->i_df.if_flags & XFS_IFEXTENTS))  &&
		    (!(ip->i_d.di_flags & XFS_DIFLAG_PREALLOC))) {
			if (error = xfs_inactive_free_eofblocks(mp, ip))
				return (VN_INACTIVE_CACHE);
		}
		goto out;
	}

	ASSERT(ip->i_d.di_nlink == 0);

	if (XFS_IS_QUOTA_ON(mp) && 
	    ip->i_ino != mp->m_sb.sb_uquotino && 
	    ip->i_ino != mp->m_sb.sb_pquotino) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0))
				return (VN_INACTIVE_CACHE);
		}
	}
	tp = xfs_trans_alloc(mp, XFS_TRANS_INACTIVE);
	if (truncate) {
		/*
		 * Do the xfs_itruncate_start() call before
		 * reserving any log space because itruncate_start
		 * will call into the buffer cache and we can't
		 * do that within a transaction.
		 */
		xfs_ilock(ip, XFS_IOLOCK_EXCL);

		/* Evict buffers on the xfsd's list. */		 
		xfs_xfsd_list_evict(bdp);

		xfs_itruncate_start(ip, XFS_ITRUNC_DEFINITE, 0);

		error = xfs_trans_reserve(tp, 0,
					  XFS_ITRUNCATE_LOG_RES(mp),
					  0, XFS_TRANS_PERM_LOG_RES,
					  XFS_ITRUNCATE_LOG_COUNT);
		if (error) {
			/* Don't call itruncate_cleanup */
			ASSERT(XFS_FORCED_SHUTDOWN(mp));
			xfs_trans_cancel(tp, 0);
			xfs_iunlock(ip, XFS_IOLOCK_EXCL);
			return (VN_INACTIVE_CACHE);
		}

		xfs_ilock(ip, XFS_ILOCK_EXCL);
		xfs_trans_ijoin(tp, ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);

		/*
		 * normally, we have to run xfs_itruncate_finish sync.
		 * But if filesystem is wsync and we're in the inactive
		 * path, then we know that nlink == 0, and that the
		 * xaction that made nlink == 0 is permanently committed
		 * since xfs_remove runs as a synchronous transaction.
		 */
		error = xfs_itruncate_finish(&tp, ip, 0, XFS_DATA_FORK,
				(!(mp->m_flags & XFS_MOUNT_WSYNC) ? 1 : 0));
		commit_flags = XFS_TRANS_RELEASE_LOG_RES;

		if (error) {
			xfs_trans_cancel(tp, commit_flags | XFS_TRANS_ABORT);
			xfs_iunlock(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
			return (VN_INACTIVE_CACHE);
		}
	} else if ((ip->i_d.di_mode & IFMT) == IFLNK) {
		
		/*
		 * If we get an error while cleaning up a
		 * symlink we bail out.
		 */
		error = (ip->i_d.di_size > XFS_IFORK_DSIZE(ip)) ?
			xfs_inactive_symlink_rmt(ip, &tp) :
		        xfs_inactive_symlink_local(ip, &tp);

		if (error) {
			ASSERT(tp == NULL);
			return (VN_INACTIVE_CACHE);
		}

		xfs_trans_ijoin(tp, ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);
		commit_flags = XFS_TRANS_RELEASE_LOG_RES;
		
	} else {
		error = xfs_trans_reserve(tp, 0,
					  XFS_IFREE_LOG_RES(mp),
					  0, 0,
					  XFS_DEFAULT_LOG_COUNT);
		if (error) {
			ASSERT(XFS_FORCED_SHUTDOWN(mp));
      			xfs_trans_cancel(tp, 0);
			return (VN_INACTIVE_CACHE);
		}
		
		xfs_ilock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
		xfs_trans_ijoin(tp, ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);
		commit_flags = 0;
	}
	
	/*
	 * If there are attributes associated with the file
	 * then blow them away now.  The code calls a routine
	 * that recursively deconstructs the attribute fork.
	 * We need to just commit the current transaction
	 * because we can't use it for xfs_attr_inactive().
	 */
	if (ip->i_d.di_anextents > 0) {
		error = xfs_inactive_attrs(ip, &tp, &commit_flags);
		/*
		 * If we got an error, the transaction is already
		 * cancelled, and the inode is unlocked. Just get out.
		 */
		 if (error)
			 return (VN_INACTIVE_CACHE);
	} else if (ip->i_afp) {
		xfs_idestroy_fork(ip, XFS_ATTR_FORK);
	}

	/*
	 * Free the inode.
	 */
	error = xfs_ifree(tp, ip);
	if (error) {
		/*
		 * If we fail to free the inode, shut down.  The cancel
		 * might do that, we need to make sure.  Otherwise the
		 * inode might be lost for a long time or forever.
		 */
		if (!XFS_FORCED_SHUTDOWN(tp->t_mountp))
			xfs_force_shutdown(tp->t_mountp, XFS_METADATA_IO_ERROR);
		xfs_trans_cancel(tp, commit_flags | XFS_TRANS_ABORT);
	} else {
		/*
		 * Credit the quota account(s). The inode is gone.
		 */
		if (XFS_IS_QUOTA_ON(tp->t_mountp)) 
			(void) xfs_trans_mod_dquot_byino(tp, ip,
							 XFS_TRANS_DQ_ICOUNT,
							 -1);   
		
		/*
		 * Just ignore errors at this point.  There is
		 * nothing we can do except to try to keep going.
		 */
		(void) xfs_trans_commit(tp, commit_flags, NULL);
	}
	/*	
	 * Release the dquots held by inode, if any.
	 */
	if (ip->i_udquot || ip->i_pdquot)
		xfs_qm_dqdettach_inode(ip);
	
	xfs_iunlock(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);

 out:
#endif /* !SIM */
	/*
	 * Clear all the inode's read-ahead state.  We need to take
	 * the lock here even though the inode is inactive, because
	 * someone might be in xfs_sync() where we play with
	 * inodes without taking references.  Of course, this is only
	 * necessary if it is a regular file since no other inodes
	 * use the read ahead state.  Also reset the read/write io
	 * sizes.  Like read-ahead, only regular files override the
	 * default read/write io sizes.
	 * Bug 516806: We do not need the ilock around the clearing
	 * of the readahead state since it is protected by its own
	 * mutex now. vfs_sync->buffer_cache->read path we also take 
	 * this mutex so this is not dangerous.
	 */
	if (vp->v_type == VREG) {
		XFS_INODE_CLEAR_READ_AHEAD(ip);
#ifndef SIM
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		ASSERT(mp->m_readio_log <= 0xff);
		ASSERT(mp->m_writeio_log <= 0xff);
		ip->i_readio_log = (uchar_t) mp->m_readio_log;
		ip->i_writeio_log = (uchar_t) mp->m_writeio_log;
		ip->i_max_io_log = (uchar_t) mp->m_writeio_log;
		ip->i_readio_blocks = mp->m_readio_blocks;
		ip->i_writeio_blocks = mp->m_writeio_blocks;
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
#endif /* !SIM */
	}

	return VN_INACTIVE_CACHE;
}


#ifndef SIM

/*
 * xfs_lookup
 *
 */
/*ARGSUSED*/
STATIC int
xfs_lookup(
	bhv_desc_t	*dir_bdp,
	char		*name,
	vnode_t		**vpp,
	pathname_t	*pnp,
	int		flags,
	vnode_t		*rdir, 
	cred_t		*credp)
{
	xfs_inode_t		*dp, *ip;
	struct vnode		*vp, *newvp;
	xfs_ino_t		e_inum;
	int			error;
	uint			lock_mode;
	uint			lookup_flags;
	uint			dir_unlocked;
	struct ncfastdata	fastdata;
	vnode_t 		*dir_vp;

	dir_vp = BHV_TO_VNODE(dir_bdp);
	vn_trace_entry(dir_vp, "xfs_lookup", (inst_t *)__return_address);

	/*
	 * If it's not a directory, fail the request.
	 */
	if (dir_vp->v_type != VDIR) {
		return XFS_ERROR(ENOTDIR);
	}

	dp = XFS_BHVTOI(dir_bdp);

	if (XFS_FORCED_SHUTDOWN(dp->i_mount))
		return XFS_ERROR(EIO);

	lock_mode = xfs_ilock_map_shared(dp);

	/*
	 * If the directory has been removed, then fail all lookups.
	 */
	if (dp->i_d.di_nlink == 0) {
		xfs_iunlock_map_shared(dp, lock_mode);
		return XFS_ERROR(ENOENT);
	}

	if (error = xfs_iaccess(dp, IEXEC, credp)) {
		xfs_iunlock_map_shared(dp, lock_mode);
		return error;
	}

	lookup_flags = DLF_IGET;
	if (lock_mode == XFS_ILOCK_SHARED) {
		lookup_flags |= DLF_LOCK_SHARED;
	}
	error = xfs_dir_lookup_int(NULL, dir_bdp, lookup_flags, name, pnp,
				  &e_inum, &ip, &fastdata, &dir_unlocked);
	if (error) {
		xfs_iunlock_map_shared(dp, lock_mode);
		return error;
	}

	vp = XFS_ITOV(ip);

	if (dir_unlocked) {
		/*
		 * If the directory had to be unlocked in the call,
		 * then its permissions may have changed.  Make sure
		 * that it is OK to give this inode back to the caller.
		 */
		if (error = xfs_iaccess(dp, IEXEC, credp)) {
			xfs_iunlock_map_shared(dp, lock_mode);
			VN_RELE(vp);
			return error;
		}
	}
	ITRACE(ip);

	xfs_iunlock_map_shared(dp, lock_mode);

	/*
	 * If vnode is a device return special vnode instead.
	 */
	if (ISVDEV(vp->v_type)) {
		newvp = spec_vp(vp, vp->v_rdev, vp->v_type, credp);
		VN_RELE(vp);
		if (newvp == NULL) {
			return XFS_ERROR(ENOSYS);
		}
		vp = newvp;
	}

	*vpp = vp;

	return 0;
}

#ifdef XFS_RW_TRACE
STATIC void
xfs_ctrunc_trace(
	int		tag,
	xfs_inode_t	*ip)
{
	if (ip->i_rwtrace == NULL) {
		return;
	}

	ktrace_enter(ip->i_rwtrace,
		     (void*)((long)tag),
		     (void*)ip, 
		     (void*)((long)private.p_cpuid),
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0,
		     (void*)0);
}
#endif /* XFS_RW_TRACE */

STATIC int xfs_create_exists(bhv_desc_t *, bhv_desc_t *, vattr_t *, int,
	int, vnode_t **, cred_t *);
STATIC int xfs_create_new(bhv_desc_t *, char *, vattr_t *, int, int,
	vnode_t **, cred_t *);
STATIC void xfs_create_broken(xfs_mount_t *, xfs_inode_t *, xfs_ino_t, uint);


/*
 * xfs_create (Just call exists or new based on vpp).
 *	new might still find that name exists because
 *	of two creates racing.
 */
STATIC int
xfs_create(
	bhv_desc_t	*dir_bdp,
	char		*name,
	vattr_t		*vap,
	int		flags,
	int		I_mode,
	vnode_t		**vpp,
	cred_t		*credp)
{
	vnode_t		*vp;
	bhv_desc_t	*bdp;


	if (vp = *vpp) {
		/*
		 * Convert the passed in vnode to an xfs inode.
		 * We need to get the behavior first.
		 */

		bdp = VNODE_TO_FIRST_BHV(vp);
		if (!BHV_IS_XFS(bdp)) {
			bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp),
				&xfs_vnodeops);
			/*
			 * If we don't have an XFS vnode, drop through to
			 * the old case.
			 */
			if (!bdp) {
				VN_RELE(vp);
				*vpp = NULL;
			}
		}
		if (bdp)
			return(xfs_create_exists(dir_bdp, bdp, vap, flags,
					I_mode, vpp, credp));
	}
	
	return(xfs_create_new(dir_bdp, name, vap, flags,
			I_mode, vpp, credp));
}

#define	XFS_CREATE_NEW_MAXTRIES	10000

/*
 * xfs_create_new (create a new file).
 *   It might still find name exists out there, though.
 *	But vpp, doens't point at a vnode.
 */
STATIC int
xfs_create_new(
	bhv_desc_t	*dir_bdp,
	char		*name,
	vattr_t		*vap,
	int		flags,
	int		I_mode,
	vnode_t		**vpp,
	cred_t		*credp)
{
	vnode_t 		*dir_vp;
	xfs_inode_t      	*dp, *ip;
        vnode_t		        *vp, *newvp;
	xfs_trans_t      	*tp;
	xfs_ino_t		e_inum;
        xfs_mount_t	        *mp;
	dev_t			rdev;
        int                     error;
        xfs_bmap_free_t 	free_list;
        xfs_fsblock_t   	first_block;
	boolean_t		dp_joined_to_trans;
	boolean_t		truncated;
	boolean_t		created = B_FALSE;
	int			dm_event_sent = 0;
	uint			cancel_flags;
	int			committed;
	uint			dir_unlocked;
	struct ncfastdata	fastdata;
	xfs_prid_t		prid;
	struct xfs_dquot	*udqp, *pdqp;
	uint			resblks;
	int			dm_di_mode;
	int			xfs_create_retries = 0;
	xfs_ino_t		e_inum_saved;	/* for retry trap code */
	int			namelen;

	dir_vp = BHV_TO_VNODE(dir_bdp);
        dp = XFS_BHVTOI(dir_bdp);
	vn_trace_entry(dir_vp, "xfs_create_new", (inst_t *)__return_address);
	dm_di_mode = vap->va_mode|VTTOIF(vap->va_type);
	namelen = strlen(name);
	if (namelen >= MAXNAMELEN)
		return XFS_ERROR(EINVAL);

#ifndef SIM
	if (DM_EVENT_ENABLED(dir_vp->v_vfsp, dp, DM_EVENT_CREATE)) {
		error = xfs_dm_send_create_event(dir_bdp, name,
				dm_di_mode, &dm_event_sent);
		if (error)
			return error;
	}
#endif
	
	/* Return through std_return after this point. */
	
	mp = dp->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	udqp = pdqp = NULL;
	if (vap->va_mask & AT_PROJID)
		prid = (xfs_prid_t)vap->va_projid;
	else 	
		prid = (xfs_prid_t)dfltprid;

	/*
	 * Make sure that we have allocated dquot(s) on disk.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		if (error = xfs_qm_vop_dqalloc(mp, dp, credp->cr_uid, prid,
					       XFS_QMOPT_QUOTALL,
					       &udqp, &pdqp)) 
			goto std_return;
	}

 try_again:
	ip = NULL;
	dp_joined_to_trans = B_FALSE;
	truncated = B_FALSE;
	
	tp = xfs_trans_alloc(mp, XFS_TRANS_CREATE);
	cancel_flags = XFS_TRANS_RELEASE_LOG_RES;
	resblks = XFS_CREATE_SPACE_RES(mp, namelen);
	/*
	 * Initially assume that the file does not exist and
	 * reserve the resources for that case.  If that is not
	 * the case we'll drop the one we have and get a more
	 * appropriate transaction later.
	 */
	error = xfs_trans_reserve(tp, resblks, XFS_CREATE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT);
	if (error == ENOSPC) {
		resblks = 0;
		error = xfs_trans_reserve(tp, 0, XFS_CREATE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT);
	}
	if (error) {
		cancel_flags = 0;
		dp = NULL;
		goto error_return;
	}

	xfs_ilock(dp, XFS_ILOCK_EXCL);

	/*
	 * If the directory has been removed, then fail all creates.
	 */
	if (dp->i_d.di_nlink == 0) {
		error = XFS_ERROR(ENOENT);
		goto error_return;
	}

	if (error = xfs_iaccess(dp, IEXEC, credp))
                goto error_return;

	/*
	 * At this point we cannot do an xfs_iget() of the entry named
	 * since we are already holding a log reservation and it could
	 * be in xfs_inactive waiting for a log reservation.  We'd just
	 * end up waiting forever for the inactive to complete.  Instead
	 * we just look to see if there is an entry with the name.  In
	 * the case where there is not, we didn't need the inode anyway.
	 * In the case where there is an entry, we'll get it later after
	 * dropping our transaction.
	 */
	error = xfs_dir_lookup_int(NULL, dir_bdp, DLF_NODNLC, name,
				   NULL, &e_inum, NULL, NULL, NULL);
	if (error && (error != ENOENT)) {
		goto error_return;
	}

	XFS_BMAP_INIT(&free_list, &first_block);

	if (error == ENOENT) {

		ASSERT(ip == NULL);

		if (error = xfs_iaccess(dp, IWRITE, credp)) {
			goto error_return;
		}

		/*
		 * XPG4 says create cannot allocate a file if the
		 * file size limit is set to 0.
		 */
		if (flags & VZFS) {
			error = XFS_ERROR(EFBIG);
			goto error_return;
		}
		
		/*
		 * Reserve disk quota and the inode.
		 */
		if (XFS_IS_QUOTA_ON(mp)) {
			if (xfs_trans_reserve_quota(tp, udqp, pdqp, resblks,
						    1, 0)) {
				error = EDQUOT;
				goto error_return;
			}
		}
		if (resblks == 0 &&
		    (error = XFS_DIR_CANENTER(mp, tp, dp, name, namelen)))
			goto error_return;
		rdev = (vap->va_mask & AT_RDEV) ? vap->va_rdev : NODEV;
		error = xfs_dir_ialloc(&tp, dp, 
				MAKEIMODE(vap->va_type,vap->va_mode), 1,
				rdev, credp, prid, resblks > 0,
				&ip, &committed);
		if (error) {
			if (error == ENOSPC)
				goto error_return;
			goto abort_return;
		}
		ITRACE(ip);

		/*
		 * At this point, we've gotten a newly allocated inode.
		 * It is locked (and joined to the transaction).
		 */

		ASSERT(ismrlocked (&ip->i_lock, MR_UPDATE));

		/*
		 * Now we join the directory inode to the transaction.
		 * We do not do it earlier because xfs_dir_ialloc
		 * might commit the previous transaction (and release
		 * all the locks).
		 */

		VN_HOLD(dir_vp);
		xfs_trans_ijoin(tp, dp, XFS_ILOCK_EXCL);
		dp_joined_to_trans = B_TRUE;

		error = XFS_DIR_CREATENAME(mp, tp, dp, name, namelen, ip->i_ino,
			&first_block, &free_list,
			resblks ? resblks - XFS_IALLOC_SPACE_RES(mp) : 0);
		if (error) {
			ASSERT(error != ENOSPC);
			goto abort_return;
		}
		xfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
		xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);

		/*
		 * If this is a synchronous mount, make sure that the
		 * create transaction goes to disk before returning to
		 * the user.
		 */
		if (mp->m_flags & XFS_MOUNT_WSYNC) {
			xfs_trans_set_sync(tp);
		}

		dp->i_gen++;
		/*
		 * Since we used the DLF_NODNLC flag above, we have to
		 * use the slower dnlc_enter() function here rather than
		 * dnlc_enter_fast().
		 */
		dnlc_enter(dir_vp, name, XFS_ITOBHV(ip), NOCRED);
		
		/*
		 * Attach the dquot(s) to the inodes and modify them incore.
		 * These ids of the inode couldn't have changed since the new
		 * inode has been locked ever since it was created.
		 */
		if (XFS_IS_QUOTA_ON(mp)) 
			xfs_qm_vop_dqattach_and_dqmod_newinode(tp, ip, udqp, 
							       pdqp);
		created = B_TRUE;
	} else {
		e_inum_saved = e_inum;

		/*
		 * The file already exists, so we're in the wrong
		 * transaction for this operation.  Cancel the old
		 * transaction and start a new one.  We have to drop
		 * our locks in doing this. But, we don't care
		 * if the directory changes. We have already checked
		 * if the dir exists and is EXEC by the user.
		 * After all, we already have the vnode held.
		 *
		 * All we need to do is truncate it.
		 */
		xfs_trans_cancel(tp, cancel_flags);
		tp = NULL;

		/*
		 * Now that we've dropped our log reservation, get a
		 * reference to the inode we are re-creating.  If we
		 * have to drop the directory lock in the call and
		 * the entry is removed, then just start over.
		 */
		error = xfs_dir_lookup_int(NULL, dir_bdp, DLF_IGET, name,
					   NULL, &e_inum, &ip, &fastdata,
					   &dir_unlocked);
		if (error) {
			if (error == ENOENT) {
				if (++xfs_create_retries >
					XFS_CREATE_NEW_MAXTRIES) {
					xfs_create_broken(mp, dp,
						e_inum_saved, dir_unlocked);
					error = XFS_ERROR(EFSCORRUPTED);
					goto error_return;
				}

				ASSERT(dir_unlocked);
				xfs_iunlock(dp, XFS_ILOCK_EXCL);
				goto try_again;
			}
			goto error_return;
		}
		ITRACE(ip);

		xfs_iunlock(dp, XFS_ILOCK_EXCL);
		dp = NULL;

		/*
		 * Done with directory. Don't care about it
		 * anymore.
		 */

		/*
		 * Since we're at a good, clean point, check for any
		 * obvious problems and get out if they occur.
		 */
		vp = XFS_ITOV(ip);
		if (!error) {
			if (flags & VEXCL) {
				error = XFS_ERROR(EEXIST);
			} else if (vp->v_type == VDIR) {
				error = XFS_ERROR(EISDIR);
#ifndef SIM
			} else if (vp->v_type == VREG && (vap->va_mask & AT_SIZE) &&
				DM_EVENT_ENABLED (vp->v_vfsp, ip, DM_EVENT_TRUNCATE)) {
					error = xfs_dm_send_data_event (DM_EVENT_TRUNCATE,
						XFS_ITOBHV(ip), vap->va_size, 0, 0, NULL);
#endif
			}
		}

		if (!error && XFS_IS_QUOTA_ON(mp)) {
			if (XFS_NOT_DQATTACHED(mp, ip)) 
				error = xfs_qm_dqattach(ip, 0);
		}
		
		if (error) {
			IRELE(ip);
			goto error_return;
		}

		/*
		 * We need to do the xfs_itruncate_start call before
		 * reserving any log space in the transaction.
		 */
		xfs_ilock(ip, XFS_IOLOCK_EXCL);
		xfs_ctrunc_trace(XFS_CTRUNC1, ip);
		if ((vp->v_type == VREG) &&
		    (vap->va_mask & AT_SIZE) &&
		    ((ip->i_d.di_size != 0) || (ip->i_d.di_nextents != 0))) {
			xfs_itruncate_start(ip, XFS_ITRUNC_MAYBE, 0);
		}

		tp = xfs_trans_alloc(mp, XFS_TRANS_CREATE_TRUNC);
		if (error = xfs_trans_reserve(tp, 0,
					      XFS_ITRUNCATE_LOG_RES(mp), 0,
					      XFS_TRANS_PERM_LOG_RES,
					      XFS_ITRUNCATE_LOG_COUNT)) {
			xfs_iunlock(ip, XFS_IOLOCK_EXCL);
			IRELE(ip);
			cancel_flags = 0;
			xfs_ctrunc_trace(XFS_CTRUNC2, ip);
			goto error_return;
		}
		
		/*
		 * Now lock inode.
		 */
		xfs_ilock(ip, XFS_ILOCK_EXCL);
			
		if (I_mode) {
			error = xfs_iaccess(ip, I_mode, credp);
		}
			
		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);

		if (error) {
			xfs_ctrunc_trace(XFS_CTRUNC4, ip);
			goto error_return;
		}

		if (vp->v_type == VREG && (vap->va_mask & AT_SIZE)) {
			/*
			 * Truncate the file.  The timestamps must
			 * be updated whether the file is changed
			 * or not.
			 */
			ASSERT(vap->va_size == 0);
			if ((ip->i_d.di_size > 0) || (ip->i_d.di_nextents)) {
				xfs_ctrunc_trace(XFS_CTRUNC5, ip);
				xfs_trans_ihold(tp, ip);
				/*
				 * always signal sync xaction.  We're
				 * truncating an existing file so the
				 * xaction must be sync regardless
				 * of whether the filesystem is wsync
				 * to make the truncate persistent
				 * in the face of a crash.
				 */
				error = xfs_itruncate_finish(&tp, ip,
							     (xfs_fsize_t)0,
							     XFS_DATA_FORK, 1);
				if (error) {
					ASSERT(ip->i_transp == tp);
					xfs_trans_ihold_release(tp, ip);
					goto abort_return;
				}
				truncated = B_TRUE;
				xfs_ichgtime(ip,
					XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
				xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
			} else {
				xfs_ichgtime(ip,
					XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
			}
#ifdef CELL
			VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_TRUNCATED, 0);
#endif

		}
		xfs_ctrunc_trace(XFS_CTRUNC6, ip);
	}


	/*
	 * xfs_trans_commit normally decrements the vnode ref count
	 * when it unlocks the inode. Since we want to return the 
	 * vnode to the caller, we bump the vnode ref count now.
	 */
	IHOLD(ip);
	vp = XFS_ITOV(ip);

	error = xfs_bmap_finish(&tp, &free_list, first_block, &committed);
	if (error) {
		xfs_bmap_cancel(&free_list);
		if (truncated) {
			/*
			 * If we truncated the file, then the inode will
			 * have been held within the previous transaction
			 * and must be unlocked now.
			 */
			xfs_iunlock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
			ASSERT(vp->v_count >= 2);
			IRELE(ip);
		}		
		goto abort_rele;
	}

	error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	if (truncated) {
		/*
		 * If we truncated the file, then the inode will
		 * have been held within the transaction and must
		 * be unlocked now.
		 */
		xfs_iunlock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
		ASSERT(vp->v_count >= 2);
		IRELE(ip);
	}
	if (error) {
		IRELE(ip);
		tp = NULL;
		goto error_return;
	}

	if (udqp)
		xfs_qm_dqrele(udqp);
	if (pdqp)
		xfs_qm_dqrele(pdqp);

        /*
         * If vnode is a device, return special vnode instead.
         */
        if (ISVDEV(vp->v_type)) {
                newvp = spec_vp(vp, vp->v_rdev, vp->v_type, credp);
                VN_RELE(vp);
                if (newvp == NULL) {
			error = XFS_ERROR(ENOSYS);
			goto std_return;
		}
                vp = newvp;
        }

        *vpp = vp;

	/* Fallthrough to std_return with error = 0  */

std_return:
#ifndef SIM
	if ( (created || (error != 0 && dm_event_sent != 0)) &&
			DM_EVENT_ENABLED(dir_vp->v_vfsp, XFS_BHVTOI(dir_bdp), 
							DM_EVENT_POSTCREATE)) {
		(void) dm_send_namesp_event(DM_EVENT_POSTCREATE,
			dir_bdp, DM_RIGHT_NULL,
			created ? vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &xfs_vnodeops):NULL,
			DM_RIGHT_NULL, name, NULL,
			dm_di_mode, error, 0);
	}
#endif
	return error;

 abort_return:
	cancel_flags |= XFS_TRANS_ABORT;
	/* FALLTHROUGH */
 error_return:

	if (tp != NULL)
		xfs_trans_cancel(tp, cancel_flags);

	if (!dp_joined_to_trans && (dp != NULL))
		xfs_iunlock(dp, XFS_ILOCK_EXCL);
	if (udqp)
		xfs_qm_dqrele(udqp);
	if (pdqp)
		xfs_qm_dqrele(pdqp);

	goto std_return;

 abort_rele:
	/*
	 * Wait until after the current transaction is aborted to
	 * release the inode.  This prevents recursive transactions
	 * and deadlocks from xfs_inactive.
	 */
	cancel_flags |= XFS_TRANS_ABORT;
	xfs_trans_cancel(tp, cancel_flags);
	IRELE(ip);
	
	if (udqp)
		xfs_qm_dqrele(udqp);
	if (pdqp)
		xfs_qm_dqrele(pdqp);

	goto std_return;
}


/*
 * xfs_create_broken is a trap routine to isolate the cause of a infinite
 *	loop condition reported in IRIX 6.4 by PV 522864. If no occurances
 *	of this error recur (that is, the trap code isn't hit), this routine
 *	should be removed in future releases.
 */
/* ARGSUSED */
STATIC void
xfs_create_broken(
	xfs_mount_t *mp,
	xfs_inode_t *dp,
	xfs_ino_t e_inum_saved,
	uint dir_unlocked)
{
	cmn_err(CE_WARN,
		"xfs_create looping, dir ino 0x%llx, ino 0x%llx, %s\n",
		dp->i_ino, e_inum_saved, mp->m_fsname);
#ifdef SIM
	abort();
#else
#ifdef	DEBUG
	debug_stop_all_cpus((void *)-1LL);
	debug("xfs");
#endif	/* DEBUG */
#endif
}


/*
 * xfs_create_exists (create on top of an existing vnode).
 *   *vpp has vnode found by a lookup.
 *	But *vpp might go away.
 */
/* ARGSUSED */
STATIC int
xfs_create_exists(
	bhv_desc_t	*dir_bdp,
	bhv_desc_t	*bdp,
	vattr_t		*vap,
	int		flags,
	int		I_mode,
	vnode_t		**vpp,
	cred_t		*credp)
{
#ifdef VNODE_TRACING
	vnode_t 		*dir_vp = BHV_TO_VNODE(dir_bdp);
#endif
	xfs_inode_t      	*dp, *ip;
        vnode_t		        *vp, *newvp;
	xfs_trans_t      	*tp = NULL;
        xfs_mount_t	        *mp;
        int                     error = 0;
	uint			cancel_flags;

        dp = XFS_BHVTOI(dir_bdp);
	vp = *vpp;

	vn_trace_entry(dir_vp, "xfs_create_exists", (inst_t *)__return_address);

	mp = dp->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp)) {
		error = XFS_ERROR(EIO);
		goto error_out;
	}

	ip = XFS_BHVTOI(bdp);
	ITRACE(ip);

	/*
	 * Since we're at a good, clean point, check for any
	 * obvious problems and get out if they occur.
	 */

	ASSERT(!(flags & VEXCL));
	if (vp->v_type == VDIR) {
		error = XFS_ERROR(EISDIR);
	}

	if (!error && XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) 
			error = xfs_qm_dqattach(ip, 0);
	}

	/*
	 * Check permissions if needed.
	 */
	if (I_mode && !error) {
		/*
		 * Now lock inode and check permissions.
		 */
			
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		error = xfs_iaccess(ip, I_mode, credp);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
	}

	if (error)
		goto error_return;

	/*
	 * All that is left is a possible truncate.
	 * Truncates are only done on VREG's and if the user
	 * requested it with AT_SIZE.
	 */

	if (vp->v_type == VREG && (vap->va_mask & AT_SIZE)) {

#ifndef SIM
		if (DM_EVENT_ENABLED (vp->v_vfsp, ip, DM_EVENT_TRUNCATE)) {

			error = xfs_dm_send_data_event (DM_EVENT_TRUNCATE,
				bdp, vap->va_size, 0, 0, NULL);
			if (error)
				goto error_return;
		}
#endif
		/*
		 * We need to do the xfs_itruncate_start call before
		 * reserving any log space in the transaction.
		 */
		xfs_ilock(ip, XFS_IOLOCK_EXCL);
		xfs_ctrunc_trace(XFS_CTRUNC1, ip);

		/*
		 * Truncate the file.  The timestamps must
		 * be updated whether the file is changed
		 * or not. But the timestamps don't need
		 * a transaction.
		 */

		if ((ip->i_d.di_size != 0) || (ip->i_d.di_nextents != 0)) {
			ASSERT(vap->va_size == 0);
			xfs_itruncate_start(ip, XFS_ITRUNC_DEFINITE, 0);

			tp = xfs_trans_alloc(mp, XFS_TRANS_SETATTR_SIZE);
			if (error = xfs_trans_reserve(tp, 0,
						      XFS_ITRUNCATE_LOG_RES(mp), 0,
						      XFS_TRANS_PERM_LOG_RES,
						      XFS_ITRUNCATE_LOG_COUNT)) {
				xfs_iunlock(ip, XFS_IOLOCK_EXCL);
				cancel_flags = 0;
				xfs_ctrunc_trace(XFS_CTRUNC2, ip);
				goto error_return;
			}
			
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
			xfs_ctrunc_trace(XFS_CTRUNC5, ip);
			xfs_trans_ihold(tp, ip);
			/*
			 * always signal sync xaction.  We're
			 * truncating an existing file so the
			 * xaction must be sync regardless
			 * of whether the filesystem is wsync
			 * to make the truncate persistent
			 * in the face of a crash.
			 */
			error = xfs_itruncate_finish(&tp, ip,
						     (xfs_fsize_t)0,
						     XFS_DATA_FORK, 1);
			if (error) {
				ASSERT(ip->i_transp == tp);
				xfs_trans_ihold_release(tp, ip);
				goto abort_return;
			}
			xfs_ichgtime(ip,
				XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
			xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

			/*
			 * xfs_trans_commit normally decrements the vnode
			 * ref count when it unlocks the inode. Since we
			 * want to return the vnode to the caller, we bump
			 * the vnode ref count now.
			 */

			IHOLD(ip);
			error = xfs_trans_commit(tp,
					XFS_TRANS_RELEASE_LOG_RES,
					NULL);
			/*
			 * If we truncated the file, then the inode will
			 * have been held within the transaction and must
			 * be unlocked now.
			 */
			xfs_iunlock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
			ASSERT(vp->v_count >= 2);
			IRELE(ip);
			if (error) { /* commit failed. */
				tp = NULL;
				goto error_return;
			}
		} else {
			xfs_ichgtime(ip,
				XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
			xfs_iunlock(ip, XFS_IOLOCK_EXCL);
		}
#ifdef CELL
		VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_TRUNCATED, 0);
#endif
		xfs_ctrunc_trace(XFS_CTRUNC6, ip);

	}

	ASSERT(*vpp == vp);
	/*
	 * If vnode is a device, return special vnode instead.
	 */
	if (ISVDEV(vp->v_type)) {
		newvp = spec_vp(vp, vp->v_rdev, vp->v_type, credp);

		VN_RELE(vp);
		if (newvp == NULL)
			return XFS_ERROR(ENOSYS);

		vp = newvp;
	}

	*vpp = vp;
	return 0;

 abort_return:
	cancel_flags |= XFS_TRANS_ABORT;
	/* FALLTHROUGH */
 error_return:
	if (tp != NULL)
		xfs_trans_cancel(tp, cancel_flags);

 error_out:
	ASSERT(*vpp);
	VN_RELE(*vpp);
	*vpp = NULL;
	return error;
}


/*
 * xfs_get_dir_entry is used to get a reference to an inode given
 * its parent directory inode and the name of the file.  It does
 * not lock the child inode, and it unlocks the directory before
 * returning.  The directory's generation number is returned for
 * use by a later call to xfs_lock_dir_and_entry.
 */
STATIC int
xfs_get_dir_entry(
	xfs_inode_t	*dp,
	char		*name,
	xfs_inode_t	**ipp,	/* inode of entry 'name' */
	int		*dir_generationp)
{
	xfs_inode_t		*ip;
	xfs_ino_t		e_inum;
	int			error;
	uint			dir_unlocked;
	struct ncfastdata	fastdata;

        xfs_ilock(dp, XFS_ILOCK_EXCL);

	/*
	 * If the link count on the directory is 0, there are no
	 * entries to look for.
	 */
	if (dp->i_d.di_nlink == 0) {
		xfs_iunlock(dp, XFS_ILOCK_EXCL);
		*ipp = NULL;
		return XFS_ERROR(ENOENT);
	}

	error = xfs_dir_lookup_int(NULL, XFS_ITOBHV(dp), DLF_IGET, name, 
				   NULL, &e_inum, &ip, &fastdata,
				   &dir_unlocked);
        if (error) {
                xfs_iunlock(dp, XFS_ILOCK_EXCL);
		*ipp = NULL;
                return error;
        }

        ASSERT((e_inum != 0) && ip);
	ITRACE(ip);

	*dir_generationp = dp->i_gen;
	xfs_iunlock(dp, XFS_ILOCK_EXCL);

	*ipp = ip;
	return 0;
}

#ifdef DEBUG

/*
 * Some counters to see if (and how often) we are hitting some deadlock
 * prevention code paths.
 */

int xfs_rm_locks;
int xfs_rm_lock_delays;
int xfs_rm_attempts;
#endif

/*
 * The following routine will lock the inodes associated with the
 * directory and the named entry in the directory. The locks are
 * acquired in increasing inode number.
 * 
 * If the entry is "..", then only the directory is locked. The
 * vnode ref count will still include that from the .. entry in
 * this case.
 *
 * The inode passed in will have been looked up using xfs_get_dir_entry().
 * Since that lookup the directory lock will have been dropped, so
 * we need to validate that the inode given is still pointed to by the
 * directory.  We use the directory inode in memory generation count
 * as an optimization to tell if a new lookup is necessary.  If the
 * directory no longer points to the given inode with the given name,
 * then we drop the directory lock, set the entry_changed parameter to 1,
 * and return.  It is up to the caller to drop the reference to the inode.
 *
 * There is a dealock we need to worry about. If the locked directory is
 * in the AIL, it might be blocking up the log. The next inode we lock
 * could be already locked by another thread waiting for log space (e.g
 * a permanent log reservation with a long running transaction (see
 * xfs_itruncate_finish)). To solve this, we must check if the directory
 * is in the ail and use lock_nowait. If we can't lock, we need to
 * drop the inode lock on the directory and try again. xfs_iunlock will
 * potentially push the tail if we were holding up the log.
 */
STATIC int
xfs_lock_dir_and_entry(
	xfs_inode_t	*dp,
	char		*name,
	xfs_inode_t	*ip,	/* inode of entry 'name' */
	int		dir_generation,
	int		*entry_changed)		       
{
	int 			attempts;
	xfs_ino_t		e_inum;
	int			error;
	int			new_dir_gen;
	struct ncfastdata	fastdata;
	xfs_inode_t		*ips[2];
	xfs_log_item_t		*lp;

#ifdef DEBUG
	xfs_rm_locks++;
#endif
	attempts = 0;

again:
	*entry_changed = 0;
        xfs_ilock(dp, XFS_ILOCK_EXCL);

	if (dp->i_gen != dir_generation) {
		/*
		 * If the link count on the directory is 0, there are no
		 * entries to lock.
		 */
		if (dp->i_d.di_nlink == 0) {
			xfs_iunlock(dp, XFS_ILOCK_EXCL);
			return XFS_ERROR(ENOENT);
		}
		/*
		 * The directory has changed somehow, so do the lookup
		 * for the entry again.  If it is changed we'll have to
		 * give up and return to our caller.  We can't allow this
		 * lookup to use the dnlc, because that could call vn_get
		 * and we could deadlock if the vnode we're after is in
		 * the inactive routine waiting for a log reservation that
		 * we hold.
		 */
		error = xfs_dir_lookup_int(NULL, XFS_ITOBHV(dp), DLF_NODNLC,
				name, NULL, &e_inum, NULL, &fastdata, NULL);
		if (error) {
			xfs_iunlock(dp, XFS_ILOCK_EXCL);
			return error;
		}

		/*
		 * The entry with the given name has changed since the
		 * call to xfs_get_dir_entry().  Just return with
		 * *entry_changed set to 1 so the caller can deal with it.
		 */
		ASSERT(e_inum != 0);
		if (e_inum != ip->i_ino) {
			xfs_iunlock(dp, XFS_ILOCK_EXCL);
			*entry_changed = 1;
			return 0;
		}
	} else {
		e_inum = ip->i_ino;
	}

	ITRACE(ip);

        /*
         * We want to lock in increasing inum. Since we've already
         * acquired the lock on the directory, we may need to release
         * if if the inum of the entry turns out to be less.
         */
	if (e_inum > dp->i_ino) {
		/*
		 * We are already in the right order, so just 
		 * lock on the inode of the entry.
		 * We need to use nowait if dp is in the AIL.
		 */

		lp = (xfs_log_item_t *)dp->i_itemp;
		if (lp && (lp->li_flags & XFS_LI_IN_AIL)) {
			if (!xfs_ilock_nowait(ip, XFS_ILOCK_EXCL)) {
				attempts++;
#ifdef DEBUG
				xfs_rm_attempts++;
#endif

				/* 
				 * Unlock dp and try again.
				 * xfs_iunlock will try to push the tail
				 * if the inode is in the AIL.
				 */

				xfs_iunlock(dp, XFS_ILOCK_EXCL);

				if ((attempts % 5) == 0) {
					delay(1); /* Don't just spin the CPU */
#ifdef DEBUG
					xfs_rm_lock_delays++;
#endif
				}
				goto again;
			}
		} else {
			xfs_ilock(ip, XFS_ILOCK_EXCL);
		}
	} else if (e_inum < dp->i_ino) {
		new_dir_gen = dp->i_gen;
                xfs_iunlock(dp, XFS_ILOCK_EXCL);

		ips[0] = ip;
		ips[1] = dp;
		xfs_lock_inodes(ips, 2, 0, XFS_ILOCK_EXCL);

                /*
                 * Make sure that things are still consistent during
                 * the period we dropped the directory lock.
                 * Do a new lookup if directory was changed.
                 */
                if (dp->i_gen != new_dir_gen) {
			/*
			 * If the directory has been unlinked, we're
			 * not going to find our entry there anymore.
			 */
			if (dp->i_d.di_nlink == 0) {
				xfs_iunlock(dp, XFS_ILOCK_EXCL);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				return XFS_ERROR(ENOENT);
			}
			/*
			 * The directory has changed somehow, so do the
			 * lookup for the entry again.  If it is changed
			 * we'll have to give up and return to our caller.
			 * We can't allow this lookup to use the dnlc,
			 * because that could call vn_get and we could
			 * deadlock if the vnode we're after is in
			 * the inactive routine waiting for a log
			 * reservation that we hold.
			 */
			error = xfs_dir_lookup_int(NULL, XFS_ITOBHV(dp),
				   DLF_NODNLC, name, NULL, &e_inum, NULL,
				   &fastdata, NULL);

                        if (error) {
				xfs_iunlock(dp, XFS_ILOCK_EXCL);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				return error;
			}

			if (e_inum != ip->i_ino) {
				xfs_iunlock(dp, XFS_ILOCK_EXCL);
                                xfs_iunlock(ip, XFS_ILOCK_EXCL);
				*entry_changed = 1;
				return 0;
                        }
                }
        }
	/* else  e_inum == dp->i_ino */
	/*     This can happen if we're asked to lock /x/..
	 *     the entry is "..", which is also the parent directory.
	 */

	return 0;
}

#ifdef DEBUG
int xfs_locked_n;
int xfs_small_retries;
int xfs_middle_retries;
int xfs_lots_retries;
int xfs_lock_delays;
#endif

/*
 * The following routine will lock n inodes in exclusive mode.
 * We assume the caller calls us with the inodes in i_ino order.
 *
 * We need to detect deadlock where an inode that we lock
 * is in the AIL and we start waiting for another inode that is locked
 * by a thread in a long running transaction (such as truncate). This can
 * result in deadlock since the long running trans might need to wait
 * for the inode we just locked in order to push the tail and free space
 * in the log.
 */
void
xfs_lock_inodes (xfs_inode_t **ips,
	int inodes,
	int first_locked,
	uint lock_mode)
{
	int attempts = 0, i, j, try_lock;
	xfs_log_item_t	*lp;

	ASSERT(ips && (inodes >= 2)); /* we need at least two */
	
	if (first_locked) {
		try_lock = 1;
		i = 1;
	} else {
		try_lock = 0;
		i = 0;
	}

again:
	for (; i < inodes; i++) {
		ASSERT(ips[i]);

		if (i && (ips[i] == ips[i-1]))	/* Already locked */
			continue;

		/*
		 * If try_lock is not set yet, make sure all locked inodes
		 * are not in the AIL.
		 * If any are, set try_lock to be used later.
		 */

		if (!try_lock) {
			for (j = (i - 1); j >= 0 && !try_lock; j--) {
				lp = (xfs_log_item_t *)ips[j]->i_itemp;
				if (lp && (lp->li_flags & XFS_LI_IN_AIL)) {
					try_lock++;
				}
			}
		}

		/*
		 * If any of the previous locks we have locked is in the AIL,
		 * we must TRY to get the second and subsequent locks. If
		 * we can't get any, we must release all we have
		 * and try again.
		 */

		if (try_lock) {
			/* try_lock must be 0 if i is 0. */
			/*
			 * try_lock means we have an inode locked
			 * that is in the AIL.
			 */
			ASSERT(i != 0);
			if (!xfs_ilock_nowait(ips[i], lock_mode)) {
				attempts++;

				/* 
				 * Unlock all previous guys and try again.
				 * xfs_iunlock will try to push the tail
				 * if the inode is in the AIL.
				 */

				for(j = i - 1; j >= 0; j--) {

					/*
					 * Check to see if we've already
					 * unlocked this one.
					 * Not the first one going back,
					 * and the inode ptr is the same.
					 */
					if ((j != (i - 1)) && ips[j] ==
								ips[j+1])
						continue;

					xfs_iunlock(ips[j], lock_mode);
				}

				if ((attempts % 5) == 0) {
					delay(1); /* Don't just spin the CPU */
#ifdef DEBUG
					xfs_lock_delays++;
#endif
				}
				i = 0;
				try_lock = 0;
				goto again;
			}
		} else {
			xfs_ilock(ips[i], lock_mode);
		}
	}

#ifdef DEBUG
	if (attempts) {
		if (attempts < 5) xfs_small_retries++;
		else if (attempts < 100) xfs_middle_retries++;
		else xfs_lots_retries++;
	} else {
		xfs_locked_n++;
	}
#endif
}

#ifdef	DEBUG
#define	REMOVE_DEBUG_TRACE(x)	{remove_which_error_return = (x);}
int remove_which_error_return = 0;
#else /* ! DEBUG */
#define	REMOVE_DEBUG_TRACE(x)
#endif	/* ! DEBUG */

/*
 * xfs_remove
 *
 */
STATIC int
xfs_remove(
	bhv_desc_t	*dir_bdp,
	char		*name,
	cred_t		*credp)
{
	vnode_t 		*dir_vp;
        xfs_inode_t             *dp, *ip;
        xfs_trans_t             *tp = NULL;
	xfs_mount_t		*mp;
        int                     error = 0;
        xfs_bmap_free_t         free_list;
        xfs_fsblock_t           first_block;
	int			cancel_flags;
	int			committed;
	int			dir_generation;
	int			entry_changed;
	int			dm_di_mode = 0;
	uint			resblks;
	int			namelen;

	dir_vp = BHV_TO_VNODE(dir_bdp);
	vn_trace_entry(dir_vp, "xfs_remove", (inst_t *)__return_address);
	dp = XFS_BHVTOI(dir_bdp);
	mp = dp->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	namelen = strlen(name);
	if (namelen >= MAXNAMELEN)
		return XFS_ERROR(EINVAL);
#ifndef SIM
	if (DM_EVENT_ENABLED(dir_vp->v_vfsp, dp, DM_EVENT_REMOVE)) {
		error = dm_send_namesp_event(DM_EVENT_REMOVE, dir_bdp, DM_RIGHT_NULL,
					NULL, DM_RIGHT_NULL,
					name, NULL, 0, 0, 0);
		if (error)
			return error;
	}
#endif

	/* From this point on, return through std_return */

 retry:
        ip = NULL;

	/*
	 * We need to get a reference to ip before we get our log
	 * reservation. The reason for this is that we cannot call
	 * xfs_iget for an inode for which we do not have a reference
	 * once we've acquired a log reservation. This is because the
	 * inode we are trying to get might be in xfs_inactive going
	 * for a log reservation. Since we'll have to wait for the
	 * inactive code to complete before returning from xfs_iget,
	 * we need to make sure that we don't have log space reserved
	 * when we call xfs_iget.  Instead we get an unlocked referece
	 * to the inode before getting our log reservation.
	 */
	error = xfs_get_dir_entry(dp, name, &ip, &dir_generation);
	if (error) {
		REMOVE_DEBUG_TRACE(__LINE__);
		goto std_return;
	}
	dm_di_mode = ip->i_d.di_mode;
	ITRACE(ip);
	 
	if (XFS_IS_QUOTA_ON(mp)) {
		ASSERT(! error);
		if (XFS_NOT_DQATTACHED(mp, dp)) 
			error = xfs_qm_dqattach(dp, 0);
		if (!error && dp != ip && XFS_NOT_DQATTACHED(mp, ip)) 
			error = xfs_qm_dqattach(ip, 0);
		if (error) {
			REMOVE_DEBUG_TRACE(__LINE__);
			IRELE(ip);
			goto std_return;
		}
	}

	tp = xfs_trans_alloc(mp, XFS_TRANS_REMOVE);
	cancel_flags = XFS_TRANS_RELEASE_LOG_RES;
	/*
	 * We try to get the real space reservation first,
	 * allowing for directory btree deletion(s) implying 
	 * possible bmap insert(s).  If we can't get the space 
	 * reservation then we use 0 instead, and avoid the bmap
	 * btree insert(s) in the directory code by, if the bmap
	 * insert tries to happen, instead trimming the LAST
	 * block from the directory.
	 */
	resblks = XFS_REMOVE_SPACE_RES(mp);
        error = xfs_trans_reserve(tp, resblks, XFS_REMOVE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error == ENOSPC) {
		resblks = 0;
		error = xfs_trans_reserve(tp, 0, XFS_REMOVE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	}
	if (error) {
		ASSERT(error != ENOSPC);
		REMOVE_DEBUG_TRACE(__LINE__);
		xfs_trans_cancel(tp, 0);
		IRELE(ip);
		return error;
	}

	error = xfs_lock_dir_and_entry(dp, name, ip, dir_generation,
				       &entry_changed);
	if (error) {
		REMOVE_DEBUG_TRACE(__LINE__);
		xfs_trans_cancel(tp, cancel_flags);
		IRELE(ip);
		goto std_return;
	}

	/*
	 * If the inode we found in the first pass is no longer
	 * the entry with the given name, then drop our transaction and
	 * inode reference and start over.
	 */
	if (entry_changed) {
		xfs_trans_cancel(tp, cancel_flags);
		IRELE(ip);
		goto retry;
	}

	/*
	 * At this point, we've gotten both the directory and the entry
	 * inodes locked.
	 */
	xfs_trans_ijoin(tp, dp, XFS_ILOCK_EXCL);
	if (dp != ip) {
		/*
		 * Increment vnode ref count only in this case since
		 * there's an extra vnode reference in the case where
		 * dp == ip.
		 */
		IHOLD(dp);
		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	}
 
	if (error = xfs_iaccess(dp, IEXEC | IWRITE, credp)) {
		REMOVE_DEBUG_TRACE(__LINE__);
		goto error_return;
	}
	if (error = _MAC_XFS_IACCESS(ip, MACWRITE, credp)) {
		REMOVE_DEBUG_TRACE(__LINE__);
		goto error_return;
	}
	if (error = xfs_stickytest(dp, ip, credp)) {
		REMOVE_DEBUG_TRACE(__LINE__);
		goto error_return;
	}

	if (XFS_ITOV(ip)->v_vfsmountedhere) {
		error = XFS_ERROR(EBUSY);
		REMOVE_DEBUG_TRACE(__LINE__);
		goto error_return;
	}
	if ((ip->i_d.di_mode & IFMT) == IFDIR) {
		error = XFS_ERROR(EPERM);
		REMOVE_DEBUG_TRACE(__LINE__);
		goto error_return;
	}

	/*
	 * Return error when removing . and ..
	 */
	if (name[0] == '.') {
		if (name[1] == '\0') {
			error = XFS_ERROR(EINVAL);
			REMOVE_DEBUG_TRACE(__LINE__);
			goto error_return;
		}
		else if (name[1] == '.' && name[2] == '\0') {
			error = XFS_ERROR(EEXIST);
			REMOVE_DEBUG_TRACE(__LINE__);
			goto error_return;
		}
	} 

	/*
	 * Entry must exist since we did a lookup in xfs_lock_dir_and_entry.
	 */
	XFS_BMAP_INIT(&free_list, &first_block);
	error = XFS_DIR_REMOVENAME(mp, tp, dp, name, namelen, ip->i_ino,
		&first_block, &free_list, 0);
	if (error) {
		ASSERT(error != ENOENT);
		REMOVE_DEBUG_TRACE(__LINE__);
		goto error1;
	}
	xfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);

	dnlc_remove(dir_vp, name);

	dp->i_gen++;
	xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);

	error = xfs_droplink(tp, ip);
	if (error) {
		REMOVE_DEBUG_TRACE(__LINE__);
		goto error1;
	}

	/*
	 * Take an extra ref on the inode so that it doesn't
	 * go to xfs_inactive() from within the commit.
	 */
	IHOLD(ip);

	/*
	 * If this is a synchronous mount, make sure that the
	 * remove transaction goes to disk before returning to
	 * the user.
	 */
	if (mp->m_flags & XFS_MOUNT_WSYNC) {
		xfs_trans_set_sync(tp);
	}

	error = xfs_bmap_finish(&tp, &free_list, first_block, &committed);
	if (error) {
		REMOVE_DEBUG_TRACE(__LINE__);
		goto error_rele;
	}

	error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	if (error) {
		IRELE(ip);
		goto std_return;
	}

	/*
	 * Before we drop our extra reference to the inode, purge it
	 * from the refcache if it is there.  By waiting until afterwards
	 * to do the IRELE, we ensure that we won't go inactive in the
	 * xfs_refcache_purge_ip routine (although that would be OK).
	 */
#ifndef SIM
	xfs_refcache_purge_ip(ip);
#endif

	vn_trace_entry(XFS_ITOV(ip), "xfs_remove", (inst_t *)__return_address);

	/*
	 * Let interposed file systems know about removed links.
	 */
	VOP_LINK_REMOVED(XFS_ITOV(ip), dir_vp, (ip)->i_d.di_nlink==0);

	IRELE(ip);

/*	Fall through to std_return with error = 0 */

std_return:
#ifndef SIM
	if (DM_EVENT_ENABLED(dir_vp->v_vfsp, dp,
						DM_EVENT_POSTREMOVE)) {
		(void) dm_send_namesp_event(DM_EVENT_POSTREMOVE,
				dir_bdp, DM_RIGHT_NULL,
				NULL, DM_RIGHT_NULL, 
			      	name, NULL, dm_di_mode, error, 0);
	}
#endif
	return error;

 error1:
	xfs_bmap_cancel(&free_list);
	cancel_flags |= XFS_TRANS_ABORT;

 error_return:
	xfs_trans_cancel(tp, cancel_flags);
	goto std_return;

 error_rele:
	/*
	 * In this case make sure to not release the inode until after
	 * the current transaction is aborted.  Releasing it beforehand
	 * can cause us to go to xfs_inactive and start a recursive
	 * transaction which can easily deadlock with the current one.
	 */
	xfs_bmap_cancel(&free_list);
	cancel_flags |= XFS_TRANS_ABORT;
	xfs_trans_cancel(tp, cancel_flags);
	/*
	 * Before we drop our extra reference to the inode, purge it
	 * from the refcache if it is there.  By waiting until afterwards
	 * to do the IRELE, we ensure that we won't go inactive in the
	 * xfs_refcache_purge_ip routine (although that would be OK).
	 */
#ifndef SIM
	xfs_refcache_purge_ip(ip);
#endif
	IRELE(ip);
	goto std_return;
}


/*
 * xfs_link
 *
 */
STATIC int
xfs_link(
	bhv_desc_t	*target_dir_bdp,
	vnode_t		*src_vp,
	char		*target_name,
	cred_t		*credp)
{
	vnode_t			*realvp;
        xfs_inode_t		*tdp, *sip;
	xfs_ino_t		e_inum;
	xfs_trans_t		*tp;
	xfs_mount_t		*mp;
	xfs_inode_t		*ips[2];
	int			error;
        xfs_bmap_free_t         free_list;
        xfs_fsblock_t           first_block;
	int			cancel_flags;
	int			committed;
	vnode_t 		*target_dir_vp;
	bhv_desc_t		*src_bdp;
	int			resblks;
	int			target_namelen;

	target_dir_vp = BHV_TO_VNODE(target_dir_bdp);
	vn_trace_entry(target_dir_vp, "xfs_link", (inst_t *)__return_address);
	target_namelen = strlen(target_name);
	if (target_namelen >= MAXNAMELEN)
		return XFS_ERROR(EINVAL);
	/*
	 * Get the real vnode.
	 */
	VOP_REALVP(src_vp, &realvp, error);
	if (!error) {
                src_vp = realvp;
	}
	vn_trace_entry(src_vp, "xfs_link", (inst_t *)__return_address);
        if (src_vp->v_type == VDIR) {
                return XFS_ERROR(EPERM);
	}

	/*
	 * For now, manually find the XFS behavior descriptor for
	 * the source vnode.  If it doesn't exist then something
	 * is wrong and we should just return an error.
	 * Eventually we need to figure out how link is going to
	 * work in the face of stacked vnodes.
	 */
	src_bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(src_vp), &xfs_vnodeops);
	if (src_bdp == NULL) {
		return XFS_ERROR(EXDEV);
	}
	sip = XFS_BHVTOI(src_bdp);
	tdp = XFS_BHVTOI(target_dir_bdp);
	mp = tdp->i_mount;
        if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

#ifndef SIM
	if (DM_EVENT_ENABLED(src_vp->v_vfsp, tdp, DM_EVENT_LINK)) {
		error = dm_send_namesp_event(DM_EVENT_LINK,
					target_dir_bdp, DM_RIGHT_NULL,
					src_bdp, DM_RIGHT_NULL,
					target_name, NULL, 0, 0, 0);
		if (error)
			return error;
	}
#endif

	/* Return through std_return after this point. */
        
	if (XFS_IS_QUOTA_ON(mp)) {
		error = 0;
		if (XFS_NOT_DQATTACHED(mp, sip)) 
			error = xfs_qm_dqattach(sip, 0);
		if (!error && sip != tdp && XFS_NOT_DQATTACHED(mp, tdp)) 
			error = xfs_qm_dqattach(tdp, 0);
		if (error)
			goto std_return;
	}

	tp = xfs_trans_alloc(mp, XFS_TRANS_LINK);
	cancel_flags = XFS_TRANS_RELEASE_LOG_RES;
	resblks = XFS_LINK_SPACE_RES(mp, target_namelen);
        error = xfs_trans_reserve(tp, resblks, XFS_LINK_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_LINK_LOG_COUNT);
	if (error == ENOSPC) {
		resblks = 0;
		error = xfs_trans_reserve(tp, 0, XFS_LINK_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_LINK_LOG_COUNT);
	}
	if (error) {
		cancel_flags = 0;
                goto error_return;
	}

	if (sip->i_ino < tdp->i_ino) {
		ips[0] = sip;
		ips[1] = tdp;
	} else {
		ips[0] = tdp;
		ips[1] = sip;
	}

	xfs_lock_inodes(ips, 2, 0, XFS_ILOCK_EXCL);

	/*
	 * Increment vnode ref counts since xfs_trans_commit &
	 * xfs_trans_cancel will both unlock the inodes and
	 * decrement the associated ref counts.
	 */
	VN_HOLD(src_vp);
	VN_HOLD(target_dir_vp);
	xfs_trans_ijoin(tp, sip, XFS_ILOCK_EXCL);
        xfs_trans_ijoin(tp, tdp, XFS_ILOCK_EXCL);

	/*
	 * If the source has too many links, we can't make any more to it.
	 */
	if (sip->i_d.di_nlink >= XFS_MAXLINK) {
		error = XFS_ERROR(EMLINK);
		goto error_return;
	}

	/*
	 * If the source has been unlinked and put on the unlinked
	 * list, we can't link to it.  Doing so would cause the inode
	 * to be placed on the list a second time when the link
	 * created here is removed.
	 */
	if (sip->i_d.di_nlink == 0) {
		error = XFS_ERROR(ENOENT);
		goto error_return;
	}

	/*
	 * If the target directory has been removed, we can't link
	 * any more files in it.
	 */
	if (tdp->i_d.di_nlink == 0) {
		error = XFS_ERROR(ENOENT);
		goto error_return;
	}

	if (error = xfs_iaccess(tdp, IEXEC | IWRITE, credp)) {
                goto error_return;
	}

	/*
	 * Make sure that nothing with the given name exists in the
	 * target directory.  We can't allow the lookup to use the
	 * dnlc, because doing so could cause deadlock if it has to
	 * do a vn_get for the vnode and the vnode is inactive and
	 * waiting for a log reservation in xfs_inactive.
	 */
	error = xfs_dir_lookup_int(NULL, target_dir_bdp, DLF_NODNLC,
			target_name, NULL, &e_inum, NULL, NULL, NULL);
	if (error != ENOENT) {
		if (error == 0) {
			error = XFS_ERROR(EEXIST);
		}
		goto error_return;
	}

	if (resblks == 0 &&
	    (error = XFS_DIR_CANENTER(mp, tp, tdp, target_name,
			target_namelen)))
		goto error_return;

	XFS_BMAP_INIT(&free_list, &first_block);

	error = XFS_DIR_CREATENAME(mp, tp, tdp, target_name, target_namelen,
				   sip->i_ino, &first_block, &free_list,
				   resblks);
	if (error)
		goto abort_return;
	xfs_ichgtime(tdp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	tdp->i_gen++;
	xfs_trans_log_inode(tp, tdp, XFS_ILOG_CORE);

	error = xfs_bumplink(tp, sip);
	if (error) {
		goto abort_return;
	}

	/*
	 * If this is a synchronous mount, make sure that the
	 * link transaction goes to disk before returning to
	 * the user.
	 */
	if (mp->m_flags & XFS_MOUNT_WSYNC) {
		xfs_trans_set_sync(tp);
	}

	error = xfs_bmap_finish (&tp, &free_list, first_block, &committed);
	if (error) {
		xfs_bmap_cancel(&free_list);
		goto abort_return;
	}

	error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	if (error) {
		goto std_return;
	}

	/*
	 * We have to use dnlc_enter() rather than dnlc_enter_fast() here,
	 * because we can't use the dnlc in the lookup call above.  Thus,
	 * we have not initialized fastdata structure to use here.
	 */
	dnlc_enter(target_dir_vp, target_name, XFS_ITOBHV(sip), credp);

	/* Fall through to std_return with error = 0. */
std_return:
#ifndef SIM
	if (DM_EVENT_ENABLED(src_vp->v_vfsp, sip,
						DM_EVENT_POSTLINK)) {
		(void) dm_send_namesp_event(DM_EVENT_POSTLINK,
				target_dir_bdp, DM_RIGHT_NULL,
				src_bdp, DM_RIGHT_NULL,
				target_name, NULL, 0, error, 0);
	}
#endif
	return error;

 abort_return:
	cancel_flags |= XFS_TRANS_ABORT;
	/* FALLTHROUGH */
 error_return:
	xfs_trans_cancel(tp, cancel_flags);

	goto std_return;
}




/*
 * xfs_mkdir
 *
 */
STATIC int
xfs_mkdir(
	bhv_desc_t	*dir_bdp,
	char		*dir_name,
	vattr_t		*vap,
	vnode_t		**vpp,
	cred_t		*credp)
{
        xfs_inode_t             *dp;
	xfs_inode_t		*cdp;	/* inode of created dir */
        xfs_trans_t             *tp;
        xfs_ino_t               e_inum;
	dev_t			rdev;
	mode_t			mode;
        xfs_mount_t		*mp;
	int			cancel_flags;
	int			error;
	int			committed;
        xfs_bmap_free_t         free_list;
        xfs_fsblock_t           first_block;
	vnode_t 		*dir_vp;
	boolean_t		dp_joined_to_trans;
	boolean_t		created = B_FALSE;
	int			dm_event_sent = 0;
	xfs_prid_t		prid;
	struct xfs_dquot	*udqp, *pdqp;
	uint			resblks;
	int			dm_di_mode;
	int			dir_namelen;

	dir_vp = BHV_TO_VNODE(dir_bdp);
        dp = XFS_BHVTOI(dir_bdp);
	mp = dp->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	dir_namelen = strlen(dir_name);
	if (dir_namelen >= MAXNAMELEN)
		return XFS_ERROR(EINVAL);

	tp = NULL;
	dp_joined_to_trans = B_FALSE;
	dm_di_mode = vap->va_mode|VTTOIF(vap->va_type);

#ifndef SIM
	if (DM_EVENT_ENABLED(dir_vp->v_vfsp, dp, DM_EVENT_CREATE)) {
		error = xfs_dm_send_create_event(dir_bdp, dir_name,
					dm_di_mode, &dm_event_sent);
		if (error)
			return error;
	}
#endif

	/* Return through std_return after this point. */

	vn_trace_entry(dir_vp, "xfs_mkdir", (inst_t *)__return_address);
	mp = dp->i_mount;
	udqp = pdqp = NULL;
	if (vap->va_mask & AT_PROJID)
		prid = (xfs_prid_t)vap->va_projid;
	else 	
		prid = (xfs_prid_t)dfltprid;

	/*
	 * Make sure that we have allocated dquot(s) on disk.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		if (error = xfs_qm_vop_dqalloc(mp, dp, credp->cr_uid, prid,
					       XFS_QMOPT_QUOTALL,
					       &udqp, &pdqp)) 
			goto std_return;
	}
	
	tp = xfs_trans_alloc(mp, XFS_TRANS_MKDIR);
	cancel_flags = XFS_TRANS_RELEASE_LOG_RES;
	resblks = XFS_MKDIR_SPACE_RES(mp, dir_namelen);
	error = xfs_trans_reserve(tp, resblks, XFS_MKDIR_LOG_RES(mp), 0,
				  XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT);
	if (error == ENOSPC) {
		resblks = 0;
		error = xfs_trans_reserve(tp, 0, XFS_MKDIR_LOG_RES(mp), 0,
					  XFS_TRANS_PERM_LOG_RES,
					  XFS_MKDIR_LOG_COUNT);
	}
	if (error) {
		cancel_flags = 0;
		dp = NULL;
		goto error_return;
	}

        xfs_ilock(dp, XFS_ILOCK_EXCL);

	/*
	 * Since dp was not locked between VOP_LOOKUP and VOP_MKDIR,
	 * the directory could have been removed.
	 */
        if (dp->i_d.di_nlink == 0) {
		error = XFS_ERROR(ENOENT);
                goto error_return;
	}

	/* 
	 * Check for directory link count overflow.
	 */
	if (dp->i_d.di_nlink >= XFS_MAXLINK) {
		error = XFS_ERROR(EMLINK);
		goto error_return;
	}

	/*
	 * Make sure that nothing with the given name exists in the
	 * target directory.  We can't allow the lookup to use the
	 * dnlc, because doing so could cause deadlock if it has to
	 * do a vn_get for the vnode and the vnode is inactive and
	 * waiting for a log reservation in xfs_inactive.
	 */
        error = xfs_dir_lookup_int(NULL, dir_bdp, DLF_NODNLC, dir_name,
				   NULL, &e_inum, NULL, NULL, NULL);
        if (error != ENOENT) {
		if (error == 0)
			error = XFS_ERROR(EEXIST);
                goto error_return;
	}

	/*
	 * check access.
	 */
	if (error = xfs_iaccess(dp, IEXEC | IWRITE, credp)) {
                goto error_return;
	}
	
	/*
	 * Reserve disk quota and the inode.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		if (xfs_trans_reserve_quota(tp, udqp, pdqp, resblks, 1, 0)) {
			error = XFS_ERROR(EDQUOT);
			goto error_return;
		}
	} 

	if (resblks == 0 &&
	    (error = XFS_DIR_CANENTER(mp, tp, dp, dir_name, dir_namelen)))
		goto error_return;
	/*
	 * create the directory inode.
	 */
	rdev = (vap->va_mask & AT_RDEV) ? vap->va_rdev : NODEV;
        mode = IFDIR | (vap->va_mode & ~IFMT);
	error = xfs_dir_ialloc(&tp, dp, mode, 2, rdev, credp, prid, resblks > 0,
		&cdp, NULL);
	if (error) {
		if (error == ENOSPC)
			goto error_return;
		goto abort_return;
	}
	ITRACE(cdp);

	/*
	 * Now we add the directory inode to the transaction.
	 * We waited until now since xfs_dir_ialloc might start
	 * a new transaction.  Had we joined the transaction
	 * earlier, the locks might have gotten released.
	 */
	VN_HOLD(dir_vp);
        xfs_trans_ijoin(tp, dp, XFS_ILOCK_EXCL);
	dp_joined_to_trans = B_TRUE;

	XFS_BMAP_INIT(&free_list, &first_block);

	error = XFS_DIR_CREATENAME(mp, tp, dp, dir_name, dir_namelen,
			cdp->i_ino, &first_block, &free_list,
			resblks ? resblks - XFS_IALLOC_SPACE_RES(mp) : 0);
	if (error) {
		ASSERT(error != ENOSPC);
		goto error1;
	}
	xfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);

	/*
	 * Since we used the DLF_NODNLC option to xfs_dir_lookup_int()
	 * above, we don't have an initialized fastdata structure to
	 * use for adding our entry to the dnlc.  Instead we have to
	 * use the slightly slower plain dnlc_enter().
	 */
	dnlc_enter(dir_vp, dir_name, XFS_ITOBHV(cdp), NOCRED);

	/*
	 * Bump the in memory version number of the parent directory
	 * so that other processes accessing it will recognize that
	 * the directory has changed.
	 */
	dp->i_gen++;
	
	error = XFS_DIR_INIT(mp, tp, cdp, dp);
	if (error) {
		goto error2;
	}

	cdp->i_gen = 1;
	error = xfs_bumplink(tp, dp);
	if (error) {
		goto error2;
	}

	dnlc_remove(XFS_ITOV(cdp), "..");

	created = B_TRUE;
	*vpp = XFS_ITOV(cdp);
	IHOLD(cdp);
		
	/*
	 * Attach the dquots to the new inode and modify the icount incore.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		xfs_qm_vop_dqattach_and_dqmod_newinode(tp, cdp, udqp, pdqp);
	}
	
	/*
	 * If this is a synchronous mount, make sure that the
	 * mkdir transaction goes to disk before returning to
	 * the user.
	 */
	if (mp->m_flags & XFS_MOUNT_WSYNC) {
		xfs_trans_set_sync(tp);
	}

	error = xfs_bmap_finish(&tp, &free_list, first_block, &committed);
	if (error) {
		IRELE(cdp);
		goto error2;
	}

	error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	if (udqp)
		 xfs_qm_dqrele(udqp);
	if (pdqp)
		 xfs_qm_dqrele(pdqp);

	if (error) {
		dnlc_remove(dir_vp, dir_name);
		IRELE(cdp);
	}

	/* Fall through to std_return with error = 0 or errno from
	 * xfs_trans_commit. */

std_return:
#ifndef SIM
	if ( (created || (error != 0 && dm_event_sent != 0)) &&
			DM_EVENT_ENABLED(dir_vp->v_vfsp, XFS_BHVTOI(dir_bdp),
						DM_EVENT_POSTCREATE)) {
		(void) dm_send_namesp_event(DM_EVENT_POSTCREATE,
					dir_bdp, DM_RIGHT_NULL,
					created ? XFS_ITOBHV(cdp):NULL,
					DM_RIGHT_NULL,
					dir_name, NULL,
					dm_di_mode, error, 0);
	}
#endif
	return error;

 error2:
	dnlc_remove(dir_vp, dir_name);
 error1:
	xfs_bmap_cancel(&free_list);
 abort_return:
	cancel_flags |= XFS_TRANS_ABORT;
 error_return:
	xfs_trans_cancel(tp, cancel_flags);

	if (udqp)
		xfs_qm_dqrele(udqp);
	if (pdqp)
		xfs_qm_dqrele(pdqp);
	
	if (!dp_joined_to_trans && (dp != NULL)) {
		xfs_iunlock(dp, XFS_ILOCK_EXCL);
	}

	goto std_return;
}


/*
 * xfs_rmdir
 *
 */
STATIC int
xfs_rmdir(
	bhv_desc_t	*dir_bdp,
	char		*name,
	vnode_t		*current_dir_vp,
	cred_t		*credp)
{
        xfs_inode_t             *dp;
        xfs_inode_t             *cdp;   /* child directory */
        xfs_trans_t             *tp;
	xfs_mount_t		*mp;
        int                     error;
        xfs_bmap_free_t         free_list;
        xfs_fsblock_t           first_block;
	int			cancel_flags;
	int			committed;
	int			dir_generation;
	int			entry_changed;
	vnode_t 		*dir_vp;
	int			dm_di_mode = 0;
	int			namelen;
	uint			resblks;

	dir_vp = BHV_TO_VNODE(dir_bdp);
        dp = XFS_BHVTOI(dir_bdp);
	vn_trace_entry(dir_vp, "xfs_rmdir", (inst_t *)__return_address);

	if (XFS_FORCED_SHUTDOWN(XFS_BHVTOI(dir_bdp)->i_mount))
		return XFS_ERROR(EIO);
	namelen = strlen(name);
	if (namelen >= MAXNAMELEN)
		return XFS_ERROR(EINVAL);

#ifndef SIM
	if (DM_EVENT_ENABLED(dir_vp->v_vfsp, dp, DM_EVENT_REMOVE)) {
		error = dm_send_namesp_event(DM_EVENT_REMOVE,
					dir_bdp, DM_RIGHT_NULL,
					NULL, DM_RIGHT_NULL,
					name, NULL, 0, 0, 0);
		if (error)
			return XFS_ERROR(error);
	}
#endif

	/* Return through std_return after this point. */

 retry:
	cdp = NULL;

	/*
	 * We need to get a reference to cdp before we get our log
	 * reservation.  The reason for this is that we cannot call
	 * xfs_iget for an inode for which we do not have a reference
	 * once we've acquired a log reservation.  This is because the
	 * inode we are trying to get might be in xfs_inactive going
	 * for a log reservation.  Since we'll have to wait for the
	 * inactive code to complete before returning from xfs_iget,
	 * we need to make sure that we don't have log space reserved
	 * when we call xfs_iget.  Instead we get an unlocked referece
	 * to the inode before getting our log reservation.
	 */
	error = xfs_get_dir_entry(dp, name, &cdp, &dir_generation);
	if (error) {
		REMOVE_DEBUG_TRACE(__LINE__);
		goto std_return;
	}
	mp = dp->i_mount;
	dm_di_mode = cdp->i_d.di_mode;

	/*
	 * Get the dquots for the inodes.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		ASSERT(! error);
		if (XFS_NOT_DQATTACHED(mp, dp))
			error = xfs_qm_dqattach(dp, 0);
		if (!error && dp != cdp && XFS_NOT_DQATTACHED(mp, cdp))
			error = xfs_qm_dqattach(cdp, 0);
		if (error) {
			IRELE(cdp);
			REMOVE_DEBUG_TRACE(__LINE__);
			goto std_return;
		}
	}

	tp = xfs_trans_alloc(mp, XFS_TRANS_RMDIR);
	cancel_flags = XFS_TRANS_RELEASE_LOG_RES;
	/*
	 * We try to get the real space reservation first,
	 * allowing for directory btree deletion(s) implying
	 * possible bmap insert(s).  If we can't get the space
	 * reservation then we use 0 instead, and avoid the bmap
	 * btree insert(s) in the directory code by, if the bmap
	 * insert tries to happen, instead trimming the LAST
	 * block from the directory.
	 */
	resblks = XFS_REMOVE_SPACE_RES(mp);
	error = xfs_trans_reserve(tp, resblks, XFS_REMOVE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_DEFAULT_LOG_COUNT);
	if (error == ENOSPC) {
		resblks = 0;
		error = xfs_trans_reserve(tp, 0, XFS_REMOVE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_DEFAULT_LOG_COUNT);
	}
	if (error) {
		ASSERT(error != ENOSPC);
		cancel_flags = 0;
		IRELE(cdp);
                goto error_return;
	}
	XFS_BMAP_INIT(&free_list, &first_block);

	/*
	 * Now lock the child directory inode and the parent directory
	 * inode in the proper order.  This will take care of validating
	 * that the directory entry for the child directory inode has
	 * not changed while we were obtaining a log reservation.
	 */
	error = xfs_lock_dir_and_entry(dp, name, cdp, dir_generation,
				       &entry_changed);
	if (error) {
		xfs_trans_cancel(tp, cancel_flags);
		IRELE(cdp);
		goto std_return;
	}
	if (error = _MAC_XFS_IACCESS(cdp, MACWRITE, credp)) {
		xfs_trans_cancel(tp, cancel_flags);
		IRELE(cdp);
		goto std_return;
	}

	/*
	 * If the inode we found in the first pass is no longer
	 * the entry with the given name, then drop our transaction and
	 * inode reference and start over.
	 */
	if (entry_changed) {
		xfs_trans_cancel(tp, cancel_flags);
		IRELE(cdp);
		goto retry;
	}

	xfs_trans_ijoin(tp, dp, XFS_ILOCK_EXCL);
	if (dp != cdp) {
		/*
		 * Only increment the parent directory vnode count if
		 * we didn't bump it in looking up cdp.  The only time
		 * we don't bump it is when we're looking up ".".
		 */
		VN_HOLD(dir_vp);
	}

	ITRACE(cdp);
	xfs_trans_ijoin(tp, cdp, XFS_ILOCK_EXCL);

	if (error = xfs_iaccess(dp, IEXEC | IWRITE, credp)) {
                goto error_return;
	}

	if (error = xfs_stickytest(dp, cdp, credp)) {
                goto error_return;
	}

	if ((cdp == dp) || (XFS_ITOV(cdp) == current_dir_vp)) {
		error = XFS_ERROR(EINVAL);
		goto error_return;
	}
	if ((cdp->i_d.di_mode & IFMT) != IFDIR) {
	        error = XFS_ERROR(ENOTDIR);
		goto error_return;
	}
	if (XFS_ITOV(cdp)->v_vfsmountedhere) {
		error = XFS_ERROR(EBUSY);
		goto error_return;
	}
	ASSERT(cdp->i_d.di_nlink >= 2);
	if (cdp->i_d.di_nlink != 2) {
		error = XFS_ERROR(EEXIST);
		goto error_return;
        }
	if (!XFS_DIR_ISEMPTY(mp, cdp)) {
		error = XFS_ERROR(EEXIST);
		goto error_return;
	}

	error = XFS_DIR_REMOVENAME(mp, tp, dp, name, namelen, cdp->i_ino,
		&first_block, &free_list, resblks);
	if (error) {
		goto error1;
	}

	xfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);

	dnlc_remove(dir_vp, name);

	/*
	 * Bump the in memory generation count on the parent
	 * directory so that other can know that it has changed.
	 */
	dp->i_gen++;

	/*
	 * Drop the link from cdp's "..".
	 */
	error = xfs_droplink(tp, dp);
	if (error) {
		goto error1;
	}

	/*
	 * Drop the link from dp to cdp.
	 */
	error = xfs_droplink(tp, cdp);
	if (error) {
		goto error1;
	}

	/*
	 * Drop the "." link from cdp to self.
	 */
	error = xfs_droplink(tp, cdp);
	if (error) {
		goto error1;
	}

	/*
	 * Take an extra ref on the child vnode so that it
	 * does not go to xfs_inactive() from within the commit.
	 */
	IHOLD(cdp);
	
	/*
	 * If this is a synchronous mount, make sure that the
	 * rmdir transaction goes to disk before returning to
	 * the user.
	 */
	if (mp->m_flags & XFS_MOUNT_WSYNC) {
		xfs_trans_set_sync(tp);
	}

	error = xfs_bmap_finish (&tp, &free_list, first_block, &committed);
	if (error) {
		xfs_bmap_cancel(&free_list);
		xfs_trans_cancel(tp, (XFS_TRANS_RELEASE_LOG_RES |
				 XFS_TRANS_ABORT));
		IRELE(cdp);
		goto std_return;
	}

	error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	if (error) {
		IRELE(cdp);
		goto std_return;
	}


	/*
	 * Let interposed file systems know about removed links.
	 */
	VOP_LINK_REMOVED(dir_vp, XFS_ITOV(cdp), (dp)->i_d.di_nlink==0);
	VOP_LINK_REMOVED(XFS_ITOV(cdp), dir_vp, (cdp)->i_d.di_nlink==0);

	IRELE(cdp);

	/* Fall through to std_return with error = 0 or the errno
	 * from xfs_trans_commit. */
std_return:
#ifndef SIM
	if (DM_EVENT_ENABLED(dir_vp->v_vfsp, dp,
						DM_EVENT_POSTREMOVE)) {
		(void) dm_send_namesp_event(DM_EVENT_POSTREMOVE,
					dir_bdp, DM_RIGHT_NULL,
					NULL, DM_RIGHT_NULL,
					name, NULL, dm_di_mode,
					error, 0);
	}
#endif
	return error;

 error1:
	xfs_bmap_cancel(&free_list);
	cancel_flags |= XFS_TRANS_ABORT;
 error_return:
	xfs_trans_cancel(tp, cancel_flags);
	goto std_return;
}



/*
 * xfs_readdir
 *
 * Read dp's entries starting at uiop->uio_offset and translate them into
 * bufsize bytes worth of struct dirents starting at bufbase.
 */
/*ARGSUSED*/
STATIC int
xfs_readdir(
	bhv_desc_t	*dir_bdp,
	uio_t		*uiop,
	cred_t		*credp,
	int		*eofp)
{
        xfs_inode_t             *dp;
        xfs_trans_t             *tp = NULL;
	int			error;
	uint			lock_mode;
	off_t			start_offset;


	vn_trace_entry(BHV_TO_VNODE(dir_bdp), "xfs_readdir",
		       (inst_t *)__return_address);
        dp = XFS_BHVTOI(dir_bdp);

	if (XFS_FORCED_SHUTDOWN(dp->i_mount))
		return XFS_ERROR(EIO);

        lock_mode = xfs_ilock_map_shared(dp);

        if ((dp->i_d.di_mode & IFMT) != IFDIR) {
		xfs_iunlock_map_shared(dp, lock_mode);
                return XFS_ERROR(ENOTDIR);
        }

	/* If the directory has been removed after it was opened. */
        if (dp->i_d.di_nlink == 0) {
                xfs_iunlock_map_shared(dp, lock_mode);
                return 0;
        }

	start_offset = uiop->uio_offset;
	error = XFS_DIR_GETDENTS(dp->i_mount, tp, dp, uiop, eofp);
	if (start_offset != uiop->uio_offset) {
		xfs_ichgtime(dp, XFS_ICHGTIME_ACC);
	}

	xfs_iunlock_map_shared(dp, lock_mode);
	
	return error;
}

/*
 * xfs_symlink
 *
 */
STATIC int
xfs_symlink(
	bhv_desc_t	*dir_bdp,
	char	       	*link_name,
	vattr_t		*vap,
	char		*target_path,
	cred_t		*credp)
{
	xfs_trans_t		*tp;
	xfs_mount_t		*mp;
	xfs_inode_t		*dp;
	xfs_inode_t		*ip;
        int 			error;
	int			pathlen;
        struct pathname		cpn;
	struct pathname		ccpn;
	xfs_ino_t		e_inum;
	dev_t			rdev;
	xfs_bmap_free_t		free_list;
	xfs_fsblock_t		first_block;
	boolean_t		dp_joined_to_trans;
	vnode_t 		*dir_vp;
	uint			cancel_flags;
	int			committed;
	xfs_fileoff_t		first_fsb;
	xfs_filblks_t		fs_blocks;
	int			nmaps;
	xfs_bmbt_irec_t		mval[SYMLINK_MAPS];
	daddr_t			d;
	char			*cur_chunk;
	int			byte_cnt;
	int			n;
	buf_t			*bp;
	xfs_prid_t		prid;
	struct xfs_dquot	*udqp, *pdqp;
	uint			resblks;
	int			link_namelen;

	dir_vp = BHV_TO_VNODE(dir_bdp);
        dp = XFS_BHVTOI(dir_bdp);
	dp_joined_to_trans = B_FALSE;
	error = 0;
	tp = NULL;

	vn_trace_entry(dir_vp, "xfs_symlink", (inst_t *)__return_address);

	mp = dp->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	link_namelen = strlen(link_name);
	if (link_namelen >= MAXNAMELEN)
		return XFS_ERROR(EINVAL);
	/*
	 * Check component lengths of the target path name.
         */
        pathlen = strlen(target_path);
        if (pathlen >= MAXPATHLEN)      /* total string too long */
                return XFS_ERROR(ENAMETOOLONG);
        if (pathlen >= MAXNAMELEN) {    /* is any component too long? */
                pn_alloc(&cpn);
                pn_alloc(&ccpn);
                bcopy(target_path, cpn.pn_path, pathlen);
                cpn.pn_pathlen = pathlen;
                while (cpn.pn_pathlen > 0 && !error) {
                        if (error = pn_getcomponent(&cpn, ccpn.pn_path, 0)) {
                                pn_free(&cpn);
                                pn_free(&ccpn);
                                if (error == ENAMETOOLONG)
                                        return error;
                        } else if (cpn.pn_pathlen) {    /* advance past slash */                                cpn.pn_path++;
                                cpn.pn_pathlen--;
                        }
                }
                pn_free(&cpn);
                pn_free(&ccpn);
        }

#ifndef SIM
	if (DM_EVENT_ENABLED(dir_vp->v_vfsp, dp, DM_EVENT_SYMLINK)) {
		error = dm_send_namesp_event(DM_EVENT_SYMLINK, dir_bdp, DM_RIGHT_NULL,
						NULL, DM_RIGHT_NULL,
						link_name, target_path,
						0, 0, 0);
		if (error)
			return error;
	}
#endif

	/* Return through std_return after this point. */

        udqp = pdqp = NULL;
	if (vap->va_mask & AT_PROJID)
		prid = (xfs_prid_t)vap->va_projid;
	else 	
		prid = (xfs_prid_t)dfltprid;

	/*
	 * Make sure that we have allocated dquot(s) on disk.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		if (error = xfs_qm_vop_dqalloc(mp, dp, credp->cr_uid, prid,
					       XFS_QMOPT_QUOTALL,
					       &udqp, &pdqp)) 
			goto std_return;
	}

	tp = xfs_trans_alloc(mp, XFS_TRANS_SYMLINK);
	cancel_flags = XFS_TRANS_RELEASE_LOG_RES;
	/*
	 * The symlink will fit into the inode data fork?
	 * There can't be any attributes so we get the whole variable part.
	 */
	if (pathlen <= XFS_LITINO(mp))
		fs_blocks = 0;
	else
		fs_blocks = XFS_B_TO_FSB(mp, pathlen);
	resblks = XFS_SYMLINK_SPACE_RES(mp, link_namelen, fs_blocks);
        error = xfs_trans_reserve(tp, resblks, XFS_SYMLINK_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_SYMLINK_LOG_COUNT);
	if (error == ENOSPC && fs_blocks == 0) {
		resblks = 0;
		error = xfs_trans_reserve(tp, 0, XFS_SYMLINK_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_SYMLINK_LOG_COUNT);
	}
	if (error) {
		cancel_flags = 0;
		dp = NULL;
                goto error_return;
	}

	xfs_ilock(dp, XFS_ILOCK_EXCL);

	/*
	 * If the directory has been removed, then we can't create
	 * anything in it.
	 */
	if (dp->i_d.di_nlink == 0) {
		error = XFS_ERROR(ENOENT);
		goto error_return;
	}

	if (error = xfs_iaccess(dp, IEXEC | IWRITE, credp)) {
                goto error_return;
	}

	/*
	 * Since we've already started a transaction, we cannot allow
	 * the lookup to do a vn_get().  Thus, stay out of the dnlc
	 * in doing the lookup.  Doing a vn_get could cause us to deadlock
	 * if the inode we are doing the get for is in inactive waiting
	 * for a log reservation.
	 */
        error = xfs_dir_lookup_int(NULL, dir_bdp, DLF_NODNLC, link_name,
				   NULL, &e_inum, NULL, NULL, NULL);
	if (error != ENOENT) {
		if (!error) {
			error = XFS_ERROR(EEXIST);
		}
                goto error_return;
	}
	/*
	 * Reserve disk quota : blocks and inode.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		if (xfs_trans_reserve_quota(tp, udqp, pdqp, resblks, 1, 0)) {
			error = XFS_ERROR(EDQUOT);
			goto error_return;
		}
	}
	
	/*
	 * Check for ability to enter directory entry, if no space reserved.
	 */
	if (resblks == 0 &&
	    (error = XFS_DIR_CANENTER(mp, tp, dp, link_name, link_namelen)))
		goto error_return;
	/*
	 * Initialize the bmap freelist prior to calling either
	 * bmapi or the directory create code.
	 */
	XFS_BMAP_INIT(&free_list, &first_block);

	/*
	 * Allocate an inode for the symlink.
	 */
	rdev = (vap->va_mask & AT_RDEV) ? vap->va_rdev : NODEV;

	error = xfs_dir_ialloc(&tp, dp, IFLNK | (vap->va_mode&~IFMT),
			       1, rdev, credp, prid, resblks > 0, &ip, NULL);
	if (error) {
		if (error == ENOSPC)
			goto error_return;
		goto error1;
	}
	ITRACE(ip);

	VN_HOLD(dir_vp);
        xfs_trans_ijoin(tp, dp, XFS_ILOCK_EXCL);
        dp_joined_to_trans = B_TRUE;

	/*
	 * Also attach the dquot(s) to it, if applicable.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		xfs_qm_vop_dqattach_and_dqmod_newinode(tp, ip, udqp, pdqp);
	}

	if (resblks)
		resblks -= XFS_IALLOC_SPACE_RES(mp);
	/*
	 * If the symlink will fit into the inode, write it inline.
	 */
	if (pathlen <= XFS_IFORK_DSIZE(ip)) {
		xfs_idata_realloc(ip, pathlen, XFS_DATA_FORK);
		bcopy(target_path, ip->i_df.if_u1.if_data, pathlen);
		ip->i_d.di_size = pathlen;

		/*
		 * The inode was initially created in extent format.
		 */
		ip->i_df.if_flags &= ~(XFS_IFEXTENTS | XFS_IFBROOT);
		ip->i_df.if_flags |= XFS_IFINLINE;

		ip->i_d.di_format = XFS_DINODE_FMT_LOCAL;
		xfs_trans_log_inode(tp, ip, XFS_ILOG_DDATA | XFS_ILOG_CORE);

	} else {
		first_fsb = 0;
		nmaps = SYMLINK_MAPS;

		error = xfs_bmapi(tp, ip, first_fsb, fs_blocks,
				  XFS_BMAPI_WRITE | XFS_BMAPI_METADATA,
				  &first_block, resblks, mval, &nmaps,
				  &free_list);
		if (error) {
			goto error1;
		}

		if (resblks)
			resblks -= fs_blocks;
		ip->i_d.di_size = pathlen;
		xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

		cur_chunk = target_path;
		for (n = 0; n < nmaps; n++) {
			d = XFS_FSB_TO_DADDR(mp, mval[n].br_startblock);
			byte_cnt = XFS_FSB_TO_B(mp, mval[n].br_blockcount);
			bp = xfs_trans_get_buf(tp, mp->m_ddev_targp, d, 
					       BTOBB(byte_cnt), 0);
			ASSERT(bp && !geterror(bp));
			if (pathlen < byte_cnt) {
				byte_cnt = pathlen;
			}
			pathlen -= byte_cnt;

			bcopy(cur_chunk, bp->b_un.b_addr, byte_cnt);
			cur_chunk += byte_cnt;

			xfs_trans_log_buf(tp, bp, 0, byte_cnt - 1);
		}
	}

	/*
	 * Create the directory entry for the symlink.
	 */
	error = XFS_DIR_CREATENAME(mp, tp, dp, link_name, link_namelen,
			ip->i_ino, &first_block, &free_list, resblks);
	if (error) {
		goto error1;
	}
	xfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);

        dnlc_enter(dir_vp, link_name, XFS_ITOBHV(ip), NOCRED);

	/*
	 * Bump the in memory version number of the parent directory
	 * so that other processes accessing it will recognize that
	 * the directory has changed.
	 */
	dp->i_gen++;

	/*
	 * If this is a synchronous mount, make sure that the
	 * symlink transaction goes to disk before returning to
	 * the user.
	 */
	if (mp->m_flags & XFS_MOUNT_WSYNC) {
		xfs_trans_set_sync(tp);
	}

	error = xfs_bmap_finish(&tp, &free_list, first_block, &committed);
	if (error) {
		goto error2;
	}
	error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	if (udqp)
		xfs_qm_dqrele(udqp);
	if (pdqp)
		xfs_qm_dqrele(pdqp);
	if (error)
		dnlc_remove(dir_vp, link_name);
	
	/* Fall through to std_return with error = 0 or errno from
	 * xfs_trans_commit	*/
std_return:
#ifndef SIM
	if (DM_EVENT_ENABLED(dir_vp->v_vfsp, XFS_BHVTOI(dir_bdp),
			     DM_EVENT_POSTSYMLINK)) {
		(void) dm_send_namesp_event(DM_EVENT_POSTSYMLINK,
						dir_bdp, DM_RIGHT_NULL,
						error? NULL:XFS_ITOBHV(ip),
						DM_RIGHT_NULL,
						link_name, target_path,
						0, error, 0);
	}
#endif
	return error;

 error2:
	dnlc_remove(dir_vp, link_name);
 error1:
	xfs_bmap_cancel(&free_list);
	cancel_flags |= XFS_TRANS_ABORT;
 error_return:
	xfs_trans_cancel(tp, cancel_flags);
	if (udqp)
		xfs_qm_dqrele(udqp);
	if (pdqp)
		xfs_qm_dqrele(pdqp);

	if (!dp_joined_to_trans && (dp != NULL)) {
		xfs_iunlock(dp, XFS_ILOCK_EXCL);
	}

	goto std_return;
}

/*
 * This is called from the customized NFS server code that
 * keeps the fid structure on the stack rather than having
 * us kmem_alloc one.  It is much more CPU efficient to do
 * it this way.
 */
int
xfs_fast_fid(
	bhv_desc_t	*bdp, 
	xfs_fid_t	*xfid)
{
	xfs_inode_t	*ip;

	ip = XFS_BHVTOI(bdp);

	xfid->fid_len = sizeof(xfs_fid_t) - sizeof(xfid->fid_len);
	xfid->fid_pad = ip->i_ino >> 32;
	xfid->fid_ino = (xfs_fid_ino_t)ip->i_ino;
	xfid->fid_gen = ip->i_d.di_gen;	

	return 0;
}

/*
 * xfs_fid this returns the old ten-byte version for NFS.
 */
STATIC int
xfs_fid(
	bhv_desc_t	*bdp,
	fid_t		**fidpp)
{
	xfs_mount_t	*mp;

	vn_trace_entry(BHV_TO_VNODE(bdp), "xfs_fid",
		       (inst_t *)__return_address);

	mp = XFS_BHVTOI(bdp)->i_mount;
	if (XFS_INO_BITS(mp) > (NBBY * (sizeof(xfs_fid_ino_t)+sizeof(u_short)))) {
	  /*
	   * If the ino won't fit into the space that is
	   * in our xfs_fid structure, then return an error.
	   */
		*fidpp = NULL;
		return XFS_ERROR(EFBIG);
	}
	
	*fidpp = (struct fid *)kmem_alloc(sizeof(xfs_fid_t), KM_SLEEP);
	return (xfs_fast_fid(bdp, (xfs_fid_t *)*fidpp));
}


/*
 * xfs_fid2
 *
 * A fid routine that takes a pointer to a previously allocated
 * fid structure (like xfs_fast_fid) but uses a 64 bit inode number.
 */
STATIC int
xfs_fid2(
	bhv_desc_t	*bdp,
	fid_t		*fidp)
{
	xfs_inode_t	*ip;
	xfs_fid2_t	*xfid;

	vn_trace_entry(BHV_TO_VNODE(bdp), "xfs_fid2",
		       (inst_t *)__return_address);
	ASSERT(sizeof(fid_t) >= sizeof(xfs_fid2_t));

	xfid = (xfs_fid2_t *)fidp;
	ip = XFS_BHVTOI(bdp);
	xfid->fid_len = sizeof(xfs_fid2_t) - sizeof(xfid->fid_len);
	xfid->fid_pad = 0;
	/*
	 * use bcopy because the inode is a long long and there's no
	 * assurance that xfid->fid_ino is properly aligned.
	 */
	bcopy(&ip->i_ino, &xfid->fid_ino, sizeof xfid->fid_ino);
	xfid->fid_gen = ip->i_d.di_gen;	

	return 0;
}


/*
 * xfs_rwlock
 */
void
xfs_rwlock(
	bhv_desc_t	*bdp,
	vrwlock_t	locktype)
{
	xfs_rwlockf(bdp, locktype, 0);
}

/*
 * currently the only supported flag is XFS_IOLOCK_RECURSIVE
 */
void
xfs_rwlockf(
	bhv_desc_t	*bdp,
	vrwlock_t	locktype,
	int		flags)
{
	xfs_inode_t	*ip;
	vnode_t 	*vp;

	vp = BHV_TO_VNODE(bdp);
	if (vp->v_type == VDIR)
		return;
	ip = XFS_BHVTOI(bdp);
	if (locktype == VRWLOCK_WRITE) {
		xfs_ilock(ip, XFS_IOLOCK_EXCL|flags);
	} else {
		ASSERT((locktype == VRWLOCK_READ) ||
		       (locktype == VRWLOCK_WRITE_DIRECT));
		xfs_ilock(ip, XFS_IOLOCK_SHARED|flags);
	}
}


/*
 * xfs_rwunlock
 */
void
xfs_rwunlock(
	bhv_desc_t	*bdp,
	vrwlock_t	locktype)
{
	xfs_rwunlockf(bdp, locktype, 0);
}

/*
 * xfs_rwunlockf
 */
void
xfs_rwunlockf(
	bhv_desc_t	*bdp,
	vrwlock_t	locktype,
	int		flags)
{
        xfs_inode_t     *ip;
	xfs_inode_t	*release_ip;
	vnode_t 	*vp;

	vp = BHV_TO_VNODE(bdp);
	if (vp->v_type == VDIR)
		return;
        ip = XFS_BHVTOI(bdp);
	if (locktype == VRWLOCK_WRITE) {
		/*
		 * In the write case, we may have added a new entry to
		 * the reference cache.  This might store a pointer to
		 * an inode to be released in this inode.  If it is there,
		 * clear the pointer and release the inode after unlocking
		 * this one.
		 */
		release_ip = ip->i_release;
		ip->i_release = NULL;
        	xfs_iunlock (ip, XFS_IOLOCK_EXCL|flags);
		
		if (release_ip != NULL) {
			VN_RELE(XFS_ITOV(release_ip));
		}
	} else {
		ASSERT((locktype == VRWLOCK_READ) ||
		       (locktype == VRWLOCK_WRITE_DIRECT));
        	xfs_iunlock(ip, XFS_IOLOCK_SHARED|flags);
	}
        return;
}


/*
 * xfs_seek
 *
 * Return an error if the new offset has overflowed and gone below
 * 0 or is greater than our maximum defined file offset.  Just checking
 * for overflow is not enough since off_t may be an __int64_t but the
 * file size may be limited to some number of bits between 32 and 64.
 */
/*ARGSUSED*/
STATIC int
xfs_seek(
	bhv_desc_t	*bdp,
	off_t		old_offset,
	off_t		*new_offsetp)
{
	vnode_t 	*vp;

	vp = BHV_TO_VNODE(bdp);
	if (vp->v_type == VDIR)
		return(0);
	if ((*new_offsetp > XFS_MAX_FILE_OFFSET) ||
	    (*new_offsetp < 0)) {
		return XFS_ERROR(EINVAL);
	} else {
		return 0;
	}
}



/*
 * xfs_frlock
 *
 * This is a stub.
 */
STATIC int
xfs_frlock(
	bhv_desc_t	*bdp,
	int		cmd,
	flock_t		*flockp,
	int		flag,
	off_t		offset,
	vrwlock_t	vrwlock,
	cred_t		*credp)
{
	xfs_inode_t	*ip;
	int		dolock, error;

	vn_trace_entry(BHV_TO_VNODE(bdp), "xfs_frlock",
		       (inst_t *)__return_address);
	ip = XFS_BHVTOI(bdp);

	dolock = (vrwlock == VRWLOCK_NONE);
	if (dolock) {
		xfs_ilock(ip, XFS_IOLOCK_EXCL);
		vrwlock = VRWLOCK_WRITE;
	}

	ASSERT(vrwlock == VRWLOCK_READ ? ismrlocked(&ip->i_iolock, MR_ACCESS) :
	       ismrlocked(&ip->i_iolock, MR_UPDATE));	

	error = fs_frlock(bdp, cmd, flockp, flag, offset, vrwlock, credp);
	if (dolock)
		xfs_iunlock(ip, XFS_IOLOCK_EXCL);
	return error;
}

/*
 * xfs_allocstore
 *
 * This is called to reserve or allocate space for the given range.
 * Currently, this only supports reserving the space for a single
 * page.  By using NDPP (number of BBs per page) bmbt_irec structures,
 * we ensure that the entire page can be mapped in a single bmap call.
 * This simplifies the back out code in that all the information we need
 * to back out is in the single bmbt_irec array orig_imap.
 */
/*ARGSUSED*/
STATIC int
xfs_allocstore(
	bhv_desc_t	*bdp,
	off_t		offset,
	size_t		count,
	cred_t		*credp)
{
	xfs_mount_t	*mp;
	xfs_inode_t	*ip;
	off_t		isize;
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	last_fsb;
        xfs_fileoff_t	curr_off_fsb;
	xfs_fileoff_t	unmap_offset_fsb;
	xfs_filblks_t	count_fsb;
	xfs_filblks_t	unmap_len_fsb;
	xfs_fsblock_t	firstblock;
	xfs_bmbt_irec_t	*imapp;
	xfs_bmbt_irec_t	*last_imapp;
	int		i;
	int		nimaps;
	int		orig_nimaps;
	int		error;
	xfs_bmbt_irec_t	imap[XFS_BMAP_MAX_NMAP];
	xfs_bmbt_irec_t	orig_imap[NDPP];
	

	vn_trace_entry(BHV_TO_VNODE(bdp), "xfs_allocstore",
		       (inst_t *)__return_address);
	/*
	 * This code currently only works for a single page.
	 */
	ASSERT(poff(offset) == 0);
	ASSERT(count == NBPP);
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	xfs_ilock(ip, XFS_ILOCK_EXCL);

	/*
	 * Make sure that the dquots exist, and that they are attached to
	 * the inode. XXXwhy do we DQALLOC here? sup
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, XFS_QMOPT_DQALLOC |
						    XFS_QMOPT_ILOCKED)) {
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				return (error);
			}
		}
	}
	isize = ip->i_d.di_size;
	if (offset >= isize) {
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		return XFS_ERROR(EINVAL);
	}
	if ((offset + count) > isize) {
		count = isize - offset;
	}
	last_fsb = XFS_B_TO_FSB(mp, offset + count);
	count_fsb = (xfs_filblks_t)(last_fsb - offset_fsb);
	orig_nimaps = NDPP;
	firstblock = NULLFSBLOCK;
	error = xfs_bmapi(NULL, ip, offset_fsb, count_fsb, 0, &firstblock, 0,
			  orig_imap, &orig_nimaps, NULL);
	if (error) {
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		return error;
	}
	ASSERT(orig_nimaps > 0);

	curr_off_fsb = offset_fsb;
	while (count_fsb > 0) {
		nimaps = XFS_BMAP_MAX_NMAP;
		firstblock = NULLFSBLOCK;
		error = xfs_bmapi(NULL, ip, curr_off_fsb,
				  (xfs_filblks_t)(last_fsb - curr_off_fsb),
				  XFS_BMAPI_DELAY | XFS_BMAPI_WRITE,
				  &firstblock, 1, imap, &nimaps, NULL);
		if (error || (nimaps == 0)) {
			/*
			 * If we didn't get anything back, we must be
			 * out of space (or quota).  Break out of the loop and
			 * back out whatever we've done so far.
			 * bmapi with BMAPI_DELAY can return EDQUOT.
			 */
			break;
		}

		/*
		 * Count up the amount of space returned.
		 */
		for (i = 0; i < nimaps; i++) {
			ASSERT(imap[i].br_startblock != HOLESTARTBLOCK);
			count_fsb -= imap[i].br_blockcount;
			ASSERT(((long)count_fsb) >= 0);
			curr_off_fsb += imap[i].br_blockcount;
			ASSERT(curr_off_fsb <= last_fsb);
		}
	}

	/*
	 * Clear out any read-ahead info since the allocations may
	 * have made it invalid.
	 */
	XFS_INODE_CLEAR_READ_AHEAD(ip);
	
	if (count_fsb == 0) {
		/*
		 * We go it all, so get out of here.
		 */
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		return 0;
	}

	/*
	 * We didn't get it all, so back out anything new that we did
	 * create.  What we do is unmap all of the holes in the original
	 * map.  This will do at least one unnecessary unmap, but it's
	 * much simpler than being exact and it still works fine since
	 * we hold the inode lock all along.
	 *
	 * We know we can't get errors here, since the extent list has
	 * already been read in and we're only removing delayed allocation
	 * extents.
	 */
	unmap_offset_fsb = offset_fsb;
	imapp = &orig_imap[0];
	last_imapp = &orig_imap[orig_nimaps - 1];
	while (imapp <= last_imapp) {
		if (unmap_offset_fsb != imapp->br_startoff) {
			unmap_len_fsb = imapp->br_startoff - unmap_offset_fsb;
			firstblock = NULLFSBLOCK;
			(void) xfs_bunmapi(NULL, ip, unmap_offset_fsb,
					    unmap_len_fsb, 0, 1, &firstblock,
					    NULL, NULL);
		}
		unmap_offset_fsb = imapp->br_startoff + imapp->br_blockcount;
		if (imapp == last_imapp) {
			if (unmap_offset_fsb < (offset_fsb + count_fsb)) {
				/*
				 * There is a hole after the last original
				 * imap, so unmap it as well.
				 */
				unmap_len_fsb = (offset_fsb + count_fsb) -
					        unmap_offset_fsb;
				firstblock = NULLFSBLOCK;
				(void) xfs_bunmapi(NULL, ip,
						   unmap_offset_fsb,
						   unmap_len_fsb, 0, 1,
						   &firstblock, NULL, NULL);
			}
		}
		imapp++;
	}
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	if (!error) {
		error = XFS_ERROR(ENOSPC);
	}
	return error;
}

STATIC int
xfs_pathconf(
	bhv_desc_t	*bdp,
	int		cmd,
	long		*valp,
	struct cred 	*credp)
{
	int error = 0;

	switch (cmd) {
	case _PC_LINK_MAX:
		*valp = XFS_MAXLINK;
		break;
	case _PC_FILESIZEBITS:
		*valp = 64;
		break;
	default:
		error = fs_pathconf(bdp, cmd, valp, credp);
		break;
	}
	return error;
}

#ifdef DATAPIPE
/*
 * xfs_fspe_dioinfo: called by file system pipe end.
 */
int 
xfs_fspe_dioinfo(
        struct vnode *vp,
	struct dioattr *da)
{
	bhv_desc_t    *bdp;
	xfs_inode_t   *ip;
	xfs_mount_t   *mp;

	bdp = vp->v_fbhv;
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	/* It's a copy of the code in xfs_fcntl - F_DIOINFO cmd */
	
	ASSERT(scache_linemask != 0);
#ifdef R10000_SPECULATION_WAR
	da->d_mem = _PAGESZ;
#else
	da->d_mem = scache_linemask + 1;
#endif

	da->d_miniosz = mp->m_sb.sb_blocksize;
	da->d_maxiosz = XFS_FSB_TO_B(mp,
				    XFS_B_TO_FSBT(mp, ctob(v.v_maxdmasz - 1)));

	return 0;
}
#endif /* DATAPIPE */

/*
 * xfs_get_uiosize - get uio size info
 */
int
xfs_get_uiosize(
	xfs_mount_t	*mp, 
	xfs_inode_t	*ip, 
	struct biosize	*bs,
	cred_t		*credp)
{
	int error;

	if (error = xfs_iaccess(ip, IREAD, credp))
		return error;

	xfs_ilock(ip, XFS_ILOCK_SHARED);

	bs->biosz_flags = (ip->i_flags & XFS_IUIOSZ) ? 1 : 0;
	bs->biosz_read = ip->i_readio_log;
	bs->biosz_write = ip->i_writeio_log;
	bs->dfl_biosz_read = mp->m_readio_log;
	bs->dfl_biosz_write = mp->m_writeio_log;

	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	return 0;
}

/*
 * xfs_set_biosize
 *
 */
int
xfs_set_uiosize(
	xfs_mount_t	*mp, 
	xfs_inode_t	*ip, 
	uint		flags,
	int		read_iosizelog,
	int		write_iosizelog,
	cred_t		*credp)
{
	int	max_iosizelog;
	int	min_iosizelog;
	int	memlimit;
	int	error;

	if (error = xfs_iaccess(ip, IWRITE, credp))
		return error;

	memlimit = NBPP;

	switch (memlimit) {
	case 4096:
		memlimit = 12;
		break;
	case 16384:
		memlimit = 14;
		break;
	default:
		return XFS_ERROR(EINVAL);
	}

	if (flags == 2) {
		/*
		 * reset the io sizes to the filesystem default values.
		 * leave the maxio size alone since it won't have changed.
		 */
		xfs_ilock(ip, XFS_ILOCK_EXCL);

		ip->i_readio_log = mp->m_readio_log;
		ip->i_readio_blocks = 1 << (int) (ip->i_readio_log -
						mp->m_sb.sb_blocklog);
		ip->i_writeio_log = mp->m_writeio_log;
		ip->i_writeio_blocks = 1 << (int) (ip->i_writeio_log -
						mp->m_sb.sb_blocklog);
		ip->i_flags &= ~XFS_IUIOSZ;

		xfs_iunlock(ip, XFS_ILOCK_EXCL);

		return 0;
	}

	/*
	 * set iosizes to specified values -- screen out bogus values
	 */
	max_iosizelog = MAX(read_iosizelog, write_iosizelog);
	min_iosizelog = MIN(read_iosizelog, write_iosizelog);

	/*
	 * new values can't be less than a page, less than the filesystem
	 * blocksize, or out of the range of tested values
	 */
	if (max_iosizelog < mp->m_sb.sb_blocklog ||
	    max_iosizelog < memlimit ||
	    min_iosizelog < mp->m_sb.sb_blocklog ||
	    min_iosizelog < memlimit ||
	    min_iosizelog < XFS_MIN_IO_LOG ||
	    max_iosizelog > XFS_MAX_IO_LOG ||
	    flags >= 2)
		return XFS_ERROR(EINVAL);

	xfs_ilock(ip, XFS_ILOCK_EXCL);

	if (!(ip->i_flags & XFS_IUIOSZ)) {
		ip->i_readio_log = (uchar_t) read_iosizelog;
		ip->i_readio_blocks = 1 << (int) (ip->i_readio_log -
						mp->m_sb.sb_blocklog);
		ip->i_writeio_log = (uchar_t) write_iosizelog;
		ip->i_writeio_blocks = 1 << (int) (ip->i_writeio_log -
						mp->m_sb.sb_blocklog);
		ip->i_flags |= XFS_IUIOSZ;
		ip->i_max_io_log = MAX(max_iosizelog, ip->i_max_io_log);
	} else {
		/*
		 * if inode already has non-default values set,
		 * only allow the values to get smaller unless
		 * explictly overridden (flags == 1)
		 */
		if (read_iosizelog < ip->i_readio_log || flags == 1) {
			ip->i_readio_log = (uchar_t) read_iosizelog;
			ip->i_readio_blocks = 1 << (int) (ip->i_readio_log -
							mp->m_sb.sb_blocklog);
			ip->i_max_io_log = MAX(max_iosizelog, ip->i_max_io_log);
		}
		if (write_iosizelog < ip->i_writeio_log || flags == 1) {
			ip->i_writeio_log = (uchar_t) write_iosizelog;
			ip->i_writeio_blocks = 1 << (int) (ip->i_writeio_log -
							mp->m_sb.sb_blocklog);
			ip->i_max_io_log = MAX(max_iosizelog, ip->i_max_io_log);
		}
	}

	xfs_iunlock(ip, XFS_ILOCK_EXCL);

	return 0;
}


/*
 * xfs_fcntl
 */
/*ARGSUSED*/
STATIC int
xfs_fcntl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flags,
	off_t		offset,
	cred_t		*credp,
	rval_t		*rvalp)
{
	int			error = 0;
	xfs_mount_t		*mp;
	struct flock		bf;
	struct irix5_flock	i5_bf;
	vnode_t 		*vp;
	xfs_inode_t		*ip;
	struct biosize		bs;
	struct dioattr		da;
	struct fsxattr		fa;
	vattr_t			va;
	struct fsdmidata	d;
	extern int		scache_linemask;

	vp = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_fcntl", (inst_t *)__return_address);
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);
	
	switch (cmd) {
#ifdef DATAPIPE
	case F_GETOPS:
		fspe_get_ops(arg);
		break;
#endif

	case F_DIOINFO:
                /*
		 * We align to the secondary cache line size so that we
		 * don't have to worry about nasty writeback caches on
		 * I/O incoherent machines.  Making this less than a page
		 * requires setting the maximum I/O size to 1 page less
		 * than maxdmasz.  This is for the case of a maximum
		 * size I/O that is not page aligned.  It requires the
		 * maximum size plus 1 pages.
		 *
		 * Note: there are three additional pieced of code that
		 * replicate F_DIOINFO: xfs_fspe_dioinfo(),
		 * xfs_dm_get_dioinfo(), efs_fspe_dioinfo. 
		 * They are *not* identical
                 */
		ASSERT(scache_linemask != 0);
		da.d_miniosz = mp->m_sb.sb_blocksize;

#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000()) {
			da.d_mem = _PAGESZ;
			da.d_miniosz = ptob(btopr(mp->m_sb.sb_blocksize));
		} else
			da.d_mem = scache_linemask + 1;
#elif R10000_SPECULATION_WAR	/* makes tlb invalidate during dma more
	effective, by decreasing the likelihood of a valid reference in the
	same page as dma user address space; leaving the tlb invalid avoids
	the speculative reference. We return the more stringent
	"requirements" on the fcntl(), but do *NOT* enforced them
	in the read/write code, to be sure we don't break apps... */
		da.d_mem = _PAGESZ;
#else
		da.d_mem = scache_linemask + 1;
#endif

		/*
		 * this only really needs to be BBSIZE.
		 * it is set to the file system block size to
		 * avoid having to do block zeroing on short writes.
		 */
		da.d_maxiosz = XFS_FSB_TO_B(mp,
				    XFS_B_TO_FSBT(mp, ctob(v.v_maxdmasz - 1)));

		if (copyout(&da, arg, sizeof(da))) {
			error = XFS_ERROR(EFAULT);
		}
		break;

	case F_FSGETXATTR:
		va.va_mask = AT_XFLAGS|AT_EXTSIZE|AT_NEXTENTS;
		error = xfs_getattr(bdp, &va, 0, credp);
		if (error)
			break;
		fa.fsx_xflags = va.va_xflags;
		fa.fsx_extsize = va.va_extsize;
		fa.fsx_nextents = va.va_nextents;
		if (copyout(&fa, arg, sizeof(fa)))
			error = XFS_ERROR(EFAULT);
		break;

	case F_FSGETXATTRA:
		va.va_mask = AT_XFLAGS|AT_EXTSIZE|AT_ANEXTENTS;
		error = xfs_getattr(bdp, &va, 0, credp);
		if (error)
			break;
		fa.fsx_xflags = va.va_xflags;
		fa.fsx_extsize = va.va_extsize;
		fa.fsx_nextents = va.va_anextents;
		if (copyout(&fa, arg, sizeof(fa)))
			error = XFS_ERROR(EFAULT);
		break;

	case F_FSSETXATTR: {
		int	attr_flags;

		if (copyin(arg, &fa, sizeof(fa))) {
			error = XFS_ERROR(EFAULT);
			break;
		}
		va.va_mask = AT_XFLAGS | AT_EXTSIZE;
		va.va_xflags = fa.fsx_xflags;
		va.va_extsize = fa.fsx_extsize;
		attr_flags = ( flags&(FNDELAY|FNONBLOCK) ) ? ATTR_NONBLOCK : 0;
		error = xfs_setattr(bdp, &va, attr_flags, credp);
		break;
	}

	case F_GETBMAP:
	case F_GETBMAPA: {
		struct	getbmap	bm;
		int		iflags;

		if (copyin(arg, &bm, sizeof(bm))) {
			error = XFS_ERROR(EFAULT);
			break;
		}
		if (bm.bmv_count < 2) {
			error = XFS_ERROR(EINVAL);
			break;
		}

		iflags = (cmd == F_GETBMAPA ? BMV_IF_ATTRFORK : 0);
		if (flags&FINVIS)
			iflags |= BMV_IF_NO_DMAPI_READ;
		error = xfs_getbmap(bdp, &bm, (struct getbmap *)arg + 1, iflags);

		if (!error && copyout(&bm, arg, sizeof(bm))) {
			error = XFS_ERROR(EFAULT);
		}
		break;
	}
	case F_GETBMAPX: {
		struct	getbmapx	bmx;
		struct	getbmap		bm;
		int			iflags;


		if (copyin(arg, &bmx, sizeof(bmx))) {
			error = XFS_ERROR(EFAULT);
			break;
		}
		if (bmx.bmv_count < 2) {
			error = XFS_ERROR(EINVAL);
			break;
		}
		/* Map input getbmapx structure to a getbmap
		 * structure for xfs_getbmap.
		 */
		GETBMAP_CONVERT(bmx,bm);

		iflags = bmx.bmv_iflags;
		if (iflags & (~BMV_IF_VALID)) {
			error = XFS_ERROR(EINVAL);
			break;
		}
		iflags |= BMV_IF_EXTENDED;
		error = xfs_getbmap(bdp, &bm, (struct getbmapx *)arg + 1, iflags);
		if (error)
			break;
		GETBMAP_CONVERT(bm,bmx);
		if (copyout(&bmx, arg, sizeof(bmx))) {
			error = XFS_ERROR(EFAULT);
		}
		break;
	}
	case F_FSSETDM:
		if (copyin(arg, &d, sizeof d)) {
			error = XFS_ERROR (EFAULT);
			break;
		}
		error = xfs_set_dmattrs(bdp, d.fsd_dmevmask, d.fsd_dmstate,
			credp);
		break;

	case F_ALLOCSP:
	case F_FREESP:

	case F_RESVSP:
	case F_UNRESVSP:

	case F_ALLOCSP64:
	case F_FREESP64:

	case F_RESVSP64:
	case F_UNRESVSP64: {
		int	attr_flags;

		/* cmd = cmd; XXX */
		if ((flags & FWRITE) == 0) {
			error = XFS_ERROR(EBADF);
		} else if (vp->v_type != VREG) {
			error = XFS_ERROR(EINVAL);
		} else if (vp->v_flag & VISSWAP) {
			error = XFS_ERROR(EACCES);
#if _MIPS_SIM == _ABI64
		} else if (ABI_IS_IRIX5_64(get_current_abi())) {
			if (copyin((caddr_t)arg, &bf, sizeof bf)) {
				error = XFS_ERROR(EFAULT);
				break;
			}
#endif
		} else if (cmd == F_ALLOCSP64 || cmd == F_FREESP64   ||
			   cmd == F_RESVSP64  || cmd == F_UNRESVSP64 || 
			   ABI_IS_IRIX5_N32(get_current_abi())) {
			/* 
			 * The n32 flock structure is the same size as the
			 * o32 flock64 structure. So the copyin_xlate
			 * with irix5_n32_to_flock works here.
			 */
			if (COPYIN_XLATE((caddr_t)arg, &bf, sizeof bf,
					 irix5_n32_to_flock,
					 get_current_abi(), 1)) {
				error = XFS_ERROR(EFAULT);
				break;
			}
		} else {
			if (copyin((caddr_t)arg, &i5_bf, sizeof i5_bf)) {
				error = XFS_ERROR(EFAULT);
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
		attr_flags = ( flags&(FNDELAY|FNONBLOCK) ) ? ATTR_NONBLOCK : 0;
		if (flags&FINVIS)
			attr_flags |= ATTR_DMI;
		if (!error) {
			error = xfs_change_file_space(bdp, cmd, &bf, offset,
						      credp, attr_flags);
		}
		break;
	}

#ifndef SIM
	case F_DMAPI:
		error = xfs_dm_fcntl(bdp, arg, flags, offset, credp, rvalp);
		break;
#endif

	case F_SETBIOSIZE:
		if (copyin(arg, &bs, sizeof(struct biosize))) {
			error = XFS_ERROR(EFAULT);
			break;
		}
		error = xfs_set_uiosize(mp, ip, bs.biosz_flags, bs.biosz_read,
					bs.biosz_write, credp);
		break;

	case F_GETBIOSIZE:
		error = xfs_get_uiosize(mp, ip, &bs, credp);
		if (copyout(&bs, arg, sizeof(struct biosize))) {
			error = XFS_ERROR(EFAULT);
			break;
		}
		break;

	default:
		error = XFS_ERROR(EINVAL);
		break;
	}
	return error;
}

int
xfs_set_dmattrs (
	bhv_desc_t	*bdp,
	u_int		evmask,
	u_int16_t	state,
	cred_t		*credp)
{
        xfs_inode_t     *ip;
	xfs_trans_t	*tp;
	xfs_mount_t	*mp;
	int		error;

	if (!cap_able_cred(credp, CAP_DEVICE_MGT))
		return XFS_ERROR(EPERM);

        ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	tp = xfs_trans_alloc(mp, XFS_TRANS_SET_DMATTRS);
	error = xfs_trans_reserve(tp, 0, XFS_ICHANGE_LOG_RES (mp), 0, 0, 0);
	if (error) {
		xfs_trans_cancel(tp, 0);
		return error;
	}
        xfs_ilock(ip, XFS_ILOCK_EXCL);
        xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);

	ip->i_d.di_dmevmask = evmask;
	ip->i_d.di_dmstate  = state;

	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	IHOLD(ip);
	error = xfs_trans_commit(tp, 0, NULL);

	return error;
}

#endif	/* !SIM */

/*
 * xfs_reclaim
 */
STATIC int
xfs_reclaim(
	bhv_desc_t	*bdp,
	int		flag)
{
	xfs_inode_t	*ip;
	xfs_fsize_t	last_byte;
	int		locked;
	int		error;
	vnode_t 	*vp;

	vp = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_reclaim", (inst_t *)__return_address);
	ASSERT(!VN_MAPPED(vp));
	ip = XFS_BHVTOI(bdp);

	ASSERT(ip->i_queued_bufs >= 0);
	locked = 0;
	
	/*
	 * If this is not an unmount (flag == 0) and the inode's data
	 * still needs to be flushed, then we do not allow
	 * the inode to be reclaimed.  This is to avoid many different
	 * deadlocks.
	 *
	 * Doing the VOP_FLUSHINVAL_PAGES() can cause
	 * us to wait in the buffer cache.  We can be called here via
	 * vn_alloc() from xfs_iget().  We can be holding any number of
	 * locks at that point in the middle of a transaction, so we
	 * can't do anything that might need log space or the locks we
	 * might be holding.  Flushing our buffers can require log space
	 * to allocate the space for delayed allocation extents underlying
	 * them.  If the transaction we're already in has all the log
	 * space, then we won't be able to get any more and we'll hang.
	 *
	 * Not allowing the inode to be reclaimed if it has dirty data
	 * also prevents memory deadlocks where it is vhand calling here
	 * via the vnode shake routine.  Since our dirty data might be
	 * delayed allocation dirty data which will require us to allocate
	 * memory to flush, we can't do this from vhand.
	 *
	 * It is OK to return an error here.  The vnode cache will just
	 * come back later.
	 *
	 * XXXajs Distinguish vhand from vn_alloc and fail vhand case
	 * if the inode is dirty.  This will prevent deadlocks where the
	 * process with the inode buffer locked needs memory.  We can't
	 * always fail when the inode is dirty because then we don't
	 * reclaim enough.  The vnode cache then grows far too large.
	 */
	if (!(flag & FSYNC_INVAL)) {
		if (VN_DIRTY(vp) || (ip->i_queued_bufs > 0)) {
			return XFS_ERROR(EAGAIN);
		}
		if (!xfs_ilock_nowait(ip, XFS_ILOCK_EXCL)) {
			return XFS_ERROR(EAGAIN);
		}
		if (!xfs_iflock_nowait(ip)) {
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			return XFS_ERROR(EAGAIN);
		}
		if ((ip->i_itemp != NULL) &&
		    ((ip->i_itemp->ili_format.ilf_fields != 0) ||
		     (ip->i_itemp->ili_last_fields != 0))) {
			(void) xfs_iflush(ip, XFS_IFLUSH_DELWRI);
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			return XFS_ERROR(EAGAIN);
		}
		locked = 1;
	}
	if ((ip->i_d.di_mode & IFMT) == IFREG) {
		if (ip->i_d.di_size > 0) {
			/*
			 * Flush and invalidate any data left around that is
			 * a part of this file.
			 *
			 * Get the inode's i/o lock so that buffers are pushed
			 * out while holding the proper lock.  We can't hold
			 * the inode lock here since flushing out buffers may
			 * cause us to try to get the lock in xfs_strategy().
			 *
			 * We don't have to call remapf() here, because there
			 * cannot be any mapped file references to this vnode
			 * since it is being reclaimed.
			 */
			if (locked) {
				xfs_ifunlock(ip);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				locked = 0;
			}
			xfs_ilock(ip, XFS_IOLOCK_EXCL);
			last_byte = xfs_file_last_byte(ip);
			
			/*
			 * If we hit an IO error, we need to make sure that the
			 * buffer and page caches of file data for
			 * the file are tossed away. We don't want to use 
			 * VOP_FLUSHINVAL_PAGES here because we don't want dirty
			 * pages to stay attached to the vnode, but be
			 * marked P_BAD. pdflush/vnode_pagebad
			 * hates that.
			 */
			if (!XFS_FORCED_SHUTDOWN(ip->i_mount)) {
				VOP_FLUSHINVAL_PAGES(vp, 0, last_byte - 1, FI_NONE);
			} else {
				VOP_TOSS_PAGES(vp, 0, XFS_MAX_FILE_OFFSET, FI_NONE);
			}
			
			ASSERT(!VN_DIRTY(vp) && 
			       (ip->i_queued_bufs == 0) &&
			       (vp->v_pgcnt == 0) &&
			       (vp->v_buf == NULL));
			ASSERT(XFS_FORCED_SHUTDOWN(ip->i_mount) ||
			       ip->i_delayed_blks == 0);
			xfs_iunlock(ip, XFS_IOLOCK_EXCL);
		} else if (XFS_FORCED_SHUTDOWN(ip->i_mount)) {
			/* 
			 * di_size field may not be quite accurate if we're
			 * shutting down.
			 */
			VOP_TOSS_PAGES(vp, 0, XFS_MAX_FILE_OFFSET, FI_NONE);
			ASSERT(!VN_DIRTY(vp) && 
			       (ip->i_queued_bufs == 0) &&
			       (vp->v_pgcnt == 0) &&
			       (vp->v_buf == NULL));
		}
	}
#ifndef SIM
	dnlc_purge_vp(vp);
#endif	/* !SIM */
	/*
	 * If the inode is still dirty, then flush it out.  If the inode
	 * is not in the AIL, then it will be OK to flush it delwri as
	 * long as xfs_iflush() does not keep any references to the inode.
	 * We leave that decision up to xfs_iflush() since it has the
	 * knowledge of whether it's OK to simply do a delwri flush of
	 * the inode or whether we need to wait until the inode is
	 * pulled from the AIL.
	 * We get the flush lock regardless, though, just to make sure
	 * we don't free it while it is being flushed.
	 */
	if (!XFS_FORCED_SHUTDOWN(ip->i_mount)) {
		if (!locked) {
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			xfs_iflock(ip);
		}
		if (ip->i_update_core ||
		    ((ip->i_itemp != NULL) &&
		     (ip->i_itemp->ili_format.ilf_fields != 0))) {
			error = xfs_iflush(ip, XFS_IFLUSH_DELWRI_ELSE_SYNC);
			/*
			 * If we hit an error, typically because of filesystem
			 * shutdown, we don't need to let vn_reclaim to know
			 * because we're gonna reclaim the inode anyway.
			 */
			if (error) {
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				xfs_ireclaim(ip);
				return (0);
			}
			xfs_iflock(ip); /* synchronize with xfs_iflush_done */
		}
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		ASSERT(ip->i_update_core == 0);
		ASSERT(ip->i_itemp == NULL || 
		       ip->i_itemp->ili_format.ilf_fields == 0);
		ASSERT(!VN_DIRTY(vp) && (ip->i_queued_bufs == 0));
	} else if (locked) {
		/*
		 * We are not interested in doing an iflush if we're
		 * in the process of shutting down the filesystem forcibly.
		 * So, just reclaim the inode.
		 */
		xfs_ifunlock(ip);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
	}
	xfs_ireclaim(ip);
	return 0;
}

#ifndef SIM
/*
 * xfs_alloc_file_space()
 *      This routine allocates disk space for the given file.
 *
 *	If alloc_type == 0, this request is for an ALLOCSP type 
 *	request which will change the file size.  In this case, no
 *	DMAPI event will be generated by the call.  A TRUNCATE event
 *	will be generated later by xfs_setattr.
 *
 *	If alloc_type != 0, this request is for a RESVSP type 
 *	request, and a DMAPI DM_EVENT_WRITE will be generated if the
 *	lower block boundary byte address is less than the file's
 *	length.
 *
 * RETURNS:
 *       0 on success
 *      errno on error
 *
 */
STATIC int
xfs_alloc_file_space( 
	xfs_inode_t		*ip,
	off_t 			offset,
	off_t			len,
	int			alloc_type,
	int			attr_flags)
{
	xfs_filblks_t		allocated_fsb;
	xfs_filblks_t		allocatesize_fsb;
	int			committed;
	off_t			count;
	xfs_filblks_t		datablocks;
	int			error;
	xfs_fsblock_t		firstfsb;
	xfs_bmap_free_t		free_list;
	xfs_bmbt_irec_t		*imapp;
	xfs_bmbt_irec_t		imaps[1];
	xfs_mount_t		*mp;
	int			numrtextents;
	int			reccount;
	uint			resblks;
	int			rt; 
	int			rtextsize;
	xfs_fileoff_t		startoffset_fsb;
	xfs_trans_t		*tp;
	int			xfs_bmapi_flags;

	vn_trace_entry(XFS_ITOV(ip), "xfs_alloc_file_space",
		       (inst_t *)__return_address);
	
	mp = ip->i_mount;

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	/*
	 * determine if this is a realtime file
	 */
        if (rt = (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) != 0) {
		if (ip->i_d.di_extsize)
			rtextsize = ip->i_d.di_extsize;
		else
			rtextsize = mp->m_sb.sb_rextsize;
        } else
                rtextsize = 0;

	if (XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0))
				return error;
		}
	}

	if (len <= 0)
		return XFS_ERROR(EINVAL);

	count = len;
	error = 0;
	imapp = &imaps[0];
	reccount = 1;
	xfs_bmapi_flags = XFS_BMAPI_WRITE | (alloc_type ? XFS_BMAPI_PREALLOC : 0);
	startoffset_fsb	= XFS_B_TO_FSBT(mp, offset);
	allocatesize_fsb = XFS_B_TO_FSB(mp, count);

/*	Generate a DMAPI event if needed.	*/

	if (alloc_type != 0 && offset < ip->i_d.di_size &&
			(attr_flags&ATTR_DMI) == 0  &&
			DM_EVENT_ENABLED(XFS_MTOVFS(mp), ip, DM_EVENT_WRITE)) {
		off_t           end_dmi_offset;

		end_dmi_offset = offset+len;
		if (end_dmi_offset > ip->i_d.di_size)
			end_dmi_offset = ip->i_d.di_size;
		error = xfs_dm_send_data_event(DM_EVENT_WRITE, XFS_ITOBHV(ip),
			offset, end_dmi_offset - offset,
			0, NULL);
		if (error)
			return(error);
	}

	/*
	 * allocate file space until done or until there is an error	
 	 */
retry:
	while (allocatesize_fsb && !error) {
		/*
		 * determine if reserving space on
		 * the data or realtime partition.
		 */
		if (rt) {
			xfs_fileoff_t s, e;

			s = startoffset_fsb / rtextsize;
			s *= rtextsize;
			e = roundup(startoffset_fsb + allocatesize_fsb,
				rtextsize);
			numrtextents = (e - s) / mp->m_sb.sb_rextsize;
			datablocks = 0;
		} else {
			datablocks = allocatesize_fsb;
			numrtextents = 0;
		}

		/*
		 * allocate and setup the transaction
		 */
		tp = xfs_trans_alloc(mp, XFS_TRANS_DIOSTRAT);
		resblks = XFS_DIOSTRAT_SPACE_RES(mp, datablocks);
		error = xfs_trans_reserve(tp,
					  resblks,
					  XFS_WRITE_LOG_RES(mp),
					  numrtextents,
					  XFS_TRANS_PERM_LOG_RES,
					  XFS_WRITE_LOG_COUNT);

		/*
		 * check for running out of space
		 */
		if (error) {
			/*
			 * Free the transaction structure.
			 */
			ASSERT(error == ENOSPC || XFS_FORCED_SHUTDOWN(mp));
			xfs_trans_cancel(tp, 0);
			break;
		}
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		if (XFS_IS_QUOTA_ON(mp)) {
			if (xfs_trans_reserve_quota(tp, 
						    ip->i_udquot, 
						    ip->i_pdquot,
						    resblks, 0, 0)) {
				error = XFS_ERROR(EDQUOT);
				goto error1;
			}
		}	

		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);

		/*
		 * issue the bmapi() call to allocate the blocks
	 	 */
		XFS_BMAP_INIT(&free_list, &firstfsb);
		error = xfs_bmapi(tp, ip, startoffset_fsb, 
				  allocatesize_fsb, xfs_bmapi_flags,
				  &firstfsb, 0, imapp, &reccount,
				  &free_list);
		if (error) {
			goto error0;
		}

		/*
		 * complete the transaction
		 */
		error = xfs_bmap_finish(&tp, &free_list, firstfsb, &committed);
		if (error) {
			goto error0;
		}

		error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		XFS_INODE_CLEAR_READ_AHEAD(ip);
		if (error) {
			break;
		}

		allocated_fsb = imapp->br_blockcount;

		if (reccount == 0) {
			error = XFS_ERROR(ENOSPC);
			break;
		}

		startoffset_fsb += allocated_fsb;
		allocatesize_fsb -= allocated_fsb;
	}
dmapi_enospc_check:
#ifndef SIM
	if (error == ENOSPC && (attr_flags&ATTR_DMI) == 0 &&
	    DM_EVENT_ENABLED(XFS_MTOVFS(mp), ip, DM_EVENT_NOSPACE)) {

		error = dm_send_namesp_event(DM_EVENT_NOSPACE,
				XFS_ITOBHV(ip), DM_RIGHT_NULL, 
				XFS_ITOBHV(ip), DM_RIGHT_NULL,
				NULL, NULL, 0, 0, 0); /* Delay flag intentionally unused */
		if (error == 0) 
			goto retry;	/* Maybe DMAPI app. has made space */
		/* else fall through with error = xfs_dm_send_data_event result. */
	}
#endif

	return error;

 error0:
	xfs_bmap_cancel(&free_list);
 error1:
	xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_ABORT);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	goto dmapi_enospc_check;
}

/*
 * Zero file bytes between startoff and endoff inclusive.
 * The iolock is held exclusive and no blocks are buffered.
 */
STATIC int
xfs_zero_remaining_bytes(
	xfs_inode_t		*ip,
	off_t			startoff,
	off_t			endoff)
{
	buf_t			*bp;
	int			error;
	xfs_bmbt_irec_t		imap;
	off_t			lastoffset;
	xfs_mount_t		*mp;
	int			nimap;
	off_t			offset;
	xfs_fileoff_t		offset_fsb;

	mp = ip->i_mount;
	bp = ngetrbuf(mp->m_sb.sb_blocksize);
	ASSERT(!geterror(bp));

	if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
		bp->b_edev = mp->m_rtdev;
		bp->b_target = &mp->m_rtdev_targ;
	} else {
		bp->b_edev = mp->m_dev;
		bp->b_target = mp->m_ddev_targp;
	}

	for (offset = startoff; offset <= endoff; offset = lastoffset + 1) {
		offset_fsb = XFS_B_TO_FSBT(mp, offset);
		nimap = 1;
		error = xfs_bmapi(NULL, ip, offset_fsb, 1, 0, NULL, 0, &imap,
			&nimap, NULL);
		if (error || nimap < 1)
			break;
		ASSERT(imap.br_blockcount >= 1);
		ASSERT(imap.br_startoff == offset_fsb);
		lastoffset = XFS_FSB_TO_B(mp, imap.br_startoff + 1) - 1;
		if (lastoffset > endoff)
			lastoffset = endoff;
		if (imap.br_startblock == HOLESTARTBLOCK)
			continue;
		ASSERT(imap.br_startblock != DELAYSTARTBLOCK);
		if (imap.br_state == XFS_EXT_UNWRITTEN)
			continue;
		bp->b_flags &= ~(B_DONE | B_WRITE);
		bp->b_flags |= B_READ;
		bp->b_blkno = XFS_FSB_TO_DB(ip, imap.br_startblock);
		bp_dcache_wbinval(bp);
		xfsbdstrat(mp, bp); 
		if (error = iowait(bp))
			break;
		bzero(bp->b_un.b_addr +
			(offset - XFS_FSB_TO_B(mp, imap.br_startoff)),
		      lastoffset - offset + 1);
		bp->b_flags &= ~(B_DONE | B_READ);
		bp->b_flags |= B_WRITE;
		xfsbdstrat(mp, bp);
		if (error = iowait(bp))
			break;
	}
	nfreerbuf(bp);
	return error;
}

/*
 * xfs_free_file_space()
 *      This routine frees disk space for the given file.
 *
 *	This routine is only called by xfs_change_file_space
 *	for an UNRESVSP type call.
 *
 * RETURNS:
 *       0 on success
 *      errno on error
 *
 */
STATIC int
xfs_free_file_space( 
	xfs_inode_t		*ip,
	off_t 			offset,
	off_t			len,
	int			attr_flags)
{
	int			committed;
	int			done;
	off_t			end_dmi_offset;
	xfs_fileoff_t		endoffset_fsb;
	int			error;
	xfs_fsblock_t		firstfsb;
	xfs_bmap_free_t		free_list;
	off_t			ilen;
	xfs_bmbt_irec_t		imap;
	off_t			ioffset;
	xfs_extlen_t		mod;
	xfs_mount_t		*mp;
	int			nimap;
	uint			resblks;
	int			rounding;
	int			specrt;
	xfs_fileoff_t		startoffset_fsb;
	xfs_trans_t		*tp;

	vn_trace_entry(XFS_ITOV(ip), "xfs_free_file_space",
		       (inst_t *)__return_address);
	
	mp = ip->i_mount;

	if (XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0))
				return error;
		}
	}
	
	error = 0;
	if (len <= 0)	/* if nothing being freed */
		return error;
	specrt =
		(ip->i_d.di_flags & XFS_DIFLAG_REALTIME) &&
		!XFS_SB_VERSION_HASEXTFLGBIT(&mp->m_sb);
	startoffset_fsb	= XFS_B_TO_FSB(mp, offset);
	end_dmi_offset = offset + len;
	endoffset_fsb = XFS_B_TO_FSBT(mp, end_dmi_offset);

	if (offset < ip->i_d.di_size &&
	    (attr_flags&ATTR_DMI) == 0  &&
	    DM_EVENT_ENABLED(XFS_MTOVFS(mp), ip, DM_EVENT_WRITE)) {
		if (end_dmi_offset > ip->i_d.di_size)
			end_dmi_offset = ip->i_d.di_size;
		error = xfs_dm_send_data_event(DM_EVENT_WRITE, XFS_ITOBHV(ip),
			offset, end_dmi_offset - offset,
			AT_DELAY_FLAG(attr_flags), NULL);
		if (error)
			return(error);
	}

	xfs_ilock(ip, XFS_IOLOCK_EXCL);
	rounding = MAX(1 << mp->m_sb.sb_blocklog, NBPP);
	ilen = len + (offset & (rounding - 1));
	ioffset = offset & ~(rounding - 1);
	if (ilen & (rounding - 1))
		ilen = (ilen + rounding) & ~(rounding - 1);
	xfs_inval_cached_pages(ip, ioffset, ilen, NULL);
	/*
	 * Need to zero the stuff we're not freeing, on disk.
	 * If its specrt (realtime & can't use unwritten extents) then
	 * we actually need to zero the extent edges.  Otherwise xfs_bunmapi
	 * will take care of it for us.
	 */
	if (specrt) {
		nimap = 1;
		error = xfs_bmapi(NULL, ip, startoffset_fsb, 1, 0, NULL, 0,
			&imap, &nimap, NULL);
		if (error)
			return error;
		ASSERT(nimap == 0 || nimap == 1);
		if (nimap && imap.br_startblock != HOLESTARTBLOCK) {
			ASSERT(imap.br_startblock != DELAYSTARTBLOCK);
			mod = imap.br_startblock % mp->m_sb.sb_rextsize;
			if (mod)
				startoffset_fsb += mp->m_sb.sb_rextsize - mod;
		}
		nimap = 1;
		error = xfs_bmapi(NULL, ip, endoffset_fsb - 1, 1, 0, NULL, 0,
			&imap, &nimap, NULL);
		if (error)
			return error;
		ASSERT(nimap == 0 || nimap == 1);
		if (nimap && imap.br_startblock != HOLESTARTBLOCK) {
			ASSERT(imap.br_startblock != DELAYSTARTBLOCK);
			mod = (imap.br_startblock + 1) % mp->m_sb.sb_rextsize;
			if (mod)
				endoffset_fsb -= mod;
		}
	}
	if (done = endoffset_fsb <= startoffset_fsb)
		/*
		 * One contiguous piece to clear
		 */
		error = xfs_zero_remaining_bytes(ip, offset, offset + len - 1);
	else {
		/*
		 * Some full blocks, possibly two pieces to clear
		 */
		if (offset < XFS_FSB_TO_B(mp, startoffset_fsb))
			error = xfs_zero_remaining_bytes(ip, offset,
				XFS_FSB_TO_B(mp, startoffset_fsb) - 1);
		if (!error &&
		    XFS_FSB_TO_B(mp, endoffset_fsb) < offset + len)
			error = xfs_zero_remaining_bytes(ip,
				XFS_FSB_TO_B(mp, endoffset_fsb),
				offset + len - 1);
	}

	/*
	 * free file space until done or until there is an error	
 	 */
	resblks = XFS_DIOSTRAT_SPACE_RES(mp, 0);
	while (!error && !done) {

		/*
		 * allocate and setup the transaction
		 */
		tp = xfs_trans_alloc(mp, XFS_TRANS_DIOSTRAT);
		error = xfs_trans_reserve(tp,
					  resblks,
					  XFS_WRITE_LOG_RES(mp),
					  0,
					  XFS_TRANS_PERM_LOG_RES,
					  XFS_WRITE_LOG_COUNT);

		/*
		 * check for running out of space
		 */
		if (error) {
			/*
			 * Free the transaction structure.
			 */
			ASSERT(error == ENOSPC || XFS_FORCED_SHUTDOWN(mp));
			xfs_trans_cancel(tp, 0);
			break;
		}
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		if (XFS_IS_QUOTA_ON(mp)) {
			if (xfs_trans_reserve_quota(tp, 
						    ip->i_udquot, 
						    ip->i_pdquot,
						    resblks, 0, 0)) {
				error = XFS_ERROR(EDQUOT);
				goto error1;
			}
		}	

		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);

		/*
		 * issue the bunmapi() call to free the blocks
	 	 */
		XFS_BMAP_INIT(&free_list, &firstfsb);
		error = xfs_bunmapi(tp, ip, startoffset_fsb, 
				  endoffset_fsb - startoffset_fsb,
				  0, 2, &firstfsb, &free_list, &done);
		if (error) {
			goto error0;
		}

		/*
		 * complete the transaction
		 */
		error = xfs_bmap_finish(&tp, &free_list, firstfsb, &committed);
		if (error) {
			goto error0;
		}

		error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		XFS_INODE_CLEAR_READ_AHEAD(ip);
	}

	xfs_iunlock(ip, XFS_IOLOCK_EXCL);
	return error;

 error0:
	xfs_bmap_cancel(&free_list);
 error1:
	xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_ABORT);
	xfs_iunlock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	return error;
}

/*
 * xfs_change_file_space()
 *      This routine allocates or frees disk space for the given file.
 *      The user specified parameters are checked for alignment and size
 *      limitations.
 *
 * RETURNS:
 *       0 on success
 *      errno on error
 *
 */
STATIC int
xfs_change_file_space( 
	bhv_desc_t	*bdp,
	int		cmd,
	flock_t 	*bf,
	off_t 		offset,
	cred_t  	*credp,
	int		attr_flags)
{
	int		clrprealloc;
	int		error;
	xfs_fsize_t	fsize;
	xfs_inode_t	*ip;
	xfs_mount_t	*mp;
	int		setprealloc;
	off_t		startoffset;
	xfs_trans_t	*tp;
	vattr_t		va;
	vnode_t		*vp;

	vp = BHV_TO_VNODE(bdp);
	vn_trace_entry(vp, "xfs_change_file_space", (inst_t *)__return_address);
	
	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	/*
	 * must be a regular file and have write permission
	 */
	if (vp->v_type != VREG)
		return XFS_ERROR(EINVAL);

	xfs_ilock(ip, XFS_ILOCK_SHARED);
	if (error = xfs_iaccess(ip, IWRITE, credp)) {
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
		return error;
	}
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	if (error = convoff(vp, bf, 0, offset, XFS_MAX_FILE_OFFSET, credp))
		return error;
	startoffset = bf->l_start;
	fsize = ip->i_d.di_size;
	/*
	 * F_RESVSP and F_UNRESVSP will reserve or unreserve file space.
	 * these calls do NOT zero the data space allocated
	 * to the file, nor do they change the file size.
 	 *
	 * F_ALLOCSP and F_FREESP will allocate and free file space.
	 * these calls cause the new file data to be zeroed and the file
	 * size to be changed.
	 */
	setprealloc = clrprealloc = 0;
	switch (cmd) {
	case F_RESVSP:
	case F_RESVSP64:
		error = xfs_alloc_file_space(ip, startoffset, bf->l_len, 1, attr_flags);
		if (error)
			return error;
		setprealloc = 1;
		break;
	case F_UNRESVSP:
	case F_UNRESVSP64:
		if (error = xfs_free_file_space(ip, startoffset, bf->l_len,
									attr_flags))
			return error;
		break;
	case F_ALLOCSP:
	case F_ALLOCSP64:
	case F_FREESP:
	case F_FREESP64:
		if (startoffset > fsize) {
			error = xfs_alloc_file_space(ip, fsize,
					startoffset - fsize, 0, attr_flags);
			if (error)
				break;
		}
		va.va_mask = AT_SIZE;
		va.va_size = startoffset;
		error = xfs_setattr(bdp, &va, attr_flags, credp);
		if (error)
			return error;
		clrprealloc = 1;
		break;
	default:
		ASSERT(0);
		return XFS_ERROR(EINVAL);
	}

	/*
	 * update the inode timestamp, mode, and prealloc flag bits
	 */
	tp = xfs_trans_alloc(mp, XFS_TRANS_WRITEID);
	if (error = xfs_trans_reserve(tp, 0, XFS_WRITEID_LOG_RES(mp),
				      0, 0, 0)) {
		/* ASSERT(0); */
		xfs_trans_cancel(tp, 0);
		return error;
	}
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	ip->i_d.di_mode &= ~ISUID;

	/*
	 * Note that we don't have to worry about mandatory
	 * file locking being disabled here because we only
	 * clear the ISGID bit if the Group execute bit is
	 * on, but if it was on then mandatory locking wouldn't
	 * have been enabled.
	 */
	if (ip->i_d.di_mode & (IEXEC >> 3))
		ip->i_d.di_mode &= ~ISGID;
	xfs_ichgtime(ip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	if (setprealloc)
		ip->i_d.di_flags |= XFS_DIFLAG_PREALLOC;
	else if (clrprealloc)
		ip->i_d.di_flags &= ~XFS_DIFLAG_PREALLOC;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	xfs_trans_set_sync(tp);
	error = xfs_trans_commit(tp, 0, NULL);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

/*ARGSUSED*/
STATIC int
xfs_ioctl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flag,
	cred_t		*credp,
	int		*rvalp,
        struct vopbd    *vbds)
{
	return XFS_ERROR(ENOTTY);
}

/*
 * print out error describing the problem with the fs
 *
 *
 * type 1 indicates the file system ran out of data space
 * type 2 indicates the file system ran out of realtime space
 *
 */
void
xfs_error(
	xfs_mount_t	*mp,
	int		type)
{
	dev_t	dev;
	
	switch (type) {
	case 1:
		dev = mp->m_dev;
		prdev("Process [%s] ran out of disk space",
		      dev, get_current_name());
		break;
	case 2:
		dev = mp->m_rtdev;
		prdev("Process [%s] ran out of rt disk space",
		      dev, get_current_name());
		break;
	}
}
#endif	/* !SIM */

#ifdef SIM

vnodeops_t xfs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	(vop_open_t)fs_noerr,
	(vop_close_t)fs_nosys,
	(vop_read_t)fs_nosys,
	(vop_write_t)fs_nosys,
	(vop_ioctl_t)fs_nosys,
	(vop_setfl_t)fs_noerr,
	(vop_getattr_t)fs_nosys,
	(vop_setattr_t)fs_nosys,
	(vop_access_t)fs_nosys,
	(vop_lookup_t)fs_nosys,
	(vop_create_t)fs_nosys,
	(vop_remove_t)fs_nosys,
	(vop_link_t)fs_nosys,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	(vop_readdir_t)fs_nosys,
	(vop_symlink_t)fs_nosys,
	(vop_readlink_t)fs_nosys,
	(vop_fsync_t)fs_nosys,
	xfs_inactive,
	(vop_fid_t)fs_nosys,
	(vop_fid2_t)fs_nosys,
	(vop_rwlock_t)fs_nosys,
	(vop_rwunlock_t)fs_nosys,
	(vop_seek_t)fs_nosys,
	(vop_cmp_t)fs_nosys,
	(vop_frlock_t)fs_nosys,
	(vop_realvp_t)fs_nosys,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_nosys,
	(vop_map_t)fs_nodev,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	(vop_poll_t)fs_nosys,
	(vop_dump_t)fs_nosys,
	(vop_pathconf_t)fs_nosys,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	xfs_reclaim,
	(vop_attr_get_t)fs_nosys,
	(vop_attr_set_t)fs_nosys,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	(vop_cover_t)fs_nosys,
	(vop_link_removed_t)fs_nosys,
	(vop_vnode_change_t)fs_nosys,
	(vop_ptossvp_t)fs_nosys,
	(vop_pflushinvalvp_t)fs_nosys,
	(vop_pflushvp_t)fs_nosys,
	(vop_pinvalfree_t)fs_nosys,
	(vop_sethole_t)fs_nosys,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	(vop_strgetmsg_t)fs_nosys,
	(vop_strputmsg_t)fs_nosys,
};

#else

vnodeops_t xfs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	xfs_open,
	xfs_close,
	xfs_read,
	xfs_write,
	xfs_ioctl,
	(vop_setfl_t)fs_noerr,
	xfs_getattr,
	xfs_setattr,
	xfs_access,
	xfs_lookup,
	xfs_create,
	xfs_remove,
	xfs_link,
	xfs_rename,
	xfs_mkdir,
	xfs_rmdir,
	xfs_readdir,
	xfs_symlink,
	xfs_readlink,
	xfs_fsync,
	xfs_inactive,
	xfs_fid,
	xfs_fid2,
	xfs_rwlock,
	xfs_rwunlock,
	xfs_seek,
	fs_cmp,
	xfs_frlock,
	(vop_realvp_t)fs_nosys,
	xfs_bmap,
	xfs_strategy,
	(vop_map_t)fs_noerr,
	(vop_addmap_t)fs_noerr,
	(vop_delmap_t)fs_noerr,
	fs_poll,
	(vop_dump_t)fs_nosys,
	xfs_pathconf,
	xfs_allocstore,
	xfs_fcntl,
	xfs_reclaim,
	xfs_attr_get,
	xfs_attr_set,
	xfs_attr_remove,
	xfs_attr_list,
	fs_cover,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	fs_tosspages,
	fs_flushinval_pages,
	fs_flush_pages,
	fs_invalfree_pages,
	fs_pages_sethole,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)xfs_vop_readbuf,
	fs_strgetmsg,
	fs_strputmsg,
};

#endif /* SIM */
