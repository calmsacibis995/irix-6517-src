#ifdef INET6
/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)udp_usrreq.c	8.4 (Berkeley) 1/21/94
 */

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/tcpipstats.h>

#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include <net/if.h>
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
#include <netinet/ip_icmp.h>
#include <netinet/ip6_icmp.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/udp6_var.h>

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */
extern int udp_ttl;

extern	void udp_detach __P((struct inpcb *));
extern	void udp_notify __P((struct inpcb *, int, void *));
static	struct mbuf *udp6_saverecv __P((struct mbuf *, struct mbuf *, int,
  struct ifnet *));

/*ARGSUSED*/
void
udp6_input(m, ifp, ipsec, opts)
	register struct mbuf *m;
	struct ifnet *ifp;
	struct ipsec *ipsec;
	struct mbuf *opts;
{
	register struct ipv6 *ip;
	register struct udphdr *uh;
	register struct inpcb *inp, *lastinp;
	struct in_pcbhead *hinp;
	int len, ipsec_failed = 0;
	struct ip6ovck save_ip;
	struct mbuf *save_opts = 0;
	u_short hashval;
	struct ifnet *ifns;
	struct	sockaddr_in6 udp_in6;
	extern struct sockaddr_in6 in6_zeroaddr;

	UDPSTAT(udps_ipackets);

	/*
	 * Get IPv6 and UDP header together in first mbuf.
	 */
	ip = mtod(m, struct ipv6 *);
#ifdef IP6PRINTFS
	if (ip6printfs & D6_UDP) {
		printf("udp6_input(%p,%p)", m, opts);
		printf(" src %s dst %s len %d mbuf(%d)\n",
		       ip6_sprintf(&ip->ip6_src),
		       ip6_sprintf(&ip->ip6_dst),
		       ip->ip6_len, m->m_len);
	}
#endif
	if (m->m_len < sizeof(struct udpip6hdr)) {
		if ((m = m_pullup(m, sizeof(struct udpip6hdr))) == 0) {
			UDPSTAT(udps_hdrops);
			goto end;
		}
		ip = mtod(m, struct ipv6 *);
	}
	uh = (struct udphdr *)(ip + 1);
#ifdef IP6PRINTFS
	if (ip6printfs & D6_UDP)
		printf("udp6_input sport %d dpost %d ulen %d\n",
		       ntohs(uh->uh_sport),
		       ntohs(uh->uh_dport),
		       ntohs(uh->uh_ulen));
#endif
	/*
	 * Make mbuf data length reflect UDP length.
	 * If not enough data to reflect UDP length, drop.
	 */
	len = ntohs((u_int16_t)uh->uh_ulen);
	if (ip->ip6_len != len) {
		if (len > ip->ip6_len || len < sizeof(struct udphdr)) {
			UDPSTAT(udps_badlen);
			goto bad;
		}
		m_adj(m, len - ip->ip6_len);
		/* ip->ip6_len = len; */
	}

	/*
	 * Save a copy of the IP header in case we want restore it
	 * for sending an ICMPv6 error message in response.
	 */
	save_ip = *((struct ip6ovck *)ip);

	/*
	 * Checksum extended UDP header and data.
	 */
	{
		register struct ip6ovck *ipc = (struct ip6ovck *)ip;

		ipc->ih6_wrd1 = ipc->ih6_wrd0 = 0;
		ipc->ih6_pr = IPPROTO_UDP;
		ipc->ih6_len = (u_int16_t)uh->uh_ulen;
		if ((uh->uh_sum = in_cksum(m, len + sizeof (struct ipv6))) != 0) {
			UDPSTAT(udps_badsum);
			goto bad;
		}
		*ipc = save_ip;
	}

