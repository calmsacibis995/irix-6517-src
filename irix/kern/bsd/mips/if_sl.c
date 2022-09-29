/* Serial Line IP
 *
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.82 $"

#include "sys/param.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/stream.h"
#include "sys/strmp.h"
#include "sys/strids.h"
#include "sys/stropts.h"
#include "sys/termio.h"
#include "sys/stty_ld.h"
#include "sys/systm.h"
#include "sys/conf.h"
#include "sys/ddi.h"
#include "bstring.h"

#include "net/if.h"
#include "net/if_types.h"
#include "net/raw.h"
#include "net/netisr.h"
#include "net/soioctl.h"
#include "net/route.h"
#include "netinet/in.h"
#include <sys/hashing.h>
#include "netinet/in_var.h"
#include "netinet/in_systm.h"		/* XXX required before ip.h */
#include "netinet/ip.h"
#include "netinet/ip_icmp.h"
#include "netinet/tcp.h"
#include "netinet/udp.h"
#include "ether.h"

#include "sys/if_sl.h"
#include "net/slcompress.h"


/* lboot values	*/
extern int if_sl_ifq_maxlen;
#define SLIP_IFQ_MAXLEN 300
extern int if_sl_bigxmit;		/* bigger packets gets slow service */
#define BIGXMIT 128
extern int if_sl_maxout;		/* limit output bursts to this */

#define MAX_LINK 64			/* must be < 128 for TCGETA */

#define MINIP	sizeof(struct ip)	/* smallest packet in normal mode */
#define MINIPCOMP sizeof(struct cp_hdr)	/* smallest when compressing */

#define SL_RMTU	2048			/* maximum permitted */
#define BAD_RMTU (SL_RMTU+1)
#define SL_TMTU	1006			/* traditional */


static struct module_info stm_info = {
    STRID_IF_SL,			/* module ID */
    "if_sl",				/* module name */
    0,					/* minimum packet size */
    INFPSZ,				/* infinite maximum packet size	*/
    8*1024,				/* hi-water mark */
    0,					/* lo-water mark */
    0,
};

static void sl_rput(queue_t*, mblk_t*);
static void sl_rsrv(queue_t*);
static int sl_open(queue_t*, dev_t*, int, int, struct cred*);
static sl_close(queue_t*, int, struct cred*);
static void sl_wput(queue_t*, mblk_t*);
static void sl_wsrv(queue_t*);

static struct qinit sl_rinit = {
	(int (*)(queue_t*,mblk_t*))sl_rput,
	(int (*)(queue_t*))sl_rsrv, sl_open, sl_close, 0, &stm_info, 0
};
static struct qinit sl_winit = {
	(int (*)(queue_t*,mblk_t*))sl_wput,
	(int (*)(queue_t*))sl_wsrv, 0, 0, 0, &stm_info, 0
};
static struct qinit sl_muxrinit = {
	(int (*)(queue_t*,mblk_t*))sl_rput,
	(int (*)(queue_t*))sl_rsrv, 0, 0, 0, &stm_info, 0
};
static struct qinit sl_muxwinit = {
	(int (*)(queue_t*,mblk_t*))sl_wput,
	(int (*)(queue_t*))sl_wsrv, 0, 0, 0, &stm_info, 0
};

int if_sldevflag = D_MP;

struct streamtab if_slinfo = {
	&sl_rinit, &sl_winit,
	&sl_muxrinit, &sl_muxwinit
};


/* header-compression data
 *	This stuff is updated by TCP/IP traffic, and is used to
 *	compress headers
 */
struct cp_hdr {
	struct ip ip;
	struct tcphdr tcp;
};

/* space for an expanding header, in an mbuf
 */
struct cp_ih {
	struct ifheader ifh;
	struct snoopheader snoop;
	struct cp_hdr	h;
};

/* non-expanding (snoop) header */
#define SNLEN (sizeof(struct ifheader)+sizeof(struct snoopheader))

/* shape of "comp" compression database */
struct cp_db {
	struct cp_db	*db_next;
	struct cp_hdr	db_hdr;
	struct {			/* psuedo-header for TCP checksum */
		struct in_addr ts_src;
		struct in_addr ts_dst;
		u_char	ts_x1;
		u_char	ts_pr;
		u_short	ts_len;
	}		db_ts;
	u_int		max_window;
	u_int		min_window;
	time_t		db_lbolt;
	u_char		db_ses;
};

#define SES_LN		2		/* bits for 'sessions' */
#define ACK_LN		6		/* remaining bits for ACKs */
#define NUM_COMP_SES	(1<<SES_LN)	/* # of simultaneous compressions */
#define RX_COMP0	NUM_COMP_SES
#define MAX_COMP_LEN	(MINIPCOMP-1)
#define MAX_COMP_DATA	(MAX_COMP_LEN - sizeof(struct cp))
#define MAX_COMP_ACK	((1<<ACK_LN)-1)	/* largest compressed ack seq # */

#define GET_ACK(ses_ack) ((ses_ack)>>SES_LN)
#define GET_SES(ses_ack) ((ses_ack) & (NUM_COMP_SES-1))
#define BAD_SES_ACK	0xff


/* a compressed header */
struct cp {
	u_char	cksum;
	u_char	ses_ack;
};

/* bytes in front of CSLIP data.
 *	This value includes space for a TCP/IP header to be to be
 *	uncompressed in front of the TCP data.
 */
#define CSLIP_HDR (MAX_HDR+SNLEN)



/* everything there is to know about each if_sl interface
 */
static struct sl_cont {
	struct	ifnet sl_if;
	struct	sl_cont *sl_next;
	queue_t	*sl_rq;			/* our queues */
	queue_t	*sl_wq;
	queue_t	*sl_data_rq;		/* the device below */
	queue_t *sl_data_wq;
	int	sl_dl_index;
	struct	mbuf *sl_wmbuf;		/* current output mbuf chain */
	struct	mbuf *sl_smbuf;		/* last small msg in output queue */
	struct	mbuf *sl_rmbase;	/* growing input mbuf chain */
	struct	mbuf *sl_rmtail;
	int	sl_bigxmit;		/* bigger packets gets slow service */
	int	sl_bigpersmall;		/* ratio of big to small packets */
	int	sl_maxout;		/* limit output bursts to this */
	int	sl_rlim;		/* limit on input mbuf length */
	int	sl_wlen;		/* bytes waiting to go out */
	int	sl_starve;		/* small packets leapfrogged */
	time_t	sl_tx_lbolt;		/* when we last transmitted */
	short	sl_rlen;
	u_char	sl_resc;		/* waiting for byte after FRAME_ESC */

	u_char	sl_type;		/* compression & so forth */
	u_char	sl_mode;		/* mode bits from daemon */
	u_char	sl_state;		/* misc. state bits */

	struct {			/* output comp buffer or checksum */
		struct cp cp;
		u_char	framend;
	} sl_cbuf;
	u_char	sl_clen;		/* compressed or cksum length */
	u_char	*sl_cptr;		/* pointer into compression buffer */

	struct {
		u_int	cksum;		/* bad checksum */
		u_int	escs;		/* escaped bytes */
		u_int	fail;		/* uncompressed packets */
		u_int	success;	/* compressed packets */
		u_int	proto;		/* wrong protocol for compression */
		u_int	poison;		/* expansion poisoned */
		u_int	flush;		/* expansion buffers flushed */
	} sl_imeters;
	struct {
		u_int	escs;		/* escaped bytes */
		u_int	allocb;		/* total streams buffers sent */
		u_int	allocb_fails;	/* failures by str_allocb() */
		u_int	empties;	/* empty frames to flush trash */
		u_int	fail;		/* total uncompressed packets */
		u_int	success;	/* compressed packets */
		u_int	new;		/* new 'session' */
		u_int	stale;		/* refreshes of stale predictors */
		u_int	small;		/* too small to compress */
		u_int	proto;		/* wrong protocol */
		u_int	big;		/* too big to compress */
		u_int	frag;		/* IP fragment */
		u_int	big_ack;	/* ACK too big */
		u_int	no_ack;		/* no ACK bit, but ack > 0 */
		u_int	probe_ack;	/* ACK bit, but ack==0 */
		u_int	no_push;	/* no PUSH bit, but user data */
		u_int	stray_push;	/* PUSH bit, but no user data */
		u_int	retry;		/* unexpected sequence # */
		u_int	ip_tos;		/* unexpected TOS */
		u_int	ttl;		/* unexpected TTL */
		u_int	window;		/* window change */
		u_int	ip_opts;	/* IP options */
		u_int	tcp_flags;	/* unexpected TCP flag bits */
		u_int	tcp_urg;	/* urgent data */
		u_int	tcp_opts;	/* TCP options */
		u_int	move2f;		/* small packets moved ahead of big */
		u_int	move2f_leak;	/* big packets leaked */
	} sl_ometers;

