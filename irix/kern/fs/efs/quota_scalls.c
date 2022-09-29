/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	 "$Revision: 1.43 $"

/*
 * Quota system calls.
 */
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/quota.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/uthread.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include "efs_inode.h"
#include "efs_sb.h"
#include <sys/capability.h>

extern time_t time;

static int qt_opendq(struct mount *, caddr_t);
static int qt_start(struct mount *mp, caddr_t addr, ulong flags);
static int qt_nullqblk(struct dqblk *qblk);
static int qt_setquota(int, uid_t, struct mount *, caddr_t);
static int qt_getquota(uid_t, struct mount *, caddr_t);
static int qt_sync(struct mount *);
static int qt_activate(struct mount *, caddr_t);

/* 
 * VFSOP for all quotactl(2) system calls on EFS filesystems.
 */
int
efs_quotactl(
	struct bhv_desc *bdp,
	int		cmd,
	int		id,
	caddr_t		addr)
{
	struct mount 	*mp;
	int 		error;
	
	if ((caddr_t)v.ve_dquot == NULL)
		return EINVAL;
	/*
	 * EFS doesn't like big uids..
	 */
	if (id > 0xffff)
		return EOVERFLOW;
	if (id < 0)
		return EINVAL;

	mp = bhvtom(bdp);

	/*
	 * vfsp is already marked busy, since
	 * we want to prevent other quotactl() syscalls which could potentially
	 * try to disable/enable, activate quotas under us.
	 */
	switch (cmd) {

	      case Q_QUOTAON:
		error = qt_opendq(mp, addr);
		break;
		
	      case Q_QUOTAOFF:
		error = qt_closedq(mp, 0);
		break;
		
	      case Q_SETQUOTA:
	      case Q_SETQLIM:
		error = qt_setquota(cmd, id, mp, addr);
		break;
		
	      case Q_GETQUOTA:
		error = qt_getquota(id, mp, addr);
		break;
		
	      case Q_SYNC:
		error = qt_sync(mp);
		break;
		
	      case Q_ACTIVATE:
		error = qt_activate(mp, addr);
		break;
		
		/* XXX TBD */
	      case Q_XGETQUOTA:
	      case Q_XSETQLIM:
	      case Q_XGETPQUOTA:
	      case Q_XSETPQLIM:
		error = ENOTSUP;
		break;
		
	      default:
		error = EINVAL;
		break;
	}

	return(error);
}

/*
 * Set the quota file up for a particular file system.
 * Called as the result of a setquota system call.
 */
int
qt_opendq(
	struct mount *mp,
	caddr_t addr)			/* quota file */
{
	if (!_CAP_ABLE(CAP_QUOTA_MGT))
		return (EPERM);
	if (mp->m_flags & M_QENABLED)
		return(EEXIST);
	if (mp->m_flags & M_QACTIVE) {
		/*
		 * We have to flush all existing entries to disk and re-initialize
		 * since root could have mucked around with the quotas file for
		 * which we hold a reference in the mount table entry. So we
		 * free the m_quotip pointer in the mount table and re-obtain it.
		 */
		(void) qt_closedq(mp, 1);
		ASSERT((mp->m_flags & (M_QENABLED|M_QACTIVE)) == 0);
	}
	return(qt_start(mp, addr, M_QENABLED));
}

/*
 * Common to both qt_activate() and qt_opendq()
 */
