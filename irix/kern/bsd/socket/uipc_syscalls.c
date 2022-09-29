/*
 * System call interface to the socket abstraction.
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	%W% (Berkeley) %G%
 */
#ident "$Revision: 4.100 $"

#include <limits.h>
#include <tcp-param.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/domain.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/sat.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/vsocket.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/xlate.h>
#include <sys/kuio.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/uthread.h>
#include <ksys/fdt.h>
#include <net/route.h>
#include "bsd/netinet/in.h"
#include "bsd/netinet/in_pcb.h"
#include <sys/kabi.h>

extern void vsocket_hold(struct vsocket *);
extern int soaccept(struct socket *, struct mbuf *);

int sockargs(struct mbuf **, void *, int, int);

#if _MIPS_SIM == _ABI64
int cmsghdr_to_irix5(struct mbuf *);
#endif

struct socketa {
	sysarg_t	domain;
	sysarg_t	type;
	sysarg_t	protocol;
};

int
socket(struct socketa *uap, rval_t *rvp)
{
	int error, error2, fd;
	struct vfile *fp;
	struct vsocket	*vso = NULL;

	error = vsocreate(uap->domain, &vso, uap->type, uap->protocol);
	if (error) {
		return error;
	}

	error2 = makevsock(&fp, &fd);
	if (error2) {
	    VSOP_CLOSE(vso, 1, 0, error);
	    return error2;
	}

	vfile_ready(fp, vso);
	rvp->r_val1 = fd;

#ifdef sgi /* Audit */
	{
	struct socket *so =
		(struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));

	_SAT_BSDIPC_CREATE(fd, so, uap->domain, uap->type, 0);
	}
#endif

#	ifdef	STP_SUPPORT
	/* mark it as an STP socket, if proto family says STP */
	if(IPPROTO_STP == uap->protocol)  {
		struct socket *so = (bhvtos(VSOCK_TO_FIRST_BHV(vso)));
	
		so->so_state |= SS_STP;
	}
#	endif	/* STP_SUPPORT */
	return 0;
}

struct binda {
	sysarg_t	s;
	caddr_t		name;
	sysarg_t	namelen;
};

