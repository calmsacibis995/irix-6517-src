/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/namefs/namevno.c	1.19"*/
#ident	"$Revision: 1.66 $"

/*
 * This file defines the vnode operations for mounted file descriptors.
 * The routines in this file act as a layer between the NAMEFS file
 * system and SPECFS/FIFOFS.  With the exception of nm_open(), nm_setattr(),
 * nm_getattr() and nm_access(), the routines simply apply the VOP
 * operation to the vnode representing the file descriptor.  This switches
 * control to the underlying file system to which the file descriptor
 * belongs.
 */

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <fs/namefs/namenode.h>
#include <sys/capability.h>
#include <sys/flock.h>

static int nm_fsync(bhv_desc_t *, int, struct cred *, off_t, off_t);
static void nm_rwlock(bhv_desc_t *, vrwlock_t);
static void nm_rwunlock(bhv_desc_t *, vrwlock_t);

/*
 * Create a reference to the vnode representing the file descriptor.
 * Then, apply the VOP_OPEN operation to that vnode.
 *
 * The vnode for the file descriptor may be switched under you.
 * If it is, search the hash list for an nodep - nodep->nm_filevp
 * pair. If it exists, return that nodep to the user.
 * If it does not exist, create a new namenode to attach
 * to the nodep->nm_filevp then place the pair on the hash list.
 *
 * Newly created objects are like children/nodes in the mounted
 * file system, with the parent being the initial mount.
 */
int
nm_open(
	bhv_desc_t *bdp,
	struct vnode **vpp,
	mode_t flag,
	struct cred *crp)
{
	register struct namenode *nodep = BHVTONM(bdp);
	register int error;
	register struct namenode *newnamep;
	struct vnode *newvp;
	struct vnode *infilevp;
	struct vnode *outfilevp;
	int s;
	int gotitflag = 0;

	ASSERT(nodep->nm_filevp);
	/*
	 * If the vnode is switched under us, the corresponding
	 * VN_RELE for this VN_HOLD will be done by the file system
	 * performing the switch. Otherwise, the corresponding
	 * VN_RELE will be done by nm_close().
	 */
	VN_HOLD(nodep->nm_filevp);
	infilevp = outfilevp = nodep->nm_filevp;

	/* the naming is confusing here, but the 1st parameter is the open vp */
	VOP_OPEN(infilevp, &outfilevp, flag, crp, error);
	if (error) {
		VN_RELE(nodep->nm_filevp);
		return (error);
	}
	if (infilevp != outfilevp) {
		mutex_lock(&nameallocmon, PZERO);
		/*
		 * See if the new filevp (outfilevp) is already associated
		 * with the mount point. If it is, then it already has a
		 * namenode associated with it.
		 */
		if ((newnamep = namefind(outfilevp, nodep->nm_mountpt)) != NULL) {
			gotitflag = 1;
			goto gotit;
		}

		/*
		 * Create a new namenode.
		 */
		newnamep = kmem_zalloc(sizeof *newnamep, KM_SLEEP);

		/*
		 * Initialize the fields of the new vnode/namenode.
		 */
		ASSERT(BHV_OPS(bdp) == &nm_vnodeops);
		newvp = vn_alloc((*vpp)->v_vfsp, outfilevp->v_type,
				 outfilevp->v_rdev);
		bhv_desc_init(NMTOBHV(newnamep), newnamep, newvp, 
			      BHV_OPS(bdp));
		vn_bhv_insert_initial(VN_BHV_HEAD(newvp), NMTOBHV(newnamep));

		s = VN_LOCK(newvp);
		newvp->v_flag = ((*vpp)->v_flag & ~VROOT) | 	
			VNOSWAP | VLOCK | VINACTIVE_TEARDOWN;
		VN_UNLOCK(newvp, s);
		newvp->v_stream = outfilevp->v_stream;
		mutex_init(&newnamep->nm_lock, MUTEX_DEFAULT, "namevno");
		newnamep->nm_vattr = nodep->nm_vattr;
		newnamep->nm_vattr.va_type = outfilevp->v_type;
		newnamep->nm_vattr.va_nodeid = nmgetid();
		newnamep->nm_vattr.va_size = 0;
		newnamep->nm_vattr.va_rdev = outfilevp->v_rdev;
		newnamep->nm_filevp = outfilevp;
		newnamep->nm_filep = nodep->nm_filep;
		/*
		 * Hold the associated file structure and the new vnode to
		 * be sure they are ours until nm_inactive or nm_unmount.
		 */
		VFILE_REF_HOLD(newnamep->nm_filep);
		VN_HOLD(outfilevp);
		newnamep->nm_mountpt = nodep->nm_mountpt;
		newnamep->nm_backp = newnamep->nm_nextp = NULL;

		/*
		 * Insert the new namenode into the hash list.
		 */
		nameinsert(newnamep);
gotit:
		/*
		 * Release the above reference to the infilevp, the reference
		 * to the NAMEFS vnode, create a reference to the new vnode
		 * and return the new vnode to the user.
		 */
		if (gotitflag)
			VN_HOLD(NMTOV(newnamep));
		VN_RELE(*vpp);
		*vpp = NMTOV(newnamep);
		mutex_unlock(&nameallocmon);
	}
	return (0);
}

