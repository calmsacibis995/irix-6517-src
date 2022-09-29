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

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"
#include "sys/atomic_ops.h"
#include "sys/kmem.h"

#include "sys/errno.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/kthread.h"
#include "sys/systm.h"
#include "sys/debug.h"

#include "net/soioctl.h"
#include "net/if.h"
#include "net/route.h"
#include "net/if_types.h"  /* IFT_HIPPI, IFT_GSN */

#include "in_systm.h"
#include "in.h"
#include "in_var.h"
#include "ip_var.h"
#ifdef INET6
#include "in6_var.h"
#endif

#include "igmp_var.h"
#include "sys/capability.h"

#define	SATOSIN(sa)	((struct sockaddr_in *)(sa))
#define	SATOSIN_NEW(sa)	((struct sockaddr_in_new *)(sa))

/*
 * Forward reference procedures
 */
void ifafree_common(struct ifaddr *, int);
int ifafree_refcnt(struct hashbucket *h, caddr_t, caddr_t);

int in_ifinit(struct ifnet *, struct in_ifaddr *, struct sockaddr_in *,
	int, int);
void in_ifscrub(struct ifnet *, struct in_ifaddr *);

int inaddr_match(struct hashbucket *, caddr_t, caddr_t, caddr_t);

struct in_bcast *in_bcast_alloc(struct in_ifaddr *);
int in_bcast_adjust_refcnt(struct hashbucket *, caddr_t, caddr_t);
int in_bcast_broadcast_match(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2);
int in_bcast_find_match(struct hashbucket *, caddr_t, caddr_t, caddr_t);
int in_bcast_free_refcnt(struct hashbucket *, caddr_t, caddr_t);
int in_bcast_iaddr_refcnt(struct hashbucket *, caddr_t, caddr_t);
int in_bcast_net_match(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2);
void in_bcast_update(struct in_bcast *, struct in_ifaddr *);

int in_broadcast_match(struct hashbucket *, caddr_t, caddr_t, caddr_t);

int in_getalias_enummatch(struct hashbucket *, caddr_t, caddr_t, caddr_t);

/*
 * External Procedures
 */
extern int ip_accept_primary_broadcast(struct in_addr *);
extern int ip_check_broadcast(struct in_ifaddr *, struct in_addr *);

extern void rawif_fixlink(struct ifnet *);
extern void link_rtrequest(int, struct rtentry *, struct sockaddr *);

/*
 * External Variables
 */
extern int ipalias_count;	/* number configured IP alias address'es */

/*
 * Module defined global data structures
 *
 * hashtable_inaddr contains the head and write lock for each hash chain
 */
struct hashinfo hashinfo_inaddr;
struct hashtable hashtable_inaddr[HASHTABLE_SIZE];

struct hashinfo hashinfo_inaddr_bcast;
struct hashtable hashtable_inaddr_bcast[HASHTABLE_SIZE];

int	in_interfaces;		/* number of external internet interfaces */
int	in_ifaddr_count;	/* number configured address'es including lb */

/*
 * External variables
 */
extern struct ifnet loif;	/* loop-back interface ifnet structure */
extern int subnetsarelocal;
extern time_t time;
extern zone_t *ipmember_zone;

/* ARGSUSED */
int
ifafree_check1(struct in_ifaddr *ia)
{
	return 1;
}

/* ARGSUSED */
int
ifafree_check2(struct in_ifaddr *ia)
{
	return 1;
}

/* ARGSUSED */
int
ifafree_check3(struct in_ifaddr *ia)
{
	return 1;
}

/* ARGSUSED */
int
ifafree_check4(struct in_ifaddr *ia)
{
	return 1;
}

/* ARGSUSED */
int
ifafree_check5(struct in_ifaddr *ia, struct in_ifaddr *ia_start)
{
	return 1;
}

/*
 * This is a hash lookup match procedure which compares the ifaddr
 * fields of the in_ifaddr structure passed in so that we can find
 * the hash table entry corresponding to this real address.
 *
 * int
 * ifafree_match(struct hashbucket *h, struct in_addr *key,
 *	struct ifnet *arg1, sockaddr_in *arg2)
 *
 * Parameters:
 *  key is a pointer to the struct in_addr, containing the IP address
 *   to lookup in the hash table.
 *  arg1 is the ifnet pointer
 *  arg2 is a NULL pointer
 *
 * NOTE: This procedure is called with ALL types of hash bucket entries which
 * are linked onto the hash chain, so we explictly check for the HTF_INET ones.
 */
/* ARGSUSED */
int
ifafree_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia;
	struct in_addr *addr;
	struct ifnet *ifp;
	int match;

	if ((h->flags & HTF_INET) == 0) { /* not unicast entry */
		return 0;
	}

	ia = (struct in_ifaddr *)h;
	ifp = (struct ifnet *)arg1;
	/*
	 * NOTE: 'addr' is the new IP address since the in_ifaddr structure
	 * has already been updated. Without this we can't be sure we found
	 * the correct entry since it was inserted using the old IP address
	 * to compute the hash table index.
	 */
	addr = (struct in_addr *)arg2;

	if ((ia->ia_ifp == ifp) &&
	    (SATOSIN(&ia->ia_addr)->sin_addr.s_addr == addr->s_addr)) {
		match = 1;
		ifafree_check3(ia);
	} else {
		match = 0;
	}
	return match;
}

/*
 * Free procedure for an ifaddr structure
 * NOTES:
 * 1. We assume on entry that the 'ifa' address is valid
 * 2. The ref count in the ifaddr structure is valid
 * 3. Upon entry we are holding the routing table write lock.
 */
void
ifafree(struct ifaddr *ifa)
{
	/* no forced delink, normal reference counting delink/release */
	ifafree_common(ifa, 0);
	return;
}

/*
 * The common ifafree procedure, to handle both cases:
 * NOTES:
 * 1. The normal free operation on ifaddr structures where the entry
 *    is delinked from the hash table and the storage freed IFF the refcnt
 *    indicates this is the last free operation.
 * 2. The forced delink operation on ifaddr structures where the entry
 *    is always delinked from the hash table, required for the SIOCDIFADDR
 *    ioctl operation. The storage is deallocated as per the refcnt
 *    like in case 1 above.
 * 3. Upon entry we are holding the routing table write lock.
 */
void
ifafree_common(struct ifaddr *ifa, int force_delink)
{
	struct in_ifaddr *ia, *ia_start;

	ia_start = (struct in_ifaddr *)ifa->ifa_start_inifaddr;
	if (ia_start == NULL) {
		panic("ifafree: ifa_start_inifaddr NULL: ia 0x%x, ifa 0x%x\n",
			ia_start, ifa);
	}
	if (ia_start->ia_hashbucket.flags & HTF_NOTINTABLE) { /* not in table */

		ifafree_check2(ia_start);
		if (ia_start->ia_ifa.ifa_refcnt == 0) {
			ia = ia_start;
		} else {
			ia = ifafree_refcnt(&(ia_start->ia_hashbucket), 0, 0)
				? ia_start : 0;
		}
		ifafree_check5(ia, ia_start);
	} else {
		/*
		 * The hash_refcnt procedure executes under the write lock
		 * for the hash chain.
		 * It first calls 'ifafree_refcnt' to perform the atomic
		 * refcnt decrement. The 'ifafree_refcnt' procedure returns
		 * the address of the entry if refcnt < 1 indicating we can
		 * deallocate the storage or zero if not.
		 */
		if (force_delink) {
			ia = (struct in_ifaddr *)hash_remove_refcnt(
					&hashinfo_inaddr,
					ifafree_match,
					ifafree_refcnt,
					(caddr_t)&(ia_start->ia_addr.sin_addr),
					(caddr_t)(ia_start->ia_ifp),
					(caddr_t)&(ia_start->ia_addr.sin_addr)
					);
		} else {
			ia = (struct in_ifaddr *)hash_refcnt(
					&hashinfo_inaddr,
					ifafree_refcnt,
					&(ia_start->ia_hashbucket));
		}
	}
	if ((ia_start->ia_ifa.ifa_refcnt < 1) && (ia == 0)) {
		ifafree_check4(ia_start);
	}
	if (ia) {
		mcb_free(ia, sizeof(struct in_ifaddr), MT_IFADDR);
	}
	return;
}

/*
 * This is the generic in_ifaddr address specific hash refcnt function.
 * It returns 1 if refcnt of entry is now <= 1 or ZERO after a refcnt
 * decrement operation.
 *
 * NOTE: This function is called from the generic hash reference count
 * procedure holding the write lock on the hash chain this entry is linked.
 */
/* ARGSUSED */
int
ifafree_refcnt(struct hashbucket *h, caddr_t arg1, caddr_t arg2)
{
	register struct in_ifaddr *ia = (struct in_ifaddr *)h;
	register int new_refval;

	if (ia->ia_ifa.ifa_refcnt == 0) {
		ifafree_check1(ia);
		return 1; /* don't free again */
	}
	new_refval = atomicAddUint(&(ia->ia_ifa.ifa_refcnt), -1);
	return ((new_refval < 1) ? 1 : 0);
}

/*
 * Initialization procedure for network interface utilities
 */
void
in_init(void)
{
#ifdef INET6
	extern void in6_init(void);
#endif

	ipmember_zone = kmem_zone_init(sizeof(struct ip_membership),"ipmulti");

	hash_init(&hashinfo_inaddr, hashtable_inaddr,
		HASHTABLE_SIZE, sizeof(struct in_addr), HASH_MASK);
	hash_init(&hashinfo_inaddr_bcast, hashtable_inaddr_bcast,
		HASHTABLE_SIZE, sizeof(struct in_addr), HASH_MASK);
#ifdef INET6
	in6_init();
#endif
	return;
}

/*
 * This is a hash lookup match procedure which compares the ia_addr
 * fields of the in_ifaddr structure passed in so that we can
 * find the real inifaddr entry if one exists.
 *
 * int
 * in_adjust_match(struct hashbucket *h, struct in_addr *key,
 *	struct ifnet *arg1, sockaddr_in *arg2)
 *
 * Parameters:
 *  key is a pointer to the old struct in_addr, containing the IP address
 *   that was used to insert this entry.
 *  arg1 is the ifnet pointer
 *  arg2 is a pointer to the new IP address
 *
 * NOTE: This procedure is called with ALL types of hash bucket entries which
 * are linked onto the hash chain, so we explictly check for the HTF_INET ones.
 */
/* ARGSUSED */
int
in_adjust_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia;
	struct in_addr *addr;
	struct ifnet *ifp;
	int match;

	if ((h->flags & HTF_INET) == 0) { /* not unicast entry */
		return 0;
	}

	ia = (struct in_ifaddr *)h;
	ifp = (struct ifnet *)arg1;
	/*
	 * NOTE: 'addr' is the new IP address since the in_ifaddr structure
	 * has already been updated. Without this we can't be sure we found
	 * the correct entry since it was inserted using the old IP address
	 * to compute the hash table index.
	 */
	addr = (struct in_addr *)arg2;

	if ((ia->ia_ifp == ifp) &&
	    (SATOSIN(&ia->ia_addr)->sin_addr.s_addr == addr->s_addr)) {
		match = 1;
	} else {
		match = 0;
	}
	return match;
}

/*
 * This procedure adjusts the hash table entry for the ia_addr inside the
 * the 'struct in_ifaddr' since the address is different than when it was
 * originally inserted into the ip address hash table.
 * The algorithm performs a hash lookup using the original value, removes
 * the old entry and inserts it back into the hash table using the new address
 * as the value of the hash key to compute it's new slot.
 *
 * NOTE: This procedure assumes the 'ia' structure has been updated to contain
 * the new address'es, pointers to the old values are input parameters.
 * We also assume that the address'es are different since we're adjusting
 * only the first/primary address associated with a network interface.
 */
void
ia_addr_adjust_entry(struct ifnet *ifp,
	struct in_ifaddr *ia,
	struct sockaddr_in *ia_addr_old)
{
	struct in_ifaddr *ia_old;
	/*
	 * different address so do some more checking to see
	 * if we have an interface support broadcast addresses.
	 */
	ia_old = (struct in_ifaddr *)hash_lookup(
			&hashinfo_inaddr,
			in_adjust_match,
			(caddr_t)&(ia_addr_old->sin_addr),
			(caddr_t)ifp,
			(caddr_t)&(ia->ia_addr.sin_addr));

	if (ia_old) {
		hash_remove(&hashinfo_inaddr, &(ia_old->ia_hashbucket));

		(void)hash_insert(&hashinfo_inaddr,
			&(ia_old->ia_hashbucket),
			(caddr_t)&(ia->ia_addr.sin_addr));
	}
	return;
}

/* ARGSUSED */
int
in_bcast_adjust_check(struct in_ifaddr *ia,
	struct sockaddr_in *ia_broadaddr_old,
	struct in_bcast *in_bcast_old,
	struct ifnet *ifp)
{
	return 1;
}
  