/* ARGSUSED */
int
bind(struct binda *uap, rval_t *rvp)
{
	int error;
	struct vsocket	*vso = NULL;
	struct mbuf *nam;

	if (uap->name == NULL)
	  return EDESTADDRREQ;
	if (error = getvsock(uap->s, &vso)) {
	    return (error);
	}
	error = sockargs(&nam, uap->name, uap->namelen, MT_SONAME);
	if (error)
		return error;
	VSOP_BIND(vso, nam, error);

#ifdef sgi /* Audit */
	if (!error) {
	    struct socket *so =
		(struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	    _SAT_BSDIPC_ADDR(uap->s, so, nam, 0);
	}
#endif
	m_freem(nam);
	return error;
}

struct listena {
	sysarg_t	s;
	sysarg_t	backlog;
};

/* ARGSUSED */
int
listen(struct listena *uap, rval_t *rvp)
{
	int error;
	struct vsocket	*vso;

	if (error = getvsock(uap->s, &vso)) {
	    return (error);
	}
	VSOP_LISTEN(vso, uap->backlog, error);
	return error;
}

struct accepta {
	sysarg_t	s;
	caddr_t		name;
	sysarg_t	*anamelen;
};
int
accept(struct accepta *uap, rval_t *rvp)
{
	struct vsocket	*vso = NULL;
	int namelen;
	int	error = 0;

	if (uap->name != NULL) {
		error = copyin(uap->anamelen, &namelen, sizeof (namelen));
		if (error)
			return error;
	}
	if (error = getvsock(uap->s, &vso)) {
	    return (error);
	}
	VSOP_ACCEPT(vso, uap->name, uap->anamelen, namelen, rvp, NULL, error);
	return error;
}

struct connecta {
	sysarg_t	s;
	caddr_t		name;
	sysarg_t	namelen;
};

/*ARGSUSED*/
int
connect(struct connecta *uap, rval_t *rvp)
{
	int	error;
	struct vsocket	*vso = NULL;

	if (error = getvsock(uap->s, &vso)) {
	    return (error);
	}
	VSOP_CONNECT(vso, uap->name, uap->namelen, error);
#ifdef sgi /* Audit */
	{
	struct socket *so =
		(struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	if (! so->so_error)
		_SAT_BSDIPC_ADDR(uap->s, so, NULL, 0); /* XXX */
	}
#endif
	return error;
}

struct sendtoa {
	sysarg_t	s;
	caddr_t		buf;
	sysarg_t	len;
	sysarg_t	flags;
	caddr_t		to;
	sysarg_t	tolen;
};

int
sendto(struct sendtoa *uap, rval_t *rvp)
{
	struct msghdr msg;
	struct iovec aiov;
	int error;
	struct vsocket	*vso;

	if (error = getvsock(uap->s, &vso))
	    return (error);

	msg.msg_name = uap->to;
	msg.msg_namelen = uap->tolen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_accrights = 0;
	msg.msg_accrightslen = 0;
	VSOP_SENDIT(vso, &msg, uap->flags, rvp, error);
	return(error);
}

struct senda {
	sysarg_t	s;
	caddr_t		buf;
	sysarg_t	len;
	sysarg_t	flags;
};

int
send(struct senda *uap, rval_t *rvp)
{
	struct msghdr msg;
	struct iovec aiov;
	struct vsocket	*vso;
	int error;

	if (error = getvsock(uap->s, &vso))
	    return (error);

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_accrights = 0;
	msg.msg_accrightslen = 0;
	VSOP_SENDIT(vso, &msg, uap->flags, rvp, error);
	return(error);
}

struct sendmsga {
	sysarg_t	s;
	caddr_t		msg;
	sysarg_t	flags;
};

#if _MIPS_SIM == _ABI64
struct irix5_msghdr {
	app32_ptr_t	msg_name;
	app32_int_t	msg_namelen;
	app32_ptr_t	msg_iov;
	app32_int_t	msg_iovlen;
	app32_ptr_t	msg_accrights;
	app32_int_t	msg_accrightslen;
};


/*ARGSUSED*/
static int
irix5_to_msghdr(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	register struct msghdr *mh;
	register struct irix5_msghdr *i5_mh;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);
	ASSERT(count == 1);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_msghdr) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
					 sizeof(struct irix5_msghdr));
		info->copysize = sizeof(struct irix5_msghdr);
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_msghdr));
	ASSERT(info->copybuf != NULL);

	mh = to;

	i5_mh = info->copybuf;
	
	mh->msg_name = (caddr_t)(__psint_t)i5_mh->msg_name;
	mh->msg_namelen = i5_mh->msg_namelen;
	mh->msg_iov = (struct iovec *)(__psint_t)i5_mh->msg_iov;
	mh->msg_iovlen = i5_mh->msg_iovlen;
	mh->msg_accrights = (caddr_t)(__psint_t)i5_mh->msg_accrights;
	mh->msg_accrightslen = i5_mh->msg_accrightslen;

	return 0;
}

#endif /* _ABI64 */


int
sendmsg(struct sendmsga *uap, rval_t *rvp)
{
	int error;
	struct msghdr msg;
	struct iovec aiov[MSG_MAXIOVLEN];
	struct vsocket	*vso;
#if _MIPS_SIM == _ABI64
	int abi = get_current_abi();
#endif

	if (error = getvsock(uap->s, &vso))
	    return (error);

#ifdef __sgi

	error = COPYIN_XLATE(uap->msg, &msg, sizeof (msg), irix5_to_msghdr,
			     abi, 1);
#else
	error = copyin(uap->msg, &msg, sizeof (msg));
#endif
	if (error)
		return error;
	if ((u_int)msg.msg_iovlen >= sizeof (aiov) / sizeof (aiov[0]))
		return EINVAL;
#ifdef __sgi
	error =
	    COPYIN_XLATE(msg.msg_iov, aiov, msg.msg_iovlen * sizeof (aiov[0]),
				irix5_to_iovec, abi,
				msg.msg_iovlen);
#else
	error =
	    copyin(msg.msg_iov, aiov, msg.msg_iovlen * sizeof (aiov[0]));
#endif
	if (error)
		return error;
	msg.msg_iov = aiov;
	VSOP_SENDIT(vso, &msg, uap->flags, rvp, error);
	return(error);
}

