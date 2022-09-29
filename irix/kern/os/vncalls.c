/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

/*#ident	"@(#)uts-comm:fs/vncalls.c	1.46"*/
#ident	"$Revision: 1.191 $"

/*
 * System call routines for operations on files.  These manipulate
 * the global and per-process file table entries which refer to
 * vnodes, the system generic file abstraction.
 *
 * Many operations take a path name.  After preparing arguments, a
 * typical operation may proceed with:
 *
 *	error = lookupname(name, seg, followlink, &dvp, &vp);
 *
 * where "name" is the path name operated on, "seg" is UIO_USERSPACE
 * or UIO_SYSSPACE to indicate the address space in which the path
 * name resides, "followlink" specifies whether to follow symbolic
 * links, "dvp" is a pointer to a vnode for the directory containing
 * "name", and "vp" is a pointer to a vnode for "name".  (Both "dvp"
 * and "vp" are filled in by lookupname()).  "error" is zero for a
 * successful lookup, or a non-zero errno (from <sys/errno.h>) if an
 * error occurred.  This paradigm, in which routines return error
 * numbers to their callers and other information is returned via
 * reference parameters, now appears in many places in the kernel.
 *
 * lookupname() fetches the path name string into an internal buffer
 * using pn_get() (pathname.c) and extracts each component of the path
 * by iterative application of the file system-specific VOP_LOOKUP
 * operation until the final vnode and/or its parent are found.
 * (There is provision for multiple-component lookup as well.)  If
 * either of the addresses for dvp or vp are NULL, lookupname() assumes
 * that the caller is not interested in that vnode.  Once a vnode has
 * been found, a vnode operation (e.g. VOP_OPEN, VOP_READ) may be
 * applied to it.
 *
 * With few exceptions (made only for reasons of backward compatibility)
 * operations on vnodes are atomic, so that in general vnodes are not
 * locked at this level, and vnode locking occurs at lower levels (either
 * locally, or, perhaps, on a remote machine.  (The exceptions make use
 * of the VOP_RWLOCK and VOP_RWUNLOCK operations.)  In addition permission 
 * checking is generally done by the specific filesystem, via its VOP_ACCESS
 * operation.  The upper (vnode) layer performs checks involving file
 * types (e.g. VREG, VDIR), since the type is static over the life of
 * the vnode (usually).
 */

#include <limits.h>
#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kfcntl.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/filio.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/locking.h>
#include <sys/mode.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/kuio.h>
#include <sys/unistd.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ddi.h>
#include <sys/runq.h>
#include <sys/mac_label.h>
#include <sys/sat.h>
#include <fifofs/fifonode.h>		/* for fifo_rdchk prototype */
#include <sys/flock.h>
#include <sys/ktime.h>
#include <sys/ktypes.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/xlate.h>
#include <sys/attributes.h>
#include <sys/mount.h>
#include <sys/arsess.h>			/* for dfltprid */
#include <sys/vsocket.h>
#include <ksys/vpag.h>
#include <ksys/vproc.h>
#include <ksys/vop_backdoor.h>
#include <string.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
/*
 * Open a file.
 */
struct opena {
	char *fname;
	sysarg_t fmode;
	usysarg_t cmode;
};

int copen(char *, int, mode_t, rval_t *);

int
open(struct opena *uap, rval_t *rvp)
{
	return copen(uap->fname, uap->fmode-FOPEN, (mode_t)uap->cmode, rvp);
}

/*
 * Create a file.
 */
struct creata {
	char *fname;
	sysarg_t cmode;
};

int
creat(struct creata *uap, rval_t *rvp)
{
	return copen(uap->fname, FWRITE|FCREAT|FTRUNC, (mode_t)uap->cmode, rvp);
}

/*
 * Common code for open() and creat().  Check permissions, allocate
 * an open file structure, and call the device open routine (if any).
 */
int
copen(char *fname, int filemode, mode_t createmode, rval_t *rvp)
{
	vnode_t *vp;
	vfile_t *fp;
	register int error;
	int fd, dupfd;
	enum vtype type;
	int sat_flags = filemode;
	int cflags = 0;
	int *ckptp = NULL;
#ifdef CKPT
	extern int ckpt_enabled;
#endif
	if ((filemode & (FREAD|FWRITE)) == 0)
		return EINVAL;

	if ((filemode & (FNONBLOCK|FNDELAY)) == (FNONBLOCK|FNDELAY))
		filemode &= ~FNDELAY;

	error = vfile_alloc(filemode & FMASK, &fp, &fd);
	if (error)
		goto out;
	/*
	 * a kludge for the usema device - it really needs to know
	 * the 'fp' for its opening file - we use ut_openfp to mark this.
	 */
	curuthread->ut_openfp = fp;
#ifdef CKPT
	/*
	 * set up to checkpoint lookup
	 */
	ckptp = (ckpt_enabled)? &fp->vf_ckpt : NULL;
#endif
	/* XXX glowell The original code here was a bit odd. Check */
	if ((filemode & FWRITE) == 0) 
		_SAT_PN_BOOK(SAT_OPEN_RO, curuthread);
	else
		_SAT_PN_BOOK(SAT_OPEN, curuthread);
	if (getfsizelimit() == 0)
		cflags |= VZFS;

	/*
	 * Last arg is a don't-care term if !(filemode & FCREAT).
	 */
	error = vn_open(fname, UIO_USERSPACE, filemode,
			(createmode & MODEMASK) & ~curuthread->ut_cmask,
			&vp, CRCREAT, cflags, ckptp);

	curuthread->ut_openfp = NULL;

	if (error)
		vfile_alloc_undo(fd, fp);
	else if (vp->v_flag & VDUP) {
		/*
		 * Special handling for /dev/fd.  Give up the file pointer
		 * and dup the indicated file descriptor (in v_rdev).  
		 */
		vfile_alloc_undo(fd, fp);
		dupfd = getminor(vp->v_rdev);
		type = vp->v_type;
		VN_FLAGCLR(vp, VDUP);
		VN_RELE(vp);
		if (type != VCHR) {
			error = EINVAL;
			goto out;
		}
		if (error = getf(dupfd, &fp))
			goto out;
		if (error = fdt_dup(0, fp, &fd))
			goto out;
		rvp->r_val1 = fd;
	} else {
		vfile_ready(fp, vp);
		rvp->r_val1 = fd;
	}

out:
	/* XXX glowell can we dispose of sat flags */
	_SAT_OPEN(rvp->r_val1, (filemode&FCREAT), sat_flags, error);
	return error;
}

/*
 * Close a file.
 */
struct closea {
	sysarg_t fdes;
};

/* ARGSUSED */
int
close(struct closea *uap, rval_t *rvp)
{
	auto vfile_t *fp;
	register int error;

	if (error = closefd((int)uap->fdes, &fp))
		return error;
	error = vfile_close(fp);
	_SAT_CLOSE(uap->fdes, error);
	return error;
}

/*
 * Read and write.
 */
struct rwa {
	sysarg_t fdes;
	char *cbuf;
	usysarg_t count;
	sysarg_t off64;		/* off64 is the offset for 64 bit apps */
	sysarg_t off1; 		/* off1 and off2 is the 64bit offset */
	sysarg_t off2;		/*   for 32 bit apps */
};
enum	rwrtn { readwrite, preadpwrite };

/*
 * Readv and writev.
 */
struct rwva {
	sysarg_t fdes;
	struct irix5_iovec *iovp;
	sysarg_t iovcnt;
};

static int	rdwrv(struct rwva *, rval_t *, int);


/*
 * This is just a small utility routine to pull this 
 * infrequently positive code out of the main I/O path.
 * It simply converts the unusual file structure flags
 * into vnode interface I/O flags.
 *
 * The FRSYNC and FAPPEND are specific to the read and
 * write paths, respectively, so we only set them as
 * appropriate.
 */
static int
set_ioflags(int fflag, int mode)
{
	register int	ioflag;
	
	ioflag = 0;
	if (fflag & FBULK)
		ioflag |= IO_BULK;
	if (fflag & FDIRECT)
		ioflag |= IO_DIRECT;
	if (fflag & FSYNC)
		ioflag |= IO_SYNC;
	if (fflag & FDSYNC)
		ioflag |= IO_DSYNC;
	if (fflag & FINVIS)
		ioflag |= IO_INVIS;
	if ((fflag & FPRIORITY) || (fflag & FPRIO))
		ioflag |= IO_PRIORITY;
	if (mode == FREAD) {
		if (fflag & FRSYNC)
			ioflag |= IO_RSYNC;
	} else {
		if (fflag & FAPPEND)
			ioflag |= IO_APPEND;
	}

	return ioflag;
}

static int 
_read(struct rwa *uap, rval_t *rvp, enum rwrtn wherefrom)
{
	struct uio		uio;
	register vnode_t	*vp;
	enum vtype		type;
	uthread_t		*ut = curuthread;
	struct iovec		aiov;
	int			ioflag;
	ssize_t			count, xfer;
	vfile_t			*fp;
	int			error, locked;

	if (error = getf((int)uap->fdes, &fp))
		return error;

	if (((uio.uio_fmode = fp->vf_flag) & FREAD) == 0) {
		_SAT_FD_RDWR(uap->fdes, FREAD, EBADF);
		return EBADF;
	}

	SYSINFO.sysread++;
	ut->ut_acct.ua_syscr++;
	aiov.iov_base = (void *)uap->cbuf;

	if ((count = uio.uio_resid = aiov.iov_len = uap->count) < 0)
		return EINVAL;
	uio.uio_iov = &aiov;
	uio.uio_iovcnt = 1;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_fp = fp;

	/*
	 * don't need to keep track of uio_limit as not increasing file size
	 */

	/*
	 * Do the read - on either a socket or a vnode.
	 */
	if (VF_IS_VSOCK(fp)) {
		if (wherefrom == preadpwrite)
			return ESPIPE;
			
		type = VSOCK;
		uio.uio_offset = (off_t)0;
		VSOP_RECEIVE(VF_TO_VSOCK(fp), 0, &uio, 0, 0, error);
#ifdef _SHAREII
		/*
		 *      ShareII potentially charges for character I/O.
		 *	and must be passed a valid (or null) vnode_t pointer.
		 */
		vp = (vnode_t *) 0;
#endif /* _SHAREII */
	} else {
		vp = VF_TO_VNODE(fp);
		type = vp->v_type;

		if (wherefrom == preadpwrite && type == VFIFO)
			return ESPIPE;

		/*
		 * Set io flags.
		 */
		if (fp->vf_flag &
		    (FDIRECT|FSYNC|FDSYNC|FRSYNC|FINVIS|FPRIORITY|FBULK|FPRIO))
			ioflag = set_ioflags(fp->vf_flag, FREAD);
		else
			ioflag = 0;

		/*
		 * Disallow outsiders reading swap.  Not worried about
		 * privacy so much as what happens when a process which
		 * has the vnode RWlock sleeps somewhere because there's
		 * no system memory, and the pager needs to acquire the
		 * lock to swap out pages -- deadlock.
		 */
		if (vp->v_flag & VISSWAP && type == VREG)
			return EACCES;

		if (wherefrom == preadpwrite) {
#if (_MIPS_SIM == _ABI64)
			/*
			 * o32 and n32 kernels will put the off_t in
			 * off1 and off2 if the caller is n32 because
			 * both kernels have only 32 bit sysarg_t's.
			 */
			if (ABI_HAS_64BIT_REGS(get_current_abi())) 
				uio.uio_offset = (off_t)uap->off64;
			else
#endif
				uio.uio_offset =
  				  (((off_t)(uap->off1) & 0xffffffff) << 32) |
					   (uap->off2 & 0xffffffff);
		} else {
			if (type == VREG) {
				locked = (fp->vf_count > 1 ||
					  ISSHDFD(ut->ut_pproxy));
				VFILE_GETOFFSET_LOCKED(fp, locked, 
						       &uio.uio_offset);
			} else {
				locked = 0;
				VFILE_GETOFFSET(fp, &uio.uio_offset);
			}
			ASSERT(wherefrom == readwrite);
		}

		VOP_READ(vp, &uio, ioflag, fp->vf_cred, &ut->ut_flid, error);
	}

	xfer = count - uio.uio_resid;
	if (error == EINTR && xfer != 0)
		error = 0;
	rvp->r_val1 = xfer;
	UPDATE_IOCH(ut, ((u_long) xfer));

	/*
	 * We don't change the fp offset for pread or for reads from sockets.
	 */
	if ((type != VSOCK) && (wherefrom == readwrite)) {
		if (type == VREG) {
			VFILE_SETOFFSET_LOCKED(fp, locked, uio.uio_offset);
		} else if (type == VFIFO) {	   /* Backward compatibility */
			VFILE_SETOFFSET(fp, xfer);
		} else {
			VFILE_SETOFFSET(fp, uio.uio_offset);
		}
	}
	/*
	 * This updates the fields in the upage for
	 * PIOCUSAGE ioctl call via /debug.
	 */
	ut->ut_acct.ua_bread += xfer;
	SYSINFO.readch += xfer;

#ifdef _SHAREII
	/*
	 *      ShareII potentially charges for character I/O.
	 */
	SHR_RDWR(vp, FREAD, xfer);
#endif /* _SHAREII */

	_SAT_FD_RDWR(uap->fdes, FREAD, error);
	return error;
}

int
read(struct rwa *uap, rval_t *rvp)
{
	return _read(uap, rvp, readwrite);
}

int
pread(struct rwa *uap, rval_t *rvp)
{
	return _read(uap, rvp, preadpwrite);
}

