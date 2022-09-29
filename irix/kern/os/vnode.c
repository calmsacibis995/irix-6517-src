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

#ident	"$Revision: 1.215 $"

#include <limits.h>
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
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
#include <sys/kthread.h>
#include <sys/uthread.h>
#include <ksys/vproc.h>
#include <sys/vfs.h>
#include <sys/vnode_private.h>
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <sys/dnlc.h>
#include <sys/sysmacros.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/sat.h>
#include <ksys/vpag.h>
#include <sys/imon.h>
#include <sys/cmn_err.h>
#include <sys/atomic_ops.h>
#include <sys/psema_cntl.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
#ifdef VNODE_TRACING
#include <sys/ktrace.h>
#endif

/*
 * Managing the pool of allocated and free vnodes:
 *
 * Whenever a vnode is needed and the number of free vnodes is above
 * (vn_vnumber - vn_epoch), and ncsize, an attempt is made to
 * reclaim a vnode from a vnode freelist.  Otherwise, or if a short search
 * of a freelist doesn't produce a reclaimable vnode, a vnode is
 * constructed from the heap.
 *
 * It is up to vn_shake to deconstruct free vnodes.
 */

/*
 * Internal data structures.
 */
/*
 * Vnode hash list bucket.
 */
typedef struct vhash_s {
	struct vnode   *vh_vnode;
	lock_t		vh_lock;
} vhash_t;

/* 
 * Macros and defines.
 */
#define	VFREELIST(count)	&vfreelist[count].vf_freelist
#define LOCK_VFREELIST(list)	mutex_spinlock(&vfreelist[list].vf_lock)
#define UNLOCK_VFREELIST(l,s)	mutex_spinunlock(&vfreelist[l].vf_lock, s)

#define LOCK_VFP(listp)		mutex_spinlock(&(listp)->vf_lock)
#define UNLOCK_VFP(listp,s)	mutex_spinunlock(&(listp)->vf_lock, s)
#define NESTED_LOCK_VFP(listp)	nested_spinlock(&(listp)->vf_lock)
#define NESTED_UNLOCK_VFP(listp) nested_spinunlock(&(listp)->vf_lock)

#define VHASHMASK 		127
#define VHASH(vnumber)		(&vhash[(vnumber) & VHASHMASK])

#define	NVSYNC			37		/* prime */
#define	vptosync(v)		(&vsync[((unsigned long)v) % NVSYNC])

/*
 * Vnode global data.
 */
static hotUint64Counter_t vn_generation; /* vnode generation number */
hotUlongCounter_t vn_vnumber;	/* # of vnodes ever allocated */
hotIntCounter_t	vn_nfree;	/* # of free vnodes */

#if MP
#pragma fill_symbol (vn_generation, 128)
#pragma fill_symbol (vn_vnumber, 128)
#pragma fill_symbol (vn_nfree, 128)
#endif

static zone_t	*vn_zone;	/* vnode heap zone */
int		vn_epoch;	/* # of vnodes freed */
				/* vn_vnumber - vn_epoch == # current vnodes */
static int	vn_minvn;	/* minimum # vnodes before reclaiming */
static int	vn_shaken;	/* damper for vn_alloc */
static uint_t 	vn_coin;	/* coin for vn_alloc */
static vhash_t	*vhash;		/* hash buckets for active vnodes */
vnode_t		*rootdir;	/* pointer to root vnode */
vfreelist_t	*vfreelist;	/* pointer to array of freelist structs */
static int	vfreelistmask;	/* number of free-lists - 1 */

/*
 * Following is global data that can't be cellularized until any given
 * vnode is accessed from only one cell.
 */
static sv_t 	vsync[NVSYNC];	/* vnode inactive/reclaim sync semaphores */
lock_t		mreg_lock;	/* spinlock protecting all vp->v_mreg */
#if MP
#pragma align_symbol (mreg_lock, L2cacheline) 
#pragma fill_symbol (mreg_lock, L2cacheline)
#endif /* MP */


/*
 * Imon data - must be here instead of imon.c so that we don't get
 * gp-relative link errors.
 */
void		(*imon_event)(struct vnode *, struct cred *cr, int);
void		(*imon_hook)(struct vnode *, dev_t, ino_t);
void 		(*imon_broadcast)(dev_t, int);
int             imon_enabled;

/*
 * Externs and static functions.
 */
static void 	vn_relink(vnlist_t *, vnode_t *, vnode_t *);
static int	vn_shake(int);

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
vnodeops_t dead_vnodeops = {
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
	(vop_map_t)fs_nodev,
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
	(vop_ptossvp_t)fs_noval,
	(vop_pflushinvalvp_t)fs_noval,
	(vop_pflushvp_t)fs_noval,
	(vop_pinvalfree_t)fs_noval,
	(vop_sethole_t)fs_noval,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	(vop_strgetmsg_t)fs_nosys,
	(vop_strputmsg_t)fs_nosys,
};

void
vn_init(void)
{
	register vfreelist_t *vfp;
	register sv_t *svp;
	register int i;
	extern int nproc, ncsize;

	/*
	 * There are ``vfreelistmask + 1'' freelists --
	 * so multiple clients can allocate vnodes simultaneously, and
	 * to keep the individual lists reasonably short.
	 */
	i = MIN(numcpus, 16);
	while (i & (i-1))
		i--;
	vfreelistmask = i - 1;

	vfp = vfreelist = (vfreelist_t *)
			kmem_zalloc(i * sizeof(vfreelist_t), KM_SLEEP);

	for (i = 0; i <= vfreelistmask; i++) {
		vn_initlist(&vfp->vf_freelist);
		vfp->vf_next = vfp + 1;
		vfp->vf_listid = i;
		init_spinlock(&vfp->vf_lock, "vf_lock", i);
		vfp++;
	}
	vfreelist[vfreelistmask].vf_next = vfreelist;

	vn_zone = kmem_zone_init(sizeof(vnode_t), "Vnodes");

	spinlock_init(&mreg_lock, "mreg_lock");

	for (svp = vsync, i = 0; i < NVSYNC; i++, svp++)
		init_sv(svp, SV_DEFAULT, "vsy", i);

	repl_init();		/* vnode replication */

	shake_register(SHAKEMGR_MEMORY, vn_shake);

	vhash = (vhash_t *)kmem_zalloc((VHASHMASK+1) * sizeof(vhash_t),
					KM_SLEEP);
	for (i = 0; i <= VHASHMASK; i++) {
		init_spinlock(&vhash[i].vh_lock, "vhash", i);
	}

	vn_minvn = MAX(nproc, ncsize);

	/* Just refer to vn_passthrup to force linker to bring int
	 * vn_passthru ops.
	 */
	if (!vn_passthrup){
		cmn_err_tag(139,CE_PANIC,"vnode pass through mode not initialized ?");
	}
}

/*
 * Find vnode in hash table with the given number.  If the vnode is 
 * found then it must be the same as the one passed in.  
 *
 * NOTE:  must not dereference 'vp' because it's possible the memory that 
 * vp refers to has been reallocated to some other use (because it was
 * freed by the vn_shake mechanism).
 */
vnode_t *
vn_find(vnode_t *vp, vnumber_t number)
{
	register vhash_t 	*vhp = VHASH(number);
	register int 		s = mutex_spinlock(&vhp->vh_lock);
	register vnode_t 	*lvp;

	for (lvp = vhp->vh_vnode; lvp; lvp = lvp->v_hashn) {
	        ASSERT(lvp->v_number != 0);
		if (lvp->v_number == number) {
		        if (lvp != vp) {
				printf("vn_find: vp=0x%x lvp=0x%x\n", vp, lvp);
				panic("vn_find error");
			}
			break;
		}
	}
	mutex_spinunlock(&vhp->vh_lock, s);

	return(lvp);
}

/*
 * Put vnode in hash table.  Must have a non-zero v_number.
 */