/* ARGSUSED */
int
sendit(
	bhv_desc_t *vsi,
	struct msghdr *mp, 
	int flags, 
	rval_t *rvp)
{
	int error;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	struct mbuf *to, *rights;
	int len;
	struct vsocket *vso = BHV_TO_VSOCK(vsi);

	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_offset = 0;
	auio.uio_resid = 0;
	auio.uio_sigpipe = 0;
	auio.uio_pio = 0;
	auio.uio_pbuf = 0;
	for (i = 0, iov = mp->msg_iov; i < mp->msg_iovlen; i++, iov++) {
		if (iov->iov_len > SSIZE_MAX)
			return EINVAL;
		auio.uio_resid += iov->iov_len;
	}
	if (mp->msg_name) {
		error = sockargs(&to, mp->msg_name, mp->msg_namelen, MT_SONAME);
		if (error)
			return error;
	} else
		to = 0;
	if (mp->msg_accrights) {
		error =
		    sockargs(&rights, mp->msg_accrights, mp->msg_accrightslen,
		    MT_RIGHTS);
		if (error)
			goto bad;
	} else
		rights = 0;
	len = auio.uio_resid;
	VSOP_SEND(vso, to, &auio, flags, rights, error);
	if (auio.uio_resid != len && error == EINTR)
		error = 0;

	/* Send sigpipe to current thread, if indicated. */
	if (auio.uio_sigpipe) {
		ASSERT(error != 0);
#if CELL_IRIX
		sigtopid(current_pid(), SIGPIPE, SIG_ISKERN, 0, 0, NULL);
#else
		sigtouthread(curuthread, SIGPIPE, (k_siginfo_t *)NULL);
#endif
	}

	rvp->r_val1 = len - auio.uio_resid;
	if (rights)
		m_freem(rights);
bad:
	if (to)
		m_freem(to);
	return error;
}

struct recvfroma {
	sysarg_t	s;
	caddr_t		buf;
	sysarg_t	len;
	sysarg_t	flags;
	caddr_t		from;
	sysarg_t	*fromlenaddr;
};

int
recvfrom(struct recvfroma *uap, rval_t *rvp)
{
	int error;
	struct msghdr msg;
	struct iovec aiov;
	int len;
	int flags;
	struct vsocket	*vso;

	if (error = getvsock(uap->s, &vso))
	    return (error);

	if (uap->fromlenaddr != NULL) {
	    error = copyin(uap->fromlenaddr, &len, sizeof (len));
	    if (error)
		    return error;
	} else
	    len = 0;
	msg.msg_name = uap->from;
	msg.msg_namelen = len;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_accrights = 0;
	msg.msg_accrightslen = 0;
	flags = uap->flags;
	VSOP_RECVIT(vso, &msg, &flags, (caddr_t)uap->fromlenaddr,
		 0, rvp, BSD43_SOCKETS, error);
	return(error);
}

struct recva {
	sysarg_t	s;
	caddr_t		buf;
	sysarg_t	len;
	sysarg_t	flags;
};

int
recv(struct recva *uap, rval_t *rvp)
{
	struct msghdr msg;
	struct iovec aiov;
	int flags;
	int error;
	struct vsocket	*vso;

	if (error = getvsock(uap->s, &vso))
	    return (error);

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_accrights = 0;
	msg.msg_accrightslen = 0;
	flags = uap->flags;
	VSOP_RECVIT(vso, &msg, &flags, 0, 0, rvp, BSD43_SOCKETS, error);
	return(error);
}

struct recvmsga {
	sysarg_t	s;
	struct msghdr	*msg;
	sysarg_t	flags;
};

struct irix5_recvmsga {
	sysarg_t		s;
	struct irix5_msghdr	*msg;
	sysarg_t		flags;
};

