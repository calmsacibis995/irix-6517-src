/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident "$Revision: 1.17 $"

/*
 *	tpitcp
 *		provide TPI interface to the sockets-based
 *		TCP protocol stack
 *
 *	tpitcp uses tpisocket for most of its functionality.
 */

#include "bstring.h"
#include "tpisocket.h"
#include "bsd/net/route.h"
#include "bsd/netinet/in_systm.h"
#include "bsd/netinet/in.h"
#include "bsd/netinet/ip.h"
#include "bsd/netinet/in_pcb.h"
#include "bsd/netinet/tcp.h"
#include "bsd/netinet/tcp_timer.h"
#include "bsd/netinet/ip_var.h"
#include "bsd/netinet/tcp_var.h"
#include "bsd/netinet/tcp_fsm.h"
#include <bsd/tcp-param.h>	/* for CLBYTES, needed for mbuf.h */
#include "sys/systm.h"


extern u_int tcp_sendspace;
extern u_int tcp_recvspace;
extern int tpitcp_nummajor;
extern int tpitcp_majors[];
extern int tpitcp_maxdevices;
extern int ip_pcbopts(struct mbuf **, struct mbuf *);
extern int ip_getmoptions(int, struct mbuf *, struct mbuf **);
extern int ip_setmoptions(int, struct mbuf **, struct mbuf *);

static int tpitcp_open( 	queue_t *, dev_t *, int, int, cred_t *);
static int tpitcp_address_size( struct tpisocket *, int, struct socket *);
static int tpitcp_convert_address( struct tpisocket *, char *, int, int, 
				struct mbuf *, struct socket *);
static int tpitcp_function( 	struct tpisocket *, int, void *);
static int tpitcp_optmgmt( 	struct tpisocket *, struct T_optmgmt_req *, 
				struct opthdr *, mblk_t *);
int tpitcp_ip_optmgmt( 		struct tpisocket *, struct T_optmgmt_req *, 
				struct opthdr *, mblk_t *);

/* controller stream stuff
 */
static struct module_info tpitcpm_info = {
	STRID_TPITCP,			/* module ID */
	"TPITCP",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size */
	10,				/* hi-water mark */
	0,				/* lo-water mark */
};

static struct qinit tpitcp_rinit = {
	NULL, tpisocket_rsrv, tpitcp_open,
	tpisocket_close, NULL, &tpitcpm_info, NULL
};

static struct qinit tpitcp_winit = {
	tpisocket_wput, tpisocket_wsrv, NULL,
	NULL, NULL, &tpitcpm_info, NULL
};

int tpitcpdevflag = 0;
struct streamtab tpitcpinfo = {&tpitcp_rinit, &tpitcp_winit, NULL, NULL};

static struct tpisocket **tpitcp_devices = NULL;

struct tpisocket_control tpitcp_control = {
	&tpitcp_nummajor,
	tpitcp_majors,
	&tpitcp_devices,
	&tpitcp_maxdevices,
	AF_INET,
	SOCK_STREAM,
	IPPROTO_TCP,
	tpitcp_address_size,
	tpitcp_convert_address,
	TSCF_NO_SEAMS,		/* tsc_flags */
	T_COTS_ORD,	/* tsc_transport_type */
	0,	 	/* tsc_tsdu_size (byte stream) */
	-1,		/* tsc_etsdu_size */
	-2,		/* tsc_cdata_size */
	-2,		/* tsc_ddata_size */
	0x10000,	/* tsc_tidu_size */
	EXPINLINE|SENDZERO,	/* tsc_provider_flag */
	tpitcp_function	/* tsc_function */
};


void
tpitcpinit( void )
{
	tpisocket_register(&tpitcp_control);
}


/*ARGSUSED*/
static int
tpitcp_open(
	register queue_t *rq,
	dev_t	*devp,
	int	flag,
	int	sflag,
	cred_t *crp)
{
	return tpisocket_open(rq, devp, flag, sflag, crp, &tpitcp_control);
}

