/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

#ident	"$Revision: 1.40 $"

#define _KERNEL 1

#include <limits.h>
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/flock.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode_private.h>
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <sys/dnlc.h>
#include <sys/sysmacros.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/sat.h>
#include <sys/imon.h>
#include <sys/cmn_err.h>
#include <sys/atomic_ops.h>
/*#include <sys/buf.h>*/

#include "sim.h"

#define	VFREELIST(count)	&vfreelist[count].vf_freelist

#define LOCK_VFREELIST(list)	mutex_spinlock(&vfreelist[list].vf_lock)
#define UNLOCK_VFREELIST(l,s)	mutex_spinunlock(&vfreelist[l].vf_lock, s)

#define LOCK_VFP(listp)	       	mutex_spinlock(&(listp)->vf_lock)
#define UNLOCK_VFP(listp,s)	mutex_spinunlock(&(listp)->vf_lock, s)
#define	NESTED_LOCK_VFP(listp)	nested_spinlock(&(listp)->vf_lock)
#define	NESTED_UNLOCK_VFP(listp) nested_spinunlock(&(listp)->vf_lock)

static struct zone *vn_zone;	/* vnode heap zone */
uint64_t vn_generation;		/* vnode generation number */
u_long	vn_vnumber;		/* # of vnodes ever allocated */
int	vn_epoch;		/* # of vnodes freed */
				/* vn_vnumber - vn_epoch == # current vnodes */
int vn_minvn;			/* minimum # vnodes before reclaiming */
int vn_nfree;			/* # of free vnodes */
static int vn_shaken;		/* damper for vn_alloc */
static unsigned int vn_coin;	/* coin for vn_alloc */
int vnode_free_ratio = 1;	/* tuneable parameter for vn_alloc */

/*
 * The following counters are used to manage the pool of allocated and
 * free vnodes.  vnode_free_ratio is the target ratio of free vnodes
 * to in-use vnodes.
 *
 * Whenever a vnode is needed and the number of free vnodes is above
 * (vn_vnumber - vn_epoch) * vnode_free_ratio, an attempt is made t
 * reclaim a vnode from a vnod freelist.  Otherwise, or if a short search
 * of a freelist doesn't produce a reclaimable vnode, a vnode is
 * constructed from the heap.
 *
 * It is up to vn_shake to and deconstruct free vnodes.
 */

vfreelist_t	*vfreelist;	/* pointer to array of freelist structs */
static int	vfreelistmask;	/* number of free-lists - 1 */

static void vn_relink(vnlist_t *, vnode_t *, vnode_t *);

typedef struct vhash {
	struct vnode	*vh_vnode;
	lock_t		 vh_lock;
} vhash_t;

vhash_t	*vhash;			/* hash buckets for active vnodes */

#define VHASHMASK 127
#define VHASH(vnumber)		(&vhash[(vnumber) & VHASHMASK])

/*
 * Dedicated vnode inactive/reclaim sync semaphores.
 * Prime number of hash buckets since address is used as the key.
 */
#define NVSYNC                  37
#define vptosync(v)             (&vsync[((unsigned long)v) % NVSYNC])
sv_t vsync[NVSYNC];

/*
 * Convert stat(2) formats to vnode types and vice versa.  (Knows about
 * numerical order of S_IFMT and vnode types.)
 */
enum vtype iftovt_tab[] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VNON
};

u_short vttoif_tab[] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFIFO, 0, S_IFSOCK
};

/*
 * Vnode operations for a free or killed vnode.
 */
