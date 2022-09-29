/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/pathname.h>
#include <ksys/vfile.h>
#include <sys/dirent.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include <sys/time.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/fs/autofs.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/mac_label.h>
#include <sys/sat.h>
#include <sys/attributes.h>

#ifdef	AUTODEBUG
int autodebug = 0;
#endif

/*
 *  Vnode ops for autofs
 */
static int auto_open(bhv_desc_t *, vnode_t **, mode_t, struct cred *);
static int auto_close(bhv_desc_t *, int, lastclose_t, struct cred *);
static int auto_getattr(bhv_desc_t *, struct vattr *, int, struct cred *);
static int auto_access(bhv_desc_t *, int, struct cred *);
static int auto_lookup(bhv_desc_t *, char *, vnode_t **,
	struct pathname *, int, vnode_t *, struct cred *);
static int auto_create(bhv_desc_t *, char *, struct vattr *, int,
	int, vnode_t **, struct cred *);
static int auto_remove(bhv_desc_t *, char *, struct cred *);
static int auto_rename(bhv_desc_t *, char *, vnode_t *, char *, 
	struct pathname *npnp, struct cred *);
static int auto_mkdir(bhv_desc_t *, char *, struct vattr *, vnode_t **, struct cred *);
static int auto_rmdir(bhv_desc_t *, char *, vnode_t *, struct cred *);
static int auto_readdir(bhv_desc_t *, struct uio *, struct cred *, int *);
static int auto_symlink(bhv_desc_t *, char *, struct vattr *, char *, struct cred *);
static int auto_fsync(bhv_desc_t *, int, struct cred *,
			off_t start, off_t stop);
static int auto_inactive(bhv_desc_t *, struct cred *);
static void auto_rwlock(bhv_desc_t *, vrwlock_t);
static void auto_rwunlock(bhv_desc_t *, vrwlock_t);

static  int auto_attrget(bhv_desc_t *, char *, char *, int *, int, struct cred *);
static  int auto_attrset(bhv_desc_t *, char *, char *, int , int, struct cred *);
static  int auto_attrlist(bhv_desc_t *, char *, int, int, struct attrlist_cursor_kern *, struct cred *);
#define	NEED_MOUNT(bdp)	(bhvtoan(bdp)->an_mntflags & MF_MNTPNT)
#define	SPECIAL_PATH(pnp) ((pnp)->pn_path[pn_pathleft(pnp) - 1] == ' ')

/* ARGSUSED */
static int
auto_open(
	bhv_desc_t *bdp,
	vnode_t **vpp,
	mode_t flag,
	struct cred *cr)
{
	return (0);
}

/* ARGSUSED */
static int
auto_close(
	bhv_desc_t *bdp,
	int flag,
	lastclose_t count,
	struct cred *cr)
{
	return (0);
}

/* ARGSUSED */
static int
auto_getattr(
	bhv_desc_t *bdp,
	struct vattr *vap,
	int flags,
	struct cred *cred)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	autonode_t *ap = bhvtoan(bdp);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_getattr vp 0x%lx\n", vp);
#endif

	ASSERT(vp->v_type == VDIR);
	vap->va_uid	= 0;
	vap->va_gid	= 0;
	vap->va_nlink	= 2;
	vap->va_nodeid	= ap->an_nodeid;
	vap->va_size	= ap->an_size;
	vap->va_atime.tv_sec = time;
	vap->va_atime.tv_nsec = 0;
	vap->va_mtime	= vap->va_ctime = vap->va_atime;
	vap->va_type	= vp->v_type;
	vap->va_mode	= ap->an_mode;
	vap->va_fsid	= vp->v_vfsp->vfs_dev;
	vap->va_rdev	= 0;
	vap->va_blksize	= MAXBSIZE;
	vap->va_nblocks	= btod(vap->va_size);
	vap->va_vcode	= 0;
	vap->va_xflags	= 0;
	vap->va_extsize = 0;
	vap->va_nextents = 0;
	vap->va_anextents = 0;
	vap->va_projid	= 0;
	vap->va_gencount = 0;

	return (0);
}


/* ARGSUSED */
static int
auto_access(
	bhv_desc_t *bdp,
	int mode,
	struct cred *cred)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	autonode_t *ap;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_access vp 0x%lx\n", vp);
#endif
	ap = bhvtoan(bdp);

	if (cred->cr_uid == 0)
		return (0);

	/*
	 * Not under a mutex because it really does
	 * not matter.  The uid fetch is atomic and the first one
	 * to get there wins anyway. The gid should not change
	 */
	if (cred->cr_uid != ap->an_uid) {
		mode >>= 3;
		if (groupmember(ap->an_gid, cred) == 0)
			mode >>= 3;
	}
	if ((ap->an_mode & mode) == mode) {
		return (0);
	}

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_access returning EACCES\n");
#endif
	return (EACCES);

}

#define	MOUNTED_ON(bdp) (bhvtoan(bdp)->an_mntflags & MF_MOUNTED)

static int
auto_lookup(
	bhv_desc_t *dbdp,
	char *nm,
	struct vnode **vpp,
	struct pathname *pnp,
	int flags,
	struct vnode *rdir,
	struct cred *cred)
{
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	struct vnode *newvp;
	int error;
	char rnm[MAXNAMLEN + 1];
	autonode_t *dap, *ap = NULL;
	int mount_action = 0;
	int special_path = 0; /* has a space at end => dont block */
	bhv_desc_t *bdp;
	int imadeit = 0;
	int old_nodeid = 0;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_lookup dvp 0x%lx nm '%s'\n", dvp, nm);
if (pnp)
	auto_dprint(autodebug, 3, "  path '%s'\n", pnp->pn_buf);
#endif
	if (pnp->pn_buf[strlen(pnp->pn_buf) - 1] == ' ')
		special_path = 1;

	dap = bhvtoan(dbdp);
	dap->an_ref_time = time;

