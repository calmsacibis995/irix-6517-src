/*
 * Copyright (c) 1982, 1986, 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)udp_usrreq.c	7.7 (Berkeley) 6/29/88 plus MULTICAST 1.2
 *	plus parts of 7.18 (Berkeley) 7/25/90
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/errno.h"
#include "sys/mbuf.h"
#include "sys/domain.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"
#include "sys/sysmacros.h"
#include "sys/sesmgr.h"
#include "sys/mac_label.h"

#include "sys/systm.h"
#include "sys/rtmon.h"
#include "net/if.h"
#include "net/route.h"

#include "in.h"
#include "in_systm.h"
#include "in_var.h"
#ifdef INET6
#include "in6_var.h"
#endif
#include "ip.h"
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include "in_pcb.h"
#include "ip_var.h"
#include "ip_icmp.h"
#include "icmp_var.h"
#include "udp.h"
#include "udp_var.h"
#ifdef INET6
#include "udp6_var.h"
#endif

#define UDP_MINHASHTABLESZ	64
#define UDP_MAXHASHTABLESZ	2048

extern	int	udp_hashtablesz;
extern  int	swipeflag;	/* true if swipe is on */

/* Forward references. */
void udp_detach(struct inpcb *);
static  struct mbuf *udp_saveopt(caddr_t, int, int);

/* External references. */
extern int in_pcb_hashtablesize(void);

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */
void
udp_init(void)
{
	if (udp_hashtablesz == 0) {
	      udp_hashtablesz = in_pcb_hashtablesize();
	}
	if (udp_hashtablesz < UDP_MINHASHTABLESZ)
	      udp_hashtablesz = UDP_MINHASHTABLESZ;
	if (udp_hashtablesz > UDP_MAXHASHTABLESZ)
	      udp_hashtablesz = UDP_MAXHASHTABLESZ;

	(void)in_pcbinitcb(&udb, udp_hashtablesz, INPFLAGS_CLTS, UDP_PCBSTAT);

	return;
}

extern int	udp_ttl;
struct	inpcb udb;	/* head of UDP protocol control block list */
int nfs_port = -1;	/* "special" port number for NFS packets */

extern void nfs_input(struct mbuf *m);	/* fast path hook to NFS */

#ifdef  INET6
#define UDP_INP(inp)    \
        (((inp)->inp_flags & INP_COMPATV6) ? \
		sin6tosa(&udp_in6) : ((struct sockaddr *)(&udp_in)))
#endif

#ifdef _UDP_UTRACE
#define UDPIN_UTRACE(name, mb, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(mb), \
		   UTPACK((ra), (mb) ? (mb)->m_off : 0));
#else
#define UDPIN_UTRACE(name, mb, ra)
#endif

/*ARGSUSED*/
void
#ifdef INET6
udp_input(m, ifp, ipsec, unused)
#else
udp_input(m, ifp, ipsec)
#endif
	struct mbuf *m;
	struct ifnet *ifp;
	struct ipsec *ipsec;
#ifdef INET6
	struct mbuf *unused;
