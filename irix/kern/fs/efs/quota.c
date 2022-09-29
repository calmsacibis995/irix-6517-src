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
#ident	 "$Revision: 1.27 $"

/*
 * Code pertaining to management of the in-core data structures.
 */
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/quota.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/tcp-param.h>		/* prototype for _insque */
#include "efs_inode.h"
#include "efs_sb.h"

extern time_t time;
/*
 * Dquot cache - hash chain headers.
 */
#define	NDQHASH		64		/* incore quota hash chain */
#define	DQHASH(uid, mp) \
	(((__psunsigned_t)(mp) + (__psunsigned_t)(uid)) & (NDQHASH-1))

struct	dqhead	{
	struct	dquot	*dqh_forw;	/* MUST be first */
	struct	dquot	*dqh_back;	/* MUST be second */
} dqhead[NDQHASH];

/*
 * Dquot free list. qfree_sema controls the pointers and dq_uid and dq_mp
 * fields in the incore struct dquot. The ordering of the different locks
 * is vfs_busy() (all quotactl() syscall entry points), qfree_sema,
 * dqp->dq_sema and the quotas inode lock.
 */
struct dquot dqfreelist;

#define dqinsheadfree(DQP) { \
	(DQP)->dq_freef = dqfreelist.dq_freef; \
	(DQP)->dq_freeb = &dqfreelist; \
	dqfreelist.dq_freef->dq_freeb = (DQP); \
	dqfreelist.dq_freef = (DQP); \
}

#define dqinstailfree(DQP) { \
	(DQP)->dq_freeb = dqfreelist.dq_freeb; \
	(DQP)->dq_freef = &dqfreelist; \
	dqfreelist.dq_freeb->dq_freef = (DQP); \
	dqfreelist.dq_freeb = (DQP); \
}

#define dqremfree(DQP) { \
	(DQP)->dq_freeb->dq_freef = (DQP)->dq_freef; \
	(DQP)->dq_freef->dq_freeb = (DQP)->dq_freeb; \
}

typedef	struct dquot *DQptr;

static int qt_dqoff(u_short, struct inode *, int, off_t *);

/*
 * Initialize quota caches.
 */
void
qt_init()
{
	struct dqhead *dhp;
	struct dquot *dqp;

	ASSERT((caddr_t)v.ve_dquot == 0);
	/*
	 * Initialize the incore quota cache.
	 */
	for (dhp = &dqhead[0]; dhp < &dqhead[NDQHASH]; dhp++) {
		dhp->dqh_forw = dhp->dqh_back = (DQptr)dhp;
	}

	mutex_init(&qfree_sema, MUTEX_DEFAULT, "qfree");
	dqfreelist.dq_freef = dqfreelist.dq_freeb = (DQptr)&dqfreelist;

	dquot = kmem_zalloc(sizeof(struct dquot) * v.v_dquot, KM_SLEEP);
	v.ve_dquot = (char *)(dquot + v.v_dquot);
	for (dqp = dquot; dqp < (struct dquot *)v.ve_dquot; dqp++) {
		dqp->dq_forw = dqp->dq_back = dqp;
		dqinsheadfree(dqp);
		init_mutex(&dqp->dq_sema, MUTEX_DEFAULT, "dq", dqp-dquot);
	}
}

/*
 * Obtain the user's on-disk quota limit for file system specified.
 * The caller has checked and validated that either the quotas is indeed
 * enabled for this file system or is being enabled (ie the mp->m_quotip
 * pointer is non null). Returns non-zero on encountering an error.
 * allocate determines whether a slot is allocated, if there are currently
 * no quotas
 */
