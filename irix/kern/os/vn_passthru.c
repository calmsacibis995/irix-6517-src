/*****************************************************************************
 * vn_passthru.c
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

#ident "$Revision: 1.31 $"

/*
 * Interposition pass through functions that could be used by the 
 * behavior layers not intending to do any thing for a large number
 * of functions.  This avoids redoing these functions in every layer.
 */

#include <limits.h>
#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <ksys/ddmap.h>
#include <sys/pfdat.h>
#include <sys/vnode.h>
#include <sys/debug.h>
#include <sys/systm.h>                  /* splhi */
#include <sys/vnode.h>
#include <sys/pvnode.h>
#include <ksys/vfile.h>                   /* FWRITE */
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/pvfs.h>

/*ARGSUSED*/
int
vn_passthru_vop_open(
	bhv_desc_t 	*bdp,
	vnode_t		**vpp,
	mode_t		mode,
	cred_t 		*credp)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_open);

	PVOP_OPEN(nbdp, vpp, mode, credp, error);
	return error;
}
	

/* ARGSUSED */
int
vn_passthru_vop_close(
	bhv_desc_t *bdp,
	int flag,
	lastclose_t lastclose,
	struct cred *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_close);

	PVOP_CLOSE(nbdp, flag, lastclose, cr, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_read(
	bhv_desc_t 	*bdp,
	struct uio 	*uiop,
	int 		ioflag,
	struct cred 	*cr,
	struct flid 	*fl)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_read);

	PVOP_READ(nbdp, uiop, ioflag, cr, fl, error);
	return error;

}

/* ARGSUSED */
int
vn_passthru_vop_write(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr,
	struct flid *fl)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_write);

	PVOP_WRITE(nbdp, uiop, ioflag, cr, fl, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_ioctl(
	bhv_desc_t *bdp,
	int cmd,
	void *arg,
	int flag,
	struct cred *cr,
	int *rvalp,
        struct vopbd *vbds)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_ioctl);

	PVOP_IOCTL(nbdp, cmd, arg, flag, cr, rvalp, vbds, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_setfl(
	bhv_desc_t 	*bdp,
	int 		oflags,
	int 		nflags,
	cred_t 		*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_setfl);

	PVOP_SETFL(nbdp, oflags, nflags, cr, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_getattr(
	bhv_desc_t 	*bdp,
	struct vattr 	*vap,
	int 		flags,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_getattr);

	PVOP_GETATTR(nbdp, vap, flags, cr, error);
	return error;
}

int
vn_passthru_vop_setattr(
	bhv_desc_t 	*bdp,
	struct vattr 	*vap,
	int 		flags,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_setattr);

	PVOP_SETATTR(nbdp, vap, flags, cr, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_access(
	bhv_desc_t 	*bdp,
	int 		mode,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_access);

	PVOP_ACCESS(nbdp, mode, cr, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_lookup(
	bhv_desc_t 	*bdp,
	char 		*nm,
	struct vnode 	**vpp,
	struct pathname *pnp,
	int 		flags,
	struct vnode 	*rdir,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_lookup);

	PVOP_LOOKUP(nbdp, nm, vpp, pnp, flags, rdir, cr, error);
	return error;
}

int
vn_passthru_vop_create(
	bhv_desc_t 	*bdp,
	char 		*name,
	struct vattr 	*vap,
	int 		flags,
	int 		mode,
	struct vnode 	**vpp,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_create);

	PVOP_CREATE(nbdp, name, vap, flags, mode, vpp, cr, error);
	return error;
}


/* ARGSUSED */
int
vn_passthru_vop_remove(
	bhv_desc_t 	*bdp,
	char 		*nm,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_remove);

	PVOP_REMOVE(nbdp, nm, cr, error);
	return error;
}

