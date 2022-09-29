/*
 * Back FS vnode operations.  These will handle disconnection.
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
#include <sys/proc.h>
#include <ksys/vproc.h>

#include "cachefs_fs.h"
#include <string.h>

#define nohang()    (curuthread ? \
			 (curuthread->ut_pproxy->prxy_flags & PRXY_NOHANG) : 0)

enum backop_stat
cachefs_getbackvp(fscache_t *fscp, fid_t *backfid, vnode_t **vpp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	int retry = 0;
	enum backop_stat status;
	vfs_t *back_vfsp;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_getbackvp: ENTER fscp 0x%p\n", fscp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_getbackvp, current_pid(),
		fscp, 0);
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(backfid));
	ASSERT(VALID_ADDR(vpp));
	ASSERT(VALID_ADDR(errorp));
	ASSERT(backfid->fid_len <= MAXFIDSZ);
	back_vfsp = FSC_TO_BACKVFS(fscp);
	if (!back_vfsp) {
		if (opmode == BACKOP_BLOCK) {
			ospl = mutex_spinlock(&fscp->fs_fslock);
			switch (sv_wait_sig(&fscp->fs_reconnect, (PZERO+1),
				&fscp->fs_fslock, ospl)) {
					case 0:
						back_vfsp = FSC_TO_BACKVFS(fscp);
						ASSERT(back_vfsp);
						break;
					case -1:
						*errorp = EINTR;
						return(BACKOP_FAILURE);
					default:
						*errorp = EINVAL;
						return(BACKOP_FAILURE);
			}
		} else {
			*errorp = EHOSTDOWN;
		return(BACKOP_NETERR);
	}
	}
	do {
		CACHEFS_STATS->cs_getbackvp++;
		VFS_VGET(back_vfsp, vpp, backfid, *errorp);
		if (!*errorp && (*vpp == NULL)) {
			*errorp = ESTALE;
		}
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_getbackvp: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" :
						"BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_getbackvp: EXIT error = %d\n", *errorp));
	return (status);
}

/*
 * --------------------------------
 * vnode operations for the back FS
 * --------------------------------
 */
