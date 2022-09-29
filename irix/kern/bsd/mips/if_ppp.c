/* PPP network interface
 *
 * Copyright 1993 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.51 $"

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/strids.h>
#include <sys/stropts.h>
#include <sys/termio.h>
#include <sys/stty_ld.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/isdn_irix.h>
#include <bstring.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/raw.h>
#include <net/netisr.h>
#include <net/soioctl.h>
#include <net/route.h>
#include <netinet/in.h>
#ifndef PPP_IRIX_53
#include <sys/hashing.h>
#endif
#include "netinet/in_var.h"
#include <netinet/in_systm.h>		/* XXX required before ip.h */
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <ether.h>

#include <sys/if_ppp.h>
#include <net/slcompress.h>

#ifdef PPP_IRIX_53
#define in_ifaddr if_addrlist
#define _IFADDR_SA(d) (&((struct ifaddr*)(d))->ifa_addr)
#ifdef RNF_NORMAL
#define ROUTING_SOCKETS			/* pre-routing sockets */
#endif
#endif
#ifdef PPP_PATCH
/* unspeakable kludge to avoid patch release */
#include "net/slcompress.c"
#endif


#if !defined(PPP_IRIX_53) && !defined(PPP_IRIX_62) && !defined(PPP_IRIX_63) && !defined(PPP_IRIX_64)
extern struct ifqueue ipintrq;		/* IP packet input queue */
#endif

extern struct module_info ppp_fram_info;

/* discard a frame this often to stress things, especially compression */
int if_ppp_test, if_ppp_test_cnt;

/* turn on extra printfs */
int if_ppp_printf;


#define MINIP	sizeof(struct ip)	/* smallest normal IP packet */
#define MINIPCOMP 3			/* smallest compressed IP packet */

/* (snoop) header */
#define SNLEN (sizeof(struct ifheader)+sizeof(struct snoopheader))

/* bytes in front of CSLIP data.
 *	This value includes space for a TCP/IP header to be to be
 *	uncompressed in front of the TCP data.
 */
#define CSLIP_HDR (MAX_HDR+SNLEN)


static struct module_info stm_info = {
	STRID_PPP,			/* module ID */
	"if_ppp",			/* module name */
	PPP_BUF_MIN,			/* minimum buffer, with .index etc */
	PPP_BUF_MAX,			/* maximum packet size	*/
	2*PPP_DEF_MRU,			/* hi-water mark */
	0,				/* lo-water mark */
	0,
};

static void ppp_rput(queue_t*, mblk_t*);
static void pc_rsrv(queue_t*);
static void pd_rsrv(queue_t*);
static int ppp_open(queue_t*, dev_t*, int, int, struct cred*);
static ppp_close(queue_t*, int, struct cred*);
static void ppp_wput(queue_t*, mblk_t*);
static void pc_wsrv(queue_t*);
static void pd_wsrv(queue_t*);

static struct qinit ppp_rinit = {
	(int (*)(queue_t*,mblk_t*))ppp_rput,
	(int (*)(queue_t*))pc_rsrv, ppp_open, ppp_close, 0, &stm_info, 0
};
static struct qinit ppp_winit = {
	(int (*)(queue_t*,mblk_t*))ppp_wput,
	(int (*)(queue_t*))pc_wsrv, 0, 0, 0, &stm_info, 0
};
static struct qinit ppp_muxrinit = {
	(int (*)(queue_t*,mblk_t*))ppp_rput,
	(int (*)(queue_t*))pd_rsrv, 0, 0, 0, &stm_info, 0
};
static struct qinit ppp_muxwinit = {
	(int (*)(queue_t*,mblk_t*))ppp_wput,
	(int (*)(queue_t*))pd_wsrv, 0, 0, 0, &stm_info, 0
};

#ifndef STR_UNLOCK_SPL
#define STR_UNLOCK_SPL(s) splx(s)
#undef STR_LOCK_SPL
#define STR_LOCK_SPL(s) (s = splstr())
#endif
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63)
int if_pppdevflag = 0;
#else
int if_pppdevflag = D_MP;
#endif

struct streamtab if_pppinfo = {
	&ppp_rinit, &ppp_winit,
	&ppp_muxrinit, &ppp_muxwinit
};



/* the state of an individual device
 */
struct ppp_dev {
	struct ppp_dev	*pd_next;
	struct ppp_dev	*pd_prev;

	struct ppp_cont *pd_pc;

	queue_t		*pd_rq;
	queue_t		*pd_wq;		/* stream above the framer */

	time_t		pd_tx_lbolt;	/* timer for extra async frame-ends */

	__uint32_t	pd_tx_accm[PPP_ACCM_LEN];

	struct vj_uncomp *pd_vj_uncomp;	/* VJ header decompression data */

	struct bsd_db	*pd_tx_bsd;	/* output "BSD Compress" compression */
	struct pred_db	*pd_tx_pred;	/* output "Predictor" compression */

	struct ppp_pf	*pd_pf;		/* frame's info */

	u_int		pd_prev_raw_ibytes;
	u_int		pd_prev_raw_obytes;

	struct ppp_link pd_l;		/* info known in user-space */
} *ppp_pd_free = 0;



/* everything there is to know about each if_ppp interface, including all
 * of the devices it is using.
 */
struct ppp_cont {
	struct ifnet	pc_if;
	struct ppp_cont *pc_next;	/* chain of all structures */
	queue_t		*pc_rq;		/* our queues */
	queue_t		*pc_wq;

	mblk_t		*pc_mp_frags;	/* MP fragment reassembly queue */

	struct ppp_bundle pc_b;		/* known in user-space */

	struct mbuf	*pc_smbuf;	/* last small msg in output queue */

	struct afilter	*pc_afilter;	/* port activity filter */

	struct ppp_dev	*pc_pd_head;	/* chain of per-device info */
	struct ppp_dev	*pc_pd_cur;	/* recently used device */

	int		pc_tx_cp_buf_size;
	u_char		*pc_tx_cp_buf;	/* output compression buffer */

	struct vj_comp	*pc_vj_comp;	/* VJ header compression data */
	struct vj_uncomp *pc_vj_uncomp;
	struct pred_db	*pc_rx_pred;	/* "Predictor" compression */
	struct pred_db	*pc_tx_pred;
	struct bsd_db	*pc_rx_bsd;	/* UNIX compress compression */
	struct bsd_db	*pc_tx_bsd;

	struct rawif	pc_rawif;	/* for snooping */
} *ppp_pc_first = 0;
#define pc_mp	pc_b.pb_mp

static char *ppp_name = "ppp";

static int pc_num = 0;




static int ppp_ioctl(struct ifnet*, int, void*);

static mblk_t *ppp_oframe(struct ppp_cont*, struct ppp_dev*,
			  int, int, int, int*, struct mbuf**, u_char**);
static void ppp_mp_reasm(struct ppp_cont*);

#define KSALLOC(s,n) ((struct s*)kern_malloc(sizeof(struct s)*(n)))
#define KSFREE(s) {if ((s) != 0) {kern_free(s); (s) = 0;}}
#define CK_WPTR(bp) ASSERT((bp)->b_wptr <= (bp)->b_datap->db_lim)

#define NEED_BEEP(pcb) ((pcp->pc_b.pb_state & (PB_ST_BEEP|PB_ST_MP_TIME)) != 0)
#define RESTART_BEEP(pcp) {						\
	if (!(pcp->pc_b.pb_state & PB_ST_BEEP_TIME) && NEED_BEEP(pcp))	\
		ppp_beep(pcp);}						\


#define FFLAG_BUSY	(HZ*2)		/* extra frame-end this often */
#define FFLAG_IDLE	(HZ/20)


/* visit the muxes below to collect their raw input byte counts
 *	Things must be locked.
 */
static void
ppp_get_raw_bytes(struct ppp_cont *pcp)
{
	struct ppp_dev *pdp;
	u_int n, ibytes, obytes;

	ibytes = 0;
	obytes = 0;
	for (pdp = pcp->pc_pd_head; pdp != 0; pdp = pdp->pd_next) {
		n = pdp->pd_pf->pf_meters.raw_ibytes;
		ibytes += n - pdp->pd_prev_raw_ibytes;
		pdp->pd_prev_raw_ibytes = n;

		n = pdp->pd_l.pl_raw_obytes;
		obytes += n - pdp->pd_prev_raw_obytes;
		pdp->pd_prev_raw_obytes = n;
	}
	pcp->pc_b.pb_meters.raw_ibytes += ibytes;
	pcp->pc_b.pb_meters.raw_obytes += obytes;
}


/* poke the daemon
 */
static void
ppp_beep(struct ppp_cont *pcp)
{
	struct ppp_dev *pdp;
	struct ppp_buf *pb;
	mblk_t *bp;
	int prev_state;
	int s;

	STR_LOCK_SPL(s);

	prev_state = pcp->pc_b.pb_state;

	/* Do not beep if the link is dead, or if the only traffic
	 * was filtered as boring.
	 */
	if (!NEED_BEEP(pcp) || !pcp->pc_rq) {
		pcp->pc_b.pb_rx_ping = 0;;
		pcp->pc_b.pb_state &= ~(PB_ST_BEEP_TIME
					| PB_ST_BEEP | PB_ST_MP_TIME
					| PB_ST_TX_BUSY | PB_ST_TX_DONE);
		STR_UNLOCK_SPL(s);
		return;
	}

	pcp->pc_b.pb_state |= PB_ST_BEEP_TIME;	/* will leave with timer */
	(void)STREAMS_TIMEOUT(ppp_beep, pcp, BEEP);

	if (prev_state & PB_ST_BEEP) {
		/* Just postpone the work if upstream is too busy to be beeped.
		 */
		if (!canput(pcp->pc_rq->q_next)
		    || 0 == (bp = allocb(PPP_BUF_BEEP,BPRI_LO))) {
			STR_UNLOCK_SPL(s);
			return;
		}

		pb = (struct ppp_buf*)bp->b_wptr;
		bp->b_wptr += PPP_BUF_BEEP;
		bzero(pb, PPP_BUF_BEEP);
		pb->dev_index = -1;

		/* note active TCP connections
		 */
		if (pcp->pc_vj_comp != 0
		    && pcp->pc_vj_comp->actconn > 0) {
			pb->type = BEEP_ACTIVE;
		} else {
			pb->type = BEEP_WEAK;
		}

		/* All drivers must say that their inputs are busy for
		 * the bundle to be called saturated on their account.
		 */
		if (pcp->pc_b.pb_rx_ping != 0) {
			if (pcp->pc_b.pb_rx_ping >= (pcp->pc_b.pb_pl_num
						     *(BEEP
						       / PPP_RX_PING_TICKS)))
				pb->un.beep.st |= BEEP_ST_RX_BUSY;
			else
				pb->un.beep.st |= BEEP_ST_RX_ACT;
			pcp->pc_b.pb_rx_ping = 0;
		}

		/* Do not consider the link busy when the timer is
		 * first started.  There might not have been enough
		 * time to know if it has been intermittently idle
		 * and busy.
		 */
		if (prev_state & PB_ST_TX_BUSY) {
			if (!(prev_state & PB_ST_TX_DONE)
			    && (prev_state & PB_ST_BEEP_TIME))
				pb->un.beep.st |= BEEP_ST_TX_BUSY;
			else
				pb->un.beep.st |= BEEP_ST_TX_ACT;
		}

		ppp_get_raw_bytes(pcp);
		pb->un.beep.raw_ibytes = pcp->pc_b.pb_meters.raw_ibytes;
		pb->un.beep.raw_obytes = pcp->pc_b.pb_meters.raw_obytes;
		pb->un.beep.tstamp = lbolt;
		putnext(pcp->pc_rq, bp);
	}

	pcp->pc_b.pb_state &= ~(PB_ST_BEEP | PB_ST_MP_TIME
				| PB_ST_TX_BUSY | PB_ST_TX_DONE);

	/* If the modem is slow enough, then the timer will happen too
	 * fast and the link will appear to be idle.  The service
	 * function will not awaken before the timer expires.
	 * So if there is still waiting IP traffic, then fake it.
	 */
	if (pcp->pc_if.if_snd.ifq_len != 0)
		pcp->pc_b.pb_state |= (PB_ST_BEEP | PB_ST_TX_BUSY);


	/* Run MP flush timer
	 */
	if (prev_state & PB_ST_MP) {
		int i;

		if (pcp->pc_b.pb_pl_num == 1
		    && pcp->pc_mp.out_head > 0
		    && (prev_state & PB_ST_MP_OPT_HEAD))
			--pcp->pc_mp.out_head;

		/* advance FIFO of max sequence numbers */
		for (i = 0; i <= MP_FLUSH_LEN-2; i++)
			pcp->pc_mp.sn_flush[i] = pcp->pc_mp.sn_flush[i+1];

		/* Mark all links to generate null fragments.
		 * Put largest sequence number seen in head of FIFO.
		 */
		i = 0;
		for (pdp = pcp->pc_pd_head; pdp != 0; pdp = pdp->pd_next) {
			if ((prev_state & (PB_ST_NEED_NULLS | PB_ST_MP_NULLS))
			    == (PB_ST_NEED_NULLS | PB_ST_MP_NULLS))
				pdp->pd_l.pl_state |= PL_ST_NULL_NEED;
			if (i < pdp->pd_l.pl_mp_sn)
				i = pdp->pd_l.pl_mp_sn;
		}
		pcp->pc_mp.sn_flush[MP_FLUSH_LEN-1] = i;

		/* keep timer running for a while after the last fragment.
		 */
		if (i != pcp->pc_mp.sn_flush[0])
			pcp->pc_b.pb_state |= PB_ST_MP_TIME;

		if (pcp->pc_mp.sn_in < pcp->pc_mp.sn_flush[0]
		    && 0 != pcp->pc_mp.sn_flush[0])
			pcp->pc_mp.sn_in = pcp->pc_mp.sn_flush[0];

		/* awaken the output so it can generate null fragments
		 */
		if (prev_state & PB_ST_NEED_NULLS) {
			pcp->pc_b.pb_state &= ~PB_ST_NEED_NULLS;
			qenable(pcp->pc_wq);
		}

		/* flushing might have made it possible to assemble fragments.
		 */
		qenable(pcp->pc_rq);
	}

	STR_UNLOCK_SPL(s);
}