	/*
	 * A space is appended to the pathname only in case if the
	 * the lookup originated from the daemon. Even in that
	 * case, the space is appended for indirect mounts or
	 * direct mounts with offsets.
	 *
	 * /usr/bugs	/bin	foo:/bin
	 * 		/etc	bar:/etc
	 *
	 * Therefore special path is *not* set in case of any request
	 * not originating in the daemon  -or-
	 * any kind of direct mount request (without any offset)  eg.
	 *
	 * /usr/local	/	foo:/root
	 *		/bin	bar:/bin
	 *
	 * NEED_MOUNT is true in case of all direct mount points.
	 * (!special_path && NEED_MOUNT) return true in case
	 * of direct mounts (both with and without offset).
	 * Since we want only no-offset direct mounts to attempt
	 * mount we put 2 additional conditions. dap->an_dirents == NULL
	 * ensures that.
	 */
	mutex_enter(&dap->an_lock);
	if ((!special_path) && NEED_MOUNT(dbdp)) {
		if ((dvp->v_vfsmountedhere == NULL) &&
		    (dap->an_dirents == NULL)) {
			/*
			 * the following synchronizes it with the do_mount in
			 * auto_readdir. If two threads are doing lookup
			 * at the same time they are serialized inside
			 * do_mount
			 */
			dap->an_mntflags |= MF_WAITING_MOUNT;
			mount_action = 1;
		} else if (MOUNTED_ON(dbdp) && (dvp->v_vfsmountedhere != NULL)) {
			/*
			 * This case happens when a process has done
			 * a cd to a direct (no offset) mntpnt then
			 * it does an ls which causes the mount to
			 * happen but the current directory of the proces
			 * is still the underlying vnode. So when that
			 * process does a cd to a directory on the
			 * mounted filesystem, the request has to be
			 * redirected.
			 */
			VFS_ROOT(dvp->v_vfsmountedhere, &newvp, error);
			if (!error) {
				VOP_LOOKUP(newvp, nm, vpp, pnp, flags,
					rdir, cred, error);
#ifdef AUTODEBUG
				auto_dprint(autodebug, 10, 
		    		    "auto_lookup RELE 0x%lx, v_ct: %d\n", 
		    		    newvp, newvp->v_count);
#endif
				VN_RELE(newvp);
			}
			mutex_exit(&dap->an_lock);
			return (error);
		}
	}
	mutex_exit(&dap->an_lock);

	if (mount_action == 1) {
		mutex_enter(&dap->an_lock);
		error = do_mount(dbdp, nm, cred);
		mutex_exit(&dap->an_lock);
		if (error)
			return (error);
		if (dvp->v_vfsmountedhere == NULL) {
			/*
			 * can be null in case of direct mount
			 * with offset in that case proceed like
			 * an offset mount otherwise if it is a
			 * simple direct mount return ENOENT
			 */
			if (dap->an_dirents == NULL)
				return (ENOENT);
			else
				goto offset_mount;
		}
		VFS_ROOT(dvp->v_vfsmountedhere, &newvp, error);
#ifdef AUTODEBUG
		auto_dprint(autodebug, 10, "auto_lookup ROOT 0x%lx\n", newvp);
#endif
		if (!error) {
			VOP_LOOKUP(newvp, nm, vpp, pnp, flags,
					    rdir, cred, error);
#ifdef AUTODEBUG
			auto_dprint(autodebug, 10, 
				    "auto_lookup RELE 0x%lx ref %d\n", 
				    newvp, newvp->v_count);
#endif
			VN_RELE(newvp);
		}
		return (error);
	}

offset_mount:
	/*
	 * In case of an direct offset mount if unmounting
	 * is going on, block it right here if the lookup is
	 * not daemon initiated.
	 * It is necessary to block at this point i.e.
	 * at the top of the heitarchy for
	 * direct offset mounts, because we do not
	 * even to attempt an autodir_lookup, which
	 * is OK for indirect mounts. That is why there
	 * is little duplication of code here and below.
	 */
	mutex_enter(&dap->an_lock);
	if ((dap->an_mntflags & MF_MNTPNT) &&
	    (dap->an_mntflags & MF_UNMOUNTING) &&
	    (!special_path)) {
		dap->an_waiters++;
		dap->an_mntflags |= MF_WAITING_UMOUNT;
		while (dap->an_mntflags & MF_UNMOUNTING) {
			if (sv_wait_sig(&dap->an_cv_umount, PZERO,
					&dap->an_lock, 0) < 0) {
				mutex_enter(&dap->an_lock);
				if (--dap->an_waiters == 0)
					dap->an_mntflags &= ~MF_WAITING_UMOUNT;
				mutex_exit(&dap->an_lock);
				return (EINTR);
			}
			mutex_enter(&dap->an_lock);
		}
		if (--dap->an_waiters == 0)
			dap->an_mntflags &= ~MF_WAITING_UMOUNT;
		if (dap->an_mntflags & MF_DONTMOUNT)
			dap->an_mntflags &= ~MF_DONTMOUNT;
	}
	mutex_exit(&dap->an_lock);

	VOP_ACCESS(dvp, VEXEC, cred, error);
	if (error)
		return (error);

	if (strcmp(nm, ".") == 0) {
		VN_HOLD(dvp);
#ifdef AUTODEBUG
		auto_dprint(autodebug, 10, 
			    "auto_lookup HOLD 0x%lx, v_ct %d\n", 
			    dvp, dvp->v_count);
#endif
		*vpp = dvp;
		return (0);
	} else if (strcmp(nm, "..") == 0) {
		autonode_t *pdap;

		pdap = bhvtoan(dbdp)->an_parent;
		ASSERT(pdap != NULL);
		*vpp = antovn(pdap);
		VN_HOLD(*vpp);
#ifdef AUTODEBUG
		auto_dprint(autodebug, 10, 
			    "auto_lookup HOLD 0x%lx, v_ct now %d\n", 
			    *vpp, (*vpp)->v_count);
#endif
		return (0);
	}

