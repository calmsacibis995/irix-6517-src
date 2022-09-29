/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_singlewrc.c 1.21     94/01/05 SMI"
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

#define	C_SINGLE_WRITER	0x1
#define	C_CACHE_VALID(TOKEN_MTIME, NEW_MTIME)	\
	((TOKEN_MTIME.tv_sec == NEW_MTIME.tv_sec) && \
		(TOKEN_MTIME.tv_nsec == NEW_MTIME.tv_nsec))

struct c_single_wr_token {
	timespec_t		swr_expire_time;
	timespec_t		swr_mod_time;
};

extern time_t time;

static int
c_single_init_cached_object(struct fscache *fscp, struct cnode *cp,
	vattr_t *vap, cred_t *cr)
{
	enum backop_stat status;
	int error = 0;
	struct c_single_wr_token *tokenp = (struct c_single_wr_token *)cp->c_token;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_init_cached_object: ENTER fscp 0x%p, cp 0x%p\n",
			fscp, cp));
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(!vap || VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	ASSERT(sizeof(*cp->c_token) >= sizeof(struct c_single_wr_token));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_single_init_cached_object,
		current_pid(), fscp, cp);
	if (!C_CACHING(cp)) {
		return(0);
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	ASSERT(VALID_ADDR(tokenp));
	CACHEFS_STATS->cs_objinits++;
	if (!vap && ((!tokenp->swr_expire_time.tv_sec ||
		!tokenp->swr_mod_time.tv_sec) ||
		(time >= tokenp->swr_expire_time.tv_sec))) {
			vap = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
			vap->va_mask = AT_ALL;
			status = BACK_VOP_GETATTR(cp, vap, 0, cr,
				C_ISFS_DISCONNECT(C_TO_FSCACHE(cp)) ? BACKOP_NONBLOCK :
				BACKOP_BLOCK, &error);
			CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_single_init_cached_object,
				0, 0);
			CFS_DEBUG(CFSDEBUG_ATTR,
				printf("c_single_init_cached_object(line %d): attr update, cp "
					"0x%p, %d links\n", __LINE__, cp, cp->c_attr->ca_nlink));
			switch (status) {
				case BACKOP_SUCCESS:
					if (!C_CACHE_VALID(tokenp->swr_mod_time, vap->va_mtime) ||
						!C_CACHE_VALID(cp->c_attr->ca_ctime, vap->va_ctime)) {
						CACHEFS_STATS->cs_objexpires++;
						error = cachefs_inval_object(cp, UNLOCKED);
						if (error)
							goto out;
					}
					VATTR_TO_CATTR(cp->c_vnode, vap, 
						       cp->c_attr, AT_SIZE);
				case BACKOP_NETERR:
					error = 0;
					tokenp->swr_mod_time = cp->c_attr->ca_mtime;
					tokenp->swr_expire_time.tv_nsec = 0;
					tokenp->swr_expire_time.tv_sec =
						cachefs_gettime_cached_object(fscp, CTOV(cp)->v_type,
						cp->c_attr->ca_mtime.tv_sec);
					cp->c_size = cp->c_attr->ca_size;
					cp->c_flags |= CN_UPDATED;
					CNODE_TRACE(CNTRACE_UPDATE, cp,
						(void *)c_single_init_cached_object, cp->c_flags,
						cp->c_fileheader->fh_metadata.md_allocents);
					if (!cp->c_frontvp) {
						error = cachefs_getfrontvp(cp);
						if (error) {
							CFS_DEBUG(CFSDEBUG_ERROR,
								printf("c_single_init_cached_object(line %d): "
									"error %d getting front vnode\n", __LINE__,
									error));
							(void)cachefs_nocache(cp);
							error = 0;
						}
					}
					break;
				default:
					break;;
			}
			CACHEFS_ZONE_FREE(Cachefs_attr_zone, vap);
	} else if (vap) {
		ASSERT(CTOV(cp)->v_type != VNON);
		/*
		 * The caller has supplied new attributes.
		 * Validate the cache, and copy the attributes to c_attr
		 * (which points to the file header attributes).  Also initialize
		 * the token.  If the cached attributes have been supplied, the
		 * token can be assumed to be initialized.
		 */
		if (!C_CACHE_VALID(tokenp->swr_mod_time, vap->va_mtime) ||
			!C_CACHE_VALID(cp->c_attr->ca_ctime, vap->va_ctime)) {
			CACHEFS_STATS->cs_objexpires++;
			error = cachefs_inval_object(cp, UNLOCKED);
			if (error)
				goto out;
		}
		VATTR_TO_CATTR(cp->c_vnode, vap, cp->c_attr, AT_SIZE);
		CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_single_init_cached_object,
			0, 0);
		tokenp->swr_mod_time = vap->va_mtime;
		tokenp->swr_expire_time.tv_nsec = 0;
		tokenp->swr_expire_time.tv_sec = cachefs_gettime_cached_object(fscp,
			CTOV(cp)->v_type, vap->va_mtime.tv_sec);
		cp->c_size = cp->c_attr->ca_size;
		cp->c_flags |= CN_UPDATED;
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_single_init_cached_object,
			cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
		CFS_DEBUG(CFSDEBUG_ATTR,
			printf("c_single_init_cached_object(line %d): attr update, cp "
				"0x%p, %d links\n", __LINE__, cp, cp->c_attr->ca_nlink));
		if (!cp->c_frontvp) {
			error = cachefs_getfrontvp(cp);
			if (error) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("c_single_init_cached_object(line %d): error %d "
						"getting front vnode\n", __LINE__, error));
				(void)cachefs_nocache(cp);
				error = 0;
			}
		}
	} else {
		ASSERT(cp->c_attr);
		cp->c_size = cp->c_attr->ca_size;
	}
	CNODE_TRACE(CNTRACE_SIZE, cp, (void *)c_single_init_cached_object,
		cp->c_size, 0);
	ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
		FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
