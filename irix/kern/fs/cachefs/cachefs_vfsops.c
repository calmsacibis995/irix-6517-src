/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_vfsops.c 1.71     94/04/21 SMI"
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/uio.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/ddi.h>
#include <string.h>
#include "cachefs_fs.h"
#include <sys/mkdev.h>
#include <sys/dnlc.h>
#include <sys/capability.h>
#include <ksys/vfile.h>
#include <sys/xlate.h>
#include <sys/kthread.h>
#include <sys/fs_subr.h>
#include <ksys/vproc.h>
#include <sys/kabi.h>


#define	CFS_MAPSIZE	256

extern mutex_t cachefs_cachelock;			/* Cache list mutex */
cachefscache_t *cachefs_cachelist = NULL;		/* Cache struct list */
lock_t cachefs_minor_lock;		/* Lock for minor device map */
major_t cachefs_major = 0;
minor_t cachefs_minor = 0;
/*
 * cachefs vfs operations.
 */
static int cachefs_vfsmount(vfs_t *, vnode_t *, struct mounta *, char *, cred_t *);
static int cachefs_rootinit(vfs_t *);
static int cachefs_mntupdate(bhv_desc_t *, vnode_t *, struct mounta *, cred_t *);
static int cachefs_unmount(bhv_desc_t *, int, cred_t *);
static int cachefs_root(bhv_desc_t *, vnode_t **);
static int cachefs_statvfs(register bhv_desc_t *, struct statvfs *, vnode_t *);
static int cachefs_sync(bhv_desc_t *, int, cred_t *);
static int cachefs_vget(bhv_desc_t *, vnode_t **, fid_t *);
static int cachefs_vfsmountroot(bhv_desc_t *, whymountroot_t);

vfsops_t cachefs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	cachefs_vfsmount,
	cachefs_rootinit,
	cachefs_mntupdate,
	fs_dounmount,
	cachefs_unmount,
	cachefs_root,
	cachefs_statvfs,
	cachefs_sync,
	cachefs_vget,
	cachefs_vfsmountroot,
	fs_realvfsops,		/* realvfsops */
        fs_import,              /* import */
	fs_quotactl		/* quotactl */
};

/*
 * Initialize the vfs structure
 */
int cachefsfstyp;
int cnodesize = 0;
struct minormap cachefs_minormap;

/*
 * Set and return the first free position from the bitmap "map".
 * Return -1 if no position found.
 */
static int
get_minor_number(void)
{
	int i, j;
	u_char *mp, m;

	i = sizeof cachefs_minormap.vec;
	for (mp = cachefs_minormap.vec; --i >= 0; mp++) {
		m = *mp;
		if (m == (char)0xff)
			continue;
		for (j = NBBY; --j >= 0; m >>= 1) {
			if (!(m & 01)) {
				j = NBBY - 1 - j;
				*mp |= 1 << j;
				return (mp - cachefs_minormap.vec) * NBBY  + j;
			}
		}
	}
	return(-1);
}

/*
 * Clear the designated position "n" in bitmap "map".
 */
void
release_minor_number(int n)
{
	if (n < 0)
		return;
	cachefs_minormap.vec[n / NBBY] &= ~(1 << (n & (NBBY-1)));
}

static dev_t
cachefs_mkmntdev(void)
{
	dev_t cachefs_dev;
	int old_spl;
	minor_t minor_num;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_mkmntdev, current_pid(),
		0, 0);
	old_spl = mutex_spinlock( &cachefs_minor_lock );
	if ((minor_num = get_minor_number()) == -1) {
		cmn_err(CE_WARN, "CacheFS: out of minor device numbers\n");
		return((dev_t)-1);
	}
	mutex_spinunlock( &cachefs_minor_lock, old_spl );
	cachefs_dev = makedev(cachefs_major, minor_num);

	return (cachefs_dev);
}

/*
 * map a back FS fsid to a cachefs fsid
 */
dev_t
make_cachefs_fsid(fscache_t *fscp, dev_t back_fsid)
{
	int old_spl;
	struct fsidmap *fsid;
	dev_t cachefs_fsid;
	int minor;
	long slot = back_fsid % FSID_MAP_SLOTS;

	mutex_enter(&fscp->fs_fsidmaplock);
	for (fsid = fscp->fs_fsidmap[slot]; fsid; fsid = fsid->fsid_next) {
		if (fsid->fsid_back == back_fsid) {
			mutex_exit(&fscp->fs_fsidmaplock);
			return fsid->fsid_cachefs;
		}
	}
	fsid = (struct fsidmap *)CACHEFS_ZONE_ALLOC(Cachefs_fsidmap_zone, KM_SLEEP);
	fsid->fsid_back = back_fsid;
	old_spl = mutex_spinlock( &cachefs_minor_lock );
	if ((minor = get_minor_number()) == -1) {
		cmn_err(CE_WARN, "CacheFS: out of minor device numbers\n");
	}
	mutex_spinunlock( &cachefs_minor_lock, old_spl );
	cachefs_fsid = makedev(cachefs_major, minor);
	fsid->fsid_cachefs = cachefs_fsid;
	fsid->fsid_next = fscp->fs_fsidmap[slot];
	fscp->fs_fsidmap[slot] = fsid;
	mutex_exit(&fscp->fs_fsidmaplock);
	return(cachefs_fsid);
}

#if _MIPS_SIM == _ABI64
/* ARGSUSED */
static int
irix5_to_cachefs_mountargs(enum xlate_mode mode, void *to, int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_cachefs_mountargs, cachefs_mountargs);

	/*
	 * only the pointers require translating
	 */
	target->cfs_cacheid = (char *)(__psint_t)source->cfs_cacheid;
	target->cfs_cachedir = (char *)(__psint_t)source->cfs_cachedir;
	target->cfs_backfs = (char *)(__psint_t)source->cfs_backfs;
	target->cfs_options = source->cfs_options;
	target->cfs_acregmin = source->cfs_acregmin;
	target->cfs_acregmax = source->cfs_acregmax;
	target->cfs_acdirmin = source->cfs_acdirmin;
	target->cfs_acdirmax = source->cfs_acdirmax;

	return 0;
}
#endif /* _ABI64 */

