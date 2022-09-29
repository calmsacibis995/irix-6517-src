#ifdef INET6
/*
 * IPv6 multicast forwarding procedures
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenner, PARC, April 1995
 * Rewritten by Tom Pusateri, Gated Consortium, November 1994
 * Translated to IPv6 by Francis Dupont, INRIA, March 1997
 *
 * MROUTING Revision: 3.5
 * $Id: ip6_mroute.c,v 1.1 1997/09/15 20:01:42 ms Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kmem.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/in6_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/ip6_icmp.h>
#include <netinet/ip6_mroute.h>

struct socket  *ip6_mrouter = NULL;

#ifdef MROUTING6
static int ip6_mrouter_init(struct socket *, struct mbuf *);
static int user_add_mfc6(struct mfc6ctl *);
static int user_delete_mfc6(struct mfc6ctl *);
static int user_add_dsif(struct mfc6ctl *);
static int user_delete_dsif(struct mfc6ctl *);
static int user_enable_mfc6(struct mfc6ctl *);
static int user_disable_mfc6(struct mfc6ctl *);
static int get_version(struct mbuf *);
static int mfc6_clean(struct radix_node *, struct walkarg *);
static int socket_send(struct mbuf *, struct mfc6entry *, int, struct ifnet *);
static int mfc6_addqueue(struct mbuf *, struct mfc6entry *);
static void mcast6_send(struct mbuf *, struct ifnet *, struct ip_moptions *);
static struct mfc6entry *mfc6_create(struct sockaddr_mcast6 *, struct ifnet *,
				     u_long flags, int *);
static struct mfc6entry *mfc6_get(struct sockaddr_mcast6 *, int *);
static int mfc6_delete(struct mfc6entry *);
static int get_info(struct mbuf *);

struct radix_node_head *mfc6_tree;

#define ALIGN(x) (((__psunsigned_t)(x)+sizeof(long)-1) & ~(sizeof(long)-1))

int mrouting6_memuse = 0;
#ifndef MROUTING6_MAXMEM
#define MROUTING6_MAXMEM	(1024 * 1024)
#endif
int mrouting6_maxmem = MROUTING6_MAXMEM;

#define RM_Malloc(p, t, n)	\
	if (mrouting6_memuse + (n) > mrouting6_maxmem) \
		p = (t) 0; \
	else { \
		p = (t)kmem_alloc((unsigned long)(n), KM_NOSLEEP); \
		if (p != (t) 0) \
			mrouting6_memuse += (n); \
	}
#define RM_Free(p, n)	\
	mrouting6_memuse -= (n); \
	kmem_free((caddr_t)p, n);
#endif

/*
 * Handle MFC6 setsockopt commands to modify the multicast routing tables.
 */
int
ip6_mrouter_set(cmd, so, m)
    int cmd;
    struct socket *so;
    struct mbuf *m;
{
#ifdef MROUTING6
	if ((cmd != MFC6_INIT) && (so != ip6_mrouter))
		return EACCES;

	switch (cmd) {
	    case MFC6_INIT:
		return ip6_mrouter_init(so, m);
	    case MFC6_DONE:
		return ip6_mrouter_done();
	    case MFC6_ADD_MFC:
		return user_add_mfc6 (mtod(m, struct mfc6ctl *));
	    case MFC6_DEL_MFC:
		return user_delete_mfc6 (mtod(m, struct mfc6ctl *));
	    case MFC6_ADD_DSIF:
		return user_add_dsif (mtod(m, struct mfc6ctl *));
	    case MFC6_DEL_DSIF:
		return user_delete_dsif (mtod(m, struct mfc6ctl *));
	    case MFC6_ENA_MFC:
		return user_enable_mfc6 (mtod(m, struct mfc6ctl *));
	    case MFC6_DIS_MFC:
		return user_disable_mfc6 (mtod(m, struct mfc6ctl *));
	    default:
		return EOPNOTSUPP;
	}
#else
	return EOPNOTSUPP;
#endif
}

/*
 * Handle MFC6 getsockopt commands
 */
int
ip6_mrouter_get(cmd, so, m)
	int cmd;
	struct socket *so;
	struct mbuf **m;
{
#ifdef MROUTING6
	struct mbuf *mb;

	if (so != ip6_mrouter)
		return EACCES;

	*m = mb = m_get(M_WAIT, MT_SOOPTS);
  
