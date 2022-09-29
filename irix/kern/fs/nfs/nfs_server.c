/* @(#)nfs_server.c	1.7 88/08/06 NFSSRC4.0 from 2.85 88/02/08 SMI
 *
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */
#ident      "$Revision: 1.230 $"

#define NFSSERVER
#include "types.h"
#include <string.h>
#include <limits.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/mbuf.h>
#include <sys/mkdev.h>
#include <sys/pathname.h>
#include <sys/pda.h>
#include <sys/resource.h>
#include <sys/sema.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/uthread.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/tcpipstats.h>
#include <sys/uuid.h>
#include <sys/capability.h>
#include <sys/sysmacros.h>
#include <sys/imon.h>
#include <sys/unistd.h>
#include <net/if.h>
#include <sys/atomic_ops.h>
#include <sys/mac.h>
#include <sys/sesmgr.h>
#include "auth.h"
#include "auth_unix.h"
#include "svc.h"
#include "xdr.h"
#include "nfs.h"
#include "export.h"
#include "nfs_stat.h"
#include "nfs_impl.h"

static void	mbuf_to_iov(struct mbuf *, struct iovec *);

#define eqfid(fid1, fid2) \
	((fid1)->fid_len == (fid2)->fid_len && \
	bcmp((char *)(fid1)->fid_data, (char *)(fid2)->fid_data, \
	(int)(fid1)->fid_len) == 0)

extern int	checkauth(struct exportinfo *, struct svc_req *, 
			  struct cred *, int anon_ok);

extern void	vattr_to_fattr3(struct vattr *, struct fattr3 *);
static int	sattr3_to_vattr(struct sattr3 *, struct vattr *);

extern mac_t mac_high_low_lp;
static zone_t *rfs_pn_zone;	/* for readlink pathname buffers 1024 */
extern zone_t *nfs_xdr_string_zone;  /* for other path components (255) */

/*
 * rpc service program version range supported
 */
#define	VERSIONMIN	2
#define	VERSIONMAX	3


/*
 * vattr mask for nfs file attributes
 */
#define	AT_NFS	(AT_TYPE|AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|\
		AT_NLINK|AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|\
		AT_RDEV|AT_BLKSIZE|AT_NBLOCKS)

/*
 * Returns true iff exported filesystem is read-only
 */
#define rdonly(exi, req) (((exi)->exi_export.ex_flags & EX_RDONLY) || \
			  (((exi)->exi_export.ex_flags & EX_RDMOSTLY) && \
			   !hostintable((struct sockaddr *) \
					svc_getcaller((req)->rq_xprt), \
					&(exi)->exi_writehash)))

/*
 * Returns true iff fname is a sane filename for creation operations
 */
#define sane(fname) ((fname) && *(fname) != '\0' && \
		     strcmp((fname),".") != 0 && strcmp((fname),"..") != 0 && \
		     !strchr((fname),'/')) 

#define SMALL_IOV_CNT	8	/* 6 Ethernet frags plus fudge */
#define MEDIUM_IOV_CNT	24	/* 22 Ethernet frags plus fudge */
#define LARGE_IOV_CNT	40

static struct zone *large_iovec_zone;

struct vnode	*fhtovp(fhandle_t *, struct exportinfo *);

extern mutex_t nfsd_count_lock;

uint32 rfs3_udp_trsize = (48 * 1024);
uint32 rfs3_tcp_trsize = (4 * 1024 * 1024);

uint32 rfs3_tsize(SVCXPRT *xprt)
{
	if (xprt->xp_type == XPRT_TCP)
		return rfs3_tcp_trsize;
	else
		return rfs3_udp_trsize;
}

/*
 * These are the interface routines for the server side of the
 * Networked File System.  See the NFS protocol specification
 * for a description of this interface.
 */


/*
 * Get file attributes.
 * Returns the current attributes of the file with the given fhandle.
 */
void
rfs_getattr(fhp, ns, exi)
	fhandle_t *fhp;
	register struct nfsattrstat *ns;
	struct exportinfo *exi;
{
	int error;
	register struct vnode *vp;
	struct vattr va;

	vp = fhtovp(fhp, exi);
	if (vp == NULL) {
#pragma mips_frequency_hint NEVER
		ns->ns_status = NFSERR_STALE;
		return;
	}
	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, get_current_cred(), error);
	if (!error) {
		error = vattr_to_nattr(&va, &ns->ns_attr);
	}
	ns->ns_status = puterrno(error);
	VN_RELE(vp);
}

/*
 * Set file attributes.
 * Sets the attributes of the file with the given fhandle.  Returns
 * the new attributes.
 */
void
rfs_setattr(args, ns, exi, req)
	struct nfssaargs *args;
	register struct nfsattrstat *ns;
	struct exportinfo *exi;
	struct svc_req *req;
{
	int error;
	register struct vnode *vp;
	struct vattr va;
	timespec_t tv;

	vp = fhtovp(&args->saa_fh, exi);
	if (vp == NULL) {
		ns->ns_status = NFSERR_STALE;
		return;
	}
	if (rdonly(exi, req)) {
		error = EROFS;
	} else {
		sattr_to_vattr(&args->saa_sa, &va);
		/*
		 * Allow SysV-compatible option to set access and
		 * modified times if root, owner, or write access.
		 *
		 * XXX - Until an NFS Protocol Revision, this may be
		 *       simulated by setting the client time in the
		 *       tv_sec field of the access and modified times
		 *       and setting the tv_usec field of the modified
		 *       time to an invalid value (1,000,000).  This
		 *       may be detected by servers modified to do the
		 *       right thing, but will not be disastrous on
		 *       unmodified servers.
		 *
		 * XXX - For now, va_mtime.tv_usec == -1 flags this in
		 *       VOP_SETATTR().
		 * XXXbe does this really work?
		 */
		if ((va.va_mtime.tv_sec != -1) &&
		    (va.va_mtime.tv_nsec == 1000000000)) {
			nanotime_syscall(&tv);
			va.va_mtime.tv_sec = va.va_atime.tv_sec = tv.tv_sec;
			va.va_mtime.tv_nsec = va.va_atime.tv_nsec = tv.tv_nsec;
		}
		VOP_SETATTR(vp, &va, ATTR_NONBLOCK, get_current_cred(), error);
		if (!error) {
			va.va_mask = AT_NFS;
			VOP_GETATTR(vp, &va, 0, get_current_cred(), error);
			if (!error) {
				error = vattr_to_nattr(&va, &ns->ns_attr);
			}
		}
	}
	ns->ns_status = puterrno(error);
	VN_RELE(vp);
}

/*
 * Directory lookup.
 * Returns an fhandle and file attributes for file name in a directory.
 * exi is read-locked on entry and remains locked on exit
 */
void
rfs_lookup(da, dr, exi)
	struct nfsdiropargs *da;
	register struct  nfsdiropres *dr;
	struct exportinfo *exi;
{
	int error;
	register struct vnode *dvp;
	struct vnode *vp;
	vnode_t *cvp = NULLVP; 		/* current component {d}vp */
	struct vattr va;
	char *da_name = da->da_name;

	dvp = fhtovp(&da->da_fhandle, exi);
	if (dvp == NULL) {
#pragma mips_frequency_hint NEVER
		dr->dr_status = NFSERR_STALE;
		return;
	}

	/*
	 *  We need to prevent a lookup of ".." from going above
	 *  the export point (for non-nohide fs's).  Since the 
	 *  exportinfo fid is the same as the fid of the export 
	 *  point, we know we are at the export point if the target 
	 *  fid in the filehandle matches the exportinfo fid.
	 */
	if (exi->exi_fid->fid_len == da->da_fhandle.fh_len
	    && !(exi->exi_export.ex_flags & EX_NOHIDE)
	    && bcmp(exi->exi_fid->fid_data, da->da_fhandle.fh_data,
		    da->da_fhandle.fh_len) == 0
	    && ISDOTDOT(da_name))
	{
#pragma mips_frequency_hint NEVER
		da_name = ".";			 /* change ".." to "." */
	}

	/*
	 * do lookup.
	 */
lookuploop:
	if (dvp->v_type != VDIR) {
#pragma mips_frequency_hint NEVER
		error = ENOTDIR;
	} else {
		VOP_LOOKUP(dvp, da_name, &vp,
			   (struct pathname *)NULL, 0, curuthread->ut_rdir,
			   get_current_cred(), error);
	}
	if (error) {
#pragma mips_frequency_hint NEVER
		vp = NULL;
	} else {
		if ((dvp->v_flag & VROOT) 
		    && dvp->v_vfsp->vfs_vnodecovered
		    && ISDOTDOT(da_name)
	    	    && (exi->exi_export.ex_flags & EX_NOHIDE))
		{
#pragma mips_frequency_hint NEVER
		    	if (!(findexivp(&exi, NULL, 
					dvp->v_vfsp->vfs_vnodecovered))) {
				VN_RELE(vp);
				cvp = dvp;
				dvp = dvp->v_vfsp->vfs_vnodecovered;
				VN_HOLD(dvp);
				VN_RELE(cvp);
				EXPORTED_MRUNLOCK();
				goto lookuploop;
			}
		}
		if (vp->v_vfsmountedhere) {
			VN_HOLD(vp);
			cvp = vp;
			if (error = traverse(&vp)) {
				VN_RELE(vp);
				error = 0;
			} else {
				int rv;

				ASSERT(vp != cvp);
				ASSERT(vp->v_vfsp != dvp->v_vfsp);

				if ((rv = findexivp(&exi, NULL, vp)) != 0
				    || !(exi->exi_export.ex_flags & EX_NOHIDE)) 
				{
					/* not nohide; back-up */
					VN_RELE(vp);
					vp = cvp;
					if (rv == 0)
						EXPORTED_MRUNLOCK();
					error = findexivp(&exi, NULL, vp);
					if (error == 0)
						EXPORTED_MRUNLOCK();
				} else {
					VN_RELE(cvp);
					if (rv == 0)
						EXPORTED_MRUNLOCK();
				}
			}
		}
		if (!error && exi) {
			va.va_mask = AT_NFS;
			VOP_GETATTR(vp, &va, 0, get_current_cred(), error);
			if (!error) {
				error = vattr_to_nattr(&va, &dr->dr_attr);
				if (!error) {
					error = makefh(&dr->dr_fhandle, vp,
						       exi);
				}
			}
		}
	}
	dr->dr_status = puterrno(error);
	if (vp) {
		VN_RELE(vp);
	}
	VN_RELE(dvp);
}

/*
 * Read symbolic link.
 * Returns the string in the symbolic link at the given fhandle.
 */
void
rfs_readlink(fhp, rl, exi)
	fhandle_t *fhp;
	register struct nfsrdlnres *rl;
	struct exportinfo *exi;
{
	struct vnode *vp;
	struct uio uio;
	struct iovec iov;
	int error;

	vp = fhtovp(fhp, exi);
	if (vp == NULL) {
		rl->rl_status = NFSERR_STALE;
		return;
	}
	if (vp->v_type != VLNK) {
		rl->rl_status = NFSERR_NXIO;
		VN_RELE(vp);
		return;
	}

	/*
	 * Allocate data for pathname.  This will be freed by rfs_rlfree.
	 */
	rl->rl_data = kmem_zone_alloc(rfs_pn_zone, KM_SLEEP);

	/*
	 * Set up io vector to read sym link data
	 */
	uio.uio_iovcnt = 1;
	uio.uio_iov = &iov;
	iov.iov_base = rl->rl_data;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_fmode = 0;
	uio.uio_offset = 0;
	uio.uio_resid = iov.iov_len = NFS_MAXPATHLEN;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;

	/*
	 * read link
	 */
	VOP_READLINK(vp, &uio, get_current_cred(), error);

	/*
	 * Clean up
	 */
	if (error) {
		kmem_zone_free(rfs_pn_zone, rl->rl_data);
		rl->rl_count = 0;
		rl->rl_data = NULL;
	} else {
		rl->rl_count = NFS_MAXPATHLEN - uio.uio_resid;
	}
	rl->rl_status = puterrno(error);
	VN_RELE(vp);
}

/*
 * Free data allocated by rfs_readlink
 */
void
rfs_rlfree(rl)
	struct nfsrdlnres *rl;
{
	if (rl->rl_data) {
		kmem_zone_free(rfs_pn_zone, rl->rl_data);
	}
}

/*
 * Read data.
 * Returns some data read from the file at the given fhandle.
 */
void
rfs_read(ra, rr, exi)
	struct nfsreadargs *ra;
	register struct nfsrdresult *rr;
	struct exportinfo *exi;
{
	register int error;
	struct vnode *vp;
	struct vattr va;
#if _PAGESZ >= 16*1024
	int pboff;
	int pbsize;
	struct buf *bp;
	extern void	chunkrelse(buf_t*);
#endif
	struct uio uio;
	struct iovec iov;
	cred_t *cr = get_current_cred();

	vp = fhtovp(&ra->ra_fhandle, exi);
	if (vp == NULL) {
#pragma mips_frequency_hint NEVER
		rr->rr_status = NFSERR_STALE;
		return;
	}
	if (vp->v_type != VREG) {
#pragma mips_frequency_hint NEVER
#ifdef DEBUG
		printf("rfs_read: attempt to read from non-file\n");
#endif
		error = EISDIR;
	} else {
		va.va_mask = AT_NFS;
		VOP_GETATTR(vp, &va, 0, cr, error);
	}
	if (error) {
		goto out;
	}

	/*
	 * This is a kludge to allow reading of files created
	 * with no read permission.  The owner of the file
	 * is always allowed to read it.
	 */
	if (cr->cr_uid != va.va_uid) {
		VOP_ACCESS(vp, VREAD, cr, error);
		if (error) {
#pragma mips_frequency_hint NEVER
			/*
			 * Exec is the same as read over the net because
			 * of demand loading.
			 */
			VOP_ACCESS(vp, VEXEC, cr, error);
		}
		if (error) {
			goto out;
		}
	}

	if (ra->ra_offset >= va.va_size) {
		rr->rr_count = 0;
		(void) vattr_to_nattr(&va, &rr->rr_attr);
		goto out;			/* after EOF */
	}

#if _MIPS_SIM == _ABI64
	/*
	 * Try the fast-path and see if it works.  If not,
	 * we fall through to the slow path.  Signal that we're
	 * NFSv2 and that we need a one-page buffer coming back.
	 *
	 * We can depend on getting a direct mapping to any page
	 * only in the 64-bit addressing model.
	 */
	bp = NULL;

	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_READBUF(vp, ra->ra_offset, ra->ra_count, IO_NFS|IO_ONEPAGE|IO_ISLOCKED, cr,
			&curuthread->ut_flid, &bp, &pboff, &pbsize, error);

	if (error == 0 && bp != NULL)  {
		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			       NETEVENT_NFSUP, pbsize, 
			       NETRES_READ);
		VOP_GETATTR(vp, &va, 0, cr, error);
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		if (!error)
			error = vattr_to_nattr(&va, &rr->rr_attr);
		rr->rr_count = pbsize;
		rr->rr_bsize = pboff;
		rr->rr_bp = bp;
		rr->rr_status = puterrno(error);
		/* more work in xdr_rrok */
		return;
	}
	VOP_RWUNLOCK(vp, VRWLOCK_READ);
#endif /* big pointers */

	rr->rr_bp = NULL;

	/*
	 * Allocate space for data.  This will be freed by the nfs_xdr
	 * call-back mechanism when the reference count goes to zero.
	 */
	rr->rr_data = (char *)kvpalloc(btoc(ra->ra_count), VM_BULKDATA, 0);

	/*
	 * Set up io vector
	 */
	uio.uio_iovcnt = 1;
	uio.uio_iov = &iov;
	iov.iov_base = rr->rr_data;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_offset = ra->ra_offset;
	uio.uio_resid = iov.iov_len = ra->ra_count;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	uio.uio_fmode = FNONBLOCK;

	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_READ(vp, &uio, IO_SYNC | IO_ISLOCKED, cr, &curuthread->ut_flid, 
		 error);
	if (error == 0)
		VOP_GETATTR(vp, &va, 0, cr, error);
	VOP_RWUNLOCK(vp, VRWLOCK_READ);
	if (error == 0)
		error = vattr_to_nattr(&va, &rr->rr_attr);
	if (error)
		goto out;
	rr->rr_count = ra->ra_count - uio.uio_resid;
	NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
	       NETEVENT_NFSUP, rr->rr_count, NETRES_READ);
	/*
	 * Remember the buffer's size so we can free it 
	 * (might be larger than real result size!)
	 */
	rr->rr_bsize = ra->ra_count;
out:
	if (error && rr->rr_data != NULL) {
#pragma mips_frequency_hint NEVER
		kvpfree(rr->rr_data, btoc(ra->ra_count));
		rr->rr_data = NULL;
		rr->rr_count = 0;
	}
	rr->rr_status = puterrno(error);
	VN_RELE(vp);
}

static void
mbuf_to_iov(m, iov)
	struct mbuf *m;
	struct iovec *iov;
{
	while (m) {
		iov->iov_base = mtod(m, caddr_t);
		iov->iov_len = m->m_len;
		iov++;
		m = m->m_next;
	}
}

extern rlim_t  rlimit_fsize_max;

/*
 * Write data to file.
 * Returns attributes of a file after writing some data to it.
 */
void
rfs_write(wa, ns, exi, req)
	struct nfswriteargs *wa;
	struct nfsattrstat *ns;
	struct exportinfo *exi;
	struct svc_req *req;
{
	register int error;
	register struct vnode *vp;
	int oddblock;
	struct vattr va;
	struct uio uio;
	cred_t *cr = get_current_cred();

