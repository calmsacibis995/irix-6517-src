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
#ident "$Revision: 1.16 $"

/*
 *	tpiudp
 *		provide TPI interface to the sockets-based
 *		UDP protocol stack
 *
 *	tpiudp uses tpisocket for most of its functionality.
 */

#include "bstring.h"
#include "tpisocket.h"
#include "bsd/net/route.h"
#include "bsd/netinet/in_systm.h"
#include "bsd/netinet/in.h"
#include "bsd/netinet/ip.h"
#include "bsd/netinet/in_pcb.h"
#include "bsd/netinet/udp.h"
#include "bsd/netinet/ip_var.h"
#include "bsd/netinet/udp_var.h"
#include <bsd/tcp-param.h>	/* for CLBYTES, needed for mbuf.h */
#include "sys/systm.h"

extern mblk_t *tpisocket_allocb( queue_t *, int, int);
extern int tpisocket_do_xtioptmgmt(struct tpisocket *,struct T_optmgmt_req *,
		mblk_t *, mblk_t *, int	*);
extern int tpitcp_ip_optmgmt();
extern int tpiudp_nummajor;
extern int tpiudp_majors[];
extern int tpiudp_maxdevices;
extern u_int udp_sendspace;
extern u_int udp_recvgrams;

static int tpiudp_open( queue_t *, dev_t *, int, int, cred_t *);
static int tpiudp_function( struct tpisocket *, int, void *);

static struct opproc tpiudp_functions[] = {
		SOL_SOCKET, tpisocket_optmgmt,
		IPPROTO_IP, tpitcp_ip_optmgmt,
		0, 0
	};

/* controller stream stuff
 */
static struct module_info tpiudpm_info = {
	STRID_TPIUDP,			/* module ID */
	"TPIUDP",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size */
	10,				/* hi-water mark */
	0,				/* lo-water mark */
};

static struct qinit tpiudp_rinit = {
	NULL, tpisocket_rsrv, tpiudp_open,
	tpisocket_close, NULL, &tpiudpm_info, NULL
};

static struct qinit tpiudp_winit = {
	tpisocket_wput, tpisocket_wsrv, NULL,
	NULL, NULL, &tpiudpm_info, NULL
};

static struct tpisocket **tpiudp_devices = NULL;

struct streamtab tpiudpinfo = {&tpiudp_rinit, &tpiudp_winit, NULL, NULL};
int tpiudpdevflag = 0;

struct tpisocket_control tpiudp_control = {
	&tpiudp_nummajor,
	tpiudp_majors,
	&tpiudp_devices,
	&tpiudp_maxdevices,
	AF_INET,
	SOCK_DGRAM,
	IPPROTO_UDP,
	tpisocket_address_size,
	tpisocket_convert_address,
	0,		/* tsc_flags */
	T_CLTS,		/* tsc_transport_type */
	0x10000, 	/* tsc_tsdu_size */
	-2,		/* tsc_etsdu_size */
	-2,		/* tsc_cdata_size */
	-2,		/* tsc_ddata_size */
	0x10000,	/* tsc_tidu_size */
	SENDZERO,	/* provider_flag */
	tpiudp_function	/* tsc_function */
};


void
tpiudpinit(void)
{
	tpisocket_register(&tpiudp_control);
}


/*ARGSUSED*/
static int
tpiudp_open(
	register queue_t *rq,
	dev_t	*devp,
	int	flag,
	int	sflag,
	cred_t *crp)
{
	return tpisocket_open(rq, devp, flag, sflag, crp, &tpiudp_control);
}


static int
tpiudp_function(
	struct tpisocket *tpiso,
	int	code,
	void   *arg)
{
	struct inpcb  *ip;
	/*	NETSPL_SPDECL(s)*/
	struct vsocket *vso = tpiso->tso_vsocket;
	struct socket *so =
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));

	switch (code) {
	case TSCF_UNBIND:
		SOCKET_LOCK(so);
		ip = sotoinpcb(so);
		if (ip == NULL) {
			SOCKET_UNLOCK(so);
			return 0;
		}
		in_pcbunlink(ip);		/* resets hashval */
		bzero(&ip->inp_laddr, sizeof(ip->inp_laddr));
		ip->inp_lport = 0;
		SOCKET_UNLOCK(so);
		break;

	case TSCF_OPTMGMT:
	{
		mblk_t	*bp;
		struct T_optmgmt_req *omr;

		bp = tpiso->tso_pending_optmgmt_req;
		if (bp == NULL ||
	    		(bp->b_wptr - bp->b_rptr) < 
					sizeof(struct T_optmgmt_req))
			return -TSYSERR;
		omr = (struct T_optmgmt_req *)bp->b_rptr;
		if ((omr->OPT_offset + omr->OPT_length) >
						(bp->b_wptr - bp->b_rptr))
			return -TBADOPT;

		if(omr->PRIM_type == _XTI_T_OPTMGMT_REQ) {
			mblk_t	*mp = NULL;
			int	error = 0;
			
			if(!(mp=tpisocket_allocb(tpiso->tso_wq,256,BPRI_MED)))
			{
				*((int *)arg) = TSCO_MORE;
				return 0;
			}
			mp->b_datap->db_type = M_PROTO;

			error = tpisocket_do_xtioptmgmt(tpiso, omr, bp, mp,
					(int *)arg);
			if (error && mp)
				freemsg(mp);
			return error;
		} else {
			return tpisocket_do_optmgmt(tpiso, (int *)arg, 
					tpiudp_functions);
		}
	}

	case TSCF_DEFAULT_VALUE:
		switch ((__psint_t)arg) {
		case TSCD_SNDBUF:
			return udp_sendspace;
		case TSCD_RCVBUF:
			return udp_recvgrams
				* (udp_sendspace+sizeof(struct sockaddr_in));
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