/* called through streams_interrupt to do an MP-safe qenable
 */
static void
ppp_wq_qenable(struct ppp_cont *pcp)
{
	int s;

	STR_LOCK_SPL(s);
	if (pcp->pc_wq != 0)
		qenable(pcp->pc_wq);
	STR_UNLOCK_SPL(s);
}


/* find the right device
 */
static struct ppp_dev*
find_pdp(struct ppp_cont *pcp,
	 int dev_index)
{
	struct ppp_dev *pdp;

	for (pdp = pcp->pc_pd_head; pdp; pdp = pdp->pd_next) {
		if (pdp->pd_l.pl_index == dev_index)
			return pdp;
	}
	return 0;
}



/* Turn off the socket interface
 *	the interface must already be locked.
 */
static void
ppp_down(struct ppp_cont *pcp)
{
	struct mbuf *m;

	pcp->pc_if.if_flags &= ~IFF_ALIVE;
	if_down(&pcp->pc_if);
	for (;;) {
		IF_DEQUEUE_NOLOCK(&pcp->pc_if.if_snd, m);
		if (!m)
			break;
		m_freem(m);
	}
}


/* process an ioctl from the socket side
 */
static int				/* 0 or error */
ppp_ioctl(struct ifnet *ifp,
	 int cmd,
	 void *data)
{
#define pcp ((struct ppp_cont*)ifp)	/* keep -fullwarn happy */
	int error = 0;
	int flags;
#ifdef _MP_NETLOCKS
	ASSERT(IFNET_ISLOCKED(ifp));
#else
	int s = splimp();
#endif

	ASSERT(pcp->pc_if.if_name == ppp_name);
	switch (cmd) {
	case SIOCSIFADDR:
		if (_IFADDR_SA(data)->sa_family == AF_INET) {
			if (pcp->pc_rq)
				pcp->pc_if.if_flags |= IFF_ALIVE;
		} else {
			error = EAFNOSUPPORT;
		}
		break;

	case SIOCSIFDSTADDR:
		break;

	case SIOCSIFFLAGS:
		flags = ((struct ifreq *)data)->ifr_flags;
		if (flags & IFF_UP) {

			if (pcp->pc_if.in_ifaddr != 0 && pcp->pc_rq) {
				pcp->pc_if.if_flags = flags | IFF_ALIVE;
			} else {
				error = EINVAL;
			}
		} else {
			ppp_down(pcp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (((struct sockaddr *)data)->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	default:
		error = EINVAL;
	}

#ifndef _MP_NETLOCKS
	splx(s);
#endif
	return error;
#undef pcp
}


/* take an mbuf chain from above and see about sending it down the stream
 */
/* ARGSUSED */
static int
#if !defined(PPP_IRIX_53) && !defined(PPP_IRIX_62) && !defined(PPP_IRIX_63) && !defined(PPP_IRIX_64)
ppp_output(struct ifnet *ifp,
	   struct mbuf *m0,
	   struct sockaddr *dst,
	   struct rtentry *rte)
#else
ppp_output(struct ifnet *ifp,
	   struct mbuf *m0,
	   struct sockaddr *dst)
#endif
{
#define pcp ((struct ppp_cont*)ifp)	/* keep -fullwarn happy */
#define PEEP_LEN 4			/* peep into IP data to filter */
	register int totlen;
	register struct ip* ip;
	register int hlen;
	struct udphdr *udp;
#define TCP  ((struct tcphdr*)udp)
#define	ICMP ((struct icmp*)udp)
#ifdef _MP_NETLOCKS

	ASSERT(IFNET_ISLOCKED(ifp));
#else
	int s;

#endif
	ASSERT(pcp->pc_if.if_name == ppp_name);

	if (dst->sa_family != AF_INET) {
		if (pcp->pc_if.if_flags & IFF_DEBUG)
			printf("ppp%d: af%u not supported\n",
			       pcp->pc_if.if_unit, dst->sa_family);
		m_freem(m0);
		return (EAFNOSUPPORT);
	}


#ifndef _MP_NETLOCKS
	s = splimp();
#endif
	if (iff_dead(pcp->pc_if.if_flags)) {
		pcp->pc_if.if_odrops++;
		IF_DROP(&pcp->pc_if.if_snd);
#ifndef _MP_NETLOCKS
		splx(s);
#endif
		m_freem(m0);
		return(ENOBUFS);
	}

	totlen = m_length(m0);
	hlen = MIN(totlen, 15*4+PEEP_LEN);
	if (m0->m_len < hlen) {
		m0 = m_pullup(m0, hlen);
		if (!m0) {
			pcp->pc_if.if_odrops++;
			IF_DROP(&pcp->pc_if.if_snd);
#ifndef _MP_NETLOCKS
			splx(s);
#endif
			return(ENOBUFS);
		}
	}

	ip = mtod(m0,struct ip*);
	hlen = ip->ip_hl<<2;
	udp = (struct udphdr*)&((char*)ip)[hlen];

	if (totlen < PEEP_LEN+hlen) {
		/* just guess about stray traffic */
		pcp->pc_b.pb_state |= PB_ST_BEEP;

	} else if (ip->ip_p == IPPROTO_ICMP) {
		/* optionally drop ICMP traffic */
		if (!(pcp->pc_b.pb_state & PB_ST_NOICMP)
		    && !IGNORING_ICMP(pcp->pc_afilter, ICMP->icmp_type)) {
			/* note activity	*/
			pcp->pc_b.pb_state |= PB_ST_BEEP;

		} else if (pcp->pc_b.pb_pl_num == 0
			   || (pcp->pc_b.pb_state & PB_ST_NOICMP)) {
			pcp->pc_if.if_odrops++;
			IF_DROP(&pcp->pc_if.if_snd);
#ifndef _MP_NETLOCKS
			splx(s);
#endif
			m_freem(m0);
			if (pcp->pc_if.if_flags & IFF_DEBUG)
				printf("ppp%d: drop ICMP type %d packet\n",
				       pcp->pc_if.if_unit,
				       ICMP->icmp_type);
			return(0);
		}

	} else if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP) {
		/* Cheat, knowing that the UDP and TCP headers start
		 * the same.
		 */
		if (!IGNORING_PORT(pcp->pc_afilter,
				   udp->uh_sport)
		    && !IGNORING_PORT(pcp->pc_afilter,
				      udp->uh_dport)) {
			/* Count it as activity */
			pcp->pc_b.pb_state |= PB_ST_BEEP;
		} else if (pcp->pc_b.pb_pl_num == 0) {
			pcp->pc_if.if_odrops++;
			IF_DROP(&pcp->pc_if.if_snd);
#ifndef _MP_NETLOCKS
			splx(s);
#endif
			m_freem(m0);
			if (pcp->pc_if.if_flags & IFF_DEBUG)
				printf("ppp%d: drop %s packet"
				       " from port %d to port %d\n",
				       pcp->pc_if.if_unit,
				       ip->ip_p==IPPROTO_TCP ? "TCP" : "UDP",
				       (int)udp->uh_sport,
				       (int)udp->uh_dport);
			return(0);
		}

	} else {
		/* just guess about stray traffic */
		pcp->pc_b.pb_state |= PB_ST_BEEP;
	}

	if (IF_QFULL(&pcp->pc_if.if_snd)) {
		struct mbuf *om;

		pcp->pc_if.if_odrops++;
		IF_DROP(&pcp->pc_if.if_snd);
		/* drop oldest packet */
		IF_DEQUEUE(&pcp->pc_if.if_snd, om);
		if (om == pcp->pc_smbuf) {
			pcp->pc_smbuf = 0;
			pcp->pc_b.pb_starve = 0;
		}
		m_freem(om);
	}

	/* Type-Of-Service improvements by moving traffic to the front of
	 *  the queue:
	 *	ancient "bigxmit" hack: based on packet size
	 *	new "telnettos" hack: based on TCP port number
	 *	sanctioned:
	 */
	if (0 != (ip->ip_tos & IPTOS_LOWDELAY)
	    || totlen <= pcp->pc_b.pb_bigxmit
	    || (pcp->pc_b.pb_telnettos
		&& (ip->ip_p == IPPROTO_ICMP
		    || (ip->ip_p == IPPROTO_TCP
			&& totlen >= PEEP_LEN+hlen
			&& (TCP->th_sport == 23
			    || TCP->th_dport == 23
			    || TCP->th_sport == 513
			    || TCP->th_dport == 513
			    || TCP->th_sport == 518
			    || TCP->th_dport == 518))))) {
		/* put interactive packets at the front of the queue, but
		 *	after older interactive packets.
		 */
		if (pcp->pc_if.if_snd.ifq_tail == 0
		    || pcp->pc_if.if_snd.ifq_tail == pcp->pc_smbuf) {
			IF_ENQUEUE(&pcp->pc_if.if_snd, m0);
			pcp->pc_smbuf = m0;
		} else {
			pcp->pc_b.pb_meters.move2f++;
			if (pcp->pc_smbuf == 0) {
				IF_PREPEND(&pcp->pc_if.if_snd, m0);
			} else {
				m0->m_act = pcp->pc_smbuf->m_act;
				pcp->pc_smbuf->m_act = m0;
				pcp->pc_if.if_snd.ifq_len++;
			}
			/* occassionally, move the end of the small-packet
			 *	part of the queue past a big packet.  This
			 *	prevents starvation of big-packet users.
			 */
			if (++pcp->pc_b.pb_starve > pcp->pc_b.pb_bigpersmall) {
				pcp->pc_b.pb_meters.move2f_leak++;
				pcp->pc_b.pb_starve = 0;
				pcp->pc_smbuf = m0->m_act;
			} else {
				pcp->pc_smbuf = m0;
			}
		}
	} else {
		if (pcp->pc_if.if_snd.ifq_tail == 0
		    || pcp->pc_smbuf == 0) {
			pcp->pc_b.pb_starve = 0;
		}
		IF_ENQUEUE(&pcp->pc_if.if_snd, m0);
	}
#ifndef _MP_NETLOCKS
	splx(s);
#endif

	/* On first packet or when we are shut down, poke the driver
	 * and daemon.
	 */
	if (pcp->pc_if.if_snd.ifq_len == 1
	    || pcp->pc_b.pb_pl_num == 0) {
#ifdef _MP_NETLOCKS
		/* qenable() now sometimes calls the target function */
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined (PPP_IRIX_64)
		IFNET_UNLOCKNOSPL(ifp);
#else
		IFNET_UNLOCK(ifp);
#endif
#endif
		streams_interrupt((strintrfunc_t)ppp_wq_qenable,pcp,0,0);
#ifdef _MP_NETLOCKS
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined (PPP_IRIX_64)
		IFNET_LOCKNOSPL(ifp);
#else
		IFNET_LOCK(ifp);
#endif
#endif
	}

	return 0;
#undef pcp
}


/* open PPP STREAMS module
 *	Be either a multiplexing driver for new style or simple module
 *	for old style.
 */
/* ARGSUSED */
static int				/* 0, minor #, or failure reason */
ppp_open(queue_t *rq,			/* our read queue */
	 dev_t *devp,
	 int flag,
	 int sflag,
	 struct cred *crp)
{
	struct ppp_cont *pcp, **pcpp;
	queue_t *wq = WR(rq);
	int s;


	if (sflag != CLONEOPEN)		/* only clone */
		return EINVAL;

	if (0 == rq->q_ptr) {		/* initialize on the 1st open */
		if (drv_priv(crp))	/* only root can add network links */
			return EPERM;

		STR_LOCK_SPL(s);
		/* look for an available slot */
		pcpp = &ppp_pc_first;
		for (;;) {
			pcp = *pcpp;
			if (!pcp) {
				if (pc_num >= MAX_PPP_DEVS) {
					STR_UNLOCK_SPL(s);
					return EBUSY;
				}
				/* make one if needed and possible */
				pcp = KSALLOC(ppp_cont,1);
				if (!pcp) {
					STR_UNLOCK_SPL(s);
					return ENOMEM;
				}
				bzero(pcp,sizeof(*pcp));
				*pcpp = pcp;
				pcp->pc_if.if_name = ppp_name;
				pcp->pc_if.if_unit = pc_num++;
				pcp->pc_if.if_type = IFT_PTPSERIAL;
				pcp->pc_if.if_ioctl = ppp_ioctl;
				pcp->pc_if.if_output = ppp_output;
				if_attach(&pcp->pc_if);
				rawif_attach(&pcp->pc_rawif,
					     &pcp->pc_if, 0, 0, 0, 0, 0, 0);
				break;
			}
			if (!pcp->pc_rq)
				break;
			pcpp = &pcp->pc_next;
		}

		rq->q_ptr = (caddr_t)pcp;
		wq->q_ptr = (caddr_t)pcp;
		pcp->pc_rq = rq;
		pcp->pc_wq = wq;

		pcp->pc_b.pb_bigxmit = 0;
		pcp->pc_b.pb_state = 0;

		if (pcp->pc_afilter)
			bzero(pcp->pc_afilter, sizeof(*pcp->pc_afilter));

		pcp->pc_if.if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		pcp->pc_if.if_mtu = PPP_DEF_MRU;
		pcp->pc_b.pb_mtru = PPP_DEF_MRU;
		pcp->pc_if.if_snd.ifq_maxlen= IFQ_MAXLEN;
		pcp->pc_if.if_snd.ifq_drops = 0;
		pcp->pc_if.if_oerrors = 0;
		pcp->pc_if.if_opackets = 0;
		pcp->pc_if.if_obytes = 0;
		pcp->pc_if.if_ierrors = 0;
		pcp->pc_if.if_ipackets = 0;
		pcp->pc_if.if_ibytes = 0;
		pcp->pc_if.if_iqdrops = 0;
		pcp->pc_if.if_odrops = 0;
		bzero(&pcp->pc_b.pb_meters, sizeof(pcp->pc_b.pb_meters));
		STR_UNLOCK_SPL(s);
		*devp = makedevice(getemajor(*devp), pcp->pc_if.if_unit);
	}

	return 0;
}



/* flush input or output
 */
static void
ppp_flush(u_char op,
	  struct ppp_cont *pcp,
	  struct ppp_dev *pdp)
{
	queue_t *wq;
	mblk_t *bp;
	int s;

	STR_LOCK_SPL(s);
	if (op & FLUSHW) {
		if (pcp != 0) {
			if (0 != (wq = pcp->pc_wq)) {
				flushq(wq,FLUSHALL);
				qenable(wq);
			}
			pcp->pc_if.if_odrops +=if_dropall(&pcp->pc_if.if_snd);
			pcp->pc_smbuf = 0;
			pcp->pc_b.pb_starve = 0;
		}
		if (pdp != 0)
			flushq(pdp->pd_wq,FLUSHALL);
	}

	if (op & FLUSHR) {
		if (pcp != 0) {
			if (pcp->pc_rq != 0)
				flushq(pcp->pc_rq,FLUSHALL);

			/* flush MP fragment reassembly queue
			 */
			while (0 != (bp = pcp->pc_mp_frags)) {
				pcp->pc_mp_frags = bp->b_next;
				freemsg(bp);
			}
			pcp->pc_mp.num_frags = 0;
		}
		if (pdp != 0)
			flushq(pdp->pd_rq,FLUSHALL);
	}
	STR_UNLOCK_SPL(s);
}



/* close PPP STREAMS module */
/* ARGSUSED */
static
ppp_close(queue_t *rq, int flag, struct cred *crp)
{
	struct ppp_cont *pcp = (struct ppp_cont*)rq->q_ptr;
#ifdef PPP_IRIX_53
#undef in_ifaddr
	struct in_ifaddr *ia;
#endif
	int s;

	ASSERT(pcp->pc_rq == rq);
	ASSERT(pcp->pc_wq->q_ptr == (caddr_t)pcp);

#ifdef _MP_NETLOCKS
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined (PPP_IRIX_64)
	IFNET_LOCK(&pcp->pc_if, s);
#else
	IFNET_LOCK(&pcp->pc_if);
#endif
#else
	s = splnet();
#endif
#ifndef PPP_IRIX_53
	(void)in_rtinit(&pcp->pc_if);
#else /* PPP_IRIX_53 */
	/* XXX This is a kludge to make the rest of the kernel forget
	 *	about this interface.  Leaving the interface around
	 *	tends to make the add-route code install new routes
	 *	that point to this interface, rather than the correct
	 *	one.
	 * That the ADDRT ioctl does not allow the routing daemon to
	 *	specify the interface, and that the add-route code does
	 *	not notice whether the interface is happy is a problem
	 *	which should be fixed.  Normally, interfaces do not come
	 *	and go, and so these deficiencies cause no hardship.
	 *	Since the routing demon may not know the correct interface,
	 *	and since one might want to be able to add a route thru an
	 *	interface that is not "up", there the correct fix is not
	 *	trivial.
	 */
	for (ia = in_ifaddr; ia; ia = ia->ia_next) {
		if (ia->ia_ifp == &pcp->pc_if) {
			if (ia->ia_flags & IFA_ROUTE) {
#ifdef ROUTING_SOCKETS
				rtinit(&ia->ia_ifa, RTM_DELETE, RTF_HOST);
#else
				rtinit(&ia->ia_dstaddr, &ia->ia_addr,
				       (int)SIOCDELRT, RTF_HOST);
#endif
				ia->ia_flags &= ~IFA_ROUTE;
			}
#ifdef ROUTING_SOCKETS
			ia->ia_addr.sin_family = AF_UNSPEC;
			ia->ia_dstaddr.sin_family = AF_UNSPEC;
#else
			ia->ia_addr.sa_family = AF_UNSPEC;
			ia->ia_dstaddr.sa_family = AF_UNSPEC;
#endif
			break;
		}
	}
#endif /* PPP_IRIX_53 */
	ppp_down(pcp);

#ifdef _MP_NETLOCKS
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined (PPP_IRIX_64)
	IFNET_UNLOCK(&pcp->pc_if, s);
#else
	IFNET_UNLOCK(&pcp->pc_if);
#endif
#else
	splx(s);
#endif
	STR_LOCK_SPL(s);
	pcp->pc_b.pb_state &= ~(PB_ST_BEEP | PB_ST_MP_TIME);
	ppp_flush(FLUSHRW, pcp, 0);	/* flush everything */
	ASSERT(pcp->pc_b.pb_pl_num == 0);
	ASSERT(pcp->pc_pd_head == 0);

	rq->q_ptr = 0;
	pcp->pc_wq->q_ptr = 0;
	pcp->pc_rq = 0;
	pcp->pc_wq = 0;
	KSFREE(pcp->pc_afilter);
	KSFREE(pcp->pc_vj_comp);
	KSFREE(pcp->pc_vj_uncomp);
	KSFREE(pcp->pc_rx_pred);
	KSFREE(pcp->pc_tx_pred);
	KSFREE(pcp->pc_rx_bsd);
	KSFREE(pcp->pc_tx_bsd);
	STR_UNLOCK_SPL(s);
	return 0;			/* XXX it is really a void */
}



/* check a PPP IOCTL
 */
static struct ppp_dev*
ppp_ckioctl(struct ppp_cont *pcp,
	    struct ppp_arg *arg,
	    char *msg)
{
	struct ppp_dev *pdp;

	pdp = find_pdp(pcp,arg->dev_index);
	if (!pdp) {
		if (pcp->pc_if.if_flags & IFF_DEBUG)
			printf("ppp%d: bogus index 0x%x for SIOC_%s\n",
			       pcp->pc_if.if_unit, arg->dev_index, msg);
	}
	return pdp;
}


static void
ppp_fin_ioctl(queue_t *wq,
	      struct ppp_arg *arg,
	      mblk_t *bp)
{
	arg->version = IF_PPP_VERSION;
	bp->b_datap->db_type = M_IOCACK;
	qreply(wq,bp);
}


/* Allocate output compression buffer.
 *	It must be big enough in case the output does not compress.
 */
static int				/* 0=failed */
ppp_get_cp(struct ppp_cont *pcp,
	   int bits)			/* 0=Predictor, or LZW code size */
{
	int size;

	/* Predictor has a worst case expansion of 1/8.
	 * LZW expands by less than the ratio of the code size to the
	 * size of a byte.
	 */
	if (!bits) {
		size = (pcp->pc_b.pb_mtru*9 + 7)/8 + PRED_OVHD;
	} else {
		size = (pcp->pc_b.pb_mtru*bits + 7)/8 + BSD_OVHD;
	}
	if (pcp->pc_tx_cp_buf != 0) {
		if (pcp->pc_tx_cp_buf_size >= size)
			return 1;
		KSFREE(pcp->pc_tx_cp_buf);
	}
	pcp->pc_tx_cp_buf_size = size;
	pcp->pc_tx_cp_buf = kern_malloc(size);
	return (pcp->pc_tx_cp_buf != 0);
}


/* handle STREAMS packets from the PPP daemon via the stream head.
 */
static void
ppp_wput(queue_t *wq,			/* our write queue */
	mblk_t *bp)
{
	struct ppp_cont *pcp = (struct ppp_cont*)wq->q_ptr;
	struct iocblk *iocp;
	struct ppp_dev *pdp;
	struct ppp_arg *arg;
	struct ppp_buf *pb;
	struct pred_db **p_pred;
	struct bsd_db **p_bsd;
	__uint32_t *p_state;
	struct vj_uncomp **pvj_uncomp;
	mblk_t *bp2;
	int i, s, totlen;
	u_char *rptr;
	struct mbuf *m;
	struct ppp_info *pi;
	queue_t *wq1;
#	define CK_ARG {					\
	if (iocp->ioc_count != sizeof(struct ppp_arg))	\
		break;					\
	arg = (struct ppp_arg*)bp->b_cont->b_rptr;}
#	define LNKP ((struct linkblk*)bp->b_cont->b_rptr)


	ASSERT(pcp->pc_wq == wq);
	ASSERT(pcp->pc_rq->q_ptr == (caddr_t)pcp);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		pcp->pc_if.if_opackets++;
		/* This limits the size of packets from the daemon to
		 * what can fit it one STREAMS buffer.  It is also
		 * not very efficient, but the daemon does not send
		 * many packets.
		 */
		if (!pullupmsg(bp, -1)) {
			pcp->pc_if.if_oerrors++;
			pcp->pc_if.if_odrops++;
			freemsg(bp);
			return;
		}
		pb = (struct ppp_buf*)bp->b_rptr;
		pdp = ((pb->dev_index == -1)
		       ? pcp->pc_pd_head
		       : find_pdp(pcp, pb->dev_index));
		if (!pdp) {
			printf("ppp%d: bogus device index 0x%x\n",
			       pcp->pc_if.if_unit, pb->dev_index);
			freemsg(bp);
			return;
		}
		m = 0;
		rptr = pb->un.info;
		totlen = bp->b_wptr - rptr;
		bp2 = ppp_oframe(pcp,pdp, pb->bits, pb->proto, totlen,
				 &totlen, &m, &rptr);
		freemsg(bp);
		if (bp2 == 0) {
			pcp->pc_if.if_oerrors++;
			pcp->pc_if.if_odrops++;
		} else {
			putnext(pdp->pd_wq, bp2);
		}
		return;

	case M_FLUSH:
		ppp_flush(*bp->b_rptr, pcp, 0);
		if (*bp->b_rptr & FLUSHR) {
			*bp->b_rptr &= ~FLUSHW;
			qreply(wq, bp);
		} else {
			freemsg(bp);
		}
		return;

	case M_IOCTL:
		iocp = (struct iocblk*)bp->b_rptr;
		switch (iocp->ioc_cmd) {
		case I_LINK:
			ASSERT(iocp->ioc_count == sizeof(struct linkblk));
			ASSERT(LNKP->l_qtop == wq);
			/* Only the data stream to the framer should ever
			 * be linked or unlinked.
			 */
			wq1 = LNKP->l_qbot;
			if (wq1->q_next->q_qinfo->qi_minfo != &ppp_fram_info) {
				iocp->ioc_error = EINVAL;
				break;
			}

			STR_LOCK_SPL(s);
			if (pcp->pc_b.pb_pl_num >= MAX_PPP_LINKS) {
				STR_UNLOCK_SPL(s);
				iocp->ioc_error = EINVAL;
				break;
			}
			if ((pdp = ppp_pd_free) != 0) {
				ppp_pd_free = pdp->pd_next;
			} else {
				if (!(pdp = KSALLOC(ppp_dev,1))) {
					iocp->ioc_error = ENOMEM;
					STR_UNLOCK_SPL(s);
					break;
				}
			}
			bzero(pdp,sizeof(*pdp));

			if (0 != (pdp->pd_next = pcp->pc_pd_head))
				pcp->pc_pd_head->pd_prev = pdp;
			pcp->pc_pd_head = pdp;
			pdp->pd_pc = pcp;
			pdp->pd_l.pl_index = LNKP->l_index;
			pdp->pd_wq = wq1;
			pdp->pd_rq = RD(pdp->pd_wq);
			pdp->pd_wq->q_ptr = (caddr_t)pdp;
			pdp->pd_rq->q_ptr = (caddr_t)pdp;
			pdp->pd_pf = (struct ppp_pf*)wq1->q_next->q_ptr;
			pcp->pc_b.pb_state |= PB_ST_NEED_NULLS;
			if (++pcp->pc_b.pb_pl_num == 1) {
				bzero(&pcp->pc_mp, sizeof(pcp->pc_mp));
				pcp->pc_mp.sn_in = -1;
			} else if (pcp->pc_b.pb_state & PB_ST_MP) {
				pdp->pd_l.pl_state |= PL_ST_BB_TX;
				pdp->pd_l.pl_mp_sn = pcp->pc_mp.sn_in;
				if (pdp->pd_next->pd_l.pl_state & PL_ST_TX_IP)
					pdp->pd_l.pl_state |= PL_ST_TX_IP;
				pcp->pc_mp.out_head = MP_FLUSH_LEN;
			}
			pdp->pd_l.pl_max_frag = PPP_MAX_MTU;
			STR_UNLOCK_SPL(s);
			noenable(pdp->pd_rq);	/* only back-enable by mux */
			/* get any accumulated input or output flowing */
			qenable(pcp->pc_rq);
			qenable(pcp->pc_wq);
			iocp->ioc_count = 0;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq,bp);
			return;

		case I_UNLINK:
			STR_LOCK_SPL(s);
			ASSERT(LNKP->l_qtop == wq);
			pdp = find_pdp(pcp, LNKP->l_index);
			ASSERT(pdp != 0);
			ASSERT(LNKP->l_qbot == pdp->pd_wq);
			if (pdp->pd_next != 0)
				pdp->pd_next->pd_prev = pdp->pd_prev;
			if (pdp->pd_prev == 0) {
				pcp->pc_pd_head = pdp->pd_next;
			} else {
				pdp->pd_prev->pd_next = pdp->pd_next;
				pdp->pd_prev = 0;
			}
			KSFREE(pdp->pd_tx_pred);
			KSFREE(pdp->pd_tx_bsd);
			KSFREE(pdp->pd_vj_uncomp);

			pcp->pc_pd_cur = 0;
			--pcp->pc_b.pb_pl_num;

			bzero(pdp,sizeof(*pdp));
			pdp->pd_next = ppp_pd_free;
			ppp_pd_free = pdp;

			STR_UNLOCK_SPL(s);
			iocp->ioc_count = 0;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq,bp);
			/* Get stalled frags freed when a link dies.
			 */
			qenable(RD(wq));
			return;

		case SIOC_PPP_AFILTER:
			if (iocp->ioc_count != sizeof(__uint32_t)) {
				iocp->ioc_error = EINVAL;
				break;
			}
			if (!pcp->pc_afilter) {
				pcp->pc_afilter = KSALLOC(afilter,1);
				if (!pcp->pc_afilter) {
					iocp->ioc_error = ENOMEM;
					break;
				}
			}
			i = copyin((caddr_t)(__psunsigned_t)*(__uint32_t*
							)bp->b_cont->b_rptr,
				   pcp->pc_afilter,
				   sizeof(struct afilter));
			if (i == 0) {
				bp->b_datap->db_type = M_IOCACK;
			} else {
				iocp->ioc_error = EFAULT;
			}
			qreply(wq,bp);
			return;

		case SIOC_PPP_NOICMP:
			CK_ARG;
			pcp->pc_b.pb_state |= PB_ST_NOICMP;
			ppp_fin_ioctl(wq,arg,bp);
			return;

		case SIOC_PPP_MTU:
			CK_ARG;
			STR_LOCK_SPL(s);
			pcp->pc_b.pb_mtru = arg->v.mtu.mtru;
			pcp->pc_if.if_mtu = arg->v.mtu.ip_mtu;
			pcp->pc_if.if_snd.ifq_maxlen = arg->v.mtu.qmax;
			pcp->pc_b.pb_bigxmit = arg->v.mtu.bigxmit;
			i = (pcp->pc_b.pb_bigxmit <= PPP_MIN_MTU
			     ? pcp->pc_if.if_mtu
			     : pcp->pc_b.pb_bigxmit);
			pcp->pc_b.pb_bigpersmall = pcp->pc_if.if_mtu/i+1;
			pcp->pc_b.pb_telnettos = arg->v.mtu.telnettos;
			STR_UNLOCK_SPL(s);
			ppp_fin_ioctl(wq,arg,bp);
			return;

		case SIOC_PPP_LCP:
			CK_ARG;
			if (!(pdp = ppp_ckioctl(pcp,arg, "PPP_LCP")))
				break;
			STR_LOCK_SPL(s);
			pdp->pd_l.pl_state &= ~(PL_ST_ACOMP
						| PL_ST_PCOMP
						| PL_ST_SYNC);
			bcopy(arg->v.lcp.tx_accm, pdp->pd_tx_accm,
			      sizeof(pdp->pd_tx_accm));
			if (arg->v.lcp.bits & SIOC_PPP_LCP_ACOMP)
				pdp->pd_l.pl_state |= PL_ST_ACOMP;
			if (arg->v.lcp.bits & SIOC_PPP_LCP_PCOMP)
				pdp->pd_l.pl_state |= PL_ST_PCOMP;
			if (arg->v.lcp.bits & SIOC_PPP_LCP_SYNC)
				pdp->pd_l.pl_state |= PL_ST_SYNC;
			STR_UNLOCK_SPL(s);
			putnext(pdp->pd_wq,bp);
			return;

		case SIOC_PPP_VJ:
			/* Turn VJ header compression on or off.
			 */
			CK_ARG;
			if (!(pdp = ppp_ckioctl(pcp,arg, "PPP_VJ")))
				break;
			STR_LOCK_SPL(s);
			pdp->pd_l.pl_link_num = arg->v.vj.link_num;
			pvj_uncomp = ((pcp->pc_b.pb_state & PB_ST_MP)
				      ? &pcp->pc_vj_uncomp
				      : &pdp->pd_vj_uncomp);
			if (arg->v.vj.rx_slots == 0) {
				KSFREE(*pvj_uncomp);
			} else {
				*pvj_uncomp = vj_uncomp_init(*pvj_uncomp,
							arg->v.vj.rx_slots);
				if (!*pvj_uncomp) {
					STR_UNLOCK_SPL(s);
					iocp->ioc_error = ENOMEM;
					break;
				}
			}

			pcp->pc_b.pb_state &= ~(PB_ST_VJ | PB_ST_VJ_SLOT);
			if (arg->v.vj.tx_slots == 0) {
				KSFREE(pcp->pc_vj_comp);
			} else {
				pcp->pc_vj_comp = vj_comp_init(pcp->pc_vj_comp,
							arg->v.vj.tx_slots);
				if (!pcp->pc_vj_comp) {
					STR_UNLOCK_SPL(s);
					iocp->ioc_error = ENOMEM;
					break;
				}
				if (arg->v.vj.tx_comp)
					pcp->pc_b.pb_state |= PB_ST_VJ;
				if (arg->v.vj.tx_compslot)
					pcp->pc_b.pb_state |= PB_ST_VJ_SLOT;
			}
			STR_UNLOCK_SPL(s);
			ppp_fin_ioctl(wq,arg,bp);
			return;

		case SIOC_PPP_TX_OK:
			CK_ARG;
			if (!(pdp = ppp_ckioctl(pcp,arg, "PPP_TX_OK")))
				break;
			STR_LOCK_SPL(s);
			if (arg->v.ok) {
				pdp->pd_l.pl_state |= PL_ST_TX_LINK;
				qenable(wq);
			} else {
				pdp->pd_l.pl_state &= ~PL_ST_TX_LINK;
			}
			STR_UNLOCK_SPL(s);
			ppp_fin_ioctl(wq,arg,bp);
			return;

		case SIOC_PPP_RX_OK:
			CK_ARG;
			if (!(pdp = ppp_ckioctl(pcp,arg, "PPP_RX_OK")))
				break;
			STR_LOCK_SPL(s);
			if (!(pcp->pc_b.pb_state & PB_ST_MP)) {
				/* if MP off, then only pass it on to
				 * the framer.
				 */
				pcp->pc_b.pb_state |= PB_ST_ON_RX_CCP;
			} else {
				if (arg->v.ok) {
					pcp->pc_b.pb_state |= PB_ST_ON_RX_CCP;
				} else {
					pcp->pc_b.pb_state &= ~PB_ST_ON_RX_CCP;
				}
				qenable(RD(wq));
			}
			STR_UNLOCK_SPL(s);
			putnext(pdp->pd_wq, bp);
			return;

		case SIOC_PPP_1_RX:
			/* Should never see this on lines under the module,
			 * so cause an error.
			 */
			break;

		case SIOC_PPP_CCP_RX:
			CK_ARG;
			if (!(pcp->pc_b.pb_state & PB_ST_MP)) {
				/* just pass it on to framer if MP off */
				if (!(pdp = ppp_ckioctl(pcp,arg,"PPP_CCP_RX")))
					break;
				STR_LOCK_SPL(s);
				pcp->pc_b.pb_state |= PB_ST_ON_RX_CCP;
				STR_UNLOCK_SPL(s);
				putnext(pdp->pd_wq, bp);
			} else {
				i = pf_sioc_rx_init(iocp, arg,
						    &pcp->pc_rx_pred,
						    &pcp->pc_rx_bsd,
						    PB_ST_RX_PRED,
						    PB_ST_RX_BSD);
				STR_LOCK_SPL(s);
				pcp->pc_b.pb_state = (i
						      | (pcp->pc_b.pb_state
							  & ~PB_ST_RX_CP)
						      | PB_ST_ON_RX_CCP);
				pcp->pc_b.pb_ccp_id = PPP_CCP_DISARM_ID;
				STR_UNLOCK_SPL(s);
				ppp_fin_ioctl(wq,arg,bp);
				qenable(RD(wq));
			}
			return;

		case SIOC_PPP_CCP_TX:
			CK_ARG;
			STR_LOCK_SPL(s);
			if (pcp->pc_b.pb_state & PB_ST_MP) {
				p_pred = &pcp->pc_tx_pred;
				p_bsd = &pcp->pc_tx_bsd;
				p_state = &pcp->pc_b.pb_state;
			} else {
				if (!(pdp = ppp_ckioctl(pcp,arg,
							"PPP_CCP_TX"))) {
					STR_UNLOCK_SPL(s);
					break;
				}
				p_pred = &pdp->pd_tx_pred;
				p_bsd = &pdp->pd_tx_bsd;
				p_state = &pdp->pd_l.pl_state;
			}
			*p_state &= ~ST_TX_CP;
			STR_UNLOCK_SPL(s);
			i = arg->v.ccp.proto;
			if (i & SIOC_PPP_CCP_PRED1) {
				KSFREE(*p_bsd);
				*p_pred = pf_pred1_init(*p_pred,
							arg->v.ccp.unit, 0);
				if (!*p_pred
				    || !ppp_get_cp(pcp, 0)) {
					iocp->ioc_error = ENOMEM;
					break;
				}
				STR_LOCK_SPL(s);
				(*p_pred)->db_i.debug = arg->v.ccp.debug;
				*p_state |= ST_TX_PRED;

			} else if (i & SIOC_PPP_CCP_BSD) {
				KSFREE(*p_pred);
				*p_bsd = pf_bsd_init(*p_bsd,
						     arg->v.ccp.unit,
						     arg->v.ccp.bits, 0);
				if (!*p_bsd
				    || !ppp_get_cp(pcp, arg->v.ccp.bits)) {
					iocp->ioc_error = ENOMEM;
					break;
				}
				STR_LOCK_SPL(s);
				(*p_bsd)->debug = arg->v.ccp.debug;
				*p_state |= ST_TX_BSD;
			}
			STR_UNLOCK_SPL(s);
			ppp_fin_ioctl(wq,arg,bp);
			return;

		case SIOC_PPP_CCP_ARM_CA:
			/* If the daemon had to separately arm the CCP
			 * Configure-Ack watcher and enable input
			 * compression, it would be hard to avoid
			 * a race between the two operations.
			 * So arming the watcher also turns on input.
			 */
			CK_ARG;
			STR_LOCK_SPL(s);
			if (!(pcp->pc_b.pb_state & PB_ST_MP)) {
				/* if MP off, then just pass it on to
				 * the framer
				 */
				if (!(pdp = ppp_ckioctl(pcp,arg,
							"PPP_CCP_ARM_CA"))) {
					STR_UNLOCK_SPL(s);
					break;
				}
				putnext(pdp->pd_wq, bp);
				pcp->pc_b.pb_ccp_id = PPP_CCP_DISARM_ID;
			} else {
				pcp->pc_b.pb_ccp_id = arg->v.ccp_arm.id;
				ppp_fin_ioctl(wq,arg,bp);
				qenable(RD(wq));
			}
			pcp->pc_b.pb_state |= PB_ST_ON_RX_CCP;
			STR_UNLOCK_SPL(s);
			return;

		case SIOC_PPP_IP_TX_OK:
			/* Turn IP on or off for compression.
			 * If MP is on, affect the entire bundle.
			 * If BF&I is in use, change only the target
			 * IP stream.
			 */
			CK_ARG;
			STR_LOCK_SPL(s);
			if (pcp->pc_b.pb_state & PB_ST_MP) {
				for (pdp = pcp->pc_pd_head;
				     pdp != 0;
				     pdp = pdp->pd_next) {
					if (arg->v.ok) {
					    pdp->pd_l.pl_state |= PL_ST_TX_IP;
					} else {
					    pdp->pd_l.pl_state &= ~PL_ST_TX_IP;
					}
				}
			} else {
				if (!(pdp = ppp_ckioctl(pcp,arg,
							"PPP_IP_TX_OK")))
					break;
				if (arg->v.ok) {
					pdp->pd_l.pl_state |= PL_ST_TX_IP;
				} else {
					pdp->pd_l.pl_state &= ~PL_ST_TX_IP;
				}
			}
			STR_UNLOCK_SPL(s);
			ppp_fin_ioctl(wq,arg,bp);
			qenable(wq);
			return;

		case SIOC_PPP_MP:
			CK_ARG;
			if (!(pdp = ppp_ckioctl(pcp,arg, "PPP_MP")))
				break;
			STR_LOCK_SPL(s);
			if (arg->v.mp.on) {
				/* The first time, start the input
				 * compression machinery in the mux.
				 */
				if (!(pcp->pc_b.pb_state & PB_ST_MP)) {
					ASSERT(pcp->pc_pd_head->pd_next == 0);
					pcp->pc_b.pb_state |= PB_ST_MP;
					bzero(&pcp->pc_mp,sizeof(pcp->pc_mp));
					pcp->pc_mp.sn_in = -1;
					KSFREE(pdp->pd_tx_pred);
					KSFREE(pdp->pd_tx_bsd);
					KSFREE(pdp->pd_vj_uncomp);
					pdp->pd_l.pl_mp_sn = 0;
				}
				pcp->pc_b.pb_state |= (PB_ST_NEED_NULLS
						       | PB_ST_MP_TIME);

			} else if (!arg->v.mp.on
				   && (pcp->pc_b.pb_state & PB_ST_MP)) {
				/* Give responsibility for compression back
				 * to devices.  This can be done only when
				 * there is at most one device in the
				 * MP bundle.
				 */
				ASSERT(pcp->pc_pd_head == 0 \
				       || pcp->pc_pd_head->pd_next == 0);
				KSFREE(pcp->pc_rx_pred);
				KSFREE(pcp->pc_tx_pred);
				KSFREE(pcp->pc_rx_bsd);
				KSFREE(pcp->pc_tx_bsd);
				KSFREE(pcp->pc_vj_uncomp);
				/* flush all MP packets */
				ppp_flush(FLUSHRW,pcp,pdp);
				pcp->pc_b.pb_state |= PB_ST_ON_RX_CCP;
				pcp->pc_b.pb_state &= ~(PB_ST_MP|PB_ST_RX_CP);
				pcp->pc_b.pb_ccp_id = PPP_CCP_DISARM_ID;
			}

			pcp->pc_b.pb_state &= ~(PB_ST_MP_OPT_HEAD
						| PB_ST_MP_SEND_SSN
						| PB_ST_MP_RECV_SSN
						| PB_ST_MP_NULLS);
			pdp->pd_l.pl_max_frag = arg->v.mp.max_frag;
			pcp->pc_mp.min_frag = arg->v.mp.min_frag;
			pcp->pc_mp.reasm_window = arg->v.mp.reasm_window;
			if (arg->v.mp.send_ssn)
				pcp->pc_b.pb_state |= PB_ST_MP_SEND_SSN;
			if (arg->v.mp.recv_ssn)
				pcp->pc_b.pb_state |= PB_ST_MP_RECV_SSN;
			if (!arg->v.mp.on) {
				pcp->pc_mp.out_head = 0;
			} else if (!arg->v.mp.must_use_mp) {
				pcp->pc_b.pb_state |= PB_ST_MP_OPT_HEAD;
			} else {
				pcp->pc_mp.out_head = MP_FLUSH_LEN;
			}
			if (arg->v.mp.mp_nulls)
				pcp->pc_b.pb_state |= PB_ST_MP_NULLS;
			pcp->pc_mp.debug = arg->v.mp.debug;
			STR_UNLOCK_SPL(s);

			/* Pass it downstream so that the framer can
			 * enable its input and/or turn off its
			 * compression machinery.
			 */
			putnext(pdp->pd_wq, bp);
			/* we may have enabled input or output */
			qenable(RD(wq));
			qenable(wq);
			return;

		case SIOC_PPP_INFO:
#			define GC(nm) (pi->pi_vj_tx.nm = pcp->pc_vj_comp->nm)
#			define GPI pi->pi_devs[i]
#			define G(s,t) bcopy(s,&GPI.t, sizeof(GPI.t))
#			define GU1(t) (GPI.dev_vj_rx.t = pdp->pd_vj_uncomp->t)
#			define GU2(t) (GPI.dev_vj_rx.t = pcp->pc_vj_uncomp->t)
			if (iocp->ioc_count != sizeof(__uint32_t)) {
				iocp->ioc_error = EINVAL;
				break;
			}
			pi = KSALLOC(ppp_info,1);
			if (!pi) {
				iocp->ioc_error = ENOMEM;
				break;
			}
			bzero(pi,sizeof(*pi));
			pi->pi_version = PI_VERSION;
			pi->pi_len = sizeof(*pi);
			STR_LOCK_SPL(s);
			/* get the overall info about the MP bundle
			 */
			bcopy(&pcp->pc_b,&pi->pi_bundle,sizeof(pi->pi_bundle));
			ppp_get_raw_bytes(pcp);
			pi->pi_bundle.pb_meters.ibytes = pcp->pc_if.if_ibytes;
			pi->pi_bundle.pb_meters.obytes = pcp->pc_if.if_obytes;
			if (pcp->pc_vj_comp) {
				GC(tx_states);
				GC(actconn);
				GC(last);
				GC(cnt_packets);
				GC(cnt_compressed);
				GC(cnt_searches);
				GC(cnt_misses);
			}
			/* get info from each link
			 */
			pdp = pcp->pc_pd_head;
			for (i = 0; i < MAX_PPP_LINKS; i++) {
				/* Get compression and other info from each
				 * link.
				 * Structures containing pointers cannot be
				 * given to the user code because it will
				 * probably have the wrong idea of their
				 * length.
				 */
				if (pdp) {
					bcopy(&pdp->pd_l,
					      &pi->pi_devs[i].dev_link,
					      sizeof(pi->pi_devs[i].dev_link));
					bcopy(&pdp->pd_pf->pf_meters,
					      &pi->pi_devs[i].dev_meters,
					      sizeof(struct pf_meters));
					GPI.dev_pf_state=pdp->pd_pf->pf_state;
				}
				if (pdp && pdp->pd_tx_bsd)
					G(pdp->pd_tx_bsd, dev_tx_db.bsd);
				else if (pdp && pdp->pd_tx_pred)
					G(pdp->pd_tx_pred, dev_tx_db.pred);
				else if (pcp->pc_tx_bsd)
					G(pcp->pc_tx_bsd, dev_tx_db.bsd);
				else if (pcp->pc_tx_pred)
					G(pcp->pc_tx_pred, dev_tx_db.pred);
				if (pdp && pdp->pd_pf->pf_bsd)
					G(pdp->pd_pf->pf_bsd, dev_rx_db.bsd);
				else if (pdp && pdp->pd_pf->pf_pred)
					G(pdp->pd_pf->pf_pred,dev_rx_db.pred);
				else if (pcp->pc_rx_bsd)
					G(pcp->pc_rx_bsd, dev_rx_db.bsd);
				else if (pcp->pc_rx_pred)
					G(pcp->pc_rx_pred, dev_rx_db.pred);
				if (pdp && pdp->pd_vj_uncomp) {
					GU1(rx_states);
					GU1(last);
					GU1(cnt_uncompressed);
					GU1(cnt_compressed);
					GU1(cnt_tossed);
					GU1(cnt_error);
				} else if (pcp->pc_vj_uncomp) {
					GU2(rx_states);
					GU2(last);
					GU2(cnt_uncompressed);
					GU2(cnt_compressed);
					GU2(cnt_tossed);
					GU2(cnt_error);
				}
				if (pdp)
					pdp = pdp->pd_next;
			}
			STR_UNLOCK_SPL(s);
			i = copyout(pi, (caddr_t)(__psunsigned_t)*(__uint32_t*
							)bp->b_cont->b_rptr,
				    sizeof(*pi));
			kern_free(pi);
			if (i != 0) {
				iocp->ioc_error = EFAULT;
			} else {
				bp->b_datap->db_type = M_IOCACK;
			}
			qreply(wq,bp);
#			undef GC
#			undef G
#			undef GPI
#			undef GU1
#			undef GU2
			return;
		}

		/* dump bad or other IOCTLs */
		bp->b_datap->db_type = M_IOCNAK;
		qreply(wq,bp);
		return;

	default:
		/* dump strange messages */
		sdrv_error(wq,bp);
		break;
	}
}


