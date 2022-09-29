/*
 * Copyright 1991, Silicon Graphics, Inc.
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
#ident	"$Revision: 1.45 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mode.h>
#include <sys/pfdat.h>		/* page flushing prototypes */
#include <sys/quota.h>
#include <sys/sema.h>
#include <sys/stat.h>		/* required by IFTOVT */
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/imon.h>
#include "efs_inode.h"
#include "efs_sb.h"

extern vnodeops_t efs_vnodeops;

/*
 * Inode hashing.
 */
#define	IHASHLOG2	9
#define	IHASHSIZE	(1 << IHASHLOG2)
#define	IHASHMASK	(IHASHSIZE - 1)
#define IHASH(dev,ino)	(&ihash[(u_int)((dev)+(ino)) & IHASHMASK])

static struct ihash {
	struct inode	*ih_next;
	mutex_t		ih_lock;
	u_long		ih_version;
} ihash[IHASHSIZE];

#define	ihlock_mp(ih)	mp_mutex_lock(&(ih)->ih_lock, PINOD)
#define	ihunlock_mp(ih)	mp_mutex_unlock(&(ih)->ih_lock)

/*
 * Initialize inodes.
 */
void
ihashinit(void)
{
	int i;

	for (i = 0; i < IHASHSIZE; i++)
		init_mutex(&ihash[i].ih_lock, MUTEX_DEFAULT, "ihash", i);
}

/*
 * Look up an inode by mount point (device) and inumber.
 * If it is in core, honor the locking protocol.
 * If it is not in core, read it in from the specified device.
 * In all cases, a pointer to a locked inode is returned.
 */
int
iget(struct mount *mp, efs_ino_t ino, struct inode **ipp)
{
	dev_t dev;
	struct ihash *ih;
	struct inode *ip, *iq;
	struct vnode *vp;
	vmap_t vmap;
	u_long version;
	int error, newnode;

	SYSINFO.iget++;
	IGETINFO.ig_attempts++;

	dev = mtoefs(mp)->fs_dev;
	ih = IHASH(dev, ino);
again:
	ihlock_mp(ih);
	for (ip = ih->ih_next; ip != NULL; ip = ip->i_next) {
		if (ip->i_number == ino && ip->i_dev == dev) {
			IGETINFO.ig_found++;
			vp = itov(ip);
			VMAP(vp, vmap);
			ihunlock_mp(ih);
			if (!(vp = vn_get(vp, &vmap, 0))) {
				IGETINFO.ig_frecycle++;
				goto again;
			}

			/*
			 * Inode cache hit: if ip is not at the front of
			 * its hash chain, move it there now.
			 */
			ihlock_mp(ih);
			if (ip->i_prevp != &ih->ih_next) {
				if (iq = ip->i_next)
					iq->i_prevp = ip->i_prevp;
				*ip->i_prevp = iq;
				iq = ih->ih_next;
				iq->i_prevp = &ip->i_next;
				ip->i_next = iq;
				ip->i_prevp = &ih->ih_next;
				ih->ih_next = ip;
			}
			ihunlock_mp(ih);
			ilock(ip);
			newnode = (ip->i_mode == 0);
			ASSERT(ip->i_dquot == NULL
			    || ip->i_dquot->dq_uid == ip->i_uid);
			goto out;
		}
	}

	/*
	 * Inode cache miss: save the hash chain version stamp and unlock
	 * the chain, so we don't deadlock in vn_alloc.
	 */
	IGETINFO.ig_missed++;
	version = ih->ih_version;
	ihunlock_mp(ih);

	/*
	 * Read the disk inode attributes into a new inode structure and get
	 * a new vnode for it.  Initialize the inode lock so we can idestroy
	 * it soon if it's a dup.
	 */
	error = efs_iread(mp, ino, &ip);
	if (error)
		return error;
	vp = vn_alloc(mtovfs(mp), IFTOVT(EFSTOSTAT(ip->i_mode & IFMT)),
		      ip->i_rdev);
	bhv_desc_init(&(ip->i_bhv_desc), ip, vp, &efs_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), &(ip->i_bhv_desc));

	init_mutex(&ip->i_lock, MUTEX_DEFAULT, "efsino", (long)vp->v_number);
	ip->i_lockid = (uint64_t)-1L;
	ilock(ip);

	/*
	 * Make sure the vnode's VENF_LOCKING flag corresponds with
	 * the inode's mode.
	 */
	if (MANDLOCK(vp, ip->i_mode))
		VN_FLAGSET(vp, VENF_LOCKING);
	else
		VN_FLAGCLR(vp, VENF_LOCKING);

	/*
	 * Put ip on its hash chain, unless someone else hashed a duplicate
	 * after we released the hash lock.
	 */
	ihlock_mp(ih);
	if (ih->ih_version != version) {
		for (iq = ih->ih_next; iq != NULL; iq = iq->i_next) {
			if (iq->i_number == ino && iq->i_dev == dev) {
				ihunlock_mp(ih);
				vn_bhv_remove(VN_BHV_HEAD(vp), &(ip->i_bhv_desc));
				vn_free(vp);
				efs_idestroy(ip);
				IGETINFO.ig_dup++;
				goto again;
			}
		}
	}

	/*
	 * These values _must_ be set before releasing ihlock!
	 */
	ip->i_hash = ih;
	if (iq = ih->ih_next)
		iq->i_prevp = &ip->i_next;
	ip->i_next = iq;
	ip->i_prevp = &ih->ih_next;
	ih->ih_next = ip;
	ih->ih_version++;
	ihunlock_mp(ih);

	/*
	 * Link ip to its mount and thread it on the mount's inode list.
	 */
	ip->i_mount = mp;
	mlock_mp(mp);
	if (iq = mp->m_inodes)
		iq->i_mprevp = &ip->i_mnext;
	ip->i_mnext = iq;
	ip->i_mprevp = &mp->m_inodes;
	mp->m_inodes = ip;
	munlock_mp(mp);

	/*
	 * Don't look for a quota yet if this inode is unallocated,
	 * because its owner (group) may not be set yet.
	 */
	if (ip->i_mode)
		ip->i_dquot = qt_getinoquota(ip);

	newnode = 1;