static int
_write(struct rwa *uap, rval_t *rvp, enum rwrtn wherefrom)
{
	struct uio		uio;
	register vnode_t	*vp;
	enum vtype		type;
	uthread_t		*ut = curuthread;
	struct iovec		aiov;
	int			ioflag;
	ssize_t			count, xfer;
	vfile_t			*fp;
	int			error, locked;

	if (error = getf((int)uap->fdes, &fp))
		return error;

	if (((uio.uio_fmode = fp->vf_flag) & FWRITE) == 0) {
		_SAT_FD_RDWR(uap->fdes, FWRITE, EBADF);
		return EBADF;
	}
	SYSINFO.syswrite++;
	ut->ut_acct.ua_syscw++;
	aiov.iov_base = (void *)uap->cbuf;
	if ((count = uio.uio_resid = aiov.iov_len = uap->count) < 0)
		return EINVAL;
	uio.uio_iov = &aiov;
	uio.uio_iovcnt = 1;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	uio.uio_sigpipe = 0;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_limit = getfsizelimit();
	uio.uio_fp = fp;

	/*
	 * Do the write - on either a socket or a vnode.
	 */
	if (VF_IS_VSOCK(fp)) {
		if (wherefrom == preadpwrite)
			return ESPIPE;
			
		type = VSOCK;
		uio.uio_offset = (off_t)0;
		VSOP_SEND(VF_TO_VSOCK(fp), 0, &uio, 0, 0, error);
#ifdef _SHAREII
		/*
		 *      ShareII potentially charges for character I/O.
		 *	and must be passed a valid (or null) vnode_t pointer.
		 */
		vp = (vnode_t *) 0;
#endif /* _SHAREII */
	} else {
		vp = VF_TO_VNODE(fp);
		type = vp->v_type;

		if (wherefrom == preadpwrite && type == VFIFO)
			return ESPIPE;

		/*
		 * Set io flags.
		 */
		if (fp->vf_flag &
		    (FAPPEND|FSYNC|FDSYNC|FDIRECT|FINVIS|FPRIORITY|FBULK|FPRIO))
			ioflag = set_ioflags(fp->vf_flag, FWRITE);
		else
			ioflag = 0;

		/*
		 * Disallow outsiders writing swap.  Not worried about data
		 * corruption so much as what happens when a process which
		 * has the vnode RWlock sleeps somewhere because there's no
		 * system memory, and the pager needs to acquire the lock to
		 * swap out pages: deadlock.
		 */
		if (vp->v_flag & VISSWAP && type == VREG)
			return EACCES;

		if (wherefrom == preadpwrite) {
#if (_MIPS_SIM == _ABI64)
			/*
			 * o32 and n32 kernels will put the off_t in
			 * off1 and off2 if the caller is n32 because
			 * both kernels have only 32 bit sysarg_t's.
			 */
			if (ABI_HAS_64BIT_REGS(get_current_abi())) 
				uio.uio_offset = (off_t)uap->off64;
			else
#endif
				uio.uio_offset =
				  (((off_t)(uap->off1) & 0xffffffff) << 32) |
				  (uap->off2 & 0xffffffff);
		} else {
			if (type == VREG) {
				locked = (fp->vf_count > 1 || ISSHDFD(ut->ut_pproxy));
				VFILE_GETOFFSET_LOCKED(fp, locked, 
						       &uio.uio_offset);
			} else {
				locked = 0;
				VFILE_GETOFFSET(fp, &uio.uio_offset);
			}
			ASSERT(wherefrom == readwrite);
		}

		VOP_WRITE(vp, &uio, ioflag, fp->vf_cred, &ut->ut_flid, error);
	}

	xfer = count - uio.uio_resid;
	if (error == EINTR && xfer != 0)
		error = 0;

	/* Send sigpipe to current thread, if indicated. */
	if (uio.uio_sigpipe) {
		ASSERT(error != 0);
		sigtouthread(curuthread, SIGPIPE, (k_siginfo_t *)NULL);
	}

	rvp->r_val1 = xfer;
	UPDATE_IOCH(ut, ((u_long) xfer));

	/*
	 * We don't change the fp offset for pwrite or for writes to sockets.
	 */
	if ((type != VSOCK) && (wherefrom == readwrite)) {
		if (type == VFIFO) {	/* Backward compatibility */
			VFILE_SETOFFSET(fp, xfer);
		} else {
			if (((fp->vf_flag & FAPPEND) == 0) || type != VREG ||
			    uap->count != 0) {		/* POSIX */
				if (type == VREG) {
					VFILE_SETOFFSET_LOCKED(fp, locked, 
						uio.uio_offset);
				} else {
					VFILE_SETOFFSET(fp, uio.uio_offset);
				}
			} else {
				if (type == VREG) {
					VFILE_SETOFFSET_LOCKED(fp, locked, 
						VFILE_NO_OFFSET);
				} else {
					VFILE_SETOFFSET(fp, VFILE_NO_OFFSET);
				}
			}
		}
	}

	SYSINFO.writech += xfer;
	ut->ut_acct.ua_bwrit += xfer;

#ifdef _SHAREII
	/*
	 *      ShareII potentially charges for character I/O.
	 */
	SHR_RDWR(vp, FWRITE, xfer);
#endif /* _SHAREII */

	_SAT_FD_RDWR(uap->fdes, FWRITE, error);
	return error;
}


int
write(struct rwa *uap, rval_t *rvp)
{
	return _write(uap, rvp, readwrite);
}

int
pwrite(struct rwa *uap, rval_t *rvp)
{
	return _write(uap, rvp, preadpwrite);
}

/*
 * This is a little hack put in so that syssgi(SGI_READB/WRITEB)
 * continues to work. Instead of calling biophysio like we used to,
 * we simply use the pread/pwrite interface here.
 */
int
syssgi_rdwrb(int which, int fdes, char *buf, usysarg_t count,
	 sysarg_t off64, sysarg_t off1, sysarg_t off2, rval_t *rvp)
{
	struct rwa 	uap;
	int		error;

	uap.fdes = fdes;
	uap.cbuf = buf;
	uap.off64 = off64;
	uap.off1 = off1;
	uap.off2 = off2;
	uap.count = count;

	if (which == FREAD)
		error = _read(&uap, rvp, preadpwrite);
	else
		error = _write(&uap, rvp, preadpwrite);
	
	/*
	 * READB/WRITEB interface returns the amount read/written in
	 * BBs not bytes.
	 */
	rvp->r_val1 = BTOBBT(rvp->r_val1);
	return (error);
}

int
readv(struct rwva *uap, rval_t *rvp)
{
	int error;

	error = rdwrv(uap, rvp, FREAD);
	_SAT_FD_RDWR(uap->fdes, FREAD, error);
	return error;
}

int
writev(struct rwva *uap, rval_t *rvp)
{
	int error;

	error = rdwrv(uap, rvp, FWRITE);
	_SAT_FD_RDWR(uap->fdes, FWRITE, error);
	return error;
}


/*
 * Common code for readv and writev calls: check permissions, set base,
 * count, and offset, and switch out to VOP_READ or VOP_WRITE code.
 */
static int
rdwrv(struct rwva *uap, rval_t *rvp, register int mode)
{
	vfile_t		*fp;
	int		error;
	uio_t		uio;
	iovec_t		aiov[16], *aiovp;
	vnode_t		*vp;
	vsock_t		*vs;
	enum vtype	type;
	iovec_t		*iovp;
	int		ioflag;
	ssize_t		count, xfer;
	int		i;
	uthread_t	*ut = curuthread;

	if (error = getf(uap->fdes, &fp))
		return error;
	if ((fp->vf_flag & mode) == 0)
		return EBADF;
	if (mode == FREAD) {
		SYSINFO.sysread++;
		ut->ut_acct.ua_syscr++;
	} else {
		SYSINFO.syswrite++;
		ut->ut_acct.ua_syscw++;
	}
	if (uap->iovcnt <= 0 || uap->iovcnt > _MAX_IOVEC)
		return EINVAL;
	uio.uio_iovcnt = uap->iovcnt;
	if (uio.uio_iovcnt > sizeof(aiov) / sizeof(aiov[0]))
		uio.uio_iov =
			kmem_alloc(uap->iovcnt * sizeof(iovec_t), KM_SLEEP);
	else
		uio.uio_iov = aiov;
	aiovp = uio.uio_iov;
	if (COPYIN_XLATE(uap->iovp, uio.uio_iov,
			uio.uio_iovcnt * sizeof(iovec_t),
			irix5_to_iovec, get_current_abi(), uio.uio_iovcnt)) {
		error = EFAULT;
		goto done;
	}

	uio.uio_resid = 0;
	iovp = uio.uio_iov;
	for (i = 0, iovp = uio.uio_iov; i < uio.uio_iovcnt; i++, iovp++) {
		/* iov_len is unsigned, we need to keep its domain as signed */
		if (iovp->iov_len > SSIZE_MAX) {
			error = EINVAL;
			goto done;
		}
		uio.uio_resid += iovp->iov_len;
		if (uio.uio_resid < 0) {
			error = EINVAL;
			goto done;
		}
	}

	count = uio.uio_resid;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	uio.uio_sigpipe = 0;
	uio.uio_limit = getfsizelimit();
	uio.uio_fmode = fp->vf_flag;
	uio.uio_fp = fp;

	if (VF_IS_VSOCK(fp)) {
		vs = VF_TO_VSOCK(fp);
		type = VSOCK;
		uio.uio_offset = (off_t)0;
		if (mode == FREAD) {
			VSOP_RECEIVE(vs, 0, &uio, 0, 0, error);
		} else {
			VSOP_SEND(vs, 0, &uio, 0, 0, error);
		}
#ifdef _SHAREII
		/*
		 *      ShareII potentially charges for character I/O
		 *	and must be passed a valid (or null) vnode_t pointer.
		 */
		vp = (vnode_t *) 0;
#endif /* _SHAREII */
	} else {
		vp = VF_TO_VNODE(fp);
		type = vp->v_type;

		/*
		 * Set io flags.
		 */
		if (fp->vf_flag &
		    (FAPPEND|FSYNC|FDSYNC|FRSYNC|FDIRECT|FINVIS|FPRIORITY|FBULK|FPRIO))
			ioflag = set_ioflags(fp->vf_flag, mode);
		else
			ioflag = 0;

		/*
		 * Disallow outsiders reading/writing swap.  Not worried
		 * about data corruption so much as what happens when a
		 * process which has the vnode RWlock sleeps somewhere
		 * because there's no system memory, and the pager needs
		 * to acquire the lock to swap out pages: deadlock.
		 */
		if (vp->v_flag & VISSWAP && type == VREG) {
			error = EACCES;
			goto done;
		}

		VFILE_GETOFFSET(fp, &uio.uio_offset);
		if (mode == FREAD) {
			VOP_READ(vp, &uio, ioflag, fp->vf_cred,
				 &ut->ut_flid, error);
		} else {
			VOP_WRITE(vp, &uio, ioflag, fp->vf_cred,
				  &ut->ut_flid, error);
		}
	}

	xfer = count - uio.uio_resid;
	if (error == EINTR && xfer != 0)
		error = 0;

	if (uio.uio_sigpipe) {
		ASSERT(error != 0);
		ASSERT(mode == FWRITE);
		sigtouthread(ut, SIGPIPE, (k_siginfo_t *)NULL);
	}

	rvp->r_val1 = xfer;
	UPDATE_IOCH(ut, ((u_long) xfer));

	if (type == VFIFO) {	/* Backward compatibility */
		VFILE_SETOFFSET(fp, xfer);
	} else if (type != VSOCK) {
		VFILE_SETOFFSET(fp, uio.uio_offset);
	}

	if (mode == FREAD) {
		SYSINFO.readch += xfer;
		ut->ut_acct.ua_bread += xfer;
	} else {
		SYSINFO.writech += xfer;
		ut->ut_acct.ua_bwrit += xfer;
	}
#ifdef _SHAREII
	/*
	 *      ShareII potentially charges for character I/O.
	 */
	SHR_RDWR(vp, mode, xfer);
#endif /* _SHAREII */

done:
	if (aiovp != aiov)
		kmem_free(aiovp, uap->iovcnt * sizeof(iovec_t));
	return error;
}

/*
 * Change current working directory (".").
 */
struct chdira {
	char *fname;
};

/* ARGSUSED */
int
chdir(struct chdira *uap, rval_t *rvp)
{
	vnode_t *vp;
	int error;

	_SAT_PN_BOOK(-1, curuthread);
	error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL);
	if (!error) {
		if (vp->v_type == VDIR) {

			VOP_ACCESS(vp, VEXEC, get_current_cred(), error);
			if (!error) {
				CURVPROC_SET_DIR(vp, &curuthread->ut_cdir,
						 VDIR_CURRENT);
			} else {
				VN_RELE(vp);
			}

		} else {
			VN_RELE(vp);
			error = ENOTDIR;
		}
	}
	_SAT_CHRWDIR(error);
	return error;
}

/*
 * File-descriptor based version of 'chdir'.
 */
struct fchdira {
	sysarg_t fd; 
};

/* ARGSUSED */
int
fchdir(struct fchdira *uap, rval_t *rvp)
{
	vfile_t *fp;
	vnode_t *vp;
	register int error;
	
	if ((error = getf((int)uap->fd, &fp)) == 0 ) {
		if (VF_IS_VNODE(fp)) {
			vp = VF_TO_VNODE(fp);
			if (vp->v_type == VDIR) {
				VOP_ACCESS(vp, VEXEC,
					get_current_cred(), error);
				if (!error) {
					VN_HOLD(vp);
					CURVPROC_SET_DIR(vp,
						&curuthread->ut_cdir, VDIR_CURRENT);
				}
			} else {
				error = ENOTDIR;
			}
		} else {
			error = ENOTDIR;
		}
	}
	_SAT_FCHDIR(uap->fd, error);
	return error;
}

/*
 * Change notion of root ("/") directory.
 */
/* ARGSUSED */
int
chroot(struct chdira *uap, rval_t *rvp)
{
	vnode_t *vp;
	register int error;

	if (!_CAP_ABLE(CAP_CHROOT))
		return EPERM;

	_SAT_PN_BOOK(-1, curuthread);

	error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return error;

	if (vp->v_type != VDIR) {
		VN_RELE(vp);
		return ENOTDIR;
	}

	VOP_ACCESS(vp, VEXEC, get_current_cred(), error);
	if (error) {
		VN_RELE(vp);
		return error;
	}

	CURVPROC_SET_DIR(vp, &curuthread->ut_rdir, VDIR_ROOT);

	_SAT_CHRWDIR(error);
	return error;
}


/*
 * Create a special file, a regular file, or a FIFO.
 */

struct xmknoda {
	sysarg_t version;	/* version of this syscall */
	char *fname;		/* pathname passed by user */
	usysarg_t fmode;	/* mode of pathname */
	usysarg_t dev;		/* device number - b/c specials only */
};

static int
cmknod(int version, char *fname, mode_t fmode, dev_t dev)
{
	vnode_t *vp;
	struct vattr vattr;
	int error;
	int cflags = VEXCL;

	if (version != _MKNOD_VER)
		return EINVAL;

	/*
	 * Zero type is equivalent to a regular file.
	 */
	if ((fmode & S_IFMT) == 0) {
		fmode |= S_IFREG;
	}

	/*
	 * Must be the super-user unless making a FIFO node.
	 */
	if (!S_ISFIFO(fmode) && !_CAP_ABLE(CAP_MKNOD)) {
		return EPERM;
	}

	/*
	 * Make sure not to accept bad file types.
	 */
	vattr.va_type = IFTOVT(fmode);
	if (vattr.va_type == VNON) {
		return EINVAL;
	}

	/*
	 * Set up desired attributes and vn_create the file.
	 */
	vattr.va_mode = (fmode & MODEMASK) & ~curuthread->ut_cmask;
	vattr.va_mask = AT_TYPE|AT_MODE;
	if (vattr.va_type == VCHR || vattr.va_type == VBLK) {
		if (dev == (dev_t)NODEV ||
		    getemajor(dev) == (dev_t)NODEV) {
				return EINVAL;
		}
		vattr.va_rdev = dev;
		vattr.va_mask |= AT_RDEV;
	}
	if (getfsizelimit() == 0)
		cflags |= VZFS;

	_SAT_PN_BOOK(SAT_FILE_CRT_DEL, curuthread);

	if ((error = vn_create(fname, UIO_USERSPACE, &vattr, cflags, 0,
					&vp, CRMKNOD, NULL)) == 0)
		VN_RELE(vp);

	/* if there was no error, lookup what we just created */
	if (sat_enabled && !error) {
		error = _SAT_LOOKUP(fname, NO_FOLLOW, curuthread);
	}
	_SAT_ACCESS(SAT_FILE_CRT_DEL, error);
	return error;
}

/*
 * Expanded mknod.
 */
int
xmknod(struct xmknoda *uap)
{
	return cmknod(uap->version, uap->fname, (mode_t)uap->fmode,
			(dev_t)uap->dev);
}

/*
 * Make a directory.
 */
struct mkdira {
	char *dname;
	usysarg_t dmode;
};

/* ARGSUSED */
int
mkdir(struct mkdira *uap, rval_t *rvp)
{
	vnode_t *vp;
	struct vattr vattr;
	int error;
	int cflags = VEXCL;

	vattr.va_type = VDIR;
	vattr.va_mode = (uap->dmode & PERMMASK) & ~curuthread->ut_cmask;
	vattr.va_mask = AT_TYPE|AT_MODE;
	if (getfsizelimit() == 0)
		cflags |= VZFS;
	_SAT_PN_BOOK(SAT_FILE_CRT_DEL, curuthread);

	error = vn_create(uap->dname, UIO_USERSPACE, &vattr, cflags, 0, &vp, 
			  CRMKDIR, NULL);
	if (!error)
		VN_RELE(vp);	/* release ref. from vn_create */

	/* if there was no error, lookup what we just created */
	if (sat_enabled && !error)
		error = _SAT_LOOKUP(uap->dname, NO_FOLLOW, curuthread);
	_SAT_ACCESS(SAT_FILE_CRT_DEL, error);
	return error;
}