/*ARGSUSED*/
static int
cachefs_mount(vfs_t *vfsp, bhv_desc_t *bdp, vnode_t *mvp, struct mounta *uap, cred_t *cr)
{
	int free_cacheid = 1;
	enum backop_stat status;
	fid_t frontfid;
	int i;
	int vfs_flag = 0;
	int cnflags = CN_ROOT;
	struct cachefs_mountargs ma;
	vnode_t *cachedirvp = NULL;
	vnode_t *backrootvp = NULL;
	vnode_t *frontrootvp = NULL;
	vnode_t *frontdirvp = NULL;
	vnode_t *cacheidvp = NULL;
	cachefscache_t *cachep = NULL;
	fscache_t *fscp = NULL;
	cnode_t *cp = NULL;
	vattr_t *attr = NULL;
	dev_t cachefs_dev;			/* devid for this mount */
	int error = 0;
	int newcache = 0;
	char *cachedir = NULL;
	size_t cachedir_len;
	size_t cacheid_len;
	char *cacheid = NULL;
	int ospl;
	fid_t *backfid = NULL;
	struct statvfs frontsb;
	struct statvfs backsb;
	fileheader_t *fhp = NULL;
	int fiderr;

	CFS_DEBUG(CFSDEBUG_VFSOP,
		printf("cachefs_mount: ENTER cachefs_mntargs %p\n", uap->dataptr));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_mount, current_pid(),
		vfsp, mvp);
	ASSERT(FILEHEADER_SIZE >= sizeof(fileheader_t));

	ASSERT((short)sizeof(vattr_t) <= kmem_zone_unitsize(Cachefs_attr_zone));
	CACHEFS_STATS->cs_vfsops++;
	/*
	 * Make sure we're root
	 */
	if (!_CAP_ABLE(CAP_MOUNT_MGT)) {
		error = EPERM;
		goto out;
	}

	/*
	 * make sure we're mounting on a directory
	 */
	if (mvp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}

	/*
	 * Assign a unique device id to the mount
	 */
	cachefs_dev = cachefs_mkmntdev();
	if (cachefs_dev == -1) {
		error = ENOSPC;
		goto out;
	}
	/*
	 * In IRIX, the filesystem independent layer handles the checking
	 * for multiple mounts on the same mount point.
	 */

	/*
	 * Copy in the arguments
	 */
	error = COPYIN_XLATE(uap->dataptr, &ma, MIN(uap->datalen, sizeof ma),
		 irix5_to_cachefs_mountargs, get_current_abi(), 1);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: unable to copy in mount args: %d\n", error));
		goto out;
	}

	if ((ma.cfs_options & (CFS_WRITE_AROUND|CFS_DUAL_WRITE)) == 0) {
		error = EINVAL;
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: invalid option flags 0x%x\n",
				ma.cfs_options));
		goto out;
	}
	/*
	 * Copy in the name of the cache directory
	 */
	cachedir = CACHEFS_ZONE_ALLOC(Cachefs_path_zone, KM_SLEEP);
	error = copyinstr(ma.cfs_cachedir, cachedir, MAXPATHLEN, &cachedir_len);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: unable to copy in cachedir name: %d\n",
				error));
		goto out;
	}
	CFS_DEBUG(CFSDEBUG_VFSOP,
		if (ma.cfs_options & CFS_DISCONNECT)
			printf("cachefs_mount: disconnected mode mount\n"));

	/*
	 * Copy in the cache ID name
	 */
	cacheid = CACHEFS_ZONE_ALLOC(Cachefs_path_zone, KM_SLEEP);
	error = copyinstr(ma.cfs_cacheid, cacheid, MAXPATHLEN, &cacheid_len);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: unable to copy in cacheid name: %d\n",
				error));
		goto out;
	}

	/*
	 * Get the cache directory vnode.
	 * The cache directory is expected to exist.  Thus, any error is returned
	 * to the user.
	 */
	error = lookupname(cachedir, UIO_SYSSPACE, FOLLOW, NULLVPP, &cachedirvp,
				NULL);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: lookup error for cache dir %s: %d\n",
				cachedir, error));
		goto out;
	}

	/*
	 * Make sure the thing we just looked up is a directory.
	 */
	if (cachedirvp->v_type != VDIR) {
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: invalid cachedir %s\n", cachedir));
		cmn_err(CE_WARN, "cachefs_mount: cachedir not a directory\n");
		error = EINVAL;
		goto out;
	}

