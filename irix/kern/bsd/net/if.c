/*
 * Copyright (c) 1980, 1986, 1993
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
 *	@(#)if.c	8.3 (Berkeley) 1/4/94
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"
#include "sys/atomic_ops.h"
#include "sys/hwgraph.h"
#include "sys/iograph.h"
#include "sys/cmn_err.h"

#include "sys/types.h"
#include "sys/kmem.h"
#include "sys/ddi.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/protosw.h"
#include "sys/signal.h"
#include "soioctl.h"
#include "ksoioctl.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/sockd.h"

#include "raw_cb.h"
#include "raw.h"
#include "if.h"
#include "if_dl.h"
#include "route.h"

#include "netinet/in.h"
#include "tc_isps.h"
#include "netinet/in_var.h"

/* Trusted IRIX */
#include "sys/sat.h"

#include "sys/systm.h"
#include "multi.h"
#include <bstring.h>
#include <string.h>
#include "sys/capability.h"
#include "sys/xlate.h"
#include "sys/kabi.h"
#include "sys/kthread.h"
#include "sys/uthread.h"

/*
 * Module define macro's
 */

#ifdef INET6
#define	SOCKADDR_EQUAL(a1, a2) \
	(bcmp((caddr_t)(a1), (caddr_t)(a2), \
	((struct sockaddr_new *)(a1))->sa_len ? \
	((struct sockaddr_new *)(a1))->sa_len : \
	sizeof(struct sockaddr_in)) == 0)
#else
#define	SOCKADDR_EQUAL(a1, a2) \
	(bcmp((caddr_t)((a1)->sa_data), (caddr_t)((a2)->sa_data), 14) == 0)
#endif

/*
 * Forward referenced procedures
 */
static int ifaddr_to_ifp_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2);

/*
 * External Procedures
 */
extern void ifafree(struct ifaddr *);
extern int arpioctl(int, caddr_t);
extern int ifconf(int, caddr_t);
extern void link_rtrequest(int, struct rtentry *, struct sockaddr *);
extern int rn_compare(void *, void *);

extern int ifnet_enummatch(struct hashbucket *,	caddr_t, caddr_t, caddr_t);
extern int ifnet_any_enummatch(struct hashbucket *, caddr_t, caddr_t, caddr_t);
extern int in_localaddr_subnet_enummatch(struct hashbucket *,
			caddr_t, caddr_t, caddr_t);
extern int rsvp_ioctl(caddr_t data);
extern  int new_atmarp_ioctl(int, caddr_t);

/*
 * External global data structures
 */
extern struct hashinfo hashinfo_inaddr;
#ifdef INET6
extern struct hashinfo hashinfo_in6addr;
#endif
extern time_t time;

/*
 * Module global data structures
 */
static int if_indexlim = 4;

u_int if_index;				/* MIB-II */
struct ifaddr **ifnet_addrs;

int	ifqmaxlen = IFQ_MAXLEN;
struct	ifnet *ifnet;
char	if_slowtimo_active = 0;

mutex_t ifhead_mutex;

/*
 * From a ifaddr structure return the starting address of the struct in_ifaddr.
 */
struct in_ifaddr *
ifatoia(struct ifaddr *ifa)
{
#ifdef INET6
	ASSERT(ifa == NULL ||
	  ((struct sockaddr_new *)(ifa->ifa_addr))->sa_family != AF_INET6);
#endif
	return ((ifa) ? ((struct in_ifaddr *)(ifa->ifa_start_inifaddr)) : 0);
}

/*
 * Procedure that finds the 'struct ifnet' interface structure corresponding
 * to the IP address supplied. This procedure is used to find either a
 * a Point-to-Point interface whose 'ifa_addr' address matches the one
 * supplied and failing that the first network interface which matches.
 * This is implemented as a separate procedure to avoid enumerating
 * the entire IP hash table if at all possible.
 *
 * NOTE: This procedure can be invoked via the macro with the same upper
 * case name.
 */
struct ifnet *
ifaddr_to_ifp(struct in_addr inaddr)
{
	struct ifnet *ifp;
	struct in_ifaddr *ia;
	struct sockaddr *addr;
	struct sockaddr_in sa_in;

	bzero(&sa_in, sizeof(struct sockaddr_in));
#ifdef _HAVE_SIN_LEN
	sa_in.sin_len = 8;
#endif
	sa_in.sin_family = AF_INET;
	sa_in.sin_addr = inaddr;
	addr = (struct sockaddr *)(&sa_in);

	/*
	 * Do the quick check first if we match the the local IP address
	 * associated with the primary address record for non PPP interfaces
	 * OR the destination address if it's a PPP interface.
	 */
	for (ifp = ifnet; ifp; ifp = ifp->if_next) {

	   ia = (struct in_ifaddr *)(ifp->in_ifaddr);

	   if ((ia == NULL) ||
	       (ia->ia_ifa.ifa_addr->sa_family != addr->sa_family)) {

		continue;
	   }
   	   /* this routine is called only by multi-cast
	    * function, check ifa_addr to support
	    * mrouted(1m) for IFF_POINTOPOINT interface/bug#403635.
	    */
	   if (ifp->if_flags & IFF_POINTOPOINT) {
           	if (SOCKADDR_EQUAL(ia->ia_ifa.ifa_dstaddr, addr) ||
                    SOCKADDR_EQUAL(ia->ia_ifa.ifa_addr, addr)) {
			return ifp;
		}
	   } else {
		if (SOCKADDR_EQUAL(ia->ia_ifa.ifa_addr, addr)) {
			return ifp;
		}
	   }
	}

	/*
	 * Now do the more expensive check for to see if we have a alias
	 * address which matches the supplied address. This case is expensive
	 * since we are must enumerate the IP address hash table. The normal
	 * case won't required this overhead to check all system addresses.
	 */
	ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
			ifaddr_to_ifp_enummatch,
			HTF_INET, /* unicast IP address entries only */
			(caddr_t)(&(sa_in.sin_addr)),
			(caddr_t)addr,
			(caddr_t)0);

	ifp = (ia) ? ia->ia_ifp : (struct ifnet *)0;
	return ifp;
}

/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as parameters.
 */
void
ifinit(void)
{
	register struct ifnet *ifp;

	IFHEAD_INITLOCK();
	for (ifp = ifnet; ifp; ifp = ifp->if_next)
		if (ifp->if_snd.ifq_maxlen == 0)
			ifp->if_snd.ifq_maxlen = ifqmaxlen;

	/* turn on the timeout processing in sockd */
	if_slowtimo_active = 1;
}

/*
 * Allocate and initialize an ifaddr structure which contains a correct
 * MAC level link structure, called a sockaddr_dl in 4.4BSD paralance.
 * It contains a sockaddr dl variance of an ifaddr structure.
 * It returns the address of the allocated ifaddr structure after intserting
 * it into the netaddr array for AF_LINK addresses. The index + 1 into this
 * array is stored in the ifnet structure.
 */
extern int routelock_inited;