        udp_in6 = in6_zeroaddr;
	if (IS_MULTIADDR6(ip->ip6_dst)) {
		struct inpcb *head = &udb;
		struct socket *last;
		struct inpcb *inpnxt;
		/*
		 * Deliver a multicast or broadcast datagram to *all* sockets
		 * for which the local and remote addresses and ports match
		 * those of the incoming datagram.  This allows more than
		 * one process to receive multi/broadcasts on the same port.
		 * (This really ought to be done for unicast datagrams as
		 * well, but that would cause problems with existing
		 * applications that open both address-specific sockets and
		 * a wildcard socket listening to the same port -- they would
		 * end up receiving duplicates of every unicast datagram.
		 * Those applications open the multiple sockets to overcome an
		 * inadequacy of the UDP socket interface, but for backwards
		 * compatibility we avoid the problem here rather than
		 * fixing the interface.  Maybe 4.5BSD will remedy this?)
		 */

		/*
		 * Construct sockaddr format source address.
		 */
		udp_in6.sin6_port = uh->uh_sport;
		udp_in6.sin6_flowinfo = ip->ip6_head & IPV6_FLOWINFO_PRIFLOW;
		COPY_ADDR6(ip->ip6_src, udp_in6.sin6_addr);
		m->m_len -= sizeof (struct udpip6hdr);
		m->m_off += sizeof (struct udpip6hdr);

#define MULTI6_GETOPT(so, opt)  opt = (struct mbuf *)0;

		/*
		 * Locate pcb(s) for datagram.
		 * (Algorithm copied from raw_intr().)
		 */
		hashval = (*head->inp_hashfun)(head, &ip->ip6_src,
		  uh->uh_sport, &ip->ip6_dst, uh->uh_dport, AF_INET6);
resync:
                /*
		 * This section of code takes advantage of the fact that
		 * we know that UDP inpcb's hash into exactly one
		 * list per port number.
		 */
		last = NULL;
		hinp = &head->inp_table[hashval];
		INHHEAD_LOCK(hinp);
		for (inp = hinp->hinp_next;
		  inp != (struct inpcb *)hinp; inp = inpnxt) {
			inpnxt = inp->inp_next;
			if (inp->inp_lport != uh->uh_dport)
				continue;
			if ((inp->inp_flags & INP_COMPATV6) == 0)
				continue;
			if ((inp->inp_latype != IPATYPE_UNBD) &&
			    ((inp->inp_latype & IPATYPE_IPV6) == 0 ||
			     !SAME_ADDR6(inp->inp_laddr6, ip->ip6_dst)))
					continue;
			if ((inp->inp_fatype != IPATYPE_UNBD) &&
			    ((inp->inp_fatype & IPATYPE_IPV6) == 0 ||
			     !SAME_ADDR6(inp->inp_faddr6, ip->ip6_src) ||
			     inp->inp_fport != uh->uh_sport))
					continue;
			if ((inp->inp_flags & INP_NEEDAUTH) &&
			    ((m->m_flags & M_AUTH) == 0 ||
			     !ipsec_match(inp, m, opts, INP_NEEDAUTH))) {
				ipsec_failed = 1;
				continue;
			}
			if ((inp->inp_flags & INP_NEEDCRYPT) &&
			    ((m->m_flags & M_CRYPT) == 0 ||
			     !ipsec_match(inp, m, opts, INP_NEEDCRYPT))) {
				ipsec_failed = 1;
				continue;
			}

			INPCB_HOLD(inp);
			if (last != NULL) {
				struct mbuf *n, *o;

				MULTI6_GETOPT(last, o);
				if ((n = m_copy(m, 0, M_COPYALL)) != NULL) {
					lastinp = sotoinpcb(last);
					INHHEAD_UNLOCK(hinp);
					SOCKET_LOCK(last);
					if (last->so_options & SO_PASSIFNAME)
						ifns = ifp;
					else
						ifns = 0;
					if (sbappendaddr(&last->so_rcv,
						sin6tosa(&udp_in6),
						n, o, ifns) == 0) {
						m_freem(n);
						if (o)
							m_freem(o);
						UDPSTAT(udps_fullsock);
					} else
						sorwakeup(last, NETEVENT_UDPUP);
/*
 * XXX This locking code was copied from the v4 udp_input().  It has a
 * race (in v4 and hence now in v6).  The race is that after the
 * head lock is dropped the list may change such that we see a pcb twice.
 * entering this case should be rare so we do not have an urgent need for a
 * fix.  in_pcbnotify() has the same race.
 */
					if (!INPCB_RELE(lastinp))
						SOCKET_UNLOCK(last);
					INHHEAD_LOCK(hinp);
					if (inp->inp_next != inpnxt) {
						struct socket *so =
						  inp->inp_socket;
						
						INHHEAD_UNLOCK(hinp);
						SOCKET_LOCK(so);
						if (!INPCB_RELE(inp))
							SOCKET_UNLOCK(so);
						INHHEAD_LOCK(hinp);
						goto resync;
					}
				}
			}
			last = inp->inp_socket;
			/*
			 * Don't look for additional matches if this one does
			 * not have either the SO_REUSEPORT or SO_REUSEADDR
			 * socket options set.  This heuristic avoids searching
			 * through all pcbs in the common case of a non-shared
			 * port.  It * assumes that an application will never
			 * clear these options after setting them.
			 */
			if ((last->so_options&(SO_REUSEPORT|SO_REUSEADDR)) == 0)
				break;
		}
		INHHEAD_UNLOCK(hinp);

		if (last == NULL) {
			/*
			 * No matching pcb found; discard datagram.
			 * (No need to send an ICMPv6 Port Unreachable
			 * for a broadcast or multicast datgram.)
			 */
			if (ipsec_failed == 0)
				UDPSTAT(udps_noportbcast);
			goto bad;
		}
		lastinp = sotoinpcb(last);
		MULTI6_GETOPT(last, save_opts);
		SOCKET_LOCK(last);
		if (last->so_options & SO_PASSIFNAME)
			ifns = ifp;
		else
			ifns = 0;
		if (sbappendaddr(&last->so_rcv, sin6tosa(&udp_in6),
		    m, save_opts, 0) == 0) {
			UDPSTAT(udps_fullsock);
			if (!INPCB_RELE(lastinp))
				SOCKET_UNLOCK(last);
			goto bad;
		}
		sorwakeup(last, NETEVENT_UDPUP);
		if (!INPCB_RELE(lastinp))
			SOCKET_UNLOCK(last);
		if (opts)
			m_freem(opts);
		return;
	}
	inp = in_pcblookupx(&udb, &ip->ip6_src, uh->uh_sport,
	    &ip->ip6_dst, uh->uh_dport, INPLOOKUP_WILDCARD, AF_INET6);
	if (inp &&
	    (inp->inp_flags & INP_NEEDAUTH) &&
	    ((m->m_flags & M_AUTH) == 0 ||
	     !ipsec_match(inp, m, opts, INP_NEEDAUTH))) {
		struct socket *so2 = inp->inp_socket;
#ifdef IP6PRINTFS
		if (ip6printfs & D6_UDP)
			printf("udp6_input no AH\n");
#endif
		SOCKET_LOCK(so2);
		if (!INPCB_RELE(inp))
			SOCKET_UNLOCK(so2);
		inp = NULL;
	}
	if (inp &&
	    (inp->inp_flags & INP_NEEDCRYPT) &&
	    ((m->m_flags & M_CRYPT) == 0 ||
	     !ipsec_match(inp, m, opts, INP_NEEDCRYPT))) {
		struct socket *so2 = inp->inp_socket;
#ifdef IP6PRINTFS
		if (ip6printfs & D6_UDP)
			printf("udp6_input no ESP\n");
#endif
		SOCKET_LOCK(so2);
		if (!INPCB_RELE(inp))
			SOCKET_UNLOCK(so2);
		inp = NULL;
	}
	if (inp == NULL) {
		UDPSTAT(udps_noport);
#ifdef IP6PRINTFS
		if (ip6printfs & D6_UDP)
			printf("udp6_input can't find inpcb\n");
#endif
		if (m->m_flags & (M_BCAST | M_MCAST)) {
			UDPSTAT(udps_noportbcast);
			goto bad;
		}
		icmp6_error(m, ICMP6_UNREACH, ICMP6_UNREACH_PORT, 0, ifp);
		goto end;
	}
#ifdef IP6PRINTFS
	if (ip6printfs & D6_UDP)
		printf("udp6_input finds inpcb %p\n", inp);
#endif