	if (nm[strlen(nm) - 1] == ' ') {
		bcopy(nm, rnm, strlen(nm) - 1);
		rnm[strlen(nm) - 1] = '\0';
	}
	else
		bcopy(nm, rnm, strlen(nm) + 1);

lookup_retry:
	/* 
	 * It is important that these initialization statements are after the lookup_retry.
	 */
	ap = NULL;
	*vpp = NULL;
	imadeit = old_nodeid = 0;
	rw_enter(&dap->an_rwlock, RW_READER);
	error = autodir_lookup(dbdp, rnm, &bdp, cred);
	if (!error) {
		*vpp = BHV_TO_VNODE(bdp);
		ap = bhvtoan(bdp);
	}
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, 
		    "auto_lookup, dir_lookup dvp 0x%lx rnm %s, vp 0x%lx, error %d\n", 
		    dvp, rnm, *vpp, error);
	if (!error)
		auto_dprint(autodebug, 10, "    vp ref %d\n", (*vpp)->v_count); 
#endif
	rw_exit(&dap->an_rwlock);
	if (!special_path) {
		if (error == ENOENT)
			mutex_enter(&dap->an_lock);
		else if ((error == 0) && 
			 (dap->an_mntflags & MF_INPROG)) {
			mutex_enter(&dap->an_lock);
			error = ENOENT;
		}
	}

	if ((error == 0) && (!special_path)) {
		mutex_enter(&ap->an_lock);
		if (ap->an_error) {
			/*
			 * We can get here by branching to lookup_retry after
			 * we had to sleep waiting for a mount locked on the
			 * same autonode we were to finish, and obviously it
			 * failed.
			 * A previous mount on this node failed, automountd
			 * is in the process of removing this autonode,
			 * return the same error here.
			 */
			error = ap->an_error;
			mutex_exit(&ap->an_lock);

			mutex_enter(&dap->an_lock);
#ifdef AUTODEBUG
			auto_dprint(autodebug, 10, 
				    "auto_lookup RELE 0x%lx ref %d\n", 
				    *vpp, (*vpp)->v_count);
#endif
			VN_RELE(*vpp);
			mutex_exit(&dap->an_lock);

			*vpp = NULL;
			return (error);
		}
		if (ap->an_mntflags & MF_UNMOUNTING) {
			ap->an_waiters++;
			ap->an_mntflags |= MF_WAITING_UMOUNT;
			while (ap->an_mntflags & MF_UNMOUNTING) {
				if (sv_wait_sig(&ap->an_cv_umount, PZERO,
						&ap->an_lock, 0) < 0) {
					mutex_enter(&ap->an_lock);
					if (--ap->an_waiters == 0)
						ap->an_mntflags &=
						    ~MF_WAITING_UMOUNT;
					mutex_exit(&ap->an_lock);
					mutex_enter(&dap->an_lock);
#ifdef AUTODEBUG
					auto_dprint(autodebug, 10, 
					    "auto_lookup EINTR RELE 0x%lx ref %d\n", 
					    *vpp, (*vpp)->v_count);
#endif
					VN_RELE(*vpp);
					mutex_exit(&dap->an_lock);

					*vpp = NULL;
					return (EINTR);
				}
				mutex_enter(&ap->an_lock);
			}
			if (--ap->an_waiters == 0)
				ap->an_mntflags &= ~MF_WAITING_UMOUNT;
			if (ap->an_mntflags & MF_DONTMOUNT) {
				/*
				 * It means the unmount was not
				 * successful so the filesystem
				 * should still be mounted so continue life.
				 */
				ap->an_mntflags &= ~MF_DONTMOUNT;
			} else {
				/*
				 * Unmount successful but autofs dirs
				 * are still hanging around, so we need
				 * another do_mount. We just simulate
				 * a ENOENT so that it does the do_mount
				 * for us below.
				 */
				mutex_enter(&dap->an_lock);
				error = ENOENT;
			}
		} else if (((*vpp)->v_vfsmountedhere == NULL) &&
				(ap->an_dirents == NULL)) {
			/*
			 * This takes care of the case where user
			 * does a umount without removing the
			 * directory. In that case we want the mount
			 * to happen, so just set ENOENT to trigger it.
			 */
			mutex_enter(&dap->an_lock);
			error = ENOENT;
		}
		mutex_exit(&ap->an_lock);
	}

	if (error == ENOENT) {
		/*
		 * If the last component of pathname has a space
		 * at the end it means that the lookup was initiated
		 * by the daemon. We should return ENOENT so that
		 * the daemon can proceed with the mount.
		 */
		if (special_path) {
			return (ENOENT);
		} else {
			/*
			 * If this directory is the root of a vfs
			 * and it is an indirect mount, trigger the
			 * mount, otherwise fall through and return
			 * ENOENT.
			 * The MF_MNTPNT flag is not set for direct
			 * maps.
			 */
			if ((dvp->v_flag & VROOT) &&
			    ((dap->an_mntflags & MF_MNTPNT) == 0)) {
				/*
				 * Create the indirect mountpoint autonode now,
				 * instead of waiting for the daemon to do a mkdir,
				 * so we can lock the actual mountpoint autonode
				 * and not its parent. This allows us to mount
				 * /home/foo in a separate, parallel thread to 
				 * one mounting /home/bar.
				 */
				if (!ap) {
					struct autoinfo *aip;
					ASSERT(dvp->v_vfsmountedhere == NULL);
					aip = vfs_bhvtoai(bhv_lookup_unlocked(VFS_BHVHEAD(dvp->v_vfsp), 
								&autofs_vfsops));
					ap = makeautonode(VDIR, dvp->v_vfsp, aip, cred);
					bdp = antobhv(ap);
					bcopy(nm, ap->an_name, strlen(nm) + 1);
					ap->an_mode = MAKEIMODE(VDIR, 0555);
					old_nodeid = ap->an_nodeid;
					mutex_exit(&dap->an_lock);
					error = auto_direnter(dap, ap);
					if (error) {
						ap->an_count--;
						freeautonode(ap);
						goto lookup_retry;
					}
					ap->an_parent = dap;
					VN_HOLD(dvp);
					*vpp = BHV_TO_VNODE(bdp);
#ifdef AUTODEBUG
					auto_dprint(autodebug, 10, 
						    "auto_lookup HOLD 0x%lx, v_ct %d\n", 
						    *vpp, (*vpp)->v_count);
#endif
					imadeit = 1;
				} else {
					mutex_exit(&dap->an_lock);
				}

				mutex_enter(&ap->an_lock);
				error = do_mount(bdp, nm, cred);
#ifdef AUTODEBUG
				auto_dprint(autodebug, 10, 
					    "auto_lookup do_mount dvp 0x%lx nm %s error %d\n", 
						    dvp, nm, error);
#endif
				mutex_exit(&ap->an_lock);

				mutex_enter(&dap->an_lock);
#ifdef AUTODEBUG
				auto_dprint(autodebug, 10, 
					    "auto_lookup DO_MOUNT RELE 0x%lx v_ct %d\n", 
					    *vpp, (*vpp)->v_count);
#endif
				VN_RELE(BHV_TO_VNODE(bdp));
				mutex_exit(&dap->an_lock);

				if (error == 0) {
					rw_enter(&dap->an_rwlock, RW_READER);
					error = autodir_lookup(dbdp, rnm, &bdp,
								cred);
					if (!error) {
						*vpp = BHV_TO_VNODE(bdp);
					}
#ifdef AUTODEBUG
					auto_dprint(autodebug, 10, 
			"auto_lookup, autodir_lookup %s vp 0x%lx error %d\n", 
						    rnm, *vpp, error);
#endif
					rw_exit(&dap->an_rwlock);
				} else if (error == EAGAIN) {
					goto lookup_retry;
				} else {
					int ret;
					autonode_t *anp;

					anp = NULL;
					rw_enter(&dap->an_rwlock, RW_READER);
					ret  = autodir_lookup(dbdp, rnm, &bdp, cred);
					if (!ret) {
						anp = bhvtoan(bdp);
						rw_exit(&dap->an_rwlock);
						mutex_enter(&anp->an_lock);
						if (imadeit && anp && 
						    ap == anp && 
						    old_nodeid == anp->an_nodeid) {
							extern int rm_autonode_forgiving(autonode_t *);
							
							if (anp->an_mntflags & MF_INPROG) {     /* mount in progress */
								k_sigset_t set;
								k_sigset_t set2 = {
									~(sigmask(SIGHUP)|sigmask(SIGINT)|
								  sigmask(SIGQUIT)|sigmask(SIGKILL)|
									  sigmask(SIGTERM)|sigmask(SIGTSTP))};
								
								anp->an_mntflags |= MF_WAITING_MOUNT;
								/*
								 * Mask out all signals except SIGHUP, SIGINT, SIGQUIT
								 * and SIGTERM. (Preserving the existing masks).
								 */
								assign_cursighold(&set, &set2);
								
								if (sv_wait_sig(&anp->an_cv_mount, PZERO,
										&anp->an_lock, 0) < 0) {
									error = EINTR;
									mutex_enter(&dap->an_lock);
#ifdef AUTODEBUG
									auto_dprint(autodebug, 10, 
										    "auto_lookup RELE 0x%lx v_ct %d\n", 
										    *vpp, (*vpp)->v_count);
#endif
									VN_RELE(antovn(anp));
									mutex_exit(&dap->an_lock);
									*vpp = NULL;
									return error;
								}
								mutex_enter(&dap->an_lock);
#ifdef AUTODEBUG
								auto_dprint(autodebug, 10, 
									    "auto_lookup RELE 0x%lx v_ct %d\n", 
									    *vpp, (*vpp)->v_count);
#endif
								VN_RELE(antovn(anp));	
								mutex_exit(&dap->an_lock);
								goto lookup_retry;
							}
							mutex_exit(&anp->an_lock);
							rw_enter(&dap->an_rwlock, RW_WRITER);
							/* 
							 * Is it ok to ignore the return value here ?.
							 */
							rm_autonode_forgiving(anp);
							rw_exit(&dap->an_rwlock);
						} else
							mutex_exit(&anp->an_lock);
						mutex_enter(&dap->an_lock);
#ifdef AUTODEBUG
						auto_dprint(autodebug, 10, 
							    "auto_lookup RELE 0x%lx v_ct %d\n", 
							    *vpp, (*vpp)->v_count);
#endif
						VN_RELE(antovn(anp));
						mutex_exit(&dap->an_lock);
					} else {
						rw_exit(&dap->an_rwlock);
					}
				}
			} else {
				if (*vpp)
					VN_RELE(*vpp);
				mutex_exit(&dap->an_lock);
			}
		}
	} else if (error == 0) {
		/*
		 * If autonode is found, and if it happens
		 * to be mounted on, we should strip off
		 * the trailing space. Because it only has
		 * a meaning within autofs. So when we cross
		 * over to another vfs, we dont need it. This is
		 * the case in heirarchical mounts.
		 */
		if ((*vpp)->v_vfsmountedhere && special_path) {
			bhv_desc_t *bdp1, *bdp2;

			bdp1 = bhv_lookup_unlocked(VFS_BHVHEAD((*vpp)->v_vfsp), &autofs_vfsops);
			bdp2 = bhv_base_unlocked(VFS_BHVHEAD((*vpp)->v_vfsmountedhere));
			if (BHV_OPS(bdp1) != BHV_OPS(bdp2)) {
				if (pnp->pn_pathlen > 0)
					pnp->pn_pathlen--;
				pnp->pn_buf[strlen(pnp->pn_buf) - 1] = '\0';
			}
		}
	}

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_lookup returning error %d\n", error);
#endif
	return (error);
}