/* ARGSUSED */
int
in_bcast_adjust_check1(struct in_ifaddr *ia,
	struct sockaddr_in *ia_broadaddr_old,
	struct in_bcast *in_bcast_old,
	struct ifnet *ifp)
{
	return 1;
}
  
/* ARGSUSED */
int
in_bcast_adjust_check2(struct in_ifaddr *ia,
	struct sockaddr_in *ia_broadaddr_old,
	struct in_bcast *in_bcast_old,
	struct ifnet *ifp)
{
	return 1;
}
  
/* ARGSUSED */
int
in_bcast_adjust_check3(struct in_ifaddr *ia,
	struct sockaddr_in *ia_broadaddr_old,
	struct in_bcast *in_bcast_old,
	struct ifnet *ifp)
{
	return 1;
}
  
/* ARGSUSED */
int
in_bcast_adjust_check4(struct in_ifaddr *ia,
	struct sockaddr_in *ia_broadaddr_old,
	struct in_bcast *in_bcast_old,
	struct ifnet *ifp)
{
	return 1;
}
  
/* ARGSUSED */
int
in_bcast_adjust_check5(struct in_ifaddr *ia,
	struct sockaddr_in *ia_broadaddr_old,
	struct in_bcast *in_bcast_old,
	struct ifnet *ifp)
{
	return 1;
}
  
/* ARGSUSED */
int
in_bcast_adjust_check10(struct in_ifaddr *ia,
	struct in_addr *in_netbroadcast_old,
	struct in_bcast *in_bcast_net_old,
	struct ifnet *ifp)
{
	return 1;
}

/* ARGSUSED */
int
in_bcast_adjust_check11(struct in_ifaddr *ia,
	struct in_addr *in_netbroadcast_old,
	struct in_bcast *in_bcast_net_old,
	struct ifnet *ifp)
{
	return 1;
}

/* ARGSUSED */
int
in_bcast_adjust_check12(struct in_ifaddr *ia,
	struct in_addr *in_netbroadcast_old,
	struct in_bcast *in_bcast_net_old,
	struct ifnet *ifp)
{
	return 1;
}

/* ARGSUSED */
int
in_bcast_adjust_check13(struct in_ifaddr *ia,
	struct in_addr *in_netbroadcast_old,
	struct in_bcast *in_bcast_net_old,
	struct ifnet *ifp)
{
	return 1;
}

/* ARGSUSED */
int
in_bcast_adjust_check14(struct in_ifaddr *ia,
	struct in_addr *in_netbroadcast_old,
	struct in_bcast *in_bcast_net_old,
	struct ifnet *ifp)
{
	return 1;
}

/*
 * This procedure is used to adjust/update the hash table entries for
 * either/both the broadcast address associated with the
 * 'struct in_ifaddr' IF the value of either address'es are different OR
 * the ia_addr field changed from when they were originally inserted into
 * the IP broadcast hash table.
 * The algorithm performs a hash lookup using the original value, removes
 * the old entry and inserts it back into the hash table using the new address
 * as the value of the hash key to compute it's new slot. There are a few
 * more details since the update must happen under the write lock for that
 * hash chain.
 *
 * NOTE: This procedure assumes the 'ia' structure has been updated to contain
 * the new address'es, pointers to the old values are input parameters.
 * Network interfaces, such as ppp, which don't support IFF_BROADCAST won't
 * have broadcast address entries, so this procedure isn't called.
 */
void
in_bcast_adjust_entry(struct ifnet *ifp,
	struct in_ifaddr *ia,
	struct sockaddr_in *ia_addr_old,
	struct sockaddr_in *ia_broadaddr_old,
	struct in_addr *in_netbroadcast_old)
{
	struct sockaddr_in ia_addr_new;
	struct in_bcast *in_bcast_old, *in_bcast_net_old;
	struct in_bcast *in_bcast_new, *in_bcast_net_new;
	struct in_bcast *in_bcast_dup, *in_bcast_net_dup;

	/*
	 * Adjust full broadcast entry if primary address changed along
	 * with adjusting the broadcast entries.
	 */
	if (ia->ia_broadaddr.sin_addr.s_addr ==
		ia_broadaddr_old->sin_addr.s_addr) { /* same as before */

		if (ia->ia_addr.sin_addr.s_addr !=
		    ia_addr_old->sin_addr.s_addr) { /* different ia_addr */

			in_bcast_adjust_check(ia, ia_broadaddr_old, 0, ifp);
			/*
			 * Do the lookup, if broadaddr found and the hash
			 * entry refcnt was 1 then update ia_addr field with
			 * the new ia_addr value. All happens under the hash
			 * chain write lock.
			 *
			 * If a match wasn't found OR the refcnt was > 1
			 * then we the entry was left intact since IP alias
			 * address'es are depending on it. In either case
			 * the entry is not delinked from the hash chain.
			 * Here the full broadcast hasn't changed so we
			 * don't have to check for adding a new entry.
			 */
			ia_addr_new = ia->ia_addr;

			in_bcast_old = (struct in_bcast *)hash_lookup_refcnt(
					&hashinfo_inaddr_bcast,
					in_bcast_broadcast_match,
					in_bcast_iaddr_refcnt,
					(caddr_t)&(ia_broadaddr_old->sin_addr),
					(caddr_t)ifp,
					(caddr_t)&ia_addr_new);
		}
	} else {
		/*
		 * We have a different full broadcast address, so
		 * lookup up this entry. If one found and it's refcnt was 1,
		 * the entry will be delinked from the hash chain and it's
		 * address returned.
		 * If refcnt of matching entryt > 1 the refcnt will be
		 * decremented but the entry won't be delinked from the
		 * chaint and NULL will be returned.
		 * If no match was found NULL is returned.
		 */
		in_bcast_adjust_check1(ia, ia_broadaddr_old, 0, ifp);

		in_bcast_old = (struct in_bcast *)hash_lookup_refcnt(
				&hashinfo_inaddr_bcast,
				in_bcast_broadcast_match,
				in_bcast_adjust_refcnt,
				(caddr_t)&(ia_broadaddr_old->sin_addr),
				(caddr_t)ifp,
				(caddr_t)0);

		if (in_bcast_old) { /* found last reference */

			in_bcast_adjust_check2(ia, ia_broadaddr_old,
				in_bcast_old, ifp);

			/* See if we already have this bcast entry */
			in_bcast_dup = (struct in_bcast *)hash_lookup(
				&hashinfo_inaddr_bcast,
				in_bcast_broadcast_match,
				(caddr_t)&(ia->ia_broadaddr.sin_addr),
				(caddr_t)ifp,
				(caddr_t)0);

			if (in_bcast_dup) { /* free old one */

				in_bcast_adjust_check3(ia, ia_broadaddr_old,
					in_bcast_dup, ifp);

				mcb_free(in_bcast_old,
					 sizeof(struct in_bcast), MT_IFADDR);
			} else {
				/*
				 * Update in_bcast entry to reflect change in
				 * ia_broadaddr value. We change the ia_addr
				 * field here since this is the last reference
				 * If the refcnt was > 1 we can't change the
				 * 'ia_addr' field as this entry is associated
				 * with alias'es which depend on 'ia_addr'.
				 * In that case hash_lookup_refcnt() returns
				 * NULL so we would have not gotten here.
				 */
				in_bcast_adjust_check4(ia, ia_broadaddr_old,
					in_bcast_dup, ifp);

				if (ia->ia_addr.sin_addr.s_addr !=
					ia_addr_old->sin_addr.s_addr) { /*new*/

					/* update all but ia_broadaddr */
					in_bcast_update(in_bcast_old, ia);
				} else {
					/* update ia_broadaddr field */
					in_bcast_old->ia_dstaddr =
						ia->ia_broadaddr;
				}
				(void)hash_insert(&hashinfo_inaddr_bcast,
					&(in_bcast_old->hashbucket),
					(caddr_t)&(ia->ia_broadaddr.sin_addr));
			}
		} else {
			/*
			 * Allocate new in_bcast entry to hold the
			 * new ia_broadaddr since the refcnt > 1
			 * we can't change the ia_broadaddr field of this
			 * existing entry since an IP alias depends on it.
			 */
			in_bcast_new = in_bcast_alloc(ia);

			in_bcast_adjust_check5(ia, ia_broadaddr_old,
				in_bcast_new, ifp);

			/* insert as new ia_broadaddr address */
			(void)hash_insert(&hashinfo_inaddr_bcast,
				&(in_bcast_new->hashbucket),
				(caddr_t)(&(ia->ia_broadaddr.sin_addr)));
		}
	}

	/*
	 * Adjust network broadcast entry if it or primary address changed
	 */
	if (ia->ia_netbroadcast.s_addr == in_netbroadcast_old->s_addr) {

		if (ia->ia_addr.sin_addr.s_addr !=
		    ia_addr_old->sin_addr.s_addr) { /* different */
			/*
			 * Do the lookup, if in_netbroadcast found and the hash
			 * entry refcnt was 1 then update ia_addr field with
			 * the new ia_addr value. All happens under the hash
			 * chain write lock.
			 *
			 * If a match wasn't found OR the refcnt was > 1
			 * then we the entry was left intact since IP alias
			 * address'es are depending on it. In either case
			 * the entry is not delinked from the hash chain.
			 * Here the full broadcast hasn't changed so we
			 * don't have to check for adding a new entry.
			 */
			ia_addr_new = ia->ia_addr;
			in_bcast_net_old=(struct in_bcast *)hash_lookup_refcnt(
				&hashinfo_inaddr_bcast,
				in_bcast_net_match,
				in_bcast_iaddr_refcnt,
				(caddr_t)in_netbroadcast_old,
				(caddr_t)ifp,
				(caddr_t)&ia_addr_new);
		}
	} else { /* different */

		in_bcast_adjust_check10(ia, in_netbroadcast_old, 0, ifp);

		in_bcast_net_old = (struct in_bcast *)hash_lookup_refcnt(
				&hashinfo_inaddr_bcast,
				in_bcast_net_match,
				in_bcast_adjust_refcnt,
				(caddr_t)in_netbroadcast_old,
				(caddr_t)ifp,
				(caddr_t)0);

		if (in_bcast_net_old) { /* found one with last reference */

			in_bcast_adjust_check11(ia, in_netbroadcast_old,
				in_bcast_net_old, ifp);

			/* See if we already have this new bcast entry */
			in_bcast_net_dup = (struct in_bcast *)hash_lookup(
				&hashinfo_inaddr_bcast,
				in_bcast_net_match,
				(caddr_t)&(ia->ia_netbroadcast.s_addr),
				(caddr_t)ifp,
				(caddr_t)0);

			if (in_bcast_net_dup) { /* yes so free old one */

				in_bcast_adjust_check12(ia,
					in_netbroadcast_old,
					in_bcast_net_dup, ifp);

				mcb_free(in_bcast_net_old,
					 sizeof(struct in_bcast), MT_IFADDR);
			} else {

				in_bcast_adjust_check13(ia,in_netbroadcast_old,
					in_bcast_net_dup, ifp);

				/*
				 * Update in_bcast entry to reflect change in
				 * ia_addr and ia_netbroadcast values.
				 * We change the ia_addr field here since this
				 * is the last reference.
				 * If the refcnt was > 1 we can't change the
				 * 'ia_addr' field as this entry is associated
				 * with alias'es which depend on 'ia_broadaddr'
				 * In that case the hash_lookup_refcnt()
				 * returns NULL so we wouldn't be here.
				 */
				if (ia->ia_addr.sin_addr.s_addr !=
					ia_addr_old->sin_addr.s_addr) { /*new*/

					in_bcast_update(in_bcast_net_old, ia);

					in_bcast_net_old->ia_dstaddr =
						ia->ia_broadaddr;
				} else {
					/* update broadcast address entries */
					in_bcast_net_old->ia_netbroadcast =
						ia->ia_netbroadcast;
				}

				/* now re-insert in_bcast_net entry */
				(void)hash_insert(&hashinfo_inaddr_bcast,
					&(in_bcast_net_old->hashbucket),
				       (caddr_t)&(ia->ia_netbroadcast.s_addr));
			}
		} else {
			/*
			 * Arrive here when either:
			 * 1. No existing in_netbroadcast entry exists OR
			 * 2. An entry exists but the refcnt indicates it's
			 *    associated with an existing IP alias entry.
			 *
			 * We next check if we have two distinct broadcast
			 * address'es before creating one for the new
			 * ia_netbroadcast address.
			 */
			if (ia->ia_netbroadcast.s_addr !=
			    ia->ia_broadaddr.sin_addr.s_addr) { /* distinct */
				/*
				 * Allocate new in_bcast entry to hold the
				 * new ia_netbroadcast since:
				 * 1. The two addres'es are distinct
				 * 2. No current ia_netbroadcast entry exists
				 * 3. The refcnt > 1 so we can't change
				 *    the broadcast address fields of and
				 *    existing entry as IP alias depends on it.
				 */
				in_bcast_net_new = in_bcast_alloc(ia);

				in_bcast_adjust_check14(ia,
					in_netbroadcast_old,
					in_bcast_net_new, ifp);

				/* insert as new ia_netbroadcast address */
				(void)hash_insert(&hashinfo_inaddr_bcast,
					&(in_bcast_net_new->hashbucket),
				     (caddr_t)(&(ia->ia_netbroadcast.s_addr)));
			}
		}
	}
	return;
}