/*
 * Close a mounted file descriptor.
 * Apply the VOP_CLOSE operation to the vnode for the file descriptor.
 */
static int
nm_close(
	bhv_desc_t *bdp,
	int flag,
	lastclose_t lastclose,
	struct cred *crp)
{
	struct namenode *nodep = BHVTONM(bdp);
	int error;

	ASSERT(nodep->nm_filevp);
	VOP_CLOSE(nodep->nm_filevp, flag, lastclose, crp, error);
	if (lastclose == L_TRUE) {
		(void) nm_fsync(bdp, FSYNC_WAIT, crp, (off_t)0, (off_t)-1);
		VN_RELE(nodep->nm_filevp);
	}
	return (error);
}

/*
 * Read from a mounted file descriptor.
 */
static int
nm_read(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *crp,
	struct flid *fl)
{
	int rv;

	ASSERT (BHVTONM(bdp)->nm_filevp);
	VOP_READ (BHVTONM(bdp)->nm_filevp, uiop, ioflag, crp, fl, rv);
	return (rv);
}

/*
 * Apply the VOP_WRITE operation on the file descriptor's vnode.
 */
static int
nm_write(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *crp,
	struct flid *fl)
{
	int rv;

	ASSERT (BHVTONM(bdp)->nm_filevp);
	VOP_WRITE(BHVTONM(bdp)->nm_filevp, uiop, ioflag, crp, fl, rv);
	return (rv);
}

/*
 * Apply the VOP_IOCTL operation on the file descriptor's vnode.
 */
static int
nm_ioctl(
	register bhv_desc_t *bdp,
	int cmd,
	void *arg,
	int mode,
	struct cred *cr,
	int *rvalp,
        struct vopbd *vbds)
{
	int rv;

	ASSERT (BHVTONM(bdp)->nm_filevp);
	VOP_IOCTL(BHVTONM(bdp)->nm_filevp, cmd, arg, mode, cr, rvalp, vbds, rv);
	return (rv);
}

/*
 * Return in vap the attributes that are stored in the namenode
 * structure. In addition, overwrite the va_mask field with 0;
 */
/* ARGSUSED */
int
nm_getattr(
	bhv_desc_t *bdp,
	struct vattr *vap,
	int flags,
	struct cred *crp)
{
	register struct namenode *nodep = BHVTONM(bdp);
	struct vattr va;
	register int error;

	va.va_mask = AT_SIZE;
	VOP_GETATTR(nodep->nm_filevp, &va, flags, crp, error);
	if (error)
		return (error);

	mutex_lock(&nodep->nm_lock, PINOD);
	*vap = nodep->nm_vattr;
	vap->va_mask = 0;
	vap->va_size = va.va_size;
	mutex_unlock(&nodep->nm_lock);
	return (0);
}

/*
 * Set the attributes of the namenode from the attributes in vap.
 */
