/*	@(#)nfs_xdr.c	1.3 88/05/26 NFSSRC4.0 from 2.38 88/02/08 SMI 	*/

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#include "types.h"
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/kabi.h>
#include <sys/mbuf.h>
#include <sys/sema.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <netinet/in.h>		/* needed before xdr.h */
#include <sys/buf.h>
#include <sys/pfdat.h>
#include <sys/kmem.h>
#include <sys/atomic_ops.h>
#include "xdr.h"
#include "nfs.h"
#include "auth.h"
#include "svc.h"
#include "nfs_impl.h"

/*
 * These are the XDR routines used to serialize and deserialize
 * the various structures passed as parameters accross the network
 * between NFS clients and servers.
 */

/*
 * File access handle
 * The fhandle struct is treated a opaque data on the wire
 */
bool_t
xdr_fhandle(xdrs, fh)
	XDR *xdrs;
	fhandle_t *fh;
{

	if (xdr_opaque(xdrs, (caddr_t)fh, NFS_FHSIZE)) {
		return (TRUE);
	}
	return (FALSE);
}


/*
 * Arguments to remote write
 */
bool_t
xdr_writeargs(xdrs, wa)
	XDR *xdrs;
	struct nfswriteargs *wa;
{
	register bool_t result = FALSE;

	if (xdr_fhandle(xdrs, &wa->wa_fhandle)
	    && xdr_u_int(xdrs, &wa->wa_begoff)
	    && xdr_u_int(xdrs, &wa->wa_offset)
	    && xdr_u_int(xdrs, &wa->wa_totcount)) {
		switch (xdrs->x_op) {
		  case XDR_DECODE:	/* server */
			result = xdrmbuf_getmbuf(xdrs, 
						 &wa->wa_mbuf,
						 &wa->wa_count);
			break;
	
		  case XDR_FREE:	/* server */
			result = TRUE;
			break;

		  case XDR_ENCODE:
			/*
			 * Try to avoid a bcopy by using putbuf
			 */
			if (wa->wa_putbuf_ok)
				result = xdrmbuf_putbuf(xdrs, wa->wa_data, wa->wa_count);
			if (!result)
				result = (xdr_bytes(xdrs, &wa->wa_data,
					&wa->wa_count, NFS_MAXDATA));
			break;
		}
	}
	return (result);
}

#define IXDR_PUT_INT(buf, v) IXDR_PUT_LONG(buf, v)
#define IXDR_GET_INT(buf) IXDR_GET_LONG(buf)

/*
 * File attributes
 */