	vp = fhtovp(&wa->wa_fhandle, exi);
	if (vp == NULL) {
#pragma mips_frequency_hint NEVER
		ns->ns_status = NFSERR_STALE;
		return;
	}
	if (rdonly(exi, req)) {
#pragma mips_frequency_hint NEVER
		error = EROFS;
	} else if (vp->v_type != VREG) {
#pragma mips_frequency_hint NEVER
#ifdef DEBUG
		printf("rfs_write: attempt to write to non-file\n");
#endif
		error = EISDIR;
	} else {
		va.va_mask = AT_UID;
		VOP_GETATTR(vp, &va, 0, cr, error);
	}
	if (!error) {
		if (cr->cr_uid != va.va_uid) {
			/*
			 * This is a kludge to allow writes of files created
			 * with read only permission.  The owner of the file
			 * is always allowed to write it.
			 */
			VOP_ACCESS(vp, VWRITE, cr, error);
		}
		if (!error) { 
			struct mbuf *m;
			struct iovec small_iov[SMALL_IOV_CNT];
			struct iovec *iovp;
			int iovcnt;

			/*
			 * We should assume that only "mbufs"
			 * are used as the XDR layer. So we never
			 * get get regular data decoded.
			 */
			ASSERT(wa->wa_data == NULL);

			iovcnt = 0;
			for (m = wa->wa_mbuf; m; m = m->m_next) {
				iovcnt++;
			}
			if (iovcnt <= SMALL_IOV_CNT) {
				iovp = small_iov;
			} else if (iovcnt <= LARGE_IOV_CNT) {
				iovp = (struct iovec *)
				    kmem_zone_alloc(large_iovec_zone,
						    KM_SLEEP);
			} else {
#pragma mips_frequency_hint NEVER
				iovp = (struct iovec *)
				    kmem_alloc((u_int)(sizeof 
						       (struct iovec) * iovcnt),
					       KM_SLEEP);
			}
			mbuf_to_iov(wa->wa_mbuf, iovp);
			uio.uio_iov = iovp;
			uio.uio_iovcnt = iovcnt;
			uio.uio_segflg = UIO_SYSSPACE;
			uio.uio_offset = wa->wa_offset;
			uio.uio_resid = wa->wa_count;
			uio.uio_limit = rlimit_fsize_max;
			uio.uio_fmode = FNONBLOCK;
			uio.uio_pmp = NULL;
			uio.uio_sigpipe = 0;
			uio.uio_pio = 0;
			uio.uio_readiolog = 0;
			/*
			 * Try to gather odd and even blocks together.
			 * 13 means 8K, and 14 means 16K.
			 */
			oddblock = (wa->wa_offset & NFS_MAXDATA);
			uio.uio_writeiolog = 14;
			uio.uio_pbuf = 0;
			VOP_RWLOCK(vp, VRWLOCK_WRITE);
			/*
			 * Make sure we write everything or else return an error.
			 * To do this, we must verify that uio_resid is 0.  If it
			 * is non-zero, we must do the write again.  The uio
			 * structure will not need to be modified to do this.  If
			 * no space is left on the device, an error will be returned.
			 */
			do {
				VOP_WRITE(vp, &uio, 
					  IO_NFS | IO_ISLOCKED | IO_UIOSZ, cr, 
					  &curuthread->ut_flid, error);
			} while (!error && uio.uio_resid);
			ASSERT(uio.uio_sigpipe == 0);
			VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
			if (iovcnt > SMALL_IOV_CNT) {
				if (iovcnt <= LARGE_IOV_CNT) {
					kmem_zone_free(large_iovec_zone,
						       iovp);
				} else {
					kmem_free((caddr_t)iovp,
						  (u_int)(sizeof (struct iovec) 
							  * iovcnt));
				}
			}
			if (!error && (exi->exi_export.ex_flags & EX_WSYNC)) {
				timespec_t ts;
				/*
				 * Dally for a while to allow other
				 * writes to come in before committing.
				 * Dally less on odd numbered blocks to
				 * help gathering into 16K and none at all
				 * at the end of a sequence (count < 8192).
				 */
				ts.tv_sec = 0;
				if (oddblock) {
					ts.tv_nsec = 500000;
				} else {
					ts.tv_nsec = 3000000;
				}
				if (wa->wa_count >= NFS_MAXDATA) {
					nano_delay(&ts);
				}
				if (VN_DIRTY(vp)) {
					VOP_FSYNC(vp, FSYNC_WAIT | FSYNC_DATA,
						  cr, (off_t)0, (off_t)-1,
						  error);
				}
			}
			NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			       NETEVENT_NFSUP, fullcount - wa->wa_count,
			       NETRES_WRITE);
		}
	}
	if (!error) {
		/*
		 * Get attributes again so we send the latest mod
		 * time to the client side for his cache.
		 */
		va.va_mask = AT_NFS;
		VOP_GETATTR(vp, &va, 0, cr, error);
	}
	if (!error) {
		error = vattr_to_nattr(&va, &ns->ns_attr);
	}
	ns->ns_status = puterrno(error);
	VN_RELE(vp);
}

/*
 * Create a file.
 * Creates a file with given attributes and returns those attributes
 * and an fhandle for the new file.
 */
void
rfs_create(args, dr, exi, req)
	struct nfscreatargs *args;
	struct nfsdiropres *dr;
	struct exportinfo *exi;
	struct svc_req *req;
{
	register int error;
	struct vattr va;
	struct vnode *vp = NULL;	/* imon checks initial value */
	register struct vnode *dvp;

        if (!sane(args->ca_da.da_name)) {
                dr->dr_status = NFSERR_ACCES;
                goto freepath;
        }
	sattr_to_vattr(&args->ca_sa, &va);
	va.va_mask |= AT_TYPE;
	/*
	 * This is a completely gross hack to make mknod
	 * work over the wire until we can wack the protocol
	 */
#define	IFMT		0170000		/* type of file */
#define	IFCHR		0020000		/* character special */
#define	IFBLK		0060000		/* block special */
#define	IFSOCK		0140000		/* socket */
	if ((va.va_mode & IFMT) == IFCHR) {
		va.va_type = VCHR;
		if (va.va_size == NFS_FIFO_DEV)
			va.va_type = VFIFO;	/* xtra kludge for namedpipe */
		else
			va.va_rdev = nfs_expdev(va.va_size);
		va.va_mask |= AT_RDEV;
		va.va_size = 0;
		va.va_mode &= ~IFMT;
	} else if ((va.va_mode & IFMT) == IFBLK) {
		va.va_type = VBLK;
		va.va_rdev = nfs_expdev(va.va_size);
		va.va_mask |= AT_RDEV;
		va.va_size = 0;
		va.va_mode &= ~IFMT;
	} else if ((va.va_mode & IFMT) == IFSOCK) {
		va.va_type = VSOCK;
	} else {
		va.va_type = VREG;
	}

	dvp = fhtovp(&args->ca_da.da_fhandle, exi);
	if (dvp == NULL) {
		dr->dr_status = NFSERR_STALE;
		goto freepath;
	}
	if (rdonly(exi, req)) {
		error = EROFS;
	} else {
		VOP_CREATE(dvp, args->ca_da.da_name, &va, 0, 
				   VWRITE, &vp, get_current_cred(), error);
	}
	if (!error) {
		error = _SESMGR_NFS_VSETLABEL(vp, mtod(req->rq_xprt->xp_ipattr, struct ipsec *));
		va.va_mask = AT_NFS;
		if (!error)
			VOP_GETATTR(vp, &va, 0, get_current_cred(), error);
		if (!error) {
			error = vattr_to_nattr(&va, &dr->dr_attr);
			if (!error) {
				error = makefh(&dr->dr_fhandle, vp, exi);
			}
		}
		VN_RELE(vp);
	}
	dr->dr_status = puterrno(error);
	VN_RELE(dvp);
    freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->ca_da.da_name);
}

/*
 * Remove a file.
 * Remove named file from parent directory.
 */
void
rfs_remove(da, status, exi, req)
	struct nfsdiropargs *da;
	enum nfsstat *status;
	struct exportinfo *exi;
	struct svc_req *req;
{
	int error;
	register struct vnode *vp;

	vp = fhtovp(&da->da_fhandle, exi);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		return;
	}
	if (rdonly(exi, req)) {
		error = EROFS;
	} else {
		VOP_REMOVE(vp, da->da_name, get_current_cred(), error);
	}
	*status = puterrno(error);
	VN_RELE(vp);
}

/*
 * rename a file
 * Give a file (from) a new name (to).
 */
void
rfs_rename(args, status, exi, req)
	struct nfsrnmargs *args;
	enum nfsstat *status;
	struct exportinfo *exi;
	struct svc_req *req;
{
	int error;
	register struct vnode *fromvp;
	register struct vnode *tovp;
	cred_t *cr = get_current_cred();
	struct exportinfo *texi;

	if (!sane(args->rna_to.da_name)) {
		*status = NFSERR_ACCES;
		goto freepaths;
	}

	fromvp = fhtovp(&args->rna_from.da_fhandle, exi);
	if (fromvp == NULL) {
		*status = NFSERR_STALE;
		goto freepaths;
	}
	if (rdonly(exi, req)) {
		error = EROFS;
		goto fromerr;
	}

	texi = findexport(&args->rna_to.da_fhandle.fh_fsid,
			(struct fid *) &args->rna_to.da_fhandle.fh_xlen);
	tovp = fhtovp(&args->rna_to.da_fhandle, texi);
	if (texi != NULL) {
		EXPORTED_MRUNLOCK();
	}

	if (tovp == NULL) {
		error = ESTALE;
		goto fromerr;
	}
	if (exi != texi) {
		/* 
		 * we must be in nohideland, or the client would
		 * never have made the request.
		 */
		VN_RELE(tovp);
		error = ESTALE;
		goto fromerr;
	}

	VOP_RENAME(fromvp, args->rna_from.da_name,
			   tovp, args->rna_to.da_name, (struct pathname *) 0,
			   cr, error);
	/*
	 *  Must explicitly post imon events because imon-vnodeop
	 *  layer will never see it if tdvp is monitored but fdvp
	 *  isn't.
	 */
	if (error == 0) {
		struct vnode *filevp;
		int lookuperr;

		IMON_EVENT(tovp, cr, IMON_CONTENT);
		VOP_LOOKUP(tovp, args->rna_to.da_name, &filevp,	
			(struct pathname *)NULL, 0, curuthread->ut_rdir,
			cr, lookuperr);
		if (!lookuperr) {
			IMON_EVENT(filevp, cr, IMON_RENAME);
			VN_RELE(filevp);
		}
	}
	VN_RELE(tovp);
fromerr:
	VN_RELE(fromvp);
	*status = puterrno(error);
freepaths:
	kmem_zone_free(nfs_xdr_string_zone, args->rna_from.da_name);
	kmem_zone_free(nfs_xdr_string_zone, args->rna_to.da_name);
}

/*
 * Link to a file.
 * Create a file (to) which is a hard link to the given file (from).
 */
void
rfs_link(args, status, exi, req)
	struct nfslinkargs *args;
	enum nfsstat *status;
	struct exportinfo *exi;
	struct svc_req *req;
{
	int error;
	register struct vnode *fromvp;
	register struct vnode *tovp;

	if (!sane(args->la_to.da_name)) {
		*status = NFSERR_ACCES;
		goto freepath;
	}
	fromvp = fhtovp(&args->la_from, exi);
	if (fromvp == NULL) {
		*status = NFSERR_STALE;
		goto freepath;
	}
	tovp = fhtovp(&args->la_to.da_fhandle, exi);
	if (tovp == NULL) {
		*status = NFSERR_STALE;
		VN_RELE(fromvp);
		goto freepath;
	}
	if (rdonly(exi, req)) {
		error = EROFS;
	} else {
		VOP_LINK(tovp, fromvp, args->la_to.da_name, get_current_cred(), error);
	}
	*status = puterrno(error);
	VN_RELE(fromvp);
	VN_RELE(tovp);
freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->la_to.da_name);
}

/*
 * Symbolicly link to a file.
 * Create a file (to) with the given attributes which is a symbolic link
 * to the given path name (to).
 */
void
rfs_symlink(args, status, exi, req)
	struct nfsslargs *args;
	enum nfsstat *status;
	struct exportinfo *exi;
	struct svc_req *req;
{
	int error;
	struct vattr va;
	register struct vnode *vp;
	register int nodesize;

        if (!sane(args->sla_from.da_name)) {
                *status = NFSERR_ACCES;
                goto freepaths;
        }
	sattr_to_vattr(&args->sla_sa, &va);
	va.va_type = VLNK;
	vp = fhtovp(&args->sla_from.da_fhandle, exi);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		goto freepaths;
	}
	if (vp->v_type != VDIR) {
		*status = NFSERR_NOTDIR;
		VN_RELE(vp);
		goto freepaths;
	}
	if (rdonly(exi, req)) {
		error = EROFS;
	} else {
		VOP_SYMLINK(vp, args->sla_from.da_name,
		    &va, args->sla_tnm, get_current_cred(), error);
		/*
		 * XXX:casey
		 * We ought to be setting the MAC label to an
		 * unusable value here, but we don't have the vnode.
		 */
	}
	*status = puterrno(error);
	VN_RELE(vp);
freepaths:
	kmem_zone_free(nfs_xdr_string_zone, args->sla_from.da_name);
	nodesize = strlen(args->sla_tnm) + 1;
	if (nodesize <= NFS_MAXNAMLEN) {
		kmem_zone_free(nfs_xdr_string_zone, args->sla_tnm);
	} else {
		kmem_free(args->sla_tnm, nodesize);
	}
}

/*
 * Make a directory.
 * Create a directory with the given name, parent directory, and attributes.
 * Returns a file handle and attributes for the new directory.
 */
void
rfs_mkdir(args, dr, exi, req)
	struct nfscreatargs *args;
	struct  nfsdiropres *dr;
	struct exportinfo *exi;
	struct svc_req *req;
{
	int error;
	struct vattr va;
	struct vnode *dvp;
	register struct vnode *vp;
	cred_t *cr = get_current_cred();

        if (!sane(args->ca_da.da_name)) {
                dr->dr_status = NFSERR_ACCES;
                goto freepath;
        }
	sattr_to_vattr(&args->ca_sa, &va);
	va.va_mask |= AT_TYPE;
	va.va_type = VDIR;
	/*
	 * Should get exclusive flag and pass it on here
	 */
	vp = fhtovp(&args->ca_da.da_fhandle, exi);
	if (vp == NULL) {
		dr->dr_status = NFSERR_STALE;
		goto freepath;
	}
	if (rdonly(exi, req)) {
		error = EROFS;
	} else {
		VOP_MKDIR(vp, args->ca_da.da_name, &va, &dvp, cr, error);
		if (!error) {
			error = _SESMGR_NFS_VSETLABEL(dvp, mtod(req->rq_xprt->xp_ipattr, struct ipsec *));
			if (error)
				VN_RELE(dvp);
		}
		if (!error) {
			/*
			 * Attributes of the newly created directory should
			 * be returned to the client.
			 */
			va.va_mask = AT_NFS;
			VOP_GETATTR(dvp, &va, 0, cr, error);
			if (!error) {
				error = vattr_to_nattr(&va, &dr->dr_attr);
				if (!error) {
					error = makefh(&dr->dr_fhandle, dvp,
						       exi);
				}
			}
			VN_RELE(dvp);
		}
	}
	dr->dr_status = puterrno(error);
	VN_RELE(vp);
    freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->ca_da.da_name);
}

/*
 * Remove a directory.
 * Remove the given directory name from the given parent directory.
 */
void
rfs_rmdir(da, status, exi, req)
	struct nfsdiropargs *da;
	enum nfsstat *status;
	struct exportinfo *exi;
	struct svc_req *req;
{
	int error;
	register struct vnode *vp;

	vp = fhtovp(&da->da_fhandle, exi);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		return;
	}
	/*
	 * Filter out "." and "..".  There are some Linux clients that
	 * send this.  The local file system will be very unhappy if
	 * it gets one of these.
	 */
	if ((da->da_name[0] == '.') && ((da->da_name[1] == '\0') ||
		((da->da_name[1] == '.') && (da->da_name[2] == '\0')))) {
			VN_RELE(vp);
			*status = NFSERR_ACCES;
			return;
	}
	if (rdonly(exi, req)) {
		error = EROFS;
	} else {
		VOP_RMDIR(vp, da->da_name, curuthread->ut_cdir,
			get_current_cred(), error);
	}
	*status = puterrno(error);
	VN_RELE(vp);
}

void
rfs_readdir(rda, rd, exi)
	struct nfsrddirargs *rda;
	register struct nfsrddirres  *rd;
	struct exportinfo *exi;
{
	register struct vnode *vp;
	int error;
	off_t offset;
	struct uio uio;
	struct iovec iov;

	vp = fhtovp(&rda->rda_fh, exi);
	if (vp == NULL) {
		rd->rd_status = NFSERR_STALE;
		return;
	}
	if (vp->v_type != VDIR) {
#ifdef DEBUG
		printf("rfs_readdir: attempt to read non-directory\n");
#endif
		error = ENOTDIR;
		goto bad;
	}
	/*
	 * check read access of dir. We have to do this here because
	 * the opendir doesn't go over the wire.
	 */
	VOP_ACCESS(vp, VREAD, get_current_cred(), error);
	if (error) {
		goto bad;
	}

	if (rda->rda_count == 0) {
		rd->rd_size = 0;
		rd->rd_eof = FALSE;
		rd->rd_entries = NULL;
		goto bad;
	}

	rda->rda_count = MIN(rda->rda_count, NFS_MAXDATA);

	/*
	 * Allocate data for entries.  This will be freed by rfs_rddirfree.
	 */
	rd->rd_entries = kmem_alloc(rda->rda_count, KM_SLEEP);
	rd->rd_bufsize = rda->rda_count;

	/*
	 * Since offset is a magic cookie, we cannot presume to mask it.
	 * The exception to prove the rule is the 32bitclients export option,
	 * which allows us to work (pretty well) with Solaris 2.5 and Irix5.3
	 * and probably other 32 bit implementations.
	 */
	rd->rd_offset = offset = rda->rda_offset;

	/*
	 * Set up io vector to read directory data
	 */
	uio.uio_iovcnt = 1;
	uio.uio_iov = &iov;
	iov.iov_base = (caddr_t)rd->rd_entries;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_offset = offset;
	uio.uio_resid = iov.iov_len = rda->rda_count;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;

	/*
	 * read directory
	 */
	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_READDIR(vp, &uio, get_current_cred(), &rd->rd_eof, error);
	VOP_RWUNLOCK(vp, VRWLOCK_READ);

	/*
	 * Clean up
	 */
	if (error) {
		rd->rd_size = 0;
		goto bad;
	}

	/*
	 * set size and eof
	 */
	if (rda->rda_count && uio.uio_resid == rda->rda_count) {
		rd->rd_size = 0;
		rd->rd_eof = TRUE;
	} else {
		rd->rd_size = rda->rda_count - uio.uio_resid;
		rd->rd_eof = rd->rd_eof ? TRUE : FALSE;
	}

	/*
	 * if client request was in the middle of a block
	 * or block begins with null entries skip entries
	 * til we are on a valid entry >= client's requested
	 * offset.
	 */
	/*
	 * Our kernel memory allocator can't free partial allocations.
	 * Furthermore, we cannot interpret directory offsets with bitwise
	 * or arithmetic operators.  Therefore we don't align non-congruent
	 * readdir request, nor do we compress free entries.
	 */

	if (exi->exi_export.ex_flags & EX_B32CLNT) {
		rd->rd_offset = (cookie3)(uio.uio_offset & 0x00000000ffffffff);
	} else {
		rd->rd_offset = uio.uio_offset;
	}
bad:
	rd->rd_status = puterrno(error);
	VN_RELE(vp);
}

void
rfs_rddirfree(rd)
	struct nfsrddirres *rd;
{
	if (rd->rd_entries != NULL)
		kmem_free(rd->rd_entries, rd->rd_bufsize);
}

void
rfs_statfs(fh, fs, exi)
	fhandle_t *fh;
	register struct nfsstatfs *fs;
	struct exportinfo *exi;
{
	int error;
	struct statvfs sb;
	register struct vnode *vp;

	vp = fhtovp(fh, exi);
	if (vp == NULL) {
		fs->fs_status = NFSERR_STALE;
		return;
	}
	VFS_STATVFS(vp->v_vfsp, &sb, vp, error);
	fs->fs_status = puterrno(error);
	if (!error) {
		fs->fs_tsize = NFS_MAXDATA;
		fs->fs_bsize = sb.f_frsize;
		fs->fs_blocks = sb.f_blocks;
		fs->fs_bfree = sb.f_bfree;
		fs->fs_bavail = sb.f_bavail;
	}
	VN_RELE(vp);
}