	struct cp_db	*sl_xmt_head;	/* "comp" compression database */
	struct cp_db	*sl_db;

	struct vj_comp *sl_vj_comp;	/* "cslip" compression data */
	struct vj_uncomp *sl_vj_uncomp;

	struct afilter	*sl_afilter;	/* port activity filter */

	struct rawif	sl_rawif;	/* for snooping */
} *sl_first = 0;
static int sl_num = 0;

static int sl_ioctl(struct ifnet*, int, void*);
static int sl_output(struct ifnet*, struct mbuf*, struct sockaddr*,
	struct rtentry *);
static void sl_poison(struct sl_cont*);


/* bits in sl_state */
#define SL_ST_BEEP	0x01		/* tell driver things are awake */
#define SL_ST_BEEP_TIME	0x02		/* beep timer running */

#define FORCE_STALE	(HZ*60*5)	/* refresh predictor this often */
#define FORCE_END	(HZ/2)		/* force frame-end this often */

#define FRAME_MASK	0340		/* mask to speed detection */
#define FRAME_END	0300		/* Frame End */
#define FRAME_ESC	0333		/* Frame Esc */
#define ESC_FRAME_END	0334		/* escaped frame end */
#define ESC_FRAME_ESC	0335		/* escaped frame esc */
#define FRAME_HIT	(FRAME_END & FRAME_MASK)


#define KSALLOC(s,n) ((struct s*)kern_malloc(sizeof(struct s)*(n)))


static char *sl_name = "sl";



static void
sl_set_mtu(struct sl_cont *slp,
	   int mtu)
{
	int j;

	if (mtu > MINIPCOMP && mtu <= SL_RMTU)
		slp->sl_if.if_mtu = mtu;
	else
		mtu = slp->sl_if.if_mtu;

	j = if_sl_ifq_maxlen;
	if (j <= 2)
		j = IFQ_MAXLEN;

	if (j > SLIP_IFQ_MAXLEN)
		j = SLIP_IFQ_MAXLEN;
	slp->sl_if.if_snd.ifq_maxlen = j;

	j = if_sl_bigxmit ? if_sl_bigxmit : (mtu/3);
	if (j <= MINIP)
		j = MINIP+1;
	slp->sl_bigxmit = j;

	slp->sl_bigpersmall = mtu/j+1;

	slp->sl_maxout = MAX(if_sl_maxout, slp->sl_bigxmit);
}


/* Turn off the socket interface
 *	the interface must already be locked.
 */
static void
sl_down(struct sl_cont *slp)
{
	struct mbuf *m;

	slp->sl_if.if_flags &= ~IFF_ALIVE;
	if_down(&slp->sl_if);
	for (;;) {
		IF_DEQUEUE(&slp->sl_if.if_snd, m);
		if (!m)
			break;
		m_freem(m);
	}
}


/* process an ioctl from the socket side
 */