/*
 * Make a hard link.
 */
struct linka {
	char	*from;
	char	*to;
};

/*
 * IRIX version - don't follow symlink from argument.
 */
/* ARGSUSED */
int
link(struct linka *uap, rval_t *rvp)
{
	return vn_link(uap->from, uap->to, UIO_USERSPACE, NO_FOLLOW);
}

/*
 * XPG4 version - follow symlink from argument.
 */
/* ARGSUSED */
int
linkfollow(struct linka *uap, rval_t *rvp)
{
	return vn_link(uap->from, uap->to, UIO_USERSPACE, FOLLOW);
}

/*
 * Rename or move an existing file.
 */
struct renamea {
	char	*from;
	char	*to;
};

/* ARGSUSED */
int
rename(struct renamea *uap, rval_t *rvp)
{
	return vn_rename(uap->from, uap->to, UIO_USERSPACE);
}

/*
 * Create a symbolic link.  Similar to link or rename except target
 * name is passed as string argument, not converted to vnode reference.
 */
struct symlinka {
	char	*target;
	char	*linkname;
};

/* ARGSUSED */
int
symlink(struct symlinka *uap, rval_t *rvp)
{
	vnode_t *dvp;
	struct vattr vattr;
	struct pathname tpn;
	struct pathname lpn;
	vpagg_t *vpag;
	int error;

	if (error = pn_get(uap->linkname, UIO_USERSPACE, &lpn))
		return error;
	_SAT_PN_SAVE(&lpn, curuthread);
	_SAT_PN_BOOK(SAT_FILE_CRT_DEL, curuthread);
	if (error = lookuppn(&lpn, NO_FOLLOW, &dvp, NULLVPP, NULL)) {
		pn_free(&lpn);
		return error;
	}
	if (dvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}
	if (error = pn_get(uap->target, UIO_USERSPACE, &tpn))
		goto out;
	_SAT_PN_SAVE(&tpn, curuthread);

	VPROC_GETVPAGG(curvprocp, &vpag);
	vattr.va_projid = (int)VPAG_GETPRID(vpag);

	vattr.va_type = VLNK;
	vattr.va_mode = 0777 & ~curuthread->ut_cmask;
	vattr.va_mask = AT_TYPE|AT_MODE|AT_PROJID;
	VOP_SYMLINK(dvp, lpn.pn_path, &vattr, tpn.pn_path,
		    get_current_cred(), error);
	pn_free(&tpn);

	if (!error)
		error = _MAC_INITIAL_PATH(uap->linkname);

	/* if there was no error, lookup what we just created */
	if (sat_enabled && !error)
		error = _SAT_LOOKUP(uap->linkname, NO_FOLLOW, curuthread);
out:
	pn_free(&lpn);
	VN_RELE(dvp);
	_SAT_ACCESS(SAT_FILE_CRT_DEL, error);
	return error;
}

/*
 * Unlink (i.e. delete) a file.
 */
struct unlinka {
	char	*fname;
};

/* ARGSUSED */
int
unlink(struct unlinka *uap, rval_t *rvp)
{
	int error;

	_SAT_PN_BOOK(SAT_FILE_CRT_DEL, curuthread);
	error = vn_remove(uap->fname, UIO_USERSPACE, RMFILE);
	_SAT_ACCESS(SAT_FILE_CRT_DEL, error);
	return error;
}

/*
 * Remove a directory.
 */
struct rmdira {
	char *dname;
};

/* ARGSUSED */
int
rmdir(struct rmdira *uap, rval_t *rvp)
{
	int error;

	_SAT_PN_BOOK(SAT_FILE_CRT_DEL, curuthread);
	error = vn_remove(uap->dname, UIO_USERSPACE, RMDIRECTORY);
	_SAT_ACCESS(SAT_FILE_CRT_DEL, error);
	return error;
}

/*
 * Get directory entries in a file system-independent format.
 */
struct getdentsa {
	sysarg_t fd;
	char *buf;
	usysarg_t count;
	int *eofp;
};

static int
cgetdents(struct getdentsa *uap, rval_t *rvp, int fmode, int *eofp)
{
	register vnode_t *vp;
	vfile_t *fp;
	struct uio auio;
	struct iovec aiov;
	register int error;

	if (error = getf((int)uap->fd, &fp))
		return error;

	if (!VF_IS_VNODE(fp))
		return ENOTDIR;

	vp = VF_TO_VNODE(fp);
	if (vp->v_type != VDIR)
		return ENOTDIR;

	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	if (aiov.iov_len == 0 || (aiov.iov_len > SSIZE_MAX))
		return EINVAL;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_pmp = NULL;
	auio.uio_pio = 0;
	auio.uio_readiolog = 0;
	auio.uio_writeiolog = 0;
	auio.uio_pbuf = 0;
	VFILE_GETOFFSET(fp, &auio.uio_offset);
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = uap->count;
	auio.uio_fmode = fmode;
	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_READDIR(vp, &auio, fp->vf_cred, eofp, error);
	VOP_RWUNLOCK(vp, VRWLOCK_READ);
	if (!error) {
		rvp->r_val1 = uap->count - auio.uio_resid;
		VFILE_SETOFFSET(fp, auio.uio_offset);
	}
	return error;
}

int
getdents(struct getdentsa *uap, rval_t *rvp)
{
	int sink;

	return cgetdents(uap, rvp, 0, &sink);
}

int
getdents64(struct getdentsa *uap, rval_t *rvp)
{
	int sink;

	return cgetdents(uap, rvp, FDIRENT64, &sink);
}

int
ngetdents(struct getdentsa *uap, rval_t *rvp)
{
	int eof, rval;

	rval = cgetdents(uap, rvp, 0, &eof);
	if (rval == 0 && copyout(&eof, uap->eofp, sizeof(eof)))
		rval = EFAULT;
	return rval;
}

int
ngetdents64(struct getdentsa *uap, rval_t *rvp)
{
	int eof, rval;

	rval = cgetdents(uap, rvp, FDIRENT64, &eof);
	if (rval == 0 && copyout(&eof, uap->eofp, sizeof(eof)))
		rval = EFAULT;
	return rval;
}

/*
 * Seek on file.
 */
struct lseeka {
	sysarg_t fdes;
	sysarg_t off;
	sysarg_t sbase;
};

struct lseek64a {
	sysarg_t fdes;
	sysarg_t pad0;
	sysarg_t off1;
	sysarg_t off2;
	sysarg_t sbase;
};

static int 
do_lseek(int fdes, off_t *offp, int sbase)
{
	vfile_t *fp;
	register vnode_t *vp;
	struct vattr vattr;
	register int error;
	off_t oldoff;

	if (error = getf(fdes, &fp)) {
		_SAT_FD_READ(fdes,error);
		return error;
	}

	if (VF_IS_VSOCK(fp))
		return ESPIPE;

	vp = VF_TO_VNODE(fp);
	/*
	 * atomically grab f_offset -
	 * no real reason to lock here but we'll give slightly better results
	 * by having our calculations be consistent
	 */
	VFILE_GETOFFSET(fp, &oldoff);

	if (sbase == 1)
		*offp += oldoff;
	else if (sbase == 2) {
		vattr.va_mask = AT_SIZE;
		VOP_GETATTR(vp, &vattr, 0, get_current_cred(), error);
		if (error)
			goto out;
		*offp += vattr.va_size;
	} else if (sbase != 0) {
		sigtopid(current_pid(), SIGSYS, SIG_ISKERN, 0, 0, 0);
		error = EINVAL;
		goto out;
	}

	VOP_SEEK(vp, oldoff, offp, error);
	if (!error)
		VFILE_SETOFFSET(fp, *offp);

out:
	_SAT_FD_READ(fdes,error);
	return error;
}

/*
 * n32 and n64 binaries are directed to lseek by libc.  But since
 * o32 and n32 kernels do not save 64 bit arg registers, n32 lseeks are
 * sent back here by lseek.
 */
int
lseek64(struct lseek64a *uap, rval_t *rvp)
{
	int error;
	off_t uap_off;
	int abi = get_current_abi();

	ASSERT(ABI_IS(abi, (ABI_IRIX5|ABI_IRIX5_N32)));

	uap_off = ((off_t)uap->off1 << 32) |
			(off_t)uap->off2 & 0xffffffff;

	error = do_lseek(uap->fdes, &uap_off, uap->sbase);

	if (!error) {
		if (ABI_IS_IRIX5_N32(abi)) {
			rvp->r_val1 = uap_off;
		} else {
			rvp->r_val1 = (long)(uap_off >> 32);
#if (_MIPS_SZLONG == 32)
			rvp->r_val2 = (long)uap_off;
#else
			/*
			 * This is necessary because the upper bits of
			 * r_val2 (v1) will be used if the return value
			 * from lseek64 is compared to.
			 * This ensures that the upper bits of v1 are 
			 * either 0x00000000 or 0xffffffff.
			 */
			rvp->r_val2 = (long)((uap_off << 32) >> 32);
#endif
		}
	}
	return(error);
}

int
lseek(struct lseeka *uap, rval_t *rvp)
{
	int error;
	off_t uap_off = (off_t)uap->off;
	int abi = get_current_abi();

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABIN32)
	if (ABI_IS_IRIX5_N32(abi))
		return(lseek64((struct lseek64a *) uap, rvp));
#endif

	error = do_lseek(uap->fdes, &uap_off, uap->sbase);

	/*
	 * If this is an o32 binary and the return value will
	 * overflow a 32 bit off_t, then return an error rather
	 * than an incorrect value.
	 */
	if (!error) {
		if (ABI_IS_IRIX5(abi) && (uap_off > SEEKLIMIT32))
			error = EOVERFLOW;
	}
	if (!error)
		rvp->r_off = uap_off;

	return(error);
}


/*
 * Determine accessibility of file.
 */
struct accessa {
	char *fname;
	sysarg_t fmode;
};

#define EFF_ONLY_OK	010	/* use effective ids instead of real */
#define EX_OK		020	/* check if a regular, executable file */

/* ARGSUSED */
int
access(struct accessa *uap, rval_t *rvp)
{
	struct vattr vattr;
	vnode_t *vp;
	cred_t *cr;
	cred_t *tmpcr;
	cred_t *savecr;
	register int error, mode, eok, exok;

	if (uap->fmode & ~(EFF_ONLY_OK|EX_OK|R_OK|W_OK|X_OK))
		return EINVAL;

	mode = ((uap->fmode & (R_OK|W_OK|X_OK)) << 6);
	eok = (uap->fmode & EFF_ONLY_OK);
	exok = (uap->fmode & EX_OK);

	if (!eok) {
		savecr = get_current_cred();
		tmpcr = crdup(savecr);
		tmpcr->cr_uid = savecr->cr_ruid;
		tmpcr->cr_gid = savecr->cr_rgid;
		tmpcr->cr_ruid = savecr->cr_uid;
		tmpcr->cr_rgid = savecr->cr_gid;
		set_current_cred(tmpcr);
	}

	_SAT_PN_BOOK(SAT_FILE_ATTR_READ, curuthread);
	error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL);
	if (error) {
		if (!eok) {
			set_current_cred(savecr);
			crfree(tmpcr);
		}
		return error;
	}

	cr = get_current_cred();
	if (mode)
		VOP_ACCESS(vp, mode, cr, error);

	/*
	 * The above VOP_ACCESS would be sufficient if not for it's
	 * being skipped if mode is zero.
	 */
	if (!error && !mode)
		error = _MAC_VACCESS(vp, cr, MACREAD);

	if (!error && exok) {
		vattr.va_mask = AT_MODE;
		VOP_GETATTR(vp, &vattr, 0, cr, error);
		if ((!error) && ((vp->v_type != VREG) ||
		    ((vattr.va_mode & (VEXEC|(VEXEC>>3)|(VEXEC>>6))) == 0)))
			error = EACCES;
	}

	if (!eok) {
		set_current_cred(savecr);
		crfree(tmpcr);
	}
	

	VN_RELE(vp);
	_SAT_ACCESS(SAT_FILE_ATTR_READ, error);
	return error;
}
#ifdef CKPT
int
fdaccess(int fd, int fmode)
{
	struct vattr vattr;
	vnode_t *vp;
	cred_t *cr;
	cred_t *tmpcr;
	cred_t *savecr;
	register int error, mode, eok, exok;
	struct vfile *fp;

	if (fmode & ~(EFF_ONLY_OK|EX_OK|R_OK|W_OK|X_OK))
		return EINVAL;

	if (error = getf(fd, &fp))
		return error;

	if (!VF_IS_VNODE(fp))
		return EINVAL;

	mode = ((fmode & (R_OK|W_OK|X_OK)) << 6);
	eok = (fmode & EFF_ONLY_OK);
	exok = (fmode & EX_OK);

	if (!eok) {
		savecr = get_current_cred();
		tmpcr = crdup(savecr);
		tmpcr->cr_uid = savecr->cr_ruid;
		tmpcr->cr_gid = savecr->cr_rgid;
		tmpcr->cr_ruid = savecr->cr_uid;
		tmpcr->cr_rgid = savecr->cr_gid;
		set_current_cred(tmpcr);
	}
	vp = VF_TO_VNODE(fp);

	cr = get_current_cred();
	if (mode)
		VOP_ACCESS(vp, mode, cr, error);

	/*
	 * The above VOP_ACCESS would be sufficient if not for it's
	 * being skipped if mode is zero.
	 */
	if (!error && !mode)
		error = _MAC_VACCESS(vp, cr, MACREAD);

	if (!error && exok) {
		vattr.va_mask = AT_MODE;
		VOP_GETATTR(vp, &vattr, 0, cr, error);
		if ((!error) && ((vp->v_type != VREG) ||
		    ((vattr.va_mode & (VEXEC|(VEXEC>>3)|(VEXEC>>6))) == 0)))
			error = EACCES;
	}
	if (!eok) {
		set_current_cred(savecr);
		crfree(tmpcr);
	}
	_SAT_ACCESS(SAT_FILE_ATTR_READ, error);
        return error;
}
#endif

struct xstatarg {
	sysarg_t version;
	char *fname;
	void *sb;
};

int
xstat(struct xstatarg *uap)
{
	vnode_t		*vp;
	register int	error;
	int		statvers;

	_SAT_PN_BOOK( SAT_FILE_ATTR_READ , curuthread);
	error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return error;

	/*
	 * Check version.
	 */
	statvers = uap->version;
	switch (statvers) {
	case _STAT_VER:
	case _STAT64_VER:
		error = xcstat(vp, uap->sb, statvers, 0, get_current_cred());
		break;
	default:
		error = EINVAL;
	}

	VN_RELE(vp);
	_SAT_ACCESS( SAT_FILE_ATTR_READ, error );
	return error;
}

