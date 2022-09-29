/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_fscache.c 1.12     94/04/21 SMI"
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <ksys/vfile.h>
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
#include <sys/kthread.h>
#include <ksys/vproc.h>

#include "cachefs_fs.h"

/* external references */
extern struct cachefsops nopcfsops, strictcfsops, singlecfsops;
extern struct cachefsops codcfsops;
extern int cachefs_cnode_count;
extern cachefscache_t *cachefs_cachelist;

/*
 * ------------------------------------------------------------------
 *
 *		fscache_create
 *
 * Description:
 *	Creates a fscache object.
 * Arguments:
 *	cachep		cache to create fscache object for
 * Returns:
 *	Returns a fscache object.
 * Preconditions:
 *	precond(cachep)
 */

/*ARGSUSED*/
fscache_t *
fscache_create(
	cachefscache_t *cachep,
	char *cacheid,
	struct cachefs_mountargs *map,
	vnode_t *cacheidvp,
	vfs_t *vfsp)
{
	fscache_t *fscp;

	CFS_DEBUG(CFSDEBUG_FSCACHE,
		printf("fscache_create: ENTER cachep 0x%p, cacheid %s\n", cachep,
			cacheid));
	ASSERT(VALID_ADDR(cachep));
	ASSERT(VALID_ADDR(cacheid));
	ASSERT(VALID_ADDR(map));
	ASSERT(VALID_ADDR(cacheidvp));
	ASSERT(VALID_ADDR(vfsp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fscache_create, current_pid(),
		cachep, cacheid);
	/* create and initialize the fscache object */
	/*LINTED alignment okay*/
	fscp = (fscache_t *)CACHEFS_KMEM_ZALLOC(sizeof (fscache_t), KM_SLEEP);
	fscp->fs_vnoderef = 0;
	fscp->fs_cache = cachep;
	fscp->fs_cacheid = cacheid;
	fscp->fs_options = map->cfs_options & ~CFS_ADD_BACK;
	if (fscp->fs_options & CFS_NOCONST_MODE)
		fscp->fs_cfsops = &nopcfsops;
	else if (fscp->fs_options & CFS_DUAL_WRITE)
		fscp->fs_cfsops = &singlecfsops;
	else if (fscp->fs_options & CFS_WRITE_AROUND)
		fscp->fs_cfsops = &strictcfsops;
	else
		fscp->fs_cfsops = &nopcfsops;
	fscp->fs_acregmin = map->cfs_acregmin;
	fscp->fs_acregmax = map->cfs_acregmax;
	fscp->fs_acdirmin = map->cfs_acdirmin;
	fscp->fs_acdirmax = map->cfs_acdirmax;
	fscp->fs_hashvers = 0;
	fscp->fs_sync_running = 0;
	fscp->fs_flags = CFS_FS_READ | CFS_FS_WRITE;
	/*
	 * Transfer the back FS root vp to the fscache structure.  Set
	 * backrootvp to NULL to prevent its being released at the end of
	 * this function.
	 */
	VN_HOLD(cacheidvp);
	fscp->fs_cacheidvp = cacheidvp;

	spinlock_init(&fscp->fs_fslock, "fscache lock");
	mutex_init(&fscp->fs_cnodelock, MUTEX_DEFAULT, "cnode hash lock");
	mutex_init(&fscp->fs_fsidmaplock, MUTEX_DEFAULT, "fsid map lock");
	sv_init(&fscp->fs_sync_wait, SV_DEFAULT, "sync wait");
	sv_init(&fscp->fs_reconnect, SV_DEFAULT, "reconnect");

	CFS_DEBUG(CFSDEBUG_FSCACHE,
		printf("fscache_create: EXIT fscp 0x%p\n", fscp));
	return (fscp);
}

/*
 * ------------------------------------------------------------------
 *
 *		fscache_destroy
 *
 * Description:
 *	Destroys the fscache object.
 * Arguments:
 *	fscp	the fscache object to destroy
 * Returns:
 * Preconditions:
 *	precond(fscp)
 *	precond(fs_vnoderef == 0)
 */

void
fscache_destroy(fscache_t *fscp)
{
	int ospl;
	vnode_t *cacheidvp;
	int slot;
	struct fsidmap *fsid;

	CFS_DEBUG(CFSDEBUG_FSCACHE,
		printf("fscache_destroy: ENTER fscp 0x%p\n", fscp));
	ASSERT(VALID_ADDR(fscp));
	ASSERT(fscp->fs_vnoderef == 0);

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fscache_destroy, current_pid(),
		fscp, 0);
	/* mark the file system as not mounted */
	ospl = mutex_spinlock(&fscp->fs_fslock);
	fscp->fs_rootvp = NULL;
	cacheidvp = fscp->fs_cacheidvp;
	fscp->fs_cacheidvp = NULL;
	mutex_spinunlock(&fscp->fs_fslock, ospl);

	/*
	 * release the back FS root vnode
	 */
	VN_RELE(cacheidvp);

	spinlock_destroy( &fscp->fs_fslock );
	mutex_destroy( &fscp->fs_cnodelock );
	mutex_destroy( &fscp->fs_fsidmaplock );
	sv_destroy( &fscp->fs_sync_wait );
	sv_destroy( &fscp->fs_reconnect );
	if (fscp->fs_bhv.bd_vobj) {
		release_minor_number(minor(FSC_TO_VFS(fscp)->vfs_dev));
	}
	for (slot = 0; slot < FSID_MAP_SLOTS; slot++) {
		while (fsid = fscp->fs_fsidmap[slot]) {
			fscp->fs_fsidmap[slot] = fsid->fsid_next;
			release_minor_number(minor(fsid->fsid_cachefs));
			CACHEFS_ZONE_FREE(Cachefs_fsidmap_zone, fsid);
		}
	}
	CACHEFS_ZONE_FREE(Cachefs_path_zone, fscp->fs_cacheid);
	if (fscp->fs_bhv.bd_vobj) {
		VFS_REMOVEBHV(FSC_TO_VFS(fscp), &fscp->fs_bhv);
	}
	CACHEFS_KMEM_FREE(fscp, sizeof (struct fscache));
	CFS_DEBUG(CFSDEBUG_FSCACHE,
		printf("fscache_create: EXIT\n"));
}