static int				/* 0 or error */
sl_ioctl(struct ifnet *ifp,
	 int cmd,
	 void *data)
{
#define slp ((struct sl_cont*)ifp)	/* keep -fullwarn happy */
	int error = 0;
	int flags;
	ASSERT(IFNET_ISLOCKED(ifp));

	ASSERT(slp->sl_if.if_name == sl_name);

	switch (cmd) {
	case SIOCSIFADDR:
		if (_IFADDR_SA(data)->sa_family == AF_INET) {
			if (slp->sl_rq)
				slp->sl_if.if_flags |= IFF_ALIVE;
		} else {
			error = EAFNOSUPPORT;
		}
		break;

	case SIOCSIFDSTADDR:
		break;

	case SIOCSIFFLAGS:
		flags = ((struct ifreq *)data)->ifr_flags;
		if (flags & IFF_UP) {

			if ((slp->sl_if.in_ifaddr != 0) && slp->sl_rq) {
				slp->sl_if.if_flags = flags | IFF_ALIVE;
			} else {
				error = EINVAL;
			}
		} else {
			sl_down(slp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (((struct sockaddr *)data)->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	case SIOC_SL_AFILTER:
		IFNET_UNLOCK(ifp);
		if (!slp->sl_afilter)
			slp->sl_afilter = KSALLOC(afilter,1);
		if (!slp->sl_afilter) {
			error = ENOMEM;
		} else {
			error = copyin((void*
					)(__psunsigned_t)*(__uint32_t*)data,
				       slp->sl_afilter,
				       sizeof(*slp->sl_afilter));
		}
		IFNET_LOCK(ifp);
		break;

	default:
		error = EINVAL;
	}

	return error;
#undef slp
}


/* poke the daemon
 *	This is called only while we have the STREAMS monitor.
 */
static void
sl_beep(struct sl_cont *slp)
{
	mblk_t *bp;

	slp->sl_state &= ~SL_ST_BEEP_TIME;

	if (!slp->sl_rq
	    || !(slp->sl_state & SL_ST_BEEP))
		return;

	if (!canput(slp->sl_rq->q_next)
	    || 0 == (bp = allocb(1,BPRI_LO))) {
		(void)STREAMS_TIMEOUT(sl_beep, slp, BEEP);
		slp->sl_state |= SL_ST_BEEP_TIME;
		return;
	}

	if (slp->sl_type == SC_CSLIP
	    && slp->sl_vj_comp->actconn <= 0) {
		*bp->b_wptr++ = BEEP_WEAK;
	} else {
		*bp->b_wptr++ = BEEP_ACTIVE;
	}
	putnext(slp->sl_rq, bp);

	slp->sl_state &= ~SL_ST_BEEP;

	/* be sure to note any traffic that happens been now and
	 * before the end of the next BEEP ticks.
	 */
	(void)STREAMS_TIMEOUT(sl_beep, slp, BEEP);
	slp->sl_state |= SL_ST_BEEP_TIME;
}


/* called through streams_interrupt to do an MP-safe qenable
 */
static void
sl_wq_qenable(struct sl_cont *slp)
{
	int s;

	STR_LOCK_SPL(s);
	if (slp->sl_wq != 0)
		qenable(slp->sl_wq);
	STR_UNLOCK_SPL(s);
}



/* take an mbuf chain from above and see about sending it down the stream
 */
/* ARGSUSED */
static int
sl_output(struct ifnet *ifp,
	  struct mbuf *m0,
	  struct sockaddr *dst,
	  struct rtentry *rte)
{
#define slp ((struct sl_cont*)ifp)	/* keep -fullwarn happy */
#define PEEP_LEN 4			/* need to peep this far to filter */
	int totlen;
	struct ip* ip;
	int hlen;

	ASSERT(slp->sl_if.if_name == sl_name);

	if (dst->sa_family != AF_INET) {
		if (slp->sl_if.if_flags & IFF_DEBUG)
			printf("sl%d: af%u not supported\n",
			       slp->sl_if.if_unit, dst->sa_family);
		m_freem(m0);
		return (EAFNOSUPPORT);
	}


	ASSERT(IFNET_ISLOCKED(ifp));

	if (iff_dead(slp->sl_if.if_flags)) {
		slp->sl_if.if_odrops++;
		IF_DROP(&slp->sl_if.if_snd);
		m_freem(m0);
		return(ENOBUFS);
	}

	totlen = m_length(m0);
	hlen = MIN(totlen, 15*4+PEEP_LEN);
	if (m0->m_len < hlen) {
		m0 = m_pullup(m0, hlen);
		if (!m0) {
			slp->sl_if.if_odrops++;
			IF_DROP(&slp->sl_if.if_snd);
			return(ENOBUFS);
		}
	}

	ip = mtod(m0,struct ip*);
	hlen = ip->ip_hl<<2;
	if (totlen < PEEP_LEN+hlen) {
		slp->sl_state |= SL_ST_BEEP;

	} else if (ip->ip_p == IPPROTO_ICMP) {
		/* optionally drop ICMP traffic */
		struct icmp *icmp;

		icmp = (struct icmp*)&((char*)ip)[hlen];
		if (!(slp->sl_mode & SC_NOICMP)
		    && !IGNORING_ICMP(slp->sl_afilter, icmp->icmp_type)) {
			/* note activity	*/
			slp->sl_state |= SL_ST_BEEP;

		} else if (!slp->sl_data_wq
			   || (slp->sl_mode & SC_NOICMP)) {
			slp->sl_if.if_odrops++;
			IF_DROP(&slp->sl_if.if_snd);
			m_freem(m0);
			if (slp->sl_if.if_flags & IFF_DEBUG)
				printf("sl%d: drop ICMP type %d packet\n",
				       slp->sl_if.if_unit,
				       icmp->icmp_type);
			return(0);
		}

	} else if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP) {
		/* Cheat, knowing that the UDP and TCP headers start
		 * the same.
		 */
		struct udphdr *udp;

		udp = (struct udphdr*)&((char*)ip)[hlen];
		if (!IGNORING_PORT(slp->sl_afilter,
				   udp->uh_sport)
		    && !IGNORING_PORT(slp->sl_afilter,
				      udp->uh_dport)) {
			/* Count it as activity */
			slp->sl_state |= SL_ST_BEEP;
		} else if (!slp->sl_data_wq) {
			slp->sl_if.if_odrops++;
			IF_DROP(&slp->sl_if.if_snd);
			m_freem(m0);
			if (slp->sl_if.if_flags & IFF_DEBUG)
				printf("sl%d: drop %s packet"
				       " from port %d to port %d\n",
				       slp->sl_if.if_unit,
				       ip->ip_p==IPPROTO_TCP ? "TCP" : "UDP",
				       (int)udp->uh_sport,
				       (int)udp->uh_dport);
			return(0);
		}

	} else {
		slp->sl_state |= SL_ST_BEEP;
	}

	if (IF_QFULL(&slp->sl_if.if_snd)) {
		struct mbuf *om;

		slp->sl_if.if_odrops++;
		IF_DROP(&slp->sl_if.if_snd);
		/* drop oldest packet */
		IF_DEQUEUE(&slp->sl_if.if_snd, om);
		if (om == slp->sl_smbuf) {
			slp->sl_smbuf = 0;
			slp->sl_starve = 0;
		}
		slp->sl_wlen -= m_length(om);
		m_freem(om);
	}

	slp->sl_wlen += totlen;
	if (totlen <= slp->sl_bigxmit) {
		/* put small packets at the front of the queue, but
		 *	after older small packets.
		 */
		if (slp->sl_if.if_snd.ifq_tail == 0
		    || slp->sl_if.if_snd.ifq_tail == slp->sl_smbuf) {
			IF_ENQUEUE(&slp->sl_if.if_snd, m0);
			slp->sl_smbuf = m0;
		} else {
			METER(slp->sl_ometers.move2f++);
			if (slp->sl_smbuf == 0) {
				IF_PREPEND(&slp->sl_if.if_snd, m0);
			} else {
				m0->m_act = slp->sl_smbuf->m_act;
				slp->sl_smbuf->m_act = m0;
				slp->sl_if.if_snd.ifq_len++;
			}
			/* occassionally, move the end of the small-packet
			 *	part of the queue past a big packet.  This
			 *	prevents starvation of big-packet users.
			 */
			if (++slp->sl_starve > slp->sl_bigpersmall) {
				METER(slp->sl_ometers.move2f_leak++);
				slp->sl_starve = 0;
				slp->sl_smbuf = m0->m_act;
			} else {
				slp->sl_smbuf = m0;
			}
		}
	} else {
		if (slp->sl_if.if_snd.ifq_tail == 0
		    || slp->sl_smbuf == 0) {
			slp->sl_starve = 0;
		}
		IF_ENQUEUE(&slp->sl_if.if_snd, m0);
	}

	/* On first packet or when we are shut down, poke the driver
	 * and daemon.
	 */
	if (slp->sl_if.if_snd.ifq_len == 1
	    || slp->sl_data_wq == 0) {
		/* qenable() now sometimes calls the target function */
		IFNET_UNLOCK(ifp);
		streams_interrupt((strintrfunc_t)sl_wq_qenable,slp,0,0);
		IFNET_LOCK(ifp);
	}

	return 0;
#undef slp
}


/* open SLIP STREAMS module
 *	Be either a multiplexing driver for new style or simple module
 *	for old style.
 */
/* ARGSUSED */
static int				/* 0, minor #, or failure reason */
sl_open(queue_t *rq,			/* our read queue */
	dev_t *devp,			/* full device number */
	int flag,
	int sflag,
	struct cred *crp)
{
	struct sl_cont *slp, **slpp;
	queue_t *wq = WR(rq);
	int s;


	if (!sflag)			/* either clone or module */
		return EINVAL;

	if (0 == rq->q_ptr) {		/* initialize on the 1st open */
		if (drv_priv(crp))	/* only root can add network links */
			return EPERM;

		STR_LOCK_SPL(s);
		/* look for an available slot */
		slpp = &sl_first;
		for (;;) {
			slp = *slpp;
			if (!slp) {
				if (sl_num >= MAX_LINK) {
					STR_UNLOCK_SPL(s);
					return EBUSY;
				}
				/* make one if needed and possible */
				slp = KSALLOC(sl_cont,1);
				if (!slp) {
					STR_UNLOCK_SPL(s);
					return ENOMEM;
				}
				bzero(slp,sizeof(*slp));
				*slpp = slp;
				slp->sl_if.if_name = sl_name;
				slp->sl_if.if_unit = sl_num++;
				slp->sl_if.if_type = IFT_SLIP;
				slp->sl_if.if_ioctl = sl_ioctl;
				slp->sl_if.if_output = sl_output;
				if_attach(&slp->sl_if);
				rawif_attach(&slp->sl_rawif,
					     &slp->sl_if, 0, 0, 0, 0, 0, 0);
			}
			if (!slp->sl_rq)
				break;
			slpp = &slp->sl_next;
		}

		rq->q_ptr = (caddr_t)slp;
		wq->q_ptr = (caddr_t)slp;
		slp->sl_state = 0;
		slp->sl_dl_index = -1;
		slp->sl_rq = rq;
		slp->sl_wq = wq;
		if (sflag == MODOPEN) {
			slp->sl_data_wq = wq;
			slp->sl_data_rq = rq;
		}

		slp->sl_maxout = BIGXMIT;
		slp->sl_bigxmit = BIGXMIT;
		slp->sl_cbuf.framend = FRAME_END;
		/* ignore everything until the first framing character */
		slp->sl_rlen = BAD_RMTU;
		slp->sl_tx_lbolt = 0;

		slp->sl_type = SC_STD;
		slp->sl_mode = 0;

		if (slp->sl_afilter)
			bzero(slp->sl_afilter, sizeof(*slp->sl_afilter));

		slp->sl_if.if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		slp->sl_if.if_mtu = SL_TMTU;
		slp->sl_if.if_snd.ifq_maxlen= IFQ_MAXLEN;
		slp->sl_if.if_snd.ifq_drops = 0;
		slp->sl_if.if_oerrors = 0;
		slp->sl_if.if_opackets = 0;
		slp->sl_if.if_obytes = 0;
		slp->sl_if.if_ierrors = 0;
		slp->sl_if.if_ipackets = 0;
		slp->sl_if.if_ibytes = 0;
		slp->sl_if.if_iqdrops = 0;
		slp->sl_if.if_odrops = 0;
		bzero(&slp->sl_imeters, sizeof(slp->sl_imeters));
		bzero(&slp->sl_ometers, sizeof(slp->sl_ometers));
		STR_UNLOCK_SPL(s);
		if (sflag != MODOPEN)
			*devp = makedevice(getemajor(*devp),
					   slp->sl_if.if_unit);
	}

	return 0;
}



/* flush input or output
 */
static void
sl_flush(u_char op,
	 struct sl_cont *slp)
{
	struct mbuf *m;
	queue_t *wq;
	int s;

	if (op & FLUSHW) {		/* free partially sent packet */
		STR_LOCK_SPL(s);
		if (0 != (wq = slp->sl_wq)) {
			flushq(wq,FLUSHALL);
			enableok(wq);
			str_unbcall(wq);
		}
		m = slp->sl_wmbuf;
		slp->sl_wmbuf = 0;
		STR_UNLOCK_SPL(s);
		if (m != 0) {
			m_freem(m);
			slp->sl_if.if_odrops++;
		}
		slp->sl_if.if_odrops += if_dropall(&slp->sl_if.if_snd);
		slp->sl_wlen = 0;
		slp->sl_smbuf = 0;
		slp->sl_starve = 0;
	}

	if (op & FLUSHR) {		/* free partially received packet */
		STR_LOCK_SPL(s);
		if (slp->sl_rq)
			flushq(slp->sl_rq,FLUSHALL);
		if (slp->sl_data_rq)
			flushq(slp->sl_data_rq,FLUSHALL);
		slp->sl_rlen = BAD_RMTU;
		slp->sl_clen = 0;
		m = slp->sl_rmbase;
		slp->sl_rmbase = 0;
		slp->sl_rmtail = 0;
		STR_UNLOCK_SPL(s);
		if (m != 0)
			m_freem(m);
	}
}



/* close SLIP STREAMS module */
/* ARGSUSED */
static
sl_close(queue_t *rq, int flag, struct cred *crp)
{
	struct sl_cont *slp = (struct sl_cont*)rq->q_ptr;
	int s;

	ASSERT(slp->sl_rq == rq);
	ASSERT(slp->sl_wq->q_ptr == (caddr_t)slp);

	IFNET_LOCK(&slp->sl_if);
	(void)in_rtinit(&slp->sl_if);

	sl_down(slp);
	IFNET_UNLOCK(&slp->sl_if);
	sl_flush(FLUSHRW, slp);		/* flush everything */

	STR_LOCK_SPL(s);
	rq->q_ptr = 0;
	slp->sl_wq->q_ptr = 0;
	slp->sl_rq = 0;
	slp->sl_wq = 0;
	slp->sl_data_wq = 0;
	slp->sl_data_rq = 0;
	if (slp->sl_afilter != 0) {
		kern_free(slp->sl_afilter);
		slp->sl_afilter = 0;
	}
	STR_UNLOCK_SPL(s);
	return 0;			/* XXX it is really a void */
}



/* handle streams packets from the stream head.  They should be only IOCTLs.
 */
static void
sl_wput(queue_t *wq,			/* our write queue */
	mblk_t *bp)
{
	struct sl_cont *slp = (struct sl_cont*)wq->q_ptr;
	struct iocblk *iocp;
	struct linkblk *linkblkp;
	struct cp_db *cp, **cp0;
	int i,j;
	int s;

	/* only the "controlling stream" gets IOCTLs from above */
	ASSERT(slp->sl_wq == wq);
	ASSERT(slp->sl_rq->q_ptr == (caddr_t)slp);

	switch (bp->b_datap->db_type) {
	case M_DATA:			/* data does not come this way */
	case M_DELAY:			/* do not allow random controls */
		sdrv_error(wq,bp);
		return;

	case M_FLUSH:
		sl_flush(*bp->b_rptr, slp);
		if (slp->sl_data_wq) {
			putnext(slp->sl_data_wq, bp);
		} else if (*bp->b_rptr & FLUSHR) {
			*bp->b_rptr &= ~FLUSHW;
			qreply(wq, bp);
		} else {
			freemsg(bp);
		}
		return;

	case M_IOCTL:
		/* Only the data stream to the modem should ever be linked
		 * or unlinked.
		 */
		iocp = (struct iocblk*)bp->b_rptr;
		switch (iocp->ioc_cmd) {
		case I_LINK:
			linkblkp = (struct linkblk*)bp->b_cont->b_rptr;
			STR_LOCK_SPL(s);
			ASSERT(iocp->ioc_count == sizeof(struct linkblk));
			ASSERT(linkblkp->l_qtop == wq);
			if (0 != slp->sl_data_wq
			    && slp->sl_data_wq != wq->q_next) {
				/* can link only 1 device */
				iocp->ioc_error = EINVAL;
			} else {
				/* ignore input until first framing character
				 */
				slp->sl_rlen = BAD_RMTU;
				slp->sl_tx_lbolt = 0;

				slp->sl_dl_index = linkblkp->l_index;
				slp->sl_data_wq = linkblkp->l_qbot;
				slp->sl_data_rq = RD(linkblkp->l_qbot);
				linkblkp->l_qbot->q_ptr = (caddr_t)slp;
				slp->sl_data_rq->q_ptr = (caddr_t)slp;

				qenable(slp->sl_wq);
				qenable(slp->sl_rq);
				qenable(slp->sl_data_rq);
			}
			STR_UNLOCK_SPL(s);
			iocp->ioc_count = 0;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq,bp);
			return;

		case I_UNLINK:
			linkblkp = (struct linkblk*)bp->b_cont->b_rptr;
			STR_LOCK_SPL(s);
			ASSERT(linkblkp->l_qtop == wq);
			ASSERT(slp->sl_dl_index == linkblkp->l_index);
			ASSERT(linkblkp->l_qbot == slp->sl_data_wq);
			slp->sl_data_rq->q_ptr = 0;
			slp->sl_data_wq->q_ptr = 0;
			slp->sl_data_rq = 0;
			slp->sl_data_wq = 0;
			slp->sl_dl_index = -1;

			if (slp->sl_type == SC_CSLIP) {
				struct tx_cstate *cs;
				cs = slp->sl_vj_comp->last_cs;
				i = slp->sl_vj_comp->tx_states;
				while (i>0) {
					if (cs->cs_active > 0)
						cs->cs_active = 0;
					cs = cs->cs_next;
					i--;
				}
				slp->sl_vj_comp->actconn = 0;
			}
			STR_UNLOCK_SPL(s);
			iocp->ioc_count = 0;
			bp->b_datap->db_type = M_IOCACK;
			qreply(wq,bp);
			return;

		case TCSETAW:
		case TCSETAF:
		case TCSETA:
			/* accept SLIP parameters */
			ASSERT(iocp->ioc_count == sizeof(struct termio));

			sl_set_mtu(slp,
				   ((STERMIO(bp)->c_cc[VMIN] << 8)
				    + (u_char)STERMIO(bp)->c_cc[VTIME]));

			i = STERMIO(bp)->c_lflag;
			if (i != slp->sl_type) {
				slp->sl_type = i;
				switch (i) {
				case SC_CKSUM:
				case SC_COMP:
					if (!slp->sl_db)
						slp->sl_db = KSALLOC(cp_db,
								     NUM_COMP_SES*2);
					if (!slp->sl_db) {
						bp->b_datap->db_type = M_IOCNAK;
						qreply(wq,bp);
						return;
					}
					cp = slp->sl_db;
					cp0 = &slp->sl_xmt_head;
					j = 0;
					/* clear transmit database */
					while (j < NUM_COMP_SES) {
						cp->db_ses = j;
						cp->db_lbolt = 0;
						*cp0 = cp;
						cp0 = &cp->db_next;
						j++; cp++;
					}
					*cp0 = 0;
					/* clear receive database */
					sl_poison(slp);
					break;

				case SC_CSLIP:
					slp->sl_vj_uncomp
					= vj_uncomp_init(slp->sl_vj_uncomp,
							 DEF_MAX_STATES);
					slp->sl_vj_comp
					= vj_comp_init(slp->sl_vj_comp,
						       DEF_MAX_STATES);
					if (!slp->sl_vj_uncomp
					    || !slp->sl_vj_comp) {
						bp->b_datap->db_type=M_IOCNAK;
						iocp->ioc_error = ENOMEM;
						qreply(wq,bp);
						return;
					}
					break;
				}
			}

			slp->sl_mode = STERMIO(bp)->c_oflag;

			if (TCSETAF == iocp->ioc_cmd)
				sl_flush(FLUSHW, slp);

			if (slp->sl_data_wq) {
				putnext(slp->sl_data_wq, bp);
			} else {
				bp->b_datap->db_type = M_IOCACK;
				iocp->ioc_count = 0;
				qreply(wq,bp);
			}
			return;

		case TCGETA:
			/* announce SLIP parameters on the way back */
			if (slp->sl_data_wq) {
				putnext(slp->sl_data_wq, bp);
			} else {
				bp->b_datap->db_type = M_IOCACK;
				sl_rput(slp->sl_rq, bp);
			}
			return;

		default:
			/* pass on or dump other IOCTLs */
			if (slp->sl_data_wq) {
				putnext(slp->sl_data_wq, bp);
			} else {
				bp->b_datap->db_type = M_IOCNAK;
				qreply(wq,bp);
			}
			break;
		}
		break;

	default:
		/* Pass other messages on to the data stream if alive,
		 * or dump it not */
		if (slp->sl_data_wq) {
			putnext(slp->sl_data_wq, bp);
		} else {
			sdrv_error(wq,bp);
		}
	}
}



/* accept a new STREAMS message from the serial line
 */
static void
sl_rput(queue_t *drq,			/* data read queue */
	mblk_t *bp)
{
	struct sl_cont *slp = (struct sl_cont*)drq->q_ptr;
	struct iocblk *iocp;

	if (!slp) {
		/* When caught in the transition of STREAMS muxes,
		 * hope the service routine is awakened to sort things out.
		 */
#ifdef DEBUG
		printf("sl_rput: stray packet with nowhere to go\n");
#endif
		putq(drq,bp);
		return;
	}

	ASSERT(slp->sl_data_rq == drq || slp->sl_rq == drq);
	ASSERT(slp->sl_wq->q_ptr == (caddr_t)slp);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		if (iff_dead(slp->sl_if.if_flags)) {
			freemsg(bp);	/* if not up, forget it */
		} else {
			putq(slp->sl_data_rq,bp);
		}
		return;

	case M_FLUSH:
		sl_flush(*bp->b_rptr, slp);
		break;

	case M_IOCACK:
		iocp = (struct iocblk*)bp->b_rptr;
		if (iocp->ioc_cmd == TCGETA) {
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			STERMIO(bp)->c_line = slp->sl_if.if_unit;
			STERMIO(bp)->c_cc[VMIN] = slp->sl_if.if_mtu >> 8;
			STERMIO(bp)->c_cc[VTIME] = slp->sl_if.if_mtu;
			STERMIO(bp)->c_lflag = slp->sl_type;
			STERMIO(bp)->c_oflag = slp->sl_mode;
		}
		break;

	case M_HANGUP:
		/* If this module has been clone-opened by the slip daemon,
		 * try not to destroy its stream stack with a hang-up message.
		 */
		if (slp->sl_data_rq != slp->sl_rq) {
			mblk_t *nbp = allocb(1,BPRI_HI);
			if (nbp != 0) {
				freemsg(bp);
				bp = nbp;
				*bp->b_wptr++ = BEEP_DEAD;
			}
		}
		break;
	}

	putnext(slp->sl_rq, bp);
}