enum backop_stat
cachefs_backop_getattr(cnode_t *cp, vattr_t *attrp, int flag, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	enum backop_stat status;
	vnode_t *backvp;
	int ospl;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_getattr: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(attrp));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	NET_VOP_GETATTR(cp->c_backvp, attrp, flag, credp, opmode, *errorp, status);
	if (*errorp == ESTALE) {
				dnlc_purge_vp(CTOV(cp));
		}
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_getattr: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_readdir(cnode_t *cp, uio_t *uiop, cred_t *credp, int *eofp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_readdir: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(eofp));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	ASSERT(uiop->uio_resid != 0);
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_rdd++;
		VOP_RWLOCK(cp->c_backvp, VRWLOCK_READ);
		VOP_READDIR(cp->c_backvp, uiop, credp, eofp, *errorp);
		VOP_RWUNLOCK(cp->c_backvp, VRWLOCK_READ);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_readdir: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" :
						"BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_readdir: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_setattr(cnode_t *cp, vattr_t *attrp, int flags, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_setattr: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(attrp));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_set++;
		VOP_SETATTR(cp->c_backvp, attrp, flags, credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_setattr: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" :
						"BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_setattr: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_access(cnode_t *cp, int mode, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_access: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_acc++;
		VOP_ACCESS(cp->c_backvp, mode, credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_access: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" :
						"BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_access: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_readlink(cnode_t *cp, uio_t *uiop, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_readlink: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_rdl++;
		VOP_READLINK(cp->c_backvp, uiop, credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_readlink: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" :
						"BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_readlink: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_lookup_back(cnode_t *dcp, char *nm, struct vnode **back_vpp,
	fid_t **fidpp, int flags, struct vnode *rdir, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp = NULL;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_lookup_back: ENTER dcp 0x%p nm %s\n", dcp, nm));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_lookup_back,
		current_pid(), dcp, nm);
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(back_vpp));
	ASSERT(VALID_ADDR(fidpp));
	ASSERT(!rdir || VALID_ADDR(rdir));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!dcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(dcp), dcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(dcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&dcp->c_statelock);
					if (!dcp->c_backvp) {
						dcp->c_backvp = backvp;
						mutex_spinunlock(&dcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&dcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_look++;
		VOP_LOOKUP(dcp->c_backvp, nm, back_vpp, NULL, flags, rdir,
			credp, *errorp);
		if (!*errorp) {
			ASSERT(*back_vpp);
			/*
			 * The lookup was successful.  Get the new file ID.
			 */
			VOP_FID(*back_vpp, fidpp, *errorp);
			if (!*errorp && !*fidpp) {
				/*
				 * We did the lookup, but could not
				 * get the file ID.  Punt.  The
				 * break below should break out of
				 * the switch.
				 */
				*errorp = ESTALE;
			}
		}
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					*back_vpp = NULL;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				*back_vpp = NULL;
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_lookup_back: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(dcp));
			default:
				*back_vpp = NULL;
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_lookup_back: EXIT vp 0x%p fidp 0x%p error = %d\n",
			*back_vpp, *fidpp, *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_create(cnode_t *dcp, char *nm, vattr_t *vap, int excl,
	int mode, vnode_t **devvpp, cred_t *credp, enum backop_mode opmode,
	int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_create: ENTER dcp 0x%p nm %s\n", dcp, nm));
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(vap));
	ASSERT(!devvpp || VALID_ADDR(devvpp));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!dcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(dcp), dcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(dcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&dcp->c_statelock);
					if (!dcp->c_backvp) {
						dcp->c_backvp = backvp;
						mutex_spinunlock(&dcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&dcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_cre++;
		*devvpp = NULL;
		VOP_CREATE(dcp->c_backvp, nm, vap, excl, mode, devvpp, credp,
			*errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_create: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(dcp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_create: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_remove(cnode_t *dcp, char *nm, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_remove: ENTER dcp 0x%p nm %s\n", dcp, nm));
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!dcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(dcp), dcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(dcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&dcp->c_statelock);
					if (!dcp->c_backvp) {
						dcp->c_backvp = backvp;
						mutex_spinunlock(&dcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&dcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_rem++;
		VOP_REMOVE(dcp->c_backvp, nm, credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_remove: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(dcp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_remove: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_link(cnode_t *tdcp, cnode_t *cp, char *tnm, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_link: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(tdcp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(tnm));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!tdcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(tdcp), tdcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(tdcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&tdcp->c_statelock);
					if (!tdcp->c_backvp) {
						tdcp->c_backvp = backvp;
						mutex_spinunlock(&tdcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&tdcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_lnk++;
		VOP_LINK(tdcp->c_backvp, cp->c_backvp, tnm, credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_link: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(tdcp));
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_link: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_rename(cnode_t *odcp, char *onm, cnode_t *ndcp, char *nnm,
	struct pathname *pnp, cred_t *credp, enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_rename: ENTER onm %s nnm %s\n", onm, nnm));
	ASSERT(VALID_ADDR(odcp));
	ASSERT(VALID_ADDR(onm));
	ASSERT(VALID_ADDR(ndcp));
	ASSERT(VALID_ADDR(nnm));
	ASSERT(!pnp || VALID_ADDR(pnp));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!odcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(odcp), odcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(odcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&odcp->c_statelock);
					if (!odcp->c_backvp) {
						odcp->c_backvp = backvp;
						mutex_spinunlock(&odcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&odcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	if (!ndcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(ndcp), ndcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(ndcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&ndcp->c_statelock);
					if (!ndcp->c_backvp) {
						ndcp->c_backvp = backvp;
						mutex_spinunlock(&ndcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&ndcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_ren++;
		VOP_RENAME(odcp->c_backvp, onm, ndcp->c_backvp, nnm, pnp,
			credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_rename: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(ndcp));
				dnlc_purge_vp(CTOV(odcp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_rename: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_mkdir(cnode_t *dcp, char *nm, vattr_t *vap, vnode_t **back_vpp,
	cred_t *credp, enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_mkdir: ENTER dcp 0x%p nm %s\n", dcp, nm));
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(vap));
	ASSERT(VALID_ADDR(back_vpp));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!dcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(dcp), dcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(dcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&dcp->c_statelock);
					if (!dcp->c_backvp) {
						dcp->c_backvp = backvp;
						mutex_spinunlock(&dcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&dcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_mkd++;
		VOP_MKDIR(dcp->c_backvp, nm, vap, back_vpp, credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_mkdir: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(dcp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_mkdir: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_rmdir(cnode_t *dcp, char *nm, vnode_t *cdir, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_rmdir: ENTER dcp 0x%p\n", dcp));
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(nm));
	ASSERT(VALID_ADDR(cdir));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!dcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(dcp), dcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(dcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&dcp->c_statelock);
					if (!dcp->c_backvp) {
						dcp->c_backvp = backvp;
						mutex_spinunlock(&dcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&dcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_rmd++;
		VOP_RMDIR(dcp->c_backvp, nm, cdir, credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_rmdir: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(dcp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_rmdir: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_symlink(cnode_t *dcp, char *lnm, vattr_t *tva, char *tnm,
	cred_t *credp, enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_symlink: ENTER dcp 0x%p\n", dcp));
	ASSERT(VALID_ADDR(dcp));
	ASSERT(VALID_ADDR(lnm));
	ASSERT(VALID_ADDR(tva));
	ASSERT(VALID_ADDR(tnm));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!dcp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(dcp), dcp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(dcp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&dcp->c_statelock);
					if (!dcp->c_backvp) {
						dcp->c_backvp = backvp;
						mutex_spinunlock(&dcp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&dcp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_sym++;
		VOP_SYMLINK(dcp->c_backvp, lnm, tva, tnm, credp, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_symlink: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(dcp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_symlink: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_frlock(cnode_t *cp, int cmd, struct flock *bfp, int flag,
	off_t offset, cred_t *credp, enum backop_mode opmode, 
	vrwlock_t vrwlock, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_frlock: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(bfp));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_frl++;
		VOP_FRLOCK(cp->c_backvp, cmd, bfp, flag, offset, vrwlock, credp,
			*errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_frlock: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_frlock: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_readvp(cnode_t *cp, uio_t *uiop, int ioflag, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;
	off_t retry_offset;
	size_t retry_len;
	void *retry_base;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_readvp: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_rdv++;
		if (opmode == BACKOP_BLOCK) {
			retry_offset = uiop->uio_offset;
			retry_len = uiop->uio_iov->iov_len;
			retry_base = uiop->uio_iov->iov_base;
		}
		VOP_RWLOCK(cp->c_backvp, VRWLOCK_READ);
		VOP_READ(cp->c_backvp, uiop, ioflag | IO_ISLOCKED, credp, 
			 &curuthread->ut_flid, *errorp);
		VOP_RWUNLOCK(cp->c_backvp, VRWLOCK_READ);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
					uiop->uio_offset = retry_offset;
					uiop->uio_resid = uiop->uio_iov->iov_len = retry_len;
					uiop->uio_iov->iov_base = retry_base;
					uiop->uio_iovcnt = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_readvp: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_readvp: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_writevp(cnode_t *cp, uio_t *uiop, int ioflag, cred_t *credp,
	enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;
	off_t retry_offset;
	size_t retry_len;
	void *retry_base;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_writevp: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(uiop));
	ASSERT(VALID_ADDR(credp));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		CACHEFS_STATS->cs_backops.cb_wrv++;
		if (opmode == BACKOP_BLOCK) {
			retry_offset = uiop->uio_offset;
			retry_len = uiop->uio_iov->iov_len;
			retry_base = uiop->uio_iov->iov_base;
		}
		VOP_RWLOCK(cp->c_backvp,
			(ioflag & IO_DIRECT) ?
				VRWLOCK_WRITE_DIRECT : VRWLOCK_WRITE);
		VOP_WRITE(cp->c_backvp, uiop, ioflag | IO_ISLOCKED, credp, 
			  &curuthread->ut_flid,
			*errorp);
		ASSERT(uiop->uio_sigpipe == 0);
		VOP_RWUNLOCK(cp->c_backvp,
			(ioflag & IO_DIRECT) ?
				VRWLOCK_WRITE_DIRECT : VRWLOCK_WRITE);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
					uiop->uio_offset = retry_offset;
					uiop->uio_resid = uiop->uio_iov->iov_len = retry_len;
					uiop->uio_iov->iov_base = retry_base;
					uiop->uio_iovcnt = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_writevp: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_writevp: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_fcntl(cnode_t *cp, int cmd, void *arg, int flags, off_t offset,
	cred_t *cr, rval_t *rvp, enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_fcntl: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, opmode, errorp)) {
				case BACKOP_FAILURE:
					if (*errorp == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		VOP_FCNTL(cp->c_backvp, cmd, arg, flags, offset, cr, rvp,
			*errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					printf("cachefs_backop_fcntl: network error %d %s %s\n",
						*errorp, (opmode == BACKOP_BLOCK) ?
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK",
						retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_fcntl: EXIT error = %d\n", *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_vnode_change(cnode_t *cp, vchange_t cmd, __psint_t val,
	enum backop_mode mode)
{
	int ospl;
	vnode_t *backvp;
	enum backop_stat status;
	int error;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_vnode_change: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid,
			&backvp, mode, &error)) {
				case BACKOP_FAILURE:
					if (error == ESTALE) {
						dnlc_purge_vp(CTOV(cp));
					}
				default:
					return(status);
				case BACKOP_SUCCESS:
					ospl = mutex_spinlock(&cp->c_statelock);
					if (!cp->c_backvp) {
						cp->c_backvp = backvp;
						mutex_spinunlock(&cp->c_statelock, ospl);
					} else {
						mutex_spinunlock(&cp->c_statelock, ospl);
						VN_RELE(backvp);
					}
		}
	} else
		status = BACKOP_SUCCESS;

	CACHEFS_STATS->cs_backvnops++;
	VOP_VNODE_CHANGE(cp->c_backvp, cmd, val);

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_vnode_change: EXIT\n"));
	return(status);
}

enum backop_stat
cachefs_backop_attr_get(cnode_t *cp, char *name, char *value, int *valuelenp,
			int flags, cred_t *cr, enum backop_mode opmode,
			int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		  printf("cachefs_backop_attr_get: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(name));
	ASSERT(VALID_ADDR(value));
	ASSERT(VALID_ADDR(valuelenp));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid, &backvp, opmode, errorp)) {
			case BACKOP_FAILURE:
				if (*errorp == ESTALE) {
					dnlc_purge_vp(CTOV(cp));
				}
			default:
				return(status);
			case BACKOP_SUCCESS:
				ospl = mutex_spinlock(&cp->c_statelock);
				if (!cp->c_backvp) {
					cp->c_backvp = backvp;
					mutex_spinunlock(&cp->c_statelock,
							 ospl);
				} else {
					mutex_spinunlock(&cp->c_statelock,
							 ospl);
					VN_RELE(backvp);
				}
				break;
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		VOP_ATTR_GET(cp->c_backvp, name, value, valuelenp,
			     flags, cr, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					  printf("cachefs_backop_attr_get: network error %d %s %s\n", *errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" : "BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
				break;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		  printf("cachefs_backop_attr_get: EXIT error = %d\n",
			 *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_attr_set(cnode_t *cp, char *name, char *value, int valuelen,
			int flags, cred_t *cr, enum backop_mode opmode,
			int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		  printf("cachefs_backop_attr_set: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(name));
	ASSERT(VALID_ADDR(value));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid, &backvp, opmode, errorp)) {
			case BACKOP_FAILURE:
				if (*errorp == ESTALE) {
					dnlc_purge_vp(CTOV(cp));
				}
			default:
				return(status);
			case BACKOP_SUCCESS:
				ospl = mutex_spinlock(&cp->c_statelock);
				if (!cp->c_backvp) {
					cp->c_backvp = backvp;
					mutex_spinunlock(&cp->c_statelock,
							 ospl);
				} else {
					mutex_spinunlock(&cp->c_statelock,
							 ospl);
					VN_RELE(backvp);
				}
				break;
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		VOP_ATTR_SET(cp->c_backvp, name, value, valuelen,
			     flags, cr, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					  printf("cachefs_backop_fcntl: network error %d %s %s\n", *errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" : "BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
				break;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		  printf("cachefs_backop_attr_set: EXIT error = %d\n",
			 *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_attr_remove(cnode_t *cp, char *name, int flags, cred_t *cr,
			   enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		  printf("cachefs_backop_attr_remove: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(name));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid, &backvp, opmode, errorp)) {
			case BACKOP_FAILURE:
				if (*errorp == ESTALE) {
					dnlc_purge_vp(CTOV(cp));
				}
			default:
				return(status);
			case BACKOP_SUCCESS:
				ospl = mutex_spinlock(&cp->c_statelock);
				if (!cp->c_backvp) {
					cp->c_backvp = backvp;
					mutex_spinunlock(&cp->c_statelock,
							 ospl);
				} else {
					mutex_spinunlock(&cp->c_statelock,
							 ospl);
					VN_RELE(backvp);
				}
				break;
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		VOP_ATTR_REMOVE(cp->c_backvp, name, flags, cr, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					  printf("cachefs_backop_fcntl: network error %d %s %s\n", *errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" : "BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
				break;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		  printf("cachefs_backop_attr_remove: EXIT error = %d\n",
			 *errorp));
	return(status);
}

enum backop_stat
cachefs_backop_attr_list(cnode_t *cp, char *buffer, int bufsize, int flags,
			 struct attrlist_cursor_kern *cursor, cred_t *cr,
			 enum backop_mode opmode, int *errorp)
{
	int ospl;
	vnode_t *backvp;
	int retry = 0;
	enum backop_stat status;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		  printf("cachefs_backop_attr_list: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(buffer));
	ASSERT(VALID_ADDR(cursor));
	ASSERT(VALID_ADDR(cr));
	ASSERT(VALID_ADDR(errorp));
	if (!cp->c_backvp) {
		switch (status = cachefs_getbackvp(C_TO_FSCACHE(cp), cp->c_backfid, &backvp, opmode, errorp)) {
			case BACKOP_FAILURE:
				if (*errorp == ESTALE) {
					dnlc_purge_vp(CTOV(cp));
				}
			default:
				return(status);
			case BACKOP_SUCCESS:
				ospl = mutex_spinlock(&cp->c_statelock);
				if (!cp->c_backvp) {
					cp->c_backvp = backvp;
					mutex_spinunlock(&cp->c_statelock,
							 ospl);
				} else {
					mutex_spinunlock(&cp->c_statelock,
							 ospl);
					VN_RELE(backvp);
				}
				break;
		}
	}
	do {
		CACHEFS_STATS->cs_backvnops++;
		VOP_ATTR_LIST(cp->c_backvp, buffer, bufsize, flags, cursor,
			      cr, *errorp);
		switch (*errorp) {
			case 0:
				status = BACKOP_SUCCESS;
				retry = 0;
				break;
			case ETIMEDOUT:
				if (nohang()) {
					retry = 0;
					status = BACKOP_FAILURE;
					break;
				}
				/* intentional fall-thru */
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNREFUSED:
			case ENOBUFS:
			case ECONNABORTED:
			case ENETRESET:
			case ECONNRESET:
			case ENONET:
			case ESHUTDOWN:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				if (opmode == BACKOP_BLOCK) {
					retry = 1;
				} else {
					retry = 0;
				}
				status = BACKOP_NETERR;
				CACHEFS_STATS->cs_neterror++;
				CFS_DEBUG(CFSDEBUG_NETERR,
					  printf("cachefs_backop_fcntl: network error %d %s %s\n", *errorp, (opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" : "BACKOP_NONBLOCK", retry ? "retry" : ""));
				break;
			case ESTALE:
				dnlc_purge_vp(CTOV(cp));
			default:
				status = BACKOP_FAILURE;
				retry = 0;
				break;
		}
	} while (retry);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		  printf("cachefs_backop_attr_list: EXIT error = %d\n",
			 *errorp));
	return(status);
}

/*
 * ------------------------------
 * vfs operations for the back FS
 * ------------------------------
 */
enum backop_stat
cachefs_backop_statvfs(fscache_t *fscp, struct statvfs *sbp,
	enum backop_mode opmode, int *errorp)
{
	enum backop_stat status;
	vfs_t *back_vfsp;
	int ospl;

	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_statvfs: ENTER fscp 0x%p\n", fscp));
	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(sbp));
	back_vfsp = FSC_TO_BACKVFS(fscp);
	if (!back_vfsp) {
		if (opmode == BACKOP_BLOCK) {
			ospl = mutex_spinlock(&fscp->fs_fslock);
			switch (sv_wait_sig(&fscp->fs_reconnect, (PZERO+1),
				&fscp->fs_fslock, ospl)) {
			case 0:
						back_vfsp = FSC_TO_BACKVFS(fscp);
						ASSERT(back_vfsp);
				break;
					case -1:
						*errorp = EINTR;
						return(BACKOP_FAILURE);
					default:
						*errorp = EINVAL;
						return(BACKOP_FAILURE);
			}
				} else {
			*errorp = EHOSTDOWN;
			return(BACKOP_NETERR);
				}
		}
	NET_VFS_STATVFS(back_vfsp, sbp, NULL, opmode, *errorp, status);
	CFS_DEBUG(CFSDEBUG_BACKOPS,
		printf("cachefs_backop_statvfs: EXIT error = %d\n", *errorp));
	return(status);
}