void
vn_hash(register vnode_t *vp)
{
	register vhash_t *vhp = VHASH(vp->v_number);
	register int s = mutex_spinlock(&vhp->vh_lock);
	register vnode_t **vpp = &vhp->vh_vnode;

	ASSERT(vp->v_number);
	vp->v_hashp = (vnode_t *)NULL;
	vp->v_hashn = *vpp;
	if (vp->v_hashn)
		vp->v_hashn->v_hashp = vp;
	*vpp = vp;
	mutex_spinunlock(&vhp->vh_lock, s);
}

/*
 * Remove vnode from hash table.  v_number is set to 0 so that vn_find
 * won't find it.
 */
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
#ifdef CKPT
	extern int ckpt_enabled;
#endif

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
	 * are still pages/buffers associated with the object.
	 * Remove the debris and print a warning.
	 * XXX LONG_MAX won't work for 64-bit offsets!
	 */
	if (vp->v_pgcnt || vp->v_dpages || vp->v_buf) {
#ifdef DEBUG
		int i;

		if (vp->v_vfsp)
			i = vp->v_vfsp->vfs_fstype;
		else
			i = 0;

		cmn_err(CE_WARN,
			"vn_reclaim: vnode 0x%x fstype %d (%s) has unreclaimed data (pgcnt %d dbuf %d dpages 0x%x), flag:%x",
			vp, i, vfssw[i].vsw_name ? vfssw[i].vsw_name : "?",
			vp->v_pgcnt, vp->v_dbuf, vp->v_dpages, vp->v_flag);
#endif
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
	vn_trace_entry(vp, "vn_reclaim", (inst_t *)__return_address);

	if (vp->v_number) {
		vn_unhash(vp);
	}
	ASSERT(vp->v_hashp == (vnode_t *)NULL);
	ASSERT(vp->v_hashn == (vnode_t *)NULL);

	/*
	 * Clear all flags except the ones relevant to the fact
	 * that it's being reclaimed.
	 */
	vp->v_flag &= (VRECLM|VWAIT|VSHARE|VLOCK);

	VN_UNLOCK(vp, s);
	vp->v_stream = NULL;
	vp->v_type = VNON;
	vp->v_fbhv = NULL;

	/*
	 * All locks should have been released by now, but
	 * the lock sema data structure needs to be taken care of.
	 */
	ASSERT(vp->v_filocks == NULL);

	/*
	 * Realaim all page cache related data.
	 * This could sleep waiting to synchronize with the
	 * threads trying to recycle hashed pages.. 
	 */
	vnode_pcache_reclaim(vp);
#ifdef CKPT
	/*
	 * Free lookup info...
	 */
	if (ckpt_enabled)
		ckpt_vnode_free(vp);
#endif
	ASSERT(vp->v_mreg == (struct pregion *)vp);
	ASSERT(vp->v_intpcount == 0);
	return 0;
}

static void
vn_wakeup(struct vnode *vp)
{
	int s;

	s = VN_LOCK(vp);
	vn_trace_entry(vp, "vn_wakeup", (inst_t *)__return_address);
	if (vp->v_flag & VWAIT) {
		sv_broadcast_bounded(vptosync(vp));
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
#define VN_ALLOC_TRIES 4

	VOPINFO.vn_alloc++;

	if (vfreelistmask) {
		list = ++private.p_hand & vfreelistmask;
		s = LOCK_VFREELIST(list);
	} else {
		s = LOCK_VFP(vfreelist);
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

#if VNODE_THRASH_TEST
	/*
	 * Turn this ifdef on to force races with 
	 * vn_alloc/vn_reclaim/vn_get/dnlc_lookup_fast, etc.
	 */
	if (fetchIntHot(&vn_nfree) > 0) {
		cnt = VN_ALLOC_TRIES;
		goto get;
	}
#endif

	/*
	 * Allocate a minumum of vn_minvn vnodes.
	 * XXX  Do this from vn_init?
	 */
	vnumber = fetchUlongHot(&vn_vnumber) - vn_epoch;	/* # of extant vnodes */
	if (vnumber < vn_minvn)
		goto alloc;

	cnt = fetchIntHot(&vn_nfree);
	vnumber -= cnt;				/* # of vnodes in use */

	/*
	 * If number of free vnode < number in-use, just alloc a new vnode.
	 */
	if (cnt < vnumber)
		goto alloc;

	/*
	 * Calculate target # of total vnodes to have allocated.
	 */
	target = vnumber;

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
				vn_trace_entry(vp, "REC FAIL1",
					(inst_t *)__return_address);
				s = LOCK_VFREELIST(list);
				vn_wakeup(vp);

				ASSERT(vp->v_listid == list);
				ASSERT(vfreelist[list].vf_lsize >= 0);
				vfreelist[list].vf_lsize++;
				vn_append(vp, vlist);
				vp = vlist->vl_next;
				vn_trace_entry(vp, "REC FAIL2",
					(inst_t *)__return_address);
				if (--cnt < 0) {
					break;
				}
				goto again;
			} else {
				vn_wakeup(vp);
				VOPINFO.vn_afree++;
				atomicAddIntHot(&vn_nfree, -1);

				ASSERT(!(vp->v_number));
#if defined(DEBUG) && defined(VNODE_INIT_BITLOCK)
				destroy_bitlock(&vp->v_flag);
#endif
				vp->v_number =
					atomicAddUint64Hot(&vn_generation, 1);
				goto gotit;
			}
		}
		NESTED_VN_UNLOCK(vp);
	}
alloc:
	UNLOCK_VFREELIST(list, s);

	VOPINFO.vn_aheap++;

	vp = kmem_zone_zalloc(vn_zone, KM_SLEEP);
	vp->v_number = atomicAddUint64Hot(&vn_generation, 1);
	alloced = 1;

#ifdef VNODE_TRACING
	vp->v_trace = ktrace_alloc(VNODE_TRACE_SIZE, 0);
#endif
	vp->v_flag = VSHARE;
	(void) atomicAddUlongHot(&vn_vnumber, 1);

	init_bitlock(&vp->v_pcacheflag, VNODE_PCACHE_LOCKBIT, "v_pcache",
		     (long)vp->v_number);
# if defined(DEBUG) && defined(VNODE_INIT_BITLOCK)
	init_bitlock(&vp->v_flag, VLOCK, "vnode", (long)vp->v_number);
# endif
	init_mutex(&vp->v_filocksem, MUTEX_DEFAULT, "vfl", (long)vp->v_number);
	init_mutex(&vp->v_buf_lock, MUTEX_DEFAULT, "vnbuf", (long)vp->v_number);

	vp->v_mreg = vp->v_mregb = (struct pregion *)vp;
	vnode_pcache_init(vp);
gotit:
	vn_hash(vp);

	ASSERT(vp->v_count == 0);
	ASSERT(vp->v_dpages == NULL && vp->v_dbuf == 0 && vp->v_pgcnt == 0);
	ASSERT(vp->v_filocks == NULL);
	ASSERT(vp->v_intpcount == 0);
	ASSERT(vp->v_flag & VSHARE);

	/* 
	 * VLOCK may or may not be set, because other threads may be
	 * trying to lock this vnode (in vn_get) in order to see if
	 * it's the one they're looking for.  They won't be able to
	 * use the vnode though because the v_number won't match.
	*/
	ASSERT(!(vp->v_flag & (VNOSWAP | VISSWAP |
			       VREPLICABLE |
			   /*  VNONREPLICABLE | XXX uncomment this */
			       VFRLOCKS | VENF_LOCKING |
			       VREMAPPING | VDOCMP | VDUP |
			       VSEMAPHORE | VUSYNC |
			       VINACT | VRECLM | VEVICT | VWAIT |
			       VFLUSH | VLOCKHOLD | VINACTIVE_TEARDOWN |
			       VROOT | VMOUNTING)));

	vnode_pcache_reinit(vp);

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

	vn_trace_hold(vp, __FILE__, __LINE__, (inst_t *)__return_address);
#ifdef CKPT
	ASSERT(vp->v_ckpt == NULL);
#endif
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
	register int mask;
	register int s;
