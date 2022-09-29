/*
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)in_pcb.h	8.1 (Berkeley) 6/10/93
 */
#ifndef __NETINET_INPCB_H__
#define __NETINET_INPCB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/sema.h"
#ifdef INET6
#include <netinet/ipsec.h>
#endif
struct in_addr;
struct mbuf;
struct route;
struct socket;
struct inpcbstat;

#ifdef INET6
/*
 * because IPv6 addresses are huge !
  */
union route_6 {
	struct route route;
	struct {
		struct  rtentry *space_rt;
		struct  sockaddr_in6 spare_dst;
	} spare_route6;
};

union in_addr_6 {
	struct {
		u_int32_t spare[3];
		struct    in_addr addr;
	} in_addr_4;
	struct  in6_addr addr6;
};
#endif

struct inaddrpair {
#ifdef INET6
	union	in_addr_6 iap_faddr6;	/* foreign host table entry */
#else
	struct	in_addr iap_faddr;	/* foreign host table entry */
#endif
	u_short	iap_fport;		/* foreign port */
#ifdef INET6
	union	in_addr_6 iap_laddr6;	/* local host table entry */
#else
	struct	in_addr	iap_laddr;	/* local host table entry */
#endif
	u_short	iap_lport;		/* local port */
};

/*
 * To save space, each hash list uses a small head structure consisting
 * only of threading pointers.
 */
struct in_pcbhead {
	/* fields must line up with PCB below */
	struct	inpcb *hinp_next;
	struct	inpcb *hinp_prev;
	struct  inpcb *hinp_waste;
	int	hinp_refcnt;
#ifdef _PCB_DEBUG
	int	hinp_traceable;
#endif
        mutex_t hinp_sem;
};

#ifdef _PCB_DEBUG
struct inpcb_trace_rec {
	long 	*addr;
	short	op;
	short	cpu;
	int	refcnt;
};

#define	_N_PCB_TRACE	128	

extern int atomicIncPort(int *, int, int);
#define	_IN_PCB_TRACE(inp, opr) { \
	if (inp->inp_traceable) { \
		int t;	\
		t = atomicIncPort(&inp->inp_trace_ptr, _N_PCB_TRACE - 1, 0); \
		(inp)->inp_trace[t].addr = __return_address; \
		(inp)->inp_trace[t].op = (opr); \
		(inp)->inp_trace[t].cpu = cpuid(); \
		(inp)->inp_trace[t].refcnt = (inp)->inp_refcnt; \
	} \
}

#else
#define	_IN_PCB_TRACE(inp, opr)
#endif	/* _PCB_DEBUG */

#ifdef INET6
/*
 * This is defined here rather than ip6_icmp.h so that ip6_icmp.h does not
 * have to be included all over the place.
 */
struct icmp6_filter {
	u_int32_t icmp6_filt[8];        /* 8*32 = 256 bits */
};
#endif

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */
struct inpcb {
	struct	inpcb *inp_next,*inp_prev;
					/* pointers to other pcb's */
	struct	inpcb *inp_head;	/* pointer back to chain of inpcb's
					   for this protocol */

	int	inp_refcnt;		/* reference count */
#ifdef _PCB_DEBUG
	int	inp_traceable;
#endif
	struct	in_pcbhead *inp_hhead;	/* pointer to hash head */
	struct	inaddrpair inp_iap;	/* foreign and local addr pair */
	/* needed by both heads and individual PCBs */
	u_short		inp_hashflags;	/* hashing flags */
	union {
	   /* The real PCB */		
	   struct {
		struct	socket *u1_socket;/* back pointer to socket */
		caddr_t	u1_ppcb;	  /* pointer to per-protocol pcb */
#ifdef INET6
		union	route_6 u1_route; /* placeholder for routing entry */
#else
		struct	route u1_route;   /* placeholder for routing entry */
#endif
		u_short	u1_hashval;	  /* Hash value */
		u_char	u1_ip_tos;	  /* TOS */
		u_char	u1_ip_ttl;	  /* TTL */
#ifdef INET6
		u_int8_t u1_rcvttl;	  /* receive time to live */
		struct ifnet *u1_rcvif;	  /* received interface */
#endif
		int	u1_flags;	  /* generic IP/datagram flags */
		struct	mbuf *u1_options; /* IP options */
		struct	mbuf *u1_moptions;/* IP multicast options */
#ifdef INET6
		u_int16_t u1_fatype;      /* foreign address type */
		u_int	u1_iflowinfo;	  /* input flow label */
		u_int	u1_oflowinfo;	  /* output flow label & prio */
		u_int16_t u1_latype;      /* local address type */
#ifdef INTERFACES_ARE_SELECTABLE
		struct ifaddr *u1_ifa;         /* interface address to use */
#define INP_IFA (inp ? inp->inp_ifa : (struct ifaddr *)0)
#else
#define INP_IFA ((struct ifaddr *)0)
#endif
#endif /* INET6 */
	   } pcb_u1;
	   /* The hashing stuff */
	   struct {
		/*
		 * Pcb_u2 is currently 16 bytes smaller than pcb_u1.
		 */
#ifdef INET6
		u_short		(*u2_hashfun)(struct inpcb *, void *,
		  u_short, void *, u_short, int);
#else
		u_short		(*u2_hashfun)(struct inpcb *, struct in_addr,
			u_short,struct in_addr,u_short);
#endif

