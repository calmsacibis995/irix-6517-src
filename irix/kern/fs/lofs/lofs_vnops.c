/*
 * Copyright (c) 1987, 1991 by Sun Microsystems, Inc.
 */

#include <sys/vnode_private.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/pfdat.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/fs/lofs_node.h>
#include <sys/fs/lofs_info.h>
#include <sys/fs_subr.h>
#include <sys/flock.h>
#include <sys/attributes.h>
#include <string.h>
#include "sys/kmem.h"

extern vnode_t *makelonode(vnode_t *, struct loinfo *);
extern void freelonode(lnode_t *);
extern vnodeops_t autofs_vnodeops;

extern vnodeops_t lofs_vnodeops;
#ifdef	LODEBUG
int lodebug = 0;
#endif

/*
 * These are the vnode ops routines which implement the vnode interface to
 * the looped-back file system.  These routines just take their parameters,
 * and then calling the appropriate real vnode routine(s) to do the work.
 */

/* ARGSUSED */
static int
lofs_open(bhv_desc_t *bdp,
	vnode_t **vpp,
	mode_t flag,
	struct cred *cr)
{
	vnode_t *vp = *vpp;
	vnode_t *rvp;
	int error;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_open vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = rvp = realvp(bdp);
	VOP_OPEN(vp, vpp, flag, cr, error);

	/*
	 * XXX: does this compute???
	 * XXX: what about reference counts for realvp, etc.
	 */
	if (!error && rvp != vp)
		*vpp = rvp;

	return (error);
}

static int
lofs_close(bhv_desc_t *bdp,
	int flag,
	lastclose_t count,
	struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;
	
#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_close vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_CLOSE(vp, flag, count, cr, rv);
	return (rv);
}

static int
lofs_read(bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr,
	flid_t *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_read vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_READ(vp, uiop, ioflag, cr, fl, rv);
	return (rv);
}

static int
lofs_write(bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr,
	flid_t *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_write vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_WRITE(vp, uiop, ioflag, cr, fl, rv);
	return (rv);
}

static int
lofs_ioctl(bhv_desc_t *bdp,
	int cmd,
	void *arg,
	int flag,
	struct cred *cr,
	int *rvalp,
        struct vopbd *vbds)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_ioctl vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_IOCTL(vp, cmd, arg, flag, cr, rvalp, vbds, rv);
	return (rv);
}

static int
lofs_setfl(bhv_desc_t *bdp,
	int oflags,
	int nflags,
	cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

	vp = realvp(bdp);
	VOP_SETFL(vp, oflags, nflags, cr, rv);
	return (rv);
}

static int
lofs_getattr(bhv_desc_t *bdp,
	struct vattr *vap,
	int flags,
	struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);

	vnode_t *xvp;
	u_int rdev;
	int error;
	bhv_desc_t *vfs_bdp;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_getattr vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	/*
	 * If we are at the root of a mounted lofs filesystem
	 * and the underlying mount point is within the same
	 * filesystem, then return the attributes of the
	 * underlying mount point rather than the attributes
	 * of the mounted directory.  This prevents /bin/pwd
	 * and the C library function getcwd() from getting
	 * confused and returning failures.
	 */
	vfs_bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vp->v_vfsp), &lofs_vfsops);
	rdev = vfs_bhvtoli(vfs_bdp)->li_rdev;
	if ((vp->v_flag & VROOT) && vp->v_vfsp->vfs_vnodecovered) {
		xvp = vp;
		while ((xvp->v_flag & VROOT) && 
		       xvp->v_vfsp->vfs_vnodecovered) {
			xvp = xvp->v_vfsp->vfs_vnodecovered;
		}
		if (xvp->v_vfsp->vfs_dev == vp->v_vfsp->vfs_dev)
			vp = xvp;
		else
			vp = realvp(bdp);
	} else
		vp = realvp(bdp);
	VOP_GETATTR(vp, vap, flags, cr, error);
	if (!error) {
		/*
		 * report lofs rdev instead of real vp's, _getcwd() in libc
		 * depends on this to uniquely identify multiple lofs mounts 
		 * of the same filesystem (ie. lofs mount /tmp on /test/a and 
		 * lofs mount /tmp on /test/b). _getcwd() uses the rdev to 
		 * uniquely identify the file when searching the list of files
		 * in a directory, the mountid isn't unique in this case 
		 * because both are mounting the same filesystem
		 */
		vap->va_rdev = rdev;
#ifdef LODEBUG
	lofs_dprint(lodebug, 5, "lofs_getattr vp 0x%x rdev %d vap 0x%x lofsid %d\n", vp, vp->v_rdev, vap, vap->va_rdev);
#endif
	}
	return (error);
}

