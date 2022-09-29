/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mkdev.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/pathname.h>
#include <sys/xlate.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/fs_subr.h>
#include "autofs.h"
#include "autofs_prot.h"
#include <sys/kabi.h>

int autofs_init(vfssw_t *, int);

int autofs_major;
int autofs_minor;
kmutex_t autofs_minor_lock;

extern vnodeops_t autofs_vnodeops;
#ifdef	AUTODEBUG
extern int autodebug;
#endif

int autofs_fstype;
struct sockaddr_in autofs_addr;

/*
 * autofs vfs operations.
 */
static int auto_vfsmount(struct vfs *, struct vnode *, struct mounta *, char *, struct cred *);
static int auto_mntupdate(bhv_desc_t *, struct vnode *, struct mounta *, struct cred *);
static int auto_unmount(bhv_desc_t *, int, struct cred *);
static int auto_root(bhv_desc_t *, struct vnode **);
static int auto_statvfs(bhv_desc_t *, struct statvfs *, struct vnode *);

vfsops_t autofs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	auto_vfsmount,	/* mount */
	fs_nosys,	/* rootinit */
	auto_mntupdate,	/* mntupdate */
	fs_dounmount,	/* dounmount */
	auto_unmount,	/* unmount */
	auto_root,	/* root */
	auto_statvfs,	/* statvfs */
	fs_sync,	/* sync */
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_realvfsops,	/* realvfsops */
	fs_import,	/* import */
	fs_quotactl	/* quotactl */
};

static void
unmount_init(void)
{
	autonode_list = NULL;
	mutex_init(&autonode_list_lock, MUTEX_DEFAULT, 
		   "autonode list");	/* XXX never destroyed */
}

#define	MAJOR_MIN 128

int
autofs_init(
	vfssw_t *vswp,
	int fstype)
{
	autofs_fstype = fstype;
	ASSERT(autofs_fstype != 0);
	/*
	 * Associate VFS ops vector with this fstype
	 */
	vswp->vsw_vfsops = &autofs_vfsops;

	mutex_init(&autofs_minor_lock, MUTEX_DEFAULT, "autofs minor lock");
	mutex_init(&autonode_count_lock, MUTEX_DEFAULT, "autonode count lock");

	/*
	 * Assign unique major number for all autofs mounts
	 */
	if ((autofs_major = getudev()) < 0) {
		cmn_err(CE_WARN,
			"autofs: autofs_init: can't get unique device number");
		autofs_major = 0;
	}
	/* initialize sockaddr_in used to talk to local daemon */
	autofs_addr.sin_family = AF_INET;
	autofs_addr.sin_port = htons(AUTOFS_PORT);
	autofs_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	unmount_init();

	return (0);
}

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int
irix5_to_autofs_args(
        enum xlate_mode mode,
        void *to,
        int count,
        xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(irix5_autofs_args, autofs_args);

        target->path = (char *)(__psint_t)source->path;
        target->opts = (char *)(__psint_t)source->opts;
        target->map = (char *)(__psint_t)source->map;
        target->mount_to = source->mount_to;
        target->rpc_to = source->rpc_to;
        target->direct = source->direct;

        return 0;
}
#endif