struct vnodeops dead_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_INVALID),
	(vop_open_t)fs_nosys,
	(vop_close_t)fs_noerr,
	(vop_read_t)fs_nosys,
	(vop_write_t)fs_nosys,
	(vop_ioctl_t)fs_nosys,
	(vop_setfl_t)fs_nosys,
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
	(vop_inactive_t)fs_noerr,
	(vop_fid_t)fs_nosys,
	(vop_fid2_t)fs_nosys,
	(vop_rwlock_t)fs_noval,
	(vop_rwunlock_t)fs_noval,
	(vop_seek_t)fs_nosys,
	(vop_cmp_t)fs_nosys,
	(vop_frlock_t)fs_nosys,
	(vop_realvp_t)fs_nosys,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_noval,
	(vop_map_t)fs_nosys,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	(vop_poll_t)fs_nosys,
	(vop_dump_t)fs_nosys,
	(vop_pathconf_t)fs_nosys,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	(vop_attr_get_t)fs_nosys,
	(vop_attr_set_t)fs_nosys,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	(vop_cover_t)fs_nosys,
	(vop_link_removed_t)fs_noval,
	(vop_vnode_change_t)fs_nosys,
};

void
vn_init(void)
{
	register vfreelist_t *vfp;
	register sv_t *svp;
	register int i;
	extern int ncsize;

	/*
	 * There are ``vfreelistmask + 1'' freelists -- so constructed
	 * so multiple clients can allocate vnodes simultaneously. and
	 * to keep the individual lists reasonable short.
	 */
	i = 1;
	vfreelistmask = i - 1;

	vfp = vfreelist = (vfreelist_t *)
			kmem_zalloc(i * sizeof(vfreelist_t), KM_SLEEP);

	for (i = 0; i <= vfreelistmask; i++) {
		vn_initlist(&vfp->vf_freelist);
		vfp->vf_next = vfp + 1;
		vfp->vf_listid = i;
		spinlock_init(&vfp->vf_lock, 0);
		vfp++;
	}
	vfreelist[vfreelistmask].vf_next = vfreelist;

        for (svp = vsync, i = 0; i < NVSYNC; i++, svp++)
                init_sv(svp, SV_DEFAULT, "vsy", i);

	vn_zone = kmem_zone_init(sizeof(vnode_t), "Vnodes");

	vhash = (vhash_t *)kmem_zalloc((VHASHMASK+1) * sizeof(vhash_t),
					KM_SLEEP);
	for (i = 0; i <= VHASHMASK; i++) {
		init_spinlock(&vhash[i].vh_lock, "vhash", i);
	}
	vn_minvn = ncsize;
}

vnode_t *
vn_find(vnumber_t number)
{
	register vhash_t *vhp = VHASH(number);
	register int s = mutex_spinlock(&vhp->vh_lock);
	register vnode_t *vp;

	for (vp = vhp->vh_vnode; vp; vp = vp->v_hashn) {
		if (vp->v_number == number)
			break;
	}
	mutex_spinunlock(&vhp->vh_lock, s);

	return(vp);
}

void
vn_hash(register vnode_t *vp)
{
	register vhash_t *vhp = VHASH(vp->v_number);
	register int s = mutex_spinlock(&vhp->vh_lock);
	register vnode_t **vpp = &vhp->vh_vnode;

	vp->v_hashp = (vnode_t *)NULL;
	vp->v_hashn = *vpp;
	if (vp->v_hashn)
		vp->v_hashn->v_hashp = vp;
	*vpp = vp;
	mutex_spinunlock(&vhp->vh_lock, s);
}

void
vn_unhash(register vnode_t *vp)
{
	register vhash_t *vhp = VHASH(vp->v_number);
	register int s = mutex_spinlock(&vhp->vh_lock);
	register vnode_t *vnext = vp->v_hashn;
	register vnode_t *vprev = vp->v_hashp;

	if (vprev)
		vprev->v_hashn = vnext;
	else
		vhp->vh_vnode = vnext;

	if (vnext)
		vnext->v_hashp = vprev;

	mutex_spinunlock(&vhp->vh_lock, s);
	vp->v_hashp = vp->v_hashn = (vnode_t *)NULL;
	vp->v_number = 0;
}

/*
 * Clean a vnode of filesystem-specific data and prepare it for reuse.
 */
