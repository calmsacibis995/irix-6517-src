/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "sys/vsocket.h: $Revision: 1.32 $"

#ifndef __VSOCKET_H__
#define __VSOCKET_H__

#include <sys/socket.h>
#include <sys/socketvar.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <ksys/behavior.h>
#endif

struct vattr;
/*
 * The vsocket is the distributed version of the Irix struct
 * socket.   It contains an interposer object containing an
 * ops-vector which allows operations to be passed down to the
 * real socket, if the server which implements the socket is
 * in the same cell as the application which owns the vsocket.
 * In this case the ops-vector is populated by lvector op-
 * erations which know how to translate vsocket address to
 * socket address and invoke the correct socket operations.
 *
 *
 *		struct vfile
 *		     |
 *		     |
 *		     |
 *		struct vsocket ----
 *		     |		  |
 *		     |		{lvector)
 *		     |		  |
 *		struct socket <----
 *		     |
 *		     |
 *
 *
 * If the server which implements the socket is on a different
 * cell to the application which owns the vsocket, then the
 * ops-vector on the client side is populated by the dc_vector
 * operations, which know how to carry out rcp operations to
 * the server cell, and on the server side by ds_vector operations
 * which know how to invoke the corresponding socket operations
 * and pass the results back.
 * 
 *              struct vfile
 *                  |
 *                  |
 *                  |
 *              struct vsocket          struct vsocket
 *                  |                          |
 * (dc_vector)      |                          |        (ds_vector)
 *                  |                          |
 *              struct dcvsock <======> struct dsvsock
 *                                             |
 *                                             |        (direct *so)
 *                                             |
 *                                      struct socket
 */

typedef struct vsocket {
	bhv_head_t	vs_bh;		/* Base behaviour */
	u_int	vs_flags;
	mutex_t	vs_lock;
	sv_t	vs_sv;
	int	vs_refcnt;
	int	vs_type;		/* cached from create */
	int	vs_protocol;		/* cached from create */
	int	vs_domain;		/* cached from create */
	void	*vs_vsent;
} vsock_t;


#ifdef _KERNEL

/*
 *  Flag values for vs_flags
 */
#define VS_LASTREFWAIT  0x0001          /* someone waiting for last ref */
#define VS_RELEWAIT     0x0002          /* wakeup on each rele */
#define	VS_CLIENT_CLOSE	0x0004		/* XXX not used? */
#define	VS_SERVER_CLOSE	0x0008		/* XXX not used? */
#define	VS_IDLE		0x0010		/* token module called idle */

/*
 * Conversion macros.
 */
#define VSOCK_TO_FIRST_BHV(vs)	(BHV_HEAD_FIRST(&(vs)->vs_bh))
#define	VS_BHV_HEAD(vs)         ((bhv_head_t *)(&((vs)->vs_bh)))
#define	BHV_TO_VSOCK(bhv)       ((struct vsocket *)(BHV_VOBJ(bhv)))
#define	BHV_TO_VSOCK_TRY(bhv)   ((struct vsocket *)(BHV_VOBJNULL(bhv)))

/*
 * Locking macros.
 */
#define VSOCKET_LOCK_INIT(v)	mutex_init(&(v->vs_lock), MUTEX_DEFAULT, "vsock")
#define VSOCKET_LOCK_DESTROY(v)	mutex_destroy(&(v->vs_lock))
#define VSOCKET_LOCK(v)		mutex_lock(&(v->vs_lock), PZERO)
#define VSOCKET_LOCK_TRY(v)	mutex_trylock(&(v->vs_lock))
#define VSOCKET_UNLOCK(v)	mutex_unlock(&(v->vs_lock))


enum vs_type {VS_CREATE, VS_CLOSE, VS_ABORT, VS_SEND, VS_RECEIVE,
	VS_ACCEPT, VS_LISTEN, VS_CONNECT, VS_CONNECT2, VS_BIND,
	VS_SHUTDOWN, VS_GETATTR, VS_SETATTR, VS_IOCTL, VS_SELECT,
	VS_GETSOCKNAME, VS_SETFL, VS_GETPEERNAME, VS_FSTAT, VS_FCNTL,
	VS_PATHCONF, VS_MAX};