/*ARGSUSED*/
void
rfs_null(argp, resp)
	caddr_t argp;
	caddr_t resp;
{
	/* do nothing */
}

/*
 * Fast path for arguments to directory operations like lookup
 * Only called on decoding, when using xdr_mbuf module.
 */
extern bool_t xdr_fast_diropargs(XDR *, struct nfsdiropargs *);
extern bool_t xdr_fast_3(XDR *, struct diropargs3 *);

/*
 * Fast path for results from directory operation like lookup
 * We know we are encoding, and all sizes are the same, so just structure copy
 * WARNING: this will break whenever the internal representation does not
 * match the external, e.g. on a little endian machine or int != 32 bits.
 */
static bool_t
xdr_fast_diropres(xdrs, dr)
        XDR *xdrs;
        struct nfsdiropres *dr;
{
	*(struct nfsdiropres *) (xdrs->x_private) = *dr;
	if (dr->dr_status) 
#pragma mips_frequency_hint NEVER
	  xdrs->x_private += BYTES_PER_XDR_UNIT;
	else
	  xdrs->x_private += sizeof (struct nfsdiropres);
	return (TRUE);
}

#ifdef TRACE
static char *rfscallnames_v2[] = {
	"RFS2_NULL",
	"RFS2_GETATTR",
	"RFS2_SETATTR",
	"RFS2_ROOT",
	"RFS2_LOOKUP",
	"RFS2_READLINK",
	"RFS2_READ",
	"RFS2_WRITECACHE",
	"RFS2_WRITE",
	"RFS2_CREATE",
	"RFS2_REMOVE",
	"RFS2_RENAME",
	"RFS2_LINK",
	"RFS2_SYMLINK",
	"RFS2_MKDIR",
	"RFS2_RMDIR",
	"RFS2_READDIR",
	"RFS2_STATFS"
};
#endif

/*
 * rfs dispatch table
 * Indexed by version,proc
 */
#define RFS_IDEMPOTENT	0x1	/* idempotent or not */
#define RFS_ALLOWANON	0x2	/* allow anonymous access */

/*
 * Types with prototypes to get ANSI parameter checking
 */
typedef void (*dispatch_proc_t) (caddr_t, caddr_t, struct exportinfo *,
				 struct svc_req *, cred_t *);
typedef void (*free_proc_t) (caddr_t);

struct rfsdisp {
	void	  (*dis_proc)();	/* proc to call */
	xdrproc_t dis_xdrargs;		/* xdr routine to get args */
	xdrproc_t dis_xdrres;		/* xdr routine to put results */
	void	  (*dis_resfree)();	/* frees space allocated by proc */
	int       dis_flags;            /* flags, see below */
};

static struct rfsdisp rfsdisptab_v2[] = {
	/*
	 * NFS VERSION 2 servr operations
	 */
	/* RFS_NULL = 0 */
	{(void (*)())rfs_null, xdr_void, xdr_void, NULL, RFS_IDEMPOTENT},

	/* RFS_GETATTR = 1 */
	{(void (*)())rfs_getattr, xdr_fhandle, xdr_attrstat, NULL, 
	   RFS_IDEMPOTENT|RFS_ALLOWANON},

	/* RFS_SETATTR = 2 */
	{(void (*)())rfs_setattr, xdr_saargs, xdr_attrstat, NULL, 0},

	/* RFS_ROOT = 3 *** NO SUCH OP *** */
	{(void (*)())rfs_null, xdr_void, xdr_void, NULL, 0},

	/* RFS_LOOKUP = 4 */
	{(void (*)())rfs_lookup, xdr_fast_diropargs,
	    xdr_fast_diropres, NULL, RFS_IDEMPOTENT},

	/* RFS_READLINK = 5 */
	{(void (*)())rfs_readlink, xdr_fhandle, xdr_rdlnres,
	     (void (*)())rfs_rlfree, RFS_IDEMPOTENT},

	/* RFS_READ = 6 */
	{(void (*)())rfs_read, xdr_readargs, xdr_rdresult, NULL,
	   RFS_IDEMPOTENT},

	/* RFS_WRITECACHE = 7 *** NO SUCH OP *** */
	{(void (*)())rfs_null, xdr_void, xdr_void, NULL, 0},

	/* RFS_WRITE = 8 */
	{(void (*)())rfs_write, xdr_writeargs,
	    xdr_attrstat, NULL, 0},

	/* RFS_CREATE = 9 */
	{(void (*)())rfs_create, xdr_creatargs, xdr_fast_diropres, NULL, 0},

	/* RFS_REMOVE = 10 */
	{(void (*)())rfs_remove, xdr_fast_diropargs, xdr_enum, NULL, 0},

	/* RFS_RENAME = 11 */
	{(void (*)())rfs_rename, xdr_rnmargs, xdr_enum, NULL, 0},

	/* RFS_LINK = 12 */
	{(void (*)())rfs_link, xdr_linkargs, xdr_enum, NULL, 0},

	/* RFS_SYMLINK = 13 */
	{(void (*)())rfs_symlink, xdr_slargs, xdr_enum, NULL, 0},

	/* RFS_MKDIR = 14 */
	{(void (*)())rfs_mkdir, xdr_creatargs, xdr_fast_diropres, NULL, 0},

	/* RFS_RMDIR = 15 */
	{(void (*)())rfs_rmdir, xdr_fast_diropargs, xdr_enum, NULL, 0},

	/* RFS_READDIR = 16 */
	{(void (*)())rfs_readdir, xdr_rddirargs, xdr_putrddirres,
	   (void (*)())rfs_rddirfree, RFS_IDEMPOTENT},

	/* RFS_STATFS = 17 */
	{(void (*)())rfs_statfs, xdr_fhandle, xdr_statfs, NULL,
	   RFS_IDEMPOTENT|RFS_ALLOWANON}
};

/*
 * A "verifier" for NFS3, that changes on every reboot, so that
 * clients can detect when a server might have lost some data
 * from async writes that needs to be re-sent. The declaration
 * as nfstime3 is just the way we want to interpret it on the server.
 */
struct nfstime3 nfs3_writeverf;

struct vnode *fh3tovp(nfs_fh3 *, struct exportinfo *);
static void		nullfree(void);

/* ARGSUSED */
static void
rfs3_getattr(GETATTR3args *args, GETATTR3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	struct vattr va;

	vp = fh3tovp(&args->object, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		return;
	}

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	resp->status = puterrno3(error);
	if (!error)
		vattr_to_fattr3(&va, &resp->resok.obj_attributes);
}

static void
rfs3_setattr(SETATTR3args *args, SETATTR3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int bverror;
	int averror;
	vnode_t *vp;
	struct vattr bva;
	struct vattr ava;
	int flag;

	vp = fh3tovp(&args->object, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_wcc.before.attributes = FALSE;
		resp->resfail.obj_wcc.after.attributes = FALSE;
		return;
	}

	bva.va_mask = AT_NFS;
	VOP_GETATTR(vp, &bva, 0, cr, bverror);

	if (rdonly(exi, req) || (vp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		VN_RELE(vp);
		resp->status = NFS3ERR_ROFS;
		if (bverror) {
			resp->resfail.obj_wcc.before.attributes = FALSE;
			resp->resfail.obj_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.obj_wcc);
		return;
	}

	if (args->guard.check) {
		if (bverror ||
		    args->guard.obj_ctime.seconds != bva.va_ctime.tv_sec ||
		    args->guard.obj_ctime.nseconds != bva.va_ctime.tv_nsec) {
			VN_RELE(vp);
			if (bverror) {
				resp->status = puterrno3(bverror);
				resp->resfail.obj_wcc.before.attributes = FALSE;
				resp->resfail.obj_wcc.after.attributes = FALSE;
			} else {
				resp->status = NFS3ERR_NOT_SYNC;
				vattr_to_wcc_data(&bva, &bva,
						    &resp->resfail.obj_wcc);
			}
			return;
		}
	}

	error = sattr3_to_vattr(&args->new_attributes, &ava);
	if (error) {
		VN_RELE(vp);
		resp->status = puterrno3(error);
		if (bverror) {
			resp->resfail.obj_wcc.before.attributes = FALSE;
			resp->resfail.obj_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.obj_wcc);
		return;
	}
	flag = ATTR_NONBLOCK;
	if (args->new_attributes.mtime.set_it == SET_TO_CLIENT_TIME)
		flag |= ATTR_UTIME;

	VOP_SETATTR(vp, &ava, flag, cr, error);
	if (error == EACCES &&
	    (ava.va_mask & (AT_MODE|AT_SIZE)) == (AT_MODE|AT_SIZE)) {
		/*
		 * Work around the filesystem limitation that can cause
		 * AT_SIZE changes to fail when also changing AT_MODE.
		 * This is common during exclusive creates.
		 */
		ava.va_mask &= ~AT_SIZE;
		VOP_SETATTR(vp, &ava, flag, cr, error);
		if (!error) {
			ava.va_mask = AT_SIZE;
			VOP_SETATTR(vp, &ava, flag, cr, error);
		}
	}

	ava.va_mask = AT_NFS;
	VOP_GETATTR(vp, &ava, 0, cr, averror);

	VN_RELE(vp);

	if (error) {
		resp->status = puterrno3(error);
		if (bverror) {
			resp->resfail.obj_wcc.before.attributes = FALSE;
			if (averror)
				resp->resfail.obj_wcc.after.attributes = FALSE;
			else {
				resp->resfail.obj_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&ava,
					    &resp->resfail.obj_wcc.after.attr);
			}
		} else {
			if (averror) {
				resp->resfail.obj_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&bva,
					&resp->resfail.obj_wcc.before.attr);
				resp->resfail.obj_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&bva, &ava,
						    &resp->resfail.obj_wcc);
			}
		}
		return;
	}

	resp->status = NFS3_OK;
	if (bverror) {
		resp->resok.obj_wcc.before.attributes = FALSE;
		if (averror)
			resp->resok.obj_wcc.after.attributes = FALSE;
		else {
			resp->resok.obj_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&ava, &resp->resok.obj_wcc.after.attr);
		}
	} else {
		if (averror) {
			resp->resok.obj_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&bva,
					&resp->resok.obj_wcc.before.attr);
			resp->resok.obj_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &ava, &resp->resok.obj_wcc);
	}
}

/* ARGSUSED */
static void
rfs3_lookup(LOOKUP3args *args, LOOKUP3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int derror;
	vnode_t *vp;
	vnode_t *dvp;
	vnode_t *cvp = NULLVP;	/* current component {d}vp */
	struct vattr va;
	struct vattr dva;
	nfs_fh3 *fhp;

	dvp = fh3tovp(&args->what.dir, exi);
	if (dvp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_attributes.attributes = FALSE;
		return;
	} 

	dva.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dva, 0, cr, derror);

	if (args->what.name == NULL || *(args->what.name) == '\0') {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ACCES;
		if (derror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&dva,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

	fhp = &args->what.dir;
	if (strcmp(args->what.name, "..") == 0 &&
	    (fhp->fh3_len == fhp->fh3_xlen) &&
	    !(exi->exi_export.ex_flags & EX_NOHIDE) &&
	    (bcmp(fhp->fh3_data, fhp->fh3_xdata, fhp->fh3_len) == 0)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_NOENT;
		if (derror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&dva,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

lookuploop3:
	VOP_LOOKUP(dvp, args->what.name, &vp, (struct pathname *)NULL,
			0, (vnode_t *)NULL, cr, error);

	if (error) {
		resp->status = puterrno3(error);
		if (derror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&dva,
					&resp->resfail.dir_attributes.attr);
		}
		VN_RELE(dvp);
		return;
	} else {
		if ((dvp->v_flag & VROOT)
		    && dvp->v_vfsp->vfs_vnodecovered
		    && ISDOTDOT(args->what.name)
		    && (exi->exi_export.ex_flags & EX_NOHIDE)) {
			if (!(findexivp(&exi, NULL,
				    dvp->v_vfsp->vfs_vnodecovered))) {
			    VN_RELE(vp);
			    cvp = dvp;
			    dvp = dvp->v_vfsp->vfs_vnodecovered;
			    VN_HOLD(dvp);
			    VN_RELE(cvp);
			    EXPORTED_MRUNLOCK();
			    goto lookuploop3;
			}
		}
		if (vp->v_vfsmountedhere) {
		    VN_HOLD(vp);
		    cvp = vp;
		    if (error = traverse(&vp)) {
			VN_RELE(vp);
			error = 0;
		    } else {
			int rv;
			if ((rv = findexivp(&exi, NULL, vp)) != 0
			    || !(exi->exi_export.ex_flags & EX_NOHIDE)) {
				/* not nohide; back-up */
				VN_RELE(vp);
				vp = cvp;
				if (rv == 0)
				    EXPORTED_MRUNLOCK();
				error = findexivp(&exi, NULL, vp);
				if (error == 0)
				    EXPORTED_MRUNLOCK();
			    } else {
				VN_RELE(cvp);
				if (rv == 0)
				    EXPORTED_MRUNLOCK();
			    }
		    }
		}
		if (!error && exi)
			error = makefh3(&resp->resok.object, vp, exi);
	}

	if (error) {
		VN_RELE(vp);
		resp->status = puterrno3(error);
		if (derror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&dva,
					&resp->resfail.dir_attributes.attr);
		}
		VN_RELE(dvp);
		return;
	}

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);
	VN_RELE(dvp);

	resp->status = NFS3_OK;
	if (error)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
	if (derror)
		resp->resok.dir_attributes.attributes = FALSE;
	else {
		resp->resok.dir_attributes.attributes = TRUE;
		vattr_to_fattr3(&dva, &resp->resok.dir_attributes.attr);
	}
}

/* ARGSUSED */
static void
rfs3_access(ACCESS3args *args, ACCESS3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	struct vattr va;

	vp = fh3tovp(&args->object, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	resp->resok.access = 0;

	if (args->access & ACCESS3_READ) {
		VOP_ACCESS(vp, VREAD, cr, error);
		if (!error)
			resp->resok.access |= ACCESS3_READ;
	}
	if ((args->access & ACCESS3_LOOKUP) && vp->v_type == VDIR) {
		VOP_ACCESS(vp, VEXEC, cr, error);
		if (!error)
			resp->resok.access |= ACCESS3_LOOKUP;
	}
	if (args->access & (ACCESS3_MODIFY|ACCESS3_EXTEND)) {
		VOP_ACCESS(vp, VWRITE, cr, error);
		if (!error)
			resp->resok.access |=
			    (args->access & (ACCESS3_MODIFY|ACCESS3_EXTEND));
	}
	if ((args->access & ACCESS3_DELETE) && vp->v_type == VDIR) {
		VOP_ACCESS(vp, VWRITE, cr, error);
		if (!error)
			resp->resok.access |= ACCESS3_DELETE;
	}
	if (args->access & ACCESS3_EXECUTE) {
		VOP_ACCESS(vp, VEXEC, cr, error);
		if (!error)
			resp->resok.access |= ACCESS3_EXECUTE;
	}

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	resp->status = NFS3_OK;
	if (error)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
}

/* ARGSUSED */
static void
rfs3_readlink(READLINK3args *args, READLINK3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	struct vattr va;
	struct iovec iov;
	struct uio uio;
	char *data;

	vp = fh3tovp(&args->symlink, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.symlink_attributes.attributes = FALSE;
		return;
	}

	data = kmem_zone_alloc(rfs_pn_zone, KM_SLEEP);

	iov.iov_base = data;
	iov.iov_len = uio.uio_resid = NFS_MAXPATHLEN-1;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_fmode = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_offset = 0;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;

	VOP_READLINK(vp, &uio, cr, error);
	if (error)
		resp->status = puterrno3(error);
	else
		resp->status = NFS3_OK;

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	if (resp->status != NFS3_OK) {
		kmem_zone_free(rfs_pn_zone, data);
		if (error)
			resp->resfail.symlink_attributes.attributes = FALSE;
		else {
			resp->resfail.symlink_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.symlink_attributes.attr);
		}
		return;
	}

	if (error)
		resp->resok.symlink_attributes.attributes = FALSE;
	else {
		resp->resok.symlink_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.symlink_attributes.attr);
	}
	resp->resok.data = data;
	*(data + (NFS_MAXPATHLEN - 1) - uio.uio_resid) = '\0';
}

static void
rfs3_readlink_free(READLINK3res *resp)
{

	if (resp->status == NFS3_OK)
		kmem_zone_free(rfs_pn_zone, resp->resok.data);
}

/* ARGSUSED */
static void
rfs3_read(READ3args *args, READ3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error, verror, eof, readflags = 0;
	vnode_t *vp;
	struct vattr va;
	struct iovec iov;
	struct uio uio;
	off_t offset;
	char *data;
#if _PAGESZ >= 16*1024
	int pboff, pbsize;
	struct buf *bp;
	extern void     chunkrelse(buf_t*);
#endif

	vp = fh3tovp(&args->file, exi);
	if (vp == NULL) {
#pragma mips_frequency_hint NEVER
		resp->status = NFS3ERR_STALE;
		resp->resfail.file_attributes.attributes = FALSE;
		return;
	}

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	if (vp->v_type != VREG) {
#pragma mips_frequency_hint NEVER
		VN_RELE(vp);
		resp->status = NFS3ERR_INVAL;
		if (verror)
			resp->resfail.file_attributes.attributes = FALSE;
		else {
			resp->resfail.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.file_attributes.attr);
		}
		return;
	}

	if (cr->cr_uid != va.va_uid) {
		VOP_ACCESS(vp, VREAD, cr, error);
		if (error) {
#pragma mips_frequency_hint NEVER
			VOP_ACCESS(vp, VEXEC, cr, error);
		}
		if (error) {
			VN_RELE(vp);
			resp->status = puterrno3(error);
			if (verror) {
				resp->resfail.file_attributes.attributes =
									FALSE;
			} else {
				resp->resfail.file_attributes.attributes = TRUE;
				vattr_to_fattr3(&va,
					&resp->resfail.file_attributes.attr);
			}
			return;
		}
	}

	offset = args->offset;
	eof = ((offset >= va.va_size) ? TRUE : FALSE);

	if (eof || args->count == 0) {
#pragma mips_frequency_hint NEVER
		VN_RELE(vp);
		resp->status = NFS3_OK;
		if (verror)
			resp->resok.file_attributes.attributes = FALSE;
		else {
			resp->resok.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va, &resp->resok.file_attributes.attr);
		}
		resp->resok.count = 0;
		resp->resok.eof = eof;
		resp->resok.data.data_len = 0;
		resp->resok.data.data_val = NULL;
		return;
	}

	if (args->count > rfs3_tsize(req->rq_xprt)) {
#pragma mips_frequency_hint NEVER
		args->count = rfs3_tsize(req->rq_xprt);
	}

	resp->resok.bp = NULL;

