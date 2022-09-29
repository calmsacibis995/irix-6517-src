/*
 * Client side interface to the virtual socket abstraction.
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 */

#ident "$Revision: 1.30 $"

#include <sys/types.h>
#include <limits.h>
#include <sys/fs_subr.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <ksys/vfile.h>
#include <sys/vsocket.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include "./dsvsock.h"
#include "./vsock.h"

#include "invk_dsvsock_stubs.h"
#include "invk_dcvsock_stubs.h"

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dcvsock_stubs.h"

extern  void            idbg_service(service_t *);
extern	void		idbg_obj_handle(obj_handle_t *);

static void dcsock_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
				tk_set_t *);
static void dcsock_tcif_return(tkc_state_t, void *, tk_set_t, tk_set_t,
				tk_disp_t);

static tkc_ifstate_t	dcsock_tclient_iface = {
		dcsock_tcif_obtain,
		dcsock_tcif_return
};

/*
 * This file contains the code for the client-side ops vector
 * for vsockets.
 *
 *		struct vfile
 *		    |
 *		    |
 *		    |
 *		struct vsocket		struct vsocket
 *		    |			       |
 * (dcsock_vector)  |			       |	(dssock_vector)
 *		    |			       |
 *		struct dcvsock <======> struct dsvsock
 *					       |
 *					       |	(direct *so)
 *					       |
 *					struct socket
 *
 * Because this is the client-side code, all of the routines in
 * the ops-vector are filled in, because all the operations go
 * through this vector.    By contrast, on the server side, most
 * of the routines - all but create and close - are never invoked
 * and so are replaced by traps.
 *
 * Each routine on the client-side has the job of getting arguments
 * from the syscall level, packaging them and sending across to the
 * server side, where they are passed down to the real socket level
 * via the struct socket, and then results are passed back which we
 * handle here.
 */


#ifdef DEBUG
int     dcstatistics[VS_MAX];
#endif /* DEBUG */

int
dcsock_trap ()
{
	debug ("dcsock_trap");
	return (0);
}

void
dcsock_initialize()
{
	dcvsock_zone = kmem_zone_init (sizeof (dcvsock_t), "dcvsock");
	if (dcvsock_zone == NULL)
		panic ("dcsock_initialize");
	vsock_statistics.dcvsock_count = 0;

	mesg_handler_register(dcvsock_msg_dispatcher, DCVS_SUBSYSID);
}

void
dcsock_alloc(dcvsock_t **dcop)
{
	dcvsock_t	*dco;

	dco = (dcvsock_t *) kmem_zone_zalloc(dcvsock_zone, KM_SLEEP);
	*dcop = dco;
	vsock_statistics.dcvsock_count++;
}

void
dcsock_free (
	dcvsock_t *dcop)
{
	kmem_zone_free(dcvsock_zone, dcop);
	vsock_statistics.dcvsock_count--;
}

int
dcsock_existing(
	struct vsocket	*vso,
	vsock_handle_t	*handlep
	)
{
	dcvsock_t	*dco;

	dcsock_alloc(&dco);
	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&(dco->dcvs_bhv), dco, vso, &dcsock_vector);
	bhv_insert_initial(VS_BHV_HEAD(vso), &(dco->dcvs_bhv));
	bcopy(handlep, &dco->dcvs_handle, sizeof(vsock_handle_t));
	tkc_create("dcvsock", dco->dcvs_tclient, dco, &dcsock_tclient_iface,
		VSOCK_NTOKENS, VSOCK_EXISTENCE_TOKENSET,
		VSOCK_EXISTENCE_TOKENSET, dco);
	return 0;
}