int
recvmsg(struct recvmsga *uap, rval_t *rvp)
{
	int flags;
	int error;
	struct msghdr msg;
	struct iovec aiov[MSG_MAXIOVLEN];
	struct vsocket	*vso;
#if _MIPS_SIM == _ABI64
	struct irix5_recvmsga *i5_uap;
	int abi = get_current_abi();
#endif

#ifdef __sgi

	error = COPYIN_XLATE(uap->msg, &msg, sizeof (msg), irix5_to_msghdr,
		abi, 1);
#else
	error = copyin(uap->msg, &msg, sizeof (msg));
#endif
	if (error)
	        return error;
	if ((u_int)msg.msg_iovlen >= sizeof (aiov) / sizeof (aiov[0]))
		return EMSGSIZE;
#ifdef __sgi
	error =
	    COPYIN_XLATE(msg.msg_iov, aiov, msg.msg_iovlen * sizeof (aiov[0]),
			irix5_to_iovec, abi, msg.msg_iovlen);
#else
	error =
	    copyin(msg.msg_iov, aiov, msg.msg_iovlen * sizeof (aiov[0]));
#endif
	if (error)
		return error;
	
	msg.msg_iov = aiov;

	if (error = getvsock(uap->s, &vso))
	    return (error);

#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(abi)) {
		i5_uap = (struct irix5_recvmsga *)(__psint_t)uap;
		flags = i5_uap->flags;
		VSOP_RECVIT(vso, &msg, &flags, 
		      &i5_uap->msg->msg_namelen,
		      &i5_uap->msg->msg_accrightslen, rvp, 
		      BSD43_SOCKETS, error);
		return(error);
	}
	else
#endif	/* _ABI64 */
	{
	flags = uap->flags;
	VSOP_RECVIT(vso, &msg, &flags, &uap->msg->msg_namelen,
	      &uap->msg->msg_accrightslen, rvp, BSD43_SOCKETS, error);
		return(error);
	}
}

#if _MIPS_SIM == _ABI64
struct irix5_xmsghdr {
	app32_ptr_t	msg_name;
	app32_int_t	msg_namelen;
	app32_ptr_t	msg_iov;
	app32_int_t	msg_iovlen;
	app32_ptr_t	msg_ctrl;
	app32_int_t	msg_ctrllen;
	app32_int_t	msg_flags;
};


/*ARGSUSED*/
static int
irix5_to_xmsghdr(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	register struct xpg4_msghdr *mh;
	register struct irix5_xmsghdr *i5_xmh;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);
	ASSERT(count == 1);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_xmsghdr) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
					 sizeof(struct irix5_xmsghdr));
		info->copysize = sizeof(struct irix5_xmsghdr);
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_xmsghdr));
	ASSERT(info->copybuf != NULL);

	mh = to;

	i5_xmh = info->copybuf;

	mh->msg_name = (caddr_t)(__psint_t)i5_xmh->msg_name;
	mh->msg_namelen = i5_xmh->msg_namelen;
	mh->msg_iov = (struct iovec *)(__psint_t)i5_xmh->msg_iov;
	mh->msg_iovlen = i5_xmh->msg_iovlen;
	mh->msg_ctrl = (caddr_t)(__psint_t)i5_xmh->msg_ctrl;
	mh->msg_ctrllen = i5_xmh->msg_ctrllen;
	mh->msg_flags = i5_xmh->msg_flags;

	return 0;
}
#endif /* _ABI64 */

static int
xmsg_to_msg(struct xpg4_msghdr *xmsg, struct msghdr *msg)
{
	msg->msg_name = xmsg->msg_name;
	msg->msg_namelen = (int)xmsg->msg_namelen;
	if (xmsg->msg_namelen > (size_t)msg->msg_namelen) {
		return(EINVAL);
	}
	msg->msg_iov = xmsg->msg_iov;
	msg->msg_iovlen = xmsg->msg_iovlen;
	msg->msg_accrights = xmsg->msg_ctrl;
	msg->msg_accrightslen = (int)xmsg->msg_ctrllen;
	if (xmsg->msg_ctrllen > (size_t)msg->msg_accrightslen) {
		return(EINVAL);
	}
	return 0;
}

struct xpg4_recvmsga {
	sysarg_t		s;
	struct xpg4_msghdr	*msg;
	sysarg_t		flags;
};

struct irix5_xpg4_recvmsga {
	sysarg_t		s;
	struct irix5_xmsghdr	*msg;
	sysarg_t		flags;
};