	switch (cmd) {
	    case MFC6_VERSION:
		return get_version(mb);
	    case MFC6_INFO_MFC:
		return get_info(mb);
	    default:
		return EOPNOTSUPP;
	}
#else
	return EOPNOTSUPP;
#endif
}

/*
 * Disable IPv6 multicast routing
 */
int
ip6_mrouter_done()
{
#ifdef MROUTING6
	int error = 0;
	int s = splnet();

	if (mfc6_tree)
		error = mfc6_tree->rnh_walktree(mfc6_tree, mfc6_clean, 0);

	splx(s);

	ip6_mrouter = (struct socket *)0;
	return error;
#else
	return 0;
#endif
}

/*
 * IPv6 multicast forwarding function. This function assumes that the packet
 * pointed to by "ip" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IPv6 multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */
int
ip6_mforward(ip, ifp, m, imo)
	struct ipv6 *ip;
	struct ifnet *ifp;
	struct mbuf *m;
	struct ip_moptions *imo;
{
#ifdef MROUTING6
	int	len, rc = 1;
	static	struct sockaddr_mcast6 key =
		{ sizeof (struct sockaddr_mcast6), AF_INET6 };
	struct	ds_ifaddr *cur;
	struct	mfc6entry *mfc;
	struct	mbuf *n;

	if (ip->ip6_hlim <= 1 || MADDR6_SCOPE(ip->ip6_dst) <= MADDR6_SCP_LINK)
	    return (0);

	COPY_ADDR6(ip->ip6_dst, key.sm6_group);
	COPY_ADDR6(ip->ip6_src, key.sm6_origin);
	mfc = mfc6_get(&key, &rc);
	if (mfc) {
	    /*
	     * Is this entry not yet complete ?
	     */
	    if ((mfc->mfc6_flags & RTF_UP) == 0) {
		rc = mfc6_addqueue(m, mfc);
		/* send a duplicate resolution request */
		socket_send(m, mfc, EADDRNOTAVAIL, ifp);
	    } else {
		/*
		 * Normal forwarding case.
		 * check for valid incoming interface
		 */
		if (ifp == mfc->mfc6_upstream) {
		    /*
		     * if correct, forward to all downstream ifs
		     */
		    mfc->mfc6_pktcnt++;
		    for (len = 0, n = m; n; n = n->m_next)
			    len += n->m_len;
		    mfc->mfc6_bytecnt += len;
		    mfc->mfc6_lastuse = time;
		    for (cur = mfc->mfc6_ds_list.stqh_first; cur;
			 cur = cur->ds_list.stqe_next)
			if ((cur->ds_threshold < ip->ip6_hlim) &&
			    (cur->ds_scope <= MADDR6_SCOPE(ip->ip6_dst)))
			    mcast6_send(m, cur->ds_ifp, imo);
		    rc = 0;
		} else {
		    /* Upstream interface is not correct. */
		    mfc->mfc6_wrongif++;
		    if (ifp->if_flags & IFF_BROADCAST) {
			/* possible pruning for user level daemon */
			socket_send(m, mfc, EADDRINUSE, ifp);
		    }
		}
	    }
	} else {
	    /*
	     * create cache entry as a place holder to queue packets
	     */
	    mfc = mfc6_create(&key, ifp, 0, &rc);
	    if (mfc) {
		rc = mfc6_addqueue(m, mfc);
	    }
	    /* ask for resolution */
	    socket_send(m, mfc, EADDRNOTAVAIL, ifp);
	}
	return (rc);
#else
	return 0;
#endif
}

#ifdef MROUTING6

/*
 * Enable IPv6 multicast routing
 */
/*ARGSUSED*/
int
ip6_mrouter_init(so, m)
	struct socket *so;
	struct mbuf *m;
{
#ifdef IP6PRINTFS
	log(LOG_DEBUG,
	    "ip6_mrouter_init: so_type = %d, pr_protocol = %d\n",
	    so->so_type, so->so_proto->pr_protocol);
#endif

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_ICMPV6)
		return EOPNOTSUPP;

	if (ip6_mrouter != (struct socket *)0)
		return EADDRINUSE;

	ip6_mrouter = so;

	if (mfc6_tree == NULL) {
		int s = splnet();

		if (!rn_inithead((void **)&mfc6_tree, 32)) {
			splx(s);
			return ENOBUFS;
		}
		splx(s);
	}
	return 0;
}

static int
get_version(mb)
    struct mbuf *mb;
{
    int *v;