#if _MIPS_SIM == _ABI64
	/*
	 * Try the fast-path and see if it works.  If not,
	 * we fall through to the slow path.  Signal that we're
	 * NFS (no read ahead) and that we need a one-page buffer coming back.
	 *
	 * We can depend on getting a direct mapping to any page
	 * only in the 64-bit addressing model. If the request
	 * is larger than a page, bag it; too complicated.
	 */

	if (args->count > _PAGESZ)
		goto page2small;

	bp = NULL;
	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_READBUF(vp, offset, args->count, IO_ISLOCKED|IO_NFS3|IO_ONEPAGE, cr,
			&curuthread->ut_flid, &bp, &pboff, &pbsize, error);

	if (error == 0 && bp != NULL)  {
		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			       NETEVENT_NFSUP, pbsize, 
			       NETRES_READ);
		va.va_mask = AT_NFS;
		VOP_GETATTR(vp, &va, 0, cr, verror);
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		resp->resok.bp = bp;
		resp->resok.count = pbsize;
		resp->resok.pboff = pboff;
		resp->status = NFS3_OK;
		if (verror)
			resp->resok.file_attributes.attributes = FALSE;
		else {
			resp->resok.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va, &resp->resok.file_attributes.attr);
		}
		if (offset + resp->resok.count == va.va_size)
			resp->resok.eof = TRUE;
		else
			resp->resok.eof = FALSE;
		return;		/* VN_RELE() done in rbrele() */
	}
	VOP_RWUNLOCK(vp, VRWLOCK_READ);
page2small:

#endif /* big pointers */

	/*
	 * Allocate space for data.  This will be freed by the nfs_xdr
	 * call-back mechanism when the reference count goes to zero.
	 */

	data = (char *)kvpalloc(btoc(args->count), VM_BULKDATA, 0);

	iov.iov_base = data;
	iov.iov_len = args->count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_offset = offset;
	uio.uio_resid = args->count;
	uio.uio_pmp = NULL;
	uio.uio_fmode = FNONBLOCK;
	uio.uio_pio = 0;
	if (args->count <= 32768) {
		if (args->count <= 16384)
			uio.uio_readiolog = 14;		/* 16K */
		else
			uio.uio_readiolog = 15;		/* 32K */
		readflags |= IO_UIOSZ;
	}
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;


	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_READ(vp, &uio, IO_ISLOCKED | IO_NFS3 | readflags,
		 cr, &curuthread->ut_flid, error);
		
	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);
	VOP_RWUNLOCK(vp, VRWLOCK_READ);
	VN_RELE(vp);

	if (error) {
		resp->status = puterrno3(error);
		kvpfree(data, btoc(args->count));
		if (verror)
			resp->resfail.file_attributes.attributes = FALSE;
		else {
			resp->resfail.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va, &resp->resfail.file_attributes.attr);
		}
		return;
	}

	resp->status = NFS3_OK;
	if (verror)
		resp->resok.file_attributes.attributes = FALSE;
	else {
		resp->resok.file_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.file_attributes.attr);
	}
	resp->resok.count = args->count - uio.uio_resid;
	if (offset + resp->resok.count == va.va_size)
		resp->resok.eof = TRUE;
	else
		resp->resok.eof = FALSE;
	resp->resok.data.data_len = args->count;
	resp->resok.data.data_val = data;
}

static void
rfs3_write(WRITE3args *args, WRITE3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int bverror;
	int averror;
	vnode_t *vp;
	struct vattr bva;
	struct vattr ava;
	off_t rlimit;
	rlim_t fsizelim;
	struct uio uio;
	struct mbuf *m;
	struct iovec medium_iov[MEDIUM_IOV_CNT];
	struct iovec *iovp;
	int iovcnt;
	int writeflags = 0;

	vp = fh3tovp(&args->file, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.file_wcc.before.attributes = FALSE;
		resp->resfail.file_wcc.after.attributes = FALSE;
		goto freepath;
	}

	VOP_RWLOCK(vp, VRWLOCK_WRITE);

	bva.va_mask = AT_NFS;
	VOP_GETATTR(vp, &bva, 0, cr, bverror);

	if (rdonly(exi, req)) {
		VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
		VN_RELE(vp);
		resp->status = NFS3ERR_ROFS;
		if (bverror) {
			resp->resfail.file_wcc.before.attributes = FALSE;
			resp->resfail.file_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.file_wcc);
		goto freepath;
	}
	if (vp->v_type != VREG) {
		VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
		VN_RELE(vp);
		resp->status = NFS3ERR_INVAL;
		if (bverror) {
			resp->resfail.file_wcc.before.attributes = FALSE;
			resp->resfail.file_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.file_wcc);
		goto freepath;
	}

	if (cr->cr_uid != bva.va_uid) {
		VOP_ACCESS(vp, VWRITE, cr, error);
		if (error && (error != EACCES)) {
			VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
			VN_RELE(vp);
			resp->status = puterrno3(error);
			if (bverror) {
				resp->resfail.file_wcc.before.attributes =
									FALSE;
				resp->resfail.file_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&bva, &bva,
						    &resp->resfail.file_wcc);
			}
			goto freepath;
		}
	}

	if (args->count == 0) {
		VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
		VN_RELE(vp);
		resp->status = NFS3_OK;
		if (bverror) {
			resp->resok.file_wcc.before.attributes = FALSE;
			resp->resok.file_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resok.file_wcc);
		resp->resok.count = 0;
		resp->resok.committed = args->stable;
		goto freepath;
	} else if (args->count > rfs3_tsize(req->rq_xprt)) {
#ifdef DEBUG 
		printf("nfs server received bogus write count %d\n", 
				args->count);
#endif
		args->count = rfs3_tsize(req->rq_xprt);
	}
		

	/*
	 * We should assume that only "mbufs"
	 * are used as the XDR layer. So we never
	 * get get regular data decoded.
	 */
	iovcnt = 0;
	for (m = args->mbuf; m; m = m->m_next) {
		iovcnt++;
	}
	if (iovcnt <= MEDIUM_IOV_CNT) {
		iovp = medium_iov;
	} else if (iovcnt <= LARGE_IOV_CNT) {
		iovp = (struct iovec *)
		    kmem_zone_alloc(large_iovec_zone,
				    KM_SLEEP);
	} else {
#pragma mips_frequency_hint NEVER
		iovp = (struct iovec *)
		    kmem_alloc(sizeof(struct iovec) * iovcnt,
			       KM_SLEEP);
	}
	mbuf_to_iov(args->mbuf, iovp);
	uio.uio_iov = iovp;
	uio.uio_iovcnt = iovcnt;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_offset = args->offset;
	uio.uio_resid = args->count;
	uio.uio_pmp = NULL;
	uio.uio_sigpipe = 0;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	if (args->count <= 32768) {
		if (args->count <= 16384)
			uio.uio_readiolog = 14;		/* 16K */
		else
			uio.uio_readiolog = 15;		/* 32K */
		writeflags |= IO_UIOSZ;
	}
	uio.uio_pbuf = 0;
	fsizelim = getfsizelimit();
	rlimit = fsizelim - args->offset;
	if (rlimit < uio.uio_resid)
		uio.uio_resid = rlimit;
	uio.uio_limit = fsizelim;
	uio.uio_fmode = FNONBLOCK;

	if (args->stable != UNSTABLE)
		writeflags |= IO_SYNC;

	/*
	 * Make sure we write everything or else return an error.  To do this,
	 * we must verify that uio_resid is 0.  If it is non-zero, we must do
	 * the write again.  The uio structure will not need to be modified
	 * to do this.  If no space is left on the device, an error will be
	 * returned.
	 */
	do {
		VOP_WRITE(vp, &uio, IO_ISLOCKED | IO_NFS3 | writeflags,
			  cr, &curuthread->ut_flid, error);
	} while (!error && uio.uio_resid);
	if (error) {
		ASSERT(uio.uio_sigpipe == 0);
		resp->status = puterrno3(error);
	} else
		resp->status = NFS3_OK;

	if (iovcnt > MEDIUM_IOV_CNT) {
		if (iovcnt <= LARGE_IOV_CNT) {
			kmem_zone_free(large_iovec_zone,
				       iovp);
		} else {
			kmem_free((caddr_t)iovp,
				  (u_int)(sizeof (struct iovec) 
					  * iovcnt));
		}
	}
	ava.va_mask = AT_NFS;
	VOP_GETATTR(vp, &ava, 0, cr, averror);

	VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
	VN_RELE(vp);

	if (resp->status != NFS3_OK) {
		if (bverror) {
			resp->resfail.file_wcc.before.attributes = FALSE;
			if (averror)
				resp->resfail.file_wcc.after.attributes = FALSE;
			else {
				resp->resfail.file_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&ava,
					    &resp->resfail.file_wcc.after.attr);
			}
		} else {
			if (averror) {
				resp->resfail.file_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&bva,
					&resp->resfail.file_wcc.before.attr);
				resp->resfail.file_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&bva, &ava,
						    &resp->resfail.file_wcc);
			}
		}
		goto freepath;
	}

	if (bverror) {
		resp->resok.file_wcc.before.attributes = FALSE;
		if (averror)
			resp->resok.file_wcc.after.attributes = FALSE;
		else {
			resp->resok.file_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&ava, &resp->resok.file_wcc.after.attr);
		}
	} else {
		if (averror) {
			resp->resok.file_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&bva,
					&resp->resok.file_wcc.before.attr);
			resp->resok.file_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&bva, &ava, &resp->resok.file_wcc);
		}
	}
	resp->resok.count = args->count - uio.uio_resid;
	if (writeflags & IO_SYNC)
		resp->resok.committed = FILE_SYNC;
	else
		resp->resok.committed = UNSTABLE;
	bcopy(&nfs3_writeverf, &resp->resok.verf, sizeof (nfs3_writeverf));
    freepath:
	if (args->data.data_val) {
		kmem_free(args->data.data_val, args->data.data_len);
	}
	
}

static void
rfs3_create(CREATE3args *args, CREATE3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int verror;
	int dbverror;
	int daverror;
	vnode_t *vp = NULL;		/* imon looks at initial contents */
	vnode_t *dvp;
	struct vattr va;
	struct vattr dbva;
	struct vattr dava;
	int mode;
	int cflags = 0;
	timespec_t *mtime;

	dvp = fh3tovp(&args->where.dir, exi);
	if (dvp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_wcc.before.attributes = FALSE;
		resp->resfail.dir_wcc.after.attributes = FALSE;
		goto freepath;
	}

	dbva.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dbva, 0, cr, dbverror);

	if (!sane(args->where.name)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ACCES;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	if (rdonly(exi, req)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ROFS;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;		/* free memory */
	}

	if (args->how.mode == EXCLUSIVE) {
		va.va_mask = AT_TYPE | AT_MODE | AT_MTIME;
		va.va_type = VREG;
		va.va_mode = (mode_t)0;
		mtime = (timespec_t *)&args->how.createhow3_u.verf;
		va.va_mtime.tv_sec = mtime->tv_sec;
		va.va_mtime.tv_nsec = mtime->tv_nsec;
		cflags |= VEXCL;
	} else {
		error = sattr3_to_vattr(&args->how.createhow3_u.obj_attributes,
					&va);
		if (error) {
			VN_RELE(dvp);
			resp->status = puterrno3(error);
			if (dbverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dbva,
						    &resp->resfail.dir_wcc);
			}
			goto freepath;
		}
		va.va_mask |= AT_TYPE;
		va.va_type = VREG;
		if (args->how.mode == GUARDED)
			cflags |= VEXCL;
	}

	mode = VWRITE;

tryagain:
	VOP_CREATE(dvp, args->where.name, &va, cflags, mode, &vp, cr, error);

	if (!error)
		error = _SESMGR_NFS_VSETLABEL(vp, mtod(req->rq_xprt->xp_ipattr, struct ipsec *));

	dava.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dava, 0, cr, daverror);

	if (error) {
		if (error != EEXIST || args->how.mode == GUARDED) {
			VN_RELE(dvp);
			resp->status = puterrno3(error);
			if (dbverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				if (daverror) {
					resp->resfail.dir_wcc.after.attributes
									= FALSE;
				} else {
					resp->resfail.dir_wcc.after.attributes
									= TRUE;
					vattr_to_fattr3(&dava,
					    &resp->resfail.dir_wcc.after.attr);
				}
			} else {
				if (daverror) {
					resp->resfail.dir_wcc.before.attributes
									= TRUE;
					vattr_to_wcc_attr(&dbva,
					    &resp->resfail.dir_wcc.before.attr);
					resp->resfail.dir_wcc.after.attributes
									= FALSE;
				} else {
					vattr_to_wcc_data(&dbva, &dava,
							&resp->resfail.dir_wcc);
				}
			}
			goto freepath;
		}
		VOP_LOOKUP(dvp, args->where.name, &vp,
				(struct pathname *)NULL, 0,
				(vnode_t *)NULL, cr, error);
		if (error) {
			if (error == ENOENT)
				goto tryagain;
			VN_RELE(dvp);
			resp->status = puterrno3(error);
			if (dbverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				if (daverror) {
					resp->resfail.dir_wcc.after.attributes
									= FALSE;
				} else {
					resp->resfail.dir_wcc.after.attributes
									= TRUE;
					vattr_to_fattr3(&dava,
					    &resp->resfail.dir_wcc.after.attr);
				}
			} else {
				if (daverror) {
					resp->resfail.dir_wcc.before.attributes
									= TRUE;
					vattr_to_wcc_attr(&dbva,
					&resp->resfail.dir_wcc.before.attr);
					resp->resfail.dir_wcc.after.attributes
									= FALSE;
				} else {
					vattr_to_wcc_data(&dbva, &dava,
							&resp->resfail.dir_wcc);
				}
			}
			goto freepath;
		}

		va.va_mask = AT_NFS;
		VOP_GETATTR(vp, &va, 0, cr, verror);

		mtime = (timespec_t *)&args->how.createhow3_u.verf;
		if (args->how.mode == EXCLUSIVE && (verror ||
		    va.va_mtime.tv_sec != mtime->tv_sec ||
		    va.va_mtime.tv_nsec != mtime->tv_nsec)) {
			VN_RELE(vp);
			VN_RELE(dvp);
			resp->status = NFS3ERR_EXIST;
			if (dbverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				if (daverror) {
					resp->resfail.dir_wcc.after.attributes
									= FALSE;
				} else {
					resp->resfail.dir_wcc.after.attributes
									= TRUE;
					vattr_to_fattr3(&dava,
					    &resp->resfail.dir_wcc.after.attr);
				}
			} else {
				if (daverror) {
					resp->resfail.dir_wcc.before.attributes
									= TRUE;
					vattr_to_wcc_attr(&dbva,
					&resp->resfail.dir_wcc.before.attr);
					resp->resfail.dir_wcc.after.attributes
									= FALSE;
				} else {
					vattr_to_wcc_data(&dbva, &dava,
							&resp->resfail.dir_wcc);
				}
			}
			goto freepath;
		}
	} else {
		va.va_mask = AT_NFS;
		VOP_GETATTR(vp, &va, 0, cr, verror);
	}

	error = makefh3(&resp->resok.obj.handle,
			vp, exi);

	VN_RELE(vp);
	VN_RELE(dvp);

	if (error)
		resp->resok.obj.handle_follows = FALSE;
	else
		resp->resok.obj.handle_follows = TRUE;

	resp->status = NFS3_OK;
	if (verror)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
	if (dbverror) {
		resp->resok.dir_wcc.before.attributes = FALSE;
		if (daverror)
			resp->resok.dir_wcc.after.attributes = FALSE;
		else {
			resp->resok.dir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&dava, &resp->resok.dir_wcc.after.attr);
		}
	} else {
		if (daverror) {
			resp->resok.dir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&dbva,
					&resp->resok.dir_wcc.before.attr);
			resp->resok.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dava, &resp->resok.dir_wcc);
	}
    freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->where.name);
}

static void
rfs3_mkdir(MKDIR3args *args, MKDIR3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int dbverror;
	int daverror;
	vnode_t *vp;
	vnode_t *dvp;
	struct vattr va;
	struct vattr dbva;
	struct vattr dava;

	dvp = fh3tovp(&args->where.dir, exi);
	if (dvp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_wcc.before.attributes = FALSE;
		resp->resfail.dir_wcc.after.attributes = FALSE;
		goto freepath;
	}

	dbva.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dbva, 0, cr, dbverror);

	if (!sane(args->where.name)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ACCES;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	if (rdonly(exi, req)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ROFS;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	error = sattr3_to_vattr(&args->attributes, &va);
	if (error) {
		VN_RELE(dvp);
		resp->status = puterrno3(error);
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	if (!(va.va_mask & AT_MODE)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_INVAL;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	va.va_mask |= AT_TYPE;
	va.va_type = VDIR;

	VOP_MKDIR(dvp, args->where.name, &va, &vp, cr, error);

	if (!error)
		error = _SESMGR_NFS_VSETLABEL(vp, mtod(req->rq_xprt->xp_ipattr, struct ipsec *));

	dava.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dava, 0, cr, daverror);

	VN_RELE(dvp);

	if (error) {
		resp->status = puterrno3(error);
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			if (daverror)
				resp->resfail.dir_wcc.after.attributes = FALSE;
			else {
				resp->resfail.dir_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&dava,
					    &resp->resfail.dir_wcc.after.attr);
			}
		} else {
			if (daverror) {
				resp->resfail.dir_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&dbva,
					&resp->resfail.dir_wcc.before.attr);
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dava,
						&resp->resfail.dir_wcc);
			}
		}
		goto freepath;
	}

	error = makefh3(&resp->resok.obj.handle, vp, exi);

	if (error)
		resp->resok.obj.handle_follows = FALSE;
	else
		resp->resok.obj.handle_follows = TRUE;

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	resp->status = NFS3_OK;
	if (error)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
	if (dbverror) {
		resp->resok.dir_wcc.before.attributes = FALSE;
		if (daverror)
			resp->resok.dir_wcc.after.attributes = FALSE;
		else {
			resp->resok.dir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&dava, &resp->resok.dir_wcc.after.attr);
		}
	} else {
		if (daverror) {
			resp->resok.dir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&dbva,
					&resp->resok.dir_wcc.before.attr);
			resp->resok.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dava, &resp->resok.dir_wcc);
	}
    freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->where.name);
}