out:
	/*
	 * Call hook for imon to see whether ip is of interest and should
	 * have its vnodeops monitored.
	 */
	if (newnode)
		IMON_CHECK(vp, dev, ino);
	*ipp = ip;
	return 0;
}

/*
 * Decrement reference count of an inode structure and unlock it.
 */
void
iput(struct inode *ip)
{
	iunlock(ip);
	irele(ip);
}

void
iinactive(struct inode *ip)
{
	ilock(ip);

	/*
	 * Last use is going away.  Set true size, free if unlinked.
	 */
	if (ip->i_nlink > 0) {
		(void) efs_itrunc(ip, ip->i_size, 0);
	} else {
		(void) efs_itrunc(ip, 0, 1);
		ASSERT(!VN_DIRTY(itov(ip)));
		efs_ifree(ip);
	}

	/*
	 * ip is closed, so reset its readahead detector
	 */
	ip->i_nextbn = 0;
	ip->i_reada = 0;

	/*
	 * Update the inode only if necessary.
	 * efs_itrunc may not do anything so we must do any final
	 * inode push here.
	 * XXXajs
	 * We set i_remember to i_size since this may be the last
	 * time that this inode is flushed.  This ignores the
	 * fact that some delwri pages may not have gone out yet
	 * so we are making uninitialized blocks permanent.
	 */
	if ((ip->i_flags & (IACC|IUPD|ICHG|IMOD)) || ip->i_updtimes) {
		ip->i_remember = ip->i_size;
		(void) efs_iupdat(ip);
	}

	iunlock(ip);
}