#ifdef CKPT
	extern int ckpt_enabled;
#endif

#ifdef DEBUG
	ASSERT(vp->v_count == 1);
	if (vp->v_intpcount)
	    printf("vn_free: v_intpcount = %d\n", vp->v_intpcount);
	ASSERT(vp->v_intpcount == 0);
#endif

	if (mask = vfreelistmask) {
		register vfreelist_t *tp;

		vfp = &vfreelist[private.p_hand++ & mask];
		tp = vfp->vf_next;
		if (vfp->vf_lsize > tp->vf_lsize)
			vfp = tp;

		vp->v_listid = vfp->vf_listid;
	} else {
		vp->v_listid = 0;
		vfp = vfreelist;
	}

	vn_trace_rele(vp, __FILE__, __LINE__, (inst_t *)__return_address);
	vp->v_count = 0;
	vp->v_fbhv = NULL;

	vnode_pcache_free(vp);
#ifdef CKPT
	if (ckpt_enabled)
		ckpt_vnode_free(vp);
#endif
	s = LOCK_VFP(vfp);
	ASSERT(vp->v_listid == vfp->vf_listid);
	vfp->vf_lsize++;
	vn_insert(vp, &vfp->vf_freelist);
	UNLOCK_VFP(vfp, s);
	atomicAddIntHot(&vn_nfree, 1);


	VOPINFO.vn_rele++;
}

static int
vn_shake_freelist(register int nfree)
{
	register struct vnode *vp;
	register vfreelist_t *vfp = vfreelist;
	register vnlist_t *vlist;
	register int list;
	int s, error;
	int shaken = 0;
#ifdef DEBUG
	static int vn_shake_lock = 0;
#endif

again:
	if (vn_epoch == INT_MAX || nfree <= 0)
		return shaken;

	if (list = vfreelistmask) {
		register int count = vfp->vf_lsize;
		register vfreelist_t *tp = vfp;

		do {
			vfp = vfp->vf_next;
			if (vfp->vf_lsize > count) {
				count = vfp->vf_lsize;
				tp = vfp;
			}
		} while (--list > 0);

		vfp = tp;
		list = tp->vf_listid;
	}

	vlist = &vfp->vf_freelist;
	s = LOCK_VFP(vfp);

	for (vp = vlist->vl_next; vp != (struct vnode *)vlist; vp = vp->v_next)
	{
		ASSERT(vp->v_listid == list);

		if (vp->v_pgcnt)
			continue;

		NESTED_VN_LOCK(vp);
		ASSERT((vp->v_flag & VINACT) == 0);
		if ((vp->v_flag & VRECLM) == 0) {
			vp->v_flag |= VRECLM;
			ASSERT(vp->v_count == 0);
			NESTED_VN_UNLOCK(vp);

			vn_unlink(vp);
			ASSERT(vfp->vf_lsize > 0);
			vfp->vf_lsize--;
			UNLOCK_VFP(vfp, s);

			error = vn_reclaim(vp, 0);

			/*
			 * Purge soft references to the vnode
			 * from the name cache.
			 */
			if (!error) {
				dnlc_remove_vp(vp);
				ASSERT(fetchIntHot(&vn_nfree) > 0);
				atomicAddIntHot(&vn_nfree, -1);
			} else {
				/*
				 * See comments in vn_alloc for explanation
				 * why we lock vfreelist before cvsema in
				 * error case.
				 */
				s = LOCK_VFP(vfp);
				vn_wakeup(vp);

				vp->v_listid = list;
				vn_append(vp, vlist);

				ASSERT(vfp->vf_lsize >= 0);
				vfp->vf_lsize++;
				UNLOCK_VFP(vfp, s);
				/*
				 * Decrement nfree if we fail so we don't
				 * get stuck here all day.
				 */
				if (nfree-- <= 0)
					return shaken;
				goto again;
			}

			s = LOCK_VFP(vfreelist);
			for (list = vfreelistmask, vfp = vfreelist->vf_next;
			     list-- > 0;
			     vfp = vfp->vf_next) {
				NESTED_LOCK_VFP(vfp);
			}
#ifdef DEBUG
			vn_shake_lock = 1;
#endif
			vn_wakeup(vp);

			/*
			 * Define a new epoch in the history of vnodes...
			 * Protected by having *every* freelist lock held.
			 */
			vn_epoch++;
			vn_shaken += 4;	/* dampen vn_alloc's desire to */
					/* allocate vnodes from heap */
			VOPINFO.vn_destroy++;

			for (list = vfreelistmask, vfp = vfreelist->vf_next;
			     list-- > 0;
			     vfp = vfp->vf_next) {
				NESTED_UNLOCK_VFP(vfp);
			}
			UNLOCK_VFP(vfreelist, s);
#ifdef DEBUG
			ASSERT(vn_shake_lock == 1);
			vn_shake_lock = 0;
#endif /* DEBUG */
#ifdef VNODE_TRACING
			ktrace_free(vp->v_trace);
#endif /* VNODE_TRACING */

			/* Teardown behavior chain state. */
			vn_bhv_head_destroy(VN_BHV_HEAD(vp));

			vp->v_flag = 0;	/* debug */
			destroy_bitlock(&vp->v_pcacheflag);
#if defined(DEBUG) && defined(VNODE_INIT_BITLOCK)
			destroy_bitlock(&vp->v_flag);
#endif
			mutex_destroy(&vp->v_filocksem);
			mutex_destroy(&vp->v_buf_lock);
			kmem_zone_free(vn_zone, vp);
			shaken++;

			if (nfree-- <= 0)
				return shaken;

			goto again;
		}
		NESTED_VN_UNLOCK(vp);
	}
	UNLOCK_VFP(vfp, s);

	return shaken;
}

/* ARGSUSED */
int
vn_shake(int level)
{
	int	total_vnodes = fetchUlongHot(&vn_vnumber) - vn_epoch;
	int	free = fetchIntHot(&vn_nfree);
	int	num_to_free;
	int	v;

	/*
	 * If we're below our configured minimum number of
	 * vnodes, then just get out.
	 */
	if (total_vnodes < vn_minvn) {
		return 0;
	}

	v = total_vnodes;
	v -= free;			/* # of in-use vnodes */
	ASSERT(v >= 0);

	if (free <= v)	{		/* don't steal any vnodes */
		return 0;		/* if free count <= inuse count */
	}

	v = free - v;			/* # over target */
	if (v < 0)
		v = 0;
	else
		v = v / 16;		/* take either 1/Nth of # over target */

	free = free / 128;		/* or 1/Mth of free vnodes... */

	num_to_free = MAX(free, v);
	num_to_free = MAX(num_to_free, 512);	/* but no more than 512 */

	/*
	 * Don't pull the number of extant vnodes below the desired
	 * minimum.
	 */
	if ((total_vnodes - num_to_free) < vn_minvn) {
		num_to_free = total_vnodes - vn_minvn;
	}

	return vn_shake_freelist(num_to_free);
}

/*
 * Routine which 1) makes a vnode invisible to vn_get, and 2) wakes up
 * anyone that has already found the vnode via vn_get but is blocked
 * there on the VINACT flag.  
 * 
 * This routine may only be called during inactivation processing.
 * It's used by nfs_inactive to resolve a deadlock condition.
 */
void
vn_gone(struct vnode *vp)
{
	register int s;

	ASSERT(vp->v_count == 0);
	ASSERT(vp->v_flag & VINACT);

	s = VN_LOCK(vp);
	vn_trace_entry(vp, "vn_gone", (inst_t *)__return_address);
	if (vp->v_flag & VWAIT) {
		sv_broadcast_bounded(vptosync(vp));
		vp->v_flag &= ~VWAIT;
	}
	vp->v_flag |= VGONE;
	VN_UNLOCK(vp, s);
}