/* Handle an incoming packet.
 *	This can be called only while the STREAMS monitor is held
 *	and STREAMS interrupts disabled because it is called from
 *	both the read service function and the read put function.
 */
static void
ppp_iframe(struct ppp_cont *pcp,
	   struct ppp_dev *pdp,
	   mblk_t *bp,			/* new block */
	   struct ppp_buf *pb)
{
	int i;
	u_int sn;
	mblk_t *bp2, **prev_bp;
	struct ppp_dev *pdp2;
	struct ppp_buf *pb2;
	int s;

	ASSERT(PPP_BUF_ALIGN(pb));

	switch (pb->proto) {
	case PPP_MP:
		/* treat MP too soon as an error */
		if (!(pcp->pc_b.pb_state & PB_ST_MP))
			break;

		pcp->pc_b.pb_meters.mp_frags++;
		if (pcp->pc_b.pb_state & PB_ST_MP_RECV_SSN) {
			/* ensure entire MP header is present
			 */
			if ((u_char*)pb+MB_BUF_HEAD_S > bp->b_wptr) {
				if (pcp->pc_b.pb_meters.mp_pullup++,
				    !pullupmsg(bp,MB_BUF_HEAD_S)) {
					pcp->pc_if.if_ierrors++;
					/* send the bad one upstream */
					pb->type = BEEP_MP_ERROR;
					break;
				}
				pb = (struct ppp_buf*)bp->b_rptr;
				ASSERT(PPP_BUF_ALIGN(pb));
			}
			sn = pb->un.mp_s.sn;
			ASSERT(bp->b_datap->db_ref == 1);   /* changing buf */
			pb->bits = pb->un.mp_s.mp_bits;
			i = ~MP_SSN_MASK;
		} else {
			if ((u_char*)pb+MB_BUF_HEAD_L > bp->b_wptr) {
				if (pcp->pc_b.pb_meters.mp_pullup++,
				    !pullupmsg(bp,MB_BUF_HEAD_L)) {
					pcp->pc_if.if_ierrors++;
					freemsg(bp);
					return;
				}
				pb = (struct ppp_buf*)bp->b_rptr;
				ASSERT(PPP_BUF_ALIGN(pb));
			}
			sn = pb->un.mp_l.sn;
			ASSERT(bp->b_datap->db_ref == 1);   /* changing buf */
			pb->bits = pb->un.mp_l.mp_bits;
			i = ~MP_LSN_MASK;
		}
		sn |= (pdp->pd_l.pl_mp_sn & i);

		/* Deal with sequence number wrap.
		 */
		STR_LOCK_SPL(s);
		if (sn < pdp->pd_l.pl_mp_sn) {
			sn -= i;
			if (sn >= MP_MAX_SN) {
				for (pdp2 = pcp->pc_pd_head;
				     pdp2 != 0;
				     pdp2 = pdp2->pd_next)
					pdp2->pd_l.pl_mp_sn -= MP_TRIM_SN;
				for (bp2 = pcp->pc_mp_frags;
				     bp2 != 0;
				     bp2 = bp2->b_next)
					((struct ppp_buf*
					  )bp2->b_rptr)->type -= MP_TRIM_SN;
				for (i = 0; i < MP_FLUSH_LEN; i++)
					pcp->pc_mp.sn_flush[i] -= MP_TRIM_SN;
				pcp->pc_mp.sn_in -= MP_TRIM_SN;
				sn -= MP_TRIM_SN;
			}
		}

		ASSERT(bp->b_datap->db_ref == 1);   /* we are changing buf */
		pb->type = sn;
		pdp->pd_l.pl_mp_sn = sn;

		/* Send a copy to the daemon if debugging.
		 * If only debugging a little, then only send a copy
		 * if the reserved bits are not zero.
		 */
		if (pcp->pc_mp.debug != 0
		    && (pcp->pc_mp.debug > 1
			|| pb->un.mp_s.rsvd != 0
			|| (!(pcp->pc_b.pb_state & PB_ST_MP_RECV_SSN)
			    && pb->un.mp_l.rsvd != 0))
		    && 0 != (bp2 = allocb(MB_BUF_HEAD_L+4, BPRI_LO))) {
			pb2 = (struct ppp_buf*)bp2->b_rptr;
			i = MP_BUF_HEAD(pcp->pc_b.pb_state&PB_ST_MP_RECV_SSN);
			bcopy(pb,pb2,i);
			pb2->type = BEEP_MP;
			*(u_short*)(bp2->b_rptr+i) = msgdsize(bp) - i;
			bp2->b_wptr += i+2;
			if (pdp->pd_rq->q_first == 0
			    && canput(pcp->pc_rq->q_next)) {
				putnext(pcp->pc_rq, bp2);
			} else {
				putq(pdp->pd_rq,bp2);
			}
		}

		/* Insert into the fragment queue.
		 */
		prev_bp = &pcp->pc_mp_frags;
		while ((bp2 = *prev_bp) != 0) {
			pb2 = (struct ppp_buf*)bp2->b_rptr;
			if (sn < pb2->type)
				break;
			prev_bp = &bp2->b_next;
		}
		bp->b_next = bp2;
		*prev_bp = bp;
		pcp->pc_mp.num_frags++;

		if (canput(pcp->pc_rq->q_next))
			ppp_mp_reasm(pcp);

		/* ensure the flush timer is running if needed */
		RESTART_BEEP(pcp);
		STR_UNLOCK_SPL(s);
		return;

	case PPP_VJC_UNCOMP:
	case PPP_VJC_COMP:
	case PPP_CP:
	case PPP_IP:
	case PPP_CCP:
		/* Data packets must be dealt with in the order in
		 * which they are received.
		 *
		 * Some compression control packets must be dealt with
		 * in the order in which they are received, and others
		 * should be handled immediately.  To keep them correctly
		 * ordered in the trace logs with respect to each other,
		 * send them all to the service function before sending
		 * them to the daemon.
		 */
		pcp->pc_if.if_ipackets++;
		putq(pcp->pc_rq,bp);
		return;
	}