static void
rfs3_symlink(SYMLINK3args *args, SYMLINK3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int dbverror;
	int daverror;
	register int nodesize;
	vnode_t *vp;
	vnode_t *dvp;
	struct vattr va;
	struct vattr dbva;
	struct vattr dava;

	dvp = fh3tovp(&args->where.dir, exi);
	if (dvp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_wcc.before.attributes = FALSE;
		resp->resfail.dir_wcc.after.attributes = FALSE;
		goto freepath;
	}
	if (dvp->v_type != VDIR) {
		resp->status = NFS3ERR_NOTDIR;
		resp->resfail.dir_wcc.before.attributes = FALSE;
		resp->resfail.dir_wcc.after.attributes = FALSE;
		VN_RELE(dvp);
		goto freepath;
	}

	dbva.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dbva, 0, cr, dbverror);

	if (!sane(args->where.name)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ACCES;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	if (rdonly(exi, req)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ROFS;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	error = sattr3_to_vattr(&args->symlink.symlink_attributes, &va);
	if (error) {
		VN_RELE(dvp);
		resp->status = puterrno3(error);
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}
	va.va_mask |= AT_TYPE;
	va.va_type = VLNK;

	VOP_SYMLINK(dvp, args->where.name, &va,
			    args->symlink.symlink_data, cr, error);

	dava.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dava, 0, cr, daverror);

	if (error) {
		VN_RELE(dvp);
		resp->status = puterrno3(error);
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			if (daverror)
				resp->resfail.dir_wcc.after.attributes = FALSE;
			else {
				resp->resfail.dir_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&dava,
					    &resp->resfail.dir_wcc.after.attr);
			}
		} else {
			if (daverror) {
				resp->resfail.dir_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&dbva,
					&resp->resfail.dir_wcc.before.attr);
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dava,
						&resp->resfail.dir_wcc);
			}
		}
		goto freepath;
	}

	VOP_LOOKUP(dvp, args->where.name, &vp, (struct pathname *)NULL,
			0, (vnode_t *)NULL, cr, error);

	VN_RELE(dvp);

	resp->status = NFS3_OK;

	if (error) {
		resp->resok.obj.handle_follows = FALSE;
		resp->resok.obj_attributes.attributes = FALSE;
		if (dbverror) {
			resp->resok.dir_wcc.before.attributes = FALSE;
			if (daverror) {
				resp->resok.dir_wcc.after.attributes = FALSE;
			} else {
				resp->resok.dir_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&dava,
					    &resp->resok.dir_wcc.after.attr);
			}
		} else {
			if (daverror) {
				resp->resok.dir_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&dbva,
					    &resp->resok.dir_wcc.before.attr);
				resp->resok.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dava,
						&resp->resok.dir_wcc);
			}
		}
		goto freepath;
	}

	error = makefh3(&resp->resok.obj.handle, vp, exi);
	if (error)
		resp->resok.obj.handle_follows = FALSE;
	else
		resp->resok.obj.handle_follows = TRUE;

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	if (error)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
	if (dbverror) {
		resp->resok.dir_wcc.before.attributes = FALSE;
		if (daverror)
			resp->resok.dir_wcc.after.attributes = FALSE;
		else {
			resp->resok.dir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&dava, &resp->resok.dir_wcc.after.attr);
		}
	} else {
		if (daverror) {
			resp->resok.dir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&dbva,
					&resp->resok.dir_wcc.before.attr);
			resp->resok.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dava, &resp->resok.dir_wcc);
	}
    freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->where.name);
	nodesize = strlen(args->symlink.symlink_data) + 1;
	if (nodesize <= NFS_MAXNAMLEN) {
		kmem_zone_free(nfs_xdr_string_zone, 
			       args->symlink.symlink_data);
	} else {
		kmem_free(args->symlink.symlink_data, nodesize);
	}
}

static void
rfs3_mknod(MKNOD3args *args, MKNOD3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int dbverror;
	int daverror;
	vnode_t *vp = NULL;
	vnode_t *dvp;
	struct vattr va;
	struct vattr dbva;
	struct vattr dava;
	int mode;
	int cflags = 0;

	dvp = fh3tovp(&args->where.dir, exi);
	if (dvp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_wcc.before.attributes = FALSE;
		resp->resfail.dir_wcc.after.attributes = FALSE;
		goto freepath;
	}

	dbva.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dbva, 0, cr, dbverror);

	if (!sane(args->where.name)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ACCES;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	if (rdonly(exi, req)) {
		VN_RELE(dvp);
		resp->status = NFS3ERR_ROFS;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	switch ((int)args->what.type) {
	case NF3CHR:
	case NF3BLK:
		error = sattr3_to_vattr(
				&args->what.mknoddata3_u.device.dev_attributes,
				&va);
		if (error) {
			VN_RELE(dvp);
			resp->status = puterrno3(error);
			if (dbverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dbva,
						&resp->resfail.dir_wcc);
			}
			goto freepath;
		}
		if (!_CAP_CRABLE(cr, CAP_MKNOD)) {
			VN_RELE(dvp);
			resp->status = puterrno3(EPERM);
			if (dbverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dbva,
						&resp->resfail.dir_wcc);
			}
			goto freepath;
		}
		if (args->what.type == NF3CHR)
			va.va_type = VCHR;
		else
			va.va_type = VBLK;
		va.va_rdev = makedev(
				args->what.mknoddata3_u.device.spec.specdata1,
				args->what.mknoddata3_u.device.spec.specdata2);
		va.va_mask |= AT_TYPE | AT_RDEV;
		break;
	case NF3SOCK:
		error = sattr3_to_vattr(
				&args->what.mknoddata3_u.pipe_attributes, &va);
		if (error) {
			VN_RELE(dvp);
			resp->status = puterrno3(error);
			if (dbverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dbva,
						&resp->resfail.dir_wcc);
			}
			goto freepath;
		}
		va.va_type = VSOCK;
		va.va_mask |= AT_TYPE;
		break;
	case NF3FIFO:
		error = sattr3_to_vattr(
				&args->what.mknoddata3_u.pipe_attributes, &va);
		if (error) {
			VN_RELE(dvp);
			resp->status = puterrno3(error);
			if (dbverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dbva,
						&resp->resfail.dir_wcc);
			}
			goto freepath;
		}
		va.va_type = VFIFO;
		va.va_mask |= AT_TYPE;
		break;
	default:
		VN_RELE(dvp);
		resp->status = NFS3ERR_BADTYPE;
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dbva, &resp->resfail.dir_wcc);
		goto freepath;
	}

	cflags |= VEXCL;

	mode = 0;

	VOP_CREATE(dvp, args->where.name, &va, cflags, mode, &vp, cr, error);

	dava.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &dava, 0, cr, daverror);

	VN_RELE(dvp);

	if (error) {
		resp->status = puterrno3(error);
		if (dbverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			if (daverror)
				resp->resfail.dir_wcc.after.attributes = FALSE;
			else {
				resp->resfail.dir_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&dava,
					    &resp->resfail.dir_wcc.after.attr);
			}
		} else {
			if (daverror) {
				resp->resfail.dir_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&dbva,
					    &resp->resfail.dir_wcc.before.attr);
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&dbva, &dava,
						&resp->resfail.dir_wcc);
			}
		}
		goto freepath;
	}

	error = makefh3(&resp->resok.obj.handle,
			vp, exi);
	if (error)
		resp->resok.obj.handle_follows = FALSE;
	else
		resp->resok.obj.handle_follows = TRUE;

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	resp->status = NFS3_OK;
	if (error)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
	if (dbverror) {
		resp->resok.dir_wcc.before.attributes = FALSE;
		if (daverror)
			resp->resok.dir_wcc.after.attributes = FALSE;
		else {
			resp->resok.dir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&dava, &resp->resok.dir_wcc.after.attr);
		}
	} else {
		if (daverror) {
			resp->resok.dir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&dbva,
					&resp->resok.dir_wcc.before.attr);
			resp->resok.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&dbva, &dava, &resp->resok.dir_wcc);
	}
    freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->where.name);
}

static void
rfs3_remove(REMOVE3args *args, REMOVE3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int bverror;
	int averror;
	vnode_t *vp;
	struct vattr bva;
	struct vattr ava;

	vp = fh3tovp(&args->object.dir, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_wcc.before.attributes = FALSE;
		resp->resfail.dir_wcc.after.attributes = FALSE;
		return;
	}

	bva.va_mask = AT_NFS;
	VOP_GETATTR(vp, &bva, 0, cr, bverror);

	if (vp->v_type != VDIR) {
		VN_RELE(vp);
		resp->status = NFS3ERR_NOTDIR;
		if (bverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.dir_wcc);
		return;
	}

	if (args->object.name == NULL || *(args->object.name) == '\0') {
		VN_RELE(vp);
		resp->status = NFS3ERR_ACCES;
		if (bverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.dir_wcc);
		return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(vp);
		resp->status = NFS3ERR_ROFS;
		if (bverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.dir_wcc);
		return;
	}

	VOP_REMOVE(vp, args->object.name, cr, error);

	ava.va_mask = AT_NFS;
	VOP_GETATTR(vp, &ava, 0, cr, averror);

	VN_RELE(vp);

	if (error) {
		resp->status = puterrno3(error);
		if (bverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			if (averror)
				resp->resfail.dir_wcc.after.attributes = FALSE;
			else {
				resp->resfail.dir_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&ava,
					    &resp->resfail.dir_wcc.after.attr);
			}
		} else {
			if (averror) {
				resp->resfail.dir_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&bva,
					&resp->resfail.dir_wcc.before.attr);
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&bva, &ava,
						&resp->resfail.dir_wcc);
			}
		}
		return;
	}

	resp->status = NFS3_OK;
	if (bverror) {
		resp->resok.dir_wcc.before.attributes = FALSE;
		if (averror)
			resp->resok.dir_wcc.after.attributes = FALSE;
		else {
			resp->resok.dir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&ava, &resp->resok.dir_wcc.after.attr);
		}
	} else {
		if (averror) {
			resp->resok.dir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&bva,
					&resp->resok.dir_wcc.before.attr);
			resp->resok.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &ava, &resp->resok.dir_wcc);
	}
}

static void
rfs3_rmdir(RMDIR3args *args, RMDIR3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int bverror;
	int averror;
	vnode_t *vp;
	struct vattr bva;
	struct vattr ava;

	vp = fh3tovp(&args->object.dir, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_wcc.before.attributes = FALSE;
		resp->resfail.dir_wcc.after.attributes = FALSE;
		return;
	}

	bva.va_mask = AT_NFS;
	VOP_GETATTR(vp, &bva, 0, cr, bverror);

	if (vp->v_type != VDIR) {
		VN_RELE(vp);
		resp->status = NFS3ERR_NOTDIR;
		if (bverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.dir_wcc);
		return;
	}

	if (args->object.name == NULL || *(args->object.name) == '\0') {
		VN_RELE(vp);
		resp->status = NFS3ERR_ACCES;
		if (bverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.dir_wcc);
		return;
	}

	/*
	 * Filter out "." and "..".  There are some Linux clients that
	 * send this.  The local file system will be very unhappy if
	 * it gets one of these.
	 */
	if ((args->object.name[0] == '.') && ((args->object.name[1] == '\0') ||
		((args->object.name[1] == '.') &&
		(args->object.name[2] == '\0')))) {
			VN_RELE(vp);
			resp->status = NFS3ERR_ACCES;
			if (bverror) {
				resp->resfail.dir_wcc.before.attributes = FALSE;
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else
				vattr_to_wcc_data(&bva, &bva,
					&resp->resfail.dir_wcc);
			return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(vp);
		resp->status = NFS3ERR_ROFS;
		if (bverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			resp->resfail.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.dir_wcc);
		return;
	}

	VOP_RMDIR(vp, args->object.name, rootdir, cr, error);

	ava.va_mask = AT_NFS;
	VOP_GETATTR(vp, &ava, 0, cr, averror);

	VN_RELE(vp);

	if (error) {
		resp->status = puterrno3(error);
		if (bverror) {
			resp->resfail.dir_wcc.before.attributes = FALSE;
			if (averror)
				resp->resfail.dir_wcc.after.attributes = FALSE;
			else {
				resp->resfail.dir_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&ava,
					    &resp->resfail.dir_wcc.after.attr);
			}
		} else {
			if (averror) {
				resp->resfail.dir_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&bva,
					&resp->resfail.dir_wcc.before.attr);
				resp->resfail.dir_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&bva, &ava,
						&resp->resfail.dir_wcc);
			}
		}
		return;
	}

	resp->status = NFS3_OK;
	if (bverror) {
		resp->resok.dir_wcc.before.attributes = FALSE;
		if (averror)
			resp->resok.dir_wcc.after.attributes = FALSE;
		else {
			resp->resok.dir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&ava, &resp->resok.dir_wcc.after.attr);
		}
	} else {
		if (averror) {
			resp->resok.dir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&bva,
					&resp->resok.dir_wcc.before.attr);
			resp->resok.dir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &ava, &resp->resok.dir_wcc);
	}
}

static void
rfs3_rename(RENAME3args *args, RENAME3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int fbverror;
	int faverror;
	int tbverror;
	int taverror;
	vnode_t *fvp;
	vnode_t *tvp;
	struct vattr fbva;
	struct vattr fava;
	struct vattr tbva;
	struct vattr tava;
	struct exportinfo *texi;

	fvp = fh3tovp(&args->from.dir, exi);
	if (fvp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.fromdir_wcc.before.attributes = FALSE;
		resp->resfail.fromdir_wcc.after.attributes = FALSE;
		resp->resfail.todir_wcc.before.attributes = FALSE;
		resp->resfail.todir_wcc.after.attributes = FALSE;
		goto freepath;
	}

	fbva.va_mask = AT_NFS;
	VOP_GETATTR(fvp, &fbva, 0, cr, fbverror);

	texi = findexport(&args->to.dir.fh3_fsid,
			(struct fid *) &args->to.dir.fh3_xlen);
	tvp = fh3tovp(&args->to.dir, texi);
	if (texi != NULL) {
		EXPORTED_MRUNLOCK();
	}

	if (tvp == NULL) {
		VN_RELE(fvp);
		resp->status = NFS3ERR_STALE;
		if (fbverror) {
			resp->resfail.fromdir_wcc.before.attributes = FALSE;
			resp->resfail.fromdir_wcc.after.attributes = FALSE;
			resp->resfail.todir_wcc.before.attributes = FALSE;
			resp->resfail.todir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&fbva, &fbva,
					&resp->resfail.fromdir_wcc);
			resp->resfail.todir_wcc.before.attributes = FALSE;
			resp->resfail.todir_wcc.after.attributes = FALSE;
		}
		goto freepath;
	}

	/*
	 * RENAME is only valid within a file system.
	 * We do not even handle nohide cases: let the client handle them.
	 */
	if (texi != exi) {
#ifdef DEBUG
		printf("rfs3_rename, trying to cross file system\n");
#endif
		VN_RELE(fvp);
		VN_RELE(tvp);
		resp->status = NFS3ERR_XDEV;
		if (fbverror) {
			resp->resfail.fromdir_wcc.before.attributes = FALSE;
			resp->resfail.fromdir_wcc.after.attributes = FALSE;
			resp->resfail.todir_wcc.before.attributes = FALSE;
			resp->resfail.todir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&fbva, &fbva,
				&resp->resfail.fromdir_wcc);
			resp->resfail.todir_wcc.before.attributes = FALSE;
			resp->resfail.todir_wcc.after.attributes = FALSE;
		}
		goto freepath;
	}

	tbva.va_mask = AT_NFS;
	VOP_GETATTR(tvp, &tbva, 0, cr, tbverror);

	if (fvp->v_type != VDIR || tvp->v_type != VDIR) {
		VN_RELE(fvp);
		VN_RELE(tvp);
		resp->status = NFS3ERR_NOTDIR;
		if (fbverror) {
			resp->resfail.fromdir_wcc.before.attributes = FALSE;
			resp->resfail.fromdir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&fbva, &fbva,
					&resp->resfail.fromdir_wcc);
		}
		if (tbverror) {
			resp->resfail.todir_wcc.before.attributes = FALSE;
			resp->resfail.todir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&tbva, &tbva,
					&resp->resfail.todir_wcc);
		}
		goto freepath;
	}

	if (args->from.name == NULL || *(args->from.name) == '\0' ||
	    !sane(args->to.name)) { 
		VN_RELE(fvp);
		VN_RELE(tvp);
		resp->status = NFS3ERR_ACCES;
		if (fbverror) {
			resp->resfail.fromdir_wcc.before.attributes = FALSE;
			resp->resfail.fromdir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&fbva, &fbva,
					&resp->resfail.fromdir_wcc);
		}
		if (tbverror) {
			resp->resfail.todir_wcc.before.attributes = FALSE;
			resp->resfail.todir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&tbva, &tbva,
					&resp->resfail.todir_wcc);
		}
		goto freepath;
	}

	/*
	 * Do not need to check texi explicitly because it must be the
	 * same as exi.  If we change the server to handle nohide renames
	 * across filesystems, then we need to explicitly check texi.
	 */
	if (rdonly(exi, req)) {
		VN_RELE(fvp);
		VN_RELE(tvp);
		resp->status = NFS3ERR_ROFS;
		if (fbverror) {
			resp->resfail.fromdir_wcc.before.attributes = FALSE;
			resp->resfail.fromdir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&fbva, &fbva,
					&resp->resfail.fromdir_wcc);
		}
		if (tbverror) {
			resp->resfail.todir_wcc.before.attributes = FALSE;
			resp->resfail.todir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&tbva, &tbva,
					&resp->resfail.todir_wcc);
		}
		goto freepath;
	}

	VOP_RENAME(fvp, args->from.name, tvp, args->to.name, (struct pathname *) 0, cr, error);

	/*
	 *  Must explicitly post imon events because imon-vnodeop
	 *  layer will never see it if tdvp is monitored but fdvp
	 *  isn't.
	 */
	if (error == 0) {
		struct vnode *filevp;
		int lookuperr;

		IMON_EVENT(tvp, cr, IMON_CONTENT);
		VOP_LOOKUP(tvp, args->to.name, &filevp,	
			(struct pathname *)NULL, 0, curuthread->ut_rdir,
			cr, lookuperr);
		if (!lookuperr) {
			IMON_EVENT(filevp, cr, IMON_RENAME);
			VN_RELE(filevp);
		}
	}

	fava.va_mask = AT_NFS;
	VOP_GETATTR(fvp, &fava, 0, cr, faverror);

	tava.va_mask = AT_NFS;
	VOP_GETATTR(tvp, &tava, 0, cr, taverror);

	VN_RELE(fvp);
	VN_RELE(tvp);

	if (error) {
		resp->status = puterrno3(error);
		if (fbverror) {
			resp->resfail.fromdir_wcc.before.attributes = FALSE;
			if (faverror) {
				resp->resfail.fromdir_wcc.after.attributes =
									FALSE;
			} else {
				resp->resfail.fromdir_wcc.after.attributes =
									TRUE;
				vattr_to_fattr3(&fava,
					&resp->resfail.fromdir_wcc.after.attr);
			}
		} else {
			if (faverror) {
				resp->resfail.fromdir_wcc.before.attributes =
									TRUE;
				vattr_to_wcc_attr(&fbva,
					&resp->resfail.fromdir_wcc.before.attr);
				resp->resfail.fromdir_wcc.after.attributes =
									FALSE;
			} else {
				vattr_to_wcc_data(&fbva, &fava,
						&resp->resfail.fromdir_wcc);
			}
		}
		if (tbverror) {
			resp->resfail.todir_wcc.before.attributes = FALSE;
			if (taverror) {
				resp->resfail.todir_wcc.after.attributes =
									FALSE;
			} else {
				resp->resfail.todir_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&tava,
					&resp->resfail.todir_wcc.after.attr);
			}
		} else {
			if (taverror) {
				resp->resfail.todir_wcc.before.attributes =
									TRUE;
				vattr_to_wcc_attr(&tbva,
					&resp->resfail.todir_wcc.before.attr);
				resp->resfail.todir_wcc.after.attributes =
									FALSE;
			} else {
				vattr_to_wcc_data(&tbva, &tava,
						&resp->resfail.todir_wcc);
			}
		}
		goto freepath;
	}

	resp->status = NFS3_OK;
	if (fbverror) {
		resp->resok.fromdir_wcc.before.attributes = FALSE;
		if (faverror)
			resp->resok.fromdir_wcc.after.attributes = FALSE;
		else {
			resp->resok.fromdir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&fava,
					&resp->resok.fromdir_wcc.after.attr);
		}
	} else {
		if (faverror) {
			resp->resok.fromdir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&fbva,
					&resp->resok.fromdir_wcc.before.attr);
			resp->resok.fromdir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&fbva, &fava,
					&resp->resok.fromdir_wcc);
		}
	}
	if (tbverror) {
		resp->resok.todir_wcc.before.attributes = FALSE;
		if (taverror)
			resp->resok.todir_wcc.after.attributes = FALSE;
		else {
			resp->resok.todir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&tava,
					&resp->resok.todir_wcc.after.attr);
		}
	} else {
		if (taverror) {
			resp->resok.todir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&tbva,
					&resp->resok.todir_wcc.before.attr);
			resp->resok.todir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&tbva, &tava, &resp->resok.todir_wcc);
	}
    freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->from.name);
	kmem_zone_free(nfs_xdr_string_zone, args->to.name);
}