/*
 * Based on the value of the vnode's reference count, attempt to set
 * a vnode's VEVICT flag.
 *
 * If v_count > 1, then do nothing and return 1.
 * If v_count == 1, then set the vnode's VEVICT flag and return 0.
 *
 * The VEVICT flag causes callers of vn_get to wait until the vnode is
 * inactivated, or the evict condition is otherwise cleared (currently, 
 * there's no interface to clear an evict condition).
 * 
 * vn_evict is used by a caller who wishes to prevent additional references
 * to a vnode iff it holds the only reference.
 */
int
vn_evict(struct vnode *vp)
{
	register int s;

	s = VN_LOCK(vp);
	vn_trace_entry(vp, "vn_evict", (inst_t *)__return_address);
	ASSERT(vp->v_count >= 1);

	if (vp->v_count == 1) {
		vp->v_flag |= VEVICT;
		VN_UNLOCK(vp, s);
		return 0;
	}
	VN_UNLOCK(vp, s);
	return 1;
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
	register int list;
	register vfreelist_t *vfp;
	register int s;

	list = vmap->v_id;
	VOPINFO.vn_get++;

	/*
	 * A note about v_number:  it gets set to a non-zero value at
	 * vn_alloc() time and then reset to zero in vn_reclaim after
	 * the call to VOP_RECLAIM.  A file system calling vn_get will 
	 * have snapshotted the v_number and put it in the vmap structure
	 * while the vnode was in its hash table:  meaning it must have
	 * been done between the time vn_alloc() completed and the time 
	 * VOP_RECLAIM completed.  Hence, vmap->v_number should always be 
	 * non-zero
	 */ 
	if (vmap->v_number == 0) {
#pragma mips_frequency_hint NEVER
	        printf("vn_get error: vp=0x%x vmap->v_number=%d\n",
		       vp, vmap->v_number);
	        panic("vn_get: vmap->v_number == 0");
	}
again:
	/*
	 * NOTE:  must not dereference 'vp' until after verifying that
	 * it still refers to a vnode.
	 */
	if (list < 0 || list > vfreelistmask)
		goto fail;

	vfp = &vfreelist[list];
	s = LOCK_VFP(vfp);

	/*
	 * Check that the epoch of vnodes hasn't changed.  Epoch only
	 * changes when a vnode is deallocated, which means that sampled
	 * vnode pointers in filesystem caches may now be stale.  If the
	 * epoch has changed, search for the vnode in the vnode hash.
	 */
	if (vmap->v_epoch != vn_epoch) {
		/* if vn_find succeeds, it's guaranteed to find vp */
		if (vn_find(vp, vmap->v_number) == NULL) {
#pragma mips_frequency_hint NEVER
			UNLOCK_VFP(vfp, s);
			VOPINFO.vn_gchg++;
			vmap->v_id = 0;
			goto fail;
		}
	}

	/*
	 * Now it's ok to dereference 'vp'.
	 */
	vn_trace_entry(vp, "GET AGAIN", (inst_t *)__return_address);
#ifdef CKPT
	ASSERT(vp->v_ckpt != (ckpt_handle_t)-1L);
#endif
	NESTED_VN_LOCK(vp);
	vn_trace_entry(vp, "GET LOCKED", (inst_t *)__return_address);
	if (vp->v_number != vmap->v_number) {
#pragma mips_frequency_hint NEVER
		NESTED_UNLOCK_VFP(vfp);
		vmap->v_id = 0;
		vn_trace_entry(vp, "GET VERS", (inst_t *)__return_address);
		VN_UNLOCK(vp, s);
		VOPINFO.vn_gchg++;
		goto fail;
	}

	/*
	 * If the vnode is being inactivated, reclaimed, or evicted,
	 * then wait until the condition clears (unless VN_GET_NOWAIT
	 * is specified).  If the vnode has VGONE set, then return
	 * immediately.
	 */
	if (vp->v_flag & (VINACT|VRECLM|VGONE|VEVICT)) {
#pragma mips_frequency_hint NEVER

		ASSERT((vp->v_flag & VEVICT) ? vp->v_count <= 1 : 1);
		ASSERT((vp->v_flag & (VINACT|VRECLM|VGONE)) ? 
		       vp->v_count == 0 : 1);

		if (vp->v_flag & VGONE) {
			vmap->v_id = -1;
			vn_trace_entry(vp, "GET GONE",
				       (inst_t *)__return_address);
			NESTED_VN_UNLOCK(vp);
			UNLOCK_VFP(vfp, s);
			goto fail;
		}
		/*
		 * If the caller cannot get stuck waiting
		 * for the vnode to complete its inactive
		 * or reclaim routine, then return NULL.
		 * Set v_id to -2 to indicate that this is
		 * why NULL was returned.
		 */
		if (flags & VN_GET_NOWAIT) {
			vmap->v_id = -2;
			vn_trace_entry(vp, "GET NOWAIT",
				       (inst_t *)__return_address);
			NESTED_VN_UNLOCK(vp);
			UNLOCK_VFP(vfp, s);
			goto fail;
		}
		NESTED_UNLOCK_VFP(vfp);
		vp->v_flag |= VWAIT;
		vn_trace_entry(vp, "GET RECL",
			(inst_t *)__return_address);
		sv_bitlock_wait(vptosync(vp), PINOD,
				&vp->v_flag, VLOCK, s);
		VOPINFO.vn_gchg++;
		goto again;
	}

	if (vp->v_count == 0) {
		/*
		 * vnode could have travelled from one freelist to
		 * another since it was sampled by caller.
		 */
		if (list != vp->v_listid) {
#pragma mips_frequency_hint NEVER
			list = vp->v_listid;
			vn_trace_entry(vp, "GET SWTCH",
				(inst_t *)__return_address);
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
#pragma mips_frequency_hint NEVER
			vn_trace_entry(vp, "GET NO BHV",
				(inst_t *)__return_address);
			NESTED_VN_UNLOCK(vp);
			UNLOCK_VFP(vfp, s);
			vmap->v_id = 0;
			goto fail;
		}

		/*
		 * Give vp one reference for our caller and unlink it from
		 * the vnode freelist.
		 */
		vp->v_count = 1;
		vn_trace_hold(vp, __FILE__, __LINE__,
			(inst_t *)__return_address);
		NESTED_VN_UNLOCK(vp);

		ASSERT(vp->v_next != vp && vp->v_prev != vp);
		ASSERT(vp->v_flag & VSHARE);
		ASSERT(vp->v_filocks == NULL);

		vn_unlink(vp);
		ASSERT(vfp->vf_lsize > 0);
		vfp->vf_lsize--;
		UNLOCK_VFP(vfp, s);

		ASSERT(fetchIntHot(&vn_nfree) > 0);
		atomicAddIntHot(&vn_nfree, -1);
		VOPINFO.vn_gfree++;

	} else {
		vp->v_count++;
		vn_trace_hold(vp, __FILE__, __LINE__, 
			      (inst_t *)__return_address);
		NESTED_VN_UNLOCK(vp);
		UNLOCK_VFP(vfp, s);
	}

#ifdef CKPT
	ASSERT(vp->v_ckpt != (ckpt_handle_t)-1L);
#endif
	return vp;
 fail:
	return NULL;
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
	register int list;
	register int s;

	list = vmap->v_id;

	/*
	 * See the note about v_number in vn_get.
	 */
	if (vmap->v_number == 0) {
	        printf("vn_purge error: vp=0x%x vmap->v_number=%d\n",
		       vp, vmap->v_number);
	        panic("vn_purge: vmap->v_number == 0");
	}
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
		/* if vn_find succeeds, it's guaranteed to find vp */
		if (vn_find(vp, vmap->v_number) == NULL) {
			UNLOCK_VFP(vfp, s);
			VOPINFO.vn_gchg++;
			return;
		}
	}

	/* if you don't SHARE you don't get to play */
	ASSERT(vp->v_flag & VSHARE);	

	/*
	 * Check whether vp has already been reclaimed since our caller
	 * sampled its version while holding a filesystem cache lock that
	 * its VOP_RECLAIM function acquires.
	 */
	NESTED_VN_LOCK(vp);
	vn_trace_entry(vp, "vn_purge", (inst_t *)__return_address);
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
	 * either in a system cache or attached to the behavior chain.
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
 * Cause a vnode to be inacessible to everyone except those that
 * already have a reference.
 */