int
qt_start(
	struct mount *mp,
	caddr_t addr,
	ulong flags)
{
	struct vnode *vp;
	struct inode *ip;
	struct dquot *dqp;
	int error;
	ssize_t resid;
	struct qt_header *qhp = NULL;
	size_t allocsize;
	enum uio_rw rw;
	u_long ireclaims;
	bhv_desc_t *bdp;
	vn_bhv_head_t *bhp;

	if (error = lookupname(addr, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp, NULL))
		return (error);
	bhp = VN_BHV_HEAD(vp);
	bdp = vn_bhv_lookup_unlocked(bhp, &efs_vnodeops);
	if (bdp == NULL) {
		return EINVAL;
	}
	ip = bhvtoi(bdp);
	if (ip->i_mount != mp || vp->v_type != VREG || ip->i_uid) {
		VN_RELE(vp);
		return (EACCES);
	}
	if (flags==M_QACTIVE && ip->i_size == 0 && !_CAP_ABLE(CAP_QUOTA_MGT)) {
		VN_RELE(vp);
		return (EACCES);
	}
	ASSERT(mp->m_quotip == NULL);
	mp->m_quotip = ip;
	/*
	 * Write the index which will contain the uid->offset
	 * mapping for the quotas file. The index size defaults to
	 * Q_MININDEX (4K) which can support upto 2045 users. If support
	 * for more users is necessary, quotacheck(1M) can be used to
	 * increment the index size from user space. The index can be
	 * upto Q_MAXINDEX size.
	 */
	if (ip->i_size == 0) {
		allocsize = Q_MININDEX;
		qhp = kmem_zalloc(allocsize, KM_SLEEP);
		qhp->qh_magic = Q_MAGIC;
		qhp->qh_index = Q_MININDEX;
		rw = UIO_WRITE;
	} else {
		allocsize = Q_HEADER;
		qhp = kmem_zalloc(allocsize, KM_SLEEP);
		rw = UIO_READ;
	}
	error = vn_rdwr(rw, vp, (caddr_t)qhp, allocsize, 0, UIO_SYSSPACE, 0,
			getfsizelimit(),
			get_current_cred(),
			&resid, &curuthread->ut_flid);
	if (error || resid ||
	    qhp->qh_magic != Q_MAGIC ||
	    !(qhp->qh_index >= Q_MININDEX && qhp->qh_index <= Q_MAXINDEX) ||
	    qhp->qh_nusers > Q_MAXUSERS ||
	    qhp->qh_index > mp->m_quotip->i_size) {
		if (!error)
			error = EACCES;
		kmem_free(qhp, allocsize);
		mp->m_quotip = NULL;
		mp->m_qindex = 0;
		VN_RELE(vp);
		return(error);
	}
	mp->m_qindex = qhp->qh_index;
	kmem_free(qhp, allocsize);
	/*
	 * The file system time limits are in the super user dquot.
	 * The time limits set the relative time the other users
	 * can be over quota for this file system.
	 * If it is zero a default is used (see sys/quota.h).
	 */
	error = qt_getdiskquota((u_int)0, mp, 1, &dqp);
	if (error == 0) {
		mp->m_btimelimit =
		    (dqp->dq_btimelimit? dqp->dq_btimelimit: DQ_BTIMELIMIT);
		mp->m_ftimelimit =
		    (dqp->dq_ftimelimit? dqp->dq_ftimelimit: DQ_FTIMELIMIT);
		mp->m_flags |= flags;
		qt_dqput(dqp);
		/*
		 * Upon success, the v_count on the quota vnode is
		 * not decremented, since we hold a reference to it
		 * in the efs mount structure.
		 */
	} else {
		/*
		 * Some sort of I/O error on the quota file.
		 */
		mp->m_quotip = NULL;
		mp->m_qindex = 0;
		VN_RELE(vp);
	}

	/*
	 * EFS assumes that the i_dquot pointer is non-NULL if an inode has
	 * relevant quota stuff associated with it. So, attach a quota structure
	 * to every incore inode. Otherwise, for example, quotaoff, followed by
	 * a quotaon can leave inodes with NULL i_dquot ptrs, and quota accounting
	 * can end up being wrong. BUG #263199, ...
	 */

loop:
	mlock(mp);
	for (ip = mp->m_inodes; ip != NULL; ip = ip->i_mnext) {
		vmap_t vmap;

		if ((ip == mp->m_quotip) || (ip == mp->m_rootip))
			continue;
		vp = itov(ip);
		/* 
		 * Try to get the mountlock out of order...
		 * We dont keep the mountlock across the qt_getinoquota() call,
		 * since it could take a while..
		 */
		if (ilock_nowait(ip) == 0) {
			VMAP(vp, vmap);
			ireclaims = mp->m_ireclaims;
			munlock(mp);
			if (!(vp = vn_get(vp, &vmap, 0)))
				goto loop;
#ifdef DEBUG
			bhp = VN_BHV_HEAD(vp);
			bdp = vn_bhv_lookup_unlocked(bhp, &efs_vnodeops);
			ASSERT(bdp != NULL);
			ASSERT(ip == bhvtoi(bdp));
#endif
			ilock(ip);
			VN_RELE(vp);
		} else {
			ireclaims = mp->m_ireclaims;
			munlock(mp);
		}
		if (ip->i_dquot == NULL)
			ip->i_dquot = qt_getinoquota(ip);
		iunlock(ip);
		mlock(mp);

		/*
		 * Don't start over unless the list has changed
		 */
		if (ireclaims != mp->m_ireclaims) {
			munlock(mp);
			goto loop;
		}
	}
	munlock(mp);

	return (error);
}

/*
 * Close off disk quotas for a file system.
 */
