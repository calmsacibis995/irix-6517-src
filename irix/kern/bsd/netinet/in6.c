#ifdef INET6
/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in.c	8.2 (Berkeley) 11/15/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include "net/soioctl.h"
#include <net/if.h>
#include <net/route.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/ip6_icmp.h>
#include <netinet/if_ether.h>
#include <netinet/if_ndp6.h>

int	in6_interfaces;		/* number of external IPv6 interfaces */
int	in6_ifaddr_count;	/* number configured address'es including lb */

struct in6_anyhead {
        struct in6_anycast *lh_first;  /* first element */
} in6_anyhead;
mutex_t anycast_lock;  /* protects ina6_refcount field and in6_anyhead list */

struct hashinfo hashinfo_in6addr;
struct hashtable hashtable_in6addr[HASHTABLE_SIZE];

extern int in_getalias_enummatch(struct hashbucket *h, caddr_t key,
  caddr_t arg1, caddr_t arg2);

void
in6_init(void)
{
	extern void sitattach(int);

	hash_init(&hashinfo_in6addr, hashtable_in6addr,
	  HASHTABLE_SIZE, sizeof(struct in6_addr), HASH_MASK);
	mutex_init(&anycast_lock, MUTEX_DEFAULT, "anycast");
	sitattach(0); /* set up psuedo interfaces for tunneling */
	return;
}

/*
 * Test if an address is an anycast address, return flags.
 */
int
in6_isanycast(addr)
	struct in6_addr *addr;
{
	register struct in6_anycast *ia;

	if (IS_ANYADDR6(*addr))
		return (0);
	mutex_lock(&anycast_lock, PZERO);
	for (ia = in6_anyhead.lh_first; ia; ia = ia->ina6_list.le_next)
		if (SAME_ADDR6(*addr, ia->ina6_addr)) {
			mutex_unlock(&anycast_lock);
			return ((int)ia->ina6_flags);
		}
	mutex_unlock(&anycast_lock);
	return (0);
}

/*
 * Add an anycast address in the list.
 */
static int
in6_addanycast(struct in6_addr *addr, u_int flags)
{
	register struct in6_anycast *ia;

	mutex_lock(&anycast_lock, PZERO);
	for (ia = in6_anyhead.lh_first; ia; ia = ia->ina6_list.le_next)
		if (SAME_ADDR6(*addr, ia->ina6_addr)) {
			ia->ina6_refcount++;
			ia->ina6_flags = max(flags, IP6ANY_VALID);
			mutex_unlock(&anycast_lock);
			return (0);
		}
	ia = (struct in6_anycast *)kmem_alloc(sizeof *ia, KM_SLEEP);
	if (ia == (struct in6_anycast *)0) {
		mutex_unlock(&anycast_lock);
		return (ENOBUFS);
	}
	bzero((caddr_t)ia, sizeof *ia);

	/* add to list */
	if ((ia->ina6_list.le_next = in6_anyhead.lh_first) != NULL)
		in6_anyhead.lh_first->ina6_list.le_prev =
		  &ia->ina6_list.le_next;
	in6_anyhead.lh_first = ia;
	ia->ina6_list.le_prev = &in6_anyhead.lh_first;

	COPY_ADDR6(*addr, ia->ina6_addr);
	ia->ina6_refcount++;
	ia->ina6_flags = max(flags, IP6ANY_VALID);
	mutex_unlock(&anycast_lock);
	return (0);
}

/*
 * Delete an anycast address in the list.
 */
static int
in6_delanycast(struct in6_addr *addr)
{
	register struct in6_anycast *ia;

	mutex_lock(&anycast_lock, PZERO);
	for (ia = in6_anyhead.lh_first; ia; ia = ia->ina6_list.le_next)
		if (SAME_ADDR6(*addr, ia->ina6_addr))
			break;
	if (ia == (struct in6_anycast *)0) {
		mutex_unlock(&anycast_lock);
		return (EADDRNOTAVAIL);
	}
	ia->ina6_refcount--;
	if (ia->ina6_refcount) {
		mutex_unlock(&anycast_lock);
		return (0);
	}

	/* remove from list */
	if (ia->ina6_list.le_next != NULL)
		ia->ina6_list.le_next->ina6_list.le_prev =
		  ia->ina6_list.le_prev;
	*ia->ina6_list.le_prev = ia->ina6_list.le_next;

	mutex_unlock(&anycast_lock);
	kmem_free(ia, sizeof(*ia));
	return (0);
}

