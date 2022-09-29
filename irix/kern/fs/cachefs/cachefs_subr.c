/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_subr.c 1.110     94/04/21 SMI"
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
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/pfdat.h>
#include <sys/dnlc.h>
#include <sys/dirent.h>
#include <sys/acct.h>
#include <sys/runq.h>
#include <ksys/vproc.h>
#include <sys/uthread.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */

#include "cachefs_fs.h"
#include <sys/siginfo.h>
#include <sys/ksignal.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/kthread.h>
#include <sys/sched.h>
#ifdef DEBUG
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <string.h>
#include <sys/var.h>
#endif /* DEBUG */



extern cachefscache_t *cachefs_cachelist;
int	cachefsio_write( struct cnode *, struct buf *, cred_t * );
int	cachefsio_read( struct cnode *, struct buf *, cred_t * );

ino_t cachefs_check_fileno = 0;


/*
 * Cache routines
 */

cachefscache_t *
cachefscache_list_find(char *name)
{
	cachefscache_t *cachep;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefscache_list_find,
		current_pid(), name, 0);
	ASSERT(VALID_ADDR(name));
	for (cachep = cachefs_cachelist; cachep != NULL;
		cachep = cachep->c_next) {
		if ( strcmp(cachep->c_name, name) == 0 )
			break;
	}
	return(cachep);
}

int
activate_cache(cachefscache_t *cachep)
{
    int error;

	if (cachep->c_refcnt) {
		return(0);
	}
	ASSERT(!cachep->c_dirvp);
	ASSERT(!cachep->c_labelvp);
	ASSERT(cachep->c_name && VALID_ADDR(cachep->c_name));
	error = lookupname(cachep->c_name, UIO_SYSSPACE, FOLLOW, NULLVPP,
		&cachep->c_dirvp, NULL);
	if (!error) {
		VOP_LOOKUP(cachep->c_dirvp, CACHELABEL_NAME, &cachep->c_labelvp, NULL,
			0, NULL, kcred, error);
		if (!error) {
			error = cachefs_async_start(cachep);
			if (!error) {
				return(0);
			} else {
				cmn_err(CE_WARN, "CacheFS: unable to start async daemon for "
					"cache %s, error %d\n", cachep->c_name, error);
			}
		} else {
			cmn_err(CE_WARN,
				"CacheFS: unable to find cache %s label %s, error %d\n",
				cachep->c_name, CACHELABEL_NAME, error);
		}
	} else {
		cmn_err(CE_WARN, "CacheFS: unable to find cache %s, error %d\n",
			cachep->c_name, error);
	}
	if (cachep->c_dirvp) {
		VN_RELE(cachep->c_dirvp);
	}
	if (cachep->c_labelvp) {
		VN_RELE(cachep->c_labelvp);
	}
	cachep->c_dirvp = cachep->c_labelvp = NULL;
	return(error);
}

void
deactivate_cache(cachefscache_t *cachep)
{
	VN_RELE(cachep->c_dirvp);
	VN_RELE(cachep->c_labelvp);
	cachep->c_dirvp = cachep->c_labelvp = NULL;
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_create
 *
 * Description:
 *	Creates a cachefscache_t object and initializes it to
 *	be NOCACHE and NOFILL mode.
 * Arguments:
 * Returns:
 *	Returns a pointer to the created object or NULL if
 *	threads could not be created.
 * Preconditions:
 */

int
cachefs_cache_create(char **name, cachefscache_t **cachep, vnode_t *cdvp)
{
	u_int metasize = (sizeof(cachefsmeta_t) + sizeof(int) - 1) / sizeof(int);
	int error = 0;
	vnode_t *labelvp = NULL;
	vattr_t *attr = NULL;
	uio_t uio;
	iovec_t iov;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_cache_create: ENTER name %s\n", *name));
	ASSERT(VALID_ADDR(name));
	ASSERT(VALID_ADDR(*name));
	ASSERT(VALID_ADDR(cachep));
	ASSERT(VALID_ADDR(cdvp));
	ASSERT(cdvp->v_count >= 1);

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_cache_create,
		current_pid(), name, cachep);
	/* allocate zeroed memory for the object */
	*cachep = (cachefscache_t *)CACHEFS_KMEM_ZALLOC(sizeof (cachefscache_t),
		/*LINTED alignment okay*/
		KM_SLEEP);

	spinlock_init(&(*cachep)->c_contentslock, "cachefscache lock");
	mutex_init(&(*cachep)->c_fslistlock, MUTEX_DEFAULT, "cache fslist");
	sv_init(&(*cachep)->c_replacement_sv, SV_DEFAULT, "cachefs replacement sv");
	sv_init(&(*cachep)->c_repwait_sv, SV_DEFAULT, "cachefs repwait cv");
#if defined(DEBUG) && defined(_CACHE_TRACE)
	spinlock_init(&(*cachep)->c_trace.ct_lock, "cache trace lock");
#endif /* DEBUG && _CACHE_TRACE */

	/* set up the work queue and get the sync thread created */
	cachefs_workq_init(&(*cachep)->c_workq);
	if (error = cachefs_async_start(*cachep)) {
		goto out;
	}

	(*cachep)->c_flags = 0;
	(*cachep)->c_name = *name;
	*name = NULL;

	attr = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
	/* get the mode bits of the cache directory */
	attr->va_mask = AT_ALL;
	VOP_GETATTR(cdvp, attr, 0, kcred, error);
	if (error)
		goto out;

	/* ensure the mode bits are 000 to keep out casual users */
	if (attr->va_mode & S_IAMB) {
		cmn_err_tag(133,CE_WARN, "cachefs: Cache Directory Mode must be 000\n");
		error = EPERM;
		goto out;
	}

	/*
	 * record the block size for the front file system
	 * set the block size to be no smaller than a page
	 */
	(*cachep)->c_bsize = attr->va_blksize;
	(*cachep)->c_bmapsize = MAX((*cachep)->c_bsize, NBPP);
	/*
	 * If the front FS block size is not a power of two, calculate the bmap
	 * size to be the nearest power of 2 less than or equal to the front FS
	 * block size.
	 *
	 * For any integer k, k & (k - 1) will be 0 if and only if k = 2**j for
	 * some integer j.
	 */
	if (attr->va_blksize & (attr->va_blksize - 1)) {
		(*cachep)->c_bmapsize = 1;
		while ((*cachep)->c_bsize >= ((*cachep)->c_bmapsize << 1)) {
			(*cachep)->c_bmapsize <<= 1;
		}
	}
	ASSERT((*cachep)->c_bmapsize);

	/* Get the label file */
	VOP_LOOKUP(cdvp, CACHELABEL_NAME, &labelvp, NULL, 0, NULL,
		kcred, error);
	if (error) {
		cmn_err(CE_WARN,
			"CacheFS: unable to open label file %s for cache %s: error %d.\n",
			CACHELABEL_NAME, (*cachep)->c_name, error);
		goto out;
	}

	/*
	 * read in the label
	 * assume that the mount process has already locked the label file
	 */
	UIO_SETUP(&uio, &iov, (caddr_t)&(*cachep)->c_label,
		sizeof (struct cache_label), (off_t)0, UIO_SYSSPACE, FREAD, (off_t)0);
	READ_VP(labelvp, &uio, 0, kcred, &curuthread->ut_flid, error);
	if (error) {
		cmn_err(CE_WARN,
			"CacheFS: error %d when reading cache label %s for cache %s.\n",
			error, CACHELABEL_NAME, (*cachep)->c_name);
		goto out;
	} else if (uio.uio_resid) {
		error = EIO;
		cmn_err(CE_WARN,
			"cachefs: incomplete cache label for cache %s.\n",
				(*cachep)->c_name);
		goto out;
	}

	/* Verify that we can handle the version this cache was created under */
	/*
	 * If the mode has not been set, this is a newly created label file.
	 * In this case, complete the version number by filling in the size
	 * of a long and the size of the cachefs metadata structure.  This
	 * must be done in the kernel because it is the kernel that creates
	 * the cache files.  In particular, each file has a vattr_t on the
	 * front.
	 */
	if (((*cachep)->c_label.cl_cfsversion == CFSVERSION) &&
		!(*cachep)->c_label.cl_cfslongsize &&
		!(*cachep)->c_label.cl_cfsmetasize) {
			/*
			 * fill in the mode portion of the version
			 * mode is the size of long
			 * assume that the label file has been locked as writer
			 */
			(*cachep)->c_label.cl_cfsversion = CFSVERSION;
			(*cachep)->c_label.cl_cfslongsize = sizeof(long);
			(*cachep)->c_label.cl_cfsmetasize = metasize;
			/*
			 * write out the label
			 */
			UIO_SETUP(&uio, &iov, (caddr_t)&(*cachep)->c_label,
				sizeof(struct cache_label), (off_t)0, UIO_SYSSPACE, FWRITE,
				(off_t)RLIM_INFINITY);
			WRITE_VP(labelvp, &uio, 0, kcred, &curuthread->ut_flid,
				error);
			if (error) {
				cmn_err(CE_WARN,
					"CacheFS: Can't write Cache Label File for cache %s, "
					"error %d\n", (*cachep)->c_name, error);
				ASSERT(uio.uio_sigpipe == 0);
				goto out;
			}
	} else if ((*cachep)->c_label.cl_cfsversion != CFSVERSION) {
		cmn_err(CE_WARN,
			"CacheFS: Invalid Cache Version for cache %s: %d, expected %d\n",
			(*cachep)->c_name, (*cachep)->c_label.cl_cfsversion, CFSVERSION);
		error = EINVAL;
		goto out;
	} else if ((*cachep)->c_label.cl_cfslongsize != sizeof(long)) {
		cmn_err(CE_WARN,
			"CacheFS: Invalid Cache Long Size for cache %s: %d, expected %d\n",
			(*cachep)->c_name, (*cachep)->c_label.cl_cfslongsize, sizeof(long));
		error = EINVAL;
		goto out;
	} else if ((*cachep)->c_label.cl_cfsmetasize != metasize) {
		cmn_err(CE_WARN,
			"CacheFS: Invalid Cache Attribute Size for cache %s: %d, "
			"expected %d\n", (*cachep)->c_name,
			(*cachep)->c_label.cl_cfsmetasize,
			metasize);
		error = EINVAL;
		goto out;
	}

	VN_HOLD(cdvp);
	(*cachep)->c_dirvp = cdvp;
	(*cachep)->c_labelvp = labelvp;
	labelvp = NULL;
out:
	if (attr)
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, attr);
	if (labelvp != NULL) {
		VN_RELE(labelvp);
	}
	if (error) {
		cachefs_cache_destroy(*cachep);
		*cachep = NULL;
	}
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_cache_create: EXIT error %d\n", error));
	return (error);
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_destroy
 *
 * Description:
 *	Destroys the cachefscache_t object.
 * Arguments:
 *	cachep	the cachefscache_t object to destroy
 * Returns:
 * Preconditions:
 *	precond(cachep)
 */

void
cachefs_cache_destroy(cachefscache_t *cachep)
{
	int error;
	struct flock fl;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_cache_destroy: ENTER cachep 0x%p\n", cachep));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_cache_destroy,
		current_pid(), cachep, 0);
	ASSERT(VALID_ADDR(cachep));
	/* stop async threads */
	while (cachep->c_workq.wq_thread_count > 0)
		(void) cachefs_async_halt(&cachep->c_workq);

	/* stop the garbage collection thread */
	cachefs_replacement_halt(cachep);

	ASSERT(cachep->c_fslist == NULL);
	ASSERT(cachep->c_refcnt == 0);

	if (cachep->c_labelvp) {
		/* unlock the label file */
		fl.l_type = F_UNLCK;
		fl.l_whence = 0;
		fl.l_start = 0;
		fl.l_len = 0;
		fl.l_sysid = 0;
		fl.l_pid = 0;
		VOP_FRLOCK(cachep->c_labelvp, F_SETLK, &fl, FREAD, (off_t)0,
			VRWLOCK_NONE, sys_cred, error);
		if (error) {
			cmn_err(CE_WARN,
				"CacheFS: Can't unlock lock label file, error %d\n", error);
		}
		VN_RELE(cachep->c_labelvp);
	}

	if (cachep->c_dirvp)
		VN_RELE(cachep->c_dirvp);

	if (cachep->c_name)
		CACHEFS_ZONE_FREE(Cachefs_path_zone, cachep->c_name);
	spinlock_destroy(&cachep->c_contentslock);
	mutex_destroy(&cachep->c_fslistlock);
	sv_destroy(&cachep->c_replacement_sv);
	sv_destroy(&cachep->c_repwait_sv);
#if defined(DEBUG) && defined(_CACHE_TRACE)
	spinlock_destroy(&(*cachep)->c_trace.ct_lock);
#endif /* DEBUG && _CACHE_TRACE */

	/* destroy the work queue */
	cachefs_workq_destroy(&cachep->c_workq);
	bzero((caddr_t)cachep, sizeof (cachefscache_t));
	CACHEFS_KMEM_FREE(cachep, sizeof (cachefscache_t));
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_cache_destroy: EXIT\n"));
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_sync
 *
 * Description:
 *	Sync a cache which includes all of its fscaches.
 * Arguments:
 *	cachep	the cachefscache_t object to sync
 * Returns:
 * Preconditions:
 *	precond(cachep)
 *	precond(cache is in rw mode)
 */

void
cachefs_cache_sync(struct cachefscache *cachep)
{
	struct fscache *fscp;
	struct fscache **syncfsc;
	int nfscs, fscidx;
	int alloc_count;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_cache_sync: ENTER cachep 0x%p\n", cachep));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_cache_sync,
		current_pid(), cachep, 0);
	ASSERT(VALID_ADDR(cachep));

	nfscs = 0;

	mutex_enter(&cachep->c_fslistlock);
	if (cachep->c_refcnt) {
		alloc_count = cachep->c_refcnt * sizeof(struct fscache *);
		syncfsc = (struct fscache **) CACHEFS_KMEM_ALLOC(alloc_count,
				/*LINTED alignment okay*/
				KM_SLEEP);
		for (fscp = cachep->c_fslist; fscp; fscp = fscp->fs_next) {
			if (!fscp->fs_sync_running) {
				fscp->fs_sync_running = 1;
				fscache_hold(fscp);
				ASSERT(nfscs < cachep->c_refcnt);
				syncfsc[nfscs++] = fscp;
			}
		}
		ASSERT(nfscs <= cachep->c_refcnt);
		mutex_exit(&cachep->c_fslistlock);
		for (fscidx = 0; fscidx < nfscs; fscidx++) {
			fscp = syncfsc[fscidx];
			fscache_sync(fscp, SYNC_NOWAIT);
			fscache_rele(fscp);
			mutex_enter(&cachep->c_fslistlock);
			fscp->fs_sync_running = 0;
			sv_broadcast(&fscp->fs_sync_wait);
			mutex_exit(&cachep->c_fslistlock);
		}
		CACHEFS_KMEM_FREE(syncfsc, alloc_count);
	} else {
		mutex_exit(&cachep->c_fslistlock);
	}

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_cache_sync: EXIT\n"));
}

/*
 * Invalidate the cached object.  This consists of flushing and invalidating
 * all buffers, clearing the file header state and allocation map, and
 * calling the consistency mechanism to invalidate the object.
 * If buffers are to be invalidated, exclusive access to the object is
 * required.  This function will release and recuire c_rwlock if necessary.
 * As a result, this function can only be used at the beginning or the
 * end of a critical section covered by c_rwlock.
 *
 * c_rwlock will be left in the state (RW_READER or RW_WRITER) in which
 * it was held by the caller.
 */
int
cachefs_inval_object(cnode_t *cp, lockmode_t lm)
{
	int error = 0;
	int ospl;
	vnode_t *vp = NULL;

	CFS_DEBUG(CFSDEBUG_SUBR | CFSDEBUG_INVAL,
		printf("cachefs_inval_object: ENTER cp %p\n", cp));
	ASSERT(cp);
	ASSERT(CTOV(cp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_inval_object,
		current_pid(), cp, 0);
	ASSERT(VALID_ADDR(cp));
	CACHEFS_STATS->cs_inval++;

	vp = CTOV(cp);

	/*
	 * For regular files, we will be invalidating buffers, so we'll need
	 * exclusive access to the code to prevent another process from
	 * creating buffers while we're invalidating them.
	 * For all file types, we'll be truncating the front file, so we'll
	 * need exclusive access.
	 */
	switch (lm) {
		case READLOCKED:	/* read lock held */
			ASSERT(RW_READ_HELD(&cp->c_rwlock));
			rw_exit(&cp->c_rwlock);
			rw_enter(&cp->c_rwlock, RW_WRITER);
			break;
		case UNLOCKED:		/* lock needed and none is held */
			rw_enter(&cp->c_rwlock, RW_WRITER);
			break;
		case WRITELOCKED:	/* write lock held */
			ASSERT(RW_WRITE_HELD(&cp->c_rwlock));
		case NOLOCK:		/* no lock is required */
			break;
	}

	ASSERT((lm == NOLOCK) || RW_WRITE_HELD(&cp->c_rwlock));
	CNODE_TRACE(CNTRACE_INVAL, cp, (void *)__return_address,
		(int)lm, 0);
	/*
	 * Prevent recursive and multiple invalidations.
	 */
	ospl = mutex_spinlock(&cp->c_statelock);
	if (cp->c_flags & CN_INVAL) {
		mutex_spinunlock(&cp->c_statelock, ospl);
		goto out2;
	}
	cp->c_flags |= CN_INVAL;
	mutex_spinunlock(&cp->c_statelock, ospl);
	switch (vp->v_type) {
		case VREG:
			/*
			 * Invalidate any buffers for the file.
			 */
			error = cachefs_flushvp(CTOBHV(cp), (off_t)0, 0, CFS_INVAL, 0);
			if (error) {
				goto out;
			}
			break;
		case VDIR:
			/*
			 * purge entries from the DNLC
			 */
			dnlc_purge_vp(vp);
			break;
		default:
			break;
	}
	if (C_CACHING(cp)) {
		ASSERT(cp->c_fileheader);
		/*
		 * De-allocate any space used
		 */
		ospl = mutex_spinlock(&cp->c_statelock);
		cp->c_fileheader->fh_metadata.md_state &=
			(MD_MDVALID | MD_KEEP | MD_NOTRUNC);
		cp->c_fileheader->fh_metadata.md_allocents = 0;
		CNODE_TRACE(CNTRACE_ALLOCENTS, cp, cachefs_inval_object,
			(int)cp->c_fileheader->fh_metadata.md_allocents, 0);
		cp->c_flags |= CN_UPDATED;
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_inval_object,
			cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
		mutex_spinunlock(&cp->c_statelock, ospl);
		if (!cp->c_frontvp) {
			error = cachefs_getfrontvp(cp);
			CFS_DEBUG(CFSDEBUG_ERROR, if (error)
				printf("cachefs_inval_object(line %d): error %d "
					"getting front vnode\n", __LINE__, error));
			if (error) {
				(void)cachefs_nocache(cp);
				error = 0;
			}
		}
		ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
			FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
		ASSERT(valid_file_header(cp->c_fileheader, NULL));
		/*
		 * Remove the cache front file's contents except for the
		 * file header.
		 */
		error = FRONT_VOP_SETATTR(cp, &Cachefs_file_attr, 0, sys_cred);
		if (error) {
			ospl = mutex_spinlock(&cp->c_statelock);
			cp->c_flags |= CN_NOCACHE;
			mutex_spinunlock(&cp->c_statelock, ospl);
			CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefs_inval_object,
				(int)cp->c_flags, 0);
			CFS_DEBUG(CFSDEBUG_ERROR | CFSDEBUG_NOCACHE,
				printf("cachefs_inval_object: error %d truncating front "
					"file, marking file as uncached\n", error));
			error = 0;
		}
	}
	/*
	 * Call the consistency mechanism to expire the object.
	 */
	CFSOP_EXPIRE_COBJECT(C_TO_FSCACHE(cp), cp, sys_cred);

out:
	ospl = mutex_spinlock(&cp->c_statelock);
	cp->c_flags &= ~(CN_INVAL | CN_NEEDINVAL);
	mutex_spinunlock(&cp->c_statelock, ospl);

out2:
	/*
	 * Restore the lock to its original state.
	 */
	switch (lm) {
		case READLOCKED:	/* read lock held */
			rw_downgrade(&cp->c_rwlock);
			break;
		case UNLOCKED:		/* lock needed and none is held */
			rw_exit(&cp->c_rwlock);
			break;
		case WRITELOCKED:	/* write lock held */
		case NOLOCK:		/* no lock is reuired */
			break;
	}

	CFS_DEBUG(CFSDEBUG_SUBR | CFSDEBUG_INVAL,
		printf("cachefs_inval_object: EXIT error %d\n", error));
	return( error );
}

int
cachefs_nocache(cnode_t *cp)
{
	int error = 0;
	int ospl;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_nocache: ENTER cp %p\n", cp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_nocache, current_pid(),
		cp, 0);
	ASSERT(VALID_ADDR(cp));
	CACHEFS_STATS->cs_nocache++;

	ospl = mutex_spinlock(&cp->c_statelock);
	cp->c_flags |= (CN_NOCACHE | CN_NEEDINVAL);
	CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefs_nocache, (int)cp->c_flags, 0);
	mutex_spinunlock(&cp->c_statelock, ospl);

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_nocache: EXIT error %d\n", error));
	return(error);
}