    v = mtod(mb, int *);

    *v = 0x0101;	/* XXX !!!! */
    mb->m_len = sizeof(int);

    return 0;
}

/*
 * Clean the IPv6 multicast routing cache walking routine
 */
/*ARGSUSED*/
static int
mfc6_clean(rn, w)
	struct radix_node *rn;
	struct walkarg *w;
{
	return mfc6_delete((struct mfc6entry *)rn);
}

/*
 * Send a message to mrouted on the multicast routing socket
 */
static int
socket_send(m0, mfc, errno, ifp)
	struct mbuf *m0;
	struct mfc6entry *mfc;
	int errno;
	struct ifnet *ifp;
{
	struct socket *s = ip6_mrouter;
	struct mbuf *m, *n = 0;
	struct ipv6 *ip;
	struct cmsghdr *cp;
	static struct sockaddr_in6 src = { sizeof(src), AF_INET6 };
	int index;
	u_long flags;

	if (s == (struct socket *)0)
		goto bad;

	n = m_get(M_DONTWAIT, MT_DATA);  /* was MT_CONTROL */
	if (n == NULL)
		goto bad;
	m = m_copy(m0, 0, (int)M_COPYALL);
	if (m == NULL)
		goto bad;

	ip = mtod(m0, struct ipv6 *);
	COPY_ADDR6(ip->ip6_src, src.sin6_addr);

	cp = mtod(n, struct cmsghdr *);
	cp->cmsg_len = sizeof(*cp) + sizeof(struct mcast6_info);
	n->m_len = ALIGN(cp->cmsg_len);
	cp->cmsg_level = IPPROTO_IPV6;
	cp->cmsg_type = MFC6_MSG;
	flags = mfc ? mfc->mfc6_flags : 0;
	bcopy(&flags, CMSG_DATA(cp), sizeof(u_long));
	bcopy(&errno, CMSG_DATA(cp) + sizeof(u_long), sizeof(int));
	index = mfc ? mfc->mfc6_upstream->if_index : 0;
	bcopy(&index,
	      CMSG_DATA(cp) + sizeof(u_long) + sizeof(int),
	      sizeof(int));
	index = ifp->if_index;
	bcopy(&index,
	      CMSG_DATA(cp) + sizeof(u_long) + 2 * sizeof(int),
	      sizeof(int));

	cp = (struct cmsghdr *)(mtod(n, caddr_t) + n->m_len);
	cp->cmsg_len = sizeof(*cp) + sizeof(struct in6_pktinfo);
	n->m_len += ALIGN(cp->cmsg_len);
	cp->cmsg_level = IPPROTO_IPV6;
	cp->cmsg_type = IPV6_PKTINFO;
	bcopy(&ip->ip6_dst, CMSG_DATA(cp), sizeof(struct in6_addr));
	index = ifp->if_index;
	bcopy(&index,
	      CMSG_DATA(cp) + sizeof(struct in6_addr),
	      sizeof(int));

	if (sbappendaddr(&s->so_rcv, (struct sockaddr *)&src, m, n, ifp) != 0) {
		sorwakeup(s, NETEVENT_IPUP);
		return 0;
	}
    bad:
	if (n)
		(void)m_free(n);
	m_freem(m0);
	return -1;
}

/*
 * Add a packet in the to-be-sent-later queue
 */
static int
mfc6_addqueue(m0, mfc)
	struct mbuf *m0;
	struct mfc6entry *mfc;
{
	register struct mbuf *m;

	if (mfc->mfc6_hold)
		m_freem(mfc->mfc6_hold);
	mfc->mfc6_hold = NULL;

	m = m_copy(m0, 0, (int)M_COPYALL);
	if (m == NULL)
		return ENOBUFS;
	/* don't step over the IPv6 header... */
	m = m_pullup(m, sizeof(struct ipv6));
	if (m == NULL)
		return ENOBUFS;
	mfc->mfc6_hold = m;
	return 0;
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static void
mcast6_send(m0, ifp, fimo)
	struct mbuf *m0;
	struct ifnet *ifp;
	struct ip_moptions *fimo;
{
	register struct ipv6 *ip = mtod(m0, struct ipv6 *);
	register struct mbuf *m = 0;
	register struct ip_moptions *imo = fimo;

