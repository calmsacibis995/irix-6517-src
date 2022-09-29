/*
 * Copyright (c) 1985, 1986, 1993
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
 *	@(#)in_var.h	8.1 (Berkeley) 6/10/93
 */

#ifndef __NETINET_IN_VAR_H__
#define __NETINET_IN_VAR_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/hashing.h>

/*
 * Interface address, Internet version.  One of these structures
 * is allocated for each interface with an Internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */
struct in_ifaddr {
#ifndef INET6
	struct	hashbucket ia_hashbucket;	/* hash bucket header */
#endif
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#ifdef INET6
#define	ia_hashbucket	ia_ifa.ifa_hashbucket
#define ia_addrflags	ia_ifa.ifa_addrflags
#endif
#define	ia_ifp		ia_ifa.ifa_ifp
#define ia_flags	ia_ifa.ifa_flags
					/* ia_{,sub}net{,mask} in host order */
	__uint32_t	ia_net;		/* network number of interface */
	__uint32_t	ia_netmask;	/* mask of net part */
	__uint32_t	ia_subnet;	/* subnet number, including net */
	__uint32_t	ia_subnetmask;	/* mask of net + subnet */
	struct	in_addr ia_netbroadcast; /* to recognize net broadcasts */

	struct	sockaddr_in ia_addr;	/* address of interface */
	struct	sockaddr_in ia_dstaddr; /* other end p-to-p link bcast addr */
#define	ia_broadaddr	ia_dstaddr

#ifdef _HAVE_SIN_LEN
	struct	sockaddr_in ia_sockmask; /* reserve space for general netmask*/
#else
	struct	sockaddr_in_new ia_sockmask; 
#endif /* _HAVE_SIN_LEN */
#ifndef INET6
	int		ia_addrflags;	/* primary or alias address flags */
#endif
};

struct	in_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_in ifra_addr;
	struct	sockaddr_in ifra_broadaddr;
#define ifra_dstaddr ifra_broadaddr
#ifdef _HAVE_SIN_LEN
	struct	sockaddr_in ifra_mask;
#else
	struct	sockaddr_in_new ifra_mask;
#endif
};

/*
 * Given a pointer to an in_ifaddr (ifaddr),
 * return a pointer to the addr as a sockaddr_in.
 */
#define	IA_SIN(ia) (&(((struct in_ifaddr *)(ia))->ia_addr))

#ifndef INET6
/*
 * ia_addrflags
 */
#define	IADDR_PRIMARY	0x1	/* Primary address entry for ifp */
#define	IADDR_ALIAS	0x2	/* Alias address entry for ifp */
#endif

extern struct ifqueue ipintrq;		/* ip packet input queue */

#ifdef _HAVE_SIN_LEN
void	in_socktrim(struct sockaddr_in *);
#else
void	in_socktrim(struct sockaddr_in_new *);
#endif

#ifdef _KERNEL
/*
 * Internet broadcast address structure, Internet version.
 * There are one of these entries for each unique broadcast address assigned
 * to this host as either a primary or alias address. These entries are kept
 * in a separate hash table containing only broadcast entries and not mixed
 * with the unicast and multicast hash table, since these are really a subset
 * of a unicast address entry.
 */
struct in_bcast {
	struct hashbucket hashbucket;	/* hash bucket header */

	__uint32_t	ia_net;		/* network number of interface */
	__uint32_t	ia_netmask;	/* network mask for net portion */
	__uint32_t	ia_subnet;	/* subnet number, including net */
	__uint32_t	ia_subnetmask;	/* mask of net + subnet */
	struct	in_addr ia_netbroadcast; /* to recognize net broadcasts */
	struct	sockaddr_in ia_addr;	/* unicast addr assoc with bcast */
	struct	sockaddr_in ia_dstaddr;	/* broadcast IP address */

	struct  ifnet   *ifp;		/* address of interface info */
	__uint32_t	refcnt;		/* num unicast addr's with same bcast*/
};

extern int inaddr_match(struct hashbucket *, caddr_t, caddr_t, caddr_t);

extern int in_broadcast(struct in_addr, struct ifnet *);
extern int in_localaddr(struct in_addr);

extern int ifnet_enummatch(struct hashbucket *, caddr_t, caddr_t, caddr_t);
extern struct ifnet *inaddr_to_ifp(struct in_addr);

extern	struct in_multi *in_addmulti(struct in_addr, struct ifnet *);
extern	void in_delmulti(struct in_multi *);
extern	int in_multi_match(struct hashbucket *, caddr_t, caddr_t, caddr_t);
extern	int in_control(struct socket *, __psint_t, caddr_t, struct ifnet *);
extern	int in_rtinit(struct ifnet *);

/*
 * Global data structures
 */
extern struct hashinfo hashinfo_inaddr;
extern struct hashinfo hashinfo_inaddr_bcast;

extern struct hashtable hashtable_inaddr[];
extern struct hashtable hashtable_inaddr_bcast[];
#endif /* _KERNEL */

/*
 * Macro for finding the interface (ifnet structure) corresponding to one
 * of our IP addresses.
 */
#define INADDR_TO_IFP(addr, ifp)	\
	/* struct in_addr addr; */	\
	/* struct ifnet  *ifp;  */	\
{					\
	(ifp) = inaddr_to_ifp((addr));	\
}

/*
 * Macro for finding the internet address structure (in_ifaddr) corresponding
 * to a given interface (ifnet structure).
 */
#define IFP_TO_IA(ifp, ia)						  \
	/* struct ifnet     *ifp; */					  \
	/* struct in_ifaddr *ia;  */					  \
{									  \
        (ia) = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,            \
			ifnet_enummatch, HTF_INET, 0, (caddr_t)ifp, 0);   \
}

/*
 * Per-interface router version information is kept in this list.
 * This information should be part of the ifnet structure but we don't wish
 * to change that - as it might break a number of things
 */
struct router_info {
	struct router_info *next;
	struct ifnet *ifp;

	int	type;	/* type of router which is querier on this interface */
	int	time;	/* # of slow timeouts since last old query */
};

/*
 * Internet multicast address structure.  There is one of these for each IP
 * multicast group to which this host belongs on a given network interface.
 * They are kept in a hash table containing both regular and multicast entries
 */
struct in_multi {
	struct hashbucket hashbucket;	/* hash bucket header */

	struct in_addr	  inm_addr;	/* IP multicast address             */
	struct ifnet     *inm_ifp;	/* back pointer to ifnet            */
	struct router_info *inm_rti;	/* router info */

	u_int		  inm_state;	/* state of the membership	    */
	u_int		  inm_refcount;	/* no. membership claims by sockets */
	u_int		  inm_timer;	/* IGMP membership report timer     */
};

#ifdef _KERNEL
/*
 * Macro for looking up the in_multi record for a given IP multicast address
 * on a given interface.  If no matching record is found, "inm" returns NULL.
 */
#define IN_LOOKUP_MULTI(addr, ifp, inm)					    \
	/* struct in_addr  addr; */					    \
	/* struct ifnet    *ifp; */					    \
	/* struct in_multi *inm; */					    \
{									    \
	(inm) = (struct in_multi *)hash_lookup(&hashinfo_inaddr,            \
		in_multi_match, (caddr_t)(&(addr)),                         \
		(caddr_t)(ifp), (caddr_t)0);                                \
}

/*
 * Declare to get symbols defined
 */
struct	socket;

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif /* __NETINET_IN_VAR_H__ */
