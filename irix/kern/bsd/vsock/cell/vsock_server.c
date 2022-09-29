/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident	"$Revision: 1.24 $"

/*
 * Server side for vsockets
 */

#include <sys/types.h>
#include <limits.h>
#include <sys/fs_subr.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/vsocket.h>
#include <ksys/cell/handle.h>
#include "./dsvsock.h"
#include "./vsock.h"

#include "invk_dsvsock_stubs.h"
#include "invk_dcvsock_stubs.h"

#ifdef DEBUG
int dss_statistics[VS_MAX];
#endif /* DEBUG */

static void dsvock_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
static void dsvock_tsif_recalled(void *, tk_set_t, tk_set_t);

static	tks_ifstate_t	dsvsock_tserver_iface = {
		dsvock_tsif_recall,
		dsvock_tsif_recalled,
		NULL,
};

vsock_interpose(vsock_t *vso, long id)
{
	dsvsock_t	*pso;
	tk_set_t        granted;
	tk_set_t        already_obtained;
	tk_set_t        refused;
	tk_set_t        wanted;
	int		error = 0;
	bhv_desc_t	*bhp;

	dssock_alloc(&pso);
	vsock_statistics.dsvsock_count++;

	tks_create("dsvsock", pso->dsvsock_tserver, pso, &dsvsock_tserver_iface,
		VSOCK_NTOKENS, (void *)id);

	wanted = VSOCK_EXISTENCE_TOKENSET;
	tks_obtain(pso->dsvsock_tserver, (tks_ch_t)cellid(),
		wanted, &granted, &refused,
		&already_obtained);
	ASSERT(granted == wanted);
	ASSERT(already_obtained == TK_NULLSET);
	ASSERT(refused == TK_NULLSET);
	tkc_create_local("dsvsock", pso->dsvsock_tclient, pso->dsvsock_tserver,
		VSOCK_NTOKENS, granted, granted, pso);

	bhv_desc_init(&(pso->dsvs_bhv), pso, vso, &dssock_vector);
	pso->dsvs_id = id;
	error = bhv_insert(VS_BHV_HEAD(vso), &pso->dsvs_bhv);
	bhp = VSOCK_TO_FIRST_BHV(vso);
	VSOCK_HANDLE_MAKE(pso->dsvs_handle, (objid_t)bhp, 
		(vsock_gen_t)id);
	return error;
}

void
vsock_handle(
	vsock_t		*vso,
	vsock_handle_t	*handle)
{
	vsock_gen_t	id;
	bhv_desc_t	*bhp;
	dsvsock_t	*dsv;

	bhp = VSOCK_TO_FIRST_BHV(vso);
	if (BHV_OPS(bhp) == &lvector) {
		/*
		 * This is still a local vsocket, so interpose
		 * a server-side.
		 */
		BHV_READ_UNLOCK(VS_BHV_HEAD(vso));
		BHV_WRITE_LOCK(VS_BHV_HEAD(vso));
		bhp = VSOCK_TO_FIRST_BHV(vso);
		if (BHV_OPS(bhp) == &lvector) {
			id = vsock_newid ();
			vsock_interpose(vso, id);
			bhp = VSOCK_TO_FIRST_BHV(vso);
			dsv = BHV_TO_DSVSOCK(bhp);
			vsock_enter_id (&dsv->dsvs_handle, vso);
		}
		BHV_WRITE_TO_READ(VS_BHV_HEAD(vso));
		bhp = VSOCK_TO_FIRST_BHV(vso);
	}
	dsv = BHV_TO_DSVSOCK(bhp);
	bcopy (&dsv->dsvs_handle, handle, sizeof(*handle));
}

void
I_dsvsock_so_create(
	int		*error,
	cell_t		sender,
	int		domain,
	int		type,
	int		proto,
	tk_set_t        to_be_obtained,
	tk_set_t        to_be_returned,
	tk_disp_t       dofret,
	tk_set_t        *already_obtained,
	tk_set_t        *granted,
	tk_set_t        *refused,
	vsock_handle_t	*handle)
{
	vsock_t		*vso = NULL;
	bhv_desc_t	*bhp = NULL;
	dsvsock_t	*dsv;

	DSS_STATISTICS(VS_CREATE);
	ASSERT(to_be_returned == TK_NULLSET);
	*error = 0;
	ASSERT (TK_IS_IN_SET(VSOCK_EXISTENCE_TOKENSET, to_be_obtained));

	*error = vsocreate(domain, &vso, type, proto);
	if (*error) {
		vso = NULL;
		return;
	}
	ASSERT (vso);
	BHV_READ_LOCK(VS_BHV_HEAD(vso));
	vsock_handle(vso, handle);
	
	bhp = VSOCK_TO_FIRST_BHV(vso);
	dsv = BHV_TO_DSVSOCK(bhp);

	if (to_be_returned != TK_NULLSET)
		tks_return(dsv->dsvsock_tserver, sender, to_be_returned,
					   TK_NULLSET, TK_NULLSET, dofret);

	tks_obtain(dsv->dsvsock_tserver, sender, to_be_obtained, granted,
			   refused, already_obtained);
	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

	return;
}