static int
lofs_setattr(bhv_desc_t *bdp,
	struct vattr *vap,
	int flags,
	struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_setattr vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_SETATTR(vp, vap, flags, cr, rv);
	return (rv);
}

static int
lofs_access(bhv_desc_t *bdp,
	int mode,
	struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_access vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_ACCESS(vp, mode, cr, rv);
	return (rv);
}

static int
lofs_fsync(bhv_desc_t *bdp,
	int syncflag,
	struct cred *cr,
	off_t start,
	off_t stop)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_fsync vp 0x%x realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_FSYNC(vp, syncflag, cr, start, stop, rv);
	return (rv);
}

/*ARGSUSED*/
static int
lofs_inactive(bhv_desc_t *bdp, struct cred *cr)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_inactive 0x%x, realvp 0x%x\n", vp, realvp(bdp));
#endif
	freelonode(bhvtol(bdp));

	/* the inactive_teardown flag must have been set at vn_alloc time */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	return VN_INACTIVE_NOCACHE;
}

static int
lofs_fid(bhv_desc_t *bdp, struct fid **fidp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_fid 0x%x, realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_FID(vp, fidp, rv);
	return (rv);
}

static int
lofs_fid2(bhv_desc_t *bdp, struct fid *fidp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug,4,"lofs_fid2 0x%x, realvp 0x%x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_FID2(vp, fidp, rv);
	return (rv);
}

static int
lofs_lookup(bhv_desc_t *dbdp,
	char *nm,
	vnode_t **vpp,
	struct pathname *pnp,
	int flags,
	vnode_t *rdir,
	struct cred *cr)
{
	vnode_t *vp = NULL, *tvp, *dvp = BHV_TO_VNODE(dbdp);
        int error;
        vnode_t *realdvp = realvp(dbdp);
        vnode_t *crossed_vp;
	bhv_desc_t *bdp;
	bhv_desc_t *vfs_dbdp = bhv_lookup_unlocked(VFS_BHVHEAD(dvp->v_vfsp),
						   &lofs_vfsops);
        struct loinfo *li = vfs_bhvtoli(vfs_dbdp);
        int     looping = 0;
        int     crossedvp = 0;
        int     doingdotdot = 0;
	int     iseof = 0;
	struct  uio uio;
	struct iovec iov;
#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "Enter lofs_lookup nm=%s path=%s dvp=0x%llx.\n",
		    nm, pnp->pn_path, dvp);
#endif

        if (nm[0] == '.') {
                if (nm [1] == '\0') {
                        *vpp = dvp;
                        VN_HOLD(dvp);
                        return (0);
                } else if ((nm[1] == '.') && (nm[2] == 0)) {
                        doingdotdot++;
                        /*
                         * Handle ".." out of mounted filesystem
                         */
                        while ((realdvp->v_flag & VROOT) &&
			       realdvp != rootdir) {
                                realdvp = realdvp->v_vfsp->vfs_vnodecovered;
                                ASSERT(realdvp != NULL);
                        }
                }
        }

        /*
         * traverse all of the intermediate shadow vnodes to get
         * to the real vnode
         */
        if (!doingdotdot)
                while (bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(realdvp),
						 &lofs_vnodeops))
                        realdvp = realvp(bdp);

        *vpp = NULL;    /* default(error) case */


        /*
         * Do the normal lookup
         */
	VOP_LOOKUP(realdvp, nm, &vp, pnp, flags, rdir, cr, error);
        if (error)
                goto out;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_lookup realdvp=0x%llx vp=0x%llx\n", 
		    realdvp, vp);
#endif

        /*
         * If this vnode is mounted on, then we
         * traverse to the vnode which is the root of
         * the mounted file system.
         */
        if (traverse(&vp)) {
		VN_RELE(vp);
                goto out;
	}

#if LODEBUG
	lofs_dprint(lodebug, 4, "lofs_lookup traverse vp=%llx\n", vp);	
