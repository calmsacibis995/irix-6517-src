/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uts-comm:fs/namefs/namevfs.c	1.23"

/*
 * $Revision: 1.49 $
 */

/*
 * This file supports the vfs operations for the NAMEFS file system.
 */

#include <string.h>
#include <sys/types.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/statvfs.h>
#include <sys/strsubr.h>
#include <sys/strmp.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <fs/namefs/namenode.h>

/*
 * The next set of lines define the bit map for obtaining
 * unique node ids. The value chosen for the maximum node id
 * is arbitrary and should be altered to better fit the system
 * on which this file system is defined.  The maximum value can
 * not exceed the maximum value for a long, since the node id
 * must fit into a long.  (In fact, currently the routine
 * that assigns node ids restricts it to fit in a u_short.)
 * The maximum number of mounted file descriptors at a given
 * time will be limited to NMMAXID-1 (node id 0 isn't used).
 *
 * nmap    --> bitmap with one bit per node id
 * testid  --> is this node already in use?
 * setid   --> mark this node id as used
 * clearid --> mark this node id as unused
 */

#define NMMAXID		8191
#define testid(i)	((nmmap[(i)/NBBY] & (1 << ((i)%NBBY))))
#define setid(i)	((nmmap[(i)/NBBY] |= (1 << ((i)%NBBY))), (nmids++))
#define clearid(i)	((nmmap[(i)/NBBY] &= ~(1 << ((i)%NBBY))), (nmids--))

static lock_t nmlock;
static char nmmap[NMMAXID/NBBY + 1];
static ushort nmids;

/*
 * Define the routines in this file.
 */
int	nm_unmountall(struct vnode *, struct cred *);

/*
 * Define external routines and variables.
 */
extern vnodeops_t nm_vnodeops;
extern vfsops_t nm_vfsops;
struct namenode	*namealloc;
mutex_t		nameallocmon;

/*
 * Define data structures.
 */
static dev_t		namedev;
static int		namefstype;
static vfs_t		*namevfsp;
static bhv_desc_t 	name_bhv;
static vfsops_t		dummyvfsops;


/*
 * File system initialization routine. Save the file system
 * type, establish a file system device number and initialize
 * namealloc.
 */
/* ARGSUSED */
int
nm_init(
	register struct vfssw *vswp,
	int fstype)
{
	register int dev;

	namefstype = fstype;
	spinlock_init(&nmlock, "nmlock");
	if ((dev = getudev()) == -1) {
		cmn_err(CE_WARN, "nm_init: can't get unique device number");
		dev = 0;
	}
	namedev = makedevice(dev, 0);
	mutex_init(&nameallocmon, MUTEX_DEFAULT, "namemon");
	namealloc = NULL;
	namevfsp = vfs_allocate();
	vfs_insertbhv(namevfsp, &name_bhv, &dummyvfsops, NULL);
	namevfsp->vfs_bsize = 1024;
	namevfsp->vfs_fstype = namefstype;
	namevfsp->vfs_fsid.val[0] = namedev;
	namevfsp->vfs_fsid.val[1] = namefstype;
	namevfsp->vfs_dev = namedev;
	return (0);
}

/*
 * Mount a file descriptor onto the node in the file system.
 * Create a new vnode, update the attributes with info from the
 * file descriptor and the mount point.  The mask, mode, uid, gid,
 * atime, mtime and ctime are taken from the mountpt.  Link count is
 * set to one, the file system id is namedev and nodeid is unique
 * for each mounted object.  Other attributes are taken from mount point.
 *
 * Make sure user is owner with write permissions on mount point or
 * is super user.
 * Hash the new vnode and return 0.
 * Upon entry to this routine, the file descriptor is in the
 * fd field of a struct namefd.  Copy that structure from user
 * space and retrieve the file descriptor.
 */
/*ARGSUSED*/
int
nm_mount(
	register struct vfs *vfsp,	/* vfs for this mount */
	struct vnode *mvp,		/* vnode of mount point */
	struct mounta *uap,		/* user arguments */
	char *attrs,
	struct cred *crp)		/* user credentials */
{
	struct namefd namefdp;
	struct vnode *filevp;		/* file descriptor vnode */
	struct vfile *fp;
	struct vnode *newvp;		/* vnode representing this mount */
	struct namenode	*nodep;		/* namenode for this mount */
	struct vattr filevattr;		/* attributes of file dec.  */
	struct vattr *vattrp;		/* attributes of this mount */
	struct stdata *stp;
	u_short nodeid;
	int error;
	int s;