static off_t
fix_offsets(char *dirbuf, ssize_t buflen, off_t startoff)
{
	dirent_t *dep;
	char *bp = dirbuf;
	off_t lastoff = startoff;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fix_offsets, current_pid(),
		dirbuf, buflen);
	ASSERT(VALID_ADDR(dirbuf));
	while (buflen) {
		dep = (dirent_t *)bp;
		ASSERT(dep->d_reclen);
		ASSERT(dep->d_reclen >= DIRENTSIZE(1));
		lastoff = dep->d_off = ++startoff;
		bp += dep->d_reclen;
		buflen -= dep->d_reclen;
	}
	return(lastoff);
}

#ifdef DEBUG
/*
 * Validate a buffer full of directory entries.
 */
int
valid_dirents( dirent_t *dep, int dircount )
{
	int good = 1;
	dirent_t *nextdep = NULL;

	ASSERT(VALID_ADDR(dep));
	ASSERT(VALID_ADDR((u_long)dep + (u_long)dircount));
	while (dircount && (dircount >= (int)DIRENTSIZE(1)) &&
		(dircount >= dep->d_reclen)) {
			if ((dep->d_reclen == 0) || (dep->d_reclen < DIRENTSIZE(1))) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("valid_dirents: bad directory entry: dep 0x%p "
						"reclen %d dircount %d\n", dep,
						(int)dep->d_reclen, dircount));
				good = 0;
				break;
			}
			nextdep = (dirent_t *)((u_long)dep + (u_long)dep->d_reclen);
			dircount -= (int)dep->d_reclen;
			if (dircount && (dircount >= DIRENTSIZE(1)) &&
				((nextdep->d_reclen == 0) ||
				(nextdep->d_reclen < DIRENTSIZE(1)))) {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("valid_dirents: next directory entry bad,"
							"dep 0x%p reclen %d nextdep 0x%p reclen %d "
							"dircount %d\n", dep, (int)dep->d_reclen,
							 nextdep, (int)nextdep->d_reclen, dircount));
					good = 0;
					break;
			}
			dep = nextdep;
	}
	return(good);
}

static int
dirent_len(dirent_t *dep, int dircount)
{
	int len = dircount;

	ASSERT(VALID_ADDR(dep));
	ASSERT(VALID_ADDR((u_long)dep + (u_long)dircount));
	while (dircount && (dircount >= (int)DIRENTSIZE(1)) &&
		(dircount >= dep->d_reclen)) {
			dircount -= (int)dep->d_reclen;
			dep = (dirent_t *)((u_long)dep + (u_long)dep->d_reclen);
	}
	return(len - dircount);
}
#endif /* DEBUG */

/*
 * Populate a directory on the front file system from the back file system.
 * We only do this population so that readdir will function without having
 * to always go to the back file system.  An alternative is to maintain the
 * directory contents in separate files for each directory.  This can be
 * maintained in such a way that it can be easily used by readdir.  The real
 * directories and files will then be created only as they are needed in
 * lookup.
 */
int
cachefs_populate_dir(cnode_t *cp, cred_t *cr)
{
	off_t lastoff = (off_t)0;
	int ospl;
	int error = 0;
	uio_t inuio;
	iovec_t iniov;
	uio_t outuio;
	iovec_t outiov;
	char *dirbuf;
	int eof = 0;
	off_t dirsize = (off_t)0;

	CFS_DEBUG(CFSDEBUG_SUBR | CFSDEBUG_POPDIR,
		printf("cachefs_populate_dir: ENTER cp %p\n", cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_populate_dir,
		current_pid(), cp, cr);

	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(CTOV(cp)->v_type == VDIR);
	ASSERT(cp->c_flags & CN_POPULATION_PENDING);
	ASSERT(RW_WRITE_HELD(&cp->c_rwlock));
	ASSERT(!issplhi(getsr()));

	if (!C_CACHING(cp)) {
		ospl = mutex_spinlock(&cp->c_statelock);
		cp->c_flags &= ~CN_POPULATION_PENDING;
		sv_broadcast(&cp->c_popwait_sv);
		mutex_spinunlock(&cp->c_statelock, ospl);
		return(0);
	}
	ASSERT(cp->c_fileheader);
	/* 
	 * Truncate the front file before we update it from the back file.
	 * This is because the front file may shrink.
	 * We want to truncate the front file but only expire the object.
	 */
	error = cachefs_inval_object(cp, WRITELOCKED);
	if (error) {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_populate_dir(line %d): error truncating "
				"front directory file: %d\n", __LINE__, error));
		ospl = mutex_spinlock(&cp->c_statelock);
		cp->c_flags &= ~CN_POPULATION_PENDING;
		sv_broadcast(&cp->c_popwait_sv);
		mutex_spinunlock(&cp->c_statelock, ospl);
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_populate_dir: nocache cp 0x%p on error %d "
				"from cachefs_inval_object\n", cp, error));
		cachefs_nocache(cp);
		return(error);
	}
	dirbuf = CACHEFS_KMEM_ALLOC(CFS_BMAPSIZE(cp), KM_SLEEP);
	UIO_SETUP(&inuio, &iniov, dirbuf, CFS_BMAPSIZE(cp), (off_t)0,
		UIO_SYSSPACE, 0, RLIM_INFINITY);
	/*
	 * The directory data starts after the file header.  Write out
	 * as much as was read into the buffer.
	 */
	UIO_SETUP(&outuio, &outiov, dirbuf, CFS_BMAPSIZE(cp),
		(off_t)DATA_START(C_TO_FSCACHE(cp)), UIO_SYSSPACE, 0, RLIM_INFINITY);
	ASSERT(!issplhi(getsr()));
	do {
		(void)BACK_VOP_READDIR(cp, &inuio, cr, &eof, BACKOP_BLOCK, &error);
		if (error) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_populate_dir(line %d): error reading "
					"back directory: %d\n", __LINE__, error));
			break;
		}
		/*
		 * Determin how much was read, if any.
		 */
		outuio.uio_resid = outiov.iov_len = CFS_BMAPSIZE(cp) - inuio.uio_resid;
		if (outiov.iov_len) {
			dirsize += outiov.iov_len;
			CFS_DEBUG(CFSDEBUG_POPDIR,
				printf("cachefs_populate_dir: read %d bytes, dirsize %d, "
					"write offset 0x%llx next offset 0x%llx\n",
					(int) outiov.iov_len, (int)dirsize,
					outuio.uio_offset, inuio.uio_offset));
			ASSERT(valid_dirents((dirent_t *)dirbuf, (int)outiov.iov_len));
			lastoff = fix_offsets(dirbuf, outiov.iov_len, lastoff);
			ASSERT(valid_dirents((dirent_t *)dirbuf, (int)outiov.iov_len));
			ASSERT(dirent_len((dirent_t *)dirbuf, (int)outiov.iov_len) ==
				(int)outiov.iov_len);
			error = FRONT_WRITE_VP(cp, &outuio, 0, sys_cred);
			if (error) {
				CACHEFS_STATS->cs_fronterror++;
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_populate_dir(line %d): error writing "
						"front directory: %d\n", __LINE__, error));
				if (error != EINTR) {
					/*
					 * There is no need to invalidate immediately, as the
					 * metadata already indicates that there is no
					 * data in the front file.
					 */
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachefs_populate_dir: nocache cp 0x%p on "
							"error %d from FRONT_WRITE_VP\n", cp, error));
					cachefs_nocache(cp);
				}
				break;
			} else if (outuio.uio_resid) {
				CACHEFS_STATS->cs_fronterror++;
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_populate_dir(line %d): incomplete "
						"write to front directory, resid %d\n", __LINE__,
						(int)outuio.uio_resid));
				/*
				 * There is no need to invalidate immediately, as the
				 * metadata already indicates that there is no
				 * data in the front file.
				 */
				CFS_DEBUG(CFSDEBUG_NOCACHE,
					printf("cachefs_populate_dir: nocache cp 0x%p on "
						"short write, resid %d\n", cp, outuio.uio_resid));
				cachefs_nocache(cp);
				error = EIO;
				break;
			}
		} else {
			break;
		}
		/*
		 * Reset the input and output uio and iov structs for another pass.
		 * Leave the offset alone.
		 */
		outiov.iov_base = iniov.iov_base = dirbuf;
		inuio.uio_resid = iniov.iov_len = CFS_BMAPSIZE(cp);
	} while (!eof);
	CNODE_TRACE(CNTRACE_POPDIR, cp, cachefs_populate_dir, (int)dirsize, error);
	ASSERT(!issplhi(getsr()));
	CACHEFS_KMEM_FREE(dirbuf, CFS_BMAPSIZE(cp));
	ospl = mutex_spinlock(&cp->c_statelock);
	cp->c_flags &= ~CN_POPULATION_PENDING;
	if (!error) {
		cp->c_fileheader->fh_metadata.md_state |= MD_POPULATED;
		cp->c_fileheader->fh_metadata.md_allocents = 1;
		CNODE_TRACE(CNTRACE_ALLOCENTS, cp, cachefs_populate_dir,
			(int)cp->c_fileheader->fh_metadata.md_allocents, 0);
		cp->c_fileheader->fh_allocmap[0].am_start_off = (off_t)0;
		cp->c_fileheader->fh_allocmap[0].am_size = dirsize;
		CNODE_TRACE(CNTRACE_ALLOCMAP, cp, cachefs_populate_dir, 0,
			(int)cp->c_fileheader->fh_allocmap[0].am_size);
		ASSERT(dirsize > 0);
		cp->c_flags |= CN_UPDATED;
		CFS_DEBUG(CFSDEBUG_POPDIR, ASSERT(valid_allocmap));
		ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
			FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_populate_dir,
			cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
		ASSERT(valid_file_header(cp->c_fileheader, NULL));
		sv_broadcast(&cp->c_popwait_sv);
		mutex_spinunlock(&cp->c_statelock, ospl);
		if (!cp->c_frontvp) {
			error = cachefs_getfrontvp(cp);
			CFS_DEBUG(CFSDEBUG_ERROR, if (error)
				printf("cachefs_populate_dir(line %d): error %d "
					"getting front vnode\n", __LINE__, error));
			if (error) {
				(void)cachefs_nocache(cp);
				error = 0;
			}
		}
	} else {
		sv_broadcast(&cp->c_popwait_sv);
		mutex_spinunlock(&cp->c_statelock, ospl);
	}
	ASSERT(!issplhi(getsr()));
	CFS_DEBUG(CFSDEBUG_SUBR | CFSDEBUG_POPDIR,
		printf("cachefs_populate_dir: EXIT error %d\n", error));
	return(error);
}

/*
 * Checks to see if the page is in the disk cache, by checking the allocmap.
 */
int
cachefs_check_allocmap(cnode_t *cp, off_t off, off_t size)
{
	int i;
	off_t size_to_look = MIN(size, cp->c_size - off);

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_check_allocmap,
		current_pid(), cp, off);
	ASSERT(VALID_ADDR(cp));
	ASSERT(C_CACHING(cp));
	ASSERT(cp->c_fileheader);
	for (i = 0; i < cp->c_fileheader->fh_metadata.md_allocents; i++) {
		allocmap_t *allocp = &cp->c_fileheader->fh_allocmap[i];

		if (off >= allocp->am_start_off) {
		    if ((off + size_to_look) <=
				(allocp->am_start_off + allocp->am_size))
					/*
					 * Found the page in the CFS disk cache.
					 */
					return (1);
		} else {
			return (0);
		}
	}
	return (0);
}