typedef struct vsocket_ops {
	bhv_position_t	vs_position;	/* position within behavior chain */
	int	(*vs_create)(struct vsocket *, int, struct vsocket  **, int, 
			     int);
	int	(*vs_close)(bhv_desc_t *, int, int);
	int	(*vs_abort)();
	int	(*vs_send)(bhv_desc_t *, struct mbuf *, struct uio *,
			int, struct mbuf *);
	int	(*vs_sendit)(bhv_desc_t *, struct msghdr *,
			int, rval_t *);
	int	(*vs_receive)(bhv_desc_t *, struct mbuf **, struct uio *,
			int *, struct mbuf **);
	int	(*vs_recvit)(bhv_desc_t *, struct msghdr *,
			int *, void *, void *, rval_t *, int);
	int	(*vs_accept)(bhv_desc_t *, caddr_t, sysarg_t *, sysarg_t, 
			rval_t *, struct vsocket **);
	int	(*vs_listen)(bhv_desc_t *, int);
	int	(*vs_connect)(bhv_desc_t *, caddr_t, sysarg_t);
	int	(*vs_connect2)(bhv_desc_t *, bhv_desc_t *);
	int	(*vs_bind)(bhv_desc_t *, struct mbuf *);
	int	(*vs_shutdown)(bhv_desc_t *, int);
	int	(*vs_getattr)(bhv_desc_t *, int, int, struct mbuf **);
	int	(*vs_setattr)(bhv_desc_t *, int, int, struct mbuf *);
	int	(*vs_ioctl)(bhv_desc_t *, int, void *, int, struct cred *, 
			    rval_t *);
	int	(*vs_select)(bhv_desc_t *, short, int, short *, 
			     struct pollhead **, unsigned int *);
	int	(*vs_getsockname)(bhv_desc_t *, caddr_t, sysarg_t *,
			sysarg_t, rval_t *);
	int	(*vs_setfl)(bhv_desc_t *, int, int, struct cred *);
	int	(*vs_getpeername)(bhv_desc_t *, caddr_t, sysarg_t *, rval_t *);
	int	(*vs_fstat)(bhv_desc_t *, struct vattr *, int, struct cred *);
	int	(*vs_fcntl)(bhv_desc_t *, int, void *, rval_t *);
	int	(*vs_pathconf)(bhv_desc_t *, int, long *, struct cred *);
} vsocket_ops_t;

extern struct vsocket_ops lvector;

#define	VS_VOP(vs) ((vsocket_ops_t *)(BHV_OPS(BHV_HEAD_FIRST(&(vs->vs_bh)))))

#define	VSOP_CREATE(vs, dom, avso, type, proto, error)			\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_create)(VSOCK_TO_FIRST_BHV(vs), dom, avso, type, proto);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_CLOSE(vs, lastclose, fflag, error)				\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_close)(VSOCK_TO_FIRST_BHV(vs), lastclose, fflag);  \
	if (!lastclose) BHV_READ_UNLOCK(&(vs)->vs_bh);			\
}
#define	VSOP_SEND(vs, nam, uio, flags, rights, error)			\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_send)(VSOCK_TO_FIRST_BHV(vs), nam, uio, flags, rights);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_SENDIT(vs, mp, flags, rvp, error)			\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_sendit)(VSOCK_TO_FIRST_BHV(vs), mp, flags, rvp);		\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_RECEIVE(vs, nam, uio, flags, rights, error)		\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_receive)(VSOCK_TO_FIRST_BHV(vs), nam, uio, flags, rights);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_RECVIT(vs, mp, flags, a1, a2, rvp, t, error)		\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_recvit)(VSOCK_TO_FIRST_BHV(vs), mp, flags, a1, a2, rvp, t);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_CONNECT(vs, nam, namelen, error)				\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_connect)(VSOCK_TO_FIRST_BHV(vs), nam, namelen);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define VSOP_BIND(vs, nam, error)					\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_bind)(VSOCK_TO_FIRST_BHV(vs), nam);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_GETATTR(vs, level, name, m, error)				\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_getattr)(VSOCK_TO_FIRST_BHV(vs), level, name, m);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_SETATTR(vs, level, name, m, error)				\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_setattr)(VSOCK_TO_FIRST_BHV(vs), level, name, m);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define VSOP_IOCTL(vs, cmd, arg, flag, cr, rvalp, error)		\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_ioctl)(VSOCK_TO_FIRST_BHV(vs), cmd, arg, flag, cr, rvalp);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_SELECT(vs, vents, anyyet, reventsp, phpp, genp, error)	\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_select)(VSOCK_TO_FIRST_BHV(vs), vents, anyyet, reventsp, phpp, genp);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_LISTEN(vs, backlog, error)					\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_listen)(VSOCK_TO_FIRST_BHV(vs), backlog);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define VSOP_ACCEPT(vs, nam, anamelen, namelen, rvp, nvso, error)		\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_accept)(VSOCK_TO_FIRST_BHV(vs), nam, anamelen, namelen, rvp, nvso);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define VSOP_GETSOCKNAME(vs, asa, alen, len, rvp, error)		\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_getsockname)(VSOCK_TO_FIRST_BHV(vs), asa, alen, len, rvp);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define VSOP_SHUTDOWN(vs, how, error)					\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_shutdown)(VSOCK_TO_FIRST_BHV(vs), how);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define VSOP_SETFL(vs, oflags, nflags, cr, error)			\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_setfl)(VSOCK_TO_FIRST_BHV(vs), oflags, nflags, cr);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define VSOP_GETPEERNAME(vs, asa, alen, rvp, error)			\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_getpeername)(VSOCK_TO_FIRST_BHV(vs), asa, alen, rvp);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define VSOP_FSTAT(vs, vap, flags, cr, error)				\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_fstat)(VSOCK_TO_FIRST_BHV(vs), vap, flags, cr);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_CONNECT2(vs1, vs2, error)					\
{									\
	BHV_READ_LOCK(&(vs1)->vs_bh);					\
	error=(*VS_VOP(vs1)->vs_connect2)(VSOCK_TO_FIRST_BHV(vs1), VSOCK_TO_FIRST_BHV(vs2));	\
	BHV_READ_UNLOCK(&(vs1)->vs_bh);					\
}
#define	VSOP_FCNTL(vs, cmd, arg, rvp, error)				\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_fcntl)(VSOCK_TO_FIRST_BHV(vs), cmd, arg, rvp);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}
#define	VSOP_PATHCONF(vs, cmd, val, cr, error)				\
{									\
	BHV_READ_LOCK(&(vs)->vs_bh);					\
	error=(*VS_VOP(vs)->vs_pathconf)(VSOCK_TO_FIRST_BHV(vs), cmd, val, cr);	\
	BHV_READ_UNLOCK(&(vs)->vs_bh);					\
}