	/*
	 * Construct sockaddr format source address.
	 * Stuff source address and datagram in user buffer.
	 */
	udp_in6.sin6_port = uh->uh_sport;
	udp_in6.sin6_flowinfo = ip->ip6_head & IPV6_FLOWINFO_PRIFLOW;
	COPY_ADDR6(ip->ip6_src, udp_in6.sin6_addr);
	if (inp->inp_flags & INP_CONTROLOPTS) {
		if (opts) {
			opts = ip6_dropoption(opts, IP6_NHDR_ESP);
			opts = ip6_dropoption(opts, IP6_NHDR_AUTH);
			opt6_reverse(ip, opts);
		}

		save_opts = udp6_saverecv(m, opts, inp->inp_flags, ifp);
	}
	if (opts)
		m_freem(opts);

	m->m_len -= sizeof(struct udpip6hdr);
	m->m_off += sizeof(struct udpip6hdr);
	SOCKET_LOCK(inp->inp_socket);
	if (inp->inp_socket->so_options & SO_PASSIFNAME)
		ifns = ifp;
	else
		ifns = 0;
	if (sbappendaddr(&inp->inp_socket->so_rcv,
			 sin6tosa(&udp_in6), m, save_opts, ifns) == 0) {
		struct socket *so2 = inp->inp_socket;
		UDPSTAT(udps_fullsock);
		m_freem(m);
		if (!INPCB_RELE(inp))
			SOCKET_UNLOCK(so2);
		goto bad1;
	}
	{ struct socket *so2 = inp->inp_socket;
	sorwakeup(inp->inp_socket, NETEVENT_UDPUP);
	if (!INPCB_RELE(inp))
		SOCKET_UNLOCK(so2);
	}
	return;
bad:
	m_freem(m);
end:
	if (opts)
		m_freem(opts);
bad1:
	if (save_opts)
		m_free(save_opts);
}

