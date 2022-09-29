/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * $Revision: 1.101 $
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
 *	(c) 1986, 1987, 1988, 1989, 1990, 1991  Sun Microsystems, Inc.
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */
#include "types.h"
#include <sys/buf.h>            /* for B_ASYNC */
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mkdev.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <netinet/in.h>
#include <net/route.h>
#include <net/if.h>
#include <sys/xlate.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include "xdr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "nfs3_clnt.h"
#include "rnode.h"
#include "nfs3_rnode.h"
#include "bds.h"
#include "string.h"
#include "lockmgr.h"

#define SMALL_XFER_SIZE 16 * 1024 /* when going through a router */
#define MAX_UDP_XFER_SIZE   48 * 1024 /* max user spec'ed */
#define MAX_TCP_XFER_SIZE   4 * 1024 * 1024 /* max user spec'ed */
#define MAX_XFER_SIZE(args) (((args) & NFSMNT_TCP) ?		\
			   MAX_TCP_XFER_SIZE : MAX_UDP_XFER_SIZE)

#define SMALL_MTU	 2 * 1024 /* Ethernets are smaller */

extern int nfs3_rah;	/* default read-ahead */

mutex_t nfs3_linkrename_lock;

extern void     vattr_to_fattr3(struct vattr *, struct fattr3 *);
extern int	makenfs3node(nfs_fh3 *, struct fattr3 *,
		    struct vfs *, bhv_desc_t **bdpp, cred_t *);
extern int	rfs3call(struct mntinfo *, int, xdrproc_t, caddr_t,
		    xdrproc_t, caddr_t, cred_t *, int *, enum nfsstat3 *);
extern int	nfs3_attrcache(bhv_desc_t *, struct fattr3 *,
		    enum staleflush, int *);
extern void	nfs_async_stop(mntinfo_t *);

char *rfsnames_v3[] = {
	"null", "getattr", "setattr", "lookup", "access",
	"readlink", "read", "write", "create", "mkdir",
	"symlink", "mknod", "remove", "rmdir", "rename",
	"link", "readdir", "readdir+", "fsstat", "fsinfo",
	"pathconf", "commit"
};

extern int	nfs_subrinit(void);
extern int	async_thread_create(mntinfo_t *);

extern  struct minormap nfs3_minormap;
extern	int nfs3_default_xfer;
extern	int nfs3_max_threads;

/*
 * nfs3 vfs operations.
 */
static	int	nfs3_mount(vfs_t *, vnode_t *, struct mounta *, char *, cred_t *);
static	int	nfs3_unmount(bhv_desc_t *, int, cred_t *);
static	int	nfs3_root(bhv_desc_t *, vnode_t **);
static	int	nfs3_statvfs(register bhv_desc_t *, struct statvfs *, vnode_t *);
static	int	nfs3_vget(bhv_desc_t *, vnode_t **, fid_t *);
static  int	nfs3rootvp(rnode_t **, vfs_t *, struct sockaddr_in *,
		    nfs_fh3 *, char *, int , int , int , cred_t *);

vfsops_t nfs3_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	nfs3_mount,	/* vfs_mount */
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_dounmount,	/* dounmount */
	nfs3_unmount,	/* vfs_unmount */
	nfs3_root,	/* vfs_root */
	nfs3_statvfs,	/* vfs_statvfs */
	nfs_sync,	/* vfs_sync */
	nfs3_vget,	/* vfs_vget */
	fs_nosys,	/* vfs_mountroot */
	fs_noerr,	/* vfs_realvfsops */
	fs_import,	/* vfs_import */
	fs_quotactl	/* quotactl */
};

/*
 * Initialize the vfs structure
 */

extern int nfs3_fstype;
dev_t   nfs3_major;
int nfs3_max_mntio;

extern lock_t mntinfolist_lock;
extern mntinfo_t *mntinfolist;

/*ARGSUSED*/
void
nfs3_init(struct vfssw *vswp, int fstyp)
{
	int i;
	extern int nbuf;

        nfs3_fstype = fstyp;
        if ((i = getudev()) < 0)
                cmn_err(CE_WARN, "nfs3_init: can't get unique device number");
        else
                nfs3_major = i;

	/*
	 * initialize the device minor map
	 */
        spinlock_init(&nfs3_minormap.lock, "nfs3_minormap");
	nfs3_minormap.size = MINORMAP_SIZE;
	nfs3_minormap.vec = kmem_zalloc(nfs3_minormap.size, KM_SLEEP);

	mutex_init(&nfs3_linkrename_lock, MUTEX_DEFAULT, "nfs3 link/rename");

	nfs_subrinit();
	/*
	 * Set a max in progress i/o per mount point so that we can't
	 * hold too many bufs io-waiting to a few down servers.
	 */
	nfs3_max_mntio = nbuf/16;
}