		int		u2_wildport;	/* wildcard port */
	    	int		u2_tablesz;	/* size of table */

		struct in_pcbhead    *u2_table; /* hash tbl inpcb's ptrs, etc*/
		struct inpcb	**u2_cache;	/* cached pcb (obsoleted!) */
		mutex_t u2_mutex;        	/* mutex to protect the whole list */

#ifdef DEBUG
		u_int		*u2_dstats;	/* distribution statistics */
		u_short		u2_stats;	/* statistics */
#endif
	   } pcb_u2;
	} inp_u;
#ifdef INET6
	struct ip_soptions {
		struct ip_seclist *lh_first;
	} inp_soptions; /* IP security */
	/*
	 * These two are used only by rip6.  Maybe there is a better place
	 * for them.
	 */
	u_int8_t inp_proto;
	struct icmp6_filter inp_filter;
#endif
#ifdef _PCB_DEBUG
	struct inpcb_trace_rec inp_trace[_N_PCB_TRACE];
	int inp_trace_ptr;
#define _PCB_SLOP	((_N_PCB_TRACE * sizeof(struct inpcb_trace_rec)) + \
			 (2 * sizeof(int)))

#endif	/* _PCB_DEBUG */
};

#ifdef INET6
#define	iap_faddr	iap_faddr6.in_addr_4.addr	/* compatibility */
#define	inp_faddr	inp_iap.iap_faddr6.in_addr_4.addr
#define	inp_faddr6	inp_iap.iap_faddr6.addr6
#else
#define	inp_faddr	inp_iap.iap_faddr
#endif
#define	inp_fport	inp_iap.iap_fport
#ifdef INET6
#define	iap_laddr	iap_laddr6.in_addr_4.addr	/* compatibility */
#define	inp_laddr	inp_iap.iap_laddr6.in_addr_4.addr
#define	inp_laddr6	inp_iap.iap_laddr6.addr6
#else
#define	inp_laddr	inp_iap.iap_laddr
#endif
#define	inp_lport	inp_iap.iap_lport

#define	inp_socket	inp_u.pcb_u1.u1_socket
#define	inp_ppcb	inp_u.pcb_u1.u1_ppcb
#ifdef INET6
#define	inp_route	inp_u.pcb_u1.u1_route.route
#else
#define	inp_route	inp_u.pcb_u1.u1_route
#endif
#define	inp_options	inp_u.pcb_u1.u1_options

#define	inp_moptions	inp_u.pcb_u1.u1_moptions
#define	inp_ip_ttl	inp_u.pcb_u1.u1_ip_ttl
#define	inp_ip_tos	inp_u.pcb_u1.u1_ip_tos
#define	inp_hashval	inp_u.pcb_u1.u1_hashval
#define inp_flags	inp_u.pcb_u1.u1_flags
#ifdef INET6
#define	inp_tos		inp_u.pcb_u1.u1_ip_tos
#define	inp_ttl		inp_u.pcb_u1.u1_ip_ttl
#define inp_rcvttl	inp_u.pcb_u1.u1_rcvttl
#define inp_rcvif	inp_u.pcb_u1.u1_rcvif

#define inp_iflowinfo	inp_u.pcb_u1.u1_iflowinfo
#define inp_oflowinfo	inp_u.pcb_u1.u1_oflowinfo
#define inp_fatype	inp_u.pcb_u1.u1_fatype
#define inp_latype	inp_u.pcb_u1.u1_latype
#endif

