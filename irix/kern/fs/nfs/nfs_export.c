/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */
#ident      "$Revision: 1.70 $"

#define NFSSERVER
#include "types.h"
#include <sys/debug.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <netinet/in.h>
#include <sys/capability.h>
#include <sys/uuid.h>
#include "auth.h"
#include "auth_unix.h"
#include "svc.h"
#include "nfs.h"
#include "export.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include <sys/xlate.h>

#define	eqfsid(f1, f2) (((f1)->val[0] == (f2)->val[0]) && ((f1)->val[1] == (f2)->val[1]))

#define eqfid(fid1, fid2) \
	((fid1)->fid_len == (fid2)->fid_len && \
	bcmp((char *)(fid1)->fid_data, (char *)(fid2)->fid_data,  \
	(int)(fid1)->fid_len) == 0)

/*
 * In the case of exported loopback mounts, the real fsid is in the first
 * 8 bytes (sizeof fsid_t) of exi_fid.fid_data.  This must be matched
 * against the supplied fsid.  The fsid in exi_fsid is the fsid of the NFS
 * mount point for the loopback mount.
 */
#define exportmatch(exi, fsid, fid) \
	(((exi)->exi_loopback) ? \
	((bcmp((exi)->exi_fid->fid_data, fsid, 2*sizeof(uint)) == 0) && eqfid((exi)->exi_fid, fid)) : \
	(eqfsid(&(exi)->exi_fsid, fsid) && eqfid((exi)->exi_fid, fid)))



extern int nfs_fstype;

mrlock_t exported_lock;
#if _MIPS_SIM == _ABI64
static int irix5_to_export(enum xlate_mode, void *, int, xlate_info_t *);
#endif

extern void release_all_locks(vfs_t *vfsp);
struct xfs_fid;
extern int xfs_fast_fid(struct bhv_desc *, struct xfs_fid *);

/*
 * Initialization routine for export routines. Should only be called once.
 */
int
nfs_exportinit(void)
{
	return (0);
}

/*
 * Exportfs system call
 */
struct exportfsa {
	char		*dname;
	struct export	*uex;
};

int
exportfs(struct exportfsa *uap)
{
	struct vnode *vp;
	struct export *kex;
	struct exportinfo *exi;
	struct fid *fid;
	fsid_t fsid;
	struct vfs *vfs;
	int error;
	int loopback = 0;

	if (! _CAP_ABLE(CAP_MOUNT_MGT)) {
		return EPERM;
	}

	/*
	 * Get the vfs id
	 */
	error = lookupname(uap->dname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL);
	if (error) {
		return error;
	}
	VOP_FID(vp, &fid, error);
	vfs = vp->v_vfsp;
	fsid = vfs->vfs_fsid;
	VN_RELE(vp);
	if (error) {
		return error;
	} else if ( vfs->vfs_fstype == nfs_fstype ) {
		bhv_desc_t *vfs_bdp = bhv_base_unlocked(VFS_BHVHEAD(vfs));
		/*
		 * Handle exported loopback mounts here.
		 * This is for the support of user-level file systems
		 * implemented as NFS server mimics.
		 * Such file systems can be detected by looking at mi_loopback
		 * in the mntinfo structure.
		 * If mi_loopback is not set, return ENFSREMOTE.
		 */
		if ( !(vfs_bhvtomi(vfs_bdp)->mi_flags & MIF_LOOPBACK) ) {
			freefid(fid);
			return ENFSREMOTE;
		} else {
			fhandle_t fh;
			int newfid_len;

			/*
			 * set the loopback indicator to TRUE so that the export entry
			 * will be appropriately marked
			 */
			loopback = 1;
			/*
			 * we got a file handle
			 * we only want part of the file handle, so copy it out and
			 * free the fid
			 */
			bcopy(fid->fid_data, &fh, fid->fid_len);
			freefid(fid);
			/*
			 * now make a new fid with fid_data containing the truncated
			 * file handle
			 */
			newfid_len = NFS_FHMAXDATA;
			fid = mem_alloc(sizeof(fid_t) - MAXFIDSZ + newfid_len);
			fid->fid_len = newfid_len;
			bcopy(&fh, fid->fid_data, newfid_len);

			bcopy(fid->fid_data, &fsid, sizeof (fsid_t));
		}
	}

	if (uap->uex == NULL) {
		EXPORTED_MRWLOCK();
		error = unexport(&fsid, fid);
		EXPORTED_MRUNLOCK();
		freefid(fid);
		return error;
	}
	exi = mem_alloc(sizeof(struct exportinfo));
	bzero(exi, sizeof(struct exportinfo));
	exi->exi_fsid = fsid;
	exi->exi_fid = fid;
	exi->exi_loopback = loopback;
	exi->exi_vfsp = vfs;
	kex = &exi->exi_export;

	/*
	 * Load in everything, and do sanity checking
	 */
	if (COPYIN_XLATE(uap->uex, kex, sizeof *kex,
			irix5_to_export, get_current_abi(), 1)) {
		error = EFAULT;
	}
	if (error) {
		goto error_return;
	}
	if (kex->ex_flags & ~EX_ALLFLAGS) {
		error = EINVAL;
		goto error_return;
	}
	if (kex->ex_flags & EX_RDMOSTLY) {
		error = loadaddrs(&kex->ex_writeaddrs, EXMAXADDRS,
				  &exi->exi_writehash);
		if (error) {
			goto error_return;
		}
	}
	switch (kex->ex_auth) {
	case AUTH_UNIX:
		error = loadaddrs(&kex->ex_unix.rootaddrs, EXMAXROOTADDRS,
				  &exi->exi_roothash);
		error = loadaddrs(&kex->ex_accessaddrs, EXMAXADDRS,
				&exi->exi_accesshash);
		break;
	default:
		error = EINVAL;
	}
	if (error) {
		goto error_return;
	}

	EXPORTED_MRWLOCK();
	export(exi);
	EXPORTED_MRUNLOCK();

	return 0;

error_return:
	freefid(exi->exi_fid);
	mem_free(exi, sizeof(struct exportinfo));
	return error;
}

