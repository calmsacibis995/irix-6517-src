/*
 * SGI rnode implementation.
 */
#ident	"$Revision: 1.79 $"

#include "types.h"
#include <sys/cred.h>
#include <sys/debug.h>		/* for ASSERT and METER */
#include <sys/param.h>
#include <sys/systm.h>		/* for splhi */
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/imon.h>
#include <sys/proc.h>
#include <sys/uthread.h>
#include "nfs.h"
#include "nfs_clnt.h"
#include "nfs3_clnt.h"
#include "rnode.h"
#include "bds.h"

extern vnodeops_t nfs_vnodeops;

/*
 * Hash table linkage: the first two members must match struct rnode.
 */
#define	RHASHLOG2	8
#define	RHASHSIZE	(1<<RHASHLOG2)
#define	RHASHMASK	(RHASHSIZE-1)

static struct rnode	*rhash[RHASHSIZE];	/* open hash table */
mutex_t			rnodemonitor;	/* serializes table operations */
sv_t			vnodewait;	/* synchronize vn_alloc waiters */

#ifdef OS_METER
static struct {
	u_long	uncaches;	/* allocations that uncached a node */
	u_long	destroys;	/* rnodes deleted from table */
	u_long	steps;		/* hash chain links traversed */
	u_long	hits;		/* rnodes found by makenfsnode */
	u_long	misses;		/* rnodes not found */
	u_long	losthits;	/* lost race for a cached node */
} rnodemeter;
#endif

#define	RHASH(fh) \
	(&rhash[((fh)->fh_hash[0] ^ (fh)->fh_hash[1] ^ (fh)->fh_hash[2] \
		 ^ (fh)->fh_hash[3] ^ (fh)->fh_hash[4] ^ (fh)->fh_hash[5] \
		 ^ (fh)->fh_hash[6] ^ (fh)->fh_hash[7]) & RHASHMASK])

#define ADDTOCHAIN(rp, rh) { \
	if ((rp)->r_next = *(rh)) \
		(*(rh))->r_prevp = &(rp)->r_next; \
	(rp)->r_prevp = (rh); \
	*(rh) = (rp); \
}

#define	REMOVEFROMCHAIN(rp) { \
	struct rnode *_rq; \
	if (_rq = (rp)->r_next) \
		_rq->r_prevp = (rp)->r_prevp; \
	*(rp)->r_prevp = _rq; \
}

#define	INITCHAIN(rp) \
	((rp)->r_next = 0, (rp)->r_prevp = &(rp))

static struct zone *rnode_zone;

/*
 * Initialize the rnode semaphores, zone allocator and spinlock pool.
 */
void
initrnodes()
{
	mutex_init(&rnodemonitor, MUTEX_DEFAULT, "rtblmon");
	sv_init(&vnodewait, SV_DEFAULT, "vnowait");
	rnode_zone = kmem_zone_init(sizeof(struct rnode),"NFS rnodes");
}

/*
 * Check for processes awaiting the association of an vnode with rp,
 * via nfs_iread, and wake all such waiters in the system up.
 * rnodemonitor protects RVNODEWAIT state.
 */
#define	vnodewait_wakeup_check(rp) \
{ \
	int s = mutex_bitlock(&(rp)->r_flags, RLOCKED); \
	if ((rp)->r_flags & RVNODEWAIT) { \
		(rp)->r_flags &= ~RVNODEWAIT; \
		sv_broadcast(&vnodewait); \
	} \
	mutex_bitunlock(&(rp)->r_flags, RLOCKED, s); \
}

/*
 * Allocate an rnode for the remote fs described by mi.
 */
static struct rnode *
rnew(struct mntinfo *mi)
{
	struct rnode *rp, *rq;

	/*
	 * Allocate memory for the rnode and account it in mi.
	 */
	rp = kmem_zone_zalloc(rnode_zone, KM_SLEEP);
	milock_mp(mi);
	mi->mi_refct++;
	if (rq = mi->mi_rnodes)
		rq->r_mprevp = &rp->r_mnext;
	rp->r_mnext = rq;
	rp->r_mprevp = &mi->mi_rnodes;
	rp->r_commit.c_flushpercent = COMMIT_FLUSH_PERCENTAGE_MAX;
	mi->mi_rnodes = rp;
	miunlock_mp(mi);

	/*
	 * Initialize the new rnode's locks.
	 */
	INITCHAIN(rp);
	sv_init(&rp->r_iowait, SV_DEFAULT, "iowait");
	mutex_init(&rp->r_rwlock, MUTEX_DEFAULT, "rwlock");
	mutex_init(&rp->r_statelock, MUTEX_DEFAULT, "rnode state mutex");
	mutex_init(&rp->r_commit.c_mutex, MUTEX_DEFAULT, "commit otwmutex");
	mrlock_init(&rp->r_otw_attr_lock, MRLOCK_BARRIER, "r_otw", -1);
	return rp;
}