/* ARGSUSED */
void
I_dsvsock_so_close(
	objid_t vsockid,
	int lastclose,
	int fflag)
{
	vsock_t		*vso = NULL;
	/* REFERENCED */
	int		error;
	bhv_desc_t	*bhp;

	DSS_STATISTICS(VS_CLOSE);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);
	vso->vs_flags |= VS_SERVER_CLOSE;
	VSOP_CLOSE(vso, lastclose, 0, error);
}

/*ARGSUSED*/
void
I_dsvsock_so_send(
	int *error,
	objid_t vsockid,
	struct uio *uio,
	iovec_t	*iovp,
	size_t	iov_len,
	int flags,
	ssize_t	*resid,
	short	*sigpipe)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_SEND);

	uio->uio_iov = iovp;
	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_send);
	PVSOP_SEND(nbdp, NULL, uio, flags, NULL, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));
	*resid = uio->uio_resid;
	*sigpipe = uio->uio_sigpipe;

}

/*ARGSUSED*/
void
I_dsvsock_so_sendit(
	int *error,
	objid_t vsockid,
	struct msghdr	*mp, 
	struct iovec	*iovi,
	size_t		iovi_len,
	int 		flags, 
	rval_t 		*rvp)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_SEND);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);
	mp->msg_iov = iovi;

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_send);
	PVSOP_SENDIT(nbdp, mp, flags, rvp, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

/*ARGSUSED*/
void
I_dsvsock_so_receive(
	int *error,
	objid_t vsockid,
	struct uio *uio,
	iovec_t	*iovp,
	size_t	iov_len,
	int *flags,
	ssize_t	*resid)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_RECEIVE);

	uio->uio_iov = iovp;
	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_receive);
	PVSOP_RECEIVE(nbdp, NULL, uio, flags, NULL, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));
	*resid = uio->uio_resid;

}

/*ARGSUSED*/
void
I_dsvsock_so_recvit(
	int *error,
	objid_t vsockid,
	register struct msghdr *mp,
	struct iovec *iovi,
	size_t		iovi_len,
	int *flags,
	void *namelenp, 
       	void *rightslenp,
	rval_t *rvp,
	int vers)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_RECEIVE);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);
	mp->msg_iov = iovi;

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_receive);
	PVSOP_RECVIT(nbdp, mp, flags, namelenp, rightslenp,
		rvp, vers, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

void
I_dsvsock_so_accept(
	int 		*error,
	objid_t 	vsockid,
	caddr_t 	name,
	sysarg_t        *anamelen,
	sysarg_t        namelen,
	rval_t 		*rvp,
	vsock_handle_t	*handle)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL, *nvso = NULL;

	DSS_STATISTICS(VS_ACCEPT);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_accept);
	PVSOP_ACCEPT(nbdp, name, anamelen, namelen, rvp, &nvso, *error);
	if (*error == 0) {
		ASSERT(nvso);
		vsock_export (nvso, handle);
	}

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

void
I_dsvsock_so_listen(
	int *error,
	objid_t vsockid,
	int backlog)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_LISTEN);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_listen);
	PVSOP_LISTEN(nbdp, backlog, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

void
I_dsvsock_so_connect(
	int *error,
	objid_t vsockid,
	caddr_t nam,
	sysarg_t     namlen)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_CONNECT);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_connect);
	PVSOP_CONNECT(nbdp, nam, namlen, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

void
I_dsvsock_so_connect2(
	int *error,
	objid_t vsockid1,
	objid_t vsockid2)
{
	bhv_desc_t	*bhp1, *nbdp1;
	bhv_desc_t	*bhp2, *nbdp2;
	vsock_t		*vso1 = NULL;
	/* REFERENCED */
	vsock_t		*vso2 = NULL;

	DSS_STATISTICS(VS_CONNECT2);

	bhp1 = OBJID_TO_BHV(vsockid1);
	vso1 = BHV_TO_VSOCK(bhp1);
	bhp2 = OBJID_TO_BHV(vsockid2);
	vso2 = BHV_TO_VSOCK(bhp2);

	BHV_READ_LOCK(VS_BHV_HEAD(vso1));

	VS_NEXT(bhp1, nbdp1, vs_connect);
	VS_NEXT(bhp2, nbdp2, vs_connect);
	PVSOP_CONNECT2(nbdp1, nbdp2, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso1));

}

