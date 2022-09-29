#ident "$Revision: 1.8 $"

#define NFSSERVER
#include "types.h"
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/mbuf.h>
#include <sys/pathname.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/siginfo.h>	/* for CLD_EXITED exit code */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/sema.h>
#include <sys/pcb.h>		/* for setjmp and longjmp */
#include <sys/atomic_ops.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#ifdef DEBUG
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#endif /* DEBUG */
#include <string.h>
#include "auth.h"
#include "auth_unix.h"
#include "svc.h"
#include "xdr.h"
#include "nfs.h"
#include "nfs_impl.h"
#include "clnt.h"
#include "export.h"
#include "xattr.h"

#define rdonly(exi, req) (((exi)->exi_export.ex_flags & EX_RDONLY) || \
			  (((exi)->exi_export.ex_flags & EX_RDMOSTLY) && \
			   !hostintable((struct sockaddr *) \
					svc_getcaller((req)->rq_xprt), \
					&(exi)->exi_writehash)))

#define noxattr(exi, flags) (((exi)->exi_export.ex_flags & EX_NOXATTR) && \
			     !((flags) & ATTR_TRUST))

extern void vattr_to_fattr3 (struct vattr *, struct fattr3 *);
extern struct vnode *fh3tovp (nfs_fh3 *, struct exportinfo *);
extern int checkauth (struct exportinfo *, struct svc_req *, struct cred *,
		      int);

/*ARGSUSED*/
static void
xattr_nullproc (void *vargs, void *vresp, struct exportinfo *exi,
		struct svc_req *req, cred_t *cr)
{
}

/*ARGSUSED*/
static void
getxattr1 (void *vargs, void *vresp, struct exportinfo *exi,
	   struct svc_req *req, cred_t *cr)
{
	GETXATTR1args *args = (GETXATTR1args *) vargs;
	GETXATTR1res *resp = (GETXATTR1res *) vresp;
	int error, length;
	vnode_t *vp;
	struct vattr va;
	char *value;

	/* convert file handle to vnode */
	vp = fh3tovp(&args->object, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	if (noxattr (exi, args->flags)) {
		resp->status = NFS3ERR_NOTSUPP;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	/* check for invalid length argument */
	if (args->length > ATTR_MAX_VALUELEN) {
		resp->status = NFS3ERR_INVAL;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	/* allocate space for attribute buffer */
	length = args->length;
	value = (length != 0 ? kmem_alloc(length, KM_SLEEP) : NULL);
	VOP_ATTR_GET(vp, args->name, value, &length, args->flags, cr, error);
	if (error)
		resp->status = puterrno3(error);
	else
		resp->status = NFS3_OK;

	va.va_mask = AT_ALL;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	if (resp->status != NFS3_OK) {
		if (value != NULL)
			kmem_free ((caddr_t) value, args->length);
		if (error)
			resp->resfail.obj_attributes.attributes = FALSE;
		else {
			resp->resfail.obj_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.obj_attributes.attr);
		}
		return;
	}

	if (error)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}

	resp->resok.size = args->length;
	resp->resok.data.value = value;
	resp->resok.data.len = length;
}

static fhandle_t *
getxattr1_getfh (void *vargs)
{
	GETXATTR1args *args = (GETXATTR1args *) vargs;

	return ((fhandle_t *)&args->object.fh3_u.nfs_fh3_i.fh3_i);
}

static void
getxattr1_free (void *vresp)
{
	GETXATTR1res *resp = (GETXATTR1res *) vresp;

	if (resp->status != NFS3_OK ||
	    resp->resok.data.value == NULL ||
	    resp->resok.size == 0)
		return;

	kmem_free((caddr_t)resp->resok.data.value, resp->resok.size);
}

static void
setxattr1 (void *vargs, void *vresp, struct exportinfo *exi,
	   struct svc_req *req, cred_t *cr)
{
	SETXATTR1args *args = (SETXATTR1args *) vargs;
	SETXATTR1res *resp = (SETXATTR1res *) vresp;
	int bverror, averror, error;
	vnode_t *vp;
	struct vattr bva, ava;

	/* convert file handle to vnode */
	vp = fh3tovp(&args->object, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_wcc.before.attributes = FALSE;
		resp->resfail.obj_wcc.after.attributes = FALSE;
		return;
	}

	if (noxattr (exi, args->flags)) {
		resp->status = NFS3ERR_NOTSUPP;
		resp->resfail.obj_wcc.before.attributes = FALSE;
		resp->resfail.obj_wcc.after.attributes = FALSE;
		return;
	}

