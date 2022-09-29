/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)in_pcb.c	7.13 (Berkeley) 6/28/90  plus MULTICAST 1.0
 */
#ident "$Revision: 4.95 $"

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/mbuf.h"
#include "sys/domain.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/proc.h"
#include "net/soioctl.h"
#include "net/if.h"
#include "net/route.h"
#include "sys/sat.h"
#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/capability.h"
#include "sys/cmn_err.h"
#include "ksys/vproc.h"
#include "sys/kmem.h"
#include "sys/atomic_ops.h"
#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "in_var.h"
#include "ip_var.h"
#include "sys/tcpipstats.h"
#include "sys/kmem.h"
#include "sys/atomic_ops.h"
#include "sys/vsocket.h"
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/ip6_var.h>
#endif
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"

/*
 * External Procedures
 */
extern struct ifaddr *ifa_ifwithaddr(struct sockaddr *);
extern struct ifaddr *ifa_ifwithdstaddr(struct sockaddr *);
extern struct ifaddr *ifa_ifwithnet(struct sockaddr *);
extern struct in_ifaddr *ifatoia(struct ifaddr *);

extern int ifnet_enummatch(struct hashbucket *, caddr_t, caddr_t, caddr_t);

/*
 * Local defines
 */
#define	SATOSIN(sa)	((struct sockaddr_in *)(sa))
#define SINTOSA(sin)	((struct sockaddr *)(sin))


/*
 * External Global Variables
 */
extern u_char inetctlerrmap[];
extern struct hashinfo hashinfo_inaddr;
extern int in_interfaces;	/* number of non-loopback network interfaces */
extern struct ifnet *ifnet;	/* head of network interface list */
extern int in_ifaddr_count;     /* Number of address'es configured */

/*
 * Module Global Variables
 */

zone_t *inpcb_zone;

extern void m_pcbinc(int nbytes, int type);
extern void m_pcbdec(int nbytes, int type);
extern void m_mcbfail(void);

u_short in_pcbnextport(struct inpcb *inp);
int in_pcbportassign(struct inpcb *, struct in_addr, u_short, int, int);

#ifdef INET6
u_short in_pcbhashfun(struct inpcb *, void *,
			u_short, void *, u_short, int);

u_short in_pcbcltshashfun(struct inpcb *, void *,
			u_short, void *, u_short, int);
#else
u_short in_pcbhashfun(struct inpcb *, struct in_addr,
			u_short, struct in_addr, u_short);

u_short in_pcbcltshashfun(struct inpcb *, struct in_addr,
			u_short, struct in_addr, u_short);
#endif

#ifdef DEBUG
#define INPCB_STATINC(head, stat) PCBSTAT(stat, (head)->inp_stats)
#define INPCB_STATDEC(head, stat) PCBSTAT_DEC(stat, (head)->inp_stats)
#define INPCB_STATSET(head, stat, val) PCBSTAT_SET(stat, (head)->inp_stats, val)
#define INPCB_DSTATINC(head, stat) head->inp_dstats[stat]++
#define INPCB_DSTATDEC(head, stat) head->inp_dstats[stat]--
#else
#define INPCB_STATINC(head, stat)
#define INPCB_STATDEC(head, stat)
#define INPCB_STATSET(head, stat, val)
#define INPCB_DSTATINC(head, stat)
#define INPCB_DSTATDEC(head, stat)
#endif

/*
 * We currently use hash bucket 0 to contain pcbs that are in the
 * process of being created.  Theoretically, we could remove
 * hash bucket 0 from the lists that we use when we're setting
 * up notifications on all sockets.
 */
#define LEADING_BUCKETS		(1)		/* Hash0 */
#define TRAILING_BUCKETS	(0)
#define BUCKET_0		(0)
#define BUCKET_INVAL		((u_short)-1)

#define SPECIAL_BUCKETS		(LEADING_BUCKETS+TRAILING_BUCKETS)

/*
 * The following constant MUST be power of 2
 * The dynamic sizing algorithm is 'INPCB_NUMHASHPERMB' base hash bucket
 * entries for every megabyte of main memory up to the limit of
 * the minimum or maximum for either the UDP or TCP tables.
 */
#define INPCB_NUMHASHPERMB      4

#if defined(_PCB_DEBUG) || defined(_PCB_UTRACE)

int
in_pcbhold(struct inpcb *inp)
{
	int r;
	r = atomicAddInt(&inp->inp_refcnt, 1);
	_IN_PCB_TRACE(inp, 4);
	PCB_UTRACE(UTN('pcbh','old '), inp, __return_address);
	return r;
}

#ifdef _PCB_DEBUG
static void
_in_pcb_fill(struct inpcb *inp, inst_t *ra)
{
	int i;
	long *p;
	int size;

	/* fill up to but not including the trace buffer */
	size = sizeof(*inp) - _PCB_SLOP;
	size /= sizeof(inst_t *);
	for (i = 0, p = (long *)inp; i < size; p++, i++)
		*p = (long) ra + 1;
}
#endif	/* _PCB_DEBUG */
#endif	/* _PCB_DEBUG || _PCB_UTRACE */

int
in_pcb_hashtablesize()
{
	return ((int)((ctob(physmem) / (1024*1024)) * INPCB_NUMHASHPERMB));
}

/* 
 * Initialize the head of the in_pcb lookup structure
 */
/* ARGSUSED */
void
in_pcbinitcb(struct inpcb *head, int tablesz, u_short flags, u_short stats)
{
	struct in_pcbhead *h;
	int i;
	static need_zone = 1;

	if (need_zone) {
		inpcb_zone = kmem_zone_init(sizeof(*head), "inpcb struct");
		need_zone = 0;
	}

#if defined(DEBUG) && defined(_PCB_DEBUG)
	printf("sizeof PCB=%d, sizeof u1 = %d, sizeof u2 = %d\n",sizeof(*head),
	sizeof(head->inp_u.pcb_u1), sizeof(head->inp_u.pcb_u2));
#endif

	head->inp_next = head->inp_prev = head->inp_head = head;
	/*
	 * We try to compute a table size based on the requested number.
	 *
	 * It makes our lives easier (and more efficient) if we end
	 * up with a table size based on a power of 2.  This avoids having
	 * to do modulus operations in the hash functions.
	 */
	for (i = 1; i < tablesz; i <<= 1) {
	}
	head->inp_tablesz = i + SPECIAL_BUCKETS;
	head->inp_table = kmem_zalloc(head->inp_tablesz * sizeof(*h),
				      KM_SLEEP);
#ifdef DEBUG
	head->inp_dstats = kmem_zalloc(head->inp_tablesz * sizeof(u_int),
				       KM_SLEEP);
#endif

#ifdef DEBUG
	head->inp_stats = (stats < NUM_PCBSTAT) /* validate */
		? stats   /* use one supplied */
		: UDP_PCBSTAT; /* use datagram if bogus */
#endif

	head->inp_hashflags = flags;
	head->inp_refcnt = 1;

	head->inp_hashfun = (flags & INPFLAGS_CLTS)
		? in_pcbcltshashfun	/* Connectionless hash function */
		: in_pcbhashfun;	/* Connection oriented hash function */

	head->inp_wildport = IPPORT_RESERVED;

	/* This mutex is used to find a unique value of port number */
	mutex_init(&head->inp_mutex, MUTEX_DEFAULT, "inp mutex");

	h = &head->inp_table[0];
	for (i = 0; i < head->inp_tablesz; i++) {
		h->hinp_next = h->hinp_prev = (struct inpcb *)h;
		h->hinp_waste = head;
		h->hinp_refcnt = 1;
#ifdef _PCB_DEBUG
		h->hinp_traceable = 0;
#endif
		mutex_init(&h->hinp_sem, MUTEX_DEFAULT, "hinp_sem");
		h++;
	}
	return;
}

in_pcballoc(struct socket *so, struct inpcb *head)
{
	register struct inpcb *inp;

	ASSERT(SOCKET_ISLOCKED(so));
#ifdef _PCB_DEBUG
	inp = (struct inpcb *)kmem_zone_alloc(inpcb_zone, curuthread ?
		KM_SLEEP : KM_NOSLEEP);
	if (inp == NULL) {
		m_mcbfail();
		return (ENOMEM);
	}
	bzero(inp, sizeof(*inp) - _PCB_SLOP);
#else
	inp = (struct inpcb *)kmem_zone_zalloc(inpcb_zone, curuthread ? 
		KM_SLEEP : KM_NOSLEEP);
	if (inp == NULL) {
		m_mcbfail();
		return (ENOMEM);
	}
#endif
	m_pcbinc(sizeof(*inp), MT_PCB);
	inp->inp_head = head;
	inp->inp_socket = so;
	inp->inp_refcnt = 1;
	so->so_pcb = (caddr_t)inp;

	inp->inp_hashval = BUCKET_INVAL;
	

#ifdef _PCB_DEBUG
	inp->inp_traceable = 1;
	if (inp->inp_trace_ptr > (_N_PCB_TRACE - 1)) {
		inp->inp_trace_ptr = 0;
	}
	if (inp->inp_trace_ptr < 0) {
		inp->inp_trace_ptr = 0;
	}
	_IN_PCB_TRACE(inp, 0);
#endif
	PCB_UTRACE(UTN('pcba','lloc'), inp, __return_address);
	return (0);
}