/*
 * Deallocate node rp from the remote fs described by mi.
 */
void
rdestroy(struct rnode *rp, struct mntinfo *mi)
{
	struct rnode *rq;

	mutex_lock(&rnodemonitor, PINOD);
	METER(rnodemeter.destroys++);
	REMOVEFROMCHAIN(rp);
	vnodewait_wakeup_check(rp);
	mutex_unlock(&rnodemonitor);
	if (mi) {
		milock_mp(mi);
		--mi->mi_refct;
		if (rq = rp->r_mnext)
			rq->r_mprevp = rp->r_mprevp;
		*rp->r_mprevp = rq;
		mi->mi_rdestroys++;
		miunlock_mp(mi);
	}
	/*
	 * Now that the rnode is no longer on the rnode list for the mount point,
	 * it cannot be found by nfs_sync.  However, nfs_sync may have already
	 * found the rnode and acquired r_rwlock.  Call nfs_rwlock to check this,
	 * waiting for r_rwlock to be released if necessary.  Immediately release
	 * r_rwlock after getting it.
	 */
	nfs_rwlock(rtobhv(rp), VRWLOCK_WRITE);
	nfs_rwunlock(rtobhv(rp), VRWLOCK_WRITE);

	/* r_bds socket should have been closed by now */
	ASSERT(rp->r_bds == NULL);
	if (rp->r_bds)
		bds_close(rp, NULL);

	if (rp->r_cred)
		crfree(rp->r_cred);
	sv_destroy(&rp->r_iowait);
	mutex_destroy(&rp->r_rwlock);
	mutex_destroy(&rp->r_statelock);
	mutex_destroy(&rp->r_commit.c_mutex);
	mrfree(&rp->r_otw_attr_lock);
	kmem_zone_free(rnode_zone, rp);
}

/*
 * Flush all cached rnodes from mi and return true if there are no active
 * rnodes other than root, false otherwise.
 */
int
rflush(struct mntinfo *mi)
{
	struct vnode *vp;
	struct rnode *rp;
	vmap_t vmap;
	int busy = 0;

again:
	milock_mp(mi);
	for (rp = mi->mi_rnodes; rp != 0; rp = rp->r_mnext) {
		if ((vp = rtov(rp)) == NULL) {
			/* racing with someone creating a new rnode */
			busy = 1;
			break;
		}
		if (vp->v_count != 0) {
			if (vp == mi->mi_rootvp)
				continue;
			busy = 1;
			break;
		}
		VMAP(vp, vmap);
		miunlock_mp(mi);
		vn_purge(vp, &vmap);
		goto again;
	}
	miunlock_mp(mi);
	return !busy;
}

int
makenfsnode(struct vfs *vfsp, struct mntinfo *mi, fhandle_t *fh, 
		struct nfsfattr *attr,  bhv_desc_t **bdpp) 
{
	int	error;
	int	newnode;

	mutex_lock(&rnodemonitor, PINOD);
	error = make_rnode(fh, vfsp, &nfs_vnodeops, &newnode,
			   mi, bdpp, attr);
	mutex_unlock(&rnodemonitor);
	return error;
}

struct rnode *
rfind(struct vfs *vfsp, fhandle_t *fh)
{
	struct rnode **rh, *rp;
	struct vnode *vp;
	int s;

	rh = RHASH(fh);
retry:
	for (rp = *rh; rp; rp = rp->r_next) {
		if (!bcmp(rtofh(rp), fh, sizeof *fh)) {
			/*
			 * If rp has a null vnode pointer, there must be
			 * another process executing in makenfsnode, below.
			 * This competing process has inserted rp onto rh and
			 * released the rnode monitor.  We must sleep until
			 * the other process finishes allocating a vnode for
			 * rp.
			 */
			vp = rtov(rp);
			if (vp && (vp->v_vfsp == vfsp)) {
				METER(rnodemeter.hits++);
				break;
			} else if (vp == 0) {
				s = mutex_bitlock(&rp->r_flags, RLOCKED);
				rp->r_flags |= RVNODEWAIT;
				mutex_unlock(&rnodemonitor);
				sv_bitlock_wait(&vnodewait, PINOD,
						&rp->r_flags, RLOCKED, s);
				mutex_lock(&rnodemonitor, PINOD);
				goto retry;
			}
		}
		METER(rnodemeter.steps++);
	}
	return(rp);
}

/*
 * Look for a cached rnode with key vfsp,fh.  Create a new rnode if we
 * miss the cache.  Update the rnode's attributes from attr, and return
 * the corresponding node via vpp.
 */
