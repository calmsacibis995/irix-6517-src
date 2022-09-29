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
 *	@(#)if_ether.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Ethernet address resolution protocol.
 * TODO:
 *	add "inuse/lock" bit (or ref. count) along with valid bit
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/time.h"
#include "sys/ioctl.h"

#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/debug.h"
#include "sys/sema.h"
#include "sys/cmn_err.h"

#include "net/soioctl.h"
#include "net/if.h"
#include "net/if_dl.h"
#include "net/route.h"
#include "misc/ether.h"

#include "in.h"
#include "in_systm.h"
#include "in_var.h"
#include "ip.h"
#include "if_ether.h"
#ifdef INET6
#include <netinet/if_ether6.h>
#endif

/* SRCROUTE */
#include "net/if_types.h"
#include "sys/kmem.h"

#ifdef DEBUG
extern int kdbx_on;
#endif

#include "sys/ddi.h"
#include "sys/atomic_ops.h"

#define SIN(s) ((struct sockaddr_in *)s)
#define SDL(s) ((struct sockaddr_dl *)s)

/* timer values */
#ifdef notyet
int	arpt_prune = (5*60*1);	/* walk list every 5 minutes */
#endif
int	arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
int	arpt_down = 20;		/* once declared down, don't send for 20 secs */
#define	rt_expire rt_rmx.rmx_expire
extern int rtrequest_promote(void);
extern void rtrequest_demote(int);

/*
 * The ether_copy() macro casts elements of these arrays to short, so
 * we must enforce proper alignment to avoid read address error exceptions.
 */
union ebcast_align ebcast_align = {{ 0xff,0xff,0xff,0xff,0xff,0xff }};

/* MULTICAST */
ushort ether_ipmulticast_min[3] = { 0x0100, 0x5e00, 0x0000 };
ushort ether_ipmulticast_max[3] = { 0x0100, 0x5e7f, 0xffff };

extern	struct ifnet loif;
extern int useloopback;     /* use loopback interface for local traffic */

struct	llinfo_arp llinfo_arp = {&llinfo_arp, &llinfo_arp};
int	arp_intimer;
int	arp_maxtries = 5;
int	arpinit_done = 0;
int llinfo_arp_count = 0; /* for arp command */

/*
 * External Procedures
 */
extern char *inet_ntoa(struct in_addr);
extern struct ifaddr *ifa_ifwithnet(struct sockaddr *);

/*
 * Source Route table:
 * This table is dynamically allocated, since it is used only by token
 * ring drivers.
 */
struct	sritab	*sritab;
int sritab_size = 0; /* for arp command */

#define	SRITAB_HASH(a)	((u_long)((((u_short*)a)[1]<<16)+((u_short*)a)[2])%_ARPTAB_NB)

#define	SRITAB_LOOK(st,addr,arpflg) { \
	register u_int n; \
	st = &sritab[SRITAB_HASH(addr) * _ARPTAB_BSIZ]; \
	for (n = 0 ; n < _ARPTAB_BSIZ ; n++,st++) { \
		if ((arpflg && (st->st_flags & SRIF_NONARP)) || \
		    (!arpflg && (st->st_flags & SRIF_ARP))) \
			continue; \
		if (!bcmp(st->st_addr,addr,sizeof(st->st_addr))) \
			break; \
	} \
	if (n >= _ARPTAB_BSIZ) \
		st = 0; \
}

extern int arpreq_alias;

static void in_arpinput(struct arpcom*, struct mbuf*, u_short*);
extern struct sritab* srifind(char*, u_char);
static int sriset(char*, u_short*, u_char, u_char);
static struct sritab* srinew(char*, u_char);
static void sritfree(char*, u_char);
extern void sritdelete(struct sritab*);

/* timer values */
#define ARPT_AGE    15		 /* aging timer 15 sec */
#define SCREAM_TIME (15/ARPT_AGE)	/* can't be smaller than ARPT_AGE */

static struct arpcom *scream_arpcom = 0;
static struct in_addr scream_iaddr;
static u_char scream_sha[6];
static int scream_timer;

static struct sockaddr_inarp sarp0 = { sizeof(sarp0), AF_INET };
#ifdef _HAVE_SA_LEN
static struct sockaddr_in sin0 = { sizeof(sin0), AF_INET };
#else
static struct sockaddr_in sin0 = { AF_INET };
#endif


/*
 * Free an arp entry.
 */
static void
arptfree(struct llinfo_arp *la)
{
	register struct rtentry *rt = la->la_rt;

	ASSERT(ROUTE_ISLOCKED());
	ASSERT(rt);
	if (rt == 0)
		panic("arptfree: no route in llinfo");
	rtrequest(RTM_DELETE, rt_key(rt), 0, rt_mask(rt), 0, 0);
}

/*
 * Timeout routine.  Age arp_tab entries periodically.
 */
/* ARGSUSED */
static void
arptimer(__psint_t all)
{
	int i;
	register struct llinfo_arp *la;
	struct arpcom *ac;
	struct in_addr inaddr;

	/*
	 * lower to imp, but no lower to protect the values in the loop.
	 * Since splimp() refuses to go lower, pass thru level 1.
	 */
	int s1 = spl0();

	ROUTE_WRLOCK();

	/* if duplicated IP address detected, defend aggressively */
	if (ac = scream_arpcom) {
		inaddr = scream_iaddr;
		ROUTE_UNLOCK();
		IFNET_LOCK(&ac->ac_if);
		ARP_WHOHAS(ac, &inaddr);
		IFNET_UNLOCK(&ac->ac_if);
		ROUTE_WRLOCK();
		if (++scream_timer >= SCREAM_TIME*2)
			scream_arpcom = 0;
	}

	dtimeout( arptimer, (caddr_t)((all+1) % (60/ARPT_AGE)), ARPT_AGE*HZ,
		plbase, cpuid());
	for (la = llinfo_arp.la_next; la != &llinfo_arp; ) {
		register struct rtentry *rt = la->la_rt;
		la = la->la_next;
		ASSERT(rt);
		ASSERT(la->la_prev);
		ASSERT(la->la_prev != &llinfo_arp);
		/*
		 * Ignore permanent entries.
		 */
		if (rt->rt_expire == 0)
			continue;
		/*
		 * Clear a temporary entry when its timer expires or
		 * when its interface is turned off.
		 */
		if (rt->rt_expire <= time
		    || rt->rt_ifp == 0 || iff_dead(rt->rt_ifp->if_flags)
		    || rt->rt_ifa == 0 || !(rt->rt_ifa->ifa_flags & IFA_ROUTE))
			arptfree(la->la_prev);
	}
	if (sritab) {
		for (i = 0; i < _ARPTAB_SIZE; i++)
			++(sritab[i]).st_timer;
	}
	ROUTE_UNLOCK();
	splx(s1);
}