/*
 * Add the exportinfo entry to the export list.
 */
void
export(exi)
struct	exportinfo	*exi;
{
	struct	exportinfo	**head;

	(void) unexport(&exi->exi_fsid, exi->exi_fid);

	head = &EXIHASH(&exi->exi_fsid);
	exi->exi_next = *head;
	*head = exi;
}


/*
 * Remove the exported directory from the export list
 */
unexport(fsid, fid)
	fsid_t *fsid;
	struct fid *fid;
{
	struct exportinfo **tail;
	struct exportinfo *exi;
	vfs_t *vfsp;

	for (tail = &EXIHASH(fsid); *tail; tail = &(*tail)->exi_next)
		if (exportmatch(*tail, fsid, fid)) {
			exi = *tail;
			*tail = (*tail)->exi_next;
			exportfree(exi);
			vfsp = getvfs(fsid);
			if (vfsp) {
				release_all_locks(vfsp);
			}
			return (0);
		}

	return (EINVAL);
}

/*
 * Get file handle system call.
 * Takes file name and returns a file handle for it.
 * Also recognizes the old getfh() which takes a file
 * descriptor instead of a file name, and does the
 * right thing. This compatibility will go away in 5.0.
 * It goes away because if a file descriptor refers to
 * a file, there is no simple way to find its parent
 * directory.
 */
struct getfha {
	char	    *fname;
	fhandle_t   *fhp;
};

int
nfs_getfh(struct getfha *uap)
{
	struct vfile *fp;
	fhandle_t fh;
	struct vnode *vp;
	struct vnode *dvp;
	struct exportinfo *exi;
	int error;
	int oldgetfh = 0;

	if (!uap) {
		return EINVAL;
	}
	if (!_CAP_ABLE(CAP_MOUNT_MGT)) {
		return EPERM;
	}
	if ((__psunsigned_t)uap->fname < fdt_nofiles()) {
		/*
		 * old getfh()
		 */
		oldgetfh = 1;
		error = getf((__psint_t)uap->fname, &fp);
		if (error) {
			return error;
		}
		if (!VF_IS_VNODE(fp)) {
			return EINVAL;
		}
		vp = VF_TO_VNODE(fp);
		dvp = NULL;
	} else {
		/*
		 * new getfh()
		 */
		error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW,
				   &dvp, &vp, NULL);
		if (error == EINVAL) {
			/*
			 *  if fname resolves to / we get EEXIST error
			 *  since we wanted the parent vnode.  Try
			 *  again with NULL dvp.
			 */
			error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW,
					   NULLVPP, &vp, NULL);
			ASSERT(vp == NULL || vp->v_type == VDIR);
			dvp = NULL;
		}
		if (error == 0 && vp == NULL) {
			/*
			 *  Last component of fname not found
			 */
			if (dvp) {
				VN_RELE(dvp);
			}
			error = ENOENT;
		}
		if (error) {
			return error;
		}
	}
	error = findexivp(&exi, dvp, vp);
	if (!error) {
		error = makefh(&fh, vp, exi);
		EXPORTED_MRUNLOCK();
		if (!error) {
			ASSERT(sizeof fh == NFS_FHSIZE);
			if (copyout(&fh, uap->fhp, sizeof fh) < 0) {
				error = EFAULT;
			}
		}
	}
	if (!oldgetfh) {
		/*
		 * new getfh(): release vnodes
		 */
		VN_RELE(vp);
		if (dvp != NULL) {
			VN_RELE(dvp);
		}
	}
	return error;
}

