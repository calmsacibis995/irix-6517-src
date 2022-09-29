/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/* $Revision */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)nfs_cnvt.c	1.14	93/07/06 SMI"	/* SVr4.0 1.4	*/
/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986, 1987, 1988, 1989, 1993  Sun Microsystems, Inc.
 *  	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */
#include "types.h"
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/mkdev.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/cmn_err.h>
#include "auth.h"               /* XXX required before clnt.h */
#include "clnt.h"
#include "xdr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "nfs3_rnode.h"
#include "netinet/in.h"
#include "nfs_stat.h"


void
vattr_to_sattr3(struct vattr *vap, struct sattr3 *sa)
{
	long mask = vap->va_mask;

	if (!(mask & AT_MODE)) {
		vap->va_mode = (mode_t) -1;
		sa->mode.set_it = FALSE;
	} else {
		sa->mode.set_it = TRUE;
		sa->mode.mode = (mode3)vap->va_mode;
	}
	if (!(mask & AT_UID)) {
		vap->va_uid = (uid_t)-1;
		sa->uid.set_it = FALSE;
	} else {
		sa->uid.set_it = TRUE;
		sa->uid.uid = (uid3)vap->va_uid;
	}
	if (!(mask & AT_GID)) {
		vap->va_gid = (gid_t)-1;
		sa->gid.set_it = FALSE;
	} else {
		sa->gid.set_it = TRUE;
		sa->gid.gid = (gid3)vap->va_gid;
	}
	if (!(mask & AT_SIZE)) {
		vap->va_size = (u_long) -1;
		sa->size.set_it = FALSE;
	} else {
		sa->size.set_it = TRUE;
		sa->size.size = (size3)vap->va_size;
	}
	if (!(mask & AT_ATIME)) {
		sa->atime.set_it = DONT_CHANGE;
	} else {
		sa->atime.set_it = SET_TO_CLIENT_TIME;
		sa->atime.atime.seconds = (uint32)vap->va_atime.tv_sec;
		sa->atime.atime.nseconds = (uint32)vap->va_atime.tv_nsec;
	}
	if (!(mask & AT_MTIME)) {
		sa->mtime.set_it = DONT_CHANGE;
	} else {
		sa->mtime.set_it = SET_TO_CLIENT_TIME;
		sa->mtime.mtime.seconds = (uint32)vap->va_mtime.tv_sec;
		sa->mtime.mtime.nseconds = (uint32)vap->va_mtime.tv_nsec;
	}
}

void
setdiropargs3(struct diropargs3 *da, char *nm, bhv_desc_t *dbdp)
{

	da->dir = *BHVTOFH3(dbdp);
	da->name = nm;
}

#define	NFS3_JUKEBOX_DELAY	10L * HZ

static long nfs3_jukebox_delay = 0;

/* XXX Temporary */
#define	uprintf	printf
/*ARGSUSED*/
int
rfs3call(mntinfo_t *mi, int which, xdrproc_t xdrargs, caddr_t argsp,
	xdrproc_t xdrres, caddr_t resp, cred_t *cr, int *douprintf,
	enum nfsstat3 *statusp, int frombio)
{
	int rpcerror;
	int user_informed;
	user_informed = 0;

	do {
		rpcerror = rfscall(mi, which, xdrargs, argsp, xdrres, resp,
				    cr, frombio);
		if (!rpcerror) {
			if (*statusp == NFS3ERR_JUKEBOX) {
				if (!user_informed && !(mi->mi_flags & MIF_SILENT)) {
					user_informed = 1;
					uprintf(
		"file temporarily unavailable on the server, retrying...\n");
				}
				delay(nfs3_jukebox_delay);
			}
		} else if (*statusp == NFS3ERR_ACCES &&
			    cr->cr_uid == 0 && cr->cr_ruid != 0) {
			/*
			 * Kludgy hack to implement setuid semantics
			 * If the original attempt failed for a setuid call,
			 * cook up some ruid creds and try again
			 */
			cr = crdup(cr);
			cr->cr_uid = cr->cr_ruid;
			rpcerror = rfscall(mi, which, xdrargs, argsp,
					   xdrres, resp, cr, frombio);
			crfree(cr);
		}
	} while (!rpcerror && *statusp == NFS3ERR_JUKEBOX);

	return (rpcerror);
}

int
nfs_subrinit(void)
{
	if (nfs3_jukebox_delay == 0L)
		nfs3_jukebox_delay = NFS3_JUKEBOX_DELAY;

	return (0);
}

