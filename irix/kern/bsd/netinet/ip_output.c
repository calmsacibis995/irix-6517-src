/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)ip_output.c	7.13 (Berkeley) 6/29/88 plus MULTICAST 1.2
 *	plus parts of 7.22 (Berkeley) 7/28/90
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"
#include "sys/kmem.h"

#include "sys/debug.h"
#include "sys/mbuf.h"
#include "sys/errno.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"

#include "sys/systm.h"
#include "sys/sesmgr.h"
#include "net/if.h"
#include "net/route.h"

#include "in.h"
#include "ipfilter.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "in_var.h"
#include "ip_var.h"
#include "ip_mroute.h"

#define sintosa(sin)	((struct sockaddr *)(sin))

/*
 * External Global Variables
 */
extern struct ifnet loif;
extern struct socket *ip_mrouter;
extern int arpt_keep;
zone_t *ipmember_zone;

/*
 * Forward references.
 */
struct mbuf *ip_insertoptions(struct mbuf *, struct mbuf *, int *);
int ip_optcopy(struct ip *, struct ip *);

void ip_mloopback(struct ifnet *, struct mbuf *, struct sockaddr_in *);

/*
 * External Procedures
 */
extern struct ifnet *ifaddr_to_ifp(struct in_addr inaddr);
extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);
extern struct ifaddr *ifa_ifwithaddr(struct sockaddr *);
extern struct ifaddr *ifa_ifwithdstaddr(struct sockaddr *);
extern struct ifaddr *ifa_ifwithnet(struct sockaddr *);
extern struct ifnet  *ip_find_first_multiif(struct in_addr);

extern struct mbuf * sesmgr_ip_output (struct mbuf *,
	struct ipsec *, struct sockaddr_in *, int *);


/*
 * swIPe related declarations
 */
int swipeflag = 0;
extern char *swipe_getopol(struct in_addr src, struct in_addr dst);
extern int sw_output(struct ifnet *ifp, struct mbuf *m, char *opol);

#define	satosin(sa)	((struct sockaddr_in *)(sa))
extern struct in_ifaddr *ifatoia(struct ifaddr *);

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
ip_output(
	struct mbuf *m0,
	struct mbuf *opt,
	struct route *ro,
	int flags,
	struct mbuf *mopts,
	struct ipsec *ipsec)
{
	register struct ip *ip, *mhip;
	register struct ifnet *ifp;
	register struct mbuf *m = m0;
	register int hlen = sizeof (struct ip);
	int len, off, error = 0;
	struct route iproute;
	struct sockaddr_in *dst;
	struct sockaddr_in ddst;
	struct in_ifaddr *ib;
	char *opol;
	int	was_zero_src = 0;

	if (opt) {
#pragma mips_frequency_hint NEVER
		m = ip_insertoptions(m, opt, &len);
		hlen = len;
	}
	ip = mtod(m, struct ip *);
	/*
	 * Fill in IP header.
	 */
	if ((flags & (IP_FORWARDING|IP_RAWOUTPUT)) == 0) {
		ip->ip_v = IPVERSION;
		ip->ip_off &= IP_DF;
		ip->ip_id = htons(atomicAddIntHot(&ip_id, 1));
		ip->ip_hl = hlen >> 2;
		IPSTAT(ips_localout);
	} else
		hlen = ip->ip_hl << 2;

