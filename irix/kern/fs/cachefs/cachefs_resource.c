/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_resource.c 1.45     94/04/21 SMI"
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
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
#include <ksys/vfile.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/dnlc.h>
#include <ksys/vproc.h>
#include <sys/kabi.h>
#include <sys/resource.h>

#include "cachefs_fs.h"
#include <ksys/vfile.h>
#include <sys/siginfo.h>
#include <sys/ksignal.h>
#include <sys/buf.h>
#include <sys/pfdat.h>
#include <sys/xlate.h>
#include <sys/kthread.h>

/*
 * Check a file ID for validity.
 * Return ESTALE for invalid file IDs (i.e., those which no longer refer to
 * a file).
 */
int
cachefs_checkfid(void *arg, rval_t *rvp)
{
	checkfidarg_t cfarg;
	int error = 0;
	vnode_t *vp = NULL;
	vfs_t *vfsp = NULL;

	if (!_CAP_ABLE(CAP_MOUNT_MGT)) {
		CFS_DEBUG(CFSDEBUG_RESOURCE,
			printf("cachefs_checkfid: EXIT EPERM\n"));
		return EPERM;
	}
	if (rvp) {
		rvp->r_val1 = rvp->r_val2 = (long)0;
	}
	error = copyin(arg, (void *)&cfarg, sizeof (cfarg));
	if (error) {
		return(error);
	}
	/*
	 * cf_fsid is really a device number.
	 */
	/* XXXthe fstype arg should probably be cachefsfstyp here... */
	vfsp = vfs_devsearch((dev_t)cfarg.cf_fsid, VFS_FSTYPE_ANY);
	if (vfsp) {
		VFS_VGET(vfsp, &vp, &cfarg.cf_fid, error);
		if (!error) {
			if (vp) {
				VN_RELE(vp);
				vp = NULL;
			} else {
				error = ESTALE;
			}
		} else if (error != EINTR) {
			error = ESTALE;
		}
	} else {
		error = EINVAL;
	}
	/*
	 * error returned must be 0, ESTALE, EINVAL, or EINTR
	 */
	ASSERT(!error || (error == ESTALE) || (error == EINVAL) ||
		(error == EINTR));
	ASSERT(!vp);
	return(error);
}

/* ARGSUSED */
int
cachefs_reconnect(void *arg, rval_t *rvp)
{
	if (!_CAP_ABLE(CAP_MOUNT_MGT)) {
		CFS_DEBUG(CFSDEBUG_RESOURCE,
			printf("cachefs_reconnect: EXIT EPERM\n"));
		return EPERM;
	}
	if (rvp) {
		rvp->r_val1 = rvp->r_val2 = (long)0;
	}
	return(ENOSYS);
}

#if _MIPS_SIM == _ABI64
/*
 * translator for irix5_cachefsrepl_arg to cachefsrepl_arg
 */
/* ARGSUSED */
static int
irix5_to_cachefsrepl_arg(enum xlate_mode mode, void *to, int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_cachefsrepl_arg, cachefsrepl_arg);
	
	target->ra_cachedir = (char *)(__psint_t)source->ra_cachedir;
	target->ra_list = (cachefsrep_t *)(__psint_t)source->ra_list;
	target->ra_ents = source->ra_ents;
	target->ra_timeout = source->ra_timeout;

	return 0;
}

/*
 * translator for irix5_cachefs_replace to cachefs_replace
 */
/* ARGSUSED */
static int
irix5_to_cachefs_replace(enum xlate_mode mode, void *to, int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_cachefs_replace, cachefs_replace);
	
	target->rep_cacheid = (char *)(__psint_t)source->rep_cacheid;
	target->rep_path = (char *)(__psint_t)source->rep_path;

	return 0;
}
#endif /* _ABI64 */

/*
 * called when the cnode is not known
 */
