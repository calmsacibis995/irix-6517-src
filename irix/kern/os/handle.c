/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.43 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <ksys/vfile.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <ksys/fdt.h>
#include <sys/debug.h>
#include <sys/handle.h>
#include <sys/attributes.h>
#include <sys/fcntl.h>
#include <sys/dmi_kern.h>


/*
 *  Ok.  This file contains stuff that defines a general handle
 *  mechanism, except that it's only used by xfs at the moment.
 */


STATIC int
copyout_handle (
	handle_t	*handle,	/* handle data to copy out */
	size_t		hsize,		/* amount of data in handle */
	void		*hbuf,		/* user's data buffer */
	size_t		*hlen)		/* user's data buffer's size */
{
	if (copyout (handle, hbuf, (int) hsize))
		return EFAULT;
#if _MIPS_SIM == _ABI64
	if (ABI_IS_64BIT(get_current_abi())) {
		if (copyout(&hsize, hlen, sizeof *hlen))
			return EFAULT;
		return 0;
	}
#endif
	if (suword(hlen, (int) hsize))
		return EFAULT;
	return 0;
}

/*
 *  Return the handle for the object named by path.
 */

int
path_to_handle (
	char	*path,		/* any path name */
	void	*hbuf,		/* user's data buffer */
	size_t	*hlen)		/* set to size of data copied to hbuf */
{
	int		error;
	vnode_t		*vp;
	handle_t	handle;

	error = lookupname (path, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return error;
	error = vp_to_handle (vp, &handle);
	VN_RELE (vp);
	if (error)
		return error;
	return copyout_handle (&handle, HSIZE (handle), hbuf, hlen);
}


/*
 *  Return the handle for the file system containing the object named by path.
 */

int
path_to_fshandle (
	char	*path,		/* any path name */
	void	*hbuf,		/* user's data buffer */
	size_t	*hlen)		/* set to size of data copied to hbuf */
{
	int		error;
	vnode_t		*vp;
	handle_t	handle;

	error = lookupname (path, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return error;
	error = vp_to_handle (vp, &handle);
	VN_RELE (vp);
	if (error)
		return error;
	return copyout_handle (&handle, sizeof (fsid_t), hbuf, hlen);
}


int
fd_to_handle (
	int	fd,		/* any file descriptor */
	void	*hbuf,		/* user's data buffer */
	size_t	*hlen)		/* set to size of data copied to hbuf */
{
	int		error;
	vnode_t		*vp;
	vfile_t		*fp;
	handle_t	handle;

	error = getf (fd, &fp);
	if (error)
		return error;
	if (!VF_IS_VNODE(fp))
		return EINVAL;
	vp = VF_TO_VNODE(fp);
	error = vp_to_handle (vp, &handle);
	if (error)
		return error;
	return copyout_handle (&handle, HSIZE (handle), hbuf, hlen);
}

/*
 *	Does attr_list with handle specified in place of a pathname.
 */

int
attr_list_by_handle(
	void		*hanp,			/* handle data */
	size_t		hlen,			/* size of handle data */
	void		*buffer,
	int		bufsize,
	int		flags,
	void		 *cursor,
	rval_t		*rvp)
{
	int		error;
	handle_t	handle;
	vnode_t		*vp;

	error = gethandle (hanp, hlen, &handle);
	if (error)
		return error;
	vp = handle_to_vp (&handle);
	if (vp == NULL)
		return EINVAL;
	error = cattr_list(vp, (char *)buffer, bufsize, flags, 
		(attrlist_cursor_kern_t *)cursor, get_current_cred(),  rvp);
	VN_RELE(vp);
	return error;
}

/*
 *	Does attr_multi with handle specified in place of a pathname.
 */

int
attr_multi_by_handle(
	void		*hanp,			/* handle data */
	size_t		hlen,			/* size of handle data */
	void		*oplist,
	int		count,
	int		flags,
	rval_t		*rvp)
{
	int		error;
	handle_t	handle;
	vnode_t		*vp;

	error = gethandle (hanp, hlen, &handle);
	if (error)
		return error;
	vp = handle_to_vp (&handle);
	if (vp == NULL)
		return EINVAL;
	error = cattr_multi(vp, (caddr_t *)oplist, count, flags,
					get_current_cred(), rvp);
	VN_RELE(vp);
	return error;
}

/*
 *	Does the fcntl F_FSSETDM subfunction - handle secified instead
 *	of an fd.
 */

int
fssetdm_by_handle(
	void		*hanp,			/* handle data */
	size_t		hlen,			/* size of handle data */
	void		*fsdmidata)		/* struct fsdimdata */
{
	int		error;
	handle_t	handle;
	vnode_t		*vp;
	struct fsdmidata fsd;
	dm_fcntl_t	dmfcntl;

	error = gethandle (hanp, hlen, &handle);
	if (error)
		return error;
	dmfcntl.dmfc_subfunc = DM_FCNTL_FSSETDM;
	if (copyin(fsdmidata, &fsd, sizeof fsd)) {
		error = EFAULT;
		return error;
	}
	dmfcntl.u_fcntl.setdmrq.fsd_dmevmask = fsd.fsd_dmevmask;
	dmfcntl.u_fcntl.setdmrq.fsd_dmstate = fsd.fsd_dmstate;

	vp = handle_to_vp (&handle);
	if (vp == NULL)
		return EINVAL;
	VOP_FCNTL(vp, F_DMAPI, &dmfcntl, 0, (off_t)0, get_current_cred(),
								NULL, error);
	VN_RELE(vp);
	return error;
}

/*
 *  Just like readlink, except that it uses a handle instead of a path
 *  to locate the vnode.
 */

int
readlink_by_handle (
	void		*hanp,			/* handle data */
	size_t		hlen,			/* size of handle data */
	void		*buf,			/* return buffer */
	size_t		bufsiz,			/* size provided */
	rval_t		*rvp)
{
	int		error;
	handle_t	handle;
	vnode_t		*vp;
	struct	iovec	aiov;
	struct	uio	auio;

	error = gethandle (hanp, hlen, &handle);
	if (error)
		return error;
	vp = handle_to_vp (&handle);
	if (vp == NULL)
		return EINVAL;
	if (vp->v_type != VLNK) {
		error = EINVAL;
		goto out;
        }

	aiov.iov_base	= buf;
	aiov.iov_len	= bufsiz;
	auio.uio_iov	= &aiov;
	auio.uio_iovcnt	= 1;

	auio.uio_pmp	= NULL;
	auio.uio_pio	= 0;
	auio.uio_pbuf	= 0;
	auio.uio_readiolog	= 0;
	auio.uio_writeiolog	= 0;

	auio.uio_fmode	= FINVIS;		/* not used otherwise */
	auio.uio_offset	= 0;
	auio.uio_segflg	= UIO_USERSPACE;
	auio.uio_resid	= bufsiz;
	VOP_READLINK (vp, &auio, get_current_cred(), error);
out:
	VN_RELE (vp);
	rvp->r_val1 = bufsiz - auio.uio_resid;
	return error;
}


/*
 *  Open a file specified by a file handle.  Follow the same basic
 *  algorithm as copen, cutting out some things not needed bacause
 *  of self-imposed restrictions.  The restrictions are that only
 *  directories and regular files are allowed and none of the funky
 *  open flags are used.
 *	+ restricted to FREAD and FWRITE, always has FINVIS
 *	+ restricted to directories and regular files.
 *	+ mask out other open flags
 *  Always sets the FINVIS flag.
 */

int
open_by_handle (
	void		*hanp,			/* handle data */
	size_t		hlen,			/* size of handle data */
	int		filemode,		/* open mode */
	rval_t		*rvp)
{
	vnode_t		*vp;
	vfile_t		*fp;
	int		fd;
	int		error;
	handle_t	handle;
	int		sat_flags = filemode;	/* what was requested */

	filemode -= FOPEN;
	if ((filemode & (FNONBLOCK|FNDELAY)) == (FNONBLOCK|FNDELAY))
		filemode &= ~FNDELAY;
	filemode &= FMASK;
	if ((filemode & (FREAD|FWRITE)) == 0)
		return EINVAL;
	filemode |= FINVIS;

	error = gethandle (hanp, hlen, &handle);
	if (error)
		return error;
	vp = handle_to_vp (&handle);
	if (vp == NULL)
		return EINVAL;

	switch (vp->v_type) {
		case VDIR:
		case VREG:
			break;
		default:
			error = EINVAL;
			goto out;
	}

	error = vfile_alloc (filemode, &fp, &fd);
	if (error)
		goto out;

	/*
	 * a kludge for the usema device - it really needs to know
	 * the 'fp' for it opening file - we use ut_openfp to mark
	 * this.
	 */
	curuthread->ut_openfp = fp;

	if ((filemode & FWRITE) == 0)
		_SAT_PN_BOOK(SAT_OPEN_RO, curuthread);
	else
		_SAT_PN_BOOK(SAT_OPEN, curuthread);

	filemode &= (FREAD | FWRITE | FINVIS);
	error = vp_open (vp, filemode, get_current_cred());

	curuthread->ut_openfp = NULL;
	if (error)
		vfile_alloc_undo(fd, fp);
	else {
		vfile_ready(fp, vp);
		rvp->r_val1 = fd;
	}

out:
	if (error)
		VN_RELE (vp);
	_SAT_OPEN (rvp->r_val1, (filemode&FCREAT), sat_flags, error);
	return error;
}


/*
 *  vp_open - based on vn_open.  Differences are
 *	+ gets a vp passed to it, instead of using lookupname
 */

int
vp_open (
	vnode_t		*vp,
	int		filemode,
	cred_t		*crp)
{
	struct vnode *tvp;
	register int mode;
	register int error;
	struct vnode *openvp;

	tvp = (struct vnode *) NULL;
	mode = 0;
	if (filemode & FREAD)
		mode |= VREAD;
	if (filemode & FWRITE)
		mode |= VWRITE;
 
	tvp = vp;
	VN_HOLD (tvp);

	VOP_SETFL (vp, 0, filemode, crp, error);
	if (error)
		goto out;

	/*
	 * Can't write directories, active texts, swap files,
	 * or read-only filesystems.  
	 */
	if (filemode & FWRITE) {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		}
		if (vp->v_vfsp->vfs_flag & VFS_RDONLY) {
			error = EROFS;
			goto out;
		}
		if ((vp->v_flag & VISSWAP) && vp->v_type == VREG) {
			error = EBUSY;
			goto out;
		}
	}
	/* Check discretionary permissions. */
	VOP_ACCESS (vp, mode, crp, error);
	if (error)
		goto out;

	/*
	 * Do opening protocol.
	 */
	openvp = vp;
	VOP_OPEN (openvp, &vp, filemode, crp, error);
	if (!error) {
		if (tvp)
			VN_RELE (tvp);
	}
out:
	if (error) {
		if (tvp)
			VN_RELE (tvp);
	}
	return error;
}


/*
 *  Make a handle from a vnode.
 */

int
vp_to_handle (
	vnode_t		*vp,
	handle_t	*handlep)
{
	int		error;
	struct	fid	*fidp = NULL;
	struct	fid	fid;
	int		hsize;

	if (vp->v_vfsp->vfs_altfsid == NULL)
		return EINVAL;
	switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			break;
		default:
			return EBADF;
	}
#ifdef SMALLFIDS
	VOP_FID (vp, &fidp, error);
#else
	VOP_FID2 (vp, &fid, error);
	fidp = &fid;
#endif /* SMALLFIDS */
	if (error)
		return error;
	if (fidp == NULL)
		return EIO;		/* FIX: what real errno? */
	bcopy (vp->v_vfsp->vfs_altfsid, &handlep->ha_fsid, sizeof (fsid_t));
	/* Copy only the currently used part of the fid struct */
	bcopy (fidp, &handlep->ha_fid, fidp->fid_len + sizeof fidp->fid_len);
	hsize = HSIZE (*handlep);
	bzero ((char *) handlep + hsize, sizeof *handlep - hsize);
#ifdef SMALLFIDS
	freefid (fidp);
#endif
	return 0;
}