static void
coalesce_allocmap(cnode_t *cp, int ent)
{
	int i;
	fileheader_t *fhp = cp->c_fileheader;

	/*
	 * We come here when an existing entry has been adjusted to contain
	 * the given offset and length.  In this case, it may be necessary
	 * to coalesce the altered entry with the one immediately following
	 * or preceding it.
	 * It is not necessarily true that the allocation map is valid at
	 * this point in that there may be some adjacent entries.  We cannot
	 * ASSERT(valid_allocmap(cp)) until after all coalescing has been
	 * completed.
	 */
	ASSERT(ent < ALLOCMAP_SIZE);
	ASSERT(ent < fhp->fh_metadata.md_allocents);
	/*
	 * Start with the preceeding entry.  Obviously, we do this only when
	 * there is a preceeding entry (ent > 0).
	 */
	if (ent > 0) {
		/*
		 * Check the preceeding entry.  Is it adjacent or overlapping?
		 */
		if ((fhp->fh_allocmap[ent - 1].am_start_off +
			fhp->fh_allocmap[ent - 1].am_size) >=
			fhp->fh_allocmap[ent].am_start_off) {
				/*
				 * Coalesce with the previous entry by increasing the
				 * size of the previous entry so that it contains the
				 * entry indexed by ent.  Then remove the entry indexed
				 * by ent by compressing the allocation map array.
				 * If the entry to be removed is at the end of the array,
				 * simply decrement the entry count.
				 */
				fhp->fh_allocmap[ent - 1].am_size =
					fhp->fh_allocmap[ent].am_start_off +
					fhp->fh_allocmap[ent].am_size -
					fhp->fh_allocmap[ent - 1].am_start_off;
				for (i = ent + 1; i < fhp->fh_metadata.md_allocents; i++) {
					fhp->fh_allocmap[i - 1] = fhp->fh_allocmap[i];
				}
				fhp->fh_metadata.md_allocents--;
				CNODE_TRACE(CNTRACE_ALLOCENTS, cp, cachefs_update_allocmap,
					fhp->fh_metadata.md_allocents, 0);
		}
	}
	ASSERT(cachefs_zone_validate((caddr_t)fhp,
		FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
	/*
	 * Check the following entry for coalescing.  We do this only if there
	 * is a following entry.  Check this condition by comparing ent + 1
	 * to the count of allocation entries.  If it is less than this count,
	 * it is valid.  If it is equal to this count, ent is the index of the
	 * last entry.
	 */
	if ((ent + 1) < fhp->fh_metadata.md_allocents) {
		if ((fhp->fh_allocmap[ent].am_start_off +
			fhp->fh_allocmap[ent].am_size) >=
			(fhp->fh_allocmap[ent + 1].am_start_off)) {
				fhp->fh_allocmap[ent].am_size =
					fhp->fh_allocmap[ent + 1].am_start_off +
					fhp->fh_allocmap[ent + 1].am_size -
					fhp->fh_allocmap[ent].am_start_off;
				for (i = ent + 2; i < fhp->fh_metadata.md_allocents; i++) {
					fhp->fh_allocmap[i - 1] = fhp->fh_allocmap[i];
				}
				fhp->fh_metadata.md_allocents--;
				CNODE_TRACE(CNTRACE_ALLOCENTS, cp, cachefs_update_allocmap,
					fhp->fh_metadata.md_allocents, 0);
		}
	}
	cp->c_flags |= CN_UPDATED;
	CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_update_allocmap,
		cp->c_flags, fhp->fh_metadata.md_allocents);
	ASSERT(cachefs_zone_validate((caddr_t)fhp,
		FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
	ASSERT(fhp->fh_metadata.md_allocents <= ALLOCMAP_SIZE);
	ASSERT(valid_file_header(fhp, NULL));
	ASSERT(valid_allocmap(cp));
}

/*
 * Determine the span of the region described by off and size relative
 * to the allocation map entry specified by allocp.
 *
 * There are five regions:
 *
 *  S_BEFORE        S_START         S_MIDDLE         S_END          S_AFTER
 *     010            020             030             040             050
 *  E_BEFORE        E_START         E_MIDDLE         E_END          E_AFTER
 *      01             02              03              04              05
 *          |-------------------------------|
 *
 * relative to the already locked section.  The type is two octal digits,
 * the 8's digit is the start type and the 1's digit is the end type.
 *
 * This code was lifted from regflck in os/flock.c.
 */

/* region types */
#define S_BEFORE    010		/* starts before */
#define S_START     020		/* starts at the start */
#define S_MIDDLE    030		/* starts in the middle */
#define S_END       040		/* starts at the end */
#define S_AFTER     050		/* starts after */
#define E_BEFORE    001		/* ends before */
#define E_START     002		/* ends at the start */
#define E_MIDDLE    003		/* ends in the middle */
#define E_END       004		/* ends at the end */
#define E_AFTER     005		/* ends after */

static int
cachefs_allocmap_region(allocmap_t *allocp, off_t off, off_t endoff)
{
	register int regntype;
	off_t allocend = allocp->am_start_off + allocp->am_size - 1;

	if (off > allocp->am_start_off) {
		if (off - 1 == allocend)
			return S_END|E_AFTER;
		if (off > allocend)
			return S_AFTER|E_AFTER;
		regntype = S_MIDDLE;
	} else if (off == allocp->am_start_off)
		regntype = S_START;
	else
		regntype = S_BEFORE;

	if (endoff < allocend) {
		if (endoff == allocp->am_start_off - 1)
			regntype |= E_START;
		else if (endoff < allocp->am_start_off)
			regntype |= E_BEFORE;
		else
			regntype |= E_MIDDLE;
	} else if (endoff == allocend)
		regntype |= E_END;
	else
		regntype |= E_AFTER;

	return  regntype;
}

/*
 * Updates the allocmap to reflect a new chunk of data that has been
 * populated.
 */
int
cachefs_update_allocmap(cnode_t *cp, off_t off, off_t size)
{
	int i;
	int error = 0;
	int ent;
	allocmap_t *allocp;
	off_t endoff = off + size;
	fileheader_t *fhp = cp->c_fileheader;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_update_allocmap,
		current_pid(), cp, off);
	ASSERT(VALID_ADDR(cp));
	ASSERT(cp->c_fileheader);
	ASSERT(fhp->fh_metadata.md_allocents <= ALLOCMAP_SIZE);
	/*
	 * If the caching state has changed, we do not update the allocation
	 * map.  Return an error so that the caller will invalidate any
	 * data which might have been written.
	 */
	if (!C_CACHING(cp)) {
		return(EFBIG);
	}
	/*
	 * We try to see if we can coalesce the current block into an existing
	 * allocation and mark it as such.
	 * If we can't do that then we make a new entry in the allocmap.
	 * when we run out of allocmaps, put the cnode in NOCACHE mode.
	 */
	for (ent = 0; ent < fhp->fh_metadata.md_allocents; ent++) {

		allocp = &fhp->fh_allocmap[ent];
		ASSERT(allocp <= &fhp->fh_allocmap[ALLOCMAP_SIZE - 1]);
		/*
		 * There are seven different overlap or containment cases (ignoring
		 * the containment variations) for a new entry relative to the
		 * current entry under examination:
		 *
		 *		1) new entry will preceed
		 *		2) new entry is adjacent at the front
		 *		3) new entry overlaps the front
		 *		4) new entry contains
		 *		5) new entry is contained or is equivalent
		 *		6) new entry overlaps the end
		 *		7) new entry is adjacent at the end
		 */
		switch (cachefs_allocmap_region(allocp, off, off + size - 1)) {
			case S_BEFORE|E_BEFORE:
				/*
				 * The new entry preceeds and does not overlap the current
				 * entry being examined.  A new entry is needed.  Put it at
				 * the end of the map and sort the map.
				 */
				if (fhp->fh_metadata.md_allocents < ALLOCMAP_SIZE) {
					/*
					 * Make room for the new entry by moving all entries
					 * up one slot.  Start at the end of the map.
					 */
					for (i = fhp->fh_metadata.md_allocents; i > ent; i--) {
						fhp->fh_allocmap[i] = fhp->fh_allocmap[i - 1];
					}
					allocp->am_start_off = off;
					allocp->am_size = size;
					CNODE_TRACE(CNTRACE_ALLOCMAP, cp, cachefs_update_allocmap,
						allocp - fhp->fh_allocmap, allocp->am_size);
					fhp->fh_metadata.md_allocents++;
					CNODE_TRACE(CNTRACE_ALLOCENTS, cp, cachefs_update_allocmap,
						fhp->fh_metadata.md_allocents, 0);
					cp->c_flags |= CN_UPDATED;
					CNODE_TRACE(CNTRACE_UPDATE, cp,
						(void *)cachefs_update_allocmap, cp->c_flags,
						fhp->fh_metadata.md_allocents);
					ASSERT(fhp->fh_metadata.md_allocents <= ALLOCMAP_SIZE);
					ASSERT(valid_file_header(fhp, NULL));
					ASSERT(valid_allocmap(cp));
					ASSERT(cachefs_zone_validate((caddr_t)fhp,
						FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
					error = 0;
				} else {
					/*
					 * The allocation map is full.
					 */
					error = EFBIG;
				}
				return(error);
			case S_BEFORE|E_START:
				/*
				 * front adjacent
				 * The new entry starts before and is adjacent to an
				 * existing entry.  Adjust the existing one and coalesce.
				 */
				allocp->am_start_off = off;
				allocp->am_size += size;
				coalesce_allocmap(cp, ent);
				return(0);
			case S_START|E_AFTER:
			case S_BEFORE|E_END:
			case S_BEFORE|E_AFTER:
				/*
				 * New entry will contain existing one.
				 */
				allocp->am_start_off = off;
				allocp->am_size = size;
				coalesce_allocmap(cp, ent);
				return(0);
			case S_START|E_END:
				/*
				 * identical
				 */
			case S_START|E_MIDDLE:
			case S_MIDDLE|E_MIDDLE:
			case S_MIDDLE|E_END:
				/*
				 * new contained
				 */
				return(0);
			case S_BEFORE|E_MIDDLE:
				/*
				 * Front overlap case.
				 */
				endoff = allocp->am_start_off + allocp->am_size;
				allocp->am_start_off = off;
				allocp->am_size = endoff - off;
				coalesce_allocmap(cp, ent);
				return(0);
			case S_MIDDLE|E_AFTER:
				/*
				 * End overlap case.
				 */
				allocp->am_size = endoff - allocp->am_start_off;
				coalesce_allocmap(cp, ent);
				return(0);
			case S_END|E_AFTER:
				/*
				 * End adjacent case.
				 */
				allocp->am_size += size;
				coalesce_allocmap(cp, ent);
				return(0);
			case S_AFTER|E_AFTER:
				/*
				 * follows, so loop continues
				 */
				break;
			default:
				/*
				 * invalid cases
				 */
				ASSERT(0);
		}
	}
	/*
	 * If we get here, a new entry must be made at the end of the list.
	 * There were either no entries in the list or the new entry could
	 * not be coalesced with the last entry in the list.
	 * Check for room in the map first.  Return EFBIG if the map is full.
	 */
	if (fhp->fh_metadata.md_allocents < ALLOCMAP_SIZE) {
		/*
		 * Add a new entry at the end of the list and update the entry
		 * count.
		 */
		fhp->fh_allocmap[fhp->fh_metadata.md_allocents].am_size = size;
		fhp->fh_allocmap[fhp->fh_metadata.md_allocents].am_start_off = off;
		fhp->fh_metadata.md_allocents++;
	} else {
		error = EFBIG;
	}
	return(error);
}

void
cachefs_iodone(struct buf *bp)
{
	ASSERT((bp->b_flags & (B_ASYNC | B_BDFLUSH)) == (B_ASYNC | B_BDFLUSH));
	ASSERT(!(bp->b_flags & B_ERROR));
	bp->b_flags |= (B_DONE | B_DELWRI);
	bp->b_iodone = NULL;
	brelse(bp);
}

int
cachefsio_write(struct cnode *cp, struct buf *bp, cred_t *cr)
{
	int error;
	enum backop_mode opmode;
	int wflags;
	int seg;
	int err = 0;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	off_t iooff;
	u_int iolen;
	char *addr = NULL;
	uio_t uio;
	iovec_t iov;
	int ospl;
	int mapped = BP_ISMAPPED(bp);

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_RDWR,
		printf( "cachefsio_write: ENTER cp 0x%p bp 0x%p cr 0x%p\n",
		    cp, bp, cr));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(bp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(fscp));
	ASSERT(!(FSC_TO_VFS(fscp)->vfs_flag & VFS_RDONLY));
	ASSERT(WRITEALLOWED(fscp->fs_root->c_backvp, cr));

	CACHEFUNC_TRACE(CFTRACE_WRITE, (void *)cachefsio_write, current_pid(),
		cp, cp->c_frontvp);
	CACHEFS_STATS->cs_writes++;
	if (cp->c_cred != NULL)
		cr = cp->c_cred;

	iooff = (off_t)BBTOOFF( bp->b_offset );

	if (mapped) {
		addr = bp->b_un.b_addr;
	} else {
		addr = bp_mapin(bp);
	}

	ASSERT((cp->c_size - iooff) > (off_t)0);
	iolen  = (u_int)MIN((off_t)bp->b_bcount, cp->c_size - iooff);
	CNODE_TRACE(CNTRACE_WRITE, cp, (void *)cachefsio_write, iooff, iolen);
	if ((bp->b_flags & (B_ASYNC | B_BDFLUSH)) == (B_ASYNC | B_BDFLUSH)) {
		opmode = BACKOP_NONBLOCK;
	} else {
		opmode = BACKOP_BLOCK;
	}
	if (((C_ISFS_WRITE_AROUND(fscp) || C_ISFS_SINGLE(fscp)))) {
		UIO_SETUP(&uio, &iov, addr, (int)iolen, iooff, UIO_NOSPACE,
			FWRITE, (off_t)RLIM_INFINITY);
		switch (BACK_WRITE_VP(cp, &uio, IO_SYNC | IO_DIRECT | IO_PFUSE_SAFE,
			cr, opmode, &err)) {
			case BACKOP_NETERR:
				goto write_error;
			case BACKOP_SUCCESS:
				if (uio.uio_resid == 0) {
					break;
				}
				err = EIO;
				CFS_DEBUG(CFSDEBUG_DIO,
					printf("cachefsio_write(line %d): incomplete back file "
						"write: resid %d, addr 0x%p, size %d, offset "
						"0x%llx\n", __LINE__, uio.uio_resid, addr,
						(int)iolen, iooff));
			case BACKOP_FAILURE:
				ospl = mutex_spinlock(&cp->c_statelock);
				cp->c_flags |= CN_NEEDINVAL;
				if ((err != EPERM) && (err != EACCES) && (err != EROFS)) {
					cp->c_flags |= CN_NOCACHE;
					CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefsio_write,
						(int)cp->c_flags, 0);
					CACHEFS_STATS->cs_nocache++;
				}
				mutex_spinunlock(&cp->c_statelock, ospl);
				CFS_DEBUG(CFSDEBUG_DIO | CFSDEBUG_ERROR,
					printf("cachefsio_write(line %d): back file write "
						"error %d, addr 0x%p, size %d, offset 0x%llx\n",
						__LINE__, err, addr, (int)iolen, iooff));
				goto write_error;
		}
	}
	if (C_CACHING(cp)) {
		/*
		 * First, check the allocation map to see if we have already read
		 * or written the blocks.  If not, attempt to allocate the necessary
		 * blocks.  Failing that, we nocache the file.  The write will still
		 * go to the back file system.
		 */
		if ((cachefs_check_allocmap(cp, iooff, (off_t)iolen) == 0) &&
			(err = cachefs_allocblocks(fscp->fs_cache, iolen, cr))) {
				/*
				 * If we cannot allocate the necessary blocks, nocache the file
				 * and start over so it can be read from the back FS.
				 * Make sure the error is cleared so that we don't return
				 * a write error, as there was not really a write error.
				 */
				ospl = mutex_spinlock(&cp->c_statelock);
				cp->c_flags |= CN_NOCACHE;
				CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefsio_write,
					(int)cp->c_flags, 0);
				CACHEFS_STATS->cs_nocache++;
				mutex_spinunlock(&cp->c_statelock, ospl);
				CFS_DEBUG(CFSDEBUG_NOCACHE,
					printf("cachefsio_write(line %d): nocache cp 0x%p\n",
						__LINE__, cp));
				err = 0;
		} else if (C_ISFS_SINGLE(fscp)) {
			ASSERT(cp->c_frontfid.fid_len &&
				(cp->c_frontfid.fid_len <= MAXFIDSZ));
			if (front_dio) {
				seg = UIO_NOSPACE;
				wflags = IO_DIRECT | IO_TRUSTEDDIO;
			} else {
				seg = UIO_SYSSPACE;
				wflags = 0;
			}
			UIO_SETUP(&uio, &iov, addr, ((int)iolen + BBMASK) & ~BBMASK,
				iooff + DATA_START(fscp), seg, FWRITE, (off_t)RLIM_INFINITY);
			err = FRONT_WRITE_VP(cp, &uio, wflags, sys_cred);
			CNODE_TRACE(CNTRACE_DIOWRITE, cp, (void *)cachefsio_write,
				(int)iooff + DATA_START(fscp), iolen);
			if (err) {
				CFS_DEBUG(CFSDEBUG_DIO,
					printf("cachefsio_write(line %d): front file write error "
						"%d, addr 0x%p, size %d, offset 0x%llx\n", __LINE__,
						err, addr, (int)iolen, iooff));
				if (err != EINTR) {
					ospl = mutex_spinlock(&cp->c_statelock);
					cp->c_flags |= (CN_NOCACHE | CN_NEEDINVAL);
					CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefsio_write,
						(int)cp->c_flags, 0);
					CACHEFS_STATS->cs_nocache++;
					mutex_spinunlock(&cp->c_statelock, ospl);
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachefsio_write(line %d): nocache cp 0x%p\n",
							__LINE__, cp));
				}
				err = 0;
			} else if (uio.uio_resid) {
				CFS_DEBUG(CFSDEBUG_DIO,
					printf("cachefsio_write(line %d): incomplete front file "
						"write: resid %d, addr 0x%p, size %d, offset "
						"0x%llx\n", __LINE__, (int)uio.uio_resid, addr,
						(int)iolen, iooff));
				ospl = mutex_spinlock(&cp->c_statelock);
				cp->c_flags |= (CN_NOCACHE | CN_NEEDINVAL);
				CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefsio_write,
					(int)cp->c_flags, 0);
				CACHEFS_STATS->cs_nocache++;
				mutex_spinunlock(&cp->c_statelock, ospl);
				CFS_DEBUG(CFSDEBUG_NOCACHE,
					printf("cachefsio_write(line %d): nocache cp 0x%p\n",
						__LINE__, cp));
				err = 0;
			} else {
				/*
				 * Update the allocation map.  If this fails, the file will
				 * be marked nocache.
				 */
				ospl = mutex_spinlock(&cp->c_statelock);
				if (cachefs_update_allocmap(cp, iooff, (off_t)iolen)) {
					cp->c_flags |= CN_NOCACHE;
					CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefsio_write,
						(int)cp->c_flags, 0);
					CACHEFS_STATS->cs_nocache++;
					mutex_spinunlock(&cp->c_statelock, ospl);
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachefsio_write(line %d): nocache cp 0x%p\n",
							__LINE__, cp));
				} else {
					mutex_spinunlock(&cp->c_statelock, ospl);
				}
				if ((cp->c_flags & CN_UPDATED) && !cp->c_frontvp) {
					error = cachefs_getfrontvp(cp);
					CFS_DEBUG(CFSDEBUG_ERROR, if (error)
						printf("cachefsio_write(line %d): error %d "
							"getting front vnode\n", __LINE__, error));
					if (error) {
						(void)cachefs_nocache(cp);
						error = 0;
					}
				}
			}

		}
	} else {
		CACHEFS_STATS->cs_nocachewrites++;
	}
write_error:
	if (!mapped) {
		bp_mapout( bp );
	}
	if (bp->b_error = err) {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefsio_write: error %d\n", err));
		if ((err == ETIMEDOUT) && (opmode == BACKOP_NONBLOCK)) {
			err = bp->b_error = 0;
			bp->b_iodone = cachefs_iodone;
		} else {
			bp->b_flags |= B_ERROR;
		}
		ospl = mutex_spinlock(&cp->c_statelock);
		cp->c_error = err;
		mutex_spinunlock(&cp->c_statelock, ospl);
	} else {
		CFSOP_MODIFY_COBJECT(fscp, cp, cr);
		ospl = mutex_spinlock(&cp->c_statelock);
		cp->c_error = err;
		if (cp->c_written >= iolen) {
			cp->c_written -= iolen;
		} else {
			cp->c_written = 0;
		}
		mutex_spinunlock(&cp->c_statelock, ospl);
	}
	iodone( bp );

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_RDWR,
		printf("cachefsio_write: EXIT error %d\n", err));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (err == ESTALE) printf("cachefsio_write: EXIT ESTALE\n"));
	return (err);
}

int
cachefsio_read(struct cnode *cp, struct buf *bp, cred_t *cr)
{
	int seg;
	int rflags;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;
	off_t off = BBTOOFF( bp->b_offset );
	char *addr = NULL;
	ssize_t iolen = (bp->b_bcount + BBMASK) & ~BBMASK;
	uio_t uio;
	iovec_t iov;
	int ospl;
	int mapped = BP_ISMAPPED(bp);
#ifdef MH_R10000_SPECULATION_WAR
	ssize_t real_iolen = 0;
	char *real_addr = NULL;
#endif /* MH_R10000_SPECULATION_WAR */

	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_RDWR,
		printf( "cachefsio_read: ENTER cp 0x%p bp 0x%p off %ld len %ld\n",
		    cp, bp, off, (long)iolen));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefsio_read, current_pid(),
		cp, bp);
	CNODE_TRACE(CNTRACE_READ, cp, (void *)cachefsio_read, bp->b_offset,
		iolen);

	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(bp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(!C_CACHING(cp) ||
		(cp->c_frontfid.fid_len && (cp->c_frontfid.fid_len <= MAXFIDSZ)));

	CACHEFS_STATS->cs_reads++;
	if (mapped) {
		addr = bp->b_un.b_addr;
	} else {
		addr = bp_mapin( bp );
	}
	ospl = mutex_spinlock( &cp->c_statelock );
	/*
	 * If we're not cacheing this file, just go directly to the back file.
	 */
	if (!C_CACHING(cp)) {
		mutex_spinunlock( &cp->c_statelock, ospl );
		CACHEFS_STATS->cs_nocachereads++;
		UIO_SETUP( &uio, &iov, addr, iolen, off, UIO_NOSPACE, FREAD,
			(off_t)RLIM_INFINITY);
		(void)BACK_READ_VP( cp, &uio, IO_SYNC | IO_DIRECT | IO_PFUSE_SAFE, cp->c_cred,
			(bp->b_flags & B_ASYNC) ? BACKOP_NONBLOCK : BACKOP_BLOCK, &error );
		CFS_DEBUG(CFSDEBUG_DIO,
			if ( error ) 
				printf("cachefsio_read(line %d): back file read error %d, "
					"addr 0x%p, size %d, offset 0x%llx\n", __LINE__, error,
					addr, iolen, off));
	} else if (cachefs_check_allocmap(cp, off, (off_t)iolen)) {
		mutex_spinunlock( &cp->c_statelock, ospl );
		CACHEFS_STATS->cs_readhit++;
		/*
		 * File was populated so we get the page from the
		 * frontvp
		 * do this with direct I/O to bypass the buffer cache
		 */
		if (front_dio) {
			seg = UIO_NOSPACE;
			rflags = IO_SYNC | IO_DIRECT | IO_TRUSTEDDIO;
		} else {
			seg = UIO_SYSSPACE;
			rflags = 0;
		}
#ifdef MH_R10000_SPECULATION_WAR
		/*
		 * we assume that the address will always be aligned and that
		 * the only thing which might not be correct is the length
		 */
		if (iolen % DIO_MINIO(fscp->fs_cache)) {
			real_addr = addr;
			real_iolen = iolen;
			iolen = ((iolen + DIO_MINIO(fscp->fs_cache) - 1) /
				DIO_MINIO(fscp->fs_cache)) * DIO_MINIO(fscp->fs_cache);
			addr = kmem_zalloc(iolen, KM_SLEEP);
		}
#endif /* MH_R10000_SPECULATION_WAR */
		UIO_SETUP( &uio, &iov, addr, iolen, off + DATA_START(fscp),
			seg, FREAD, (off_t)RLIM_INFINITY );
		error = FRONT_READ_VP( cp, &uio, rflags, sys_cred );
#ifdef MH_R10000_SPECULATION_WAR
		if (real_iolen) {
			bcopy(addr, real_addr, real_iolen);
			if (uio.uio_resid < (iolen - real_iolen)) {
				uio.uio_resid = 0;
			} else {
				uio.uio_resid -= (iolen - real_iolen);
			}
			kmem_free(addr, iolen);
			addr = real_addr;
			iolen = real_iolen;
		}
#endif /* MH_R10000_SPECULATION_WAR */
		if (error) {
			CFS_DEBUG(CFSDEBUG_DIO,
				printf("cachefsio_read(line %d): front file read error "
					"%d, addr 0x%p, size %d, offset 0x%llx\n", __LINE__,
					error, addr, iolen, off));
			if (error != EINTR) {
				/*
				 * Mark the cnode CN_NOCACHE but do not call cachefs_nocache
				 * until later.  It is very important that cachefs_nocache
				 * not be called until after brelse.  A deadlock may result
				 * otherwise.  This means that the calling of cachefs_nocache
				 * must be deferred until fscache_sync or cachefs_inactive.
				 */
				ospl = mutex_spinlock(&cp->c_statelock);
				cp->c_flags |= (CN_NOCACHE | CN_NEEDINVAL);
				CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefsio_read,
					(int)cp->c_flags, 0);
				CACHEFS_STATS->cs_nocache++;
				mutex_spinunlock(&cp->c_statelock, ospl);
				CFS_DEBUG(CFSDEBUG_NOCACHE,
					printf("cachefsio_read(line %d): nocache cp 0x%p\n",
						__LINE__, cp));
				UIO_SETUP( &uio, &iov, addr, iolen, off, UIO_NOSPACE,
					FREAD, (off_t)RLIM_INFINITY);
				(void)BACK_READ_VP( cp, &uio, IO_SYNC | IO_DIRECT | IO_PFUSE_SAFE,
					cp->c_cred,
					(bp->b_flags & B_ASYNC) ? BACKOP_NONBLOCK : BACKOP_BLOCK,
					&error );
				CFS_DEBUG(CFSDEBUG_DIO,
					if ( error )
						printf("cachefsio_read(line %d): back file read error "
							"%d, addr 0x%p, size %d, offset 0x%llx\n",
							__LINE__, error, addr, iolen, off));
			}
		}
	} else {
		mutex_spinunlock( &cp->c_statelock, ospl );
		CACHEFS_STATS->cs_readmiss++;
		/*
		 * We are assuming that this request does not overlap an existing
		 * set of blocks.
		 */
		if (error = cachefs_allocblocks(fscp->fs_cache, iolen, cr)) {
			/*
			 * If we cannot allocate the necessary blocks, nocache the file
			 * and start over so it can be read from the back FS.
			 */
			ospl = mutex_spinlock(&cp->c_statelock);
			cp->c_flags |= CN_NOCACHE;
			CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefsio_read,
				(int)cp->c_flags, 0);
			CACHEFS_STATS->cs_nocache++;
			mutex_spinunlock(&cp->c_statelock, ospl);
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefsio_read(line %d): nocache cp 0x%p\n",
					__LINE__, cp));
		}
		UIO_SETUP( &uio, &iov, addr, iolen, off, UIO_NOSPACE, FREAD,
			(off_t)RLIM_INFINITY);
		(void)BACK_READ_VP( cp, &uio, IO_SYNC | IO_DIRECT | IO_PFUSE_SAFE, cp->c_cred,
			(bp->b_flags & B_ASYNC) ? BACKOP_NONBLOCK : BACKOP_BLOCK, &error );
		if ( !error && C_CACHING(cp) ) {
			/*
			 * Write the data to the front file.  If there is no error,
			 * update the allocation map.
			 */
			if (front_dio) {
				seg = UIO_NOSPACE;
				rflags = IO_SYNC | IO_DIRECT | IO_TRUSTEDDIO;
			} else {
				seg = UIO_SYSSPACE;
				rflags = 0;
			}
			UIO_SETUP( &uio, &iov, addr, iolen,
				off + DATA_START(fscp), seg, FWRITE,
				(off_t)RLIM_INFINITY);
			error = FRONT_WRITE_VP( cp, &uio,
				rflags, sys_cred );
			CNODE_TRACE(CNTRACE_DIOWRITE, cp, (void *)cachefsio_read,
				(int)off + DATA_START(fscp), iolen);
			if (!error && uio.uio_resid) {
				error = EIO;
			} else if (!error) {
				ospl = mutex_spinlock(&cp->c_statelock);
				if (cachefs_update_allocmap(cp, off, (off_t)iolen)) {
					cp->c_flags |= CN_NOCACHE;
					CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefsio_read,
						(int)cp->c_flags, 0);
					CACHEFS_STATS->cs_nocache++;
					mutex_spinunlock(&cp->c_statelock, ospl);
					CFS_DEBUG(CFSDEBUG_NOCACHE,
						printf("cachefsio_read(line %d): nocache cp 0x%p\n",
							__LINE__, cp));
				} else {
					mutex_spinunlock(&cp->c_statelock, ospl);
				}
				if ((cp->c_flags & CN_UPDATED) && !cp->c_frontvp) {
					error = cachefs_getfrontvp(cp);
					CFS_DEBUG(CFSDEBUG_ERROR, if (error)
						printf("cachefsio_read(line %d): error %d "
							"getting front vnode\n", __LINE__, error));
					if (error) {
						(void)cachefs_nocache(cp);
						error = 0;
					}
				}
#ifdef DEBUG
			} else {
				CFS_DEBUG(CFSDEBUG_DIO,
					printf("cachefsio_read(line %d): front file write error "
						"%d, addr 0x%p, size %d, offset 0x%llx\n", __LINE__,
						error, addr, iolen, off));
#endif /* DEBUG */
			}