/* ARGSUSED */
int
nm_setattr(
	bhv_desc_t *bdp,
	register struct vattr *vap,
	int flags,
	struct cred *crp)
{
	register struct namenode *nodep = BHVTONM(bdp);
	register struct vattr *nmvap = &nodep->nm_vattr;
	register long mask = vap->va_mask;
	int error = 0;

	/*
	 * Cannot set these attributes.
	 */
	if (mask & (AT_NOSET|AT_SIZE))
		return (EINVAL);

	nm_rwlock(bdp, VRWLOCK_WRITE);
	mutex_lock(&nodep->nm_lock, PINOD);

	/*
	 * Change ownership/group/time/access mode of mounted file
	 * descriptor.  Must be owner or privileged.
	 */
	if (crp->cr_uid != nmvap->va_uid && !_CAP_CRABLE(crp, CAP_FOWNER)) {
		error = EPERM;
		goto out;
	}
	/*
	 * If request to change mode, copy new
	 * mode into existing attribute structure.
	 */
	if (mask & AT_MODE) {
		nmvap->va_mode = vap->va_mode & ~VSVTX;
		if (!groupmember(nmvap->va_gid,crp) && !_CAP_CRABLE(crp, CAP_FOWNER)) {
			nmvap->va_mode &= ~VSGID;
		}
	}
	/*
	 * If request was to change user or group, turn off suid and sgid
	 * bits.
	 * If the system was configured with the "restricted_chown" option,
	 * the owner is not permitted to give away the file, and can change
	 * the group id only to a group of which he or she is a member.
	 */
	if (mask & (AT_UID|AT_GID)) {
		int checksu = 0;

		if (restricted_chown) {
			if (((mask & AT_UID) && vap->va_uid != nmvap->va_uid)
			  || ((mask & AT_GID)
			    && !groupmember(vap->va_gid, crp)))
				checksu = 1;
		} else if (crp->cr_uid != nmvap->va_uid)
			checksu = 1;

		if (checksu && !_CAP_CRABLE(crp, CAP_FOWNER)) {
			error = EPERM;
			goto out;
		}
		if ((nmvap->va_mode & (VSUID|VSGID)) && !_CAP_CRABLE(crp, CAP_FOWNER)){
			nmvap->va_mode &= ~(VSUID|VSGID);
		}
		if (mask & AT_UID)
			nmvap->va_uid = vap->va_uid;
		if (mask & AT_GID)
			nmvap->va_gid = vap->va_gid;
	}
	/*
	 * If request is to modify times, make sure user has write
	 * permissions on the file.
	 */
	if (mask & (AT_ATIME|AT_MTIME)) {
		if (!(nmvap->va_mode & VWRITE) && !_CAP_CRABLE(crp, CAP_DAC_OVERRIDE)){
			error = EACCES;
			goto out;
		}
		if (mask & AT_ATIME)
			nmvap->va_atime = vap->va_atime;
		if (mask & AT_MTIME) {
			nmvap->va_mtime = vap->va_mtime;
			nanotime_syscall(&nmvap->va_ctime);
		}
	}
out:
	mutex_unlock(&nodep->nm_lock);
	nm_rwunlock(bdp, VRWLOCK_WRITE);
	return (error);
}

/*
 * Check mode permission on the namenode.  If the namenode bits deny the
 * privilege is checked.  In addition an access check is performed
 * on the mounted file. Finally, if the file was opened without the
 * requested access at mount time, deny the access.
 */
/* ARGSUSED */
static int
nm_access(
	bhv_desc_t *bdp,
	int mode,
	struct cred *crp)
{
	register struct namenode *nodep = BHVTONM(bdp);
	int error, fmode;

	VOP_ACCESS(nodep->nm_filevp, mode, crp, error);
	if (error)
		return (error);
	/*
	 * Last stand.  Regardless of the requestor's credentials, don't
	 * grant a permission that wasn't granted at the time the mounted
	 * file was originally opened.
	 */
	fmode = nodep->nm_filep->vf_flag;
	if (((mode & VWRITE) && (fmode & FWRITE) == 0)
	  || ((mode & VREAD) && (fmode & FREAD) == 0))
		return (EACCES);
	return (error);
}