/*
 * Common code for both old getfh() and new getfh().
 * If old getfh(), then dvp is NULL.
 * Strategy: if vp is in the export list, then
 * return the associated file handle. Otherwise, ".."
 * once up the vp and try again, until the root of the
 * filesystem is reached.
 *
 * Return: (0 w/export lock held) or (nonzero w/o export lock held).
 */
findexivp(exip, dvp, vp)
	struct exportinfo **exip;
	struct vnode *dvp;  /* parent of vnode want fhandle of */
	struct vnode *vp;   /* vnode we want fhandle of */
{
	struct fid *fid;
	int error;
	fsid_t *fsidp;
	fhandle_t fh;

	VN_HOLD(vp);
	if (dvp != NULL) {
		VN_HOLD(dvp);
	}
	for (;;) {
		VOP_FID(vp, &fid, error);
		if (error) {
			break;
		} else if ( vp->v_vfsp->vfs_fstype == nfs_fstype ) {
			bhv_desc_t *bdp = vn_bhv_base_unlocked(VN_BHV_HEAD(vp));

			if ( !(vn_bhvtomi(bdp)->mi_flags & MIF_LOOPBACK) ) { 
				freefid(fid);
				error = ENFSREMOTE;
				break;
			} else {
				int newfid_len;

				/*
				 * we got a file handle
				 * we only want part of the file handle, so copy it out and
				 * free the fid
				 * we copy the file handle out so we can access its fields
				 * without alignment difficulties
				 */
				bcopy(fid->fid_data, &fh, fid->fid_len);
				freefid(fid);
				/*
				 * now make a new fid with fid_data containing the truncated
				 * file handle
				 */
				newfid_len = NFS_FHMAXDATA;
				fid = mem_alloc(sizeof(fid_t) - MAXFIDSZ + newfid_len);
				fid->fid_len = newfid_len;
				bcopy(&fh, fid->fid_data, newfid_len);
				fsidp = &fh.fh_fsid;
			}
		} else {
			fsidp = &vp->v_vfsp->vfs_fsid;
		}
		*exip = findexport(fsidp, fid);
		freefid(fid);
		if (*exip != NULL) {
			/*
			 * Found the export info
			 */
			error = 0;
			break;
		}

		/*
		 * We have just failed finding a matching export.
		 * If we're at the root of this filesystem, then
		 * it's time to stop (with failure).
		 */
		if (vp->v_flag & VROOT) {
			error = EINVAL;
			break;
		}

		/*
		 * Now, do a ".." up vp. If dvp is supplied, use it,
	 	 * otherwise, look it up.
		 */
		if (dvp == NULL) {
			if (vp->v_type != VDIR) {
				error = EINVAL;
				break;
			}
			VOP_LOOKUP(vp, "..", &dvp,
				(struct pathname *)NULL, 0,
				curuthread->ut_rdir, get_current_cred(),
				error);
			if (error) {
				break;
			}
		}
		VN_RELE(vp);
		vp = dvp;
		dvp = NULL;
	}
	VN_RELE(vp);
	if (dvp != NULL) {
		VN_RELE(dvp);
	}
	return (error);
}

extern struct vnodeops xfs_vnodeops;

/*
 * Make an fhandle from a vnode
 */
makefh(fh, vp, exi)
	register fhandle_t *fh;
	struct vnode *vp;
	struct exportinfo *exi;
{
	struct fid *fidp;
	bhv_desc_t *bdp;
	int error;
	fhandle_t newfh;

