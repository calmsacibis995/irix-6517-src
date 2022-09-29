/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/procfs/prvfsops.c	1.8"*/
#ident	"$Revision: 1.56 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fs_subr.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ddi.h>
#include <sys/swap.h>
#include <sys/capability.h>
#include <string.h>
#include <sys/tuneable.h>
#include "prdata.h"
#include "prsystm.h"
#include "procfs_private.h"

int		procfs_type;
zone_t          *procfs_trzone;
procfs_info_t   procfs_info;
vfs_t		*procfs_vfs;
lock_t          procfs_lock;
struct prnode   *procfs_freelist;
int             procfs_nfree;
struct prnode   procfs_root;
struct prnode   procfs_pinfo;
struct bhv_desc procfs_bhv;
int             procfs_refcnt;
sv_t		jswait;

extern vfsops_t prvfsops;


/* ARGSUSED */
static int
prmount(
	struct vfs *vfsp,
	struct vnode *mvp,
	struct mounta *uap,
	char *attrs,
	struct cred *cr)
{
        int err;

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return EPERM;
	if (mvp->v_type != VDIR)
		return ENOTDIR;
	if (mvp->v_count > 1 || (mvp->v_flag & VROOT))
		return EBUSY;
	/*
	 * Prevent duplicate mount.
	 */
        ASSERT(PROCFS_CELL_HERE());
        err = procfs_newvfs(vfsp);
	if (err)
		return (err);

	/* 
         * Now initialize the vfs and the special prnodes
	 */
	procfs_initnodes(vfsp);
	vfs_insertbhv(vfsp, &procfs_bhv, &prvfsops, NULL);
        procfs_initvfs(vfsp);
#if     CELL_IRIX
        procfs_cellmount(vfsp, &procfs_bhv);
#endif  /* CELL_IRIX */
	return 0;
}

/* ARGSUSED */
static int
prunmount(
	bhv_desc_t *bdp,
	int flags,
	struct cred *cr)
{
        int error;
        vfs_t *vfsp = bhvtovfs(bdp);

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return EPERM;

	/*
	 * Ensure that no /proc vnodes are in use.
	 */
        ASSERT(flags == 0);
	error = procfs_umcheck(vfsp);
	if (error)
		return (error);

	procfs_umfinal(vfsp, bdp);
	return 0;
}

/* ARGSUSED */
int
prroot(
	bhv_desc_t *bdp,
	struct vnode **vpp)
{
	struct vnode *vp = procfs_root.pr_vnode;

	VN_HOLD(vp);
	*vpp = vp;
	return 0;
}

/* ARGSUSED */
int
prstatvfs(
	bhv_desc_t *bdp,
	register struct statvfs *sp,
	struct vnode *vp)
{
	register int n;
	pgno_t maxsmem, maxvmem;
	struct vfs *vfsp;

	if (bdp == NULL)
		return EINVAL;
	vfsp = bhvtovfs(bdp);

	/* ffree and favail fields. */
	n = v.v_proc - activeproccount();

	/* get max virtual swap available */
	getmaxswap(&maxsmem, &maxvmem, NULL);

	bzero((caddr_t)sp, sizeof(*sp));
	sp->f_bsize	= NBPP;		/* XXX svr4 has 1024 for these two */
	sp->f_frsize	= NBPP;
					/* and zero for these three */
	sp->f_blocks	= maxmem + maxsmem - tune.t_minasmem;
	sp->f_bfree	= dtop(freeswap) + GLOBAL_FREEMEM();
	sp->f_bavail	= sp->f_bfree;
	sp->f_files	= 2*v.v_proc + 5;
	sp->f_ffree	= n;
	sp->f_favail	= n;
	sp->f_fsid	= vfsp->vfs_dev;
	strcpy(sp->f_basetype, vfssw[procfs_type].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = PNSIZ;
	strcpy(sp->f_fstr, "/proc");
	strcpy(&sp->f_fstr[6], "/proc");
	return 0;
}

/*
 * /proc VFS operations vector.
 */
vfsops_t prvfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	prmount,
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_dounmount,
	prunmount,
	prroot,
	prstatvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
#if     CELL_IRIX
        procfs_realvfsops,  /* realvfsops */
        procfs_import,      /* import */
#else   /* CELL_IRIX */
	fs_noerr,	/* realvfsops */
	fs_import,	/* import */
#endif  /* CELL_IRIX */
	fs_quotactl     /* quotactl */
};

void
prinit(
	register struct vfssw *vswp,
	int fstype)
{
	dev_t dev;

	/*
         * Initialize per-cell data
         */
	ASSERT(fstype != 0);
	procfs_type = fstype;
	spinlock_init(&procfs_lock, "prfree");
	sv_init(&jswait, SV_DEFAULT, "jointstop");
	procfs_trzone = kmem_zone_init(sizeof(struct pr_tracemasks), "trmasks");

	/*
	 * Associate VFS ops vector with this fstype.  Note that in the cell
         * configuration, this initialization is going to be done multiple
         * times, with the same data stored each time.  We could make the
	 * initialization conditional on PROCFS_CELL_HERE but then it
	 * would not get initialized properly.
         * The upshot is that it is easiest (and harmless) to leave the
         * multiple inititialization in for now.
	 */
	vswp->vsw_vfsops = &prvfsops;
	vswp->vsw_vnodeops = &prvnodeops;

	/*
         * Do initialization of the global info (propagated as expinfo to other
         * cells) on the special procfs cell (normally the golden cell).  This
         * currently consists of the following:
	 *    o A unique "device" number 
	 */
        if (PROCFS_CELL_HERE()) {
	        dev = getudev();
		if (dev  == -1) {
		        cmn_err(CE_WARN, "prinit: can't get unique device number");
			dev = 0;
		}
		procfs_info.info_dev = makedevice(dev, 0);
	}

}