extern mac_t mac_admin_high_lp;

#if _MIPS_SIM == _ABI64
static int irix5_to_nfs_args(enum xlate_mode, void *, int, xlate_info_t *);
#endif

#define ROUNDUP(a, b)   ((((a) + (b) - 1) / (b)) * (b))

/*
 * nfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
int nfs3_use_readdir = 0;

#define ROUNDUP(a, b)   ((((a) + (b) - 1) / (b)) * (b))

/*
 * Returns the prefered transfer size in bytes, based on
 * if the route goes through a gateway, or has "small" MTU.
 */
static int
nfs3tsize(int flags, struct sockaddr *addr)
{
	struct route ro;
	int value;

	if (flags & NFSMNT_TCP) {
		return (nfs3_default_xfer);
	}
	ROUTE_RDLOCK();
	ro.ro_dst = *addr;
	ro.ro_rt = NULL;
	rtalloc(&ro);
	if (ro.ro_rt && ((ro.ro_rt->rt_flags & RTF_GATEWAY)  ||
			 ((ro.ro_rt->rt_ifp &&
			   ro.ro_rt->rt_ifp->if_mtu < SMALL_MTU))) &&
	    nfs3_default_xfer > SMALL_XFER_SIZE) {
		value = SMALL_XFER_SIZE;
	} else {
		value = nfs3_default_xfer;
	}
	if (ro.ro_rt) {
	    rtfree_needpromote(ro.ro_rt);
	}
	ROUTE_UNLOCK();

	return (value);
}

/*ARGSUSED*/
static int
nfs3_mount(vfs_t *vfsp, vnode_t *mvp, struct mounta *uap, char *attrs, cred_t *cr)
{
	int ospl;
	int error = 0;
	rnode_t *rtrp = NULL;		/* the server's root */
	mntinfo_t *mi = NULL;		/* mount info, pointed at by vfs */
	nfs_fh3 fh;			/* root fhandle */
	struct nfs_args args;		/* nfs mount arguments */
	struct sockaddr_in addr;	/* server's address */
	size_t hlen;			/* length of hostname */
	char shostname[HOSTNAMESZ];	/* server's hostname */
	extern struct ifnet *ifaddr_to_ifp(struct in_addr inaddr);

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT)) {
		return (EPERM);
	}

	if (mvp->v_type != VDIR) {
		return (ENOTDIR);
	}

	/*
	 * get arguments
	 */
	if (error = COPYIN_XLATE(uap->dataptr, &args, MIN(uap->datalen, sizeof args),
		 irix5_to_nfs_args, get_current_abi(), 1)) {
		return (error);
	}

	/*
	 * Get server address
	 */
	if (copyin((caddr_t) args.addr, (caddr_t) &addr, sizeof (addr))) {
		error = EFAULT;
	}
	if (error)
		goto errout;
	
	/*
	 * For now we just support AF_INET
	 */
	if (addr.sin_family != AF_INET)
		return EPFNOSUPPORT;

	/*
	 * Get the root fhandle
	 */
	fh.fh3_length = args.fh_len;
#ifdef NFS3_DEBUG
	printf("fh.fh3_length = %d\n ", fh.fh3_length);