/*
 * used by IP load sharing driver.
 */
void
expire_arp(__uint32_t ipaddr)
{
	register struct llinfo_arp *la;
	register struct rtentry *rt;

	ROUTE_WRLOCK();
	for (la = llinfo_arp.la_next; la != &llinfo_arp; ) {
		rt = la->la_rt;
		la = la->la_next;
		ASSERT(rt);
		ASSERT(la->la_prev);
		ASSERT(la->la_prev != &llinfo_arp);
		if (SIN(rt_key(rt))->sin_addr.s_addr == ipaddr)
			arptfree(la->la_prev);
	}
	ROUTE_UNLOCK();
}


int
send_arp(struct arpcom *ac, struct in_addr *sip, struct in_addr *tip,
	u_char *enaddr, short op, u_char *ether_dest, struct mbuf *m)
{
	register struct ether_header *eh;
	register struct ether_arp *ea;
	struct sockaddr sa;
#ifdef DEBUG
	extern int kdbx_on;
#endif

	ASSERT(IFNET_ISLOCKED(&ac->ac_if) || kdbx_on);
	m->m_len = sizeof(*ea);
	m->m_off = MMAXOFF - m->m_len;
	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	bzero((caddr_t)ea, sizeof (*ea));
	ether_copy(ether_dest, (caddr_t)eh->ether_dhost);
	eh->ether_type = ETHERTYPE_ARP;		/* if_output will swap */
	if (ac->ac_if.if_type == IFT_ISO88025)
		ea->arp_hrd = htons(ARPHRD_802);
	else
		ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(op);
	ether_copy((caddr_t)enaddr, (caddr_t)ea->arp_sha);
	IP_copy((caddr_t)sip, (caddr_t)ea->arp_spa);
	IP_copy((caddr_t)tip, (caddr_t)ea->arp_tpa);
	sa.sa_family = AF_UNSPEC;
#ifdef _HAVE_SA_LEN
	sa.sa_len = sizeof(sa);
#endif /* _HAVE_SA_LEN */
	return((*ac->ac_if.if_output)(&ac->ac_if, m, &sa, NULL));
}

static int
arp_alias_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
        struct in_ifaddr *ia;
        struct ifnet *ifp;
        int match = 0;
        __uint32_t *tnetp;

	tnetp  = (__uint32_t *)arg1;

        ia = (struct in_ifaddr *)h;
        ifp = (struct ifnet *)key;

        if (ia->ia_ifp == ifp &&
                ia->ia_addrflags & IADDR_ALIAS)
	{
		if (ia->ia_net == *tnetp)
                	match = 1;
	}
        return match;
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- arp header source ip address
 *	- arp header target ip address
 *	- arp header source ethernet address
 */