lookup_back:
	if (!(ma.cfs_options & CFS_NO_BACKFS)) {
		/*
		 * Get the back file system root vnode.
		 * This also is expected to exist.
		 */
		error = lookupname(ma.cfs_backfs, UIO_USERSPACE, FOLLOW,
				NULLVPP, &backrootvp, NULL);
		if (error) {
			CFS_DEBUG(CFSDEBUG_VFSOP,
				printf("cachefs_mount: lookup error for back FS %s: %d\n",
					ma.cfs_backfs, error));
			goto out;
		}

		/*
		 * Make sure the thing we just looked up is a directory
		 * and a root of a file system.
		 */
		if (backrootvp->v_type != VDIR || !(backrootvp->v_flag & VROOT)) {
			CFS_DEBUG(CFSDEBUG_VFSOP,
				printf("cachefs_mount: invalid backpath %s\n", ma.cfs_backfs));
			cmn_err(CE_WARN, "cachefs_mount: backpath not a directory\n");
			error = EINVAL;
			goto out;
		}
		NET_VFS_STATVFS(backrootvp->v_vfsp, &backsb, NULL, BACKOP_BLOCK,
			error, status);
		if (error) {
			CFS_DEBUG(CFSDEBUG_VFSOP,
				printf("cachefs_mount: statvfs error %d for %s\n", error,
					ma.cfs_backfs));
			cmn_err(CE_WARN, "CacheFS mount: statvfs failed for %s\n",
				ma.cfs_backfs);
			goto out;
		}
		if (backsb.f_flag | ST_NOTRUNC) {
			vfs_flag |= VFS_NOTRUNC;
		}
		if (backsb.f_flag | ST_RDONLY) {
			vfs_flag |= VFS_RDONLY;
		}
		attr = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
		/*
		 * Get the attributes for the root from the back FS.
		 */
		attr->va_mask = AT_ALL;
		NET_VOP_GETATTR(backrootvp, attr, 0, cr, BACKOP_BLOCK, error, status);
		if (error) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_mount: unable to get back root attrs: %d\n",
					error));
			goto out;
		}
		NET_VOP_FID(backrootvp, &backfid, BACKOP_BLOCK, error, status);
		if (error || (backfid == NULL)) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_mount: unable to get back root file ID: %d\n",
					error));
			if (!error) {
				error = ESTALE;
			}
			goto out;
		}
	} else if (!(ma.cfs_options & CFS_DISCONNECT)) {
		error = EINVAL;
		goto out;
	}
	VFS_STATVFS(cachedirvp->v_vfsp, &frontsb, NULL, error);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: statvfs error %d for %s\n", error,
				cachedir));
		cmn_err(CE_WARN, "CacheFS mount: statvfs failed for %s\n", cachedir);
		goto out;
	}

	/*
	 * Lock out other mounts and unmounts until we safely have
	 * a mounted fscache object.
	 */
	mutex_enter(&cachefs_cachelock);

	/*
	 * Find the cache structure
	 */
	cachep = cachefscache_list_find(cachedir);

	/* if the cache object does not exist, then create it */
	if (cachep == NULL) {
		error = cachefs_cache_create(&cachedir, &cachep, cachedirvp);
		if (error) {
			mutex_exit(&cachefs_cachelock);
			CFS_DEBUG(CFSDEBUG_VFSOP,
				printf("cachefs_mount: failed to create cachefscache: %d\n",
					error));
			goto out;
		}
		ASSERT(cachep);

		cachep->c_next = cachefs_cachelist;
		cachefs_cachelist = cachep;
		/*
		 * indicate that this is a completely new cache
		 */
		newcache = 1;
	} else if (error = activate_cache(cachep)) {
		mutex_exit(&cachefs_cachelock);
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: unable to activate cache %s, error %d\n",
				cachedir, error));
		goto out;
	}

	/*
	 * Look up the cacheid in the cache directory.  It should have been created
	 * by the mount command if it did not already exist.
	 */
	VOP_LOOKUP(cachedirvp, cacheid, &cacheidvp, NULL, 0,
		NULL, sys_cred, error);
	if (error) {
		cmn_err(CE_WARN, "CacheFS mount: lookup error %d for %s/%s\n",
			error, cachep->c_name, cacheid);
		goto out1;
	}
	/*
	 * The cacheid vnode must be a directory vnode.
	 */
	if (cacheidvp->v_type != VDIR) {
		cmn_err(CE_WARN, "CacheFS mount: %s/%s is not a directory\n",
			cachep->c_name, cacheid);
		error = ENOTDIR;
		goto out1;
	}
	/*
	 * look up the fscache
	 * if one exists, the file system has already been mounted, return EBUSY
	 * if this is a new cache, there should be no fscache structures, so
	 * don't bother looking
	 */
	mutex_enter(&cachep->c_fslistlock);
	if (newcache || !(fscp = fscache_list_find(cachep, cacheid))) {
		ASSERT(!newcache || !cachep->c_fslist);
		fscp = fscache_create(cachep, cacheid, &ma, cacheidvp, vfsp);
		ASSERT(fscp);
		fscache_list_add(cachep, fscp);
		free_cacheid = 0;
	} else {
		free_cacheid = 1;
		error = EBUSY;
		mutex_exit(&cachep->c_fslistlock);
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: file system %s already mounted, fscp "
				"0x%p\n", cacheid, fscp));
		fscp = NULL;
		if (fhp) {
			VOP_FID2(frontrootvp, &frontfid, fiderr);
			if (!fiderr) {
				CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
			} else {
				CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
			}
			fhp = NULL;
		}
		goto out1;
	}
	mutex_exit(&cachep->c_fslistlock);

	/*
	 * Look up the front file for the root.  If it does not exist,
	 * create it.
	 */
	VOP_LOOKUP(cacheidvp, CACHEFS_ROOTNAME, &frontrootvp,
		NULL, 0, NULL, sys_cred, error);
	switch (error) {
		case 0:
			/*
			 * We found the front file for the root.  Read in the
			 * file header.
			 */
			error = cachefs_read_file_header(frontrootvp, &fhp, VDIR, cachep);
			if (!error) {
				if (!backrootvp) {
					if (fhp->fh_metadata.md_state & MD_NOTRUNC) {
						vfs_flag |= VFS_NOTRUNC;
					}
					ASSERT(valid_file_header(fhp, NULL));
				}
				ASSERT((fhp->fh_metadata.md_state & MD_INIT) ||
					((vfs_flag & VFS_NOTRUNC) &&
					(fhp->fh_metadata.md_state & MD_NOTRUNC)) ||
					(!(vfs_flag & VFS_NOTRUNC) &&
					!(fhp->fh_metadata.md_state & MD_NOTRUNC)));
				break;
			} else {
				VN_RELE(frontrootvp);
				(void)cachefs_remove_frontfile(cacheidvp, CACHEFS_ROOTNAME);
				fhp = NULL;
				/*
				 * Fall through to ENOENT processing.
				 */
			}
		case ENOENT:
			if (backrootvp) {
				ASSERT(backfid);
				error = cachefs_create_frontfile(cachep, cacheidvp,
					CACHEFS_ROOTNAME, &frontrootvp, backfid, &fhp, VDIR);
				if ( error ) {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_mount: root file create error %d "
							"for %s/%s/%s\n", error, cachep->c_name,
							cacheid, CACHEFS_ROOTNAME));
					ASSERT(!fhp);
					goto out1;
				}
				if (backsb.f_flag | ST_NOTRUNC) {
					vfs_flag |= VFS_NOTRUNC;
					fhp->fh_metadata.md_state |= MD_NOTRUNC;
					cnflags |= CN_UPDATED;
					ASSERT(cachefs_zone_validate((caddr_t)fhp,
						FILEHEADER_BLOCK_SIZE(cachep)));
					ASSERT(valid_file_header(fhp, NULL));
				}
				if (backsb.f_flag | ST_RDONLY) {
					vfs_flag |= VFS_RDONLY;
				}
			} else {
				/*
				 * no root front file, so we must wait for the back FS
				 * mount to complete
				 */
				mutex_exit(&cachefs_cachelock);
				ospl = mutex_spinlock(&fscp->fs_fslock);
				switch (sv_wait_sig(&fscp->fs_reconnect, (PZERO+1),
					&fscp->fs_fslock, ospl)) {
						case 0:
							ma.cfs_options &= ~CFS_NO_BACKFS;
							CFS_DEBUG(CFSDEBUG_VFSOP,
								if (ma.cfs_options & CFS_DISCONNECT)
									printf("cachefs_mount: looking up back "
										"FS\n"));
							goto lookup_back;
						case -1:
							error = EINTR;
							mutex_enter(&cachefs_cachelock);
							goto out1;
						default:
							error = EINVAL;
							mutex_enter(&cachefs_cachelock);
							goto out1;
				}
			}
			break;
		default:
			CACHEFS_STATS->cs_fronterror++;
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_mount: root file lookup error %d for "
					"%s/%s/%s\n", error, cachep->c_name, cacheid,
					CACHEFS_ROOTNAME));
			ASSERT(!fhp);
			goto out1;
	}
	ASSERT(backfid || fhp);
	/*
	 * Look up the directory for the root.  If it does not exist,
	 * create it.
	 */
	error = cachefs_lookup_frontdir(cacheidvp,
		backfid ? backfid : &fhp->fh_metadata.md_backfid, &frontdirvp);
	switch (error) {
		case 0:
			break;
		case ENOENT:
			error = cachefs_create_frontdir(cachep, cacheidvp,
				backfid ? backfid : &fhp->fh_metadata.md_backfid, &frontdirvp);
			if ( error ) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_mount: root directory create "
						"error %d for %s/%s\n", error, cachep->c_name,
						cacheid));
				if (fhp) {
					VOP_FID2(frontrootvp, &frontfid, fiderr);
					if (!fiderr) {
						CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
					} else {
						CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
					}
					fhp = NULL;
				}
				goto out1;
			}
			break;
		default:
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_mount: cache dir lookup error %d for "
					"%s/%s\n", error, cachep->c_name, cacheid));
			if (fhp) {
				VOP_FID2(frontrootvp, &frontfid, fiderr);
				if (!fiderr) {
					CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
				} else {
					CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
				}
				fhp = NULL;
			}
			goto out1;
	}

	if (attr) {
		/* record the back file system block size */
		fscp->fs_bsize = attr->va_blksize;
		/*
		 * Set the bmap size for this FS to be the least common multiple of
		 * the front and back block sizes.
		 */
		for (i = 1, fscp->fs_bmapsize = MAX(fscp->fs_bsize, cachep->c_bmapsize);
			(fscp->fs_bmapsize * i) % MIN(fscp->fs_bsize, cachep->c_bmapsize);
			i++) {
				;
		}
		fscp->fs_bmapsize *= i;
		ASSERT(((fscp->fs_bmapsize * i) %
			MIN(fscp->fs_bsize, cachep->c_bmapsize) == 0));
	} else {
		ASSERT(fhp);
		fscp->fs_bsize = cachep->c_bsize;
		fscp->fs_bmapsize = cachep->c_bmapsize;
	}
	CFS_DEBUG(CFSDEBUG_VFSOP,
		printf("cachefs_mount: fs_bsize %lld, fs_bmapsize %lld, c_bsize %lld, "
			"c_bmapsize %lld\n", fscp->fs_bsize, fscp->fs_bmapsize,
			cachep->c_bsize, cachep->c_bmapsize));
	ASSERT(fscp->fs_bsize > 0);
	ASSERT(fscp->fs_bmapsize > 0);
	ASSERT(cachep->c_bsize > 0);
	ASSERT(cachep->c_bmapsize > 0);
	ASSERT((fscp->fs_bsize & (fscp->fs_bsize - 1)) == 0);
	ASSERT((fscp->fs_bmapsize & (fscp->fs_bmapsize - 1)) == 0);
	ASSERT((cachep->c_bsize & (cachep->c_bsize - 1)) == 0);
	ASSERT((cachep->c_bmapsize & (cachep->c_bmapsize - 1)) == 0);

	vfs_insertbhv(vfsp, &fscp->fs_bhv, &cachefs_vfsops, fscp);
	vfsp->vfs_dev = cachefs_dev;
	vfsp->vfs_fsid.val[0] = cachefs_dev;
	vfsp->vfs_fsid.val[1] = cachefsfstyp;
	vfsp->vfs_fstype = cachefsfstyp;
	vfsp->vfs_bsize = cachep->c_bmapsize;
	vfsp->vfs_flag |= vfs_flag;

	/*
	 * fill in the front and back vfs pointers as we'll need them
	 * during makecachefsnode
	 */
	ospl = mutex_spinlock(&fscp->fs_fslock);
	fscp->fs_frontvfsp = frontrootvp->v_vfsp;
	if (backrootvp) {
		fscp->fs_backvfsp = backrootvp->v_vfsp;
	} else {
		fscp->fs_backvfsp = NULL;
	}
	mutex_spinunlock(&fscp->fs_fslock, ospl);

	/* make a cnode for the root of the file system */
	error = makecachefsnode( (cnode_t *)NULL,
		backfid ? backfid : &fhp->fh_metadata.md_backfid, fscp, fhp,
		frontrootvp, frontdirvp, backrootvp, attr, cr, cnflags, &cp);
	if (error) {
		ASSERT(!cp);
		if (fhp) {
			VOP_FID2(frontrootvp, &frontfid, fiderr);
			if (!fiderr) {
				CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
			} else {
				CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
			}
			fhp = NULL;
		}
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_mount: makecachefsnode failed, error %d\n", error));
		goto out1;
	} else {
		fhp = NULL;
	}

	/* stick the root cnode in the fscache object */
	ospl = mutex_spinlock(&fscp->fs_fslock);
	fscp->fs_root = cp;
	fscp->fs_rootvp = CTOV(cp);
	VN_FLAGSET(fscp->fs_rootvp, VROOT);
	fscp->fs_rootvp->v_type = CTOV(cp)->v_type;
	ASSERT(fscp->fs_rootvp->v_type == VDIR);
	ASSERT( fscp->fs_rootvp->v_count > 0 );
	mutex_spinunlock(&fscp->fs_fslock, ospl);