static int
vn_reclaim(struct vnode *vp, int flag)
{
	int error, s;

	VOPINFO.vn_reclaim++;

	/*
	 * Only make the VOP_RECLAIM call if there are behaviors
	 * to call.
	 */
	if (vp->v_fbhv != NULL) {
		VOP_RECLAIM(vp, flag, error);
		if (error)
			return error;
	}
	ASSERT(vp->v_fbhv == NULL);

	/*
	 * File system erred somewhere along the line, and there
	 * are still pages associated with the object.
	 * Remove the debris and print a warning.
	 * XXX LONG_MAX won't work for 64-bit offsets!
	 */
	if (vp->v_pgcnt || vp->v_dpages || vp->v_buf) {
		int i;

		if (vp->v_vfsp)
			i = vp->v_vfsp->vfs_fstype;
		else
			i = 0;

		cmn_err(CE_WARN,
			"vn_reclaim: vnode 0x%x fstype %d (%s) has unreclaimed data (pgcnt %d, dbuf %d dpages 0x%x), flag:%x",
			vp, i, vfssw[i].vsw_name ? vfssw[i].vsw_name : "?",
			vp->v_pgcnt, vp->v_dbuf, vp->v_dpages, vp->v_flag);
		VOP_FLUSHINVAL_PAGES(vp, 0, LONG_MAX, FI_NONE);
	}

	ASSERT(vp->v_dpages == NULL && vp->v_dbuf == 0 && vp->v_pgcnt == 0);
	/*
	 * The v_pgcnt assertion will catch debug systems that screw up.
	 * Patch up v_pgcnt for non-debug systems -- v_pgcnt probably
	 * means accounting problem here, not hashed data.
	 */
	vp->v_pgcnt = 0;

	s = VN_LOCK(vp);
	if (vp->v_number) {
		vn_unhash(vp);
	}
	ASSERT(vp->v_hashp == (vnode_t *)NULL);
	ASSERT(vp->v_hashn == (vnode_t *)NULL);

	vp->v_flag &= (VRECLM|VWAIT|VSHARE|VLOCK);
	VN_UNLOCK(vp, s);
	vp->v_stream = NULL;
#ifdef PGCACHEDEBUG
	{
	extern void panyvp(vnode_t *);
	extern void pfindany(vnode_t *, pgno_t);
	pfindany(vp, 0xffffff);
	panyvp(vp);
	}
#endif
	vp->v_type = VNON;
	vp->v_fbhv = NULL;

	ASSERT(vp->v_mreg == (struct pregion *)vp);
	ASSERT(vp->v_intpcount == 0);
	return 0;
}

static void
vn_wakeup(struct vnode *vp)
{
	int s = VN_LOCK(vp);
	if (vp->v_flag & VWAIT) {
		sv_broadcast(vptosync(vp));
	}
	vp->v_flag &= ~(VRECLM|VWAIT);
	VN_UNLOCK(vp, s);
}

/*
 * Allocate a vnode struct for filesystem usage.
 * Reclaim the oldest on the global freelist if there are any,
 * otherwise allocate another.
 */