	ASSERT(EXPORTED_MRRHELD());

	bdp = vn_bhv_base_unlocked(VN_BHV_HEAD(vp));
	if (BHV_OPS(bdp) == &xfs_vnodeops) {
		/*
		 * Usual XFS case - avoid kmem_alloc/free
		 */
		fh->fh_fsid = vp->v_vfsp->vfs_fsid;
		ASSERT(exi->exi_fid->fid_len == NFS_FHMAXDATA);
#define COPY_FID(dst,src) \
			((int *)(dst)) [0] = ((int *)(src)) [0]; \
			((int *)(dst)) [1] = ((int *)(src)) [1]; \
			((int *)(dst)) [2] = ((int *)(src)) [2]; 

		COPY_FID( &fh->fh_xlen, &exi->exi_fid->fid_len);
		return xfs_fast_fid(bdp, (struct xfs_fid *)&fh->fh_len);
	}

#pragma mips_frequency_hint NEVER
	fidp = (struct fid *)0;
	VOP_FID(vp, &fidp, error);
	if (error || fidp == NULL) {
		return (ENFSREMOTE);
	} else if ( vp->v_vfsp->vfs_fstype == nfs_fstype ) {
		if ( !(vn_bhvtomi(bdp)->mi_flags & MIF_LOOPBACK) ||
			(exi->exi_fid->fid_len > NFS_FHMAXDATA) ) {
				error = ENFSREMOTE;
		} else {
			/*
			 * what we get back in the file ID is a file handle
			 * only, it is a partial file handle
			 * we'll have to overlay the data from the export entry
			 * copy first only the bytes corresponding to fh_fsid,
			 * fh_len, and fh_data
			 * next copy the export entry fid into the file handle field
			 * fh->fh_xdata
			 */
			bzero((caddr_t) fh, sizeof(*fh));
			bcopy(fidp->fid_data, &newfh, fidp->fid_len);
			fh->fh_fsid = newfh.fh_fsid;
			fh->fh_len = newfh.fh_len;
			bcopy(newfh.fh_data, fh->fh_data, NFS_FHMAXDATA);
			fh->fh_xlen = exi->exi_fid->fid_len;
			bcopy(exi->exi_fid->fid_data, fh->fh_xdata, exi->exi_fid->fid_len);
		}
	} else if ((fidp->fid_len > NFS_FHMAXDATA) ||
	    (exi->exi_fid->fid_len > NFS_FHMAXDATA)) {
			error = ENFSREMOTE;
	} else {
		bzero((caddr_t) fh, sizeof(*fh));
		fh->fh_fsid = vp->v_vfsp->vfs_fsid;
		fh->fh_len = fidp->fid_len;
		bcopy(fidp->fid_data, fh->fh_data, fidp->fid_len);
		fh->fh_xlen = exi->exi_fid->fid_len;
		bcopy(exi->exi_fid->fid_data, fh->fh_xdata, fh->fh_xlen);
	}
	freefid(fidp);
	return (error);
}