int
qt_closedq(struct mount *mp, int unmounting)
{
	struct dquot *dqp;
	struct inode *qip, *ip;
	struct vnode *qvp, *vp;
	u_long ireclaims;
#ifdef DEBUG
	bhv_desc_t *bdp;
	vn_bhv_head_t *bhp;
#endif

	if (!_CAP_ABLE(CAP_QUOTA_MGT))
		return (EPERM);
	/*
	 * If we are in the process of a umount, force everything to disk
	 * and disable quotas.
	 */
	if (unmounting) {
		if ((mp->m_flags & (M_QENABLED | M_QACTIVE)) == 0)
			return (0);
		mp->m_flags &= ~(M_QENABLED | M_QACTIVE);
	} else {
		if ((mp->m_flags & M_QENABLED) == 0)
			return (EEXIST);
		mp->m_flags &= ~M_QENABLED;
		ASSERT((mp->m_flags & M_QACTIVE) == 0);
	}

	qip = mp->m_quotip;
	ASSERT(qip != NULL);
	/*
	 * The following code is very similar to what
	 * efs_sync() does
	 */
loop:
	mlock(mp);
	for (ip = mp->m_inodes; ip != NULL; ip = ip->i_mnext) {
		vmap_t vmap;

		if ((ip == qip) || (ip == mp->m_rootip))
			continue;
		vp = itov(ip);

		if (ilock_nowait(ip) == 0) {
			VMAP(vp, vmap);
			ireclaims = mp->m_ireclaims;
			munlock(mp);
			if (!(vp = vn_get(vp, &vmap, 0)))
				goto loop;
#ifdef DEBUG
			bhp = VN_BHV_HEAD(vp);
			bdp = vn_bhv_lookup_unlocked(bhp, &efs_vnodeops);
			ASSERT(bdp != NULL);
			ASSERT(ip == bhvtoi(bdp));
#endif
			ilock(ip);
			VN_RELE(vp);
		} else {
			ireclaims = mp->m_ireclaims;
			munlock(mp);
		}
		/* 
		 * We didn't keep the mountlock across the dqrele() call,
		 * since it can take a while..
		 */
		qt_dqrele(ip->i_dquot);
		ip->i_dquot = NULL;
		iunlock(ip);
		mlock(mp);
		if (mp->m_ireclaims != ireclaims) {
			munlock(mp);
			goto loop;
		}
	}
	munlock(mp);
	/*
	 * Run down the dquot table and clean and invalidate the
	 * dquots for this file system.
	 */
	mutex_lock(&qfree_sema, PZERO);
	for (dqp = dquot; dqp < (struct dquot *)v.ve_dquot; dqp++) {
		if (dqp->dq_mp == mp) {
			DQLOCK(dqp);
			ASSERT(dqp->dq_cnt == 0);
			ASSERT((dqp->dq_flags & DQ_MOD) == 0);
			qt_dqinval(dqp);
			DQUNLOCK(dqp);
		}
	}
	mutex_unlock(&qfree_sema);
	/*
	 * sync and release the quota file inode
	 */
	qvp = itov(qip);
	VN_RELE(qvp);
	if (unmounting) {
		vmap_t vmap;
		VMAP(qvp, vmap);
		vn_purge(qvp, &vmap);
	}
	mp->m_quotip = NULL;
	mp->m_ftimelimit = 0;
	mp->m_btimelimit = 0;
	mp->m_qindex = 0;
	return (0);
}

/*
 * Set various fields of the dqblk according to the command.
 * Q_SETQUOTA - assign an entire dqblk structure.
 * Q_SETQLIM - assign a dqblk structure except for the usage.
 */
