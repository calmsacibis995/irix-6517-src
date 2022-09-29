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
 *	@(#)if_loop.c	7.4 (Berkeley) 6/27/88 plus MULTICAST 1.1
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/errno.h"

#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/mbuf.h"
#include "sys/sbd.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/systm.h"

#include "raw.h"
#include "soioctl.h"

#include "bstring.h"
#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_types.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/in6_var.h>
#include <netinet/ip6.h>
#include <netinet/if_ndp6.h>
#endif
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef sgi
extern int lomtu;
#else
#define	LOMTU	(1024+512)
#endif

struct	ifnet loif;
#ifdef sgi
int	looutput(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst,
	struct rtentry *);
int	loioctl(struct ifnet *ifp, int cmd, caddr_t data);
#else
int	looutput(), loioctl();
#endif

#ifdef sgi

/*
 * For snooping purposes, pretend lo0 provides an encapsulation consisting
 * a socket address family (loop_family).
 */
static struct rawif	lorawif;	/* loopback raw interface */

struct loopbufhead {
	struct ifheader		ifh;	/* interface queue header */
	struct snoopheader	snoop;	/* snoop protocol header */
	__int32_t		filler;	/* align start of this structure */
	struct loopheader	loop;	/* loopback header */
};
#endif

void
loattach(void)
{
	register struct ifnet *ifp = &loif;

	ifp->if_name = "lo";
	ifp->if_mtu = lomtu;
	ifp->if_type = IFT_LOOP;
	ifp->if_flags = IFF_LOOPBACK | IFF_MULTICAST | IFF_CKSUM;
	ifp->if_ioctl = (int (*)(struct ifnet *, int , void *))loioctl;
	ifp->if_output = looutput;
#ifdef INET6
	ifp->if_ndtype = IFND6_LOOP;
#endif
	if_attach(ifp);
	rawif_attach(&lorawif, ifp,
		     (caddr_t) 0, (caddr_t) 0, 0,
		     sizeof(struct loopheader), 0, 0);
}

/* ARGSUSED */
int
looutput(struct ifnet *ifp,
	 register struct mbuf *m0,
	 struct sockaddr *dst,
	 struct rtentry *rte)
{
	struct mbuf *m;
	int data_len;
	int pad;
#ifdef INET6
	int af;
#endif

	ASSERT(IFNET_ISLOCKED(ifp) || ifp->if_flags & IFF_LOOPBACK);
	/*
	 * Handle raw output, which puts the true destination address family
	 * in a loopheader at the beginning of the user's buffer.
	 */
	if (dst->sa_family == AF_RAW) {
		if (m0->m_len < sizeof(struct loopheader)) {
			m_freem(m0);
			return EIO;
		}
		dst->sa_family = mtod(m0, struct loopheader *)->loop_family;
		M_ADJ(m0, sizeof(struct loopheader));
	}

	data_len = m_length(m0);		/* close enough */

	/*
	 * Tell everyone all checksums are good.
	 */
	m0->m_flags |= M_CKSUMMED;

	/*
	 * Make sure that the data is doubleword aligned, so compute
	 * a number of bytes to add to the header if we reuse the mbuf.
	 * This ought to be 8-mtod(...), but be sloppy and save two
	 * instructions.
	 */
	pad = (mtod(m0, __psint_t) & 0x7) + sizeof(struct loopbufhead);
	/*
	 * Prepend an ifheader containing the interface pointer and enough
	 * space for a snoopheader and loopheader.
	 */
	if (!M_HASCL(m0)
	    && m0->m_off >= MMINOFF + pad) {
		M_ADJ(m0, -pad);
		IF_INITHEADER(mtod(m0, caddr_t), ifp, pad);
	} else {
		pad = sizeof(struct loopbufhead);
		MGET(m, M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			m_freem(m0);
			return ENOBUFS;
		}
		M_INITIFHEADER(m, ifp, pad);
		m->m_next = m0;
		m0 = m;
	}
	ASSERT(!(mtod(m0, __psint_t) & 0x7));

	/*
	 * If there are any snoopers, provide SNOOP_FILTERLEN contiguous words
	 * of loopheader and data for filtering.
	 */
	IFNET_LOCK(&loif);	/* until needs to support many loopback */
	if (RAWIF_SNOOPING(&lorawif)) {
		struct loopbufhead *lbh;
		union {
			struct loopheader loop;
			__uint32_t  s[SNOOP_FILTERLEN];
			u_char	    c[1];
		} buf;
		int len, cnt;
		caddr_t hp;
		
		/*
		 * Set the loopback "MAC header" only here since it
		 * is used only when snooping.
		 */
		lbh = (struct loopbufhead *)&mtod(m0, caddr_t)[pad] - 1;
		lbh->loop.loop_family = dst->sa_family;
		lbh->loop.loop_spare = 0;

		if (m0->m_len >= (sizeof(*lbh)+pad
				  - sizeof(lbh->loop) + sizeof(buf))) {
			hp = (caddr_t) &lbh->loop;  /* use m0 as buf */
		} else {
	                hp = (caddr_t)&buf;
			buf.loop = lbh->loop;
			len = sizeof(buf) - sizeof(buf.loop);
			cnt = m_datacopy(m0, pad, len, (caddr_t)(&buf.loop+1));
			if (cnt < len)
				bzero(&buf.c[cnt], len - cnt);
		}
		snoop_input(&lorawif, 0, hp, m0, data_len);
	}
	ifp->if_opackets++;
	ifp->if_obytes += data_len;
	ifp->if_ibytes += data_len;
#ifdef INET6
	af = ((struct sockaddr_new *)dst)->sa_family;
	if (af == AF_INET || af == AF_INET6) {
		network_input(m0, af, 0);
#else
	if (dst->sa_family == AF_INET) {
		network_input(m0, dst->sa_family, 0);
#endif
	} else if (RAWIF_DRAINING(&lorawif)) {
		drain_input(&lorawif, dst->sa_family, (caddr_t) 0, m0);
	} else {
		IFNET_UNLOCK(&loif);
#ifdef DEBUG
		printf("lo%d: can't handle address family %d\n",
				ifp->if_unit, dst->sa_family);
#endif
		m_freem(m0);
		return (EAFNOSUPPORT);
	}
	ifp->if_ipackets++;
	IFNET_UNLOCK(&loif);
	return (0);
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
#ifdef sgi
int
loioctl(register struct ifnet *ifp,
	int cmd,
	caddr_t data)
#else
loioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
#endif
{
#ifdef sgi /* MULTICAST */
	register struct sockaddr *sa = (struct sockaddr *)data;
#endif /* MULTICAST */
	int error = 0;

	ASSERT(IFNET_ISLOCKED(ifp));
	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP|IFF_RUNNING;
		/*
		 * Everything else is done at a higher level.
		 */
#ifdef sgi
		/* allow MTU to be tuned without rebooting */
		ifp->if_mtu = lomtu;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		switch (sa->sa_family) {
#ifdef INET6
		case AF_INET6:
#endif
		case AF_INET:
			break;
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	default:
		error = EINVAL;
	}
	return (error);
}
