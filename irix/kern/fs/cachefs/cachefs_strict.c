/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_strict.c 1.23     94/01/05 SMI"
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
#include <netinet/in.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include "cachefs_fs.h"
#include <sys/buf.h>
#include <sys/pfdat.h>
#include <sys/kthread.h>
#include <ksys/vproc.h>

#define	C_CACHE_VALID(TOKEN_MTIME, NEW_MTIME)   \
	((TOKEN_MTIME.tv_sec == NEW_MTIME.tv_sec) && \
		(TOKEN_MTIME.tv_nsec == NEW_MTIME.tv_nsec))

struct c_strict_token {
	timespec_t		str_mod_time;
};

extern time_t time;

/* ARGSUSED */
static int
c_strict_init_cached_object(struct fscache *fscp, struct cnode *cp,
	vattr_t *vap, cred_t *cr)
{
	enum backop_stat status;
	int error = 0;
	struct c_strict_token *tokenp = (struct c_strict_token *)cp->c_token;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_init_cached_object: ENTER fscp 0x%p, cp 0x%p\n",
			fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_strict_init_cached_object,
		current_pid(), fscp, cp);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(!vap || VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	ASSERT(sizeof(*cp->c_token) >= sizeof(struct c_strict_token));
	if (!C_CACHING(cp)) {
		return(0);
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	ASSERT(VALID_ADDR(tokenp));
	CACHEFS_STATS->cs_objinits++;
	if (!vap) {
		vap = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
		vap->va_mask = AT_ALL;
		status = BACK_VOP_GETATTR(cp, vap, 0, cr,
			C_ISFS_DISCONNECT(C_TO_FSCACHE(cp)) ? BACKOP_NONBLOCK :
			BACKOP_BLOCK, &error);
		switch (status) {
			case BACKOP_SUCCESS:
				if (!C_CACHE_VALID(tokenp->str_mod_time, vap->va_mtime) ||
					!C_CACHE_VALID(cp->c_attr->ca_ctime, vap->va_ctime) ||
					(cp->c_size != vap->va_size)) {
						CACHEFS_STATS->cs_objexpires++;
						error = cachefs_inval_object(cp, UNLOCKED);
						if (error)
							goto out;
				}
				VATTR_TO_CATTR(cp->c_vnode, vap, cp->c_attr, 
					       AT_SIZE);
				CNODE_TRACE(CNTRACE_ATTR, cp,
					(void *)c_strict_init_cached_object, 0, 0);
				CFS_DEBUG(CFSDEBUG_ATTR,
					printf("c_strict_init_cached_object(line %d): attr update, "
						"cp 0x%p\n", __LINE__, cp));
			case BACKOP_NETERR:
				tokenp->str_mod_time = cp->c_attr->ca_mtime;
				cp->c_flags |= CN_UPDATED;
				CNODE_TRACE(CNTRACE_UPDATE, cp,
					(void *)c_strict_init_cached_object, cp->c_flags,
					cp->c_fileheader->fh_metadata.md_allocents);
				if (!cp->c_frontvp) {
					error = cachefs_getfrontvp(cp);
					CFS_DEBUG(CFSDEBUG_ERROR, if (error)
						printf("c_strict_init_cached_object(line %d): error %d "
							"getting front vnode\n", __LINE__, error));
					if (error) {
						(void)cachefs_nocache(cp);
						error = 0;
					}
				}
				cp->c_size = cp->c_attr->ca_size;
				CNODE_TRACE(CNTRACE_SIZE, cp,
					(void *)c_strict_init_cached_object, cp->c_size, 0);
				error = 0;
			default:
				break;
		}
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, vap);
	} else {
		ASSERT(CTOV(cp)->v_type != VNON);
		if (!C_CACHE_VALID(tokenp->str_mod_time, vap->va_mtime) ||
			!C_CACHE_VALID(cp->c_attr->ca_ctime, vap->va_ctime) ||
			(cp->c_size != vap->va_size)) {
				CACHEFS_STATS->cs_objexpires++;
				error = cachefs_inval_object(cp, UNLOCKED);
				if (error)
					goto out;
		}
		VATTR_TO_CATTR(cp->c_vnode, vap, cp->c_attr, AT_SIZE);
		CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_strict_init_cached_object,
			0, 0);
		CFS_DEBUG(CFSDEBUG_ATTR,
			printf("c_strict_init_cached_object(line %d): attr update, cp "
				"0x%p\n", __LINE__, cp));
		tokenp->str_mod_time = cp->c_attr->ca_mtime;
		cp->c_flags |= CN_UPDATED;
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_strict_init_cached_object,
			cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
		if (!cp->c_frontvp) {
			error = cachefs_getfrontvp(cp);
			CFS_DEBUG(CFSDEBUG_ERROR, if (error)
				printf("c_strict_init_cached_object(line %d): error %d "
					"getting front vnode\n", __LINE__, error));
			if (error) {
				(void)cachefs_nocache(cp);
				error = 0;
			}
		}
		cp->c_size = cp->c_attr->ca_size;
		CNODE_TRACE(CNTRACE_SIZE, cp, (void *)c_strict_init_cached_object,
			cp->c_size, 0);
	}
	ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
		FILEHEADER_BLOCK_SIZE(fscp->fs_cache)));
