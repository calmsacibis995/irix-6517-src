/*
 * Server side interface to the virtual socket abstraction.
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 */

#ident "$Revision: 1.21 $"

#include <sys/types.h>
#include <limits.h>
#include <sys/kmem.h>
#include <sys/fs_subr.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <ksys/vproc.h>
#include <sys/vnode.h>
#include <sys/vsocket.h>
#include <ksys/cell/handle.h>
#include "./dsvsock.h"
#include "./vsock.h"

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dsvsock_stubs.h"

extern struct vsocket_ops dssock_vector;

/*
 * This file contains the code for the server-side ops vector
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
 * Because this code doesn't actually do anything below the
 * dsvsock level - that work is carried out by the real socket
 * code, it only needs create, and close routines.   All the
 * other routines are trapped because they ought never to be
 * called.
 */

#ifdef DEBUG
int     dsstatistics[VS_MAX];
#endif /* DEBUG */


int
dssock_trap()
{
	debug ("dssock_trap");
	return (0);
}

void
dssock_initialize()
{
	dsvsock_zone = kmem_zone_init (sizeof (dsvsock_t), "dsvsock");
	vsock_statistics.dsvsock_count = 0;

	mesg_handler_register(dsvsock_msg_dispatcher, DSVS_SUBSYSID);
}

void
dssock_alloc (dsvsock_t **dsop)
{
	dsvsock_t	*dso;

	dso = (dsvsock_t *) kmem_zone_zalloc(dsvsock_zone, KM_SLEEP);
	*dsop = dso;
	vsock_statistics.dsvsock_count++;
	return;
}

/* ARGSUSED */
int
dssock_create(
	struct vsocket *vso,
	int             dom,
	struct vsocket  **avso,
	int             type,
	int             proto)
{
	struct socket *so = NULL;
	int error = 0;

	DSSTATISTICS(VS_CREATE);
	error = socreate(dom, &so, type, proto);
	if (!error) {
		bhv_desc_init(&(so->so_bhv), so, vso, &dssock_vector);
		bhv_insert_initial(VS_BHV_HEAD(vso), &(so->so_bhv));
	}
	return (error);
}

void
dssock_destroy(bhv_desc_t *bdp)
{
	dsvsock_t *dso;

	dso = BHV_PDATA(bdp);
	tkc_release1(dso->dsvsock_tclient, VSOCK_EXISTENCE_TOKEN);
	tkc_recall(dso->dsvsock_tclient, VSOCK_EXISTENCE_TOKENSET,
		TK_DISP_CLIENT_ALL);
	tks_recall(dso->dsvsock_tserver, VSOCK_EXISTENCE_TOKENSET, NULL);
}

extern void dsvsock_dispose(void *);

/* ARGSUSED */
int
dssock_close(
	bhv_desc_t	*bdp,
	int		lastclose,
	int             fflag)
{
	bhv_desc_t	*nbdp;
	struct socket 	*so;
	int 		error = 0;
	dsvsock_t	*dso = BHV_PDATA(bdp);
	vsock_t		*vso;
	int		last;

	DSSTATISTICS(VS_CLOSE);
	vso = BHV_VOBJ(&(dso->dsvs_bhv));
	last = vsock_hremove (vso, &dso->dsvs_handle);
	if (last) {
		ASSERT(vso->vs_refcnt == 1);
		nbdp = BHV_NEXT(bdp);
		so = BHV_PDATA(nbdp);
		error = soclose (so);
		dsvsock_dispose ((void *)dso);
	}
	return (error);
}

int
dssock_vs_send (
	bhv_desc_t *bdp,
	struct mbuf *nam,
	register struct uio *uio,
	int flags,
	struct mbuf *rights)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_SEND);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_send);
	PVSOP_SEND(nbdp, nam, uio, flags, rights, error);
	return (error);
}

int
dssock_vs_sendit (
	bhv_desc_t *bdp,
	struct msghdr	*mp, 
	int 		flags, 
	rval_t 		*rvp)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_SEND);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_send);
	PVSOP_SENDIT(nbdp, mp, flags, rvp, error);
	return (error);
}

int
dssock_vs_receive (
	bhv_desc_t *bdp,
	struct mbuf **nam,
	register struct uio *uio,
	int *flags,
	struct mbuf **rightsp)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_RECEIVE);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_receive);
	PVSOP_RECEIVE(nbdp, nam, uio, flags, rightsp, error);
	return (error);
}

int
dssock_vs_recvit (
	bhv_desc_t *bdp,
	register struct msghdr *mp,
	int *flags,
	void *namelenp, 
       	void *rightslenp,
	rval_t *rvp,
	int vers)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_RECEIVE);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_receive);
	PVSOP_RECVIT(nbdp, mp, flags, namelenp, rightslenp,
		rvp, vers, error);
	return (error);
}

int
dssock_vs_listen(
	bhv_desc_t *bdp,
	int backlog)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_LISTEN);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_listen);
	PVSOP_LISTEN(nbdp, backlog, error);
	return (error);
}

