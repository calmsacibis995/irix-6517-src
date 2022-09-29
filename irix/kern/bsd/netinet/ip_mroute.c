/*
 * Procedures for the kernel part of Multicast Routing.
 * (See RFC-1075.)
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenenr, PARC, April 1995
 *
 * MROUTING 3.5.1.1
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"
#include "sys/kmem.h"

#include "sys/debug.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/protosw.h"
#include "sys/tcpipstats.h"

#include "sys/errno.h"
#include "sys/time.h"
#include "sys/cmn_err.h"
#include "sys/systm.h"
#include "sys/sesmgr.h"
#include "net/soioctl.h"
#include "net/if.h"
#include "net/route.h"
#include "net/raw.h"
#include "net/netisr.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "in_var.h"
#include "ip_var.h"
#include "udp.h"
#include "igmp.h"
#include "igmp_var.h"
#include "ip_mroute.h"

mrlock_t mroute_lock;
#define MROUTE_MRRLOCK()	mraccess(&mroute_lock)
#define MROUTE_MRWLOCK()	mrupdate(&mroute_lock)
#define MROUTE_MRUNLOCK()	mrunlock(&mroute_lock)
#define MROUTE_ISLOCKED(type)	ismrlocked(&mroute_lock, type)
#define TBF_INITLOCK(t, n)	mutex_init(&(t)->tbf_lock, MUTEX_DEFAULT, n)
#define TBF_DESTROYLOCK(t)	mutex_destroy(&(t)->tbf_lock)
#define TBF_LOCK(t)		mutex_lock(&t->tbf_lock, PZERO)
#define TBF_UNLOCK(t)	mutex_unlock(&t->tbf_lock)

extern char *inet_ntoa(struct in_addr);
extern struct ifaddr *ifa_ifwithaddr(struct sockaddr *);
#ifdef INET6
extern void rip_input(struct mbuf *, struct ifnet *, struct ipsec *,
  struct mbuf *);
#else
extern void rip_input(struct mbuf *, struct ifnet *, struct ipsec *);
#endif
extern int socket_send(struct socket *, struct mbuf *, struct sockaddr_in *);

/*
 * Forward references
 */
static int ip_mrouter_init(struct socket *so, struct mbuf *m);
extern int ip_mrouter_done(void);
static int add_vif(struct vifctl *vifcp);
static int del_vif(vifi_t *vifip);
static int add_mfc(struct mfcctl *mfccp);
static int del_mfc(struct mfcctl *mfccp);
static int get_vif_cnt(struct sioc_vif_req *req);
static int get_sg_cnt(struct sioc_sg_req *req);
static int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *,vifi_t xmt_vif);

static int get_version(struct mbuf *mb);
static int set_pim(int *i);
static int get_pim(struct mbuf *m);
static void phyint_send(struct ip *, struct vif *, struct mbuf *);
static void encap_send(struct ip *, struct vif *, struct mbuf *);
#ifdef INET6
void ipip_input(struct mbuf *, struct ifnet *, struct ipsec *, struct mbuf *);
#else
void ipip_input(struct mbuf *, struct ifnet *, struct ipsec *);
#endif

/*
 * Global Variables
 * All but ip_mrouter and ip_mrtproto are static.
 */
#ifdef sgi
/* The following are in ip_input.c to allow this file to be optional */
extern struct socket  *ip_mrouter;
extern int ip_mrtproto;

static void
log(int level, char *fmt, ...)
{
	char plus_newline[256];

	va_list ap;
	va_start(ap, fmt);
	(void) strcpy(plus_newline, fmt);
	(void) strcat(plus_newline, "\n");
	icmn_err(level, plus_newline, ap);
	va_end(ap);
}
#endif

#define NO_RTE_FOUND 	0x1
#define RTE_FOUND	0x2

struct mbuf    *mfctable[MFCTBLSIZ];
u_char		nexpire[MFCTBLSIZ];
extern struct vif	viftable[MAXVIFS];

u_int		mrtdebug = 0;	  /* debug level 	*/
#define		DEBUG_MFC	0x02
#define		DEBUG_FORWARD	0x04
#define		DEBUG_EXPIRE	0x08
#define		DEBUG_XMIT	0x10
u_int       	tbfdebug = 0;     /* tbf debug level 	*/

#ifdef RSVP_ISI
extern u_int		rsvpdebug;	  /* rsvp debug level   */
extern int rsvp_on;
extern struct socket *ip_rsvpd;	  /* Yet another RSVP enabler */
#endif /* RSVP_ISI */

#define		UPCALL_EXPIRE	10		/* number of timeouts */

void tbf_control(struct vif *, struct mbuf *, struct ip *, u_long p_len);
void tbf_queue(struct vif *vifp, struct mbuf *m, struct ip *ip);
void tbf_dequeue(struct vif *vifp, int j);
void tbf_process_q(struct vif *vifp);
void tbf_reprocess_q(struct vif *vifp);
int tbf_dq_sel(struct vif *vifp, struct ip *ip);
void tbf_send_packet(struct vif *vifp, struct mbuf *m);
void tbf_update_tokens(struct vif *vifp);
int priority(struct ip *ip);

/*
 * 'Interfaces' associated with decapsulator (so we can tell
 * packets that went through it from ones that get reflected
 * by a broken gateway).  These interfaces are never linked into
 * the system ifnet list & no routes point to them.  I.e., packets
 * can't be sent this way.  They only exist as a placeholder for
 * multicast source verification.
 */
struct ifnet multicast_decap_if[MAXVIFS];

#define ENCAP_TTL 64
#define ENCAP_PROTO IPPROTO_IPIP	/* 4 */

/* prototype IP hdr for encapsulated packets */
struct ip multicast_encap_iphdr = {
#if defined(ultrix) || defined(i386)
	sizeof(struct ip) >> 2, IPVERSION,
#else
	IPVERSION, sizeof(struct ip) >> 2,
#endif
	0,				/* tos */
	sizeof(struct ip),		/* total length */
	0,				/* id */
	0,				/* frag offset */
	ENCAP_TTL, ENCAP_PROTO,	
	0,				/* checksum */
};

extern vifi_t	   numvifs;
/*
 * Private variables.
 */
static int have_encap_tunnel = 0;

/*
 * one-back cache used by ipip_input to locate a tunnel's vif
 * given a datagram's src ip address.
 */
static u_long last_encap_src;
static struct vif *last_encap_vif;


/*
 * whether or not PIM is enabled.
 */
static int pim;
/*
 * Rate limit for assert notification messages, in usec
 */
#define ASSERT_MSG_TIME		3000000

/*
 * Initialization
 */
void
ip_mroute_init(void)
{
	extern struct bsd_kernaddrs bsd_kernaddrs;
	mrlock_init(&mroute_lock, MRLOCK_BARRIER, "mroute", 0);
	bsd_kernaddrs.bk_addr[_KA_MRTPROTO] = (caddr_t)&ip_mrtproto;
	bsd_kernaddrs.bk_addr[_KA_MFCTABLE] = (caddr_t)&mfctable;
	bsd_kernaddrs.bk_addr[_KA_VIFTABLE] = (caddr_t)&viftable;
	return;
}

/*
 * Hash function for a source, group entry
 */
#define MFCHASH(a, g) MFCHASHMOD(((a) >> 20) ^ ((a) >> 10) ^ (a) ^ \
			((g) >> 20) ^ ((g) >> 10) ^ (g))

/*
 * Find a route for a given origin IP address and Multicast group address
 * Type of service parameter to be added in the future!!!
 */