/*
 * ------------------------------------------------------------------
 *
 *		fscache_hold
 *
 * Description:
 *	Increments the reference count on the fscache object
 * Arguments:
 *	fscp		fscache object to incriment reference count on
 * Returns:
 * Preconditions:
 *	precond(fscp)
 */

void
fscache_hold(fscache_t *fscp)
{
	int ospl;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fscache_hold, current_pid(),
		fscp, 0);
	ASSERT(VALID_ADDR(fscp));
	ospl = mutex_spinlock(&fscp->fs_fslock);
	fscp->fs_vnoderef++;
	ASSERT(fscp->fs_vnoderef > 0);
	mutex_spinunlock(&fscp->fs_fslock, ospl);
}

/*
 * ------------------------------------------------------------------
 *
 *		fscache_rele
 *
 * Description:
 *	Decriments the reference count on the fscache object
 * Arguments:
 *	fscp		fscache object to decriment reference count on
 * Returns:
 * Preconditions:
 *	precond(fscp)
 */

void
fscache_rele(fscache_t *fscp)
{
	int ospl;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fscache_rele, current_pid(),
		fscp, 0);
	ASSERT(VALID_ADDR(fscp));
	ospl = mutex_spinlock(&fscp->fs_fslock);
	ASSERT(fscp->fs_vnoderef > 0);
	fscp->fs_vnoderef--;
	mutex_spinunlock(&fscp->fs_fslock, ospl);
}

/*
 * ------------------------------------------------------------------
 *
 *		fscache_sync
 *
 * Description:
 *	Syncs any data for this fscache to the front file system.
 * Arguments:
 *	fscp	fscache to sync
 *	flags	SYNC_CLOSE means toss any cnodes that are not being used
 * Returns:
 * Preconditions:
 *	precond(fscp)
 */

