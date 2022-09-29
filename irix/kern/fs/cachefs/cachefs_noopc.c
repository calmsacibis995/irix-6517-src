/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_noopc.c 1.11     94/01/05 SMI"
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
#include <sys/kthread.h>
#include <ksys/vproc.h>
#include "cachefs_fs.h"

/*ARGSUSED*/
static int
c_nop_init_cached_object(struct fscache *fscp, struct cnode *cp, vattr_t *vap,
	cred_t *cr)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_nop_init_cached_object: ENTER fscp 0x%p, cp 0x%p\n",
			fscp, cp));
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(!vap || VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_nop_init_cached_object,
		current_pid(), fscp, cp);
	if (!C_CACHING(cp)) {
		return(0);
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	CACHEFS_STATS->cs_objinits++;
	if (vap) {
		VATTR_TO_CATTR(cp->c_vnode, vap, cp->c_attr, AT_SIZE);
		CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_nop_init_cached_object,
	 		0, 0);
		CFS_DEBUG(CFSDEBUG_ATTR,
			printf("c_nop_init_cached_object(line %d): attr update, cp "
				"0x%p\n", __LINE__, cp));
		CFS_DEBUG(CFSDEBUG_SIZE,
			if (cp->c_size != cp->c_attr->ca_size)
				printf("c_nop_modify_cached_object(line %d): cp 0x%p size "
					"change from %lld to %lld\n", __LINE__, cp, cp->c_size,
					cp->c_attr->ca_size));
		cp->c_size = cp->c_attr->ca_size;
		cp->c_flags |= CN_UPDATED;
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_nop_init_cached_object,
			cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
		if (!cp->c_frontvp) {
			error = cachefs_getfrontvp(cp);
			if (error) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("c_nop_init_cached_object(line %d): error %d "
						"getting front vnode\n", __LINE__, error));
				(void)cachefs_nocache(cp);
				error = 0;
			}
		}
	} else {
		vap = CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
		vap->va_mask = AT_ALL;
		/*
		 * We're initilaizing the attributes and the caller did not provide
		 * them.  We must wait for the back FS to respond.
		 */
		(void)BACK_VOP_GETATTR(cp, vap, 0, cr, BACKOP_BLOCK, &error);
		CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_nop_init_cached_object,
	 		0, 0);
		CFS_DEBUG(CFSDEBUG_ATTR,
			printf("c_nop_init_cached_object(line %d): attr update, cp "
				"0x%p\n", __LINE__, cp));
		if (!error) {
			VATTR_TO_CATTR(cp->c_vnode, vap, cp->c_attr, AT_SIZE);
			CFS_DEBUG(CFSDEBUG_SIZE,
				if (cp->c_size != cp->c_attr->ca_size)
					printf("c_nop_modify_cached_object(line %d): cp 0x%p size "
						"change from %lld to %lld\n", __LINE__, cp, cp->c_size,
						cp->c_attr->ca_size));
			cp->c_size = cp->c_attr->ca_size;
			cp->c_flags |= CN_UPDATED;
			CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_nop_init_cached_object,
				cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
			if (!cp->c_frontvp) {
				error = cachefs_getfrontvp(cp);
				if (error) {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("c_nop_init_cached_object(line %d): error %d "
							"getting front vnode\n", __LINE__, error));
					(void)cachefs_nocache(cp);
					error = 0;
				}
			}
		}
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, vap);
	}
	CNODE_TRACE(CNTRACE_SIZE, cp, (void *)c_nop_init_cached_object,
 		cp->c_size, 0);
	ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
		FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_nop_modify_cached_object: EXIT error %d\n", error));
	return (error);
}