int
geterrno3(enum nfsstat3 status)
{

#ifdef DEBUG
	switch (status) {
	case NFS3_OK:
		return (0);
	case NFS3ERR_PERM:
		return (EPERM);
	case NFS3ERR_NOENT:
		return (ENOENT);
	case NFS3ERR_IO:
		return (EIO);
	case NFS3ERR_NXIO:
		return (ENXIO);
	case NFS3ERR_ACCES:
		return (EACCES);
	case NFS3ERR_EXIST:
		return (EEXIST);
	case NFS3ERR_XDEV:
		return (EXDEV);
	case NFS3ERR_NODEV:
		return (ENODEV);
	case NFS3ERR_NOTDIR:
		return (ENOTDIR);
	case NFS3ERR_ISDIR:
		return (EISDIR);
	case NFS3ERR_INVAL:
		return (EINVAL);
	case NFS3ERR_FBIG:
		return (EFBIG);
	case NFS3ERR_NOSPC:
		return (ENOSPC);
	case NFS3ERR_ROFS:
		return (EROFS);
	case NFS3ERR_MLINK:
		return (EMLINK);
	case NFS3ERR_NAMETOOLONG:
		return (ENAMETOOLONG);
	case NFS3ERR_NOTEMPTY:
		return (ENOTEMPTY);
#ifdef EDQUOT
	case NFS3ERR_DQUOT:
		return (EDQUOT);
#endif
	case NFS3ERR_STALE:
		return (ESTALE);
	case NFS3ERR_REMOTE:
		return (EREMOTE);
	case NFS3ERR_NOATTR:
		return (ENOATTR);
	case NFS3ERR_BADHANDLE:
		return (ESTALE);
	case NFS3ERR_NOT_SYNC:
		return (EINVAL);
	case NFS3ERR_BAD_COOKIE:
		return (EINVAL);
	case NFS3ERR_NOTSUPP:
		return (EOPNOTSUPP);
	case NFS3ERR_TOOSMALL:
		return (EINVAL);
	case NFS3ERR_SERVERFAULT:
		return (EIO);
	case NFS3ERR_BADTYPE:
		return (EINVAL);
	case NFS3ERR_JUKEBOX:
		return (ENXIO);
	default:
		cmn_err(CE_WARN, "geterrno3: got status %d", status);
		return ((int) status);
	}
#else
	switch (status) {
	case NFS3ERR_DQUOT:
		return (EDQUOT);
	case NFS3ERR_NAMETOOLONG:
		return (ENAMETOOLONG);
	case NFS3ERR_NOTEMPTY:
		return (ENOTEMPTY);
	case NFS3ERR_STALE:
		return (ESTALE);
	case NFS3ERR_NOTSUPP:
		return (EOPNOTSUPP);
	case NFS3ERR_REMOTE:
		return (EREMOTE);
	case NFS3ERR_BADHANDLE:
	case NFS3ERR_NOT_SYNC:
	case NFS3ERR_BAD_COOKIE:
	case NFS3ERR_TOOSMALL:
	case NFS3ERR_SERVERFAULT:
	case NFS3ERR_BADTYPE:
		return ((int) status);
	}
	return ((int) status);
#endif
}

gid_t
setdirgid3(bhv_desc_t *dbdp)
{
	vnode_t *dvp;

	/*
	 * To determine the expected group-id of the created file:
	 *  1)	If the filesystem was not mounted with the Old-BSD-compatible
	 *	GRPID option, and the directory's set-gid bit is clear,
	 *	then use the process's gid.
	 *  2)	Otherwise, set the group-id to the gid of the parent directory.
	 */
	dvp = BHV_TO_VNODE(dbdp);
	if (!(dvp->v_vfsp->vfs_flag & VFS_GRPID) &&
	    !(bhvtor(dbdp)->r_attr.va_mode & VSGID)) {
		return (get_current_cred()->cr_gid);
	}
	return (bhvtor(dbdp)->r_attr.va_gid);
}

mode_t
setdirmode3(bhv_desc_t *dbdp, mode_t om)
{

	/*
	 * Modify the expected mode (om) so that the set-gid bit matches
	 * that of the parent directory (dvp).
	 */
	om &= ~VSGID;
	if (bhvtor(dbdp)->r_attr.va_mode & VSGID)
		om |= VSGID;
	return (om);
}