#endif
	if (fh.fh3_length > NFS3_FHSIZE) {
		error = EINVAL;
		goto errout;
	}
	
	if (copyin((caddr_t)args.fh, (caddr_t)&(fh.fh3_u.data), fh.fh3_length)) {
		error = EFAULT;
		goto errout;
	}

	/*
	 * Get server's hostname
	 */
	if (args.flags & NFSMNT_HOSTNAME) {
		error = (copyinstr(args.hostname, shostname, 
			sizeof(shostname), &hlen));
		if (error)
			goto errout;
	} else
		(void) strncpy(shostname, "unknown-host", sizeof (shostname));


	/*
	 * Extract the user-specified read and write sizes here and round
	 * them up to a multiple of 512.  If a size is not specified, use
	 * the default.  We have to do this here so that any negotiations
	 * with the server will override these values if they are too
	 * large for the server.  Some servers will simply drop read or
	 * write requests that are too large.
	 */
	if (args.flags & NFSMNT_RSIZE) {
		if (args.rsize <= 0) {
			error = EINVAL;
			goto errout;
		}
		args.rsize = ROUNDUP(MIN(MAX_XFER_SIZE(args.flags), args.rsize), 512);
	} else {
		args.rsize = nfs3tsize(args.flags, (struct sockaddr *)&addr);
	}
	if (args.flags & NFSMNT_WSIZE) {
		if (args.wsize <= 0) {
			error = EINVAL;
			goto errout;
		}
		args.wsize = ROUNDUP(MIN(MAX_XFER_SIZE(args.flags), args.wsize), 512);
	} else {
		args.wsize = nfs3tsize(args.flags, (struct sockaddr *)&addr);
	}
	/*
	 * Get root vnode.
	 */
	error = nfs3rootvp(&rtrp, vfsp, &addr, &fh, shostname, args.flags,
		args.rsize, args.wsize, cr);
	if (error)
		goto errout;

	/*
	 * Set option fields in mount info record
	 */
	mi = rtomi(rtrp);
	/*
	 * mark loopback mounts of local file systems so we can avoid
	 * using the buffer cache on the client side, thus avoiding
	 * deadlocks
	 */
	if (ifaddr_to_ifp(addr.sin_addr) && (addr.sin_port == NFS_PORT))
		atomicSetUint(&mi->mi_flags, MIF_LOCALLOOP);

	if (args.flags & NFSMNT_NOAC)
		atomicSetUint(&mi->mi_flags, MIF_NOAC);
	mi->mi_ipmac = vfsp->vfs_mac ? vfsp->vfs_mac->mv_ipmac :
		mac_admin_high_lp;
	if (args.flags & NFSMNT_RETRANS) {
		mi->mi_retrans = args.retrans;
	}
	if (args.flags & NFSMNT_TIMEO) {
		if (args.timeo <= 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_timeo = args.timeo;
	}
	
	/*
	 * Set filesystem block size to a page multiple
	 */
	mi->mi_bsize = ROUNDUP(MIN(mi->mi_tsize, mi->mi_stsize), NBPP);

#ifdef NFS3_DEBUG 
	printf("nfs3_mount:  mi_tsize %d, mi_stsize %d, mi_bsize %d\n",
		mi->mi_tsize, mi->mi_stsize, mi->mi_bsize);
#endif
	if (args.flags & NFSMNT_ACREGMIN) {
		if (args.acregmin <= 0)
			mi->mi_acregmin = ACMINMAX;
		else
			mi->mi_acregmin = MIN(args.acregmin, ACMINMAX);
	}
	if (args.flags & NFSMNT_ACREGMAX) {
		if (args.acregmax <= 0)
			mi->mi_acregmax = ACMAXMAX;
		else
			mi->mi_acregmax = MIN(args.acregmax, ACMAXMAX);
	}
	if (args.flags & NFSMNT_ACDIRMIN) {
		if (args.acdirmin <= 0)
			mi->mi_acdirmin = ACMINMAX;
		else
			mi->mi_acdirmin = MIN(args.acdirmin, ACMINMAX);
	}
	if (args.flags & NFSMNT_ACDIRMAX) {
		if (args.acdirmax <= 0)
			mi->mi_acdirmax = ACMAXMAX;
		else
			mi->mi_acdirmax = MIN(args.acdirmax, ACMAXMAX);
	}
	if (args.flags & NFSMNT_SYMTTL) {
		mi->mi_symttl = args.symttl;
	}

	if (args.flags & NFSMNT_BASETYPE)
		strncpy(mi->mi_basetype, args.base, sizeof mi->mi_basetype);
	else
		strncpy(mi->mi_basetype, vfssw[vfsp->vfs_fstype].vsw_name,
							sizeof mi->mi_basetype);
	
	if (args.flags & NFSMNT_NAMEMAX)
		mi->mi_namemax = args.namemax;
	else
		/*
		 * It seems like it would be better to set namemax
		 * to 0 if it is unknown, especially since it is unsigned.
		 * But there are programs that check blindly
		 */
		mi->mi_namemax = (u_int)-1;
	mi->mi_rdahead = nfs3_rah;
	if (args.flags & NFSMNT_ASYNCNLM) {
		mi->mi_nlm &= ~(MI_NLM4_SYNC | MI_NLM_SYNC);
	}
	if (args.flags & NFSMNT_BDSAUTO) {
		mi->mi_bdsauto = args.bdsauto;
	}
	if (args.flags & NFSMNT_BDSWINDOW) {
		mi->mi_bdswindow = args.bdswindow;
	}
	if (args.flags & NFSMNT_BDSBUFLEN) {
		if (args.bdsbuflen && args.bdsbuflen < IO_NBPP)
			mi->mi_bdsbuflen = IO_NBPP;
		else
			mi->mi_bdsbuflen = args.bdsbuflen;
	} else {
		mi->mi_bdsbuflen = 0;
		mi->mi_bdsbuflen = ~(mi->mi_bdsbuflen);
	}
	if (args.flags & NFSMNT_SILENT) {
		atomicSetUint(&mi->mi_flags, MIF_SILENT);
	}
	if (args.flags & NFSMNT_PID) {
		mi->mi_pid = args.pid;
		VN_FLAGSET(mi->mi_rootvp, VROOTMOUNTOK);
	}
	if (args.flags & NFSMNT_THREADS) {
		if (args.maxthreads < nfs3_max_threads)
			mi->mi_max_threads = args.maxthreads;
	}
	ospl = mutex_spinlock(&mntinfolist_lock);
	mi->mi_next = mntinfolist;
	mi->mi_prev = NULL;
	if (mntinfolist)
		mntinfolist->mi_prev = mi;
	mntinfolist = mi;
	mutex_spinunlock(&mntinfolist_lock, ospl);
	/* Use readdirplus */
	if (nfs3_use_readdir)
		atomicSetUint(&mi->mi_flags, MIF_READDIR);
	if (mi->mi_max_threads && async_thread_create (mi)) {
		mi->mi_threads++;
	}
	return(0);

errout:
        if (rtrp) {     /* implies  mi != NULL  as well */
                vmap_t rtvmap;

                VMAP(rtov(rtrp), rtvmap);
                VN_RELE(rtov(rtrp));
                vn_purge(rtov(rtrp), &rtvmap);
                vfs_putnum(&nfs3_minormap, mi->mi_mntno);
                mutex_destroy(&mi->mi_lock);
                mutex_destroy(&mi->mi_async_lock);
                sv_destroy(&mi->mi_async_reqs_cv);
                sv_destroy(&mi->mi_async_cv);
		sv_destroy(&mi->mi_nohang_wait);
		if (mi->mi_con)
		    nfscon_free(mi);
		if (mi->mi_flags & MIF_BDS)
			spinlock_destroy(&mi->mi_bds_vslock);
		VFS_REMOVEBHV(vfsp, &mi->mi_bhv);
                kmem_free((caddr_t)mi, sizeof (*mi));
        }

	return (error);
}