struct vnode *
vn_alloc(struct vfs *vfsp, enum vtype type, dev_t dev)
{
	register struct vnode *vp;
	register int list;
	register vnlist_t *vlist;
	register int cnt, s, i;
	register u_long vnumber;
	long target;
	int alloced = 0;
	int error;
#define VN_ALLOC_TRIES 1

	VOPINFO.vn_alloc++;

	if (vfreelistmask) {
		ASSERT(vfreelistmask == 0);
	} else {
		LOCK_VFP(vfreelist);
		list = 0;
	}

	vlist = VFREELIST(list);
	vp = vlist->vl_next;

	/*
	 * Easy cases: if list is empty, allocate a new vnode from the
	 * heap; if first vnode on the list is empty, use it.
	 */
	if (vp == (struct vnode *)vlist)
		goto alloc;

	if (vp->v_fbhv == NULL) {
		ASSERT(!vp->v_dbuf && !vp->v_dpages && !vp->v_pgcnt);
		cnt = VN_ALLOC_TRIES;
		goto get;
	}

	/*
	 * Allocate a minumum of vn_minvn vnodes.
	 * XXX  Do this from vn_init?
	 */
	vnumber = vn_vnumber - vn_epoch;	/* # of extant vnodes */
	if (vnumber < vn_minvn)
		goto alloc;

	ASSERT(vnode_free_ratio > 0);

	cnt = vn_nfree;
	vnumber -= cnt;				/* # of vnodes in use */

	/*
	 * If number of free vnode < number in-use, just alloc a new vnode.
	 */
	if (cnt < vnumber)
		goto alloc;

	/*
	 * Calculate target # of free vnodes to have allocated.
	 * It is # of in-use vnodes * vnode_free_ratio.
	 * vnode_free_ratio is always a small number, so register
	 * additions should be faster than a multiply.
	 */
	target = (long)vnumber;
	for (i = vnode_free_ratio; --i > 0; target += vnumber)
		;

	/*
	 * If number of free vnode < half of target, alloc a new vnode.
	 */
	if (cnt < target/2)
		goto alloc;

	/*
	 * If below target # of free vnodes, devise the chance that
	 * we'll manufacture a new vnode from the heap.  The closer
	 * we are to target, the more likely we'll just allocate from
	 * the freelist -- don't want to manufacture vnodes willy-nilly
	 * just to have vhand/vn_shake decommission them.
	 */
	if (cnt < target) {
		vnumber = target / 16;
		i = 0xf;
		if (vn_shaken > 0) {
			vn_shaken--;
			vnumber <<= 1;
		}

		while (cnt < target - vnumber) {
			i >>= 1;
			vnumber <<= 1;
		}

		if (!(++vn_coin & i))
			goto alloc;
	}

	/*
	 * If a reclaimable vnode isn't found after searching a very
	 * few vnodes, put those vnodes on the tail of the free list
	 * and allocate a vnode from the heap.  This shouldn't happen
	 * often, and vn_shake will trim down the number of vnodes if
	 * the count rises too high.
	 */
	cnt = VN_ALLOC_TRIES;
again:
	for ( ; vp != (struct vnode *)vlist ; vp = vp->v_next) {
		ASSERT(vp->v_listid == list);

		if (vp->v_dbuf || vp->v_dpages || vp->v_pgcnt > 8) {
			VOPINFO.vn_afreeloops++;
			if (--cnt < 0) {
				if (vlist->vl_next == vp) {
					vn_unlink(vp);
					vn_append(vp, vlist);
				} else if (vp->v_next !=
					   (struct vnode *)vlist) {
					vn_relink(vlist, vp, vp->v_prev);
				}
				VOPINFO.vn_afreemiss++;
				break;
			}
			continue;
		}
	get:
		VOPINFO.vn_afreeloops++;

		NESTED_VN_LOCK(vp);
		ASSERT(vp->v_count == 0);
		ASSERT((vp->v_flag & VINACT) == 0);
		if ((vp->v_flag & VRECLM) == 0) {

			vp->v_flag |= VRECLM;
			NESTED_VN_UNLOCK(vp);

			if (vlist->vl_next != vp && vlist->vl_prev != vp) {
				vn_relink(vlist, vp->v_next, vp->v_prev);
				vp->v_next = vp->v_prev = vp;
			} else
				vn_unlink(vp);
			ASSERT(vlist->vl_next->v_prev == (struct vnode *)vlist);
			ASSERT(vlist->vl_prev->v_next == (struct vnode *)vlist);
			ASSERT(vp->v_listid == list);
			ASSERT(vfreelist[list].vf_lsize > 0);
			vfreelist[list].vf_lsize--;
			UNLOCK_VFREELIST(list, s);

			error = vn_reclaim(vp, 0);

			if (error) {
				/*
				 * Freelist lock must be held before cvsema'ing
				 * vnode.  A vn_get could happen on this vnode:
				 * just after this process releases vp, it gets
				 * an interrupt; the vn_get process acquires
				 * freelist lock and dequeues it from nowhere;
				 * then this process puts it back on free list.
				 */
				s = LOCK_VFREELIST(list);
				vn_wakeup(vp);

				ASSERT(vp->v_listid == list);
				ASSERT(vfreelist[list].vf_lsize >= 0);
				vfreelist[list].vf_lsize++;
				vn_append(vp, vlist);
				vp = vlist->vl_next;
				if (--cnt < 0) {
					break;
				}
				goto again;
			} else {
				vn_wakeup(vp);
				VOPINFO.vn_afree++;
				atomicAddInt(&vn_nfree, -1);

				ASSERT(!(vp->v_number));
				goto gotit;
			}
		}
		NESTED_VN_UNLOCK(vp);
	}
alloc:
	UNLOCK_VFREELIST(list, s);

	VOPINFO.vn_aheap++;

	vp = kmem_zone_zalloc(vn_zone, KM_SLEEP);
	alloced = 1;

	vp->v_flag = VSHARE;
	(void) atomicAddLong((long *)&vn_vnumber, 1);

	/* We never free the vnodes in the simulator, so these don't
	   get destroyed either */
	init_mutex(&vp->v_filocksem, MUTEX_DEFAULT, "vfl", (long)vp);
	vp->v_mreg = vp->v_mregb = (struct pregion *)vp;

gotit:
	vp->v_number = atomicAddUint64(&vn_generation, 1);

	ASSERT(vp->v_number);
	vn_hash(vp);

	ASSERT(vp->v_count == 0);
	ASSERT(vp->v_dpages == NULL && vp->v_dbuf == 0 && vp->v_pgcnt == 0);
	ASSERT(vp->v_flag & VSHARE);
	ASSERT(vp->v_filocks == NULL);
	ASSERT(vp->v_intpcount == 0);

	/* Initialize the first behavior and the behavior chain head. */
	if (!alloced) {
		ASSERT(VN_BHV_NOT_READ_LOCKED(VN_BHV_HEAD(vp)) &&
		       VN_BHV_NOT_WRITE_LOCKED(VN_BHV_HEAD(vp)));
		vn_bhv_head_reinit(VN_BHV_HEAD(vp));
	} else
		vn_bhv_head_init(VN_BHV_HEAD(vp), "vnode");

	vp->v_count = 1;
	vp->v_vfsp = vfsp;
	vp->v_type = type;
	vp->v_rdev = dev;
	vp->v_next = vp->v_prev = vp;

	return vp;
}