	bva.va_mask = AT_ALL;
	VOP_GETATTR(vp, &bva, 0, cr, bverror);

	if (rdonly(exi, req) || (vp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		VN_RELE(vp);
		resp->status = NFS3ERR_ROFS;
		if (bverror) {
			resp->resfail.obj_wcc.before.attributes = FALSE;
			resp->resfail.obj_wcc.after.attributes = FALSE;
		}
		else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.obj_wcc);
		return;
	}

	VOP_ATTR_SET(vp, args->name, args->data.value,
		     args->data.len, args->flags, cr, error);

	ava.va_mask = AT_ALL;
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
				vattr_to_fattr3(&ava, &resp->resfail.obj_wcc.after.attr);
			}
		}
		else {
			if (averror) {
				resp->resfail.obj_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&bva, &resp->resfail.obj_wcc.before.attr);
				resp->resfail.obj_wcc.after.attributes = FALSE;
			}
			else {
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
	}
	else {
		if (averror) {
			resp->resok.obj_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&bva,
					  &resp->resok.obj_wcc.before.attr);
			resp->resok.obj_wcc.after.attributes = FALSE;
		}
		else
			vattr_to_wcc_data(&bva, &ava, &resp->resok.obj_wcc);
	}
}

static fhandle_t *
setxattr1_getfh (void *vargs)
{
	SETXATTR1args *args = (SETXATTR1args *) vargs;

	return ((fhandle_t *)&args->object.fh3_u.nfs_fh3_i.fh3_i);
}

static void
rmxattr1 (void *vargs, void *vresp, struct exportinfo *exi,
	  struct svc_req *req, cred_t *cr)
{
	RMXATTR1args *args = (RMXATTR1args *) vargs;
	RMXATTR1res *resp = (RMXATTR1res *) vresp;
	int bverror, averror, error;
	vnode_t *vp;
	struct vattr bva, ava;

	/* convert file handle to vnode */
	vp = fh3tovp(&args->object, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_wcc.before.attributes = FALSE;
		resp->resfail.obj_wcc.after.attributes = FALSE;
		return;
	}

	if (noxattr (exi, args->flags)) {
		resp->status = NFS3ERR_NOTSUPP;
		resp->resfail.obj_wcc.before.attributes = FALSE;
		resp->resfail.obj_wcc.after.attributes = FALSE;
		return;
	}

	bva.va_mask = AT_ALL;
	VOP_GETATTR(vp, &bva, 0, cr, bverror);

	if (rdonly(exi, req) || (vp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		VN_RELE(vp);
		resp->status = NFS3ERR_ROFS;
		if (bverror) {
			resp->resfail.obj_wcc.before.attributes = FALSE;
			resp->resfail.obj_wcc.after.attributes = FALSE;
		}
		else
			vattr_to_wcc_data(&bva, &bva, &resp->resfail.obj_wcc);
		return;
	}

	VOP_ATTR_REMOVE(vp, args->name, args->flags, cr, error);

	ava.va_mask = AT_ALL;
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
				vattr_to_fattr3(&ava, &resp->resfail.obj_wcc.after.attr);
			}
		}
		else {
			if (averror) {
				resp->resfail.obj_wcc.before.attributes = TRUE;
				vattr_to_wcc_attr(&bva, &resp->resfail.obj_wcc.before.attr);
				resp->resfail.obj_wcc.after.attributes = FALSE;
			}
			else {
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
	}
	else {
		if (averror) {
			resp->resok.obj_wcc.before.attributes = TRUE;
			vattr_to_wcc_attr(&bva,
					  &resp->resok.obj_wcc.before.attr);
			resp->resok.obj_wcc.after.attributes = FALSE;
		}
		else
			vattr_to_wcc_data(&bva, &ava, &resp->resok.obj_wcc);
	}
}

static fhandle_t *
rmxattr1_getfh (void *vargs)
{
	RMXATTR1args *args = (RMXATTR1args *) vargs;

	return ((fhandle_t *)&args->object.fh3_u.nfs_fh3_i.fh3_i);
}

/*ARGSUSED*/
static void
listxattr1 (void *vargs, void *vresp, struct exportinfo *exi,
	    struct svc_req *req, cred_t *cr)
{
	LISTXATTR1args *args = (LISTXATTR1args *) vargs;
	LISTXATTR1res *resp = (LISTXATTR1res *) vresp;
	int error;
	vnode_t *vp;
	struct vattr va;
	attrlist_cursor_kern_t cursor;