static int
nfs3rootvp(rnode_t **rtrpp, vfs_t *vfsp, struct sockaddr_in *addr, nfs_fh3 *fh,
	char *shostname, int flags, int rsize, int wsize, cred_t *cr)
{
	vnode_t *rtvp = NULL;	/* the server's root */
	bhv_desc_t *rtbdp;	/* the server's root behavior */
	register mntinfo_t *mi = NULL;	/* mount info point at by vfs */
	struct vattr va;		/* root vnode attributes */
	fattr3 na;			/* root vnode attributes in nfs form */
	struct FSINFO3args args;
	struct FSINFO3res res;
	register int error;
	int douprintf;
	int bhv_inserted;
	vmap_t rtvmap;

	ASSERT(cr->cr_ref != 0);

	/*
	 * Create a mount record and link it to the vfs struct.
	 */
	bhv_inserted = 0;
	mi = (mntinfo_t *)kmem_zalloc(sizeof (*mi), KM_SLEEP);
	mi->mi_ipmac = vfsp->vfs_mac ? vfsp->vfs_mac->mv_ipmac :
		mac_admin_high_lp;
	mi->mi_ipmac = NULL;
	if ((flags & NFSMNT_SOFT) == 0)
		mi->mi_flags |= MIF_HARD;
	if ((flags & NFSMNT_NOINT) == 0)
		mi->mi_flags |= MIF_INT;

	/* mi_bds being set implies that spinlock has been init'd */
	if (flags & NFSMNT_BDS) {
		mi->mi_flags |= MIF_BDS;
		spinlock_init(&mi->mi_bds_vslock, "BDS mountinfo vslock");
		mi->mi_bds_blksize = mi->mi_bds_maxiosz = 4 * 1024 * 1024;
	}
	if (flags & NFSMNT_SHORTUID)
		mi->mi_flags |= MIF_SHORTUID;
	mi->mi_nlm =  MI_NLM_DEFAULT | MI_NLM4_DEFAULT;
	mi->mi_lmaddr = mi->mi_addr = *addr;
	mi->mi_lmaddr.sin_port = 0;
	mi->mi_retrans = NFS_RETRIES;
	mi->mi_timeo = NFS_TIMEO;
	if ((mi->mi_mntno = vfs_getnum(&nfs3_minormap)) == -1) {
		error = ENFILE;
		goto bad;
	}
	mi->mi_fsidmaps = NULL;
	init_mutex(&mi->mi_lock, MUTEX_DEFAULT, "nm3", mi->mi_mntno);
	sv_init(&mi->mi_nohang_wait, SV_DEFAULT, "nohang wait");
	mi->mi_prog = NFS_PROGRAM;
	mi->mi_vers = NFS3_VERSION;
	mi->mi_nlmvers = NLM4_VERS;
	mi->mi_rfsnames = rfsnames_v3;

	bcopy(shostname, mi->mi_hostname, HOSTNAMESZ);
	mi->mi_acregmin = ACREGMIN;
	mi->mi_acregmax = ACREGMAX;
	mi->mi_acdirmin = ACDIRMIN;
	mi->mi_acdirmax = ACDIRMAX;
	if (flags & NFSMNT_PRIVATE)
		mi->mi_flags |= MIF_PRIVATE;
	if (flags & NFSMNT_TCP) {
		mi->mi_flags |= MIF_TCP;
		if (error = nfscon_get(mi))
			goto bad;
        }

	vfsp->vfs_fsid.val[0] = vfsp->vfs_dev = 
		makedev(nfs3_major, mi->mi_mntno);
	vfsp->vfs_fsid.val[1] = vfsp->vfs_fstype = nfs3_fstype;
	vfsp->vfs_flag |= VFS_CELLULAR;
	vfs_insertbhv(vfsp, &mi->mi_bhv, &nfs3_vfsops, mi);
	bhv_inserted = 1;

	/*
	 * Initialize fields used to support async putpage operations.
	 */
	mi->mi_async_reqs = mi->mi_async_tail = NULL;
	mi->mi_threads = 0;
	mi->mi_max_threads = nfs3_max_threads;
	mutex_init(&mi->mi_async_lock, MUTEX_DEFAULT, "nfs3_async_lock");
	sv_init(&mi->mi_async_reqs_cv, SV_DEFAULT, "nfs3_async_reqs");
	sv_init(&mi->mi_async_cv, SV_DEFAULT, "nfs3_async");

	/*
	 * Make the root vnode, use it to get attributes,
	 * then remake it with the attributes.
	 *
	 * XXX we need to pass a V3 filehandle at this point.
	 * we set mi_rootfsid to -1 so that fattr3_to_vattr will
	 * store the base fsid in the rnode's vattr structure
	 */
	mi->mi_rootfsid = -1;
	error = makenfs3node(fh, (fattr3 *) NULL, vfsp, &rtbdp, cr);
	if (error) {
		goto bad;
	}
	rtvp = BHV_TO_VNODE(rtbdp);
	if ((rtvp->v_flag & VROOT) != 0) {
		error = EINVAL;
		goto bad;
	}
	VN_FLAGSET(rtvp, VROOT);

	VOP_GETATTR(rtvp, &va, 0, cr, error);
	if (error)
		goto bad;
	vattr_to_fattr3(&va, &na);
	mi->mi_rootvp = rtvp;
	mi->mi_root = bhvtor(rtbdp);

	args.fsroot = *BHVTOFH3(rtbdp);
	douprintf = 1;
	error = rfs3call(mi, NFSPROC3_FSINFO,
			xdr_FSINFO3args, (caddr_t)&args,
			xdr_FSINFO3res, (caddr_t)&res,
			cr, &douprintf, &res.status);
	if (error)
		goto bad;
	error = geterrno3(res.status);
	if (!error) {
		uint32 res_rtsize, res_wtsize;
		/*
		 * intergraph sets rt and wtpref to 0, we need to deal with it.
		 * To do so, we'll use the server's max if pref is 0.
		 * if max is zero, we'll fall back to 8k
		 */
		res_rtsize = (res.resok.rtpref ?
				res.resok.rtpref : res.resok.rtmax);
		res_wtsize = (res.resok.wtpref ?
				res.resok.wtpref : res.resok.wtmax);
		if (res_rtsize == 0)
			res_rtsize = 8192;
		if (res_wtsize == 0)
			res_wtsize = 8192;

		mi->mi_tsize  = MIN(res_rtsize, rsize);
		mi->mi_stsize = MIN(res_wtsize, wsize);

		if (res.resok.obj_attributes.attributes)
			(void)nfs3_attrcache(rtbdp, 
					&res.resok.obj_attributes.attr,
					NOFLUSH, NULL);
		else
			(void)nfs3_attrcache(rtbdp,
					     &na,
					     NOFLUSH, NULL);
		
		if (res.resok.properties & FSF3_LINK)
			mi->mi_flags |= MIF_LINK;
		if (res.resok.properties & FSF3_SYMLINK)
			mi->mi_flags |= MIF_SYMLINK;
	} else {
		/* 
		 * Set this as we cannot have mi_tsize and mi_stsize be zero.
		 */
		mi->mi_tsize  = rsize;
		mi->mi_stsize = wsize;

		if (res.resfail.obj_attributes.attributes)
			(void)nfs3_attrcache(rtbdp, 
				&res.resfail.obj_attributes.attr,
				NOFLUSH, NULL);
		else
			(void)nfs3_attrcache(rtbdp,
					     &na,
					     NOFLUSH, NULL);
	}
	/*
	 * XXX Why does this code ignore most of the fields from
	 * the NFSPROC3_FSINFO call?
	 */

	*rtrpp = bhvtor(rtbdp);
	return (0);
bad:
	if (rtvp) {
		VMAP(rtvp, rtvmap);
		VN_RELE(rtvp);
		vn_purge(rtvp, &rtvmap);
	}
	if (mi) {
		if (mi->mi_con)
			nfscon_free(mi);
		if (mi->mi_flags & MIF_BDS)
			spinlock_destroy(&mi->mi_bds_vslock);
		if (mi->mi_mntno != -1)
			vfs_putnum(&nfs3_minormap, mi->mi_mntno);
		mutex_destroy(&mi->mi_lock);
		mutex_destroy(&mi->mi_async_lock);
		sv_destroy(&mi->mi_async_reqs_cv);
		sv_destroy(&mi->mi_async_cv);
		sv_destroy(&mi->mi_nohang_wait);
		if (bhv_inserted) {
			VFS_REMOVEBHV(vfsp, &mi->mi_bhv);
		}
		kmem_free((caddr_t)mi, sizeof (*mi));
	}
	*rtrpp = NULL;
	return (error);
}