out1:
	if (error) {
		if (fscp) {
			mutex_enter(&cachep->c_fslistlock);
			fscache_list_remove(cachep, fscp);
			mutex_exit(&cachep->c_fslistlock);
			fscache_destroy(fscp);
			fscp = NULL;
		}
		/*
		 * If there was an error and we just created this cache, remove it from
		 * the list and destroy it.
		 */
		if (newcache && cachep) {
			ASSERT(cachefs_cachelist == cachep);
			cachefs_cachelist = cachep->c_next;
			cachep->c_next = NULL;
			cachefs_cache_destroy(cachep);
		}
	}
	/* allow other mounts and unmounts to proceed */
	mutex_exit(&cachefs_cachelock);

out:
	ASSERT(!fhp);
	ASSERT(!error || !fscp);
	/*
	 * Cleanup our mess
	 */
	if (backfid)
		freefid(backfid);
	if (cachedir)
		CACHEFS_ZONE_FREE(Cachefs_path_zone, cachedir);
	if (free_cacheid && cacheid)
		CACHEFS_ZONE_FREE(Cachefs_path_zone, cacheid);
	if (cachedirvp != NULL)
		VN_RELE(cachedirvp);
	if (backrootvp != NULL)
		VN_RELE(backrootvp);
	if (frontrootvp != NULL)
		VN_RELE(frontrootvp);
	if (frontdirvp != NULL)
		VN_RELE(frontdirvp);
	if (cacheidvp)
		VN_RELE(cacheidvp);
	if (attr)
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, attr);

	CFS_DEBUG(CFSDEBUG_VFSOP,
		printf("cachefs_mount: EXIT, error %d\n", error));

	/* for some reason, REFERENCED does not
	 * seem to work for this variable, so we
	 * do this to quiet the compiler.
	 */
	status = status;

	return (error);
}