int
vn_passthru_vop_link(
	bhv_desc_t 	*bdp,
	struct vnode 	*svp,
	char 		*tnm,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_link);

	PVOP_LINK(nbdp, svp, tnm, cr, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_rename(
	bhv_desc_t *bdp,		/* old (source) parent vnode */
	char *snm,			/* old (source) entry name */
	struct vnode *tdvp,		/* new (target) parent vnode */
	char *tnm,			/* new (target) entry name */
	struct pathname *tpnp,		/* new (target) pathname or null */
	struct cred *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_rename);

	PVOP_RENAME(nbdp, snm, tdvp, tnm, tpnp, cr, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_mkdir(
	bhv_desc_t 	*bdp,
	char 		*dirname,
	struct vattr 	*vap,
	struct vnode 	**vpp,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_mkdir);

	PVOP_MKDIR(nbdp, dirname, vap, vpp, cr, error);
	return error;
}

int
vn_passthru_vop_rmdir(
	bhv_desc_t 	*bdp,
	char 		*nm,
	struct vnode 	*cdir,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_rmdir);

	PVOP_RMDIR(nbdp, nm, cdir, cr, error);
	return error;
}

int
vn_passthru_vop_readdir(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	struct cred	*cr,
	int		*eofp)
{

	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_readdir);

	PVOP_READDIR(nbdp, uiop, cr, eofp, error);
	return error;

}

/* ARGSUSED */
int
vn_passthru_vop_symlink(
	bhv_desc_t 	*bdp,		/* ptr to parent dir vnode */
	char 		*linkname,	/* name of symbolic link */
	struct vattr 	*vap,		/* attributes */
	char 		*target,	/* target path */
	struct cred 	*cr)		/* user credentials */
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_symlink);

	PVOP_SYMLINK(nbdp, linkname, vap, target, cr, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_readlink(
	bhv_desc_t *bdp,
	struct uio *uiop,
	struct cred *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_readlink);

	PVOP_READLINK(nbdp, uiop, cr, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_fsync(
	bhv_desc_t 	*bdp,
	int 		flag,
	struct cred 	*cr,
	off_t		start,
	off_t		stop)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_fsync);

	PVOP_FSYNC(nbdp, flag, cr, start, stop, error);
	return error;
}

/* ARGSUSED */
int
vn_passthru_vop_inactive(
	bhv_desc_t 	*bdp,
	struct cred 	*cr)
{
	bhv_desc_t	*nbdp;
	int		rv;

	PV_NEXT(bdp, nbdp, vop_inactive);

	PVOP_INACTIVE(nbdp, cr, rv);

	return rv;
}

int
vn_passthru_vop_fid(
	bhv_desc_t *bdp,
	struct fid **fidpp)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_fid);

	PVOP_FID(nbdp, fidpp, error);
	return error;
}

int
vn_passthru_vop_fid2(
	bhv_desc_t 	*bdp,
	struct fid 	*fidp)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_fid2);

	PVOP_FID2(nbdp, fidp, error);
	return error;
}

/* ARGSUSED */
void
vn_passthru_vop_rwlock(
	bhv_desc_t 	*bdp, 
	vrwlock_t 	write_lock)
{
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_rwlock);

	PVOP_RWLOCK(nbdp, write_lock);
}

/* ARGSUSED */
void
vn_passthru_vop_rwunlock(
	bhv_desc_t 	*bdp, 
	vrwlock_t 	write_lock)
{
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_rwunlock);

	PVOP_RWUNLOCK(nbdp, write_lock);
}

/* ARGSUSED */
int
vn_passthru_vop_seek(
	bhv_desc_t 	*bdp,
	off_t 		ooff,
	off_t 		*noffp)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_seek);

	PVOP_SEEK(nbdp, ooff, noffp, error);
	return error;
}

/*ARGSUSED*/
int
vn_passthru_vop_cmp(
	bhv_desc_t	*bdp,
	vnode_t		*vp)
{
	bhv_desc_t	*nbdp;
	int		equal;

	PV_NEXT(bdp, nbdp, vop_cmp);

	PVOP_CMP(nbdp, vp, equal);
	return equal;
}

