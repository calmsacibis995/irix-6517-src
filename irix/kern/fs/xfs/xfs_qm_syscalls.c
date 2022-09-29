#ident "$Revision: 1.30 $"

#include <sys/param.h>
#include <sys/sysinfo.h>
#include <sys/buf.h>
#include <sys/ksa.h>
#include <sys/vnode.h>
#include <sys/pfdat.h>
#include <sys/uuid.h>
#include <sys/capability.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/vfs.h>
#include <sys/atomic_ops.h>
#include <sys/systm.h>
#include <sys/ktrace.h>
#include <sys/quota.h>
#include <limits.h>
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
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_btree.h"
#include "xfs_bmap.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_buf_item.h"
#include "xfs_da_btree.h"
#include "xfs_inode.h"
#include "xfs_error.h"
#include "xfs_itable.h"
#include "xfs_trans_priv.h"
#include "xfs_bit.h"
#include "xfs_clnt.h"
#include "xfs_quota.h"
#include "xfs_dqblk.h"
#include "xfs_dquot_item.h"
#include "xfs_dquot.h"
#include "xfs_qm.h"
#include "xfs_quota_priv.h"

extern int 	xfs_fstype;

STATIC int	xfs_qm_scall_trunc_qfiles(xfs_mount_t *, uint);
STATIC int	xfs_qm_scall_getquota(xfs_mount_t *, xfs_dqid_t, uint, caddr_t);
STATIC int	xfs_qm_scall_getqstat(xfs_mount_t *, caddr_t);
STATIC int	xfs_qm_scall_setqlim(xfs_mount_t *, xfs_dqid_t, uint, caddr_t);
STATIC int	xfs_qm_scall_quotaon(xfs_mount_t *, uint);
STATIC int	xfs_qm_scall_qwarn(xfs_mount_t *, xfs_dqid_t, caddr_t);
STATIC int	xfs_qm_log_quotaoff(xfs_mount_t *, xfs_qoff_logitem_t **, uint);
STATIC int	xfs_qm_log_quotaoff_end(xfs_mount_t *, xfs_qoff_logitem_t *,
					uint);
STATIC uint	xfs_qm_import_flags(uint *);
STATIC uint	xfs_qm_export_flags(uint);
STATIC uint	xfs_qm_import_qtype_flags(uint *);
STATIC uint	xfs_qm_export_qtype_flags(uint);
STATIC void	xfs_qm_export_dquot(xfs_mount_t *, xfs_disk_dquot_t *,
				    struct fs_disk_quota *);


/*
 * The main distribution switch of all XFS quotactl system calls.
 */
int
xfs_quotactl(
	struct bhv_desc *bdp,
	int		cmd,
	int		id,
	caddr_t		addr)
{
	xfs_mount_t	*mp;
	int 		error;
	struct vfs	*vfsp;

        mp = XFS_BHVTOM(bdp);
	vfsp = bhvtovfs(bdp);

#ifdef DEBUG		
	/*
	 * Internal quota accounting check for debugging
	 */
	if (cmd == 50) 
		return (xfs_qm_internalqcheck(mp));
#endif
	if (addr == NULL && cmd != Q_SYNC)
		return XFS_ERROR(EINVAL);
	if (id < 0 && cmd != Q_SYNC)
		return XFS_ERROR(EINVAL);

	/*
	 * The following commands are valid even when quotaoff.
	 */
	switch (cmd) {
	      	/* 
		 * truncate quota files. quota must be off.
		 */
	      case Q_QUOTARM:
		if (XFS_IS_QUOTA_ON(mp) || addr == NULL)
			return XFS_ERROR(EINVAL);
		if (vfsp->vfs_flag & VFS_RDONLY)
			return XFS_ERROR(EROFS);
		return (xfs_qm_scall_trunc_qfiles(mp, 
				       xfs_qm_import_qtype_flags((uint *)addr)));
		/*
		 * Get quota status information.
		 */
	      case Q_GETQSTAT:

		return (xfs_qm_scall_getqstat(mp, addr));

		/*
		 * QUOTAON for root f/s and quota enforcement on others..
		 * Quota accounting for non-root f/s's must be turned on
		 * at mount time.
		 */
	      case Q_QUOTAON:
		if (addr == NULL)
			return XFS_ERROR(EINVAL);
		if (vfsp->vfs_flag & VFS_RDONLY)
			return XFS_ERROR(EROFS);
		return (xfs_qm_scall_quotaon(mp,
					  xfs_qm_import_flags((uint *)addr)));
	      case Q_QUOTAOFF:
		if (vfsp->vfs_flag & VFS_RDONLY)
			return XFS_ERROR(EROFS);
		if (mp->m_dev == rootdev) {
			return (xfs_qm_scall_quotaoff(mp,
					    xfs_qm_import_flags((uint *)addr),
					    B_FALSE));
		}
		break;
		
	      default:
		break;
	}

	if (! XFS_IS_QUOTA_ON(mp))
		return XFS_ERROR(ESRCH);

	switch (cmd) {
	      case Q_QUOTAOFF:
		if (vfsp->vfs_flag & VFS_RDONLY)
			return XFS_ERROR(EROFS);
		error = xfs_qm_scall_quotaoff(mp,
					    xfs_qm_import_flags((uint *)addr),
					    B_FALSE);
		break;

		/* 
		 * Defaults to XFS_GETUQUOTA. 
		 */
	      case Q_XGETQUOTA:
		error = xfs_qm_scall_getquota(mp, (xfs_dqid_t)id, XFS_DQ_USER, 
					addr);
		break;
		/*
		 * Set limits, both hard and soft. Defaults to Q_SETUQLIM.
		 */
	      case Q_XSETQLIM:
		if (vfsp->vfs_flag & VFS_RDONLY)
			return XFS_ERROR(EROFS);
		error = xfs_qm_scall_setqlim(mp, (xfs_dqid_t)id, XFS_DQ_USER,
					     addr);
		break;
#ifndef _NOPROJQUOTAS
	       case Q_XSETPQLIM:
		if (vfsp->vfs_flag & VFS_RDONLY)
			return XFS_ERROR(EROFS);
		error = xfs_qm_scall_setqlim(mp, (xfs_dqid_t)id, XFS_DQ_PROJ,
					     addr);
		break;

	      		
	      case Q_XGETPQUOTA:
		error = xfs_qm_scall_getquota(mp, (xfs_dqid_t)id, XFS_DQ_PROJ, 
					addr);
		break;
#else	
	      case Q_XSETPQLIM:
	      case Q_XGETPQUOTA:
		error = XFS_ERROR(ENOTSUP);
		break;
#endif
		/*
		 * Add a warning to the User/Project.
		 */
	      case Q_WARN:
		/*
		 * XXX when this is supported, is it a readonly op?
		 */
		error = xfs_qm_scall_qwarn(mp, (xfs_dqid_t)id, addr);
		break;

		/*
		 * Quotas are entirely undefined after quotaoff in XFS quotas.
		 * For instance, there's no way to set limits when quotaoff.
		 */
	      case Q_ACTIVATE:
	      case Q_SETQUOTA:
	      case Q_GETQUOTA:
	      case Q_GETPQUOTA:
	      case Q_SETQLIM:
	      case Q_SETPQLIM:
	      case Q_SYNC:
		error = XFS_ERROR(ENOTSUP);
		break;

	      default:
		error = XFS_ERROR(EINVAL);
		break;
	}
	return (error);

}



/*
 * Turn off quota accounting and/or enforcement for all udquots and/or
 * pdquots. Called only at mount time.
 *
 * This assumes that there are no dquots of this file system cached 
 * incore, and modifies the ondisk dquot directly. Therefore, for example,
 * it is an error to call this twice, without purging the cache.
 */