int
qt_setquota(
	int cmd,
	uid_t uid,
	struct mount *mp,
	caddr_t addr)
{
	struct dquot *dqp;
	struct dquot *xdqp;
	struct dqblk newlim;
	int error;

	if (!_CAP_ABLE(CAP_QUOTA_MGT))
		return (EPERM);
	if ((mp->m_flags & (M_QENABLED | M_QACTIVE)) == 0)
		return (ESRCH);
	ASSERT(mp->m_quotip != NULL);
	if (copyin(addr, &newlim, sizeof newlim))
		return (EFAULT);
	/*
	 * If we are being passed a null quota, we zero out the disk entry
	 * only if it already exists. If a disk entry does not already exist 
	 * we ignore the new entry.
	 * If non-null, allocate a new entry if necessary.
	 */
	if (qt_nullqblk(&newlim)) {
		if (error = qt_getdiskquota((u_int)uid, mp, 0, &xdqp))
			return(error);
		if (qt_nullqblk(&xdqp->dq_dqb)) {
			qt_dqput(xdqp);
			return(0);
		}
	} else if (error = qt_getdiskquota((u_int)uid, mp, 1, &xdqp))
		return (error);

	dqp = xdqp;
	/*
	 * Don't change disk usage on Q_SETQLIM
	 */
	if (cmd == Q_SETQLIM) {
		newlim.dqb_curblocks = dqp->dq_curblocks;
		newlim.dqb_curfiles = dqp->dq_curfiles;
	}
	dqp->dq_dqb = newlim;
	if (uid == 0) {
		/*
		 * Timelimits for the super user set the relative time
		 * the other users can be over quota for this file system.
		 * If it is zero a default is used (see sys/quota.h).
		 */
		mp->m_btimelimit =
		    newlim.dqb_btimelimit? newlim.dqb_btimelimit: DQ_BTIMELIMIT;
		mp->m_ftimelimit =
		    newlim.dqb_ftimelimit? newlim.dqb_ftimelimit: DQ_FTIMELIMIT;
	} else {
		/*
		 * If the user is now over quota, start the timelimit.
		 * The user will not be warned.
		 */
		if (dqp->dq_curblocks >= dqp->dq_bsoftlimit &&
		    dqp->dq_bsoftlimit && dqp->dq_btimelimit == 0)
			dqp->dq_btimelimit = time + mp->m_btimelimit;
		else
			dqp->dq_btimelimit = 0;
		if (dqp->dq_curfiles >= dqp->dq_fsoftlimit &&
		    dqp->dq_fsoftlimit && dqp->dq_ftimelimit == 0)
			dqp->dq_ftimelimit = time + mp->m_ftimelimit;
		else
			dqp->dq_ftimelimit = 0;
	}
	dqp->dq_flags |= DQ_MOD;
	qt_dqupdate(dqp);
	qt_dqput(dqp);
	return (0);
}

/*
 * Q_GETQUOTA - return current values in a dqblk structure.
 */
int
qt_getquota(
	uid_t uid,
	struct mount *mp,
	caddr_t addr)
{
	struct dquot *dqp;
	struct dquot *xdqp;
	int error;

	if (uid != get_current_cred()->cr_ruid && !_CAP_ABLE(CAP_QUOTA_MGT))
		return (EPERM);
	if ((mp->m_flags & (M_QENABLED | M_QACTIVE)) == 0)
		return (ESRCH);
	if (error = qt_getdiskquota((u_int)uid, mp, 0, &xdqp))
		return (error);
	dqp = xdqp;
	/*
	 * If there is no entry present, then we return a zeroed out block,
	 * which implies that this uid has no quotas.
	 */
	error = copyout(&dqp->dq_dqb, addr, sizeof dqp->dq_dqb);
	qt_dqput(dqp);
	return (error ? EFAULT : 0);
}

/*
 * Q_SYNC - sync quota files to disk.
 * Since we enforce this upon unmounting or quotaoff, don't bother
 * if we cannot get the lock.
 */
int
qt_sync(
	struct mount *mp)
{
	struct dquot *dqp;

	if (mp != NULL && (mp->m_flags & (M_QENABLED|M_QACTIVE)) == 0)
		return (ESRCH);
	for (dqp = dquot; dqp < (struct dquot *)v.ve_dquot; dqp++) {
		if (mutex_trylock(&dqp->dq_sema)) {
			if ((dqp->dq_flags & DQ_MOD) &&
			    (dqp->dq_mp != NULL) &&
			    (mp == NULL || dqp->dq_mp == mp) &&
			    (dqp->dq_mp->m_flags & (M_QENABLED|M_QACTIVE))) {
				qt_dqupdate(dqp);
			}
			DQUNLOCK(dqp);
		}
	}
	return (0);
}

/*
 * Q_ACTIVATE - set up mp->m_quotip etc for updating
 * Called from the quotactl system call, when we want to update the
 * quotas file for file system without enabling quotas. This is to permit
 * the user command "edquota" to update the quotas without mucking around
 * with the "quotas" file itself.
 */
int
qt_activate(
	struct mount *mp,
	caddr_t addr)			/* quota file */
{
	if (mp->m_flags & (M_QENABLED | M_QACTIVE))
		return(0);
	return(qt_start(mp, addr, M_QACTIVE));
}

int
qt_nullqblk(
	struct dqblk *qblk)
{
	if ((qblk->dqb_bhardlimit !=0) ||
	    (qblk->dqb_bsoftlimit !=0) ||
	    (qblk->dqb_curblocks !=0) ||
	    (qblk->dqb_fhardlimit !=0) ||
	    (qblk->dqb_fsoftlimit !=0) ||
	    (qblk->dqb_curfiles !=0) ||
	    (qblk->dqb_btimelimit !=0) ||
	    (qblk->dqb_ftimelimit !=0))
		return(0);
	else
		return(1);
}