/*
 * Convert IPv6 address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
static int ipv6round = 0;
char *
ip6_sprintf(addr)
        register struct in6_addr *addr;
{
        static char ipv6buf[8][46];
        register int i;
        register char *cp;
        register u_int16_t *a = (u_int16_t *)addr;
        register u_int8_t *d;
        int dcolon = 0;

	ipv6round = (ipv6round + 1) & 7;
	cp = ipv6buf[ipv6round];

        for (i = 0; i < 8; i++) {
                if (dcolon == 1) {
                        if (*a == 0) {
				if (i == 7)
					*cp++ = ':';
                                a++;
                                continue;
                        } else
                                dcolon = 2;
                }
                if (*a == 0) {
                        if (dcolon == 0 && i == 7) {
				*cp++ = '0';
				*cp++ = '0';
				break;
			}
                        if (dcolon == 0 && *(a + 1) == 0) {
				if (i == 0)
	                                *cp++ = ':';
                                *cp++ = ':';
                                dcolon = 1;
                        } else {
                                *cp++ = '0';
                                *cp++ = ':';
                        }
			a++;
                        continue;
                }
                d = (u_int8_t *)a;
                *cp++ = digits[*d >> 4];
                *cp++ = digits[*d++ & 0xf];
                *cp++ = digits[*d >> 4];
                *cp++ = digits[*d & 0xf];
                *cp++ = ':';
                a++;
        }
        *--cp = 0;
        return (ipv6buf[ipv6round]);
}

/* ARGSUSED */
int
in6addr_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	struct in6_ifaddr *ia;
	struct in6_addr *addr;

	if (h->flags & HTF_INET) { /* internet address */
		ia = (struct in6_ifaddr *)h;
		addr = (struct in6_addr *)key;
		if (SAME_ADDR6(IA_SIN6(ia)->sin6_addr, *addr))
			return (1);
	}
	return (0);
}

/*
 * This match procedure is called after the hashing lookup has matched
 * the supplied IP address AND the supplied 'struct ifnet' address
 * matches that which was stored in the entry.
 *
 *inaddr_ifnetaddr_match(struct in6_ifaddr *ia, struct in6_addr *key,
 *	struct ifnet *arg1, caddr_t arg2)
 */
/* ARGSUSED */
int
in6addr_ifnetaddr_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in6_ifaddr *ia;
	struct in6_addr *addr;
	struct ifnet *ifp;

	if (h->flags & HTF_INET) { /* internet address */
		ia = (struct in6_ifaddr *)h;
		addr = (struct in6_addr *)key;
		ifp = (struct ifnet *)arg1;

		if ((ia->ia_ifp == ifp) &&
		  SAME_ADDR6(IA_SIN6(ia)->sin6_addr, *addr))
			return (1);
	}
	return (0);
}

static struct in6_ifaddr *
in6_inifaddr_alloc(struct ifnet *ifp)
{
	struct in6_ifaddr *ia;

	ia = (struct in6_ifaddr *)kmem_alloc(sizeof *ia, KM_SLEEP);
	bzero((caddr_t)ia, sizeof *ia);

	ia->ia_hashbucket.flags = HTF_INET;
	IA_SIN6(ia)->sin6_family = AF_INET6;
	ia->ia_ifp = ifp;

	ia->ia_ifa.ifa_refcnt = 1;
	ia->ia_ifa.ifa_start_inifaddr = (caddr_t)ia;
	ia->ia_ifa.ifa_addr = sin6tosa(&ia->ia_addr);
	/*
	 * ifa_broadaddr (same as ifa_dstaddr) is not need for V6 so
	 * only initialize ifa_dstaddr if point-to-point.
	 */
	if (ifp->if_flags & IFF_POINTOPOINT)
		ia->ia_ifa.ifa_dstaddr = sin6tosa(&ia->ia_dstaddr);
	ia->ia_ifa.ifa_netmask = 
	  (struct sockaddr_new *)sin6tosa(&ia->ia_sockmask);
	ia->ia_sockmask.sin6_len = sizeof(ia->ia_sockmask);
	return (ia);
}


