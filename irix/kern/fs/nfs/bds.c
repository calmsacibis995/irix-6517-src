/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * bds.c - client side, kernel level bulk data protocol.
 * $Revision: 1.33 $
 *
 * Entry points in this file are single threaded for a particular rnode.
 * In V3, the locking is on the r_statelock; in V2, all I/O is single 
 * threaded per rnode on the rw_lock.
 *
 * Errors cause the socket to be destroyed.  The rnode is set up such that
 * the socket will be recreated on the next I/O.  We don't bother to open
 * the BDS socket in nfs_open(); we wait until there is an I/O request.
 *
 * rp->r_bds being set implies that there is a file currently open on this
 * connection. if bds_open(rp) returns error, either rp->r_bds is NULL, or
 * bds_open() just tried a reopen due to new ioflags. A failed reopen does
 * not close the file.
 */

#include "types.h"
#include <sys/debug.h>
#include <sys/systm.h> 
#include <sys/fcntl.h>
#include <sys/uio.h> 
#include <sys/cred.h> 
#include <sys/errno.h> 
#include <sys/vfs.h> 
#include <sys/vnode.h>
#include <ksys/vfile.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/vsocket.h>
#include <sys/socketvar.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/in_pcb.h>
#include "nfs.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "bds.h"
#include <sys/atomic_ops.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_fsops.h>

/*
 * Try to get this much socket buffer space.
 */
#define SOCKBUF		(1024 << 10)

#define	SAVE_UIO 	{ int i; \
			uio = *uiop; \
			for (i = 0; i < uiop->uio_iovcnt; ++i) \
				io[i] = uiop->uio_iov[i]; \
			}
#define RESTORE_UIO 	{ int i; \
			*uiop = uio; \
			for (i = 0; i < uio.uio_iovcnt; ++i) \
			    	uio.uio_iov[i] = io[i]; \
			}
#define RP_IS_WBEH(rp)	(rp->r_bds_flags & RBDS_WBEHIND)
#define RP_IS_DIRTY(rp)	(rp->r_bds_flags & RBDS_DIRTY)
#define RP_IS_SPECD(rp)	(rp->r_bds_flags & RBDS_SPECIFIED)

#define	RETRY(e)	(e == ETIMEDOUT || e == ENETDOWN || e == ENETUNREACH ||\
		 e == ECONNREFUSED || e == ECONNABORTED || e == EAGAIN ||      \
		 e == ENETRESET || e == ECONNRESET || e == ENONET ||           \
		 e == ESHUTDOWN || e == EHOSTDOWN || e == EHOSTUNREACH ||      \
		 e == EPIPE || e == ENOTCONN)

#define vtos(vso) ((struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso)))))
#if	defined(DEBUG)
#define	BDS_DEBUG
#endif

#ifdef	BDS_DEBUG
#define curepid() (private.p_curuthread->ut_flid.fl_pid)
#define	debug(x)	{ printf("p=%d ", curepid()); printf x; }
#define	iferror(x)	{ if (error) { printf("p=%d ", curepid()); printf x; } }
#else
#define	debug(x)
#define	iferror(x)
#endif

/* private to this file */
static int	bds_uiomove(uio_rw_t rw, struct rnode *rp, struct uio *uiop,
			    int eintr);
static int	send_msg(struct rnode *rp, int ioflag, int eintr,
			 bds_msg *req, void *optr, size_t olen,
			 bds_msg *rep, void *iptr, size_t *ilenp);
static int	bds_connect(struct vsocket **vs, mntinfo_t *mi);
static void	bds_msg_init(bds_msg *m, struct rnode *rp, struct cred *cred);

/* externs */
extern int	bindresvport(struct vsocket *so);
struct sginapa	{ long ticks; };
extern int	sginap(struct sginapa *uap, rval_t *rvp);

/*
 * NOTE: bds_msg_init() may be called before we have ever
 * connected to a BDS server, meaning rp->r_bds_* fields may
 * all be newly initialized to 0.
 */

static void
bds_msg_init(bds_msg *m, struct rnode *rp, struct cred *cred)
{
	static	unsigned long	xid = 0;
	u_long	myxid;
	
	bzero(m, sizeof(*m));
	myxid = atomicAddUlong(&xid, 1);
	if (myxid == 0)
		myxid = atomicAddUlong(&xid, 1);
	m->bds_xid = flip64(myxid);
	m->bds_uid = flip64(cred->cr_uid);
	m->bds_gid = flip64(cred->cr_gid);
	if (RP_IS_SPECD(rp))
		m->bds_priority = rp->r_bds_priority;
	else
		m->bds_priority = BDSATTR_PRIORITY_DONTCARE;
	m->bds_priority = flip64(m->bds_priority);
	return;
}