/*
 * VFS_MOUNT
 */
/*ARGSUSED*/
static int
cachefs_vfsmount(vfs_t *vfsp, vnode_t *mvp, struct mounta *uap, char *attrs, cred_t *cr)
{
	return(cachefs_mount(vfsp, NULL, mvp, uap, cr));
}

/*
 * VFS_MNTUPDATE
 */
static int
cachefs_mntupdate(bhv_desc_t *bdp, vnode_t *mvp, struct mounta *uap, cred_t *cr)
{
	return(cachefs_mount(bhvtovfs(bdp), bdp, mvp, uap, cr));
}

/*
 * Complete a disconnected mount
 */
/*ARGSUSED*/
int
cachefs_complete_mount(void *argp, rval_t *rvp)
{
	struct cachefs_mountargs ma;
	vnode_t *backrootvp = NULL;
	cachefscache_t *cachep = NULL;
	fscache_t *fscp = NULL;
	int error = 0;
	char *cachedir = NULL;
	size_t cachedir_len;
	size_t cacheid_len;
	char *cacheid = NULL;

	CFS_DEBUG(CFSDEBUG_VFSOP,
		printf("cachefs_complete_mount: ENTER cachefs_mntargs %p\n",
			argp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_complete_mount,
		current_pid(), argp, NULL);
	/*
	 * Make sure we're root
	 */
	if (!_CAP_ABLE(CAP_MOUNT_MGT)) {
		error = EPERM;
		goto out;
	}

	/*
	 * Copy in the arguments
	 */
	error = COPYIN_XLATE(argp, &ma, sizeof ma,
		 irix5_to_cachefs_mountargs, get_current_abi(), 1);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP, printf(
			"cachefs_complete_mount: unable to copy in mount args: %d\n",
			error));
		goto out;
	}

	/*
	 * remounts are allowed, but only to add the back FS
	 */
	if (!(ma.cfs_options & CFS_ADD_BACK)) {
		error = EINVAL;
		goto out;
	}

	if ((ma.cfs_options & CFS_ADD_BACK) && !(ma.cfs_options & CFS_DISCONNECT)) {
		error = EINVAL;
		goto out;
	}

	if (!ma.cfs_backfs) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Copy in the name of the cache directory
	 */
	cachedir = CACHEFS_ZONE_ALLOC(Cachefs_path_zone, KM_SLEEP);
	error = copyinstr(ma.cfs_cachedir, cachedir, MAXPATHLEN, &cachedir_len);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP, printf(
			"cachefs_complete_mount: unable to copy in cachedir name: %d\n",
			error));
		goto out;
	}
	CFS_DEBUG(CFSDEBUG_VFSOP,
		if (ma.cfs_options & (CFS_DISCONNECT | CFS_ADD_BACK))
			printf("cachefs_complete_mount: %s %s\n",
				(ma.cfs_options & CFS_DISCONNECT) ? "CFS_DISCONNECT" : "",
				(ma.cfs_options & CFS_ADD_BACK) ? "CFS_ADD_BACK" : ""));

	/*
	 * Copy in the cache ID name
	 */
	cacheid = CACHEFS_ZONE_ALLOC(Cachefs_path_zone, KM_SLEEP);
	error = copyinstr(ma.cfs_cacheid, cacheid, MAXPATHLEN, &cacheid_len);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP, printf(
			"cachefs_complete_mount: unable to copy in cacheid name: %d\n",
			error));
		goto out;
	}

	/*
	 * Get the back file system root vnode.
	 * This also is expected to exist.
	 */
	error = lookupname(ma.cfs_backfs, UIO_USERSPACE, FOLLOW,
			NULLVPP, &backrootvp, NULL);
	if (error) {
		CFS_DEBUG(CFSDEBUG_VFSOP,
			printf("cachefs_complete_mount: lookup error for back FS %s: %d\n",
				ma.cfs_backfs, error));
		goto out;
	}

	/*
	 * Make sure the thing we just looked up is a directory
	 * and a root of a file system.
	 */
	if (backrootvp->v_type != VDIR || !(backrootvp->v_flag & VROOT)) {
		CFS_DEBUG(CFSDEBUG_VFSOP, printf(
			"cachefs_complete_mount: invalid backpath %s\n", ma.cfs_backfs));
		cmn_err(CE_WARN, "cachefs_complete_mount: backpath not a directory\n");
		error = EINVAL;
		goto out;
	}

	/*
	 * Lock out unmounts until we have safely modified the fscache object.
	 */
	mutex_enter(&cachefs_cachelock);

	/*
	 * locate the cachefscache structure
	 */
	if (cachep = cachefscache_list_find(cachedir)) {
		/*
		 * having the cachefscache structure, locate the fscache,
		 * set the back FS vfs pointer, and wake up any processes
		 * waiting for this pointer
		 */
		mutex_enter(&cachep->c_fslistlock);
		if (fscp = fscache_list_find(cachep, cacheid)) {
			fscp->fs_backvfsp = backrootvp->v_vfsp;
			sv_broadcast(&fscp->fs_reconnect);
		} else {
			error = EINVAL;
		}
		mutex_exit(&cachep->c_fslistlock);
	} else {
		error = EINVAL;
	}
	mutex_exit(&cachefs_cachelock);