/*
 * Create a "control" mbuf containing all the received informations.
 */
/* ARGSUSED */
struct mbuf *
udp6_saverecv(m0, opts, flags, ifp)
	struct mbuf *m0, *opts;
	int flags;
	struct ifnet *ifp;
{
	register struct ipv6 *ip;
	register struct cmsghdr *cp;
	struct mbuf *m;
	int recval, size = 0;

	if ((m = m_get(M_DONTWAIT, MT_DATA)) == NULL)  /* MT_CONTROL */
		return ((struct mbuf *) NULL);
	if (flags & (INP_RECVOPTS | INP_RECVRETOPTS)) {
		log(LOG_INFO, "udp6_saverecv: IP_RECVOPTS failed\n");
		flags &= ~(IP_RECVOPTS | IP_RECVRETOPTS);
		if ((flags & INP_CONTROLOPTS) == 0) {
			m_free(m);
			return ((struct mbuf *) NULL);
		}
	}
	if (flags & INP_RECVDSTADDR) {
		ip = mtod(m0, struct ipv6 *);
		cp = (struct cmsghdr *)(mtod(m, caddr_t) + size);
		cp->cmsg_len = sizeof(*cp) + sizeof(struct in6_addr);
		size += cp->cmsg_len;
		cp->cmsg_level = IPPROTO_IP;
		cp->cmsg_type = IP_RECVDSTADDR;
		bcopy(&ip->ip6_dst, CMSG_DATA(cp), sizeof(struct in6_addr));
	} else if (flags & INP_RECVPKTINFO) {
		ip = mtod(m0, struct ipv6 *);
		recval = ifp->if_index;
		cp = (struct cmsghdr *)(mtod(m, caddr_t) + size);
		cp->cmsg_len = sizeof(*cp) + sizeof(struct in6_pktinfo);
		size += cp->cmsg_len;
		cp->cmsg_level = IPPROTO_IPV6;
		cp->cmsg_type = IPV6_PKTINFO;
		bcopy(&ip->ip6_dst, CMSG_DATA(cp), sizeof(struct in6_addr));
		bcopy(&recval,
		      CMSG_DATA(cp) + sizeof(struct in6_addr),
		      sizeof(int));
	}
	if (flags & INP_RECVTTL) {
		ip = mtod(m0, struct ipv6 *);
		recval = ip->ip6_hlim;
		/* align size */
		size = (size + (sizeof(int) - 1)) & ~(sizeof(int) - 1);
		cp = (struct cmsghdr *)(mtod(m, caddr_t) + size);
		cp->cmsg_len = sizeof(*cp) + sizeof(int);
		size += cp->cmsg_len;
		cp->cmsg_level = IPPROTO_IP;
		cp->cmsg_type = IP_TTL;
		bcopy(&recval, CMSG_DATA(cp), sizeof(int));
	}
	m->m_len = size;
	return (m);
}

