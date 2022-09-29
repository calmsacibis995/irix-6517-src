#ifdef INET6
/*
 * Simple Internet Transition.
 */

#define NSIT	1
#define NCTI	1

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/kmem.h>
#include <net/soioctl.h>
#include <sys/tcpipstats.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/ip6_icmp.h>
#include <netinet/if_ndp6.h>
#include <netinet/if_sit6.h>
#include <sys/debug.h>

/*#include "bpfilter.h"*/
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

struct sit_head sitif;
struct sit_softc *last_sitif;
u_int sitcnt, cticnt;

extern	int arpt_keep, arpt_prune;	/* use same values as arp cache */
#define rt_expire	rt_rmx.rmx_expire

int	sitoutput __P((struct ifnet *, struct mbuf *, struct sockaddr *,
		       struct rtentry *));
int	sitioctl __P((struct ifnet *, int, void *));
void	sit_rtrequest __P((int, struct rtentry *, struct sockaddr *));

/*
 * attach SIT interfaces
 */

/* ARGSUSED */
void
sitattach(n)
	int n;
{
	sitif.stqh_first = NULL;
	sitif.stqh_last = &sitif.stqh_first;

	/* add preconfigured SIT interfaces */
	sitadd(NSIT);
	/* add preconfigured CTI interfaces */
#if NCTI > 0
	ctiadd(NSIT + NCTI);
#endif
	last_sitif = sitif.stqh_first;

	/* don't start at 1970/01/01 at 0h00 */
	if (time == 0)
		time++;
}


void
sitadd(cnt)
	u_int cnt;
{
	register u_int i;
	register struct sit_softc *sc;
	register struct ifnet *ifp;

	for (i = sitcnt; i < cnt; i++) {
		sc = kmem_alloc(sizeof(struct sit_softc), KM_NOSLEEP);
		if (sc == NULL)
			break;
		bzero(sc, sizeof(struct sit_softc));

		sc->sit_list.stqe_next = NULL;
		*sitif.stqh_last = sc;
		sitif.stqh_last = &sc->sit_list.stqe_next;


		sitcnt = i + 1;
		ifp = &sc->sit_if;
		ifp->if_unit = i;
		ifp->if_name = "sit";
		ifp->if_mtu = SITMTU;
#if 0 /* XXX6 */
		ifp->if_flags = SIT_NBMA;
#endif
		ifp->if_ioctl = sitioctl;
		ifp->if_output = sitoutput;
		ifp->if_type = IFT_OTHER;
		sc->sit_llinfo.sit_ip.ip_ttl = MAXTTL;
		sc->sit_llinfo.sit_ip.ip_p = IPPROTO_IPV6;
		ifp->if_hdrlen = sizeof(struct ip);
		ifp->if_addrlen = 4;
		ifp->if_6llocal = &sc->sit_llip6;
		ifp->if_ndtype = IFND6_SIT;
		if_attach(ifp);
#if NBPFILTER > 0
		bpfattach(ifp, DLT_NULL, sizeof(u_int));
#endif
	}
	cticnt = sitcnt - NSIT;
}

/*
 * output of IPv6 packets by encapsulation in IPv4
 */
int
sitoutput(ifp, m0, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	register struct mbuf *m;
	register struct rtentry *rt;
	register struct llinfo_sit *ll;
	struct ip *ip;
	int error, len;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_SITOUT)
		printf("sitoutput(%p,%p,%p,%p) -> %s\n",
		       ifp, m0, dst, rt0,
		       ip6_sprintf(&((struct sockaddr_in6 *)dst)->sin6_addr));
#endif

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		m_freem(m0);
		return (ENETDOWN);
	}

	ifp->if_lastchange = time;
	ifp->if_opackets++;

	ROUTE_RDLOCK();
	if ((rt = rt0) != 0) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			if ((rt0 = rt = rtalloc1(dst, 1)) != 0)
				RT_RELE(rt);
			else {
				ROUTE_UNLOCK();
				m_freem(m0);
				return (EHOSTUNREACH);
			}
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1);
				if ((rt = rt->rt_gwroute) == 0) {
					ROUTE_UNLOCK();
					m_freem(m0);
					return (EHOSTUNREACH);
				}
			}
		}
		if ((ll = (struct llinfo_sit *)rt->rt_llinfo) == 0 ||
		    (ll->sit_flags & RTF_UP) == 0) {
			ROUTE_UNLOCK();
			m_freem(m0);
			return (EHOSTUNREACH);
		}
		if (rt->rt_expire)
			rt->rt_expire = time + arpt_keep;
		ROUTE_UNLOCK();
	} else {
		ROUTE_UNLOCK();
		return (EHOSTDOWN);	/* ll = &sc->sit_llinfo; ??? */
	}

