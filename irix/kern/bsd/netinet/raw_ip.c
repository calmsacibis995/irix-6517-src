/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)raw_ip.c	7.4 (Berkeley) 6/29/88 plus MULTICAST 3.3
 *	plus portions of 7.8 (Berkeley) 7/25/90
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/protosw.h"
#include "sys/socketvar.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include "sys/tcpipstats.h"
#include "sys/sesmgr.h"

#include "net/if.h"
#include "net/route.h"
#include "net/raw.h"
#include "net/raw_cb.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "ip_var.h"
#include "in_var.h"
#include "ip_mroute.h"
#include "in_pcb.h"

extern struct socket *ip_rsvpd;

/*
 * Raw interface to IP protocol.
 */
/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
/* ARGSUSED */
void
rip_input(
	struct mbuf *m,
	struct ifnet *ifp,
#ifdef INET6
	struct ipsec *ipsec,
	struct mbuf *unused)
#else
	struct ipsec *ipsec)
#endif
{
	register struct ip *ip = mtod(m, struct ip *);
	extern struct sockaddr_in in_zeroaddr;
        struct  sockaddr_in ripdst, ripsrc;
        struct  sockproto ripproto;

        ripdst = in_zeroaddr;
        ripsrc = in_zeroaddr;
        ripproto.sp_family = PF_INET;
	/* Trix: no policy enforced on raw ip */
	_SESMGR_SOATTR_FREE(ipsec);

	ripproto.sp_protocol = ip->ip_p;
	ripdst.sin_addr = ip->ip_dst;
	ripsrc.sin_addr = ip->ip_src;
	(void)raw_input(m, &ripproto, (struct sockaddr *)&ripsrc,
			(struct sockaddr *)&ripdst, ifp);
}

/*
 * Generate IP header and pass packet to ip_output.
 * Tack on options user may have setup with control call.
 */
/* ARGSUSED */
rip_output(struct mbuf *m0, struct socket *so, __uint32_t dst)
{
	register struct mbuf *m;
	register struct ip *ip;
	struct rawcb *rp = sotorawcb(so);
	struct mbuf *opts = rp->rcb_options;
	struct sockaddr_in *sin;
	int flags = ((so->so_options & SO_DONTROUTE)
		     | IP_ALLOWBROADCAST | IP_MULTICASTOPTS);

	ASSERT(SOCKET_ISLOCKED(so));
	/*
	 * If the user handed us a complete IP packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((rp->rcb_flags & RAW_HDRINCL) ||
	     rp->rcb_proto.sp_protocol == IPPROTO_RAW) {
		ip = mtod(m0, struct ip *);
		if (ip->ip_src.s_addr != 0) {
			struct ifnet *ifp;

			/*
			 * Verify that the source address is one of ours.
			 */
			ifp = inaddr_to_ifp(ip->ip_src);
			if (ifp == NULL) {
				m_freem(m0);
				return EADDRNOTAVAIL;
			}
		}
		if (rp->rcb_proto.sp_protocol == IPPROTO_RAW) {
			ip->ip_dst = ((struct sockaddr_in *
				       )rp->rcb_faddr)->sin_addr;
		} else {
			if (ip->ip_id == 0)
				ip->ip_id = htons(atomicAddIntHot(&ip_id, 1));
			opts = NULL;
			/* prevent ip_output from overwriting header fields */
			flags |= IP_RAWOUTPUT;
		}

		/* verify that ip_len is valid */
		if (m_length(m0) < ip->ip_len) {
			m_freem(m0);
			return EMSGSIZE;
		}

		m = m0;
	} else {
		int len = 0;

		/*
		 * Calculate data length and get an mbuf
		 * for IP header.
		 */
		for (m = m0; m; m = m->m_next)
			len += m->m_len;
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			m_freem(m0);
			return (ENOBUFS);
		}
		
		/*
		 * Fill in IP header as needed.
		 */
		m->m_off = MMAXOFF - sizeof(struct ip);
		m->m_len = sizeof(struct ip);
		m->m_next = m0;
		ip = mtod(m, struct ip *);
		ip->ip_tos = 0;
		ip->ip_off = 0;
		ip->ip_p = rp->rcb_proto.sp_protocol;
		ip->ip_len = sizeof(struct ip) + len;
		sin = (struct sockaddr_in *)rp->rcb_laddr;
		if (sin != 0) {
			if (sin->sin_family != AF_INET) {
				m_freem(m);
				return (EAFNOSUPPORT);
			}
			ip->ip_src.s_addr = sin->sin_addr.s_addr;
		} else
			ip->ip_src.s_addr = 0;
		ip->ip_dst = ((struct sockaddr_in *)rp->rcb_faddr)->sin_addr;
		ip->ip_ttl = MAXTTL;
	}

	return ip_output(m, opts, &rp->rcb_route, flags, rp->rcb_moptions,
			so->so_sesmgr_data);
}

/*
 * Raw IP socket option processing.
 */
rip_ctloutput(
	int op,
	struct socket *so,
	int level,
	int optname,
	struct mbuf **m)
{
	int error = 0;
	register struct rawcb *rp = sotorawcb(so);

	ASSERT(SOCKET_ISLOCKED(so));
	if (level != IPPROTO_IP)
		error = EINVAL;
	else switch (op) {

	case PRCO_SETOPT:
		switch (optname) {
		case IP_OPTIONS:
			return (ip_pcbopts(&rp->rcb_options, *m));

		case IP_RSVP_VIF_ON:
			error = ip_rsvp_vif_init(so, *m);
			break;

		case IP_RSVP_VIF_OFF:
			error = ip_rsvp_vif_done(so, *m);
			break;

		case IP_RSVP_ON:
			ip_rsvpd = so;
			break;

		case IP_HDRINCL:
			if (m == 0 || *m == 0 || (*m)->m_len < sizeof (int)) {
				error = EINVAL;
				break;
			}
			if (*mtod(*m, int *))
				rp->rcb_flags |= RAW_HDRINCL;
			else
				rp->rcb_flags &= ~RAW_HDRINCL;
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_VIF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_setmoptions(optname, &rp->rcb_moptions, *m);
			break;

		default:
			error = ip_mrouter_set(optname, so, *m);
			/* 
			 * If mcast routing not built in the kernel, return
			 * a more appropriate error from the lboot stub.
			 */
			if (error == ENOPKG)
				error = EINVAL;
			break;
		}
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case IP_OPTIONS:
			*m = m_get(M_WAIT, MT_SOOPTS);
			if (rp->rcb_options) {
				(*m)->m_off = rp->rcb_options->m_off;
				(*m)->m_len = rp->rcb_options->m_len;
				bcopy(mtod(rp->rcb_options, caddr_t),
				    mtod(*m, caddr_t), (unsigned)(*m)->m_len);
			} else
				(*m)->m_len = 0;
			break;

		case IP_HDRINCL:
			*m = m_get(M_WAIT, MT_SOOPTS);
			(*m)->m_len = sizeof (int);
			*mtod(*m, int *) = rp->rcb_flags & RAW_HDRINCL;
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_VIF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			*m = 0;
			error = ip_getmoptions(optname, rp->rcb_moptions, m);
			break;

		default:
			error = ip_mrouter_get(optname, so, m);
			if (error == ENOPKG)
				error = EINVAL;
			break;
		}
		break;
	}
	if (op == PRCO_SETOPT && *m)
		(void)m_free(*m);
	return (error);
}