/*
 * Dummy op so that creats and opens with O_CREAT
 * of mounted streams will work.
 */
/*ARGSUSED*/
static int
nm_create(
	bhv_desc_t *bdp,
	char *name,
	struct vattr *vap,
	int flags,
	int mode,
	struct vnode **vpp,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	register int error;

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}

	if (*name == '\0') {
		/*
		 * Null component name refers to the root.
		 */
		if ((error = nm_access(bdp, mode, cr)) == 0) {
			VN_HOLD(dvp);
			*vpp = dvp;
		}
	} else {
		error = ENOSYS;
	}
	return (error);
}

/*
 * Links are not allowed on mounted file descriptors.
 */
/*ARGSUSED*/
int
nm_link(
	register bhv_desc_t *bdp,
	struct vnode *vp,
	char *tnm,
	struct cred *crp)

{
	return (EXDEV);
}

/*
 * Apply the VOP_FSYNC operation on the file descriptor's vnode.
 */
static int
nm_fsync(
	bhv_desc_t *bdp,
	int flag,
	struct cred *crp,
	off_t start,
	off_t stop)
{
	int error;

	ASSERT (BHVTONM(bdp)->nm_filevp);
	VOP_FSYNC (BHVTONM(bdp)->nm_filevp, flag, crp, start, stop, error);
	return (error);
}

/*
 * Inactivate a vnode/namenode by...
 * clearing its unique node id, removing it from the hash list
 * and freeing the memory allocated for it.
 */
/* ARGSUSED */
int
nm_inactive(
	bhv_desc_t *bdp,
	struct cred *crp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register struct namenode *nodep = BHVTONM(bdp);

	/*
	 * Note:  Maintain this ordering since vfile_close and VN_RELE may sleep.
	 */
	mutex_lock(&nodep->nm_lock, PINOD);
	mutex_lock(&nameallocmon, PZERO);

        /* remove name from list */
        nameremove(nodep);

	mutex_unlock(&nameallocmon);
	mutex_unlock(&nodep->nm_lock);

        nmclearid(nodep);
        (void)vfile_close(nodep->nm_filep);
	VN_RELE(nodep->nm_filevp);
	
	mutex_destroy(&nodep->nm_lock);

	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
        kmem_free(nodep, sizeof *nodep);

	/* the inactive_teardown flag must have been set at vn_alloc time */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	return VN_INACTIVE_NOCACHE;
}

/*
 * Apply the VOP_FID operation on the file descriptor's vnode.
 */
int
nm_fid(
	bhv_desc_t *bdp,
	struct fid **fidnodep)
{
	int error;

	ASSERT (BHVTONM(bdp)->nm_filevp);
	VOP_FID(BHVTONM(bdp)->nm_filevp, fidnodep, error);
	return (error);
}

/*
 * Apply the VOP_FID2 operation.
 */
int
nm_fid2(
	bhv_desc_t *bdp,
	struct fid *fidnodep)
{
	int error;

	ASSERT (BHVTONM(bdp)->nm_filevp);
	VOP_FID2(BHVTONM(bdp)->nm_filevp, fidnodep, error);
	return (error);
}

/*
 * Lock the namenode associated with vp.
 */
/* ARGSUSED */
static void
nm_rwlock(
	bhv_desc_t *bdp,
	vrwlock_t write_flag)
{
	register struct namenode *nodep = BHVTONM(bdp);

	VOP_RWLOCK(nodep->nm_filevp, write_flag);
}

/*
 * Unlock the namenode associated with vp.
 */
/* ARGSUSED */
static void
nm_rwunlock(
	bhv_desc_t *bdp,
	vrwlock_t write_flag)
{
	register struct namenode *nodep = BHVTONM(bdp);

	VOP_RWUNLOCK(nodep->nm_filevp, write_flag);
}

/*
 * Apply the VOP_SEEK operation on the file descriptor's vnode.
 */