struct ifaddr *
if_create_linkaddr(struct ifnet *ifp)
{
	unsigned socksize, ifasize;
	int namelen, unitlen, masklen;
	char workbuf[12];
	register struct sockaddr_dl *sdl;
	register struct ifaddr *ifa;

	ifp->if_index = ++if_index;

	if (!routelock_inited) {
		routelock_inited++;
		ROUTE_INITLOCK();
	}

	ROUTE_WRLOCK();

	if (ifnet_addrs == 0 || if_index >= if_indexlim) {

		unsigned n = (if_indexlim <<= 1) * sizeof(ifa);
		struct ifaddr **q = (struct ifaddr **)kmem_zalloc(n, KM_SLEEP);

		if (ifnet_addrs) {
			bcopy((caddr_t)ifnet_addrs, (caddr_t)q, n/2);
			kmem_free(ifnet_addrs, n/2);
		}
		ifnet_addrs = q;
	}
	ROUTE_UNLOCK();

	/*
	 * create a Link Level name for this device
	 * XXX This duplicates the facilities of rawif_attach()
	 */
	sprintf(workbuf, "%d", ifp->if_unit);
	namelen = strlen(ifp->if_name);
	unitlen = strlen(workbuf);

	masklen = sizeof(*sdl)-sizeof(sdl->sdl_data)+unitlen+namelen;

	if (ifp->if_addrlen == 0) {
		/*
		 * Assume 48-bit/6 byte Ethernet link address for now.
		 */
		ifp->if_addrlen = 6;
	}

	socksize = masklen + ifp->if_addrlen;
#define ROUNDUP(a) (1 + (((a) - 1) | (sizeof(__uint64_t) - 1)))
	if (socksize < sizeof(*sdl))
		socksize = sizeof(*sdl);
	socksize = ROUNDUP(socksize);
	/*
	 * Compute enough space for one ifaddr structure and two socket
	 * address'es, one for the primary and one for the network mask.
	 */
	ifasize = sizeof(struct ifaddr) + 2 * socksize;

	ifa = (struct ifaddr *)kmem_zalloc(ifasize, KM_SLEEP);

	/* compute sockaddr dl's address and save as ifaddr address */
	sdl = (struct sockaddr_dl *)((char *)ifa + sizeof(struct ifaddr));

	ifa->ifa_addr = (struct sockaddr *)sdl;
#ifdef _HAVE_SA_LEN
	sdl->sdl_len = socksize;
#endif
	sdl->sdl_family = AF_LINK;

	bcopy(ifp->if_name, sdl->sdl_data, namelen);
	bcopy(workbuf, namelen + sdl->sdl_data, unitlen);
	sdl->sdl_nlen = (namelen += unitlen);

	/*
	 * NOTE: The real MAC level adress will be copied into the ifaddr
	 * structure 'ifa_addr' starting at offset sdl_nlen into the sdl_data
	 * array. The storage is allocated for a sockaddr_dl structure with
	 * at least 6 bytes reserved for the MAC level address PLUS the
	 * name and unit number string lengths. The sdl_alen field is ZERO
	 * to indicate that the MAC adress hasn't been copied into the adress
	 * structure. The real MAC adress is set when raw_ifattach calls
	 * rawif_fixlink with MAC address is known.
	 */
	sdl->sdl_index = ifp->if_index;
	sdl->sdl_type = ifp->if_type;

	/* save the ifaddr address */
	ifnet_addrs[if_index - 1] = ifa;

	/*
	 * initialize the sockaddr_dl for the netmask address
	 */
	sdl = (struct sockaddr_dl *)(socksize + (caddr_t)sdl);
#ifdef _HAVE_SA_LEN
	ifa->ifa_netmask = (struct sockaddr *)sdl;
	sdl->sdl_len = masklen;
#else
	ifa->ifa_netmask = (struct sockaddr_new *)sdl;
#endif
	sdl->sdl_family = AF_LINK;

	ifa->ifa_ifp = ifp;
	ifa->ifa_rtrequest = link_rtrequest;

	while (namelen != 0)
		sdl->sdl_data[--namelen] = 0xff;

	return ifa;
}

/*
 * Attach an interface to the list of "active" interfaces.
 */
void
if_attach(struct ifnet *ifp)
{
	register struct ifnet **p;

	IFNET_INITLOCKS(ifp);		

	p = &ifnet;
	while (*p && !((*p)->if_flags & IFF_LOOPBACK)) {
		ASSERT(*p != ifp);
		p = &((*p)->if_next);
	}
	ifp->if_next = *p;
	*p = ifp;
	(void)if_create_linkaddr(ifp);
	return;
}

/*
 * Hash lookup match procedure looking for an entry which matches the
 * complete sockaddr address and is for the specified interface.
 *
 * ifwithaddr_match(struct ifaddr *ifa, struct in_addr *key,
 * struct sockaddr *arg1, struct ifnet *arg2);
 */
/* ARGSUSED */
static int
ifwithaddr_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
#ifdef INET6
	struct ifaddr *ifa;
#else
	struct in_ifaddr *ia;
#endif
	struct sockaddr *addr;
	struct ifnet *ifp;
	int match = 0;

	if ((h->flags & HTF_INET) == 0) { /* skip wrong entry type */
		return match;
	}
#ifdef INET6
	ifa = (struct ifaddr *)h;
#else
	ia = (struct in_ifaddr *)h;
#endif
	addr = (struct sockaddr *)arg1;
	ifp = (struct ifnet *)arg2;

#ifdef INET6
	if (ifa->ifa_ifp == ifp) { /* keep checking for complete match */

		if (ifa->ifa_addr->sa_family == addr->sa_family) {

			if (SOCKADDR_EQUAL(ifa->ifa_addr, addr)) {
#else
	if (ia->ia_ifp == ifp) { /* keep checking for complete match */

		if (ia->ia_ifa.ifa_addr->sa_family == addr->sa_family) {

			if (SOCKADDR_EQUAL(ia->ia_ifa.ifa_addr, addr)) {
#endif
				match = 1;
			} else {
#ifdef INET6
				/* bcast not used by V6 */
				if ((((struct sockaddr_new *)addr)->sa_family !=
				  AF_INET6) &&
				  (ifp->if_flags & IFF_BROADCAST) &&
				  SOCKADDR_EQUAL(ifa->ifa_broadaddr, addr)) {
					match = 1;
				}
#else
				if ((ifp->if_flags & IFF_BROADCAST) &&
			    SOCKADDR_EQUAL(ia->ia_ifa.ifa_broadaddr, addr)) {
					match = 1;
				}
#endif
			}
		}
	}
	return match;
}

/*
 * Hash enumerate match procedure looking for a sockaddr address
 * associated with the specified point-to-point interface.
 *
 * ifaddr_to_ifp_enummatch(struct ifaddr *ifa, struct in_addr *key.
 *      struct sockaddr *arg1,	caddr_t arg2)
 *
 * NOTE: This enumeration match procedure is called with only hash bucket
 * entries which match the specified type on the enumerate call.
 * Here that is HTF_INET types.
 */
/* ARGSUSED */
static int
ifaddr_to_ifp_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct sockaddr *addr = (struct sockaddr *)arg1;
	int match = 0;

#ifdef INET6
	ASSERT(addr->sa_family != AF_INET6);
#endif
	if (ia->ia_ifa.ifa_addr->sa_family == addr->sa_family) {
		/*
		 * Note: The comparison here is against the destination
		 * for IFF_POINTOPOINT interfaces and against the local address
		 * for other network interfaces. Hence this requires that the
		 * address supplied via the 'struct ip_mreq' structure is the
		 * either local address associated with the interface for
		 * non PPP cases or the destination address for PPP interfaces
		 */
		if (ia->ia_ifp->if_flags & IFF_POINTOPOINT) {
			if (SOCKADDR_EQUAL(ia->ia_ifa.ifa_dstaddr, addr)) {
				match = 1;
			}
		} else {
			if (SOCKADDR_EQUAL(ia->ia_ifa.ifa_addr, addr)) {
				match = 1;
			}
		}
	}
	return match;
}

/*
 * Hash look match procedure looking for a matching broadcast
 * sockaddr address for any of our specified interfaces.
 *
 * ifwithaddr_bcast_match(struct in_bcast *b, struct in_addr *key.
 *     struct sockaddr *arg1, caddr_t arg2)
 *
 * NOTE: This lookup match procedure is called with the hash bucket
 * entries inserted into the table. We check here for HTF_BROADCAST types
 * just in case somebody adds a new type later.
 */
/* ARGSUSED */
static int
ifwithaddr_bcast_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_bcast *in_bcastp = (struct in_bcast *)h;
	struct sockaddr_in *addr = (struct sockaddr_in *)arg1;

	if ((h->flags & HTF_BROADCAST) == 0) { /* not broadcast entry */
		return 0;
	}
	/*
	 * Check that supplied lookup key matches the one used on entry
	 * insertion, before we check the rest of the fields to be sure
	 * this is the correct one since there can be a full broadcast
	 * entry and a netbroadcast entry depending on the netmask.
	 */
	if (bcmp(h->key, key, sizeof(struct in_addr) != 0)) { /* no match */
		return 0;
	}

	return (((in_bcastp->ia_addr.sin_family == addr->sin_family) &&
	   (in_bcastp->ia_dstaddr.sin_addr.s_addr == addr->sin_addr.s_addr))
	     ? 1 : 0);
}