int
make_rnode(fhandle_t *fh, struct vfs *vfsp, struct vnodeops *vops,
	   int *newnode, struct mntinfo *mi, bhv_desc_t **bdpp,
	   struct nfsfattr *attr)
{
	struct rnode **rh, *rp;
	struct vnode *vp;
	vmap_t vmap;
	int	status;

	/*
	 * Search fh's hash chain for an rnode.  Use a monitor for all hash
	 * chains (could optimize with individual hash chain locks) to avoid
	 * making duplicate rnode cache entries.
	 */

retry:
	if (rp = rfind(vfsp, fh)) {
		*newnode = 0;
		vp = rtov(rp);
		VMAP(vp, vmap);

		/*
		 * Leave the monitor before trying to get rp's
		 * vnode, to avoid deadlocking with rdestroy.
		 */
		mutex_unlock(&rnodemonitor);
		if (!(vp = vn_get(vp, &vmap, 0))) {
			if (vmap.v_id == -1) {
				*bdpp = 0;
				return ENOENT;
			}
			METER(rnodemeter.losthits++);
			mutex_lock(&rnodemonitor, PINOD);
			goto retry;
		}
		mutex_lock(&rnodemonitor, PINOD);
#ifdef DEBUG
		{
			bhv_desc_t *bdp;

			/*
			 * Look for both nfs and nfs3 since we don't
			 * really know who's calling us.
			 */
			bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp),
						     &nfs_vnodeops);
			if (bdp == NULL) {
				bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp),
							     &nfs3_vnodeops);
			}
			ASSERT(bdp != NULL);
			ASSERT(bhvtor(bdp) == rp);
		}
#endif
	} else {
		rh = RHASH(fh);
		rp = rnew(mi);
		if (rp == 0) {
			mutex_unlock(&rnodemonitor);
			*bdpp = 0;
			return ENFILE;
		}
		METER(rnodemeter.misses++);
		*newnode = 1;
		rp->r_fh = *fh;
		ADDTOCHAIN(rp, rh);

		/*
		 * Leave the monitor before trying to allocate a vnode
		 * to avoid deadlocking with rdestroy.
		 */
		mutex_unlock(&rnodemonitor);
		vp = vn_alloc(vfsp,
			      attr ? n2v_type(attr) : VNON,
			      attr ? n2v_rdev(attr) : 0);
		mutex_lock(&rnodemonitor, PINOD);
		bhv_desc_init(rtobhv(rp), rp, vp, vops);
		vn_bhv_insert_initial(VN_BHV_HEAD(vp), rtobhv(rp));
		rp->r_mi = mi;
		vnodewait_wakeup_check(rp);

		/*
		 * Update attributes only if caller passed us some, taking
		 * care to lock and unlock the rnode.  If imon is enabled,
		 * localize remote identifier attributes and call imon's hook.
		 */
		if (attr) {
			status = rlock_nohang(rp);
			if (status) {
				return status;
			}
			status = nfs_attrcache(rtobhv(rp), attr, NOFLUSH, NULL);
			if (status) {
			    runlock(rp);
			    return (EAGAIN);
			}
			runlock(rp);
			if (imon_enabled) {
				struct vattr va;
				nattr_to_vattr(rp, attr, &va);
				(*imon_hook)(vp, va.va_fsid, va.va_nodeid);
			}
		}
	}
	*bdpp = rtobhv(rp);
	return 0;
}

int
raccess(rnode_t *rp)
{
	int error = 0;
	int tryagain;
	mntinfo_t *mi = rtomi(rp);
	timespec_t ts;

	if (!nohang()) {
		mraccessf(&rp->r_otw_attr_lock, PLTWAIT);
	} else {
#ifdef DEBUG_NOHANG
		if (debug_rlock()) {
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
			if (!mrtryaccess(&rp->r_otw_attr_lock)) {
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
			}
		} while(tryagain);
	}
	return(error);
}

int
rlock_nohang(rnode_t *rp)
{
	int error = 0;
	int tryagain;
	mntinfo_t *mi = rtomi(rp);
	timespec_t ts;

	if (!nohang()) {
		mrupdatef(&rp->r_otw_attr_lock, PLTWAIT);
	} else {
#ifdef DEBUG_NOHANG
		if (debug_rlock()) {
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
			if (!mrtryupdate(&rp->r_otw_attr_lock)) {
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
			}
		} while(tryagain);
	}
	return(error);
}

void
runlock(rnode_t *rp)
{
	mntinfo_t *mi = rtomi(rp);

	mutex_enter(&mi->mi_lock);
	mrunlock(&rp->r_otw_attr_lock);
	sv_broadcast(&mi->mi_nohang_wait);
	mutex_exit(&mi->mi_lock);
}