#endif

        /*
         * We only make lonode for the real vnode when
         * real vnode is a directory.
         *
         * We can't do it on shared text without hacking on distpte
         */
        if (vp->v_type != VDIR) {
                *vpp = vp;
                goto out;
        }

	/* 
	 * For autofs vnodes which haven't yet been mounted.
	 * XXX: hack to get autofs sub-mounts of a lofs mount to 
	 *      work properly with the pwd command. Without this
	 *      hack if the user does a cd to an autofs mount 
	 *      without having done a lookup first of the autofs mount
	 *      then the autofs mount does not take place and pwd will
	 *      not work.
	 *
	 *  example:
	 *    if we come here because the user did something like
	 *
	 *        cd /hosts/localhost/hosts/remotehost
	 *
	 *    without having done a
	 *
	 *        cd /hosts/remotehost first
	 *
	 *    then this code attempts to do the autofs mount of 
	 *    /hosts/remotehost first for the user.
	 *
	 */
	if (vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &autofs_vnodeops)) {
		/* 
		 * This array should be a small size for performance
		 * reasons. because we are doing an expensive readdir
		 * operation and just throwing away the results.
		 */
		char data[16];

		/* 
		 * Create a uio struct.
		 */
		uio.uio_iovcnt = 1;
		uio.uio_iov = &iov;
		iov.iov_base = data;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_offset = 0;
		uio.uio_resid = iov.iov_len = sizeof(data);
		uio.uio_pmp = NULL;
		uio.uio_pio = 0;
		uio.uio_readiolog = 0;
		uio.uio_writeiolog = 0;
		uio.uio_pbuf = 0;
		
		VOP_RWLOCK(vp, VRWLOCK_READ);
		VOP_READDIR(vp, &uio, cr, &iseof, error);
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		if (vp->v_vfsmountedhere) {
			if (traverse(&vp)) {
				VN_RELE(vp);
				goto out;
			}
		}
	}
        /*
         * if the found vnode (vp) is not of type lofs
         * then we're just going to make a shadow of that
         * vp and get out.
         *
         * If the found vnode (vp) is of lofs type, and
         * we're not doing dotdot, we have to check if
         * we're looping or crossing lofs mount point
         */
        if (!doingdotdot && VFS_FOPS(vp->v_vfsp) == &lofs_vfsops) {
                /*
                 * if the parent vp has the lo_looping set
                 * then the child will have lo_looping as
                 * well and we're going to return the
                 * shadow of the vnode covered by vp
                 *
                 * Otherwise check if we're looping, i.e.
                 * vp equals the root vp of the lofs or
                 * vp equals the root vp of the lofs we have
                 * crossed.
                 */
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), 
					  &lofs_vnodeops);
                if (!(bhvtol(dbdp))->lo_looping) {
                        if (vp == li->li_rootvp ||
                            vp == (bhvtol(dbdp))->lo_crossedvp) {
                                looping++;
#if LODEBUG
				lofs_dprint(lodebug, 5,
					    "lofs_lookup looping detected "
					    "vp=%llx, rvp=%llx, cvp=%llx\n",
					    vp, li->li_rootvp,
					    (bhvtol(dbdp))->lo_crossedvp);
#endif
                                goto get_real_vnode;
                        } else if (bdp && (vfs_bhvtoli(bdp))->li_rootvp !=
				   li->li_rootvp) {
                                /*
                                 * just cross another lofs mount point.
                                 * remember the root vnode of the new lofs
                                 */
                                crossedvp++;
                                crossed_vp = (vfs_bhvtoli(bdp))->li_rootvp;
#if LODEBUG
				lofs_dprint(lodebug, 5,
					    "lofs_lookup crossed vp=%llx\n",
					    crossed_vp);
#endif
				bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(li->li_rootvp), 
							     &lofs_vnodeops);
                                if (vp == realvp(bdp) ||
                                    vp == li->li_rootvp) {
                                        looping++;
#if LODEBUG
					lofs_dprint(lodebug, 5,
						"lofs_lookup looping detected "
						"vp=%llx, rvp=%llx, realvp=%llx\n",
						vp, li->li_rootvp,
						realvp(bdp));
#endif
				    get_real_vnode:
					tvp = vp;
					while (bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp),
									    &lofs_vnodeops))
						vp = realvp(bdp);
					ASSERT(vp);
					VN_HOLD(vp);
					VN_RELE(tvp);
                                }
                        }
                }
        }
        *vpp = makelonode(vp, li);

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(*vpp),
				  &lofs_vnodeops);
	if (looping || ((bhvtol(dbdp))->lo_looping && !doingdotdot))
		(bhvtol(bdp))->lo_looping = 1;

        if (crossedvp)
                (bhvtol(bdp))->lo_crossedvp = crossed_vp;

        if ((bhvtol(dbdp))->lo_crossedvp)
                (bhvtol(bdp))->lo_crossedvp = (bhvtol(dbdp))->lo_crossedvp;