int
replace_front_file(cachefscache_t *cachep, char *path, vnode_t *vp)
{
	int error = 0;
	fileheader_t *fhp = NULL;		/* file header data */
	fid_t frontfid;

	/*
	 * Allocate and read the file header.
	 */
	ASSERT(VALID_ADDR(vp));
	error = cachefs_read_file_header(vp, &fhp, VNON, cachep);
	if (!error) {
		if (fhp->fh_metadata.md_state & MD_KEEP) {
			/*
			 * Truncate the file.
			 */
			/*
			 * Modify the file header to reflect that
			 * there is no data.  Then write the file
			 * header back out.
			 */
			fhp->fh_metadata.md_state &=
				(MD_MDVALID | MD_KEEP | MD_NOTRUNC);
			fhp->fh_metadata.md_allocents = 0;
			error = cachefs_write_file_header(NULL, vp, fhp,
				FILEHEADER_BLOCK_SIZE(cachep), DIO_ALIGNMENT(cachep));
			CFS_DEBUG(CFSDEBUG_ERROR,
				if (error)
					printf("replace_front_file(line %d): error "
						"writing file header %d\n", __LINE__, error));
			/*
			 * Truncate the file to FILEHEADERSIZE
			 * bytes.  This leaves only the file header
			 * data in the file.
			 */
			VOP_SETATTR(vp, &Cachefs_file_attr, 0,
				sys_cred, error);
			CFS_DEBUG(CFSDEBUG_ERROR,
				if (error)
					printf("replace_front_file(line %d): VOP_SETATTR "
						"error %d\n", __LINE__, error));
			VOP_FSYNC(vp, 0, sys_cred,
				(off_t)0, (off_t)-1,error);
			CFS_DEBUG(CFSDEBUG_ERROR,
				if (error)
					printf("replace_front_file(line %d): fsync error "
						"%d\n", __LINE__, error));
			VN_RELE(vp);
		}
		VOP_FID2(vp, &frontfid, error);
		if (!error) {
			CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
		} else {
			CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
		}
	} else {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("replace_front_file(line %d): file header read "
				"error %d\n", __LINE__, error));
		VN_RELE(vp);
		error = vn_remove(path, UIO_SYSSPACE,
			(vp->v_type == VDIR) ? RMDIRECTORY : RMFILE);
		CFS_DEBUG(CFSDEBUG_ERROR, if (error)
			printf("replace_front_file(line %d): unable to remove %s, "
				"error %d\n", __LINE__, path, error));
	}
	return(error);
}

/*
 * The user daemon passes in a list of files to be purged from the cache.
 */