static int
auto_create(
	bhv_desc_t *dbdp,
	char *nm,
	struct vattr *va,
	int excl,
	int mode,
	vnode_t **vpp,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	int error;
#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_create dvp 0x%lx nm %s\n", dvp, nm);
#endif

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}

	if (dvp->v_vfsmountedhere == NULL)
		return (ENOSYS);

	/*
	 * Redirect call to filesystem mounted at dvp
	 */
	VFS_ROOT(dvp->v_vfsmountedhere, &dvp, error);
	if (error)
		return (error);

	VOP_CREATE(dvp, nm, va, excl, mode, vpp, cr, error);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "auto_create RELE 0x%lx v_ct %d\n", 
		    dvp, dvp->v_count);
#endif
	VN_RELE(dvp);

	return (error);
}

/* ARGSUSED */
static int
auto_remove(
	bhv_desc_t *dbdp,
	char *nm,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	int error;
#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_remove dvp 0x%lx nm %s\n", dvp, nm);
#endif

	if (dvp->v_vfsmountedhere == NULL)
		return (ENOSYS);

	/*
	 * Redirect call to filesystem mounted at dvp
	 */
	VFS_ROOT(dvp->v_vfsmountedhere, &dvp, error);
	if (error)
		return (error);

	VOP_REMOVE(dvp, nm, cr, error);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "auto_remove RELE 0x%lx v_ct %d\n", 
		    dvp, dvp->v_count);