/*
 * Generic internet control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
in6_control(so, cmd, data, ifp)
	struct socket *so;
	__psint_t cmd;
	caddr_t data;
	register struct ifnet *ifp;
{
	register struct in6_ifreq *ifr = (struct in6_ifreq *)data;
	register struct in6_ifaddr *ia = 0;
	struct in6_aliasreq *ifra = (struct in6_aliasreq *)data;
	struct sockaddr_in6 oldaddr;
	int error = 0, hostIsNew, maskIsNew, iaIsNew = 0;
	extern void ifafree_common(struct ifaddr *, int);
	int do_lladdr = 0;

	/*
	 * Find address for this interface, if it exists.
	 */
	if (ifp) {
		IFNET_LOCK(ifp);
		/*
		 * The primary address associated with this ifnet record
		 * is stored in the ifnet record. In the old linked-list
		 * scheme it was the first 'struct ifaddr' hung off the
		 * ifnet structure which contained a matching ifnet address.
		 */
		 ia = (struct in6_ifaddr *)(ifp->in6_ifaddr);
	}
	switch (cmd) {
	case SIOCAIFADDR6:
	case SIOCDIFADDR6:
	case SIOCVIFADDR6:
		if (ifra->ifra_addr.sin6_family == AF_INET6) {
			ia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
			  in6addr_ifnetaddr_match,
			  (caddr_t)(&(satosin6(&(ifra->ifra_addr))->sin6_addr)),
			  (caddr_t)ifp, (caddr_t)0);
		}
		if (cmd != SIOCAIFADDR6 && ia == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		/* FALLTHROUGH */
	case SIOCSIFADDR6:
	case SIOCSIFNETMASK6:
	case SIOCSIFDSTADDR6:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}

		if (ifp == 0)
			panic("in6_control");
		if (ia == (struct in6_ifaddr *)0) {
			ia = in6_inifaddr_alloc(ifp);

			if (ifp->in6_ifaddr == (caddr_t)0) {
				ia->ia_addrflags = IADDR_PRIMARY;
				ifp->in6_ifaddr = (caddr_t)ia;
				if ((ifp->if_flags & IFF_LOOPBACK) == 0)
					(void)atomicAddInt(&in6_interfaces, 1);
				do_lladdr = 1;
			} else {
				ia->ia_addrflags = IADDR_ALIAS;
			}
			iaIsNew = 1;
		}
		break;

	case SIOCGIFADDR6:
	case SIOCGIFNETMASK6:
	case SIOCGIFDSTADDR6:
		if (ia == (struct in6_ifaddr *)0) {
			if (ifp)
				IFNET_UNLOCK(ifp);
			return (EADDRNOTAVAIL);
		}
		break;
	}
	switch (cmd) {

	case SIOCGIFADDR6:
		ifr->ifr_Addr = ia->ia_addr;
		break;

	case SIOCGIFDSTADDR6:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0) {
			error = EINVAL;
			break;
		}
		ifr->ifr_Addr = ia->ia_dstaddr;
		break;

	case SIOCGIFNETMASK6:
		ifr->ifr_Addr = ia->ia_sockmask;
		break;

	case SIOCSIFDSTADDR6:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0) {
			error = EINVAL;
			break;
		}
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = ifr->ifr_Addr;
		if (ifp->if_ioctl && (error = (*ifp->if_ioctl)
					(ifp, SIOCSIFDSTADDR, (caddr_t)ia))) {
			ia->ia_dstaddr = oldaddr;
			break;
		}
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = sin6tosa(&oldaddr);
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr = sin6tosa(&ia->ia_dstaddr);
			rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
		}
		break;

	case SIOCSIFADDR6:
		error = in6_ifinit(ifp, ia, &ifr->ifr_Addr, 1);
		break;

	case SIOCSIFNETMASK6:
		ifr->ifr_Addr = ia->ia_sockmask;
		break;

	case SIOCAIFADDR6:
		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;
		if (ia->ia_addr.sin6_family == AF_INET6) {
			if (ifra->ifra_addr.sin6_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (SAME_SOCKADDR(&ifra->ifra_addr,
						  &ia->ia_addr))
				hostIsNew = 0;
		}
		if (ifra->ifra_mask.sin6_len) {
			in6_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin6_family == AF_INET6)) {
			in6_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskIsNew  = 1; /* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.sin6_family == AF_INET6) {
		    if (iaIsNew) {
		       ASSERT(ia->ia_hashbucket.flags == HTF_INET);
		       (void)hash_insert(&hashinfo_in6addr,
			 &(ia->ia_hashbucket),
			 (caddr_t)(&(satosin6(&(ifra->ifra_addr))->sin6_addr)));
		       /* Bump number of configured address'es */
		       (void)atomicAddInt(&in6_ifaddr_count, 1);
		    }
		    if (hostIsNew || maskIsNew)
			error = in6_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		}
		break;

	case SIOCDIFADDR6:
		if (ifp->in6_ifaddr == (caddr_t)ia) {
			/*
			 * (Copied from in_control):
			 * Requested to delete primary address for interface! 
			 * We make this an error since we assume the user 
			 * doesn't know the consequences of this action.  
			 * The problem is that the alias address'es have 
			 * host routes associated with them and the primary 
			 * address is used in the host route.  Deleting the 
			 * primary address when alias'es exist causes serious 
			 * problems when trying to delete subsequent alias'es.  
			 * This could return no error, aka a no-op instead.  
			 */
			error = EOPNOTSUPP;
			break;
		}

		in6_ifscrub(ifp, ia);

		(void)atomicAddInt(&in6_ifaddr_count, -1);

		/*
		 * force delink of entry but conditionally release storage
		 * NOTE: We hold the write lock here to protect the routing
		 * daemon from looking at address'es that may be in the
		 * process of being deleted.
		 */
		ROUTE_WRLOCK();
		ifafree_common(&(ia->ia_ifa), 1);
		ROUTE_UNLOCK();
		break;

	case SIOCVIFADDR6:
		if (ifra->ifra_mask.sin6_len)
			ia->ia_flags |= IFA_BOOTING;
		else
			ia->ia_flags &= ~IFA_BOOTING;
		break;

	case SIOCADDANY6:
		return (in6_addanycast(&ifr->ifr_Addr.sin6_addr,
		  ifr->ifr_Addr.sin6_flowinfo));

	case SIOCDELANY6:
		return (in6_delanycast(&ifr->ifr_Addr.sin6_addr));

	case SIOCGIFSITE6:
		((struct ifreq *)data)->ifr_site6 = ifp->if_site6;
		break;

	case SIOCSIFSITE6:
		/* a value of zero means no site */
		ifp->if_site6 = ((struct ifreq *)data)->ifr_site6;
		break;
	
	case SIOCLIFADDR6:	/* Get list of interface network addresses */

		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}
		ifra = (struct in6_aliasreq *)data;

		if (ifra->ifra_addr.sin6_family == AF_INET6) {

			/* Check if the requested 'nth' element is valid */
			if (ifra->cookie < 0) { /* bad nth element req */
				error = EINVAL;
				break;
			}

			if (ifra->cookie == 0) {
				/*
				 * return primary address for the interface
				 */
				ia = (struct in6_ifaddr *)(ifp->in6_ifaddr);
			} else {
				/*
				 * return all alias'es for the interface
				 */
				ia = (struct in6_ifaddr *)hash_enum_getnext(
					&hashinfo_in6addr,
					in_getalias_enummatch,
					HTF_INET,
					1, /* skip primary addr which is zero*/
					ifra->cookie,
					(caddr_t)ifp);
			}

			if (ia == (struct in6_ifaddr *)0) {
				ifra->cookie = -1;
				break;
			}
			ifra->cookie++;
			COPY_ADDR6(satosin6(ia->ia_ifa.ifa_addr)->sin6_addr,
			  ifra->ifra_addr.sin6_addr);
			COPY_ADDR6(ia->ia_sockmask.sin6_addr,
			  ifra->ifra_mask.sin6_addr);
		}
		break;

	default:
		if (ifp == 0 || ifp->if_ioctl == 0) {
			error = EOPNOTSUPP;
			break;
		}
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		break;
	}
	if (ifp)
		IFNET_UNLOCK(ifp);
	/*
	 * If do_lladdr is set then the first IPv6 address for this interface
	 * has just been added and we need to set up the Link Local Address.
	 * Every interface is required to have one (except loopback).
	 */
	if (do_lladdr && (ifp->if_ndtype & IFND6_LLSET) == 0 &&
	  !(ifp->if_ndtype & IFND6_LOOP)) {
		struct in6_aliasreq ifr6;
		struct sockaddr_in6 *saddr6;
		struct sockaddr_in6 *mask6;

		ASSERT(GETLLADDR(ifp) != NULL);
		bzero((caddr_t)&ifr6, sizeof(ifr6));
		sprintf(ifr6.ifra_name, "%s%d", ifp->if_name, ifp->if_unit);
		saddr6 = &ifr6.ifra_addr;
		bzero(saddr6, sizeof(struct sockaddr_in6));
		saddr6->sin6_len = sizeof(struct sockaddr_in6);
		saddr6->sin6_family = AF_INET6;
		bcopy(GETLLADDR(ifp),
		  &saddr6->sin6_addr, sizeof(struct in6_addr));

		mask6 = &ifr6.ifra_mask;
		bzero((caddr_t)mask6, sizeof(struct sockaddr_in6));
		mask6->sin6_len = sizeof(struct sockaddr_in6);
		mask6->sin6_family = AF_UNSPEC;
		mask6->sin6_addr.s6_addr32[0] = -1;
		mask6->sin6_addr.s6_addr32[1] = -1;
		mask6->sin6_addr.s6_addr32[2] = -1;
		mask6->sin6_addr.s6_addr32[3] = -1;

		return (in6_control(so, SIOCAIFADDR6, (caddr_t)&ifr6, ifp));
	}
	return (error);
}