#define	inp_hashfun	inp_u.pcb_u2.u2_hashfun
#define	inp_tablesz	inp_u.pcb_u2.u2_tablesz
#define	inp_table	inp_u.pcb_u2.u2_table
#define inp_mutex       inp_u.pcb_u2.u2_mutex
#ifdef DEBUG
#define inp_stats	inp_u.pcb_u2.u2_stats
#define inp_dstats	inp_u.pcb_u2.u2_dstats
#endif
#define inp_wildport	inp_u.pcb_u2.u2_wildport

#ifdef INET6
/* address types */
#define IPATYPE_UNBD            0x0     /* unbound address */
#define IPATYPE_IPV4            0x1     /* IPv4 address */
#define IPATYPE_IPV6            0x2     /* IPv6 address */
#define IPATYPE_DUAL            0x3     /* both IPv4 & IPv6 address */
#endif

/* flags in inp_flags: */
#define	INP_RECVOPTS		0x01	/* receive incoming IP options */
#define	INP_RECVRETOPTS		0x02	/* receive IP options for reply */
#define	INP_RECVDSTADDR		0x04	/* receive IP dst address */
#ifdef INET6
#define INP_RECVPKTINFO         0x1000  /* receive packet info */
#define INP_RECVTTL             0x2000  /* receive TTL/hlim info */
#define	INP_CONTROLOPTS		\
  (INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR|INP_RECVPKTINFO|INP_RECVTTL)
#else
#define	INP_CONTROLOPTS		(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR)
#endif
#define	INP_HDRINCL		0x08	/* user supplies entire IP header */
#ifdef INET6
#define INP_NOPROBE             0x80    /* don't trigger NUD probes */

#define INP_COMPATV4            0x10    /* can be used by IPv4 stack */
#define INP_COMPATV6            0x20    /* can be used by IPv6 stack */
#define INP_COMPATANY           0x30    /* can be used by any stack */

#define INP_NEEDAUTH            0x100   /* need authentication */
#define INP_NEEDCRYPT           0x200   /* need confidentiality */
#endif

/* flags for in_pcblookup operation */
#define	INPLOOKUP_WILDCARD	0x1            
#define	INPLOOKUP_LISTEN	0x4     /* only search bucket 0 */
#define	INPLOOKUP_ALL		0x8     /* search one particular bucket */
#define	INPLOOKUP_BIND		0x10

/* flags in inp_hashflags */
#define INPFLAGS_CLTS		0x0001		/* Connectionless service */
#define INPFLAGS_LISTEN		0x0002		/* Listening endpoint */
#define INPFLAGS_MOVED		0x0004		/* PCB has moved */

#ifdef INET6
#define IP6_ADDRS_EQUAL(inp, laddr, lport, faddr, fport) \
			(((inp)->inp_lport == (lport)) && \
			((inp)->inp_fport == (fport)) && \
			SAME_ADDR6((inp)->inp_faddr6, faddr) && \
			SAME_ADDR6((inp)->inp_laddr6, laddr))
#endif

#define IP_ADDRS_EQUAL(inp, laddr, lport, faddr, fport) \
			(((inp)->inp_lport == (lport)) && \
			((inp)->inp_fport == (fport)) && \
			((inp)->inp_faddr.s_addr == (faddr).s_addr) && \
			((inp)->inp_laddr.s_addr == (laddr).s_addr))

#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb)

#ifdef _KERNEL

#ifdef INET6
#define sotopf(so)  ((so)->so_proto->pr_domain->dom_family)
#endif

#define _TBLELEM(inp)	(&(inp)->inp_head->inp_table[(inp)->inp_hashval])
#define _TBLELEMHEAD(inp)	_TBLELEM(inp)->hinp_next
#define _TBLELEMTAIL(inp)	_TBLELEM(inp)->hinp_prev

#include "sys/atomic_ops.h"
#if defined(_PCB_DEBUG) || (_PCB_UTRACE)
extern int in_pcbhold(struct inpcb *);
#define INPCB_HOLD(inp)			in_pcbhold(inp)
#else
#define INPCB_HOLD(inp)			atomicAddInt(&(inp)->inp_refcnt, 1)
#endif	/* _PCB_DEBUG */