int
lxstat(struct xstatarg *uap)
{
	vnode_t		*vp;
	register int	error;
	int		statvers;

	_SAT_PN_BOOK( SAT_FILE_ATTR_READ , curuthread);
	error = lookupname(uap->fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return error;

	/*
	 * Check version.
	 */
	statvers = uap->version;
	switch (statvers) {
	case _STAT_VER:
	case _STAT64_VER:
		error = xcstat(vp, uap->sb, statvers, 0, get_current_cred());
		break;
	default:
		error = EINVAL;
	}

	VN_RELE(vp);
	_SAT_ACCESS( SAT_FILE_ATTR_READ, error );
	return error;
}

struct fxstatarg {
	sysarg_t version;
	sysarg_t fdes;
	void *sb;
};

int
fxstat(struct fxstatarg *uap)
{
	vfile_t		*fp;
	register int	error;
	int		statvers;
	void		*obj;

	if (error = getf((int)uap->fdes, &fp))
		return error;

	if (VF_IS_VNODE(fp))
		obj = (void *) VF_TO_VNODE(fp);
	else
		obj = (void *) VF_TO_VSOCK(fp);

	statvers = uap->version;
	switch (statvers) {
	case _STAT_VER:
	case _STAT64_VER:
		error = xcstat(obj, uap->sb, statvers, fp->vf_flag,
			       get_current_cred());
		break;
	default:
		error = EINVAL;
	}

	_SAT_FD_READ( uap->fdes, error );
	return error;
}


#if _MIPS_SIM == _ABI64
static int
vattr_to_irix5_64_stat(
	vnode_t 	*vp,
	struct vattr	*vattr,
	void		*ubp)
{
	struct stat		sb;
	register struct vfssw	*vswp;
	register int		error = 0;
	long			*zerop;

	sb.st_mode = VTTOIF(vattr->va_type) | vattr->va_mode;
	sb.st_uid = vattr->va_uid;
	sb.st_gid = vattr->va_gid;
	sb.st_dev = vattr->va_fsid;
	sb.st_ino = vattr->va_nodeid;
	sb.st_nlink = vattr->va_nlink;
	sb.st_size = vattr->va_size;
	sb.st_atim = vattr->va_atime;
	sb.st_ctim = vattr->va_ctime;
	sb.st_mtim = vattr->va_mtime;
	sb.st_rdev = vattr->va_rdev;
	sb.st_blksize = vattr->va_blksize;
	sb.st_blocks = vattr->va_nblocks;
	sb.st_projid = vattr->va_projid;

	/*
	 * Zero out all the pad fields and the fstype field
	 * so that the user can't see unitialized kernel data.
	 * This should be quicker than the old way which was
	 * to bzero the entire stat structure at the beginning
	 * of the routine.
	 *
	 * We unroll the necessary loops here ourselves since
	 * loop unrolling is turned off in the compiler and the
	 * code actually comes out shorter this way (only the
	 * 8 long loop would be shorter.
	 */
	ASSERT(_ST_PAD1SZ == 3);
	sb.st_pad1[0] = 0L;
	sb.st_pad1[1] = 0L;
	sb.st_pad1[2] = 0L;

	ASSERT(_ST_PAD2SZ == 2);
	sb.st_pad2[0] = 0L;
	sb.st_pad2[1] = 0L;

	sb.st_pad3 = 0L;

	ASSERT(_ST_PAD4SZ == 7);
	sb.st_pad4[0] = 0L;
	sb.st_pad4[1] = 0L;
	sb.st_pad4[2] = 0L;
	sb.st_pad4[3] = 0L;
	sb.st_pad4[4] = 0L;
	sb.st_pad4[5] = 0L;
	sb.st_pad4[6] = 0L;

	zerop = (long *)sb.st_fstype;
	ASSERT((_ST_FSTYPSZ / sizeof(long)) == 2);
	zerop[0] = 0L;
	zerop[1] = 0L;

	if (vp && vp->v_vfsp) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			strcpy(sb.st_fstype, vswp->vsw_name);

	}
	if (copyout(&sb, ubp, sizeof(sb)))
		error = EFAULT;
	return error;
}
#endif	/* _ABI64 */


static int
vattr_to_irix5_stat(
	vnode_t		*vp,
	struct vattr	*vattr,
	void	*ubp)
{
	struct irix5_stat	sb;
	register struct vfssw	*vswp;
	register int		error = 0;
	app32_long_t		*zerop;

	sb.st_mode = VTTOIF(vattr->va_type) | vattr->va_mode;
	sb.st_uid = vattr->va_uid;
	sb.st_gid = vattr->va_gid;
	sb.st_dev = vattr->va_fsid;
	sb.st_ino = vattr->va_nodeid;
	if (sb.st_ino != vattr->va_nodeid) 	/* truncated! */
		return EOVERFLOW;
	sb.st_nlink = vattr->va_nlink;
	sb.st_size = vattr->va_size;
	TIMESPEC_TO_IRIX5(&vattr->va_atime, &sb.st_atime);
	TIMESPEC_TO_IRIX5(&vattr->va_ctime, &sb.st_ctime);
	TIMESPEC_TO_IRIX5(&vattr->va_mtime, &sb.st_mtime);
	sb.st_rdev = vattr->va_rdev;
	sb.st_blksize = vattr->va_blksize;
	sb.st_blocks = vattr->va_nblocks;
	if (sb.st_blocks != vattr->va_nblocks)	/* truncated */
		sb.st_blocks = INT_MAX;
	sb.st_projid = vattr->va_projid;
	
	/*
	 * Zero out all the pad fields and the fstype field
	 * so that the user can't see unitialized kernel data.
	 * This should be quicker than the old way which was
	 * to bzero the entire stat structure at the beginning
	 * of the routine.
	 *
	 * We unroll the necessary loops here ourselves since
	 * loop unrolling is turned off in the compiler and the
	 * code actually comes out shorter this way (only the
	 * 8 long loop would be shorter.
	 */
	ASSERT(_ST_PAD1SZ == 3);
	sb.st_pad1[0] = 0;
	sb.st_pad1[1] = 0;
	sb.st_pad1[2] = 0;

	ASSERT(_ST_PAD2SZ == 2);
	sb.st_pad2[0] = 0;
	sb.st_pad2[1] = 0;

	sb.st_pad3 = 0;

	ASSERT(_ST_PAD4SZ == 7);
	sb.st_pad4[0] = 0;
	sb.st_pad4[1] = 0;
	sb.st_pad4[2] = 0;
	sb.st_pad4[3] = 0;
	sb.st_pad4[4] = 0;
	sb.st_pad4[5] = 0;
	sb.st_pad4[6] = 0;

	zerop = (app32_long_t *)sb.st_fstype;
	ASSERT((_ST_FSTYPSZ / sizeof(app32_long_t)) == 4);
	zerop[0] = 0;
	zerop[1] = 0;
	zerop[2] = 0;
	zerop[3] = 0;

	if (vp && vp->v_vfsp) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			strcpy(sb.st_fstype, vswp->vsw_name);

	}
	if (copyout(&sb, ubp, sizeof(sb)))
		error = EFAULT;
	return error;
}

static int
vattr_to_irix5_n32_stat(
	vnode_t		*vp,
	struct vattr	*vattr,
	void		*ubp)
{
	struct irix5_n32_stat	sb;
	register struct vfssw	*vswp;
	register int		error = 0;
	app32_long_t		*zerop;

	sb.st_mode = VTTOIF(vattr->va_type) | vattr->va_mode;
	sb.st_uid = vattr->va_uid;
	sb.st_gid = vattr->va_gid;
	sb.st_dev = vattr->va_fsid;
	sb.st_ino = vattr->va_nodeid;
	sb.st_nlink = vattr->va_nlink;
	sb.st_size = vattr->va_size;
	TIMESPEC_TO_IRIX5(&vattr->va_atime, &sb.st_atime);
	TIMESPEC_TO_IRIX5(&vattr->va_ctime, &sb.st_ctime);
	TIMESPEC_TO_IRIX5(&vattr->va_mtime, &sb.st_mtime);
	sb.st_rdev = vattr->va_rdev;
	sb.st_blksize = vattr->va_blksize;
	sb.st_blocks = vattr->va_nblocks;
	sb.st_projid = vattr->va_projid;

	/*
	 * Zero out all the pad fields and the fstype field
	 * so that the user can't see unitialized kernel data.
	 * This should be quicker than the old way which was
	 * to bzero the entire stat structure at the beginning
	 * of the routine.
	 *
	 * We unroll the necessary loops here ourselves since
	 * loop unrolling is turned off in the compiler and the
	 * code actually comes out shorter this way (only the
	 * 8 long loop would be shorter.
	 */
	ASSERT(_ST_PAD1SZ == 3);
	sb.st_pad1[0] = 0;
	sb.st_pad1[1] = 0;
	sb.st_pad1[2] = 0;

	ASSERT(_ST_PAD2SZ == 2);
	sb.st_pad2[0] = 0;
	sb.st_pad2[1] = 0;

	sb.st_pad3 = 0;

	ASSERT(_ST_PAD4SZ == 7);
	sb.st_pad4[0] = 0;
	sb.st_pad4[1] = 0;
	sb.st_pad4[2] = 0;
	sb.st_pad4[3] = 0;
	sb.st_pad4[4] = 0;
	sb.st_pad4[5] = 0;
	sb.st_pad4[6] = 0;

	zerop = (app32_long_t *)sb.st_fstype;
	ASSERT((_ST_FSTYPSZ / sizeof(app32_long_t)) == 4);
	zerop[0] = 0;
	zerop[1] = 0;
	zerop[2] = 0;
	zerop[3] = 0;

	if (vp && vp->v_vfsp) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			strcpy(sb.st_fstype, vswp->vsw_name);

	}
	if (copyout(&sb, ubp, sizeof(sb)))
		error = EFAULT;
	return error;
}

static int
vattr_to_irix5_stat64(
	vnode_t		*vp,
	struct vattr	*vattr,
	void		*ubp)
{
	struct irix5_stat64	sb;
	register struct vfssw	*vswp;
	register int		error = 0;
	app32_long_t		*zerop;

	sb.st_mode = VTTOIF(vattr->va_type) | vattr->va_mode;
	sb.st_uid = vattr->va_uid;
	sb.st_gid = vattr->va_gid;
	sb.st_dev = vattr->va_fsid;
	sb.st_ino = vattr->va_nodeid;
	sb.st_nlink = vattr->va_nlink;
	sb.st_size = vattr->va_size;
	TIMESPEC_TO_IRIX5(&vattr->va_atime, &sb.st_atime);
	TIMESPEC_TO_IRIX5(&vattr->va_ctime, &sb.st_ctime);
	TIMESPEC_TO_IRIX5(&vattr->va_mtime, &sb.st_mtime);
	sb.st_rdev = vattr->va_rdev;
	sb.st_blksize = vattr->va_blksize;
	sb.st_blocks = vattr->va_nblocks;
	sb.st_projid = vattr->va_projid;

	/*
	 * Zero out all the pad fields and the fstype field
	 * so that the user can't see unitialized kernel data.
	 * This should be quicker than the old way which was
	 * to bzero the entire stat structure at the beginning
	 * of the routine.
	 *
	 * We unroll the necessary loops here ourselves since
	 * loop unrolling is turned off in the compiler and the
	 * code actually comes out shorter this way (only the
	 * 8 long loop would be shorter.
	 */
	ASSERT(_ST_PAD1SZ == 3);
	sb.st_pad1[0] = 0;
	sb.st_pad1[1] = 0;
	sb.st_pad1[2] = 0;

	ASSERT(_ST_PAD2SZ == 2);
	sb.st_pad2[0] = 0;
	sb.st_pad2[1] = 0;

	sb.st_pad3 = 0;

	ASSERT(_ST_PAD4SZ == 7);
	sb.st_pad4[0] = 0;
	sb.st_pad4[1] = 0;
	sb.st_pad4[2] = 0;
	sb.st_pad4[3] = 0;
	sb.st_pad4[4] = 0;
	sb.st_pad4[5] = 0;
	sb.st_pad4[6] = 0;

	zerop = (app32_long_t *)sb.st_fstype;
	ASSERT((_ST_FSTYPSZ / sizeof(app32_long_t)) == 4);
	zerop[0] = 0;
	zerop[1] = 0;
	zerop[2] = 0;
	zerop[3] = 0;

	if (vp && vp->v_vfsp) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			strcpy(sb.st_fstype, vswp->vsw_name);

	}
	if (copyout(&sb, ubp, sizeof(sb)))
		error = EFAULT;
	return error;
}

int
xcstat(void *obj, void *ubp, int statvers, int fflag, cred_t *cr)
{
	vattr_t			vattr;
	int			error;
	int			abi;
	vnode_t			*vp;
	extern mac_label	*mac_equal_equal_lp; /*XXX:casey-temporary*/

	vattr.va_mask = AT_STAT;
	vattr.va_projid = (int)dfltprid;
	if (fflag & FSOCKET) {
		vp = NULL;
		VSOP_FSTAT(((vsock_t *)obj), &vattr, 0, cr, error);
	} else {
		vp = (vnode_t *)obj;
		VOP_GETATTR(vp, &vattr, 0, cr, error);
		if (!error)
			error = _MAC_VACCESS(vp, cr, VREAD);
	}
	if (error)
		return error;


	if (statvers == _STAT64_VER)
		return vattr_to_irix5_stat64(vp, &vattr, ubp);

	abi = get_current_abi();
	/*
	 * If the file is too large to return its proper size,
	 * then set the file size returned to the maximum size.
	 * n32 and n64 stat structures can represent all sizes.
	 */
	if (!(ABI_IS_IRIX5_64(abi) || ABI_IS_IRIX5_N32(abi)) &&
	    (vattr.va_size > SEEKLIMIT32)) {
		vattr.va_size = SEEKLIMIT32;
	}

	if (ABI_IS_IRIX5(abi))
		return vattr_to_irix5_stat(vp, &vattr, ubp);
	else if (ABI_IS_IRIX5_N32(abi))
		return vattr_to_irix5_n32_stat(vp, &vattr, ubp);
#if _MIPS_SIM == _ABI64
	return vattr_to_irix5_64_stat(vp, &vattr, ubp);
#else
	ASSERT(0);
	return EINVAL;
#endif

}

struct getmountidargs {
	char *fname;
	void *mountidp;
};

/*
 * This call is used to obtain the fsid of the filesystem in which
 * the named file resides.  It is used to improve the robustness of
 * getcwd() in the face of mount points of filesystem types that can
 * hang (see the getcwd() source in libc).
 */