#endif
	VN_RELE(dvp);

	return (error);
}

/* ARGSUSED */
static int
auto_rename(
	bhv_desc_t *odbdp,
	char *onm,
	vnode_t *ndvp,
	char *nnm,
	struct pathname *npnp,
	struct cred *cr)
{
	vnode_t *odvp = BHV_TO_VNODE(odbdp);
	int error;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, 
		    "auto_rename odvp 0x%lx onm %s to ndvp 0x%lx nnm %s\n",
		    odvp, onm, ndvp, nnm);
#endif
	if (odvp->v_vfsmountedhere == NULL || ndvp->v_vfsmountedhere == NULL)
		return (ENOSYS);

	/*
	 * Redirect call to filesystem mounted at odvp/ndvp
	 */
	VFS_ROOT(odvp->v_vfsmountedhere, &odvp, error);
	if (error)
		return (error);
	VFS_ROOT(ndvp->v_vfsmountedhere, &ndvp, error);
	if (error) {
		VN_RELE(odvp);
		return (error);
	}

	VOP_RENAME(odvp, onm, ndvp, nnm, (struct pathname *) 0, cr, error);
	VN_RELE(odvp);
	VN_RELE(ndvp);

	return (error);
}

static int
auto_mkdir(
	bhv_desc_t *dbdp,
	char *nm,
	struct vattr *va,
	struct vnode **vpp,
	struct cred *cred)
{
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	bhv_desc_t *bdp;
	autonode_t *ap, *dap;
	struct vnode *vp;
	int error;
	int namelen = strlen(nm);
	char rnm[MAXNAMLEN + 1];
	struct autoinfo *aip;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 1, "auto_mkdir dvp 0x%lx nm %s\n", dvp, nm);
#endif

	/*
	 * Check if mounted-on
	 */
	if (dvp->v_vfsmountedhere) {
		VFS_ROOT(dvp->v_vfsmountedhere, &vp, error);
		if (error)
			return (error);
		VOP_MKDIR(vp, nm, va, vpp, cred, error);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 1, 
		"auto_mkdir, vp 0x%lx v_cnt %d, vpp 0x%lx v_cnt %d, error %d\n", 
		vp, vp->v_count, *vpp, (*vpp)->v_count, error);
	auto_dprint(autodebug, 10, "auto_mkdir RELE 0x%lx v_ct %d\n", 
		    vp, vp->v_count);
#endif
		VN_RELE(vp);
		return(error);
	}

	/*
	 * If the last char in name is a space, it means
	 * that it is from the daemon, strip it off.
	 * Everbody else should get an ENOSYS.
	 */
	if (nm[namelen - 1] == ' ') {
		bcopy(nm, rnm, namelen - 1);
		rnm[namelen - 1] = '\0';
	}
	else
		return (ENOSYS);

	if ((nm[0] == '.') &&
	    (namelen == 1 || (namelen == 2 && nm[1] == '.')))
		return (EINVAL);

	VOP_ACCESS(dvp, VEXEC|VWRITE, cred, error);
	if (error)
		return (error);

	dap = bhvtoan(dbdp);
	rw_enter(&dap->an_rwlock, RW_READER);
	error = autodir_lookup(dbdp, rnm, &bdp, cred);
	if (!error) {
		*vpp = BHV_TO_VNODE(bdp);
	}
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, 
		    "auto_mkdir, autodir_lookup returned: vp 0x%lx error %d\n", 
		    *vpp, error);
#endif
	rw_exit(&dap->an_rwlock);

	if (!error) {
#ifdef AUTODEBUG
		auto_dprint(autodebug, 8, 
			    "auto_mkdir RELE 0x%lx v_ct %d\n", 
			    *vpp, (*vpp)->v_count);
#endif
		VN_RELE(*vpp);
		return (EEXIST);
	}

	aip = vfs_bhvtoai(bhv_lookup_unlocked(VFS_BHVHEAD(dvp->v_vfsp), &autofs_vfsops));
	ap = makeautonode(VDIR, dvp->v_vfsp, aip, cred);
	vp = antovn(ap);

	bcopy(rnm, ap->an_name, strlen(rnm) + 1);
	ap->an_mode = MAKEIMODE(VDIR, va->va_mode);

	error = auto_direnter(dap, ap);
	if (error) {
#ifdef AUTODEBUG
		auto_dprint(autodebug, 10, "auto_mkdir error %d\n", error);
#endif
		ap->an_count--;
		freeautonode(ap);
		ap = NULL;
		*vpp = (struct vnode *) 0;
	} else {
		ap->an_parent = dap;
		VN_HOLD(dvp);
#ifdef AUTODEBUG
		auto_dprint(autodebug, 8, "auto_mkdir HOLD 0x%lx, v_ct %d\n", 
			    dvp, dvp->v_count);
#endif

		/*
		 * Get a reference so we have two, one for the caller which
		 * he may drop and a second to keep the vnode out of inactive 
		 * as long as we have an_count references to it.
		 *
		 * This new approach to vnode references will only be used
		 * in CELL_IRIX configurations until after the tree split,
		 * at Which point it should become universal.
		 */
#if CELL_IRIX
		VN_HOLD(vp);
#endif /* CELL_IRIX */	
		*vpp = vp;
	}
#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_mkdir vp 0x%lx, v_ct %d\n", 
		    vp, vp->v_count);