	/*
	 * Get the file descriptor from user space.
	 * Make sure the file descriptor is valid and has an
	 * associated file pointer.
	 * If so, extract the vnode from the file pointer.
	 */
	if (uap->datalen != sizeof(struct namefd))
		return (EINVAL);
	if (copyin(uap->dataptr, &namefdp, uap->datalen))
		return (EFAULT);
	if (error = getf(namefdp.fd, &fp))
		return (error);
	/*
	 * If the mount point already has something mounted
	 * on it, disallow this mount.  (This restriction may
	 * be removed in a later release).
	 */
	if (mvp->v_flag & VROOT) {
		return (EBUSY);
	}

	if (!VF_IS_VNODE(fp)) {
		return (EINVAL);
	}

	filevp = VF_TO_VNODE(fp);
	if (filevp->v_type == VDIR) {
		return (EINVAL);
	}

	/*
         * Make sure the file descriptor is not the root of some
         * file system.
         */
        if (filevp->v_flag & VROOT) {
		return (EBUSY);
	}

        /*
         * Create a reference and allocate a namenode to represent
         * this mount request.
         */
	nodep = kmem_zalloc(sizeof *nodep, KM_SLEEP);

	vattrp = &nodep->nm_vattr;

	vattrp->va_mask = AT_ALL;
	VOP_GETATTR(mvp, vattrp, 0, crp, error);
	if (error)
		goto out;

	filevattr.va_mask = AT_ALL;
	VOP_GETATTR(filevp, &filevattr, 0, crp, error);
	if (error)
		goto out;
	/*
	 * Make sure the user is the owner of the mount point (or
	 * is privileged) and has write permission.
	 */
	if (vattrp->va_uid != crp->cr_uid && !_CAP_CRABLE(crp, CAP_MOUNT_MGT)) {
		error = EPERM;
		goto out;
	}
	VOP_ACCESS(mvp, VWRITE, crp, error);
	if (error)
		goto out;

	/*
	 * If the file descriptor has file/record locking, don't
	 * allow the mount to succeed.
	 */
	if (filevp->v_flag & VFRLOCKS) {
		error = EACCES;
		goto out;
	}
	/*
	 * Establish a unique node id to represent the mount.
	 * If can't, return error.
	 */
	if ((nodeid = nmgetid()) == 0) {
		error = ENOMEM;
		goto out;
	}

	/*
	 * Initialize the namenode.
	 */
	mutex_init(&nodep->nm_lock, MUTEX_DEFAULT, "namevfs");
	nodep->nm_mountpt = mvp;
	nodep->nm_filep = fp;
	nodep->nm_filevp = filevp;
	/*
	 * Hold filevp and fp to be sure they are ours until nm_inactive
	 * or nm_unmount
	 */
	VN_HOLD(filevp);
	VFILE_REF_HOLD(fp);
	STRHEAD_LOCK(&filevp->v_stream, stp);
	if (stp) {
		int s;
		STR_LOCK_SPL(s);
		stp->sd_flag |= STRMOUNT;
		STR_UNLOCK_SPL(s);
	}
	STRHEAD_UNLOCK(stp);

	/*
	 * The attributes for the mounted file descriptor were initialized
	 * above by applying VOP_GETATTR to the mount point.  Some of
	 * the fields of the attributes structure will be overwritten
	 * by the attributes from the file descriptor.
	 */
	vattrp->va_type    = filevattr.va_type;
	vattrp->va_fsid    = namedev;
	vattrp->va_nodeid  = nodeid;
	vattrp->va_nlink   = 1;
	vattrp->va_size    = filevattr.va_size;
	vattrp->va_rdev    = filevattr.va_rdev;
	vattrp->va_blksize = filevattr.va_blksize;
	vattrp->va_nblocks = filevattr.va_nblocks;
	vattrp->va_vcode   = filevattr.va_vcode;

	/*
	 * Initialize the new vnode structure for the mounted file
	 * descriptor.
	 */
	newvp = vn_alloc(vfsp, filevp->v_type, filevp->v_rdev);
	bhv_desc_init(NMTOBHV(nodep), nodep, newvp, &nm_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(newvp), NMTOBHV(nodep));