int
cachefs_replacement(void *arg, rval_t *rvp)
{
	timespec_t wait_time;
	timespec_t remaining;
	fid_t frontfid;
	char *file_list = NULL;		/* pointer to user address of
					   replacement entry list, each entry
					   being a cachefsrep_t */
	int entsize = 0;		/* size of user's replacement entry */
					/* this will vary with the ABI */
	int i;
	size_t pathlen;
	vnode_t *frontvp = NULL;
 	replarg_t reparg;
	cachefsrep_t repent;
	char *dirname = NULL;
	size_t dirnamelen;
	int error;
	cachefscache_t *cachep = NULL;
	int ospl;
	int again = 0;
	int cflags = 0;
	char *pathname = NULL;

	CFS_DEBUG(CFSDEBUG_REPLACEMENT | CFSDEBUG_RESOURCE,
		printf("cachefs_replacement: ENTER\n"));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_replacement,
		current_pid(), arg, 0);
	if (!_CAP_ABLE(CAP_MOUNT_MGT)) {
		CFS_DEBUG(CFSDEBUG_REPLACEMENT | CFSDEBUG_RESOURCE,
			printf("cachefs_replacement: EXIT EPERM\n"));
		return EPERM;
	}
	if (rvp) {
		rvp->r_val1 = rvp->r_val2 = (long)0;
	}

	error = COPYIN_XLATE(arg, (void *)&reparg,
		ABI_IS_64BIT(get_current_abi()) ? sizeof(replarg_t) :
		MIN(sizeof(irix5_replarg_t), sizeof(replarg_t)),
		irix5_to_cachefsrepl_arg, get_current_abi(), 1);
	if (error) {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_replacement: unable to copy in args: %d\n", error));
		goto out;
	}
	if (!reparg.ra_timeout) {
		reparg.ra_timeout = replacement_timeout;
	}
	dirname = CACHEFS_ZONE_ALLOC(Cachefs_path_zone, KM_SLEEP);
	error = copyinstr(reparg.ra_cachedir, dirname, MAXPATHLEN + 1, &dirnamelen);
	if (error) {
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf(
				"cachefs_replacement: unable to copy in cache dir name: %d\n",
				error));
		goto out;
	}
	if (reparg.ra_ents) {
		if (!reparg.ra_list) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("cachefs_replacement: invalid replacement list\n"));
			error = EINVAL;
			goto out;
		}
		if (ABI_IS_64BIT(get_current_abi())) {
			entsize = sizeof(cachefsrep_t);
		} else {
			entsize = MIN(sizeof(cachefsrep_t), sizeof(irix5_cachefsrep_t));
		}
		/*
		 * point to the list of replacement entries
		 */
		file_list = (char *)reparg.ra_list;
	}

	mutex_enter(&cachefs_cachelock);
	/*
	 * Find the cache structure
	 */
	cachep = cachefscache_list_find(dirname);
	CFS_DEBUG(CFSDEBUG_REPLACEMENT,
		printf("cachefs_replacement: cachep 0x%p file list 0x%p %d entries\n",
			cachep, file_list, reparg.ra_ents));
	/*
	 * If cachep is NULL, no cache exists for the data dir name supplied.
	 */
	if (cachep == NULL) {
		error = ENOENT;
		mutex_exit(&cachefs_cachelock);
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_replacement: cache %s is not active\n", dirname));
		goto out;
	}

	if (file_list) {
		ASSERT(entsize);
		ASSERT(entsize <= sizeof(repent));
		if (getfsizelimit() == 0)
			cflags = VZFS;
		/*
		 * Process the file list.
		 * The daemon has removed all of the files.
		 */
		pathname = CACHEFS_ZONE_ALLOC(Cachefs_path_zone, KM_SLEEP);
		for (i = 0; i < reparg.ra_ents; i++) {
			/*
			 * copy in a single entry and move the entry pointer (file_list)
			 * to the next user entry
			 */
			error = COPYIN_XLATE(file_list, (void *)&repent, entsize,
				irix5_to_cachefs_replace, get_current_abi(), 1);
			if (error) {
				mutex_exit(&cachefs_cachelock);
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_replacement: unable to copy replacement "
						"entry %d: error %d\n", i, error));
				goto out;
			}
			file_list += entsize;
			/*
			 * Copy the path name in from the list entry.  Once we have this,
			 * we will use it to look up the file as well as get the cache
			 * ID.  The cache ID will be used to look up the fscache
			 * structure.
			 */
			error = copyinstr(repent.rep_path, pathname, MAXPATHLEN + 1,
				&pathlen);
			if (error) {
				mutex_exit(&cachefs_cachelock);
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_replacement: unable to copy cache ID for "
						"replacement entry %d: error %d\n", i, error));
				goto out;
			}
			/*
			 * open the front file
			 * we'll need to read the file header to get the state from
			 * the metadata
			 */
			error = vn_open(pathname, UIO_SYSSPACE, FREAD, 0, &frontvp, CROPEN,
				cflags, NULL);
			CFS_DEBUG(CFSDEBUG_ERROR,
				if (error)
					printf("cachefs_replacement(line %d): front file lookup "
						"error %d\n", __LINE__, error));
			if (!error) {
				VOP_FID2(frontvp, &frontfid, error);
				if (!error) {
					error = replace_front_file(cachep, pathname, frontvp);
				}
			}
		}
		/*
		 * free the file list and pathname data areas.
		 */
		CACHEFS_ZONE_FREE(Cachefs_path_zone, pathname);
		file_list = NULL;
	}

	error = 0;

	/*
	 * wait for the kernel to request a replacement or termination
	 */
	do {
		ospl = mutex_spinlock(&cachep->c_contentslock);
		if (cachep->c_flags & CACHE_GCDAEMON_HALT) {
			cachep->c_flags &= ~CACHE_GCDAEMON_ACTIVE;
			sv_broadcast(&cachep->c_repwait_sv);
			mutex_spinunlock(&cachep->c_contentslock, ospl);
			mutex_exit(&cachefs_cachelock);
			error = ESTALE;
			again = 0;
			CFS_DEBUG(CFSDEBUG_REPLACEMENT,
				printf("cachefs_replacement: replacement halted, error %d, "
					"again %d\n", error, again));
		} else if (cachep->c_flags & CACHE_GARBAGE_COLLECT) {
				cachep->c_flags |= CACHE_GCDAEMON_ACTIVE;
				cachep->c_flags &= ~CACHE_GARBAGE_COLLECT;
				mutex_spinunlock(&cachep->c_contentslock, ospl);
				mutex_exit(&cachefs_cachelock);
				error = 0;
				again = 0;
				CFS_DEBUG(CFSDEBUG_REPLACEMENT, printf(
					"cachefs_replacement: garbage collection request\n"));
		} else {
			sv_broadcast(&cachep->c_repwait_sv);
			cachep->c_flags &= ~CACHE_GCDAEMON_ACTIVE;
			cachep->c_flags |= CACHE_GCDAEMON_WAITING;
			mutex_exit(&cachefs_cachelock);
			CFS_DEBUG(CFSDEBUG_REPLACEMENT,
				printf("cachefs_replacement: replacement waiting for "
					"requests\n"));
			wait_time.tv_sec = reparg.ra_timeout;
			wait_time.tv_nsec = 0;
			if (sv_timedwait_sig(&cachep->c_replacement_sv, (PZERO+1) | PLTWAIT,
				&cachep->c_contentslock, ospl, 0, &wait_time, &remaining)) {
					ASSERT(!issplhi(getsr()));
					ospl = mutex_spinlock(&cachep->c_contentslock);
					cachep->c_flags &= ~CACHE_GCDAEMON_WAITING;
					mutex_spinunlock(&cachep->c_contentslock, ospl);
					error = EINTR;
					again = 0;
			} else {
				ASSERT(!issplhi(getsr()));
				ospl = mutex_spinlock(&cachep->c_contentslock);
				cachep->c_flags &= ~CACHE_GCDAEMON_WAITING;
				if (cachep->c_flags & CACHE_GCDAEMON_HALT) {
					sv_broadcast(&cachep->c_repwait_sv);
					mutex_spinunlock(&cachep->c_contentslock, ospl);
					error = ESTALE;
					again = 0;
					CFS_DEBUG(CFSDEBUG_REPLACEMENT,
						printf("cachefs_replacement: replacement halted, "
							"error %d, again %d\n", error, again));
				} else if (!remaining.tv_sec && !remaining.tv_nsec) {
					cachep->c_flags |= CACHE_GCDAEMON_ACTIVE;
					mutex_spinunlock(&cachep->c_contentslock, ospl);
					error = ETIMEDOUT;
					again = 0;
					CFS_DEBUG(CFSDEBUG_REPLACEMENT,
						printf("cachefs_replacement: replacement timeout, "
							"returning ETIMEDOUT\n"));
				} else {
					mutex_spinunlock(&cachep->c_contentslock, ospl);
					again = 1;
					CFS_DEBUG(CFSDEBUG_REPLACEMENT,
						printf("cachefs_replacement: replacement awakened, "
							"error %d, again %d\n", error, again));
					mutex_enter(&cachefs_cachelock);
				}
			}
		}
	} while (again);