/*ARGSUSED*/
static int
tpitcp_address_size(
	struct	tpisocket *tpiso,
	int	code,
	struct	socket *nso)
{
	switch (code) {
	case TSCA_LOCAL_ADDRESS:
	case TSCA_REMOTE_ADDRESS:
		return sizeof(struct sockaddr_in);
	case TSCA_OPTIONS:
		return(1024); /* SVR4 value */
	case TSCA_ACTUAL_OPTIONS:
		if (tpiso->tso_flags & TSOF_XTI) {
			int t;
#if 0
			/* See comments about XTI below XXX */
			/* 3 opthdrs + sizeof 32-bit uint + char (+ align) */
			t = (3 * sizeof(struct t_opthdr)) + 
				sizeof(xtiscalar_t) + sizeof(xtiuscalar_t);
#else
			/* 2 opthdrs + char */
			t = ((2 * sizeof(struct t_opthdr)) + 
				sizeof(xtiscalar_t));
#endif
			if (tpiso->tso_saved_conn_opts) {
				t += msgdsize(tpiso->tso_saved_conn_opts);
			}
			return t;
		} else {
			return(4);
		}
	default:
		return 0;
	}
}


/*ARGSUSED*/
static int
tpitcp_convert_address(
	struct	tpisocket *tpiso,
	char   	*ptr,
	int	mode,
	int	size,
	struct	mbuf *m,
	struct	socket *nso)	/* nso means socket already locked! */
{
	struct	vsocket *vso;
	struct	socket *so;
	struct	tcpcb *tp;
	struct	inpcb *inp;
	int	s;

	if (mode == (TSCA_MODE_READ | TSCA_OPTIONS)) {
		u_short maxseg;
		vso = (nso != NULL ? NULL : tpiso->tso_vsocket);
		so = (nso != NULL ? nso : (struct socket *)
			(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso))));
		s = splnet();
		if (!nso)
			SOCKET_LOCK(so);
		inp = sotoinpcb(so);
		if (inp == 0) {
			splx(s);
			if (!nso)
				SOCKET_UNLOCK(so);
			return EINVAL;
		}
		tp = intotcpcb(inp);
		/* probably not needed, safety */
		if (tp == 0) {
			splx(s);
			if (!nso)
				SOCKET_UNLOCK(so);
			return EINVAL;
		}
		maxseg = tp->t_maxseg;
		splx(s);
		if (!nso)
			SOCKET_UNLOCK(so);
		
		if (tpiso->tso_flags & TSOF_XTI) {
			struct t_opthdr *top;
			char *p2 = ptr;
			/*
			 * XXX
			 * In XTI mode, we currently do not return
			 * MAXSEG because of stupidity in the XTI tests.
			 * They check for "privileged" options in connection
			 * confirmations, and also have the annoying habit
			 * of taking options received on a connection
			 * indication and giving them back with the connection
			 * response; this causes all sorts of things to fail
			 * with TACCESS, and generally makes life painful.
			 */
			 /*
			  * We allocate a scalar to hold the IP TOS (even
			  * though it is only one byte) in order to ensure
			  * alignment of subsequent options.
			  */
#if 0
			if (size < ((3 * sizeof(*top)) + 
				sizeof(xtiscalar_t) + sizeof(xtiuscalar_t))) {
				return EINVAL;
			}
#else
			if (size < ((2 * sizeof(*top)) + 
				sizeof(xtiscalar_t))) {
				return EINVAL;
			}
#endif
			bzero(ptr, size);
			top = (struct t_opthdr *)ptr;
#if 0
			top->len = sizeof(*top) + sizeof(xtiuscalar_t);
			top->level = INET_TCP;
			top->name = TCP_MAXSEG;
			top->status = 0;
			p2 = (char *)(top + 1);
			*((xtiuscalar_t *)p2) = (xtiuscalar_t)maxseg;
			top = OPT_NEXTHDR(ptr, size, top);
			if (top == 0) {
				return EINVAL;
			}
#endif
			top->len = sizeof(*top);
			top->level = INET_IP;
			top->name = IP_OPTIONS;
			top->status = 0;
			top = OPT_NEXTHDR(ptr, size, top);
			if (top == 0) {
				return EINVAL;
			}
			top->len = sizeof(*top) + 1;
			top->level = INET_IP;
			top->name = IP_TOS;
			top->status = 0;
			p2 = (char *)(top + 1);
			*p2 = 0;			/* XXX */
			/*
			 * If any options were saved during T_CONN_REQ
			 * processing, stick them in now
			 */
			if (tpiso->tso_saved_conn_opts) {
				mblk_t *bp = tpiso->tso_saved_conn_opts;
				p2 = (char *)OPT_NEXTHDR(ptr, size, top);
				while (bp) {
					int len = bp->b_wptr - bp->b_rptr;
					bcopy(bp->b_rptr, p2, len);
					p2 += len;
					bp = bp->b_cont;
				}
				freemsg(tpiso->tso_saved_conn_opts);
				tpiso->tso_saved_conn_opts = 0;
			}
			return 0;
		} else {
			if (size < 4)
				return EINVAL;
			bzero(ptr, size);
			ptr[0] = 2; /* segment size code */
			ptr[1] = 4; /* length */
			ptr[2] = maxseg >> 8;
			ptr[3] = maxseg;
			return 0;
		}
	}
	return tpisocket_convert_address(tpiso, ptr, mode, size, m, nso);
}