int
vn_passthru_vop_frlock(
	bhv_desc_t 	*bdp,
	int 		cmd,
	struct flock 	*lfp,
	int 		flag,
	off_t 		offset,
	vrwlock_t	vrwlock,
	cred_t 		*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_frlock);

	PVOP_FRLOCK(nbdp, cmd, lfp, flag, offset, vrwlock, cr, error);
	return error;
}

int
vn_passthru_vop_realvp(
	bhv_desc_t	*bdp,
	vnode_t		**vpp)
{

	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_realvp);

	PVOP_REALVP(nbdp, vpp, error);
	return error;
}

/*ARGSUSED*/
int
vn_passthru_vop_bmap(
	bhv_desc_t	*bdp,
	off_t		offs,
	ssize_t		count,
	int		flags,
	cred_t		*credp,
	struct bmapval	*bmapp,
	int		*nbmaps)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_bmap);

	PVOP_BMAP(nbdp, offs, count, flags, credp, bmapp, nbmaps, error);
	return error;
}

/*ARGSUSED*/
void
vn_passthru_vop_strategy(
	bhv_desc_t 	*bdp, 
	struct buf 	*bp)
{
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_strategy);

	PVOP_STRATEGY(nbdp, bp);
}

/* ARGSUSED */
int
vn_passthru_vop_map(
	bhv_desc_t *bdp,
	off_t off,
	size_t len,
	mprot_t prot,
	u_int flags,
	struct cred *cr,
	vnode_t **nvp)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_map);

	PVOP_MAP(nbdp, off, len, prot, flags, cr, nvp, error);
	return error;
}

int
vn_passthru_vop_addmap(
	bhv_desc_t *bdp,
	vaddmap_t op,
	vhandl_t *vt,
	pgno_t *pgno,
	off_t off,
	size_t len,
	mprot_t prot,
	struct cred *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_addmap);

	PVOP_ADDMAP(nbdp, op, vt, pgno, off, len, prot, cr, error);
	return error;
}

int
vn_passthru_vop_delmap(
	bhv_desc_t *bdp,
	vhandl_t *vt,
	size_t len,
	struct cred *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_delmap);

	PVOP_DELMAP(nbdp, vt, len, cr, error);
	return error;
}

/*ARGSUSED*/
int
vn_passthru_vop_poll(
	bhv_desc_t	*bdp,
	short 		events,
	int 		anyyet,
	short 		*reventsp,
	struct pollhead **phpp,
	unsigned int	*genp)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_poll);

	PVOP_POLL(nbdp, events, anyyet, reventsp, phpp, genp, error);
	return error;
}

/*ARGSUSED*/
int
vn_passthru_vop_dump(
	bhv_desc_t	*bdp,
	caddr_t		addr,
	daddr_t		daddr,
	u_int		flag)
{

	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_dump);

	PVOP_DUMP(nbdp, addr, daddr, flag, error);
	return error;
}
	

/*ARGSUSED*/
int
vn_passthru_vop_pathconf(
	bhv_desc_t	*bdp,
	int 		cmd, 
	long 		*valp, 
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_pathconf);

	PVOP_PATHCONF(nbdp, cmd, valp, cr, error);
	return error;
}



/* ARGSUSED */
int
vn_passthru_vop_allocstore(
	bhv_desc_t 	*bdp,
	off_t 		off,
	size_t 		len,
	struct cred 	*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_allocstore);

	PVOP_ALLOCSTORE(nbdp, off, len, cr, error);
	return error;
}


/* ARGSUSED */
int
vn_passthru_vop_fcntl(
	bhv_desc_t 	*bdp,
	int 		cmd,
	void 		*arg,
	int 		flags,
	off_t 		offset,
	cred_t 		*cr,
	rval_t 		*rvp)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_fcntl);
	ASSERT(nbdp);

	PVOP_FCNTL(nbdp, cmd, arg, flags, offset, cr, rvp, error);
	return error;
}