#if NBFILTER > 0
	if (ifp->if_bpf) {
		/* too soon ?! */
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a dummy header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer to it).
		 */
		struct mbuf m;
		u_int af = dst->sa_family;

		m.m_next = m0;
		m.m_len = 4;
		m.m_data = (char *)&af;

		bpf_mtap(ifp, &m);
	}
#endif

	len = 0;
	for (m = m0; m; m = m->m_next)
		len += m->m_len;
	/*
	 * Get a header mbuf.
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		m_freem(m0);
		return (ENOBUFS);
	}
	m->m_len = sizeof(struct ip);
	m->m_next = m0;
	len += sizeof(struct ip);
	bcopy((caddr_t)&ll->sit_ip, mtod(m, caddr_t), sizeof(struct ip));

	/*
	 * Fill in IPv4 header.
	 */
	ip = mtod(m, struct ip *);
	ip->ip_len = len;
	ifp->if_obytes += len;

	/*
	 * Output final datagram.
	 */
#ifdef IP6PRINTFS
	if (ip6printfs & D6_SITOUT)
		printf("sitoutput src %08x dst %08x len %d\n",
		       ntohl(ip->ip_src.s_addr),
		       ntohl(ip->ip_dst.s_addr),
		       len);
#endif
	error = ip_output(m, (struct mbuf *)0, &ll->sit_route, 0, NULL, NULL);
	if (error)
		ifp->if_oerrors++;
	return (error);
}

/*
 * Ioctls on SIT pseudo-interfaces.
 */

int
sitioctl(ifp, cmd, data)
	struct ifnet *ifp;
	int cmd;
	void *data;
{
	struct ifaddr *ifa;
	struct in6_addr *addr;
#if 0
	struct sockaddr_dl *sdl = 0;
#endif
#ifdef _HAVE_SA_LEN
	struct sockaddr_in sin = {sizeof(sin), AF_INET};
#else
	struct sockaddr_in sin = {AF_INET};
#endif
	int error = 0;

	ASSERT(IFNET_ISLOCKED(ifp));

	switch (cmd) {

	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		if ((ifa == 0) ||
		  (((struct sockaddr_new *)ifa->ifa_addr)->sa_family !=
		  AF_INET6)) {
			error = EAFNOSUPPORT;
			break;
		}
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("sitioctl ifp %p go up %s\n", ifp,
			    ip6_sprintf(&satosin6(ifa->ifa_addr)->sin6_addr));
#endif
		ifa->ifa_rtrequest = sit_rtrequest;
		ifa->ifa_flags |= RTF_CLONING;
		addr = &satosin6(ifa->ifa_addr)->sin6_addr;
		if (IS_LINKLADDR6(*addr)) {
			COPY_ADDR6(*addr,
				   ((struct sit_softc *)ifp)->sit_llip6);
			ifp->if_ndtype |= IFND6_LLSET;
			break;
		}
		sin.sin_addr.s_addr = addr->s6_addr32[3];
		if ((addr->s6_addr32[0] != 0) ||
		    (addr->s6_addr32[1] != 0) ||
		    (addr->s6_addr32[2] != 0) ||
		    (ifa_ifwithaddr((struct sockaddr *)&sin) == 0))
			break;
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("sitioctl sets src %lx\n",
			       ntohl(sin.sin_addr.s_addr));
#endif
		((struct sit_softc *)ifp)->sit_src = sin.sin_addr;
		ifp->if_flags |= IFF_UP | IFF_RUNNING;
#if 0
		if (ifp->if_addrlist && ifp->if_addrlist->ifa_addr &&
		    (ifp->if_addrlist->ifa_addr->sa_family == AF_LINK)) {
			sdl = (struct sockaddr_dl *)ifp->if_addrlist->ifa_addr;
			bcopy(&sin.sin_addr, LLADDR(sdl), sdl->sdl_alen = 4);
		}
#endif
		break;

	default:
		error = EINVAL;
	}
	return (error);
}

/*
 * IPv6-into-IPv4 input routine.
 */

/*ARGSUSED*/
void
sit_input(struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec,
  struct mbuf *unused)
{
	struct ip *ip = mtod(m, struct ip *);
	struct sit_softc *sc = last_sitif;
	struct mbuf *n;
	int iphlen;