bds_open(struct rnode *rp, int ioflag, struct cred *cred)
{
	int	timeo = 1, error, reopen = 0, i, s;
	mntinfo_t *mi = rtomi(rp);
	struct	vsocket *vs;
	bds_msg m;
	uint64	oflags;
	struct	_BDS_config cfg;

	if (RP_IS_SPECD(rp))
		oflags = rp->r_bds_oflags;
	else {
		ioflag &= BDS_IOFLAG_MASK;
		oflags = BDS_OPEN_UNALIGNED;
		if (ioflag & IO_BULK)
			oflags |= BDS_OPEN_WRITEBEHIND;
	}
	
	if (rp->r_bds) {
		debug(("bds_open (reopen)\n"));

		if (rp->r_bds_vers == 0)
			return 0;

		if (oflags == rp->r_bds_oflags &&
		    !(rp->r_bds_flags & RBDS_NEED_REOPEN))
			return 0;

		reopen = 1;
		goto bds_reopen; /* change in semantics? */
	}

bds_open_retry:
	debug(("bds_open\n"));
	if (!(mi->mi_flags & MIF_BDS)) {
		return (ENOPROTOOPT);
	}

	vs = NULL;
	s = splock(mi->mi_bds_vslock);
	i = mi->mi_bds_nextvs;
	do {
		if (mi->mi_bds_vslist[i]) {
			vs = mi->mi_bds_vslist[i];
			mi->mi_bds_vslist[i] = NULL;
			break;
		}
		if (++i == BDS_SOCKETS)
			i = 0;
	} while (i != mi->mi_bds_nextvs);
	spunlock(mi->mi_bds_vslock, s);
	
	while ((vs == NULL) && (error = bds_connect(&vs, mi))) {
		struct	sginapa	ticks;
		rval_t	r;

		debug(("bds_connect failed because %d\n", error));
		ASSERT(vs == NULL);

		if (!RETRY(error))
			return error;

		printf("BDS server %s not responding, retrying in %d seconds\n",
		       mi->mi_hostname, timeo);
		if (timeo < 16) timeo <<= 1;
		ticks.ticks = timeo * HZ;
		r.r_val1 = 0;
		sginap(&ticks, &r);
		if (r.r_val1)
			return EINTR;
	}

	/*
	 * If we got here, we have a connected socket - save it.
	 */
	rp->r_bds = vs;
	rp->r_bds_flags &= RBDS_SPECIFIED;

bds_reopen:
	/*
	 * {re}open the file on the remote machine.
	 */
	
	bds_msg_init(&m, rp, cred);
	ASSERT(mi->mi_bdsbuflen == 0 || mi->mi_bdsbuflen >= 4096);
	if (RP_IS_SPECD(rp) &&
	    (rp->r_bds_buflen != BDSATTR_BUFSIZE_DONTCARE))
	{
		m.bds_buflen = flip64(rp->r_bds_buflen);
	} else if (~mi->mi_bdsbuflen == 0) {
		m.bds_buflen = 0;
		m.bds_buflen = ~(m.bds_buflen);	/* set to ~0 */
	} else
		m.bds_buflen = flip64(mi->mi_bdsbuflen);
	
	if (reopen) {
		m.bds_cmd     = flip64(BDS_REOPEN);
		m.bds_pathlen = 0;
		m.bds_oflags  = flip64(oflags);
		error = send_msg(rp, -1, 0, &m, NULL, 0, &m, NULL, NULL);
		
	} else if (rp->r_flags & RV3) {
		uint64	pathlen = sizeof(rp->r_ufh.r_fh3);
		size_t sz = sizeof(cfg);
		
		m.bds_cmd     = flip64(BDS_FH3OPEN);
		m.bds_pathlen = flip64(pathlen);
		m.bds_oflags  = flip64(oflags | BDS_OPEN_GETCONFIG);
		error = send_msg(rp, -1, 0,
				 &m, &rp->r_ufh.r_fh3, pathlen,
				 &m, &cfg, &sz);

	} else {
		uint64	pathlen = sizeof(rp->r_fh);
		size_t sz = sizeof(cfg);
		
		m.bds_cmd     = flip64(BDS_FH2OPEN);
		m.bds_pathlen = flip64(pathlen);
		m.bds_oflags  = flip64(oflags | BDS_OPEN_GETCONFIG);
		error = send_msg(rp, -1, 0,
				 &m, &rp->r_fh, pathlen,
				 &m, &cfg, &sz);

	}
	if (error) {
bds_open_goterr:
		debug(("bds_open: send_msg said %d\n", error));
		ASSERT(!rp->r_bds);

		/*
		 * do not retry if error != EAGAIN, or if reopening
		 * when server is buffering writes, as data may be lost.
		 */

		if ((error != EAGAIN) || (reopen && RP_IS_DIRTY(rp)))
		{
			if (error != EINTR) {
				printf("connection to BDS server %s lost\n",
				       rtomi(rp)->mi_hostname);
			}
			return error;
		}

		reopen = 0;
		goto bds_open_retry;
	}

	/*
	 * Cool - we made it.
	 */
	if (timeo > 1) {
		printf("BDS server %s OK\n", mi->mi_hostname);
	}
	if (m.bds_cmd == BDS_NAK) {
		if (!reopen)
			bds_close(rp, NULL);
		return (ntoh_errno(m.bds_error));
	}
	if (reopen)
		rp->r_bds_flags &= ~RBDS_NEED_REOPEN;
	
	if (oflags & BDS_OPEN_WRITEBEHIND)
		rp->r_bds_flags |= RBDS_WBEHIND;

	rp->r_bds_vers   = flip64(m.bds_ack1);
	if (!RP_IS_SPECD(rp)) {
		rp->r_bds_oflags = oflags;
		if (!reopen) {
			rp->r_bds_priority = BDSATTR_PRIORITY_DONTCARE;
			rp->r_bds_buflen = BDSATTR_BUFSIZE_DONTCARE;
		}
	}
	/*
	 * Hack to detect BDS 1.x servers. They don't clear the ack/nak
	 * structures before sending reply, so fields that didn't exist
	 * then may have our data reflected back at us. bds_buffer and
	 * bds_ack1 overlap.
	 *
	 * Note: bds_buffer is forced to be at least 4096 so that it can
	 * never be confused for a BDS protocol version, which is what
	 * is returned to us in bds_ack1.
	 */

	if (rp->r_bds_vers >= 4096)
		rp->r_bds_vers = 0;

	if (!reopen && rp->r_bds_vers >= 1) {
		int cleanup;
		
		cfg.cfg_bytes = flip64(cfg.cfg_bytes);
		
		if ((((char *)&cfg.cfg_maxiosz)-((char *)&cfg)) < cfg.cfg_bytes)
			mi->mi_bds_maxiosz = flip64(cfg.cfg_maxiosz);

		if ((((char *)&cfg.cfg_blksize)-((char *)&cfg)) < cfg.cfg_bytes) {
			if (cfg.cfg_blksize)
				mi->mi_bds_blksize = flip64(cfg.cfg_blksize);
			else
				debug(("bds_open: got a 0 cfg_blksize\n"));
		}
		/* if there are unread bytes, read them. ewwww! */

		cleanup = cfg.cfg_bytes - sizeof(cfg);
		while (cleanup > 0) {
			char buf[256];
			size_t thistime;

			thistime = cleanup > 256 ? 256 : cleanup;
			error = send_msg(rp, -1, 0,
					 NULL, NULL, 0,
					 NULL, buf, &thistime);
			if (error)
				goto bds_open_goterr;
			cleanup -= thistime;
		}
	}

	debug(("bds_open: open successful. BDS protocol version %d\n",
	       (int)rp->r_bds_vers));
	return (0);
}