#endif
{
	register struct udpiphdr *ui;
	register struct inpcb *inp, *lastinp;
	int len;
	struct ip ip;
	struct in_pcbhead *hinp;
	u_short hashval;
	struct mbuf *opts = 0;

	struct sockaddr_in udp_in;
	extern struct sockaddr_in in_zeroaddr;
#ifdef INET6
	struct  sockaddr_in6 udp_in6;
	extern struct sockaddr_in6 in6_zeroaddr;
#endif
	struct ifnet *ifns;

	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
		 NETEVENT_UDPUP, NETCNT_NULL, NETRES_PROTCALL);
	UDPSTAT(udps_ipackets);

	/*
	 * ip_intr has pulled up entire IP header with options.
	 * Strip IP options, if any; should skip this,
	 * make available to user, and use on returned packets,
	 * but we don't yet have a way to check the checksum
	 * with options still present.
	 *
	 */
	ui = mtod(m, struct udpiphdr *);
	if (((struct ip *)ui)->ip_hl > (sizeof (struct ip) >> 2)) {
#pragma mips_frequency_hint NEVER
		ip_stripoptions(m, (struct mbuf *)0);
	}
	if (m->m_len < sizeof (struct udpiphdr)) {
#pragma mips_frequency_hint NEVER
		if ((m = m_pullup(m, sizeof (struct udpiphdr))) == 0) {
			UDPSTAT(udps_hdrops);
			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
				 NETEVENT_UDPUP, m->m_len, NETRES_HEADER);
			_SESMGR_SOATTR_FREE(ipsec);
			return;
		}
		ui = mtod(m, struct udpiphdr *);
	}

	/*
	 * Make mbuf data length reflect UDP length.
	 * If not enough data to reflect UDP length, drop.
	 */
	len = ntohs((u_short)ui->ui_ulen);
	if (((struct ip *)ui)->ip_len != len) {
		if ((len > ((struct ip *)ui)->ip_len) ||
		    (len < sizeof (struct udphdr))) {
			#pragma mips_frequency_hint NEVER
			UDPSTAT(udps_badlen);
			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
				 NETEVENT_UDPUP, len, NETRES_SIZE);
			goto bad;
		}
		m_adj(m, len - ((struct ip *)ui)->ip_len);
	}
	/*
	 * If we get a zero length UDP packet, just silently drop it.
	 * It's not actually wrong, but passing it up the stack causes
	 * a multitude of problems.
	 */
	if (len == sizeof(struct udphdr)) {
		#pragma mips_frequency_hint NEVER
		goto bad;
	}
	/*
	 * Save a copy of the IP header in case we want to restore it for ICMP.
	 */
	ip = *(struct ip*)ui;

	/*
	 * Checksum extended UDP header and data.
	 * Believe the link layer if possible.
	 */
	if (ui->ui_sum) {
		ui->ui_next = ui->ui_prev = 0;
		ui->ui_x1 = 0;
		ui->ui_len = ui->ui_ulen;
		if (!(m->m_flags & M_CKSUMMED) && 
		    in_cksum(m, len + sizeof (struct ip))) {
#pragma mips_frequency_hint NEVER
			UDPSTAT(udps_badsum);
			_SESMGR_SOATTR_FREE(ipsec);
			m_freem(m);
			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
			    NETEVENT_UDPUP, len, NETRES_CKSUM);
			return;
		}
	}

	if (ui->ui_dport == nfs_port) {
		struct mbuf *tmp;
		/*
		 * Special-case fast path for NFS
		 */
		tmp = _SESMGR_NFS_SET_IPSEC(ipsec, &m);
		if (tmp != NULL) {
			UDPIN_UTRACE(UTN('nfsi','nput'), tmp, __return_address);
			nfs_input(tmp);
		}
		return;
	}
		

	udp_in = in_zeroaddr;
#ifdef INET6
	udp_in6 = in6_zeroaddr;
#endif

	if (!IN_MULTICAST(ntohl(ui->ui_dst.s_addr)) &&
	    !in_broadcast(ui->ui_dst, ifp)) {
		/*
		 * Locate pcb for datagram.
		 * We don't use a cache here, since it's a wildcard lookup.
		 * Basically, we'll always fail the match, so skip it.
		 */
		inp = in_pcblookupx(&udb,
#ifdef INET6
			&ui->ui_src, ui->ui_sport, &ui->ui_dst, ui->ui_dport,
			INPLOOKUP_WILDCARD, AF_INET);
#else
			ui->ui_src, ui->ui_sport, ui->ui_dst, ui->ui_dport,
			INPLOOKUP_WILDCARD);
#endif
		if (inp == 0) {		
			struct in_addr null_inaddr = {0};
			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
					NETEVENT_UDPUP, len, NETRES_UNREACH);
			UDPSTAT(udps_noport);
			*(struct ip *)ui = ip;
			ASSERT(ui == mtod(m, struct udpiphdr *));

			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, ifp,
				   null_inaddr, NULL, ipsec);
			return;
		}

		/*
		 * Construct sockaddr format source address.
		 * Stuff source address and datagram in user buffer.
		 */