static struct opproc tpitcp_functions[] = {
		SOL_SOCKET,	tpisocket_optmgmt,
		IPPROTO_TCP,	tpitcp_optmgmt,
		IPPROTO_IP,	tpitcp_ip_optmgmt,
		0,		0
	};

static int
tpitcp_function(
	struct tpisocket *tpiso,
	int	code,
	void   *arg)
{
	struct vsocket *vso = tpiso->tso_vsocket;
	struct socket *so =
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	struct  inpcb *ip;
	struct	tcpcb *tp;
	/*	NETSPL_SPDECL(s)*/

	switch (code) {
	case TSCF_UNBIND:
		SOCKET_LOCK(so);
		ip = sotoinpcb(so);
		if (ip == NULL) {
			SOCKET_UNLOCK(so);
			return 0;
		}
		tp = intotcpcb(ip);
		/* probably not needed, safety */
		if (tp == 0) {
			SOCKET_UNLOCK(so);
			return 0;
		}
		if (tp->t_state > TCPS_LISTEN) {
			SOCKET_UNLOCK(so);
			return EINVAL;
		}
		tp->t_state = TCPS_CLOSED;
		in_pcbunlink(ip);		/* resets hashval */
		bzero(&ip->inp_laddr, sizeof(ip->inp_laddr));
		ip->inp_lport = 0;
		SOCKET_UNLOCK(so);
		break;

	case TSCF_OPTMGMT:
		return tpisocket_do_optmgmt(tpiso, (int *)arg, tpitcp_functions);

	case TSCF_DEFAULT_VALUE:
		switch ((__psint_t) arg) {
		case TSCD_SNDBUF:
			return tcp_sendspace;
		case TSCD_RCVBUF:
			return tcp_recvspace;
		case TSCD_SNDLOWAT:
			return MCLBYTES;
		case TSCD_RCVLOWAT:
			return 1;
		default:
			return -1;
		}
		
	default:
		break;
	}
	return 0;
}



static int
tpitcp_optmgmt(
	struct	tpisocket *tpiso,
	struct T_optmgmt_req *req,
	struct opthdr  *opt,
	mblk_t         *mp)
{
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so =
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	struct  inpcb *ip = sotoinpcb(so);
	struct	tcpcb *tp;
	int	val;
	int	val2;
	/*	NETSPL_SPDECL(s)*/

	SOCKET_LOCK(so);
	ip = sotoinpcb(so);
	if (ip == NULL) {
		SOCKET_UNLOCK(so);
		return -TOUTSTATE;
	}
	tp = intotcpcb(ip);
	/* probably not needed, safety */
	if (tp == 0) {
		SOCKET_UNLOCK(so);
		return -TOUTSTATE;
	}

	switch (req->MGMT_flags) {

	case T_NEGOTIATE:
		switch (opt->name) {
		case TCP_NODELAY:
			if (opt->len != OPTLEN(sizeof(int))) {
				SOCKET_UNLOCK(so);
				return -TBADOPT;
			}
			if (*(int *)OPTVAL(opt))
				tp->t_flags |= TF_NODELAY;
			else
				tp->t_flags &= ~TF_NODELAY;
			break;

		case TCP_MAXSEG:	/* not yet */
			SOCKET_UNLOCK(so);
			return -TACCES;

		default:
			SOCKET_UNLOCK(so);
			return -TBADOPT;
		}

		/* fall through to retrieve value */

	case T_CHECK:
		switch (opt->name) {
		case TCP_NODELAY:
		case TCP_MAXSEG:
			switch (opt->name) {
			case TCP_NODELAY:
				val = (tp->t_flags & TF_NODELAY) != 0;
				break;

			case TCP_MAXSEG:
				val = tp->t_maxseg;
				break;
			}
			SOCKET_UNLOCK(so);
			if (! tpisocket_makeopt(mp, IPPROTO_TCP,
	 				       opt->name, &val,
					       sizeof(int)))
				return ENOSR;
			break;

		default:
			req->MGMT_flags = T_FAILURE;
			break;
		}
		break;

	case T_DEFAULT:
		val = 0;
		val2 = tp->t_maxseg;
		SOCKET_UNLOCK(so);
		if (! tpisocket_makeopt(mp, IPPROTO_TCP, TCP_NODELAY,
				       &val, sizeof(int)))
			return ENOSR;
		if (! tpisocket_makeopt(mp, IPPROTO_TCP, TCP_MAXSEG, 
					&val2, sizeof(int)))
			return ENOSR;
		break;
	default:
		SOCKET_UNLOCK(so);
		break;
	}

	return 0;

}