int
bds_connect(struct vsocket **vsp, mntinfo_t *mi)
{
	struct sockaddr_in sin;
	struct vsocket *vs;
	struct socket *so;
	int error;
	/*REFERENCED*/
	int error2;
	struct mbuf *nam;

	*vsp = NULL;

	if (error = vsocreate(AF_INET, &vs, SOCK_STREAM, IPPROTO_TCP)) {
		iferror(("bds_connect: vsockcreate failed\n"));
		return (error);
	}

	if (error = bindresvport(vs)) {
		iferror(("bds_connect: bindresvport failed\n"));
		VSOP_CLOSE(vs, 1, 0, error2);
		return (error);
	}
	debug(("bds bound to port %d\n", sotoinpcb(vtos(vs))->inp_iap.iap_lport));

	/*
	 * Set no delay, the recv & send windows.
	 */
	nam = m_get(M_WAIT, MT_SOOPTS);
	if (!nam) {
nonam: 		
		debug(("bds_connect: m_get failed\n"));
		VSOP_CLOSE(vs, 1, 0, error);
		return (ENOBUFS);
	}
	*mtod(nam, int*) = 1;
	nam->m_len = sizeof(int);
	VSOP_SETATTR(vs, IPPROTO_TCP, TCP_NODELAY, nam, error);
	iferror(("bds TCP_NODELAY failed\n"));
	if (!(nam = m_get(M_WAIT, MT_SOOPTS))) {
		goto nonam;
	}
	nam->m_len = sizeof(int);
	*mtod(nam, int*) = mi->mi_bdswindow ? mi->mi_bdswindow : SOCKBUF;
	debug(("bds_connect: setting SO_RCV/SNDBUF to %d\n", *mtod(nam, int*)));
	VSOP_SETATTR(vs, SOL_SOCKET, SO_RCVBUF, nam, error);
	iferror(("bds SO_RCVBUF failed\n"));
	if (!(nam = m_get(M_WAIT, MT_SOOPTS))) {
		goto nonam;
	}
	nam->m_len = sizeof(int);
	*mtod(nam, int*) = mi->mi_bdswindow ? mi->mi_bdswindow : SOCKBUF;
	VSOP_SETATTR(vs, SOL_SOCKET, SO_SNDBUF, nam, error);
	iferror(("bds SO_SNDBUF failed\n"));
	if (!(nam = m_get(M_WAIT, MT_SOOPTS))) {
		goto nonam;
	}
	nam->m_len = sizeof(int);
	*mtod(nam, int*) = 1;
	VSOP_SETATTR(vs, SOL_SOCKET, SO_KEEPALIVE, nam, error);
	iferror(("bds SO_KEEPALIVE failed\n"));

	/*
	 * Connect it.
	 * XXX - can't use the 
	 * error conditions.  This is a connect interface problem.
	 */
	so = vtos(vs);
	sin = mi->mi_addr;
	sin.sin_port = htons(BDS_PORT);

	/*
	 * Temporarily cribbed from uipc_connect because the connect() interface
	 * wants to do a copyin.
	 */
	SOCKET_LOCK(so);
	debug(("bds connect(%x) port=%d\n",
	    vs, sotoinpcb(so)->inp_iap.iap_lport));
	ASSERT(0 == (so->so_state & SS_NBIO)); /* removed NBIO code */

	if (!(nam = m_get(M_WAIT, MT_SONAME))) {
		SOCKET_UNLOCK(so);
		goto nonam;
	}
	*mtod(nam, struct sockaddr_in*) = sin;
	nam->m_len = sizeof(sin);

	error = soconnect(so, nam);
	m_freem(nam);
	if (error)
		goto bad;
	ASSERT(0 == (so->so_state & SS_NBIO)); /* removed NBIO code */
	
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		int rv;

		SOCKET_MPUNLOCK_SOTIMEO(so, (PZERO+1), rv);
		SOCKET_LOCK(so);
		if (rv) {
			error = EINTR;
			goto bad;
		}
	}
	error = so->so_error;
	so->so_error = 0;

bad:
	so->so_state &= ~SS_ISCONNECTING;
	SOCKET_UNLOCK(so);
	if (error) {
		VSOP_CLOSE(vs, 1, 0, error2);
	} else
		*vsp = vs;
	iferror(("bds_connect: error=%d\n", error));
	return error;
}

/*
 * if CRED is not NULL, we will not actually close the socket, but
 * rather just send a close request to the BDS server. This will
 * allow rp->r_bds to be reused. This may, if there's an I/O error,
 * result in us being recursively called to simply close the socket.
 * Calls from within bds.c should pass a NULL credentials pointer,
 */

bds_close(struct rnode *rp, struct cred *cred)
{
	int	error = 0, error2;
	mntinfo_t *mi = rtomi(rp);

	debug(("bds_close(r_bds=%x)\n", rp->r_bds));
	if (rp->r_bds == NULL)
		return 0;

	if (cred && (mi->mi_flags & MIF_BDS) && rp->r_bds_vers) {
		bds_msg m;

		bds_msg_init(&m, rp, cred);
		rp->r_bds_flags ^= (rp->r_bds_flags & RBDS_SPECIFIED);
		m.bds_cmd = flip64(BDS_CLOSE);
		error = send_msg(rp, -1, 0, &m, NULL, 0, &m, NULL, NULL);
		if (error) {
			ASSERT(rp->r_bds == NULL);
			return error;
		}
		if (flip64(m.bds_cmd) == BDS_ACK) {
			struct vsocket *vs;
			int s;
			
			s = splock(mi->mi_bds_vslock);
			vs = mi->mi_bds_vslist[mi->mi_bds_nextvs];
			mi->mi_bds_vslist[mi->mi_bds_nextvs] =rp->r_bds;
			if (++mi->mi_bds_nextvs == BDS_SOCKETS)
				mi->mi_bds_nextvs = 0;
			spunlock(mi->mi_bds_vslock, s);
			
			rp->r_bds = NULL;
			if (vs)
				VSOP_CLOSE(vs, 1, 0, error2);
			return 0;
		}
		error = ntoh_errno(m.bds_error);

		/* got NAK -- fall through and close the socket */
	} else if (cred)
		rp->r_bds_flags ^= (rp->r_bds_flags & RBDS_SPECIFIED);

	if (rp->r_bds) {
		VSOP_CLOSE(rp->r_bds, 1, 0, error2);
		if (!error)
			error = error2;
		rp->r_bds = NULL;
	}
	iferror(("NFS/bds: failed to close BDS connection: %d\n", error));
	return (error);
}

void
bds_unmount(mntinfo_t *mi)
{
	int i;
	/*REFERENCED*/
	int error;

	debug(("bds_unmount\n"));
	
	for (i = 0; i < BDS_SOCKETS; i++) {
		if (mi->mi_bds_vslist[i]) {
			VSOP_CLOSE(mi->mi_bds_vslist[i], 1, 0, error);
			mi->mi_bds_vslist[i] = NULL;
		}
	}
	atomicClearUint(&mi->mi_flags, MIF_BDS);	/* anal, safe */
}	

long
bds_blksize(struct rnode *rp)
{
	mntinfo_t *mi = rtomi(rp);

	debug(("bds_blksize\n"));

	if (mi->mi_bds_blksize == 0 && rp->r_bds == NULL &&
	    mi->mi_rootvp && (rtov(rp) != mi->mi_rootvp))
	{
		bds_open(rp, 0, sys_cred);
		bds_close(rp, sys_cred);
	}
	return mi->mi_bds_blksize;
}

