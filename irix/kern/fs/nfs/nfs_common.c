/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * $Revision: 1.13 $
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
#define NFSSERVER
#include "types.h"
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/mbuf.h>
#include <sys/mkdev.h>
#include <sys/pathname.h>
#include <sys/siginfo.h>        /* for CLD_EXITED exit code */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/cmn_err.h>
#include "auth.h"
#include "auth_unix.h"
#include "svc.h"
#include "xdr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "export.h"

#ifdef DEBUG
int	dbg_vattr_to_wcc_data		= 0;
int	dbg_vattr_to_fattr3		= 0;
int	dbg_rw_destroy			= 0;
int	dbg_rw_enter			= 0;
#endif /* DEBUG */

extern major_t	getemajor(dev_t);
extern minor_t	geteminor(dev_t);


static ftype3 vt_to_nf3[] = {
	NF3NONE, NF3REG, NF3DIR, NF3BLK, NF3CHR, NF3LNK, NF3FIFO, NF3NONE, NF3SOCK
};

void
vattr_to_fattr3(struct vattr *vap, struct fattr3 *fap)
{
#ifdef DEBUG
	char	*self = "vattr_to_fattr3";

	if (dbg_vattr_to_fattr3)
	    printf ("%s(vap, fap)\n", self, vap, fap);
#endif /* DEBUG */
	ASSERT(vap->va_type >= VNON && vap->va_type <= VSOCK);
	fap->type = vt_to_nf3[vap->va_type];
	fap->mode = (mode3)(vap->va_mode & MODEMASK);
	fap->nlink = (__uint32_t)vap->va_nlink;
	if (vap->va_uid == UID_NOBODY)
		fap->uid = (uid3)NFS_UID_NOBODY;
	else
		fap->uid = (uid3)vap->va_uid;
	if (vap->va_gid == GID_NOBODY)
		fap->gid = (gid3)NFS_GID_NOBODY;
	else
		fap->gid = (gid3)vap->va_gid;
	fap->size = (size3)vap->va_size;
	fap->used = (size3)BBTOOFF((size3)vap->va_nblocks);
	fap->rdev.specdata1 = (__uint32_t)getemajor(vap->va_rdev);
	fap->rdev.specdata2 = (__uint32_t)geteminor(vap->va_rdev);
	fap->fsid = (__uint64_t)vap->va_fsid;
	fap->fileid = (fileid3)vap->va_nodeid;
	fap->atime.seconds = vap->va_atime.tv_sec;
	fap->atime.nseconds = vap->va_atime.tv_nsec;
	fap->mtime.seconds = vap->va_mtime.tv_sec;
	fap->mtime.nseconds = vap->va_mtime.tv_nsec;
	fap->ctime.seconds = vap->va_ctime.tv_sec;
	fap->ctime.nseconds = vap->va_ctime.tv_nsec;
}

void
vattr_to_wcc_attr(struct vattr *vap, struct wcc_attr *wccap)
{

	wccap->size = (size3)vap->va_size;
	wccap->mtime.seconds = vap->va_mtime.tv_sec;
	wccap->mtime.nseconds = vap->va_mtime.tv_nsec;
	wccap->ctime.seconds = vap->va_ctime.tv_sec;
	wccap->ctime.nseconds = vap->va_ctime.tv_nsec;
}

void
vattr_to_wcc_data(struct vattr *bvap, struct vattr *avap, struct wcc_data *wccp)
{
#ifdef DEBUG
	char	*self = "vattr_to_wcc_data";

	if (dbg_vattr_to_wcc_data)
	    printf ("%s(0x%x 0x%x 0x%x)\n", self, bvap, avap, wccp);
#endif /* DEBUG */
	if (bvap) {
		wccp->before.attributes = TRUE;
		vattr_to_wcc_attr(bvap, &wccp->before.attr);
	} else {
		wccp->before.attributes = FALSE;
	}
	if (avap) {
		wccp->after.attributes = TRUE;
		vattr_to_fattr3(avap, &wccp->after.attr);
	} else {
		wccp->after.attributes = FALSE;
	}
}


nfsstat3
puterrno3(int error)
{

#ifdef DEBUG
	switch (error) {
	case 0:
		return (NFS3_OK);
	case EPERM:
		return (NFS3ERR_PERM);
	case ENOENT:
		return (NFS3ERR_NOENT);
	case EIO:
		return (NFS3ERR_IO);
	case ENXIO:
		return (NFS3ERR_NXIO);
	case EAGAIN:
		return (NFS3ERR_JUKEBOX);
	case EACCES:
		return (NFS3ERR_ACCES);
	case EEXIST:
		return (NFS3ERR_EXIST);
	case EXDEV:
		return (NFS3ERR_XDEV);
	case ENODEV:
		return (NFS3ERR_NODEV);
	case ENOTDIR:
		return (NFS3ERR_NOTDIR);
	case EISDIR:
		return (NFS3ERR_ISDIR);
	case EINVAL:
		return (NFS3ERR_INVAL);
	case EFBIG:
		return (NFS3ERR_FBIG);
	case ENOSPC:
		return (NFS3ERR_NOSPC);
	case EROFS:
		return (NFS3ERR_ROFS);
	case EMLINK:
		return (NFS3ERR_MLINK);
	case ENAMETOOLONG:
		return (NFS3ERR_NAMETOOLONG);
	case ENOTEMPTY:
		return (NFS3ERR_NOTEMPTY);
#ifdef EDQUOT
	case EDQUOT:
		return (NFS3ERR_DQUOT);
#endif
	case ESTALE:
		return (NFS3ERR_STALE);
	case EREMOTE:
		return (NFS3ERR_REMOTE);
	case EOPNOTSUPP:
		return (NFS3ERR_NOTSUPP);
	case ENOATTR:
		return (NFS3ERR_NOATTR);
	default:
		cmn_err(CE_WARN, "puterrno3: got error %d", error);
		return ((enum nfsstat3) error);
	}
#else
	switch (error) {
	case EDQUOT:
		return (NFS3ERR_DQUOT);
	case ENAMETOOLONG:
		return (NFS3ERR_NAMETOOLONG);
	case ENOTEMPTY:
		return (NFS3ERR_NOTEMPTY);
	case ESTALE:
		return (NFS3ERR_STALE);
	case EOPNOTSUPP:
		return (NFS3ERR_NOTSUPP);
	case EREMOTE:
		return (NFS3ERR_REMOTE);
	case EAGAIN:
		return (NFS3ERR_JUKEBOX);
	}
	return ((enum nfsstat3) error);
#endif
}