out:
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_init_cached_object: EXIT error %d\n", error));
	return (error);
}

static int
c_single_check_cached_object(struct fscache *fscp, struct cnode *cp,
	vattr_t *vap, cred_t *cr, lockmode_t lm)
{
	enum backop_stat status;
	struct c_single_wr_token *tokenp = (struct c_single_wr_token *)cp->c_token;
	vattr_t *attrs = NULL;
	int error = 0;
	time_t mytime = time;
	int ospl;

	ASSERT(cr != NULL);
	ASSERT(cp);
	ASSERT(cp->c_vnode);
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_check_cached_object: ENTER fscp 0x%p, cp 0x%p\n",
			fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_single_check_cached_object,
		current_pid(), fscp, cp);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(!vap || VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	if (cp->c_flags & CN_NEEDINVAL) {
		error = cachefs_inval_object(cp, lm);
		if (error) {
			return(error);
		}
	}
	if (!C_CACHING(cp)) {
		if (!vap) {
			attrs = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
			vap = attrs;
			CACHEFS_STATS->cs_backchecks++;
			vap->va_mask = AT_ALL;
			status = BACK_VOP_GETATTR(cp, vap, 0, cr, BACKOP_BLOCK, &error);
			switch (status) {
				case BACKOP_FAILURE:
					goto out;
				case BACKOP_SUCCESS:
					break;
			}
		}
		ospl = mutex_spinlock(&cp->c_statelock);
		CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_single_check_cached_object,
			0, 0);
		CFS_DEBUG(CFSDEBUG_ATTR,
			printf("c_single_check_cached_object(line %d): attr update, cp "
				"0x%p, %d links\n", __LINE__, cp, cp->c_attr->ca_nlink));
		if (!C_DIRTY(cp)) {
			CFS_DEBUG(CFSDEBUG_SIZE,
				if (cp->c_size != vap->va_size)
					printf("c_single_check_cached_object(line %d): cp 0x%p "
						"size change from %lld to %lld\n", __LINE__, cp,
						cp->c_size, vap->va_size));
			cp->c_size = vap->va_size;
			CNODE_TRACE(CNTRACE_SIZE, cp,
				(void *)c_single_check_cached_object, cp->c_size, 0);
		}
		mutex_spinunlock(&cp->c_statelock, ospl);
		ASSERT(CTOV(cp)->v_type != VNON);
		goto out;
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	ASSERT(VALID_ADDR(tokenp));
	CACHEFS_STATS->cs_objchecks++;
	ospl = mutex_spinlock(&cp->c_statelock);
	if (((mytime >= tokenp->swr_expire_time.tv_sec) ||
	    (cp->c_flags & CN_MODIFIED))) {
		mutex_spinunlock(&cp->c_statelock, ospl);
		CFS_DEBUG(CFSDEBUG_CFSOPS,
			printf("c_single_check_cached_object: %s: time %d exp time %d\n",
				cp->c_flags & CN_MODIFIED ? "object modified" : "token expired",
				mytime, tokenp->swr_expire_time.tv_sec));
		if (!vap) {
			CACHEFS_STATS->cs_backchecks++;
			attrs = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
			vap = attrs;
			/*
			 * Token expired or client wrote to file and needs to verify
			 * attrs.  We get the attributes from the back file, placing
			 * them into some temporary storage.  We do not get them
			 * directly into the cnode because we cannot hold c_statelock
			 * across VOP_GETATTR.
			 */
			vap->va_mask = AT_ALL;
			status = BACK_VOP_GETATTR(cp, vap, 0, cr,
				(cp->c_flags & CN_NOCACHE) ? BACKOP_BLOCK : BACKOP_NONBLOCK,
				&error);
			switch (status) {
				case BACKOP_NETERR:
					error = 0;			/* pretend we never did this */
					ospl = mutex_spinlock(&cp->c_statelock);
					/*
					 * reset the token so we don't check again immediately.
					 */
					tokenp->swr_expire_time.tv_sec =
					    cachefs_gettime_cached_object(fscp, CTOV(cp)->v_type,
						cp->c_fileheader->fh_metadata.md_attributes.ca_mtime.tv_sec);
					cp->c_flags |= CN_UPDATED;
					CNODE_TRACE(CNTRACE_UPDATE, cp,
						(void *)c_single_check_cached_object, cp->c_flags,
						cp->c_fileheader->fh_metadata.md_allocents);
					mutex_spinunlock(&cp->c_statelock, ospl);
					if (!cp->c_frontvp) {
						error = cachefs_getfrontvp(cp);
						if (error) {
							CFS_DEBUG(CFSDEBUG_ERROR,
								printf("c_single_check_cached_object(line %d): "
									"error %d getting front vnode\n", __LINE__,
									error));
							(void)cachefs_nocache(cp);
							error = 0;
						}
					}
					CFS_DEBUG(CFSDEBUG_NETERR,
						printf("c_single_check_cached_object(line %d): network "
							"error, token expire time reset to %d\n",
							__LINE__, tokenp->swr_expire_time.tv_sec));
					ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
						FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
				case BACKOP_FAILURE:
					goto out;
				case BACKOP_SUCCESS:
					break;
			}
		}
		ospl = mutex_spinlock(&cp->c_statelock);
		if (cp->c_flags & CN_MODIFIED) {
			cp->c_flags &= ~CN_MODIFIED;
			tokenp->swr_mod_time = vap->va_mtime;
			/*
			 * update the fsid if necessary
			 */
			if (cp->c_attr->ca_fsid != vap->va_fsid) {
				cp->c_attr->ca_fsid = vap->va_fsid;
			}
		} else if (!C_CACHE_VALID(tokenp->swr_mod_time, vap->va_mtime) ||
			!C_CACHE_VALID(cp->c_attr->ca_ctime, vap->va_ctime)) {
			mutex_spinunlock(&cp->c_statelock, ospl);
			CACHEFS_STATS->cs_objexpires++;
			error = cachefs_inval_object(cp, lm);
			if (error)
				goto out;
			if ((CTOV(cp))->v_type == VREG) {
				CACHEFS_STATS->cs_backchecks++;
				vap->va_mask = AT_ALL;
				status = BACK_VOP_GETATTR(cp, vap, 0, cr,
					(cp->c_flags & CN_NOCACHE) ? BACKOP_BLOCK : BACKOP_NONBLOCK,
					&error);
				switch (status) {
					case BACKOP_NETERR:
						error = 0;			/* pretend we never did this */
						ospl = mutex_spinlock(&cp->c_statelock);
						/*
						 * reset the token so we don't check again immediately.
						 */
						tokenp->swr_expire_time.tv_sec =
						    cachefs_gettime_cached_object(fscp,
							CTOV(cp)->v_type,
							cp->c_fileheader->fh_metadata.md_attributes.ca_mtime.tv_sec);
						cp->c_flags |= CN_UPDATED;
						CNODE_TRACE(CNTRACE_UPDATE, cp,
							(void *)c_single_check_cached_object, cp->c_flags,
							cp->c_fileheader->fh_metadata.md_allocents);
						mutex_spinunlock(&cp->c_statelock, ospl);
						if (!cp->c_frontvp) {
							error = cachefs_getfrontvp(cp);
							if (error) {
								CFS_DEBUG(CFSDEBUG_ERROR,
									printf("c_single_check_cached_object"
										"(line %d): error %d getting front "
										"vnode\n", __LINE__, error));
								(void)cachefs_nocache(cp);
								error = 0;
							}
						}
						CFS_DEBUG(CFSDEBUG_NETERR,
							printf("c_single_check_cached_object(line %d): "
								"network error, token expire time reset to "
								"%d\n", __LINE__,
								tokenp->swr_expire_time.tv_sec));
						ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
							FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
					case BACKOP_FAILURE:
						goto out;
					case BACKOP_SUCCESS:
						break;
				}
			}
			ospl = mutex_spinlock(&cp->c_statelock);
			tokenp->swr_mod_time = vap->va_mtime;
			VATTR_TO_CATTR(cp->c_vnode, vap, cp->c_attr, 
				       !C_DIRTY(cp) ? AT_SIZE : 0);
			CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_single_check_cached_object,
				0, 0);
			CFS_DEBUG(CFSDEBUG_ATTR,
				printf("c_single_check_cached_object(line %d): attr update, cp "
					"0x%p, %d links\n", __LINE__, cp, cp->c_attr->ca_nlink));
			cp->c_size = cp->c_attr->ca_size;
			ASSERT(CTOV(cp)->v_type != VNON);
		} else if (cp->c_attr->ca_fsid != vap->va_fsid) {
			cp->c_attr->ca_fsid = vap->va_fsid;
		}
		tokenp->swr_expire_time.tv_sec =
		    cachefs_gettime_cached_object(fscp, CTOV(cp)->v_type,
				cp->c_attr->ca_mtime.tv_sec);
		cp->c_flags |= CN_UPDATED;
		CNODE_TRACE(CNTRACE_UPDATE, cp,
			(void *)c_single_check_cached_object, cp->c_flags,
			cp->c_fileheader->fh_metadata.md_allocents);
		mutex_spinunlock(&cp->c_statelock, ospl);
		if (!cp->c_frontvp) {
			error = cachefs_getfrontvp(cp);
			if (error) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("c_single_check_cached_object(line %d): error %d "
						"getting front vnode\n", __LINE__, error));
				(void)cachefs_nocache(cp);
				error = 0;
			}
		}
		ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
			FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