int
vn_passthru_vop_reclaim(
	bhv_desc_t	*bdp,
	int		flag)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_reclaim);

	PVOP_RECLAIM(nbdp, flag, error);
	return error;
}

/*ARGSUSED*/
int
vn_passthru_vop_attr_get(
	bhv_desc_t	*bdp,
	char 		*name, 
	char 		*value, 
	int 		*valuelenp, 
	int 		flags,
	struct cred 	*cred)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_attr_get);

	PVOP_ATTR_GET(nbdp, name, value, valuelenp, flags, cred, error);
	return error;
}

/*ARGSUSED*/
int
vn_passthru_vop_attr_set(
	bhv_desc_t 	*bdp, 
	char 		*name, 
	char 		*value, 
	int 		valuelen, 
	int 		flags,
	struct cred 	*cred)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_attr_set);

	PVOP_ATTR_SET(nbdp, name, value, valuelen, flags, cred, error);
	return error;
}

/*ARGSUSED*/
int                                                             /* error */
vn_passthru_vop_attr_remove(
	bhv_desc_t 	*bdp, 
	char 		*name, 
	int 		flags, 
	struct cred 	*cred)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_attr_remove);

	PVOP_ATTR_REMOVE(nbdp, name, flags, cred, error);
	return error;
}

/*ARGSUSED*/
int                                                             /* error */
vn_passthru_vop_attr_list(
	bhv_desc_t 	*bdp, 
	char 		*buffer, 
	int 		bufsize, 
	int 		flags,
	struct attrlist_cursor_kern * cursor,
	struct cred 	*cred)
{ 
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_attr_list);

	PVOP_ATTR_LIST(nbdp, buffer, bufsize, flags, cursor, cred, error);
	return error;
}

/*ARGSUSED*/
int                                                             /* error */
vn_passthru_vop_cover(
	bhv_desc_t 	*bdp, 
	struct mounta 	*uap,
	char 		*attrs, 
	cred_t  	*cred)
{ 
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_cover);

	PVOP_COVER(nbdp, uap, attrs, cred, error);
	return error;
}

/*ARGSUSED*/
void
vn_passthru_vop_link_removed(
	bhv_desc_t 	*bdp, 
	vnode_t	 	*dvp,
	int		linkzero)
{ 
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_link_removed);

	PVOP_LINK_REMOVED(nbdp, dvp, linkzero);
}

/*ARGSUSED*/
void
vn_passthru_vop_vnode_change(
	bhv_desc_t 	*bdp, 
	vchange_t 	cmd,
	__psint_t	val)
{ 
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_vnode_change);

	PVOP_VNODE_CHANGE(nbdp, cmd, val);
}

/*ARGSUSED*/
void
vn_passthru_vop_tosspages(
	bhv_desc_t 	*bdp, 
	off_t 		first,
	off_t 		last,
	int		fiopt)
{ 
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_tosspages);

	PVOP_TOSS_PAGES(nbdp, first, last, fiopt);
}

/*ARGSUSED*/
void
vn_passthru_vop_flushinval_pages(
	bhv_desc_t 	*bdp, 
	off_t 		first,
	off_t 		last,
	int		fiopt)
{ 
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_flushinval_pages);

	PVOP_FLUSHINVAL_PAGES(nbdp, first, last, fiopt);
}

/*ARGSUSED*/
int
vn_passthru_vop_flush_pages(
	bhv_desc_t 	*bdp, 
	off_t 		first,
	off_t 		last,
	uint64_t	flags,
	int		fiopt)
{ 
	bhv_desc_t	*nbdp;
	int		rv;

	PV_NEXT(bdp, nbdp, vop_flush_pages);

	PVOP_FLUSH_PAGES(nbdp, first, last, flags, fiopt, rv);
	return(rv);
}