/*
 * autofs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
static int
auto_mount(
	struct vfs *vfsp,
	bhv_desc_t *bdp,
	struct vnode *vp,
	struct mounta *uap,
	struct cred *cr)
{
	int error = 0;
	struct autofs_args args;
	struct vnode *rootvp = NULL;
	autonode_t *root;
	struct autoinfo *aip;
	char strbuff[MAXPATHLEN+1];
	dev_t autofs_dev;
	size_t len;

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT)) {
		return (EPERM);
	}

	/*
	 * Get arguments
	 */
	if (COPYIN_XLATE(uap->dataptr, &args, 
			 MIN(uap->datalen, sizeof (args)),
			 irix5_to_autofs_args, get_current_abi(), 1) < 0) {
		return (EFAULT);
	}

	/*
	 * For a remount just update mount information
	 * i.e. default mount options, map name, etc.
	 */
	if (uap->flags & MS_REMOUNT) {
		aip = vfs_bhvtoai(bdp);
		if (aip == NULL)
			return (EINVAL);

		aip->ai_mount_to = args.mount_to;
		aip->ai_rpc_to = args.rpc_to;
		aip->ai_direct = args.direct;
		/*
		 * Get default mount options
		 */
		if (copyinstr(args.opts, strbuff, sizeof (strbuff),
			(size_t *) &len)) {
			return (EFAULT);
		}
		kmem_free(aip->ai_opts, aip->ai_optslen);
		aip->ai_opts = kmem_alloc(len, KM_SLEEP);
		aip->ai_optslen = len;
		bcopy(strbuff, aip->ai_opts, len);

		/*
		 * Get map name
		 */
		if (copyinstr(args.map, strbuff, sizeof (strbuff),
			(size_t *) &len)) {
			return (EFAULT);
		}
		kmem_free(aip->ai_map, aip->ai_maplen);
		aip->ai_map = kmem_alloc(len, KM_SLEEP);
		aip->ai_maplen = len;
		bcopy(strbuff, aip->ai_map, len);

		return (0);
	}

	/*
	 * allocate autoinfo struct and attach it to vfs
	 */
	aip = (struct autoinfo *) kmem_zalloc(sizeof (*aip), KM_SLEEP);

	aip->ai_refcnt = 0;
	aip->ai_mount_to = args.mount_to;
	aip->ai_rpc_to = args.rpc_to;
	vfsp->vfs_bsize = 1024;
	vfsp->vfs_fstype = autofs_fstype;

	/*
	 * Assign a unique device id to the mount
	 */
	mutex_enter(&autofs_minor_lock);
	do {
		autofs_minor = (autofs_minor + 1) & MAXMIN;
		autofs_dev = makedev(autofs_major, autofs_minor);
	} while (vfs_devsearch(autofs_dev, VFS_FSTYPE_ANY));
	mutex_exit(&autofs_minor_lock);

	vfsp->vfs_dev = autofs_dev;
	vfsp->vfs_fsid.val[0] = autofs_dev;
	vfsp->vfs_fsid.val[1] = autofs_fstype;
	vfsp->vfs_flag |= VFS_CELLULAR;
	vfs_insertbhv(vfsp, &aip->ai_bhv, &autofs_vfsops, aip);
	vfsp->vfs_bcount = 0;
	if (error)
		goto errout;
	/*
	 * Get path for mountpoint
	 */
	if (copyinstr(args.path, strbuff, sizeof (strbuff), (size_t *) &len)) {
		error = EFAULT;
		goto errout;
	}
	aip->ai_path = kmem_alloc(len, KM_SLEEP);
	aip->ai_pathlen = len;
	bcopy(strbuff, aip->ai_path, len);

	/*
	 * Get default mount options
	 */
	if (copyinstr(args.opts, strbuff, sizeof (strbuff), (size_t *) &len)) {
		error = EFAULT;
		goto errout;
	}
	aip->ai_opts = kmem_alloc(len, KM_SLEEP);
	aip->ai_optslen = len;
	bcopy(strbuff, aip->ai_opts, len);

	/*
	 * Get map name
	 */
	if (copyinstr(args.map, strbuff, sizeof (strbuff), (size_t *) &len)) {
		error = EFAULT;
		goto errout;
	}
	aip->ai_map = kmem_alloc(len, KM_SLEEP);
	aip->ai_maplen = len;
	bcopy(strbuff, aip->ai_map, len);

	/*
	 * Get mount type
	 */
	aip->ai_direct = args.direct;

	/*
	 * Make the root vnode
	 */
	root = makeautonode(VDIR, vfsp, aip, cr);
	rootvp = antovn(root);
	strcpy(root->an_name, aip->ai_path); /* for debugging only */
	root->an_parent = root;

	VN_FLAGSET(rootvp, VROOT);
	root->an_mode = 0555;	/* read and search perms */
	if (aip->ai_direct) {
		VN_FLAGSET(rootvp, VROOTMOUNTOK);
		root->an_mntflags |= MF_MNTPNT;
	}
	aip->ai_rootvp = rootvp;
	aip->ai_root = root;

	/*
	 * All the root autonodes are held in a link list
	 * This facilitates the unmounting. The unmount
	 * thread then just has to traverse this link list
	 */
	mutex_enter(&autonode_list_lock);
	if (autonode_list == NULL)
		autonode_list = root;
	else {
		autonode_t *anp;
		anp = root;
		anp->an_next = autonode_list;
		autonode_list = anp;
	}
	mutex_exit(&autonode_list_lock);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, 
		    "auto_mount: vfsp 0x%lx root 0x%lx aip 0x%lx\n",
		    vfsp, rootvp, aip);
#endif

	return (0);

errout:
	/*
	 * NOTE: We should only arrive here if aip has already been
	 * allocated, and this is NOT a remount.
	 */
	ASSERT(aip != NULL);
	ASSERT((uap->flags & MS_REMOUNT) == 0);

	if (aip->ai_path != NULL)
		kmem_free(aip->ai_path, aip->ai_pathlen);
	if (aip->ai_opts != NULL)
		kmem_free(aip->ai_opts, aip->ai_optslen);
	if (aip->ai_map != NULL)
		kmem_free(aip->ai_map, aip->ai_maplen);
	/*
        * no locking is needed for bhv_remove()
        * since the new vfs is not on
        * the global vfs list yet
        */
	VFS_REMOVEBHV(vfsp, &aip->ai_bhv);
	kmem_free(aip, sizeof (*aip));

	return (error);
}