out:
	if (cachedir)
		CACHEFS_ZONE_FREE(Cachefs_path_zone, cachedir);
	if (cacheid)
		CACHEFS_ZONE_FREE(Cachefs_path_zone, cacheid);
	if (backrootvp != NULL)
		VN_RELE(backrootvp);
	CFS_DEBUG(CFSDEBUG_VFSOP,
		printf("cachefs_complete_mount: EXIT, error %d\n", error));
	return (error);
}


#ifdef DEBUG
static void
printbusy(struct fscache *fscp)
{
	register int i;
	register struct cnode *cp;
	vnode_t *vp;

	printf("busy cnodes in fscache %s\n", fscp->fs_cacheid);
	for (i = 0; i < CNODE_BUCKET_SIZE; i++) {
		for (cp = fscp->fs_cnode[i]; cp != NULL; cp = cp->c_hash) {
			if ((cp->c_flags & (CN_ROOT|CN_DESTROY)) == 0) {
				vp = CTOV(cp);
				printf("cnode %p: count: %lu\n", cp, vp ? vp->v_count : 0);
			}
		}
	}
}
#endif /* DEBUG */

int
active_cache_count(void)
{
	int found = 0;
	struct cachefscache *cachep;

	for (cachep = cachefs_cachelist; cachep; cachep = cachep->c_next) {
		if (cachep->c_refcnt != 0) {
			ASSERT(cachep->c_fslist != NULL);
			found++;
		}
	}
	return(found);
}

/* ARGSUSED */
static int
cachefs_unmount(bhv_desc_t *bdp, int flags, cred_t *cr)
{
	cnode_t *rootcp;
	fscache_t *fscp = BHVTOFSCACHE(bdp);
	struct cachefscache *cachep = fscp->fs_cache;
	int ospl;
	int active;
	vnode_t *rootvp;
#ifdef DEBUG
	extern int cachefs_cnode_count;
#endif /* DEBUG */

	CFS_DEBUG(CFSDEBUG_VFSOP,
		printf("cachefs_unmount: ENTER fscp %p\n", fscp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_unmount, current_pid(),
		bhvtovfs(bdp), flags);
	CACHEFS_STATS->cs_vfsops++;
	if (!_CAP_ABLE(CAP_MOUNT_MGT))
		return (EPERM);

	/* lock out other unmounts, mount, and replacement */
	mutex_enter(&cachefs_cachelock);

	/*
	 * flush data while the async daemons are still running
	 * Wait for any sync in progress to finish, then set fs_sync_running
	 * to make all future sync skip this fscache.  This will work
	 * because fscache_sync is only called from here and from
	 * cachefs_cache_sync, here is the only place where SYNC_WAIT
	 * is used, and we are guaranteed that only one unmount will be
	 * in progress for this fscace.
	 */
	mutex_enter(&cachep->c_fslistlock);
	while (fscp->fs_sync_running) {
		/*
		 * This is an unmount and sync is running.  Wait for it.
		 */
		sv_wait(&fscp->fs_sync_wait, PZERO, &cachep->c_fslistlock, 0);
		mutex_enter(&cachep->c_fslistlock);
	}
	fscp->fs_sync_running = 1;
	mutex_exit(&cachep->c_fslistlock);
	fscache_sync(fscp, SYNC_WAIT | SYNC_CLOSE);

	/*
	 * No active cnodes on this cache && rootvp refcnt == 1
	 * get c_fslistlock to prevent a race with cachefs_cache_sync
	 */
	mutex_enter(&cachep->c_fslistlock);
	ospl = mutex_spinlock(&fscp->fs_fslock);
	if ((fscp->fs_vnoderef > 1) || (fscp->fs_rootvp->v_count != 1)) {

		CFS_DEBUG(CFSDEBUG_VFSOP, if (fscp->fs_vnoderef > 1) printbusy(fscp));

		fscp->fs_sync_running = 0;
		mutex_spinunlock(&fscp->fs_fslock, ospl);
		mutex_exit(&cachep->c_fslistlock);
		mutex_exit(&cachefs_cachelock);

		return (EBUSY);
	}
	rootvp = fscp->fs_rootvp;
	rootcp = fscp->fs_root;
	vn_bhv_remove(VN_BHV_HEAD(CTOV(rootcp)), CTOBHV(rootcp));
	rootcp->c_fscache = NULL;
	rootcp->c_vnode = NULL;
	rootcp->c_flags |= CN_DESTROY;
	rootcp->c_refcnt = 0;
	ASSERT(fscp->fs_vnoderef > 0);
	fscp->fs_vnoderef--;
	mutex_spinunlock(&fscp->fs_fslock, ospl);
	/*
	 * remove the fscache structure form the list for the cache
	 */
	fscache_list_remove(cachep, fscp);

	mutex_exit(&cachep->c_fslistlock);

	/* get rid of the root cnode */
	ASSERT(!rootcp->c_inhash);
	vn_free(rootvp);
	cachefs_cnode_free(rootcp);

	/*
	 * it is safe to do an unprotected check of c_refcnt here because
	 * it will only be incremented while cachefs_cachelock is held and
	 * we hold that lock here
	 */
	if (!cachep->c_refcnt) {
		/*
		 * halt the async daemons if there are no more mounts
		 * for this cache
		 */
		while (cachefs_async_halt(&cachep->c_workq) == EBUSY) {
			cmn_err(CE_WARN,
				"CacheFS unmount: waiting for cache async queue...\n");
		}
		deactivate_cache(cachep);
	}

	fscache_destroy(fscp);

	active = active_cache_count();
	/*
	 * If there are no mounted file systems, there should be a known
	 * amount of allocated memory.
	 */
	if (!active) {
		ASSERT(cachefs_cnode_count == 0);
		cachefs_mem_check();
	}

	mutex_exit(&cachefs_cachelock);

	CFS_DEBUG(CFSDEBUG_VFSOP, printf("cachefs_unmount: EXIT\n"));
	return (0);
}

static int
cachefs_root(bhv_desc_t *bdp, vnode_t **vpp)
{
	/*LINTED alignment okay*/
	struct fscache *fscp = BHVTOFSCACHE(bdp);

	ASSERT(fscp != NULL);
	ASSERT(fscp->fs_rootvp != NULL);
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_root, current_pid(),
		bhvtovfs(bdp), vpp);
	CACHEFS_STATS->cs_vfsops++;
	*vpp = fscp->fs_rootvp;
	VN_HOLD(*vpp);
	CNODE_TRACE(CNTRACE_HOLD, fscp->fs_root, (void *)cachefs_root,
		(*vpp)->v_count, 0);
	return (0);
}