/*
 * Free an isolated vnode, putting it at the front of a vfreelist.
 * The vnode must not have any other references.
 */
void
vn_free(struct vnode *vp)
{
	register vfreelist_t *vfp;
	register int s;

	ASSERT(vp->v_count == 1);
	if (vp->v_intpcount)
		printf("vn_free: v_intpcount = %d\n", vp->v_intpcount);
	ASSERT(vp->v_intpcount == 0);

	if (vfreelistmask) {
		ASSERT(vfreelistmask == 0);
	} else {
		vp->v_listid = 0;
		vfp = vfreelist;
	}

	vp->v_count = 0;
	vp->v_fbhv = NULL;

	s = LOCK_VFP(vfp);
	ASSERT(vp->v_listid == vfp->vf_listid);
	vfp->vf_lsize++;
	vn_insert(vp, &vfp->vf_freelist);
	UNLOCK_VFP(vfp, s);
	atomicAddInt(&vn_nfree, 1);

	VOPINFO.vn_rele++;
}

/*
 * Get and reference a vnode, possibly removing it from the freelist.
 * If v_count is zero and VINACT is set, then vn_rele is inactivating
 * and we must wait for vp to go on the freelist, or to be reclaimed.
 * If v_count is zero and VRECLM is set, vn_alloc is reclaiming vp;
 * we must sleep till vp is reclaimed, then return false to our caller,
 * who will try again to hash vp's identifier in its filesystem cache.
 * If during the sleep on vfreelock we miss a reclaim, we will notice
 * that v_number has changed.
 */