/*
 * Delete any existing route for an interface.
 */
void
in6_ifscrub(ifp, ia)
	register struct ifnet *ifp;
	register struct in6_ifaddr *ia;
{
	if ((ia->ia_flags & IFA_ROUTE) == 0)
		return;
	if (ifp->if_flags & (IFF_LOOPBACK|IFF_POINTOPOINT))
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
	else
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
	ia->ia_flags &= ~IFA_ROUTE;

	/*
	 * If the interface supports multicast, leave the
	 * "solicited nodes" multicast group on that interface.
	 */
	if (ifp->if_flags & IFF_MULTICAST) {
		register struct in6_multi *inm;
		struct in6_addr addr;

		addr.s6_addr32[0] = htonl(0xff020000);
		addr.s6_addr32[1] = 0;
		addr.s6_addr32[2] = htonl(1);
		addr.s6_addr32[3] = 0xff000000 |
		  ia->ia_addr.sin6_addr.s6_addr32[3];
		IN6_LOOKUP_MULTI(addr, ifp, inm);
		IFNET_UNLOCK(ifp);
		if (inm != NULL)
			in6_delmulti(inm);
		IFNET_LOCK(ifp);
	}
}

/*
 * Initialize an interface's IPv6 address
 * and routing table entry.
 */
int
in6_ifinit(ifp, ia, sin, scrub)
	register struct ifnet *ifp;
	register struct in6_ifaddr *ia;
	struct sockaddr_in6 *sin;
	int scrub;
{
	struct sockaddr_in6 oldaddr;
	int flags = RTF_UP, error;

	ASSERT(IFNET_ISLOCKED(ifp));

	oldaddr = ia->ia_addr;
	ia->ia_addr = *sin;
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl &&
	    (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia))) {
		ia->ia_addr = oldaddr;
		return (error);
	}
	if (scrub) {
		ia->ia_ifa.ifa_addr = sin6tosa(&oldaddr);
		in6_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = sin6tosa(&ia->ia_addr);
	}
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin6_family != AF_INET6)
			return (0);
		flags |= RTF_HOST;
	}
	/*
	 * Don't create cloning route for an host.
	 */