/*
 * Registration for the positions at which the different vsock behaviors 
 * are chained.  When on the same chain, a behavior with a higher position 
 * number is invoked before one with a lower position number.
 */
#define	VSOCK_POSITION_SOCK	BHV_POSITION_BASE   	/* chain bottom */
#define	VSOCK_POSITION_DC	BHV_POSITION_TOP  	/* distr. client */
#define	VSOCK_POSITION_DS	BHV_POSITION_TOP  	/* distr. server */

#define VS_NEXT(cur, next, op)		\
{					\
	next = BHV_NEXT(cur);		\
	ASSERT(next);			\
}

#define PVS_VOP(bdp)    ((struct  vsocket_ops *)(BHV_OPS(bdp)))

#define PVSOP_SEND(bdp, nam, uio, flags, rights, error)			\
	error=(*PVS_VOP(bdp)->vs_send)(bdp, nam, uio, flags, rights)
#define PVSOP_SENDIT(bdp, mp, flags, rvp, error)		\
	error=(*PVS_VOP(bdp)->vs_sendit)(bdp, mp, flags, rvp)
#define PVSOP_RECEIVE(bdp, nam, uio, flags, rights, error)		\
	error=(*PVS_VOP(bdp)->vs_receive)(bdp, nam, uio, flags, rights)
#define PVSOP_RECVIT(bdp, mp, flags, a1, a2, rvp, t, error)\
	error=(*PVS_VOP(bdp)->vs_recvit)(bdp, mp, flags, a1, a2, rvp, t)
#define	PVSOP_CONNECT(bdp, nam, namelen, error)				\
	error=(*PVS_VOP(bdp)->vs_connect)(bdp, nam, namelen)
#define PVSOP_CONNECT2(bdp1, bdp2, error)				\
	error=(*PVS_VOP(bdp1)->vs_connect2)(bdp1, bdp2)
#define PVSOP_BIND(bdp, nam, error)					\
	error=(*PVS_VOP(bdp)->vs_bind)(bdp, nam)
#define PVSOP_GETATTR(bdp, level, name, m, error)			\
	error=(*PVS_VOP(bdp)->vs_getattr)(bdp, level, name, m)
#define PVSOP_SETATTR(bdp, level, name, m, error)			\
	error=(*PVS_VOP(bdp)->vs_setattr)(bdp, level, name, m)
