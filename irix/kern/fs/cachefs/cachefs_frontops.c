/*
 * Front FS vnode operations.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <ksys/vfile.h>
#include <sys/filio.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/dirent.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/dnlc.h>
#include <sys/kthread.h>
#include <sys/uthread.h>
#include <ksys/vproc.h>

#include "cachefs_fs.h"
#include <string.h>

int
cachefs_getfrontdirvp(struct cnode *cp)
{
	int ospl;
	int error = 0;
	vnode_t *vp = NULL;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_getfrontdirvp: ENTER cp %p\n", cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_getfrontdirvp,
		current_pid(), cp, 0);
	ASSERT(VALID_ADDR(cp));
	ASSERT(cp->c_frontdirfid.fid_len &&
		(cp->c_frontdirfid.fid_len <= MAXFIDSZ));
	ASSERT(CTOV(cp)->v_count > 0);
	VFS_VGET(FSC_TO_FRONTVFS(C_TO_FSCACHE(cp)), &vp,
		&cp->c_frontdirfid, error);
	if (!error) {
		if (vp != NULL) {
			ospl = mutex_spinlock(&cp->c_statelock);
			if (!cp->c_frontdirvp) {
				cp->c_frontdirvp = vp;
				mutex_spinunlock(&cp->c_statelock, ospl);
			} else {
				mutex_spinunlock(&cp->c_statelock, ospl);
				VN_RELE(vp);
			}
		} else {
			error = ESTALE;
		}
	}

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_getfrontdirvp: EXIT error = %d\n", error));
	return (error);
}

int
cachefs_getfrontvp(struct cnode *cp)
{
	int error = 0;
	vnode_t *vp = NULL;
	int ospl;

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_getfrontvp: ENTER cp %p\n", cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_getfrontvp,
		current_pid(), cp, 0);
	ASSERT(VALID_ADDR(cp));
	if (!cp->c_frontfid.fid_len) {
		return(ESTALE);
	}
	ASSERT(cp->c_frontfid.fid_len <= MAXFIDSZ);
	ASSERT(!(cp->c_flags & CN_RECLAIM));
	VFS_VGET(FSC_TO_FRONTVFS(C_TO_FSCACHE(cp)), &vp, &cp->c_frontfid, error);
	if (!error) {
		if (vp != NULL) {
			ospl = mutex_spinlock(&cp->c_statelock);
			if (!cp->c_frontvp) {
				cp->c_frontvp = vp;
				mutex_spinunlock(&cp->c_statelock, ospl);
			} else {
				ASSERT(cp->c_frontvp == vp);
				mutex_spinunlock(&cp->c_statelock, ospl);
				VN_RELE(vp);
			}
		} else {
			error = ESTALE;
		}
	}

	CFS_DEBUG(CFSDEBUG_SUBR,
		printf("cachefs_getfrontvp: EXIT error = %d\n", error));
	return (error);
}

/*
 * --------------------------------
 * vnode operations for the back FS
 * --------------------------------
 */