/* ARGSUSED */
int
dcsock_create(
	struct vsocket	*vso,
	int		dom,
	struct vsocket	**avso,
	int		type,
	int		proto)
{
	dcvsock_t		*dco;
	tk_set_t already, refused;
	tk_set_t granted;
	vsock_handle_t	clhandle;
	service_t svc;
	int	error = 0;
	/* REFERENCED */
	int	msgerr;

	DCSTATISTICS(VS_CREATE);

	svc = dssock_service_id;
	dcsock_alloc(&dco);

	msgerr = invk_dsvsock_so_create(svc, &error, cellid(),
			dom, type, proto,
			VSOCK_EXISTENCE_TOKENSET, TK_NULLSET, TK_NULLSET,
			&already, &granted, &refused,
			&clhandle);
	ASSERT(!msgerr);
	if (!error) {

	    /* Initialize and insert behavior descriptor. */
	    bhv_desc_init(&(dco->dcvs_bhv), dco, vso, &dcsock_vector);
	    bhv_insert_initial(VS_BHV_HEAD(vso), &(dco->dcvs_bhv));

	    bcopy(&clhandle, &dco->dcvs_handle, sizeof(vsock_handle_t));
	    tkc_create("dcvsock", dco->dcvs_tclient, dco, &dcsock_tclient_iface,
		    VSOCK_NTOKENS, VSOCK_EXISTENCE_TOKENSET,
		    VSOCK_EXISTENCE_TOKENSET, dco);
	    vsock_enter_id (&clhandle, vso);
	}

	if (error)
		dcsock_free (dco);

	return error;
}

/* ARGSUSED */
int
dcsock_close(
	bhv_desc_t	*vsi,
	int		lastclose,
	int		flag)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	vsock_t		*vso;
	int		last;
	tk_set_t        retset;
	tk_disp_t       why;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_CLOSE);

	vso = BHV_VOBJ(&(dco->dcvs_bhv));
	last = vsock_hremove (vso, &dco->dcvs_handle);
	if (last) {
		ASSERT(vso->vs_refcnt == 1);
		vso->vs_flags |= VS_CLIENT_CLOSE;
		tkc_release1(dco->dcvs_tclient, VSOCK_EXISTENCE_TOKEN);
		tkc_returning(dco->dcvs_tclient,
			VSOCK_EXISTENCE_TOKENSET, &retset, &why, 1);
		ASSERT(retset & VSOCK_EXISTENCE_TOKENSET);

		msgerr = invk_dsvsock_so_return(DCVS_TO_SERVICE(dco),
			cellid(), DCVS_TO_OBJID(dco),
			retset, TK_NULLSET, why);
		ASSERT(!msgerr);

		tkc_returned(dco->dcvs_tclient, retset, TK_NULLSET);
		tkc_destroy(dco->dcvs_tclient);
		bhv_remove(&(vso->vs_bh), &(dco->dcvs_bhv));
		dcsock_free (dco);
		vsocket_free(vso);
	}

	return 0;
}

/* ARGSUSED */
int
dcsock_send(
	bhv_desc_t	*vsi,
	struct mbuf	*nam,
	register struct uio *uio,
	int		flags,
	struct mbuf	*rights)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	ssize_t		resid;
	short		sigpipe;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_SEND);

	ASSERT(nam == NULL);
	ASSERT(rights == NULL);
	msgerr = invk_dsvsock_so_send(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), uio, uio->uio_iov, uio->uio_iovcnt, 
		flags, &resid, &sigpipe);
	ASSERT(!msgerr);
	uio->uio_resid = resid;
	uio->uio_sigpipe = sigpipe;

	return error;
}

int
dcsock_sendit(
	bhv_desc_t	*vsi,
	struct msghdr	*mp, 
	int 		flags, 
	rval_t 		*rvp)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_SEND);

	msgerr = invk_dsvsock_so_sendit(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), mp, mp->msg_iov, mp->msg_iovlen, 
		flags, rvp);
	ASSERT(!msgerr);

	return error;
}

/* ARGSUSED */
int
dcsock_receive(
	bhv_desc_t	*vsi,
	struct mbuf	**nam,
	struct uio 	*uio,
	int		*flags,
	struct mbuf	**rightsp)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	ssize_t		resid;
	int		lflags;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_RECEIVE);

	ASSERT(nam == NULL);
	ASSERT(rightsp == NULL);
	if (flags == NULL) {
		flags = &lflags;
		*flags = 0;
	}
	msgerr = invk_dsvsock_so_receive(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), uio, uio->uio_iov, uio->uio_iovcnt, 
		flags, &resid);
	ASSERT(!msgerr);

	uio->uio_resid = resid;

	return error;
}

