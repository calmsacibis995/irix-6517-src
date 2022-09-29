/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)raw_ip.c	8.2 (Berkeley) 1/4/94
 */

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/in6_var.h>
#include <netinet/ip6_icmp.h>
#include <netinet/if_ether.h>
#include <netinet/if_ndp6.h>
#include <netinet/ip6_mroute.h>
#include <sys/tcpipstats.h>


static struct inpcb ripcb;

#define RIP6_HASHTABLESZ	1
void
rip6_init(void)
{
	(void)in_pcbinitcb(&ripcb, RIP6_HASHTABLESZ,
	  INPFLAGS_CLTS, RP6_PCBSTAT);
}

/*
 * Raw interface to IP protocol.
 */

static	struct mbuf *raw6_saverecv __P((struct mbuf *, int, struct ifnet *));

/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
/* ARGSUSED */
void
rip6_input(struct mbuf *m, struct ifnet *ifp,
  struct ipsec *ipsec, struct mbuf *opts)
{
	register struct ipv6 *ip = mtod(m, struct ipv6 *);
	register struct inpcb *inp, *lastinp, *inpnxt;
	struct socket *last = 0;
	struct mbuf *copts = 0;
	int ipsec_failed = 0;
	struct sockaddr_in6 rip6src = { sizeof(rip6src), AF_INET6 };
	struct in_pcbhead *hinp;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_RAW)
		printf("rip6_input(%p,%p) src %s dst %s len %d nh %d\n",
		       m, opts,
		       ip6_sprintf(&ip->ip6_src),
		       ip6_sprintf(&ip->ip6_dst),
		       ip->ip6_len,
		       ip->ip6_nh);
#endif
	COPY_ADDR6(ip->ip6_src, rip6src.sin6_addr);
	rip6src.sin6_flowinfo = ip->ip6_head & IPV6_FLOWINFO_PRIFLOW;

/*
 * Since we always look at all of the pcbs, we only have one bucket.
 * If it turns out that there are a lot of raw ip6 sockets, we could hash
 * with ip6_nh (instead of a port number which we don't have) and then make
 * a special check for sockets with inp_proto == 0 (the wildcarders).
 */