	NETPAR(NETSCHED, NETEVENTKN, ip->ip_id,
		 NETEVENT_IPDOWN, NETCNT_NULL, NETRES_PROTCALL);

;
try_swipe_again:
;
	if (swipeflag) {
		opol=(char *)NULL;
		if((ip->ip_p!=IPPROTO_SWIPE) && !(flags & IP_SWIPED)){
		    if (ip->ip_src.s_addr == INADDR_ANY) {
			ddst.sin_family = AF_INET;
#ifdef _HAVE_SIN_LEN
			ddst.sin_len = sizeof(ddst);
#endif			
			ddst.sin_addr= ip->ip_dst;
			if((ib = ifatoia(ifa_ifwithnet(sintosa(&ddst))))||
			   (ib = ifatoia(ifa_ifwithdstaddr(sintosa(&ddst))))){
			    ip->ip_src = IA_SIN(ib)->sin_addr;
			}else{
			    if(was_zero_src){
				printf("Couldn't figure source address!\n");
				was_zero_src = 0;
				goto sendit;
			    }else{
				was_zero_src++;
				goto skip_swipe;
			    }
			}
		    }
		    opol=swipe_getopol(ip->ip_src,ip->ip_dst);
		    if(opol){
			ip->ip_len = htons((u_short)ip->ip_len);
			ip->ip_off = htons((u_short)ip->ip_off);
			ip->ip_sum = 0;
			ip->ip_sum = in_cksum(m, hlen);
			error = sw_output(ifp, m, opol);
			return (error);
		    }
		    if(was_zero_src){
			was_zero_src = 0;
			goto sendit;
		    }
		}
	}
;
skip_swipe:
;
	/*
	 * Route packet.
	 */
	if (ro == 0) {
		ro = &iproute;
		bzero((caddr_t)ro, sizeof (*ro));
	}
	dst = (struct sockaddr_in *)&ro->ro_dst;
	/*
	 * If there is a cached route, check that it is to the same
	 * destination and is still up.  If not, free it and try again.
	 */
	ROUTE_RDLOCK();
	if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
	   dst->sin_addr.s_addr != ip->ip_dst.s_addr)) {
		rtfree_needpromote(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)0;
	}
	if (ro->ro_rt == 0) {
		dst->sin_family = AF_INET;
#ifdef _HAVE_SIN_LEN
		dst->sin_len = sizeof(*dst);
#endif
		dst->sin_addr = ip->ip_dst;
	}
	/*
	 * If routing to interface only, short circuit routing lookup.
	 */
	if (flags & IP_ROUTETOIF) {
		struct in_ifaddr *ia;
		struct ifaddr *ifa;

#pragma mips_frequency_hint NEVER
		ROUTE_UNLOCK();
		if ((ifa = ifa_ifwithdstaddr(sintosa(dst))) == 0 &&
		    ((ifa = ifa_ifwithnet(sintosa(dst))) == 0)) {
			IPSTAT(ips_noroute);
			error = ENETUNREACH;
			goto bad;
		}
		ia = (struct in_ifaddr *)(ifa->ifa_start_inifaddr);
		ifp = ia->ia_ifp;
		ip->ip_ttl = 1;
	} else {
		if (ro->ro_rt == 0) {
			rtalloc(ro);
			if ((ro->ro_rt)
			  && (ro->ro_rt->rt_gateway->sa_family == AF_LINK)
			  && (!bcmp(ro->ro_rt->rt_ifp->if_name, "lo",2)))
				ro->ro_rt->rt_rmx.rmx_expire = time + arpt_keep;
		}
		if (ro-> ro_rt == 0 || (ifp = ro->ro_rt->rt_ifp) == 0) {
			ROUTE_UNLOCK();
                        /*
                         * if the ip_dst is not multicast, end of traffic.
                         * otherwise, find a multicast interface to
                         * deliver the multicast data. bug#386607.
                         */
                        if ((ifp = ip_find_first_multiif(ip->ip_dst)) == 0) {
				IPSTAT(ips_noroute);
				if (in_localaddr(ip->ip_dst))
					error = EHOSTUNREACH;
				else
					error = ENETUNREACH;
				goto bad;
			}
		} else {
			ro->ro_rt->rt_use++;
			if (ro->ro_rt->rt_flags & RTF_GATEWAY)
				dst = (struct sockaddr_in *)ro->ro_rt->rt_gateway;
			ROUTE_UNLOCK();
		}
	}
	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		struct ip_moptions *imo;
		struct in_multi *inm;

#pragma mips_frequency_hint NEVER
		/*
		 * IP destination address is multicast.  Make sure "dst"
		 * still points to the address in "ro".  (It may have been
		 * changed to point to a gateway address, above.)
		 */
		dst = (struct sockaddr_in *)&ro->ro_dst;
		/*
		 * See if the caller provided any multicast options
		 */
		if ((flags & IP_MULTICASTOPTS) && mopts != NULL) {
			imo = mtod(mopts, struct ip_moptions *);
			ip->ip_ttl = imo->imo_multicast_ttl;
			if (imo->imo_multicast_ifp != NULL)
				ifp = imo->imo_multicast_ifp;
		}
		else {
			imo = NULL;
			ip->ip_ttl = IP_DEFAULT_MULTICAST_TTL;
		}
		/*
		 * Confirm that the outgoing interface supports multicast.
		 * (or an explicit option was set, e.g. RSVP)
		 */
		if ((ifp->if_flags & IFF_MULTICAST) == 0 &&
		    ((imo == NULL) || (imo->imo_multicast_vif == -1))) {
			IPSTAT(ips_noroute);
			error = ENETUNREACH;
			goto bad;
		}
		/*
		 * If source address not specified yet, use address
		 * of outgoing interface.
		 */
		if (ip->ip_src.s_addr == INADDR_ANY) {
			register struct in_ifaddr *ia;

			if (ia = (struct in_ifaddr *)ifp->in_ifaddr) {
				ip->ip_src = IA_SIN(ia)->sin_addr;
			} else {
				ia = (struct in_ifaddr *)hash_enum(
					&hashinfo_inaddr, ifnet_enummatch,
					HTF_INET, (caddr_t)0,
					(caddr_t)ifp, (caddr_t)0);
				if (ia) {
					ip->ip_src = IA_SIN(ia)->sin_addr;
				}
			}
		}

		/*
		 * See if we have destination multicast group
		 * on the outgoing interface.
		 */
		IN_LOOKUP_MULTI(ip->ip_dst, ifp, inm);

		if (inm != NULL && (imo == NULL || imo->imo_multicast_loop)) {
			/*
			 * If we belong to the destination multicast group
			 * on the outgoing interface, and the caller did not
			 * forbid loopback, loop back a copy.
			 */
			ip_mloopback(ifp, m, dst);
		}
		else if (ip_mrouter && (flags & IP_FORWARDING) == 0) {
			/*
			 * If we are acting as a multicast router, perform
			 * multicast forwarding as if the packet had just
			 * arrived on the interface to which we are about
			 * to send.  The multicast forwarding function
			 * recursively calls this function, using the
			 * IP_FORWARDING flag to prevent infinite recursion.
			 *
			 * Multicasts that are looped back by ip_mloopback(),
			 * above, will be forwarded by the ip_input() routine,
			 * if necessary.
			 */
			if (ip_mforward(ip, m, ifp, imo) != 0) {
				m_freem(m);
				goto done;
			}
		}
		/*
		 * Multicasts with a time-to-live of zero may be looped-
		 * back, above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip->ip_ttl == 0 || ifp == &loif) {
			m_freem(m);
			goto done;
		}
		goto sendit;
	}
	/*
	 * If source address not specified yet, use primary
	 * address of outgoing interface.
	 */
	if (ip->ip_src.s_addr == INADDR_ANY) {
		register struct in_ifaddr *ia;
		if (ro->ro_rt && !(flags & IP_ROUTETOIF)) {
			ROUTE_RDLOCK();
			if (ro->ro_rt->rt_srcifa) {
				ia = ifatoia(ro->ro_rt->rt_srcifa);
				ASSERT(ia);
				ip->ip_src = IA_SIN(ia)->sin_addr;
				ROUTE_UNLOCK();
				goto haveaddr;
			}
			ROUTE_UNLOCK();
		}
		if (ia = (struct in_ifaddr *)ifp->in_ifaddr) {
			ip->ip_src = IA_SIN(ia)->sin_addr;
		} else {
			ia = (struct in_ifaddr *)hash_enum(
				&hashinfo_inaddr, ifnet_enummatch,
				HTF_INET, (caddr_t)0,
				(caddr_t)ifp, (caddr_t)0);
			if (ia) {
				ip->ip_src = IA_SIN(ia)->sin_addr;
			}
		}
	}