in_pcbbind(register struct inpcb *inp, struct mbuf *nam)
{
	register struct socket *so = inp->inp_socket;
	register struct sockaddr_in *sin = 0;
	u_short lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	int r = 0;

	INPCB_STATINC(inp->inp_head, inpcb_bind);

	ASSERT(SOCKET_ISLOCKED(so));

	if (in_ifaddr_count <= 0) { /* no addr's configured NOT even LB */
		return EADDRNOTAVAIL;
	}

#ifdef INET6
	if (inp->inp_lport || inp->inp_latype != IPATYPE_UNBD) {
#else
	if (inp->inp_lport || inp->inp_laddr.s_addr != INADDR_ANY) {
#endif
		return (EINVAL);
	}

	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
	     (so->so_options & SO_ACCEPTCONN) == 0))
		wild = INPLOOKUP_WILDCARD;
	if (nam) {
		sin = mtod(nam, struct sockaddr_in *);
		if ((so->so_options & SO_XOPEN) && sin->sin_family != AF_INET) {
		        return EAFNOSUPPORT;
		}
		if (nam->m_len != sizeof (*sin)) 
			return (EINVAL);
		lport = sin->sin_port;
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			/* treat SO_REUSEADDR as SO_REUSEPORT for multicast */
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
		} else if (sin->sin_addr.s_addr != INADDR_ANY) {
			sin->sin_port = 0;		/* yech... */
			if (ifa_ifwithaddr((struct sockaddr *)sin) == 0) {
				sin->sin_port = lport;
				return (EADDRNOTAVAIL);
			}
		}
		if (lport) {
			/* GROSS */
			if (ntohs(lport) < IPPORT_RESERVED &&
			    !_CAP_ABLE(CAP_PRIV_PORT)) {
				return (EACCES);
			}
			r = in_pcbportassign(inp, sin->sin_addr, lport, 
					     reuseport, wild);

			/*
			 * XXX:casey
			 * The audit call that was here before
			 * caused a problem in autofs, and wasn't
			 * really appropriate anyway. The auditing
			 * done in the bind system call ought to
			 * suffice.
			 */
			return r;
		}
	}
	if (sin) {
		inp->inp_laddr = sin->sin_addr;
#ifdef INET6
		if (inp->inp_laddr.s_addr != INADDR_ANY) {
			inp->inp_latype = IPATYPE_IPV4;
			inp->inp_laddr6.s6_addr32[0] = 0;
			inp->inp_laddr6.s6_addr32[1] = 0;
			inp->inp_laddr6.s6_addr32[2] = htonl(0xffff);
		}
#endif
	}
	ASSERT(lport == 0);
	return in_pcbportassign(inp, inp->inp_laddr, lport, reuseport, wild);
}

/*
 * This routine does the work in moving pcbs from one hash chain
 * to another and assigning port numbers to pcbs as necessary.
 */
static int
tcp_full_assign(struct inpcb *inp,
	struct in_addr faddr, u_short fport,
	struct in_addr laddr)
{
	struct inpcb *t;
	struct inpcb *head = inp->inp_head;
	struct in_pcbhead *hinp;
	u_short ohashval = inp->inp_hashval;
	u_short hashval;
	u_short olport = inp->inp_lport, lport;
	int all;
	struct inpcb *tinp;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	ASSERT(fport);

moreport:
	all = 1;
	INPCB_STATINC(head, inpcb_lookup);
	if (!olport) {
	  	lport = in_pcbnextport(inp);
		/*
		 * XXX
		 * The 6.5 code has a semantic deviation from previous
		 * releases that can cause clients to get ports that
		 * servers have bound.
		 * This causes bizarre behavior in which a client can connect
		 * to another client rather than to the server.  The only time
		 * this is really a problem is if they're both on the same
		 * system.  We work around this by never handing out the same
		 * port that we're trying to connect to.
		 * A cleaner fix would be to scan bucket 0 for conflicts.
		 */
		if (lport == fport) {
			lport = in_pcbnextport(inp);
		}
	} else 
	  	lport = olport;
#ifdef INET6
	hashval = (*head->inp_hashfun)(head, &faddr, fport, &laddr, lport,
	  AF_INET);
#else
	hashval = (*head->inp_hashfun)(head, faddr, fport, laddr, lport);
#endif
	
again:
	INHHEAD_LOCK(&head->inp_table[hashval]);

	/*
	 * Compute the hash function and search the
	 * hash chain associated with this bucket for a match.
	 */
	hinp = &head->inp_table[hashval];
	for (t = hinp->hinp_next; t != (struct inpcb *)hinp; 
	     t = t->inp_next) {
	        if (!IP_ADDRS_EQUAL(t, laddr, lport, faddr, fport))
			continue;
		INPCB_STATINC(head, inpcb_found_chain);
		goto foundit;
	}

	t = 0;
foundit:
	if (t) {
	  	INPCB_STATINC(head, inpcb_success);
	  	INHHEAD_UNLOCK(&head->inp_table[hashval]);
	  	if (!all) {
	    		INHHEAD_UNLOCK(&head->inp_table[hashval - 
				       (head->inp_tablesz - SPECIAL_BUCKETS) / 2]);
	  	} 
		if (!olport)
	    		/* If we are looking for a unique number, keep looking */
	    		goto moreport;
	  	return EADDRINUSE;		
	} else {
	  	if (all) {
	    		all = 0;
	    		hashval += ((head->inp_tablesz - SPECIAL_BUCKETS) / 2);
	    		goto again;
	  	} else {
	    		/* This must be the TIME_WAIT bucket, unlock its lock */
	    		INHHEAD_UNLOCK(&head->inp_table[hashval]);
	    		hashval -= ((head->inp_tablesz - SPECIAL_BUCKETS) / 2);
	  	}

	}
	
	/*
	 * Assign addresses. 
	 */

	inp->inp_faddr = faddr;
#ifdef INET6
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		inp->inp_fatype = IPATYPE_IPV4;
		inp->inp_faddr6.s6_addr32[0] = 0;
		inp->inp_faddr6.s6_addr32[1] = 0;
		inp->inp_faddr6.s6_addr32[2] = htonl(0xffff);
	}
#endif
	inp->inp_fport = fport;
	inp->inp_laddr = laddr;
#ifdef INET6
	if (inp->inp_laddr.s_addr != INADDR_ANY) {
		inp->inp_latype = IPATYPE_IPV4;
		inp->inp_laddr6.s6_addr32[0] = 0;
		inp->inp_laddr6.s6_addr32[1] = 0;
		inp->inp_laddr6.s6_addr32[2] = htonl(0xffff);
	}
#endif
	inp->inp_lport = lport;

	/*
	 * If the two hashvals are different, move from one list to the other.
	 * This could happen in two cases: the socket has been bound, but
	 * this is the first time it has full connection; the socket is created
	 * from incoming SYN and hasn't been put into a bucket. In the second
	 * case, we don't lock another hash entry; in the first case, we are
	 * in bucket 0 and since we always lock another bucket before we lock
	 * bucket 0, it's not going to be a deadlock.
	 */
	if (ohashval != BUCKET_INVAL) {
	        INHHEAD_LOCK(inp->inp_hhead);
		remque(inp);
		INPCB_DSTATDEC(head, ohashval);
		INHHEAD_UNLOCK(inp->inp_hhead);
	}
	/* Reset hashval for the inp */
	inp->inp_hashval = hashval;
	inp->inp_hhead = &head->inp_table[hashval];
	tinp = _TBLELEMTAIL(inp);
	insque(inp, tinp);
	ASSERT(inp->inp_next != inp);
	ASSERT(inp->inp_prev != inp);
	INPCB_DSTATINC(head, inp->inp_hashval);
	INHHEAD_UNLOCK(inp->inp_hhead);
	return (0);
}

/*
 * This routine tries to find a unique port number for the PCB
 * and put the PCB in the appropriate hash bucket.
 */
int
in_pcbportassign(struct inpcb *inp,
	struct in_addr laddr, u_short lport,
	int reuseport,
	int wild)
{
	struct inpcb *t = 0, *tinp;
	struct inpcb *head = inp->inp_head;
	u_short nhashval;
	u_short olport = lport;
	struct in_addr faddr;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	faddr.s_addr = 0;
	/*
	 * Try to find a unique value for the port in the pcb table.
	 */
	do {
		if (t) {
		        mutex_unlock(&head->inp_mutex);
			if (t != inp) {
				struct socket *so2 = t->inp_socket;
				SOCKET_LOCK(so2);
				if (!INPCB_RELE(t))
					SOCKET_UNLOCK(so2);
			} else {
				INPCB_RELE(t);
			}
		}

		/*
		 * If no port was given, try to get the next port.  Rehash
		 * to determine where we're going.
		 */
		if (!olport)
			lport = in_pcbnextport(inp);
#ifdef INET6
		mutex_lock(&head->inp_mutex, PZERO);
	} while ((t = in_pcblookupx(head,
		    &faddr, 0, &laddr, lport, INPLOOKUP_BIND|INPLOOKUP_ALL|wild,
		      AF_INET)) && !olport);
#else
		mutex_lock(&head->inp_mutex, PZERO);
	} while ((t = in_pcblookupx(head, faddr, 0, laddr, lport, 
				    INPLOOKUP_BIND|INPLOOKUP_ALL|wild)) 
		 && !olport);
#endif

#ifdef INET6
        nhashval = (*head->inp_hashfun)(inp->inp_head,
					&faddr, 0, &laddr, lport, AF_INET);
#else
        nhashval = (*head->inp_hashfun)(inp->inp_head,
					faddr, 0, laddr, lport);
