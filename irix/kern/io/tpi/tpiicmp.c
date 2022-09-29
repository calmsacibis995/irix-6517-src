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
#ident	"$Revision: 1.13 $"

/*
 *	tpiicmp
 *		provide TPI interface to the sockets-based
 *		ICMP protocol stack
 *
 *	tpiicmp uses tpisocket for most of its functionality.
 */

#include "bstring.h"
#include "tpisocket.h"
#include "sys/param.h"
#include "sys/tcp-param.h"
#include "net/route.h"
#include "net/raw_cb.h"
#include "netinet/in_systm.h"
#include "netinet/in.h"
#include "netinet/ip.h"
#include "netinet/in_pcb.h"
#include "netinet/udp.h"
#include "sys/systm.h"

extern int tpiicmp_nummajor;
extern int tpiicmp_majors[];
extern int tpiicmp_maxdevices;

extern int ip_pcbopts(struct mbuf **, struct mbuf *);

static int tpiicmp_open( queue_t *, dev_t *, int, int, cred_t *);
static int tpiicmp_ip_optmgmt(struct tpisocket *, struct T_optmgmt_req *, 
				struct opthdr *, mblk_t *);
int tpiicmp_function(struct tpisocket *, int, void *);
int tpiicmp_convert_address(struct tpisocket *, caddr_t, int, int, 
				struct mbuf *, struct socket *);
void tpiicmpinit(void);


static struct opproc tpiicmp_functions[] = {
		SOL_SOCKET, tpisocket_optmgmt,
		IPPROTO_IP, tpiicmp_ip_optmgmt,
		0, 0
	};

/* controller stream stuff
 */
static struct module_info tpiicmpm_info = {
	STRID_TPIICMP,			/* module ID */
	"TPIICMP",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size */
	10,				/* hi-water mark */
	0,				/* lo-water mark */
};

static struct qinit tpiicmp_rinit = {
	NULL, tpisocket_rsrv, tpiicmp_open,
	tpisocket_close, NULL, &tpiicmpm_info, NULL
};

static struct qinit tpiicmp_winit = {
	tpisocket_wput, tpisocket_wsrv, NULL,
	NULL, NULL, &tpiicmpm_info, NULL
};

int tpiicmpdevflag = 0;
struct streamtab tpiicmpinfo = {&tpiicmp_rinit, &tpiicmp_winit, NULL, NULL};

static struct tpisocket **tpiicmp_devices = NULL;

struct tpisocket_control tpiicmp_control = {
	&tpiicmp_nummajor,
	tpiicmp_majors,
	&tpiicmp_devices,
	&tpiicmp_maxdevices,
	AF_INET,
	SOCK_RAW,
	IPPROTO_ICMP,
	tpisocket_address_size,
	tpiicmp_convert_address,
	TSCF_DEFAULT_ADDRESS,    /* tsc_flags */
	T_CLTS,		/* tsc_transport_type */
	0x10000, 	/* tsc_tsdu_size */
	-2,		/* tsc_etsdu_size */
	-2,		/* tsc_cdata_size */
	-2,		/* tsc_ddata_size */
	0x10000,	/* tsc_tidu_size */
	0,		/* tsc_provider_flag */
	tpiicmp_function	/* tsc_function */
};


void
tpiicmpinit(void)
{
	tpisocket_register(&tpiicmp_control);
}


/*ARGSUSED*/
static int
tpiicmp_open(
	register queue_t *rq,
	dev_t	*devp,
	int	flag,
	int	sflag,
	cred_t	*crp)
{
	return tpisocket_open(rq, devp, flag, sflag, crp, &tpiicmp_control);
}