void
arprequest(struct arpcom *ac, struct in_addr *sip, struct in_addr *tip,
	u_char *enaddr)
{
	register struct mbuf *m;
#ifdef DEBUG
	extern int kdbx_on;
#endif
        struct in_ifaddr * alias_ia;
        struct  ifaddr *alias_ifaddr;
        struct sockaddr *alias_ifa_addr;
        __uint32_t i;
        __uint32_t tnet = 0;
	int gratuitous_arp = 0;
	



	ASSERT(IFNET_ISLOCKED(&ac->ac_if) || kdbx_on);
	if ((m = m_get(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	
			
        if (arpreq_alias)
        {
		if (sip && tip && sip->s_addr == tip->s_addr)
			gratuitous_arp = 1;	
		
		if (!gratuitous_arp) {	
			if (tip) {
			i = ntohl(tip->s_addr);
	        	if (IN_CLASSA(i))
        	       		tnet = i & IN_CLASSA_NET;
        		else if (IN_CLASSB(i))
                		tnet = i & IN_CLASSB_NET;
        		else if (IN_CLASSC(i))
                		tnet = i & IN_CLASSC_NET;
        		else if (IN_CLASSD(i))
                		tnet = i & IN_CLASSD_NET;
			else
				tnet = 0;
			}

                	alias_ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
                        arp_alias_match,  HTF_INET,
                        (caddr_t) &ac->ac_if,
                        (caddr_t) &tnet,
                        (caddr_t) 0);
                	if (alias_ia)
                	{
                        	alias_ifaddr = &(alias_ia->ia_ifa);
                        	alias_ifa_addr = alias_ifaddr->ifa_addr;
                		sip = (struct in_addr*)(&((struct sockaddr_in*)alias_ifa_addr)->sin_addr.s_addr);
                	}
        	}
	}
	(void)send_arp(ac, sip, tip, enaddr, ARPOP_REQUEST,
		(u_char *)etherbroadcastaddr, m);
}

/*
 * backward compatibility interface for those drivers not modified to
 * use the new implementation of arp
 */
int
arpwhohas(struct arpcom *ac, struct in_addr *addr)
{
	/*
	 * If the arp route request function has not been set, set it
	 * now.  Doing this here has the advantage that if_rtrequest
	 * will be set by the arp request sent out when the driver
	 * comes up.  This will occur before any routes have been
	 * allocated.
	 */
	if (ac->ac_if.if_rtrequest != arp_rtrequest) {
		ac->ac_if.if_rtrequest = arp_rtrequest;
	}
	ARP_WHOHAS(ac, addr);
	return(0);
}

/* ARGSUSED */
static int
in_arp_match(struct hashbucket *h, caddr_t key1, caddr_t key2, caddr_t arg)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct in_addr *addr1 = (struct in_addr *)key1;
	struct in_addr *addr2 = (struct in_addr *)key2;
	struct ifnet *ifp = (struct ifnet *)arg;
	int match = 0;

	if (h->flags & HTF_INET) {
		if ((ia->ia_ifp == ifp) &&
		 ((addr1->s_addr == IA_SIN(ia)->sin_addr.s_addr) ||
		 (addr2 && (addr2->s_addr == IA_SIN(ia)->sin_addr.s_addr)))) {
			match = 1;
		}
	}
	return match;
}

/*
 * Parallel to link_rtrequest.
 */
/* ARGSUSED */
void
arp_rtrequest(int req, struct rtentry *rt, struct sockaddr *sa)
{
	register struct sockaddr *gate = rt->rt_gateway;
	register struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
#ifdef _HAVE_SA_LEN
	static struct sockaddr_dl null_sdl = {
				(u_char)sizeof(null_sdl),
				(u_char)AF_LINK,
				(u_short)0,
				(u_char)0,
				(u_char)0,
				(u_char)0,
				(u_char)0};
#else /* _HAVE_SA_LEN */
	static struct sockaddr_dl null_sdl = {
				(u_short)AF_LINK,
				(u_short)0,
				(u_char)0,
				(u_char)0,
				(u_char)0,
				(u_char)0};
#endif /* _HAVE_SA_LEN */

	ASSERT(ROUTE_ISWRLOCKED());
	if (!arpinit_done) {
		extern int network_intr_pri;
		arpinit_done = 1;
		timeout_pri(arptimer, (caddr_t)0, HZ, network_intr_pri+1);
	}
	if (rt->rt_flags & RTF_GATEWAY)
		return;
	switch (req) {

	case RTM_ADD:
		/*
		 * XXX: If this is a manually added route to interface
		 * such as older version of routed or gated might provide,
		 * restore cloning bit.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0 &&
		    SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
			rt->rt_flags |= RTF_CLONING;
		if (rt->rt_flags & RTF_CLONING) {
			/*
			 * Case 1: This route should come from a route to iface
			 */
			rt_setgate(rt, rt_key(rt),
					(struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = rt->rt_ifp->if_type;
			SDL(gate)->sdl_index = rt->rt_ifp->if_index;
			rt->rt_expire = time;
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE) {
			IFNET_LOCK(rt->rt_ifp);
			arprequest((struct arpcom *)rt->rt_ifp,
				   &SIN(rt_key(rt))->sin_addr,
				   &SIN(rt_key(rt))->sin_addr,
				   (u_char *)LLADDR(SDL(gate)));
			IFNET_UNLOCK(rt->rt_ifp);
		}
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
#ifdef _HAVE_SA_LEN
		    gate->sa_len < sizeof(null_sdl)
#else
		    _FAKE_SA_LEN_DST(gate) < sizeof(null_sdl)
#endif
		    ) {
#ifdef DEBUG
			printf("arp_rtrequest: bad gateway value, family %d, "
			       "sa_len is %d (need %d)\n",
			       gate->sa_family,
#ifdef _HAVE_SA_LEN
			       gate->sa_len,
#else
			       _FAKE_SA_LEN_DST(gate),
#endif
			       sizeof(null_sdl));
#endif /* DEBUG */
			break;
		}
		SDL(gate)->sdl_type = rt->rt_ifp->if_type;
		SDL(gate)->sdl_index = rt->rt_ifp->if_index;
		if (la != 0) {
			break; /* This happens on a route change */
		}
		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		R_Malloc(la, struct llinfo_arp *, sizeof(*la));
		rt->rt_llinfo = (caddr_t)la;
		if (la == 0) {
#ifdef DEBUG
			cmn_err(CE_DEBUG, "arp_rtrequest: malloc failed\n");
#endif /* DEBUG */
			break;
		}
		Bzero(la, sizeof(*la));
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		insque(la, &llinfo_arp);
		ASSERT(la->la_next);
		ASSERT(la->la_prev);

		/*
		 * This keeps the multicast addresses from showing up
		 * in `arp -a' listings as unresolved.  It's not actually
		 * functional.  Then the same for broadcast.
		 */
		if (IN_MULTICAST(ntohl(SIN(rt_key(rt))->sin_addr.s_addr))) {
			if (rt->rt_ifp->if_type == IFT_ISO88025) {
			   TOKEN_MAP_IP_MULTICAST(&SIN(rt_key(rt))->sin_addr,
					       LLADDR(SDL(gate)));
			} else {
			   ETHER_MAP_IP_MULTICAST(&SIN(rt_key(rt))->sin_addr,
					       LLADDR(SDL(gate)));
			}
			SDL(gate)->sdl_alen = 6;
			rt->rt_expire = 0;
		}
		if (in_broadcast(SIN(rt_key(rt))->sin_addr, rt->rt_ifp)) {
			bcopy(etherbroadcastaddr, LLADDR(SDL(gate)), 6);
			SDL(gate)->sdl_alen = 6;
			rt->rt_expire = 0;
		}
		if (hash_lookup(&hashinfo_inaddr, in_arp_match,
			(caddr_t)&(SIN(rt_key(rt))->sin_addr), (caddr_t)0,
			(caddr_t)&((struct arpcom *)rt->rt_ifp)->ac_if)) {
		    /*
		     * This test used to be
		     *	if (loif.if_flags & IFF_UP)
		     * It allowed local traffic to be forced
		     * through the hardware by configuring the loopback down.
		     * However, it causes problems during network configuration
		     * for boards that can't receive packets they send.
		     * It is now necessary to clear "useloopback" and remove
		     * the route to force traffic out to the hardware.
		     */
			rt->rt_expire = 0;
			Bcopy(((struct arpcom *)rt->rt_ifp)->ac_enaddr,
				LLADDR(SDL(gate)), SDL(gate)->sdl_alen = 6);
			if (useloopback)
				rt->rt_ifp = &loif;

		}
		break;

	case RTM_DELETE:
		if (la == 0)
			break;
		remque(la);
		if (la->la_rt &&
			(SDL(la->la_rt->rt_gateway)->sdl_family == AF_LINK) &&
			SDL(la->la_rt->rt_gateway)->sdl_alen) {
				sritfree(LLADDR(SDL(la->la_rt->rt_gateway)), 1);
		}
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		if (la->la_hold) {
			struct mbuf *om = 0;
			om = swap_ptr((void **)&la->la_hold, (void *)0);
			m_freem(om);
		}
		Free((caddr_t)la);
	}
}

/*
 * Lookup or enter a new address in arptab.
 *
 * There are three forms of ARP entries in the kernel routing table,
 * the published, proxy-only host-route form with the SARP_PROXY bit set,
 * the non-published host route, and the published network route.
 * In theory, there should only be one form in the table at a time.
 *
 * To find an entry suitable for setting MAC headers in non-ARP traffic,
 * the kernel asks for a route to the target IP address.  That will
 * match either the non-published host route or the published network
 * route.
 *
 * To answer an ARP query received from the wire, the kernel asks for a route
 * to the target IP address with the SARP_PROXY bit appended.  That routing
 * table probe will match either the published network-route form (which does
 * not have the SARP_PROXY bit) or the proxy-only host route with SARP_PROXY
 * bit.
 */
static struct llinfo_arp *
arplookup(u_long addr,			/* target IP address */
	  int create,			/* 1=add route if necessary */
	  int proxy)			/* 0 or SARP_PROXY */
{
	register struct rtentry *rt;
	struct sockaddr_inarp sarp;

	ASSERT(ROUTE_ISRDLOCKED());
	sarp = sarp0;
	sarp.sarp_addr.s_addr = addr;
	if (proxy) {
		sarp.sarp_other = SARP_PROXY;
	}
	rt = rtalloc1((struct sockaddr *)&sarp, create);
	if (rt == 0)
		return (0);
	RT_RELE(rt);
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_gateway->sa_family != AF_LINK) {
#ifdef DEBUG
		if (create) {
			cmn_err(CE_DEBUG,
				"arplookup failed for address %s, rt 0x%p\n",
				inet_ntoa(sarp.sarp_addr), rt);
		}
#endif /* DEBUG */
		return (0);
	}
	return ((struct llinfo_arp *)rt->rt_llinfo);
}

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
char *
ether_sprintf(u_char *ap)
{
#	define NUM_BUFS 4
	static struct {
		char b[18];	/* 11:22:33:44:55:66\0 */
	} etherbufs[NUM_BUFS];
	static int bufno;
	static char digits[] = "0123456789abcdef";
	int i;
	char *cp, *cp0;

	cp0 = etherbufs[bufno].b;
	bufno = (bufno+1) % NUM_BUFS;
	cp = cp0;
	for (i = 0; i < 6; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (cp0);
}

/*
 * Resolve an IP address into an ethernet address.  If success,
 * desten is filled in.  If there is no entry in arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desten has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 */
int
arpresolve(struct arpcom *ac, struct rtentry *rt0, struct mbuf *m,
	struct in_addr *ipdst, u_char *desten, int *errp)
{
	register struct llinfo_arp *la;
	struct sockaddr_dl *sdl;
	struct rtentry *rt, *tmprt;
	struct mbuf *om;
	int promoted;
	int needsend = 0;

	*errp = 0;
	/*
	 * fast arpresolve; no lock
	 *
	 * Works if 'rt0' is an LLINFO route, with a gateway, and is up.
	 * The address family of the gateway must be AF_LINK.  The MAC address
	 * length must be non-zero to show the entry has been resolved.
	 *
	 * Assumptions:
	 *	Once rt_gateway is set it will never change; this relies on
	 *	all MAC addresses for the same network being the same length.
	 *      If rt_setgate() ever frees the memory, we could have a major
	 *      problem on our hands.
	 */
#define _ARP_FLAGS	(RTF_UP|RTF_LLINFO|RTF_HOST)
	if (rt0 && ((rt0->rt_flags & _ARP_FLAGS) == _ARP_FLAGS) &&
		!(rt0->rt_gwroute) && (rt0->rt_gateway &&
		((sdl = SDL(rt0->rt_gateway)) && (sdl->sdl_family == AF_LINK)&&
		sdl->sdl_alen))) {
		ether_copy(LLADDR(sdl), desten);
		return 1;
	}
#undef _ARP_FLAGS

	/* do slow way */
	if (IN_MULTICAST(ntohl(ipdst->s_addr))) {
		if (ac->ac_if.if_type == IFT_ISO88025){
			TOKEN_MAP_IP_MULTICAST(ipdst, desten);
		} else {
			ETHER_MAP_IP_MULTICAST(ipdst, desten);
		}
		return(1);
	} else if (m->m_flags & M_BCAST) {
		ether_copy(etherbroadcastaddr, desten);
		return (1);
	}

	/*
	 * We take the lock for reading initially to get parallelism.
	 * If we need to modify the rt_gwroute member, we must
	 * grab the write lock.  We take pains, sometimes at
	 * the expense of ugly locking code, to grab the write lock
	 * only when absolutely necessary.
	 * In order to allow for multiple readers potentially messing with
	 * the same arp/route entry, we use swap_ptr() to manipulate the held
	 * message.
	 */
	ROUTE_RDLOCK();
	if (rt = rt0) {
retry:
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0) {
				promoted = rtrequest_promote();
				if (rt->rt_gwroute == 0) 
					goto lookup;
				else {
					/* rt_gwroute was changed while trying to 
					   get the write lock; demote and continue */
					#pragma mips_frequency_hint NEVER
					rtrequest_demote(promoted);
				} 
			}
			if (((rt->rt_flags & (RTF_HOST|RTF_STATIC))==RTF_HOST)
			    && (rt->rt_gwroute == rt
				|| !(rt->rt_gwroute->rt_flags & RTF_LLINFO))) {
				struct sockaddr_dl *sdl =
					SDL(rt->rt_gwroute->rt_gateway);
				if (sdl && sdl->sdl_family == AF_LINK) {
					/*
					 * the hacks just keep on coming:
					 * this was an ARP entry, so free &
					 * get again; this avoids the
					 * message below and the host route
					 * getting deleted just because we
					 * had a stale gwroute.
					 */
					tmprt =	rt->rt_gwroute;
					promoted = rtrequest_promote();
					if (rt->rt_gwroute) {
						if (rt->rt_gwroute != tmprt) {
					           /* rt_gwroute was changed while 
						      trying to grab the write lock; 
						      retry since we do not know  
						      what type of route was added */
					      #pragma mips_frequency_hint NEVER
							rtrequest_demote(
								    promoted);
							goto retry;
						}
						rtfree(rt->rt_gwroute);
					}
					goto lookup;
				}
					
				/*
				 * We have a host route that points to
				 * itself, or to some other route that
				 * is not an ARP entry, but we need to
				 * ARP.  Free the route and get an ARP
				 * entry instead.  Only do this if the
				 * route is non-static; we trust the
				 * system administrator to know what
				 * they're doing.
				 *
				 * The locking is messy; we should
				 * clean this up some day.
				 */
				promoted = rtrequest_promote();
				cmn_err(CE_WARN,
					"!ARP: deleting route %s-->%s",
					inet_ntoa(SIN(rt_key(rt))->sin_addr),
					inet_ntoa(SIN(rt->rt_gateway
						      )->sin_addr));
				rtfree(rt->rt_gwroute);
				rt->rt_gwroute = 0;
				(void) rtrequest(RTM_DELETE, rt_key(rt),
					rt->rt_gateway, rt_mask(rt),
					rt->rt_flags, (struct rtentry **)0);
				rtrequest_demote(promoted);
				goto makeone;
			}
			if (((rt=rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				promoted = rtrequest_promote();
				if (rt0->rt_gwroute == rt) {
					rtfree(rt);
					rt = rt0;
lookup:
                                	rt->rt_gwroute = 
						rtalloc1(rt->rt_gateway, 1);
				} else {
					/* rt_gwroute was changed while trying to
					   grab write lock; since change must have
					   occured in this logic block, just 
					   continue using new rt_gwroute          */
					#pragma mips_frequency_hint NEVER
					rt = rt0;
				}
				rtrequest_demote(promoted);
				if ((rt = rt->rt_gwroute) == 0) {
					ROUTE_UNLOCK();
					*errp = EHOSTUNREACH;
					if (m) m_freem(m);
					return(0);
				}
			}
		}
		if ((rt->rt_flags & RTF_REJECT) &&
			((rt->rt_rmx.rmx_expire == 0) ||
			(time < rt->rt_rmx.rmx_expire))) {
				ROUTE_UNLOCK();
				if (rt == rt0)
					*errp = EHOSTDOWN;
				else
					*errp = EHOSTUNREACH;
				if (m) {
					m_freem(m);
				}
				return(0);
		}
	}
	if (rt) {
		la = (struct llinfo_arp *)rt->rt_llinfo;
		ASSERT(!(rt->rt_flags & RTF_LLINFO) || la);
		ASSERT(!la || (rt->rt_flags & RTF_LLINFO));
	} else {
makeone:
		if (la = arplookup(ipdst->s_addr, 1, 0))
			rt = la->la_rt;
	}
	if (la == 0 || rt == 0) {
		m_freem(m);
		ROUTE_UNLOCK();
		*errp = EHOSTUNREACH;
		return (0);
	}
	sdl = SDL(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, that the interface
	 * is up and still has the right address, and that the address
	 * is resolved.
	 * Otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time)
	    && sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0
	    && rt->rt_ifp != 0 && !iff_dead(rt->rt_ifp->if_flags)
	    && rt->rt_ifa != 0 && (rt->rt_ifa->ifa_flags & IFA_ROUTE)) {
		bcopy(LLADDR(sdl), desten, sdl->sdl_alen);
		ROUTE_UNLOCK();
		return 1;
	}
	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet.  Replace the held mbuf with this
	 * latest one.
	 */
	if (om = swap_ptr((void **)&la->la_hold, (void *)m)) {
		m_freem(om);
	}
	if (rt->rt_expire) {
		rt->rt_flags &= ~RTF_REJECT;
		if (la->la_asked == 0 || rt->rt_expire != time) {
			rt->rt_expire = time;
			if (la->la_asked++ < arp_maxtries) {
				needsend = 1;
			} else {
				rt->rt_flags |= RTF_REJECT;
				rt->rt_expire += arpt_down;
				la->la_asked = 0;
			}

		}
	}
	ROUTE_UNLOCK();
	if (needsend)
		ARP_WHOHAS(ac, ipdst);

	return (0);
}

