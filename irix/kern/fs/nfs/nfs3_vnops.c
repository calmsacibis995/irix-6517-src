/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * $Revision: 1.248 $
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986, 1987, 1988, 1989, 1990, 1991, 1992  Sun Microsystems, Inc.
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#include "types.h"
#include <string.h>
#include <sys/buf.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/dirent.h>
#include <sys/dnlc.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/pfdat.h>		/* page flushing prototypes */
#include <ksys/vproc.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/unistd.h>
#include <sys/xlate.h>
#include <sys/kfcntl.h>
#include <sys/acl.h>
#include <sys/attributes.h>
#include <fs/specfs/spec_lsnode.h>
#include <limits.h>
#include <sys/ksignal.h>
#include "xdr.h"
#include "lockmgr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "nfs3_clnt.h"
#include "rnode.h"
#include "nfs3_rnode.h"
#include "auth.h"
#include "svc.h"
#include "xattr.h"
#include "nlm_debug.h"
#include "bds.h"
#include <sys/uuid.h>
#include <sys/fs/xfs_fsops.h>

#include <sys/ktrace.h>
#undef KTRACE

#ifdef NSD
#include "nsd.h"
#endif

/*
 * Perform MAC & ACL checks on client. Permission bit checks are not done
 * here, but on the server. Because the ACL and permission bits are kept
 * in sync, that should work ok.
 */
#define _NFS_IACCESS(bdp,mode,cr)					\
	do {								\
		int error;						\
		if (error = _MAC_NFS_IACCESS(bdp,mode,cr))		\
			return (error);					\
		if ((error = _ACL_NFS_IACCESS(bdp,mode,cr)) > 0)	\
			return (error);					\
	} while (0)

/*
 * Is "chown" restricted?   If it is, a process not
 * running as super-user cannot change the owner, 
 * and can only change the group to a group of which
 * it's currently a member.
 */
int	rstchown = 0;

extern mutex_t nfs3_linkrename_lock;

extern int nfs3_unst_wtmark;
extern time_t nfs3_commit_prd;



#ifdef NFSCACHEDEBUG
int nfs3_cache_valid(rnode_t *, timespec_t , off_t);
#endif /* NFSCACHEDEBUG */
extern void	dump_rddir_cache(char *, rddir_cache *);
extern unsigned long	btopr(unsigned long);

static void	verf_update(rnode_t *, off_t, int, writeverf3 *, struct buf *,
			cred_t *);
static int	commit_file(rnode_t *, cred_t *);
static int	rw3vp(bhv_desc_t *, struct uio *, enum uio_rw, int, cred_t *,
			flid_t *fl);
static int	nfs3write(bhv_desc_t *, caddr_t, off_t, long, cred_t *,
			stable_how *, int, struct buf *);
static int	nfs3read(bhv_desc_t *, buf_t *, off_t, long, unsigned *, cred_t *,
			post_op_attr *, int);
static int	nfs3setattr(bhv_desc_t *, struct vattr *, int, cred_t *);
static int	nfs3lookup(bhv_desc_t *, char *, vnode_t **,
			struct pathname *, int, vnode_t *, cred_t *);
static int	nfs3create(bhv_desc_t *, char *, struct vattr *, int,
			int, vnode_t **, cred_t *);
static int	nfs3mknod(bhv_desc_t *, char *, struct vattr *, int,
			int, vnode_t **, cred_t *);
static int	nfs3rename(bhv_desc_t *, char *, vnode_t *, char *, cred_t *);
static int	do_nfs3readdir(bhv_desc_t *, rddir_cache *, cred_t *, u_char);
static void	nfs3readdir(bhv_desc_t *, rddir_cache *, cred_t *, u_char);
static void	nfs3readdirplus(bhv_desc_t *, rddir_cache *, cred_t *, u_char);
static void     nospc_cleanup(struct rnode *, struct buf *);
static int	nfs3_bio(bhv_desc_t *, struct buf *, stable_how *, cred_t *);
static void	nfs3_riodone(struct rnode *);
static void	nfs3_riowait (struct rnode *);
static int	nfs3_commit(bhv_desc_t *, cred_t *, int);

extern int	nfs3_getattr_otw(bhv_desc_t *vp, struct vattr *vap,
			cred_t *cr);
extern void	setdiropargs3(struct diropargs3 *, char *, bhv_desc_t *);
extern void	vattr_to_sattr3(struct vattr *, struct sattr3 *);
extern void     nfs_async_readdir(bhv_desc_t *, struct rddir_cache *,
		    cred_t *,
		    int (*)(bhv_desc_t *, struct rddir_cache *, cred_t *, u_char),
		    u_char);
extern void	nfs_async_bio(bhv_desc_t *, struct buf *,
		    cred_t *, int (*)(bhv_desc_t *, struct buf *, stable_how *, cred_t *));
extern int	makenfs3node(nfs_fh3 *, struct fattr3 *,
		    struct vfs *, bhv_desc_t **, cred_t *);
extern void	nfs_cache_check(bhv_desc_t *, timespec_t, timespec_t,
				off_t, cred_t *);
extern void	nfs3_cache_check(bhv_desc_t *,
		    struct wcc_attr *, cred_t *);
extern void	nfs3_cache_check_fattr3(bhv_desc_t *,
		    struct fattr3 *, cred_t *);
extern int	nfs3_attrcache(bhv_desc_t *, struct fattr3 *, 
				enum staleflush, int *);
extern int      rfs3call(struct mntinfo *, int, xdrproc_t, caddr_t,
		    xdrproc_t, caddr_t, cred_t *, int *, enum nfsstat3 *, int);
extern int	nfs3_lockctl(bhv_desc_t *, struct flock *, int, struct cred *);
extern mode_t	setdirmode3(bhv_desc_t *, mode_t);
extern gid_t	setdirgid3(bhv_desc_t *);

extern void	nfs_purge_caches(bhv_desc_t *, cred_t *);
extern void	nfs_purge_access_cache(bhv_desc_t *);
extern void	nfs_purge_rddir_cache(bhv_desc_t *);
extern int	nfs3_validate_caches(bhv_desc_t *, cred_t *);
extern void	nfs3_cache_wcc_data(bhv_desc_t *, wcc_data *, cred_t *, int *);
extern int	nfs_setfl(bhv_desc_t *p, int of, int nf, cred_t *cr);
extern minor_t	geteminor(dev_t);
extern major_t	getemajor(dev_t);
extern int	crcmp(struct cred *,struct cred *);
static int	nfs3_cleanlocks(bhv_desc_t *, pid_t, long, struct cred *);
int 		nfs3_do_commit(bhv_desc_t *, offset3, count3, 
				cred_t *, struct buf *);
extern int	async_bio;

extern int	nbuf;
extern volatile int	b_unstablecnt;

/*
 * Error flags used to pass information about certain special errors
 * which need to be handled specially.
 */
#define	NFS_EOF			-98

/*
 * These are the vnode ops routines which implement the vnode interface to
 * the networked file system.  These routines just take their parameters,
 * make them look networkish by putting the right info into interface structs,
 * and then calling the appropriate remote routine(s) to do the work.
 *
 * Note on directory name lookup cacheing:  If we detect a stale fhandle,
 * we purge the directory cache relative to that vnode.  This way, the
 * user won't get burned by the cache repeatedly.  See <nfs/rnode.h> for
 * more details on rnode locking.
 */

static int	nfs3_close(bhv_desc_t *, int, lastclose_t, struct cred *);
static int	nfs3_open(bhv_desc_t *, vnode_t **, mode_t, struct cred *);
static int	nfs3_read(bhv_desc_t *, struct uio *, int, cred_t *, flid_t *);
static int	nfs3_write(bhv_desc_t *, struct uio *, int, cred_t *, 
			   flid_t *);
static int	nfs3_ioctl(bhv_desc_t *, int, void *, int, struct cred *, 
			   int *, struct vopbd *);
static int	nfs3_getattr(bhv_desc_t *, struct vattr *, int, cred_t *);
static int	nfs3_setattr(bhv_desc_t *, struct vattr *, int, cred_t *);
static int	nfs3_access(bhv_desc_t *, int, cred_t *);
static int	nfs3_readlink(bhv_desc_t *, struct uio *, cred_t *);
static int	nfs3_fsync(bhv_desc_t *, int, cred_t *, off_t, off_t);
static int	nfs3_inactive(bhv_desc_t *, cred_t *);
static int	nfs3_lookup(bhv_desc_t *, char *, vnode_t **,
			struct pathname *, int, vnode_t *, cred_t *);
static int	nfs3_create(bhv_desc_t *, char *, struct vattr *, int,
			int, vnode_t **, cred_t *);
static int	nfs3_remove(bhv_desc_t *, char *, cred_t *);
static int	nfs3_link(bhv_desc_t *, vnode_t *, char *, cred_t *);
static int	nfs3_rename(bhv_desc_t *, char *, vnode_t *,
			char *, struct pathname *, cred_t *);
static int	nfs3_mkdir(bhv_desc_t *, char *, register struct vattr *,
			vnode_t **, cred_t *);
static int	nfs3_rmdir(bhv_desc_t *, char *, vnode_t *, cred_t *);
static int	nfs3_symlink(bhv_desc_t *, char *, struct vattr *, char *,
			cred_t *);
static int	nfs3_readdir(bhv_desc_t *, struct uio *, cred_t *, int *);
static int	nfs3_fid(bhv_desc_t *, struct fid **);
static int	nfs3_bmap(bhv_desc_t *vp, off_t offset, ssize_t count, 
		  int flag, struct cred *cred, struct bmapval *bmv, int *nbmv);
static int	nfs3_seek(bhv_desc_t *, off_t, off_t *);
static int	nfs3_frlock(bhv_desc_t *, int, struct flock *, int, off_t,
			vrwlock_t, struct cred *);
static int	nfs3_pathconf(bhv_desc_t *, int, long *, cred_t *);
static int	nfs3_reclaim(bhv_desc_t *, int);
static void	nfs3_strategy(bhv_desc_t *, struct buf *);
static int	nfs3_getxattr(bhv_desc_t *, char *, char *, int *, int,
			      cred_t *);
static int	nfs3_setxattr(bhv_desc_t *, char *, char *, int, int,
			      cred_t *);
static int	nfs3_rmxattr(bhv_desc_t *, char *, int, cred_t *);
static int	nfs3_listxattr(bhv_desc_t *, char *, int, int,
			       attrlist_cursor_kern_t *, cred_t *);
extern int	nfs3_fcntl(bhv_desc_t *, int, void *, int, off_t, cred_t *,
			   rval_t  *);
static int	nfs3_background_commit(rnode_t *, commit_t *);
int		nfs3_async_commit(bhv_desc_t *, struct buf *);
static void	nfs3_cancel_bgcommit(commit_t *);
static int	nfs3_do_background_commit(bhv_desc_t *, cred_t *);

#if _MIPS_SIM == _ABI64
int irix5_n32_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
#endif

vnodeops_t nfs3_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	nfs3_open,	/* vop_open */
	nfs3_close,	/* vop_close */
	nfs3_read,	/* vop_read */
	nfs3_write,	/* vop_write */
	nfs3_ioctl,	/* vop_ioctl */
	nfs_setfl,	/* vop_setfl - changed to allow O_DIRECT for bds */
	nfs3_getattr,	/* vop_getattr */
	nfs3_setattr,	/* vop_setattr */
	nfs3_access,	/* vop_access */
	nfs3_lookup,	/* vop_lookup */
	nfs3_create,	/* vop_create */
	nfs3_remove,	/* vop_remove */
	nfs3_link,	/* vop_link */
	nfs3_rename,	/* vop_rename */
	nfs3_mkdir,	/* vop_mkdir */
	nfs3_rmdir,	/* vop_rmdir */
	nfs3_readdir,	/* vop_readdir */
	nfs3_symlink,	/* vop_symlink */
	nfs3_readlink,	/* vop_readlink */
	nfs3_fsync,	/* vop_fsync */
	nfs3_inactive,	/* vop_inactive */
	nfs3_fid,	/* vop_fid */
	(vop_fid2_t)fs_nosys,
	nfs_rwlock,	/* vop_rwlock */
	nfs_rwunlock,	/* vop_rwunlock */
	nfs3_seek,	/* vop_seek */
	fs_cmp,		/* vop_cmp */
	nfs3_frlock,	/* vop_frlock */
	(vop_realvp_t)fs_nosys,
	nfs3_bmap,	/* vop_bmap */
	nfs3_strategy,	/* vop_strategy */
	(vop_map_t)fs_noerr,
	(vop_addmap_t)fs_noerr,
	(vop_delmap_t)fs_noerr,
	fs_poll,	/* vop_poll */
	(vop_dump_t)fs_nosys,
	nfs3_pathconf,	/* vop_pathconf */
	(vop_allocstore_t)fs_nosys,
	nfs3_fcntl,	/* vop_fcntl */
	nfs3_reclaim,	/* vop_reclaim */
	nfs3_getxattr,	/* vop_attr_get */
	nfs3_setxattr,	/* vop_attr_set */
	nfs3_rmxattr,	/* vop_attr_remove */
	nfs3_listxattr,	/* vop_attr_list */
	fs_cover,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	fs_tosspages,
	fs_flushinval_pages,
	fs_flush_pages,
	fs_invalfree_pages,
	fs_pages_sethole,
	nfs3_async_commit, /* vop_commit */
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};

#define BDS_GET_FCNTL(x) \
	bds_get_fcntl(rp, mi, cmd, arg, &x, sizeof(x), offset, cr);

static int
bds_get_fcntl(
	rnode_t		*rp,
	mntinfo_t	*mi,
	int		cmd,
	void		*arg,
	void		*cmdstruct,
	int		cmdstructsz,
	off_t		offset,
	cred_t		*cr)
{
	int error;
	
	if (!(mi->mi_flags & MIF_BDS) || bds_vers(rp, 3) < 2)
		return EINVAL;
	
	mutex_enter(&rp->r_statelock);
	error = bds_fcntl(rp, cmd, cmdstruct, offset, cr);
	mutex_exit(&rp->r_statelock);
	
	if (error)
		return error;
	if (copyout(cmdstruct, arg, cmdstructsz))
		return EFAULT;

	return 0;
}

#define BDS_SET_FCNTL(x) \
	bds_set_fcntl(rp, mi, cmd, arg, &x, sizeof(x), offset, cr);

static int
bds_set_fcntl(
	rnode_t		*rp,
	mntinfo_t	*mi,
	int		cmd,
	void		*arg,
	void		*cmdstruct,
	int		cmdstructsz,
	off_t		offset,
	cred_t		*cr)
{
	int error;
	
	if (!(mi->mi_flags & MIF_BDS) || bds_vers(rp, 3) < 2)
		return EINVAL;

	if (copyin(arg, cmdstruct, cmdstructsz))
		return EFAULT;
	
	mutex_enter(&rp->r_statelock);
	error = bds_fcntl(rp, cmd, cmdstruct, offset, cr);
	mutex_exit(&rp->r_statelock);

	return error;
}