int
nm_seek(
	bhv_desc_t *bdp,
	off_t ooff,
	off_t *noffp)
{
	int error;

	ASSERT (BHVTONM(bdp)->nm_filevp);
	VOP_SEEK(BHVTONM(bdp)->nm_filevp, ooff, noffp, error);
	return (error);
}

/*
 * nm_frlock
 */
int
nm_frlock(
	bhv_desc_t	*bdp,
	int		cmd,
	flock_t		*flockp,
	int		flag,
	off_t		offset,
	vrwlock_t	vrwlock,
	cred_t		*credp)
{
	int		dolock, error;

	/*
	 * Note that it seems a little dubious that the lock is being
	 * attached to the namefs vnode and not the one below it,
	 * because if locks were also applied directly to the one
	 * below (via its file system) then we'd potentially have
	 * inconsistent locks despite the fact that there's really
	 * just one "object".  It's been this way for a lot of years
	 * however, with apparently no problems for users...
	 */

	dolock = (vrwlock == VRWLOCK_NONE);
	if (dolock) {
		nm_rwlock(bdp, VRWLOCK_WRITE);
		vrwlock = VRWLOCK_WRITE;
	} 

	error = fs_frlock(bdp, cmd, flockp, flag, offset, vrwlock, credp);
	if (dolock)
		nm_rwunlock(bdp, VRWLOCK_WRITE);

	return error;
}

/*
 * Return the vnode representing the file descriptor in vpp.
 */
int
nm_realvp(
	register bhv_desc_t *bdp,
	register struct vnode **vpp)
{
	vnode_t *vp;
	register struct vnode *fvp = BHVTONM(bdp)->nm_filevp;
	struct vnode *rvp;
	int error;

	ASSERT(fvp);
	vp = fvp;
	VOP_REALVP(vp, &rvp, error);
	if (!error)
		vp = rvp;
	*vpp = vp;
	return (0);
}

/*
 * Apply VOP_POLL to the vnode representing the mounted file descriptor.
 */
static int
nm_poll(bhv_desc_t *bdp,
	register short events,
	int anyyet,
	register short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	int error;

	ASSERT (BHVTONM(bdp)->nm_filevp);
	VOP_POLL(BHVTONM(bdp)->nm_filevp, events, anyyet, reventsp, phpp, genp,
		 error);
	return (error);
}

static void
nm_vnode_change(
        bhv_desc_t	*bdp, 
        vchange_t 	cmd, 
        __psint_t 	val)
{
	/* 
	 * In addition to applying to the underlying vnode, also apply
	 * to the namefs vnode since it's the one visible to the vnode
	 * layer above.  
	 */
	VOP_VNODE_CHANGE(BHVTONM(bdp)->nm_filevp, cmd, val);
	fs_vnode_change(bdp, cmd, val);
}

vnodeops_t nm_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	nm_open,
	nm_close,
	nm_read,
	nm_write,
	nm_ioctl,
	fs_setfl,
	nm_getattr,
	nm_setattr,
	nm_access,
	(vop_lookup_t)fs_nosys,
	nm_create,
	(vop_remove_t)fs_nosys,
	nm_link,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	(vop_readdir_t)fs_nosys,
	(vop_symlink_t)fs_nosys,
	(vop_readlink_t)fs_nosys,
	nm_fsync,
	nm_inactive,
	nm_fid,
	nm_fid2,
	nm_rwlock,
	nm_rwunlock,
	nm_seek,
	fs_cmp,
	nm_frlock,
	nm_realvp,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_noval,
	(vop_map_t)fs_nodev,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	nm_poll,
	(vop_dump_t)fs_nosys,
	fs_pathconf,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	(vop_attr_get_t)fs_nosys,
	(vop_attr_set_t)fs_nosys,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	fs_cover,	
	(vop_link_removed_t)fs_noval,
	nm_vnode_change,
	(vop_ptossvp_t)fs_noval,
	(vop_pflushinvalvp_t)fs_noval,
	(vop_pflushvp_t)fs_nosys,
	(vop_pinvalfree_t)fs_noval,
	(vop_sethole_t)fs_noval,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};