static void
rfs3_link(LINK3args *args, LINK3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int verror;
	int bverror;
	int averror;
	vnode_t *vp;
	vnode_t *dvp;
	struct vattr va;
	struct vattr bva;
	struct vattr ava;
	struct exportinfo *dexi;

	vp = fh3tovp(&args->file, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.file_attributes.attributes = FALSE;
		resp->resfail.linkdir_wcc.before.attributes = FALSE;
		resp->resfail.linkdir_wcc.after.attributes = FALSE;
		goto freepath;
	}

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	dexi = findexport(&args->link.dir.fh3_fsid,
			(struct fid *) &args->link.dir.fh3_xlen);

	dvp = fh3tovp(&args->link.dir, dexi);
	if (dvp == NULL) {
		VN_RELE(vp);
		if (dexi != NULL)
			EXPORTED_MRUNLOCK();
		resp->status = NFS3ERR_STALE;
		if (verror)
			resp->resfail.file_attributes.attributes = FALSE;
		else {
			resp->resfail.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.file_attributes.attr);
		}
		resp->resfail.linkdir_wcc.before.attributes = FALSE;
		resp->resfail.linkdir_wcc.after.attributes = FALSE;
		goto freepath;
	}

	bva.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &bva, 0, cr, bverror);

	if (dvp->v_type != VDIR) {
		VN_RELE(vp);
		VN_RELE(dvp);
		if (dexi != NULL)
			EXPORTED_MRUNLOCK();
		resp->status = NFS3ERR_NOTDIR;
		if (verror)
			resp->resfail.file_attributes.attributes = FALSE;
		else {
			resp->resfail.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.file_attributes.attr);
		}
		if (bverror) {
			resp->resfail.linkdir_wcc.before.attributes = FALSE;
			resp->resfail.linkdir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&bva, &bva,
					&resp->resfail.linkdir_wcc);
		}
		goto freepath;
	}

	if (!sane(args->link.name)) {
		VN_RELE(vp);
		VN_RELE(dvp);
		if (dexi != NULL)
			EXPORTED_MRUNLOCK();
		resp->status = NFS3ERR_ACCES;
		if (verror)
			resp->resfail.file_attributes.attributes = FALSE;
		else {
			resp->resfail.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.file_attributes.attr);
		}
		if (bverror) {
			resp->resfail.linkdir_wcc.before.attributes = FALSE;
			resp->resfail.linkdir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&bva, &bva,
					&resp->resfail.linkdir_wcc);
		}
		goto freepath;
	}

	if (rdonly(exi, req) || rdonly(dexi, req)) {
		VN_RELE(vp);
		VN_RELE(dvp);
		if (dexi != NULL)
			EXPORTED_MRUNLOCK();
		resp->status = NFS3ERR_ROFS;
		if (verror)
			resp->resfail.file_attributes.attributes = FALSE;
		else {
			resp->resfail.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.file_attributes.attr);
		}
		if (bverror) {
			resp->resfail.linkdir_wcc.before.attributes = FALSE;
			resp->resfail.linkdir_wcc.after.attributes = FALSE;
		} else {
			vattr_to_wcc_data(&bva, &bva,
					&resp->resfail.linkdir_wcc);
		}
		goto freepath;
	}

	VOP_LINK(dvp, vp, args->link.name, cr, error);

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	ava.va_mask = AT_NFS;
	VOP_GETATTR(dvp, &ava, 0, cr, averror);

	VN_RELE(vp);
	VN_RELE(dvp);

	if (dexi)
		EXPORTED_MRUNLOCK();

	if (error) {
		resp->status = puterrno3(error);
		if (verror)
			resp->resfail.file_attributes.attributes = FALSE;
		else {
			resp->resfail.file_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.file_attributes.attr);
		}
		if (bverror) {
			resp->resfail.linkdir_wcc.before.attributes = FALSE;
			if (averror) {
				resp->resfail.linkdir_wcc.after.attributes =
									FALSE;
			} else {
				resp->resfail.linkdir_wcc.after.attributes =
									TRUE;
				vattr_to_fattr3(&va,
					&resp->resfail.linkdir_wcc.after.attr);
			}
		} else {
			if (averror) {
				resp->resfail.linkdir_wcc.before.attributes =
									TRUE;
				vattr_to_wcc_attr(&bva,
					&resp->resfail.linkdir_wcc.before.attr);
				resp->resfail.linkdir_wcc.after.attributes =
									FALSE;
			} else {
				vattr_to_wcc_data(&bva, &ava,
						&resp->resfail.linkdir_wcc);
			}
		}
		goto freepath;
	}

	resp->status = NFS3_OK;
	if (verror)
		resp->resok.file_attributes.attributes = FALSE;
	else {
		resp->resok.file_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.file_attributes.attr);
	}
	if (bverror) {
		resp->resok.linkdir_wcc.before.attributes = FALSE;
		if (averror)
			resp->resok.linkdir_wcc.after.attributes = FALSE;
		else {
			resp->resok.linkdir_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resok.linkdir_wcc.after.attr);
		}
	} else {
		if (averror) {
			resp->resok.linkdir_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&bva,
					&resp->resok.linkdir_wcc.before.attr);
			resp->resok.linkdir_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &ava, &resp->resok.linkdir_wcc);
	}
    freepath:
	kmem_zone_free(nfs_xdr_string_zone, args->link.name);
}

#ifdef nextdp
#undef nextdp
#endif
#define	nextdp(dp)	((struct dirent *)((char *)(dp) + (dp)->d_reclen))
#ifdef DEBUG
int testreaddir = 0;
#endif /* DEBUG */