out:
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_init_cached_object: EXIT error %d\n", error));
	return (error);
}

/* ARGSUSED */
static int
c_strict_check_cached_object(struct fscache *fscp, struct cnode *cp,
	vattr_t *vap, cred_t *cr, lockmode_t lm)
{
	enum backop_stat status;
	struct c_strict_token *tokenp = (struct c_strict_token *)cp->c_token;
	vattr_t *attrs = NULL;
	int error = 0;
	int ospl;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_check_cached_object: ENTER fscp 0x%p, cp 0x%p\n",
			fscp, cp));

	ASSERT(cr != NULL);
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_strict_check_cached_object,
		current_pid(), fscp, cp);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(!vap || VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	CACHEFS_STATS->cs_objchecks++;
	if (cp->c_flags & CN_NEEDINVAL) {
		error = cachefs_inval_object(cp, lm);
		if (error) {
			return(error);
		}
	}
	if (!vap) {
		CACHEFS_STATS->cs_backchecks++;
		vap = attrs = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone,
			KM_SLEEP);
		vap->va_mask = AT_ALL;
		status = BACK_VOP_GETATTR(cp, vap, 0, cr,
			(cp->c_flags & CN_NOCACHE) ? BACKOP_BLOCK : BACKOP_NONBLOCK,
			&error);
		switch (status) {
			case BACKOP_NETERR:
				error = 0;          /* pretend we never did this */
			case BACKOP_FAILURE:
				goto out;
			case BACKOP_SUCCESS:
				break;
		}
	}
	/*
	 * if this is an uncached file, we still need to update the size
	 */
	if (!C_CACHING(cp)) {
		ospl = mutex_spinlock(&cp->c_statelock);
		if (!C_DIRTY(cp)) {
			CFS_DEBUG(CFSDEBUG_SIZE,
				if (cp->c_size != vap->va_size)
					printf("c_strict_check_cached_object(line %d): cp 0x%p "
						"size change from %lld to %lld\n", __LINE__, cp,
						cp->c_size, vap->va_size));
			cp->c_size = vap->va_size;
			CNODE_TRACE(CNTRACE_SIZE, cp, (void *)c_strict_check_cached_object,
				cp->c_size, 0);
		}
		mutex_spinunlock(&cp->c_statelock, ospl);
		return(error);
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	ASSERT(VALID_ADDR(tokenp));
	ospl = mutex_spinlock(&cp->c_statelock);
	if (!C_CACHE_VALID(tokenp->str_mod_time, vap->va_mtime) ||
		!C_CACHE_VALID(cp->c_attr->ca_ctime, vap->va_ctime) ||
		(cp->c_flags & CN_MODIFIED) || (cp->c_size != vap->va_size)) {
			mutex_spinunlock(&cp->c_statelock, ospl);
			CACHEFS_STATS->cs_objexpires++;
			error = cachefs_inval_object(cp, lm);
			if (error)
				goto out;
			ospl = mutex_spinlock(&cp->c_statelock);
			if ((CTOV(cp))->v_type == VREG) {
				CACHEFS_STATS->cs_backchecks++;
				mutex_spinunlock(&cp->c_statelock, ospl);
				vap->va_mask = AT_ALL;
				status = BACK_VOP_GETATTR(cp, vap, 0, cr,
					(cp->c_flags & CN_NOCACHE) ? BACKOP_BLOCK : BACKOP_NONBLOCK,
					&error);
				switch (status) {
					case BACKOP_NETERR:
						error = 0;          /* pretend we never did this */
					case BACKOP_FAILURE:
						goto out;
					case BACKOP_SUCCESS:
						break;
				}
				ospl = mutex_spinlock(&cp->c_statelock);
			}
			cp->c_flags &= ~CN_MODIFIED;
	}
	VATTR_TO_CATTR(cp->c_vnode, vap, cp->c_attr, 
		       !C_DIRTY(cp) ? AT_SIZE : 0);
	CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_strict_check_cached_object, 0, 0);
	CFS_DEBUG(CFSDEBUG_ATTR,
		printf("c_strict_check_cached_object(line %d): attr update, cp "
			"0x%p\n", __LINE__, cp));
	CFS_DEBUG(CFSDEBUG_SIZE,
		if (cp->c_size != cp->c_attr->ca_size)
			printf("c_strict_check_cached_object(line %d): cp 0x%p "
				"size change from %lld to %lld\n", __LINE__, cp,
				cp->c_size, vap->va_size));
#ifdef DEBUG
	if (cp->c_size != cp->c_attr->ca_size) {
		CNODE_TRACE(CNTRACE_SIZE, cp, (void *)c_strict_check_cached_object,
			cp->c_attr->ca_size, 0);
	}