int
xfs_qm_scall_quotaoff(
	xfs_mount_t	*mp,
	uint		flags,
	boolean_t	force)
{
	uint			dqtype;
	int			s;
	int			error;
	uint			inactivate_flags;
	xfs_qoff_logitem_t 	*qoffstart;
	uint 			sbflags, newflags;
	extern dev_t		rootdev;
	int			nculprits;

	if (!force && !_CAP_ABLE(CAP_QUOTA_MGT))
		return XFS_ERROR(EPERM);
	/*
	 * Only root file system can have quotas enabled on disk but not
	 * in core. Note that quota utilities (like quotaoff) _expect_ 
	 * errno == EEXIST here.
	 */
	if (mp->m_dev != rootdev && (mp->m_qflags & flags) == 0)
		return XFS_ERROR(EEXIST);
	error = 0;

#ifdef _NOPROJQUOTAS
	flags &= (XFS_UQUOTA_ACCT | XFS_UQUOTA_ENFD);
#else
	flags &= (XFS_ALL_QUOTA_ACCT | XFS_ALL_QUOTA_ENFD);
#endif	
	/* 
	 * We don't want to deal with two quotaoffs messing up each other,
	 * so we're going to serialize it. quotaoff isn't exactly a performance
	 * critical thing. XXXcurrently, we also take the vfslock.
	 * If quotaoff, then we must be dealing with the root filesystem.
	 */
	ASSERT(mp->m_quotainfo || mp->m_dev == rootdev);
	if (mp->m_quotainfo)
		mutex_lock(&(XFS_QI_QOFFLOCK(mp)), PINOD);

	/*
	 * Root file system may or may not have quotas on in core.
	 * We have to perform the quotaoff accordingly.
	 */
	if (mp->m_dev == rootdev) {
		s = XFS_SB_LOCK(mp);
		sbflags = mp->m_sb.sb_qflags;
		if ((mp->m_qflags & flags) == 0) {
			mp->m_sb.sb_qflags &= ~(flags);
			newflags = mp->m_sb.sb_qflags;
			XFS_SB_UNLOCK(mp, s);
			if (mp->m_quotainfo)
				mutex_unlock(&(XFS_QI_QOFFLOCK(mp)));
			if (sbflags != newflags)
			      error = xfs_qm_write_sb_changes(mp, XFS_SB_QFLAGS);
			return (error);
		}			
		XFS_SB_UNLOCK(mp, s);
			
		if ((sbflags & flags) != (mp->m_qflags & flags)) {
			/* 
			 * This can happen only with proj+usr quota 
			 * combination. Note: 1) accounting cannot be turned
			 * off without enforcement also getting turned off.
			 * 2) Every flag that exist in mpqflags MUST exist
			 * in sbqflags (but not vice versa).
			 * which means at this point sbqflags = UQ+PQ+..,
			 * and mpqflags = UQ or PQ.
			 */
			ASSERT(sbflags & XFS_PQUOTA_ACCT);
			ASSERT((sbflags & XFS_ALL_QUOTA_ACCT) != 
			       (mp->m_qflags & XFS_ALL_QUOTA_ACCT));
			
			/* XXX TBD Finish this for proj quota support */
			/* We need to update the SB and mp separately */
			return XFS_ERROR(EINVAL);
		}
	}
	ASSERT(mp->m_quotainfo);

	/*
	 * if we're just turning off quota enforcement, change mp and go.
	 */
	if ((flags & XFS_ALL_QUOTA_ACCT) == 0) {
		mp->m_qflags &= ~(flags);
		
		s = XFS_SB_LOCK(mp);
		mp->m_sb.sb_qflags = mp->m_qflags;
		XFS_SB_UNLOCK(mp, s);
		mutex_unlock(&(XFS_QI_QOFFLOCK(mp)));
		
		/* XXX what to do if error ? Revert back to old vals incore ? */
		error = xfs_qm_write_sb_changes(mp, XFS_SB_QFLAGS);
		return (error);
	}

	dqtype = 0;
	inactivate_flags = 0;
	/* 
	 * If accounting is off, we must turn enforcement off, clear the
	 * quota 'CHKD' certificate to make it known that we have to
	 * do a quotacheck the next time this quota is turned on.
	 */
	if (flags & XFS_UQUOTA_ACCT) {
		dqtype |= XFS_QMOPT_UQUOTA;
		flags |= (XFS_UQUOTA_CHKD | XFS_UQUOTA_ENFD);
		inactivate_flags |= XFS_UQUOTA_ACTIVE;
	}
	if (flags & XFS_PQUOTA_ACCT) {
		dqtype |= XFS_QMOPT_PQUOTA;
		flags |= (XFS_PQUOTA_CHKD | XFS_PQUOTA_ENFD);
		inactivate_flags |= XFS_PQUOTA_ACTIVE;
	}
	
	/*
	 * Nothing to do? Don't complain.
	 * This happens when we're just turning off quota enforcement.
	 */
	if ((mp->m_qflags & flags) == 0) {
		mutex_unlock(&(XFS_QI_QOFFLOCK(mp)));
		return (0);
	}
	
	/*
	 * Write the LI_QUOTAOFF log record, and do SB changes atomically,
	 * and synchronously.
	 */
	xfs_qm_log_quotaoff(mp, &qoffstart, flags);

	/*
	 * Next we clear the XFS_MOUNT_*DQ_ACTIVE bit(s) in the mount struct
	 * to take care of the race between dqget and quotaoff. We don't take
	 * any special locks to reset these bits. All processes need to check
	 * these bits *after* taking inode lock(s) to see if the particular
	 * quota type is in the process of being turned off. If *ACTIVE, it is
	 * guaranteed that all dquot structures and all quotainode ptrs will all
	 * stay valid as long as that inode is kept locked. 
	 *
	 * There is no turning back after this.
	 */
	mp->m_qflags &= ~inactivate_flags;

	/*
	 * Give back all the dquot reference(s) held by inodes.
	 * Here we go thru every single incore inode in this file system, and
	 * do a dqrele on the i_udquot/i_pdquot that it may have.
	 * Essentially, as long as somebody has an inode locked, this guarantees
	 * that quotas will not be turned off. This is handy because in a
	 * transaction once we lock the inode(s) and check for quotaon, we can
	 * depend on the quota inodes (and other things) being valid as long as
	 * we keep the lock(s).
	 */
	xfs_qm_dqrele_all_inodes(mp, flags);
	
	/*
	 * Next we make the changes in the quota flag in the mount struct. 
	 * This isn't protected by a particular lock directly, because we
	 * don't want to take a mrlock everytime we depend on quotas being on.
	 */
	mp->m_qflags &= ~(flags);	

	/*
	 * Go through all the dquots of this file system and purge them,
	 * according to what was turned off. We may not be able to get rid
	 * of all dquots, because dquots can have temporary references that
	 * are not attached to inodes. eg. xfs_setattr, xfs_create.
	 * So, if we couldn't purge all the dquots from the filesystem,
	 * we can't get rid of the incore data structures.
	 */
	while (nculprits = xfs_qm_dqpurge_all(mp, dqtype|XFS_QMOPT_QUOTAOFF)) {
		delay(10 * nculprits);
	}

	/*
	 * Transactions that had started before ACTIVE state bit was cleared 
	 * could have logged many dquots, so they'd have higher LSNs than 
	 * the first QUOTAOFF log record does. If we happen to crash when
	 * the tail of the log has gone past the QUOTAOFF record, but 
	 * before the last dquot modification, those dquots __will__
	 * recover, and that's not good.
	 *
	 * So, we have QUOTAOFF start and end logitems; the start
	 * logitem won't get overwritten until the end logitem appears...
	 */
	xfs_qm_log_quotaoff_end(mp, qoffstart, flags);

	/*	
	 * If quotas is completely disabled, close shop.
	 */
	if ((flags & XFS_MOUNT_QUOTA_ALL) == XFS_MOUNT_QUOTA_ALL) {
		mutex_unlock(&(XFS_QI_QOFFLOCK(mp)));
		xfs_qm_destroy_quotainfo(mp);	
		return (0);
	}

	/*
	 * Release our quotainode references, and vn_purge them,
	 * if we don't need them anymore.
	 */
	if ((dqtype & XFS_QMOPT_UQUOTA) &&
	    XFS_QI_UQIP(mp)) {
		XFS_PURGE_INODE(XFS_ITOV(XFS_QI_UQIP(mp)));
		XFS_QI_UQIP(mp) = NULL;
	}
	if ((dqtype & XFS_QMOPT_PQUOTA) && 
	    XFS_QI_PQIP(mp)) {
		XFS_PURGE_INODE(XFS_ITOV(XFS_QI_PQIP(mp)));
		XFS_QI_PQIP(mp) = NULL;
	}
	mutex_unlock(&(XFS_QI_QOFFLOCK(mp)));

	return (error);
}
	