	s = VN_LOCK(newvp);
	newvp->v_flag = filevp->v_flag | VROOT | VNOSWAP | VLOCK |
		VINACTIVE_TEARDOWN;
	VN_UNLOCK(newvp, s);
	newvp->v_stream = filevp->v_stream;

	/*
	 * Initialize the vfs structure.
	 */
	vfsp->vfs_flag |= VFS_CELLULAR;
	vfs_insertbhv(vfsp, &nodep->nm_vfsbhv, &nm_vfsops, nodep);
	vfsp->vfs_bsize = 1024;
	vfsp->vfs_fstype = namefstype;
	vfsp->vfs_fsid.val[0] = namedev;
	vfsp->vfs_fsid.val[1] = namefstype;
	vfsp->vfs_dev = namedev;
	vfsp->vfs_bcount = 0;

	mutex_lock(&nameallocmon, PZERO);
	nameinsert(nodep);
	mutex_unlock(&nameallocmon);
	return (0);
out:
	/*
	 * nodep's vnode has not yet been allocated, so no vn_free.
	 */
	kmem_free(nodep, sizeof *nodep);

	return (error);
}

/*
 * Unmount a file descriptor from a node in the file system.
 * If the user is not the owner of the mount point and is not super user,
 * the request is denied.
 * Otherwise, remove the namenode from the hash list.
 * If the mounted file descriptor was that of a stream and this
 * was the last mount of the stream, turn off the STRMOUNT flag.
 * If the rootvp is referenced other than through the mount,
 * nm_inactive will clean up.
 */
/* ARGSUSED */
int
nm_unmount(
	bhv_desc_t *bdp,
	int flags,
	struct cred *crp)
{
	struct namenode *nodep = VFS_BHVTONM(bdp);
	struct vnode *vp, *fvp;
	struct vattr mpattr;
	struct stdata *stp;
	int error;
	struct vfs *vfsp = bhvtovfs(bdp);


	vp = NMTOV(nodep);
	fvp = nodep->nm_filevp;

	mpattr.va_mask = AT_UID;
	ASSERT(vfsp->vfs_vnodecovered != NULLVP);
	VOP_GETATTR(vfsp->vfs_vnodecovered, &mpattr, 0, crp, error);
	if (error)
		return error;
	if (mpattr.va_uid != crp->cr_uid && !_CAP_CRABLE(crp, CAP_MOUNT_MGT))
		return (EPERM);

	mutex_lock(&nameallocmon, PZERO);
	nameremove(nodep);
	if ((namefind(fvp, NULLVP) == NULL) && fvp->v_stream) {
		STRHEAD_LOCK(&fvp->v_stream, stp);
		ASSERT(stp);
		stp->sd_flag &= ~STRMOUNT;
		STRHEAD_UNLOCK(stp);
	}
	mutex_unlock(&nameallocmon);

	error = VN_LOCK(vp);
	if (vp->v_count == 1) {
		VN_UNLOCK(vp, error);
		(void)vfile_close(nodep->nm_filep);
		VN_RELE(fvp);

		nmclearid(nodep);
		vn_bhv_remove(VN_BHV_HEAD(vp), NMTOBHV(nodep));
		vn_free(vp);
		mutex_destroy(&nodep->nm_lock);
		VFS_REMOVEBHV(vfsp, bdp);
		kmem_free(nodep, sizeof *nodep);
	} else {
		vp->v_count--;
		vp->v_flag &= ~VROOT;
		vp->v_vfsp = namevfsp;
		/* only the mount is going away */
		VFS_REMOVEBHV(vfsp, bdp);
		VN_UNLOCK(vp, error);
	}

	return (0);
}

/*
 * Create a reference to the root of a mounted file descriptor.
 * This routine is called from lookupname() in the event a path
 * is being searched that has a mounted file descriptor in it.
 */
int
nm_root(
	bhv_desc_t	*bdp,
	struct vnode **vpp)
{
	struct namenode *nodep = VFS_BHVTONM(bdp);
	struct vnode *vp = NMTOV(nodep);

	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

/*
 * Return in sp the status of this file system.
 */
/* ARGSUSED */
int
nm_statvfs(
	bhv_desc_t *bdp,
	register struct statvfs *sp,
	struct vnode *vp)
{
	struct vfs *vfsp;