/*ARGSUSED*/
static int
c_nop_check_cached_object(struct fscache *fscp, struct cnode *cp, vattr_t *vap,
	cred_t *cr, lockmode_t lm)
{
	vattr_t *attrs = NULL;
	enum backop_stat status;
	int ospl;
	int error = 0;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_nop_check_cached_object,
		current_pid(), fscp, cp);
	CACHEFS_STATS->cs_objchecks++;
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
			vap->va_mask = AT_SIZE;
			status = BACK_VOP_GETATTR(cp, vap, 0, cr, BACKOP_BLOCK, &error);
			if (status == BACKOP_SUCCESS) {
				ospl = mutex_spinlock(&cp->c_statelock);
				CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_nop_check_cached_object,
					0, 0);
				CFS_DEBUG(CFSDEBUG_ATTR,
					printf("c_nop_check_cached_object(line %d): attr update, "
						"cp 0x%p\n", __LINE__, cp));
				if (!C_DIRTY(cp)) {
					CFS_DEBUG(CFSDEBUG_SIZE,
						if (cp->c_size != vap->va_size)
							printf("c_nop_check_cached_object(line %d): cp "
								"0x%p size change from %lld to %lld\n",
								__LINE__, cp, cp->c_size, vap->va_size));
					cp->c_size = vap->va_size;
					CNODE_TRACE(CNTRACE_SIZE, cp,
						(void *)c_nop_check_cached_object, cp->c_size, 0);
				}
				mutex_spinunlock(&cp->c_statelock, ospl);
				ASSERT(CTOV(cp)->v_type != VNON);
			}
			CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrs);
		} else {
			ospl = mutex_spinlock(&cp->c_statelock);
			if (!C_DIRTY(cp)) {
				CFS_DEBUG(CFSDEBUG_SIZE,
					if (cp->c_size != vap->va_size)
						printf("c_nop_check_cached_object(line %d): cp "
							"0x%p size change from %lld to %lld\n",
							__LINE__, cp, cp->c_size, vap->va_size));
				cp->c_size = vap->va_size;
				CNODE_TRACE(CNTRACE_SIZE, cp,
					(void *)c_nop_check_cached_object, cp->c_size, 0);
				ASSERT(CTOV(cp)->v_type != VNON);
			}
			mutex_spinunlock(&cp->c_statelock, ospl);
		}
	}
	return (error);
}

/*ARGSUSED*/
static void
c_nop_modify_cached_object(struct fscache *fscp, struct cnode *cp, cred_t *cr)
{
	vattr_t *attrs;
	int error = 0;
	int ospl;

	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_nop_modify_cached_object: ENTER fscp 0x%p, cp 0x%p\n",
			fscp, cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_nop_modify_cached_object,
		current_pid(), fscp, cp);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(cr));
	if (!C_CACHING(cp)) {
		return;
	}
	ASSERT(VALID_ADDR(cp->c_attr));
	CACHEFS_STATS->cs_objmods++;
	attrs = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone, KM_SLEEP);
	attrs->va_mask = AT_ALL;
	(void)BACK_VOP_GETATTR(cp, attrs, 0, cr, BACKOP_BLOCK, &error);
	if (error == 0) {
		ospl = mutex_spinlock(&cp->c_statelock);
		VATTR_TO_CATTR(cp->c_vnode, attrs, cp->c_attr, 0);
		CNODE_TRACE(CNTRACE_ATTR, cp, (void *)c_nop_modify_cached_object,
	 		0, 0);
		CFS_DEBUG(CFSDEBUG_ATTR,
			printf("c_nop_modify_cached_object(line %d): attr update, cp "
				"0x%p\n", __LINE__, cp));
		cp->c_flags |= CN_UPDATED;
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)c_nop_modify_cached_object,
			cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
		mutex_spinunlock(&cp->c_statelock, ospl);
		if (!cp->c_frontvp) {
			error = cachefs_getfrontvp(cp);
			if (error) {
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("c_nop_modify_cached_object(line %d): error %d "
						"getting front vnode\n", __LINE__, error));
				(void)cachefs_nocache(cp);
				error = 0;
			}
		}
		ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
			FILEHEADER_BLOCK_SIZE(C_TO_FSCACHE(cp)->fs_cache)));
	}
	CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrs);
	CFS_DEBUG(CFSDEBUG_CFSOPS,
		printf("c_nop_modify_cached_object: EXIT\n"));
}

/*ARGSUSED*/
static void
c_nop_invalidate_cached_object(struct fscache *fscp, struct cnode *cp,
	cred_t *cr)
{
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_nop_invalidate_cached_object,
		current_pid(), fscp, cp);
	CACHEFS_STATS->cs_objinvals++;
}

/*ARGSUSED*/
static void
c_nop_expire_cached_object(struct fscache *fscp, struct cnode *cp, cred_t *cr)
{
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)c_nop_expire_cached_object,
		current_pid(), fscp, cp);
	CACHEFS_STATS->cs_objexpires++;
}

struct cachefsops nopcfsops = {
	c_nop_init_cached_object,
	c_nop_check_cached_object,
	c_nop_modify_cached_object,
	c_nop_invalidate_cached_object,
	c_nop_expire_cached_object
};