/* compute a SLIP checksum of the SGI flavor
 *	The length is included in the checksum to defend zeros which
 *	might otherwise be silently lost when compressing the header
 *	(and thereby guessing the length).
 */
#define SUMROT(ck)	((((ck) >> 1) | ((ck) << 7)) & 0xff)
#define SUMUNROT(ck)	((((ck) << 1) | ((ck) >> 7)) & 0xff)
#define SUMADD(old,new)	(SUMROT(old) ^ (new))

static int
sl_cksum(struct mbuf *m,
	 int off,			/* skip this much first */
	 int totlen,			/* putative data length */
	 u_char *ses_ackp)
{
	unsigned int ck;
	u_char c, *cp;
	int len;

	ck = totlen;
	if (totlen >= 256) {
		ck &= 0xff;
		ck = SUMADD(ck, (totlen>>8) & 0xff);
	}
	for (; m != 0; m = m->m_next) {
		cp = mtod(m, u_char*);
		len = m->m_len;
		if (len <= off) {
			off -= len;
			continue;
		}
		len -= off;
		cp += off;
		off = 0;
		while (len > 0) {
			c = *cp;
			ck = SUMADD(ck, c);
			len--;
			cp++;
		}
	}
	ASSERT(off == 0);
	if (ses_ackp != 0)
		*ses_ackp = c;
	return ck;
}