int
makefh3(nfs_fh3 *fh, struct vnode *vp, struct exportinfo *exi)
{
	struct fid *fidp;
	int error;
	register char *p, *q;
	register int i;

	bhv_desc_t *bdp;
	fhandle_t newfh;


	ASSERT(EXPORTED_MRRHELD());

	fh->fh3_length = sizeof (fh->fh3_u.nfs_fh3_i);
	bdp = vn_bhv_base_unlocked(VN_BHV_HEAD(vp));
	if (BHV_OPS(bdp) == &xfs_vnodeops) {
		/*
		 * Usual XFS case - avoid kmem_alloc/free
		 */
		fh->fh3_fsid = vp->v_vfsp->vfs_fsid;
		ASSERT(exi->exi_fid->fid_len == NFS_FHMAXDATA);
		COPY_FID( &fh->fh3_xlen, &exi->exi_fid->fid_len);
		return (xfs_fast_fid(bdp,
				  (struct xfs_fid*) &(fh->fh3_len)));
	}


	fidp = (struct fid *) 0;
	VOP_FID(vp, &fidp, error);
	if (error || fidp == NULL) {
		return (EREMOTE);
	} else if (vp->v_vfsp->vfs_fstype == nfs_fstype) {
		/* loopback stuff */
		if ( !(vn_bhvtomi(bdp)->mi_flags & MIF_LOOPBACK) ||
			(exi->exi_fid->fid_len > sizeof (fh->fh3_data))) {
				error = ENFSREMOTE;
		} else {
			/*
			 * what we get back in the file ID is a file handle
			 * only, it is a partial file handle
			 * we'll have to overlay the data from the export entry
			 * copy first only the bytes corresponding to fh_fsid,
			 * fh_len, and fh_data
			 * next copy the export entry fid into the file handle field
			 * fh->fh_xdata
			 */
			bzero((caddr_t) &fh->fh3_u, sizeof(fh->fh3_u));
			bcopy(fidp->fid_data, &newfh, fidp->fid_len);
			fh->fh3_fsid = newfh.fh_fsid;
			fh->fh3_len = newfh.fh_len;
			bcopy(newfh.fh_data, fh->fh3_data, NFS_FHMAXDATA);
			fh->fh3_xlen = exi->exi_fid->fid_len;
			bcopy(exi->exi_fid->fid_data, fh->fh3_xdata, exi->exi_fid->fid_len);
		}
	} else if (fidp->fid_len > sizeof (fh->fh3_data) ||
	    exi->exi_fh.fh_xlen > sizeof (fh->fh3_xdata)) {
		freefid(fidp);
		return (EREMOTE);
	} else {
		fh->fh3_u.nfs_fh3_i.fh3_i = exi->exi_fh;

		fh->fh3_len = fidp->fid_len;
		p = (char *)&fidp->fid_data;
		q = (char *)&fh->fh3_data;
		for (i = 0; i < (int)fh->fh3_len; i++)
			*q++ = *p++;
		while (q < &fh->fh3_data[NFS_FHMAXDATA])
			*q++ = '\0';
		fh->fh3_fsid = vp->v_vfsp->vfs_fsid;
		fh->fh3_xlen = exi->exi_fid->fid_len;

		bcopy(exi->exi_fid->fid_data, fh->fh3_xdata, fh->fh3_xlen);
	}

	freefid(fidp);
	return (0);
}

/*
 * Find the export structure associated with the given filesystem
 * If found, returns with exported_lock held.
 */
struct exportinfo *
findexport(fsid_t *fsid, struct fid *fid)
{
	struct exportinfo *exi;

	EXPORTED_MRRLOCK();

#define FASTCMP12(a,b) (((int *)a)[0]==((int *)b)[0] && \
	                ((int *)a)[1]==((int *)b)[1] &&  \
                        ((int *)a)[2]==((int *)b)[2])

	if (fid->fid_len == NFS_FHMAXDATA) { /* "usual" length */
		for (exi = EXIHASH(fsid); exi; exi = exi->exi_next) {
			if (eqfsid(&(exi)->exi_fsid, fsid) &&
			    FASTCMP12(fid,(exi->exi_fid)))
			    return (exi);
		    }
	}

#pragma mips_frequency_hint NEVER
	for (exi = EXIHASH(fsid); exi; exi = exi->exi_next) {
		if (exportmatch(exi, fsid, fid))
			return (exi);
	}
	EXPORTED_MRUNLOCK();
	return (NULL);
}


/*
 * Invalidate any matching cached pointers to a vfs.
 */
void
nfs_purge_vfsp(vfsp)
	struct	vfs	*vfsp;
{
	struct	exportinfo	*exi;
	int	i;
	int	found;

	/*
	 * Don't bother acquiring the export list lock
	 * if this vfs is not exported.
	 */
	found = 0;
	for (i = 0; ((i < EXIHASHTABSIZE) && !found); i++)
		for (exi = exihashtab[i]; exi; exi = exi->exi_next)
			if (exi->exi_vfsp == vfsp) {
				found = 1;
				break;
			}
				
	if (exi == NULL)
		return;

	/*
	 * Acquiring the exportinfo list lock as a WRITER
	 * ensures that there are no nfs requests in progress
	 * when we do this.
	 */
	EXPORTED_MRWLOCK();
	for (i = 0; i < EXIHASHTABSIZE; i++)
		for (exi = exihashtab[i]; exi; exi = exi->exi_next)
			if (exi->exi_vfsp == vfsp)
				exi->exi_vfsp = NULL;
	EXPORTED_MRUNLOCK();
}

/*
 * Load from user space, a list of internet addresses into kernel space
 */