#endif
	if (t) {
	  	/* We can come here only if the port number isn't specified */
	  	ASSERT(olport != 0);
		if ((reuseport & t->inp_socket->so_options) == 0) {
		  
		  	mutex_unlock(&head->inp_mutex);
		  	if (t != inp) {
		        	struct socket *so2 = t->inp_socket;
				SOCKET_LOCK(so2);
				if (!INPCB_RELE(t))
			        	SOCKET_UNLOCK(so2);
		  	} else {
		        	INPCB_RELE(t);
		  	}
		  	return EADDRINUSE;		
		}
		/* clean t up down below */
	}
	
	inp->inp_laddr = laddr;
#ifdef INET6
	if (inp->inp_laddr.s_addr != INADDR_ANY) {
		inp->inp_latype = IPATYPE_IPV4;
		inp->inp_laddr6.s6_addr32[0] = 0;
		inp->inp_laddr6.s6_addr32[1] = 0;
		inp->inp_laddr6.s6_addr32[2] = htonl(0xffff);
	}
#endif
	inp->inp_lport = lport;

	/* For UDP, this could be anywhere in the hash table;
	 * For TCP, this can only be bucket 0.
	 */
        INHHEAD_LOCK(&head->inp_table[nhashval]);
	inp->inp_hashval = nhashval;
        inp->inp_hhead = &head->inp_table[inp->inp_hashval];
	tinp = _TBLELEMTAIL(inp);
	insque(inp, tinp);
        ASSERT(inp->inp_next != inp);
	ASSERT(inp->inp_prev != inp);
	INPCB_DSTATINC(head, inp->inp_hashval);
        INHHEAD_UNLOCK(inp->inp_hhead);

        mutex_unlock(&head->inp_mutex);
	if (t) {
		if (t != inp) {
			struct socket *so2 = t->inp_socket;
			/* release here now that list is unlocked */
			SOCKET_LOCK(so2);
			if (!INPCB_RELE(t))
				SOCKET_UNLOCK(so2);
		} else {
			INPCB_RELE(t);
		}
	}
	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(register struct inpcb *inp, struct mbuf *nam)
{
	register struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

	INPCB_STATINC(inp->inp_head, inpcb_connect);

	if (nam->m_len != sizeof (*sin))
		return (EINVAL);
	return (in_pcbsetaddrx(inp, sin,inp->inp_laddr,(struct inaddrpair *)0));
}

int
in_pcbsetaddr(register struct inpcb *inp, register struct sockaddr_in *sin,
	struct inaddrpair *iap)
{
	INPCB_STATINC(inp->inp_head, inpcb_setaddr);

	return in_pcbsetaddrx(inp, sin, inp->inp_laddr, iap);
}

/*
 * This routine assigns the right laddr, faddr, fport into PCB.
 * If the local port number hasn't been assigned yet, choose a
 * unique connection for it.
 */
static int
udp_full_assign(struct inpcb *inp,
	struct in_addr faddr, u_short fport,
	struct in_addr laddr, struct inaddrpair *iap)
{
	struct inpcb *t;
	struct inpcb *head = inp->inp_head;
	u_short ohashval = inp->inp_hashval;
	u_short nhashval, lport;
	u_short olport = inp->inp_lport;
	int didbind = 0;
	struct in_pcbhead *hinp;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	ASSERT(fport && faddr.s_addr);
	if (!olport) {
	  	/* If we don't have a unique port number yet, look for one 
	   	 * until we find it */
	  	ASSERT(ohashval == BUCKET_INVAL);
	  	mutex_lock(&inp->inp_head->inp_mutex, PZERO);
	  	do {
	    		lport = in_pcbnextport(inp);
#ifdef INET6
	    		nhashval = (*head->inp_hashfun)(inp->inp_head, &faddr, 
			  fport, &laddr, lport, AF_INET);
#else
	    		nhashval = (*head->inp_hashfun)(inp->inp_head, faddr, 
					    		fport, laddr, lport);
#endif
	    		hinp = &head->inp_table[nhashval];
	    		INHHEAD_LOCK(hinp);
	    		for (t = hinp->hinp_next; t != (struct inpcb *)hinp;
		 	     t = t->inp_next) {
	      			if (IP_ADDRS_EQUAL(t, laddr, lport, faddr, fport))
					goto foundit;
	      			continue;
	    		}
	    		t = 0;
	    		break;
	foundit:
	    		INHHEAD_UNLOCK(hinp);
	  	} while (t);
	} else {
	  	/* If we have full connection specified, we check to
	   	 * see if it's already in the chain.
	   	*/
	  	lport = olport;
#ifdef INET6
	  	nhashval = (*head->inp_hashfun)(inp->inp_head, &faddr, 
		  fport, &laddr, lport, AF_INET);
#else
	  	nhashval = (*head->inp_hashfun)(inp->inp_head, faddr, 
					        fport, laddr, lport);
#endif
	  	hinp = &head->inp_table[nhashval];
	  	INHHEAD_LOCK(hinp);
	  	for (t = hinp->hinp_next; t != (struct inpcb *)hinp;
	       		t = t->inp_next) {
	    		if (IP_ADDRS_EQUAL(t, laddr, lport, faddr, fport)) {
	      		INHHEAD_UNLOCK(hinp);
	      		return EADDRINUSE;
	    	}
	    	continue;
	 }
	  t = 0;
	}
	
	ASSERT(t == 0);
	if (!inp->inp_lport && iap) {
		/*
		 * Bind PCB now.  This code is needed for applications that
		 * have not previously bound and then call the UDP connect
		 * code passing in the inaddrpair structure.
		 * The local port will never get put into the PCB otherwise.
		 */
		inp->inp_lport = lport;
		didbind = 1;
	}

	/*
	 * Assign addresses.  If iap is provided, we insert the
	 * information into it.  Otherwise, we set up the inp
	 * and rehash, if necessary.
	 */

	if (iap) {
		iap->iap_faddr = faddr;
		iap->iap_fport = fport;
		iap->iap_laddr = laddr;
		iap->iap_lport = lport;
		if (didbind) {
			goto fixhash;
		}
		goto done;
	}

	inp->inp_faddr = faddr;
#ifdef INET6
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		inp->inp_fatype = IPATYPE_IPV4;
		inp->inp_faddr6.s6_addr32[0] = 0;
		inp->inp_faddr6.s6_addr32[1] = 0;
		inp->inp_faddr6.s6_addr32[2] = htonl(0xffff);
	}
#endif
	inp->inp_fport = fport;
	inp->inp_laddr = laddr;
#ifdef INET6
	if (inp->inp_laddr.s_addr != INADDR_ANY) {
		inp->inp_latype = IPATYPE_IPV4;
		inp->inp_laddr6.s6_addr32[0] = 0;
		inp->inp_laddr6.s6_addr32[1] = 0;
		inp->inp_laddr6.s6_addr32[2] = htonl(0xffff);
	}
#endif
	inp->inp_lport = lport;

fixhash:

       /*
        * If the two hashvals are different, move from one list to the other.
        */
       if (ohashval != nhashval) {
               struct inpcb *tinp;
               inp->inp_hashval = nhashval;
               inp->inp_hhead = &head->inp_table[nhashval];
               tinp = _TBLELEMTAIL(inp);
               insque(inp, tinp);
		ASSERT(inp->inp_prev != inp);
		ASSERT(inp->inp_next != inp);
               INPCB_DSTATINC(head, inp->inp_hashval);
       } 

done:
	INHHEAD_UNLOCK(hinp);
	if (!olport)
	  mutex_unlock(&inp->inp_head->inp_mutex);
	return (0);
}
/*
 * Changes made here may need to be replicated in tcp_pcbsetaddrx() if
 * appropriate.
 */
int
in_pcbsetaddrx(register struct inpcb *inp,
	struct sockaddr_in *sin,
	struct in_addr laddr,
	struct inaddrpair *iap)
{
	register struct in_ifaddr *ia, *ia_xx;
	register struct sockaddr_in *ifaddr;
	int error;
	u_short fport;

	INPCB_STATINC(inp->inp_head, inpcb_setaddrx);

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);

	if (in_ifaddr_count > 0) {

		if (ia_xx = (struct in_ifaddr *)(ifnet->in_ifaddr)) {
			/*
			 * If the destination address is INADDR_ANY,
			 * use the first interface's primary local address.
			 * If the supplied address is INADDR_BROADCAST,
			 * and the primary interface supports broadcast,
			 * choose the broadcast address for that interface.
			 */
			if (sin->sin_addr.s_addr == INADDR_ANY) {
				    sin->sin_addr = ia_xx->ia_addr.sin_addr;
			} else {
				if (sin->sin_addr.s_addr ==
				     (u_long)INADDR_BROADCAST &&
				   (ia_xx->ia_ifp->if_flags & IFF_BROADCAST)) {

					sin->sin_addr =
						ia_xx->ia_broadaddr.sin_addr;
				}
			}
		}
  	}