haveaddr:
	/*
	 * Look for broadcast address and verify user
	 * is allowed to send such a packet.
	 */
	if (in_broadcast(dst->sin_addr, ifp)) {

#pragma mips_frequency_hint NEVER
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if ((flags & IP_ALLOWBROADCAST) == 0) {
			error = EACCES;
			goto bad;
		}
		/* don't allow broadcast messages to be fragmented */
		if (ip->ip_len > ifp->if_mtu) {
			error = EMSGSIZE;
			goto bad;
		}
		m->m_flags |= M_BCAST;
	} else {
		m->m_flags &= ~M_BCAST;
	}
sendit:

	if (swipeflag) {
		if(was_zero_src)
		    goto try_swipe_again;
	}

	/*
	 * Trix: ifp is defined at this point so we know the type of options
	 * that must be applied to the outgoing packet.
	 */

        if (sesmgr_enabled) {
#pragma mips_frequency_hint NEVER
                m0 = m = sesmgr_ip_output(m, ipsec, dst, &error);
                if (m == 0) {
                        IPSTAT(ips_odropped);
                        goto bad;
                }
                ip   = mtod(m, struct ip *);
                hlen = ip->ip_hl << 2;
        }

	/*
	 * If small enough for interface, can just send directly.
	 */
	if ((ip->ip_len <= ifp->if_mtu) || (ifp->if_flags & IFF_L2IPFRAG)) {
		NETPAR(NETFLOW, NETFLOWTKN, ip->ip_id,
			 NETEVENT_IPDOWN, ip->ip_len, NETRES_MBUF);
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);
		/*
		 * Do in-line header checksum in the normal case.
		 * Skip the 5'th short word, since it is the checksum itself.
		 */
		if (hlen == sizeof(struct ip)) {
			__uint32_t cksum;
#define ckp ((ushort*)ip)
			cksum = (ckp[0] + ckp[1] + ckp[2] + ckp[3] + ckp[4]
				+ ckp[6] + ckp[7] + ckp[8] + ckp[9]);
			cksum = (cksum & 0xffff) + (cksum >> 16);
			cksum = (cksum & 0xffff) + (cksum >> 16);
			ip->ip_sum = cksum ^ 0xffff;
#undef ckp
		} else {
#pragma mips_frequency_hint NEVER
			ip->ip_sum = 0;
			ip->ip_sum = in_cksum(m, hlen);
		}
		ASSERT(ro);

		/*
		 * Stateful filtering on output packets
		 */
		if (ipf_process) {
#pragma mips_frequency_hint NEVER
		    struct in_addr ipdst = ip->ip_dst;
		    if ((m = ((*ipf_process)(m, ifp->if_index, IPF_OUT))) == 0) {
			IPSTAT(ips_cantforward);
			goto done;
		    }
		    else {
			/* make sure route/dst is still valid */
			ip = mtod(m, struct ip *);
			if (ipdst.s_addr != ip->ip_dst.s_addr) {
			    IPSTAT(ips_odropped);
			    error = EINVAL;
			    m0 = m;
			    goto bad;
			}
		    }
		}
		IFNET_UPPERLOCK(ifp);
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst, ro->ro_rt);
		IFNET_UPPERUNLOCK(ifp);
		goto done;
	}
	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & IP_DF) {
#pragma mips_frequency_hint NEVER
		error = EMSGSIZE;
		IPSTAT(ips_cantfrag);
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
#pragma mips_frequency_hint NEVER
		error = EMSGSIZE;
		goto bad;
	}

	{
	int mhlen, firstlen = len;
	struct mbuf **mnext = &m->m_act;
	/*
	 * Choose a good fragment size for UDP.  We want the last
	 *	fragment to be a page, on the grounds that the first
	 *	part is likely to be UDP, RPC/XDR, etc.  That means we want
	 *	to make the first segment be the "odd" part.
	 *	Avoid choosing a length of 0 or a length not a multiple of 8.
	 *	Allow the length of each fragment to be more than a page
	 *	if the MTU allows.
	 *	If possible, compute a final data length that is the largest
	 *	multiple of the page size that is no greater than the MTU.
	 *	Let the first data length be the left-over.
	 * We start with len set to as much data as the MTU allows.
	 */
	if (ip->ip_len > VCL_MAX
	    && len > VCL_MAX) {
#if VCL_MAX != (VCL_MAX & -VCL_MAX) || VCL_MAX < 8
		% error: this assumes VCL_MAX is a big power of 2!
#endif
		off = len & -VCL_MAX;		/* truncate to pages of data */
		firstlen = (ip->ip_len - hlen) % off;	/* get extra */
		firstlen = (firstlen+7) & ~7;

		mhlen = firstlen+off;		/* put extra in 1st frag */
		if (mhlen <= len)		/*	if it will fit */
			firstlen = mhlen;

		len = off;
	}
	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + firstlen; off < ip->ip_len; off += len) {
		MGET(m, M_DONTWAIT, MT_HEADER);
		if (m == 0) {
#pragma mips_frequency_hint NEVER
			error = ENOBUFS;
			IPSTAT(ips_odropped);
			goto sendorfree;
		}
		m->m_off = MMAXOFF - hlen;
		mhip = mtod(m, struct ip *);
		*mhip = *ip;
		if (hlen > sizeof (struct ip)) {
#pragma mips_frequency_hint NEVER
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_hl = mhlen >> 2;
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		if (ip->ip_off & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= ip->ip_len)
			len = ip->ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
#pragma mips_frequency_hint NEVER
			m_freem(m);		/* stopleak */
			error = ENOBUFS;	/* ??? */
			IPSTAT(ips_odropped);
			goto sendorfree;
		}
		HTONS(mhip->ip_off);
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		*mnext = m;
		mnext = &m->m_act;
		IPSTAT(ips_ofragments);
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m_adj(m0, hlen + firstlen - ip->ip_len);
	ip->ip_len = htons((u_short)(hlen + firstlen));
	ip->ip_off = htons((ip->ip_off | IP_MF));
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m0, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0) {
			NETPAR(NETFLOW, NETFLOWTKN, ip->ip_id,
				 NETEVENT_IPDOWN, ip->ip_len, NETRES_NULL);
			ASSERT(ro);
			/*
			 * Stateful filtering on output packets
			 */
			if (ipf_process) {
#pragma mips_frequency_hint NEVER
			    struct in_addr ipdst = ip->ip_dst;
			    if ((m = ((*ipf_process)(m, ifp->if_index, IPF_OUT))) == 0) {
				IPSTAT(ips_cantforward);
				error = EINVAL;
				continue;
			    }
			    else {
				/* make sure route/dst is still valid */
				ip = mtod(m, struct ip *);
				if (ipdst.s_addr != ip->ip_dst.s_addr) {
				    IPSTAT(ips_odropped);
				    error = EINVAL;
				    m_freem(m);
				    continue;
				}
			    }
			}
			IFNET_UPPERLOCK(ifp);
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt);
			IFNET_UPPERUNLOCK(ifp);
		} else {
#pragma mips_frequency_hint NEVER
			NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
				 NETEVENT_IPDOWN, ip->ip_len, NETRES_NULL);
			m_freem(m);
		}
	}

	if (error == 0)
		IPSTAT(ips_fragmented);
    }