#ifdef INET6
		if (inp->inp_flags & INP_COMPATV6) {
			udp_in6.sin6_port = ui->ui_sport;
			udp_in6.sin6_flowinfo = 0;
			udp_in6.sin6_addr.s6_addr32[0] = 0;
			udp_in6.sin6_addr.s6_addr32[1] = 0;
			udp_in6.sin6_addr.s6_addr32[2] = htonl(0xffff);
			udp_in6.sin6_addr.s6_addr32[3] = ui->ui_src.s_addr;
		} else {
			udp_in.sin_port = ui->ui_sport;
			udp_in.sin_addr = ui->ui_src;
		}
#else
		udp_in.sin_port = ui->ui_sport;
		udp_in.sin_addr = ui->ui_src;
#endif
		m->m_len -= sizeof (struct udpiphdr);
		m->m_off += sizeof (struct udpiphdr);
		if (inp->inp_socket->so_options & SO_PASSIFNAME)
		    ifns = ifp;
		else
		  ifns = 0;

		if (inp->inp_flags & INP_CONTROLOPTS) {
			struct mbuf **mp = &opts;

			if (inp->inp_flags & INP_RECVDSTADDR) {
				*mp = udp_saveopt((caddr_t) &ui->ui_dst,
				    sizeof(struct in_addr), IP_RECVDSTADDR);
				if (*mp)
					mp = &(*mp)->m_next;
			}
		}
		SOCKET_LOCK(inp->inp_socket);
#ifdef INET6
		if (sbappendaddr(&inp->inp_socket->so_rcv, 
			UDP_INP(inp), m, opts, ifns) == 0) {
#else
		if (sbappendaddr(&inp->inp_socket->so_rcv, 
			(struct sockaddr *)&udp_in, m, opts, ifns) == 0) {
#endif
			struct socket *so2 = inp->inp_socket;
			UDPSTAT(udps_fullsock);
			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
				 NETEVENT_UDPUP, len, NETRES_SBFULL);
			if (!INPCB_RELE(inp))
				SOCKET_UNLOCK(so2);
			goto bad;
		}
		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			 NETEVENT_UDPUP, len, NETRES_NULL);
		
		/*
		 * Our cheezeball hack of using MT_RIGHTS for this requires us
		 * to free it here, since sbappendaddr() makes a copy.
		 */
		if (opts) {
			m_freem(opts);
		}
		sorwakeup(inp->inp_socket, NETEVENT_UDPUP);
		{ struct socket *so2 = inp->inp_socket;
		if (!INPCB_RELE(inp))
			SOCKET_UNLOCK(so2);
		}
		_SESMGR_SOATTR_FREE(ipsec);
		return;
	} else {
		struct socket *last;
		struct inpcb *inpnxt;
		struct inpcb *head = &udb;
		int done = 0;

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
		 * fixing the interface.  Maybe 4.4BSD will remedy this?)
		 */

		/*
		 * Construct sockaddr format source address.
		 */
		udp_in.sin_port = ui->ui_sport;
		udp_in.sin_addr = ui->ui_src;
#ifdef INET6
                udp_in6.sin6_port = ui->ui_sport;
		udp_in6.sin6_flowinfo = 0;
		udp_in6.sin6_addr.s6_addr32[0] = 0;
		udp_in6.sin6_addr.s6_addr32[1] = 0;
		udp_in6.sin6_addr.s6_addr32[2] = htonl(0xffff);
		udp_in6.sin6_addr.s6_addr32[3] = ui->ui_src.s_addr;
#endif
		m->m_len -= sizeof (struct udpiphdr);
		m->m_off += sizeof (struct udpiphdr);
		/*
		 * Locate pcb(s) for datagram.
		 * (Algorithm copied from raw_intr().)
		 */
#ifdef INET6
		hashval = (*head->inp_hashfun)(head, &ui->ui_src,
			ui->ui_sport, &ui->ui_dst, ui->ui_dport, AF_INET);
#else
		hashval = (*head->inp_hashfun)(head, ui->ui_src,
			ui->ui_sport, ui->ui_dst, ui->ui_dport);
#endif

		/*
		 * Pass destination address so we can later decide if the
		 * packet is multicast or not
		 */
		if (sesmgr_enabled) {
			struct mbuf **mp = &opts;

			*mp = udp_saveopt((caddr_t) &ui->ui_dst,
					  sizeof(struct in_addr),
					  IP_RECVDSTADDR);
			if (*mp)
				mp = &(*mp)->m_next;
		}