/* VFS_MOUNT */
/*ARGSUSED*/
static int
auto_vfsmount(
	struct vfs *vfsp,
        struct vnode *vp,
        struct mounta *uap,
	char *attrs,
        struct cred *cr)
{
	return(auto_mount(vfsp, NULL, vp, uap, cr));
}

/* VFS_MNTUPDATE */
static int
auto_mntupdate(
	bhv_desc_t *bdp,
        struct vnode *vp,
        struct mounta *uap,
        struct cred *cr)
{
	return(auto_mount(bhvtovfs(bdp), bdp, vp, uap, cr));
}

/*
 * Undo autofs mount
 */
/*ARGSUSED*/
static int
auto_unmount(
	bhv_desc_t *bdp,
	int i,
	struct cred *cr)
{
	struct autoinfo *aip;
	struct vnode *rvp;
	autonode_t *rap, *ap, **app;

	aip = vfs_bhvtoai(bdp);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_unmount bdp 0x%lx aip 0x%lx\n", 
		    bdp, aip);
#endif

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return (EPERM);

	rap = aip->ai_root;
	rvp = aip->ai_rootvp;

	/*
	 * In case of all autofs mountpoints the following
	 * should be true
	 */
	ASSERT(rap->an_parent == rap);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "auto_unmount r_ct %d v_ct %d\n", 
		    aip->ai_refcnt, rvp->v_count);
#endif
	if (rvp->v_count > 1) {
		return (EBUSY);
	}

	/*
	 * Remove the root vnode from the linked list of
	 * root autonodes.
	 */
	ASSERT(rap->an_dirents == NULL);

	mutex_enter(&autonode_list_lock);
	app = &autonode_list;
	for (;;) {
		ap = *app;
		if (ap == NULL)
			cmn_err(CE_PANIC,
				"autofs: "
				"root autonode not found for vfs 0x%lx\n",
				bhvtovfs(bdp));
		if (ap == rap)
			break;
		app = &ap->an_next;
	}
	*app = ap->an_next;
	mutex_exit(&autonode_list_lock);

	VFS_REMOVEBHV(bhvtovfs(bdp), bdp);
	ASSERT(rvp->v_count == 1);
	mutex_enter(&rap->an_lock);
	rap->an_size -= 2;
	rap->an_count--;
	mutex_exit(&rap->an_lock);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "about to rele 0x%lx, %d\n", 
		    rvp, rvp->v_count);
#endif
	VN_RELE(rvp);

	kmem_free(aip->ai_path, aip->ai_pathlen);
	kmem_free(aip->ai_opts, aip->ai_optslen);
	kmem_free(aip->ai_map, aip->ai_maplen);
	kmem_free(aip, sizeof (*aip));

	return (0);
}

/*
 * find root of autofs
 */
static int
auto_root(
	bhv_desc_t *bdp,
	struct vnode **vpp)
{
	*vpp = (struct vnode *) vfs_bhvtoai(bdp)->ai_rootvp;
	/*
	 * this is the only place where we get a chance
	 * to time stamp the autonode for direct mounts
	 */
	vfs_bhvtoai(bdp)->ai_root->an_ref_time = time;
	VN_HOLD(*vpp);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "auto_root HOLD 0x%lx, v_ct %d\n", 
		    *vpp, (*vpp)->v_count);
	auto_dprint(autodebug, 3, "auto_root bdp 0x%lx vp 0x%lx\n", 
		    bdp, *vpp);
#endif
	return (0);
}

/*
 * Get file system statistics.
 */
/*ARGSUSED*/
static int
auto_statvfs(
	register bhv_desc_t *bdp,
	struct statvfs *sbp,
	struct vnode *vp)
{

	struct vfs *vfsp = bhvtovfs(bdp);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "auto_statvfs 0x%lx\n", vfsp);
#endif

	bzero((caddr_t)sbp, (int)sizeof (*sbp));
	sbp->f_bsize	= vfsp->vfs_bsize;
	sbp->f_frsize	= sbp->f_bsize;
	sbp->f_blocks	= 0;
	sbp->f_bfree	= 0;
	sbp->f_bavail	= 0;
	sbp->f_files	= 0;
	sbp->f_ffree	= 0;
	sbp->f_favail	= 0;
	sbp->f_fsid	= vfsp->vfs_dev;
	strcpy(sbp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sbp->f_flag	= vf_to_stf(vfsp->vfs_flag);
	sbp->f_namemax	= MAXNAMELEN;
	strcpy(sbp->f_fstr, "autofs");

	return (0);
}