done:
	if (ro == &iproute && (flags & IP_ROUTETOIF) == 0 && ro->ro_rt) {
		rtfree_needlock(ro->ro_rt);
	}
	return (error);
bad:
#pragma mips_frequency_hint NEVER
	NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
		 NETEVENT_IPDOWN, NETCNT_NULL, NETRES_ERROR);
	m_freem(m0);
	goto done;
}

/*
 * Insert IP options into preformed packet.
 * Adjust IP destination as required for IP source routing,
 * as indicated by a non-zero in_addr at the start of the options.
 */
struct mbuf *
ip_insertoptions(register struct mbuf *m, struct mbuf *opt, int *phlen)
{
	register struct ipoption *p = mtod(opt, struct ipoption *);
	struct mbuf *n;
	register struct ip *ip = mtod(m, struct ip *);
	unsigned optlen;

	optlen = opt->m_len - sizeof(p->ipopt_dst);
	if (optlen + ip->ip_len > IP_MAXPACKET)
		return (m);		/* XXX should fail */
	if (p->ipopt_dst.s_addr)
		ip->ip_dst = p->ipopt_dst;
	if (M_HASCL(m) || MMINOFF + optlen > m->m_off) {
		MGET(n, M_DONTWAIT, MT_HEADER);
		if (n == 0)
			return (m);
		m->m_len -= sizeof(struct ip);
		m->m_off += sizeof(struct ip);
		n->m_next = m;
#ifdef sgi
		if (m->m_flags & M_CKSUMMED)
			n->m_flags |= M_CKSUMMED;
#endif
		m = n;
		m->m_off = MMAXOFF - sizeof(struct ip) - optlen;
		m->m_len = optlen + sizeof(struct ip);
		bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
	} else {
		m->m_off -= optlen;
		m->m_len += optlen;
		ovbcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
	}
	ip = mtod(m, struct ip *);
	bcopy((caddr_t)p->ipopt_list, (caddr_t)(ip + 1), (unsigned)optlen);
	*phlen = sizeof(struct ip) + optlen;
	ip->ip_len += optlen;
	return (m);
}