/*
 * backward compatibility interface for those drivers not modified to
 * use the new implementation of arp
 */
int
ip_arpresolve(struct arpcom *ac, struct mbuf *m, struct in_addr *ipdst,
	u_char *desten)
{
	int error;
	int status;
	struct rtentry *rt;
	struct ifaddr *ifa;
	struct sockaddr_in sin;

	/*
	 * we have to look up the route here so that arpresolve will work
	 * properly if the route is a gateway route or if the route entry
	 * has not been fully initialized (as might be the case early
	 * in system startup)
	 * Allocate the route and hold the reference count until after
	 * arpresolve.
	 */
	sin = sin0;
	sin.sin_addr.s_addr = ipdst->s_addr;
	ROUTE_RDLOCK();
	/*
	 * If the ifnet structure field if_rtrequest is not arp_rtrequest,
	 * set it here.  We need this to be set so that routes are
	 * properly initialized.  We also need to set ifa_rtrequest in
	 * the ifaddr so that the data allocated by arp_rtrequest can
	 * be freed.
	 */
	if (ac->ac_if.if_rtrequest != arp_rtrequest) {
		ac->ac_if.if_rtrequest = arp_rtrequest;
		/*
		 * Get the ifaddr and make sure ifa_rtrequest is arp_rtrequest.
		 */
		ifa = ifaof_ifpforaddr((struct sockaddr *)&sin, &ac->ac_if);
		if (ifa->ifa_rtrequest != arp_rtrequest) {
			ifa->ifa_rtrequest = arp_rtrequest;
		}
	}
	rt = rtalloc1((struct sockaddr *)&sin, 1);
	if (rt) {
		/*
		 * Make sure the ifaddr for the route references arp_rtrequest.
		 * If it does not, make it do so and make the appropriate calls
		 * to arp_rtrequest (the calls which would have been made by
		 * rtrequest).
		 */
		if (rt->rt_ifa && (rt->rt_ifa->ifa_rtrequest != arp_rtrequest)) {
			rt->rt_ifa->ifa_rtrequest = arp_rtrequest;
			/*
			 * Always do an RTM_ADD.  If the route is a cloning one, also
			 * do an RTM_RESOLVE.  This will complete the calls to
			 * arp_rtrequest which would have been made had ifa_rtrequest
			 * been set properly.
			 */
			arp_rtrequest(RTM_ADD, rt, NULL);
			if (rt->rt_flags & RTF_CLONING) {
				arp_rtrequest(RTM_RESOLVE, rt, NULL);
			}
		}
		ROUTE_UNLOCK();
		status = arpresolve(ac, rt, m, ipdst, desten, &error);
		if (!status && error) {
			cmn_err(CE_WARN, "!ip_arpresolve: error %d for IP address %s\n",
				error, inet_ntoa(*ipdst));
		}
		ROUTE_WRLOCK();
		rtfree(rt);
		ROUTE_UNLOCK();
	} else {
		ROUTE_UNLOCK();
		/*
		 * If we could not allocate the route, don't bother calling
		 * arpresolve because it must have the route.
		 */
		cmn_err(CE_WARN, "!ip_arpresolve: no route to host %s\n",
			inet_ntoa(*ipdst));
		status = 0;
	}
	return(status);
}