int
bds_vers(struct rnode *rp, int nfs_vers)
{
	debug(("bds_vers\n"));
	
	if (rp->r_bds == NULL) {
		if (nfs_vers > 2)
			mutex_enter(&rp->r_statelock);
		/* open bds connection to server */
		send_msg(rp, 0, 0, NULL, NULL, 0, NULL, NULL, NULL);
		if (nfs_vers > 2)
			mutex_exit(&rp->r_statelock);
	}
	if (rp->r_bds)
		return rp->r_bds_vers;
	return -1;
}

bds_write(struct rnode *rp, struct uio *uiop, int ioflag, struct cred *cred)
{
	int	error;
	uint64	wanted;
	struct	uio uio;
	struct	iovec io[16];	/* XXX - track rwv() */
	bds_msg	m;

	debug(("bds_write(off=%x len=%x)\n",
	       uiop->uio_offset, uiop->uio_resid));

	if (rp->r_bds_flags & RBDS_PEND_ERR) {
		debug(("bds_write returning pending error\n"));
		rp->r_bds_flags ^= RBDS_PEND_ERR;
		return ECONNRESET;
	}
	wanted = uiop->uio_resid;
	SAVE_UIO;

	while (1) {
		bds_msg_init(&m, rp, cred);
		m.bds_cmd    = flip64(BDS_WRITE);
		m.bds_offset = flip64(uiop->uio_offset);
		m.bds_length = flip64(wanted);
		
		error = send_msg(rp, ioflag, 3, &m, uiop, 0, &m, NULL, NULL);
		if (error == 0)
			break;
		else if (error != EAGAIN)
			return error;
		else if (RP_IS_DIRTY(rp))
			return ECONNRESET;
		
		RESTORE_UIO;
	}	

	if (flip64(m.bds_cmd) == BDS_NAK)
		return (ntoh_errno(m.bds_error));

	if (RP_IS_WBEH(rp) && !RP_IS_DIRTY(rp))
		rp->r_bds_flags |= RBDS_DIRTY;

	/*
	 * If we're appending to the file, we need to set rp->r_size to
	 * the new size if the file has grown, otherwise when the next
	 * write comes in, and we go to NFS to ask for the file size,
	 * it will not be updated from the last write.
	 */

	if (uiop->uio_offset > rp->r_size)
		rp->r_size = uiop->uio_offset;
	uiop->uio_resid = wanted - flip64(m.bds_bytes);
	return (0);
}

bds_read(struct rnode *rp, struct uio *uiop, int ioflag, struct cred *cred)
{
	uint64	moved = 0;
	int	error = 0;
	bds_msg	m;

	debug(("bds_read(off=%x len=%x)\n",
	    uiop->uio_offset, uiop->uio_resid));

	if (rp->r_bds_flags & RBDS_PEND_ERR) {
		debug(("bds_read returning pending error\n"));
		rp->r_bds_flags ^= RBDS_PEND_ERR;
		return ECONNRESET;
	}
	while (uiop->uio_resid) {
		bds_msg_init(&m, rp, cred);
		m.bds_cmd    = flip64(BDS_READ);
		m.bds_offset = flip64(uiop->uio_offset);
		m.bds_length = flip64(uiop->uio_resid);

		error = send_msg(rp, ioflag, 1, &m, NULL, 0, &m, uiop, NULL);

		if (error) {
			if (RP_IS_DIRTY(rp)) {
				if (moved) {
					rp->r_bds_flags |= RBDS_PEND_ERR;
					return 0;
				}
				return (error == EAGAIN) ? ECONNRESET : error;
			} 
			if (error != EAGAIN) {
				return error;
			}
			ASSERT(uiop->uio_resid);
			continue;
		}
		if (flip64(m.bds_cmd) == BDS_NAK)
			return ntoh_errno(m.bds_error);

		if (flip64(m.bds_bytes) == 0)
			break;
		
		moved += flip64(m.bds_bytes);
	}

	return (0);
}

bds_sync(struct rnode *rp, struct cred *cred)
{
	int	error;
	int	pend_err = rp->r_bds_flags & RBDS_PEND_ERR;
	bds_msg	m;
	
	if (!rp->r_bds)
		return 0;	/* hey! no problem! it syncs just fine! */
	
	debug(("bds_sync()\n"));

	bds_msg_init(&m, rp, cred);
	m.bds_cmd = flip64(BDS_SYNC);
	error = send_msg(rp, -1, 0, &m, NULL, 0, &m, NULL, NULL);

	if (pend_err)
		rp->r_bds_flags ^= RBDS_PEND_ERR;
	
	if (error == EINTR)
		return EINTR;
	else if (error)
		return ENOLINK;
	else if (pend_err)
		return ECONNRESET;

	if (flip64(m.bds_cmd) == BDS_NAK)
		return (ntoh_errno(m.bds_error));

	if (RP_IS_DIRTY(rp))
		rp->r_bds_flags &= ~RBDS_DIRTY;
	return (0);
}