#endif
	return (error);

}

static int
auto_rmdir(
	bhv_desc_t *dbdp,
	char *nm,
	struct vnode *cdir,
	struct cred *cred)
{
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	autonode_t *dap, *ap, *cap, **app;
	bhv_desc_t *bdp;
	int error, namelen;
	struct vnode *vp;
	char rnm[MAXNAMLEN + 1];

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_rmdir dvp 0x%lx nm %s\n", dvp, nm);
#endif
	/*
	 * Check if mounted-on
	 */
	if (dvp->v_vfsmountedhere) {
		VFS_ROOT(dvp->v_vfsmountedhere, &vp, error);
		if (error)
			return (error);

		VOP_RMDIR(vp, nm, cdir, cred, error);
#ifdef AUTODEBUG
		auto_dprint(autodebug, 10, "auto_rmdir RELE 0x%lx v_ct %d\n", 
			    vp, vp->v_count);
#endif
		VN_RELE(vp);
		return(error);
	}

	namelen = strlen(nm);
	if (namelen == 0)
		cmn_err(CE_PANIC, "autofs: autofs_rmdir");

	/*
	 * If the last char in name is a space, it means
	 * that it is from the daemon, strip it off.
	 * Everbody else should get an ENOSYS.
	 */
	if (nm[namelen - 1] == ' ') {
		bcopy(nm, rnm, namelen - 1);
		rnm[namelen - 1] = '\0';
	}
	else
		return (ENOSYS);

	if (nm[0] == '.') {
		if (namelen == 1)
			return (EINVAL);
		else if ((namelen == 2) && (nm[1] == '.'))
			return (EEXIST);
	}

	/*
	 * XXX -- Since we've gone to all the trouble of
	 * calling VOP_ACCESS, it would be nice to do
	 * something with the result...
	 */
	VOP_ACCESS(dvp, VEXEC|VWRITE, cred, error);

	dap = bhvtoan(dbdp);
	rw_enter(&dap->an_rwlock, RW_WRITER);

	error = autodir_lookup(dbdp, rnm, &bdp, cred);
	if (error) {
		rw_exit(&dap->an_rwlock);
		return (ENOENT);
	}

	/*
	 * prevent a race between mount and rmdir
	 */
	vp = BHV_TO_VNODE(bdp);
	if (VN_VFSLOCK(vp)) {
		error = EBUSY;
		goto bad;
	}
	if (vp->v_vfsmountedhere != NULL) {
		error = EBUSY;
		goto vfs_unlock_bad;
	}

	ap = bhvtoan(bdp);

	/*
	 * Can't remove directory if the sticky bit is set
	 * unless you own the parent dir and curr dir.
	 */
	if ((dap->an_mode & S_ISVTX) && (cred->cr_uid != 0) &&
	    (cred->cr_uid != dap->an_uid) &&
	    (cred->cr_uid != ap->an_uid)) {
		error = EPERM;
		goto vfs_unlock_bad;
	}

	if ((vp == dvp) || (vp == cdir)) {
		error = EINVAL;
		goto vfs_unlock_bad;
	}
	if (ap->an_dirents != NULL) {
		error = ENOTEMPTY;
		goto vfs_unlock_bad;
	}
	if ((app = &dap->an_dirents) == NULL)
		cmn_err(CE_PANIC,
			"autofs: auto_rmdir: null directory list 0x%lx",
			dap);

	for (;;) {
		cap = *app;
		if (cap == NULL) {
			cmn_err(CE_WARN,
				"autofs: auto_rmdir: No entry for %s\n", nm);
			error = ENOENT;
			goto vfs_unlock_bad;
		}
		if ((cap == ap) &&
		    (strcmp(cap->an_name, rnm) == 0))
			break;
		app = &cap->an_next;
	}

	/*
	 * cap points to the correct directory entry
	 */
	*app = cap->an_next;
	cap->an_next = NULL;
	rw_exit(&dap->an_rwlock);
	mutex_enter(&dap->an_lock);
	dap->an_size--;
	mutex_exit(&dap->an_lock);

	VN_VFSUNLOCK(vp);
	/*
	 * the autonode had a pointer to parent Vnode
	 */

#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "auto_rmdir RELE 0x%lx v_ct %d\n", 
		    dvp, dvp->v_count);
#endif
	VN_RELE(dvp);

	mutex_enter(&cap->an_lock);
	cap->an_size -= 2;
	cap->an_count--;
	mutex_exit(&cap->an_lock);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "auto_rmdir RELE 0x%lx v_ct %d, ref %d\n", 
		    antovn(cap), (antovn(cap))->v_count, cap->an_count);
#endif
	VN_RELE(antovn(cap));
	return (0);

vfs_unlock_bad:
	VN_VFSUNLOCK(vp);
bad:
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "auto_rmdir RELE 0x%lx v_ct %d\n", 
		    vp, vp->v_count);
#endif
	VN_RELE(vp);
	rw_exit(&dap->an_rwlock);
	return (error);
}

static void
autofs_fmtdirent(
	void *buf, 
	ino_t id,
	off_t off,
	char *name,
	int namlen,
	u_short reclen)
{
	register struct dirent *bep = (struct dirent *)buf;

	bep->d_ino = id;
	bep->d_off = off;
	bep->d_reclen = reclen;
	bcopy(name, bep->d_name, namlen);
	bep->d_name[namlen] = '\0';
}

static void
irix5_autofs_fmtdirent(
	void *buf, 
	ino_t id,
	off_t off,
	char *name,
	int namlen,
	u_short reclen)
{
	register struct irix5_dirent *bep = (struct irix5_dirent *)buf;

	bep->d_ino = id;
	bep->d_off = off;
	bep->d_reclen = reclen;
	bcopy(name, bep->d_name, namlen);
	bep->d_name[namlen] = '\0';
}