#ifdef DEBUG
		} else if (error != EINTR) {
			CFS_DEBUG(CFSDEBUG_DIO,
				printf("cachefsio_read(line %d): back file read error %d, "
					"addr 0x%p, size %d, offset 0x%llx\n", __LINE__, error,
					addr, iolen, off));
#endif /* DEBUG */
		}
	}
	if (!mapped) {
		bp_mapout( bp );
	}
	bp->b_resid = uio.uio_resid;
	if ( bp->b_error = error ) {
		bp->b_flags |= B_ERROR;
		CACHEFS_STATS->cs_readerrors++;
	}
	iodone( bp );
	CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_RDWR,
		printf("cachefsio_read: EXIT error %d\n", error));
	CFS_DEBUG(CFSDEBUG_STALE,
		if (error == ESTALE) printf("cachefsio_read: EXIT ESTALE\n"));
	return(error);
}

void
cachefs_workq_init(struct cachefs_workq *qp)
{
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_workq_init,
		current_pid(), qp, 0);
	ASSERT(VALID_ADDR(qp));
	qp->wq_head = qp->wq_tail = NULL;
	qp->wq_length =
	    qp->wq_thread_count =
	    qp->wq_max_len =
	    qp->wq_halt_request = 0;
	qp->wq_keepone = 0;
	sv_init(&qp->wq_req_sv, SV_DEFAULT, "cachefs async io cv");
	sv_init(&qp->wq_halt_sv, SV_DEFAULT, "cachefs halt drain cv");
	spinlock_init(&qp->wq_queue_lock, "work queue lock");
}

void
cachefs_workq_destroy( struct cachefs_workq *qp )
{
	int printed = 0;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_workq_destroy,
		current_pid(), qp, 0);
	ASSERT(VALID_ADDR(qp));
	while ( cachefs_async_halt(qp) == EBUSY ) {
		if ( !printed ) {
			printed = 1;
			cmn_err( CE_WARN,
				"cachefs_workq_destroy: waiting for async daemons\n" );
		}
	}
	if ( printed ) {
		cmn_err( CE_WARN,
			"cachefs_workq_destroy: async daemons halted\n" );
	}
	sv_destroy( &qp->wq_req_sv );
	sv_destroy( &qp->wq_halt_sv );
	spinlock_destroy( &qp->wq_queue_lock );
}

int
cachefs_async_start(cachefscache_t *cachep)
{
	rval_t rval;
	int error = 0;
	int ospl;

	ospl = mutex_spinlock(&cachep->c_workq.wq_queue_lock);
	if (cachep->c_workq.wq_thread_count == 0) {
		cachep->c_workq.wq_keepone = 1;
		cachep->c_workq.wq_thread_busy = 0;
		cachep->c_workq.wq_halt_request = 0;
		mutex_spinunlock(&cachep->c_workq.wq_queue_lock, ospl);
		/*
		 * create the cachefs_async_daemon process
		 */
		if ( (error = fork(NULL, &rval)) == 0 ) {
			if ( rval.r_val1 == 0 ) {
				/*
				 * this is the new process
				 * call cachefs_async_daemon which never returns
				 */
				cachefs_async_daemon( &cachep->c_workq );
				/* NOTREACHED */
			}
			ospl = mutex_spinlock(&cachep->c_workq.wq_queue_lock);
			cachep->c_workq.wq_thread_count++;
			mutex_spinunlock(&cachep->c_workq.wq_queue_lock, ospl);
		} else {
			cmn_err(CE_WARN,
				"CacheFS: unable to start async daemon, error %d\n",
				error);
		}
	} else {
		mutex_spinunlock(&cachep->c_workq.wq_queue_lock, ospl);
	}
	return(error);
}

void
cachefs_async_daemon(struct cachefs_workq *qp)
{
	rval_t rval;
	struct cachefs_req *rp;
	int left;
	k_sigset_t catch_sigs = {
		sigmask(SIGINT) | sigmask(SIGQUIT) | sigmask(SIGKILL) | sigmask(SIGTERM)
	};
	int old_spl;
	int error = 0;
	rval_t rvp;
	uthread_t *ut;
	sigvec_t *sigvp;
	proc_t	*curp = NULL;
	timespec_t ts;
	timespec_t remaining;

	CFS_DEBUG(CFSDEBUG_PROCS,
		printf( "cachefs_async_daemon: ENTER qp 0x%p\n", qp ));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_async_daemon,
		current_pid(), qp, 0);
	ASSERT(VALID_ADDR(qp));
	/*
	 * No need for user VM.
	 */
	vrelvm();

	/*
	 * close all open files
	 */
	fdt_release();

proc_start:		/* where duplicate processes must start */
	ut = curuthread;
	curp = UT_TO_PROC(ut);

	/*
	 * Set scheduler policy and priority
	 */
	VPROC_SCHED_SETSCHEDULER(curvprocp, 21, SCHED_TS, &rvp.r_val1, &error);
	if (error) {
	    printf("Unable to set cachefsd scheduler priority/policy: error=%d\n",
		error);
	}

	/*
	 * set things up so zombies don't lie about
	 */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_UPDATE, sigvp);
	sigvp->sv_flags |= SNOWAIT|SNOCLDSTOP;
	
	/*
	 * set up the signals to be caught
	 */
	sigfillset(&sigvp->sv_sigign);
	sigdiffset(&sigvp->sv_sigign, &catch_sigs);
	sigvp->sv_sigcatch = catch_sigs;
	VPROC_PUT_SIGVEC(curvprocp);

	old_spl = ut_lock(ut);
	sigemptyset(ut->ut_sighold);
	ut_unlock(ut, old_spl);
	
	bcopy("cachefs_async", (void *)curp->p_psargs, 14);
	bcopy("cachefs_async", (void *)curp->p_comm, 13);
	CFS_DEBUG(CFSDEBUG_PROCS,
		printf( "cachefs_async_daemon: proc_start %s pid %d\n",
			curp->p_psargs, (int)curp->p_pid ));

	ASSERT(!issplhi(getsr()));
	old_spl = mutex_spinlock(&qp->wq_queue_lock);
	left = 1;
	for (;;) {
		/* if there are no pending requests */
		if (qp->wq_head == NULL) {
			/* see if thread should exit */
			if (qp->wq_halt_request || (left == 0)) {
				if (((qp->wq_thread_count - qp->wq_thread_busy)
								> 1) ||
				    (qp->wq_keepone == 0))
					break;
			}

			/* wake up thread in async_halt if necessary */
			if (qp->wq_halt_request)
			{
				CFS_DEBUG(CFSDEBUG_PROCS,
					printf(
						"cachefs_async_daemon[%d]: Halt request, %s\n",
						(int)curp->p_pid,
						qp->wq_keepone ? "keep one" : "keep none" ));
				sv_signal(&qp->wq_halt_sv);
				if ((qp->wq_thread_count > 1) || (qp->wq_keepone == 0)) {
					break;
				}
			}
			CFS_DEBUG(CFSDEBUG_PROCS,
				printf( "cachefs_async_daemon[%d]: waiting for requests\n",
					(int)curp->p_pid ));

			/* sleep until there is something to do */
			ts.tv_sec = CFS_ASYNC_TIMEOUT;
			ts.tv_nsec = 0;
			(void)sv_timedwait( &qp->wq_req_sv, PZERO | PLTWAIT,
				&qp->wq_queue_lock, old_spl, 0,
				&ts, &remaining);
			ASSERT(!issplhi(getsr()));
			left = remaining.tv_sec || remaining.tv_nsec;
			old_spl = mutex_spinlock( &qp->wq_queue_lock );
			CFS_DEBUG(CFSDEBUG_PROCS,
				printf("cachefs_async_daemon[%d]: %s, left = %d\n",
					(int)curp->p_pid, left ? "request" : "cv timeout",
					left ));
			continue;
		/*
		 * see if we need another process
		 * simply duplicate the current process
		 */
		} else if ((qp->wq_thread_count < cachefs_max_threads) &&
			((qp->wq_length >= (qp->wq_thread_count * 2)) ||
			((qp->wq_thread_count - qp->wq_thread_busy) == 1))) {
				qp->wq_thread_count++;
				qp->wq_thread_busy++;
				mutex_spinunlock(&qp->wq_queue_lock, old_spl);
				if ( fork(NULL, &rval) == 0 ) {
					if ( rval.r_val1 == 0 ) {
						/*
						 * this is the new process
						 * it has to go to the beginning and acquire
						 * the mutex before doing anything else
						 */
						goto proc_start;
					} else {
						/*
						 * this case is the parent process, simply continue
						 * normal execution
						 */
						old_spl = mutex_spinlock(&qp->wq_queue_lock);
					}
				} else {
					/*
					 * If the for failed, decrement the thread count.
					 */
					old_spl = mutex_spinlock(&qp->wq_queue_lock);
					qp->wq_thread_count--;
				}
		} else {
			qp->wq_thread_busy++;
		}
		if (qp->wq_head != NULL) {
			left = 1;

			/* remove request from the list */
			rp = qp->wq_head;
			qp->wq_head = rp->cfs_next;
			if (rp->cfs_next == NULL)
				qp->wq_tail = NULL;

			/* decrement count of requests */
			qp->wq_length--;

			/* do the request */
			mutex_spinunlock(&qp->wq_queue_lock, old_spl);
			cachefs_do_req(rp);
			ASSERT(!issplhi(getsr()));
			old_spl = mutex_spinlock(&qp->wq_queue_lock);
		}
		qp->wq_thread_busy--;
	}
	ASSERT(qp->wq_head == NULL);
	qp->wq_thread_count--;
	ASSERT((qp->wq_thread_count > 0) || !qp->wq_keepone);
	if (qp->wq_halt_request && qp->wq_thread_count == 0)
		sv_signal(&qp->wq_halt_sv);
	mutex_spinunlock(&qp->wq_queue_lock, old_spl);
	CFS_DEBUG(CFSDEBUG_PROCS,
		printf( "cachefs_async_daemon[%d]: EXIT (process exiting, queue 0x%p, "
			"thread count now %d)\n", (int)curp->p_pid, qp,
			qp->wq_thread_count));
	exit(CLD_EXITED, 0);
	/*NOTREACHED*/
}

/*
 * attempt to halt all the async threads associated with a given workq
 */
int
cachefs_async_halt(struct cachefs_workq *qp)
{
	int error = 0;
	int ospl;
	timespec_t ts;

	CFS_DEBUG(CFSDEBUG_PROCS,
		printf( "cachefs_async_halt: ENTER qp 0x%p\n", qp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_async_halt,
		current_pid(), qp, 1);
	ASSERT(VALID_ADDR(qp));
	ospl = mutex_spinlock(&qp->wq_queue_lock);
	qp->wq_keepone = 0;

	if (qp->wq_thread_count > 0) {
		qp->wq_halt_request = 1;
		sv_broadcast(&qp->wq_req_sv);
		ts.tv_sec = CFS_ASYNC_TIMEOUT;
		ts.tv_nsec = 0;
		(void)sv_timedwait( &qp->wq_halt_sv, PZERO,
			&qp->wq_queue_lock, ospl, 0, &ts, NULL);
		ASSERT(!issplhi(getsr()));
		ospl = mutex_spinlock( &qp->wq_queue_lock);
		qp->wq_halt_request = 0;
		qp->wq_keepone = 1;
		if (qp->wq_thread_count > 0) {
			error = EBUSY;
		} else {
			ASSERT(qp->wq_length == 0 && qp->wq_head == NULL);
		}
	}
	mutex_spinunlock(&qp->wq_queue_lock, ospl);
	CFS_DEBUG(CFSDEBUG_PROCS,
		printf( "cachefs_async_halt: EXIT error = %d\n", error ));
	return (error);
}

int
cachefs_addqueue(struct cachefs_req *rp, struct cachefs_workq *qp)
{
	int error = 0;
	int ospl;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_addqueue, current_pid(),
		rp, qp);
	ASSERT(VALID_ADDR(rp));
	ASSERT(VALID_ADDR(qp));
	ASSERT(!issplhi(getsr()));
	ospl = mutex_spinlock(&qp->wq_queue_lock);
	if (qp->wq_thread_count == 0) {
		error = EBUSY;
		cmn_err( CE_WARN, "cachefs_addqueue: no async procs for queue 0x%p\n",
			qp );
		goto out;
	}
	CACHEFS_STATS->cs_asyncreqs++;
	if (qp->wq_tail)
		qp->wq_tail->cfs_next = rp;
	else
		qp->wq_head = rp;
	qp->wq_tail = rp;
	rp->cfs_next = NULL;
	qp->wq_length++;
	if (qp->wq_length > qp->wq_max_len)
		qp->wq_max_len = qp->wq_length;

	sv_signal(&qp->wq_req_sv);
out:
	mutex_spinunlock(&qp->wq_queue_lock, ospl);
	ASSERT(!issplhi(getsr()));
	return (error);
}

static void
cachefs_async_read(struct cachefs_io_req *prp, cred_t *cr)
{
	struct cnode *cp = prp->cp_cp;
	struct buf *bp = prp->cp_buf;
	int ospl;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_async_read,
		current_pid(), prp, cr);
	ASSERT(VALID_ADDR(prp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(cp->c_nio > 0);
	ASSERT(cp->c_ioflags & CIO_ASYNCREAD);
	(void)cachefsio_read( cp, bp, cr );
	ospl = mutex_spinlock(&cp->c_iolock);
	if (--cp->c_nio == 0) {
		cp->c_ioflags &= ~(CIO_ASYNCWRITE | CIO_ASYNCREAD);
		sv_broadcast(&cp->c_iosv);
	}
	mutex_spinunlock(&cp->c_iolock, ospl);
}

static void
cachefs_async_write(struct cachefs_io_req *prp, cred_t *cr)
{
	struct cnode *cp = prp->cp_cp;
	struct buf *bp = prp->cp_buf;
	int ospl;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_async_write,
		current_pid(), prp, cr);
	ASSERT(VALID_ADDR(prp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(cp->c_nio > 0);
	ASSERT(cp->c_ioflags & CIO_ASYNCWRITE);
	(void)cachefsio_write( cp, bp, cr );
	ospl = mutex_spinlock(&cp->c_iolock);
	if (--cp->c_nio == 0) {
		cp->c_ioflags &= ~(CIO_ASYNCWRITE | CIO_ASYNCREAD);
		sv_broadcast(&cp->c_iosv);
	}
	mutex_spinunlock(&cp->c_iolock, ospl);
}

void
cachefs_do_req(struct cachefs_req *rp)
{
	int ospl;
	cnode_t *dcp;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_do_req, current_pid(),
		rp, rp->cfs_cmd);
	ASSERT(VALID_ADDR(rp));
	ASSERT(!issplhi(getsr()));
	switch (rp->cfs_cmd) {
	case CFS_POPULATE_DIR:		/* populate a file */
		CFS_DEBUG(CFSDEBUG_ASYNC,
			printf( "cachefs_do_req: CFS_POPULATE_DIR\n" ));
		dcp = rp->cfs_req_u.cu_popdir.cpd_cp;
		rw_enter(&dcp->c_rwlock, RW_WRITER);
		cachefs_populate_dir(dcp, rp->cfs_cr);
		ASSERT(!issplhi(getsr()));
		rw_exit(&dcp->c_rwlock);
		VN_RELE(CTOV(dcp));
		CNODE_TRACE(CNTRACE_HOLD, dcp, (void *)cachefs_do_req,
			CTOV(dcp)->v_count, 0);
		break;
	case CFS_CACHE_SYNC:
		CFS_DEBUG(CFSDEBUG_ASYNC,
			printf( "cachefs_do_req: CFS_CACHE_SYNC\n" ));
		cachefs_cache_sync(rp->cfs_req_u.cu_fs_sync.cf_cachep);
		ospl = mutex_spinlock(
			&rp->cfs_req_u.cu_fs_sync.cf_cachep->c_contentslock);
		rp->cfs_req_u.cu_fs_sync.cf_cachep->c_syncs--;
		mutex_spinunlock(
			&rp->cfs_req_u.cu_fs_sync.cf_cachep->c_contentslock, ospl);
		ASSERT(!issplhi(getsr()));
		break;
	case CFS_ASYNCREAD:
		CFS_DEBUG(CFSDEBUG_ASYNC,
			printf( "cachefs_do_req: CFS_ASYNCREAD\n" ));
		cachefs_async_read(&rp->cfs_req_u.cu_io, rp->cfs_cr);
		ASSERT(!issplhi(getsr()));
		break;
	case CFS_ASYNCWRITE:
		CFS_DEBUG(CFSDEBUG_ASYNC,
			printf( "cachefs_do_req: CFS_ASYNCWRITE\n" ));
		cachefs_async_write(&rp->cfs_req_u.cu_io, rp->cfs_cr);
		ASSERT(!issplhi(getsr()));
		break;
	case CFS_NOOP:
		CFS_DEBUG(CFSDEBUG_ASYNC,
			printf( "cachefs_do_req: CFS_NOOP\n" ));
		break;
	default:
		panic("c_do_req: Invalid CFS async operation\n");
	}
	crfree(rp->cfs_cr);
	CACHEFS_ZONE_FREE(Cachefs_req_zone, rp);
}




#ifdef DEBUG
int cachefs_mem_usage = 0;
int cachefs_zone_usage = 0;
struct km_wrap *Cachefs_memlist = NULL;

lock_t cachefs_kmem_lock;

void *
cachefs_kmem_alloc(void *(*func)(size_t, int), int size, int flag)
{
	caddr_t mp = NULL;
	struct km_wrap *kwp;
	int n = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
	int allocsize = n + (2 * WRAPSIZE);
	int old_spl;

	ASSERT(size != 0);
	ASSERT(n >= size);
	ASSERT(n < (size + sizeof(void *)));
	mp = (func)(allocsize, flag);
	if (mp == NULL) {
		return (NULL);
	}
	old_spl = mutex_spinlock( &cachefs_kmem_lock );
	/*LINTED alignment okay*/
	kwp = (struct km_wrap *)mp;
	kwp->kw_size = allocsize;
	kwp->kw_req = size;
	kwp->kw_caller = (void *)__return_address;
	kwp->kw_prev = NULL;
	kwp->kw_next = Cachefs_memlist;
	if (Cachefs_memlist) {
		Cachefs_memlist->kw_prev = kwp;
	}
	Cachefs_memlist = kwp;
	/*LINTED alignment okay*/
	kwp->kw_other = (struct km_wrap *) ((__psunsigned_t)mp + WRAPSIZE + n);
	kwp = (struct km_wrap *)kwp->kw_other;
	ASSERT(!((__psunsigned_t)kwp & (sizeof(void *) - 1)));
	ASSERT((int)(((__psunsigned_t)kwp + (__psunsigned_t)WRAPSIZE) -
		(__psunsigned_t)mp) == allocsize);
	kwp->kw_size = allocsize;
	kwp->kw_req = size;
	/*LINTED alignment okay*/
	kwp->kw_other = (struct km_wrap *)mp;
	kwp->kw_caller = kwp->kw_prev = kwp->kw_next = NULL;

	ASSERT(cachefs_mem_usage >= 0);
	cachefs_mem_usage += allocsize;
	mutex_spinunlock( &cachefs_kmem_lock, old_spl );

	ASSERT(VALID_ADDR((__psunsigned_t)mp + (__psunsigned_t)WRAPSIZE));
	ASSERT(!(((__psunsigned_t)mp + (__psunsigned_t)WRAPSIZE) &
		(__psunsigned_t)(sizeof(void *) - 1)));
	CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_kmem_alloc,
		mp, allocsize, __return_address);

	return (mp + (__psunsigned_t)WRAPSIZE);
}

int
cachefs_kmem_validate(caddr_t mp, int size)
{
	struct km_wrap *front_kwp;
	struct km_wrap *back_kwp;
	int n = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
	int freesize = n + (2 * WRAPSIZE);

	ASSERT(n < (size + sizeof(void *)));
	ASSERT(VALID_ADDR(mp));
	ASSERT(VALID_ADDR((u_long)mp + (u_long)n));
	/*LINTED alignment okay*/
	front_kwp = (struct km_wrap *)(mp - (__psunsigned_t)WRAPSIZE);
	/*LINTED alignment okay*/
	back_kwp = (struct km_wrap *)((caddr_t)mp + n);
	ASSERT(VALID_ADDR(front_kwp));
	ASSERT(VALID_ADDR(back_kwp));

	return(!((__psunsigned_t)front_kwp & (sizeof(void *) - 1)) &&
		!((__psunsigned_t)back_kwp & (sizeof(void *) - 1)) &&
		(front_kwp->kw_req == size) &&
		(front_kwp->kw_other == back_kwp) &&
		(front_kwp->kw_size == freesize) &&
		(back_kwp->kw_req == size) &&
		(back_kwp->kw_other == front_kwp) &&
		(back_kwp->kw_size == freesize) &&
		(back_kwp->kw_caller == NULL) &&
		(back_kwp->kw_prev == NULL) &&
		(back_kwp->kw_next == NULL));
}

/* ARGSUSED */
int
cachefs_zone_validate(caddr_t mp, int size)
{
#ifdef CFS_DEBUG_ZONES
	return (cachefs_kmem_validate(mp, size));
#else
	return(1);
#endif
}