resync:
                /*
                 * This section of code takes advantage of the fact that
                 * we know that UDP inpcb's hash into exactly one
                 * list per port number.
                 */
		last = NULL;
		hinp = &head->inp_table[hashval];
                INHHEAD_LOCK(hinp);
		for (inp = hinp->hinp_next; !done && 
			inp != (struct inpcb *)hinp; inp = inpnxt) {
		        ASSERT(INHHEAD_ISLOCKED(hinp));
		        inpnxt = inp->inp_next;
			/*
			 * Make sure sockets with no multicast options
			 * do not get multicast packets.
			 */
			if (IN_MULTICAST(ntohl(ui->ui_dst.s_addr)) &&
				(inp->inp_moptions == NULL)) {
					continue;
			}
			if (inp->inp_lport != ui->ui_dport) {
				continue;
			}
#ifdef INET6
			if ((inp->inp_flags & INP_COMPATV4) == 0)
				continue;
			if (inp->inp_latype != IPATYPE_UNBD) {
				if ((inp->inp_latype & IPATYPE_IPV4) == 0 ||
				  inp->inp_laddr.s_addr != ui->ui_dst.s_addr)
					continue;
			}
			if (inp->inp_fatype != IPATYPE_UNBD) {
				if ((inp->inp_fatype & IPATYPE_IPV4) == 0 ||
				  inp->inp_faddr.s_addr != ui->ui_src.s_addr ||
				    inp->inp_fport != ui->ui_sport)
					continue;
			}
#else
			if (inp->inp_laddr.s_addr != INADDR_ANY) {
				if (inp->inp_laddr.s_addr !=
					ui->ui_dst.s_addr)
					continue;
			}
			if (inp->inp_faddr.s_addr != INADDR_ANY) {
				if (inp->inp_faddr.s_addr !=
					ui->ui_src.s_addr ||
				    inp->inp_fport != ui->ui_sport)
					continue;
			}
#endif

			INPCB_HOLD(inp);
			if (last != NULL) {
				struct mbuf *n;

				if ((n = m_copy(m, 0, M_COPYALL)) != NULL) {
					lastinp = sotoinpcb(last);
					INHHEAD_UNLOCK(hinp);
					SOCKET_LOCK(last);
					if (last->so_options & SO_PASSIFNAME)
					    ifns = ifp;
					else
					    ifns = 0;
					if (sbappendaddr(&last->so_rcv,
#ifdef INET6
					  UDP_INP(sotoinpcb(last)), n, 
#else
					  (struct sockaddr *)&udp_in, n, 
#endif
					  opts, ifns) == 0) {
						m_freem(n);
						UDPSTAT(udps_fullsock);
						NETPAR(NETFLOW, NETDROPTKN,
						  NETPID_NULL, NETEVENT_UDPUP,
						  len, NETRES_SBFULL);
					} else {
						sorwakeup(last, NETEVENT_UDPUP);
					}
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
						goto resync;
					}
				}
			}
			last = inp->inp_socket;
			/*
			 * Don't look for additional matches if this one does
			 * not have one of the SO_REUSE{ADDR,PORT} options set.
			 * This heuristic avoids searching through all pcbs
			 * in the common case of a non-shared port.  It
			 * assumes that an application will never clear
			 * the SO_REUSE{ADDR,PORT} option after setting it.
			 */
			if ((last->so_options & 
				(SO_REUSEADDR|SO_REUSEPORT))==0) {
				done = 1;
				break;
			}
		} /* per-hash for loop */
		INHHEAD_UNLOCK(hinp);

		if (last == NULL) {
			/*
			 * No matching pcb found; discard datagram.
			 * (No need to send an ICMP Port Unreachable
			 * for a broadcast or multicast datgram.)
			 */

			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
			       NETEVENT_UDPUP, len, NETRES_UNREACH);
			UDPSTAT(udps_noportbcast);
			goto bad;
		}
		lastinp = sotoinpcb(last);
		SOCKET_LOCK(last);
		if (last->so_options & SO_PASSIFNAME)
		    ifns = ifp;
		else
		    ifns = 0;