out:
#ifdef LODEBUG
        lo_dprint(4,
        "lo_lookup dvp %p realdvp %p nm '%s' newvp %p real vp %p error %d\n",
                dvp, realvp(dbdp), nm, *vpp, vp, error);
#endif

	return (error);
}

/*ARGSUSED*/
static int
lofs_create(bhv_desc_t *bdp,
	char *nm,
	struct vattr *va,
	int exclusive,
	int mode,
	vnode_t **vpp,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_create vp 0x%x realvp 0x%x\n", dvp, realvp(bdp));
#endif
	dvp = realvp(bdp);
	VOP_CREATE(dvp, nm, va, exclusive, mode, vpp, cr, rv);
	return (rv);
}

static int
lofs_remove(bhv_desc_t *bdp,
	char *nm,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_remove vp 0x%x realvp 0x%x\n", dvp, realvp(bdp));
#endif
	dvp = realvp(bdp);
	VOP_REMOVE(dvp, nm, cr, rv);
	return (rv);
}

/*
 * Find the first realvp() descending from the given vnode
 * that is not an lofs vnode.
 */
static vnode_t *
lofs_non_lofs_realvp(vnode_t *vp)
{
	bhv_desc_t *bdp;

	do {
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &lofs_vnodeops);
		if (bdp != NULL) {
			vp = realvp(bdp);
		} else {
			break;
		}
	} while (1);

	return (vp);
}

static int
lofs_link(bhv_desc_t *tdbdp,
	vnode_t *vp,
	char *tnm,
	struct cred *cr)
{
	vnode_t *tdvp = BHV_TO_VNODE(tdbdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_link vp 0x%x realvp 0x%x\n", tdvp, realvp(tdbdp));
#endif

	vp = lofs_non_lofs_realvp(vp);

	/*
	 * We know that the given vnode is an lofs vnode (since we
	 * got called), so skip it before searching for a non lofs
	 * vnode.
	 */
	tdvp = lofs_non_lofs_realvp(realvp(tdbdp));

	if (vp->v_vfsp != tdvp->v_vfsp)
		return (EXDEV);
	VOP_LINK(tdvp, vp, tnm, cr, rv);
	return (rv);
}

static int
lofs_rename(bhv_desc_t *odbdp,
	char *onm,
	vnode_t *ndvp,
	char *nnm,
	struct pathname *npnp,
	struct cred *cr)
{
	vnode_t *odvp = BHV_TO_VNODE(odbdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4,
		"lofs_rename vp 0x%x realvp 0x%x\n", odvp, realvp(odbdp));
#endif
	odvp = lofs_non_lofs_realvp(realvp(odbdp));
	ndvp = lofs_non_lofs_realvp(ndvp);