void
cachefs_kmem_free(caddr_t mp, int size)
{
	struct km_wrap *front_kwp;
	struct km_wrap *back_kwp;
	int n = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
	int freesize = n + (2 * WRAPSIZE);
	caddr_t p;
	int old_spl;

	ASSERT(cachefs_kmem_validate(mp, size));

	/*LINTED alignment okay*/
	front_kwp = (struct km_wrap *)(mp - (__psunsigned_t)WRAPSIZE);
	/*LINTED alignment okay*/
	back_kwp = (struct km_wrap *)((caddr_t)mp + n);

	old_spl = mutex_spinlock( &cachefs_kmem_lock );
	if (front_kwp->kw_next) {
		front_kwp->kw_next->kw_prev = front_kwp->kw_prev;
	}
	if (front_kwp->kw_prev) {
		front_kwp->kw_prev->kw_next = front_kwp->kw_next;
	} else {
		Cachefs_memlist = front_kwp->kw_next;
	}
	cachefs_mem_usage -= freesize;
	ASSERT(cachefs_mem_usage >= 0);
	ASSERT(!cachefs_mem_usage || Cachefs_memlist);
	mutex_spinunlock( &cachefs_kmem_lock, old_spl );

	p = (caddr_t)front_kwp;
	front_kwp->kw_size = back_kwp->kw_size = 0;
	front_kwp->kw_other = back_kwp->kw_other = NULL;
	CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_kmem_free,
		n, freesize, __return_address);
	kmem_free(p, freesize);
}

void *
cachefs_zone_alloc(zone_t *zone, int flag)
{
#if !defined(CFS_DEBUG_ZONES)
	void *ptr;
#endif

	ASSERT(VALID_ADDR(zone));
	ASSERT(kmem_zone_unitsize(zone));
	cachefs_zone_usage += kmem_zone_unitsize(zone); 
	ASSERT(cachefs_zone_usage >= kmem_zone_unitsize(zone));
#ifdef CFS_DEBUG_ZONES
	return (cachefs_kmem_alloc(kmem_alloc, kmem_zone_unitsize(zone), flag));
#else
	ptr = kmem_zone_zalloc(zone, flag);
	CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_zone_alloc,
		ptr, zone, __return_address);
	return(ptr);
#endif
}

void *
cachefs_zone_zalloc(zone_t *zone, int flag)
{
#if !defined(CFS_DEBUG_ZONES)
	void *ptr;
#endif

	ASSERT(VALID_ADDR(zone));

	ASSERT(kmem_zone_unitsize(zone));
	cachefs_zone_usage += kmem_zone_unitsize(zone);
	ASSERT(cachefs_zone_usage >= kmem_zone_unitsize(zone));
#ifdef CFS_DEBUG_ZONES
	return (cachefs_kmem_alloc(kmem_zalloc, kmem_zone_unitsize(zone), flag));
#else
	ptr = kmem_zone_zalloc(zone, flag);
	CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_zone_zalloc,
		ptr, zone, __return_address);
	return(ptr);
#endif
}

void
cachefs_zone_free(zone_t *zone, void *ptr)
{
	ASSERT(VALID_ADDR(zone));
	ASSERT(VALID_ADDR(ptr));

	ASSERT(kmem_zone_unitsize(zone));
	cachefs_zone_usage -= kmem_zone_unitsize(zone);
	ASSERT(cachefs_zone_usage >= 0);
#ifdef CFS_DEBUG_ZONES

	cachefs_kmem_free(ptr, kmem_zone_unitsize(zone));
#else
	CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_zone_free,
		ptr, zone, __return_address);
	kmem_zone_free(zone, ptr);
#endif
}

static char *
cfs_flush_flags( int flags )
{
	static char flagstr[37];

	flagstr[0] = 0;
	if ( flags ) {
		if ( flags & CFS_INVAL ) {
			strcat( flagstr, "CFS_INVAL " );
		}
		if ( flags & CFS_FREE ) {
			strcat( flagstr, "CFS_FREE " );
		}
		if ( flags & CFS_TRUNC ) {
			strcat( flagstr, "CFS_TRUNC" );
		}
	} else {
		strcat( flagstr, "-NONE-" );
	}
	return( flagstr );
}
#endif /* DEBUG */

int
cachefs_flushvp(
	bhv_desc_t *bdp,
	off_t offset,
	size_t len,
	int flags,
	uint64_t bflags )
{
	int error = 0;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cnode_t *cp = BHVTOC(bdp);
	int ospl;
	u_long bsize = CFS_BMAPSIZE(cp);

	CFS_DEBUG(CFSDEBUG_FLUSH,
		printf(
		"cachefs_flushvp: ENTER vp %p off %llx len %lx flags %s bflags %llx\n",
			vp, offset, (long)len, cfs_flush_flags(flags),
			bflags));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_flushvp, current_pid(),
		vp, offset);
	ASSERT(VALID_ADDR(vp));
	ASSERT(!issplhi(getsr()));
	/*
	 * If offset and len are both 0, then the entire file is to be flushed
	 * otherwise, a range is to be flushed.
	 * flags determines what sort of flushing is to be done.  The flags
	 * may consist of any one of the following:  CFS_INVAL, CFS_FREE, or
	 * CFS_TRUNC.  The flags have the following meanings:
	 *
	 *		CFS_INVAL	- flush and toss
	 *		CFS_FREE	- flush and free, but keep for reclaim
	 *		CFS_TRUNC	- truncate
	 *
	 * CFS_INVAL and CFS_TRUNC only make sense with offset and len == 0
	 */
	if ( (offset == (off_t)0) && (len == (size_t)0) ) {
		len = (size_t)((cp->c_size + (off_t)(bsize - 1)) &
			~(off_t)(bsize - 1)) + (cachefs_readahead * bsize);
		if ( flags & CFS_TRUNC ) {
			CFS_DEBUG(CFSDEBUG_FLUSH,
				printf("cachefs_flushvp: truncating, file size %d flush len %d "
					"vpages %d\n", (int)cp->c_size, (int)len, vp->v_pgcnt));
			VOP_TOSS_PAGES(vp, 0, len - 1, FI_NONE);
			ASSERT(!VN_DIRTY(vp));
		} else if ( flags & (CFS_INVAL | CFS_FREE) ) {
			CFS_DEBUG(CFSDEBUG_FLUSH,
				printf("cachefs_flushvp: invalidating and freeing, file size "
					"%d flush len %d " "vpages %d\n", (int)cp->c_size,
					(int)len, vp->v_pgcnt));
			VOP_FLUSHINVAL_PAGES( vp, offset, offset + len - 1, FI_NONE);
			/*
			 * Any dirty pages left are bad ones.  Clean them out.
			 */
			if (VN_DIRTY(vp)) {
				CFS_DEBUG(CFSDEBUG_FLUSH,
					printf("cachefs_flushvp: dirty pages remain after "
						"invalidation, vp 0x%p\n", vp));
				VOP_TOSS_PAGES(vp, 0, len - 1, FI_NONE);
				ASSERT(!VN_DIRTY(vp));
			}
		} else {
			CFS_DEBUG(CFSDEBUG_FLUSH,
				printf("cachefs_flushvp: flushing, file size %d flush len %d "
					"vpages %d\n", (int)cp->c_size, (int)len, vp->v_pgcnt));
			VOP_FLUSH_PAGES( vp, offset, offset + len - 1, bflags, FI_NONE, error );
			CFS_DEBUG(CFSDEBUG_ERROR,
				if (error)
					printf("cachefs_flushvp: pflushvp error %d\n", error));
		}
		CFS_DEBUG(CFSDEBUG_FLUSH,
			printf("cachefs_flushvp: flushed, vpages %d\n", vp->v_pgcnt));
	} else {
		if ( flags & (CFS_TRUNC | CFS_INVAL) ) {
			error = EINVAL;
			CFS_DEBUG(CFSDEBUG_FLUSH | CFSDEBUG_ERROR,
				printf("cachefs_flushvp: CFS_TRUNC | CFS_INVAL "
					"with non-zero offset and/or len\n"));
		} else if ( flags & CFS_FREE ) {
			VOP_FLUSHINVAL_PAGES( vp, offset, offset+len-1, FI_NONE );
		} else {
			error = chunkpush( vp, offset, offset+len-1, bflags );
			CFS_DEBUG(CFSDEBUG_ERROR,
				if (error)
					printf("cachefs_flushvp: chunkpush error %d\n", error));
		}
		CFS_DEBUG(CFSDEBUG_ERROR,
			if ( !error )
				printf("cachefs_flushvp: flushed, file size %d flush len %d\n",
					(int)cp->c_size, (int)len));
	}
	/*
	 * wait for asynchronous I/O on the cnode to complete if B_ASYNC has not
	 * been specified
	 */
	if ( !error && !(bflags & B_ASYNC) ) {
		ospl = mutex_spinlock(&cp->c_iolock);
		ASSERT(!(cp->c_ioflags & (CIO_ASYNCWRITE | CIO_ASYNCREAD)) ||
			(cp->c_nio > 0));
		while ( !error && (cp->c_ioflags & (CIO_ASYNCWRITE | CIO_ASYNCREAD)) ) {
			if (error = sv_wait_sig(&cp->c_iosv, PZERO+1,
				&cp->c_iolock, ospl) ) {
					ASSERT(!issplhi(getsr()));
					break;
			}
			ASSERT(!issplhi(getsr()));
			ospl = mutex_spinlock(&cp->c_iolock);
		}
		if ( !error ) {
			mutex_spinunlock(&cp->c_iolock, ospl);
#ifdef DEBUG
		} else {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_flushvp: sv_wait_sig error %d\n", error));
#endif
		}
	}
	CFS_DEBUG(CFSDEBUG_FLUSH,
		printf("cachefs_flushvp: EXIT error = %d\n", error));
	return( error );
}

/*
 * Construct an ascii name from a file id.
 */
void
make_ascii_name(fid_t *fidp, char *strp)
{
	int i;
	u_int index;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)make_ascii_name, current_pid(),
		fidp, strp);
	ASSERT(VALID_ADDR(fidp));
	ASSERT(VALID_ADDR(strp));
	for (i = 0; i < fidp->fid_len; i++) {
		index = fidp->fid_data[i] & 0xf;
		*strp++ = "0123456789abcdef"[index];
		index = (fidp->fid_data[i] >> 4) & 0xf;
		*strp++ = "0123456789abcdef"[index];
	}
	*strp = '\0';
}

u_int
fidhash( fid_t *fidp, u_int hashsize )
{
	u_int hashval = 0;
	int i;

	ASSERT(VALID_ADDR(fidp));
	ASSERT(fidp->fid_len <= MAXFIDSZ);
	hashval = fidp->fid_len;
	for ( i = 0; i < fidp->fid_len; i++ ) {
		hashval += fidp->fid_data[i];
	}
	return( hashval % hashsize );
}

/*
 * File header cache.
 */

/*
 * LRU entry positions.
 */
#define TAIL				1
#define HEAD				2

/*
 * Number of slots in the file header cache.  This is chosen to be
 * the largest prime number less than 512.
 */
#define FILEHEADER_CACHE_SLOTS		509

/*
 * File header LRU list head and tail.
 */
struct filheader_cache_entry *Fileheader_lru_head = NULL;
struct filheader_cache_entry *Fileheader_lru_tail = NULL;
u_int Fileheader_lru_count = 0;
extern int fileheader_cache_size;
/*
 * File header cache hash table.
 */
struct filheader_cache_entry * Fileheader_cache[FILEHEADER_CACHE_SLOTS];
/*
 * Spin lock to protect file header cache and LRU.
 */
lock_t Fileheader_cache_lock;
/*
 * Fileheader cache versioning to minimize search restarts after
 * releasing Fileheader_cache_lock.
 */
u_int Fileheader_cache_version = 0;
/*
 * number of cache entries allocated
 */
int Fileheader_cache_entries = 0;

struct filheader_cache_entry *
fileheader_lru_remove(struct filheader_cache_entry *ep)
{
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_lru_remove: ENTER ep 0x%p\n", ep));
	CACHEFS_STATS->cs_fileheaders.cf_lruremoves++; \
	if (ep) {
		if (ep->fce_flags & ENTRY_LRU) {
			if (ep->fce_lrunext) {
				ep->fce_lrunext->fce_lruprev = ep->fce_lruprev;
			} else {
				ASSERT(ep == Fileheader_lru_tail);
				Fileheader_lru_tail = ep->fce_lruprev;
			}
			if (ep->fce_lruprev) {
				ep->fce_lruprev->fce_lrunext = ep->fce_lrunext;
			} else {
				ASSERT(ep == Fileheader_lru_head);
				Fileheader_lru_head = ep->fce_lrunext;
			}
			ep->fce_lrunext = ep->fce_lruprev = NULL;
			Fileheader_lru_count--;
			ep->fce_flags &= ~ENTRY_LRU;
		}
	} else if (ep = Fileheader_lru_head) {
		ASSERT(ep->fce_flags & ENTRY_LRU);
		Fileheader_lru_head = ep->fce_lrunext;
		if (Fileheader_lru_head) {
			Fileheader_lru_head->fce_lruprev = NULL;
		} else {
			Fileheader_lru_tail = NULL;
		}
		ep->fce_lrunext = ep->fce_lruprev = NULL;
		Fileheader_lru_count--;
		ep->fce_flags &= ~ENTRY_LRU;
	}
	ASSERT(Fileheader_lru_head || !Fileheader_lru_tail);
	ASSERT(!Fileheader_lru_head || Fileheader_lru_head->fce_lrunext ||
		(Fileheader_lru_head == Fileheader_lru_tail));
	ASSERT(Fileheader_lru_count || !Fileheader_lru_head);
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_lru_remove: EXIT ep 0x%p\n", ep));
	return(ep);
}

#define FILEHEADER_LRU_ENTER(ep, loc) \
{ \
	CFS_DEBUG(CFSDEBUG_SUBR, \
		printf("FILEHEADER_LRU_ENTER(%s, line %d): ep 0x%p %s\n", __FILE__, \
			__LINE__, ep, (loc == TAIL) ? "tail" : "head")); \
	ASSERT(!(ep->fce_flags & ENTRY_LRU)); \
	ASSERT(!(ep->fce_flags & ENTRY_NEW)); \
	CACHEFS_STATS->cs_fileheaders.cf_lruenters++; \
	if (loc == TAIL) { \
		if (Fileheader_lru_tail) { \
			(ep)->fce_lruprev = Fileheader_lru_tail; \
			Fileheader_lru_tail->fce_lrunext = ep; \
			Fileheader_lru_tail = ep; \
		} else { \
			ASSERT(!Fileheader_lru_head); \
			Fileheader_lru_head = Fileheader_lru_tail = ep; \
			(ep)->fce_lruprev = NULL; \
		} \
		(ep)->fce_lrunext = NULL; \
	} else { \
		(ep)->fce_lrunext = Fileheader_lru_head; \
		(ep)->fce_lruprev = NULL; \
		if (Fileheader_lru_head) { \
			Fileheader_lru_head->fce_lruprev = ep; \
		} \
		Fileheader_lru_head = ep; \
	} \
	ep->fce_flags |= ENTRY_LRU; \
	Fileheader_lru_count++; \
	ASSERT(Fileheader_lru_head); \
	ASSERT(Fileheader_lru_head->fce_lrunext || \
		(Fileheader_lru_head == Fileheader_lru_tail)); \
}

#define FREE_CACHE_ENTRY(ep) \
{ \
	CFS_DEBUG(CFSDEBUG_SUBR, \
		printf("FREE_CACHE_ENTRY(%s, line %d): ep 0x%p\n", __FILE__, \
			__LINE__, ep)); \
	ASSERT(!((ep)->fce_flags & ENTRY_LRU)); \
	ASSERT(!((ep)->fce_flags & ENTRY_CACHED)); \
	if ((ep)->fce_header) \
		CACHEFS_ZONE_FREE(Cachefs_fileheader_zone, (ep)->fce_header); \
	if ((ep)->fce_fid) \
		CACHEFS_ZONE_FREE(Cachefs_fid_zone, (ep)->fce_fid); \
	sv_destroy(&ep->fce_wait); \
	CACHEFS_ZONE_FREE(Cachefs_fhcache_zone, ep); \
	Fileheader_cache_entries--; \
}

#define FILEHEADER_CACHE_ENTER(ep) \
{ \
	CACHEFS_STATS->cs_fileheaders.cf_cacheenters++; \
	if (Fileheader_cache[slot]) { \
		Fileheader_cache[slot]->fce_prev = newent; \
	} \
	newent->fce_next = Fileheader_cache[slot]; \
	Fileheader_cache[slot] = newent; \
	newent->fce_flags |= ENTRY_CACHED; \
	Fileheader_cache_version++; \
}

/*
 * remove the entry from the cache
 */
#define FILEHEADER_CACHE_REMOVE(ep) \
{ \
	CFS_DEBUG(CFSDEBUG_SUBR, \
		printf("FILEHEADER_CACHE_REMOVE(%s, line %d): ep 0x%p\n", __FILE__, \
			__LINE__, ep)); \
	CACHEFS_STATS->cs_fileheaders.cf_cacheremoves++; \
	if ((ep)->fce_next) { \
		(ep)->fce_next->fce_prev = (ep)->fce_prev; \
	} \
	if ((ep)->fce_prev) { \
		(ep)->fce_prev->fce_next = (ep)->fce_next; \
	} else { \
		Fileheader_cache[(ep)->fce_slot] = (ep)->fce_next; \
	} \
	(ep)->fce_flags &= ~ENTRY_CACHED; \
}

/*
 * search file header cache for an entry matching the fid for the
 * given vnode
 * if no entry is found, a place holder is entered and all subsequest
 * lookups will wait
 */
struct filheader_cache_entry *
fileheader_cache_find(vnode_t *vp)
{
	int ospl;
	u_int slot;
	fid_t *fidp = NULL;
	struct filheader_cache_entry *ep = NULL;
	struct filheader_cache_entry *newent = NULL;
	u_int vers;
	int error;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_cache_find: ENTER vp 0x%p\n", vp));
	fidp = CACHEFS_ZONE_ALLOC(Cachefs_fid_zone, KM_SLEEP);
	VOP_FID2(vp, fidp, error);
	if (!error) {
		slot = fidhash(fidp, FILEHEADER_CACHE_SLOTS);
		ospl = mutex_spinlock(&Fileheader_cache_lock);
restart:
		vers = Fileheader_cache_version;
		for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
			ASSERT((ep->fce_flags & ENTRY_DESTROY) || ep->fce_fid);
			ASSERT(fidp);
			if (!(ep->fce_flags & ENTRY_DESTROY) &&
				FID_MATCH(fidp, ep->fce_fid)) {
					CACHEFS_STATS->cs_fileheaders.cf_hits++;
					ep->fce_ref++;
					if (ep->fce_flags & ENTRY_NEW) {
						ASSERT(!(ep->fce_flags & ENTRY_LRU));
						sv_wait(&ep->fce_wait, PZERO, &Fileheader_cache_lock,
							ospl);
						ospl = mutex_spinlock(&Fileheader_cache_lock);
						if (ep->fce_flags & ENTRY_DESTROY) {
							/*
							 * last waiter frees the entry if it is
							 * marked ENTRY_DESTROY
							 */
							if (--ep->fce_ref == 0) {
								ASSERT(!ep->fce_header);
								FREE_CACHE_ENTRY(ep);
							}
							goto restart;
						} else {
							ASSERT(ep->fce_header);
							mutex_spinunlock(&Fileheader_cache_lock, ospl);
							if (newent) {
								ASSERT(newent->fce_fid == fidp);
								FREE_CACHE_ENTRY(newent);
							} else if (fidp) {
								CACHEFS_ZONE_FREE(Cachefs_fid_zone, fidp);
							}
							ASSERT((ep->fce_flags &
								(ENTRY_VALID | ENTRY_CACHED)) ==
								(ENTRY_VALID | ENTRY_CACHED));
							CFS_DEBUG(CFSDEBUG_SUBR,
								printf("fileheader_cache_find: EXIT ep 0x%p\n",
									ep));
							return(ep);
						}
					} else {
						(void)fileheader_lru_remove(ep);
						mutex_spinunlock(&Fileheader_cache_lock, ospl);
						if (newent) {
							ASSERT(newent->fce_fid == fidp);
							FREE_CACHE_ENTRY(newent);
						} else if (fidp) {
							CACHEFS_ZONE_FREE(Cachefs_fid_zone, fidp);
						}
						ASSERT((ep->fce_flags & (ENTRY_VALID | ENTRY_CACHED))
							== (ENTRY_VALID | ENTRY_CACHED));
						CFS_DEBUG(CFSDEBUG_SUBR,
							printf("fileheader_cache_find: EXIT ep 0x%p\n",
								ep));
						return(ep);
					}
			}
		}
		if (newent) {
			FILEHEADER_CACHE_ENTER(newent);
		} else {
			mutex_spinunlock(&Fileheader_cache_lock, ospl);
			newent = (struct filheader_cache_entry *)CACHEFS_ZONE_ZALLOC(
				Cachefs_fhcache_zone, KM_SLEEP);
			Fileheader_cache_entries++;
			newent->fce_flags = ENTRY_NEW;
			newent->fce_fid = fidp;
			newent->fce_ref = 1;
			newent->fce_slot = slot;
			sv_init(&newent->fce_wait, SV_DEFAULT,
				"Fileheader cache entry wait");
			ospl = mutex_spinlock(&Fileheader_cache_lock);
			if (vers != Fileheader_cache_version) {
				goto restart;
			} else {
				FILEHEADER_CACHE_ENTER(newent);
			}
		}
		ASSERT(newent);
		ASSERT(newent->fce_fid == fidp);
		ep = newent;
		newent = NULL;
		fidp = NULL;
		mutex_spinunlock(&Fileheader_cache_lock, ospl);
		ASSERT(ep);
	} else {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("fileheader_cache_find: VOP_FID2 error for vnode 0x%p\n",
				vp));
		CACHEFS_ZONE_FREE(Cachefs_fid_zone, fidp);
		ep = NULL;
	}
	CACHEFS_STATS->cs_fileheaders.cf_misses++;
	ASSERT(!ep || ((ep->fce_flags & (ENTRY_NEW | ENTRY_CACHED)) ==
		(ENTRY_NEW | ENTRY_CACHED)));
	ASSERT(!fidp);
	ASSERT(!newent);
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_cache_find: EXIT new ep 0x%p\n", ep));
	return(ep);
}