#ifdef INET6
		if (sbappendaddr(&last->so_rcv, UDP_INP(sotoinpcb(last)),
#else
		if (sbappendaddr(&last->so_rcv, (struct sockaddr *)&udp_in,
#endif
		     m, opts, ifns) == 0)
		{
			    UDPSTAT(udps_fullsock);
			    NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
				 NETEVENT_UDPUP, len, NETRES_SBFULL);
			    if (!INPCB_RELE(lastinp))
			    	SOCKET_UNLOCK(last);
			    goto bad;
		}

		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			 NETEVENT_UDPUP, len, NETRES_NULL);
		sorwakeup(last, NETEVENT_UDPUP);
		if (!INPCB_RELE(lastinp))
			SOCKET_UNLOCK(last);
		_SESMGR_SOATTR_FREE(ipsec);
		if (opts) {
			m_freem(opts);
		}
		return;
	}
bad:
	_SESMGR_SOATTR_FREE(ipsec);
	m_freem(m);
	if (opts) {
		m_freem(opts);
	}
}

/*
 * Notify a udp user of an asynchronous error;
 * just wake up so that he can collect error status.
 */
#ifdef sgi
/*ARGSUSED*/
void
udp_notify(
	register struct inpcb *inp,
	int errno,
	void *data)
#else
udp_notify(inp, errno)
	register struct inpcb *inp;
	int errno;
#endif
{
	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket, NETEVENT_UDPUP);
	sowwakeup(inp->inp_socket, NETEVENT_UDPUP);
}

#ifdef sgi /* MTUDisc */
void
udp_ctlinput(cmd, sa, icp)
	int cmd;
	struct sockaddr *sa;
	struct icmp *icp;
#else
udp_ctlinput(cmd, sa, ip)
	int cmd;
	struct sockaddr *sa;
	register struct ip *ip;
#endif /* MTUDisc */
{
	register struct udphdr *uh;
	struct in_addr zeroin_addr;
	extern u_char inetctlerrmap[];
	struct in_addr src;
	register u_short dport = 0, sport = 0;
#ifdef sgi /* MTUDisc */
	register struct ip *ip = icp ? &icp->icmp_ip : NULL;
#endif /* MTUDisc */

	zeroin_addr.s_addr = 0;

	if (!PRC_IS_REDIRECT(cmd) && 
	    ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0))
		return;
	if (ip) {
		uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		if (((caddr_t)uh+sizeof(uh->uh_sport)+sizeof(uh->uh_dport)) <=
		    (caddr_t)ip + ip->ip_len) {
			sport = uh->uh_sport;
			dport = uh->uh_dport;
		}
		src = ip->ip_src;
	} else
		src = zeroin_addr;
#ifdef sgi
	in_pcbnotify(&udb, sa, dport, src, sport, cmd, udp_notify, 0);
#else
	in_pcbnotify(&udb, sa, dport, src, sport, cmd, udp_notify);
#endif
}