resync:
	last = NULL;
	hinp = &ripcb.inp_table[0];
	INHHEAD_LOCK(&ripcb.inp_table[0]);
	for (inp = hinp->hinp_next; inp != (struct inpcb *)hinp; inp = inpnxt) {
		inpnxt = inp->inp_next;
		if (inp->inp_proto && inp->inp_proto != ip->ip6_nh)
			continue;
		/*
		 * Right now only IPv6 is using ripcb so this next check really
		 * isn't needed.
		 */
		if ((inp->inp_flags & INP_COMPATV6) == 0)
			continue;

		if (inp->inp_latype != IPATYPE_UNBD &&
		    ((inp->inp_latype & IPATYPE_IPV6) == 0 ||
		     !SAME_ADDR6(inp->inp_laddr6, ip->ip6_dst)))
			continue;
		if (inp->inp_fatype != IPATYPE_UNBD &&
		    ((inp->inp_fatype & IPATYPE_IPV6) == 0 ||
		     !SAME_ADDR6(inp->inp_faddr6, ip->ip6_src)))
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
		if ((inp->inp_proto == IPPROTO_ICMPV6) &&
		    icmp6_filter(inp, ip))
			continue;
		INPCB_HOLD(inp);
		if (last) {
			struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);
			lastinp = sotoinpcb(last);

#define RAW_CONTROLOPTS	\
	(INP_RECVDSTADDR|INP_RECVPKTINFO|INP_RECVTTL)
			if (lastinp->inp_flags & RAW_CONTROLOPTS)
				copts = raw6_saverecv(m,
				  lastinp->inp_flags, ifp);
			else
				copts = (struct mbuf *)0;
			if (n) {
				INHHEAD_UNLOCK(hinp);
				SOCKET_LOCK(last);
				if (sbappendaddr(&last->so_rcv,
				    sin6tosa(&rip6src), n, copts, 
				    (last->so_options & SO_PASSIFNAME) ?
				    ifp : 0) == 0) {
					/* should notify about lost packet */
					m_freem(n);
					if (copts)
						m_freem(copts);
				} else
					sorwakeup(last, NETEVENT_SOUP);
				copts = 0;
/*
 * XXX This locking code was copied from the v4 udp_input().  It has a
 * race (in v4 and hence now in v6).  The race is that after the
 * head lock is dropped the list may change such that we see a pcb twice.
 * entering this case should be rare so we do not have an urgent need for a
 * fix.  in_pcbnotify() and udp_input have the same race.
 */
				if (!INPCB_RELE(lastinp))
					SOCKET_UNLOCK(last);
				INHHEAD_LOCK(hinp);
				if (inp->inp_next != inpnxt) {
					struct socket *so = inp->inp_socket;

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
	}
	INHHEAD_UNLOCK(hinp);
	if (last) {
		lastinp = sotoinpcb(last);
		if (lastinp->inp_flags & RAW_CONTROLOPTS)
			copts = raw6_saverecv(m, lastinp->inp_flags, ifp);
		else
			copts = (struct mbuf *)0;
		SOCKET_LOCK(last);
		if (sbappendaddr(&last->so_rcv,
			 sin6tosa(&rip6src), m, copts,
			 (last->so_options & SO_PASSIFNAME) ? ifp : 0) == 0) {
			m_freem(m);
			if (copts)
				m_freem(copts);
		} else
			sorwakeup(last, NETEVENT_SOUP);
		if (!INPCB_RELE(lastinp))
			SOCKET_UNLOCK(last);
	} else if (ipsec_failed || ip->ip6_nh != IPPROTO_ICMPV6) {
		IP6STAT(ip6s_noproto);
		IP6STAT_DEC(ip6s_delivered, 1);
		icmp6_errparam(m, opts,
		  (__psunsigned_t)&((struct ipv6 *)0)->ip6_nh,
		  ifp, ICMP6_PARAMPROB_NH);
	} else
		m_freem(m);

	/* TODO: options */
	if (opts)
		m_freem(opts);
}

/*
 * Save in a "control" mbuf received informations.
 */
static struct mbuf *
raw6_saverecv(m0, flags, ifp)
	struct mbuf *m0;
	int flags;
	struct ifnet *ifp;
{
	register struct ipv6 *ip;
	register struct cmsghdr *cp;
	struct mbuf *m;
	int recval, size = 0;

	if ((m = m_get(M_DONTWAIT, MT_DATA)) == NULL)  /* was MT_CONTROL */
		return ((struct mbuf *) NULL);
	cp = mtod(m, struct cmsghdr *);
	if (flags & INP_RECVDSTADDR) {
		ip = mtod(m0, struct ipv6 *);
		cp->cmsg_len = sizeof(*cp) + sizeof(struct in6_addr);
		size += _ALIGN(cp->cmsg_len);
		cp->cmsg_level = IPPROTO_IP;
		cp->cmsg_type = IP_RECVDSTADDR;
		bcopy(&ip->ip6_dst, CMSG_DATA(cp), sizeof(struct in6_addr));
		cp = (struct cmsghdr *)(mtod(m, caddr_t) + size);
	} else if (flags & INP_RECVPKTINFO) {
		ip = mtod(m0, struct ipv6 *);
		recval = ifp->if_index;
		cp->cmsg_len = sizeof(*cp) + sizeof(struct in6_pktinfo);
		size += _ALIGN(cp->cmsg_len);
		cp->cmsg_level = IPPROTO_IPV6;
		cp->cmsg_type = IPV6_PKTINFO;
		bcopy(&ip->ip6_dst, CMSG_DATA(cp), sizeof(struct in6_addr));
		bcopy(&recval,
		      CMSG_DATA(cp) + sizeof(struct in6_addr),
		      sizeof(int));
		cp = (struct cmsghdr *)(mtod(m, caddr_t) + size);
	}
	if (flags & INP_RECVTTL) {
		ip = mtod(m0, struct ipv6 *);
		recval = ip->ip6_hlim;
		cp->cmsg_len = sizeof(*cp) + sizeof(int);
		size += _ALIGN(cp->cmsg_len);
		cp->cmsg_level = IPPROTO_IP;
		cp->cmsg_type = IP_TTL;
		bcopy(&recval, CMSG_DATA(cp), sizeof(int));
	}
	m->m_len = size;
	return (m);
}

/*
 * Generate IPv6 header and pass packet to ip6_output.
 * Tack on options user may have setup with control call.
 */
static int
rip6_output(register struct mbuf *m, struct socket *so, struct in6_addr *dst)
{
	register struct ipv6 *ip;
	register struct inpcb *inp = sotoinpcb(so);
	struct mbuf *opts;
	int flags = so->so_options & SO_DONTROUTE;
	struct mbuf *n;
	int len = 0;
	struct mbuf *o;
	int oplen;
	struct ip_moptions *imo;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_RAW)
		printf("rip6_output(%p,%p,%s)\n", m, so, ip6_sprintf(dst));
#endif
	for (n = m; n; n = n->m_next)
		len += n->m_len;
	/*
	 * If the user handed us a complete IPv6 packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((inp->inp_flags & INP_HDRINCL) == 0) {
		opts = inp->inp_options;
		for (o = opts, oplen = 0; o; o = o->m_next)
			oplen += OPT6_LEN(o);
		if ((len + oplen) > IP_MAXPACKET + sizeof(struct ipv6)) {
			m_freem(m);
			return(EMSGSIZE);
		}
		MGET(n, M_WAIT, MT_HEADER);
		n->m_off = MMAXOFF - sizeof (struct ipv6);
		n->m_len = sizeof (struct ipv6);
		n->m_next = m;
		m = n;
		ip = mtod(m, struct ipv6 *);
		COPY_ADDR6(inp->inp_laddr6, ip->ip6_src);
		COPY_ADDR6(*dst, ip->ip6_dst);
		if (inp->inp_proto == IPPROTO_ICMPV6) {
			register struct ip6ovck *ipc;
			register struct icmpv6 *icp;

			if ((m = m_pullup(m, sizeof(*ip) + sizeof(*icp))) == 0)
				return(ENOBUFS);
			ip = mtod(m, struct ipv6 *);
			icp = (struct icmpv6 *)(ip + 1);
			if (icp->icmp6_cksum == 0) {
				ipc = (struct ip6ovck *)ip;
				ipc->ih6_wrd0 = 0;
				ipc->ih6_wrd1 = 0;
				ipc->ih6_len = htons(len);
				ipc->ih6_pr = IPPROTO_ICMPV6;
				icp->icmp6_cksum = in_cksum(m, len);
			}
		}
		ip->ip6_head = inp->inp_oflowinfo | IPV6_VERSION;
		ip->ip6_len = len;
		ip->ip6_nh = inp->inp_proto;
		ip->ip6_hlim = inp->inp_ttl;
	} else {
		ip = mtod(m, struct ipv6 *);
		/* don't allow both user specified and setsockopt options,
		   and don't allow packet length sizes that will crash */
		opts = NULL;
		NTOHS(ip->ip6_len);
		if (ip->ip6_len + sizeof(struct ipv6) > len) {
			m_freem(m);
			return(EMSGSIZE);
		}
		/* prevent ip6_output from overwriting header fields */
		flags |= IP_RAWOUTPUT;
		IP6STAT(ip6s_rawout);
	}
	if (inp->inp_flags & INP_NOPROBE)
		m->m_flags |= M_NOPROBE;
	if (inp->inp_moptions != NULL)
		imo = mtod(inp->inp_moptions, struct ip_moptions *);
	else
		imo = NULL;
	return (ip6_output(m, opts, &inp->inp_route, flags, imo, inp));
}