int
getmountid(struct getmountidargs *uap)
{
	vnode_t		*vp;
	int		error;
	mountid_t	mntid;
	

	_SAT_PN_BOOK( SAT_FILE_ATTR_READ , curuthread);
	error = lookupname(uap->fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return error;

	mntid.val[0] = vp->v_vfsp->vfs_fsid.val[0];
	mntid.val[1] = vp->v_vfsp->vfs_fsid.val[1];
	mntid.val[2] = 0;
	mntid.val[3] = 0;
	if (copyout(&(mntid), uap->mountidp, sizeof(mountid_t))) {
		error = EFAULT;
	}

	VN_RELE(vp);
	_SAT_ACCESS( SAT_FILE_ATTR_READ, error );
	return error;
}


static int	cpathconf(void *, int, rval_t *, int, struct cred *);

/* fpathconf/pathconf interfaces */

int
fpathconf(struct fpathconfa *uap, rval_t *rvp)
{
	vfile_t *fp;
	void	*obj;
	register int error;

	if (error = getf((int)uap->fdes, &fp))
		return error;

	if (VF_IS_VNODE(fp))
		obj = (void *)VF_TO_VNODE(fp);
	else
		obj = (void *)VF_TO_VSOCK(fp);
		
	return cpathconf(obj, (int)uap->name, rvp, fp->vf_flag,
			 get_current_cred());
}

int
pathconf(struct pathconfa *uap, rval_t *rvp)
{
	vnode_t *vp;
	register int error;

	_SAT_PN_BOOK(SAT_FILE_ATTR_READ, curuthread);
	error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return error;
	error = cpathconf(vp, (int)uap->name, rvp, 0, get_current_cred());
	VN_RELE(vp);
	return error;
}

/*
 * Common code for pathconf(), fpathconf() system calls
 */
static int
cpathconf(void *obj, int cmd, rval_t *rvp, int fflag, struct cred *cr)
{
	register int error;
	long val;

	if (fflag & FSOCKET) {
		switch (cmd) {

		case _PC_NAME_MAX:
		case _PC_NO_TRUNC:
		case _PC_PATH_MAX:
		case _PC_FILESIZEBITS:
			error = EINVAL;
			break;

		default:
			VSOP_PATHCONF(((vsock_t *)obj), cmd, &val, cr, error);
			break;
		}
	} else
		VOP_PATHCONF((vnode_t *)obj, cmd, &val, cr, error);
	if (!error)
		rvp->r_val1 = val;

	return error;
}

/*
 * Read the contents of a symbolic link.
 */
struct readlinka {
	char *name;
	char *buf;
	sysarg_t count;
};

int
readlink(struct readlinka *uap, rval_t *rvp)
{
	vnode_t *vp;
	struct iovec aiov;
	struct uio auio;
	int error;

	_SAT_PN_BOOK(SAT_READ_SYMLINK, curuthread);
	error = lookupname(uap->name, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return error;

	if (error = _MAC_VACCESS(vp, get_current_cred(), MACREAD))
		goto out;

	if (vp->v_type != VLNK) {
		error = EINVAL;
		goto out;
	}

	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_fmode = 0;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = uap->count;
	auio.uio_sigpipe = 0;
	auio.uio_pmp = NULL;
        auio.uio_pio = 0;
	auio.uio_readiolog = 0;
	auio.uio_writeiolog = 0;
        auio.uio_pbuf = 0;
	VOP_READLINK(vp, &auio, get_current_cred(), error);
out:
	VN_RELE(vp);
	rvp->r_val1 = uap->count - auio.uio_resid;
	_SAT_ACCESS(SAT_READ_SYMLINK, error);
	return error;
}

/*
 * Change mode of file given path name.
 */
struct chmoda {
	char *fname;
	usysarg_t fmode;
};

/* ARGSUSED */
int
chmod(struct chmoda *uap, rval_t *rvp)
{
	struct vattr vattr;
	int error;

	vattr.va_mode = (mode_t)(uap->fmode & MODEMASK);
	vattr.va_mask = AT_MODE;

	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);
	error = namesetattr(uap->fname, FOLLOW, &vattr, 0);
	_SAT_CHMOD(uap->fmode, error);
	return error;
}

/*
 * Change mode of file given file descriptor.
 */
struct fchmoda {
	sysarg_t fd;
	usysarg_t fmode;
};

/* ARGSUSED */
int
fchmod(struct fchmoda *uap, rval_t *rvp)
{
	struct vattr vattr;
	int error;

	vattr.va_mode = (mode_t)(uap->fmode & MODEMASK);
	vattr.va_mask = AT_MODE;
	error = fdsetattr((int)uap->fd, &vattr, 0);
	_SAT_FCHMOD((int)uap->fd,vattr.va_mode,error);
	return error;
}

/*
 * Change ownership of file given file name.
 */
struct chowna {
	char *fname;
	sysarg_t uid;
	sysarg_t gid;
};

/* ARGSUSED */
int
chown(struct chowna *uap, rval_t *rvp)
{
	struct vattr vattr;
	int error;

	if (uap->uid < -1 || uap->uid > MAXUID ||
	    uap->gid < -1 || uap->gid > MAXUID)
		return EINVAL;
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	vattr.va_mask = 0;
	if (vattr.va_uid != (uid_t)-1)
		vattr.va_mask |= AT_UID;
	if (vattr.va_gid != (gid_t)-1)
		vattr.va_mask |= AT_GID;

	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);
	error = namesetattr(uap->fname, FOLLOW, &vattr, 0);
	_SAT_CHOWN(uap->uid, uap->gid, error);
	return error;
}

/* ARGSUSED */
int
lchown(struct chowna *uap, rval_t *rvp)
{
	struct vattr vattr;
	int error;

	if (uap->uid < -1 || uap->uid > MAXUID ||
	    uap->gid < -1 || uap->gid > MAXUID)
		return EINVAL;
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	vattr.va_mask = 0;
	if (vattr.va_uid != (uid_t)-1)
		vattr.va_mask |= AT_UID;
	if (vattr.va_gid != (gid_t)-1)
		vattr.va_mask |= AT_GID;

	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);
	error = namesetattr(uap->fname, NO_FOLLOW, &vattr, 0);
	_SAT_CHOWN(uap->uid, uap->gid, error);
	return error;
}

/*
 * Change ownership of file given file descriptor.
 */
struct fchowna {
	sysarg_t fd;
	sysarg_t uid;
	sysarg_t gid;
};

/* ARGSUSED */
int
fchown(struct fchowna *uap, rval_t *rvp)
{
	struct vattr vattr;
	int error;

	if (uap->uid < -1 || uap->uid > MAXUID ||
	    uap->gid < -1 || uap->gid > MAXUID)
		return EINVAL;
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	vattr.va_mask = 0;
	if (vattr.va_uid != (uid_t)-1)
		vattr.va_mask |= AT_UID;
	if (vattr.va_gid != (gid_t)-1)
		vattr.va_mask |= AT_GID;
	error = fdsetattr((int)uap->fd, &vattr, 0);
	_SAT_FCHOWN((int)uap->fd,vattr.va_uid,vattr.va_gid,error);
	return error;
}

/* ARGSUSED */
int
chproj(char *fname, enum symfollow followlink, sysarg_t p, rval_t *rvp)
{
	struct vattr 	vattr;
	int 		error;
	prid_t		prid;

	/* Make sure user is eligible to change project ID */
	/* XXX Need better capability here?? */
	if (!_CAP_ABLE(CAP_SETUID))
		return EPERM;

	if (p < 0)
		return EINVAL;

	if (copyin((caddr_t)p, &prid, sizeof(prid_t)))
		    return EFAULT;
	/*
	 * Inode keeps the prid as an __uint16_t.
	 * Hence, the limitation. There's no good reason
	 * to allow for larger proj ids, IMO.
	 */
	if ((prid & 0xffff) != prid)
		return EOVERFLOW;
	
	vattr.va_projid = (int)prid;
	vattr.va_mask = AT_PROJID;

	/* _SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread); */
	error = namesetattr(fname, followlink, &vattr, 0);
	/* _SAT_CHPROJ(uap->uid, uap->gid, error); */
	return error;
}

/* ARGSUSED */
int
fchproj(int fd, sysarg_t p, rval_t *rvp)
{
	struct vattr 	vattr;
	int 		error;
	prid_t		prid;

	if (!_CAP_ABLE(CAP_SETUID))
		return EPERM;
	if (p < 0)
		return EINVAL;
	if (copyin((caddr_t)p, &prid, sizeof(prid_t)))
		    return EFAULT;
	if ((prid & 0xffff) != prid)
		return EOVERFLOW;

	vattr.va_projid = (int)prid;
	vattr.va_mask = AT_PROJID;

	/* _SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread); */
	error = fdsetattr(fd, &vattr, 0);
	/* _SAT_FCHOWN((int)fd,vattr.va_uid,vattr.va_gid,error); */
	return error;
}

/*
 * Truncate or extend a file by name and descriptor.
 */
static int	ctruncate(vnode_t *, off_t, int);

struct irix5_truncate64a {
	char		*name;
	sysarg_t	pad0;
	sysarg_t	size1;
	sysarg_t	size2;
};

/*
 * n32 and n64 binaries are directed to truncate by libc.  But since
 * o32 and n32 kernels do not save 64 bit arg registers, n32 truncates are
 * sent back here by truncate.
 */
/*ARGSUSED*/
int
truncate64(struct irix5_truncate64a *uap, rval_t *rvp)
{
	int error;
	vnode_t *vp;
	off_t size;

	ASSERT(ABI_IS(get_current_abi(), (ABI_IRIX5_N32|ABI_IRIX5)));

	_SAT_PN_BOOK(SAT_FILE_WRITE, curuthread);
	if (error = lookupname(uap->name, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL))
		return error;
	if (vp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
	} else {
		size = ((((off_t)(uap->size1) & 0xffffffff) << 32) |
			(uap->size2 & 0xffffffff));
		error = ctruncate(vp, size, 1);
	}
	VN_RELE(vp);
	_SAT_ACCESS(SAT_FILE_WRITE, error);
	return error;
}

struct truncatea {
	char *name;
	sysarg_t size;
};

/*ARGSUSED*/
int
truncate(struct truncatea *uap, rval_t *rvp)
{
	int error;
	vnode_t *vp;

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABIN32)
	if (ABI_IS_IRIX5_N32(get_current_abi()))
		return(truncate64((struct irix5_truncate64a *) uap, rvp));
#endif

	_SAT_PN_BOOK(SAT_FILE_WRITE, curuthread);
	if (error = lookupname(uap->name, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL))
		return error;
	if (vp->v_vfsp->vfs_flag & VFS_RDONLY)
		error = EROFS;
	else
		error = ctruncate(vp, (off_t)uap->size, 1);
	VN_RELE(vp);
	_SAT_ACCESS(SAT_FILE_WRITE, error);
	return error;
}

struct irix5_ftruncate64a {
	sysarg_t	fdes;
	sysarg_t	pad0;
	sysarg_t	size1;
	sysarg_t	size2;
};

/*
 * n32 and n64 binaries are directed to ftruncate by libc.  But since
 * o32 and n32 kernels do not save 64 bit arg registers, n32 ftruncates are
 * sent back here by ftruncate.
 */
/*ARGSUSED*/
int
ftruncate64(struct irix5_ftruncate64a *uap, rval_t *rvp)
{
	int error;
	vfile_t *fp;
	vnode_t *vp;
	off_t size;

	ASSERT(ABI_IS(get_current_abi(), (ABI_IRIX5_N32|ABI_IRIX5)));

	if (error = getf((int)uap->fdes, &fp)) {
		_SAT_FD_RDWR((int)uap->fdes,FWRITE,error);
		return error;
	}

	if ((fp->vf_flag & FWRITE) == 0) {
		_SAT_FD_RDWR((int)uap->fdes,FWRITE,EBADF);
		return EBADF;
	}

	if (!VF_IS_VNODE(fp)) {
		_SAT_FD_RDWR((int)uap->fdes,FWRITE,EINVAL);
		return EINVAL;
	}

	vp = VF_TO_VNODE(fp);

	if (vp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
	} else {
		size = ((((off_t)(uap->size1) & 0xffffffff) << 32)
				| (uap->size2 & 0xffffffff));
		error = ctruncate(vp, size, 0);
	}

	_SAT_FD_RDWR((int)uap->fdes, FWRITE, error);
	return error;
}

struct ftruncatea {
	sysarg_t fdes;
	sysarg_t size;
};

/*ARGSUSED*/
int
ftruncate(struct ftruncatea *uap, rval_t *rvp)
{
	int error;
	vfile_t *fp;
	vnode_t *vp;

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABIN32)
	if (ABI_IS_IRIX5_N32(get_current_abi()))
		return(ftruncate64((struct irix5_ftruncate64a *) uap, rvp));
#endif

	if (error = getf((int)uap->fdes, &fp)) {
		_SAT_FD_RDWR((int)uap->fdes,FWRITE,error);
		return error;
	}
	if ((fp->vf_flag & FWRITE) == 0) {
		_SAT_FD_RDWR((int)uap->fdes,FWRITE,EBADF);
		return EBADF;
	}
	if (!VF_IS_VNODE(fp)) {
		_SAT_FD_RDWR((int)uap->fdes, FWRITE, EINVAL);
		return EINVAL;
	}

	vp = VF_TO_VNODE(fp);

	if (vp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
	} else {
		error = ctruncate(vp, (off_t)uap->size, 0);

	}

	_SAT_FD_RDWR((int)uap->fdes, FWRITE, error);
	return error;
}

static int
ctruncate(vnode_t *vp, off_t size, int checkperm)
{
	struct vattr vattr;
	int rv;

	if (vp->v_type == VDIR)
		return EISDIR;

	if (vp->v_type != VREG)
		return EINVAL;

	if (size < (off_t)0)
		return EINVAL;

	if (getfsizelimit() < size)
		return EFBIG;

	vattr.va_size = size;
	vattr.va_mask = AT_SIZE | (checkperm ? 0 : AT_SIZE_NOPERM);

	VOP_SETATTR(vp, &vattr, 0, get_current_cred(), rv);

	return(rv);
}


/*
 * Set access/modify times on named file.
 */
struct utimea {
	char *fname;
	time_t *tptr;
};

/* ARGSUSED */
int
utime(struct utimea *uap, rval_t *rvp)
{
	time_t tv[2];
	struct vattr vattr;
	int flags = 0;
	int error;

	if (uap->tptr != NULL) {
		if (copyin(uap->tptr, tv, sizeof(tv)))
			return EFAULT;
		flags |= ATTR_UTIME;
		vattr.va_atime.tv_sec = tv[0];
		vattr.va_atime.tv_nsec = 0;
		vattr.va_mtime.tv_sec = tv[1];
		vattr.va_mtime.tv_nsec = 0;
	} else {
		nanotime_syscall(&vattr.va_atime);
		vattr.va_mtime = vattr.va_atime;
		tv[0] = vattr.va_atime.tv_sec;
		tv[1] = vattr.va_mtime.tv_sec;
	}
	vattr.va_mask = AT_ATIME|AT_MTIME;

	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);
	error = namesetattr(uap->fname, FOLLOW, &vattr, flags);
	_SAT_UTIME(uap->tptr, tv[0], tv[1], error);
	return error;
}

/*
 * Common routine for modifying attributes of named files.
 */
int
namesetattr(char *fnamep, enum symfollow followlink, struct vattr *vap,
	    int flags)
{
	vnode_t *vp;
	register int error;

	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);
	if (error = lookupname(fnamep, UIO_USERSPACE, followlink, NULLVPP, &vp, NULL))
		return error;
	if (vp->v_vfsp->vfs_flag & VFS_RDONLY)
		error = EROFS;
	else
	{
#ifdef _SHAREII
		/*
		 *	ShareII allows the limiting of the permission
		 *	bits settable by a user, here that is enforced.
		 */
		if (error = SHR_SETATTR(vap->va_mask, &vap->va_mode)) {
			VN_RELE(vp);
			return error;
		}

#endif /* _SHAREII */
		VOP_SETATTR(vp, vap, flags, get_current_cred(), error);
	}
	VN_RELE(vp);
	return error;
}

/*
 * Common routine for modifying attributes of files referenced
 * by descriptor.
 */
int
fdsetattr(int fd, struct vattr *vap, int flags)
{
	vfile_t *fp;
	register vnode_t *vp;
	register int error;

	if ((error = getf(fd, &fp)) == 0) {
		if (!VF_IS_VNODE(fp))
			return ENOSYS;

		vp = VF_TO_VNODE(fp);
		if (vp->v_vfsp->vfs_flag & VFS_RDONLY)
			return EROFS;

#ifdef _SHAREII
		/*
		 *	ShareII allows the limiting of the permission
		 *	bits settable by a user, here that is enforced.
		 */
		if (error = SHR_SETATTR(vap->va_mask, &vap->va_mode))
			return error;
#endif /* _SHAREII */
		VOP_SETATTR(vp, vap, flags, get_current_cred(), error);
	}
	return error;
}

/*
 * Flush output pending for file.
 */
struct fsynca {
	sysarg_t fd;
};

/* ARGSUSED */
int
fsync(struct fsynca *uap, rval_t *rvp)
{
	vfile_t *fp;
	register int error;

	if (error = getf((int)uap->fd, &fp)) {
		_SAT_FD_READ( uap->fd, error );
		return error;
	}
	if ((fp->vf_flag & FWRITE) == 0) {
		_SAT_FD_READ( uap->fd, EBADF );
		return EBADF;
	}
	if (!VF_IS_VNODE(fp)) {
		_SAT_FD_READ( uap->fd, EINVAL );
		return EINVAL;
	}
	VOP_FSYNC(VF_TO_VNODE(fp), FSYNC_WAIT, get_current_cred(),
				(off_t)0, (off_t)-1, error);
	_SAT_FD_READ( uap->fd, error );
	return error;
}
/* ARGSUSED */
int
fdatasync(struct fsynca *uap, rval_t *rvp)
{
	vfile_t *fp;
	register int error;

	if (error = getf((int)uap->fd, &fp))
		return error;
	if ((fp->vf_flag & FWRITE) == 0)
		return EBADF;
	if (!VF_IS_VNODE(fp))
		return EINVAL;
	VOP_FSYNC(VF_TO_VNODE(fp), FSYNC_WAIT | FSYNC_DATA,
		  get_current_cred(), (off_t)0, (off_t)-1, error);
	return(error);
}