udp_output(
	register struct inpcb *inp,
	struct mbuf *m0,
	struct inaddrpair *iap,
	struct ipsec * ipsec)
{
	register struct mbuf *m;
	register struct udpiphdr *ui;
	register int len = 0;
	__uint32_t cksum;
	struct ifnet *ifp;
	int do_fastpath = 0;
	int error = 0;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP headers if needed.
	 */
	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
		 NETEVENT_UDPDOWN, NETCNT_NULL, NETRES_PROTCALL);
	for (m = m0; m; m = m->m_next)
		len += m->m_len;
	if (len > (IP_MAXPACKET - sizeof(struct udpiphdr))) {
		NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
			 NETEVENT_UDPDOWN, len, NETRES_SIZE);
		m_freem(m0);
		return (EMSGSIZE);
	}
	if (m0 && m_hasroom(m0, sizeof (struct udpiphdr))) {
		/*
		 * Just use the room in the mbuf we already have
		 */
		m = m0;
		m->m_off -= sizeof (struct udpiphdr);
		m->m_len += sizeof (struct udpiphdr);
	} else {
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
	}

	/*
	 * Fill in mbuf with extended UDP header
	 * and addresses and length put into network format.
	 */
	ui = mtod(m, struct udpiphdr *);
	ui->ui_next = ui->ui_prev = 0;
	ui->ui_x1 = 0;
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_len = htons((u_short)len + sizeof (struct udphdr));
	ui->ui_src = iap->iap_laddr;
	ui->ui_dst = iap->iap_faddr;
	ui->ui_sport = iap->iap_lport;
	ui->ui_dport = iap->iap_fport;
	ui->ui_ulen = ui->ui_len;

	/*
	 * Stuff checksum (required now) and output datagram.
	 */
	ui->ui_sum = 0;
	{
		/*
		 * Let the link layer compute the checksum if it wants
		 */
		struct rtentry *rt;

		if ((rt = inp->inp_route.ro_rt)
		    && 0 != (ifp = rt->rt_ifp)
		    && sizeof(*ui) + len <= ifp->if_mtu
		    && 0 != (rt->rt_flags & RTF_UP)
		    && (((struct sockaddr_in *)&inp->inp_route.ro_dst
			 )->sin_addr.s_addr == ui->ui_dst.s_addr)
		    && !IN_MULTICAST(ntohl(ui->ui_dst.s_addr))) {
		    	if ((0 != (ifp->if_flags & IFF_CKSUM)) ||
			    (0 != (rt->rt_flags & RTF_CKSUM))) {
				ui->ui_sum = 0xffff;
				m->m_flags |= M_CKSUMMED;
			} else {
				if ((ui->ui_sum = in_cksum(m, sizeof(*ui)
							   + len)) == 0) {
				    ui->ui_sum = 0xffff;
				}
			}
			if (inp->inp_options == 0 &&
			    inp->inp_moptions == 0 &&
			    !(inp->inp_socket->so_options & 
			      (SO_DONTROUTE|SO_BROADCAST))) {
				do_fastpath = 1;
			}
		} else {
			if ((ui->ui_sum = in_cksum(m, sizeof(*ui)+len)) == 0) {
				ui->ui_sum = 0xffff;
			}
		}
	}
	((struct ip *)ui)->ip_len = sizeof (struct udpiphdr) + len;
	((struct ip *)ui)->ip_ttl = inp->inp_ip_ttl;	/* XXX */
	((struct ip *)ui)->ip_tos = inp->inp_ip_tos;	/* XXX */
	NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
		 NETEVENT_UDPDOWN, len, NETRES_NULL);
	UDPSTAT(udps_opackets);

	if (do_fastpath && !swipeflag && !sesmgr_enabled) {
		struct ip *ip = (struct ip *)ui;
		struct sockaddr *dst;
	        (*(char *)ip) = 0x45; /* IP vers and header len */
		ip->ip_id = htons(atomicAddIntHot(&ip_id, 1));
		ip->ip_off = 0;
#define ckp ((ushort*)ip)
		cksum = (ckp[0] + ckp[1] + ckp[2] + ckp[3] + ckp[4]
			+ ckp[6] + ckp[7] + ckp[8] + ckp[9]);
		cksum = (cksum & 0xffff) + (cksum >> 16);
		cksum = (cksum & 0xffff) + (cksum >> 16);
		ip->ip_sum = cksum ^ 0xffff;
		IPSTAT(ips_localout);
		IFNET_UPPERLOCK(ifp);
		dst = (inp->inp_route.ro_rt->rt_flags & RTF_GATEWAY) ?
			inp->inp_route.ro_rt->rt_gateway : 
			&inp->inp_route.ro_dst;
		error = (*ifp->if_output)(ifp, m, dst, inp->inp_route.ro_rt);
		IFNET_UPPERUNLOCK(ifp);
		return error;
	}
	return ip_output(m, inp->inp_options, &inp->inp_route,
	    (inp->inp_socket->so_options & (SO_DONTROUTE |
					    SO_BROADCAST))
	    | IP_MULTICASTOPTS, inp->inp_moptions, ipsec);

}

extern u_int udp_sendspace, udp_recvgrams;