	/* send everything else upstream to the daemon
	 */
	if (pdp->pd_rq->q_first == 0
	    && canput(pcp->pc_rq->q_next))
		putnext(pcp->pc_rq, bp);
	else
		putq(pdp->pd_rq,bp);
}


/* Try to assemble MP fragments.
 *	Combine fragments in a packet until last sequence number
 *	invalidated by the timer.  Stop building packets at the last
 *	sequence number invalidated by the timer.
 *
 *	STREAMS interrupts must be disabled before calling here.
 *
 *	canput(pcp->pc_rq->q_next)) must have given permission.
 *
 *	The caller must ensure the flush timer is running if needed.
 */
static void
ppp_mp_reasm(struct ppp_cont *pcp)
{
	mblk_t *bp, *nbp;
	struct ppp_buf *pb, *npb;
	struct ppp_dev *pdp;
	int sn_M;
	int i;


	/* Compute M (minimum of the maximum sequence numbers seen on all
	 * links), and keep it past the last finished sequence number.
	 */
	sn_M = MP_MAX_SN;
	for (pdp = pcp->pc_pd_head; pdp != 0; pdp = pdp->pd_next) {
		if (sn_M > pdp->pd_l.pl_mp_sn)
			sn_M = pdp->pd_l.pl_mp_sn;
	}
	/* Continue assembling fragments as long as they have consecutive
	 * numbers following the previous fragment finished.
	 */
	if (sn_M < pcp->pc_mp.sn_in)
		sn_M = pcp->pc_mp.sn_in;

	/* Limit the number of outstanding segments by forcing excess
	 * to be processed.
	 */
	i = pcp->pc_mp.num_frags - pcp->pc_mp.reasm_window;
	bp = pcp->pc_mp_frags;
	while (i > 0) {
		pb = (struct ppp_buf*)bp->b_rptr;
		sn_M = pb->type;
		bp = bp->b_next;
		i--;
	}

	for (;;) {
next_packet:;
		bp = pcp->pc_mp_frags;
		if (!bp)
			return;

		pb = (struct ppp_buf*)bp->b_rptr;
		ASSERT(PPP_BUF_ALIGN(pb));

		/* Assemble no packet before its time, in case fragments
		 * for an earlier packet finally arrive.
		 * However, to avoid losing all of the STREAMS buffers in
		 * the system in the fragment queue because the peer
		 * is being naughty, limit the maximum number of
		 * fragments.
		 */
		if (pb->type > sn_M
		    && pb->type > pcp->pc_mp.sn_in+1)
			goto missing_frag;

		/* Assemble the first fragments on the list.  Since
		 * assembled packets must be delivered in order, there
		 * is no profit in looking ahead in the list.
		 */
		for (;;) {
			nbp = bp->b_next;

			if (pb->bits & MP_E_BIT) {
				/* The E bit means we have the end of
				 * a packet.  If all relevant fragments
				 * have arrived, then accept or discard
				 * the packet.
				 * Otherwise wait for more fragments.
				 */
				if (pb->bits != (MP_B_BIT | MP_E_BIT)
				    && pb->type > sn_M)
					goto missing_frag;
				pcp->pc_mp_frags = nbp;
				pcp->pc_mp.num_frags--;
				break;
			}

			/* Stop if we run out of fragments.
			 */
			if (!nbp)
				goto missing_frag;

			npb = (struct ppp_buf*)nbp->b_rptr;

			if (pb->type+1 != npb->type) {
				/* We have a sequence number gap.
				 * It can mean we have a finished but
				 * broken packet.  Be patient if we might
				 * get another fragment for the packet.
				 * Dump it if not.
				 */
				if (pb->type >= sn_M)
					goto missing_frag;
				pcp->pc_mp_frags = nbp;
				pcp->pc_mp.num_frags--;
				break;
			}

			/* Here we have consecutive sequence numbers.
			 * The B bit in the next fragment means the
			 * fragment before it should have had an E bit,
			 * but the peer messed up.
			 */
			if (npb->bits & MP_B_BIT) {
				pcp->pc_mp_frags = nbp;
				pcp->pc_mp.num_frags--;
				break;
			}

			/* Combine the two consecutive fragments by
			 * unlinking the second fragment from the main
			 * chain and appending it to the first fragment.
			 * The sequence number of the combination is that
			 * of the second fragment.
			 */
			bp->b_next = nbp->b_next;
			ASSERT(bp->b_datap->db_ref == 1);
			pb->bits |= npb->bits;
			pb->type = npb->type;
			nbp->b_rptr += MP_BUF_HEAD(pcp->pc_b.pb_state
						   & PB_ST_MP_RECV_SSN);
			if (nbp->b_rptr >= nbp->b_wptr) {
				ASSERT(nbp->b_rptr <= nbp->b_wptr);
				if (nbp->b_cont == 0) {
					/* ignore empty fragments */
					freemsg(nbp);
				} else {
					linkb(bp,nbp->b_cont);
					freeb(nbp);
				}
			} else {
				linkb(bp,nbp);
			}
			pcp->pc_mp.num_frags--;
			if (sn_M <= pb->type)
				pcp->pc_mp.sn_in = sn_M = pb->type;
		}
		if (sn_M <= pb->type)
			pcp->pc_mp.sn_in = sn_M = pb->type;

		/* Discard packets we were too impatient to wait for
		 * and are now out of order.
		 */
		if (pb->type < pcp->pc_mp.sn_in) {
			pcp->pc_if.if_ierrors++;
			ASSERT(bp->b_datap->db_ref == 1);
			pb->type = BEEP_MP_ERROR;
			putq(pcp->pc_rq,bp);
			pcp->pc_b.pb_meters.mp_stale++;
			continue;
		}

		/* Discard incomplete packets
		 */
		if (pb->bits != (MP_B_BIT | MP_E_BIT)) {
			pcp->pc_if.if_ierrors++;
			ASSERT(bp->b_datap->db_ref == 1);
			pb->type = BEEP_MP_ERROR;
			putq(pcp->pc_rq,bp);
			pcp->pc_b.pb_meters.mp_no_bits++;
			continue;
		}

		/* Trim MP header from new packet.
		 * Copy the data from the first STREAMS buffer to
		 * a new buffer, with a simple PPP header and properly
		 * aligned.
		 */
		nbp = allocb(PPP_BUF_HEAD_INFO,BPRI_HI);
		if (0 == nbp) {
			pcp->pc_b.pb_meters.mp_alloc_fails++;
			pcp->pc_if.if_ierrors++;
			ASSERT(bp->b_datap->db_ref == 1);
			pb->type = BEEP_MP_ERROR;
			putq(pcp->pc_rq,bp);
			continue;
		}
		npb = (struct ppp_buf*)nbp->b_wptr;
		nbp->b_wptr += PPP_BUF_HEAD_INFO;
		npb->type = BEEP_FRAME;
		npb->dev_index =  pb->dev_index;
		npb->bits = (MP_B_BIT | MP_E_BIT);
		bp->b_rptr += MP_BUF_HEAD(pcp->pc_b.pb_state
					  & PB_ST_MP_RECV_SSN);
		ASSERT(bp->b_rptr <= bp->b_wptr);

		/* Expand compressed protocol field.
		 */
		for (i = 0; ; i <<= 8) {
			if (bp->b_rptr == bp->b_wptr) {
				mblk_t *bp2;

				bp2 = bp->b_cont;
				freeb(bp);
				bp = bp2;

				/* Discard null MP packets */
				if (bp == 0) {
					pcp->pc_b.pb_meters.mp_null++;
					freemsg(nbp);
					goto next_packet;
				}
				continue;
			}

			i |= *bp->b_rptr++;

			/* The peer could be stupid and sending us
			 * address and control fields.
			 */
			if (i == PPP_ADDR
			    && bp->b_rptr != bp->b_wptr)
				continue;
			if (i == (PPP_ADDR<<8)+PPP_CNTL) {
				i = 0;
				continue;
			}
			/* The last byte of the protocol is odd.
			 */
			if (i & 1)
				break;
		}
		npb->proto = i;
		nbp->b_cont = bp;


		switch (i) {
		case PPP_MP:
			npb->type = BEEP_MP_ERROR;
			pcp->pc_b.pb_meters.mp_nested++;
			break;

		case PPP_IP:
		case PPP_VJC_UNCOMP:
		case PPP_VJC_COMP:
		case PPP_CP:
		case PPP_CCP:
			pcp->pc_if.if_ipackets++;
			putq(pcp->pc_rq,nbp);
			continue;
		}
		/* send everything else upstream */
		putnext(pcp->pc_rq, nbp);
	}
missing_frag:;

	/* Keep flush timer running while there are unassembled fragments.
	 */
	pcp->pc_b.pb_state |= PB_ST_MP_TIME;
}