#ifdef _PCB_UTRACE
#include "sys/rtmon.h"
#define PCB_UTRACE(name, inp, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(inp), \
		UTPACK((ra), (inp) ? (inp)->inp_refcnt : 0));
#else
#define PCB_UTRACE(name, inp, ra)
#endif	/* _PCB_UTRACE */

#define INPCB_RELE(inp)			in_pcbrele(inp)

#define INHHEAD_LOCK(hinp)   { \
        ASSERT(!mutex_mine(&(hinp)->hinp_sem)); \
        mutex_lock(&(hinp)->hinp_sem, PZERO); \
  }
#define INHHEAD_UNLOCK(hinp) { \
        ASSERT(mutex_mine(&(hinp)->hinp_sem)); \
	mutex_unlock(&(hinp)->hinp_sem); \
  }
#define INHHEAD_ISLOCKED(hinp) mutex_mine(&(hinp)->hinp_sem)

extern void	in_pcbinitcb(struct inpcb *, int, u_short, u_short);
extern void	in_losing(struct inpcb *);
extern int	in_pcballoc(struct socket *, struct inpcb *);
extern int	in_pcbbind(struct inpcb *, struct mbuf *);
extern int	in_pcbconnect(struct inpcb *, struct mbuf *);
extern int	in_pcbdetach(struct inpcb *);
extern void	in_pcbdisconnect(struct inpcb *);
extern void	in_pcbnotify(struct inpcb *, struct sockaddr *, u_short,
			     struct in_addr, u_short, int,
			     void (*)(struct inpcb *, int, void *),
			     void *);
extern int	in_pcbsetaddr(struct inpcb *, struct sockaddr_in *,
				struct inaddrpair *);
extern int	in_pcbsetaddrx(struct inpcb *, struct sockaddr_in *, 
			struct in_addr, struct inaddrpair *);
extern void	in_setpeeraddr(struct inpcb *, struct mbuf *);
extern void	in_setsockaddr(struct inpcb *, struct mbuf *);
extern int	in_pcbrele(struct inpcb *);

#ifdef INET6
struct inpcb *in_pcblookupx(struct inpcb *, void *, u_short,
	void *, u_short, int, int);
struct inpcb *in_pcbfusermatch(struct inpcb *, void *, u_short, int);
#else
struct inpcb *in_pcblookupx(struct inpcb *, struct in_addr, u_short,
	struct in_addr, u_short, int);
struct inpcb *in_pcbfusermatch(struct inpcb *, struct in_addr, u_short);
#endif
extern void	in_pcbunlink(struct inpcb *);
/* support for TCP listening endpoints */
extern void	in_pcblisten(struct inpcb *);
extern void	in_pcbunlisten(struct inpcb *);
/* support for TCP TIME-WAIT endpoints */
extern void	in_pcbmove(struct inpcb *);

#ifdef INET6
struct rtentry *in6_rthost __P((struct in6_addr *));
void        in6_rtalloc __P((struct route *, struct ifaddr *));

extern int  in6_pcbbind __P((struct inpcb *, struct mbuf *));
extern int  in6_pcbrebind __P((struct inpcb *, int));
extern int  in6_pcbconnect __P((struct inpcb *, struct mbuf *));
void     in6_pcbnotify __P((struct inpcb *, struct sockaddr *,
        u_int, struct in6_addr *, u_int, int, void (*)(struct inpcb *, int,
	void *), void *));
void     in6_pcbnotifyall __P((struct inpcb *, struct sockaddr *,
	            int, void (*)(struct inpcb *, int, void *), void *));
int      in6_setifa __P((struct inpcb *, struct ifaddr *));

int      ipsec_attach __P((struct inpcb *, struct sockaddr_ipsec *, int));
int      ipsec_detach __P((struct inpcb *, struct sockaddr_ipsec *, int));
int      ipsec_match __P((struct inpcb *, struct mbuf *,
                          struct mbuf *, int));

void     ip6_freesoptions __P((struct inpcb *));
int      ip6_getsoptions __P((struct inpcb *, int, struct mbuf **));
int      ip6_setoptions __P((struct mbuf **, struct mbuf *,
                             struct inpcb *));
#endif /* INET6 */

#endif /* _KERNEL */
#ifdef __cplusplus
}
#endif
#endif /* __NETINET_INPCB_H__ */