void
I_dsvsock_so_shutdown(
	int *error,
	objid_t vsockid,
	int how)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_SHUTDOWN);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_shutdown);
	PVSOP_SHUTDOWN(nbdp, how, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

void
I_dsvsock_so_bind(
	int *error,
	objid_t vsockid,
	char *m,
	size_t m_len)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;
	struct mbuf     *nam;

	DSS_STATISTICS(VS_BIND);

	nam = m_get(M_WAIT, MT_SONAME);
	if (nam == NULL) {
		*error = ENOBUFS;
	}
	nam->m_len = m_len;
	ASSERT(m_len <= sizeof(nam->m_u));
	bcopy(m, &nam->m_u, m_len);
	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_bind);
	PVSOP_BIND(nbdp, nam, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));
	m_freem(nam);

}

void
I_dsvsock_so_getattr(
	int *error,
	objid_t vsockid,
	int level,
	int name,
	char	**m,
	size_t	*m_len,
	void 	**bdesc)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;
	struct mbuf	*m2 = NULL;

	DSS_STATISTICS(VS_GETATTR);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_getattr);
	PVSOP_GETATTR(nbdp, level, name, &m2, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

	if (m2) {
		*m = (char *)&m2->m_u;
		*m_len = m2->m_len;
	} else {
		*m = NULL;
		*m_len = 0;
	}
	*bdesc = m2;

}

/* ARGSUSED */
void
I_dsvsock_so_getattr_done(
	char	*m,
	size_t	m_len,
	void 	*bdesc)
{
	(void) m_free((struct mbuf *)bdesc);
}

void
I_dsvsock_so_setattr(
	int *error,
	objid_t vsockid,
	int level,
	int name,
	char *m,
	size_t m_len)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;
	struct mbuf 	*m2;

	DSS_STATISTICS(VS_SETATTR);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);
	/*
	 * Need to make a real local mbuf
	 */
	if (m) {
		m2 = m_get(M_WAIT, MT_SOOPTS);
		ASSERT(m_len <= sizeof(m2->m_u));
		m2->m_len = m_len;
		bcopy(m, &m2->m_u, m_len);
	} else {
		m2 = NULL;
	}

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_setattr);
	PVSOP_SETATTR(nbdp, level, name, m2, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

void
I_dsvsock_so_ioctl(
	int *error,
	objid_t vsockid,
	int cmd,
	void *arg,
	int flag,
	credid_t credid,
	rval_t *rvalp)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;
	struct cred *cr = CREDID_GETCRED(credid);

	DSS_STATISTICS(VS_IOCTL);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_ioctl);
	PVSOP_IOCTL(nbdp, cmd, arg, flag, cr, rvalp, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

	if (cr)
		crfree(cr);

}

void
I_dsvsock_so_select(
	int *error,
	objid_t vsockid,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_SELECT);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

retry:
	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_select);
	PVSOP_SELECT(nbdp, events, anyyet, reventsp, phpp, genp, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

	/*
	 * In the distributed case, add this event to the distributed
	 * poll list on the server.  Return a NULL php to the client.
	 */
	if (!anyyet && (*reventsp == 0) && *phpp) {
		if (distributed_polladd(*phpp, events, *genp) != 0)
			goto retry;
	}
	*phpp = NULL;

}

void
I_dsvsock_so_getpeername(
	int *error,
	objid_t vsockid,
	caddr_t asa,
	sysarg_t *alen,
	rval_t *rvp)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_GETPEERNAME);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_getpeername);
	PVSOP_GETPEERNAME(nbdp, asa, alen, rvp, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}
void
I_dsvsock_so_getsockname(
	int *error,
	objid_t vsockid,
	caddr_t asa,
	sysarg_t *alen,
	sysarg_t len,
	rval_t *rvp)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_GETSOCKNAME);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_getsockname);
	PVSOP_GETSOCKNAME(nbdp, asa, alen, len, rvp, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

void
I_dsvsock_so_setfl(
	int *error,
	objid_t vsockid,
	int oflags,
	int nflags,
	credid_t credid)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;
	struct cred	*cr = CREDID_GETCRED(credid);

	DSS_STATISTICS(VS_SETFL);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_setfl);
	PVSOP_SETFL(nbdp, oflags, nflags, cr, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

	if (cr)
		crfree(cr);
}

void
I_dsvsock_so_fcntl(
	int *error,
	objid_t vsockid,
	int cmd,
	void *arg,
	rval_t *rvp)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;

	DSS_STATISTICS(VS_FCNTL);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_fcntl);
	PVSOP_FCNTL(nbdp, cmd, arg, rvp, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

}