/*
 * Called from Ethernet interrupt handlers
 * when ether packet type ETHERTYPE_ARP
 * is received.  Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
arpinput(struct arpcom *ac, struct mbuf *m)
{
	register struct arphdr *ar;

	M_SKIPIFHEADER(m);
	if (m->m_len < sizeof(struct arphdr))
		goto out;
	ar = mtod(m, struct arphdr *);
	if (ntohs(ar->ar_hrd) != ARPHRD_ETHER
	    && ntohs(ar->ar_hrd) != ARPHRD_802)
		goto out;
	if (m->m_len < sizeof(struct arphdr) + 2 * ar->ar_hln + 2 * ar->ar_pln)
		goto out;

	switch (ntohs(ar->ar_pro)) {

	case ETHERTYPE_IP:
		in_arpinput(ac, m, 0);
		return;

	default:
		break;
	}
out:
	m_freem(m);
}

/* SRCROUTE */
void
srarpinput(struct arpcom *ac, struct mbuf *m, u_short *sri)
{
	register struct arphdr *ar;

	M_SKIPIFHEADER(m);

	if (m->m_len < sizeof(struct arphdr))
		goto out;
	ar = mtod(m, struct arphdr *);

	/* O.K. Let's be a little more tolerant */
	if (ntohs(ar->ar_hrd) != ARPHRD_ETHER
	    && ntohs(ar->ar_hrd) != ARPHRD_802)
		goto out;

	if (m->m_len < sizeof(struct arphdr) + 2 * ar->ar_hln + 2 * ar->ar_pln)
		goto out;

	switch (ntohs(ar->ar_pro)) {
	case ETHERTYPE_IP:
		in_arpinput(ac, m, sri);
		return;
	default:
		break;
	}
out:
	m_freem(m);
}