out:
	ASSERT(!mutex_mine(&cachefs_cachelock));
	/*
	 * Make sure no dynamically allocated memory is retained.
	 */
	if (dirname)
		CACHEFS_ZONE_FREE(Cachefs_path_zone, dirname);
	if (pathname)
		CACHEFS_ZONE_FREE(Cachefs_path_zone, pathname);
	CFS_DEBUG(CFSDEBUG_REPLACEMENT | CFSDEBUG_RESOURCE,
		printf("cachefs_replacement: EXIT error %d\n", error));
	ASSERT(!issplhi(getsr()));
	return(error);
}

void
cachefs_replacement_halt(cachefscache_t *cachep)
{
	int ospl;
	timespec_t ts;

	CFS_DEBUG(CFSDEBUG_REPLACEMENT | CFSDEBUG_RESOURCE,
		printf("cachefs_replacement_halt: ENTER cachep 0x%x\n", cachep));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_replacement_halt,
		current_pid(), cachep, 0);
	ASSERT(VALID_ADDR(cachep));
	ospl = mutex_spinlock(&cachep->c_contentslock);
	cachep->c_flags |= CACHE_GCDAEMON_HALT;
	if (sv_signal(&cachep->c_replacement_sv)) {
		CFS_DEBUG(CFSDEBUG_REPLACEMENT,
			printf("cachefs_replacement_halt: waiting for daemon to halt\n"));
		ts.tv_sec = GCWAIT_TIME;
		ts.tv_nsec = 0;
		(void)sv_timedwait_sig( &cachep->c_repwait_sv, PZERO+1,
			&cachep->c_contentslock, ospl, 0, &ts, NULL);
	} else {
		CFS_DEBUG(CFSDEBUG_REPLACEMENT,
			printf("cachefs_replacement_halt: no daemon awakened\n"));
		mutex_spinunlock(&cachep->c_contentslock, ospl);
	}
	ASSERT(!(cachep->c_flags & CACHE_GCDAEMON_WAITING));
	CFS_DEBUG(CFSDEBUG_REPLACEMENT | CFSDEBUG_RESOURCE,
		printf("cachefs_replacement_halt: EXIT\n"));
}

