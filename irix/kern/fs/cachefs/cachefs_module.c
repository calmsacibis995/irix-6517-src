/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_module.c 1.13     94/02/03 SMI"
 */

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <rpc/types.h>
#include <sys/mode.h>
#include <sys/cmn_err.h>
#include "cachefs_fs.h"
#include <sys/systm.h>
#include <sys/debug.h>

extern struct vfsops cachefs_vfsops;

extern krwlock_t reclaim_lock;
mutex_t cachefs_cachelock;			/* Cache list mutex */
extern mutex_t cachefs_rename_lock;
extern lock_t cachefs_minor_lock;		/* Lock for minor device map */
#ifdef DEBUG
extern lock_t cachefs_kmem_lock;
#endif /* DEBUG */
extern lock_t cachefs_cnode_cnt_lock;
extern lock_t Fileheader_cache_lock;
extern int cachefs_major;

zone_t *Cachefs_fileheader_zone = NULL;	/* zone for file header allocations */
zone_t *Cachefs_attr_zone = NULL;		/* zone for attribute allocations */
zone_t *Cachefs_cnode_zone = NULL;		/* zone for cnode allocations */
zone_t *Cachefs_fid_zone = NULL;		/* zone for fid allocations */
zone_t *Cachefs_path_zone = NULL;		/* zone for path name allocations */
zone_t *Cachefs_fhcache_zone = NULL;	/* zone for file header cache entries */
zone_t *Cachefs_req_zone = NULL;		/* zone for async requests */
zone_t *Cachefs_fsidmap_zone = NULL;	/* zone for fsid mappings */

/*
 * front file creation attributes
 */
vattr_t Cachefs_file_attr = {
	AT_MODE | AT_SIZE,			/* va_mask */
	VREG,						/* va_type */
	(mode_t)CACHEFS_FILEMODE,	/* va_mode */
	(uid_t)0,					/* va_uid */
	(gid_t)0,					/* va_gid */
	(dev_t)0,					/* va_fsid */
	(ino_t)0,					/* va_nodeid */
	(nlink_t)0,					/* va_nlink */
	(off_t)FILEHEADER_SIZE,		/* va_size */
	{0,0},						/* va_atime */
	{0,0},						/* va_mtime */
	{0,0},						/* va_ctime */
	(dev_t)0,					/* va_rdev */
	(u_long)0,					/* va_blksize */
	(u_long)0,					/* va_nblocks */
	(u_long)0,					/* va_vcode */
	(u_long)0,					/* va_xflags */
	(u_long)0,					/* va_extsize */
	(u_long)0					/* va_nextents */
};

/*
 * front dir creation attributes
 */
vattr_t Cachefs_dir_attr = {
	AT_MODE,					/* va_mask */
	(vtype_t)0,					/* va_type */
	(mode_t)CACHEFS_DIRMODE,	/* va_mode */
	(uid_t)0,					/* va_uid */
	(gid_t)0,					/* va_gid */
	(dev_t)0,					/* va_fsid */
	(ino_t)0,					/* va_nodeid */
	(nlink_t)0,					/* va_nlink */
	(off_t)0,					/* va_size */
	{0,0},						/* va_atime */
	{0,0},						/* va_mtime */
	{0,0},						/* va_ctime */
	(dev_t)0,					/* va_rdev */
	(u_long)0,					/* va_blksize */
	(u_long)0,					/* va_nblocks */
	(u_long)0,					/* va_vcode */
	(u_long)0,					/* va_xflags */
	(u_long)0,					/* va_extsize */
	(u_long)0					/* va_nextents */
};

#ifdef DEBUG
int cnodetrace = CNTRACE_FRONTSIZE | CNTRACE_POPDIR | CNTRACE_INVAL;
int cachetrace = CACHETRACE_NONE;
int cachefsdebug = CFSDEBUG_VALIDATE | CFSDEBUG_ERROR | CFSDEBUG_FILEHEADER | CFSDEBUG_NOCACHE;
cfstrace_t Cachefs_functrace;
int cachefunctrace = CFTRACE_NONE;
#endif /* DEBUG */