/*
 * vfs operations
 */
/*ARGSUSED*/
static int
nfs3_unmount(bhv_desc_t *bdp, int flags, cred_t *cr)
{
	mntinfo_t *mi = vfs_bhvtomi(bdp);
	int ospl;
	vnode_t *vp;
	u_short omax;
	struct fsidmap *fsid, *nextfsid;
	vmap_t rtvmap;
	extern int nfs_wakeup(struct mntinfo *);

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return (EPERM);

	klm_shutdown();
	/*
	 * Wait until all asynchronous putpage operations on
	 * this file system are complete before flushing rnodes
	 * from the cache.
	 */
	if (!rflush(mi)) {
		return EBUSY;
	}
	omax = mi->mi_max_threads;
	nfs_async_stop(mi);

	if (!rflush(mi)) {
		return EBUSY;
	}

	if (mi->mi_refct != 1)
		cmn_err(CE_PANIC, "nfs3_umount: mi_refcnt != 1\n");

	vp = mi->mi_rootvp;
	ASSERT(vp->v_count > 0);
	ospl = mutex_spinlock(&mntinfolist_lock);
	if ((vp->v_count != 1) || (mi->mi_nlm & MI_NLM_RECLAIMING)) {
		mutex_spinunlock(&mntinfolist_lock, ospl);
		mutex_enter(&mi->mi_async_lock);
		mi->mi_max_threads = omax;
		mutex_exit(&mi->mi_async_lock);
		return (EBUSY);
	}
	if (mi->mi_flags & MIF_BDS) {
		bds_unmount(mi);
		spinlock_destroy(&mi->mi_bds_vslock);
	}
	if (mi->mi_next) {
		mi->mi_next->mi_prev = mi->mi_prev;
	}
	if (mi->mi_prev) {
		mi->mi_prev->mi_next = mi->mi_next;
	} else {
		mntinfolist = mi->mi_next;
	}
	mutex_spinunlock(&mntinfolist_lock, ospl);

	VMAP(vp, rtvmap);
	VN_RELE(vp);
	vn_purge(vp, &rtvmap);
	ASSERT(mi->mi_rnodes == NULL);
	vfs_putnum(&nfs3_minormap, mi->mi_mntno);
	for (fsid = mi->mi_fsidmaps; fsid; fsid = nextfsid) {
		vfs_putnum(&nfs3_minormap, minor(fsid->fsid_local));
                nextfsid = fsid->fsid_next;
                kmem_free(fsid, sizeof *fsid);
	}
	mutex_destroy(&mi->mi_lock);
	mutex_destroy(&mi->mi_async_lock);
	sv_destroy(&mi->mi_async_reqs_cv);
	sv_destroy(&mi->mi_async_cv);
	sv_destroy(&mi->mi_nohang_wait);
        if (mi->mi_flags & MIF_TCP)
            nfscon_free(mi);
	VFS_REMOVEBHV(bhvtovfs(bdp), &mi->mi_bhv);
	kmem_free((caddr_t)mi, sizeof (*mi));

	return (0);
}