int
xpg4_recvmsg(struct xpg4_recvmsga *uap, rval_t *rvp)
{
	int error;
	struct msghdr msg;
	struct xpg4_msghdr xmsg;
	struct iovec aiov[MSG_MAXIOVLEN];
	int flags;
#if _MIPS_SIM == _ABI64
	struct irix5_xpg4_recvmsga *i5_uap;
	int abi = get_current_abi();
#endif
	struct vsocket	*vso;

	if (error = getvsock(uap->s, &vso))
	    return (error);

	error = COPYIN_XLATE(uap->msg, &xmsg, sizeof (xmsg),
		irix5_to_xmsghdr, abi, 1);
	if (error)
		return error;
	if ((u_int)xmsg.msg_iovlen >= sizeof (aiov) / sizeof (aiov[0]))
		return EMSGSIZE;
	error =
	    COPYIN_XLATE(xmsg.msg_iov, aiov, xmsg.msg_iovlen * sizeof (aiov[0]),
			irix5_to_iovec, abi, xmsg.msg_iovlen);
	if (error)
		return error;
	xmsg.msg_iov = aiov;
	error = xmsg_to_msg(&xmsg, &msg);
	if (error)
		return error;
#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(abi)) {
		i5_uap = (struct irix5_xpg4_recvmsga *)(__psint_t)uap;
		flags = i5_uap->flags;
		VSOP_RECVIT(vso, &msg, &flags, 
		      &i5_uap->msg->msg_namelen,
		      &i5_uap->msg->msg_ctrllen, rvp, XPG4_SOCKETS, error);
		if (!error) {
			error = copyout(&flags, &i5_uap->msg->msg_flags,
				sizeof(i5_uap->msg->msg_flags));
		}
	}
	else
#endif	/* _ABI64 */
	{
		flags = uap->flags;
		VSOP_RECVIT(vso, &msg, &flags, 
		      &uap->msg->msg_namelen,
		      &uap->msg->msg_ctrllen, rvp, XPG4_SOCKETS, error);
		if (!error) {
			error = copyout(&flags, &uap->msg->msg_flags,
				sizeof(uap->msg->msg_flags));
		}
	}
	return(error);
}


/* ARGSUSED */
int
recvit(
	bhv_desc_t *vsi,
	register struct msghdr *mp,
	int *flags,
	void *namelenp, 
       	void *rightslenp,
	rval_t *rvp,
	int vers)
{
	__int64_t nl;
	int error;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	struct mbuf *from, *rights;
	int len;
	struct vsocket *vso = BHV_TO_VSOCK(vsi);

	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	auio.uio_pio = 0;
	auio.uio_pbuf = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		if (iov->iov_len > SSIZE_MAX)
			return EINVAL;
		if (iov->iov_len == 0)
			continue;
		auio.uio_resid += iov->iov_len;
	}
	len = auio.uio_resid;
	VSOP_RECEIVE(vso, &from, &auio, flags, &rights, error);
	if (auio.uio_resid != len && error == EINTR)
		error = 0;
	rvp->r_val1 = len - auio.uio_resid;
	if (mp->msg_name) {
		len = mp->msg_namelen;
		if (len <= 0 || from == 0)
			len = 0;
		else {
		        struct sockaddr *tmp;
			tmp = mtod(from, struct sockaddr *);
			if (len > from->m_len)
			        copyout((caddr_t)tmp, 
					mp->msg_name, from->m_len);
			else 
			        copyout((caddr_t)tmp, 
					mp->msg_name, len);
			/* For unix domain socket, make sure we have
			 * a null terminated string by keeping one more
			 * byte than the real length. So we subtract it
			 * here when we return the real length. -- lguo
			 */
			if (tmp->sa_family == AF_UNIX)
			  len = from->m_len - 1;
			else 
			  len = from->m_len;
		}
		if (vers == XPG4_SOCKETS) {
			if (ABI_IS_64BIT(get_current_abi())) {
				nl = (__int64_t)len;
				(void) copyout(&nl, namelenp, sizeof (__int64_t));
			} else {
				(void) copyout(&len, namelenp, sizeof (__int32_t));
			}
		} else {
			(void) copyout(&len, namelenp, sizeof (int));
		}
	}
	if (mp->msg_accrights) {
		len = mp->msg_accrightslen;
		if (len <= 0 || rights == 0)
			len = 0;
		else {
			if (len > rights->m_len)
				len = rights->m_len;

#if _MIPS_SIM == _ABI64
			/*
			 * hack for binary compat w/ 32-bit systems
			 */
			if (vso->vs_domain == AF_INET) {
				cmsghdr_to_irix5(rights);
				len = (len < rights->m_len) ? len :
					rights->m_len;
			}
#endif	/* _ABI64 */
			(void) copyout(mtod(rights, caddr_t),
			    mp->msg_accrights, len);
		}
		(void) copyout(&len, rightslenp, sizeof (int));
	}
	if (rights)
		m_freem(rights);
	if (from)
		m_freem(from);
	return error;
}