	/* convert file handle to vnode */
	vp = fh3tovp(&args->object, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	if (noxattr (exi, args->flags)) {
		resp->status = NFS3ERR_NOTSUPP;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	/* check for invalid length argument */
	if (args->length > ATTR_MAX_VALUELEN) {
		resp->status = NFS3ERR_INVAL;
		resp->resfail.obj_attributes.attributes = FALSE;
		return;
	}

	/* allocate space for response buffer */
	if ((resp->resok.data.len = args->length) != 0)
		resp->resok.data.value = kmem_alloc(args->length, KM_SLEEP);
	else
		resp->resok.data.value = NULL;

	/* initialize cursor */
	cursor = args->cursor;
	VOP_ATTR_LIST(vp, resp->resok.data.value, args->length,
		      args->flags, &cursor, cr, error);
	if (error)
		resp->status = puterrno3(error);
	else {
		resp->status = NFS3_OK;
		resp->resok.cursor = cursor;
	}

	va.va_mask = AT_ALL;
	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	if (resp->status != NFS3_OK) {
		if (resp->resok.data.value != NULL)
			kmem_free ((caddr_t) resp->resok.data.value,
				   args->length);
		if (error)
			resp->resfail.obj_attributes.attributes = FALSE;
		else {
			resp->resfail.obj_attributes.attributes = TRUE;
			vattr_to_fattr3(&va,
					&resp->resfail.obj_attributes.attr);
		}
		return;
	}

	if (error)
		resp->resok.obj_attributes.attributes = FALSE;
	else {
		resp->resok.obj_attributes.attributes = TRUE;
		vattr_to_fattr3(&va, &resp->resok.obj_attributes.attr);
	}
}

static fhandle_t *
listxattr1_getfh (void *vargs)
{
	LISTXATTR1args *args = (LISTXATTR1args *) vargs;

	return ((fhandle_t *)&args->object.fh3_u.nfs_fh3_i.fh3_i);
}

static void
listxattr1_free (void *vresp)
{
	LISTXATTR1res *resp = (LISTXATTR1res *) vresp;

	if (resp->status != NFS3_OK ||
	    resp->resok.data.value == NULL ||
	    resp->resok.data.len == 0)
		return;

	kmem_free((caddr_t) resp->resok.data.value, resp->resok.data.len);
}

/* ARGSUSED */
static bool_t
xattr_void (XDR *X, caddr_t c)
{
	return (TRUE);
}

typedef void (*xattrproc_disp_t) (void *, void *, struct exportinfo *,
				struct svc_req *, cred_t *);
typedef void (*xattrproc_free_t) (void *);
typedef fhandle_t *(*xattrproc_fhandle_t) (void *);

struct xattrdisp {
	xattrproc_disp_t dis_proc;	  /* proc to call */
	xdrproc_ansi_t dis_xdrargs;	  /* xdr routine to get args */
	int	  dis_argsz;		  /* sizeof args */
	xdrproc_ansi_t dis_xdrres;	  /* xdr routine to put results */
	int	  dis_ressz;		  /* size of results */
	xattrproc_free_t dis_resfree;	  /* frees space allocated by proc */
	int       dis_flags;		  /* flags, see below */
	xattrproc_fhandle_t dis_getfh;	  /* return fhandle for the req */
};

#ifdef TRACE
static char *xattrcallnames_v1[] = {
	"XATTR_NULL",
	"XATTR_GETXATTR",
	"XATTR_SETXATTR",
	"XATTR_RMXATTR",
	"XATTR_LISTXATTR",
};
#endif

/*
 * Version 1 of the Extended Attribute Protocol
 */
static struct xattrdisp xattrdisptab_v1[] = {
	/* XATTRPROC1_NULL = 0 */
	{xattr_nullproc, xattr_void, 0, xattr_void, 0, NULL, 0, NULL},

	/* XATTRPROC1_GETXATTR = 1 */
	{getxattr1,
	 xdr_GETXATTR1args, sizeof (GETXATTR1args),
	 xdr_GETXATTR1res, sizeof (GETXATTR1res),
	 getxattr1_free, XATTR_IDEMPOTENT, getxattr1_getfh},

	/* XATTR1_SETXATTR = 2 */
	{setxattr1,
	 xdr_SETXATTR1args, sizeof (SETXATTR1args),
	 xdr_SETXATTR1res, sizeof (SETXATTR1res),
	 NULL, XATTR_IDEMPOTENT, setxattr1_getfh},

	/* XATTR1_RMXATTR = 3 */
	{rmxattr1,
	 xdr_RMXATTR1args, sizeof (RMXATTR1args),
	 xdr_RMXATTR1res, sizeof (RMXATTR1res),
	 NULL, XATTR_IDEMPOTENT, rmxattr1_getfh},

	/* XATTR1_LISTXATTR = 4 */
	{listxattr1,
	 xdr_LISTXATTR1args, sizeof (LISTXATTR1args),
	 xdr_LISTXATTR1res, sizeof (LISTXATTR1res),
	 listxattr1_free, XATTR_IDEMPOTENT, listxattr1_getfh},
};

static struct xattr_disptable {
	int dis_nprocs;
#ifdef TRACE
	char **dis_procnames;
#endif
	struct xattrdisp *dis_table;
} xattr_disptable[] = {
	{sizeof (xattrdisptab_v1) / sizeof (xattrdisptab_v1[0]),
#ifdef TRACE
	 xattrcallnames_v1,
#endif
	 xattrdisptab_v1},
};

#define DISPTABLE_MAX (sizeof (xattr_disptable) / sizeof (xattr_disptable[0]))

int
xattr_dispatch (struct svc_req *req, XDR *xdrin, caddr_t args, caddr_t res)
{
	struct xattrdisp *disp;
	int error = 0;
	struct exportinfo *exi = NULL;
	int vers = req->rq_vers;
	int which = req->rq_proc;

	/* only accept calls for XATTR_PROGRAM */
	if (req->rq_prog != XATTR_PROGRAM) {
		printf("XATTR server: bad prog number (%d)\n", req->rq_prog);
		error = NFS_RPC_PROG;
		disp = &xattr_disptable[0].dis_table[0];
		goto done;
	}

	/* make sure the version is in the table */
	if (vers < 0 || vers >= DISPTABLE_MAX) {
		error = NFS_RPC_VERS;
		disp = &xattr_disptable[0].dis_table[0];
		goto done;
	}

	/* make sure the procedure is contained in the table */
	if (which < 0 || which >= xattr_disptable[vers].dis_nprocs) {
		printf("XATTR server: bad proc number (%d)\n", which);
		error = NFS_RPC_PROC;
		disp = &xattr_disptable[vers].dis_table[0];
		goto done;
	}

	/* select dispatch table */
	disp = &xattr_disptable[vers].dis_table[which];

	/* initialize argument and result buffers */
	bzero (args, disp->dis_argsz);
	bzero (res, disp->dis_ressz);

	/* decode arguments, calling the xdr proc directly */
	if (!(*disp->dis_xdrargs) (xdrin, args)) {
		printf("XATTR server: bad args for proc %d\n", which);
		error = NFS_RPC_DECODE;
		goto done;
	}

	/*
	 * Find export information and check authentication,
	 * setting the credential if everything is ok.
	 */
	if (which != XATTRPROC1_NULL) {
		/*
	 	 * XXX: this isn't really quite correct. Instead of doing
	  	 * this blind cast, we should extract out the fhandle for
		 * each NFS call. What's more, some procedures (like rename)
	 	 * have more than one fhandle passed in, and we should check
		 * that the two fhandles point to the same exported path.
		 */
		fhandle_t *fh;
		
		if (disp->dis_getfh)
			fh = (*disp->dis_getfh) (args);
		else
			fh = (fhandle_t *) args;

		exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (exi != NULL && !checkauth(exi, req, get_current_cred(), 0)) {
			error = NFS_RPC_AUTH;
			(void) printf("XATTR server: weak authentication, source IP address=%s\n", inet_ntoa(req->rq_xprt->xp_raddr.sin_addr));
			goto done;
		}
	}

	(*disp->dis_proc) (args, res, exi, req, get_current_cred ());
done:
	if (exi)
		EXPORTED_MRUNLOCK();
	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
	       NETEVENT_NFSUP, NETCNT_NULL, which + NETRES_RFSNULL);

	/* serialize and send results */
	if (error >= 0) {
		error = nfs_sendreply (req->rq_xprt, disp->dis_xdrres, res,
				       error, XATTR1_VERSION, XATTR1_VERSION);
		if (error) {
			printf("XATTR server: reply error %d\n", error);
		}
	}

	/* free arguments struct */
	xdrin->x_op = XDR_FREE;
	if (disp->dis_xdrargs)
		(*disp->dis_xdrargs) (xdrin, args);

	/* free results struct */
	if (disp->dis_resfree)
		(*disp->dis_resfree) (res);

	return (0);
}