void
ireclaim(struct inode *ip)
{
	struct ihash *ih;
	struct inode *iq;
	struct mount *mp;
	vnode_t *vp;

	/*
	 * Remove from old hash list.
	 */
	IGETINFO.ig_reclaims++;
	ih = ip->i_hash;
	ihlock_mp(ih);
	if (iq = ip->i_next)
		iq->i_prevp = ip->i_prevp;
	*ip->i_prevp = iq;
	ihunlock_mp(ih);

	/*
	 * Remove from mount's inode list.
	 */
	mp = ip->i_mount;
	mlock_mp(mp);
	if (iq = ip->i_mnext)
		iq->i_mprevp = ip->i_mprevp;
	*ip->i_mprevp = iq;
	mp->m_ireclaims++;
	munlock_mp(mp);

	/*
	 * Release any quota structure.
	 */
	if (ip->i_dquot) {
		qt_dqrele(ip->i_dquot);
		ip->i_dquot = NULL;
	}

	/*
	 * Get the inode lock now so that we are forced to wait
	 * for efs_sync() to finish with the inode if it is manipulating
	 * it.  This is necessary because efs_sync() manipulates inodes
	 * on the mount list without actually taking a reference on their
	 * corresponding vnodes.  Thus, the vnode could be reclaimed while
	 * efs_sync() is mucking with it.  Getting the lock here makes
	 * us wait.
	 */
	ilock(ip);

	/*
	 * Pull our behavior descriptor from the vnode chain.
	 */
	vp = itov(ip);
	vn_bhv_remove(VN_BHV_HEAD(vp), &(ip->i_bhv_desc));

	efs_idestroy(ip);
}

/*
 * Flush all inactive inodes in mp.  Return true if no user references
 * were found, false otherwise.
 */
int
iflush(struct mount *mp, int flag)
{
	struct inode *ip;
	struct vnode *vp;
	vmap_t vmap;
	int busy = 0;

again:
	mlock_mp(mp);
	for (ip = mp->m_inodes; ip != NULL; ip = ip->i_mnext) {
		/*
		 * It's up to our caller to purge the root and quotas
		 * vnodes later.
		 */
		vp = itov(ip);
		if (vp->v_count != 0) {
			if (vp->v_count == 1
			    && (ip == mp->m_rootip || ip == mp->m_quotip)) {
				continue;
			}
			if ((flag & FLUSH_ALL) == 0) {
				busy = 1;
				break;
			}
			/* ignore busy inodes but continue flushing others */
			continue;
		}
		/*
		 * Sample vp mapping while holding mp locked on MP systems,
		 * so we don't purge a reclaimed or nonexistent vnode.
		 */
		VMAP(vp, vmap);
		munlock_mp(mp);
		vn_purge(vp, &vmap);
		goto again;
	}
	munlock_mp(mp);
	return !busy;
}

/*
 * Locking primitives.
 * Unfortunately, we must detect double trips because certain SVR4
 * vnodes calls nest (VOP_MAP calls VOP_ADDMAP indirectly).  Also,
 * efs_bmap needs the inode locked, and no one locks it in certain
 * cases where bmap is called from page cache code (e.g. unmount).
 */
void
ilock(struct inode *ip)
{
	uint64_t id;
	if ((id = get_thread_id()) == ip->i_lockid) {
		ip->i_locktrips++;
		ASSERT(mutex_mine(&ip->i_lock));
		ASSERT(ip->i_locktrips > 0);
		return;
	}
	mutex_lock(&ip->i_lock, PINOD|PLTWAIT);
	ip->i_lockid = id;
}

int
ilock_nowait(struct inode *ip)
{
	uint64_t id;
	if ((id = get_thread_id()) == ip->i_lockid) {
		ip->i_locktrips++;
		ASSERT(mutex_mine(&ip->i_lock));
		return 1;
	}
	if (mutex_trylock(&ip->i_lock)) {
		ip->i_lockid = id;
		return 1;
	}
	return 0;
}

void
iunlock(struct inode *ip)
{
	ASSERT(mutex_mine(&ip->i_lock));
	ASSERT(get_thread_id() == ip->i_lockid);
	if (ip->i_locktrips > 0) {
		--ip->i_locktrips;
		return;
	}
	ip->i_lockid = (uint64_t)-1L;
	mutex_unlock(&ip->i_lock);
}