#define MFCFIND(o, g, rt) { \
	register struct mbuf *_mb_rt = mfctable[MFCHASH(o,g)]; \
	register struct mfc *_rt = NULL; \
	rt = NULL; \
	IP_MROUTE_STAT(mrts_mfc_lookups); \
	while (_mb_rt) { \
		_rt = mtod(_mb_rt, struct mfc *); \
		if ((_rt->mfc_origin.s_addr == o) && \
		    (_rt->mfc_mcastgrp.s_addr == g) && \
		    (_mb_rt->m_act == NULL)) { \
			rt = _rt; \
			break; \
		} \
		_mb_rt = _mb_rt->m_next; \
	} \
	if (rt == NULL) { \
		IP_MROUTE_STAT(mrts_mfc_misses); \
	} \
}

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) { \
	    register int xxs; \
		\
	    delta = (a).tv_usec - (b).tv_usec; \
	    if ((xxs = (a).tv_sec - (b).tv_sec)) { \
	       switch (xxs) { \
		      case 2: \
			  delta += 1000000; \
			      /* fall through */ \
		      case 1: \
			  delta += 1000000; \
			  break; \
		      default: \
			  delta += (1000000 * xxs); \
	       } \
	    } \
}

#define TV_LT(a, b) (((a).tv_usec < (b).tv_usec && \
	      (a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
int
ip_mrouter_set(int cmd, struct socket *so, struct mbuf *m)
{
    if (cmd != MRT_INIT && so != ip_mrouter) return EACCES;

    switch (cmd) {
	case MRT_INIT:      return ip_mrouter_init(so, m);
	case MRT_DONE:      return ip_mrouter_done();
	case MRT_ADD_VIF:   return add_vif(mtod(m, struct vifctl *));
	case MRT_DEL_VIF:   return del_vif(mtod(m, vifi_t *));
	case MRT_ADD_MFC:   return add_mfc(mtod(m, struct mfcctl *));
	case MRT_DEL_MFC:   return del_mfc(mtod(m, struct mfcctl *));
	case MRT_PIM:	    return set_pim(mtod(m, int *));
	default:            return EOPNOTSUPP;
    }
}

/*
 * Handle MRT getsockopt commands
 */
int
ip_mrouter_get(cmd, so, m)
    int cmd;
    struct socket *so;
    struct mbuf **m;
{
    struct mbuf *mb;

    if (so != ip_mrouter) return EACCES;

    *m = mb = m_get(M_WAIT, MT_SOOPTS);

    switch (cmd) {
	case MRT_VERSION:   return get_version(mb);
	case MRT_PIM:	    return get_pim(mb);
	default:            return EOPNOTSUPP;
    }
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
int
mrt_ioctl(cmd, data)
    int cmd;
    caddr_t data;
{
    switch (cmd) {
      case (SIOCGETVIFCNT):
	  return (get_vif_cnt((struct sioc_vif_req *)data));
      case (SIOCGETSGCNT):
	  return (get_sg_cnt((struct sioc_sg_req *)data));
      default:
	  return (EINVAL);
    }
    /* NOTREACHED */
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
static int
get_sg_cnt(register struct sioc_sg_req *req)
{
    register struct mfc *rt;

    MROUTE_MRRLOCK();
    MFCFIND(req->src.s_addr, req->grp.s_addr, rt);

    if (rt != NULL) {
	req->pktcnt = rt->mfc_pkt_cnt;
	req->bytecnt = rt->mfc_byte_cnt;
	req->wrong_if = rt->mfc_wrong_if;
    } else
	req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
    MROUTE_MRUNLOCK();

    return 0;
}



/*
 * returns the input and output packet and byte counts on the vif provided
 */
static int
get_vif_cnt(req)
    register struct sioc_vif_req *req;
{
    register vifi_t vifi = req->vifi;

    if (vifi >= numvifs) return EINVAL;

    req->icount = viftable[vifi].v_pkt_in;
    req->ocount = viftable[vifi].v_pkt_out;
    req->ibytes = viftable[vifi].v_bytes_in;
    req->obytes = viftable[vifi].v_bytes_out;

    return 0;
}

/*
 * Enable multicast routing
 */
static int
ip_mrouter_init(struct socket *so ,struct mbuf *m)
{

    int *v;

    if (mrtdebug)
	log(LOG_DEBUG,"ip_mrouter_init: so_type = %d, pr_protocol = %d",
		so->so_type, so->so_proto->pr_protocol);

#ifdef sgi
    ip_mrtproto = IGMP_DVMRP;    /* for netstat only */
#endif
    if (so->so_type != SOCK_RAW ||
	so->so_proto->pr_protocol != IPPROTO_IGMP) return EOPNOTSUPP;

    if (!m || (m->m_len != sizeof(int)))
	return ENOPROTOOPT;

    v = mtod(m, int *);
    if (*v != 1)
	return ENOPROTOOPT;

    if (ip_mrouter != NULL) return EADDRINUSE;

    ip_mrouter = so;

    bzero((caddr_t)mfctable, sizeof(mfctable));
    bzero((caddr_t)nexpire, sizeof(nexpire));

    pim = 0;
    return 0;
}

/*
 * Disable multicast routing
 */
int
ip_mrouter_done(void)
{
    vifi_t vifi;
    int i;
    struct ifnet *ifp;
    struct ifreq ifr;
    struct mbuf *mb_rt;
    struct mbuf *m;
    struct rtdetq *rte;

    MROUTE_MRWLOCK();

    ip_mrouter = NULL;

    /*
     * For each phyint in use, disable promiscuous reception of all IP
     * multicasts.
     */
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_lcl_addr.s_addr != 0 &&
	    !(viftable[vifi].v_flags & VIFF_TUNNEL)) {
	    ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;
	    ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr
								= INADDR_ANY;
	    ifp = viftable[vifi].v_ifp;
	    IFNET_LOCK(ifp);
#ifdef sgi
	    (void) (*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&(ifr.ifr_addr));
#else
	    (void) (*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
#endif
	    IFNET_UNLOCK(ifp);
	}
	if (viftable[vifi].v_tbf) {
	    for (i=viftable[vifi].v_tbf->q_len-1; i >= 0; i--) {
		    m_freem(viftable[vifi].v_tbf->qtable[i].pkt_m->m_act);
		    m_freem(viftable[vifi].v_tbf->qtable[i].pkt_m);
		    tbf_dequeue(viftable+vifi,i);
	    }
	    kmem_free(viftable[vifi].v_tbf, sizeof (struct tbf));
        }
    }
    bzero((caddr_t)viftable, sizeof(viftable));
    numvifs = 0;
    pim = 0;

    /*
     * Free all multicast forwarding cache entries.
     */
    for (i = 0; i < MFCTBLSIZ; i++) {
	mb_rt = mfctable[i];
	while (mb_rt) {
	    if (mb_rt->m_act != NULL) {
		while (m = mb_rt->m_act) {
		    mb_rt->m_act = m->m_act;
		    rte = mtod(m, struct rtdetq *);
		    m_freem(rte->m);
		    m_free(m);
		}
	    }
	    mb_rt = m_free(mb_rt);
	}
    }

    bzero((caddr_t)mfctable, sizeof(mfctable));

    /*
     * Reset de-encapsulation cache
     */
    last_encap_src = NULL;
    last_encap_vif = NULL;

    have_encap_tunnel = 0;

    MROUTE_MRUNLOCK();

#ifdef DEBUG
    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_done");
#endif

    return 0;
}

static int
get_version(mb)
    struct mbuf *mb;
{
    int *v;

    v = mtod(mb, int *);

    *v = 0x0305;	/* XXX !!!! */
    mb->m_len = sizeof(int);

    return 0;
}

/*
 * Set PIM assert processing global
 */
static int
set_pim(int *i)
{
    if ((*i != 1) && (*i != 0))
	return EINVAL;

    pim = *i;

    return 0;
}

/*
 * Get PIM assert processing global
 */
static int
get_pim(struct mbuf *m)
{
    int *i;

    i = mtod(m, int *);

    *i = pim;

    return 0;
}

/*
 * Add a vif to the vif table
 */
static int
add_vif(register struct vifctl *vifcp)
{
    register struct vif *vifp = viftable + vifcp->vifc_vifi;
    struct sockaddr_in sin = {AF_INET};
    struct ifaddr *ifa;
    struct ifnet *ifp;
    struct ifreq ifr;
    int error, s;
    struct tbf *v_tbf;

    if (vifcp->vifc_vifi >= MAXVIFS)  return EINVAL;
    MROUTE_MRWLOCK();
    if (vifp->v_lcl_addr.s_addr != 0) {
    	MROUTE_MRUNLOCK();
	return EADDRINUSE;
    }

    /*
     * Allocate a new token buffer structure, so we fail early
     * if we cannot get one. Or recycle if already had one.
     */
    v_tbf = vifp->v_tbf;
    if (v_tbf == NULL) {
	    v_tbf = kmem_zalloc(sizeof (struct tbf), KM_NOSLEEP);
	    if (v_tbf == NULL) {
		    MROUTE_MRUNLOCK();
		    return ENOBUFS;
	    }
	    TBF_INITLOCK(v_tbf, "tbf_lock");
    }


    /* Find the interface with an address in AF_INET family */
    sin.sin_addr = vifcp->vifc_lcl_addr;
    ifa = ifa_ifwithaddr((struct sockaddr *)&sin);
    if (ifa == 0) {
	MROUTE_MRUNLOCK();
	return EADDRNOTAVAIL;
    }
    ifp = ifa->ifa_ifp;

    if (vifcp->vifc_flags & VIFF_TUNNEL) {
	if ((vifcp->vifc_flags & VIFF_SRCRT) == 0) {

	    /*
	     * An encapsulating tunnel is wanted.  Tell ipip_input() to
	     * start paying attention to encapsulated packets.
	     */
	    if (have_encap_tunnel == 0) {
		have_encap_tunnel = 1;
		for (s = 0; s < MAXVIFS; ++s) {
		    multicast_decap_if[s].if_name = "mdecap";
		    multicast_decap_if[s].if_unit = s;
		}
	    }
	    /*
	     * Set interface to fake encapsulator interface
	     */
	    ifp = &multicast_decap_if[vifcp->vifc_vifi];
	    /*
	     * Prepare cached route entry
	     */
	    bzero(&vifp->v_route, sizeof(vifp->v_route));
	} else {
	    log(LOG_ERR, "Source routed tunnels not supported.");
	    return EOPNOTSUPP;
	}
    } else {
	/* Make sure the interface supports multicast */
	if ((ifp->if_flags & IFF_MULTICAST) == 0) {
	    MROUTE_MRUNLOCK();
	    return EOPNOTSUPP;
	}

	/* Enable promiscuous reception of all IP multicasts from the if */
	((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;
	((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr = INADDR_ANY;
	IFNET_LOCK(ifp);
#ifdef sgi
	error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&(ifr.ifr_addr));
#else
	error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);
#endif
	IFNET_UNLOCK(ifp);
	if (error) {
	    MROUTE_MRUNLOCK();
	    return error;
	}
    }

    /* define parameters for the tbf structure */
    vifp->v_tbf = v_tbf;
    vifp->v_tbf->q_len = 0;
    vifp->v_tbf->n_tok = 0;
    vifp->v_tbf->last_pkt_t = 0;

    vifp->v_flags     = vifcp->vifc_flags;
    vifp->v_threshold = vifcp->vifc_threshold;
    vifp->v_lcl_addr  = vifcp->vifc_lcl_addr;
    vifp->v_rmt_addr  = vifcp->vifc_rmt_addr;
    vifp->v_ifp       = ifp;

    vifp->v_rate_limit= vifcp->vifc_rate_limit;

#ifdef RSVP_ISI
    vifp->v_rsvp_on = 0;
    vifp->v_rsvpd = NULL;
#endif /* RSVP_ISI */
    /* initialize per vif pkt counters */
    vifp->v_pkt_in    = 0;
    vifp->v_pkt_out   = 0;
    vifp->v_bytes_in  = 0;
    vifp->v_bytes_out = 0;

    /* Adjust numvifs up if the vifi is higher than numvifs */
    if (numvifs <= vifcp->vifc_vifi) numvifs = vifcp->vifc_vifi + 1;

    if (mrtdebug)
	log(LOG_DEBUG, "add_vif #%d, lcladdr %x, %s %x, thresh %x, rate %d",
	    vifcp->vifc_vifi, 
	    ntohl(vifcp->vifc_lcl_addr.s_addr),
	    (vifcp->vifc_flags & VIFF_TUNNEL) ? "rmtaddr" : "mask",
	    ntohl(vifcp->vifc_rmt_addr.s_addr),
	    vifcp->vifc_threshold,
	    vifcp->vifc_rate_limit);    
    MROUTE_MRUNLOCK();
    
    return 0;
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif(vifi_t *vifip)
{
    register struct vif *vifp = viftable + *vifip;
    register vifi_t vifi;
    struct ifnet *ifp;
    struct ifreq ifr;
    int i;

    if (*vifip >= numvifs) return EINVAL;
    if (vifp->v_lcl_addr.s_addr == 0) return EADDRNOTAVAIL;

    MROUTE_MRWLOCK();

    if (!(vifp->v_flags & VIFF_TUNNEL)) {
	((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;
	((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr = INADDR_ANY;
	ifp = vifp->v_ifp;
	IFNET_LOCK(ifp);
#ifdef sgi
	(void) (*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&(ifr.ifr_addr));
#else
	(void) (*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
#endif
	IFNET_UNLOCK(ifp);
    }

    if (vifp == last_encap_vif) {
	last_encap_vif = 0;
	last_encap_src = 0;
    }

    for (i=vifp->v_tbf->q_len-1; i >= 0; i--) {
	    m_freem(vifp->v_tbf->qtable[i].pkt_m->m_act);
	    m_freem(vifp->v_tbf->qtable[i].pkt_m);
	    tbf_dequeue(vifp,i);
    }
    TBF_DESTROYLOCK(vifp->v_tbf);
    kmem_free(vifp->v_tbf, sizeof(struct tbf));
    bzero((caddr_t)vifp, sizeof (*vifp));

    /* Adjust numvifs down */
    for (vifi = numvifs; vifi > 0; vifi--)
	if (viftable[vifi-1].v_lcl_addr.s_addr != 0) break;
    numvifs = vifi;
    MROUTE_MRUNLOCK();

    if (mrtdebug)
      log(LOG_DEBUG, "del_vif %d, numvifs %d", *vifip, numvifs);

    return 0;
}

/*
 * Add an mfc entry
 */
static int
add_mfc(struct mfcctl *mfccp)
{
    struct mfc *rt;
    register struct mbuf *mb_rt;
    u_long hash;
    struct mbuf *mb_ntry;
    struct rtdetq *rte;
    register u_short nstl;
    int i;

    MROUTE_MRWLOCK();
    MFCFIND(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr, rt);

    /* If an entry already exists, just update the fields */
    if (rt) {
	if (mrtdebug & DEBUG_MFC)
	    log(LOG_DEBUG,"add_mfc update o %x g %s p %x\n",
		ntohl(mfccp->mfcc_origin.s_addr),
		inet_ntoa(mfccp->mfcc_mcastgrp),
		mfccp->mfcc_parent);
	rt->mfc_parent = mfccp->mfcc_parent;
	for (i = 0; i < numvifs; i++)
	    rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
    	MROUTE_MRUNLOCK();
	return 0;
    }

    /* 
     * Find the entry for which the upcall was made and update
     */
    hash = MFCHASH(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);
    for (mb_rt = mfctable[hash], nstl = 0; mb_rt; mb_rt = mb_rt->m_next) {

	rt = mtod(mb_rt, struct mfc *);
	if ((rt->mfc_origin.s_addr == mfccp->mfcc_origin.s_addr) &&
	    (rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr) &&
	    (mb_rt->m_act != NULL)) {

	    if (nstl++)
		log(LOG_ERR, "add_mfc %s o %x g %x p %x dbx %x",
		    "multiple kernel entries",
		    ntohl(mfccp->mfcc_origin.s_addr),
		    ntohl(mfccp->mfcc_mcastgrp.s_addr),
		    mfccp->mfcc_parent, mb_rt->m_act);
	    if (mrtdebug & DEBUG_MFC)
		log(LOG_DEBUG,"add_mfc o %x g %x p %x dbg %x",
		    ntohl(mfccp->mfcc_origin.s_addr),
		    ntohl(mfccp->mfcc_mcastgrp.s_addr),
		    mfccp->mfcc_parent, mb_rt->m_act);


	    rt->mfc_origin     = mfccp->mfcc_origin;
	    rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;
	    rt->mfc_parent     = mfccp->mfcc_parent;
	    for (i = 0; i < numvifs; i++)
		rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
	    /* initialize pkt counters per src-grp */
	    rt->mfc_pkt_cnt    = 0;
	    rt->mfc_byte_cnt   = 0;
	    rt->mfc_wrong_if   = 0;
	    rt->mfc_last_assert.tv_sec = rt->mfc_last_assert.tv_usec = 0;

	    rt->mfc_expire = 0;	/* Don't clean this guy up */
	    nexpire[hash]--;

	    /* send packets Qed at the end of this entry */
	    while (mb_rt->m_act) {
		mb_ntry = mb_rt->m_act;
		rte = mtod(mb_ntry, struct rtdetq *);
		ip_mdq(rte->m, rte->ifp, rt, -1);
		mb_rt->m_act = mb_ntry->m_act;
		m_freem(rte->m);
		m_free(mb_ntry);
	    }
	}
    }

    /*
     * It is possible that an entry is being inserted without an upcall
     */
    if (nstl == 0) {
	if (mrtdebug & DEBUG_MFC)
	    log(LOG_DEBUG,"add_mfc no upcall h %d o %x g %x p %x",
		hash, ntohl(mfccp->mfcc_origin.s_addr),
		ntohl(mfccp->mfcc_mcastgrp.s_addr),
		mfccp->mfcc_parent);
	
	for (mb_rt = mfctable[hash]; mb_rt; mb_rt = mb_rt->m_next) {
	    
	    rt = mtod(mb_rt, struct mfc *);
	    if ((rt->mfc_origin.s_addr == mfccp->mfcc_origin.s_addr) &&
		(rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr)) {

		rt->mfc_origin     = mfccp->mfcc_origin;
		rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;
		rt->mfc_parent     = mfccp->mfcc_parent;
		for (i = 0; i < numvifs; i++)
		    rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
		/* initialize pkt counters per src-grp */
		rt->mfc_pkt_cnt    = 0;
		rt->mfc_byte_cnt   = 0;
		rt->mfc_wrong_if   = 0;
		rt->mfc_last_assert.tv_sec = rt->mfc_last_assert.tv_usec = 0;
		if (rt->mfc_expire)
		    nexpire[hash]--;
		rt->mfc_expire	   = 0;
	    }
	}
	if (mb_rt == NULL) {
	    /* no upcall, so make a new entry */
	    MGET(mb_rt, M_DONTWAIT, MT_MRTABLE);
	    if (mb_rt == NULL) {
		MROUTE_MRUNLOCK();
		return ENOBUFS;
	    }
	    
	    rt = mtod(mb_rt, struct mfc *);
	    
	    /* insert new entry at head of hash chain */
	    rt->mfc_origin     = mfccp->mfcc_origin;
	    rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;
	    rt->mfc_parent     = mfccp->mfcc_parent;
	    for (i = 0; i < numvifs; i++)
		rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
	    /* initialize pkt counters per src-grp */
	    rt->mfc_pkt_cnt    = 0;
	    rt->mfc_byte_cnt   = 0;
	    rt->mfc_wrong_if   = 0;
	    rt->mfc_last_assert.tv_sec = rt->mfc_last_assert.tv_usec = 0;
	    rt->mfc_expire     = 0;

	    /* link into table */
	    mb_rt->m_next  = mfctable[hash];
	    mfctable[hash] = mb_rt;
	    mb_rt->m_act = NULL;
	}
    }
    MROUTE_MRUNLOCK();
    return 0;
}


/*
 * Delete an mfc entry
 */
static int
del_mfc(struct mfcctl *mfccp)
{
    struct in_addr 	origin;
    struct in_addr 	mcastgrp;
    struct mfc 		*rt;
    struct mbuf 	*mb_rt;
    struct mbuf 	**nptr;
    u_long 		hash;

    MROUTE_MRWLOCK();

    origin = mfccp->mfcc_origin;
    mcastgrp = mfccp->mfcc_mcastgrp;
    hash = MFCHASH(origin.s_addr, mcastgrp.s_addr);

    if (mrtdebug & DEBUG_MFC)
	log(LOG_DEBUG,"del_mfc orig %x mcastgrp %x",
	    ntohl(origin.s_addr), ntohl(mcastgrp.s_addr));

    nptr = &mfctable[hash];
    while ((mb_rt = *nptr) != NULL) {
        rt = mtod(mb_rt, struct mfc *);
	if (origin.s_addr == rt->mfc_origin.s_addr &&
	    mcastgrp.s_addr == rt->mfc_mcastgrp.s_addr &&
	    mb_rt->m_act == NULL)
	    break;

	nptr = &mb_rt->m_next;
    }
    if (mb_rt == NULL) {
    	MROUTE_MRUNLOCK();
	return EADDRNOTAVAIL;
    }

    MFREE(mb_rt, *nptr);
    MROUTE_MRUNLOCK();

    return 0;
}

/*
 * IP multicast forwarding function. This function assumes that the packet
 * pointed to by "ip" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IP multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */

#define IP_HDR_LEN  20	/* # bytes of fixed IP header (excluding options) */
#define TUNNEL_LEN  12  /* # bytes of IP option for tunnel encapsulation  */

int 
ip_mforward(
	struct ip *ip,
	struct mbuf *m,
	struct ifnet *ifp,
	struct ip_moptions *imo)
{
    register struct mfc *rt;
    register struct vif *vifp;
    struct sockaddr_in 	k_igmpsrc	= { AF_INET };
    static int srctun = 0;
    register struct mbuf *mm;
#ifdef RSVP_ISI
    vifi_t vifi;
#endif /* RSVP_ISI */

    if (mrtdebug & DEBUG_FORWARD)
	log(LOG_DEBUG, "ip_mforward: src %x, dst %x, ifp %x",
	    ntohl(ip->ip_src.s_addr), ntohl(ip->ip_dst.s_addr), ifp);

    if (ip->ip_hl < (IP_HDR_LEN + TUNNEL_LEN) >> 2 ||
	((u_char *)(ip + 1))[1] != IPOPT_LSRR ) {
	/*
	 * Packet arrived via a physical interface or
	 * an encapuslated tunnel.
	 */
    } else {

	/*
	 * Packet arrived through a source-route tunnel.
	 * Source-route tunnels are no longer supported.
	 */
	if ((srctun++ % 1000) == 0)
	    log(LOG_ERR, "ip_mforward: received source-routed packet from %x",
		ntohl(ip->ip_src.s_addr));

	return 1;
    }

#ifdef RSVP_ISI
    if ((imo) && ((vifi = imo->imo_multicast_vif) < numvifs)) {
	if (ip->ip_ttl < 255)
	    ip->ip_ttl++;	/* compensate for -1 in *_send routines */
	if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
	    vifp = viftable + vifi;
	    printf("Sending IPPROTO_RSVP from %x to %x on vif %d (%s%s%d)\n",
		ntohl(ip->ip_src), ntohl(ip->ip_dst), vifi,
		(vifp->v_flags & VIFF_TUNNEL) ? "tunnel on " : "",
		vifp->v_ifp->if_name, vifp->v_ifp->if_unit);
	}
	return (ip_mdq(m, ifp, 0, vifi));
    }
    if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
	printf("Warning: IPPROTO_RSVP from %x to %x without vif option\n",
	    ntohl(ip->ip_src), ntohl(ip->ip_dst));
    }
#endif /* RSVP_ISI */

    /*
     * Don't forward a packet with time-to-live of zero or one,
     * or a packet destined to a local-only group.
     */
    if (ip->ip_ttl <= 1 ||
	ntohl(ip->ip_dst.s_addr) <= INADDR_MAX_LOCAL_GROUP)
	return 0;

    /*
     * Determine forwarding vifs from the forwarding cache table
     */
    MROUTE_MRRLOCK();
    MFCFIND(ip->ip_src.s_addr, ip->ip_dst.s_addr, rt);

    /* Entry exists, so forward if necessary */
    if (rt != NULL) {
	int error;
	error = ip_mdq(m, ifp, rt, -1);
	MROUTE_MRUNLOCK();
	return (error);
    } else {
	/*
	 * If we don't have a route for packet's origin,
	 * Make a copy of the packet &
	 * send message to routing daemon
	 */

	register struct mbuf *mb_rt;
	register struct mbuf *mb_ntry;
	register struct mbuf *mb0;
	register struct rtdetq *rte;
	register struct mbuf *rte_m;
	register u_long hash;
	register int npkts;
#ifdef UPCALL_TIMING
	struct timeval tp;

	GET_TIME(tp);
#endif /* UPCALL_TIMING */

	/*
	 * We need to drop the reader lock and get a writer one,
	 * since we will be modifying the forwarding cache below.
	 */
	MROUTE_MRUNLOCK();
	MROUTE_MRWLOCK();
	IP_MROUTE_STAT(mrts_no_route);
	if (mrtdebug & (DEBUG_FORWARD | DEBUG_MFC))
	    log(LOG_DEBUG, "ip_mforward: no rte s %x g %x",
		ntohl(ip->ip_src.s_addr),
		ntohl(ip->ip_dst.s_addr));

	/*
	 * Allocate mbufs early so that we don't do extra work if we are
	 * just going to fail anyway.
	 */
	MGET(mb_ntry, M_DONTWAIT, MT_MRTABLE);
	if (mb_ntry == NULL) {
	    MROUTE_MRUNLOCK();
	    return ENOBUFS;
	}
	mb0 = m_copy(m, 0, M_COPYALL);
	if (mb0)
	    mb0 = m_pullup(mb0, sizeof (struct ip));
	if (mb0 == NULL) {
	    MROUTE_MRUNLOCK();
	    m_free(mb_ntry);
	    return ENOBUFS;
	}

	/* is there an upcall waiting for this packet? */
	hash = MFCHASH(ip->ip_src.s_addr, ip->ip_dst.s_addr);
	for (mb_rt = mfctable[hash]; mb_rt; mb_rt = mb_rt->m_next) {
	    rt = mtod(mb_rt, struct mfc *);
	    if ((ip->ip_src.s_addr == rt->mfc_origin.s_addr) &&
		(ip->ip_dst.s_addr == rt->mfc_mcastgrp.s_addr))
		break;
	}

	if (mb_rt == NULL) {
	    int hlen = ip->ip_hl << 2;
	    int i;
	    struct igmpmsg *im;

	    /* no upcall, so make a new entry */
	    MGET(mb_rt, M_DONTWAIT, MT_MRTABLE);
	    if (mb_rt == NULL) {
		m_free(mb_ntry);
		m_freem(mb0);
		MROUTE_MRUNLOCK();
		return ENOBUFS;
	    }
	    /* Make a copy of the header to send to the user level process */
	    mm = m_copy(m, 0, hlen);
	    if (mm)
		mm = m_pullup(mm, hlen);
	    if (mm == NULL) {
		m_free(mb_ntry);
		m_freem(mb0);
		m_free(mb_rt);
		MROUTE_MRUNLOCK();
		return ENOBUFS;
	    }
	    /* 
	     * Send message to routing daemon to install 
	     * a route into the kernel table
	     */
	    k_igmpsrc.sin_addr = ip->ip_src;
	    
	    im = mtod(mm, struct igmpmsg *);
	    im->im_msgtype	= IGMPMSG_NOCACHE;
	    im->im_mbz		= 0;

	    if (socket_send(ip_mrouter, mm, &k_igmpsrc) < 0) {
		if (mrtdebug & DEBUG_MFC)
		    log(LOG_WARNING, "ip_mforward: ip_mrouter socket queue full");
		IP_MROUTE_STAT(mrts_upq_sockfull);
		m_free(mb_ntry);
		m_freem(mb0);
		m_free(mb_rt);
		MROUTE_MRUNLOCK();
		return ENOBUFS;
	    }

	    IP_MROUTE_STAT(mrts_upcalls);
	    rt = mtod(mb_rt, struct mfc *);

	    /* insert new entry at head of hash chain */
	    rt->mfc_origin.s_addr     = ip->ip_src.s_addr;
	    rt->mfc_mcastgrp.s_addr   = ip->ip_dst.s_addr;
	    rt->mfc_expire	      = UPCALL_EXPIRE;
	    nexpire[hash]++;
	    for (i = 0; i < numvifs; i++)
		rt->mfc_ttls[i] = 0;
	    rt->mfc_parent = -1;

	    /* link into table */
	    mb_rt->m_next  = mfctable[hash];
	    mfctable[hash] = mb_rt;
	    mb_rt->m_act = NULL;

	    rte_m = mb_rt;
	} else {
	    if (mb_rt->m_act == NULL) {
		    /*
		     * Handle the race of add_mfc getting in between
		     * the first and second searches of the routing table
		     */
		    int error;

		    error = ip_mdq(m, ifp, rt, -1);
		    m_free(mb_ntry);
		    m_freem(mb0);
		    MROUTE_MRUNLOCK();
		    return (error);
	    }
	    /* determine if q has overflowed */
	    for (rte_m = mb_rt, npkts = 0; rte_m->m_act; rte_m = rte_m->m_act)
		npkts++;

	    if (npkts > MAX_UPQ) {
		IP_MROUTE_STAT(mrts_upq_ovflw);
		m_free(mb_ntry);
		m_freem(mb0);
		MROUTE_MRUNLOCK();
		return 0;
	    }
	}

	mb_ntry->m_act = NULL;
	rte = mtod(mb_ntry, struct rtdetq *);

	rte->m 			= mb0;
	rte->ifp 		= ifp;
#ifdef UPCALL_TIMING
	rte->t			= tp;
#endif /* UPCALL_TIMING */

	/* Add this entry to the end of the queue */
	rte_m->m_act		= mb_ntry;

	MROUTE_MRUNLOCK();
	return 0;
    }		
    /* NOTREACHED */
}


/*
 * Clean up cache entries if upcalls are not serviced
 * Call from the Slow Timeout mechanism, every half second.
 */
void
ip_mroute_slowtimo(void)
{
    struct mbuf *mb_rt, *m, **nptr;
    struct rtdetq *rte;
    struct mfc *mfc;
    int i;

    MROUTE_MRWLOCK();
    for (i = 0; i < MFCTBLSIZ; i++) {
	if (nexpire[i] == 0)
	    continue;
	nptr = &mfctable[i];
	for (mb_rt = *nptr; mb_rt != NULL; mb_rt = *nptr) {
	    mfc = mtod(mb_rt, struct mfc *);

	    /*
	     * Skip real cache entries
	     * Make sure it wasn't marked to not expire (shouldn't happen)
	     * If it expires now
	     */
	    if (mb_rt->m_act != NULL &&
	        mfc->mfc_expire != 0 &&
		--mfc->mfc_expire == 0) {
		if (mrtdebug & DEBUG_EXPIRE)
		    log(LOG_DEBUG, "expire_upcalls: expiring (%x %x)",
			ntohl(mfc->mfc_origin.s_addr),
			ntohl(mfc->mfc_mcastgrp.s_addr));
		/*
		 * drop all the packets
		 * free the mbuf with the pkt, if, timing info
		 */
		while (mb_rt->m_act) {
		    m = mb_rt->m_act;
		    mb_rt->m_act = m->m_act;
	     
		    rte = mtod(m, struct rtdetq *);
		    m_freem(rte->m);
		    m_free(m);
		}
		IP_MROUTE_STAT(mrts_cache_cleanups);
		nexpire[i]--;

		MFREE(mb_rt, *nptr);
	    } else {
		nptr = &mb_rt->m_next;
	    }
	}
    }
    MROUTE_MRUNLOCK();
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
ip_mdq(
    register struct mbuf *m,
    register struct ifnet *ifp,
    register struct mfc *rt,
    vifi_t xmt_vif)
{
    register struct ip  *ip = mtod(m, struct ip *);
    register vifi_t vifi;
    register struct vif *vifp;
    register int plen = ntohs(ip->ip_len);

/*
 * Macro to send packet on vif.  Since RSVP packets don't get counted on
 * input, they shouldn't get counted on output, so statistics keeping is
 * seperate.
 */
#define MC_SEND(ip,vifp,m) {                             \
                if ((vifp)->v_flags & VIFF_TUNNEL)	 \
                    encap_send((ip), (vifp), (m));       \
                else                                     \
                    phyint_send((ip), (vifp), (m));      \
}

#ifdef RSVP_ISI
    /*
     * If xmt_vif is not -1, send on only the requested vif.
     *
     * (since vifi_t is u_short, -1 becomes MAXUSHORT, which > numvifs.
     */
    if (xmt_vif < numvifs) {
        MC_SEND(ip, viftable + xmt_vif, m);
	return 1;
    }
    if (rt == NULL)	/* should "never" happen */
	return (1);
#endif /* RSVP_ISI */

    /*
     * Don't forward if it didn't arrive from the parent vif for its origin.
     */
    vifi = rt->mfc_parent;
    if ((vifi >= numvifs) || (viftable[vifi].v_ifp != ifp)) {
	/* came in the wrong interface */
	if (mrtdebug & DEBUG_FORWARD)
	    log(LOG_DEBUG, "wrong if: ifp %x vifi %d vififp %x",
		ifp, vifi, viftable[vifi].v_ifp); 
	IP_MROUTE_STAT(mrts_wrong_if);
	rt->mfc_wrong_if++;
	/*
	 * If we are doing PIM assert processing, and we are forwarding
	 * packets on this interface, and it is a broadcast medium
	 * interface (and not a tunnel), send a message to the routing daemon.
	 */
	if (pim && rt->mfc_ttls[vifi] &&
		(ifp->if_flags & IFF_BROADCAST) &&
		!(viftable[vifi].v_flags & VIFF_TUNNEL)) {
	    struct sockaddr_in k_igmpsrc;
	    struct mbuf *mm;
	    struct igmpmsg *im;
	    int hlen = ip->ip_hl << 2;
	    struct timeval now;
	    register u_long delta;

	    GET_TIME(now);

	    TV_DELTA(rt->mfc_last_assert, now, delta);

	    if (delta > ASSERT_MSG_TIME) {
		mm = m_copy(m, 0, hlen);
		if (mm && (M_HASCL(mm) || mm->m_len < hlen))
		    mm = m_pullup(mm, hlen);
		if (mm == NULL) {
		    return ENOBUFS;
		}

		rt->mfc_last_assert = now;
		
		im = mtod(mm, struct igmpmsg *);
		im->im_msgtype	= IGMPMSG_WRONGVIF;
		im->im_mbz	= 0;
		im->im_vif	= vifi;

		k_igmpsrc.sin_addr = im->im_src;

		socket_send(ip_mrouter, mm, &k_igmpsrc);
	    }
	}
	return 0;
    }

    /* If I sourced this packet, it counts as output, else it was input. */
    if (ip->ip_src.s_addr != viftable[vifi].v_lcl_addr.s_addr) {
	viftable[vifi].v_pkt_in++;
	viftable[vifi].v_bytes_in += plen;
    }
    rt->mfc_pkt_cnt++;
    rt->mfc_byte_cnt += plen;

    /*
     * For each vif, decide if a copy of the packet should be forwarded.
     * Forward if:
     *		- the ttl exceeds the vif's threshold
     *		- there are group members downstream on interface
     */
    for (vifp = viftable, vifi = 0; vifi < numvifs; vifp++, vifi++)
	if ((rt->mfc_ttls[vifi] > 0) &&
	    (ip->ip_ttl > rt->mfc_ttls[vifi])) {
	    MC_SEND(ip, vifp, m);
	}

    return 0;
}

#ifdef RSVP_ISI
/*
 * check if a vif number is legal/ok. This is used by ip_output, to export
 * numvifs there, 
 */
int
legal_vif_num(int vif)
{
    if (vif >= 0 && vif < numvifs)
       return(1);
    else
       return(0);
}
#endif /* RSVP_ISI */

static void
phyint_send(
	struct ip *ip,
	struct vif *vifp,
	struct mbuf *m)
{
    register struct mbuf *mb_copy;
    register struct mbuf *mopts;
    register struct ip_moptions *imo;

    ASSERT(MROUTE_ISLOCKED(MR_ACCESS) || MROUTE_ISLOCKED(MR_UPDATE));
    mb_copy = m_copy(m, 0, M_COPYALL);
    if (mb_copy)
	mb_copy = m_pullup(mb_copy, sizeof (struct ip));
    if (mb_copy == NULL)
	return;
    MGET(mopts, M_DONTWAIT, MT_IPMOPTS);
    if (mopts == NULL) {
	m_freem(mb_copy);
	return;
    }

    imo = mtod(mopts, struct ip_moptions *);
    imo->imo_multicast_ifp  = vifp->v_ifp;
    imo->imo_multicast_ttl  = ip->ip_ttl - 1;
    imo->imo_multicast_loop = 1;
#ifdef RSVP_ISI
    imo->imo_multicast_vif  = -1;
#endif

    /* link options to enable both mbufs to passed on */
    mb_copy->m_act = mopts;

    if (vifp->v_rate_limit <= 0)
	tbf_send_packet(vifp, mb_copy);
    else
	tbf_control(vifp, mb_copy, mtod(mb_copy, struct ip *), ip->ip_len);
}


static void
encap_send(
    register struct ip *ip,
    register struct vif *vifp,
    register struct mbuf *m)
{
    register struct mbuf *mb_copy;
    register struct ip *ip_copy;
    register int i, len = ip->ip_len;

    ASSERT(MROUTE_ISLOCKED(MR_ACCESS) || MROUTE_ISLOCKED(MR_UPDATE));
    /*
     * copy the old packet & pullup it's IP header into the
     * new mbuf so we can modify it.  Try to fill the new
     * mbuf since if we don't the ethernet driver will.
     */
    MGET(mb_copy, M_DONTWAIT, MT_DATA);
    if (mb_copy == NULL)
	return;
    mb_copy->m_off += 16;
    mb_copy->m_len = sizeof(multicast_encap_iphdr);
    if ((mb_copy->m_next = m_copy(m, 0, M_COPYALL)) == NULL) {
	m_freem(mb_copy);
	return;
    }
    i = MMAXOFF - mb_copy->m_off;
    if (i > len)
	i = len;
    mb_copy = m_pullup(mb_copy, i);
    if (mb_copy == NULL)
	return;

    /*
     * fill in the encapsulating IP header.
     */
    ip_copy = mtod(mb_copy, struct ip *);
    *ip_copy = multicast_encap_iphdr;
    ip_copy->ip_id = htons(atomicAddIntHot(&ip_id, 1));
    ip_copy->ip_len += len;
    ip_copy->ip_src = vifp->v_lcl_addr;
    ip_copy->ip_dst = vifp->v_rmt_addr;

    /*
     * turn the encapsulated IP header back into a valid one.
     */
    ip = (struct ip *)((caddr_t)ip_copy + sizeof(multicast_encap_iphdr));
    --ip->ip_ttl;
    HTONS(ip->ip_len);
    HTONS(ip->ip_off);
    ip->ip_sum = 0;
#if defined(LBL) && !defined(ultrix)
    ip->ip_sum = ~oc_cksum((caddr_t)ip, ip->ip_hl << 2, 0);
#else
    mb_copy->m_off += sizeof(multicast_encap_iphdr);
    ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
    mb_copy->m_off -= sizeof(multicast_encap_iphdr);
#endif

    if (vifp->v_rate_limit <= 0)
	tbf_send_packet(vifp, mb_copy);
    else
	tbf_control(vifp, mb_copy, ip, ip_copy->ip_len);
}

/*
 * De-encapsulate a packet and feed it back through ip input (this
 * routine is called whenever IP gets a packet with proto type
 * ENCAP_PROTO and a local destination address).
 */
/*ARGSUSED*/
void
#ifdef INET6
ipip_input(register struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec,
  struct mbuf *unused)
#else
ipip_input(register struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec)
#endif
{
    register struct ip *ip = mtod(m, struct ip *);
    register int hlen = ip->ip_hl << 2;
    register struct vif *vifp;

    _SESMGR_SOATTR_FREE(ipsec);

    if (!have_encap_tunnel) {
#ifdef INET6
	rip_input(m, ifp, NULL, NULL);
#else
	rip_input(m, ifp, NULL);
#endif
	return;
    }
    /*
     * dump the packet if it's not to a multicast destination or if
     * we don't have an encapsulating tunnel with the source.
     * Note:  This code assumes that the remote site IP address
     * uniquely identifies the tunnel (i.e., that this site has
     * at most one tunnel with the remote site).
     */
    if (! IN_MULTICAST(ntohl(((struct ip *)((char *)ip + hlen))->ip_dst.s_addr))) {
	IP_MROUTE_STAT(mrts_bad_tunnel);
	m_freem(m);
	return;
    }
    MROUTE_MRRLOCK();
    if (ip->ip_src.s_addr != last_encap_src) {
	register struct vif *vife;

	vifp = viftable;
	vife = vifp + numvifs;
	last_encap_src = ip->ip_src.s_addr;
	last_encap_vif = 0;
	for ( ; vifp < vife; ++vifp)
	    if (vifp->v_rmt_addr.s_addr == ip->ip_src.s_addr) {
		if ((vifp->v_flags & (VIFF_TUNNEL|VIFF_SRCRT))
		    == VIFF_TUNNEL)
		    last_encap_vif = vifp;
		break;
	   }
    }
    if ((vifp = last_encap_vif) == 0) {
	last_encap_src = 0;
	IP_MROUTE_STAT(mrts_cant_tunnel); /*XXX*/
	m_freem(m);
	if (mrtdebug)
	    log(LOG_DEBUG, "ip_mforward: no tunnel with %x",
		ntohl(ip->ip_src.s_addr));
	MROUTE_MRUNLOCK();
	return;
    }
    ifp = vifp->v_ifp;
    MROUTE_MRUNLOCK();

    hlen -= sizeof(struct ifheader);
    m->m_off += hlen;
    m->m_len -= hlen;
    /* 
     * Just like ip_forward(), force a new checksum. 
     * Add padding if needed to align pointer on a pointer boundary.
     */
    m->m_flags &= ~M_CKSUMMED;
    hlen = ((u_int) - mtod(m, __psint_t)) % sizeof(void *);
    m->m_off -= hlen;
    m->m_len += hlen;
    IF_INITHEADER(mtod(m, struct ifheader *), ifp, sizeof(struct ifheader) + hlen);
    network_input(m, AF_INET, 0);
}


/*
 * Token bucket filter module
 */
void
tbf_control(struct vif *vifp,
    register struct mbuf *m,
    register struct ip *ip,
    register u_long p_len)
{

    TBF_LOCK(vifp->v_tbf);
    tbf_update_tokens(vifp);

    /* if there are enough tokens, 
     * and the queue is empty,
     * send this packet out
     */

    if (vifp->v_tbf->q_len == 0) {
	if (p_len <= vifp->v_tbf->n_tok) {
	    vifp->v_tbf->n_tok -= p_len;
	    tbf_send_packet(vifp, m);
	} else if (p_len > MAX_BKT_SIZE) {
	    /* drop if packet is too large */
	    IP_MROUTE_STAT(mrts_pkt2large);
	    m_freem(m->m_act);
	    m_freem(m);
	    TBF_UNLOCK(vifp->v_tbf);
	    return;
	} else {
	    /* queue packet and timeout till later */
	    tbf_queue(vifp, m, ip);
	    timeout(tbf_reprocess_q, (caddr_t)vifp, HZ/100);
	}
    } else if (vifp->v_tbf->q_len < MAXQSIZE) {
	/* within queue length, so queue pkts and process queue */
	tbf_queue(vifp, m, ip);
	tbf_process_q(vifp);
    } else {
	/* queue length too much, try to dq and queue and process */
	if (!tbf_dq_sel(vifp, ip)) {
	    IP_MROUTE_STAT(mrts_q_overflow);
	    m_freem(m->m_act);
	    m_freem(m);
	    TBF_UNLOCK(vifp->v_tbf);
	    return;
	} else {
	    tbf_queue(vifp, m, ip);
	    tbf_process_q(vifp);
	}
    }
    TBF_UNLOCK(vifp->v_tbf);
    return;
}

/* 
 * adds a packet to the queue at the interface
 */
void
tbf_queue(register struct vif *vifp, register struct mbuf *m, register struct ip *ip)
{
    register u_int ql;

    ql = vifp->v_tbf->q_len;

    vifp->v_tbf->qtable[ql].pkt_m = m;
    vifp->v_tbf->qtable[ql].pkt_len = (mtod(m, struct ip *))->ip_len;
    vifp->v_tbf->qtable[ql].pkt_ip = ip;

    vifp->v_tbf->q_len++;
}


/* 
 * processes the queue at the interface
 * assumes lock is already held
 */
void
tbf_process_q(register struct vif *vifp)
{
    struct pkt_queue pkt_1;

    /* loop through the queue at the interface and send as many packets
     * as possible
     */
    while (vifp->v_tbf->q_len > 0) {
	/* locate the first packet */
	pkt_1 = vifp->v_tbf->qtable[0];

	/* determine if the packet can be sent */
	if (pkt_1.pkt_len <= vifp->v_tbf->n_tok) {
	    /* if so, reduce no of tokens, dequeue and send the packet.
	     */
	    vifp->v_tbf->n_tok -= pkt_1.pkt_len;

	    tbf_dequeue(vifp,0);
	    tbf_send_packet(vifp, pkt_1.pkt_m);

	} else break;
    }
}

/* 
 * removes the jth packet from the queue at the interface
 * assumes lock is already held
 */
void
tbf_dequeue(register struct vif *vifp, register int j)
{
    register int i;

    for (i=j+1; i <= vifp->v_tbf->q_len - 1; i++) {
	vifp->v_tbf->qtable[i-1] = vifp->v_tbf->qtable[i];
    }		
    vifp->v_tbf->qtable[i-1].pkt_m = NULL;
    vifp->v_tbf->qtable[i-1].pkt_len = NULL;
    vifp->v_tbf->qtable[i-1].pkt_ip = NULL;

    vifp->v_tbf->q_len--;

    if (tbfdebug > 1)
	log(LOG_DEBUG, "tbf_dequeue: vif# %d qlen %d",vifp-viftable, i-1);
}

/*
 * "timeout" function called a hundred times per second per
 * interface that is rate limited.
 */
void
tbf_reprocess_q(register struct vif *vifp)
{

    if (ip_mrouter == NULL) 
	return;

    TBF_LOCK(vifp->v_tbf);
    tbf_update_tokens(vifp);

    tbf_process_q(vifp);

    if (vifp->v_tbf->q_len)
	timeout(tbf_reprocess_q, (caddr_t)vifp, HZ/100);
    TBF_UNLOCK(vifp->v_tbf);
}

/* function that will selectively discard a member of the queue
 * based on the precedence value and the priority obtained through
 * a lookup table - not yet implemented accurately!
 * assumes lock is already held
 */
int
tbf_dq_sel(register struct vif *vifp, register struct ip *ip)
{
    register int i;
    register u_int p;

    p = priority(ip);

    for(i=vifp->v_tbf->q_len-1;i >= 0;i--) {
	if (p > priority(vifp->v_tbf->qtable[i].pkt_ip)) {
	    m_freem(vifp->v_tbf->qtable[i].pkt_m->m_act);
	    m_freem(vifp->v_tbf->qtable[i].pkt_m);
	    tbf_dequeue(vifp,i);
	    IP_MROUTE_STAT(mrts_drop_sel);
	    return(1);
	}
    }
    return(0);
}

void
tbf_send_packet(
    register struct vif *vifp,
    register struct mbuf *m)
{
    register struct mbuf *mcp;
    int error;
    int s = splnet();

    /* count only packets actually sent */
    vifp->v_pkt_out++;
    vifp->v_bytes_out += (mtod(m, struct ip *))->ip_len;

    /* if source route tunnels */
    if (vifp->v_flags & VIFF_SRCRT) {
	error = ip_output(m, (struct mbuf *)0, (struct route *)0,
			  IP_FORWARDING, (struct mbuf *)0, NULL);
	if (mrtdebug & DEBUG_XMIT)
	    log(LOG_DEBUG, "srcrt_send on vif %d err %d", vifp-viftable, error);
    } else if (vifp->v_flags & VIFF_TUNNEL) {
	/* If tunnel options */
	error = ip_output(m, (struct mbuf *)0, (struct route *)0,
			  IP_FORWARDING, (struct mbuf *)0, NULL);
    } else {
	/* if physical interface option, extract the options and then send */
	mcp = m->m_act;
	m->m_act = NULL;
	error = ip_output(m, (struct mbuf *)0, (struct route *)0,
			  IP_FORWARDING|IP_MULTICASTOPTS, mcp, NULL);
	m_freem(mcp);

	if (mrtdebug & DEBUG_XMIT)
	    log(LOG_DEBUG, "phyint_send on vif %d err %d", vifp-viftable, error);
    }
    splx(s);
}

/* determine the current time and then
 * the elapsed time (between the last time and time now)
 * in milliseconds & update the no. of tokens in the bucket
 * assumes lock is already held
 */
void
tbf_update_tokens(register struct vif *vifp)
{
    struct timeval tp;
    register u_long t;
    register u_long elapsed;

    GET_TIME(tp);

    t = tp.tv_sec*1000 + tp.tv_usec/1000;

    elapsed = (t - vifp->v_tbf->last_pkt_t) * vifp->v_rate_limit /8;
    vifp->v_tbf->n_tok += elapsed;
    vifp->v_tbf->last_pkt_t = t;

    if (vifp->v_tbf->n_tok > MAX_BKT_SIZE)
	vifp->v_tbf->n_tok = MAX_BKT_SIZE;
}

int
priority(register struct ip *ip)
{
    register int prio;

    /* temporary hack; may add general packet classifier some day */

    /*
     * The UDP port space is divided up into four priority ranges:
     * [0, 16384)     : unclassified - lowest priority
     * [16384, 32768) : audio - highest priority
     * [32768, 49152) : whiteboard - medium priority
     * [49152, 65536) : video - low priority
     */
    if (ip->ip_p == IPPROTO_UDP) {
	struct udphdr *udp = (struct udphdr *)(((char *)ip) + (ip->ip_hl << 2));

	switch (ntohl(udp->uh_sport) & 0xc000) {
	    case 0x4000:
		prio = 70;
		break;
	    case 0x8000:
		prio = 60;
		break;
	    case 0xc000:
		prio = 55;
		break;
	    default:
		prio = 50;
		break;
	}

	if (tbfdebug > 1) log(LOG_DEBUG, "port %x prio %d", ntohl(udp->uh_sport), prio);
    } else
	prio = 50;


    return prio;
}

/*
 * End of token bucket filter modifications 
 */

#ifdef RSVP_ISI

int
ip_rsvp_vif_init(so, m)
    struct socket *so;
    struct mbuf *m;
{
    int i;
    register int s;

    if (rsvpdebug)
	printf("ip_rsvp_vif_init: so_type = %d, pr_protocol = %d\n",
	       so->so_type, so->so_proto->pr_protocol);

    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return EOPNOTSUPP;

    /* Check mbuf. */
    if (m == NULL || m->m_len != sizeof(int)) {
	return EINVAL;
    }
    i = *(mtod(m, int *));

    if (rsvpdebug)
	printf("ip_rsvp_vif_init: vif = %d rsvp_on = %d\n",i,rsvp_on);

    s = splnet();

    /* Check vif. */
    if (!legal_vif_num(i)) {
	splx(s);
	return EADDRNOTAVAIL;
    }

    /* Check if socket is available. */
    if (viftable[i].v_rsvpd != NULL) {
	splx(s);
	return EADDRINUSE;
    }

    viftable[i].v_rsvpd = so;
    /* This may seem silly, but we need to be sure we don't over-increment
     * the RSVP counter, in case something slips up.
     */
    if (!viftable[i].v_rsvp_on) {
	viftable[i].v_rsvp_on = 1;
	rsvp_on++;
    }

    splx(s);
    return 0;
}

int
ip_rsvp_vif_done(so, m)
    struct socket *so;
    struct mbuf *m;
{
    int i;
    register int s;

    if (rsvpdebug)
	printf("ip_rsvp_vif_done: so_type = %d, pr_protocol = %d\n",
	       so->so_type, so->so_proto->pr_protocol);

    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return EOPNOTSUPP;

    /* Check mbuf. */
    if (m == NULL || m->m_len != sizeof(int)) {
	return EINVAL;
    }
    i = *(mtod(m, int *));

    s = splnet();

    /* Check vif. */
    if (!legal_vif_num(i)) {
	splx(s);
        return EADDRNOTAVAIL;
    }

    if (rsvpdebug)
	printf("ip_rsvp_vif_done: v_rsvpd = %x so = %x\n",
	       viftable[i].v_rsvpd, so);

    viftable[i].v_rsvpd = NULL;
    /* This may seem silly, but we need to be sure we don't over-decrement
     * the RSVP counter, in case something slips up.
     */
    if (viftable[i].v_rsvp_on) {
	viftable[i].v_rsvp_on = 0;
	rsvp_on--;
    }

    splx(s);
    return 0;
}

void
ip_rsvp_force_done(so)
    struct socket *so;
{
    int vifi;
    register int s;

    /* Don't bother if it is not the right type of socket. */
    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return;

    s = splnet();

    /* The socket may be attached to more than one vif...this
     * is perfectly legal.
     */
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_rsvpd == so) {
	    viftable[vifi].v_rsvpd = NULL;
	    /* This may seem silly, but we need to be sure we don't
	     * over-decrement the RSVP counter, in case something slips up.
	     */
	    if (viftable[vifi].v_rsvp_on) {
		viftable[vifi].v_rsvp_on = 0;
		rsvp_on--;
	    }
	}
    }

    splx(s);
    return;
}
#endif /* RSVP_ISI */