	if (bdp == NULL)
		return (EINVAL);
	vfsp = bhvtovfs(bdp);
	bzero((caddr_t)sp, sizeof(*sp));
	sp->f_bsize	= 1024;
	sp->f_frsize	= 1024;
	sp->f_blocks	= 0;
	sp->f_bfree	= 0;
	sp->f_bavail	= 0;
	sp->f_files	= 0;
	sp->f_ffree	= 0;
	sp->f_favail	= 0;
	sp->f_fsid	= vfsp->vfs_dev;
	strcpy(sp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sp->f_flag	= vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax	= 0;
	return (0);
}

/*
 * Since this file system has no disk blocks of its own, apply
 * the VOP_FSYNC operation on the mounted file descriptor.
 * bdflush() doesn't really care about any of this since it will push
 * the underlying file via the file's vfs.
 */
int
nm_sync(
	bhv_desc_t *bdp,
	int flag,
	struct cred *crp)
{
	struct namenode *nodep;
	int error;
	int sync_flag;

	if (bdp == NULL || (flag & SYNC_BDFLUSH)) 
		return (0);

	nodep = VFS_BHVTONM(bdp);
	if (flag & SYNC_CLOSE) {
		error = nm_unmountall(nodep->nm_filevp, crp);
	}
	else {
		sync_flag = (flag & SYNC_WAIT) ? FSYNC_WAIT : 0;
		VOP_FSYNC(nodep->nm_filevp, sync_flag, crp,
			(off_t)0, (off_t)-1, error);
	}
	return error;
}

/*
 * Insert a namenode into the namealloc hash list.
 * namealloc is a doubly linked list that contains namenode
 * as links. Each link has a unique namenode with a unique
 * nm_mountvp field. The nm_filevp field of the namenode need not
 * be unique, since a file descriptor may be mounted to multiple
 * nodes at the same time.
 * This routine inserts a namenode link onto the front of the
 * linked list.
 * Called within nameallocmon to serialize find/insert with remove.
 */
void
nameinsert(struct namenode *nodep)
{
	nodep->nm_backp = NULL;
	nodep->nm_nextp = namealloc;
	namealloc = nodep;
	if (nodep->nm_nextp)
		nodep->nm_nextp->nm_backp = nodep;
}

/*
 * Search the doubly linked list, namealloc, for a namenode that
 * has a nm_filevp field of vp and a nm_mountpt of mnt.
 * If the namenode/link is found, return the address. Otherwise,
 * return NULL.
 * If mnt is NULL, return the first link with a nm_filevp of vp.
 * Called within nameallocmon to serialize find/insert with remove.
 */
struct namenode *
namefind(
	struct vnode *vp,
	struct vnode *mnt)
{
	register struct namenode *tnode;

	for (tnode = namealloc; tnode; tnode = tnode->nm_nextp)
		if (tnode->nm_filevp == vp &&
		    (!mnt || (mnt && tnode->nm_mountpt == mnt)))
			break;
	return (tnode);
}

/*
 * Remove a namenode from the hash table.
 * If nodep is the only node on the list, set namealloc to NULL.
 * Called within nameallocmon to serialize find/insert with remove.
 */
void
nameremove(struct namenode *nodep)
{
	register struct namenode *tnode;

	for (tnode = namealloc; tnode; tnode = tnode->nm_nextp)
		if (tnode == nodep) {
			if (nodep == namealloc)      /* delete first link */
				namealloc = nodep->nm_nextp;
			if (tnode->nm_nextp)
				tnode->nm_nextp->nm_backp = tnode->nm_backp;
			if (tnode->nm_backp)
				tnode->nm_backp->nm_nextp = tnode->nm_nextp;
			break;
		}
}

/*
 * Clear the bit in the bit map corresponding to the
 * nodeid in the namenode.
 */
void
nmclearid(struct namenode *nodep)
{
	int s;

	/*
	 * Safeguard against decrementing nmids when bit has
	 * already been cleared.
	 */
	s = mp_mutex_spinlock(&nmlock);
	if (testid(nodep->nm_vattr.va_nodeid))
		clearid(nodep->nm_vattr.va_nodeid);
	mp_mutex_spinunlock(&nmlock, s);
}

/*
 * Attempt to establish a unique node id. Start searching
 * the bit map where the previous search stopped. If a
 * free bit is located, set the bit and keep track of
 * it because it will become the new node id.
 */
u_short
nmgetid(void)
{
	int s;
	register u_short i;
	register u_short j;
	static u_short prev = 0;

	s = mp_mutex_spinlock(&nmlock);
	i = prev;

	for (j = NMMAXID; j--; ) {
		if (i++ >= (u_short)NMMAXID)
			i = 1;

		if (!testid(i)) {
			setid(i);
			prev = i;
			mp_mutex_spinunlock(&nmlock, s);
			return i;
		}
	}

	mp_mutex_spinunlock(&nmlock, s);
	cmn_err(CE_WARN, "nmgetid: could not establish a unique node id\n");
	return 0;
}

/*
 * Force the unmouting of a file descriptor from ALL of the nodes
 * that it was mounted to.
 *
 * At the present time, the only usage for this routine is in the
 * event one end of a pipe was mounted. At the time the unmounted
 * end gets closed down, the mounted end is forced to be unmounted.
 *
 * This routine searches the namealloc hash list for all namenodes
 * that have a nm_filevp field equal to vp. Each time one is found,
 * the dounmount() routine is called. This causes the nm_unmount()
 * routine to be called and thus, the file descriptor is unmounted
 * from the node.
 *
 * The reference count for vp is incremented to protect the vnode from
 * being released in the event the mount was the only thing keeping
 * the vnode active. If that is the case, the VOP_CLOSE operation is
 * applied to the vnode, prior to it being released.
 */
int
nm_unmountall(
	struct vnode *vp,
	struct cred *crp)
{
	register struct namenode *nodep;
	register struct vfs *vfsp;
	struct vnode *namenode_vp;
	struct stdata *stp;
	int error, realerr;