STATIC int
xfs_qm_scall_trunc_qfiles(
	xfs_mount_t	*mp,
	uint		flags)
{
	int 		error;
	xfs_inode_t	*qip;
	/*
	 * Function stolen from xfs_vnodeops.c 
	 */
	extern int  xfs_truncate_file(xfs_mount_t *mp, xfs_inode_t *ip);

	if (!_CAP_ABLE(CAP_QUOTA_MGT))
		return XFS_ERROR(EPERM);
	error = 0;
	if (!XFS_SB_VERSION_HASQUOTA(&mp->m_sb) || flags == 0)
		return XFS_ERROR(EINVAL);

	if ((flags & XFS_DQ_USER) &&
	    mp->m_sb.sb_uquotino != NULLFSINO) {
		error = xfs_iget(mp, NULL, 
				 mp->m_sb.sb_uquotino,
				 NULL,
				 &qip, 0);
		if (! error) {
			(void) xfs_truncate_file(mp, qip);
			VN_RELE(XFS_ITOV(qip));
		}
	}
	
	if ((flags & XFS_DQ_PROJ) &&
	    mp->m_sb.sb_pquotino != NULLFSINO) {
		error = xfs_iget(mp, NULL, 
				 mp->m_sb.sb_pquotino,
				 NULL,
				 &qip, 0);
		if (! error) {
			(void) xfs_truncate_file(mp, qip);
			VN_RELE(XFS_ITOV(qip));
		}
	}

	return (error);
}


/* 
 * This does two separate functions:
 * Switch on quotas for the root file system. This will take effect only
 * on reboot.
 * Switch on (a given) quota enforcement for both root and non-root filesystems.
 * This takes effect immediately.
 */
STATIC int
xfs_qm_scall_quotaon(
	xfs_mount_t	*mp,
	uint		flags)
{
	extern dev_t	rootdev;
	int		error, s;
	uint		qf;
	uint		accflags;
	__int64_t	sbflags;
	boolean_t	rootfs;
	boolean_t	delay;
	
	if (!_CAP_ABLE(CAP_QUOTA_MGT))
		return XFS_ERROR(EPERM);

	rootfs = (boolean_t) (mp->m_dev == rootdev);

#ifdef _NOPROJQUOTAS
	flags &= (XFS_UQUOTA_ACCT | XFS_UQUOTA_ENFD);
#else
	flags &= (XFS_ALL_QUOTA_ACCT | XFS_ALL_QUOTA_ENFD);
#endif	
	/*
	 * If caller wants to turn on accounting on /, but accounting
	 * is already turned on, ignore ACCTing flags.
	 * Switching on quota accounting for non-root filesystems
	 * must be done at mount time.
	 */
	accflags = flags & XFS_ALL_QUOTA_ACCT;
	if (!rootfs || 
	    (accflags && rootfs && ((mp->m_qflags & accflags) == accflags))) {
		flags &= ~(XFS_ALL_QUOTA_ACCT);
	}

	sbflags = 0;
	delay = (boolean_t) ((flags & XFS_ALL_QUOTA_ACCT) != 0);

	if (flags == 0)
		return XFS_ERROR(EINVAL);

	/* Only rootfs can turn on quotas with a delayed effect */
	ASSERT(!delay || rootfs);

	/*
	 * Can't enforce without accounting. We check the superblock
	 * qflags here instead of m_qflags because rootfs can have
	 * quota acct on ondisk without m_qflags' knowing.
	 */
	if (((flags & XFS_UQUOTA_ACCT) == 0 &&
	    (mp->m_sb.sb_qflags & XFS_UQUOTA_ACCT) == 0 &&
	    (flags & XFS_UQUOTA_ENFD))
	    ||
	    ((flags & XFS_PQUOTA_ACCT) == 0 &&
	    (mp->m_sb.sb_qflags & XFS_PQUOTA_ACCT) == 0 &&
	    (flags & XFS_PQUOTA_ENFD))) {
#ifdef QUOTADEBUG
		printf("Can't enforce without accounting.\n");
#endif		
		return XFS_ERROR(EINVAL);
	}
	/*
	 * If everything's upto-date incore, then don't waste time.
	 */
	if ((mp->m_qflags & flags) == flags)
		return XFS_ERROR(EEXIST);

	/*
	 * Change superblock version (if needed) for the root filesystem
	 */
	if (rootfs && !XFS_SB_VERSION_HASQUOTA(&mp->m_sb)) {
#ifdef DEBUG
		unsigned oldv = mp->m_sb.sb_versionnum;
#endif
		s = XFS_SB_LOCK(mp);
		XFS_SB_VERSION_ADDQUOTA(&mp->m_sb);
		mp->m_sb.sb_uquotino = NULLFSINO;
		mp->m_sb.sb_pquotino = NULLFSINO;
		mp->m_sb.sb_qflags = 0;
		XFS_SB_UNLOCK(mp, s);
#ifdef DEBUG
		cmn_err(CE_NOTE, 
			"Old superblock version %x, converting to %x.",
			oldv, mp->m_sb.sb_versionnum);
#endif
		sbflags |= (XFS_SB_VERSIONNUM | XFS_SB_UQUOTINO |
			    XFS_SB_PQUOTINO | XFS_SB_QFLAGS);
	}

	/*
	 * Change sb_qflags on disk but not incore mp->qflags
	 * if this is the root filesystem.
	 */
	s = XFS_SB_LOCK(mp);
	qf = mp->m_sb.sb_qflags;
	mp->m_sb.sb_qflags = qf | flags;
	XFS_SB_UNLOCK(mp, s);
	
	/*
	 * There's nothing to change if it's the same.
	 */
	if ((qf & flags) == flags && sbflags == 0) 
		return XFS_ERROR(EEXIST);
	sbflags |= XFS_SB_QFLAGS;

	if (error = xfs_qm_write_sb_changes(mp, sbflags))
		return (error);
	/*
	 * If we had just turned on quotas (ondisk) for rootfs, or if we aren't
	 * trying to switch on quota enforcement, we are done.
	 */
	if (delay || 
	    ((mp->m_sb.sb_qflags & XFS_UQUOTA_ACCT) != 
	     (mp->m_qflags & XFS_UQUOTA_ACCT)) ||
	    (flags & XFS_ALL_QUOTA_ENFD) == 0)
		return (0);
	
	if (! XFS_IS_QUOTA_RUNNING(mp))
		return XFS_ERROR(ESRCH);

	/*
	 * Switch on quota enforcement in core. This applies to both root
	 * and non-root file systems.
	 */
	mutex_lock(&(XFS_QI_QOFFLOCK(mp)), PINOD);
	mp->m_qflags |= (flags & XFS_ALL_QUOTA_ENFD);
	mutex_unlock(&(XFS_QI_QOFFLOCK(mp)));
	
	return (0);
}



/*
 * Return quota status information, such as uquota-off, enforcements, etc.
 */