	iphlen = ip->ip_hl << 2;
	if ((ifp->if_flags & IFF_POINTOPOINT) &&
	    (sc->sit_dst.s_addr == ip->ip_src.s_addr) &&
	    (sc->sit_src.s_addr == ip->ip_dst.s_addr)) {
	  /* success */
	} else {
		last_sitif = sitif.stqh_first;
		for (sc = last_sitif; sc; sc = sc->sit_list.stqe_next) {
			ifp = &sc->sit_if;
			if (((ifp->if_flags & IFF_UP) == 0) ||
			    ((ifp->if_flags & IFF_RUNNING) == 0))
				continue;
			if ((ifp->if_flags & IFF_POINTOPOINT) == 0) {
				if (sc->sit_src.s_addr == ip->ip_dst.s_addr)
					last_sitif = sc;
			} else if ((sc->sit_dst.s_addr == ip->ip_src.s_addr) &&
				   (sc->sit_src.s_addr == ip->ip_dst.s_addr)) {
				last_sitif = sc;
				break;
			}
		}
		if (sc == 0)
			ifp = &last_sitif->sit_if;
	}
	ifp->if_ipackets++;
	for (n = m; n; n = n->m_next)
		ifp->if_ibytes += n->m_len;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_SITIN)
		printf("sit_input src %08x dst %08x len %d\n",
		       ntohl(ip->ip_src.s_addr),
		       ntohl(ip->ip_dst.s_addr),
		       ip->ip_len + iphlen);
#endif
	/*
	 * Strip IPv4 header including options.
	 */
	m_adj(m, iphlen);

	/*
	 * Get IPv6 header.
	 */
	if (m->m_len < sizeof (struct ipv6) &&
	    (m = m_pullup(m, sizeof (struct ipv6))) == 0) {
		IP6STAT(ip6s_toosmall);
		return;
	}

	ip6_input(m, ifp, NULL, NULL);
}

/*
 * TODO: sit_ctlinput()
 */

/*
 * Parallel to other *rtrequest.
 */
/* ARGSUSED */
void
sit_rtrequest(req, rt, sa)
	int req;
	register struct rtentry *rt;
	struct sockaddr *sa;
{
	register struct sockaddr *gate = rt->rt_gateway;
	register struct llinfo_sit *ll = (struct llinfo_sit *)rt->rt_llinfo;
	struct in_addr addr;
	struct route *ro;
#ifdef _HAVE_SA_LEN
	struct sockaddr_in null_in = {sizeof(null_in), AF_INET};
#else
	struct sockaddr_in null_in = {AF_INET};
#endif

#define SIN(sa)	((struct sockaddr_in *)sa)

	if (rt->rt_flags & RTF_GATEWAY)
		return;
#ifdef IP6PRINTFS
	if (ip6printfs & D6_SITCTL)
		printf("sit_rtrequest(%d,%p,%p)\n", req, rt, sa);
#endif
	switch (req) {

	case RTM_ADD:
		if ((rt->rt_flags & RTF_CLONING) &&
		    (rt->rt_flags & RTF_HOST) &&
		    (rt->rt_ifa->ifa_flags & RTF_HOST)) {
#ifdef IP6PRINTFS
			if (ip6printfs & D6_SITCTL)
				printf("sit_rtrequest: uncloning\n");
#endif
			rt->rt_flags &= ~RTF_CLONING;
			rt_setgate(rt, rt_key(rt),
				   (struct sockaddr *)&null_in);
			gate = rt->rt_gateway;
		}
		if (rt->rt_flags & RTF_CLONING) {
			/*
			 * This route should come from a route to iface.
			 */
#ifdef IP6PRINTFS
			if (ip6printfs & D6_SITCTL)
				printf("sit_rtrequest: cloning\n");
#endif
			rt_setgate(rt, rt_key(rt),
				   (struct sockaddr *)&null_in);
			gate = rt->rt_gateway;
			/*
			 * Give this route an expiration time !?
			 */
			rt->rt_expire = time;
			return;
		}
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("sit_rtrequest: resolve\n");
#endif
		/*
		 * Have we get both IPv4 addresses ?
		 */
		if ((gate->sa_family != AF_INET)
#ifdef _HAVE_SA_LEN
			||
		    (gate->sa_len != sizeof(struct sockaddr_in))
#endif
			    ) {
			log(LOG_ERR, "sit_rtrequest: invalid gateway\n");
			return;
		}
		addr.s_addr = IA_SIN6(rt->rt_ifa)->sin6_addr.s6_addr32[3];
		if (ll != 0)
			break;		/* This happens on a route change */
		/*
		 * This route may come from cloning or
		 * a manual route add with an IPv4 address.
		 */
		R_Malloc(ll, struct llinfo_sit *, sizeof(*ll));
		rt->rt_llinfo = (caddr_t)ll;
		if (ll == 0)
			return;
		Bzero(ll, sizeof(*ll));
		ll->sit_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		if (req == RTM_RESOLVE)
			rt->rt_flags |= RTF_DYNAMIC;
		if (SAME_SOCKADDR((struct sockaddr_in6 *)rt_key(rt),
				  IA_SIN6(rt->rt_ifa))) {
			extern struct  ifnet loif;
#ifdef IP6PRINTFS
			if (ip6printfs & D6_SITCTL)
				printf("sit_rtrequest: local hack\n");
#endif
			/* 0 or time + arpt_keep ??? */
			rt->rt_expire = 0;
			SIN(gate)->sin_addr = addr;
			rt->rt_ifp = &loif;
			rt->rt_rmx.rmx_mtu = loif.if_mtu;
			return;
		}
		break;

	case RTM_DELETE:
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("sit_rtrequest: delete\n");
#endif
		if (ll == 0)
			return;
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~(RTF_LLINFO | RTF_DYNAMIC);
		if (ll->sit_route.ro_rt)
			RTFREE(ll->sit_route.ro_rt);
		Free((caddr_t)ll);
		return;

	 case RTM_EXPIRE:
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("sit_rtrequest: expire\n");
#endif
		if (ll == 0) {
			log(LOG_ERR,
			    "IPv6 SIT route expire without llinfo!\n");
			return;
		}
		if (rt->rt_refcnt > 0)
			return;
		rtrequest(RTM_DELETE, rt_key(rt), 0, rt_mask(rt), 0, 0);
		return;
	}
	if (((rt->rt_ifp->if_flags & IFF_UP) == 0) ||
	    ((rt->rt_ifp->if_flags & IFF_RUNNING) == 0)) {
		log(LOG_NOTICE, "sit_rtrequest on down ifp\n");
		return;
	}

