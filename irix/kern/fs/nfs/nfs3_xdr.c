/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 * $Revision: 1.22 $
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
 *  	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc.
 *  	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#include "types.h"
#include <sys/buf.h>            /* for B_ASYNC */
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/pfdat.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <sys/dirent.h>
#include <sys/kabi.h>
#include "auth_unix.h"          /* for NGRPS in nfs_init */
#include "xdr.h"
#define SVCFH
#include "nfs.h"
#include "auth.h"
#include "svc.h"
#include "export.h"
#include "nfs_clnt.h"
#include "nfs3_clnt.h"
#include "rnode.h"
#include "nfs_impl.h"
#include "string.h"

extern bool_t	xdr_nfs_fh3(register XDR *, nfs_fh3 *);

#define xdr_filename3(xdrs,objp) xdr_string(xdrs, objp, NFS_MAXNAMLEN)
#define xdr_nfspath3(xdrs,objp) xdr_string(xdrs, objp, NFS_MAXPATHLEN)

#define xdr_fileid3(xdrs,objp) xdr_u_longlong_t(xdrs, objp)
#define xdr_cookie3(xdrs,objp) xdr_u_longlong_t(xdrs, objp)
#define xdr_uid3(xdrs, objp)   xdr_uint32(xdrs, objp)
#define xdr_gid3(xdrs, objp)   xdr_uint32(xdrs, objp)
#define xdr_size3(xdrs,objp)   xdr_u_longlong_t(xdrs, objp)
#define xdr_offset3(xdrs,objp) xdr_u_longlong_t(xdrs, objp)
#define xdr_mode3(xdrs, objp)  xdr_uint32(xdrs, objp)
#define xdr_count3(xdrs, objp) xdr_uint32(xdrs, objp)

#define xdr_cookieverf3(xdrs, objp) xdr_opaque(xdrs, \
				     objp, NFS3_COOKIEVERFSIZE)

__inline bool_t
xdr_nfstime3(register XDR *xdrs, nfstime3 *objp)
{
	if (!xdr_uint32(xdrs, &objp->seconds))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->nseconds))
		return (FALSE);
	return (TRUE);
}

__inline bool_t
xdr_specdata3(register XDR *xdrs, specdata3 *objp)
{

	if (!xdr_uint32(xdrs, &objp->specdata1))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->specdata2))
		return (FALSE);
	return (TRUE);
}

#define xdr_ftype3(xdrs, objp)   xdr_enum(xdrs, (enum_t *)objp)
#define xdr_ftype3(xdrs, objp)   xdr_enum(xdrs, (enum_t *)objp)

