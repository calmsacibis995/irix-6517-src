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
#ident	 "$Revision: 1.19 $"

/*
 * Routines used in checking limits on file system usage.
 */
#include <sys/types.h>
#include <sys/capability.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/quota.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/sat.h>
#include <sys/vnode.h>
#include "efs_inode.h"
#include "efs_sb.h"

/*
 * Find the dquot structure that should
 * be used in checking i/o on inode ip.
 * The inode should be locked.
 */
struct dquot *
qt_getinoquota(struct inode *ip)
{
	struct dquot *dqp = NULL;
	struct mount *mp;
	struct dquot *xdqp;

	ASSERT(mutex_mine(&ip->i_lock));
	mp = ip->i_mount;
	/*
	 * Check for quotas enabled.
	 */
	if ((mp->m_flags & M_QENABLED) == 0)
		return(NULL);
	/*
	 * Check for someone doing I/O to quota file.
	 */
	if ((ip == mp->m_quotip) || (ip->i_uid == 0))
		return(NULL);
	if (qt_getdiskquota((u_int)ip->i_uid, mp, 0, &xdqp))
		return(NULL);
	dqp = xdqp;
	if (dqp->dq_fhardlimit == 0 && dqp->dq_fsoftlimit == 0 &&
	    dqp->dq_bhardlimit == 0 && dqp->dq_bsoftlimit == 0) {
		qt_dqput(dqp);
		dqp = NULL;
	} else {
		DQUNLOCK(dqp);
	}
	return (dqp);
}

/*
 * Update disk usage, and take corrective action.
 */
int
qt_chkdq(struct inode *ip, long change, int force, int *spaceleft)
{
	struct dquot *dqp;
	u_long ncurblocks;
	int error = 0;

	/*
	 * No need to check M_QENABLED flag, since we take action only if
	 * ip->i_dquot is non-NULL.
	 */
	if (change == 0)
		return (0);
	dqp = ip->i_dquot;
	if (dqp == NULL)
		return (0);
	DQLOCK(dqp);
	dqp->dq_flags |= DQ_MOD;
	if (change < 0) {
		if ((int)dqp->dq_curblocks + change >= 0)
			dqp->dq_curblocks += change;
		else
			dqp->dq_curblocks = 0;
		if (dqp->dq_curblocks < dqp->dq_bsoftlimit)
			dqp->dq_btimelimit = 0;
		DQUNLOCK(dqp);
		return (0);
	}

	ncurblocks = dqp->dq_curblocks + change;
	/*
	 * Allocation. Check hard and soft limits.
	 * Skip checks for privileged user.
	 */
	if (_CAP_ABLE(CAP_QUOTA_MGT))
		goto out;
	/*
	 * Dissallow allocation if it would bring the current usage over
	 * the hard limit or if the user is over his soft limit and his time
	 * has run out.
	 */
	if (ncurblocks > dqp->dq_bhardlimit && dqp->dq_bhardlimit && !force) {
		error = EDQUOT;
		if (spaceleft) {
			*spaceleft = dqp->dq_bhardlimit - dqp->dq_curblocks;
			if (*spaceleft < 0)
				*spaceleft = 0;
			ASSERT(*spaceleft <= change);
		}
	}
	if (ncurblocks >= dqp->dq_bsoftlimit && dqp->dq_bsoftlimit) {
		if (dqp->dq_curblocks < dqp->dq_bsoftlimit ||
		    dqp->dq_btimelimit == 0) {
			dqp->dq_btimelimit =
			    time +
			    ip->i_mount->m_btimelimit;
		} else if (time > dqp->dq_btimelimit && !force) {
			error = EDQUOT;
			if (spaceleft)
				*spaceleft = 0;
		}
	}
out:
	if (error == 0)
		dqp->dq_curblocks = ncurblocks;
	DQUNLOCK(dqp);
	return (error);
}

/*
 * Check the inode limit, applying corrective action.
 */
int
qt_chkiq(struct mount *mp, struct inode *ip, u_int uid, int force)
{
	struct dquot *dqp;
	u_long ncurfiles;
	struct dquot *xdqp;
	int error = 0;

	if ((mp->m_flags & M_QENABLED) == 0)
		return (0);
	/*
	 * Free.
	 * Non-null ip implies that that inode is being freed up.
	 */
	if (ip != NULL) {
		dqp = ip->i_dquot;
		if (dqp == NULL)
			return (0);
		DQLOCK(dqp);
		dqp->dq_flags |= DQ_MOD;
		if (dqp->dq_curfiles)
			dqp->dq_curfiles--;
		if (dqp->dq_curfiles < dqp->dq_fsoftlimit)
			dqp->dq_ftimelimit = 0;
		DQUNLOCK(dqp);
		return (0);
	}

	/*
	 * Skip checks as well as accounting for the privileged. 
	 * Note that it is possible for the uid to be 0 without
	 * the curproc having root credentials. 
	 * Ref BUG# 338286 "bugs in quotas can blindside nfs
	 * locking and UDS"
	 */
	if (uid == 0 || _CAP_ABLE(CAP_QUOTA_MGT))
		goto out2;

	/*
	 * Allocation. Get dquot for <uid, fs>.
	 */
	if (error = qt_getdiskquota((u_int)uid, mp, 0, &xdqp))
		goto out2;
	dqp = xdqp;
	dqp->dq_flags |= DQ_MOD;

	/*
	 * As a matter of policy, no quota accounting is done
	 * for those who don't have any quota limits.
	 * Conversely, we keep accounting if dquot has any limit,
	 * either for blocks OR for files.
	 * This is consistent with quotacheck(1M) and qt_getinoquota()
	 * ie. getinoquota() returns a null dquot if limits == 0,
	 * so i_dquot field of the inode will be NULL. Therefore, 
	 * dq_curfiles field won't get decremented (above) either. 
	 *
	 * XXX none of the current policies seem to account for
	 * limits changing between 0 and > 0 without a quotacheck
	 * or reboot ...
	 */
	if (dqp->dq_fhardlimit == 0 && dqp->dq_fsoftlimit == 0 &&
	    dqp->dq_bhardlimit == 0 && dqp->dq_bsoftlimit == 0)
		goto out1;

	ncurfiles = dqp->dq_curfiles + 1;
	/*
	 * Dissallow allocation if it would bring the current usage over
	 * the hard limit or if the user is over his soft limit and his time
	 * has run out.
	 */
	if (ncurfiles > dqp->dq_fhardlimit && dqp->dq_fhardlimit && !force) {
		error = EDQUOT;
	} else if (ncurfiles >= dqp->dq_fsoftlimit && dqp->dq_fsoftlimit) {
		if (ncurfiles == dqp->dq_fsoftlimit || dqp->dq_ftimelimit==0) {
			dqp->dq_ftimelimit = time + mp->m_ftimelimit;
		} else if (time > dqp->dq_ftimelimit && !force) {
			error = EDQUOT;
		}
	}
	if (error == 0)
		dqp->dq_curfiles++;
out1:
	qt_dqput(dqp);
out2:
	return (error);
}

/*
 * Release a dquot.
 */
void
qt_dqrele(struct dquot *dqp)
{

	if (dqp != NULL) {
		DQLOCK(dqp);
		if ((dqp->dq_cnt == 1) && (dqp->dq_flags & DQ_MOD))
			qt_dqupdate(dqp);
		qt_dqput(dqp);
	}
}