/*
 * Get file system statistics.
 */
/* ARGSUSED */
static int
cachefs_statvfs(register bhv_desc_t *bdp, struct statvfs *sbp, vnode_t *vp)
{
	struct fscache *fscp = BHVTOFSCACHE(bdp);
	/*REFERENCED*/
	struct statvfs sb;
	int error;
	struct vfs *vfsp;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_statvfs, current_pid(),
		bhvtovfs(bdp), sbp);
	CACHEFS_STATS->cs_vfsops++;

	/*
	 * Fill in the statvfs structure of the caller fron the back FS since
	 * it dictates most of the values.
	 */
	(void)BACK_VFS_STATVFS(fscp, sbp, BACKOP_BLOCK, &error);
	if (error)
		return (error);

	/*
	 * There are a few fields which have cachefs-specific values.  Fill
	 * these in now: f_basetype, f_fstr, f_fsid, and f_flag.
	 */
	vfsp = bhvtovfs(bdp);
	strcpy(sbp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	/*
	 * fill in f_fstr with the cache id
	 */
	strncpy(sbp->f_fstr, fscp->fs_cacheid, sizeof(sbp->f_fstr) - 1);
	sbp->f_fsid = vfsp->vfs_fsid.val[0];
	sbp->f_flag = vf_to_stf(vfsp->vfs_flag);

	return (0);
}

/*
 * queue a request to sync the given fscache
 */
static void
queue_sync(struct cachefscache *cachep, cred_t *cr)
{
	struct cachefs_req *rp;
	int ospl;
#ifdef DEBUG
	int error;
#endif /* DEBUG */

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)queue_sync, current_pid(),
		cachep, cr);
	ASSERT(VALID_ADDR(cachep));
	ASSERT(VALID_ADDR(cr));
	ospl = mutex_spinlock(&cachep->c_contentslock);
	if (cachep->c_syncs < cachep->c_refcnt) {
		mutex_spinunlock(&cachep->c_contentslock, ospl);
		cachep->c_syncs++;
		/*LINTED alignment okay*/
		rp = (struct cachefs_req *)CACHEFS_ZONE_ZALLOC(Cachefs_req_zone,
			KM_SLEEP);
		rp->cfs_cmd = CFS_CACHE_SYNC;
		rp->cfs_cr = cr;
		rp->cfs_req_u.cu_fs_sync.cf_cachep = cachep;
		crhold(rp->cfs_cr);
#ifdef DEBUG
		error = cachefs_addqueue(rp, &cachep->c_workq);
		ASSERT(error == 0);
#else /* DEBUG */
		(void)cachefs_addqueue(rp, &cachep->c_workq);
#endif /* DEBUG */
	} else {
		mutex_spinunlock(&cachep->c_contentslock, ospl);
	}
}

/*ARGSUSED*/
static int
cachefs_sync(bhv_desc_t *bdp, int flag, cred_t *cr)
{
	struct fscache *fscp;
	struct cachefscache *cachep;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_sync, current_pid(),
		bhvtovfs(bdp), flag);
	ASSERT(VALID_ADDR(bhvtovfs(bdp)));
	ASSERT(VALID_ADDR(cr));
	/*
	 * queue an async request to do the sync.
	 * We always sync an entire cache (as opposed to an
	 * individual fscache) so that we have an opportunity
	 * to set the clean flag.
	 */
	CACHEFS_STATS->cs_vfsops++;
	if (bdp) {
		/*LINTED alignment okay*/
		fscp = BHVTOFSCACHE(bdp);
		queue_sync(fscp->fs_cache, cr);
	} else {
		mutex_enter(&cachefs_cachelock);
		for (cachep = cachefs_cachelist; cachep != NULL;
		    cachep = cachep->c_next) {
			queue_sync(cachep, cr);
		}
		mutex_exit(&cachefs_cachelock);
	}
	return (0);
}

