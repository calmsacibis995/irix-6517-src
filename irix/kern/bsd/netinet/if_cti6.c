#ifdef INET6
/*
 * Configured Tunnel Interface (special case of SIT)
 */

#define NSIT    1
#define NCTI    1


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
#if NBFILTER > 0
#include <net/bpf.h>
#endif

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

int	ctioutput __P((struct ifnet *, struct mbuf *, struct sockaddr *,
		       struct rtentry *));
int	ctiioctl __P((struct ifnet *, int, void *));

void
ctiadd(cnt)
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
		ifp->if_unit = i - NSIT;
		ifp->if_name = "cti";
		ifp->if_mtu = SITMTU;
		ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		ifp->if_ioctl = ctiioctl;
		ifp->if_output = ctioutput;
		ifp->if_type = IFT_OTHER;
		sc->sit_llinfo.sit_ip.ip_ttl = MAXTTL;
		sc->sit_llinfo.sit_ip.ip_p = IPPROTO_IPV6;
		ifp->if_hdrlen = sizeof(struct ip);
		ifp->if_addrlen = 8;
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
/* ARGSUSED */
int
ctioutput(ifp, m0, dst, rt)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	register struct mbuf *m;
	register struct llinfo_sit *ll;
	struct ip *ip;
	int error, len;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_SITOUT)
		printf("ctioutput(%p,%p,%p,%p) -> %s\n",
		       ifp, m0, dst, rt,
		       ip6_sprintf(&((struct sockaddr_in6 *)dst)->sin6_addr));
#endif

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		m_freem(m0);
		return (ENETDOWN);
	}

	ifp->if_lastchange = time;
	ifp->if_opackets++;
	ll = &((struct sit_softc *)ifp)->sit_llinfo;

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

	/*
	 * Fill in IPv4 header.
	 */
	bcopy((caddr_t)&ll->sit_ip, mtod(m, caddr_t), sizeof(struct ip));
	ip = mtod(m, struct ip *);
	ip->ip_len = len;
	ifp->if_obytes += len;

	/*
	 * Output final datagram.
	 */
#ifdef IP6PRINTFS
	if (ip6printfs & D6_SITOUT)
		printf("ctioutput src %08x dst %08x len %d\n",
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
 * Ioctls on CTI pseudo-interfaces.
 */

int
ctiioctl(ifp, cmd, data)
	struct ifnet *ifp;
	int cmd;
	void *data;
{
	struct ifaddr *ifa;
	struct ifreq *ifr;
	struct in6_addr *addr;
#if 0
	struct sockaddr_dl *sdl = 0;
#endif
	static struct sockaddr_in sin = {sizeof(sin), AF_INET};
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
			printf("ctiioctl ifp %p go up %s\n", ifp,
			    ip6_sprintf(&satosin6(ifa->ifa_addr)->sin6_addr));
#endif
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
			printf("ctiioctl sets src %lx\n",
			       ntohl(sin.sin_addr.s_addr));
#endif
		((struct sit_softc *)ifp)->sit_src = sin.sin_addr;
		ifp->if_flags |= IFF_UP;
#if 0
		if (ifp->if_addrlist && ifp->if_addrlist->ifa_addr &&
		    (ifp->if_addrlist->ifa_addr->sa_family == AF_LINK)) {
			sdl = (struct sockaddr_dl *)ifp->if_addrlist->ifa_addr;
			bcopy(&sin.sin_addr, LLADDR(sdl), 4);
			sdl->sdl_alen = 8;
		}
#endif
		if (((struct sockaddr_new *)ifa->ifa_dstaddr)->sa_family !=
		  AF_INET6)
			break;
		/* falls in */

	case SIOCSIFDSTADDR:
		ifa = (struct ifaddr *)data;
		if ((ifa == 0) ||
		  (((struct sockaddr_new *)ifa->ifa_dstaddr)->sa_family !=
		  AF_INET6)) {
			error = EAFNOSUPPORT;
			break;
		}
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("ctiioctl ifp %p dst %s\n", ifp,
			  ip6_sprintf(&satosin6(ifa->ifa_dstaddr)->sin6_addr));
#endif
		addr = &satosin6(ifa->ifa_dstaddr)->sin6_addr;
		sin.sin_addr.s_addr = addr->s6_addr32[3];
		if ((addr->s6_addr32[0] != 0) ||
		    (addr->s6_addr32[1] != 0) ||
		    (addr->s6_addr32[2] != 0) ||
		    (addr->s6_addr32[3] == 0))
			break;
#ifdef IP6PRINTFS
		if (ip6printfs & D6_SITCTL)
			printf("ctiioctl sets dst %lx\n",
			       ntohl(sin.sin_addr.s_addr));
#endif
		((struct sit_softc *)ifp)->sit_dst = sin.sin_addr;
#if 0
		if (ifp->if_addrlist && ifp->if_addrlist->ifa_addr &&
		    (ifp->if_addrlist->ifa_addr->sa_family == AF_LINK)) {
			sdl = (struct sockaddr_dl *)ifp->if_addrlist->ifa_addr;
			bcopy(&sin.sin_addr, LLADDR(sdl) + 4, 4);
				sdl->sdl_alen = 8;
		}
#endif
		ifp->if_flags |= IFF_RUNNING;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		if ((ifr == 0) ||
		  (((struct sockaddr_new *)&ifr->ifr_addr)->sa_family !=
		  AF_INET6)) {
			error = EAFNOSUPPORT;
			break;
		}
		break;

	default:
		error = EINVAL;
	}
	return (error);
}
#endif /* INET6 */
