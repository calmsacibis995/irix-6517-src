/*
 * Copyright (c) 1988, 1991 by Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/fs/lofs_node.h>
#include <sys/fs/lofs_info.h>

extern vnodeops_t lofs_vnodeops;
#ifdef	LODEBUG
extern int lodebug;
#endif
extern uint lofs_mountid;
static lnode_t *lfind(struct vnode *, struct loinfo *);
static void lsave(lnode_t *);
static struct vfs *makelfsnode(struct vfs *, struct loinfo *);
static void freelfsnode(struct lfsnode *, struct loinfo *);
static struct lfsnode *lfsfind(struct vfs *, struct loinfo *);
int lofs_lfscount = 0;
int lofs_lncount = 0;
int lofs_freecount = 0;

lnode_t *lofreelist;
kmutex_t lofs_ltable_lock;	/* protects lofs_ltable and lofreelist */

void
lofs_subrinit()
{
	mutex_init(&lofs_ltable_lock, MUTEX_DEFAULT, "lofs ltable lock");
}

/*
 * return a looped back vnode for the given vnode.
 * If no lnode exists for this vnode create one and put it
 * in a table hashed by vnode.  If the lnode for
 * this fhandle is already in the table return it
 * The lnode will be flushed from the
 * table when lofs_inactive calls freelonode.
 * NOTE: vp is assumed to be a held vnode.
 */
struct vnode *
makelonode(vp, li)
	struct vnode *vp;
	struct loinfo *li;
{
	lnode_t *lp;
	struct vfs *vfsp;
	struct vnode *lvp;
	vmap_t vmap;

again:
	mutex_enter(&lofs_ltable_lock);
	if ((lp = lfind(vp, li)) == NULL) {
		vfsp = makelfsnode(vp->v_vfsp, li);
		if (lofreelist) {
			lp = lofreelist;
			lofreelist = lp->lo_next;
			lofs_freecount--;
			mutex_exit(&lofs_ltable_lock);
			bzero((caddr_t)lp, (u_int)(sizeof (*lp)));
		} else {
			mutex_exit(&lofs_ltable_lock);
			lp = (lnode_t *)kmem_zalloc(sizeof (*lp), KM_SLEEP);
		}
		lvp = vn_alloc(vfsp, vp->v_type, li->li_rdev);
		bhv_desc_init(ltobhv(lp), lp, lvp, &lofs_vnodeops);
		vn_bhv_insert_initial(VN_BHV_HEAD(lvp), ltobhv(lp));

		/*
		 * The VDOCMP flag tells the VN_CMP macro that lofs
		 * has a special implementation of VOP_CMP.
		 */
		VN_FLAGSET(lvp, VDOCMP | VINACTIVE_TEARDOWN );
		lp->lo_vp = vp;
		lp->lo_vnode = lvp;

		mutex_enter(&lofs_ltable_lock);
		if (lfind(vp, li) != NULL) {
			vn_bhv_remove(VN_BHV_HEAD(lvp), ltobhv(lp));
			lp->lo_next = lofreelist;
			lofreelist = lp;
			lofs_freecount++;
			mutex_exit(&lofs_ltable_lock);
			vn_free(lvp);
			goto again;
		}

		lsave(lp);
		li->li_refct++;
		mutex_exit(&lofs_ltable_lock);
	} else {
		lvp = ltov(lp);
		VMAP(lvp, vmap);
		mutex_exit(&lofs_ltable_lock);
		if (!(lvp = vn_get(lvp, &vmap, 0))) {
			goto again;
		}
		VN_RELE(vp);
	}

	return lvp;
}

/*
 * Get/Make vfs structure for given real vfs
 */
static struct vfs *
makelfsnode(vfsp, li)
	struct vfs *vfsp;
	struct loinfo *li;
{
	struct lfsnode *lfs;