#define PVSOP_IOCTL(bdp, cmd, arg, flag, cr, rvalp, error)		\
	error=(*PVS_VOP(bdp)->vs_ioctl)(bdp, cmd, arg, flag, cr, rvalp)
#define	PVSOP_SELECT(bdp, vents, anyyet, reventsp, phpp, genp, error)	\
	error=(*PVS_VOP(bdp)->vs_select)(bdp, vents, anyyet, reventsp, phpp, genp)
#define PVSOP_SHUTDOWN(bdp, how, error)					\
	error=(*PVS_VOP(bdp)->vs_shutdown)(bdp, how)
#define	PVSOP_LISTEN(bdp, backlog, error)					\
	error=(*PVS_VOP(bdp)->vs_listen)(bdp, backlog)
#define PVSOP_ACCEPT(bdp, nam, anamelen, namelen, rvp, nvso, error)             \
	error=(*PVS_VOP(bdp)->vs_accept)(bdp, nam, anamelen, namelen, \
		rvp, nvso)
#define PVSOP_GETSOCKNAME(bdp, asa, alen, len, rvp, error)		\
	error=(*PVS_VOP(bdp)->vs_getsockname)(bdp, asa, alen, len, rvp)
#define PVSOP_GETPEERNAME(bdp, asa, alen, rvp, error)			\
	 error=(*PVS_VOP(bdp)->vs_getpeername)(bdp, asa, alen, rvp)
#define PVSOP_SETFL(bdp, oflags, nflags, cr, error)			\
	error=(*PVS_VOP(bdp)->vs_setfl)(bdp, oflags, nflags, cr)
#define	PVSOP_FCNTL(bdp, cmd, arg, rvp, error)				\
	error=(*PVS_VOP(bdp)->vs_fcntl)(bdp, cmd, arg, rvp)
#define PVSOP_FSTAT(bdp, vap, flags, cr, error)                           \
         error=(*PVS_VOP(bdp)->vs_fstat)(bdp, vap, flags, cr)

enum vsockop {VT_CREATE, VT_CLOSE, VT_ABORT, VT_SEND, VT_RECEIVE,
	VT_ACCEPT, VT_LISTEN, VT_CONNECT, VT_CONNECT2, VT_BIND,
	VT_SHUTDOWN, VT_GETATTR, VT_SETATTR, VT_IOCTL, VT_SELECT};

#ifdef DEBUG
#define	LSTATISTICS(i)		lstatistics[i]++
#define	DCSTATISTICS(i)		dcstatistics[i]++
#define	DSSTATISTICS(i)		dsstatistics[i]++
#define	DSS_STATISTICS(i)	dss_statistics[i]++
#else
#define	LSTATISTICS(i)
#define	DCSTATISTICS(i)
#define	DSSTATISTICS(i)
#define	DSS_STATISTICS(i)
#endif /* DEBUG */

/* Create a new vsocket/socket */
#define vsocreate(dom, avso, type, proto)	\
        vsocreate_internal(dom, avso, type, proto, NULL)

/* Wrap an existing socket in a vsocket */
#define vsowrap(so, avso, dom, type, proto)	\
        vsocreate_internal(dom, avso, type, proto, so)

int	vsocreate_internal(int, struct vsocket **, int, int, struct socket *);
int	vsoo_close(struct vsocket *, int);
int	getvsock(int, struct vsocket **);
int	vso_bind(struct vsocket *, struct mbuf *);
int	vsoo_listen(struct vsocket *, int);
int	vso_connect(struct vsocket *, caddr_t, int);
int	vsoo_send(struct vsocket *, struct mbuf *,
		register struct uio *, int, struct mbuf *);
int	vsoo_receive(struct vsocket *, struct mbuf **,
		struct uio *, int, struct mbuf **);
int	vsoo_setopt(struct vsocket *, int, int, struct mbuf *);

void	vso_initialize(void);
struct vsocket 	*vsocket_alloc(void);
void	vsocket_release(struct vsocket *);
int	vsock_drop_ref(struct vsocket *);

void	vsock_trace_init(void);
void	vsock_trace_header (void);
void	vsock_trace (enum vs_type, void *,
		void *, u_int, long, long, long);
void	idbg_vsocket(struct vsocket *);
int 	sendit(bhv_desc_t *, struct msghdr *, int, rval_t *);

int 	recvit(bhv_desc_t *, struct msghdr *, int *, void *, 
		void *, rval_t *, int);


#endif /* _KERNEL */

#endif /* __VSOCKET_H__ */
