/*
 * Pass through interface to the virtual socket abstraction.
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 */

#ident "$Revision: 1.12 $"

#include <sys/types.h>
#include <limits.h>
#include <sys/fs_subr.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/vsocket.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/debug.h>

/*
 * This file contains the code for the pass-through ops vector
 * for vsockets.   Pass-through operations  - when a socket is
 * implemented entirely on a single node - replace the client-
 * and server-side operations needed when the vsocket and the
 * real socket are implemented on two different nodes.
 *
 *		struct vfile
 *		    |
 *		    |
 *		    |
 *		struct vsocket
 *		    |	
 *		    | (lvector)
 *		    |
 *		struct socket
 *
 * Each socket operation comes down from the syscall level - or
 * in some cases across from tpi - and from the vsocket it is
 * redirected to the real socket operations which use the struct
 * socket.
 *
 * The db_vector could strictly be replaced by a slghtly more
 * powerful VOPS_ macro which would call the sobind()....
 * routines directly with the socket *so address derived from
 * the vsocket *vso address.     I've left them as real routines
 * in initial implementation because they are useful for debugging
 * and statistics gathering, and because it keeps the overall
 * structure consistent.
 */

#ifdef	DEBUG
int	lstatistics[VS_MAX];
#endif

extern	socreate(int, struct socket **, register int, int);
extern	sogetopt(bhv_desc_t *, int, int, struct mbuf **);
extern	sosetopt(bhv_desc_t *, int, int, struct mbuf *);
extern	sopoll(bhv_desc_t *, short, int, short *, struct pollhead **, unsigned int *);
extern	soioctl(bhv_desc_t *, int, void *, int, struct cred *, rval_t *);
extern	uipc_connect(bhv_desc_t *, caddr_t, sysarg_t);
extern	uipc_getsockname(bhv_desc_t *, caddr_t, sysarg_t *, sysarg_t, rval_t *);
extern	uipc_accept(bhv_desc_t *, caddr_t, sysarg_t *, sysarg_t, rval_t *, 
		struct vsocket **);
extern	uipc_getpeername(bhv_desc_t *, caddr_t, sysarg_t *, rval_t *);
extern	uipc_setfl1(bhv_desc_t *, int, int, struct cred *);
extern	uipc_getattr1(bhv_desc_t *, struct vattr *, int, struct cred *);
extern	uipc_fcntl1(bhv_desc_t *, int, void *, rval_t *);
extern	void idbg_soc(struct socket *);

/*ARGSUSED*/
static int
lcreate(
	struct vsocket *vso,
	int             dom,
	struct vsocket  **avso,
	int             type,
	int             proto)
{
	struct socket *so;
	int error;

	LSTATISTICS(VS_CREATE);
	error = socreate(dom, &so, type, proto);
	if (!error) {
		bhv_desc_init(&(so->so_bhv), so, vso, &lvector);
		bhv_insert_initial(&(vso->vs_bh), &(so->so_bhv));
	}
	return (error);
}

/*ARGSUSED*/
static int
lclose(
	bhv_desc_t *vsi,
	int lastclose,
	int flag)
{
	struct vsocket *vso = BHV_TO_VSOCK(vsi);
	struct socket *so;
	int error = 0;

	LSTATISTICS(VS_CLOSE);
	VSO_UTRACE(UTN('lclo','se0 '), vso, __return_address);
	VSOCKET_LOCK(vso);
	/* Wait for other references to be released */
	vso->vs_flags |= VS_LASTREFWAIT;
	while (vso->vs_refcnt > 1) {
		VSO_UTRACE(UTN('lclo','se1 '), vso, __return_address);
		sv_wait(&vso->vs_sv, PZERO, &vso->vs_lock, 0);
		VSOCKET_LOCK(vso);
	}
	VSOCKET_UNLOCK(vso);

	if (lastclose) {
		so = BHV_PDATA(vsi);
		ASSERT(so);
		/*
		 * Hold socket lock to serialize with vsock_from_addr(); this
		 * way it can't find the vsock from the socket
		 */
		VSO_UTRACE(UTN('lclo','se2 '), vso, __return_address);
		SO_UTRACE(UTN('lclo','se3 '), so, __return_address);
		SOCKET_LOCK(so);
		bhv_remove(&(vso->vs_bh), &(so->so_bhv));
		BHV_VOBJNULL(&(so->so_bhv)) = 0;	/* SCA */
		SOCKET_UNLOCK(so);
		error = soclose(so);
		vsocket_release(vso);
	}
	return (error);
}

vsocket_ops_t lvector = {
    BHV_IDENTITY_INIT_POSITION(VSOCK_POSITION_SOCK),
    lcreate,		/* create */
    lclose,		/* close */
    NULL,	 	/* abort */
    sosend,		/* send */
    sendit,		/* sendit */
    soreceive,		/* receive */
    recvit,		/* recvit */
    uipc_accept,	/* accept */
    solisten,		/* listen */
    uipc_connect,	/* connect */
    soconnect2,		/* connect2 */
    sobind,		/* bind */
    soshutdown,		/* shutdown */
    sogetopt,		/* getattr */
    sosetopt,		/* setattr */
    soioctl,		/* ioctl */
    sopoll,		/* select */
    uipc_getsockname,	/* getsockname */
    uipc_setfl1,	/* setfl */
    uipc_getpeername,	/* getpeername */
    uipc_getattr1,	/* fstat */
    uipc_fcntl1,	/* fcntl */
    fs_pathconf,	/* pathconf */
};