/*
 * enter a new file header into the cache and wake up any procs
 * waiting
 */
void
fileheader_cache_enter(struct filheader_cache_entry *ep, fileheader_t *fhp)
{
	int ospl;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_cache_enter: ENTER ep 0x%p, fhp 0x%p\n", ep, fhp));
	ASSERT(!ep->fce_header);
	ASSERT((ep->fce_flags & (ENTRY_NEW | ENTRY_CACHED)) ==
		(ENTRY_NEW | ENTRY_CACHED));
	ospl = mutex_spinlock(&Fileheader_cache_lock);
	Fileheader_cache_version++;
	ep->fce_header = fhp;
	ep->fce_flags &= ~ENTRY_NEW;
	ep->fce_flags |= ENTRY_VALID;
	sv_broadcast(&ep->fce_wait);
	ASSERT((ep->fce_flags & (ENTRY_VALID | ENTRY_CACHED)) ==
		(ENTRY_VALID | ENTRY_CACHED));
	mutex_spinunlock(&Fileheader_cache_lock, ospl);
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_cache_enter: EXIT\n"));
}

/*
 * Release a file header cache entry, taking it out of the cache
 * entirely.
 */
void
fileheader_cache_release(struct filheader_cache_entry *ep)
{
	int ospl;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_cache_release: ENTER ep 0x%p\n", ep));
	ASSERT(!ep->fce_header);
	ospl = mutex_spinlock(&Fileheader_cache_lock);
	FILEHEADER_CACHE_REMOVE(ep);
	/*
	 * decrement the reference count and check for waiters
	 */
	if (--ep->fce_ref) {
		CFS_DEBUG(CFSDEBUG_SUBR,
			printf("fileheader_cache_release: mark ep 0x%p destroy\n", ep));
		/*
		 * only update the version if there are processes
		 * waiting on this entry
		 */
		Fileheader_cache_version++;
		ep->fce_flags |= ENTRY_DESTROY;
		sv_broadcast(&ep->fce_wait);
	} else {
		/*
		 * nothing waiting, so free the entry
		 */
		FREE_CACHE_ENTRY(ep);
	}
	mutex_spinunlock(&Fileheader_cache_lock, ospl);
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_cache_release: EXIT\n"));
}

void
fileheader_cache_remove(vnode_t *vp)
{
	int ospl;
	u_int slot;
	fid_t frontfid;
	struct filheader_cache_entry *ep;
	int error;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_cache_remove: ENTER vp 0x%p\n", vp));
	VOP_FID2(vp, &frontfid, error);
	if (!error) {
		CACHEFS_STATS->cs_fileheaders.cf_purges++;
		slot = fidhash(&frontfid, FILEHEADER_CACHE_SLOTS);
		ospl = mutex_spinlock(&Fileheader_cache_lock);
		for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
			if (FID_MATCH(&frontfid, ep->fce_fid)) {
				if (ep->fce_ref) {
					CFS_DEBUG(CFSDEBUG_SUBR,
						printf("fileheader_cache_remove: mark ep 0x%p "
							"destroy\n", ep));
					ep->fce_flags |= ENTRY_DESTROY;
				} else {
					FILEHEADER_CACHE_REMOVE(ep);
					fileheader_lru_remove(ep);
					FREE_CACHE_ENTRY(ep);
				}
				break;
			}
		}
		mutex_spinunlock(&Fileheader_cache_lock, ospl);
	}
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("fileheader_cache_remove: EXIT\n"));
}

/*
 * Allocate a completely empty file header.  Do this through
 * fileheader_cache_find to prevent races.  When the header has
 * been allocated, enter it into the cache.
 */
fileheader_t *
alloc_fileheader(vnode_t *vp)
{
	struct filheader_cache_entry *ep;
	fileheader_t *fhp = NULL;

	if (ep = fileheader_cache_find(vp)) {
		if (!ep->fce_header) {
			fhp = (fileheader_t *)CACHEFS_ZONE_ZALLOC(Cachefs_fileheader_zone,
				KM_SLEEP);
#ifdef DEBUG
			ep->fce_caller = (void *)__return_address;
#endif /* DEBUG */
			fileheader_cache_enter(ep, fhp);
		} else {
			/*
			 * A file header already exists.  Use it.
			 */
			fhp = ep->fce_header;
			bzero((void *)fhp, sizeof(fileheader_t));
		}
	}
	return(fhp);
}

static struct filheader_cache_entry *
locate_header(fileheader_t *fhp)
{
	u_int slot;
	struct filheader_cache_entry *ep = NULL;
	int found = 0;
	struct filheader_cache_entry *entry = NULL;

	CFS_DEBUG(CFSDEBUG_SUBR, printf("locate header 0x%p\n", fhp));
	for (slot = 0; slot < FILEHEADER_CACHE_SLOTS; slot++) {
		for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
			if (ep->fce_header == fhp) {
				CFS_DEBUG(CFSDEBUG_SUBR,
					printf("    ep 0x%p: fid 0x%p ref %d flags 0x%x\n", ep,
						ep->fce_fid, ep->fce_ref, ep->fce_flags));
				found++;
				entry = ep;
			}
		}
	}
	ASSERT((found == 0) || (found == 1));
	return(entry);
}

#ifdef DEBUG
char *
entryflags_to_str(u_int flags)
{
	static char str[59];

	str[0] = '\0';
	if (flags & ENTRY_NEW) {
		strcat(str, "ENTRY_NEW ");
	}
	if (flags & ENTRY_DESTROY) {
		strcat(str, "ENTRY_DESTROY ");
	}
	if (flags & ENTRY_VALID) {
		strcat(str, "ENTRY_VALID ");
	}
	if (flags & ENTRY_LRU) {
		strcat(str, "ENTRY_LRU ");
	}
	if (flags & ENTRY_CACHED) {
		strcat(str, "ENTRY_CACHED ");
	}
	return(str);
}

static void
print_cache_entry(struct filheader_cache_entry *ep, int slot)
{
	char *sym;
	off_t offset;

	qprintf("Cache entry 0x%p:\n", ep);
	qprintf("    header: 0x%p\n", ep->fce_header);
	qprintf("       fid: 0x%p\n", ep->fce_fid);
	qprintf("       ref: %d\n", ep->fce_ref);
	qprintf("     flags: %s\n",
		entryflags_to_str(ep->fce_flags));
	qprintf("      slot: %d (expected %d)\n", ep->fce_slot,
		slot);
	sym = fetch_kname(ep->fce_caller, (void *)&offset);
	qprintf("    caller: %s+0x%llx [0x%p]\n",
		sym ? sym : "unknown", offset, ep->fce_caller);
}

void
idbg_headercache( __psint_t addr )
{
	struct filheader_cache_entry *ep = NULL;
	int slot;

	if (addr == -1) {
		for (slot = 0; slot < FILEHEADER_CACHE_SLOTS; slot++) {
			for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
				print_cache_entry(ep, slot);
			}
		}
	} else if (addr < -1) {
		ep = locate_header((fileheader_t *)addr);
		if (ep) {
			print_cache_entry(ep, ep->fce_slot);
		} else {
			qprintf("File header 0x%p not in file header cache\n",
				(void *)addr);
		}
	} else {
		qprintf("Invalid fileheader address 0x%p.\n", addr);
	}
}

static int
find_fileheader_by_vnode(vnode_t *vp, fileheader_t *fhp)
{
	fid_t frontfid;
	u_int slot;
	int ospl;
	struct filheader_cache_entry *ep = NULL;
	int found = 0;
	int error = 0;

	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(fhp));
	VOP_FID2(vp, &frontfid, error);
	if (!error) {
		slot = fidhash(&frontfid, FILEHEADER_CACHE_SLOTS);
		ospl = mutex_spinlock(&Fileheader_cache_lock);
		for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
			if (ep->fce_header == fhp) {
				ASSERT(FID_MATCH(&frontfid, ep->fce_fid));
				found++;
			}
		}
		CFS_DEBUG(CFSDEBUG_ERROR, if (!found) (void)locate_header(fhp));
		mutex_spinunlock(&Fileheader_cache_lock, ospl);
	}
	ASSERT((found == 1) || (found == 0));
	return(found);
}

int
find_fileheader_by_fid(fid_t *fidp, fileheader_t *fhp)
{
	u_int slot;
	int ospl;
	struct filheader_cache_entry *ep = NULL;
	int found = 0;

	ASSERT(VALID_ADDR(fidp));
	ASSERT(VALID_ADDR(fhp));
	slot = fidhash(fidp, FILEHEADER_CACHE_SLOTS);
	ospl = mutex_spinlock(&Fileheader_cache_lock);
	for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
		if (ep->fce_header == fhp) {
			ASSERT(FID_MATCH(fidp, ep->fce_fid));
			found++;
		}
	}
	CFS_DEBUG(CFSDEBUG_ERROR, if (!found) (void)locate_header(fhp));
	mutex_spinunlock(&Fileheader_cache_lock, ospl);
	ASSERT((found == 1) || (found == 0));
	return(found);
}
#endif /* DEBUG */

/*
 * Release a reference on a file header.  If the reference count goes to
 * zero, put the file header onto the file header LRU.  If the number of
 * entries on the LRU equals or exceeds the maximum allowed, take the
 * first entry from the LRU and free it.
 */
void
release_fileheader(fid_t *fidp, fileheader_t *fhp)
{
	u_int slot;
	int ospl;
	struct filheader_cache_entry *ep = NULL;
	struct filheader_cache_entry *relp;

	ASSERT(VALID_ADDR(fhp));
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("release_fileheader: ENTER fidp 0x%p fhp 0x%p\n", fidp, fhp));
	CFS_DEBUG(CFSDEBUG_ERROR,
		if (!fidp)
			printf("release_fileheader(line %d): NULL front FID\n", __LINE__));
	CACHEFS_STATS->cs_fileheaders.cf_releases++;
	if (!fhp) {
		return;
	}
	if (fidp) {
		slot = fidhash(fidp, FILEHEADER_CACHE_SLOTS);
		ospl = mutex_spinlock(&Fileheader_cache_lock);
		for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
			if (ep->fce_header == fhp) {
				ASSERT(FID_MATCH(fidp, ep->fce_fid));
				if (!--ep->fce_ref) {
					if (ep->fce_flags & ENTRY_DESTROY) {
						CFS_DEBUG(CFSDEBUG_SUBR,
							printf("release_fileheader: ep 0x%p destroy\n",
								ep));
						FILEHEADER_CACHE_REMOVE(ep);
						FREE_CACHE_ENTRY(ep);
					} else if (fileheader_cache_size == 0) {
						FILEHEADER_CACHE_REMOVE(ep);
						FREE_CACHE_ENTRY(ep);
					} else if (Fileheader_lru_count >= fileheader_cache_size) {
						relp = fileheader_lru_remove(NULL);
						ASSERT(relp);
						ASSERT(!relp->fce_ref);
						ASSERT(relp != ep);
						FILEHEADER_CACHE_REMOVE(relp);
						FREE_CACHE_ENTRY(relp);
						FILEHEADER_LRU_ENTER(ep, TAIL);
					} else {
						FILEHEADER_LRU_ENTER(ep, TAIL);
					}
				}
				break;
			}
		}
		if (!ep && (ep = locate_header(fhp))) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("release_fileheader: ep 0x%p missed in fid lookup\n",
					ep));
			if (!--ep->fce_ref) {
				if (ep->fce_flags & ENTRY_DESTROY) {
					CFS_DEBUG(CFSDEBUG_SUBR,
						printf("release_fileheader: ep 0x%p destroy\n", ep));
					FILEHEADER_CACHE_REMOVE(ep);
					FREE_CACHE_ENTRY(ep);
				} else if (fileheader_cache_size == 0) {
					FILEHEADER_CACHE_REMOVE(ep);
					FREE_CACHE_ENTRY(ep);
				} else if (Fileheader_lru_count >= fileheader_cache_size) {
					relp = fileheader_lru_remove(NULL);
					ASSERT(relp);
					ASSERT(!relp->fce_ref);
					ASSERT(relp != ep);
					FILEHEADER_CACHE_REMOVE(relp);
					FREE_CACHE_ENTRY(relp);
					FILEHEADER_LRU_ENTER(ep, TAIL);
				} else {
					FILEHEADER_LRU_ENTER(ep, TAIL);
				}
			}
		}
		mutex_spinunlock(&Fileheader_cache_lock, ospl);
	} else {
		ospl = mutex_spinlock(&Fileheader_cache_lock);
		for (slot = 0; slot < FILEHEADER_CACHE_SLOTS; slot++) {
			for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
				if (ep->fce_header == fhp) {
					if (!--ep->fce_ref) {
						if (ep->fce_flags & ENTRY_DESTROY) {
							FILEHEADER_CACHE_REMOVE(ep);
							FREE_CACHE_ENTRY(ep);
						} else if (fileheader_cache_size == 0) {
							FILEHEADER_CACHE_REMOVE(ep);
							FREE_CACHE_ENTRY(ep);
						} else if (Fileheader_lru_count >=
							fileheader_cache_size) {
								relp = fileheader_lru_remove(NULL);
								ASSERT(relp);
								ASSERT(!relp->fce_ref);
								FILEHEADER_CACHE_REMOVE(relp);
								FREE_CACHE_ENTRY(relp);
								FILEHEADER_LRU_ENTER(ep, TAIL);
						} else {
							FILEHEADER_LRU_ENTER(ep, TAIL);
						}
					}
					break;
				}
			}
		}
		mutex_spinunlock(&Fileheader_cache_lock, ospl);
	}
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("release_fileheader: EXIT\n"));
}

#ifdef DEBUG
/* ARGSUSED */
void
idbg_checklru( __psint_t addr )
{
	int lrucount = 0;
	int cachecount = 0;
	int newcount = 0;
	int destroycount = 0;
	int validcount = 0;
	int slot;
	struct filheader_cache_entry *ep;

	for (slot = 0; slot < FILEHEADER_CACHE_SLOTS; slot++) {
		for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
			if (ep->fce_flags & ENTRY_LRU) {
				lrucount++;
			} else {
				print_cache_entry(ep, slot);
			}
			if (ep->fce_flags & ENTRY_NEW) {
				newcount++;
			}
			if (ep->fce_flags & ENTRY_DESTROY) {
				destroycount++;
			}
			if (ep->fce_flags & ENTRY_VALID) {
				validcount++;
			}
			if (ep->fce_flags & ENTRY_CACHED) {
				cachecount++;
			}
		}
	}
	qprintf("%d entries marked LRU\n%d entries marked cached\n"
		"%d entries marked new\n%d entries marked destroy\n"
		"%d entries marked valid\nLRU count %d, cache entry count %d\n",
		lrucount, cachecount, newcount, destroycount, validcount,
		Fileheader_lru_count, Fileheader_cache_entries);
}
#endif /* DEBUG */

void
cachefs_mem_check(void)
{
	int ospl;
	struct filheader_cache_entry *ep;
	struct filheader_cache_entry *relp;
	int slot;

	if (Fileheader_lru_count != Fileheader_cache_entries) {
		cmn_err(CE_WARN, "CacheFS: file header LRU count (%d) not equal "
			"to file header cache entry count (%d)\n", Fileheader_lru_count,
			Fileheader_cache_entries);
		ospl = mutex_spinlock(&Fileheader_cache_lock);
restart:
		for (slot = 0; slot < FILEHEADER_CACHE_SLOTS; slot++) {
			for (ep = Fileheader_cache[slot]; ep; ep = ep->fce_next) {
				if (!(ep->fce_flags & ENTRY_LRU)) {
					ep->fce_ref = 0;
					if (ep->fce_flags & ENTRY_DESTROY) {
						CFS_DEBUG(CFSDEBUG_SUBR,
							printf("release_fileheader: ep 0x%p destroy\n",
								ep));
						FILEHEADER_CACHE_REMOVE(ep);
						FREE_CACHE_ENTRY(ep);
					} else if (fileheader_cache_size == 0) {
						FILEHEADER_CACHE_REMOVE(ep);
						FREE_CACHE_ENTRY(ep);
					} else if (Fileheader_lru_count >= fileheader_cache_size) {
						relp = fileheader_lru_remove(NULL);
						ASSERT(relp);
						ASSERT(!relp->fce_ref);
						ASSERT(relp != ep);
						FILEHEADER_CACHE_REMOVE(relp);
						FREE_CACHE_ENTRY(relp);
						FILEHEADER_LRU_ENTER(ep, TAIL);
					} else {
						FILEHEADER_LRU_ENTER(ep, TAIL);
					}
					goto restart;
				}
			}
		}
		mutex_spinunlock(&Fileheader_cache_lock, ospl);
	}
}

/* Calculate a 16 bit ones-complement checksum for a single buffer.
 *	This routine always adds even address bytes to the high order 8 bits
 *	of the 16 bit checksum and odd address bytes are added to the low
 *	order 8 bits of the 16 bit checksum.  The caller must swap bytes in
 *	the sum to make this correct.
 *
 * The caller must ensure that the length is not zero or > 32K.
 */
static u_int
cksum(ushort *src,			/* first byte */
	     int len)			/* # of bytes */
{
	u_int ck = 0;

	ASSERT(!((__psint_t)src & 1));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cksum, current_pid(), src,
		len);
	ASSERT(VALID_ADDR(src));
	ASSERT(VALID_ADDR((__psint_t)src + (__psint_t)len));
	/* do 64 byte blocks for the 128-byte cache line of the IP17 */
	while (len >= 64) {
		ck += src[0]; ck += src[1]; ck += src[2]; ck += src[3];
		ck += src[4]; ck += src[5]; ck += src[6]; ck += src[7];

		ck += src[8]; ck += src[9]; ck += src[10]; ck += src[11];
		ck += src[12]; ck += src[13]; ck += src[14]; ck += src[15];

		ck += src[16]; ck += src[17]; ck += src[18]; ck += src[19];
		ck += src[20]; ck += src[21]; ck += src[22]; ck += src[23];

		ck += src[24]; ck += src[25]; ck += src[26]; ck += src[27];
		ck += src[28]; ck += src[29]; ck += src[30]; ck += src[31];
		src += 32;
		len -= 64;
	}

	/* we have < 64 bytes remaining */
	if (0 != (len&32)) {
		ck += src[0]; ck += src[1]; ck += src[2]; ck += src[3];
		ck += src[4]; ck += src[5]; ck += src[6]; ck += src[7];
		ck += src[8]; ck += src[9]; ck += src[10]; ck += src[11];
		ck += src[12]; ck += src[13]; ck += src[14]; ck += src[15];
		src += 16;
	}
	if (0 != (len&16)) {
		ck += src[0]; ck += src[1]; ck += src[2]; ck += src[3];
		ck += src[4]; ck += src[5]; ck += src[6]; ck += src[7];
		src += 8;
	}
	if (0 != (len&8)) {
		ck += src[0]; ck += src[1]; ck += src[2]; ck += src[3];
		src += 4;
	}

	if (0 != (len&4)) {
		ck += src[0]; ck += src[1];
		src += 2;
	}

	if (0 != (len&2)) {
		ck += src[0];
		src += 1;
	}

	if (0 != (len&1)) {
#ifdef _MIPSEL
		ck += *(unchar*)src;
#else
		ck += *(unchar*)src << 8;
#endif
	}

	ck = (ck & 0xffff) + (ck >> 16);
	return(0xffff & (ck + (ck >> 16)));
}