#define MASK(x)	ia->ia_sockmask.sin6_addr.s6_addr32[x]
	if ((MASK(0) == 0xffffffff) &&
	    (MASK(1) == 0xffffffff) &&
	    (MASK(2) == 0xffffffff) &&
	    (MASK(3) == 0xffffffff))
		ia->ia_flags |= RTF_HOST;
#undef MASK
	if ((error = rtinit(&(ia->ia_ifa), (int)RTM_ADD, flags)) == 0)
		ia->ia_flags |= IFA_ROUTE;
	/*
	 * If the interface supports multicast, join the "all nodes" and
	 * "solicited nodes" multicast groups on that interface.
	 */
	if (ifp->if_flags & IFF_MULTICAST) {
		struct in6_addr addr;
		extern struct in6_addr allnodes6_group;

		(void)in6_addmulti(&allnodes6_group, ifp);
		addr.s6_addr32[0] = htonl(0xff020000);
		addr.s6_addr32[1] = 0;
		addr.s6_addr32[2] = htonl(1);
		addr.s6_addr32[3] = 0xff000000 | sin->sin6_addr.s6_addr32[3];
		(void)in6_addmulti(&addr, ifp);
	}
	return (error);
}

/*
 * This procedure is used for the hash lookup match which compares the
 * multi-cast address in the entry with that supplied along with comparing
 * the given interface address pointers.
 * NOTE: The INADDR_ALLHOSTS_GROUP is added to the multicast address list
 * for each interface.
 *
 * in6_multi_match(struct in6_multi *inm, struct in6_addr *key,
 *  struct ifnet *arg)
 */