int
dcsock_recvit(
	bhv_desc_t	*vsi,
	register struct msghdr *mp,
	int *flags,
	void *namelenp, 
       	void *rightslenp,
	rval_t *rvp,
	int vers)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_RECEIVE);

	msgerr = invk_dsvsock_so_recvit(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), mp, mp->msg_iov, mp->msg_iovlen, flags, 
		namelenp, rightslenp, rvp, vers);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_accept(
	bhv_desc_t	*vsi,
	caddr_t		name,
	sysarg_t        *anamelen,
	sysarg_t        namelen,
	rval_t		*rvp,
	struct vsocket	**nvso)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		error2;
	vsock_handle_t	vso_handle;
	vfile_t		*fp;
	int		fd;
	struct vsocket	*vso;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_ACCEPT);

	ASSERT(nvso == NULL);
	msgerr = invk_dsvsock_so_accept(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), name, anamelen, namelen, rvp, 
		&vso_handle);
	ASSERT(!msgerr);
	if (error == 0) {
		vsock_import(&vso_handle, &vso);
		ASSERT(vso);
		if (error = vfile_alloc(FREAD|FWRITE|FSOCKET, &fp, &fd)) {
			VSOP_CLOSE(vso, 1, 0, error2);
			return error;
		}
		vfile_ready(fp, vso);
		rvp->r_val1 = fd;
	}

	return error;
}

int
dcsock_listen(
	bhv_desc_t	*vsi,
	int		backlog)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_LISTEN);

	msgerr = invk_dsvsock_so_listen(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), backlog);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_connect(
	bhv_desc_t	*vsi,
	caddr_t		nam,
	sysarg_t    	namlen)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_CONNECT);

	msgerr = invk_dsvsock_so_connect(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), nam, namlen);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_connect2(
	bhv_desc_t	*vsi1,
	bhv_desc_t	*vsi2)
{
	dcvsock_t	*dco1 = BHV_PDATA(vsi1);
	dcvsock_t	*dco2 = BHV_PDATA(vsi2);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_CONNECT2);

	msgerr = invk_dsvsock_so_connect2(DCVS_TO_SERVICE(dco1), &error,
		DCVS_TO_OBJID(dco1), DCVS_TO_OBJID(dco2));
	ASSERT(!msgerr);

	return error;
}

int
dcsock_bind(
	bhv_desc_t	*vsi,
	struct mbuf	*nam)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_BIND);

	ASSERT(nam ? (nam->m_next == NULL) : 1);
	msgerr = invk_dsvsock_so_bind(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), (char *)&nam->m_u, nam->m_len);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_shutdown(
	bhv_desc_t	*vsi,
	int    		how)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_SHUTDOWN);

	msgerr = invk_dsvsock_so_shutdown(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), how);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_getattr(
	bhv_desc_t	*vsi,
	int		level,
	int		name,
	struct mbuf	**m)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	struct mbuf     *m2;
	/* REFERENCED */
	int		msgerr;
	char		*mu;
	size_t		mu_size;

	DCSTATISTICS(VS_GETATTR);

	m2 = m_get(M_WAIT, MT_SOOPTS);
	if (m2 == NULL)
		return ENOBUFS;
	mu = (char *)&m2->m_u;
	mu_size = sizeof(m2->m_u);
	msgerr = invk_dsvsock_so_getattr(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), level, name, &mu, &mu_size);
	ASSERT(!msgerr);
	m2->m_len = mu_size;

	*m = m2;
	return error;
}

int
dcsock_setattr(
	bhv_desc_t	*vsi,
	int		level,
	int		name,
	struct mbuf	*m)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_SETATTR);

	ASSERT(m ? (m->m_next == NULL) : 1);
	msgerr = invk_dsvsock_so_setattr(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), level, name, (char *)&m->m_u, 
		m->m_len);
	ASSERT(!msgerr);
	if (m)
		(void) m_free(m);

	return error;
}

int
dcsock_ioctl(
	bhv_desc_t	*vsi,
	int		cmd,
	void		*arg,
	int		flag,
	struct cred	*cr,
	rval_t		*rvalp)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_IOCTL);

	msgerr = invk_dsvsock_so_ioctl(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), cmd, arg, flag, CRED_GETID(cr), rvalp);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_select(
	bhv_desc_t	*vsi,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead	**phpp,
	unsigned int	*genp)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_SELECT);

	msgerr = invk_dsvsock_so_select(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), events, anyyet, reventsp, phpp, genp);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_getsockname(
	bhv_desc_t	*vsi,
	caddr_t		asa,
	sysarg_t	*alen,
	sysarg_t	len,
	rval_t		*rvp)
{
	dcvsock_t	*dco  = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_GETSOCKNAME);

	msgerr = invk_dsvsock_so_getsockname(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), asa, alen, len, rvp);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_setfl(
	bhv_desc_t	*vsi,
	int		oflags,
	int		nflags,
	struct cred	*cr)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_SETFL);

	msgerr = invk_dsvsock_so_setfl(DCVS_TO_SERVICE(dco), &error,
		DCVS_TO_OBJID(dco), oflags, nflags, CRED_GETID(cr));
	ASSERT(!msgerr);

	return error;
}