#ifdef INET6
	if (inp->inp_latype == IPATYPE_UNBD) {
#else
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
#endif
		register struct route *ro;

		ia = (struct in_ifaddr *)0;
		/* 
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */
		ROUTE_RDLOCK();

		ro = &inp->inp_route;
		if (ro->ro_rt &&
		    (SATOSIN(&ro->ro_dst)->sin_addr.s_addr !=
			sin->sin_addr.s_addr || 
		    inp->inp_socket->so_options & SO_DONTROUTE)) {
			rtfree_needpromote(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}
		if ((inp->inp_socket->so_options & SO_DONTROUTE)== 0 && /*XXX*/
		    (ro->ro_rt == (struct rtentry *)0 ||
		    ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
			/*
			 * No route yet, so try to acquire one
			 */
			ro->ro_dst.sa_family = AF_INET;
#ifdef _HAVE_SA_LEN
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
#endif
			((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
				sin->sin_addr;
			rtalloc(ro);
		}
		/*
		 * If we found a route, use the address corresponding to the
		 * outgoing interface unless it is the loopback (in case a
		 * route to our address on another net goes to loopback).
		 */
		if (ro->ro_rt && (ro->ro_rt->rt_srcifa ||
			!(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))) {

			ia = ro->ro_rt->rt_srcifa ?
				ifatoia(ro->ro_rt->rt_srcifa) :
				ifatoia(ro->ro_rt->rt_ifa);

			if (ia && (ia->ia_addrflags & IADDR_ALIAS)) {
				/*
				 * If the address is an alias AND has the
				 * same network interface as the routing
				 * entry we use the primary address of the
				 * network interface instead.
				 */
				if (ia->ia_ifp == ro->ro_rt->rt_ifp) {
					ia = (struct in_ifaddr *)
						ia->ia_ifp->in_ifaddr;
					if (ia == 0) {
						goto findit;
					}
				}
				/* fast exit case */
				ROUTE_UNLOCK();
				goto gotit;
			}
		}
findit:
		ROUTE_UNLOCK();

		if (ia == 0) {
			fport = sin->sin_port;
			sin->sin_port = 0;

			/* check if address associated with ppp interface */
			if (ia = ifatoia(ifa_ifwithdstaddr(SINTOSA(sin)))) {
				/* restore port number */
				sin->sin_port = fport;
				goto gotit;
			}
			/*
			 * Now do the more expensive check to find the
			 * best matching network number of all of the
			 * address'es we know about.
			 * NOTE: With huge numbers of IP alias'es this is a
			 * very expensive operation, to be avoided if possible.
			 */
			if (ia = ifatoia(ifa_ifwithnet(SINTOSA(sin)))) {
				/* restore port number */
				sin->sin_port = fport;
				goto gotit;
			}

			/* restore port number in case we fast exit */
			sin->sin_port = fport;

			if (in_interfaces > 0) { /* use primary if not any */
				ia = (struct in_ifaddr *)(ifnet->in_ifaddr);
				error = (ia) ? 0 : EADDRNOTAVAIL;
			} else {
				error = EADDRNOTAVAIL;
			}
			if (error) { /* failed so quit */
				return error;
			}
		}
gotit:
		/*
		 * If the destination address is multicast and an outgoing
		 * interface has been set as a multicast option, use the
		 * address of that interface as our source address.
		 */
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) &&
					inp->inp_moptions != NULL) {
			struct ip_moptions *imo;
			struct ifnet *ifp;

			imo = mtod(inp->inp_moptions, struct ip_moptions *);
			if (imo->imo_multicast_ifp != NULL) {
				ifp = imo->imo_multicast_ifp;

				/*
				 * Find interface address record and
				 * return its primary address.
				 */
				ia = (struct in_ifaddr *)hash_enum(
					&hashinfo_inaddr, ifnet_enummatch,
					HTF_INET, (caddr_t)0,
					(caddr_t)ifp, (caddr_t)0);

				if (ia == 0) {
					return (EADDRNOTAVAIL);
				}
			}
		}
		ifaddr = (struct sockaddr_in *)&ia->ia_addr;
	}
/* XXX what is this line for?  It doesn't look right */
	laddr = laddr.s_addr ? inp->inp_laddr : ifaddr->sin_addr;

	if (inp->inp_lport == 0) {
		/*
		 * It might be possible to just pass INPLOOKUP_WILD
		 * into in_pcbassign() below, but that *might* cause spurious
		 * conflicts.  Just get a local port the old-fashioned
		 * way.
		 */
		if ((error = in_pcbbind(inp, (struct mbuf *)0))) {
			return error;
		}
	}
	
	if (inp->inp_head->inp_hashflags & INPFLAGS_CLTS) {
	  	return udp_full_assign(inp, sin->sin_addr, sin->sin_port, laddr, iap);
	} else
	  	return tcp_full_assign(inp, sin->sin_addr, sin->sin_port, laddr);
}

void
in_pcbdisconnect(struct inpcb *inp)
{
	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
#ifdef INET6
	CLR_ADDR6(inp->inp_faddr6);
	inp->inp_fatype = IPATYPE_UNBD;
	inp->inp_oflowinfo &= IPV6_FLOWINFO_PRIORITY;
#else
	inp->inp_faddr.s_addr = INADDR_ANY;
#endif
	inp->inp_fport = 0;
	if (inp->inp_socket->so_state & SS_NOFDREF)
		in_pcbdetach(inp);
	return;
}

/*
 * Walk PCB hash table and count various sockets and TCP/IP specific
 * ones and return the various counts
 */
void
in_pcb_sockstats(struct sockstat *stp)
{
	register struct inpcb *ip, *ipnxt;
	register struct tcpcb *tp;
	register struct socket *so;
	int ehash, hash, ix;
	struct in_pcbhead *hinp;

	bzero (stp, sizeof(struct sockstat));
	ehash = tcb.inp_tablesz - 1;

	for (hash = 0; hash <= ehash; hash++) {
resync:
		hinp = &tcb.inp_table[hash];
		INHHEAD_LOCK(hinp);
		ip = hinp->hinp_next;

		if (ip == (struct inpcb *)hinp) { /* get next bucket */
			INHHEAD_UNLOCK(hinp);
			continue;
		}
		INPCB_HOLD(ip);
		for (; (ip != (struct inpcb *)hinp) && (ip->inp_hhead == hinp);
			ip = ipnxt) {

			so = ip->inp_socket;
			ipnxt = ip->inp_next;
			INPCB_HOLD(ipnxt);

			INHHEAD_UNLOCK(hinp);
			SOCKET_LOCK(so);

			if (so->so_type < TCPSTAT_MAX_SOCKTYPES) {
				stp->open_sock[so->so_type]++;

				if (so->so_type == SOCK_STREAM) {
					tp = intotcpcb(ip);
					if (tp && (tp->t_state <
						   TCPSTAT_MAX_TCPSTATES)) {
					   ix = (so->so_options & SS_TPISOCKET)
					     ? TCPSTAT_TPISOCKET : tp->t_state;
					   stp->tcp_sock[ix]++;
					}
				}
			} else { /* bogus value so count as illegal */
				stp->open_sock[0]++;
			}
			if (!INPCB_RELE(ip)) {
				SOCKET_UNLOCK(so);
			}

			INHHEAD_LOCK(hinp);
			if (ipnxt->inp_next == 0) {
				/*
				 * 'ipnx->inp_next' was removed from the
				 * list while we had it unlocked so check
				 * this case for lock consistency.
				 */
				so = ipnxt->inp_socket;
				INHHEAD_UNLOCK(hinp);

				SOCKET_LOCK(so);
				if (!INPCB_RELE(ipnxt)) {
					SOCKET_UNLOCK(so);
				}
				hash++; 
				if (hash <= ehash) {
					goto resync;
				}
				return;
			}
		} /* end of inner for loop */

		INHHEAD_UNLOCK(hinp);
		if (ip != (struct inpcb *)hinp) {
			so = ip->inp_socket;
			SOCKET_LOCK(so);
			if (!INPCB_RELE(ip)) {
				SOCKET_UNLOCK(so);
			}
		}
	} /* end of outer for loop */
	return;
}

/*
 * Removes a PCB from the active list, clearing various bits of cached state.
 * Normally called from in_pcbdetach(), but may be called directly by TCP to
 * ensure that segments do not arrive for connections in the middle of being
 * torn down.
 */
void
in_pcbunlink(struct inpcb *inp)
{
#ifdef DEBUG
	struct inpcb *head = inp->inp_head;
#endif

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));

	if (inp->inp_hashval == BUCKET_INVAL)
		return;

	INHHEAD_LOCK(inp->inp_hhead);
	if (inp->inp_prev == 0) {
		/*
		 * Unlink was already done, so no need to do it again.
		 */
	        INHHEAD_UNLOCK(inp->inp_hhead);
		return;
	}

	INPCB_DSTATDEC(head, inp->inp_hashval);
	remque(inp);

	inp->inp_hashval = BUCKET_INVAL;
	inp->inp_next = inp->inp_prev = 0;

	INHHEAD_UNLOCK(inp->inp_hhead);
	INPCB_STATDEC(head, inpcb_curpcbs);

	_IN_PCB_TRACE(inp, 6);
	PCB_UTRACE(UTN('pcbu','nlnk'), inp, __return_address);
	return;
}

int
in_pcbdetach(struct inpcb *inp)
{
	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));

	/*
	 * Take this PCB off the list of active PCBs, if necessary.
	 */
	_IN_PCB_TRACE(inp, 5);
	PCB_UTRACE(UTN('pcbd','etac'), inp, __return_address);
	in_pcbunlink(inp);

	if (inp->inp_socket->so_state & SS_CLOSED) {
		return 0;
	}
	inp->inp_socket->so_state |= SS_CLOSED;
#ifdef _PCB_DEBUG
	inp->inp_socket->so_callout_arg = __return_address;
#endif
	return(INPCB_RELE(inp));
}

/*
 * Decrement the ref count of the PCB and free if last reference
 */