int
cachefs_write_file_header(cnode_t *cp, vnode_t *vp, fileheader_t *fhp,
	int fh_blocksize, int alignment)
{
	int seg;
	int wflags;
	int ospl;
	int error = 0;
	struct uio uio;
	iovec_t iov;
	caddr_t pbuf = NULL;
	caddr_t mem = NULL;
	int allocsize = fh_blocksize + alignment - 1;

	CFS_DEBUG(CFSDEBUG_SUBR | CFSDEBUG_WRITEHDR,
		printf("cachefs_write_file_header: ENTER cp 0x%p vp 0x%p allocents "
			"%d\n", cp, vp, (int)fhp->fh_metadata.md_allocents));
	CACHEFUNC_TRACE(CFTRACE_WRITEHDR, (void *)cachefs_write_file_header,
		cp, vp, fhp->fh_metadata.md_allocents);
	ASSERT(valid_file_header(fhp, NULL));
	ASSERT(!cp || VALID_ADDR(cp));
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(fhp));
	ASSERT(allocsize >= sizeof(fileheader_t));
	/*
	 * By default, we use the caller's memory as the output buffer.
	 * If we must allocate an intermediate buffer, it will be done below.
	 * Allocate directly through kmem_alloc as opposed to using the cachefs
	 * allocation wrappers.  This is because the allocated address must be
	 * cache aligned.  (It may also have to be aligned to some other value.)
	 */
	pbuf = (caddr_t)fhp;
	if (!(alignment & (alignment - 1))) {
		/*
		 * If the file header supplied by the caller is not already aligned,
		 * we must allocate an aligned buffer and do bcopy.
		 */
		if (cp || (__psunsigned_t)pbuf & (__psunsigned_t)(alignment - 1)) {
			pbuf = mem = kmem_alloc(allocsize, KM_SLEEP | KM_CACHEALIGN);
			CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_write_file_header,
				mem, allocsize, 0);
			/*
			 * alignment is a power of 2, so align the buffer the fast way
			 * If the caller has supplied a cnode, operations on the file
			 * header must be protected.  Rather than hold a mutex across the
			 * write below, just use an intermediate buffer.
			 */
			if ((__psunsigned_t)pbuf & (__psunsigned_t)(alignment - 1)) {
				pbuf = (caddr_t)(((__psunsigned_t)pbuf +
					(__psunsigned_t)(alignment - 1)) &
					~((__psunsigned_t)(alignment - 1)));
			}
		}
		ASSERT(!((__psunsigned_t)pbuf & (__psunsigned_t)(alignment - 1)));
	} else if (cp || (__psunsigned_t)pbuf % (__psunsigned_t)(alignment)){
		pbuf = mem = kmem_alloc(allocsize, KM_SLEEP | KM_CACHEALIGN);
		CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_write_file_header,
			mem, allocsize, 0);
		/*
		 * alignment is not a power of 2, so align the buffer the long
		 * way (multiplication & division)
		 * again, we only do this if the buffer is not already properly
		 * aligned
		 */
		pbuf = (caddr_t)((((__psunsigned_t)pbuf +
			(__psunsigned_t)(alignment - 1)) * (__psunsigned_t)alignment) /
			(__psunsigned_t)alignment);
		ASSERT(!((__psunsigned_t)pbuf % (__psunsigned_t)(alignment)));
	}
	ASSERT(VALID_ADDR(pbuf));
	ASSERT(((__psint_t)pbuf + fh_blocksize) <= ((__psint_t)mem + allocsize));
	/*
	 * If a cnode was supplied, we must do some locking for protection.  We
	 * assume that if cp is supplied, then the file header pointed to by
	 * fhp is actually cp->c_fileheader.
	 * If no cnode was supplied, the file header pointer can be assumed to
	 * be private to the caller.
	 */
	if (cp) {
		ASSERT(mem);
		ASSERT((caddr_t)pbuf != (caddr_t)fhp);
		ASSERT(((__psint_t)pbuf + FILEHEADER_SIZE) <=
			((__psint_t)mem + allocsize));
		ospl = mutex_spinlock(&cp->c_statelock);
		bcopy((caddr_t)fhp, pbuf, FILEHEADER_SIZE);
		mutex_spinunlock(&cp->c_statelock, ospl);
	} else if (mem) {
		/*
		 * Copy the file header data to the aligned buffer for writing.
		 */
		ASSERT(((__psint_t)pbuf + FILEHEADER_SIZE) <=
			((__psint_t)mem + allocsize));
		bcopy((caddr_t)fhp, pbuf, FILEHEADER_SIZE);
	}
	ASSERT(valid_file_header((fileheader_t *)pbuf, NULL));
	/*
	 * Calculate the checksum for the file header.  Do this over the entire
	 * header with the checksum field itself being 0.
	 * Only calculate the sum for the header portion (FILEHEADER_SIZE), the
	 * remainder of the block is of undefined contents.
	 */
	((fileheader_t *)pbuf)->fh_metadata.md_checksum = 0;
	((fileheader_t *)pbuf)->fh_metadata.md_checksum = cksum((ushort *)pbuf,
		sizeof(fileheader_t));
	ASSERT(((fileheader_t *)pbuf)->fh_metadata.md_checksum != 0);
	ASSERT(valid_file_header((fileheader_t *)pbuf, NULL));
	CFS_DEBUG(CFSDEBUG_VALIDATE,
		ASSERT(valid_file_header((fileheader_t *)pbuf,
			cp ? cp->c_backfid : NULL)));
	if (front_dio && (fhp->fh_metadata.md_attributes.ca_type == VREG)) {
		seg = UIO_NOSPACE;
		wflags = IO_SYNC | IO_DIRECT | IO_TRUSTEDDIO;
	} else {
		seg = UIO_SYSSPACE;
		wflags = 0;
	}
	UIO_SETUP(&uio, &iov, pbuf, fh_blocksize, (off_t)0, seg, FWRITE,
		RLIM_INFINITY);
	/*
	 * Use IO_SYNC to make sure the underlying file system updates
	 * everything before returning.  For local file systems such as
	 * XFS and EFS, this means updating the inode on disk.
	 * Use IO_DIRECT to make the file system not use the buffer cache.
	 */
	WRITE_VP(vp, &uio, wflags, sys_cred, &curuthread->ut_flid, error);
	if (!error && uio.uio_resid) {
		CACHEFS_STATS->cs_fronterror++;
		error = EIO;
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_write_file_header(line %d): incomplete write of "
				"file header, resid %d\n", __LINE__, (int)uio.uio_resid));
#ifdef DEBUG
	} else if (error) {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_write_file_header(line %d): file header write "
				"error %d\n", __LINE__, error));
		ASSERT(uio.uio_sigpipe == 0);
#endif
	}
	ASSERT(cachefs_zone_validate((caddr_t)fhp, fh_blocksize));
	if (mem) {
		kmem_free(mem, allocsize);
		CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_write_file_header,
			mem, allocsize, 1);
	}
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_write_file_header: EXIT error %d allocents %d\n", error,
			(int)fhp->fh_metadata.md_allocents));
	return(error);
}

int
cachefs_write_header(cnode_t *cp, int force)
{
	int error = 0;
	int ospl;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_write_header: ENTER cp %p\n", cp));
	ASSERT(VALID_ADDR(cp));
	CACHEFS_STATS->cs_fileheaders.cf_writes++;
	CACHEFUNC_TRACE(CFTRACE_OTHER, cachefs_write_header, current_pid(),
		cp, 0);
	CNODE_TRACE(CNTRACE_WRITEHDR, cp, (void *)cachefs_write_header,
		cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
	ospl = mutex_spinlock(&cp->c_statelock);
	/*
	 * Don't bother updating files which have been marked CN_NOCACHE.
	 * The exception is cnodes marked CN_NEEDINVAL.  These must be
	 * updated if a front file exists.
	 * Don't attempt to update files for which there is no front file.
	 * Sometimes cnodes don't have file headers (c_fileheader is NULL);
	 * just ignore them.
	 */
	if (!(cp->c_flags & CN_UPDATED) || (cp->c_flags & CN_UPDATE_PENDING)) {
		mutex_spinunlock(&cp->c_statelock, ospl);
		return(0);
	} else if ((((cp->c_flags & (CN_NOCACHE | CN_NEEDINVAL)) == CN_NOCACHE) &&
		!force)) {
			cp->c_flags &= ~CN_UPDATED;
			CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_write_header,
				cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
			mutex_spinunlock(&cp->c_statelock, ospl);
			return(0);
	}
	ASSERT( VALID_ADDR(cp->c_fileheader) );
	cp->c_flags &= ~CN_UPDATED;
	cp->c_flags |= CN_UPDATE_PENDING;
	CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_write_header,
		cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
	mutex_spinunlock(&cp->c_statelock, ospl);
	if (cp->c_frontvp || !(error = cachefs_getfrontvp(cp))) {
		error = cachefs_write_file_header(cp, cp->c_frontvp, cp->c_fileheader,
			FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache),
			DIO_ALIGNMENT(C_TO_FSCACHE(cp)->fs_cache));
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_write_header: nocache cp 0x%p on "
				"error %d from cachefs_getfrontvp\n", cp, error));
		cachefs_nocache(cp);
	}
	ospl = mutex_spinlock(&cp->c_statelock);
	if (error) {
		cp->c_flags |= CN_NOCACHE;
		CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefs_write_header,
			(int)cp->c_flags, 0);
		CACHEFS_STATS->cs_nocache++;
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_write_header(line %d): nocache cp 0x%p, error %d\n",
				__LINE__, cp, error));
	}
	cp->c_flags &= ~CN_UPDATE_PENDING;
	mutex_spinunlock(&cp->c_statelock, ospl);
	CFS_DEBUG(CFSDEBUG_VALIDATE,
		if (!error && cp)
			validate_fileheader(cp, __FILE__, __LINE__));
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_write_header: EXIT error %d\n", error));
	return(error);
}

/*
 * It is assumed that the file header pointed to by fhp is isolated (i.e.,
 * it is not pointed to by any cnode).  Thus, no locking is necessary.
 */
/* ARGSUSED */
int
cachefs_read_file_header(vnode_t *vp, fileheader_t **fhp, vtype_t vtype,
	cachefscache_t *cachep)
{
	int seg;
	int rflags;
	struct uio uio;
	iovec_t iov;
	int error = 0;
	u_int header_sum = 0;
	u_int calculated_sum = 0;
	caddr_t pbuf = NULL;
	caddr_t mem = NULL;
	struct filheader_cache_entry *cache_entry;
	int alignment = DIO_ALIGNMENT(cachep);
	int minio = DIO_MINIO(cachep);
	int fh_blocksize = FILEHEADER_BLOCK_SIZE(cachep);
	int allocsize = fh_blocksize + alignment + minio;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_read_file_header: ENTER vp %p\n", vp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_read_file_header,
		current_pid(), vp, fhp);
	ASSERT(VALID_ADDR(vp));
	ASSERT(VALID_ADDR(fhp));
	ASSERT(allocsize >= sizeof(fileheader_t));
	CACHEFS_STATS->cs_fileheaders.cf_reads++;
	cache_entry = fileheader_cache_find(vp);
	if (!cache_entry) {
		return(ENOSYS);
	}
	if (*fhp = cache_entry->fce_header) {
		return(0);
	}
	ASSERT(cache_entry->fce_flags & ENTRY_NEW);
#ifdef DEBUG
	cache_entry->fce_caller = (void *)__return_address;
#endif /* DEBUG */
	/*
	 * allocate a properly aligned buffer
	 * assume that the zone is set up so that allocations will always
	 * be properly aligned
	 */
	*fhp = (fileheader_t *)CACHEFS_ZONE_ALLOC(Cachefs_fileheader_zone,
		KM_SLEEP);
	pbuf = (caddr_t)*fhp;
	if (!(alignment & (alignment - 1))) {
		/*
		 * If the file header supplied by the caller is not already aligned,
		 * we must allocate an aligned buffer and do bcopy.
		 */
#ifdef MH_R10000_SPECULATION_WAR
		/*
		 * On R10000 O2s, we have to pay attention to the I/O length
		 * constraint.
		 */
		if (((__psunsigned_t)pbuf & (__psunsigned_t)(alignment - 1)) ||
			(fh_blocksize % minio)) {
#else /* MH_R10000_SPECULATION_WAR */
		if ((__psunsigned_t)pbuf & (__psunsigned_t)(alignment - 1)) {
#endif /* MH_R10000_SPECULATION_WAR */
				pbuf = mem = kmem_zalloc(allocsize, KM_SLEEP | KM_CACHEALIGN);
				CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_read_file_header,
					mem, allocsize, 0);
				/*
				 * alignment is a power of 2, so align the buffer the fast way
				 * but only align if pbuf is not already properly aligned
				 */
				if ((__psunsigned_t)pbuf & (__psunsigned_t)(alignment - 1)) {
					pbuf = (caddr_t)(((__psunsigned_t)pbuf +
						(__psunsigned_t)(alignment - 1)) &
						~((__psunsigned_t)(alignment - 1)));
				}
				fh_blocksize = ((fh_blocksize + minio - 1) / minio) * minio;
		}
		ASSERT(!((__psunsigned_t)pbuf & (__psunsigned_t)(alignment - 1)));
#ifdef MH_R10000_SPECULATION_WAR
		ASSERT((fh_blocksize % minio) == 0);
#endif /* MH_R10000_SPECULATION_WAR */
	} else if ((__psunsigned_t)pbuf % (__psunsigned_t)(alignment)){
		pbuf = mem = kmem_zalloc(allocsize, KM_SLEEP | KM_CACHEALIGN);
		CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_read_file_header,
			mem, allocsize, 0);
		/*
		 * alignment is not a power of 2, so align the buffer the long
		 * way (multiplication & division)
		 * again, we only do this if the buffer is not already properly
		 * aligned
		 */
		pbuf = (caddr_t)((((__psunsigned_t)pbuf +
			(__psunsigned_t)(alignment - 1)) * (__psunsigned_t)alignment) /
			(__psunsigned_t)alignment);
		fh_blocksize = ((fh_blocksize + minio - 1) / minio) * minio;
		ASSERT(!((__psunsigned_t)pbuf % (__psunsigned_t)(alignment)));
		ASSERT((fh_blocksize % minio) == 0);
	}
	ASSERT(VALID_ADDR(pbuf));
	ASSERT(((__psint_t)pbuf + fh_blocksize) <= ((__psint_t)mem + allocsize));
	/*
	 * read the file header into the buffer
	 */
	if (front_dio && ((vtype == VREG) || (vtype == VNON))) {
		seg = UIO_NOSPACE;
		rflags = IO_SYNC | IO_DIRECT | IO_TRUSTEDDIO;
	} else {
		seg = UIO_SYSSPACE;
		rflags = 0;
	}
	UIO_SETUP(&uio, &iov, pbuf, fh_blocksize, (off_t)0,
		seg, FREAD, RLIM_INFINITY);
	READ_VP(vp, &uio, rflags, sys_cred, &curuthread->ut_flid, error);
	if (!error) {
		if ((fh_blocksize - uio.uio_resid) < FILEHEADER_SIZE) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_read_file_header(line %d): incomplete read of "
					"file header, resid %d\n", __LINE__, (int)uio.uio_resid));
			CACHEFS_STATS->cs_badheader.cbh_short++;
			error = EIO;
			CACHEFS_ZONE_FREE(Cachefs_fileheader_zone, *fhp);
		} else {
			/*
			 * copy the file header data into the area supplied by the
			 * caller, only copying FILEHEADER_SIZE bytes
			 * this is only done if an intermediate buffer is used, in
			 * which case, mem will be non-NULL
			 */
			if (mem) {
				bcopy(pbuf, (caddr_t)*fhp, FILEHEADER_SIZE);
			}
			if ((*fhp)->fh_metadata.md_checksum == 0) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_read_file_header(line %d): file header "
						"checksum is 0\n", __LINE__));
				(*fhp)->fh_metadata.md_state = 0;
				CACHEFS_STATS->cs_badheader.cbh_checksum++;
				error = EIO;
				CACHEFS_ZONE_FREE(Cachefs_fileheader_zone, *fhp);
			} else {
				header_sum = (u_long)(*fhp)->fh_metadata.md_checksum;
				(*fhp)->fh_metadata.md_checksum = 0;
				calculated_sum = cksum((ushort *)*fhp, sizeof(fileheader_t));
				if (calculated_sum != header_sum) {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_read_file_header(line %d): bad file "
							"header checksum, got %d (0x%x) expected %d\n",
							__LINE__, (int)header_sum, (int)header_sum,
							(int)calculated_sum));
					(*fhp)->fh_metadata.md_state = 0;
					CACHEFS_STATS->cs_badheader.cbh_checksum++;
					error = EIO;
					CACHEFS_ZONE_FREE(Cachefs_fileheader_zone, *fhp);
				} else if (!valid_file_header(*fhp, (fid_t *)NULL)) {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_read_file_header(line %d): invalid "
							"file header\n", __LINE__));
					(*fhp)->fh_metadata.md_state = 0;
					CACHEFS_STATS->cs_badheader.cbh_data++;
					error = EIO;
					CACHEFS_ZONE_FREE(Cachefs_fileheader_zone, *fhp);
				} else {
					fileheader_cache_enter(cache_entry, *fhp);
				}
			}
		}
		CFS_DEBUG(CFSDEBUG_FILEHEADER,
			if (*fhp && (*fhp)->fh_metadata.md_state & MD_INIT)
				printf("cachefs_read_file_header(line %d): uninitialized "
					"file header\n", __LINE__));
	} else {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_read_file_header(line %d): file header read "
				"error %d\n", __LINE__, error));
		CACHEFS_STATS->cs_badheader.cbh_readerr++;
		CACHEFS_ZONE_FREE(Cachefs_fileheader_zone, *fhp);
	}
	if (mem) {
		kmem_free(mem, allocsize);
		CACHEFUNC_TRACE(CFTRACE_ALLOC, (void *)cachefs_read_file_header,
			mem, allocsize, 1);
	}
	if (error) {
		ASSERT(!*fhp);
		fileheader_cache_release(cache_entry);
	}
	ASSERT(cachefs_zone_validate((caddr_t)*fhp, fh_blocksize));
	ASSERT(!*fhp || find_fileheader_by_vnode(vp, *fhp));
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_read_file_header: EXIT error %d\n", error));
	return(error);
}

int
cachefs_lookup_frontdir(vnode_t *dvp, fid_t *backfid, vnode_t **frontdirvp)
{
	char *name;
	int error;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_lookup_frontdir: ENTER vp %p\n", dvp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_lookup_frontdir,
		current_pid(), dvp, backfid);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(backfid));
	ASSERT(VALID_ADDR(frontdirvp));
	name = CACHEFS_KMEM_ALLOC(backfid->fid_len * 2 + 1, KM_SLEEP);
	make_ascii_name(backfid, name);
	ASSERT(strlen(name) < backfid->fid_len * 2 + 1);
	VOP_LOOKUP(dvp, name, frontdirvp, (struct pathname *)NULL, 0,
		(vnode_t *)NULL, sys_cred, error);
	if (error && (error != ENOENT)) {
		CACHEFS_STATS->cs_fronterror++;
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_lookup_frontdir(line %d): front directory "
				"lookup error, file %s: %d\n", __LINE__, name, error));
	}
	CACHEFS_KMEM_FREE(name, backfid->fid_len * 2 + 1);
	ASSERT( error || *frontdirvp );
	ASSERT( !error || !*frontdirvp );
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_lookup_frontdir: EXIT error %d\n", error));
	return(error);
}

#ifdef DEBUG
int
cachefs_name_is_legal(char *name)
{
	int legal = 1;

	ASSERT(VALID_ADDR(name));
	if (name) {
		if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0)) {
			legal = 0;
		} else {
			while (*name) {
				if (*name == '/') {
					legal = 0;
					break;
				}
				name++;
			}
		}
	} else {
		legal = 0;
	}
	return(legal);
}
#endif

int
cachefs_create_frontdir(cachefscache_t *cachep, vnode_t *dvp, fid_t *backfid,
	vnode_t **frontdirvp)
{
	char *name;
	int error;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_create_frontdir: ENTER cachep %p, vnode 0x%p\n",
			cachep, dvp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_create_frontdir,
		current_pid(), cachep, dvp);
	ASSERT(VALID_ADDR(cachep));
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(backfid));
	ASSERT(VALID_ADDR(frontdirvp));
	name = CACHEFS_KMEM_ALLOC(backfid->fid_len * 2 + 1, KM_SLEEP);
	make_ascii_name(backfid, name);
	ASSERT(strlen(name) < backfid->fid_len * 2 + 1);
	ASSERT(cachefs_name_is_legal(name));
	error = cachefs_allocfile(cachep);
	if (!error) {
		VOP_MKDIR(dvp, name, &Cachefs_dir_attr, frontdirvp, sys_cred,
			error);
		if (error) {
			CACHEFS_STATS->cs_fronterror++;
			CFS_DEBUG(CFSDEBUG_ERROR,
				if (error != EEXIST)
					printf("cachefs_create_frontdir(line %d): error creating "
						"front directory, file %s: %d\n", __LINE__,
						name, error));
		}
	} else {
		CACHEFS_STATS->cs_fronterror++;
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_create_frontdir(line %d): error allocating "
				"front directory, file %s: %d\n", __LINE__, name, error));
	}
	CACHEFS_KMEM_FREE(name, backfid->fid_len * 2 + 1);
	ASSERT( error || *frontdirvp );
	ASSERT( !error || !*frontdirvp );
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_create_frontdir: EXIT error %d\n", error));
	return(error);
}

/*
 * create a front file
 */