int
qt_getdiskquota(u_int uid, struct mount *mp, int allocate, struct dquot **dqpp)
{
	struct dquot *dqp = NULL;
	struct dqhead *dhp;
	struct inode *qip;
	int error = 0;
	off_t offset;
	ssize_t resid;

	*dqpp = NULL;
	dhp = &dqhead[DQHASH(uid, mp)];
	qip = mp->m_quotip;
	ASSERT(qip != NULL);
	/*
	 * Check the cache first.
	 */
	mutex_lock(&qfree_sema, PZERO);
	for (dqp = dhp->dqh_forw; dqp != (DQptr)dhp; dqp = dqp->dq_forw) {
		if (dqp->dq_uid != uid || dqp->dq_mp != mp)
			continue;
		DQLOCK(dqp);
		/*
		 * Cache hit with no references.
		 * Take the structure off the free list.
		 */
		if (dqp->dq_cnt == 0)
			dqremfree(dqp);
		dqp->dq_cnt++;
		mutex_unlock(&qfree_sema);
		/*
		 * If this entry has no slot in the index, allocate one now.
		 */
		if ((dqp->dq_off == 0) && allocate)
			goto get_offset;
		*dqpp = dqp;
		return(0);
	}
	/*
	 * Not in cache.
	 * Get dquot at head of free list. qfree_sema is still locked.
	 */
	if ((dqp = dqfreelist.dq_freef) == &dqfreelist) {
		cmn_err_tag(77,CE_ALERT,
			    "Incore quota table overflow. lboot(1M) with larger value for NDQUOT");
		mutex_unlock(&qfree_sema);
		return(ENOSPC);
	}
	DQLOCK(dqp);
	ASSERT(dqp->dq_cnt == 0);
	ASSERT((dqp->dq_flags & (DQ_MOD)) == 0);
	/*
	 * Take it off the free list, and off the hash chain it was on.
	 * Then put it on the new hash chain.
	 */
	dqremfree(dqp);
	_remque(dqp);
	dqp->dq_cnt = 1;
	dqp->dq_uid = uid;
	dqp->dq_mp = mp;
	dqp->dq_off = 0;
	dqp->dq_flags = 0;
	_insque(dqp, dhp);
	/*
	 * Release qfree_sema here. If an i/o error occures and we need to
	 * stick the structure back on the freelist, qt_dqput() will
	 * re-acquire it.
	 */
	mutex_unlock(&qfree_sema);
get_offset:
	ilock(qip);
	error = qt_dqoff((u_short)uid, qip, allocate, &offset);
	if ((error == 0) && (offset < qip->i_size)) {
		/*
		 * Read quota info off disk.
		 */
		dqp->dq_off = offset;
		/*
		 * since quotas are maintained by the system - really 
		 * don't want to use user's rlimit since a small rlimit
		 * could limit our ability to update the quota file
		 */
		error = vn_rdwr(UIO_READ, itov(qip), (caddr_t)&dqp->dq_dqb,
				sizeof dqp->dq_dqb, offset, UIO_SYSSPACE, 0,
				RLIM_INFINITY, get_current_cred(),
				&resid, sys_flid);
		iunlock(qip);
		if (error || resid) {
			error = EIO;
			bzero(&dqp->dq_dqb, sizeof dqp->dq_dqb);
			goto errexit;
		}
		/*
		 * If the user is over the limit and timer has not been started,
		 * then start it. Check to make sure that the filesystem is not
		 * mounted readonly, since we could be doing this if quotas
		 * has not been enabled.
		 */
		if (dqp->dq_curfiles >= dqp->dq_fsoftlimit &&
		    dqp->dq_fsoftlimit && dqp->dq_ftimelimit == 0) {
			if (!(mtovfs(mp)->vfs_flag & VFS_RDONLY)) {
				dqp->dq_flags |= DQ_MOD;
				dqp->dq_ftimelimit = time + mp->m_ftimelimit;
			}
		}
		if (dqp->dq_curblocks >= dqp->dq_bsoftlimit &&
		    dqp->dq_bsoftlimit && dqp->dq_btimelimit == 0) {
			if (!(mtovfs(mp)->vfs_flag & VFS_RDONLY)) {
				dqp->dq_flags |= DQ_MOD;
				dqp->dq_btimelimit = time + mp->m_btimelimit;
			}
		}
	} else {
		/* 
		 * we can get here if:
		 * 1) offset beyond end of file, error == 0
		 * 2) error == ENODATA - something wrong with file but continue
		 * 3) error != ENODATA - error reading file.
		 * All but 2) cause us to continue...
		 */
		iunlock(qip);
		if (error && error != ENODATA) {
			/*
			 * I/O error in reading quota file.
			 * Return error to the caller and qt_dqput() the dquot
			 * structure, possibly putting it back on to the
			 * freelist.
			 */
errexit:
			dqp->dq_flags = 0;
			dqp->dq_off = 0;
			qt_dqput(dqp);
			dqp = NULL;
		} else {
			error = 0;
			bzero(&dqp->dq_dqb, sizeof dqp->dq_dqb);
		}
	}
	*dqpp = dqp;
	return(error);
}