/* ARGSUSED */
static void
rfs3_readdir(READDIR3args *args, READDIR3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int verror;
	vnode_t *vp;
	struct vattr va;
	struct iovec iov;
	struct uio uio;
	char *data;
	int iseof;

	vp = fh3tovp(&args->dir, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_attributes.attributes = FALSE;
		return;
	}

	VOP_RWLOCK(vp, VRWLOCK_READ);

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	if (vp->v_type != VDIR) {
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		VN_RELE(vp);
		resp->status = NFS3ERR_NOTDIR;
		if (verror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

	VOP_ACCESS(vp, VREAD, cr, error);
	if (error) {
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		VN_RELE(vp);
		resp->status = puterrno3(error);
		if (verror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

	if (args->count < sizeof (struct dirent)) {
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		VN_RELE(vp);
		resp->status = NFS3ERR_TOOSMALL;
		if (verror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}
	if (args->count > rfs3_tsize(req->rq_xprt))
		args->count = rfs3_tsize(req->rq_xprt);

	data = (char *)kmem_alloc(args->count, KM_SLEEP);

	iov.iov_base = data;
	iov.iov_len = args->count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_segflg = UIO_SYSSPACE;
	if ((args->cookie & 0xffffffff00000000LL) == 0xffffffff00000000LL) {
		/* 
		 * getting this cookie from an old buggy client
		 * we need to strip off the 0xfffffff like we
		 * used to
		 */
		uio.uio_offset = (uint32_t) args->cookie;
	} else {
		uio.uio_offset = args->cookie;
	}
	uio.uio_resid = args->count;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;

	VOP_READDIR(vp, &uio, cr, &iseof, error);

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	VOP_RWUNLOCK(vp, VRWLOCK_READ);
	VN_RELE(vp);

	if (error) {
		kmem_free((caddr_t)data, args->count);
		resp->status = puterrno3(error);
		if (verror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

	if (args->count == uio.uio_resid && !iseof) {
		kmem_free((caddr_t)data, args->count);
		resp->status = NFS3ERR_TOOSMALL;
		if (verror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

	resp->status = NFS3_OK;
	if (verror)
		resp->resok.dir_attributes.attributes = FALSE;
	else {
		resp->resok.dir_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.dir_attributes.attr);
	}
	bzero(resp->resok.cookieverf, sizeof (cookieverf3));
	resp->resok.reply.entries = (entry3 *)data;
	resp->resok.reply.eof = iseof;
	resp->resok.size = args->count - uio.uio_resid;
	resp->resok.count = args->count;
	/*  THIS IS TO MAKE READDIR WORK WITH 32 BIT CLIENTS */
	if (exi->exi_export.ex_flags & EX_B32CLNT) {
		int size = resp->resok.size;
		struct dirent *dp = (struct dirent *)data;
		/*
		 * lop off the high order 32 bits of the dirent cookie if 
		 * the 32 bit export option is specified.  I would rather 
		 * do it in xdr, but we don't know from filesystem at 
		 * that point
		 */

		while (size > 0) {
			size -= dp->d_reclen;
			dp->d_off = (cookie3)(dp->d_off & 0x00000000ffffffff);
			dp = nextdp(dp);
		}
	}
}

static void
rfs3_readdir_free(READDIR3res *resp)
{

	if (resp->status == NFS3_OK)
		kmem_free((caddr_t)resp->resok.reply.entries,
			resp->resok.count);
}

#define MIN_NAME_LENGTH	RNDUP(1)

/* Assumes encoded and decoded sizes are close enough */
#define ENTRY_SIZE(nl)	(int) (sizeof(bool_t) + \
			sizeof(fileid3) + \
			(nl) + \
			sizeof(cookie3) + \
			sizeof(post_op_attr) + \
			sizeof(post_op_fh3))

/* ARGSUSED */
static void
rfs3_readdirplus(READDIRPLUS3args *args, READDIRPLUS3res *resp,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	int verror;
	vnode_t *vp;
	struct vattr va;
	struct iovec iov;
	struct uio uio;
	char *data;
	int iseof;
	struct dirent *dp;
	int size;
	vnode_t *nvp;
	struct vattr nva;
	post_op_attr *atp;
	post_op_fh3 *fhp;
	int nents;
	int b32clnt;
	int datalen;
	int remainder;
	char *datap;
	off_t next_offset;
	long namelen_sum = 0;
	int avg_len;

#ifdef DEBUG
	if (testreaddir) {
		resp->status = NFS3ERR_NOTSUPP;
		resp->resfail.dir_attributes.attributes = FALSE;
		return;
	}
#endif /* DEBUG */

	vp = fh3tovp(&args->dir, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.dir_attributes.attributes = FALSE;
		return;
	}

	VOP_RWLOCK(vp, VRWLOCK_READ);

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	if (vp->v_type != VDIR) {
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		VN_RELE(vp);
		resp->status = NFS3ERR_NOTDIR;
		if (verror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

	VOP_ACCESS(vp, VREAD, cr, error);
	if (error) {
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		VN_RELE(vp);
		resp->status = puterrno3(error);
		if (verror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

	if (args->dircount < sizeof (struct dirent)) {
		VOP_RWUNLOCK(vp, VRWLOCK_READ);
		VN_RELE(vp);
		resp->status = NFS3ERR_TOOSMALL;
		if (verror)
			resp->resfail.dir_attributes.attributes = FALSE;
		else {
			resp->resfail.dir_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
		}
		return;
	}

	/*
	 * remainder will keep track of how much data space is left
	 * in the reply.  This accounts for the directory entry data
	 * as well as the attributes and the file handle.  It is
	 * assumed in the calculation of remainder that all entries will
	 * have attributes and a file handle.
	 * maxcount is the maximum size of the reply.  We must not exceed
	 * this.  In the XDR reply function, we will ensure that this
	 * value is not exceeded, but here we want to make sure that
	 * we do not do more work (reading directory entries and getting
	 * file attributes and file handles) than is necessary.
	 *
	 * We start of accounting for the stuff in the reply other than
	 * the directory entry list.
	 */
	remainder = args->maxcount - sizeof(post_op_attr) -
			sizeof(cookieverf3) - 2 * sizeof(bool_t);

	/*
	 * datap points to the current location in the directory data
	 * buffer where data can be read in from the local file system.
	 * nents will keep track of the total number of entries
	 * retrieved.
	 * next_offset is the offset of the next directory entry to
	 * be retrieved from the local file system.  This starts with
	 * the cookie specified in the RPC arguments.
	 *
	 * dircount tells us how big the user's buffer is on the client
	 * (or how much space the client has allocated for the directory
	 * entries, not counting the attributes and file handles).  We
	 * will not read more entries than will fit in this buffer.
	 */
	datap = data = (char *)kmem_alloc(args->dircount, KM_SLEEP);
	nents = 0;
	if ((args->cookie & 0xffffffff00000000LL) ==
	    0xffffffff00000000LL) {
		/*
		 * getting this cookie from an old buggy client
		 * we need to strip off the 0xfffffff like we
		 * used to
		 */
		next_offset = (uint32_t) args->cookie;
	} else {
		next_offset = args->cookie;
	}
	/*
	 * avg_len is the agerage file name length calculated based on
	 * the entries read in.  We start off with the smallest length
	 * returnable in the RPC reply.  We use this so that will will
	 * avoid reading more entries from the local file system
	 * than we will need.  We want to avoid over-reading the
	 * directory entries because with large directories (starting
	 * somewhere between 1500 and 3000 entries and going up), reading
	 * the directory is expensive.  If we scoop up 5000 entries
	 * and only send 160 or so in the reply, the wasted time adds
	 * up.
	 */
	avg_len = MIN_NAME_LENGTH;

	/*
	 * Scoop up the directory entries.  We will do this in multiple
	 * passes to ensure that we do not read more entries than we
	 * will need and to make efficient use of the space in the
	 * reply by returning as many entries as we can.  This keeps
	 * the number of RPCs to a minimum and does not incur extra
	 * overhead from reading directory entries we don't need
	 * and gettting attributes and file handles for files we won't
	 * be returning in the list.
	 * The directory reading loop will terminate when there is
	 * no room left in the directory entry buffer or there is no
	 * room left in the reply for any more directory entries or
	 * the local file system returns no entries on a readdir (either
	 * EOF or the buffer is not big enough).
	 */
	do {
		/*
		 * based on how much space is left in the directory entry
		 * buffer and how many more entries we estimate will fit
		 * in the reply, calculate the data length to be used
		 * on this directory reading pass.
		 */
		datalen = MIN(args->dircount - ((long)datap - (long)data),
			(remainder / ENTRY_SIZE(avg_len)) *
			DIRENTSIZE(avg_len));
		ASSERT(datalen > 0);
		ASSERT(datalen <= remainder);
		iov.iov_base = datap;
		iov.iov_len = datalen;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = next_offset;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_resid = datalen;
		uio.uio_pmp = NULL;
		uio.uio_pio = 0;
		uio.uio_readiolog = 0;
		uio.uio_writeiolog = 0;
		uio.uio_pbuf = 0;

		VOP_READDIR(vp, &uio, cr, &iseof, error);

		/*
		 * Ignore EINVAL errors.  EFS will give these when the next
		 * directory entry is too large for the buffer.  Return
		 * EINVAL if no entries are being returned to the client
		 * (i.e., the client's buffer is too small).
		 */
		if ((error && (error != EINVAL)) ||
		    ((error == EINVAL) && (nents == 0))) {
			VOP_RWUNLOCK(vp, VRWLOCK_READ);
			VN_RELE(vp);
			kmem_free((caddr_t)data, args->dircount);
			resp->status = puterrno3(error);
			if (verror)
				resp->resfail.dir_attributes.attributes = FALSE;
			else {
				resp->resfail.dir_attributes.attributes = TRUE;
				vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
			}
			return;
		}
		error = 0;

		if (uio.uio_resid == datalen) {
			/*
			 * If we've gotten some entries
			 * break out of the directory reading loop.
			 */
			if (nents != 0) {
				break;
			}
		toosmall:
			VOP_RWUNLOCK(vp, VRWLOCK_READ);
			VN_RELE(vp);
			kmem_free((caddr_t)data, args->dircount);
			resp->status = NFS3ERR_TOOSMALL;
			if (verror)
				resp->resfail.dir_attributes.attributes = FALSE;
			else {
				resp->resfail.dir_attributes.attributes = TRUE;
				vattr_to_fattr3(&va,
					&resp->resfail.dir_attributes.attr);
			}
			return;
		}

		size = datalen - uio.uio_resid;
		/*
		 * This is kludgey, but here is where we are going to lop
		 * off the high order 32 bits of the dirent cookie if the
		 * 32 bit export option is specified.  I would rather do it
		 * in xdr, but we don't know from filesystem at that point
		 */

		b32clnt = (exi->exi_export.ex_flags & EX_B32CLNT);

		dp = (struct dirent *)datap;
		while (size > 0) {
			remainder -= ENTRY_SIZE(strlen(dp->d_name));
			/*
			 * If there is no more room in the reply for
			 * this entry, set datap to be consistent with
			 * the entry count (so that datap - data holds
			 * exactly nents directory entries).
			 */
			if (remainder < 0) {
				datap = (char *)dp;
				iseof = 0;
				break;
			}
			namelen_sum += strlen(dp->d_name);
			nents++;
			size -= dp->d_reclen;
			next_offset = dp->d_off;
			if (b32clnt) {
				dp->d_off = (cookie3)(dp->d_off &
					0x00000000ffffffff);
			}
			dp = nextdp(dp);
		}
		if (!nents) {
			goto toosmall;
		}
		avg_len = (int)(namelen_sum / nents);
		datap = (char *)dp;
	} while ((remainder > ENTRY_SIZE(avg_len)) &&
		(args->dircount - DIRENTSIZE(avg_len) >
		((long)datap - (long)data)));

	/*
	 * We've finished all of the readdirs for this request, so get the
	 * post-op attributes and unlock the vnode.
	 */
	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	VOP_RWUNLOCK(vp, VRWLOCK_READ);

	atp = (post_op_attr *)kmem_alloc(nents * sizeof (*atp), KM_SLEEP);
	fhp = (post_op_fh3 *)kmem_alloc(nents * sizeof (*fhp), KM_SLEEP);

	resp->resok.attributes = atp;
	resp->resok.handles = fhp;

	for (size = (long)datap - (long)data, dp = (struct dirent *)data;
	    size > 0;
	    size -= dp->d_reclen, dp = nextdp(dp), atp++, fhp++) {
		if (dp->d_ino == 0) {
			atp->attributes = FALSE;
			fhp->handle_follows = FALSE;
			continue;
		}
		VOP_LOOKUP(vp, dp->d_name, &nvp,
				    (struct pathname *)NULL, 0,
				    (vnode_t *)NULL, cr, error);
		if (error) {
			atp->attributes = FALSE;
			fhp->handle_follows = FALSE;
			continue;
		}
		if (nvp->v_vfsmountedhere) {
			/*
			 * if vfsmounted here, whatever we have is false
			 * if that filesystem is exported -nohide.
			 * Rather than figure all this 
			 * out, we just return no attributes or handle.
			 */
			atp->attributes = FALSE;
			fhp->handle_follows = FALSE;
			VN_RELE(nvp);
			continue;
		}
		if (ISDOTDOT(dp->d_name) && (vp->v_flag & VROOT)) {
			atp->attributes = FALSE;
			fhp->handle_follows = FALSE;
			VN_RELE(nvp);
			continue;
		}
		nva.va_mask = AT_NFS;
		VOP_GETATTR(nvp, &nva, 0, cr, error);
		if (!error) {
			atp->attributes = TRUE;
			vattr_to_fattr3(&nva, &atp->attr);
		} else
			atp->attributes = FALSE;
		error = makefh3(&fhp->handle, nvp, exi);
		if (!error)
			fhp->handle_follows = TRUE;
		else
			fhp->handle_follows = FALSE;
		VN_RELE(nvp);
	}

	VN_RELE(vp);

	resp->status = NFS3_OK;
	if (verror)
		resp->resok.dir_attributes.attributes = FALSE;
	else {
		resp->resok.dir_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.dir_attributes.attr);
	}
	bzero(resp->resok.cookieverf, sizeof (cookieverf3));
	resp->resok.reply.entries = (entryplus3 *)data;
	resp->resok.reply.eof = iseof;
	resp->resok.size = nents;
	resp->resok.count = args->dircount;
	resp->resok.maxcount = args->maxcount;
}

static void
rfs3_readdirplus_free(READDIRPLUS3res *resp)
{

	if (resp->status == NFS3_OK) {
		kmem_free((caddr_t)resp->resok.reply.entries,
			  resp->resok.count);
		kmem_free((caddr_t)resp->resok.handles,
			  resp->resok.size * sizeof (post_op_fh3));
		kmem_free((caddr_t)resp->resok.attributes,
			  resp->resok.size * sizeof (post_op_attr));
	}
}

/* ARGSUSED */
static void
rfs3_fsstat(FSSTAT3args *args, FSSTAT3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int verror;
	vnode_t *vp;
	struct vattr va;
	struct statvfs sb;

	vp = fh3tovp(&args->fsroot, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	VFS_STATVFS(vp->v_vfsp, &sb, NULL, error);	/* XXX fix arg 3 */

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	VN_RELE(vp);

	if (error) {
		resp->status = puterrno3(error);
		if (verror)
			resp->resfail.obj_attributes.attributes = FALSE;
		else {
			resp->resfail.obj_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.obj_attributes.attr);
		}
		return;
	}

	resp->status = NFS3_OK;
	if (verror)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
	resp->resok.tbytes = (size3)sb.f_frsize * (size3)sb.f_blocks;
	resp->resok.fbytes = (size3)sb.f_frsize * (size3)sb.f_bfree;
	resp->resok.abytes = (size3)sb.f_frsize * (size3)sb.f_bavail;
	resp->resok.tfiles = (size3)sb.f_files;
	resp->resok.ffiles = (size3)sb.f_ffree;
	resp->resok.afiles = (size3)sb.f_favail;
	resp->resok.invarsec = 0;
}

/* ARGSUSED */
static void
rfs3_fsinfo(FSINFO3args *args, FSINFO3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	struct vattr va;

	vp = fh3tovp(&args->fsroot, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	resp->status = NFS3_OK;
	if (error)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
	resp->resok.rtmax = rfs3_tsize(req->rq_xprt);
	resp->resok.rtpref = rfs3_tsize(req->rq_xprt);
	resp->resok.wtmax = rfs3_tsize(req->rq_xprt);
	resp->resok.wtpref = rfs3_tsize(req->rq_xprt);
	resp->resok.rtmult = 512;
	resp->resok.wtmult = 512;
	/* JWS - These last and next two look bogus */
	resp->resok.dtpref = MAXBSIZE;
	resp->resok.maxfilesize = MAXOFF_T;
	resp->resok.time_delta.seconds = 0;
	resp->resok.time_delta.nseconds = 1000;
	resp->resok.properties = FSF3_LINK | FSF3_SYMLINK |
				FSF3_HOMOGENEOUS | FSF3_CANSETTIME;
}

/* ARGSUSED */
static void
rfs3_pathconf(PATHCONF3args *args, PATHCONF3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int verror;
	vnode_t *vp;
	struct vattr va;
	long val;

	vp = fh3tovp(&args->object, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	va.va_mask = AT_NFS;
	VOP_GETATTR(vp, &va, 0, cr, verror);

	VOP_PATHCONF(vp, _PC_LINK_MAX, &val, cr, error);
	if (error)
		goto errout;
	resp->resok.link_max = (uint32)val;

	VOP_PATHCONF(vp, _PC_NAME_MAX, &val, cr, error);
	if (error)
		goto errout;
	resp->resok.name_max = (uint32)val;

	VOP_PATHCONF(vp, _PC_NO_TRUNC, &val, cr, error);
	if (error)
		goto errout;
	if (val == 1)
		resp->resok.no_trunc = TRUE;
	else
		resp->resok.no_trunc = FALSE;

	VOP_PATHCONF(vp, _PC_CHOWN_RESTRICTED, &val, cr, error);
	if (error)
		goto errout;
	if (val == 1)
		resp->resok.chown_restricted = TRUE;
	else
		resp->resok.chown_restricted = FALSE;

	VN_RELE(vp);

	resp->status = NFS3_OK;
	if (verror)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
	resp->resok.case_insensitive = FALSE;
	resp->resok.case_preserving = TRUE;
	return;

errout:
	VN_RELE(vp);

	resp->status = puterrno3(error);
	if (verror)
		resp->resfail.obj_attributes.attributes = FALSE;
	else {
		resp->resfail.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resfail.obj_attributes.attr);
	}
}

static void
rfs3_commit(COMMIT3args *args, COMMIT3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	int bverror;
	int averror;
	vnode_t *vp;
	struct vattr bva;
	struct vattr ava;

	vp = fh3tovp(&args->file, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.file_wcc.before.attributes = FALSE;
		resp->resfail.file_wcc.after.attributes = FALSE;
		return;
	}

	bva.va_mask = AT_NFS;
	VOP_GETATTR(vp, &bva, 0, cr, bverror);

	if (vp->v_type != VREG || rdonly(exi, req)) {
		error = NFS3ERR_BADHANDLE;
	} else {
		if (args->count > 0) {
			VOP_FSYNC(vp, FSYNC_WAIT, cr, (off_t) args->offset,
			    (off_t) args->offset + args->count - 1, error);
		} else { /* args->count == 0 */	
			VOP_FSYNC(vp, FSYNC_WAIT, cr, (off_t) args->offset,
			    (off_t) -1, error);
		}
	}

	ava.va_mask = AT_NFS;
	VOP_GETATTR(vp, &ava, 0, cr, averror);

	VN_RELE(vp);

	if (error) {
		resp->status = puterrno3(error);
		if (bverror) {
			resp->resfail.file_wcc.before.attributes = FALSE;
			if (averror)
				resp->resfail.file_wcc.after.attributes = FALSE;
			else {
				resp->resfail.file_wcc.after.attributes = TRUE;
				vattr_to_fattr3(&ava,
					&resp->resfail.file_wcc.after.attr);
			}
		} else {
			if (averror) {
				resp->resfail.file_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&bva,
					&resp->resfail.file_wcc.before.attr);
				resp->resfail.file_wcc.after.attributes = FALSE;
			} else {
				vattr_to_wcc_data(&bva, &ava,
						&resp->resfail.file_wcc);
			}
		}
		return;
	}

	resp->status = NFS3_OK;

	if (bverror) {
		resp->resok.file_wcc.before.attributes = FALSE;
		if (averror)
			resp->resok.file_wcc.after.attributes = FALSE;
		else {
			resp->resok.file_wcc.after.attributes = TRUE;
			vattr_to_fattr3(&ava, &resp->resok.file_wcc.after.attr);
		}
	} else {
		if (averror) {
			resp->resok.file_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&bva,
					&resp->resok.file_wcc.before.attr);
			resp->resok.file_wcc.after.attributes = FALSE;
		} else
			vattr_to_wcc_data(&bva, &ava, &resp->resok.file_wcc);
	}
	bcopy(&nfs3_writeverf, &resp->resok.verf, sizeof (nfs3_writeverf));
}

static int
sattr3_to_vattr(struct sattr3 *sap, struct vattr *vap)
{
	timespec_t tv;

	vap->va_mask = 0;
	if (sap->mode.set_it) {
		vap->va_mode = (mode_t)sap->mode.mode;
		vap->va_mask |= AT_MODE;
	}
	if (sap->uid.set_it) {
		vap->va_uid = (uid_t)sap->uid.uid;
		vap->va_mask |= AT_UID;
	}
	if (sap->gid.set_it) {
		vap->va_gid = (gid_t)sap->gid.gid;
		vap->va_mask |= AT_GID;
	}
	if (sap->size.set_it) {
		if (sap->size.size > NFS3_MAX_FILE_OFFSET)
			return (EINVAL);
		vap->va_size = sap->size.size;
		vap->va_mask |= AT_SIZE;
	}
	if (sap->atime.set_it == SET_TO_CLIENT_TIME) {
		vap->va_atime.tv_sec = (time_t)sap->atime.atime.seconds;
		vap->va_atime.tv_nsec = (time_t)sap->atime.atime.nseconds;
		vap->va_mask |= AT_ATIME;
	} else if (sap->atime.set_it == SET_TO_SERVER_TIME) {
		nanotime_syscall(&tv);
		vap->va_atime.tv_sec = tv.tv_sec;
		vap->va_atime.tv_nsec = tv.tv_nsec;
		vap->va_mask |= AT_ATIME;
	}
	if (sap->mtime.set_it == SET_TO_CLIENT_TIME) {
		vap->va_mtime.tv_sec = (time_t)sap->mtime.mtime.seconds;
		vap->va_mtime.tv_nsec = (time_t)sap->mtime.mtime.nseconds;
		vap->va_mask |= AT_MTIME;
	} else if (sap->mtime.set_it == SET_TO_SERVER_TIME) {
		nanotime_syscall(&tv);
		vap->va_mtime.tv_sec = tv.tv_sec;
		vap->va_mtime.tv_nsec = tv.tv_nsec;
		vap->va_mask |= AT_MTIME;
	}

	return (0);
}

/*
 * Convert an nfs_fh3 into a vnode.
 * Uses the file id (fh_len + fh_data) in the file handle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 */
struct vnode *
fh3tovp(nfs_fh3 *fh, struct exportinfo *exi)
{
	register struct vfs *vfsp;
	struct vnode *vp;
	int error;
	fsid_t *fsidp;
	fid_t loopfid;
	fid_t *fidp;


	if (exi == NULL) {
		return (NULL);	/* not exported */
	}

	if (fh->fh3_length != 32) {
#ifdef DEBUG
		printf("fhtovp_end:(%s)\n", "bad length");
#endif
		return (NULL);
	}

	if (exi->exi_loopback) {
		/*
		 * For exported loopback mounts, use the fsid stored in
		 * exi (see fhtovp for more complete comment).
		 */
		fsidp = & exi->exi_fsid;
		fidp = &loopfid;
		loopfid.fid_len = fh->fh3_length;
		bzero(&fh->fh3_xlen, NFS_FHMAXDATA + sizeof(u_short));
		bcopy(fh->fh3_u.data, loopfid.fid_data, fh->fh3_length);
	} else {
		fsidp = &fh->fh3_fsid;
		fidp = (struct fid *)&(fh->fh3_len);
	}
	vfsp = exi->exi_vfsp;
	if (vfsp == NULL)
		vfsp = exi->exi_vfsp = getvfs(fsidp);
	if (vfsp == NULL) {
#ifdef DEBUG
		printf(
			"fhtovp_end:(%s) fh3_fsid 0x%x 0x%x\n",
			"getvfs NULL",
			fh->fh3_fsid.val[0], fh->fh3_fsid.val[1]);
#endif
		return (NULL);
	}
	VFS_VGET(vfsp, &vp, fidp, error);
	if (error || vp == NULL) {
#ifdef DEBUG
		printf(
			"fhtovp_end:(%s)\n", "VFS_GET failed or vp NULL");
#endif
		return (NULL);
	}
	return (vp);
}

#ifdef TRACE
static char *rfscallnames_v3[] = {
	"RFS3_NULL",
	"RFS3_GETATTR",
	"RFS3_SETATTR",
	"RFS3_LOOKUP",
	"RFS3_ACCESS",
	"RFS3_READLINK",
	"RFS3_READ",
	"RFS3_WRITE",
	"RFS3_CREATE",
	"RFS3_MKDIR",
	"RFS3_SYMLINK",
	"RFS3_MKNOD",
	"RFS3_REMOVE",
	"RFS3_RMDIR",
	"RFS3_RENAME",
	"RFS3_LINK",
	"RFS3_READDIR",
	"RFS3_READDIRPLUS",
	"RFS3_FSSTAT",
	"RFS3_FSINFO",
	"RFS3_PATHCONF",
	"RFS3_COMMIT"
};
#endif

struct rfsdisp rfsdisptab_v3[] = {
	/*
	 * VERSION 3
	 */
	
	/* RFS_NULL = 0 */
	{(void (*)())rfs_null, xdr_void, xdr_void, NULL, 0},

	/* RFS3_GETATTR = 1 */
	{rfs3_getattr, xdr_GETATTR3args, xdr_GETATTR3res,
	    nullfree, RFS_IDEMPOTENT|RFS_ALLOWANON},

	/* RFS3_SETATTR = 2 */
	{rfs3_setattr, xdr_SETATTR3args, xdr_SETATTR3res,
	    nullfree, 0},

	/* RFS3_LOOKUP = 3 */
	{rfs3_lookup, xdr_fast_3, xdr_LOOKUP3res,
	    nullfree, RFS_IDEMPOTENT},

	/* RFS3_ACCESS = 4 */
	{rfs3_access, xdr_ACCESS3args, xdr_ACCESS3res,
	    nullfree, RFS_IDEMPOTENT},

	/* RFS3_READLINK = 5 */
	{rfs3_readlink, xdr_READLINK3args, xdr_READLINK3res,
	    rfs3_readlink_free, RFS_IDEMPOTENT},

	/* RFS3_READ = 6 */
	{rfs3_read, xdr_READ3args, xdr_READ3res,
	    nullfree, RFS_IDEMPOTENT},

	/* RFS3_WRITE = 7 */
	{rfs3_write, xdr_WRITE3args, xdr_WRITE3res, nullfree, 0},

	/* RFS3_CREATE = 8 */
	{rfs3_create, xdr_CREATE3args, xdr_CREATE3res, nullfree, 0},

	/* RFS3_MKDIR = 9 */
	{rfs3_mkdir, xdr_MKDIR3args, xdr_MKDIR3res, nullfree, 0},

	/* RFS3_SYMLINK = 10 */
	{rfs3_symlink, xdr_SYMLINK3args, xdr_SYMLINK3res, nullfree, 0},

	/* RFS3_MKNOD = 11 */
	{rfs3_mknod, xdr_MKNOD3args, xdr_MKNOD3res, nullfree, 0},

	/* RFS3_REMOVE = 12 */
	{rfs3_remove, xdr_fast_3, xdr_REMOVE3res, nullfree, 0},

	/* RFS3_RMDIR = 13 */
	{rfs3_rmdir, xdr_fast_3, xdr_RMDIR3res, nullfree, 0},

	/* RFS3_RENAME = 14 */
	{rfs3_rename, xdr_RENAME3args, xdr_RENAME3res, nullfree, 0},

	/* RFS3_LINK = 15 */
	{rfs3_link, xdr_LINK3args, xdr_LINK3res, nullfree, 0},

	/* RFS3_READDIR = 16 */
	{rfs3_readdir, xdr_READDIR3args, xdr_READDIR3res,
	    rfs3_readdir_free, RFS_IDEMPOTENT},

	/* RFS3_READDIRPLUS = 17 */
	{rfs3_readdirplus, xdr_READDIRPLUS3args, xdr_READDIRPLUS3res,
	    rfs3_readdirplus_free, RFS_IDEMPOTENT},

	/* RFS3_FSSTAT = 18 */
	{rfs3_fsstat, xdr_FSSTAT3args, xdr_FSSTAT3res,
	    nullfree, RFS_IDEMPOTENT},

	/* RFS3_FSINFO = 19 */
	{rfs3_fsinfo, xdr_FSINFO3args, xdr_FSINFO3res,
	    nullfree, RFS_IDEMPOTENT|RFS_ALLOWANON},

	/* RFS3_PATHCONF = 20 */
	{rfs3_pathconf, xdr_PATHCONF3args, xdr_PATHCONF3res,
	    nullfree, RFS_IDEMPOTENT},

	/* RFS3_COMMIT = 21 */
	{rfs3_commit, xdr_COMMIT3args, xdr_COMMIT3res,
	    nullfree, RFS_IDEMPOTENT}
};

static struct rfs_disptable {
	int dis_nprocs;
	struct rfsdisp *dis_table;
} rfs_disptable[] = {
        {sizeof (rfsdisptab_v2) / sizeof (rfsdisptab_v2[0]),
	    rfsdisptab_v2},
	{sizeof (rfsdisptab_v3) / sizeof (rfsdisptab_v3[0]),
	    rfsdisptab_v3},
};

void
rfs_init()
{
	timespec_t now;
#ifdef DEBUG

	/*
	 * Squeeze the boot time down into 64 bits for use as a
	 * write verifier to detect lost data on server reboot.
	 */
	ASSERT( sizeof (nfs3_writeverf) == sizeof (writeverf3));
#endif
	nanotime(&now);
	nfs3_writeverf.seconds  = now.tv_sec;
	nfs3_writeverf.nseconds = now.tv_nsec;

	mutex_init(&nfsd_count_lock, MUTEX_DEFAULT, "nfsd_count");
	mrlock_init(&exported_lock, MRLOCK_DBLTRIPPABLE, "exported", -1);

	large_iovec_zone = kmem_zone_init(sizeof(struct iovec) * LARGE_IOV_CNT,
						"NFS iovec");
	rfs_pn_zone = kmem_zone_init(NFS_MAXPATHLEN, "rfspn");
}

/*
 * If nfs_portmon is set, then clients are required to use
 * privileged ports (ports < IPPORT_RESERVED) in order to get NFS services.
 */
extern int nfs_portmon;

int
rfs_dispatch(struct svc_req *req, XDR *xdrin, caddr_t args,
	     caddr_t res, struct pernfsq *p)
{
	register struct rfsdisp *disp;
	int error, oerror;
	struct exportinfo *exi = NULL;
	struct dupreq *dup;
	fhandle_t *fh;
	int version;
	int vers = req->rq_vers;
	int which = req->rq_proc;
	int ejukebox = 0;	/* True if V2 EAGAIN */

	error = 0;
	version = vers;
	if (version == NFS3_VERSION)
	    SVSTAT3.ncalls++;
	else
	    SVSTAT.ncalls++;
	if (vers < VERSIONMIN || vers > VERSIONMAX) {
#pragma mips_frequency_hint NEVER
		/* No printf because this can happen with V3 servers */
		error = NFS_RPC_VERS;
		disp = &rfs_disptable[0].dis_table[0];
		goto done;
	}
	vers -= VERSIONMIN;
	if (which < 0 || which >= rfs_disptable[vers].dis_nprocs) {
#pragma mips_frequency_hint NEVER
		error = NFS_RPC_PROC;
		disp = &rfs_disptable[vers].dis_table[0];
		printf("nfs_server: vers %d bad proc number (%d) max %d\n",
			vers, which, rfs_disptable[vers].dis_nprocs);
		goto done;
	}
	/*
	 * All NFS operations have at least one file handle as the
	 * first argument. So we do some common checking here.
	 * The ops that take a second file handle like rename
	 * need to check the second one themselves.
	 */
		
	if (version == NFS3_VERSION) {
		fh = &((nfs_fh3 *)args)->fh3_u.nfs_fh3_i.fh3_i;
		SVSTAT3.reqs[which]++;
	} else {
		fh = (fhandle_t *) args;
		SVSTAT.reqs[which]++;
	}

	disp = &rfs_disptable[vers].dis_table[which];

	/*
	 * Zero args struct and decode into it.
	 */
	bzero(args, (u_int)NFS_MAX_ARGS);
	if ( ! ( (xdrproc_ansi_t)(disp->dis_xdrargs)) (xdrin, args)) {
#pragma mips_frequency_hint NEVER
		error = NFS_RPC_DECODE;
		(void) printf("NFS%d server: bad args for proc %d\n", 
			      vers, which);
		disp = &rfs_disptable[vers].dis_table[0];
		goto done;
	}

	/*
	 * Find export information and check authentication,
	 * setting the credential if everything is ok.
	 */
	if (which != RFS_NULL) {
		int anon_ok;

		if ((disp->dis_flags & RFS_ALLOWANON) &&
		    eqfid((fid_t*)&fh->fh_len, (fid_t *)&fh->fh_xlen))
			anon_ok = 1;
		else 
			anon_ok = 0;

		exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (exi != NULL && !checkauth(exi, req, get_current_cred(),
					      anon_ok)) {
#pragma mips_frequency_hint NEVER
			error = NFS_RPC_AUTH;
			(void) printf(
"NFS server: weak authentication, source IP address=%s\n",
			inet_ntoa(req->rq_xprt->xp_raddr.sin_addr));
			goto done;
		}

	}

	/*
	 * Clear results, count operations.
	 */
	bzero(res, (u_int)NFS_MAX_RESULT);

	if (disp->dis_flags & RFS_IDEMPOTENT) {
		( *(dispatch_proc_t)disp->dis_proc)(args, res, exi, req,
						    get_current_cred());
	} else {
		dup = svckudp_dup(req, p, res);
		if (dup == (struct dupreq *)NULL) {
#pragma mips_frequency_hint NEVER
			/*
			 * drop all in-progress requests; no reply
			 */
			error = NFS_RPC_DUP;
		} else if ((dup->dr_status & DUP_DONE) == 0) {
			/*
			 * Call actual service routine!
			 */
			( *(dispatch_proc_t)disp->dis_proc)(args, res, exi,
						    req, get_current_cred());
			svckudp_dupdone(dup, req, p, res);
		} else if (version == NFS_VERSION &&
			    *((enum nfsstat *)(res)) == NFSERR_JUKEBOX) {
			svckudp_dupdone(dup, req, p, res);
			/* This is a non idempotent request, with DUP_DONE set
			 * in dr_status, and last gn_status == 
			 * NFSERR_JUKEBOX.  Hence, re-issue the version 2
			 * request.
			 */
			bzero(res, (u_int)NFS_MAX_RESULT);
			( *(dispatch_proc_t)disp->dis_proc)(args, res, exi,
						    req, get_current_cred());
			svckudp_dupdone(dup, req, p, res);
		}

	}

	if (version == NFS_VERSION &&
	    (*(enum nfsstat *)(res)) == NFSERR_JUKEBOX) {
		ejukebox = 1;
	}

	/*
	 * implicitly re-send old reply for finished non-idempotent req's
	 */

done:
	if (exi != NULL) {
		EXPORTED_MRUNLOCK();
	}
	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
	       NETEVENT_NFSUP, NETCNT_NULL, which + NETRES_RFSNULL);
	/*
	 * Serialize and send results!
	 *
	 *	Do not send a reply if error NFSERR_JUKEBOX ( == EAGAIN) is
	 *	contained in the reply structure status.   This causes the
	 *	version 2 client to time out and retry the request.
	 *
	 *	This is done to support DMAPI applications like an HSM.  An
	 *	HSM might cause a read request, for example, to take a long
	 *	time.  So the nfs server is changed to set the FNONBLOCK
	 *	flag, causing the file system to return NFSERR_JUKEBOX
	 *	( == EAGAIN) if the request will take a while.
	 *
	 *	For NFS version 3, a request which might take a while will
	 *	return NFS3ERR_JUKEBOX in the status of the reply, and the
	 *	version 3 client will deal with that.
	 */
	oerror = error;
	if (error >=0 && ejukebox == 0) {
		error = nfs_sendreply(req->rq_xprt, disp->dis_xdrres, res, error,
			NFS_VERSION, NFS3_VERSION);
		if (error) {
#pragma mips_frequency_hint NEVER
			(void) printf("NFS server: reply error %d\n", error);
			if (version == NFS3_VERSION)
			    SVSTAT3.nbadcalls++;
			else
			    SVSTAT.nbadcalls++;
		}
	} else {
		krpc_toss(req->rq_xprt);
	}

	/*
	 * Free results struct (readlink, read, and readdir ref counts)
	 */
	if (oerror != NFS_RPC_AUTH && disp->dis_resfree != NULL) {
		((free_proc_t) disp->dis_resfree) (res);
	}
	return(0);
}

int
vattr_to_nattr(vap, na)
	register struct vattr *vap;
	register struct nfsfattr *na;
{
	long mask = vap->va_mask;
	ASSERT(mask & AT_TYPE);
	na->na_type = vtype_to_ntype(vap->va_type);
	na->na_mode = (mask & AT_MODE) ? vap->va_mode : -1;
	switch (na->na_type) {
	case NFREG:
		na->na_mode |= S_IFREG;
		break;
	case NFDIR:
		na->na_mode |= S_IFDIR;
		break;
	case NFBLK:
		na->na_mode |= S_IFBLK;
		break;
	case NFCHR:
		na->na_mode |= S_IFCHR;
		break;
	case NFLNK:
		na->na_mode |= S_IFLNK;
		break;
	case NFSOCK:
		na->na_mode |= S_IFSOCK;
		break;
	}
	na->na_uid = (mask & AT_UID) ? vap->va_uid : -1;
	na->na_gid = (mask & AT_GID) ? vap->va_gid : -1;
	na->na_fsid = (mask & AT_FSID) ? vap->va_fsid : -1;
	na->na_nodeid = (mask & AT_NODEID) ? vap->va_nodeid : -1;
	na->na_nlink = (mask & AT_NLINK) ? vap->va_nlink : -1;
	/*
	 * If the size will not fit in the NFS2 attribute structure,
	 * then fail the entire attribute fetch.  This will prevent
	 * NFS2 clients from messing with large files that they can't
	 * understand.
	 */
	if (vap->va_size > (off_t)INT_MAX) {
#pragma mips_frequency_hint NEVER
		return EOVERFLOW;
	} else {
		na->na_size = (mask & AT_SIZE) ? vap->va_size : -1;
	}
	if (mask & AT_ATIME) {
		na->na_atime.tv_sec  = vap->va_atime.tv_sec;
		na->na_atime.tv_usec = vap->va_atime.tv_nsec / 1000;
	} else {
		na->na_atime.tv_sec = na->na_atime.tv_usec = -1;
	}
	if (mask & AT_MTIME) {
		na->na_mtime.tv_sec  = vap->va_mtime.tv_sec;
		na->na_mtime.tv_usec = vap->va_mtime.tv_nsec / 1000;
	} else {
		na->na_mtime.tv_sec = na->na_mtime.tv_usec = -1;
	}
	if (mask & AT_CTIME) {
		na->na_ctime.tv_sec  = vap->va_ctime.tv_sec;
		na->na_ctime.tv_usec = vap->va_ctime.tv_nsec / 1000;
	} else {
		na->na_ctime.tv_sec = na->na_ctime.tv_usec = -1;
	}
	if (mask & AT_RDEV) {
		na->na_rdev = nfs_cmpdev(vap->va_rdev);
	} else {
		na->na_rdev = -1;
	}
	na->na_blocks = (mask & AT_NBLOCKS) ? vap->va_nblocks : -1;
	na->na_blocksize = (mask & AT_BLKSIZE) ? vap->va_blksize : -1;

	/*
	 * This bit of ugliness is a *TEMPORARY* hack to preserve the
	 * over-the-wire protocols for named-pipe vnodes.  It remaps the
	 * VFIFO type to the special over-the-wire type. (see note in nfs.h)
	 *
	 * BUYER BEWARE:
	 *  If you are porting the NFS to a non-SUN server, you probably
	 *  don't want to include the following block of code.  The
	 *  over-the-wire special file types will be changing with the
	 *  NFS Protocol Revision.
	 */
	if (vap->va_type == VFIFO)
		NA_SETFIFO(na);
	return 0;
}

static void
nullfree(void)
{
}

/*
 * Wire attributes to vnode attributes.
 * NOTE: All fields need to be copied regardless of the field value.
 *	Special values may be coming over the wire as hacks.  In the
 *	case of VFIFO, VCHR with size == -1 is sent.
 */
void
sattr_to_vattr(sa, vap)
	register struct nfssattr *sa;
	register struct vattr *vap;
{
	long mask;

	mask = 0;
	vap->va_mode = sa->sa_mode;
	vap->va_uid = (uid_t)sa->sa_uid;
	vap->va_gid = (gid_t)sa->sa_gid;
	vap->va_size = sa->sa_size;
	vap->va_atime.tv_sec  = sa->sa_atime.tv_sec;
	vap->va_atime.tv_nsec = sa->sa_atime.tv_usec * 1000;
	vap->va_mtime.tv_sec  = sa->sa_mtime.tv_sec;
	vap->va_mtime.tv_nsec = sa->sa_mtime.tv_usec * 1000;

	if ((sa->sa_mode != (u_int) -1) && (sa->sa_mode != (u_short) -1))
		mask |= AT_MODE;
	if (sa->sa_uid != (u_int) -1)
		mask |= AT_UID;
	if (sa->sa_gid != (u_int) -1)
		mask |= AT_GID;
	if (sa->sa_size != (u_int) -1)
		mask |= AT_SIZE;
	if (sa->sa_atime.tv_sec != (u_int) -1)
		mask |= AT_ATIME;
	if (sa->sa_mtime.tv_sec != (u_int) -1)
		mask |= AT_MTIME;
	vap->va_mask = mask;
}

/*
 * Convert an fhandle into a vnode.
 * Uses the file id (fh_len + fh_data) in the fhandle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 */
struct vnode *
fhtovp(fh, exi)
	fhandle_t *fh;
	struct exportinfo *exi;
{
	register struct vfs *vfsp;
	struct vnode *vp;
	int error;
	fsid_t *fsidp;
	fid_t loopfid;
	fid_t *fidp;

	if (exi == NULL) {
		return (NULL);	/* not exported */
	}

	if ( exi->exi_loopback ) {
		/*
		 * For exported loopback mounts, use the fsid stored in the exportinfo
		 * structure to get the vfs pointer.  Otherwise, use the fsid in the
		 * file handle.
		 * Also construct a new fid with the file handle in fid_data.
		 * There is an ugly side-effect operation going on here too.  We
		 * blot out the part of the file handle passed in by the caller
		 * which contains the export fid.
		 */
		fsidp = &exi->exi_fsid;
		fidp = &loopfid;
		loopfid.fid_len = sizeof(fhandle_t);
		bzero(&fh->fh_xlen, NFS_FHMAXDATA + sizeof(u_short));
		bcopy(fh, loopfid.fid_data, sizeof(fhandle_t));
	} else {
		fsidp = &fh->fh_fsid;
		fidp = (struct fid *)&(fh->fh_len);
	}
	vfsp = exi->exi_vfsp;
	if (vfsp == NULL)
		vfsp = exi->exi_vfsp = getvfs(fsidp);
	if (vfsp == NULL)
		return (NULL);
	VFS_VGET(vfsp, &vp, fidp, error);
	if (error || vp == NULL) {
#pragma mips_frequency_hint NEVER
		return (NULL);
	}
	return (vp);
}

/*
 * Determine if two addresses are equal
 * Only AF_INET supported for now
 */
eqaddr(addr1, addr2)
	struct sockaddr *addr1;
	struct sockaddr *addr2;
{

	if (addr1->sa_family != addr2->sa_family) {
#pragma mips_frequency_hint NEVER
		return (0);
	}
	switch (addr1->sa_family) {
	case AF_INET:
		return (((struct sockaddr_in *) addr1)->sin_addr.s_addr ==
			((struct sockaddr_in *) addr2)->sin_addr.s_addr);
	}
	return (0);
}

hostintable(sa, ht)
	struct sockaddr *sa;
	struct exaddrhashtable *ht;
{
	struct sockaddr **end, **hash;

	if (ht->table == NULL)
		return (0);
	end = ht->endtable;
	hash = exaddrhash(ht, sa);
	while (*hash != NULL) {
		if (eqaddr(*hash, sa)) {
			return (1);
		}
		if (++hash == end) {
			hash = ht->table;
		}
	}
	return (0);
}

/*
 * Process the RPC credentials.
 * So far only "Unix" style is supported.
 */
int
checkauth( struct exportinfo *exi, struct svc_req *req,
	   register struct cred *cred, int anon_ok)
{
	int flavor, i;
	register u_int *decode;

	/*
	 * Check for privileged port number
	 */
	if (nfs_portmon &&
	    ntohs(req->rq_xprt->xp_raddr.sin_port) >= IPPORT_RESERVED) {
#pragma mips_frequency_hint NEVER
		(void) printf("NFS request from unprivileged port.\n");
		return (0);
	}

	/*
	 * Set uid, gid, and gids to auth params
	 */
	flavor = req->rq_cred.oa_flavor;
	if (flavor != exi->exi_export.ex_auth) {
		flavor = AUTH_NULL;
	}
	switch (flavor) {
	case AUTH_NULL:
		cred->cr_uid = (o_uid_t)exi->exi_export.ex_anon;
		cred->cr_gid = (o_gid_t)exi->exi_export.ex_anon;
		cred->cr_ngroups = 0;
		break;

	case AUTH_UNIX:
		/*
		 * For performance, we delayed decoding the credentials
		 * until this point.
		 */

		if ((exi->exi_export.ex_flags & EX_ACCESS) &&
                    !hostintable((struct sockaddr *)svc_getcaller((req)->rq_xprt),
                     &(exi)->exi_accesshash)) {
#ifdef	DEBUG
                        (void) printf("NFS server: access error\n");
#endif
			return(0);
                }

		decode = (u_int *) (req->rq_cred.oa_base);
		decode++; /* aup_time */
		i = *decode++;
		decode += (i + BYTES_PER_XDR_UNIT - 1) / BYTES_PER_XDR_UNIT;
		cred->cr_uid = *decode++;
		cred->cr_gid = *decode++;
		cred->cr_ngroups = *decode++;
		if (cred->cr_uid == 0 &&
		    !hostintable((struct sockaddr *)svc_getcaller(req->rq_xprt),
				 &exi->exi_roothash)) {
			cred->cr_uid = 
			  ((uid_t)exi->exi_export.ex_anon == NFS_UID_NOBODY ?
			   UID_NOBODY : (uid_t)exi->exi_export.ex_anon);
			cred->cr_gid = 
			  ((gid_t)exi->exi_export.ex_anon == NFS_GID_NOBODY ?
			   GID_NOBODY : (gid_t)exi->exi_export.ex_anon);
			cred->cr_ngroups = 1;
			cred->cr_groups[0] = (gid_t)exi->exi_export.ex_anon;
		} else {
			cred->cr_ngroups = MIN(ngroups_max, cred->cr_ngroups);
			for (i = 0; i < cred->cr_ngroups; i++)
				cred->cr_groups[i] = *decode++;
		}
		break;

	default:
		return (0);
	}

	/* 
	 * Allow access even if ex_anon == -1 if anon_ok is true,
	 * so that mounts will work
	 */
	if (cred->cr_uid == (uid_t) -1) {
		cred->cr_uid = UID_NOBODY;
		cred->cr_gid = GID_NOBODY;
	} else {
		anon_ok = 1;
	}

	return (anon_ok);
}

/*
 * Given a pointer to an NFS2 fhandle_t and a mode, open
 * the file corresponding to the fhandle_t with the given mode and
 * return a file descriptor.  
 */
int
nfs_cnvt(void *fhp, int filemode, int *nfd)
{
	fhandle_t tfh;
	struct vnode *vp;
	int fd, error;
	struct exportinfo *exi;

	if (!_CAP_ABLE(CAP_MOUNT_MGT))
		return EPERM;

	if (copyin(fhp, &tfh, sizeof (tfh)))
		return EFAULT;

	filemode -= FOPEN;	/* adjust user-level flags to match kernel */
	if ((filemode & FCREAT) || !(filemode & (FREAD|FWRITE)))
		return EINVAL;

	exi = findexport(&tfh.fh_fsid, (struct fid *)&tfh.fh_xlen);
	vp = fhtovp(&tfh, exi);
	if (exi) 
		EXPORTED_MRUNLOCK();
	if (vp == NULL)
		return ESTALE;

	if (error = vfile_assign(&vp, filemode|FNONBLOCK, &fd)) {
		VN_RELE(vp);
	} else {
		if (nfd) *nfd = fd;
	}
	return error;
}

/*
 * Given a pointer to an NFS3 fhandle_t and a mode, open
 * the file corresponding to the fhandle_t with the given mode and
 * return a file descriptor.  
 */
int
nfs_cnvt3(void *fhp, int filemode, int *nfd)
{
	nfs_fh3 tfh;
	struct vnode *vp;
	int fd, error;
	struct exportinfo *exi;

	if (!_CAP_ABLE(CAP_MOUNT_MGT))
		return EPERM;

	if (copyin(fhp, &tfh, sizeof (tfh)))
		return EFAULT;

	filemode -= FOPEN;	/* adjust user-level flags to match kernel */
	if ((filemode & FCREAT) || !(filemode & (FREAD|FWRITE)))
		return EINVAL;

	exi = findexport(&tfh.fh3_fsid, (struct fid *)&tfh.fh3_xlen);
	vp = fh3tovp(&tfh, exi);
	if (exi) 
		EXPORTED_MRUNLOCK();
	if (vp == NULL)
		return ESTALE;

	if (error = vfile_assign(&vp, filemode|FNONBLOCK, &fd)) {
		VN_RELE(vp);
	} else {
		if (nfd) *nfd = fd;
	}
	return error;
}