/*ARGSUSED*/
static int
cachefs_vget(bhv_desc_t *bdp, vnode_t **vpp, fid_t *fidp)
{
	fid_t frontfid;
	enum backop_stat status;
	int error;
	fileheader_t *fhp = NULL;
	vnode_t *frontvp = NULL;
	vnode_t *backvp = NULL;
	vnode_t *frontdirvp = NULL;
	cnode_t *cp = NULL;
	fscache_t *fscp = BHVTOFSCACHE(bdp);
	int cnflag = 0;
	vfs_t *frontvfs = NULL;
	int fiderr;

	CFS_DEBUG(CFSDEBUG_VFSOP,
		printf("cachefs_vget: ENTER vfsp 0x%p\n", bhvtovfs(bdp)));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_vget, current_pid(),
		bhvtovfs(bdp), vpp);
	ASSERT(VALID_ADDR(fscp));
	frontvfs = FSC_TO_FRONTVFS(fscp);
	ASSERT(VALID_ADDR(frontvfs));
	*vpp = NULL;
	/*
	 * A cachefs file id is the file id of the front file.  Pass it along
	 * to the front FS to get the front vp.
	 */
	CACHEFS_STATS->cs_vfsops++;
	VFS_VGET(frontvfs, &frontvp, fidp, error);
	if (error) {
		return(error);
	} else if (!frontvp) {
		return(ESTALE);
	}
	/*
	 * Read the file header to get the file id for the back file.
	 */
	error = cachefs_read_file_header(frontvp, &fhp, VNON, fscp->fs_cache);
	if (!error) {
		CFS_DEBUG(CFSDEBUG_VFSOP,
			if (!(fhp->fh_metadata.md_state & MD_KEEP))
				printf("cachefs_vget: exported file without MD_KEEP set, vp "
					"0x%p\n", frontvp));
		if (fhp->fh_metadata.md_state & MD_MDVALID) {
			ASSERT(fhp->fh_metadata.md_backfid.fid_len > 0);
			/*
			 * Look up the cnode.  If there is not one, makecachefsnode
			 * will return ENOENT.
			 */
			error = makecachefsnode( (cnode_t *)NULL,
				&fhp->fh_metadata.md_backfid, fscp, (fileheader_t *)NULL,
				(vnode_t *)NULL, (vnode_t *)NULL, (vnode_t *)NULL,
				(vattr_t *)NULL, sys_cred, 0, &cp);
			switch (error) {
			case 0:
				ASSERT(cp);
				*vpp = CTOV(cp);
				break;
			case ENOENT:
				/*
				 * There is no cnode.  We must go to the back file system
				 * now to get the back vp before creating the cnode.
				 * This operation may succeed, fail or get a network error.
				 * For the latter, we simply create the cnode without the
				 * back vnode and fill it in later.  So, BACKOP_SUCCESS and
				 * BACKOP_NETERR are handled identically.
				 */
				status = cachefs_getbackvp(fscp, &fhp->fh_metadata.md_backfid,
					&backvp, BACKOP_NONBLOCK, &error);
				switch (status) {
					case BACKOP_FAILURE:
						break;
					case BACKOP_SUCCESS:
					case BACKOP_NETERR:
						error = 0;
						ASSERT(valid_file_header(fhp, NULL));
						cnflag = CN_UPDATED;
						if (fhp->fh_metadata.md_attributes.ca_type == VDIR) {
							error = cachefs_lookup_frontdir(
								fscp->fs_cacheidvp,
								&fhp->fh_metadata.md_backfid, &frontdirvp);
							if (error == ENOENT) {
								error = cachefs_create_frontdir(
									fscp->fs_cache, fscp->fs_cacheidvp,
									&fhp->fh_metadata.md_backfid,
									&frontdirvp);
								if (error) {
									error = 0;
									cnflag = CN_NOCACHE;
									VOP_FID2(frontvp, &frontfid, fiderr);
									if (!fiderr) {
										CACHEFS_RELEASE_FILEHEADER(&frontfid,
											fhp);
									} else {
										CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
									}
								}
							}
						}
						ASSERT(cachefs_zone_validate((caddr_t)fhp,
							FILEHEADER_BLOCK_SIZE(fscp->fs_cache)));
						error = makecachefsnode( (cnode_t *)NULL,
							&fhp->fh_metadata.md_backfid, fscp, fhp,
							frontvp, frontdirvp, backvp, (vattr_t *)NULL,
							sys_cred, cnflag, &cp);
						switch (error) {
							case 0:
								fhp = NULL;
								ASSERT(cp);
								*vpp = CTOV(cp);
								break;
							case EEXIST:
								if (cp) {
									error = 0;
									*vpp = CTOV(cp);
								} else {
									error = ESTALE;
								}
								break;
							default:
								break;
						}
				}
			default:
				break;
			}
		} else {
			error = ESTALE;
		}
	}
	if (fhp) {
		ASSERT(frontvp);
		VOP_FID2(frontvp, &frontfid, fiderr);
		if (!fiderr) {
			CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
		} else {
			CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
		}
	}
	if (frontdirvp)
		VN_RELE(frontdirvp);
	if (frontvp)
		VN_RELE(frontvp);
	if (backvp)
		VN_RELE(backvp);
	ASSERT(error || *vpp);
	CFS_DEBUG(CFSDEBUG_VFSOP,
		printf("cachefs_vget: EXIT error %d\n", error));
	return (error);
}


/*ARGSUSED*/
static int
cachefs_mountroot(vfs_t *vfsp, bhv_desc_t *bdp, whymountroot_t why)
{
	int error;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_mountroot, current_pid(),
		vfsp, why);
	CACHEFS_STATS->cs_vfsops++;
	switch (why) {
	default:
		error = ENOSYS;
		break;
	}
	return (error);
}

/* VFS_ROOTINIT */
static int
cachefs_rootinit(vfs_t *vfsp)
{
	return(cachefs_mountroot(vfsp, NULL, ROOT_INIT));
}

/* VFS_MOUNTROOT */
static int
cachefs_vfsmountroot(bhv_desc_t *bdp, whymountroot_t why)
{
	return(cachefs_mountroot(bhvtovfs(bdp), bdp, why));
}