/* ARGSUSED */
struct in_ifaddr *
arp_findaddr(struct ifnet *ifp, struct in_addr *itaddr, struct in_addr *isaddr,
	u_char *tenaddr, u_char *senaddr, int op)
{
	struct in_ifaddr *ia = 0;

	/*
	 * look up itaddr or isaddr in hashinfo_inaddr, matching when
	 * one of the two addresses matches a key in the table
	 */
	ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
		in_arp_match, (caddr_t)itaddr, (caddr_t)isaddr,
		(caddr_t)ifp);

	if (!ia) {
		/*
		 * No address match was found, so match on the interface
		 */
		ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
			ifnet_enummatch, HTF_INET, (caddr_t)0,
			(caddr_t)ifp, (caddr_t)0);
	}
	return(ia);
}

/*
 * ARP for Internet protocols on 10 Mb/s Ethernet.
 * Algorithm is that given in RFC 826.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 * We no longer handle negotiations for use of trailer protocol:
 * Formerly, ARP replied for protocol type ETHERTYPE_TRAIL sent
 * along with IP replies if we wanted trailers sent to us,
 * and also sent them in response to IP replies.
 * This allowed either end to announce the desire to receive
 * trailer packets.
 * We no longer reply to requests for ETHERTYPE_TRAIL protocol either,
 * but formerly didn't normally send requests.
 */
static void
in_arpinput(register struct arpcom *ac, struct mbuf *m, u_short *sri)
{
	static struct ether_arp zeros;
	static time_t last_bcast_warn = 0;
	struct in_ifaddr *ia = 0;
	int create = 0;
	struct mbuf *held_mp = NULL;
	register struct ether_arp *ea;
	struct ether_header *eh;
	register struct llinfo_arp *la = 0;
	register struct rtentry *rt;
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	int op;


	ASSERT(IFNET_ISLOCKED(&ac->ac_if));
	ea = mtod(m, struct ether_arp *);
	op = ntohs(ea->arp_op);
	IP_copy(ea->arp_spa, &isaddr);
	IP_copy(ea->arp_tpa, &itaddr);

	if (ac->ac_if.if_findaddr) {
		ia = (*ac->ac_if.if_findaddr)(&ac->ac_if,
					      (void *)&itaddr, (void *)&isaddr,
					      ea->arp_tha, ea->arp_sha, op);
	} else {
		ia = arp_findaddr(&ac->ac_if, &itaddr, &isaddr,
				  ea->arp_tha, ea->arp_sha, op);
	}
	if (!ia)
		goto out;

	myaddr = IA_SIN(ia)->sin_addr;
	if (!bcmp(ea->arp_sha, ac->ac_enaddr,sizeof(ea->arp_sha)))
		goto out;		/* it's from me, ignore it. */

	if (!bcmp(ea->arp_sha, etherbroadcastaddr, sizeof(ea->arp_sha))) {
		cmn_err(CE_WARN,
		    "!ARP: MAC address is broadcast for IP address %s",
		    inet_ntoa(isaddr));
		goto out;
	}

	if (!bcmp(ea->arp_sha, zeros.arp_sha, sizeof(ea->arp_sha))) {
		cmn_err(CE_WARN,
		    "!ARP: MAC address is 0 for IP address %s",
		    inet_ntoa(isaddr));
		goto out;
	}

	/*
	 * RFC 1122 says we should silently discard an incoming datagram
	 * containing an broadcast addr as an IP source address.
	 */
#define TIME_BCAST_WARN	(60*60*24)	/* warning at most once a day */
	if (in_broadcast(isaddr, &ac->ac_if)) {
		if ((time - last_bcast_warn) >= TIME_BCAST_WARN) {
		    last_bcast_warn = time;
		    cmn_err(CE_WARN,
		    	"!ARP: got MAC address on %s for BCAST IP address %s",
		    	ac->ac_if.if_name, inet_ntoa(isaddr));
		}
		goto out;
	}

	ROUTE_RDLOCK();
	if (isaddr.s_addr == myaddr.s_addr) {
		if (scream_arpcom) {
			if (scream_timer >= SCREAM_TIME
				&& scream_arpcom == ac) {
					scream_timer = 0;
					cmn_err(CE_ALERT,
						"arp: host with MAC address %s"
						" is still using my IP address"
						" %s!\n",
						ether_sprintf(&scream_sha[0]),
						inet_ntoa(scream_iaddr));
			}
		} else {
			ether_copy(ea->arp_sha, &scream_sha[0]);
			scream_iaddr.s_addr = myaddr.s_addr;
			scream_arpcom = ac;
			scream_timer = 0;
			cmn_err(CE_ALERT, "arp: host with MAC address %s"
				" is using my IP address %s!\n",
				ether_sprintf(&scream_sha[0]),
				inet_ntoa(scream_iaddr));
		}
		itaddr = myaddr;
		ROUTE_UNLOCK();
		goto reply;
	}
	create = (itaddr.s_addr == myaddr.s_addr);
	la = arplookup(isaddr.s_addr, create, 0);
	ASSERT(!la || !la->la_rt || la->la_rt->rt_gateway);
	if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))) {
#ifdef DEBUG
		if (sdl->sdl_alen &&
		    bcmp(ea->arp_sha, LLADDR(sdl), sdl->sdl_alen))
			cmn_err(CE_NOTE,
			    "!arp info for %s overwritten from %s to %s\n",
			    inet_ntoa(isaddr),
			    ether_sprintf((u_char *)LLADDR(sdl)),
			    ether_sprintf(ea->arp_sha));
#endif
		bcopy(ea->arp_sha, LLADDR(sdl),
			    sdl->sdl_alen = sizeof(ea->arp_sha));
		if (rt->rt_expire)
			rt->rt_expire = time + arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;
		held_mp = swap_ptr((void **)&la->la_hold, 0);
		if (sri) {
			(void)sriset((char*)&ea->arp_sha[0], sri, 1, create);
		}
		ROUTE_UNLOCK();
		if (held_mp) {
			(*ac->ac_if.if_output)(&ac->ac_if, held_mp,
				rt_key(rt), rt);
		}
	} else {
		ROUTE_UNLOCK();
	}