/*
 * Release dquot.
 * If we are going to put it on the freelist, we need to grab the
 * quota structure lock (which we already have) and the hash/freelist lock
 * in the right order.
 */
void
qt_dqput(struct dquot *dqp)
{

	ASSERT(dqp->dq_cnt != 0);
	if (dqp->dq_cnt == 1) {
		/*
	         * This is needed for the qt_dqput() call from qt_chkiq()
	         */
		if (dqp->dq_flags & DQ_MOD)
			qt_dqupdate(dqp);
		if (!mutex_trylock(&qfree_sema)) {
			/*
		 	 * Grab the freelist semaphore and the quota structure
		 	 * semaphore in the right order.
		 	 */
			DQUNLOCK(dqp);
			mutex_lock(&qfree_sema, PZERO);
			DQLOCK(dqp);
		}
		if (--dqp->dq_cnt == 0)
			dqinstailfree(dqp);
		mutex_unlock(&qfree_sema);
	} else
		dqp->dq_cnt--;
	DQUNLOCK(dqp);
}

/*
 * Update on disk quota info.
 */
void
qt_dqupdate(struct dquot *dqp)
{
	struct inode *qip;
	ssize_t resid;
	int error;
	off_t offset;

	qip = dqp->dq_mp->m_quotip;
	ASSERT(qip != NULL);
	ASSERT(mutex_mine(&dqp->dq_sema));
	ASSERT(dqp->dq_flags & DQ_MOD);
	dqp->dq_flags &= ~DQ_MOD;
	ilock(qip);

	if (dqp->dq_off == 0) {
		error = qt_dqoff(dqp->dq_uid, qip, 1, &offset);
		if (error || (offset > qip->i_size + sizeof(struct dqblk)))
			goto out;
		dqp->dq_off = offset;
	}

	(void) vn_rdwr(UIO_WRITE, itov(qip), (caddr_t)&dqp->dq_dqb,
		       sizeof dqp->dq_dqb, dqp->dq_off, UIO_SYSSPACE, 0,
		       RLIM_INFINITY, get_current_cred(), &resid,
		       sys_flid);
out:
	/*
	 * Ignore any errors, since this is only a side effect to our
	 * action of syncing or freeing up the inode or some such similar
	 * action.
	 */
	iunlock(qip);
}

/*
 * Invalidate a dquot.
 * Take the dquot off its hash list and put it on a private,
 * unfindable hash list. Also, put it at the head of the free list.
 * Both the freelist semaphore and the quota structure semaphore should
 * be locked when this is called.
 */
void
qt_dqinval(struct dquot *dqp)
{

	ASSERT(mutex_mine(&qfree_sema));
	ASSERT(mutex_mine(&dqp->dq_sema));
	ASSERT(!dqp->dq_cnt);
	ASSERT(!(dqp->dq_flags & DQ_MOD));
	dqp->dq_flags = 0;
	_remque(dqp);
	dqremfree(dqp);
	dqp->dq_cnt = 0;
	dqp->dq_uid = -1;
	dqp->dq_off = 0;
	dqp->dq_mp = NULL;
	dqp->dq_forw = dqp;
	dqp->dq_back = dqp;
	dqp->dq_flags = 0;
	dqinsheadfree(dqp);
}

/*
 * This returns the byte offset into the quotas file, where the quota
 * information for the uid is located. If the entry does not exist and
 * if indicated, it will allocate a new slot for the uid.
 * qip is the quotas file inode pointer which must be locked by the caller.
 * Returns 0 in success, errno on error.
 * As a special case - ENODATA is returned if the file isn't large enough (
 * doesn't contain the requested uid).
 */