int front_dio = 1;

/*
 * Cache initialization routine.  This routine should only be called
 * once.  It performs the following tasks:
 *	- Initalize all global locks
 * 	- Call sub-initialization routines (localize access to variables)
 */
int
cachefs_init(vswp, fstyp)
	struct vfssw *vswp;
	int fstyp;
{
	int i;
#ifdef DEBUG
	static boolean_t cachefs_up = B_FALSE;	/* XXX - paranoid */

	ASSERT(cachefs_up == B_FALSE);
#endif /* DEBUG */

	Cachefs_fileheader_zone = kmem_zone_init(sizeof(fileheader_t),
		"CacheFS file headers");
	Cachefs_attr_zone = kmem_zone_init(sizeof(vattr_t),
		"CacheFS file attributes");
	Cachefs_cnode_zone = kmem_zone_init(sizeof(cnode_t), "CacheFS cnodes");
	Cachefs_fid_zone = kmem_zone_init(sizeof(fid_t), "CacheFS fids");
	Cachefs_path_zone = kmem_zone_init(MAXPATHLEN + 1, "CacheFS paths");
	Cachefs_fhcache_zone = kmem_zone_init(sizeof(struct filheader_cache_entry),
		"CacheFS fileheader cache entries");
	Cachefs_req_zone = kmem_zone_init(sizeof(struct cachefs_req),
		"CacheFS async req");
	Cachefs_fsidmap_zone = kmem_zone_init(sizeof(struct fsidmap),
		"CacheFS fsid map");
	mutex_init(&cachefs_cachelock, MUTEX_DEFAULT, "cachefs cache list lock");
#ifdef DEBUG
	spinlock_init( &cachefs_kmem_lock, "cachefs kmem lock" );
#endif /* DEBUG */
	spinlock_init( &cachefs_cnode_cnt_lock, "cnode count lock" );
	mutex_init(&cachefs_rename_lock, MUTEX_DEFAULT, "cachefs rename lock");
	spinlock_init( &cachefs_minor_lock, "cachefs minor lock" );
	spinlock_init( &Fileheader_cache_lock, "cachefs fileheader cache lock" );
	/*
	 * Assign unique major number for all nfs mounts
	 */
	if ((cachefs_major = getudev()) == -1) {
		cmn_err(CE_WARN,
			"cachefs: init: can't get unique device number");
		cachefs_major = 0;
	}
#ifdef DEBUG
	cachefs_up = B_TRUE;
#endif /* DEBUG */
	vswp->vsw_vfsops = &cachefs_vfsops;
	cachefsfstyp = fstyp;
	cachefs_idbg_init();
	for (i = 0; i < maxcpus; i++) {
		if (pdaindr[i].CpuId == -1)
			continue;
		pdaindr[i].pda->cfsstat = kern_calloc(1, sizeof(struct cachefs_stats));
	}
	return (0);
}

caddr_t
cachefs_getstat(int cpu)
{
	return((caddr_t)pdaindr[cpu].pda->cfsstat);
}

int
cachefs_clearstat(void)
{
	int i;
	caddr_t ptr, newptr;

	/*
	 * make sure we have the correct permissions
	 */
	if (!_CAP_ABLE(CAP_DAC_OVERRIDE) || !_CAP_ABLE(CAP_MAC_READ) ||
		!_CAP_ABLE(CAP_MAC_WRITE) || !_CAP_ABLE(CAP_NETWORK_MGT)) {
			return(EPERM);
	}
	for (i = 0; i < maxcpus; i++) {
		if ((pdaindr[i].CpuId == -1) || !(ptr = pdaindr[i].pda->cfsstat))
			continue;
		newptr = (caddr_t)kern_calloc(1, sizeof(struct cachefs_stats));
		pdaindr[i].pda->cfsstat = newptr;
		kern_free(ptr);
	}
	return(0);
}