reply:
	if (op != ARPOP_REQUEST) {
out:
		m_freem(m);
		return;
	}
	if (itaddr.s_addr == myaddr.s_addr) {
		/* I am the target */
		ether_copy(ea->arp_sha, ea->arp_tha);
		ether_copy(ac->ac_enaddr, ea->arp_sha);
	} else {
		ROUTE_RDLOCK();
		la = arplookup(itaddr.s_addr, 0, SARP_PROXY);
		if (!la) {
			ROUTE_UNLOCK();
			goto out;
		}
		rt = la->la_rt;
		/*
		 * This test is not strictly necessary, because arplookup()
		 * should not find a local ARP entry, such they are
		 * host routes and will not match the SARP_PROXY address.
		 */
		if (!(rt->rt_flags & RTF_ANNOUNCE)) {
			ROUTE_UNLOCK();
			goto out;
		}
		ether_copy(ea->arp_sha, ea->arp_tha);
		sdl = SDL(rt->rt_gateway);
		bcopy(LLADDR(sdl), ea->arp_sha, sizeof(ea->arp_sha));
		ROUTE_UNLOCK();
	}

	IP_copy((caddr_t)ea->arp_spa, (caddr_t)ea->arp_tpa);
	IP_copy((caddr_t)&itaddr, (caddr_t)ea->arp_spa);
	ea->arp_op = htons(ARPOP_REPLY);
	ea->arp_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	eh = (struct ether_header *)sa.sa_data;
	ether_copy((caddr_t)ea->arp_tha, (caddr_t)eh->ether_dhost);
	eh->ether_type = ETHERTYPE_ARP;
	sa.sa_family = AF_UNSPEC;
#ifdef _HAVE_SA_LEN
	sa.sa_len = sizeof(sa);
#endif /* _HAVE_SA_LEN */
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa, (struct rtentry *)0);
	return;
}

arpioctl(int cmd, caddr_t data)
{
	register struct sockaddr_dl *sdl;
	register struct rtentry *rt;
	register struct arpreq *ar = (struct arpreq *)data;
	register struct llinfo_arp *la = 0;
	register struct sockaddr_in *sin;
	int promoted = 0;

	if (ar->arp_pa.sa_family != AF_INET ||
	    ar->arp_ha.sa_family != AF_UNSPEC)
		return (EAFNOSUPPORT);

	sin = (struct sockaddr_in *)&ar->arp_pa;
	ROUTE_RDLOCK();
	la = arplookup(sin->sin_addr.s_addr,
		       (cmd == SIOCSARP) ? 1 : 0, 0);
	if (!la) {
		/* if not found, try for a proxy-only entry */
		la = arplookup(sin->sin_addr.s_addr,
			       (cmd == SIOCSARP) ? 1 : 0, SARP_PROXY);
		if (!la) {
			if (!ifa_ifwithnet(&ar->arp_pa)) {
				ROUTE_UNLOCK();
				return (ENETUNREACH);
			}
			ROUTE_UNLOCK();
			return (ENOENT);
		}
	}
	ASSERT(la->la_rt);
	rt = la->la_rt;
	ASSERT(rt->rt_gateway);
	switch (cmd) {

	case SIOCSARP:		/* set entry */
	case SIOCSARPX:	{	/* Ext set entry */
		sdl = SDL(rt->rt_gateway);
		sdl->sdl_alen = 6;
		ether_copy((caddr_t)ar->arp_ha.sa_data, (caddr_t)LLADDR(sdl));

		/* SRCROUTE */
		if (cmd == SIOCSARPX) {
			struct sritab *st;
			struct arpreqx *arx = (struct arpreqx *)ar;

			if (sriset((char*)LLADDR(sdl),arx->arp_sri,1,0)) {
				ROUTE_UNLOCK();
				return(EADDRNOTAVAIL);
			}
			if ((st = srifind((char*)LLADDR(sdl),1)) == 0) {
				ROUTE_UNLOCK();
				return(EADDRNOTAVAIL);
			}
			st->st_flags |= SRIF_PERM;
		}
		/* SRCROUTE */
		/*
		 * If the entry is to be permanent, set the expiration
		 * time to 0, otherwise, set it to the current time
		 * plus the keep time.
		 */
		if (ar->arp_flags & ATF_PERM) {
			rt->rt_expire = 0;
		} else {
			rt->rt_expire = time + arpt_keep;
		}
		/*
		 * If the entry is to be published, send out an ARP
		 * request.
		 */
		if (ar->arp_flags & ATF_PUBL) {
			rt->rt_flags |= RTF_ANNOUNCE;
			RT_HOLD(rt);
			ROUTE_UNLOCK();
			IFNET_LOCK(rt->rt_ifp);
			arprequest((struct arpcom *)rt->rt_ifp,
				   &SIN(rt_key(rt))->sin_addr,
				   &SIN(rt_key(rt))->sin_addr,
				   (u_char *)LLADDR(sdl));
			IFNET_UNLOCK(rt->rt_ifp);
			ROUTE_RDLOCK();
			rtfree_needpromote(rt);
		}
		}
		break;

	/* SRCROUTE */
	case SIOCDARPX:		/* Ext delete entry */
		sdl = SDL(rt->rt_gateway);
		sritfree((char*)LLADDR(sdl), 1);
		/* FALL THRU... */

	case SIOCDARP:		/* delete entry */
		promoted = rtrequest_promote();
		arptfree(la);
		rtrequest_demote(promoted);
		break;

	case SIOCGARP:		/* get entry */
	case SIOCGARPX:		/* Ext get entry */
		sdl = SDL(rt->rt_gateway);
		bcopy((caddr_t)LLADDR(sdl), (caddr_t)ar->arp_ha.sa_data,
		    sdl->sdl_alen);
		if ((rt->rt_expire == 0 || rt->rt_expire > time) &&
			sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
				ar->arp_flags = ATF_COM |
					(rt->rt_expire ? 0 : ATF_PERM);
		} else {
			ar->arp_flags = 0;
		}
		if (rt->rt_flags & RTF_ANNOUNCE) {
			ar->arp_flags |= ATF_PUBL;
		}

		/* SRCROUTE */
		if (cmd == SIOCGARPX) {
			struct sritab *st =srifind((char*)LLADDR(sdl),1);
			struct arpreqx *arx = (struct arpreqx *)ar;

			arx->arp_sri[0] = 0;
			if (!st)
				break;
			bcopy((caddr_t)&st->st_ri[0],(caddr_t)&arx->arp_sri[0],
				sizeof(st->st_ri));
		}
		break;
	}
	ROUTE_UNLOCK();
	return (0);
}