static int
auto_readdir(
	bhv_desc_t *bdp,
	struct uio *uiop,
	struct cred *cred,
	int *eofp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	autonode_t *ap = bhvtoan(bdp);
	autonode_t *cap;
	register void *dep;		/* ptr to dirent */
	register int total_bytes_wanted;
	register off_t offset;
	register int outcount = 0;
	caddr_t outbuf;
	int namelen;
	int mount_action = 0;
	int error = 0;
	int target_abi;
	int bufsize;
	int dotsize, dotdotsize = 0;
	int reached_max = 0;
	int reclen;
	void (*dirent_func)(void *, ino_t, off_t, char *, int, u_short);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_readdir vp 0x%lx eofp %d\n", 
		    vp, (eofp == NULL ? 0:1));
#endif

	error = 0;
        /*
	 * For direct mounts trigger a mount here.
	 * If /usr/share/man is such a mount point,
	 * an ls /usr/share/man should do the right thing.
	 *
	 * Undo the lock held by getdents, before
	 * attempting a do_mount, because do_mount
	 * may cause a mkdir to happen, which will
	 * again grab this lock.
	 */
	auto_rwunlock(bdp, VRWLOCK_READ);
	mutex_enter(&ap->an_lock);
	if (NEED_MOUNT(bdp) &&
	    ((!(MOUNTED_ON(bdp)) && (vp->v_vfsmountedhere == NULL) &&
	    (ap->an_dirents == NULL)) || (ap->an_mntflags & MF_INPROG))) {
		/*
		 * Start by assuming we have to trigger the mount ourselves.
		 */
		mount_action = 1;
		while (ap->an_mntflags & (MF_WAITING_MOUNT | MF_INPROG)) {
			ap->an_mntflags |= MF_WAITING_MOUNT;
			if (sv_wait_sig(&ap->an_cv_mount, PZERO,
					&ap->an_lock, 0) < 0) {
				auto_rwlock(bdp, VRWLOCK_READ);
				return (EINTR);
			} else if (ap->an_error == 0) {
				/*
				 * Someone else did the mount for us.
				 */
				mount_action = 0;
			} else {
				/*
				 * Return the error encountered.
				 */
				mutex_enter(&ap->an_lock);
				error = ap->an_error;
				mutex_exit(&ap->an_lock);
				auto_rwlock(bdp, VRWLOCK_READ);
				return (error);
			}
			mutex_enter(&ap->an_lock);
		}
		if (mount_action) {
			ap->an_mntflags |= MF_WAITING_MOUNT;
			error = do_mount(bdp, ".", cred);
			mutex_exit(&ap->an_lock);
			auto_rwlock(bdp, VRWLOCK_READ);
			if (error)
				return (error);
		} else {
			mutex_exit(&ap->an_lock);
			auto_rwlock(bdp, VRWLOCK_READ);
		}
	} else {
		mutex_exit(&ap->an_lock);
		auto_rwlock(bdp, VRWLOCK_READ);
	}

	if (vp->v_vfsmountedhere != NULL) {
		vnode_t *newvp;

		VFS_ROOT(vp->v_vfsmountedhere, &newvp, error);
		if (!error) {
			VOP_RWLOCK(newvp, VRWLOCK_READ);
			VOP_READDIR(newvp, uiop, cred, eofp, error);
			VOP_RWUNLOCK(newvp, VRWLOCK_READ);
#ifdef AUTODEBUG
			auto_dprint(autodebug, 10, 
				    "auto_readdir RELE 0x%lx v_ct %d\n", 
				    newvp, newvp->v_count);
#endif
			VN_RELE(newvp);
		}
		return (error);
	}
	/*
	 * XXX need code to list indirect
	 * directory or offset path directories.
	 * For indirect case - need to think
	 * about showing potential names as
	 * well as mounted names.
	 */

	if (uiop->uio_iovcnt != 1)
		return (EINVAL);

	target_abi = GETDENTS_ABI(get_current_abi(), uiop);
	/* XXX : always return . and .. */
	switch(target_abi) {
	case ABI_IRIX5_64:
	case ABI_IRIX5_N32:
		dirent_func = autofs_fmtdirent;
		dotsize = DIRENTSIZE(2);
		dotdotsize = DIRENTSIZE(3);
		bufsize = uiop->uio_iov->iov_len + DIRENTBASESIZE;
		break;
	case ABI_IRIX5:
		dirent_func = irix5_autofs_fmtdirent;
		dotsize = IRIX5_DIRENTSIZE(2);
		dotdotsize = IRIX5_DIRENTSIZE(3);
		bufsize = uiop->uio_iov->iov_len + IRIX5_DIRENTBASESIZE;
		break;
	}
	outbuf = kmem_zalloc(bufsize, KM_SLEEP);
	dep = outbuf;
	total_bytes_wanted = uiop->uio_iov->iov_len - (dotsize + dotdotsize);
	/*
	 * Held when getdents calls VOP_RWLOCK
	 */
	ASSERT(RW_READ_HELD(&ap->an_rwlock));
	cap = ap->an_dirents;
	offset = 0;
	while (cap) {
		namelen = strlen(cap->an_name) + 1;
		if (offset >= uiop->uio_offset) {
			switch(target_abi) {
			case ABI_IRIX5_64:
			case ABI_IRIX5_N32:
				reclen = DIRENTSIZE(namelen);
				break;
			case ABI_IRIX5:
				reclen = IRIX5_DIRENTSIZE(namelen);
				break;
			}
			if (outcount + reclen > total_bytes_wanted) {
				reached_max = 1;
				break;
			}
			(*dirent_func)(dep, cap->an_nodeid, cap->an_offset, 
				       cap->an_name, namelen, reclen);
			outcount += reclen;
			dep = (void *)((char *)dep + reclen);
		}
		offset = cap->an_offset;
		cap = cap->an_next;
	}
	if (uiop->uio_offset == 0) {
		/*
		 * first time so fudge . and ..
		 * offset 1, namelen 2
		 * offset 2, namelen 3
		 */
		(*dirent_func)(dep, ap->an_nodeid, 1, ".",  2, dotsize);
		outcount += dotsize;
		dep = (void *)((char *)dep + dotsize);
		
		(*dirent_func)(dep, ap->an_parent->an_nodeid, 2, "..", 3, 
			       dotdotsize);
		outcount += dotdotsize;
		dep = (void *)((char *)dep + dotdotsize);
	}
	if (outcount)
		error = uiomove(outbuf, outcount, UIO_READ, uiop);
	if (!error) {
		if (reached_max) {
			/*
			 * This entry did not get added to the buffer on this
			 * call. We need to add it on the next call therefore
			 * set uio_offset to this entry's offset.
			 */
			uiop->uio_offset = offset;
		} else {
			/*
			 * Process next entry on next call.
			 */
			uiop->uio_offset = offset +1;
		}
		if (cap == NULL && eofp)
			*eofp = 1;
	}
	kmem_free((void *)outbuf, bufsize);
	return (error);
}