struct shutdowna {
	sysarg_t	s;
	sysarg_t	how;
};

/* ARGSUSED */
int
shutdown(struct shutdowna *uap, rval_t *rvp)
{
	int error;
	struct vsocket *vso;

	if (error = getvsock(uap->s, &vso))
		return error;
	VSOP_SHUTDOWN(vso, uap->how, error);
	if (!error) {
		struct socket *so =
		(struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	    	_SAT_BSDIPC_SHUTDOWN(uap->s, so, uap->how, 0);
	}
	return error;
}

struct setsockopta {
	sysarg_t	s;
	sysarg_t	level;
	sysarg_t	name;
	caddr_t		val;
	sysarg_t	valsize;
};

/* ARGSUSED */
int
setsockopt(struct setsockopta *uap, rval_t *rvp)
{
	int error;
	struct vsocket	*vso;
	struct mbuf *m = NULL;

	if (error = getvsock(uap->s, &vso))
	    return (error);
	if (uap->valsize > MLEN)
		return EINVAL;
	if (uap->val) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (m == NULL)
			return ENOBUFS;
		error = copyin(uap->val, mtod(m, caddr_t), uap->valsize);
		if (error) {
			(void) m_free(m);
			return error;
		}
		m->m_len = uap->valsize;
	}
	VSOP_SETATTR(vso, uap->level, uap->name, m, error);
	return (error);
}

struct getsockopta {
	sysarg_t	s;
	sysarg_t	level;
	sysarg_t	name;
	caddr_t		val;
	sysarg_t	*avalsize;
};

/* ARGSUSED */
int
getsockopt(struct getsockopta *uap, rval_t *rvp)
{
	int error;
	struct vsocket	*vso;
	struct mbuf *m = NULL;
	int valsize;

	if (error = getvsock(uap->s, &vso))
	    return error;
	if (uap->val) {
		error = copyin(uap->avalsize, &valsize, sizeof (valsize));
		if (error)
			return error;
	} else
		valsize = 0;
	VSOP_GETATTR(vso, uap->level, uap->name, &m, error);
	if (error)
		goto bad;
	if (uap->val && valsize && m != NULL) {
		if (valsize > m->m_len)
			valsize = m->m_len;
		error = copyout(mtod(m, caddr_t), uap->val, valsize);
		if (error)
			goto bad;
		error = copyout(&valsize, uap->avalsize, sizeof (valsize));
	}
bad:
	if (m != NULL)
		(void) m_free(m);
	return error;
}

/*
 * Get socket name.
 */
struct getsocknamea {
	sysarg_t	fdes;
	caddr_t		asa;
	sysarg_t	*alen;
};

/* ARGSUSED */
int
getsockname(struct getsocknamea *uap, rval_t *rvp)
{
	int error;
	int len;
	struct vsocket	*vso;

	if (error = getvsock(uap->fdes, &vso)) {
	    return (error);
	}
	error = copyin(uap->alen, &len, sizeof (len));
	if (error)
		return error;
	VSOP_GETSOCKNAME(vso, uap->asa, uap->alen, len, rvp, error);
	return error;
}

/*
 * Get name of peer for connected socket.
 */
struct getpeernamea {
	sysarg_t	fdes;
	caddr_t		asa;
	sysarg_t	*alen;
};

/* ARGSUSED */
int
getpeername(struct getpeernamea *uap, rval_t *rvp)
{
	int error;
	struct vsocket *vso;

	if (error = getvsock(uap->fdes, &vso))
		return error;
	VSOP_GETPEERNAME(vso, uap->asa, uap->alen, rvp, error);
	return (error);
}