/* poison the expansion of future received compressed packets
 */
static void
sl_poison(struct sl_cont *slp)
{
	int i;

	if (slp->sl_db != 0) {
		for (i = RX_COMP0; i < NUM_COMP_SES+RX_COMP0; i++)
			slp->sl_db[i].db_lbolt = 0;
		METER(slp->sl_imeters.flush++);
	}

	if (slp->sl_vj_uncomp)
		slp->sl_vj_uncomp->flags |= SLF_TOSS;
}



/* swallow a new mbuf, just now converted from streams buffers
 */
static void
sl_mbin(struct sl_cont *slp,
	struct mbuf *m,			/* start of the chain */
	int len)			/* total data length */
{
	struct cp_db *db;
	struct cp_ih *ih;
	int netlen;
	u_char ses_ack;
	caddr_t snooph;

	slp->sl_if.if_ipackets++;
	if (len > SL_RMTU) {
		if (slp->sl_if.if_flags & IFF_DEBUG)
			printf("sl%d: dropping babble\n",
			       slp->sl_if.if_unit);
		slp->sl_if.if_ierrors++;
		sl_poison(slp);
		m_freem(m);
		return;
	}


	switch (slp->sl_type) {
	case SC_CKSUM:
	case SC_COMP:
		if (len < sizeof(struct cp)) {
			if (slp->sl_if.if_flags & IFF_DEBUG)
				printf("sl%d: dropping %d byte fragment\n",
				       slp->sl_if.if_unit, len);
			slp->sl_if.if_ierrors++;
			sl_poison(slp);
			m_freem(m);
			return;
		}

		/* compute the checksum and grab the protocol byte.
		 *
		 * XXX combine checksum with reception, to avoid this
		 *	separate pass.
		 */
		if (0 != sl_cksum(m, sizeof(struct cp_ih),len,&ses_ack)) {
			if (slp->sl_if.if_flags & IFF_DEBUG)
				printf("sl%d: bad checksum\n",
				       slp->sl_if.if_unit);
			METER(slp->sl_imeters.cksum++);
			slp->sl_if.if_ierrors++;
			sl_poison(slp);
			m_freem(m);
			return;
		}
		/* discard the protocol byte
		 */
		m_adj(m,-(int)sizeof(struct cp));
		netlen = len - sizeof(struct cp);

		ASSERT(m->m_off == MMINOFF);
		ASSERT(m->m_len == MIN(netlen + sizeof(struct cp_ih), MLEN));

		if (netlen > MAX_COMP_LEN
		    || ses_ack == BAD_SES_ACK) {
			struct xh {
				struct cp_ih pad;
				struct cp_hdr h;
			};
			ASSERT(m->m_len >= MINIPCOMP);

			snooph = (caddr_t)&mtod(m,struct xh*)->h;

			METER(slp->sl_imeters.fail++);
			if (ses_ack == BAD_SES_ACK) {
				METER(slp->sl_imeters.proto++);
				break;
			}

			/* make new entry in the database
			 */
			db = &slp->sl_db[GET_SES(ses_ack)+RX_COMP0];
			db->db_hdr = *(struct cp_hdr*)snooph;
			db->db_hdr.ip.ip_sum = 0;
			*(char*)&db->db_hdr.ip = ((sizeof(struct ip)>>2)
						  + (IPVERSION<<4));
			db->db_hdr.tcp.th_seq = (ntohl(db->db_hdr.tcp.th_seq)
						 + netlen-MINIPCOMP);
			db->db_hdr.ip.ip_off = 0;
			db->db_hdr.ip.ip_id ^= htons(0x8000);
			db->db_hdr.tcp.th_sum = 0;
			db->db_hdr.tcp.th_off = sizeof(struct tcphdr)/4;
			db->db_hdr.tcp.th_x2 = 0;
			db->db_hdr.tcp.th_urp = 0;
			db->db_hdr.tcp.th_flags = TH_ACK;
			db->db_ts.ts_src = db->db_hdr.ip.ip_src;
			db->db_ts.ts_dst = db->db_hdr.ip.ip_dst;
			db->db_ts.ts_x1 = 0;
			db->db_ts.ts_pr = IPPROTO_TCP;
			db->db_ts.ts_len = htons(sizeof(ih->h.tcp));
			db->db_lbolt = lbolt+FORCE_STALE;
			break;
		}


		/* expand compressed header
		 */
		ASSERT(m->m_next == 0);

		METER(slp->sl_imeters.success++);

		db = &slp->sl_db[GET_SES(ses_ack)+RX_COMP0];
		if (0 == db->db_lbolt) {
			METER(slp->sl_imeters.poison++);
			slp->sl_if.if_ierrors++;
			m_freem(m);
			return;
		}
		db->db_hdr.ip.ip_id = htons(ntohs(db->db_hdr.ip.ip_id)+1);
		db->db_hdr.ip.ip_len = htons(MINIPCOMP + netlen);
		db->db_hdr.tcp.th_ack = htonl(ntohl(db->db_hdr.tcp.th_ack)
					      + GET_ACK(ses_ack));

		mtod(m,struct ifheader*)->ifh_hdrlen -= MINIPCOMP;

		ih = mtod(m,struct cp_ih*);
		ih->h = db->db_hdr;

		/* the sender predicted us to set the PUSH bit based on
		 *	the data length.
		 */
		if (netlen != 0)
			ih->h.tcp.th_flags |= TH_PUSH;

		/* restore the TCP and IP cksums
		 */
		ih->h.ip.ip_sum = ~in_cksum_sub((ushort*)&ih->h.ip,
						sizeof(ih->h.ip),
						0);
		ih->h.tcp.th_sum = ~in_cksum_sub((ushort*)&ih->h.tcp,
						 sizeof(ih->h.tcp)+netlen,
						 in_cksum_sub((ushort*)&db->db_ts,
							      sizeof(db->db_ts),
							      netlen));
		/* ready the sequence # for next time */
		db->db_hdr.tcp.th_seq += netlen;
		snooph = (caddr_t)&ih->h;
		netlen += sizeof(ih->h);
		break;


	case SC_CSLIP:
		snooph = mtod(m, caddr_t) + CSLIP_HDR;
		ses_ack = *(char*)snooph & 0xf0;
		if (len >= 3 && ses_ack != (IPVERSION << 4)) {
			if (ses_ack & TYPE_COMPRESSED_TCP) {
				ses_ack = TYPE_COMPRESSED_TCP;
			} else if (ses_ack == TYPE_UNCOMPRESSED_TCP) {
				ASSERT(m->m_len > CSLIP_HDR);
				*(char*)snooph &= 0x4f;
			}
			netlen = vj_uncompress(m, len, (u_char**)&snooph,
					       ses_ack, slp->sl_vj_uncomp);
		} else {
			netlen = len;
		}
		break;


	default:
		snooph = mtod(m, caddr_t);
		netlen = len;
		break;
	}


	if (netlen < MINIP) {
		if (slp->sl_if.if_flags & IFF_DEBUG)
			printf("sl%d: drop %d byte fragment\n",
			       slp->sl_if.if_unit,netlen);
		slp->sl_if.if_ierrors++;
		m_freem(m);
		return;
	}

	/* send the packet up the layers
	 */
	IFNET_LOCK(&slp->sl_if);
	if (RAWIF_SNOOPING(&slp->sl_rawif)) {
		(void)snoop_input(&slp->sl_rawif, 0, snooph, m, netlen);
	}
	if (iff_dead(slp->sl_if.if_flags)) {
		slp->sl_if.if_ierrors++;    /* drop input if down */
		m_freem(m);
	} else {
		if (network_input(m, AF_INET, 0)) {
			slp->sl_if.if_iqdrops++;
		}
	}
	IFNET_UNLOCK(&slp->sl_if);
}