STATIC int
xfs_qm_scall_getqstat(
	xfs_mount_t 	*mp,
	caddr_t		addr)
{
	fs_quota_stat_t	out;
	xfs_inode_t	*uip, *pip;
	boolean_t	tempuqip, temppqip;
	__uint16_t	sbflags;

	uip = pip = NULL;
	tempuqip = temppqip = B_FALSE;
	bzero(&out, sizeof(fs_quota_stat_t));

	out.qs_version = FS_QSTAT_VERSION;
	if (! XFS_SB_VERSION_HASQUOTA(&mp->m_sb)) {
		out.qs_uquota.qfs_ino = NULLFSINO;
		out.qs_pquota.qfs_ino = NULLFSINO;
		goto done;
	}
	out.qs_flags = (__uint16_t) xfs_qm_export_flags(mp->m_qflags & 
							(XFS_ALL_QUOTA_ACCT|
							 XFS_ALL_QUOTA_ENFD));
	/*
	 * If the qflags are different on disk, as can be the case when 
	 * root filesystem's quotas are being turned on, return them in the
	 * HI 8 bits.
	 */
	if (mp->m_dev == rootdev) {
		sbflags = (__uint16_t) xfs_qm_export_flags(mp->m_sb.sb_qflags & 
							   (XFS_ALL_QUOTA_ACCT|
							    XFS_ALL_QUOTA_ENFD));
		ASSERT((out.qs_flags & 0xff00) == 0);
		if (sbflags != out.qs_flags) 
			out.qs_flags |= ((sbflags & 0x00ff) << 8);
	}

	out.qs_pad = 0;
	out.qs_uquota.qfs_ino = mp->m_sb.sb_uquotino;
	out.qs_pquota.qfs_ino = mp->m_sb.sb_pquotino;
	if (mp->m_quotainfo) {
		uip = mp->m_quotainfo->qi_uquotaip;
		pip = mp->m_quotainfo->qi_pquotaip;
	}
	if (!uip && mp->m_sb.sb_uquotino != NULLFSINO) {
		if (xfs_iget(mp, NULL, mp->m_sb.sb_uquotino, 0,
			     &uip, 0) == 0) 
			tempuqip = B_TRUE;
	}
	if (!pip && mp->m_sb.sb_pquotino != NULLFSINO) {
		if (xfs_iget(mp, NULL, mp->m_sb.sb_pquotino, 0,
			     &pip, 0) == 0) 
			temppqip = B_TRUE;
	}
	if (uip) {
		out.qs_uquota.qfs_nblks = uip->i_d.di_nblocks;
		out.qs_uquota.qfs_nextents = uip->i_d.di_nextents;
		if (tempuqip)
			VN_RELE(XFS_ITOV(uip));
	}
	if (pip) {
		out.qs_pquota.qfs_nblks = pip->i_d.di_nblocks;
		out.qs_pquota.qfs_nextents = pip->i_d.di_nextents;
		if (temppqip)
			VN_RELE(XFS_ITOV(pip));
	}
	if (mp->m_quotainfo) {
		out.qs_incoredqs = XFS_QI_MPLNDQUOTS(mp);
		out.qs_btimelimit = XFS_QI_BTIMELIMIT(mp);
		out.qs_itimelimit = XFS_QI_ITIMELIMIT(mp);
		out.qs_rtbtimelimit = XFS_QI_RTBTIMELIMIT(mp);
		out.qs_bwarnlimit = XFS_QI_BWARNLIMIT(mp);
		out.qs_iwarnlimit = XFS_QI_IWARNLIMIT(mp);
	}
 done:
	if (copyout(&out, addr, sizeof(fs_quota_stat_t)))	
		return XFS_ERROR(EFAULT);
	return (0);
}

/*
 * Adjust quota limits, and start/stop timers accordingly.
 */
STATIC int
xfs_qm_scall_setqlim(
	xfs_mount_t		*mp,
	xfs_dqid_t		id,
	uint			type,
	caddr_t			addr)
{
	xfs_disk_dquot_t	*ddq;
	fs_disk_quota_t		newlim;
	xfs_dquot_t		*dqp;
	xfs_trans_t		*tp;
	int			error;
	xfs_qcnt_t		hard, soft;

	if (!_CAP_ABLE(CAP_QUOTA_MGT))
		return XFS_ERROR(EPERM);
	if (copyin(addr, &newlim, sizeof newlim))
		return XFS_ERROR(EFAULT);

	if ((newlim.d_fieldmask & (FS_DQ_LIMIT_MASK|FS_DQ_TIMER_MASK)) == 0)
		return (0);

	tp = xfs_trans_alloc(mp, XFS_TRANS_QM_SETQLIM); 
	if (error = xfs_trans_reserve(tp, 0, sizeof(xfs_disk_dquot_t) + 128,
				      0, 0, XFS_DEFAULT_LOG_COUNT)) {
		xfs_trans_cancel(tp, 0);
		return (error);
	}

	/*
	 * We don't want to race with a quotaoff so take the quotaoff lock.
	 * (We don't hold an inode lock, so there's nothing else to stop
	 * a quotaoff from happening). (XXXThis doesn't currently doesn't happen
	 * because we take the vfslock before calling xfs_qm_sysent).
	 */
	mutex_lock(&(XFS_QI_QOFFLOCK(mp)), PINOD);

	/*
	 * Get the dquot (locked), and join it to the transaction.
	 * Allocate the dquot if this doesn't exist.
	 */
	if (error = xfs_qm_dqget(mp, NULL, id, type, XFS_QMOPT_DQALLOC, &dqp)) {
		xfs_trans_cancel(tp, XFS_TRANS_ABORT);
		mutex_unlock(&(XFS_QI_QOFFLOCK(mp)));
		ASSERT(error != ENOENT);
		return (error);	    
	}
	xfs_dqtrace_entry(dqp, "Q_SETQLIM: AFT DQGET");
	xfs_trans_dqjoin(tp, dqp);
	ddq = &dqp->q_core;

	/*
	 * Make sure that hardlimits are >= soft limits before changing.
	 */
	hard = (newlim.d_fieldmask & FS_DQ_BHARD) ?
		(xfs_qcnt_t) XFS_BB_TO_FSB(mp, newlim.d_blk_hardlimit) :
			ddq->d_blk_hardlimit;
	soft = (newlim.d_fieldmask & FS_DQ_BSOFT) ?
		(xfs_qcnt_t) XFS_BB_TO_FSB(mp, newlim.d_blk_softlimit) :
			ddq->d_blk_softlimit;
	if (hard == 0 || hard >= soft) {
		ddq->d_blk_hardlimit = hard;
		ddq->d_blk_softlimit = soft;
	}
#ifdef QUOTADEBUG
	else 
		printf("blkhard 0x%x < blksoft 0x%x\n", hard, soft);
#endif			
	hard = (newlim.d_fieldmask & FS_DQ_RTBHARD) ?
		(xfs_qcnt_t) XFS_BB_TO_FSB(mp, newlim.d_rtb_hardlimit) :
			ddq->d_rtb_hardlimit;
	soft = (newlim.d_fieldmask & FS_DQ_RTBSOFT) ?
		(xfs_qcnt_t) XFS_BB_TO_FSB(mp, newlim.d_rtb_softlimit) :
			ddq->d_rtb_softlimit;
	if (hard == 0 || hard >= soft) {
		ddq->d_rtb_hardlimit = hard;
		ddq->d_rtb_softlimit = soft;
	}
#ifdef QUOTADEBUG
	else 
		printf("rtbhard 0x%x < rtbsoft 0x%x\n", hard, soft);
#endif	
	
	hard = (newlim.d_fieldmask & FS_DQ_IHARD) ?
		(xfs_qcnt_t) newlim.d_ino_hardlimit : ddq->d_ino_hardlimit;
	soft = (newlim.d_fieldmask & FS_DQ_ISOFT) ?
		(xfs_qcnt_t) newlim.d_ino_softlimit : ddq->d_ino_softlimit;
	if (hard == 0 || hard >= soft) {
		ddq->d_ino_hardlimit = hard;
		ddq->d_ino_softlimit = soft;
	}
#ifdef QUOTADEBUG
	else 
		printf("ihard 0x%x < isoft 0x%x\n", hard, soft);
#endif		
	if (id == 0) {
		/*
		 * Timelimits for the super user set the relative time
		 * the other users can be over quota for this file system.
		 * If it is zero a default is used.
		 */
		if (newlim.d_fieldmask & FS_DQ_BTIMER) {
			mp->m_quotainfo->qi_btimelimit = newlim.d_btimer;
			dqp->q_core.d_btimer = newlim.d_btimer;
		}
		if (newlim.d_fieldmask & FS_DQ_ITIMER) {
			mp->m_quotainfo->qi_itimelimit = newlim.d_itimer;
			dqp->q_core.d_itimer = newlim.d_itimer;
		}
		if (newlim.d_fieldmask & FS_DQ_RTBTIMER) {
			mp->m_quotainfo->qi_rtbtimelimit = newlim.d_rtbtimer;
			dqp->q_core.d_rtbtimer = newlim.d_rtbtimer;
		}
#ifdef NOTYET
		/*
		 * Ditto, for warning limits.
		 * XXXthese aren't quite in use yet.
		 */
		if (newlim.d_bwarns > 0) {
			mp->m_quotainfo->qi_bwarnlimit = newlim.d_bwarns;
			dqp->q_core.d_bwarns = newlim.d_bwarns;
		}
		if (newlim.d_iwarns > 0) {
			mp->m_quotainfo->qi_iwarnlimit = newlim.d_iwarns;
			dqp->q_core.d_iwarns = newlim.d_iwarns;
		}
		if (newlim.d_rtbwarns > 0) {
			dqp->q_core.d_rtbwarns = newlim.d_rtbwarns;
		}
#endif
	} else /* if (XFS_IS_QUOTA_ENFORCED(mp)) */ {
		/*
		 * If the user is now over quota, start the timelimit.
		 * The user will not be 'warned'. Warnings increase only
		 * by request via Q_WARN.
		 * Note that we keep the timers ticking, whether enforcement
		 * is on or off. We don't really want to bother with iterating
		 * over all ondisk dquots are turning the timers on/off.
		 */
		xfs_qm_adjust_dqtimers(mp, ddq);
	}
	dqp->dq_flags |= XFS_DQ_DIRTY;
	xfs_trans_log_dquot(tp, dqp);

	xfs_dqtrace_entry(dqp, "Q_SETQLIM: COMMIT");
	xfs_trans_commit(tp, 0, NULL);
	/* xfs_qm_dqprint(dqp); */
	xfs_qm_dqrele(dqp);
	mutex_unlock(&(XFS_QI_QOFFLOCK(mp)));

	return (0);
}