int
sockargs(struct mbuf **aname, void *name, int namelen, int type)
{
	register struct mbuf *m;
	int error;

	if (((u_int)namelen > MLEN) || (namelen == 0)) {
		return EINVAL;
	}
	/* Ensure that internalized access rights can still fit in 1 mbuf.
	   In 64-bit mode, a pointer and an int have different sizes. */
	if (type == MT_RIGHTS && 
		(u_int)namelen / sizeof(int) * sizeof(struct vfile *) > MLEN) {
		return EINVAL;
	}
	/* always return a valid pointer */
	m = m_get(M_WAIT, type);
	m->m_len = namelen;
	error = copyin(name, mtod(m, caddr_t), namelen);
	if (error)
		(void) m_free(m);
	else
		*aname = m;
	return (error);
}

/*
 * This function is exported to nfs_server.c
 */
int
getvsock(int fd, struct vsocket **sop)
{
	vfile_t *vfp;
	int error;

	if (error = getf(fd, &vfp))
		return error;
	if (VF_IS_VSOCK(vfp))
		*sop = VF_TO_VSOCK(vfp);
	else
		return ENOTSOCK;
	return 0;
}

/*
 * hack to make S5 copyin/out look like bsd
 * XXX remove these and their evil macros in socketvar.h when the SVR4.1
 *     DDI/DKI copyin and copyout are done
 */
#undef copyin
int					/* return a good errno */
bsd_copyin(void *from, void *to, int len)
{
	return (copyin(from,to,len) ? EFAULT : 0);
}

#undef copyout
int					/* return a good errno */
bsd_copyout(void *from, void *to, int len)
{
	return (copyout(from,to,len) ? EFAULT : 0);
}

#if _MIPS_SIM == _ABI64
struct irix5_cmsghdr {
	app32_int_t	cmsg_len;
	app32_int_t	cmsg_level;
	app32_int_t	cmsg_type;
};

int
irix5_to_cmsghdr(struct mbuf *m)
{
	struct irix5_cmsghdr *i5_mh;
	struct cmsghdr *mh;
	char buf[64];	/* XXX */
	int len;
	caddr_t d, s;

	if (ABI_IS_IRIX5_64(get_current_abi())) {
		return 0;
	}

	i5_mh = mtod(m, struct irix5_cmsghdr *);

	len = i5_mh->cmsg_len;
	len += (sizeof(*mh) - sizeof(*i5_mh));

	if (len > sizeof(buf)) {
		return 0;
	}

	mh = (struct cmsghdr *)buf;

	mh->cmsg_len = len;
	mh->cmsg_level = i5_mh->cmsg_level;
	mh->cmsg_type = i5_mh->cmsg_type;

	d = (caddr_t)CMSG_DATA(mh);
	s = (caddr_t)(i5_mh + 1);

	/* copy remaining data */
	bcopy(s, d, len - sizeof(*mh));

	m->m_len = len;

	bcopy(buf, mtod(m, caddr_t), len);

	return 1;
}

int
cmsghdr_to_irix5(struct mbuf *m)
{
	struct irix5_cmsghdr *i5_mh;
	struct cmsghdr *mh;
	int len;
	caddr_t d, s;
	char buf[64];	/* XXX */

	if (ABI_IS_IRIX5_64(get_current_abi())) {
		return 0;
	}

	i5_mh = (struct irix5_cmsghdr *)buf;
	mh = mtod(m, struct cmsghdr *);

	len = mh->cmsg_len;
	len -= (sizeof(*mh) - sizeof(*i5_mh));

	if (len > sizeof(buf)) {
		return 0;
	}

	i5_mh->cmsg_len = len;
	i5_mh->cmsg_level = mh->cmsg_level;
	i5_mh->cmsg_type = mh->cmsg_type;

	d = (caddr_t)(i5_mh + 1);
	s = (caddr_t)(mh + 1);

	/* copy remaining data */
	bcopy(s, d, len - sizeof(*i5_mh));

	m->m_len = len;

	bcopy(buf, mtod(m, caddr_t), len);

	return 1;
}
#endif	/* _ABI64 */

/*
 * Make a new open file for a vsocket.
 */
int
makevsock(struct vfile **fpp, int *fdp)
{
	return vfile_alloc(FREAD|FWRITE|FSOCKET, fpp, fdp);
}