#if (_MIPS_SIM == _ABI64)
int irix5_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5(void *, int, xlate_info_t *);
int irix5_n32_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5_n32(void *, int, xlate_info_t *);
#endif
/*
 * File control.
 */

struct fcntla {
	sysarg_t fdes;
	sysarg_t cmd;
	sysarg_t arg;
};

int
fcntl(struct fcntla *uap, rval_t *rvp)
{
	vfile_t *fp;
	register int i, error;
	vnode_t *vp;
	vsock_t *vs;
	off_t offset;
	off_t stop;
	int fsflag, flag, fd;
	int cmd;
	struct flock bf;
	struct irix5_flock i5_bf;
	int abi = get_current_abi();

	/* record command for sat */
	_SAT_SET_SUBSYSNUM(uap->cmd);

	if (error = getf(uap->fdes, &fp)) {
		_SAT_FD_READ(uap->fdes,error);
		return error;
	}
	if (VF_IS_VNODE(fp)) {
		VFILE_GETOFFSET(fp, &offset);
		vp = VF_TO_VNODE(fp);
	} else
		vs = VF_TO_VSOCK(fp);

	flag = fp->vf_flag;

	switch (uap->cmd) {
	case F_DUPFD:
		if ((i = uap->arg) < 0)
			error = EINVAL;
		else if ((error = fdt_dup(i, fp, &fd)) == 0)
			rvp->r_val1 = fd;
		break;

	case F_GETFD:
		rvp->r_val1 = fdt_getflags(uap->fdes);
		break;

	case F_SETFD:
		fdt_setflags(uap->fdes, (char)uap->arg);
		break;

	case F_GETFL:
		rvp->r_val1 = fp->vf_flag+FOPEN;
		break;

	case F_SETFL:
		if ((uap->arg & (FNONBLOCK|FNDELAY)) == (FNONBLOCK|FNDELAY))
			uap->arg &= ~FNDELAY;
		if (VF_IS_VNODE(fp)) {
			VOP_SETFL(vp, flag, uap->arg, get_current_cred(), 
				  error);
		} else {
			VSOP_SETFL(vs, flag, uap->arg, get_current_cred(), 
				   error);
		}
		if (!error) {
			uap->arg &= FMASK;
			VFILE_SETFLAGS(fp, (FREAD|FWRITE|FSOCKET|FPRIORITY|FPRIO),
				       (uap->arg-FOPEN) & ~(FREAD|FWRITE));
		}
		break;

	case F_FSYNC:
	case F_FSYNC64:
		/*
		 * copyin args, handle multiple abi/offset lengths nonsense.
		 * stolen from the allocsp/allocsp64/freesp/etc. code
		 */
#if (_MIPS_SIM == _ABI64)
		if (ABI_IS_IRIX5_64(abi)) {
			if (copyin((caddr_t)uap->arg, &bf, sizeof(bf))) {
				error = EFAULT;
				break;
			}
		} else if (ABI_IS_IRIX5_N32(abi) || uap->cmd == F_FSYNC64) {
			if (COPYIN_XLATE((caddr_t)uap->arg, &bf, sizeof(bf),
			    irix5_n32_to_flock, abi, 1)) {
				error = EFAULT;
				break;
			}
		} else {
			if (copyin((caddr_t)uap->arg, &i5_bf, sizeof i5_bf)) {
				error = EFAULT;
				break;
			}
			/* Now expand to 64 bit sizes. */
			bf.l_type = i5_bf.l_type;
			bf.l_whence = i5_bf.l_whence;
			bf.l_start = i5_bf.l_start;
			bf.l_len = i5_bf.l_len;
		}
#else /* _ABI64 */
		/*
		 * o32 kernels cannot use the xlate functions
		 * because xlate simply does a copyout and returns if the
		 * caller abi is the same as the kernels.  However,
		 * in this case, the kernel flock size is same the
		 * as n32 flock size.
		 */

		if (ABI_IS_IRIX5_N32(abi) || (uap->cmd == F_FSYNC64)) {
			if (copyin((caddr_t)uap->arg, &bf, sizeof(bf))) {
				error = EFAULT;
				break;
			}
		} else {
			if (copyin((caddr_t)uap->arg, &i5_bf, sizeof i5_bf)) {
				error = EFAULT;
				break;
			}
			/* Now expand to 64 bit sizes. */
			bf.l_type = i5_bf.l_type;
			bf.l_whence = i5_bf.l_whence;
			bf.l_start = i5_bf.l_start;
			bf.l_len = i5_bf.l_len;
		}
#endif /* _ABI64 */

		if (bf.l_start < 0 || bf.l_len < 0)  {
			error = EINVAL;
			break;
		}

		if (bf.l_len == 0)  {
			stop = -1;
		} else if ((stop = bf.l_start + bf.l_len - 1) < 0)  {
			error = EOVERFLOW;
			break;
		}

		fsflag = 0;

		switch (bf.l_type)  {
		case 1:
			fsflag |= FSYNC_DATA;
		case 0:
			fsflag |= FSYNC_WAIT;

			VOP_FSYNC(vp, fsflag, get_current_cred(),
				  bf.l_start, stop, error);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;

	case F_SETLK:
	case F_SETBSDLK:
	case F_SETLKW:
	case F_SETBSDLKW:
	case F_GETLK:
	case F_SETLK64:
	case F_GETLK64:
	case F_SETLKW64:

		if (!VF_IS_VNODE(fp)) {
			error = EBADF;	/* not supported for sockets */
			break;
		}

		/*
		 * Copy in input fields only.
		 * For compatibility we overlay an SVR3 flock on an
		 * SVR4 flock.  This works because the input field
		 * offsets in "struct flock" were preserved.
		 */
		cmd = uap->cmd;

#if (_MIPS_SIM == _ABI64)

		/* This section of code does not use to the
		 * copyin_xlate technology, because it used to have
		 * irix4 compatibility stuff also.  Could rewrite someday...
		 */
		if (ABI_IS_IRIX5_64(abi)) {
			if (copyin((caddr_t)uap->arg, &bf, sizeof(bf))) {
				error = EFAULT;
				break;
			}
			/* Convert back to 32 bit commands so we don't
			 * propogate new commands all the way down.
			 * Be careful to use uap->cmd later if we need
			 * to know the original command.
			 */
			if (cmd == F_SETLK64) {
				cmd = F_SETLK;
			} else if (cmd == F_GETLK64) {
				cmd = F_GETLK;
			} else if (cmd == F_SETLKW64) {
				cmd = F_SETLKW;
			}
		} else if (ABI_IS_IRIX5_N32(abi) ||
			   cmd == F_SETLK64 ||
			   cmd == F_GETLK64 ||
			   cmd == F_SETLKW64) {
			if (COPYIN_XLATE((caddr_t)uap->arg, &bf, sizeof(bf),
			    irix5_n32_to_flock, abi, 1)) {
				error = EFAULT;
				break;
			}
			if (cmd == F_SETLK64) {
				cmd = F_SETLK;
			} else if (cmd == F_GETLK64) {
				cmd = F_GETLK;
			} else if (cmd == F_SETLKW64) {
				cmd = F_SETLKW;
			}
		} else {
			if (copyin((caddr_t)uap->arg, &i5_bf, sizeof i5_bf)) {
				error = EFAULT;
				break;
			}
			/* Now expand to 64 bit sizes. */
			bf.l_type = i5_bf.l_type;
			bf.l_whence = i5_bf.l_whence;
			bf.l_start = i5_bf.l_start;
			bf.l_len = i5_bf.l_len;
		}
#else /* _ABI64 */
		/*
		 * o32 kernels cannot use the xlate functions
		 * because xlate simply does a copyout and returns if the
		 * caller abi is the same as the kernels.  However,
		 * in this case, the kernel flock size is same the
		 * as n32 flock size.
		 */

		if (ABI_IS_IRIX5_N32(abi) ||
		    (cmd == F_SETLK64) ||
		    (cmd == F_GETLK64) ||
		    (cmd == F_SETLKW64)) {
			if (copyin((caddr_t)uap->arg, &bf, sizeof(bf))) {
				error = EFAULT;
				break;
			}
			/*
			 * Convert back to 32 bit commands so we don't
			 * propogate new commands all the way down.
			 * Be careful to use uap->cmd later if we need
			 * to know the original command.
			 */
			if (cmd == F_SETLK64) {
				cmd = F_SETLK;
			} else if (cmd == F_GETLK64) {
				cmd = F_GETLK;
			} else if (cmd == F_SETLKW64) {
				cmd = F_SETLKW;
			}
		} else {
			if (copyin((caddr_t)uap->arg, &i5_bf, sizeof i5_bf)) {
				error = EFAULT;
				break;
			}
			/* Now expand to 64 bit sizes. */
			bf.l_type = i5_bf.l_type;
			bf.l_whence = i5_bf.l_whence;
			bf.l_start = i5_bf.l_start;
			bf.l_len = i5_bf.l_len;
		}
#endif /* _ABI64 */

		bf.l_pid = curuthread->ut_flid.fl_pid;
		bf.l_sysid = curuthread->ut_flid.fl_sysid;

		VOP_FRLOCK(vp, cmd, &bf, flag, offset, VRWLOCK_NONE, 
			   get_current_cred(), error);
		if (error) {
		    
			/*
			 * Translation for backward compatibility.
			 */
			if (error == EAGAIN)
				error = EACCES;
			break;
		}

		/*
		 * If command is GETLK and no lock is found, only
		 * the type field is changed.
		 */
		if (cmd == F_GETLK && bf.l_type == F_UNLCK) {
			if (copyout(&bf.l_type,
				    &((struct flock *)uap->arg)->l_type,
				    sizeof(bf.l_type)))
				error = EFAULT;
			break;
		}

		if (uap->cmd == F_GETLK) {
#if (_MIPS_SIM == _ABI64)
			/*
			 * Copy out SVR4 flock.
			 */
			int i;

			for (i = 0; i < 4; i++)
				bf.l_pad[i] = 0;
			/* have to support the irix5 and irix5_n32
			 * abi's, which have different flock structures.
			 */
			if (ABI_IS_IRIX5(abi)) {
				if (XLATE_COPYOUT(&bf, (caddr_t)uap->arg,
						  sizeof bf,
						  flock_to_irix5,
						  abi, 1))
					error = EFAULT;
			} else if (XLATE_COPYOUT(&bf, (caddr_t)uap->arg,
						sizeof bf,
						flock_to_irix5_n32,
						abi, 1)) {
				error = EFAULT;
			}
#else
			if (ABI_IS_IRIX5_N32(abi)) {
				if (copyout(&bf, (caddr_t) uap->arg,
					    sizeof(bf)))
					error = EFAULT;
			} else {
				int i;

				/* Copyout a 32bit flock structure */
				i5_bf.l_type = bf.l_type;
				i5_bf.l_whence = bf.l_whence;
				i5_bf.l_start = bf.l_start;
				i5_bf.l_len = bf.l_len;
				i5_bf.l_sysid = bf.l_sysid;
				i5_bf.l_pid = bf.l_pid;
				for (i = 0; i < 4; i++)
					i5_bf.pad[i] = 0;
				if (copyout(&i5_bf, (caddr_t)uap->arg, sizeof i5_bf))
					error = EFAULT;
			}
#endif 

		} else if (uap->cmd == F_GETLK64) {
			if (copyout(&bf, (caddr_t)uap->arg, sizeof(bf)))
				error = EFAULT;
		}
		break;

	case F_RSETLK:
	case F_RSETLKW:
	case F_RGETLK:
		if (!_CAP_ABLE(CAP_DAC_OVERRIDE) || !_CAP_ABLE(CAP_MAC_READ) ||
			!_CAP_ABLE(CAP_MAC_WRITE))
				return EPERM;

		if (!VF_IS_VNODE(fp)) {
			error = EBADF;	/* not supported for sockets */
			break;
		}

		/*
		 * EFT only interface, applications cannot use
		 * this interface when _STYPES is defined.
		 * This interface supports an expanded
		 * flock struct--see fcntl.h.
		 */
#if (_MIPS_SIM == _ABI64)
		if (ABI_IS_IRIX5(abi)) {
			if (COPYIN_XLATE((caddr_t)uap->arg, &bf, sizeof bf,
				 	irix5_to_flock, abi, 1)) {
				error = EFAULT;
				break;
			}
		} else if (COPYIN_XLATE((caddr_t)uap->arg, &bf, sizeof bf,
				 	irix5_n32_to_flock,
					abi, 1)) {
			error = EFAULT;
			break;
		}
#else
		if (ABI_IS_IRIX5_N32(abi)) {
			if (copyin((caddr_t) uap->arg, &bf, sizeof (bf))) {
				error = EFAULT;
				break;
			}
		} else {
			if (copyin((caddr_t)uap->arg, &i5_bf, sizeof i5_bf)) {
				error = EFAULT;
				break;
			}
			/* Now expand to 64 bit sizes. */
			bf.l_type = i5_bf.l_type;
			bf.l_whence = i5_bf.l_whence;
			bf.l_start = i5_bf.l_start;
			bf.l_len = i5_bf.l_len;
			bf.l_pid = i5_bf.l_pid;
			bf.l_sysid = i5_bf.l_sysid;
		}
#endif

		VOP_FRLOCK(vp, uap->cmd, &bf, flag, offset, VRWLOCK_NONE, 
			   get_current_cred(), error);
		if (error) {
			/*
			 * Translation for backward compatibility.
			 */
			if (error == EAGAIN)
				error = EACCES;
			break;
		}
		if (uap->cmd == F_RGETLK) {
#if (_MIPS_SIM == _ABI64)
			if (ABI_IS_IRIX5(abi)) {
				if (XLATE_COPYOUT(&bf, (caddr_t)uap->arg,
						sizeof bf,
						flock_to_irix5,
						abi, 1))
					error = EFAULT;
			} else if (XLATE_COPYOUT(&bf, (caddr_t)uap->arg,
						sizeof bf,
						flock_to_irix5_n32,
						abi, 1))
				error = EFAULT;
#else
			if (ABI_IS_IRIX5_N32(abi)) {
				if (copyout(&bf, (caddr_t)uap->arg, sizeof bf))
					error = EFAULT;
			} else {
				/* Copyout a 32bit flock structure */
				i5_bf.l_type = bf.l_type;
				i5_bf.l_whence = bf.l_whence;
				i5_bf.l_start = bf.l_start;
				i5_bf.l_len = bf.l_len;
				i5_bf.l_sysid = bf.l_sysid;
				i5_bf.l_pid = bf.l_pid;
				if (copyout(&i5_bf, (caddr_t)uap->arg, sizeof i5_bf))
					error = EFAULT;
			}
#endif
		}
		break;

	case F_CHKLK:
	case F_CHKLKW:
	case F_CLNLK:
		/* internal use only */
		error = EINVAL;
		break;

	case F_CHKFL:
		/*
		 * This is for internal use only, to allow the vnode layer
		 * to validate a flags setting before applying it.  User
		 * programs can't issue it.
		 */
		error = EINVAL;
		break;

	case F_SETPRIO:
	case F_GETPRIO:
		if (!_CAP_ABLE(CAP_DEVICE_MGT))
			error = EPERM;
		else {
			short prval = (short)uap->arg;

			if (uap->cmd == F_SETPRIO) {
				VFILE_SETPRI(fp, prval);
				if (prval == 0) {
					VFILE_SETFLAGS(fp, ~FPRIO, 0);
				} else {
					VFILE_SETFLAGS(fp, ~0, FPRIO);
				}
			} else {
				if (fp->vf_flag & FPRIO)
					VFILE_GETPRI(fp, prval);
				else
					prval = 0;
				rvp->r_val1 = prval;
			}
		}
		break;

	default:
		if (VF_IS_VNODE(fp)) {
			VOP_FCNTL(vp, uap->cmd, (void *)uap->arg, flag, offset,
				  get_current_cred(), rvp, error);
		} else {
			VSOP_FCNTL(vs, uap->cmd, (void *)uap->arg, rvp, error);
		}
		break;
	}

	_SAT_FD_READ(uap->fdes, error);
	return error;
}

/*
 * Duplicate a file descriptor.
 */
struct dupa {
	sysarg_t fdes;
};

int
dup(struct dupa *uap, rval_t *rvp)
{
	vfile_t *fp;
	register int error;
	int fd;

	if (error = getf((int)uap->fdes, &fp)) {
		_SAT_DUP((int)uap->fdes,0,error);
		return error;
	}
	if (error = fdt_dup(0, fp, &fd)) {
		_SAT_DUP((int)uap->fdes,0,error);
		return error;
	}
	rvp->r_val1 = fd;
	_SAT_DUP((int)uap->fdes,fd,0);
	return 0;
}

/*
 * I/O control.
 */
struct ioctla {
	sysarg_t fdes;
	sysarg_t cmd;
	sysarg_t arg;
};

static int
ioctl_fionread(struct ioctla *uap, vnode_t *vp, vfile_t *fp)
{
	off_t offset;
	struct vattr vattr;
	int error;
	irix5_off_t i5_offset;

	vattr.va_mask = AT_SIZE;
	VOP_GETATTR(vp, &vattr, 0, get_current_cred(), error);
	if (error)
		return error;
	VFILE_GETOFFSET(fp, &offset);
	offset = vattr.va_size - offset;

	if (ABI_IS_IRIX5(get_current_abi())) {
		/* truncate 64 bit off_t to 32 bit */
		i5_offset = offset;
		if (copyout(&i5_offset, (caddr_t)uap->arg, sizeof i5_offset))
			return EFAULT;
	} else 
		if (copyout(&offset, (caddr_t)uap->arg, sizeof offset))
			return EFAULT;

	return 0;
}

int
ioctl(struct ioctla *uap, rval_t *rvp)
{
	vfile_t *fp;
	register int error;
	register vnode_t *vp;
	int flag;
	int rv = 0;
        vopbd_t vbd;

	/* record command for sat */
	_SAT_SET_SUBSYSNUM(uap->cmd);
	_SAT_SET_OPENFD(uap->fdes);

	if (error = getf((int)uap->fdes, &fp))
		return error;
	SYSINFO.sysioctl++;
	if (VF_IS_VSOCK(fp)) {
		VSOP_IOCTL(VF_TO_VSOCK(fp), uap->cmd, (void *)uap->arg, 
			   fp->vf_flag, get_current_cred(), rvp, error);
	} else {
		vp = VF_TO_VNODE(fp);
		if (vp->v_type == VREG || vp->v_type == VDIR) {
			/*
			 * Handle these two ioctls for regular files and
			 * directories.  All others will usually be failed
			 * with ENOTTY by the VFS-dependent code.  System V
			 * always failed all ioctls on regular files, but SunOS
			 * supported these.
			 */
			switch (uap->cmd) {
			case FIONREAD:
				/*
				 * function to reduce stack usage in
				 * deeper paths...
				 */
				return ioctl_fionread(uap,vp,fp);

			case FIONBIO:
				if (copyin((caddr_t)uap->arg, &flag, 
					   sizeof(flag)))
					return EFAULT;

				if (flag) {
					VFILE_SETFLAGS (fp, ~0, FNDELAY);
				} else {
					VFILE_SETFLAGS (fp, ~FNDELAY, 0);
				}
				return 0;

			default:
				break;
			}
		}

		vbd.vopbd_req = VOPBDR_NIL;
		VOP_IOCTL(vp, uap->cmd, (void *)uap->arg, fp->vf_flag,
			  get_current_cred(), &rv, &vbd, error);
		switch (vbd.vopbd_req) {
		case VOPBDR_NIL:
		  	break;

		case VOPBDR_ASSIGN: {
		        vnode_t *xvp = vbd.vopbd_parm.vopbdp_assign.vopbda_vp;
                        int cpi = vbd.vopbd_parm.vopbdp_assign.vopbda_cpi;
                        int n;

		        error = vfile_assign(&xvp, FREAD, &n);
			if (error) {
				VN_RELE(xvp);
			} else {
				rv = n;
#ifdef CKPT
				if (ckpt_enabled)
				        ckpt_setfd(n, cpi);
#endif
			}
			break;
		}

		case VOPBDR_DUP: {
		        vfile_t *vfp = vbd.vopbd_parm.vopbdp_dup.vopbdd_vfp;
                        int srcfd = vbd.vopbd_parm.vopbdp_dup.vopbdd_srcfd;
                        int fd;

			if (error = fdt_dup(0, vfp, &fd)) {
			        VFILE_REF_RELEASE(vfp);
				_SAT_DUP(srcfd, 0, error);
				break;
			}

			ASSERT(vfp->vf_count >= 2);

			VFILE_REF_RELEASE(vfp);
			rv = fd;
			_SAT_DUP(srcfd, fd, 0);
		  	break;
		}
		}
	}
	rvp->r_val1 = rv;

	if (error == 0) {
		switch (uap->cmd) {
		case FIONBIO:
			/*
			 * XXX Used to "return EFAULT;" when copyin() failed.
			 * But the arg data type is device-specific. It can be
			 * either a pointer to a device-specific data structure
			 * or an integer. So on failure, treat the arg as an
			 * integer.
			 */
			if (copyin((caddr_t)uap->arg, &flag, sizeof(flag)))
				flag = uap->arg;

			if (flag) {
				VFILE_SETFLAGS (fp, ~0, FNDELAY);
			} else {
				VFILE_SETFLAGS (fp, ~FNDELAY, 0);
			}
			break;

		default:
			break;
		}
	}
	return error;
}

#if (_MIPS_SIM == _ABI64)
int
irix5_to_iovec(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	register struct iovec *iov;
	register struct irix5_iovec *i5_iov;
	int i;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_iovec) * count <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
					 sizeof(struct irix5_iovec) * count);
		info->copysize = sizeof(struct irix5_iovec) * count;
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_iovec) * count);
	ASSERT(info->copybuf != NULL);

	iov = to;

	i5_iov = info->copybuf;

	for (i = 0; i < count; i++, iov++, i5_iov++) {
		iov->iov_base = (void *)(__psunsigned_t)i5_iov->iov_base;
		iov->iov_len = (size_t)i5_iov->iov_len;
	}

	return 0;
}