/*
 * Copy options from ip to jp,
 * omitting those not copied during fragmentation.
 */
ip_optcopy(struct ip *ip, struct ip *jp)
{
	register u_char *cp, *dp;
	int opt, optlen, cnt;

	cp = (u_char *)(ip + 1);
	dp = (u_char *)(jp + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP) {
			/* Preserve for IP mcast tunnel's LSRR alignment. */
			*dp++ = IPOPT_NOP;
			optlen = 1;
			continue;
		} else
			optlen = cp[IPOPT_OLEN];
		/* bogus lengths should have been caught by ip_dooptions */
		if (optlen > cnt)
			optlen = cnt;
		if (IPOPT_COPIED(opt)) {
			bcopy((caddr_t)cp, (caddr_t)dp, (unsigned)optlen);
			dp += optlen;
		}
	}
	for (optlen = dp - (u_char *)(jp+1); optlen & 0x3; optlen++)
		*dp++ = IPOPT_EOL;
	return (optlen);
}

/*
 * IP socket option processing.
 */
ip_ctloutput(
	int op,
	struct socket *so,
	int level,
	int optname,
	struct mbuf **mp)
{
	register struct inpcb *inp = sotoinpcb(so);
	register struct mbuf *m = *mp;
	int optval;
	int error = 0;

#ifdef INET6
	if ((level != IPPROTO_IP) && (level != IPPROTO_IPV6))
#else
	if (level != IPPROTO_IP)
#endif
		error = EINVAL;
	else switch (op) {

	case PRCO_SETOPT:
		switch (optname) {
		case IP_OPTIONS:
			return (ip_pcbopts(&inp->inp_options, m));

		case IP_TOS:
		case IP_TTL:
		case IP_RECVDSTADDR:
#ifdef INET6
		case IPV6_RECVPKTINFO:
		case IP_RECVTTL:
#endif
			if (m == NULL || m->m_len != sizeof(int)) {
				error = EINVAL;
			} else {
				int optval = *mtod(m, int *);
				switch (optname) {

				case IP_TOS:
					inp->inp_ip_tos = optval;
					break;

				case IP_TTL:
					inp->inp_ip_ttl = optval;
					break;

#define	OPTSET(bit) \
	if (optval) \
		inp->inp_flags |= bit; \
	else \
		inp->inp_flags &= ~bit;

				case IP_RECVOPTS:
					OPTSET(INP_RECVOPTS);
					break;

				case IP_RECVRETOPTS:
					OPTSET(INP_RECVRETOPTS);
					break;

				case IP_RECVDSTADDR:
					OPTSET(INP_RECVDSTADDR);
					break;

#ifdef INET6
				case IPV6_RECVPKTINFO:
					OPTSET(INP_RECVPKTINFO);
					break;

				case IP_RECVTTL:
					OPTSET(INP_RECVTTL);
					break;
#endif
				}
			}
			break;
#undef OPTSET

		case IP_MULTICAST_IF:
		case IP_MULTICAST_VIF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_setmoptions(optname, &inp->inp_moptions, m);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case IP_OPTIONS:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			if (inp->inp_options) {
				m->m_off = inp->inp_options->m_off;
				m->m_len = inp->inp_options->m_len;
				bcopy(mtod(inp->inp_options, caddr_t),
				    mtod(m, caddr_t), (unsigned)m->m_len);
			} else
				m->m_len = 0;
			break;
		case IP_TOS:
		case IP_TTL:
		case IP_RECVDSTADDR:
#ifdef INET6
		case IPV6_RECVPKTINFO:
		case IP_RECVTTL:
#endif
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);
			switch (optname) {

			case IP_TOS:
				optval = inp->inp_ip_tos;
				break;

			case IP_TTL:
				optval = inp->inp_ip_ttl;
				break;

#define	OPTBIT(bit)	(inp->inp_flags & bit ? 1 : 0)

			case IP_RECVOPTS:
				optval = OPTBIT(INP_RECVOPTS);
				break;

			case IP_RECVRETOPTS:
				optval = OPTBIT(INP_RECVRETOPTS);
				break;

			case IP_RECVDSTADDR:
				optval = OPTBIT(INP_RECVDSTADDR);
				break;

#ifdef INET6
			case IPV6_RECVPKTINFO:
				optval = OPTBIT(INP_RECVPKTINFO);
				break;

			case IP_RECVTTL:
				optval = OPTBIT(INP_RECVTTL);
				break;
#endif
			}
#undef OPTBIT
			*mtod(m, int *) = optval;
			break;
		case IP_MULTICAST_IF:
		case IP_MULTICAST_VIF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			*mp = 0;
			error = ip_getmoptions(optname, inp->inp_moptions, mp);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	}
	if ((op == PRCO_SETOPT) && m)
		(void)m_free(m);
	return (error);
}