int	/* not static, also called from tpiicmp */
tpiicmp_function(
	struct tpisocket *tpiso,
	int	code,
	void *	arg)
{
	struct vsocket *vso = tpiso->tso_vsocket;
	struct socket *so =
	    (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	struct	rawcb *rp = sotorawcb(so);
	int	s;

	switch (code) {
	case TSCF_UNBIND:
		if (rp == NULL)
			return 0;
		s = splimp();
		if (rp->rcb_laddr != 0) {
			m_freem(rcbtom(rp->rcb_laddr));
			rp->rcb_laddr = 0;
		}
		if (rp->rcb_options != NULL) {
			m_freem(rp->rcb_options);
			rp->rcb_options = NULL;
		}
		if (rp->rcb_route.ro_rt)
			rtfree_needlock(rp->rcb_route.ro_rt);
		splx(s);
		break;

	case TSCF_OPTMGMT:
		return tpisocket_do_optmgmt(tpiso, (int *)arg,
					    tpiicmp_functions);

	case TSCF_DEFAULT_VALUE:
		switch ((__psint_t) arg) {
		case TSCD_SNDBUF:
			return 0;
		case TSCD_RCVBUF:
			return 0;
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
tpiicmp_ip_optmgmt(
	struct	tpisocket *tpiso,
	struct T_optmgmt_req *req,
	struct opthdr  *opt,
	mblk_t         *mp)
{
	struct	vsocket *vso = tpiso->tso_vsocket;
	struct	socket *so =
	    (struct socket *)(BHV_PDATA((VSOCK_TO_FIRST_BHV(vso))));
	struct  rawcb *rp;
	int	error;
	struct	mbuf *m;
	int	value;

	rp = sotorawcb(so);
	if (rp == NULL)
		return -TOUTSTATE;

	switch (req->MGMT_flags) {

	case T_NEGOTIATE:
		switch (opt->name) {
		case IP_OPTIONS:
			m = tpiso->tso_pending_optmgmt_mbuf;
			if (m == NULL) {
				MGET(m,M_DONTWAIT,MT_SOOPTS);
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
			bcopy(OPTVAL(opt),mtod(m,caddr_t),opt->len);
			error = ip_pcbopts(&rp->rcb_options,m);
			if (error) {
				if (error == EINVAL)
					error = -TBADOPT;
				return error;
			}
			break;

		case IP_HDRINCL:
			if (opt->len < sizeof(int))
				return -TBADOPT;		
			if (*((int *) OPTVAL(opt))) 
				rp->rcb_flags |= RAW_HDRINCL;
			else
				rp->rcb_flags &= ~RAW_HDRINCL;
			break;
			
		default:
			return -TBADOPT;
		}

		/* fall through to retrieve value */

	case T_CHECK:
		switch (opt->name) {
		case IP_OPTIONS:
			if (rp->rcb_options) {
				if (!tpisocket_makeopt(mp, IPPROTO_IP, IP_OPTIONS,
					 mtod(rp->rcb_options,caddr_t),
					 rp->rcb_options->m_len))
					return ENOSR;
				if (!tpisocket_makeopt(mp, IPPROTO_IP, IP_OPTIONS,
						     (char *) 0, 0))
					return ENOSR;
				break;
			}
			req->MGMT_flags = T_FAILURE;
			break;

		case IP_HDRINCL:
			value = (rp->rcb_flags & RAW_HDRINCL);
			if (! tpisocket_makeopt(mp, IPPROTO_IP, IP_HDRINCL,
					(caddr_t) &value,
					sizeof(value)))
				return ENOSR;
			break;

		default:
			req->MGMT_flags = T_FAILURE;
			break;
		}
		break;

	case T_DEFAULT:
		switch (opt->name) {
		case IP_OPTIONS:
			if (! tpisocket_makeopt(mp, IPPROTO_IP, IP_OPTIONS,
						(char *) 0, 0))
				return ENOSR;
			break;
		case IP_HDRINCL:
			value = 0;
			if (! tpisocket_makeopt(mp, IPPROTO_IP, IP_HDRINCL,
					(caddr_t) &value,
					sizeof(value)))
				return ENOSR;
			break;
		default:
			req->MGMT_flags = T_FAILURE;
			break;
		}
		break;
	default:
		return -TBADOPT;
	}

	return 0;
}

int
tpiicmp_convert_address(
	struct tpisocket *tpiso,
	caddr_t	ptr,
	int	mode,
	int	size,
	struct mbuf *m,
	struct socket *nso)
{
	struct sockaddr_in *sin;

	if (((mode & (TSCA_CODE|TSCA_MODE)) == 
			(TSCA_DEFAULT_ADDRESS|TSCA_MODE_TO_SOCKETS)) &&
	     (ptr == NULL) &&
	     (m != NULL)) {
		m->m_len = sizeof(struct sockaddr_in);
		sin = mtod(m, struct sockaddr_in *);
		bzero(sin, sizeof(struct sockaddr_in));
		sin->sin_family = AF_INET;
		return(0);
	}
	return(tpisocket_convert_address(tpiso, ptr, mode, size, m, nso));
}