out:
		if (attrs) {
			CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrs);
		}
	} else {
		mutex_spinunlock(&cp->c_statelock, ospl);
	}
	ASSERT(!attrs);
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_check_cached_object: EXIT error %d\n", error));
	return (error);
}

/*ARGSUSED*/
static void
c_single_modify_cached_object(struct fscache *fscp, struct cnode *cp,
	cred_t *cr)
{
	vattr_t *attrs;
	int error = 0;
	struct c_single_wr_token *tokenp = (struct c_single_wr_token *)cp->c_token;
	int ospl;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_modify_cached_object: ENTER fscp 0x%p, cp 0x%p\n",
			fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_single_modify_cached_object,
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
	attrs = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
	ospl = mutex_spinlock(&cp->c_statelock);
	cp->c_flags |= CN_MODIFIED;
	mutex_spinunlock(&cp->c_statelock, ospl);
	attrs->va_mask = AT_ALL;
	(void)BACK_VOP_GETATTR(cp, attrs, 0, cr, BACKOP_BLOCK, &error);
	ospl = mutex_spinlock(&cp->c_statelock);
	if (error) {
		cp->c_flags &= ~CN_MODIFIED;
		tokenp->swr_expire_time.tv_sec = 0;
		tokenp->swr_mod_time.tv_sec = 0;
	} else {
		VATTR_TO_CATTR(cp->c_vnode, attrs, cp->c_attr, 0);
		CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_single_modify_cached_object,
			0, 0);
		CFS_DEBUG(CFSDEBUG_ATTR,
			printf("c_single_modify_cached_object(line %d): attr update, cp "
				"0x%p, %d links\n", __LINE__, cp, cp->c_attr->ca_nlink));
	}
	cp->c_flags |= CN_UPDATED;
	CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_single_modify_cached_object,
		cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
	mutex_spinunlock(&cp->c_statelock, ospl);
	if (!cp->c_frontvp) {
		error = cachefs_getfrontvp(cp);
		if (error) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("c_single_modify_cached_object(line %d): error %d "
					"getting front vnode\n", __LINE__, error));
			(void)cachefs_nocache(cp);
			error = 0;
		}
	}
	ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
		FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
	CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrs);
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_modify_cached_object: EXIT\n"));
}