void
fscache_sync(struct fscache *fscp, int flags)
{
	int vc;
	int vcount;
	struct cnode *cp;
	cnode_t *clist = NULL;
	int i;
	vnode_t *vp;
	vmap_t vmap;
	u_int hashvers;
	extern krwlock_t reclaim_lock;
	int ospl;

	CFS_DEBUG(CFSDEBUG_FSCACHE,
		printf("fscache_sync: ENTER fscp 0x%p flags 0x%x\n", fscp,
			flags));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fscache_sync, current_pid(),
		fscp, flags);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(!issplhi(getsr()));
	if (!(flags & SYNC_WAIT)) {
		if (!mutex_trylock(&fscp->fs_cnodelock)) {
			return;
		}
	} else {
		mutex_enter(&fscp->fs_cnodelock);
	}
	ASSERT(!issplhi(getsr()));
	/*
	 * Go through the cnodes and sync the dirty ones. If this is an unmount
	 * Throw away the inactive cnodes too.
	 */
	ASSERT(!issplhi(getsr()));
	for (vcount = 0, i = 0; i < CNODE_BUCKET_SIZE; i++) {
start_again:
		/*
		 * record the hash version in case we have to release fs_cnodelock
		 */
		hashvers = fscp->fs_hashvers;
		for (vc = 0, cp = fscp->fs_cnode[i]; cp != NULL; cp = cp->c_hash) {
			ASSERT(VALID_ADDR(cp));
			ASSERT(cp->c_inhash);
			ASSERT(!issplhi(getsr()));
			ospl = mutex_spinlock(&cp->c_statelock);
			vp = CTOV(cp);
			if (vp) {
				ASSERT(vp->v_vfsp->vfs_fstype == cachefsfstyp);
				/*
				 * Also toss all inactive cnodes is this is an unmount.
				 */
				if (vp->v_count) {
					vc++;
				}
				if ((vp->v_count == 0) && ((flags & SYNC_CLOSE) ||
					(cp->c_flags & CN_DESTROY))) {
						ASSERT(!(cp->c_flags & CN_ROOT));
						VMAP( vp, vmap );
						/*
						 * Once the cnode has been removed from the hash,
						 * we will be committed to purging it.  vn_purge,
						 * however, may fail.
						 */
						cp->c_flags |= CN_DESTROY;
						mutex_spinunlock(&cp->c_statelock, ospl);
						ASSERT(!issplhi(getsr()));
						mutex_exit(&fscp->fs_cnodelock);
						(void)cachefs_write_header(cp, 0);
						/*
						 * Since this is and unmount, there should be no
						 * possibility of a race between vn_get and vn_purge,
						 * so vn_purge can be assumed to have succeeded unless
						 * there was a race with vn_reclaim.  In any case, it
						 * can be assumed that the cnode has been freed.  There
						 * should be no danger of a memory leak.
						 * There is also a possible race with vn_get in
						 * makecachefsnode and dnlc_lookup.
						 */
						vn_purge( vp, &vmap );
						mutex_enter(&fscp->fs_cnodelock);
						if (fscp->fs_hashvers != hashvers) {
							ASSERT(!issplhi(getsr()));
							goto start_again;
						}
						ASSERT(!issplhi(getsr()));
				} else if (!(cp->c_flags & CN_DESTROY) &&
					!(vp->v_flag & VINACT) &&
					((cp->c_flags & (CN_WRITTEN | CN_UPDATED)) ||
					(flags & SYNC_CLOSE))) {
						VMAP( vp, vmap );
						cp->c_flags |= CN_SYNC;
						mutex_spinunlock(&cp->c_statelock, ospl);
						ASSERT(!issplhi(getsr()));
						/*
						 * Avoid deadlocks by releasing fs_cnodelock before
						 * calling vn_get.
						 */
						mutex_exit(&fscp->fs_cnodelock);
						/*
						 * get a reference to the vnode.
						 */
						if ( vp = vn_get( vp, &vmap, 0 ) ) {
							ospl = mutex_spinlock(&cp->c_statelock);
							cp->c_flags &= ~CN_SYNC;
							mutex_spinunlock(&cp->c_statelock, ospl);
							ASSERT( vp->v_vfsp->vfs_fstype == cachefsfstyp );
							CNODE_TRACE(CNTRACE_ACT, cp, (void *)fscache_sync,
								cp->c_flags, vp->v_count);
							/*
							 * It is important here that FSYNC_WAIT not
							 * be used unless this is an unmount.  If it is
							 * used, a deadlock will occur.
							 */
							(void) cachefs_fsync(CTOBHV(cp),
							(flags & (SYNC_CLOSE | SYNC_WAIT)) ? FSYNC_WAIT : 0,
							sys_cred, (off_t)0, (off_t)-1);
							(void)cachefs_write_header(cp, 0);
							VN_RELE(vp);
							CNODE_TRACE(CNTRACE_ACT, cp, (void *)fscache_sync,
								cp->c_flags, vp->v_count);
						} else {
							ospl = mutex_spinlock(&cp->c_statelock);
							cp->c_flags &= ~CN_SYNC;
							mutex_spinunlock(&cp->c_statelock, ospl);
						}
						mutex_enter(&fscp->fs_cnodelock);
						/*
						 * if the hash chain might have changed, start at the
						 * beginning of the chain
						 */
						if (fscp->fs_hashvers != hashvers) {
							ASSERT(!issplhi(getsr()));
							goto start_again;
						}
						ASSERT(!issplhi(getsr()));
				} else {
					mutex_spinunlock(&cp->c_statelock, ospl);
				}
				ASSERT(!cp->c_hash || VALID_ADDR(cp->c_hash));
			} else if (cp->c_flags & CN_UPDATED) {
				mutex_spinunlock(&cp->c_statelock, ospl);
				mutex_exit(&fscp->fs_cnodelock);
				(void)cachefs_write_header(cp, 0);
				mutex_enter(&fscp->fs_cnodelock);
				if (fscp->fs_hashvers != hashvers) {
					ASSERT(!issplhi(getsr()));
					goto start_again;
				}
			} else {
				mutex_spinunlock(&cp->c_statelock, ospl);
			}
		}
		vcount += vc;
		CFS_DEBUG(CFSDEBUG_FSCACHE,
			printf("fscache_sync: add %d to vcount (now %d)\n", vc, vcount));
	}
	clist = fscp->fs_reclaim;
	fscp->fs_reclaim = NULL;
	mutex_exit(&fscp->fs_cnodelock);
	while (cp = clist) {
		clist = cp->c_hash;
		cp->c_hash = NULL;
		(void)cachefs_write_header(cp, 0);
		cachefs_cnode_free(cp);
	}
	CFS_DEBUG(CFSDEBUG_FSCACHE,
		printf("fscache_sync: EXIT, vcount %d\n", vcount));
	if ((vcount <= 1) && (flags & SYNC_CLOSE)) {
		fscache_free_cnodes(fscp);
	}
}