	realerr = 0;
	/*
	 * For each namenode that is associated with the file:
	 * If the v_vfsp field is not namevfsp, dounmount it.  Otherwise,
	 * it was created in nm_open() and will be released in time.
	 * We must remove nodep from the namealloc list before releasing
	 * nameallocmon, to avoid races to destroy nodep.
	 */
	STRHEAD_LOCK(&vp->v_stream, stp);
        if (stp)
		stp->sd_flag &= ~STRMOUNT;
	STRHEAD_UNLOCK(stp);
	
restart:
	mutex_lock(&nameallocmon, PZERO);
	for (nodep = namealloc; nodep; nodep = nodep->nm_nextp) {

		if ((nodep->nm_filevp == vp) &&
			(vfsp = NMTOV(nodep)->v_vfsp) != NULL &&
			(vfsp != namevfsp)) {

			if (nodep == namealloc)      /* delete first link */
				namealloc = nodep->nm_nextp;

			if (nodep->nm_nextp)
				nodep->nm_nextp->nm_backp = nodep->nm_backp;

			if (nodep->nm_backp)
				nodep->nm_backp->nm_nextp = nodep->nm_nextp;

			namenode_vp = NMTOV(nodep);

			/*
                         * Hold the vnode to keep it from going away since
                         * dounmount calls vfile_close on the file pointer stored
                         * in the vfsp.
			 */
			VN_HOLD(namenode_vp);

			mutex_unlock(&nameallocmon);

			/* base only? xxx huy */
			VFS_DOUNMOUNT(vfsp, 0, NULL, crp, error);
			if (error)
				realerr = error;
			else
			        vfs_deallocate(vfsp);
			VN_RELE(namenode_vp);
			goto restart;
		}
	}
	mutex_unlock(&nameallocmon);
	return realerr;
}

/*
 * Define the vfs operations vector.
 */
vfsops_t nm_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	nm_mount,
	fs_nosys,   /* rootinit */
	fs_nosys,   /* mntupdate */
	fs_dounmount,
        nm_unmount,
	nm_root,
	nm_statvfs,
	nm_sync,
	fs_nosys,    /* vget */
	fs_nosys,    /* mountroot */
	fs_noerr,    /* realvfsops */
	fs_import,   /* import */
	fs_quotactl  /* quotactl */
};

static vfsops_t dummyvfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	fs_nosys,    /* mount */
	fs_nosys,    /* rootinit */
	fs_nosys,    /* mntupdate */
	fs_nosys,    /* dounmount */
        fs_nosys,    /* unmount */
	fs_nosys,    /* root */
	nm_statvfs,
	nm_sync,
	fs_nosys,    /* vget */
	fs_nosys,    /* mountroot */
	fs_noerr,    /* realvfsops */
	fs_nosys,    /* import */
	fs_nosys     /* quotactl */
};