/* accept a new STREAMS message from the serial line
 */
static void
ppp_rput(queue_t *rq,			/* data read queue */
	 mblk_t *bp)
{
	struct ppp_dev *pdp;
	struct ppp_cont *pcp;
	struct ppp_buf *pb;

	pdp = (struct ppp_dev*)rq->q_ptr;

	/* if the MUX has been cut loose but not reconnected,
	 * junk the data.
	 */
	if (!pdp) {
#ifdef DEBUG
		printf("ppp_rput: stray packet with nowhere to go\n");
#endif
		putq(rq,bp);
		return;
	}
	pcp = pdp->pd_pc;
	ASSERT(find_pdp(pcp,pdp->pd_l.pl_index) == pdp);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		pb = (struct ppp_buf*)bp->b_rptr;
		if (bp->b_wptr < bp->b_rptr+PPP_BUF_HEAD
		    || !PPP_BUF_ALIGN(pb)) {
			if (pcp->pc_if.if_flags & IFF_DEBUG)
				printf("ppp%d: bad PPP header\n",
				       pcp->pc_if.if_unit);
			pcp->pc_b.pb_meters.rput_pullup++;
			if (!pullupmsg(bp, PPP_BUF_HEAD)) {
				pcp->pc_if.if_ierrors++;
				freemsg(bp);
				return;
			}
			pb = (struct ppp_buf*)bp->b_rptr;
		}

		/* fix index of message in transition when the MUX
		 * was built
		 */
		if (bp->b_datap->db_ref != 1) {
			printf("ppp%d: shared PPP buffer\n",
			       pcp->pc_if.if_unit);
			freemsg(bp);
			return;
		}
		if (pb->dev_index == -1)
			pb->dev_index = pdp->pd_l.pl_index;

		/* Send it to the daemon if it is not IP.
		 * Put it on the queue for the mux if it is IP.
		 * (Do not try to put it on the IP queue directly,
		 * because there could be problems between the STREAMS
		 * monitor and the locks.)
		 */
		if (pb->type == BEEP_FRAME) {
			ASSERT(bp->b_wptr >= bp->b_rptr+PPP_BUF_HEAD_INFO);
			ppp_iframe(pcp,pdp,bp,pb);

		} else {
			/* mark death notices quickly */
			if (pb->type == BEEP_DEAD)
				pdp->pd_l.pl_state &= ~PL_ST_TX_LINK;

			/* queue error notifications in sequence to
			 * reset VJ-compression slot ID compression.
			 */
			putq(pcp->pc_rq,bp);
		}
		return;


	case M_CTL:
		/* If it is not a busy indication from an ISDN driver,
		 * then ignore it.
		 */
		if (bp->b_wptr == bp->b_rptr+sizeof(struct isdn_mctl)
		    &&((struct isdn_mctl*)bp->b_rptr)->type==ISDN_ACTIVE_RX) {
			if (if_ppp_printf)
				printf("ppp%d: %d RX busy\n",
				       pcp->pc_if.if_unit,
				       pdp->pd_l.pl_link_num);
			pcp->pc_b.pb_rx_ping++;
			pcp->pc_b.pb_state |= PB_ST_BEEP;
			RESTART_BEEP(pcp);

		} else if (pcp->pc_if.if_flags & IFF_DEBUG) {
			printf("ppp%d: odd M_CTL: len=%d type=%d\n",
			       pcp->pc_if.if_unit,
			       bp->b_wptr-bp->b_rptr,
			       ((struct isdn_mctl*)bp->b_rptr)->type);
		}
		freemsg(bp);
		return;


	case M_FLUSH:
		ppp_flush(*bp->b_rptr, 0, pdp);
		if (*bp->b_rptr & FLUSHW) {
			*bp->b_rptr &= ~FLUSHR;
			qreply(rq, bp);
		} else {
			freemsg(bp);
		}
		return;
	}

	putnext(pcp->pc_rq, bp);
}



