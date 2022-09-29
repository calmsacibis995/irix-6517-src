/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)if_ether.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IF_NDP6_H_
#define _NETINET_IF_NDP6_H_

/*
 * Definitions for if_ndtype
 */

#define IFND6_TYPE	0xff000000	/* type of the interface */
#define IFND6_FLAGS	0x00ff0000	/* flags */
#define IFND6_LLOFF	0x0000ff00	/* link-local address offset */
#define IFND6_L2OFF	0x000000ff	/* link-layer address offset */

#define IFND6_UNKNOWN	0x00000000	/* unknown ND type */
#define IFND6_LOOP	0x01000000	/* loopback */
#define IFND6_IEEE	0x02000000	/* IEEE LAN */
#define IFND6_SIT	0x03000000	/* SIT IPv6 over IPv4 */
#define IFND6_TUG	0x04000000	/* TUG IPv6 over IPv6 */
#define IFND6_TUN	0x05000000	/* TUN interface/device */
#define IFND6_PPP	0x06000000	/* PPP serial */
#define IFND6_ATM	0x07000000	/* ATM */
#define IFND6_LLINK	0xff000000	/* link-local pseudo interface */

#define IFND6_IS_LLINK(ifp) \
	(((ifp)->if_ndtype & IFND6_TYPE) == IFND6_LLINK)

#define IFND6_INLL	0x00010000	/* participate to llink */
#define IFND6_ADDRES	0x00020000	/* use address resolution */
#define IFND6_MTU	0x00040000	/* fancy MTU values */

#define IFND6_LLSET	0x00100000	/* link-local address is set */

#define GETLLADDR(ifp) ((ifp)->if_6llocal)  /* link local addr */

#define GETL2ADDR(ifp) ((ifp)->if_6l2addr)  /* link layer addr */

#ifdef _KERNEL
/*
 * IPv6 Neighbor Discovery Protocol.
 *
 * See RFC xxxx for protocol description.
 */

struct llinfo_ndp6 {
	struct	rtentry *ln_rt;
	struct	mbuf *ln_hold;		/* last packet until resolved/timeout */
	long	ln_asked;		/* last time we QUERIED for this addr */
	short	ln_state;		/* state */
	short	ln_flags;		/* flags (is_router) */
/* deletion time in seconds, 0 == locked */
#define ln_timer ln_rt->rt_rmx.rmx_expire
};
#define LLNDP6_PHASEDOUT	0	/* held very old routes */
#define LLNDP6_INCOMPLETE	1	/* no info yet */
#define LLNDP6_PROBING		2	/* usable, NUD probes it */
#define LLNDP6_REACHABLE	3	/* valid */
#define LLNDP6_BUILTIN		4	/* don't fall into probing */

#define M_NOPROBE		M_BCAST	/* don't trigger NUD probes */

/*
 * Neighbor Discovery specific interface flag.
 */
#define IFA_BOOTING	RTF_XRESOLVE

#ifdef INET6

void	ndsol6_output __P((struct ifnet *,
		struct mbuf *, struct in6_addr *, struct in6_addr *));
void	ndadv6_output __P((struct ifnet *,
		struct in6_addr *, struct in6_addr *, int));
void	redirect6_output __P((struct mbuf *, struct rtentry *));
int	ndsol6_input __P((struct mbuf *, struct ifnet *));
int	ndadv6_input __P((struct mbuf *, struct ifnet *));
int	rtsol6_input __P((struct mbuf *, struct ifnet *));
int	rtadv6_input __P((struct mbuf *, struct ifnet *));
int	redirect6_input __P((struct mbuf *, struct ifnet *));

int	ndp6_resolve __P((struct ifnet *,
	   struct rtentry *, struct mbuf *, struct sockaddr *, u_char *));
void	ndp6_ifinit __P((struct ifnet *, struct ifaddr *));
void	ndp6_rtrequest __P((int, struct rtentry *, struct sockaddr *));
void	ndp6_rtlost __P((struct rtentry *, int));
struct	llinfo_ndp6 *ndplookup __P((struct in6_addr *, int));
#endif
#endif

#endif