static int
auto_symlink(
	bhv_desc_t *dbdp,
	char *lnm,
	struct vattr *tva,
	char *tnm,
	struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	int error;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_symlink dvp 0x%lx %s\n", dvp, lnm);
#endif
	if (dvp->v_vfsmountedhere == NULL)
		return (ENOSYS);

	/*
	 * Redirect call to filesystem mounted at dvp.
	 */
	VFS_ROOT(dvp->v_vfsmountedhere, &dvp, error);
	if (error)
		return (error);

	VOP_SYMLINK(dvp, lnm, tva, tnm, cr, error);
	VN_RELE(dvp);

	return (error);
}

/* ARGSUSED */
static int
auto_fsync(
	bhv_desc_t *bdp,
	int syncflag,
	struct cred *cred,
	off_t start,
	off_t stop
)
{
	return (0);
}

/* ARGSUSED */
static int
auto_inactive(
	bhv_desc_t *bdp,
	struct cred *cred)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	autonode_t *ap = bhvtoan(bdp);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, 
		    "auto_inactive vp 0x%lx, v_count %d, ref %d\n", 
		    vp, vp->v_count, ap->an_count);
#endif

	/* the inactive_teardown flag must have been set at vn_alloc time */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	/*
	 * Under the old, idiosyncratic approach to reference management,
	 * we would enter auto_inactive repeatedly, and only free the
         * autonode the last such time, once an independent reference
	 * counting scheme allowed us to do so.  Under the new scheme,
	 * inactive is only once, by which time all references should
	 * be gone.
	 *
	 * The new scheme is required by CFS for CELL_IRIX.  It should
	 * be made universal once the tree splits.
	 */
#if CELL_IRIX
	ASSERT(ap->an_size == 0 && ap->an_count == 0);
	freeautonode(ap);
#else /* CELL_IRIX */	
	if (ap->an_size == 0 && ap->an_count == 0)
		freeautonode(ap);
#endif /* CELL_IRIX */	

	return VN_INACTIVE_NOCACHE;
}

static void
auto_rwlock(
	bhv_desc_t *bdp,
	vrwlock_t write_lock)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	autonode_t *ap = bhvtoan(bdp);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_rwlock vp 0x%lx\n", vp);
#endif
	if (write_lock == VRWLOCK_READ)
		rw_enter(&ap->an_rwlock, RW_READER);
	else
		rw_enter(&ap->an_rwlock, RW_WRITER);
}

/* ARGSUSED */
static void
auto_rwunlock( 
	bhv_desc_t *bdp,
	vrwlock_t write_lock)
{
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	autonode_t *ap = bhvtoan(bdp);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_rwunlock vp 0x%lx r %d q 0x%x\n", 
		    vp, ap->an_rwlock.mr_lbits, &(ap->an_rwlock.mr_waiters));
#endif
	rw_exit(&ap->an_rwlock);
}

/*ARGSUSED*/
static int
auto_attrget(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	int i;

	if ((i = _MAC_AUTOFS_ATTR_GET(bdp,name,value,valuelenp,flags,cred)) >= 0)
		return i;

	return ENOATTR;
}
/*ARGSUSED*/
static int
auto_attrset(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int valuelenp,
	int flags,
	struct cred *cred)
{
	int i;

	if ((i = _MAC_AUTOFS_ATTR_SET(bdp,name,value,valuelenp,flags,cred)) >= 0)
		return i;

	return ENOATTR;
}

/*ARGSUSED*/
static int
auto_attrlist(
	bhv_desc_t *bdp,
	char *buffer,
	int bufsize,
	int flags,
	attrlist_cursor_kern_t *cursor,
	struct cred *cred)
{
	int i;

	if ((i = _MAC_AUTOFS_ATTR_LIST(bdp,buffer,bufsize,flags,cursor,cred)) >= 0)
		return i;

	return ENOATTR;
}

vnodeops_t autofs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	auto_open,	/* open */
	auto_close,	/* close */
	(vop_read_t)fs_nosys,
	(vop_write_t)fs_nosys,
	(vop_ioctl_t)fs_nosys,
	fs_setfl,	/* setfl */
	auto_getattr,	/* getattr */
	(vop_setattr_t)fs_nosys,
	auto_access,	/* access */
	auto_lookup,	/* lookup */
	auto_create,	/* create */
	auto_remove,	/* remove */
	(vop_link_t)fs_nosys,
	auto_rename,	/* rename */
	auto_mkdir,	/* mkdir */
	auto_rmdir,	/* rmdir */
	auto_readdir,	/* readdir */
	auto_symlink,	/* symlink */
	(vop_readlink_t)fs_nosys,
	auto_fsync,	/* fsync */
	auto_inactive,	/* inactive */
	(vop_fid_t)fs_nosys,
	(vop_fid2_t)fs_nosys,
	auto_rwlock,	/* rwlock */
	auto_rwunlock,	/* rwunlock */
	(vop_seek_t)fs_nosys,
	fs_cmp,		/* cmp */
	(vop_frlock_t)fs_nosys,
	(vop_realvp_t)fs_nosys,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_noval,
	(vop_map_t)fs_nodev,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	fs_poll,	/* poll */
	(vop_dump_t)fs_nosys,
	(vop_pathconf_t)fs_nosys,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_nosys,
	auto_attrget,
	auto_attrset,
	(vop_attr_remove_t)fs_nosys,
	auto_attrlist,
	fs_cover,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
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