void
vn_kill(struct vnode *vp)
{
	register int s;
	/*REFERENCED*/
	int  closerr;

	/* if you don't SHARE you don't get to play */
	ASSERT(vp->v_flag & VSHARE);
	ASSERT(vp->v_type == VCHR);
	ASSERT(vp->v_count > 0);

	s = VN_LOCK(vp);
	/* 
	 * Add ref so don't race with vn_rele/vn_reclaim.
	 *
	 * XXX How does this prevent a race?  Since the VOP_CLOSE below 
	 * doesn't cause a vn_rele, then whatever vn_rele we're worried 
	 * about could equally as well happen prior to us bumping the
	 * count here.  Could this code be an artifact of the bug
	 * in vhangup where it was calling vn_kill on a session vnode
	 * that was already deallocated?
	 */
	vp->v_count++;
	vn_trace_hold(vp, __FILE__, __LINE__, (inst_t *)__return_address);
	VN_UNLOCK(vp, s);

	/*
	 * Clean out any file locks in the unlikely event there are any.
	 */
	if (vp->v_flag & VFRLOCKS)
		fs_cleanlock(vp, sys_flid, sys_cred);

	/*
	 * Pass special note to close routine - this won't really
	 * close anything but will prevent any further "gets" from
	 * succeeding on the vnode.
	 */
	VOP_CLOSE(vp, 0, FROM_VN_KILL, sys_cred, closerr);

	/* release our added ref */
	VN_RELE(vp);
}

/*
 * Add a reference to a referenced vnode.
 */
struct vnode *
vn_hold(struct vnode *vp)
{
	register int s = VN_LOCK(vp);

	/*
	 * Check validity of this request.  Because, the effects of vn_hold-ing
         * a vnode on the free list are so dire, we always want to panic rather
         * than doing this.  Note that non-VSHARE vnodes, which do not get
         * into the vnode free lists, are allowed to do a vn_hold when the
         * v_count is zero and that autofs takes advantage of this dubious 
         * privilege.
         *
         * Also note that autofs in the CELL_IRIX case has been modified not 
         * to do this (vn_hold a vnode with v_count equal to zero) so we 
         * could enforce stricter requirements in that case but do not
         * bother since CELL_IRIX is a dead issue in the kudzu base.
         * This will be an issue when this is merged into teak but by
         * that time teak should be enforcing the stricter requirments
         * in all configruations.  This will require hand-merging in any
	 * case.
	 */
        if (vp->v_count <= 0) {
#pragma mips_frequency_hint NEVER
		/*
		 * We are holding a vnode with no hold or a corrupted
		 * hold count.  This probably means the vnode is on the
		 * free list. If VSHARE is set, panic.
		 * If VSHARE is not set, someone (autofs) can hold the vnode
		 * with a count of 0. All others panic.
		 */
		if ((vp->v_flag & VSHARE) || (vp->v_count != 0))
			cmn_err_tag(140,CE_PANIC, 
				"holding vnode on free list %x(%x,%d)", 
				vp, vp->v_flag, vp->v_count);
	}

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
	register int mask;
	int private_vnode;
	/*REFERENCED*/
	int cache;
	extern void usync_cleanup(caddr_t);

	VOPINFO.vn_rele++;

	s = VN_LOCK(vp);
	if (--vp->v_count <= 0) {

		/*
		 * Make sure that the count has not gone through 
		 * zero.  If we were to let this go through unchecked,
		 * all kinds of terrible things could happen to the
		 * vnode lists and we wouldn't have a clue as we
		 * surveyed the wreckage.
		 */
        	if (vp->v_count < 0) {
#pragma mips_frequency_hint NEVER
	        	cmn_err_tag(141,CE_PANIC, 
				    "vnode ref count negative %x(%x,%d)", 
				    vp, vp->v_flag, vp->v_count);
		}

		/*
		 * It is absolutely, positively the case that
		 * the lock manager will not be releasing vnodes
		 * without first having released all of its locks.
		 */
		ASSERT(!(vp->v_flag & VLOCKHOLD));
		private_vnode = !(vp->v_flag & VSHARE);

		/*
		 * As soon as we turn this on, noone can find us
		 * in vn_get until we turn off VINACT or VRECLM
		 */
		vp->v_flag |= VINACT;
		VN_UNLOCK(vp, s);

		/*
		 * Cleanup left over file locks.  These can get here 
		 * due to the NLM interfaces that could keep the last
		 * close logic from finding them.  We need to do it
		 * now instead of reclaim time because XFS can reuse
		 * vnodes/inodes without going through reclaim.
		 *
		 * Note that the intent here is merely to clean up
		 * state associated with this particular vnode, which
		 * is why cleanlocks() is used instead of fs_cleanlock()
		 * (the latter being used when distributed vnodes need
		 * to be taken into account).  
		 */
		if (vp->v_filocks != NULL)
			cleanlocks(vp, IGN_PID, 0L);
		ASSERT(vp->v_filocks == NULL);

		/*
		 * If the address space backed by this vnode represents
		 * any usync objects, clean them up.
		 */
		if (vp->v_flag & VUSYNC) {
			usync_cleanup((caddr_t) vp);
			VN_FLAGCLR(vp, VUSYNC);
		}

		/*
		 * Do not make the VOP_INACTIVE call if there
		 * are no behaviors attached to the vnode to call.
		 */
		if (vp->v_fbhv != NULL) {
			VOP_INACTIVE(vp, get_current_cred(), cache);
			ASSERT(private_vnode ? 1 : cache == VN_INACTIVE_CACHE ?
			       !(vp->v_flag & VINACTIVE_TEARDOWN) : 
			       (vp->v_flag & VINACTIVE_TEARDOWN));
		}

		/*
		 * For filesystems that do not want to be part of
		 * the global vnode cache, we must not touch the
		 * vp after we have called inactive
		 */
		if (private_vnode)
			return;

		ASSERT(vp->v_next == vp && vp->v_prev == vp);

		if (mask = vfreelistmask) {
			register vfreelist_t *tp;

			vfp = &vfreelist[private.p_hand++ & mask];
			tp = vfp->vf_next;
			if (vfp->vf_lsize > tp->vf_lsize)
				vfp = tp;

			vp->v_listid = vfp->vf_listid;
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
		 *
		 * VRECLM is turned off too because it may
		 * have veen vn_reclaim'd above (this stmt.
		 * not true anymore).
		 */
		NESTED_VN_LOCK(vp);
		if (vp->v_flag & VWAIT) {
			vn_trace_entry(vp, "RELE WAKEUP",
				(inst_t *)__return_address);
			sv_broadcast_bounded(vptosync(vp));
		}
		vp->v_flag &= ~(VINACT|VWAIT|VRECLM|VGONE|VEVICT);

		/* 
		 * If not interposed for replication, and if the vnode 
		 * was opened for write, NONREPLICABLE bit is turned on.
		 * Turn it off now.
		 */
		VN_CLRNONREPLICABLE(vp);

		NESTED_UNLOCK_VFP(vfp);
		VN_UNLOCK(vp, s);

		ASSERT(fetchIntHot(&vn_nfree) >= 0);
		atomicAddIntHot(&vn_nfree, 1);
	} else
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

/*
 * Read or write a vnode.  Called from kernel code.
 */
int
vn_rdwr(enum uio_rw rw,
	struct vnode *vp,
	caddr_t base,
	size_t len,
	off_t offset,
	enum uio_seg seg,
	int ioflag,
	off_t ulimit,		/* meaningful only if rw is UIO_WRITE */
	cred_t *cr,
	ssize_t *residp,
	struct flid *fl)
{
	struct uio uio;
	struct iovec iov;
	int error;

	if (rw == UIO_WRITE && (vp->v_vfsp->vfs_flag & VFS_RDONLY))
		return EROFS;

	if ((ssize_t)len < 0)
		return EINVAL;
	iov.iov_base = base;
	iov.iov_len = len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_segflg = seg;
	uio.uio_resid = len;
	uio.uio_limit = ulimit;
	uio.uio_sigpipe = 0;
	uio.uio_pmp = NULL;
        uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
        uio.uio_pbuf = 0;

	if (rw == UIO_WRITE) {
		uio.uio_fmode = FWRITE;
		VOP_WRITE(vp, &uio, ioflag, cr, fl, error);
	} else {
		uio.uio_fmode = FREAD;
		VOP_READ(vp, &uio, ioflag, cr, fl, error);
	}
	ASSERT(uio.uio_sigpipe == 0);
	if (residp)
		*residp = uio.uio_resid;
	else if (uio.uio_resid)
		error = EIO;
	return error;
}


/*
 * Open/create a vnode.
 * This may be callable by the kernel, the only known use
 * of user context being that the current user credentials
 * are used for permissions.  crwhy is defined iff filemode & FCREAT.
 */
int
vn_open(char *pnamep,
	enum uio_seg seg,
	register int filemode,
	mode_t createmode,
	struct vnode **vpp,
	enum create crwhy,
	int cflags,
	int *ckpt)
{
	struct vnode *vp, *tvp, *openvp;
	register int mode;
	register int error;
	struct vattr vattr;
#ifdef CKPT
	ckpt_handle_t lookup = NULL;
#endif
	tvp = (struct vnode *)NULL;
	mode = 0;
	if (filemode & FREAD)
		mode |= VREAD;
	if (filemode & (FWRITE|FTRUNC))
		mode |= VWRITE;
 
	if (filemode & FCREAT) {
		/*
		 * Wish to create a file.
		 */
		vattr.va_type = VREG;
		vattr.va_mode = createmode;
		vattr.va_mask = AT_TYPE|AT_MODE;
		if (filemode & FTRUNC) {
			vattr.va_size = 0;
			vattr.va_mask |= AT_SIZE;
		}
		if (filemode & FEXCL)
			cflags |= VEXCL;
		filemode &= ~(FTRUNC|FEXCL);
#ifdef _SHAREII
		/*
		 * Adjust mode for share umask.
		 */
		SHR_SETATTR(vattr.va_mask, &vattr.va_mode);
#endif /* _SHAREII */
		
		/* 
		 * vn_create can take a while, so preempt.
		 */
		if (error = vn_create(pnamep, seg, &vattr, cflags, mode, &vp,
						crwhy, ckpt))
			return error;
		tvp = vp;
		VN_HOLD(tvp);

		VOP_SETFL(vp, 0, filemode, get_current_cred(), error);
		if (error)
			goto out;
	} else {
		/*
		 * Wish to open a file.  Just look it up.
		 */
		if (error = lookupname(pnamep, seg, FOLLOW, NULLVPP, &vp,
#ifdef CKPT
				(ckpt)? &lookup : NULL))
#else
				NULL))
#endif
			return error;

		tvp = vp;
		VN_HOLD(tvp);
#ifdef CKPT
		if (ckpt) {
			if (lookup)
				*ckpt = ckpt_lookup_add(vp, lookup);
			else
				*ckpt = -1;
		}
#endif
		VOP_SETFL(vp, 0, filemode, get_current_cred(), error);
		if (error)
			goto out;

		/*
		 * Can't write directories, active texts, swap files, or
		 * read-only filesystems.  Can't truncate files
		 * on which mandatory locking is in effect.
		 */
		if (filemode & (FWRITE|FTRUNC)) {
			if (vp->v_type == VDIR) {
				error = EISDIR;
				goto out;
			}
			if (vp->v_vfsp->vfs_flag & VFS_RDONLY) {
				error = EROFS;
				goto out;
			}
			if ((vp->v_flag & VISSWAP) && vp->v_type == VREG) {
				error = EBUSY;
				goto out;
			}
			/*
			 * Can't truncate files on which mandatory locking
			 * is in effect and locks exist on the file.
			 */
			if ((filemode & FTRUNC) && 
			    (vp->v_flag & (VFRLOCKS|VENF_LOCKING)) ==
			    (VFRLOCKS|VENF_LOCKING)) {
				error = EAGAIN;
			}
			if (error)
				goto out;
		}
		/* Check discretionary permissions.*/
		VOP_ACCESS(vp, mode, get_current_cred(), error);
		if (error)
			goto out;
	}

	if ((filemode & FWRITE) && !VN_ISREPLICABLE(vp)){
		VN_FLAGSET(vp, VNONREPLICABLE);
	}

	/*
	 * Do opening protocol.
	 */
	openvp = vp;
	VOP_OPEN(openvp, &vp, filemode, get_current_cred(), error);
	if (!error) {
		/*REFERENCED*/
		int unused;

		if (tvp) {
			VN_RELE(tvp);
			/* avoid extra VN_RELE in error case below */
			tvp = NULL;	
		}
		/*
		 * Truncate if required.
		 */
		if ((filemode & FTRUNC) && vp->v_type == VREG) {
			vattr.va_size = 0;
			vattr.va_mask = AT_SIZE;
			VOP_SETATTR(vp, &vattr, 0, get_current_cred(), error);
			if (error)
				/*
				 * since the open never succeeded, there can't
				 * be any locks
				 */
				VOP_CLOSE(vp, filemode, L_TRUE, 
					  get_current_cred(), unused);
		}
	}