/* ARGSUSED */
int
nfs3_fcntl(
	bhv_desc_t *bdp,
	int	cmd,
	void	*arg,
	int	flags,
	off_t	offset,
	cred_t	*cr,
	rval_t 	*rvp)
{
	int     		error = 0;
	struct flock            bf;
	struct irix5_flock      i5_bf;
	vnode_t			*vp = BHV_TO_VNODE(bdp);
	rnode_t			*rp = BHVTOR(bdp);
	mntinfo_t		*mi = RTOMI(rp);


	switch (cmd) {
	case F_ALLOCSP:
	case F_FREESP:
		
	case F_RESVSP:
	case F_UNRESVSP:
		
	case F_ALLOCSP64:
	case F_FREESP64:
		
	case F_RESVSP64:
	case F_UNRESVSP64:
		if ((flags & FWRITE) == 0) {
			error = EBADF;
		} else if (vp->v_type != VREG) {
			error = EINVAL;
		} else if (vp->v_flag & VISSWAP) {
			error = EACCES;
#if _MIPS_SIM == _ABI64
		} else if (ABI_IS_IRIX5_64(get_current_abi())) {
			if (copyin((caddr_t)arg, &bf, sizeof bf)) {
				error = EFAULT;
				break;
			}
#endif
		} else if (cmd == F_ALLOCSP64 || cmd == F_FREESP64   ||
			   cmd == F_RESVSP64  || cmd == F_UNRESVSP64 ||
			   ABI_IS_IRIX5_N32(get_current_abi())) {
			/*
			 * The n32 flock structure is the same size as the
		 	 * o32 flock64 structure. So the copyin_xlate
			 * with irix5_n32_to_flock works here.
			 */
			if (COPYIN_XLATE((caddr_t)arg, &bf, sizeof bf,
					 irix5_n32_to_flock,
					 get_current_abi(), 1)) {
				error = EFAULT;
				break;
			}
		} else {
			if (copyin((caddr_t)arg, &i5_bf, sizeof i5_bf)) {
				error = EFAULT;
				break;
			}
			/*
			 * Now expand to 64 bit sizes.
			 */
			bf.l_type = i5_bf.l_type;
			bf.l_whence = i5_bf.l_whence;
			bf.l_start = i5_bf.l_start;
			bf.l_len = i5_bf.l_len;
		}
		if ((mi->mi_flags & MIF_BDS) && bds_vers(rp, 3) >= 2) {
			mutex_enter(&rp->r_statelock);
			error = bds_fcntl(rp, cmd, &bf, offset, cr);
			mutex_exit(&rp->r_statelock);
			break;
		}
		if (cmd == F_RESVSP || cmd == F_UNRESVSP ||
		    cmd == F_RESVSP64 || cmd == F_UNRESVSP64)
		{
			error = EINVAL;
			break;
		}
		if ((error = convoff(vp, &bf, 0, offset, SEEKLIMIT32, cr)) == 0) {
			struct vattr vattr;
			vattr.va_size = bf.l_start;
			vattr.va_mask = AT_SIZE;
			error = nfs3_setattr(bdp, &vattr, 0, cr);
		}
		break;
	case F_BDS_FSOPERATIONS + XFS_GROWFS_DATA: {
		xfs_growfs_data_t grow;

		error = BDS_SET_FCNTL(grow);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GROWFS_LOG: {
		xfs_growfs_log_t grow;

		error = BDS_SET_FCNTL(grow);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GROWFS_RT: {
		xfs_growfs_rt_t grow;

		error = BDS_SET_FCNTL(grow);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_COUNTS: {
		xfs_fsop_counts_t counts;

		error = BDS_GET_FCNTL(counts);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_SET_RESBLKS: {
		xfs_fsops_getblks_t getblks;

		if (!(mi->mi_flags & MIF_BDS) || bds_vers(rp, 3) < 2) {
			error = EINVAL;
			break;
		}
		if (copyin(arg, &getblks, sizeof(getblks))) {
			error = EFAULT;
			break;
		}
		mutex_enter(&rp->r_statelock);
		error = bds_fcntl(rp, cmd, &getblks, offset, cr);
		mutex_exit(&rp->r_statelock);

		if (error == 0 && copyout(&getblks, arg, sizeof(getblks)))
			error = EFAULT;
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GET_RESBLKS: {
		xfs_fsops_getblks_t getblks;

		error = BDS_GET_FCNTL(getblks);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY_V1: {
		xfs_fsop_geom_v1_t geom;

		error = BDS_GET_FCNTL(geom);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY_V2: {
		xfs_fsop_geom_v2_t geom;

		error = BDS_GET_FCNTL(geom);
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY: {
		xfs_fsop_geom_t geom;

		error = BDS_GET_FCNTL(geom);
		break;
	}
	case F_FSGETXATTR:
	case F_FSGETXATTRA: {
		struct fsxattr fa;

		error = BDS_GET_FCNTL(fa);
		break;
	}
	case F_FSSETXATTR: {
		struct fsxattr fa;

		error = BDS_SET_FCNTL(fa);
		break;
	}
	case F_DIOINFO: {
		struct dioattr	da;
		int vers;
		
		if (!(mi->mi_flags & MIF_BDS) || (vers = bds_vers(rp, 3)) == -1) {
			error = EINVAL;
			break;
		}
		if (vers == 0)
			da.d_miniosz = 4096; /* BDS 1.0 */
		else
			da.d_miniosz = 1;
		da.d_mem     = _PAGESZ;
		da.d_maxiosz = mi->mi_bds_maxiosz;

		if (copyout(&da, arg, sizeof(da))) {
			error = EFAULT;
		}
		break;
	}
	case F_GETBDSATTR: {
		bdsattr_t ba;

		error = BDS_GET_FCNTL(ba);
		break;
	}
	case F_SETBDSATTR: {
		bdsattr_t ba;

		error = BDS_SET_FCNTL(ba);
		break;
	}
	default:
		error = EINVAL;
	}								

	return error;
}


/*
 * This is referenced in sgi/chunkio.c to special-case commits.
 * A function so that it can be stubbed out in a master.d file.
 */
vnodeops_t *
nfs3_getvnodeops(void)
{

	return (&nfs3_vnodeops);
}

/* ARGSUSED */
static int
nfs3_open(bhv_desc_t *bdp, vnode_t **vpp, mode_t flag, struct cred *cr)
{
	int error = 0;
	struct vattr va;
	rnode_t *rp;

	rp = BHVTOR(bdp);

	/*
	 * If we can't support BDS, return error if FDIRECT was set.
	 */
	if ((flag & FDIRECT) && !(rtomi(rp)->mi_flags & MIF_BDS))
		return EINVAL;

	mutex_enter(&rp->r_statelock);
	if (rp->r_cred == NULL) {
		crhold(cr);
		rp->r_cred = cr;
	}
	mutex_exit(&rp->r_statelock);

	/*
	 * If there is no cached data or if close-to-open
	 * consistancy checking is turned off, we can avoid
	 * the over the wire getattr.  Otherwise, force a
	 * call to the server to get fresh attributes and to
	 * check caches. This is required for close-to-open
	 * consistency.
	 */
	va.va_mask = AT_ALL;
	error = nfs3_getattr_otw(bdp, &va, cr);

	return (error);
}

/* ARGSUSED */
static int
nfs3_close(bhv_desc_t *bdp, int flag, lastclose_t lastclose, 
	   struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register rnode_t *rp;
	int error;

	ASSERT(vp->v_count > 0);
	if (lastclose == L_FALSE)
		return (0);

	rp = BHVTOR(bdp);

	ASSERT(!haslocks(vp, current_pid(), (sysid_t)0));
	if (!(rtomi(rp)->mi_flags & MIF_PRIVATE) && (rp->r_flags & RCLEANLOCKS)) {
		(void)nfs3_cleanlocks(bdp, current_pid(), 0, cr);
	}

	if (rp->r_bds) {
		int bdserror = 0;
		
		mutex_enter(&rp->r_statelock);
		if (rp->r_bds_flags & RBDS_DIRTY)
			bdserror = bds_sync(rp, cr);
		mutex_exit(&rp->r_statelock);
		if (bdserror && rp->r_error == 0)
			rp->r_error = bdserror;
	}
	
	/*
	 * If the file is an unlinked file, then flush the lookup
	 * cache so that nfs3_inactive will be called if this is
	 * the last reference.  It will invalidate all of the
	 * cached pages, without writing them out.  Writing them
	 * out is not required because they will be written to a
	 * file which will be immediately removed.
	 */
	if (rp->r_unldrp != NULL) {
		
		mutex_enter(&rp->r_statelock);
		INVAL_ATTRCACHE(bdp);
		mutex_exit(&rp->r_statelock);

		VOP_INVALFREE_PAGES(vp, rp->r_localsize);
		nfs3_riowait(rp);
		dnlc_purge_vp(vp);
		error = rp->r_error;
		rp->r_error = 0;
		return (error);
	}
	if (VN_MAPPED(vp)) {
		ASSERT(vp->v_count > 1);
		    ;
	} else {
		if (vp->v_type != VLNK && rp->r_unldrp != NULL || rp->r_error) {
			VOP_INVALFREE_PAGES(vp, rp->r_localsize);
			dnlc_purge_vp(vp);
		}
		nfs3_riowait(rp);
	}
	if ((flag & FWRITE) && (rp->r_localsize > 0)) {
		/*
		 * Do not pass B_STALE otherwise when write completes, 
		 * buffer will be released
		 */
		VOP_FLUSH_PAGES(vp, (off_t)0, rp->r_localsize - 1, B_ASYNC, FI_NONE, error);
		nfs3_riowait(rp);
		if (commit_file(rp, cr) != 0) {
		/*
		 * Here call into chunk cache to lock the buffers
		 * for the file and call VOP_COMMIT, nfs3_async_commit().
		 * Those commits will fail, too, but in doing so
		 * forces the cache to re-write the buffers SYNC.
		 */
			VOP_FLUSH_PAGES(rtov(rp), (off_t)0, 
			    rp->r_localsize - 1, B_STALE, FI_NONE, error);
		}

		/*
		 * Now that all outstanding I/O is finished, clear the dirty
		 * flag so that if the file size changes on the server side
		 * we listen to that size and not our own anymore.
		 */
		rclrflag(rp, RDIRTY);
	} else
		error = 0;

	/*
	 * If no error was recorded above, then return any errors
	 * which may have occurred on asynchronous writes.
         */
	if (!error)
		error = rp->r_error;
	rp->r_error = 0;
	return (error);
}

int nfs3_putpage = 0;
/* ARGSUSED */
static int
nfs3_read(bhv_desc_t *bdp, struct uio *uiop, int ioflag, cred_t *cr, flid_t *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int 	error;
	if (vp->v_type != VREG)
		return (EISDIR);

	if (!(ioflag & IO_ISLOCKED))
		nfs_rwlock(bdp, VRWLOCK_READ);

	if ((ioflag & IO_RSYNC) && (ioflag & (IO_SYNC | IO_DSYNC))) {
		if (!(IS_SWAPVP(vp))) {
			nfs3_putpage++;
		}
	}
	error = rw3vp(bdp, uiop, UIO_READ, ioflag, cr, fl);

	if (!(ioflag & IO_ISLOCKED))
		nfs_rwunlock(bdp, VRWLOCK_READ);

	return error;
}

/*ARGSUSED*/
static int
nfs3_write(bhv_desc_t *bdp, struct uio *uiop, int ioflag, cred_t *cr, flid_t *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int	error;

	if (vp->v_type != VREG)
		return (EISDIR);

	if (!(ioflag & IO_ISLOCKED))
		nfs_rwlock(bdp, VRWLOCK_WRITE);

	if (ioflag & IO_APPEND) {
		struct vattr va;

		va.va_mask = AT_SIZE;
		error = nfs3getattr(bdp, &va, 0, cr);
		if (error)
			goto out;
		uiop->uio_offset = va.va_size;
	}

	error = rw3vp(bdp, uiop, UIO_WRITE, ioflag, cr, fl);

 out:
	if (!(ioflag & IO_ISLOCKED))
		nfs_rwunlock(bdp, VRWLOCK_WRITE);

	return error;
}

#define ROUNDUP(a, b)	((((a) + (b) - 1) / (b)) * (b))
int ethan_do_bmap_dontalloc = 1;

/* ARGSUSED */
static int
rw3vp(bhv_desc_t *bdp,
	struct uio *uio,
	enum uio_rw rw,
	int ioflag,
	cred_t *cr,
	flid_t *fl)
{
	rnode_t *rp = BHVTOR(bdp);
	vnode_t *vp = BHV_TO_VNODE(bdp);
	off_t off;
	caddr_t base;
	off_t remainder;
	register int on;
	off_t n;
	int error = 0;
	int eof = 0;
	long long resid = uio->uio_resid;
	off_t offset = uio->uio_offset;
	mntinfo_t *mi = RTOMI(rp);
	int readahead_blocks = 0;
	struct bmapval bmv[MAX_READ_BLOCKS];
	int size, len;
	daddr_t bn;
	struct buf *bp;
	int nmaps, i;
	int degraded;
#ifdef KTRACE
	off_t save_offset;
#endif
	post_op_attr fa;

	ASSERT(rw == UIO_READ || rw == UIO_WRITE);
	ASSERT(mi);

	if (uio->uio_resid == 0)
		return (0);

	if (uio->uio_offset > MAXOFF_T)
		return (EFBIG);

	/*
	 * To use bds, bds mount option must be set. Then, either
	 * FDIRECT needs to be set, or this needs to be "auto" bds.
	 * auto is used if:
	 * * FBULK is set, and access is larger than BDS_BULK_AUTO
	 * * or the bdsauto mount option is set, and we're larger than the
	 *   user specified minimum size
	 * * not mapped
	 * * old-style XFS DIRECT alignment restriction must be met
	 *   unless this is BDS 2.0 or better
	 */
	if (!(mi->mi_flags & MIF_BDS))
		goto rw3vp_nobds;
	
	if ((ioflag & IO_DIRECT) ||			/* explicit BDS */
	    ((((ioflag & IO_BULK) && (BDS_BULK_AUTO <= uio->uio_resid))
	      || (mi->mi_bdsauto && (mi->mi_bdsauto <= uio->uio_resid)))
	     && !VN_MAPPED(vp)))			/* executables */
	{
		/*
		 * BDS 1.[01] does not allow unaligned accesses, so
		 * if the access is unaligned, do not allow an "auto"
		 * BDS access unless we've got BDS 2.0 or better
		 */
		if (!(ioflag & IO_DIRECT) &&
		    ((uio->uio_resid & 4095) != 0 ||
		     (uio->uio_offset & 4095) != 0) &&
		    (bds_vers(rp, 3) <= 0))
		{
			goto rw3vp_nobds;
		}

		/*
		 * Don't do bdsauto if the request is too large for BDS.
		 */

		if (!(ioflag & IO_DIRECT) &&
		    (bds_vers(rp, 3) < 0 ||
		     uio->uio_resid > mi->mi_bds_maxiosz))
		{
			goto rw3vp_nobds;
		}

		/*
		 * Nuke any cached pages.
		 */
		if (rw == UIO_WRITE && VN_CACHED(vp)) {
			nfs3_fsync(bdp, FSYNC_INVAL, cr, (off_t)0, (off_t)-1);
		}

		mutex_enter(&rp->r_statelock);
		if (rw == UIO_READ)
			error = bds_read (rp, uio, ioflag, cr);
		else {
			error = bds_write(rp, uio, ioflag, cr);
			INVAL_ATTRCACHE(bdp);
		}
		mutex_exit(&rp->r_statelock);
		return (error);
	}
rw3vp_nobds:
	error = nfs3_validate_caches(bdp, cr);
	if (error)
		return(error);

	/*
	 * if writes have gone through BDS, and BDS is dirty from writebehinds,
	 * but now we're using NFS, do a sync to ensure consistency. To avoid
	 * doing a mutex_enter if we don't have to, we check for these conditions
	 * both before and after locking.
	 */
	if (rw == UIO_WRITE && rp->r_bds && (rp->r_bds_flags & RBDS_DIRTY)) {
		mutex_enter(&rp->r_statelock);
		if (rp->r_bds && rp->r_bds_flags & RBDS_DIRTY)
			error = bds_sync(rp, cr);
		mutex_exit(&rp->r_statelock);
		if (error)
			return error;
	}
		
	off = uio->uio_offset + uio->uio_resid;
	if (uio->uio_offset < 0)
		return (EINVAL);

#ifdef KTRACE
	PIDTRACE2(T_RW3VP, T_START, (rw == UIO_READ) ? RFS_READ : RFS_WRITE,
		save_offset = uio->uio_offset);
#endif /* KTRACE  */

	size = rtoblksz(rp);
	ASSERT(size > 0);
	len = BTOBBT(size);

	/*
	 * If we hold a file lock, set IO_DIRECT if the lock does not
	 * span the buffer(s) touched by the I/O.
	 * We don't support mantatory file locking, so we don't bother
	 * with checking for it.
	 */
	if (vp->v_flag & VFRLOCKS) {
		if ((rw == UIO_WRITE) && (uio->uio_segflg != UIO_SYSSPACE)) {
		    flock_t ld;

		    /*
		     * See if this process has a file lock which spans the
		     * blocks which will be modified by this write.  If so,
		     * we can go through the buffer cache to do the write,
		     * otherwise, we must set IO_DIRECT.
		     */
		    ld.l_start = (uio->uio_offset / size) * size;
		    ld.l_len = (((uio->uio_offset + uio->uio_resid +
			    size - 1) / size) * size) - ld.l_start;
		    ld.l_pid = curuthread->ut_flid.fl_pid;
		    ld.l_whence = 0;
		    ld.l_type = F_WRLCK;
		    ld.l_sysid = 0;
		    if (!haslock(vp, &ld)) {
			ioflag |= IO_DIRECT;
			VOP_FLUSHINVAL_PAGES(vp,
				(uio->uio_offset / size) * size,
				MIN((((uio->uio_offset + uio->uio_resid +
					size - 1) / size) * size),
					rp->r_localsize) - 1, FI_REMAPF_LOCKED);
		    }
		}
	}
	/*
	 * If this is a loopback mount of a local file system, we must
	 * avoid doing the write through the buffer cache, thus making
	 * the buffer dirty and in need of flushing.  To accomplish
	 * this, we set the IO_SYNC flag.  This relies on the code below
	 * which calls nfswrite directly for IO_SYNC rather than using
	 * bwrite.
	 */
	if (rtomi(rp)->mi_flags & MIF_LOCALLOOP) {
		ioflag |= IO_SYNC;
	}

	/*
	 * Check to make sure that the process will not exceed
	 * its limit on file size.  It is okay to write up to
	 * the limit, but not beyond.  Thus, the write which
	 * reaches the limit will be short and the next write
	 * will return an error.
	 */
	remainder = 0;
	if (rw == UIO_WRITE && vp->v_type == VREG &&
	    uio->uio_offset + uio->uio_resid > uio->uio_limit) {
		remainder = uio->uio_offset + uio->uio_resid - uio->uio_limit;
		uio->uio_resid = uio->uio_limit - uio->uio_offset;
		if (uio->uio_resid <= 0) {
			uio->uio_resid += remainder;
#ifdef KTRACE
			PIDTRACE2(T_RW3VP, T_EXIT_OK,
			    (rw == UIO_READ) ? RFS_READ : RFS_WRITE,
			    save_offset);
#endif /* KTRACE  */
			return (EFBIG);
		}
	}

	readahead_blocks = mi->mi_rdahead;

	do {
		if (ioflag & IO_DIRECT) {
		    ASSERT(!(vp->v_flag & VISSWAP) || (poff(uio->uio_offset) == 0));
		    off = 0;
		    bn = -1;
		    n = MIN(size, uio->uio_resid);
		} else {
		    off = uio->uio_offset % size; /* mapping offset */
		    n = MIN((offset3)(size - off), uio->uio_resid);
		    bn = OFFTOBBT(uio->uio_offset - off);
		}
		bmv[0].bn = bn;
		bmv[0].offset = bn;
		bmv[0].length = len;
		bmv[0].eof = 0; /* conservative */
		bmv[0].pboff = off;
		bmv[0].bsize = size;
		bmv[0].pbsize = size;
		bmv[0].pbdev = vp->v_rdev;
		bmv[0].pmp = uio->uio_pmp;
		if (ioflag & IO_DIRECT) {
		    /*
		     * file lock file - normal read/write
		     * or swapin in/out
		     */

		    /*
		     * We do not currently support direct I/O from user
		     * programs.  The only time IO_DIRECT will be set when
		     * uio_segflg is UIO_USERSPACE is when the I/O is a
		     * write and a file lock is held on the file.
		     */
		    ASSERT((uio->uio_segflg != UIO_USERSPACE) ||
			(rw == UIO_WRITE));

		    if (uio->uio_segflg != UIO_USERSPACE) {
			bp = ngeteblk(0);
			bp->b_un.b_addr = uio->uio_iov->iov_base;
			if (!(ioflag & IO_PFUSE_SAFE))
				bp->b_flags |= B_PRIVATE_B_ADDR;
		    } else {
			bp = ngeteblk(size);
		    }
		     if (rw == UIO_READ) {
			    off_t diff;

			    error = nfs3read(bdp, bp, offset, (long)n,
				    (unsigned *)&bp->b_resid, cr, &fa, 0);
			    if (error) {
					bp->b_flags |= B_ERROR;
					bp->b_error = error;
				    brelse(bp);
				    break;
			    }
			    offset += n;
			    rp->r_lastr = bn;
			    diff = rp->r_size - uio->uio_offset;
			    if (diff <= 0) {
				    brelse(bp);
				    break;
			    }
			    if (diff < n) {
				    n = diff;
				    eof = 1;
			    }
		     }
		} else if (rw == UIO_READ) {
			off_t diff;
			off_t	maxlocalsize;
			daddr_t r_size_bn = OFFTOBBT(rp->r_size - 1);

			if (ethan_do_bmap_dontalloc)
				bmv[0].eof |= BMAP_DONTALLOC;
			diff = rp->r_size - uio->uio_offset;
			if (diff <= 0) 
				break;
			if (diff < n) {
				n = diff;
				eof = 1;
				bmv[0].pbsize = n;
				readahead_blocks = 0;
			} else if (readahead_blocks < 0 || rp->r_lastr + len != bn)
				readahead_blocks = 0;
			else if (readahead_blocks > (MAX_READ_BLOCKS - 1))
				readahead_blocks = MAX_READ_BLOCKS - 1;

			for (i = 1; i <= readahead_blocks; i++) {
				bmv[i].bn = bmv[i].offset = bmv[i-1].bn + len;
				if (bmv[i].bn > r_size_bn)
					break;
				bmv[i].length = len;
				bmv[i].eof = ethan_do_bmap_dontalloc ? BMAP_DONTALLOC : 0;
				bmv[i].pboff = 0;
				bmv[i].bsize = size;
				bmv[i].pbsize = size;
				bmv[i].pbdev = bmv[0].pbdev;
				bmv[i].pmp = uio->uio_pmp;
			}
			nmaps = i;

			rp->r_lastr = bn;
			maxlocalsize = BBTOB(bn) + (nmaps * size);
			if (rp->r_localsize < maxlocalsize)
				rp->r_localsize = maxlocalsize;
			bp = chunkread(vp, bmv, nmaps, cr);
			ASSERT(bp->b_edev != NODEV);
		} else {    /* UIO_WRITE */
			if ((rp->r_flags & RDONTWRITE) 
			    || (rp->r_error == ENOSPC)) {
				error = rp->r_error;
				break;
			}
			if (rp->r_localsize < BBTOB(bn) + size)
				    rp->r_localsize = BBTOB(bn) + size;

			if ( (uio->uio_offset >= ROUNDUP(rp->r_size, size)) ||
				((off == 0) && ((n == size) ||
				((uio->uio_offset + n) >= rp->r_size))) ) {
				/*
				 * Skip the unnecessary read and bzero() on
				 * the portion of the file being overwritten.
				 * Must still bzero() the unwritten portions
				 * of new buffers.
				 * The n==size case requires no bzero().
				 */
				bp = getchunk(vp, bmv, cr);
				ASSERT(bp->b_edev != NODEV);

				if (!(bp->b_flags & B_ERROR) && n != size) {
					int end = off + n;

					(void) bp_mapin(bp);
					ASSERT(off >= 0 && off < size);
					ASSERT(n > 0 && n <= size);
					ASSERT(end <= size);
					if (off != 0)
						bzero(bp->b_un.b_addr, off);
					if (end < size)
						bzero(bp->b_un.b_addr + end,
						    size - end);
				}
			} else {
				bp = chunkread(vp, bmv, 1, cr);
				ASSERT(bp->b_edev != NODEV);
			}
			/*
			 * The reference port did some fancy work
			 * here to avoid using the page cache if
			 * the file was marked VNOCACHE, but
			 * our makedepend stuff forces us to use
			 * the buffer cache.
			 */
		}

		/*
		 * Check for a read error.  If there was one,
		 * return the error code.
		 */
		if (bp->b_flags & B_ERROR) {
			
			error = bp->b_error;
			if (!error)
				error = EIO;
			brelse(bp);
			break;
		}

#ifdef _VCE_AVOIDANCE
		if (!vce_avoidance)
#endif /* _VCE_AVOIDANCE */
		{
		(void) bp_mapin(bp);
		base = bp->b_un.b_addr;
		}
		on = bmv[0].pboff;

#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			error = biomove(bp, on, n, rw, uio);
			if (!error) {
				(void) bp_mapin(bp);
				base = bp->b_un.b_addr;
			}
		} else
#endif /* _VCE_AVOIDANCE */
		error = uiomove(base + on, n, rw, uio);
		if (rw == UIO_READ) {
			brelse(bp);
		} else {
			ASSERT((on + n) <= size);
					
			if (!error) {
				mutex_enter(&rp->r_statelock);
				/*
				 * r_size is the maximum number of
				 * bytes known to be in the file.
				 * Make sure it is at least as high as the
				 * last byte we just wrote into the buffer.
				 */
				if (rp->r_size < uio->uio_offset)
					rp->r_size = uio->uio_offset;
				rsetflag(rp, RDIRTY);
				mutex_exit(&rp->r_statelock);

				/*
				 * Threshold used to decide if we start 
				 * sending data synchronously.
				 */
				degraded = (b_unstablecnt > (nbuf * nfs3_unst_wtmark / 100));

				if ((ioflag & (IO_DIRECT|IO_SYNC)) ||
				    (uio->uio_fmode & (FSYNC|FDSYNC)) ||
				    ((degraded) && !(bp->b_flags & (B_NFS_ASYNC | B_NFS_UNSTABLE))))
				{
					/* 
					 * In degraded mode, the current buffer was not used 
					 * previously for a delayed write.
					 */ 
					stable_how stab_comm = FILE_SYNC;
					error = nfs3write(bdp,
						  bp->b_un.b_addr + off, offset,
						  n, cr, &stab_comm, 0, bp);
					bp->b_flags &= ~B_ASYNC;
					bp->b_flags |= B_DONE;
					brelse(bp);
					offset += n;
				} else if (!async_bio || !mi->mi_max_threads ||
					   !mi->mi_threads ||
					   (mi->mi_flags & MIF_NO_SERVER)) {
					if (bp->b_flags & (B_NFS_ASYNC | B_NFS_UNSTABLE))
						atomicAddInt(&b_unstablecnt, -1);
					bp->b_flags &=
						~(B_ASYNC|B_NFS_ASYNC|B_NFS_UNSTABLE|B_NFS_RETRY);
					bwrite(bp);
				} else if ((off + n < size) ||
				    (rtomi(rp)->mi_flags & MIF_PRIVATE)) {
					if ((bp->b_flags & (B_NFS_ASYNC|B_NFS_UNSTABLE)) == 0)
						atomicAddInt(&b_unstablecnt, 1);
					bp->b_flags |= B_NFS_ASYNC;
					bp->b_flags &= ~(B_NFS_UNSTABLE|B_NFS_RETRY);
					bdwrite(bp);
				} else {
					if ((bp->b_flags & (B_NFS_ASYNC|B_NFS_UNSTABLE)) == 0)
						atomicAddInt(&b_unstablecnt, 1);
					bp->b_flags |= B_AGE|B_NFS_ASYNC;
					bp->b_flags &= ~(B_NFS_UNSTABLE|B_NFS_RETRY);
					bawrite(bp);
				}
				/*
				 * Errors after the uiomove should be
				 * reflected in the uio structure.
				 */
				if (error) {
					uioupdate(uio, -n);
				}
			} else {
				/*
				 * If we used getchunk() earlier, the buffer contains
				 * garbage and must be nuked.
				 */
				bp->b_flags &= ~(B_ASYNC|B_NFS_ASYNC|
						 B_NFS_UNSTABLE|B_NFS_RETRY);
				bp->b_flags |= B_DONE|B_ERROR|B_STALE;
				brelse(bp);
			}
		}

	} while (error == 0 && uio->uio_resid > 0 && !eof);

	if (error == EINTR &&
	    rw == UIO_WRITE &&
	    ((ioflag & IO_SYNC) || (ioflag & IO_DSYNC) || degraded)) {
		uio->uio_resid = resid;
		uio->uio_offset = offset;
	} else {
		uio->uio_resid += remainder;
		if (error && rw == UIO_WRITE && uio->uio_resid != resid)
			error = 0;
	}

#ifdef KTRACE
	PIDTRACE2(T_RW3VP, T_EXIT_OK,
		    (rw == UIO_READ) ? RFS_READ : RFS_WRITE, save_offset);
#endif /* KTRACE  */
	return (error);
}


static int nfs3_do_sync_writes = 0;

/*
 * Special attr update test for delayed writes.  Check if these
 * are the latest modified attrs.
 */
static int
latestmod(struct rnode *rp, struct fattr3 *na)
{
	if ((na->mtime.seconds > rp->r_attr.va_mtime.tv_sec) ||
	    ((na->mtime.seconds == rp->r_attr.va_mtime.tv_sec) &&
	    (na->mtime.nseconds > rp->r_attr.va_mtime.tv_nsec)))
		return(1);
	return(0);
}

/*
 * Write to file.  Writes to remote server in largest size
 * chunks that the server can handle.
 */
#ifdef NFSCACHEDEBUG
int good_nfs3_write_ok=0;
int good_nfs3_write_bad=0;
#endif /* NFSCACHEDEBUG */

static int
nfs3write(bhv_desc_t *bdp, caddr_t base, off_t offset, long count, cred_t *cr,
	stable_how *stab_comm, int frombio, struct buf *bp)
{
	mntinfo_t *mi;
	WRITE3args args;
	WRITE3res res;
	int error;
	int tsize;
	int douprintf;
	stable_how stable;
	rnode_t *rp;
	long vcount;
	off_t voffset = -1;
	writeverf3 verf, *verfp = 0;
	commit_t *cp;

	ASSERT(count > 0);

	rp = BHVTOR(bdp);
	mi = RTOMI(rp);
	cp = &rp->r_commit;
	douprintf = 1;

	if (nfs3_do_sync_writes)
		stable = FILE_SYNC;
	else
		stable = *stab_comm;

	/*
	 * If we're getting close to using all unstable buffers, try to get a
	 * background commit going to clear things up. b_unstablecnt tracks the
	 * number of buffers which are NFS UNSTABLE or ASYNC. nbuf is the total
	 * number of buffers in the system. We cannot let all buffers become
	 * b_unstable since that could starve the system of buffers if the NFS
	 * server(s) are down or slow.
	 *
	 * To control this, we try to commit a portion of the rnode's
	 * uncommited range asynchronously, and then find and "stabilize" all
	 * buffers in the range. Here we do that if b_unstablecnt is getting
	 * dangerous and this rnode's unstable region is contributing. The
	 * exact heuristics are open to change.
	 *
	 * verf_update() also calls nfs3_background_commit periodically, based
	 * upon the time since our last nfs3_background_commit.
	 */
	
	if (cp->c_flags == COMMIT_UNSTABLE && b_unstablecnt > (9 * nbuf * nfs3_unst_wtmark / 1000)) {
		mutex_enter(&cp->c_mutex);
		if ((cp->c_flags & COMMIT_UNSTABLE) && (cp->c_count > rtoblksz(rp)))
			nfs3_background_commit(rp, cp);
		mutex_exit(&cp->c_mutex);
	}

	ASSERT(BHV_TO_VNODE(bdp));
	ASSERT(BHVTOR(bdp));
	ASSERT(rtofh3(BHVTOR(bdp)));
	args.file = *BHVTOFH3(bdp);

	error = raccess(rp);
	if (error) {
		return(error);
	}
	do {
		if (verfp) { /* only do this copy before subsequent requests */
			bcopy(verfp, &verf, sizeof (verf));
			verfp = 0;
		}
		tsize = MIN(mi->mi_stsize, count);
		args.offset = (offset3)offset;
		args.count = (count3)tsize;
		args.stable = stable;
		args.data.data_len = (u_int)tsize;
		args.data.data_val = base;
		args.data.putbuf_ok = !(bp->b_flags & B_PRIVATE_B_ADDR);
		error = rfs3call(mi, NFSPROC3_WRITE,
				xdr_WRITE3args, (caddr_t)&args,
				xdr_WRITE3res, (caddr_t)&res,
				cr, &douprintf, &res.status, frombio);
		if (error == ENFS_TRYAGAIN) {
			error = 0;
			continue;
		}
		if (error) {
			runlock(rp);
			return (error);
		}
		error = geterrno3(res.status);
		if (!error) {
			if ((int)res.resok.count > tsize) {
				cmn_err(CE_WARN,
				"nfs3write: server wrote %d, requested was %d",
					(int)res.resok.count, tsize);
				runlock(rp);
				return (EIO);
			}
			tsize = (int)res.resok.count;
			if (res.resok.committed == UNSTABLE) {
				if (args.stable != UNSTABLE) {
					cmn_err(CE_WARN,
			"nfs3write: server %s did not commit to stable storage",
						mi->mi_hostname);
					runlock(rp);
					return (EIO);
				}
				/*
				 * Record the initial offset and verf and the
				 * total count to pass to the verf_update().
				 */
				if (voffset == -1) {
					voffset = offset;
					verfp = &res.resok.verf;
				}
				vcount = offset + tsize - voffset;
			}
			count -= tsize;
			base += tsize;
			offset += tsize;
		} 
	} while (!error && (count > 0));


	if (res.status == NFS3_OK) {
		if (res.resok.file_wcc.after.attributes && (!frombio ||
		   latestmod(rp, &res.resok.file_wcc.after.attr))) {
			if (nfs3_attrcache(bdp, 
					&res.resok.file_wcc.after.attr,
					NOFLUSH, NULL)) {
			      PURGE_ATTRCACHE(bdp);
#ifdef NFSCACHEDEBUG
			      printf("bad cache nfs3_write(ok, %d)\n",
					good_nfs3_write_ok);
				good_nfs3_write_ok=0;
			} else {
				good_nfs3_write_ok++;
#endif /* NFSCACHEDEBUG */
			}
		}
	} else {
		PURGE_ATTRCACHE(bdp);
		if (res.resfail.file_wcc.after.attributes && (!frombio ||
		    latestmod(rp, &res.resfail.file_wcc.after.attr))) {
			if (nfs3_attrcache(bdp, 
				&res.resfail.file_wcc.after.attr,
				NOFLUSH, NULL)) {
#ifdef NFSCACHEDEBUG
			      printf("bad cache nfs3_write(bad, %d)\n",
					good_nfs3_write_bad);
				good_nfs3_write_bad=0;
			} else {
				good_nfs3_write_bad++;
#endif /* NFSCACHEDEBUG */
			}
		}
	}
	runlock(rp);

	/*
	 * Perform the real verf update.  Use bp to indicate the range
	 * to exclude from the flush to prevent deadlocks.
	 *
	 * If the last res.status (reflected in errno) is a failure, we
	 * skip this check because the caller will set b_error.  There
	 * is no need to commit a buffer with an error in it.
	 *
	 * Call verf_update() with the initial verf.  If iterations of the
	 * above loop give different verfs (rare), we cannot immediately
	 * flush this buffer (since we already have it locked).  Any
	 * subsequent write/commit of the file will eventually detect the
	 * new verf so this buffer can then be flushed.
	 */
	if (!error && voffset != -1) {
		if (!verfp)
			verfp = &verf;
		verf_update(rp, voffset, vcount, verfp, bp, cr);
	}
	return (error);
}

/*
 * Read from a file.  Reads data in largest chunks our interface can handle.
 */
static int
nfs3read(bhv_desc_t *bdp, buf_t *bp, off_t offset, long count,
	unsigned *residp, cred_t *cr, post_op_attr *fap, int frombio)
{
	mntinfo_t *mi;
	READ3args args;
	READ3res res;
	register int tsize;
	int error;
	int rpcerror;
	int douprintf;
	caddr_t base;

	base = bp->b_un.b_addr;	/* NULL if BMAP_DONTALLOC */
	mi = VN_BHVTOMI(bdp);
	douprintf = 1;
	if (!count) {
		return (EINVAL);
	}
	do {
		do {
			tsize = MIN(mi->mi_tsize, count);
			res.resok.data.data_val = base;
			res.resok.data.data_len = tsize;
			res.resok.bp = bp;
			args.file = *BHVTOFH3(bdp);
			args.offset = (offset3)offset;
			args.count = (count3)tsize;
			rpcerror = rfs3call(mi, NFSPROC3_READ,
					xdr_READ3args, (caddr_t)&args,
					xdr_READ3res, (caddr_t)&res,
					cr, &douprintf, &res.status, frombio);
		} while (rpcerror == ENFS_TRYAGAIN);

		if (!rpcerror) {
			error = geterrno3(res.status);
			if (!error) {
				if (res.resok.count > 0) {
					count -= res.resok.count;
					/* if partial read, b_addr must be set */
					if (count) {
						if (base == NULL)
							base = bp->b_un.b_addr;
						ASSERT(base);
						if (base)
							base += res.resok.count;
					}
					offset += res.resok.count;
				} else {
					/*
					 * We should clean up the buf structure
					 * associated with the zero read
					 */
				}
			}
		}
	} while (!rpcerror && !error && count && !res.resok.eof);

	*residp = (unsigned)count;

	if (!rpcerror) {
		if (!error)
			*fap = res.resok.file_attributes;
	} else
		error = rpcerror;

	return (error);
}

/*ARGSUSED*/
static int
nfs3_ioctl(bhv_desc_t *bdp, int com, void *arg, int flag, struct cred *cr, 
	   int *rvalp, struct vopbd *vbds)
{

	return (ENOTTY);
}

#ifdef NFSCACHEDEBUG
int	nfs3_getattrflushcnt = 0;
int	nfs3_setattrflushcnt = 0;
#endif /* NFSCACHEDEBUG */
/* ARGSUSED */
static int
nfs3_getattr(bhv_desc_t *bdp, struct vattr *vap, int flags, cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	rnode_t *rp;
	long	 mask = vap->va_mask;
	int 	 error;

	/*
	 * Only need to flush pages if asking for the mtime
	 * and if there any dirty pages.  Since we don't know
	 * whether there are any dirty pages if the file is
	 * mmap'd, then flush just to be safe.
	 */
	rp = BHVTOR(bdp);
	if (vap->va_mask & AT_MTIME) {
		if ((rp->r_flags & RDIRTY) ||
		    rp->r_iocount > 0 ||
		    VN_MAPPED(vp)) {
			/* sun called nfs3_putpage, but we should probably
			 * call something like pflushvp
			 */
#ifdef NFSCACHEDEBUG
			nfs3_getattrflushcnt++;
#endif /* NFSCACHEDEBUG */
			VOP_FLUSH_PAGES(vp, (off_t)0, rp->r_localsize - 1, B_ASYNC, FI_NONE, error);
		}
	}

	error = nfs3getattr(bdp, vap, flags, cr);
	vap->va_mask = mask;
	return (error);
}

static int
nfs3_setattr(bhv_desc_t *bdp, struct vattr *vap, int flags, cred_t *cr)
{
	int error;
	long mask;
	rnode_t *rp = BHVTOR(bdp);
	vnode_t *vp;
	uid_t fuid = rp->r_attr.va_uid;
	gid_t fgid = rp->r_attr.va_gid;
	mode_t fmode = rp->r_attr.va_mode;

	if (_MAC_NFS_IACCESS(bdp, VWRITE, cr)) {
		return(EACCES);
	}

	mask = vap->va_mask;
	vp = BHV_TO_VNODE(bdp);

	if (mask & AT_NOSET)
		return (EINVAL);
	
	if (flags & ATTR_LAZY) {
		return 0;       /* dont bother going out with this */
	}

	if (mask & (AT_ATIME | AT_MTIME)) {
		if (cr->cr_uid != fuid && !_CAP_CRABLE(cr, CAP_FOWNER)) {
			if (flags & ATTR_UTIME) {
				return (EPERM);
			}
			_NFS_IACCESS(bdp, VWRITE, cr);
		}
	}

	if (mask & (AT_MODE | AT_XFLAGS | AT_EXTSIZE | AT_UID | AT_GID)) {
		if (cr->cr_uid != fuid && !_CAP_CRABLE(cr, CAP_FOWNER)) {
			return (EPERM);
		}
	}

	if (mask & (AT_UID | AT_GID)) {
		uid_t uid = (mask & AT_UID) ? vap->va_uid : fuid;
		gid_t gid = (mask & AT_GID) ? vap->va_gid : fgid;

		/*
		 * If _POSIX_CHOWN_RESTRICTED is enabled, you must have
		 * CAP_CHOWN
		 */
		if (rstchown && (fuid != uid || !groupmember(gid, cr)) &&
		    !_CAP_CRABLE(cr, CAP_CHOWN)) {
			return (EPERM);
		}

		/*
		 * If you chown a file, you must have CAP_FSETID in order
		 * to prevent clearing of the setuid & setgid bits.
		 */
		if ((fmode & (VSUID | VSGID)) && !_CAP_CRABLE(cr, CAP_FSETID))
			vap->va_mode &= ~(VSUID | VSGID);
	}

	if (mask & AT_MODE) {
		if (cr->cr_uid != fuid && (fmode & VSUID) &&
		    !_CAP_CRABLE(cr, CAP_FSETID))
			vap->va_mode &= ~VSUID;

		if (!groupmember(fuid, cr) && (fmode & VSGID) &&
		    !_CAP_CRABLE(cr, CAP_FSETID))
			vap->va_mode &= ~VSGID;
	}

	/*
	 * Don't mess with the size of swap files.
	 */
	if ((mask & AT_SIZE) &&
	    (vp->v_type == VREG) &&
	    (vp->v_flag & VISSWAP)) {
		return (EACCES);
	}
	error = nfs3setattr(bdp, vap, flags, cr);
	return (error);
}

int 	nfs3_setattr_retrycnt = 0;
static int
nfs3setattr(bhv_desc_t *bdp, struct vattr *vap, int flags, cred_t *cr)
{
	int rlock_held;
	int error;
	long mask;
	SETATTR3args args;
	SETATTR3res res;
	int douprintf;
	rnode_t *rp;
	vnode_t *vp;
	int bsize; 

	mask = vap->va_mask;
	vp = BHV_TO_VNODE(bdp);

	if ((((mask & AT_UID) && (vap->va_uid > 0xffff)) ||
	     ((mask & AT_GID) && (vap->va_gid > 0xffff))) &&
	    (VN_BHVTOMI(bdp)->mi_flags & MIF_SHORTUID))
	{
		/* trying to change ower or group, the fs is mounted shortuid
		 * and the user or group id won't fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}

	rp = BHVTOR(bdp);

	bsize = rtoblksz(rp);

	nfs_rwlock(bdp, VRWLOCK_WRITE);
	if (vp->v_type != VCHR) {
		if (mask & AT_SIZE) {
			if ((vap->va_size < rp->r_size) && vp->v_type == VREG) {
				VOP_TOSS_PAGES(vp, vap->va_size, 
					MAX(rp->r_size, rp->r_localsize) - 1, FI_REMAPF_LOCKED);
				rp->r_localsize = ROUNDUP(vap->va_size, bsize);
				rp->r_size = vap->va_size;
			}
		}
		/*
		 * Only need to flush pages if setting the mtime
		 * and if there any dirty pages.  Since we don't
		 * know whether there are any dirty pages if the
		 * file is mmap'd, then flush just to be safe.
		 */
		if ((mask & AT_MTIME) != 0 &&
		    ((rp->r_flags & RDIRTY) != 0 ||
		    rp->r_iocount > 0 ||
		    VN_MAPPED(vp))) {
#ifdef NFSCACHEDEBUG
			nfs3_setattrflushcnt++;
#endif /* NFSCACHEDEBUG */
			VOP_FLUSH_PAGES(vp, (off_t)0, rp->r_localsize - 1, B_ASYNC,
					FI_NONE, error);
			nfs3_riowait(rp);
		}
	}

	args.object = *rtofh3(rp);
	vattr_to_sattr3(vap, &args.new_attributes);
	if ((mask & AT_ATIME) && !(flags & ATTR_UTIME))
		args.new_attributes.atime.set_it = SET_TO_SERVER_TIME;
	if ((mask & AT_MTIME) && !(flags & ATTR_UTIME))
		args.new_attributes.mtime.set_it = SET_TO_SERVER_TIME;
tryagain:
	if (mask & AT_SIZE) {
		args.guard.check = TRUE;
		args.guard.obj_ctime.seconds = rp->r_attr.va_ctime.tv_sec;
		args.guard.obj_ctime.nseconds = rp->r_attr.va_ctime.tv_nsec;
	} else
		args.guard.check = FALSE;

	douprintf = 1;
	error = rlock_nohang(rp);
	if (error) {
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
		return(error);
	}
	error = rfs3call(RTOMI(rp), NFSPROC3_SETATTR,
			xdr_SETATTR3args, (caddr_t)&args,
			xdr_SETATTR3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	/*
	 * Purge the access cache is changing either the owner
	 * of the file, the group owner, or the mode.  These
	 * may change the access permissions of the file, so
	 * purge old information and start over again.
	 */
	if ((mask & (AT_UID | AT_GID | AT_MODE)) && rp->r_acc != NULL)
		nfs_purge_access_cache(bdp);

	if (error) {
		PURGE_ATTRCACHE(bdp);
		runlock(rp);
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
		return (error);
	}

	rlock_held = 1;
	error = geterrno3(res.status);
	if (!error) {
		/*
		 * If changing the size of the file, invalidate
		 * any local cached data which is no longer part
		 * of the file.  We also possibly invalidate the
		 * last page in the file.  We could use
		 * pvn_vpzero(), but this would mark the page as
		 * modified and require it to be written back to
		 * the server for no particularly good reason.
		 * This way, if we access it, then we bring it
		 * back in.  A read should be cheaper than a
		 * write.
		 * Update the size before caching the attributes because
		 * set_attrcache_time() will VOP_INVALFREE_PAGES() based
		 * on r_localsize.
		 * Update attributes and runlock() before VOP_TOSS_PAGES()
		 * to avoid a deadlock on the otw lock.
		 */
		off_t msize;
		if (mask & AT_SIZE) {
			msize = MAX(rp->r_size, rp->r_localsize);
			rp->r_localsize = ROUNDUP(vap->va_size, bsize);
			rp->r_size = vap->va_size;
		}
		nfs3_cache_wcc_data(bdp, &res.resok.obj_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(rp);
		if (mask & AT_SIZE) {
			/* 
			 * check to see that VOP_TOSS_PAGES doesn't
			 * take an assertion botch
			 */
			if (vap->va_size < msize )
				VOP_TOSS_PAGES(vp, vap->va_size, msize - 1, FI_REMAPF_LOCKED);
		}
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
	} else {
		nfs3_cache_wcc_data(bdp, &res.resfail.obj_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(rp);
		if (res.status == NFS3ERR_NOT_SYNC) {
			if (res.resfail.obj_wcc.after.attributes)
				goto tryagain;
			vap->va_mask = AT_CTIME;
			if (nfs3getattr(bdp, vap, 0, cr) == 0) {
				int size = args.new_attributes.size.size;

				/*
				 * This getattr may have regrown the file
				 * after we shrank it above, so reshrink it
				 * if necessary.  No need to retoss pages
				 * since we retained the rwlock.
				 */
				if ((mask & AT_SIZE) && (size < rp->r_size) &&
				    vp->v_type == VREG) {
					rp->r_localsize = ROUNDUP(size, bsize);
					rp->r_size = size;
				}
				vap->va_size = size;
				goto tryagain;
			}
		}
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
		PURGE_STALE_FH(error, bdp, cr);
	}

	return (error);
}

/* ARGSUSED */
static int
nfs3_access(bhv_desc_t *bdp, int mode, cred_t *cr)
{
	int rlock_held;
	int error;
	ACCESS3args args;
	ACCESS3res res;
	int douprintf;
	access_cache *acp;
	uint32 acc;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	rnode_t *rp;
	cred_t *cred = NULL;

	_NFS_IACCESS(bdp, mode, cr);

	acc = 0;
	if (mode & VREAD)
		acc |= ACCESS3_READ;
	if (mode & VWRITE) {
		if ((vp->v_vfsp->vfs_flag & VFS_RDONLY) && !ISVDEV(vp->v_type))
			return (EACCES);
		if (vp->v_type == VDIR)
			acc |= ACCESS3_DELETE;
		acc |= ACCESS3_MODIFY | ACCESS3_EXTEND;
	}
	if (mode & VEXEC) {
		if (vp->v_type == VDIR)
			acc |= ACCESS3_LOOKUP;
		else
			acc |= ACCESS3_EXECUTE;
	}

	rp = BHVTOR(bdp);
	if (rp->r_acc != NULL) {
		error = nfs3_validate_caches(bdp, cr);
		if (error) 
			return (error);
	}

	mutex_enter(&rp->r_statelock);
	for (acp = rp->r_acc; acp != NULL; acp = acp->next) {
		if (crcmp(acp->cred,cr) == 0) {
			if ((acp->known & acc) == acc) {
				if ((acp->allowed & acc) == acc) {
					mutex_exit(&rp->r_statelock);
					return (0);
				}
				mutex_exit(&rp->r_statelock);
				return (EACCES);
			}
			
			break;
		}
	}
	mutex_exit(&rp->r_statelock);

	args.object = *BHVTOFH3(bdp);
	args.access = acc;
	cred = cr;
tryagain:
	error = rlock_nohang(rp);
	if (error) {
		if (cred != cr)
			crfree(cred);
		return (error);
	}
	douprintf = 1;
	error = rfs3call(RTOMI(rp), NFSPROC3_ACCESS,
			xdr_ACCESS3args, (caddr_t)&args,
			xdr_ACCESS3res, (caddr_t)&res,
			cred, &douprintf, &res.status, 0);

	if (error) {
		runlock(rp);
		if (cred != cr)
			crfree(cred);
		return (error);
	}

	error = geterrno3(res.status);
	rlock_held = 1;
	if (!error) {
		nfs3_cache_post_op_attr(bdp, &res.resok.obj_attributes, cr,
			&rlock_held);
		if (rlock_held)
			runlock(rp);
		if (args.access != res.resok.access) {
			/*
			 * The following code implements the semantic that
			 * a setuid root program has *at least* the
			 * permissions of the user that is running the
			 * program.  See rfs3call() for more portions
			 * of the implementation of this functionality.
			 */
			if (cred->cr_uid == 0 && cred->cr_ruid != 0) {
				cred = crdup(cr);
				cred->cr_uid = cred->cr_ruid;
				goto tryagain;
			}
			error = EACCES;
		}
		mutex_enter(&rp->r_statelock);
		for (acp = rp->r_acc; acp != NULL; acp = acp->next) {
			if (crcmp(acp->cred, cr) == 0)
				break;
		}
		if (acp != NULL) {
			acp->known |= args.access;
			acp->allowed &= ~args.access;
			acp->allowed |= res.resok.access;
		} else {
			acp = (access_cache *)
				kmem_alloc(sizeof (*acp), KM_NOSLEEP);
			if (acp != NULL) {
				acp->next = rp->r_acc;
				rp->r_acc = acp;
				crhold(cr);
				acp->cred = cr;
				acp->known = args.access;
				acp->allowed = res.resok.access;
			}
		}
		mutex_exit(&rp->r_statelock);
	} else {
		nfs3_cache_post_op_attr(bdp, &res.resfail.obj_attributes, cr,
			&rlock_held);
		if (rlock_held)
			runlock(rp);
		PURGE_STALE_FH(error, bdp, cr);
	}
	if (cred != cr)
		crfree(cred);
	return (error);
}

static int nfs3_do_symlink_cache = 1;

static int
nfs3_readlink(bhv_desc_t *bdp, struct uio *uiop, cred_t *cr)
{
	int rlock_held;
	int error;
	READLINK3args args;
	READLINK3res res;
	rnode_t *rp;
	int douprintf;
	int len;
	bool_t savecontents;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	/*
	 * Can't readlink anything other than a symbolic link.
	 */
	if (vp->v_type != VLNK)
		return (EINVAL);

	rp = BHVTOR(bdp);
	if (nfs3_do_symlink_cache && rp->r_symlink.contents != NULL) {
		error = nfs3_validate_caches(bdp, cr);
		if (error)
			return (error);
		mutex_enter(&rp->r_statelock);
		if (rp->r_symlink.contents != NULL) {
			error = uiomove(rp->r_symlink.contents,
					rp->r_symlink.len, UIO_READ, uiop);
			mutex_exit(&rp->r_statelock);
			return (error);
		}
		mutex_exit(&rp->r_statelock);
	}

	args.symlink = *BHVTOFH3(bdp);
	res.resok.data = (char *)kmem_alloc(MAXPATHLEN, KM_SLEEP);
	savecontents = FALSE;

	douprintf = 1;
	error = rlock_nohang(rp);
	if (error) {
		return(error);
	}
	error = rfs3call(RTOMI(rp), NFSPROC3_READLINK,
			xdr_READLINK3args, (caddr_t)&args,
			xdr_READLINK3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	rlock_held = 1;
	if (!error) {
		error = geterrno3(res.status);
		if (!error) {
			nfs3_cache_post_op_attr(bdp,
						&res.resok.symlink_attributes,
						cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			len = strlen(res.resok.data);
			error = uiomove(res.resok.data, len, UIO_READ, uiop);
			if (nfs3_do_symlink_cache &&
			    rp->r_symlink.contents == NULL) {
				mutex_enter(&rp->r_statelock);
				if (rp->r_symlink.contents == NULL) {
					rp->r_symlink.contents = res.resok.data;
					rp->r_symlink.len = len;
					rp->r_symlink.size = MAXPATHLEN;
					savecontents = TRUE;
				}
				mutex_exit(&rp->r_statelock);
			}
		} else {
			nfs3_cache_post_op_attr(bdp,
						&res.resfail.symlink_attributes,
						cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			PURGE_STALE_FH(error, bdp, cr);
		}
	} else {
		runlock(rp);
	}

	if (!savecontents)
		kmem_free((caddr_t)res.resok.data, MAXPATHLEN);

	/*
	 * The over the wire error for attempting to readlink something
	 * other than a symbolic link is ENXIO.  However, we need to
	 * return EINVAL instead of ENXIO, so we map it here.
	 */
	return (error == ENXIO ? EINVAL : error);
}

/*
 * Flush local dirty pages to stable storage on the server.
 * XXX when this function does something real you need to look
 * at nfs3_read's RSYNC cases - jeffreyh
 */
/* ARGSUSED */
static int
nfs3_fsync(bhv_desc_t *bdp, int syncflag, cred_t *cr, off_t start, off_t stop)
{
	register rnode_t *rp;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	int bsize;
	int error = 0, error2 = 0;

	rp = BHVTOR(bdp);
	/* we need to account for pages that may have been set up for readahead */

	if (stop == (off_t)-1)
		stop = rp->r_localsize - 1;

	if (start > stop)
		return 0;

	bsize = rtoblksz(rp);

	if (syncflag & FSYNC_INVAL) {
		nfs_rwlock(bdp, VRWLOCK_WRITE);

		VOP_FLUSHINVAL_PAGES(vp, start, stop, FI_REMAPF_LOCKED);

		ASSERT(start > 0 || stop < rp->r_localsize ||
			(!VN_DIRTY(vp) && vp->v_pgcnt == 0));
		if (stop >= rp->r_localsize - 1) {
			rp->r_localsize = ROUNDUP(start, bsize);
		}
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
	} else {
		VOP_FLUSH_PAGES(vp, start, stop, B_ASYNC, FI_NONE, error2);
	}
	nfs3_riowait(rp);
	if (commit_file(rp, cr) != 0) {
	/*
	 * Here call into chunk cache to lock the buffers
	 * for the file and call VOP_COMMIT, nfs3_async_commit().
	 * Those commits will fail, too, but in doing so
	 * forces the cache to re-write the buffers SYNC.
	 */
		VOP_FLUSH_PAGES(vp, start, stop, B_STALE, FI_NONE, error);
	}
	if (rp->r_bds) {
		mutex_enter(&rp->r_statelock);
		error2 = bds_sync(rp, cr);
		mutex_exit(&rp->r_statelock);
	}
	return (error ? error : (error2 ? error2 : rp->r_error));
}

/*
 * Weirdness: if the file was removed or the target of a rename
 * operation while it was open, it got renamed instead.  Here we
 * remove the renamed file.
 */
/* ARGSUSED */
static int
nfs3_inactive(bhv_desc_t *bdp, cred_t *cr)
{
	int error;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register rnode_t *rp = BHVTOR(bdp);

	/*
	 * There are no more references to the vnode, so it is safe to clear
	 * RCLEANLOCKS.
	 */
	rclrflag(rp, RCLEANLOCKS);
	if (vp->v_type != VLNK && rp->r_unldrp != NULL) {
		register vnode_t *unldvp;
		rnode_t *unldrp;
		register char *unlname;
		register cred_t *unlcred;
		REMOVE3args args;
		REMOVE3res res;
		int douprintf;

		/*
		 * Save the vnode pointer for the directory where the
		 * unlinked-open file got renamed, then set it to NULL
		 * to prevent another thread from getting here before
		 * we're done with the remove.  While we have the
		 * statelock, make local copies of the pertinent rnode
		 * fields.  If we weren't to do this in an atomic way, the
		 * the unl* fields could become inconsistent with respect
		 * to each other due to a race condition between this
		 * code and nfs_remove().  See bug report 1034328.
		 */
		nfs3_riowait(rp);
		mutex_enter(&rp->r_statelock);
		if (rp->r_unldrp != NULL) {
			unldrp = rp->r_unldrp;
			unldvp = rtov(unldrp);
			rp->r_unldrp = NULL;
			unlname = rp->r_unlname;
			rp->r_unlname = NULL;
			unlcred = rp->r_unlcred;
			rp->r_unlcred = NULL;
			mutex_exit(&rp->r_statelock);
			rclrflag(rp, RDIRTY);

			/* toss all pages */
			if (vp->v_type != VCHR) {
				nfs_rwlock(bdp, VRWLOCK_WRITE);
				if (rp->r_localsize > 0)
					VOP_TOSS_PAGES(vp, 0, rp->r_localsize - 1, FI_NONE);
				nfs_rwunlock(bdp, VRWLOCK_WRITE);
			}

			/*
			 * Do the remove operation on the renamed file
			 */
			setdiropargs3(&args.object, unlname, rtobhv(unldrp));
			douprintf = 1;
			error = rfs3call(RTOMI(unldrp), NFSPROC3_REMOVE,
					xdr_REMOVE3args, (caddr_t)&args,
					xdr_REMOVE3res, (caddr_t)&res,
					unlcred, &douprintf, &res.status, 0);

			if (error) {
				/* XXX SHOULDN"T WE DO SOMETHING HERE? */
			}
			
			/*
			 * Release stuff held for the remove
			 */
			VN_RELE(unldvp);
			kmem_free((caddr_t)unlname, NFS_MAXNAMLEN);
			crfree(unlcred);
		} else {
			mutex_exit(&rp->r_statelock);
			VOP_FLUSH_PAGES(vp, (off_t)0, rp->r_localsize - 1, 
				B_ASYNC, FI_NONE, error);
			nfs3_riowait(rp);
		}
	} else {
		VOP_FLUSH_PAGES(vp, (off_t)0, rp->r_localsize - 1, B_ASYNC, FI_NONE, error);
		nfs3_riowait(rp);
	}

	if (rp->r_bds) {
		mutex_enter(&rp->r_statelock);
		bds_close(rp, cr);
		mutex_exit(&rp->r_statelock);
	}

#ifdef NSD
	/*
	 * For local servers (nsd) we call a close routine
	 * so they know the file is no longer in use.
	 */
	{
		mntinfo_t *mi;
		extern bool_t xdr_nfs_fh3(XDR *, nfs_fh3 *);
		bool_t b;

		mi = RTOMI(rp);
		if ((mi->mi_flags & MIF_SILENT) && (vp->v_type != VDIR)) {
			error = rlock_nohang(rp);
			if (error) {
				return error;
			}

			error = nsd_rfscall(mi, NSDPROC1_CLOSE,
			    xdr_nfs_fh3, (caddr_t)rtofh3(rp), xdr_bool,
			    (caddr_t)&b, cr);

			runlock(rp);
			PURGE_STALE_FH(ESTALE, bdp, cr);
		}
	}
#endif

	/*
	 * Forget any asynchronous errors which till now may have stopped
         * further i/o.  Reset the readahead detector on last close.
	 */
	rp->r_error = 0;
	rp->r_lastr = 0;

	return VN_INACTIVE_CACHE;
}

/*
 * Remote file system operations having to do with directory manipulation.
 */

static int
nfs3_lookup(bhv_desc_t *bdp, char *nm, vnode_t **vpp,
	struct pathname *pnp, int flags, vnode_t *rdir, cred_t *cr)
{
	int error;
	vnode_t *vp;

	error = nfs3lookup(bdp, nm, vpp, pnp, flags, rdir, cr);

	/*
	 * If vnode is a device, create special vnode.
	 */
	if (!error && ISVDEV((*vpp)->v_type)) {
		vp = *vpp;
		*vpp = spec_vp(vp, vp->v_rdev, vp->v_type, cr);

		VN_RELE(vp);
	}

	return (error);
}


/* ARGSUSED */
static int
nfs3lookup(bhv_desc_t *dbdp, char *nm, vnode_t **vpp,
	struct pathname *pnp, int flags, vnode_t *rdir, cred_t *cr)
{
	int rlock_held;
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	bhv_desc_t *bdp;
	int error;
	LOOKUP3args args;
	LOOKUP3res res;
	int douprintf;
	struct vattr vattr;
	rnode_t *drp = BHVTOR(dbdp);

#ifdef KTRACE
	TRACE1(T_NFS3_LOOKUP, T_START, dvp);
#endif /* KTRACE  */
	    
	/*
	 * If lookup is for "", just return dvp.  Don't need
	 * to send it over the wire, look it up in the dnlc,
	 * or perform any access checks.
	 */
	if (*nm == '\0') {
		VN_HOLD(dvp);
		*vpp = dvp;
#ifdef KTRACE
		TRACE1(T_NFS3_LOOKUP, T_EXIT_OK, dvp);
#endif /* KTRACE  */
		return (0);
	}

	*vpp = NULL;

	/*
	 * Can't do lookups in non-directories.
	 */
	if (dvp->v_type != VDIR)
	{
#ifdef KTRACE
		TRACE2(T_NFS3_LOOKUP, T_EXIT_ERROR, dvp, ENOTDIR);
#endif /* KTRACE  */
		return (ENOTDIR);
	}

	/*
	 * If lookup is for "." (or ".." if this is a mountpoint,
	 * just return dvp.  Don't need
	 * to send it over the wire or look it up in the dnlc,
	 * just need to check access.
	 */
	if (ISDOT(nm) || (ISDOTDOT(nm)) && (dvp->v_flag & VROOT)) {
		error = nfs3_access(dbdp, VEXEC, cr);
		if (error)
		{
#ifdef KTRACE
			TRACE2(T_NFS3_LOOKUP, T_EXIT_ERROR_1, dvp, error);
#endif /* KTRACE  */
			return (error);
		}
		VN_HOLD(dvp);
		*vpp = dvp;
#ifdef KTRACE
		TRACE1(T_NFS3_LOOKUP, T_EXIT_OK_1, dvp);
#endif /* KTRACE  */
		return (0);
	}

	/*
	 * Lookup this name in the DNLC.  If successful, then check
	 * access and then recheck the DNLC.  The DNLC is rechecked
	 * just in case this entry got invalidated during the call
	 * to nfs3_access.
	 *
	 * The assumption is made that nfs3_access invokes
	 * nfs3_validate_caches to make sure the access cache is valid.
	 * If the attributes were timed out, an ACCESS call is made to
	 * the server which returns new attributes.  When this happens,
	 * the attribute cache is updated and the DNLC will be purged
	 * if appropriate.
	 *
	 * Another assumption that is being made is that it is safe
	 * to say that a file exists which may not on the server.
	 * Any operations to the server will fail with ESTALE.
	 */
	bdp = dnlc_lookup(dvp, nm, ANYCRED, 0);
	if (bdp != NULL) {
		VN_RELE(BHV_TO_VNODE(bdp));
		error = nfs3_access(dbdp, VEXEC, cr);
		if (error)
		{
#ifdef KTRACE
			TRACE2(T_NFS3_LOOKUP, T_EXIT_ERROR_2, dvp, error);
#endif /* KTRACE  */
			return (error);
		}
		bdp = dnlc_lookup(dvp, nm, ANYCRED, 0);
		if (bdp != NULL) {
			*vpp = BHV_TO_VNODE(bdp);
			return (0);
		}
	}

	setdiropargs3(&args.what, nm, dbdp);

	douprintf = 1;
	error = raccess(drp);
	if (error) {
		return(error);
	}
again_lookup:
	error = rfs3call(VN_BHVTOMI(dbdp), NFSPROC3_LOOKUP,
			xdr_LOOKUP3args, (caddr_t)&args,
			xdr_LOOKUP3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	rlock_held = 1;
	if (!error) {
		error = geterrno3(res.status);
		if (!error) {
			if (res.resok.dir_attributes.attributes &&
			    latestmod(drp, &res.resok.dir_attributes.attr)) {
				nfs3_cache_post_op_attr(dbdp,
					&res.resok.dir_attributes,
					cr, &rlock_held);
			}
			if (rlock_held)
				runlock(drp);
			if (res.resok.obj_attributes.attributes) {
				error = makenfs3node(&res.resok.object,
						&res.resok.obj_attributes.attr,
						dvp->v_vfsp, &bdp, cr);
				if (error == EAGAIN)  {
#ifdef DEBUG
					printf("nfs3_lookup: mknfsnode returns EAGAIN, retrying\n");
#endif /* DEBUG */
					goto again_lookup;
				} 
				if (!error) {
					*vpp = BHV_TO_VNODE(bdp);
					dnlc_enter(dvp, nm, bdp, NOCRED);
				}
			} else {
				error = makenfs3node(&res.resok.object, NULL,
						dvp->v_vfsp, &bdp, cr);
				if (!error) {
					*vpp = BHV_TO_VNODE(bdp);
					if ((*vpp)->v_type == VNON) {
						vattr.va_mask = AT_TYPE;
						error = nfs3getattr(bdp, 
								&vattr, 0, cr);
						if (error) {
							VN_RELE(*vpp);
							return (error);
						}
					}
					dnlc_enter(dvp, nm, bdp, NOCRED);
				}
			}
		} else {
			if (res.resfail.dir_attributes.attributes &&
			    latestmod(drp, &res.resfail.dir_attributes.attr)) {
				nfs3_cache_post_op_attr(dbdp,
						&res.resfail.dir_attributes,
						cr, &rlock_held);
			}
			if (rlock_held)
				runlock(drp);
			PURGE_STALE_FH(error, dbdp, cr);
		}
	} else {
		runlock(drp);
	}

	return (error);
}

static int
nfs3_create(bhv_desc_t *dbdp, char *nm, struct vattr *va,
	int flags, int mode, vnode_t **vpp, cred_t *cr)
{
	bhv_desc_t *bdp;
	int error;

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}

	if (nm == NULL || *nm == '\0')
		return (EINVAL);

	error = nfs3_lookup(dbdp, nm, vpp, NULL, 0, NULL, cr);
	if (!error) {
		if (flags & VEXCL)
			error = EEXIST;
		else if ((*vpp)->v_type == VDIR && (mode & VWRITE))
			error = EISDIR;
		else {
			VOP_ACCESS(*vpp, mode, cr, error);
			if (!error) {
				if ((va->va_mask & AT_SIZE) && 
					(*vpp)->v_type == VREG) {
					va->va_mask = AT_SIZE;
					bdp = vn_bhv_lookup_unlocked(
							VN_BHV_HEAD(*vpp),
							&nfs3_vnodeops);
					ASSERT(bdp != NULL);
					error = nfs3setattr(bdp, va, 0, cr);
				}
			}
		}
		if (error) {
			VN_RELE(*vpp);
		}
		return (error);
	} else if (error == ENOENT) {
		/*
		 * XPG4 says create cannot allocate a file if the
		 * file size limit is set to 0.
		 */
		_NFS_IACCESS(dbdp, VWRITE, cr);
		if (flags & VZFS) {
			return EFBIG;
		}
	}

	if (va->va_type == VREG)
		return (nfs3create(dbdp, nm, va, flags, mode, vpp, cr));
	return (nfs3mknod(dbdp, nm, va, flags, mode, vpp, cr));
}

/* ARGSUSED */
static int
nfs3create(bhv_desc_t *dbdp, char *nm, struct vattr *va,
	int flags, int mode, vnode_t **vpp, cred_t *cr)
{
	int rlock_held;
	rnode_t *drp = BHVTOR(dbdp);
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	bhv_desc_t *bdp;
	int error;
	CREATE3args args;
	CREATE3res res;
	int douprintf;
	struct vattr vattr;
	char hw_serial[4] = "123";

	/*
	 * Decide what the group-id of the created file should be.
	 * Set it in attribute list as advisory...then do a setattr
	 * if the server didn't get it right the first time.
	 */
	va->va_gid = setdirgid3(dbdp);
	va->va_mask |= AT_GID;

	if (((cr->cr_uid > 0xffff) || (va->va_gid > 0xffff)) &&
	    (VN_BHVTOMI(dbdp)->mi_flags & MIF_SHORTUID))
	{
		/* trying to change ower or group, the fs is mounted shortuid
		 * and the user or group id won't fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}

	setdiropargs3(&args.where, nm, dbdp);
	if (flags & VEXCL) {
		args.how.mode = EXCLUSIVE;
		((timespec_t *)(&args.how.createhow3_u.verf))->tv_sec =
								atoi(hw_serial);
		((timespec_t *)(&args.how.createhow3_u.verf))->tv_nsec = 0;
	} else {
		args.how.mode = UNCHECKED;
		vattr_to_sattr3(va, &args.how.createhow3_u.obj_attributes);
	}

	douprintf = 1;
	error = rlock_nohang(drp);
	if (error) {
		return (error);
	}
again_create:
	error = rfs3call(VN_BHVTOMI(dbdp), NFSPROC3_CREATE,
			xdr_CREATE3args, (caddr_t)&args,
			xdr_CREATE3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		PURGE_ATTRCACHE(dbdp);
		runlock(drp);
		return (error);
	}

	rlock_held = 1;
	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dbdp, &res.resok.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		if (BHVTOR(dbdp)->r_dir != NULL)
			nfs_purge_rddir_cache(dbdp);

		if (!res.resok.obj.handle_follows) {
			error = nfs3lookup(dbdp, nm, vpp, NULL, 0, NULL, cr);
			if (error)
				return (error);
			bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(*vpp),
						     &nfs3_vnodeops);
			ASSERT(bdp != NULL);
		} else {
			if (res.resok.obj_attributes.attributes) {
				error = makenfs3node(&res.resok.obj.handle,
						&res.resok.obj_attributes.attr,
						dvp->v_vfsp, &bdp, cr);
				if (error == EAGAIN) {
#ifdef DEBUG
					printf("nfs3_create: mknfsnode returns EAGAIN, retrying\n");
#endif /* DEBUG */
					goto again_create;
				}
				if (!error) {
					*vpp = BHV_TO_VNODE(bdp);
				}
			} else {
				(void)makenfs3node(&res.resok.obj.handle, NULL,
						dvp->v_vfsp, &bdp, cr);
				*vpp = BHV_TO_VNODE(bdp);
				if ((*vpp)->v_type == VNON) {
					vattr.va_mask = AT_TYPE;
					error = nfs3getattr(bdp, &vattr, 0, cr);
					if (error) {
						VN_RELE(*vpp);
						return (error);
					}
				}
			}
			if (!error) {
				dnlc_enter(dvp, nm, bdp, NOCRED);
			}
		}

		if ((flags & VEXCL) ||
		    va->va_gid != BHVTOR(bdp)->r_attr.va_gid) {
			/*
			 * If doing an exclusive create or if the gid on
			 * the file is not right, then generate a SETATTR
			 * to correct the attributes.  If doing an
			 * exclusive create, then try to set the mtime
			 * and the atime to the server's current time.
			 * It is somewhat expected that these fields
			 * will be used to store the exclusive create
			 * cookie.  If not, server implementors will
			 * need to know that a SETATTR will follow an
			 * exclusive create and the cookie should be
			 * destroyed if appropriate.
			 * Also turn off the unnecessary AT_SIZE during
			 * exclusive create to prevent failures in remote
			 * filesystems that fail with SETATTR(AT_SIZE) on
			 * files with mode==0.
			 */
			if (flags & VEXCL) {
				va->va_mask |= (AT_MODE | AT_MTIME | AT_ATIME);
				va->va_mask &= ~AT_SIZE;
			}
			/* 
			 * If the gid was set correctly above, don't try to set 
			 * it again. AT_GID is still in va_mask at this point. 
			 * If setattr fails with AT_GID, try again without it.
			 */
			if (va->va_gid == BHVTOR(bdp)->r_attr.va_gid)
				va->va_mask &= ~AT_GID;
no_gid:
                        error = nfs3setattr(bdp, va, 0, cr);
                        if (error && (va->va_mask & AT_GID)) {
                                va->va_mask &= ~AT_GID;
                                goto no_gid;
                        }
			if (error) {
				/* couldn't correct the attributes */
				VN_RELE(BHV_TO_VNODE(bdp));
				(void) nfs3_remove(dbdp, nm, cr);
				return(error);
			}
		}

		/*
		 * If vnode is a device create special vnode
		 */
		if (ISVDEV((*vpp)->v_type)) {
			vnode_t *newvp;

			newvp = spec_vp(*vpp, (*vpp)->v_rdev,
					(*vpp)->v_type, cr);
			VN_RELE(*vpp);
			*vpp = newvp;
		}
	} else {
		nfs3_cache_wcc_data(dbdp, &res.resfail.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		PURGE_STALE_FH(error, dbdp, cr);
	}
	return (error);
}
/* ARGSUSED */
static int
nfs3mknod(bhv_desc_t *dbdp, char *nm, struct vattr *va,
	int flags, int mode, vnode_t **vpp, cred_t *cr)
{
	int rlock_held;
	rnode_t *drp = BHVTOR(dbdp);
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	bhv_desc_t *bdp;
	int error;
	MKNOD3args args;
	MKNOD3res res;
	int douprintf;
	struct vattr vattr;

	/*
	 * Decide what the group-id of the created file should be.
	 * Set it in attribute list as advisory...then do a setattr
	 * if the server didn't get it right the first time.
	 */
	va->va_gid = setdirgid3(dbdp);
	va->va_mask |= AT_GID;

	if (((cr->cr_uid > 0xffff) || (va->va_gid > 0xffff)) &&
	    (VN_BHVTOMI(dbdp)->mi_flags & MIF_SHORTUID))
	{
		/* trying to change ower or group, the fs is mounted shortuid
		 * and the user or group id won't fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}

	switch ((int)va->va_type) {
	case VCHR:
		setdiropargs3(&args.where, nm, dbdp);
		args.what.type = NF3CHR;
		vattr_to_sattr3(va,
				&args.what.mknoddata3_u.device.dev_attributes);
		args.what.mknoddata3_u.device.spec.specdata1 =
							getemajor(va->va_rdev);
		args.what.mknoddata3_u.device.spec.specdata2 =
							geteminor(va->va_rdev);
		break;
	case VBLK:
		setdiropargs3(&args.where, nm, dbdp);
		args.what.type = NF3BLK;
		vattr_to_sattr3(va,
				&args.what.mknoddata3_u.device.dev_attributes);
		args.what.mknoddata3_u.device.spec.specdata1 =
							getemajor(va->va_rdev);
		args.what.mknoddata3_u.device.spec.specdata2 =
							geteminor(va->va_rdev);
		break;
	case VFIFO:
		setdiropargs3(&args.where, nm, dbdp);
		args.what.type = NF3FIFO;
		vattr_to_sattr3(va, &args.what.mknoddata3_u.pipe_attributes);
		break;
	case VSOCK:
		setdiropargs3(&args.where, nm, dbdp);
	 	args.what.type = NF3SOCK;
	 	vattr_to_sattr3(va, &args.what.mknoddata3_u.pipe_attributes);
	 	break;
	default:
		return (EINVAL);
	}

	douprintf = 1;
	error = rlock_nohang(drp);
	if (error) {
		return (error);
	}
	error = rfs3call(VN_BHVTOMI(dbdp), NFSPROC3_MKNOD,
			xdr_MKNOD3args, (caddr_t)&args,
			xdr_MKNOD3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		PURGE_ATTRCACHE(dbdp);
		runlock(drp);
		return (error);
	}

	rlock_held = 1;
	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dbdp, &res.resok.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		if (BHVTOR(dbdp)->r_dir != NULL)
			nfs_purge_rddir_cache(dbdp);

		if (!res.resok.obj.handle_follows) {
			error = nfs3lookup(dbdp, nm, vpp, NULL, 0, NULL, cr);
			if (error)
				return (error);
			bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(*vpp),
						     &nfs3_vnodeops);
			ASSERT(bdp != NULL);
		} else {
			if (res.resok.obj_attributes.attributes) {
				error = makenfs3node(&res.resok.obj.handle,
						&res.resok.obj_attributes.attr,
						dvp->v_vfsp, &bdp, cr);
				*vpp = BHV_TO_VNODE(bdp);
			} 
			if (!res.resok.obj_attributes.attributes || 
			    (error == EAGAIN)) {
				(void) makenfs3node(&res.resok.obj.handle, NULL,
						    dvp->v_vfsp, &bdp, cr);
				*vpp = BHV_TO_VNODE(bdp);
				if ((*vpp)->v_type == VNON) {
					vattr.va_mask = AT_TYPE;
					error = nfs3getattr(bdp, &vattr, 0, cr);
					if (error) {
						VN_RELE(*vpp);
						return (error);
					}
				}

			}
			dnlc_enter(dvp, nm, bdp, NOCRED);
		}

		if (va->va_gid != BHVTOR(bdp)->r_attr.va_gid) {
			va->va_mask = AT_GID;
			(void) nfs3setattr(bdp, va, 0, cr);
		}

		/*
		 * If vnode is a device create special vnode
		 */
		if (ISVDEV((*vpp)->v_type)) {
			vnode_t *newvp;

			newvp = spec_vp(*vpp, (*vpp)->v_rdev,
					(*vpp)->v_type, cr);
			VN_RELE(*vpp);
			*vpp = newvp;
		}
	} else {
		nfs3_cache_wcc_data(dbdp, &res.resfail.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		PURGE_STALE_FH(error, dbdp, cr);
	}
	return (error);
}

/*
 * Weirdness: if the vnode to be removed is open
 * we rename it instead of removing it and nfs_inactive
 * will remove the new name.
 */
static int
nfs3_remove(bhv_desc_t *dbdp, char *nm, cred_t *cr)
{
	int rlock_held;
	rnode_t *drp = BHVTOR(dbdp);
	int error;
	REMOVE3args args;
	REMOVE3res res;
	vnode_t *vp;
	char *tmpname;
	int douprintf;
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	rnode_t *rp;
	bhv_desc_t *bdp;

	_NFS_IACCESS(dbdp, VEXEC | VWRITE, cr);

	error = nfs3lookup(dbdp, nm, &vp, NULL, 0, NULL, cr);
	if (error)
		return (error);

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &nfs3_vnodeops);
	ASSERT(bdp != NULL);

	if (error = _MAC_NFS_IACCESS(bdp, MACWRITE, cr)) {
		VN_RELE(vp);
		return (error);
	}

	if (vp->v_type == VDIR && !_CAP_CRABLE(cr, CAP_MKNOD)) {
		VN_RELE(vp);
		return (EPERM);
	}

	/*
	 * We need to flush the name cache so we can
	 * check the real reference count on the vnode
	 */
	dnlc_purge_vp(vp);

	rp = BHVTOR(bdp);
	if (vp->v_type != VLNK && vp->v_count > 1 && rp->r_unldrp == NULL) {
		tmpname = newname();
		error = nfs3rename(dbdp, nm, dvp, tmpname, cr);
		if (error)
			kmem_free((caddr_t)tmpname, NFS_MAXNAMLEN);
		else {
			mutex_enter(&rp->r_statelock);
			if (rp->r_unldrp == NULL) {
				VN_HOLD(dvp);
				rp->r_unldrp = BHVTOR(dbdp);
				if (rp->r_unlcred != NULL)
					crfree(rp->r_unlcred);
				crhold(cr);
				rp->r_unlcred = cr;
				rp->r_unlname = tmpname;
			} else {
				kmem_free((caddr_t)rp->r_unlname,
					NFS_MAXNAMLEN);
				rp->r_unlname = tmpname;
			}
			mutex_exit(&rp->r_statelock);
		}
	} else {
		/*
		 * Bug 292445 - We occasionally run into
		 * a vnode that has v_dbuf=0 but some pages
		 * hanging off v_dpages.  trusting just the count
		 * means we don't even invalidate the pages,
		 * but on top of that nfs_invalidate_pages
		 * eventually does a VOP_FLUSHINVAL_PAGES which may
		 * actually schedule some async writes, so
		 * instead of the original
		 *      mutex_enter(&rp->r_statelock);
		 *      rp->r_flags &= ~RDIRTY;
		 *      mutex_exit(&rp->r_statelock);
		 *      if (vp->v_dbuf != 0) {
		 *      	ASSERT(vp->v_type != VCHR);
		 *      	nfs_invalidate_pages(vp, 0, cr);
		 *      }
		 * we do what nfsV2 is doing and VOP_TOSS_PAGES
		 * the whole file (since we're deleting it)
		 */
		rsetflag(rp, RDIRTY);
		if (vp->v_dbuf !=0 || vp->v_dpages != NULL) {
			nfs_rwlock(bdp, VRWLOCK_WRITE);
			if (rp->r_localsize > 0)
				VOP_TOSS_PAGES(vp, 0, rp->r_localsize - 1, FI_NONE);
			nfs_rwunlock(bdp, VRWLOCK_WRITE);
			rp->r_localsize = 0;
			vp->v_pgcnt = 0;
			rclrflag(rp, RDIRTY);
		}

		setdiropargs3(&args.object, nm, dbdp);

		douprintf = 1;
		error = rlock_nohang(drp);
		if (error) {
			VN_RELE(vp);
			return (error);
		}
		error = rfs3call(VN_BHVTOMI(dbdp), NFSPROC3_REMOVE,
				xdr_REMOVE3args, (caddr_t)&args,
				xdr_REMOVE3res, (caddr_t)&res,
				cr, &douprintf, &res.status, 0);

		PURGE_ATTRCACHE(bdp);

		if (error) {
			PURGE_ATTRCACHE(dbdp);
			runlock(drp);
		} else {
			rlock_held = 1;
			error = geterrno3(res.status);
			if (!error) {
				nfs3_cache_wcc_data(dbdp, &res.resok.dir_wcc,
						cr, &rlock_held);
				if (rlock_held)
					runlock(drp);
				if (BHVTOR(dbdp)->r_dir != NULL)
					nfs_purge_rddir_cache(dbdp);
			} else {
				nfs3_cache_wcc_data(dbdp, &res.resfail.dir_wcc,
						cr, &rlock_held);
				if (rlock_held)
					runlock(drp);
				PURGE_STALE_FH(error, dbdp, cr);
			}
		}
	}

	VN_RELE(vp);

	return (error);
}

static int
nfs3_link(bhv_desc_t *tdbdp, vnode_t *svp, char *tnm, cred_t *cr)
{
	post_op_attr *popattr;
	wcc_data *wcc;
	int rlock_held;
	rnode_t *tdrp = BHVTOR(tdbdp);
	rnode_t *srp;
	int error;
	LINK3args args;
	LINK3res res;
	vnode_t *realvp;
	int douprintf;
	mntinfo_t *mi;
	vnode_t *tdvp = BHV_TO_VNODE(tdbdp);
	bhv_desc_t *sbdp;

	VOP_REALVP(svp, &realvp, error);
	if (!error)
		svp = realvp;

	sbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(svp), &nfs3_vnodeops);
	if (sbdp == NULL) {
		return EXDEV;
	}
	mi = VN_BHVTOMI(sbdp);
	if (!(mi->mi_flags & MIF_LINK))
		return (EOPNOTSUPP);

	args.file = *BHVTOFH3(sbdp);
	setdiropargs3(&args.link, tnm, tdbdp);

	dnlc_remove(tdvp, tnm);

	_NFS_IACCESS(tdbdp, VEXEC | VWRITE, cr);

	douprintf = 1;
	srp = BHVTOR(sbdp);
	mutex_enter(&nfs3_linkrename_lock);
	error = rlock_nohang(tdrp);
	if (error) {
		mutex_exit(&nfs3_linkrename_lock);
		return(error);
	}
	if (tdbdp != sbdp) {
		error = rlock_nohang(srp);
		if (error) {
			runlock(tdrp);
			mutex_exit(&nfs3_linkrename_lock);
			return(error);
		}
	}
	error = rfs3call(mi, NFSPROC3_LINK,
			xdr_LINK3args, (caddr_t)&args,
			xdr_LINK3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		PURGE_ATTRCACHE(tdbdp);
		runlock(tdrp);
		if (tdbdp != sbdp) {
			runlock(srp);
		}
		mutex_exit(&nfs3_linkrename_lock);
		return (error);
	}

	error = geterrno3(res.status);

	/*
	 * nfs3_linkrename_lock must be held here because
	 * nfs3_cache_post_op_attr or nfs3_cache_wcc_data may
	 * result in a call to set_attrcache_time which will
	 * release and reacquire r_lock if the data for the file
	 * must be flushed.
	 */
	rlock_held = 1;
	if (!error) {
		popattr = &res.resok.file_attributes;
		wcc = &res.resok.linkdir_wcc;
	} else {
		popattr = &res.resfail.file_attributes;
		wcc = &res.resfail.linkdir_wcc;
	}
	nfs3_cache_post_op_attr(sbdp, popattr, cr, &rlock_held);
	if (rlock_held)
		runlock(srp);
	if (tdbdp != sbdp) {
		rlock_held = 1;
		nfs3_cache_wcc_data(tdbdp, wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(tdrp);
	}
	mutex_exit(&nfs3_linkrename_lock);
	if (!error) {
		if (BHVTOR(tdbdp)->r_dir != NULL)
			nfs_purge_rddir_cache(tdbdp);
		dnlc_enter(tdvp, tnm, sbdp, NOCRED);
	} else if (error == EOPNOTSUPP)
		atomicClearUint(&mi->mi_flags, MIF_LINK);

	return (error);
}

/* ARGSUSED */
static int
nfs3_rename(bhv_desc_t *odbdp, char *onm, vnode_t *ndvp, char *nnm,
	struct pathname *npnp, cred_t *cr)
{
	vnode_t *realvp;
	int error;

	VOP_REALVP(ndvp, &realvp, error);
	if (!error)
		ndvp = realvp;

	return (nfs3rename(odbdp, onm, ndvp, nnm, cr));
}

/*
 * nfs3rename does the real work of renaming in NFS Version 3.
 */
static int
nfs3rename(bhv_desc_t *odbdp, char *onm, vnode_t *ndvp, char *nnm,
	cred_t *cr)
{
	int rlock_held;
	wcc_data *fwcc, *twcc;
	rnode_t *odrp = BHVTOR(odbdp);
	rnode_t *ndrp;
	int error;
	RENAME3args args;
	RENAME3res res;
	int douprintf;
	vnode_t *nvp;
	vnode_t *ovp;
	vnode_t *odvp = BHV_TO_VNODE(odbdp);
	char *tmpname;
	rnode_t *rp;
	bhv_desc_t *ndbdp;
	bhv_desc_t *nbdp;

	if (strcmp(onm, ".") == 0 || strcmp(onm, "..") == 0 ||
	    strcmp(nnm, ".") == 0 || strcmp(nnm, "..") == 0)
		return (EINVAL);

	/*
	 * Lookup the target file.  If it exists, it needs to be
	 * checked to see whether it is a mount point and whether
	 * it is active (open).
	 */
	ndbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(ndvp), &nfs3_vnodeops);
	if (ndbdp == NULL) {
		return (EXDEV);
	}

	_NFS_IACCESS(odbdp, VEXEC | VWRITE, cr);
	if (ndvp && odvp != ndvp)
		_NFS_IACCESS(ndbdp, VEXEC | VWRITE, cr);

	if (bhvtor(odbdp)->r_attr.va_fsid
            != bhvtor(ndbdp)->r_attr.va_fsid) {
                return EXDEV;
        }

	error = nfs3lookup(ndbdp, nnm, &nvp, NULL, 0, NULL, cr);

	if (!error) {
		/*
		 * If this file has been mounted on, then just
		 * return busy because renaming to it would remove
		 * the mounted file system from the name space.
		 */
		if (nvp->v_vfsmountedhere != NULL) {
			VN_RELE(nvp);
			return (EBUSY);
		}

		/*
		 * Purge the name cache of all references to this vnode
		 * so that we can check the reference count to infer
		 * whether it is active or not.
		 */
		dnlc_purge_vp(nvp);

		/*
		 * If the vnode is active, arrange to rename it to a
		 * temporary file so that it will continue to be
		 * accessible.  This implements the "unlink-open-file"
		 * semantics for the target of a rename operation.
		 * Before doing this though, make sure that the
		 * source and target files are not already the same.
		 */
		if (nvp->v_type != VLNK && nvp->v_count > 1) {

			/*
			 * Lookup the source name.
			 */
			error = nfs3lookup(odbdp, onm, &ovp, NULL, 0, NULL, cr);

			/*
			 * The source name *should* already exist.
			 */
			if (error) {
				VN_RELE(nvp);
				return (error);
			}

			/*
			 * Compare the two vnodes.  If they are the same,
			 * just release all held vnodes and return success.
			 */
			if (ovp == nvp) {
				VN_RELE(ovp);
				VN_RELE(nvp);
				return (0);
			}

			/*
			 * Can't mix and match directories and non-
			 * directories in rename operations.  If
			 * they don't match, then return the error
			 * based on the type for source (ovp) file.
			 */
			if (ovp->v_type == VDIR) {
				if (nvp->v_type != VDIR)
					error = ENOTDIR;
			} else {
				if (nvp->v_type == VDIR)
					error = EISDIR;
			}

			if (error) {
				VN_RELE(ovp);
				VN_RELE(nvp);
				return (error);
			}

			VN_RELE(ovp);

			/*
			 * The target file exists, is not the same as
			 * the source file, and is active.  Rename it
			 * to avoid the server removing the file
			 * completely.
			 */
			tmpname = newname();
			error = nfs3rename(ndbdp, nnm, ndvp, tmpname, cr);
			if (error)
				kmem_free((caddr_t)tmpname, NFS_MAXNAMLEN);
			else {
				nbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(nvp),
							      &nfs3_vnodeops);
				ASSERT(nbdp != NULL);
				rp = BHVTOR(nbdp);
				mutex_enter(&rp->r_statelock);
				if (rp->r_unldrp == NULL) {
					VN_HOLD(ndvp);
					rp->r_unldrp = BHVTOR(ndbdp);
					if (rp->r_unlcred != NULL)
						crfree(rp->r_unlcred);
					crhold(cr);
					rp->r_unlcred = cr;
					rp->r_unlname = tmpname;
				} else {
					kmem_free((caddr_t)rp->r_unlname,
						NFS_MAXNAMLEN);
					rp->r_unlname = tmpname;
				}
				mutex_exit(&rp->r_statelock);
			}
		}

		VN_RELE(nvp);
	}

	dnlc_remove(odvp, onm);
	dnlc_remove(ndvp, nnm);

	setdiropargs3(&args.from, onm, odbdp);
	setdiropargs3(&args.to, nnm, ndbdp);

	douprintf = 1;
	ndrp = BHVTOR(ndbdp);
	if (odrp != ndrp) {
		mutex_enter(&nfs3_linkrename_lock);
		error = rlock_nohang(odrp);
		if (error) {
			mutex_exit(&nfs3_linkrename_lock);
			return(error);
		}
		error = rlock_nohang(ndrp);
		if (error) {
			runlock(odrp);
			mutex_exit(&nfs3_linkrename_lock);
			return(error);
		}
	} else {
		error = rlock_nohang(odrp);
		if (error) {
			return(error);
		}
	}
	error = rfs3call(VN_BHVTOMI(odbdp), NFSPROC3_RENAME,
			xdr_RENAME3args, (caddr_t)&args,
			xdr_RENAME3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		PURGE_ATTRCACHE(odbdp);
		PURGE_ATTRCACHE(ndbdp);
		runlock(odrp);
		if (odrp != ndrp) {
			mutex_exit(&nfs3_linkrename_lock);
			runlock(ndrp);
		}
		return (error);
	}

	error = geterrno3(res.status);

	/*
	 * nfs3_linkrename_lock must be held here because
	 * nfs3_cache_wcc_data may result in a call to
	 * set_attrcache_time which will release and reacquire
	 * r_lock if the data for the file must be flushed.
	 */
	if (!error) {
		fwcc = &res.resok.fromdir_wcc;
		twcc = &res.resok.todir_wcc;
	} else {
		fwcc = &res.resfail.fromdir_wcc;
		twcc = &res.resfail.todir_wcc;
	}
	rlock_held = 1;
	nfs3_cache_wcc_data(odbdp, fwcc, cr, &rlock_held);
	if (rlock_held)
		runlock(odrp);
	if (odbdp != ndbdp) {
		rlock_held = 1;
		nfs3_cache_wcc_data(ndbdp, twcc, cr, &rlock_held);
		if (rlock_held)
			runlock(ndrp);
		mutex_exit(&nfs3_linkrename_lock);
	}
	if (!error) {
		if (BHVTOR(odbdp)->r_dir != NULL)
			nfs_purge_rddir_cache(odbdp);
		if (odbdp != ndbdp) {
			if (BHVTOR(ndbdp)->r_dir != NULL)
				nfs_purge_rddir_cache(ndbdp);
		}
	}

	return (error);
}

static int
nfs3_mkdir(bhv_desc_t *dbdp, char *nm, struct vattr *va, vnode_t **vpp,
	cred_t *cr)
{
	int rlock_held;
	rnode_t *drp = BHVTOR(dbdp);
	int error;
	MKDIR3args args;
	MKDIR3res res;
	int douprintf;
	struct vattr vattr;
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	bhv_desc_t *bdp;

	_NFS_IACCESS(dbdp, VEXEC | VWRITE, cr);

	setdiropargs3(&args.where, nm, dbdp);

	/*
	 * Decide what the group-id and set-gid bit of the created directory
	 * should be.  May have to do a setattr to get the gid right.
	 */
	va->va_gid = setdirgid3(dbdp);
	va->va_mode = setdirmode3(dbdp, va->va_mode);
	va->va_mask |= AT_MODE|AT_GID;

	if (((cr->cr_uid > 0xffff) || (va->va_gid > 0xffff)) &&
	    (VN_BHVTOMI(dbdp)->mi_flags & MIF_SHORTUID))
	{
		/* trying to change ower or group, the fs is mounted shortuid
		 * and the user or group id won't fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}

	vattr_to_sattr3(va, &args.attributes);

	dnlc_remove(dvp, nm);

	_NFS_IACCESS(dbdp, VEXEC | VWRITE, cr);

	douprintf = 1;
	error = rlock_nohang(drp);
	if (error) {
		return(error);
	}
	error = rfs3call(VN_BHVTOMI(dbdp), NFSPROC3_MKDIR,
			xdr_MKDIR3args, (caddr_t)&args,
			xdr_MKDIR3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		PURGE_ATTRCACHE(dbdp);
		runlock(drp);
		return (error);
	}

	rlock_held = 1;
	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dbdp, &res.resok.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		if (BHVTOR(dbdp)->r_dir != NULL)
			nfs_purge_rddir_cache(dbdp);

		if (!res.resok.obj.handle_follows) {
			error = nfs3lookup(dbdp, nm, vpp, NULL, 0, NULL, cr);
			if (error)
				return (error);
			bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(*vpp),
						     &nfs3_vnodeops);
			ASSERT(bdp != NULL);
		} else {
			if (res.resok.obj_attributes.attributes) {
				error = makenfs3node(&res.resok.obj.handle,
						&res.resok.obj_attributes.attr,
						dvp->v_vfsp, &bdp, cr);
				if (error == EAGAIN) {
#ifdef DEBUG
					printf("nfs3_mkdir: mknfsnode returns EAGAIN, retrying\n");
#endif /* DEBUG */
				}
				*vpp = BHV_TO_VNODE(bdp);
			} 
			if (!res.resok.obj_attributes.attributes || 
			    (error == EAGAIN)) {
				(void) makenfs3node(&res.resok.obj.handle, NULL,
						    dvp->v_vfsp, &bdp, cr);
				*vpp = BHV_TO_VNODE(bdp);
				if ((*vpp)->v_type == VNON) {
					vattr.va_mask = AT_TYPE;
					error = nfs3getattr(bdp, &vattr, 0, cr);
					if (error) {
						VN_RELE(*vpp);
						return (error);
					}
				}
			}
			dnlc_enter(dvp, nm, bdp, NOCRED);
		}
		if (va->va_gid != BHVTOR(bdp)->r_attr.va_gid) {
			va->va_mask = AT_GID;
			(void) nfs3setattr(bdp, va, 0, cr);
		}
	} else {
		nfs3_cache_wcc_data(dbdp, &res.resfail.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
	}

	return (error);
}

static int
nfs3_rmdir(bhv_desc_t *dbdp, char *nm, vnode_t *cdir, cred_t *cr)
{
	int rlock_held;
	rnode_t *drp = BHVTOR(dbdp);
	int error;
	RMDIR3args args;
	RMDIR3res res;
	vnode_t *vp;
	int douprintf;
	bhv_desc_t *ndp;

	_NFS_IACCESS(dbdp, VEXEC | VWRITE, cr);

	/*
	 * Attempt to prevent a rmdir(".") from succeeding.
	 */
	error = nfs3lookup(dbdp, nm, &vp, NULL, 0, NULL, cr);
	if (error)
		return (error);

	ndp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &nfs3_vnodeops);
	ASSERT(ndp != NULL);

	if (error = _MAC_NFS_IACCESS(ndp, MACWRITE, cr)) {
		VN_RELE(vp);
		return (error);
	}

	if (vp == cdir) {
		VN_RELE(vp);
		return (EINVAL);
	}

	dnlc_purge_vp(vp);

	VN_RELE(vp);

	setdiropargs3(&args.object, nm, dbdp);

	douprintf = 1;
	error = rlock_nohang(drp);
	if (error) {
		return(error);
	}
	error = rfs3call(VN_BHVTOMI(dbdp), NFSPROC3_RMDIR,
			xdr_RMDIR3args, (caddr_t)&args,
			xdr_RMDIR3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		PURGE_ATTRCACHE(dbdp);
		runlock(drp);
		return (error);
	}

	rlock_held = 1;
	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dbdp, &res.resok.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		if (BHVTOR(dbdp)->r_dir != NULL)
			nfs_purge_rddir_cache(dbdp);
	} else {
		nfs3_cache_wcc_data(dbdp, &res.resfail.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		if (error == ENOTEMPTY)
			error = EEXIST;
		PURGE_STALE_FH(error, dbdp, cr);
	}

	return (error);
}

static int
nfs3_symlink(bhv_desc_t *dbdp, char *lnm, struct vattr *tva, char *tnm,
	cred_t *cr)
{
	int rlock_held;
	rnode_t *drp = BHVTOR(dbdp);
	int error;
	SYMLINK3args args;
	SYMLINK3res res;
	int douprintf;
	mntinfo_t *mi;
	vnode_t *vp = NULL;
	rnode_t *rp;
	char *contents;
	vnode_t *dvp = BHV_TO_VNODE(dbdp);
	bhv_desc_t *bdp;

	_NFS_IACCESS(dbdp, VEXEC | VWRITE, cr);

	mi = VN_BHVTOMI(dbdp);

	if (!(mi->mi_flags & MIF_SYMLINK))
		return (EOPNOTSUPP);

	if (((cr->cr_uid > 0xffff) || (cr->cr_gid > 0xffff)) &&
	    (mi->mi_flags & MIF_SHORTUID))
	{
		/* trying to change ower or group, the fs is mounted shortuid
		 * and the user or group id won't fit in 16bits.  Complain.
		 */
		return(EOVERFLOW);
	}

	setdiropargs3(&args.where, lnm, dbdp);
	vattr_to_sattr3(tva, &args.symlink.symlink_attributes);
	args.symlink.symlink_data = tnm;

	dnlc_remove(dvp, lnm);

	_NFS_IACCESS(dbdp, VEXEC | VWRITE, cr);

	douprintf = 1;
	error = rlock_nohang(drp);
	if (error) {
		return(error);
	}
	error = rfs3call(mi, NFSPROC3_SYMLINK,
			xdr_SYMLINK3args, (caddr_t)&args,
			xdr_SYMLINK3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		PURGE_ATTRCACHE(dbdp);
		runlock(drp);
		return (error);
	}

	rlock_held = 1;
	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dbdp, &res.resok.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		if (BHVTOR(dbdp)->r_dir != NULL)
			nfs_purge_rddir_cache(dbdp);

		if (res.resok.obj.handle_follows) {
			if (res.resok.obj_attributes.attributes) {
				error == makenfs3node(&res.resok.obj.handle,
						&res.resok.obj_attributes.attr,
						dvp->v_vfsp, &bdp, cr);
				if (error != EAGAIN) {
					vp = BHV_TO_VNODE(bdp);
				}
			} 
			if (!res.resok.obj_attributes.attributes || 
			    (error == EAGAIN)) {
				(void) makenfs3node(&res.resok.obj.handle, NULL,
						dvp->v_vfsp, &bdp, cr);
				vp = BHV_TO_VNODE(bdp);
				vp->v_type = VLNK;
				vp->v_rdev = 0;
			}
			dnlc_enter(dvp, lnm, bdp, NOCRED);
			rp = BHVTOR(bdp);
			if (nfs3_do_symlink_cache &&
			    rp->r_symlink.contents == NULL) {
				contents = (char *) kmem_alloc(MAXPATHLEN,
								KM_NOSLEEP);
				if (contents != NULL) {
					mutex_enter(&rp->r_statelock);
					if (rp->r_symlink.contents == NULL) {
						rp->r_symlink.len = strlen(tnm);
						bcopy(tnm, contents,
						    rp->r_symlink.len);
						rp->r_symlink.contents =
								    contents;
						rp->r_symlink.size = MAXPATHLEN;
						mutex_exit(&rp->r_statelock);
					} else {
						mutex_exit(&rp->r_statelock);
						kmem_free(contents, MAXPATHLEN);
					}
				}
			}
			VN_RELE(vp);
		}
	} else {
		nfs3_cache_wcc_data(dbdp, &res.resfail.dir_wcc, cr, &rlock_held);
		if (rlock_held)
			runlock(drp);
		PURGE_STALE_FH(error, dbdp, cr);
		if (error == EOPNOTSUPP)
			atomicClearUint(&mi->mi_flags, MIF_SYMLINK);
	}

	return (error);
}

int 
nfs3_xlate_rdc(rddir_cache *rdc, irix5_dirent_t *dpout)
{
	irix5_dirent_t	*dp5 = dpout;
	struct dirent	*dp;
	int		 newentlen = 0;
	int 		 i;

	ASSERT(dp5 != NULL);

	/* copy each entry into the new buffer */

	for (dp = (struct dirent *)rdc->entries; 
		(char *)dp - rdc->entries <  rdc->entlen; 
			dp = (struct dirent *)((char *)dp + dp->d_reclen)) {
		dp5->d_ino = dp->d_ino;
		dp5->d_off = dp->d_off;
		for (i = 0; dp5->d_name[i] = dp->d_name[i]; i++) 
			;
		dp5->d_reclen = IRIX5_DIRENTSIZE(i);
		newentlen += dp5->d_reclen;
		dp5 = (irix5_dirent_t *)((char *)dp5 + dp5->d_reclen);
	}
	/* return the size of the used portion of the buffer */
	ASSERT(newentlen <= rdc->entlen);
	return (newentlen);
}

#ifdef NOTDEF
/*
 * Read directory entries.
 * There are some weird things to look out for here.  The uio_offset
 * field is either 0 or it is the offset returned from a previous
 * readdir.  It is an opaque value used by the server to find the
 * correct directory block to read. The count field is the number
 * of blocks to read on the server.  This is advisory only, the server
 * may return only one block's worth of entries.  Entries may be compressed
 * on the server.
 */
#define	NAMEB	16
static int
nfs3_dump_rdc(rddir_cache *rdc, u_char abi)
{
    int		i;
    u_char	*p;
    char	nameb[NAMEB];

    if (ABI_IS_IRIX5_64(abi) || ABI_IS_IRIX5_N32(abi)) {
	dirent_t	*dp;
	printf ("ABI_IS_IRIX5_64 0x%x\n", rdc);
	for (i = 0, p = (u_char *)rdc->entries;
	    ((char *)p - (char *)rdc->entries) < rdc->entlen; i++, p++) {
		printf (" %02x ", *p & 0xff);
		if ((i % 16) == 15)
		    printf ("\n");
	}
	printf ("\n");
	dp = (dirent_t *)rdc->entries;
	printf ("ino %ld off %d reclen %d\n",
	    dp->d_ino, (u_int)dp->d_off, (unsigned short)dp->d_reclen);
    }
    if (ABI_IS_IRIX5(abi)) {
	irix5_dirent_t	*dp;
	printf ("ABI_IS_IRIX5 0x%x\n", rdc);
	for (dp = (irix5_dirent_t *)rdc->entries;
	    ((char *)dp - (char *)rdc->entries) < rdc->entlen;
	    dp = (irix5_dirent_t *)((char *)dp + dp->d_reclen)) {
	    printf ("dp 0x%x ino %d off %d reclen %d ",
		    dp, dp->d_ino, (u_int)dp->d_off, dp->d_reclen);
	    bzero (&nameb[0], NAMEB);
	    for (i = 0;
		(i < (dp->d_reclen - (sizeof(dp->d_ino) + sizeof(dp->d_off) + sizeof (dp->d_reclen)))) && (i < NAMEB-1);
		i++) {
		nameb[i] = dp->d_name[i];
	    }
	    printf ("%s", &nameb[0]);
	    printf ("\n");
	}
    }
    return 0;
}
#endif /* NOTDEF */

static int
nfs3_readdir(bhv_desc_t *bdp, struct uio *uiop, cred_t *cr, int *eofp)
{
	int error;
	u_int count;
	rnode_t *rp;
	rddir_cache *rdc;
	rddir_cache *nrdc;
	rddir_cache *rrdc;
	u_char	abi;

#ifdef KTRACE
	TRACE1(T_NFS3_READDIR, T_START, vp);
#endif /* KTRACE  */
	abi = GETDENTS_ABI(get_current_abi(), uiop);
	/*
	 * Make sure that the directory cache is valid.
	 */
	rp = BHVTOR(bdp);
	if (rp->r_dir != NULL) {
		error = nfs3_validate_caches(bdp, cr);
		if (error)
		{
#ifdef KTRACE
		    TRACE2(T_NFS3_READDIR, T_EXIT_ERROR, vp, error);
#endif /* KTRACE  */
			return (error);
		}
	}

	/*
	 * Limit the user request (and thus our readdir cache entry size
	 * and our eventual READDIR[PLUS] size) to the server transfer
	 * size or our library's READDIRSZ of 16K.
	 */ 
	count = MIN(uiop->uio_iov->iov_len, MAX(RTOMI(rp)->mi_tsize, 16384));
	ASSERT(count != 0);

	nrdc = NULL;
top:
	/*
	 * Short circuit last readdir which always returns 0 bytes.
	 * This can be done after the directory has been read through
	 * completely at least once.  This will set r_direof which
	 * can be used to find the value of the last cookie.
	 */
	mutex_enter(&rp->r_statelock);
	if (rp->r_direof != NULL &&
	    uiop->uio_offset == rp->r_direof->nfs3_ncookie) {
		mutex_exit(&rp->r_statelock);
		if (eofp)
			*eofp = 1;
		if (nrdc != NULL)
			kmem_free((caddr_t)nrdc, sizeof (*nrdc));
#ifdef KTRACE
		TRACE1(T_NFS3_READDIR, T_EXIT_OK, vp);
#endif /* KTRACE  */
		return (0);
	}
	/*
	 * Look for a cache entry.  Cache entries are identified
	 * by the NFS cookie value and the byte count requested.
	 */
look:
	rdc = rp->r_dir;
	while (rdc != NULL) {
		/*
		 * To NFS 3, the cookie is an opaque 8 byte entity.  To
		 * the rest of the system, the cookie is really an
		 * offset.  Thus, NFS 3 stores the cookie in off_t
		 * sized elements and compares them to off_t offsets.
		 * This is valid as long as the client makes no other
		 * assumptions about the values of cookies.  The only
		 * valid tests are equal and not equal.
		 */
		if (rdc->nfs3_cookie == uiop->uio_offset &&
		    rdc->buflen == count) {
			/*
			 * If the cache entry is in the process of being
			 * filled in, wait until this completes.  The
			 * RDDIRWAIT bit is set to indicate that someone
			 * is waiting and then the thread currently
			 * filling the entry is done, it should do a
			 * cv_broadcast to wakeup all of the threads
			 * waiting for it to finish.
			 */
			if  (rdc->flags & RDDIR) {
				rdc->flags |= RDDIRWAIT;
				if (sv_wait_sig(&rdc->cv, PZERO,
					  &rp->r_statelock, 0)) {
					/*
					 * We got interrupted, probably
					 * the user typed ^C or an alarm
					 * fired.  We remove our reference
					 * to the cache entry and then
					 * free the new entry if we allocated
					 * one.
					 */
					if (nrdc != NULL) {
						kmem_free((caddr_t)nrdc,
							    sizeof (*nrdc));
					}
#ifdef KTRACE
					TRACE2(T_NFS3_READDIR, T_EXIT_ERROR_1, vp, EINTR);
#endif /* KTRACE  */
					return (EINTR);
				} else
					mutex_enter(&rp->r_statelock);
				goto look;
			}
			/*
			 * Check to see if a readdir is required to
			 * fill the entry.  If so, mark this entry
			 * as being filled, remove our reference,
			 * and branch to the code to fill the entry.
			 */
			if (rdc->flags & RDDIRREQ) {
				rdc->flags &= ~RDDIRREQ;
				rdc->flags |= RDDIR;
				if (nrdc != NULL) {
					kmem_free((caddr_t)nrdc,
						    sizeof (*nrdc));
				}
				nrdc = rdc;
				mutex_exit(&rp->r_statelock);
				goto bottom;
			}
			/*
			 * If an error occurred while attempting
			 * to fill the cache entry, just return it.
			 */
			if (rdc->error) {
				error = rdc->error;
				mutex_exit(&rp->r_statelock);
				if (nrdc != NULL) {
					kmem_free((caddr_t)nrdc,
						    sizeof (*nrdc));
				}
#ifdef KTRACE
				TRACE2(T_NFS3_READDIR, T_EXIT_ERROR_2, vp, error);
#endif /* KTRACE  */
				return (error);
			}
			/*
			 * The cache entry is complete and good,
			 * copyout the dirent structs to the calling
			 * thread.
			 * If the caller is o32 abi we need to 
			 * to translate from 64 bit entries.
			 * rather than change this to use xlate copyout (which
			 * just does the translation in core and then does a 
			 * copyout, we'll do the translation right now if 
			 * necessary and call uiomove with a different 
			 * (translated) source buffer.
			 */

			if (ABI_IS_IRIX5_64(abi) || ABI_IS_IRIX5_N32(abi)) {
				 error = uiomove(rdc->entries, rdc->entlen,
						UIO_READ, uiop);
			} else {
				int newbuflen,tmplen;
				irix5_dirent_t *newbufp;
				if (rdc->entlen > rdc->buflen) {
					printf("WRN:entlen(%d) > buflen(%d)\n",
						rdc->buflen, rdc->entlen);
				}
				tmplen = MAX(rdc->buflen, rdc->entlen);
				newbufp = (irix5_dirent_t  *) 
					  kmem_alloc(tmplen, KM_NOSLEEP);
				if (newbufp == NULL) {
					error = ENOMEM;
				} else {
					newbuflen = nfs3_xlate_rdc(rdc,
						   	  	   newbufp);
					error = uiomove(newbufp, newbuflen,
						UIO_READ, uiop);
					kmem_free(newbufp, tmplen);
				}
			}
			/*
			 * If no error occurred during the copyout,
			 * update the offset in the uio struct to
			 * contain the value of the next NFS 3 cookie
			 * and set the eof value appropriately.
			 */
			if (!error) {
				uiop->uio_offset = rdc->nfs3_ncookie;
				if (eofp)
					*eofp = rdc->eof;
			}
			/*
			 * Decide whether to do readahead.  Don't if
			 * have already read to the end of directory.
			 */
			if (rdc->eof) {
				mutex_exit(&rp->r_statelock);
				if (nrdc != NULL) {
					kmem_free((caddr_t)nrdc,
						    sizeof (*nrdc));
				}
#ifdef KTRACE
				TRACE2(T_NFS3_READDIR, error ? T_EXIT_ERROR_3: T_EXIT_OK_1, vp, error);
#endif /* KTRACE  */
				return (error);
			}
			/*
			 * Now look for a readahead entry.
			 */
			rrdc = rp->r_dir;
			while (rrdc != NULL) {
				if (rrdc->nfs3_cookie == rdc->nfs3_ncookie &&
				    rrdc->buflen == count)
					break;
				rrdc = rrdc->next;
			}
			/*
			 * Check to see whether we found an entry
			 * for the readahead.  If so, we don't need
			 * to do anything further, so free the new
			 * entry if one was allocated.  Otherwise,
			 * allocate a new entry, add it to the cache,
			 * and then initiate an asynchronous readdir
			 * operation to fill it.
			 */
			if (rrdc != NULL) {
				if (nrdc != NULL) {
					kmem_free((caddr_t)nrdc,
						    sizeof (*nrdc));
				}
			} else {
				if (nrdc != NULL)
					rrdc = nrdc;
				else {
					rrdc = (rddir_cache *)
					    kmem_alloc(sizeof (*rrdc),
							KM_NOSLEEP);
				}
				if (rrdc != NULL) {
					rrdc->nfs3_cookie = rdc->nfs3_ncookie;
					rrdc->buflen = count;
					rrdc->flags = RDDIR;
					sv_init(&rrdc->cv, SV_DEFAULT,
					    "rddir_cache cv");
					rrdc->next = rp->r_dir;
					rp->r_dir = rrdc;
					mutex_exit(&rp->r_statelock);
					nfs_async_readdir(bdp, rrdc, cr,
						    do_nfs3readdir, abi);
#ifdef KTRACE
					TRACE2(T_NFS3_READDIR, error ? T_EXIT_ERROR_4: T_EXIT_OK_2, vp, error);
#endif /* KTRACE  */
					return (error);
				}
			}
			mutex_exit(&rp->r_statelock);
#ifdef KTRACE
			TRACE2(T_NFS3_READDIR, error ? T_EXIT_ERROR_5: T_EXIT_OK_3, vp, error);
#endif /* KTRACE  */
			return (error);
		}
		rdc = rdc->next;
	}

	/*
	 * Didn't find an entry in the cache.  Construct a new empty
	 * entry and link it into the cache.  Other processes attempting
	 * to access this entry will need to wait until it is filled in.
	 *
	 * Since kmem_alloc may block, another pass through the cache
	 * will need to be taken to make sure that another process
	 * hasn't already added an entry to the cache for this request.
	 */
	if (nrdc == NULL) {
		mutex_exit(&rp->r_statelock);
		nrdc = (rddir_cache *)kmem_alloc(sizeof (*nrdc),
							KM_SLEEP);
		nrdc->nfs3_cookie = uiop->uio_offset;
		nrdc->buflen = count;
		nrdc->flags = RDDIR;
		sv_init(&nrdc->cv, SV_DEFAULT, "rddir_cache cv");
		goto top;
	}

	/*
	 * Add this entry to the cache.
	 */
	nrdc->next = rp->r_dir;
	rp->r_dir = nrdc;
	mutex_exit(&rp->r_statelock);

bottom:
	/*
	 * Do the readdir.  This routine decides whether to use
	 * READDIR or READDIRPLUS.
	 */
	error = do_nfs3readdir(bdp, nrdc, cr, abi);

	/*
	 * If this operation failed, just return the error which occurred.
	 */
	if (error != 0) {
#ifdef KTRACE
		TRACE2(T_NFS3_READDIR, T_EXIT_ERROR_6, vp, error);
#endif /* KTRACE  */
		return (error);
	}

	/*
	 * Since the RPC operation will have taken sometime and blocked
	 * this process, another pass through the cache will need to be
	 * taken to find the correct cache entry.  It is possible that
	 * the correct cache entry will not be there (although one was
	 * added) because the directory changed during the RPC operation
	 * and the readdir cache was flushed.  In this case, just start
	 * over.  It is hoped that this will not happen too often... :-)
	 */
	nrdc = NULL;
	goto top;
	/* NOTREACHED */
}

static int
do_nfs3readdir(bhv_desc_t *bdp, rddir_cache *rdc, cred_t *cr, u_char abi)
{
	int error;
	rnode_t *rp;
	mntinfo_t *mi;

	rp = BHVTOR(bdp);
	mi = RTOMI(rp);

	/*
	 * Issue the proper request.  READDIRPLUS is used unless
	 * the server does not support READDIRPLUS or until the
	 * directory has been completely read once using READDIRPLUS.
	 * Once the directory has been completely read, the DNLC is
	 * assumed to be loaded and any further use of READDIRPLUS
	 * is just unnecessary overhead.  READDIR is used to retrieve
	 * directory entries which we do not find cached.  If the
	 * directory cache and the DNLC are flushed because the
	 * directory has changed, READDIRPLUS is used once again to
	 * repopulate the DNLC.
	 */
	if (!(mi->mi_flags & MIF_READDIR) && !(rp->r_flags & REOF)) {
		nfs3readdirplus(bdp, rdc, cr, abi);
		if (rdc->error == EOPNOTSUPP)
			nfs3readdir(bdp, rdc, cr, abi);
	} else
		nfs3readdir(bdp, rdc, cr, abi);

	mutex_enter(&rp->r_statelock);
	rdc->flags &= ~RDDIR;
	if (rdc->flags & RDDIRWAIT) {
		rdc->flags &= ~RDDIRWAIT;
		sv_broadcast(&rdc->cv);
	}
	error = rdc->error;
	if (error)
		rdc->flags |= RDDIRREQ;
	mutex_exit(&rp->r_statelock);

	return (error);
}

static void
nfs3readdir(bhv_desc_t *bdp, rddir_cache *rdc, cred_t *cr, u_char abi)
{
	int rlock_held;
	int error;
	READDIR3args args;
	READDIR3res res;
	rnode_t *rp;
	u_int count;
	int douprintf;

#ifdef KTRACE
	TRACE1(T_NFS3_READDIR, T_START, vp);
#endif /* KTRACE  */
	count = rdc->buflen;

	rp = BHVTOR(bdp);

	args.dir = *rtofh3(rp);
	args.cookie = (cookie3)rdc->nfs3_cookie;
	bcopy((caddr_t)rp->r_cookieverf, (caddr_t) args.cookieverf, 
			sizeof (cookieverf3));
	args.count = count;

	res.resok.reply.entries = (entry3 *)kmem_alloc(count, KM_SLEEP);
	res.resok.size = count;
	res.resok.cookie = args.cookie;
	res.resok.rddir_abi = abi;

	douprintf = 1;
	error = rlock_nohang(rp);
	if (error) {
		kmem_free((caddr_t)res.resok.reply.entries, count);
		rdc->entries = NULL;
		rdc->error = error;
		return;
	}
	error = rfs3call(RTOMI(rp), NFSPROC3_READDIR,
			xdr_READDIR3args, (caddr_t)&args,
			xdr_READDIR3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (!error) {
		error = geterrno3(res.status);
		rlock_held = 1;
		if (!error) {
			nfs3_cache_post_op_attr(bdp, &res.resok.dir_attributes,
						cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			rdc->nfs3_ncookie = (off_t)res.resok.cookie;
			bcopy((caddr_t)res.resok.cookieverf,
				(caddr_t)rp->r_cookieverf, sizeof(cookieverf3));
			rdc->entries = (char *)res.resok.reply.entries;
			if (res.resok.reply.eof) {
				rdc->eof = 1;
				mutex_enter(&rp->r_statelock);
				rp->r_direof = rdc;
				mutex_exit(&rp->r_statelock);
				rsetflag(rp, REOF);
			} else
				rdc->eof = 0;
			rdc->entlen = res.resok.size;
			rdc->error = 0;
		} else {
			nfs3_cache_post_op_attr(bdp, &res.resfail.dir_attributes,
						cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			PURGE_STALE_FH(error, bdp, cr);
		}
	} else {
		runlock(rp);
	}
	if (error) {
		kmem_free((caddr_t)res.resok.reply.entries, count);
		rdc->entries = NULL;
		rdc->error = error;
	}
#ifdef KTRACE
	TRACE2(T_NFS3READDIR, error ? T_EXIT_ERROR_3: T_EXIT_OK_1, vp, error);
#endif /* KTRACE  */
}
#ifdef nextdp
#undef nextdp
#endif
#define	nextdp(dp)	((struct dirent *)(roundto64int((char *)(dp) + (dp)->d_reclen)))

/*
 * Read directory entries.
 * There are some weird things to look out for here.  The uio_offset
 * field is either 0 or it is the offset returned from a previous
 * readdir.  It is an opaque value used by the server to find the
 * correct directory block to read. The count field is the number
 * of blocks to read on the server.  This is advisory only, the server
 * may return only one block's worth of entries.  Entries may be compressed
 * on the server.
 */
static void
nfs3readdirplus(bhv_desc_t *bdp, rddir_cache *rdc, cred_t *cr, u_char abi)
{
	int rlock_held;
	int error;
	READDIRPLUS3args args;
	READDIRPLUS3res res;
	rnode_t *rp;
	mntinfo_t *mi;
	u_int count;
	int douprintf;
	char *buf;
	char *ibufp;
	struct dirent *idp;
	struct dirent *odp;
	post_op_attr *atp;
	post_op_fh3 *fhp;
	int isize;
	int osize;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	vnode_t *nvp = NULL;
	bhv_desc_t *nbdp;
	off_t loff;


#ifdef KTRACE
	TRACE1(T_NFS3READDIRPP, T_START, vp);
#endif /* KTRACE  */
	count = rdc->buflen;

	rp = BHVTOR(bdp);
	mi = RTOMI(rp);

	args.dir = *rtofh3(rp);
	loff = args.cookie = (cookie3)rdc->nfs3_cookie;
	bcopy((caddr_t)rp->r_cookieverf, (caddr_t)args.cookieverf,
		sizeof(cookieverf3));
	args.dircount = count;
	args.maxcount = mi->mi_tsize;

	res.resok.reply.entries = (entryplus3 *)kmem_alloc(args.maxcount, KM_SLEEP);
	res.resok.size = args.maxcount;
	res.resok.rddir_abi = abi;

	douprintf = 1;
	error = rlock_nohang(rp);
	if (error) {
		rdc->entries = NULL;
		rdc->error = error;
		goto out;
	}
	error = rfs3call(mi, NFSPROC3_READDIRPLUS,
			xdr_READDIRPLUS3args, (caddr_t)&args,
			xdr_READDIRPLUS3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		rdc->entries = NULL;
		rdc->error = error;
		runlock(rp);
		goto out;
	}

	rlock_held = 1;
	error = geterrno3(res.status);
	if (error) {
		nfs3_cache_post_op_attr(bdp, &res.resfail.dir_attributes, cr,
			&rlock_held);
		if (rlock_held)
			runlock(rp);
		PURGE_STALE_FH(error, bdp, cr);
		if (error == EOPNOTSUPP)
			atomicSetUint(&mi->mi_flags, MIF_READDIR);
		rdc->entries = NULL;
		rdc->error = error;
		goto out;
	}

	nfs3_cache_post_op_attr(bdp, &res.resok.dir_attributes, cr,
		&rlock_held);
	if (rlock_held)
		runlock(rp);

	buf = (char *)kmem_alloc(count, KM_SLEEP);

	odp = (struct dirent *)buf;
	ibufp = (char *)res.resok.reply.entries;
	isize = res.resok.size;
	osize = 0;
	while (isize > 0) {
		idp = (struct dirent *)roundto64int(ibufp);

		isize -= (char *)idp - (char *)ibufp;
		if (osize + idp->d_reclen > count) {
			res.resok.reply.eof = FALSE;
			break;
		}
		bcopy((char *)idp, (char *)odp, idp->d_reclen);
		loff = idp->d_off;
		osize += odp->d_reclen;
		odp = nextdp(odp);
		isize -= idp->d_reclen;
		ibufp += idp->d_reclen;

		atp = (post_op_attr *)roundto64int(ibufp);
		isize -= (char *)atp - (char *)ibufp;
		ibufp = (char *)atp;
		if (!atp->attributes) {
			ibufp += sizeof (atp->attributes);
			isize -= sizeof (atp->attributes);
		} else {
			ibufp += sizeof (*atp);
			isize -= sizeof (*atp);
		}

		fhp = (post_op_fh3 *)ibufp;

		if (ibufp != (char *)roundto32int(ibufp)) {
#ifdef DEBUG
			printf("nfs3readdirplus Doing this weird alignment thing\n");
#endif
			fhp = (post_op_fh3 *)roundto64int(ibufp);
			isize -= (char *)fhp - ibufp;
			ibufp = (char *)fhp;
                }

		if (!fhp->handle_follows) {
			ibufp += sizeof (fhp->handle_follows);
			isize -= sizeof (fhp->handle_follows);
			continue;
		}
		ibufp += sizeof (*fhp);
		isize -= sizeof (*fhp);

		/*
		 * Add this entry to the DNLC if it is "." and
		 * we have attributes.  Otherwise, we end up
		 * polluting the DNLC with "." entries or not
		 * being able to determine what type of file
		 * this entry references.
		 * jws - also, don't enter ".." here
		 */
		if (atp->attributes && strcmp(idp->d_name, ".") != 0) {
			ASSERT(atp->attr.type >= NF3NONE 
				&& atp->attr.type <= NF3FIFO);
			error = makenfs3node(&fhp->handle, &atp->attr,
					vp->v_vfsp, &nbdp, cr);
			if (!error) {
				/* XXX should be setting vtype here perhaps */
				nvp = BHV_TO_VNODE(nbdp);
				dnlc_remove(vp, idp->d_name);
				dnlc_enter(vp, idp->d_name, nbdp, NOCRED);
				VN_RELE(nvp);
#ifdef DEBUG
			} else {
				printf("ERROR in makenfs3node call == %d\n",
					error);
#endif
			} 
		}
	}

	rdc->nfs3_ncookie = (off_t)loff;
	bcopy((caddr_t)res.resok.cookieverf, (caddr_t)rp->r_cookieverf,
		sizeof(cookieverf3));
	rdc->entries = buf;
	if (res.resok.reply.eof) {
		rdc->eof = 1;
		mutex_enter(&rp->r_statelock);
		rp->r_direof = rdc;
		mutex_exit(&rp->r_statelock);
		rsetflag(rp, REOF);
	} else
		rdc->eof = 0;
	rdc->entlen = osize;
	rdc->error = 0;

out:
	kmem_free((caddr_t)res.resok.reply.entries, args.maxcount);
#ifdef KTRACE
	TRACE2(T_NFS3READDIRPP, error ? T_EXIT_ERROR_3: T_EXIT_OK_1, vp, error);
#endif /* KTRACE  */
}

/*
 * Convert from file system blocks to device blocks
 */
/* ARGSUSED */
static int
nfs3_bmap(bhv_desc_t *bdp, off_t offset, ssize_t count, int flag, struct cred *cred, struct bmapval *bmv, int *nbmv)
{
	register struct bmapval *rabmv;
	register int bsize;		/* server's block size in bytes */
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register struct rnode *rp = BHVTOR(bdp);
	register off_t size;
	register int n = *nbmv;

	ASSERT(n > 0);
	bsize = rtoblksz(rp);

	bmv->length = BTOBBT(bsize);
	bmv->bsize = bsize;
	bmv->bn = BTOBBT(offset / bsize * bsize);
	bmv->offset = bmv->bn;
	bmv->pmp = NULL;
	size = BBTOB(bmv->bn) + bsize;
	bmv->pboff = offset % bsize;
	bmv->pbsize = MIN(bsize - bmv->pboff, count);
	bmv->pbdev = vp->v_rdev;
	bmv->eof = 0;

	while (--n > 0 && size < MAX(rp->r_size, rp->r_localsize)) {
		rabmv = bmv + 1;

		rabmv->length = bmv->length;
		rabmv->bn = bmv->bn + bmv->length;
		rabmv->offset = rabmv->bn;
		rabmv->pmp = NULL;
		rabmv->pbsize = rabmv->bsize = bsize;
		rabmv->pboff = 0;
		rabmv->pbdev = vp->v_rdev;
		rabmv->eof = 0;
		size += bsize;

		bmv++;
	}

	if (size >= MAX(rp->r_size, rp->r_localsize)) {
		bmv->eof |= BMAP_EOF;
		*nbmv -= n;
	}

	if (rp->r_localsize < size) {
		rp->r_localsize = size;
	}

	return (0);
}

/*
 * Bump iocount
 */
static void
nfs3_riostart(struct rnode *rp)
{
	mntinfo_t *mi= RTOMI(rp);
	extern int nfs3_max_mntio;

	mutex_enter(&mi->mi_async_lock);
	rp->r_iocount++;
	mutex_exit(&mi->mi_async_lock);
}

/*
 * Drop the rnode's i/o count and if it goes to zero, wakeup anyone
 * interested in flushing i/o.  Unlock the rnode in any case.
 */
static void
nfs3_riodone(struct rnode *rp)
{
	mntinfo_t *mi = RTOMI(rp);
	extern int nfs3_max_mntio;

	mutex_enter(&mi->mi_async_lock);
	ASSERT(rp->r_iocount > 0);
	if (--rp->r_iocount == 0) {
		sv_broadcast(&rp->r_iowait);
	}
	mutex_exit(&mi->mi_async_lock);
}

static void
nfs3_riowait (struct rnode *rp)
{
	mntinfo_t *mi = RTOMI(rp);

	mutex_enter(&mi->mi_async_lock);
	while (rp->r_iocount) {
		sv_wait(&rp->r_iowait, PRIBIO, &mi->mi_async_lock, 0);
		mutex_enter(&mi->mi_async_lock);
	}
	mutex_exit(&mi->mi_async_lock);
}

/*
 * Clean up pending BIO WRITE async requests after a ENOSPC failure.
 */
static void
nospc_cleanup(struct rnode *rp, struct buf *bp)
{
        mntinfo_t *mi = RTOMI(rp);
        struct nfs_async_reqs *async_req;
        struct buf *async_req_bp;

        mutex_enter(&mi->mi_async_lock);

        /*
         * Scan async request list.
         * For each async request matching these conditions :
         *      it is a WRITE NFS_BIO request,
         *      it concerns the rnode rp points to,
         *      the request buffer offset is beyond bp's offset
         * the request buffer is flagged B_ERROR.
         * The request will be aborted when scheduled by nfs3_bio.
         */
        async_req = mi->mi_async_reqs;
        while (async_req  != NULL) {
                async_req_bp = async_req->a_nfs_bp;
                if ((async_req->a_io == NFS_BIO)
                    && (bhvtor(async_req->a_bdp) == rp)
                    && (!(async_req_bp->b_flags & B_READ))
                    && (BBTOOFF(async_req_bp->b_blkno) >= BBTOOFF(bp->b_blkno))) {
                        /* Flag the buffer with B_ERROR */
                        async_req_bp->b_flags |= B_ERROR;
                }

                async_req = async_req->a_next;
        }
        mutex_exit(&mi->mi_async_lock);
}

static int
nfs3_bio(bhv_desc_t *bdp, struct buf *bp, stable_how *stab_comm, cred_t *cr)
{
	register rnode_t *rp = BHVTOR(bdp);
	post_op_attr fa;
	long count, rem;
	int error = 0;
	int read = bp->b_flags & B_READ ? 1 : 0;
	cred_t *cred;
	off_t offset;

	offset = BBTOOFF(bp->b_blkno);
#ifdef KTRACE
	PIDTRACE2(T_NFS3_BIO, T_START, read ? RFS_READ : RFS_WRITE, offset);
#endif /* KTRACE  */
	if (read) {
		mutex_enter(&rp->r_statelock);
		if (rp->r_cred != NULL) {
			cred = rp->r_cred;
			crhold(cred);
		} else {
			rp->r_cred = cr;
			crhold(cr);
			cred = cr;
			crhold(cred);
		}
		mutex_exit(&rp->r_statelock);
	read_again:
		error = bp->b_error = nfs3read(bdp, bp,
					    offset, (long)bp->b_bcount,
					    (unsigned *)&bp->b_resid, 
						cred, &fa,  1);
		crfree(cred);
		if (!error) {
			if (bp->b_resid) {
				/*
				 * Didn't get it all because we hit EOF,
				 * zero all the memory beyond the EOF.
				 */
				/* bzero(rdaddr + */
				bzero(bp->b_un.b_addr +
				    (bp->b_bcount - bp->b_resid),
				    (u_int)bp->b_resid);
			}
			if (bp->b_resid == bp->b_bcount &&
			    offset >= rp->r_size) {
				/*
				 * We didn't read anything at all as we are
				 * past EOF.  Return an error indicator back
				 * but don't destroy the pages (yet).
				 * JWS - WHY NOT?
				 */
				error = NFS_EOF;
			}
		} else if (error == EACCES) {
			mutex_enter(&rp->r_statelock);
			if (cred != cr) {
				if (rp->r_cred != NULL)
					crfree(rp->r_cred);
				rp->r_cred = cr;
				crhold(cr);
				cred = cr;
				crhold(cred);
				mutex_exit(&rp->r_statelock);
				goto read_again;
			}
			mutex_exit(&rp->r_statelock);
		}
	} else {
		if ((!(rp->r_flags & RDONTWRITE)) 
		    && (!(bp->b_flags & B_ERROR))) {
			mutex_enter(&rp->r_statelock);
			if (rp->r_cred != NULL) {
				cred = rp->r_cred;
				crhold(cred);
			} else {
				rp->r_cred = cr;
				crhold(cr);
				cred = cr;
				crhold(cred);
			}
			mutex_exit(&rp->r_statelock);
		write_again:
			/*
			 * Can't use r_size in the MIN macro below because
			 * the MIN macro will cause r_size to be evaluated
			 * twice and there is no lock to prevent r_size from
			 * changing value in between.  Hence, just evaluate
			 * r_size once by storing it in a local variable.
			 * The load from r_size is assumed to be atomic.
			 */
			rem = rp->r_size - offset;
			count = MIN(bp->b_bcount, rem);
			if (count <= 0) {
#ifdef NFSCACHEDEBUG
				cmn_err(CE_WARN, 
					"nfs3_bio: %d length write at %d", 
					count, offset);
#endif /* NFSCACHEDEBUG */
			} else {
				error = nfs3write(bdp, bp->b_un.b_addr, 
						offset, count, cred, stab_comm,
						1, bp);
#ifdef NFSCACHEDEBUG
				if (error)
					cmn_err(CE_WARN,
						"nfs3_bio: write failed %d length %d offset %d error", count, offset, error);
#endif
						
				if (error == EACCES) {
					mutex_enter(&rp->r_statelock);
					if (cred != cr) {
						if (rp->r_cred != NULL)
							crfree(rp->r_cred);
						rp->r_cred = cr;
						crhold(cr);
						crfree(cred);
						cred = cr;
						crhold(cred);
						mutex_exit(&rp->r_statelock);
						goto write_again;
					}
					mutex_exit(&rp->r_statelock);
				}
				if (error != 0 &&
				    error != EINTR &&
				    (error != EACCES ||
				    !(bp->b_flags & B_ASYNC))) {
					if (error == ESTALE) {
						rsetflag(rp, RDONTWRITE);
					}
#ifdef DEBUG
					printf(
					    "NFS3 write error %d on host %s\n",
					    error,
		VN_BHVTOMI(vn_bhv_lookup_unlocked(VN_BHV_HEAD(bp->b_vp), &nfs3_vnodeops))->mi_hostname);
#endif /* DEBUG */
#ifdef NFSCACHEDEBUG
					if (error == EACCES) {
						printf ("nfs3_bio: EACCES\n");
					}
#endif /* NFSCACHEDEBUG */
				}
				bp->b_error = error;
				if (error != 0 && error != EINTR &&
				    (bp->b_flags & B_ASYNC))
					rp->r_error = error;
                                if (error == ENOSPC)
                                        nospc_cleanup(rp, bp);
			}
			crfree(cred);
		} else {
			error = rp->r_error;
		}
	}

	nfs3_riodone(rp);

	if (!error && read) {
		if (fa.attributes) {
       			if (fa.attr.mtime.seconds != rp->r_attr.va_mtime.tv_sec
                   	    || fa.attr.mtime.nseconds 
					!= rp->r_attr.va_mtime.tv_nsec)
				INVAL_ATTRCACHE(bdp);
		}
	}

	if (error != 0 && error != NFS_EOF) {
#ifdef NFSCACHEDEBUG
		printf("in nfs3_bio, setting bp->b_flags to B_ERROR, bp=0x%x\n",
			bp);
		printf("		error = %d\n",error);
#endif /* NFSCACHEDEBUG */
		bp->b_flags |= B_ERROR;
	}
	iodone(bp);

#ifdef KTRACE
	PIDTRACE2(T_NFS3_BIO, T_EXIT_OK, read ? RFS_READ : RFS_WRITE, offset);
#endif /* KTRACE  */
	return (error);
}

/* ARGSUSED */
static int
nfs3_fid(bhv_desc_t *bdp, struct fid **fidpp)
{
	register fid_t *fp;
	nfs_fhandle *fhp;
	register struct rnode *rp;
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);

	/*
	 * We want to return to the caller a file ID which the caller can
	 * later pass to nfs3_vget() (via VFS_VGET()) to get a vnode.  The
	 * trick is to pass one that will be valid after a reboot, system
	 * crash, or umount/mount as well as for the entire time that the file
	 * system is mounted.  It is possible that the file on the server may
	 * be removed.  This should be the only case where the file ID data
	 * is invalid or stale.
	 */

	ASSERT(vp);					/* none of the parameters may be NULL */
	ASSERT(fidpp);

	/*
	 * The nfs3 file handle is composed of a length field and some data.
	 * The data is NFS_FHANDLE_LEN bytes in length.  The actual length of
	 * the file handle is given in fh_len.  This makes the file handle
	 * correspond very nicely to a fid_t.
	 */
	rp = BHVTOR(bdp);
	fhp = &(rp->r_ufh.r_fh3);
	/*
	 * It is possible for the file handle data size to be larger than the
	 * maximum data size for the fid_t (MAXFIDSZ).  If this is so, return
	 * EINVAL.  EINVAL is not a very self-explanatory error number in this
	 * case.  It is more of a catchall.  Print a warning on the console or
	 * system log to indicate the failure.  Hopefully, this will provide
	 * enough information to diagnose the problem.
	 */
	if ( fhp->fh_len > MAXFIDSZ ) {
		cmn_err(CE_WARN, "nfs3_fid: file handle larger than file ID data area");
		return(EINVAL);
	}
	/*
	 * The file handle length has been calculated and is such that the
	 * entire file handle will fit into the data field of the file ID.
	 * Allocate the fid_t, allocating only enough memory to hold fid_len
	 * and the file handle data.  The caller is expected to use freefid()
	 * to free the file ID when it is no longer needed.
	 *
	 * Use a calculation similar to the one use above for the file handle
	 * size to determine the file ID size.  The reason is the same as
	 * above.
	 *
	 * Once the file ID space has been allocated, it can be filled in.
	 * fid_len gets fhp->fh_len (the size of the file handle) and fid_data
	 * gets the file handle data (fh_buf).
	 */
	fp = (fid_t *)kmem_zalloc(sizeof(fid_t) - MAXFIDSZ + fhp->fh_len, KM_SLEEP);
	fp->fid_len = fhp->fh_len;
	bcopy((caddr_t)fhp->fh_buf, (caddr_t)fp->fid_data, fhp->fh_len);
	*fidpp = (struct fid *)fp;
	return (0);
}

/* ARGSUSED */
static int
nfs3_seek(bhv_desc_t *bdp, off_t ooff, off_t *noffp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);

	/*
	 * Because we stuff the readdir cookie into the offset field
	 * someone may attempt to do an lseek with the cookie which
	 * we want to succeed.
	 */
	if (vp->v_type == VDIR)
		return (0);
	return ((*noffp < 0 || *noffp > MAXOFF_T) ? EINVAL : 0);
}

int
nfs3_lockctl(bhv_desc_t *bdp, struct flock *lfp, int cmd, struct cred *cred)
{
	vattr_t attrs;
	int islock;
	int error = 0;
	lockhandle_t lh;
	nfs_fhandle *fh3;
	vnode_t *vp;

	vp = BHV_TO_VNODE(bdp);
	lh.lh_vp = vp;
	lh.lh_servername = vn_bhvtomi(bdp)->mi_hostname;
	/*
	 * lockd servers always expect the file handle to be packaged into
	 * the netobj with the length in n_len and the file handle data in
	 * n_bytes
	 * this is true regarless of the version
	 */
	fh3 = (nfs_fhandle *)bhvtofh(bdp);
	lh.lh_id.n_bytes = fh3->fh_buf;
	lh.lh_id.n_len = fh3->fh_len;
	lh.lh_timeo = vn_bhvtomi(bdp)->mi_timeo;
	islock = (lfp->l_type != F_UNLCK) && (cmd != F_GETLK);
	if (!islock) {
		VOP_FSYNC(vp, FSYNC_INVAL, cred, (off_t)0, (off_t)-1, error);
		/*
		 * On each successful lock request,
		 * invalidate the attributes to ensure
		 * a call to the server on the next
		 * check.  If the file is mapped, also
		 * check the attributes here.
		 */
		INVAL_ATTRCACHE(bdp);
		if (VN_MAPPED(vp)) {
		    (void)nfs3_getattr(bdp, &attrs, 0,
			cred);
		}
	}
	switch (vn_bhvtomi(bdp)->mi_nlmvers) {
		case NLM4_VERS:
			error = klm_lockctl(&lh, lfp, cmd, cred);
			if (error != ENOPKG) {
				break;
			}
			/*
			 * reset the timeout here because klm_lockctl may 
			 * have changed it
			 */
			lh.lh_timeo = vn_bhvtomi(bdp)->mi_timeo;
		case NLM_VERS:
			vn_bhvtomi(bdp)->mi_nlmvers = NLM_VERS;
			error = klm_lockctl(&lh, lfp, cmd, cred);
			switch (error) {
				case 0:
					break;
				case ENOPKG:
					error = ENOLCK;
					vn_bhvtomi(bdp)->mi_nlmvers = NLM4_VERS;
					break;
			}
			break;
		default:
			error = EINVAL;
	}
	switch (error) {
		case 0:
			if (islock) {
				VOP_FSYNC(vp, FSYNC_INVAL, cred,
					(off_t)0, (off_t)-1, error);
				/*
				 * On each successful lock request,
				 * invalidate the attributes to ensure
				 * a call to the server on the next
				 * check.  If the file is mapped, also
				 * check the attributes here.  Do not do
				 * invalidation for unlocks.
				 */
				INVAL_ATTRCACHE(bdp);
				if (VN_MAPPED(vp)) {
				    (void)nfs3_getattr(bdp, &attrs, 0, cred);
				}
			}
			break;
		case ENOPKG:
			error = ENOLCK;
		default:
			break;
	}
	return (error);
}

static int
nfs3_cleanlocks(bhv_desc_t *bdp, pid_t pid, long sysid, struct cred *cr)
{
	struct cred *cred;
	struct flock ld;
	int s;
	u_int oldsig;
	uthread_t *ut = curuthread;
	k_sigset_t oldmask;
	k_sigset_t newmask;
	int error;

	NLM_DEBUG(NLMDEBUG_CLEAN,
		printf("nfs3_frlock: cleaning locks for process %d\n",
		    pid));
	cred = crdup(cr);
	ld.l_pid = pid;
	ld.l_sysid = sysid;
	ld.l_type = F_UNLCK;
	ld.l_whence = 0;
	ld.l_start = 0;
	ld.l_len = 0; /* unlock entire file */
	/*
	 * We cannot allow signals here.
	 * Mask off everything but those
	 * which cannot be masked and
	 * restore the mask after the call.
	 */
	sigfillset(&newmask);
	sigdiffset(&newmask, &cantmask);
	s = ut_lock(ut);
	oldmask = *ut->ut_sighold;
	*ut->ut_sighold = newmask;
	oldsig = ut->ut_cursig;
	ut->ut_cursig = 0;
	ut_unlock(ut, s);

	error = nfs3_lockctl(bdp, &ld, F_SETLK, cred);

	s = ut_lock(ut);
	*ut->ut_sighold = oldmask;
	ut->ut_cursig = oldsig;
	ut_unlock(ut, s);
	crfree(cred);
	if (error) {
		rsetflag(bhvtor(bdp), RCLEANLOCKS);
	}
	return(error);
}

static int
nfs3_frlock(bhv_desc_t *bdp, int cmd, struct flock *lfp, int flag,
	off_t offset, vrwlock_t vrwlock, struct cred *cr)
{
	int dolock;
	int error;
	flock_t fl_save;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	rnode_t *rp = BHVTOR(bdp);

	if (cmd == F_CLNLK || cmd == F_CHKLK || cmd == F_CHKLKW) {
		if (cmd == F_CLNLK) {
		        /*
			 * Clean remote first, then clean local by falling
			 * through.
			 */
			if (!(rtomi(rp)->mi_flags & MIF_PRIVATE) &&
				(haslocks(vp, lfp->l_pid, lfp->l_sysid) ||
				(rp->r_flags & RCLEANLOCKS))) {
					error = nfs3_cleanlocks(bdp,
						lfp->l_pid, lfp->l_sysid, cr);
			}
			/* fall through to fs_frlock */
		}

		dolock = (vrwlock == VRWLOCK_NONE);
		if (dolock) {
			nfs_rwlock(bdp, VRWLOCK_WRITE);
			vrwlock = VRWLOCK_WRITE;
		}

		error = fs_frlock(bdp, cmd, lfp, flag, offset, vrwlock, cr);
		if (dolock)
			nfs_rwunlock(bdp, VRWLOCK_WRITE);

		NLM_DEBUG(NLMDEBUG_ERROR,
			if (error)
				printf("nfs3_frlock: clean/check error %d\n",
					error));
		return(error);
	}

	if (cmd != F_GETLK && cmd != F_SETLK && cmd != F_SETLKW &&
	    cmd != F_SETBSDLK && cmd != F_SETBSDLKW)
		return EINVAL;

	ASSERT(vrwlock == VRWLOCK_NONE);
	nfs_rwlock(bdp, VRWLOCK_WRITE);

	if (vn_bhvtomi(bdp)->mi_flags & MIF_PRIVATE) {
		error = fs_frlock(bdp, cmd, lfp, flag, offset, VRWLOCK_WRITE, 
				  cr);
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
	} else {
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
		error = convoff(vp, lfp, 0, offset, SEEKLIMIT, cr);
		if (!error) {
			ASSERT(lfp->l_sysid == 0L);
			/*
			 * issue the remote lock request
			 * if there is no error, record the lock in the vnode
			 * if that fails, issue an unlock request
			 */
			if (lfp->l_type == F_UNLCK) {
				error = nfs_reclock(vp, lfp, cmd, flag, cr);
				if (!error) {
					error = nfs3_lockctl(bdp, lfp, cmd, cr);
				}
			} else {
				fl_save = *lfp;
				error = nfs3_lockctl(bdp, lfp, cmd, cr);
				if (!error && (cmd != F_GETLK)) {
				    /*
				     * Record the lock in the vnode.
				     * nfs_reclock calls reclock which
				     * will only return an error when
				     * the lock could not be recorded.
				     * Thus, if there is a lock record
				     * for this lock, there must have
				     * been a previous identical
				     * request.
				     */
				    error = nfs_reclock(vp, &fl_save,
					    cmd, flag, cr);
				    if (error) {
					/*REFERENCED*/
					int err;

				    	fl_save.l_type = F_UNLCK;
				    	err = nfs3_lockctl(bdp, &fl_save,
						cmd, cr);
				    	NLM_DEBUG(NLMDEBUG_ERROR,
				    	    if (err)
				    		printf("nfs3_frlock: "
				    		    "unable to release "
				    		    "lock after error, "
				    		    "error %d\n", err));
				    }
				}
			}
		        if (error) {
			    /*
			     * If we got an error, we have no
			     * idea what the lock state is, so
			     * we'll have to do clean up on
			     * close.
			     */
			    rsetflag(rp, RCLEANLOCKS);
		        }
		}
	}
	NLM_DEBUG(NLMDEBUG_ERROR,
		if (error == EINVAL)
			printf("nfs3_frlock: EINVAL error\n", error));
	return error;
}

/*ARGSUSED*/
static int
nfs3_pathconf(bhv_desc_t *bdp, int cmd, long *valp, cred_t *cr)
{
	int rlock_held;
	rnode_t *rp = BHVTOR(bdp);
	int error;
	PATHCONF3args args;
	PATHCONF3res res;
	int douprintf;
#ifdef NFS_DEBUG
	char	*self = "nfs3_pathconf";

	printf ("%s()\n", self);
#endif /* DEBUG */
	args.object = *BHVTOFH3(bdp);

	douprintf = 1;
	error = rlock_nohang(rp);
	if (error) {
		return(error);
	}
	error = rfs3call(VN_BHVTOMI(bdp), NFSPROC3_PATHCONF,
			xdr_PATHCONF3args, (caddr_t)&args,
			xdr_PATHCONF3res, (caddr_t)&res,
			cr, &douprintf, &res.status, 0);

	if (error) {
		runlock(rp);
		return (error);
	}

	error = geterrno3(res.status);

	rlock_held = 1;
	if (!error) {
		nfs3_cache_post_op_attr(bdp, &res.resok.obj_attributes, cr,
			&rlock_held);
		if (rlock_held)
			runlock(rp);
		switch (cmd) {
		case _PC_LINK_MAX:
			*valp = res.resok.link_max;
			break;
		case _PC_NAME_MAX:
			*valp = res.resok.name_max;
			break;
		case _PC_PATH_MAX:
			*valp = MAXPATHLEN;
			break;
		case _PC_CHOWN_RESTRICTED:
			*valp = res.resok.chown_restricted;
			break;
		case _PC_NO_TRUNC:
			*valp = res.resok.no_trunc;
			break;
			
	    	case _PC_SYNC_IO:
		    	*valp = 1;
		case _PC_ASYNC_IO:
		case _PC_ABI_ASYNC_IO:
		    	*valp = 1;
			break;
		case _PC_ABI_AIO_XFER_MAX:
#if _MIPS_SIM == _ABI64
		    	if (!ABI_IS_IRIX5_64(get_current_abi())) 
			    *valp = INT_MAX;
			else
#endif /* _ABI64 */
			    *valp = LONG_MAX;		
		default:
			return (EINVAL);
		}
	} else {
		nfs3_cache_post_op_attr(bdp, &res.resfail.obj_attributes, cr,
			&rlock_held);
		if (rlock_held)
			runlock(rp);
	}

	return (error);
}

/* ARGSUSED1 */
static int
nfs3_reclaim(bhv_desc_t *bdp, int flag)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct rnode *rp;

	ASSERT(!VN_MAPPED(vp));
	ASSERT(BHV_OPS(bdp) == &nfs3_vnodeops);
	rp = BHVTOR(bdp);
	ASSERT(rp);
	if (vp->v_type == VREG) {
		ASSERT(vn_bhvtomi(bdp));
		ASSERT(!rp->r_iocount);
		nfs_rwlock(bdp, VRWLOCK_WRITE);
		VOP_FLUSHINVAL_PAGES(vp, 0, rp->r_localsize - 1, FI_NONE);
                /* Work around panic 689440 */
                if (vp->v_pgcnt) {
			cmn_err(CE_WARN, 
				"nfs3_reclaim: r_localsize %d too small", 
				rp->r_localsize);
                        VOP_FLUSHINVAL_PAGES(vp, 0, LONG_MAX, FI_NONE);
                }
		nfs_rwunlock(bdp, VRWLOCK_WRITE);
		rp->r_localsize = 0;
	}
	dnlc_purge_vp(vp);
	ASSERT(rp);
	rinactive(rp);
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
	rdestroy(rp, vn_bhvtomi(bdp));
	return 0;
}

static void
nfs3_strategy(bhv_desc_t *bdp, struct buf *bp)
{
	struct rnode *rp;
	cred_t *cr = get_current_cred();

	/*
	 * If there was an asynchronous write error on this rnode
	 * then we just return the old error code. This continues
	 * until the rnode goes away (zero ref count). We do this because
	 * there can be many procs writing this rnode.
	 */
	rp = BHVTOR(bdp);
	if (rp->r_error) {
		bp->b_error = rp->r_error;
		bp->b_flags |= B_ERROR;
		iodone(bp);
		return;
	}
	if (bp->b_pages)
		bp_mapin(bp);
	nfs3_riostart(rp);
	if (bp->b_flags & B_ASYNC) {
		nfs_async_bio(bdp, bp, cr, nfs3_bio);
	} else {
		stable_how stab_comm = FILE_SYNC;
		nfs3_bio(bdp, bp, &stab_comm, cr);
	}
	/*
	 * In any case, rnode is unlocked by now.
	 */
	return;

}

/*
 * Compare two verification identifiers, one from the wire (char *)
 * and one stored in the rnode's commit chain (as a uint64_t).
 */
static int
verf_cmp(writeverf3 *otw_verf, __uint64_t verf)
{
#define ALIGNED_64(x)	(((__psint_t)(x) & 63) == 0)
	if (ALIGNED_64(otw_verf))
		return(*(__uint64_t *)otw_verf == verf);
	return (bcmp(otw_verf, (char *)&verf, NFS3_WRITEVERFSIZE) == 0);
}

int ethan_background_commits = 1;

/*
 * If appropriate, start a background commit on the rnode.
 * The rnode's commit structure must be locked.
 */

static int
nfs3_background_commit(rnode_t *rp, commit_t *cp)
{
	offset3 count;
	u_int bsize = rtoblksz(rp);
	time_t curtime;
		
	if (ethan_background_commits == 0)
		return 0;
	
	if (cp->c_flags != COMMIT_UNSTABLE)
		return 0;

	curtime = lbolt;

	/* Don't bother committing unless we've got several buffers to commit */

	count = cp->c_count;
	count = (count < (count3)~0) ? count : ~0;
	count = count * cp->c_flushpercent / 100; /* dont change to *= */
	count = ROUNDUP(count, MAX(bsize, NBPP));
	if (count == 0) {
		cp->c_flushtime = curtime;
		return 0;
	}
	if (count > cp->c_count)
		count = cp->c_count;
	
	cp->c_flags |= COMMIT_FLUSHING;
	cp->c_flushtime = curtime;
	cp->c_flushoffset = cp->c_offset;
	cp->c_flushcount = count;
	cp->c_offset += count;
	cp->c_count -= count;
	
	if (nfs3_async_commit(rtobhv(rp), NULL) == -1) {
		cp->c_offset -= count;
		cp->c_count += count;
		cp->c_flags &= ~COMMIT_FLUSHING;
		return 0;
	}
	cp->c_flags |= COMMIT_FLUSH_ASYNC;
	return 1;
}


/*
 * Update the rnode's commit data.
 */
static void
verf_update(rnode_t *rp, off_t offset, int count, writeverf3 *verf,
	struct buf *bp, cred_t *cr)
{
	off_t curmaxoff;
	commit_t *cp = &rp->r_commit;
	/*REFERENCED*/
	int error;

	mutex_enter(&cp->c_mutex);
	/*
	 * Clean or completely committed file.  Just initialize and return.
	 */
	if (!IS_UNSTABLE(rp)) {
#pragma mips_frequency_hint NEVER
		time_t curtime;

		bcopy(verf, (char *)&cp->c_verf, NFS3_WRITEVERFSIZE);
		cp->c_offset = offset;
		cp->c_count = count;
		cp->c_flags |= COMMIT_UNSTABLE;
		curtime = lbolt;
		if (curtime > cp->c_flushtime)
			cp->c_flushtime = curtime; /* don't roll back time */
		mutex_exit(&cp->c_mutex);
		return;
	} 
	/*
	 * Verifier failure.  `Lock' out subsequent callers, and
	 * force all cached buffers for this vnode into the VOP_COMMIT
	 * path.  Until FAILING is turned off, callers will update commit
	 * data as if normal, but commits will always fail.  This has the 
	 * effect of rewriting all uncommitted buffers syncronously.
	 *
	 * Explicitly exclude the range of the given bp to avoid deadlock
	 * since we have it (b_lock) locked already.
	 */
	if (!verf_cmp(verf, cp->c_verf) && !IS_FAILING(rp)) {
#pragma mips_frequency_hint NEVER
		off_t c_offset;
		off_t c_count;
		off_t b_offset = BBTOOFF(bp->b_offset);
		off_t b_count = bp->b_bcount;
		off_t hi_coffset, hi_boffset;
		off_t offset, count;
		cred_t *tmpcred = get_current_cred();

		if (cp->c_flags & COMMIT_FLUSHING)
			nfs3_cancel_bgcommit(cp);

		c_offset = cp->c_offset;
		c_count = cp->c_count;

		cp->c_flags |= COMMIT_FAILING;
		mutex_exit(&cp->c_mutex);

		hi_coffset = c_offset + c_count;
		hi_boffset = b_offset + b_count;
		set_current_cred(cr);
		if (c_offset < b_offset) { /* commit range below bp range */
			if (hi_coffset < b_offset)
				count = c_count;
			else
				count = b_offset - c_offset;

			VOP_FLUSH_PAGES(rtov(rp), c_offset, c_offset + count - 1,
			    B_STALE, FI_NONE, error);
		}

		if (hi_coffset > hi_boffset) { /* commit range above bp range */
			if (c_offset > hi_boffset)
				offset = c_offset;
			else
				offset = hi_boffset;

			VOP_FLUSH_PAGES(rtov(rp), offset, hi_coffset - 1,
			    B_STALE, FI_NONE, error);
		}
		set_current_cred(tmpcred);

		mutex_enter(&cp->c_mutex);
		cp->c_flags &= ~COMMIT_FAILING;
		bcopy(verf, (char *)&cp->c_verf, NFS3_WRITEVERFSIZE);
	}

	curmaxoff = cp->c_offset + cp->c_count;
	if ((offset + count) > curmaxoff)
		curmaxoff = offset + count;
	if (cp->c_offset > offset)
		cp->c_offset = offset;
	cp->c_count = curmaxoff - cp->c_offset;

	/* If it has been long enough, start background commit. */

	if (lbolt - cp->c_flushtime > nfs3_commit_prd)
		nfs3_background_commit(rp, cp);

	mutex_exit(&cp->c_mutex);
}


#define IS_INRANGE(rp, offset, count)						\
	((!(((offset)+(count)) <= (rp)->r_commit.c_offset ||			\
	    (offset) >= ((rp)->r_commit.c_offset + (rp)->r_commit.c_count))) ||	\
	 (IS_FLUSHING(rp) &&							\
	  !(((offset)+(count)) <= (rp)->r_commit.c_flushoffset ||		\
	    (offset) >= ((rp)->r_commit.c_flushoffset + (rp)->r_commit.c_flushcount))))
 
/* 
 * The async commit routine is called from within buffer cache code 
 * before it frees up a buffer
 */
int
nfs3_async_commit(bhv_desc_t *bdp, struct buf *bp) 
{
	mntinfo_t *mi;
	struct nfs_async_reqs *args;
	rnode_t *rp = BHVTOR(bdp);
	commit_t *cp = &rp->r_commit;
	offset3 offset;
	count3 count;
	cred_t *cr; 

	/*
	 * if bp is NULL, we want to do a pre-emptive, yet
	 * optional, commit. If it can't be done async, don't
	 * do it at all. Also, we will already be holding c_mutex.
	 */
  
	if (bp) {
		offset = BBTOOFF(bp->b_offset);
		count = bp->b_bcount;
	}
	
	/*
	 * Don't bother trying a commit unless the rnode is
	 * marked unstable AND the buffer is in the dirty range.
	 * And DON'T allow this thread to block on a sleeping mutex,
	 * which is held for over-the-wire ops.
	 */
	
	if (bp && mutex_trylock(&cp->c_mutex)) {
		if (!IS_UNSTABLE(rp) || !IS_INRANGE(rp, offset, count)) {
			mutex_exit(&cp->c_mutex);
			brelse(bp);
			return(0);
		}
		mutex_exit(&cp->c_mutex);
	}

	cr = get_current_cred();
	mi = VN_BHVTOMI(bdp);
	/*
	 * If asyncio has been disabled, then make a synchronous request.
	 */
	if (bp && (!async_bio || !(bp->b_flags & B_ASYNC) 
		   || mi->mi_max_threads == 0))
		return nfs3_do_commit(bdp, offset, count, cr, bp);

	/*
	 * If we can't allocate a request structure, do the commit
	 * operation synchronously in this thread's context.
	 */
	if ((args = (struct nfs_async_reqs *)
	    kmem_alloc(sizeof (*args), KM_NOSLEEP)) == NULL) {
		if (bp)
			return nfs3_do_commit(bdp, offset, count, cr, bp);
		else
			return -1;
	}

	args->a_bdp = bdp;
	ASSERT(cr != NULL);
	crhold(cr);
	args->a_cred = cr;
	args->a_io = NFS_COMMIT;
	args->a_nfs_bufp = bp;
	if (bp) {
		args->a_nfs_offset = offset;
		args->a_nfs_count = count;
	}
	/*
	 * Check if we should create an async thread.  If we can't
	 * and there aren't any threads already running, do the
	 * bio synchronously.
	 */
	mutex_enter(&mi->mi_async_lock);
	if (mi->mi_threads == 0) {
		mutex_exit(&mi->mi_async_lock);
		kmem_free((caddr_t)args, sizeof (*args));
		crfree(cr);
		if (bp)
			return nfs3_do_commit(bdp, offset, count, cr, bp);
		else
			return -1;
	}


        /*
	 * This op is likely being called by a system that needs buffer 
	 * memory.  Link the request structure into the async service 
	 * list FIRST, and wakeup async thread to do the i/o.
	 */
	args->a_next = mi->mi_async_reqs;
	if (mi->mi_async_reqs == NULL) {
		mi->mi_async_tail = args;
	}
	mi->mi_async_reqs = args;
	sv_signal(&mi->mi_async_reqs_cv);
	mutex_exit(&mi->mi_async_lock);
	return 0;
}


/*
 * commit_file():
 * Called by nfs3_close() and nfs3_fsync().
 * Commit the entire file now, so that future calls to 
 * nfs3_async_commit() can avoid the buffer-by-buffer commits,
 * called via VOP_FLUSH_PAGES(B_STALE) and the buffer cache wand.
 */
static int
commit_file(rnode_t *rp, cred_t *cr)
{
	int error;
	commit_t *cp = &rp->r_commit;

	mutex_enter(&cp->c_mutex);
	if (!IS_UNSTABLE(rp)) {
		error = 0;
	} else {
		offset3 count;
		offset3 offset;

		/* 
		 * Memorize {offset, count} actually used by nfs3_commit.
		 * Used to call chunkstabilize(). 
		 */
		if (cp->c_flushoffset < cp->c_offset) {
			offset = cp->c_flushoffset;
			count = cp->c_count + cp->c_offset - cp->c_flushoffset;
		} else {
			offset = cp->c_offset;
			count = cp->c_count;
		}
			
		error = nfs3_commit(rtobhv(rp), cr, 0);
		chunkstabilize(rtov(rp), offset, offset + count - cp->c_count);
	}
	mutex_exit(&cp->c_mutex);
	return(error);
}

/* Push back flush time, cancelling any in-progress bg commits */
/* If there's a bg commit, add back the range that it's flushing */
			
void
nfs3_cancel_bgcommit(commit_t *cp)
{
	time_t curtime = lbolt;

	if (cp->c_flushtime >= curtime)
		cp->c_flushtime++;
	else
		cp->c_flushtime = curtime;
	
	if (cp->c_flushoffset < cp->c_offset) {
		cp->c_count += cp->c_offset - cp->c_flushoffset;
		cp->c_offset = cp->c_flushoffset;
	}
	cp->c_flags &= ~COMMIT_FLUSHING;
}	

static int
nfs3_do_background_commit(bhv_desc_t *bdp, cred_t *cr)
{
	rnode_t *rp = BHVTOR(bdp);
	int error = 0;
	commit_t *cp = &rp->r_commit;
	offset3 offset;
	count3 count;
	int douprintf = 1;
	COMMIT3args args;
	COMMIT3res res;
	time_t starttime, curtime;
	
	crhold(cr);
	mutex_enter(&cp->c_mutex);
	if (cp->c_flags != (COMMIT_UNSTABLE | COMMIT_FLUSHING | COMMIT_FLUSH_ASYNC)) {
		cp->c_flags &= ~(COMMIT_FLUSH_ASYNC | COMMIT_FLUSHING);
		goto out_keepflush;
	}
	cp->c_flags &= ~COMMIT_FLUSH_ASYNC;
	offset = cp->c_flushoffset;
	count = cp->c_flushcount;
	cp->c_flushtime = starttime = lbolt; /* don't measure time getting mutex */
	mutex_exit(&cp->c_mutex);

	args.file = *BHVTOFH3(bdp);
	args.offset = offset;
	args.count = count;

	error = rfs3call(RTOMI(rp), NFSPROC3_COMMIT,
			 xdr_COMMIT3args, (caddr_t)&args,
			 xdr_COMMIT3res, (caddr_t)&res,
			 cr, &douprintf, &res.status, 1);
	if (error == 0 &&
	    (res.status != NFS3_OK || !verf_cmp(&res.resok.verf, cp->c_verf)))
		error = EIO;

	curtime = lbolt;	/* don't measure time getting mutex */

	mutex_enter(&cp->c_mutex);

	if (cp->c_flushtime != starttime)
		goto out_keepflush; /* we've been cancelled, die quitely */

	if (error) {
		if (cp->c_offset > offset) {
			cp->c_count += cp->c_offset - offset;
			cp->c_offset = offset;
		}
		goto out;
	}
	if (curtime > starttime + HZ/10) {
		if (cp->c_flushpercent > COMMIT_FLUSH_PERCENTAGE_MIN)
			cp->c_flushpercent -= COMMIT_FLUSH_PERCENTAGE_INTERVAL;
	} else {
		if (cp->c_flushpercent < COMMIT_FLUSH_PERCENTAGE_MAX)
			cp->c_flushpercent += COMMIT_FLUSH_PERCENTAGE_INTERVAL;
	}

	/* If region was dirtied, writer is not sequential,
	 * stop bg commits by leaving FLUSHING flag set */

	if (cp->c_offset <= offset) {
		cp->c_flushcount = 0;
		goto out_keepflush;
	}

	if (cp->c_offset < offset + count)
		count = cp->c_offset - offset;

	chunkstabilize(rtov(rp), offset, offset + count);
out:
	cp->c_flags &= ~COMMIT_FLUSHING;
out_keepflush:
	mutex_exit(&cp->c_mutex);
	crfree(cr);
	return(error);
}

/*
 * Called by async threads via VOP_FLUSH_PAGES(BSTALE) and the 
 * buffer cache wand to commit a buffer.
 * If the commit fails, mark the buffer with an error, which causes 
 * it to get rewritten syncronously by the buffer cache code.
 */ 
int
nfs3_do_commit(bhv_desc_t *bdp, offset3 offset, count3 count, 
		cred_t *cr, struct buf *bp)
{
	rnode_t *rp = BHVTOR(bdp);
	int error;

	if (bp == NULL)
		return nfs3_do_background_commit(bdp, cr);

	mutex_enter(&rp->r_commit.c_mutex);
	if (!IS_UNSTABLE(rp) || !IS_INRANGE(rp, offset, count)) {
		error = 0;
	} else if (IS_FAILING(rp)) {
		error = EIO;
	} else {
		error = nfs3_commit(bdp, cr, 1);
	}
	mutex_exit(&rp->r_commit.c_mutex);
	if (error) {
		bp->b_error = error;
		bp->b_flags |= B_ERROR;
	}
	brelse(bp);
	return(error);
}

/*
 * The over-the-wire routine.  Returns 0 if the rfscall actually
 * happens AND the resulting status is NFS3_OK AND the resulting 
 * verifier matches the one stored in the rnode.
 */
static int
nfs3_commit(bhv_desc_t *bdp, cred_t *cr, int frombio)
{
	int error;
	rnode_t *rp;
	commit_t *cp;
	COMMIT3args args;
	COMMIT3res res;
	int douprintf;
	count3 count; 

	if (!WRITEALLOWED(BHV_TO_VNODE(bdp), cr)) {
		cmn_err(CE_WARN,"NFSV3 commit: dirty rnode on ROFS?");
		return (0);
	}

	rp = BHVTOR(bdp);
	crhold(cr);
	cp = &rp->r_commit;

	if (cp->c_flags & COMMIT_FLUSHING)
		nfs3_cancel_bgcommit(cp);
commit_more:

	count = (cp->c_count < (count3)~0) ? cp->c_count : ~0;

	args.file = *BHVTOFH3(bdp);
	args.offset = cp->c_offset;
	args.count = count;

	douprintf = 1;
	error = rfs3call(RTOMI(rp), NFSPROC3_COMMIT,
			xdr_COMMIT3args, (caddr_t)&args,
			xdr_COMMIT3res, (caddr_t)&res,
			cr, &douprintf, &res.status, frombio);

	if (error == 0) {
		switch(res.status) {
		case NFS3_OK:
			if (!verf_cmp(&res.resok.verf, cp->c_verf)) {
				error = EIO;
			}
			break;

		case NFS3ERR_STALE:
			rp->r_error = ESTALE;
			error = 0;
			rsetflag(rp, RDONTWRITE);
			PURGE_ATTRCACHE(bdp);
			break;

		case NFS3ERR_BADHANDLE:
		case NFS3ERR_IO:
		case NFS3ERR_SERVERFAULT:
			error = EIO;
			break;

		default:	/* out of spec, shouldn't happen */
			break;

		}
	}
	if (error == 0) {
		cp->c_count -= count;
		cp->c_offset += count;
		if (cp->c_count > 0) {
			goto commit_more;
		} else {
			cp->c_flags &= ~COMMIT_UNSTABLE;
		}
	}
	crfree(cr);
	return (error);
}

static int
nfs3_getxattr (bhv_desc_t *bdp, char *name, char *value, int *valuelenp,
	       int flags, cred_t *cr)
{
	int error;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	_NFS_IACCESS(bdp, VREAD, cr);

	if (vp->v_vfsp->vfs_flag & VFS_DOXATTR)
		flags |= ATTR_TRUST;
	else
		flags &= ~ATTR_TRUST;

	if (!getxattr_cache(bdp, name, value, valuelenp, flags, cr, &error))
		if (!error)
			error = getxattr_otw(bdp, name, value, valuelenp,
					     flags, cr);
	return (error);
}

static int
nfs3_setxattr (bhv_desc_t *bdp, char *name, char *value, int valuelen,
	       int flags, cred_t *cr)
{
	int rlock_held;
	rnode_t *rp = BHVTOR(bdp);
	int error;
	struct SETXATTR1args args;
	struct SETXATTR1res res;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	_NFS_IACCESS(bdp, VWRITE, cr);

	res.status = (enum nfsstat3) ENOATTR;
	res.resok.obj_wcc.before.attributes = FALSE;
	res.resok.obj_wcc.after.attributes = FALSE;
	res.resfail.obj_wcc.before.attributes = FALSE;
	res.resfail.obj_wcc.after.attributes = FALSE;

	if (vp->v_vfsp->vfs_flag & VFS_DEFXATTR) {
		res.status = (enum nfsstat3) 0;
		return (0);
	}

	args.object = *bhvtofh3(bdp);
	args.name = name;
	args.data.value = value;
	args.data.len = valuelen;
	if (vp->v_vfsp->vfs_flag & VFS_DOXATTR)
		args.flags = flags | ATTR_TRUST;
	else
		args.flags = flags & ~ATTR_TRUST;

	error = rlock_nohang(rp);
	if (error) {
		return(error);
	}
	error = xattrcall (VN_BHVTOMI(bdp), XATTRPROC1_SETXATTR,
			   xdr_SETXATTR1args, (caddr_t)&args,
			   xdr_SETXATTR1res, (caddr_t)&res,
			   cr, &res.status);

	if (error) {
		PURGE_ATTRCACHE(bdp);
		runlock(rp);
	} else {
		rlock_held = 1;
		error = geterrno3(res.status);
		PURGE_XATTRCACHE(bdp);
		if (!error) {
			nfs3_cache_wcc_data(bdp, &res.resok.obj_wcc, cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
		}
		else {
			nfs3_cache_wcc_data(bdp, &res.resfail.obj_wcc, cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			PURGE_STALE_FH(error, bdp, cr);
		}
	}
	return (error);
}

static int
nfs3_rmxattr(bhv_desc_t *bdp, char *name, int flags, cred_t *cr)
{
	int rlock_held;
	rnode_t *rp = BHVTOR(bdp);
	int error;
	struct RMXATTR1args args;
	struct RMXATTR1res res;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	_NFS_IACCESS(bdp, VWRITE, cr);

	res.status = (enum nfsstat3) ENOATTR;
	res.resok.obj_wcc.before.attributes = FALSE;
	res.resok.obj_wcc.after.attributes = FALSE;
	res.resfail.obj_wcc.before.attributes = FALSE;
	res.resfail.obj_wcc.after.attributes = FALSE;

	if (vp->v_vfsp->vfs_flag & VFS_DEFXATTR) {
		res.status = (enum nfsstat3) 0;
		return (0);
	}

	args.object = *bhvtofh3(bdp);
	args.name = name;
	if (vp->v_vfsp->vfs_flag & VFS_DOXATTR)
		args.flags = flags | ATTR_TRUST;
	else
		args.flags = flags & ~ATTR_TRUST;

	error = rlock_nohang(rp);
	if (error) {
		return(error);
	}
	error = xattrcall (VN_BHVTOMI(bdp), XATTRPROC1_RMXATTR,
			   xdr_RMXATTR1args, (caddr_t)&args,
			   xdr_RMXATTR1res, (caddr_t)&res,
			   cr, &res.status);

	if (error) {
		PURGE_ATTRCACHE(bdp);
		runlock(rp);
	}
	else {
		rlock_held = 1;
		error = geterrno3(res.status);
		PURGE_XATTRCACHE(bdp);
		if (!error) {
			nfs3_cache_wcc_data(bdp, &res.resok.obj_wcc, cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
		}
		else {
			nfs3_cache_wcc_data(bdp, &res.resfail.obj_wcc, cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			PURGE_STALE_FH(error, bdp, cr);
		}
	}
	return (error);
}

static int
nfs3_listxattr (bhv_desc_t *bdp, char *buffer, int bufsize, int flags,
		attrlist_cursor_kern_t *cursor, cred_t *cr)
{
	int rlock_held;
	rnode_t *rp = BHVTOR(bdp);
	int error;
	struct LISTXATTR1args args;
	struct LISTXATTR1res res;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	/* perform access check */
	_NFS_IACCESS(bdp, VREAD, cr);

	/* initialize args */
	args.object = *bhvtofh3(bdp);
	args.length = bufsize;
	if (vp->v_vfsp->vfs_flag & VFS_DOXATTR)
		args.flags = flags | ATTR_TRUST;
	else
		args.flags = flags & ~ATTR_TRUST;
	args.cursor = *cursor;

	/* initialize response */
	res.resok.size = res.resok.data.len = args.length;
	if (args.length == 0)
		res.resok.data.value = NULL;
	else
		res.resok.data.value = (char *) kmem_alloc (args.length,
							    KM_SLEEP);

	res.status = (enum nfsstat3) ENOATTR;
	res.resok.obj_attributes.attributes = FALSE;
	res.resfail.obj_attributes.attributes = FALSE;

	error = rlock_nohang(rp);
	if (error) {
		if (res.resok.data.value != NULL)
			kmem_free((caddr_t) res.resok.data.value,
				  res.resok.size);
		return (error);
	}
	error = xattrcall (VN_BHVTOMI(bdp), XATTRPROC1_LISTXATTR,
			   xdr_LISTXATTR1args, (caddr_t)&args,
			   xdr_LISTXATTR1res, (caddr_t)&res,
			   cr, &res.status);

	rlock_held = 1;
	if (!error) {
		error = geterrno3(res.status);
		if (!error) {
			nfs3_cache_post_op_attr(bdp, &res.resok.obj_attributes,
						cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			bcopy(res.resok.data.value, buffer,
			      res.resok.data.len);
			*cursor = res.resok.cursor;
		}
		else {
			nfs3_cache_post_op_attr(bdp,
						&res.resfail.obj_attributes,
						cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			PURGE_STALE_FH(error, bdp, cr);
		}
	} else {
		runlock(rp);
	}

	if (res.resok.data.value != NULL)
		kmem_free((caddr_t) res.resok.data.value, res.resok.size);
	return (error);
}
