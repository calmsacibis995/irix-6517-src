/*	@(#)nfs_vfsops.c	1.5 88/08/04 NFSSRC4.0 from 2.84 88/03/21 SMI	*/

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */
#ident	"$Revision: 1.151 $"

#include "types.h"
#include <sys/buf.h>		/* for B_ASYNC */
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/pda.h>
#include <sys/sema.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/pfdat.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <ksys/vproc.h>
#include <netinet/in.h>
#include <sys/xlate.h>
#include "auth_unix.h"		/* for NGRPS in nfs_init */
#include "xdr.h"
#define SVCFH
#include "nfs.h"
#include "export.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "bds.h"
#include "string.h"
#include "nfs_stat.h"
#include "lockmgr.h"
#include "nfs3_clnt.h"

extern char *rfsnames[];

int	nfs_fstype;
int	nfs3_fstype;	/* to make stubbing easier */
dev_t	nfs_major;
int	rpc_ngroups_max;


struct zone *nfsreadargs_zone;
struct zone *nfsrdresult_zone;
struct zone *nfswriteargs_zone;

lock_t mntinfolist_lock;

mntinfo_t *mntinfolist = NULL;

/*ARGSUSED*/
int
nfs_init(struct vfssw *vswp, int fstype)
{
	int i;
	extern lock_t async_lock;
	extern sv_t async_wait;
	extern zone_t *nfs_xdr_string_zone;
	extern void nfstcp_init(void);
	extern void chtables_init(void);

	nfs_fstype = fstype;
	if ((i = getudev()) < 0)
		cmn_err(CE_WARN, "nfs_init: can't get unique device number");
	else
		nfs_major = i;
	initrnodes();
	nfstcp_init();

	/*
	 * Initialize locks and semaphores.
	 */
	spinlock_init(&mntinfolist_lock, "nfs mntinfolist");
	spinlock_init(&async_lock, "nfs async");
	sv_init(&async_wait, SV_DEFAULT, "asyncwt");

	/*
	 * Initialize the device minor map.
	 */
	nfs_minormap.size = MINORMAP_SIZE;
	nfs_minormap.vec = kmem_zalloc(nfs_minormap.size, KM_SLEEP);
	spinlock_init(&nfs_minormap.lock, "nfs_minormap");

	/*
	 * Initialize zones.
	 */
	nfs_xdr_string_zone = kmem_zone_init(NFS_MAXNAMLEN, "xdrstr");

	/*
	 * Determine # of groups to use in RPC credentials.
	 */
	rpc_ngroups_max = MIN(ngroups_max, NGRPS);

	for (i = 0; i < maxcpus; i++) {
		if (pdaindr[i].CpuId == -1)
			continue;
                pdaindr[i].pda->nfsstat =
                        kmem_zalloc_node_hint(sizeof(struct nfs_stat),
				KM_SLEEP, pdaindr[i].pda->p_nodeid);
	}

	/* 
	 * Initialize client table array's and their corresponding
	 * mutexes.
	 */
	chtables_init();

	return 0;
}

extern mac_label *mac_admin_high_lp;

#if _MIPS_SIM == _ABI64
static int irix5_to_nfs_args(enum xlate_mode, void *, int, xlate_info_t *);
#endif