/*
 * Locate an interface based on a complete socket address.
 */
struct ifaddr *
ifa_ifwithaddr(struct sockaddr *addr)
{
	struct ifnet *ifp;
#ifndef INET6
	struct in_ifaddr *ia;
#endif
	struct in_bcast *in_bcastp;
	struct ifaddr *ifa = (struct ifaddr *)0;
#ifdef INET6
	struct hashinfo *hi;
	caddr_t key;

	if (((struct sockaddr_new *)addr)->sa_family == AF_INET6) {
		hi = &hashinfo_in6addr;
		key = (caddr_t)(&(((struct sockaddr_in6 *)(addr))->sin6_addr));
	} else {
		hi = &hashinfo_inaddr;
		key = (caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr));
	}
#endif
	for (ifp = ifnet; ifp; ifp = ifp->if_next) {

#ifdef INET6
		ifa = (struct ifaddr *)hash_lookup(hi, ifwithaddr_match, key,
			(caddr_t)addr, (caddr_t)ifp); 
		if (ifa) /* match so return the ifaddr address */
			break;
#else
		ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
			ifwithaddr_match,
			(caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr)),
			(caddr_t)addr,
			(caddr_t)ifp);

		if (ia) { /* match so return the ifaddr address */
			ifa = &(ia->ia_ifa);
			break;
		}
#endif
	}
#ifdef INET6
	/* v6 doesn't have bcast addrs so skip this */
	if (ifa == 0 && ((struct sockaddr_new *)addr)->sa_family != AF_INET6) {
#else
	if (ifa == 0) {
#endif
		/*
		 * Didn't find a direct address match so we now do the lookup
		 * looking for a matching broadcast address on any interface.
		 * This is the high performance check when having lots of
		 * address'es since checking for a matching broadcast address
		 * is now a fast hash table lookup in broadcast hash table.
		 */
		in_bcastp = (struct in_bcast *)hash_lookup(
			&hashinfo_inaddr_bcast,
			ifwithaddr_bcast_match,
			(caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr)),
			(caddr_t)addr,
			(caddr_t)0);
		if (in_bcastp) {
			/*
			 * found matching broadcast address so return address,
			 * if set, of primary address structure for interface
			 */
#ifdef INET6
			ifa = (struct ifaddr *)in_bcastp->ifp->in_ifaddr;
#else
			ia = (struct in_ifaddr *)in_bcastp->ifp->in_ifaddr;
			ifa = (ia) ? &(ia->ia_ifa) : 0;
#endif
		}
	}
	return ifa;
}

/*
 * Hash enumerate match procedure looking for a destination sockaddr address
 * associated with the specified point-to-point interface.
 *
 * ifwithdstaddr_enummatch(struct ifaddr *ifa, struct in_addr *key.
 *      struct sockaddr *arg1,	struct ifnet *arg2)
 *
 * NOTE: This enumeration match procedure is called with only hash bucket
 * entries which match the specified type on the enumerate call.
 * Here that is HTF_INET types.
 */
/* ARGSUSED */
static int
ifwithdstaddr_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
#ifdef INET6
	struct ifaddr *ifa;
#else
	struct in_ifaddr *ia;
#endif
	struct sockaddr *addr;
	struct ifnet *ifp;
	int match = 0;

#ifdef INET6
	ifa = (struct ifaddr *)h;
#else
	ia = (struct in_ifaddr *)h;
#endif
	addr = (struct sockaddr *)arg1;
	ifp = (struct ifnet *)arg2;

#ifdef INET6
	if (ifa->ifa_ifp == ifp) { /* keep checking for complete match */

		if (ifa->ifa_ifp->if_flags & IFF_POINTOPOINT) {

			if (ifa->ifa_dstaddr->sa_family == addr->sa_family) {
			    if (SOCKADDR_EQUAL(ifa->ifa_dstaddr, addr))
					match = 1;
			}
		}
	}
#else
	if (ia->ia_ifp == ifp) { /* keep checking for complete match */

		if (ia->ia_ifp->if_flags & IFF_POINTOPOINT) {

			if (ia->ia_ifa.ifa_dstaddr->sa_family
				== addr->sa_family) {

			    if (SOCKADDR_EQUAL(ia->ia_ifa.ifa_dstaddr, addr)) {
					match = 1;
				}
			}
		}
	}
#endif
	return match;
}

/*
 * Locate the point to point interface with a given destination address.
 */
struct ifaddr *
ifa_ifwithdstaddr(struct sockaddr *addr)
{
	struct ifnet *ifp;
#ifdef INET6
	struct ifaddr *ifa = (struct ifaddr *)0;
	struct hashinfo *hi;
	caddr_t key;

	if (((struct sockaddr_new *)addr)->sa_family == AF_INET6) {
		hi = &hashinfo_in6addr;
		key = (caddr_t)(&(((struct sockaddr_in6 *)(addr))->sin6_addr));
	} else {
		hi = &hashinfo_inaddr;
		key = (caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr));
	}

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {

	    if (ifp->if_flags & IFF_POINTOPOINT) {

		ifa = (struct ifaddr *)hash_enum(hi,
			ifwithdstaddr_enummatch,
			HTF_INET, /* unicast IP address entries only */
			key, (caddr_t)addr, (caddr_t)ifp);

		if (ifa) /* match so quit */
			break;
	    }
	}
#else
	struct in_ifaddr *ia;
	struct ifaddr *ifa = (struct ifaddr *)0;

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {

	    if (ifp->if_flags & IFF_POINTOPOINT) {

		ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
			ifwithdstaddr_enummatch,
			HTF_INET, /* unicast IP address entries only */
			(caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr)),
			(caddr_t)addr,
			(caddr_t)ifp);

		if (ia) { /* match so quit */
			ifa = &(ia->ia_ifa);
			break;
		}
	    }
	}
#endif
	return ifa;
}

/*
 * Find an AF_LINK interface address specific to an adress and the
 * 'struct ifaddr' address. This procedures ASSUMES that 'addr' is
 * a well formed sockaddr_dl containing valid ifnet_addrs indexes.
 */