/*
 * This hash refcnt procedure which is called when we've found a matching
 * in_bcast entry and we need to update it's reference count.
 *
 * in_bcast_adjust_refcnt(struct hashbucket *h, struct ifnet *arg1,
 *  caddr_t arg2)
 *
 * NOTE: This procedure should be called with only HTF_BROADCAST
 * hash bucket entries, but explictly checking is added for safety.
 * We are called under the write lock for this hash chain since
 * the hash function will remove the entry from the chain if we return
 * a non-zero return value.
 */
/* ARGSUSED */
int
in_bcast_adjust_refcnt(struct hashbucket *h, caddr_t arg1, caddr_t arg2)
{
	struct in_bcast *in_bcastp;
	int rval;

	if ((h->flags & HTF_BROADCAST) == 0) { /* not broadcast entry */
		return 0;
	}

	in_bcastp = (struct in_bcast *)h;

	if (in_bcastp->refcnt == 1) {
		/*
		 * Return status to have this entry delinked and it's
		 * address returned to the caller since this is the
		 * last reference and we're updating the entry.
		 */
		rval = 1;
	} else {
		/* Decrement refcnt in this in_bcast entry */
		(void)atomicAddUint(&in_bcastp->refcnt, -1);

		/*
		 * Return status to have this entry left along since
		 * the refnct indicates IP alias'es depend on this entry.
		 * So we'll have to allocate a new entry.
		 */
		rval = 0;
	}
	return rval;
}

/*
 * Allocate and initialize an in_bcast structure
 */
struct in_bcast *
in_bcast_alloc(struct in_ifaddr *ia)
{
	register struct in_bcast *ia_bcast;

	/* allocate and initialize in_bcast structure */
	ia_bcast = (struct in_bcast *)mcb_get(M_WAIT,
		sizeof(struct in_bcast), MT_IFADDR);

	ia_bcast->hashbucket.flags = HTF_BROADCAST;
	ia_bcast->ia_net = ia->ia_net;
	ia_bcast->ia_netmask = ia->ia_netmask;
	ia_bcast->ia_subnet = ia->ia_subnet;
	ia_bcast->ia_subnetmask = ia->ia_subnetmask;
	ia_bcast->ia_netbroadcast = ia->ia_netbroadcast;
	ia_bcast->ia_dstaddr = ia->ia_dstaddr;
	ia_bcast->ia_addr = ia->ia_addr;
	ia_bcast->ifp = ia->ia_ifp;
	ia_bcast->refcnt = 1;

	return ia_bcast;
}

/*
 * This is a hash lookup match procedure which compares the important
 * fields of the broadcast hash table entry for a complete match.
 * If a match is found we atomically decrements the reference count in
 * the entry by 1 and returns a match indication of non-zero IF the
 * reference count is less-than-or-equal to 1.
 * This allows us to remove the broadcast address entries associated with
 * a network interface from the broadcast hash table.
 *
 * in_bcast_free_match(struct hashbucket *h, struct in_addr *key,
 *  struct ifnet *arg1, struct in_ifaddr *arg2)
 *
 * NOTE: This procedure is called with only HTF_BROADCAST hash bucket entries,
 */
/* ARGSUSED */
int
in_bcast_free_match(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_bcast *ia_bcast;
	struct ifnet *ifp;
	struct in_ifaddr *ia;
	int match;

	if ((h->flags & HTF_BROADCAST) == 0) { /* not broadcast entry */
		return 0;
	}

	/*
	 * The address supplied must match all of the following fields in the
	 * in_bcast entry: ia_broadaddr, ia_netbroadcast, ia_subnet and ia_net
	 *
	 * NOTE: There are TWO entries inserted into the broadcast hash table
	 * that correspond to the full broadcast address AND the network
	 * broadcast address, so that either can be found via the hash lookup.
	 *
	 * So we check that supplied lookup key matches that the one used when
	 * the entry was inserted before we check the rest of the fields to be
	 * sure that the correct one is matched.
	 */
	if (bcmp(h->key, key, sizeof(struct in_addr) != 0)) { /* no match */
		return 0;
	}

	ia_bcast = (struct in_bcast *)h;
	ifp = (struct ifnet *)arg1;
	ia = (struct in_ifaddr *)arg2;

	if ((ia_bcast->ifp == ifp) &&
	    (ia_bcast->ia_broadaddr.sin_addr.s_addr ==
		ia->ia_broadaddr.sin_addr.s_addr) &&
	    (ia_bcast->ia_netbroadcast.s_addr ==
		ia->ia_netbroadcast.s_addr) &&
	    (ia_bcast->ia_net == ia->ia_net) &&
	    (ia_bcast->ia_subnet == ia->ia_subnet)) {

		/* return match indication if entry can be removed */
		match = 1;
	} else {
		/* return NO match indication */
		match = 0;
	}
	return match;
}

/*
 * Lookup in_bcast entries associated with this in_ifaddr address structure.
 * The two broadcast address are the full and netbroadcast one.
 * Under the lookup operation we call the refcnt procedure to the entry.
 * The entry is delinked and it's address is returned if the count falls
 * below the threshold used in the refcnt procedure.
 */
void
in_bcast_free(struct in_ifaddr *ia, struct ifnet *ifp)
{
	register struct in_bcast *in_bcastp_remove;

	/*
	 * lookup full broadcast address and if found call the refcnt
	 * procedure 'in_bcast_free_refcnt' on this entry.
	 * If last refcnt the address of the entry is returned after
	 * being delinked from the hash chain.
	 */
	in_bcastp_remove = (struct in_bcast *)hash_lookup_refcnt(
				&hashinfo_inaddr_bcast,
				in_bcast_free_match,
				in_bcast_free_refcnt,
				(caddr_t)(&(ia->ia_broadaddr.sin_addr)),
				(caddr_t)ifp,
				(caddr_t)ia);
	if (in_bcastp_remove) {
		mcb_free(in_bcastp_remove, sizeof(struct in_bcast), MT_IFADDR);
	}

	if (ia->ia_broadaddr.sin_addr.s_addr != ia->ia_netbroadcast.s_addr) {

		/*
		 * lookup netbroadcast address and if found call the refcnt
		 * procedure 'in_bcast_free_refcnt' on this entry.
		 * If last refcnt the address of the entry is returned after
		 * being delinked from the hash chain.
		 */
		in_bcastp_remove = (struct in_bcast *)hash_lookup_refcnt(
				&hashinfo_inaddr_bcast,
				in_bcast_free_match,
				in_bcast_free_refcnt,
				(caddr_t)(&(ia->ia_netbroadcast)),
				(caddr_t)ifp,
				(caddr_t)ia);
		if (in_bcastp_remove) {
			mcb_free(in_bcastp_remove, sizeof(struct in_bcast),
				 MT_IFADDR);
		}
	}
	return;
}

/*
 * This is a hash lookup match procedure which compares the important
 * fields of the broadcast hash table entry for a complete match.
 * If a match is found we atomically decrements the reference count in
 * the entry by 1 and returns a match indication of non-zero IF the
 * reference count is less-than-or-equal to 1.
 * This allows us to remove the broadcast address entries associated with
 * a network interface from the broadcast hash table.
 *
 * in_bcast_free_refcnt(struct hashbucket *h, struct in_addr *key,
 *  struct ifnet *arg1, struct in_ifaddr *arg2)
 *
 * NOTE: This procedure is called with only HTF_BROADCAST hash bucket entries,
 */
/* ARGSUSED */
int
in_bcast_free_refcnt(struct hashbucket *h, caddr_t arg1, caddr_t arg2)
{
	struct in_bcast *in_bcastp;
	int new_refcnt;

	if ((h->flags & HTF_BROADCAST) == 0) { /* not broadcast entry */
		return 0;
	}

	in_bcastp = (struct in_bcast *)h;
	new_refcnt = atomicAddUint(&in_bcastp->refcnt, -1);

	/* return match indication if entry can be deallocated */
	return ((new_refcnt < 1) ? 1 : 0);
}

/*
 * This procedure is called to perform the lookup operations on the
 * various broadcast address entries and to have there refcnt's updated
 * if entry(ies) already exist.
 * In the event that they do NOT exist, we allocate and insert into
 * the broadcast address table the required entries.
 * This procedure is called when IP alias address'es are created and the
 * corresponding 'struct in_ifaddr' parameter has been allocated and
 * initialized.
 *
 * NOTE: In the case of Class C IP alias'es there can be up to TWO broadcast
 * address entries for this entire group assuming all of the following fields
 * match: ifp, ia_broadaddr, ia_netbroadcast, ia_net and ia_subnet.
 *
 * In this case the refcnt's for either/both the broadcast address entry
 * associated with the primary address for the interface and the
 * netbroadcast address entry will be incremented.
 *
 * Returns
 * -------
 * Non-zero => New entries were allocated and inserted into the hash table
 * Zero => Existing entries exist so refcnt's were incremented
 */
int
in_bcast_find(struct ifnet *ifp, struct in_ifaddr *ia)
{
	struct in_bcast *in_bcastp, *ia_bcast_net;
	int status;

	/*
	 * Check if we already have hash table entry for full broadcast
	 * address. If found increment it's ref cnt and return it's address.
	 */
	in_bcastp = (struct in_bcast *)hash_lookup(
				&hashinfo_inaddr_bcast,
				in_bcast_find_match,
				(caddr_t)&(ia->ia_broadaddr.sin_addr),
				(caddr_t)ifp,
				(caddr_t)ia);
	/*
	 * Next check if the ia_netbroadcast value differs from ia_broadaddr.
	 * If so we lookup the ia_netbroadcast value and if we match exactly
	 * it's reference count gets incremented.
	 */
	if (ia->ia_broadaddr.sin_addr.s_addr != ia->ia_netbroadcast.s_addr) {

		ia_bcast_net = (struct in_bcast *)hash_lookup(
			&hashinfo_inaddr_bcast,
			in_bcast_find_match,
			(caddr_t)&(ia->ia_netbroadcast),
			(caddr_t)ifp,
			(caddr_t)ia);
	} else { /* equal */

		ia_bcast_net = (struct in_bcast *)0;
	}

	if (in_bcastp || ia_bcast_net) {
		/*
		 * found bcast entries so return status noting we
		 * simply did ref cnt operations and no NEW broadcast
		 * entries were created!
		 */
		status = 0;
	} else {
		/*
		 * No exact matching broadcast entries found so check if we
		 * have more than one broadcast address to allocate and
		 * insert into bcast hash table. The two entries are the
		 * full broadcast address and the network broadcast address.
		 */
		in_bcastp = in_bcast_alloc(ia);

		(void)hash_insert(&hashinfo_inaddr_bcast,
				&(in_bcastp->hashbucket),
				(caddr_t)(&(ia->ia_broadaddr.sin_addr)));

		if (ia->ia_broadaddr.sin_addr.s_addr !=
		    ia->ia_netbroadcast.s_addr) { /* have two address'es */

			ia_bcast_net = in_bcast_alloc(ia);

			(void)hash_insert(&hashinfo_inaddr_bcast,
				&(ia_bcast_net->hashbucket),
				(caddr_t)(&(ia->ia_netbroadcast)));
		}
		status = 1;
	}
	return status;
}

/* ARGSUSED */
int
in_bcast_find_match_check1(struct in_ifaddr *ia, struct in_bcast *in_bcastp,
	struct ifnet *ifp)
{
	return 1;
}

/* ARGSUSED */
int
in_bcast_find_match_check2(struct in_ifaddr *ia, struct in_bcast *in_bcastp,
	struct ifnet *ifp)
{
	return 1;
}

/*
 * This is a hash lookup match procedure which compares the important
 * fields of the broadcast hash table entry for a complete match.
 * If a match is found we atomically increment the reference count in
 * the entry and return a match indication.
 * This allows us to avoid inserting duplicate entries for the same
 * interface into the broadcast hash table.
 *
 * in_bcast_find_match(struct hashbucket *h, struct in_addr *key,
 *  struct ifnet *arg1, struct in_ifaddr *arg2)
 *
 * NOTE: This procedure is called with only HTF_BROADCAST hash bucket entries,
 * since the're stored in a separate hashtable, but explict checking is done
 * for safety.
 */