	m = m_copy(m0, 0, (int)M_COPYALL);
	if (m == NULL)
		return;
	if (imo == NULL) {
		imo = (struct ip_moptions *)kmem_alloc(sizeof(*imo),
						   KM_NOSLEEP);
		if (imo == NULL) {
			m_freem(m);
			return;
		}
		imo->imo_multicast_ifp = ifp;
		imo->imo_multicast_ttl = ip->ip6_hlim - 1;
		imo->imo_multicast_loop = 1;
	}
	/* do MTU here ? */
	(void) ip6_output(m, NULL, NULL, IP_FORWARDING, imo, NULL);

	if (fimo == NULL)
		kmem_free(imo, sizeof(*imo));
}

/*
 * Create an IPv6 multicast routing cache entry
 */
static struct mfc6entry *
mfc6_create(key, ifp, flags, errp)
	struct sockaddr_mcast6 *key;
	struct ifnet *ifp;
	u_long flags;
	int *errp;
{
	register struct mfc6entry *mfc;
	register struct radix_node *rn;
	int len, s = splnet();

	if (mfc6_tree == NULL) {
		splx(s);
		*errp = ENOENT;
		return NULL;
	}
#define ROUNDUP(a) (1 + (((a) - 1) | (sizeof(long) - 1)))
	len = sizeof(*mfc) + ROUNDUP(key->sm6_len);
	RM_Malloc(mfc, struct mfc6entry *, len);
	if (mfc == NULL) {
		splx(s);
		*errp = ENOBUFS;
		return NULL;
	}
	bzero(mfc, len);
	mfc->mfc6_upstream = ifp;
	mfc->mfc6_flags = flags;

	mfc->mfc6_ds_list.stqh_first = NULL;
	mfc->mfc6_ds_list.stqh_last = &mfc->mfc6_ds_list.stqh_first;

	mfc->mfc6_lastuse = time;
	bcopy(key, mfc + 1, key->sm6_len);

	rn = mfc6_tree->rnh_addaddr((caddr_t)key, (caddr_t)0,
				    mfc6_tree, mfc->mfc6_nodes);
	if (rn == NULL) {
		RM_Free(mfc, len);
		splx(s);
		*errp = EEXIST;
		return NULL;
	}
	splx(s);
	*errp = 0;
	return mfc;
}

/*
 * Get an IPv6 multicast routing cache entry
 */
static struct mfc6entry *
mfc6_get(key, errp)
	struct sockaddr_mcast6 *key;
	int *errp;
{
	register struct mfc6entry *mfc;
	register struct radix_node *rn;
	int s = splnet();

	if (mfc6_tree == NULL) {
		splx(s);
		*errp = ENOENT;
		return NULL;
	}
	rn = mfc6_tree->rnh_matchaddr((caddr_t)key, mfc6_tree);
	if ((rn == NULL) || (rn->rn_flags & RNF_ROOT)) {
		splx(s);
		*errp = ESRCH;
		return NULL;
	}
	mfc = (struct mfc6entry *)rn;
	splx(s);
	return mfc;
}

/*
 * Delete an IPv6 multicast routing cache entry
 */
static int
mfc6_delete(mfc)
	struct mfc6entry *mfc;
{
	register struct radix_node *rn;
	register struct ds_ifaddr *cur;
	int s = splnet();

	mfc->mfc6_flags &= ~RTF_UP;
	rn = mfc6_tree->rnh_deladdr((caddr_t)mfc6_key(mfc),
				    (caddr_t)0,
				    mfc6_tree);
	if (rn == NULL) {
		splx(s);
		return ESRCH;
	}
	if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
		panic("mfc6_delete");
	while ((cur = mfc->mfc6_ds_list.stqh_first) != NULL) {
		if ((mfc->mfc6_ds_list.stqh_first =
		  mfc->mfc6_ds_list.stqh_first->ds_list.stqe_next) == NULL)
			mfc->mfc6_ds_list.stqh_last =
			  &mfc->mfc6_ds_list.stqh_first;
		RM_Free(cur, sizeof(*cur));
	}
	if (mfc->mfc6_hold)
		m_freem(mfc->mfc6_hold);
	mfc->mfc6_hold = NULL;
	RM_Free(mfc, sizeof(*mfc) + ROUNDUP(mfc6_key(mfc)->sm6_len));
	splx(s);
	return 0;
}

/*
 * Add multicast routing entry user call
 */
static int
user_add_mfc6(mfccp)
	struct mfc6ctl *mfccp;
{
	struct mfc6entry *mfc;
	static struct sockaddr_mcast6 key = { sizeof key, AF_INET6 };
	struct ifnet *ifp;
	struct in6_ifaddr *ia;
	int error = 0;