/* deal with accumulated STREAMS buffers,
 *	copying them into mbuf chains */
static void
sl_rsrv(queue_t *crq)			/* our read queue */
{
	struct sl_cont *slp;
	int mblen, lim;
	u_char *mbptr, *strptr;
	struct mbuf *pm;
	mblk_t *bp;
	struct mbuf *mbase, *mtail;
	int s;

	slp = (struct sl_cont*)crq->q_ptr;

	if (!slp) {
		/* When caught in the transition of STREAMS muxes,
		 * hope to be re-awakened when things are sorted out.
		 */
#ifdef DEBUG
		printf("sl_rsrv: all dressed up and nowhere to go\n");
#endif
		return;
	}

	ASSERT(slp->sl_data_rq == crq || slp->sl_rq == crq);
	ASSERT(slp->sl_wq->q_ptr == (caddr_t)slp);


	STR_LOCK_SPL(s);
	mbase = slp->sl_rmbase;
	mtail = pm = slp->sl_rmtail;
	slp->sl_rmbase = 0;
	slp->sl_rmtail = 0;
	STR_UNLOCK_SPL(s);
	bp = 0;
	for (;;) {
		if (0 == bp) {
			bp = getq(slp->sl_data_rq);
			if (0 == bp)
				break;

			/* deal with buffers caught in MUX transition
			 */
			if (bp->b_datap->db_type != M_DATA) {
				sl_rput(slp->sl_data_rq,bp);
				bp = 0;
				continue;
			}
		}

		/* get next mbuf for msg block data.
		 */
		if (0 == pm) {
			if (0 != mbase) {
				mblen = MLEN;
			} else {
				switch (slp->sl_type) {
				case SC_CKSUM:
				case SC_COMP:
					mblen = MAX(sizeof(struct cp_ih) + 20,
						    MLEN);
					lim = sizeof(struct cp_ih);
					break;
				case SC_CSLIP:
					mblen = CSLIP_HDR+MLEN;
					lim = CSLIP_HDR;
					break;
				default:
					mblen = MAX(SNLEN+20,MLEN);
					lim = SNLEN;
					break;
				}
			}
			slp->sl_rlim = mblen;
			pm = m_vget(M_DONTWAIT, mblen, MT_DATA);
			if (0 == pm) {
				/* flush if no more mbufs */
				slp->sl_if.if_iqdrops++;
				sl_flush(FLUSHR, slp);
				freemsg(bp);
				bp = 0;
				m_freem(mbase);
				mbase = 0;
				continue;   /* but avoid race */
			}

			if (0 == mbase) {   /* start a new mbuf chain */
				slp->sl_resc = 0;
				M_INITIFHEADER(pm, &slp->sl_if, lim);
				mbase = pm;
			} else {	/* add to existing chain */
				pm->m_len = 0;
				mtail->m_next = pm;
			}
			mtail = pm;
		}

		/* Since we have to see if each byte is special,
		 *	we may as well copy.
		 */
		strptr = bp->b_rptr;
		mblen = bp->b_wptr - strptr;
		lim = slp->sl_rlim - pm->m_len;
		if (lim > mblen) lim = mblen;
		mblen = 0;
		mbptr = mtod(pm, u_char*) + pm->m_len;
		while (mblen < lim) {
			u_char c = *strptr++;
			if (FRAME_HIT == (c & FRAME_MASK)) {
				if (slp->sl_resc) {
					slp->sl_resc = 0;
					c = (c == ESC_FRAME_END
					     ? FRAME_END
					     : FRAME_ESC);
					METER(slp->sl_imeters.escs++);
				} else if (c == FRAME_ESC) {
					slp->sl_resc = 1;
					lim--;
					continue;
				} else if (c == FRAME_END) {
					pm->m_len += mblen;
					slp->sl_rlen += mblen;
					if (slp->sl_rlen > 0) {
						sl_mbin(slp, mbase,
							slp->sl_rlen);
						slp->sl_rlen = 0;
						mbase = 0;
						mtail = 0;
						pm = 0;
					}
					break;
				}
			}
			*mbptr++ = c;
			++mblen;
		}
		slp->sl_if.if_ibytes += mblen;

		bp->b_rptr = strptr;
		if (strptr >= bp->b_wptr) {
			mblk_t *bp2 = bp->b_cont;
			freeb(bp);
			bp = bp2;
		}

		if (0 == pm)
			continue;

		/* count bytes used in this mbuf */
		pm->m_len += mblen;
		slp->sl_rlen += mblen;
		if (pm->m_len >= slp->sl_rlim) {
			/* at end of mbuf, get another
			 */
			if (slp->sl_rlen > SL_RMTU) {
				pm->m_len = 0;	/* detect babblers */
				slp->sl_rlen = BAD_RMTU;
			} else {
				pm = 0;
			}
		}
	}

	STR_LOCK_SPL(s);
	slp->sl_rmbase = mbase;
	slp->sl_rmtail = mtail;
	STR_UNLOCK_SPL(s);
}