/*ARGSUSED*/
void
vn_passthru_vop_invalfree_pages(
	bhv_desc_t 	*bdp, 
	off_t 		filesize)
{ 
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_invalfree_pages);

	PVOP_INVALFREE_PAGES(nbdp, filesize);
}

/*ARGSUSED*/
void
vn_passthru_vop_pages_sethole(
	bhv_desc_t 	*bdp, 
	pfd_t	 	*pfd, 
	int 		cnt,
	int		doremap,
	off_t		remapoffset)
{ 
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_pages_sethole);

	PVOP_PAGES_SETHOLE(nbdp, pfd, cnt, doremap, remapoffset);
}

/*ARGUSED*/
int
vn_passthru_vop_commit(
	bhv_desc_t	*bdp,
	struct buf	*bp)
{
	bhv_desc_t	*nbdp;
	int		rv;

	PV_NEXT(bdp, nbdp, vop_commit);

	PVOP_COMMIT(nbdp, bp, rv);

	return rv;
}

/*ARGUSED*/
int
vn_passthru_vop_readbuf(
	bhv_desc_t 	*bdp,
	off_t		offset,
	ssize_t		count,
	int		flags,
	struct cred 	*credp,
	struct flid	*flid,
	struct buf	**bp,
	int		*pboff,
	int		*pbsize)
{
	bhv_desc_t 	*nbdp;
	int		rv;

	PV_NEXT(bdp, nbdp, vop_readbuf);

	PVOP_READBUF(nbdp, offset, count, flags, credp, flid,
			bp, pboff, pbsize, rv);
	
	return rv;
}

int
vn_passthru_vop_strgetmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	int             error;
        bhv_desc_t      *nbdp;

	PV_NEXT(bdp, nbdp, vop_strgetmsg);

        PVOP_STRGETMSG(nbdp, mctl, mdata, prip, flagsp, fmode, rvp, error);

        return error;
}

int
vn_passthru_vop_strputmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	int             error;
        bhv_desc_t      *nbdp;

	PV_NEXT(bdp, nbdp, vop_strputmsg);

        PVOP_STRPUTMSG(nbdp, mctl, mdata, pri, flag, fmode, error);

        return error;
}

vnodeops_t vn_passthru_ops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_INVALID),
		/* position - must be set before use */
        vn_passthru_vop_open,        
        vn_passthru_vop_close,       
        vn_passthru_vop_read,        
        vn_passthru_vop_write,       
        vn_passthru_vop_ioctl,       
        vn_passthru_vop_setfl,       
        vn_passthru_vop_getattr,     
        vn_passthru_vop_setattr,     
        vn_passthru_vop_access,      
        vn_passthru_vop_lookup,      
        vn_passthru_vop_create,      
        vn_passthru_vop_remove,      
        vn_passthru_vop_link,        
        vn_passthru_vop_rename,      
        vn_passthru_vop_mkdir,       
        vn_passthru_vop_rmdir,       
        vn_passthru_vop_readdir,     
        vn_passthru_vop_symlink,     
        vn_passthru_vop_readlink,    
        vn_passthru_vop_fsync,       
        vn_passthru_vop_inactive,    
        vn_passthru_vop_fid,         
        vn_passthru_vop_fid2,        
        vn_passthru_vop_rwlock,      
        vn_passthru_vop_rwunlock,    
        vn_passthru_vop_seek,        
        vn_passthru_vop_cmp,         
        vn_passthru_vop_frlock,      
        vn_passthru_vop_realvp,      
        vn_passthru_vop_bmap,        
        vn_passthru_vop_strategy,    
        vn_passthru_vop_map,         
        vn_passthru_vop_addmap,      
        vn_passthru_vop_delmap,      
        vn_passthru_vop_poll,        
        vn_passthru_vop_dump,        
        vn_passthru_vop_pathconf,    
        vn_passthru_vop_allocstore,  
        vn_passthru_vop_fcntl,       
        vn_passthru_vop_reclaim,     
        vn_passthru_vop_attr_get,    
        vn_passthru_vop_attr_set,    
        vn_passthru_vop_attr_remove, 
        vn_passthru_vop_attr_list,   
        vn_passthru_vop_cover,       
	vn_passthru_vop_link_removed,
	vn_passthru_vop_vnode_change,
	vn_passthru_vop_tosspages,
	vn_passthru_vop_flushinval_pages,
	vn_passthru_vop_flush_pages,
	vn_passthru_vop_invalfree_pages,
	vn_passthru_vop_pages_sethole,
	vn_passthru_vop_commit,
	vn_passthru_vop_readbuf,
	vn_passthru_vop_strgetmsg,
	vn_passthru_vop_strputmsg,
};