/* ARGSUSED */
int
in6_multi_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	struct in6_multi *inm;
	struct in6_addr *addr;
	struct ifnet *ifp;

	if (h->flags & HTF_MULTICAST) { /* multicast address node */
		inm = (struct in6_multi *)h;
		addr = (struct in6_addr *)key;
		ifp = (struct ifnet *)arg1;

		if ((inm->inm6_ifp == ifp) &&
		  SAME_ADDR6(inm->inm6_addr, *addr))
			return (1);
	}
	return (0);
}

/*
 * Add an address to the list of IPv6 multicast addresses
 * for a given interface.
 */
struct in6_multi *
in6_addmulti(addr, ifp)
	register struct in6_addr *addr;
	register struct ifnet *ifp;
{
	register struct in6_multi *inm;
	struct sockaddr_in6 sin6;

	ASSERT(IFNET_ISLOCKED(ifp));
	/*
	 * See if address already in list.
	 */
	IN6_LOOKUP_MULTI(*addr, ifp, inm);
	if (inm != NULL) {
		/*
		 * Found it; just increment the reference count.
		 */
		(void)atomicAddUint(&(inm->inm6_refcount), 1);
	} else {
		/*
		 * New address; allocate a new multicast record
		 * and link it into the interface's multicast list.
		 */
		inm = (struct in6_multi *)kmem_alloc(sizeof(*inm),
		    KM_NOSLEEP);
		if (inm == NULL) {
			return (NULL);
		}
		COPY_ADDR6(*addr, inm->inm6_addr);
		inm->inm6_ifp = ifp;
		inm->inm6_refcount = 1;
		inm->hashbucket.flags = HTF_MULTICAST;
		/*
		 * Ask the network driver to update its multicast reception
		 * filter appropriately for the new address.
		 */
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		COPY_ADDR6(*addr, sin6.sin6_addr);
		if ((ifp->if_ioctl == NULL) ||
		    (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&sin6) != 0) {
			kmem_free(inm, sizeof(*inm));
			return (NULL);
		}
		/*
		 * Let ICMPv6 know that we have joined
		 * a new IPv6 multicast group.  Drop ifnet lock to prevent
		 * deadlock.
		 */
		(void)hash_insert(&hashinfo_in6addr, &(inm->hashbucket),
			(caddr_t)(addr));
		IFNET_UNLOCK(ifp);
		icmp6_joingroup(inm);
		IFNET_LOCK(ifp);
	}
	return (inm);
}

/*
 * Delete a multicast address record.
 */
void
in6_delmulti(inm)
	register struct in6_multi *inm;
{
	struct sockaddr_in6 sin6;
	struct ifnet *ifp = inm->inm6_ifp;

	IFNET_LOCK(ifp);
	if (atomicAddUint(&inm->inm6_refcount, -1) == 0) {
		/*
		 * No remaining claims to this record; let ICMPv6 know that
		 * we are leaving the multicast group.
		 */
		icmp6_leavegroup(inm);
		/*
		 * Notify the network driver to update its multicast reception
		 * filter.
		 */
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		COPY_ADDR6(inm->inm6_addr, sin6.sin6_addr);
		(*inm->inm6_ifp->if_ioctl)(inm->inm6_ifp,
					   SIOCDELMULTI, (caddr_t)&sin6);
		/*
		 * Remove entry while holding ifnet lock so that nobody else
		 * can find us.  Removing from the IP hash table does NOT
		 * delete storage; that will be done below after the call to
		 * igmp_leavegroup().
		 */
		hash_remove(&hashinfo_in6addr, (struct hashbucket *)inm);
		IFNET_UNLOCK(ifp);
		kmem_free(inm, sizeof(*inm));
	} else {
		IFNET_UNLOCK(ifp);
	}
}
#endif /* INET6 */