/*
 * Raw IPv6 socket option processing.
 */
int
rip6_ctloutput(op, so, level, optname, m)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **m;
{
	register struct inpcb *inp = sotoinpcb(so);
	register int error;
	register int flag;

	ASSERT(SOCKET_ISLOCKED(so));
	if ((level != IPPROTO_IP) && (level != IPPROTO_IPV6) &&
	    ((level == IPPROTO_ICMPV6) && (optname != ICMP6_FILTER))) {
		if (op == PRCO_SETOPT && *m != 0)
			(void)m_free(*m);
		return (EINVAL);
	}

	switch (optname) {

	case IP_HDRINCL:
	case IPV6_NOPROBE:
		error = 0;
		if (optname == IP_HDRINCL)
			flag = INP_HDRINCL;
		else
			flag = INP_NOPROBE;
		if (op == PRCO_SETOPT) {
			if (m == 0 || *m == 0 || (*m)->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(*m, int *))
				inp->inp_flags |= flag;
			else
				inp->inp_flags &= ~flag;
			if (*m)
				(void)m_free(*m);
		} else {
			*m = m_get(M_WAIT, MT_SOOPTS);
			(*m)->m_len = sizeof (int);
			*mtod(*m, int *) = inp->inp_flags & flag;
		}
		return (error);

	/* case IP6_NAT: */

	case MFC6_INIT:
	case MFC6_DONE:
	case MFC6_ADD_MFC:
	case MFC6_DEL_MFC:
	case MFC6_ADD_DSIF:
	case MFC6_DEL_DSIF:
	case MFC6_VERSION:
		if (op == PRCO_SETOPT) {
			error = ip6_mrouter_set(optname, so, *m);
			if (*m)
				(void)m_free(*m);
		} else if (op == PRCO_GETOPT) {
			error = ip6_mrouter_get(optname, so, m);
		} else
			error = EINVAL;
		return (error);

	case ICMP6_FILTER:
		if (op == PRCO_SETOPT) {
			if (inp->inp_proto != IPPROTO_ICMPV6)
				error = EOPNOTSUPP;
			else
				error = icmp6_filter_set(inp, *m);
			if (*m)
				(void)m_free(*m);
		} else if (op == PRCO_GETOPT) {
			error = icmp6_filter_get(inp, m);
		} else
			error = EINVAL;
		return (error);
	}
	return (ip6_ctloutput(op, so, level, optname, m));
}