/* ARGSUSED */
int
in_bcast_find_match(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia;
	struct in_bcast *in_bcastp;
	struct ifnet *ifp;
	int match;

	if ((h->flags & HTF_BROADCAST) == 0) { /* not broadcast entry */
		return 0;
	}

	/*
	 * Check that supplied lookup key matches that the one used when
	 * the entry was inserted before we check the rest of the fields to be
	 * sure that the correct one is matched.
	 */
	if (bcmp(h->key, key, sizeof(struct in_addr) != 0)) { /* no match */
		return 0;
	}

	in_bcastp = (struct in_bcast *)h;
	ifp = (struct ifnet *)arg1;
	ia = (struct in_ifaddr *)arg2;

	/*
	 * The ia structure supplied must match all of the following fields
	 * in the in_bcast entry:
	 * ifp, ia_net, ia_subnet, ia_broadaddr and ia_netbroadcast
	 *
	 * NOTE: There can be up to two broadcast entries inserted into the
	 * broadcast hash table corresponding to the full broadcast address
	 * AND the network broadcast address. Either can be found via a
	 * hash lookup with the hash key noting which one is significant.
	 */
	if ((in_bcastp->ifp == ifp) &&
	    (in_bcastp->ia_net == ia->ia_net) &&
	    (in_bcastp->ia_subnet == ia->ia_subnet) &&
	    (in_bcastp->ia_broadaddr.sin_addr.s_addr ==
			ia->ia_broadaddr.sin_addr.s_addr) &&
	    (in_bcastp->ia_netbroadcast.s_addr ==
			ia->ia_netbroadcast.s_addr)) {

		(void)atomicAddUint(&in_bcastp->refcnt, 1);
		match = 1;
		in_bcast_find_match_check1(ia, in_bcastp, ifp);
	} else {
		match = 0;
		in_bcast_find_match_check2(ia, in_bcastp, ifp);
	}
	return match;
}

/*
 * This hash refcnt procedure which is called when we've found a matching
 * in_bcast entry and we examine it's reference count and update the
 * entry accordingly.
 *
 * in_bcast_examine_refcnt(struct hashbucket *h, 
 *	struct ifnet *arg1, struct sockaddr_in *arg2)
 *
 * NOTE: This procedure should be called with only HTF_BROADCAST
 * hash bucket entries, but explictly checking is added for safety.
 * We are also called withe hash table write lock for this chain since
 * the hash function will remove the entry from the chain if we return
 * a non-zero return value.
 */
/* ARGSUSED */
int
in_bcast_iaddr_refcnt(struct hashbucket *h, caddr_t arg1, caddr_t arg2)
{
	struct sockaddr_in *ia_addr_new;
	struct in_bcast *in_bcastp;

	if ((h->flags & HTF_BROADCAST) == 0) { /* not broadcast entry */
		return 0;
	}

	in_bcastp = (struct in_bcast *)h;
	ia_addr_new = (struct sockaddr_in *)arg2;

	if (in_bcastp->refcnt == 1) {
		/*
		 * Return status to have this entry delinked and it';s
		 * address returned to the the caller since this is the
		 * last reference and we're updating the entry.
		 */
		in_bcastp->ia_addr = *ia_addr_new;
	}
	/*
	 * Return status of zero so that this entry
	 * wont' be delinked from the hash chain.
	 */
	return 0;
}

/*
 * Update fields normally initialized from 'struct in_ifaddr' used
 * when in_bcast structure allocated.
 */
void
in_bcast_update(struct in_bcast *ia_bcast, struct in_ifaddr *ia)
{
	ia_bcast->ia_net = ia->ia_net;
	ia_bcast->ia_netmask = ia->ia_netmask;
	ia_bcast->ia_subnet = ia->ia_subnet;
	ia_bcast->ia_subnetmask = ia->ia_subnetmask;
	ia_bcast->ia_netbroadcast = ia->ia_netbroadcast;

	ia_bcast->ia_addr = ia->ia_addr;
	ia_bcast->refcnt = 1;

	return;
}

/*
 * Allocate and initialize the network address structure
 */
struct in_ifaddr *
in_inifaddr_alloc(struct ifnet *ifp)
{
	struct in_ifaddr *ia;

	/*
	 * allocate and initialize in_ifaddr structure
	 */
	ia = (struct in_ifaddr *)mcb_get(M_WAIT,
			sizeof(struct in_ifaddr), MT_IFADDR);

	ia->ia_hashbucket.flags = HTF_INET;
	/*
	 * initialize protocol independent ifaddr info
	 */
	IA_SIN(ia)->sin_family = AF_INET;
	ia->ia_ifp = ifp;
	
	/* initialize the address reference count */
	ia->ia_ifa.ifa_refcnt = 1;

	/* save start address of struct in_ifaddr */
	ia->ia_ifa.ifa_start_inifaddr = (caddr_t)ia;

	/* set address of addr and dstaddr sockaddr in ifaddr struct */
	ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	ia->ia_ifa.ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;

	ia->ia_ifa.ifa_netmask =
#ifdef _HAVE_SIN_LEN
		(struct sockaddr *)&ia->ia_sockmask;
#else
		(struct sockaddr_new *)&ia->ia_sockmask;
#endif
	/*
	 * NOTE: The setting of the sockmask address family field is probaly
	 * not required since the netmask comparison code assumes that only
	 * the sa_data array of the sockaddr matches. That portion starts with
	 * the port number and the remaining bytes of the address. The XOR an
	 * AND for loops in ifa_ifwithnet() and ifaof_ifpforaddr() in if.c is
	 * where all the work gets done. What is CRITICAL is that the 'sa_len'
	 * field of the ifa_netmask sockaddr be set properly since that
	 * controls the byte comparison/matching loop.
	 *
	 * Somewhat orthogonal to all of this is the sin_len field which was
	 * added in 4.4 BSD to the 'struct sockaddr'. The first byte becomes
	 * the length or number of bytes used in the address structure.
	 */
	ia->ia_sockmask.sin_family = AF_UNSPEC;
	ia->ia_sockmask.sin_len = _SIN_ADDR_SIZE;

	if (ifp->if_flags & IFF_BROADCAST) {
#ifdef _HAVE_SIN_LEN
		ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
#endif
		ia->ia_broadaddr.sin_family = AF_INET;
	}
	return ia;
}

/*
 * returns a printable string version of an internet address
 */
char *
inet_ntoa(struct in_addr in)
{
#	define NUM_BUFS 4
	static struct {
		char b[16];	/* 123.123.123.123\0 */
	} bufs[NUM_BUFS];
	static int bufno;
	u_char n, *p;
	char *b, *b0;
	int i;

	p = (u_char *)&in;
	b0 = bufs[bufno].b;
	bufno = (bufno+1) % NUM_BUFS;
	b = b0;
	for (i = 0; i < 4; i++) {
		if (i)
			*b++ = '.';
		n = *p;
		if (n > 99) {
			*b++ = '0' + (n / 100);
			n %= 100;
		}
		if (*p > 9) {
			*b++ = '0' + (n / 10);
			n %= 10;
		}
		*b++ = '0' + n;
		p++;
	}
	*b = 0;
	return (b0);
}

/*
 * Internet address interpretation routine.
 * All the network library routines call this
 * routine to interpret entries in the data bases
 * which are expected to be an address.
 * The value returned is in network order.
 */
__uint32_t
inet_addr(register char *cp)
{
	register __uint32_t val, base, n;
	register char c;
	__uint32_t parts[4], *pp = parts;

again:
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal.
	 */
	val = 0; base = 10;
	if (*cp == '0')
		base = 8, cp++;
	if (*cp == 'x' || *cp == 'X')
		base = 16, cp++;
	while (c = *cp) {
		if ((c >= '0') && (c <= '9')) {
			val = (val * base) + (c - '0');
			cp++;
			continue;
		}
		if (base == 16) {
			if ((c >= 'a') && (c <= 'f')) {
				val = (val << 4) + (c + 10 - 'a');
				cp++;
				continue;
			}
			if ((c >= 'A') && (c <= 'F')) {
				val = (val << 4) + (c + 10 - 'A');
				cp++;
				continue;
			}
		}
		break;
	}
	if (*cp == '.') {
		/*
		 * Internet format:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */
		if (pp >= parts + 4) {
			return (-1);
		}
		*pp++ = val, cp++;
		goto again;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp && *cp != ' ' && *cp != '\t' && *cp != '\n') {
		return (-1);
	}
	*pp++ = val;
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts;
	switch (n) {

	case 1:				/* a -- 32 bits */
		val = parts[0];
		break;

	case 2:				/* a.b -- 8.24 bits */
		val = (parts[0] << 24) | (parts[1] & 0xffffff);
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
			(parts[2] & 0xffff);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
		      ((parts[2] & 0xff) << 8) | (parts[3] & 0xff);
		break;

	default:
		return (-1);
	}
	val = htonl(val);
	return (val);
}

/*
 * This is a hash enumeration match procedure which compares the network
 * number desired with the network number in the hash bucket entry.
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
int
in_net_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	int net = (__psint_t)key;

	return ((net == ia->ia_net) ? 1 : 0);
}

/*
 * Return the network number from an internet address.
 */
__uint32_t
in_netof(struct in_addr in)
{
	register __uint32_t i = ntohl(in.s_addr);
	register __uint32_t net;
	register struct in_ifaddr *ia;

	if (IN_CLASSA(i))
		net = i & IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		net = i & IN_CLASSB_NET;
	else if (IN_CLASSC(i))
		net = i & IN_CLASSC_NET;
	else if (IN_CLASSD(i))
		net = i & IN_CLASSD_NET;
	else
		return (0);
	/*
	 * Check whether network is a subnet; if so, return subnet number.
	 */
	ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
		in_net_enummatch, HTF_INET,
		(caddr_t)(__psint_t)net, (caddr_t)0, (caddr_t)0);
	if (ia) {
		net = (i & ia->ia_subnetmask);
	}
	return net;
}

/*
 * This is a hash enumeration match procedure which gets called when the
 * subnets are NOT local. It AND's the network number desired with the
 * subnet mask in this entry to see if it matches the subnet number
 * in the hash bucket entry. The desired network number is supplied in 'key'.
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
int
in_localaddr_net_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	uint i = (__psint_t)key;

	return (((i & ia->ia_subnetmask) == ia->ia_subnet) ? 1 : 0);
}

/*
 * This is a hash enumeration match procedure which is called as the match
 * procedure WHEN 'subnetsarelocal' is non-zero. We compare the network
 * number desired with (the ip address in 'key' AND'ed with the network mask)
 * in this hash bucket entry. If they are equal we have a match.
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
int
in_localaddr_subnet_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	uint i = (__psint_t)key;

	return (((i & ia->ia_netmask) == ia->ia_net) ? 1 : 0);
}

/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net.
 * Otherwise, it includes only the directly-connected (sub)nets.
 */
int
in_localaddr(struct in_addr in)
{
	register struct in_ifaddr *ia;
	register __uint32_t i;

	i = ntohl(in.s_addr);

	if (subnetsarelocal) {
		ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
			in_localaddr_subnet_enummatch, HTF_INET,
			(caddr_t)(__psint_t)i, (caddr_t)0, (caddr_t)0);
	} else {
		ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
			in_localaddr_net_enummatch, HTF_INET,
			(caddr_t)(__psint_t)i, (caddr_t)0, (caddr_t)0);
	}
	return ((ia) ? 1 : 0);
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(struct in_addr in)
{
	register __uint32_t i = ntohl(in.s_addr);
	register __uint32_t net;

	if (IN_EXPERIMENTAL(i) || IN_MULTICAST(i))
		return (0);
	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		if (net == 0 || net == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * This enumaration match procedure is called after for each entry in the
 * hash table. We are checking for ANY Internet entry which matches the
 * interface address supplied in 'arg1'.
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
int
ifnet_any_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct ifnet *ifp = (struct ifnet *)arg1;

	return ((ia->ia_ifp == ifp) ? 1 : 0);
}

/*
 * This enumaration match procedure is called after for each entry in the
 * hash table. We are checking for the primary IP address associated
 * with this interface. We return a TRUE if a matching entry is founc.
 *
 * int ifnet_enummatch(struct hashbucket *h, caddr_t key,
 *   struct ifnet *arg1, caddr_t arg2)
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 * This procedure is used by both v4 and v6.  Since v4 and v6 use different
 * hash tables there is no confusion.
 */
/* ARGSUSED */
int
ifnet_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
#ifdef INET6
	struct ifaddr *ifa = (struct ifaddr *)h;
#else
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
#endif
	struct ifnet *ifp = (struct ifnet *)arg1;

#ifdef INET6
	if ((ifa->ifa_ifp == ifp) && (ifa->ifa_addrflags & IADDR_PRIMARY))
#else
	if ((ia->ia_ifp == ifp) && (ia->ia_addrflags & IADDR_PRIMARY))
#endif
		return (1);
	return (0);
}

/*
 * This match procedure is called after the hashing lookup has indexed
 * into the hash table and is comparing hash bucket entries for a match
 * with the supplied key (IP address). We return the non-zero if the key
 * matches the ia_addr field of the 'struct in_ifaddr'.
 * NOTE: The key is the same as the ia_'addr' field.
 */