	/*
	 * Automatic mapping.
	 */
	if (((rt->rt_ifp->if_flags & IFF_POINTOPOINT) == 0) &&
	    (SIN(gate)->sin_addr.s_addr == INADDR_ANY)) {
		register struct sockaddr_in6 *sin;
		struct in_addr in;

		sin = (struct sockaddr_in6 *)rt_key(rt);
		if ((sin->sin6_family != AF_INET6) ||
		    (sin->sin6_len != sizeof(*sin))) {
			log(LOG_ERR, "sit_rtrequest: invalid destination\n");
			return;
		}
		in.s_addr = sin->sin6_addr.s6_addr32[3];
		if ((sin->sin6_addr.s6_addr32[0] != 0) ||
		    (sin->sin6_addr.s6_addr32[1] != 0) ||
		    (sin->sin6_addr.s6_addr32[2] != 0) ||
		    !in_canforward(in)) {
			log(LOG_NOTICE, "sit_rtrequest: bad mapping\n");
			return;
		}
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("sit_rtrequest auto dst %08x\n",
			       ntohl(in.s_addr));
#endif
		SIN(gate)->sin_addr = in;
	}
	/*
	 * Point-to-point case
	 */
	if (SIN(gate)->sin_addr.s_addr == INADDR_ANY) {
		register struct sit_softc *sc;

		sc = (struct sit_softc *)(rt->rt_ifp);
		SIN(gate)->sin_addr = sc->sit_dst;
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("sit_rtrequest p2p dst %08x\n",
			       ntohl(SIN(gate)->sin_addr.s_addr));
#endif
	}

	/*
	 * Fill llinfo_sit structure.
	 */
	ll->sit_ip.ip_ttl = MAXTTL;
	ll->sit_ip.ip_p = IPPROTO_IPV6;
	ll->sit_ip.ip_src = addr;
	ll->sit_ip.ip_dst = SIN(gate)->sin_addr;
	ro = &ll->sit_route;
	ro->ro_dst.sa_family = AF_INET;
#ifdef _HAVE_SA_LEN
	ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
#endif
	SIN(&ro->ro_dst)->sin_addr = SIN(gate)->sin_addr;

	/*
	 * If there is a cached route,
	 * check that it is to the same destination
	 * and is still up.  If not, free it and try again.
	 */
	if (ro->ro_rt) {
		struct sockaddr *dst = rt_key(ro->ro_rt);

		if ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
		    SIN(gate)->sin_addr.s_addr != SIN(dst)->sin_addr.s_addr) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}
	}
	rtalloc(ro);
	if (ro->ro_rt)
		ro->ro_rt->rt_use++;
	if (ro->ro_rt && (rt->rt_rmx.rmx_mtu == 0)) {
		if (ro->ro_rt->rt_rmx.rmx_mtu)
			rt->rt_rmx.rmx_mtu =
				ro->ro_rt->rt_rmx.rmx_mtu - sizeof(struct ip);
		else
			rt->rt_rmx.rmx_mtu =
				ro->ro_rt->rt_ifp->if_mtu - sizeof(struct ip);
	} else if (rt->rt_rmx.rmx_mtu == 0)
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
	ll->sit_flags |= RTF_UP;
}
#endif /* INET6 */