struct ifaddr *
ifa_ifwithlinkaddr(struct sockaddr *addr)
{
	register struct sockaddr_dl *sdl = (struct sockaddr_dl *)addr;
	register struct ifaddr *ifa = (struct ifaddr *)0;

	if (sdl->sdl_index && sdl->sdl_index <= if_index) {
		ifa = ifnet_addrs[sdl->sdl_index - 1];
	}
	return ifa;
}

/*
 * Find an AF_LINK address structure for the specified ifnet address.
 * We search the ifnet_addrs array looking for a matching ifnet address.
 */
struct ifaddr *
ifa_ifwithlink_ifp(struct ifnet *ifp)
{
	register struct ifaddr *ifa;
	register short i;

	for (i=0; i < if_indexlim; i++) {
		if ((ifa = ifnet_addrs[i])) { /* check this one */
			if (ifa->ifa_ifp == ifp) {
				return ifa;
			}
		}
	}
	return ((struct ifaddr *)0);
}

/*
 * Hash enum bestmatch procedure which is called ONLY with hash bucket
 * of the requested type and the various arguments.
 *
 * ifwithnet_bestmatch(struct ifaddr *ifa, struct in_addr *key,
 *  struct sockaddr *arg1, struct ifnet *arg2)
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
static int
ifwithnet_bestmatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
#ifdef INET6
	struct ifaddr *ifa;
#else
	struct in_ifaddr *ia;
#endif
	struct sockaddr *addr;
	char *cp, *cp2, *cp3, *cplim;

#ifdef INET6
	ifa = (struct ifaddr *)h;
#else
	ia = (struct in_ifaddr *)h;
#endif
	addr = (struct sockaddr *)arg1;

#ifdef INET6
	if (ifa->ifa_addr->sa_family != addr->sa_family ||
	    ifa->ifa_netmask == 0) {
#else
	if (ia->ia_ifa.ifa_addr->sa_family != addr->sa_family ||
	    ia->ia_ifa.ifa_netmask == 0) {
#endif
		/* no match so keep enumerating */
		return 0;
	}

	cp = addr->sa_data;
#ifdef INET6
	/* cp2 = ifa->ifa_addr->sa_data; */
	cp2 = (ifa->ifa_ifp->if_flags & IFF_POINTOPOINT)
		? ifa->ifa_dstaddr->sa_data
		: ifa->ifa_addr->sa_data;
	cp3 = ifa->ifa_netmask->sa_data;
	cplim = (char *)ifa->ifa_netmask+ ifa->ifa_netmask->sa_len;
#else
	/* cp2 = ia->ia_ifa.ifa_addr->sa_data; */
	cp2 = (ia->ia_ifa.ifa_ifp->if_flags & IFF_POINTOPOINT)
		? ia->ia_ifa.ifa_dstaddr->sa_data
		: ia->ia_ifa.ifa_addr->sa_data;
	cp3 = ia->ia_ifa.ifa_netmask->sa_data;
	cplim = (char *)ia->ia_ifa.ifa_netmask+ ia->ia_ifa.ifa_netmask->sa_len;
#endif

	while (cp3 < cplim) {
		if ((*cp++ ^ *cp2++) & *cp3++) {
			return 0; /* no match but keep going */
		}
	}
	/* one of the possible matches so indicate so */
	return 1;
}

/*
 * The hash enum bestmatch refines function. It is called to determine if
 * the new matchine hash bucket entry 'h' has a more "refined" network mask
 * than the hashbucket entry with the best match so far in 'bestsofar'.
 * It returns non-zero IF the new entry's network mask is "better" than
 * the previous best.
 *
 * IF the two entries have equal length bit strings for network numbers AND
 *  the new entry corresponds to a primary address entry for the interface AND
 *  the previous best enrtry was an alias address
 * THEN return the primary address entry.
 *
 * ifwithnet_refines(struct hashbucket *new, struct hashbucket *bestsofar)
 */
static int
ifwithnet_refines(struct hashbucket *new, struct hashbucket *bestsofar)
{
#ifdef INET6
	register struct ifaddr *ifa = (struct ifaddr *)new;
	register struct ifaddr *best = (struct ifaddr *)bestsofar;
#else
	register struct in_ifaddr *ia = (struct in_ifaddr *)new;
	register struct in_ifaddr *best = (struct in_ifaddr *)bestsofar;
#endif
	int rtn, result;

	/*
	 * Here we are comparing rn_compare(new, bestsofar).
	 * The procedure 'rn_compare(m, n)' returns:
	 *
	 * -1 => if 'm' is worse than 'n' meaning 'm' has a shorter string
	 *       of matching bits in the network number than 'n'
	 *  0 => if 'm' is equal to 'n' means 'm' has an equal number
	 *       of matching bits in the network number than 'n'
	 *  1 => if 'm' is better than 'n' means 'm' has a longer string
	 *       of bits in the network number than 'n'. In other words 'm' is
	 *       more refined than 'n', this is the previous case that was
	 *       being checked.
	 */
#ifdef INET6
	result = rn_compare((void *)ifa->ifa_netmask,
			(void *)best->ifa_netmask);
#else
	result = rn_compare((void *)ia->ia_ifa.ifa_netmask,
			(void *)best->ia_ifa.ifa_netmask);
#endif

	switch (result) {
		case -1:
			/*
			 * Here 'ifa' network number is shorter than
			 * 'best' so return less_refined(0) indicating
			 * to keep 'best' as the comparison result.
			 */
			rtn = 0;
			break;
		case 0:
			/*
			 * Here both network numbers are equal so favor
			 * the primary address over the alias address.
			 */
#ifdef INET6
			rtn = ((ifa->ifa_addrflags & IADDR_PRIMARY) &&
				(best->ifa_addrflags & IADDR_ALIAS)) ? 1 : 0;
#else
			rtn = ((ia->ia_addrflags & IADDR_PRIMARY) &&
				(best->ia_addrflags & IADDR_ALIAS)) ? 1 : 0;
#endif
			break;
		default:
			/*
			 * Here network number 'ifa' is longer than the
			 * best so far so we'll make this the new 'best'
			 */
			rtn = 1;
			break;
	}
	return rtn;
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(struct sockaddr *addr)
{
#ifdef INET6
	struct ifaddr *ifa;
	u_int af = ((struct sockaddr_new *)addr)->sa_family;
	struct hashinfo *hi;
	caddr_t key;
#else
	struct in_ifaddr *ia;
	struct ifaddr *ifa_best;
	u_int af = addr->sa_family;
#endif

	if (af >= AF_MAX)
		return ((struct ifaddr *)0);

	if (af == AF_LINK) {
		return (ifa_ifwithlinkaddr(addr));
	}

#ifdef INET6
	if (af == AF_INET6) {
		hi = &hashinfo_in6addr;
		key = (caddr_t)(&(((struct sockaddr_in6 *)(addr))->sin6_addr));
	} else {
		hi = &hashinfo_inaddr;
		key = (caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr));
	}

	ifa = (struct ifaddr *)hash_enum_bestmatch(hi,
#else
	ia = (struct in_ifaddr *)hash_enum_bestmatch(&hashinfo_inaddr,
#endif
			ifwithnet_bestmatch,
			ifwithnet_refines,
			HTF_INET, /* Strict IP address entries only */
#ifdef INET6
			key, (caddr_t)addr, (caddr_t)0);
#else
			(caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr)),
			(caddr_t)addr,
			(caddr_t)0);
	ifa_best = (ia) ? &(ia->ia_ifa) : (struct ifaddr *)0;
#endif
#ifdef INET6
	return ifa;
#else
	return ifa_best;
#endif
}