/*ARGSUSED*/
int
irix5_to_flock(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_flock, flock);

	target->l_type = source->l_type;
	target->l_whence = source->l_whence;
	target->l_start = source->l_start;
	target->l_len = source->l_len;
	target->l_pid = source->l_pid;
	target->l_sysid = source->l_sysid;

	return 0;
}

/* ARGSUSED */
int
flock_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register int i;

	XLATE_COPYOUT_PROLOGUE(flock, irix5_flock);
	ASSERT(count == 1);

	target->l_type = source->l_type;
	target->l_whence = source->l_whence;
	target->l_start = source->l_start;
	target->l_len = source->l_len;
	target->l_sysid = source->l_sysid;
	target->l_pid = source->l_pid;
	for (i = 0; i < 4; i++)
		target->pad[i] = 0;

	return 0;
}

/*ARGSUSED*/
int
irix5_n32_to_flock(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_n32_flock, flock);

	target->l_type = source->l_type;
	target->l_whence = source->l_whence;
	target->l_start = source->l_start;
	target->l_len = source->l_len;
	target->l_pid = source->l_pid;
	target->l_sysid = source->l_sysid;

	return 0;
}

/* ARGSUSED */
int
flock_to_irix5_n32(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register int i;
	XLATE_COPYOUT_PROLOGUE(flock, irix5_n32_flock);

	ASSERT(count == 1);

	target->l_type = source->l_type;
	target->l_whence = source->l_whence;
	target->l_start = source->l_start;
	target->l_len = source->l_len;
	target->l_sysid = source->l_sysid;
	target->l_pid = source->l_pid;
	for (i = 0; i < 4; i++)
		target->pad[i] = 0;

	return 0;
}
#endif	/* _ABI64 */


/*
 * Arbitrary Attribute Support -
 *
 * The following code section supports "arbitrary attributes" on filesystem
 * objects.  It makes use of an SGI extension to the VOP layer.
 */

/*
 * Get the value of an attribute associated with an object, given its pathname.
 */
static int cattr_get(vnode_t *vp, char *attrname, char *attrvalue,
			     int *lengthp, int flags,
			     cred_t *cred, rval_t *rvp);
struct attr_geta {
	char *path;
	char *attrname;
	char *value;
	int *lengthp;
	sysarg_t flags;
};

/*ARGSUSED*/
int
attr_get(struct attr_geta *uap, rval_t *rvp)
{
	register int error;
	vnode_t *vp;

	if ((uap->flags & (ATTR_ROOT|ATTR_DONTFOLLOW)) != uap->flags)
		return EINVAL;
	if ((uap->flags & ATTR_ROOT) && (!_CAP_ABLE(CAP_DEVICE_MGT)))
		return EPERM;
	_SAT_PN_BOOK(SAT_FILE_ATTR_READ, curuthread);
	if (error = lookupname(uap->path, UIO_USERSPACE,
		       (uap->flags & ATTR_DONTFOLLOW) ? NO_FOLLOW : FOLLOW,
		       NULLVPP, &vp, NULL))
		return error;
	uap->flags &= ~ATTR_DONTFOLLOW;
	error = cattr_get(vp, uap->attrname, uap->value, uap->lengthp,
			      uap->flags, get_current_cred(), rvp);
	VN_RELE(vp);
	return error;
}

/*
 * Get the value of an attribute associated with an object, given an open fd.
 */
struct attr_getfa {
	sysarg_t fdes;
	char *attrname;
	char *value;
	int *lengthp;
	sysarg_t flags;
};

/*ARGSUSED*/
int
attr_getf(struct attr_getfa *uap, rval_t *rvp)
{
	vfile_t *fp;
	int error;

	if ((uap->flags & ATTR_ROOT) != uap->flags)
		return EINVAL;
	if ((uap->flags & ATTR_ROOT) && (!_CAP_ABLE(CAP_DEVICE_MGT)))
		return EPERM;
	if (error = getf((int)uap->fdes, &fp))
		return error;
	if (!VF_IS_VNODE(fp))
		return ENOSYS;
	error = cattr_get(VF_TO_VNODE(fp), uap->attrname, uap->value, 
			  uap->lengthp, uap->flags, fp->vf_cred, rvp);
	return error;
}

/*
 * Common code for attr_get() and attr_getf().  Check permissions,
 * set up and tear down argument buffer mapping, etc.
 */
/*ARGSUSED*/
static int
cattr_get(vnode_t *vp, char *attrname, char *attrvalue, int *lengthp,
		  int flags, cred_t *cred, rval_t *rvp)
{
	int error, length, size;
	char *name, *value;

	if (copyin((char *)lengthp, &length, sizeof(length)))
		return EFAULT;
	if (length < 0)
		return EINVAL;

	name = kmem_alloc(MAXNAMELEN, KM_SLEEP);
	if (copyinstr(attrname, name, MAXNAMELEN, NULL)) {
		kmem_free(name, MAXNAMELEN);
		return EFAULT;
	}
	size = (length == 0) ? 1 : length;
	size = (size < ATTR_MAX_VALUELEN) ? size : ATTR_MAX_VALUELEN;
	value = kmem_alloc(size, KM_SLEEP);
	if (length > size)
		length = size;
	VOP_ATTR_GET(vp, name, value, &length, flags, cred, error);
	kmem_free(name, MAXNAMELEN);
	if (!error && length && copyout(value, attrvalue, length))
		error = EFAULT;
	kmem_free(value, size);
	if (copyout(&length, (char *)lengthp, sizeof(length)))
		error = EFAULT;

	return error;
}

/*
 * Set the value of an attribute associated with an object, given its pathname.
 */
static int cattr_set(vnode_t *vp, char *attrname, char *attrvalue,
			     int length, int flags,
			     cred_t *cred, rval_t *rvp);
struct attr_seta {
	char *path;
	char *attrname;
	char *value;
	sysarg_t length;
	sysarg_t flags;
};

/*ARGSUSED*/
int
attr_set(struct attr_seta *uap, rval_t *rvp)
{
	register int error;
	vnode_t *vp;

	if ((uap->flags & (ATTR_ROOT|ATTR_DONTFOLLOW|ATTR_CREATE|ATTR_REPLACE))
	    != uap->flags)
		return EINVAL;
	if ((uap->flags & ATTR_ROOT) && (!_CAP_ABLE(CAP_DEVICE_MGT)))
		return EPERM;
	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);
	if (error = lookupname(uap->path, UIO_USERSPACE,
		       (uap->flags & ATTR_DONTFOLLOW) ? NO_FOLLOW : FOLLOW,
		       NULLVPP, &vp, NULL))
		return error;
	uap->flags &= ~ATTR_DONTFOLLOW;
	error = cattr_set(vp, uap->attrname, uap->value, uap->length,
			      uap->flags, get_current_cred(), rvp);
	VN_RELE(vp);
	return error;
}

/*
 * Set the value of an attribute associated with an object, given an open fd.
 */
struct attr_setfa {
	sysarg_t fdes;
	char *attrname;
	char *value;
	sysarg_t length;
	sysarg_t flags;
};

/*ARGSUSED*/
int
attr_setf(struct attr_setfa *uap, rval_t *rvp)
{
	vfile_t *fp;
	int error;

	if ((uap->flags & (ATTR_ROOT|ATTR_CREATE|ATTR_REPLACE)) != uap->flags)
		return EINVAL;
	if ((uap->flags & ATTR_ROOT) && (!_CAP_ABLE(CAP_DEVICE_MGT)))
		return EPERM;
	if (error = getf((int)uap->fdes, &fp))
		return error;
	if (!VF_IS_VNODE(fp))
		return ENOSYS;
	error = cattr_set(VF_TO_VNODE(fp), uap->attrname, uap->value, 
		      uap->length,
		      uap->flags | ((fp->vf_flag&FINVIS) ? ATTR_KERNOTIME : 0),
		      fp->vf_cred, rvp);
	return error;
}

/*
 * Common code for attr_set() and attr_setf().  Check the args, permissions,
 * set up and tear down argument buffer mapping, etc.
 */
/*ARGSUSED*/
static int
cattr_set(vnode_t *vp, char *attrname, char *attrvalue, int length,
		  int flags, cred_t *cred, rval_t *rvp)
{
	char *name, *value;
	int error, size;

	if ((flags & (ATTR_CREATE|ATTR_REPLACE)) == (ATTR_CREATE|ATTR_REPLACE))
		return EINVAL;
	if (length < 0)
		return EINVAL;
	if (length > ATTR_MAX_VALUELEN)
		return E2BIG;

	name = kmem_alloc(MAXNAMELEN, KM_SLEEP);
	if (copyinstr(attrname, name, MAXNAMELEN, NULL)) { 
		kmem_free(name, MAXNAMELEN);
		return EFAULT;
	}
	size = (length == 0) ? 1 : length;
	value = kmem_alloc(size, KM_SLEEP);
	if (copyin(attrvalue, value, length)) {
		error = EFAULT;
	} else {
		VOP_ATTR_SET(vp, name, value, length, flags, cred, error);
	}
	kmem_free(value, size);
	kmem_free(name, MAXNAMELEN);
	return error;
}


/*
 * Remove an attribute associated with an object, given its pathname.
 */
static int cattr_remove(vnode_t *vp, char *attrname, int flags,
				cred_t *cred, rval_t *rvp);
struct attr_removea {
	char *path;
	char *attrname;
	sysarg_t flags;
};

