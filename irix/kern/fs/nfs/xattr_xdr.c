#ident "$Revision: 1.3 $"

#include "types.h"
#include <sys/buf.h>            /* for B_ASYNC */
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/proc.h>           /* for u.u_cred only */
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <sys/dirent.h>
#include "auth_unix.h"          /* for NGRPS in nfs_init */
#include "xdr.h"
#define SVCFH
#include "nfs.h"
#include "export.h"
#include "nfs_clnt.h"
#include "nfs3_clnt.h"
#include "rnode.h"
#include "string.h"
#include "auth.h"
#include "svc.h"
#include "xattr.h"

extern bool_t xdr_nfs_fh3 (XDR *, nfs_fh3 *);
extern bool_t xdr_post_op_attr (XDR *, post_op_attr *);
extern bool_t xdr_wcc_data (XDR *, wcc_data *);

static bool_t
xdr_xattrname1(register XDR *xdrs, xattrname1 *objp)
{
	if (!xdr_string(xdrs, objp, ~0))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_GETXATTR1args(register XDR *xdrs, caddr_t args)
{
	GETXATTR1args *objp = (GETXATTR1args *) args;

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	if (!xdr_xattrname1(xdrs, &objp->name))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->length))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdr_GETXATTR1resok(register XDR *xdrs, GETXATTR1resok *objp)
{
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_bytes(xdrs, &objp->data.value, &objp->data.len,
		       objp->data.len))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdr_GETXATTR1resfail(register XDR *xdrs, GETXATTR1resfail *objp)
{
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_GETXATTR1res(register XDR *xdrs, caddr_t args)
{
	GETXATTR1res *objp = (GETXATTR1res *) args;

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
		case NFS3_OK:
			if (!xdr_GETXATTR1resok(xdrs, &objp->resok))
				return (FALSE);
			break;
		default:
			if (!xdr_GETXATTR1resfail(xdrs, &objp->resfail))
				return (FALSE);
			break;
	}
	return (TRUE);
}

bool_t
xdr_SETXATTR1args(register XDR *xdrs, caddr_t args)
{
	SETXATTR1args *objp = (SETXATTR1args *) args;

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	if (!xdr_xattrname1(xdrs, &objp->name))
		return (FALSE);
	if (!xdr_bytes(xdrs, &objp->data.value, &objp->data.len,
		       ATTR_MAX_VALUELEN))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdr_SETXATTR1resok(register XDR *xdrs, SETXATTR1resok *objp)
{
	if (!xdr_wcc_data(xdrs, &objp->obj_wcc))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdr_SETXATTR1resfail(register XDR *xdrs, SETXATTR1resfail *objp)
{
	if (!xdr_wcc_data(xdrs, &objp->obj_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_SETXATTR1res(register XDR *xdrs, caddr_t args)
{
	SETXATTR1res *objp = (SETXATTR1res *) args;

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
		case NFS3_OK:
			if (!xdr_SETXATTR1resok(xdrs, &objp->resok))
				return (FALSE);
			break;
		default:
			if (!xdr_SETXATTR1resfail(xdrs, &objp->resfail))
				return (FALSE);
			break;
	}
	return (TRUE);
}

bool_t
xdr_RMXATTR1args(register XDR *xdrs, caddr_t args)
{
	RMXATTR1args *objp = (RMXATTR1args *) args;

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	if (!xdr_xattrname1(xdrs, &objp->name))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdr_RMXATTR1resok(register XDR *xdrs, RMXATTR1resok *objp)
{
	if (!xdr_wcc_data(xdrs, &objp->obj_wcc))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdr_RMXATTR1resfail(register XDR *xdrs, RMXATTR1resfail *objp)
{
	if (!xdr_wcc_data(xdrs, &objp->obj_wcc))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_RMXATTR1res(register XDR *xdrs, caddr_t args)
{
	RMXATTR1res *objp = (RMXATTR1res *) args;

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
		case NFS3_OK:
			if (!xdr_RMXATTR1resok(xdrs, &objp->resok))
				return (FALSE);
			break;
		default:
			if (!xdr_RMXATTR1resfail(xdrs, &objp->resfail))
				return (FALSE);
			break;
	}
	return (TRUE);
}

bool_t
xdr_LISTXATTR1args(register XDR *xdrs, caddr_t args)
{
	LISTXATTR1args *objp = (LISTXATTR1args *) args;

	if (!xdr_nfs_fh3(xdrs, &objp->object))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->length))
		return (FALSE);
	if (!xdr_uint32(xdrs, &objp->flags))
		return (FALSE);
	if (!xdr_opaque(xdrs, &objp->cursor, sizeof (objp->cursor)))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdr_LISTXATTR1resok(register XDR *xdrs, LISTXATTR1resok *objp)
{
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	if (!xdr_bytes(xdrs, &objp->data.value, &objp->data.len,
		       objp->data.len))
		return (FALSE);
	if (!xdr_opaque(xdrs, &objp->cursor, sizeof (objp->cursor)))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdr_LISTXATTR1resfail(register XDR *xdrs, LISTXATTR1resfail *objp)
{
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_LISTXATTR1res(register XDR *xdrs, caddr_t args)
{
	LISTXATTR1res *objp = (LISTXATTR1res *) args;

	if (!xdr_nfsstat3(xdrs, &objp->status))
		return (FALSE);
	switch (objp->status) {
		case NFS3_OK:
			if (!xdr_LISTXATTR1resok(xdrs, &objp->resok))
				return (FALSE);
			break;
		default:
			if (!xdr_LISTXATTR1resfail(xdrs, &objp->resfail))
				return (FALSE);
			break;
	}
	return (TRUE);
}