/*ARGSUSED*/
int
rip6_usrreq(so, req, m, nam, control)
	register struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	register int error = 0;
	register struct inpcb *inp = sotoinpcb(so);
	struct inpcb *tinp;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_RAW)
		printf("rip6_usrreq(%p,%d,%p,%p,%p)\n",
		       so, req, m, nam, control);
#endif
	ASSERT(SOCKET_ISLOCKED(so));
	if (req == PRU_CONTROL)
		return (in6_control(so, (u_long)m, (caddr_t)nam,
			(struct ifnet *)control));
	if (req != PRU_SEND && control && control->m_len) {
		m_freem(control);
		if (m)
			m_freem(m);
		return (EOPNOTSUPP);
	}

	switch (req) {

	case PRU_ATTACH:
		if (inp)
			panic("rip6_attach");
		if ((so->so_state & SS_PRIV) == 0) {
			error = EACCES;
			break;
		}
		if ((error = soreserve(so, RAWSNDQ, RAWRCVQ)) ||
		    (error = in_pcballoc(so, &ripcb)))
			break;
		inp = (struct inpcb *)so->so_pcb;
		inp->inp_flags = INP_COMPATV6;
		inp->inp_proto = (long)nam;
		/* default is to not filter */
		ICMP6_FILTER_SETPASSALL(&inp->inp_filter);
		inp->inp_ttl = MAXTTL;
		/*
		 * in_pcballoc doesn't place the pcb into the hash table
		 * so do it now.  Normally we would wait until bind but all
		 * raw pcbs go to hash chain 0 so we might as well do it now.
		 * If it turns out that there are a lot of raw ip6
		 * sockets, we could hash with ip6_nh (instead of a port
		 * number which we don't have) and then make a special check
		 * for sockets with inp_proto == 0 (the wildcarders).
		 */
		inp->inp_hashval = 0;
		inp->inp_hhead = &ripcb.inp_table[inp->inp_hashval];
		INHHEAD_LOCK(&ripcb.inp_table[inp->inp_hashval]);
		tinp = _TBLELEMTAIL(inp);
		insque(inp, tinp);
		INHHEAD_UNLOCK(inp->inp_hhead);
		break;

	case PRU_DISCONNECT:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		/* FALLTHROUGH */
	case PRU_ABORT:
		soisdisconnected(so);
		/* FALLTHROUGH */
	case PRU_DETACH:
		if (inp == 0)
			panic("rip6_detach");
		if (so == ip6_mrouter)
			ip6_mrouter_done();
		in_pcbdetach(inp);
		break;

	case PRU_BIND:
	    {
		struct sockaddr_in6 *addr = mtod(nam, struct sockaddr_in6 *);

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (ifnet == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (addr->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT;
			break;
		}
		if (IS_ANYSOCKADDR(addr)) {
			inp->inp_latype = IPATYPE_UNBD;
		} else if (IS_IPV4SOCKADDR(addr)) {
			error = EADDRNOTAVAIL;
			break;
		} else {
			if (ifa_ifwithaddr(sin6tosa(addr)) == 0) {
				error = EADDRNOTAVAIL;
				break;
			} else
				inp->inp_latype = IPATYPE_IPV6;
		}
		COPY_ADDR6(addr->sin6_addr, inp->inp_laddr6);
		break;
	    }
	case PRU_CONNECT:
	    {
		struct sockaddr_in6 *addr = mtod(nam, struct sockaddr_in6 *);

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (ifnet == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (addr->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT;
			break;
		}
		COPY_ADDR6(addr->sin6_addr, inp->inp_faddr6);
		if (IS_ANYSOCKADDR(addr))
			inp->inp_fatype = IPATYPE_UNBD;
		else if (IS_IPV4SOCKADDR(addr)) {
			error = EADDRNOTAVAIL;
			break;
		} else {
			inp->inp_fatype = IPATYPE_IPV6;
		}
		soisconnected(so);
		break;
	    }

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Mark the connection as being incapable of further input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	/*
	 * Ship a packet out.  The appropriate raw output
	 * routine handles any massaging necessary.
	 */
	case PRU_SEND:
	    {
		register struct in6_addr *dst;
#if (MULTI_HOMED > 0)
		struct ifaddr *ifa = 0;
#endif
		struct in6_addr laddr;
		struct mbuf *mopts = 0;
		int latype = 0, flowinfo = 0, hlim = 0, compat = 0;

		if (so->so_state & SS_ISCONNECTED) {
			if (nam) {
				error = EISCONN;
				break;
			}
			dst = &inp->inp_faddr6;
		} else {
			if (nam == NULL) {
				error = ENOTCONN;
				break;
			}
			if (nam->m_len != sizeof(struct sockaddr_in6)) {
				error = EINVAL;
				break;
			}
			if (_FAKE_SA_FAMILY(mtod(nam, struct sockaddr *)) !=
			  AF_INET6) {
				error = EAFNOSUPPORT;
				break;
			}
			dst = &mtod(nam, struct sockaddr_in6 *)->sin6_addr;
			if ((ifnet == 0) || IS_IPV4ADDR6(*dst)) {
				error = EADDRNOTAVAIL;
				break;
			}
		}
		if (control) {
			COPY_ADDR6(inp->inp_laddr6, laddr);
			latype = inp->inp_latype;
			compat = inp->inp_flags & INP_COMPATANY;
			flowinfo = inp->inp_oflowinfo;
			hlim = inp->inp_ttl;
			mopts = inp->inp_moptions;
#if (MULTI_HOMED > 0)
			ifa = inp->inp_ifa;
			if (ifa)
				ifa->ifa_refcnt++;
#endif
			error = ip6_setcontrol(inp, control);
		}
		if (error == 0)
			error = rip6_output(m, so, dst);
		if (control) {
			COPY_ADDR6(laddr, inp->inp_laddr6);
			inp->inp_latype = latype;
			inp->inp_flags |= compat;
			inp->inp_oflowinfo = flowinfo;
			inp->inp_ttl = hlim;
#if (MULTI_HOMED > 0)
			if (inp->inp_ifa)
				ifafree(inp->inp_ifa);
			inp->inp_ifa = ifa;
#endif
			if (inp->inp_moptions != mopts)
				m_free(inp->inp_moptions);
			inp->inp_moptions = mopts;
		}
		m = NULL;		
		break;
	    }

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	/*
	 * Not supported.
	 */
	case PRU_RCVOOB:
	case PRU_RCVD:
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;

	default:
		panic("rip6_usrreq");
	}
	if (control != NULL)
		m_freem(control);
	if (m != NULL)
		m_freem(m);
	return (error);
}

/* ARGSUSED */
void
rip6_ctlinput(cmd, sa, arg)
	int cmd;
	struct sockaddr *sa;
	void *arg;
{
	register struct inpcb *inp, *inpnxt;
	struct sockaddr_in6 *dst6 = (struct sockaddr_in6 *)sa;
	extern void in_rtchange(struct inpcb *, int , void *);
	struct in_pcbhead *hinp;
	int havelock;

	if (!PRC_IS_REDIRECT(cmd))
		return;
	if ((sa == NULL) || (_FAKE_SA_FAMILY(sa) != AF_INET6))
		return;
	if (IS_ANYSOCKADDR(dst6) || IS_IPV4SOCKADDR(dst6))
		return;

/*
 * Since we always look at all of the pcbs, we only have one bucket.
 * If it turns out that there are a lot of raw ip6 sockets, we could hash
 * with ip6_nh (instead of a port number which we don't have) and then make
 * a special check for sockets with inp_proto == 0 (the wildcarders).
 */
resync:
	hinp = &ripcb.inp_table[0];
	havelock = 1;
	INHHEAD_LOCK(&ripcb.inp_table[0]);
	inp = hinp->hinp_next;
	if (inp != (struct inpcb *)hinp) {
		INPCB_HOLD(inp);
		for (; inp != (struct inpcb *)hinp && (inp->inp_hhead == hinp);
		  inp = inpnxt) {
			inpnxt = inp->inp_next;
			INPCB_HOLD(inpnxt);
			if ((inp->inp_flags & INP_COMPATV6) == 0 ||
			    inp->inp_socket == 0 ||
			    inp->inp_route.ro_rt == 0 ||
			    !SAME_ADDR6(dst6->sin6_addr, 
			    satosin6(&inp->inp_route.ro_dst)->sin6_addr)) {
				struct socket *so2 = inp->inp_socket;

				INHHEAD_UNLOCK(hinp);
				SOCKET_LOCK(so2);
				if (!INPCB_RELE(inp))
					SOCKET_UNLOCK(so2);
				INHHEAD_LOCK(hinp);
				if (inpnxt->inp_next == 0) {
					struct socket *so2 = inpnxt->inp_socket;
					INHHEAD_UNLOCK(hinp);
					SOCKET_LOCK(so2);
					if (!INPCB_RELE(inpnxt)) {
						SOCKET_UNLOCK(so2);
					}
					goto resync;
				}
				continue;
			}

			INHHEAD_UNLOCK(hinp);
			SOCKET_LOCK(inp->inp_socket);
			in_rtchange(inp, 0, 0);
			if (!INPCB_RELE(inp))
				SOCKET_UNLOCK(inp->inp_socket);
			INHHEAD_LOCK(hinp);
			if (inpnxt->inp_next == 0) {
				struct socket *so2 = inpnxt->inp_socket;
				INHHEAD_UNLOCK(hinp);
				SOCKET_LOCK(so2);
				if (!INPCB_RELE(inpnxt)) {
					SOCKET_UNLOCK(so2);
				}
				goto resync;
			}
		}
		if (inp != (struct inpcb *)hinp) {
			struct socket *so2 = inp->inp_socket;
			INHHEAD_UNLOCK(hinp);
			havelock = 0;
			SOCKET_LOCK(so2);
			if (!INPCB_RELE(inp)) {
				SOCKET_UNLOCK(so2);
			}
		}
	}
	if (havelock) {
		INHHEAD_UNLOCK(hinp);
	}
}