#endif /* DEBUG */
	cp->c_size = cp->c_attr->ca_size;
	tokenp->str_mod_time = vap->va_mtime;
	cp->c_flags |= CN_UPDATED;
	CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_strict_check_cached_object,
		cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
	mutex_spinunlock(&cp->c_statelock, ospl);
	if (!cp->c_frontvp) {
		error = cachefs_getfrontvp(cp);
		CFS_DEBUG(CFSDEBUG_ERROR, if (error)
			printf("c_strict_check_cached_object(line %d): error %d "
				"getting front vnode\n", __LINE__, error));
		if (error) {
			(void)cachefs_nocache(cp);
			error = 0;
		}
	}
	ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
		FILEHEADER_BLOCK_SIZE(fscp->fs_cache)));

out:
	if (attrs)
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrs);

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_check_cached_object: EXIT error %d\n", error));

	return (error);
}

/* ARGSUSED */
static void
c_strict_modify_cached_object(struct fscache *fscp, struct cnode *cp,
	cred_t *cr)
{
	vattr_t *attrs;
	struct c_strict_token *tokenp = (struct c_strict_token *)cp->c_token;
	int error = 0;
	int ospl;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_modify_cached_object: ENTER fscp 0x%p, cp 0x%p\n",
			fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_strict_modify_cached_object,
		current_pid(), fscp, cp);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(cr));
	if (!C_CACHING(cp)) {
		return;
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	ASSERT(VALID_ADDR(tokenp));
	CACHEFS_STATS->cs_objmods++;
	if (CTOV(cp)->v_type == VDIR) {
		ospl = mutex_spinlock(&cp->c_statelock);
		tokenp->str_mod_time.tv_sec = 0;
		cp->c_flags |= CN_MODIFIED;
		mutex_spinunlock(&cp->c_statelock, ospl);
	} else {
		attrs = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
		attrs->va_mask = AT_ALL;
		(void)BACK_VOP_GETATTR(cp, attrs, 0, cr, BACKOP_BLOCK, &error);
		ospl = mutex_spinlock(&cp->c_statelock);
		if (error) {
			tokenp->str_mod_time.tv_sec = 0;
		} else {
			tokenp->str_mod_time = attrs->va_mtime;
			VATTR_TO_CATTR(cp->c_vnode, attrs, cp->c_attr, 0);
			CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_strict_modify_cached_object,
				0, 0);
			CFS_DEBUG(CFSDEBUG_ATTR,
				printf("c_strict_modify_cached_object(line %d): attr update, "
					"cp 0x%p\n", __LINE__, cp));
		}
		cp->c_flags |= (CN_UPDATED | CN_MODIFIED);
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_strict_modify_cached_object,
			cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
		mutex_spinunlock(&cp->c_statelock, ospl);
		if (!cp->c_frontvp) {
			error = cachefs_getfrontvp(cp);
			CFS_DEBUG(CFSDEBUG_ERROR, if (error)
				printf("c_strict_modify_cached_object(line %d): error %d "
					"getting front vnode\n", __LINE__, error));
			if (error) {
				(void)cachefs_nocache(cp);
				error = 0;
			}
		}
		ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
			FILEHEADER_BLOCK_SIZE(fscp->fs_cache)));
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrs);
	}
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_modify_cached_object: EXIT\n"));
}

/*ARGSUSED*/
static void
c_strict_invalidate_cached_object(struct fscache *fscp, struct cnode *cp,
	cred_t *cr)
{
	int error;
	int ospl;
	struct c_strict_token *tokenp = (struct c_strict_token *)cp->c_token;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_invalidate_cached_object: ENTER fscp 0x%p, cp "
			"0x%p\n", fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_strict_invalidate_cached_object,
		current_pid(), fscp, cp);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(cr));
	if (!C_CACHING(cp)) {
		return;
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	ASSERT(VALID_ADDR(tokenp));
	CACHEFS_STATS->cs_objinvals++;
	ospl = mutex_spinlock(&cp->c_statelock);
	tokenp->str_mod_time.tv_sec = 0;
	cp->c_flags |= CN_UPDATED;
	CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_strict_invalidate_cached_object,
		cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
	mutex_spinunlock(&cp->c_statelock, ospl);
	if (!cp->c_frontvp) {
		error = cachefs_getfrontvp(cp);
		CFS_DEBUG(CFSDEBUG_ERROR, if (error)
			printf("c_strict_invalidate_cached_object(line %d): error %d "
				"getting front vnode\n", __LINE__, error));
		if (error) {
			(void)cachefs_nocache(cp);
			error = 0;
		}
	}
	ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
		FILEHEADER_BLOCK_SIZE(fscp->fs_cache)));
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_invalidate_cached_object: EXIT\n"));
}

/*ARGSUSED*/
static void
c_strict_expire_cached_object(struct fscache *fscp, struct cnode *cp,
	cred_t *cr)
{
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_expire_cached_object: ENTER fscp 0x%p, cp "
			"0x%p\n", fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_strict_expire_cached_object,
		current_pid(), fscp, cp);
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_strict_expire_cached_object: EXIT\n"));
}

struct cachefsops strictcfsops = {
	c_strict_init_cached_object,
	c_strict_check_cached_object,
	c_strict_modify_cached_object,
	c_strict_invalidate_cached_object,
	c_strict_expire_cached_object
};