bool_t
xdr_fattr(xdrs, na)
	XDR *xdrs;
	register struct nfsfattr *na;
{
	register xdr_long_t *ptr;

	if (xdrs->x_op == XDR_ENCODE) {
		ptr = XDR_INLINE(xdrs, 17 * BYTES_PER_XDR_UNIT);
		if (ptr != NULL) {
			IXDR_PUT_ENUM(ptr, na->na_type);
			IXDR_PUT_INT(ptr, na->na_mode);
			IXDR_PUT_INT(ptr, na->na_nlink);
			IXDR_PUT_INT(ptr, na->na_uid);
			IXDR_PUT_INT(ptr, na->na_gid);
			IXDR_PUT_INT(ptr, na->na_size);
			IXDR_PUT_INT(ptr, na->na_blocksize);
			IXDR_PUT_INT(ptr, na->na_rdev);
			IXDR_PUT_INT(ptr, na->na_blocks);
			IXDR_PUT_INT(ptr, na->na_fsid);
			IXDR_PUT_INT(ptr, na->na_nodeid);
			IXDR_PUT_LONG(ptr, na->na_atime.tv_sec);
			IXDR_PUT_LONG(ptr, na->na_atime.tv_usec);
			IXDR_PUT_LONG(ptr, na->na_mtime.tv_sec);
			IXDR_PUT_LONG(ptr, na->na_mtime.tv_usec);
			IXDR_PUT_LONG(ptr, na->na_ctime.tv_sec);
			IXDR_PUT_LONG(ptr, na->na_ctime.tv_usec);
			return (TRUE);
		}
	} else {
		ptr = XDR_INLINE(xdrs, 17 * BYTES_PER_XDR_UNIT);
		if (ptr != NULL) {
			na->na_type = IXDR_GET_ENUM(ptr, enum nfsftype);
			na->na_mode = IXDR_GET_INT(ptr);
			na->na_nlink = IXDR_GET_INT(ptr);
			na->na_uid = IXDR_GET_INT(ptr);
			na->na_gid = IXDR_GET_INT(ptr);
			na->na_size = IXDR_GET_INT(ptr);
			na->na_blocksize = IXDR_GET_INT(ptr);
			na->na_rdev = IXDR_GET_INT(ptr);
			na->na_blocks = IXDR_GET_INT(ptr);
			na->na_fsid = IXDR_GET_INT(ptr);
			na->na_nodeid = IXDR_GET_INT(ptr);
			na->na_atime.tv_sec = IXDR_GET_LONG(ptr);
			na->na_atime.tv_usec = IXDR_GET_LONG(ptr);
			na->na_mtime.tv_sec = IXDR_GET_LONG(ptr);
			na->na_mtime.tv_usec = IXDR_GET_LONG(ptr);
			na->na_ctime.tv_sec = IXDR_GET_LONG(ptr);
			na->na_ctime.tv_usec = IXDR_GET_LONG(ptr);
			return (TRUE);
		}
	}
	if (xdr_enum(xdrs, (enum_t *)&na->na_type) &&
	    xdr_u_int(xdrs, &na->na_mode) &&
	    xdr_u_int(xdrs, &na->na_nlink) &&
	    xdr_u_int(xdrs, &na->na_uid) &&
	    xdr_u_int(xdrs, &na->na_gid) &&
	    xdr_u_int(xdrs, &na->na_size) &&
	    xdr_u_int(xdrs, &na->na_blocksize) &&
	    xdr_u_int(xdrs, &na->na_rdev) &&
	    xdr_u_int(xdrs, &na->na_blocks) &&
	    xdr_u_int(xdrs, &na->na_fsid) &&
	    xdr_u_int(xdrs, &na->na_nodeid) &&
	    xdr_irix5_timeval(xdrs, &na->na_atime) &&
	    xdr_irix5_timeval(xdrs, &na->na_mtime) &&
	    xdr_irix5_timeval(xdrs, &na->na_ctime) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Arguments to remote read
 */
bool_t
xdr_readargs(xdrs, ra)
	XDR *xdrs;
	struct nfsreadargs *ra;
{

	if (xdr_fhandle(xdrs, &ra->ra_fhandle) &&
	    xdr_u_int(xdrs, &ra->ra_offset) &&
	    xdr_u_int(xdrs, &ra->ra_count) &&
	    xdr_u_int(xdrs, &ra->ra_totcount) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Status OK portion of remote read reply
 */
bool_t
xdr_rrok(xdrs, rrok)
	XDR *xdrs;
	struct nfsrrok *rrok;
{
	bool_t retval;
	struct buf *bp = rrok->rrok_bp;
	struct vnode *vp;

	if (!xdr_fattr(xdrs, &rrok->rrok_attr)) {
		return (FALSE);
	}

	if (xdrs->x_op != XDR_ENCODE || rrok->rrok_count == 0) {
		/* client side or EOF */
		if (!xdr_bytes(xdrs, &rrok->rrok_data,
			      &rrok->rrok_count, NFS_MAXDATA) ) {
			return (FALSE);
		}
		return (TRUE);
	}

	if (bp == NULL) {
		bool_t retval;
		/* server side VOP_READ path */

		retval = xdrmbuf_putbuf(xdrs, rrok->rrok_data, rrok->rrok_count);
		if (!retval)
			retval = xdr_bytes(xdrs, &rrok->rrok_data,
					   &rrok->rrok_count, NFS_MAXDATA);
		kvpfree(rrok->rrok_data, btoc(rrok->rrok_bsize));
		return (retval);
        }

	/* server side VOP_READBUF path */
	rrok->rrok_data = bp_mapin(bp) + rrok->rrok_bsize;
	retval = xdrmbuf_putbuf(xdrs, rrok->rrok_data, rrok->rrok_count);
	if (retval == FALSE)
		retval = xdr_bytes(xdrs, &rrok->rrok_data,
				   &rrok->rrok_count, NFS_MAXDATA);

	vp = bp->b_vp;
	brelse(bp);
	VN_RELE(vp);
	return (retval);
}

struct xdr_discrim rdres_discrim[2] = {
	{ (int)NFS_OK, xdr_rrok },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Reply from remote read
 */
bool_t
xdr_rdresult(xdrs, rr)
	XDR *xdrs;
	struct nfsrdresult *rr;
{

	if (xdr_union(xdrs, (enum_t *)&(rr->rr_status),
	      (caddr_t)&(rr->rr_ok), rdres_discrim, xdr_void) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * File attributes which can be set
 */
bool_t
xdr_sattr(xdrs, sa)
	XDR *xdrs;
	struct nfssattr *sa;
{
	if (xdr_u_int(xdrs, &sa->sa_mode) &&
	    xdr_u_int(xdrs, &sa->sa_uid) &&
	    xdr_u_int(xdrs, &sa->sa_gid) &&
	    xdr_u_int(xdrs, &sa->sa_size) &&
	    xdr_irix5_timeval(xdrs, &sa->sa_atime) &&
	    xdr_irix5_timeval(xdrs, &sa->sa_mtime) ) {
		return (TRUE);
	}
	return (FALSE);
}

struct xdr_discrim attrstat_discrim[2] = {
	{ (int)NFS_OK, xdr_fattr },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Reply status with file attributes
 */
bool_t
xdr_attrstat(xdrs, ns)
	XDR *xdrs;
	struct nfsattrstat *ns;
{

	if (xdr_union(xdrs, (enum_t *)&(ns->ns_status),
	      (caddr_t)&(ns->ns_attr), attrstat_discrim, xdr_void) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * NFS_OK part of read sym link reply union
 */
bool_t
xdr_srok(xdrs, srok)
	XDR *xdrs;
	struct nfssrok *srok;
{

	auto u_int count;

	count = srok->srok_count;
	if (xdr_bytes(xdrs, &srok->srok_data, &count,
	    NFS_MAXPATHLEN) ) {
		srok->srok_count = count;
		return (TRUE);
	}
	return (FALSE);
}

struct xdr_discrim rdlnres_discrim[2] = {
	{ (int)NFS_OK, xdr_srok },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Result of reading symbolic link
 */
bool_t
xdr_rdlnres(xdrs, rl)
	XDR *xdrs;
	struct nfsrdlnres *rl;
{

	if (xdr_union(xdrs, (enum_t *)&(rl->rl_status),
	      (caddr_t)&(rl->rl_srok), rdlnres_discrim, xdr_void) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Arguments to readdir
 */
bool_t
xdr_rddirargs(xdrs, rda)
	XDR *xdrs;
	struct nfsrddirargs *rda;
{

	if (xdr_fhandle(xdrs, &rda->rda_fh) &&
	    xdr_u_int(xdrs, &rda->rda_offset) &&
	    xdr_u_int(xdrs, &rda->rda_count) ) {
		return (TRUE);
	}
	return (FALSE);
}

/* 
 * System V.3 directory entries:
 *	struct  dirent {
 *		ino_t	d_ino;		// inode number of entry
 *		off_t	d_off;		// offset of this record
 *		u_short d_reclen;	// length of this record
 *		char    d_name[1];	// name, consistent with reclen
 *	};
 *
 * are on the wire as:
 *	union entlist (boolean valid) {
 * 		TRUE:	struct nfsdirent;
 *			u_long nxtoffset;
 * 			union entlist;
 *		FALSE:
 *	}
 *
 * where nfsdirent is:
 * 	struct nfsdirent {
 *		u_long	de_fid;
 *		string	de_name<NFS_MAXNAMELEN>;
 *	}
 *
 * Thus a directory read reply is:
 *	union (enum status) {
 *		NFS_OK: union entlist;
 *			boolean eof;
 *		default:
 *	}
 */
#define	nextdp(dp)	((struct dirent *)((char *)(dp) + (dp)->d_reclen))
#define	irix5_nextdp(irix5_dp)	\
	((struct irix5_dirent *)((char *)(irix5_dp) + (irix5_dp)->d_reclen))

static bool_t
xdr_ino_t(
	XDR *xdrs,
	ino_t *ip)
{
#if (_MIPS_SZLONG != 64)
	__uint32_t	ino;
#endif

	if (sizeof(ino_t) == sizeof(u_long)) {
		return (xdr_u_long(xdrs, (u_long *)ip));
	}
#pragma set woff 1110	/* For the next 2 blocks, the paranoia level
	* is good, but results in "not reachable" complaints from the
	* compiler, which turn into errors because we promote them.  Since
	* the paranoia level is good, we just disable the warning around this
	* block */
#if (_MIPS_SZLONG != _MIPS_SZINT)
	if (sizeof(ino_t) == sizeof(u_int)) {
		return (xdr_u_int(xdrs, (u_int *)ip));
	}
#endif
#if (_MIPS_SZLONG != 64)
	if (sizeof(ino_t) == sizeof(__uint64_t)) {
		if (xdrs->x_op == XDR_DECODE) {
			if (!xdr_u_int(xdrs, &ino)) {
				return (FALSE);
			}
			*ip = (ino_t)ino;
			return (TRUE);
		} else {
			ino = (__uint32_t)(*ip);
			return (xdr_u_int(xdrs, &ino));
		}
	}
#endif
	return (FALSE);
#pragma reset woff 1110
}

/*
 * ENCODE ONLY (server side)
 */
bool_t
xdr_putrddirres(xdrs, rd)
	XDR *xdrs;
	struct nfsrddirres *rd;
{
	struct dirent *dp;
	char *endbuf;
	int size;
	int more = TRUE;

	ASSERT(xdrs->x_op == XDR_ENCODE);

	if (!XDR_PUTINT(xdrs, &rd->rd_status)) {
		return (FALSE);
	}
	if (rd->rd_status != NFS_OK) {
		return (TRUE);
	}

	/* address of the end of the dirent buffer */
	endbuf = (char *)rd->rd_entries+rd->rd_bufsize-1;

	size = rd->rd_bufsize;
	for (dp = rd->rd_entries;
	     rd->rd_size > 0;
	     rd->rd_size -= dp->d_reclen, dp = nextdp(dp) ) {
		int namlen, rnduplen;
		int ino, doff; /* truncated to 32 bits as per protocol */

		namlen = strlen(dp->d_name);
		size -= 4*BYTES_PER_XDR_UNIT;
		rnduplen = RNDUP(namlen);
		size -= rnduplen;
		if (size < 0 || (dp->d_name + rnduplen) > endbuf) {
			/*
			 * Make sure we do not send too much to the
			 * client (more than its bufsize), or go off the end
			 * of the dirent buffer in XDR_PUTBYTES().
			 */
			rd->rd_eof = FALSE;
			break;
		}
		ino = dp->d_ino;
		doff = dp->d_off;
		if (!XDR_PUTINT(xdrs, &more) ||
		    !XDR_PUTINT(xdrs, &ino) ||
		    !XDR_PUTINT(xdrs, &namlen) ||
		    !XDR_PUTBYTES(xdrs, dp->d_name, rnduplen) ||
		    !XDR_PUTINT(xdrs, &doff)) {
			return (FALSE);
		}
	}
	more = 0;
	if (!XDR_PUTINT(xdrs, &more)) {
		return (FALSE);
	}
	if (!XDR_PUTINT(xdrs, &rd->rd_eof)) {
		return (FALSE);
	}
	return (TRUE);
}

static bool_t xdr_fmtdirent(XDR *, void **, int *, u_long *);
static bool_t irix5_xdr_fmtdirent(XDR *, void **, int *, u_long *);

/*
 * DECODE ONLY
 */
bool_t
xdr_getrddirres(xdrs, rd)
	XDR *xdrs;
	struct nfsrddirres *rd;
{
	char *dp;
	int size;
	bool_t valid;
	u_long offset;
	int (*dirent_func)(XDR *, void **, int *, u_long *);

	if (!xdr_enum(xdrs, (enum_t *)&rd->rd_status)) {
		return (FALSE);
	}
	if (rd->rd_status != NFS_OK) {
		return (TRUE);
	}

	/* The dirent structs differ between the 32 bit abi and the kernel,
	 * so we have to know who our client is to do generate
	 * correct directory entries.
	 */
	switch (rd->rd_abi) {
	case ABI_IRIX5_64:
	case ABI_IRIX5_N32:
		dirent_func = xdr_fmtdirent;
		break;

	case ABI_IRIX5:
		dirent_func = irix5_xdr_fmtdirent;
		break;
	}
	size = rd->rd_size;
	dp = (char *) rd->rd_entries;
	for (;;) {
		if (! xdr_bool(xdrs, &valid)) {
			return (FALSE);
		}
		if (! valid) {
			break;	/* end of entlist */
		}
		if (dirent_func(xdrs, (void **)&dp, &size, &offset) == FALSE)
			return FALSE;
	}
	if (!xdr_bool(xdrs, &rd->rd_eof)) {
		return (FALSE);
	}
	rd->rd_size = dp - (char *)(rd->rd_entries);
	rd->rd_offset = offset;
	return (TRUE);
}

/*
 * Arguments for directory operations
 */
bool_t
xdr_diropargs(xdrs, da)
	XDR *xdrs;
	struct nfsdiropargs *da;
{
	register char *name = da->da_name;

	if (xdr_fhandle(xdrs, &da->da_fhandle) &&
	    xdr_string(xdrs, &da->da_name, NFS_MAXNAMLEN) ) {
		if (name == 0 && da->da_name != 0) {
			name = da->da_name;
		}
		if (xdrs->x_op != XDR_FREE && name[0] == '\0') {
			return (FALSE);
		}
		return (TRUE);
	}
	return (FALSE);
}

/*
 * NFS_OK part of directory operation result
 */
bool_t
xdr_drok(xdrs, drok)
	XDR *xdrs;
	struct nfsdrok *drok;
{

	if (xdr_fhandle(xdrs, &drok->drok_fhandle) &&
	    xdr_fattr(xdrs, &drok->drok_attr) ) {
		return (TRUE);
	}
	return (FALSE);
}

struct xdr_discrim diropres_discrim[2] = {
	{ (int)NFS_OK, xdr_drok },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Results from directory operation 
 */
bool_t
xdr_diropres(xdrs, dr)
	XDR *xdrs;
	struct nfsdiropres *dr;
{

	if (xdr_union(xdrs, (enum_t *)&(dr->dr_status),
	      (caddr_t)&(dr->dr_drok), diropres_discrim, xdr_void) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Time structure
 */
bool_t
xdr_irix5_timeval(xdrs, tv)
	XDR *xdrs;
	struct irix5_timeval *tv;
{

	if (xdr_int(xdrs, &tv->tv_sec) &&
	    xdr_int(xdrs, &tv->tv_usec) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * arguments to setattr
 */
bool_t
xdr_saargs(xdrs, argp)
	XDR *xdrs;
	struct nfssaargs *argp;
{

	if (xdr_fhandle(xdrs, &argp->saa_fh) &&
	    xdr_sattr(xdrs, &argp->saa_sa) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * arguments to create and mkdir
 */
bool_t
xdr_creatargs(xdrs, argp)
	XDR *xdrs;
	struct nfscreatargs *argp;
{

	if (xdr_diropargs(xdrs, &argp->ca_da) &&
	    xdr_sattr(xdrs, &argp->ca_sa) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * arguments to link
 */
bool_t
xdr_linkargs(xdrs, argp)
	XDR *xdrs;
	struct nfslinkargs *argp;
{

	if (xdr_fhandle(xdrs, &argp->la_from) &&
	    xdr_diropargs(xdrs, &argp->la_to) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * arguments to rename
 */
bool_t
xdr_rnmargs(xdrs, argp)
	XDR *xdrs;
	struct nfsrnmargs *argp;
{

	if (xdr_diropargs(xdrs, &argp->rna_from) &&
	    xdr_diropargs(xdrs, &argp->rna_to) ) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * arguments to symlink
 */
bool_t
xdr_slargs(xdrs, argp)
	XDR *xdrs;
	struct nfsslargs *argp;
{

	if (xdr_diropargs(xdrs, &argp->sla_from) &&
	    xdr_string(xdrs, &argp->sla_tnm, (u_int)NFS_MAXPATHLEN) &&
	    xdr_sattr(xdrs, &argp->sla_sa)) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * NFS_OK part of statfs operation
 */
bool_t
xdr_fsok(xdrs, fsok)
	XDR *xdrs;
	struct nfsstatfsok *fsok;
{

	if (xdr_long(xdrs, (long *)&fsok->fsok_tsize) &&
	    xdr_long(xdrs, (long *)&fsok->fsok_bsize) &&
	    xdr_long(xdrs, (long *)&fsok->fsok_blocks) &&
	    xdr_long(xdrs, (long *)&fsok->fsok_bfree) &&
	    xdr_long(xdrs, (long *)&fsok->fsok_bavail) ) {
		return (TRUE);
	}
	return (FALSE);
}

struct xdr_discrim statfs_discrim[2] = {
	{ (int)NFS_OK, xdr_fsok },
	{ __dontcare__, NULL_xdrproc_t }
};

/*
 * Results of statfs operation
 */
bool_t
xdr_statfs(xdrs, fs)
	XDR *xdrs;
	struct nfsstatfs *fs;
{

	if (xdr_union(xdrs, (enum_t *)&(fs->fs_status),
	      (caddr_t)&(fs->fs_fsok), statfs_discrim, xdr_void)) {
		return (TRUE);
	}
	return (FALSE);
}

static bool_t
xdr_fmtdirent(
	XDR *xdrs,
	void **vp,
	int *sizep,
	u_long *noffp)
{
	struct dirent *dp = *vp;
	u_long offset;
	int reclen;
	u_int namlen;

	if (!xdr_ino_t(xdrs, &dp->d_ino) ||
	    !xdr_u_int(xdrs, &namlen) ||
		(namlen > NFS_MAXNAMLEN) ||
	    ((reclen = DIRENTSIZE(namlen)) > *sizep) ||
	    !xdr_opaque(xdrs, (caddr_t)dp->d_name, (u_int)namlen) ||
	    !xdr_u_long(xdrs, &offset)) {
		return (FALSE);
	}
	dp->d_off = offset;
	dp->d_reclen = reclen;
	dp->d_name[namlen] = '\0';
	*sizep -= reclen;
	if (*sizep <= 0) {
		return (FALSE);
	}
	*vp = nextdp(dp);
	*noffp = offset;
	return TRUE;
}

static bool_t
irix5_xdr_fmtdirent(
	XDR *xdrs,
	void **vp,
	int *sizep,
	u_long *noffp)
{
	struct irix5_dirent *dp = *vp;
	u_long offset;
	int reclen;
	u_int namlen;

	if (!xdr_u_int(xdrs, &dp->d_ino) ||
	    !xdr_u_int(xdrs, &namlen) ||
		(namlen > NFS_MAXNAMLEN) ||
	    ((reclen = IRIX5_DIRENTSIZE(namlen)) > *sizep) ||
	    !xdr_opaque(xdrs, (caddr_t)dp->d_name, (u_int)namlen) ||
	    !xdr_u_long(xdrs, &offset)) {
		return (FALSE);
	}
	dp->d_off = offset;
	dp->d_reclen = reclen;
	dp->d_name[namlen] = '\0';
	*sizep -= reclen;
	if (*sizep <= 0) {
		return (FALSE);
	}
	*vp = irix5_nextdp(dp);
	*noffp = offset;
	return TRUE;
}