static int
qt_dqoff(
	u_short uid,
	struct inode *qip,
	int allocate,
	off_t *off)
{
	int offset = -1;
	u_short *uidp;
	struct qt_header *qhp;
	size_t index_size;
	struct dqblk *dqp;
	int maxusers = 0;
	ssize_t resid;
	int error = 0;

	/*
	 * The first three u_shorts contain the magic number, size of the
	 * index(in bytes) and the number of entries in the index respectively.
	 * Following this we have a sequence of u_shorts which gives the uid
	 * of the users having quotas in this file system.
	 * The offset of where the quota information is stored in the file is
	 * a direct funtion of where the uid slot is wrt to &header->qh_uid[0].
	 * The index block is a minimum of Q_MININDEX bytes (supporting
	 * 2045 users) and can go upto Q_MAXINDEX bytes.
	 */
	index_size = qip->i_mount->m_qindex;
	if ((index_size < Q_MININDEX) || (index_size > Q_MAXINDEX) ||
	    (index_size > qip->i_size)) {
		/*
		 * Bogus quotas file. Don't even bother continuing.
		 */
		return(EIO);
	}
	qhp = kmem_alloc(index_size, KM_SLEEP);
	error = vn_rdwr(UIO_READ, itov(qip), (caddr_t)qhp,
				      index_size, 0, UIO_SYSSPACE, 0,
				      RLIM_INFINITY, get_current_cred(),
				      &resid, sys_flid);
	if (error)
		goto out;
	if (resid) {
		/* file not big enough - old code returned 0 w/o errno */
		error = ENODATA;
		goto out;
	}

	/*
	 * The index actually includes the header that has to be accounted for.
	 */
	maxusers = (index_size/sizeof(u_short)) - (Q_HEADER/sizeof(u_short));
	if ((qhp->qh_magic != Q_MAGIC) ||
	    (qhp->qh_index < Q_MININDEX) ||
	    (qhp->qh_index > Q_MAXINDEX) ||
	    (qhp->qh_nusers > maxusers)) {
		error = EINVAL;
		goto out;
	}

	for (uidp = (u_short *)&qhp->qh_uid[0];
	     uidp - (u_short *)&qhp->qh_uid[0] < qhp->qh_nusers;
	     uidp++) {
		if ((u_short)*uidp == (u_short)uid) {
			offset = (int)(uidp - &qhp->qh_uid[0]);
			goto out;
		}
	}
	if (!allocate) {
		error = ENODATA;
		goto out;
	}

	/*
	 * Couldn't find offset for uid. If we have been asked to allocate,
	 * (done only by qt_setquota() and qt_start()) we will
	 * provide a slot for this uid. Increment number of entries.
	 * Since the index block containing uid-offset mapping has changed,
	 * we write it out to disk.
	 */
	if (qhp->qh_nusers == maxusers) {
		error = ENOSPC;
		goto out;
	}
	offset = qhp->qh_nusers++;
	qhp->qh_uid[offset] = (u_short)uid;

	error = vn_rdwr(UIO_WRITE, itov(qip), (caddr_t)qhp,
				((offset+1) * 2) + Q_HEADER, 0, UIO_SYSSPACE,
				0, RLIM_INFINITY,
				get_current_cred(), &resid, sys_flid);
	if (error)
		goto out;
	if (resid) {
		/* couldn't extend file - old code returned 0 w/o errno */
		error = ENODATA;
		goto out;
	}
	/*
	 * We also write out the zeroed out struct dqblk for the user <uid>.
	 */
	dqp = (struct dqblk *)qhp;
	bzero(dqp, sizeof *dqp);
	error = vn_rdwr(UIO_WRITE, itov(qip), (caddr_t)dqp,
				sizeof *dqp, index_size +
				(offset * sizeof(struct dqblk)), UIO_SYSSPACE,
				0, RLIM_INFINITY,
				get_current_cred(), &resid, sys_flid);
	if (error)
		goto out;
	if (resid) {
		/* couldn't write index - old code returned 0 w/o errno */
		error = ENODATA;
		goto out;
	}
out:
	kmem_free(qhp, index_size);
	if (!error) {
		ASSERT(offset >= 0);
		*off = index_size + ((off_t)offset * sizeof(struct dqblk));
	}
	return error;
}