vnode_t *
vn_get(register struct vnode *vp, register vmap_t *vmap, uint flags)
{
	register int list = vmap->v_id;
	register vfreelist_t *vfp;
	register int s;

	VOPINFO.vn_get++;

	/*
	 * Check that the epoch of vnodes hasn't changed.  Epoch only
	 * changes when a vnode is deallocated, which means that sampled
	 * vnode pointers in filesystem caches may now be stale.  If the
	 * epoch has changed, search for the vnode in the vnode hash.
	 */
again:
	if (list < 0 || list > vfreelistmask)
		return 0;

	vfp = &vfreelist[list];
	s = LOCK_VFP(vfp);

	if (vmap->v_epoch != vn_epoch) {
		vp = vn_find(vmap->v_number);
		if (vp == NULL) {
			UNLOCK_VFP(vfp, s);
			VOPINFO.vn_gchg++;
			vmap->v_id = 0;
			return 0;
		}
	}

	NESTED_VN_LOCK(vp);
	if (vp->v_number != vmap->v_number) {
		NESTED_UNLOCK_VFP(vfp);
		vmap->v_id = 0;
		VN_UNLOCK(vp, s);
		VOPINFO.vn_gchg++;
		return 0;
	}

	if (vp->v_count == 0) {
		/*
		 * Combine the inactive and reclaim race detection code.
		 */
		if (vp->v_flag & (VINACT|VRECLM|VGONE)) {
			ASSERT((vp->v_flag & VGONE) == 0);
			/*
			 * If the caller cannot get stuck waiting
			 * for the vnode to complete its inactive
			 * or reclaim routine, then return NULL.
			 * Set v_id to -2 to indicate that this is
			 * why NULL was returned.
			 */
			if (flags & VN_GET_NOWAIT) {
				vmap->v_id = -2;
				NESTED_VN_UNLOCK(vp);
				UNLOCK_VFP(vfp, s);
				return 0;
			}
			NESTED_UNLOCK_VFP(vfp);
			vp->v_flag |= VWAIT;
			sv_bitlock_wait(vptosync(vp), PINOD, &vp->v_flag, VLOCK, s);
			VOPINFO.vn_gchg++;
			goto again;
		}

		/*
		 * vnode could have travelled from one freelist to
		 * another since it was sampled by caller.
		 */
		if (list != vp->v_listid) {
			list = vp->v_listid;
			NESTED_VN_UNLOCK(vp);
			UNLOCK_VFP(vfp, s);
			VOPINFO.vn_gchg++;
			goto again;
		}

		/*
		 * If there are no behaviors attached to this vnode,
		 * there is no point in giving it back to the caller.
		 * This can happen if the behavior was detached in
		 * the filesystem's inactive routine.
		 */
		if (vp->v_fbhv == NULL) {
			NESTED_VN_UNLOCK(vp);
			UNLOCK_VFP(vfp, s);
			vmap->v_id = 0;
			return NULL;
		}

		/*
		 * Give vp one reference for our caller and unlink it from
		 * the vnode freelist.
		 */
		vp->v_count = 1;
		NESTED_VN_UNLOCK(vp);

		ASSERT(vp->v_next != vp && vp->v_prev != vp);
		ASSERT(vp->v_flag & VSHARE);
		ASSERT(vp->v_filocks == NULL);

		vn_unlink(vp);
		ASSERT(vfp->vf_lsize > 0);
		vfp->vf_lsize--;
		UNLOCK_VFP(vfp, s);

		ASSERT(vn_nfree > 0);
		atomicAddInt(&vn_nfree, -1);
		VOPINFO.vn_gfree++;

		return vp;
	}

	vp->v_count++;
	NESTED_VN_UNLOCK(vp);
	ASSERT(vp->v_flag & VSHARE);
	UNLOCK_VFP(vfp, s);

	return vp;
}

/*
 * purge a vnode from the cache
 * At this point the vnode is guaranteed to have no references (v_count == 0)
 * The caller has to make sure that there are no ways someone could
 * get a handle (via vn_get) on the vnode (usually done via a mount/vfs lock).
 */