	for (ifp = ifnet; ifp; ifp = ifp->if_next)
		if (ifp->if_index == mfccp->mfc6c_index)
			break;
	if (ifp == NULL)
		return ENXIO;
	IFP_TO_IA6(ifp, ia);
	if (ia == NULL)
		return ENETDOWN;

	COPY_ADDR6(mfccp->mfc6c_group, key.sm6_group);
	COPY_ADDR6(mfccp->mfc6c_origin, key.sm6_origin);
	mfc = mfc6_get(&key, &error);
	if (mfc == NULL) {
		if (error == ESRCH) {
			error = 0;
			mfc = mfc6_create(&key, ifp, 0, &error);
		}
		if (error)
			return error;
	}
	mfc->mfc6_upstream = ifp;
	return 0;
}

/*
 * Delete multicast routing entry user call
 */
static int
user_delete_mfc6(mfccp)
	struct mfc6ctl *mfccp;
{
	struct mfc6entry *mfc;
	static struct sockaddr_mcast6 key = { sizeof key, AF_INET6 };
	int error = 0;

	COPY_ADDR6(mfccp->mfc6c_group, key.sm6_group);
	COPY_ADDR6(mfccp->mfc6c_origin, key.sm6_origin);
	mfc = mfc6_get(&key, &error);
	if (mfc != NULL)
		error = mfc6_delete(mfc);
	return error;
}

/*
 * Get multicast routing entry infos user call
 */
static int
get_info(mb)
	struct mbuf *mb;
{
	register struct mfc6info *mfci = mtod(mb, struct mfc6info *);
	register struct mfc6entry *mfc;
	register struct ds_ifaddr *cur;
	static struct sockaddr_mcast6 key = { sizeof key, AF_INET6 };
	int s, error = 0;

	COPY_ADDR6(mfci->mfc6i_group, key.sm6_group);
	COPY_ADDR6(mfci->mfc6i_origin, key.sm6_origin);
	s = splnet();
	mfc = mfc6_get(&key, &error);
	if (mfc == NULL) {
		splx(s);
		return error;
	}

	mfci->mfc6i_index = mfc->mfc6_upstream->if_index;
	mfci->mfc6i_ifnum = 0;
	for (cur = mfc->mfc6_ds_list.stqh_first; cur; 
	     cur = cur->ds_list.stqe_next)
		mfci->mfc6i_ifnum++;
	mfci->mfc6i_flags = mfc->mfc6_flags;
	mfci->mfc6i_pktcnt = mfc->mfc6_pktcnt;
	mfci->mfc6i_bytecnt = mfc->mfc6_bytecnt;
	mfci->mfc6i_wrongif = mfc->mfc6_wrongif;
	mfci->mfc6i_lastuse = mfc->mfc6_lastuse;
	mb->m_len = sizeof(*mfci);
	splx(s);
	return error;
}

/*
 * Add downstream interface user call
 */
static int
user_add_dsif(mfccp)
	struct mfc6ctl *mfccp;
{
	struct mfc6entry *mfc;
	static struct sockaddr_mcast6 key = { sizeof key, AF_INET6 };
	struct ifnet *ifp;
	struct in6_ifaddr *ia;
	struct ds_ifaddr *cur;
	int s, error = 0;

	for (ifp = ifnet; ifp; ifp = ifp->if_next)
		if (ifp->if_index == mfccp->mfc6c_index)
			break;
	if (ifp == NULL)
		return ENXIO;
	IFP_TO_IA6(ifp, ia);
	if (ia == NULL)
		return EAFNOSUPPORT;
	if ((ifp->if_flags & IFF_MULTICAST) == 0)
		return EOPNOTSUPP;