void
cachefs_garbage_collect(cachefscache_t *cachep)
{
	int ospl;

	CFS_DEBUG(CFSDEBUG_RESOURCE,
		printf("cachefs_garbage_collect: ENTER cachep 0x%x\n", cachep));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_garbage_collect,
		current_pid(), cachep, 0);
	ASSERT(VALID_ADDR(cachep));
	ASSERT(!issplhi(getsr()));
	/*
	 * Wake up the garbage collection daemon.  It will scan the cache for
	 * files which have not been accessed in some time.  It will then remove
	 * enough of these so that the file and block allocations are below their
	 * respective high water marks.  The daemon runs in user space.
	 * There should be one daemon for each cache.
	 */
	CACHEFS_STATS->cs_replacements++;
	ospl = mutex_spinlock(&cachep->c_contentslock);
	/*
	 * Indicate that garbage collection is needed.  If the daemon is
	 * active, it will see this before blocking at c_replacement_sv
	 * again.
	 */
	cachep->c_flags |= CACHE_GARBAGE_COLLECT;
	sv_signal(&cachep->c_replacement_sv);
	mutex_spinunlock(&cachep->c_contentslock, ospl);
	ASSERT(!issplhi(getsr()));
	CFS_DEBUG(CFSDEBUG_RESOURCE, printf("cachefs_garbage_collect: EXIT\n"));
}