	if (vfsp == li->li_realvfs)
		return (li->li_mountvfs);
	if ((lfs = lfsfind(vfsp, li)) == NULL) {
		lfs = (struct lfsnode *)kmem_zalloc(sizeof (*lfs), KM_SLEEP);
		lofs_lfscount++;
		lfs->lfs_realvfs = vfsp;
		bhv_head_init(&(lfs->lfs_vfs.vfs_bh), "lofs");
		vfs_insertbhv(&lfs->lfs_vfs, &lfs->lfs_bhv, &lofs_vfsops, li);
		lfs->lfs_vfs.vfs_fstype = lofsfstype;
		lfs->lfs_vfs.vfs_dev = vfsp->vfs_dev;
		lfs->lfs_vfs.vfs_fsid.val[0] = vfsp->vfs_fsid.val[0];
		lfs->lfs_vfs.vfs_fsid.val[1] = vfsp->vfs_fsid.val[1];
		lfs->lfs_vfs.vfs_flag =
			(vfsp->vfs_flag | li->li_mflag) & INHERIT_VFS_FLAG;
		lfs->lfs_vfs.vfs_bsize = vfsp->vfs_bsize;
		lfs->lfs_next = li->li_lfs;
		li->li_lfs = lfs;
	}
	lfs->lfs_refct++;
	return (&lfs->lfs_vfs);
}

/*
 * Free lfs node since no longer in use
 */
static void
freelfsnode(lfs, li)
	struct lfsnode *lfs;
	struct loinfo *li;
{
	struct lfsnode *prev = NULL;
	struct lfsnode *this;

	for (this = li->li_lfs; this; this = this->lfs_next) {
		if (this == lfs) {
			if (prev == NULL)
				li->li_lfs = lfs->lfs_next;
			else
				prev->lfs_next = lfs->lfs_next;
			bhv_remove(&(lfs->lfs_vfs.vfs_bh), &(lfs->lfs_bhv))
			bhv_head_destroy(&(lfs->lfs_vfs.vfs_bh));
			kmem_free(lfs, sizeof (struct lfsnode));
			lofs_lfscount--;
			return;
		}
		prev = this;
	}
	/* NOTREACHED */
#ifdef LODEBUG
	lofs_dprint(lodebug, 1, "freelfsnode: Could not find 0x%x\n", lfs);
#endif
}

/*
 * Find lfs given real vfs and mount instance(li)
 */
static struct lfsnode *
lfsfind(vfsp, li)
	struct vfs *vfsp;
	struct loinfo *li;
{
	struct lfsnode *lfs;

	for (lfs = li->li_lfs; lfs; lfs = lfs->lfs_next)
		if (lfs->lfs_realvfs == vfsp)
			return (lfs);
	return (NULL);
}

/*
 * Find real vfs given loopback vfs
 */
struct vfs *
lofs_realvfs(vfsp, li)
	struct vfs *vfsp;
	struct loinfo *li;
{
	struct lfsnode *lfs;

	if (vfsp == li->li_mountvfs)
		return (li->li_realvfs);
	for (lfs = li->li_lfs; lfs; lfs = lfs->lfs_next)
		if (vfsp == &lfs->lfs_vfs)
			return (lfs->lfs_realvfs);
	/* NOTREACHED */
#ifdef LODEBUG
	lofs_dprint(lodebug, 1, "lofs_realvfs: Could not find 0x%x\n", vfsp);
#endif
}

/*
 * Lnode lookup stuff.
 * These routines maintain a table of lnodes hashed by vp so
 * that the lnode for a vp can be found if it already exists.
 * NOTE: LTABLESIZE must be a power of 2 for ltablehash to work!
 */

#define	LTABLESIZE	64
#define	ltablehash(vp)	((((__psunsigned_t)(vp))>>10) & (LTABLESIZE-1))

lnode_t *ltable[LTABLESIZE];

/*
 * Put a lnode in the table
 */
static void
lsave(lp)
	lnode_t *lp;
{

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lsave lp %x hash %d\n",
			lp, ltablehash(lp->lo_vp));