	if (odvp->v_vfsp != ndvp->v_vfsp)
		return (EXDEV);
	VOP_RENAME(odvp, onm, ndvp, nnm, npnp, cr, rv);
	return (rv);
}

static int
lofs_mkdir(bhv_desc_t *bdp,
	char *nm,
	register struct vattr *va,
	vnode_t **vpp,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	vnode_t *vp;
	int error;
	bhv_desc_t *vfs_dbdp;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_mkdir vp 0x%x realvp 0x%x\n", dvp, realvp(bdp));
#endif
	VOP_MKDIR(realvp(bdp), nm, va, &vp, cr, error);
	if (!error) {
		vfs_dbdp = bhv_lookup_unlocked(VFS_BHVHEAD(dvp->v_vfsp), 
							&lofs_vfsops);
		*vpp = makelonode(vp, vfs_bhvtoli(vfs_dbdp));
	}
	return (error);
}

static int
lofs_rmdir(bhv_desc_t *bdp,
	char *nm,
	vnode_t *cdir,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_rmdir vp 0x%x realvp 0x%x\n", dvp, realvp(bdp));
#endif
	dvp = realvp(bdp);
	VOP_RMDIR(dvp, nm, cdir, cr, rv);
	return (rv);
}

static int
lofs_symlink(bhv_desc_t *bdp,
	char *lnm,
	struct vattr *tva,
	char *tnm,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_symlink vp 0x%x realvp 0x%x\n", dvp, realvp(bdp));
#endif
	dvp = realvp(bdp);
	VOP_SYMLINK(dvp, lnm, tva, tnm, cr, rv);
	return (rv);
}

static int
lofs_readlink(bhv_desc_t *bdp,
	struct uio *uiop,
	struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

	vp = realvp(bdp);
	VOP_READLINK(vp, uiop, cr, rv);
	return (rv);
}

static int
lofs_readdir(bhv_desc_t *bdp,
	register struct uio *uiop,
	struct cred *cr,
	int *eofp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int rv;

#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_readdir vp %x realvp %x\n", vp, realvp(bdp));
#endif
	vp = realvp(bdp);
	VOP_READDIR(vp, uiop, cr, eofp, rv);
	return (rv);
}

static void
lofs_rwlock(bhv_desc_t *bdp, vrwlock_t write_lock)
{
	vnode_t *vp;

	vp = realvp(bdp);
	VOP_RWLOCK(vp, write_lock);
}

static void
lofs_rwunlock(bhv_desc_t *bdp, vrwlock_t write_lock)
{
	vnode_t *vp;

	vp = realvp(bdp);
	VOP_RWUNLOCK(vp, write_lock);
}

static int
lofs_seek(bhv_desc_t *bdp, off_t ooff, off_t *noffp)
{
	vnode_t *vp;
	int rv;

	vp = realvp(bdp);
	VOP_SEEK(vp, ooff, noffp, rv);
	return (rv);
}

static int
lofs_cmp(bhv_desc_t *bdp, vnode_t *vp2)
{
	vnode_t *vp1;
	int cmp;

	vp1 = lofs_non_lofs_realvp(realvp(bdp));
	vp2 = lofs_non_lofs_realvp(vp2);

	cmp = VN_CMP(vp1, vp2);
	return cmp;
}

static int
lofs_frlock(bhv_desc_t *bdp,
	int cmd,
	struct flock *bfp,
	int flag,
	off_t offset,
	vrwlock_t vrwlock,
	cred_t *cr)
{
	vnode_t *vp;
	int rv;

	vp = realvp(bdp);
	VOP_FRLOCK(vp, cmd, bfp, flag, offset, vrwlock, cr, rv);
	return (rv);
}

static int
lofs_realvp(bhv_desc_t *bdp, vnode_t **vpp)
{
	vnode_t *vp;
	int rv;

	vp = lofs_non_lofs_realvp(realvp(bdp));
#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_realvp 0x%x, vp 0x%x\n", bdp, vp);
#endif
	VOP_REALVP(vp, vpp, rv)
	if (rv)
		*vpp = vp;
	return (0);
}

static int
lofs_bmap(bhv_desc_t *bdp,
	off_t off,
	ssize_t count,
	int flag,
	struct cred *cr,
	struct bmapval *bmv,
	int *nbmv)
{
	vnode_t *vp;
	int error;

	vp = realvp(bdp);
	VOP_BMAP(vp, off, count, flag, cr, bmv, nbmv, error);
	return (error);
}

static void
lofs_strategy(bhv_desc_t *bdp,
	struct buf *buf)
{
	vnode_t *vp;

	vp = realvp(bdp);
	VOP_STRATEGY(vp, buf);
}

/* ARGSUSED */
static int
lofs_map(bhv_desc_t *bdp,
	off_t off,
	size_t len,
	mprot_t prot,
	u_int flags,
	struct cred *cr,
	vnode_t **nvp)
{
	vnode_t	*vp;
	int error;

	vp = realvp(bdp);
	VOP_MAP(vp, off, len, prot, flags, cr, &vp, error);
	if (error) 
		return error;

	/*
	 * Check if a different vnode should be returned.
	 */
	if (vp != BHV_TO_VNODE(bdp)) {
		VN_HOLD(vp);		/* must return a ref to the caller */
		*nvp = vp;
	}

	return 0;
}

static int
lofs_poll(bhv_desc_t *bdp,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	vnode_t *vp;
	int rv;

	vp = realvp(bdp);
	VOP_POLL(vp, events, anyyet, reventsp, phpp, genp, rv);
	return (rv);
}

static int
lofs_dump(bhv_desc_t *bdp,
	caddr_t addr,
	daddr_t bn,
	u_int count)
{
	vnode_t *vp;
	int rv;

	vp = realvp(bdp);
	VOP_DUMP(vp, addr, bn, count, rv);
	return (rv);
}

static int
lofs_pathconf(bhv_desc_t *bdp,
	int cmd,
	long *valp,
	struct cred *cr)
{
	vnode_t *vp;
	int rv;