/*ARGSUSED*/
udp_usrreq(so, req, m, nam, rights)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *rights;
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	u_long udp_recvspace;
	struct in_addr oladdr;
	struct in_addr *from = 0;
	struct ipsec *ipsec = so->so_sesmgr_data;

	ASSERT(SOCKET_ISLOCKED(so));
	if (req == PRU_CONTROL) {
#ifdef INET6
		if (sotopf(so) == AF_INET)
			return (in_control(so, (__psint_t)m, (caddr_t)nam,
			  (struct ifnet *)rights));
		else
			return (in6_control(so, (__psint_t)m, (caddr_t)nam,
			  (struct ifnet *)rights));
#else
		return (in_control(so, (__psint_t)m, (caddr_t)nam,
			(struct ifnet *)rights));
#endif
	}
	if ((req != PRU_SEND) && rights && rights->m_len) {
		error = EINVAL;
		goto release;
	}
	if (inp == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	/*
	 * Note: need to block udp_input while changing
	 * the udp pcb queue and/or pcb addresses.
	 */
	switch (req) {

	case PRU_ATTACH:
		if (inp != NULL) {
			error = EINVAL;
			break;
		}
		error = in_pcballoc(so, &udb);
		if (error)
			break;
		udp_recvspace = udp_recvgrams
#ifdef INET6
			* (udp_sendspace+sizeof(struct sockaddr_in6));
#else
			* (udp_sendspace+sizeof(struct sockaddr_in));
#endif
		soreserve(so, udp_sendspace, udp_recvspace);
#ifdef INET6
		if (sotopf(so) == AF_INET)
			((struct inpcb *) so->so_pcb)->inp_flags = INP_COMPATV4;
		else
			((struct inpcb *)so->so_pcb)->inp_flags = INP_COMPATANY;
#endif
		((struct inpcb *) so->so_pcb)->inp_ip_ttl = udp_ttl;
		break;

	case PRU_DETACH:
		udp_detach(inp);
		inp = 0;
		break;

	case PRU_BIND:
#ifdef INET6
		if (sotopf(so) == AF_INET)
			error = in_pcbbind(inp, nam);
		else
			error = in6_pcbbind(inp, nam);
#else
		error = in_pcbbind(inp, nam);
#endif
		break;

	case PRU_LISTEN:
		error = EOPNOTSUPP;
		break;

	case PRU_CONNECT:
#ifdef INET6
		if (inp->inp_fatype != IPATYPE_UNBD) {
#else
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
#endif
			error = EISCONN;
			break;
		}
		NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
			 NETEVENT_UDPDOWN, NETCNT_NULL, NETRES_CONN);
#ifdef INET6
		if (sotopf(so) == AF_INET)
			error = in_pcbconnect(inp, nam);
		else
			error = in6_pcbconnect(inp, nam);
#else
		error = in_pcbconnect(inp, nam);
#endif
		NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
			 NETEVENT_UDPDOWN, NETCNT_NULL, NETRES_CONNDONE);
		if (error == 0)
			soisconnected(so);
		break;

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	case PRU_ACCEPT:
		error = EOPNOTSUPP;
		break;

	case PRU_DISCONNECT:
#ifdef INET6
		if (inp->inp_fatype == IPATYPE_UNBD) {
#else
		if (inp->inp_faddr.s_addr == INADDR_ANY) {
#endif
			error = ENOTCONN;
			break;
		}
		in_pcbdisconnect(inp);
#ifdef INET6
		CLR_ADDR6(inp->inp_laddr6);
		inp->inp_latype = IPATYPE_UNBD;
		inp->inp_flags |= INP_COMPATANY;
		inp->inp_iflowinfo = 0;
#else
		inp->inp_laddr.s_addr = INADDR_ANY;
#endif
		so->so_state &= ~SS_ISCONNECTED;		/* XXX */
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SEND: {
		struct inaddrpair ipr;
		struct sockaddr_in *sin;

#ifdef INET6
		/*
		 * The sendmsg() hack below is not needed for v6 because
		 * the equivalent functionality exists as part of the
		 * IPV6_PKTINFO option.
		 */
		if (sotopf(so) == AF_INET6)
			return (udp6_output(inp, m, nam, rights));
#endif
		if (nam) {
			/*
			 * Hack to allow sendmsg() call to specify source
			 * address to use.  This avoids having to open
			 * thousands of sockets when using large numbers of
			 * aliases.
			 */
			oladdr = inp->inp_laddr;
			if (rights) {
				struct cmsghdr *cmsg;
				struct sockaddr_in sin;
#if _MIPS_SIM == _ABI64
				extern int irix5_to_cmsghdr(struct mbuf *);

				irix5_to_cmsghdr(rights);
#endif	/* _ABI64 */
				cmsg = mtod(rights, struct cmsghdr *);
				bzero((caddr_t)&sin, sizeof(sin));
#define _LEN	(sizeof(*cmsg) + sizeof(*from))
				if (rights->m_len < _LEN) {
					error = EINVAL;
					break;
				}
				if (cmsg->cmsg_len != _LEN ||
				    cmsg->cmsg_level != IPPROTO_IP ||
				    cmsg->cmsg_type != IP_SENDSRCADDR) {
					error = EINVAL;
					break;
				}
				from = (struct in_addr *)CMSG_DATA(cmsg);
				sin.sin_family = AF_INET;
				sin.sin_addr = *from;

				/* must be one of our addresses */
				if (!ifa_ifwithaddr((struct sockaddr *)&sin)) {
					error = EADDRNOTAVAIL;
					break;
				}
			}
#undef _LEN
#ifdef INET6
			if (inp->inp_fatype != IPATYPE_UNBD) {
#else
			if (inp->inp_faddr.s_addr != INADDR_ANY) {
#endif
				error = EISCONN;
				break;
			}
			sin = mtod(nam, struct sockaddr_in *);
			if (nam->m_len != sizeof(*sin)) {
				error = EINVAL;
				break;
			}
			if (from) {
				if (inp->inp_lport == 0) {
					error = in_pcbbind(inp,
						(struct mbuf *)0);
					if (error) {
						break;
					}
				}
				inp->inp_laddr = *from;
			}
			if (error = in_pcbsetaddrx(inp, sin, inp->inp_laddr,
			    &ipr)) {
				inp->inp_laddr = oladdr;
				break;
			}
			error = udp_output(inp, m, &ipr, ipsec);
			inp->inp_laddr = oladdr;
		} else {
#ifdef INET6
			if (inp->inp_fatype == IPATYPE_UNBD) {
#else
			if (inp->inp_faddr.s_addr == INADDR_ANY) {
#endif
				error = ENOTCONN;
				break;
			}
			error = udp_output(inp, m, &inp->inp_iap, ipsec);
		}
		m = NULL;
		}
		break;

	case PRU_ABORT:
		soisdisconnected(so);
		udp_detach(inp);
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	case PRU_SENDOOB:
	case PRU_FASTTIMO:
	case PRU_SLOWTIMO:
	case PRU_PROTORCV:
	case PRU_PROTOSEND:
		error =  EOPNOTSUPP;
		break;

	case PRU_RCVD:
	case PRU_RCVOOB:
		return (EOPNOTSUPP);	/* do not free mbuf's */

	case PRU_SOCKLABEL:
                if (!sesmgr_enabled)
			error = EOPNOTSUPP;
                else
                        sesmgr_set_label(inp->inp_socket,(mac_label *)nam);
		break;

	default:
		panic("udp_usrreq");
	}
release:
	if (m != NULL)
		m_freem(m);
	return (error);
}

#ifdef sgi
void
#endif
udp_detach(struct inpcb *inp)
{
	in_pcbdetach(inp);
}

/*
 * Create a "control" mbuf containing the specified data
 * with the specified type for presentation with a datagram.
 */
struct mbuf *
udp_saveopt(caddr_t p, int size, int type)
{
	register struct cmsghdr *cp;
	struct mbuf *m;

	if ((m = m_get(M_DONTWAIT, MT_RIGHTS)) == NULL)	/* XXX control */
		return ((struct mbuf *) NULL);
	cp = (struct cmsghdr *) mtod(m, struct cmsghdr *);
	bcopy(p, CMSG_DATA(cp), size);
	size += sizeof(*cp);
	m->m_len = size;
	cp->cmsg_len = size;
	cp->cmsg_level = IPPROTO_IP;
	cp->cmsg_type = type;
	return (m);
}