int
cachefs_create_frontfile( cachefscache_t *cachep, vnode_t *dvp, char *name,
	vnode_t **frontvp, fid_t *backfid, fileheader_t **fhpp, vtype_t vtype )
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_create_frontfile: ENTER cachep %p, vnode 0x%p, "
			"name %s\n", cachep, dvp, name));
	ASSERT(vtype != VNON);
	CACHEFUNC_TRACE(CFTRACE_CREATE, (void *)cachefs_create_frontfile,
		current_pid(), cachep, dvp);
	ASSERT(VALID_ADDR(cachep));
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(name));
	ASSERT(VALID_ADDR(frontvp));
	ASSERT(!backfid || VALID_ADDR(backfid));
	ASSERT(VALID_ADDR(fhpp));
	ASSERT(cachefs_name_is_legal(name));
	/*
	 * First, allocate the file and enough blocks for the file header.
	 * This step is merely internal accounting.
	 */
	error = cachefs_allocfile(cachep);
	if (!error) {
		/*
		 * the file header occupies a block of data defined by the minimum
		 * direct I/O transfer size
		 */
		error = cachefs_allocblocks(cachep, FILEHEADER_BLOCK_SIZE(cachep),
			sys_cred);
		if (!error) {
			/*
			 * Now, create the front file, using the standard attributes.
			 * If this succeeds, set the attributes to make sure that the
			 * file size has been properly set.  We cannot, unfortuantely,
			 * assume that the underlying file system has done this in the
			 * creation of the file.
			 */
			*frontvp = NULL;
			VOP_CREATE(dvp, name, &Cachefs_file_attr, 1,
				VREAD | VWRITE, frontvp, sys_cred, error);
			switch (error) {
				case EEXIST:
					ASSERT(!issplhi(getsr()));
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_create_frontfile(line %d): front file "
							"create error, file %s: EEXIST\n", __LINE__, name));
					VOP_LOOKUP(dvp, name, frontvp,
						(struct pathname *)NULL, 0,
						(vnode_t *)NULL, sys_cred,
						error);
					if (!error) {
						error = cachefs_read_file_header(*frontvp, fhpp,
							vtype, cachep);
						if (error) {
							VN_RELE(*frontvp);
							*frontvp = NULL;
							CACHEFS_STATS->cs_fronterror++;
							CFS_DEBUG(CFSDEBUG_ERROR,
								printf("cachefs_create_frontfile(line %d): "
									"error reading file header for file %s: "
									"%d\n", __LINE__, name, error));
						}
					}
					ASSERT(!issplhi(getsr()));
					break;
				case 0:
					ASSERT(!issplhi(getsr()));
					*fhpp = alloc_fileheader(*frontvp);
					if (!*fhpp) {
						VN_RELE(*frontvp);
						*frontvp = NULL;
						error = ENOSYS;
						CACHEFS_STATS->cs_fronterror++;
						CFS_DEBUG(CFSDEBUG_ERROR,
							printf("cachefs_create_frontfile(line %d): "
								"error allocating file header for file %s: "
								"%d\n", __LINE__, name, error));
						break;
					}
					if (backfid) {
						(*fhpp)->fh_metadata.md_backfid.fid_len =
							backfid->fid_len;
						bcopy(backfid->fid_data,
							&(*fhpp)->fh_metadata.md_backfid.fid_data,
							backfid->fid_len);
						(*fhpp)->fh_metadata.md_state = MD_INIT | MD_MDVALID;
					} else {
						(*fhpp)->fh_metadata.md_state = MD_INIT;
					}
					(*fhpp)->fh_metadata.md_attributes.ca_type = vtype;
					ASSERT(!issplhi(getsr()));
					break;
				default:
					CACHEFS_STATS->cs_fronterror++;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_create_frontfile(line %d): front file "
							"create error, file %s: %d\n", __LINE__, name,
							error));
					break;
			}
		} else {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_create_frontfile(line %d): front block "
					"allocation error for %d bytes, file %s: %d\n", __LINE__,
					FILEHEADER_BLOCK_SIZE(cachep), name, error));
		}
#ifdef DEBUG
	} else {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_create_frontfile(line %d): front file allocation "
				"error, file %s: %d\n", __LINE__, name, error));
#endif
	}
	/*
	 * We return with no error and the front file(s) held (v_count > 1) or
	 * with an error and *frontvp NULL.
	 */
	ASSERT( error || *frontvp );
	ASSERT( !error || (*frontvp == NULL) );
	ASSERT( error || (*frontvp)->v_count );
	/*
	 * If there was no error, *fhpp must be non-NULL, otherwise it must
	 * be NULL.
	 */
	ASSERT( error || *fhpp );
	ASSERT( !error || (*fhpp == NULL) );
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_create_frontfile: EXIT error %d\n", error));
	return( error );
}

int
cachefs_remove_frontfile(vnode_t *dvp, char *nm)
{
	vnode_t *vp = NULL;
	int error = 0;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_remove_frontfile: ENTER dvp %p, name %s\n", dvp, nm));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_remove_frontfile,
		current_pid(), dvp, nm);
	ASSERT(VALID_ADDR(dvp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(cachefs_name_is_legal(nm));
	VOP_LOOKUP(dvp, nm, &vp, NULL, 0, NULL, sys_cred, error);
	switch (error) {
		case ENOENT:
			CFS_DEBUG(CFSDEBUG_SUBR,
				printf("cachefs_remove_frontfile: EXIT error %d\n", error));
			return(0);
		case 0:
			fileheader_cache_remove(vp);
			VN_RELE(vp);
			break;
		default:
			CACHEFS_STATS->cs_fronterror++;
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_remove_front(line %d): error looking up front "
					"file, file %s: %d\n", __LINE__, nm, error));
	}
	VOP_REMOVE(dvp, nm, sys_cred, error);
	switch (error) {
		case ENOENT:
			error = 0;
		case 0:
			break;
		default:
			CACHEFS_STATS->cs_fronterror++;
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_remove_front(line %d): error removing front "
					"file, file %s: %d\n", __LINE__, nm, error));
			break;
	}
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_remove_frontfile: EXIT error %d\n", error));
	return( error );
}

static int
cachefs_remove_frontdir_contents(fscache_t *fscp, char *dirname)
{
	uio_t uio;
	iovec_t iov;
	int error = 0;
	int eof = 0;
	char *dirbuf = NULL;
	int len;
	dirent_t *dep;
	vnode_t *dvp;

	VOP_LOOKUP(fscp->fs_cacheidvp, dirname, &dvp, NULL, 0, NULL, sys_cred,
		error);
	if (error) {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_remove_frontdir_contents(line %d): error "
				"looking up front directory, file %s: %d\n",
				__LINE__, dirname, error));
		return(error);
	}
	dirbuf = CACHEFS_KMEM_ALLOC(fscp->fs_bmapsize, KM_SLEEP);
	do {
		UIO_SETUP(&uio, &iov, dirbuf, fscp->fs_bmapsize, (off_t)0, UIO_SYSSPACE,
			0, RLIM_INFINITY);
		VOP_READDIR(dvp, &uio, sys_cred, &eof, error);
		if (!error) {
			len = fscp->fs_bmapsize - uio.uio_resid;
			if (!len) {
				break;
			}
			dep = (dirent_t *)dirbuf;
			while (len && !error) {
				ASSERT(len >= (int)DIRENTSIZE(1));
				ASSERT(len >= dep->d_reclen);
				/*
				 * if the name is not . or .., remove it
				 */
				if (strcmp(dep->d_name, ".") && strcmp(dep->d_name, "..")) {
					VOP_REMOVE(dvp, dep->d_name, sys_cred, error);
					CFS_DEBUG(CFSDEBUG_ERROR, if (error)
						printf("cachefs_remove_frontdir_contents(line %d): "
							"error removing front directory, file %s: %d\n",
							__LINE__, dep->d_name, error));
				}
				len -= (int)dep->d_reclen;
				dep = (dirent_t *)((u_long)dep + (u_long)dep->d_reclen);
			}
		}
	} while(!error && !eof);
	CACHEFS_KMEM_FREE(dirbuf, fscp->fs_bmapsize);
	VN_RELE(dvp);
	return(error);
}

int
cachefs_remove_frontdir(fscache_t *fscp, fid_t *backfid, int rem_contents)
{
	int error = 0;
	char *dirname;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_remove_frontdir: ENTER fscp 0x%p, backfid 0x%p\n",
			fscp, backfid));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_remove_frontdir,
		current_pid(), fscp, backfid);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(backfid));
	ASSERT(backfid->fid_len <= MAXFIDSZ);
	if (backfid->fid_len == 0) {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_remove_frontdir(line %d): empty file ID\n",
				__LINE__));
		CFS_DEBUG(CFSDEBUG_SUBR,
			printf("cachefs_remove_frontdir: EXIT error 0\n"));
		return(0);
	}
	dirname = CACHEFS_KMEM_ALLOC(backfid->fid_len * 2 + 1, KM_SLEEP);
	make_ascii_name(backfid, dirname);
	VOP_RMDIR(fscp->fs_cacheidvp, dirname, curuthread->ut_cdir,
		sys_cred, error);
	switch (error) {
		case ENOENT:
			error = 0;
		case 0:
		case EINTR:
			break;
		case EEXIST:
			if (rem_contents) {
				/*
				 * non-empty directory
				 * remove its contents
				 */
				error = cachefs_remove_frontdir_contents(fscp, dirname);
				if (!error) {
					VOP_RMDIR(fscp->fs_cacheidvp, dirname,
						curuthread->ut_cdir,
						sys_cred, error);
					if (!error)
						break;
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_remove_frontdir(line %d): error "
							"removing front directory, file %s: %d\n",
							__LINE__, dirname, error));
				}
				/*
				 * on error, drop through to the default case
				 */
			} else {
				break;
			}
		default:
			CACHEFS_STATS->cs_fronterror++;
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_remove_frontdir(line %d): error removing "
					"front directory, file %s: %d\n", __LINE__, dirname,
					error));
			break;
	}
	CACHEFS_KMEM_FREE(dirname, backfid->fid_len * 2 + 1);
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_remove_frontdir: EXIT error %d\n", error));
	return( error );
}

/*
 * Returns the tod in secs when the consistency of the object should
 * be checked.
 * This function is used by both the single writer and strict consistency
 * modes.
 */
u_long
cachefs_gettime_cached_object(struct fscache *fscp, vtype_t vtype, u_long mtime)
{
	u_long xsec;
	u_long acmin, acmax;
	time_t mytime = time;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_gettime_cached_object: ENTER fscp 0x%p, mtime %d\n",
			fscp, mtime));
	ASSERT(vtype != VNON);
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_gettime_cached_object,
		current_pid(), fscp, vtype);
	ASSERT(VALID_ADDR(fscp));
	/*
	 * Expire time is based on the number of seconds since the last change
	 * (i.e. files that changed recently are likely to change soon),
	 */
	if (vtype == VDIR) {
		acmin = fscp->fs_acdirmin;
		acmax = fscp->fs_acdirmax;
	} else {
		acmin = fscp->fs_acregmin;
		acmax = fscp->fs_acregmax;
	}

	xsec = mytime - mtime;
	xsec = MAX(xsec, acmin);
	xsec = MIN(xsec, acmax);
	xsec += mytime;
	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_gettime_cached_object: EXIT exp time %d\n", xsec));
	return (xsec);
}

int
await_population(cnode_t *cp)
{
	int ospl;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)await_population, current_pid(),
		cp, 0);
	ASSERT(VALID_ADDR(cp));
	ospl = mutex_spinlock(&cp->c_statelock);
	while (cp->c_flags & CN_POPULATION_PENDING) {
		if (sv_wait_sig(&cp->c_popwait_sv, (PZERO+1), &cp->c_statelock,
			ospl)) {
				ASSERT(!issplhi(getsr()));
				return(EINTR);
		}
		ASSERT(!issplhi(getsr()));
		(void)mutex_spinlock(&cp->c_statelock);
	}
	mutex_spinunlock(&cp->c_statelock, ospl);
	return(0);
}

int
cachefs_update_directory( cnode_t *dcp, int flag, cred_t *cr )
{
	struct cachefs_req *rp;
	int ospl;
	int error = 0;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_update_directory,
		current_pid(), dcp, flag);
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(RW_WRITE_HELD(&dcp->c_rwlock));
	ASSERT(!issplhi(getsr()));
	ospl = mutex_spinlock(&dcp->c_statelock);
	if (C_CACHING(dcp)) {
		if (dcp->c_flags & CN_POPULATION_PENDING) {
			ASSERT(VALID_ADDR(dcp->c_fileheader));
			/*
			 * If this is a synchronous update, wait for the in-progress
			 * population.
			 */
			if (flag & IO_SYNC) {
				do {
					rw_exit(&dcp->c_rwlock);
					if (sv_wait_sig(&dcp->c_popwait_sv, (PZERO+1),
						&dcp->c_statelock, ospl)) {
							ASSERT(!issplhi(getsr()));
							rw_enter(&dcp->c_rwlock, RW_WRITER);
							return(EINTR);
					}
					ASSERT(!issplhi(getsr()));
					rw_enter(&dcp->c_rwlock, RW_WRITER);
					(void)mutex_spinlock(&dcp->c_statelock);
				} while (dcp->c_flags & CN_POPULATION_PENDING);
				/*
				 * It is possible that CN_POPULATION_PENDING has been turned
				 * off but MD_POPULATED is still not set.
				 */
				if (!(dcp->c_fileheader->fh_metadata.md_state & MD_POPULATED)) {
					dcp->c_flags |= CN_POPULATION_PENDING;
					mutex_spinunlock(&dcp->c_statelock, ospl);
					ASSERT(!issplhi(getsr()));
					dnlc_purge_vp(CTOV(dcp));
					error = cachefs_populate_dir(dcp, cr);
					ASSERT(RW_WRITE_HELD(&dcp->c_rwlock));
					ASSERT(error ||
						(dcp->c_fileheader->fh_metadata.md_state &
						MD_POPULATED));
				} else {
					mutex_spinunlock(&dcp->c_statelock, ospl);
				}
			} else {
				mutex_spinunlock(&dcp->c_statelock, ospl);
			}
			ASSERT(!issplhi(getsr()));
		} else {
			dcp->c_fileheader->fh_metadata.md_state &= ~MD_POPULATED;
			dcp->c_flags |= (CN_POPULATION_PENDING | CN_UPDATED);
			CNODE_TRACE(CNTRACE_UPDATE, dcp, (void *)cachefs_update_directory,
				dcp->c_flags, dcp->c_fileheader->fh_metadata.md_allocents);
			ASSERT(valid_file_header(dcp->c_fileheader, NULL));
			mutex_spinunlock(&dcp->c_statelock, ospl);
			if (!dcp->c_frontvp) {
				error = cachefs_getfrontvp(dcp);
				CFS_DEBUG(CFSDEBUG_ERROR, if (error)
					printf("cachefs_update_directory(line %d): error %d "
						"getting front vnode\n", __LINE__, error));
				if (error) {
					(void)cachefs_nocache(dcp);
					error = 0;
				}
			}
			ASSERT(!issplhi(getsr()));
			dnlc_purge_vp(CTOV(dcp));
			if (flag & IO_SYNC) {
				error = cachefs_populate_dir(dcp, cr);
				ASSERT(RW_WRITE_HELD(&dcp->c_rwlock));
				ASSERT(error ||
					(dcp->c_fileheader->fh_metadata.md_state & MD_POPULATED));
			} else {
				rp = (struct cachefs_req *)
					CACHEFS_ZONE_ZALLOC(Cachefs_req_zone, KM_SLEEP);
				rp->cfs_req_u.cu_popfile.cpf_cp = dcp;
				VN_HOLD(CTOV(dcp));
				CNODE_TRACE(CNTRACE_HOLD, dcp, (void *)cachefs_update_directory,
					CTOV(dcp)->v_count, 0);
				rp->cfs_cr = cr;
				crhold(rp->cfs_cr);
				rp->cfs_cmd = CFS_POPULATE_DIR;
				error = cachefs_addqueue(rp,
					&C_TO_FSCACHE(dcp)->fs_cache->c_workq);
				if (error) {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_update_directory: cachefs_addqueue "
							"error %d\n", error));
					VN_RELE(CTOV(dcp));
					CNODE_TRACE(CNTRACE_HOLD, dcp,
						(void *)cachefs_update_directory, CTOV(dcp)->v_count,
						0);
					ospl = mutex_spinlock(&dcp->c_statelock);
					dcp->c_flags &= ~CN_POPULATION_PENDING;
					mutex_spinunlock(&dcp->c_statelock, ospl);
					CACHEFS_ZONE_FREE(Cachefs_req_zone, rp);
					error = 0;
				}
			}
			ASSERT(!issplhi(getsr()));
		}
	} else {
		mutex_spinunlock(&dcp->c_statelock, ospl);
	}
	ASSERT(RW_WRITE_HELD(&dcp->c_rwlock));
	return( error );
}

int
cachefs_irix5_fmtdirent(void **buf, struct dirent *dep, int count)
{
	register struct irix5_dirent *gdp = (struct irix5_dirent *)*buf;
	register ino_t fileno = dep->d_ino;
	register off_t offset = dep->d_off;
	register ssize_t reclen = dep->d_reclen;
	register u_short namelen = reclen - DIRENTBASESIZE;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_irix5_fmtdirent: namelen = %d, reclen = %d\n",
			namelen, (int)reclen));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_irix5_fmtdirent,
		current_pid(), buf, dep);
	ASSERT(VALID_ADDR(buf));
	ASSERT(VALID_ADDR(dep));
	if ((reclen < DIRENTSIZE(1)) || (namelen > MAXPATHLEN) ||
		(reclen > count)) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_irix5_fmtdirent: invalid entry found: reclen "
					"%d, namelen %d, count %d, offset %d\n", (int)reclen,
					(int)namelen, (int)count, (int)offset));
			return(EINVAL);
	}
	/*
	 * In order to keep the d_off values valid, we leave d_reclen the same
	 * as in the 64-bit directory entry.  All that changes is the size of
	 * d_off and the positioning of fields.
	 */
	gdp->d_reclen = reclen;
	gdp->d_ino = fileno;
	gdp->d_off = offset;
	bcopy((caddr_t)dep->d_name, (caddr_t)gdp->d_name, namelen + 1);
#ifdef DEBUG
	dep = (dirent_t *)((u_long)dep + (u_long)reclen);
	count -= (int)reclen;
	if ((count >= DIRENTSIZE(1)) && (dep->d_reclen < DIRENTSIZE(1))) {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_irix5_fmtdirent: next entry invalid: reclen "
				"%d, new dep 0x%p, buf 0x%p, *buf 0x%p, count %d\n",
				(int)dep->d_reclen, dep, buf, *buf,
				count));
	}
#endif
	*buf = (void *)((u_long)gdp + (u_long)gdp->d_reclen);
	return(0);
}

int
cachefs_search_dir(cnode_t *dcp, char *nm, lockmode_t lm)
{
	caddr_t dirbuf;
	uio_t uio;
	iovec_t iov;
	int size;
	int ospl;
	dirent_t *dep;
	int error = 0;
	int count;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_search_dir,
		current_pid(), dcp, nm);
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(C_CACHING(dcp));
	ASSERT(VALID_ADDR(dcp->c_fileheader));
	/*
	 * Do not wait for a pending population here.  That would cause a
	 * deadlock.  If a population is pending, MD_POPULATED will not be set
	 * in the metadata.
	 */
	ospl = mutex_spinlock(&dcp->c_statelock);
	if (dcp->c_fileheader->fh_metadata.md_state & MD_POPULATED) {
		size = dcp->c_fileheader->fh_allocmap[0].am_size;
		ASSERT(size > 0);
		mutex_spinunlock(&dcp->c_statelock, ospl);
		dirbuf = CACHEFS_KMEM_ALLOC(size, KM_SLEEP);
		UIO_SETUP(&uio, &iov, dirbuf, size,
			(off_t)DATA_START(C_TO_FSCACHE(dcp)), UIO_SYSSPACE, 0,
			RLIM_INFINITY);
		error = FRONT_READ_VP(dcp, &uio, 0, sys_cred);
		count = size - (int)uio.uio_resid;
		ASSERT(valid_dirents((dirent_t *)dirbuf, (int)count));
		if (!error && count) {
			dep = (dirent_t *)dirbuf;
			error = ENOENT;
			while ((count >= (int)(DIRENTSIZE(1))) &&
				(count >= (int)dep->d_reclen)) {
					if ((dep->d_reclen < DIRENTSIZE(1)) ||
						((dep->d_reclen - DIRENTBASESIZE) > MAXPATHLEN)) {
							(void)cachefs_inval_object(dcp, lm);
							error = 0;
							break;
					}
					if (strcmp(dep->d_name, nm) == 0) {
						error = 0;
						break;
					}
					count -= dep->d_reclen;
					dep = (dirent_t *)((u_long)dep + (u_long)dep->d_reclen);
			}
		}
		CACHEFS_KMEM_FREE(dirbuf, size);
	} else {
		mutex_spinunlock(&dcp->c_statelock, ospl);
		error = 0;
	}
	return(error);
}

int
cachefs_sys(int cmd, void *argp, rval_t *rvp)
{
	int error = 0;

	switch (cmd) {
		case CACHEFSSYS_REPLACEMENT:
			error = cachefs_replacement(argp, rvp);
			break;
		case CACHEFSSYS_CHECKFID:
			error = cachefs_checkfid(argp, rvp);
			break;
		case CACHEFSSYS_RECONNECT:
			error = cachefs_reconnect(argp, rvp);
			break;
		case CACHEFSSYS_MOUNT:
			error = cachefs_complete_mount(argp, rvp);
			break;
		default:
			error = EINVAL;
	}
	return(error);
}