int
tpitcp_ip_optmgmt(
	struct	tpisocket *tpiso,
	struct T_optmgmt_req *req,
	struct opthdr  *opt,
	mblk_t         *mp)
{
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so =
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	struct  inpcb *ip = sotoinpcb(so);
	int	error;
	struct	mbuf *m;

	ip = sotoinpcb(so);
	if (ip == NULL)
		return -TOUTSTATE;

	switch (req->MGMT_flags) {

	case T_NEGOTIATE:
		m = tpiso->tso_pending_optmgmt_mbuf;
		if (m == NULL) {
			MGET(m, M_DONTWAIT, MT_SOOPTS);
			if (m == NULL)
				return ENOSR;
		} else
			tpiso->tso_pending_optmgmt_mbuf = NULL;
		m->m_len = 0;
		if (opt->len > M_TRAILINGSPACE(m)) {
			m_freem(m);
			return -TBADOPT;
		}
		m->m_len = opt->len;
		bcopy(OPTVAL(opt), mtod(m, caddr_t), opt->len);

		switch (opt->name) {
		case IP_OPTIONS:

			error = ip_pcbopts(&ip->inp_options, m);
			/*
			 * don't free m, since ip_pcbopts either held it
			 * or freed it.
			 */
			if (error) {
				if (error == EINVAL)
					error = -TBADOPT;
				return error;
			}
			break;
#ifdef sgi
		case IP_TOS:
		case IP_TTL:
			if (m->m_len != sizeof(int))
				error = EINVAL;
			else {
				int optval = *mtod(m, int *);
				switch (opt->name) {

				case IP_TOS:
					ip->inp_ip_tos = optval;
					break;

				case IP_TTL:
					ip->inp_ip_ttl = optval;
					break;
				}
			}
			m_freem(m);
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_setmoptions(opt->name, &ip->inp_moptions, m);
			m_freem(m);
			break;
#endif

		default:
			m_freem(m);
			return -TBADOPT;
		}

		/* fall through to retrieve value */

	case T_CHECK:
		switch (opt->name) {
		case IP_OPTIONS:
			if (ip->inp_options) {
				if (!tpisocket_makeopt(mp, IPPROTO_IP, 
					 IP_OPTIONS,
					 mtod(ip->inp_options, caddr_t), 
					 ip->inp_options->m_len))
					return ENOSR;
				if (!tpisocket_makeopt(mp, IPPROTO_IP, 
						IP_OPTIONS, (char *)0, 0))
					return ENOSR;
				break;
			}
#ifdef sgi
		case IP_TOS:
			if (!tpisocket_makeopt(mp, IPPROTO_IP, IP_TOS,
					& ip->inp_ip_tos,
					sizeof ip->inp_ip_tos))
				return ENOSR;
			break;

		case IP_TTL:
			if (!tpisocket_makeopt(mp, IPPROTO_IP, IP_TTL,
					& ip->inp_ip_ttl,
					sizeof ip->inp_ip_ttl))
				return ENOSR;
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
			MGET(m, M_DONTWAIT, MT_SOOPTS);
			if (m == NULL)
				return ENOSR;
			m->m_len = 0;
			error = ip_getmoptions(opt->name, ip->inp_moptions, &m);
			if (!error &&
			    !tpisocket_makeopt(mp, IPPROTO_IP, opt->name, 
					mtod(m, char *), m->m_len)) {
				error = ENOSR;
			}
			m_freem(m);
			return error;

		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			if (! tpisocket_makeopt(mp, IPPROTO_IP, opt->name, 
					(char *)0, 0))
				return ENOSR;
			break;

#endif
		default:
			req->MGMT_flags = T_FAILURE;
			break;
		}
		break;

	case T_DEFAULT:
		if (! tpisocket_makeopt(mp, IPPROTO_IP, IP_OPTIONS, 
				(char *)0, 0))
			return ENOSR;
		break;
	}

	return 0;
}