bds_fcntl(struct rnode *rp, int cmd, void *arg, off_t offset, struct cred *cred)
{
	int error = 0, newcmd;
	bds_msg m;
	bdsattr_t *bap;
	flock_t *oflp;
	bds_flock64_t nfl;
	
	debug(("bds_fcntl(r_bds=%x, cmd=%d)\n", rp->r_bds, cmd));

	bds_msg_init(&m, rp, cred);
	m.bds_cmd = flip64(BDS_FCNTL);
	m.bds_u.bds_fcntl.offset = flip64(offset);
	
	switch (cmd) {
		/*
		 * When the NFS code calls us for ALLOC/FREE/RESV/UNRESV,
		 * it must have first done the copyin or COPYIN_XLATE and
		 * be passing us a flock_t pointer in arg that has directly
		 * readable values.
		 */

	case F_ALLOCSP:
		newcmd = BDSFCNTL_XFS_ALLOCSP;
		goto bds_fcntl_xfsalloc;

	case F_FREESP:
		newcmd = BDSFCNTL_XFS_FREESP;
		goto bds_fcntl_xfsalloc;
		
	case F_RESVSP:
		newcmd = BDSFCNTL_XFS_RESVSP;
		goto bds_fcntl_xfsalloc;

	case F_UNRESVSP:
		newcmd = BDSFCNTL_XFS_UNRESVSP;
		goto bds_fcntl_xfsalloc;
		
	case F_ALLOCSP64:
		newcmd = BDSFCNTL_XFS_ALLOCSP64;
 		goto bds_fcntl_xfsalloc;

	case F_FREESP64:	
		newcmd = BDSFCNTL_XFS_FREESP64;
		goto bds_fcntl_xfsalloc;
	
	case F_RESVSP64:
		newcmd = BDSFCNTL_XFS_RESVSP64;
		goto bds_fcntl_xfsalloc;

	case F_UNRESVSP64:
		newcmd = BDSFCNTL_XFS_UNRESVSP64;
		goto bds_fcntl_xfsalloc;

bds_fcntl_xfsalloc:
		m.bds_u.bds_fcntl.cmd = htonl(newcmd);
		m.bds_u.bds_fcntl.size = htonl(sizeof nfl);
		
		oflp = arg;
		nfl.l_type   = htons (oflp->l_type);
		nfl.l_whence = htons (oflp->l_whence);
		nfl.l_start  = flip64(oflp->l_start);
		nfl.l_len    = flip64(oflp->l_len);
		nfl.l_sysid  = flip64(oflp->l_sysid);
		nfl.l_pid    = flip64(oflp->l_pid);
		nfl.l_pad[0] = flip64(oflp->l_pad[0]);
		nfl.l_pad[1] = flip64(oflp->l_pad[1]);
		nfl.l_pad[2] = flip64(oflp->l_pad[2]);
		nfl.l_pad[3] = flip64(oflp->l_pad[3]);

		error = send_msg(rp, -1, 3,
				 &m, &nfl, sizeof(nfl),
				 &m, NULL, NULL);
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);
		break;

	case F_BDS_FSOPERATIONS + XFS_GROWFS_DATA:
	{
		xfs_growfs_data_t *grow = arg;

		m.bds_u.bds_fcntl.cmd = htonl(BDSFCNTL_XFS_FSOPERATIONS);
		m.bds_u.bds_fcntl.offset = flip64(XFS_GROWFS_DATA);
		m.bds_u.bds_fcntl.size = htonl(sizeof *grow);

		grow->newblocks = flip64(grow->newblocks);
		grow->imaxpct = htonl(grow->imaxpct);

		error = send_msg(rp, -1, 3,
				 &m, grow, sizeof(*grow),
				 &m, NULL, 0);
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GROWFS_LOG:
	{
		xfs_growfs_log_t *grow = arg;

		m.bds_u.bds_fcntl.cmd = htonl(BDSFCNTL_XFS_FSOPERATIONS);
		m.bds_u.bds_fcntl.offset = flip64(XFS_GROWFS_LOG);
		m.bds_u.bds_fcntl.size = htonl(sizeof *grow);

		grow->newblocks = htonl(grow->newblocks);
		grow->isint = htonl(grow->isint);

		error = send_msg(rp, -1, 3,
				 &m, grow, sizeof(*grow),
				 &m, NULL, 0);
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GROWFS_RT:
	{
		xfs_growfs_rt_t *grow = arg;

		m.bds_u.bds_fcntl.cmd = htonl(BDSFCNTL_XFS_FSOPERATIONS);
		m.bds_u.bds_fcntl.offset = flip64(XFS_GROWFS_RT);
		m.bds_u.bds_fcntl.size = htonl(sizeof *grow);

		grow->newblocks = flip64(grow->newblocks);
		grow->extsize = htonl(grow->extsize);

		error = send_msg(rp, -1, 3,
				 &m, grow, sizeof(*grow),
				 &m, NULL, 0);
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_COUNTS:
	{
		xfs_fsop_counts_t *counts = arg;
		size_t sz = sizeof(*counts);

		m.bds_u.bds_fcntl.cmd = htonl(BDSFCNTL_XFS_FSOPERATIONS);
		m.bds_u.bds_fcntl.offset = flip64(XFS_FS_COUNTS);
		m.bds_u.bds_fcntl.size = 0;

		error = send_msg(rp, -1, 0,
				 &m, NULL, 0,
				 &m, counts, &sz);

		if (sz != sizeof(*counts))
			error = EINVAL;
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		if (error)
			break;

		counts->freedata = flip64(counts->freedata);
		counts->freertx  = flip64(counts->freertx);
		counts->freeino  = flip64(counts->freeino);
		counts->allocino = flip64(counts->allocino);

		break;		
	}
	/*
	 * SET_RESBLKS is an in *and* out function. The resblks
	 * field is used for the uint64 input value.
	 */
	case F_BDS_FSOPERATIONS + XFS_SET_RESBLKS:
	{
		xfs_fsops_getblks_t *getblks = arg;
		size_t szi = sizeof(getblks->resblks);
		size_t szo = sizeof(*getblks);

		m.bds_u.bds_fcntl.cmd = htonl(BDSFCNTL_XFS_FSOPERATIONS);
		m.bds_u.bds_fcntl.offset = flip64(XFS_SET_RESBLKS);
		m.bds_u.bds_fcntl.size = htonl(szi);

		getblks->resblks = flip64(getblks->resblks);

		error = send_msg(rp, -1, 3,
				 &m, &getblks->resblks, szi,
				 &m, getblks, &szo);

		if (szo != sizeof(*getblks))
			error = EINVAL;
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		if (error)
			break;

		getblks->resblks = flip64(getblks->resblks);
		getblks->resblks_avail = flip64(getblks->resblks_avail);

		break;
	}
	case F_BDS_FSOPERATIONS + XFS_GET_RESBLKS:
	{
		xfs_fsops_getblks_t *getblks = arg;
		size_t sz = sizeof(*getblks);

		m.bds_u.bds_fcntl.cmd = htonl(BDSFCNTL_XFS_FSOPERATIONS);
		m.bds_u.bds_fcntl.offset = flip64(XFS_GET_RESBLKS);
		m.bds_u.bds_fcntl.size = 0;

		error = send_msg(rp, -1, 0,
				 &m, NULL, 0,
				 &m, getblks, &sz);

		if (sz != sizeof(*getblks))
			error = EINVAL;
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		if (error)
			break;

		getblks->resblks = flip64(getblks->resblks);
		getblks->resblks_avail = flip64(getblks->resblks_avail);

		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY_V1:
	{
		xfs_fsop_geom_v1_t *geom = arg;
		size_t sz = sizeof(*geom);

		m.bds_u.bds_fcntl.cmd = BDSFCNTL_XFS_FSOPERATIONS;
		m.bds_u.bds_fcntl.offset = flip64(cmd - F_BDS_FSOPERATIONS);
		m.bds_u.bds_fcntl.size = 0;

		error = send_msg(rp, -1, 0,
				 &m, NULL, 0,
				 &m, geom, &sz);

		if (sz != sizeof(*geom))
			error = EINVAL;
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		if (error)
			break;

		geom->blocksize  = ntohl(geom->blocksize);
		geom->rtextsize  = ntohl(geom->rtextsize);
		geom->agblocks   = ntohl(geom->agblocks);
		geom->agcount    = ntohl(geom->agcount);
		geom->logblocks  = ntohl(geom->logblocks);
		geom->sectsize   = ntohl(geom->sectsize);
		geom->inodesize  = ntohl(geom->inodesize);
		geom->imaxpct    = ntohl(geom->imaxpct);
		geom->datablocks = flip64(geom->datablocks);
		geom->rtblocks   = flip64(geom->rtblocks);
		geom->rtextents  = flip64(geom->rtextents);
		geom->logstart   = flip64(geom->logstart);
		
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY_V2:
	{
		xfs_fsop_geom_v2_t *geom = arg;
		size_t sz = sizeof(*geom);

		m.bds_u.bds_fcntl.cmd = BDSFCNTL_XFS_FSOPERATIONS;
		m.bds_u.bds_fcntl.offset = flip64(cmd - F_BDS_FSOPERATIONS);
		m.bds_u.bds_fcntl.size = 0;

		error = send_msg(rp, -1, 0,
				 &m, NULL, 0,
				 &m, geom, &sz);

		if (sz != sizeof(*geom))
			error = EINVAL;
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		if (error)
			break;

		geom->blocksize  = ntohl(geom->blocksize);
		geom->rtextsize  = ntohl(geom->rtextsize);
		geom->agblocks   = ntohl(geom->agblocks);
		geom->agcount    = ntohl(geom->agcount);
		geom->logblocks  = ntohl(geom->logblocks);
		geom->sectsize   = ntohl(geom->sectsize);
		geom->inodesize  = ntohl(geom->inodesize);
		geom->imaxpct    = ntohl(geom->imaxpct);
		geom->datablocks = flip64(geom->datablocks);
		geom->rtblocks   = flip64(geom->rtblocks);
		geom->rtextents  = flip64(geom->rtextents);
		geom->logstart   = flip64(geom->logstart);
		geom->sunit      = ntohl(geom->sunit);
		geom->swidth     = ntohl(geom->swidth);
		
		break;
	}
	case F_BDS_FSOPERATIONS + XFS_FS_GEOMETRY:
	{
		xfs_fsop_geom_t *geom = arg;
		size_t sz = sizeof(*geom);

		m.bds_u.bds_fcntl.cmd = BDSFCNTL_XFS_FSOPERATIONS;
		m.bds_u.bds_fcntl.offset = flip64(XFS_FS_GEOMETRY);
		m.bds_u.bds_fcntl.size = 0;

		error = send_msg(rp, -1, 0,
				 &m, NULL, 0,
				 &m, geom, &sz);

		if (sz != sizeof(*geom))
			error = EINVAL;
		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);

		if (error)
			break;

		geom->blocksize    = ntohl(geom->blocksize);
		geom->rtextsize    = ntohl(geom->rtextsize);
		geom->agblocks     = ntohl(geom->agblocks);
		geom->agcount      = ntohl(geom->agcount);
		geom->logblocks    = ntohl(geom->logblocks);
		geom->sectsize     = ntohl(geom->sectsize);
		geom->inodesize    = ntohl(geom->inodesize);
		geom->imaxpct      = ntohl(geom->imaxpct);
		geom->datablocks   = flip64(geom->datablocks);
		geom->rtblocks     = flip64(geom->rtblocks);
		geom->rtextents    = flip64(geom->rtextents);
		geom->logstart     = flip64(geom->logstart);
		geom->sunit        = ntohl(geom->sunit);
		geom->swidth       = ntohl(geom->swidth);
		geom->version	   = ntohl(geom->version);
		geom->flags	   = ntohl(geom->flags);
		geom->logsectsize  = ntohl(geom->logsectsize);
		geom->rtsectsize   = ntohl(geom->rtsectsize);
		geom->dirblocksize = ntohl(geom->dirblocksize);
		
		break;
	}
	case F_FSGETXATTR:
		newcmd = BDSFCNTL_XFS_FSGETXATTR;
		goto bds_fcntl_getxattr;
		
	case F_FSGETXATTRA:
		newcmd = BDSFCNTL_XFS_FSGETXATTRA;
		goto bds_fcntl_getxattr;

bds_fcntl_getxattr:
	{
		struct fsxattr *fap = arg;
		size_t sz = sizeof(*fap);

		m.bds_u.bds_fcntl.cmd = htonl(newcmd);
		m.bds_u.bds_fcntl.size = 0;

		error = send_msg(rp, -1, 0,
				 &m, NULL, 0,
				 &m, fap, &sz);

		if (sz != sizeof(*fap))
			error = EINVAL;

		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);
		if (error)
			break;
	
		fap->fsx_xflags = ntohl(fap->fsx_xflags);
		fap->fsx_extsize = ntohl(fap->fsx_extsize);
		fap->fsx_nextents = ntohl(fap->fsx_nextents);

		break;
	}
	case F_FSSETXATTR:
	{
		struct fsxattr *fap = arg;

		m.bds_u.bds_fcntl.cmd = htonl(BDSFCNTL_XFS_FSSETXATTR);
		m.bds_u.bds_fcntl.size = htonl(sizeof *fap);

		fap->fsx_xflags = htonl(fap->fsx_xflags);
		fap->fsx_extsize = htonl(fap->fsx_extsize);
		fap->fsx_nextents = htonl(fap->fsx_nextents);

		error = send_msg(rp, -1, 3,
				 &m, fap, sizeof(*fap),
				 &m, NULL, 0);

		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);
		break;
	}
	case F_GETBDSATTR:
	{
		size_t sz = sizeof(*bap);

		m.bds_u.bds_fcntl.cmd = htonl(BDSFCNTL_BDS_GETBDSATTR);
		m.bds_u.bds_fcntl.size = 0;

		bap = arg;
		error = send_msg(rp, -1, 0, &m, NULL, 0, &m, bap, &sz);

		if (!error && flip64(m.bds_cmd) == BDS_NAK)
			error = ntoh_errno(m.bds_error);
		if (error)
			break;
		
		if (((char *)bap) + sz > ((char *)&bap->bdsattr_flags)) {
			bap->bdsattr_flags = flip64(bap->bdsattr_flags);
			if (bap->bdsattr_flags & BDSATTRF_WRITEBEHIND)
				rp->r_bds_flags |= RBDS_WBEHIND;
			else
				rp->r_bds_flags &= ~RBDS_WBEHIND;
		}
		if (((char *)bap) + sz > ((char *)&bap->bdsattr_bufsize))
			bap->bdsattr_bufsize = flip64(bap->bdsattr_bufsize);
		
		if (((char *)bap) + sz > ((char *)&bap->bdsattr_priority))
			bap->bdsattr_priority = flip64(bap->bdsattr_priority);

		break;
	}
	case F_SETBDSATTR:
	{
		int old_oflags = rp->r_bds_oflags;
		
		rp->r_bds_flags |= RBDS_SPECIFIED;

		bap = arg;
		if (bap->bdsattr_bufsize != rp->r_bds_buflen) {
			rp->r_bds_buflen = bap->bdsattr_bufsize;
			rp->r_bds_flags |= RBDS_NEED_REOPEN;
		}
		rp->r_bds_priority = bap->bdsattr_priority;
		
		rp->r_bds_oflags = BDS_OPEN_UNALIGNED;
		if (bap->bdsattr_flags & BDSATTRF_BUFFERS_SINGLE)
			rp->r_bds_oflags |= BDS_OPEN_BUF_SINGLE;

		if (bap->bdsattr_flags & BDSATTRF_BUFFERS_RENEW)
			rp->r_bds_oflags |= BDS_OPEN_BUF_RENEW;

		if (bap->bdsattr_flags & BDSATTRF_BUFFERS_RENEWRR)
			rp->r_bds_oflags |= BDS_OPEN_BUF_RENEWRR;

		if (bap->bdsattr_flags & BDSATTRF_BUFFERS_KEEP)
			rp->r_bds_oflags |= BDS_OPEN_BUF_KEEP;

		if (bap->bdsattr_flags & BDSATTRF_WRITEBEHIND)
			rp->r_bds_oflags |= BDS_OPEN_WRITEBEHIND;
		
		if (bap->bdsattr_flags & BDSATTRF_BUFFERS_PREALLOC)
			rp->r_bds_oflags |= BDS_OPEN_BUF_PREALLOC;

		if (rp->r_bds_oflags != old_oflags)
			rp->r_bds_flags |= RBDS_NEED_REOPEN;

		if (rp->r_bds && (rp->r_bds_flags & RBDS_NEED_REOPEN))
			error = bds_open(rp, 0, cred); /* do reopen if needed */
		else
			error = 0;

		if (rp->r_bds_oflags & BDS_OPEN_BUF_PREALLOC)
			rp->r_bds_oflags ^= BDS_OPEN_BUF_PREALLOC;

		break;
	}
	default:
		error = EINVAL;
		break;
	}

	if (error == 0 && flip64(m.bds_cmd) == BDS_NAK)
		error = ntoh_errno(m.bds_error);
	
	return error;
}

static int
bds_uiomove(uio_rw_t rw, struct rnode *rp, struct uio *uiop, int eintr)
{
	int	error;
#ifdef BDS_DEBUG
	ssize_t	start_resid = uiop->uio_resid;
#endif
	debug(("bds uio%s %d\n",
	    rw == UIO_WRITE ? "write" : "read", uiop->uio_resid));
	ASSERT(rp->r_bds);

	for ( ;; ) {
		ssize_t resid;

		resid = uiop->uio_resid;
		
		if (rw == UIO_READ) {
			int vsop_flags = MSG_WAITALL;
			if (eintr)
				vsop_flags |= _MSG_NOINTR;
			debug(("bds_uiomove reading %d\n", (int)uiop->uio_resid));
			VSOP_RECEIVE(rp->r_bds, 0, uiop, &vsop_flags, 0, error);
		} else {
			debug(("bds_uiomove writing %d\n", (int)uiop->uio_resid));
			VSOP_SEND(rp->r_bds, 0, uiop,
				  eintr ? _MSG_NOINTR : 0, 0, error);
		}

		if (uiop->uio_resid == 0)
			return (0);

		if (vtos(rp->r_bds)->so_state & SS_CANTRCVMORE)
			error = EAGAIN;	/* close socket and reconnect */

		if (error) {
			debug(("bds_uiomove got error %d, moved %d bytes\n",
			       error, (int)(start_resid - uiop->uio_resid)));
			if (RETRY(error))
				error = EAGAIN;
			return (error);
		} else if (uiop->uio_resid == resid)
			return (EAGAIN);
	}
}

/*
 * send_msg:
 * rp,
 * ioflag, if -1 do not call bds_open(), trust that r_bds is valid
 *         else, always call bds_open() to open socket & check ioflag semantics
 * eintr > 0, do not allow EINTR to interrupt. legit values 0, 1, 2, 3
 * req, pointer to bds_msg to send
 * optr, olen, rep, iptr, ilen:
 *  if ptr is null, there is no data to be written/read other than bds_msg
 *  if len is 0, ptr is a uio, else just a char * with len bytes of data
 *  if uio, must adjust uio_resid
 *  if a char *, there's no way to return actual bytes transferred
 *  rep, pointer to bds_msg of reply
 *  read MIN(ilen, rep->bds_bytes) into iptr
 *
 * if send_msg returns error, it must have closed the socket
 * if send_msg receives a NAK, it may close the socket
 * the reply has bds_cmd set to 0 to indicate that no reply has been read.
 */

static int
send_msg(struct rnode *rp, int ioflag, int eintr,
	 bds_msg *req, void *optr, size_t olen,
	 bds_msg *rep, void *iptr, size_t *ilenp)
{
	int		error = 0;
	int		optr_error = 0;
	size_t		bytes = 0;
	struct	iovec	io;
	struct	uio	uio;
	struct	uio	*uiop;
	uint64		cmd = 0;
	uint64		xid;

	if (ioflag != -1) {
		struct vsocket *rbds = rp->r_bds;

		if (error = bds_open(rp, ioflag, sys_cred)) {
			return error;
		}
		/*
		 * On first connection to a server, we do a F_GETBDSATTR
		 * to determine if it has writebehind on by default.
		 */
		if (rp->r_bds && rp->r_bds_vers >= 2 && rbds != rp->r_bds) {
			bdsattr_t ba;

			if (error = bds_fcntl(rp, F_GETBDSATTR, &ba, 0, sys_cred))
				return error;
			if (ba.bdsattr_flags & BDSATTRF_WRITEBEHIND)
				rp->r_bds_flags |= RBDS_WBEHIND;
			else
				rp->r_bds_flags &= ~RBDS_WBEHIND;
		}
	}
	ASSERT(rp->r_bds);

	bzero(&uio, sizeof(uio));
	uio.uio_iov    = &io;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_iovcnt = 1;

	/* send req */

	if (req) {
		if (rp->r_bds_vers < 2)
			req->bds_priority = BDSATTR_PRIORITY_DONTCARE;

		io.iov_base    = req;
		io.iov_len     = sizeof(*req);
		uio.uio_resid  = sizeof(*req);
		uio.uio_limit  = sizeof(*req);
		uiop           = &uio;
		
		/* eintr doesn't matter until req is sent */
		
		error = bds_uiomove(UIO_WRITE, rp, uiop, 0);
		
		/*
		 * since req == rep is possible, don't trash request until sent
		 * above. no return() should be added above w/out first
		 * clearing rep->bds_cmd
		 */
		
		if (rep)
			rep->bds_cmd = 0;
		
		if (error)
			goto send_msg_failed;
	}

	if (optr) {
		if (olen) {
			io.iov_base   = optr;
			io.iov_len    = olen;
			uio.uio_resid = olen;
			uio.uio_limit = olen;
			uiop          = &uio;
		} else
			uiop          = optr;
		
		optr_error = bds_uiomove(UIO_WRITE, rp, uiop, eintr > 2);
		uiop->uio_sigpipe = 0;
	}

	if (rep) {
		xid           = req->bds_xid;
		io.iov_base   = rep;
		io.iov_len    = sizeof(*rep);
		uio.uio_resid = sizeof(*rep);
		uio.uio_limit = sizeof(*rep);
		uiop          = &uio;
		
		error = bds_uiomove(UIO_READ, rp, uiop, eintr > 1);
		if (error) {
			if (optr_error)
				error = optr_error;
			goto send_msg_failed;
		}
		cmd = flip64(rep->bds_cmd);
		if ((rep->bds_xid != xid) || (cmd != BDS_ACK && cmd != BDS_NAK)) {
			debug(("BDS: cmd=%d,%d xid=%d,%d EPROTO\n",
			       cmd, BDS_ACK,
			       (int)flip64(rep->bds_xid), (int)flip64(xid)));
			if (optr_error)
				error = optr_error;
			else
				error = EPROTO;
			goto send_msg_failed;
		}
		if (cmd == BDS_NAK) {
			debug(("bds got NAK %d\n", ntoh_errno(rep->bds_error)));
		} else
			bytes = flip64(rep->bds_bytes);
	}

	if (optr_error) {
		if (cmd == BDS_NAK) {
			bds_close(rp, NULL);
			return 0;
		} else {
			error = optr_error;
			goto send_msg_failed;
		}
	}
	
	if (iptr && (bytes || !rep)) {
		int adjust = 0;
		size_t ilen = ilenp ? *ilenp : 0;
		
		if (ilen) {
			if (rep && bytes < ilen)
				*ilenp = ilen = bytes;
			
			io.iov_base    = iptr;
			io.iov_len     = ilen;
			uio.uio_resid  = ilen;
			uio.uio_limit  = ilen;
			uiop           = &uio;
		} else {
			uiop           = iptr;
			if (rep && bytes < uiop->uio_resid) {
				adjust = uiop->uio_resid - bytes;
				uiop->uio_resid = bytes;
			}
			ilen           = uiop->uio_resid;
		}

		error = bds_uiomove(UIO_READ, rp, uiop, eintr > 0);
		uiop->uio_resid += adjust;
		if (error)
			goto send_msg_failed;
		if (bytes)
			bytes -= ilen;
		
		/* read off any excess bytes */
		
		if (bytes) {
			char foo[128];
			
			debug(("BDS send_msg clearing %d bytes\n", bytes));
			io.iov_base = foo;
			while (bytes) {
				int curbytes = (bytes < 128) ? bytes : 128;
				
				uio.uio_resid = uio.uio_limit = curbytes;
				io.iov_len = curbytes;
				bds_uiomove(UIO_READ, rp, uiop, 1);
				bytes -= curbytes;
			}
		}
	}
	
	return 0;

send_msg_failed:
	bds_close(rp, NULL);
	return error;
}


uint64
hton_errno(int error)
{
	switch (error) {
	    case EPERM:		return (flip64(1));
	    case ENOENT:	return (flip64(2));
	    case ESRCH:		return (flip64(3));
	    case EINTR:		return (flip64(4));
	    case EIO:		return (flip64(5));
	    case ENXIO:		return (flip64(6));
	    case E2BIG:		return (flip64(7));
	    case ENOEXEC:	return (flip64(8));
	    case EBADF:		return (flip64(9));
	    case ECHILD:	return (flip64(10));
	    case EAGAIN:	return (flip64(11));
	    case ENOMEM:	return (flip64(12));
	    case EACCES:	return (flip64(13));
	    case EFAULT:	return (flip64(14));
	    case ENOTBLK:	return (flip64(15));
	    case EBUSY:		return (flip64(16));
	    case EEXIST:	return (flip64(17));
	    case EXDEV:		return (flip64(18));
	    case ENODEV:	return (flip64(19));
	    case ENOTDIR:	return (flip64(20));
	    case EISDIR:	return (flip64(21));
	    case EINVAL:	return (flip64(22));
	    case ENFILE:	return (flip64(23));
	    case EMFILE:	return (flip64(24));
	    case ENOTTY:	return (flip64(25));
	    case ETXTBSY:	return (flip64(26));
	    case EFBIG:		return (flip64(27));
	    case ENOSPC:	return (flip64(28));
	    case ESPIPE:	return (flip64(29));
	    case EROFS:		return (flip64(30));
	    case EMLINK:	return (flip64(31));
	    case EPIPE:		return (flip64(32));
	    case EOPNOTSUPP:	return (flip64(33));
	    case ESTALE:	return (flip64(34));
	    case EPROTO:	return (flip64(35));
	    default:		return (flip64(5));	/* EIO */
	}
}

int
ntoh_errno(uint64 error)
{
	switch (flip64(error)) {
	    case 1: return (EPERM);
	    case 2: return (ENOENT);
	    case 3: return (ESRCH);
	    case 4: return (EINTR);
	    case 5: return (EIO);
	    case 6: return (ENXIO);
	    case 7: return (E2BIG);
	    case 8: return (ENOEXEC);
	    case 9: return (EBADF);
	    case 10: return (ECHILD);
	    case 11: return (EAGAIN);
	    case 12: return (ENOMEM);
	    case 13: return (EACCES);
	    case 14: return (EFAULT);
	    case 15: return (ENOTBLK);
	    case 16: return (EBUSY);
	    case 17: return (EEXIST);
	    case 18: return (EXDEV);
	    case 19: return (ENODEV);
	    case 20: return (ENOTDIR);
	    case 21: return (EISDIR);
	    case 22: return (EINVAL);
	    case 23: return (ENFILE);
	    case 24: return (EMFILE);
	    case 25: return (ENOTTY);
	    case 26: return (ETXTBSY);
	    case 27: return (EFBIG);
	    case 28: return (ENOSPC);
	    case 29: return (ESPIPE);
	    case 30: return (EROFS);
	    case 31: return (EMLINK);
	    case 32: return (EPIPE);
	    case 33: return (EOPNOTSUPP);
	    case 34: return (ESTALE);
	    case 35: return (EPROTO);
	    default: return (EIO);
	}
}