int
in_pcbrele(struct inpcb *inp)
{
	int rv;

 	if (atomicAddInt(&inp->inp_refcnt, -1)) {
		_IN_PCB_TRACE(inp, 1);
		PCB_UTRACE(UTN('pcbr','ele0'), inp, __return_address);
		return (0);
	}
	rv = 0;
#ifdef _PCB_DEBUG
	if (!mutex_mine(&inp->inp_socket->so_sem)) {
		cmn_err_tag(279,CE_PANIC,
		"in_pcbrele: socket 0x%x (inp 0x%x) not locked", 
		inp->inp_socket, inp);
	}
#endif
	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	ASSERT(inp->inp_next == 0 && inp->inp_prev == 0);
	ASSERT(inp->inp_hashval == BUCKET_INVAL);

	inp->inp_socket->so_pcb = 0;
#ifdef _PCB_DEBUG
	inp->inp_socket->so_callout_arg = __return_address;
#ifdef __SO_LOCK_DEBUG
	SO_TRACE(inp->inp_socket, 8);
#endif
#endif
	if (sofree(inp->inp_socket)) {
#ifdef _PCB_DEBUG
		inp->inp_socket->so_callout = (int (*)(void))inp;
		_IN_PCB_TRACE(inp, 3);
#endif
		PCB_UTRACE(UTN('pcbr','ele1'), inp, __return_address);
		inp->inp_socket = 0;
		rv = 1;
	}
	if (inp->inp_options)
		(void)m_free(inp->inp_options);
#ifdef _PCB_DEBUG
	inp->inp_options = __return_address;
#endif
	if (inp->inp_route.ro_rt)
		rtfree_needlock(inp->inp_route.ro_rt);
#ifdef INET6
	if (inp->inp_moptions) {
		if (inp->inp_flags & INP_COMPATV4)
			ip_freemoptions(inp->inp_moptions);
		else
			ip6_freemoptions(inp->inp_moptions);
	}
	if (inp->inp_soptions.lh_first)
		ip6_freesoptions(inp);
#else
	ip_freemoptions(inp->inp_moptions);
#endif
	PCB_UTRACE(UTN('pcbr','ele2'), inp, __return_address);
#ifdef _PCB_DEBUG
	_in_pcb_fill(inp, __return_address);
#endif
	(void) kmem_zone_free(inpcb_zone, inp);
	m_pcbdec(sizeof (struct inpcb), MT_PCB);
	return(rv);
}