/* ARGSUSED */
STATIC int
xfs_qm_scall_qwarn(
	xfs_mount_t 	*mp,
	xfs_dqid_t 	id,
	caddr_t 	addr)
{
	/*
	 * XXX when this is supported, if it's a write operation
	 * (changes the filesystem), it shouldn't work in a read-only
	 * filesystem.  That has to be enforced in xfs_qm_sysent().
	 */
#ifdef _IRIX62_XFS_ONLY
	return XFS_ERROR(ENOTSUP);
#else
	return XFS_ERROR(ENOTSUP); /* temp */
#endif
}
	   
STATIC int
xfs_qm_scall_getquota(
	xfs_mount_t 	*mp,
	xfs_dqid_t 	id,
	uint		type,
	caddr_t 	addr)
{
	xfs_dquot_t	*dqp;
	fs_disk_quota_t	out;
	int 		error;

	if (id != get_current_cred()->cr_ruid && !_CAP_ABLE(CAP_QUOTA_MGT))
		return XFS_ERROR(EPERM);
	/*
	 * Try to get the dquot. We don't want it allocated on disk, so
	 * we aren't passing the XFS_QMOPT_DOALLOC flag. If it doesn't
	 * exist, we'll get ENOENT back.
	 */
	if (error = xfs_qm_dqget(mp, NULL, id, type, 0, &dqp)) {
		return (error);
	}

	xfs_dqtrace_entry(dqp, "Q_GETQUOTA SUCCESS");
	/*
	 * If everything's NULL, this dquot doesn't quite exist as far as
	 * our utility programs are concerned.
	 */
	if (XFS_IS_DQUOT_UNINITIALIZED(dqp)) {
		xfs_qm_dqput(dqp);
		return XFS_ERROR(ENOENT);
	}
	/* xfs_qm_dqprint(dqp); */
	/*
	 * Convert the disk dquot to the exportable format
	 */
	xfs_qm_export_dquot(mp, &dqp->q_core, &out);
	error = copyout(&out, addr, sizeof(out));
	xfs_qm_dqput(dqp);
	return (error ? XFS_ERROR(EFAULT) : 0);
}


STATIC int
xfs_qm_log_quotaoff_end(
	xfs_mount_t 		*mp,
	xfs_qoff_logitem_t 	*startqoff,
	uint			flags)
{
	xfs_trans_t	       *tp;
	int 			error;
	xfs_qoff_logitem_t     *qoffi;

	tp = xfs_trans_alloc(mp, XFS_TRANS_QM_QUOTAOFF_END);

	if (error = xfs_trans_reserve(tp, 0, sizeof(xfs_qoff_logitem_t) * 2,
				      0, 0, XFS_DEFAULT_LOG_COUNT)) {
		xfs_trans_cancel(tp, 0);
		return (error);
	}

	qoffi = xfs_trans_get_qoff_item(tp, startqoff,
					flags & XFS_ALL_QUOTA_ACCT);
	xfs_trans_log_quotaoff_item(tp, qoffi);

	/*
	 * We have to make sure that the transaction is secure on disk before
	 * we return and actually stop quota accounting. So, make it synchronous.
	 * We don't care about quotoff's performance.
	 */
	xfs_trans_set_sync(tp);
	error = xfs_trans_commit(tp, 0, NULL);
	return (error);
}


STATIC int
xfs_qm_log_quotaoff(
	xfs_mount_t 	       *mp,
	xfs_qoff_logitem_t     **qoffstartp,
	uint		       flags)
{
	xfs_trans_t	       *tp;
	int 			error, s;
	xfs_qoff_logitem_t     *qoffi;
	uint			oldsbqflag;

	tp = xfs_trans_alloc(mp, XFS_TRANS_QM_QUOTAOFF);
	if (error = xfs_trans_reserve(tp, 0, 
				      sizeof(xfs_qoff_logitem_t) * 2 +
				      mp->m_sb.sb_sectsize + 128,
				      0,
				      0, 
				      XFS_DEFAULT_LOG_COUNT)) {
		goto error0;
	}

	qoffi = xfs_trans_get_qoff_item(tp, NULL, flags & XFS_ALL_QUOTA_ACCT);
	xfs_trans_log_quotaoff_item(tp, qoffi);

	s = XFS_SB_LOCK(mp);
	oldsbqflag = mp->m_sb.sb_qflags;
	mp->m_sb.sb_qflags = (mp->m_qflags & ~(flags)) & XFS_MOUNT_QUOTA_ALL;
	XFS_SB_UNLOCK(mp, s);

	xfs_mod_sb(tp, XFS_SB_QFLAGS);

	/*
	 * We have to make sure that the transaction is secure on disk before
	 * we return and actually stop quota accounting. So, make it synchronous.
	 * We don't care about quotoff's performance.
	 */
	xfs_trans_set_sync(tp);
	error = xfs_trans_commit(tp, 0, NULL);
 
error0:
	if (error) {
		xfs_trans_cancel(tp, 0);
		/*
		 * No one else is modifying sb_qflags, so this is OK.
		 * We still hold the quotaofflock.
		 */
		s = XFS_SB_LOCK(mp);
		mp->m_sb.sb_qflags = oldsbqflag;
		XFS_SB_UNLOCK(mp, s);
	} 
	*qoffstartp = qoffi;
	return (error);

}