int
dcsock_getpeername(
	bhv_desc_t	*vsi,
	caddr_t		asa,
	sysarg_t	*alen,
	rval_t		*rvp)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_GETPEERNAME);

	msgerr = invk_dsvsock_so_getpeername(DCVS_TO_SERVICE(dco), &error,
		    DCVS_TO_OBJID(dco), asa, alen, rvp);
	ASSERT(!msgerr);

	return error;
}

int
dcsock_fstat(
	bhv_desc_t	*vsi,
	struct vattr	*vap,
	int		flags,
	struct cred	*cr)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_FSTAT);

	msgerr = invk_dsvsock_so_fstat(DCVS_TO_SERVICE(dco), &error,
		    DCVS_TO_OBJID(dco), vap, flags, CRED_GETID(cr));
	ASSERT(!msgerr);

	return error;
}

int
dcsock_fcntl(
	bhv_desc_t	*vsi,
	int		cmd,
	void		*arg,
	rval_t		*rvp)
{
	dcvsock_t	*dco = BHV_PDATA(vsi);
	int		error = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTATISTICS(VS_FCNTL);

	msgerr = invk_dsvsock_so_fcntl(DCVS_TO_SERVICE(dco), &error,
		    DCVS_TO_OBJID(dco), cmd, arg, rvp);
	ASSERT(!msgerr);

	return error;
}

struct vsocket_ops dcsock_vector = {
	BHV_IDENTITY_INIT_POSITION(VSOCK_POSITION_DS),
    dcsock_create,		/* create */
    dcsock_close,		/* close */
    dcsock_trap,		/* abort */
    dcsock_send,		/* send */
    dcsock_sendit,		/* sendit */
    dcsock_receive,		/* receive */
    dcsock_recvit,		/* recvit */
    dcsock_accept,		/* accept */
    dcsock_listen,		/* listen */
    dcsock_connect,		/* connect */
    dcsock_connect2,		/* connect2 */
    dcsock_bind,		/* bind */
    dcsock_shutdown,		/* shutdown */
    dcsock_getattr,		/* getattr */
    dcsock_setattr,		/* setattr */
    dcsock_ioctl,		/* ioctl */
    dcsock_select,		/* select */
    dcsock_getsockname,		/* getsockname */
    dcsock_setfl,		/* setfl */
    dcsock_getpeername,		/* getpeername */
    dcsock_fstat,		/* fstat */
    dcsock_fcntl,		/* fcntl */
    fs_pathconf,		/* pathconf */
};

/* ARGSUSED */
static void
dcsock_tcif_obtain(
	void		*obj,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*refused)
{
	debug ("dcsock_tcif_obtain");
}

static void
dcsock_tcif_return(
	tkc_state_t	tclient,
	void		*obj,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	dcvsock_t	*dco = (dcvsock_t *)obj;
	/* REFERENCED */
	int		msgerr;

	ASSERT(tclient == dco->dcvs_tclient);
	ASSERT(TK_IS_IN_SET(VSOCK_EXISTENCE_TOKENSET, to_be_returned));

	msgerr = invk_dsvsock_so_return(DCVS_TO_SERVICE(dco),
			cellid(), DCVS_TO_OBJID(dco),
			to_be_returned, unknown, why);
	ASSERT(!msgerr);
	tkc_returned(tclient, to_be_returned, TK_NULLSET);
}

void
I_dcvsock_recall(int	*error,
	vsock_handle_t	*handle,
	tk_set_t	to_be_revoked,
	tk_disp_t	why)
{
	dcvsock_t	*dco;
	vsock_t         *vso = NULL;

	vsock_lookup_id(handle, &vso);
	ASSERT(vso);
	dco = BHV_PDATA(vso->vs_bh.bh_first);
	tkc_recall(dco->dcvs_tclient, to_be_revoked, why);
	*error = 0;
	return;
}