void
in_setsockaddr(register struct inpcb *inp, struct mbuf *nam)
{
	register struct sockaddr_in *sin;
#ifdef INET6
	register struct sockaddr_in6 *sin6;
#endif

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));	
#ifdef INET6
	if (sotopf(inp->inp_socket) == AF_INET6) {
		nam->m_len = sizeof (*sin6);
		sin6 = mtod(nam, struct sockaddr_in6 *);
		bzero((caddr_t)sin6, sizeof (*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_port = inp->inp_lport;
		sin6->sin6_flowinfo = inp->inp_iflowinfo;
		COPY_ADDR6(inp->inp_laddr6, sin6->sin6_addr);
	} else {
#endif
		nam->m_len = sizeof (*sin);
		sin = mtod(nam, struct sockaddr_in *);
		bzero((caddr_t)sin, sizeof (*sin));
		sin->sin_family = AF_INET;
		sin->sin_port = inp->inp_lport;
		sin->sin_addr = inp->inp_laddr;
#ifdef INET6
	}
#endif
}

void
in_setpeeraddr(struct inpcb *inp, struct mbuf *nam)
{
	register struct sockaddr_in *sin;
#ifdef INET6
	register struct sockaddr_in6 *sin6;
#endif

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));	
#ifdef INET6
	if (sotopf(inp->inp_socket) == AF_INET6) {
		nam->m_len = sizeof (*sin6);
		sin6 = mtod(nam, struct sockaddr_in6 *);
		bzero((caddr_t)sin6, sizeof (*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_port = inp->inp_fport;
		sin6->sin6_flowinfo = inp->inp_iflowinfo;
		COPY_ADDR6(inp->inp_faddr6, sin6->sin6_addr);
	} else {
#endif
		nam->m_len = sizeof (*sin);
		sin = mtod(nam, struct sockaddr_in *);
		bzero((caddr_t)sin, sizeof (*sin));
		sin->sin_family = AF_INET;
		sin->sin_port = inp->inp_fport;
		sin->sin_addr = inp->inp_faddr;
#ifdef INET6
	}
#endif
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Must be called at splnet.
 */

void
in_pcbnotify(
	struct inpcb *head,
	struct sockaddr *dst,
	u_short fport,
	struct in_addr laddr,
	u_short lport,
	int cmd,
	void (*notify)(struct inpcb *, int, void *),
	void *data)
{
	register struct inpcb *inp, *inpnxt;
	struct in_addr faddr;
	int errno;
	void in_rtchange(struct inpcb *, int, void *);
	int hash;
	struct in_pcbhead *hinp;
	int havelock = 0;
	int start = 1, end = head->inp_tablesz;

	if ((unsigned)cmd > PRC_NCMDS || dst->sa_family != AF_INET)
		return;
	faddr = ((struct sockaddr_in *)dst)->sin_addr;
	if (faddr.s_addr == INADDR_ANY)
		return;

	/*
	 * Redirects go to all references to the destination,
	 * and use in_rtchange to invalidate the route cache.
	 * Dead host indications: notify all references to the destination.
	 * Otherwise, if we have knowledge of the local port and address,
	 * deliver only to that socket.
	 */
	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
		fport = 0;
		lport = 0;
		laddr.s_addr = 0;
		if (cmd != PRC_HOSTDEAD)
			notify = in_rtchange;
	}

	/*
	 * See if we can optimize the delivery of this message; if we have
	 * the required values for the hash function, we can just scan one
	 * bucket.
	 */
	if (head->inp_hashflags & INPFLAGS_CLTS) {
		if (lport) {
			start = (*head->inp_hashfun)(head, 
#ifdef INET6
				&faddr, fport, &laddr, lport, AF_INET);
#else
				faddr, fport, laddr, lport);
#endif
			end = start + 1;
		}
	} else {
		/* This ignores TIME-WAITers for TCP, but who cares? */
		if (faddr.s_addr && laddr.s_addr && lport && fport) {
			start = (*head->inp_hashfun)(head, 
#ifdef INET6
				&faddr, fport, &laddr, lport, AF_INET);
#else
				faddr, fport, laddr, lport);
#endif
			end = start + 1;
		}
	}
	errno = inetctlerrmap[cmd];
	for (hash = start; hash < end; hash++) {
resync:
	havelock = 1;
	hinp = &head->inp_table[hash];
	INHHEAD_LOCK(hinp);
	inp = hinp->hinp_next;
	if (inp != (struct inpcb *)hinp) {
	INPCB_HOLD(inp);
	for (; (inp != (struct inpcb *)hinp) &&
	       (inp->inp_hhead == hinp); inp = inpnxt) {
	        ASSERT(INHHEAD_ISLOCKED(hinp));
		inpnxt = inp->inp_next;
		ASSERT(inpnxt != inp);
		INPCB_HOLD(inpnxt);
#ifdef INET6
		if ((inp->inp_flags & INP_COMPATV4) == 0 ||
		    (inp->inp_fatype & IPATYPE_IPV4) == 0 ||
#else
		if (
#endif
		    inp->inp_faddr.s_addr != faddr.s_addr ||
		    inp->inp_socket == 0 ||
		    (lport && inp->inp_lport != lport) ||
#ifdef INET6
		    (laddr.s_addr && ((inp->inp_latype & IPATYPE_IPV4) == 0 ||
		    inp->inp_laddr.s_addr != laddr.s_addr)) ||
#else
		    (laddr.s_addr && inp->inp_laddr.s_addr != laddr.s_addr) ||
#endif
		    (fport && inp->inp_fport != fport)) {
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
		if (notify)
			(*notify)(inp, errno, data);
		if (!INPCB_RELE(inp))
			SOCKET_UNLOCK(inp->inp_socket);
		INHHEAD_LOCK(hinp);
		if (inpnxt->inp_next == 0) {
			struct socket *so2 = inpnxt->inp_socket;
			INHHEAD_UNLOCK(hinp);
			SOCKET_LOCK(so2);
			if (!INPCB_RELE(inpnxt))
				SOCKET_UNLOCK(so2);
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
	} /* if */
	if (havelock) {
		INHHEAD_UNLOCK(hinp);
	}
	}	/* end for */
	return;
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in_losing(struct inpcb *inp)
{
	register struct rtentry *rt;
	struct rt_addrinfo info;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	if ((rt = inp->inp_route.ro_rt)) {
		ROUTE_WRLOCK();
		inp->inp_route.ro_rt = 0;
		bzero((caddr_t)&info, sizeof(info));
		info.rti_info[RTAX_DST] =
			(struct sockaddr *)&inp->inp_route.ro_dst;
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
#ifdef _HAVE_SIN_LEN
		info.rti_info[RTAX_NETMASK] = rt_mask(rt);
#else
		info.rti_info[RTAX_NETMASK] = (struct sockaddr*)rt_mask(rt);
#endif
		rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);
		if (rt->rt_flags & RTF_DYNAMIC)
			(void) rtrequest(RTM_DELETE, rt_key(rt),
				rt->rt_gateway, rt_mask(rt), rt->rt_flags, 
				(struct rtentry **)0);
		else  {
			/*
			 * A new route can be allocated
			 * the next time output is attempted.
			 */
			rtfree(rt);
		}
		ROUTE_UNLOCK();
	}
	return;
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
/*ARGSUSED*/
void
in_rtchange(register struct inpcb *inp, int flag, void *data)
{

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	if (inp->inp_route.ro_rt) {
		rtfree_needlock(inp->inp_route.ro_rt);
		inp->inp_route.ro_rt = 0;
		/*
		 * A new route can be allocated the next time
		 * output is attempted.
		 */
	}
	return;
}

struct inpcb *
in_pcblookupx(struct inpcb *head,
#ifdef INET6
	void *faddr_arg, u_short fport,
	void *laddr_arg, u_short lport,
	int flags, int family)
#else
	struct in_addr faddr, u_short fport,
	struct in_addr laddr, u_short lport,
	int flags)
#endif
{
	register struct inpcb *inp = 0, *curmatch = 0, *lastmatch = 0,
		*match = 0;
	struct in_pcbhead *hinp;
	int matchwild = 3, wildcard;
	int all = (flags & INPLOOKUP_ALL);
	u_short hashval, smhashval, start, end;
#ifdef INET6
	int compat;
	struct in6_addr t_faddr, *faddr;
	struct in6_addr t_laddr, *laddr;

/*
 * Note, in the merged code we still need to check inp_flags against compat
 * even though we convert the v4 addr into a v6 addr.  This is because
 * we can not send up v6 addresses to applications that opened AF_INET
 * sockets and called recvfrom().  They won't know what to do with the addr.
 * So, even though laddr may match inp_laddr we still need to check
 * inp_flags to see if the socket is of the expected type.
 *
 * There is an exception to the above case.  If the sender is using
 * a v4 compatibility address for the src and dst addr of the packet then
 * we could pass the last 4 bytes of that addr up to the
 * caller of recvfrom().  However, the v4 mapped addrs are used for
 * communicating with v4 only hosts so why would anybody do that?  If the
 * host only understands v4 the packet will be rejected even if the src
 * and dst addrs are v4 mapped.
 */
	if (family == AF_INET) {
		compat = INP_COMPATV4;
		t_faddr.s6_addr32[0] = 0;
		t_faddr.s6_addr32[1] = 0;
		if (((struct in_addr *)faddr_arg)->s_addr == INADDR_ANY)
			t_faddr.s6_addr32[2] = 0;
		else
			t_faddr.s6_addr32[2] = htonl(0xffff);
		t_faddr.s6_addr32[3] = ((struct in_addr *)faddr_arg)->s_addr;
		faddr = &t_faddr;

		t_laddr.s6_addr32[0] = 0;
		t_laddr.s6_addr32[1] = 0;
		if (((struct in_addr *)laddr_arg)->s_addr == INADDR_ANY)
			t_faddr.s6_addr32[2] = 0;
		else
			t_laddr.s6_addr32[2] = htonl(0xffff);
		t_laddr.s6_addr32[3] = ((struct in_addr *)laddr_arg)->s_addr;
		laddr = &t_laddr;
	} else {
		compat = INP_COMPATV6;
		faddr = (struct in6_addr *)faddr_arg;
		laddr = (struct in6_addr *)laddr_arg;
	}
#endif /* INET6 */

	flags &= ~INPLOOKUP_ALL;

	INPCB_STATINC(head, inpcb_lookup);

#ifdef INET6
	hashval = (*head->inp_hashfun)(head, faddr_arg, fport, laddr_arg,
	  lport, family);
#else
	hashval = (*head->inp_hashfun)(head, faddr, fport, laddr, lport);
#endif

	/*
	 * If we are CLTS, we jump over the exact match and cache
	 * code and go directly to the wildcard setup.
	 *
	 * The idea here is that we've set up to hash by local port number
	 * only (see in_pcbcltshashfun, below).  This means that we only
	 * need to check the hash bucket corresponding to the local port
	 * number instead of the entire hash.  We still need to look for
	 * wildcard matches so we can't really take advantage of the cache
	 * since we may have a "better" match.  The point of hashing
	 * by local port numbers is that "most" clts inpcbs are going
	 * to be wildcarded.  We try to minimize the amount of the total
	 * inpcb list that we have to search while ensuring "completeness"
	 * in the search.
	 */
	if (head->inp_hashflags & INPFLAGS_CLTS) {
		start = hashval;
		end = hashval + 1;

		INPCB_STATINC(head, inpcb_wildcard);

		goto lookwild;
	} else {
		if (flags & INPLOOKUP_LISTEN) { /* listen/wildcard lookup */

			INPCB_STATINC(head, inpcb_listen);

			/* only search first bucket */
			start = 0;
			end = 1;
			goto lookwild;
		} else if (flags & INPLOOKUP_WILDCARD) { /* wildcard lookup */

			INPCB_STATINC(head, inpcb_wildcard);

		} else if (flags & INPLOOKUP_BIND) {
			start = 0;
			end = (head->inp_tablesz - SPECIAL_BUCKETS) / 2;
			goto lookwild;
		} else { /* normal lookup */
			INPCB_STATINC(head, inpcb_regular);
		}
	}

again:
	INHHEAD_LOCK(&head->inp_table[hashval]);
	/*
	 * Compute the hash function and search the
	 * hash chain associated with this bucket for a match.
	 */
	hinp = &head->inp_table[hashval];
	for (inp = hinp->hinp_next; inp != (struct inpcb *)hinp; 
		inp = inp->inp_next) {
                ASSERT(inp->inp_next != inp);
#ifdef INET6
		if ((inp->inp_flags & compat) == 0)
			continue;
		if (!IP6_ADDRS_EQUAL(inp, *laddr, lport, *faddr, fport))
#else
		if (!IP_ADDRS_EQUAL(inp, laddr, lport, faddr, fport))
#endif
			continue;
		INPCB_HOLD(inp);
		INPCB_STATINC(head, inpcb_found_chain);
		goto foundit;
	}

	inp = 0;
foundit:
	if (inp || !(flags & INPLOOKUP_WILDCARD)) {
		if (inp) { /* success */

			INPCB_STATINC(head, inpcb_success);

		} else { /* failed*/
			if (all) {
				all = 0;
                                INHHEAD_UNLOCK(&head->inp_table[hashval]);
				hashval += 
				 ((head->inp_tablesz - SPECIAL_BUCKETS) / 2);
				goto again;
			}

			INPCB_STATINC(head, inpcb_failed);

		}
                INHHEAD_UNLOCK(&head->inp_table[hashval]);
		return inp;
	}

        INHHEAD_UNLOCK(&head->inp_table[hashval]);
	
	/*
	 * Arrive here to search the entire hash table (all buckets)
	 * when a wildcard match is required.  This is used for COTS only.
	 */
	start = 0;
	end = (head->inp_tablesz - SPECIAL_BUCKETS) / 2;

lookwild:
#ifdef DEBUG
	if (!(flags & INPLOOKUP_LISTEN)) {
		INPCB_STATINC(head, inpcb_searched_all);
	}
#endif
lookwild2:
	/*
	 * Wildcard lookup, we search all the hash
	 * buckets and associated chains.
	 */
	for (hashval = start, hinp = &head->inp_table[start];
			hashval < end;
			hashval++, hinp++) {
	    for (; hashval < head->inp_tablesz && (hashval < end) &&
		hinp == (struct in_pcbhead *)hinp->hinp_next;
				hashval++, hinp++) {
	    }
	    if ((hashval >= head->inp_tablesz) || (hashval >= end)) {
		continue;
	    }
            INHHEAD_LOCK(hinp);
            for (inp = hinp->hinp_next; inp != (struct inpcb *)hinp; 
		inp = inp->inp_next) {
                ASSERT(inp->inp_next != inp);
		if ((flags & INPLOOKUP_LISTEN) && 
		    !(inp->inp_hashflags & INPFLAGS_LISTEN)) {
			continue;
		}
	    	if (inp->inp_lport != lport)
			continue;
#ifdef INET6
		if ((inp->inp_flags & compat) == 0)
			continue;
#endif
		wildcard = 0;
#ifdef INET6
		if (inp->inp_latype != IPATYPE_UNBD) {
		    	if (IS_ANYADDR6(*laddr))
				wildcard++;
		    	else if (!SAME_ADDR6(inp->inp_laddr6, *laddr))
			    	continue;
		} else {
		    	if (!IS_ANYADDR6(*laddr))
				wildcard++;
		}
		if (inp->inp_fatype != IPATYPE_UNBD) {
		    	if (IS_ANYADDR6(*faddr))
				wildcard++;
		    	else if (!SAME_ADDR6(inp->inp_faddr6, *faddr) ||
			  inp->inp_fport != fport)
				continue;
		} else {
		    	if (!IS_ANYADDR6(*faddr))
				wildcard++;
		}
#else
		if (inp->inp_laddr.s_addr != INADDR_ANY) {
		    	if (laddr.s_addr == INADDR_ANY)
				wildcard++;
		    	else if (inp->inp_laddr.s_addr != laddr.s_addr)
			    	continue;
		} else {
		    	if (laddr.s_addr != INADDR_ANY)
				wildcard++;
		}
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
		    	if (faddr.s_addr == INADDR_ANY)
				wildcard++;
		    	else if (inp->inp_faddr.s_addr != faddr.s_addr ||
			     	inp->inp_fport != fport)
				continue;
		} else {
		    	if (faddr.s_addr != INADDR_ANY)
				wildcard++;
		}
#endif
		if (wildcard && (flags & INPLOOKUP_WILDCARD) == 0)
		    	continue;
		if (wildcard < matchwild) {
                        curmatch = inp;
			smhashval = hashval;
		    	matchwild = wildcard;
		    	if (matchwild == 0)
				break;
		}
	    }

	    if (curmatch) {
		/*
		 * Hold the current match.  If we find a better one in another
		 * bucket, we'll release this one and use the new one.  If not,
		 * this is our best option right now.
		 */
		INPCB_HOLD(curmatch);
	    }
	    INHHEAD_UNLOCK(hinp);	/* drop lock to avoid deadlock */
	    if (curmatch) {
	    	if (lastmatch) {
			/*
			 * New match is better than old, or we wouldn't have
			 * one.  Release old PCB and update lastmatch with the
			 * current matching PCB.  Clear current match too.
			 */
			struct socket *so2 = lastmatch->inp_socket;
			SOCKET_LOCK(so2);
			if (!INPCB_RELE(lastmatch))
				SOCKET_UNLOCK(so2);
	    	}
		lastmatch = curmatch;	/* curmatch held already */
		curmatch = (struct inpcb *)0;
	    }
	    if (!matchwild) {
		break; /* out of for loop */
	    }
	    continue;	/* top of for loop */
	}

	/*
	 * Come out of for loop with no locks held.
	 * We shouldn't have both a current match and a last match, so if
	 * we have a current match, use it, otherwise take the last one.
	 */
	match = curmatch ? curmatch : lastmatch;
	if (!match && all) {
		all = 0;
		start = end;
		end = head->inp_tablesz;
		goto lookwild2;
	}

#ifdef DEBUG
	if (match) { /* success */
		INPCB_STATINC(head, inpcb_success);
	} else { /* failed */
		INPCB_STATINC(head, inpcb_failed);
	}
#endif
	return (match);
}

/*
 * This function returns a "hint" indicating what the next lport
 * value might be by incrementing through the list of possible
 * port numbers.
 *
 * The caller should verify the port during a bind operation by looking
 * up the completed inp address using a locking form of in_pcblookupx.
 */
u_short
in_pcbnextport(struct inpcb *inp)
{
	struct inpcb *head = inp->inp_head;
	u_short lport;

	/*
	 * This used to wrap at IPPORT_USERRESERVED (5000)
	 * but that would cause an infinite loop with 4000
	 * anonymous ports. The limit needs to be MAXPORT/2
	 * in order to work around a bug in the boot PROMs:
	 * they do not handle ports with the sign bit set.
	 */
	/* avoid lock on head via atomic op */
	lport = (u_short)atomicIncPort(&head->inp_wildport, 
		IPPORT_MAXPORT / 2, IPPORT_RESERVED);
	lport = htons(lport);
	return lport;
}

/*
 * NOTE:
 * These hash functions are based on the fact that the number of
 * hash buckets will be (a power of 2) + 1.
 *
 * Check out the procedure in_pcbinitcb()
 * This is the TCP hash function.
 * 
 * When the connection isn't fully specified, hash it to bucket 0.
 */
u_short
in_pcbhashfun(struct inpcb *head,
#ifdef INET6
		void *faddr, u_short fport,
		void *laddr, u_short lport, int type)
#else
		struct in_addr faddr, u_short fport,
		struct in_addr laddr, u_short lport)
#endif
{
	__uint32_t value;
	
#ifdef INET6
	if (type == AF_INET) {
		value = ((struct in_addr *)faddr)->s_addr + fport +
		  ((struct in_addr *)laddr)->s_addr + lport;
	} else {
		/*
		 * Most IPv6 addresses will follow the Aggregatable
		 * Global Unicast Format which uses the lower 64 bits
		 * (words 2 and 3 of s6_addr32) as a globally unique
		 * interface ID.  We calculate the hash value only using
		 * that interface ID since that should be good enough
		 * to get a good distribution across the hash buckets.
		 */
		value = ((struct in6_addr *)faddr)->s6_addr32[2] +
		  ((struct in6_addr *)faddr)->s6_addr32[3] + fport +
		  ((struct in6_addr *)laddr)->s6_addr32[2] +
		  ((struct in6_addr *)laddr)->s6_addr32[3] + lport;
	}
#else
        if (fport == 0 && faddr.s_addr == 0)
	  return BUCKET_0;
	value = faddr.s_addr + fport + laddr.s_addr + lport;
#endif
	value &= (((head->inp_tablesz - SPECIAL_BUCKETS) / 2) - 1);
	value += LEADING_BUCKETS;

	ASSERT(value <= head->inp_tablesz);

	return (u_short)value;
}

/*
 * This is the connectionless (UDP) hash function.
 * NOTE: We "never" hash into bucket 0.
 */
/* ARGSUSED */
u_short
in_pcbcltshashfun(struct inpcb *head,
#ifdef INET6
		void *faddr, u_short fport,
		void *laddr, u_short lport, int type)
#else
		struct in_addr faddr, u_short fport,
		struct in_addr laddr, u_short lport)