/*
 * nfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
static int
nfs_mount(vfsp, mvp, uap, attrs, cr)
	struct vfs *vfsp;
	struct vnode *mvp;
	struct mounta *uap;
	char *attrs;
	struct cred *cr;
{
	int ospl;
	int error;			/* error number */
	struct vnode *rtvp;		/* the server's root */
	struct nfs_args args;		/* nfs mount arguments */
	struct sockaddr_in saddr;	/* server's address */
	fhandle_t fh;			/* root fhandle */
	char hostname[HOSTNAMESZ];	/* server's hostname */
	struct mntinfo *mi;		/* mount info, pointed at by vfs */
	struct rnode *rp;
	extern struct ifnet *ifaddr_to_ifp(struct in_addr inaddr);

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return EPERM;

	/*
	 * get arguments
	 */
	if (error = COPYIN_XLATE(uap->dataptr, &args, MIN(uap->datalen, sizeof args),
			 irix5_to_nfs_args, get_current_abi(), 1))
		return error;

	/*
	 * Get server address
	 */
	if (copyin(args.addr, &saddr, sizeof saddr))
		return EFAULT;
	/*
	 * For now we just support AF_INET
	 */
	if (saddr.sin_family != AF_INET)
		return EPFNOSUPPORT;

	/*
	 * Get the root fhandle
	 */
	if (copyin(args.fh, &fh, sizeof fh))
		return EFAULT;

	/*
	 * Get server's hostname
	 */
	if (args.flags & NFSMNT_HOSTNAME) {
		if (copyin(args.hostname, hostname, HOSTNAMESZ))
			return EFAULT;
	} else {
		(void) strncpy(hostname, inet_ntoa(saddr.sin_addr),
			       sizeof hostname);
	}

	/*
	 * Get root vnode.
	 */
	error = makenfsroot(vfsp, &saddr, &fh, hostname, &rp, args.flags);
	if (error)
		return error;

	/*
	 * Set option fields in mount info record
	 */
	rtvp = rtov(rp);
	mi = rtomi(rp);
	/*
	 * mark loopback mounts of local file systems so that we can bypass
	 * buffer cache on the client side, thus avoiding deadlocks
	 */
	if (ifaddr_to_ifp(saddr.sin_addr) && (saddr.sin_port == NFS_PORT))
		mi->mi_flags |= MIF_LOCALLOOP;
	mi->mi_ipmac = vfsp->vfs_mac ?
	    vfsp->vfs_mac->mv_ipmac : mac_admin_high_lp;
	if (args.flags & NFSMNT_NOAC)
		mi->mi_flags |= MIF_NOAC;
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
	if (args.flags & NFSMNT_RETRANS) {
		mi->mi_retrans = args.retrans;
	}
	if (args.flags & NFSMNT_TIMEO) {
		mi->mi_timeo = args.timeo;
		if (args.timeo <= 0) {
			error = EINVAL;
			goto errout;
		}
	}
	if (args.flags & NFSMNT_RSIZE) {
		if (args.rsize <= 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_tsize = MIN(mi->mi_tsize, args.rsize);
	}
	if (args.flags & NFSMNT_WSIZE) {
		if (args.wsize <= 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_stsize = MIN(mi->mi_stsize, args.wsize);
	}
	if (args.flags & NFSMNT_ACREGMIN) {
		mi->mi_acregmin = args.acregmin;
	}
	if (args.flags & NFSMNT_ACREGMAX) {
		if (args.acregmax < mi->mi_acregmin) {
			error = EINVAL;
			printf("nfs_mount: acregmax == 0\n");
			goto errout;
		}
		mi->mi_acregmax = args.acregmax;
	}
	if (args.flags & NFSMNT_ACDIRMIN) {
		mi->mi_acdirmin = args.acdirmin;
	}
	if (args.flags & NFSMNT_ACDIRMAX) {
		if (args.acdirmax < mi->mi_acdirmin) {
			error = EINVAL;
			printf("nfs_mount: acdirmax == 0\n");
			goto errout;
		}
		mi->mi_acdirmax = args.acdirmax;
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
	/*
	 * If the user has specified asynchronous NLM calls, turn of the
	 * flag indicating the use of synchronous ones.
	 */
	if (args.flags & NFSMNT_ASYNCNLM) {
		mi->mi_nlm &= ~MI_NLM_SYNC;
	}
	if (args.flags & NFSMNT_SILENT) {
		atomicSetUint(&mi->mi_flags, MIF_SILENT);
	}
	if (args.flags & NFSMNT_PID) {
		mi->mi_pid = args.pid;
		VN_FLAGSET(mi->mi_rootvp, VROOTMOUNTOK);
	}
	mi->mi_rfsnames = rfsnames;
	ospl = mutex_spinlock(&mntinfolist_lock);
	mi->mi_next = mntinfolist;
	mi->mi_prev = NULL;
	if (mntinfolist)
		mntinfolist->mi_prev = mi;
	mntinfolist = mi;
	mutex_spinunlock(&mntinfolist_lock, ospl);
	return(0);

errout:
	if (rtvp) {	/* implies  mi != NULL  as well */
		vmap_t rtvmap;
		VMAP(rtvp, rtvmap);
		VN_RELE(rtvp);
		if (mi->mi_con)
		    nfscon_free(mi);
		vn_purge(rtvp, &rtvmap);
		vfs_putnum (&nfs_minormap, mi->mi_mntno);
		VFS_REMOVEBHV(vfsp, &mi->mi_bhv);
		mutex_destroy(&mi->mi_lock);
		kmem_free(mi, sizeof *mi);
	}
	return (error);
}

static int nfs_statvfs(bhv_desc_t *, struct statvfs *, struct vnode *);

int
makenfsroot(vfsp, sin, fh, hostname, rpp, flags)
	struct vfs *vfsp;		/* vfs of fs, if NULL make one */
	struct sockaddr_in *sin;	/* server address */
	fhandle_t *fh;			/* swap file fhandle */
	char *hostname;			/* swap server name */
	struct rnode **rpp;		/* result parameter root vp */
	int flags;			/* mount flags */
{
	struct mntinfo *mi;		/* mount info, pointed at by vfs */
	struct vnode *rtvp = NULL;	/* the server's root vnode */
	bhv_desc_t *rtbdp;		/* the server's root behavior */
	int error;			/* error number */
	struct cred *cr = NULL;		/* credentials for rtvp's rnode */
	struct rnode *rp = NULL;	/* root rnode pointer */
	int bhv_inserted = 0;		/* nfs behavior inserted yet? */
	struct statvfs sb;		/* server's file system stats */

	/*
	 * Create a mount record and link it to the vfs struct.
	 */
	mi = kmem_zalloc(sizeof *mi, KM_SLEEP);
	if ((flags & NFSMNT_SOFT) == 0)
		mi->mi_flags |= MIF_HARD;
	if ((flags & NFSMNT_NOINT) == 0)
		mi->mi_flags |= MIF_INT;
	if (flags & NFSMNT_BDS) {
		mi->mi_flags |= MIF_BDS;
		mi->mi_bds_blksize = mi->mi_bds_maxiosz = 4 * 1024 * 1024;
	}
	mi->mi_lmaddr = mi->mi_addr = *sin;
	mi->mi_lmaddr.sin_port = 0;
	mi->mi_nlm = MI_NLM_DEFAULT;
	mi->mi_retrans = NFS_RETRIES;
	mi->mi_timeo = NFS_TIMEO;
	if ((mi->mi_mntno = vfs_getnum(&nfs_minormap)) == -1) {
		error = ENFILE;
		goto bad;
	}
	mi->mi_fsidmaps = NULL;
	bcopy(hostname, mi->mi_hostname, HOSTNAMESZ);
	mi->mi_acregmin = ACREGMIN;
	mi->mi_acregmax = ACREGMAX;
	mi->mi_acdirmin = ACDIRMIN;
	mi->mi_acdirmax = ACDIRMAX;
	if (flags & NFSMNT_PRIVATE)
		mi->mi_flags |= MIF_PRIVATE;
	if (flags & NFSMNT_SHORTUID)
		mi->mi_flags |= MIF_SHORTUID;
	if (flags & NFSMNT_LOOPBACK)
		mi->mi_flags |= MIF_LOOPBACK;
	mi->mi_symttl = SYMTTL;
	mi->mi_ipmac = (vfsp->vfs_mac) ?
	    vfsp->vfs_mac->mv_ipmac : mac_admin_high_lp;
	init_mutex(&mi->mi_lock, MUTEX_DEFAULT, "nmi", mi->mi_mntno);
	mi->mi_prog = NFS_PROGRAM;
	mi->mi_vers = NFS_VERSION;
	mi->mi_nlmvers = (u_long)NLM_VERS;
        mi->mi_tsize = NFS_MAXDATA;
	if (flags & NFSMNT_TCP) {
		mi->mi_flags |= MIF_TCP;
		if (error = nfscon_get(mi))
			goto bad;
        }

	/*
	 * Make a vfs fsid (aka mount.m_dev) from the mount counter.
	 * The fsid is used only to distinguish mount table entries,
	 * and to distinguish inodes from different filesystems.
	 */
	vfsp->vfs_fsid.val[0] = vfsp->vfs_dev =
		makedev(nfs_major, mi->mi_mntno);
	vfsp->vfs_fsid.val[1] = nfs_fstype;
	vfs_insertbhv(vfsp, &mi->mi_bhv, &nfs_vfsops, mi);
	bhv_inserted = 1;
	vfsp->vfs_fstype = nfs_fstype;
	strncpy(mi->mi_basetype, vfssw[nfs_fstype].vsw_name,
		sizeof mi->mi_basetype);


	/*
	 * XXX Instead of making the
	 * root vnode twice, which would require throwing it out of the
	 * rnode cache before the second makenfsnode, we call nfs_getattr,
	 * n2v_type, and n2v_rdev.
	 */
	error = makenfsnode(vfsp, mi, fh, NULL, &rtbdp);
	if (error) {
		goto bad;
	}
	rtvp = BHV_TO_VNODE(rtbdp);
	if ((rtvp->v_flag & VROOT) != 0) {
		error = EBUSY;
		goto bad;
	}

	/*
	 * Get credentials and attributes.  Set v_type and v_rdev from the
	 * nfs attributes.  Link vfsp and mi to rtvp.
	 */
	cr = get_current_cred();
	crhold(cr);
	error = nfs_getattr(rtbdp, NULL, 0, cr);
	if (error) {
		goto bad;
	}
	rp = bhvtor(rtbdp);
	rtvp->v_type = n2v_type(&rp->r_nfsattr);
	rtvp->v_rdev = n2v_rdev(&rp->r_nfsattr);
	VN_FLAGSET(rtvp, VROOT);
	mi->mi_rootvp = rtvp;
	mi->mi_root = rp;
	mi->mi_rootfsid = rp->r_nfsattr.na_fsid;

	/*
	 * Get server's filesystem stats.  Use these to set transfer
	 * sizes and filesystem block size.
	 */
	error = nfs_statvfs(&mi->mi_bhv, &sb, rtvp);
	if (error)
		goto bad;

#if 0	/* This works unless your page size is > 8192. In any case we
	 * don't want to increase NFS_MAXDATA beyond 8192.
	 */
	/*
	 * Set filesystem block size to maximum data transfer size, which
	 * must be congruent with page size.
	 */
#if NFS_MAXDATA & NBPC-1
? ? ? "What? NFS_MAXDATA isn't page-congruent!"
#endif
#endif
	mi->mi_bsize = MAX(NFS_MAXDATA, NBPP);

	/*
	 * Need credentials in the rtvp so do_bio can find them
	 * (rtvp is really a swap or dump file).
	 */
	bhvtor(rtbdp)->r_cred = cr;

	*rpp = rp;
	return 0;
bad:
	if (rtvp) {
		vmap_t rtvmap;
		VMAP(rtvp, rtvmap);
		VN_RELE(rtvp);
		vn_purge(rtvp, &rtvmap);
	}
        if (mi->mi_con)
            nfscon_free(mi);
	if (mi->mi_mntno != -1) {
		vfs_putnum (&nfs_minormap, mi->mi_mntno);
		mutex_destroy(&mi->mi_lock);
	}
	if (bhv_inserted) {
		VFS_REMOVEBHV(vfsp, &mi->mi_bhv);
	}
	kmem_free(mi, sizeof *mi);
	if (cr) {
		crfree(cr);
	}
	return error;
}

/*
 * Nfs_umount is the FSS hook corresponding to nfs_unmount the vfsop.
 */
/* ARGSUSED */
static int
nfs_unmount(bdp, flags, cr)
	bhv_desc_t *bdp;
	int flags;
	struct cred *cr;
{
	int ospl;
	struct mntinfo *mi;
	struct vnode *rtvp;
	struct fsidmap *fsid, *nextfsid;
	vmap_t rtvmap;
	extern int nfs_wakeup(struct mntinfo *);

	if (!_CAP_CRABLE(cr, CAP_MOUNT_MGT))
		return EPERM;

	klm_shutdown();
	/*
	 * Purge the name and rnode caches for this filesystem.
	 * We don't need to purge the page cache by vfsp because
	 * rflush destroys all rnodes, purging file by file.
	 */
	mi = vfs_bhvtomi(bdp);
	if (!rflush(mi))
		return EBUSY;

	/*
	 * Insist on only one reference for the root vnode.
	 */
	ASSERT(mi->mi_refct == 1);
	rtvp = mi->mi_rootvp;
	ospl = mutex_spinlock(&mntinfolist_lock);
	if ((rtvp->v_count != 1) || (mi->mi_nlm & MI_NLM_RECLAIMING)) {
		mutex_spinunlock(&mntinfolist_lock, ospl);
		return EBUSY;
	}
	if (mi->mi_flags & MIF_BDS)
		bds_unmount(mi);
	if (mi->mi_next) {
		mi->mi_next->mi_prev = mi->mi_prev;
	}
	if (mi->mi_prev) {
		mi->mi_prev->mi_next = mi->mi_next;
	} else {
		mntinfolist = mi->mi_next;
	}
	mutex_spinunlock(&mntinfolist_lock, ospl);

	VMAP(rtvp, rtvmap);
	VN_RELE(rtvp);
	vn_purge(rtvp, &rtvmap);
	ASSERT(mi->mi_rnodes == NULL);

	vfs_putnum(&nfs_minormap, mi->mi_mntno);
	for (fsid = mi->mi_fsidmaps; fsid; fsid = nextfsid) {
		vfs_putnum(&nfs_minormap, minor(fsid->fsid_local));
		nextfsid = fsid->fsid_next;
		kmem_free(fsid, sizeof *fsid);
	}
	VFS_REMOVEBHV(bhvtovfs(bdp), &mi->mi_bhv);
	mutex_destroy(&mi->mi_lock);
        if (mi->mi_flags & MIF_TCP)
            nfscon_free(mi);
	kmem_free(mi, sizeof *mi);
	return 0;
}

static int
nfs_root(bhv_desc_t *bdp, struct vnode **vpp)
{
	struct vnode *vp;
	
	vp = vfs_bhvtomi(bdp)->mi_rootvp;
	VN_HOLD(vp);
	*vpp = vp;
	return 0;
}

/*
 * Get file system statistics.
 */
static int
nfs_statvfs(vfs_bdp, sbp, vp)
	register bhv_desc_t *vfs_bdp;
	register struct statvfs *sbp;
	struct vnode *vp;
{
	struct mntinfo *mi;
	fhandle_t *fh;
	struct nfsstatfs fs;
	int error;
	vfs_t *vfsp = bhvtovfs(vfs_bdp);
	vnode_t *realvp;

	if (vfs_bdp == NULL)
		return EINVAL;
	mi = vfs_bhvtomi(vfs_bdp);
	if (vp != NULL) {
		bhv_desc_t *bdp;

		/*
		 * We could have been passed a spec or fifo, so must check
		 * for an underlying real vp that is the nfs vnode.
		 */
		VOP_REALVP(vp, &realvp, error);
		if (!error)
			vp = realvp;
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &nfs_vnodeops);
		ASSERT(bdp != NULL);
		fh = bhvtofh(bdp);
	} else {
		fh = rtofh(mi->mi_root);
	}
	error = rfscall(mi, RFS_STATFS, xdr_fhandle, (caddr_t)fh,
			xdr_statfs, (caddr_t)&fs, get_current_cred(), 0);
	if (!error) {
		error = geterrno(fs.fs_status);
	}
	if (!error) {
		if (mi->mi_stsize) {
			mi->mi_stsize = MIN(mi->mi_stsize, fs.fs_tsize);
		} else {
			mi->mi_stsize = fs.fs_tsize;
		}
		/* limit server's max transfer size to be 8K */
		mi->mi_stsize = MIN(mi->mi_stsize, NFS_MAXDATA);

		sbp->f_bsize = fs.fs_bsize;
		sbp->f_frsize = fs.fs_bsize;
		sbp->f_blocks = fs.fs_blocks;
		sbp->f_bfree = fs.fs_bfree;
		sbp->f_bavail = fs.fs_bavail;
		sbp->f_files = -1;
		sbp->f_ffree = -1;
		sbp->f_favail = -1;
                /*
                 *      XXX - This is wrong, should be a real fsid
                 */
				sbp->f_fsid = (ulong_t)vfsp->vfs_fsid.val[0];
                strncpy(sbp->f_basetype,mi->mi_basetype,sizeof sbp->f_basetype);
                sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
		if (mi->mi_flags & MIF_LOOPBACK)
			sbp->f_flag |= ST_LOCAL;
		sbp->f_namemax = mi->mi_namemax;

		/*
		 * Copy the server's hostname into f_fstr.
		 */
		strncpy(sbp->f_fstr, mi->mi_hostname, sizeof sbp->f_fstr);
	}
	return error;
}

/*
 * Flush any pending I/O to file system vfsp.
 */
#define PREEMPT_MASK	0x7f

/*ARGSUSED*/
int
nfs_sync(vfs_bdp, flags, cr)
	bhv_desc_t *vfs_bdp;
	int flags;
	struct cred *cr;
{
	struct mntinfo *mi;
	int error, lasterr, mntlock;
	unsigned int preempt;
	struct rnode *rp;
	struct vnode *vp;
	vmap_t vmap;
	u_long rdestroys;
	/*REFERENCED*/
	bhv_desc_t *bdp;


	error = lasterr = 0;

	/*
	 * if not interested in rnodes, skip all this
	 */
	if ((flags & (SYNC_DELWRI|SYNC_CLOSE|SYNC_PDFLUSH)) == 0)
		goto end;

	mi = vfs_bhvtomi(vfs_bdp);
loop:
	milock(mi);
	mntlock = 1;
	for (rp = mi->mi_rnodes; rp != 0; rp = rp->r_mnext) {

		if ((vp = rtov(rp)) == NULL)
			/* racing with someone creating a new rnode */
			continue;

		/*
		 * ignore rnodes which need no flushing
		 */
		if (flags & SYNC_DELWRI) {
			if (!VN_DIRTY(vp))
				continue;
		}
		else if (flags & SYNC_PDFLUSH) {
			if (!vp->v_dpages)
				continue;
		}

		/*
		 * don't diddle with swap files - too dangerous since
		 * we can sleep waiting for memory holding the inode locked
		 */
		if (vp->v_flag & VISSWAP)
			continue;

		/*
		 * Try to lock rwlock without sleeping.  If we can't, we must
		 * unlock mi, get vp (potentially off the vnode freelist),
		 * and sleep for rwlock.
		 */
		if (nfs_rwlock_nowait(rp) == 0) {
			if (flags & (SYNC_BDFLUSH|SYNC_PDFLUSH))
				continue;
			mntlock = 0;
			VMAP(vp, vmap);
			rdestroys = mi->mi_rdestroys;
			miunlock(mi);
			if (!(vp = vn_get(vp, &vmap, 0)))
				goto loop;
#ifdef DEBUG
			bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp),
						     &nfs_vnodeops);
			if (bdp == NULL)
				bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp),
							     &nfs3_vnodeops);
			ASSERT(bdp != NULL);
			ASSERT(rp == bhvtor(bdp));