vnodeops_t *vn_passthrup = &vn_passthru_ops;

/************************************************************************
 * VFS pass-through ops
 ************************************************************************/
#include <sys/pvfs.h>

/*ARGSUSED*/
int                                                             /* error */
vfs_passthru_vfs_mntupdate(
	bhv_desc_t *bdp,
        struct vnode *mvp,
        struct mounta *uap,
        struct cred *cr)
{ 
	int		error;
	bhv_desc_t	*nbdp;

	PVFS_NEXT(bdp, nbdp);
	PVFS_MNTUPDATE(nbdp, mvp, uap, cr, error);
	return error;
}


int
vfs_passthru_vfs_dounmount(
	bhv_desc_t	*bdp,
	int		flags,
	struct vnode    *rootvp,
	cred_t		*cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PVFS_NEXT(bdp, nbdp);
	PVFS_DOUNMOUNT(nbdp, flags, rootvp, cr, error);
	return error;
}

int
vfs_passthru_vfs_unmount(
	bhv_desc_t	*bdp,
	int		flags,
	cred_t		*cr)
{
	int             error;
        bhv_desc_t      *nbdp;

        PVFS_NEXT(bdp, nbdp);
        PVFS_UNMOUNT(nbdp, flags, cr, error);
        return error;
}

int
vfs_passthru_vfs_root(
	bhv_desc_t	*bdp,
	vnode_t		**vpp)
{
	int             error;
        bhv_desc_t      *nbdp;

        PVFS_NEXT(bdp, nbdp);
        PVFS_ROOT(nbdp, vpp, error);
        return error;
}

int
vfs_passthru_vfs_statvfs(
	bhv_desc_t	*bdp,
	struct statvfs	*sp,
	vnode_t		*vp)
{
	int             error;
        bhv_desc_t      *nbdp;

        PVFS_NEXT(bdp, nbdp);
        PVFS_STATVFS(nbdp, sp, vp, error);
        return error;
}

int
vfs_passthru_vfs_sync(
	bhv_desc_t	*bdp,
	int		flags,
	cred_t		*cr)
{
	int             error;
        bhv_desc_t      *nbdp;

        PVFS_NEXT(bdp, nbdp);
        PVFS_SYNC(nbdp, flags, cr, error);
        return error;
}

int
vfs_passthru_vfs_vget(
        bhv_desc_t      *bdp,
        vnode_t         **vpp,
        struct fid      *fidp)
{
        int             error;
        bhv_desc_t      *nbdp;

        PVFS_NEXT(bdp, nbdp);
        PVFS_VGET(nbdp, vpp, fidp, error);
        return error;
}

int
vfs_passthru_vfs_mountroot(
	bhv_desc_t	*bdp,
	enum whymountroot why)
{
	int             error;
        bhv_desc_t      *nbdp;

        PVFS_NEXT(bdp, nbdp);
        PVFS_MOUNTROOT(nbdp, why, error);
        return error;
}