/* ARGSUSED */
int
inaddr_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia;
	struct in_addr *addr;
	int match = 0;

	if (h->flags & HTF_INET) { /* internet address */

		ia = (struct in_ifaddr *)h;
		addr = (struct in_addr *)key;

		if (IA_SIN(ia)->sin_addr.s_addr == addr->s_addr) {
			match = 1;
		}
	}
	return match;
}

/*
 * This match procedure is called after the hashing lookup has matched
 * the supplied IP address AND the supplied 'struct ifnet' address
 * matches that which was stored in the entry.
 *
 *inaddr_ifnetaddr_match(struct in_ifaddr *ia, struct in_addr *key,
 *	struct ifnet *arg1, caddr_t arg2)
 */
/* ARGSUSED */
int
inaddr_ifnetaddr_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_ifaddr *ia;
	struct in_addr *addr;
	struct ifnet *ifp;
	int match = 0;

	if (h->flags & HTF_INET) { /* internet address */
		ia = (struct in_ifaddr *)h;
		addr = (struct in_addr *)key;
		ifp = (struct ifnet *)arg1;

		if ((ia->ia_ifp == ifp) &&
		    IA_SIN(ia)->sin_addr.s_addr == addr->s_addr) {
			match = 1;
		}
	}
	return match;
}

/*
 * Procedure that finds the interface (ifnet structure) corresponding
 * to one of our IP addresses. Used by the macro of the upper case
 * version of the same name. This is the optimized case where we
 * do the direct hash table lookup to see if we have a matching
 * source IP address.
 */
struct ifnet *
inaddr_to_ifp(struct in_addr addr)
{
	register struct in_ifaddr *ia;

	ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
			inaddr_match,
			(caddr_t)&(addr),
			(caddr_t)0,
			(caddr_t)0);

	return (((ia) ? ia->ia_ifp : (struct ifnet *)0));
}

/*
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
int
in_rtinit_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct ifnet *ifp = (struct ifnet *)key;

	return (((ia->ia_ifp == ifp) && (ia->ia_flags & IFA_ROUTE)) ? 1 : 0);
}

/*
 * Delete a host route for an in_ifaddr structure if one exists for
 * the supplied interface
 *
 * NOTE: This procedure is used by the slip and pp drivers to delete
 * there host route on closing. This procedure removes these drivers
 * from having to know about all the hash table ip address lookup stuff.
 */
int
in_rtinit(struct ifnet *ifp)
{
	struct in_ifaddr *ia;

	ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
		in_rtinit_enummatch, HTF_INET,
		(caddr_t)ifp, (caddr_t)0, (caddr_t)0);
	if (ia) {
		(void)rtinit(&ia->ia_ifa, RTM_DELETE, RTF_HOST);
		ia->ia_flags &= ~IFA_ROUTE;

		ia->ia_addr.sin_family = AF_UNSPEC;
		ia->ia_dstaddr.sin_family = AF_UNSPEC;
	}
	return 1;
}

/*
 * Trim a mask in a sockaddr_in structure
 * Produce a length of 0 for an address of 0.
 * Otherwise produce the index of the first zero byte.
 */
void
in_socktrim(ap)
#ifdef _HAVE_SIN_LEN
	struct sockaddr_in *ap;
#else
	struct sockaddr_in_new *ap;
#endif
{
	register char *cp;

	if (ap->sin_addr.s_addr == 0) {
		ap->sin_len = 0;
		return;
	}
	cp = (char *)(&ap->sin_addr.s_addr+1);
	while (*--cp == 0)
		continue;
	ap->sin_len = cp - (char*)ap + 1;
	return;
}