/* ARGSUSED */
void
udp6_ctlinput(cmd, sa, arg)
	int cmd;
	struct sockaddr *sa;
	void *arg;
{
	struct ctli_arg *ca = (struct ctli_arg *)arg;
	register struct ipv6 *ip;
	register struct udphdr *uh;
	extern u_char inetctlerrmap[];

	if (!PRC_IS_REDIRECT(cmd) &&
	    ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0))
		return;
	if (ca) {
		ip = ca->ctli_ip;
		uh = (struct udphdr *)(ip + 1);
		in6_pcbnotify(&udb, sa, uh->uh_dport,
			      &ip->ip6_src, uh->uh_sport,
			      cmd, udp_notify, 0);
	} else
		in6_pcbnotify(&udb, sa, 0, NULL, 0, cmd, udp_notify, 0);
}

int
udp6_output(inp, m0, addr, control)
	register struct inpcb *inp;
	register struct mbuf *m0;
	struct mbuf *addr, *control;
{
	register struct udpiphdr *ui;
	register struct udpip6hdr *ui6;
	register struct ip6ovck *ipc;
	register int len = 0;
	struct in6_addr laddr;
	int latype = 0, flowinfo = 0, hlim = 0, compat = 0, error = 0;
	register struct mbuf *m;
	struct ip_moptions *imo;
	struct mbuf *mopt;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	for (m = m0; m; m = m->m_next)
		len += m->m_len;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_UDP)
		printf("udp6_output(%p,%p,%p,%p)\n", inp, m0, addr, control);
#endif
	if (control || addr) {
		COPY_ADDR6(inp->inp_laddr6, laddr);
		latype = inp->inp_latype;
		compat = inp->inp_flags & INP_COMPATANY;
		flowinfo = inp->inp_oflowinfo;
	}
	if (control) {
		/*
		 * Changing the moptions in the pcb is kind of dumb but ok
		 * because we have the socket lock held.
		 */
		hlim = inp->inp_ttl;
		mopt = inp->inp_moptions;
#ifdef INTERFACES_ARE_SELECTABLE
		ifa = inp->inp_ifa;
#endif
		error = ip6_setcontrol(inp, control);
		if (error)
			goto restore;
	}
	if (addr) {
		if (inp->inp_fatype != IPATYPE_UNBD) {
			error = EISCONN;
			goto release;
		}
		/*
		 * Must block input while temporarily connected.
		 */
		error = in6_pcbconnect(inp, addr);
		if (error) {
			goto release;
		}
	} else {
		if (inp->inp_fatype == IPATYPE_UNBD) {
			error = ENOTCONN;
			goto release;
		}
	}

	/* IPv4 centric! */
	if (inp->inp_flags & INP_COMPATV4)
		goto version4;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_UDP)
		printf("udp6_output to %s\n", ip6_sprintf(&inp->inp_faddr6));