#endif
	lp->lo_next = ltable[ltablehash(lp->lo_vp)];
	ltable[ltablehash(lp->lo_vp)] = lp;
}

/*
 * Remove a lnode from the table
 */
void
freelonode(lp)
	lnode_t *lp;
{
	lnode_t *lt;
	lnode_t *ltprev = NULL;
	struct loinfo *li;
	struct lfsnode *lfs;
	struct vfs *vfsp;
	struct vnode *vp = ltov(lp);
	struct vnode *realvp = realvp(ltobhv(lp));
	bhv_desc_t *vfs_bdp;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "freelonode lp %x hash %d\n",
			lp, ltablehash(lp->lo_vp));
#endif
	mutex_enter(&lofs_ltable_lock);

	for (lt = ltable[ltablehash(lp->lo_vp)]; lt;
	    ltprev = lt, lt = lt->lo_next) {
		if (lt == lp) {
#ifdef LODEBUG
			lofs_dprint(lodebug, 4, "freeing %x, vfsp %x\n",
					vp, vp->v_vfsp);
#endif
			vfsp = vp->v_vfsp;
			vfs_bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), 
							&lofs_vfsops);
			li = vfs_bhvtoli(vfs_bdp);
			li->li_refct--;
			if (vfsp != li->li_mountvfs) {
				/* check for ununsed lfs */
				lfs = lfsfind(lofs_realvfs(vfsp, li), li);
				lfs->lfs_refct--;
				if (lfs->lfs_refct <= 0)
					freelfsnode(lfs, li);
			}
			if (ltprev == NULL) {
				ltable[ltablehash(lp->lo_vp)] = lt->lo_next;
			} else {
				ltprev->lo_next = lt->lo_next;
			}
			lt->lo_next = lofreelist;
			lofreelist = lt;
			lofs_freecount++;
			/*
			 * Pull our behavior out of the chain of this
			 * vnode.
			 */
			VN_FLAGCLR(vp, VDOCMP);
			vn_bhv_remove(VN_BHV_HEAD(vp), ltobhv(lp));
			mutex_exit(&lofs_ltable_lock);
			VN_RELE(realvp);
			return;
		}
	}
	mutex_exit(&lofs_ltable_lock);
#ifdef LODEBUG
	lofs_dprint(lodebug, 1, "freelonode: Could not find 0x%x\n", lp);
#endif
}

/*
 * Lookup a lnode by vp
 */
static lnode_t *
lfind(vp, li)
	struct vnode *vp;
	struct loinfo *li;
{
	register lnode_t *lt;
	bhv_desc_t *bdp;

	lt = ltable[ltablehash(vp)];
	while (lt != NULL) {
		if (lt->lo_vp == vp) {
			bdp = bhv_lookup_unlocked(VFS_BHVHEAD(ltov(lt)->v_vfsp),
							&lofs_vfsops);
			if (vfs_bhvtoli(bdp) == li) {
				return (lt);
			}
		}
		lt = lt->lo_next;
	}
	return (NULL);
}

/*
 * Utilities used by both client and server
 * Standard levels:
 * 0) no debugging
 * 1) hard failures
 * 2) soft failures
 * 3) current test software
 * 4) main procedure entry points
 * 5) main procedure exit points
 * 6) utility procedure entry points
 * 7) utility procedure exit points
 * 8) obscure procedure entry points
 * 9) obscure procedure exit points
 * 10) random stuff
 * 11) all <= 1
 * 12) all <= 2
 * 13) all <= 3
 * ...
 */

#ifdef	LODEBUG
/* VARARGS3 */
void
lofs_dprint(int var, int level, char *str, ...)
{
	va_list ap;
	va_start(ap, str);
	if (var == level || (var > 10 && (var - 10) >= level))
		icmn_err(CE_CONT, str, ap);
	va_end(ap);
	return;
}
#endif