/*
 *  Get a vnode for the object referenced by handle.
 */

struct vnode *
handle_to_vp (
	handle_t	*handlep)
{
	struct	vfs	*vfsp;
	struct	vnode	*vp;
		int	error;

	vfsp = altgetvfs (&handlep->ha_fsid);
	if (vfsp == NULL)
		return NULL;
	VFS_VGET (vfsp, &vp, &handlep->ha_fid, error);
	if (error)
		return NULL;
	return vp;
}


/*
 *  Get a handle of the form (void *, size_t) from user space and
 *  convert it to a handle_t handle.  Do as much validation of the
 *  result as possible; the content of each fid still has to be
 *  checked by its consumer.
 */

int
gethandle (
	void		*hanp,		/* input,  handle data */
	size_t		hlen,		/* input,  size of handle data */
	handle_t	*hp)		/* output, copy of data */
{
	if (hlen < sizeof hp->ha_fsid || hlen > sizeof *hp)
		return EINVAL;
	if (copyin (hanp, hp, hlen))
		return EFAULT;
	if (hlen < sizeof *hp)
		bzero (((char *) hp) + hlen, sizeof *hp - hlen);
	if (hlen == sizeof hp->ha_fsid)	
		return 0;	/* FS handle, nothing more to check */
	if (hp->ha_fid.fid_len != (hlen - sizeof hp->ha_fsid -
	    sizeof hp->ha_fid.fid_len) || *((short *) hp->ha_fid.fid_data))
	{
		return EINVAL;
	}
	return 0;
}


/*
 *  An alternate version of getvfs.  Needed because handles require
 *  a file system ID that remains constant for the life of the file
 *  system.  Cloned from getvfs in os/vfs.c, where this might arguably
 *  belong.  It's here because it's only used by things related to
 *  these handles.
 */

struct vfs *
altgetvfs (fsid_t *fsid)
{
	register struct vfs *vfsp;
	int s;

	s = vfs_spinlock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next) {
		if (vfsp->vfs_altfsid &&
		    vfsp->vfs_altfsid->val[0] == fsid->val[0] &&
		    vfsp->vfs_altfsid->val[1] == fsid->val[1] &&
		    !(vfsp->vfs_flag & VFS_OFFLINE)) {
			break;
		}
	}
	vfs_spinunlock(s);
	return vfsp;
}