void
vn_purge(struct vnode *vp, vmap_t *vmap)
{
	register vfreelist_t *vfp;
	register int list = vmap->v_id;
	register int s;

	/* if you don't SHARE you don't get to play */
	ASSERT(vp->v_flag & VSHARE);
again:
	if (list < 0 || list > vfreelistmask)
		return;

	vfp = &vfreelist[list];
	s = LOCK_VFP(vfp);

	/*
	 * Check that the epoch of vnodes hasn't changed.  Epoch only
	 * changes when a vnode is deallocated, which means that sampled
	 * vnode pointers in filesystem caches may now be stale.  If the
	 * epoch has changed, search for the vnode in the vnode hash.
	 */
	if (vmap->v_epoch != vn_epoch) {
		vp = vn_find(vmap->v_number);
		if (vp == NULL) {
			UNLOCK_VFP(vfp, s);
			VOPINFO.vn_gchg++;
			return;
		}
	}

	/*
	 * Check whether vp has already been reclaimed since our caller
	 * sampled its version while holding a filesystem cache lock that
	 * its VOP_RECLAIM function acquires.
	 */
	NESTED_VN_LOCK(vp);
	if (vp->v_number != vmap->v_number) {
		NESTED_VN_UNLOCK(vp);
		UNLOCK_VFP(vfp, s);
		return;
	}

	/*
	 * If vp is being reclaimed or inactivated, wait until it is inert,
	 * then proceed.  Can't assume that vnode is actually reclaimed
	 * just because the reclaimed flag is asserted -- a vn_alloc
	 * reclaim can fail.
	 */
	if (vp->v_flag & (VINACT | VRECLM)) {
		ASSERT(vp->v_count == 0);
		NESTED_UNLOCK_VFP(vfp);
		vp->v_flag |= VWAIT;
		sv_bitlock_wait(vptosync(vp), PINOD, &vp->v_flag, VLOCK, s);
		goto again;
	}

	/*
	 * Another process could have raced in and gotten this vnode...
	 */
	if (vp->v_count > 0) {
		NESTED_UNLOCK_VFP(vfp);
		VN_UNLOCK(vp, s);
		return;
	}

	/*
	 * vnode could have travelled from one freelist to
	 * another since it was sampled by caller.
	 */
	if (list != vp->v_listid) {
		list = vp->v_listid;
		NESTED_UNLOCK_VFP(vfp);
		VN_UNLOCK(vp, s);
		VOPINFO.vn_gchg++;
		goto again;
	}

	vp->v_flag |= VRECLM;
	NESTED_VN_UNLOCK(vp);

	vn_unlink(vp);
	/*
	 * XXX	There is no routine that relies on a freelist's vf_lsize
	 * XXX	exactly matching the number of free list entries.  Since
	 * XXX	this vnode is going right back on the same freelist, we
	 * XXX	won't bother to decrement, and later, increment vf_lsize.
	ASSERT(vfp->vf_lsize > 0);
	vfp->vf_lsize--;
	 */
	UNLOCK_VFP(vfp, s);

	/*
	 * Call VOP_RECLAIM and clean vp. The FSYNC_INVAL flag tells
	 * vp's filesystem to flush and invalidate all cached resources.
	 * When vn_reclaim returns, vp should have no private data,
	 * either in a system cache or attached to v_data.
	 */
	if (vn_reclaim(vp, FSYNC_INVAL) != 0)
		panic("vn_purge: cannot reclaim");

	/*
	 * Setting v_listid is protected by VRECLM flag above...
	vp->v_listid = list;
	 */
	s = LOCK_VFP(vfp);
	ASSERT(vp->v_listid == list);
	vn_insert(vp, &vfp->vf_freelist);

	/*
	 * XXX	See comments above about vf_lsize.
	ASSERT(vfp->vf_lsize >= 0);
	vfp->vf_lsize++;
	 */
	UNLOCK_VFP(vfp, s);

	/*
	 * Wakeup anyone waiting for vp to be reclaimed.
	 */
	vn_wakeup(vp);
}

/*
 * Add a reference to a referenced vnode.
 */
struct vnode *
vn_hold(struct vnode *vp)
{
	register int s = VN_LOCK(vp);
	ASSERT(!(vp->v_flag & VSHARE) || (vp->v_count > 0));
	ASSERT((vp->v_flag & VSHARE) || (vp->v_count >= 0));
	vp->v_count++;
	VN_UNLOCK(vp, s);
	return vp;
}