/*ARGSUSED*/
int
attr_remove(struct attr_removea *uap, rval_t *rvp)
{
	register int error;
	vnode_t *vp;

	if ((uap->flags & (ATTR_ROOT|ATTR_DONTFOLLOW)) != uap->flags)
		return EINVAL;
	if ((uap->flags & ATTR_ROOT) && (!_CAP_ABLE(CAP_FOWNER)))
		return EPERM;
	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);
	if (error = lookupname(uap->path, UIO_USERSPACE,
		       (uap->flags & ATTR_DONTFOLLOW) ? NO_FOLLOW : FOLLOW,
		       NULLVPP, &vp, NULL))
		return error;
	uap->flags &= ~ATTR_DONTFOLLOW;
	error = cattr_remove(vp, uap->attrname, uap->flags,
			     get_current_cred(), rvp);
	VN_RELE(vp);
	return error;
}

/*
 * Remove an attribute associated with an object, given an open fd.
 */
struct attr_removefa {
	sysarg_t fdes;
	char *attrname;
	sysarg_t flags;
};

/*ARGSUSED*/
int
attr_removef(struct attr_removefa *uap, rval_t *rvp)
{
	vfile_t *fp;
	int error;

	if ((uap->flags & ATTR_ROOT) != uap->flags)
		return EINVAL;
	if ((uap->flags & ATTR_ROOT) && (!_CAP_ABLE(CAP_FOWNER)))
		return EPERM;
	if (error = getf((int)uap->fdes, &fp))
		return error;
	if (!VF_IS_VNODE(fp))
		return ENOSYS;
	error = cattr_remove(VF_TO_VNODE(fp), uap->attrname,
		    uap->flags | ((fp->vf_flag&FINVIS) ? ATTR_KERNOTIME : 0),
		    fp->vf_cred, rvp);
	return error;
}

/*
 * Common code for attr_remove() and attr_removef().  Check permissions,
 * set up and tear down argument buffer mapping, etc.
 */
/*ARGSUSED*/
static int
cattr_remove(vnode_t *vp, char *attrname, int flags, cred_t *cred, rval_t *rvp)
{
	register int error;
	char *name;

	name = kmem_alloc(MAXNAMELEN, KM_SLEEP);
	if (copyinstr(attrname, name, MAXNAMELEN, NULL)) { 
		error = EFAULT;
	} else {
		VOP_ATTR_REMOVE(vp, name, flags, cred, error);
	}
	kmem_free(name, MAXNAMELEN);

	return error;
}


/*
 * List the attributes associated with an object, given its pathname.
 */
struct attr_lista {
	char			*path;
	char			*buffer;
	sysarg_t		bufsize;
	sysarg_t		flags;
	attrlist_cursor_kern_t	*cursorp;
};

/*ARGSUSED*/
int
attr_list(struct attr_lista *uap, rval_t *rvp)
{
	register int error;
	vnode_t *vp;

	if ((uap->flags & (ATTR_ROOT|ATTR_DONTFOLLOW)) != uap->flags)
		return EINVAL;
	if ((uap->flags & ATTR_ROOT) && (!_CAP_ABLE(CAP_FOWNER)))
		return EPERM;
	_SAT_PN_BOOK(SAT_FILE_ATTR_READ, curuthread);
	if (error = lookupname(uap->path, UIO_USERSPACE,
		       (uap->flags & ATTR_DONTFOLLOW) ? NO_FOLLOW : FOLLOW,
		       NULLVPP, &vp, NULL))
		return error;
	uap->flags &= ~ATTR_DONTFOLLOW;
	error = cattr_list(vp, uap->buffer, uap->bufsize, uap->flags,
			       uap->cursorp, get_current_cred(), rvp);
	VN_RELE(vp);
	return error;
}

/*
 * List the attributes associated with an object, given an open fd.
 */
struct attr_listfa {
	sysarg_t		fdes;
	char			*buffer;
	sysarg_t		bufsize;
	sysarg_t		flags;
	attrlist_cursor_kern_t	*cursorp;
};

/*ARGSUSED*/
int
attr_listf(struct attr_listfa *uap, rval_t *rvp)
{
	vfile_t *fp;
	int error;

	if ((uap->flags & ATTR_ROOT) != uap->flags)
		return EINVAL;
	if ((uap->flags & ATTR_ROOT) && (!_CAP_ABLE(CAP_FOWNER)))
		return EPERM;
	if (error = getf((int)uap->fdes, &fp))
		return error;
	if (!VF_IS_VNODE(fp))
		return ENOSYS;
	error = cattr_list(VF_TO_VNODE(fp), uap->buffer, uap->bufsize, 
			   uap->flags, uap->cursorp, fp->vf_cred, rvp);
	return error;
}

/*
 * Common code for attr_list() and attr_listf().
 * If we have a proerly aligned output ubuffer, we can "map" it in
 * rather than allocating a new buffer and using copyin/copyout.
 */
/*ARGSUSED*/
int
cattr_list(vnode_t *vp, char *bufferp, int bufsize, int flags,
		   attrlist_cursor_kern_t *cursorp,
		   cred_t *cred, rval_t *rvp)
{
	attrlist_cursor_kern_t cursor;
	int alignment, error;

	/*
	 * If the user gave us a properly aligned buffer, lock it down and
	 * work directly within that buffer.  Also ensure it is big enough.
	 */
	alignment = sizeof( ((attrlist_t *)0)->al_count ) - 1;
	if (((__psint_t)bufferp & alignment) != 0) {
		return EFAULT;
	}
	bufsize &= ~alignment;		/* round down the alignment */
	if ((bufsize < sizeof(attrlist_t)+sizeof(attrlist_ent_t)+MAXNAMELEN) ||
	    (bufsize > ATTR_MAX_VALUELEN)) {
		return EINVAL;
	}
	if (copyin(cursorp, &cursor, (int)sizeof(cursor))) {
		return EFAULT;
	}
	if (error = useracc(bufferp, bufsize, B_READ, 0)) {
		return error;
	}

	VOP_ATTR_LIST(vp, bufferp, bufsize, flags, &cursor, cred, error);

	unuseracc(bufferp, bufsize, B_READ);
	if (!error && copyout(&cursor, cursorp, (int)sizeof(cursor)))
		error = EFAULT;
	return error;
}

/*
 * Do multiple attribute operations on a single object, given its pathname.
 */
struct attr_multia {
	char *path;
	caddr_t *oplistp;
	sysarg_t count;
	sysarg_t flags;
};

/*ARGSUSED*/
int
attr_multi(struct attr_multia *uap, rval_t *rvp)
{
	register int error;
	vnode_t *vp;

	if ((uap->flags & (ATTR_ROOT|ATTR_DONTFOLLOW)) != uap->flags)
		return EINVAL;
	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);
	if (error = lookupname(uap->path, UIO_USERSPACE,
		       (uap->flags & ATTR_DONTFOLLOW) ? NO_FOLLOW : FOLLOW,
		       NULLVPP, &vp, NULL))
		return error;
	uap->flags &= ~ATTR_DONTFOLLOW;
	error = cattr_multi(vp, uap->oplistp, uap->count, uap->flags,
				get_current_cred(), rvp);
	VN_RELE(vp);
	return error;
}

/*
 * Do multiple attribute operations on a single object, given an open fd.
 */
struct attr_multifa {
	sysarg_t fdes;
	caddr_t *oplistp;
	sysarg_t count;
	sysarg_t flags;
};

/*ARGSUSED*/
int
attr_multif(struct attr_multifa *uap, rval_t *rvp)
{
	vfile_t *fp;
	int error;

	if ((uap->flags & ATTR_ROOT) != uap->flags)
		return EINVAL;
	if (error = getf((int)uap->fdes, &fp))
		return error;
	if (!VF_IS_VNODE(fp))
		return ENOSYS;
	error = cattr_multi(VF_TO_VNODE(fp), uap->oplistp, uap->count, 
			    uap->flags, fp->vf_cred, rvp);
	return error;
}

#if (_MIPS_SIM == _ABI64)
int irix5_n32_to_attr_multiop(enum xlate_mode mode, void *to, int count,
				   xlate_info_t *info);
int attr_multiop_to_irix5_n32(void *from, int count, xlate_info_t *info);
#endif

/*
 * Common code for attr_multi() and attr_multif().  Check permissions,
 * set up and tear down argument buffer mapping, etc.
 */
/*ARGSUSED*/
int
cattr_multi(vnode_t *vp, caddr_t *oplistp, int count, int flags,
		    cred_t *cred, rval_t *rvp)
{
#if (_MIPS_SIM == _ABI64)
	struct attr_multiop_kern_64 *oplist, *op;
	int abi = get_current_abi();
#else
	struct attr_multiop_kern_32 *oplist, *op;
#endif
	char *name, *value;
	int error, oplistsize, size, i;

	if ((count < 0) || (count > ATTR_MAX_MULTIOPS))
		return EINVAL;
	if (count == 0)
		return 0;

	/*
	 * Copy in the list of operations.
	 */
	oplistsize = count * sizeof(*oplist);
	oplist = kmem_alloc(oplistsize, KM_SLEEP);
	if (COPYIN_XLATE(oplistp, oplist, oplistsize, irix5_n32_to_attr_multiop,
				  abi, count)) {
	}

	/*
	 * Set up a buffer to store the current attribute name.
	 * We will copy in each attribute name as we process it, then
	 * overwrite it with the next attribute name.
	 */
	name = kmem_alloc(MAXNAMELEN, KM_SLEEP);

#if 0
	/*
	 * Lock the vnode against other changes while we are processing an
	 * "atomic" multi-attribute operation list.
	 */
	if (flags & ATTR_ATOMIC) {
		/*
		 * Check to see whether we will need an exclusive lock or not.
		 */
		lock_type = VRWLOCK_READ;
		for (op = oplist, i = 0; i < count; op++, i++) {
			if ((op->am_opcode == ATTR_OP_SET) ||
			    (op->am_opcode == ATTR_OP_REMOVE)) {
				lock_type = VRWLOCK_WRITE;
				break;
			}
		}
		/* GROT: this lock should be private to the attribute calls */
		VOP_RWLOCK(vp, lock_type);

		/*
		 * Check each operation to see if it would succeed.
		 */
		fail = 0;
		for (op = oplist, i = 0; i < count; op++, i++) {
			if (op->am_error) {
				continue;
			}
			if ((op->am_opcode != ATTR_OP_GET) &&
			    (op->am_opcode != ATTR_OP_SET) &&
			    (op->am_opcode != ATTR_OP_REMOVE)) {
				op->am_error = EINVAL;
				fail++;
				continue;
			}
			if (copyinstr((char *)op->am_attrname, name,
				      MAXNAMELEN, NULL)) {
				op->am_error = EFAULT;
				continue;
			}
			length = 0;
			VOP_ATTR_GET(vp, name, NULL, &length,
						 op->am_flags, cred, error);
			op->am_error = 0;
			switch (op->am_opcode) {
			case ATTR_OP_GET:
				if (error != EFBIG) {
					op->am_error = error;
					fail++;
				} else if (length > op->am_length) {
					op->am_error = EFBIG;
					fail++;
				}
				break;
			case ATTR_OP_SET:
				if (error == ENXIO) {
					if (op->am_flags & ATTR_NOCREATE) {
						op->am_error = ENXIO;
						fail++;
					}
				} else if (error == EFBIG) {
					if (op->am_flags & ATTR_CREATE) {
						op->am_error = EEXIST;
						fail++;
					}
				} else if ((op->am_length <= 0) ||
					   (op->am_length > 65536)) {
					op->am_error = EFBIG;
					fail++;
				} else {
					op->am_error = error;
					fail++;
				}
				break;
			case ATTR_OP_REMOVE:
				if (error != EFBIG) {
					op->am_error = error;
					fail++;
				}
				break;
			}
		}
		if (fail) {
			error = EPERM;
			goto out;
		}
	}
#endif /* 0 */

	/*
	 * Perform each operation given by the user.
	 * If the user specified atomic behavior, then none of these ops
	 * should fail since we just checked them (above).
	 * If they did not specify atomic behavior, then each operation
	 * takes its chances against other competing ops in the system.
	 */
	for (op = oplist, i = 0; i < count; op++, i++) {
		if (op->am_error) {
			continue;
		}
		if (copyinstr((char *)op->am_attrname, name, MAXNAMELEN,
			      NULL)) {
			op->am_error = EFAULT;
			continue;
		}
		switch (op->am_opcode) {
		case ATTR_OP_GET:
			if ((op->am_length < 0) ||
			    (op->am_length > ATTR_MAX_VALUELEN)) {
				op->am_error = EFBIG;
				break;
			}
			size = (op->am_length == 0) ? 1 : op->am_length;
			value = kmem_alloc(size, KM_SLEEP);
			VOP_ATTR_GET(vp, name, value,
							&op->am_length,
							op->am_flags, cred,
							op->am_error);
			if (!op->am_error && copyout(value,
						     (char *)op->am_attrvalue,
						     op->am_length)) {
				op->am_error = EFAULT;
			}
			kmem_free(value, size);
			break;
		case ATTR_OP_SET:
			if ((op->am_length < 0) ||
			    (op->am_length > ATTR_MAX_VALUELEN)) {
				op->am_error = EFBIG;
				break;
			}
			size = (op->am_length == 0) ? 1 : op->am_length;
			value = kmem_alloc(size, KM_SLEEP);
			if (copyin((char *)op->am_attrvalue, value,
				   op->am_length)) {
				op->am_error = EFAULT;
			} else {
				VOP_ATTR_SET(vp, name, value,
								op->am_length,
/* GROT: or in FINVIS flag */					op->am_flags,
								cred,
								op->am_error);
			}
			kmem_free(value, size);
			break;
		case ATTR_OP_REMOVE:
			VOP_ATTR_REMOVE(vp, name, op->am_flags,
/* GROT: or in FINVIS flag */			cred, op->am_error);
			break;
		default:
			op->am_error = EINVAL;
			break;
		}
	}
	error = 0;

#if 0
out:
	if (flags & ATTR_ATOMIC) {
		/* GROT: this lock should be private to the attribute calls */
		VOP_RWUNLOCK(vp, lock_type);
	}
#endif /* 0 */
	if (XLATE_COPYOUT(oplist, oplistp, oplistsize,
				  attr_multiop_to_irix5_n32,
				  abi, count)) {
	}
	kmem_free(name, MAXNAMELEN);
	kmem_free(oplist, oplistsize);
	return error;
}

#if (_MIPS_SIM == _ABI64)
/*ARGSUSED*/
int
irix5_n32_to_attr_multiop(enum xlate_mode mode, void *to, int count,
			       xlate_info_t *info)
{
	COPYIN_XLATE_VARYING_PROLOGUE(attr_multiop_kern_32,
				     attr_multiop_kern_64,
				     count*sizeof(struct attr_multiop_kern_32));

	while (count) {
		target->am_opcode = source->am_opcode;
		target->am_error = source->am_error;
		target->am_attrname = source->am_attrname;
		target->am_attrvalue = source->am_attrvalue;
		target->am_length = source->am_length;
		target->am_flags = source->am_flags;
		target++;
		source++;
		count--;
	}

	return 0;
}

/*ARGSUSED*/
int
attr_multiop_to_irix5_n32(void *from, int count, register xlate_info_t *info)
{
	XLATE_COPYOUT_VARYING_PROLOGUE(attr_multiop_kern_64,
				     attr_multiop_kern_32,
				     count*sizeof(struct attr_multiop_kern_32));

	while (count) {
		target->am_opcode = source->am_opcode;
		target->am_error = source->am_error;
		target->am_attrname = source->am_attrname;
		target->am_attrvalue = source->am_attrvalue;
		target->am_length = source->am_length;
		target->am_flags = source->am_flags;
		target++;
		source++;
		count--;
	}

	return 0;
}
#endif /* _ABI64*/