	vp = realvp(bdp);
	VOP_PATHCONF(vp, cmd, valp, cr, rv);
	return (rv);
}

static void
lofs_vnode_change(
        bhv_desc_t	*bdp, 
        vchange_t 	cmd, 
        __psint_t 	val)
{
	/* 
	 * In addition to applying to the real vnode, also apply
	 * to the lofs vnode since it's the one visible to the vnode
	 * layer above.  
	 */
	VOP_VNODE_CHANGE(realvp(bdp), cmd, val);
	fs_vnode_change(bdp, cmd, val);
}
/*ARGSUSED*/
static int
lofs_attrget(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	vnode_t *xvp;
	int error;
	
#ifdef LODEBUG
	lofs_dprint(lodebug, 4, "lofs_attrget vp 0x%x realvp 0x%x\n", vp, 
		    realvp(bdp));
#endif
	/*
	 * If we are at the root of a mounted lofs filesystem
	 * and the underlying mount point is within the same
	 * filesystem, then return the attributes of the
	 * underlying mount point rather than the attributes
	 * of the mounted directory.  This prevents /bin/pwd
	 * and the C library function getcwd() from getting
	 * confused and returning failures.
	 */
	if ((vp->v_flag & VROOT) &&
	    (xvp = vp->v_vfsp->vfs_vnodecovered) != NULL &&
	    vp->v_vfsp->vfs_dev == xvp->v_vfsp->vfs_dev)
	  vp = xvp;
	else
	  vp = realvp(bdp);
	VOP_ATTR_GET(vp, name, value, valuelenp, flags, cred, error);

#ifdef LODEBUG
	lofs_dprint(lodebug, 5, "lofs_attrget vp 0x%x \n", vp);
#endif
	return (error);

}

/*ARGSUSED*/
static int
lofs_attrset(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int valuelenp,
	int flags,
	struct cred *cred)
{

        vnode_t *vp = realvp(bdp);
        int rv;

#ifdef LODEBUG
        lofs_dprint(lodebug, 4, "lofs_attrset vp 0x%x realvp 0x%x\n", vp,
		    realvp(bdp));
#endif
        VOP_ATTR_SET(vp, name, value, valuelenp, flags, cred, rv);
        return (rv);

}
/*ARGSUSED*/
static int			       			       
lofs_attrremove(bhv_desc_t *bdp,
	char *name, 
	int flags, 
	struct cred *cred)
{
        vnode_t *vp = realvp(bdp);
        int error;

#ifdef LODEBUG
        lofs_dprint(lodebug, 4, "lofs_attrremove vp 0x%x realvp 0x%x\n", vp,
		    realvp(bdp));
#endif
	VOP_ATTR_REMOVE(vp, name, flags, cred, error);
	return(error);
}

/*ARGSUSED*/
static int		       				       
lofs_attrlist(
	bhv_desc_t *bdp, 
	char *buffer, 
	int bufsize, 
	int flags,
	attrlist_cursor_kern_t *cursor, 
	struct cred *cred)
{
        vnode_t *vp = realvp(bdp);
	int  error=0;

#ifdef LODEBUG
        lofs_dprint(lodebug, 4, "lofs_attrlist vp 0x%x realvp 0x%x\n", vp,
		    realvp(bdp));
#endif
	VOP_ATTR_LIST(vp, buffer, bufsize, flags, cursor, cred, error);
	return(error);
}


/*
 * Loopback vnode operations vector.
 */
vnodeops_t lofs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	lofs_open,
	lofs_close,
	lofs_read,
	lofs_write,
	lofs_ioctl,
	lofs_setfl,
	lofs_getattr,
	lofs_setattr,
	lofs_access,
	lofs_lookup,
	lofs_create,
	lofs_remove,
	lofs_link,
	lofs_rename,
	lofs_mkdir,
	lofs_rmdir,
	lofs_readdir,
	lofs_symlink,
	lofs_readlink,
	lofs_fsync,
	lofs_inactive,
	lofs_fid,
	lofs_fid2,
	lofs_rwlock,
	lofs_rwunlock,
	lofs_seek,
	lofs_cmp,
	lofs_frlock,
	lofs_realvp,
	lofs_bmap,
	lofs_strategy,
	lofs_map,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	lofs_poll,
	lofs_dump,
	lofs_pathconf,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	lofs_attrget,
	lofs_attrset,
	lofs_attrremove,
	lofs_attrlist,
	fs_cover,
	(vop_link_removed_t)fs_noval,
	lofs_vnode_change,
        fs_tosspages,
	fs_flushinval_pages,
	fs_flush_pages,
	fs_invalfree_pages,
	fs_pages_sethole,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};