/*
 * Translate an internal style on-disk-dquot to the exportable format.
 * Currently, the main difference is that the counters/limits are all in
 * Basic Blocks (BBs) instead of the internal FSBs.
 * This is used by qm_sysent to service syscalls like quotactl(Q_XGETQUOTA)
 */
STATIC void
xfs_qm_export_dquot(
	xfs_mount_t		*mp,
	xfs_disk_dquot_t	*src,
	struct fs_disk_quota	*dst)
{
	bzero(dst, sizeof(*dst));
	dst->d_version = FS_DQUOT_VERSION;  /* different from src->d_version */
	dst->d_flags = xfs_qm_export_qtype_flags(src->d_flags);
	dst->d_id = src->d_id;
	dst->d_blk_hardlimit = (__uint64_t) 
		XFS_FSB_TO_BB(mp, src->d_blk_hardlimit);
	dst->d_blk_softlimit = (__uint64_t) 
		XFS_FSB_TO_BB(mp, src->d_blk_softlimit);
	dst->d_ino_hardlimit = (__uint64_t) src->d_ino_hardlimit;
	dst->d_ino_softlimit = (__uint64_t) src->d_ino_softlimit;
		
	dst->d_bcount = (__uint64_t) XFS_FSB_TO_BB(mp, src->d_bcount);
	dst->d_icount = (__uint64_t) src->d_icount;
	dst->d_btimer = (__uint32_t) src->d_btimer;
	dst->d_itimer = (__uint32_t) src->d_itimer;
	dst->d_iwarns = src->d_iwarns;
	dst->d_bwarns = src->d_bwarns;

	dst->d_rtb_hardlimit = (__uint64_t) 
		XFS_FSB_TO_BB(mp, src->d_rtb_hardlimit);
	dst->d_rtb_softlimit = (__uint64_t) 
		XFS_FSB_TO_BB(mp, src->d_rtb_softlimit);
	dst->d_rtbcount = (__uint64_t) XFS_FSB_TO_BB(mp, src->d_rtbcount);
	dst->d_rtbtimer = (__uint32_t) src->d_rtbtimer;
	dst->d_rtbwarns = src->d_rtbwarns;
	
	/*
	 * Internally, we don't reset all the timers when quota enforcement
	 * gets turned off. No need to confuse the userlevel code, 
	 * so return zeroes in that case.
	 */
	if (! XFS_IS_QUOTA_ENFORCED(mp)) {
		dst->d_btimer = 0;
		dst->d_itimer = 0;
		dst->d_rtbtimer = 0;
	}
	
#ifdef QUOTADEBUG	
	if (XFS_IS_QUOTA_ENFORCED(mp) && dst->d_id != 0) {
		if (((int) dst->d_bcount >= (int) dst->d_blk_softlimit) &&
		    (dst->d_blk_softlimit > 0)) {
			ASSERT(dst->d_btimer != 0);
		}
		if (((int) dst->d_icount >= (int) dst->d_ino_softlimit) &&
		    (dst->d_ino_softlimit > 0)) {
			ASSERT(dst->d_itimer != 0);
		}
	}
#endif
}	

STATIC uint
xfs_qm_import_qtype_flags(
	uint *uflagsp)
{
	uint uflags;
	
	if (copyin(uflagsp, &uflags, sizeof(uint)) < 0) {
		return (0);
	}

	/*
	 * Can't be both at the same time.
	 */
	if (((uflags & (XFS_PROJ_QUOTA | XFS_USER_QUOTA)) ==
	     (XFS_PROJ_QUOTA | XFS_USER_QUOTA)) ||
	    ((uflags & (XFS_PROJ_QUOTA | XFS_USER_QUOTA)) == 0))
		return (0);

	return (uflags & XFS_USER_QUOTA) ? 
		XFS_DQ_USER : XFS_DQ_PROJ;
}

STATIC uint
xfs_qm_export_qtype_flags(
	uint flags)
{
	/*
	 * Can't be both at the same time.
	 */
	ASSERT((flags & (XFS_PROJ_QUOTA | XFS_USER_QUOTA)) !=
		(XFS_PROJ_QUOTA | XFS_USER_QUOTA));
	ASSERT((flags & (XFS_PROJ_QUOTA | XFS_USER_QUOTA)) != 0);

	return (flags & XFS_DQ_USER) ? 
		XFS_USER_QUOTA : XFS_PROJ_QUOTA;
}

STATIC uint
xfs_qm_import_flags(
	uint *uflagsp)
{
	uint flags, uflags;
	
	if (copyin(uflagsp, &uflags, sizeof(uint)) < 0) 
		return (0);

	flags = 0;
	if (uflags & XFS_QUOTA_UDQ_ACCT)
		flags |= XFS_UQUOTA_ACCT;
	if (uflags & XFS_QUOTA_PDQ_ACCT)
		flags |= XFS_PQUOTA_ACCT;
	if (uflags & XFS_QUOTA_UDQ_ENFD)
		flags |= XFS_UQUOTA_ENFD;
	if (uflags & XFS_QUOTA_PDQ_ENFD)
		flags |= XFS_PQUOTA_ENFD;
	return (flags);
}


STATIC uint
xfs_qm_export_flags(
	uint flags)
{
	uint uflags;
	
	uflags = 0;
	if (flags & XFS_UQUOTA_ACCT)
		uflags |= XFS_QUOTA_UDQ_ACCT;
	if (flags & XFS_PQUOTA_ACCT)
		uflags |= XFS_QUOTA_PDQ_ACCT;
	if (flags & XFS_UQUOTA_ENFD)
		uflags |= XFS_QUOTA_UDQ_ENFD;
	if (flags & XFS_PQUOTA_ENFD)
		uflags |= XFS_QUOTA_PDQ_ENFD;
	return (uflags);
}


/* 
 * Go thru all the inodes in the file system, releasing their dquots.
 * Note that the mount structure gets modified to indicate that quotas are off
 * AFTER this, in the case of quotaoff. This also gets called from 
 * xfs_rootumount.
 */