/* deal with accumulated STREAMS data buffers,
 *	copying them into mbuf chains
 */
static void
pc_rsrv(queue_t *rq)			/* our read queue */
{
	struct ppp_cont *pcp;
	struct ppp_dev *pdp;
	struct ppp_buf *pb;
	mblk_t *bp, *nbp;
	int proto;
	int totlen, len, dlen, mblen, mboff;
	struct mbuf *m, *m1, *m2;
	caddr_t snooph;
	struct vj_uncomp *vj_uncomp;
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined(PPP_IRIX_64)
#ifdef PPP_IRIX_64
	register s1;
#endif
#endif

	pcp = (struct ppp_cont*)rq->q_ptr;
	if (!pcp) {
		/* When caught in the transition of STREAMS muxes,
		 * hope to be re-awakened when things are sorted out.
		 */
#ifdef DEBUG
		printf("pc_rsrv: all dressed up and nowhere to go\n");
#endif
		return;
	}
	ASSERT(pcp->pc_rq == rq);
	ASSERT(pcp->pc_wq->q_ptr == (caddr_t)pcp);

	/* Check upstream for room for control info once, outside the loop.
	 */
	if (!canput(pcp->pc_rq->q_next))
		goto rcv_stop;

	ppp_mp_reasm(pcp);		/* reassemble MP fragments */

again:;
	while (0 != (bp = getq(rq))) {
		pb = (struct ppp_buf*)bp->b_rptr;
		ASSERT(bp->b_wptr >= bp->b_rptr+PPP_BUF_HEAD);
		ASSERT(PPP_BUF_ALIGN(pb));

		vj_uncomp = pcp->pc_vj_uncomp;
		if (pb->bits != 0) {
			if (!(pcp->pc_b.pb_state & PB_ST_MP)
			    && pb->type == BEEP_FRAME) {
				/* must be a stray from before shut down */
				pcp->pc_b.pb_meters.mp_stray++;
				pb->type = BEEP_MP_ERROR;
			}
			pdp = 0;
		} else {
			pdp = find_pdp(pcp, pb->dev_index);
			if (!pdp) {
				if (pcp->pc_if.if_flags & IFF_DEBUG)
					printf("ppp%d:"
					       " strange dev_index 0x%x\n",
					       pcp->pc_if.if_unit,
					       pb->dev_index);
				freemsg(bp);
				goto again;
			}
			if (!vj_uncomp)
				vj_uncomp = pdp->pd_vj_uncomp;
		}

		/* Everything unrecognized must be an error.
		 * Pass it to the daemon, and kick the VJ header decompressor.
		 */
		if (pb->type != BEEP_FRAME) {
			if (pb->type != BEEP_MP) {
				pcp->pc_if.if_ipackets++;
				pcp->pc_if.if_ierrors++;
				pcp->pc_if.if_ibytes += (msgdsize(bp)
							 - PPP_BUF_HEAD);
				if (vj_uncomp)
					vj_uncomp->flags |= SLF_TOSS;
			}
			putnext(pcp->pc_rq,bp);
			goto again;
		}


		ASSERT(bp->b_wptr >= bp->b_rptr+PPP_BUF_MIN);
		/* save protocol before discarding STREAMS buffer */
		proto = pb->proto;

		switch (proto) {
		case PPP_CP:
			if (!(pcp->pc_b.pb_state & PB_ST_ON_RX_CCP)) {
				putbq(rq,bp);
				goto rcv_stop;
			}
			/* Decompress a CP packet.
			 * XXX this really should be done directly
			 *	to mbufs instead of wasting a copy.
			 *	Except what about non-IP packets?
			 */
			pcp->pc_b.pb_meters.cp_rx++;
			if (pcp->pc_b.pb_state & PB_ST_RX_BSD) {
				bp = pf_bsd_decomp(pcp->pc_rx_bsd,bp);
			} else if (pcp->pc_b.pb_state & PB_ST_RX_PRED) {
				bp = pf_pred1_decomp(pcp->pc_rx_pred,bp);
			}
			if (bp != 0) {
				/* if compression worked, ensure the
				 * protocol makes sense.
				 */
				pb = (struct ppp_buf*)bp->b_rptr;
				ASSERT(bp->b_wptr >= bp->b_rptr+PPP_BUF_MIN);
				ASSERT(PPP_BUF_ALIGN(pb));
				proto = pb->proto;
				if (proto != PPP_VJC_UNCOMP
				    && proto != PPP_VJC_COMP
				    && proto != PPP_IP) {
					pcp->pc_b.pb_meters.cp_rx_bad++;
					pb->type = BEEP_CP_BAD;
					putnext(pcp->pc_rq,bp);
					if (vj_uncomp)
						vj_uncomp->flags |= SLF_TOSS;
					goto again;
				}
				break;
			}

			/* if decompression failed, tell the daemon
			 */
			pcp->pc_b.pb_meters.cp_rx_bad++;
			bp = allocb(PPP_BUF_MIN, BPRI_HI);
			if (!bp)
				goto again;
			pb = (struct ppp_buf*)bp->b_wptr;
			pb->type = BEEP_CP_BAD;
			pb->dev_index = -1;
			bp->b_wptr += PPP_BUF_MIN;
			CK_WPTR(bp);
			pcp->pc_b.pb_state &= ~PB_ST_RX_CP;
			putnext(pcp->pc_rq,bp);
			if (vj_uncomp)
				vj_uncomp->flags |= SLF_TOSS;
			goto again;

		case PPP_CCP:
			if (!pullupmsg(bp,
				       PPP_BUF_MIN+sizeof(pb->un.pkt))) {
				/* if the pullup fails,
				 * let the daemon worry about it.
				 */
				putnext(pcp->pc_rq,bp);
				goto again;
			}
			pb = (struct ppp_buf*)bp->b_rptr;
			switch (pb->un.pkt.code) {
			case PPP_CODE_CR:
			case PPP_CODE_TR:
			case PPP_CODE_RREQ:
				/* Turn off TX compression immediately
				 * upon Configure-Request, Terminate-Request,
				 * or Reset-Request.
				 */
				if (pdp)
					pdp->pd_l.pl_state &= ~ST_TX_CP;
				pcp->pc_b.pb_state &= ~ST_TX_CP;
				break;

			case PPP_CODE_CA:
				/* Stop handling input upon Configure-Ack
				 * to start the compression dicionary.
				 */
				if (pb->un.pkt.id == pcp->pc_b.pb_ccp_id)
					pcp->pc_b.pb_state &= ~PB_ST_ON_RX_CCP;
				break;

			case PPP_CODE_RACK:
				/* Stop handling input upon Reset-Ack
				 * to restart the compression dictionary.
				 */
				if (pcp->pc_b.pb_state & PB_ST_RX_CP)
					pcp->pc_b.pb_state &= ~PB_ST_ON_RX_CCP;
				break;
			}

			/* all CCP must go upstream to the daemon */
			putnext(pcp->pc_rq,bp);
			goto again;


		case PPP_VJC_UNCOMP:
		case PPP_VJC_COMP:
		case PPP_IP:
			if (!(pcp->pc_b.pb_state & PB_ST_ON_RX_CCP)) {
				putbq(rq,bp);
				goto rcv_stop;
			}
			if ((pcp->pc_b.pb_state & PB_ST_RX_BSD)
			    && proto <= MAX_BSD_COMP_PROTO
			    && proto >= MIN_BSD_COMP_PROTO)
				pf_bsd_incomp(pcp->pc_rx_bsd, bp, proto);
			break;

		default:
			/* junk must go upstream */
			putnext(pcp->pc_rq,bp);
			goto again;
		}

		bp->b_rptr += PPP_BUF_MIN;
		totlen = msgdsize(bp);
		pcp->pc_if.if_ibytes += totlen;
		dlen = totlen;

		/* copy from STREAMS buffers to mbufs.
		 * XXX
		 *	vj_uncompress() assumes the first mbuf
		 *	contains no more than MLEN data bytes;
		 */
		mblen = MIN(dlen+CSLIP_HDR, CSLIP_HDR+MLEN);
		m1 = m = m_vget(M_DONTWAIT, mblen, MT_DATA);
		if (!m) {
			/* Treat mbuf allocation error like a data
			 * error and kick the VJ decompressor.
			 */
			freemsg(bp);
			pcp->pc_if.if_iqdrops++;
			if (vj_uncomp)
				vj_uncomp->flags |= SLF_TOSS;
			goto again;
		}
		M_INITIFHEADER(m, &pcp->pc_if, CSLIP_HDR);
		m->m_len = mblen;
		mboff = CSLIP_HDR;
		mblen -= CSLIP_HDR;
		for (;;) {
			while ((len = bp->b_wptr - bp->b_rptr) <= 0) {
				nbp = bp->b_cont;
				freeb(bp);
				bp = nbp;
			}

			len = MIN(mblen, len);
			bcopy(bp->b_rptr, mtod(m1,char*)+mboff, len);

			if (0 >= (totlen -= len)) {
				/* That should be the end of the STREAMS data.
				 */
				ASSERT(msgdsize(bp) == len);
				ASSERT(mblen == len);
				freemsg(bp);
				break;
			}

			bp->b_rptr += len;
			mboff += len;

			if ((mblen -= len) == 0) {
				mblen = MIN(totlen, VCL_MAX);
				m2 = m_vget(M_DONTWAIT, mblen, MT_DATA);
				if (!m2) {
					m_freem(m);
					freemsg(bp);
					pcp->pc_if.if_iqdrops++;
					if (vj_uncomp)
						vj_uncomp->flags |= SLF_TOSS;
					goto again;
				}
				m1->m_next = m2;
				m1 = m2;
				mboff = 0;
			}
		}


		snooph = mtod(m, caddr_t) + CSLIP_HDR;
		if (proto == PPP_VJC_UNCOMP
		    || proto == PPP_VJC_COMP) {
			if (!vj_uncomp) {
				if (pcp->pc_if.if_flags & IFF_DEBUG)
					printf("ppp%d: unexpected VJ"
					       "compressed packet\n",
					       pcp->pc_if.if_unit);
				pcp->pc_if.if_ierrors++;
				m_freem(m);
				goto again;
			}
			dlen = vj_uncompress(m, dlen,
					     (u_char**)&snooph,
					     (proto == PPP_VJC_COMP
					      ? TYPE_COMPRESSED_TCP
					      : TYPE_UNCOMPRESSED_TCP),
					     vj_uncomp);
		}

		/* send the packet up the layers
		 */
#ifdef _MP_NETLOCKS
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined (PPP_IRIX_64)
		IFNET_LOCK(&pcp->pc_if, s);
#else
		IFNET_LOCK(&pcp->pc_if);
#endif
		if (RAWIF_SNOOPING(&pcp->pc_rawif)) {
			(void)snoop_input(&pcp->pc_rawif,
					  0, snooph, m, dlen);
		}
		if (iff_dead(pcp->pc_if.if_flags)) {
			/* it may have been turned off since we checked */
			m_freem(m);
		} else {
#if !defined(PPP_IRIX_53) && !defined(PPP_IRIX_62) && !defined(PPP_IRIX_63) && !defined(PPP_IRIX_64)
			if (network_input(m, AF_INET, 0))
				pcp->pc_if.if_iqdrops++;
#else /* pre-kudzu */
#ifdef PPP_IRIX_64
			IFQ_LOCK(&ipintrq, s1);
#else
			IFQ_LOCK(&ipintrq);
#endif
			if (IF_QFULL(&ipintrq)) {
				pcp->pc_if.if_iqdrops++;
				m_freem(m);
				IF_DROP(&ipintrq);
			} else {
				IF_ENQUEUE_NOLOCK(&ipintrq, m);
				schednetisr(NETISR_IP);
			}
#ifdef PPP_IRIX_64
			IFQ_UNLOCK(&ipintrq, s1);
#else
			IFQ_UNLOCK(&ipintrq);
#endif
#endif /* pre-kudzu */
		}
#if !defined(PPP_IRIX_53) && !defined(PPP_IRIX_62) && !defined(PPP_IRIX_63) && !defined(PPP_IRIX_64)
		IFNET_UNLOCK(&pcp->pc_if);
#else
		IFNET_UNLOCK(&pcp->pc_if, s);
#endif
#else /* _MP_NETLOCKS */
		if (RAWIF_SNOOPING(&pcp->pc_rawif)) {
			(void)snoop_input(&pcp->pc_rawif,
					  0, snooph, m, dlen);
		}
		s = splimp();
		if (iff_dead(pcp->pc_if.if_flags)) {
			/* it may have been turned off since we checked */
			m_freem(m);
		} else if (IF_QFULL(&ipintrq)) {
			pcp->pc_if.if_iqdrops++;
			IF_DROP(&ipintrq);
			m_freem(m);
		} else {
			IF_ENQUEUE(&ipintrq, m);
			schednetisr(NETISR_IP);
		}
		splx(s);
#endif /* _MP_NETLOCKS */
	}
rcv_stop:

	/* ensure the flush timer is running if needed */
	RESTART_BEEP(pcp);

	/* push accumulated control messages toward the PPP daemon */
	for (pdp = pcp->pc_pd_head; pdp != 0; pdp = pdp->pd_next) {
		if (0 != pdp->pd_rq) {
			if (!canput(pcp->pc_rq->q_next))
				break;
			qenable(pdp->pd_rq);
		}
	}
}