#endif
{
	__uint32_t value;

	if (lport == 0) {
		value = 0;	
	} else {
		value = lport;
		value &= ((head->inp_tablesz - SPECIAL_BUCKETS) - 1);
		value += LEADING_BUCKETS;
	}
	return ((u_short)value);
}

/*
 * Support for listening TCP endpoints. This is actually only
 * called from tcp_usrreq: PRU_LISTEN. The only reason it is
 * here is that logically, only this file knows the internals
 * of inpcb data structure. Make it inline.
 */
void 
in_pcblisten(struct inpcb *inp)
{
	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	ASSERT(inp->inp_hashval == 0);
	inp->inp_hashflags = INPFLAGS_LISTEN;
}

void
in_pcbunlisten(struct inpcb *inp)
{
	inp->inp_hashflags &= ~INPFLAGS_LISTEN;
	/*
	 * The PCB will get rehashed when in_pcbconnect() is called
	 */
	return;
}

/*
 * Move a PCB in a hash table -- COTS only.
 */
void
in_pcbmove(struct inpcb *inp)
{
	struct inpcb *head;
	struct in_pcbhead *ohead;

	ASSERT(inp);
	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	head = inp->inp_head;
	ASSERT(head);
	ohead = inp->inp_hhead;
	INHHEAD_LOCK(inp->inp_hhead);
	remque(inp);
	INPCB_DSTATDEC(head, inp->inp_hashval);

	ASSERT(inp->inp_hashval != BUCKET_INVAL);
	inp->inp_hashval += (head->inp_tablesz - SPECIAL_BUCKETS) / 2;

	ASSERT(inp->inp_lport != 0);
	INHHEAD_LOCK(&head->inp_table[inp->inp_hashval]);
	inp->inp_hashflags |= INPFLAGS_MOVED;
	inp->inp_hhead = &head->inp_table[inp->inp_hashval];
	insque(inp, _TBLELEMTAIL(inp));
	ASSERT(inp->inp_next != inp);
	ASSERT(inp->inp_prev != inp);
	INHHEAD_UNLOCK(inp->inp_hhead);
	INHHEAD_UNLOCK(ohead);

	INPCB_DSTATINC(head, inp->inp_hashval);
	return;
}

#ifdef INET6
void
in_pcbrehash(struct inpcb *head, struct inpcb *inp)
{
	u_short nhashval;
	u_short ohashval = inp->inp_hashval;
	struct inpcb *tinp;

	nhashval = (*head->inp_hashfun)(inp->inp_head, &inp->inp_faddr6,
	  inp->inp_fport, &inp->inp_laddr6, inp->inp_lport, AF_INET6);
	if (ohashval == nhashval) 
		return;
	if (ohashval != BUCKET_INVAL) {
		remque(inp);
		INHHEAD_UNLOCK(inp->inp_hhead);
	}
	INHHEAD_LOCK(&head->inp_table[nhashval]);
	inp->inp_hashval = nhashval;
	inp->inp_hhead = &head->inp_table[inp->inp_hashval];
	tinp = _TBLELEMTAIL(inp);
	insque(inp, tinp);
}
#endif

/*
 * These routines live in this file so that internal details of PCB mgmt. are
 * not exported to TCP.
 */
/*
 * A TCP-specific version of in_pcbsetaddrx() that fills in both local and
 * remote addresses/ports at the same time, which makes outgoing connections
 * more efficient.
 */
int
tcp_pcbsetaddrx(register struct inpcb *inp,
	struct sockaddr_in *sin,
	struct in_addr laddr)
{
	register struct in_ifaddr *ia, *ia_xx;
	register struct sockaddr_in *ifaddr;
	int error = 0;
	u_short fport;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);

	if (in_ifaddr_count > 0) {

		if (ia_xx = (struct in_ifaddr *)(ifnet->in_ifaddr)) {
			/*
			 * If the destination address is INADDR_ANY,
			 * use the first interface's primary local address.
			 * If the supplied address is INADDR_BROADCAST,
			 * fail; can't do that with TCP.
			 */
			if (sin->sin_addr.s_addr == INADDR_ANY) {
				    sin->sin_addr = ia_xx->ia_addr.sin_addr;
			} else if (sin->sin_addr.s_addr ==
				     (u_long)INADDR_BROADCAST) {
				return EADDRNOTAVAIL;
			}
		}
  	}