/* compress an mbuf
 */
static struct mbuf*
sl_cout(struct sl_cont *slp,
	struct mbuf *m)
{
	struct cp_hdr *hdr;
	struct cp_db *db, **db0;
	u_int w;
	int len0, len;

	/* waste space for a little time & consiseness */
#define SNAP(db,hdr,len) ((db)->db_lbolt = lbolt+FORCE_STALE,	\
			  (db)->db_hdr = *(hdr),		\
	  (db)->db_hdr.tcp.th_seq = ntohl(db->db_hdr.tcp.th_seq) + (len))
#define BAILOUT(db,hdr,len,cnt) {SNAP(db,hdr,len); \
	METER(slp->sl_ometers.fail++); \
	METER(slp->sl_ometers.cnt++); \
	goto sumcomp; }


	slp->sl_cbuf.cp.ses_ack = BAD_SES_ACK;
	len0 = m_length(m);
	if (slp->sl_type == SC_CKSUM)
		goto sumcomp;

	if (len0 < MINIPCOMP) {
		METER(slp->sl_ometers.fail++);
		METER(slp->sl_ometers.small++);
		goto sumcomp;
	}
	hdr = mtod(m,struct cp_hdr*);
	if (hdr->ip.ip_p != IPPROTO_TCP) {
		METER(slp->sl_ometers.fail++);
		METER(slp->sl_ometers.proto++);
		goto sumcomp;
	}

	if (m->m_len < MINIPCOMP) {
		m = m_pullup(m, MINIPCOMP);
		if (0 == m) {
			slp->sl_if.if_iqdrops++;
			IF_DROP(&slp->sl_if.if_snd);
			return 0;
		}
	}

	/* find entry in the database
	 */
	db0 = &slp->sl_xmt_head;
	for (;;) {
		db = *db0;
		if (db->db_hdr.tcp.th_sport == hdr->tcp.th_sport
		    && db->db_hdr.tcp.th_dport == hdr->tcp.th_dport
		    && db->db_hdr.ip.ip_dst.s_addr == hdr->ip.ip_dst.s_addr
		    && db->db_hdr.ip.ip_src.s_addr == hdr->ip.ip_src.s_addr)
			break;
		if (0 == db->db_next) {	/* steal the oldest one */
			db->db_lbolt = 0;
			break;
		}
		db0 = &db->db_next;
	}
	*db0 = db->db_next;		/* move it to the front */
	db->db_next = slp->sl_xmt_head;
	slp->sl_xmt_head = db;
	slp->sl_cbuf.cp.ses_ack = db->db_ses;
	len = len0 - MINIPCOMP;

	/* do not compress if new session, big change in TCP window,
	 *	or if it has been a long time.
	 */
	if (db->db_lbolt < lbolt
	    || hdr->tcp.th_win > db->max_window
	    || hdr->tcp.th_win < db->min_window) {
		METER(db->max_window == 0 ? slp->sl_ometers.new++ : 0);
		db->max_window = hdr->tcp.th_win;
		db->min_window = MAX((hdr->tcp.th_win*3)/4,
				     slp->sl_if.if_mtu);
		if (db->db_lbolt < lbolt) {
			if (db->db_lbolt == 0) {
				BAILOUT(db,hdr,len,new);
			} else {
				BAILOUT(db,hdr,len,stale);
			}
		} else {
			BAILOUT(db,hdr,len,window);
		}
	}

	/* check the common cases first, and quit if high,
	 *	incompressible traffic
	 */
	if (len > MAX_COMP_DATA) {
		BAILOUT(db,hdr,len,big);
	}

	w = ntohl(hdr->tcp.th_ack) - ntohl(db->db_hdr.tcp.th_ack);
	if (w > MAX_COMP_ACK)
		BAILOUT(db,hdr,len,big_ack);
	slp->sl_cbuf.cp.ses_ack |= (w<<SES_LN);
	/* BSD always sets the ACK bit */
	if (!(hdr->tcp.th_flags & TH_ACK))
		BAILOUT(db,hdr,len,no_ack);
	if (!(hdr->tcp.th_flags & TH_PUSH)) {
		if (len != 0)
			BAILOUT(db,hdr,len,no_push);
	} else {
		if (len == 0)
			BAILOUT(db,hdr,len,stray_push);
	}

	/* quit if strange stuff set
	 */
	if (hdr->tcp.th_seq != htonl(db->db_hdr.tcp.th_seq))
		BAILOUT(db,hdr,len,retry);
	if (*(char*)&hdr->ip != ((sizeof(struct ip)>>2) + (IPVERSION<<4)))
		BAILOUT(db,hdr,len,ip_opts);
	if (hdr->ip.ip_tos != db->db_hdr.ip.ip_tos)
		BAILOUT(db,hdr,len,ip_tos);
	if (hdr->ip.ip_off != 0)
		BAILOUT(db,hdr,len,frag);
	if (hdr->ip.ip_ttl != db->db_hdr.ip.ip_ttl)
		BAILOUT(db,hdr,len,ttl);

	if (hdr->tcp.th_off != sizeof(struct tcphdr)/4
	    || hdr->tcp.th_x2 != 0)
		BAILOUT(db,hdr,len,tcp_opts);
	if (hdr->tcp.th_urp != 0)
		BAILOUT(db,hdr,len,tcp_urg);
	if (0 != (hdr->tcp.th_flags & ~(TH_ACK|TH_PUSH)))
		BAILOUT(db,hdr,len,tcp_flags);

	/* ok to compress */
	METER(slp->sl_ometers.success++);
	SNAP(db,hdr,len);
	m_adj(m, MINIPCOMP);
	slp->sl_wlen -= MINIPCOMP;
	len0 = len;