/* Deal with accumulated control messages.
 *	This service function is for each individual STREAMS below the mux.
 *	It is enabled by the service function of the mux when it seems
 *	some good might be done.
 */
static void
pd_rsrv(queue_t *rq)
{
	mblk_t *bp;
	struct ppp_dev *pdp;
	struct ppp_cont *pcp;

	pdp = (struct ppp_dev*)rq->q_ptr;
	if (!pdp) {
		/* When caught in the transition of STREAMS muxes,
		 * hope to be re-awakened when things are sorted out.
		 */
#ifdef DEBUG
		printf("pd_rsrv: all dressed up and nowhere\n");
#endif
		return;
	}

	pcp = pdp->pd_pc;

	pdp = (struct ppp_dev*)rq->q_ptr;
	ASSERT(pdp->pd_wq->q_ptr == (caddr_t)pdp);
	pcp = pdp->pd_pc;
	ASSERT(find_pdp(pcp,pdp->pd_l.pl_index) == pdp);

	for (;;) {
		if (!canput(pcp->pc_rq->q_next))
			return;

		bp = getq(rq);
		if (!bp)
			return;

		ppp_rput(rq, bp);
	}
}


/* send a frame
 *	Copy from a simple buffer or a string of mbufs to a string
 *	of STREAMS buffers.  Discard mbufs along the way.
 */
static mblk_t *				/* message to send or 0 if failed */
ppp_oframe(struct ppp_cont *pcp,
	   struct ppp_dev *pdp,
	   int mp_bits,			/* MP header bits + non-0 MP flag */
	   int proto,			/* PPP protocol or 0 */
	   int totlen,			/* data bytes in this fragment */
	   int *pmlim,			/* bytes in this mbuf */
	   struct mbuf **pm,		/* pointer to next mbuf */
	   u_char **prptr)		/* current source */
{
#define ADDBYTE_RAW(b) {if (PPP_ASYNC_MAP((u_char)(b), pdp->pd_tx_accm)) {\
	*wptr++ = PPP_ESC; *wptr++ = (b) ^ PPP_ESC_BIT;			\
	pdp->pd_l.pl_raw_obytes += 2;					\
	} else {							\
		*wptr++ = (b); pdp->pd_l.pl_raw_obytes ++;}}
#define ADDBYTE_FCS(b) {c = (b); ADDBYTE_RAW(c); fcs = PPP_FCS(fcs,c);}
	register u_char c;
	register u_short fcs;
	register u_char *wptr;
	register u_char *rptr = *prptr;
	register int slim;
	register int mlim;
	mblk_t *bp, *bp0, *bp9;

	/* get the first STREAMS buffer
	 */
	slim = totlen + 1+3*2+PPP_BUF_TAIL*2-1 + PPP_MP_MAX_OVHD;
	bp0 = allocb(MIN(slim, PPP_MAX_SBUF), BPRI_LO);
	if (bp0 == 0) {
		m_freem(*pm);
		*pm = 0;
		return 0;
	}

	bp9 = bp = bp0;
	wptr = bp->b_wptr;

	/* Insert leading flag byte on async link only if link has been idle.
	 */
	if (!(pdp->pd_l.pl_state & PL_ST_SYNC)) {
		if (lbolt >= pdp->pd_tx_lbolt) {
			*wptr++ = PPP_FLAG;
			pdp->pd_l.pl_raw_obytes++;
		}
		pdp->pd_tx_lbolt = lbolt + FFLAG_BUSY;
	}
	fcs = PPP_FCS_INIT;

	/* Add Address and Control fields if not compressed, on both
	 * sync and async links.
	 * Do not compress control packets, those with PPP protocol
	 * numbers above 255.
	 */
	if (!(pdp->pd_l.pl_state & PL_ST_ACOMP)
	    || 0 != (proto>>8)) {
		ADDBYTE_FCS(PPP_ADDR);
		ADDBYTE_FCS(PPP_CNTL);
	}

	if (mp_bits != 0) {
		if (!(pdp->pd_l.pl_state & PL_ST_PCOMP))
			ADDBYTE_FCS(PPP_MP>>8);
		ADDBYTE_FCS(PPP_MP);
		slim = pcp->pc_mp.sn_out++;
		if (pcp->pc_b.pb_state & PB_ST_MP_SEND_SSN) {
			slim = ((slim & 0xfff)
				| (mp_bits << (MP_BIT_SHIFT+8)));
		} else {
			ADDBYTE_FCS(mp_bits << MP_BIT_SHIFT);
			ADDBYTE_FCS(slim>>16);
		}
		ADDBYTE_FCS(slim>>8);
		ADDBYTE_FCS(slim);
		if (0 != (proto>>8))
			ADDBYTE_FCS(proto>>8);
		if (proto != 0)		/* check for a null MP fragment */
			ADDBYTE_FCS(proto);

	} else {
		if (!(pdp->pd_l.pl_state & PL_ST_PCOMP)
		    || 0 != (proto>>8))
			ADDBYTE_FCS(proto>>8);
		ADDBYTE_FCS(proto);
	}

	slim = bp->b_datap->db_lim - wptr;
	mlim = MIN(*pmlim,totlen);
	*pmlim -= mlim;
	totlen -= mlim;
	for (;;) {
		/* Stop at the end of the frame or the end of the buffer.
		 * Do not be confused by 0-length mbufs in the middle
		 * or end of the chain.
		 * If there was no mbuf (e.g. for compressed data),
		 * just stop at the end of the frame.
		 * Discard all mbufs that have been completed, including
		 * those at the end of the frame.
		 * Leave the pointers and lengths of the caller ready for
		 * the next frame (e.g. for MP).
		 */
		if (mlim == 0) {
			if (*pmlim == 0) {
				if (*pm != 0)
					*pm = m_free(*pm);
				if (*pm == 0) {
#ifdef DEBUG
					if (totlen != 0)
						printf("ppp%d: premature"
						       " end of buffer;"
						       " totlen=%d\n",
						       pcp->pc_if.if_unit,
						       totlen);
#endif
					goto eop;
				}
				if (0 == (*pmlim = (*pm)->m_len))
					continue;
				rptr = mtod(*pm, u_char*);
			}
			if (totlen == 0)
				goto eop;
			mlim = MIN(*pmlim,totlen);
			totlen -= mlim;
			*pmlim -= mlim;
		}

		if (slim <= 1) {
			bp->b_wptr = wptr;
			CK_WPTR(bp);
			slim = totlen+mlim+PPP_BUF_TAIL*2-1;
			bp = allocb(MIN(slim, PPP_MAX_SBUF), BPRI_LO);
			if (bp == 0) {
				freemsg(bp0);
				m_freem(*pm);
				return 0;
			}
			bp9->b_cont = bp;
			bp9 = bp;
			wptr = bp->b_wptr;
			slim = bp->b_datap->db_lim - wptr;
		}

		/* just copy the bytes for a sync line
		 */
		if (pdp->pd_l.pl_state & PL_ST_SYNC) {
			int len = MIN(slim,mlim);
			bcopy(rptr,wptr,len);
			rptr += len;
			wptr += len;
			slim -= len;
			mlim -= len;
			/* Account for the output, including the FCS
			 * added by the serial driver.
			 */
			pdp->pd_l.pl_raw_obytes += len+2;

		} else {
			do {
				c = *rptr++;
				fcs = PPP_FCS(fcs, c);
				if (PPP_ASYNC_MAP(c, pdp->pd_tx_accm)) {
					*wptr++ = PPP_ESC;
					pdp->pd_l.pl_raw_obytes++;
					--slim;
					c ^= PPP_ESC_BIT;
				}
				*wptr++ = c;
				--mlim;
				pdp->pd_l.pl_raw_obytes++;
			} while (--slim > 1 && mlim != 0);
		}
	}