out:
	if (error) {
		if (tvp)
			VN_RELE(tvp);
		VN_RELE(vp);
	} else
		*vpp = vp;
	return error;
}

/*
 * Create a vnode (makenode).
 */
/*ARGSUSED*/
int
vn_create(
        char		*pnamep,
	enum uio_seg 	seg,
	vattr_t 	*vap,
	int 		flags,
	int 		mode,
	vnode_t 	**vpp,
	enum create 	why,
	int		*ckpt)
{
	vnode_t		*dvp;	/* ptr to parent dir vnode */
	pathname_t	 pn;
	int 		error;
	vpagg_t		*vpag;
	int		existing = 0;

	ckpt_handle_t	*lookupp = NULL;
#ifdef CKPT
	ckpt_handle_t	lookup = NULL;
#endif
	ASSERT((vap->va_mask & (AT_TYPE|AT_MODE)) == (AT_TYPE|AT_MODE));

	/*
	 * VOP_CREATE/MKDIR needs the project id.
	 */
	VPROC_GETVPAGG(curvprocp, &vpag);
	vap->va_projid = VPAG_GETPRID(vpag);
	vap->va_mask |= AT_PROJID;

	/*
	 * Lookup directory.
	 * If new object is a file, call lower level to create it.
	 * Note that it is up to the lower level to enforce exclusive
	 * creation, if the file is already there.
	 * This allows the lower level to do whatever
	 * locking or protocol that is needed to prevent races.
	 * If the new object is directory call lower level to make
	 * the new directory, with "." and "..".
	 */
	if (error = pn_get(pnamep, seg, &pn))
		return error;
	_SAT_PN_SAVE(&pn, curuthread);
	dvp = NULL;
	*vpp = NULL;
#ifdef CKPT
	lookupp = (ckpt)? &lookup : NULL;
#endif
	/*
	 * lookup will find the parent directory for the vnode.
	 * When it is done the pn holds the name of the entry
	 * in the directory.
	 * If this is a non-exclusive create we also find the node itself.
	 */
	if (flags & VEXCL) 
		error = lookuppn(&pn, NO_FOLLOW, &dvp, NULLVPP, lookupp); 
	else 
		error = lookuppn(&pn, FOLLOW, &dvp, vpp, lookupp); 
	if (error) {
		pn_free(&pn);
		if (why == CRMKDIR && error == EINVAL)
			error = EEXIST;		/* SVID */
		return error;
	}
	ASSERT(dvp->v_count > 0);
	vn_trace_entry(dvp, "vn_create", (inst_t *)__return_address);
	if (*vpp)
		vn_trace_entry(*vpp, "vn_create:f", (inst_t *)__return_address);

	if (why != CRMKNOD)
		vap->va_mode &= ~VSVTX;

	/*
	 * Make sure filesystem is writeable.
	 */
	if (dvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		if (*vpp)
			VN_RELE(*vpp);
		error = EROFS;
	} else if (!(flags & VEXCL) && *vpp != NULL) {
		register struct vnode *vp = *vpp;

		/*
		 * File already exists.  If a mandatory lock has been
		 * applied, return EAGAIN.
		 */
		if ((vp->v_flag & (VFRLOCKS|VENF_LOCKING)) == 
		    (VFRLOCKS|VENF_LOCKING)) {
			error = EAGAIN;
			VN_RELE(vp);
			goto out;
		}

		/* do not permit truncating a swap file */
		if ((vp->v_flag & VISSWAP) && vp->v_type == VREG) {
			error = EBUSY;
			VN_RELE(vp);
			goto out;
		}

		/*
		 * If the file is the root of a VFS, we've crossed a
		 * mount point and the "containing" directory that we
		 * acquired above (dvp) is irrelevant because it's in
		 * a different file system.  We apply VOP_CREATE to the
		 * target itself instead of to the containing directory
		 * and supply a null path name to indicate (conventionally)
		 * the node itself as the "component" of interest.
		 *
		 * The intercession of the file system is necessary to
		 * ensure that the appropriate permission checks are
		 * done.
		 */
		if (vp->v_flag & VROOT) {
			/*
			 * lvp (NULL) is needed since VOP_CREATE now has vpp
			 * as an in and out parameter.
			 * It has special meaning if set.
			 */
			vnode_t 	*lvp = NULL;

			ASSERT(why != CRMKDIR);
			VOP_CREATE(vp, "", vap, flags, mode, &lvp, 
				   get_current_cred(), error);
			/*
			 * If the create succeeded, it will have created
			 * a new reference to the vnode.  Give up the
			 * original reference.
			 */
			VN_RELE(vp);
			goto out;
		}

		/*
		 * Don't throw the vnode. Give it to VOP_CREATE
		 * so it can prevent another lookup and then
		 * deal with it in a non-racy manner.
		 */
		ASSERT(*vpp == vp);
		if (why == CRMKDIR) /* Won't be going to VOP_CREATE */
			VN_RELE(vp);
		ASSERT(!error);
		existing = 1;
	}

	if (error == 0) {
		/*
		 * Call fs dependent mkdir() to create dir.  Otherwise, fs
		 * dependent create.
		 */
		if (why == CRMKDIR || (why == CRMKNOD && vap->va_type == VDIR)) {
			VOP_MKDIR(dvp, pn.pn_path, vap, vpp, 
				  get_current_cred(), error);
		} else {
			VOP_CREATE(dvp, pn.pn_path, vap, flags, mode,
					   vpp, get_current_cred(), error);
			if (!error && *vpp) {
				IMON_EVENT(*vpp, get_current_cred(), 	
					   IMON_CONTENT);
			} else if ((error == ENOSYS) && *vpp) {
				VN_RELE(*vpp);
			}
		}
	}
out:
#ifdef CKPT
	if (lookup) {

		ASSERT(ckpt);

		if (!error && *vpp)
			*ckpt = ckpt_lookup_add(*vpp, lookup);
		else {
			*ckpt = -1;
			ckpt_lookup_free(lookup);
		}

	} else if (ckpt)
		*ckpt = -1;
#endif

        /*
         * Set Trix extended attributes on the vnode if:
         *      there was no previous error, and
         *      the vnode is newly created, and
         *      we have a handle on that vnode
	 *
         * At this time, those attributes are
         *      MAC label
         *      directory default ACL
         *
         * If appropriate extended security attributes cannot
         * be set on a filesystem object, it is removed.
         */
        if (!error && !existing && *vpp) {

		/* MAC label
		 *
		 * No file/directory should never get created with the
		 * moldy bit set by default: check to see if the process
		 * label has the moldy bit set and set the label without
		 * it.
		 */
		mac_label * label = get_current_cred()->cr_mac;
                if ( _MAC_IS_MOLDY ( label ) )
		{
		    if ( label = _MAC_DEMLD ( label ) )
		    {
			error = _MAC_VSETLABEL(*vpp, label );
			kern_free ( label );
		    }
		    else
			error = ENOMEM;
		}
		else
		    error = _MAC_VSETLABEL(*vpp, label );

		/* Directory default ACL */
                if ( error || (error = _ACL_INHERIT(dvp, *vpp, vap))) {
                        cmn_err_tag(318,CE_NOTE, "vn_create: %s(%d)",
                                __FILE__, __LINE__);
                        VOP_REMOVE(dvp, pn.pn_path, get_current_cred(), error);
                }
        }
 
	pn_free(&pn);
	VN_RELE(dvp);
	return error;
}