sumcomp:
	slp->sl_cptr = (u_char*)&slp->sl_cbuf;
	slp->sl_wlen += sizeof(slp->sl_cbuf);
	slp->sl_clen = sizeof(slp->sl_cbuf);
	w = sl_cksum(m, 0, len0+sizeof(struct cp),0);
	slp->sl_cbuf.cp.cksum = (SUMROT(w)
				 ^ SUMUNROT(slp->sl_cbuf.cp.ses_ack));
	return m;
}



/*
 * convert mbufs to stream buffers, and ship them to the serial line
 */
static void
sl_wsrv(queue_t *wq)			/* our write queue */
{
	struct sl_cont *slp;
	int mblen, lim;
	u_char *mbptr, *strptr;
	struct mbuf *pm;
	mblk_t *bp, *bp0, *bp9;
	u_char inactive;
	int burst;
	u_char c;
	int s;


	slp = (struct sl_cont*)wq->q_ptr;

	ASSERT(slp->sl_wq == wq || slp->sl_data_wq == wq);
	ASSERT(slp->sl_rq->q_ptr == (caddr_t)slp);

	if (!slp->sl_data_wq
	    || !canput(slp->sl_data_wq->q_next)) {  /* if constitpated, */
		noenable(slp->sl_wq);	/* go to sleep */
		if (!(slp->sl_state & SL_ST_BEEP_TIME))
			sl_beep(slp);
		return;
	}
	enableok(slp->sl_wq);		/* if flowing, awakening is ok */

	STR_LOCK_SPL(s);		/* resume old mbuf chain */
	pm = slp->sl_wmbuf;		/* locking to protect from flushes */
	slp->sl_wmbuf = 0;
	STR_UNLOCK_SPL(s);
	bp0 = bp = 0;
	inactive = (pm == 0
		    && slp->sl_clen == 0
		    && lbolt > slp->sl_tx_lbolt);
	burst = 0;
	for (;;) {
		if (0 == pm) {		/* if no old chain, */
			/* send packet end */
			lim = slp->sl_clen;
			if (lim != 0) {
				mbptr = slp->sl_cptr;

			} else {
				/* start a new chain */
				/* hold ifnet lock to pass assert() in snoop.
				 */
				IFNET_LOCK(&slp->sl_if);
				IF_DEQUEUE(&slp->sl_if.if_snd, pm);
				if (pm == slp->sl_smbuf) {
					slp->sl_smbuf = 0;
					slp->sl_starve = 0;
				}
				if (0 == pm) {
					IFNET_UNLOCK(&slp->sl_if);
					break;
				}
				slp->sl_if.if_opackets++;
				if (iff_dead(slp->sl_if.if_flags)) {
					slp->sl_if.if_oerrors++;
					IFNET_UNLOCK(&slp->sl_if);
					m_freem(pm);
					pm = 0;
					continue;
				}

				if (RAWIF_SNOOPING(&slp->sl_rawif)) {
					struct mbuf *n;
					int len = m_length(pm);

					n = m_vget(M_DONTWAIT, len+SNLEN,
						   MT_DATA);
					if (n == 0) {
						snoop_drop(&slp->sl_rawif,
							   SN_PROMISC,
							   mtod(pm,caddr_t),
							   pm->m_len);
					} else {
					    IF_INITHEADER(mtod(n,caddr_t),
							  &slp->sl_if,
						  SNLEN);
					    (void)m_datacopy(pm, 0, len,
						     mtod(n,caddr_t)+SNLEN);
					    (void)snoop_input(&slp->sl_rawif,
							     SN_PROMISC,
							     mtod(pm,caddr_t),
							     n, len);
					}
				}
				IFNET_UNLOCK(&slp->sl_if);

				switch (slp->sl_type) {
				case SC_CKSUM:
				case SC_COMP:
					pm = sl_cout(slp,pm);
					break;
				case SC_CSLIP:
					c = vj_compress(&pm,slp->sl_vj_comp,
							&slp->sl_wlen,
							0,1,0);
					if (!pm) {
						slp->sl_if.if_oerrors++;
						continue;
					}
					if (c != TYPE_IP)
						*mtod(pm,char*) |= c;
					slp->sl_cptr = &slp->sl_cbuf.framend;
					slp->sl_clen = 1;
					slp->sl_wlen++;;
					break;
				default:
					slp->sl_cptr = &slp->sl_cbuf.framend;
					slp->sl_clen = 1;
					slp->sl_wlen++;
					break;
				}

				continue;
			}

		} else {
			lim = pm->m_len;
			if (lim == 0) {		/* forget empty stuff */
				pm = m_free(pm);
				continue;
			}
			mbptr = mtod(pm, u_char*);
		}


		if (0 == bp) {		/* get a new buffer */
			mblen = slp->sl_wlen;
			if (mblen <= 0) {
				if (slp->sl_if.if_flags & IFF_DEBUG)
					printf("sl%d: short %d in %d\n",
					       slp->sl_if.if_unit,
					       -mblen, m_length(pm));
				mblen = m_length(pm);
				slp->sl_wlen = mblen;
			}
			if (mblen > slp->sl_maxout)
				mblen = slp->sl_maxout;
			bp = str_allocb(mblen, slp->sl_wq, BPRI_LO);
			if (0 == bp) {
				METER(slp->sl_ometers.allocb_fails++);
				noenable(slp->sl_wq);
				break;
			}
			METER(slp->sl_ometers.allocb++);
			if (0 != bp0) {
				bp9->b_cont = bp;
			} else {
				/* on the 1st packet in a while, end any
				 *	preceeding garbage.
				 */
				if (inactive) {
					METER(slp->sl_ometers.empties++);
					*bp->b_wptr++ = FRAME_END;
					inactive = 0;
				}
				bp0 = bp;
			}
			bp9 = bp;
		}


		strptr = bp->b_wptr;
		mblen = bp->b_datap->db_lim - strptr;
		if (mblen <= 1) {	/* new buffer if almost full */
			bp = 0;
			continue;
		}
		if (lim > mblen) lim = mblen;

/* We have to examine each byte we send in order to convert FRAME_END to
 *	FRAME_ESC:FRAME_END and FRAME_ESC to FRAME_ESC:FRAME_ESC,
 *	so we may as well copy one byte at a time.
 *
 *	This should be improved into a loop that would fetch, check, and
 *	store 32 bits at a time.  It could check for FRAME_END by masking
 *	against 0x80808080, since most traffic will be text, and UNIX text is
 *	generally 7-bit ASCII.
 */
		for (mblen = 0; mblen < lim; mblen++, mbptr++) {
			c = *mbptr;
			if ((c == FRAME_END && mbptr != &slp->sl_cbuf.framend)
			    || c == FRAME_ESC) {
				/* we need 2 bytes in the output */
				if ((bp->b_datap->db_lim - strptr) < 2)
					break;
				--lim;
				*strptr++ = FRAME_ESC;
				c = (c == FRAME_ESC
				     ? ESC_FRAME_ESC
				     : ESC_FRAME_END);
				METER(slp->sl_ometers.escs++);
			}
			*strptr++ = c;
		}
		slp->sl_if.if_obytes += mblen;
		slp->sl_wlen -= mblen;
		if (0 != pm) {
			pm->m_len -= mblen;
			pm->m_off += mblen;
		} else {
			slp->sl_clen -= mblen;
			slp->sl_cptr += mblen;
		}
		bp->b_wptr = strptr;

		burst += mblen;		/* limit latency by limiting burst */
		if (burst >= slp->sl_maxout) {
			putnext(slp->sl_data_wq, bp0);
			bp = 0;
			bp0 = 0;
			slp->sl_tx_lbolt = lbolt + FORCE_END;
			if (!canput(slp->sl_data_wq->q_next)) {
				noenable(slp->sl_wq);
				break;
			}
			burst = 0;
		}
	}
	slp->sl_wmbuf = pm;

	if (0 != bp0) {			/* send what we have */
		putnext(slp->sl_data_wq, bp0);
		slp->sl_tx_lbolt = lbolt + FORCE_END;
	}

	/* let the deamon know about the activity */
	if (!(slp->sl_state & SL_ST_BEEP_TIME))
		sl_beep(slp);
}