eop:;

	/* if async, install the FCS and ending flag
	 */
	if (!(pdp->pd_l.pl_state & PL_ST_SYNC)) {
		if (slim < PPP_BUF_TAIL*2-1) {
			bp->b_wptr = wptr;
			CK_WPTR(bp);
			bp = allocb(PPP_BUF_TAIL*2-1, BPRI_LO);
			if (bp == 0) {
				freemsg(bp0);
				return 0;
			}
			bp9->b_cont = bp;
			wptr = bp->b_wptr;
		}
		fcs ^= PPP_FCS_INIT;
		ADDBYTE_RAW(fcs);
		fcs >>= 8;
		ADDBYTE_RAW(fcs);
		*wptr++ = PPP_FLAG;
	}
	bp->b_wptr = wptr;
	CK_WPTR(bp);

	*prptr = rptr;
	return bp0;
#undef ADDBYTE_RAW
#undef ADDBYTE_FCS
}


/* convert mbufs to stream buffers, and ship them to the serial line
 */
static void
ppp_wsrvs(struct ppp_cont *pcp,
	  struct ppp_dev *pdp)
{
	int len, mlim;
	u_char *rptr;
	mblk_t *bp;
	struct mbuf *m;
	int proto, cycle, i, j;
	u_char mp_bits;
	u_long st;

	cycle = pcp->pc_b.pb_pl_num;
	if (cycle != 0) for (;;) {
		if (!pdp)
			pdp = pcp->pc_pd_head;
		if (((pdp->pd_l.pl_state & (PL_ST_TX_LINK
				       | PL_ST_TX_IP
				       | PL_ST_BB_TX))
		     != (PL_ST_TX_LINK | PL_ST_TX_IP))
		    || !canput(pdp->pd_wq->q_next)) {
			pdp = pdp->pd_next;
			/* quit if all output streams are full */
			if (--cycle <= 0)
				break;
			continue;
		}
		cycle = pcp->pc_b.pb_pl_num;

#ifdef _MP_NETLOCKS
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined (PPP_IRIX_64)
		IFNET_LOCK(&pcp->pc_if, s);
#else
		IFNET_LOCK(&pcp->pc_if);
#endif
#else
		s = splimp();
#endif
		IF_DEQUEUE(&pcp->pc_if.if_snd, m);
		if (m == pcp->pc_smbuf) {
			pcp->pc_smbuf = 0;
			pcp->pc_b.pb_starve = 0;
		}
#ifdef _MP_NETLOCKS
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined (PPP_IRIX_64)
		IFNET_UNLOCK(&pcp->pc_if, s);
#else
		IFNET_UNLOCK(&pcp->pc_if);
#endif
#else
		splx(s);
#endif
		if (0 == m) {
			pcp->pc_b.pb_state |= PB_ST_TX_DONE;
			if (!(pdp->pd_l.pl_state & PL_ST_SYNC))
				pdp->pd_tx_lbolt = MIN(pdp->pd_tx_lbolt,
						       lbolt + FFLAG_IDLE);
			break;
		}


		len = m_length(m);
		if (RAWIF_SNOOPING(&pcp->pc_rawif)) {
			struct mbuf *n;

			n = m_vget(M_DONTWAIT, len+SNLEN,
				   MT_DATA);
			if (n == 0) {
				snoop_drop(&pcp->pc_rawif, SN_PROMISC,
					   mtod(m,caddr_t), m->m_len);
			} else {
				IF_INITHEADER(mtod(n,caddr_t),
					      &pcp->pc_if,
					      SNLEN);
				(void)m_datacopy(m, 0, len,
						 mtod(n,caddr_t)+SNLEN);
				(void)snoop_input(&pcp->pc_rawif,
						  SN_PROMISC,
						  mtod(m,caddr_t),
						  n, len);
			}
		}
		pcp->pc_if.if_obytes += len;
		pcp->pc_if.if_opackets++;


		if ((*mtod(m,char*) & 0xf0) == (IPVERSION << 4)
		    && pcp->pc_vj_comp != 0) {
			st = pcp->pc_b.pb_state;
			switch (vj_compress(&m, pcp->pc_vj_comp, &len,
					    (!(st & PB_ST_MP)
					     ? pdp->pd_l.pl_link_num : 0),
					    st & PB_ST_VJ,
					    st & PB_ST_VJ_SLOT)) {
			case TYPE_UNCOMPRESSED_TCP:
				proto = PPP_VJC_UNCOMP;
				break;
			case TYPE_COMPRESSED_TCP:
				proto = PPP_VJC_COMP;
				break;
			default:
				if (!m) {
					/* give up on packet if header
					 * compression (usually a pullup())
					 * fails.
					 */
					pcp->pc_if.if_oerrors++;
					continue;
				}
				proto = PPP_IP;
				break;
			}
		} else {
			proto = PPP_IP;
		}

		/* Compress into the static buffer.
		 * There is no need to check that the protocol is
		 * legally compressible because this code only deals with IP.
		 *
		 * If the packet does not compress, send it uncompressed.
		 */
		if ((pdp->pd_l.pl_state | pcp->pc_b.pb_state) & ST_TX_BSD) {
			struct bsd_db *bsd_db = (pcp->pc_tx_bsd
						 ? pcp->pc_tx_bsd
						 : pdp->pd_tx_bsd);
			mlim = pf_bsd_comp(bsd_db,
					   pcp->pc_tx_cp_buf,
					   proto, m, len);
			if (mlim <= len) {
				len = mlim;
				rptr = pcp->pc_tx_cp_buf;
				m_freem(m);
				m = 0;
				proto = PPP_CP;
			} else {
				bsd_db->incomp_count++;
				mlim = m->m_len;
				rptr = mtod(m, u_char*);
			}
		} else if (((pdp->pd_l.pl_state
			     | pcp->pc_b.pb_state) & ST_TX_PRED)
			   && len <= pcp->pc_b.pb_mtru-PRED_OVHD) {
			/* Attempt Predictor Type 1 only if the packet
			 * is small enough to be sent as an incompressable
			 * Predictor Type 1 packet in case it fails to shrink.
			 */
			mlim = pf_pred1_comp(pcp->pc_tx_pred
					     ? pcp->pc_tx_pred
					     : pdp->pd_tx_pred,
					     pcp->pc_tx_cp_buf,
					     proto, m, len,
					     pdp->pd_l.pl_state & PL_ST_PCOMP);
			len = mlim;
			rptr = pcp->pc_tx_cp_buf;
			m_freem(m);
			m = 0;
			proto = PPP_CP;

		} else {
			mlim = m->m_len;
			rptr = mtod(m, u_char*);
		}

		i = len;
		if (pcp->pc_mp.out_head == 0) {
			mp_bits = 0;
		} else {
			/* pick target MP fragment length for this packet */
			if (i > pcp->pc_mp.min_frag) {
				i = ((i+pcp->pc_b.pb_pl_num-1)
				     / pcp->pc_b.pb_pl_num);
				if (i < pcp->pc_mp.min_frag)
					i = pcp->pc_mp.min_frag;
			}
			mp_bits = MP_B_BIT | ~(MP_B_BIT | MP_E_BIT);
		}

		/* fragment the packet among the links
		 */
		for (;;) {
			/* limit next frag length to MP MTU for this link
			 * link if we are are using MP headers.
			 */
			j = i;
			if (j > pdp->pd_l.pl_max_frag)
				j = pdp->pd_l.pl_max_frag;
			if (j >= len) {
				j = len;
				if (mp_bits != 0)
					mp_bits |= MP_E_BIT;
			}
			bp = ppp_oframe(pcp,pdp,mp_bits,proto,j,
					&mlim,&m,&rptr);
			if (!bp) {
				pcp->pc_if.if_oerrors++;
				pcp->pc_if.if_odrops++;
				break;
			}
			/* trash a frame periodically to stress things */
			if (if_ppp_test != 0
			    && ++if_ppp_test_cnt >= if_ppp_test) {
				if_ppp_test_cnt = 0;
				freemsg(bp);
			} else {
				putnext(pdp->pd_wq, bp);
			}

			/* This link is busy and so does not need a null */
			pdp->pd_l.pl_state &= ~PL_ST_NULL_NEED;

			/* spread the load evenly */
			pdp = pdp->pd_next;
			if (!pdp)
				pdp = pcp->pc_pd_head;

			/* stop blackballing the new link after turning
			 * on MP headers when going from 1 to 2 links.
			 */
			pdp->pd_l.pl_state &= ~PL_ST_BB_TX;

			/* quit at end of packet */
			if ((len -= j) == 0) {
				m_freem(m);
				break;
			}

			/* Pick a link in MP bundle for the next fragment.
			 * If all links are busy, use the one we started
			 * with.
			 */
			for (;;) {
				if (((pdp->pd_l.pl_state & (PL_ST_TX_LINK
							| PL_ST_TX_IP
							| PL_ST_BB_TX))
				     == (PL_ST_TX_LINK | PL_ST_TX_IP))
				    && (canput(pdp->pd_wq->q_next)
					|| cycle <= 0))
					break;
				--cycle;
				pdp = pdp->pd_next;
				if (!pdp)
					pdp = pcp->pc_pd_head;
			}
			cycle = pcp->pc_b.pb_pl_num;

			mp_bits &= ~MP_B_BIT;
			pcp->pc_b.pb_state |= (PB_ST_NEED_NULLS|PB_ST_MP_TIME);
			proto = 0;
		}

	}
	pcp->pc_pd_cur = pdp;

	/* send MP nulls
	 */
	if (pcp->pc_mp.out_head != 0) {
		for (pdp = pcp->pc_pd_head; pdp != 0; pdp = pdp->pd_next) {
			if (((pdp->pd_l.pl_state & (PL_ST_TX_LINK
						    | PL_ST_BB_TX
						    | PL_ST_NULL_NEED))
			     == (PL_ST_TX_LINK | PL_ST_NULL_NEED))
			    && canput(pdp->pd_wq->q_next)) {
				pdp->pd_l.pl_state &= ~PL_ST_NULL_NEED;
				mlim = 0;
				m = 0;
				rptr = 0;
				bp = ppp_oframe(pcp,pdp, MP_B_BIT|MP_E_BIT,
						0, 0, &mlim, &m, &rptr);
				if (!bp)
					break;
				putnext(pdp->pd_wq, bp);
				pcp->pc_pd_head->pd_l.pl_state &= ~PL_ST_BB_TX;
			}
		}
	}

	RESTART_BEEP(pcp);
}


/* write-service function for the main module
 *	Run for the first packet after idleness.
 */
static void
pc_wsrv(queue_t *wq)
{
	struct ppp_cont *pcp = (struct ppp_cont*)wq->q_ptr;

	if (!pcp)
		return;			/* quit if stream dead */

	ASSERT(pcp->pc_wq == wq);
	ASSERT(pcp->pc_rq->q_ptr == (caddr_t)pcp);

	ppp_wsrvs(pcp, pcp->pc_pd_cur);
}


/* write-service function for the lower streams
 *	Run as the result of "back enabling" from downstream.
 */
static void
pd_wsrv(queue_t *wq)
{
	struct ppp_dev *pdp;
	struct ppp_cont *pcp;

	pdp = (struct ppp_dev*)wq->q_ptr;
	if (!pdp)			/* quit if stream dead */
		return;

	ASSERT(pdp->pd_rq->q_ptr == (caddr_t)pdp);
	pcp = pdp->pd_pc;
	ASSERT(find_pdp(pcp,pdp->pd_l.pl_index) == pdp);

	ppp_wsrvs(pcp,pdp);
}