/*
 * Link.
 */
int
vn_link(char *from, char *to, enum uio_seg seg, enum symfollow follow)
{
	struct vnode *fvp;		/* from vnode ptr */
	struct vnode *tdvp;		/* to directory vnode ptr */
	struct pathname pn;
	register int error;
	struct vattr vattr;
	long fsid;

	fvp = tdvp = NULL;
	if (error = pn_get(to, seg, &pn))
		return error;
	_SAT_PN_SAVE(&pn, curuthread);

	if (error = lookupname(from, seg, follow, NULLVPP, &fvp, NULL))
		goto out;

	if (error = lookuppn(&pn, FOLLOW, &tdvp, NULLVPP, NULL))
		goto out;

	/*
	 * Make sure both source vnode and target directory vnode are
	 * in the same vfs and that it is writeable.
	 */
	vattr.va_mask = AT_FSID;
	VOP_GETATTR(fvp, &vattr, 0, get_current_cred(), error);
	if (error)
		goto out;
	fsid = vattr.va_fsid;
	VOP_GETATTR(tdvp, &vattr, 0, get_current_cred(), error);
	if (error)
		goto out;
	if (fsid != vattr.va_fsid) {
		error = EXDEV;
		goto out;
	}
	if (tdvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}

	if (!error)
		VOP_LINK(tdvp, fvp, pn.pn_path, get_current_cred(), error);
out:
	pn_free(&pn);
	if (fvp)
		VN_RELE(fvp);
	if (tdvp)
		VN_RELE(tdvp);
	_SAT_ACCESS2(SAT_FILE_CRT_DEL2, error);
	return error;
}

/*
 * Rename.
 */