/*
 * Generic internet control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
in_control(struct socket *so, __psint_t cmd, caddr_t data, struct ifnet *ifp)
{
	struct in_aliasreq *ifra;
	struct ifaliasreq *ifa_req;
	struct ifreq *ifr = (struct ifreq *)data;
	struct in_ifaddr *ia;
	__uint32_t i;
	int error = 0;
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
		ia = (struct in_ifaddr *)(ifp->in_ifaddr);
	} else {
		ia = (struct in_ifaddr *)0;
	}

	switch (cmd) {

	case SIOCGIFADDR:	/* Get interface address */
		if (ia) {
			ifr->ifr_addr = *(ia->ia_ifa.ifa_addr);
		} else {
			error = EADDRNOTAVAIL;
		}
		break;

	case SIOCGIFDSTADDR:	/* Get destination address */
		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0) {
			error = EINVAL;
		} else {
			if (ia) {
				ifr->ifr_dstaddr = *(ia->ia_ifa.ifa_dstaddr);
			} else {
				error = EADDRNOTAVAIL;
			}
		}
		break;

	case SIOCGIFBRDADDR:	/* Get broadcast address */
		if (ifp == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EINVAL;
		} else {
			if (ia) {
				ifr->ifr_dstaddr = *(ia->ia_ifa.ifa_broadaddr);
			} else {
				error = EADDRNOTAVAIL;
			}
		}
		break;

	case SIOCGIFNETMASK:	/* Get network mask */

		if (ia) {
#ifdef _HAVE_SIN_LEN
			*SATOSIN(&ifr->ifr_addr) = ia->ia_sockmask;
#else
			*SATOSIN_NEW(&ifr->ifr_addr) = ia->ia_sockmask;
#endif
		} else {
			error = EADDRNOTAVAIL;
		}
		break;

	case SIOCAIFADDR:	/* Add interface alias address */
	{
		int iaIsNew = 0, hostIsNew = 1, maskIsNew = 0;
		int local_ia_addrflags = IADDR_ALIAS;
		int alias;

		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		ifra = (struct in_aliasreq *)data;

		if (ia) {
			/*
			 * primary address set in ifnet record so check
			 * if we have match addresses. If so this is a
			 * duplicate of the primary address for this
			 * interface.
			 */
			if ((ia->ia_ifp == ifp) &&
				(IA_SIN(ia)->sin_addr.s_addr ==
				  ifra->ifra_addr.sin_addr.s_addr)) {
				hostIsNew = 0;
				goto foundit;
			}
		}
		/*
		 * Find address entry matching both ifnet and in_addr
		 */
		ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
			inaddr_ifnetaddr_match,
		(caddr_t)(&(SATOSIN(&(ifra->ifra_addr))->sin_addr)),
			(caddr_t)ifp,
			(caddr_t)0);

		if (ia && (ia->ia_addr.sin_family == AF_INET)) {
			/*
			 * Found a matching entry so we do NOT
			 * have to add a new alias address
			 */
#ifdef _HAVE_SIN_LEN
			if (ifra->ifra_addr.sin_len == 0)
#else
			if (ifra->ifra_addr.sin_addr.s_addr == 0)
#endif
			{
				/*
				 * Typical case is where ifconfig
				 * set's the netmask first and only
				 * later sets the ifaddr. In that case
				 * the the primary address is zero.
				 * The other case when the supplied
				 * interfaces address is zero, we
				 * set the primary address in the
				 * request structure, but it's not
				 * returned to the user process.
				 */
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else {
				/*
				 * Check if add alias is a no-op case
				 * if we already have that address
				 */
				if (ifra->ifra_addr.sin_addr.s_addr ==
				       ia->ia_addr.sin_addr.s_addr) {
					hostIsNew = 0;
				}
			}
		} else {
			/*
			 * Allocate and initialize in_ifaddr structure
			 * Also note that this address needs to be
			 * added to the ip address hash table.
			 */
			ia = in_inifaddr_alloc(ifp);
			iaIsNew = 1;

			if (ifp->in_ifaddr == (caddr_t)0) {
				/*
				 * Note the primary in_ifaddr for the
				 * interface has NOT been set AND this
				 * is being done via an alias "type"
				 * ioctl operation.
				 * We save the address flags to use
				 * later because it's the primary and
				 * save the address of the record for
				 * this interface so it's always valid.
				 */
				local_ia_addrflags = ia->ia_addrflags
					= IADDR_PRIMARY;

				ifp->in_ifaddr = (caddr_t)ia;
				if (ifp != &loif) {
				  (void)atomicAddInt(&in_interfaces,1);
				}
			 }
		}
foundit:
		if (ifra->ifra_mask.sin_len) {

			in_ifscrub(ifp, ia);

			ia->ia_sockmask = ifra->ifra_mask;
			/*
			 * NOTE: The af_family must NOT be set in the
			 * ia_sockmask field. The af_family is passed here
			 * via the alias ioctl but it's cleared here.
			 * The reason is the address comparison procedures
			 * require that only the length field be set. You
			 * can argue this either way but BSD 4.4lite2 doesn't
			 * set it in the routing socket code and so to avoid
			 * getting the wrong comparison results any code which
			 * calls the rn_refines procedures must adhere to the
			 * length only policy. The reason is that the radix
			 * tree code requires the first byte of the byte
			 * string to compare contains the length. Hence the
			 * ifa_ifwithnet* procedure(s) fall into this category
			 * since they call the radix tree comparison functions.
			 */
			ia->ia_sockmask.sin_family = AF_UNSPEC;

			ia->ia_subnetmask =
				ntohl(ia->ia_sockmask.sin_addr.s_addr);
			maskIsNew = 1;
		}

		if ((ifra->ifra_dstaddr.sin_family == AF_INET) &&
			(ifp->if_flags & IFF_POINTOPOINT)) {

			/* delete any existing route for this interface */
			in_ifscrub(ifp, ia);

			/* update destination address in 'struct in_ifaddr' */
			ia->ia_dstaddr =
				*((struct sockaddr_in *)(&ifra->ifra_dstaddr));
			maskIsNew  = 1;	/* We lie; but the effect's the same */
	        }

		if (ifra->ifra_addr.sin_family == AF_INET) {

			if (iaIsNew) {
				/*
				 * Add address record to IP address hash table
				 */
				IA_SIN(ia)->sin_family = AF_INET;
				IA_SIN(ia)->sin_addr= ifra->ifra_addr.sin_addr;
				ia->ia_hashbucket.flags = HTF_INET;
				ia->ia_addrflags = local_ia_addrflags;

				(void)hash_insert(&hashinfo_inaddr,
					&(ia->ia_hashbucket),
			(caddr_t)(&(SATOSIN(&(ifra->ifra_addr))->sin_addr)));

				/* Bump number of configured address'es */
				(void)atomicAddInt(&in_ifaddr_count, 1);
			}

			if (hostIsNew || maskIsNew) {
				/*
				 * initialize address flags
				 */
				ia->ia_addrflags = local_ia_addrflags;

				/*
				 * initialize new route for this interface.
				 * The parameters mean don't scrub old
				 * interface and make host route since this
				 * is an alias address.
				 */
				alias = (local_ia_addrflags & IADDR_ALIAS)
						? 1 : 0;
				error = in_ifinit(ifp, ia,
					SATOSIN(&ifra->ifra_addr), 1, alias);
		        }
	        }

		if ((ifra->ifra_broadaddr.sin_family == AF_INET) &&
			(ifp->if_flags & IFF_BROADCAST)) {

			/* update broadcast address in 'struct in_ifaddr' */
			ia->ia_broadaddr =
			     *((struct sockaddr_in *)(&ifra->ifra_broadaddr));

			if (iaIsNew &&
			    (ia->ia_broadaddr.sin_addr.s_addr != INADDR_ANY)) {
				/*
				 * Search for a matching broadcast address
				 * entries in the broadcast hash table.
				 * Create them if not found OR increment
				 * the reference counts if they already exist.
				 */
				(void)in_bcast_find(ifp, ia);
			}
		} else {
			/*
			 * We arrive here when the broadcast address isn't
			 * explictly specified on the command line.
			 * In this case in_ifinit() would have set it to the
			 * default value depending on the class of the
			 * specified IP address. So we only have to handle
			 * creating the hash table entries for the broadcast
			 * addresses for this ip alias if the interface
			 * supports broadcast and we added the main address.
			 */
			if (hostIsNew) {

				if (iaIsNew &&
				    (ia->ia_broadaddr.sin_addr.s_addr
						!= INADDR_ANY) &&
				    (ifp->if_flags & IFF_BROADCAST)) {

					(void)in_bcast_find(ifp, ia);
				}
			}
		}

		/* Pass on the flag to device drives that support this */
		if (ifp && ifp->if_ioctl) {
		  	int err; 
		  	/* Filter EINVAL (which the driver returns if driver
			 * doesn't support cmd, no big deal) else singal 
			 * error 
		   	 */ 
			if (ifp->if_flags & IFF_IPALIAS) {
				if( (err = (*ifp->if_ioctl)(ifp, cmd, 
					(caddr_t)&ia->ia_ifa)) != EINVAL )
		    			error = err; 
			} else {
				if (ifp->if_type==IFT_HIPPI || 
				    ifp->if_type==IFT_GSN) {
					if ((err = (*ifp->if_ioctl)(ifp, cmd,
					    data)) != EINVAL)
						error = err;
				}
			}
		}

	}
		break;

	case SIOCDIFADDR:	/* Remove interface alias address */

		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}

		ifra = (struct in_aliasreq *)data;
		if (ifra->ifra_addr.sin_family == AF_INET) {
			/*
			 * Look for match of ifnet and s_addr fields.
			 */
			ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
				inaddr_ifnetaddr_match,
			(caddr_t)(&(SATOSIN(&(ifra->ifra_addr))->sin_addr)),
				(caddr_t)ifp,
				(caddr_t)0);
		}
		if (ia == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (ifp->in_ifaddr == (caddr_t)ia) {
			/*
			 * Requested to delete primary address for interface!
			 * We make this an error since we assume the user
			 * doesn't know the consequences of this action.
			 * The problem is that the alias address'es have
			 * host routes associated with them and the primary
			 * address is used in the host route. Deleting the
			 * primary address when alias'es exist causes serious
			 * problems when trying to delete subsequent alias'es.
			 * This could return no error, aka a no-op instead.
			 */
			error = EOPNOTSUPP;
			break;
		}

		/* delete any existing route for this interface */
		in_ifscrub(ifp, ia);

		/* Decrement number of configured address'es */
		(void)atomicAddInt(&in_ifaddr_count, -1);

		if ( ifp && 
		     (ifp->if_type == IFT_HIPPI || ifp->if_type == IFT_GSN) &&
		     ifp->if_ioctl){
			/* Only pass on flag to the HIPPI and GSN drivers */
			int err; 
			  /* Filter EINVAL (which the driver returns if driver 
			   * doesn't support cmd) else singal error 
			   */ 
			if((err = (*ifp->if_ioctl)(ifp, cmd, data)) != EINVAL)
				error = err; 
		} else 
			/* Lookup broadcast address entries for non HIPPI/GSN
			 * and recover if required 
			 */
			in_bcast_free(ia, ifp);

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

	case SIOCLIFADDR:	/* Get list of interface network addresses */

		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}
		ifa_req = (struct ifaliasreq *)data;

		if (ifa_req->ifra_addr.sa_family == AF_INET) {

			/* Check if the requested 'nth' element is valid */
			if (ifa_req->cookie < 0) { /* bad nth element req */
				error = EINVAL;
				break;
			}

			if (ifa_req->cookie == 0) {
				/*
				 * return primary address for the interface
				 */
				ia = (struct in_ifaddr *)(ifp->in_ifaddr);
			} else {
				/*
				 * return all alias'es for the interface
				 */
				ia = (struct in_ifaddr *)hash_enum_getnext(
					&hashinfo_inaddr,
					in_getalias_enummatch,
					HTF_INET,
					1, /* skip primary addr which is zero*/
					ifa_req->cookie,
					(caddr_t)ifp);
			}

			if (ia == (struct in_ifaddr *)0) {
				ifa_req->cookie = -1;
				break;
			}
			ifa_req->cookie++;
			ifa_req->ifra_addr = *(ia->ia_ifa.ifa_addr);
			ifa_req->ifra_broadaddr = *(ia->ia_ifa.ifa_broadaddr);
#ifdef _HAVE_SIN_LEN
			*SATOSIN(&ifa_req->ifra_mask) = ia->ia_sockmask;
#else
			*SATOSIN_NEW(&ifa_req->ifra_mask) = ia->ia_sockmask;
#endif /* _HAVE_SIN_LEN */
		}
		break;

	case SIOCSIFBRDADDR:	/* Set interface broadcast address */
		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}
		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}

		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EINVAL;
		} else {
			if (ia) {
				ia->ia_broadaddr =
				*((struct sockaddr_in *)(&ifr->ifr_broadaddr));
			} else {
				error = EADDRNOTAVAIL;
			}
		}
		break;

	case SIOCSIFNETMASK:	/* Set interface network mask */

		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}

		if (ia == (struct in_ifaddr *)0) {

			ia = in_inifaddr_alloc(ifp);
			/*
			 * We set the not-in-hash flag to indicate that
			 * the primary address is NOT available and so this
			 * entry hashn't been inserted into the address table
			 * This is because we allow ifconfig to set the
			 * network mask BEFORE setting the interface address
			 */
			ia->ia_hashbucket.flags = HTF_INET | HTF_NOTINTABLE;

			if (ifp != &loif) {
				(void)atomicAddInt(&in_interfaces, 1);
			}

			if (ifp->in_ifaddr == (caddr_t)0) {
				/*
				 * Note the primary 'struct in_ifaddr' for this
				 * interface since it has NOT been set yet
				 */
				ia->ia_addrflags = IADDR_PRIMARY;
				ifp->in_ifaddr = (caddr_t)ia;
			}
		}
		i = SATOSIN(&(ifr->ifr_addr))->sin_addr.s_addr;
		ia->ia_subnetmask = ntohl(ia->ia_sockmask.sin_addr.s_addr = i);

		in_socktrim(&ia->ia_sockmask);
		break;

	case SIOCSIFADDR:	/* Set interface address */
		{
		struct sockaddr_in ia_addr_old;
		struct sockaddr_in ia_broadaddr_old;
		struct in_addr ia_netbroadcast_old;
		int addBcast = 1;
		int iaAdjust = 0;

		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}
		if (ia == (struct in_ifaddr *)0) {

			/* allocate and initialize in_ifaddr structure */
			ia = in_inifaddr_alloc(ifp);

			/* set the interface address */
			IA_SIN(ia)->sin_addr.s_addr =
				SATOSIN(&ifr->ifr_addr)->sin_addr.s_addr;
			/*
			 * Add 'ia' to 'inaddr' hash table
			 * The majority of the fields in 'struct in_ifaddr'
			 * are set by the in_ifinit procedure.
			 */
			(void)hash_insert(&hashinfo_inaddr,
				&(ia->ia_hashbucket),
			 (caddr_t)(&(SATOSIN(&(ifr->ifr_addr))->sin_addr)));

			/* Bump number of configured address'es */
			(void)atomicAddInt(&in_ifaddr_count, 1);

			if (ifp->in_ifaddr == (caddr_t)0) {
				/*
				 * Note the primary address'es in_ifaddr' for
				 * the interface since it has NOT been set.
				 */
				ia->ia_addrflags = IADDR_PRIMARY;
				ifp->in_ifaddr = (caddr_t)ia;
			}

			if (ifp != &loif) {
				(void)atomicAddInt(&in_interfaces, 1);
			}
			/*
			 * NOTE: The default case is to add the broadcast
			 * address'es AFTER the main ia structure is fully
			 * initialized down below, since the netbroadcast
			 * and other fields are set by 'in_ifinit'.
			 */
		} else {
			/*
			 * If we arrive here the ifnet structure already has
			 * it's in_ifaddr address set. We check if the primary
			 * address needs to be inserted into the IP address
			 * hash table.
			 *
			 * NOTE: The not-in-table case arises when:
			 *
			 * 1. ifconfig does the ioctl to set the network mask
			 *    first then the ioctl to set the interface address
			 * 2. For the point-to-point interfaces we allow
			 *    the destination address to be set before the
			 *    interface address is set.
			 */
			if (ia->ia_hashbucket.flags & HTF_NOTINTABLE) {

				ia->ia_hashbucket.flags &= ~HTF_NOTINTABLE;
				ia->ia_addrflags = IADDR_PRIMARY;

				/* initialize the primary address info */
				IA_SIN(ia)->sin_family = AF_INET;
				IA_SIN(ia)->sin_addr.s_addr =
				  SATOSIN(&ifr->ifr_addr)->sin_addr.s_addr;

				ia->ia_ifp = ifp;
				(void)hash_insert(&hashinfo_inaddr,
					&(ia->ia_hashbucket),
			(caddr_t)(&(SATOSIN(&(ifr->ifr_addr))->sin_addr)));

				/* Bump number of configured address'es */
				(void)atomicAddInt(&in_ifaddr_count, 1);
			} else {
				/*
				 * We arrive here when the 'ia_addr' field
				 * of an existing in_ifaddr structure changes.
				 * This may require the hash entry for the
				 * ia_addr be moved so that it will reside
				 * on the correct hash chain depending on
				 * the hash index computed from it's IP address
				 * NOTE; We must save ia_addr, ia_broadcast
				 * and the ia_netbroadcast values so that the
				 * adjustment of the hash table entries can be
				 * done after the in_ifinit call which
				 * initializes the new fields, updates the
				 * routes, etc.
				 * There are three entries to consider, the
				 * main ip address which resides in the address
				 * hash table AND up to two broadcast addresses
				 * which reside in the bcast hash table.
				 * All of these entries may require adjustment
				 * in the hash table in the event any of there
				 * addresses change.
				 *
				 * We also reset the address flag since this
				 * address could have been an alias which is
				 * now going to be the primary address. This
				 * case is easily reproduced by the commands:
				 * ifconfig ec0 alias host => set primary
				 * ifconfig ec0 alias alt =>add alias 'alt'
				 * ifconfig ec0 -alias alt => clear alias
				 * ifconfig ec0 down => down net interface
				 * ifconfig ec0 alt => set 'alt' as primary
				 */
				ia->ia_addrflags = IADDR_PRIMARY;
				ia_addr_old = ia->ia_addr;

				/* save copies of both bcast addresses */
				ia_broadaddr_old = ia->ia_broadaddr;
				ia_netbroadcast_old = ia->ia_netbroadcast;

				/* Don't add new bcast hash table entries */
				addBcast = 0;

				/* Note adjust unicast and bcast entries */
				iaAdjust = 1;
			}
		}
		/*
		 * Scrub the old route before initializing the new
		 * one and then initialize the in_ifaddr structure.
		 */
		error = in_ifinit(ifp, ia, SATOSIN(&ifr->ifr_addr), 1, 0);

		/*
		 * Now that the 'ia' structure is fully initialized we can
		 * create and insert into the hash table the broadcast entries
		 * for all interfaces which support broadcast. PPP ones don't
		 * an so a braodcast hash table entry won't be created.
		 */
		if (addBcast &&
		    (ia->ia_broadaddr.sin_addr.s_addr != INADDR_ANY) &&
		    (ifp->if_flags & IFF_BROADCAST)) {

			(void)in_bcast_find(ifp, ia);
		}

		if (iaAdjust) {
			if (ia->ia_addr.sin_addr.s_addr !=
				ia_addr_old.sin_addr.s_addr) { /* new addr */

				ia_addr_adjust_entry(ifp, ia, &ia_addr_old);
			}

			/*
			 * Interfaces not supporting IFF_BROADCAST, only the
			 * ia_addr is updated since the dst_addr must be
			 * updated via a different ioctl operation.
			 */
			if (ifp->if_flags & IFF_BROADCAST) {

				in_bcast_adjust_entry(ifp, ia, &ia_addr_old,
					&ia_broadaddr_old,
					&ia_netbroadcast_old);
			}
			break;
		}
	}
		break;

	case SIOCSIFDSTADDR:		/* Set interface destination address */
	    {
		struct sockaddr_in oldaddr;

		if (!_CAP_ABLE(CAP_NETWORK_MGT)) {
			error = EPERM;
			break;
		}
		if (ifp == 0) {
			error = EOPNOTSUPP;
			break;
		}
		/*
		 * Operation is valid if only a point-to-pont interface
		 */
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0) {
			error = EINVAL;
			break;
		}

		if (ia == (struct in_ifaddr *)0) {

			/* allocate and initialize in_ifaddr structure */
			ia = in_inifaddr_alloc(ifp);

			if (ifp != &loif) {
				(void)atomicAddInt(&in_interfaces, 1);
			}

			if (ifp->in_ifaddr == (caddr_t)0) {
				/*
				 * Note the primary 'struct in_ifaddr' for this
				 * interface since it has NOT been set yet
				 * SO we not that the in_ifaddr entry can't be
				 * in the hash table.
				 * NOTE: This is a little weird, allowing
				 * ifconfig or ifctl callers to set a number
				 * of other parameters before ever setting the
				 * primary IP address. Hence we handle this
				 * case by noting to insert the entry when we
				 * get the primary IP address.
				 */
				ia->ia_hashbucket.flags |= HTF_NOTINTABLE;
				ifp->in_ifaddr = (caddr_t)ia;
			}
		}

		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = *(struct sockaddr_in *)&(ifr->ifr_dstaddr);

		if (ifp->if_ioctl &&
		    (error = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR,
				&(ia->ia_ifa)))) {
			ia->ia_dstaddr = oldaddr;
			break;
		}
		if (ia->ia_flags & IFA_ROUTE) {
			/*
			 * entry has route associated with it so delete
			 * the old one and add one with the new dest addr
			 */
			ia->ia_ifa.ifa_dstaddr = (struct sockaddr *)&oldaddr;
			(void)rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr =
				(struct sockaddr *)&ia->ia_dstaddr;
			(void)rtinit(&(ia->ia_ifa), (int)RTM_ADD,
				RTF_HOST|RTF_UP);
		}
	    }
		break;

	default:
		if (ifp == 0 || ifp->if_ioctl == 0) {
			error = EOPNOTSUPP;
			break;
		}
		error = ((*ifp->if_ioctl)(ifp, cmd, data));
	}

	if (ifp) {
		IFNET_UNLOCK(ifp);
	}
	return error;
}