/*
 * find root of nfs
 */
static int
nfs3_root(bhv_desc_t *bdp, vnode_t **vpp)
{
	struct vnode *vp;

	vp = vfs_bhvtomi(bdp)->mi_rootvp;
	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

/*
 * Get file system statistics.
 */
static int
nfs3_statvfs(bhv_desc_t *vfs_bdp, struct statvfs *sbp, vnode_t *vp)
{
	rnode_t *rp;
	int error;
	struct FSSTAT3args args;
	struct FSSTAT3res res;
	struct mntinfo *mi;
	int douprintf;
	bhv_desc_t *bdp;
	struct vfs *vfsp = bhvtovfs(vfs_bdp);
	vnode_t *realvp;

	mi = vfs_bhvtomi(vfs_bdp);

	if (vp != NULL) {
		/*
		 * We could have been passed a spec or fifo, so must check
		 * for an underlying real vp that is the nfs3 vnode.
		 */
		VOP_REALVP(vp, &realvp, error);
		if (!error)
			vp = realvp;
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &nfs3_vnodeops);
		ASSERT(bdp != NULL);
	} else {
		bdp = rtobhv(mi->mi_root);
		vp = mi->mi_rootvp;
	}

	args.fsroot = *bhvtofh3(bdp);

	douprintf = 1;
	rp = BHVTOR(bdp);

	error = rlock_nohang(rp);
	if (error) {
		return error;
	}

	error = rfs3call(mi, NFSPROC3_FSSTAT,
			xdr_FSSTAT3args, (caddr_t)&args,
			xdr_FSSTAT3res, (caddr_t)&res,
			get_current_cred(), &douprintf, &res.status);

	if (error) {
		runlock(rp);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		if (res.resok.obj_attributes.attributes)
#ifdef DEBUG
			if (nfs3_attrcache(bdp, 
					&res.resok.obj_attributes.attr,
					NOFLUSH, NULL))
				printf("bad nfs3_attrcache in nfs3_statvfs\n");
#else
			nfs3_attrcache(bdp, &res.resok.obj_attributes.attr,
					NOFLUSH, NULL);
#endif /* DEBUG */
		/* 
		 * JWS - This MAXBSIZE here is worrysome.  
		 * JWS - What should it be?
		 */
		sbp->f_bsize = MAXBSIZE;
		sbp->f_frsize = MAXBSIZE;
		sbp->f_blocks = (u_long)(res.resok.tbytes / sbp->f_bsize);
		sbp->f_bfree = (u_long)(res.resok.fbytes / sbp->f_bsize);
		sbp->f_bavail = (u_long)(res.resok.abytes / sbp->f_bsize);
		sbp->f_files = (u_long)res.resok.tfiles;
		sbp->f_ffree = (u_long)res.resok.ffiles;
		sbp->f_favail = (u_long)res.resok.afiles;
		/*
		 *	XXX - This is wrong, should be a real fsid
		 */
		sbp->f_fsid = (ulong_t)vfsp->vfs_fsid.val[0];
		strncpy(sbp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name,
			FSTYPSZ);
		sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
		sbp->f_namemax = (u_long)-1;
	} else {
		if (res.resfail.obj_attributes.attributes)
#ifdef DEBUG
			if (nfs3_attrcache(bdp, 
					&res.resfail.obj_attributes.attr,
					NOFLUSH, NULL))
				printf("bad nfs3_attrcache in nfs3_statvfs(bad)\n");
#else
			nfs3_attrcache(bdp,
					&res.resfail.obj_attributes.attr,
					NOFLUSH, NULL);
#endif /* DEBUG */
	}
	runlock(rp);
	return (error);
}