int
vn_rename(char *from, char *to, enum uio_seg seg)
{
	struct vnode *fdvp;		/* from directory vnode ptr */
	struct vnode *fvp;		/* from vnode ptr */
	struct vnode *tdvp;		/* to directory vnode ptr */
	struct pathname fpn;		/* from pathname */
	struct pathname tpn;		/* to pathname */
	register int error;

	fdvp = tdvp = fvp = NULL;
	/*
	 * Get to and from pathnames.
	 */
	if (error = pn_get(from, seg, &fpn))
		return error;
	_SAT_PN_SAVE(&fpn, curuthread);

	if (error = pn_get(to, seg, &tpn)) {
		pn_free(&fpn);
		return error;
	}
	_SAT_PN_SAVE(&tpn, curuthread);

	/*
	 * Lookup to and from directories.
	 */

	if (error = lookuppn(&fpn, NO_FOLLOW, &fdvp, &fvp, NULL))
		goto out;
	vn_trace_entry(fdvp, "vn_rename:fd", (inst_t *)__return_address);
	/*
	 * Make sure there is an entry.
	 */
	if (fvp == NULL) {
		error = ENOENT;
		goto out;
	}
	vn_trace_entry(fvp, "vn_rename:f", (inst_t *)__return_address);
	/*
	 * Make sure we're not moving an active swap file.
	 * This prevents mv/reboot/rm from being able to
	 * remove the swap file.  Must do swap -d first.
	 */
	if (fvp->v_flag & VISSWAP && fvp->v_type == VREG) {
		error = EBUSY;
		goto out;
	}

	if (error = lookuppn(&tpn, NO_FOLLOW, &tdvp, NULLVPP, NULL))
		goto out;
	vn_trace_entry(tdvp, "vn_rename:td", (inst_t *)__return_address);
	/*
	 * Make sure that the from vnode and to directory are 
	 * in the same vfs, or that the from vnode is not a 
	 * mount point (for lofs renames), and that the from and 
	 * to directories share the same vfs.
	 * Also make sure that the to directory is writable.
	 * XXX this traditional vnodes test differs from the va_fsid
	 *     test used by vn_link
	 */
	if ((fvp->v_vfsp != tdvp->v_vfsp && (fvp->v_flag & VROOT) != 0) ||
            (fdvp->v_vfsp != tdvp->v_vfsp)) {
		error = EXDEV;
		goto out;
	}
	if (tdvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}
	VOP_RENAME(fdvp, fpn.pn_path, tdvp, tpn.pn_path, &tpn,
			   get_current_cred(), error);
	/*
	 *  Must explicitly post imon events because imon-vnode
	 *  layer will never see it if fdvp isn't monitored. K<bob> 6/22/94
	 */
	if (error == 0) {
		IMON_EVENT(tdvp, get_current_cred(), IMON_CONTENT);
		IMON_EVENT(fvp, get_current_cred(), IMON_RENAME);
	}
out:
	pn_free(&fpn);
	pn_free(&tpn);
	if (fvp)
		VN_RELE(fvp);
	if (fdvp)
		VN_RELE(fdvp);
	if (tdvp)
		VN_RELE(tdvp);
	_SAT_ACCESS2(SAT_FILE_CRT_DEL2, error);
	return error;
}

/*
 * Remove a file or directory.
 */
int
vn_remove(char *fnamep, enum uio_seg seg, enum rm dirflag)
{
	struct vnode *vp;		/* entry vnode */
	struct vnode *dvp;		/* ptr to parent dir vnode */
	struct pathname pn;		/* name of entry */
	enum vtype vtype;
	register int error;
	register struct vfs *vfsp;

	if (error = pn_get(fnamep, seg, &pn))
		return error;
	_SAT_PN_SAVE(&pn, curuthread);
	vp = NULL;
	if (error = lookuppn(&pn, NO_FOLLOW, &dvp, &vp, NULL)) {
		pn_free(&pn);
		return error;
	}

	vn_trace_entry(dvp, "vn_remove", (inst_t *)__return_address);
	/*
	 * Make sure there is an entry.
	 */
	if (vp == NULL) {
		error = ENOENT;
		goto out;
	}
	vn_trace_entry(vp, "vn_remove", (inst_t *)__return_address);

	vfsp = vp->v_vfsp;

	/*
	 * If the named file is the root of a mounted filesystem, fail.
	 */
	if (vp->v_flag & VROOT) {
		error = EBUSY;
		goto out;
	}

	/*
	 * Make sure filesystem is writeable.
	 */
	if (vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}

	/*
	 * Make sure we're not removing an active swap file.
	 */
	if (vp->v_flag & VISSWAP && vp->v_type == VREG) {
		error = EBUSY;
		goto out;
	}

	/*
	 * If vnode represents a named semaphore,
	 * cleanup the kernel semaphore state.
	 */
	if (vp->v_flag & VSEMAPHORE) {
		if (error = psema_indirect_unlink(vp))
			goto out;
	}

	/*
	 * Release vnode before removing.
	 */
	vtype = vp->v_type;
	VN_RELE(vp);
	vp = NULL;
	/*
	 * If caller is using rmdir(2), it can be applied only to directories.
	 * Unlink(2) can be applied to anything.
	 */
	if (dirflag == RMDIRECTORY) {
		if (vtype != VDIR) {
			error = ENOTDIR;
			goto out;
		}
		VOP_RMDIR(dvp, pn.pn_path, curuthread->ut_cdir,
			  get_current_cred(), error);
	} else
		VOP_REMOVE(dvp, pn.pn_path, get_current_cred(), error);
out:
	pn_free(&pn);
	if (vp != NULL)
		VN_RELE(vp);
	VN_RELE(dvp);
	return error;
}

/*
 * Compare two vnodes.  For now we use the ops of the base
 * behavior to decide if the VOP_CMP() call will make any
 * sense.
 */
int
vn_cmp(vnode_t *vp1, vnode_t *vp2)
{
	bhv_desc_t	*bdp1;
	bhv_desc_t	*bdp2;
	int		cmp;

	if (vp1 == vp2) {
		return 1;
	}
	if ((vp1 == NULL) ||
	    (vp2 == NULL) ||
	    (vp1->v_fbhv == NULL) ||
	    (vp2->v_fbhv == NULL)) {
		return 0;
	}
	bdp1 = vn_bhv_base_unlocked(VN_BHV_HEAD(vp1));
	bdp2 = vn_bhv_base_unlocked(VN_BHV_HEAD(vp2));
	if (BHV_OPS(bdp1) == BHV_OPS(bdp2)) {
		VOP_CMP(vp1, vp2, cmp);
		return cmp;
	}

	return 0;
}

/*ARGSUSED*/
pfd_t *
vn_pfind(struct vnode *vp, pgno_t pageno, int ckey, void *pm)
{

	pfd_t	*pfd;

	pfd = vnode_pfind(vp, pageno, ckey);

#if  defined(NUMA_REPLICATION)
	/* 
	 * If we found a page, and vnode is a candidate for replication,
	 * check with the replication module, if it's okay to return 
	 * this page.
	 */
	if (pfd && VN_ISREPLICABLE(vp))
		pfd = repl_pfind(vp, pageno, ckey, pm, pfd);

#endif	/* defined(NUMA_REPLICATION) */

	return pfd; 
}

#ifdef VNODE_TRACING
/*
 * Vnode tracing code.
 */
void
vn_trace_entry(vnode_t *vp, char *func, inst_t *ra)
{
	ktrace_enter(vp->v_trace, (void *)(__psint_t)VNODE_KTRACE_ENTRY,
		(void *)func, 0, (void *)(__psint_t)vp->v_count, (void *)ra,
		(void *)(__psunsigned_t)vp->v_flag, (void *)(__psint_t)cpuid(),
		(void *)(__psint_t)current_pid(), 0, 0, 0, 0, 0, 0, 0, 0);
}

void
vn_trace_hold(vnode_t *vp, char *file, int line, inst_t *ra)
{
	ktrace_enter(vp->v_trace, (void *)(__psint_t)VNODE_KTRACE_HOLD,
		(void *)file, (void *)(__psint_t)line,
		(void *)(__psint_t)vp->v_count, (void *)ra,
		(void *)(__psunsigned_t)vp->v_flag, (void *)(__psint_t)cpuid(),
		(void *)(__psint_t)current_pid(), 0, 0, 0, 0, 0, 0, 0, 0);
}

void
vn_trace_ref(vnode_t *vp, char *file, int line, inst_t *ra)
{
	ktrace_enter(vp->v_trace, (void *)(__psint_t)VNODE_KTRACE_REF,
		(void *)file, (void *)(__psint_t)line,
		(void *)(__psint_t)vp->v_count, (void *)ra,
		(void *)(__psunsigned_t)vp->v_flag, (void *)(__psint_t)cpuid(),
		(void *)(__psint_t)current_pid(), 0, 0, 0, 0, 0, 0, 0, 0);
}

void
vn_trace_rele(vnode_t *vp, char *file, int line, inst_t *ra)
{
	ktrace_enter(vp->v_trace, (void *)(__psint_t)VNODE_KTRACE_RELE,
		(void *)file, (void *)(__psint_t)line,
		(void *)(__psint_t)vp->v_count, (void *)ra,
		(void *)(__psunsigned_t)vp->v_flag, (void *)(__psint_t)cpuid(),
		(void *)(__psint_t)current_pid(), 0, 0, 0, 0, 0, 0, 0, 0);
}
#endif /* VNODE_TRACING */