/*
 * Used to establish the new procfs vfs structure.  Since there can only be one
 * procfs filesystem in the entire system, there can only be one procfs vfs on
 * each cell.
 *
 * The value returned is either zero or EBUSY.
 */
int
procfs_newvfs(
        vfs_t *vfsp)
{
        int err = 0;
        int s;

        s = mutex_spinlock(&procfs_lock);
        if (procfs_vfs)
            	err = EBUSY;
        else
                procfs_vfs = vfsp;
        mutex_spinunlock(&procfs_lock, s);
	return (err);
}

/*
 * Set up special nodes for procfs file system.
 */
void
procfs_initnodes(
        vfs_t *vfsp)
{
	struct vnode *vp;
	struct prnode *pnp;
	int s;

	/*
	 * Initialize /proc node.
	 */
	pnp = &procfs_root;
	vp = vn_alloc(vfsp, VDIR, 0);
	bhv_desc_init(PRNODETOBHV(pnp), pnp, vp, &prvnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), PRNODETOBHV(pnp));

	/*
	 * All we want left on is VROOT, but we can't clear the lock bit.
	 */
	s = VN_LOCK(vp);
	vp->v_flag = VROOT | VLOCK | VINACTIVE_TEARDOWN;
	VN_UNLOCK(vp, s);
	pnp->pr_vnode = vp;
	pnp->pr_mode = 0555;	/* read and search permissions */
	pnp->pr_type = PR_PROCDIR;
	pnp->pr_next = NULL;
	pnp->pr_proc = NULL;
	pnp->pr_tpid = -1;
	pnp->pr_free = NULL;
	pnp->pr_opens = 0;
	pnp->pr_writers = 0;
	pnp->pr_pflags = 0;
	mutex_init(&pnp->pr_lock, MUTEX_DEFAULT, "prroot");

	/*
	 * Initialize /proc/pinfo node.
	 */
	pnp = &procfs_pinfo;
	vp = vn_alloc(vfsp, VDIR, 0);
	bhv_desc_init(PRNODETOBHV(pnp), pnp, vp, &prvnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), PRNODETOBHV(pnp));
	VN_FLAGSET(vp, VNOSWAP | VINACTIVE_TEARDOWN);
	pnp->pr_vnode = vp;
	pnp->pr_mode = 0555;
	pnp->pr_type = PR_PSINFO;
	pnp->pr_next = NULL;
	pnp->pr_proc = NULL;
	pnp->pr_tpid = -1;
	pnp->pr_free = NULL;
	pnp->pr_opens = 0;
	pnp->pr_writers = 0;
	pnp->pr_pflags = 0;
	mutex_init(&pnp->pr_lock, MUTEX_DEFAULT, "pinfo");
}

/*
 * Complete initialization of procfs vfs
 *
 * Used by prmount and procfs_import to complete initialization of a vfs.  Setting
 * the behavior is done by the caller since this is different in each case.
 */
void
procfs_initvfs(
        vfs_t *vfsp)
{
	vfsp->vfs_fstype = procfs_type;
	vfsp->vfs_dev = procfs_info.info_dev;
	vfsp->vfs_fsid.val[0] = procfs_info.info_dev;
	vfsp->vfs_fsid.val[1] = procfs_type;
	vfsp->vfs_bsize = 1024;
	vfs_setflag(vfsp, VFS_NOTRUNC | VFS_CELLROOT | VFS_CELLULAR);
        VFS_EXPINFO(vfsp, &procfs_info, sizeof(procfs_info_t));
}

/*
 * Check whether a file system can be unmounted
 *
 * Checks for active refs to the file system and returns an error code (either
 * zero or EBUSY) to indicate the disposition of the request.
 */
/* ARGSUSED */
int
procfs_umcheck(
        vfs_t *vfsp)
{
        ASSERT(vfsp == procfs_vfs);
        if (procfs_root.pr_vnode->v_count > 1)
	        return (EBUSY);
        if (procfs_pinfo.pr_vnode->v_count > 1)
	        return (EBUSY);
	if (procfs_refcnt)
	        return (EBUSY);
	return (0);
}

/*
 * Complete destruction of procfs vfs
 *
 * Implements the final dereferencing and behavior elimination for a procfs
 * file system, either the main or only one or a local image on another cell.
 */
void
procfs_umfinal(
        vfs_t *vfsp,
        bhv_desc_t *bdp)
{
        int s;

	ASSERT(bdp == &procfs_bhv);
	ASSERT(vfsp == procfs_vfs);
	VN_RELE(procfs_pinfo.pr_vnode);
	VN_RELE(procfs_root.pr_vnode);
	VFS_REMOVEBHV(vfsp, bdp);
        s = mutex_spinlock(&procfs_lock);
	procfs_vfs = NULL;
        mutex_spinunlock(&procfs_lock, s);
}