#endif
			nfs_rwlock(rtobhv(rp), VRWLOCK_WRITE);
			VN_RELE(vp);
		}

		if (flags & SYNC_CLOSE) {
			VOP_FLUSHINVAL_PAGES(vp, 0, rp->r_size - 1, FI_NONE);

		} else if (flags & SYNC_PDFLUSH) {
			if (vp->v_dpages) {
				if (mntlock) {
					rdestroys = mi->mi_rdestroys;
					miunlock(mi);
					mntlock = 0;
				}
				pdflush(vp, B_DELWRI);
			}
		}
		else if ((flags & SYNC_DELWRI) && VN_DIRTY(vp)) {
			if (mntlock) {
				rdestroys = mi->mi_rdestroys;
				miunlock(mi);
				mntlock = 0;
			}
			VOP_FLUSH_PAGES(vp, (off_t)0, rp->r_size - 1,
					flags & SYNC_WAIT ? 0 : B_ASYNC, FI_NONE, error);
		}

		/*
		 * Release rp, check error and whether to preempt, and if
		 * we let go of mi's lock and someone has deleted an rnode
		 * from the mi_rnodes list, restart the loop.
		 */
		nfs_rwunlock(rtobhv(rp), VRWLOCK_WRITE);
		if (error)
			lasterr = error;

		if (mntlock == 0) {
			preempt = 0;
			mntlock = 1;
			goto done_preempt;
		} else
		if ((++preempt & PREEMPT_MASK) == 0)  {
			/* release mount point lock */
			rdestroys = mi->mi_rdestroys;
			miunlock(mi);
	done_preempt:
			milock(mi);
			if (mi->mi_rdestroys != rdestroys) {
				miunlock(mi);
				goto loop;
			}
		}
	}
	miunlock(mi);