void
xfs_qm_dqrele_all_inodes(
	struct xfs_mount *mp,
	uint		 flags)
{
	vmap_t 		vmap;
	xfs_inode_t	*ip, *topino;
	uint		ireclaims;
	vnode_t		*vp;
	boolean_t	vnode_refd;

	ASSERT(mp->m_quotainfo);

again:
	XFS_MOUNT_ILOCK(mp);
	ip = mp->m_inodes;
	if (ip == NULL) {
		XFS_MOUNT_IUNLOCK(mp);
		return;
	}
        do {		
		/*
		 * skip markers from xfs_sync
		 */

		if (ip->i_mount == NULL) {
			ip = ip->i_mnext;
			continue;
		}

		/*
		 * Rootinode, rbmip and rsumip have blocks associated with it.
		 */
		if (ip == XFS_QI_UQIP(mp) || ip == XFS_QI_PQIP(mp)) {
			ASSERT(ip->i_udquot == NULL);
			ASSERT(ip->i_pdquot == NULL);
			ip = ip->i_mnext;
			continue;
		}
		vp = XFS_ITOV(ip);
		vnode_refd = B_FALSE;
		if (xfs_ilock_nowait(ip, XFS_ILOCK_EXCL) == 0) {
			/*
			 * Sample vp mapping while holding the mplock, lest
			 * we come across a non-existent vnode.
			 */
			VMAP(vp, vmap);
			ireclaims = mp->m_ireclaims;
			topino = mp->m_inodes;
			XFS_MOUNT_IUNLOCK(mp);
			
			/* XXX restart limit ? */
#ifndef _IRIX62_XFS_ONLY
			if (!(vp = vn_get(vp, &vmap, 0)))
				goto again;
#else
			if (!(vp = vn_get(vp, &vmap)))
				goto again;
			ASSERT(ip == XFS_VTOI(vp));
#endif
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			vnode_refd = B_TRUE;
		} else {
			ireclaims = mp->m_ireclaims;
			topino = mp->m_inodes;
			XFS_MOUNT_IUNLOCK(mp);
		}

		/* 
		 * We don't keep the mountlock across the dqrele() call,
		 * since it can take a while..
		 */
		if ((flags & XFS_UQUOTA_ACCT) && ip->i_udquot) {
			xfs_qm_dqrele(ip->i_udquot);
			ip->i_udquot = NULL;
		}
		if ((flags & XFS_PQUOTA_ACCT) && ip->i_pdquot) {
			xfs_qm_dqrele(ip->i_pdquot);
			ip->i_pdquot = NULL;
		}
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		/*
		 * Wait until we've dropped the ilock and mountlock to
		 * do the vn_rele. Or be condemned to an eternity in the
		 * inactive code in hell.
		 */
		if (vnode_refd)
			VN_RELE(vp);
		XFS_MOUNT_ILOCK(mp);
		/*
		 * If an inode was inserted or removed, we gotta
		 * start over again.
		 */
		if (topino != mp->m_inodes || mp->m_ireclaims != ireclaims) {
			/* XXX use a sentinel */
			XFS_MOUNT_IUNLOCK(mp);
			goto again;
		}
		ip = ip->i_mnext;
	} while (ip != mp->m_inodes);

	XFS_MOUNT_IUNLOCK(mp);
	return;
}

/*------------------------------------------------------------------------*/
#ifdef DEBUG
/*
 * This contains all the test functions for XFS disk quotas.
 * Currently it does an quota accounting check. ie. it walks through
 * all inodes in the file system, calculating the dquot accounting fields,	
 * and prints out any inconsistencies. 
 */
xfs_dqhash_t *qmtest_udqtab;
xfs_dqhash_t *qmtest_pdqtab;
int	      qmtest_hashmask;
int	      qmtest_nfails;
mutex_t	      qcheck_lock;

#define DQTEST_HASHVAL(mp, id) (((__psunsigned_t)(mp) + \
				 (__psunsigned_t)(id)) & \
				(qmtest_hashmask - 1))

#define DQTEST_HASH(mp, id, type)   ((type & XFS_DQ_USER) ? \
				     (qmtest_udqtab + \
				      DQTEST_HASHVAL(mp, id)) : \
				     (qmtest_pdqtab + \
				      DQTEST_HASHVAL(mp, id)))

#define DQTEST_LIST_PRINT(l, NXT, title) \
{ \
	  xfs_dqtest_t	*dqp; int i = 0;\
	  printf("%s (#%d)\n", title, (int) (l)->qh_nelems); \
	  for (dqp = (xfs_dqtest_t *)(l)->qh_next; dqp != NULL; \
	       dqp = (xfs_dqtest_t *)dqp->NXT) { \
	    printf("\t%d\.\t\"%d (%s)\"\t bcnt = %d, icnt = %d\n", \
			 ++i, (int) dqp->d_id, \
		         DQFLAGTO_TYPESTR(dqp),      \
			 (int) dqp->d_bcount, \
			 (int) dqp->d_icount); } \
}
	
typedef struct dqtest {
	xfs_dqmarker_t	q_lists;
	xfs_dqhash_t	*q_hash;        /* the hashchain header */
	struct xfs_mount *q_mount;	/* filesystem this relates to */
	xfs_dqid_t	d_id;		/* user id or proj id */
	xfs_qcnt_t	d_bcount;	/* # disk blocks owned by the user */
	xfs_qcnt_t	d_icount;	/* # inodes owned by the user */
} xfs_dqtest_t;

STATIC void
xfs_qm_hashinsert(xfs_dqhash_t *h, xfs_dqtest_t *dqp)
{							
	xfs_dquot_t *d;			
	if ((d) = (h)->qh_next)		
		(d)->HL_PREVP = &((dqp)->HL_NEXT);	
	(dqp)->HL_NEXT = d;			
	(dqp)->HL_PREVP = &((h)->qh_next);		
	(h)->qh_next = (xfs_dquot_t *)dqp;			
	(h)->qh_version++;			
	(h)->qh_nelems++;			
}
STATIC void
xfs_qm_dqtest_print(
	xfs_dqtest_t	*d)
{
	printf( "-----------DQTEST DQUOT----------------\n");
	printf( "---- dquot ID	=  %d\n", (int) d->d_id);
	printf( "---- type      =  %s\n", XFS_QM_ISUDQ(d) ? "USR" :
	       "PRJ");
	printf( "---- fs        =  0x%x\n", d->q_mount);
	printf( "---- bcount	=  %llu (0x%x)\n", 
	       d->d_bcount,
	       (int)d->d_bcount);
	printf( "---- icount	=  %llu (0x%x)\n", 
	       d->d_icount,
	       (int)d->d_icount);
	printf( "---------------------------\n");
}

STATIC void
xfs_qm_dqtest_failed(
	xfs_dqtest_t	*d,
	xfs_dquot_t	*dqp,
	char 		*reason,
	xfs_qcnt_t	a,
	xfs_qcnt_t	b,
	int		error)
{	
	qmtest_nfails++;
	if (error)
		printf("quotacheck failed for %d, error = %d\nreason = %s\n",
		       d->d_id, error, reason);
	else
		printf("quotacheck failed for %d (%s) [%d != %d]\n",
		       d->d_id, reason, (int)a, (int)b);
	xfs_qm_dqtest_print(d);
	if (dqp)
		xfs_qm_dqprint(dqp);
}

STATIC int
xfs_dqtest_cmp2(
	xfs_dqtest_t	*d,
	xfs_dquot_t	*dqp)
{
	int err = 0;
	if (dqp->q_core.d_icount != d->d_icount) {
		xfs_qm_dqtest_failed(d, dqp, "icount mismatch", 
				     dqp->q_core.d_icount, d->d_icount, 0);
		err++;
	}
	if (dqp->q_core.d_bcount != d->d_bcount) {
		xfs_qm_dqtest_failed(d, dqp, "bcount mismatch", 
				     dqp->q_core.d_bcount, d->d_bcount, 0);
		err++;
	}
	if (dqp->q_core.d_blk_softlimit &&
	    dqp->q_core.d_bcount >= dqp->q_core.d_blk_softlimit) {
		if (dqp->q_core.d_btimer == 0 &&
		    dqp->q_core.d_id != 0) {
			printf("%d [%s] [0x%x] BLK TIMER NOT STARTED\n", 
			       (int) d->d_id, DQFLAGTO_TYPESTR(d), d->q_mount);
			err++;
		}
	}
	if (dqp->q_core.d_ino_softlimit &&
	    dqp->q_core.d_icount >= dqp->q_core.d_ino_softlimit) {
		if (dqp->q_core.d_itimer == 0 &&
		    dqp->q_core.d_id != 0) {
			printf("%d [%s] [0x%x] INO TIMER NOT STARTED\n", 
			       (int) d->d_id, DQFLAGTO_TYPESTR(d), 
			       d->q_mount);
			err++;
		}
	}
#if 0
	if (!err) {
		printf("%d [%s] [0x%x] qchecked\n", 
		       (int) d->d_id, XFS_QM_ISUDQ(d) ? "USR" : "PRJ", d->q_mount);
	}
#endif
	return (err);
}

STATIC void
xfs_dqtest_cmp(
	xfs_dqtest_t	*d)
{
	xfs_dquot_t	*dqp;
	int		error;

	/* xfs_qm_dqtest_print(d); */
	if (error = xfs_qm_dqget(d->q_mount, NULL, d->d_id, d->dq_flags, 0,
				 &dqp)) {
		xfs_qm_dqtest_failed(d, NULL, "dqget failed", 0, 0, error);
		return;
	}
	xfs_dqtest_cmp2(d, dqp);
	xfs_qm_dqput(dqp);
}