/*
 * Delete any existing route for an interface.
 *
 * NOTE:  For alias addresses we delete the host route we created
 * for the alias address when it was added.
 */
void
in_ifscrub(struct ifnet *ifp, struct in_ifaddr *ia)
{
	struct sockaddr *save_addr, *save_dstaddr;
	int flags;

	if (ia->ia_flags & IFA_ROUTE) {

		if (ia->ia_addrflags & IADDR_ALIAS) {
			/*
			 * In the IP alias address case, since we have a host
			 * route we need to fixup the dstaddr field so that it
			 * points to the correct address. Host routes imply
			 * the rtinit proc uses 'dstaddr' as the IP address
			 * on which to search.
			 */
			flags = RTF_HOST;

			/* save original addr and dstaddr for later restore */
			save_dstaddr = ia->ia_ifa.ifa_dstaddr;

			/* now make dst address point to alias address */
			ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;

			/*
			 * now make addr address point to primary address
			 * so that the host route's gateway address will
			 * be the primary address of this interface.
			 */
			save_addr = ia->ia_ifa.ifa_addr;

			ia->ia_ifa.ifa_addr =
		       ((struct in_ifaddr *)(ifp->in_ifaddr))->ia_ifa.ifa_addr;

			(void)rtinit(&(ia->ia_ifa), (int)RTM_DELETE, flags);

			/* restore original dstaddr and addr address'es */
			ia->ia_ifa.ifa_dstaddr = save_dstaddr;
			ia->ia_ifa.ifa_addr = save_addr;

		} else {
			flags = (ifp->if_flags &(IFF_LOOPBACK|IFF_POINTOPOINT))
				? RTF_HOST : 0;
			(void)rtinit(&(ia->ia_ifa), (int)RTM_DELETE, flags);
		}
		ia->ia_flags &= ~IFA_ROUTE;
	}
	return;
}

/*
 * Initialize an interface's internet address and routing table entry.
 *
 * NOTE: The 'alias' parameter is non-zero this is requesting that
 * a host route be made for IP alias address on the interface 'ifp'.
 */
int
in_ifinit(struct ifnet *ifp, struct in_ifaddr *ia, struct sockaddr_in *sin,
	  int scrub, int alias)
{
/* #ifdef IFA_LINK_RTREQUEST_ENABLED */
	struct in_ifaddr *ia_xx;
/* #endif */
	struct sockaddr_in oldaddr;
	struct sockaddr *save_addr, *save_dstaddr;
	__uint32_t i = ntohl(sin->sin_addr.s_addr);
	int flags = RTF_UP, error;
	int restore_addr = 0;
	int broadcast = 0;
#ifdef _ALIAS_HOST_ROUTES
	int oflags = flags;
#endif
	int wasup = ifp->if_flags & IFF_UP;

	ASSERT(IFNET_ISLOCKED(ifp));

	oldaddr = ia->ia_addr;
	ia->ia_addr = *sin;

	if (alias == 0) {
		/*
		 * Not an alias address add so assume SIOCSIFADDR case.
		 * Give the interface a chance to initialize if this is its
		 * first address, and to validate the address if necessary.
		 */
		if (ifp->if_ioctl &&
		   (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR,
					(caddr_t)&ia->ia_ifa))) {
			ia->ia_addr = oldaddr;
			return error;
		}

		/*
		 * If the interface did not know its MAC address when it
		 * attached itself, then fix it up now. Use a pointer to
		 * NULL as the address which informs the procedure to look
		 * in the raw interface list for the address.
		 */
		rawif_fixlink(ifp);
	}

/*#ifdef IFA_LINK_RTREQUEST_ENABLED */
	/*
	 * Enable this code when the link_rtrequest procedure/framework
	 * is properly handles the initialization case.
	 * The original design envisioned for Merit's BGP5 was that the
	 * routing function was specified and enabled when the link driver
	 * was initialized and called when address'es changed for that link.
	 * When the routing socket messages are sent into the kernel his
	 * function was called to handle the refcnt operation on the address
	 * asociated with routes which are being added, deleted or changed.
	 *
	 * This hook is important to maintain the reference counts on the
	 * address structures BUT the correct address structure and associated
	 * refcnt is not correct when and IP alias is added but it is NOT on
	 * the same subnet as the primary address of theinterface.
	 * When you are on the same subnet the correct address structure
	 * is found and the refcnt is correct when adding and deleting routes,
	 * etc. So we skip calling the procedure when the interface's
	 * address(es) are initialized since the refcnt was already set when
	 * the structure was allocated. This happens when settting the primary
	 * or adding an IP alias address.
	 */
	if ((ia_xx = (struct in_ifaddr *)ifp->in_ifaddr)) {
		/*
		 * set the link-level route installation function
		 * if the driver has not specified one, use the default
		 */
		if (! ia_xx->ia_ifa.ifa_rtrequest) { /* not set yet */
			if (ifp->if_rtrequest) {
				ia_xx->ia_ifa.ifa_rtrequest = ifp->if_rtrequest;
			} else {
				ia_xx->ia_ifa.ifa_rtrequest = link_rtrequest;
			}
		}
	}
/*#endif IFA_LINK_RTREQUEST_ENABLED */

	if (alias) {
		if (scrub) {
			struct sockaddr *dst = (struct sockaddr*)sin;
			struct radix_node *rn;
#ifdef _HAVE_SA_LEN
			rn = rn_match((void*)sin,
				rt_tables[dst->sa_family]);
#else
			rn = rn_match((void*)sin,
				rt_tables[_FAKE_SA_FAMILY(dst)]);
#endif
			if (rn) {
				rtrequest(RTM_DELETE, dst, 
					((struct rtentry*)rn)->rt_gateway,
#ifdef _HAVE_SA_LEN
					(struct sockaddr *)rn->rn_mask,
#else
					(struct sockaddr_new *)rn->rn_mask,
#endif
					0, NULL);
			}
		}
	}

	if (!wasup && (ifp->if_flags & IFF_UP)) {
		ifp->if_lastchange = time;
	}
	if (scrub) {
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
		in_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	}
	if (IN_CLASSA(i))
		ia->ia_netmask = IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		ia->ia_netmask = IN_CLASSB_NET;
	else
		ia->ia_netmask = IN_CLASSC_NET;
	/*
	 * The subnet mask usually includes at least the standard network part,
	 * but may may be smaller in the supernetting case.
	 * If it is set, we believe it.
	 */
	if (ia->ia_subnetmask == 0) {
		ia->ia_subnetmask = ia->ia_netmask;
		ia->ia_sockmask.sin_addr.s_addr = htonl(ia->ia_subnetmask);
	} else
		ia->ia_netmask &= ia->ia_subnetmask;
	ia->ia_net = i & ia->ia_netmask;
	ia->ia_subnet = i & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);

	/*
	 * Add route for the network.
	 *
	 * Check for hostroute override case where we add a host route
	 * for the IP alias address for this interface.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;

	if (ifp->if_flags & IFF_BROADCAST) {

		ia->ia_broadaddr.sin_addr.s_addr =
			htonl(ia->ia_subnet | ~ia->ia_subnetmask);
		ia->ia_netbroadcast.s_addr =
			htonl(ia->ia_net | ~ ia->ia_netmask);

		if (alias) { /* for alias address create a host route */

			/* save original addr and dstaddr for later restore */
			save_dstaddr = ia->ia_ifa.ifa_dstaddr;

			save_addr = ia->ia_ifa.ifa_addr;
#ifdef _ALIAS_HOST_ROUTES
			/* now make dst address point to alias address */
			ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
			/*
			 * now make addr address point to primary address
			 * so that the host route's gateway address will
			 * be the primary address of this interface.
			 */
			ia->ia_ifa.ifa_addr =
		       ((struct in_ifaddr *)(ifp->in_ifaddr))->ia_ifa.ifa_addr;

			flags |= (RTF_HOST | RTF_HOSTALIAS | RTF_ANNOUNCE | RTF_STATIC);
#else
			/*
			 * Host routes via broadcast interfaces don't work
			 * right with the new ARP cloning mechanism.  For now,
			 * don't make them
			 */
			/*
			 * set the link-level route installation function
			 * if the driver has not specified one, use the default
			 * XXX this code could probably be removed and
			 * the above 'ia_xx' stuff done for all addresses and
			 * not just the primary.
			 */
			if (!ia->ia_ifa.ifa_rtrequest) { /* not set yet */
				if (ifp->if_rtrequest) {
					ia->ia_ifa.ifa_rtrequest = ifp->if_rtrequest;
				}
			}
#endif
			restore_addr = 1;
			broadcast++;
		}
	} else if (ifp->if_flags & IFF_LOOPBACK) {

		if (alias) {
			/* save original addr and dstaddr for later restore */
			save_dstaddr = ia->ia_ifa.ifa_dstaddr;
			save_addr = ia->ia_ifa.ifa_addr;

			/* now make dst address point to alias address */
			ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;

			/*
			 * Make address record's primary address point
			 * to the primary address of the loopback interface
			 */
			ia->ia_ifa.ifa_addr =
		       ((struct in_ifaddr *)(ifp->in_ifaddr))->ia_ifa.ifa_addr;

			restore_addr = 1;
			flags |= (RTF_HOST | RTF_HOSTALIAS);
		} else {
			/*
			 * Point destination address to local address storage
			 */
			ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
			flags |= RTF_HOST;
		}
	} else {
		if (ifp->if_flags & IFF_POINTOPOINT) {
			if (ia->ia_dstaddr.sin_family != AF_INET)
				return (0);
			flags |= RTF_HOST;
		}
	}

	if ((error = rtinit(&(ia->ia_ifa), (int)RTM_ADD, flags)) == 0) {
		ia->ia_flags |= IFA_ROUTE;
	}
	if (alias && restore_addr) {
#ifdef _ALIAS_HOST_ROUTES
		int need_net_route = 0;
		if (broadcast && (((struct sockaddr_in *)(ia->ia_ifa.ifa_addr))->sin_addr.s_addr &
		    ia->ia_subnetmask) !=
		    (((struct in_ifaddr *)(ifp->in_ifaddr))->ia_subnet)) {
			/*
			 * set up to add a cloning route, if the alias is
			 * not on the same subnet as the primry
			 */
			need_net_route = 1;
		}
#endif
		/*
		 * restore original dstaddr and addr addresses
		 */
		ia->ia_ifa.ifa_dstaddr = save_dstaddr;
		ia->ia_ifa.ifa_addr = save_addr;
#ifdef _ALIAS_HOST_ROUTES
		if (broadcast && need_net_route) {
			/*
			 * set the link-level route installation function
			 * if the driver has not specified one, use the default
			 * XXX this code could probably be removed and
			 * the above 'ia_xx' stuff done for all addresses and
			 * not just the primary.
			 */
			if (!ia->ia_ifa.ifa_rtrequest) { /* not set yet */
				if (ifp->if_rtrequest) {
					ia->ia_ifa.ifa_rtrequest = ifp->if_rtrequest;
				}
			}
			/*
			 * add cloning network route to restore 
			 * (accidental) functionality of previous releases
			 */
			(void) rtinit(&(ia->ia_ifa), (int)RTM_ADD, oflags);
		}
#endif
	}
	/*
	 * If the interface supports multicast, join the "all hosts"
	 * multicast group on that interface.
	 */
	if (ifp->if_flags & IFF_MULTICAST) {
		struct in_addr addr;

		addr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);
		in_addmulti(addr, ifp);
	}
	return (0);
}

/*
 * This is a hash enumeration match procedure which is used to get the
 * 'nth' IP alias address associated with the interface specified by 'ifp'.
 * The starting index of the enumeration is 'start' and can be used to skip
 * the primary address (index zero) or start at the first alias (index 1).
 *
 * int
 * in_getalias_enummatch(struct hashbucket *h, caddr_t start,
 *	caddr_t nth, struct ifnet *arg2)
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 * This function is used by ipv4 and ipv6.
 */
/* ARGSUSED */
int
in_getalias_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
#ifdef INET6
	struct ifaddr *ifa = (struct ifaddr *)h;
	struct ifnet *ifp = (struct ifnet *)arg2;

	return (((ifa->ifa_ifp == ifp) && (ifa->ifa_addrflags & IADDR_ALIAS))
		? 1 : 0);
#else
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct ifnet *ifp = (struct ifnet *)arg2;

	return (((ia->ia_ifp == ifp) && (ia->ia_addrflags & IADDR_ALIAS))
		? 1 : 0);
#endif
}