/*
 * This is a hash enumeration match procedure which calls returns the first
 * record which matches the interface address AND has an address matching
 * the address family supplied.
 *
 * ifa_ifwithaf_enummatch(struct ifaddr *ifa, struct sockaddr *addr,
 *	struct ifnet *arg1, int arg2)
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
static int
ifwithaf_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
#ifdef INET6
	struct ifaddr *ifa = (struct ifaddr *)h;
#else
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
#endif
	struct ifnet *ifp = (struct ifnet *)arg1;
	int af = (__psint_t)arg2;
	int found;

#ifdef INET6
	found = ((ifa->ifa_ifp == ifp) &&
	    (ifa->ifa_addrflags & IADDR_PRIMARY) &&
	    (((struct sockaddr_new *)ifa->ifa_addr)->sa_family == af)) ? 1 : 0;
#else
	found = ((ia->ia_ifp == ifp) && (ia->ia_addrflags & IADDR_PRIMARY) &&
	    (ia->ia_ifa.ifa_addr->sa_family == af)) ? 1 : 0;
#endif

	return found;
}

/*
 * Find an interface using a specific address family
 */
struct ifaddr *
ifa_ifwithaf(int af)
{
	register struct ifnet *ifp;
#ifndef INET6
	struct in_ifaddr *ia;
#endif
	struct ifaddr *ifa = (struct ifaddr *)0;
#ifdef INET6
	struct hashinfo *hi;
#endif

	if (af >= AF_MAX)
		return ifa;

#ifdef INET6
	hi = (af == AF_INET6 ?  &hashinfo_in6addr : &hashinfo_inaddr);
#endif

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {

#ifdef INET6
		ifa = (struct ifaddr *)hash_enum(hi,
#else
		ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
#endif
			ifwithaf_enummatch,
			HTF_INET, /* Strict IP address entries only */
			(caddr_t)0,
			(caddr_t)ifp,
			(caddr_t)(__psint_t)af);

#ifdef INET6
		if (ifa) /* match so quit */
			break;
#else
		if (ia) { /* match so quit */
			ifa = &(ia->ia_ifa);
			break;
		}
#endif
	}
	return ifa;
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 *
 * NOTE: Typically 'addr' aka 'arg1' is NOT a complete sockaddr
 * but the network portion of that address since this procedure is
 * called by the rtrequest() procedure in the routing code.
 *
 * ifaof_ifpforaddr_enummatch(struct hashbucket *h, struct in_addr *key,
 *	struct sockaddr *arg1, struct ifnet *arg2)
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/*ARGSUSED*/
static int
ifaof_ifpforaddr_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
#ifdef INET6
	struct ifaddr *ifa = (struct ifaddr *)h;
	struct sockaddr *addr = (struct sockaddr *)arg1;
	struct ifnet *ifp = (struct ifnet *)arg2;
	char *cp, *cp2, *cp3, *cplim;

	if (ifa->ifa_ifp != ifp) { /* ensure this entry is for this interface */
		return 0;
	}

	if (ifa->ifa_addr->sa_family != addr->sa_family)
		return 0;

	if (ifa->ifa_netmask == 0) {
		if (SOCKADDR_EQUAL(addr, ifa->ifa_addr) || (ifa->ifa_dstaddr &&
		     SOCKADDR_EQUAL(addr, ifa->ifa_dstaddr))) {
			return 1;
		}
		return 0;
	}
	cp = addr->sa_data;
	cp2 = ifa->ifa_addr->sa_data;
	cp3 = ifa->ifa_netmask->sa_data;
	cplim = (char *)ifa->ifa_netmask+ ifa->ifa_netmask->sa_len;
#else
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct sockaddr *addr = (struct sockaddr *)arg1;
	struct ifnet *ifp = (struct ifnet *)arg2;
	char *cp, *cp2, *cp3, *cplim;

	if (ia->ia_ifp != ifp) { /* ensure this entry is for this interface */
		return 0;
	}

	if (ia->ia_ifa.ifa_addr->sa_family != addr->sa_family)
		return 0;

	if (ia->ia_ifa.ifa_netmask == 0) {
		if (SOCKADDR_EQUAL(addr, ia->ia_ifa.ifa_addr) ||
		    (ia->ia_ifa.ifa_dstaddr &&
		     SOCKADDR_EQUAL(addr, ia->ia_ifa.ifa_dstaddr))) {
			return 1;
		}
		return 0;
	}
	cp = addr->sa_data;
	cp2 = ia->ia_ifa.ifa_addr->sa_data;
	cp3 = ia->ia_ifa.ifa_netmask->sa_data;
	cplim = (char *)ia->ia_ifa.ifa_netmask+ ia->ia_ifa.ifa_netmask->sa_len;
#endif

	for (; cp3 < cplim; cp3++) {
		if ((*cp++ ^ *cp2++) & *cp3) {
			break;
		}
	}
	return ((cp3 == cplim) ? 1 /* match */ : 0);
}

/*
 * Compare the ifaddr information of the primary address of the network
 * interfaces with the socket address supplied. This is a fast optimization
 * to check if we have a complete matching address against the primary address
 * for the interface. This avoids the overhead of enumerating the entire IP
 * address hash table just to find the first complete matching address using
 * the network number portion of the address.
 */
static struct ifaddr *
ifaof_ifpforaddr_primary(struct sockaddr *addr, struct ifnet *ifp)
{
	char *cp, *cp2, *cp3, *cplim;
#ifdef INET6
	struct ifaddr *ifa;

	if (((struct sockaddr_new *)addr)->sa_family == AF_INET6)
		ifa = (struct ifaddr *)ifp->in6_ifaddr;
	else
		ifa = (struct ifaddr *)ifp->in_ifaddr;
	if ((ifa == 0) || ifa->ifa_addr->sa_family != addr->sa_family) {
		return ((struct ifaddr *)0);
	}

	if (ifa->ifa_netmask == 0) { /* no network mask */

		if (SOCKADDR_EQUAL(addr, ifa->ifa_addr) || (ifa->ifa_dstaddr &&
		     SOCKADDR_EQUAL(addr, ifa->ifa_dstaddr))) {

			return (ifa);
		}
		return (0);
	}
	cp = addr->sa_data;
	cp2 = ifa->ifa_addr->sa_data;
	cp3 = ifa->ifa_netmask->sa_data;
	cplim = (char *)ifa->ifa_netmask + ifa->ifa_netmask->sa_len;
#else
	struct ifaddr *ifa = (struct ifaddr *)0;
	struct in_ifaddr *ia = (struct in_ifaddr *)ifp->in_ifaddr;

	if ((ia == 0) || ia->ia_ifa.ifa_addr->sa_family != addr->sa_family) {
		return ((struct ifaddr *)0);
	}

	if (ia->ia_ifa.ifa_netmask == 0) { /* no network mask */

		if (SOCKADDR_EQUAL(addr, ia->ia_ifa.ifa_addr) ||
		    (ia->ia_ifa.ifa_dstaddr &&
		     SOCKADDR_EQUAL(addr, ia->ia_ifa.ifa_dstaddr))) {

			ifa = &(ia->ia_ifa);
		}
		return ifa;
	}
	cp = addr->sa_data;
	cp2 = ia->ia_ifa.ifa_addr->sa_data;
	cp3 = ia->ia_ifa.ifa_netmask->sa_data;
	cplim = (char *)ia->ia_ifa.ifa_netmask+ ia->ia_ifa.ifa_netmask->sa_len;
#endif

	for (; cp3 < cplim; cp3++) {
		if ((*cp++ ^ *cp2++) & *cp3) {
			break;
		}
	}
	return ((cp3 == cplim)
#ifdef INET6
		? ifa /* match */
#else
		? &(ia->ia_ifa) /* match */
#endif
		: (struct ifaddr *)0); /* no match */
}