STATIC int
xfs_qm_internalqcheck_dqget(
	xfs_mount_t	*mp,
	xfs_dqid_t	id,
	uint		type,
	xfs_dqtest_t 	**O_dq)
{
	xfs_dqtest_t 	*d;
	xfs_dqhash_t	*h;

	h = DQTEST_HASH(mp, id, type);
	for (d = (xfs_dqtest_t *) h->qh_next; d != NULL; 
	     d = (xfs_dqtest_t *) d->HL_NEXT) {
		/* DQTEST_LIST_PRINT(h, HL_NEXT, "@@@@@ dqtestlist @@@@@"); */
		if (d->d_id == id && mp == d->q_mount) {
			*O_dq = d;
			return (0);
		}
	}
	d = kmem_zalloc(sizeof(xfs_dqtest_t), KM_SLEEP);
	d->dq_flags = type;
	d->d_id = id;
	d->q_mount = mp;
	d->q_hash = h;
	xfs_qm_hashinsert(h, d);
	*O_dq = d;
	return (0);
}	

STATIC int
xfs_qm_internalqcheck_get_dquots(
	xfs_mount_t	*mp,
	xfs_dqid_t	uid,
	xfs_dqid_t	prid,
	xfs_dqtest_t 	**ud,
	xfs_dqtest_t 	**pd)
{
	if (XFS_IS_UQUOTA_ON(mp)) 
		xfs_qm_internalqcheck_dqget(mp, uid, XFS_DQ_USER, ud);
	if (XFS_IS_PQUOTA_ON(mp)) 
		xfs_qm_internalqcheck_dqget(mp, prid, XFS_DQ_PROJ, pd);
	return (0);
}


STATIC int
xfs_qm_internalqcheck_dqadjust(
	xfs_inode_t		*ip,
	xfs_dqtest_t        	*d)
{
	d->d_icount++;
	d->d_bcount += (xfs_qcnt_t)ip->i_d.di_nblocks;
	return (0);
}

/* ARGSUSED */
STATIC int
xfs_qm_internalqcheck_adjust(
	xfs_mount_t     *mp,            /* mount point for filesystem */
        xfs_trans_t     *tp,            /* transaction pointer */
        xfs_ino_t       ino,            /* inode number to get data for */
        void            *buffer,        /* not used */
        daddr_t         bno,            /* starting block of inode cluster */	
	void		*dip,		/* not used */
	int		*res)		/* bulkstat result code */
{
	xfs_inode_t     	*ip;
	xfs_dqtest_t		*ud, *pd;
	uint			lock_flags;
	extern	dev_t		rootdev;
	boolean_t		ipreleased;
	int			error;

	ASSERT(XFS_IS_QUOTA_RUNNING(mp));

	if (ino == mp->m_sb.sb_uquotino || ino == mp->m_sb.sb_pquotino) {
		*res = BULKSTAT_RV_NOTHING;
                return XFS_ERROR(EINVAL);
	}
	ipreleased = B_FALSE;
 again:
	lock_flags = XFS_ILOCK_SHARED;
        if (error = xfs_iget(mp, tp, ino, lock_flags, &ip, bno)) {
		*res = BULKSTAT_RV_NOTHING;
                return (error);
	}

        if (ip->i_d.di_mode == 0) {
                xfs_iput(ip, lock_flags);
		*res = BULKSTAT_RV_NOTHING;
                return XFS_ERROR(ENOENT);
        }
	
	/*
	 * This inode can have blocks after eof which can get released 
	 * when we send it to inactive. Since we don't check the dquot
	 * until the after all our calculations are done, we must get rid
	 * of those now.
	 */
	if (! ipreleased) {
		xfs_iput(ip, lock_flags);
		ipreleased = B_TRUE;
		goto again;
	}
	if (error = xfs_qm_internalqcheck_get_dquots(mp, 
					     (xfs_dqid_t) ip->i_d.di_uid,
					     (xfs_dqid_t) ip->i_d.di_projid,
					     &ud, &pd)) {
		xfs_iput(ip, lock_flags);
		*res = BULKSTAT_RV_NOTHING;
		return (error);
	}
	if (XFS_IS_UQUOTA_ON(mp)) {
		ASSERT(ud);
		(void) xfs_qm_internalqcheck_dqadjust(ip, ud);
	}
	if (XFS_IS_PQUOTA_ON(mp)) {
		ASSERT(pd);
		(void) xfs_qm_internalqcheck_dqadjust(ip, pd);
	}
	xfs_iput(ip, lock_flags);
	*res = BULKSTAT_RV_DIDONE;
	return (0);
}


/* PRIVATE, debugging */
int
xfs_qm_internalqcheck(
	xfs_mount_t	*mp)
{
	ino64_t 	lastino;
	int 		done, count;
	int		i;
	xfs_dqtest_t	*d, *e;
	xfs_dqhash_t	*h1;
	int 		error;

	lastino = 0;
	qmtest_hashmask = 32;
	count = 5;
	done = 0;
	qmtest_nfails = 0;
	
	if (! XFS_IS_QUOTA_ON(mp))
		return XFS_ERROR(ESRCH);
	
	xfs_log_force(mp, (xfs_lsn_t)0, XFS_LOG_FORCE | XFS_LOG_SYNC);
	bflush(mp->m_dev);
	xfs_log_force(mp, (xfs_lsn_t)0, XFS_LOG_FORCE | XFS_LOG_SYNC);
	bflush(mp->m_dev);
	
	mutex_lock(&qcheck_lock, PINOD);
	/* There should be absolutely no quota activity while this
	   is going on. */
	qmtest_udqtab = kmem_zalloc(qmtest_hashmask *
				    sizeof(xfs_dqhash_t), KM_SLEEP);
	qmtest_pdqtab = kmem_zalloc(qmtest_hashmask *
				    sizeof(xfs_dqhash_t), KM_SLEEP);
	do {
		/*
		 * Iterate thru all the inodes in the file system,
		 * adjusting the corresponding dquot counters
		 */
		if (error = xfs_bulkstat(mp, NULL, &lastino, &count, 
				 xfs_qm_internalqcheck_adjust,
				 0, NULL, BULKSTAT_FG_IGET, &done)) {
			break;
		}
	} while (! done);
	if (error) {
		printf("Bulkstat returned error 0x%x\n", 
		       error);
	}
	printf("Checking results against system dquots\n");
	for (i = 0; i < qmtest_hashmask; i++) {
		h1 = &qmtest_udqtab[i];
		for (d = (xfs_dqtest_t *) h1->qh_next; d != NULL; ) {
			xfs_dqtest_cmp(d);
			e = (xfs_dqtest_t *) d->HL_NEXT;
			kmem_free(d, sizeof(xfs_dqtest_t)); 
			d = e;
		}
		h1 = &qmtest_pdqtab[i];
		for (d = (xfs_dqtest_t *) h1->qh_next; d != NULL; ) {
			xfs_dqtest_cmp(d);
			e = (xfs_dqtest_t *) d->HL_NEXT;
			kmem_free(d, sizeof(xfs_dqtest_t)); 
			d = e;
		}	
	}

	if (qmtest_nfails) {
		printf("**************  quotacheck failed  **************\n");
		printf("failures = %d\n", qmtest_nfails);
	} else {
		printf("**************  quotacheck successful! **************\n");
	}
	kmem_free(qmtest_udqtab, qmtest_hashmask * sizeof(xfs_dqhash_t));
	kmem_free(qmtest_pdqtab, qmtest_hashmask * sizeof(xfs_dqhash_t));
	mutex_unlock(&qcheck_lock);
	return (qmtest_nfails);
}

#endif /* DEBUG */