/*
 * unconditionally free all cnodes for the given fscace
 */
void
fscache_free_cnodes(fscache_t *fscp)
{
	cnode_t *cp;
	int i;

	mutex_enter(&fscp->fs_cnodelock);
	for (i = 0; i < CNODE_BUCKET_SIZE; i++) {
		while (cp = fscp->fs_cnode[i]) {
			cachefs_remhash(cp);
			cp->c_flags |= CN_DESTROY;
			cp->c_refcnt = 0;
			if (!cp->c_vnode) {
				cachefs_cnode_free( cp );
			} else {
				ASSERT(cp->c_flags & CN_ROOT);
				ASSERT(cp = fscp->fs_root);
			}
		}
	}
	mutex_exit(&fscp->fs_cnodelock);
}

/*
 * ------------------------------------------------------------------
 *
 *		fscache_list_find
 *
 * Description:
 *	Finds the desired fscache structure on a cache's
 *	file system list.
 * Arguments:
 *	cachep	holds the list of fscache objects to search
 *	fsid	the numeric identifier of the fscache
 * Returns:
 *	Returns an fscache object on success or NULL on failure.
 * Preconditions:
 *	precond(cachep)
 *	precond(the fslistlock must be held)
 */

fscache_t *
fscache_list_find(cachefscache_t *cachep, char *cacheid)
{
	fscache_t *fscp = cachep->c_fslist;

	ASSERT(mutex_owner(&cachep->c_fslistlock) == curthreadp);

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fscache_list_find, current_pid(),
		cachep, cacheid);
	ASSERT(VALID_ADDR(cachep));
	ASSERT(VALID_ADDR(cacheid));
	while (fscp != NULL) {
		if (strcmp(fscp->fs_cacheid, cacheid) == 0) {
			ASSERT(fscp->fs_cache == cachep);
			break;
		}
		fscp = fscp->fs_next;
	}

	return (fscp);
}

/*
 * ------------------------------------------------------------------
 *
 *		fscache_list_add
 *
 * Description:
 *	Adds the specified fscache object to the list on
 *	the specified cachep.
 * Arguments:
 *	cachep	holds the list of fscache objects
 *	fscp	fscache object to add to list
 * Returns:
 * Preconditions:
 *	precond(cachep)
 *	precond(fscp)
 *	precond(fscp cannot already be on a list)
 *	precond(the fslistlock must be held)
 */

void
fscache_list_add(cachefscache_t *cachep, fscache_t *fscp)
{
	ASSERT(mutex_owner(&cachep->c_fslistlock) == curthreadp);

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fscache_list_add, current_pid(),
		cachep, fscp);
	ASSERT(VALID_ADDR(cachep));
	ASSERT(VALID_ADDR(fscp));
	fscp->fs_next = cachep->c_fslist;
	cachep->c_fslist = fscp;
	cachep->c_refcnt++;
}

/*
 * ------------------------------------------------------------------
 *
 *		fscache_list_remove
 *
 * Description:
 *	Removes the specified fscache object from the list
 *	on the specified cachep.
 * Arguments:
 *	cachep	holds the list of fscache objects
 *	fscp	fscache object to remove from list
 * Returns:
 * Preconditions:
 *	precond(cachep)
 *	precond(fscp)
 *	precond(the fslistlock must be held)
 */

void
fscache_list_remove(cachefscache_t *cachep, fscache_t *fscp)
{
	struct fscache **pfscp = &cachep->c_fslist;

	ASSERT(mutex_owner(&cachep->c_fslistlock) == curthreadp);

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)fscache_list_remove,
		current_pid(), cachep, fscp);
	ASSERT(VALID_ADDR(cachep));
	ASSERT(VALID_ADDR(fscp));
	while (*pfscp != NULL) {
		if (fscp == *pfscp) {
			*pfscp = fscp->fs_next;
			cachep->c_refcnt--;
			break;
		}
		pfscp = &(*pfscp)->fs_next;
	}
}