#endif
	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IPv6 headers.
	 */
	MGET(m, M_DONTWAIT, MT_HEADER);
	if (m == 0) {
		NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
			 NETEVENT_UDPDOWN, len, NETRES_MBUF);
		m_freem(m0);
		return (ENOBUFS);
	}

	m->m_off = MMAXOFF - sizeof (struct udpip6hdr);
	m->m_len = sizeof (struct udpip6hdr);
	m->m_next = m0;
	/*
	 * Fill in mbuf with extended UDP header
	 * and addresses and length put into network format.
	 */
	ui6 = mtod(m, struct udpip6hdr *);
	ipc = (struct ip6ovck *)ui6;
	ipc->ih6_wrd1 = ipc->ih6_wrd0 = 0;
	ipc->ih6_pr = IPPROTO_UDP;
	ipc->ih6_len = htons((u_int16_t)len + sizeof (struct udphdr));
	COPY_ADDR6(inp->inp_laddr6, ui6->ui6_src);
	COPY_ADDR6(inp->inp_faddr6, ui6->ui6_dst);
	ui6->ui6_sport = inp->inp_lport;
	ui6->ui6_dport = inp->inp_fport;
	ui6->ui6_ulen = ipc->ih6_len;

	/*
	 * Stuff checksum and output datagram.
	 */
	ui6->ui6_sum = 0;
	if ((ui6->ui6_sum = in_cksum(m, sizeof (struct udpip6hdr) + len)) == 0)
		ui6->ui6_sum = 0xffff;
	ui6->ui6_head = inp->inp_oflowinfo | IPV6_VERSION;
	ui6->ui6_len = sizeof (struct udphdr) + len;
	ui6->ui6_ulen = htons(ui6->ui6_len);
	ui6->ui6_nh = IPPROTO_UDP;
	ui6->ui6_hlim = inp->inp_ttl;
	UDPSTAT(udps_opackets);
	if (inp->inp_moptions != NULL)
		imo = mtod(inp->inp_moptions, struct ip_moptions *);
	else
		imo = NULL;
	error = ip6_output(m, inp->inp_options, &inp->inp_route,
	    inp->inp_socket->so_options & (SO_DONTROUTE | SO_BROADCAST),
	    imo, inp);
	goto sent;

version4:

	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IPv4 headers.
	 */
	MGET(m, M_DONTWAIT, MT_HEADER);
	if (m == 0) {
		NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
			 NETEVENT_UDPDOWN, len, NETRES_MBUF);
		m_freem(m0);
		return (ENOBUFS);
	}

	m->m_off = MMAXOFF - sizeof (struct udpiphdr);
	m->m_len = sizeof (struct udpiphdr);
	m->m_next = m0;
	/*
	 * Fill in mbuf with extended UDP header
	 * and addresses and length put into network format.
	 */
	ui = mtod(m, struct udpiphdr *);
	ui->ui_next = ui->ui_prev = 0;
	ui->ui_x1 = 0;
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_len = htons((u_int16_t)len + sizeof (struct udphdr));
	ui->ui_src = inp->inp_laddr;
	ui->ui_dst = inp->inp_faddr;
	ui->ui_sport = inp->inp_lport;
	ui->ui_dport = inp->inp_fport;
	ui->ui_ulen = ui->ui_len;

	/*
	 * Stuff checksum and output datagram.
	 */
	ui->ui_sum = 0;
	if ((ui->ui_sum = in_cksum(m, sizeof (struct udpiphdr) + len)) == 0)
		ui->ui_sum = 0xffff;
	((struct ip *)ui)->ip_len = sizeof (struct udpiphdr) + len;
	((struct ip *)ui)->ip_ttl = inp->inp_ttl;	/* XXX */
	((struct ip *)ui)->ip_tos = inp->inp_tos;	/* XXX */
	UDPSTAT(udps_opackets);
	error = ip_output(m, inp->inp_options, &inp->inp_route,
	    inp->inp_socket->so_options & (SO_DONTROUTE | SO_BROADCAST),
	    inp->inp_moptions, 0);

sent:
	if (addr)
		in_pcbdisconnect(inp);
restore:
	if (control || addr) {
		COPY_ADDR6(laddr, inp->inp_laddr6);
		inp->inp_latype = latype;
		inp->inp_flags |= compat;
		inp->inp_oflowinfo = flowinfo;
	}
	if (control) {
		inp->inp_ttl = hlim;
#ifdef INTERFACES_ARE_SELECTABLE
		inp->inp_ifa = ifa;
#endif
		if (inp->inp_moptions != mopt)
			m_free(inp->inp_moptions);
		inp->inp_moptions = mopt;
		m_freem(control);
	}
	return (error);

release:
	if (control)
		m_freem(control);
	m_freem(m);
	return (error);
}
#endif /* INET6 */