end:
	return lasterr;
}

int
nfs_vget(vfs_bdp, vpp, fid)
	bhv_desc_t *vfs_bdp;
	struct vnode **vpp;
	struct fid *fid;
{
	struct mntinfo *mi;
	struct rnode *rp;
	struct vnode *vp;
	bhv_desc_t *bdp;
	fhandle_t fh;
	int error;
	struct nfsattrstat ns;
	struct vfs *vfsp = bhvtovfs(vfs_bdp);

	*vpp = NULL;
	mi = vfs_bhvtomi(vfs_bdp);

	if ( fid->fid_len < sizeof(fhandle_t) ) {
		return( EINVAL );
	}
	ASSERT( fid->fid_len <= MAXFIDSZ );
	/*
	 * For proper alignment, we have to copy the file handle out
	 * from the fid to pass it to makenfsnode.  This is because
	 * fid_data is an array of char and fid_len is an unsigned
	 * short.  Thus, the file handle contained in fid_data is
	 * improperly aligned (i.e., it's on a half-word boundary).
	 */
	bcopy( fid->fid_data, &fh, fid->fid_len );
	/*
	 * Search through this mount's rnodes for one that matches fid.
	 * If we find a cached vnode, try to vn_get it (another process
	 * may be in the midst of uncaching it, so vn_get can fail).
	 * The fid is really the file handle.  This relies upon nfs_fid
	 * putting the file handle into fid->fid_data.
	 */
retry:
	mutex_lock(&rnodemonitor, PINOD);
	if (rp = rfind(vfsp, &fh)) {
		vmap_t vmap;
		vp = rtov(rp);
		VMAP(vp, vmap);
		mutex_unlock(&rnodemonitor);
		if (vp = vn_get(vp, &vmap, 0)) {
			/*
			 * We need to make sure that the file has not
			 * been removed from the server.
			 * Check the attributes by calling nfs_getattr
			 * with a NULL vattr pointer.  We don't need
			 * the attributes to be returned here.
			 * This makes nfs_vget more like nfs_lookup.
			 * nfs_lookup validates the attributes of the
			 * directory prior to going
			 * to the dnlc.
			 */
			error = nfs_getattr(rtobhv(rp),
				(vattr_t *)NULL, 0, sys_cred);
			if (error) {
				VN_RELE(vp);
				vp = NULL;
			}
			*vpp = vp;
			return(error);
		} else if (vmap.v_id == -1) {
			*vpp = 0;
			return ENOENT;
		}
		goto retry;
	}
	mutex_unlock(&rnodemonitor);
	/*
	 * If we get here, we could not locate the rnode.
	 * Just make a new rnode since we know that the fid is really the file
	 * handle.
	 * Since we know the file handle, use it to get the attributes so they
	 * can be passed to makenfsnode.
	 */
	error = rfscall(mi, RFS_GETATTR, xdr_fhandle, (caddr_t)fid->fid_data,
		xdr_attrstat, (caddr_t)&ns, sys_cred, 0);
	if ( !error && !(error = geterrno(ns.ns_status)) ) {
		error = makenfsnode(vfsp, mi, &fh, &ns.ns_attr, &bdp);
		if (!error) {
			*vpp = BHV_TO_VNODE(bdp);
		}
	}
	return( error );
}

static int
nfs_rootinit(vfsp)
	struct vfs *vfsp;
{
	if (time == 0)
		time++;
	return nfs_rmount(vfsp, ROOT_INIT);
}


/* ARGSUSED */
static int
nfs_mountroot(bdp, why)
	bhv_desc_t *bdp;
	enum whymountroot why;
{
	if (why == ROOT_REMOUNT) {
		return 0;
	} else if (why == ROOT_UNMOUNT) {
		return 0;
	} else {
		cmn_err(CE_PANIC, "nfs_mountroot: invalid argument");
	}
	/*NOTREACHED*/
}

vfsops_t nfs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	nfs_mount,
	nfs_rootinit,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_dounmount,
	nfs_unmount,
	nfs_root,
	nfs_statvfs,
	nfs_sync,
	nfs_vget,
	nfs_mountroot,
	fs_nosys,	/* realvfsops */
	fs_import,	/* import */
	fs_quotactl     /* quotactl */
};

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

	return 0;
}
#endif	/* _ABI64 */