void
I_dsvsock_so_fstat(
	int *error,
	objid_t vsockid,
	struct vattr *vap,
	int flags,
	credid_t credid)
{
	bhv_desc_t	*bhp, *nbdp;
	vsock_t		*vso = NULL;
	struct cred	*cr = CREDID_GETCRED(credid);

	DSS_STATISTICS(VS_FSTAT);

	bhp = OBJID_TO_BHV(vsockid);
	vso = BHV_TO_VSOCK(bhp);

	BHV_READ_LOCK(VS_BHV_HEAD(vso));

	VS_NEXT(bhp, nbdp, vs_fstat);
	PVSOP_FSTAT(nbdp, vap, flags, cr, *error);

	BHV_READ_UNLOCK(VS_BHV_HEAD(vso));

	if (cr)
		crfree(cr);
}

static void
dsvock_tsif_recall(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	to_be_recalled,
	tk_disp_t	why)
{
	dsvsock_t       *dso = (dsvsock_t *)obj;
	vsock_handle_t	shandle;
	int		msgerr;

	if (client == cellid()) {
		tkc_recall(dso->dsvsock_tclient, to_be_recalled, why);
	} else {
		service_t svc;
		int error;

		SERVICE_MAKE(svc, (cell_t)client, SVC_VSOCK);
		DSVS_HANDLE_MAKE(shandle, dso);
		msgerr = invk_dcvsock_recall(svc, &error,
				&shandle,
				to_be_recalled, why);
		ASSERT(!msgerr);

	}
}

void
dsvsock_dispose(
	void		*obj)
{
	vsock_t		*vso = NULL;
	dsvsock_t       *dso = (dsvsock_t *)obj;

	vso = BHV_VOBJ(&(dso->dsvs_bhv));
	ASSERT(vso->vs_refcnt == 1);
	tkc_release1(dso->dsvsock_tclient, VSOCK_EXISTENCE_TOKEN);
	bhv_remove(&(vso->vs_bh), &(dso->dsvs_bhv));
	tkc_destroy_local(dso->dsvsock_tclient);
	tks_destroy(dso->dsvsock_tserver);
	kmem_zone_free (dsvsock_zone, dso);
	vso->vs_bh.bh_first = NULL;
	vsocket_free(vso);

}


/* ARGSUSED */
static void
dsvock_tsif_recalled(
	void		*obj,
	tk_set_t	done,
	tk_set_t	ndone)
{
	vsock_t		*vso = NULL;
	dsvsock_t       *dso = (dsvsock_t *)obj;


	vso = BHV_VOBJ(&(dso->dsvs_bhv));
	if (vso->vs_flags & VS_IDLE) {
		dsvsock_dispose(dso);
	}

}

/*
 * server side of client returning tokens
 * called on client lastclose
 */
void
I_dsvsock_so_return(
	cell_t		sender,
	objid_t		objid,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{ 
	bhv_desc_t	*bhp;
	dsvsock_t       *dso;
	vsock_t		*vso = NULL;
	int		last;
	bhv_desc_t	*nbdp;
	struct socket 	*so;
	int		error;

	bhp = OBJID_TO_BHV(objid);
	dso = BHV_PDATA(bhp);
	vso = BHV_VOBJ(&(dso->dsvs_bhv));
	ASSERT(to_be_returned == VSOCK_EXISTENCE_TOKENSET);
	last = vsock_hremove (vso, &dso->dsvs_handle);
	tks_return(dso->dsvsock_tserver, sender, to_be_returned,
				TK_NULLSET, unknown, why);
	if (last) {
		ASSERT(vso->vs_refcnt == 1);
		nbdp = BHV_NEXT(bhp);
		so = BHV_PDATA(nbdp);
		error = soclose (so);
		ASSERT(error == 0);
		dsvsock_dispose (dso);
	}
}

/* ARGSUSED */
void
I_dsvsock_obtain_exist (
	cell_t		sender,
	vsock_handle_t  *handlep,
	int		*domp,
	int		*typep,
	int		*protop,
	int		*exist_granted)
{
	vsock_t		*vso = NULL;
	tk_set_t        already_obtained;
	tk_set_t        refused;
	tk_set_t        granted;
	bhv_desc_t	*bhp = NULL;
	dsvsock_t	*dsv;

	vsock_lookup_id(handlep, &vso);
	ASSERT(vso);

	bhp = VSOCK_TO_FIRST_BHV(vso);
	dsv = BHV_TO_DSVSOCK(bhp);
	tks_obtain(dsv->dsvsock_tserver, (tks_ch_t)sender,
		VSOCK_EXISTENCE_TOKENSET, &granted, &refused,
		&already_obtained);
	*domp = vso->vs_domain;
	*typep = vso->vs_type;
	*protop = vso->vs_protocol;
	if (TK_IS_IN_SET(VSOCK_EXISTENCE_TOKENSET, granted)) {
		*exist_granted = 1;
	} else {
		*exist_granted = 0;
		/* drop ref obtained by vsock_lookup_id */
		vsock_drop_ref(vso);
	}
}