/*
 * Set up IP options in pcb for insertion in output packets.
 * Store in mbuf with pointer in pcbopt, adding pseudo-option
 * with destination address if source routed.
 */
ip_pcbopts(
	struct mbuf **pcbopt,
	register struct mbuf *m)
{
	register cnt, optlen;
	register u_char *cp;
	u_char opt;

	/* turn off any old options */
	if (*pcbopt)
		(void)m_free(*pcbopt);
	*pcbopt = 0;
	if (m == (struct mbuf *)0 || m->m_len == 0) {
		/*
		 * Only turning off any previous options.
		 */
		if (m)
			(void)m_free(m);
		return (0);
	}

#ifndef	vax
	if (m->m_len % sizeof(__int32_t))
		goto bad;
#endif
	/*
	 * IP first-hop destination address will be stored before
	 * actual options; move other options back
	 * and clear it when none present.
	 */
#if MAX_IPOPTLEN >= MMAXOFF - MMINOFF
	if (m->m_off + m->m_len + sizeof(struct in_addr) > MAX_IPOPTLEN)
		goto bad;
#else
	if (m->m_off + m->m_len + sizeof(struct in_addr) > MMAXOFF)
		goto bad;
#endif
	cnt = m->m_len;
	m->m_len += sizeof(struct in_addr);
	cp = mtod(m, u_char *) + sizeof(struct in_addr);
	ovbcopy(mtod(m, caddr_t), (caddr_t)cp, (unsigned)cnt);
	bzero(mtod(m, caddr_t), sizeof(struct in_addr));

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= IPOPT_OLEN || optlen > cnt)
				goto bad;
		}
		switch (opt) {

		default:
			break;
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			/*
			 * user process specifies route as:
			 *	->A->B->C->D
			 * D must be our final destination (but we can't
			 * check that since we may not have connected yet).
			 * A is first hop destination, which doesn't appear in
			 * actual IP option, but is stored before the options.
			 */
			if (optlen < IPOPT_MINOFF - 1 + sizeof(struct in_addr))
				goto bad;
			m->m_len -= sizeof(struct in_addr);
			cnt -= sizeof(struct in_addr);
			optlen -= sizeof(struct in_addr);
			cp[IPOPT_OLEN] = optlen;
			/*
			 * Move first hop before start of options.
			 */
			bcopy((caddr_t)&cp[IPOPT_OFFSET+1], mtod(m, caddr_t),
			    sizeof(struct in_addr));
			/*
			 * Then copy rest of options back
			 * to close up the deleted entry.
			 */
			ovbcopy((caddr_t)(&cp[IPOPT_OFFSET+1] +
			    sizeof(struct in_addr)),
			    (caddr_t)&cp[IPOPT_OFFSET+1],
			    (unsigned)cnt + sizeof(struct in_addr));
			break;
		}
	}
	if (m->m_len > MAX_IPOPTLEN + sizeof(struct in_addr))
		goto bad;
	*pcbopt = m;
	return (0);

bad:
	(void)m_free(m);
	return (EINVAL);
}

/*
 * Set the IP multicast options in response to user setsockopt().
 */
ip_setmoptions(
	int optname,
	struct mbuf **mopts,
	struct mbuf *m)
{
	int error = 0;
	struct ip_moptions *imo;
	u_char loop;
	int i;
	struct in_addr addr;
	struct ip_mreq *mreq;
	struct ifnet *ifp;
	struct route ro;
	struct sockaddr_in *dst;
	struct ip_membership *ipm, **pipm;

	/* Multicast requires priv under trix */
	if (sesmgr_enabled && !cap_able(CAP_NETWORK_MGT))
		return (EPERM);