/*
 * This is a hash enumeration match procedure which compares the network
 * number desired with the subnet number in the hash bucket entry,
 * to see if we have a match.
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
static int
in_iaonnetof_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia;
	__uint32_t net;
	int match = 0;

	ia = (struct in_ifaddr *)h;
	net = (__psint_t)key;

	if (ia->ia_subnet == net)
		match = 1;
	return match;
}

/*
 * Return address info for specified internet network.
 */
struct in_ifaddr *
in_iaonnetof(__uint32_t net)
{
	return ((struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
			in_iaonnetof_enummatch, HTF_INET,
			(caddr_t)(__psint_t)net, (caddr_t)0, (caddr_t)0));
}

/*
 * The hash lookup match function to compare the broadcast IP address
 * in the 'struct in_addr' (aka key) to the broadcast address field
 * in the broadcast hash table entry supplied. We also check that this
 * entry corresponds to the network interface designated by ifp (aka arg1).
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the lookup call. Here that is HTF_BROADCAST hash
 * bucket entries.
 *
 * in_bcast_broadcast_match(struct in_ifaddr *ia, struct in_addr *key,
 *	struct ifnet *arg1, caddr_t arg2)
 */
/* ARGSUSED */
int
in_bcast_broadcast_match(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_bcast *in_bcastp;
	struct in_addr *addr;
	struct ifnet *ifp;
	int match;

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

	in_bcastp = (struct in_bcast *)h;
	addr = (struct in_addr *)key;
	ifp = (struct ifnet *)arg1;

	if ((in_bcastp->ifp == ifp) &&
	    (in_bcastp->ia_addr.sin_family == AF_INET) &&
	    (in_bcastp->ia_dstaddr.sin_addr.s_addr == addr->s_addr)) {
		/*
		 * match on the interface AND the full broadcast address
		 */
		match = 1;
	} else {
		match = 0;
	}
	return match;
}

/*
 * The hash lookup match function to compare the netbroadcast IP address
 * in the 'struct in_addr' (aka key) to the netbroadcast address field
 * in the broadcast hash table entry supplied. We also check that this
 * entry corresponds to the network interface designated by ifp (aka arg1).
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the lookup call. Here that is HTF_BROADCAST hash
 * bucket entries.
 *
 * in_bcast_net_match(struct in_ifaddr *ia, struct in_addr *key,
 *	struct ifnet *arg1, caddr_t arg2)
 */
/* ARGSUSED */
int
in_bcast_net_match(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_bcast *in_bcastp;
	struct in_addr *addr;
	struct ifnet *ifp;
	__uint32_t t;
	int match = 0;

	/*
	 * Check that supplied lookup key matches the one used on entry
	 * insertion, before we check the rest of the fields to be sure
	 * this is the correct one since there can be a full broadcast
	 * entry and a netbroadcast entry depending on the netmask.
	 */
	if (bcmp(h->key, key, sizeof(struct in_addr) != 0)) { /* no match */
		return 0;
	}

	in_bcastp = (struct in_bcast *)h;
	addr = (struct in_addr *)key;
	ifp = (struct ifnet *)arg1;
	t = ntohl(addr->s_addr);

	if ((in_bcastp->ifp == ifp) &&
	    (in_bcastp->ia_addr.sin_family == AF_INET) &&
	    ((in_bcastp->ia_netbroadcast.s_addr == addr->s_addr) ||
	     /* Check for old-style (host 0) broadcast */
	     (t == in_bcastp->ia_subnet) || (t == in_bcastp->ia_net))) {
		/*
		 * match on the interface AND the address family AND
		 * (either the net broadcast address or the subnet numbers)
		 */
		match = 1;
	}
	return match;
}

/*
 * The hash enumerate match function to compare the address supplied in
 * 'in_addr' (aka key) to the address'es for the supplied network interface
 * in 'ifp'.
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 *
 * in_broadcast_enummatch(struct in_ifaddr *ia, struct in_addr *key,
 *	struct ifnet *arg1, caddr_t arg2)
 */
/* ARGSUSED */
int
in_broadcast_enummatch(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct ifnet *ifp = (struct ifnet *)arg1;
	struct in_addr *addr;
	__uint32_t t;
	int match;

	if (ia->ia_ifp != ifp)
		return 0;

	if ((ia->ia_ifp->if_flags & IFF_BROADCAST) == 0) {
		return 0;
	}

	addr = (struct in_addr *)key;
	t = ntohl(addr->s_addr);
	match = 0;

	if (ia->ia_ifa.ifa_addr->sa_family == AF_INET &&
	 (addr->s_addr == SATOSIN(ia->ia_ifa.ifa_broadaddr)->sin_addr.s_addr ||
		     addr->s_addr == ia->ia_netbroadcast.s_addr ||
		     /*
		      * Check for old-style (host 0) broadcast.
		      */
		     t == ia->ia_subnet || t == ia->ia_net)) {
			    match = 1;
	}
	return match;
}

/*
 * The hash lookup match function to compare the broadcast IP address
 * in the 'struct in_addr' (aka key) to the any of the broadcast address
 * fields in the broadcast hash table entry supplied. We also check that this
 * entry corresponds to the network interface designated by ifp (aka arg1).
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the lookup call. Here that is HTF_BROADCAST hash
 * bucket entries. 'arg2' is ignored here.
 *
 * in_broadcast_match(struct in_ifaddr *ia, struct in_addr *key,
 *	struct ifnet *arg1, caddr_t arg2)
 */
/* ARGSUSED */
int
in_broadcast_match(struct hashbucket *h, caddr_t key,
	caddr_t arg1, caddr_t arg2)
{
	struct in_bcast *in_bcastp;
	struct in_addr *addr;
	struct ifnet *ifp;
	__uint32_t t;
	int match;

	if ((h->flags & HTF_BROADCAST) == 0) { /* not broadcast entry */
		return 0;
	}

	in_bcastp = (struct in_bcast *)h;
	addr = (struct in_addr *)key;
	ifp = (struct ifnet *)arg1;

	if (in_bcastp->ifp != ifp) { /* not right interface */
		return 0;
	}

	if ((in_bcastp->ifp->if_flags & IFF_BROADCAST) == 0) {
		/* no bcast support on this network interface */
		return 0;
	}
	      
	t = ntohl(addr->s_addr);
	if ((in_bcastp->ia_addr.sin_family == AF_INET) &&

	     ((in_bcastp->ia_dstaddr.sin_addr.s_addr == addr->s_addr) ||
	      (in_bcastp->ia_netbroadcast.s_addr == addr->s_addr) ||
	       /* Check for old-style (host 0) broadcast */
	      (t == in_bcastp->ia_subnet) || (t == in_bcastp->ia_net))) {
		/*
		 * match on the address family AND
		 * (either full broadcast address OR net broadcast address) OR
		 *  either the subnet OR net numbers)
		 */
		 match = 1;
	} else {
		 match = 0;
	}
	return match;
}

/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(struct in_addr in, struct ifnet *ifp)
{
	struct in_ifaddr *ia;
	struct in_bcast *in_bcastp;

	if (in.s_addr == INADDR_BROADCAST || in.s_addr == INADDR_ANY) {
		return 1;
	}

	/*
	 * Check if address matches broadcast address for the
	 * primary address supplied for the interface.
	 */
	if (ia = (struct in_ifaddr *)ifp->in_ifaddr) {
		if (ip_check_broadcast(ia, &in)) {
			return 1;
		}
	}
	/*
	 * First check if address matches broadcast address for the
	 * primary address for any of our network interfaces.
	 * This handles the case when we get the broadcast address
	 * for a particular network interface passed in with 'ifp'
	 * corresponding to the loopback interface.
	 */
	if (ip_accept_primary_broadcast(&in)) {
		return 1;
	}

	/* Check if this network interface supports broadcast */
	if ((ifp->if_flags & IFF_BROADCAST) == 0) { /* no cigar */
		return 0;
	}

	/*
	 * NOTE: The broadcast hash lookup will return either
	 * a HTF_BROADCAST entry or NULL.
	 */
	in_bcastp = (struct in_bcast *)hash_lookup(&hashinfo_inaddr_bcast,
			in_broadcast_match,
			(caddr_t)&in,
			(caddr_t)ifp,
			(caddr_t)0);
	return ((in_bcastp) ? 1 : 0);
}

/*
 * This procedure is used for the hash lookup match which compares the
 * multi-cast address in the entry with that supplied along with comparing
 * the given interface address pointers.
 * NOTE: The INADDR_ALLHOSTS_GROUP is added to the multicast address list
 * for each interface.
 *
 * in_multi_match(struct in_multi *inm, struct in_addr *key,
 *  struct ifnet *arg)
 */
/* ARGSUSED */
int
in_multi_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	struct in_multi *inm;
	struct in_addr *addr;
	struct ifnet *ifp;
	int match = 0;

	if (h->flags & HTF_MULTICAST) { /* multicast address node */

		inm = (struct in_multi *)h;
		addr = (struct in_addr *)key;
		ifp = (struct ifnet *)arg1;

		if ((inm->inm_ifp == ifp) &&
		    (inm->inm_addr.s_addr == addr->s_addr)) {
			 match = 1;
		}
	}
	return match;
}

/*
 * Add an address to the list of IP multicast addresses for a given interface.
 */
struct in_multi *
in_addmulti(struct in_addr addr, struct ifnet *ifp)
{
	register struct in_multi *inm;
	struct sockaddr_in sin;

	ASSERT(IFNET_ISLOCKED(ifp));

	inm = (struct in_multi *)hash_lookup(&hashinfo_inaddr,
		in_multi_match, (caddr_t)&addr, (caddr_t)ifp, (caddr_t)0);
	
	if (inm) { /* found so simply increment refcount */

		(void)atomicAddUint(&(inm->inm_refcount), 1);
		return inm;
	}

	/*
	 * Ask the network driver to update its multicast reception
	 * filter appropriately for the new address.
	 */
	if (ifp->if_ioctl == NULL) {
		return NULL;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	sin.sin_addr = addr;
	if ((*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&sin) !=  0) {
		return NULL;
	}
	/*
	 * New address; get a buffer for a new multicast record
	 * and link it into the interface's multicast list.
	 */
	inm = (struct in_multi *)mcb_get(M_WAIT,
			sizeof(struct in_multi), MT_IFADDR);

	inm->inm_refcount = 1;
	inm->hashbucket.flags = HTF_MULTICAST;
	inm->inm_addr = addr;
	inm->inm_ifp = ifp;

	/*
	 * Perform insertion under the ifnet lock to serialize adds/deletes.
	 */
	(void)hash_insert(&hashinfo_inaddr, &(inm->hashbucket),
			  (caddr_t)(&addr));
	IFNET_UNLOCK(ifp);

	/*
	 * Let IGMP know that we have joined a new IP multicast group.
	 * We must do this with the ifnet lock unlocked, because we
	 * grab the lock again to send the IGMP HOST MEMBERSHIP packet.
	 * Also do hash insert without interface lock to avoid deadlock.
	 */

	igmp_joingroup(inm);

	IFNET_LOCK(ifp);

	return inm;
}

/*
 * Delete a multicast address record.
 */
void
in_delmulti(struct in_multi *inm)
{
	struct sockaddr_in sin;
	struct ifnet *ifp = inm->inm_ifp;
	/*REFERENCED*/
	NETSPL_DECL(s)

	IFNET_LOCK(ifp);

	if (atomicAddUint(&inm->inm_refcount, -1) == 0) {
		/*
		 * Notify network driver to update its
		 * multicast reception filter.
		 */
		sin.sin_family = AF_INET;
		sin.sin_port = 0;
		sin.sin_addr = inm->inm_addr;
		(*inm->inm_ifp->if_ioctl)(inm->inm_ifp, SIOCDELMULTI,
			(caddr_t)&sin);

		/*
		 * Remove entry while holding ifnet lock so that nobody else
		 * can find us.   Removing from the IP hash table does NOT
		 * delete storage; that will be done below after the call to
		 * igmp_leavegroup().
		 */
		hash_remove(&hashinfo_inaddr, (struct hashbucket *)inm);

		IFNET_UNLOCK(ifp);
		/*
		 * No remaining claims to this IP group; let IGMP know that
		 * we are leaving the multicast group. We must do this with
		 * the lock unlocked, because we grab the lock again for
		 * the sending of the IGMP LEAVE packet.
		 */
		igmp_leavegroup(inm);

		mcb_free(inm, sizeof(struct in_multi), MT_IFADDR);
	} else {
		IFNET_UNLOCK(ifp);
	}
	return;
}

/*
 * find the first interface with IFF_MULTICAST
 */
struct ifnet *
ip_find_first_multiif(struct in_addr dst)
{
    struct ifnet *ifp;

    if ( !IN_MULTICAST(dst.s_addr) )
        return 0;

    for (ifp = ifnet; ifp; ifp=ifp->if_next)
        if (ifp->if_flags & IFF_MULTICAST)
            return ifp;
    return 0;
}