/*
 * Find an interface address specific to an interface best matching
 * the given address, 'addr', but take the first complete match.
 *
 * NOTE: Typically 'addr' aka 'arg1' is NOT a complete sockaddr
 * but the network portion of that address since this procedure is
 * called by the rtrequest() procedure in the routing code.
 */
struct ifaddr *
ifaof_ifpforaddr(struct sockaddr *addr, struct ifnet *ifp)
{
#ifdef INET6
	struct ifaddr *ifa;
	struct hashinfo *hi;
	caddr_t key;

	if ((((struct sockaddr_new *)addr)->sa_family >= AF_MAX) ||
	  (ifp == 0)) {
		return ((struct ifaddr *)0);
	}

	if (((struct sockaddr_new *)addr)->sa_family == AF_LINK) {
		return (ifa_ifwithlinkaddr(addr));
	}

	/*
	 * fast optimization case is to check for perfect complete match
	 * with the primary address associated with the network interface.
	 * This avoid the enumeration of the entire IP address hash table.
	 */
	if (ifa = ifaof_ifpforaddr_primary(addr, ifp)) {
		return ifa;
	}

	if (((struct sockaddr_new *)addr)->sa_family == AF_INET6) {
		hi = &hashinfo_in6addr;
		key = (caddr_t)(&(((struct sockaddr_in6 *)(addr))->sin6_addr));
	} else {
		hi = &hashinfo_inaddr;
		key = (caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr));
	}

	ifa = (struct ifaddr *)hash_enum(hi,
			ifaof_ifpforaddr_enummatch,
			HTF_INET, /* regular IP address entries only */
			key, (caddr_t)addr, (caddr_t)ifp);

	if (ifa == NULL) {
		/*
		 * Didn't find first match so
		 * return primary address if same address family
		 */
		if (((struct sockaddr_new *)addr)->sa_family == AF_INET6)
			ifa = (struct ifaddr *)(ifp->in6_ifaddr);
		else
			ifa = (struct ifaddr *)(ifp->in_ifaddr);
#else
	struct in_ifaddr *ia;
	struct ifaddr *ifa;

	if ((addr->sa_family >= AF_MAX) || (ifp == 0)) {
		return ((struct ifaddr *)0);
	}

	if (addr->sa_family == AF_LINK) {
		return (ifa_ifwithlinkaddr(addr));
	}

	/*
	 * fast optimization case is to check for perfect complete match
	 * with the primary address associated with the network interface.
	 * This avoid the enumeration of the entire IP address hash table.
	 */
	if (ifa = ifaof_ifpforaddr_primary(addr, ifp)) {
		return ifa;
	}

	ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
			ifaof_ifpforaddr_enummatch,
			HTF_INET, /* regular IP address entries only */
			(caddr_t)(&(((struct sockaddr_in *)(addr))->sin_addr)),
			(caddr_t)addr,
			(caddr_t)ifp);

	if (ia) { /* found first match so quit */
		ifa = &(ia->ia_ifa);
	} else {
		/*
		 * Otherwise return primary address if same address family
		 */
		ia = (struct in_ifaddr *)(ifp->in_ifaddr);
		ifa = (ia) ? &(ia->ia_ifa) : (struct ifaddr *)0;
#endif

		if (!ifa || (ifa->ifa_addr->sa_family != addr->sa_family)) {
			ifa = (struct ifaddr *)0;
		}
	}
	return ifa;
}

/*
 * Default action when installing a route with a Link Level gateway.
 * Lookup an appropriate real ifa to point to.
 * This should be moved to /sys/net/link.c eventually.
 */
void
link_rtrequest(int cmd, struct rtentry *rt, struct sockaddr *sa)
{
	struct ifaddr *ifa;
	struct sockaddr *dst;
	struct ifnet *ifp;

	ASSERT(ROUTE_ISWRLOCKED());
	if (cmd != RTM_ADD || ((ifa = rt->rt_ifa) == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) || ((dst = rt_key(rt)) == 0))
		return;

	if (ifa = ifaof_ifpforaddr(dst, ifp)) {

		ifafree(rt->rt_ifa);

		atomicAddUint(&(ifa->ifa_refcnt), 1);
		rt->rt_ifa = ifa;

		if (ifa->ifa_rtrequest && ifa->ifa_rtrequest != link_rtrequest)
			ifa->ifa_rtrequest(cmd, rt, sa);
	}
	return;
}

/*
 * This is a hash enumeration match procedure which calls the function
 * 'pfctlinput' with the 'cmd' integer and the 'struct sockaddr *' which is
 * stored in the hash table entry.
 *
 * if_down_enummatch(struct ifaddr *ifa, struct sockaddr *addr,
 *	int cmd, int *pfctlinput)
 *	int pfctlinput(int, struct sockaddr *))
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/*ARGSUSED*/
static int
if_down_enummatch(struct hashbucket *h, caddr_t key, caddr_t cmd, caddr_t arg)
{
#ifdef INET6
	struct ifaddr *ifa = (struct ifaddr *)h;
#else
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
#endif
	struct ifnet *ifp = (struct ifnet *)key;

#ifdef INET6
	if ((ifa->ifa_ifp == ifp) && (ifa->ifa_addrflags & IADDR_PRIMARY)) {
#else
	if ((ia->ia_ifp == ifp) && (ia->ia_addrflags & IADDR_PRIMARY)) {
#endif
		/*
		 * call function with cmd value and sockaddr address
		 */
#ifdef DEBUG
		if (cmd != (caddr_t)PRC_IFDOWN)
			printf("if_down_enummatch: bogus cmd value %d\n", cmd);
#endif
#ifdef INET6
		pfctlinput(PRC_IFDOWN, ifa->ifa_addr);
#else
		pfctlinput(PRC_IFDOWN, (struct sockaddr *)(&(ia->ia_addr)));
#endif
	}
	return 0;
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
void
if_down(struct ifnet *ifp)
{
	(void)hash_enum(&hashinfo_inaddr,
			if_down_enummatch,
			HTF_INET, /* Strict IP address entries only */
			(caddr_t)ifp, /* arg to enummatch function */
			(caddr_t)PRC_IFDOWN, /* arg to pfctlinput function */
			(caddr_t)0);
#ifdef INET6
	(void)hash_enum(&hashinfo_in6addr,
			if_down_enummatch,
			HTF_INET, /* Strict IP address entries only */
			(caddr_t)ifp, /* arg to enummatch function */
			(caddr_t)PRC_IFDOWN, /* arg to pfctlinput function */
			(caddr_t)0);
#endif
	rt_ifmsg(ifp);
	return;
}

/*
 * Handle interface watchdog timer routines.  Called
 * from sockd, we decrement timers (if set) and 
 * call the appropriate interface routine on expiration.
 */
void
if_slowtimo(void)
{
	register struct ifnet *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		IFNET_UPPERLOCK(ifp);
		if (ifp->if_timer == 0 || --ifp->if_timer) {
			IFNET_UPPERUNLOCK(ifp);
			continue;
		}
		if (ifp->if_watchdog)
			(*ifp->if_watchdog)(ifp);
		IFNET_UPPERUNLOCK(ifp);
	}
}

/*
 * Map interface name to interface structure pointer.
 */
struct ifnet *
ifunit(char *name)
{
	register char *cp;
	register struct ifnet *ifp;
	int unit;
	int namlen = -1;

	for (cp = name;  cp < name+IFNAMSIZ && *cp; cp++) {
		if (*cp < '0' || *cp > '9') {
			namlen = -1;
			continue;
		}
		if (namlen < 0) {
			namlen = cp-name;
			unit = 0;
		}
		unit = unit*10 + *cp - '0';
	}
	if (namlen <= 0)		/* bad if all digits or none */
		return (0);

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		/* Check interface name before unit number cuz most
		 * interfaces start with unit # 0.
		 */
		if (!bcmp(ifp->if_name, name, namlen)
		    && ifp->if_name[namlen] == '\0'
		    && unit == ifp->if_unit)
			break;
	}
	return ifp;
}