int
cachefs_frontop_getattr(cnode_t *cp, vattr_t *attrp, int flag, cred_t *credp)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_getattr: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(attrp));
	ASSERT(VALID_ADDR(credp));
	if (cp->c_frontvp || !(error = cachefs_getfrontvp(cp))) {
		ASSERT(VALID_ADDR(cp->c_frontvp));
		VOP_GETATTR(cp->c_frontvp, attrp, flag, credp, error);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_frontop_getattr: nocache cp 0x%p on error %d from "
				"cachefs_getfrontvp\n", cp, error));
		cachefs_nocache(cp);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_getattr: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_setattr(cnode_t *cp, vattr_t *attrp, int flags, cred_t *credp)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_setattr: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(attrp));
	ASSERT(VALID_ADDR(credp));
	if (cp->c_frontvp || !(error = cachefs_getfrontvp(cp))) {
		ASSERT(VALID_ADDR(cp->c_frontvp));
		VOP_SETATTR(cp->c_frontvp, attrp, flags, credp, error);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_frontop_setattr: nocache cp 0x%p on error %d from "
				"cachefs_getfrontvp\n", cp, error));
		cachefs_nocache(cp);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_setattr: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_fsync(cnode_t *cp, int syncflag, cred_t *credp,
			off_t start, off_t stop)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_fsync: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(credp));
	if (cp->c_frontvp) {
		ASSERT(VALID_ADDR(cp->c_frontvp));
		VOP_FSYNC(cp->c_frontvp, syncflag, credp, start, stop, error);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_fsync: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_lookup(cnode_t *dcp, char *nm, struct vnode **vpp,
	struct pathname *pnp, int flags, struct vnode *rdir, cred_t *cr)
{
	int error;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_lookup: ENTER dcp 0x%p\n", dcp));
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(cr));
	if (dcp->c_frontdirvp || !(error = cachefs_getfrontdirvp(dcp))) {
		ASSERT(VALID_ADDR(dcp->c_frontdirvp));
		VOP_LOOKUP(dcp->c_frontdirvp, nm, vpp, pnp, flags, rdir, cr,
				error);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_frontop_lookup: nocache cp 0x%p on error %d from "
				"cachefs_getfrontdirvp\n", dcp, error));
		cachefs_nocache(dcp);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_lookup: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_link(cnode_t *tdcp, cnode_t *cp, char *tnm, cred_t *credp)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_link: ENTER tdcp 0x%p cp 0x%p tnm %s\n",
			tdcp, cp, tnm));
	ASSERT(VALID_ADDR(tdcp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(tnm));
	ASSERT(VALID_ADDR(credp));
	if (!tdcp->c_frontdirvp) {
		error = cachefs_getfrontdirvp(tdcp);
		if (error) {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_frontop_link: nocache cp 0x%p on error %d "
					"from cachefs_getfrontdirvp\n", cp, error));
			cachefs_nocache(tdcp);
			return(error);
		}
	}
	if (!cp->c_frontvp) {
		if (error = cachefs_getfrontvp(cp)) {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_frontop_link: nocache cp 0x%p on error %d "
					"from cachefs_getfrontvp\n", cp, error));
			cachefs_nocache(cp);
			return(error);
		}
	}
	ASSERT(VALID_ADDR(tdcp->c_frontdirvp));
	ASSERT(VALID_ADDR(cp->c_frontvp));
	VOP_LINK(tdcp->c_frontdirvp, cp->c_frontvp, tnm, credp, error);
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_link: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_readvp(cnode_t *cp, uio_t *uiop, int ioflag, cred_t *credp)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_readvp: ENTER cp 0x%p offset %lld len %ld\n",
			cp, uiop->uio_offset, uiop->uio_resid));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(credp));
	if (cp->c_frontvp || !(error = cachefs_getfrontvp(cp))) {
		ASSERT(VALID_ADDR(cp->c_frontvp));
		VOP_RWLOCK(cp->c_frontvp, VRWLOCK_READ);
		VOP_READ(cp->c_frontvp, uiop, ioflag | IO_ISLOCKED, credp,
			&curuthread->ut_flid, error);
		VOP_RWUNLOCK(cp->c_frontvp, VRWLOCK_READ);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_frontop_readvp: nocache cp 0x%p on error %d "
				"from cachefs_getfrontvp\n", cp, error));
		cachefs_nocache(cp);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_readvp: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_writevp(cnode_t *cp, uio_t *uiop, int ioflag, cred_t *credp)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_writevp: ENTER cp 0x%p offset %lld len %ld "
			"first data 0x%x\n", cp, uiop->uio_offset, uiop->uio_resid,
			*((int *)uiop->uio_iov->iov_base)));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(credp));
	if (cp->c_frontvp || !(error = cachefs_getfrontvp(cp))) {
		ASSERT(VALID_ADDR(cp->c_frontvp));
		VOP_RWLOCK(cp->c_frontvp,
			(ioflag & IO_DIRECT) ? VRWLOCK_WRITE_DIRECT : VRWLOCK_WRITE);
		VOP_WRITE(cp->c_frontvp, uiop, ioflag | IO_ISLOCKED, credp,
			&curuthread->ut_flid, error);
		ASSERT(uiop->uio_sigpipe == 0);
		VOP_RWUNLOCK(cp->c_frontvp,
			(ioflag & IO_DIRECT) ? VRWLOCK_WRITE_DIRECT : VRWLOCK_WRITE);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_frontop_writevp: nocache cp 0x%p on error %d "
				"from cachefs_getfrontvp\n", cp, error));
		cachefs_nocache(cp);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_writevp: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_rename(cnode_t *odcp, char *onm, cnode_t *ndcp,
	char *nnm, struct pathname *pnp, cred_t *cr)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_rename: ENTER odcp 0x%p ndcp 0x%p onm %s "
			"nnm %s\n", odcp, ndcp, onm, nnm));
	ASSERT(VALID_ADDR(odcp));
	ASSERT(VALID_ADDR(ndcp));
	ASSERT(VALID_ADDR(onm));
	ASSERT(VALID_ADDR(nnm));
	ASSERT(VALID_ADDR(cr));
	if (!odcp->c_frontdirvp) {
		error = cachefs_getfrontdirvp(odcp);
		if (error) {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_frontop_rename: nocache cp 0x%p on error %d "
					"from cachefs_getfrontdirvp\n", odcp, error));
			cachefs_nocache(odcp);
			return(error);
		}
	}
	if (!ndcp->c_frontdirvp) {
		error = cachefs_getfrontdirvp(ndcp);
		if (error) {
			CFS_DEBUG(CFSDEBUG_NOCACHE,
				printf("cachefs_frontop_rename: nocache cp 0x%p on error %d "
					"from cachefs_getfrontdirvp\n", ndcp, error));
			cachefs_nocache(ndcp);
			return(error);
		}
	}
	ASSERT(VALID_ADDR(odcp->c_frontdirvp));
	ASSERT(VALID_ADDR(ndcp->c_frontdirvp));
	VOP_RENAME(odcp->c_frontdirvp, onm, ndcp->c_frontdirvp, nnm, pnp,
		cr, error);
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_rename: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_remove(cnode_t *dcp, char *nm, cred_t *cr)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_remove: ENTER dcp 0x%p nm %s\n", dcp, nm));
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(cr));
	if (dcp->c_frontdirvp || !(error = cachefs_getfrontdirvp(dcp))) {
		ASSERT(VALID_ADDR(dcp->c_frontdirvp));
		VOP_REMOVE(dcp->c_frontdirvp, nm, cr, error);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_frontop_remove: nocache cp 0x%p on error %d "
				"from cachefs_getfrontdirvp\n", dcp, error));
		cachefs_nocache(dcp);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_remove: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_fid(cnode_t *cp, struct fid **fidpp)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_fid: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	if (cp->c_frontvp || !(error = cachefs_getfrontvp(cp))) {
		ASSERT(VALID_ADDR(cp->c_frontvp));
		VOP_FID(cp->c_frontvp, fidpp, error);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_frontop_fid: nocache cp 0x%p on error %d "
				"from cachefs_getfrontvp\n", cp, error));
		cachefs_nocache(cp);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_fid: EXIT error = %d\n", error));
	return(error);
}

int
cachefs_frontop_fid2(cnode_t *cp, struct fid *fidp)
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_fid: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	if (cp->c_frontvp || !(error = cachefs_getfrontvp(cp))) {
		ASSERT(VALID_ADDR(cp->c_frontvp));
		VOP_FID2(cp->c_frontvp, fidp, error);
	} else {
		CFS_DEBUG(CFSDEBUG_NOCACHE,
			printf("cachefs_frontop_fid2: nocache cp 0x%p on error %d "
				"from cachefs_getfrontvp\n", cp, error));
		cachefs_nocache(cp);
	}
	CFS_DEBUG(CFSDEBUG_FRONTOPS,
		printf("cachefs_frontop_fid2: EXIT error = %d\n", error));
	return(error);
}