/*
 * Release a vnode.  Decrements reference count and calls
 * VOP_INACTIVE on last reference.
 */
void
vn_rele(struct vnode *vp)
{
	register vfreelist_t *vfp;
	register int s;
	int private_vnode;
	/* REFERENCED */
	int cache;

	VOPINFO.vn_rele++;

	s = VN_LOCK(vp);
	ASSERT(vp->v_count > 0);
	if (--vp->v_count == 0) {

		/*
		 * It is absolutely, positively the case that
		 * the lock manager will not be releasing vnodes
		 * without first having released all of its locks.
		 */
		ASSERT(!(vp->v_flag & VLOCKHOLD));
		private_vnode = !(vp->v_flag & VSHARE);
		/*
		 * As soon as we turn this on, noone can find us in vn_get
		 * until we turn off VINACT or VRECLM
		 */
		vp->v_flag |= VINACT;
		VN_UNLOCK(vp, s);

		/*
		 * Do not make the VOP_INACTIVE call if there
		 * are no behaviors attached to the vnode to call.
		 */
		if (vp->v_fbhv != NULL) {
			VOP_INACTIVE(vp, get_current_cred(), cache);
		}
		/*
		 * For filesystems that do not want to be part of
		 * the global vnode cache, we must not touch the
		 * vp after we have called inactive
		 */
		if (private_vnode)
			return;

		ASSERT(vp->v_next == vp && vp->v_prev == vp);

		if (vfreelistmask) {
			ASSERT(vfreelistmask == 0);
		} else {
			vp->v_listid = 0;
			vfp = vfreelist;
		}

		ASSERT(vp->v_listid <= vfreelistmask);
		s = LOCK_VFP(vfp);
		if (vp->v_fbhv == NULL)
			vn_insert(vp, &vfp->vf_freelist);
		else
			vn_append(vp, &vfp->vf_freelist);

		ASSERT(vfp->vf_lsize >= 0);
		vfp->vf_lsize++;

		/*
		 * Must hold freelist lock here to prevent
		 * vnode from being deallocated first.
		 * VRECLM is turned off, too, because it may
		 * have been vn_reclaim'd, above (this stmt.
		 * not true anymore).
		 */
		NESTED_VN_LOCK(vp);
		if (vp->v_flag & VWAIT) {
			sv_broadcast(vptosync(vp));
		}
		vp->v_flag &= ~(VINACT|VWAIT|VRECLM|VGONE);

		NESTED_VN_UNLOCK(vp);
		UNLOCK_VFP(vfp, s);

		ASSERT(vn_nfree >= 0);
		atomicAddInt(&vn_nfree, 1);

		return;
	}
	VN_UNLOCK(vp, s);
}

/*
 * Vnode list primitives.  The callers must exclude one another.
 */
void
vn_initlist(struct vnlist *vl)
{
	vl->vl_next = vl->vl_prev = (struct vnode *)vl;
}

void
vn_insert(struct vnode *vp, struct vnlist *vl)
{
	vp->v_next = vl->vl_next;
	vp->v_prev = (struct vnode *)vl;
	vl->vl_next = vp;

	/*
	 * imon depends on vp this...
	 */
	vp->v_next->v_prev = vp;
}

void
vn_unlink(register struct vnode *vp)
{
	register struct vnode *next = vp->v_next;
	register struct vnode *prev = vp->v_prev;

	next->v_prev = prev;
	prev->v_next = next;
	vp->v_next = vp->v_prev = vp;
}

static void
vn_relink(
	register vnlist_t *vlist,
	register vnode_t *next,
	register vnode_t *prev)
{
	register struct vnode *N = vlist->vl_next;
	register struct vnode *P = vlist->vl_prev;
	next->v_prev = (struct vnode *)vlist;
	P->v_next = N;
	N->v_prev = P;
	vlist->vl_next = next;
	vlist->vl_prev = prev;
	prev->v_next = (struct vnode *)vlist;
}