#ifdef INET6
	if (inp->inp_latype == IPATYPE_UNBD) {
#else
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
#endif
		register struct route *ro;

		ia = (struct in_ifaddr *)0;
		/* 
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */
		ROUTE_RDLOCK();

		ro = &inp->inp_route;
		if (ro->ro_rt &&
		    (SATOSIN(&ro->ro_dst)->sin_addr.s_addr !=
			sin->sin_addr.s_addr || 
		    inp->inp_socket->so_options & SO_DONTROUTE)) {
			rtfree_needpromote(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}
		if ((inp->inp_socket->so_options & SO_DONTROUTE)== 0 && /*XXX*/
		    (ro->ro_rt == (struct rtentry *)0 ||
		    ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
			/*
			 * No route yet, so try to acquire one
			 */
			ro->ro_dst.sa_family = AF_INET;
#ifdef _HAVE_SA_LEN
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
#endif
			((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
				sin->sin_addr;
			rtalloc(ro);
		}
		/*
		 * If we found a route, use the address corresponding to the
		 * outgoing interface unless it is the loopback (in case a
		 * route to our address on another net goes to loopback).
		 */
		if (ro->ro_rt && (ro->ro_rt->rt_srcifa ||
			!(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))) {

			ia = ro->ro_rt->rt_srcifa ?
				ifatoia(ro->ro_rt->rt_srcifa) :
				ifatoia(ro->ro_rt->rt_ifa);

			if (ia && (ia->ia_addrflags & IADDR_ALIAS)) {
				/*
				 * If the address is an alias AND has the
				 * same network interface as the routing
				 * entry we use the primary address of the
				 * network interface instead.
				 */
				if (ia->ia_ifp == ro->ro_rt->rt_ifp) {
					ia = (struct in_ifaddr *)
						ia->ia_ifp->in_ifaddr;
					if (ia == 0) {
						goto findit;
					}
				}
				/* fast exit case */
				ROUTE_UNLOCK();
				goto gotit;
			}
		}
findit:
		ROUTE_UNLOCK();

		if (ia == 0) {
			fport = sin->sin_port;
			sin->sin_port = 0;

			/* check if address associated with ppp interface */
			if (ia = ifatoia(ifa_ifwithdstaddr(SINTOSA(sin)))) {
				/* restore port number */
				sin->sin_port = fport;
				goto gotit;
			}
			/*
			 * Now do the more expensive check to find the
			 * best matching network number of all of the
			 * addresses we know about.
			 * NOTE: With huge numbers of IP aliases this is a
			 * very expensive operation, to be avoided if possible.
			 */
			if (ia = ifatoia(ifa_ifwithnet(SINTOSA(sin)))) {
				/* restore port number */
				sin->sin_port = fport;
				goto gotit;
			}

			/* restore port number in case we fast exit */
			sin->sin_port = fport;

			if (in_interfaces > 0) { /* use primary if not any */
				ia = (struct in_ifaddr *)(ifnet->in_ifaddr);
				error = (ia) ? 0 : EADDRNOTAVAIL;
			} else {
				error = EADDRNOTAVAIL;
			}
			if (error) { /* failed so quit */
				return error;
			}
		}
gotit:
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			/* can't do that with TCP */
			return EADDRNOTAVAIL;
		}
		ifaddr = (struct sockaddr_in *)&ia->ia_addr;
	}
	laddr = laddr.s_addr ? inp->inp_laddr : ifaddr->sin_addr;

	/*
	 * Get a local port and check for conflicts.
	 * Technically, this violates BSD semantics by potentially assigning
	 * a local port that may already be in use by another service, but
	 * that should theoretically be safe since we avoid a conflict with
	 * any fully specified connections.  What this would introduce is the
	 * problem that somebody would not be able to restart their application
	 * because we are now sitting on their port.  If the app uses
	 * SO_REUSEADDR, this is no problem, but otherwise we will prevent them
	 * from rebinding without killing us.
	 * So, we mark ourself as using SO_REUSEADDR, which should prevent
	 * this.
	 */
	error = tcp_full_assign(inp, sin->sin_addr, sin->sin_port, laddr);
	if (error == 0) {
		inp->inp_socket->so_options |= SO_REUSEADDR;
	}
	return error;
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument 'sin'.
 * If we don't have a local address for this socket yet, then pick one.
 */
int
tcp_pcbconnect(register struct inpcb *inp, struct mbuf *nam)
{
	register struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

	if (nam->m_len != sizeof (*sin))
		return (EINVAL);
	return (tcp_pcbsetaddrx(inp, sin, inp->inp_laddr));
}

/*
 * This is a stripped-down version of in_pcblookupx() that simply checks if
 * either the remote or local address+port match the address+port passed in.
 */
struct inpcb *
in_pcbfusermatch(struct inpcb *head,
#ifdef INET6
	void *addr_arg, u_short port,
	int family)
#else
	struct in_addr addr, u_short port)
#endif
{
	register struct inpcb *inp = 0;
	struct in_pcbhead *hinp;
	u_short hashval, start, end;
#ifdef INET6
	int compat;
	struct in6_addr t_addr, *addr;

/*
 * Note, in the merged code we still need to check inp_flags against compat
 * even though we convert the v4 addr into a v6 addr.  This is because
 * we can not send up v6 addresses to applications that opened AF_INET
 * sockets and called recvfrom().  They won't know what to do with the addr.
 * So, even though laddr may match inp_laddr we still need to check
 * inp_flags to see if the socket is of the expected type.
 *
 * There is an exception to the above case.  If the sender is using
 * a v4 compatibility address for the src and dst addr of the packet then
 * we could pass the last 4 bytes of that addr up to the
 * caller of recvfrom().  However, the v4 mapped addrs are used for
 * communicating with v4 only hosts so why would anybody do that?  If the
 * host only understands v4 the packet will be rejected even if the src
 * and dst addrs are v4 mapped.
 */
	if (family == AF_INET) {
		compat = INP_COMPATV4;
		t_addr.s6_addr32[0] = 0;
		t_addr.s6_addr32[1] = 0;
		if (((struct in_addr *)addr_arg)->s_addr == INADDR_ANY)
			t_addr.s6_addr32[2] = 0;
		else
			t_addr.s6_addr32[2] = htonl(0xffff);
		t_addr.s6_addr32[3] = ((struct in_addr *)addr_arg)->s_addr;
		addr = &t_addr;
	} else {
		compat = INP_COMPATV6;
		addr = (struct in6_addr *)addr_arg;
	}
#endif /* INET6 */

	start = 0;
	end = (head->inp_tablesz - SPECIAL_BUCKETS);

	/* We search all the hash buckets and associated chains. */
	for (hashval = start, hinp = &head->inp_table[start];
			hashval < end;
			hashval++, hinp++) {
	    for (; hashval < head->inp_tablesz && (hashval < end) &&
		hinp == (struct in_pcbhead *)hinp->hinp_next;
				hashval++, hinp++) {
	    }
	    if ((hashval >= head->inp_tablesz) || (hashval >= end)) {
		continue;
	    }
            INHHEAD_LOCK(hinp);
            for (inp = hinp->hinp_next; inp != (struct inpcb *)hinp; 
		inp = inp->inp_next) {
#ifdef INET6
		if ((inp->inp_flags & compat) == 0)
			continue;
#endif
#ifdef INET6
		if (inp->inp_lport == port) {
			if (inp->inp_latype != IPATYPE_UNBD) {
				if (SAME_ADDR6(inp->inp_laddr6, *addr)) {
					goto match;
				}
			} else if (IS_ANYADDR6(*addr)) {
				goto match;
			}
		}
		if (inp->inp_fport == port) {
			if (inp->inp_fatype != IPATYPE_UNBD) {
				if (SAME_ADDR6(inp->inp_faddr6, *addr)) {
					goto match;
				}
			} else if (IS_ANYADDR6(*addr)) {
match:
				INPCB_HOLD(inp);
				INHHEAD_UNLOCK(hinp);
				return inp;
			}
		}

#else
		if ((inp->inp_laddr.s_addr == addr.s_addr
		    && inp->inp_lport == port)
		    || (inp->inp_faddr.s_addr == addr.s_addr
		    && inp->inp_fport == port)) {
			INPCB_HOLD(inp);
			INHHEAD_UNLOCK(hinp);
			return inp;
		}
#endif
		}
	        INHHEAD_UNLOCK(hinp);
	}
	return (struct inpcb *)0;
}

extern struct  inpcb udb;
extern struct  inpcb tcb;

int
in_vsock_from_addr(vsock_t **vsop, struct fid *fidp)
{
	struct inpcb *head = &tcb, *end = &udb;
	struct inpcb *inpcb = NULL;
	vsock_t		*vso = NULL;
	struct sockaddr_in sin;
	extern void vsocket_hold(struct vsocket *);
	int proto = 0;

	bcopy(fidp->fid_data, &sin, sizeof(sin));

	if (fidp->fid_len != sizeof(sin)) {
		/*
		 * new version of fuser; protocol is in byte following
		 * address
		 */
		if (fidp->fid_len > sizeof(fidp->fid_data)) {
			fidp->fid_len = sizeof(fidp->fid_data) - 1;
		}
		proto = fidp->fid_data[fidp->fid_len - 1];
		if (proto == IPPROTO_UDP) {
			head = &udb;
			end = &udb;
		} else {
			head = &tcb;
			end = &tcb;
		}
	}

	for (; head; head = (head == end ? NULL : end)) {
		/* try address as local */
#ifdef INET6
		inpcb = in_pcbfusermatch(head, &sin.sin_addr, sin.sin_port,
			 AF_INET);
#else
		inpcb = in_pcbfusermatch(head, sin.sin_addr, sin.sin_port);
#endif
		if (inpcb)
			break;
	}
	*vsop = NULL;
	if (inpcb) {
		ASSERT(inpcb->inp_socket);
		SO_UTRACE(UTN('vsoc','add1'), inpcb->inp_socket, inpcb);
		SOCKET_LOCK(inpcb->inp_socket);
		if (!(inpcb->inp_socket->so_state & SS_NOFDREF)) {
			vso = BHV_TO_VSOCK_TRY(&(inpcb->inp_socket->so_bhv));
			if (vso && !(vso->vs_flags & VS_LASTREFWAIT)) {
				VSO_UTRACE(UTN('vsoc','add2'), vso, inpcb);
				vsocket_hold(vso);
				*vsop = vso;
			}
		}
		if (!INPCB_RELE(inpcb))
			SOCKET_UNLOCK(inpcb->inp_socket);
		return *vsop ? 0 : ENOENT;
	}
	return ENOENT;
}