/*
 * Convert an Internet multicast socket address into an ethernet address.
 */
ether_cvtmulti(register struct sockaddr *sa, int *all)
{
#ifdef INET6
    switch (((struct sockaddr_new *)sa)->sa_family) {
#else
    switch (sa->sa_family) {
#endif
	case AF_UNSPEC:
	    break;

	case AF_INET:
	    if (((struct sockaddr_in *)sa)->sin_addr.s_addr == INADDR_ANY) {
		/*
		 * An IP address of INADDR_ANY means listen to all
		 * of the Ethernet multicast addresses used for IP.
		 * (This is for the sake of IP multicast routers.)
		 */
		*all = 1;
		return 0;
	    } else {
		struct in_addr ia;
		ia = ((struct sockaddr_in *)sa)->sin_addr;
		ETHER_MAP_IP_MULTICAST(&ia, sa->sa_data);
		sa->sa_family = AF_UNSPEC;
	    }
	    break;
#ifdef INET6
	case AF_INET6:
	    if (IS_ANYSOCKADDR((struct sockaddr_in6 *)sa)) {
		    /*
		     * An unspec IPv6 address means listen to all
		     * of the Ethernet multicast addresses used for IPv6.
		     * (This is for the sake of IP multicast routers.)
		     */
		    *all = 1;
		    return 0;
	    } else {
		    struct sockaddr_in6 sin6;

		    sin6 = *(struct sockaddr_in6 *)sa;
		    ETHER_MAP_IP6_MULTICAST(&sin6.sin6_addr, sa->sa_data);
		    sa->sa_family = AF_UNSPEC;
	    }
	    break;
#endif

	case AF_RAW:
	    sa->sa_family = AF_UNSPEC;
	    break;

	default:
	    return EAFNOSUPPORT;
    }

    /*
     * Verify that we have a valid Ethernet multicast addresses.
     */
    if ((sa->sa_data[0] & 1) != 1) {
	    return EINVAL;
    }
    *all = 0;
    return(0);
}

/* SRCROUTE */
/*
 * This routine should be called within splimp.
 * Called by delete ioctl and when corresponding arp entry is deleted.
 */
static void
sritfree(char *addr, u_char arpflg)
{
	struct sritab *st;

	if (!sritab)
	    return;

	SRITAB_LOOK(st,addr,arpflg);
	if (st)
		if (st->st_ref_cnt > 0)
			st->st_ref_cnt--;
		else
			bzero((char*)st, sizeof(*st));
}

/*
 * flushing sri/arp table is expensive.
 * Arptimo should compensate to minimize sri stealing.
 */
void
sritdelete(struct sritab *st)
{
	/* XXX: should call sritfree(), but for speed.... */
	bzero((char*)st, sizeof(*st));
}

/*
 * Allocate new src route table.
 * Entries used by arp and none-arp are mutually excluded.
 */
static struct sritab *
srinew(char *addr, u_char arpflg)
{
	register n;
	int oldest = -1;
	register struct sritab *st, *sto = NULL;

	st = &sritab[SRITAB_HASH(addr)*_ARPTAB_BSIZ];
	for (n = 0; n < _ARPTAB_BSIZ; n++,st++) {
		if ((st->st_flags&SRIF_VALID) == 0)
			goto out;	 /* found an empty entry */
		if (st->st_flags&SRIF_PERM)
			continue;
		if ((arpflg && (st->st_flags & SRIF_NONARP)) ||
		    (!arpflg && (st->st_flags & SRIF_ARP)))
			continue;
		if (st->st_ref_cnt)     /* this is shared by many */
			continue;
		if ((int) st->st_timer > oldest) {
			oldest = st->st_timer;
			sto = st;
		}
	}
	if (sto == NULL)
		return (NULL);
	st = sto;

	/* If stealing a sri, then arptab also must be freed */
	sritdelete(st);
out:
	bcopy(addr, (char*)&st->st_addr[0], sizeof(st->st_addr));
	st->st_flags = (arpflg == 0) ? SRIF_NONARP : SRIF_ARP;
	return(st);
}

/*
 * This routine is call by 802 drivers to fill route info into MAC header.
 */
struct sritab *
srifind(char *addr, u_char arpflg)
{
	struct sritab *st;

	if (!sritab) {
		sritab = kmem_zalloc(_ARPTAB_SIZE * sizeof (struct sritab),
				     KM_NOSLEEP);
		if (!sritab)
		    return(NULL);
		sritab_size = _ARPTAB_SIZE;
	}

	SRITAB_LOOK(st,addr,arpflg);
	if (!st)
		return(NULL);
	if ((st->st_flags & SRIF_VALID) == 0)
		return(NULL);
	st->st_timer = 0;		/* reset the timer */
	return(st);
}

u_short *
srilook(char *addr, u_char arpflg)
{
	struct sritab *st;

	st = srifind(addr, arpflg);
	if (!st)
		return(NULL);
	return(&st->st_ri[0]);
}

/*
 * This routine is called only by in_arpinput and set arpioctl.
 */
static int
sriset(char*addr, u_short *ri, u_char arpflg, u_char newarp)
{
	u_long len;
	u_short *ori;
	struct sritab *st;
	register u_long n;

	if (!sritab) {
		sritab = kmem_zalloc(_ARPTAB_SIZE * sizeof (struct sritab),
				     KM_NOSLEEP);
		if (!sritab)
		    return(-1);
	}

	st = &sritab[SRITAB_HASH(addr) * _ARPTAB_BSIZ];
	for (n = 0 ; n < _ARPTAB_BSIZ ; n++,st++) {
		if ((arpflg && (st->st_flags & SRIF_NONARP)) ||
		   (!arpflg && (st->st_flags & SRIF_ARP)))
			continue;
		if (!bcmp(st->st_addr,addr,sizeof(st->st_addr)))
			break;
		}
	if (n >= _ARPTAB_BSIZ) {
		st = srinew(addr, arpflg);
		if (!st)
			return(-1);
	} else {
		if (newarp) {
			st->st_ref_cnt++;
		}
	}
	/* Take latest no matter what because the shortest but old route
	 * maybe dead by now.
	 */
	ori = &st->st_ri[0];
#ifdef STASH
	if ((st->st_flags & SRIF_VALID) &&
	   ((ori[0]&SRI_LENMASK) <= (ri[0]&SRI_LENMASK)))
		return(0);
#endif
	len = ((ri[0]&SRI_LENMASK) >> 8);
	bcopy((char*)ri, (char*)ori, len);
	if (arpflg)
		st->st_flags |= (SRIF_VALID|SRIF_ARP);
	else
		st->st_flags |= (SRIF_VALID|SRIF_NONARP);
	return(0);
}