#if (defined(sgi) && _MIPS_SIM == _ABI64)

static struct ifconf *
irix5_to_ifconf(struct ifconf *user_ic, struct ifconf *native_ic)
{
	struct irix5_ifconf *i5_ic;
	char abi = get_current_abi();

	if (ABI_IS_IRIX5_64(abi))
		return user_ic;

	ASSERT(ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi));

	i5_ic = (struct irix5_ifconf *)user_ic;

	native_ic->ifc_len = i5_ic->ifc_len;
	native_ic->ifc_ifcu.ifcu_req =
		(struct ifreq *)(__psint_t)i5_ic->ifc_ifcu.ifcu_req;

	return native_ic;
}

static struct ifconf *
ifconf_to_irix5(struct ifconf *native_ic, struct ifconf *user_ic)
{
	struct irix5_ifconf *i5_ic;
	int abi = get_current_abi();

	if (ABI_IS_IRIX5_64(abi))
		return native_ic;

	ASSERT(ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi));

	i5_ic = (struct irix5_ifconf *)user_ic;

	i5_ic->ifc_len = native_ic->ifc_len;
	i5_ic->ifc_ifcu.ifcu_req =
		(app32_ptr_t)(__psint_t)native_ic->ifc_ifcu.ifcu_req;

	return user_ic;
}

#endif /* (defined(sgi) && _MIPS_SIM == _ABI64) */

/*
 * Interface ioctls.
 */
int
ifioctl(struct socket *so, int cmd, caddr_t data)
{
	register struct ifnet *ifp;
	register struct ifreq *ifr;
	int error = 0;
	int flags_on;
	struct ifreq ifr2;
	struct ifdatareq *ifd = (struct ifdatareq *)data;

	ASSERT(SOCKET_ISLOCKED(so));	
	switch (cmd) {

#if _MIPS_SIM == _ABI64
	case IRIX5_SIOCGIFCONF:
#endif
	case SIOCGIFCONF:
		return (ifconf(cmd, data));

	case SIOCSARP:
	case SIOCDARP:
	case SIOCSARPX:
	case SIOCDARPX:
		if ( !_CAP_ABLE(CAP_NETWORK_MGT) )
			return (EPERM);
		/* FALL THROUGH */
	case SIOCGARP:
	case SIOCGARPX:
		return (arpioctl(cmd, data));

	case SIOCATMARP:
		return(new_atmarp_ioctl(cmd, data));

#ifdef sgi
        case SIOCIFISPSCTL:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		return (rsvp_ioctl(data));
#endif
	}
	if (cmd == SIOCGIFDATA) {
		ifd = (struct ifdatareq *)data;
		ifp = ifunit(ifd->ifd_name);
	} else {
		ifr = (struct ifreq *)data;
		ifp = ifunit(ifr->ifr_name);
	}
	if (ifp == 0)
		return (ENXIO);
	IFNET_LOCK(ifp);

	/* 
	 * TRUSTEDIRIX: Audit attempt to change config,
	 * whether successful or not.
	 */
	switch (cmd) {
	default:
		break;

	case SIOCSIFFLAGS:
	case SIOCSIFSEND:
	case SIOCSIFRECV:
	case SIOCSIFMETRIC:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFSTATS:
	case SIOCSIFHEAD:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
		    error = EPERM;
		/* If an ioctl is called before p0init is executed, 
		 * _SAT_BSDIPC_IF_CONFIG will cause a tlb miss because
		 * curuthread is not defined until then.  Therefore, if
		 * curuthread is NULL, we drop through this case
		 */
		if (!curuthread)
		    break;
		_SAT_BSDIPC_IF_CONFIG(*curuthread->ut_scallargs, so,
					cmd, ifr, error);
		break;
	}

	switch (cmd) {

	case SIOCGIFFLAGS:
		ifr->ifr_flags = ifp->if_flags;
		break;

	case SIOCGIFDATA:
		bcopy(&ifp->if_data, &ifd->ifd_ifd, sizeof(ifd->ifd_ifd));
		break;

	case OSIOCGIFFLAGS:
		*((short*)(&ifr->ifr_flags)) = (short)ifp->if_flags;
		break;

	case SIOCGIFRECV:
		ifr->ifr_perf = ifp->if_recvspace;
		break;

	case SIOCGIFSEND:
		ifr->ifr_perf = ifp->if_sendspace;
		break;

	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;

	case SIOCSIFSEND:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			/* XXX isn't this covered above? */
			error = EPERM;
			break;
		}
		ifp->if_sendspace = ifr->ifr_perf;
		break;

	case SIOCSIFRECV:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			/* XXX isn't this covered above? */
			error = EPERM;
			break;
		}
		ifp->if_recvspace = ifr->ifr_perf;
		break;

	case OSIOCSIFFLAGS:
		ifr = &ifr2;
		bcopy(data, &ifr2, sizeof(struct ifreq));
		data = (caddr_t)&ifr2;
		ifr->ifr_flags >>= 16;
		ifr->ifr_flags &= 0xffff;	/* clear extraneous bits */
		cmd = SIOCSIFFLAGS;
		/* FALL THROUGH */

	case SIOCSIFFLAGS:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		flags_on = ifr->ifr_flags & ~ifp->if_flags;
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(ifr->ifr_flags &~ IFF_CANTCHANGE);

		if ((ifr->ifr_flags ^ ifp->if_flags) & IFF_UP)
			ifp->if_lastchange = time;

		if (ifp->if_ioctl)
			(void) (*ifp->if_ioctl)(ifp, cmd, data);
		/* if the interface was turned on, tell routing daemons */
		if ((flags_on & ifp->if_flags & IFF_UP) != 0)
			rt_ifmsg(ifp);
		break;

	case SIOCSIFMETRIC:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		ifp->if_metric = ifr->ifr_metric;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
		    	break;
		}
		if (ifp->if_ioctl == NULL) {
			error = EOPNOTSUPP;
		    	break;
		}
		error = (*ifp->if_ioctl)(ifp, cmd, &ifr->ifr_addr);
		IFNET_UNLOCK(ifp);
		return(error);

	case SIOCSIFSTATS:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		ifp->if_ipackets = ifr->ifr_stats.ifs_ipackets;
		ifp->if_ierrors = ifr->ifr_stats.ifs_ierrors;
		ifp->if_opackets = ifr->ifr_stats.ifs_opackets;
		ifp->if_oerrors = ifr->ifr_stats.ifs_oerrors;
		ifp->if_collisions = ifr->ifr_stats.ifs_collisions;
		break;

	case SIOCGIFSTATS:
		ifr->ifr_stats.ifs_ipackets = ifp->if_ipackets;
		ifr->ifr_stats.ifs_ierrors = ifp->if_ierrors;
		ifr->ifr_stats.ifs_opackets = ifp->if_opackets;
		ifr->ifr_stats.ifs_oerrors = ifp->if_oerrors;
		ifr->ifr_stats.ifs_collisions = ifp->if_collisions;
		break;

	case SIOCSIFHEAD:
		{
		struct ifnet **ifpp, *ifpc;

		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		IFHEAD_LOCK();
		for (ifpp = &ifnet, ifpc = ifnet; ifpc != ifp;
		     ifpp = &ifpc->if_next, ifpc = ifpc->if_next) {
			ASSERT(ifpc->if_next != 0);
		}
		*ifpp = ifpc->if_next;
		ifp->if_next = ifnet;
		ifnet = ifp;
		IFHEAD_UNLOCK();
		break;
		}
 
	case SIOCVIF:		/* logical driver operations */
	case SIOCVIFMAP:
	case SIOCVIFSTAT:
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
		    	break;
		}
		if (ifp->if_ioctl == NULL) {
			error = EOPNOTSUPP;
		    	break;
		}
		error = (*ifp->if_ioctl)(ifp, cmd, data);
	    	break;

	default:
		if (so->so_proto == 0) {
			error = EOPNOTSUPP;
			break;
		}
		IFNET_UNLOCK(ifp);
		error = ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
		/*
		 * Warning: the command code is passed down to the control
		 * function via the first arg, which means we need to widen
		 * it to a pointer for passing. It must be narrowed back
		 * to an int in the PRU_CONTROL arm of this function.
		 * The add and cast removes compiler warnings.
		 */
			(struct mbuf *)(NULL+cmd), (void *)data, (void *)ifp));
		return (error);
	}
	IFNET_UNLOCK(ifp);
	return (error);
}

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 */
/*ARGSUSED*/
int
ifconf(int cmd, caddr_t data)
{
	register struct ifconf *ifc = (struct ifconf *)data;
#if _MIPS_SIM == _ABI64
	struct ifconf native_ifconf;
#endif
	register struct ifnet *ifp = ifnet;
	struct in_ifaddr *ia;
	char *cp, *ep;
	struct ifreq ifr, *ifrp;
	int space;
	int error = 0;

	XLATE_FROM_IRIX5(irix5_to_ifconf, ifc, &native_ifconf);
	space = ifc->ifc_len;

	ifrp = ifc->ifc_req;
	ep = ifr.ifr_name + sizeof (ifr.ifr_name) - 2;

	for (; space > sizeof (ifr) && ifp; ifp = ifp->if_next) {

		/* Create interface name along with unit number in buffer */
		(void)strncpy(ifr.ifr_name, ifp->if_name, sizeof ifr.ifr_name);
		for (cp = ifr.ifr_name; cp < ep && *cp; cp++)
			continue;
		sprintf(cp, "%d", ifp->if_unit);	/* may be > 9 */

		/*
		 * return primary address for interface so we either copy the
		 * primary interface address into the buffer or zero it.
		 */
#ifdef INET6
		/*
		 * To maintain binary backwards compatibility we only
		 * return ipv4 addresses so don't bother looking at
		 * in6_ifaddr.
		 */
#endif
		if ((ia = (struct in_ifaddr *)ifp->in_ifaddr)) {
			ifr.ifr_addr = *(ia->ia_ifa.ifa_addr);
		} else {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
		}

		/*
		 * copy back to caller name and primary i/f address
		 */
#ifndef _HAVE_SA_LEN
		error = copyout(&ifr, ifrp, sizeof(ifr));
		if (error)
			break;
		space -= sizeof (ifr), ifrp++;
#else
		{
		register struct sockaddr *sa = ifa->ifa_addr;

		if (sa->sa_len <= sizeof(*sa)) {
			ifr.ifr_addr = *sa;
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
					sizeof (ifr));
			ifrp++;
		} else {
			space -= sa->sa_len - sizeof(*sa);
			if (space < sizeof (ifr))
				break;
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
					sizeof (ifr.ifr_name));
			if (error == 0)
			    error = copyout((caddr_t)sa,
			      (caddr_t)&ifrp->ifr_addr, sa->sa_len);
			ifrp = (struct ifreq *)
				(sa->sa_len+ (caddr_t)&ifrp->ifr_addr);
		}
		if (error)
			break;
		space -= sizeof (ifr);
		}