/*ARGSUSED*/
static int
nfs3_vget(bhv_desc_t *vfs_bdp, vnode_t **vpp, fid_t *fidp)
{
	struct mntinfo *mi;
	struct rnode *rp;
	struct vnode *vp;
	int error;
	int douprintf;
	struct GETATTR3args args;
	struct GETATTR3res res;

	*vpp = NULL;
	mi = vfs_bhvtomi(vfs_bdp);

	if ( fidp->fid_len < sizeof(fhandle_t) ) {
		return( EINVAL );
	}
	ASSERT( fidp->fid_len <= MAXFIDSZ );
	/*
	 * For proper alignment, we have to copy the file handle out from
	 * the fid to pass it to makenfsnode.  This is because fid_data is
	 * an array of char and fid_len is an unsigned short.  Thus, the
	 * file handle contained in fid_data is improperly aligned
	 * (i.e., it's on a half-word boundary).
	 */
	args.object.fh3_length = fidp->fid_len;
	bcopy(fidp->fid_data, args.object.fh3_u.data, fidp->fid_len);
	/*
	 * Search through this mount's rnodes for one that matches fidp.
	 * If we find a cached vnode, try to vn_get it (another process
	 * may be in the midst of uncaching it, so vn_get can fail).
	 * The fid is really the file handle.  This relies upon nfs_fid
	 * putting the file handle into fidp->fid_data.
	 */
retry:
	mutex_lock(&rnodemonitor, PINOD);
	if (rp = rfind(bhvtovfs(vfs_bdp), (fhandle_t *)&args.object)) {
		vmap_t vmap;
		struct vattr vattr;

		vp = rtov(rp);
		VMAP(vp, vmap);
		mutex_unlock(&rnodemonitor);
		if (vp = vn_get(vp, &vmap, 0)) {
			/*
			 * We need to make sure that the file has not
			 * been removed from the server.
			 * Check the attributes by calling nfs3_getattr.
			 * This makes nfs_vget more like nfs3_lookup.
			 * nfs3_lookup validates the attributes of the
			 * directory prior to going
			 * to the dnlc.
			 * We have to go through VOP_GETATTR because
			 * nfs3_getattr is declared static in a different
			 * file.
			 */
			vattr.va_mask = AT_TYPE;
			VOP_GETATTR(vp, &vattr, 0, sys_cred, error);
			if (error) {
				VN_RELE(vp);
				vp = NULL;
			}
			*vpp = vp;
			return(error);
		} else if (vmap.v_id == -1) {
			*vpp = NULL;
			return(ENOENT);
		}
		goto retry;
	}
	mutex_unlock(&rnodemonitor);
	/*
	 * If we get here, we could not locate the rnode.  Just make a new
	 * rnode since we know that the fid is really the file handle.
	 * Since we know the file handle, use it to get the attributes so they
	 * can be passed to makenfsnode.
	 */
	douprintf = 1;
	error = rfs3call(mi, NFSPROC3_GETATTR,
		xdr_GETATTR3args, (caddr_t)&args,
		xdr_GETATTR3res, (caddr_t)&res,
		sys_cred,  &douprintf, &res.status);
	if ( !error && !(error = geterrno(res.status)) ) {
		bhv_desc_t *bdp;

		error = makenfs3node(&args.object,
			    &res.res_u.ok.obj_attributes,
			    bhvtovfs(vfs_bdp), &bdp, sys_cred);
		if (!error) {
			*vpp = BHV_TO_VNODE(bdp);
		}
	}
	return( error );
}

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int
irix5_to_nfs_args(
        enum xlate_mode mode,
        void *to,
        int count,
        xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(irix5_nfs_args, nfs_args);

        target->addr = (struct sockaddr_in *)(__psint_t)source->addr;
        target->fh = (fhandle_t *)(__psint_t)source->fh;
        target->flags = source->flags;
        target->wsize = source->wsize;
        target->rsize = source->rsize;
        target->timeo = source->timeo;
        target->retrans = source->retrans;
        target->hostname = (char *)(__psint_t)source->hostname;
        target->acregmin = source->acregmin;
        target->acregmax = source->acregmax;
        target->acdirmin = source->acdirmin;
        target->acdirmax = source->acdirmax;
        target->symttl = source->symttl;
        target->bdsauto = source->bdsauto;
        target->bdswindow = source->bdswindow;
	target->bdsbuflen = source->bdsbuflen;
        bcopy((char *)(__psint_t)source->base, target->base, FSTYPSZ);
        target->namemax = source->namemax;
	target->fh_len = source->fh_len;
	target->pid = source->pid;

        return 0;
}
#endif  /* _ABI64 */