int
loadaddrs(addrs, maxaddrs, ht)
	struct exaddrlist *addrs;
	int maxaddrs;
	struct exaddrhashtable *ht;
{
	int naddrs, shift;
	unsigned allocsize, length, tablesize;
	struct sockaddr *sa, **end, **hash;

	naddrs = addrs->naddrs;
	if (naddrs > maxaddrs) {
		return (EINVAL);
	}
	if (naddrs == 0) {
		addrs->addrvec = NULL;
		ht->table = NULL;
		return (0);
	}

	allocsize = naddrs * sizeof(struct sockaddr);
	sa = mem_alloc(allocsize);
	if (copyin(addrs->addrvec, sa, allocsize)) {
		mem_free(sa, allocsize);
		return (EFAULT);
	}
	addrs->addrvec = sa;

	/*
	 * Allocate the hash table.  Reserve a sentinel to terminate linear
	 * probes, add naddrs/8 to ensure a good alpha, and round up to the
	 * nearest power of 2.
	 */
	length = naddrs + 1 + (naddrs >> 3);
	if (length & (length-1)) {
		shift = 1;
		if (length & 0xff00)
			shift += 8, length &= 0xff00;
		if (length & 0xf0f0)
			shift += 4, length &= 0xf0f0;
		if (length & 0xcccc)
			shift += 2, length &= 0xcccc;
		if (length & 0xaaaa)
			shift += 1;
		length = 1 << shift;
	}
	tablesize = length * sizeof(struct sockaddr *);
	ht->tablesize = tablesize;
	ht->hashmask = length - 1;
	ht->table = mem_alloc(tablesize);
	bzero(ht->table, tablesize);
	ht->endtable = ht->table + length;

	/*
	 * Initialize hash table entries.  XXX should fail if not AF_INET
	 */
	for (end = ht->endtable; --naddrs >= 0; sa++) {
		if (sa->sa_family != AF_INET)
			continue;
		hash = exaddrhash(ht, sa);
		while (*hash != NULL) {
			if (++hash == end)
				hash = ht->table;
		}
		*hash = sa;
	}
	return (0);
}


/*
 * Free an entire export list node
 */
void
exportfree(exi)
	struct exportinfo *exi;
{
	struct export *ex;

	ex = &exi->exi_export;
	switch (ex->ex_auth) {
	case AUTH_UNIX:
		if (ex->ex_unix.rootaddrs.naddrs) {
			mem_free(ex->ex_unix.rootaddrs.addrvec,
				 (ex->ex_unix.rootaddrs.naddrs *
				  sizeof(struct sockaddr)));
			mem_free(exi->exi_roothash.table,
				 exi->exi_roothash.tablesize);
		}
		if ((ex->ex_flags & EX_ACCESS) && ex->ex_accessaddrs.naddrs) {
			mem_free(ex->ex_accessaddrs.addrvec,
				ex->ex_accessaddrs.naddrs *
				 sizeof(struct sockaddr));
			mem_free(exi->exi_accesshash.table,
				exi->exi_accesshash.tablesize);
		}
		break;
	}
	if ((ex->ex_flags & EX_RDMOSTLY) && ex->ex_writeaddrs.naddrs) {
		mem_free(ex->ex_writeaddrs.addrvec,
			 ex->ex_writeaddrs.naddrs * sizeof(struct sockaddr));
		mem_free(exi->exi_writehash.table,
			 exi->exi_writehash.tablesize);
	}
	freefid(exi->exi_fid);
	mem_free(exi, sizeof(struct exportinfo));
}

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int
irix5_to_export(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_export, export);

	target->ex_flags = source->ex_flags;
	target->ex_anon = source->ex_anon;
	target->ex_auth = source->ex_auth;
	target->ex_unix.rootaddrs.naddrs = source->ex_unix.rootaddrs.naddrs;
	target->ex_unix.rootaddrs.addrvec =
		(struct sockaddr *)(__psint_t)source->ex_unix.rootaddrs.addrvec;
	target->ex_writeaddrs.naddrs = source->ex_writeaddrs.naddrs;
	target->ex_writeaddrs.addrvec =
		(struct sockaddr *)(__psint_t)source->ex_writeaddrs.addrvec;
	target->ex_accessaddrs.naddrs = source->ex_accessaddrs.naddrs;
	target->ex_accessaddrs.addrvec =
		(struct sockaddr *)(__psint_t)source->ex_accessaddrs.addrvec;
	return 0;
}
#endif