xdr_fattr3(register XDR *xdrs, fattr3 *objp)
{
	if (!xdr_ftype3(xdrs, &objp->type))
		return (FALSE);
	if (!xdr_mode3(xdrs, &objp->mode))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->nlink))
		return (FALSE);
	if (!xdr_uid3(xdrs, &objp->uid))
		return (FALSE);
	if (!xdr_gid3(xdrs, &objp->gid))
		return (FALSE);
	if (!xdr_size3(xdrs, &objp->size))
		return (FALSE);
	if (!xdr_size3(xdrs, &objp->used))
		return (FALSE);
	if (!xdr_specdata3(xdrs, &objp->rdev))
		return (FALSE);
	if (!xdr_u_longlong_t(xdrs, &objp->fsid))
		return (FALSE);
	if (!xdr_fileid3(xdrs, &objp->fileid))
		return (FALSE);
	if (!xdr_nfstime3(xdrs, &objp->atime))
		return (FALSE);
	if (!xdr_nfstime3(xdrs, &objp->mtime))
		return (FALSE);
	if (!xdr_nfstime3(xdrs, &objp->ctime))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_post_op_attr(register XDR *xdrs, post_op_attr *objp)
{
	if (!xdr_bool(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	switch (objp->attributes) {
		case TRUE:
			if (!xdr_fattr3(xdrs, &objp->attr)) {
				return (FALSE);
			}
			break;
		case FALSE:
			break;
		default:
			return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_wcc_attr(register XDR *xdrs, wcc_attr *objp)
{

	if (!xdr_size3(xdrs, &objp->size))
		return (FALSE);
	if (!xdr_nfstime3(xdrs, &objp->mtime))
		return (FALSE);
	if (!xdr_nfstime3(xdrs, &objp->ctime))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_pre_op_attr(register XDR *xdrs, pre_op_attr *objp)
{

	if (!xdr_bool(xdrs, &objp->attributes))
		return (FALSE);
	switch (objp->attributes) {
	case TRUE:
		if (!xdr_wcc_attr(xdrs, &objp->attr))
			return (FALSE);
		break;
	case FALSE:
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_wcc_data(register XDR *xdrs, wcc_data *objp)
{

	if (!xdr_pre_op_attr(xdrs, &objp->before))
		return (FALSE);
	if (!xdr_post_op_attr(xdrs, &objp->after))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_post_op_fh3(register XDR *xdrs, post_op_fh3 *objp)
{

	if (!xdr_bool(xdrs, &objp->handle_follows))
		return (FALSE);
	switch (objp->handle_follows) {
	case TRUE:
		if (!xdr_nfs_fh3(xdrs, &objp->handle))
			return (FALSE);
		break;
	case FALSE:
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_createverf3(register XDR *xdrs, createverf3 objp)
{

	if (!xdr_opaque(xdrs, objp, NFS3_CREATEVERFSIZE))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_writeverf3(register XDR *xdrs, writeverf3 objp)
{

	if (!xdr_opaque(xdrs, objp, NFS3_WRITEVERFSIZE))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_nfs_fh3(register XDR *xdrs, nfs_fh3 *objp)
{

	if (!xdr_u_int(xdrs, &objp->fh3_length)) {
		return (FALSE);
	}

	if (objp->fh3_length > NFS3_FHSIZE) {
		return (FALSE);
	}

	if (xdrs->x_op == XDR_DECODE || xdrs->x_op == XDR_ENCODE) {
		return (xdr_opaque(xdrs, objp->fh3_u.data, objp->fh3_length));
	}

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

bool_t
xdr_diropargs3(register XDR *xdrs, diropargs3 *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->dir))
		return (FALSE);
	if (!xdr_filename3(xdrs, &objp->name))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_time_how(register XDR *xdrs, time_how *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_set_mode3(register XDR *xdrs, set_mode3 *objp)
{

	if (!xdr_bool(xdrs, &objp->set_it))
		return (FALSE);
	switch (objp->set_it) {
	case TRUE:
		if (!xdr_mode3(xdrs, &objp->mode))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_set_uid3(register XDR *xdrs, set_uid3 *objp)
{

	if (!xdr_bool(xdrs, &objp->set_it))
		return (FALSE);
	switch (objp->set_it) {
	case TRUE:
		if (!xdr_uid3(xdrs, &objp->uid))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_set_gid3(register XDR *xdrs, set_gid3 *objp)
{

	if (!xdr_bool(xdrs, &objp->set_it))
		return (FALSE);
	switch (objp->set_it) {
	case TRUE:
		if (!xdr_gid3(xdrs, &objp->gid))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_set_size3(register XDR *xdrs, set_size3 *objp)
{

	if (!xdr_bool(xdrs, &objp->set_it))
		return (FALSE);
	switch (objp->set_it) {
	case TRUE:
		if (!xdr_size3(xdrs, &objp->size))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_set_atime(register XDR *xdrs, set_atime *objp)
{

	if (!xdr_time_how(xdrs, &objp->set_it))
		return (FALSE);
	switch (objp->set_it) {
	case SET_TO_CLIENT_TIME:
		if (!xdr_nfstime3(xdrs, &objp->atime))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_set_mtime(register XDR *xdrs, set_mtime *objp)
{

	if (!xdr_time_how(xdrs, &objp->set_it))
		return (FALSE);
	switch (objp->set_it) {
	case SET_TO_CLIENT_TIME:
		if (!xdr_nfstime3(xdrs, &objp->mtime))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_sattr3(register XDR *xdrs, sattr3 *objp)
{

	if (!xdr_set_mode3(xdrs, &objp->mode))
		return (FALSE);
	if (!xdr_set_uid3(xdrs, &objp->uid))
		return (FALSE);
	if (!xdr_set_gid3(xdrs, &objp->gid))
		return (FALSE);
	if (!xdr_set_size3(xdrs, &objp->size))
		return (FALSE);
	if (!xdr_set_atime(xdrs, &objp->atime))
		return (FALSE);
	if (!xdr_set_mtime(xdrs, &objp->mtime))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_GETATTR3args(register XDR *xdrs, GETATTR3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_GETATTR3resok(register XDR *xdrs, GETATTR3resok *objp)
{

	if (!xdr_fattr3(xdrs, &objp->obj_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_GETATTR3res(register XDR *xdrs, GETATTR3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_GETATTR3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_sattrguard3(register XDR *xdrs, sattrguard3 *objp)
{

	if (!xdr_bool(xdrs, &objp->check))
		return (FALSE);
	switch (objp->check) {
	case TRUE:
		if (!xdr_nfstime3(xdrs, &objp->obj_ctime))
			return (FALSE);
		break;
	case FALSE:
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_SETATTR3args(register XDR *xdrs, SETATTR3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	if (!xdr_sattr3(xdrs, &objp->new_attributes))
		return (FALSE);
	if (!xdr_sattrguard3(xdrs, &objp->guard))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_SETATTR3resok(register XDR *xdrs, SETATTR3resok *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->obj_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_SETATTR3resfail(register XDR *xdrs, SETATTR3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->obj_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_SETATTR3res(register XDR *xdrs, SETATTR3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_SETATTR3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_SETATTR3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_READDIR3args(register XDR *xdrs, READDIR3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->dir))
		return (FALSE);
	if (!xdr_cookie3(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_cookieverf3(xdrs, objp->cookieverf))
		return (FALSE);
	if (!xdr_count3(xdrs, &objp->count))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_LOOKUP3args(register XDR *xdrs, LOOKUP3args *objp)
{

	if (!xdr_diropargs3(xdrs, &objp->what))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_LOOKUP3resok(register XDR *xdrs, LOOKUP3resok *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_LOOKUP3resfail(register XDR *xdrs, LOOKUP3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_LOOKUP3res(register XDR *xdrs, LOOKUP3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_LOOKUP3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_LOOKUP3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_ACCESS3args(register XDR *xdrs, ACCESS3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->access))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ACCESS3resok(register XDR *xdrs, ACCESS3resok *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->access))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ACCESS3resfail(register XDR *xdrs, ACCESS3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ACCESS3res(register XDR *xdrs, ACCESS3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_ACCESS3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_ACCESS3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_READLINK3args(register XDR *xdrs, READLINK3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->symlink)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_READLINK3resok(register XDR *xdrs, READLINK3resok *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->symlink_attributes))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->data, MAXPATHLEN))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_READLINK3resfail(register XDR *xdrs, READLINK3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->symlink_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_READLINK3res(register XDR *xdrs, READLINK3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_READLINK3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_READLINK3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_READ3args(register XDR *xdrs, READ3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->file))
		return (FALSE);
	if (!xdr_offset3(xdrs, &objp->offset))
		return (FALSE);
	if (!xdr_count3(xdrs, &objp->count))
		return (FALSE);
	return (TRUE);
}

extern void gather_chunk_alloc_pages(buf_t *bp);
extern void gather_chunk_insert_pages(buf_t *bp, pfd_t *pfd);

bool_t
xdr_READ3resok(register XDR *xdrs, READ3resok *objp)
{
	int retval;
	struct buf *bp = objp->bp;
	struct vnode *vp;

	if (!xdr_post_op_attr(xdrs, &objp->file_attributes))
		return (FALSE);
	if (!xdr_count3(xdrs, &objp->count))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->eof))
		return (FALSE);
	if (xdrs->x_op != XDR_ENCODE || objp->count == 0) {
		if (xdrs->x_op == XDR_DECODE && objp->data.data_val == NULL) {
			pfd_t *pfd;

			if (xdrmbuf_getpfdchain(xdrs, &pfd)) {
				gather_chunk_insert_pages(bp, pfd);
				return (TRUE);
			}
			gather_chunk_alloc_pages(bp);
			objp->data.data_val = bp->b_un.b_addr;
		}
		if (!xdr_bytes(xdrs, (char **)&objp->data.data_val,
			       (u_int *)&objp->data.data_len, ~0))
			return (FALSE);
		return(TRUE);
	}

	if (bp == NULL) {
		/* server side VOP_READ path */
		retval = xdrmbuf_putbuf(xdrs, objp->data.data_val, objp->count);
		if (retval == FALSE)
			retval = xdr_bytes(xdrs, &objp->data.data_val,
					   &objp->count, ~0);
		kvpfree(objp->data.data_val, btoc(objp->data.data_len));
		return (retval);
	}

	/* server side VOP_READBUF path */
	objp->data.data_val = bp_mapin(bp) + objp->pboff;
	retval = xdrmbuf_putbuf(xdrs, objp->data.data_val, objp->count);
	if (retval == FALSE)
		retval = xdr_bytes(xdrs, &objp->data.data_val, &objp->count, ~0);

	vp = bp->b_vp;
	brelse(bp);
	VN_RELE(vp);
	return (retval);
}

bool_t
xdr_READ3resfail(register XDR *xdrs, READ3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->file_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_READ3res(register XDR *xdrs, READ3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_READ3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_READ3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_stable_how(register XDR *xdrs, stable_how *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_WRITE3args(register XDR *xdrs, WRITE3args *objp)
{
	bool_t result = FALSE;
	u_int count;

	if (!xdr_nfs_fh3(xdrs, &objp->file))
		return (FALSE);
	if (!xdr_offset3(xdrs, &objp->offset))
		return (FALSE);
	if (!xdr_count3(xdrs, &objp->count))
		return (FALSE);
	if (!xdr_stable_how(xdrs, &objp->stable))
		return (FALSE);
	switch (xdrs->x_op) {
	case XDR_DECODE:      /* server */
		result = xdrmbuf_getmbuf(xdrs, &objp->mbuf, &count);
		ASSERT(count == objp->count);
		break;

	case XDR_FREE:        /* server */
		result = TRUE;
		break;

	case XDR_ENCODE: 
		/*
		 * Try to avoid a bcopy by using putbuf
		 */
		if (objp->data.putbuf_ok)
			result = xdrmbuf_putbuf(xdrs, objp->data.data_val,
						objp->data.data_len);
		if (result == FALSE)
			result = xdr_bytes(xdrs, 
				(char **)&objp->data.data_val,
				(u_int *)&objp->data.data_len, ~0);
		break;
	}
	return (result);
}

bool_t
xdr_WRITE3resok(register XDR *xdrs, WRITE3resok *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->file_wcc))
		return (FALSE);
	if (!xdr_count3(xdrs, &objp->count))
		return (FALSE);
	if (!xdr_stable_how(xdrs, &objp->committed))
		return (FALSE);
	if (!xdr_writeverf3(xdrs, objp->verf))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_WRITE3resfail(register XDR *xdrs, WRITE3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->file_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_WRITE3res(register XDR *xdrs, WRITE3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_WRITE3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_WRITE3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_createmode3(register XDR *xdrs, createmode3 *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_createhow3(register XDR *xdrs, createhow3 *objp)
{

	if (!xdr_createmode3(xdrs, &objp->mode))
		return (FALSE);
	switch (objp->mode) {
	case UNCHECKED:
	case GUARDED:
		if (!xdr_sattr3(xdrs, &objp->createhow3_u.obj_attributes))
			return (FALSE);
		break;
	case EXCLUSIVE:
		if (!xdr_createverf3(xdrs, objp->createhow3_u.verf))
			return (FALSE);
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_CREATE3args(register XDR *xdrs, CREATE3args *objp)
{

	if (!xdr_diropargs3(xdrs, &objp->where))
		return (FALSE);
	if (!xdr_createhow3(xdrs, &objp->how))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_CREATE3resok(register XDR *xdrs, CREATE3resok *objp)
{

	if (!xdr_post_op_fh3(xdrs, &objp->obj))
		return (FALSE);
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_CREATE3resfail(register XDR *xdrs, CREATE3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_CREATE3res(register XDR *xdrs, CREATE3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_CREATE3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_CREATE3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_MKDIR3args(register XDR *xdrs, MKDIR3args *objp)
{

	if (!xdr_diropargs3(xdrs, &objp->where))
		return (FALSE);
	if (!xdr_sattr3(xdrs, &objp->attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_MKDIR3resok(register XDR *xdrs, MKDIR3resok *objp)
{

	if (!xdr_post_op_fh3(xdrs, &objp->obj))
		return (FALSE);
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_MKDIR3resfail(register XDR *xdrs, MKDIR3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_MKDIR3res(register XDR *xdrs, MKDIR3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_MKDIR3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_MKDIR3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_symlinkdata3(register XDR *xdrs, symlinkdata3 *objp)
{

	if (!xdr_sattr3(xdrs, &objp->symlink_attributes))
		return (FALSE);
	if (!xdr_nfspath3(xdrs, &objp->symlink_data))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_SYMLINK3args(register XDR *xdrs, SYMLINK3args *objp)
{

	if (!xdr_diropargs3(xdrs, &objp->where))
		return (FALSE);
	if (!xdr_symlinkdata3(xdrs, &objp->symlink))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_SYMLINK3resok(register XDR *xdrs, SYMLINK3resok *objp)
{

	if (!xdr_post_op_fh3(xdrs, &objp->obj))
		return (FALSE);
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_SYMLINK3resfail(register XDR *xdrs, SYMLINK3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_SYMLINK3res(register XDR *xdrs, SYMLINK3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_SYMLINK3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_SYMLINK3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_devicedata3(register XDR *xdrs, devicedata3 *objp)
{

	if (!xdr_sattr3(xdrs, &objp->dev_attributes))
		return (FALSE);
	if (!xdr_specdata3(xdrs, &objp->spec))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_mknoddata3(register XDR *xdrs, mknoddata3 *objp)
{

	if (!xdr_ftype3(xdrs, &objp->type))
		return (FALSE);
	switch (objp->type) {
	case NF3CHR:
	case NF3BLK:
		if (!xdr_devicedata3(xdrs, &objp->mknoddata3_u.device))
			return (FALSE);
		break;
	case NF3SOCK:
	case NF3FIFO:
		if (!xdr_sattr3(xdrs, &objp->mknoddata3_u.pipe_attributes))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_MKNOD3args(register XDR *xdrs, MKNOD3args *objp)
{

	if (!xdr_diropargs3(xdrs, &objp->where))
		return (FALSE);
	if (!xdr_mknoddata3(xdrs, &objp->what))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_MKNOD3resok(register XDR *xdrs, MKNOD3resok *objp)
{

	if (!xdr_post_op_fh3(xdrs, &objp->obj))
		return (FALSE);
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_MKNOD3resfail(register XDR *xdrs, MKNOD3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_MKNOD3res(register XDR *xdrs, MKNOD3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_MKNOD3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_MKNOD3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_REMOVE3args(register XDR *xdrs, REMOVE3args *objp)
{

	if (!xdr_diropargs3(xdrs, &objp->object))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_REMOVE3resok(register XDR *xdrs, REMOVE3resok *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_REMOVE3resfail(register XDR *xdrs, REMOVE3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_REMOVE3res(register XDR *xdrs, REMOVE3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_REMOVE3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_REMOVE3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_RMDIR3args(register XDR *xdrs, RMDIR3args *objp)
{

	if (!xdr_diropargs3(xdrs, &objp->object))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_RMDIR3resok(register XDR *xdrs, RMDIR3resok *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_RMDIR3resfail(register XDR *xdrs, RMDIR3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->dir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_RMDIR3res(register XDR *xdrs, RMDIR3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_RMDIR3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_RMDIR3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_RENAME3args(register XDR *xdrs, RENAME3args *objp)
{

	if (!xdr_diropargs3(xdrs, &objp->from))
		return (FALSE);
	if (!xdr_diropargs3(xdrs, &objp->to))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_RENAME3resok(register XDR *xdrs, RENAME3resok *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->fromdir_wcc))
		return (FALSE);
	if (!xdr_wcc_data(xdrs, &objp->todir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_RENAME3resfail(register XDR *xdrs, RENAME3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->fromdir_wcc))
		return (FALSE);
	if (!xdr_wcc_data(xdrs, &objp->todir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_RENAME3res(register XDR *xdrs, RENAME3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_RENAME3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_RENAME3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_LINK3args(register XDR *xdrs, LINK3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->file))
		return (FALSE);
	if (!xdr_diropargs3(xdrs, &objp->link))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_LINK3resok(register XDR *xdrs, LINK3resok *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->file_attributes))
		return (FALSE);
	if (!xdr_wcc_data(xdrs, &objp->linkdir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_LINK3resfail(register XDR *xdrs, LINK3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->file_attributes))
		return (FALSE);
	if (!xdr_wcc_data(xdrs, &objp->linkdir_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_LINK3res(register XDR *xdrs, LINK3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_LINK3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_LINK3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

#ifdef nextdp
#undef nextdp
#endif
#if _IRIX5
#define	nextdp5(dp)	((struct irix5_dirent *)((char *)(dp) + (dp)->d_reclen))
#endif /* _IRIX5  */
#define	nextdp(dp)	((struct dirent *)((char *)(dp) + (dp)->d_reclen))
#undef DIRSIZ
#define	DIRSIZ(dp)	((dp)->d_reclen)
#ifdef roundup
#undef roundup
#endif
#define	roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))

/*
 * ENCODE ONLY
 */
bool_t
xdr_putdirlist(register XDR *xdrs, READDIR3resok *objp)
{
	struct dirent *dp;
	char *name;
	int size;
	int bufsize;
	uint namlen;
	bool_t true = TRUE;
	bool_t false = FALSE;
	int entrysz;
	int tofit;
	fileid3 fileid;
	cookie3 cookie;
	if (xdrs->x_op != XDR_ENCODE) {
		return (FALSE);
	}

	bufsize = (1 + 1 + 2) * BYTES_PER_XDR_UNIT;
	if (objp->dir_attributes.attributes)
		bufsize += 21 * BYTES_PER_XDR_UNIT;
	for (size = objp->size, dp = (struct dirent *)objp->reply.entries;
	    size > 0;
	    size -= dp->d_reclen, dp = nextdp(dp)) {
		if (dp->d_reclen == 0 /* || DIRSIZ(dp) > dp->d_reclen */) {
			return (FALSE);
		}
		if (dp->d_ino == 0) {
			continue;
		}
		name = dp->d_name;
		namlen = strlen(dp->d_name);
		entrysz = (1 + 2 + 1 + 2) * BYTES_PER_XDR_UNIT +
			    roundup(namlen, BYTES_PER_XDR_UNIT);
		tofit = entrysz + 2 * BYTES_PER_XDR_UNIT;
		if (bufsize + tofit > objp->count) {
			objp->reply.eof = FALSE;
			break;
		}
		fileid = (fileid3)(dp->d_ino);
		cookie = (cookie3)(dp->d_off);

		if (!xdr_bool(xdrs, &true) ||
		    !xdr_fileid3(xdrs, &fileid) ||
		    !xdr_bytes(xdrs, &name, &namlen, ~0) ||
		    !xdr_cookie3(xdrs, &cookie)) {
			return (FALSE);
		}
		bufsize += entrysz;
	}
	if (!xdr_bool(xdrs, &false)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->reply.eof)) {
		return (FALSE);
	}
	return (TRUE);
}

#define	reclen64(namlen)	roundto64int(dirsz + (namlen) + 1)

/*
 * DECODE ONLY
 */
bool_t
xdr_getdirlist(register XDR *xdrs, READDIR3resok *objp)
{
	struct dirent *dp;
	uint namlen;
	int size;
	bool_t valid;
	int dirsz;
	fileid3 fileid;
	cookie3 cookie;

	if (xdrs->x_op != XDR_DECODE) {
		return (FALSE);
	}
	size = objp->size;
	dp = (struct dirent *)objp->reply.entries;
	cookie = objp->cookie;
	dirsz = dp->d_name - (char *)dp;
	for (;;) {
		if (!xdr_bool(xdrs, &valid)) {
			return (FALSE);
		}
		if (!valid) {
			break;
		}
		if (!xdr_fileid3(xdrs, &fileid) ||
			!xdr_u_int(xdrs, &namlen)) {
			return (FALSE);
		}
		if ((namlen > NFS_MAXNAMLEN) || (reclen64(namlen) > size)) {
			objp->reply.eof = FALSE;
			goto bufovflow;
		}
		if (!xdr_opaque(xdrs, dp->d_name, namlen)) {
			return (FALSE);
		}
		if (!xdr_cookie3(xdrs, &cookie)) {
			    return (FALSE);
		}
		dp->d_ino = (ino_t)fileid;
		dp->d_reclen = reclen64(namlen);
		dp->d_name[namlen] = '\0';
		dp->d_off = (off_t)cookie;
		size -= dp->d_reclen;
		dp = nextdp(dp);
	}
	
	if (!xdr_bool(xdrs, &objp->reply.eof)) {
		return (FALSE);
	}
bufovflow:
	objp->size = (char *)dp - (char *)(objp->reply.entries);
	objp->cookie = cookie;
	return (TRUE);
}

bool_t
xdr_READDIR3resok(register XDR *xdrs, READDIR3resok *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
		return (FALSE);
	if (!xdr_cookieverf3(xdrs, objp->cookieverf))
		return (FALSE);
	if (xdrs->x_op == XDR_DECODE) {
		if (!xdr_getdirlist(xdrs, objp))
			return (FALSE);
	} else if (xdrs->x_op == XDR_ENCODE) {
		if (!xdr_putdirlist(xdrs, objp))
			return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_READDIR3resfail(register XDR *xdrs, READDIR3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_READDIR3res(register XDR *xdrs, READDIR3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_READDIR3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_READDIR3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_READDIRPLUS3args(register XDR *xdrs, READDIRPLUS3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->dir))
		return (FALSE);
	if (!xdr_cookie3(xdrs, &objp->cookie))
		return (FALSE);
	if (!xdr_cookieverf3(xdrs, objp->cookieverf))
		return (FALSE);
	if (!xdr_count3(xdrs, &objp->dircount))
		return (FALSE);
	if (!xdr_count3(xdrs, &objp->maxcount))
		return (FALSE);
	return (TRUE);
}

/*
 * ENCODE ONLY
 */
bool_t
xdr_putdirpluslist(register XDR *xdrs, READDIRPLUS3resok *objp)
{
	struct dirent *dp;
	char *name;
	int nents;
	int bufsize;
	uint namlen;
	bool_t true = TRUE;
	bool_t false = FALSE;
	int entrysz;
	int tofit;
	fileid3 fileid;
	cookie3 cookie;
	post_op_attr *atp;
	post_op_fh3 *fhp;

	ASSERT(xdrs->x_op == XDR_ENCODE);

	bufsize = (1 + 1 + 2) * BYTES_PER_XDR_UNIT;
	if (objp->dir_attributes.attributes)
		bufsize += 21 * BYTES_PER_XDR_UNIT;
	dp = (struct dirent *)objp->reply.entries;
	nents = objp->size;
	atp = objp->attributes;
	fhp = objp->handles;
	while (nents > 0) {
		if (dp->d_reclen == 0 /* || DIRSIZ(dp) > dp->d_reclen */) {
			return (FALSE);
		}
		if (dp->d_ino == 0) {
			dp = nextdp(dp);
			atp++;
			fhp++;
			nents--;
			continue;
		}
		name = dp->d_name;
		namlen = strlen(dp->d_name);
		entrysz = (1 + 2 + 1 + 2) * BYTES_PER_XDR_UNIT +
			    roundup(namlen, BYTES_PER_XDR_UNIT);
		if (atp->attributes) {
			entrysz += ((1 + 21) * BYTES_PER_XDR_UNIT);
		} else {
			entrysz += (1 * BYTES_PER_XDR_UNIT);
		}
		if (fhp->handle_follows) {
			entrysz += ((1 * BYTES_PER_XDR_UNIT) +
				(1 * BYTES_PER_XDR_UNIT) +
				RNDUP(fhp->handle.fh3_length));
		} else {
			entrysz += (1 * BYTES_PER_XDR_UNIT);
		}
		tofit = entrysz + 2 * BYTES_PER_XDR_UNIT;
		if (bufsize + tofit > objp->maxcount) {
			objp->reply.eof = FALSE;
			break;
		}
		fileid = (fileid3)(dp->d_ino);
		cookie = (cookie3)(dp->d_off);
		if (!xdr_bool(xdrs, &true) ||
		    !xdr_fileid3(xdrs, &fileid) ||
		    !xdr_bytes(xdrs, &name, &namlen, ~0) ||
		    !xdr_cookie3(xdrs, &cookie) ||
		    !xdr_post_op_attr(xdrs, atp) ||
		    !xdr_post_op_fh3(xdrs, fhp)) {
			return (FALSE);
		}
		dp = nextdp(dp);
		atp++;
		fhp++;
		nents--;
		bufsize += entrysz;
	}
	if (!xdr_bool(xdrs, &false)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->reply.eof)) {
		return (FALSE);
	}
	return (TRUE);
}

/*
 * DECODE ONLY
 */
bool_t
xdr_getdirpluslist(register XDR *xdrs, READDIRPLUS3resok *objp)
{
	struct dirent *dp;
	uint namlen;
	int size;
	bool_t valid;
	int dirsz;
	fileid3 fileid;
	cookie3 cookie;
	char *bufp;
	post_op_attr at;
	post_op_fh3 fh;
	char *tbufp;

	if (xdrs->x_op != XDR_DECODE) {
		return (FALSE);
	}

	size = objp->size;
	bufp = (char *)objp->reply.entries;
	dp = (struct dirent *)NULL;
	dirsz = dp->d_name - (char *)dp;
	for (;;) {
		if (!xdr_bool(xdrs, &valid)) {
			return (FALSE);
		}
		if (!valid) {
			break;
		}
		if (!xdr_fileid3(xdrs, &fileid) ||
		    !xdr_u_int(xdrs, &namlen)) {
			return (FALSE);
		}
		dp = (struct dirent *)roundto64int(bufp);
		size -= (char *)dp - (char *)(bufp);
		if (reclen64(namlen) > size) {
			objp->reply.eof = FALSE;
			goto bufovflow;
		}
		if (!xdr_opaque(xdrs, dp->d_name, namlen) ||
		    !xdr_cookie3(xdrs, &cookie)) {
			return (FALSE);
		}
		dp->d_ino = (ino_t)fileid;
		dp->d_reclen = reclen64(namlen);
		dp->d_name[namlen] = '\0';
		dp->d_off = (off_t)cookie;
		size -= dp->d_reclen;

		tbufp = (char *)roundto64int(bufp + dp->d_reclen);
		size -= (char *)tbufp - (char *)(bufp+dp->d_reclen);

		if (!xdr_post_op_attr(xdrs, &at)) {
			return (FALSE);
		}
		if (!at.attributes) {
			if (sizeof (at.attributes) > size) {
				objp->reply.eof = FALSE;
				goto bufovflow;
			}
			bcopy((char *)&at.attributes, tbufp,
				sizeof (at.attributes));
			tbufp += sizeof (at.attributes);
			size -= sizeof (at.attributes);
		} else {
			if (sizeof (at) > size) {
				objp->reply.eof = FALSE;
				goto bufovflow;
			}
			bcopy((char *)&at, tbufp, sizeof (at));
			tbufp += sizeof (at);
			size -= sizeof (at);
		}
		if (tbufp != (char *)roundto32int(tbufp)) {
			char *kbufp = tbufp;
			tbufp = (char *)roundto32int(kbufp);
			size -= tbufp - kbufp;
		}

		if (!xdr_post_op_fh3(xdrs, &fh)) {
			return (FALSE);
		}
		if (!fh.handle_follows) {
			if (sizeof (fh.handle_follows) > size) {
				objp->reply.eof = FALSE;
				goto bufovflow;
			}
			bcopy((char *)&fh.handle_follows, tbufp,
				sizeof (fh.handle_follows));
			tbufp += sizeof (fh.handle_follows);
			size -= sizeof (fh.handle_follows);
		} else {
			if (sizeof (fh) > size) {
				objp->reply.eof = FALSE;
				goto bufovflow;
			}
			bcopy((char *)&fh, tbufp, sizeof (fh));
			tbufp += sizeof (fh);
			size -= sizeof (fh);
		}

		bufp = tbufp;
	}
	if (!xdr_bool(xdrs, &objp->reply.eof)) {
		return (FALSE);
	}
bufovflow:
	objp->size = bufp - (char *)(objp->reply.entries);
	return (TRUE);
}


bool_t
xdr_READDIRPLUS3resok(register XDR *xdrs, READDIRPLUS3resok *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
		return (FALSE);
	if (!xdr_cookieverf3(xdrs, objp->cookieverf))
		return (FALSE);
	if (xdrs->x_op == XDR_DECODE) {
		if (!xdr_getdirpluslist(xdrs, objp))
			return (FALSE);
	} else if (xdrs->x_op == XDR_ENCODE) {
		if (!xdr_putdirpluslist(xdrs, objp))
			return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_READDIRPLUS3resfail(register XDR *xdrs, READDIRPLUS3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_READDIRPLUS3res(register XDR *xdrs, READDIRPLUS3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_READDIRPLUS3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_READDIRPLUS3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_FSSTAT3args(register XDR *xdrs, FSSTAT3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->fsroot))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_FSSTAT3resok(register XDR *xdrs, FSSTAT3resok *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes)) {
		return (FALSE);
	}
	if (!xdr_size3(xdrs, &objp->tbytes)) {
		return (FALSE);
	}
	if (!xdr_size3(xdrs, &objp->fbytes)) {
		return (FALSE);
	}
	if (!xdr_size3(xdrs, &objp->abytes)) {
		return (FALSE);
	}
	if (!xdr_size3(xdrs, &objp->tfiles)) {
		return (FALSE);
	}
	if (!xdr_size3(xdrs, &objp->ffiles)) {
		return (FALSE);
	}
	if (!xdr_size3(xdrs, &objp->afiles)) {
		return (FALSE);
	}
	if (!xdr_uint32(xdrs, &objp->invarsec)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_FSSTAT3resfail(register XDR *xdrs, FSSTAT3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_FSSTAT3res(register XDR *xdrs, FSSTAT3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_FSSTAT3resok(xdrs, &objp->resok)) {
			return (FALSE);
		}
		break;
	default:
		if (!xdr_FSSTAT3resfail(xdrs, &objp->resfail)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}

bool_t
xdr_FSINFO3args(register XDR *xdrs, FSINFO3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->fsroot))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_FSINFO3resok(register XDR *xdrs, FSINFO3resok *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->rtmax))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->rtpref))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->rtmult))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->wtmax))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->wtpref))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->wtmult))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->dtpref))
		return (FALSE);
	if (!xdr_size3(xdrs, &objp->maxfilesize))
		return (FALSE);
	if (!xdr_nfstime3(xdrs, &objp->time_delta))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->properties))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_FSINFO3resfail(register XDR *xdrs, FSINFO3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_FSINFO3res(register XDR *xdrs, FSINFO3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_FSINFO3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_FSINFO3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_PATHCONF3args(register XDR *xdrs, PATHCONF3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_PATHCONF3resok(register XDR *xdrs, PATHCONF3resok *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->link_max))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->name_max))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->no_trunc))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->chown_restricted))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->case_insensitive))
		return (FALSE);
	if (!xdr_bool(xdrs, &objp->case_preserving))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_PATHCONF3resfail(register XDR *xdrs, PATHCONF3resfail *objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_PATHCONF3res(register XDR *xdrs, PATHCONF3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_PATHCONF3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_PATHCONF3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}

bool_t
xdr_COMMIT3args(register XDR *xdrs, COMMIT3args *objp)
{

	if (!xdr_nfs_fh3(xdrs, &objp->file))
		return (FALSE);
	if (!xdr_offset3(xdrs, &objp->offset))
		return (FALSE);
	if (!xdr_count3(xdrs, &objp->count))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_COMMIT3resok(register XDR *xdrs, COMMIT3resok *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->file_wcc))
		return (FALSE);
	if (!xdr_writeverf3(xdrs, objp->verf))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_COMMIT3resfail(register XDR *xdrs, COMMIT3resfail *objp)
{

	if (!xdr_wcc_data(xdrs, &objp->file_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_COMMIT3res(register XDR *xdrs, COMMIT3res *objp)
{

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
	case NFS3_OK:
		if (!xdr_COMMIT3resok(xdrs, &objp->resok))
			return (FALSE);
		break;
	default:
		if (!xdr_COMMIT3resfail(xdrs, &objp->resfail))
			return (FALSE);
		break;
	}
	return (TRUE);
}