#endif /* _HAVE_SA_LEN */
	} /* end of for ifnet loop */
	ifc->ifc_len -= space;

	XLATE_TO_IRIX5(ifconf_to_irix5, (struct ifconf *)data, ifc);
	return (error);
}

/*
 * flush an IF queue, return number of entries dropped
 */
int
if_dropall(struct ifqueue* ifq)
{
  	NETSPL_DECL(s1)
	struct mbuf *m;
	int i = 0;

	IFQ_LOCK(ifq, s1);
	for (;;) {
		IF_DEQUEUE_NOLOCK(ifq, m);
		if (m == 0) {
			IFQ_UNLOCK(ifq, s1);
			return (i);
		}
		m_freem(m);
		IF_DROP(ifq);
		i++;
	}
}

vertex_hdl_t if_hwgraph_netvhdl = GRAPH_VERTEX_NONE;

graph_error_t
if_init_hwgraph(void)
{
	graph_error_t rc;
	if (if_hwgraph_netvhdl != GRAPH_VERTEX_NONE) {
		return GRAPH_SUCCESS;
	}
	rc = hwgraph_path_add(GRAPH_VERTEX_NONE, EDGE_LBL_NET, 
		&if_hwgraph_netvhdl);
	if (rc != GRAPH_SUCCESS) {
		cmn_err_tag(166,CE_WARN, "couldn't create /hw/net vertex");
		return GRAPH_CANNOT_ALLOC;
	}
	return GRAPH_SUCCESS;
}
	
/*
 * Like hwgraph_char_device_add but also adds an alias into /hw/net.
 */
graph_error_t
if_hwgraph_add(
	vertex_hdl_t parent, 
	char *path, 
	char *prefix,
	char *alias,
	vertex_hdl_t *ret
)
{
	graph_error_t rc;

	if (if_hwgraph_netvhdl == GRAPH_VERTEX_NONE) {
		rc = if_init_hwgraph();
		if (rc != GRAPH_SUCCESS) {
			return rc;
		}
	}
	rc = hwgraph_char_device_add(parent, path, prefix, ret);
	if (rc == GRAPH_SUCCESS) {
		rc = hwgraph_edge_add(if_hwgraph_netvhdl, *ret, alias);
	}
	return rc;
}

/*
 * Called to add an alias in /hw/net (for drivers with pre-existing hwgraph
 * support).
 */

graph_error_t
if_hwgraph_alias_add(vertex_hdl_t target, char *alias)
{
	graph_error_t rc;
	if (if_hwgraph_netvhdl == GRAPH_VERTEX_NONE) {
		rc = if_init_hwgraph();
		if (rc != GRAPH_SUCCESS) {
			return rc;
		}
	}
	return hwgraph_edge_add(if_hwgraph_netvhdl, target, alias);
}

/*
 * Called to remove an alias in /hw/net if a driver is removed from the
 * system.
 */
graph_error_t
if_hwgraph_alias_remove(char *path)
{
	vertex_hdl_t	netvhdl;
	graph_error_t	rc;

	if (if_hwgraph_netvhdl == GRAPH_VERTEX_NONE) {
		return GRAPH_NOT_FOUND;
	}

	rc = hwgraph_traverse(if_hwgraph_netvhdl, path, &netvhdl);
	if (rc == GRAPH_SUCCESS) {
		rc = hwgraph_edge_remove(if_hwgraph_netvhdl, path, 0);
	}
	return rc;
}