int
cachefs_allocfile(cachefscache_t *cachep)
{
	int error = 0;
	int collect = 0;
	struct statvfs sb;
	int ospl;
	/*REFERENCED*/
	int unused;

	CFS_DEBUG(CFSDEBUG_RESOURCE,
		printf("cachefs_allocfile: ENTER cachep 0x%x\n", cachep));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_allocfile, current_pid(),
		cachep, 0);
	ASSERT(VALID_ADDR(cachep));
	ASSERT(!issplhi(getsr()));

	VFS_STATVFS(cachep->c_dirvp->v_vfsp, &sb, (vnode_t *)NULL, unused);

	ospl = mutex_spinlock(&cachep->c_contentslock);

	/* if there are no more available inodes */
	if ((sb.f_files - sb.f_ffree + 1) > cachep->c_label.cl_maxfiles) {
		error = ENOSPC;
		collect = 1;
	}

	/* else if there are more available inodes */
	else {
		/*
		 * We want to garbage collect if the file allocation has
		 * exceeded the high water mark.
		 */
		collect = ((sb.f_files - sb.f_ffree + 1) >=
			cachep->c_label.cl_filehiwat);
	}

	mutex_spinunlock(&cachep->c_contentslock, ospl);

	if (collect) {
		cachefs_garbage_collect(cachep);
	}
	ASSERT(!issplhi(getsr()));

	CFS_DEBUG(CFSDEBUG_RESOURCE,
		printf("cachefs_allocfile: EXIT error %d\n", error));
	return (error);
}

/*ARGSUSED*/
int
cachefs_allocblocks(cachefscache_t *cachep, int bytes, cred_t *cr)
{
	int error = 0;
	int collect = 0;
	struct statvfs sb;
	int ospl;
	int blocks;

	CFS_DEBUG(CFSDEBUG_RESOURCE,
		printf("cachefs_allocblocks: ENTER cachep 0x%x, %d bytes\n", cachep,
			bytes));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_allocblocks,
		current_pid(), cachep, 0);
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(cachep));
	if (bytes == 0) {
		CFS_DEBUG(CFSDEBUG_RESOURCE,
			printf("cachefs_allocblocks: EXIT 0-byte request\n"));
		return(0);
	}
	ASSERT(!issplhi(getsr()));
	/*
	 * Ask the front file system for some stats.
	 * Calculate the number of blocks in use on the fron tfile system.
	 */
	VFS_STATVFS(cachep->c_dirvp->v_vfsp, &sb, (vnode_t *)NULL, error);
	if (error) {
		CACHEFS_STATS->cs_fronterror++;
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_allocblocks(line %d): front file system "
				"statvfs error: %d\n", __LINE__, error));
		return(error);
	}

	/*
	 * calculate the number of front file system blocks we want
	 */
	blocks = (bytes + sb.f_frsize - 1) / sb.f_frsize;

	ospl = mutex_spinlock(&cachep->c_contentslock);

	/*
	 * If there are no more available blocks, set the error to ENOSPC
	 * and wake up the replacement daemon.  We will wait for the
	 * daemon to finish before returning to the caller.
	 */
	if ((sb.f_blocks - sb.f_bfree + blocks) > cachep->c_label.cl_maxblks) {
		/*
		 * We are at or over the maximum block limit.  Trigger collection
		 * but don't wait for it.  Return ENOSPC.
		 */
		collect = 1;
		error = ENOSPC;
	}

	/*
	 * Otherwise, if we will equal or exceed the high water mark for
	 * block usage on the front file system with this allocation,
	 * wake up the replacement daemon, but do not wait for it.
	 */
	else {
		collect = ((sb.f_blocks - sb.f_bfree + blocks) >=
			cachep->c_label.cl_blkhiwat);
	}

	mutex_spinunlock(&cachep->c_contentslock, ospl);

	/*
	 * If we need to replace some files, wake up the daemon.
	 */
	if (collect) {
		cachefs_garbage_collect(cachep);
	}
	ASSERT(!issplhi(getsr()));

	CFS_DEBUG(CFSDEBUG_RESOURCE,
		printf("cachefs_allocblocks: EXIT error %d\n", error));
	return (error);
}