	if (*mopts == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		MGET(*mopts, M_WAIT, MT_IPMOPTS);
		if (*mopts == NULL)
			return (ENOBUFS);
		imo = mtod(*mopts, struct ip_moptions *);
		imo->imo_multicast_ifp   = NULL;
		imo->imo_multicast_vif   = -1;
		imo->imo_multicast_ttl   = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_multicast_loop  = IP_DEFAULT_MULTICAST_LOOP;
		imo->imo_membership = NULL;
	}

	imo = mtod(*mopts, struct ip_moptions *);

	switch (optname) {

	/* store an index number for the vif you wanna use in the send */
	case IP_MULTICAST_VIF:
		if (m == NULL || m->m_len != sizeof(int)) {
			error = EINVAL;
			break;
		}
		i = *(mtod(m, int *));
		if (!legal_vif_num(i) && i != -1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_vif = i;
		break;

	case IP_MULTICAST_IF:
		/*
		 * Select the interface for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != sizeof(struct in_addr)) {
			error = EINVAL;
			break;
		}
		addr = *(mtod(m, struct in_addr *));
		/*
		 * INADDR_ANY is used to remove a previous selection.
		 * When no interface is selected, a default one is
		 * chosen every time a multicast packet is sent.
		 */
		if (addr.s_addr == INADDR_ANY) {
			imo->imo_multicast_ifp = NULL;
			break;
		}
		/*
		 * The selected interface is identified by its local
		 * IP address.  Find the interface and confirm that
		 * it supports multicasting.
		 * NOTE: We can't just do the fast hash lookup assuming
		 * that the address is for any network interface since
		 * it might correspond to a PPP interface where the
		 * destination address must be check not the local address
		 * which is what is inserted in the hash table.
		 */
		ifp = ifaddr_to_ifp(addr);

		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		imo->imo_multicast_ifp = ifp;
		break;

	case IP_MULTICAST_TTL:
		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_ttl = *(mtod(m, u_char *));
		break;

	case IP_MULTICAST_LOOP:
		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.
		 */
		if (m == NULL || m->m_len != 1 ||
		   (loop = *(mtod(m, u_char *))) > 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_loop = loop;
		break;

	case IP_ADD_MEMBERSHIP:
		/*
		 * Add a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(ntohl(mreq->imr_multiaddr.s_addr))) {
			error = EINVAL;
			break;
		}
		/*
		 * If no interface address was provided, use the interface of
		 * the route to the given multicast address.
		 */
		if (mreq->imr_interface.s_addr == INADDR_ANY) {

			ROUTE_RDLOCK();
			bzero((caddr_t)&ro, sizeof(ro));
			dst = (struct sockaddr_in *)&ro.ro_dst;
			dst->sin_family = AF_INET;
			dst->sin_addr = mreq->imr_multiaddr;
			rtalloc(&ro);

			if (ro.ro_rt == NULL) {
                                if ((ifp = ip_find_first_multiif(mreq->imr_multiaddr)) == 0 ) {
					error = EADDRNOTAVAIL;
					ROUTE_UNLOCK();
					break;
				}
				ROUTE_UNLOCK();
			} else {
				ifp = ro.ro_rt->rt_ifp;
				rtfree_needpromote(ro.ro_rt);
				ROUTE_UNLOCK();
			}
		} else {
			/* first check if we have an interface match */
			ifp = ifaddr_to_ifp(mreq->imr_interface);
		}
		/*
		 * See if we found an interface, and confirm that it
		 * supports multicast.
		 */
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * See if the membership already exists
		 */
		for (ipm = imo->imo_membership; ipm; ipm = ipm->next) {
			if (ipm->ipm_membership->inm_ifp == ifp &&
			    ipm->ipm_membership->inm_addr.s_addr
						== mreq->imr_multiaddr.s_addr)
				break;
		}
		if (ipm) {
			error = EADDRINUSE;
			break;
		}
		ipm = (struct ip_membership *)
		    kmem_zone_alloc(ipmember_zone, KM_NOSLEEP);
		if (ipm == NULL) {
			error = ENOBUFS;
			break;
		}
		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
		IFNET_LOCK(ifp);
		ipm->ipm_membership = in_addmulti(mreq->imr_multiaddr, ifp);
		if (ipm->ipm_membership == NULL) {
			error = ENOBUFS;
			IFNET_UNLOCK(ifp);
			kmem_zone_free(ipmember_zone, ipm);
			break;
		}
		IFNET_UNLOCK(ifp);
		ipm->next = imo->imo_membership;
		imo->imo_membership = ipm;
		break;

	case IP_DROP_MEMBERSHIP:
		/*
		 * Drop a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(ntohl(mreq->imr_multiaddr.s_addr))) {
			error = EINVAL;
			break;
		}
		/*
		 * If an interface address was specified, get a pointer
		 * to its ifnet structure.
		 */
		if (mreq->imr_interface.s_addr == INADDR_ANY)
			ifp = NULL;
		else {
			INADDR_TO_IFP(mreq->imr_interface, ifp);
			if (ifp == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
		}
		/*
		 * Find the membership in the membership list.
		 */
		for (ipm = imo->imo_membership; ipm; ipm = ipm->next) {
			if ((ifp == NULL ||
			     ipm->ipm_membership->inm_ifp == ifp) &&
			     ipm->ipm_membership->inm_addr.s_addr
					    == mreq->imr_multiaddr.s_addr)
				break;
		}
		if (ipm == NULL) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * Give up the multicast address record to which the
		 * membership points.
		 */
		in_delmulti(ipm->ipm_membership);
		/*
		 * Remove the gap in the membership list and free.
		 */
		for (pipm = &(imo->imo_membership); *pipm;
		     pipm = &(*pipm)->next) {
			if (*pipm == ipm) {
				*pipm = ipm->next;
				kmem_zone_free(ipmember_zone, ipm);
				break;
			}
		}
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	/*
	 * If all options have default values, no need to keep the mbuf.
	 */
	if (imo->imo_multicast_ifp   == NULL &&
	    imo->imo_multicast_vif   == -1 &&
	    imo->imo_multicast_ttl   == IP_DEFAULT_MULTICAST_TTL &&
	    imo->imo_multicast_loop  == IP_DEFAULT_MULTICAST_LOOP &&
	    imo->imo_membership == NULL) {
		m_free(*mopts);
		*mopts = NULL;
	}

	return(error);
}

/*
 * Return the IP multicast options in response to user getsockopt().
 */
ip_getmoptions(
	int optname,
	struct mbuf *mopts,
	struct mbuf **m)
{
	u_char *ttl;
	u_char *loop;
	struct in_addr *addr;
	struct ip_moptions *imo;
	struct in_ifaddr *ia;

	if (*m == NULL)		/* use supplied mbuf if it exists */
		*m = m_get(M_WAIT, MT_IPMOPTS);

	imo = (mopts == NULL) ? NULL : mtod(mopts, struct ip_moptions *);

	switch (optname) {

	case IP_MULTICAST_VIF: 
		if (imo!=NULL)
		    *(mtod(*m, int *)) = imo->imo_multicast_vif;
		else
		    *(mtod(*m, int *)) = -1;
		(*m)->m_len = sizeof(int);
		return(0);

	case IP_MULTICAST_IF:
		addr = mtod(*m, struct in_addr *);
		(*m)->m_len = sizeof(struct in_addr);
		if (imo == NULL || imo->imo_multicast_ifp == NULL)
			addr->s_addr = INADDR_ANY;
		else {
			IFP_TO_IA(imo->imo_multicast_ifp, ia);
			addr->s_addr = (ia == NULL) ? INADDR_ANY
					: IA_SIN(ia)->sin_addr.s_addr;
		}
		return(0);

	case IP_MULTICAST_TTL:
		ttl = mtod(*m, u_char *);
		(*m)->m_len = 1;
		*ttl = (imo == NULL) ? IP_DEFAULT_MULTICAST_TTL
				     : imo->imo_multicast_ttl;
		return(0);

	case IP_MULTICAST_LOOP:
		loop = mtod(*m, u_char *);
		(*m)->m_len = 1;
		*loop = (imo == NULL) ? IP_DEFAULT_MULTICAST_LOOP
				      : imo->imo_multicast_loop;
		return(0);

	default:
		return(EOPNOTSUPP);
	}
}

/*
 * Discard the IP multicast options.
 */
void
ip_freemoptions(struct mbuf *mopts)
{
	struct ip_moptions *imo;
	struct ip_membership *ipm, *nipm;

	if (mopts != NULL) {
		imo = mtod(mopts, struct ip_moptions *);
		for (ipm = imo->imo_membership; ipm; ipm = nipm) {
			nipm = ipm->next;
			in_delmulti(ipm->ipm_membership);
			kmem_zone_free(ipmember_zone, ipm);
		}
		m_free(mopts);
	}
}

/*
 * Routine called from ip_output() to loop back a copy of an IP multicast
 * packet to the input queue of a specified interface.  Note that this
 * calls the output routine of the loopback "driver", but with an interface
 * pointer that might NOT be &loif -- easier than replicating that code here.
 */
void
ip_mloopback(
	struct ifnet *ifp,
	register struct mbuf *m,
	register struct sockaddr_in *dst)
{
	register struct ip *ip;
	struct mbuf *copym;
	
	copym = m_copy(m, 0, M_COPYALL);
	if (copym)
	    copym = m_pullup(copym, sizeof (struct ip));
	if (copym != NULL) {

		/*
		 * We don't bother to fragment if the IP length is greater
		 * than the interface's MTU.  Can this possibly matter?
		 */
		ip = mtod(copym, struct ip *);
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(copym, ip->ip_hl << 2);
		IFNET_UPPERLOCK(ifp);
		(void) looutput(ifp, copym, (struct sockaddr *)dst, NULL);
		IFNET_UPPERUNLOCK(ifp);
	}
}