	COPY_ADDR6(mfccp->mfc6c_group, key.sm6_group);
	COPY_ADDR6(mfccp->mfc6c_origin, key.sm6_origin);
	mfc = mfc6_get(&key, &error);
	if (mfc == NULL)
		return error;
	s = splnet();
	for (cur = mfc->mfc6_ds_list.stqh_first; cur;
	     cur = cur->ds_list.stqe_next)
		if (cur->ds_ifp == ifp)
			break;
	if (cur == NULL) {
		RM_Malloc(cur, struct ds_ifaddr *, sizeof(*cur));
		if (cur == NULL) {
			splx(s);
			return ENOBUFS;
		}

		cur->ds_list.stqe_next = NULL;
		*mfc->mfc6_ds_list.stqh_last = cur;
		mfc->mfc6_ds_list.stqh_last = &cur->ds_list.stqe_next;


		cur->ds_ifp = ifp;
	}
	cur->ds_threshold = mfccp->mfc6c_threshold;
	cur->ds_scope = mfccp->mfc6c_scope;
	splx(s);
	return 0;
}

/*
 * Delete downstream interface user call
 */
static int
user_delete_dsif(mfccp)
	struct mfc6ctl *mfccp;
{
	struct mfc6entry *mfc;
	static struct sockaddr_mcast6 key = { sizeof key, AF_INET6 };
	struct ifnet *ifp;
	struct ds_ifaddr *cur;
	int s, error = 0;

	for (ifp = ifnet; ifp; ifp = ifp->if_next)
		if (ifp->if_index == mfccp->mfc6c_index)
			break;
	if (ifp == NULL)
		return ENXIO;

	COPY_ADDR6(mfccp->mfc6c_group, key.sm6_group);
	COPY_ADDR6(mfccp->mfc6c_origin, key.sm6_origin);
	mfc = mfc6_get(&key, &error);
	if (mfc == NULL)
		return error;
	s = splnet();
	for (cur = mfc->mfc6_ds_list.stqh_first; cur;
	     cur = cur->ds_list.stqe_next)
		if (cur->ds_ifp == ifp)
			break;
	if (cur == NULL) {
		splx(s);
		return ESRCH;
	}

	/* remove it */
	if (mfc->mfc6_ds_list.stqh_first == cur) {
		if ((mfc->mfc6_ds_list.stqh_first =
		     mfc->mfc6_ds_list.stqh_first->ds_list.stqe_next) == NULL)
			mfc->mfc6_ds_list.stqh_last =
			  &mfc->mfc6_ds_list.stqh_first;
	} else {
		struct ds_ifaddr *curelm = mfc->mfc6_ds_list.stqh_first;
		while (curelm->ds_list.stqe_next != cur)
			curelm = curelm->ds_list.stqe_next;
		if ((curelm->ds_list.stqe_next =
		    curelm->ds_list.stqe_next->ds_list.stqe_next) == NULL)
			mfc->mfc6_ds_list.stqh_last =
			  &(curelm)->ds_list.stqe_next;
	}

	splx(s);
	RM_Free(cur, sizeof(*cur));
	return 0;
}

/*
 * Enable multicast routing entry user call
 */
static int
user_enable_mfc6(mfccp)
	struct mfc6ctl *mfccp;
{
	struct mfc6entry *mfc;
	static struct sockaddr_mcast6 key = { sizeof key, AF_INET6 };
	struct ifqueue *ifq;
	int s, error = 0;

	COPY_ADDR6(mfccp->mfc6c_group, key.sm6_group);
	COPY_ADDR6(mfccp->mfc6c_origin, key.sm6_origin);
	mfc = mfc6_get(&key, &error);
	if (mfc == NULL)
		return error;
	mfc->mfc6_flags |= RTF_UP;
	mfc->mfc6_lastuse = time;
	if (mfc->mfc6_hold == NULL)
		return error;

	/* Put held multicast packet back on input queue */
	s = splimp();
	ifq = &ipintrq;
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(mfc->mfc6_hold);
		error = ENOBUFS;
	} else
		IF_ENQUEUE(&ipintrq, mfc->mfc6_hold);
	mfc->mfc6_hold = NULL;
	splx(s);
	schednetisr(NETISR_IP);
	return error;
}

/*
 * Disable multicast routing entry user call
 */
static int
user_disable_mfc6(mfccp)
	struct mfc6ctl *mfccp;
{
	struct mfc6entry *mfc;
	static struct sockaddr_mcast6 key = { sizeof key, AF_INET6 };
	int error = 0;

	COPY_ADDR6(mfccp->mfc6c_group, key.sm6_group);
	COPY_ADDR6(mfccp->mfc6c_origin, key.sm6_origin);
	mfc = mfc6_get(&key, &error);
	if (mfc == NULL)
		return error;
	mfc->mfc6_flags &= ~RTF_UP;
	return 0;
}

#endif /* MROUTING6 */
#endif /* INET6 */