/*ARGSUSED*/
static void
c_single_invalidate_cached_object(struct fscache *fscp, struct cnode *cp,
	cred_t *cr)
{
	struct c_single_wr_token *tokenp = (struct c_single_wr_token *)cp->c_token;
	int ospl;
	int error;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_invalidate_cached_object: ENTER fscp 0x%p, cp "
			"0x%p\n", fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_single_invalidate_cached_object,
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
	tokenp->swr_mod_time.tv_sec = 0;
	tokenp->swr_expire_time.tv_sec = 0;
	cp->c_flags |= CN_UPDATED;
	CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_single_invalidate_cached_object,
		cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
	mutex_spinunlock(&cp->c_statelock, ospl);
	if (!cp->c_frontvp) {
		error = cachefs_getfrontvp(cp);
		if (error) {
			CFS_DEBUG(CFSDEBUG_ERROR,
				printf("c_single_invalidate_cached_object(line %d): error %d "
					"getting front vnode\n", __LINE__, error));
			(void)cachefs_nocache(cp);
			error = 0;
		}
	}
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_invalidate_cached_object: EXIT\n"));
}

/*ARGSUSED*/
static void
c_single_expire_cached_object(struct fscache *fscp, struct cnode *cp,
	cred_t *cr)
{
	struct c_single_wr_token *tokenp = (struct c_single_wr_token *)cp->c_token;
	int ospl;
	int error;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_expire_cached_object: ENTER fscp 0x%p, cp "
			"0x%p\n", fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_single_expire_cached_object,
		current_pid(), fscp, cp);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(cr));
	if (!C_CACHING(cp)) {
		return;
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	ASSERT(VALID_ADDR(tokenp));
	ospl = mutex_spinlock(&cp->c_statelock);
	tokenp->swr_expire_time.tv_sec = 0;
	cp->c_flags |= CN_UPDATED;
	CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_single_expire_cached_object,
		cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
	mutex_spinunlock(&cp->c_statelock, ospl);
	if (!cp->c_frontvp) {
		error = cachefs_getfrontvp(cp);
		if (error) {
			CFS_DEBUG(CFSDEBUG_ERROR, if (error)
				printf("c_single_expire_cached_object(line %d): error %d "
					"getting front vnode\n", __LINE__, error));
			(void)cachefs_nocache(cp);
			error = 0;
		}
	}
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_single_expire_cached_object: EXIT\n"));
}

struct cachefsops singlecfsops = {
	c_single_init_cached_object,
	c_single_check_cached_object,
	c_single_modify_cached_object,
	c_single_invalidate_cached_object,
	c_single_expire_cached_object
};