int
dssock_vs_connect(
	bhv_desc_t *bdp,
	caddr_t nam,
	sysarg_t     namlen)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_CONNECT);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_connect);
	PVSOP_CONNECT(nbdp, nam, namlen, error);
	return (error);
}

/*ARGSUSED*/
int
dssock_vs_connect2(
	bhv_desc_t *bdp1,
	bhv_desc_t *bdp2)
{
	debug ("dssock_vs_connect2");
	return (0);
}

int
dssock_vs_bind(
	bhv_desc_t *bdp,
	struct mbuf *nam)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_BIND);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_bind);
	PVSOP_BIND(nbdp, nam, error);
	return (error);
}

int
dssock_vs_shutdown(
	bhv_desc_t *bdp,
	int how)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_SHUTDOWN);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_shutdown);
	PVSOP_SHUTDOWN(nbdp, how, error);
	return (error);
}

int
dssock_vs_getattr(
	bhv_desc_t *bdp,
	int level,
	int name,
	struct mbuf **m)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_GETATTR);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_getattr);
	PVSOP_GETATTR(nbdp, level, name, m, error);
	return (error);
}

int
dssock_vs_setattr(
	bhv_desc_t *bdp,
	int level,
	int name,
	struct mbuf *m)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_SETATTR);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_setattr);
	PVSOP_SETATTR(nbdp, level, name, m, error);
	return (error);
}

dssock_vs_accept(
	bhv_desc_t *bdp,
	caddr_t name,
	sysarg_t        *anamelen,
	sysarg_t        namelen,
	rval_t *rvp,
	struct vsocket	**nvso)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_ACCEPT);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_accept);
	PVSOP_ACCEPT(nbdp, name, anamelen, namelen, rvp, nvso, error);
	return (error);
}

dssock_vs_ioctl(
	bhv_desc_t *bdp,
	int cmd,
	void *arg,
	int flag,
	struct cred *cr,
	rval_t *rvalp)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_IOCTL);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_ioctl);
	PVSOP_IOCTL(nbdp, cmd, arg, flag, cr, rvalp, error);
	return (error);
}


dssock_vs_select(
	bhv_desc_t *bdp,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_SELECT);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_select);
	PVSOP_SELECT(nbdp, events, anyyet, reventsp, phpp, genp, error);
	return (error);
}

/* ARGSUSED */
int
dssock_vs_setfl(
	bhv_desc_t *bdp,
	int oflags,
	int nflags,
	struct cred *cr)
{
	debug ("dssock_vs_setfl");
	return (0);
}

dssock_vs_getpeername(
	bhv_desc_t *bdp,
	caddr_t asa,
	sysarg_t *alen,
	rval_t *rvp)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_GETPEERNAME);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_getpeername);
	PVSOP_GETPEERNAME(nbdp, asa, alen, rvp, error);
	return (error);
}

dssock_vs_getsockname(
	bhv_desc_t *bdp,
	caddr_t asa,
	sysarg_t *alen,
	sysarg_t len,
	rval_t *rvp)
{
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_GETSOCKNAME);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_getsockname);
	PVSOP_GETSOCKNAME(nbdp, asa, alen, len, rvp, error);
	return (error);
}

dssock_vs_fcntl(
	bhv_desc_t *bdp,
	int cmd,
        void *arg,
        rval_t *rvp)
{
	bhv_desc_t      *nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_FCNTL);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_fcntl);
	PVSOP_FCNTL(nbdp, cmd, arg, rvp, error);
	return (error);
}

dssock_vs_fstat(
	bhv_desc_t *bdp,
	struct vattr *vap,
	int flags,
	struct cred *cr
	)
{
	bhv_desc_t      *nbdp;
	/* REFERENCED */
	dsvsock_t *pso;
	int error = 0;

	DSSTATISTICS(VS_FSTAT);
	pso = BHV_PDATA(bdp);
	VS_NEXT(bdp, nbdp, vs_fstat);
	PVSOP_FSTAT(nbdp, vap, flags, cr, error);
	return (error);
}

struct vsocket_ops dssock_vector = {
	BHV_IDENTITY_INIT_POSITION(VSOCK_POSITION_DS),
    dssock_create,		/* create */
    dssock_close,		/* close */
    dssock_trap,		/* abort */
    dssock_vs_send,		/* send */
    dssock_vs_sendit,		/* sendit */
    dssock_vs_receive,		/* receive */
    dssock_vs_recvit,		/* recvit */
    dssock_vs_accept,		/* accept */
    dssock_vs_listen,		/* listen */
    dssock_vs_connect,		/* connect */
    dssock_vs_connect2,		/* connect2 */
    dssock_vs_bind,		/* bind */
    dssock_vs_shutdown,		/* shutdown */
    dssock_vs_getattr,		/* getattr */
    dssock_vs_setattr,		/* setattr */
    dssock_vs_ioctl,		/* ioctl */
    dssock_vs_select,		/* select */
    dssock_vs_getsockname,	/* getsockname */
    dssock_vs_setfl,		/* setfl */
    dssock_vs_getpeername,	/* getpeername */
    dssock_vs_fstat,		/* fstat */
    dssock_vs_fcntl,		/* fcntl */
    fs_pathconf,		/* pathconf */
};
